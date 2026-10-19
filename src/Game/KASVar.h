// =============================================================================
//  KASVar.h
//  - Cataclysm's KAS variable store: 12-byte entries {scope hash, name hash,
//    value} in one growable array (cataclysm.exe 0x004918a0-0x00491c70)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___KASVAR_H
#define ___KASVAR_H

#include "Types.h"

// one variable (0xC bytes on ILP32 and LP64)
typedef struct CatKasVar
{
    udword scopeHash;
    udword nameHash;
    sdword value;
} CatKasVar;

_Static_assert(sizeof(CatKasVar) == 0xC, "a KAS variable is 0xC bytes in cataclysm.exe");

udword cat_kasHashString(const char *string);                        // 0x00491860 (M2-R8 kasf library)
void cat_kasVarStoreInit(void);                                      // 0x004918a0
void cat_kasVarDestroyAll(void);                                     // 0x00491ae0 (thunk 0x004918c0)
CatKasVar *cat_kasVarFindOrCreate(udword scopeHash, udword nameHash); // 0x004918d0: the entry (its value at +8) [audit: said the value word; the exe returns the entry, its callers read +8]
void cat_kasVarDestroyEntry(void *entry);                            // 0x004919d0
void cat_kasVarDestroy(udword scopeHash, udword nameHash);           // 0x00491a50
CatKasVar *cat_kasVarFind(udword scopeHash, udword nameHash);        // 0x00491b20: the entry, NULL when absent (gunShootGunsAtTarget 0x0048c32e reads +8)
void cat_kasVarSave(void);                                           // 0x00491b80
void cat_kasVarLoad(void);                                           // 0x00491be0

#endif
