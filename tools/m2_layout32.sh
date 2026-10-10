#!/usr/bin/env bash
# Check the ILP32 (cataclysm.exe) layout pins of src/Game/SpaceObj.h and src/Game/Mesh.h.
#
# Usage: tools/m2_layout32.sh
#   Compiles a one-line translation unit that includes SpaceObj.h with clang -m32 -fsyntax-only, so the
#   "#if defined(__i386__)" branch of the CAT layout asserts (ShipStaticInfo 0x458, GunStatic 0x11C,
#   CustShipHeader 0x8C and the field offsets) is evaluated; Mesh.h likewise (polygonobject 0x74, meshdata head
#   0x20, docs/m2-r2-mesh.md), texreg.h (texreg 0x34) and SoundStructs.h (SENTENCELUT header 0x10,
#   docs/m2-r2-startup.md). Prints the ILP32 sizes on success.
#   No 32-bit libc is needed: an empty gnu/stubs-32.h shim is created in a temporary directory.
# Exit status: 0 if every assert holds, 1 otherwise.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf -- "$tmp"' EXIT
mkdir -p "$tmp/shim/gnu"
: > "$tmp/shim/gnu/stubs-32.h"
cat > "$tmp/layout.c" <<'EOF'
#include "SpaceObj.h"
#include "Mesh.h"
#include "texreg.h"
#include "SoundStructs.h"
char (*cat_ssi)[sizeof(ShipStaticInfo)] = 1;
char (*cat_gs)[sizeof(GunStatic)] = 1;
char (*cat_csh)[sizeof(CustShipHeader)] = 1;
char (*cat_po)[sizeof(polygonobject)] = 1;
char (*cat_tr)[sizeof(texreg)] = 1;
char (*cat_sl)[sizeof(SENTENCELUT)] = 1;
EOF
cc=(clang -m32 -fsyntax-only -std=gnu11 -D_X86 -malign-double -DHW_GAME_HOMEWORLD -DHW_BUILD_FOR_DISTRIBUTION
	-D_LINUX_FIX_ME -D_GNU_SOURCE -I"$root/src/Game" -I"$root/src/SDL" -I"$root/src/Ships"
	-I"$root/src/ThirdParty/CRC" -I"$root/src/ThirdParty/LZSS" -I"$root/src/ThirdParty/Titan" -I/usr/include/SDL2
	-idirafter /usr/include/x86_64-linux-gnu -I"$tmp/shim" -Wno-int-conversion)
out="$("${cc[@]}" "$tmp/layout.c" 2>&1 || true)"
if grep -q "error" <<<"$out"; then
	grep "error" <<<"$out"
	echo "m2_layout32: FAIL"
	exit 1
fi
"${cc[@]}" -Wint-conversion "$tmp/layout.c" 2>&1 | grep -o "'char (\*)\[[0-9]*\]'" | tr -d "'" |
	paste -sd' ' | sed 's/^/ILP32 sizes (ShipStaticInfo GunStatic CustShipHeader polygonobject texreg SENTENCELUT): /'
echo "m2_layout32: OK (all ILP32 layout asserts hold)"
