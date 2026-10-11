#!/usr/bin/env bash
# mission_test.sh - the checksum oracle's stage-2 acceptance test: two headless Mission01 runs (M2-R4 rest-oracle,
# docs/m2-r4-rest-oracle.md "oracle"; the stage-2 exit test of docs/m2-r3-summary.md).
#
# Usage: tools/oracle/mission_test.sh [--bin PATH] [--label PREFIX] [--ticks N] [--timeout S] [--perturb T]
#                                      [--copy-tick T] [--copy-field F] [--no-perturb-run]
#   --bin PATH       engine binary (default build/debug/hwi-engine: the exit test is judged with ASan on)
#   --label PREFIX   harness run label prefix (default oracle-mission)
#   --ticks N        universe ticks per run (default 3000)
#   --timeout S      harness timeout per run in seconds (default 900; an ASan run does about 10 ticks/s)
#   --perturb T      tick before which run P draws one extra gamerand() (default 1500; 0 = skip run P)
#   --copy-tick T    tick of the 'T' record changed in the perturbed copy (default 2000)
#   --copy-field F   field changed in the perturbed copy (default ships_health)
#
# Steps, all through tools/m2_harness.sh --mode mission (headless, silent, no input, the unmodified-data harness):
#   1. run A and run B of Mission01 for N ticks; their .hwo streams must be byte-identical (cmp) and
#      tools/oracle/hwo_compare.py must say IDENTICAL (exit 0); both must report ASan 0;
#   2. run P, the same run with /oracleSelfTestPerturb T; the tool must report first divergent tick T, likely origin
#      rng10 (exit 1);
#   3. a copy of A with one bit of FIELD flipped in the 'T' record of tick COPYTICK; the tool must report that tick and
#      that field (exit 1).
# Prints PASS or FAIL per step and exits non-zero if any step fails. Output: the harness run dirs (runs/<label>-*).
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
if [[ -d "$root/original/game" ]]; then main="$root"; else main="$(cd "$root/../../.." && pwd)"; fi
HARNESS="${M2_HARNESS:-$main/original/scratch-m2/m2r2-harness}"
bin="$root/build/debug/hwi-engine"; label=oracle-mission; ticks=3000; tmo=900; perturb=1500; ctick=2000; cfield=ships_health
while [[ $# -gt 0 ]]; do
	case "$1" in
		--bin) bin=$(realpath -- "$2"); shift ;;
		--label) label=$2; shift ;;
		--ticks) ticks=$2; shift ;;
		--timeout) tmo=$2; shift ;;
		--perturb) perturb=$2; shift ;;
		--copy-tick) ctick=$2; shift ;;
		--copy-field) cfield=$2; shift ;;
		--no-perturb-run) perturb=0 ;;
		-h|--help) sed -n '2,22p' "$0"; exit 0 ;;
		*) echo "mission_test.sh: unknown option $1" >&2; exit 2 ;;
	esac
	shift
done

harness="$root/tools/m2_harness.sh"
compare="$root/tools/oracle/hwo_compare.py"
runs="$HARNESS/runs"
fail=0
step() { echo "== $*"; }
verdict() { if [[ $1 -eq 0 ]]; then echo "PASS: $2"; else echo "FAIL: $2"; fail=1; fi; }
asan_of() { grep -h "^file loads" "$runs/$1/summary.txt" | sed 's/^.*ASan errors: \([0-9]*\).*$/\1/'; }

for r in a b; do
	"$harness" "$label-$r" --mode mission --mission-ticks "$ticks" --timeout "$tmo" --bin "$bin" > /dev/null 2>&1
	grep -h "^label\|^milestone\|^first stop\|^file loads\|^oracle:\|^mission:" "$runs/$label-$r/summary.txt"
done
A="$runs/$label-a/run.hwo"; B="$runs/$label-b/run.hwo"; X="$runs/$label-a/run.perturbed-copy.hwo"

step "1. run A vs run B (Mission01, $ticks ticks)"
for r in a b; do
	grep -q "^milestone: oracle mission run done" "$runs/$label-$r/summary.txt"; c=$?
	verdict $c "run $r reached the requested tick (milestone 'oracle mission run done')"
	[[ "$(asan_of "$label-$r")" == "0" ]]; c=$?
	verdict $c "run $r: ASan errors 0"
done
cmp "$A" "$B"; c=$?
verdict $c "byte-identical streams ($(stat -c %s "$A" 2>/dev/null) bytes, sha256 $(sha256sum "$A" 2>/dev/null | cut -c1-16))"
python3 "$compare" "$A" "$B" --json "$runs/$label-b/compare.json"; c=$?
verdict $c "hwo_compare.py: identical (exit $c)"

if [[ "$perturb" -gt 0 ]]; then
	step "2. run A vs run P (one extra gamerand() before tick $perturb)"
	"$harness" "$label-perturb" --mode mission --mission-ticks "$ticks" --perturb "$perturb" --timeout "$tmo" --bin "$bin" > /dev/null 2>&1
	grep -h "^milestone\|^first stop\|^file loads\|^oracle:" "$runs/$label-perturb/summary.txt"
	P="$runs/$label-perturb/run.hwo"
	python3 "$compare" "$A" "$P" --json "$runs/$label-perturb/compare.json"; c=$?
	python3 - "$runs/$label-perturb/compare.json" "$perturb" <<'PYEOF'
import json, sys
o = json.load(open(sys.argv[1])); t = int(sys.argv[2])
ok = o["first_divergent_tick"] == t and o["likely_origin"] == "rng10"
print("tool says: tick %s, origin %s, draws %s -> %s" % (o["first_divergent_tick"], o["likely_origin"], o.get("rng_draws"), "expected" if ok else "NOT expected"))
sys.exit(0 if ok else 1)
PYEOF
	c2=$?
	verdict $(( (c == 1 && c2 == 0) ? 0 : 1 )) "perturbed run reported at tick $perturb, origin rng10 (exit $c)"
fi

step "3. run A vs a copy of A with '$cfield' changed at tick $ctick"
python3 "$compare" --perturb-copy "$A" "$X" --tick "$ctick" --field "$cfield"
python3 "$compare" "$A" "$X" --json "$runs/$label-a/compare-copy.json"; c=$?
python3 - "$runs/$label-a/compare-copy.json" "$ctick" "$cfield" <<'PYEOF'
import json, sys
o = json.load(open(sys.argv[1])); t = int(sys.argv[2]); f = sys.argv[3]
names = {"missiles_n": "missiles", "missiles_h": "missiles", "bullets_n": "bullets", "bullets_h": "bullets",
         "derelicts_n": "derelicts", "derelicts_h": "derelicts", "resources_n": "resources", "resources_h": "resources",
         "player_ru": "players", "player_ships": "players"}
want = names.get(f, f)
ok = o["first_divergent_tick"] == t and o["likely_origin"] == want
print("tool says: tick %s, origin %s -> %s" % (o["first_divergent_tick"], o["likely_origin"], "expected" if ok else "NOT expected"))
sys.exit(0 if ok else 1)
PYEOF
c2=$?
verdict $(( (c == 1 && c2 == 0) ? 0 : 1 )) "flipped copy reported at tick $ctick, field $cfield (exit $c)"

if [[ $fail -eq 0 ]]; then echo "MISSION TEST PASS"; else echo "MISSION TEST FAIL"; fi
exit $fail
