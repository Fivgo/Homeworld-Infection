// =============================================================================
//  Worker.c
//  - Cataclysm's sWorker / bWorker: harvests resources with its jack, repairs
//    friendly ships and salvages (cataclysm.exe 0x005c7ac0-0x005c9bf0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each);
//      cat_workerHarvestsResource has no Ghidra function (the code at 0x005c9130, read from the disassembly).
//      cat_shipIsLiveWorker 0x005c8e00 moved here from CommandLayer.c (its declaration stays in CommandLayer.h).
//      cat_vecWorldToShipLocal 0x005c85b0 of this block is in Dissolve.c (its first caller).

#include "Worker.h"

#include <math.h>

#include "AIShip.h"
#include "AITrack.h"
#include "Alliance.h"
#include "Battle.h"
#include "CommandLayer.h"
#include "Dock.h"
#include "ETG.h"
#include "FastMath.h"
#include "Globals.h"
#include "LOD.h"
#include "Matrix.h"
#include "Mesh.h"
#include "MeshAnim.h"
#include "ModuleShip.h"
#include "ResCollect.h"
#include "SaveGame.h"
#include "SinglePlayer.h"
#include "StatScript.h"
#include "Tweak.h"
#include "UnivUpdate.h"
#include "Universe.h"

#define WORKER_DEG_TO_HALF_RAD      (0.5f * 0.01745329238474369f)   // 0x0072a4c8 x 0x0072a4f8
#define WORKER_BEAM_TAN_MIN         0.1745329238474369              // 0x0072c028 (double): 10 degrees
#define WORKER_BEAM_TAN_MAX         0.45378560200333595             // 0x0072c020 (double): 26 degrees
#define WORKER_STANDOFF_SCALE       5.0f        // 0x0072a624
#define WORKER_STANDOFF_MAX         1500.0f     // 0x0072bf80
#define WORKER_STANDOFF_MIN         800.0f      // 0x0072bfe8
#define WORKER_FACE_ACCURACY        0.99f
#define WORKER_FACE_CONE            90.0f       // 0x008c6b48: degrees
#define WORKER_TUTORIAL_REPAIR      10.0f       // 0x0072a4f0
#define WORKER_HARVEST_REACH        2000.0f     // 0x44fa0000
#define WORKER_HARVEST_KEEP         3000.0f     // 0x453b8000
#define WORKER_NEVER                1.0e21f     // 0x6258d727

static WorkerStatics WorkerStaticSect;          // 0x00a78d78
static WorkerStatics WorkerStaticBeast;         // 0x00a78d48

static scriptStructEntry WorkerStaticScriptTable[] =
{
    { "jackRetractTime", scriptSetReal32CB, &(WorkerStaticSect.jackRetractTime), &(WorkerStaticSect) },

    END_SCRIPT_STRUCT_ENTRY
};

static scriptStructEntry WorkerRepairScriptTable[] =
{
    { "repairDistance", scriptSetReal32CB, &(WorkerStaticSect.repairDistance), &(WorkerStaticSect.repairDistance) },
    { "repairCone", scriptSetReal32CB, &(WorkerStaticSect.repairCone), &(WorkerStaticSect.repairDistance) },
    { "repairBeamWidth", scriptSetReal32CB, &(WorkerStaticSect.repairBeamWidth), &(WorkerStaticSect.repairDistance) },

    END_SCRIPT_STRUCT_ENTRY
};

// the beam width factors (cat_workerRepairEffect computes them once: 2 tan 10 degrees, 2 tan 26 degrees)
static bool32 workerBeamFactorsSet = FALSE;     // 0x00a78d40
static real32 workerBeamMinFactor;              // 0x008c6b44
static real32 workerBeamMaxFactor;              // 0x008c6b40

// does the exe's animation number play now (madanim.nCurrentAnim holds the number since M2-R6 nis-fow-camera)
static bool32 workerAnimIs(Ship *ship, sdword number)
{
    return (ship->madBindings->nCurrentAnim == number);
}

// the exe's madanim +0x14 (set while an animation holds at its end)
static bool32 workerAnimPaused(Ship *ship)
{
    return ship->madBindings->bPaused;
}

static void cat_workerStaticInitRepairPoints(char *directory, char *field, ShipStaticInfo *statinfo)    // 0x005c7ac0
{
    WorkerStatics *wstat = (WorkerStatics *)statinfo->custstatinfo;
    DockStaticPoint *point;

    wstat->repairDistance = 0.0f;
    wstat->repairCone = 0.0f;
    wstat->repairBeamWidth = 0.0f;
    wstat->repairConeCos = 0.0f;
    vecZeroVector(wstat->repairBeamOrigin);
    scriptSetStruct(directory, field, WorkerRepairScriptTable, &wstat->repairDistance);
    // (a missing key reaches a debug hook only)
    wstat->repairConeCos = (real32)cos((double)(wstat->repairCone * WORKER_DEG_TO_HALF_RAD));
    point = cat_dockFindDockStaticPoint("gun0", statinfo->dockStaticInfo);
    wstat->repairBeamOrigin = point->position;
    statinfo->canSpecialBandBoxFriendlies = TRUE;
    statinfo->unknownFlag212 = TRUE;
    statinfo->unknownFlag213 = TRUE;
    statinfo->canSingleClickSpecialActivate = TRUE;
}

// among ships at full health: the nearest one with the fewest idle Workers sent to it, else the first nearer one
static Ship *cat_workerPickLeastBusyTarget(Ship *worker, SelectCommand *ships)              // 0x005c7c60
{
    sdword fewest = 0x7fffffff;
    sdword pick = -1;
    sdword second = -1;
    real32 best = WORKER_NEVER;
    real32 distsq;
    vector diff;
    sdword i;

    for (i = 0; i < ships->numShips; i++)
    {
        if (ships->ShipPtr[i]->catRepairIdleWorkers <= fewest)
        {
            fewest = ships->ShipPtr[i]->catRepairIdleWorkers;
        }
    }
    for (i = 0; i < ships->numShips; i++)
    {
        vecSub(diff, ships->ShipPtr[i]->obj.posinfo.position, worker->obj.posinfo.position);
        distsq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        if (distsq < best)
        {
            if (ships->ShipPtr[i]->catRepairIdleWorkers == fewest)
            {
                pick = i;
                best = distsq;
            }
            else if ((pick == -1) && (second == -1))
            {
                second = i;
            }
        }
    }
    if (pick != -1)
    {
        return ships->ShipPtr[pick];
    }
    if (second != -1)
    {
        return ships->ShipPtr[second];
    }
    return NULL;
}

// the damaged allied ship to repair: the nearest one no Worker repairs yet, else the first nearer one; with none
//      damaged, the least busy one at full health (*idle TRUE). Ships that cannot be repaired are taken off the list.
static Ship *cat_workerChooseRepairTarget(Ship *worker, SelectCommand *ships, bool32 *idle)    // 0x005c7d60
{
    MaxSelection full;
    Ship *ship;
    Ship *part;
    Ship *module;
    sdword pick = -1;
    sdword second = -1;
    sdword i = 0;
    real32 best = WORKER_NEVER;
    real32 fraction, mfraction, distsq;
    vector diff;

    full.numShips = 0;
    *idle = FALSE;
    while (i < ships->numShips)
    {
        ship = ships->ShipPtr[i];
        if ((ship->header.objtype != OBJ_ShipType) || !allianceIsShipAlly(ship, worker->playerowner) ||
            (twRepairRate[ship->staticinfo->shipclass] <= 0.0f))
        {
            ships->numShips--;
            ships->ShipPtr[i] = ships->ShipPtr[ships->numShips];
            continue;
        }
        if (!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
        {
            fraction = ship->targetable.health / ship->staticinfo->health.maxhealth;
            part = ship;
            for (module = cat_moduleIterFirst(ship); module != NULL; module = cat_moduleIterNext())
            {
                mfraction = module->targetable.health / module->staticinfo->health.maxhealth;
                if (mfraction < fraction)
                {
                    part = module;
                    fraction = mfraction;
                }
            }
            if (part->staticinfo->health.maxhealth <= part->targetable.health)
            {
                full.ShipPtr[full.numShips] = ship;
                full.numShips++;
            }
            else
            {
                vecSub(diff, ship->obj.posinfo.position, worker->obj.posinfo.position);
                distsq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
                if (distsq < best)
                {
                    if (ship->catRepairWorkers == 0)
                    {
                        pick = i;
                        best = distsq;
                    }
                    else if ((pick == -1) && (second == -1))
                    {
                        second = i;
                    }
                }
            }
        }
        i++;
    }
    if (pick != -1)
    {
        return ships->ShipPtr[pick];
    }
    if (second != -1)
    {
        return ships->ShipPtr[second];
    }
    if (full.numShips > 0)
    {
        *idle = TRUE;
        return cat_workerPickLeastBusyTarget(worker, (SelectCommand *)&full);
    }
    return NULL;
}

// a box is { centre x, y, z, extent x, y, z } (the extents are the collision box's full lengths)
static void cat_boxUnion(real32 *out, real32 *a, real32 *b)                                 // 0x005c7f80
{
    real32 hix, hiy, hiz, lox, loy, loz;

    hix = b[0] + b[3]; if (hix < a[0] + a[3]) hix = a[0] + a[3];
    hiy = b[1] + b[4]; if (hiy < a[1] + a[4]) hiy = a[1] + a[4];
    hiz = b[2] + b[5]; if (hiz < a[2] + a[5]) hiz = a[2] + a[5];
    lox = a[0] - a[3]; if (b[0] - b[3] <= a[0] - a[3]) lox = b[0] - b[3];
    loy = a[1] - a[4]; if (b[1] - b[4] <= a[1] - a[4]) loy = b[1] - b[4];
    loz = a[2] - a[5]; if (b[2] - b[5] <= a[2] - a[5]) loz = b[2] - b[5];
    out[3] = (hix - lox) * 0.5f;
    out[4] = (hiy - loy) * 0.5f;
    out[5] = (hiz - loz) * 0.5f;
    out[0] = out[3] + lox;
    out[1] = out[4] + loy;
    out[2] = out[5] + loz;
}

static void workerStaticBox(ShipStaticInfo *statinfo, real32 *box)
{
    StaticCollInfo *coll = &statinfo->common.staticCollInfo;

    box[0] = coll->uplength * 0.5f + coll->collrectoffset.x;
    box[1] = coll->rightlength * 0.5f + coll->collrectoffset.y;
    box[2] = coll->forwardlength * 0.5f + coll->collrectoffset.z;
    box[3] = coll->uplength;
    box[4] = coll->rightlength;
    box[5] = coll->forwardlength;
}

// the collision box of a ship and its modules, in the ship's frame
static void cat_shipGetBoundingBox(Ship *ship, real32 *box)                                 // 0x005c80d0
{
    real32 modbox[6];
    Ship *module;

    workerStaticBox(ship->staticinfo, box);
    for (module = cat_moduleIterFirst(ship); module != NULL; module = cat_moduleIterNext())
    {
        workerStaticBox(module->staticinfo, modbox);
        cat_boxUnion(box, box, modbox);
    }
}

// the distance from a point to the box of ship (a coordinate inside the box counts as itself, as in the exe)
static real32 cat_shipPointOutsideBoxDistance(vector *point, Ship *ship)                    // 0x005c81e0
{
    vector diff;
    vector local;
    real32 box[6];

    vecSub(diff, *point, ship->obj.posinfo.position);
    matMultiplyVecByMat(&local, &diff, &ship->rotatable.rotinfo.coordsys);
    cat_shipGetBoundingBox(ship, box);
    if (local.x < box[0] - box[3])      local.x = (box[0] - box[3]) - local.x;
    else if (box[3] + box[0] < local.x) local.x = (box[3] + box[0]) - local.x;
    if (local.y < box[1] - box[4])      local.y = (box[1] - box[4]) - local.y;
    else if (box[4] + box[1] < local.y) local.y = (box[4] + box[1]) - local.y;
    if (local.z < box[2] - box[5])      local.z = (box[2] - box[5]) - local.z;
    else if (box[5] + box[2] < local.z) local.z = (box[5] + box[2]) - local.z;
    return fsqrt(local.x * local.x + local.y * local.y + local.z * local.z);
}

static void cat_workerRepairEffect(Ship *worker)                                            // 0x005c8310
{
    WorkerSpec *spec = (WorkerSpec *)worker->ShipSpecifics;
    WorkerStatics *wstat = (WorkerStatics *)worker->staticinfo->custstatinfo;
    Ship *target = spec->repairTarget;
    etglod *etgLOD = etgSpecialPurposeEffectTable[EGT_REPAIR_BEAM];
    etgeffectstatic *stat;
    vector trajectory, pos;
    matrix coordsys;
    real32 dist, width, wmax, wcap;

    if ((etgLOD != NULL) && (worker->obj.currentLOD < etgLOD->nLevels) &&
        ((stat = etgLOD->level[worker->obj.currentLOD]) != NULL) && !etgFrequencyExceeded(stat))
    {
        aishipGetTrajectory(worker, (SpaceObjRotImpTarg *)target, &trajectory);
        matCreateCoordSysFromHeading(&coordsys, &trajectory);
        matMultiplyMatByVec(&pos, &worker->rotatable.rotinfo.coordsys, &wstat->repairBeamOrigin);
        vecAddTo(pos, worker->obj.posinfo.position);
        dist = cat_shipPointOutsideBoxDistance(&pos, target);
        if (!workerBeamFactorsSet)
        {
            workerBeamFactorsSet = TRUE;
            workerBeamMinFactor = (real32)(tan(WORKER_BEAM_TAN_MIN) * 2.0);
            workerBeamMaxFactor = (real32)(tan(WORKER_BEAM_TAN_MAX) * 2.0);
        }
        // max(2 tan 10 x d, min(2 tan 26 x d, the target's diameter))
        width = workerBeamMinFactor * dist;
        wcap = target->staticinfo->common.staticCollInfo.collspheresize + target->staticinfo->common.staticCollInfo.collspheresize;
        wmax = workerBeamMaxFactor * dist;
        if (width <= ((wcap < wmax) ? wcap : wmax))
        {
            width = (wmax <= wcap) ? wmax : wcap;
        }
        spec->repairEffectOn = TRUE;
        spec->repairEffectDist = dist;
        spec->repairEffect = etgEffectCreate(stat, worker, &pos, &worker->obj.posinfo.velocity, &coordsys, 1.0f,
                                             SOF_AttachCoordsys | SOF_AttachVelocity | SOF_AttachNLips, 2,
                                             SCALECAST(Real32ToUdword(dist)), SCALECAST(Real32ToUdword(width)));
        return;
    }
    spec->repairEffect = NULL;
}

static void cat_workerStopRepairEffect(Ship *worker)                                        // 0x005c84c0
{
    WorkerSpec *spec = (WorkerSpec *)worker->ShipSpecifics;

    if (spec->repairEffect != NULL)
    {
        etgEffectDelete(spec->repairEffect);
        univRemoveObjFromRenderList(&spec->repairEffect->obj);
        listDeleteNode(&spec->repairEffect->header.objlink);
        spec->repairEffectOn = FALSE;
        spec->repairEffect = NULL;
        spec->repairEffectDist = 0.0f;
    }
}

// let go of the repair target and stop the beam (cat_workerReleaseRepairTarget 0x005c8510; inlined in most of the
//      Worker's slots)
static void cat_workerReleaseRepairTarget(Ship *worker)                                     // 0x005c8510
{
    WorkerSpec *spec = (WorkerSpec *)worker->ShipSpecifics;

    if (spec->repairTarget != NULL)
    {
        spec->repairTarget->catRepairWorkers--;
        if (worker->catSalvageBigGun == spec->repairTarget)
        {
            worker->catSalvageBigGun = NULL;
        }
        spec->repairTarget = NULL;
    }
    if (spec->repairEffectOn)
    {
        cat_workerStopRepairEffect(worker);
        spec->repairEffect = NULL;
        spec->repairEffectOn = FALSE;
        spec->repairEffectDist = 0.0f;
    }
    spec->repairState = WORKER_REPAIR_Choose;
}

// is the worker within its standoff (target radius x 5, 800 .. 1500) of the target's box
static bool32 cat_workerComputeRepairStandoff(Ship *worker)                                 // 0x005c8600
{
    Ship *target = ((WorkerSpec *)worker->ShipSpecifics)->repairTarget;
    real32 standoff = target->staticinfo->common.staticCollInfo.collspheresize * WORKER_STANDOFF_SCALE;
    vector diff, local;
    real32 box[6];

    if (WORKER_STANDOFF_MAX <= standoff)
    {
        standoff = WORKER_STANDOFF_MAX;
    }
    else if (standoff < WORKER_STANDOFF_MIN)
    {
        standoff = WORKER_STANDOFF_MIN;
    }
    vecSub(diff, worker->obj.posinfo.position, target->obj.posinfo.position);
    matMultiplyVecByMat(&local, &diff, &target->rotatable.rotinfo.coordsys);
    cat_shipGetBoundingBox(target, box);
    return (box[0] - (standoff + box[3]) < local.x) && (local.x < box[0] + standoff + box[3]) &&
           (box[1] - (standoff + box[4]) < local.y) && (local.y < box[1] + standoff + box[4]) &&
           (box[2] - (standoff + box[5]) < local.z) && (local.z < box[2] + standoff + box[5]);
}

// fly to the repair standoff, keep the beam's length, face the target; TRUE in place and within the repair cone
static bool32 cat_workerFlyToRepairPoint(Ship *worker)                                      // 0x005c8740
{
    WorkerSpec *spec = (WorkerSpec *)worker->ShipSpecifics;
    WorkerStatics *wstat = (WorkerStatics *)worker->staticinfo->custstatinfo;
    Ship *target = spec->repairTarget;
    bool32 inPlace = cat_workerComputeRepairStandoff(worker);
    vector dir, away;
    real32 dot, cone, dist, scale;

    vecSub(dir, target->obj.posinfo.position, worker->obj.posinfo.position);
    vecNormalize(&dir);
    dot = worker->rotatable.rotinfo.coordsys.m13 * dir.x + worker->rotatable.rotinfo.coordsys.m23 * dir.y +
          worker->rotatable.rotinfo.coordsys.m33 * dir.z;
    cone = wstat->repairConeCos;
    if (!inPlace)
    {
        aishipFlyToPointAvoidingObjsFunc(worker, &target->obj.posinfo.position, 0, 0.0f, NULL);
    }
    else
    {
        if (spec->repairEffectOn)
        {
            vecSub(away, worker->obj.posinfo.position, target->obj.posinfo.position);
            dist = fsqrt(away.x * away.x + away.y * away.y + away.z * away.z);
            if (dist < spec->repairEffectDist)
            {
                // back off to the beam's starting length
                scale = (1.0f / dist) * (spec->repairEffectDist - dist);
                away.x = scale * away.x + worker->obj.posinfo.position.x;
                away.y = scale * away.y + worker->obj.posinfo.position.y;
                away.z = scale * away.z + worker->obj.posinfo.position.z;
                aishipFlyToPointAvoidingObjsFunc(worker, &away, 0, 0.0f, NULL);
                goto face;
            }
        }
        aitrackForceZeroVelocity(worker);
        aitrackVelocityVectorGuidance((SpaceObjRotImpTargGuidance *)worker, &target->obj.posinfo.velocity);
    }
face:
    vecSub(dir, target->obj.posinfo.position, worker->obj.posinfo.position);
    vecNormalize(&dir);
    if (aitrackHeadingFunc((SpaceObjRotImpTargGuidance *)worker, &dir, WORKER_FACE_ACCURACY, WORKER_FACE_ACCURACY, 0.0f, 0))
    {
        aitrackZeroRotationAnywhere(worker);
    }
    return inPlace && (cone <= dot);
}

// the ship of host and its modules with the lowest health fraction
Ship *cat_shipFindMostDamagedPart(Ship *ship)                                               // 0x005c8940
{
    real32 fraction = ship->targetable.health / ship->staticinfo->health.maxhealth;
    real32 mfraction;
    Ship *part = ship;
    Ship *module;

    for (module = cat_moduleIterFirst(ship); module != NULL; module = cat_moduleIterNext())
    {
        mfraction = module->targetable.health / module->staticinfo->health.maxhealth;
        if (mfraction < fraction)
        {
            part = module;
            fraction = mfraction;
        }
    }
    return part;
}

// the jack: animation 0 starts unless it plays (and still runs); the retract time moves on (the exe's inlined block)
static void workerJackOut(Ship *worker)
{
    WorkerSpec *spec = (WorkerSpec *)worker->ShipSpecifics;
    WorkerStatics *wstat = (WorkerStatics *)worker->staticinfo->custstatinfo;

    if (!workerAnimIs(worker, 0))
    {
        if (!workerAnimIs(worker, -1) && !workerAnimPaused(worker))
        {
            return;
        }
        madAnimationStart(worker, 0, 0);      // CAT: the exe's third argument, 0 (M2-R6 nis-fow-camera)
    }
    spec->jackRetractTime = universe.totaltimeelapsed + wstat->jackRetractTime;
}

static bool32 cat_workerRepairTarget(Ship *worker, SelectCommand *targets)                  // 0x005c89a0
{
    WorkerSpec *spec = (WorkerSpec *)worker->ShipSpecifics;
    WorkerStatics *wstat = (WorkerStatics *)worker->staticinfo->custstatinfo;
    Ship *target;
    Ship *host;
    Ship *part;
    bool32 idle;
    real32 rate, add, health;
    vector pos;

    if (bitTest(worker->header.flags, SOF_Disabled | SOF_Hide | SOF_Dead) || worker->catEmpStunned)
    {
        cat_workerReleaseRepairTarget(worker);
        return FALSE;
    }
    switch (spec->repairState)
    {
        case WORKER_REPAIR_Choose:
            if ((targets == NULL) || (targets->numShips < 1))
            {
                spec->repairState = WORKER_REPAIR_Done;
                return FALSE;
            }
            target = cat_workerChooseRepairTarget(worker, targets, &idle);
            if (target == NULL)
            {
                return FALSE;
            }
            if (idle)
            {
                // every ship is at full health: fly once towards the least busy one and let go
                target->catRepairIdleWorkers++;
                worker->catSalvageBigGun = target;
                spec->repairTarget = target;
                cat_workerFlyToRepairPoint(worker);
                // (the exe's release takes from catRepairWorkers although catRepairIdleWorkers was raised; kept)
                cat_workerReleaseRepairTarget(worker);
                return FALSE;
            }
            target->catRepairWorkers++;
            worker->catSalvageBigGun = target;
            spec->repairTarget = target;
            spec->repairState = WORKER_REPAIR_Approach;
            return FALSE;
        case WORKER_REPAIR_Approach:
            if (cat_workerFlyToRepairPoint(worker))
            {
                spec->repairState = WORKER_REPAIR_Jack;
            }
            return FALSE;
        case WORKER_REPAIR_Jack:
            cat_workerFlyToRepairPoint(worker);
            if (workerAnimIs(worker, 0))
            {
                spec->jackRetractTime = universe.totaltimeelapsed + wstat->jackRetractTime;
                if (workerAnimPaused(worker))
                {
                    cat_workerRepairEffect(worker);
                    spec->repairState = WORKER_REPAIR_Repair;
                }
            }
            else if (workerAnimIs(worker, -1) || workerAnimPaused(worker))
            {
                madAnimationStart(worker, 0, 0);  // CAT: the exe's third argument, 0 (M2-R6 nis-fow-camera)
                spec->jackRetractTime = universe.totaltimeelapsed + wstat->jackRetractTime;
            }
            return FALSE;
        case WORKER_REPAIR_Repair:
            break;
        case WORKER_REPAIR_Done:
            cat_workerReleaseRepairTarget(worker);
            return TRUE;
        default:
            return TRUE;
    }

    // repairing: the beam follows the worker, the most damaged part of the target gains twRepairRate per second
    if (spec->repairEffect != NULL)
    {
        matMultiplyMatByVec(&pos, &worker->rotatable.rotinfo.coordsys, &wstat->repairBeamOrigin);
        vecAddTo(pos, worker->obj.posinfo.position);
        spec->repairEffect->obj.posinfo.position = pos;
    }
    cat_workerFlyToRepairPoint(worker);
    workerJackOut(worker);
    host = spec->repairTarget;
    rate = twRepairRate[host->staticinfo->shipclass];
    part = cat_shipFindMostDamagedPart(host);
    add = universe.phystimeelapsed * rate;
    if (tutorial == TUTORIAL_ONLY)
    {
        add = add * WORKER_TUTORIAL_REPAIR;
    }
    health = add + part->targetable.health;
    part->targetable.health = health;
    health = (part->staticinfo->health.maxhealth <= health) ? part->staticinfo->health.maxhealth : health;
    part->targetable.health = health;
    if (health == part->staticinfo->health.maxhealth)
    {
        if (battleCanChatterAtThisTime(0x52, worker))
        {
            battleChatterAttempt(-1, 0x52, worker, worker->shiptype);
        }
        cat_workerStopRepairEffect(worker);
        cat_workerReleaseRepairTarget(worker);
    }
    return FALSE;
}

bool32 cat_shipIsLiveWorker(Ship *ship)                                                     // 0x005c8e00
{
    return (ship->header.objtype == OBJ_ShipType) && ((ship->shiptype == sWorker) || (ship->shiptype == bWorker));
}

// the harvest beam from point to target (a resource), made or moved each update (ship->rceffect)
void cat_workerBeamEffectUpdate(Ship *ship, SpaceObj *target, sdword effectIndex, DockStaticPoint *point)  // 0x005c8e30
{
    etglod *etgLOD = etgSpecialPurposeEffectTable[effectIndex];
    etgeffectstatic *stat;
    vector pos, dir;
    matrix coordsys;
    real32 dist, scale, width;

    matMultiplyMatByVec(&pos, &ship->rotatable.rotinfo.coordsys, &point->position);
    pos.x = universe.phystimeelapsed * ship->obj.posinfo.velocity.x + pos.x + ship->obj.posinfo.position.x;
    pos.y = universe.phystimeelapsed * ship->obj.posinfo.velocity.y + pos.y + ship->obj.posinfo.position.y;
    pos.z = universe.phystimeelapsed * ship->obj.posinfo.velocity.z + pos.z + ship->obj.posinfo.position.z;
    vecSub(dir, target->posinfo.position, pos);
    dist = fsqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    scale = 1.0f / dist;
    vecMultiplyByScalar(dir, scale);
    matCreateCoordSysFromHeading(&coordsys, &dir);
    width = ((StaticInfo *)target->staticinfo)->staticCollInfo.avoidcollspheresize * 0.5f;
    if ((etgLOD == NULL) || (etgLOD->nLevels <= ship->obj.currentLOD))
    {
        stat = NULL;
    }
    else
    {
        stat = etgLOD->level[ship->obj.currentLOD];
    }
    if (ship->rceffect == NULL)
    {
        if ((stat != NULL) && !etgFrequencyExceeded(stat))
        {
            ship->rceffect = etgEffectCreate(stat, NULL, &pos, &ship->obj.posinfo.velocity, &coordsys, 1.0f,
                                             SOF_AttachVelocity, 2, SCALECAST(Real32ToUdword(dist)),
                                             SCALECAST(Real32ToUdword(width)));
            return;
        }
        ship->rceffect = NULL;
        return;
    }
    ship->rceffect->obj.posinfo.position = pos;
    ship->rceffect->rotatable.rotinfo.coordsys = coordsys;
}

// is target within 45 degrees of the heading; unless onlyCheck, turn to it (or stop turning once facing it)
static bool32 cat_workerTurnTowardTarget(Ship *ship, SpaceObj *target, bool32 onlyCheck)   // 0x005c9010
{
    real32 cosLimit = (real32)cos((double)(WORKER_FACE_CONE * WORKER_DEG_TO_HALF_RAD));
    vector dir;
    bool32 facing;

    vecSub(dir, target->posinfo.position, ship->obj.posinfo.position);
    vecNormalize(&dir);
    facing = (cosLimit <= ship->rotatable.rotinfo.coordsys.m13 * dir.x + ship->rotatable.rotinfo.coordsys.m23 * dir.y +
                          ship->rotatable.rotinfo.coordsys.m33 * dir.z);
    if (!onlyCheck)
    {
        if (facing)
        {
            vecZeroVector(ship->rotatable.rotinfo.rotspeed);
            vecZeroVector(ship->rotatable.rotinfo.torque);
            return TRUE;
        }
        vecSub(dir, target->posinfo.position, ship->obj.posinfo.position);
        vecNormalize(&dir);
        aitrackHeadingFunc((SpaceObjRotImpTargGuidance *)ship, &dir, WORKER_FACE_ACCURACY, WORKER_FACE_ACCURACY, 0.0f, 0);
    }
    return facing;
}

// rcstate1 of a harvesting Worker (cat_workerHarvestsResource; names chosen here)
#define WORKER_HARVEST_Start        0
#define WORKER_HARVEST_Approach     1
#define WORKER_HARVEST_Turn         2
#define WORKER_HARVEST_Jack         3
#define WORKER_HARVEST_Asteroid     4
#define WORKER_HARVEST_Dust         5
#define WORKER_HARVEST_Nebula       6
#define WORKER_HARVEST_Stabilize    7

// the jack out and the beam on, then one CollectResources (the exe's inlined tail of harvest states 4, 5 and 6)
static void workerHarvestStep(Ship *ship, Resource *resource, sdword effectIndex)
{
    WorkerStatics *wstat = (WorkerStatics *)ship->staticinfo->custstatinfo;

    workerJackOut(ship);
    cat_workerBeamEffectUpdate(ship, &resource->obj, effectIndex, wstat->gunPoint);
    CollectResources(ship, resource);
}

// the harvest callback of sWorker / bWorker (ship->ShipXHarvestsResourceY; the code at 0x005c9130, no Ghidra
//      function): fly within 2000, turn to it, extend the jack, then beam and collect each update; a resource that is a
//      disguised mimic is attacked instead. NULL (or a hyperspace in single player) stops the harvest.
void cat_workerHarvestsResource(Ship *ship, Resource *resource)                             // 0x005c9130
{
    WorkerStatics *wstat = (WorkerStatics *)ship->staticinfo->custstatinfo;
    AttackCommand attack;
    SelectCommand selectone;
    ubyte state;

    if ((resource == NULL) || (singlePlayerGameInfo.hyperspaceState != 0))
    {
        if (ship->rceffect != NULL)
        {
            TurnHarvestEffectOff(ship);
        }
        ship->rcstate1 = WORKER_HARVEST_Start;
        return;
    }
    state = ship->rcstate1;
    if (state != WORKER_HARVEST_Stabilize)
    {
        if ((state == WORKER_HARVEST_Asteroid) || (state == WORKER_HARVEST_Nebula) || (state == WORKER_HARVEST_Dust))
        {
            // still facing it and within 3000: go on, else stop the beam and steady first
            if (!cat_workerTurnTowardTarget(ship, &resource->obj, TRUE) ||
                !cat_MoveReachedDestinationRadius(ship, &resource->obj.posinfo.position, WORKER_HARVEST_KEEP))
            {
                if (ship->rceffect != NULL)
                {
                    TurnHarvestEffectOff(ship);
                }
                ship->rcstate1 = WORKER_HARVEST_Stabilize;
                goto dostate;
            }
        }
        ship->shipidle = FALSE;
    }
dostate:
    switch (ship->rcstate1)
    {
        case WORKER_HARVEST_Start:
            ship->rcstate1 = WORKER_HARVEST_Approach;
            return;
        case WORKER_HARVEST_Approach:
            // (the exe also moves on when the flight reports an object in the way)
            if (cat_MoveReachedDestinationRadius(ship, &resource->obj.posinfo.position, WORKER_HARVEST_REACH) ||
                (aishipFlyToPointAvoidingObjsFunc(ship, &resource->obj.posinfo.position,
                                                  AISHIP_FastAsPossible + AISHIP_FirstPointInDirectionFlying +
                                                  AISHIP_PointInDirectionFlying + AISHIP_DontFlyToObscuredPoints, 0.0f,
                                                  &resource->obj.posinfo.velocity) == AISHIP_FLY_OBJECT_IN_WAY))
            {
                ship->rcstate1 = WORKER_HARVEST_Turn;
            }
            return;
        case WORKER_HARVEST_Turn:
            if (cat_workerTurnTowardTarget(ship, &resource->obj, FALSE))
            {
                vecZeroVector(ship->obj.posinfo.velocity);
                vecZeroVector(ship->obj.posinfo.force);
                vecZeroVector(ship->rotatable.rotinfo.rotspeed);
                vecZeroVector(ship->rotatable.rotinfo.torque);
                ship->rcstate1 = WORKER_HARVEST_Jack;
            }
            return;
        case WORKER_HARVEST_Jack:
            if (!workerAnimIs(ship, 0))
            {
                if (workerAnimIs(ship, -1) || workerAnimPaused(ship))
                {
                    madAnimationStart(ship, 0, 0);   // CAT: the exe's third argument, 0 (M2-R6 nis-fow-camera)
                    ((WorkerSpec *)ship->ShipSpecifics)->jackRetractTime = universe.totaltimeelapsed + wstat->jackRetractTime;
                }
                return;
            }
            ((WorkerSpec *)ship->ShipSpecifics)->jackRetractTime = universe.totaltimeelapsed + wstat->jackRetractTime;
            if (!workerAnimPaused(ship))
            {
                return;
            }
            switch (resource->header.objtype)
            {
                case OBJ_AsteroidType:
                    ship->rcstate1 = WORKER_HARVEST_Asteroid;
                    cat_workerBeamEffectUpdate(ship, &resource->obj, EGT_WORKER_RESOURCE_SOLID, wstat->gunPoint);
                    return;
                case OBJ_NebulaType:
                    ship->rcstate1 = WORKER_HARVEST_Nebula;
                    return;
                case OBJ_DustType:
                    ship->rcstate1 = WORKER_HARVEST_Dust;
                    return;
                default:
                    return;
            }
        case WORKER_HARVEST_Asteroid:
            if ((resource->header.objtype == OBJ_AsteroidType) && (((Asteroid *)resource)->catMimicShip != NULL))
            {
                // a mimic posing as this asteroid: attack it
                selectone.numShips = 1;
                selectone.ShipPtr[0] = ship;
                attack.numTargets = 1;
                attack.TargetPtr[0] = (TargetPtr)((Asteroid *)resource)->catMimicShip;
                clAttack(&universe.mainCommandLayer, &selectone, &attack);
                return;
            }
            workerHarvestStep(ship, resource, EGT_WORKER_RESOURCE_SOLID);
            return;
        case WORKER_HARVEST_Dust:
            workerHarvestStep(ship, resource, EGT_VORTEX_EFFECT);
            HandleDustCloudScaling((DustCloud *)resource);
            return;
        case WORKER_HARVEST_Nebula:
            workerHarvestStep(ship, resource, EGT_VORTEX_EFFECT);
            return;
        case WORKER_HARVEST_Stabilize:
            if (aitrackStabilizeGuidance((SpaceObjRotImpTargGuidance *)ship))
            {
                ship->rcstate1 = WORKER_HARVEST_Start;
            }
            return;
        default:
            return;
    }
}

void WorkerStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)       // 0x005c9580
{
    WorkerStatics *wstat = NULL;
    GunStaticInfo *gsi;
    GunStatic *gun;
    matrix local;
    matrix coordsys;
    vector heading;

    if (statinfo->shiptype == sWorker)
    {
        wstat = &WorkerStaticSect;
    }
    else if (statinfo->shiptype == bWorker)
    {
        wstat = &WorkerStaticBeast;
    }
    else
    {
        dbgFatalf(DBG_Loc, "WORKER: Static info for %s cannot be loaded", ShipTypeToStr(statinfo->shiptype));
    }
    wstat->jackRetractTime = 6.0f;                  // 0x40c00000
    wstat->grinderPoint = cat_dockFindDockStaticPoint("Grinder", statinfo->dockStaticInfo);
    wstat->gunPoint = cat_dockFindDockStaticPoint("gun0", statinfo->dockStaticInfo);
    statinfo->custstatinfo = wstat;
    scriptSetStruct(directory, filename, WorkerStaticScriptTable, wstat);
    // the first gun points down its normal in the frame of the mesh's first object
    gsi = statinfo->gunStaticInfo;
    if ((gsi != NULL) && (gsi->numGuns >= 1))
    {
        gun = &gsi->gunstatics[0];
        matGetMatFromHMat(&local, &statinfo->common.LOD->level[0].hBindings[0].object->localMatrix);
        heading.x = -gun->gunnormal.x;
        heading.y = -gun->gunnormal.y;
        heading.z = -gun->gunnormal.z;
        matCreateCoordSysFromHeading(&coordsys, &heading);
        matMultiplyMatByMat(&gun->defaultgunorientation, &coordsys, &local);
    }
    statinfo->canSpecialBandBoxFriendlies = TRUE;
    statinfo->unknownFlag212 = TRUE;
    cat_salvageStaticInit(directory, filename, statinfo);
    cat_workerStaticInitRepairPoints(directory, filename, statinfo);
}

void WorkerInit(Ship *ship)                                                                 // 0x005c9710
{
    WorkerSpec *spec = (WorkerSpec *)ship->ShipSpecifics;

    spec->unknown00 = 0;
    spec->unknown01 = 0;
    spec->jackRetractTime = WORKER_NEVER;
    spec->specialMode = WORKER_MODE_None;
    ship->ShipXHarvestsResourceY = cat_workerHarvestsResource;
    cat_salvageInit(ship);
    spec->repairState = WORKER_REPAIR_Choose;
    spec->repairTarget = NULL;
    spec->repairEffect = NULL;
    spec->repairEffectOn = FALSE;
    spec->repairEffectDist = 0.0f;
}

bool32 WorkerSpecialTarget(Ship *ship,void *custom)                                         // 0x005c9770
{
    WorkerSpec *spec = (WorkerSpec *)ship->ShipSpecifics;
    bool32 done;

    if (spec->specialMode == WORKER_MODE_Repair)
    {
        done = cat_workerRepairTarget(ship, (SelectCommand *)custom);
    }
    else if (spec->specialMode == WORKER_MODE_Salvage)
    {
        done = cat_salvageSpecialTarget(ship, custom);
    }
    else
    {
        spec->specialMode = WORKER_MODE_None;
        return TRUE;
    }
    if (done)
    {
        spec->specialMode = WORKER_MODE_None;
    }
    return done;
}

void WorkerOrderChanged(Ship *ship)                                                         // 0x005c97d0
{
    cat_salvageOrderChanged(ship);
    cat_workerReleaseRepairTarget(ship);
    ((WorkerSpec *)ship->ShipSpecifics)->specialMode = WORKER_MODE_None;
}

// the harvest beam off and the harvest state reset (inlined in WorkerHouseKeep, WorkerDied and WorkerHaltCommand)
static void workerHarvestOff(Ship *ship)
{
    if (ship->rceffect != NULL)
    {
        TurnHarvestEffectOff(ship);
    }
    ship->rcstate1 = 0;
}

void WorkerHouseKeep(Ship *ship)                                                            // 0x005c9880
{
    WorkerSpec *spec = (WorkerSpec *)ship->ShipSpecifics;

    if (bitTest(ship->header.flags, SOF_Disabled | SOF_Crazy | SOF_Hide | SOF_Dead))
    {
        workerHarvestOff(ship);
    }
    // fold the jack back once its time is up
    if ((spec->jackRetractTime < universe.totaltimeelapsed) && !workerAnimIs(ship, 1) && workerAnimPaused(ship))
    {
        madAnimationStart(ship, 1, 2);       // CAT: the exe's third argument, 2 (M2-R6 nis-fow-camera)
        spec->jackRetractTime = WORKER_NEVER;
    }
    cat_salvageHouseKeep(ship);
    if (!bitTest(ship->header.flags, SOF_Disabled | SOF_Hide | SOF_Dead) && !ship->catEmpStunned)
    {
        if (spec->repairEffectOn && (spec->repairEffect == NULL))
        {
            cat_workerRepairEffect(ship);
        }
        return;
    }
    cat_workerReleaseRepairTarget(ship);
}

void WorkerRemoveShipReferences(Ship *ship, Ship *shiptoremove)                             // 0x005c99f0
{
    cat_salvageRemoveShipReferences(ship, shiptoremove);
    if (shiptoremove == ((WorkerSpec *)ship->ShipSpecifics)->repairTarget)
    {
        cat_workerReleaseRepairTarget(ship);
    }
}

void WorkerDied(Ship *ship)                                                                 // 0x005c9aa0
{
    workerHarvestOff(ship);
    cat_salvageDied(ship);
    cat_workerReleaseRepairTarget(ship);
}

void Worker_PreFix(Ship *ship)                                                              // 0x005c9b90
{
    WorkerSpec *spec = (WorkerSpec *)ship->ShipSpecifics;

    cat_salvage_PreFix(ship);
    spec->repairTarget = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->repairTarget);
    spec->repairEffect = NULL;
}

void Worker_Fix(Ship *ship)                                                                 // 0x005c9bc0
{
    WorkerSpec *spec = (WorkerSpec *)ship->ShipSpecifics;

    cat_salvage_Fix(ship);
    spec->repairTarget = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->repairTarget);
}

bool32 WorkerHaltCommand(Ship *ship)                                                        // 0x005c9bf0
{
    workerHarvestOff(ship);
    return TRUE;
}

// CAT: WorkerHeader 0x008c6b50 (sWorker, bWorker). Slot 26 is kasfSpecialOff 0x00497db0 (no known caller of slot 26;
//      left NULL here)
CustShipHeader WorkerHeader =
{
    .sizeofShipSpecifics = sizeof(WorkerSpec),
    .CustShipStaticInit = WorkerStaticInit,
    .CustShipInit = WorkerInit,
    .CustShipSpecialTarget = WorkerSpecialTarget,
    .CustShipOrderChanged = WorkerOrderChanged,
    .CustShipHousekeep = WorkerHouseKeep,
    .CustShipRemoveShipReferences = WorkerRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))WorkerRemoveShipReferences,
    .CustShipDied = WorkerDied,
    .CustShip_PreFix = Worker_PreFix,
    .CustShip_Fix = Worker_Fix,
    .CustShipHaltCommand = WorkerHaltCommand,
};
