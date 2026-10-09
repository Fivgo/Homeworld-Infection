#!/usr/bin/env bash
# run_game.sh - launch Homeworld: Emergence (Cataclysm) from original/game/ in the project's
# Wine prefix (GE-Proton10-34 via umu-run), by default through CET's cataclysm_loader.exe,
# inside a WINDOWED nested gamescope session (never fullscreen).
#
# gamescope on this machine needs SDL_VIDEODRIVER=x11 (COSMIC 150% fractional-scale viewport
# crash on the Wayland path) and --prefer-vk-device 10de:2b85 (else it can grab the AMD iGPU).
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
LOG_DIR="$PROJECT_ROOT/logs"
TS="$(date +%Y%m%d-%H%M%S)"
LOG="$LOG_DIR/run_game-$TS.log"
STATE="$LOG_DIR/run_game.state"

PROTON="${HWC_PROTON:-GE-Proton10-34}"
PREFIX="${HWC_PREFIX:-$PROJECT_ROOT/prefix}"
GAMEID="${HWC_GAMEID:-umu-1766413869}"      # -> SteamAppId 1766413869 -> X11 WM_CLASS steam_app_1766413869
STEAM_ROOT="${HWC_STEAM_ROOT:-$HOME/.local/share/Steam}"
GAME_ROOT="$PROJECT_ROOT/original/game"
GAME_DIR="${HWC_GAME_DIR:-}"
SIZE="${HWC_SIZE:-1920x1080}"
WINDOW=""
VK_DEVICE="${HWC_VK_DEVICE:-10de:2b85}"     # RTX 5090 Vulkan vendor:device
MODE=cet            # cet | original | exe
EXE=""
USE_GAMESCOPE=1
VDESKTOP=""
NO_DXVK=0
WINE_LOG=0
DIRECT=0
SCALER=""; FILTER=""; FPS=""; GRAB=0
USE_DEFAULT_ARGS=1
DEFAULT_ARGS=(/noswddraw)   # GOG's playTask (goggame-1766413869.info) launches "cataclysm.exe /noswddraw"
DRY_RUN=0
KILL=0
SYNC_RES=0          # 1 = store --size as the game's video mode before launch, 2 = only that
GAME_ARGS=()
PREFIX_CLI=""       # set when --prefix is given (else --kill uses the prefix recorded by the running launch)
# The GOG game (Relic Win32 code; HomeworldSDL moved this into its .cfg) keeps its video mode
# in HKLM\SOFTWARE\Sierra On-Line\Cataclysm, i.e. this section of prefix/system.reg (32-bit view):
CAT_KEY_TXT='[Software\\Wow6432Node\\Sierra On-Line\\Cataclysm]'

usage() {
  cat <<USAGE
Usage: tools/run_game.sh [options] [-- extra game args]

Default: cd original/game && gamescope (window ${SIZE}) -> umu-run cataclysm_loader.exe /noswddraw
The loader starts cataclysm.exe suspended, injects CataclysmEngineTweaks.dll and resumes it
(reference/CataclysmEngineTweaks/loader/Main.cpp).

What to launch:
  --cet               cataclysm_loader.exe (default; needs tools/install_cet.sh)
  --original          cataclysm.exe directly (no CET hooks)
  --exe PATH          any program (relative to the game dir, absolute, or a Wine builtin
                      such as 'notepad' - used to test the plumbing without the game)
  --no-default-args   do not pass GOG's default '/noswddraw' (and '/window' with --no-gamescope)
  -- ARGS...          extra game arguments, e.g. -- /safeGL  (HW1-era switches: /window
                      /noBorder /640../1600 /safeGL /sw /noSound /disableAVI /heap N)

Window / display:
  --size WxH          game resolution = gamescope nested size (default $SIZE)
  --window WxH        gamescope window size if different (e.g. --size 1280x960 --window 2560x1920)
  --scaler S          gamescope -S auto|integer|fit|fill|stretch
  --filter F          gamescope -F linear|nearest|fsr|nis|pixel
  --fps N             gamescope -r N (nested refresh)
  --grab-cursor       gamescope --force-grab-cursor (keeps the RTS cursor inside the window)
  --sync-res          before launching, store --size as the game's saved video mode
                      (registry HKLM\Software\Sierra On-Line\Cataclysm screenWidth/screenHeight,
                      screenDepth=32, via 'reg import', ~3 s) so the game renders 1:1 in the
                      gamescope window. Without it a mismatch only triggers a WARNING.
  --sync-res-only     do just that and exit (nothing is launched)
  --no-gamescope      run directly on the desktop via Xwayland (CET may pick a huge
                      multi-monitor desktop size - prefer gamescope or --virtual-desktop)
  --virtual-desktop WxH  (with --no-gamescope) wrap in a Wine virtual desktop (explorer /desktop)

Wine / Proton:
  --no-dxvk           PROTON_USE_WINED3D=1: d3d9 via wined3d instead of DXVK (only matters for
                      CET's D3D9 rgld3d.dll; stock D3D7 path and OpenGL never use DXVK)
  --wine-log          PROTON_LOG=1 -> $LOG_DIR/steam-1766413869.log (+DXVK logs)
  --proton NAME|DIR   default $PROTON
  --prefix DIR        default $PREFIX
  --direct-proton     use '<proton>/proton waitforexitandrun' instead of umu-run
  --game-dir DIR      default: directory of cataclysm.exe under original/game/

Other:
  --dry-run           print environment + command, launch nothing
  --kill              stop everything running in the prefix (wineserver -k) and the
                      gamescope recorded in $STATE (prefix + Proton are taken
                      from that state file too unless --prefix is given)
  -h, --help

Log: $LOG_DIR/run_game-<timestamp>.log (also logs/run_game-latest.log)
State for tools/screenshot.sh: $STATE
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --cet) MODE=cet; shift ;;
    --original) MODE=original; shift ;;
    --exe) MODE=exe; EXE="${2:?--exe needs a path}"; shift 2 ;;
    --no-default-args) USE_DEFAULT_ARGS=0; shift ;;
    --size) SIZE="${2:?}"; shift 2 ;;
    --window) WINDOW="${2:?}"; shift 2 ;;
    --scaler) SCALER="${2:?}"; shift 2 ;;
    --filter) FILTER="${2:?}"; shift 2 ;;
    --fps) FPS="${2:?}"; shift 2 ;;
    --grab-cursor) GRAB=1; shift ;;
    --no-gamescope) USE_GAMESCOPE=0; shift ;;
    --virtual-desktop) VDESKTOP="${2:?}"; shift 2 ;;
    --no-dxvk|--wined3d) NO_DXVK=1; shift ;;
    --wine-log) WINE_LOG=1; shift ;;
    --proton) PROTON="${2:?}"; shift 2 ;;
    --prefix) PREFIX="${2:?}"; PREFIX_CLI="$PREFIX"; shift 2 ;;
    --direct-proton) DIRECT=1; shift ;;
    --game-dir) GAME_DIR="${2:?}"; shift 2 ;;
    --dry-run) DRY_RUN=1; shift ;;
    --kill) KILL=1; shift ;;
    --sync-res) SYNC_RES=1; shift ;;
    --sync-res-only) SYNC_RES=2; shift ;;
    -h|--help) usage; exit 0 ;;
    --) shift; GAME_ARGS=("$@"); break ;;
    *) echo "run_game.sh: unknown option '$1' (try --help)" >&2; exit 2 ;;
  esac
done

re='^[0-9]+x[0-9]+$'
[[ "$SIZE" =~ $re ]] || { echo "--size must be WxH" >&2; exit 2; }
[[ -z "$WINDOW" || "$WINDOW" =~ $re ]] || { echo "--window must be WxH" >&2; exit 2; }
[[ -z "$VDESKTOP" || "$VDESKTOP" =~ $re ]] || { echo "--virtual-desktop must be WxH" >&2; exit 2; }
W="${SIZE%x*}"; H="${SIZE#*x}"
WIN="${WINDOW:-$SIZE}"; WW="${WIN%x*}"; WH="${WIN#*x}"

mkdir -p "$LOG_DIR"
exec > >(tee -a "$LOG") 2>&1
log() { printf '%s [run_game] %s\n' "$(date '+%F %T')" "$*"; }
die() { printf '%s [run_game] ERROR: %s\n' "$(date '+%F %T')" "$*"; exit 1; }
log "log file: $LOG"

# --- Proton ----------------------------------------------------------------------
PROTON_DIR=""
for d in "$PROTON" "$STEAM_ROOT/compatibilitytools.d/$PROTON" "$STEAM_ROOT/steamapps/common/$PROTON"; do
  if [[ -f "$d/proton" ]]; then PROTON_DIR="$(cd -- "$d" && pwd)"; break; fi
done
[[ -n "$PROTON_DIR" ]] || die "Proton '$PROTON' not found"

state_get() { [[ -f "$STATE" ]] && sed -n "s/^$1=//p" "$STATE" | tail -n1 || true; }

# --- --kill --------------------------------------------------------------------------
if [[ $KILL -eq 1 ]]; then
  gspid="$(state_get GAMESCOPE_PID)"
  # a launch with --prefix X records PREFIX=X (and its Proton) in the state file; use them unless
  # --prefix is given again, so a bare --kill reaches the prefix that is actually running
  kprefix="${PREFIX_CLI:-$(state_get PREFIX)}"; kprefix="${kprefix:-$PREFIX}"
  kproton="$(state_get PROTON_DIR)"; [[ -n "$kproton" && -x "$kproton/files/bin/wineserver" ]] || kproton="$PROTON_DIR"
  log "wineserver -k in $kprefix"
  WINEPREFIX="$kprefix" "$kproton/files/bin/wineserver" -k 2>/dev/null || log "(no wineserver running)"
  for _ in 1 2 3 4 5 6 7 8 9 10; do   # gamescope exits by itself once its child is gone
    [[ -n "$gspid" ]] && kill -0 "$gspid" 2>/dev/null || break
    sleep 0.5
  done
  if [[ -n "$gspid" ]] && kill -0 "$gspid" 2>/dev/null; then
    log "stopping gamescope pid $gspid"; kill -TERM "$gspid" 2>/dev/null || true
  fi
  runpid="$(state_get RUN_PID)"
  # the running run_game.sh removes its own state file after reading the exit code
  if [[ -z "$runpid" ]] || ! kill -0 "$runpid" 2>/dev/null; then rm -f -- "$STATE"; fi
  exit 0
fi

# --- game dir / exe ----------------------------------------------------------------------
if [[ -z "$GAME_DIR" ]]; then
  exe0="$(find -H "$GAME_ROOT" -maxdepth 4 -type f -iname 'cataclysm.exe' ! -path '*/.cet-backup/*' -print -quit 2>/dev/null || true)"
  [[ -n "$exe0" ]] && GAME_DIR="$(dirname -- "$exe0")"
fi
if [[ -z "$GAME_DIR" || ! -d "$GAME_DIR" ]]; then
  [[ $DRY_RUN -eq 1 ]] || die "no game directory (run tools/extract_installer.sh, or pass --game-dir)"
  log "dry run: no game directory yet, using placeholder"
  GAME_DIR="$GAME_ROOT"
fi
GAME_DIR="$(cd -- "$GAME_DIR" 2>/dev/null && pwd || echo "$GAME_DIR")"

need() {  # need FILE HINT
  [[ $SYNC_RES -eq 2 ]] && return 0   # --sync-res-only touches the registry, not the game files
  if [[ -z "$(find -H "$GAME_DIR" -maxdepth 1 -iname "$1" -print -quit 2>/dev/null)" ]]; then
    if [[ $DRY_RUN -eq 1 ]]; then log "dry run: WARNING missing $GAME_DIR/$1 ($2)"; else die "missing $GAME_DIR/$1 ($2)"; fi
  fi
}
case "$MODE" in
  cet)
    # loader/Main.cpp CheckCurrentDirectory(): both files must be in the working directory
    need cataclysm_loader.exe "run tools/install_cet.sh"
    need CataclysmEngineTweaks.dll "run tools/install_cet.sh"
    need cataclysm.exe "extract the game first"
    EXE_ARG="$GAME_DIR/cataclysm_loader.exe" ;;
  original)
    need cataclysm.exe "extract the game first"
    EXE_ARG="$GAME_DIR/cataclysm.exe" ;;
  exe)
    if [[ "$EXE" == /* ]]; then EXE_ARG="$EXE"
    elif [[ -e "$GAME_DIR/$EXE" ]]; then EXE_ARG="$GAME_DIR/$EXE"
    else EXE_ARG="$EXE"; log "'$EXE' is not a file here - assuming a program inside the prefix (e.g. a Wine builtin)"; fi ;;
esac
ARGS=()
if [[ $MODE != exe && $USE_DEFAULT_ARGS -eq 1 ]]; then
  ARGS+=("${DEFAULT_ARGS[@]}")
  # Stock HW1/HWC defaults to exclusive fullscreen (HomeworldSDL src/SDL/main.c:690-691
  # /fullscreen default, /window). Inside gamescope that fullscreen is confined to the nested
  # window; directly on the desktop it is not, so force a window there.
  if [[ $USE_GAMESCOPE -eq 0 && -z "$VDESKTOP" ]]; then ARGS+=(/window); fi
fi
ARGS+=("${GAME_ARGS[@]}")

# GOG's installer copies __support/app/Cataclysm.cfg ("language=0") into the game root;
# innoextract does not. Seed it once if missing (game runs only).
if [[ $MODE != exe && $DRY_RUN -eq 0 && $SYNC_RES -ne 2 && ! -e "$GAME_DIR/Cataclysm.cfg" && -f "$GAME_DIR/__support/app/Cataclysm.cfg" ]]; then
  cp -p -- "$GAME_DIR/__support/app/Cataclysm.cfg" "$GAME_DIR/Cataclysm.cfg"
  log "seeded $GAME_DIR/Cataclysm.cfg from __support/app/ (GOG post-install file)"
fi

# --- pre-flight ------------------------------------------------------------------------------
if [[ $DRY_RUN -eq 0 ]]; then
  [[ -f "$PREFIX/system.reg" ]] || die "no Wine prefix at $PREFIX - run tools/setup_prefix.sh"
  [[ $DIRECT -eq 1 ]] || command -v umu-run >/dev/null 2>&1 || die "umu-run not found - run tools/setup_prefix.sh"
  [[ $USE_GAMESCOPE -eq 0 ]] || command -v gamescope >/dev/null 2>&1 || die "gamescope not found (or use --no-gamescope)"
  oldpid="$(state_get RUN_PID)"
  if [[ -n "$oldpid" ]] && kill -0 "$oldpid" 2>/dev/null; then
    die "another run_game.sh (pid $oldpid) is still running - stop it first (tools/run_game.sh --kill)"
  fi
fi

# --- environment ------------------------------------------------------------------------------
ENVV=(WINEPREFIX="$PREFIX" GAMEID="$GAMEID" STORE=gog PROTONPATH="$PROTON_DIR" PROTON_VERB=waitforexitandrun)
if [[ $DIRECT -eq 1 ]]; then
  ENVV=(WINEPREFIX="$PREFIX" STEAM_COMPAT_CLIENT_INSTALL_PATH="$STEAM_ROOT" STEAM_COMPAT_DATA_PATH="$PREFIX"
        SteamAppId=1766413869 SteamGameId=1766413869 STORE=gog)
fi
[[ $NO_DXVK -eq 1 ]] && ENVV+=(PROTON_USE_WINED3D=1)
[[ $WINE_LOG -eq 1 ]] && ENVV+=(PROTON_LOG=1 PROTON_LOG_DIR="$LOG_DIR")

if [[ $DIRECT -eq 1 ]]; then LAUNCH=("$PROTON_DIR/proton" waitforexitandrun); else LAUNCH=(umu-run); fi

# --- the game's saved video mode (prefix registry) --------------------------------------------
saved_video() {  # sets SAVED_W SAVED_H SAVED_GL from $PREFIX/system.reg (plain text read, no Wine)
  SAVED_W=""; SAVED_H=""; SAVED_GL=""
  [[ -f "$PREFIX/system.reg" ]] || return 0
  local sec hw hh
  # ENVIRON, not awk -v: -v would turn the file's literal '\\' into '\'
  sec="$(K="$CAT_KEY_TXT" awk '/^\[/ { s = (index($0, ENVIRON["K"]) == 1); next } s && NF { print }' "$PREFIX/system.reg" 2>/dev/null || true)"
  hw="$(sed -n 's/^"screenWidth"=dword:\([0-9a-fA-F]\{1,8\}\).*/\1/p' <<<"$sec" | tail -n1)"
  hh="$(sed -n 's/^"screenHeight"=dword:\([0-9a-fA-F]\{1,8\}\).*/\1/p' <<<"$sec" | tail -n1)"
  if [[ -n "$hw" ]]; then SAVED_W=$((16#$hw)); fi
  if [[ -n "$hh" ]]; then SAVED_H=$((16#$hh)); fi
  SAVED_GL="$(sed -n 's/^"glToSelect"="\(.*\)"$/\1/p' <<<"$sec" | tail -n1)"
}
report_video() {
  saved_video
  if [[ -z "$SAVED_W" || -z "$SAVED_H" ]]; then
    log "saved mode: none in the prefix registry yet (first CET start: NewRendererSelection picks OpenGL at the desktop size$([[ $USE_GAMESCOPE -eq 1 ]] && echo " = ${W}x${H} inside gamescope"))"
    return 0
  fi
  log "saved mode: ${SAVED_W}x${SAVED_H}${SAVED_GL:+, glToSelect=$SAVED_GL} (HKLM\\Software\\Sierra On-Line\\Cataclysm in $PREFIX/system.reg)"
  if [[ $USE_GAMESCOPE -eq 1 && "${SAVED_W}x${SAVED_H}" != "$SIZE" && $SYNC_RES -eq 0 ]]; then
    log "WARNING: saved mode ${SAVED_W}x${SAVED_H} != --size $SIZE: gamescope will scale/letterbox. Add --sync-res for 1:1."
  fi
}
write_video_mode() {  # store ${W}x${H}x32 in the game's key (reg import merges; other values stay)
  local regf="$PREFIX/drive_c/hwc-video-mode.reg" out rc i
  log "+ reg import C:\\hwc-video-mode.reg  (screenWidth=$W screenHeight=$H screenDepth=32)"
  [[ $DRY_RUN -eq 0 ]] || return 0
  printf 'Windows Registry Editor Version 5.00\r\n\r\n[HKEY_LOCAL_MACHINE\\Software\\Wow6432Node\\Sierra On-Line\\Cataclysm]\r\n"screenWidth"=dword:%08x\r\n"screenHeight"=dword:%08x\r\n"screenDepth"=dword:00000020\r\n' "$W" "$H" > "$regf"
  set +e
  out="$(env "${ENVV[@]}" "${LAUNCH[@]}" reg import 'C:\hwc-video-mode.reg' 2>&1)"; rc=$?
  set -e
  rm -f -- "$regf"
  for ((i = 0; i < 20; i++)); do   # wineserver writes system.reg when it shuts down
    saved_video
    if [[ "${SAVED_W}x${SAVED_H}" == "$SIZE" ]]; then log "video mode stored: ${SIZE}x32"; return 0; fi
    sleep 0.5
  done
  printf '%s\n' "$out" | grep -v '^ProtonFixes' | tail -n 6 | sed 's/^/    /'
  die "reg import (rc=$rc) did not store ${SIZE} (saved: ${SAVED_W:-none}x${SAVED_H:-none})"
}

if [[ $MODE != exe ]]; then report_video; fi
if [[ $SYNC_RES -gt 0 ]]; then
  [[ $MODE != exe ]] || die "--sync-res/--sync-res-only apply to the game, not to --exe"
  write_video_mode
  if [[ $SYNC_RES -eq 2 ]]; then log "sync-res-only: done (nothing launched)"; exit 0; fi
fi
if [[ -n "$VDESKTOP" ]]; then
  [[ $USE_GAMESCOPE -eq 1 ]] && log "note: --virtual-desktop inside gamescope is redundant; using it anyway"
  # explorer resolves the program relative to the working directory (= game dir)
  prog="$EXE_ARG"; [[ "$prog" == "$GAME_DIR/"* ]] && prog="${prog#"$GAME_DIR"/}"
  INNER=("${LAUNCH[@]}" explorer "/desktop=HomeworldEmergence,$VDESKTOP" "$prog" "${ARGS[@]}")
else
  INNER=("${LAUNCH[@]}" "$EXE_ARG" "${ARGS[@]}")
fi

if [[ $USE_GAMESCOPE -eq 1 ]]; then
  GS=(gamescope --prefer-vk-device "$VK_DEVICE" --backend sdl -w "$W" -h "$H" -W "$WW" -H "$WH")
  [[ -n "$FPS" ]] && GS+=(-r "$FPS")
  [[ -n "$SCALER" ]] && GS+=(-S "$SCALER")
  [[ -n "$FILTER" ]] && GS+=(-F "$FILTER")
  [[ $GRAB -eq 1 ]] && GS+=(--force-grab-cursor)
  # The inner shell records gamescope's Wayland socket + Xwayland display for screenshot.sh.
  CMD=(env SDL_VIDEODRIVER=x11 HWC_STATE="$STATE" "${GS[@]}" --
       bash -c 'printf "GAMESCOPE_WAYLAND_DISPLAY=%s\nGAMESCOPE_DISPLAY=%s\nINNER_PID=%s\n" "${GAMESCOPE_WAYLAND_DISPLAY:-}" "${DISPLAY:-}" "$$" >> "$HWC_STATE"; "$@"; rc=$?; echo "INNER_EXIT=$rc" >> "$HWC_STATE"; exit $rc'
       hwc-inner env "${ENVV[@]}" "${INNER[@]}")
else
  CMD=(env "${ENVV[@]}" "${INNER[@]}")
fi

log "game dir : $GAME_DIR"
log "launch   : $MODE -> $EXE_ARG ${ARGS[*]:-}"
log "proton   : $PROTON_DIR ($(cat "$PROTON_DIR/version" 2>/dev/null || echo '?'))  prefix: $PREFIX"
log "d3d9     : $([[ $NO_DXVK -eq 1 ]] && echo 'wined3d (PROTON_USE_WINED3D=1)' || echo 'DXVK (Proton default)')"
if [[ $USE_GAMESCOPE -eq 1 ]]; then
  log "display  : nested gamescope, game ${W}x${H} in a ${WW}x${WH} window (windowed; never -f)"
else
  log "display  : directly on the desktop (Xwayland)${VDESKTOP:+, Wine virtual desktop $VDESKTOP}"
fi
log "command  : (cd $(printf '%q' "$GAME_DIR") && $(printf '%q ' "${CMD[@]}"))"

if [[ $DRY_RUN -eq 1 ]]; then log "dry run: not launching"; exit 0; fi

# --- launch -----------------------------------------------------------------------------------
ln -sfn "$(basename -- "$LOG")" "$LOG_DIR/run_game-latest.log"
cd -- "$GAME_DIR"     # CET's loader checks for its files in the CURRENT directory
START_MARK="$(mktemp)"
{
  echo "RUN_PID=$$"
  echo "STARTED=$(date -Iseconds)"
  echo "MODE=$MODE"
  echo "USE_GAMESCOPE=$USE_GAMESCOPE"
  echo "GAME_DIR=$GAME_DIR"
  echo "EXE=$EXE_ARG"
  echo "LOG=$LOG"
  echo "SIZE=$SIZE"
  echo "PREFIX=$PREFIX"
  echo "PROTON_DIR=$PROTON_DIR"
} > "$STATE"

CHILD=""
cleanup() {
  local sig="$1"
  log "caught SIG$sig - stopping game"
  WINEPREFIX="$PREFIX" "$PROTON_DIR/files/bin/wineserver" -k 2>/dev/null || true
  sleep 1
  if [[ -n "$CHILD" ]]; then kill -TERM "$CHILD" 2>/dev/null || true; fi
}
trap 'cleanup INT' INT
trap 'cleanup TERM' TERM

set +e
# timestamp every output line as it arrives (bash builtin strftime; mawk would block-buffer a pipe)
stamp_lines() { local line; while IFS= read -r line || [[ -n "$line" ]]; do printf '%(%Y-%m-%d %H:%M:%S)T %s\n' -1 "$line"; done; }
"${CMD[@]}" > >(stamp_lines) 2>&1 &
CHILD=$!
[[ $USE_GAMESCOPE -eq 1 ]] && echo "GAMESCOPE_PID=$CHILD" >> "$STATE"   # env execs gamescope: same pid
echo "CHILD_PID=$CHILD" >> "$STATE"
log "started pid $CHILD"
wait "$CHILD"; rc=$?
# wait returns early (>128) when a trapped signal arrives; wait again for the real exit
while kill -0 "$CHILD" 2>/dev/null; do wait "$CHILD"; rc=$?; done
set -e
sleep 0.3   # let the timestamping reader drain

if [[ $USE_GAMESCOPE -eq 1 ]]; then
  inner_rc="$(state_get INNER_EXIT)"
  log "gamescope exit status: $rc$([[ $rc -eq 134 ]] && echo ' (SIGABRT on teardown: known harmless gamescope 3.14.24 behaviour)')"
  if [[ -n "$inner_rc" ]]; then rc="$inner_rc"
  else log "no INNER_EXIT recorded: gamescope went down before the game side finished (killed/crashed); reporting gamescope's status"; fi
fi
log "exit status: $rc"
for f in CataclysmEngineTweaks.log rgld3d9.log; do   # CET dllmain.cpp LOG_FILE_NAME, hw_rgl D3D9 d3driver.cpp
  p="$(find -H "$GAME_DIR" -maxdepth 1 -iname "$f" -newer "$START_MARK" -print -quit 2>/dev/null || true)"
  if [[ -n "$p" ]]; then
    log "---- tail of $f ----"
    tail -n 60 -- "$p" | sed 's/^/    /'
  fi
done
rm -f -- "$START_MARK" "$STATE"
log "log: $LOG"
exit "$rc"
