#!/usr/bin/env bash
# screenshot.sh - capture the running game for visual verification, with NO input injection.
#
# Methods (all verified on this Pop!_OS/COSMIC/Wayland/NVIDIA box, 2026-10-08):
#   gamescope : 'gamescopectl screenshot <file>' over gamescope's private Wayland socket
#               (gamescope 3.14.24 ConCommand "screenshot"); exact game frame, game resolution
#   x11       : X11 GetImage of the window on Xwayland via python3-xlib + PIL (Wine windows,
#               or the nested gamescope window itself); works for GL/Vulkan content
#   desktop   : cosmic-screenshot --interactive=false (whole desktop, all outputs, logical px)
# NOT usable here: grim 1.4.0 ("compositor doesn't support wlr-screencopy-unstable-v1").
# For the GAME prefer gamescope (auto does): on 2026-10-08 the CET/OpenGL game rendered a full
# 1920x1080 frame while its X11 window on gamescope's :2 reported 640x480, so an x11 capture of
# :2 came back 640x480 and squeezed; gamescopectl returned the real 1920x1080 frame.
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
LOG_DIR="$PROJECT_ROOT/logs"
SHOT_DIR="$LOG_DIR/screenshots"
STATE="$LOG_DIR/run_game.state"
LOG="$LOG_DIR/screenshot-$(date +%Y%m%d).log"

MODE=auto
# Proton's game windows have WM_CLASS steam_app_1766413869; the game's title is exactly 'Emergence'
# (matched string = '<title> <class>'). No bare 'homeworld'/'cataclysm': those also match editor or
# terminal windows titled '...Homeworld-Infection...' on :1 and would capture the user's desktop.
MATCH='steam_app_1766413869|^emergence( |$)'
WID=""
OUT=""
LIST=0
XDISPLAY="${DISPLAY:-:1}"
GS_SOCK=""
WAIT_SECS=10

usage() {
  cat <<USAGE
Usage: tools/screenshot.sh [mode] [options]

Modes (default --auto):
  --auto        gamescope if tools/run_game.sh is running one, else an X11 window matching
                --match, else the gamescope window on the desktop, else the full desktop
  --gamescope   gamescopectl screenshot (needs a live gamescope; socket from $STATE
                or the newest \$XDG_RUNTIME_DIR/gamescope-N)
  --x11         X11 GetImage of one window on \$DISPLAY (default $XDISPLAY)
  --desktop     cosmic-screenshot of the whole desktop (non-interactive)

Options:
  --out FILE        output PNG (default $SHOT_DIR/<mode>-<timestamp>.png)
  --match REGEX     x11: case-insensitive regex on WM_NAME + WM_CLASS
                    (default '$MATCH'; Proton names game windows steam_app_<SteamAppId>)
  --window-id ID    x11: capture this window id (hex 0x... or decimal)
  --display :N      x11: X display (default $XDISPLAY; gamescope's inner one is in the state file)
  --socket NAME     gamescope: Wayland socket name, e.g. gamescope-0
  --list            list viewable named X11 windows (id, geometry, class, title) and exit
  -h, --help

Prints the PNG path as the last line; a near-black frame triggers a WARNING.
Log: $LOG_DIR/screenshot-<date>.log
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --auto) MODE=auto; shift ;;
    --gamescope) MODE=gamescope; shift ;;
    --x11) MODE=x11; shift ;;
    --desktop) MODE=desktop; shift ;;
    --out) OUT="${2:?}"; shift 2 ;;
    --match) MATCH="${2:?}"; shift 2 ;;
    --window-id) WID="${2:?}"; MODE=x11; shift 2 ;;
    --display) XDISPLAY="${2:?}"; shift 2 ;;
    --socket) GS_SOCK="${2:?}"; shift 2 ;;
    --list) LIST=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "screenshot.sh: unknown option '$1' (try --help)" >&2; exit 2 ;;
  esac
done

mkdir -p "$SHOT_DIR"
exec > >(tee -a "$LOG") 2>&1
log() { printf '%s [shot] %s\n' "$(date '+%F %T')" "$*" >&2; }
die() { log "ERROR: $*"; exit 1; }
state_get() { [[ -f "$STATE" ]] && sed -n "s/^$1=//p" "$STATE" | tail -n1 || true; }
stamp() { date +%Y%m%d-%H%M%S-%3N; }   # ms: two captures in one second must not collide

# ---------------------------------------------------------------- X11 helper (python3-xlib + PIL)
x11_tool() {  # x11_tool list|capture DISPLAY REGEX WID OUT
  python3 - "$@" <<'PY'
import re, sys
from Xlib import display as xdisplay, X, error as xerror
from PIL import Image
action, dpy_name, regex, wid, out = sys.argv[1:6]
d = xdisplay.Display(dpy_name)
root = d.screen().root
rg = root.get_geometry()
NET_WM_NAME = d.intern_atom('_NET_WM_NAME'); UTF8 = d.intern_atom('UTF8_STRING')
def title(w):
    try:
        p = w.get_full_property(NET_WM_NAME, UTF8)
        if p and p.value:
            v = p.value
            return v.decode('utf-8', 'replace') if isinstance(v, (bytes, bytearray)) else str(v)
    except xerror.XError:
        pass
    n = w.get_wm_name() or ''
    return n.decode('latin-1') if isinstance(n, (bytes, bytearray)) else str(n)
def walk(w):
    yield w
    try:
        kids = w.query_tree().children
    except xerror.XError:
        return
    for c in kids:
        yield from walk(c)
def info(w):
    try:
        a = w.get_attributes()
        if a.map_state != X.IsViewable:
            return None
        g = w.get_geometry()
        name = title(w)
        cls = w.get_wm_class() or ()
        t = w.translate_coords(root, 0, 0)
        return dict(w=w, id=w.id, name=str(name), cls=' '.join(cls), x=-t.x, y=-t.y, wd=g.width, ht=g.height)
    except xerror.XError:
        return None
wins = [i for i in (info(w) for w in walk(root)) if i and (i['name'] or i['cls'])]
if action == 'list':
    print(f"display {dpy_name}: root {rg.width}x{rg.height}")
    for i in wins:
        print(f"0x{i['id']:08x} {i['wd']:>5}x{i['ht']:<5} +{i['x']}+{i['y']}  class={i['cls']!r}  title={i['name']!r}")
    sys.exit(0)
if wid:
    target = d.create_resource_object('window', int(wid, 0))
    i = info(target)
    if not i:
        sys.exit(f"window {wid} not viewable")
else:
    rx = re.compile(regex, re.I)
    cands = [i for i in wins if rx.search(i['name'] + ' ' + i['cls'])]
    if not cands:
        sys.exit(f"no viewable window matches /{regex}/ on {dpy_name} (try --list)")
    i = max(cands, key=lambda c: c['wd'] * c['ht'])
# X11 GetImage needs the rectangle on-screen: clip the window to the root
x0, y0 = max(0, -i['x']), max(0, -i['y'])
x1, y1 = min(i['wd'], rg.width - i['x']), min(i['ht'], rg.height - i['y'])
if x1 <= x0 or y1 <= y0:
    sys.exit("window is entirely off-screen")
raw = i['w'].get_image(x0, y0, x1 - x0, y1 - y0, X.ZPixmap, 0xffffffff)
im = Image.frombytes('RGB', (x1 - x0, y1 - y0), raw.data, 'raw', 'BGRX')
im.save(out)
print(f"captured 0x{i['id']:x} class={i['cls']!r} title={i['name']!r} {x1-x0}x{y1-y0}"
      + ("" if (x0, y0, x1, y1) == (0, 0, i['wd'], i['ht']) else f" (clipped from {i['wd']}x{i['ht']})"))
PY
}

if [[ $LIST -eq 1 ]]; then
  x11_tool list "$XDISPLAY" '' '' ''
  gd="$(state_get GAMESCOPE_DISPLAY)"
  if [[ -n "$gd" && "$gd" != "$XDISPLAY" ]]; then x11_tool list "$gd" '' '' '' || true; fi
  exit 0
fi

# ---------------------------------------------------------------- methods
shot_gamescope() {  # $1 = out
  command -v gamescopectl >/dev/null 2>&1 || { log "gamescopectl not installed"; return 1; }
  local sock="$GS_SOCK" rt="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
  [[ -n "$sock" ]] || sock="$(state_get GAMESCOPE_WAYLAND_DISPLAY)"
  if [[ -z "$sock" ]]; then
    sock="$(ls -1t "$rt"/gamescope-* 2>/dev/null | grep -v '\.lock$' | head -n1 || true)"
    sock="${sock##*/}"
  fi
  [[ -n "$sock" && -S "$rt/$sock" ]] || { log "no live gamescope socket${sock:+ ($sock)}"; return 1; }
  rm -f -- "$1"
  log "gamescopectl screenshot via $sock"
  GAMESCOPE_WAYLAND_DISPLAY="$sock" timeout 10 gamescopectl screenshot "$1" >&2 || { log "gamescopectl failed"; return 1; }
  # gamescope encodes the PNG on a worker thread after the next composite
  local i last=-1 sz
  for ((i = 0; i < WAIT_SECS * 10; i++)); do
    if [[ -s "$1" ]]; then
      sz="$(stat -c %s -- "$1")"
      [[ "$sz" == "$last" ]] && return 0
      last="$sz"
    fi
    sleep 0.1
  done
  log "gamescope did not write $1 within ${WAIT_SECS}s (it only captures when it composites a frame)"
  return 1
}

shot_x11() {  # $1 = out, $2 = regex
  local res
  if res="$(x11_tool capture "$XDISPLAY" "$2" "$WID" "$1" 2>&1)"; then log "$res"; return 0; fi
  log "x11: $res"; return 1
}

shot_desktop() {  # $1 = out
  command -v cosmic-screenshot >/dev/null 2>&1 || { log "cosmic-screenshot not installed"; return 1; }
  local tmp f
  tmp="$(mktemp -d)"
  timeout 30 cosmic-screenshot --interactive=false --notify=false --save-dir "$tmp" >/dev/null 2>&1 || true
  f="$(ls -1 "$tmp"/*.png 2>/dev/null | head -n1 || true)"
  [[ -n "$f" ]] || { rmdir "$tmp" 2>/dev/null || true; log "cosmic-screenshot produced nothing"; return 1; }
  mv -f -- "$f" "$1"; rmdir "$tmp" 2>/dev/null || true
  log "desktop capture (all outputs, logical pixels)"
}

report() {  # $1 = png
  python3 - "$1" <<'PY' >&2
import sys
from PIL import Image, ImageStat
im = Image.open(sys.argv[1]).convert('RGB')
st = ImageStat.Stat(im)
mean = sum(st.mean) / 3; sd = sum(st.stddev) / 3
print(f"{sys.argv[1]}: {im.width}x{im.height} mean={mean:.1f} stddev={sd:.1f}")
if mean < 3 and sd < 3:
    print("WARNING: frame is (nearly) black - game not drawing yet, minimized, or wrong window")
PY
}

[[ -n "$OUT" ]] || OUT="$SHOT_DIR/$MODE-$(stamp).png"
mkdir -p "$(dirname -- "$OUT")"
ok=0
case "$MODE" in
  gamescope) shot_gamescope "$OUT" && ok=1 ;;
  x11)       shot_x11 "$OUT" "$MATCH" && ok=1 ;;
  desktop)   shot_desktop "$OUT" && ok=1 ;;
  auto)
    if [[ "$(state_get USE_GAMESCOPE)" == 1 ]] && shot_gamescope "$OUT"; then ok=1
    elif shot_x11 "$OUT" "$MATCH"; then ok=1
    elif [[ -z "$WID" ]] && shot_x11 "$OUT" ' gamescope gamescope$'; then ok=1   # host window: WM_CLASS gamescope, title = game's
    elif shot_desktop "$OUT"; then ok=1
    fi ;;
esac
[[ $ok -eq 1 ]] || die "no capture method worked (mode $MODE)"
report "$OUT"
echo "$OUT"
