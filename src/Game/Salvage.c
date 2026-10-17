// =============================================================================
//  Salvage.c
//  - Cataclysm's salvage system: salvagers (Worker, SalCap corvettes,
//    p1StandardCorvette) that tow targets to salvage depots (carriers,
//    motherships, processors, bHeavyCruiser) (cataclysm.exe 0x004f7a90-0x004fd600)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R3 ships-a: blocks and init; M2-R4 combat: the runtime). Every body follows the exe function
//      named on it (r6 decompile, disassembly where the decompile is unclear). The exe's debug prints of
//      "SALVAGING: ..." go to a stripped nullsub in the retail build and are left out.

#include "Salvage.h"
#include "Dissolve.h"
#include "Crystal.h"

#include <string.h>
#ifndef _MSC_VER
    #include <strings.h>
#endif

#include "AIShip.h"
#include "AITrack.h"
#include "Alliance.h"
#include "Attributes.h"
#include "Battle.h"
#include "BeastWeapon.h"
#include "Carrier.h"
#include "Clamp.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "ConsMgr.h"
#include "Debug.h"
#include "Dock.h"
#include "ETG.h"
#include "Globals.h"
#include "GravWellGenerator.h"
#include "HeavyCruiser.h"
#include "KASFunc.h"
#include "MeshAnim.h"
#include "Memory.h"
#include "Mothership.h"
#include "P1Mothership.h"
#include "P1StandardCorvette.h"
#include "Processor.h"
#include "SalCapCorvette.h"
#include "SaveGame.h"
#include "Select.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "SpeechEvent.h"
#include "StatScript.h"
#include "Tactics.h"
#include "Tweak.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "Vector.h"
#include "Worker.h"

#ifdef _MSC_VER
    #define strcasecmp _stricmp
#endif

/*=============================================================================
    Data
=============================================================================*/

// the statics of salvagers without their own block, and their key table (cat_salvageStaticInit 0x004f90e0: the
// block 0x00a57768, the table 0x008922d8 whose only key is salvageFlyToDistance)
static CatSalvageStatics cat_salvageDefaultStatics;
static CatSalvagerInfo cat_salvageDefaultSalvagerInfo;      // 0x00a57770, for any other type
static CatSalvageDepot cat_salvageDefaultDepot;             // 0x00a577a0, for any other type

static Ship *cat_salvageAIDepotShip = NULL;                 // 0x00a57798: the AI's preferred depot
static real32 cat_salvageTooStrongTime = 0.0f;              // 0x00a5779c: last "target too strong" message (real time)

// the attached-salvager iterator (cat_salvageNextAttachedSalvager 0x004fc650: 0x00a578e4/e8/ec)
static Ship *cat_salvageIterTowed = NULL;
static sdword cat_salvageIterCount = 0;
static sdword cat_salvageIterIndex = 0;

scriptStructEntry cat_salvageStaticScriptTable[] =
{
    { "salvageFlyToDistance", scriptSetReal32CB, &(cat_salvageDefaultStatics.salvageFlyToDistance), &(cat_salvageDefaultStatics) },

    END_SCRIPT_STRUCT_ENTRY
};

// CAT: GetShipStaticInfo 0x0053ea50 takes the type only; this tree's still takes a race, which it ignores
#define cat_salvageStaticInfo(type)     GetShipStaticInfo((type), RACE_Sect)

// CAT (M2-R5 layout-dock): SpaceObj.attributes is the exe's dword at +0x38; the salvagers test its bit 0x40000
//      (CAT_ATTRIBUTES_DockWithEnemies, Attributes.h) directly

/*=============================================================================
    The per-ship blocks
=============================================================================*/

static DockStaticPoint *cat_salvageFindDockStaticPoint(char *name, DockStaticInfo *dockstaticinfo)
{
    // CAT: cat_dockFindDockStaticPoint 0x00463d00 (Dock.c); this wrapper only adds a NULL test for a type without
    //      dock points (the exe reads the pointer unchecked; every salvager has dock points)
    if (dockstaticinfo == NULL)
    {
        return NULL;
    }
    return cat_dockFindDockStaticPoint(name, dockstaticinfo);
}

CatSalvagerInfo *cat_salvageGetSalvagerInfo(Ship *ship)                     // 0x004f8ec0
{
    ubyte *specifics = (ubyte *)ship->ShipSpecifics;

    switch (ship->shiptype)
    {
        case sWorker:
        case bWorker:
            return (CatSalvagerInfo *)(specifics + offsetof(WorkerSpec, salvager));            // exe +0x0C
        case kSalCapCorvette:
        case tSalCapCorvette:
            return (CatSalvagerInfo *)(specifics + offsetof(CatSalCapCorvetteSpec, salvager));     // exe +0x08
        case p1StandardCorvette:
            return (CatSalvagerInfo *)(specifics + offsetof(P1StandardCorvetteSpec, salvager));    // exe +0x13C
        default:
            return &cat_salvageDefaultSalvagerInfo;
    }
}

// CAT (M2-R4 command): the salvager types (cat_shipIsSalvager 0x004f8f60: sWorker, bWorker, kSalCapCorvette,
//      tSalCapCorvette, p1StandardCorvette; trJunkYardDawg reaches a compiled-out nullsubReturnFalse and is no salvager)
bool32 cat_shipIsSalvager(Ship *ship)                                       // 0x004f8f60
{
    switch (ship->shiptype)
    {
        case sWorker:
        case bWorker:
        case kSalCapCorvette:
        case tSalCapCorvette:
        case p1StandardCorvette:
            return TRUE;
        default:
            return FALSE;
    }
}

// CAT (M2-R4 combat): the salvager's current target, the block's +0x04 (read by RemoveShipFromBeingTargeted 0x004453c0
//      as *(cat_salvageGetSalvagerInfo(ship) + 4))
Ship *cat_salvageGetTarget(Ship *ship)
{
    return cat_salvageGetSalvagerInfo(ship)->target;
}

CatSalvageDepot *cat_salvageGetDepotInfo(Ship *ship)                        // 0x004f91c0
{
    ubyte *specifics = (ubyte *)ship->ShipSpecifics;

    switch (ship->shiptype)
    {
        case sCarrier:
        case bCarrier:
        case kCarrier:
        case tCarrier:
        case dKushanCarrier:
            return (CatSalvageDepot *)(specifics + offsetof(CarrierSpec, depot));          // exe +0x1C
        case sProcessor:
        case bProcessor:
            return (CatSalvageDepot *)(specifics + offsetof(ProcessorSpec, depot));        // exe +0x00
        case p1Mothership:
            return (CatSalvageDepot *)(specifics + offsetof(P1MothershipSpec, depot));     // exe +0x00
        case sMothership:
        case bMothership:
        case trGhostship:
            return (CatSalvageDepot *)(specifics + offsetof(MothershipSpec, depot));       // exe +0xAC
        case bHeavyCruiser:
            return (CatSalvageDepot *)(specifics + offsetof(HeavyCruiserSpec, depot));     // exe +0x18
        default:
            return &cat_salvageDefaultDepot;
    }
}

// the salvage statics of a salvager (the switch in cat_salvageStaticInit 0x004f90e0 and cat_salvageSpecialTarget)
static CatSalvageStatics *cat_salvageGetStatics(ShipStaticInfo *statinfo)
{
    switch (statinfo->shiptype)
    {
        case sWorker:
        case bWorker:
            return &((WorkerStatics *)statinfo->custstatinfo)->salvage;
        case kSalCapCorvette:
        case tSalCapCorvette:
            return &((CatSalvagerStatics *)statinfo->custstatinfo)->salvage;
        case p1StandardCorvette:
            return &((P1StandardCorvetteStatics *)statinfo->custstatinfo)->salvage;
        default:
            return &cat_salvageDefaultStatics;
    }
}

// the Worker's mode byte at specifics +0x08 (Ship+0x640), 1 = a salvaging worker (cat_salvageDeliverToDepot)
static ubyte cat_salvageWorkerMode(Ship *ship)
{
    return ((WorkerSpec *)ship->ShipSpecifics)->specialMode;
}

// frees a dock point a ship holds on host (the block inlined at every user of Ship+0x615 in the exe)
static void cat_salvageFreePoint(Ship *ship, Ship *host)
{
    sbyte index = ship->catSalvagePointIndex;
    DockStaticPoint *point;

    if (index != -1)
    {
        point = &host->staticinfo->dockStaticInfo->dockstaticpoints[index];
        host->dockInfo->catNumBusyOfType[point->type]--;
        host->dockInfo->thisDockBusy[index] = 0;
        ship->catSalvagePointIndex = -1;
    }
}

// the number of salvage points of a type (statinfo->dockStaticInfo numOfType[DPT_Salvage] + [DPT_SalvageLatch]),
// -1 when it has none (cat_salvageSpecialTarget states 9/10; cat_salvageCheckTarget tests < 1)
static sdword cat_salvageNumPoints(Ship *target)
{
    DockStaticInfo *dockstaticinfo = target->staticinfo->dockStaticInfo;
    sdword num;

    if (dockstaticinfo == NULL)
    {
        return -1;
    }
    num = dockstaticinfo->numOfType[DPT_SalvageLatch] + dockstaticinfo->numOfType[DPT_Salvage];
    return (num < 1) ? -1 : num;
}

// removes target from the special-order targets of every salvaging salvager (inlined in cat_salvageDeliverToDepot
// 0x004fa050 and cat_salvageNoDepotAbort 0x004fa700)
static void cat_salvageRemoveFromSalvageOrders(Ship *target)
{
    Node *node;
    Ship *ship;
    CommandToDo *command;
    SelectCommand *targets;
    sdword i;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (!cat_shipIsSalvager(ship))
        {
            continue;
        }
        command = getShipAndItsCommand(&universe.mainCommandLayer, ship);
        if ((command != NULL) && (command->ordertype.order == COMMAND_SPECIAL) && (command->specialtargets != NULL) &&
            (!cat_shipIsLiveWorker(ship) || (cat_salvageWorkerMode(ship) == 1)))
        {
            targets = (SelectCommand *)command->specialtargets;
            i = 0;
            while (i < targets->numShips)
            {
                if (targets->ShipPtr[i] == target)
                {
                    targets->numShips--;
                    targets->ShipPtr[i] = targets->ShipPtr[targets->numShips];
                }
                else
                {
                    i++;
                }
            }
        }
    }
}

/*=============================================================================
    Queries
=============================================================================*/

void cat_salvageResetGlobals(void)                                          // 0x004f7a90
{
    cat_salvageAIDepotShip = NULL;
    cat_salvageTooStrongTime = 0.0f;
}

void cat_salvageBeamEffect(Ship *salvager)                                  // 0x004f7ab0
{
    CatSalvagerInfo *info = cat_salvageGetSalvagerInfo(salvager);
    etglod *etgLOD = etgTractorBeamEffectTable[salvager->shiprace];
    etgeffectstatic *stat = NULL;

    info->tractorBeam = TRUE;
    if ((etgLOD != NULL) && (salvager->obj.currentLOD < etgLOD->nLevels))
    {
        stat = etgLOD->level[salvager->obj.currentLOD];
    }
    if ((stat != NULL) && !etgFrequencyExceeded(stat))
    {
        salvager->rceffect = etgEffectCreate(stat, salvager, NULL, NULL, NULL, salvager->magnitudeSquared,
                                             SOF_AttachPosition|SOF_AttachCoordsys|SOF_AttachNLips, 0);
        salvager->rceffect->obj.posinfo.velocity.x = 0.0f;
        salvager->rceffect->obj.posinfo.velocity.y = 0.0f;
        salvager->rceffect->obj.posinfo.velocity.z = 0.0f;
        info->tractorBeam = TRUE;
        univUpdateObjRotInfo((SpaceObjRot *)salvager->rceffect);
        // CAT: Cataclysm's salvage-beam sound event 0x816 (the exe's literal)
        salvager->soundevent.specialHandle = soundEventPlay(salvager, 0x816, NULL);
        return;
    }
    salvager->rceffect = NULL;
}

bool32 cat_salvageIsBeingSalvaged(Ship *ship)                               // 0x004f7c00
{
    return bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingSalvaged) ? TRUE : FALSE;
}

// the per-type dock point type a depot takes a dockable target to (the byte table at 0x004f7ce9 in
// cat_salvageDepotCanAccept, the same switch in cat_salvageApproachTarget)
static sdword cat_salvageDepotPointType(ShipType targettype, Ship *depot)
{
    switch (targettype)
    {
        case dBigGunUnit:
            // 0x12 + (0xB if not): DPT_Destroyer_EB at an sMothership, else DPT_SalvageProcess [M2-R4 combat resume:
            //      the WIP had DPT_Swarmer_In/DPT_Mothership_Out, 21/34, a miscount of the enum]
            return (depot->shiptype == sMothership) ? DPT_Destroyer_EB : DPT_SalvageProcess;
        case dMessagePod:
        case dBuoy:
            return DPT_Med_In;
        case dCrystal0:
        case dCrystal1:
            return DPT_Crystal_Process;
        default:
            return -1;
    }
}

bool32 cat_salvageDepotCanAccept(Ship *depot, Ship *target)                 // 0x004f7c20
{
    sdword type;

    if (!bitTest(depot->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvageDepot))
    {
        return FALSE;
    }
    if ((singlePlayerGame) && (depot->shiptype == kCarrier))
    {
        return FALSE;
    }
    if (bitTest(depot->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
    {
        return FALSE;
    }
    if ((!singlePlayerGame) && !cat_shipIsCrystal(target) && (depot->shiprace == RACE_Beast) &&
        !cat_shipHasUpgrade(depot, 0x10000))
    {
        return FALSE;
    }
    switch (target->shiptype)
    {
        case dBigGunUnit:
        case dMessagePod:
        case dBuoy:
            type = cat_salvageDepotPointType(target->shiptype, depot);
            return depot->staticinfo->dockStaticInfo->numOfType[type] > 0;
        case dCrystal0:
        case dCrystal1:
            return cat_shipHasUpgrade(depot, 0x100000);
        default:
            return TRUE;
    }
}

bool32 cat_salvageTargetDamagedEnough(Ship *target)                         // 0x004f7d70
{
    real32 limit;

    if ((singlePlayerGame) && (singlePlayerGameInfo.currentMission == 8) && (kasfMissionSkillGet() < 2))
    {
        return TRUE;
    }
    if (cat_shipIsCrystal(target))
    {
        return TRUE;
    }
    if ((target->shiptype == dMessagePod) || (target->shiptype == dBuoy) || (target->shiptype == dBigGunUnit))
    {
        return TRUE;
    }
    if ((singlePlayerGame) && (singlePlayerGameInfo.currentMission == 8) && (kasfMissionSkillGet() == 2))
    {
        limit = 0.75f;
    }
    else
    {
        limit = twSalvageDamageLimit[target->staticinfo->shipclass];
        // the exe prints "SALVAGING: Damage limit for ship class %s is not between 0 and 1." when out of range
    }
    return target->targetable.health <= limit * target->staticinfo->health.maxhealth;
}

bool32 cat_playerHasSalvageUpgradeShip(Player *player)                      // 0x004f7e70
{
    Node *node;
    Ship *ship;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner == player) && cat_shipHasUpgrade(ship, 0x100000))
        {
            return TRUE;
        }
    }
    return FALSE;
}

// any ship of player with upgrade 0x10000 that is not a bCruiseMissile (the loop inlined twice in the exe)
static bool32 cat_salvagePlayerHasBeastUpgradeShip(Player *player)
{
    Node *node;
    Ship *ship;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner == player) && cat_shipHasUpgrade(ship, 0x10000) && (ship->shiptype != bCruiseMissile))
        {
            return TRUE;
        }
    }
    return FALSE;
}

// the type a Beast ship turns back into: a non-Beast type whose beastAttackConvertTo is it (the loop over the 145
// types inlined in cat_salvageCheckTarget and cat_salvageConvertType), or -1
static ShipType cat_salvageUnconvertType(ShipType beasttype)
{
    ShipType type;
    ShipStaticInfo *statinfo;

    for (type = 0; type < TOTAL_NUM_SHIPS; type++)
    {
        statinfo = cat_salvageStaticInfo(type);
        if ((statinfo->beastAttacksRequired != -1) && (statinfo->beastAttackConvertTo == beasttype) &&
            ((type < bRecon) || (type > bMissileCorvette)))
        {
            return type;
        }
    }
    return -1;
}

bool32 cat_salvageCheckTarget(Player *player, Ship *target, sword *reasons) // 0x004f7eb0
{
    sword count[CAT_SALVAGE_NUM_REASONS];
    sdword i;
    bool32 ok;

    memset(count, 0, sizeof(count));

    if (target->header.objtype == OBJ_ShipType)
    {
        if (!bitTest(target->obj.attributes, CAT_ATTRIBUTES_DockWithEnemies) &&
            ((player == target->playerowner) || allianceIsShipAlly(target, player)))
        {
            count[CAT_SALVAGE_REASON_Friendly]++;
        }
        if (bitTest(target->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
        {
            count[CAT_SALVAGE_REASON_Forbidden]++;
        }
        if (cat_salvageNumPoints(target) < 1)
        {
            count[CAT_SALVAGE_REASON_NoPoints]++;
        }
        if (bitTest(target->header.catFlags18, CAT_SHIPFLAG_Orphaned) && (tutorial != 1))
        {
            count[CAT_SALVAGE_REASON_NoPoints]++;
        }
        if (!cat_cmHasSupportUnits(target->shiptype, player))
        {
            count[CAT_SALVAGE_REASON_NoSupportType]++;
        }
        if (cat_shipIsCrystal(target))
        {
            // CAT-TODO(stage 5 multiplayer options): the multiplayer game option bit 0x20 of 0x008860ec (crystals salvageable) is a game
            //      option of the Cataclysm tpGameCreated layout; until it is ported crystals are not salvageable in
            //      multiplayer, as with the bit clear
            if (!singlePlayerGame)
            {
                count[CAT_SALVAGE_REASON_Forbidden]++;
            }
            else if (!cat_playerHasSalvageUpgradeShip(player))
            {
                count[CAT_SALVAGE_REASON_NoCrystalUpgrade]++;
            }
        }
        if ((player->race == RACE_Beast) && !cat_shipIsCrystal(target) && !singlePlayerGame &&
            !cat_salvagePlayerHasBeastUpgradeShip(player))
        {
            count[CAT_SALVAGE_REASON_NoBeastUpgrade]++;
        }
        switch (player->race)
        {
            case RACE_Beast:
                if (!singlePlayerGame)
                {
                    if (!cat_shipIsCrystal(target) && (target->staticinfo->beastAttacksRequired == -1))
                    {
                        count[CAT_SALVAGE_REASON_NoSupport]++;
                    }
                }
                else if ((target->shiptype != dMessagePod) && (target->shiptype != dBigGunUnit) &&
                         (target->shiptype != dBuoy) && !cat_shipIsCrystal(target) &&
                         (target->staticinfo->beastAttacksRequired == -1))
                {
                    count[CAT_SALVAGE_REASON_NoSupport]++;
                }
                break;
            case RACE_Kushan:
            case RACE_Taiidan:
            case RACE_P1:
            case RACE_Traders:
            case RACE_Derelict:
                // the exe prints "SALVAGING: %s is using Sect's conversion rules for salvaging." and uses them
            case RACE_Sect:
                if (!singlePlayerGame && (target->shiptype >= bRecon) && (target->shiptype <= bMissileCorvette) &&
                    (cat_salvageUnconvertType(target->shiptype) == -1))
                {
                    count[CAT_SALVAGE_REASON_NoSupport]++;
                }
                break;
            default:
                // the exe prints "SALVAGING: Unhandled race."
                break;
        }
        switch (target->shiptype)
        {
            case sWorker:
            case bWorker:
            case kSalCapCorvette:
            case tSalCapCorvette:
            case p1StandardCorvette:
                if (cat_salvageGetSalvagerInfo(target)->target != NULL)
                {
                    count[CAT_SALVAGE_REASON_Forbidden]++;
                }
                break;
            default:
                break;
        }
        switch (target->shiptype)
        {
            case kGravWellGenerator:
            case tGravWellGenerator:
                // the exe reads Ship+0x658, the grav well's GravDerelict: a spent grav well cannot be salvaged
                if (((GravWellGeneratorSpec *)target->ShipSpecifics)->GravDerelict == 0)
                {
                    break;
                }
                count[CAT_SALVAGE_REASON_Forbidden]++;
                break;
            case sMothership:
            case bMothership:
            case trFloatingCity:
            case trCargoBarge:
            case trMiningBase:
            case trResearchStation:
            case trJunkYardDawg:
            case trJunkYardHQ:
            case trJunk_LGun:
            case trJunk_SGun:
            case dBeastMothership:
                count[CAT_SALVAGE_REASON_Forbidden]++;
                break;
            default:
                break;
        }
    }
    else if (target->header.objtype == OBJ_AsteroidType)
    {
        count[CAT_SALVAGE_REASON_Asteroid] = 1;
    }
    else if (target->header.objtype == OBJ_DustType)
    {
        count[CAT_SALVAGE_REASON_Dust] = 1;
        count[CAT_SALVAGE_REASON_Forbidden] = 0;
    }
    else
    {
        count[CAT_SALVAGE_REASON_NoPoints] = 1;
    }

    ok = TRUE;
    for (i = 0; i < CAT_SALVAGE_NUM_REASONS; i++)
    {
        if (count[i] != 0)
        {
            ok = FALSE;
        }
        if (reasons != NULL)
        {
            reasons[i] += count[i];
        }
    }
    return ok;
}

bool32 cat_salvageCanSalvage(Player *player, Ship *target)                 // 0x004f83f0
{
    return cat_salvageCheckTarget(player, target, NULL);
}

bool32 cat_salvageCheckTargetReasons(Player *player, Ship *target, sword *reasons)     // 0x004f8410
{
    return cat_salvageCheckTarget(player, target, reasons);
}

Ship *cat_salvageChooseTarget(Ship *salvager, SelectCommand *targets, sdword *pointIndex)   // 0x004f8430
{
    sdword i, first = -1, nearest = -1;
    real32 distSq, nearestDistSq = 1e21f;
    Ship *target;
    uword playerIndex = salvager->playerowner->playerIndex;
    sdword points;

    *pointIndex = 0;
    for (i = 0; i < targets->numShips; i++)
    {
        target = targets->ShipPtr[i];
        if (!cat_salvageCheckTarget(salvager->playerowner, target, NULL))
        {
            continue;
        }
        points = cat_salvageNumPoints(target);
        if (points < 0)
        {
            points = 0;
        }
        if (points == target->shipOrDerelict.salvageNumTagged[playerIndex])
        {
            // every salvage point already has a salvager of this player
            if (*pointIndex == 0)
            {
                *pointIndex = 1;
                first = i;
            }
            continue;
        }
        if (!cat_salvageTargetDamagedEnough(target) && (!singlePlayerGame || (playerIndex != 1)))
        {
            if (*pointIndex == 0)
            {
                *pointIndex = 2;
                first = i;
            }
            continue;
        }
        if ((target->shipOrDerelict.salvageNumTagged[playerIndex] > 0) &&
            (target->shipOrDerelict.salvageNumTagged[playerIndex] < points))
        {
            // a target already being salvaged by this player that still needs salvagers is taken at once
            nearest = i;
            break;
        }
        distSq = (target->obj.posinfo.position.x - salvager->obj.posinfo.position.x) * (target->obj.posinfo.position.x - salvager->obj.posinfo.position.x) +
                 (target->obj.posinfo.position.y - salvager->obj.posinfo.position.y) * (target->obj.posinfo.position.y - salvager->obj.posinfo.position.y) +
                 (target->obj.posinfo.position.z - salvager->obj.posinfo.position.z) * (target->obj.posinfo.position.z - salvager->obj.posinfo.position.z);
        if (distSq < nearestDistSq)
        {
            nearest = i;
            nearestDistSq = distSq;
        }
    }
    if (nearest != -1)
    {
        *pointIndex = 0;
        return targets->ShipPtr[nearest];
    }
    if (*pointIndex == 0)
    {
        return NULL;
    }
    return targets->ShipPtr[first];
}

Ship *cat_salvageFindNearestDepot(Ship *salvager, Ship *target)            // 0x004f85f0
{
    Ship *depots[16];
    udword numDepots = 0, i;
    Node *node;
    Ship *ship, *nearest = NULL;
    real32 distSq, nearestDistSq = 3.40282347e+38f;     // 0x7f7fffff

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner == salvager->playerowner) &&
            bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvageDepot) && (numDepots < 16))
        {
            depots[numDepots++] = ship;
        }
    }
    // the exe prints "Found more depot ships than the list could hold" / "No depot ships were found"
    for (i = 0; i < numDepots; i++)
    {
        ship = depots[i];
        if (cat_salvageDepotCanAccept(ship, target))
        {
            distSq = (target->obj.posinfo.position.x - ship->obj.posinfo.position.x) * (target->obj.posinfo.position.x - ship->obj.posinfo.position.x) +
                     (target->obj.posinfo.position.y - ship->obj.posinfo.position.y) * (target->obj.posinfo.position.y - ship->obj.posinfo.position.y) +
                     (target->obj.posinfo.position.z - ship->obj.posinfo.position.z) * (target->obj.posinfo.position.z - ship->obj.posinfo.position.z);
            if (distSq < nearestDistSq)
            {
                nearestDistSq = distSq;
                nearest = ship;
            }
        }
    }
    return nearest;
}

static void cat_salvageAlignToVelocity(Ship *ship)                          // 0x004f86e0
{
    vector heading = ship->obj.posinfo.velocity;

    vecNormalize(&heading);
    aitrackHeadingFunc((SpaceObjRotImpTargGuidance *)ship, &heading, 0.999f, 0.999f, 0.0f, 0);
}

bool32 cat_salvageGetTargetAttachPoint(Ship *salvager, Ship *target)        // 0x004f8730
{
    CatSalvagerInfo *info = cat_salvageGetSalvagerInfo(salvager);
    DockStaticInfo *dockstaticinfo = target->staticinfo->dockStaticInfo;
    sdword i, found = -1, lastSalvage = -1;
    DockStaticPoint *point;

    // a free latch point first, else the last free salvage point
    for (i = 0; i < dockstaticinfo->numDockPoints; i++)
    {
        point = &dockstaticinfo->dockstaticpoints[i];
        if ((point->type == DPT_SalvageLatch) && (target->dockInfo->thisDockBusy[i] == 0))
        {
            found = i;
            break;
        }
        if ((point->type == DPT_Salvage) && (target->dockInfo->thisDockBusy[i] == 0))
        {
            lastSalvage = i;
        }
        found = lastSalvage;
    }
    if (found == -1)
    {
        return FALSE;
    }
    point = &dockstaticinfo->dockstaticpoints[found];
    target->dockInfo->thisDockBusy[found] = 1;
    target->dockInfo->catNumBusyOfType[point->type]++;
    info->attachPoint = point;
    return TRUE;
}

/*=============================================================================
    The salvager
=============================================================================*/

void cat_salvageDetach(Ship *salvager)                                      // 0x004f88b0
{
    CatSalvagerInfo *info = cat_salvageGetSalvagerInfo(salvager);
    Ship *target;
    sdword anim;
    Node *node;
    Ship *ship;
    CatSalvagerInfo *other;
    sdword salvagePlayer;

    cat_shipSetDisableReason(salvager, 0, 2);
    bitClear(salvager->shipOrDerelict.specialFlags2, SPECIAL_2_CatNoAvoidance);
    if (salvager->guidance.clampInfo != NULL)
    {
        unClampObj((SpaceObjRotImpTargGuidance *)salvager);
        info->noDamageTargetTime = universe.totaltimeelapsed;
        info->noDamageTarget = info->target;
    }
    if (salvager->rceffect != NULL)
    {
        etgEffectDelete(salvager->rceffect);
        univRemoveObjFromRenderList((SpaceObj *)salvager->rceffect);
        listDeleteNode(&salvager->rceffect->header.objlink);
        cat_salvageGetSalvagerInfo(salvager)->tractorBeam = FALSE;
        salvager->rceffect = NULL;
    }
    if (info->attachPoint != NULL)
    {
        anim = cat_salvageOpenAnimIndex(salvager->shiptype);
        if ((anim != -1) && (salvager->madBindings->nCurrentAnim == anim))
        {
            anim = cat_salvageCloseAnimIndex(salvager->shiptype);
            if ((anim != -1) && (salvager->madBindings->nCurrentAnim != anim))
            {
                // CAT-TODO(stage 3): the exe passes madAnimationStart 0x004b6b90 a third argument, 2 (MeshAnim.c)
                madAnimationStart(salvager, anim);
            }
        }
        target = info->target;
        if (info->attachPoint != NULL)
        {
            // the exe indexes the busy words by the point's dockindex field (+0x04)
            target->dockInfo->thisDockBusy[info->attachPoint->dockindex] = 0;
            target->dockInfo->catNumBusyOfType[info->attachPoint->type]--;
            info->attachPoint = NULL;
        }
        info->attachPoint = NULL;
    }
    target = info->target;
    if (target != NULL)
    {
        uword playerIndex = salvager->playerowner->playerIndex;

        if ((target->shiptype == dBigGunUnit) && (salvager->catSalvageBigGun != NULL) &&
            (salvager->catSalvageBigGun->shiptype == dBigGun))
        {
            salvager->catSalvageBigGun = NULL;
        }
        target->shipOrDerelict.salvageNumTagged[playerIndex]--;
        if (target->shipOrDerelict.salvageNumTagged[playerIndex] < 0)
        {
            target->shipOrDerelict.salvageNumTagged[playerIndex] = 0;
        }
        if (target->shipOrDerelict.salvageNumTagged[playerIndex] == 0)
        {
            target->catSalvagePlayer = -1;
        }
        if ((info->dockwith != NULL) && (cat_salvageNextAttachedSalvager(target) == NULL) &&
            (target->catSalvageBigGun == info->dockwith))
        {
            target->catSalvageBigGun = NULL;
        }
        if ((info->dockwith != NULL) && (target->catSalvageDepot == NULL) && (target->catSalvagePointIndex != -1))
        {
            cat_salvageFreePoint(target, info->dockwith);
        }
        // the target stays claimed by its player only while a salvager of that player still has it
        if (target->catSalvagePlayer != -1)
        {
            salvagePlayer = target->catSalvagePlayer;
            if (target->shipOrDerelict.salvageNumTagged[salvagePlayer] > 0)
            {
                for (node = universe.ShipList.head; node != NULL; node = node->next)
                {
                    ship = (Ship *)listGetStructOfNode(node);
                    if ((ship->playerowner->playerIndex == salvagePlayer) && cat_shipIsSalvager(ship))
                    {
                        other = cat_salvageGetSalvagerInfo(ship);
                        if ((other->target == target) && (other->state > CAT_SALVAGE_FLYTOTARGET))
                        {
                            goto stillclaimed;
                        }
                    }
                }
            }
            target->catSalvagePlayer = -1;
        }
stillclaimed:
        if (cat_salvageNextAttachedSalvager(target) == NULL)
        {
            target->catSalvageNotifyTime = CAT_SALVAGE_NEVER;
            bitClear(target->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvageTargeted);
        }
        if ((target->catSalvageDepot == NULL) && (cat_salvageNextAttachedSalvager(target) == NULL))
        {
            bitClear(target->header.flags, SOF_Disabled);
            bitClear(target->header.flags, SOF_DontDrawTrails);
        }
        if (target->catSalvageDepot == NULL)
        {
            bitSet(target->header.flags, SOF_Selectable);
        }
        info->target = NULL;
    }
    info->state = CAT_SALVAGE_CHOOSE;
    info->target = NULL;
    info->dockwith = NULL;
    info->tractorBeam = FALSE;
    info->noAvoid = FALSE;
    info->noDamageTargetTime = 0.0f;
    info->noDamageTarget = NULL;
    info->attachPoint = NULL;
    bitClear(salvager->shipOrDerelict.specialFlags, SPECIAL_BrokenFormation);
}

void cat_salvageStaticInit(char *directory, char *filename, ShipStaticInfo *statinfo)    // 0x004f90e0
{
    CatSalvageStatics *salstat;

    // sWorker/bWorker and the SalCap corvettes keep the salvage statics at +0x0C of their own statics, p1StandardCorvette
    // at +0x00; any other type shares one block
    switch (statinfo->shiptype)
    {
        case sWorker:
        case bWorker:
            salstat = &((WorkerStatics *)statinfo->custstatinfo)->salvage;
            break;
        case kSalCapCorvette:
        case tSalCapCorvette:
            salstat = &((CatSalvagerStatics *)statinfo->custstatinfo)->salvage;
            break;
        case p1StandardCorvette:
            salstat = &((P1StandardCorvetteStatics *)statinfo->custstatinfo)->salvage;
            break;
        default:
            salstat = &cat_salvageDefaultStatics;
            break;
    }
    salstat->salvageFlyToDistance = 0.0f;
    salstat->centerDockPoint = NULL;
    scriptSetStruct(directory, filename, cat_salvageStaticScriptTable, salstat);
    salstat->centerDockPoint = cat_salvageFindDockStaticPoint("center", statinfo->dockStaticInfo);
}

void cat_salvageInit(Ship *ship)                                            // 0x004f9010
{
    // CAT: the exe's specialFlags bit 0x80000 marks a salvager (SPECIAL_CatSalvager, SpaceObj.h)
    bitSet(ship->shipOrDerelict.specialFlags, SPECIAL_CatSalvager);
    cat_salvageDetach(ship);
    cat_salvageGetSalvagerInfo(ship)->forceDetach = FALSE;
}

// the exe's unnamed tail of the special target: stop and steady the target (cat_salvageSpecialTarget, the code at
// switchD_004faf5e_caseD_f; states 3, 4 and 5 share it)
static void cat_salvageDampTarget(Ship *target)
{
    if (target->rowGetOutOfWay != NULL)
    {
        switch (target->rowGetOutOfWay->shiptype)
        {
            case sWorker:
            case bWorker:
            case kSalCapCorvette:
            case tSalCapCorvette:
            case p1StandardCorvette:
                rowRemoveShipFromGettingOutOfWay(target);
                break;
            default:
                break;
        }
    }
    target->obj.posinfo.velocity.x *= 0.3f;
    target->obj.posinfo.velocity.y *= 0.3f;
    target->obj.posinfo.velocity.z *= 0.3f;
    target->rotatable.rotinfo.rotspeed.x *= 0.3f;
    target->rotatable.rotinfo.rotspeed.y *= 0.3f;
    target->rotatable.rotinfo.rotspeed.z *= 0.3f;
}

// the number of this player's salvagers clamped to target (states 9 and 10)
static sdword cat_salvageNumAttached(Ship *target, Player *player)
{
    CatClampedList *clamped;
    Ship *ship;
    sdword i, num = 0;

    if ((target->guidance.clampInfo == NULL) || (target->guidance.clampInfo->clampedObjs == NULL))
    {
        return 0;
    }
    clamped = target->guidance.clampInfo->clampedObjs;
    for (i = 0; i < clamped->numShips; i++)
    {
        ship = clamped->ShipPtr[i];
        if ((ship->header.objtype == OBJ_ShipType) && (ship->playerowner == player) && cat_shipIsSalvager(ship))
        {
            num++;
        }
    }
    return num;
}

// every salvager clamped to target tows it to depot (states 8 and cat_salvageRemoveShipReferences)
static void cat_salvageSetDepotForAll(Ship *target, Ship *depot)
{
    Ship *ship;

    for (ship = cat_salvageNextAttachedSalvager(target); ship != NULL; ship = cat_salvageNextAttachedSalvager(NULL))
    {
        cat_salvageGetSalvagerInfo(ship)->dockwith = depot;
    }
}

static bool32 cat_salvageFlyToTarget(Ship *salvager, Ship *target)          // 0x004fa3a0
{
    real32 speedmult = cat_shipGetSpeedMultiplier(salvager);
    real32 maxvel = tacticsGetShipsMaxVelocity(salvager);

    if (MoveReachedDestinationVariable(salvager, &target->obj.posinfo.position, 6000.0f))
    {
        return TRUE;
    }
    aishipFlyToPointAvoidingObjsFunc(salvager, &target->obj.posinfo.position, 0, maxvel * speedmult, NULL);
    cat_salvageAlignToVelocity(salvager);
    return FALSE;
}

// the towed ship (salvager) approaches the depot (target) and takes one of its dock points (cat_salvageApproachTarget
// 0x004fa410: used for message pods, buoys and crystals, which a depot takes in through a dock point)
static bool32 cat_salvageApproachTarget(Ship *salvager, Ship *target)
{
    real32 speedmult = cat_shipGetSpeedMultiplier(salvager);
    real32 maxvel = tacticsGetShipsMaxVelocity(salvager);
    real32 arrival = 50.0f;
    sdword type, wanted, i, numDockPoints;
    sbyte index;
    DockStaticPoint *point;
    vector dest;

    if (!MoveReachedDestinationVariable(salvager, &target->obj.posinfo.position, 6000.0f))
    {
        aishipFlyToPointAvoidingObjsFunc(salvager, &target->obj.posinfo.position, 0, maxvel * speedmult, NULL);
        cat_salvageAlignToVelocity(salvager);
        return FALSE;
    }

    if (salvager->catSalvagePointIndex == -1)
    {
        switch (salvager->shiptype)
        {
            case dBigGunUnit:
                type = (target->shiptype == sMothership) ? DPT_Destroyer_EB : DPT_SalvageProcess;     // 0x12 / 0x1d
                break;
            case dMessagePod:
            case dBuoy:
                type = DPT_Med_In;
                break;
            case dCrystal0:
            case dCrystal1:
                type = DPT_Crystal_Process;
                break;
            default:
                type = -1;
                break;
        }
        wanted = type;
        if ((target->shiptype == sMothership) && !cat_playerHasCompletedShip(sMothershipDockingBay, target->playerowner) &&
            (type == DPT_Med_In))
        {
            wanted = DPT_Med_In_Engineering;
        }
        numDockPoints = target->staticinfo->dockStaticInfo->numDockPoints;
        index = 0;
        for (i = 0; i < numDockPoints; i = index)
        {
            if ((target->staticinfo->dockStaticInfo->dockstaticpoints[i].type == (DockPointType)wanted) &&
                (target->dockInfo->thisDockBusy[i] == 0))
            {
                if (index != -1)
                {
                    // CAT: the exe counts the busy point under the unadjusted type (Med_In for the Engineering point)
                    target->dockInfo->catNumBusyOfType[type]++;
                    target->dockInfo->thisDockBusy[index] = 1;
                    salvager->catSalvagePointIndex = index;
                }
                break;
            }
            index++;
        }
    }

    if (salvager->catSalvagePointIndex == -1)
    {
        aitrackStabilizeGuidance((SpaceObjRotImpTargGuidance *)salvager);
        aitrackZeroVelocity(salvager);
        return FALSE;
    }

    point = &target->staticinfo->dockStaticInfo->dockstaticpoints[salvager->catSalvagePointIndex];
    dest.x = point->maxdist * point->conenormal.x + point->position.x;
    dest.y = point->maxdist * point->conenormal.y + point->position.y;
    dest.z = point->maxdist * point->conenormal.z + point->position.z;
    matMultiplyMatByVec(&dest, &target->rotatable.rotinfo.coordsys, &dest);
    vecAddTo(dest, target->obj.posinfo.position);
    if ((salvager->shiptype == dCrystal0) || (salvager->shiptype == dCrystal1))
    {
        arrival = 150.0f;
    }
    if (MoveReachedDestinationVariable(salvager, &dest, arrival))
    {
        return TRUE;
    }
    if ((salvager->catSalvageBigGun != target) &&
        (0.0f < (target->obj.posinfo.position.x - dest.x) * (target->obj.posinfo.position.x - salvager->obj.posinfo.position.x) +
                (target->obj.posinfo.position.y - dest.y) * (target->obj.posinfo.position.y - salvager->obj.posinfo.position.y) +
                (target->obj.posinfo.position.z - dest.z) * (target->obj.posinfo.position.z - salvager->obj.posinfo.position.z)))
    {
        salvager->catSalvageBigGun = target;
    }
    aishipFlyToPointAvoidingObjsFunc(salvager, &dest, AISHIP_PointInDirectionFlying, 0.0f, NULL);
    return FALSE;
}

// no depot will take the target: tell the player why, release every salvager on it and drop it from every
// salvage order (cat_salvageNoDepotAbort 0x004fa700)
static void cat_salvageNoDepotAbort(Ship *salvager)
{
    CatSalvagerInfo *info = cat_salvageGetSalvagerInfo(salvager);
    Ship *target = info->target;
    CatClampedList *clamped;
    Ship *ship;
    CommandToDo *command;
    sdword i;

    // CAT-TODO(stage 3): for the current player the exe shows a command message (clCommandMessage 0x004ac820, red):
    //      a crystal without a ship with upgrade 0x100000, or a Beast player without a ship with upgrade 0x10000
    //      (string table entries 0x00bb4e18 / 0x00bb4e1c)

    if ((target->guidance.clampInfo != NULL) && (target->guidance.clampInfo->clampedObjs != NULL))
    {
        clamped = target->guidance.clampInfo->clampedObjs;
        for (i = clamped->numShips - 1; i >= 0; i--)
        {
            ship = target->guidance.clampInfo->clampedObjs->ShipPtr[i];
            command = getShipAndItsCommand(&universe.mainCommandLayer, ship);
            if ((command != NULL) && (command->ordertype.order == COMMAND_SPECIAL) && cat_shipIsSalvager(ship))
            {
                cat_salvageDetach(ship);
            }
            if ((target->guidance.clampInfo == NULL) || (target->guidance.clampInfo->clampedObjs == NULL))
            {
                break;
            }
        }
    }
    cat_salvageRemoveFromSalvageOrders(target);
}

bool32 cat_salvageSpecialTarget(Ship *ship, void *custom)                  // 0x004faad0
{
    SelectCommand *targets = (SelectCommand *)custom;
    CatSalvagerInfo *info = cat_salvageGetSalvagerInfo(ship);
    CatSalvageStatics *statics = cat_salvageGetStatics(ship->staticinfo);
    Ship *target, *depot;
    sdword pointIndex, anim, num;
    SelectCommand selectone;
    bool32 (*approach)(Ship *, Ship *);
    bool32 changed;

    if (targets == NULL)
    {
        return TRUE;
    }
    if (!cat_shipHasUpgrade(ship, 0x80000))
    {
        return TRUE;
    }
    if ((info->target != NULL) && (info->target->guidance.clampInfo != NULL) &&
        (info->target->guidance.clampInfo->host != NULL) &&
        (info->state != CAT_SALVAGE_CHOOSE) && (info->state != CAT_SALVAGE_FLYTOTARGET))
    {
        // the target has been clamped to something else
        cat_salvageDetach(ship);
    }

    switch (info->state)
    {
        case CAT_SALVAGE_CHOOSE:
            if ((targets->numShips != 0) || (info->target != NULL))
            {
                pointIndex = 0;
                target = cat_salvageChooseTarget(ship, targets, &pointIndex);
                if (target != NULL)
                {
                    if (pointIndex != 0)
                    {
                        // wait near a target that is full or too strong
                        if (!cat_MoveReachedDestinationRadius(ship, &target->obj.posinfo.position, statics->salvageFlyToDistance + statics->salvageFlyToDistance))
                        {
                            aishipFlyToPointAvoidingObjsFunc(ship, &target->obj.posinfo.position, AISHIP_PointInDirectionFlying, 0.0f, NULL);
                        }
                        else
                        {
                            aitrackStabilizeGuidance((SpaceObjRotImpTargGuidance *)ship);
                            aitrackZeroVelocity(ship);
                        }
                        if ((pointIndex == 2) && (ship->playerowner == universe.curPlayerPtr) &&
                            (twNotifySalvageTargetTooStrongRate + cat_salvageTooStrongTime < universe.realtimeelapsed))
                        {
                            // CAT-TODO(stage 3): the exe shows the "target too strong" command message here
                            //      (clCommandMessage 0x004ac820, string 0x00bb4f08, red)
                            cat_salvageTooStrongTime = universe.realtimeelapsed;
                        }
                        return FALSE;
                    }
                    target->shipOrDerelict.salvageNumTagged[ship->playerowner->playerIndex]++;
                    info->target = target;
                    info->state = CAT_SALVAGE_FLYTOTARGET;
                    if (target->shiptype == dBigGunUnit)
                    {
                        Node *node;
                        Ship *biggun = NULL;

                        for (node = universe.ShipList.head; node != NULL; node = node->next)
                        {
                            if (((Ship *)listGetStructOfNode(node))->shiptype == dBigGun)
                            {
                                biggun = (Ship *)listGetStructOfNode(node);
                                break;
                            }
                        }
                        ship->catSalvageBigGun = biggun;
                    }
                    return FALSE;
                }
            }
            break;

        case CAT_SALVAGE_FLYTOTARGET:
            aishipFlyToPointAvoidingObjsFunc(ship, &info->target->obj.posinfo.position, AISHIP_PointInDirectionFlying, 0.0f, NULL);
            if (!cat_MoveReachedDestinationRadius(ship, &info->target->obj.posinfo.position, statics->salvageFlyToDistance))
            {
                return FALSE;
            }
            if ((info->target->catSalvagePlayer != -1) &&
                (info->target->catSalvagePlayer != ship->playerowner->playerIndex))
            {
                // another player's salvagers hold it
                aitrackStabilizeGuidance((SpaceObjRotImpTargGuidance *)ship);
                aitrackZeroVelocity(ship);
                return FALSE;
            }
            info->target->catSalvagePlayer = ship->playerowner->playerIndex;
            info->state = CAT_SALVAGE_GETPOINT;
            return FALSE;

        case CAT_SALVAGE_GETPOINT:
            if (!cat_salvageGetTargetAttachPoint(ship, info->target))
            {
                cat_salvageDetach(ship);
                info->state = CAT_SALVAGE_CHOOSE;
                return FALSE;
            }
            info->state = CAT_SALVAGE_CONEINSIDE;
            return FALSE;

        case CAT_SALVAGE_CONEINSIDE:
            if (cat_dockFlyToConeInside(ship, info->target, info->attachPoint))
            {
                bitSet(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatNoAvoidance);
                info->noAvoid = TRUE;
                info->state = CAT_SALVAGE_CONEORIGIN;
            }
            if (info->target != NULL)
            {
                cat_salvageDampTarget(info->target);
            }
            return FALSE;

        case CAT_SALVAGE_CONEORIGIN:
            if (cat_dockFlyToConeOrigin(ship, info->target, info->attachPoint))
            {
                anim = cat_salvageOpenAnimIndex(ship->shiptype);
                if ((anim != -1) && (ship->madBindings->nCurrentAnim != anim))
                {
                    // CAT-TODO(stage 3): the exe passes madAnimationStart 0x004b6b90 a third argument, 0 (MeshAnim.c)
                    madAnimationStart(ship, anim);
                    madAnimationPause(ship, TRUE);
                }
                info->state = CAT_SALVAGE_LINK;
            }
            if (info->target != NULL)
            {
                cat_salvageDampTarget(info->target);
            }
            return FALSE;

        case CAT_SALVAGE_LINK:
            if (cat_linkFlyIntoPosition(ship, statics->centerDockPoint, info->target, info->attachPoint))
            {
                if ((info->target->shiptype < bRecon) || (info->target->shiptype > bMissileCorvette) ||
                    ((ship->shiptype >= bRecon) && (ship->shiptype <= bMissileCorvette)) ||
                    cat_shipHasUpgrade(ship, 0x400000))
                {
                    info->state = CAT_SALVAGE_CLAMP;
                }
                else if (cat_shipIsLiveWorker(ship))
                {
                    // a Sect worker latched to a Beast ship infects itself
                    cat_beastInfectShip(ship, info->target);
                    info->state = CAT_SALVAGE_DONE;
                }
                // else the exe prints "Salvager salvaging Beast ship isn't a worker.  Unexpected." and waits
            }
            if (info->target != NULL)
            {
                cat_salvageDampTarget(info->target);
            }
            return FALSE;

        default:
            return TRUE;

        case CAT_SALVAGE_CLAMP:
            clampObjToObj((SpaceObjRotImpTargGuidance *)ship, (SpaceObjRotImpTargGuidance *)info->target);
            if (info->attachPoint->type != DPT_SalvageLatch)
            {
                cat_salvageBeamEffect(ship);
            }
            selectone.ShipPtr[0] = info->target;
            info->noAvoid = FALSE;
            selectone.numShips = 1;
            clHalt(&universe.mainCommandLayer, &selectone);
            info->state = CAT_SALVAGE_FINDDEPOT;
            // deliberately fall through

        case CAT_SALVAGE_FINDDEPOT:
            if (!singlePlayerGame || (ship->playerowner->playerIndex != 1))
            {
                depot = cat_salvageFindNearestDepot(ship, cat_salvageGetSalvagerInfo(ship)->target);
            }
            else
            {
                // the single-player AI tows to its chosen depot when that depot takes the target
                depot = cat_salvageAIDepotShip;
                if ((depot == NULL) || !cat_salvageDepotCanAccept(depot, cat_salvageGetSalvagerInfo(ship)->target))
                {
                    depot = cat_salvageFindNearestDepot(ship, cat_salvageGetSalvagerInfo(ship)->target);
                }
            }
            cat_salvageSetDepotForAll(cat_salvageGetSalvagerInfo(ship)->target, depot);
            if (info->dockwith != NULL)
            {
                // CAT: battle chatter event 0x50 of cataclysm.exe (stage 3 owns the chatter ids)
                if (battleCanChatterAtThisTime(0x50, ship))
                {
                    battleChatterAttempt(-1, 0x50, ship, ship->shiptype);
                }
                info->target->catSalvageDepotCheckTime = universe.totaltimeelapsed;
                info->state = CAT_SALVAGE_WAIT;
                return FALSE;
            }
            cat_salvageNoDepotAbort(ship);
            return FALSE;

        case CAT_SALVAGE_WAIT:
            num = cat_salvageNumAttached(info->target, ship->playerowner);
            if (num < cat_salvageNumPoints(info->target))
            {
                // not enough salvagers yet: the target stays selectable and the player is told every so often
                bitSet(info->target->header.flags, SOF_Selectable);
                bitClear(info->target->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvageTargeted);
                if (info->target->catSalvageNotifyTime == CAT_SALVAGE_NEVER)
                {
                    info->target->catSalvageNotifyTime = universe.realtimeelapsed;
                    return FALSE;
                }
                if (twNotifySalvagersNeededRate + info->target->catSalvageNotifyTime < universe.realtimeelapsed)
                {
                    if (ship->playerowner->playerIndex == universe.curPlayerPtr->playerIndex)
                    {
                        // CAT: Cataclysm's "more salvagers needed" speech event (the exe's literal)
                        speechEventQueue(ship, 0x2000898, -1, -1, -1, -1, -1, 0.0f, -1);
                        // CAT-TODO(stage 3): and the info-overlay message (cat_ioMessageAdd 0x004ac940, string
                        //      0x00bb5038, red)
                    }
                    info->target->catSalvageNotifyTime = universe.realtimeelapsed;
                    return FALSE;
                }
            }
            else
            {
                bitClear(info->target->header.flags, SOF_Selectable);
                info->state = CAT_SALVAGE_TOW;
            }
            return FALSE;

        case CAT_SALVAGE_TOW:
            num = cat_salvageNumAttached(info->target, ship->playerowner);
            if (num < cat_salvageNumPoints(info->target))
            {
                info->state = CAT_SALVAGE_WAIT;
                return FALSE;
            }
            changed = FALSE;
            if (info->target->catSalvageDepotCheckTime + 10.0f < universe.totaltimeelapsed)
            {
                // every 10 s: is another depot now more than 1000 closer?
                depot = cat_salvageFindNearestDepot(ship, info->target);
                // CAT: the exe reads the new depot's position unchecked (NULL + 0x58 when no depot takes the target any
                //      more, an access violation); here no depot means no change
                if ((depot != NULL) && (depot != info->dockwith))
                {
                    real32 olddist = cat_vecDistance(&info->target->obj.posinfo.position, &info->dockwith->obj.posinfo.position);
                    real32 diff = olddist - cat_vecDistance(&info->target->obj.posinfo.position, &depot->obj.posinfo.position);

                    if (diff < 0.0f)
                    {
                        diff = -diff;
                    }
                    if (1000.0f < diff)
                    {
                        Ship *other;

                        cat_salvageFreePoint(info->target, info->dockwith);
                        changed = TRUE;
                        for (other = cat_salvageNextAttachedSalvager(info->target); other != NULL; other = cat_salvageNextAttachedSalvager(NULL))
                        {
                            cat_salvageGetSalvagerInfo(other)->state = CAT_SALVAGE_FINDDEPOT;
                        }
                    }
                }
                info->target->catSalvageDepotCheckTime = universe.totaltimeelapsed;
                if (changed)
                {
                    return FALSE;
                }
            }
            target = cat_salvageGetSalvagerInfo(ship)->target;
            if (!bitTest(target->header.flags, SOF_Disabled))
            {
                // the first frame of the tow: tell the players
                sdword event = -1;

                if (!bitTest(target->obj.attributes, CAT_ATTRIBUTES_DockWithEnemies) &&
                    ((universe.curPlayerPtr == target->playerowner) || allianceIsShipAlly(target, universe.curPlayerPtr)) &&
                    (universe.curPlayerIndex == target->playerowner->playerIndex))
                {
                    event = 0x2001041;      // CAT: "our ship is being salvaged" (the exe's literal)
                }
                else if ((universe.curPlayerIndex == ship->playerowner->playerIndex) &&
                         (target->shiptype != dCrystal0) && (target->shiptype != dCrystal1))
                {
                    event = 0x2001042;      // CAT: "we are salvaging" (the exe's literal)
                }
                if (event != -1)
                {
                    speechEventQueue(NULL, event, -1, -1, 0, universe.curPlayerIndex, -1, 5.0f, -1);
                }
            }
            selectone.ShipPtr[0] = target;
            selectone.numShips = 1;
            selectone.timeLastStatus = 0.0f;
            // CAT: the exe passes RemoveShipsFromDoingStuff 0x00444810 the reason 0x1A
            RemoveShipsFromDoingStuff(&universe.mainCommandLayer, &selectone,0x1A);
            bitClear(target->header.flags, SOF_Selectable);
            bitSet(target->header.flags, SOF_Disabled);
            bitSet(target->header.flags, SOF_DontDrawTrails);
            bitClear(target->shipOrDerelict.specialFlags, SPECIAL_SinglePlayerLimitSpeed);
            shipHasJustBeenDisabled(target, TRUE);         // CAT: the exe passes 1 (0x004fb6f7)
            clRemoveShipFromSelection(&selSelected, target);
            bitSet(info->target->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvageTargeted);
            if (!bitTest(info->target->shipOrDerelict.specialFlags, SPECIAL_SalvageTargetAlreadyDriven))
            {
                switch (info->target->shiptype)
                {
                    case dMessagePod:
                    case dBuoy:
                    case dCrystal0:
                    case dCrystal1:
                        approach = cat_salvageApproachTarget;
                        break;
                    default:
                        approach = cat_salvageFlyToTarget;
                        break;
                }
                if (approach(info->target, info->dockwith))
                {
                    info->state = CAT_SALVAGE_STOP;
                }
                bitSet(info->target->shipOrDerelict.specialFlags, SPECIAL_SalvageTargetAlreadyDriven);
            }
            return FALSE;

        case CAT_SALVAGE_STOP:
            info->target->obj.posinfo.velocity.x = 0.0f;
            info->target->obj.posinfo.velocity.y = 0.0f;
            info->target->obj.posinfo.velocity.z = 0.0f;
            info->target->rotatable.rotinfo.rotspeed.x = 0.0f;
            info->target->rotatable.rotinfo.rotspeed.y = 0.0f;
            info->target->rotatable.rotinfo.rotspeed.z = 0.0f;
            info->state = CAT_SALVAGE_DELIVER;
            return FALSE;

        case CAT_SALVAGE_DELIVER:
            if (!singlePlayerGame && (info->dockwith->command != NULL) &&
                (info->dockwith->command->ordertype.order == COMMAND_MP_HYPERSPACING) &&
                (info->target != NULL) && cat_shipIsCrystal(info->target))
            {
                // a crystal is not handed to a depot that is leaving through hyperspace
                cat_salvageDetach(ship);
                break;
            }
            cat_salvageDeliverToDepot(ship, info->dockwith);
            // CAT: battle chatter event 0x32 of cataclysm.exe (stage 3 owns the chatter ids)
            if (battleCanChatterAtThisTime(0x32, ship))
            {
                battleChatterAttempt(-1, 0x32, ship, ship->shiptype);
            }
            info->state = CAT_SALVAGE_DELIVERED;
            return FALSE;

        case CAT_SALVAGE_DELIVERED:
            return FALSE;

        case CAT_SALVAGE_DONE:
            cat_salvageDetach(ship);
            return TRUE;
    }
    info->state = CAT_SALVAGE_DONE;
    return FALSE;
}

void cat_salvageOrderChanged(Ship *ship)                                    // 0x004fbde0
{
    CatSalvagerInfo *info = cat_salvageGetSalvagerInfo(ship);
    Ship *target = info->target;

    cat_salvageDetach(ship);
    if (target != NULL)
    {
        info->noDamageTargetTime = universe.totaltimeelapsed;
        info->noDamageTarget = target;
    }
}

void cat_salvageHouseKeep(Ship *ship)                                       // 0x004fbeb0
{
    CatSalvagerInfo *info = cat_salvageGetSalvagerInfo(ship);
    CatSalvagerInfo *own;
    CommandToDo *command;
    SelectCommand *targets;
    Ship *target;
    sdword i;
    SelectCommand selectone;

    if (universe.totaltimeelapsed - info->noDamageTargetTime > 5.0f)
    {
        info->noDamageTargetTime = 0.0f;
        info->noDamageTarget = NULL;
    }

    // a latched salvager whose beam went away gets it back
    if ((ship->rceffect == NULL) && (info->target != NULL) && (ship->guidance.clampInfo != NULL) &&
        ((Ship *)ship->guidance.clampInfo->host == info->target) && (info->attachPoint->type == DPT_Salvage))
    {
        // the exe prints a warning first when the beam flag was off
        cat_salvageBeamEffect(ship);
    }

    // CAT: the exe tests (univUpdateCounter & 0x1F) != 0 (0x004fbf72: test 0x1f; je skip), i.e. 31 frames of 32
    command = ship->command;
    if (((universe.univUpdateCounter & 0x1F) != 0) && (command != NULL) &&
        (command->ordertype.order == COMMAND_SPECIAL) && (command->specialtargets != NULL) &&
        (command->selection->ShipPtr[0] == ship))
    {
        own = cat_salvageGetSalvagerInfo(ship);
        if ((own->target != NULL) && cat_salvageTargetDamagedEnough(own->target))
        {
            cat_shipHasJustBeenSalvaged(own->target, ship->playerowner);
        }
        targets = (SelectCommand *)ship->command->specialtargets;
        for (i = 0; (i < targets->numShips) && (i < 10); i++)
        {
            target = targets->ShipPtr[i];
            if ((target->header.objtype == OBJ_ShipType) && cat_salvageCheckTarget(ship->playerowner, target, NULL) &&
                cat_salvageTargetDamagedEnough(target))
            {
                // CAT: the exe passes the salvager's own target here, not the list's (0x004fc05e)
                cat_shipHasJustBeenSalvaged(info->target, ship->playerowner);
            }
            targets = (SelectCommand *)ship->command->specialtargets;
        }
    }

    if (info->forceDetach)
    {
        info->forceDetach = FALSE;
        selectone.numShips = 1;
        selectone.ShipPtr[0] = ship;
        // CAT: the exe passes RemoveShipsFromDoingStuff 0x00444810 the reason 0x515
        RemoveShipsFromDoingStuff(&universe.mainCommandLayer, &selectone,0x515);
    }
}

void cat_salvageDied(Ship *ship)                                            // 0x004fc190
{
    cat_salvageDetach(ship);
}

void cat_salvageRemoveShipReferences(Ship *ship, Ship *shiptoremove)       // 0x004fc1a0
{
    CatSalvagerInfo *info = cat_salvageGetSalvagerInfo(ship);
    Ship *depot;

    if ((singlePlayerGame) && (singlePlayerGameInfo.currentMission == 2) && (shiptoremove->shiptype == dMessagePod))
    {
        // Mission 2's message pod: only a dead pod releases the salvager (the other references are skipped)
        if (shiptoremove != info->target)
        {
            goto checkdepot;
        }
        if (bitTest(shiptoremove->header.flags, SOF_Dead))
        {
            cat_salvageDetach(ship);
        }
    }
    if (shiptoremove == info->target)
    {
        cat_salvageDetach(ship);
    }
checkdepot:
    if (shiptoremove == info->dockwith)
    {
        cat_salvageFreePoint(info->target, info->dockwith);
        depot = cat_salvageFindNearestDepot(ship, cat_salvageGetSalvagerInfo(ship)->target);
        cat_salvageSetDepotForAll(cat_salvageGetSalvagerInfo(ship)->target, depot);
        if (info->dockwith != NULL)
        {
            info->state = CAT_SALVAGE_WAIT;
            return;
        }
        cat_salvageNoDepotAbort(ship);
    }
}

void cat_salvage_PreFix(Ship *ship)                                         // 0x004fc460
{
    CatSalvagerInfo *info = cat_salvageGetSalvagerInfo(ship);

    // CAT: the exe stores the attach point as its index field (DockStaticPoint.dockindex, +0x04), -1 for none
    info->target = (Ship *)(size_t)SpaceObjRegistryGetID((SpaceObj *)info->target);
    info->dockwith = (Ship *)(size_t)SpaceObjRegistryGetID((SpaceObj *)info->dockwith);
    info->noDamageTarget = (Ship *)(size_t)SpaceObjRegistryGetID((SpaceObj *)info->noDamageTarget);
    if (info->attachPoint != NULL)
    {
        info->attachPoint = (DockStaticPoint *)(size_t)info->attachPoint->dockindex;
    }
    else
    {
        info->attachPoint = (DockStaticPoint *)(size_t)-1;
    }
}

void cat_salvage_Fix(Ship *ship)                                            // 0x004fc550
{
    CatSalvagerInfo *info = cat_salvageGetSalvagerInfo(ship);
    sdword index;

    info->target = (Ship *)SpaceObjRegistryGetObj((sdword)(size_t)info->target);
    info->dockwith = (Ship *)SpaceObjRegistryGetObj((sdword)(size_t)info->dockwith);
    info->noDamageTarget = (Ship *)SpaceObjRegistryGetObj((sdword)(size_t)info->noDamageTarget);
    index = (sdword)(size_t)info->attachPoint;
    if (index != -1)
    {
        info->attachPoint = &info->target->staticinfo->dockStaticInfo->dockstaticpoints[index];
    }
    else
    {
        info->attachPoint = NULL;
    }
}

Ship *cat_salvageNextAttachedSalvager(Ship *towed)                          // 0x004fc650
{
    CatClampedList *clamped;
    Ship *ship;

    if (towed != NULL)
    {
        cat_salvageIterTowed = towed;
        cat_salvageIterIndex = 0;
        if ((towed->guidance.clampInfo == NULL) || (towed->guidance.clampInfo->clampedObjs == NULL))
        {
            cat_salvageIterCount = 0;
            return NULL;
        }
        cat_salvageIterCount = towed->guidance.clampInfo->clampedObjs->numShips;
    }
    if (cat_salvageIterCount == 0)
    {
        return NULL;
    }
    clamped = cat_salvageIterTowed->guidance.clampInfo->clampedObjs;
    while (cat_salvageIterIndex < clamped->numShips)
    {
        ship = clamped->ShipPtr[cat_salvageIterIndex++];
        if ((ship->header.objtype == OBJ_ShipType) && cat_shipIsSalvager(ship))
        {
            return ship;
        }
        clamped = cat_salvageIterTowed->guidance.clampInfo->clampedObjs;
    }
    return NULL;
}

Ship *cat_salvageAttachedIterFirst(Ship *target)                            // 0x004fc7a0
{
    return cat_salvageNextAttachedSalvager(target);
}

Ship *cat_salvageAttachedIterNext(void)                                     // 0x004fc7b0
{
    return cat_salvageNextAttachedSalvager(NULL);
}

void cat_salvageSetAIDepotShip(Ship *ship)                                  // 0x004fd2e0
{
    cat_salvageAIDepotShip = ship;
}

Ship *cat_salvageGetAIDepotShip(void)                                       // 0x004fd2f0
{
    return cat_salvageAIDepotShip;
}

void cat_salvageForceDetach(Ship *ship)                                     // 0x004fd580
{
    CatSalvagerInfo *info;

    if ((ship->header.objtype == OBJ_ShipType) && cat_shipIsSalvager(ship))
    {
        info = cat_salvageGetSalvagerInfo(ship);
        if ((info->target != NULL) && (ship->guidance.clampInfo != NULL) &&
            ((Ship *)ship->guidance.clampInfo->host == info->target))
        {
            info->forceDetach = TRUE;
        }
    }
}

/*=============================================================================
    The depot
=============================================================================*/

ShipType cat_salvageConvertType(Ship *salvaged, Ship *depot)                // 0x004fc8c0
{
    ShipType type;

    if (salvaged->shiptype == dBigGunUnit)
    {
        return dBigGunUnit;
    }
    switch (depot->shiprace)
    {
        case RACE_Beast:
            if ((depot->shiptype >= bRecon) && (depot->shiptype <= bMissileCorvette))
            {
                if (salvaged->staticinfo->beastAttacksRequired != -1)
                {
                    return salvaged->staticinfo->beastAttackConvertTo;
                }
                return -1;
            }
            // a Beast depot that is not a Beast type uses Sect's rules (the exe prints which race does)
            // deliberately fall through
        case RACE_Kushan:
        case RACE_Taiidan:
        case RACE_P1:
        case RACE_Traders:
        case RACE_Derelict:
        case RACE_Sect:
            if ((salvaged->shiptype < bRecon) || (salvaged->shiptype > bMissileCorvette))
            {
                return salvaged->shiptype;
            }
            type = cat_salvageUnconvertType(salvaged->shiptype);
            return type;
        default:
            // the exe prints "SALVAGING: Unhandled race."
            return -1;
    }
}

// the delivered ship turns into the depot owner's ship (cat_salvageDepotProcessGeneric 0x004f9500)
static void cat_salvageDepotProcessGeneric(Ship *depot, sdword slot)
{
    CatSalvageDepotSlot *depotslot = &cat_salvageGetDepotInfo(depot)->slots[slot];
    Ship *salvaged = depotslot->ship;
    Ship *converted;
    ShipType type;
    uword playerIndex;
    sdword i;
    ubyte color;

    if (depotslot->state == 0)
    {
        salvaged->catSalvageConverting = TRUE;
        salvaged->catSalvageConvertStart = depotslot->startTime;
        salvaged->catSalvageConvertEnd = depotslot->endTime;
        type = cat_salvageConvertType(salvaged, salvaged->catSalvageDepot);
        salvaged->catSalvageConvertBindings = cat_univMeshBindingsDupeNoGuns(cat_salvageStaticInfo(type));
        playerIndex = depot->playerowner->playerIndex;
        if ((playerIndex < MAX_MULTIPLAYER_PLAYERS) && salvaged->staticinfo->shipOrDerelict.teamColor[playerIndex])
        {
            salvaged->catSalvageConvertColor = (ubyte)depot->playerowner->playerIndex;
            depotslot->state = 1;
            return;
        }
        for (i = 0; i < MAX_MULTIPLAYER_PLAYERS; i++)
        {
            if (salvaged->staticinfo->shipOrDerelict.teamColor[i])
            {
                break;
            }
        }
        if (i == MAX_MULTIPLAYER_PLAYERS)
        {
            i = 0;
        }
        salvaged->catSalvageConvertColor = (ubyte)i;
        depotslot->state = 1;
    }
    else if (depotslot->state == 1)
    {
        if (depotslot->endTime <= universe.totaltimeelapsed)
        {
            color = salvaged->catSalvageConvertColor;
            type = cat_salvageConvertType(salvaged, depot);
            converted = cat_univConvertShip(salvaged, type, depot->playerowner);
            if (converted != NULL)
            {
                converted->colorScheme = (sbyte)color;
                bitSet(converted->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvaged);
                converted->catSalvageDepot = NULL;
                if (depot->playerowner->playerIndex == 0)
                {
                    bitSet(converted->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvagedFlag18);
                }
                else
                {
                    bitClear(converted->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvagedFlag18);
                }
                cat_univShipSalvagedCountInc(converted->shiptype, depot->playerowner->playerIndex);
            }
            memset(depotslot, 0, sizeof(*depotslot));
        }
    }
    // else the exe prints "Unhandled state for generic depot processing."
}

// a Beast depot infects what it is given (cat_salvageDepotProcessBeast 0x004f9760)
static void cat_salvageDepotProcessBeast(Ship *depot, sdword slot)
{
    CatSalvageDepotSlot *depotslot = &cat_salvageGetDepotInfo(depot)->slots[slot];

    if (depotslot->state == 0)
    {
        depotslot->startTime = universe.totaltimeelapsed;
        depotslot->state = 1;
        depotslot->endTime = universe.totaltimeelapsed + 2.0f;
    }
    else if (depotslot->state == 1)
    {
        if (depotslot->endTime < universe.totaltimeelapsed)
        {
            cat_beastInfectShip(depotslot->ship, depot);
            depotslot->state = 2;
        }
    }
    // state 2: done, the slot is freed when the infected ship goes; any other: "Unhandled state ..."
}

// a crystal is dissolved into resource units (cat_salvageDepotProcessCrystal 0x004f98b0) [M2-R4 ships-b]
static void cat_salvageDepotProcessCrystal(Ship *depot, sdword slot)
{
    CatSalvageDepotSlot *depotslot = &cat_salvageGetDepotInfo(depot)->slots[slot];
    Ship *crystal = depotslot->ship;
    CrystalSpec *crystalspec = (CrystalSpec *)crystal->ShipSpecifics;
    DockStaticInfo *dockstaticinfo = depot->staticinfo->dockStaticInfo;
    vector point;
    sdword remaining;
    real32 fraction;

    // the crystal dissolves into the depot's DPT_Crystal_Effect point
    matMultiplyMatByVec(&point, &depot->rotatable.rotinfo.coordsys,
                        &dockstaticinfo->dockstaticpoints[(sbyte)dockstaticinfo->firstOfType[DPT_Crystal_Effect]].position);
    point.x += depot->obj.posinfo.position.x;
    point.y += depot->obj.posinfo.position.y;
    point.z += depot->obj.posinfo.position.z;
    switch (depotslot->state)
    {
        case 0:
            clampObjToObj((SpaceObjRotImpTargGuidance *)crystal, (SpaceObjRotImpTargGuidance *)depot);
            cat_shipDissolveStart(crystal, &point, twCrystalProcessingTime);
            depotslot->state = 1;
            crystalspec->processing = TRUE;
            speechEventQueue(NULL, 0x2001054, -1, -1, 0, depot->playerowner->playerIndex, -1, 5.0f, -1);
            break;

        case 1:
            // the owner gets the dissolved part of the crystal's resource units
            fraction = cat_shipDissolveFraction(crystal);
            if (fraction >= 1.0f)
            {
                depotslot->state = 2;
                return;
            }
            // (x87: fild value * (1.0 - fraction), truncated by __ftol; double is the nearest C precision)
            remaining = (sdword)((1.0 - (double)fraction) *
                                 (double)((CrystalStatics *)crystal->staticinfo->custstatinfo)->resourceValue);
            cat_dockDepositResourceUnits(depot->playerowner, crystalspec->resourceValue - remaining);
            crystalspec->resourceValue = remaining;
            cat_shipDissolveSetDirection(crystal, &point);
            break;

        case 2:
            // CAT-TODO(stage 6): the KAS variable G_CrystalsHarvested is incremented (cat_kasHashString,
            //      cat_kasVarFind: the Cataclysm KAS variables come with the kas2c missions)
            // CAT-TODO(stage 4): so is the depot owner's crystal count in the AI player block (0x00b62d58 + player *
            //      0x7f8)
            crystalspec->noBlast = TRUE;
            crystalspec->processing = FALSE;
            cat_salvageFreePoint(crystal, depot);
            unClampObj((SpaceObjRotImpTargGuidance *)crystal);
            univReallyDeleteThisShipRightNow(crystal);  // CAT-TODO(M2-R4 #1 follow-up): the exe passes a second argument, 1
            depotslot->ship = NULL;
            depotslot->startTime = 0.0f;
            depotslot->endTime = 0.0f;
            depotslot->state = 0;
            depotslot->justDelivered = FALSE;
            speechEventQueue(NULL, 0x2001055, -1, -1, 0, depot->playerowner->playerIndex, -1, 5.0f, -1);
            break;

        default:
            break;
    }
}

// a dBigGunUnit is drawn as converting, then taken apart (cat_salvageDepotProcessBigGun 0x004f9bc0)
static void cat_salvageDepotProcessBigGun(Ship *depot, sdword slot)
{
    CatSalvageDepot *depotinfo = cat_salvageGetDepotInfo(depot);
    CatSalvageDepotSlot *depotslot = &depotinfo->slots[slot];
    Ship *salvaged = depotslot->ship;
    ShipType type;

    if (depotslot->state == 0)
    {
        salvaged->catSalvageConverting = TRUE;
        salvaged->catSalvageConvertStart = depotslot->startTime;
        salvaged->catSalvageConvertEnd = depotslot->endTime;
        type = cat_salvageConvertType(salvaged, salvaged->catSalvageDepot);
        salvaged->catSalvageConvertBindings = cat_univMeshBindingsDupeNoGuns(cat_salvageStaticInfo(type));
        salvaged->catSalvageConvertColor = (ubyte)salvaged->colorScheme;
        depotslot->state = 1;
    }
    else if (depotslot->state == 1)
    {
        if (depotslot->endTime <= universe.totaltimeelapsed)
        {
            depotslot->startTime = universe.totaltimeelapsed;
            depotslot->state = 2;
            depotslot->endTime = universe.totaltimeelapsed + 3.0f;
            depotinfo->bigGunSlot = (sbyte)slot;
        }
    }
    else if (depotslot->state == 2)
    {
        if (depotslot->endTime <= universe.totaltimeelapsed)
        {
            cat_univShipSalvagedCountInc(salvaged->shiptype, depot->playerowner->playerIndex);
            univReallyDeleteThisShipRightNow(salvaged);
            depotinfo->bigGunSlot = -1;
            memset(depotslot, 0, sizeof(*depotslot));
        }
    }
    // else the exe prints "Unhandled state for big gun processing."
}

// a message pod or buoy flies into a dock point of the depot and goes away (cat_salvageDepotProcessDockable
// 0x004f9dc0)
static void cat_salvageDepotProcessDockable(Ship *depot, sdword slot)
{
    CatSalvageDepotSlot *depotslot = &cat_salvageGetDepotInfo(depot)->slots[slot];
    Ship *salvaged = depotslot->ship;
    DockStaticPoint *point = &depot->staticinfo->dockStaticInfo->dockstaticpoints[salvaged->catSalvagePointIndex];

    switch (depotslot->state)
    {
        case 0:
            bitSet(salvaged->header.flags, SOF_CatDocking);
            bitSet(salvaged->shipOrDerelict.specialFlags2, SPECIAL_2_CatNoAvoidance);
            salvaged->shipidle = FALSE;
            salvaged->autostabilizeship = FALSE;
            salvaged->dockvars.catDockPointIndex = salvaged->catSalvagePointIndex;
            depotslot->state = 1;
            return;
        case 1:
            salvaged->shipidle = FALSE;
            salvaged->autostabilizeship = FALSE;
            if (cat_dockFlyToConeInside(salvaged, depot, point))
            {
                depotslot->state = 2;
            }
            return;
        case 2:
            salvaged->shipidle = FALSE;
            salvaged->autostabilizeship = FALSE;
            if (cat_dockFlyToConeOrigin(salvaged, depot, point))
            {
                matMultiplyMatByVec(&salvaged->catDockLatchHeading, &depot->rotatable.rotinfo.coordsys, &point->conenormal);
                depotslot->state = 3;
            }
            return;
        case 3:
            salvaged->shipidle = FALSE;
            salvaged->autostabilizeship = FALSE;
            if (cat_dockFlyToConeOriginLatch(salvaged, depot, 4.0f))
            {
                depotslot->state = 4;
            }
            return;
        case 4:
            cat_salvageFreePoint(salvaged, depot);
            cat_univShipSalvagedCountInc(salvaged->shiptype, depot->playerowner->playerIndex);
            univReallyDeleteThisShipRightNow(salvaged);
            memset(depotslot, 0, sizeof(*depotslot));
            return;
        default:
            return;
    }
}

typedef void (*CatSalvageProcessFn)(Ship *depot, sdword slot);

// which processing a delivered ship gets (the selection inlined in cat_salvageDepotHouseKeep and cat_salvageDepotDied)
static CatSalvageProcessFn cat_salvageDepotProcessFor(Ship *depot, Ship *salvaged)
{
    if (cat_shipIsCrystal(salvaged))
    {
        return cat_salvageDepotProcessCrystal;
    }
    switch (salvaged->shiptype)
    {
        case dMessagePod:
        case dBuoy:
            return cat_salvageDepotProcessDockable;
        case dBigGunUnit:
            return cat_salvageDepotProcessBigGun;
        default:
            return (depot->shiprace == RACE_Beast) ? cat_salvageDepotProcessBeast : cat_salvageDepotProcessGeneric;
    }
}

// the salvagers that towed the slot's ship let go and pick another target or group up (cat_salvageDepotReleaseSalvagers
// 0x004f92a0)
static void cat_salvageDepotReleaseSalvagers(Ship *depot, sdword slot)
{
    Ship *salvaged = cat_salvageGetDepotInfo(depot)->slots[slot].ship;
    CatClampedList *clamped;
    Ship *ship;
    CommandToDo *command;
    sdword i, pointIndex;
    SelectCommand selectone;

    if ((salvaged->guidance.clampInfo == NULL) || (salvaged->guidance.clampInfo->clampedObjs == NULL))
    {
        return;
    }
    for (i = salvaged->guidance.clampInfo->clampedObjs->numShips - 1; i >= 0; i--)
    {
        if ((salvaged->guidance.clampInfo == NULL) || (salvaged->guidance.clampInfo->clampedObjs == NULL))
        {
            break;
        }
        clamped = salvaged->guidance.clampInfo->clampedObjs;
        ship = clamped->ShipPtr[i];
        if ((ship->header.objtype == OBJ_ShipType) && cat_shipIsSalvager(ship))
        {
            command = getShipAndItsCommand(&universe.mainCommandLayer, ship);
            pointIndex = 0;
            if (cat_salvageChooseTarget(ship, (SelectCommand *)command->specialtargets, &pointIndex) == NULL)
            {
                selectone.numShips = 1;
                selectone.timeLastStatus = 0.0f;
                selectone.ShipPtr[0] = ship;
                // CAT: the exe passes RemoveShipsFromDoingStuff 0x00444810 the reason 0x1C
                RemoveShipsFromDoingStuff(&universe.mainCommandLayer, &selectone,0x1C);
                GroupShip(&universe.mainCommandLayer, ship, depot);
            }
            cat_salvageDetach(ship);
        }
    }
}

// the salvager hands its target to a free slot of the depot (cat_salvageDeliverToDepot 0x004fa050)
void cat_salvageDeliverToDepot(Ship *salvager, Ship *depot)
{
    CatSalvagerInfo *info = cat_salvageGetSalvagerInfo(salvager);
    CatSalvageDepot *depotinfo = cat_salvageGetDepotInfo(depot);
    CatSalvageDepotSlot *depotslot;
    real32 time;
    sdword i;
    Ship *ship;

    for (i = 0; i < CAT_SALVAGE_NUM_DEPOT_SLOTS; i++)
    {
        depotslot = &depotinfo->slots[i];
        if (depotslot->ship == NULL)
        {
            time = info->target->staticinfo->common.staticCollInfo.forwardlength / twSalTransSpeed;
            if (twSalTransMaxTime <= time)
            {
                time = twSalTransMaxTime;
            }
            depotslot->ship = info->target;
            depotslot->startTime = universe.totaltimeelapsed;
            depotslot->endTime = universe.totaltimeelapsed + time;
            depotslot->justDelivered = TRUE;
            info->target->catSalvageDepot = depot;
            cat_salvageRemoveFromSalvageOrders(info->target);
            for (ship = cat_salvageNextAttachedSalvager(info->target); ship != NULL; ship = cat_salvageNextAttachedSalvager(NULL))
            {
                cat_shipSetDisableReason(ship, 1, 2);
            }
            return;
        }
    }
}

void cat_salvageDepotInit(Ship *ship)                                       // 0x004fc7c0
{
    CatSalvageDepot *depot = cat_salvageGetDepotInfo(ship);

    bitSet(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvageDepot);
    memset(depot, 0, sizeof(*depot));
    depot->bigGunSlot = -1;
}

void cat_salvageDepotHouseKeep(Ship *ship)                                  // 0x004fc9d0
{
    CatSalvageDepot *depot = cat_salvageGetDepotInfo(ship);
    sdword slot;

    for (slot = 0; slot < CAT_SALVAGE_NUM_DEPOT_SLOTS; slot++)
    {
        if (depot->slots[slot].ship != NULL)
        {
            if (depot->slots[slot].justDelivered)
            {
                cat_salvageDepotReleaseSalvagers(ship, slot);
                depot->slots[slot].justDelivered = FALSE;
            }
            else
            {
                cat_salvageDepotProcessFor(ship, cat_salvageGetDepotInfo(ship)->slots[slot].ship)(ship, slot);
            }
        }
    }
}

void cat_salvageDepotDied(Ship *ship)                                       // 0x004fcc30
{
    CatSalvageDepot *depot = cat_salvageGetDepotInfo(ship);
    CatSalvageDepotSlot *depotslot;
    CatSalvageProcessFn process;
    Ship *salvaged;
    sdword slot;

    for (slot = 0; slot < CAT_SALVAGE_NUM_DEPOT_SLOTS; slot++)
    {
        depotslot = &depot->slots[slot];
        salvaged = depotslot->ship;
        if (salvaged == NULL)
        {
            continue;
        }
        cat_salvageFreePoint(salvaged, ship);
        process = cat_salvageDepotProcessFor(ship, salvaged);
        if (process == cat_salvageDepotProcessCrystal)
        {
            cat_shipDissolveStop(salvaged);         // CAT: 0x00460c70 [M2-R4 ships-b]
        }
        else if ((process == cat_salvageDepotProcessDockable) || (process == cat_salvageDepotProcessBigGun))
        {
            salvaged->dockvars.catDockPointIndex = -1;
        }
        else if (process == cat_salvageDepotProcessGeneric)
        {
            salvaged->catSalvageConverting = FALSE;
            salvaged->catSalvageConvertEnd = universe.totaltimeelapsed - 1.0f;
        }
        else
        {
            // the Beast processing: the ship is no longer disabled, and selectable again
            salvaged->header.flags = (salvaged->header.flags & ~(SOF_Disabled|SOF_DontDrawTrails)) | SOF_Selectable;
            goto clearslot;
        }
        salvaged->deathtime = universe.totaltimeelapsed + 1.0f;
clearslot:
        salvaged->catSalvageDepot = NULL;
        memset(depotslot, 0, sizeof(*depotslot));
    }
}

void cat_salvageDepotRemoveShipReferences(Ship *ship, Ship *shiptoremove)  // 0x004fcf60
{
    CatSalvageDepot *depot = cat_salvageGetDepotInfo(ship);
    sdword slot;

    if ((depot->bigGunSlot != -1) && (depot->slots[depot->bigGunSlot].ship == shiptoremove))
    {
        depot->bigGunSlot = -1;
    }
    for (slot = 0; slot < CAT_SALVAGE_NUM_DEPOT_SLOTS; slot++)
    {
        if (depot->slots[slot].ship == shiptoremove)
        {
            cat_salvageFreePoint(shiptoremove, ship);
            memset(&depot->slots[slot], 0, sizeof(depot->slots[slot]));
        }
    }
}

void cat_salvageDepot_PreFix(Ship *ship)                                    // 0x004fd0e0
{
    CatSalvageDepot *depot = cat_salvageGetDepotInfo(ship);
    sdword slot;

    for (slot = 0; slot < CAT_SALVAGE_NUM_DEPOT_SLOTS; slot++)
    {
        depot->slots[slot].ship = (Ship *)(size_t)SpaceObjRegistryGetID((SpaceObj *)depot->slots[slot].ship);
    }
}

void cat_salvageDepotFix(Ship *ship)                                        // 0x004fd1e0
{
    CatSalvageDepot *depot = cat_salvageGetDepotInfo(ship);
    sdword slot;

    for (slot = 0; slot < CAT_SALVAGE_NUM_DEPOT_SLOTS; slot++)
    {
        depot->slots[slot].ship = (Ship *)SpaceObjRegistryGetObj((sdword)(size_t)depot->slots[slot].ship);
    }
}

bool32 cat_salvageTargetHasBeam(Ship *ship)                                 // 0x004fd300
{
    if (ship->header.objtype != OBJ_ShipType)
    {
        return FALSE;
    }
    if (ship->shiptype != dBigGunUnit)
    {
        return FALSE;
    }
    if (ship->catSalvageDepot == NULL)
    {
        return FALSE;
    }
    return cat_salvageGetDepotInfo(ship->catSalvageDepot)->bigGunSlot != -1;
}

real32 cat_salvageDepotSlotProgress(Ship *ship)                             // 0x004fd450
{
    CatSalvageDepot *depot = cat_salvageGetDepotInfo(ship->catSalvageDepot);
    CatSalvageDepotSlot *depotslot = &depot->slots[depot->bigGunSlot];
    real32 progress;

    progress = (universe.totaltimeelapsed - depotslot->startTime) / (depotslot->endTime - depotslot->startTime);
    if (1.0f <= progress)
    {
        return 1.0f;
    }
    if (progress < 0.0f)
    {
        return 0.0f;
    }
    return progress;
}
