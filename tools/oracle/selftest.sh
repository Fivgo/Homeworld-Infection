#!/usr/bin/env bash
# selftest.sh - the checksum oracle's recreation-side acceptance test (docs/m2-r2-oracle.md "runs").
#
# Usage: tools/oracle/selftest.sh [--bin PATH] [--label PREFIX] [--ticks N] [--perturb T] [--copy-tick T] [--copy-field F]
#   --bin PATH       engine binary (default build/release/hwi-engine, the -O2 build oracle runs use)
#   --label PREFIX   harness run label prefix (default oracle-st)
#   --ticks N        universe ticks per run (default 512)
#   --perturb T      tick before which the perturbed run draws one extra gamerand() (default 300)
#   --copy-tick T    tick of the 'T' record changed in the perturbed copy (default 400)
#   --copy-field F   field changed in the perturbed copy (default ships_pos)
#
# Steps, all through tools/m2_harness.sh --mode oracle (headless, no input, the unmodified-data harness):
#   1. run A and run B of the same scripted start; their .hwo streams must be byte-identical (cmp) and
#      tools/oracle/hwo_compare.py must say IDENTICAL (exit 0);
#   2. run P, the same start with /oracleSelfTestPerturb T; the tool must report first divergent tick T, likely origin
#      rng10, "1 draw(s) ahead" (exit 1);
#   3. a copy of A with one bit of FIELD flipped in the 'T' record of tick COPYTICK; the tool must report that tick and
#      that field (exit 1).
# Prints PASS or FAIL per step and exits non-zero if any step fails. Output: the harness run dirs (runs/<label>-*).
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
if [[ -d "$root/original/game" ]]; then main="$root"; else main="$(cd "$root/../../.." && pwd)"; fi
HARNESS="${M2_HARNESS:-$main/original/scratch-m2/m2r2-harness}"
bin="$root/build/release/hwi-engine"; label=oracle-st; ticks=512; perturb=300; ctick=400; cfield=ships_pos
while [[ $# -gt 0 ]]; do
	case "$1" in
		--bin) bin=$(realpath -- "$2"); shift ;;
		--label) label=$2; shift ;;
		--ticks) ticks=$2; shift ;;
		--perturb) perturb=$2; shift ;;
		--copy-tick) ctick=$2; shift ;;
		--copy-field) cfield=$2; shift ;;
		-h|--help) sed -n '2,20p' "$0"; exit 0 ;;
		*) echo "selftest.sh: unknown option $1" >&2; exit 2 ;;
	esac
	shift
done

harness="$root/tools/m2_harness.sh"
compare="$root/tools/oracle/hwo_compare.py"
runs="$HARNESS/runs"
fail=0
step() { echo "== $*"; }
verdict() { if [[ $1 -eq 0 ]]; then echo "PASS: $2"; else echo "FAIL: $2"; fail=1; fi; }

for r in a b; do
	"$harness" "$label-$r" --mode oracle --oracle-ticks "$ticks" --timeout 120 --bin "$bin" > /dev/null 2>&1
	grep -h "^milestone\|^first stop\|^oracle:" "$runs/$label-$r/summary.txt"
done
"$harness" "$label-perturb" --mode oracle --oracle-ticks "$ticks" --perturb "$perturb" --timeout 120 --bin "$bin" > /dev/null 2>&1
grep -h "^milestone\|^first stop\|^oracle:" "$runs/$label-perturb/summary.txt"
A="$runs/$label-a/run.hwo"; B="$runs/$label-b/run.hwo"; P="$runs/$label-perturb/run.hwo"; X="$runs/$label-a/run.perturbed-copy.hwo"

step "1. run A vs run B (same scripted start)"
cmp "$A" "$B"; c=$?
verdict $c "byte-identical streams ($(stat -c %s "$A") bytes, sha256 $(sha256sum "$A" | cut -c1-16))"
python3 "$compare" "$A" "$B" --json "$runs/$label-b/compare.json"; c=$?
verdict $c "hwo_compare.py: identical (exit $c)"

step "2. run A vs run P (one extra gamerand() before tick $perturb)"
python3 "$compare" "$A" "$P" --json "$runs/$label-perturb/compare.json"; c=$?
python3 - "$runs/$label-perturb/compare.json" "$perturb" <<'EOF'
import json, sys
o = json.load(open(sys.argv[1])); t = int(sys.argv[2])
ok = o["first_divergent_tick"] == t and o["likely_origin"] == "rng10" and o["rng_draws"] == 1
print("tool says: tick %s, origin %s, draws %s -> %s" % (o["first_divergent_tick"], o["likely_origin"], o["rng_draws"], "expected" if ok else "NOT expected"))
sys.exit(0 if ok else 1)
EOF
c2=$?
verdict $(( (c == 1 && c2 == 0) ? 0 : 1 )) "perturbed run reported at tick $perturb, rng10, 1 draw ahead (exit $c)"

step "3. run A vs a copy of A with '$cfield' changed at tick $ctick"
python3 "$compare" --perturb-copy "$A" "$X" --tick "$ctick" --field "$cfield"
python3 "$compare" "$A" "$X" --json "$runs/$label-a/compare-copy.json"; c=$?
python3 - "$runs/$label-a/compare-copy.json" "$ctick" "$cfield" <<'EOF'
import json, sys
o = json.load(open(sys.argv[1])); t = int(sys.argv[2]); f = sys.argv[3]
names = {"missiles_n": "missiles", "missiles_h": "missiles", "bullets_n": "bullets", "bullets_h": "bullets",
         "derelicts_n": "derelicts", "derelicts_h": "derelicts", "resources_n": "resources", "resources_h": "resources",
         "nships": "univcheck"}
want = names.get(f, f)
ok = o["first_divergent_tick"] == t and o["likely_origin"] == want
print("tool says: tick %s, origin %s -> %s" % (o["first_divergent_tick"], o["likely_origin"], "expected" if ok else "NOT expected"))
sys.exit(0 if ok else 1)
EOF
c2=$?
verdict $(( (c == 1 && c2 == 0) ? 0 : 1 )) "perturbed copy reported at tick $ctick, field $cfield (exit $c)"

[[ $fail -eq 0 ]] && echo "SELFTEST PASS" || echo "SELFTEST FAIL"
exit $fail
