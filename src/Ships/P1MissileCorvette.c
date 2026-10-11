// =============================================================================
//  P1MissileCorvette.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/06/1998 by ddunlop
// =============================================================================

#include "P1MissileCorvette.h"

#include "Attack.h"
#include "DefaultShip.h"
#include "Gun.h"
#include "Universe.h"

typedef struct
{
    AttackSideStep attacksidestep;
    real32 lasttimeRegeneratedMissiles;
    real32 lasttimeFiredVolley;
    real32 lasttimeDidSpecialTargeting;
    sdword curTargetIndex;
} P1MissileCorvetteSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(P1MissileCorvetteSpec) == 24, "P1MissileCorvetteSpec is 24 bytes in cataclysm.exe (P1MissileCorvetteHeader sizeofShipSpecifics)");   // CAT
#endif

typedef struct
{
    real32 missilecorvetteGunRange[NUM_TACTICS_TYPES];
    real32 missilecorvetteTooCloseRange[NUM_TACTICS_TYPES];
    real32 missileRegenerateTime;
    real32 missileVolleyTime;
    real32 missileLagVolleyTime;
    AttackSideStepParameters sidestepParameters;
} P1MissileCorvetteStatics;

// CAT: P1 (0x00a7a8c0) and every other race (0x00a7a880: bMissileCorvette) (P1MissileCorvetteStaticInit
//      0x005be2e0); HW1 had one block
P1MissileCorvetteStatics P1MissileCorvetteStatic;
P1MissileCorvetteStatics P1MissileCorvetteStaticBeast;

scriptStructEntry P1MissileCorvetteScriptTable[] =
{
    { "MissileRegenerateTime",scriptSetReal32CB, &(P1MissileCorvetteStatic.missileRegenerateTime), &(P1MissileCorvetteStatic) },
    { "MissileVolleyTime",scriptSetReal32CB, &(P1MissileCorvetteStatic.missileVolleyTime), &(P1MissileCorvetteStatic) },
    { "MissileLagVolleyTime",scriptSetReal32CB, &(P1MissileCorvetteStatic.missileLagVolleyTime), &(P1MissileCorvetteStatic) },

    END_SCRIPT_STRUCT_ENTRY
};

void P1MissileCorvetteStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    udword i;
    // CAT: P1, else the Beast block; no memset first (P1MissileCorvetteStaticInit 0x005be2e0)
    P1MissileCorvetteStatics *corvstat = (statinfo->shiprace == RACE_P1) ? &P1MissileCorvetteStatic : &P1MissileCorvetteStaticBeast;

    scriptSetStruct(directory,filename,AttackSideStepParametersScriptTable,(ubyte *)&corvstat->sidestepParameters);
    scriptSetStruct(directory,filename,P1MissileCorvetteScriptTable,(ubyte *)corvstat);

    statinfo->custstatinfo = corvstat;

    for(i=0;i<NUM_TACTICS_TYPES;i++)
    {
        corvstat->missilecorvetteGunRange[i] = statinfo->bulletRange[i];
        corvstat->missilecorvetteTooCloseRange[i] = statinfo->minBulletRange[i] * 0.9f;
    }
}

void P1MissileCorvetteInit(Ship *ship)
{
    P1MissileCorvetteSpec *spec = (P1MissileCorvetteSpec *)ship->ShipSpecifics;

    attackSideStepInit(&spec->attacksidestep);

    spec->lasttimeRegeneratedMissiles = 0.0f;
    spec->lasttimeFiredVolley = 0.0f;
    spec->lasttimeDidSpecialTargeting = -100000.0f;
    spec->curTargetIndex = 0;
}

void P1MissileCorvetteAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    P1MissileCorvetteSpec *spec = (P1MissileCorvetteSpec *)ship->ShipSpecifics;
    P1MissileCorvetteStatics *corvstat = (P1MissileCorvetteStatics *)((ShipStaticInfo *)ship->staticinfo)->custstatinfo;

    attackSideStep(ship,target,&spec->attacksidestep,&corvstat->sidestepParameters);
}

void P1MissileCorvetteAttackPassive(Ship *ship,Ship *target,bool32 rotate)
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

void P1MissileCorvetteHousekeep(Ship *ship)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    P1MissileCorvetteSpec *spec = (P1MissileCorvetteSpec *)ship->ShipSpecifics;
    P1MissileCorvetteStatics *mcorvettestat = (P1MissileCorvetteStatics *)shipstaticinfo->custstatinfo;
    sdword numGuns;
    sdword i;
    Gun *gun;
    GunStatic *gunstatic;
    sdword minMissiles;
    sdword minIndex;

    if ((universe.totaltimeelapsed - spec->lasttimeDidSpecialTargeting) > mcorvettestat->missileLagVolleyTime)
    {
        if ((universe.totaltimeelapsed - spec->lasttimeRegeneratedMissiles) > mcorvettestat->missileRegenerateTime)
        {
            spec->lasttimeRegeneratedMissiles = universe.totaltimeelapsed;
            numGuns = ship->gunInfo->numGuns;

            // find missile launcher with least missiles, and give it a missile
            for (i=0,minIndex=-1,minMissiles=100000;i<numGuns;i++)
            {
                gunstatic = &shipstaticinfo->gunStaticInfo->gunstatics[i];
                if (gunstatic->guntype == GUN_MissileLauncher)
                {
                    gun = &ship->gunInfo->guns[i];
                    if (gun->numMissiles < gunstatic->maxMissiles)
                    {
                        if (gun->numMissiles < minMissiles)
                        {
                            minMissiles = gun->numMissiles;
                            minIndex = i;
                        }
                    }
                }
            }

            if (minIndex != -1)
            {
                ship->gunInfo->guns[minIndex].numMissiles++;
            }
        }
    }
}

bool32 P1MissileCorvetteSpecialTarget(Ship *ship,void *custom)
{
    SelectAnyCommand *targets;
    P1MissileCorvetteSpec *spec;
    P1MissileCorvetteStatics *mcorvettestat;

    spec = (P1MissileCorvetteSpec *)ship->ShipSpecifics;
    mcorvettestat = (P1MissileCorvetteStatics *)((ShipStaticInfo *)ship->staticinfo)->custstatinfo;

    spec->lasttimeDidSpecialTargeting = universe.totaltimeelapsed;

    if ((universe.totaltimeelapsed - spec->lasttimeFiredVolley) > mcorvettestat->missileVolleyTime)
    {
        spec->lasttimeFiredVolley = universe.totaltimeelapsed;
    }
    else
    {
        return FALSE;
    }

    // CAT: P1MissileCorvetteSpecialTarget 0x005be480 fires no missile volley: once the volley time has passed it
    //      resets the target index and reports the order done (HW1 fired every loaded launcher at the targets in turn)
    targets = (SelectAnyCommand *)custom;
    (void)targets;
    spec->curTargetIndex = 0;
    return TRUE;
}

// CAT: the exe's header (P1MissileCorvetteHeader 0x008c58d8: bMissileCorvette, p1MissileCorvette; original/scratch-m2/m2r2-shipstatic-tools/headers.txt).
//      Slot 6: the exe folded the identical "rotate ? attackPassiveRotate : attackPassive" body into P1MothershipAttackPassive 0x005be540.
CustShipHeader P1MissileCorvetteHeader =
{
    .sizeofShipSpecifics = sizeof(P1MissileCorvetteSpec),
    .CustShipStaticInit = P1MissileCorvetteStaticInit,
    .CustShipInit = P1MissileCorvetteInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = P1MissileCorvetteAttackPassive,
    .CustShipSpecialTarget = P1MissileCorvetteSpecialTarget,
    .CustShipHousekeep = P1MissileCorvetteHousekeep,
    .CustShipAttack = P1MissileCorvetteAttack,
};

