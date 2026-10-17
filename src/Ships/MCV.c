// =============================================================================
//  MCV.c
//  - Cataclysm's sMCV (mimic corvette): a side-stepping corvette with the
//    mimic block (cataclysm.exe 0x005b80f0-0x005b8310)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each); most
//      forward to the Mimic code (Mimic.c).

#include "MCV.h"

#include "CommandDefs.h"
#include "CommandLayer.h"
#include "FormationDefs.h"
#include "P1StandardCorvette.h"
#include "StatScript.h"

static AttackSideStepParameters MCVStatic;     // 0x00a7ac30

void MCVStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)          // 0x005b80f0
{
    statinfo->canSpecialBandBoxFriendlies = TRUE;
    statinfo->unknownFlag212 = TRUE;
    statinfo->unknownFlag213 = TRUE;
    statinfo->custstatinfo = &MCVStatic;
    scriptSetStruct(directory, filename, AttackSideStepParametersScriptTable, (ubyte *)&MCVStatic);
}

void MCVInit(Ship *ship)                                                                    // 0x005b8130
{
    MCVSpec *spec = (MCVSpec *)ship->ShipSpecifics;
    sdword i;

    for (i = 0; i < 0x18; i++)
    {
        spec->linkKills[i / NUM_CLASSES][i % NUM_CLASSES] = 0;
    }
    bitSet(ship->header.catFlags18, CAT_SHIPFLAG_Mimic);
    spec->mimic.mode = MIMIC_Off;
    spec->mimic.fade = 0.0f;
    spec->mimic.apparentBindings = NULL;
    spec->mimic.apparentPlayer = -1;
    spec->mimic.apparentType = -1;
    spec->mimic.apparentRace = -1;
    spec->mimic.apparentAsteroid = -1;
    spec->mimic.nextPlayer = -1;
    spec->mimic.nextType = -1;
    spec->mimic.nextRace = -1;
    spec->mimic.nextAsteroid = -1;
    spec->mimic.nextBindings = NULL;
    spec->mimic.fakeResource = NULL;
    spec->mimic.ramDamageScale = 2.0f;              // 0x40000000
    attackSideStepInit(&spec->sidestep);
}

void MCVAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)                        // 0x005b81d0
{
    attackStraightForward(ship, target, maxdist, 1000.0f);
    bitSet(ship->shipOrDerelict.specialFlags, SPECIAL_Kamikaze | SPECIAL_BrokenFormation);
}

void MCVAttackPassive(Ship *ship, Ship *target, bool32 rotate)                              // 0x005b8210
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;
    CommandToDo *command = ship->command;

    if ((spec->mode == MIMIC_On) && (command != NULL) && (command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
        (command->formation.formationtype == SPHERE_FORMATION))
    {
        bitSet(ship->shipOrDerelict.specialFlags, SPECIAL_Kamikaze | SPECIAL_BrokenFormation);
        return;
    }
    if (spec->mode == MIMIC_Off)
    {
        if ((rotate) & ((bool32)((ShipStaticInfo *)(ship->staticinfo))->rotateToRetaliate))
        {
            attackPassiveRotate(ship, target);
            return;
        }
        attackPassive(ship, target);
    }
}

void cat_mcvUnmimic(Ship *ship, bool32 animate)                                             // 0x005b8280
{
    cat_mimicUnmimic(ship, animate);
}

bool32 MCVSpecialTarget(Ship *ship, void *custom)                                           // 0x005b82a0
{
    MimicSpecialTarget(ship, custom);
    return TRUE;
}

void MCVHouseKeep(Ship *ship)                                                               // 0x005b82c0
{
    MimicHouseKeep(ship);
}

void MCVDied(Ship *ship)                                                                    // 0x005b82d0
{
    MimicDied(ship);
}

bool32 MCVHaltCommand(Ship *ship)                                                           // 0x005b82e0
{
    return MimicHaltCommand(ship);
}

void MCV_PreFix(Ship *ship)                                                                 // 0x005b82f0
{
    Mimic_PreFix(ship);
}

void MCV_Fix(Ship *ship)                                                                    // 0x005b8300
{
    Mimic_Fix(ship);
}

void MCVRemoveShipReferences(Ship *ship, Ship *shiptoremove)                                // 0x005b8310
{
    MimicRemoveShipReferences(ship, shiptoremove);
}

// CAT: MCVHeader 0x008c4d10 (sMCV). Slot 5 is GenericInterceptorFire 0x005b22c0; slot 7 is
//      P1StandardCorvetteSpecialActivate 0x005be7a0 (MimicSpecialActivate, return 1)
CustShipHeader MCVHeader =
{
    .sizeofShipSpecifics = sizeof(MCVSpec),
    .CustShipStaticInit = MCVStaticInit,
    .CustShipInit = MCVInit,
    .CustShipFire = GenericInterceptorFire,
    .CustShipAttackPassive = MCVAttackPassive,
    .CustShipSpecialActivate = P1StandardCorvetteSpecialActivate,
    .CustShipSpecialTarget = MCVSpecialTarget,
    .CustShipHousekeep = MCVHouseKeep,
    .CustShipRemoveShipReferences = MCVRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))MCVRemoveShipReferences,
    .CustShipDied = MCVDied,
    .CustShip_PreFix = MCV_PreFix,
    .CustShip_Fix = MCV_Fix,
    .CustShipAttack = MCVAttack,
    .CustShipHaltCommand = MCVHaltCommand,
};
