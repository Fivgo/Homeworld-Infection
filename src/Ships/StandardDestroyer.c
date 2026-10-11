// =============================================================================
//  StandardDestroyer.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 01/06/1998 by bpasechnik
// =============================================================================

#include "StandardDestroyer.h"

#include "Attack.h"
#include "Debug.h"
#include "DefaultShip.h"
#include "Gun.h"
#include "ObjTypes.h"
#include "Universe.h"

// CAT: 0xD30 bytes in cataclysm.exe (StandardDestroyerHeader sizeofShipSpecifics 3376). StandardDestroyerHouseKeep
//      0x005c7800 uses +0x00 (last missile regeneration) and +0x08 (last volley, written by the volley code); the
//      rest is not read by any reused-HW1 body and is kept opaque at the exe's size (its users are M2-R4 #3/#4)
typedef struct
{
    real32 lasttimeRegeneratedMissiles;     // +0x00
    udword unknown04;                       // +0x04
    real32 lasttimeFiredVolley;             // +0x08
    ubyte unknown0C[0xD30 - 0x0C];
} StandardDestroyerSpec;
_Static_assert(sizeof(StandardDestroyerSpec) == 0xD30, "StandardDestroyerSpec is 0xD30 bytes in cataclysm.exe");

typedef struct
{
    real32 standarddestroyerGunRange[NUM_TACTICS_TYPES];
    real32 standarddestroyerTooCloseRange[NUM_TACTICS_TYPES];
    // CAT: sDestroyer's missile regeneration keys (+0x18..+0x20; key table 0x008c6950, read only for the Sect
    //      block by StandardDestroyerStaticInit 0x005c7780 and used by StandardDestroyerHouseKeep 0x005c7800)
    real32 missileRegenerateTime;
    real32 missileVolleyTime;
    real32 missileLagVolleyTime;
} StandardDestroyerStatics;

// CAT: one block per race that has the ship: Sect (0x00a7a520: sDestroyer), Kushan (0x00a7a5a0), Taiidan
//      (0x00a7a560) (StandardDestroyerStaticInit 0x005c7780); HW1 had R1/R2
StandardDestroyerStatics StandardDestroyerStaticSect;
StandardDestroyerStatics StandardDestroyerStaticKushan;
StandardDestroyerStatics StandardDestroyerStaticTaiidan;
StandardDestroyerStatics StandardDestroyerStaticOther;  // CAT: the exe writes into the statinfo itself for any other race

// CAT: sDestroyer's missile keys (0x008c6950)
scriptStructEntry StandardDestroyerScriptTable[] =
{
    { "MissileRegenerateTime", scriptSetReal32CB, &(StandardDestroyerStaticSect.missileRegenerateTime), &(StandardDestroyerStaticSect) },
    { "MissileVolleyTime",     scriptSetReal32CB, &(StandardDestroyerStaticSect.missileVolleyTime),     &(StandardDestroyerStaticSect) },
    { "MissileLagVolleyTime",  scriptSetReal32CB, &(StandardDestroyerStaticSect.missileLagVolleyTime),  &(StandardDestroyerStaticSect) },

    END_SCRIPT_STRUCT_ENTRY
};

void StandardDestroyerStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    udword i;
    StandardDestroyerStatics *sdestroyerstat;

    // CAT: by Cataclysm race; the Sect block also reads the missile keys (StandardDestroyerStaticInit 0x005c7780)
    switch (statinfo->shiprace)
    {
        case RACE_Sect:
            scriptSetStruct(directory,filename,StandardDestroyerScriptTable,(ubyte *)&StandardDestroyerStaticSect);
            sdestroyerstat = &StandardDestroyerStaticSect;
            break;
        case RACE_Kushan:  sdestroyerstat = &StandardDestroyerStaticKushan;  break;
        case RACE_Taiidan: sdestroyerstat = &StandardDestroyerStaticTaiidan; break;
        default:
            dbgAssertOrIgnore(FALSE);
            sdestroyerstat = &StandardDestroyerStaticOther;
            break;
    }

    statinfo->custstatinfo = sdestroyerstat;

    for(i=0;i<NUM_TACTICS_TYPES;i++)
    {
        sdestroyerstat->standarddestroyerGunRange[i] = statinfo->bulletRange[i];
        sdestroyerstat->standarddestroyerTooCloseRange[i] = statinfo->minBulletRange[i] * 0.9f;
    }
}

void StandardDestroyerAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    StandardDestroyerStatics *sdestroyerstat = (StandardDestroyerStatics *)shipstaticinfo->custstatinfo;

    attackStraightForward(ship,target,sdestroyerstat->standarddestroyerGunRange[ship->tacticstype],sdestroyerstat->standarddestroyerTooCloseRange[ship->tacticstype]);
}

void StandardDestroyerAttackPassive(Ship *ship,Ship *target,bool32 rotate)
{
    if ((rotate) & ((bool32)((ShipStaticInfo *)(ship->staticinfo))->rotateToRetaliate))
    {
        attackPassiveRotate(ship,target);
    }
    else
    {
        attackPassive(ship,target);
    }
}

void StandardDestroyerHouseKeep(Ship *ship)
{
    // CAT: new slot-10 body (StandardDestroyerHouseKeep 0x005c7800): sDestroyer regenerates one missile every
    //      MissileRegenerateTime seconds, once MissileLagVolleyTime has passed since its last volley, into the
    //      missile gun with the fewest missiles that is below its maximum. The exe tests the gun's current WEAPON_*
    //      type (Gun +0x00, Gun.catWeaponType since M2-R4 combat) for WEAPON_MISSILE.
    StandardDestroyerSpec *spec = (StandardDestroyerSpec *)ship->ShipSpecifics;
    StandardDestroyerStatics *sdestroyerstat = (StandardDestroyerStatics *)ship->staticinfo->custstatinfo;
    GunInfo *gunInfo;
    Gun *gun;
    sdword i, pick, fewest;

    if ((ship->shiptype != sDestroyer) ||
        ((universe.totaltimeelapsed - spec->lasttimeFiredVolley) <= sdestroyerstat->missileLagVolleyTime) ||
        ((universe.totaltimeelapsed - spec->lasttimeRegeneratedMissiles) <= sdestroyerstat->missileRegenerateTime))
    {
        return;
    }
    spec->lasttimeRegeneratedMissiles = universe.totaltimeelapsed;

    gunInfo = ship->gunInfo;
    pick = -1;
    fewest = 100000;
    for (i = 0; i < gunInfo->numGuns; i++)
    {
        gun = &gunInfo->guns[i];
        if ((gun->catWeaponType == WEAPON_MISSILE) &&
            (gun->numMissiles < ship->staticinfo->gunStaticInfo->gunstatics[i].maxMissiles) &&
            (gun->numMissiles < fewest))
        {
            pick = i;
            fewest = gun->numMissiles;
        }
    }
    if (pick != -1)
    {
        gunInfo->guns[pick].numMissiles++;
    }
}

// CAT: the exe's header (StandardDestroyerHeader 0x008c6990: sDestroyer, kStandardDestroyer, tStandardDestroyer).
//      Slot 6: the exe folded the identical "rotate ? attackPassiveRotate : attackPassive" body into P1MothershipAttackPassive 0x005be540.
//      Slot 21: the exe folded the identical attackStraightForward(gunRange, tooCloseRange) body into MissileDestroyerAttack 0x005bb3e0.
CustShipHeader StandardDestroyerHeader =
{
    .sizeofShipSpecifics = sizeof(StandardDestroyerSpec),
    .CustShipStaticInit = StandardDestroyerStaticInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = StandardDestroyerAttackPassive,
    .CustShipHousekeep = StandardDestroyerHouseKeep,         // CAT: new slot 10
    .CustShipAttack = StandardDestroyerAttack,
};

