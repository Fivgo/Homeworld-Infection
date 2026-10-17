#!/usr/bin/env bash
# Check the ILP32 (cataclysm.exe) layout pins of src/Game/SpaceObj.h and src/Game/Mesh.h.
#
# Usage: tools/m2_layout32.sh
#   Compiles a one-line translation unit that includes SpaceObj.h with clang -m32 -fsyntax-only, so the
#   "#if defined(__i386__)" branch of the CAT layout asserts (ShipStaticInfo 0x458, GunStatic 0x11C,
#   CustShipHeader 0x8C and the field offsets) is evaluated; Mesh.h likewise (polygonobject 0x74, meshdata head
#   0x20, docs/m2-r2-mesh.md), texreg.h (texreg 0x34) and SoundStructs.h (SENTENCELUT header 0x10,
#   docs/m2-r2-startup.md); since M2-R3 also AITeam.h (AlternativeShips 0xA8), Formation.h (MilitaryParadeCommand
#   0x104), Randy.h (ranstream 0x14), SaveGame.h (SaveChunk 8) and SpaceObj.h's ClampInfo 0x38 / StaticCollInfo 0x70
#   (docs/m2-r3-closeout-headers.md); since M2-R3 etg-particles also Particle.h (particle 0x100,
#   point/mesh/billboard/electric-bolt system headers 0x1c/0x30/0x50/0x24, lens flare source within 0x40;
#   docs/m2-r3-etg-particles.md) [audit: the etg-particles lines had split the closeout sentence]; since M2-R3
#   ships-a also Cloak.h, Salvage.h, BeastWeapon.h, Timer.h (CatTimer) and, by a second -m32 -fsyntax-only pass over
#   the ship files, every reused HW1 header's specifics size against the exe's sizeofShipSpecifics
#   (docs/m2-r3-ships-a.md); since M2-R4 rest-oracle also Universe.h (Player 0x6530, Universe 0x3D9BC), GameStats.h
#   (PlayerStats 0x7F8, GameStats 0x47E0), ResearchAPI.h (TechStatics 0x7AB4, PlayerResearchInfo 0x6250) and ConsMgr.h
#   (shipinprogress 0x30) (docs/m2-r4-rest-oracle.md); since M2-R5 layout-dock also the object layouts of SpaceObj.h
#   (SpaceObj 0x98 with the attribute dword at +0x38, Ship 0x63C with health at 0x170 and CET's Ship offsets [audit: not 40], Missile
#   0x258, Bullet 0x164, Derelict 0x294, Asteroid 0x1B4; docs/m2-r5-layout-dock.md); since M2-R5 command-tactics also
#   CommandLayer.h (CommandToDo 0x330 with the waypoint MoveCommand 0x108), Formation.h (FormationCommand 0x60) and
#   Tactics.h (TacticsInfo; docs/m2-r5-command-tactics.md). Prints the ILP32 sizes on success.
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
#include "AITeam.h"
#include "Formation.h"
#include "Randy.h"
#include "SaveGame.h"
#include "Particle.h"
#include "Cloak.h"
#include "Salvage.h"
#include "BeastWeapon.h"
#include "Timer.h"
#include "Universe.h"
#include "GameStats.h"
#include "ResearchAPI.h"
#include "ConsMgr.h"
#include "CommandLayer.h"
#include "Tactics.h"
char (*cat_ssi)[sizeof(ShipStaticInfo)] = 1;
char (*cat_gs)[sizeof(GunStatic)] = 1;
char (*cat_csh)[sizeof(CustShipHeader)] = 1;
char (*cat_po)[sizeof(polygonobject)] = 1;
char (*cat_tr)[sizeof(texreg)] = 1;
char (*cat_sl)[sizeof(SENTENCELUT)] = 1;
char (*cat_ci)[sizeof(ClampInfo)] = 1;
char (*cat_sci)[sizeof(StaticCollInfo)] = 1;
char (*cat_alt)[sizeof(AlternativeShips)] = 1;
char (*cat_mpc)[sizeof(MilitaryParadeCommand)] = 1;
char (*cat_rs)[sizeof(ranstream)] = 1;
char (*cat_sc)[sizeof(SaveChunk)] = 1;
char (*cat_pt)[sizeof(particle)] = 1;
char (*cat_eb)[sizeof(eboltSystem)] = 1;
char (*cat_pl)[sizeof(Player)] = 1;
char (*cat_un)[sizeof(Universe)] = 1;
char (*cat_ps)[sizeof(PlayerStats)] = 1;
char (*cat_ts)[sizeof(TechStatics)] = 1;
char (*cat_sip)[sizeof(shipinprogress)] = 1;
char (*cat_ship)[sizeof(Ship)] = 1;
char (*cat_mis)[sizeof(Missile)] = 1;
char (*cat_bul)[sizeof(Bullet)] = 1;
char (*cat_der)[sizeof(Derelict)] = 1;
char (*cat_ast)[sizeof(Asteroid)] = 1;
char (*cat_ctd)[sizeof(CommandToDo)] = 1;
char (*cat_fc)[sizeof(FormationCommand)] = 1;
char (*cat_ti)[sizeof(TacticsInfo)] = 1;
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
	paste -sd' ' | sed 's/^/ILP32 sizes (ShipStaticInfo GunStatic CustShipHeader polygonobject texreg SENTENCELUT ClampInfo StaticCollInfo AlternativeShips MilitaryParadeCommand ranstream SaveChunk particle eboltSystem Player Universe PlayerStats TechStatics shipinprogress Ship Missile Bullet Derelict Asteroid CommandToDo FormationCommand TacticsInfo): /'
# M2-R3 ships-a: the ship files whose specifics/statics sizes are pinned under __i386__ (asserts in the files); M2-R4
# ships-b added the Cataclysm-only ships (docs/m2-r4-ships-b.md)
ships=(AdvanceSupportFrigate Carrier DFGFrigate DefaultShip FloatingCity GenericDefender GenericInterceptor
	GravWellGenerator HeavyCorvette HeavyCruiser IonCannonFrigate MinelayerCorvette MissileDestroyer Mothership
	MultiGunCorvette P1IonArrayFrigate P1MissileCorvette P1Mothership P1StandardCorvette ProximitySensor
	ResourceCollector SalCapCorvette StandardDestroyer StandardFrigate
	CruiseMissile Crystal dBeastMothership HiveFrigate ModuleShip MultiBeamFrigate NomadMoon RepairDroid Slipgate
	Swarmer Acolyte ACV Dreadnought Leech MCV Mimic Processor RammingFrigate RKO Sentinel Worker)
shipfiles=()
for s in "${ships[@]}"; do shipfiles+=("$root/src/Ships/$s.c"); done
shipout="$("${cc[@]}" -w "${shipfiles[@]}" 2>&1 || true)"
if grep -q "error" <<<"$shipout"; then
	grep "error" <<<"$shipout"
	echo "m2_layout32: FAIL (ship specifics)"
	exit 1
fi
echo "ILP32 ship specifics: ${#ships[@]} files, every sizeofShipSpecifics assert holds"
echo "m2_layout32: OK (all ILP32 layout asserts hold)"
