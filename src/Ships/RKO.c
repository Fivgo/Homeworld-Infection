// =============================================================================
//  RKO.c
//  - Cataclysm's dRKO (the Naggarok): picks a ship, flies at it, swallows it
//    into its weapon point and gains missiles from it (cataclysm.exe
//    0x005c3df0-0x005c59a0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each).
//      cat_shipIsRKO 0x005c54c0 moved here from Collision.c (it was a static copy there).
//      The exe's debug hooks (nullsubReturnFalse 0x006fe9a0 calls at each state change) do nothing and are left out.

#include "RKO.h"

#include "AITrack.h"
#include "Attack.h"
#include "Clamp.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "DefaultShip.h"
#include "Dissolve.h"
#include "Dock.h"
#include "FastMath.h"
#include "Globals.h"
#include "Leech.h"
#include "Matrix.h"
#include "RammingFrigate.h"
#include "Randy.h"
#include "Salvage.h"
#include "SaveGame.h"
#include "Sensors.h"
#include "ShipDisable.h"
#include "UnivUpdate.h"
#include "Universe.h"
#include "Vector.h"

#define RKO_SEARCH_INTERVAL         2.0f            // 0x0072a6c0
#define RKO_SEARCH_START            -2.0f           // 0xc0000000: the best closing speed starts below any
#define RKO_KILL_FACTOR             9999.0f         // 0x0072a6a4: damage = maxhealth * this
#define RKO_FACING_COS              0.8660200238f   // 0x0072c004: within 30 degrees
#define RKO_MOVED_SQ                160000.0f       // 0x0072c008: the target moved more than 400 since the plan
#define RKO_WAIT_PER_DIST           4.9999999e-05f  // 0x0072c00c
#define RKO_EAT_TIME_DEFAULT        16.0f           // 0x0072aaec: the dissolve time without an RKOEatMeTime
#define RKO_APPROACH_GAP            10.0f           // 0x0072a4f0
#define RKO_ARRIVE_DIST             500.0f          // 0x008c6408
#define RKO_FLEE_TIME               64.0f           // 0x008c640c
#define RKO_FLEE_TIME_TAIIDAN       4.0f            // 0x008c6410
#define RKO_FLEE_DIST               40000.0f        // 0x008c6414
#define RKO_FLEE_ARRIVE_DIST        20000.0f        // 0x008c642c
#define RKO_BLEND_NEW               0.2f            // 0x008c6430
#define RKO_BLEND_OLD               1.0f            // 0x008c6434
#define RKO_PULL_DIST_SQ            100000000.0f    // 0x008c643c: targets within 10000 are slowed and set drifting
#define RKO_PULL_DRIFT              5.0f            // 0x008c6440
#define RKO_PULL_SLOW               0.7f            // 0x008c6444
#define RKO_BRAKE                   0.5f            // 0x008c6448
#define RKO_SPEED_CHECK             100.0f          // 0x008c644c
#define RKO_BIGGUN_WARN_TIME        70.0f           // 0x008c6450
#define RKO_TURN_TIMEOUT            10.0f           // 0x005c46de push
#define RKO_TURN_ACCURACY           0.995f          // 0x005c46b1 push
#define RKO_APPROACH_ACCURACY       0.999f          // 0x005c4890 push
#define RKO_RANDOM_TO_UNIT          2.3283064365386963e-10f    // 0x0072a4e0, 2^-32

static udword RKOStatic;        // 0x00a7a5fc: a 4-byte block nothing reads

// cat_new_00515930: the radius of the big-gun danger sphere, 50000 + 18000 (two data words, 0x0089400c and 0x00893e4c,
//      nothing else reads or writes)
static real32 rkoBigGunRadiusA = 50000.0f;      // 0x0089400c
static real32 rkoBigGunRadiusB = 18000.0f;      // 0x00893e4c

static real32 cat_rkoBigGunRadius(void)                                                     // 0x00515930
{
    return rkoBigGunRadiusA + rkoBigGunRadiusB;
}

// CAT-TODO(stage 6): the Cataclysm KAS variables (cat_kasHashString 0x004a7cb0, cat_kasVarFind) come with the kas2c
//      missions; until then no variable is found and the exe's fallback (0 / not set) applies
static sdword rkoKasVariable(char *name)
{
    (void)name;
    return 0;
}

static real32 rkoRandomCoord(real32 size)
{
    real32 low = -size;
    return (size - low) * (real32)ranRandom(RANDOM_CAT_SHIPS) * RKO_RANDOM_TO_UNIT + low;
}

void RKOStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)          // 0x005c3df0
{
    statinfo->custstatinfo = &RKOStatic;
}

bool32 cat_vecWithinDistance(vector *a, vector *b, real32 dist)                             // 0x005c3e00
{
    real32 dx = a->x - b->x;
    real32 dy = a->y - b->y;
    real32 dz = a->z - b->z;
    return (dx * dx + dy * dy + dz * dz < dist * dist);
}

bool32 cat_vecWithinDistanceSq(vector *a, vector *b, real32 distsq)                         // 0x005c3e50
{
    real32 dx = a->x - b->x;
    real32 dy = a->y - b->y;
    real32 dz = a->z - b->z;
    return (dx * dx + dy * dy + dz * dz < distsq);
}

static void cat_rkoTurnTowardPoint(Ship *rko, vector *pos)                                  // 0x005c3ea0
{
    vector dir;
    vector heading;
    real32 maxvelocity;

    vecSub(dir, *pos, rko->obj.posinfo.position);
    vecNormalize(&dir);
    cat_shipBlendHeading(rko, &dir, RKO_BLEND_NEW, RKO_BLEND_OLD);
    heading.x = rko->rotatable.rotinfo.coordsys.m13;
    heading.y = rko->rotatable.rotinfo.coordsys.m23;
    heading.z = rko->rotatable.rotinfo.coordsys.m33;
    vecNormalize(&heading);
    maxvelocity = rko->staticinfo->common.maxvelocity;
    rko->obj.posinfo.velocity.x = maxvelocity * heading.x;
    rko->obj.posinfo.velocity.y = maxvelocity * heading.y;
    rko->obj.posinfo.velocity.z = maxvelocity * heading.z;
    // the exe then walks universe.ShipList calling cat_shipIsACV on each ship and drops the result (no effect)
}

void RKOInit(Ship *ship)                                                                    // 0x005c3f80
{
    RKOSpec *spec = (RKOSpec *)ship->ShipSpecifics;

    spec->state = RKO_Start;
    spec->naggarokMissiles = 0;
    spec->nextSearchTime = 0.0f;
    spec->restrictCenter.x = 0.0f;
    spec->restrictCenter.y = 0.0f;
    spec->restrictCenter.z = 0.0f;
    spec->restrictRadius = 1000000.0f;              // 0x49742400
    spec->restrictRadiusSq = 1.0e12f;               // 0x5368d4a5
    spec->restrictActive = FALSE;
    spec->fleeing = FALSE;
    spec->bigGunWarning = FALSE;
    spec->turnTimerActive = FALSE;
    spec->ateTaiidan = FALSE;
    spec->naggarokEMP = 0;
}

// a target being eaten dies with the RKO (RKODied 0x005c4000; inlined three times in RKOHouseKeep)
static void rkoKillTarget(Ship *ship)
{
    RKOSpec *spec = (RKOSpec *)ship->ShipSpecifics;
    Ship *target;

    if ((spec->state == RKO_Eating) && ((target = spec->target) != NULL))
    {
        ApplyDamageToTarget((SpaceObjRotImpTarg *)target, target->staticinfo->health.maxhealth * RKO_KILL_FACTOR,
                            GS_LargeEnergyCannon, DEATH_Killed_By_Player, ship->playerowner->playerIndex, NULL, 2);
    }
}

void RKODied(Ship *ship)                                                                    // 0x005c4000
{
    rkoKillTarget(ship);
}

static bool32 cat_rkoCanTarget(Ship *rko, Ship *target, bool32 traitor)                    // 0x005c4050
{
    RKOSpec *spec = (RKOSpec *)rko->ShipSpecifics;
    bool32 canTarget = !bitTest(rko->shipOrDerelict.specialFlags2, SPECIAL_2_CatKasNaggarok);
    ShipClass shipclass;
    real32 dx, dy, dz;

    if ((rko->playerowner == target->playerowner) && ((target->shiprace != RACE_Taiidan) || !traitor))
    {
        return FALSE;
    }
    if (canTarget)
    {
        shipclass = target->staticinfo->shipclass;
        if ((shipclass != CLASS_Frigate) && (shipclass != CLASS_Destroyer) && (shipclass != CLASS_HeavyCruiser) &&
            (shipclass != CLASS_Carrier))
        {
            return FALSE;
        }
        if (bitTest(target->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
        {
            return FALSE;
        }
        if (spec->restrictActive == TRUE)
        {
            dx = spec->restrictCenter.x - target->obj.posinfo.position.x;
            dy = spec->restrictCenter.y - target->obj.posinfo.position.y;
            dz = spec->restrictCenter.z - target->obj.posinfo.position.z;
            if (spec->restrictRadiusSq < dx * dx + dy * dy + dz * dz)
            {
                return FALSE;
            }
        }
        if (!cat_univPointInUniverse(&target->obj.posinfo.position))
        {
            return FALSE;
        }
    }
    return canTarget;
}

void cat_shipReleaseAttachedRammers(Ship *ship)                                             // 0x005c4130
{
    CatClampedList *clamped;
    Ship *attached;
    sdword i;

    if ((ship->guidance.clampInfo != NULL) && ((clamped = ship->guidance.clampInfo->clampedObjs) != NULL))
    {
        for (i = clamped->numShips - 1; i >= 0; i--)
        {
            attached = ship->guidance.clampInfo->clampedObjs->ShipPtr[i];
            cat_rammingFrigateForceRelease(attached);
            if (cat_shipIsLeech(attached))
            {
                ((LeechSpec *)attached->ShipSpecifics)->released = 1;
            }
            cat_salvageForceDetach(attached);
        }
    }
}

static void cat_rkoFaceShip(Ship *rko, Ship *other)                                         // 0x005c41a0
{
    vector dir;
    vector heading;

    vecSub(dir, other->obj.posinfo.position, rko->obj.posinfo.position);
    vecNormalize(&dir);
    heading.x = rko->rotatable.rotinfo.coordsys.m13;
    heading.y = rko->rotatable.rotinfo.coordsys.m23;
    heading.z = rko->rotatable.rotinfo.coordsys.m33;
    vecNormalize(&heading);
    if (vecDotProduct(dir, heading) <= RKO_FACING_COS)
    {
        cat_shipSetHeading(rko, &dir);
    }
}

// the RKO's weapon point in world space (RKOHouseKeep 0x005c4ec0, 0x005c51c0)
static void rkoWeaponPoint(Ship *ship, vector *result)
{
    DockStaticPoint *point = cat_dockFindDockStaticPoint("weapon", ship->staticinfo->dockStaticInfo);   // 0x008c6550

    matMultiplyMatByVec(result, &ship->rotatable.rotinfo.coordsys, &point->position);
    vecAddTo(*result, ship->obj.posinfo.position);
}

static void rkoBrake(Ship *ship)
{
    vecMultiplyByScalar(ship->obj.posinfo.velocity, RKO_BRAKE);
}

// slow down when not moving within 30 degrees of the heading (RKOHouseKeep 0x005c4260 and its COMMAND_MOVE case)
static void rkoBrakeOffHeading(Ship *ship)
{
    vector *velocity = &ship->obj.posinfo.velocity;
    vector heading;
    vector dir;

    if (RKO_SPEED_CHECK * RKO_SPEED_CHECK < velocity->z * velocity->z + velocity->y * velocity->y + velocity->x * velocity->x)
    {
        heading.x = ship->rotatable.rotinfo.coordsys.m13;
        heading.y = ship->rotatable.rotinfo.coordsys.m23;
        heading.z = ship->rotatable.rotinfo.coordsys.m33;
        dir = *velocity;
        vecNormalize(&dir);
        if (vecDotProduct(dir, heading) <= RKO_FACING_COS)
        {
            rkoBrake(ship);
        }
    }
}

void RKOHouseKeep(Ship *ship)                                                               // 0x005c4260
{
    RKOSpec *spec = (RKOSpec *)ship->ShipSpecifics;
    Ship *obj;
    Ship *target;
    Node *node;
    bool32 traitor;
    real32 best, closing, dist, dx, dy, dz, eatTime, fraction;
    vector diff, dir, pos;
    SelectCommand selectone;

    rkoBrakeOffHeading(ship);

    if (spec->naggarokEMP == 0)
    {
        spec->naggarokEMP = (udword)rkoKasVariable("G_NaggarokEMP");                       // 0x008bacfc
    }
    // an RKO under kasfEnableNaggarok control stops once it has taken that much EMP
    if (bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatKasNaggarok) &&
        (spec->naggarokEMP <= (udword)ship->catEmpShockCount))
    {
        rkoKillTarget(ship);
        rkoBrake(ship);
        return;
    }

    if ((spec->state == RKO_Eating) && (spec->bigGunWarning == TRUE))
    {
        vecSub(diff, ship->obj.posinfo.position, spec->bigGunPos);
        if (vecMagnitudeSquared(diff) < spec->bigGunRadiusSq)
        {
            rkoKillTarget(ship);
        }
    }
    if (spec->state != RKO_Turn)
    {
        spec->turnTimerActive = FALSE;
    }

    if ((ship->command != NULL) && (ship->command->ordertype.order != COMMAND_NULL))
    {
        if (ship->command->ordertype.order == COMMAND_MOVE)
        {
            rkoBrakeOffHeading(ship);
            // the exe then walks universe.ShipList calling cat_shipIsACV and drops the result (no effect)
        }
        if (spec->state != RKO_Start)
        {
            rkoKillTarget(ship);
            spec->state = RKO_Start;
            ship->autostabilizeship = TRUE;
        }
        goto warningcheck;
    }

    if (!spec->fleeing)
    {
        switch (spec->state)
        {
            case RKO_Start:
                spec->state = RKO_Search;
                // fall through
            case RKO_Search:
                if (universe.totaltimeelapsed < spec->nextSearchTime)
                {
                    break;
                }
                spec->target = NULL;
                best = RKO_SEARCH_START;
                spec->nextSearchTime = universe.totaltimeelapsed + RKO_SEARCH_INTERVAL;
                traitor = (rkoKasVariable("G_Traitor") == 1);                              // 0x008c64e4
                // the ship approaching fastest; a Taiidan ship beats any other, a carrier beats any non-carrier
                for (node = universe.SpaceObjList.head; node != NULL; node = node->next)
                {
                    obj = (Ship *)node->structptr;
                    if ((obj->header.objtype != OBJ_ShipType) || (obj == ship) || !cat_rkoCanTarget(ship, obj, traitor))
                    {
                        continue;
                    }
                    closing = ship->obj.posinfo.velocity.x * (ship->obj.posinfo.position.x - obj->obj.posinfo.position.x) +
                              ship->obj.posinfo.velocity.y * (ship->obj.posinfo.position.y - obj->obj.posinfo.position.y) +
                              ship->obj.posinfo.velocity.z * (ship->obj.posinfo.position.z - obj->obj.posinfo.position.z);
                    target = spec->target;
                    if ((target == NULL) ||
                        (((obj->shiprace != RACE_Taiidan) || (target->shiprace == RACE_Taiidan)) && (target->shiptype != sCarrier)))
                    {
                        if ((best < closing) &&
                            ((target == NULL) ||
                             (((obj->shiptype != sCarrier) || (target->shiptype == sCarrier)) &&
                              ((obj->shiprace == RACE_Taiidan) || (target->shiprace != RACE_Taiidan)))))
                        {
                            spec->target = obj;
                            best = closing;
                        }
                    }
                    else
                    {
                        spec->target = obj;
                        best = closing;
                    }
                }
                if (spec->target == NULL)
                {
                    break;
                }
                spec->state = RKO_Found;
                // fall through
            case RKO_Found:
                spec->state = RKO_Plan;
                // fall through
            case RKO_Plan:
                target = spec->target;
                spec->targetStartPos = target->obj.posinfo.position;
                spec->approachPoint = target->obj.posinfo.position;
                vecSub(spec->toTarget, target->obj.posinfo.position, ship->obj.posinfo.position);
                spec->toTargetDist = fsqrt(vecMagnitudeSquared(spec->toTarget));
                spec->approachHeading = spec->toTarget;
                vecMultiplyByScalar(spec->approachHeading, 1.0f / spec->toTargetDist);
                dist = -(ship->staticinfo->common.staticCollInfo.collspheresize +
                         spec->target->staticinfo->common.staticCollInfo.collspheresize + RKO_APPROACH_GAP);
                spec->approachPoint.x += dist * spec->approachHeading.x;
                spec->approachPoint.y += dist * spec->approachHeading.y;
                spec->approachPoint.z += dist * spec->approachHeading.z;
                aitrackZeroVelocity(ship);
                spec->state = RKO_Turn;
                // fall through
            case RKO_Turn:
                ship->autostabilizeship = FALSE;
                vecZeroVector(ship->obj.posinfo.velocity);
                if ((spec->target != NULL) && !cat_univPointInUniverse(&spec->target->obj.posinfo.position))
                {
                    spec->target = NULL;
                    spec->state = RKO_Start;
                    break;
                }
                if (!aitrackHeadingFunc((SpaceObjRotImpTargGuidance *)ship, &spec->approachHeading, RKO_TURN_ACCURACY,
                                        0.0f, 0.0f, 0))
                {
                    if (!spec->turnTimerActive)
                    {
                        spec->turnTimerActive = TRUE;
                        cat_timerStartGameTime(&spec->turnTimer, RKO_TURN_TIMEOUT);
                    }
                    if ((spec->turnTimerActive != TRUE) || !cat_timerExpired(&spec->turnTimer))
                    {
                        break;
                    }
                    dir = spec->approachHeading;
                    vecNormalize(&dir);
                    cat_shipSetHeading(ship, &dir);
                }
                spec->state = RKO_Turned;
                // fall through
            case RKO_Turned:
                ship->autostabilizeship = TRUE;
                spec->state = RKO_WaitStart;
                // fall through
            case RKO_WaitStart:
                spec->waitStart = universe.totaltimeelapsed;
                spec->state = RKO_Wait;
                spec->waitEnd = spec->toTargetDist * RKO_WAIT_PER_DIST + spec->waitStart;
                // fall through
            case RKO_Wait:
                vecZeroVector(ship->obj.posinfo.velocity);
                ship->autostabilizeship = FALSE;
                if (!(1.0f <= (universe.totaltimeelapsed - spec->waitStart) / (spec->waitEnd - spec->waitStart)))
                {
                    break;
                }
                spec->state = RKO_Waited;
                // fall through
            case RKO_Waited:
                ship->autostabilizeship = TRUE;
                spec->state = RKO_Go;
                // fall through
            case RKO_Go:
                ship->autostabilizeship = FALSE;
                spec->state = RKO_Approach;
                // fall through
            case RKO_Approach:
                ship->autostabilizeship = FALSE;
                // the exe walks universe.ShipList calling cat_shipIsACV and drops the result (no effect)
                if ((spec->target != NULL) && !cat_univPointInUniverse(&spec->target->obj.posinfo.position))
                {
                    spec->target = NULL;
                    spec->state = RKO_Start;
                    break;
                }
                aitrackHeadingFunc((SpaceObjRotImpTargGuidance *)ship, &spec->approachHeading, RKO_APPROACH_ACCURACY,
                                   0.0f, 0.0f, 0);
                vecSub(diff, spec->approachPoint, ship->obj.posinfo.position);
                target = spec->target;
                if (target != NULL)
                {
                    dx = target->obj.posinfo.position.x - ship->obj.posinfo.position.x;
                    dy = target->obj.posinfo.position.y - ship->obj.posinfo.position.y;
                    dz = target->obj.posinfo.position.z - ship->obj.posinfo.position.z;
                    if (dx * dx + dy * dy + dz * dz < RKO_PULL_DIST_SQ)
                    {
                        vecMultiplyByScalar(target->obj.posinfo.velocity, RKO_PULL_SLOW);
                        cat_shipStartDrift(target, RKO_PULL_DRIFT, RKO_PULL_DRIFT);
                    }
                }
                if (RKO_ARRIVE_DIST * RKO_ARRIVE_DIST < diff.x * diff.x + diff.y * diff.y + diff.z * diff.z)
                {
                    vecCopyAndNormalize(&diff, &ship->obj.posinfo.velocity);
                    vecMultiplyByScalar(ship->obj.posinfo.velocity, ship->staticinfo->common.maxvelocity);
                    break;
                }
                // overshot the approach point: put it there
                if (diff.x * spec->toTarget.x + diff.y * spec->toTarget.y + diff.z * spec->toTarget.z <= 0.0f)
                {
                    cat_univSetShipPosition(ship, &spec->approachPoint);
                }
                vecZeroVector(ship->obj.posinfo.velocity);
                spec->state = RKO_Arrived;
                // fall through
            case RKO_Arrived:
                ship->autostabilizeship = TRUE;
                spec->state = RKO_Arrived2;
                // fall through
            case RKO_Arrived2:
                spec->state = RKO_CheckTarget;
                // fall through
            case RKO_CheckTarget:
                if ((spec->target != NULL) && !cat_univPointInUniverse(&spec->target->obj.posinfo.position))
                {
                    spec->target = NULL;
                    spec->state = RKO_Start;
                    break;
                }
                target = spec->target;
                if (target == NULL)
                {
                    spec->state = RKO_Start;
                    break;
                }
                vecSub(diff, target->obj.posinfo.position, spec->targetStartPos);
                if (!(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z <= RKO_MOVED_SQ))
                {
                    spec->state = RKO_Start;
                    break;
                }
                spec->state = RKO_Grab;
                // fall through
            case RKO_Grab:
                spec->state = RKO_Swallow;
                // fall through
            case RKO_Swallow:
                ship->autostabilizeship = FALSE;
                vecZeroVector(ship->rotatable.rotinfo.rotspeed);
                target = spec->target;
                cat_shipSetDisableReason(target, 1, 0xb);
                selectone.ShipPtr[0] = target;
                selectone.numShips = 1;
                RemoveShipsFromDoingStuff(&universe.mainCommandLayer, &selectone);  // CAT-TODO(M2-R4 command follow-up): reason 0xca
                rkoWeaponPoint(ship, &pos);
                eatTime = RKO_EAT_TIME_DEFAULT;
                if (0.0f < target->staticinfo->RKOEatMeTime)
                {
                    eatTime = target->staticinfo->RKOEatMeTime;
                }
                cat_shipDissolveStart(target, &pos, eatTime);
                target->catRKOMissilesTaken = 0;
                ship->autostabilizeship = FALSE;
                spec->state = RKO_Eating;
                cat_shipReleaseAttachedRammers(target);        // inlined in the exe
                cat_rammingFrigateForceRelease(target);
                cat_rkoFaceShip(ship, target);
                if ((target->guidance.clampInfo == NULL) || (target->guidance.clampInfo->host == NULL))
                {
                    clampObjToObj((SpaceObjRotImpTargGuidance *)target, (SpaceObjRotImpTargGuidance *)ship);
                }
                spec->ateTaiidan = (spec->target->shiprace == RACE_Taiidan);
                // fall through
            case RKO_Eating:
                fraction = cat_shipDissolveFraction(spec->target);
                if (1.0f <= fraction)
                {
                    fraction = 1.0f;
                }
                else
                {
                    fraction = cat_shipDissolveFraction(spec->target);
                }
                rkoBrake(ship);
                // one missile per RKOGetsMissiles share eaten (the low byte of the key, signed)
                target = spec->target;
                while ((real32)target->catRKOMissilesTaken <
                       (real32)(sbyte)target->staticinfo->RKOGetsMissiles * fraction)
                {
                    target->catRKOMissilesTaken++;
                    spec->naggarokMissiles++;
                    target = spec->target;
                }
                if (!(1.0f <= cat_shipDissolveFraction(spec->target)))
                {
                    break;
                }
                spec->state = RKO_Eaten;
                // fall through
            case RKO_Eaten:
                univReallyDeleteThisShipRightNow(spec->target);     // RKORemoveShipReferences clears spec->target
                ship->autostabilizeship = TRUE;
                spec->state = RKO_Start;
                spec->fleeing = TRUE;
                spec->fleePoint.x = ship->rotatable.rotinfo.coordsys.m13 * RKO_FLEE_DIST + ship->obj.posinfo.position.x;
                spec->fleePoint.y = ship->rotatable.rotinfo.coordsys.m23 * RKO_FLEE_DIST + ship->obj.posinfo.position.y;
                spec->fleePoint.z = ship->rotatable.rotinfo.coordsys.m33 * RKO_FLEE_DIST + ship->obj.posinfo.position.z;
                cat_timerStartGameTime(&spec->fleeTimer, (spec->ateTaiidan == TRUE) ? RKO_FLEE_TIME_TAIIDAN : RKO_FLEE_TIME);
                break;
            default:
                break;
        }
    }

    // keep the dissolve effect between the weapon point and the target
    if ((spec->target != NULL) && (spec->target->catDissolveEffect != NULL))
    {
        Effect *effect = spec->target->catDissolveEffect;

        rkoWeaponPoint(ship, &pos);
        vecSub(diff, spec->target->obj.posinfo.position, pos);
        dir = diff;
        vecNormalize(&dir);
        matCreateCoordSysFromHeading(&effect->rotatable.rotinfo.coordsys, &dir);
        effect->obj.posinfo.position = pos;
        *(real32 *)effect->variable = fsqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
        cat_shipDissolveSetDirection(spec->target, &pos);
    }

    if (spec->fleeing == TRUE)
    {
        if (cat_timerExpired(&spec->fleeTimer))
        {
            spec->fleeing = FALSE;
        }
        else if (!cat_vecWithinDistanceSq(&ship->obj.posinfo.position, &spec->fleePoint,
                                          RKO_FLEE_ARRIVE_DIST * RKO_FLEE_ARRIVE_DIST))
        {
            cat_rkoTurnTowardPoint(ship, &spec->fleePoint);
        }
        else
        {
            spec->fleePoint.x = rkoRandomCoord(smUniverseSizeX);
            spec->fleePoint.y = rkoRandomCoord(smUniverseSizeY);
            spec->fleePoint.z = rkoRandomCoord(smUniverseSizeZ);
        }
    }

warningcheck:
    if ((spec->bigGunWarning == TRUE) && cat_timerExpired(&spec->bigGunTimer))
    {
        spec->bigGunWarning = FALSE;
    }
}

void RKORemoveShipReferences(Ship *ship, Ship *shiptoremove)                                // 0x005c5450
{
    RKOSpec *spec = (RKOSpec *)ship->ShipSpecifics;

    if (shiptoremove == spec->target)
    {
        cat_shipDissolveStop(shiptoremove);
        spec->target = NULL;
        spec->state = RKO_Start;
    }
}

void RKO_PreFix(Ship *ship)                                                                 // 0x005c5480
{
    RKOSpec *spec = (RKOSpec *)ship->ShipSpecifics;

    spec->target = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->target);
}

void RKO_Fix(Ship *ship)                                                                    // 0x005c54a0
{
    RKOSpec *spec = (RKOSpec *)ship->ShipSpecifics;

    spec->target = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->target);
}

bool32 cat_shipIsRKO(Ship *ship)                                                            // 0x005c54c0
{
    if (ship->header.objtype == OBJ_ShipType)
    {
        return (ship->shiptype == dRKO);
    }
    return FALSE;
}

sdword cat_rkoGetNaggarokMissiles(Ship *rko)                                                // 0x005c54e0
{
    return ((RKOSpec *)rko->ShipSpecifics)->naggarokMissiles;
}

void cat_rkoDecNaggarokMissiles(Ship *rko)                                                  // 0x005c54f0
{
    ((RKOSpec *)rko->ShipSpecifics)->naggarokMissiles--;
}

// the danger sphere of a big-gun shot and a random point outside it (cat_rkoSetBigGunPos 0x005c5500,
//      cat_rkoWarnBigGunFire 0x005c57c0)
static void rkoWarn(Ship *rko, vector *pos)
{
    RKOSpec *spec = (RKOSpec *)rko->ShipSpecifics;
    real32 radiusA, radiusB, x, y, z, dx, dy, dz;
    bool32 found = FALSE;
    sdword tries = 0;

    spec->bigGunWarning = TRUE;
    cat_timerStartGameTime(&spec->bigGunTimer, RKO_BIGGUN_WARN_TIME);
    radiusA = cat_rkoBigGunRadius();
    radiusB = cat_rkoBigGunRadius();
    spec->bigGunRadiusSq = radiusB * radiusA;
    spec->bigGunPos = *pos;
    do
    {
        x = rkoRandomCoord(smUniverseSizeX);
        y = rkoRandomCoord(smUniverseSizeY);
        z = rkoRandomCoord(smUniverseSizeZ);
        dx = x - pos->x;
        dy = y - pos->y;
        dz = z - pos->z;
        if (radiusB * radiusA < dx * dx + dy * dy + dz * dz)
        {
            spec->avoidPos.x = x;
            spec->avoidPos.y = y;
            spec->avoidPos.z = z;
            found = TRUE;
        }
        tries++;
    } while (!found);
    // (more than 1000 tries reaches a debug hook only)
}

void cat_rkoSetBigGunPos(Ship *rko, vector *pos)                                            // 0x005c5500
{
    if (rko != NULL)
    {
        rkoWarn(rko, pos);
    }
}

bool32 cat_rkoGetAvoidLocation(Ship *rko, vector *pos, vector *dir)                         // 0x005c56b0
{
    RKOSpec *spec = (RKOSpec *)rko->ShipSpecifics;

    if (spec->bigGunWarning == TRUE)
    {
        *pos = spec->bigGunPos;
        *dir = spec->avoidPos;
    }
    return (spec->bigGunWarning == TRUE);
}

bool32 cat_rkoIsSafe(Ship *rko)                                                             // 0x005c5700
{
    RKOSpec *spec = (RKOSpec *)rko->ShipSpecifics;
    vector diff;

    if (spec->bigGunWarning == TRUE)
    {
        vecSub(diff, rko->obj.posinfo.position, spec->bigGunPos);
        if (diff.x * diff.x + diff.y * diff.y + diff.z * diff.z < spec->bigGunRadiusSq)
        {
            return FALSE;
        }
    }
    return TRUE;
}

bool32 cat_rkoBeware(Ship *rko)                                                             // 0x005c5760
{
    return (((RKOSpec *)rko->ShipSpecifics)->bigGunWarning == TRUE);
}

// the last dRKO in universe.ShipList, single player only
static Ship *rkoFindLast(void)
{
    Node *node;
    Ship *rko = NULL;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        if (((Ship *)node->structptr)->shiptype == dRKO)
        {
            rko = (Ship *)node->structptr;
        }
    }
    return rko;
}

void cat_rkoNotifyBigGunExplosion(vector *pos)                                              // 0x005c5780
{
    Ship *rko;

    if (singlePlayerGame && ((rko = rkoFindLast()) != NULL))
    {
        ((RKOSpec *)rko->ShipSpecifics)->bigGunWarning = FALSE;
    }
}

void cat_rkoWarnBigGunFire(vector *pos)                                                     // 0x005c57c0
{
    Ship *rko;

    if (singlePlayerGame && ((rko = rkoFindLast()) != NULL))
    {
        rkoWarn(rko, pos);
    }
}

void RKOAttackPassive(Ship *ship,Ship *target,bool32 rotate)                                // 0x005c59a0
{
    attackPassive(ship, target);
}

CustShipHeader RKOHeader =
{
    .sizeofShipSpecifics = sizeof(RKOSpec),
    .CustShipStaticInit = RKOStaticInit,
    .CustShipInit = RKOInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = RKOAttackPassive,
    .CustShipHousekeep = RKOHouseKeep,
    .CustShipRemoveShipReferences = RKORemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))RKORemoveShipReferences,
    .CustShipDied = RKODied,
    .CustShip_PreFix = RKO_PreFix,
    .CustShip_Fix = RKO_Fix,
};
