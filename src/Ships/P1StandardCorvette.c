// =============================================================================
//  P1StandardCorvette.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/06/1998 by ddunlop
// =============================================================================

#include "P1StandardCorvette.h"

#include <string.h>

#include "AIShip.h"
#include "Attack.h"
#include "CommandLayer.h"
#include "DefaultShip.h"
#include "MCV.h"
#include "Memory.h"
#include "Mimic.h"
#include "Salvage.h"
#include "Universe.h"

// CAT: P1StandardCorvette.c follows cataclysm.exe's block (0x005be610..0x005be8f0): a mimic and a salvager. The
//      mimic half calls the Mimic code (Mimic.c, MCV.c) [M2-R4 ships-b: the p1scMimic* stand-ins are gone].

P1StandardCorvetteStatics P1StandardCorvetteStatic;     // 0x00a7a7e0

void P1StandardCorvetteStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    P1StandardCorvetteStatics *corvstat = &P1StandardCorvetteStatic;

    // CAT: P1StandardCorvetteStaticInit 0x005be610: the side-step keys into +0x08, then the salvage statics; no
    //      memset first
    scriptSetStruct(directory,filename,AttackSideStepParametersScriptTable,(ubyte *)&corvstat->sidestepParameters);
    statinfo->custstatinfo = corvstat;
    cat_salvageStaticInit(directory, filename, statinfo);
}

void P1StandardCorvetteInit(Ship *ship)
{
    P1StandardCorvetteSpec *spec = (P1StandardCorvetteSpec *)ship->ShipSpecifics;

    // CAT: P1StandardCorvetteInit 0x005be660: side-step state, the mimic flag and mimic fields, the salvager
    attackSideStepInit(&spec->attacksidestep);
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
    spec->specialMode = 0;
    cat_salvageInit(ship);
}

void P1StandardCorvetteAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    P1StandardCorvetteSpec *spec = (P1StandardCorvetteSpec *)ship->ShipSpecifics;
    P1StandardCorvetteStatics *corvstat = (P1StandardCorvetteStatics *)((ShipStaticInfo *)ship->staticinfo)->custstatinfo;

    vector heading;
    vector trajectory;

    // CAT: P1StandardCorvetteAttack 0x005be6e0: a mimicking corvette attacks straight on (too-close range 1000)
    //      and, once facing the target (heading . trajectory > 0.5), goes kamikaze (SPECIAL_Kamikaze |
    //      SPECIAL_BrokenFormation); otherwise the HW1 side-step attack
    if (spec->mimic.mode == MIMIC_On)
    {
        attackStraightForward(ship,target,maxdist,1000.0f);
        heading.x = ship->rotatable.rotinfo.coordsys.m13;
        heading.y = ship->rotatable.rotinfo.coordsys.m23;
        heading.z = ship->rotatable.rotinfo.coordsys.m33;
        aishipGetTrajectory(ship,target,&trajectory);
        if (0.5f < trajectory.x * heading.x + trajectory.y * heading.y + trajectory.z * heading.z)
        {
            bitSet(ship->shipOrDerelict.specialFlags, SPECIAL_Kamikaze | SPECIAL_BrokenFormation);
        }
        return;
    }
    attackSideStep(ship,target,&spec->attacksidestep,&corvstat->sidestepParameters);
}

bool32 P1StandardCorvetteSpecialActivate(Ship *ship)                       // CAT: new slot 7 (0x005be7a0)
{
    MimicSpecialActivate(ship);
    return TRUE;
}

bool32 P1StandardCorvetteSpecialTarget(Ship *ship, void *custom)           // CAT: new slot 8 (0x005be7c0)
{
    P1StandardCorvetteSpec *spec = (P1StandardCorvetteSpec *)ship->ShipSpecifics;
    bool32 done;

    // the special-target order is a mimic order (mode 1) or a salvage order (mode 2), set by the UI/AI that
    // issued it; the mode is cleared when the order completes
    switch (spec->specialMode)
    {
        case 1:
            done = MimicSpecialTarget(ship, custom);
            break;
        case 2:
            done = cat_salvageSpecialTarget(ship, custom);
            break;
        default:
            spec->specialMode = 0;
            return TRUE;
    }
    if (done)
    {
        spec->specialMode = 0;
    }
    return done;
}

void P1StandardCorvetteOrderChanged(Ship *ship)                             // CAT: new slot 9 (0x005be830)
{
    P1StandardCorvetteSpec *spec = (P1StandardCorvetteSpec *)ship->ShipSpecifics;

    cat_salvageOrderChanged(ship);
    spec->specialMode = 0;
}

void P1StandardCorvetteHouseKeep(Ship *ship)                                // CAT: new slot 10 (0x005be850)
{
    MimicHouseKeep(ship);
    cat_salvageHouseKeep(ship);
}

void P1StandardCorvetteRemoveShipReferences(Ship *ship, Ship *shiptoremove) // CAT: new slots 11/12 (0x005be870)
{
    MimicRemoveShipReferences(ship, shiptoremove);
    cat_salvageRemoveShipReferences(ship, shiptoremove);
}

void P1StandardCorvetteDied(Ship *ship)                                     // CAT: new slot 15 (0x005be890)
{
    MimicDied(ship);
    cat_salvageDied(ship);
}

void P1StandardCorvette_PreFix(Ship *ship)                                  // CAT: new slot 16 (0x005be8b0)
{
    Mimic_PreFix(ship);
    cat_salvage_PreFix(ship);
}

void P1StandardCorvette_Fix(Ship *ship)                                     // CAT: new slot 19 (0x005be8d0)
{
    Mimic_Fix(ship);
    cat_salvage_Fix(ship);
}

// CAT: the exe's header (P1StandardCorvetteHeader 0x008c5a88: p1StandardCorvette). Slot 6 is MCVAttackPassive and
//      slot 27 MCVHaltCommand (MCV.c) [M2-R4 ships-b: they were copies here].
CustShipHeader P1StandardCorvetteHeader =
{
    .sizeofShipSpecifics = sizeof(P1StandardCorvetteSpec),
    .CustShipStaticInit = P1StandardCorvetteStaticInit,
    .CustShipInit = P1StandardCorvetteInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = MCVAttackPassive,                             // CAT: MCVAttackPassive 0x005b8210
    .CustShipSpecialActivate = P1StandardCorvetteSpecialActivate,          // CAT: new
    .CustShipSpecialTarget = P1StandardCorvetteSpecialTarget,              // CAT: new
    .CustShipOrderChanged = P1StandardCorvetteOrderChanged,                // CAT: new slot 9
    .CustShipHousekeep = P1StandardCorvetteHouseKeep,                      // CAT: new
    .CustShipRemoveShipReferences = P1StandardCorvetteRemoveShipReferences,    // CAT: new
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))P1StandardCorvetteRemoveShipReferences,   // CAT: new slot 12
    .CustShipDied = P1StandardCorvetteDied,                                // CAT: new
    .CustShip_PreFix = P1StandardCorvette_PreFix,                          // CAT: new
    .CustShip_Fix = P1StandardCorvette_Fix,                                // CAT: new
    .CustShipAttack = P1StandardCorvetteAttack,
    .CustShipHaltCommand = MCVHaltCommand,                                 // CAT: new slot 27 (MCVHaltCommand 0x005b82e0)
};
