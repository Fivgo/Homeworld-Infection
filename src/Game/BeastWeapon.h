// =============================================================================
//  BeastWeapon.h
//  - Cataclysm's Beast infection weapon of bHeavyCruiser, bCarrier,
//    bMothership and dBeastMothership (cataclysm.exe 0x004298f0-0x0042b650)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___BEASTWEAPON_H
#define ___BEASTWEAPON_H

#include "LinkedList.h"
#include "ShipSelect.h"
#include "SpaceObj.h"
#include "Types.h"

// CAT: new file (M2-R3 ships-a). The reused HW1 headers of Carrier, HeavyCruiser and Mothership call into this
//      weapon from their slots, so its predicates, its tweak file, its per-ship block and the block's init/close are
//      ported here. The weapon's runtime (target search, the infection bullets, cat_beastWeaponHouseKeep 0x0042a900,
//      cat_beastAttackHit 0x0042af40, cat_beastInfectShip 0x004298f0, save/load) is combat code, M2-R4 #3; until
//      then those entry points are CAT-PROVISIONAL and do nothing, so the four ships never fire it.

// RACE_BEAST\beastweapon.script (cat_beastWeaponStaticInit 0x00429b90, key table cat_beastWeaponScriptTable
// 0x00751698, block cat_beastWeaponTweaks 0x00c52140: a loaded flag, then the six keys at +0x04..+0x18)
typedef struct CatBeastWeaponTweaks
{
    bool8 loaded;                               // +0x00
    ubyte pad[3];
    real32 beastAttackCascadePoints;            // +0x04
    real32 beastAttackCascadeDelay;             // +0x08
    real32 beastAttackCascadeDistance;          // +0x0C
    real32 beastAttackTimeSpentStunned;         // +0x10
    real32 beastAttackTimeToCompleteAttacks;    // +0x14
    real32 stunnedSpinRate;                     // +0x18
} CatBeastWeaponTweaks;
_Static_assert(sizeof(CatBeastWeaponTweaks) == 0x1C, "cat_beastWeaponTweaks is 0x1C bytes in cataclysm.exe");

extern CatBeastWeaponTweaks cat_beastWeaponTweaks;

// The weapon block at +0x00 of the specifics of the four ships (Ship+0x638; cat_beastWeaponInit 0x0042a040), 0x18
// bytes on ILP32: the current target, the list of infection bullets in flight, and two words the runtime uses
typedef struct CatBeastWeaponSpec
{
    struct Ship *target;        // +0x00 cleared by init and by cat_beastWeaponOrderChanged
    LinkedList bulletList;      // +0x04
    udword unknown10;           // +0x10 (M2-R4 #3)
    udword unknown14;           // +0x14 cleared by init
} CatBeastWeaponSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatBeastWeaponSpec) == 0x18, "the beast weapon block is 0x18 bytes in cataclysm.exe");
#endif

bool32 cat_staticHasBeastWeapon(ShipStaticInfo *statinfo);
bool32 cat_shipHasBeastWeapon(Ship *ship);
void cat_beastWeaponStaticInit(void);

void cat_beastWeaponInit(Ship *ship);
void cat_beastWeaponClose(Ship *ship);
void cat_beastWeaponOrderChanged(Ship *ship);
void cat_beastWeaponHouseKeep(Ship *ship);
void cat_beastWeaponBulletRemoval(Ship *ship, Ship *unused, struct Bullet *bullet);
void cat_beastWeaponRemoveShipReferences(Ship *ship, Ship *shiptoremove);
bool32 cat_beastWeaponHasTarget(Ship *ship, SelectCommand *targets);
bool32 cat_beastWeaponAttack(Ship *ship, void *targets);
void cat_beastWeaponSave(Ship *ship);
void cat_beastWeaponLoad(Ship *ship);
void cat_beastWeaponFix(Ship *ship);

#endif
