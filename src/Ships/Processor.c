// =============================================================================
//  Processor.c
//  - Cataclysm's sProcessor / bProcessor: the salvage depot with auto-repair
//    beams (cataclysm.exe 0x005be8f0-0x005bf3a0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each). The
//      depot half is Salvage.c's; with the 0x40000 upgrade each DPT_AutoRepair point beams twRepairRate into the most
//      damaged own ship within twProcessorAutoRepairDistance and its cone.

#include "Processor.h"

#include <math.h>
#include <string.h>

#include "Attack.h"
#include "DefaultShip.h"
#include "ETG.h"
#include "FastMath.h"
#include "Matrix.h"
#include "RKO.h"
#include "SaveGame.h"
#include "ShipSelect.h"
#include "Tweak.h"
#include "UnivUpdate.h"
#include "Universe.h"
#include "Worker.h"

#define PROCESSOR_TIMER_MIN         0.001f      // 0x0072bfd0
#define PROCESSOR_DEG_TO_HALF_RAD   (0.5f * 0.01745329238474369f)   // 0x0072a4c8 x 0x0072a4f8
#define PROCESSOR_TOO_CLOSE         3000.0f     // ProcessorAttack 0x005bf3a0

// the cone test's constants, computed once from the tweakables (0x00a77b04 set, 0x00a77afc, 0x00a77b00)
static bool32 processorConeSet = FALSE;
static real32 processorConeCos;
static real32 processorRangeSq;

static void cat_processorRepairEffect(ProcessorBeam *beam)                                  // 0x005be8f0
{
    Ship *processor = beam->processor;
    Ship *target = beam->target;
    etglod *etgLOD;
    etgeffectstatic *stat;
    vector pos, dir;
    matrix coordsys;
    real32 dist, width, scale;

    matMultiplyMatByVec(&pos, &processor->rotatable.rotinfo.coordsys, &beam->point->position);
    vecAddTo(pos, processor->obj.posinfo.position);
    vecSub(dir, target->obj.posinfo.position, pos);
    dist = fsqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    width = target->staticinfo->common.staticCollInfo.collspheresize;
    if (twProcessorAutoRepairBeamWidth < width)
    {
        width = twProcessorAutoRepairBeamWidth;
    }
    scale = 1.0f / dist;
    vecMultiplyByScalar(dir, scale);
    matCreateCoordSysFromHeading(&coordsys, &dir);
    if (beam->effect == NULL)
    {
        etgLOD = etgSpecialPurposeEffectTable[EGT_REPAIR_BEAM];
        if ((etgLOD != NULL) && (processor->obj.currentLOD < etgLOD->nLevels) &&
            ((stat = etgLOD->level[processor->obj.currentLOD]) != NULL) && !etgFrequencyExceeded(stat))
        {
            beam->effect = etgEffectCreate(stat, NULL, &pos, &processor->obj.posinfo.velocity, &coordsys, 1.0f, 0, 2,
                                           SCALECAST(Real32ToUdword(dist)), SCALECAST(Real32ToUdword(width)));
            return;
        }
        beam->effect = NULL;
        return;
    }
    beam->effect->obj.posinfo.position = pos;
    beam->effect->obj.posinfo.velocity = processor->obj.posinfo.velocity;
    beam->effect->rotatable.rotinfo.coordsys = coordsys;
}

// within twProcessorAutoRepairDistance of the beam's point and inside its cone (half twProcessorAutoRepairConeAngle)
static bool32 cat_processorRepairTargetInCone(ProcessorBeam *beam, Ship *target)            // 0x005beae0
{
    Ship *processor = beam->processor;
    vector pos, diff, cone;
    real32 distsq;

    if (!processorConeSet)
    {
        processorConeSet = TRUE;
        processorConeCos = (real32)cos((double)(twProcessorAutoRepairConeAngle * PROCESSOR_DEG_TO_HALF_RAD));
        processorRangeSq = twProcessorAutoRepairDistance * twProcessorAutoRepairDistance;
    }
    matMultiplyMatByVec(&pos, &processor->rotatable.rotinfo.coordsys, &beam->point->position);
    vecAddTo(pos, processor->obj.posinfo.position);
    vecSub(diff, target->obj.posinfo.position, pos);
    distsq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    if (processorRangeSq < distsq)
    {
        return FALSE;
    }
    matMultiplyMatByVec(&cone, &processor->rotatable.rotinfo.coordsys, &beam->point->conenormal);
    if ((cone.x * diff.x + cone.y * diff.y + cone.z * diff.z) / fsqrt(distsq) < processorConeCos)
    {
        return FALSE;
    }
    return TRUE;
}

// the own damaged ship with the lowest health fraction the beam reaches
static Ship *cat_processorFindRepairTarget(ProcessorBeam *beam)                             // 0x005bec30
{
    Ship *found = NULL;
    Ship *ship;
    Node *node;
    real32 best = 1.0e21f;
    real32 fraction;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)node->structptr;
        if ((ship->playerowner == beam->processor->playerowner) &&
            !bitTest(ship->header.flags, SOF_Hide | SOF_Dead) &&
            (0.0f < twRepairRate[ship->staticinfo->shipclass]) &&
            !bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt) &&
            (ship->targetable.health < ship->staticinfo->health.maxhealth))
        {
            fraction = ship->staticinfo->health.oneOverMaxHealth * ship->targetable.health;
            if ((fraction < best) && cat_processorRepairTargetInCone(beam, ship))
            {
                found = ship;
                best = fraction;
            }
        }
    }
    return found;
}

// one beam per DPT_AutoRepair dock point, at most four
static void cat_processorInitRepairBeams(Ship *processor)                                   // 0x005bed00
{
    ProcessorSpec *spec = (ProcessorSpec *)processor->ShipSpecifics;
    DockStaticInfo *dockinfo = processor->staticinfo->dockStaticInfo;
    ProcessorBeam *beam;
    sdword i;

    memset(spec->beams, 0, sizeof(spec->beams));
    spec->numBeams = 0;
    for (i = 0; i < (sbyte)dockinfo->numOfType[DPT_AutoRepair]; i++)
    {
        if (spec->numBeams > PROCESSOR_MAX_BEAMS - 1)
        {
            return;
        }
        beam = &spec->beams[spec->numBeams];
        beam->processor = processor;
        beam->point = &dockinfo->dockstaticpoints[(sbyte)dockinfo->firstOfType[DPT_AutoRepair] + i];
        beam->target = NULL;
        beam->effect = NULL;
        cat_timerStartGameTime(&beam->timer, PROCESSOR_TIMER_MIN);
        spec->numBeams++;
    }
}

// the beam lets go of its target: the effect goes, the next search after twProcessorAutoRepairUpdateDelay (the exe
//      inlines this in five places)
static void processorBeamRelease(ProcessorBeam *beam)
{
    Effect *effect = beam->effect;

    beam->target = NULL;
    if (effect != NULL)
    {
        etgEffectDelete(effect);
        univRemoveObjFromRenderList(&effect->obj);
        listDeleteNode(&effect->header.objlink);
        beam->effect = NULL;
    }
    cat_timerStartGameTime(&beam->timer, (twProcessorAutoRepairUpdateDelay <= PROCESSOR_TIMER_MIN) ?
                                          PROCESSOR_TIMER_MIN : twProcessorAutoRepairUpdateDelay);
}

static void cat_processorAutoRepair(Ship *processor)                                        // 0x005bedd0
{
    ProcessorSpec *spec = (ProcessorSpec *)processor->ShipSpecifics;
    ProcessorBeam *beam;
    Ship *target;
    Ship *part;
    real32 health;
    sdword i;

    if (!bitTest(processor->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt) &&
        cat_shipHasUpgrade(processor, CAT_UPGRADE_Repair) &&
        !bitTest(processor->header.flags, SOF_Disabled | SOF_Hide | SOF_Dead) &&
        !bitTest(processor->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvageTargeted) &&
        (processor->catSalvageDepot == NULL) &&
        !bitTest(processor->header.flags, SOF_Hyperspace) &&
        !bitTest(processor->shipOrDerelict.specialFlags, SPECIAL_Hyperspacing) &&
        !processor->catEmpStunned)
    {
        for (i = 0; i < spec->numBeams; i++)
        {
            beam = &spec->beams[i];
            target = beam->target;
            if (target != NULL)
            {
                if (bitTest(target->header.flags, SOF_Hide | SOF_Dead) ||
                    (target->staticinfo->health.maxhealth <= target->targetable.health) ||
                    !cat_processorRepairTargetInCone(beam, target))
                {
                    processorBeamRelease(beam);
                }
                else
                {
                    part = cat_shipFindMostDamagedPart(beam->target);
                    health = universe.phystimeelapsed * twRepairRate[beam->target->staticinfo->shipclass] +
                             part->targetable.health;
                    part->targetable.health = health;
                    if (part->staticinfo->health.maxhealth < health)
                    {
                        health = part->staticinfo->health.maxhealth;
                    }
                    part->targetable.health = health;
                }
            }
            if (beam->target == NULL)
            {
                if (cat_timerExpired(&beam->timer))
                {
                    target = cat_processorFindRepairTarget(beam);
                    if (target == NULL)
                    {
                        cat_timerStartGameTime(&beam->timer, (PROCESSOR_TIMER_MIN < twProcessorAutoRepairUpdateRate) ?
                                                              twProcessorAutoRepairUpdateRate : PROCESSOR_TIMER_MIN);
                    }
                    else
                    {
                        beam->target = target;
                        cat_timerStartGameTime(&beam->timer, PROCESSOR_TIMER_MIN);
                    }
                }
                if (beam->target == NULL)
                {
                    continue;
                }
            }
            cat_processorRepairEffect(beam);
        }
        return;
    }
    for (i = 0; i < spec->numBeams; i++)
    {
        processorBeamRelease(&spec->beams[i]);
    }
}

void ProcessorInit(Ship *ship)                                                              // 0x005bf090
{
    cat_processorInitRepairBeams(ship);
    cat_salvageDepotInit(ship);
}

void ProcessorHouseKeep(Ship *ship)                                                         // 0x005bf0b0
{
    cat_processorAutoRepair(ship);
    cat_salvageDepotHouseKeep(ship);
}

void ProcessorDied(Ship *ship)                                                              // 0x005bf0d0
{
    ProcessorSpec *spec = (ProcessorSpec *)ship->ShipSpecifics;
    sdword i;

    for (i = 0; i < spec->numBeams; i++)
    {
        processorBeamRelease(&spec->beams[i]);
    }
    cat_salvageDepotDied(ship);
}

void ProcessorRemoveShipReferences(Ship *ship, Ship *shiptoremove)                          // 0x005bf170
{
    ProcessorSpec *spec = (ProcessorSpec *)ship->ShipSpecifics;
    sdword i;

    for (i = 0; i < spec->numBeams; i++)
    {
        if (spec->beams[i].target == shiptoremove)
        {
            processorBeamRelease(&spec->beams[i]);
        }
    }
    cat_salvageDepotRemoveShipReferences(ship, shiptoremove);
}

// an effect that goes away by itself: the beam forgets it (without deleting it) and lets go of its target
void ProcessorEffectRemoval(Ship *ship, Effect *effect)                                     // 0x005bf220
{
    ProcessorSpec *spec = (ProcessorSpec *)ship->ShipSpecifics;
    sdword i;

    for (i = 0; i < spec->numBeams; i++)
    {
        if (spec->beams[i].effect == effect)
        {
            spec->beams[i].effect = NULL;
            processorBeamRelease(&spec->beams[i]);
        }
    }
}

void Processor_PreFix(Ship *ship)                                                           // 0x005bf2d0
{
    ProcessorSpec *spec = (ProcessorSpec *)ship->ShipSpecifics;
    ProcessorBeam *beam;
    sdword i;

    for (i = 0; i < spec->numBeams; i++)
    {
        beam = &spec->beams[i];
        beam->processor = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)beam->processor);
        beam->point = (DockStaticPoint *)(smemsize)beam->point->dockindex;
        beam->target = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)beam->target);
        beam->effect = NULL;
    }
    cat_salvageDepot_PreFix(ship);
}

void Processor_Fix(Ship *ship)                                                              // 0x005bf330
{
    ProcessorSpec *spec = (ProcessorSpec *)ship->ShipSpecifics;
    ProcessorBeam *beam;
    sdword i;

    for (i = 0; i < spec->numBeams; i++)
    {
        beam = &spec->beams[i];
        beam->processor = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)beam->processor);
        beam->point = &ship->staticinfo->dockStaticInfo->dockstaticpoints[(sdword)(smemsize)beam->point];
        beam->target = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)beam->target);
    }
    cat_salvageDepotFix(ship);
}

void ProcessorAttack(Ship *ship, SpaceObjRotImpTarg *target, real32 maxdist)                // 0x005bf3a0
{
    attackStraightForward(ship, target, maxdist, PROCESSOR_TOO_CLOSE);
}

// CAT: ProcessorHeader 0x008c5b60 (sProcessor, bProcessor): no StaticInit (slot 1 NULL); slot 6 is the shared
//      attackPassive body (RKOAttackPassive 0x005c59a0)
CustShipHeader ProcessorHeader =
{
    .sizeofShipSpecifics = sizeof(ProcessorSpec),
    .CustShipInit = ProcessorInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = RKOAttackPassive,
    .CustShipHousekeep = ProcessorHouseKeep,
    .CustShipRemoveShipReferences = ProcessorRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))ProcessorRemoveShipReferences,
    .CustShipEffectRemoval = ProcessorEffectRemoval,
    .CustShipDied = ProcessorDied,
    .CustShip_PreFix = Processor_PreFix,
    .CustShip_Fix = Processor_Fix,
    .CustShipAttack = ProcessorAttack,
};
