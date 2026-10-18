// =============================================================================
//  RepairDroid.h
//  - Cataclysm's sRepairDroid / bRepairDroid ship type
//    (cataclysm.exe RepairDroidStaticInit 0x005c29b0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___REPAIRDROID_H
#define ___REPAIRDROID_H

#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b). 8 bytes of specifics in cataclysm.exe (RepairDroidHeader 0x008c6170), none used.
//      The droids that crawl over a Sect ship's hull are not ships of this type but proxies kept in an array at
//      Ship+0x4C0: droidCreateArray 0x005c3070 (univCreateShip, FixShip), droidUpdateArray 0x005c32f0
//      (cat_univShipSelfRepair, with the option enableRepairbots), cat_droidFreeArray 0x005c32b0 (univFreeShipContents,
//      univDeleteDeadShip), cat_droidRenderArray 0x005c3cf0 (the renderer, M2-R5 render), with cat_droidRandomWalkVertex
//      0x005c29e0, cat_droidWalkTowardPoint 0x005c2b80 and droidIsValidPredockPos 0x005c2fd0 (M2-R6 nis-fow-camera).
//      The update draws from random stream 0.
typedef struct
{
    udword unused[2];
} RepairDroidSpec;

extern CustShipHeader RepairDroidHeader;

// CAT (M2-R5 render; fields named by M2-R6 nis-fow-camera from droidUpdateArray 0x005c32f0): a ship's repair droids
//      (droidCreateArray 0x005c3070: a 0x18-byte header, 0x2C per droid, each droid a 0x63C-byte sRepairDroid ship
//      object that is not on any list, all in one block). The droid positions are in the owner's object space.
typedef struct CatRepairDroid
{
    struct Ship *ship;                  // +0x00 the droid's object
    ubyte state;                        // +0x04 CAT_DROID_* (0 inside the ship)
    ubyte pad05[3];
    vector target;                      // +0x08 where it flies (owner object space)
    sdword vertex;                      // +0x14 the hull vertex it sits on (object 0 of LOD 0), -1 none
    sdword polygon;                     // +0x18 the polygon the walk is at, -1 none
    sdword previousVertex;              // +0x1C the vertex before the last walk step home
    real32 stateTime;                   // +0x20 the end of the walk / repair phase
    real32 nextWeldTime;                // +0x24 the next welding effect
    real32 unused28;                    // +0x28 -1.0 at launch, never read
} CatRepairDroid;
typedef struct CatRepairDroidArray
{
    CatRepairDroid *droids;             // +0x00
    sdword numDroids;                   // +0x04
    struct Ship *owner;                 // +0x08
    sdword dockPoint;                   // +0x0C the last launch / return point used (of the dock type 17 points), -1 at creation
    real32 nextLaunchTime;              // +0x10 -1.0 at creation; a droid other than the first waits for it
    real32 lastHealth;                  // +0x14 the owner's health after the last update
} CatRepairDroidArray;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatRepairDroid) == 0x2C, "a repair droid entry is 0x2C bytes in cataclysm.exe");
_Static_assert(sizeof(CatRepairDroidArray) == 0x18, "the repair droid header is 0x18 bytes in cataclysm.exe");
#endif

// the droid states (droidUpdateArray 0x005c32f0; names chosen here)
#define CAT_DROID_Inside        0       // in the ship; launched while the ship is damaged
#define CAT_DROID_Launching     1       // flies out of its dock point to the hull
#define CAT_DROID_Walking       2       // walks the hull vertex to vertex
#define CAT_DROID_Welding       3       // sits on a vertex and welds (effects)
#define CAT_DROID_Moving        4       // one walk step between two welds
#define CAT_DROID_Returning     5       // the ship is repaired (or far): walks back to a dock point
#define CAT_DROID_Docking       6       // flies into the dock point

#define CAT_DROID_DOCKPOINT_TYPE    17  // the dock points of the droids (DockStaticInfo first/numOfType[17], +0x11 / +0x34)

CatRepairDroidArray *droidCreateArray(struct Ship *ship, sdword numDroids);    // 0x005c3070
void cat_droidFreeArray(CatRepairDroidArray *array);                            // 0x005c32b0
void droidUpdateArray(CatRepairDroidArray *array);                              // 0x005c32f0
bool32 droidIsValidPredockPos(DockStaticPoint *point, vector *position);       // 0x005c2fd0
extern udword catDroidUpdates;          // HWI-TEST (M2-R6 nis-fow-camera): droidUpdateArray calls (the render self-test logs it)
void cat_droidRenderArray(CatRepairDroidArray *array);                         // 0x005c3cf0

#endif
