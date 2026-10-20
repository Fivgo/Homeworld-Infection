/*=============================================================================
    Name    : SinglePlayer.h
    Purpose : Definitions for SinglePlayer.c

    Created 3/3/1998 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___SINGLEPLAYER_H
#define ___SINGLEPLAYER_H

#include "KAS.h"
#include "LinkedList.h"
#include "Types.h"

/*=============================================================================
    Switches:
=============================================================================*/

#ifdef HW_BUILD_FOR_DEBUGGING

#define SP_ERROR_CHECKING           1           //general error checking
#define SP_VERBOSE_LEVEL            3           //control specific output code
#define SP_NISLET_TEST              1           //test NISlets
#define SP_DEBUGKEYS                1           //single player debugging keys
#define SP_DEBUGLEVEL2              1           //more debug keys

#else

#define SP_ERROR_CHECKING           0           //general error checking
#define SP_VERBOSE_LEVEL            0           //control specific output code
#define SP_NISLET_TEST              0
#define SP_DEBUGKEYS                0           //single player debugging keys
#define SP_DEBUGLEVEL2              0           //more debug keys

#endif

/*=============================================================================
    Definitions:
=============================================================================*/
typedef enum
{
    NO_HYPERSPACE,
    HYPERSPACE_PREWAIT,
    HYPERSPACE_WAITINGROLLCALL,
    HYPERSPACE_WAITINGFORFLEETCOMMAND,
    HYPERSPACE_SHIPSLEAVING,
    HYPERSPACE_WHITEOUT,
    HYPERSPACE_LOADDONE,
    HYPERSPACE_SHIPSARRIVING,
    HYPERSPACE_FAILED,                  // CAT (M2-R8 singleplayer-tutorial): in Cataclysm the state while the next
                                        //      mission's opening NIS plays in its staging level (singlePlayerGameUpdate
                                        //      0x005197d0 sets it after the white-out; no case); a failed jump ends in 10
    // CAT (M2-R8 kas-runtime-mission01): two more states (singlePlayerGameUpdate 0x005197d0's switch has cases 9 and
    //      10; Ghidra's HW1 enum shows them as FAILED|PREWAIT and FAILED|WAITINGROLLCALL). A level is loaded in
    //      HYPERSPACE_SHIPSARRIVING and waits there (no case) until its KAS script calls StartLevelByHyperspacingShips
    //      (0x00498230), which sets HYPERSPACE_CATSHIPSHYPERSPACINGIN: the fleet then arrives. Names chosen
    HYPERSPACE_CATSHIPSHYPERSPACINGIN,  // 9
    HYPERSPACE_CATFAILEDRETURN          // 10: the end of a failed hyperspace jump (the exe's case 10)
} HyperspaceState;

#define HS_INACTIVE         0
#define HS_POPUP_INTO       1
#define HS_POPUP_OUTOF      2
#define HS_SLICING_INTO     3
#define HS_SLICING_OUTOF    4
#define HS_COLLAPSE_INTO    5
#define HS_COLLAPSE_OUTOF   6
#define HS_FINISHED         7

#define HSF_WAIT            1   //wait for bit to clear before changing state

#define SP_CAMERA_POINT_NAME        "StartCameraPosition"

#define NUMBER_SINGLEPLAYER_MISSIONS 20     // CAT: MISSION_17 added (M2-R8 kas-runtime-mission01)

/*=============================================================================
    Type definitions:
=============================================================================*/
// for HYPERSPACE_SHIPSLEAVING, here are some substates:
typedef enum
{
    FOCUSING_ON_MOTHERSHIP,
    LOCKED_FOCUS_FOR_HOP
} HyperspaceSubState;

typedef enum
{
    MISSION_ENUM_NOT_INITIALISED = 0,

    // these preserve the original mission numbers 
    MISSION_1_KHARAK_SYSTEM,
    MISSION_2_OUTSKIRTS_OF_KHARAK_SYSTEM,
    MISSION_3_RETURN_TO_KHARAK,
    MISSION_4_GREAT_WASTELANDS_TRADERS,
    MISSION_5_GREAT_WASTELANDS_REVENGE,
    MISSION_6_DIAMOND_SHOALS,
    MISSION_7_THE_GARDENS_OF_KADESH,
    MISSION_8_THE_CATHEDRAL_OF_KADESH,
    MISSION_9_SEA_OF_LOST_SOULS,
    MISSION_10_SUPER_NOVA_STATION,
    MISSION_11_TENHAUSER_GATE,
    MISSION_12_GALACTIC_CORE,
    MISSION_13_THE_KAROS_GRAVEYARD,
    MISSION_14_BRIDGE_OF_SIGHS,
    MISSION_15_CHAPEL_PERILOUS,
    MISSION_16_HIIGARA,
    MISSION_17,             // CAT: Cataclysm's 17th and last mission (singlePlayerGameUpdate 0x005197d0: missions 1 .. 0x11)

    // whilst these are out of sequence
    MISSION_5B_TURANIC_RAIDER_PLANETOID,
    MISSION_TUTORIAL,

} MissionEnum;

typedef struct
{
    MissionEnum currentMission;
    sdword onCompletePlayNIS;
    bool32 onCompleteHyperspace;

    sdword onLoadPlayNIS;

    HyperspaceState hyperspaceState;
    udword oldAutoLaunch;    //bits 0,1,2 = mothership, carrier1, carrier2
    real32 oldClipPlane;
    sdword rollcallShipsDocked;
    sdword rollcallShipsRemaining;
    HyperspaceSubState hyperspaceSubState;
    real32 hyperspaceTimeStamp;
    LinkedList ShipsInHyperspace;   // linked list of ships in hyperspace ether (uses InsideShip structure)
    sdword resourceUnitsCollected;

    GrowSelection ShipsToHyperspace;
    bool32 giveComputerFleetControl;
    bool32 asteroid0sCanMove;

    bool32 playerCanHyperspace;
    bool32 hyperspaceFails;

} SinglePlayerGameInfo;             // later save this data structure

typedef enum
{
    SHIPHYPERSPACE_NONE,
    SHIPHYPERSPACE_WAITING,
    SHIPHYPERSPACE_ACCELERATINGAWAY,
    SHIPHYPERSPACE_WHIPAWAY,
    SHIPHYPERSPACE_GONE,
    SHIPHYPERSPACE_WHIPTOWARDS,
    SHIPHYPERSPACE_DECELERATE,
    SHIPHYPERSPACE_DONE
} ShipHyperspaceState;

typedef struct ShipSinglePlayerGameInfo
{
    bool32 midlevelHyperspaceIn;
    bool32 midlevelHyperspaceOut;
    bool32 staticHyperspaceGate;
    hvector midlevelHyperspaceTo;
    struct Effect* hyperspaceEffect;

    ShipHyperspaceState shipHyperspaceState;
    real32 timeToHyperspace;
    real32 timeToDelayHyperspace;
    real32 forbehind;       // how far ship is forward or behind mothership
    real32 rightleft;       // how far ship is right or left of mothership
    real32 updown;          // how far ship is up or down of mothership

    real32 clipt;                   //[0..1] or -1, hyperspace clip parameter
    real32 cliptDelta;              //clipt incrementer
    udword hsState;                 //hyperspace effect state
    udword hsFlags;
} ShipSinglePlayerGameInfo;

extern SinglePlayerGameInfo singlePlayerGameInfo;
extern bool32 singlePlayerGameLoadNewLevelFlag;
extern bool32 hyperspaceFails;
extern regionhandle spHyperspaceRollCallScreen;     // 0x00a6ada0
extern bool32 spCollectResourcesAtEndOfMission;

void spHyperspaceButtonPushed(void);
// CAT (M2-R6 mainrgn-popup)
extern bool32 catSpBigGunAllowed;                       // 0x00894108
sdword cat_spPlayerCanHyperspace(void);                 // 0x005193b0
char *cat_spMissionFlagValue(void);                     // 0x0051c120

void spHyperspaceSelectionIn(SelectCommand *selection,hvector *destination);
void spHyperspaceSelectionOut(SelectCommand *selection);
void spHyperspaceSelectionInStatic(SelectCommand *selection,hvector *destination);
void spHyperspaceSelectionOutStatic(SelectCommand *selection);

void singlePlayerInit(void);
void singlePlayerPostInit(bool32 loadingSaveGame);
void singlePlayerClose(void);
void singlePlayerSetMissionAttributes(char *directory,char *filename);

void singlePlayerGameCleanup(void);

void singlePlayerGameUpdate(void);
// CAT: singlePlayerLoadNewLevel 0x0051ac80 (nisLevel: load the mission's NIS staging level MissionNN_NISmm; keepMission:
//      do not advance the mission number)
void singlePlayerLoadNewLevel(sdword nisLevel, sdword keepMission);
void singlePlayerStartGame(void);
void singlePlayerNextLevel(void);
bool32 cat_spKasScriptGenerated(MissionEnum mission);  // HWI: the mission's kas2c script exists (part B: 02 .. 17)

void singlePlayerShipDied(Ship *ship);

void singlePlayerMissionCompleteCB(void);
void singlePlayerMissionFailedCB(void);

#if SP_DEBUGKEYS
void singlePlayerCheckDebugKeys(sdword ID);
#endif

void spSetCurrentMission(MissionEnum mission);

MissionEnum spGetCurrentMission (void);

void GetMissionsDirAndFile(MissionEnum mission, bool32 nisLevel);     // CAT: 0x00517200 (+ the staging level)
extern bool32 catHrMissionNisVariant;   // CAT: 0x00a6adac, the level being loaded is a NIS staging level (HorseRace.c)
extern bool32 catSpStartInLoadingScreen;    // CAT: 0x00a6ac68, gameStart's loading screen is up (utility.c)
extern bool32 catSpFleetArriving;           // CAT: 0x00a6ad78, a level's fleet is arriving
extern real32 catSpLevelFade;               // CAT: 0x00a6ac70, the black fade over a newly loaded level
extern real32 catSpWhiteFlash;              // CAT: 0x00a75280, the white flash of Mission01's nislet

void spMainScreen(bool32 closeSensors);   // CAT: 0x00518d80 (closeSensors: every caller but kasfOpenSensors passes 1)

bool32 GetStartPointPlayer(hvector *startpoint);
bool32 GetPointOfName(hvector *point,char *name);

ShipSinglePlayerGameInfo *spNewShipSinglePlayerGameInfo(void);

// flags for spLockout
#define SPLOCKOUT_MOUSE     1
#define SPLOCKOUT_MR        2
#define SPLOCKOUT_DESELECT  4
#define SPLOCKOUT_EVERYTHING    (SPLOCKOUT_MOUSE+SPLOCKOUT_MR+SPLOCKOUT_DESELECT)

void spLockout(udword flags);
void spMainScreenAndLockout(udword flags);
void cat_spMainScreenAndLockoutKeepSensors(udword flags);       // CAT: 0x00518e90 (M2-R8 kasf library)
void cat_spFlagDeleteWhenMissionEnds(struct Ship *ship);         // CAT: 0x0051c0b0 (M2-R8 kasf library)
void spUnlockout(void);

void LoadSinglePlayerGame(void);
void SaveSinglePlayerGame(void);

// CAT: the KAS scripts by cataclysm.exe's index (0 .. 16 = Mission01 .. Mission17, 17 = Tutorial1; -1 none):
//      WatchFunctionToMissionEnum 0x0051b230, MissionEnumToWatchFunction 0x0051b340 (the saves' mission word)
sdword           WatchFunctionToMissionEnum(KASWatchFunction watchFunction);
KASWatchFunction MissionEnumToWatchFunction(sdword missionIndex);
// CAT (M2-R8 kas-runtime-mission01): a script's generated function list (each function with its cataclysm.exe address)
//      and its Watch function's exe address, by the exe's index; NULL when the script is not generated yet (the address
//      is still given for an index 0 .. CAT_SP_KAS_SCRIPTS - 1: the saves' base, kasConvertFuncPtrToOffset)
#define CAT_SP_KAS_SCRIPTS  18
const struct CatKasFunctionTag *cat_spKasScriptFunctions(sdword missionIndex, udword *count, udword *watchExe);

#if SP_NISLET_TEST
void spNISletTestAttempt(sdword index);
#endif

void singlePlayerStartNis(char *nisname, char *scriptname, hvector *positionAndAngle, bool32 loadLevel, bool32 keepMission);
extern sdword singlePlayerNisNumber;        // CAT: 0x00894694
extern sdword singlePlayerNisletNumber;     // CAT: 0x00894698
bool32 spFindCameraAttitude(vector *position);

void UpdateMidLevelHyperspacingShips(void);


//stores the current mission filename
extern char CurrentLevelName[];

/*=============================================================================
    Data:
=============================================================================*/

extern sdword SINGLEPLAYER_STARTINGRUS;
extern char spMissionsDir[];
extern char spMissionsFile[];

extern real32 spFleetModifier;
extern real32 spDifficultyHealthModifier;     // CAT: 0x00894194 (cat_univSetShipMaxHealth 0x0053c4a0)
void cat_spGetDifficultyModifiers(real32 *healthFactor, sdword *buildTimeDivisor, sdword *buildCostDivisor,
                                  sdword difficulty);     // CAT: 0x00516f90
struct tagfeatom;
void cat_spGameDifficulty(char *name, struct tagfeatom *atom);   // CAT: 0x0051c140 ("SP_GameDifficulty")
extern real32 spDifficultyHealthLoadFactor;   // CAT: 0x008941a4 (FixShip 0x00500950)

extern real32 spHyperspaceDelay;
extern bool32   spHoldHyperspaceWindow;

extern real32 HYPERSPACEGATE_HEALTH;
extern real32 HYPERSPACEGATE_WIDTH;
extern real32 HYPERSPACEGATE_HEIGHT;
extern real32 HYPERSPACEGATE_WAYPOINTDIST;

extern real32 SINGLEPLAYER_MISSION14_SPHERE_OVERRIDE;

bool32 cat_spWarpToLevelEnabled(void);      // CAT: 0x0051bd90 (M2-R4 rest-oracle)
void cat_spGetHyperspaceModuleTypes(sdword *count, ShipType *types);     // CAT: 0x0051c0c0 (M2-R6 nis-fow-camera)

#endif
