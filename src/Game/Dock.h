/*=============================================================================
    Name    : Dock.h
    Purpose : Definitions for dock.c

    Created 11/25/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___DOCK_H
#define ___DOCK_H

#include "render.h"
#include "ShipSelect.h"
#include "SpaceObj.h"

/*=============================================================================
    CAT (M2-R5 dock): Cataclysm replaces HW1's DOCK_* bitmask with a dock PURPOSE 0..10. clDock 0x00466c70,
    clWrapDock 0x00452d60, FindNearestShipToDockAt 0x00465750, ThisShipCanDockWith 0x00464290 and
    cat_dockShipDocksAtShip 0x00465860 take it as their DockType; cat_dockGetDockClass 0x00463d70 turns it, with
    the ship's class, type and flags, into the dock point type (DPT_*, ObjTypes.h) the ship uses on the host.
    The numbers are the exe's (the call sites: kasfShipsDockSupportWith 0, kasfShipsDockStay 1, kasfShipsLaunch 2,
    processCollectResource 3, mrRetire 7, aimProcessDock 8/9, kasfSlipgateDock 10); the names are chosen here.
=============================================================================*/
#define CAT_DOCK_SUPPORT                0       // dock for repair/support (HW1's refuel and repair)
#define CAT_DOCK_STAY                   1       // dock and stay inside until launched (SPECIAL_STAY_TILL_EXPLICITLAUNCH)
#define CAT_DOCK_LAUNCH                 2       // launch from the host (clLaunchShip; the hive's swarmers)
#define CAT_DOCK_DEPOSIT_RESOURCES      3       // latch at a resource-transfer point and deposit RUs
#define CAT_DOCK_LEECH                  4       // DPT_Leech latch
#define CAT_DOCK_TOW                    5       // DPT_Tow latch
#define CAT_DOCK_UNUSED6                6       // no dock class (cat_dockGetDockClass returns -1)
#define CAT_DOCK_RETIRE                 7       // retire: dock, refund the build cost, delete
#define CAT_DOCK_SUPPORT_INSTANT        8       // as 0 but the ship is put inside at once (no flight)
#define CAT_DOCK_STAY_INSTANT           9       // as 1 but at once (the single-player hyperspace roll call, hive swarmers)
#define CAT_DOCK_SLIPGATE               10      // transit through a slipgate pair
typedef udword DockType;                        // CAT: now the purpose

// CAT (M2-R5 dock): the dock states of cat_dockShipDocksAtShip 0x00465860 (Ship.dockvars.dockstate, exe +0x49C).
//      Names chosen here from what each state does; the numbers are the exe's.
#define CAT_DOCKSTATE_APPROACH          0       // fly towards the host, pick the nearest free dock point
#define CAT_DOCKSTATE_FLY_IN_FRONT      1       // (no setter found) fly in front of the host, then 2
#define CAT_DOCKSTATE_FLY_TO_CONE       2       // fly into the dock point's cone
#define CAT_DOCKSTATE_LATCH             5       // fly down the cone to its origin; then inside (or slipgate transit)
#define CAT_DOCKSTATE_CONE_ORIGIN       6       // a latch point: fly to the cone origin
#define CAT_DOCKSTATE_LINK_IN           7       // a latch point: fly into position and clamp
#define CAT_DOCKSTATE_INSIDE            8       // inside the host until catDockTime, then launch or stay
#define CAT_DOCKSTATE_LATCHED           9       // clamped at a latch point until catDockTime, then deposit and leave
#define CAT_DOCKSTATE_LAUNCH            10      // find a launch point, put the ship outside
#define CAT_DOCKSTATE_BUILD_ATTACHED    11      // an externally built ship clamped to its maker
#define CAT_DOCKSTATE_FLY_OUT           12      // fly out of the cone: done
#define CAT_DOCKSTATE_SLIPGATE_ENTER    13      // at the gate: into the partner gate
#define CAT_DOCKSTATE_SLIPGATE_EXIT     14      // out of the partner gate
#define CAT_DOCKSTATE_SLIPGATE_WAIT_IN  16      // the 1.5 s transit wait before 13
#define CAT_DOCKSTATE_SLIPGATE_WAIT_OUT 17      // the 1.5 s transit wait before 14

typedef struct
{
    DockType dockType;                      // +0x290 CAT: the purpose
    udword formationToGoIntoAfterDockingID; // +0x294
    bool8 wasHarvesting;                    // +0x298
    bool8 allDockingWithSameShip;           // +0x299
    bool8 allNearShip;                      // +0x29A
    bool8 haveFlyedFarEnoughAway;           // +0x29B
} DockCommand;

#if RND_VISUALIZATION
void dockDrawDockInfo(Ship *ship);
#endif

sdword dockFindDockIndex(char *name,DockStaticInfo *dockstaticinfo);
DockStaticPoint *cat_dockFindDockStaticPoint(char *name,DockStaticInfo *dockstaticinfo);   // CAT: 0x00463d00
real32 ClampToWithin(real32 number,real32 min,real32 max);
void GetDirectionVectorOfShip(vector *result,udword direction,matrix *coordsys);   // CAT: takes the frame (0x004631f0)
// CAT (M2-R4 combat): Cataclysm-only dock helpers (Dock.c; the salvage system and the link code use them)
void cat_dockDepositResourceUnits(struct Player *player,sdword resourceUnits);             // CAT: 0x00463270
sdword cat_salvageOpenAnimIndex(ShipType type);                                            // CAT: 0x004638f0
sdword cat_salvageCloseAnimIndex(ShipType type);                                           // CAT: 0x004639b0
bool32 cat_matCoordSysAligned(matrix *a,matrix *b,real32 cosAngle);                        // CAT: 0x00463840
void cat_linkComputeTargetCoordSys(Ship *ship,DockStaticPoint *linkpoint,Ship *host,DockStaticPoint *hostpoint,matrix *out);   // CAT: 0x00463680
bool32 cat_dockFlyToConeOriginLatch(Ship *ship,Ship *dockwith,real32 radius);              // CAT: 0x00464b30
bool32 cat_dockFlyToConeInside(Ship *ship,Ship *dockwith,DockStaticPoint *dockpoint);      // CAT: 0x004671d0
bool32 cat_dockFlyToConeOrigin(Ship *ship,Ship *dockwith,DockStaticPoint *dockpoint);      // CAT: 0x004673c0
bool32 cat_linkFlyIntoPosition(Ship *ship,DockStaticPoint *linkpoint,Ship *host,DockStaticPoint *hostpoint);   // CAT: 0x004675c0

// CAT (M2-R5 dock): the Cataclysm dock system
sdword cat_dockGetDockClass(ShipClass shipclass,ShipType shiptype,bool32 externalBuild,udword typeFlags,DockType purpose,Ship *dockwith);   // 0x00463d70
bool32 cat_dockCanDockWithBusyHost(Ship *ship,DockType purpose);                           // 0x00463a70
bool32 cat_dockHasFreeSlotForShip(Ship *host,ShipClass shipclass,ShipType shiptype,bool32 externalBuild,udword typeFlags);   // 0x00464910
void cat_dockInitShipDockInfo(Ship *ship);                                                 // 0x00463bb0
bool32 cat_dockFindNearestDockPoint(Ship *ship,Ship *dockwith,DockType purpose);           // 0x00464660
bool32 cat_dockFindLaunchPoint(Ship *ship,Ship *dockwith);                                 // 0x004649b0
bool32 cat_dockFlyOutOfCone(Ship *ship,Ship *dockwith);                                    // 0x004650d0
bool32 cat_dockFlyInFrontOfHost(Ship *ship,Ship *dockwith);                                // 0x00465630
udword cat_dockShipDocksAtShip(Ship *ship,Ship *dockwith,DockType purpose);                // 0x00465860
void cat_dockFinishedDocking(Ship *ship,Ship *dockwith,struct CommandToDo *command);       // 0x004667f0
bool32 cat_dockShipIsInsideHost(Ship *ship,Ship *host);                                    // 0x00468f80
void dockReserveDockPoint(Ship *ship,Ship *dockwith,sdword dockpointindex);                // 0x00464610 (REWRITTEN)

bool32 ThisShipCanDockWith(Ship *ship,Ship *dockwith,DockType purpose,bool32 checkCounts); // 0x00464290: the exe's 4th argument
Ship *FindNearestShipToDockAt(Ship *ship,DockType purpose);                                // 0x00465750
void InitializeDockVars(Ship *ship,Ship *dockship,DockType purpose);                       // 0x00463c40
void dockChangeSingleShipToDock(struct CommandToDo *command,Ship *ship,Ship *dockship,bool32 wasHarvesting,DockType purpose);   // 0x004644c0
bool32 processDockToDo(struct CommandToDo *docktodo);                                      // 0x00466950
void RemoveShipFromDocking(Ship *ship);                                                    // 0x00465360 (REWRITTEN)
void RemoveShipFromLaunching(Ship *ship);                                                  // 0x00465590 (REWRITTEN)
void DockCleanup(struct CommandToDo *docktodo);                                            // 0x00467970
void LaunchCleanup(struct CommandToDo *launchtodo);                                        // 0x004679b0

void dockPutShipInside(Ship *ship,Ship *dockwith);                                         // 0x004652c0
void dockRemoveShipFromInside(Ship *ship,Ship *dockwith);                                  // 0x004632e0
void dockPutShipOutside(Ship *ship,Ship *creator,vector *createat,udword headingdirection,udword updirection);   // 0x00463350

bool32 ShipIsRefuelingAtCarrierMother(struct Ship *ship);                                  // 0x00467a90
bool32 ShipIsWaitingForSoftLaunch(struct Ship *ship);                                      // 0x00467a60

// CAT (M2-R5 dock): the scripted dock self-test (/catDockTest <ticks>; harness mode dock), not in the exe
void catDockSelfTestSet(char *string);
bool32 catDockSelfTestRequested(void);
void catDockSelfTestTick(void);

#endif
