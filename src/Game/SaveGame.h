/*=============================================================================
    Name    : SaveGame.h
    Purpose : Definitions for SaveGame.c

    Created date by user
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___SAVEGAME_H
#define ___SAVEGAME_H

#include "ShipSelect.h"
#include "SpaceObj.h"
#include "Types.h"

#define SAVE_VERSION_NUMBER_ORIGINAL    0x10ad0027  // save game version for original Homeworld (Relic)

#define SAVE_VERSION_NUMBER_HWSDL_1     0x10ad0028  // => (20031124) bump for Homeworld SDL project
#define SAVE_VERSION_NUMBER_HWSDL_2     0x10ad0029  // => (20061214) currentMission -> MissionEnum

// don't forget to update SaveGame.c: supportedVersionNumbers[] if you change this
#define SAVE_VERSION_NUMBER             SAVE_VERSION_NUMBER_HWSDL_2

#define BASIC_STRUCTURE                 0x80000000
#define VARIABLE_STRUCTURE              0x40000000
#define INFO_CHUNK                      0x20000000
#define LINKEDLISTSPACEOBJS             0x10000000
#define AIPLAYER                        0x08000000

#define SAVE_PING                       0x00001000
#define IDTOPTRTABLE                    0x00000f00
#define SAVE_SALVAGEINFO                0x00000e00  // CAT-DORMANT (M2-R4 combat): HW1's salvage-info chunk; nothing writes or reads it
#define SAVESTRING                      0x00000d00
#define PATH                            0x00000c00
#define AITEAMMOVE                      0x00000b00
#define AITEAM                          0x00000a00
#define SELECTION                       0x00000900
#define SAVE_NEBTENDRIL                 0x00000800
#define SAVE_NEBCHUNK                   0x00000700
#define SAVE_SHIPSINPROGRESS            0x00000600
#define SAVE_ATTACKATOM                 0x00000500
#define SAVE_RETREATATOM                0x00000400
#define SAVE_SLAVEINFO                  0x00000300
#define SAVE_STATICINFO                 0x00000200
#define SAVE_MINEFORMATIONINFO          0x00000100
#define SAVE_SHIPSINSIDEME              0x000000f0
#define SAVE_MILITARYSLOT               0x000000e0
#define SAVE_MILITARYPARADE             0x000000d0
#define SAVE_RESEARCHTOPIC              0x000000c0
#define SAVE_CLOUDSYSTEM                0x000000b0
#define SAVE_COMMANDTODO                0x000000a0
#define SAVE_CAMERACOMMAND              0x00000090
#define SAVE_CAMERASTACKENTRY           0x00000080
#define SAVE_SHIPSINGLEPLAYERGAMEINFO   0x00000070
#define SAVE_DOCKINFO                   0x00000060
#define SAVE_GUNINFO                    0x00000050
#define SAVE_BLOB                       0x00000040
#define SAVE_ATTACKTARGETS              0x00000030
#define SAVE_UNIVERSE                   0x00000020
#define SAVE_SPACEOBJ                   0x00000010


#define SAVE_OBJTYPEMASK                0x0000000f

typedef sdword TypeOfSaveChunk;     // STRUCTURE | SAVE_xxx     e.g. BASIC_STRUCTURE|SAVE_UNIVERSE

typedef struct
{
    TypeOfSaveChunk type;
    sdword contentsSize;
    // chunk contents goes here
} SaveChunk;

typedef struct
{
    sdword info;
} InfoChunkContents;

#define chunkContents(c) ((void *) (((ubyte *)c) + sizeof(SaveChunk)))

/*-----------------------------------------------------------------------------
    CAT: Cataclysm's save container and chunk sizes (docs/formats/savegame.md; M2-R3 "save-deltamap").
    The container is HW1's: a raw udword version, then {sdword type; sdword contentsSize; contents} chunks to EOF
    (SaveThisChunk 0x004fd7c0, LoadNextChunk 0x004fd840). Cataclysm writes 0x10ad0027 (SaveGame 0x004fdaf0) and
    PreLoadGame 0x004fdfc0 accepts nothing else. The sizes below are the exe's chunk contents sizes (ILP32 structs).
    hwi-engine still WRITES its own 64-bit layout under SAVE_VERSION_NUMBER (GoK's 0x10ad0029), so its files are never
    mistaken for GOG saves; a 0x10ad0027 file goes to saveCatLoadHook (SaveGame.c), the place the 32-bit reader
    plugs in once the user's two saves fix the layout.
-----------------------------------------------------------------------------*/
#define SAVE_VERSION_NUMBER_CATACLYSM   SAVE_VERSION_NUMBER_ORIGINAL   // 0x10ad0027, unchanged from HW1 1.04

// new chunk sub-types (BASIC_STRUCTURE | x)
#define SAVE_CAT_PREGAMETABLE           0x00004000  // cat_saveGamePreGameTable 0x004fda00: 3 {ptr,size} entries at 0x00892660
#define SAVE_CAT_MISC9                  0x00009000  // cat_saveGameMiscChunks 0x005ba170: 0x24 bytes from 0x00a762e8
#define SAVE_CAT_MISCA                  0x0000a000  //   0x24 bytes from 0x00a77ab0
#define SAVE_CAT_MISCB                  0x0000b000  //   0x17A0 bytes from 0x00a76310
#define SAVE_CAT_FOWGRID                0x00002000  // cat_saveSavePlayerFOW 0x004ff6b0: the local player's CatFOWGrid (0x20 bytes, ILP32)
#define SAVE_CAT_FOWCELLS               0x00003000  //   then its cells (cat_fowGridSize bytes)

// contents sizes in the exe's saves
#define SAVE_CAT32_STATEBLOCK           0x9C        // BASIC_STRUCTURE (sub-type 0) right after the pre-game block: 39 dwords from 0x00c32c00
#define SAVE_CAT32_PLAYERNAMES          0x90        // 8 x 18
#define SAVE_CAT32_CAPTAINGAMEINFO      0x2B0       // tpGameCreated (HW1 0x268)
#define SAVE_CAT32_PREGAMETABLE         0x0C
#define SAVE_CAT32_MISC9                0x24
#define SAVE_CAT32_MISCA                0x24
#define SAVE_CAT32_MISCB                0x17A0
#define SAVE_CAT32_UNIVERSE             0x3D9BC     // SaveUniverse 0x005034c0, BASIC | SAVE_UNIVERSE (HW1 0x30A0)
#define SAVE_CAT32_PLAYER               0x6530      // Player stride inside the Universe chunk (PreLoadGame loop)
#define SAVE_CAT32_SHIP                 0x63C       // SaveShip 0x004ffd70: + sizeofShipSpecifics - 4 (+0x7C precise selection, + MAD)
#define SAVE_CAT32_BLOB                 0x100       // SaveBlob 0x004fe7d0, BASIC | SAVE_BLOB
#define SAVE_CAT32_SPGAMEINFO           0x54        // SaveSinglePlayerGame 0x0051bee0, SinglePlayerGameInfo (a BASIC chunk of sub-type 0)
#define SAVE_CAT32_SHIPSPGAMEINFO       0x48        // SaveShip 0x004ffd70: BASIC | SAVE_SHIPSINGLEPLAYERGAMEINFO is the ship's
                                                    //   ShipSinglePlayerGameInfo (M2-R6 oracle-mode-a: every ship of the
                                                    //   skirmish recording has one)
#define SAVE_CAT32_SHIPSTATICINFO       0x458       // a dynamic ShipStaticInfo (BASIC | SAVE_STATICINFO)
#define SAVE_CAT32_CLAMPINFO            0x38        // SaveClampInfo (inlined in 0x00501050)
#define SAVE_CAT32_RANSTREAM            0x14        // ranSave 0x004efb50, 27 chunks

_Static_assert(sizeof(SaveChunk) == 8, "chunk header is 8 bytes in cataclysm.exe saves");
_Static_assert(sizeof(InfoChunkContents) == 4, "an INFO chunk holds one sdword in cataclysm.exe saves");
#if defined(__i386__) || defined(_M_IX86)
// the structs whose exe layout this tree already has must match the exe's chunk sizes (tools/m2_layout32.sh)
_Static_assert(sizeof(ShipStaticInfo) == SAVE_CAT32_SHIPSTATICINFO, "ShipStaticInfo chunk is 0x458 in cataclysm.exe");
_Static_assert(sizeof(ClampInfo) == SAVE_CAT32_CLAMPINFO, "ClampInfo chunk is 0x38 in cataclysm.exe");
#endif

#define sizeofSaveChunk(n) (sizeof(SaveChunk) + n)

#define VerifyChunk(c,t,s)              \
    dbgAssertOrIgnore(c);                       \
    dbgAssertOrIgnore((c)->type == (t));        \
    dbgAssertOrIgnore((c)->contentsSize == (s))

#define VerifyChunkNoSize(c,t)          \
    dbgAssertOrIgnore(c);                       \
    dbgAssertOrIgnore((c)->type == (t));

void cat_saveStateBlock(void);                         // CAT: the 0x9C block of SaveGame 0x004fdaf0 (M2-R6 oracle-mode-a)
void cat_loadStateBlock(void);                         // CAT: LoadGame 0x004fe0e0
void cat_saveGamePreGameTable(void);                   // CAT: 0x004fda00
void cat_loadGamePreGameTable(void);                   // CAT: LoadGame 0x004fe0e0
void cat_saveSavePlayerFOW(struct Player *player);     // CAT: 0x004ff6b0 (M2-R6 nis-fow-camera)
void cat_saveLoadPlayerFOW(struct Player *player);     // CAT: 0x004ff7e0
bool32 SaveGame(char *filename);
void LoadGame(char *filename);
void PreLoadGame(char *filename);

void SaveThisChunk(SaveChunk *thischunk);
SaveChunk *CreateChunk(TypeOfSaveChunk type,sdword contentsSize,void *contents);
SaveChunk *LoadNextChunk();

sdword SpaceObjRegistryGetID(SpaceObj *obj);
SpaceObj *SpaceObjRegistryGetObj(sdword id);
Ship *SpaceObjRegistryGetShip(sdword id);
Resource *SpaceObjRegistryGetResource(sdword id);
Bullet *SpaceObjRegistryGetBullet(sdword id);
TargetPtr SpaceObjRegistryGetTarget(sdword id);
void SaveInfoNumber(sdword info);
sdword LoadInfoNumber();
void *ConvertNumToPointerInList(LinkedList *list,sdword num);
sdword ConvertPointerInListToNum(LinkedList *list,void *entry);
void SpaceObjRegistryRegister(SpaceObj *obj);

void SaveSelection(SpaceObjSelection *selection);
SpaceObjSelection *LoadSelection(void);
SpaceObjSelection *LoadSelectionAndFix(void);
void FixSelection(SpaceObjSelection *selection);

void SaveGrowSelection(GrowSelection *grow);
void LoadGrowSelection(GrowSelection *grow);
void LoadGrowSelectionAndFix(GrowSelection *grow);
void FixGrowSelection(GrowSelection *grow);

void SaveMaxSelection(MaxSelection *maxselection);
void LoadMaxSelectionAndFix(MaxSelection *maxselection);

void SaveLinkedListOfInsideShips(LinkedList *list);
void LoadLinkedListOfInsideShips(LinkedList *list);
void FixLinkedListOfInsideShips(LinkedList *list);

#define SavePlayerToPlayerIndex(p) (((p) == NULL) ? -1 : (p)->playerIndex)
#define SavePlayerIndexToPlayer(i) ((((sdword)(i)) == -1) ? NULL : &universe.players[((sdword)(i))])

typedef void (*SaveStuffInLinkedListCB)(void *stuff);
typedef void (*LoadStuffInLinkedListCB)(LinkedList *list);
typedef void (*FixStuffInLinkedListCB)(void *stuff);

void SaveLinkedListOfStuff(LinkedList *list,SaveStuffInLinkedListCB savestuffCB);
void LoadLinkedListOfStuff(LinkedList *list,LoadStuffInLinkedListCB loadstuffCB);
void FixLinkedListOfStuff(LinkedList *list,FixStuffInLinkedListCB fixstuffCB);

void SaveStructureOfSize(void *structure,sdword size);
void *LoadStructureOfSize(sdword size);
void LoadStructureOfSizeToAddress(void *address,sdword size);

void Save_String(char *string);
char *Load_String(void);
void Load_StringToAddress(char *addr);

#define VERIFYSAVEFILE_OK               0
#define VERIFYSAVEFILE_ERROROPENING     -1
#define VERIFYSAVEFILE_BADVERSION       -2

sdword VerifySaveFile(char *filename);
sdword saveCatChunkSize32(TypeOfSaveChunk type);   // CAT: the exe's contents size of a fixed-size chunk type, -1 if it varies
sdword saveCatLoadHook(void);                       // CAT: a 0x10ad0027 (GOG Cataclysm) file; see SaveGame.c

bool32 LoadInfoNumberOptional(sdword *info);
SaveChunk *LoadNextChunkSafe();

#endif
