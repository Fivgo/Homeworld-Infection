// =============================================================================
//  ShipNames.h
//  - Cataclysm's ship names: the player's motherships and carriers get a name
//    from shipnames.script when they are created (cataclysm.exe
//    0x0050ee70-0x0050f900)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___SHIPNAMES_H
#define ___SHIPNAMES_H

#include "SpaceObj.h"
#include "Types.h"

// CAT: new file (M2-R5 hud). The exe's own error message names it ("Make MAX_SHIP_NAMES bigger (ShipNames.c)").

#define MAX_SHIP_NAMES          100
#define SHIP_NAME_LENGTH        100

// one name (0x70 bytes in the exe, the table at 0x00a5b640)
typedef struct CatShipName
{
    struct Ship *ship;          // +0x00 the ship that has the name (NULL: free)
    sdword shipID;              // +0x04 the ship's registry ID while saving (cat_saveShipRecordTablePreFix)
    ShipType type;              // +0x08
    char name[SHIP_NAME_LENGTH];// +0x0C
} CatShipName;

#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatShipName) == 0x70, "a ship name record is 0x70 bytes in cataclysm.exe");
#endif

void cat_shipNamesSetCB(char *directory, char *field, void *dataToFillIn);   // 0x0050ee70
void cat_shipNamesInit(void);                                               // 0x0050efd0
bool32 cat_shipNamesShipIsNameable(Ship *ship);                             // 0x0050f190
void cat_shipNamesAssign(Ship *ship);                                       // 0x0050f320
sdword cat_shipNamesCountFree(ShipType type);                               // 0x0050f540
void cat_shipNamesRename(Ship *ship, sdword freeIndex);                     // 0x0050f570
char *cat_shipNamesGetFreeName(ShipType type, sdword freeIndex);            // 0x0050f600
void cat_shipNamesRelease(Ship *ship);                                      // 0x0050f660
char *cat_shipNamesGetName(Ship *ship);                                     // 0x0050f810

#endif
