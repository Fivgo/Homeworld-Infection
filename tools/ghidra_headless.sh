#!/usr/bin/env bash
# Headless Ghidra pipeline for the Homeworld: Cataclysm (Emergence) executable and its DLLs.
#
#   tools/ghidra_headless.sh [options] <pe-file> [project-name]
#
# What it does (see docs/ghidra-pipeline.md):
#   1. ensures a PyGhidra venv exists (offline install from the wheels bundled with Ghidra)
#   2. creates/reuses the Ghidra project  ghidra/projects/<project-name>.gpr
#   3. first run:  imports <pe-file> as x86:LE:32:default:windows, runs
#        preScripts  ConfigureAnalysis.py, ImportSymbolsJson.py phase=pre
#        auto-analysis
#        postScripts ImportSymbolsJson.py phase=post, ExportAnalysis.py
#      later runs (program already in the project): -process, no re-analysis unless --reanalyze,
#        re-applies symbols (phase=all) and re-exports
#   4. exports to ghidra/exports/<export-name>/   (export-name defaults to the PE basename)
#
# Options:
#   --reimport        delete and re-import the program (fresh analysis)
#   --reanalyze       re-run auto-analysis on the existing program before exporting
#   --no-decompile    skip decompiled.c (fast)
#   --threads N       decompiler worker threads (default 16)
#   --timeout S       per-function decompile timeout (default 60)
#   --max-cpu N       analysis CPU cap (default: all)
#   --symbols FILE    CET symbol JSON (default ghidra/cet_symbols.json; missing file is fine)
#   --export-name X   export directory name (default: PE basename)
#   --profile P       ConfigureAnalysis profile: msvc6 (default) | fast | default
#   --noexec-sections L  comma list of memory blocks to quarantine before analysis (import only;
#                     e.g. .cms_t on cataclysm.exe): execute flag cleared + restrict disassembly
#   --hw1-match       also run MatchHW1Strings.py (propose names from HW1 source strings)
#   --hw1-apply       ... and rename FUN_* functions with the proposals (ANALYSIS source)
#   --hw1-validate    ... treat existing names as ground truth (for symbolised HW1 builds)
#   --hw1-src DIRS    comma-separated HW1 source dirs (default reference/HomeworldSDL/src)
#
# Env: GHIDRA_HOME (default ~/Applications/ghidra_12.1.4_PUBLIC), PYGHIDRA_VENV, GHIDRA_MAXMEM (8G),
#      GHIDRA_PROJECTS_DIR / GHIDRA_EXPORTS_DIR (override ghidra/projects, ghidra/exports; e.g. for
#      throw-away stand-in runs under /tmp), CET_SYMBOLS (same as --symbols)
set -euo pipefail

ROOT="$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)"
GHIDRA_HOME="${GHIDRA_HOME:-$HOME/Applications/ghidra_12.1.4_PUBLIC}"
SCRIPTS="$ROOT/ghidra/scripts"
PROJECTS="${GHIDRA_PROJECTS_DIR:-$ROOT/ghidra/projects}"
EXPORTS="${GHIDRA_EXPORTS_DIR:-$ROOT/ghidra/exports}"

REIMPORT=0; REANALYZE=0; DECOMPILE=1; THREADS=16; TIMEOUT=60; MAXCPU=""; PROFILE=msvc6; NOEXEC=""
SYMBOLS="${CET_SYMBOLS:-$ROOT/ghidra/cet_symbols.json}"; EXPORT_NAME=""
HW1=0; HW1_APPLY=0; HW1_VALIDATE=0; HW1_SRC="$ROOT/reference/HomeworldSDL/src"
POS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --reimport) REIMPORT=1 ;;
    --reanalyze) REANALYZE=1 ;;
    --no-decompile) DECOMPILE=0 ;;
    --threads) THREADS="$2"; shift ;;
    --timeout) TIMEOUT="$2"; shift ;;
    --max-cpu) MAXCPU="$2"; shift ;;
    --symbols) SYMBOLS="$2"; shift ;;
    --export-name) EXPORT_NAME="$2"; shift ;;
    --profile) PROFILE="$2"; shift ;;
    --noexec-sections) NOEXEC="$2"; shift ;;
    --hw1-match) HW1=1 ;;
    --hw1-apply) HW1=1; HW1_APPLY=1 ;;
    --hw1-validate) HW1=1; HW1_VALIDATE=1 ;;
    --hw1-src) HW1_SRC="$2"; shift ;;
    -h|--help) sed -n '2,36p' "$0"; exit 0 ;;
    -*) echo "unknown option $1" >&2; exit 2 ;;
    *) POS+=("$1") ;;
  esac
  shift
done
[[ ${#POS[@]} -ge 1 ]] || { sed -n '2,36p' "$0"; exit 2; }
# A glob such as original/game/.cet-backup/*/rgl.dll that matches several files would otherwise
# silently turn the second match into the project name.
[[ ${#POS[@]} -le 2 ]] || { echo "too many arguments (did a glob match several files?): ${POS[*]}" >&2; exit 2; }

PE="$(readlink -f "${POS[0]}")"
[[ -f "$PE" ]] || { echo "no such file: ${POS[0]}" >&2; exit 2; }
PE_BASE="$(basename "$PE")"
PROJECT="${POS[1]:-${PE_BASE%.*}}"
PROJECT="${PROJECT//[^A-Za-z0-9_.-]/_}"
EXPORT_NAME="${EXPORT_NAME:-$PE_BASE}"
OUT="$EXPORTS/$EXPORT_NAME"
LOGDIR="$OUT/logs"
# Refuse to analyse Galaxy's download stub by mistake (it is not the game).
if [[ "$PE_BASE" == GOG_Galaxy_* ]]; then
  echo "refusing: $PE_BASE is the GOG Galaxy download stub, not the game executable" >&2
  exit 3
fi
mkdir -p "$PROJECTS" "$OUT" "$LOGDIR"

# ---- sanity: Ghidra + JDK (Ghidra needs a full JDK; a JRE makes LaunchSupport fail with
#      "Unable to prompt user for JDK path, no TTY detected")
[[ -x "$GHIDRA_HOME/support/analyzeHeadless" ]] || { echo "Ghidra not found at $GHIDRA_HOME" >&2; exit 4; }
command -v javac >/dev/null || { echo "javac missing: sudo apt-get install openjdk-21-jdk-headless" >&2; exit 4; }

# ---- PyGhidra venv (same path Ghidra's own pyghidraRun uses, so GUI + headless share it)
GVER="$(sed -n 's/^application.version=//p' "$GHIDRA_HOME/Ghidra/application.properties")"
PYGHIDRA_VENV="${PYGHIDRA_VENV:-${XDG_CONFIG_HOME:-$HOME/.config}/ghidra/ghidra_${GVER}_PUBLIC/venv}"
if ! "$PYGHIDRA_VENV/bin/python3" -c 'import pyghidra' 2>/dev/null; then
  echo "[pipeline] creating PyGhidra venv at $PYGHIDRA_VENV (offline, bundled wheels)"
  python3 -m venv "$PYGHIDRA_VENV"
  "$PYGHIDRA_VENV/bin/python3" -m pip install -q --no-index \
      -f "$GHIDRA_HOME/Ghidra/Features/PyGhidra/pypkg/dist" pyghidra
fi

# Heap: pyghidra.ghidra_launch forwards -X<arg>/-D<arg> to the JVM (argparse, any position).
JVM_HEAP="-Xmx${GHIDRA_MAXMEM:-8G}"

# PyGhidra would otherwise leave ghidra/scripts/__pycache__/*.pyc in the repo (gotcha G18)
export PYTHONDONTWRITEBYTECODE=1

run_headless() {
  # == support/pyghidraRun -H, minus its interactive "install PyGhidra (y/n)?" prompts
  "$PYGHIDRA_VENV/bin/python3" -m pyghidra.ghidra_launch --install-dir "$GHIDRA_HOME" \
      "$JVM_HEAP" ghidra.app.util.headless.AnalyzeHeadless "$@"
}

# ---- does the program already exist in the project?
EXISTS=0
if [[ -f "$PROJECTS/$PROJECT.gpr" ]] && \
   grep -qaF "$PE_BASE" "$PROJECTS/$PROJECT.rep/idata/~index.dat" 2>/dev/null; then
  EXISTS=1
fi

COMMON=(-scriptPath "$SCRIPTS" -log "$LOGDIR/application.log" -scriptlog "$LOGDIR/script.log")
[[ -n "$MAXCPU" ]] && COMMON+=(-max-cpu "$MAXCPU")
EXPORT_ARGS=()
[[ $HW1 -eq 1 ]] && EXPORT_ARGS+=(-postScript MatchHW1Strings.py "src=$HW1_SRC" "out=$OUT/hw1_string_matches.csv" "apply=$HW1_APPLY" "validate=$HW1_VALIDATE")
EXPORT_ARGS+=(-postScript ExportAnalysis.py "outdir=$OUT" "decompile=$DECOMPILE" "threads=$THREADS" "timeout=$TIMEOUT")

# PE32 -> force x86:LE:32:default:windows + PeLoader; anything else (e.g. an ELF build of HW1
# used as ground truth) -> let Ghidra pick loader and language
LANG_ARGS=()
if [[ "$(head -c2 "$PE")" == "MZ" ]]; then
  LANG_ARGS=(-processor x86:LE:32:default -cspec windows -loader PeLoader)
fi

START=$(date +%s)
set +e
if [[ $EXISTS -eq 0 || $REIMPORT -eq 1 ]]; then
  echo "[pipeline] import + analyse $PE -> project $PROJECT"
  run_headless "$PROJECTS" "$PROJECT" -import "$PE" -overwrite \
      "${LANG_ARGS[@]}" \
      -analysisTimeoutPerFile 7200 \
      "${COMMON[@]}" \
      -preScript ConfigureAnalysis.py "profile=$PROFILE" "noexec=$NOEXEC" \
      -preScript ImportSymbolsJson.py "json=$SYMBOLS" phase=pre \
      -postScript ImportSymbolsJson.py "json=$SYMBOLS" phase=post "report=$OUT/cet_apply_report.csv" \
      "${EXPORT_ARGS[@]}" 2>&1 | tee "$LOGDIR/headless.out"
  RC=${PIPESTATUS[0]}
else
  ANALYSIS=(-noanalysis)
  [[ $REANALYZE -eq 1 ]] && ANALYSIS=(-analysisTimeoutPerFile 7200)
  echo "[pipeline] reuse project $PROJECT program $PE_BASE (analysis: ${ANALYSIS[*]})"
  # ConfigureAnalysis.py runs only on import, so these options would otherwise be dropped silently
  if [[ -n "$NOEXEC" || "$PROFILE" != msvc6 ]]; then
    echo "[pipeline] WARNING: --noexec-sections/--profile apply only on import; ignored for the existing program (add --reimport)" >&2
  fi
  run_headless "$PROJECTS" "$PROJECT" -process "$PE_BASE" "${ANALYSIS[@]}" \
      "${COMMON[@]}" \
      -preScript ImportSymbolsJson.py "json=$SYMBOLS" phase=all "report=$OUT/cet_apply_report.csv" \
      "${EXPORT_ARGS[@]}" 2>&1 | tee "$LOGDIR/headless.out"
  RC=${PIPESTATUS[0]}
fi
set -e
END=$(date +%s)

# analyzeHeadless exits 0 even when a Python post-script raises (verified on 12.1.4: the log
# shows "ERROR <Script>.py> Traceback ..."), so surface script/import errors explicitly
if grep -qE "^ERROR .*\.py> |Traceback \(most recent|REPORT: Import failed|program file\(s\) not found|Unable to lock project" "$LOGDIR/headless.out"; then
  echo "[pipeline] WARNING: errors in $LOGDIR/headless.out" >&2
  [[ $RC -eq 0 ]] && RC=5
fi
echo "[pipeline] rc=$RC wall=$((END-START))s  exports: $OUT"
ls -la "$OUT" | sed 's/^/[pipeline]   /'
exit "$RC"
