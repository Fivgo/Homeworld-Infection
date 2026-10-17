// =============================================================================
//  Leech.c
//  - Cataclysm's sLeech: latches onto an enemy ship and drains its resource
//    units or its hull (cataclysm.exe 0x005b6890-0x005b80f0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each).
//      cat_leechIsLeeching 0x005b7ce0 moved here from ShipSelect.c (its declaration stays in ShipSelect.h).

#include "Leech.h"

#include "AIShip.h"
#include "AITrack.h"
#include "Clamp.h"
#include "ClassDefs.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "CommandWrap.h"
#include "Dock.h"
#include "ETG.h"
#include "FastMath.h"
#include "Matrix.h"
#include "Memory.h"
#include "MeshAnim.h"
#include "Randy.h"
#include "SaveGame.h"
#include "Select.h"
#include "SoundEvent.h"
#include "UnivUpdate.h"
#include "Universe.h"

#define LEECH_AUTO_TARGET_RANGE     7000.0f
#define LEECH_LAST_TARGET_TIME      5.0f        // 0x0072a624
#define LEECH_LATCH_OFFSET          6.0f        // 0x0072a7f0
#define LEECH_LEAVE_FACTOR          0.2f        // 0x0072b0f0

static udword LeechStatic;      // 0x00a7ac3c: a 4-byte block nothing reads

// the "no targets / target found" messages of the local player's leeches (0x00a762dc .. 0x00a762e4)
static ubyte leechMsgTargetFound = 0;          // 0x00a762dc
static ubyte leechMsgNoTarget = 0;             // 0x00a762dd
static real32 leechMsgQuietUntil = 0.0f;       // 0x00a762e0
static real32 leechMsgDue = 0.0f;              // 0x00a762e4

ubyte cat_leechGetAction(Ship *ship)                                                        // 0x005b6890
{
    return ((LeechSpec *)ship->ShipSpecifics)->action;
}

void cat_leechSetAction(Ship *ship, ubyte action)                                           // 0x005b68a0
{
    ((LeechSpec *)ship->ShipSpecifics)->action = action;
    if (action == LEECH_ACTION_None)
    {
        speechEventQueue(ship, 0x40008b1, 0, -1, -1, -1, -1, 0.0f, -1);
    }
    else if (action == LEECH_ACTION_DrainRUs)
    {
        speechEventQueue(ship, 0x40008af, 0, -1, -1, -1, -1, 0.0f, -1);
    }
    else if (action == LEECH_ACTION_DrainHull)
    {
        speechEventQueue(ship, 0x40008b0, 0, -1, -1, -1, -1, 0.0f, -1);
    }
}

// the venting effect of a hull drain (etgSpecialPurposeEffectTable[EGT_LEECH_VENT_EFFECT], the exe's 0x00c41d24), at
// the lower LOD of the leech and its target, then again in 3..5 s
static void cat_leechDrainEffect(Ship *ship, ubyte action)                                  // 0x005b6920
{
    LeechSpec *spec = (LeechSpec *)ship->ShipSpecifics;
    etglod *etgLOD = etgSpecialPurposeEffectTable[EGT_LEECH_VENT_EFFECT];
    etgeffectstatic *stat;
    udword lod;

    if ((action != LEECH_ACTION_DrainHull) || (etgLOD == NULL))
    {
        return;
    }
    lod = ship->obj.currentLOD;
    if ((spec->target != NULL) && (spec->target->obj.currentLOD <= lod))
    {
        lod = spec->target->obj.currentLOD;
    }
    stat = ((sdword)lod < etgLOD->nLevels) ? etgLOD->level[lod] : NULL;
    if ((stat != NULL) && !etgFrequencyExceeded(stat))
    {
        if (univSpaceObjInRenderList((SpaceObj *)ship))
        {
            etgEffectCreate(stat, ship, NULL, NULL, NULL, 0.0f, EAF_PosCoord, 2,
                            SCALECAST(Real32ToUdword(1.0f)), SCALECAST(Real32ToUdword(1.0f)));
        }
        spec->nextEffectTime = (real32)ranRandom(RANDOM_STREAM_0) * 4.656613e-10f + 3.0f + universe.totaltimeelapsed;
    }
}

// a free DPT_Leech point of ship, -1 none (the exe inlines the scan in cat_leechPickTarget)
static bool32 leechShipHasFreePoint(Ship *ship)
{
    DockStaticInfo *dockstaticinfo = ship->staticinfo->dockStaticInfo;
    sdword i;

    for (i = 0; i < dockstaticinfo->numDockPoints; i++)
    {
        if ((dockstaticinfo->dockstaticpoints[i].type == DPT_Leech) && (ship->dockInfo->thisDockBusy[i] == 0))
        {
            return TRUE;
        }
    }
    return FALSE;
}

// a random special target with a free leech point (a resource drain skips ships with too little hull)
static bool32 cat_leechPickTarget(Ship *ship)                                               // 0x005b6a10
{
    LeechSpec *spec = (LeechSpec *)ship->ShipSpecifics;
    MaxSelection *targets = spec->targets;
    Ship *candidate;
    sdword *pick;
    sdword num = 0;
    sdword i;

    spec->target = NULL;
    if (targets != NULL)
    {
        if (targets->numShips == 0)
        {
            return FALSE;
        }
        pick = memAlloc(targets->numShips * sizeof(sdword), "LeechPick", 0);
        for (i = 0; i < targets->numShips; i++)
        {
            candidate = targets->ShipPtr[i];
            if (((spec->drainMode != LEECH_ACTION_DrainRUs) ||
                 ((real32)(candidate->staticinfo->leechRUsPerSecond << 2) < candidate->targetable.health)) &&
                ((candidate->guidance.clampInfo == NULL) || (candidate->guidance.clampInfo->host == NULL)) &&
                (candidate->dockInfo != NULL) && leechShipHasFreePoint(candidate))
            {
                pick[num++] = i;
            }
        }
        if (num != 0)
        {
            spec->target = targets->ShipPtr[pick[ranRandom(RANDOM_GAME) % (udword)num]];
        }
        memFree(pick);
    }
    if ((ship->playerowner == universe.curPlayerPtr) && (leechMsgQuietUntil < universe.realtimeelapsed))
    {
        if (spec->target == NULL)
        {
            leechMsgNoTarget = 1;
        }
        else
        {
            leechMsgTargetFound = 1;
        }
        if (leechMsgDue == 0.0f)
        {
            leechMsgDue = universe.realtimeelapsed + 0.5f;
        }
    }
    return spec->target != NULL;
}

// reserve a random free DPT_Leech point on the target
static bool32 cat_leechReserveLatchPoint(Ship *ship)                                       // 0x005b6be0
{
    LeechSpec *spec = (LeechSpec *)ship->ShipSpecifics;
    Ship *target = spec->target;
    DockStaticInfo *dockstaticinfo;
    sdword *free;
    sdword num = 0;
    sdword i;
    bool32 found;

    if ((target == NULL) || (target->dockInfo == NULL) ||
        ((dockstaticinfo = target->staticinfo->dockStaticInfo)->numDockPoints <= 0))
    {
        return FALSE;
    }
    free = memAlloc(dockstaticinfo->numDockPoints * sizeof(sdword), "LeechFree", 0);
    for (i = 0; i < dockstaticinfo->numDockPoints; i++)
    {
        if ((dockstaticinfo->dockstaticpoints[i].type == DPT_Leech) && (target->dockInfo->thisDockBusy[i] == 0))
        {
            free[num++] = i;
        }
    }
    if (num != 0)
    {
        spec->latchPoint = free[ranRandom(RANDOM_GAME) % (udword)num];
        target->dockInfo->thisDockBusy[spec->latchPoint] = 1;
    }
    found = (num != 0);
    memFree(free);
    return found;
}

// give the latch point back (inlined at every user in the exe)
static void leechFreeLatchPoint(LeechSpec *spec)
{
    if (spec->latchPoint != -1)
    {
        if (spec->target != NULL)
        {
            spec->target->dockInfo->thisDockBusy[spec->latchPoint] = 0;
        }
        spec->latchPoint = -1;
    }
}

// the mesh animation: 0 opens the mouth to latch, 1 closes it (the exe's madAnimationStart(ship, n, 0 / 2))
static void leechCloseBody(Ship *ship, LeechSpec *spec)
{
    cat_madAnimationStartNumber(ship, 1);       // CAT-TODO(stage 3): the exe passes a third argument, 2
    spec->bodyOpen = 0;
}

static void cat_leechDetach(Ship *ship)                                                     // 0x005b6ce0
{
    LeechSpec *spec = (LeechSpec *)ship->ShipSpecifics;
    ubyte state = spec->state;

    spec->target = NULL;
    switch (state)
    {
        case LEECH_Idle:
        case 5:
        case LEECH_DetachNext:
        default:
            return;
        case LEECH_Done:
            spec->state = LEECH_Idle;
            return;
        case LEECH_PickTarget:
        case LEECH_Approach:
        case LEECH_Latch:
        case 4:
        case LEECH_ACTION_None:
        case LEECH_ACTION_DrainRUs:
        case LEECH_ACTION_DrainHull:
        case LEECH_Detach:
        case LEECH_DetachFull:
        case LEECH_Leave:
        case LEECH_Leave2:
            break;
    }
    if (state == LEECH_ACTION_DrainRUs)
    {
        speechEventQueue(ship, 0x20008b2, 0, -1, -1, -1, -1, 0.0f, -1);
    }
    else if (state == LEECH_ACTION_DrainHull)
    {
        speechEventQueue(ship, 0x20008b3, 0, -1, -1, -1, -1, 0.0f, -1);
    }
    // the target is already cleared, so the point's busy word stays (as in the exe)
    leechFreeLatchPoint(spec);
    bitClear(ship->header.flags, SOF_CatDocking);
    bitClear(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatNoAvoidance);
    if (!cat_leechPickTarget(ship))
    {
        if (spec->drainMode == LEECH_ACTION_DrainRUs)
        {
            if (ship->resources != 0)
            {
                spec->returnToDock = 1;
                spec->full = 0;
            }
        }
        else
        {
            spec->state = LEECH_Leave;
        }
    }
    else
    {
        spec->state = LEECH_PickTarget;
    }
    if (spec->bodyOpen)
    {
        leechCloseBody(ship, spec);
    }
    unClampObj((SpaceObjRotImpTargGuidance *)ship);
}

// the special targets a leech may drain: no corvettes, fighters, components or micro ships; of the module ships only
// the first one counts, as its host
static MaxSelection *cat_leechFilterTargets(SelectCommand *targets)                         // 0x005b6e20
{
    MaxSelection *filtered;
    Ship *target;
    bool32 moduleDone = FALSE;
    sdword i;

    if (targets == NULL)
    {
        return NULL;
    }
    filtered = memAlloc(sizeof(MaxSelection), "LeechTargets", NonVolatile);
    filtered->numShips = 0;
    for (i = 0; i < targets->numShips; i++)
    {
        target = targets->ShipPtr[i];
        switch (target->shiptype)
        {
            case sMothershipSpecial: case sMothershipEngineering: case sMothershipArmour: case sMothershipMicro:
            case sMothershipWeapons: case sMothershipDockingBay: case sMothershipSupport: case sMothershipCommand:
            case sMothershipEnginesLower: case sMothershipEnginesUpper: case sMothershipLowerSpine:
            case sMothershipBigGun: case sMothershipOreCanArmour: case bMothershipDockingBay:
            case bMothershipEnginesLower: case bSectMoship: case bMothershipSupport:
                if (!moduleDone && (target->dockvars.dockship != NULL))
                {
                    filtered->ShipPtr[filtered->numShips++] = target->dockvars.dockship;
                    moduleDone = TRUE;
                }
                break;
            default:
                switch (target->staticinfo->shipclass)
                {
                    case CLASS_Corvette:
                    case CLASS_Fighter:
                    case CLASS_Component:
                    case CLASS_Micro:
                        break;
                    default:
                        filtered->ShipPtr[filtered->numShips++] = target;
                        break;
                }
                break;
        }
    }
    return filtered;
}

// the latch point's world position and cone normal on the target
static DockStaticPoint *leechLatchPoint(Ship *target, sdword index, vector *position, vector *normal)
{
    DockStaticPoint *point = &target->staticinfo->dockStaticInfo->dockstaticpoints[index];

    matMultiplyMatByVec(position, &target->rotatable.rotinfo.coordsys, &point->position);
    position->x += target->obj.posinfo.position.x;
    position->y += target->obj.posinfo.position.y;
    position->z += target->obj.posinfo.position.z;
    matMultiplyMatByVec(normal, &target->rotatable.rotinfo.coordsys, &point->conenormal);
    return point;
}

// the leech is at its point: it docks (no collision avoidance) and latches
static void leechStartLatch(Ship *ship, LeechSpec *spec)
{
    spec->state = LEECH_Latch;
    bitSet(ship->header.flags, SOF_CatDocking);
    bitSet(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatNoAvoidance);
}

// the chosen action becomes the drain mode (inlined three times in the exe); TRUE when the mode changed
static bool32 leechTakeAction(LeechSpec *spec)
{
    ubyte old = spec->drainMode;

    if ((spec->action == LEECH_ACTION_None) || (spec->action == LEECH_ACTION_DrainRUs) ||
        (spec->action == LEECH_ACTION_DrainHull))
    {
        spec->drainMode = spec->action;
        spec->full = 0;
    }
    return spec->drainMode != old;
}

// the target is remembered for a while (a leech with a last target does no bump damage)
static void leechRememberTarget(LeechSpec *spec)
{
    if (spec->target != NULL)
    {
        spec->lastTargetTime = universe.totaltimeelapsed + LEECH_LAST_TARGET_TIME;
        spec->lastTarget = spec->target;
    }
}

static bool32 cat_leechProcess(Ship *ship, SelectAnyCommand *targets, sbyte mode)          // 0x005b6f30
{
    LeechSpec *spec = (LeechSpec *)ship->ShipSpecifics;
    SpaceObjRotImpTarg *target = (SpaceObjRotImpTarg *)spec->target;
    DockStaticPoint *point;
    vector position, normal, diff, dest, heading, up;
    matrix coordsys;
    real32 dist, hps;

    if (mode == 0)
    {
        spec->state = LEECH_PickTarget;
        if (spec->targets != NULL)
        {
            memFree(spec->targets);
            spec->targets = NULL;
        }
        spec->targets = cat_leechFilterTargets((SelectCommand *)targets);
    }
    else if ((mode == 2) && (spec->state != LEECH_Idle))
    {
        spec->state = LEECH_Leave;
    }
    if ((target == NULL) && (spec->state != LEECH_PickTarget) && (spec->state != LEECH_Leave))
    {
        cat_leechDetach(ship);
        return spec->state == LEECH_Idle;
    }
    switch (spec->state)
    {
        case LEECH_PickTarget:
            leechFreeLatchPoint(spec);
            if (cat_leechPickTarget(ship) && cat_leechReserveLatchPoint(ship))
            {
                point = leechLatchPoint(spec->target, spec->latchPoint, &position, &normal);
                diff.x = ship->obj.posinfo.position.x - position.x;
                diff.y = ship->obj.posinfo.position.y - position.y;
                diff.z = ship->obj.posinfo.position.z - position.z;
                dist = fsqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                if (normal.x * diff.x + normal.y * diff.y + normal.z * diff.z < dist * point->coneangle)
                {
                    spec->approachDist = ClampToWithin(dist, point->mindist, point->maxdist);
                    spec->state = LEECH_Approach;
                    break;
                }
                leechStartLatch(ship, spec);
                break;
            }
            spec->state = LEECH_Leave;
            break;

        case LEECH_Approach:
            point = leechLatchPoint((Ship *)target, spec->latchPoint, &position, &normal);
            diff.x = ship->obj.posinfo.position.x - position.x;
            diff.y = ship->obj.posinfo.position.y - position.y;
            diff.z = ship->obj.posinfo.position.z - position.z;
            dist = fsqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
            if (normal.x * diff.x + normal.y * diff.y + normal.z * diff.z < dist * point->coneangle)
            {
                // fly to the cone at the approach distance
                ship->shipidle = FALSE;
                dest.x = spec->approachDist * normal.x + position.x;
                dest.y = spec->approachDist * normal.y + position.y;
                dest.z = spec->approachDist * normal.z + position.z;
                aishipFlyToPointAvoidingObjsFunc(ship, &dest, 7, -100.0f, &target->obj.posinfo.velocity);
                if ((spec->bodyOpen != 1) && MoveReachedDestinationVariable(ship, &dest, 50.0f))
                {
                    spec->bodyOpen = 1;
                    cat_madAnimationStartNumber(ship, 0);   // CAT-TODO(stage 3): the exe passes a third argument, 0
                }
                if (!MoveReachedDestinationVariable(ship, &dest, point->mindist))
                {
                    break;
                }
            }
            leechStartLatch(ship, spec);
            break;

        case LEECH_Latch:
            point = &((Ship *)target)->staticinfo->dockStaticInfo->dockstaticpoints[spec->latchPoint];
            matMultiplyMatByVec(&normal, &target->rotatable.rotinfo.coordsys, &point->conenormal);
            matCreateCoordSysFromHeading(&coordsys, &normal);
            GetDirectionVectorOfShip(&heading, point->headingdirection, &coordsys);
            GetDirectionVectorOfShip(&up, point->updirection, &coordsys);
            ship->shipidle = FALSE;
            aitrackHeadingAndUp(ship, &heading, &up, 0.9999f);
            leechLatchPoint((Ship *)target, spec->latchPoint, &position, &normal);
            dest.x = normal.x * LEECH_LATCH_OFFSET + position.x;
            dest.y = normal.y * LEECH_LATCH_OFFSET + position.y;
            dest.z = normal.z * LEECH_LATCH_OFFSET + position.z;
            aishipFlyToPointAvoidingObjsFunc(ship, &dest, 4, -35.0f, &target->obj.posinfo.velocity);
            if ((spec->bodyOpen != 1) && MoveReachedDestinationVariable(ship, &dest, 50.0f))
            {
                spec->bodyOpen = 1;
                cat_madAnimationStartNumber(ship, 0);       // CAT-TODO(stage 3): the exe passes a third argument, 0
            }
            if (!MoveReachedDestinationVariable(ship, &dest, 2.0f))
            {
                break;
            }
            // latched: clamp next housekeep, start draining
            leechTakeAction(spec);
            spec->clampPending = 1;
            spec->state = spec->drainMode;
            cat_leechDrainEffect(ship, 5);
            cat_leechDrainEffect(ship, spec->drainMode);
            break;

        case LEECH_ACTION_None:
            if (leechTakeAction(spec))
            {
                cat_leechDrainEffect(ship, spec->drainMode);
                spec->state = spec->drainMode;
            }
            break;

        case LEECH_ACTION_DrainRUs:
        case LEECH_ACTION_DrainHull:
            if (universe.totaltimeelapsed < spec->nextDrainTime)
            {
                break;
            }
            hps = ((Ship *)target)->staticinfo->leechHPsPerSecond;
            if (spec->state == LEECH_ACTION_DrainRUs)
            {
                // resource units while the target has more than two drains of hull left
                if (hps + hps < target->health)
                {
                    ship->resources = ((Ship *)target)->staticinfo->leechRUsPerSecond + ship->resources;
                    if (ship->staticinfo->maxresources < ship->resources)
                    {
                        ship->resources = ship->staticinfo->maxresources;
                    }
                }
                if (ship->staticinfo->maxresources <= ship->resources)
                {
                    spec->state = LEECH_DetachFull;
                    leechRememberTarget(spec);
                }
                if (target->health <= hps + hps)
                {
                    if (spec->state == LEECH_ACTION_DrainRUs)
                    {
                        spec->state = LEECH_DetachNext;
                        leechRememberTarget(spec);
                    }
                }
                else if (ApplyDamageToTarget(target, hps, 0, 0x29a, ship->playerowner->playerIndex, ship, 0x12))
                {
                    spec->state = LEECH_DetachFull;
                    leechRememberTarget(spec);
                }
            }
            else if (ApplyDamageToTarget(target, hps, 0, 0x29a, ship->playerowner->playerIndex, ship, 0x13))
            {
                spec->state = LEECH_DetachFull;
                leechRememberTarget(spec);
            }
            if (leechTakeAction(spec))
            {
                cat_leechDrainEffect(ship, spec->drainMode);
                spec->state = spec->drainMode;
            }
            spec->nextDrainTime = universe.totaltimeelapsed + 1.0f;
            break;

        case LEECH_Detach:
        case LEECH_DetachFull:
        case LEECH_DetachNext:
            point = &spec->target->staticinfo->dockStaticInfo->dockstaticpoints[spec->latchPoint];
            if (target != NULL)
            {
                // back off along the cone
                unClampObj((SpaceObjRotImpTargGuidance *)ship);
                leechLatchPoint((Ship *)target, spec->latchPoint, &position, &normal);
                ship->shipidle = FALSE;
                dest.x = point->mindist * normal.x + position.x;
                dest.y = point->mindist * normal.y + position.y;
                dest.z = point->mindist * normal.z + position.z;
                GetDirectionVectorOfShip(&heading, point->headingdirection, &target->rotatable.rotinfo.coordsys);
                GetDirectionVectorOfShip(&up, point->updirection, &target->rotatable.rotinfo.coordsys);
                aitrackHeadingAndUp(ship, &heading, &up, 0.9999f);
                aishipFlyToPointAvoidingObjsFunc(ship, &dest, 0, point->maxdist, &target->obj.posinfo.velocity);
            }
            if (spec->bodyOpen &&
                ((target == NULL) || MoveReachedDestinationVariable(ship, &target->obj.posinfo.position, point->mindist)))
            {
                leechCloseBody(ship, spec);
            }
            if ((target != NULL) && !MoveReachedDestinationVariable(ship, &dest, point->mindist * LEECH_LEAVE_FACTOR))
            {
                break;
            }
            if (spec->state == LEECH_Detach)
            {
                leechFreeLatchPoint(spec);
                spec->state = LEECH_Leave;
            }
            else if (spec->state == LEECH_DetachFull)
            {
                spec->returnToDock = 1;
                spec->full = 1;
                spec->state = LEECH_Leave;
                leechFreeLatchPoint(spec);
            }
            else if (spec->state == LEECH_DetachNext)
            {
                // on to the next target
                leechFreeLatchPoint(spec);
                if ((target != NULL) && (spec->targets != NULL))
                {
                    selSelectionRemoveSingleShip(spec->targets, (Ship *)target);
                }
                if (cat_leechPickTarget(ship))
                {
                    spec->state = LEECH_PickTarget;
                    break;
                }
                if (ship->resources != 0)
                {
                    spec->returnToDock = 1;
                    spec->full = 1;
                }
                spec->state = LEECH_Leave;
            }
            break;

        case LEECH_Leave:
        case LEECH_Leave2:
            leechRememberTarget(spec);
            if (spec->bodyOpen)
            {
                spec->bodyOpen = 0;
                cat_madAnimationStartNumber(ship, 1);       // CAT-TODO(stage 3): the exe passes a third argument, 2
            }
            leechFreeLatchPoint(spec);
            bitClear(ship->header.flags, SOF_CatDocking);
            bitClear(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatNoAvoidance);
            unClampObj((SpaceObjRotImpTargGuidance *)ship);
            spec->state = LEECH_Idle;
            if (spec->targets != NULL)
            {
                memFree(spec->targets);
                spec->targets = NULL;
            }
            break;

        default:
            break;
    }
    return spec->state == LEECH_Idle;
}

void LeechStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)       // 0x005b7ad0
{
    (void)directory;
    (void)filename;
    statinfo->custstatinfo = &LeechStatic;
    statinfo->canSpecialBandBoxFriendlies = TRUE;
    statinfo->unknownFlag212 = TRUE;
    statinfo->unknownFlag213 = FALSE;
}

void LeechInit(Ship *ship)                                                                  // 0x005b7b00
{
    LeechSpec *spec = (LeechSpec *)ship->ShipSpecifics;

    spec->state = LEECH_Idle;
    spec->drainMode = LEECH_ACTION_DrainHull;
    spec->bodyOpen = 0;
    spec->target = NULL;
    spec->latchPoint = -1;
    spec->full = 0;
    spec->returnToDock = 0;
    spec->returning = 0;
    spec->nextEffectTime = 0.0f;
    spec->clampPending = 0;
    spec->action = LEECH_ACTION_DrainHull;
    spec->lastTargetTime = 0.0f;
    spec->lastTarget = NULL;
    spec->targets = NULL;
    spec->nextDrainTime = 0.0f;
    spec->released = 0;
}

void LeechClose(Ship *ship)                                                                 // 0x005b7b70
{
    LeechSpec *spec = (LeechSpec *)ship->ShipSpecifics;

    if (spec->targets != NULL)
    {
        memFree(spec->targets);
        spec->targets = NULL;
    }
}

void LeechDied(Ship *ship)                                                                  // 0x005b7ba0
{
    LeechSpec *spec = (LeechSpec *)ship->ShipSpecifics;

    leechFreeLatchPoint(spec);
    if (spec->targets != NULL)
    {
        memFree(spec->targets);
        spec->targets = NULL;
    }
}

bool32 LeechSpecialTarget(Ship *ship,void *custom)                                          // 0x005b7c00
{
    LeechSpec *spec = (LeechSpec *)ship->ShipSpecifics;
    SelectAnyCommand *targets = (SelectAnyCommand *)custom;
    sbyte mode;

    if ((targets->numTargets == 0) || !cat_shipHasUpgrade(ship, CAT_UPGRADE_Leech) || (targets->numTargets < 1))
    {
        return TRUE;
    }
    mode = ((spec->state == LEECH_Idle) || (spec->state == LEECH_Done)) ? 0 : 1;
    return cat_leechProcess(ship, targets, mode);
}

void LeechOrderChanged(Ship *ship)                                                          // 0x005b7c70
{
    cat_leechProcess(ship, NULL, 2);
}

void LeechRemoveShipReferences(Ship *ship,Ship *shiptoremove)                               // 0x005b7c90
{
    LeechSpec *spec = (LeechSpec *)ship->ShipSpecifics;

    if (spec->targets != NULL)
    {
        selSelectionRemoveSingleShip(spec->targets, shiptoremove);
    }
    if (shiptoremove == spec->target)
    {
        cat_leechDetach(ship);
    }
    if (shiptoremove == spec->lastTarget)
    {
        spec->lastTargetTime = 0.0f;
        spec->lastTarget = NULL;
    }
}

bool32 cat_leechIsLeeching(Ship *ship)                                                      // 0x005b7ce0
{
    return ((LeechSpec *)ship->ShipSpecifics)->state != LEECH_Idle;
}

// an aggressive leech with no order takes the enemies near it as its targets
static void cat_leechAutoTarget(Ship *ship)                                                 // 0x005b7d00
{
    SelectCommand *enemies;
    SelectAnyCommand *targets;
    SelectCommand selectone;
    sdword i;

    if ((ship->command != NULL) || ((enemies = getEnemiesWithinProximity(ship, LEECH_AUTO_TARGET_RANGE)) == NULL) ||
        (enemies->numShips <= 0))
    {
        return;
    }
    selectone.numShips = 1;
    selectone.timeLastStatus = 0.0f;
    selectone.ShipPtr[0] = ship;
    targets = memAlloc(sizeofSelectCommand(enemies->numShips), "LeechAuto", 0);
    for (i = 0; i < enemies->numShips; i++)
    {
        targets->TargetPtr[i] = (TargetPtr)enemies->ShipPtr[i];
    }
    targets->numTargets = enemies->numShips;
    clSpecial(&universe.mainCommandLayer, &selectone, targets);
    memFree(targets);
}

void LeechHouseKeep(Ship *ship)                                                             // 0x005b7da0
{
    LeechSpec *spec = (LeechSpec *)ship->ShipSpecifics;
    SelectCommand selectone;
    Ship *dockwith;

    if ((spec->lastTargetTime != 0.0f) && (spec->lastTargetTime < universe.totaltimeelapsed))
    {
        spec->lastTargetTime = 0.0f;
        spec->lastTarget = NULL;
    }
    // the local player's "no leech target" message, at most every 2 s
    if ((ship->playerowner == universe.curPlayerPtr) && (leechMsgDue != 0.0f) && (leechMsgDue < universe.realtimeelapsed))
    {
        leechMsgQuietUntil = universe.realtimeelapsed + 2.0f;
        leechMsgDue = 0.0f;
        if (leechMsgNoTarget && !leechMsgTargetFound)
        {
            // CAT-TODO(stage 3): clCommandMessage(the "no target" string 0x00bb507c, colour 0xff2e3def)
        }
        leechMsgNoTarget = 0;
        leechMsgTargetFound = 0;
    }
    if (spec->clampPending)
    {
        spec->clampPending = 0;
        if (spec->target != NULL)
        {
            clampObjToObj((SpaceObjRotImpTargGuidance *)ship, (SpaceObjRotImpTargGuidance *)spec->target);
        }
    }
    if (spec->returnToDock)
    {
        spec->returnToDock = 0;
        spec->returning = 1;
        // CAT: the exe passes a memAlloc'd one-ship selection it never frees; a stack one here. Purpose 3
        //      (LeechHouseKeep 0x005b7270; M2-R5 dock)
        selectone.numShips = 1;
        selectone.ShipPtr[0] = ship;
        dockwith = FindNearestShipToDockAt(ship, CAT_DOCK_DEPOSIT_RESOURCES);
        clWrapDock(&universe.mainCommandLayer, &selectone, CAT_DOCK_DEPOSIT_RESOURCES, dockwith);
    }
    if ((ship->resources < 1) && spec->returning)
    {
        spec->returning = 0;
        if (spec->full)
        {
            spec->full = 0;
        }
    }
    if ((ship->command != NULL) && (spec->state == LEECH_Idle) && (ship->command->ordertype.order == COMMAND_SPECIAL))
    {
        selectone.numShips = 1;
        selectone.ShipPtr[0] = ship;
        RemoveShipsFromDoingStuff(&universe.mainCommandLayer, &selectone,0x2C);  // CAT: reason 0x2C as the exe (RemoveShipsFromDoingStuff 0x00444810)
    }
    if ((ship->tacticstype == Aggressive) && ((universe.univUpdateCounter & 0xf) == 0xe) &&
        ((ship->command == NULL) || (ship->command->ordertype.order != COMMAND_SPECIAL)))
    {
        cat_leechAutoTarget(ship);
    }
    if ((spec->state == LEECH_ACTION_DrainHull) && (spec->nextEffectTime < universe.totaltimeelapsed))
    {
        cat_leechDrainEffect(ship, LEECH_ACTION_DrainHull);
    }
    if (spec->released)
    {
        spec->released = 0;
        selectone.numShips = 1;
        selectone.ShipPtr[0] = ship;
        RemoveShipsFromDoingStuff(&universe.mainCommandLayer, &selectone,0x515);  // CAT: reason 0x515 as the exe (RemoveShipsFromDoingStuff 0x00444810)
    }
}

bool32 cat_shipIsLeech(Ship *ship)                                                          // 0x005b7fd0
{
    return ship->shiptype == sLeech;
}

LeechSpec *cat_leechGetSpecifics(Ship *ship)                                                // 0x005b7ff0
{
    return (LeechSpec *)ship->ShipSpecifics;
}

void Leech_PreFix(Ship *ship)                                                               // 0x005b8000
{
    LeechSpec *spec = (LeechSpec *)ship->ShipSpecifics;

    spec->target = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->target);
    spec->lastTarget = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->lastTarget);
}

void Leech_Save(Ship *ship)                                                                 // 0x005b8030
{
    LeechSpec *spec = (LeechSpec *)ship->ShipSpecifics;

    if (spec->targets != NULL)
    {
        SaveSelection((SpaceObjSelection *)spec->targets);
    }
}

void Leech_Load(Ship *ship)                                                                 // 0x005b8050
{
    LeechSpec *spec = (LeechSpec *)ship->ShipSpecifics;
    SpaceObjSelection *loaded;
    sdword i;

    // the exe reads the chunk into a new 0xFA8-byte selection (cat_saveLoadMaxSelection 0x00501e70)
    if (spec->targets != NULL)
    {
        spec->targets = memAlloc(sizeof(MaxSelection), "LeechTargets", NonVolatile);
        loaded = LoadSelection();
        spec->targets->numShips = loaded->numSpaceObjs;
        for (i = 0; i < loaded->numSpaceObjs; i++)
        {
            spec->targets->ShipPtr[i] = (Ship *)loaded->SpaceObjPtr[i];
        }
        memFree(loaded);
    }
}

void Leech_Fix(Ship *ship)                                                                  // 0x005b8080
{
    LeechSpec *spec = (LeechSpec *)ship->ShipSpecifics;
    sdword i;

    spec->target = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->target);
    spec->lastTarget = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->lastTarget);
    if (spec->targets != NULL)
    {
        for (i = 0; i < spec->targets->numShips; i++)
        {
            spec->targets->ShipPtr[i] = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->targets->ShipPtr[i]);
        }
    }
}

// CAT: LeechHeader 0x008c4c60 (sLeech)
CustShipHeader LeechHeader =
{
    .sizeofShipSpecifics = sizeof(LeechSpec),
    .CustShipStaticInit = LeechStaticInit,
    .CustShipInit = LeechInit,
    .CustShipClose = LeechClose,
    .CustShipSpecialTarget = LeechSpecialTarget,
    .CustShipOrderChanged = LeechOrderChanged,
    .CustShipHousekeep = LeechHouseKeep,
    .CustShipRemoveShipReferences = LeechRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))LeechRemoveShipReferences,
    .CustShipDied = LeechDied,
    .CustShip_PreFix = Leech_PreFix,
    .CustShip_Save = Leech_Save,
    .CustShip_Load = Leech_Load,
    .CustShip_Fix = Leech_Fix,
};
