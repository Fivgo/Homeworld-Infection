// =============================================================================
//  Dreadnought.c
//  - Cataclysm's sDreadnought: a missile destroyer whose upgrade grows a
//    charge cannon module (cataclysm.exe 0x005b0e50-0x005b13e0, 0x005b3850-0x005b3900)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each). The
//      cannon's charge glow (cat_dreadnoughtRenderCharge 0x005b1110, cat_dreadnoughtChargeEffect 0x00516a40) is the
//      renderer's: stage 3. cat_dreadnoughtVariantStaticInit 0x005b3850 has no caller and no header slot.

#include "Dreadnought.h"

#include "Attack.h"
#include "Blast.h"
#include "Clamp.h"
#include "DefaultShip.h"
#include "Dissolve.h"
#include "Gun.h"
#include "RKO.h"
#include "SaveGame.h"
#include "ShipSelect.h"
#include "SoundEvent.h"
#include "StatScript.h"
#include "UnivUpdate.h"
#include "Universe.h"

#define DREADNOUGHT_CANNON_RECHARGE     70.0f       // 0x008c40e8
#define DREADNOUGHT_CANNON_FIRE_DELAY   1.0f        // 0x008c40ec

DreadnoughtStatics DreadnoughtStatic;               // 0x00aadc80

// 0x008c40f0
scriptStructEntry DreadnoughtScriptTable[] =
{
    { "MissileRegenerateTime", scriptSetReal32CB, &(DreadnoughtStatic.missileRegenerateTime), &(DreadnoughtStatic) },
    { "MissileVolleyTime",     scriptSetReal32CB, &(DreadnoughtStatic.missileVolleyTime),     &(DreadnoughtStatic) },
    { "MissileLagVolleyTime",  scriptSetReal32CB, &(DreadnoughtStatic.missileLagVolleyTime),  &(DreadnoughtStatic) },

    END_SCRIPT_STRUCT_ENTRY
};

void DreadnoughtStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)     // 0x005b0e50
{
    sdword i;

    statinfo->custstatinfo = &DreadnoughtStatic;
    scriptSetStruct(directory,filename,DreadnoughtScriptTable,(ubyte *)&DreadnoughtStatic);
    for (i = 0; i < NUM_TACTICS_TYPES; i++)
    {
        DreadnoughtStatic.gunRange[i] = statinfo->bulletRange[i];
        DreadnoughtStatic.tooCloseRange[i] = statinfo->minBulletRange[i] * 0.9f;   // 0x0072a5b4
    }
}

void DreadnoughtInit(Ship *ship)                                                            // 0x005b0eb0
{
    DreadnoughtSpec *spec = (DreadnoughtSpec *)ship->ShipSpecifics;
    sdword i;

    spec->lasttimeRegeneratedMissiles = 0.0f;
    spec->unknown04 = 0;
    spec->lasttimeFiredVolley = -100000.0f;
    spec->unknown0C = 0;
    cat_timerStartGameTime(&spec->cannonChargeTimer, DREADNOUGHT_CANNON_RECHARGE);
    for (i = 0; i < 40; i++)
    {
        spec->entries[i].enabled = 1;
    }
    spec->cannonFiring = FALSE;
    spec->cannon = NULL;
}

void DreadnoughtHouseKeep(Ship *ship)                                                       // 0x005b0f10
{
    DreadnoughtSpec *spec = (DreadnoughtSpec *)ship->ShipSpecifics;
    DreadnoughtStatics *dreadstat = (DreadnoughtStatics *)ship->staticinfo->custstatinfo;
    GunInfo *gunInfo;
    Gun *gun;
    sdword i, pick = -1, fewest = 100000;

    // a missile every MissileRegenerateTime, once MissileLagVolleyTime has passed since the last volley, into the
    // missile gun with the fewest missiles below its maximum (StandardDestroyerHouseKeep's code, for every type)
    if (((universe.totaltimeelapsed - spec->lasttimeFiredVolley) > dreadstat->missileLagVolleyTime) &&
        ((universe.totaltimeelapsed - spec->lasttimeRegeneratedMissiles) > dreadstat->missileRegenerateTime))
    {
        spec->lasttimeRegeneratedMissiles = universe.totaltimeelapsed;
        gunInfo = ship->gunInfo;
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
            ship->gunInfo->guns[pick].numMissiles++;
        }
    }
    // the cannon fires a type-4 blast 1 s after the order, if charged; rams on the ship let go
    if ((spec->cannon != NULL) && spec->cannonFiring && cat_timerExpired(&spec->cannonFireTimer))
    {
        cat_shipReleaseAttachedRammers(ship);
        spec->cannonFiring = FALSE;
        if (cat_shipHasUpgrade(ship, CAT_UPGRADE_DreadnoughtCannon) && cat_timerExpired(&spec->cannonChargeTimer))
        {
            cat_blastCreate(4, ship->obj.posinfo.position.x, ship->obj.posinfo.position.y, ship->obj.posinfo.position.z,
                            ship->playerowner->playerIndex);
            cat_timerStartGameTime(&spec->cannonChargeTimer, DREADNOUGHT_CANNON_RECHARGE);
        }
    }
    // the upgrade grows the cannon (not while dissolving), and its build progress follows a timer
    if (cat_shipHasUpgrade(ship, CAT_UPGRADE_DreadnoughtCannon))
    {
        if (spec->cannon == NULL)
        {
            if (!ship->catDissolving)
            {
                cat_dreadnoughtCreateCannon(ship);
                cat_timerStartGameTime(&spec->cannonBuildTimer, 0.01f);
            }
        }
    }
    if (spec->cannon == NULL)
    {
        return;
    }
    if (!cat_timerExpired(&spec->cannonBuildTimer))
    {
        spec->cannon->catBuildProgress = cat_timerFractionElapsed(&spec->cannonBuildTimer);
    }
}

bool32 DreadnoughtSpecialActivate(Ship *ship)                                               // 0x005b1160
{
    DreadnoughtSpec *spec = (DreadnoughtSpec *)ship->ShipSpecifics;

    if ((spec->cannon != NULL) && cat_shipHasUpgrade(ship, CAT_UPGRADE_DreadnoughtCannon) && !spec->cannonFiring &&
        cat_timerExpired(&spec->cannonChargeTimer))
    {
        speechEventQueue(ship, 0x40008ac, -1, -1, -1, -1, -1, 0.0f, -1);
        spec->cannonFiring = TRUE;
        cat_timerStartGameTime(&spec->cannonFireTimer, DREADNOUGHT_CANNON_FIRE_DELAY);
    }
    return TRUE;
}

void DreadnoughtDied(Ship *ship)                                                            // 0x005b11e0
{
    DreadnoughtSpec *spec = (DreadnoughtSpec *)ship->ShipSpecifics;
    Ship *cannon = spec->cannon;
    SpaceObjRotImpTarg *host;

    if (cannon == NULL)
    {
        return;
    }
    // a cannon on a dreadnought docked at a Sect mothership is clamped to the mothership: let it go first
    if ((cannon->guidance.clampInfo != NULL) && ((host = cannon->guidance.clampInfo->host) != NULL) &&
        (((Ship *)host)->shiptype == sMothership))
    {
        unClampObj((SpaceObjRotImpTargGuidance *)cannon);
        bitClear(cannon->header.flags, SOF_CatShipPart);
    }
    univReallyDeleteThisShipRightNow(cannon);
}

void Dreadnought_PreFix(Ship *ship)                                                         // 0x005b1230
{
    DreadnoughtSpec *spec = (DreadnoughtSpec *)ship->ShipSpecifics;

    spec->cannon = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->cannon);
}

void Dreadnought_Fix(Ship *ship)                                                            // 0x005b1250
{
    DreadnoughtSpec *spec = (DreadnoughtSpec *)ship->ShipSpecifics;

    spec->cannon = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->cannon);
}

void DreadnoughtRemoveShipReferences(Ship *ship,Ship *shiptoremove)                         // 0x005b1270
{
    DreadnoughtSpec *spec = (DreadnoughtSpec *)ship->ShipSpecifics;

    if (spec->cannon == shiptoremove)
    {
        spec->cannonFiring = FALSE;
        spec->cannon = NULL;
    }
}

bool32 cat_dreadnoughtHasCannonUpgrade(Ship *ship)                                          // 0x005b1290
{
    return (ship->shiptype == sDreadnought) && cat_shipHasUpgrade(ship, CAT_UPGRADE_DreadnoughtCannon);
}

real32 cat_dreadnoughtChargeFraction(Ship *ship)                                            // 0x005b12c0
{
    real32 fraction = cat_timerFractionElapsed(&((DreadnoughtSpec *)ship->ShipSpecifics)->cannonChargeTimer);

    return (fraction > 1.0f) ? 1.0f : fraction;
}

// the cannon module: a ship of the dreadnought's player clamped to it (or to the ship it is clamped to) as a ship part
void cat_dreadnoughtCreateCannon(Ship *ship)                                                // 0x005b12f0
{
    DreadnoughtSpec *spec = (DreadnoughtSpec *)ship->ShipSpecifics;
    Ship *cannon;
    Ship *dest = ship;

    cat_timerStartGameTime(&spec->cannonBuildTimer, cat_shipUpgradeFlagsTime(ship, CAT_UPGRADE_DreadnoughtCannon));
    // CAT: the exe's univCreateShip 0x00542320 adds the cannon to the universe with the dreadnought as its creator
    //      (7 arguments: ..., built 0, creator ship, 0); univAddShip does the first, the second is set here
    cannon = univAddShip(sDreadnoughtCannon, ship->shiprace, &ship->obj.posinfo.position, ship->playerowner, 0);
    cannon->catCreator = ship;
    cannon->rotatable.rotinfo.coordsys = ship->rotatable.rotinfo.coordsys;
    gameStatsAddShip(cannon, cannon->playerowner->playerIndex);
    unitCapCreateShip(cannon, cannon->playerowner);
    if ((ship->guidance.clampInfo != NULL) && (ship->guidance.clampInfo->host != NULL))
    {
        dest = (Ship *)ship->guidance.clampInfo->host;
    }
    clampObjToObj((SpaceObjRotImpTargGuidance *)cannon, (SpaceObjRotImpTargGuidance *)dest);
    bitSet(cannon->header.flags, SOF_CatShipPart);
    spec->cannon = cannon;
}

void cat_dreadnoughtReclampCannon(Ship *ship)                                               // 0x005b13b0
{
    Ship *cannon = ((DreadnoughtSpec *)ship->ShipSpecifics)->cannon;

    if (cannon != NULL)
    {
        unClampObj((SpaceObjRotImpTargGuidance *)cannon);
        clampObjToObj((SpaceObjRotImpTargGuidance *)cannon, (SpaceObjRotImpTargGuidance *)ship);
    }
}

// guns by weapon type: projectile, energy, ion and big gun shoot; missiles and mines launch while there are any
void DreadnoughtFire(Ship *ship, SpaceObjRotImpTarg *target)                                // 0x005b3870
{
    GunInfo *gunInfo = ship->gunInfo;
    Gun *gun;
    sdword i;

    if (gunInfo == NULL)
    {
        return;
    }
    for (i = 0; i < gunInfo->numGuns; i++)
    {
        gun = &gunInfo->guns[i];
        if (!cat_gunCanFire(ship, gun))
        {
            continue;
        }
        switch (gun->catWeaponType)
        {
            case WEAPON_PROJECTILE:
            case WEAPON_ENERGY:
            case WEAPON_ION:
            case WEAPON_BIGGUN:
                gunShoot(ship, gun, target);
                break;
            case WEAPON_MISSILE:
            case WEAPON_MINE:
                if (gun->numMissiles > 0)
                {
                    missileShoot(ship, gun, target);
                }
                break;
            default:
                break;
        }
    }
}

void DreadnoughtAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)               // the slot-21 body
{
    DreadnoughtStatics *dreadstat = (DreadnoughtStatics *)ship->staticinfo->custstatinfo;

    // CAT: slot 21 is MissileDestroyerAttack 0x005bb3e0, the folded attackStraightForward(gunRange, tooCloseRange)
    (void)maxdist;
    attackStraightForward(ship, target, dreadstat->gunRange[ship->tacticstype], dreadstat->tooCloseRange[ship->tacticstype]);
}

// CAT: DreadnoughtHeader 0x008c4130 (sDreadnought); slot 6 is the folded attackPassive body (P1MothershipAttackPassive),
//      slot 8 nullsubReturnFalse
CustShipHeader DreadnoughtHeader =
{
    .sizeofShipSpecifics = sizeof(DreadnoughtSpec),
    .CustShipStaticInit = DreadnoughtStaticInit,
    .CustShipInit = DreadnoughtInit,
    .CustShipFire = DreadnoughtFire,
    .CustShipAttackPassive = DefaultShipAttackPassive,
    .CustShipSpecialActivate = DreadnoughtSpecialActivate,
    .CustShipSpecialTarget = nullsubReturnFalseTarget,
    .CustShipHousekeep = DreadnoughtHouseKeep,
    .CustShipRemoveShipReferences = DreadnoughtRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))DreadnoughtRemoveShipReferences,
    .CustShipDied = DreadnoughtDied,
    .CustShip_PreFix = Dreadnought_PreFix,
    .CustShip_Fix = Dreadnought_Fix,
    .CustShipAttack = DreadnoughtAttack,
};
