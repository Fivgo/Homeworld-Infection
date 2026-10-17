// =============================================================================
//  ACV.c
//  - Cataclysm's sACV / bACV (assault corvette): side-stepping gun corvette
//    with the EMP pulse upgrade (cataclysm.exe 0x005adba0-0x005aeed0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each).
//      cat_acvLinkState moved here from ShipSelect.c (its declaration stays in ShipSelect.h).

#include "ACV.h"

#include "AIShip.h"
#include "Battle.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "CommandWrap.h"
#include "ETG.h"
#include "Formation.h"
#include "Gun.h"
#include "P1Mothership.h"
#include "RKO.h"
#include "Salvage.h"
#include "SaveGame.h"
#include "ShipDisable.h"
#include "SoundEvent.h"
#include "SoundEventDefs.h"
#include "StatScript.h"
#include "Tactics.h"
#include "Universe.h"
#include "Vector.h"

#define ACV_EMP_PULSE_TIME          2.0f        // 0x0072bf70
#define ACV_EMP_RECHARGE_TIME       75.0f       // 0x0072bf74
#define ACV_EMP_TRIGGER_DIST        2500.0f     // 0x0072bf78: pulse when this close to empTarget
#define ACV_EMP_RKO_RANGE           4000.0f     // 0x008c3320: an RKO is hit (and triggers the pulse) from further
#define ACV_EMP_NEVER               1.0e21f     // 0x6258d727

static ACVStatics ACVStaticSect;    // 0x00ab74e0
static ACVStatics ACVStaticBeast;   // 0x00ab7500

static bool32 acvIsACV(Ship *ship)
{
    return (ship->shiptype == sACV) || (ship->shiptype == bACV);
}

bool32 cat_empShipClampedToPlayer(udword playerIndex, Ship *ship)                          // 0x005adba0
{
    Ship *salvager;
    bool32 clamped = FALSE;

    salvager = cat_salvageAttachedIterFirst(ship);
    if ((salvager != NULL) && bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvageTargeted) &&
        (salvager->playerowner->playerIndex == playerIndex))
    {
        clamped = TRUE;
    }
    if ((ship->catSalvageDepot != NULL) && (ship->catSalvageDepot->playerowner->playerIndex == playerIndex))
    {
        return TRUE;
    }
    return clamped;
}

void cat_acvLinkReset(Ship *ship)                                                           // 0x005adc10
{
    ACVSpec *spec = (ACVSpec *)ship->ShipSpecifics;

    if (acvIsACV(ship))
    {
        spec->linkStateA = 2;
        spec->linkStateB = 2;
    }
}

void ACVDied(Ship *ship)                                                                    // 0x005adc40
{
    growSelectClose(&((ACVSpec *)ship->ShipSpecifics)->empTargets);
}

bool32 cat_acvIsFullyLinked(Ship *ship)                                                     // 0x005adc60
{
    return acvIsACV(ship) && (((ACVSpec *)ship->ShipSpecifics)->empState == ACV_EMP_Targeted);
}

void ACVRemoveShipReferences(Ship *ship, Ship *shiptoremove)                                // 0x005adc90
{
    ACVSpec *spec = (ACVSpec *)ship->ShipSpecifics;
    SelectCommand *sel = spec->empTargets.selection;
    sdword i = 0;

    if (sel != NULL)
    {
        while (i < spec->empTargets.selection->numShips)
        {
            sel = spec->empTargets.selection;
            if (sel->ShipPtr[i] == shiptoremove)
            {
                sel->numShips--;
                sel->ShipPtr[i] = sel->ShipPtr[sel->numShips];
            }
            else
            {
                i++;
            }
        }
    }
    if (shiptoremove == spec->empTarget)
    {
        spec->empTarget = NULL;
        if (spec->empState == ACV_EMP_Targeted)
        {
            spec->empState = ACV_EMP_Ready;
        }
    }
}

bool32 cat_acvIsLinking(Ship *ship)                                                         // 0x005add10
{
    return (((ACVSpec *)ship->ShipSpecifics)->empState == ACV_EMP_Pulsing);
}

sdword cat_acvLinkState(Ship *ship)
{
    // CAT: the ACV specifics byte at Ship+0x6F0 that cat_selMakeShipsNotLinkedACVs 0x00511df0 reads inline
    return ((ACVSpec *)ship->ShipSpecifics)->empState;
}

// an EMP from attacker (NULL: none) can hit target (cat_empCanAffectShip 0x005add30; inlined in cat_empPulse and
//      ACVSpecialTarget, which use cat_empShipClampedToPlayer for the salvage/depot test)
bool32 cat_empCanAffectShip(Ship *target, Ship *attacker)                                   // 0x005add30
{
    bool32 affect;
    bool32 salvaged;
    Ship *salvager;
    uword playerIndex;

    if ((0.0f < target->staticinfo->TimeShockedByEMP) ||
        bitTest(target->shipOrDerelict.specialFlags2, SPECIAL_2_CatParalyzed))
    {
        affect = (attacker == NULL) || (target->playerowner != attacker->playerowner);
    }
    else
    {
        affect = FALSE;
    }
    if (target->catSalvageDepot != NULL)
    {
        affect = FALSE;
    }
    if (attacker != NULL)
    {
        playerIndex = attacker->playerowner->playerIndex;
        salvaged = FALSE;
        salvager = cat_salvageAttachedIterFirst(target);
        if ((salvager != NULL) && bitTest(target->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvageTargeted) &&
            (salvager->playerowner->playerIndex == playerIndex))
        {
            salvaged = TRUE;
        }
        if (((target->catSalvageDepot != NULL) && (target->catSalvageDepot->playerowner->playerIndex == playerIndex)) ||
            salvaged)
        {
            affect = FALSE;
        }
    }
    if (target->catDissolving == TRUE)
    {
        return FALSE;
    }
    return affect;
}

// every ACV of group pulses: the ships within EMP_Range of one of them (an RKO within 4000) are shocked with the
//      group's size as the strength (cat_empPulse 0x005ade10)
void cat_empPulse(GrowSelection *group, Ship *ship)                                         // 0x005ade10
{
    GrowSelection hit;
    SelectCommand *acvs;
    ACVSpec *acvspec;
    ACVStatics *acvstat;
    Ship *acv;
    Ship *other;
    Node *node;
    etgeffectstatic *stat;
    etglod *etgLOD;
    vector position;
    vector velocity;
    matrix coordsys;
    real32 dist;
    bool32 chattered;
    sdword i;

    growSelectInit(&hit);
    acvs = group->selection;
    for (i = 0; i < acvs->numShips; i++)
    {
        acv = acvs->ShipPtr[i];
        acvstat = (ACVStatics *)acv->staticinfo->custstatinfo;
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            other = (Ship *)node->structptr;
            if (other == acv)
            {
                continue;
            }
            if (cat_empCanAffectShip(other, acv))
            {
                dist = cat_vecDistance(&acv->obj.posinfo.position, &other->obj.posinfo.position);
                if ((dist < acvstat->empRange) || (cat_shipIsRKO(other) && (dist < ACV_EMP_RKO_RANGE)))
                {
                    growSelectAddShipNoDuplication(&hit, other);
                }
            }
        }
        acvs = group->selection;
    }

    if (hit.selection->numShips < 1)
    {
        speechEventQueue(ship, 0x4000810, ship->shiptype, -1, -1, -1, -1, 0.0f, -1);
    }
    else
    {
        chattered = FALSE;
        soundEventPlay(ship, 0x836, NULL);
        speechEventQueue(ship, 0x40008bd, -1, -1, -1, -1, -1, 0.0f, -1);
        for (i = 0; i < group->selection->numShips; i++)
        {
            acv = group->selection->ShipPtr[i];
            acvspec = (ACVSpec *)acv->ShipSpecifics;
            growSelectClose(&acvspec->empTargets);
            growSelectInit(&acvspec->empTargets);
            acvspec->empTime = universe.totaltimeelapsed;
            acvspec->empState = ACV_EMP_Pulsing;
            etgLOD = etgSpecialPurposeEffectTable[EGT_ELECTRO_MAGNETIC_PULSE];
            if ((etgLOD != NULL) && (etgLOD->nLevels > 0) && ((stat = etgLOD->level[0]) != NULL) &&
                !etgFrequencyExceeded(stat))
            {
                coordsys = acv->rotatable.rotinfo.coordsys;
                position = acv->obj.posinfo.position;
                vecZeroVector(velocity);
                etgEffectCreate(stat, acv, &position, &velocity, &coordsys, 1.0f, EAF_Full, 0);
            }
            acvspec->empTarget = NULL;
        }
        for (i = 0; i < group->selection->numShips; i++)
        {
            if (group->selection->ShipPtr[i] != ship)
            {
                growSelectAddShip(&((ACVSpec *)ship->ShipSpecifics)->empTargets, group->selection->ShipPtr[i]);
            }
        }
        for (i = 0; i < hit.selection->numShips; i++)
        {
            cat_empShockShip(hit.selection->ShipPtr[i], group->selection->numShips);
            growSelectAddShip(&((ACVSpec *)ship->ShipSpecifics)->empTargets, hit.selection->ShipPtr[i]);
            if (!chattered && (hit.selection->ShipPtr[i]->playerowner->playerIndex == universe.curPlayerIndex))
            {
                battleChatterFleetAttempt(-1, 0x49, -1, NULL);
                chattered = TRUE;
            }
        }
    }
    growSelectClose(&hit);
}

bool32 cat_shipIsACV(Ship *ship)                                                            // 0x005ae1b0
{
    return acvIsACV(ship);
}

// the ACVs that pulse together (a formation's charged ACVs, or ship alone) into out; returns the one nearest their
//      centre (cat_acvCollectEMPTargets 0x005ae1d0)
static Ship *cat_acvCollectEMPTargets(Ship *ship, bool32 alone, GrowSelection *out)         // 0x005ae1d0
{
    CommandToDo *command;
    SelectCommand *sel;
    ACVSpec *acvspec;
    Ship *acv;
    Ship *nearest = NULL;
    vector centre = { 0.0f, 0.0f, 0.0f };
    real32 best, dist, scale;
    sdword i;

    if (!(ubyte)alone)
    {
        command = ship->command;
        if ((command == NULL) || !(command->ordertype.attributes & COMMAND_MASK_FORMATION))
        {
            growSelectAddShip(out, ship);
        }
        else
        {
            sel = command->selection;
            for (i = 0; i < sel->numShips; i++)
            {
                acv = sel->ShipPtr[i];
                acvspec = (ACVSpec *)acv->ShipSpecifics;
                if (acvIsACV(acv) && ((acvspec->empState == ACV_EMP_Ready) || (acvspec->empState == ACV_EMP_Targeted)) &&
                    cat_shipHasUpgrade(acv, CAT_UPGRADE_ACVEMP) && !bitTest(acv->header.flags, SOF_Crazy))
                {
                    growSelectAddShip(out, acv);
                }
            }
        }
    }
    else
    {
        growSelectAddShip(out, ship);
    }
    sel = out->selection;
    for (i = 0; i < sel->numShips; i++)
    {
        centre.x += sel->ShipPtr[i]->obj.posinfo.position.x;
        centre.y += sel->ShipPtr[i]->obj.posinfo.position.y;
        centre.z += sel->ShipPtr[i]->obj.posinfo.position.z;
    }
    best = ACV_EMP_NEVER;
    scale = 1.0f / (real32)sel->numShips;
    centre.x = scale * centre.x;
    centre.y = scale * centre.y;
    centre.z = scale * centre.z;
    for (i = 0; i < sel->numShips; i++)
    {
        dist = cat_vecDistance(&centre, &sel->ShipPtr[i]->obj.posinfo.position);
        if (dist < best)
        {
            best = dist;
            nearest = out->selection->ShipPtr[i];
        }
        sel = out->selection;
    }
    return nearest;
}

// the pulse of ACVSpecialActivate and ACVHouseKeep (inlined in both): the group within EMP_Max_Combine_Distance of
//      its centre ship pulses from there if ship is part of it, else ship pulses alone
static void acvFirePulse(Ship *ship, bool32 alone)
{
    GrowSelection group;
    GrowSelection single;
    Ship *centre;
    Ship *acv;
    vector centrepos;
    sdword i;

    growSelectInit(&group);
    centre = cat_acvCollectEMPTargets(ship, alone, &group);
    centrepos = centre->obj.posinfo.position;
    for (i = 0; i < group.selection->numShips; i++)
    {
        acv = group.selection->ShipPtr[i];
        if (((ACVStatics *)acv->staticinfo->custstatinfo)->empMaxCombineDistance <
            cat_vecDistance(&centrepos, &acv->obj.posinfo.position))
        {
            growSelectRemoveShip(&group, acv);
        }
    }
    if ((group.selection->numShips < 2) || !ShipInSelection(group.selection, ship))
    {
        growSelectInit(&single);
        growSelectAddShip(&single, ship);
        cat_empPulse(&single, ship);
        growSelectClose(&single);
    }
    else
    {
        cat_empPulse(&group, centre);
    }
    growSelectClose(&group);
}

static bool32 acvCanPulse(Ship *ship)
{
    ACVSpec *spec = (ACVSpec *)ship->ShipSpecifics;

    return ((spec->empState == ACV_EMP_Ready) || (spec->empState == ACV_EMP_Targeted)) &&
           cat_shipHasUpgrade(ship, CAT_UPGRADE_ACVEMP) && !bitTest(ship->header.flags, SOF_Crazy);
}

void ACVStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)         // 0x005ae360
{
    ACVStatics *acvstat = (statinfo->shiprace != RACE_Sect) ? &ACVStaticBeast : &ACVStaticSect;

    acvstat->empMaxCombineDistance = statinfo->EMP_Max_Combine_Distance;
    acvstat->empRange = statinfo->EMP_Range;
    acvstat->unknown0C = 2400.0f;                   // 0x45160000
    statinfo->custstatinfo = acvstat;
    statinfo->canSpecialBandBoxFriendlies = FALSE;
    statinfo->unknownFlag212 = TRUE;
    statinfo->unknownFlag213 = FALSE;
    scriptSetStruct(directory, filename, AttackSideStepParametersScriptTable, (ubyte *)&acvstat->sidestepParameters);
}

void ACVInit(Ship *ship)                                                                    // 0x005ae3d0
{
    ACVSpec *spec = (ACVSpec *)ship->ShipSpecifics;
    sdword i;

    for (i = 0; i < 0x18; i++)
    {
        spec->linkKills[i / NUM_CLASSES][i % NUM_CLASSES] = 0;
    }
    growSelectClose(&spec->empTargets);
    growSelectInit(&spec->empTargets);
    spec->empTime = universe.totaltimeelapsed;
    spec->empState = ACV_EMP_Pulsing;
    spec->attackPending = FALSE;
    spec->linkStateA = 0;
    spec->linkStateB = 0;
    spec->linkCanLinkA = 0;
    spec->linkCanLinkB = 0;
    attackSideStepInit(&spec->sidestep);
}

bool32 ACVSpecialActivate(Ship *ship)                                                       // 0x005ae450
{
    ACVSpec *spec = (ACVSpec *)ship->ShipSpecifics;

    if (spec->empState == ACV_EMP_Ready)
    {
        if (acvCanPulse(ship))
        {
            acvFirePulse(ship, TRUE);
        }
    }
    else
    {
        spec->empState = ACV_EMP_Ready;
        spec->empTarget = NULL;
    }
    return TRUE;
}

bool32 ACVSpecialTarget(Ship *ship,void *custom)                                            // 0x005ae5a0
{
    ACVSpec *spec = (ACVSpec *)ship->ShipSpecifics;
    SpecialCommand *targets = (SpecialCommand *)custom;
    Ship *target;
    sdword i;

    if ((targets->numTargets > 0) && ((spec->empState == ACV_EMP_Ready) || (spec->empState == ACV_EMP_Targeted)))
    {
        for (i = 0; i < targets->numTargets; i++)
        {
            target = (Ship *)targets->TargetPtr[i];
            if ((target->header.objtype == OBJ_ShipType) && cat_empCanAffectShip(target, ship))
            {
                spec->empTarget = (Ship *)targets->TargetPtr[i];
                spec->empState = ACV_EMP_Targeted;
                spec->attackPending = TRUE;
                return TRUE;
            }
        }
    }
    return TRUE;
}

// order the ACVs of the formation that target the same ship (or ship alone) to attack it (cat_acvLinkedAttack
//      0x005ae720)
static void cat_acvLinkedAttack(Ship *ship)                                                 // 0x005ae720
{
    ACVSpec *spec = (ACVSpec *)ship->ShipSpecifics;
    CommandToDo *command = ship->command;
    SelectCommand *sel;
    ACVSpec *acvspec;
    Ship *acv;
    MaxSelection attackers;
    AttackCommand attack;
    SelectCommand selectone;
    sdword i;

    if ((command != NULL) && (command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
        ((sel = command->selection) != NULL) && (sel->numShips > 1))
    {
        attackers.numShips = 0;
        attackers.ShipPtr[0] = NULL;
        for (i = 0; i < sel->numShips; i++)
        {
            acv = sel->ShipPtr[i];
            acvspec = (ACVSpec *)acv->ShipSpecifics;
            if (acvIsACV(acv) && (acvspec->attackPending != FALSE) && (acvspec->empState == ACV_EMP_Targeted) &&
                (acvspec->empTarget == spec->empTarget))
            {
                attackers.ShipPtr[attackers.numShips] = acv;
                attackers.numShips++;
                acvspec->attackPending = FALSE;
            }
            sel = ship->command->selection;
        }
        attack.TargetPtr[0] = (TargetPtr)spec->empTarget;
        attack.numTargets = 1;
        clAttack(&universe.mainCommandLayer, (SelectCommand *)&attackers, &attack);
        return;
    }
    attack.TargetPtr[0] = (TargetPtr)spec->empTarget;
    selectone.numShips = 1;
    attack.numTargets = 1;
    selectone.ShipPtr[0] = ship;
    clAttack(&universe.mainCommandLayer, &selectone, &attack);
    spec->attackPending = FALSE;
}

real32 cat_acvEMPChargeFraction(Ship *ship)                                                 // 0x005ae840
{
    ACVSpec *spec = (ACVSpec *)ship->ShipSpecifics;
    real32 fraction;

    fraction = (spec->empState == ACV_EMP_Pulsing) ? 0.0f : -1.0f;
    if (spec->empState == ACV_EMP_Recharging)
    {
        fraction = ((universe.totaltimeelapsed - spec->empTime) - ACV_EMP_PULSE_TIME) / ACV_EMP_RECHARGE_TIME;
    }
    else if (spec->empState != ACV_EMP_Pulsing)
    {
        if (!cat_shipHasUpgrade(ship, CAT_UPGRADE_ACVEMP))
        {
            return 0.0f;
        }
        return 1.0f;
    }
    if (fraction <= 1.0f)
    {
        return fraction;
    }
    return 1.0f;
}

void ACVHouseKeep(Ship *ship)                                                               // 0x005ae8c0
{
    ACVSpec *spec = (ACVSpec *)ship->ShipSpecifics;
    real32 elapsed = universe.totaltimeelapsed - spec->empTime;
    Ship *target;
    real32 distsq;

    if ((spec->empState == ACV_EMP_Pulsing) && ((ACV_EMP_PULSE_TIME < elapsed) || (elapsed < 0.0f)))
    {
        growSelectClose(&spec->empTargets);
        growSelectInit(&spec->empTargets);
        spec->empState = ACV_EMP_Recharging;
    }
    if ((spec->empState == ACV_EMP_Recharging) &&
        ((ACV_EMP_RECHARGE_TIME + ACV_EMP_PULSE_TIME < elapsed) || (elapsed < 0.0f)))
    {
        spec->empState = ACV_EMP_Ready;
        spec->empTime = 0.0f;
    }
    if (spec->empState == ACV_EMP_Targeted)
    {
        target = spec->empTarget;
        if (spec->attackPending == TRUE)
        {
            cat_acvLinkedAttack(ship);
            spec->attackPending = FALSE;
        }
        // CAT-TODO(stage 3 FOW): a target the player cannot see (cat_fowCanPlayerSeeShip(owner, target, 1)
        //      0x0047f480) cancels the order (clWrapSpecial with no targets); fog of war is stage 3, every ship is seen
        distsq = (ship->obj.posinfo.position.x - target->obj.posinfo.position.x) *
                 (ship->obj.posinfo.position.x - target->obj.posinfo.position.x) +
                 (ship->obj.posinfo.position.y - target->obj.posinfo.position.y) *
                 (ship->obj.posinfo.position.y - target->obj.posinfo.position.y) +
                 (ship->obj.posinfo.position.z - target->obj.posinfo.position.z) *
                 (ship->obj.posinfo.position.z - target->obj.posinfo.position.z);
        if ((cat_shipIsRKO(target) && (distsq < ACV_EMP_RKO_RANGE * ACV_EMP_RKO_RANGE)) ||
            (distsq < ACV_EMP_TRIGGER_DIST * ACV_EMP_TRIGGER_DIST))
        {
            spec->empState = ACV_EMP_Ready;
            if (acvCanPulse(ship))
            {
                acvFirePulse(ship, FALSE);
            }
            spec->empTarget = NULL;
        }
    }
}

void ACVFire(Ship *ship,SpaceObjRotImpTarg *target)                                         // 0x005aeb90
{
    GunInfo *guninfo = ship->gunInfo;
    Gun *gun;
    sdword i;

    if ((guninfo == NULL) || (guninfo->numGuns <= 0))
    {
        return;
    }
    // by the gun's current weapon type (Gun +0x00), not the static gun type as in DefaultShipFire
    for (i = 0; i < guninfo->numGuns; i++)
    {
        gun = &guninfo->guns[i];
        if (cat_gunCanFire(ship, gun))
        {
            switch (gun->catWeaponType)
            {
                case WEAPON_PROJECTILE:
                case WEAPON_ENERGY:
                case WEAPON_ION:
                case WEAPON_BIGGUN:
                    gunShoot(ship, gun, target);
                    break;
                case WEAPON_MISSILE:
                case WEAPON_MINE:
                    if (gun->numMissiles > 0)
                    {
                        missileShoot(ship, gun, target);
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

void ACVAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)                        // 0x005aec20
{
    ACVSpec *spec = (ACVSpec *)ship->ShipSpecifics;
    ACVStatics *acvstat = (ACVStatics *)ship->staticinfo->custstatinfo;
    CommandToDo *command;
    Ship *lead;

    if ((spec->empState != ACV_EMP_Targeted) || ((SpaceObjRotImpTarg *)spec->empTarget != target))
    {
        if (spec->empState != ACV_EMP_Targeted)
        {
            attackSideStep(ship, target, &spec->sidestep, &acvstat->sidestepParameters);
        }
        if (spec->empState == ACV_EMP_Targeted)
        {
            spec->empState = ACV_EMP_Ready;
            spec->empTarget = NULL;
        }
        return;
    }
    // flying at the EMP target: a formation follows its lead ACV
    command = ship->command;
    if (command != NULL)
    {
        if ((command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
            ((lead = command->selection->ShipPtr[0]) != ship) && acvIsACV(lead) &&
            (spec->empTarget == ((ACVSpec *)lead->ShipSpecifics)->empTarget))
        {
            return;
        }
        if (command->ordertype.attributes & COMMAND_MASK_FORMATION)
        {
            if (command->selection->ShipPtr[0] == ship)
            {
                aishipFlyToPointAvoidingObjsFunc(ship, &target->obj.posinfo.position, 10, command->formation.travelvel,
                                                 NULL);
                ship->shipidle = FALSE;
                processFormationToDo(ship->command, FALSE, FALSE);
            }
            ship->shipidle = FALSE;
            return;
        }
    }
    aishipFlyToPointAvoidingObjsFunc(ship, &target->obj.posinfo.position, 10,
                                     cat_shipGetSpeedMultiplier(ship) * tacticsGetShipsMaxVelocity(ship), NULL);
    ship->shipidle = FALSE;
}

GrowSelection *cat_acvGetSpecifics(Ship *ship)                                              // 0x005aed60
{
    return &((ACVSpec *)ship->ShipSpecifics)->empTargets;
}

void ACV_Save(Ship *ship)                                                                   // 0x005aed70
{
    SaveGrowSelection(&((ACVSpec *)ship->ShipSpecifics)->empTargets);
}

void ACV_Load(Ship *ship)                                                                   // 0x005aed90
{
    LoadGrowSelection(&((ACVSpec *)ship->ShipSpecifics)->empTargets);
}

void ACV_Fix(Ship *ship)                                                                    // 0x005aedb0
{
    ACVSpec *spec = (ACVSpec *)ship->ShipSpecifics;
    sdword i;

    for (i = 0; i < spec->empTargets.selection->numShips; i++)
    {
        spec->empTargets.selection->ShipPtr[i] =
            (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->empTargets.selection->ShipPtr[i]);
    }
    spec->empTarget = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->empTarget);
    spec->link.partner = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->link.partner);
}

void ACV_PreFix(Ship *ship)                                                                 // 0x005aee20
{
    ACVSpec *spec = (ACVSpec *)ship->ShipSpecifics;

    spec->empTarget = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->empTarget);
    spec->link.partner = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->link.partner);
    if (spec->link.effect != NULL)
    {
        spec->link.effect = NULL;
    }
}

bool32 ACVHaltCommand(Ship *ship)                                                           // 0x005aee80
{
    ACVSpec *spec = (ACVSpec *)ship->ShipSpecifics;

    if (spec->empState == ACV_EMP_Targeted)
    {
        spec->empState = ACV_EMP_Ready;
        spec->empTarget = NULL;
    }
    if ((ship->command != NULL) && (ship->command->ordertype.order == COMMAND_CAT_UNLINK))
    {
        return FALSE;
    }
    return (spec->link.partner == NULL);
}

bool32 ACVMoveCommand(Ship *ship)                                                           // 0x005aeed0
{
    ACVSpec *spec = (ACVSpec *)ship->ShipSpecifics;

    if (spec->empState == ACV_EMP_Targeted)
    {
        spec->empState = ACV_EMP_Ready;
        spec->empTarget = NULL;
    }
    return TRUE;
}

// slot 22 points at ACVMoveCommand 0x005aeed0 too (cdecl ignores the extra arguments); C needs the dock signature
static bool32 ACVDockCommand(Ship *ship, sdword dockType, Ship *dockwith)
{
    return ACVMoveCommand(ship);
}

CustShipHeader ACVHeader =
{
    .sizeofShipSpecifics = sizeof(ACVSpec),
    .CustShipStaticInit = ACVStaticInit,
    .CustShipInit = ACVInit,
    .CustShipClose = ACVDied,
    .CustShipFire = ACVFire,
    .CustShipAttackPassive = P1MothershipAttackPassive,
    .CustShipSpecialActivate = ACVSpecialActivate,
    .CustShipSpecialTarget = ACVSpecialTarget,
    .CustShipHousekeep = ACVHouseKeep,
    .CustShipRemoveShipReferences = ACVRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))ACVRemoveShipReferences,
    .CustShipDied = ACVDied,
    .CustShip_PreFix = ACV_PreFix,
    .CustShip_Save = ACV_Save,
    .CustShip_Load = ACV_Load,
    .CustShip_Fix = ACV_Fix,
    .CustShipMoveCommand = ACVMoveCommand,
    .CustShipAttack = ACVAttack,
    .CustShipDockCommand = ACVDockCommand,
    .CustShipHaltCommand = ACVHaltCommand,
};
