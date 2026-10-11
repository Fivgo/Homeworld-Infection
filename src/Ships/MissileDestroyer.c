// =============================================================================
//  MissileDestroyer.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "MissileDestroyer.h"

#include "AIShip.h"
#include "AITrack.h"
#include "Attack.h"
#include "Battle.h"
#include "Collision.h"
#include "DefaultShip.h"
#include "Gun.h"
#include "SoundEvent.h"
#include "StatScript.h"
#include "Universe.h"

#define VOLLEY_BEGIN        10
#define VOLLEY_NOT_BEGIN    20

typedef struct
{
    real32 lasttimeRegeneratedMissiles;
    real32 lasttimeFiredVolley;
    real32 lasttimeDidSpecialTargeting;
    sdword curTargetIndex;
    sdword volleyState;
} MissileDestroyerSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(MissileDestroyerSpec) == 20, "MissileDestroyerSpec is 20 bytes in cataclysm.exe (MissileDestroyerHeader sizeofShipSpecifics)");   // CAT
#endif

typedef struct
{
    real32 missiledestroyerGunRange[NUM_TACTICS_TYPES];
    real32 missiledestroyerTooCloseRange[NUM_TACTICS_TYPES];
    real32 missileRegenerateTime;
    real32 missileVolleyTime;
    real32 missileLagVolleyTime;
} MissileDestroyerStatics;

// CAT: Taiidan (0x00a7aa20) and every other race (0x00a7a9e0) (MissileDestroyerStaticInit 0x005bb350); only
//      tMissileDestroyer uses this header. HW1 had R1/R2
MissileDestroyerStatics MissileDestroyerStaticTaiidan;
MissileDestroyerStatics MissileDestroyerStaticOther;

scriptStructEntry MissileDestroyerScriptTable[] =
{
    { "MissileRegenerateTime",scriptSetReal32CB, &(MissileDestroyerStaticTaiidan.missileRegenerateTime), &(MissileDestroyerStaticTaiidan) },
    { "MissileVolleyTime",scriptSetReal32CB, &(MissileDestroyerStaticTaiidan.missileVolleyTime), &(MissileDestroyerStaticTaiidan) },
    { "MissileLagVolleyTime",scriptSetReal32CB, &(MissileDestroyerStaticTaiidan.missileLagVolleyTime), &(MissileDestroyerStaticTaiidan) },

    END_SCRIPT_STRUCT_ENTRY
};

void MissileDestroyerStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    udword i;
    // CAT: Taiidan, else the other block (MissileDestroyerStaticInit 0x005bb350)
    MissileDestroyerStatics *mdestroyerstat = (statinfo->shiprace == RACE_Taiidan) ? &MissileDestroyerStaticTaiidan : &MissileDestroyerStaticOther;

    statinfo->custstatinfo = mdestroyerstat;

    scriptSetStruct(directory,filename,MissileDestroyerScriptTable,(ubyte *)mdestroyerstat);

    for(i=0;i<NUM_TACTICS_TYPES;i++)
    {
        mdestroyerstat->missiledestroyerGunRange[i] = statinfo->bulletRange[i];
        mdestroyerstat->missiledestroyerTooCloseRange[i] = statinfo->minBulletRange[i] * 0.9f;
    }
}

void MissileDestroyerInit(Ship *ship)
{
    MissileDestroyerSpec *spec = (MissileDestroyerSpec *)ship->ShipSpecifics;

    spec->lasttimeRegeneratedMissiles = 0.0f;
    spec->lasttimeFiredVolley = 0.0f;
    spec->lasttimeDidSpecialTargeting = -100000.0f;
    spec->curTargetIndex = 0;
    spec->volleyState = VOLLEY_BEGIN;
}

void MissileDestroyerAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    MissileDestroyerStatics *mdestroyerstat = (MissileDestroyerStatics *)shipstaticinfo->custstatinfo;

    attackStraightForward(ship,target,mdestroyerstat->missiledestroyerGunRange[ship->tacticstype],mdestroyerstat->missiledestroyerTooCloseRange[ship->tacticstype]);
}

void MissileDestroyerAttackPassive(Ship *ship,Ship *target,bool32 rotate)
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

void MissileDestroyerHousekeep(Ship *ship)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    MissileDestroyerSpec *spec = (MissileDestroyerSpec *)ship->ShipSpecifics;
    MissileDestroyerStatics *mdestroyerstat = (MissileDestroyerStatics *)shipstaticinfo->custstatinfo;
    sdword numGuns;
    sdword i;
    Gun *gun;
    GunStatic *gunstatic;
    sdword minMissiles;
    sdword minIndex;

    if ((universe.totaltimeelapsed - spec->lasttimeDidSpecialTargeting) > mdestroyerstat->missileLagVolleyTime)
    {
        if ((universe.totaltimeelapsed - spec->lasttimeRegeneratedMissiles) > mdestroyerstat->missileRegenerateTime)
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

bool32 MissileDestroyerSpecialTarget(Ship *ship,void *custom)
{
    SelectAnyCommand *targets;
    sdword i;
    sdword numShipsToTarget;
    sdword numGuns;
    Gun *gun;
    real32 range;
    GunStatic *gunstatic;
    SpaceObjRotImpTarg *target;
    ShipStaticInfo *shipstaticinfo;
    MissileDestroyerSpec *spec;
    MissileDestroyerStatics *mdestroyerstat;
    bool32 firedSomeMissiles,triedToFire;
    vector trajectory;

    spec = (MissileDestroyerSpec *)ship->ShipSpecifics;
    shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    mdestroyerstat = (MissileDestroyerStatics *)shipstaticinfo->custstatinfo;

    spec->lasttimeDidSpecialTargeting = universe.totaltimeelapsed;

    // check that the "volley reload time" has passed
    if ((universe.totaltimeelapsed - spec->lasttimeFiredVolley) > mdestroyerstat->missileVolleyTime)
    {
        spec->lasttimeFiredVolley = universe.totaltimeelapsed;
    }
    else
    {
        return FALSE;
    }

    targets = (SelectAnyCommand *)custom;
    numShipsToTarget = targets->numTargets;

    if (numShipsToTarget == 0)
    {
        // have destroyed targets, so we are done
        spec->curTargetIndex = 0;
        spec->volleyState = VOLLEY_BEGIN;
        return TRUE;
    }

    if(spec->volleyState == VOLLEY_BEGIN)
    {
        spec->volleyState = VOLLEY_NOT_BEGIN;
        //////////////////////
        //Volley attack speech event!
        //event num: COMM_MissleDest_VolleyAttack
        //use battle chatter
        if(ship->playerowner->playerIndex == universe.curPlayerIndex)
        {
            if (battleCanChatterAtThisTime(BCE_COMM_MissleDest_VolleyAttack, ship))
            {
                battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_COMM_MissleDest_VolleyAttack, ship, SOUND_EVENT_DEFAULT);
            }
        }
        //////////////////////
    }

    if (spec->curTargetIndex >= numShipsToTarget)
    {
        spec->curTargetIndex = 0;
    }

    numGuns = ship->gunInfo->numGuns;
    firedSomeMissiles = FALSE;
    triedToFire=FALSE;
    for (i=0;i<numGuns;i++)
    {
        gunstatic = &shipstaticinfo->gunStaticInfo->gunstatics[i];
        if (gunstatic->guntype == GUN_MissileLauncher)
        {
            vecSub(trajectory,ship->impactable.collInfo.collPosition,targets->TargetPtr[spec->curTargetIndex]->impactable.collInfo.collPosition);
            range = RangeToTarget(ship,targets->TargetPtr[spec->curTargetIndex],&trajectory);
            gun = &ship->gunInfo->guns[i];
            if(range < (gun->gunstatic->bulletrange*0.9f))
            {
                triedToFire=TRUE;
                if (gunHasMissiles(gun))
                {
                    firedSomeMissiles = TRUE;

                    target = targets->TargetPtr[spec->curTargetIndex];

                    missileShoot(ship,gun,target);

                    spec->curTargetIndex++;
                    if (spec->curTargetIndex >= numShipsToTarget)
                    {
                        spec->curTargetIndex = 0;
                    }
                }
            }
        }
    }

    if (firedSomeMissiles)
    {
        aitrackZeroVelocity(ship);
        return FALSE;
    }
    else if(triedToFire)
    {
        // all empty of missiles, so we are done
        spec->curTargetIndex = 0;
        spec->volleyState = VOLLEY_BEGIN;
        return TRUE;
    }
    else
    {
        //didn't try to fire...so lets fly towards a target...lets pick...0
        aishipFlyToShipAvoidingObjs(ship,(Ship *)targets->TargetPtr[0],AISHIP_FastAsPossible|AISHIP_PointInDirectionFlying,0.0f);
    }
    
    return FALSE;
}

// CAT: the exe's header (MissileDestroyerHeader 0x008c5320: tMissileDestroyer; original/scratch-m2/m2r2-shipstatic-tools/headers.txt).
//      Slot 6: the exe folded the identical "rotate ? attackPassiveRotate : attackPassive" body into P1MothershipAttackPassive 0x005be540.
CustShipHeader MissileDestroyerHeader =
{
    .sizeofShipSpecifics = sizeof(MissileDestroyerSpec),
    .CustShipStaticInit = MissileDestroyerStaticInit,
    .CustShipInit = MissileDestroyerInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = MissileDestroyerAttackPassive,
    .CustShipSpecialTarget = MissileDestroyerSpecialTarget,
    .CustShipHousekeep = MissileDestroyerHousekeep,
    .CustShipAttack = MissileDestroyerAttack,
};

