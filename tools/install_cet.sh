#!/usr/bin/env bash
# install_cet.sh - download Cataclysm Engine Tweaks (CET, github.com/ncblakely/CataclysmEngineTweaks)
# into original/cet/ and install it into the extracted game directory, backing up the
# stock rgl.dll / rgld3d.dll (and anything else the zip overwrites) first, per the CET README.
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
LOG_DIR="$PROJECT_ROOT/logs"
TS="$(date +%Y%m%d-%H%M%S)"
LOG="$LOG_DIR/install_cet-$TS.log"
CET_REPO="ncblakely/CataclysmEngineTweaks"
CET_DIR="${HWC_CET_DIR:-$PROJECT_ROOT/original/cet}"
GAME_ROOT="$PROJECT_ROOT/original/game"
GAME_DIR="${HWC_GAME_DIR:-}"

TAG=""            # empty = latest release
ZIP=""
DOWNLOAD_ONLY=0
NO_DOWNLOAD=0
KEEP_INI=0
RESTORE=0
RESTORE_DIR=""
DRY_RUN=0

usage() {
  cat <<USAGE
Usage: tools/install_cet.sh [options]

1. downloads the CET release zip with 'gh release download -R $CET_REPO'
   into $CET_DIR/ (+ RELEASE-<tag>.json, <zip>.sha256)
2. backs up every game file the zip would overwrite (always rgl.dll and rgld3d.dll)
   to <game>/.cet-backup/<timestamp>/
3. unzips into the game directory (the one holding cataclysm.exe) and verifies hashes.
Launch afterwards with tools/run_game.sh (uses cataclysm_loader.exe by default).

Options:
  --download-only    step 1 only (no game needed)
  --tag TAG          release tag (default: latest; e.g. v0.18.03)
  --zip PATH         install this zip instead of downloading
  --no-download      use the newest cet_*.zip already in $CET_DIR
  --game-dir DIR     game directory (default: dir of cataclysm.exe under original/game/)
  --keep-ini         keep an existing CataclysmEngineTweaks.ini (upgrade without losing edits)
  --restore [DIR]    undo: put back the files from the OLDEST (= stock; or the given)
                     .cet-backup/<ts>/ and delete the files CET added
  --dry-run          show what would happen, change nothing
  -h, --help         this help

Logs: $LOG_DIR/install_cet-<timestamp>.log
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --download-only) DOWNLOAD_ONLY=1; shift ;;
    --tag) TAG="${2:?}"; shift 2 ;;
    --zip) ZIP="${2:?}"; shift 2 ;;
    --no-download) NO_DOWNLOAD=1; shift ;;
    --game-dir) GAME_DIR="${2:?}"; shift 2 ;;
    --keep-ini) KEEP_INI=1; shift ;;
    --restore) RESTORE=1; shift
               if [[ $# -gt 0 && "$1" != --* ]]; then RESTORE_DIR="$1"; shift; fi ;;
    --dry-run) DRY_RUN=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "install_cet.sh: unknown option '$1' (try --help)" >&2; exit 2 ;;
  esac
done

mkdir -p "$LOG_DIR"
exec > >(tee -a "$LOG") 2>&1
log() { printf '%s [cet] %s\n' "$(date '+%F %T')" "$*"; }
die() { printf '%s [cet] ERROR: %s\n' "$(date '+%F %T')" "$*"; exit 1; }
run() { log "+ $*"; if [[ $DRY_RUN -eq 0 ]]; then "$@"; fi; }
log "log file: $LOG"

find_game_dir() {
  if [[ -n "$GAME_DIR" ]]; then
    [[ -d "$GAME_DIR" ]] || die "--game-dir $GAME_DIR does not exist"
    GAME_DIR="$(cd -- "$GAME_DIR" && pwd)"
  else
    local exe
    exe="$(find "$GAME_ROOT" -maxdepth 4 -type f -iname 'cataclysm.exe' ! -path '*/.cet-backup/*' -print -quit 2>/dev/null || true)"
    [[ -n "$exe" ]] || die "no cataclysm.exe under $GAME_ROOT - run tools/extract_installer.sh first"
    GAME_DIR="$(dirname -- "$exe")"
  fi
  [[ -n "$(find "$GAME_DIR" -maxdepth 1 -iname 'cataclysm.exe' -print -quit)" ]] \
    || die "$GAME_DIR has no cataclysm.exe (CET's loader needs it in the same directory: loader/Main.cpp:19)"
  log "game dir: $GAME_DIR"
}

# ------------------------------------------------------------------ restore
if [[ $RESTORE -eq 1 ]]; then
  find_game_dir
  if [[ -z "$RESTORE_DIR" ]]; then
    # oldest backup = the one taken from the stock files by the first install
    RESTORE_DIR="$(ls -1d "$GAME_DIR"/.cet-backup/*/ 2>/dev/null | sort | head -n1 || true)"
  fi
  RESTORE_DIR="${RESTORE_DIR%/}"
  [[ -n "$RESTORE_DIR" && -d "$RESTORE_DIR" ]] || die "no backup found under $GAME_DIR/.cet-backup/"
  log "restoring from $RESTORE_DIR"
  if [[ -f "$RESTORE_DIR/CET_ADDED.txt" ]]; then
    while IFS= read -r f; do
      [[ -n "$f" && -e "$GAME_DIR/$f" ]] && run rm -f -- "$GAME_DIR/$f"
    done < "$RESTORE_DIR/CET_ADDED.txt"
  fi
  while IFS= read -r -d '' f; do
    rel="${f#"$RESTORE_DIR"/}"
    case "$rel" in BACKUP_MANIFEST.sha256|CET_ADDED.txt|CET_ZIP.txt) continue ;; esac
    run cp -p -- "$f" "$GAME_DIR/$rel"
  done < <(find "$RESTORE_DIR" -type f -print0)
  log "restored. Check against the pristine manifest with: tools/extract_installer.sh --verify"
  exit 0
fi

# ------------------------------------------------------------------ download
download() {
  command -v gh >/dev/null 2>&1 || return 1
  gh auth status >/dev/null 2>&1 || log "gh not logged in - trying anyway (public repo)"
  run mkdir -p "$CET_DIR"
  local args=(release download)
  [[ -n "$TAG" ]] && args+=("$TAG")
  args+=(-R "$CET_REPO" -p 'cet_*.zip' -D "$CET_DIR" --clobber)
  run gh "${args[@]}" || return 1
  local view=(release view)
  [[ -n "$TAG" ]] && view+=("$TAG")
  if [[ $DRY_RUN -eq 0 ]]; then
    local meta tag
    meta="$(gh "${view[@]}" -R "$CET_REPO" --json tagName,name,publishedAt,url,body,assets 2>/dev/null || true)"
    tag="$(python3 -c 'import json,sys; print(json.loads(sys.stdin.read() or "{}").get("tagName","unknown"))' <<<"$meta")"
    [[ -n "$meta" ]] && printf '%s\n' "$meta" > "$CET_DIR/RELEASE-$tag.json"
    log "release: $tag ($(python3 -c 'import json,sys; d=json.loads(sys.stdin.read() or "{}"); print(d.get("publishedAt",""), d.get("url",""))' <<<"$meta"))"
  fi
}
download_curl() {  # fallback without gh
  local api="https://api.github.com/repos/$CET_REPO/releases/${TAG:+tags/}${TAG:-latest}" url
  run mkdir -p "$CET_DIR"
  url="$(curl -fsSL "$api" | python3 -c 'import json,sys; a=[x["browser_download_url"] for x in json.load(sys.stdin)["assets"] if x["name"].startswith("cet_") and x["name"].endswith(".zip")]; print(a[0] if a else "")')"
  [[ -n "$url" ]] || die "no cet_*.zip asset found via $api"
  run curl -fL --retry 3 -o "$CET_DIR/$(basename -- "$url")" "$url"
}

if [[ -z "$ZIP" ]]; then
  if [[ $NO_DOWNLOAD -eq 0 ]]; then
    download || { log "gh download failed - falling back to curl + GitHub API"; download_curl; }
  fi
  if [[ $DRY_RUN -eq 1 && -z "$(ls "$CET_DIR"/cet_*.zip 2>/dev/null)" ]]; then
    log "dry run: no zip present yet; stopping"; exit 0
  fi
  if [[ -n "$TAG" ]]; then
    ZIP="$(ls -1 "$CET_DIR"/cet_"${TAG#v}".zip 2>/dev/null | head -n1 || true)"
  fi
  [[ -n "$ZIP" ]] || ZIP="$(ls -1 "$CET_DIR"/cet_*.zip 2>/dev/null | sort -V | tail -n1 || true)"
fi
[[ -n "$ZIP" && -f "$ZIP" ]] || die "CET zip not found (looked in $CET_DIR)"
ZIP_SHA="$(sha256sum -- "$ZIP" | cut -d' ' -f1)"
[[ $DRY_RUN -eq 1 ]] || printf '%s  %s\n' "$ZIP_SHA" "$(basename -- "$ZIP")" > "$ZIP.sha256"
log "zip: $ZIP"
log "sha256: $ZIP_SHA"
log "contents:"
unzip -l -- "$ZIP" | sed 's/^/    /'

mapfile -t ENTRIES < <(unzip -Z1 -- "$ZIP" | grep -v '/$')
log "entries: ${ENTRIES[*]}"

if [[ $DOWNLOAD_ONLY -eq 1 ]]; then
  log "download-only: done (install later with: tools/install_cet.sh --no-download)"
  exit 0
fi

# ------------------------------------------------------------------ install
find_game_dir
same=0
ini_edited=0
for e in "${ENTRIES[@]}"; do
  [[ -f "$GAME_DIR/$e" ]] || break
  if [[ "$(unzip -p -- "$ZIP" "$e" | sha256sum | cut -d' ' -f1)" != "$(sha256sum -- "$GAME_DIR/$e" | cut -d' ' -f1)" ]]; then
    # CataclysmEngineTweaks.ini is the user's settings file (RUNBOOK troubleshooting edits it):
    # an edited copy still counts as installed, so a re-run never resets it to the defaults.
    if [[ "$e" == CataclysmEngineTweaks.ini ]]; then ini_edited=1; else break; fi
  fi
  same=$((same + 1))
done
if [[ $same -eq ${#ENTRIES[@]} ]]; then
  log "already installed: all ${#ENTRIES[@]} files in $GAME_DIR match $(basename -- "$ZIP") - nothing to do (no new backup taken)"
  if [[ $ini_edited -eq 1 ]]; then
    log "note: CataclysmEngineTweaks.ini differs from the zip (your edits are kept); for the defaults: unzip -p '$ZIP' CataclysmEngineTweaks.ini"
  fi
  exit 0
fi
BK="$GAME_DIR/.cet-backup/$TS"
run mkdir -p "$BK"
ADDED=()
BACKED=()
for e in "${ENTRIES[@]}"; do
  # Wine resolves names case-insensitively; catch e.g. RGL.DLL vs rgl.dll so we never end up
  # with two copies that differ only in case.
  existing="$(find "$GAME_DIR" -maxdepth 1 -iname "$(basename -- "$e")" -print -quit 2>/dev/null || true)"
  if [[ -n "$existing" ]]; then
    run cp -p -- "$existing" "$BK/$(basename -- "$existing")"
    BACKED+=("$(basename -- "$existing" | tr '[:upper:]' '[:lower:]')")
    if [[ "$(basename -- "$existing")" != "$(basename -- "$e")" ]]; then
      log "case differs: $(basename -- "$existing") vs zip $(basename -- "$e") - removing old name after backup"
      run rm -f -- "$existing"
    fi
  else
    ADDED+=("$e")
  fi
done
for must in rgl.dll rgld3d.dll; do   # CET README step 2
  if [[ " ${BACKED[*]} " != *" $must "* ]]; then
    src="$(find "$GAME_DIR" -maxdepth 1 -iname "$must" -print -quit)"
    [[ -n "$src" ]] && run cp -p -- "$src" "$BK/"
  fi
done
if [[ $DRY_RUN -eq 0 ]]; then
  printf '%s\n' "${ADDED[@]}" > "$BK/CET_ADDED.txt"
  printf '%s  %s\n' "$ZIP_SHA" "$(basename -- "$ZIP")" > "$BK/CET_ZIP.txt"
  ( cd "$BK" && find . -maxdepth 1 -type f ! -name '*.txt' ! -name '*.sha256' -printf '%f\0' | sort -z | xargs -0 -r sha256sum > BACKUP_MANIFEST.sha256 )
  log "backup in $BK:"; ls -la "$BK" | sed 's/^/    /'
fi

OLD_INI=""
if [[ $KEEP_INI -eq 1 && -f "$BK/CataclysmEngineTweaks.ini" ]]; then OLD_INI="$BK/CataclysmEngineTweaks.ini"; fi

run unzip -o -q -- "$ZIP" -d "$GAME_DIR"
if [[ -n "$OLD_INI" ]]; then
  run cp -p -- "$OLD_INI" "$GAME_DIR/CataclysmEngineTweaks.ini"
  log "kept previous CataclysmEngineTweaks.ini (--keep-ini)"
fi

if [[ $DRY_RUN -eq 0 ]]; then
  bad=0
  for e in "${ENTRIES[@]}"; do
    [[ -n "$OLD_INI" && "$e" == CataclysmEngineTweaks.ini ]] && continue
    a="$(unzip -p -- "$ZIP" "$e" | sha256sum | cut -d' ' -f1)"
    b="$(sha256sum -- "$GAME_DIR/$e" | cut -d' ' -f1)"
    if [[ "$a" == "$b" ]]; then log "ok   $e  $b"; else log "BAD  $e"; bad=1; fi
  done
  [[ $bad -eq 0 ]] || die "installed files do not match the zip"
fi

cat <<NOTE

CET installed into $GAME_DIR
  backup of replaced files : $BK
  undo                     : tools/install_cet.sh --restore
  launch                   : tools/run_game.sh            (cataclysm_loader.exe -> injects CataclysmEngineTweaks.dll)
  stock exe without CET    : tools/run_game.sh --original (CET's rgl.dll/rgld3d.dll stay in place; --restore for stock DLLs)
CET .ini defaults that matter here (CataclysmEngineTweaks.ini):
  NewRendererSelection=true  first start (device CRC mismatch) switches to OpenGL at the
                             desktop resolution (PatchRGL.cpp InitWindow_DeviceCrcCheck)
  BorderlessWindow=true      forces /window /noBorder (Main.cpp ApplyStartupPatches)
  DisableIntroMovies=true    UpdateRateShift=2 (64 Hz universe; 0 = stock 16 Hz)
  The bundled D3D9 renderer (rgld3d.dll -> d3d9.dll -> DXVK) is chosen in the in-game video options.
NOTE
log "log: $LOG"
