// =============================================================================
//  KASVar.c
//  - Cataclysm's KAS variable store: 12-byte entries {scope hash, name hash,
//    value} in one growable array (cataclysm.exe 0x004918a0-0x00491c70)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R7 ai-player-skirmish). The exe links these functions in the KAS code (after kasfVarValueGet and
//      the other kas2c helpers); the AI player owns the store's life (aiplayerStartup 0x00410fa0 initialises it,
//      aiplayerShutdown 0x00411000 frees it, aiplayerSave / aiplayerLoad 0x00412380 / 0x00412400 save and load it
//      after the AI variables). Its readers (the kas2c missions and the few game sites that test a mission variable
//      such as G_ResourcersRetired, with cat_kasHashString) are stage 6's. Each body follows the r6 decompile and
//      the disassembly of its exe function (address on each).

#include "KASVar.h"

#include "Memory.h"
#include "SaveGame.h"

static CatKasVar *catKasVars = NULL;            // 0x00a43f24
static sdword catKasVarsUsed = 0;               // 0x00a43f28
static sdword catKasVarsAllocated = 0;          // 0x00a43f2c
static sdword catKasVarStamp = 0;               // 0x00882078: set to 0x6aac470 by the init, saved and loaded, read nowhere
                                                //      else (name chosen here)

// 0x004918a0 (the exe does not free an existing array here; aiplayerShutdown's cat_kasVarDestroyAll does)
void cat_kasVarStoreInit(void)
{
    catKasVars = NULL;
    catKasVarsUsed = 0;
    catKasVarsAllocated = 0;
    catKasVarStamp = 0x6aac470;
}

// 0x00491ae0
void cat_kasVarDestroyAll(void)
{
    if (catKasVars != NULL)
    {
        memFree(catKasVars);
        catKasVars = NULL;
        catKasVarsUsed = 0;
        catKasVarsAllocated = 0;
    }
}

// the index of the variable, -1 when absent (the search loop of Find / FindOrCreate / Destroy)
static sdword catKasVarIndex(udword scopeHash, udword nameHash)
{
    sdword i;

    for (i = 0; i < catKasVarsUsed; i++)
    {
        if ((catKasVars[i].scopeHash == scopeHash) && (catKasVars[i].nameHash == nameHash))
        {
            return i;
        }
    }
    return -1;
}

// 0x004918d0: a new variable starts at 0; the array starts at 64 entries and grows by 32; the entry is returned, as the
//      exe (its callers write the value at +8) [audit: returned the value word]
CatKasVar *cat_kasVarFindOrCreate(udword scopeHash, udword nameHash)
{
    sdword i = catKasVarIndex(scopeHash, nameHash);

    if (i >= 0)
    {
        return &catKasVars[i];
    }
    if (catKasVars == NULL)
    {
        catKasVarsUsed = 0;
        catKasVarsAllocated = 0x40;
        catKasVars = memAlloc(0x40 * sizeof(CatKasVar), "KASVars", 0);
    }
    else if (catKasVarsAllocated <= catKasVarsUsed)
    {
        catKasVarsAllocated += 0x20;
        catKasVars = memRealloc(catKasVars, catKasVarsAllocated * sizeof(CatKasVar), "KASVars", 0);
    }
    i = catKasVarsUsed++;
    catKasVars[i].scopeHash = scopeHash;
    catKasVars[i].nameHash = nameHash;
    catKasVars[i].value = 0;
    return &catKasVars[i];
}

// removes entry i, the later ones moving down
static void catKasVarRemoveAt(sdword i)
{
    catKasVarsUsed--;
    for (; i < catKasVarsUsed; i++)
    {
        catKasVars[i] = catKasVars[i + 1];
    }
}

// 0x004919d0: by the entry's address
void cat_kasVarDestroyEntry(void *entry)
{
    sdword i;

    for (i = 0; i < catKasVarsUsed; i++)
    {
        if ((void *)&catKasVars[i] == entry)
        {
            catKasVarRemoveAt(i);
            return;
        }
    }
}

// 0x00491a50
void cat_kasVarDestroy(udword scopeHash, udword nameHash)
{
    sdword i = catKasVarIndex(scopeHash, nameHash);

    if (i >= 0)
    {
        catKasVarRemoveAt(i);
    }
}

// 0x00491b20: the entry, as the exe (gunShootGunsAtTarget 0x0048c32e reads its value at +8) [audit: returned the value word]
CatKasVar *cat_kasVarFind(udword scopeHash, udword nameHash)
{
    sdword i = catKasVarIndex(scopeHash, nameHash);

    return (i >= 0) ? &catKasVars[i] : NULL;
}

// 0x00491b80: the count, the allocation, the stamp, then each entry as a structure of 0xC bytes
void cat_kasVarSave(void)
{
    sdword i;

    SaveInfoNumber(catKasVarsUsed);
    SaveInfoNumber(catKasVarsAllocated);
    SaveInfoNumber(catKasVarStamp);
    for (i = 0; i < catKasVarsUsed; i++)
    {
        SaveStructureOfSize(&catKasVars[i], sizeof(CatKasVar));
    }
}

// 0x00491be0: an allocation of 0 leaves the store empty (and does not read the entries, as the exe)
void cat_kasVarLoad(void)
{
    sdword i;

    catKasVarsUsed = LoadInfoNumber();
    catKasVarsAllocated = LoadInfoNumber();
    catKasVarStamp = LoadInfoNumber();
    if (catKasVarsAllocated == 0)
    {
        // CAT-FIX: the exe sets the pointer to NULL without freeing an array still allocated (a leak)
        if (catKasVars != NULL)
        {
            memFree(catKasVars);
        }
        catKasVars = NULL;
        return;
    }
    if (catKasVars != NULL)
    {
        memFree(catKasVars);
    }
    catKasVars = memAlloc(catKasVarsAllocated * sizeof(CatKasVar), "KASVars", 0);
    for (i = 0; i < catKasVarsUsed; i++)
    {
        LoadStructureOfSizeToAddress(&catKasVars[i], sizeof(CatKasVar));
    }
}
