// =============================================================================
//  Sentinel.c
//  - Cataclysm's sSentinel / bSentinel: in a force-field formation they span
//    a shield of triangles that soaks up hits (cataclysm.exe 0x005c5af0-0x005c6fa0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each).
//      cat_shipIsSentinelFieldActive 0x005c6e50 and cat_shipIsSentinelInField 0x005c6ec0 moved here from ShipSelect.c
//      (their declarations stay in ShipSelect.h). The field's drawing (cat_sentinelFieldSegmentAdd 0x005c5b20,
//      cat_sentinelFieldsRenderBehind / RenderRest, cat_sentinelFieldQueueSegment 0x005c5ec0, cat_sentinelFieldQueue
//      0x005c67b0, cat_sentinelFieldSegmentDraw 0x005c60a0) is the renderer's: stage 3.

#include "Sentinel.h"

#include <math.h>

#include "Attack.h"
#include "Clamp.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "DefaultShip.h"
#include "Dreadnought.h"
#include "ETG.h"
#include "FastMath.h"
#include "Formation.h"
#include "FormationDefs.h"
#include "Matrix.h"
#include "MeshAnim.h"
#include "SaveGame.h"
#include "ShipSelect.h"
#include "StatScript.h"
#include "UnivUpdate.h"
#include "Universe.h"

AttackSideStepParameters SentinelStaticSect;    // 0x00a7a5e0: sSentinel
AttackSideStepParameters SentinelStaticBeast;   // 0x00a7a5f0: bSentinel

static const real32 sentinelMaxCharge[3] = { 4000.0f, 6000.0f, 10000.0f };     // 0x008c6808
static const real32 sentinelChargeRate[3] = { 4.0f, 8.0f, 16.0f };             // 0x008c6814

#define SENTINEL_IN_POSITION        50.0f       // 0x008c67b8
#define SENTINEL_LEVEL_RATE         0.2f        // 0x008c67c4
#define SENTINEL_FADE_RATE          0.6f        // 0x008c67c8

static bool32 sentinelIsSentinel(Ship *ship)
{
    return (ship->shiptype == sSentinel) || (ship->shiptype == bSentinel);
}

bool32 cat_shipIsLiveSentinel(Ship *ship)                                                   // 0x005c5af0
{
    return (ship->header.objtype == OBJ_ShipType) && sentinelIsSentinel(ship);
}

void SentinelStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)     // 0x005c5d10
{
    AttackSideStepParameters *sentinelstat = (statinfo->shiprace != RACE_Sect) ? &SentinelStaticBeast : &SentinelStaticSect;

    scriptSetStruct(directory,filename,AttackSideStepParametersScriptTable,(ubyte *)sentinelstat);
    statinfo->custstatinfo = sentinelstat;
    statinfo->canSpecialBandBoxFriendlies = TRUE;
}

void SentinelInit(Ship *ship)                                                               // 0x005c5d60
{
    SentinelSpec *spec = (SentinelSpec *)ship->ShipSpecifics;

    // (the exe runs an empty 3-step loop first)
    spec->fieldActive = FALSE;
    spec->fieldLevel = 0.0f;
    spec->numLinks = 0;
    spec->unknown338 = 0;
    spec->disrupted = FALSE;
    spec->disruptEndTime = 0.0f;
}

void SentinelAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)                  // 0x005c5da0
{
    attackStraightForward(ship, target, maxdist, 1000.0f);
}

void SentinelAttackPassive(Ship *ship,Ship *target,bool32 rotate)                          // 0x005c5dd0
{
    // a Sentinel in a sphere does not fight back
    if ((ship->command != NULL) && (ship->command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
        (ship->command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION))
    {
        return;
    }
    if (rotate & (bool32)ship->staticinfo->rotateToRetaliate)
    {
        attackPassiveRotate(ship, target);
        return;
    }
    attackPassive(ship, target);
}

// the formation leader's field update: who is in position, which triangles are up, the field level; a sphere's
// Sentinels clamp to the protected ship while their field is up
static void cat_sentinelFormationUpdate(Ship *ship)                                        // 0x005c6830
{
    CommandToDo *command = ship->command;
    SelectCommand *formation;
    Ship *centre;
    Ship *member;
    SentinelSpec *spec, *aspec, *bspec;
    vector offset;
    vector diff;
    sdword i, j;
    bool32 active;

    if ((command != NULL) && (command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
        ((command->formation.formationtype == FORCEFIELD_WALL_FORMATION) ||
         (command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION)) &&
        (command->selection->ShipPtr[0] == ship))
    {
        centre = ship;
        if (command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION)
        {
            // a sphere needs a protected ship in the universe, else it falls back to a wall
            if (!(command->ordertype.attributes & COMMAND_MASK_PROTECTING))
            {
                command->formation.formationtype = FORCEFIELD_WALL_FORMATION;
                formationContentHasChanged(ship->command);
            }
            else
            {
                centre = command->protect->ShipPtr[0];
            }
            if (centre->shiplink.belongto == NULL)
            {
                ship->command->formation.formationtype = FORCEFIELD_WALL_FORMATION;
                formationContentHasChanged(ship->command);
                centre = ship;
            }
        }
        formation = ship->command->selection;
        for (i = 0; i < formation->numShips; i++)
        {
            ((SentinelSpec *)formation->ShipPtr[i]->ShipSpecifics)->fieldActive = FALSE;
        }
        for (i = 0; i < formation->numShips; i++)
        {
            member = formation->ShipPtr[i];
            spec = (SentinelSpec *)member->ShipSpecifics;
            matMultiplyMatByVec(&offset, &centre->rotatable.rotinfo.coordsys, &member->formationOffset);
            diff.x = (member->obj.posinfo.position.x - centre->impactable.collInfo.collPosition.x) - offset.x;
            diff.y = (member->obj.posinfo.position.y - centre->impactable.collInfo.collPosition.y) - offset.y;
            diff.z = (member->obj.posinfo.position.z - centre->impactable.collInfo.collPosition.z) - offset.z;
            spec->inPosition = (fabs((double)fsqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z)) <=
                                SENTINEL_IN_POSITION);
            spec->fieldActive = FALSE;
        }
        for (i = 0; i < formation->numShips; i++)
        {
            member = formation->ShipPtr[i];
            spec = (SentinelSpec *)member->ShipSpecifics;
            active = spec->inPosition ? TRUE : FALSE;
            if (spec->inPosition)
            {
                for (j = 0; j < spec->numLinks; j++)
                {
                    if (!((SentinelSpec *)spec->links[j].a->ShipSpecifics)->inPosition ||
                        !((SentinelSpec *)spec->links[j].b->ShipSpecifics)->inPosition)
                    {
                        active = FALSE;
                    }
                }
            }
            if ((formation->numShips < 3) || spec->depleted || member->catEmpStunned || spec->disrupted || !active)
            {
                active = FALSE;
                spec->fieldLevel = 0.0f;
            }
            else
            {
                if (spec->fieldLevel == 0.0f)
                {
                    spec->fade = 1.0f;
                }
                spec->fieldLevel = universe.phystimeelapsed * SENTINEL_LEVEL_RATE + spec->fieldLevel;
                if (spec->fieldLevel > 1.0f)
                {
                    spec->fieldLevel = 1.0f;
                    if (spec->fade > 0.0f)
                    {
                        spec->fade = spec->fade - SENTINEL_FADE_RATE * universe.phystimeelapsed;
                        if (spec->fade < 0.0f)
                        {
                            spec->fade = 0.0f;
                        }
                    }
                }
            }
            spec->fieldActive = active;
            // the first partner of each triangle that is still down takes this Sentinel's state
            for (j = 0; j < spec->numLinks; j++)
            {
                aspec = (SentinelSpec *)spec->links[j].a->ShipSpecifics;
                bspec = (SentinelSpec *)spec->links[j].b->ShipSpecifics;
                if (!aspec->fieldActive)
                {
                    aspec->fieldActive = active;
                }
                else if (!bspec->fieldActive)
                {
                    bspec->fieldActive = active;
                }
            }
        }
        if (ship->command->formation.formationtype != FORCEFIELD_SPHERE_FORMATION)
        {
            return;
        }
        for (i = 0; i < formation->numShips; i++)
        {
            member = formation->ShipPtr[i];
            if (!((SentinelSpec *)member->ShipSpecifics)->fieldActive)
            {
                unClampObj((SpaceObjRotImpTargGuidance *)member);
            }
            else if ((member->guidance.clampInfo == NULL) || (member->guidance.clampInfo->host == NULL))
            {
                clampObjToObj((SpaceObjRotImpTargGuidance *)member,
                              (SpaceObjRotImpTargGuidance *)member->command->protect->ShipPtr[0]);
            }
        }
        return;
    }
    if ((command != NULL) && (command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION))
    {
        return;
    }
    ((SentinelSpec *)ship->ShipSpecifics)->fieldActive = FALSE;
    unClampObj((SpaceObjRotImpTargGuidance *)ship);
}

void SentinelHouseKeep(Ship *ship)                                                          // 0x005c5e20
{
    SentinelSpec *spec = (SentinelSpec *)ship->ShipSpecifics;
    real32 max = sentinelMaxCharge[spec->chargeLevel];
    real32 rate;

    // the charge refills (half as much in single player); a full charge ends the depletion
    if (singlePlayerGame)
    {
        max *= 0.5f;
    }
    if (spec->charge < max)
    {
        rate = sentinelChargeRate[spec->chargeLevel];
        if (singlePlayerGame)
        {
            rate *= 0.5f;
        }
        spec->charge = rate + spec->charge;
    }
    if (max <= spec->charge)
    {
        spec->depleted = FALSE;
    }
    if ((universe.univUpdateCounter & 0xc) != 0)
    {
        cat_sentinelFormationUpdate(ship);
    }
}

real32 cat_sentinelFieldChargeFraction(SentinelSpec *spec)                                 // 0x005c5ea0
{
    return spec->charge / sentinelMaxCharge[spec->chargeLevel];
}

// a bullet (or beam) hits the field: its damage against the Sentinel's armour, rounded (the exe's 1.5 * 2^52 trick),
// comes off the charge
void cat_sentinelFieldBulletHit(Ship *sentinel, void *unused, Bullet *bullet)              // 0x005c6c60
{
    SentinelSpec *spec = (SentinelSpec *)sentinel->ShipSpecifics;
    real32 damage;

    (void)unused;
    spec->lastHitTime = universe.totaltimeelapsed;
    damage = cat_weaponDamageVsArmour(((Bullet *)bullet)->soundType, sentinel->catArmourLevel) *
             ((Bullet *)bullet)->damage;
    spec->charge = spec->charge - (real32)(sdword)lrint((double)damage);
    if (spec->charge < 0.0f)
    {
        spec->depleted = TRUE;
    }
}

// a missile hits the field: its hit effect at the missile, its damage (truncated) off the charge
void cat_sentinelFieldMissileHit(Ship *sentinel, void *unused, Missile *missile)            // 0x005c6cd0
{
    SentinelSpec *spec = (SentinelSpec *)sentinel->ShipSpecifics;
    Missile *m = (Missile *)missile;
    etglod *etgLOD = m->hitEffect;
    etgeffectstatic *stat = NULL;

    (void)unused;
    spec->lastHitTime = universe.totaltimeelapsed;
    if ((etgLOD != NULL) && (sentinel->obj.currentLOD < etgLOD->nLevels))
    {
        stat = etgLOD->level[sentinel->obj.currentLOD];
    }
    if (univSpaceObjInRenderList(&sentinel->obj) && (stat != NULL) && etgHitEffectsEnabled && !etgFrequencyExceeded(stat))
    {
        etgEffectCreate(stat, NULL, &m->obj.posinfo.position, &sentinel->obj.posinfo.velocity,
                        &m->rotatable.rotinfo.coordsys, sentinel->magnitudeSquared, 0, 2, 100, 0);
    }
    spec->charge = spec->charge - (real32)(sdword)m->damage;
    if (spec->charge < 0.0f)
    {
        spec->depleted = TRUE;
    }
}

void SentinelRemoveShipReferences(Ship *ship,Ship *shiptoremove)                            // 0x005c6d90
{
    SentinelSpec *spec = (SentinelSpec *)ship->ShipSpecifics;
    sdword i;

    // a triangle with the removed ship goes; the last one takes its place (the moved one is not checked again)
    for (i = 0; i < spec->numLinks; i++)
    {
        if ((spec->links[i].a == shiptoremove) || (spec->links[i].b == shiptoremove))
        {
            if (i < spec->numLinks - 1)
            {
                // CAT: the exe copies from (count * 8) past +0x63C / +0x640, i.e. the entry one past the last
                //      (0x005c6dbb); kept
                spec->links[i] = spec->links[spec->numLinks];
            }
            spec->numLinks--;
        }
    }
}

bool32 SentinelFormationCommand(Ship *ship, sdword formation)                               // 0x005c6df0
{
    if ((formation != FORCEFIELD_WALL_FORMATION) && (formation != FORCEFIELD_SPHERE_FORMATION))
    {
        bitClear(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSentinelFormation);
        cat_madAnimationStartNumber(ship, 1);       // CAT-TODO(stage 3): the exe passes a third argument, 2
        return TRUE;
    }
    bitSet(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSentinelFormation);
    cat_madAnimationStartNumber(ship, 0);           // CAT-TODO(stage 3): the exe passes a third argument, 0
    return TRUE;
}

bool32 cat_shipIsSentinelFieldActive(Ship *ship)                                            // 0x005c6e50
{
    SentinelSpec *spec = (SentinelSpec *)ship->ShipSpecifics;

    return (spec->fieldActive == 1) && (spec->fieldLevel == 1.0f);
}

bool32 cat_shipIsSentinelInSphere(Ship *ship)                                               // 0x005c6e80
{
    return sentinelIsSentinel(ship) && (ship->command != NULL) &&
           (ship->command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
           (ship->command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION);
}

bool32 cat_shipIsSentinelInField(Ship *ship)                                                // 0x005c6ec0
{
    return sentinelIsSentinel(ship) && (ship->command != NULL) &&
           (ship->command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
           ((ship->command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION) ||
            (ship->command->formation.formationtype == FORCEFIELD_WALL_FORMATION));
}

sdword cat_sentinelFieldLinks(Ship *sentinel, SentinelLink **links)
{
    SentinelSpec *spec = (SentinelSpec *)sentinel->ShipSpecifics;

    *links = spec->links;
    return spec->numLinks;
}

void Sentinel_PreFix(Ship *ship)                                                            // 0x005c6f00
{
    SentinelSpec *spec = (SentinelSpec *)ship->ShipSpecifics;
    sdword i;

    for (i = 0; i < spec->numLinks; i++)
    {
        spec->links[i].a = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->links[i].a);
        spec->links[i].b = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->links[i].b);
    }
}

void Sentinel_Fix(Ship *ship)                                                               // 0x005c6f50
{
    SentinelSpec *spec = (SentinelSpec *)ship->ShipSpecifics;
    sdword i;

    for (i = 0; i < spec->numLinks; i++)
    {
        spec->links[i].a = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->links[i].a);
        spec->links[i].b = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->links[i].b);
    }
}

// CAT: SentinelHeader 0x008c6830 (sSentinel, bSentinel); slot 5 is DreadnoughtFire, slots 7 and 8 kasfSpecialOff, slot
//      34 the formation command
CustShipHeader SentinelHeader =
{
    .sizeofShipSpecifics = sizeof(SentinelSpec),
    .CustShipStaticInit = SentinelStaticInit,
    .CustShipInit = SentinelInit,
    .CustShipFire = DreadnoughtFire,
    .CustShipAttackPassive = SentinelAttackPassive,
    .CustShipSpecialActivate = kasfSpecialOffShip,
    .CustShipSpecialTarget = kasfSpecialOffTarget,
    .CustShipHousekeep = SentinelHouseKeep,
    .CustShipRemoveShipReferences = SentinelRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))SentinelRemoveShipReferences,
    .CustShip_PreFix = Sentinel_PreFix,
    .CustShip_Fix = Sentinel_Fix,
    .CustShipAttack = SentinelAttack,
    .CustShipFormationCommand = SentinelFormationCommand,
};
