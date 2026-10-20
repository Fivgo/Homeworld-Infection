/*=============================================================================
    Name    : SaveCat32.h
    Purpose : CAT: reading cataclysm.exe's saves (version 0x10ad0027) on an LP64
              build. The exe's saves are raw dumps of its ILP32 structs
              (docs/formats/savegame.md); the loader converts each struct chunk
              field by field into this tree's native struct (SaveCat32.c, the
              generated tables in SaveCat32Layouts.c, tools/m2_save32_gen.py),
              then runs the same load and fix-up code as for a native save.

    Created 2026-10-18 for Homeworld-Infection (M2-R6 oracle-mode-a). This
    tree is REULA-licensed, see src/COPYING.REULA.
=============================================================================*/

#ifndef ___SAVECAT32_H
#define ___SAVECAT32_H

#include "AIPlayer.h"
#include "AITeam.h"
#include "AIVar.h"
#include "B-Spline.h"
#include "Blobs.h"
#include "CameraCommand.h"
#include "Clouds.h"
#include "CommandLayer.h"
#include "ConsMgr.h"
#include "FOW.h"
#include "HS.h"             // M2-R8 singleplayer-tutorial: hsStaticGate
#include "KAS.h"            // M2-R8 singleplayer-tutorial: the KAS label entries
#include "Formation.h"
#include "MeshAnim.h"
#include "Nebulae.h"
#include "Objectives.h"     // M2-R8 singleplayer-tutorial: Objective, FleetIntelligence
#include "Ping.h"           // M2-R8 singleplayer-tutorial: ping
#include "Randy.h"
#include "ResearchAPI.h"
#include "SaveGame.h"
#include "SinglePlayer.h"
#include "SpaceObj.h"
#include "Tactics.h"
#include "TitanInterfaceC.h"
#include "Types.h"
#include "Universe.h"

// LP64 builds convert; an ILP32 build has the exe's layouts already and would copy (not built today)
#if defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64)
#define SAVECAT32_LP64  1
#else
#define SAVECAT32_LP64  0
#endif

/*=============================================================================
    Conversion tables (generated):
=============================================================================*/

typedef enum
{
    SC32_COPY,          // n bytes, same size in both layouts
    SC32_PTR,           // count pointers: 4 bytes sign-extended into 8 (an ID, an index, NULL or a stale pointer)
    SC32_SEXT,          // count signed scalars of s32 bytes into s64 bytes (long)
    SC32_ZEXT,          // count unsigned scalars (Windows' 2-byte wchar_t, unsigned long)
    SC32_ARRAY          // count elements of sub, strides s32 / s64
} SaveCat32OpKind;

struct SaveCat32Layout;

typedef struct SaveCat32Op
{
    udword kind;
    udword off32;
    udword off64;
    udword n;
    udword count;
    udword s32;
    udword s64;
    const struct SaveCat32Layout *sub;
} SaveCat32Op;

typedef struct SaveCat32Layout
{
    const char *name;
    udword size32;      // the exe's sizeof
    udword size64;      // this build's sizeof
    udword nops;
    const SaveCat32Op *ops;
} SaveCat32Layout;

typedef struct SaveCat32SpecLayout
{
    CustShipHeader *header;                 // shipHeaders[] entry
    const SaveCat32Layout *spec;            // its sizeofShipSpecifics struct
} SaveCat32SpecLayout;

#if SAVECAT32_LP64
extern const SaveCat32Layout saveCat32Layout_CaptainGameInfo;
extern const SaveCat32Layout saveCat32Layout_Ship;
extern const SaveCat32Layout saveCat32Layout_ClampInfo;
extern const SaveCat32Layout saveCat32Layout_DamageLightStatic;
extern const SaveCat32Layout saveCat32Layout_Gun;
extern const SaveCat32Layout saveCat32Layout_DockInfo;
extern const SaveCat32Layout saveCat32Layout_ShipsInsideMe;
extern const SaveCat32Layout saveCat32Layout_InsideShip;
extern const SaveCat32Layout saveCat32Layout_ShipSinglePlayerGameInfo;
extern const SaveCat32Layout saveCat32Layout_Asteroid;
extern const SaveCat32Layout saveCat32Layout_Bullet;
extern const SaveCat32Layout saveCat32Layout_Missile;
extern const SaveCat32Layout saveCat32Layout_Derelict;
extern const SaveCat32Layout saveCat32Layout_DustCloud;
extern const SaveCat32Layout saveCat32Layout_Nebula;
extern const SaveCat32Layout saveCat32Layout_blob;
extern const SaveCat32Layout saveCat32Layout_Universe;
extern const SaveCat32Layout saveCat32Layout_CommandToDo;
extern const SaveCat32Layout saveCat32Layout_MilitaryParadeCommand;
extern const SaveCat32Layout saveCat32Layout_MilitarySlot;
extern const SaveCat32Layout saveCat32Layout_CameraCommand;
extern const SaveCat32Layout saveCat32Layout_CameraStackEntry;
extern const SaveCat32Layout saveCat32Layout_RetreatAtom;
extern const SaveCat32Layout saveCat32Layout_ResourceVolume;
extern const SaveCat32Layout saveCat32Layout_MineFormationInfo;
extern const SaveCat32Layout saveCat32Layout_ShipStaticInfo;
extern const SaveCat32Layout saveCat32Layout_AsteroidStaticInfo;
extern const SaveCat32Layout saveCat32Layout_SinglePlayerGameInfo;
extern const SaveCat32Layout saveCat32Layout_madanim;
extern const SaveCat32Layout saveCat32Layout_splinecurve;
extern const SaveCat32Layout saveCat32Layout_PlayerResearchInfo;
extern const SaveCat32Layout saveCat32Layout_shipsinprogress;
extern const SaveCat32Layout saveCat32Layout_shipinprogress;
extern const SaveCat32Layout saveCat32Layout_CatFOWGrid;
extern const SaveCat32Layout saveCat32Layout_ResearchTopic;
extern const SaveCat32Layout saveCat32Layout_AttackAtom;
extern const SaveCat32Layout saveCat32Layout_DustCloudStaticInfo;
extern const SaveCat32Layout saveCat32Layout_cloudSystem;
extern const SaveCat32Layout saveCat32Layout_AIPlayer;                      // M2-R7 ai-player-skirmish
extern const SaveCat32Layout saveCat32Layout_RequestShips;
extern const SaveCat32Layout saveCat32Layout_TeamWaitingForTheseShips;
extern const SaveCat32Layout saveCat32Layout_AITeamMove;
extern const SaveCat32Layout saveCat32Layout_nebChunk;          // M2-R8 kas-runtime-mission01
extern const SaveCat32Layout saveCat32Layout_nebTendril;
extern const SaveCat32Layout saveCat32Layout_KasSelection;      // M2-R8 singleplayer-tutorial: the mission tail
extern const SaveCat32Layout saveCat32Layout_LabelledPath;
extern const SaveCat32Layout saveCat32Layout_LabelledVector;
extern const SaveCat32Layout saveCat32Layout_LabelledVolume;
extern const SaveCat32Layout saveCat32Layout_Objective;
extern const SaveCat32Layout saveCat32Layout_FleetIntelligence;
extern const SaveCat32Layout saveCat32Layout_hsStaticGate;
extern const SaveCat32Layout saveCat32Layout_ping;
extern const SaveCat32SpecLayout saveCat32SpecLayouts[];
#endif

/*=============================================================================
    Data:
=============================================================================*/

extern bool32 saveCat32Loading;     // TRUE while a cataclysm.exe save is read (saveCatLoadHook .. end of LoadGame)

/*=============================================================================
    Functions:
=============================================================================*/

#if SAVECAT32_LP64
void saveCat32Convert(const SaveCat32Layout *layout, const void *src32, void *dst64);
const SaveCat32Layout *saveCat32SpecLayout(CustShipHeader *header);
#endif

// a chunk holding one struct (and nothing else): its contents converted into a new chunk of the native size; the
// old chunk is freed. No-op unless saveCat32Loading.
SaveChunk *saveCat32ChunkStruct(SaveChunk *chunk, const void *layout);

// a chunk holding a head of head32 bytes (converted by headLayout, or copied when it is NULL) followed by count
// elements (converted by elemLayout, or 4-byte pointers sign-extended to 8 when it is NULL); the native head is
// head64 bytes (the offset of the native array)
SaveChunk *saveCat32ChunkArray(SaveChunk *chunk, const void *headLayout, udword head32, udword head64,
                               const void *elemLayout, sdword count);

sdword saveCat32ChunkCount(SaveChunk *chunk, udword offset32);

// a ship's VARIABLE_STRUCTURE chunk: Ship + specifics - 4 (+ PreciseSelection) (+ the mesh-animation block)
SaveChunk *saveCat32ChunkShip(SaveChunk *chunk);

// chunk accounting for the log line at the end of a load
void saveCat32Begin(void);
void saveCat32End(char *filename);

#if SAVECAT32_LP64
#define SAVECAT32_LAYOUT(name)  ((const void *)&saveCat32Layout_##name)
#else
#define SAVECAT32_LAYOUT(name)  ((const void *)NULL)
#endif

#endif
