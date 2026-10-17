#!/usr/bin/env bash
# record_reference.sh - the user's side of the checksum oracle, as one command (launched from the desktop shortcuts
# "Cataclysm Oracle: Record / Verify" or by hand).
#
#   record   launch the GOG game (no gamescope, Wine virtual desktop) through hwi_oracle_loader.exe with the oracle
#            DLL in record mode; the user starts a skirmish from the menus and plays; the DLL records the game as
#            SavedGames\RecordedGames\<NAME> and writes the first checksum stream. On exit the outputs are copied to
#            original/oracle/<NAME>-<timestamp>/ (gitignored) and summarised.
#   verify   replay the recording twice through the oracle (virtual clock, no input) and compare the two streams.
#
# Env: ORACLE_NAME (default oracle01), ORACLE_STOP ticks (default 9600 = 10 min at 16 Hz), ORACLE_DESKTOP (1920x1080).
set -uo pipefail
ROOT="$(cd "$(dirname "$(readlink -f "$0")")/../.." && pwd)"
GAME="$ROOT/original/game"
NAME="${ORACLE_NAME:-oracle01}"; STOP="${ORACLE_STOP:-9600}"; DESK="${ORACLE_DESKTOP:-1920x1080}"
OUTROOT="$ROOT/original/oracle"
mode="${1:-record}"

pause() { if [[ -t 0 ]]; then echo; read -r -p "Press Enter to close this window" _; fi; }
die() { echo "ERROR: $*" >&2; pause; exit 1; }

# newest built oracle DLL + loader (main checkout or the orchestrator's worktree)
newest_build() {
  ls -t -d "$ROOT"/build/oracle-dll "$ROOT"/.claude/worktrees/*/build/oracle-dll 2>/dev/null | head -1
}
install_dll() {
  local b; b="$(newest_build)"
  [[ -n "$b" && -f "$b/hwi_oracle.dll" && -f "$b/hwi_oracle_loader.exe" ]] || die "no built oracle DLL found (expected build/oracle-dll/hwi_oracle.dll); run tools/oracle/dll/build.sh"
  cp -u "$b/hwi_oracle.dll" "$b/hwi_oracle_loader.exe" "$GAME/" || die "could not copy the DLL into $GAME"
  echo "oracle DLL: $(sha256sum "$GAME/hwi_oracle.dll" | cut -c1-12)  loader: $(stat -c %s "$GAME/hwi_oracle_loader.exe") B  (from $b)"
}
launch() {  # $1 = HWI_ORACLE value
  echo "HWI_ORACLE=$1"
  # +seh: Wine logs every exception with its address, so a crash leaves a trace in logs/steam-*.log
  # (2026-10-17: the first recording session crashed with an access violation and left no address).
  HWI_ORACLE="$1" WINEDEBUG="${WINEDEBUG:-+seh}" "$ROOT/tools/run_game.sh" --no-gamescope --virtual-desktop "$DESK" --wine-log --exe hwi_oracle_loader.exe -- /logOn
  local rc=$?
  local L; L="$(ls -t "$ROOT"/logs/steam-*.log 2>/dev/null | head -1)"
  if [[ -n "$L" ]]; then
    cp "$L" "$OUTROOT/last-wine-$(date +%Y%m%d-%H%M%S).log" 2>/dev/null
    grep -E 'Unhandled exception|code=c0000005|code=80000003' "$L" | grep -vE '40010006|406d1388' | tail -3 | sed 's/^/  wine: /'
  fi
  return $rc
}
collect() {  # $1 = label
  local ts dest; ts="$(date +%Y%m%d-%H%M%S)"; dest="$OUTROOT/$NAME-$1-$ts"; mkdir -p "$dest"
  cp -v "$GAME/SavedGames/RecordedGames/$NAME"* "$dest/" 2>/dev/null
  cp -v "$GAME/$NAME"-*.hwo "$GAME/$NAME"-*.hwo.log "$GAME"/hwi_oracle.hwo "$GAME"/hwi_oracle.hwo.log "$dest/" 2>/dev/null
  cp -v "$GAME/hwi_oracle_loader.log" "$dest/" 2>/dev/null
  echo "collected into $dest"; ls -la "$dest" | awk 'NR>1{print "   "$5, $9}'
  echo "$dest"
}

[[ -d "$GAME" && -f "$GAME/cataclysm.exe" ]] || die "game not found at $GAME"
mkdir -p "$OUTROOT"
case "$mode" in
  record)
    cat <<MSG
== Cataclysm Oracle: RECORD a reference game ==
The GOG game opens in a $DESK window with the checksum oracle attached (this is the real game: sound is on).
 1. Start a SKIRMISH from the menus: a small map, one AI opponent, default settings.
 2. Play normally for about 10 minutes (build, harvest, move, attack). Save the game once if you like.
 3. The oracle stops recording by itself at tick $STOP; then quit the game normally.
Recording name: $NAME
MSG
    install_dll
    launch "mode=record:$NAME;stop=$STOP;out=$NAME-rec.hwo"; rc=$?
    echo "game exited with code $rc"
    dest="$(collect rec | tail -1)"
    if [[ -f "$GAME/SavedGames/RecordedGames/$NAME" ]]; then
      echo "RECORDING OK: $NAME ($(stat -c %s "$GAME/SavedGames/RecordedGames/$NAME") B). Next: run 'Cataclysm Oracle: Verify'."
    else
      echo "No recording file was written. Check $dest/*.log (the DLL logs why: identity check, hook sites, mode)."
    fi
    pause ;;
  verify)
    [[ -f "$GAME/SavedGames/RecordedGames/$NAME" ]] || die "no recording named $NAME; run the Record shortcut first"
    install_dll
    for r in a b; do
      echo "== replay $r of 2 (virtual clock, no input; just watch, then it exits by itself) =="
      launch "mode=play:$NAME;clock=virtual;rng0=split;out=$NAME-play-$r.hwo"; echo "exit code $?"
    done
    dest="$(collect play | tail -1)"
    echo "== compare the two replays =="
    python3 "$ROOT/tools/oracle/hwo_compare.py" "$GAME/$NAME-play-a.hwo" "$GAME/$NAME-play-b.hwo" | tee "$dest/compare.txt"
    echo "Result and streams are in $dest. If the two replays are IDENTICAL, the recording is a valid reference; tell Claude."
    pause ;;
  *) die "usage: $0 record|verify" ;;
esac
