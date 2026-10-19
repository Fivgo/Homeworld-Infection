// =============================================================================
//  Slipgate.c
//  - Cataclysm's dSlipgateSmall / Medium / Large: paired gates ships dock with
//    to cross to the partner gate (cataclysm.exe 0x005c6fa0-0x005c7780)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each). The gates
//      are opened, paired and closed by the KAS (stage 6); the crossing itself is the dock code's purpose 10
//      (CAT_DOCK_SLIPGATE, Dock.c; M2-R5 dock).

#include "Slipgate.h"

#include "CommandDefs.h"
#include "CommandLayer.h"
#include "Dock.h"
#include "ETG.h"
#include "Mothership.h"
#include "Formation.h"
#include "SaveGame.h"
#include "UnivUpdate.h"
#include "Universe.h"

#define SLIPGATE_OPENING_TIME   2.25f       // 0x0072c018
#define SLIPGATE_CLOSING_TIME   0.625f      // 0x0072c014
#define SLIPGATE_OPEN_TIMEOUT   4.0f        // 0x0072a530
#define SLIPGATE_MAX_WAITING    1000

// CAT: 4-byte blocks nothing reads (dSlipgateSmall 0x00a7a5d0, dSlipgateMedium 0x00a7a5c8, dSlipgateLarge 0x00a7a5cc,
//      dSlipgateNode 0x00a7a5c4)
static udword SlipgateStaticSmall, SlipgateStaticMedium, SlipgateStaticLarge, SlipgateStaticNode;

// the effect of each state at the gate's LOD (EGT_SLIPGATE_* in etgSpecialPurposeEffectTable, the exe's 0x00c41d34 ..
// 0x00c41d3c); the old effect goes first
static void cat_slipgateSetEffect(Ship *gate, ubyte state)                              // 0x005c6fa0
{
    SlipgateSpec *spec = (SlipgateSpec *)gate->ShipSpecifics;
    etglod *etgLOD;
    etgeffectstatic *stat = NULL;

    switch (state)
    {
        case SLIPGATE_Closed:
            cat_effectDeleteNow(spec->effect);
            etgLOD = etgSpecialPurposeEffectTable[EGT_SLIPGATE_PERMANENT_EFFECT];
            spec->stateEndTime = 0.0f;
            break;
        case SLIPGATE_Opening:
        case SLIPGATE_Open:
            cat_effectDeleteNow(spec->effect);
            etgLOD = etgSpecialPurposeEffectTable[EGT_SLIPGATE_OPENING_EFFECT];
            spec->stateEndTime = universe.totaltimeelapsed + SLIPGATE_OPENING_TIME;
            break;
        case SLIPGATE_Closing:
            cat_effectDeleteNow(spec->effect);
            etgLOD = etgSpecialPurposeEffectTable[EGT_SLIPGATE_CLOSING_EFFECT];
            spec->stateEndTime = universe.totaltimeelapsed + SLIPGATE_CLOSING_TIME;
            break;
        default:
            etgLOD = NULL;
            break;
    }
    // CAT: the exe deletes the old effect without clearing the pointer; etgEffectDelete calls every ship's
    //      SlipgateEffectRemoval, which clears it
    if ((etgLOD != NULL) && (gate->obj.currentLOD < etgLOD->nLevels))
    {
        stat = etgLOD->level[gate->obj.currentLOD];
    }
    if ((state < 4) && (stat != NULL))
    {
        spec->effect = etgEffectCreate(stat, gate, NULL, NULL, NULL, 0.0f, EAF_Full, 3,
                                       SCALECAST(Real32ToUdword((state == SLIPGATE_Closed) ? spec->permanentSize
                                                                                            : spec->openingSize)),
                                       0, spec->color);
    }
    spec->state = state;
}

bool32 cat_slipgateOpen(Ship *gate)                                                     // 0x005c70e0
{
    SlipgateSpec *spec = (SlipgateSpec *)gate->ShipSpecifics;
    SlipgateSpec *partnerspec = NULL;
    bool32 inGame = !bitTest(gate->header.flags, SOF_NISShip);

    // in the game both gates open (an NIS gate has no partner)
    if (inGame)
    {
        partnerspec = (SlipgateSpec *)spec->partner->ShipSpecifics;
        spec->openTime = universe.totaltimeelapsed;
        partnerspec->openTime = universe.totaltimeelapsed;
    }
    else
    {
        spec->openTime = 0.0f;
    }
    if (spec->state == SLIPGATE_Closed)
    {
        cat_slipgateSetEffect(gate, SLIPGATE_Opening);
    }
    if (inGame && (partnerspec->state == SLIPGATE_Closed))
    {
        cat_slipgateSetEffect(spec->partner, SLIPGATE_Opening);
    }
    return (spec->state == SLIPGATE_Open) && (!inGame || (partnerspec->state == SLIPGATE_Open));
}

void cat_slipgateClose(Ship *gate)                                                      // 0x005c7180
{
    cat_slipgateSetEffect(gate, SLIPGATE_Closing);
    if (!bitTest(gate->header.flags, SOF_NISShip))
    {
        cat_slipgateSetEffect(((SlipgateSpec *)gate->ShipSpecifics)->partner, SLIPGATE_Closing);
    }
}

void cat_slipgateSetPartner(Ship *gate, Ship *partner)                                  // 0x005c71b0
{
    ((SlipgateSpec *)gate->ShipSpecifics)->partner = partner;
}

void SlipgateStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)  // 0x005c71c0
{
    (void)directory;
    (void)filename;
    switch (statinfo->shiptype)
    {
        case dSlipgateSmall:    statinfo->custstatinfo = &SlipgateStaticSmall;  break;
        case dSlipgateMedium:   statinfo->custstatinfo = &SlipgateStaticMedium; break;
        case dSlipgateLarge:    statinfo->custstatinfo = &SlipgateStaticLarge;  break;
        case dSlipgateNode:     statinfo->custstatinfo = &SlipgateStaticNode;   break;
        default:                statinfo->custstatinfo = statinfo;              break;
    }
}

void SlipgateInit(Ship *ship)                                                           // 0x005c7250
{
    // the size of each gate's effects and its colour (a stack table in the exe)
    static const struct { ShipType type; real32 openingSize; real32 permanentSize; udword color; } gates[3] =
    {
        { dSlipgateSmall,  1250.0f,  5000.0f, 0xff0000ff },
        { dSlipgateMedium, 2600.0f, 10400.0f, 0xff00ffff },
        { dSlipgateLarge, 11100.0f, 44400.0f, 0xffff0000 },
    };
    SlipgateSpec *spec = (SlipgateSpec *)ship->ShipSpecifics;
    sdword i;

    spec->state = SLIPGATE_Closed;
    spec->openTime = 0.0f;
    spec->partner = NULL;
    spec->effect = NULL;
    spec->unknown1C = 0;
    spec->unknown20 = 0;
    bitSet(ship->shipOrDerelict.specialFlags, SPECIAL_FriendlyStatus);
    bitSet(ship->header.catFlags18, CAT_SHIPFLAG_Invincible);
    bitClear(ship->header.flags, SOF_Selectable);
    bitSet(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSlipgate);
    for (i = 0; i < 3; i++)
    {
        if (gates[i].type == ship->shiptype)
        {
            spec->openingSize = gates[i].openingSize;
            spec->permanentSize = gates[i].permanentSize;
            spec->color = gates[i].color;
            break;
        }
    }
    growSelectInit(&spec->waiting);
}

void cat_slipgateAddShip(Ship *gate, Ship *ship)                                        // 0x005c73b0
{
    growSelectAddShip(&((SlipgateSpec *)gate->ShipSpecifics)->waiting, ship);
}

// the ships that protect ship, or parade around it, dock with the gate as well
static void cat_slipgateDockWaitingShips(Ship *gate, Ship *ship)                        // 0x005c73d0
{
    static Ship *candidates[SLIPGATE_MAX_WAITING];
    static ubyte selectionBuffer[sizeof(SelectCommand) + sizeof(ShipPtr) * (SLIPGATE_MAX_WAITING - 1)];
    SelectCommand *selection = (SelectCommand *)selectionBuffer;
    CommandToDo *command;
    Node *node;
    sdword num = 0, i;

    for (node = universe.mainCommandLayer.todolist.head; node != NULL; node = node->next)
    {
        command = (CommandToDo *)listGetStructOfNode(node);
        if (((command->ordertype.attributes & COMMAND_MASK_PROTECTING) && (command->protect != NULL) &&
             ShipInSelection((SelectCommand *)command->protect, ship)) ||
            ((command->ordertype.order == COMMAND_MILITARY_PARADE) && (command->militaryParade != NULL) &&
             (command->militaryParade->aroundShip == ship)))
        {
            for (i = 0; i < command->selection->numShips; i++)
            {
                if (num < SLIPGATE_MAX_WAITING)
                {
                    candidates[num++] = command->selection->ShipPtr[i];
                }
            }
        }
    }
    selection->numShips = 0;
    for (i = 0; i < num; i++)
    {
        if (ThisShipCanDockWith(candidates[i], gate, CAT_DOCK_SLIPGATE, FALSE))     // the exe's purpose 10 (M2-R5 dock)
        {
            selection->ShipPtr[selection->numShips++] = candidates[i];
        }
    }
    if (selection->numShips > 0)
    {
        clDock(&universe.mainCommandLayer, selection, CAT_DOCK_SLIPGATE, gate);
    }
}

void SlipgateHouseKeep(Ship *ship)                                                      // 0x005c74d0
{
    SlipgateSpec *spec = (SlipgateSpec *)ship->ShipSpecifics;
    sdword i;

    if (spec->waiting.selection != NULL)
    {
        for (i = 0; i < spec->waiting.selection->numShips; i++)
        {
            cat_slipgateDockWaitingShips(ship, spec->waiting.selection->ShipPtr[i]);
        }
        growSelectReset(&spec->waiting);
    }
    // an open gate closes again (with its partner) 4 s after it was opened
    if ((spec->effect != NULL) && (spec->openTime != 0.0f) &&
        (spec->openTime + SLIPGATE_OPEN_TIMEOUT < universe.totaltimeelapsed))
    {
        cat_slipgateSetEffect(ship, SLIPGATE_Closing);
        if (!bitTest(ship->header.flags, SOF_NISShip))
        {
            cat_slipgateSetEffect(spec->partner, SLIPGATE_Closing);
        }
        spec->openTime = 0.0f;
    }
    if (((spec->state == SLIPGATE_Opening) || (spec->state == SLIPGATE_Closing)) && (spec->stateEndTime != 0.0f) &&
        (spec->stateEndTime < universe.totaltimeelapsed))
    {
        if (spec->state == SLIPGATE_Opening)
        {
            spec->state = SLIPGATE_Open;
        }
        else if (spec->state == SLIPGATE_Closing)
        {
            spec->state = SLIPGATE_Closed;
            cat_effectDeleteNow(spec->effect);
        }
        spec->stateEndTime = 0.0f;
    }
    if (spec->effect == NULL)
    {
        cat_slipgateSetEffect(ship, spec->state);
    }
}

void SlipgateDied(Ship *ship)                                                           // 0x005c7620
{
    SlipgateSpec *spec = (SlipgateSpec *)ship->ShipSpecifics;

    cat_effectDeleteNow(spec->effect);
    if (spec->waiting.selection != NULL)
    {
        growSelectClose(&spec->waiting);
    }
}

void SlipgateRemoveShipReferences(Ship *ship,Ship *shiptoremove)                        // 0x005c7670
{
    SlipgateSpec *spec = (SlipgateSpec *)ship->ShipSpecifics;

    // CAT: the exe compares the partner with the gate itself, not with shiptoremove (0x005c7674: cmp [eax+0x638],eax),
    //      so a dead partner is never cleared; kept
    if (spec->partner == ship)
    {
        spec->partner = NULL;
    }
    if ((spec->waiting.selection != NULL) && (spec->waiting.selection->numShips > 0))
    {
        growSelectRemoveShip(&spec->waiting, shiptoremove);
    }
}

void SlipgateEffectRemoval(Ship *ship, struct Effect *effect)                           // 0x005c76b0
{
    SlipgateSpec *spec = (SlipgateSpec *)ship->ShipSpecifics;

    if (spec->effect == effect)
    {
        spec->effect = NULL;
        if (spec->state == SLIPGATE_Opening)
        {
            spec->state = SLIPGATE_Open;
        }
        else if (spec->state == SLIPGATE_Closing)
        {
            spec->state = SLIPGATE_Closed;
        }
    }
}

void Slipgate_PreFix(Ship *ship)                                                        // 0x005c76f0
{
    SlipgateSpec *spec = (SlipgateSpec *)ship->ShipSpecifics;

    spec->effect = NULL;
    spec->partner = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->partner);
}

void Slipgate_Save(Ship *ship)                                                          // 0x005c7720
{
    SaveGrowSelection(&((SlipgateSpec *)ship->ShipSpecifics)->waiting);
}

void Slipgate_Load(Ship *ship)                                                          // 0x005c7740
{
    LoadGrowSelectionAndFix(&((SlipgateSpec *)ship->ShipSpecifics)->waiting);
}

void Slipgate_Fix(Ship *ship)                                                           // 0x005c7760
{
    SlipgateSpec *spec = (SlipgateSpec *)ship->ShipSpecifics;

    spec->partner = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->partner);
}

// CAT: SlipgateHeader 0x008c68c0 (dSlipgateSmall, dSlipgateMedium, dSlipgateLarge); slot 4 is SlipgateDied as well
CustShipHeader SlipgateHeader =
{
    .sizeofShipSpecifics = sizeof(SlipgateSpec),
    .CustShipStaticInit = SlipgateStaticInit,
    .CustShipInit = SlipgateInit,
    .CustShipClose = SlipgateDied,
    .CustShipHousekeep = SlipgateHouseKeep,
    .CustShipRemoveShipReferences = SlipgateRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))SlipgateRemoveShipReferences,
    .CustShipEffectRemoval = SlipgateEffectRemoval,
    .CustShipDied = SlipgateDied,
    .CustShip_PreFix = Slipgate_PreFix,
    .CustShip_Save = Slipgate_Save,
    .CustShip_Load = Slipgate_Load,
    .CustShip_Fix = Slipgate_Fix,
};
