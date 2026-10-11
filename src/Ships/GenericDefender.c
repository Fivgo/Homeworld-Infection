// =============================================================================
//  GenericDefender.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "GenericDefender.h"

#include "AIShip.h"
#include "AITrack.h"
#include "Battle.h"
#include "Collision.h"
#include "Debug.h"
#include "DefaultShip.h"
#include "FastMath.h"
#include "GenericInterceptor.h"
#include "Gun.h"
#include "MinelayerCorvette.h"
#include "Physics.h"
#include "ShipSelect.h"
#include "SoundEvent.h"

#define DEBUG_DEFENDER 0

//local prototypes
bool32 kamikazeAttack(Ship *ship,SelectAnyCommand *targets);

typedef struct
{
    udword dummy;
} GenericDefenderSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(GenericDefenderSpec) == 4, "GenericDefenderSpec is 4 bytes in cataclysm.exe (GenericDefenderHeader sizeofShipSpecifics)");   // CAT
#endif

typedef struct
{
    real32 gunRange[NUM_TACTICS_TYPES];
    real32 tooCloseRange[NUM_TACTICS_TYPES];
    real32 CIRCLE_RIGHT_VELOCITY;
    real32 CIRCLE_RIGHT_THRUST;
} GenericDefenderStatics;

// CAT: one block per type: kHeavyDefender (0x00aadc40) and tHeavyDefender (0x00aadc20)
//      (GenericDefenderStaticInit 0x005b1420); HW1 had light/heavy x R1/R2
GenericDefenderStatics kHeavyDefenderStatic;
GenericDefenderStatics tHeavyDefenderStatic;
GenericDefenderStatics GenericDefenderStaticOther;  // CAT: the exe writes into the statinfo itself for any other type

scriptStructEntry DefenderStaticScriptTable[] =
{
    { "CIRCLE_RIGHT_VELOCITY",    scriptSetReal32CB,  &(kHeavyDefenderStatic.CIRCLE_RIGHT_VELOCITY),  &(kHeavyDefenderStatic) },
    { "CIRCLE_RIGHT_THRUST",      scriptSetReal32CB,  &(kHeavyDefenderStatic.CIRCLE_RIGHT_THRUST),    &(kHeavyDefenderStatic) },

    END_SCRIPT_STRUCT_ENTRY
};

void GenericDefenderStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    udword i;
    GenericDefenderStatics *defenderstat = NULL;

    // CAT: by Cataclysm type (GenericDefenderStaticInit 0x005b1420); no LightDefender
    switch (statinfo->shiptype)
    {
        case kHeavyDefender:
           defenderstat = &kHeavyDefenderStatic;
           break;
        case tHeavyDefender:
           defenderstat = &tHeavyDefenderStatic;
           break;
        default:
            dbgAssertOrIgnore(FALSE);
            defenderstat = &GenericDefenderStaticOther;
            break;
    }

    statinfo->custstatinfo = defenderstat;

    for(i=0;i<NUM_TACTICS_TYPES;i++)
    {
        defenderstat->gunRange[i] = statinfo->bulletRange[i];
        defenderstat->tooCloseRange[i] = statinfo->minBulletRange[i] * 0.9f;
    }

    defenderstat->CIRCLE_RIGHT_VELOCITY = 20.0f;
    defenderstat->CIRCLE_RIGHT_THRUST = 0.1f;

    scriptSetStruct(directory,filename,DefenderStaticScriptTable,(ubyte *)defenderstat);
}

#define DEFENDER_APPROACH        1
#define DEFENDER_CIRCLE          2

void GenericDefenderAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    GenericDefenderStatics *defenderstat = (GenericDefenderStatics *)shipstaticinfo->custstatinfo;
    real32 newrange;
    //newrange = maxdist - defenderstat->gunRange;
    //newrange = defenderstat->gunRange - newrange;
    //newrange = defenderstat->gunRange - maxdist + defenderstat->gunRange;
    //could do check if maxdist == gunRange, then don't do calcs, just use
    //max dist

    vector trajectory;
    real32 dist;
    real32 range;
    real32 temp;
    real32 rightvelocity;
    vector right;

    newrange = (defenderstat->gunRange[ship->tacticstype]*2) - maxdist;
    if(newrange < 0.0f)
        newrange = defenderstat->tooCloseRange[ship->tacticstype] + 100.0f;
    else if(newrange > defenderstat->gunRange[ship->tacticstype])
    {
        newrange = defenderstat->gunRange[ship->tacticstype];
    }

    switch (ship->aistateattack)
    {
        case 0:
            ship->aistateattack = DEFENDER_APPROACH;
            // fall through to DEFENDER_APPROACH

        case DEFENDER_APPROACH:
            aishipGetTrajectory(ship,target,&trajectory);
            dist = fsqrt(vecMagnitudeSquared(trajectory));
            range = RangeToTargetGivenDist(ship,target,dist);
            if (range > newrange)
            {
                aishipFlyToShipAvoidingObjsWithVel(ship,target,AISHIP_FastAsPossible|AISHIP_PointInDirectionFlying,0.0f,&target->obj.posinfo.velocity);
                break;
            }
#if DEBUG_DEFENDER
            dbgMessagef("%x Changing to circle",ship);
#endif
            ship->aistateattack = DEFENDER_CIRCLE;
            goto circleAlreadyCalculatedTrajectory;

        case DEFENDER_CIRCLE:
            aishipGetTrajectory(ship,target,&trajectory);
            dist = fsqrt(vecMagnitudeSquared(trajectory));
            range = RangeToTargetGivenDist(ship,target,dist);
            if (range > newrange)
            {
#if DEBUG_DEFENDER
            dbgMessagef("%x Changing back to approach",ship);
#endif
                ship->aistateattack = DEFENDER_APPROACH;
                break;
            }
circleAlreadyCalculatedTrajectory:

            vecDivideByScalar(trajectory,dist,temp);

            aitrackHeading(ship,&trajectory,FLYSHIP_ATTACKACCURACY);

            matGetVectFromMatrixCol2(right,ship->rotatable.rotinfo.coordsys);
            rightvelocity = vecDotProduct(right,ship->obj.posinfo.velocity);

            if (rightvelocity < defenderstat->CIRCLE_RIGHT_VELOCITY)
            {
                physApplyForceToObj(&ship->obj,ship->guidance.nonstatvars.thruststrength[TRANS_RIGHT]*defenderstat->CIRCLE_RIGHT_THRUST,TRANS_RIGHT);
            }

            if (range < defenderstat->tooCloseRange[ship->tacticstype])
            {
                aitrackZeroForwardVelocity(ship);
            }

            if (ship->attackvars.multipleAttackTargets)
            {
                gunShootGunsAtMultipleTargets(ship);
            }
            else
            {
                gunShootGunsAtTarget(ship,target,range,&trajectory);
            }

            break;

        default:
            dbgAssertOrIgnore(FALSE);
            break;
    }

//    attackStraightForward(ship,target,newrange,defenderstat->tooCloseRange[ship->tacticstype]);
}

void GenericDefenderAttackPassive(Ship *ship,Ship *target,bool32 rotate)
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

/*=============================================================================
    Name    : kamikazeAttack
    Purpose : Flys ship into one of the targets in the target list

=============================================================================*/
//if ship misses a ship, this is the so called wind up distance

#define K_Start         0
#define K_Ram           1
#define K_Ram2          2
#define K_TurnAround    3
#define K_TurnAround2   4
#define K_Yell          5

#define kamikazeFlyByDist 1200.0f

static void cat_kamikazeSteer(Ship *ship, Ship *target, real32 range)    // 0x005b16f0
{
    // CAT-PROVISIONAL: inside 800 units the exe steers the kamikaze straight at the target's collision centre by
    //      setting its velocity (from the speed multipliers and tacticsGetShipsMaxVelocity of both ships, the
    //      closing distance per frame, and cat_shipBlendHeadingDefault 0x005bffe0); skipped for non-ships, ships
    //      with SOF_CatDocking or orphaned modules. It writes the physics state directly and is ported with the
    //      collision/physics code (M2-R4 #3); until then the HW1 approach flies the last 800 units.
    (void)ship;
    (void)target;
    (void)range;
}

static void cat_kamikazeCheckHit(Ship *ship, Ship *target)                 // 0x005b1990
{
    // CAT-PROVISIONAL: when the kamikaze touches its target's collision box the exe applies the kamikaze collision at
    //      once (kamikazeCheck + ObjectsCollided with the two collision spheres); ported with Collision.c (M2-R4 #3).
    //      Until then the collision code's own per-frame kamikaze test handles the hit, as in HW1.
    (void)ship;
    (void)target;
}

void doKamikazeAttack(Ship *ship,SpaceObjRotImpTarg *target)
{
    real32 mag,dist,range,shipvel;
    vector destination,trajectory,heading,shipheading,EinsteinVelocity;
    ShipStaticInfo *shipstaticinfo = ship->staticinfo;

    vecSub(trajectory,target->impactable.collInfo.collPosition,ship->obj.posinfo.position);
    mag = vecMagnitudeSquared(trajectory);
    dist = fsqrt(mag);

    destination = trajectory;
    vecNormalize(&destination);
    heading = destination;

    //this hokey fudge factor is needed so the kamikaze ship doesn't
    //slow down as it reaches its destination
    mag = dist + 500.0f;
    vecScalarMultiply(destination,destination,mag);
    vecAddTo(destination,target->impactable.collInfo.collPosition);

    range = RangeToTargetGivenDist(ship,target,dist);

    switch(ship->kamikazeState)
    {
    case K_Start:
        //if within a threshold range
        bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_KamikazeCrazyFast);
        if(range < 500.0f)
        {
            //calculate flyby dist
            matGetVectFromMatrixCol3(shipheading, ship->rotatable.rotinfo.coordsys);
            vecScalarMultiply(shipheading,shipheading,kamikazeFlyByDist);
            vecAdd(ship->kamikazeVector,shipheading,target->impactable.collInfo.collPosition);
            ship->kamikazeState = K_TurnAround;
        }
        ship->kamikazeState = K_Ram;

        break;

    case K_Ram:
        if (range < 400.0f)
        {
            ship->kamikazeState = K_Yell;
        }
    case K_Ram2:
        //lets use...mmmmm..twister theor...no...relativity
        vecSub(EinsteinVelocity,ship->obj.posinfo.velocity,target->obj.posinfo.velocity);
        shipvel = fsqrt(vecMagnitudeSquared(EinsteinVelocity));
        // CAT: tMinelayerCorvette only; no DefenseFighter case (doKamikazeAttack 0x005b1a50)
        if(ship->shiptype == tMinelayerCorvette)
        {
            ((MinelayerCorvetteSpec *)(ship->ShipSpecifics))->mineaistate = MINE_DROP_ATTACK;
        }

        if (isShipStaticInterceptor(shipstaticinfo))
        {
            if (range < shipstaticinfo->bulletRange[ship->tacticstype])
            {
                GenericInterceptorStatics *interceptorstat = (GenericInterceptorStatics *)shipstaticinfo->custstatinfo;
                uword targetIndex;

                if (target->header.objtype == OBJ_ShipType)
                {
                    targetIndex = (uword)((ShipStaticInfo *)target->staticinfo)->shipclass;
                }
                else
                {
                    targetIndex = (uword)NUM_CLASSES;
                }

                if (GenericInterceptorCanFire(ship,target,&trajectory,interceptorstat->triggerHappy[ship->tacticstype][targetIndex]))
                {
                    ship->staticinfo->custshipheader.CustShipFire(ship,target);
                }
            }
        }
        else if (!(bitTest(ship->catShipFlags, CAT_SHIPFLAG_Mimic) && cat_shipIsMimicking(ship)))
        {
            // CAT: a mimicking ship holds its fire (0x005b1a50)
            gunShootGunsAtTarget(ship,target,range,&trajectory);
        }
        aishipFlyToPointAvoidingObjs(ship,&destination,AISHIP_PointInDirectionFlying | AISHIP_FirstPointInDirectionFlying | AISHIP_FastAsPossible,0.0f);
        if(range < 1500.0f)
        {
            bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_KamikazeCrazyFast);
        }
        else
        {
            bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_KamikazeCrazyFast);
        }
        // CAT: the last 800 units are steered, and a touch of the collision boxes is a hit at once (0x005b1a50;
        //      the blob test is the exe's: different blobs, or either ship without one)
        if (range < 800.0f)
        {
            cat_kamikazeSteer(ship, (Ship *)target, range);
        }
        if (range < 1.0f)
        {
            if (((ship->obj.collMyBlob != target->obj.collMyBlob) || (ship->obj.collMyBlob == NULL) || (target->obj.collMyBlob == NULL)) &&
                collCheckRectBumpCollision((SpaceObjRotImp *)ship, (SpaceObjRotImp *)target))
            {
                cat_kamikazeCheckHit(ship, (Ship *)target);
                return;
            }
        }

/*
        if(range < 400.0f && (shipvel < ship->staticinfo->common.maxvelocity*0.7f ||
                   vecDotProduct(trajectory,heading) < 0))
        {
            //ship is inside 400.0f, and isn't faceing ship, or isn't
            //going fast enough

            //calculate flyby dist
            matGetVectFromMatrixCol3(shipheading, ship->rotatable.rotinfo.coordsys);
            vecScalarMultiply(shipheading,shipheading,kamikazeFlyByDist);
            vecAdd(ship->kamikazeVector,shipheading,target->obj.posinfo.position);
            ship->kamikazeState = K_TurnAround;
        }
*/
        break;
    case K_TurnAround:
        bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_KamikazeCrazyFast);
        aishipFlyToPointAvoidingObjs(ship,&ship->kamikazeVector,AISHIP_PointInDirectionFlying | AISHIP_FirstPointInDirectionFlying | AISHIP_FastAsPossible,0.0f);
        if(MoveReachedDestinationVariable(ship,&ship->kamikazeVector,300.0f))
            ship->kamikazeState = K_TurnAround2;
        break;
    case K_TurnAround2:
        bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_KamikazeCrazyFast);
        if(aitrackHeadingWithFlags(ship,&heading,0.90f,AITRACKHEADING_IGNOREUPVEC))
            ship->kamikazeState = K_Ram;
        break;
    case K_Yell:
//      speechEvent(ship, COMM_LInt_Kamikaze, 0);
        battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_Kamikaze, ship, SOUND_EVENT_DEFAULT);
        ship->kamikazeState = K_Ram2;
        break;
    }

        //fire guns here...
    //
    //
    //Sound note:   Could put in kamikaze state change here after
    //              getting to within a certain distance of target
    //
    //          ask bryce how to...
}

// CAT: the exe's header (GenericDefenderHeader 0x008c42d8: kHeavyDefender, tHeavyDefender; original/scratch-m2/m2r2-shipstatic-tools/headers.txt).
//      Slot 6: the exe folded the identical "rotate ? attackPassiveRotate : attackPassive" body into P1MothershipAttackPassive 0x005be540.
CustShipHeader GenericDefenderHeader =
{
    .sizeofShipSpecifics = sizeof(GenericDefenderSpec),
    .CustShipStaticInit = GenericDefenderStaticInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = GenericDefenderAttackPassive,
    .CustShipAttack = GenericDefenderAttack,
};

