#!/usr/bin/env bash
# setup_prefix.sh - create/verify the dedicated Wine prefix for Homeworld: Emergence
# using GE-Proton10-34 through umu-launcher (no Steam client needed).
#
# Why umu: it runs Proton inside the Steam Linux Runtime 3.0 "sniper" container that
# GE-Proton's toolmanifest.vdf requires ("require_tool_appid" "1628350"), downloads that
# runtime itself (~/.local/share/umu/steamrt3), and needs no running Steam. A plain
# 'proton run' (--direct-proton) also works on this box but runs on host libraries
# and does not relay console output. See docs/RUNBOOK.md.
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
LOG_DIR="$PROJECT_ROOT/logs"
TS="$(date +%Y%m%d-%H%M%S)"
LOG="$LOG_DIR/setup_prefix-$TS.log"

PROTON="${HWC_PROTON:-GE-Proton10-34}"
PREFIX="${HWC_PREFIX:-$PROJECT_ROOT/prefix}"
GAMEID="${HWC_GAMEID:-umu-1766413869}"   # umu-<GOG product id>: Proton sets SteamAppId=1766413869,
                                          # so Wine windows get WM_CLASS steam_app_1766413869
UMU_VERSION="${HWC_UMU_VERSION:-1.4.4}"
UMU_TAR_SHA256_1_4_4="eb590691841f7fad3fc3ad8fd5db4ccb87849fe7948e62b28ece7a4ee48cc851"
UMU_HOME="$HOME/.local/share/umu-launcher"
UMU_BIN_LINK="$HOME/.local/bin/umu-run"
STEAM_ROOT="${HWC_STEAM_ROOT:-$HOME/.local/share/Steam}"

DIRECT=0
REINSTALL_UMU=0
CHECK_ONLY=0
DRY_RUN=0
WINETRICKS=()

usage() {
  cat <<USAGE
Usage: tools/setup_prefix.sh [options]

Creates (or verifies) the Wine prefix
  $PREFIX
with Proton '$PROTON' via umu-run (installed to ~/.local if missing), then runs
'cmd /c ver' inside it headlessly and reports the graphics DLL setup.

Options:
  --proton NAME|DIR  Proton build (default $PROTON). NAME is looked up in
                     $STEAM_ROOT/compatibilitytools.d/ then steamapps/common/
  --prefix DIR       prefix location (default $PREFIX)
  --direct-proton    do not use umu: call '<proton>/proton waitforexitandrun' with
                     STEAM_COMPAT_CLIENT_INSTALL_PATH / STEAM_COMPAT_DATA_PATH (host libs)
  --reinstall-umu    re-download umu-launcher $UMU_VERSION (zipapp) even if umu-run exists
  --check            only verify an existing prefix (no creation, no downloads)
  --winetricks V..   after setup, run 'umu-run winetricks V..' in the prefix (all
                     remaining args are verbs), e.g. --winetricks win98   (none needed so far)
  --dry-run          print the commands, change nothing
  -h, --help         this help

Environment overrides: HWC_PROTON HWC_PREFIX HWC_GAMEID HWC_UMU_VERSION HWC_STEAM_ROOT
Logs: $LOG_DIR/setup_prefix-<timestamp>.log
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --proton) PROTON="${2:?}"; shift 2 ;;
    --prefix) PREFIX="${2:?}"; shift 2 ;;
    --direct-proton) DIRECT=1; shift ;;
    --reinstall-umu) REINSTALL_UMU=1; shift ;;
    --check) CHECK_ONLY=1; shift ;;
    --dry-run) DRY_RUN=1; shift ;;
    --winetricks) shift; WINETRICKS=("$@"); break ;;
    -h|--help) usage; exit 0 ;;
    *) echo "setup_prefix.sh: unknown option '$1' (try --help)" >&2; exit 2 ;;
  esac
done

mkdir -p "$LOG_DIR"
exec > >(tee -a "$LOG") 2>&1
log() { printf '%s [prefix] %s\n' "$(date '+%F %T')" "$*"; }
die() { printf '%s [prefix] ERROR: %s\n' "$(date '+%F %T')" "$*"; exit 1; }
run() { log "+ $*"; if [[ $DRY_RUN -eq 0 ]]; then "$@"; fi; }
log "log file: $LOG"

# --- resolve Proton ------------------------------------------------------------
resolve_proton() {
  local p="$PROTON"
  if [[ -d "$p" && -f "$p/proton" ]]; then PROTON_DIR="$(cd -- "$p" && pwd)"; return; fi
  for d in "$STEAM_ROOT/compatibilitytools.d/$p" "$STEAM_ROOT/steamapps/common/$p"; do
    if [[ -f "$d/proton" ]]; then PROTON_DIR="$d"; return; fi
  done
  die "Proton '$p' not found (looked in $STEAM_ROOT/compatibilitytools.d and steamapps/common)"
}
resolve_proton
PROTON_VERSION="$(cat "$PROTON_DIR/version" 2>/dev/null || echo '?')"
log "proton: $PROTON_DIR (version file: $PROTON_VERSION)"
if grep -q '"require_tool_appid" "1628350"' "$PROTON_DIR/toolmanifest.vdf" 2>/dev/null; then
  log "proton toolmanifest requires Steam Linux Runtime 3.0 (sniper, appid 1628350) - umu provides it"
fi

# --- umu-launcher ----------------------------------------------------------------
umu_ok() { command -v umu-run >/dev/null 2>&1 && umu-run --version >/dev/null 2>&1; }

install_umu() {
  # pip is not an option: Pop!_OS 24.04 Python is PEP 668 "externally managed" and
  # umu-launcher is not published on PyPI (checked 2026-10-08). The GitHub release
  # ships a self-contained zipapp (umu-launcher-<ver>-zipapp.tar -> umu/umu-run).
  local tmp tarball url
  tmp="$(mktemp -d)"
  tarball="umu-launcher-$UMU_VERSION-zipapp.tar"
  url="https://github.com/Open-Wine-Components/umu-launcher/releases/download/$UMU_VERSION/$tarball"
  if command -v gh >/dev/null 2>&1 && gh auth status >/dev/null 2>&1; then
    run gh release download "$UMU_VERSION" -R Open-Wine-Components/umu-launcher -p "$tarball" -D "$tmp" --clobber
  else
    run curl -fL --retry 3 -o "$tmp/$tarball" "$url"
  fi
  if [[ $DRY_RUN -eq 1 ]]; then rm -rf -- "$tmp"; return 0; fi
  local got; got="$(sha256sum "$tmp/$tarball" | cut -d' ' -f1)"
  log "umu tarball sha256: $got"
  if [[ "$UMU_VERSION" == "1.4.4" && "$got" != "$UMU_TAR_SHA256_1_4_4" ]]; then
    die "sha256 mismatch for $tarball (expected $UMU_TAR_SHA256_1_4_4)"
  fi
  tar -xf "$tmp/$tarball" -C "$tmp"
  mkdir -p "$UMU_HOME/$UMU_VERSION" "$(dirname -- "$UMU_BIN_LINK")"
  install -m 755 "$tmp/umu/umu-run" "$UMU_HOME/$UMU_VERSION/umu-run"
  ln -sfn "$UMU_HOME/$UMU_VERSION/umu-run" "$UMU_BIN_LINK"
  rm -rf -- "$tmp"
  hash -r
  log "installed umu-run -> $UMU_HOME/$UMU_VERSION/umu-run (symlink $UMU_BIN_LINK)"
  case ":$PATH:" in *":$HOME/.local/bin:"*) ;; *) log "WARNING: ~/.local/bin is not on PATH";; esac
}

if [[ $DIRECT -eq 0 ]]; then
  if [[ $REINSTALL_UMU -eq 1 ]] || ! umu_ok; then
    [[ $CHECK_ONLY -eq 1 ]] && die "umu-run missing (run without --check to install it)"
    install_umu
  fi
  [[ $DRY_RUN -eq 1 ]] || log "umu: $(umu-run --version 2>&1 | head -n1)"
fi

# --- run a command in the prefix ---------------------------------------------------
# All game/tool launches use this same environment (run_game.sh mirrors it).
umu_env=(WINEPREFIX="$PREFIX" GAMEID="$GAMEID" STORE=gog PROTONPATH="$PROTON_DIR" PROTON_VERB=waitforexitandrun)
direct_env=(STEAM_COMPAT_CLIENT_INSTALL_PATH="$STEAM_ROOT" STEAM_COMPAT_DATA_PATH="$PREFIX"
            SteamAppId=1766413869 SteamGameId=1766413869 STORE=gog)

in_prefix() {  # args: windows command...
  if [[ $DIRECT -eq 1 ]]; then
    env "${direct_env[@]}" "$PROTON_DIR/proton" waitforexitandrun "$@"
  else
    env "${umu_env[@]}" umu-run "$@"
  fi
}

prefix_ok() { [[ -f "$PREFIX/system.reg" && -f "$PREFIX/version" && -d "$PREFIX/drive_c/windows/syswow64" ]]; }

if [[ $CHECK_ONLY -eq 1 ]]; then
  prefix_ok || die "no usable prefix at $PREFIX"
  log "prefix exists: $PREFIX (created by $(cat "$PREFIX/version"))"
else
  if prefix_ok; then
    log "prefix already exists: $PREFIX ($(cat "$PREFIX/version")); Proton upgrades it in place if versions differ"
  else
    log "creating prefix at $PREFIX$([[ $DIRECT -eq 0 && ! -d $HOME/.local/share/umu/steamrt3 ]] && echo ' (first umu run also downloads the sniper runtime, ~800 MB)')"
  fi
  if [[ $DIRECT -eq 1 ]]; then
    # Keep the umu layout (WINEPREFIX == STEAM_COMPAT_DATA_PATH, pfx -> .) so both modes share one prefix.
    run mkdir -p "$PREFIX"
    [[ -e "$PREFIX/pfx" || $DRY_RUN -eq 1 ]] || run ln -s . "$PREFIX/pfx"
  fi
fi

# Proton creates/upgrades the prefix on first use; 'cmd /c ver' is a headless probe.
log "probe: cmd /c ver inside the prefix"
VER_OUT=""
if [[ $DRY_RUN -eq 0 ]]; then
  set +e
  VER_OUT="$(in_prefix cmd /c ver 2>&1)"
  rc=$?
  VER_OUT="$(grep -v '^ProtonFixes' <<<"$VER_OUT" || true)"
  set -e
  printf '%s\n' "$VER_OUT" | sed 's/^/    /'
else
  log "+ ${umu_env[*]} umu-run cmd /c ver"
fi

if [[ $DRY_RUN -eq 1 ]]; then
  [[ ${#WINETRICKS[@]} -gt 0 ]] && log "+ ${umu_env[*]} umu-run winetricks ${WINETRICKS[*]}"
  log "dry run done"; exit 0
fi
prefix_ok || die "prefix was not created (see output above)"
if grep -q 'Microsoft Windows' <<<"$VER_OUT"; then
  log "OK: $(grep -o 'Microsoft Windows [0-9.]*' <<<"$VER_OUT" | head -n1)"
elif [[ $DIRECT -eq 1 ]]; then
  log "direct proton does not relay console output (observed); prefix files exist, treating as OK"
else
  die "cmd /c ver did not answer (rc=$rc)"
fi

# --- report graphics/runtime DLL setup ------------------------------------------------
dll_kind() {  # $1 = path
  local f="$1"
  [[ -e "$f" ]] || { echo "missing"; return; }
  if grep -qa 'dxvk' "$f" 2>/dev/null; then echo "DXVK (Vulkan)";
  elif grep -qa 'vkd3d-proton' "$f" 2>/dev/null; then echo "vkd3d-proton";
  elif grep -qa 'Wine builtin DLL' "$f" 2>/dev/null; then echo "Wine builtin";
  else echo "other ($(stat -c %s "$f") bytes)"; fi
}
S32="$PREFIX/drive_c/windows/syswow64"
log "32-bit DLLs the game path touches (syswow64):"
for d in ddraw d3d8 d3d9 opengl32 dsound msvcp140 vcruntime140; do
  printf '    %-17s %s\n' "$d.dll" "$(dll_kind "$S32/$d.dll")"
done
log "renderer mapping: rgld3d.dll (stock = DirectDraw/D3D7) -> Wine ddraw -> wined3d  (DXVK never applies)"
log "                  rgld3d.dll (CET/hw_rgl = Direct3D 9)  -> d3d9 -> DXVK by default; PROTON_USE_WINED3D=1 -> wined3d"
log "                  OpenGL (opengl32.dll)                 -> Wine opengl32 -> host NVIDIA GL"

if [[ ${#WINETRICKS[@]} -gt 0 ]]; then
  [[ $DIRECT -eq 1 ]] && die "--winetricks needs umu (drop --direct-proton)"
  run env "${umu_env[@]}" umu-run winetricks "${WINETRICKS[@]}"
fi

cat <<SUMMARY

prefix     : $PREFIX
proton     : $PROTON_DIR ($PROTON_VERSION)
launcher   : $([[ $DIRECT -eq 1 ]] && echo "direct proton (host libraries)" || echo "umu-run (Steam Linux Runtime sniper container)")
runtime    : $HOME/.local/share/umu/steamrt3
same env by hand:
  WINEPREFIX=$PREFIX GAMEID=$GAMEID STORE=gog PROTONPATH=$PROTON_DIR umu-run <program.exe> [args]
log        : $LOG
SUMMARY
