#!/usr/bin/env bash
# extract_installer.sh - unpack the GOG offline installer for Homeworld: Emergence
# (Homeworld: Cataclysm) into original/game/ with innoextract, write a pristine
# MANIFEST.sha256 and report the game executables (file type, PE header, size).
#
# Refuses to run on GOG's Galaxy download stub (GOG_Galaxy_Homeworld_Emergence.exe),
# which contains no game data. See docs/RUNBOOK.md.
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
# HWC_ORIG_DIR / HWC_GAME_ROOT exist only so the script can be tested on fixtures.
ORIG_DIR="${HWC_ORIG_DIR:-$PROJECT_ROOT/original}"
GAME_ROOT="${HWC_GAME_ROOT:-$ORIG_DIR/game}"
# original/ or original/game may be symlinks to another disk: work on the real directory, so
# --force moves the real game dir aside (not just the link) and re-extracts on that disk.
GAME_ROOT="$(readlink -f -- "$GAME_ROOT" 2>/dev/null || printf '%s' "$GAME_ROOT")"
# innoextract built from git master (Inno Setup 6.3/6.4.0 + 2025 GOG Galaxy fixes) lives here:
IE_BUILD_ROOT="${HWC_IE_BUILD_ROOT:-$PROJECT_ROOT/build/innoextract-git}"
INNOEXTRACT="${HWC_INNOEXTRACT:-}"
LOG_DIR="$PROJECT_ROOT/logs"
TS="$(date +%Y%m%d-%H%M%S)"
LOG="$LOG_DIR/extract_installer-$TS.log"

# Defaults for the Wine fallback (only used with --wine-install). GE-Proton10-34 cannot run
# Inno Setup stubs: they carry DYNAMIC_BASE with relocations stripped, GE's Wine relocates them
# anyway and they crash (tested 2026-10-08). Proton Experimental runs them. A throwaway prefix
# keeps the game prefix clean.
INSTALLER_PROTON="${HWC_INSTALLER_PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton - Experimental}"
INSTALLER_PREFIX="${HWC_INSTALLER_PREFIX:-$PROJECT_ROOT/build/installer-prefix}"
GAMEID="${HWC_GAMEID:-umu-1766413869}"   # GOG ProductID 1766413869 (from the Galaxy stub's VERSIONINFO)
SILENT=0

INSTALLER=""
LANGUAGE="${HWC_LANGUAGE:-english}"
BUILD_IE=0
FORCE=0
MODE=extract          # extract | list | verify | info | wine-install
DRY_RUN=0

usage() {
  cat <<USAGE
Usage: tools/extract_installer.sh [options]

Unpacks setup_homeworld_emergence*.exe (+ any setup_*-N.bin parts) from
  $ORIG_DIR
into
  $GAME_ROOT
with 'innoextract --gog', then writes $GAME_ROOT/MANIFEST.sha256
(sha256sum format) + MANIFEST.sizes + EXTRACT_INFO.txt, copies __support/app/*
(Cataclysm.cfg) into the game root like GOG's installer does,
and prints every .exe found with 'file' output, PE timestamp, image base and size.

Options:
  --installer PATH   use this installer instead of auto-detecting in original/
  --language L       installer language (default english; the 1.01 hotfix (12948) installer
                     also offers german french russian brazilian chinese). Passed for
                     determinism: in build 12948 every language lists the same 68 app/
                     entries (66 unique files; goggame-*.ico and webcache.zip appear twice).
  --force            original/game/ is not empty: move it aside to
                     original/game.prev-<timestamp>/ and extract fresh
  --list             only list the installer contents (innoextract --list-sizes), write nothing
  --info             only print installer info (innoextract --info, --gog-game-id), write nothing
  --verify           re-check original/game/ against MANIFEST.sha256 / MANIFEST.sizes
                     and list files that are not in the manifest
  --report           only (re)print the exe report for an existing original/game/
  --innoextract PATH  use this innoextract binary
  --build-innoextract  (then exit) clone + build innoextract git master into
                     build/innoextract-git/ (needs cmake g++ liblzma-dev libboost-{iostreams,
                     filesystem,date-time,system,program-options}-dev); used automatically
                     afterwards. Distro innoextract 1.9 handles Inno Setup <= 6.2;
                     git master (1.10-dev) handles 6.3/6.4.0 and newer GOG Galaxy layouts.
  --wine-install     FALLBACK if innoextract cannot parse the installer: run GOG's real
                     installer GUI under Proton Experimental in build/installer-prefix with
                     /DIR=original/game preset - YOU click through it. (GE-Proton10-34 crashes
                     on Inno stubs; GOG's wizard aborts at the Welcome page with /VERYSILENT.)
  --silent           with --wine-install: try /VERYSILENT anyway
  --dry-run          print what would be done, change nothing
  -h, --help         this help

Logs: $LOG_DIR/extract_installer-<timestamp>.log
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --installer) INSTALLER="${2:?--installer needs a path}"; shift 2 ;;
    --installer=*) INSTALLER="${1#*=}"; shift ;;
    --force) FORCE=1; shift ;;
    --language) LANGUAGE="${2:?--language needs a value}"; shift 2 ;;
    --innoextract) INNOEXTRACT="${2:?--innoextract needs a path}"; shift 2 ;;
    --build-innoextract) BUILD_IE=1; shift ;;
    --list) MODE=list; shift ;;
    --info) MODE=info; shift ;;
    --verify) MODE=verify; shift ;;
    --report) MODE=report; shift ;;
    --wine-install) MODE=wine-install; shift ;;
    --silent) SILENT=1; shift ;;
    --dry-run) DRY_RUN=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "extract_installer.sh: unknown option '$1' (try --help)" >&2; exit 2 ;;
  esac
done

mkdir -p "$LOG_DIR"
exec > >(tee -a "$LOG") 2>&1
log()  { printf '%s [extract] %s\n' "$(date '+%F %T')" "$*"; }
die()  { printf '%s [extract] ERROR: %s\n' "$(date '+%F %T')" "$*"; exit 1; }
run()  { log "+ $*"; if [[ $DRY_RUN -eq 0 ]]; then "$@"; fi; }

log "log file: $LOG"
log "project root: $PROJECT_ROOT"

GOG_HOWTO="Get the OFFLINE installer: log in at https://www.gog.com -> avatar menu -> Games
(library) -> Homeworld: Emergence -> 'Download offline backup game installers' -> Windows ->
English -> download setup_homeworld_emergence_*.exe (and any setup_homeworld_emergence_*-N.bin
parts listed with it) -> put them in $ORIG_DIR/ (keep the original file names)."

# --- Galaxy stub detection ---------------------------------------------------
is_galaxy_stub() {  # $1 = file; true if it is the GOG Galaxy downloader stub
  local f="$1" base
  base="$(basename -- "$f")"
  [[ "$base" == GOG_Galaxy_* ]] && return 0
  # VERSIONINFO of the stub: InternalName "GOG Galaxy - Game Installer.exe"
  if command -v strings >/dev/null 2>&1; then
    strings -el -n 8 -- "$f" 2>/dev/null | grep -q 'GOG Galaxy - Game Installer' && return 0
  fi
  return 1
}

refuse_stub() {
  cat <<MSG
------------------------------------------------------------------------------
REFUSING: '$1' is the GOG Galaxy downloader stub, not the game installer.
It is a ~0.5 MB UPX-packed launcher (VERSIONINFO InternalName "GOG Galaxy - Game
Installer.exe", ProductID 1766413869) that only installs/drives GOG Galaxy; it
contains no Homeworld data and innoextract cannot unpack it.

$GOG_HOWTO
------------------------------------------------------------------------------
MSG
  exit 3
}

# --- installer discovery -----------------------------------------------------
find_installer() {
  if [[ -n "$INSTALLER" ]]; then
    [[ -f "$INSTALLER" ]] || die "installer not found: $INSTALLER"
    INSTALLER="$(cd -- "$(dirname -- "$INSTALLER")" && pwd)/$(basename -- "$INSTALLER")"
    is_galaxy_stub "$INSTALLER" && refuse_stub "$INSTALLER"
    return 0
  fi
  local cands=()
  while IFS= read -r -d '' f; do cands+=("$f"); done < <(
    find -L "$ORIG_DIR" -maxdepth 1 -type f -iname 'setup_homeworld_emergence*.exe' -print0 2>/dev/null | sort -z)
  if [[ ${#cands[@]} -eq 0 ]]; then
    local stub
    stub="$(find -L "$ORIG_DIR" -maxdepth 1 -type f -iname 'GOG_Galaxy_*.exe' -print -quit 2>/dev/null || true)"
    if [[ -n "$stub" ]]; then refuse_stub "$stub"; fi
    log "no setup_homeworld_emergence*.exe in $ORIG_DIR"
    log "files present:"; ls -la "$ORIG_DIR" || true
    printf '%s\n' "$GOG_HOWTO"
    exit 2
  fi
  if [[ ${#cands[@]} -gt 1 ]]; then
    log "several installers found; using the newest (pass --installer to choose):"
    printf '    %s\n' "${cands[@]}"
    INSTALLER="$(ls -t -- "${cands[@]}" | head -n1)"
  else
    INSTALLER="${cands[0]}"
  fi
  is_galaxy_stub "$INSTALLER" && refuse_stub "$INSTALLER"
  return 0
}

describe_installer() {
  local size base stem
  size=$(stat -c %s -- "$INSTALLER")
  base="$(basename -- "$INSTALLER")"; stem="${base%.*}"
  log "installer: $INSTALLER"
  log "  size: $size bytes ($(numfmt --to=iec "$size" 2>/dev/null || echo "$size"))"
  log "  file: $(file -b -- "$INSTALLER")"
  if [[ $size -lt 50000000 ]]; then
    log "  WARNING: under 50 MB - the real offline installer is expected to be hundreds of MB."
  fi
  local parts=()
  while IFS= read -r -d '' p; do parts+=("$p"); done < <(
    find -L "$(dirname -- "$INSTALLER")" -maxdepth 1 -type f -iname "${stem}-*.bin" -print0 2>/dev/null | sort -z)
  if [[ ${#parts[@]} -gt 0 ]]; then
    log "  .bin parts (innoextract picks these up automatically from the same directory):"
    local p; for p in "${parts[@]}"; do log "    $(basename -- "$p")  $(stat -c %s -- "$p") bytes"; done
  else
    log "  no ${stem}-N.bin parts next to it (fine for a single-file installer)"
  fi
  if [[ $DRY_RUN -eq 0 ]]; then
    log "  sha256: $(sha256sum -- "$INSTALLER" | cut -d' ' -f1)"
  fi
}

# --- PE / exe report ---------------------------------------------------------
pe_report() {  # args: files...
  python3 - "$@" <<'PY'
import sys, struct, datetime, hashlib, os, re
CH = {0x0001:'RELOCS_STRIPPED',0x0002:'EXECUTABLE',0x0020:'LARGE_ADDRESS_AWARE',0x0100:'32BIT',0x2000:'DLL'}
DCH = {0x0020:'HIGH_ENTROPY_VA',0x0040:'DYNAMIC_BASE',0x0100:'NX_COMPAT',0x0400:'NO_SEH',0x8000:'TS_AWARE'}
SUB = {1:'native',2:'GUI',3:'console'}
MACH = {0x14c:'i386',0x8664:'x86-64',0xaa64:'arm64'}
def flags(v, table): return '|'.join(n for b,n in table.items() if v & b) or '0'
for path in sys.argv[1:]:
    try:
        d = open(path,'rb').read()
    except OSError as e:
        print(f"  {path}: {e}"); continue
    rel = path
    print(f"== {rel}")
    print(f"   size      : {len(d)} bytes")
    print(f"   sha256    : {hashlib.sha256(d).hexdigest()}")
    if d[:2] != b'MZ':
        print("   not an MZ/PE file"); continue
    e = struct.unpack_from('<I', d, 0x3c)[0]
    if d[e:e+4] != b'PE\0\0':
        print("   MZ without PE header (DOS stub?)"); continue
    machine, nsec, ts, _, _, optsz, ch = struct.unpack_from('<HHIIIHH', d, e+4)
    opt = e + 24
    magic = struct.unpack_from('<H', d, opt)[0]
    entry = struct.unpack_from('<I', d, opt+16)[0]
    if magic == 0x20b:
        base = struct.unpack_from('<Q', d, opt+24)[0]; kind = 'PE32+'
    else:
        base = struct.unpack_from('<I', d, opt+28)[0]; kind = 'PE32'
    soi = struct.unpack_from('<I', d, opt+56)[0]
    sub, dch = struct.unpack_from('<HH', d, opt+68)
    lmaj, lmin = d[opt+2], d[opt+3]
    when = datetime.datetime.fromtimestamp(ts, datetime.timezone.utc).strftime('%Y-%m-%d %H:%M:%S UTC')
    secs = []
    so = opt + optsz
    for i in range(nsec):
        n = d[so+40*i:so+40*i+8].rstrip(b'\0').decode('latin-1')
        vsz, va, rsz = struct.unpack_from('<III', d, so+40*i+8)
        secs.append(f"{n}(va=0x{va:x},vsz=0x{vsz:x},raw=0x{rsz:x})")
    print(f"   format    : {kind} {MACH.get(machine, hex(machine))}, subsystem {SUB.get(sub, sub)}, linker {lmaj}.{lmin}")
    print(f"   timestamp : 0x{ts:08x} = {when}")
    print(f"   imagebase : 0x{base:x}   entry RVA 0x{entry:x}   SizeOfImage 0x{soi:x}")
    print(f"   chars     : {flags(ch, CH)}   dllchars: {flags(dch, DCH)}")
    print(f"   sections  : {' '.join(secs)}")
    if any(s.startswith('UPX') for s in secs):
        print("   NOTE      : UPX-packed")
    m = re.search(rb'[A-Za-z]:\\[ -~]{1,240}?\.pdb', d)
    if m:
        print(f"   pdb path  : {m.group(0).decode('latin-1')}")
    if os.path.basename(path).lower() == 'cataclysm.exe':
        ok = (base == 0x400000) and not (dch & 0x0040)
        print(f"   CET check : image base 0x{base:x} {'== 0x400000' if base==0x400000 else '!= 0x400000 (CET will refuse: dllmain.cpp CheckModuleLoadAddress)'}; "
              f"DYNAMIC_BASE {'set (ASLR could relocate it)' if dch & 0x0040 else 'not set'} -> {'OK for CET' if ok else 'CHECK'}")
PY
}

exe_report() {
  [[ -d "$GAME_ROOT" ]] || die "$GAME_ROOT does not exist (extract first)"
  local exes=()
  while IFS= read -r -d '' f; do exes+=("$f"); done < <(find -H "$GAME_ROOT" -type f -iname '*.exe' -print0 | sort -z)
  log "executables under original/game/: ${#exes[@]}"
  local f
  for f in "${exes[@]}"; do
    printf '\n%s\n' "--- ${f#"$PROJECT_ROOT"/}"
    printf '   file      : %s\n' "$(file -b -- "$f")"
    pe_report "$f" | sed '1d'
  done
  local dlls=()
  while IFS= read -r -d '' f; do dlls+=("$f"); done < <(find -H "$GAME_ROOT" -type f -iname '*.dll' -print0 | sort -z)
  if [[ ${#dlls[@]} -gt 0 ]]; then
    printf '\nDLLs (%d):\n' "${#dlls[@]}"
    for f in "${dlls[@]}"; do printf '   %-60s %10s bytes  %s\n' "${f#"$GAME_ROOT"/}" "$(stat -c %s -- "$f")" "$(file -b -- "$f" | cut -c1-60)"; done
  fi
  local main
  main="$(find -H "$GAME_ROOT" -maxdepth 4 -type f -iname 'cataclysm.exe' -print -quit)"
  if [[ -n "$main" ]]; then
    log "game directory (contains cataclysm.exe): ${main%/*}"
  else
    log "WARNING: no cataclysm.exe found - CET's loader (loader/Main.cpp) requires that exact name in its working directory"
  fi
}

# --- manifest -----------------------------------------------------------------
# Same layout as the first extraction made on 2026-10-08 (docs/binary-fingerprint.md):
#   MANIFEST.sha256 : sha256sum format "<sha256>  ./<path>"  (sha256sum -c compatible)
#   MANIFEST.sizes  : "<bytes>  ./<path>"
# Sorted with 'sort -z' in the caller's locale; MANIFEST.* and EXTRACT_INFO.txt excluded.
manifest_files() {  # NUL-separated relative paths, run inside $GAME_ROOT
  find . -type f ! -name 'MANIFEST.sha256' ! -name 'MANIFEST.sizes' ! -name 'EXTRACT_INFO.txt' \
       ! -path './.cet-backup/*' -print0 | sort -z
}

write_manifest() {
  local t1 t2
  t1="$(mktemp)"; t2="$(mktemp)"   # temp files outside GAME_ROOT so find cannot list them
  ( cd "$GAME_ROOT"
    manifest_files | xargs -0 -r sha256sum -- > "$t1"
    manifest_files | xargs -0 -r stat -c '%s  %n' -- > "$t2" )
  mv -f -- "$t1" "$GAME_ROOT/MANIFEST.sha256"
  mv -f -- "$t2" "$GAME_ROOT/MANIFEST.sizes"
  chmod 644 "$GAME_ROOT/MANIFEST.sha256" "$GAME_ROOT/MANIFEST.sizes"
  log "MANIFEST.sha256: $(grep -c . "$GAME_ROOT/MANIFEST.sha256") files, $(awk '{s+=$1} END{print s}' "$GAME_ROOT/MANIFEST.sizes") bytes"
}

verify_manifest() {
  [[ -f "$GAME_ROOT/MANIFEST.sha256" ]] || die "no $GAME_ROOT/MANIFEST.sha256"
  local rc=0
  log "sha256sum -c MANIFEST.sha256 (only failures are printed)"
  ( cd "$GAME_ROOT" && sha256sum -c --quiet MANIFEST.sha256 ) || rc=1
  if [[ -f "$GAME_ROOT/MANIFEST.sizes" ]]; then
    ( cd "$GAME_ROOT"
      while IFS= read -r line; do
        sz="${line%%  *}"; f="${line#*  }"
        [[ "$f" == ./MANIFEST.* ]] && continue
        if [[ -f "$f" ]]; then
          [[ "$(stat -c %s -- "$f")" == "$sz" ]] || echo "SIZE CHANGED: $f"
        fi
      done < MANIFEST.sizes )
  fi
  log "files present but not in the manifest (CET install / __support/app overlay / game-written files):"
  ( cd "$GAME_ROOT"
    comm -13 <(awk '{sub(/^[0-9a-f]+  /,""); print}' MANIFEST.sha256 | sort) \
             <(manifest_files | tr '\0' '\n' | sort) | sed 's/^/    EXTRA /' )
  if [[ $rc -eq 0 ]]; then log "verify: all manifest files present and unchanged"; else log "verify: MISMATCHES above"; fi
  return $rc
}

# GOG installers keep files the Inno script copies into the install root under
# __support/app/ (here: Cataclysm.cfg = "language=0"). innoextract does not run the
# install script, so mirror that copy (only for files that are absent). INFERRED
# from the GOG __support layout, not from the Inno script itself.
apply_support_app() {
  local d="$GAME_ROOT/__support/app" f rel
  [[ -d "$d" ]] || return 0
  while IFS= read -r -d '' f; do
    rel="${f#"$d"/}"
    if [[ -e "$GAME_ROOT/$rel" ]]; then
      log "__support/app/$rel: already present in game root, left alone"
    else
      run mkdir -p "$(dirname -- "$GAME_ROOT/$rel")"
      run cp -p -- "$f" "$GAME_ROOT/$rel"
      log "__support/app/$rel -> $rel (GOG post-install copy; not in MANIFEST)"
    fi
  done < <(find "$d" -type f -print0)
}

# --- main ----------------------------------------------------------------------
case "$MODE" in
  verify)
    if verify_manifest; then exit 0; else exit 1; fi ;;
  report)
    exe_report; exit 0 ;;
esac

build_innoextract() {
  local src="$IE_BUILD_ROOT/src" bld="$IE_BUILD_ROOT/build"
  for t in git cmake g++; do command -v "$t" >/dev/null 2>&1 || die "$t missing (needed for --build-innoextract)"; done
  run mkdir -p "$IE_BUILD_ROOT"
  if [[ -d "$src/.git" ]]; then run git -C "$src" pull --ff-only -q; else run git clone -q --depth 1 https://github.com/dscharrer/innoextract.git "$src"; fi
  run cmake -S "$src" -B "$bld" -DCMAKE_BUILD_TYPE=Release
  run cmake --build "$bld" -j "$(nproc)"
  [[ $DRY_RUN -eq 1 ]] || log "built: $("$bld/innoextract" --version | head -n1)"
}
if [[ $BUILD_IE -eq 1 ]]; then
  build_innoextract
  log "innoextract git build ready; rerun without --build-innoextract to extract (it is picked up automatically)"
  exit 0
fi
if [[ -z "$INNOEXTRACT" ]]; then
  if [[ -x "$IE_BUILD_ROOT/build/innoextract" ]]; then INNOEXTRACT="$IE_BUILD_ROOT/build/innoextract"
  elif command -v innoextract >/dev/null 2>&1; then INNOEXTRACT="$(command -v innoextract)"
  else die "innoextract not installed (sudo apt-get install innoextract, or --build-innoextract)"; fi
fi
[[ -x "$INNOEXTRACT" ]] || die "innoextract not executable: $INNOEXTRACT"
log "innoextract: $INNOEXTRACT ($("$INNOEXTRACT" --version 2>/dev/null | head -n1))"
innoextract() { "$INNOEXTRACT" "$@"; }

# Pre-flight parse: innoextract prints "Warning: Unexpected setup data version" (or loader
# revision / checksum warnings) for Inno Setup versions it does not know.
preflight_parse() {
  local out
  out="$(innoextract --info -- "$INSTALLER" 2>&1 | head -n 40 || true)"
  printf '%s\n' "$out" | sed -n '1,6p' | sed 's/^/    /'
  if grep -qE 'Unexpected setup (data version|loader revision)|checksum mismatch|Not a supported Inno Setup' <<<"$out"; then
    log "WARNING: $INNOEXTRACT does not fully understand this installer's Inno Setup version."
    log "  -> tools/extract_installer.sh --build-innoextract   (git master: Inno 6.3/6.4.0)"
    log "  -> or the Wine fallback: tools/extract_installer.sh --wine-install"
    return 1
  fi
  return 0
}

find_installer
describe_installer
PARSE_OK=1
preflight_parse || PARSE_OK=0

case "$MODE" in
  list)
    run innoextract --list-sizes --gog -- "$INSTALLER"; exit 0 ;;
  info)
    run innoextract --info -- "$INSTALLER" || true
    run innoextract --gog-game-id -- "$INSTALLER" || true
    exit 0 ;;
esac

if [[ -d "$GAME_ROOT" ]] && [[ -n "$(ls -A -- "$GAME_ROOT" 2>/dev/null)" ]]; then
  if [[ $FORCE -eq 1 ]]; then
    run mv -- "$GAME_ROOT" "$GAME_ROOT.prev-$TS"
  elif [[ $DRY_RUN -eq 1 ]]; then
    log "dry run: $GAME_ROOT is not empty - a real run would stop here (use --force to move it aside)"
  else
    die "$GAME_ROOT is not empty. Use --force to move it aside to game.prev-$TS/, or --verify / --report to inspect it."
  fi
fi
run mkdir -p "$GAME_ROOT"

if [[ "$MODE" == wine-install ]]; then
  command -v umu-run >/dev/null 2>&1 || die "umu-run missing - run tools/setup_prefix.sh first (installs it)"
  [[ -f "$INSTALLER_PROTON/proton" ]] || die "installer Proton not found: $INSTALLER_PROTON (set HWC_INSTALLER_PROTON)"
  WIN_DIR="Z:$(printf '%s' "$GAME_ROOT" | sed 's#/#\\#g')"
  inno_args=(/SP- /NOICONS /NORESTART /LANG="$LANGUAGE" "/DIR=$WIN_DIR" "/LOG=$WIN_DIR\\inno_install.log")
  if [[ $SILENT -eq 1 ]]; then
    inno_args=(/VERYSILENT /SUPPRESSMSGBOXES "${inno_args[@]}")
    log "fallback: SILENT install (known to abort at GOG's Welcome page - see inno_install.log)"
  else
    log "fallback: GOG installer GUI opens now - click through it; the install folder is preset to $WIN_DIR"
  fi
  cd "$(dirname -- "$INSTALLER")"
  set +e
  run env WINEPREFIX="$INSTALLER_PREFIX" GAMEID="$GAMEID" STORE=gog PROTONPATH="$INSTALLER_PROTON" \
      umu-run "$INSTALLER" "${inno_args[@]}"
  wrc=$?
  set -e
  [[ $DRY_RUN -eq 1 ]] && { log "dry run: stopping"; exit 0; }
  if [[ $wrc -ne 0 || -z "$(find -H "$GAME_ROOT" -maxdepth 2 -iname 'cataclysm.exe' -print -quit)" ]]; then
    log "installer exit code $wrc (Inno: 1 init failed, 2/5 cancelled/aborted, 3/4 fatal error)"
    [[ -f "$GAME_ROOT/inno_install.log" ]] && tail -n 8 "$GAME_ROOT/inno_install.log" | sed 's/^/    /'
    die "Wine install did not produce cataclysm.exe"
  fi
else
  if [[ $PARSE_OK -eq 0 && $FORCE -eq 0 ]]; then
    die "refusing to extract with an innoextract that warned about the format (see above); add --force to try anyway"
  fi
  # --gog: GOG extra archives / split .bin parts; --exclude-temp: skip Inno {tmp} helpers
  # (installer skin, EULAs); --language: pins language-tagged entries should a future build
  # have any (build 12948 has none: identical lists for all six languages, checked 2026-10-08).
  if ! run innoextract --gog --exclude-temp --language "$LANGUAGE" --output-dir "$GAME_ROOT" -- "$INSTALLER"; then
    log "innoextract FAILED. If it says 'Not a supported Inno Setup installer' / unknown version:"
    log "  1) tools/extract_installer.sh --build-innoextract --force   (innoextract git master)"
    log "  2) or: tools/setup_prefix.sh && tools/extract_installer.sh --force --wine-install"
    exit 1
  fi
fi

[[ $DRY_RUN -eq 1 ]] && { log "dry run: stopping before manifest/report"; exit 0; }

# innoextract maps Inno's {app} to app/. The rest of the project (docs, CET, run_game.sh)
# expects cataclysm.exe directly in original/game/, so flatten app/ one level.
if [[ -d "$GAME_ROOT/app" && ! -e "$GAME_ROOT/cataclysm.exe" ]]; then
  log "flattening $GAME_ROOT/app/ into $GAME_ROOT/ (innoextract maps {app} -> app/)"
  shopt -s dotglob
  mv -n -- "$GAME_ROOT/app/"* "$GAME_ROOT/"
  shopt -u dotglob
  rmdir -- "$GAME_ROOT/app" || log "WARNING: $GAME_ROOT/app not empty after flatten (name clash?)"
fi
other_top="$(find -H "$GAME_ROOT" -mindepth 1 -maxdepth 1 -type d ! -name __support ! -name 3Dfx ! -name credits ! -name feman ! -name Movies ! -name MultiPlayer ! -name SavedGames ! -name ScreenShots -printf '%f ' || true)"
[[ -z "$other_top" ]] || log "note: other top-level dirs from the installer: $other_top"

INST_SHA="$(sha256sum -- "$INSTALLER" | cut -d' ' -f1)"
{
  echo "installer: $(basename -- "$INSTALLER")"
  echo "installer_size: $(stat -c %s -- "$INSTALLER")"
  echo "installer_sha256: $INST_SHA"
  echo "extracted: $(date -Iseconds)  mode: $MODE  language: $LANGUAGE"
  echo "innoextract: $(innoextract --version 2>/dev/null | head -n1)"
  echo "gog_game_id: $(innoextract --gog-game-id -- "$INSTALLER" 2>/dev/null | grep -oE '[0-9]{6,}' | head -n1 || true)"
  echo "--- innoextract --info"
  innoextract --info -- "$INSTALLER" 2>&1 || true
} > "$GAME_ROOT/EXTRACT_INFO.txt"
log "wrote $GAME_ROOT/EXTRACT_INFO.txt"

write_manifest
log "wrote $GAME_ROOT/MANIFEST.sha256 and MANIFEST.sizes (pristine extraction)"
apply_support_app
log "top level of original/game/:"; ls -la "$GAME_ROOT"
exe_report
log "next: tools/setup_prefix.sh (if not done), tools/install_cet.sh, tools/run_game.sh"
log "log: $LOG"
