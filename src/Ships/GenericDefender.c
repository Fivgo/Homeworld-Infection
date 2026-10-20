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
#include "Tactics.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "Vector.h"

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

// CAT (M2-R4 combat): turns ship towards dir, 0.07 of dir to 1 of its heading, and refreshes its collision box
//      (cat_shipBlendHeadingDefault 0x005bffe0; the weights are the exe's data at 0x008c5da4 / 0x008c5da8)
static void cat_shipBlendHeadingDefault(Ship *ship, vector *dir)
{
    const real32 wdir = 0.07f, wcur = 1.0f;
    vector heading;
    real32 sum = wdir + wcur;

    heading.x = (wcur * ship->rotatable.rotinfo.coordsys.m13 + wdir * dir->x) / sum;
    heading.y = (wcur * ship->rotatable.rotinfo.coordsys.m23 + wdir * dir->y) / sum;
    heading.z = (wcur * ship->rotatable.rotinfo.coordsys.m33 + wdir * dir->z) / sum;
    vecNormalize(&heading);
    matCreateCoordSysFromHeading(&ship->rotatable.rotinfo.coordsys, &heading);
    collUpdateCollRectangle((SpaceObjRotImp *)ship);
}

// CAT (M2-R4 combat): inside 800 units a kamikaze that is at least as fast as its target is steered at the target's
//      collision centre by setting its velocity: at speed (at least 200, x1.3 below its top speed) when it is still
//      200 or more away or the target is big (collision sphere 50 or more), else so that it reaches the target in the
//      next frame (at least 100) plus the target's velocity; it turns 7% towards it (cat_kamikazeSteer 0x005b16f0).
//      Not at a non-ship, a docking ship or an orphaned module.
static void cat_kamikazeSteer(Ship *ship, Ship *target, real32 range)    // 0x005b16f0
{
    vector targetPos, shipPos, dir;
    real32 speed, closing;

    if ((target->header.objtype != OBJ_ShipType) || bitTest(target->header.flags, SOF_CatDocking) ||
        bitTest(target->header.catFlags18, CAT_SHIPFLAG_Invincible))
    {
        return;
    }
    // CAT (M2-R10 oracle-rung-physics): the ship's top speed is stored, the target's compared unrounded (0x005b172f ..
    //      0x005b1757); the later test compares the unrounded product with the stored speed (0x005b1916)
    if ((real32)(tacticsGetShipsMaxVelocity(ship) * cat_shipGetSpeedMultiplier(ship)) <
        tacticsGetShipsMaxVelocity(target) * cat_shipGetSpeedMultiplier(target))
    {
        return;
    }
    targetPos = target->impactable.collInfo.collPosition;
    shipPos = ship->obj.posinfo.position;
    speed = fsqrt(ship->obj.posinfo.velocity.z * ship->obj.posinfo.velocity.z +
                  ship->obj.posinfo.velocity.y * ship->obj.posinfo.velocity.y +
                  ship->obj.posinfo.velocity.x * ship->obj.posinfo.velocity.x);
    vecSub(dir, targetPos, shipPos);
    vecNormalize(&dir);
    if ((range >= 200.0f) || (range <= 0.0f) || (target->staticinfo->common.staticCollInfo.collspheresize >= 50.0f))
    {
        if (speed < 200.0f)
        {
            speed = 200.0f;
        }
        if (speed < tacticsGetShipsMaxVelocity(ship) * cat_shipGetSpeedMultiplier(ship))
        {
            speed = 1.3f * speed;
        }
        ship->obj.posinfo.velocity.x = dir.x * speed;
        ship->obj.posinfo.velocity.y = dir.y * speed;
        ship->obj.posinfo.velocity.z = dir.z * speed;
    }
    else
    {
        if (universe.phystimeelapsed <= 0.0f)
        {
            closing = 100.0f;
        }
        else
        {
            closing = cat_vecDistance(&shipPos, &targetPos) / universe.phystimeelapsed;
            if (closing < 100.0f)
            {
                closing = 100.0f;
            }
        }
        ship->obj.posinfo.velocity.x = dir.x * closing + target->obj.posinfo.velocity.x;
        ship->obj.posinfo.velocity.y = dir.y * closing + target->obj.posinfo.velocity.y;
        ship->obj.posinfo.velocity.z = closing * dir.z + target->obj.posinfo.velocity.z;
    }
    cat_shipBlendHeadingDefault(ship, &dir);
}

// CAT (M2-R4 combat): a kamikaze that touches its target's collision box hits at once: kamikazeCheck, then the two
//      collide with their collision spheres (cat_kamikazeCheckHit 0x005b1990)
static void cat_kamikazeCheckHit(Ship *ship, Ship *target)                 // 0x005b1990
{
    vector distvector;
    real32 distsquared;

    if (kamikazeCheck((SpaceObjRotImpTarg *)ship, (SpaceObjRotImpTarg *)target))
    {
        vecSub(distvector, target->impactable.collInfo.collPosition, ship->impactable.collInfo.collPosition);
        distsquared = distvector.x * distvector.x + distvector.y * distvector.y + distvector.z * distvector.z;
        ObjectsCollided((SpaceObjRotImpTarg *)ship, (SpaceObjRotImpTarg *)target,
                        ship->staticinfo->common.staticCollInfo.collspheresize + target->staticinfo->common.staticCollInfo.collspheresize,
                        &distvector, fsqrt(distsquared), distsquared);
    }
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
        else if (!(bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic) && cat_shipIsMimicking(ship)))
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

