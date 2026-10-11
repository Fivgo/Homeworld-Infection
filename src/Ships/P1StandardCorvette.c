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
#include "Memory.h"
#include "Salvage.h"
#include "Universe.h"

// CAT: P1StandardCorvette.c follows cataclysm.exe's block (0x005be610..0x005be8f0): a mimic and a salvager. The
//      mimic half calls the Mimic code (MimicHeader, Cataclysm-only; MimicSpecialActivate 0x005b8c60 and friends),
//      which is M2-R4 #4 (ship code B); those calls go through the p1scMimic* stand-ins below, each CAT-PROVISIONAL
//      and doing nothing until M2-R4 #4 points them at its Mimic functions. No mimic mode is ever entered before
//      that, so the corvette fights and salvages only.

P1StandardCorvetteStatics P1StandardCorvetteStatic;     // 0x00a7a7e0

static void p1scMimicSpecialActivate(Ship *ship) { (void)ship; }                            // MimicSpecialActivate
static bool32 p1scMimicSpecialTarget(Ship *ship, void *custom) { (void)ship; (void)custom; return TRUE; }   // MimicSpecialTarget
static void p1scMimicHouseKeep(Ship *ship) { (void)ship; }                                   // MimicHouseKeep 0x005b8f30
static void p1scMimicRemoveShipReferences(Ship *ship, Ship *shiptoremove) { (void)ship; (void)shiptoremove; }
static void p1scMimicDied(Ship *ship) { (void)ship; }                                        // MimicDied
static void p1scMimic_PreFix(Ship *ship) { (void)ship; }                                     // Mimic_PreFix
static void p1scMimic_Fix(Ship *ship) { (void)ship; }                                        // Mimic_Fix

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
    bitSet(ship->catShipFlags, CAT_SHIPFLAG_Mimic);
    spec->mimicMode = 0;
    spec->mimicUnknown124 = 0;
    spec->mimicDataD8 = NULL;
    spec->mimicUnknownE4 = -1;
    spec->mimicApparentType = -1;
    spec->mimicUnknownD0 = -1;
    spec->mimicUnknownE8 = -1;
    spec->mimicUnknown110 = -1;
    spec->mimicUnknownF8 = -1;
    spec->mimicUnknownFC = -1;
    spec->mimicUnknown114 = -1;
    spec->mimicData104 = NULL;
    spec->mimicResource = 0;
    spec->specialMode = 0;
    cat_salvageInit(ship);
}

void P1StandardCorvetteAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    P1StandardCorvetteSpec *spec = (P1StandardCorvetteSpec *)ship->ShipSpecifics;
    P1StandardCorvetteStatics *corvstat = (P1StandardCorvetteStatics *)((ShipStaticInfo *)ship->staticinfo)->custstatinfo;

    // CAT: P1StandardCorvetteAttack 0x005be6e0: a mimicking corvette attacks straight on (too-close range 1000)
    //      and, once facing the target (heading . trajectory > 0.5), sets specialFlags 0x6 in the exe's numbering
    //      (CAT-PROVISIONAL: the specialFlags renumbering is M2-R4 #2 and mimic mode is never entered before M2-R4 #4,
    //      so the flag is not set here); otherwise the HW1 side-step attack
    if (spec->mimicMode == 1)
    {
        attackStraightForward(ship,target,maxdist,1000.0f);
        return;
    }
    attackSideStep(ship,target,&spec->attacksidestep,&corvstat->sidestepParameters);
}

void P1StandardCorvetteAttackPassive(Ship *ship,Ship *target,bool32 rotate)
{
    P1StandardCorvetteSpec *spec = (P1StandardCorvetteSpec *)ship->ShipSpecifics;

    // CAT: slot 6 is MCVAttackPassive 0x005b8210 (shared with MCVHeader): a mimicking corvette in a type-5 formation
    //      sets specialFlags 0x6 (exe numbering, CAT-PROVISIONAL as in P1StandardCorvetteAttack) and does not fire;
    //      a mimicking one never fires passively; otherwise HW1's passive attack
    if (spec->mimicMode == 1)
    {
        return;
    }
    if (spec->mimicMode == 0)
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
}

bool32 P1StandardCorvetteSpecialActivate(Ship *ship)                       // CAT: new slot 7 (0x005be7a0)
{
    p1scMimicSpecialActivate(ship);
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
            done = p1scMimicSpecialTarget(ship, custom);
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
    p1scMimicHouseKeep(ship);
    cat_salvageHouseKeep(ship);
}

void P1StandardCorvetteRemoveShipReferences(Ship *ship, Ship *shiptoremove) // CAT: new slots 11/12 (0x005be870)
{
    p1scMimicRemoveShipReferences(ship, shiptoremove);
    cat_salvageRemoveShipReferences(ship, shiptoremove);
}

void P1StandardCorvetteDied(Ship *ship)                                     // CAT: new slot 15 (0x005be890)
{
    p1scMimicDied(ship);
    cat_salvageDied(ship);
}

void P1StandardCorvette_PreFix(Ship *ship)                                  // CAT: new slot 16 (0x005be8b0)
{
    p1scMimic_PreFix(ship);
    cat_salvage_PreFix(ship);
}

void P1StandardCorvette_Fix(Ship *ship)                                     // CAT: new slot 19 (0x005be8d0)
{
    p1scMimic_Fix(ship);
    cat_salvage_Fix(ship);
}

bool32 P1StandardCorvetteHaltCommand(Ship *ship)
{
    P1StandardCorvetteSpec *spec = (P1StandardCorvetteSpec *)ship->ShipSpecifics;

    // CAT: new slot 27: the exe's MCVHaltCommand 0x005b82e0 = MimicHaltCommand 0x005b99e0: a halt during a
    //      transformation (mode 8) ends it and frees the mimic data
    if (spec->mimicMode == 8)
    {
        spec->mimicMode = 0;
        if (spec->mimicDataD8 != NULL)
        {
            memFree(spec->mimicDataD8);
            spec->mimicDataD8 = NULL;
        }
        if (spec->mimicData104 != NULL)
        {
            memFree(spec->mimicData104);
            spec->mimicData104 = NULL;
        }
        // CAT-PROVISIONAL: cat_univDeleteMimicResource(spec->mimicResource) (Universe, M2-R4 #4); never set before then
        spec->mimicResource = 0;
    }
    return TRUE;
}

// CAT: the exe's header (P1StandardCorvetteHeader 0x008c5a88: p1StandardCorvette). Slot 6 is MCVAttackPassive and
//      slot 27 MCVHaltCommand (shared with MCVHeader, Cataclysm-only); their bodies are this file's
//      P1StandardCorvetteAttackPassive / P1StandardCorvetteHaltCommand.
CustShipHeader P1StandardCorvetteHeader =
{
    .sizeofShipSpecifics = sizeof(P1StandardCorvetteSpec),
    .CustShipStaticInit = P1StandardCorvetteStaticInit,
    .CustShipInit = P1StandardCorvetteInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = P1StandardCorvetteAttackPassive,
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
    .CustShipHaltCommand = P1StandardCorvetteHaltCommand,                  // CAT: new slot 27
};
