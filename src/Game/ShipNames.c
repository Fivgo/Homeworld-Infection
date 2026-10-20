// =============================================================================
//  ShipNames.c
//  - Cataclysm's ship names: the player's motherships and carriers get a name
//    from shipnames.script when they are created (cataclysm.exe
//    0x0050ee70-0x0050f900)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R5 hud). The bodies follow the r6 decompile (addresses on each function). The names show in the
//      overlay's tooltip (cat_moTooltipDraw) and the popup menu's selection items. The four save / load record-table
//      functions (cat_saveShipRecordTablePreFix 0x0050f870 .. cat_loadShipRecordTableFix 0x0050f8e0) are at the end
//      (M2-R6 oracle-mode-a: the exe saves the 0x2BC0-byte table with its ship IDs).

#include "ShipNames.h"

#include <stdio.h>
#include <string.h>

#include "Debug.h"
#include "Globals.h"
#include "mainswitches.h"
#include "ObjTypes.h"
#include "StatScript.h"
#include "Universe.h"
#include "SaveGame.h"
#include "SaveCat32.h"
#include "Memory.h"

static CatShipName catShipNames[MAX_SHIP_NAMES];    // 0x00a5b640
static sdword catShipNameCount = 0;                 // 0x00a5e200
static char catShipNameRecording[SHIP_NAME_LENGTH] = "";   // 0x00a5e204: the name shown while a recording plays

static scriptEntry ShipNamesScriptTable[] =         // 0x008935a0
{
    { "ShipName", cat_shipNamesSetCB, NULL },
    END_SCRIPT_ENTRY
};

// the named types: sCarrier, sMothership, bCarrier, bMothership, dKushanCarrier
static bool32 catShipNamesTypeNamed(ShipType type)
{
    switch (type)
    {
        case sCarrier:
        case sMothership:
        case bCarrier:
        case bMothership:
        case dKushanCarrier:
            return TRUE;
        default:
            return FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipNamesSetCB                                0x0050ee70
    Description : "ShipName <type> <name>" adds a name for a named type.
----------------------------------------------------------------------------*/
void cat_shipNamesSetCB(char *directory, char *field, void *dataToFillIn)
{
    char typeName[256], name[256];
    ShipType type;

    (void)directory;
    (void)dataToFillIn;
    typeName[0] = name[0] = '\0';
    sscanf(field, "%255s %255s", typeName, name);
    type = StrToShipType(typeName);
    if (!catShipNamesTypeNamed(type))
    {
        dbgFatalf(DBG_Loc, "\nShip Type not valid for nameing:  %s.  Please remove from '%s'.", typeName,
                  "shipnames.script");
    }
    catShipNames[catShipNameCount].type = type;
    strncpy(catShipNames[catShipNameCount].name, name, SHIP_NAME_LENGTH - 1);
    catShipNameCount++;
    if (catShipNameCount > 99)
    {
        dbgFatalf(DBG_Loc, "\nTo many ship names in '%s'!  Make MAX_SHIP_NAMES bigger (ShipNames.c).", "shipnames.script");
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipNamesInit                                 0x0050efd0
    Description : Clear the table, read shipnames.script, and check that each named type has names (the exe needs
                  one free mothership name per race and three carrier names; its messages say 8 and 16).
----------------------------------------------------------------------------*/
void cat_shipNamesInit(void)
{
    sdword index;

    catShipNameCount = 0;
    for (index = 0; index < MAX_SHIP_NAMES; index++)
    {
        catShipNames[index].ship = NULL;
        catShipNames[index].shipID = 0;
        catShipNames[index].type = 0;
        catShipNames[index].name[0] = '\0';
    }
    scriptSet(NULL, "shipnames.script", ShipNamesScriptTable);
    if (cat_shipNamesCountFree(sMothership) < 1)
    {
        dbgFatalf(DBG_Loc, "\nNot enough names for sMothership in '%s'!  Need %d names, only %d found.", "shipnames.script",
                  8, cat_shipNamesCountFree(sMothership));
    }
    if (cat_shipNamesCountFree(bMothership) < 1)
    {
        dbgFatalf(DBG_Loc, "\nNot enough names for bMothership in '%s'!  Need %d names, only %d found.", "shipnames.script",
                  8, cat_shipNamesCountFree(bMothership));
    }
    if (cat_shipNamesCountFree(sCarrier) < 3)
    {
        dbgFatalf(DBG_Loc, "\nNot enough names for sCarrier in '%s'!  Need %d names, only %d found.", "shipnames.script",
                  0x10, cat_shipNamesCountFree(sCarrier));
    }
    if (cat_shipNamesCountFree(bCarrier) < 3)
    {
        dbgFatalf(DBG_Loc, "\nNot enough names for bCarrier in '%s'!  Need %d names, only %d found.", "shipnames.script",
                  0x10, cat_shipNamesCountFree(bCarrier));
    }
}

// CAT: the owner test of cat_shipNamesShipIsNameable / Assign / Release: the local player's ship (player 0 in a
//      single-player game, sigsPlayerIndex in a multiplayer one)
static bool32 catShipNamesOwnerNamed(Ship *ship)
{
    if (!multiPlayerGame)
    {
        return (ship->playerowner->playerIndex == 0);
    }
    return (ship->playerowner->playerIndex == (uword)sigsPlayerIndex);
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipNamesShipIsNameable                       0x0050f190
----------------------------------------------------------------------------*/
bool32 cat_shipNamesShipIsNameable(Ship *ship)
{
    return (ship != NULL && catShipNamesOwnerNamed(ship) && catShipNamesTypeNamed(ship->shiptype));
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipNamesAssign                               0x0050f320
    Description : Give a new local named ship the first free name of its type (univCreateShip,
                  SelectionSwitchPlayerOwner); a record it held before is freed first. With no free name the exe
                  stores it in the record after the last.
----------------------------------------------------------------------------*/
void cat_shipNamesAssign(Ship *ship)
{
    sdword index, found;

    if (!cat_shipNamesShipIsNameable(ship))
    {
        return;
    }
    found = 0;
    for (index = 0; index < catShipNameCount; index++)
    {
        if (catShipNames[index].type == ship->shiptype && catShipNames[index].ship == NULL)
        {
            break;
        }
        found++;
    }
    for (index = 0; index < catShipNameCount; index++)
    {
        if (catShipNames[index].ship == ship)
        {
            catShipNames[index].ship = NULL;
            break;
        }
    }
    catShipNames[found].ship = ship;
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipNamesCountFree                            0x0050f540
----------------------------------------------------------------------------*/
sdword cat_shipNamesCountFree(ShipType type)
{
    sdword index, count = 0;

    for (index = 0; index < catShipNameCount; index++)
    {
        if (catShipNames[index].type == type && catShipNames[index].ship == NULL)
        {
            count++;
        }
    }
    return count;
}

// CAT: the record of the freeIndex-th free name of a type (the record after the last when there is none)
static sdword catShipNamesFreeRecord(ShipType type, sdword freeIndex)
{
    sdword index, seen = -1;

    for (index = 0; index < catShipNameCount; index++)
    {
        if (catShipNames[index].type == type && catShipNames[index].ship == NULL)
        {
            seen++;
        }
        if (seen == freeIndex)
        {
            break;
        }
    }
    return index;
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipNamesRename                               0x0050f570
    Description : Give a ship the freeIndex-th free name of its type (the popup menu's rename items).
----------------------------------------------------------------------------*/
void cat_shipNamesRename(Ship *ship, sdword freeIndex)
{
    sdword record = catShipNamesFreeRecord(ship->shiptype, freeIndex);
    sdword index;

    for (index = 0; index < catShipNameCount; index++)
    {
        if (catShipNames[index].ship == ship)
        {
            catShipNames[index].ship = NULL;
            break;
        }
    }
    catShipNames[record].ship = ship;
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipNamesGetFreeName                          0x0050f600
----------------------------------------------------------------------------*/
char *cat_shipNamesGetFreeName(ShipType type, sdword freeIndex)
{
    return catShipNames[catShipNamesFreeRecord(type, freeIndex)].name;
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipNamesRelease                              0x0050f660
    Description : A local named ship gives its name back (univRemoveShipReferences, SelectionSwitchPlayerOwner).
----------------------------------------------------------------------------*/
void cat_shipNamesRelease(Ship *ship)
{
    sdword index;

    if (!cat_shipNamesShipIsNameable(ship))
    {
        return;
    }
    for (index = 0; index < catShipNameCount; index++)
    {
        if (catShipNames[index].ship == ship)
        {
            catShipNames[index].ship = NULL;
            return;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipNamesGetName                              0x0050f810
    Description : A ship's name (an empty name while a recorded game plays).
----------------------------------------------------------------------------*/
char *cat_shipNamesGetName(Ship *ship)
{
    sdword index;

    if (playPackets)
    {
        return catShipNameRecording;
    }
    for (index = 0; index < catShipNameCount; index++)
    {
        if (catShipNames[index].ship == ship)
        {
            break;
        }
    }
    return catShipNames[index].name;
}

/*-----------------------------------------------------------------------------
    CAT (M2-R6 oracle-mode-a): the name table in a save. SaveUniverse 0x005034c0 first stores each record's ship as
    its registry ID (cat_saveShipRecordTablePreFix 0x0050f870), then cat_saveShipRecordTable writes the count (INFO)
    and the 100 records as one BASIC chunk of 0x2BC0 bytes; LoadUniverse 0x00503910 reads them back
    (cat_loadShipRecordTable 0x0050f8c0) and LoadGame 0x004fe0e0, after FixAllSpaceObjs, turns the IDs into ships again
    (cat_loadShipRecordTableFix 0x0050f8e0). The exe's record is 0x70 bytes (a 4-byte pointer, then the ID, type and
    name); this tree's is the same with an 8-byte pointer, so a 32-bit table is converted record by record.
-----------------------------------------------------------------------------*/
#define CAT_SHIPNAME_RECORD32   0x70

void cat_saveShipRecordTablePreFix(void)                                    // 0x0050f870
{
    sdword i;

    for (i = 0; i < MAX_SHIP_NAMES; i++)
    {
        catShipNames[i].shipID = SpaceObjRegistryGetID((SpaceObj *)catShipNames[i].ship);
    }
}

void cat_saveShipRecordTable(void)
{
    ubyte *table32;
    sdword i;

    SaveInfoNumber(catShipNameCount);
    if (saveCat32Saving)
    {
        // CAT (M2-R9 recorder-leftovers): the exe's 0x70-byte records: the ship pointer (written as 0: the load takes
        //      the ship from the ID), the ID, the type and the name
        table32 = memAlloc(CAT_SHIPNAME_RECORD32 * MAX_SHIP_NAMES, "shipnames32", Pyrophoric);
        memset(table32, 0, CAT_SHIPNAME_RECORD32 * MAX_SHIP_NAMES);
        for (i = 0; i < MAX_SHIP_NAMES; i++)
        {
            memcpy(table32 + CAT_SHIPNAME_RECORD32 * i + 4, &catShipNames[i].shipID, sizeof(sdword));
            memcpy(table32 + CAT_SHIPNAME_RECORD32 * i + 8, &catShipNames[i].type, sizeof(sdword));
            memcpy(table32 + CAT_SHIPNAME_RECORD32 * i + 0xC, catShipNames[i].name, SHIP_NAME_LENGTH);
        }
        SaveStructureOfSize(table32, CAT_SHIPNAME_RECORD32 * MAX_SHIP_NAMES);
        memFree(table32);
        return;
    }
    SaveStructureOfSize(catShipNames, sizeof(catShipNames));
}

void cat_loadShipRecordTable(void)                                          // 0x0050f8c0
{
    SaveChunk *chunk;
    const ubyte *src;
    sdword i;

    catShipNameCount = LoadInfoNumber();
    if (!saveCat32Loading)
    {
        LoadStructureOfSizeToAddress(catShipNames, sizeof(catShipNames));
        return;
    }
    chunk = LoadNextChunk();
    VerifyChunk(chunk, BASIC_STRUCTURE, CAT_SHIPNAME_RECORD32 * MAX_SHIP_NAMES);
    src = (const ubyte *)chunkContents(chunk);
    for (i = 0; i < MAX_SHIP_NAMES; i++)
    {
        catShipNames[i].ship = NULL;            // the exe's stale pointer; the fix-up below sets it from the ID
        memcpy(&catShipNames[i].shipID, src + CAT_SHIPNAME_RECORD32 * i + 4, sizeof(sdword));
        memcpy(&catShipNames[i].type, src + CAT_SHIPNAME_RECORD32 * i + 8, sizeof(sdword));
        memcpy(catShipNames[i].name, src + CAT_SHIPNAME_RECORD32 * i + 0xC, SHIP_NAME_LENGTH);
    }
    memFree(chunk);
}

void cat_loadShipRecordTableFix(void)                                       // 0x0050f8e0
{
    sdword i;

    for (i = 0; i < MAX_SHIP_NAMES; i++)
    {
        catShipNames[i].ship = (Ship *)SpaceObjRegistryGetObj(catShipNames[i].shipID);
    }
}
