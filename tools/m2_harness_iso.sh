#!/usr/bin/env bash
# m2_harness_iso.sh - build an ISOLATION data tree for tools/m2_harness.sh --data, for testing one M2-R2 item while
# other stage-1 items are still unported. NOT the gate: the gate is the plain harness (unmodified install).
#
# Usage: tools/m2_harness_iso.sh <name> <stage>[,<stage>...]
#   Creates $HARNESS/<name>/ as a symlink mirror of original/scratch-m2/hw1-tree (the data-deltas /ignoreBigfiles tree:
#   extracted cataclysm.big + update.big overlay + workaround stages 1-14 + the stage-15 mesh down-conversion, see
#   docs/m2-r1-data-deltas.md section 4), then UNDOES the listed workaround stages, so the files of those stages are
#   the original Cataclysm files again (or absent, where the stage only added a stub for a file Cataclysm does not
#   ship). Run the engine on it with:  tools/m2_harness.sh <label> --data $HARNESS/<name> -- /ignoreBigfiles [...]
# Stages that can be undone (numbers as in docs/m2-r1-data-deltas.md section 4):
#   3 aiship.script  4 tweak.script  5 dock.script  6 gatherstatsfor.script  7 crates.script
#   8 parade_{r1mothership,r2mothership,p2mothership,carrier}.script  9 speech LUT names  12 battlechatter.script
#   13 colpick.script  14 tactical.script  15 the mesh down-conversion (all .geo/.peo back to the 0x74 originals)
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -d "$root/original/game" ]]; then main="$root"; else main="$(cd "$root/../../.." && pwd)"; fi
S="$main/original/scratch-m2"
X="$main/original/game_extracted"
HARNESS="${M2_HARNESS:-$S/m2r2-harness}"
name=${1:?usage: m2_harness_iso.sh <name> <stages>}
stages=${2:?usage: m2_harness_iso.sh <name> <stages>}
T="$HARNESS/$name"
[[ -d "$S/hw1-tree" ]] || { echo "no $S/hw1-tree (data-deltas tools/setup_tree.sh mesh)" >&2; exit 1; }
rm -rf -- "$T"; mkdir -p "$T"
cp -as "$S/hw1-tree/." "$T/"
restore() { rm -f -- "$T/$1"; ln -s "$2" "$T/$1"; }
IFS=, read -r -a list <<< "$stages"
for s in "${list[@]}"; do
	case "$s" in
		3) restore aiship.script "$X/cataclysm/aiship.script" ;;
		4) restore tweak.script "$X/update/tweak.script" ;;
		5) rm -f -- "$T/dock.script" ;;
		6) restore gatherstatsfor.script "$X/cataclysm/gatherstatsfor.script" ;;
		7) restore crates.script "$X/cataclysm/crates.script" ;;
		8) rm -f -- "$T/parade_r1mothership.script" "$T/parade_r2mothership.script" "$T/parade_p2mothership.script" \
			"$T/parade_carrier.script" ;;
		9) rm -f -- "$T/soundfx/speechsentence_comp.lut" "$T/soundfx/speechphrase_comp.lut" ;;
		12) restore battlechatter.script "$X/cataclysm/battlechatter.script" ;;
		13) restore colpick.script "$X/cataclysm/colpick.script" ;;
		14) restore tactical.script "$X/cataclysm/tactical.script" ;;
		15) # the mesh down-conversion: every .geo/.peo back to the original 0x74-layout file (update.big's copy
			# first if it has one; it has none today), docs/m2-r2-mesh.md
			n15=0
			while IFS= read -r -d '' f; do
				rel=${f#"$T/"}
				if [[ -e "$X/update/$rel" ]]; then src="$X/update/$rel"; else src="$X/cataclysm/$rel"; fi
				[[ -e "$src" ]] || { echo "stage 15: no original for $rel" >&2; exit 1; }
				restore "$rel" "$src"; n15=$((n15 + 1))
			done < <(find "$T" \( -iname '*.geo' -o -iname '*.peo' \) -print0)
			echo "stage 15 undone: $n15 meshes point at the original files" ;;
		*) echo "stage $s cannot be undone here" >&2; exit 2 ;;
	esac
done
echo "isolation tree $T: stages undone: $stages; $(find "$T" -type l | wc -l) symlinks"
