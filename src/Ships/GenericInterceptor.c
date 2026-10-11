// =============================================================================
//  GenericInterceptor.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "GenericInterceptor.h"

#include <math.h>

#include "AIShip.h"
#include "AITrack.h"
#include "Battle.h"
#include "Cloak.h"
#include "Collision.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "DefenseFighter.h"
#include "FastMath.h"
#include "Gun.h"
#include "MadLinkIn.h"
#include "MadLinkInDefs.h"
#include "MaxMultiplayer.h"
#include "NIS.h"
#include "Randy.h"
#include "ShipSelect.h"
#include "SoundEvent.h"
#include "Tactics.h"
#include "Universe.h"


#define DEBUG_INTERCEPTOR 0

typedef struct
{
    real32 lasttimefired;
    real32 aitimekill;              // AI timestamp for when we went into kill mode
    vector aivec;                   // AI vector when breaking
    bool16 aiAIPflightman;          // AI boolean indicating if attack in progress (AIP) flight maneuver has been chosen
    uword activeGun;
} GenericInterceptorSpec;

// CAT: 0x50 bytes in cataclysm.exe (CloakedFighterHeader sizeofShipSpecifics 80): the interceptor specifics, then
//      Cataclysm's cloak block at +0x18 (CloakedFighterInit 0x005b36f0 -> cat_cloakInit(ship, ship+0x650)) and the
//      per-player detected flags at +0x30 (sub_53F9F0 0x0053f9f0 reads Ship+0x668); HW1's cloak fuel fields are gone
typedef struct                      //Inherited From GenericInterceptorSpec
{
    real32 lasttimefired;
    real32 aitimekill;              // AI timestamp for when we went into kill mode
    vector aivec;                   // AI vector when breaking
    bool16 aiAIPflightman;          // AI boolean indicating if attack in progress (AIP) flight maneuver has been chosen
    uword activeGun;
    CatCloak cloak;                 // CAT: +0x18
    bool32 detected[MAX_MULTIPLAYER_PLAYERS];   // CAT: +0x30
} CloakedFighterSpec;
_Static_assert(sizeof(GenericInterceptorSpec) == 0x18, "GenericInterceptorSpec is 0x18 bytes in cataclysm.exe");
_Static_assert(offsetof(CloakedFighterSpec, cloak) == CAT_CLOAK_SPECOFS_CLOAKEDFIGHTER, "cloak at specifics +0x18");
_Static_assert(offsetof(CloakedFighterSpec, detected) == CAT_DETECTED_SPECOFS_CLOAKEDFIGHTER, "detected at +0x30");
_Static_assert(sizeof(CloakedFighterSpec) == 0x50, "CloakedFighterSpec is 0x50 bytes in cataclysm.exe");

typedef struct
{
    real32 lasttimefired;
    real32 aitimekill;              // AI timestamp for when we went into kill mode
    vector aivec;                   // AI vector when breaking
    bool16 aiAIPflightman;          // AI boolean indicating if attack in progress (AIP) flight maneuver has been chosen
    uword activeGun;
    vector rallypoint;
    vector orig_point;
    real32 angle;
    sdword bombingdelay;
} AttackBomberSpec;
// CAT: the exe's sizeofShipSpecifics (GenericInterceptorHeader 24, AttackBomberHeader 56); no pointers, same on LP64
_Static_assert(sizeof(AttackBomberSpec) == 0x38, "AttackBomberSpec is 0x38 bytes in cataclysm.exe");

void GenericInterceptorStaticInit(char *directory, char *filename, struct ShipStaticInfo *statinfo);
void GenericInterceptorInit(Ship *ship);
void GenericInterceptorFire(Ship *ship, SpaceObjRotImpTarg *target);
void GenericInterceptorPassiveAttack(Ship *ship, Ship *target, bool32 rotate);
void GenericInterceptorPassiveAttack(Ship *ship, Ship *target, bool32 rotate);


#define FAKE_FLY_BY_DISTANCE_MUCH_BIGGER_THAN_NEEDED   15000.0f

sdword FIGHTER_BREAK_ANGLE_MIN = 25;
sdword FIGHTER_BREAK_ANGLE_MAX = 45;

sdword FIGHTER_BREAK_VERTICAL_ANGLE_MIN = 10;
sdword FIGHTER_BREAK_VERTICAL_ANGLE_MAX = 30;

bool32 BombersUseBombingRun = FALSE;

/*
typedef struct
{
    real32 firetime;
    real32 maxFlyAwayDist[NUM_TACTICS_TYPES][NUM_CLASSES+1];
    real32 maxFlyAwayDistSquared[NUM_TACTICS_TYPES][NUM_CLASSES+1];    // use NUM_CLASSES entry as DefaultClass (so array len = NUM_CLASSES+1)
    real32 breakRange[NUM_TACTICS_TYPES][NUM_CLASSES+1];
    real32 flyPastDist[NUM_TACTICS_TYPES][NUM_CLASSES+1];
    real32 triggerHappy[NUM_TACTICS_TYPES][NUM_CLASSES+1];
    real32 faceTargetAccuracy[NUM_TACTICS_TYPES][NUM_CLASSES+1];
    real32 maxAttackTime[NUM_TACTICS_TYPES][NUM_CLASSES+1];
    FlightManProb flightmanProb[NUM_TACTICS_TYPES][NUM_CLASSES+1];
} GenericInterceptorStatics;
*/
// CAT: no CloakedFighterStatics (HW1 cloak fuel statics; CloakedFighterStaticInit 0x005b36d0 reads no cloak keys)

GenericInterceptorStatics GenericInterceptorStatic;

// CAT: one statics block per ship type, 14 types (GenericInterceptorStaticInit 0x005b1e70 switches on the type and
//      keeps 14 blocks of 0x3640 bytes at 0x00a7e4a0..0x00aaa5e0); HW1 had per-race light/heavy/bomber/cloaked blocks
//      and P1/P2/TargetDrone blocks. Cataclysm has no CloakedFighterStatics: CloakedFighterStaticInit 0x005b36d0 only
//      calls GenericInterceptorStaticInit, so bCloakedFighter gets a plain GenericInterceptorStatics block too.
GenericInterceptorStatics sReconInterceptorStatic;
GenericInterceptorStatics sAcolyteInterceptorStatic;
GenericInterceptorStatics sMimicInterceptorStatic;
GenericInterceptorStatics bReconInterceptorStatic;
GenericInterceptorStatics bAcolyteInterceptorStatic;
GenericInterceptorStatics bInterceptorInterceptorStatic;
GenericInterceptorStatics bCloakedFighterInterceptorStatic;
GenericInterceptorStatics bAttackBomberInterceptorStatic;
GenericInterceptorStatics kAttackBomberInterceptorStatic;
GenericInterceptorStatics kHeavyInterceptorInterceptorStatic;
GenericInterceptorStatics tAttackBomberInterceptorStatic;
GenericInterceptorStatics tHeavyInterceptorInterceptorStatic;
GenericInterceptorStatics p1FighterInterceptorStatic;
GenericInterceptorStatics dAcolyteInterceptorStatic;

scriptStructEntry LIStaticScriptTable[] =
{
    { "gunsFireTime",           scriptSetReal32CB,                   &(GenericInterceptorStatic.firetime),           &(GenericInterceptorStatic) },

    { "flightmanTurnaround",    (void(*)(char *,char *,void*)) scriptSetFlightManTurnaroundCB,      &(GenericInterceptorStatic.flightmanProb),      &(GenericInterceptorStatic) },
    { "flightmanAIP",           (void(*)(char *,char *,void*))scriptSetFlightManAIPCB,             &(GenericInterceptorStatic.flightmanProb),      &(GenericInterceptorStatic) },
    { "flightmanEvasiveBehind", (void(*)(char *,char *,void*))scriptSetFlightManEvasiveBehindCB,   &(GenericInterceptorStatic.flightmanProb),      &(GenericInterceptorStatic) },
    { "flightmanEvasiveFront",  (void(*)(char *,char *,void*))scriptSetFlightManEvasiveFrontCB,    &(GenericInterceptorStatic.flightmanProb),      &(GenericInterceptorStatic) },
    { "flightmanEvasivePure",   (void(*)(char *,char *,void*))scriptSetFlightManEvasivePureCB,     &(GenericInterceptorStatic.flightmanProb),      &(GenericInterceptorStatic) },
    { "maxFlyAwayDist",         (void(*)(char *,char *,void*))scriptSetReal32CB_ARRAY,             &(GenericInterceptorStatic.maxFlyAwayDist),     &(GenericInterceptorStatic) },
    { "breakRange",             (void(*)(char *,char *,void*))scriptSetReal32CB_ARRAY,             &(GenericInterceptorStatic.breakRange),         &(GenericInterceptorStatic) },
    { "flyPastDist",            (void(*)(char *,char *,void*))scriptSetReal32CB_ARRAY,             &(GenericInterceptorStatic.flyPastDist),        &(GenericInterceptorStatic) },
    { "triggerHappy",           (void(*)(char *,char *,void*))scriptSetCosAngCB_ARRAY,             &(GenericInterceptorStatic.triggerHappy),       &(GenericInterceptorStatic) },
    { "faceTargetAccuracy",     (void(*)(char *,char *,void*))scriptSetCosAngCB_ARRAY,             &(GenericInterceptorStatic.faceTargetAccuracy), &(GenericInterceptorStatic) },
    { "maxAttackTime",          (void(*)(char *,char *,void*))scriptSetReal32CB_ARRAY,             &(GenericInterceptorStatic.maxAttackTime),      &(GenericInterceptorStatic) },

    END_SCRIPT_STRUCT_ENTRY
};


void GenericInterceptorStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    GenericInterceptorStatics *interceptorstat = NULL;
    udword i;
    TacticsType t;

    // CAT: the 14 Cataclysm types that use this StaticInit (directly or through CloakedFighterStaticInit,
    //      AcolyteStaticInit, MimicStaticInit), one block each (GenericInterceptorStaticInit 0x005b1e70)
    switch (statinfo->shiptype)
    {
        case sRecon:            interceptorstat = &sReconInterceptorStatic;             break;
        case sAcolyte:          interceptorstat = &sAcolyteInterceptorStatic;           break;
        case sMimic:            interceptorstat = &sMimicInterceptorStatic;             break;
        case bRecon:            interceptorstat = &bReconInterceptorStatic;             break;
        case bAcolyte:          interceptorstat = &bAcolyteInterceptorStatic;           break;
        case bInterceptor:      interceptorstat = &bInterceptorInterceptorStatic;       break;
        case bCloakedFighter:   interceptorstat = &bCloakedFighterInterceptorStatic;    break;
        case bAttackBomber:     interceptorstat = &bAttackBomberInterceptorStatic;      break;
        case kAttackBomber:     interceptorstat = &kAttackBomberInterceptorStatic;      break;
        case kHeavyInterceptor: interceptorstat = &kHeavyInterceptorInterceptorStatic;  break;
        case tAttackBomber:     interceptorstat = &tAttackBomberInterceptorStatic;      break;
        case tHeavyInterceptor: interceptorstat = &tHeavyInterceptorInterceptorStatic;  break;
        case p1Fighter:         interceptorstat = &p1FighterInterceptorStatic;          break;
        case dAcolyte:          interceptorstat = &dAcolyteInterceptorStatic;           break;
        default:
            break;
    }
    dbgAssertOrIgnore(interceptorstat != NULL);

    statinfo->custstatinfo = interceptorstat;

    memset(interceptorstat,0,sizeof(*interceptorstat));

    scriptSetStruct(directory,filename,LIStaticScriptTable,(ubyte *)interceptorstat);

    for(t = Evasive; t < NUM_TACTICS_TYPES; t++)
        for (i = 0; i < NUM_CLASSES + 1; i++)
        {
            interceptorstat->maxFlyAwayDistSquared[t][i] = interceptorstat->maxFlyAwayDist[t][i] * interceptorstat->maxFlyAwayDist[t][i];
        }

    for(t = Evasive; t < NUM_TACTICS_TYPES; t++)
        for (i = 0; i < NUM_CLASSES; i++)
        {
            if (interceptorstat->flyPastDist[t][i] == 0.0f)
            {
                interceptorstat->maxFlyAwayDist[t][i]        = interceptorstat->maxFlyAwayDist[t][NUM_CLASSES];
                interceptorstat->breakRange[t][i]            = interceptorstat->breakRange[t][NUM_CLASSES];
                interceptorstat->flyPastDist[t][i]           = interceptorstat->flyPastDist[t][NUM_CLASSES];
                interceptorstat->triggerHappy[t][i]          = interceptorstat->triggerHappy[t][NUM_CLASSES];
                interceptorstat->faceTargetAccuracy[t][i]    = interceptorstat->faceTargetAccuracy[t][NUM_CLASSES];
                interceptorstat->maxAttackTime[t][i]         = interceptorstat->maxAttackTime[t][NUM_CLASSES];
                interceptorstat->flightmanProb[t][i]         = interceptorstat->flightmanProb[t][NUM_CLASSES];
            }
        }

    // CAT: no TargetDrone bullet-range override (no TargetDrone type; GenericInterceptorStaticInit 0x005b1e70 ends
    //      after the per-class fallback)
}

void GenericInterceptorInit(Ship *ship)
{
    GenericInterceptorSpec *spec = (GenericInterceptorSpec *)ship->ShipSpecifics;

    spec->activeGun = 0;
}

bool32 GenericInterceptorCanFire(Ship *ship,SpaceObjRotImpTarg *target,vector *trajectory,real32 triggerHappy)
{
    GenericInterceptorSpec *spec = (GenericInterceptorSpec *)ship->ShipSpecifics;
    GenericInterceptorStatics *interceptorstat = (GenericInterceptorStatics *)(((ShipStaticInfo *)ship->staticinfo)->custstatinfo);
    Gun *curgun;
    Gun *gun;
    GunInfo *gunInfo = ship->gunInfo;
    sdword i;
    sdword numGuns;
    bool32 returnval = FALSE;
    bool32 canUseGun;
    sdword pickgun;

    // CAT: GenericInterceptorCanFire 0x005b2120. No TargetDrone test and no trigger-happy adjustments (the exe never
    //      reads triggerHappy: fixed guns aim with cat_gunFixedGunAimedAtTarget); a mimicking ship never fires;
    //      cat_gunCanFire in place of gunCanShoot; an Acolyte picks among the first half of its guns; fixed guns fire
    (void)trajectory;
    (void)triggerHappy;

    if (bitTest(ship->catShipFlags, CAT_SHIPFLAG_Mimic) && cat_shipIsMimicking(ship))
    {
        return FALSE;
    }

    if(ship->staticinfo->madStatic != NULL)
    {
        //ship has meshanimations
        if(ship->staticinfo->madStatic->numGunOpenIndexes > 0)
        {
            //has gun opening animations
            if(ship->madGunStatus != MAD_STATUS_GUNS_OPEN &&
                ship->madGunStatus != MAD_STATUS_GUNS_OPENING)
            {
                madOpenGunsShip(ship);
                return FALSE;
            }
            else if(ship->madGunStatus != MAD_STATUS_GUNS_OPEN)
            {
                return FALSE;
            }
            //otherwise, we're open..try to fire away!
        }
    }

    if ((universe.totaltimeelapsed - spec->lasttimefired) < interceptorstat->firetime)
    {
        return FALSE;   // if time since last shot fired is less than the gun firetime, don't shoot.
    }

    numGuns = gunInfo->numGuns;
    if (cat_shipIsAcolyte(ship))
    {
        numGuns = gunInfo->numGuns / 2;
    }

    for (i=0,canUseGun=FALSE,pickgun=spec->activeGun;i<numGuns;i++)
    {
        if (cat_gunCanFire(ship, &gunInfo->guns[pickgun]))
        {
            canUseGun = TRUE;
            break;
        }

        pickgun++;
        if (pickgun>=numGuns)
        {
            pickgun = 0;
        }
    }

    if (canUseGun)
    {
        spec->activeGun = pickgun;
        gun = &gunInfo->guns[spec->activeGun];

        switch (gun->gunstatic->guntype)
        {
            case GUN_Fixed:
                return cat_gunFixedGunAimedAtTarget(ship,gun,target,TRUE);

            case GUN_Gimble:
                return gunOrientGimbleGun(ship,gun,target) ? TRUE : FALSE;

            case GUN_NewGimble:
                // orient all other gimble guns as well
                for (i=0,curgun=&gunInfo->guns[0];i<gunInfo->numGuns;i++,curgun++)
                {
                    if (i == spec->activeGun)
                    {
                        returnval = gunOrientGimbleGun(ship,curgun,target);
                    }
                    else
                    {
                        gunOrientGimbleGun(ship,curgun,target);
                    }
                }
                return returnval;

            default:
                return FALSE;
        }
    }
    else
    {
        return FALSE;
    }
}

void GenericInterceptorFire(Ship *ship,SpaceObjRotImpTarg *target)
{
    GenericInterceptorSpec *spec = (GenericInterceptorSpec *)ship->ShipSpecifics;
    GunInfo *gunInfo = ship->gunInfo;

    spec->lasttimefired = universe.totaltimeelapsed;

    gunShoot(ship,&gunInfo->guns[spec->activeGun],target);

    spec->activeGun++;
    if (spec->activeGun >= gunInfo->numGuns)
    {
        spec->activeGun = 0;
    }
    // CAT: an Acolyte alternates between its first two guns (GenericInterceptorFire 0x005b22c0)
    if (cat_shipIsAcolyte(ship) && (spec->activeGun > 1))
    {
        spec->activeGun = 0;
    }
}

void setupWhipStrafe(Ship *ship,GenericInterceptorSpec *spec,Ship *target)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    vector targetshipvec;
    vector tmpvec2;
    real32 zsqr,rsqr;
    real32 temp;
    uword whipstrafeflags;

    targetshipvec = target->obj.posinfo.position;
    temp = target->staticinfo->common.staticCollInfo.approxcollspheresize + shipstaticinfo->bulletRange[ship->tacticstype] * WHIPSTRAFE_BULLETRANGEOPTIMUM;

    if (ship->obj.posinfo.position.z > targetshipvec.z)
    {
        whipstrafeflags = WHIPSTRAFE_ABOVE;
        targetshipvec.z += temp;
    }
    else
    {
        whipstrafeflags = 0;
        targetshipvec.z -= temp;
    }

    vecSub(tmpvec2,targetshipvec,ship->obj.posinfo.position);

    zsqr = tmpvec2.z * tmpvec2.z;
    rsqr = tmpvec2.x * tmpvec2.x + tmpvec2.y * tmpvec2.y;

    if (zsqr <= rsqr*WHIPSTRAFE_MAXANGCOMPAREFACTOR)
    {
        // angle is appropriatley low, so continue
        vecMultiplyByScalar(tmpvec2,WHIPSTRAFE_FLYBYOVERSHOOT);
        vecAdd(spec->aivec,ship->obj.posinfo.position,tmpvec2);

        spec->aiAIPflightman = TRUE;
        flightmanInit(ship,FLIGHTMAN_WHIP_STRAFE);
        dbgAssertOrIgnore(ship->flightman == FLIGHTMAN_WHIP_STRAFE);
        ship->flightmanState2 = (ubyte) whipstrafeflags;
        ship->aistateattack = STATE_WHIPSTRAFE1;
        spec->aitimekill = universe.totaltimeelapsed;
    }
}

#define TransitionToSTATE_REAPPROACH           \
    ship->aistateattack = STATE_REAPPROACH;                                                                                \
    spec->aiAIPflightman = FALSE;                                                                                          \
    ship->shipOrDerelict.aistate = 0;                                                                                                     \
                                                                                                                           \
    if ((ship->flightman == FLIGHTMAN_NULL) && (target->header.objtype == OBJ_ShipType) && (isCapitalShip((Ship *)target)) && (((ShipStaticInfo *)(target->staticinfo))->shipclass != CLASS_Mothership)) \
    {                                                                                                                      \
        if (flightmanTestRandom(&interceptorstat->flightmanProb[ship->tacticstype][targetIndex],FLIGHTMAN_AIP,FLIGHTMAN_WHIP_STRAFE))         \
        {                                                                                                                  \
            setupWhipStrafe(ship,spec,(Ship *)target);                                                                             \
        }                                                                                                                  \
    }

void GenericInterceptorPassiveAttack(Ship *ship,Ship *target,bool32 rotate)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    GenericInterceptorStatics *interceptorstat = (GenericInterceptorStatics *)shipstaticinfo->custstatinfo;
    uword targetIndex = (uword)((ShipStaticInfo *)target->staticinfo)->shipclass;
    vector trajectory;
    real32 range;
    real32 dist,temp;

    //Commented out for now...was causing crashwith CloakedFighter
    //dbgAssertOrIgnore(shipstaticinfo->custshipheader.CustShipFire == GenericInterceptorFire);

    // CAT: the first gun's current bullet speed (cat_gunGetBulletSpeed), and only when the ship has guns at all
    //      (GenericInterceptorPassiveAttack 0x005b2470)
    if ((shipstaticinfo->gunStaticInfo != NULL) && (ship->gunInfo != NULL))
        aishipGetTrajectoryWithVelPrediction(ship,(SpaceObjRotImpTarg *)target,cat_gunGetBulletSpeed(&ship->gunInfo->guns[0]),&trajectory);
    else
        aishipGetTrajectory(ship,(SpaceObjRotImpTarg *)target,&trajectory);

    dist = fsqrt(vecMagnitudeSquared(trajectory));
    vecDivideByScalar(trajectory,dist,temp);

    range = RangeToTargetGivenDist(ship,(SpaceObjRotImpTarg *)target,dist);

    if ((rotate) & ((bool32)shipstaticinfo->rotateToRetaliate))
    {
        aitrackHeading(ship,&trajectory,FLYSHIP_ATTACKACCURACY);
    }

    if (range < shipstaticinfo->bulletRange[ship->tacticstype])
    {
        if (GenericInterceptorCanFire(ship,(SpaceObjRotImpTarg *)target,&trajectory,interceptorstat->triggerHappy[ship->tacticstype][targetIndex]))
        {
            //GenericInterceptorFire(ship);
            shipstaticinfo->custshipheader.CustShipFire(ship,(SpaceObjRotImpTarg *)target);
        }
    }
    /*
    //check to see if we should back up!
    if(ship->tacticstype == Evasive)
    {
        command = getShipAndItsCommand(&universe.mainCommandLayer,ship);
        if(command != NULL)
        {
            if(command->ordertype.order == COMMAND_NULL)
            {
                //command group is doint nothing but passive attacking
                if(command->ordertype.attributes & COMMAND_MASK_FORMATION)
                {
                    if(command->selection->ShipPtr[0] != ship)
                        return; //not leader..so return! so we only move leader
                }
                if(command->ordertype.attributes & COMMAND_MASK_HOLDING_PATTERN ||
                   command->ordertype.attributes & COMMAND_MASK_PROTECTING)
                {
                    return;  //if doing either of these things...we don't want to back up
                }
            }
            else
            {
                return; //if doing something else, we have to return
            }
        }
        vecScalarMultiply(trajectory,trajectory,-5000.0f);
        aishipFlyToPointAvoidingObjs(ship,&trajectory,AISHIP_FastAsPossible,0.0f);
    }
    */
}

FlightManProb *GenericInterceptorGetFlightManProb(Ship *ship,SpaceObjRotImpTarg *target)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    uword targetIndex;

    if ((target != NULL) && (target->header.objtype == OBJ_ShipType))
    {
        targetIndex = (uword)((ShipStaticInfo *)target->staticinfo)->shipclass;
    }
    else
    {
        targetIndex = (uword)NUM_CLASSES;
    }

    // CAT: no DefenseFighter case (GenericInterceptorGetFlightManProb 0x005b25c0)
    return &((GenericInterceptorStatics *)shipstaticinfo->custstatinfo)->flightmanProb[ship->tacticstype][targetIndex];
}

void CalcRallyPoint(vector s, vector t, AttackBomberSpec *diveinfo)
{
    s.z = t.z;

    vecSub(diveinfo->rallypoint, s, t);
    vecNormalize(&diveinfo->rallypoint);
    vecMultiplyByScalar(diveinfo->rallypoint, 700.0f);

    if (t.z < 0.0f)
    {
        diveinfo->rallypoint.z += 1000.0f;
    }
    else
    {
        diveinfo->rallypoint.z -= 1000.0f;
    }

    diveinfo->angle = -1.0f;
}

void GenericInterceptorAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    GenericInterceptorSpec *spec = (GenericInterceptorSpec *)ship->ShipSpecifics;
    vector trajectory;
    //real32 tempreal;
    real32 dist;
    real32 range;
    real32 temp;
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    GenericInterceptorStatics *interceptorstat = (GenericInterceptorStatics *)shipstaticinfo->custstatinfo;
    uword targetIndex;
    udword flighttodo;
    vector tmpvec;
    matrix tmpmat;
    sdword randeg;
    real32 randegf;
    udword flags;
    sdword curdistonly;
    vector rallypoint,dest;
    AttackBomberSpec *diveinfo;
    real32 sintheta, costheta;
    TypeOfFormation formtype = NO_FORMATION;

    if (target->header.objtype == OBJ_ShipType)
    {
        targetIndex = (uword)((ShipStaticInfo *)target->staticinfo)->shipclass;
    }
    else
    {
        targetIndex = (uword)NUM_CLASSES;
    }

    switch (ship->aistateattack)
    {
        case STATE_INIT:
            spec->aiAIPflightman = FALSE;
            // CAT: only kAttackBomber/tAttackBomber may start a bombing run, and never at a ship target (HW1: never at
            //      the R1 mothership) (GenericInterceptorAttack 0x005b2620)
            if(((ship->shiptype != kAttackBomber) && (ship->shiptype != tAttackBomber)) || (!BombersUseBombingRun) ||
               (targetIndex > CLASS_Frigate) || (target->header.objtype == OBJ_ShipType))
            {
                ship->aistateattack = STATE_APPROACH;
                aishipGetTrajectory(ship,target,&trajectory);
                aishipFlyToShipAvoidingObjs(ship,target,AISHIP_PointInDirectionFlying + AISHIP_CarTurn,0.0f);
                range = RangeToTarget(ship,target,&trajectory);

                if (range < shipstaticinfo->bulletRange[ship->tacticstype])
                {
                    ship->aistateattack = STATE_KILL;
                    spec->aitimekill = universe.totaltimeelapsed;
                }
                break;
            }
            ship->aistateattack = STATE_START_BOMBING_RUN;
            CalcRallyPoint(ship->obj.posinfo.position, target->obj.posinfo.position, (AttackBomberSpec *)ship->ShipSpecifics);

            // fall right into bombing run code

        case STATE_START_BOMBING_RUN:
            diveinfo = (AttackBomberSpec *)ship->ShipSpecifics;
            if(diveinfo->angle == -1.0f)
            {
                memcpy(&diveinfo->orig_point, &diveinfo->rallypoint, sizeof(vector));
                diveinfo->angle = DEG_TO_RAD(45);
                sintheta = sin(diveinfo->angle);
                costheta = cos(diveinfo->angle);
                diveinfo->rallypoint.x = (diveinfo->orig_point.x * costheta) + (diveinfo->orig_point.y * sintheta);   // rotate rallypoint X degrees clockwise
                diveinfo->rallypoint.y = (diveinfo->orig_point.y * costheta) - (diveinfo->orig_point.x * sintheta);
            }
            vecAdd(rallypoint, target->obj.posinfo.position, diveinfo->rallypoint);
            vecSub(tmpvec, ship->obj.posinfo.position, rallypoint);
            if(vecMagnitudeSquared(tmpvec) < 5625)  // within 75 metres
            {
                if((gamerand() & 15) == 0)      // 1 in 16 chance of starting bombing dive
                {
                    ship->aistateattack = STATE_BOMBING_DIVE;
                    diveinfo->rallypoint.x = -(diveinfo->rallypoint.x);
                    diveinfo->rallypoint.y = -(diveinfo->rallypoint.y);
                    diveinfo->rallypoint.z = -200.0f;
                    diveinfo->bombingdelay = 15;
                }
                else
                {
                    diveinfo->angle += DEG_TO_RAD(10);
                    if(diveinfo->angle >= DEG_TO_RAD(360))
                        diveinfo->angle -= DEG_TO_RAD(360);
                    sintheta = sin(diveinfo->angle);
                    costheta = cos(diveinfo->angle);
                    diveinfo->rallypoint.x = (diveinfo->orig_point.x * costheta) + (diveinfo->orig_point.y * sintheta);   // rotate rallypoint X degrees clockwise
                    diveinfo->rallypoint.y = (diveinfo->orig_point.y * costheta) - (diveinfo->orig_point.x * sintheta);
                }
            }
            else
                aishipFlyToPointAvoidingObjsWithVel(ship, &rallypoint, AISHIP_PointInDirectionFlying | AISHIP_DontFlyToObscuredPoints | AISHIP_FastAsPossible, -400.0f, &target->obj.posinfo.velocity);

            break;

        case STATE_BOMBING_DIVE:
            diveinfo = (AttackBomberSpec *)ship->ShipSpecifics;
            vecAdd(rallypoint, target->obj.posinfo.position, diveinfo->rallypoint);
            vecSub(tmpvec, ship->obj.posinfo.position, rallypoint);
            if(vecMagnitudeSquared(tmpvec) < 5625)  // within 75 metres
                ship->aistateattack = STATE_INIT;
            else
                aishipFlyToPointAvoidingObjsWithVel(ship, &rallypoint, AISHIP_PointInDirectionFlying | AISHIP_DontFlyToObscuredPoints | AISHIP_FastAsPossible, -400.0f, &target->obj.posinfo.velocity);

            if(diveinfo->bombingdelay)
                diveinfo->bombingdelay--;
            else if (GenericInterceptorCanFire(ship,target,&trajectory,interceptorstat->triggerHappy[ship->tacticstype][targetIndex]))
                shipstaticinfo->custshipheader.CustShipFire(ship,target);
            break;

        case STATE_APPROACH:
#if DEBUG_INTERCEPTOR
            dbgMessagef("Ship %x STATE_APPROACH",(udword)ship);
#endif
            aishipGetTrajectory(ship,target,&trajectory);
            aishipFlyToShipAvoidingObjs(ship,target,AISHIP_PointInDirectionFlying + AISHIP_CarTurn,0.0f);
            range = RangeToTarget(ship,target,&trajectory);

            if (range < shipstaticinfo->bulletRange[ship->tacticstype])
            {
                ship->aistateattack = STATE_KILL;
                spec->aitimekill = universe.totaltimeelapsed;
            }
            break;

        case STATE_TURNAROUND:
#if DEBUG_INTERCEPTOR
            dbgMessagef("Ship %x STATE_TURNAROUND",(udword)ship);
#endif
            if (flightmanExecute(ship))
            {
                ship->tacticsTalk = 0;          //reset var
                TransitionToSTATE_REAPPROACH
            }
            break;

        case STATE_REAPPROACH:
#if DEBUG_INTERCEPTOR
            dbgMessagef("Ship %x STATE_REAPPROACH",(udword)ship);
#endif
            aishipGetTrajectory(ship,target,&trajectory);
            aishipFlyToShipAvoidingObjs(ship,target,AISHIP_PointInDirectionFlying,0.0f);
            range = RangeToTarget(ship,target,&trajectory);

            if (range < shipstaticinfo->bulletRange[ship->tacticstype])
            {
                ship->aistateattack = STATE_KILL;
                spec->aitimekill = universe.totaltimeelapsed;
            }
            break;

        case STATE_WHIPSTRAFE1:
#if DEBUG_INTERCEPTOR
            dbgMessagef("Ship %x STATE_WHIPSTRAFE1",(udword)ship);
#endif
//            dbgAssertOrIgnore(ship->flightman == FLIGHTMAN_WHIP_STRAFE);
            if (ship->flightman != FLIGHTMAN_WHIP_STRAFE)
            {
                dbgMessagef("Warning: Whip strafe cancelled");
                ship->aistateattack = STATE_INIT;
                break;
            }

            aishipGetTrajectoryWithAngleCorrection(ship,target,&trajectory);

            dist = fsqrt(vecMagnitudeSquared(trajectory));
            vecDivideByScalar(trajectory,dist,temp);

            range = RangeToTargetGivenDist(ship,target,dist);

            if (range < (shipstaticinfo->bulletRange[ship->tacticstype]*0.7f))
            {
                aishipFlyToPointAvoidingObjs(ship,&spec->aivec,AISHIP_FastAsPossible,WHIPSTRAFE_MINVELOCITY);
                aitrackHeadingWithBank(ship,&trajectory,0.99999f /*interceptorstat->faceTargetAccuracy[targetIndex]*/,shipstaticinfo->shipOrDerelict.sinbank);

                if (GenericInterceptorCanFire(ship,target,&trajectory,interceptorstat->triggerHappy[ship->tacticstype][targetIndex]))
                {
                    //GenericInterceptorFire(ship);
                    shipstaticinfo->custshipheader.CustShipFire(ship,target);
                }
            }
            else
            {
                aishipFlyToPointAvoidingObjs(ship,&spec->aivec,AISHIP_FastAsPossible | AISHIP_PointInDirectionFlying,WHIPSTRAFE_MINVELOCITY);
            }

            if ((MoveReachedDestinationVariable(ship,&spec->aivec,WHIPSTRAFE_REACHDESTINATIONRANGE) || ((universe.totaltimeelapsed - spec->aitimekill) > interceptorstat->maxAttackTime[ship->tacticstype][targetIndex])))
            {
                ship->aistateattack = STATE_WHIPSTRAFE2;
            }
            break;

        case STATE_WHIPSTRAFE2:
#if DEBUG_INTERCEPTOR
            dbgMessagef("Ship %x STATE_WHIPSTRAFE2",(udword)ship);
#endif
//            dbgAssertOrIgnore(ship->flightman == FLIGHTMAN_WHIP_STRAFE);
            if (ship->flightman != FLIGHTMAN_WHIP_STRAFE)
            {
                dbgMessagef("Warning: Whip strafe cancelled");
                ship->aistateattack = STATE_INIT;
                break;
            }

            if (flightmanExecute(ship))
            {
                ship->aistateattack = STATE_BREAK;

                vecSub(tmpvec,ship->obj.posinfo.position,target->obj.posinfo.position);
                tmpvec.z = 0.0f;
                vecNormalizeToLength(&tmpvec,interceptorstat->flyPastDist[ship->tacticstype][targetIndex]);

                randeg = randombetween(FIGHTER_BREAK_ANGLE_MIN,FIGHTER_BREAK_ANGLE_MAX);
                randegf = (real32) ((randeg & 1) ? randeg : -randeg);
                randegf = DEG_TO_RAD(randegf);

                matMakeRotAboutZ(&tmpmat,(real32)cos(randegf),(real32)sin(randegf));
                matMultiplyMatByVec(&spec->aivec,&tmpmat,&tmpvec);

                vecAddTo(spec->aivec,target->obj.posinfo.position);
            }
            break;

        case STATE_KILL:
#if DEBUG_INTERCEPTOR
            dbgMessagef("Ship %x STATE_KILL",(udword)ship);
#endif
            // CAT: the first gun's current bullet speed, and only when the ship has guns (0x005b2620)
            if ((shipstaticinfo->gunStaticInfo != NULL) && (ship->gunInfo != NULL))
                aishipGetTrajectoryWithVelPrediction(ship,target,cat_gunGetBulletSpeed(&ship->gunInfo->guns[0]),&trajectory);
            else
                aishipGetTrajectory(ship,target,&trajectory);

            dist = fsqrt(vecMagnitudeSquared(trajectory));
            vecDivideByScalar(trajectory,dist,temp);

            if (vecDotProduct(trajectory,target->obj.posinfo.velocity) < 0)     // never track a velocity of ship moving towards us,
            {                                                               // because we don't want to back up or slow down.
                aishipFlyToShipAvoidingObjs(ship,target,AISHIP_FastAsPossible,-shipstaticinfo->common.maxvelocity);
            }
            else
            {
                aishipFlyToShipAvoidingObjsWithVel(ship,target,AISHIP_FastAsPossible,-shipstaticinfo->common.maxvelocity,&target->obj.posinfo.velocity);
            }

            if (ship->flightman == FLIGHTMAN_BARREL_ROLL || ship->flightman == FLIGHTMAN_SWARMER_BARRELROLL || ship->flightman == FLIGHTMAN_ROLL180)
            {
                flags = AITRACKHEADING_DONTROLL;
                if(flightmanExecute(ship))
                    ship->tacticsTalk = 0;  //reset variable
            }
            else
            {
                flags = 0;
                if(ship->flightman == FLIGHTMAN_CELEB_FLIP || ship->flightman == FLIGHTMAN_BARRELROLL_OUT)
                {
                    flightmanExecute(ship);
                    break;
                }
            }

            aitrackHeadingWithBankFlags(ship,&trajectory,interceptorstat->faceTargetAccuracy[ship->tacticstype][targetIndex],shipstaticinfo->shipOrDerelict.sinbank,flags);

            range = RangeToTargetGivenDist(ship,target,dist);

            if (range < shipstaticinfo->bulletRange[ship->tacticstype])
            {
                if (GenericInterceptorCanFire(ship,target,&trajectory,interceptorstat->triggerHappy[ship->tacticstype][targetIndex]))
                {
                    //GenericInterceptorFire(ship);
                    shipstaticinfo->custshipheader.CustShipFire(ship,target);

                    if ((ship->flightman == FLIGHTMAN_NULL) && (!spec->aiAIPflightman))
                    {
                        spec->aiAIPflightman = TRUE;  // only try to do barrel roll once, don't keep repeating random check
                        if (flightmanTestRandom(&interceptorstat->flightmanProb[ship->tacticstype][targetIndex],FLIGHTMAN_AIP,FLIGHTMAN_BARREL_ROLL))
                        {
                            bitSet(ship->tacticsTalk,TACTALK_BARRELROLL);
                            flightmanInit(ship,FLIGHTMAN_BARREL_ROLL);
                        }
                        else if (flightmanTestRandom(&interceptorstat->flightmanProb[ship->tacticstype][targetIndex],FLIGHTMAN_AIP,FLIGHTMAN_CELEB_FLIP))
                        {
                            flightmanInit(ship,FLIGHTMAN_CELEB_FLIP);
                        }
                        else if (flightmanTestRandom(&interceptorstat->flightmanProb[ship->tacticstype][targetIndex],FLIGHTMAN_AIP,FLIGHTMAN_SWARMER_BARRELROLL))
                        {
                            flightmanInit(ship,FLIGHTMAN_SWARMER_BARRELROLL);
                        }
                        else if (flightmanTestRandom(&interceptorstat->flightmanProb[ship->tacticstype][targetIndex],FLIGHTMAN_AIP,FLIGHTMAN_ROLL180))
                        {
                            flightmanInit(ship,FLIGHTMAN_ROLL180);
                        }
                    }
                }

                if ((range < interceptorstat->breakRange[ship->tacticstype][targetIndex])) // || ((universe.totaltimeelapsed - spec->aitimekill) > interceptorstat->maxAttackTime[ship->tacticstype][targetIndex]))
                {
                    randeg = randombetween(FIGHTER_BREAK_ANGLE_MIN,FIGHTER_BREAK_ANGLE_MAX);
                    if (flightmanTestRandom(&interceptorstat->flightmanProb[ship->tacticstype][targetIndex],FLIGHTMAN_AIP,FLIGHTMAN_BARRELROLL_OUT))
                    {
                        ship->aistateattack = STATE_BARRELROLL_OUT;
                        flightmanInit(ship,FLIGHTMAN_BARRELROLL_OUT);
                        randegf = (real32) randeg;  // always break left during barrelroll out
                    }
                    else
                    {
                        ship->aistateattack = STATE_BREAK;
                        ship->tacticsTalk = 0;  //reset variable

                        if (targetIndex == CLASS_Fighter)
                            randegf = (real32) randeg;  // always break left for fighters so they avoid each other better
                        else
                            randegf = (real32) ((randeg & 1) ? randeg : -randeg);
                    }
                    randegf = DEG_TO_RAD(randegf);

                    // CAT: the formation type of the ship's own command when it is a formation command (HW1: the
                    //      ship's formationcommand), and no R1-mothership exception (0x005b2620)
                    if ((ship->command != NULL) && (ship->command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
                        (ship->tacticstype != Evasive))
                        formtype = ship->command->formation.formationtype;
                    else
                        formtype = NO_FORMATION;

                    if ( (ABS(trajectory.z) >= 0.75) )
                    {
                        //vecScalarMultiply(tmpvec,trajectory,interceptorstat->flyPastDist[ship->tacticstype][targetIndex]);
                        vecScalarMultiply(tmpvec,trajectory,FAKE_FLY_BY_DISTANCE_MUCH_BIGGER_THAN_NEEDED);
                        matMakeRotAboutY(&tmpmat,(real32)cos(randegf),(real32)sin(randegf));
                        matMultiplyMatByVec(&spec->aivec,&tmpmat,&tmpvec);
                    }
                    else if ( (formtype == DELTA_FORMATION) || (formtype == BROAD_FORMATION) )
                    {
                        randeg = randombetween(FIGHTER_BREAK_VERTICAL_ANGLE_MIN,FIGHTER_BREAK_VERTICAL_ANGLE_MAX);
                        randegf = (real32) randeg;
                        randegf = DEG_TO_RAD(randegf);
                        tmpvec.x = trajectory.x;
                        tmpvec.y = trajectory.y;
                        tmpvec.z = 0.0f;
                        //vecNormalizeToLength(&tmpvec,interceptorstat->flyPastDist[ship->tacticstype][targetIndex]);
                        vecNormalizeToLength(&tmpvec,FAKE_FLY_BY_DISTANCE_MUCH_BIGGER_THAN_NEEDED);

                        if (ABS(trajectory.x) > ABS(trajectory.y))
                        {
                            matMakeRotAboutY(&tmpmat,(real32)cos(randegf),(real32)sin(randegf));
                        }
                        else
                        {
                            matMakeRotAboutX(&tmpmat,(real32)cos(randegf),(real32)sin(randegf));
                        }
                        matMultiplyMatByVec(&spec->aivec,&tmpmat,&tmpvec);
                        if (trajectory.z > 0.0f)
                        {
                            // attacking from below, so stay below
                            if ((spec->aivec.z) > 0.0f) spec->aivec.z = -spec->aivec.z;
                        }
                        else
                        {
                            // attacking from above, so stay above
                            if ((spec->aivec.z) < 0.0f) spec->aivec.z = -spec->aivec.z;
                        }
                    }
                    else
                    {
                        //vecScalarMultiply(tmpvec,trajectory,interceptorstat->flyPastDist[ship->tacticstype][targetIndex]);
                        vecScalarMultiply(tmpvec,trajectory,FAKE_FLY_BY_DISTANCE_MUCH_BIGGER_THAN_NEEDED);
                        matMakeRotAboutZ(&tmpmat,(real32)cos(randegf),(real32)sin(randegf));
                        matMultiplyMatByVec(&spec->aivec,&tmpmat,&tmpvec);
                    }


#ifdef DEBUG_TACTICS
                    if(tacticsOn)
#endif
                    {
                        //slowly reduce verticle component so as to organize
                        //things and reduce deadlyness of verticle attacks
                        //tempreal = vecMagnitudeSquared(spec->aivec);
                        //tempreal = fsqrt(tempreal);
                        //spec->aivec.x *= tacticsInfo.InterceptorVerticalMultiplier;
                        //vecNormalize(&spec->aivec);
                        //vecScalarMultiply(spec->aivec,spec->aivec,tempreal);
                    }

                    vecAddTo(spec->aivec,target->obj.posinfo.position);
                }
            }
            break;

        case STATE_BARRELROLL_OUT:
            if(flightmanExecute(ship))
                ship->aistateattack = STATE_BREAK;
            break;

        case STATE_BREAK:
#if DEBUG_INTERCEPTOR
            dbgMessagef("Ship %x STATE_BREAK",(udword)ship);
#endif
            aishipGetTrajectory(ship,target,&trajectory);
            dist = vecMagnitudeSquared(trajectory);

            curdistonly = TRUE;
            if(target->header.objtype == OBJ_ShipType)
            {
                if( ((ShipStaticInfo *)target->staticinfo)->shipclass <= CLASS_Frigate)
                {
                    //ship is a frigate..so lets base our fly away distance on current distance ONLY!
                    curdistonly = FALSE;
                }
            }
            if ((dist > interceptorstat->maxFlyAwayDistSquared[ship->tacticstype][targetIndex]) || ((MoveReachedDestinationVariable(ship,&spec->aivec,INTERCEPTORBREAK_TOLERANCE) && curdistonly)) )
            {
abortbreakafterall:
                matGetVectFromMatrixCol3(tmpvec,ship->rotatable.rotinfo.coordsys);
                if (vecDotProduct(trajectory,tmpvec) > 0.0f)
                {
                    flighttodo = FLIGHTMAN_DONOTHING;
                }
                else
                {
                    flighttodo = flightmanGetRandom(&interceptorstat->flightmanProb[ship->tacticstype][targetIndex],FLIGHTMAN_TURNAROUND);
                    bitSet(ship->tacticsTalk,TACTALK_FLIPTURN);
                }
                //flightmanInit(ship,flighttodo);
                flightmanInitFunc(ship,flighttodo,0);
                ship->aistateattack = STATE_TURNAROUND;
            }
            else
            {
                if(!curdistonly)
				{
					vecSub(dest,spec->aivec,target->obj.posinfo.position);
					vecNormalize(&dest);
					vecScalarMultiply(dest,dest,20000.0f);
					vecAddTo(dest,target->obj.posinfo.position);
					if(aishipFlyToPointAvoidingObjs(ship,&dest,AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured|AISHIP_FastAsPossible|AISHIP_PointInDirectionFlying,INTERCEPTORBREAK_MINVELOCITY) & AISHIP_FLY_OBJECT_IN_WAY)
					{
						goto abortbreakafterall;
					}
				}
				else if (aishipFlyToPointAvoidingObjs(ship,&spec->aivec,AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured|AISHIP_FastAsPossible|AISHIP_PointInDirectionFlying,INTERCEPTORBREAK_MINVELOCITY) & AISHIP_FLY_OBJECT_IN_WAY)
                {
                    goto abortbreakafterall;
                }
            }
            break;

        default:
            // CAT: no assertion (0x005b2620 returns)
            break;
    }

}

void CloakedFighterStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    // CAT: only the interceptor statics (CloakedFighterStaticInit 0x005b36d0); no cloak fuel keys, the cloak is
    //      Cataclysm's charge-based cloak (Cloak.c, cat_cloakInit/cat_cloakUpdate)
    GenericInterceptorStaticInit(directory,filename,statinfo);
}


void CloakedFighterInit(Ship *ship)
{
    CloakedFighterSpec *spec = (CloakedFighterSpec *)ship->ShipSpecifics;

    // CAT: Cataclysm's cloak with drain and charge rate 0.1 (CloakedFighterInit 0x005b36f0); the exe inlines
    //      GenericInterceptorInit (activeGun = 0)
    spec->activeGun = 0;
    cat_cloakInit(ship, &spec->cloak, 0.1f, 0.1f);
}


/*-----------------------------------------------------------------------------
    Name        : SpawnCloakingEffect
    Description : Create an effect for cloaking or decloaking
    Inputs      : ship - ship that is (de)cloaking
                  etgLOD - effect LOD
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void SpawnCloakingEffect(Ship *ship, etglod *etgLOD)
{
    etgeffectstatic *stat;
    sdword LOD;

    if (etgLOD != NULL)
    {
        LOD = ship->obj.currentLOD;
        if (LOD >= etgLOD->nLevels)
        {
            stat = NULL;
        }
        else
        {
            stat = etgLOD->level[LOD];
        }
    }
    else
    {
        stat = NULL;
    }
#if ETG_DISABLEABLE
    if (stat != NULL && etgEffectsEnabled)
#else
    if (stat != NULL)
#endif
    {
        etgEffectCreate(stat, ship, NULL, NULL, NULL, ship->magnitudeSquared, EAF_Full, 3,
            ship->shiprace, ship->shiptype, ship->staticinfo->common.staticCollInfo.collspheresize);
    }
}

bool32 CloakedFighterSpecialActivate(Ship *ship)
{
    // CAT: toggles Cataclysm's cloak (CloakedFighterSpecialActivate 0x005b3720); no fuel test, effect or chatter here
    return cat_cloakActivate(ship);
}

void CloakedFighterFire(Ship *ship,SpaceObjRotImpTarg *target)
{
    // CAT: a fully cloaked fighter does not fire (it does not decloak to fire either); otherwise the interceptor
    //      fire, which the exe inlines with its Acolyte gun wrap (CloakedFighterFire 0x005b37b0)
    if (!cat_shipIsCloakedFully(ship))
    {
        GenericInterceptorFire(ship,target);
    }
}

void CloakedFighterHouseKeep(Ship *ship)
{
    // CAT: Cataclysm's cloak update (CloakedFighterHouseKeep 0x005b3840)
    cat_cloakUpdate(ship);
}

void CloakedFighterAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    // CAT: new slot-21 body (CloakedFighterAttack 0x005b3730): attack like an interceptor while visible; a fully
    //      cloaked fighter that is told to attack decloaks first (the exe returns 1/0, which no caller reads)
    if (!cat_shipIsCloakedFully(ship))
    {
        GenericInterceptorAttack(ship,target,maxdist);
        return;
    }
    cat_cloakActivate(ship);
}

void CloakedFighterAttackPassive(Ship *ship,Ship *target,bool32 rotate)
{
    // CAT: new slot-6 body (CloakedFighterAttackPassive 0x005b3770), the passive-attack twin of CloakedFighterAttack
    if (!cat_shipIsCloakedFully(ship))
    {
        GenericInterceptorPassiveAttack(ship,target,rotate);
        return;
    }
    cat_cloakActivate(ship);
}

bool32 InterceptorInRange(Ship *ship,SpaceObjRotImpTarg *target)
{
    GenericInterceptorStatics *interceptorstat = (GenericInterceptorStatics *)ship->staticinfo->custstatinfo;
    real32 range;
    uword targetIndex;
    vector trajectory;
    aishipGetTrajectory(ship,target,&trajectory);

    range = RangeToTarget(ship,target,&trajectory);

    if (target->header.objtype == OBJ_ShipType)
    {
        targetIndex = (uword)((ShipStaticInfo *)target->staticinfo)->shipclass;
    }
    else
    {
        targetIndex = (uword)NUM_CLASSES;
    }


    if ((range < interceptorstat->breakRange[ship->tacticstype][targetIndex]))
        return FALSE;
    //if((range > ship->staticinfo->bulletRange))
    //    return FALSE;
    return TRUE;
}

bool32 GenericInterceptorSpecialActivate(Ship *ship)
{
    // CAT: no speed burst (HW1 gave one to LightInterceptor, a type Cataclysm dropped); the exe's slot 7 of the
    //      interceptor and attack bomber headers is a bare "return 1", which the linker folded with kasfSpecialOff
    //      0x00497db0 (GenericInterceptorHeader 0x008c4448, AttackBomberHeader 0x008c4568)
    (void)ship;
    return TRUE;
}

// CAT: the exe's header contents (shipHeaders 0x008b1938; original/scratch-m2/m2r2-shipstatic-tools/headers.txt)
CustShipHeader GenericInterceptorHeader =       // 0x008c4448: sRecon, bRecon, bInterceptor, kHeavyInterceptor, tHeavyInterceptor, p1Fighter
{
    .sizeofShipSpecifics = sizeof(GenericInterceptorSpec),
    .CustShipStaticInit = GenericInterceptorStaticInit,
    .CustShipInit = GenericInterceptorInit,
    .CustShipFire = GenericInterceptorFire,
    .CustShipAttackPassive = GenericInterceptorPassiveAttack,
    .CustShipSpecialActivate = GenericInterceptorSpecialActivate,
    .CustShipAttack = GenericInterceptorAttack,
};

CustShipHeader CloakedFighterHeader =           // 0x008c44d8: bCloakedFighter
{
    .sizeofShipSpecifics = sizeof(CloakedFighterSpec),
    .CustShipStaticInit = CloakedFighterStaticInit,
    .CustShipInit = CloakedFighterInit,
    .CustShipFire = CloakedFighterFire,
    .CustShipAttackPassive = CloakedFighterAttackPassive,   // CAT: was GenericInterceptorPassiveAttack
    .CustShipSpecialActivate = CloakedFighterSpecialActivate,
    .CustShipHousekeep = CloakedFighterHouseKeep,
    .CustShipAttack = CloakedFighterAttack,                 // CAT: was GenericInterceptorAttack
};

CustShipHeader AttackBomberHeader =             // 0x008c4568: bAttackBomber, kAttackBomber, tAttackBomber
{
    .sizeofShipSpecifics = sizeof(AttackBomberSpec),
    .CustShipStaticInit = GenericInterceptorStaticInit,
    .CustShipInit = GenericInterceptorInit,
    .CustShipFire = GenericInterceptorFire,
    .CustShipAttackPassive = GenericInterceptorPassiveAttack,
    .CustShipSpecialActivate = GenericInterceptorSpecialActivate,
    .CustShipAttack = GenericInterceptorAttack,
};

// CAT: no TargetDroneHeader (no TargetDrone type in Cataclysm; HW1LEGACY id, never in shipHeaders)
