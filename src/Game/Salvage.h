// =============================================================================
//  Salvage.h
//  - Cataclysm's salvage system: salvagers (Worker, SalCap corvettes,
//    p1StandardCorvette) that tow targets to salvage depots (carriers,
//    motherships, processors, bHeavyCruiser) (cataclysm.exe 0x004f7a90-0x004fd600)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___SALVAGE_H
#define ___SALVAGE_H

#include <stddef.h>

#include "Attack.h"
#include "ShipSelect.h"
#include "SpaceObj.h"
#include "Types.h"

// CAT: new file (M2-R3 ships-a; the runtime M2-R4 combat). The exe's salvage system replaces HW1's SalCapCorvette.c
//      salvage code: a salvager flies to a target's DPT_Salvage/DPT_SalvageLatch dock point, clamps itself to it,
//      waits until enough salvagers of its player hold the target, tows it to the nearest depot that accepts it and
//      hands it over; the depot then converts, infects, dissolves or takes apart what it was given (one slot per
//      delivery). Every function of 0x004f7a90..0x004fd600 is here; the addresses are on each body in Salvage.c.

// The salvager block, 0x28 bytes on ILP32, inside the salvager's specifics (cat_salvageGetSalvagerInfo 0x004f8ec0):
// sWorker/bWorker +0x0C (Ship+0x644), kSalCapCorvette/tSalCapCorvette +0x08 (Ship+0x640), p1StandardCorvette +0x13C
// (Ship+0x774). The names of +0x0C..+0x18 are HW1's SalCapCorvetteSpec fields with the same job; the others are
// chosen here from their users.
typedef struct CatSalvagerInfo
{
    sdword state;                   // +0x00 CAT_SALVAGE_* (cat_salvageSpecialTarget 0x004faad0)
    struct Ship *target;            // +0x04 the ship being salvaged
    struct Ship *dockwith;          // +0x08 the depot it is towed to
    bool32 tractorBeam;             // +0x0C the beam effect is on (cat_salvageBeamEffect 0x004f7ab0)
    bool32 noAvoid;                 // +0x10 inside the target's cone: no collision with it (states 3..7)
    real32 noDamageTargetTime;      // +0x14 when noDamageTarget was set (cat_salvageDetach, cat_salvageOrderChanged)
    struct Ship *noDamageTarget;    // +0x18 the last target: no bump damage with it for 5 s (cat_salvageHouseKeep)
    udword unused1C;                // +0x1C cleared by nothing, read by nothing
    DockStaticPoint *attachPoint;   // +0x20 the target's salvage point it holds (cat_salvageGetTargetAttachPoint)
    bool8 forceDetach;              // +0x24 cat_salvageForceDetach 0x004fd580 sets it, cat_salvageHouseKeep acts on it
    ubyte pad25[3];
} CatSalvagerInfo;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(CatSalvagerInfo, attachPoint) == 0x20, "the attach point is at +0x20 in cataclysm.exe");
_Static_assert(offsetof(CatSalvagerInfo, forceDetach) == 0x24, "the force-detach byte is at +0x24 in cataclysm.exe");
_Static_assert(sizeof(CatSalvagerInfo) == 0x28, "the salvager block is 0x28 bytes in cataclysm.exe");
#endif

// cat_salvageSpecialTarget's states (the switch at 0x004faad0; 6 is never used)
#define CAT_SALVAGE_CHOOSE          0   // pick a target from the order's targets
#define CAT_SALVAGE_FLYTOTARGET     1   // fly to it; claim it for this salvager's player
#define CAT_SALVAGE_GETPOINT        2   // reserve a salvage point on it
#define CAT_SALVAGE_CONEINSIDE      3   // fly into the point's cone
#define CAT_SALVAGE_CONEORIGIN      4   // fly down the cone; open the claws (Worker animation 3)
#define CAT_SALVAGE_LINK            5   // line up with the point (cat_linkFlyIntoPosition)
#define CAT_SALVAGE_CLAMP           7   // clamp to the target, beam on, halt the target
#define CAT_SALVAGE_FINDDEPOT       8   // every salvager on the target picks the nearest depot
#define CAT_SALVAGE_WAIT            9   // wait for enough salvagers of this player
#define CAT_SALVAGE_TOW             10  // disable the target and tow it to the depot
#define CAT_SALVAGE_STOP            11  // stop the target
#define CAT_SALVAGE_DELIVER         12  // hand it over to the depot
#define CAT_SALVAGE_DELIVERED       13  // wait for the depot to release this salvager
#define CAT_SALVAGE_DONE            14  // detach; the order is done

// One delivery slot of a depot, 0x14 bytes (cat_salvageDepotHouseKeep 0x004fc9d0 walks 16 of them)
typedef struct CatSalvageDepotSlot
{
    struct Ship *ship;              // +0x00 the delivered ship
    real32 startTime;               // +0x04
    real32 endTime;                 // +0x08
    sdword state;                   // +0x0C per processing function
    bool32 justDelivered;           // +0x10 set by cat_salvageDeliverToDepot: release the salvagers next housekeep
} CatSalvageDepotSlot;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatSalvageDepotSlot) == 0x14, "a depot slot is 0x14 bytes in cataclysm.exe");
#endif

#define CAT_SALVAGE_NUM_DEPOT_SLOTS 16

// The salvage depot block, 0x144 bytes on ILP32, inside the depot's specifics (cat_salvageDepotInit 0x004fc7c0):
// carriers +0x1C (Ship+0x654), processors +0x00 (Ship+0x638), motherships +0xAC (Ship+0x6E4), bHeavyCruiser +0x18
// (Ship+0x650). cat_salvageDepotInit clears 0x51 dwords and sets the byte at +0x140 to -1.
typedef struct CatSalvageDepot
{
    CatSalvageDepotSlot slots[CAT_SALVAGE_NUM_DEPOT_SLOTS];     // +0x000
    sbyte bigGunSlot;               // +0x140 the slot whose big gun is being taken apart (cat_salvageTargetHasBeam), -1
    ubyte pad[3];
} CatSalvageDepot;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(CatSalvageDepot, bigGunSlot) == 0x140, "the big-gun slot byte is at +0x140 in cataclysm.exe");
_Static_assert(sizeof(CatSalvageDepot) == 0x144, "the salvage depot block is 0x144 bytes in cataclysm.exe");
#endif

// The salvager statics, 8 bytes (cat_salvageStaticInit 0x004f90e0): the "salvageFlyToDistance" key and the ship's
// "center" dock point (found by name; NULL if the ship has none)
typedef struct CatSalvageStatics
{
    real32 salvageFlyToDistance;
    DockStaticPoint *centerDockPoint;
} CatSalvageStatics;

// The statics of sWorker/bWorker and the SalCap corvettes (0x18 bytes on ILP32; SalCapCorvetteStaticInit 0x005c59c0
// clears 0x14 of them and reads AttackSideStepParametersScriptTable into the start): the side-step parameters, then
// the salvage statics at +0x0C (cat_salvageStaticInit 0x004f90e0)
typedef struct CatSalvagerStatics
{
    AttackSideStepParameters sidestepParameters;    // +0x00
    CatSalvageStatics salvage;                      // +0x0C
} CatSalvagerStatics;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(CatSalvagerStatics, salvage) == 0x0C, "the salvage statics are at +0x0C in cataclysm.exe");
_Static_assert(sizeof(CatSalvagerStatics) == 0x14, "0x14 bytes used of the 0x18-byte blocks in cataclysm.exe");
#endif

// cat_salvageCheckTarget's reason counters (0x004f7eb0: nine words, filled for the salvage cursor/status text)
typedef enum
{
    CAT_SALVAGE_REASON_NoPoints,        // [0] no salvage points, a KAS derelict, or not a ship
    CAT_SALVAGE_REASON_NoSupport,       // [1] no support units for it, or Beast rules forbid it
    CAT_SALVAGE_REASON_NoSupportType,   // [2] the player cannot build a ship of its class
    CAT_SALVAGE_REASON_Friendly,        // [3] own or allied ship
    CAT_SALVAGE_REASON_NoCrystalUpgrade,// [4] a crystal without the 0x100000 upgrade ship
    CAT_SALVAGE_REASON_NoBeastUpgrade,  // [5] Beast player without the 0x10000 upgrade ship
    CAT_SALVAGE_REASON_Asteroid,        // [6] an asteroid
    CAT_SALVAGE_REASON_Dust,            // [7] a dust cloud
    CAT_SALVAGE_REASON_Forbidden,       // [8] being built, a salvager with a target, a depot type, ...
    CAT_SALVAGE_NUM_REASONS
} CatSalvageReason;

CatSalvagerInfo *cat_salvageGetSalvagerInfo(Ship *ship);
CatSalvageDepot *cat_salvageGetDepotInfo(Ship *ship);
bool32 cat_shipIsSalvager(Ship *ship);
Ship *cat_salvageGetTarget(Ship *ship);

void cat_salvageResetGlobals(void);
void cat_salvageBeamEffect(Ship *salvager);
bool32 cat_salvageIsBeingSalvaged(Ship *ship);
bool32 cat_salvageDepotCanAccept(Ship *depot, Ship *target);
bool32 cat_salvageTargetDamagedEnough(Ship *target);
bool32 cat_playerHasSalvageUpgradeShip(struct Player *player);
bool32 cat_salvageCheckTarget(struct Player *player, Ship *target, sword *reasons);
bool32 cat_salvageCanSalvage(struct Player *player, Ship *target);
bool32 cat_salvageCheckTargetReasons(struct Player *player, Ship *target, sword *reasons);
Ship *cat_salvageChooseTarget(Ship *salvager, SelectCommand *targets, sdword *pointIndex);
Ship *cat_salvageFindNearestDepot(Ship *salvager, Ship *target);
bool32 cat_salvageGetTargetAttachPoint(Ship *salvager, Ship *target);
ShipType cat_salvageConvertType(Ship *salvaged, Ship *depot);
Ship *cat_salvageNextAttachedSalvager(Ship *towed);
Ship *cat_salvageAttachedIterFirst(Ship *target);
Ship *cat_salvageAttachedIterNext(void);
void cat_salvageSetAIDepotShip(Ship *ship);
Ship *cat_salvageGetAIDepotShip(void);
bool32 cat_salvageTargetHasBeam(Ship *ship);
real32 cat_salvageDepotSlotProgress(Ship *ship);
void cat_salvageForceDetach(Ship *ship);

void cat_salvageStaticInit(char *directory, char *filename, ShipStaticInfo *statinfo);
void cat_salvageInit(Ship *ship);
void cat_salvageDetach(Ship *salvager);
bool32 cat_salvageSpecialTarget(Ship *ship, void *custom);
void cat_salvageHouseKeep(Ship *ship);
void cat_salvageOrderChanged(Ship *ship);
void cat_salvageRemoveShipReferences(Ship *ship, Ship *shiptoremove);
void cat_salvageDied(Ship *ship);
void cat_salvage_PreFix(Ship *ship);
void cat_salvage_Fix(Ship *ship);

void cat_salvageDeliverToDepot(Ship *salvager, Ship *depot);
void cat_salvageDepotInit(Ship *ship);
void cat_salvageDepotHouseKeep(Ship *ship);
void cat_salvageDepotDied(Ship *ship);
void cat_salvageDepotRemoveShipReferences(Ship *ship, Ship *shiptoremove);
void cat_salvageDepot_PreFix(Ship *ship);
void cat_salvageDepotFix(Ship *ship);

#endif
