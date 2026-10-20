#include "SinglePlayer.h"
#include "FOW.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "AIPlayer.h"
#include "AIResourceMan.h"
#include "AIShip.h"
#include "AITrack.h"
#include "AIVar.h"
#include "Alliance.h"
#include "Animatic.h"
#include "Attributes.h"
//#include "bink.h"
#include "Collision.h"
#include "CommandDefs.h"
#include "CommandWrap.h"
#include "Damage.h"
#include "FEFlow.h"
#include "FastMath.h"
#include "File.h"
#include "font.h"
#include "GravWellGenerator.h"
#include "HorseRace.h"
#include "HS.h"
#include "InfoOverlay.h"
#include "KASFunc.h"
#include "KASVar.h"
#include "OracleDump.h"         // HWI-ORACLE: oracleMissionDirectStart (singlePlayerStartGame)
#include "MissionSelfTest.h"    // HWI-TEST (M2-R8 kas-runtime-mission01): the mission self-test's flow lines
#include "LaunchMgr.h"
#include "LevelLoad.h"
#include "LinkedList.h"
#include "SaveCat32.h"
#include "Mimic.h"         // CAT: cat_spSaveMiscInfo / cat_spLoadMiscInfo (M2-R6 oracle-mode-a)
#include "mainrgn.h"
#include "Memory.h"
#include "Mothership.h"
#include "mouse.h"
#include "NIS.h"
#include "Objectives.h"
#include "ObjTypes.h"
#include "Physics.h"
#include "Ping.h"
#include "Randy.h"
#include "ResearchGUI.h"
#include "SaveGame.h"
#include "Select.h"
#include "Sensors.h"
#include "SoundEvent.h"
#include "SoundEventDefs.h"
#include "StatScript.h"
#include "StringSupport.h"
#include "TaskBar.h"
#include "TradeMgr.h"
#include "Tutor.h"
#include "Tweak.h"
#include "Universe.h"
#include "Blast.h"
#include "UnivUpdate.h"
#include "utility.h"

// CAT (M2-R8 kas-runtime-mission01): Cataclysm's kas2c missions, generated from cataclysm.exe (src/Missions,
//      tools/m2_kas2c_gen.py): HW1's compiled scripts (src/Missions/*.kas) are not built
#include "CatMissions.h"
#include "KASVar.h"
#include "texreg.h"           // CAT: cat_trPrecacheShipTextures (the Mission01 nislet)
#include "Mimic.h"

#ifdef _MSC_VER
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif


#define ShipCanMakeHyperspace(ship) (ShipCanHyperspace(ship) && ((ship)->shipOrDerelict.specialFlags & SPECIAL_SinglePlayerWithinHyperSpaceRange))
#define WARP_FILENAME "Warp.script"

//stores the current mission filename
char CurrentLevelName[256] = "";

udword singlePlayerCurrentMissionIndex = 0;

MissionEnum missionSequence[] = {
#if defined(HW_GAME_HOMEWORLD)
    MISSION_1_KHARAK_SYSTEM,                // *REQUIRED* - creates initial fleet
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
    MISSION_17,                             // CAT: Cataclysm has 17 missions (singlePlayerGameUpdate 0x005197d0's mission switch)
#elif defined(HW_GAME_RAIDER_RETREAT)
    MISSION_1_KHARAK_SYSTEM,
    MISSION_2_OUTSKIRTS_OF_KHARAK_SYSTEM,
    MISSION_3_RETURN_TO_KHARAK,
    MISSION_4_GREAT_WASTELANDS_TRADERS,
    MISSION_5B_TURANIC_RAIDER_PLANETOID,
#elif defined(HW_GAME_DEMO)
    MISSION_1_KHARAK_SYSTEM,
    MISSION_2_OUTSKIRTS_OF_KHARAK_SYSTEM,
#endif
};

#define NUMBER_MISSIONS_IN_SEQUENCE   (sizeof(missionSequence) / sizeof(MissionEnum))

// you'd think you could change that 19 to <= 16 but the NIS stuff overloads this
// in an array declaration that includes end markers and so is larger than 16
#define NUMBER_SINGLEPLAYER_MISSIONS  20     // CAT: MISSION_17 added (SinglePlayer.h)

#define NUMBER_SINGLEPLAYER_NIS       12

// state of kas debug overlays
#if SP_DEBUGLEVEL2
extern sdword mrKASDebugDrawStates;
extern sdword mrKASDebugDrawTimers;
extern sdword mrKASDebugDrawVars;
extern sdword mrKASDebugDrawVolumes;
extern sdword mrKASSkipFactor;
#endif

extern fonthandle selGroupFont0;

bool32 hyperspaceAtEndOfSinglePlayerNIS = FALSE;
bool32 hyperspaceOverride               = FALSE;
bool32 singlePlayerGameLoadNewLevelFlag = FALSE;
bool32 singlePlayerHyperspacingInto     = FALSE;
bool32 spHoldHyperspaceWindow           = FALSE;
bool32 spBinkPlay                       = FALSE;
bool32 triggerNIS                       = FALSE;
bool32 spCollectResourcesAtEndOfMission = FALSE;

real32 spFleetModifier = 0.0f;

// CAT: the single-player difficulty's health factor for the Somtaaw ships (0x00894194, 1.0 in the exe image; set by
//      cat_spGetDifficultyModifiers 0x00516f90 from the difficulty 0..4 as 6, 3, 1, 0.85, 0.7 and read by
//      cat_univSetShipMaxHealth 0x0053c4a0). The difficulty screen (cat_spGameDifficulty 0x0051c140) and its save are
//      not ported yet (stage 6 single player), so the factor stays 1.0 (difficulty 2, the exe's default 0x008941a0)
real32 spDifficultyHealthModifier = 1.0f;
// CAT (M2-R6 oracle-mode-a): the factor FixShip 0x00500950 multiplies a loaded single-player Somtaaw ship's health by
//      (0x008941a4, 1.0 in the exe image; kasLoad 0x00491450 sets it to the difficulty factor over the saved one).
//      CAT-TODO(stage 6 single player): kasLoad's assignment needs cat_spGetDifficultyModifiers 0x00516f90 (KAS.c kasLoad reads the saved difficulty since M2-R8 kas-runtime-mission01)
real32 spDifficultyHealthLoadFactor = 1.0f;
real32 spFleetStr      = 0.0f;

real32 BaseFleetStrengthForLevel    [NUMBER_SINGLEPLAYER_MISSIONS+1];
real32 AdvancedFleetStrengthForLevel[NUMBER_SINGLEPLAYER_MISSIONS+1];

sdword SINGLEPLAYER_DEBUGLEVEL  =   0;
sdword SINGLEPLAYER_STARTINGRUS = 200;
sdword warpLevel                =   0;

// CAT: 0x0051bd90: a single-player game with the warp-to-level script active (the exe keeps a bool the warp script's
//      scriptSetBool writes at 0x00a67490; here the level WarpToLevelEnabled() found). Callers: universeClosePlayers/
//      universeClose (the upgrades of player 0 survive a warp) and gameStart.
bool32 cat_spWarpToLevelEnabled(void)
{
    return (singlePlayerGame && (warpLevel != 0));
}


fibfileheader *spHyperspaceRollCallHandle = NULL;
regionhandle   spHyperspaceRollCallScreen = NULL;

char spMissionsDir [32] = "";
char spMissionsFile[32] = "";

SinglePlayerGameInfo singlePlayerGameInfo;

bool32 HYPERSPACE_MOTHERSHIP_ALWAYS_ARRIVES_FIRST = TRUE;

real32 HS_CLIPT_NEG_FINISH                      =      0.9f;
real32 HS_CLIPT_NEG_SCALAR                      =      0.33f;
real32 HS_CLIPT_NEG_THRESH                      =     -0.4f;
real32 HS_CLIPT_POS_SCALAR                      =      0.5f;
real32 HS_CLIPT_POS_START                       =      1.0f;
real32 HS_CLIPT_POS_THRESH                      =      0.5f;
real32 HYPERSPACEGATE_HEALTH                    =  20000.0f;
real32 HYPERSPACEGATE_HEIGHT                    =   1000.0f;
real32 HYPERSPACEGATE_WAYPOINTDIST              =    800.0f;
real32 HYPERSPACEGATE_WIDTH                     =   1000.0f;
real32 HYPERSPACE_ACCELERATE_TIME               =      3.0f;
real32 HYPERSPACE_ACCELERATION                  =   1000.0f;
real32 HYPERSPACE_BASE_ARRIVETIME               =      2.0f;
real32 HYPERSPACE_BASE_MOTHERSHIPARRIVETIME     =      1.0f;
real32 HYPERSPACE_BASE_WAITTIME                 =      3.0f;
real32 HYPERSPACE_DECELERATE_DIST               =      0.0f;
real32 HYPERSPACE_DECELERATE_INITIAL_VEL        =      0.0f; 
real32 HYPERSPACE_DECELERATE_TIME               =      3.0f;
real32 HYPERSPACE_DECELERATION                  =  10000.0f;
real32 HYPERSPACE_FARCLIPPLANE                  = 100000.0f;
real32 HYPERSPACE_FARCLIPPLANESQR               =      0.0f;
real32 HYPERSPACE_FUNNEL_WIDTH                  =   5000.0f;
real32 HYPERSPACE_FUNNEL_WIDTH_TO_BACK_FACTOR   =      0.5f;
real32 HYPERSPACE_HEADINGACCURACY               =      0.98f;
real32 HYPERSPACE_MAXWAITTIME_DISTANCE          =      1.0f;
real32 HYPERSPACE_MINWAITTIME_DISTANCE          =     -1.0f;
real32 HYPERSPACE_RANDOM_WAITTIME               =      0.2f;
real32 HYPERSPACE_RANGE                         = (3000.0f * 3000.0f);
real32 HYPERSPACE_SLICE_RATE                    =      0.075f;
real32 HYPERSPACE_TRAVELDIST_PER_RU_SPENT       =  10000.0f;
real32 HYPERSPACE_WAITTIME_PER_DISTANCE         =      0.0005f;
real32 HYPERSPACE_WAIT_FOR_MOTHERSHIPFOCUS_TIME =      5.0f;
real32 HYPERSPACE_WHIPTOWARDSDIST               =      0.0f;
real32 HYPERSPACE_WHIP_VELOCITY                 =  50000.0f;
real32 HYPERSPACE_WHITEOUTTIME                  =      0.2f;
real32 SINGLEPLAYER_BOBBIGGESTRADIUS_LEVEL6     =  40000.0f;
real32 SINGLEPLAYER_MISSION14_SPHERE_OVERRIDE   =  50000.0f;
real32 SUBMESSAGE_SAFETY_TIMEOUT                =     15.0f;
real32 spHyperspaceDelay                        =      0.0f;


static sdword WarpToLevelEnabled(void);
static udword convertMissionToSequenceIndex(MissionEnum mission);

static MissionEnum spGetAdjacentMission(sbyte direction);

static void singlePlayerKasMissionStart(MissionEnum missionnum);

/*-----------------------------------------------------------------------------
    CAT (M2-R8 kas-runtime-mission01): the mission flow's tables (cataclysm.exe .data), by the mission number 1 .. 17
-----------------------------------------------------------------------------*/
#ifdef _WIN32
    #define NIS_PATH "nis\\"
#else
    #define NIS_PATH "nis/"
#endif
// 0x008944bc: the mission is entered from its opening NIS, played in its staging level SinglePlayer\MissionNN_NISmm
//      (the 11 MissionNN_NISmm directories of the data; Mission05_NIS03 is not entered this way)
static const bool32 catSpMissionStartsInNis[19] = { 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0 };
// 0x00894508: the NIS of the mission (the staging level's mm, and the NIS played once the fleet has arrived in a
//      mission not entered from it), -1 none
static const sdword catSpMissionNis[19] = { 0, 1, -1, -1, 2, -1, 4, 5, -1, -1, 6, -1, 7, -1, 8, 9, 10, 11, 0 };
// 0x00894470: the fleet is put at the start point at once when the level has loaded (no arrival flight)
static const bool32 catSpMissionArrivesAtOnce[19] = { 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0 };
// 0x00894550: the NIS files by NIS number (1 .. 11)
static char *const catSpNisNames[12][2] =
{
    { NULL,                NULL                   },
    { NIS_PATH"n01.nis",   NIS_PATH"n01.script"   },
    { NIS_PATH"n02.nis",   NIS_PATH"n02.script"   },
    { NIS_PATH"n03.nis",   NIS_PATH"n03.script"   },
    { NIS_PATH"n04.nis",   NIS_PATH"n04.script"   },
    { NIS_PATH"n05.nis",   NIS_PATH"n05.script"   },
    { NIS_PATH"n06.nis",   NIS_PATH"n06.script"   },
    { NIS_PATH"n07.nis",   NIS_PATH"n07.script"   },
    { NIS_PATH"n08.nis",   NIS_PATH"n08.script"   },
    { NIS_PATH"n09.nis",   NIS_PATH"n09.script"   },
    { NIS_PATH"n10.nis",   NIS_PATH"n10.script"   },
    { NIS_PATH"n11.nis",   NIS_PATH"n11.script"   },
};
// 0x008945a0 + 4 * (nislet + mission * 3): the nislets a KAS script plays (G_PlayNislet); the exe's table overlaps the
//      end of the NIS table (its indices below 4 are never used)
#define CAT_SP_NISLETS_PER_MISSION  3
static char *catSpNisletName(sdword mission, sdword nislet)
{
    switch (nislet + mission * CAT_SP_NISLETS_PER_MISSION)
    {
        case 4:  return NIS_PATH"nislet01";        // 0x008945b0: Mission01, nislet 1
        case 28: return NIS_PATH"nislet03";        // 0x00894610: Mission09, nislet 1
        case 31: return NIS_PATH"nislet02";        // 0x0089461c: Mission10, nislet 1
        default: return NULL;
    }
}

// CAT: 0x00a6ac70 / 0x00a6ac6c / 0x00a6adc0: the black fade over the main view after a level has loaded (1.0 down to
//      0.0 at 0.5 a second of game time), its restart flag and its last time; 0x00a75280: the white flash of
//      Mission01's nislet. Names chosen
// CAT-TODO(stage 6 single player): mrRegionDraw 0x00569260 draws catSpLevelFade (black, unless 0x00a4cabc) and catSpWhiteFlash (white) over the main view; mainrgn.c does not yet
real32 catSpLevelFade = 0.0f;
static bool32 catSpLevelFadeRestart = FALSE;
static real32 catSpLevelFadeLastTime = 0.0f;
real32 catSpWhiteFlash = 0.0f;
// CAT: 0x00a6ac74: the level being loaded was asked for by the end of its opening NIS (the mission number stays);
//      0x00a6ad78: a level's fleet is arriving (hsStaticGateRender draws no static gate meanwhile); 0x00a6adbc /
//      0x00a6adb8: the fleet is to be put at the start point at once, and when. Names chosen
static bool32 catSpLevelFromNisStop = FALSE;
bool32 catSpFleetArriving = FALSE;
static bool32 catSpArriveAtOnce = FALSE;
static real32 catSpArriveAtOnceTime = 0.0f;
// CAT: 0x00a6adac: the level being loaded is a NIS staging level (GetMissionsDirAndFile, singlePlayerLoadNewLevel; the
//      loading picture, HorseRace.c hrChooseSinglePlayerBitmap)
bool32 catHrMissionNisVariant = FALSE;
// CAT: 0x00a6ac68: gameStart's loading screen is up while it starts the single-player game (gameStart 0x005a9290 sets
//      it around singlePlayerStartGame), so the level load of the opening NIS puts up no other. Name chosen
bool32 catSpStartInLoadingScreen = FALSE;
extern char *singlePlayerNISName;                       // 0x00a6ada4 (below)
extern bool8 etgHasBeenStarted;                         // utility.c (0x00ab7668)

static void spAbortHyperspaceCB(char *string, featom *atom);
static void spGoNowHyperspaceCB(char *string, featom *atom);

static void spShipsDockedDrawCB   (featom *atom, regionhandle region);
static void spShipsRemainingDrawCB(featom *atom, regionhandle region);

static void SetBaseFleetStrCB       (char *directory, char *field, void *dataToFillIn);
static void SetOnMissionCompleteInfo(char *directory, char *field, void *dataToFillIn);
static void SetWarpFleetCB          (char *directory, char *field, void *dataToFillIn);
static void WarpFleetStrengthCB     (char *directory, char *field, void *dataToFillIn);
static void WarpFleetRUStrengthCB   (char *directory, char *field, void *dataToFillIn);
static void WarpPreLoadCB           (char *directory, char *field, void *dataToFillIn);

static void FindShipsOfShipTypeOfPlayer(GrowSelection *growselect,ShipType shiptype,Player *player);

scriptEntry SinglePlayerTweaks[] =
{
    makeEntry( HS_CLIPT_NEG_FINISH,                         scriptSetReal32CB    ),
    makeEntry( HS_CLIPT_NEG_SCALAR,                         scriptSetReal32CB    ),
    makeEntry( HS_CLIPT_NEG_THRESH,                         scriptSetReal32CB    ),
    makeEntry( HS_CLIPT_POS_SCALAR,                         scriptSetReal32CB    ),
    makeEntry( HS_CLIPT_POS_START,                          scriptSetReal32CB    ),
    makeEntry( HS_CLIPT_POS_THRESH,                         scriptSetReal32CB    ),
    makeEntry( HYPERSPACEGATE_HEALTH,                       scriptSetReal32CB    ),
    makeEntry( HYPERSPACEGATE_HEIGHT,                       scriptSetReal32CB    ),
    makeEntry( HYPERSPACEGATE_WAYPOINTDIST,                 scriptSetReal32CB    ),
    makeEntry( HYPERSPACEGATE_WIDTH ,                       scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_ACCELERATE_TIME,                  scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_ACCELERATION,                     scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_BASE_ARRIVETIME,                  scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_BASE_MOTHERSHIPARRIVETIME,        scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_BASE_WAITTIME,                    scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_DECELERATE_TIME,                  scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_DECELERATION,                     scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_FARCLIPPLANE,                     scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_HEADINGACCURACY,                  scriptSetCosAngCB    ),
    makeEntry( HYPERSPACE_MAXWAITTIME_DISTANCE,             scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_MINWAITTIME_DISTANCE,             scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_MOTHERSHIP_ALWAYS_ARRIVES_FIRST,  scriptSetBool        ),
    makeEntry( HYPERSPACE_RANDOM_WAITTIME,                  scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_RANGE,                            scriptSetReal32SqrCB ),
    makeEntry( HYPERSPACE_SLICE_RATE,                       scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_TRAVELDIST_PER_RU_SPENT,          scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_WAITTIME_PER_DISTANCE,            scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_WAIT_FOR_MOTHERSHIPFOCUS_TIME,    scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_WHIP_VELOCITY,                    scriptSetReal32CB    ),
    makeEntry( HYPERSPACE_WHITEOUTTIME,                     scriptSetReal32CB    ),
    makeEntry( SINGLEPLAYER_BOBBIGGESTRADIUS_LEVEL6,        scriptSetReal32CB    ),
    makeEntry( SINGLEPLAYER_DEBUGLEVEL,                     scriptSetSdwordCB    ),
    makeEntry( SINGLEPLAYER_MISSION14_SPHERE_OVERRIDE,      scriptSetReal32CB    ),
    makeEntry( SINGLEPLAYER_STARTINGRUS,                    scriptSetSdwordCB    ),
    makeEntry( SUBMESSAGE_SAFETY_TIMEOUT,                   scriptSetReal32CB    ),

    { "BaseFleetStrengthForLevel", SetBaseFleetStrCB, NULL },
    
    END_SCRIPT_ENTRY
};

scriptEntry WarpScriptTable[] =
{
    { "WarpStartRU", scriptSetSdwordCB, &universe.players[0].resourceUnits },
    { "WarpFleet",   SetWarpFleetCB,    NULL                               },
    
    END_SCRIPT_ENTRY
};

scriptEntry WarpCalcFleetStrengthTable[] =
{
    { "WarpStartRU", WarpFleetRUStrengthCB, NULL },
    { "WarpFleet",   WarpFleetStrengthCB,   NULL },
    
    END_SCRIPT_ENTRY
};

scriptEntry WarpPreLoadTable[] =
{
    { "WarpFleet", WarpPreLoadCB, NULL },
    
    END_SCRIPT_ENTRY
};

scriptEntry SinglePlayerScriptTable[] =
{
    { "onMissionComplete",     SetOnMissionCompleteInfo, &singlePlayerGameInfo                   },
    { "onMissionLoad",         scriptSetSdwordCB,        &singlePlayerGameInfo.onLoadPlayNIS     },
    { "MissionStartEnemyRUs",  scriptSetSdwordCB,        &universe.players[1].resourceUnits      },
    { "MissionStartEnemyRace", scriptSetShipRaceCB,      &universe.players[1].race               },
    { "Asteroid0sCanMove",     scriptSetBool,            &singlePlayerGameInfo.asteroid0sCanMove },
    
    END_SCRIPT_ENTRY
};

fecallback spHyperspaceCallback[] =
{
    { spAbortHyperspaceCB, "AbortHyperspace" },
    { spGoNowHyperspaceCB, "GoNowHyperspace" },
    
    { NULL,                NULL              },
};

fedrawcallback spHyperspaceDrawCallback[] =
{
    { spShipsRemainingDrawCB, "ShipsRemainingDraw" },
    { spShipsDockedDrawCB,    "ShipsDockedDraw"    },
    
    { NULL,                   NULL                 },
};


udword convertMissionToSequenceIndex(MissionEnum mission)
{
    udword index = 0;
    
    for (index = 0; index < NUMBER_MISSIONS_IN_SEQUENCE; index++)
    {
        if (mission == missionSequence[index])
        {
            return index;
        }
    }
    
    // getting here is seriously bad and I don't see how to recover from it
    // other than to lie about the index which will probably crash things anyway
    // so let's die at the point of failure not somewhere else down the line
    dbgFatalf(DBG_Loc, "unknown MissionEnum enum: %u", mission);
    
    return 0;  // this will never be called but keeps the compiler quiet
}

void spResetMissionSequenceToBeginning(void)
{
    singlePlayerCurrentMissionIndex = 0;
    
    spSetCurrentMission(missionSequence[singlePlayerCurrentMissionIndex]);
}

void spSetCurrentMission(MissionEnum mission)
{
    singlePlayerGameInfo.currentMission = mission;
    
    singlePlayerCurrentMissionIndex
        = convertMissionToSequenceIndex(mission);
}

MissionEnum spGetPreviousMission(void)
{
    return spGetAdjacentMission(-1);
}

MissionEnum spGetCurrentMission(void)
{
    return singlePlayerGameInfo.currentMission;
}

MissionEnum spGetNextMission(void)
{
    return spGetAdjacentMission(+1);
}

MissionEnum spGetAdjacentMission(sbyte direction)
{
    sdword adjacentMissionIndex
        = convertMissionToSequenceIndex(spGetCurrentMission())
        + (direction < 0 ? -1 : +1);
    
    if (adjacentMissionIndex < 0
    ||  adjacentMissionIndex >= NUMBER_MISSIONS_IN_SEQUENCE)
    {
        return MISSION_ENUM_NOT_INITIALISED;
    }
    
    return missionSequence[adjacentMissionIndex];
}



/*-----------------------------------------------------------------------------
    Name        : GetMissionsDirAndFile                             0x00517200
    Description : CAT: the mission's level, or with nisLevel its NIS staging level SinglePlayer\MissionNN_NISmm\
                  MissionNN_NISmm.mission (mm = catSpMissionNis[mission]); records which one (catHrMissionNisVariant)
----------------------------------------------------------------------------*/
void GetMissionsDirAndFile(MissionEnum mission, bool32 nisLevel)
{
    if (!nisLevel)
    {
#ifdef _WIN32
        sprintf(spMissionsDir, "SinglePlayer\\Mission%02d\\", mission);
#else
        sprintf(spMissionsDir, "SinglePlayer/Mission%02d/", mission);
#endif
        sprintf(spMissionsFile,"Mission%02d.mission", mission);
    }
    else
    {
#ifdef _WIN32
        sprintf(spMissionsDir, "SinglePlayer\\Mission%02d_NIS%02d\\", mission, catSpMissionNis[mission]);
#else
        sprintf(spMissionsDir, "SinglePlayer/Mission%02d_NIS%02d/", mission, catSpMissionNis[mission]);
#endif
        sprintf(spMissionsFile,"Mission%02d_NIS%02d.mission", mission, catSpMissionNis[mission]);
    }
    catHrMissionNisVariant = nisLevel;
}

bool32 GetPointOfName(hvector *point,char *name)
{
    hvector *vectptr = kasVectorPtr(cat_kasHashString(name));       // CAT: by hash (GetPointOfName 0x00517280)

    if (vectptr != NULL)
    {
        *point = *vectptr;
        return TRUE;
    }
    else
    {
        point->x = 0.0f;   point->y = 0.0f;   point->z = 0.0f;   point->w = 0.0f;
        return FALSE;
    }
}

bool32 GetStartPointPlayer(hvector *startpoint)
{
    return GetPointOfName(startpoint,"StartPointPlayer");
}

void SetBaseFleetStrCB(char *directory,char *field,void *dataToFillIn)
{
    sdword level;
    real32 basestrength;
    real32 advancedstrength;

    RemoveCommasFromString(field);

    sscanf(field,"%d %f %f",&level,&basestrength,&advancedstrength);

    dbgAssertOrIgnore(level <= NUMBER_SINGLEPLAYER_MISSIONS);

    BaseFleetStrengthForLevel[level] = basestrength;
    AdvancedFleetStrengthForLevel[level] = advancedstrength;
}

real32 GetStrengthOfShip(Ship *ship)
{
    // for now, just return RU cost of ship:
    if (ship->shiptype == Mothership)
    {
        return 0.0f;
    }
    return (real32)ship->staticinfo->buildCost;
}

real32 GetStrengthOfShipStatic(ShipStaticInfo *shipstatic)
{
    // for now, just return RU cost of ship:
    if (shipstatic->shiptype == Mothership)
    {
        return 0.0f;
    }
    return (real32)shipstatic->buildCost;
}

real32 GetStrengthOfRUs(sdword RUs)
{
    return (real32)RUs;
}

/*-----------------------------------------------------------------------------
    Name        : GetFleetStrengthHyperspace
    Description : Returns fleet strength of ships in hyperspace of player
    Inputs      :
    Outputs     :
    Return      : Returns fleet strength of ships in hyperspace of player
----------------------------------------------------------------------------*/
void GetFleetStrengthHyperspace(Player *player)
{
    // find total strength of fleet in hyperspace
    Node *node = singlePlayerGameInfo.ShipsInHyperspace.head;
    InsideShip *insideShip;
    Ship *ship;

    spFleetStr = 0.0f;
    while (node != NULL)
    {
        insideShip = (InsideShip *)listGetStructOfNode(node);
        ship = insideShip->ship;
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (ship->playerowner == player)
        {
            spFleetStr += GetStrengthOfShip(ship);
            // also check ships inside it:

            if ((ship->shipsInsideMe != NULL))
            {
                // check ships inside too
                Node *insidenode = ship->shipsInsideMe->insideList.head;
                InsideShip *insideship2;

                while (insidenode != NULL)
                {
                    insideship2 = (InsideShip *)listGetStructOfNode(insidenode);
                    dbgAssertOrIgnore(insideship2->ship->header.objtype == OBJ_ShipType);

                    spFleetStr += GetStrengthOfShip(insideship2->ship);

                    insidenode = insidenode->next;
                }
            }
        }

        node = node->next;
    }

    spFleetStr += GetStrengthOfRUs(player->resourceUnits);
}


/*-----------------------------------------------------------------------------
    Name        : GetFleetStrengthInUniverseOfPlayer
    Description : Returns fleet strength of ships of player in universe
    Inputs      :
    Outputs     :
    Return      : Returns fleet strength of ships of player in universe
----------------------------------------------------------------------------*/
void GetFleetStrengthInUniverseOfPlayer(Player *player)
{
    Node *objnode = universe.ShipList.head;
    Ship *ship;

    spFleetStr = 0.0f;
    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (ship->playerowner == player)
        {
            spFleetStr += GetStrengthOfShip(ship);

            // also check ships inside it:

            if ((ship->shipsInsideMe != NULL))
            {
                // check ships inside too
                Node *insidenode = ship->shipsInsideMe->insideList.head;
                InsideShip *insideship2;

                while (insidenode != NULL)
                {
                    insideship2 = (InsideShip *)listGetStructOfNode(insidenode);
                    dbgAssertOrIgnore(insideship2->ship->header.objtype == OBJ_ShipType);

                    spFleetStr += GetStrengthOfShip(insideship2->ship);

                    insidenode = insidenode->next;
                }
            }
        }

        objnode = objnode->next;
    }

    spFleetStr += GetStrengthOfRUs(player->resourceUnits);
}

void GetFleetStrengthWarpScript(Player *player)
{
    spFleetStr = 0.0f;
    scriptSet(NULL,WARP_FILENAME,WarpCalcFleetStrengthTable);
}

void singlePlayerPreLoadCheck(void)
{
    if (warpLevel)
    {
        scriptSet(NULL,WARP_FILENAME,WarpPreLoadTable);
    }
}

/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
typedef void (*GetStrengthOfFleet)(Player *player);

void SetFleetModifier(sdword level,GetStrengthOfFleet getStrengthOfFleetFunc)
{
    real32 comparestrength,comparestrength2;

    comparestrength = BaseFleetStrengthForLevel[level];
    comparestrength2 = AdvancedFleetStrengthForLevel[level];

    if ((comparestrength == 0.0f) || (comparestrength2 <= comparestrength))
    {
        spFleetModifier = 0.0f;
        return;
    }

    getStrengthOfFleetFunc(&universe.players[0]);

    // set spFleetModifier to 0 to 1 based on spFleetStr commpared to comparestrength and comparestrength2

    if (spFleetStr <= comparestrength)
    {
        spFleetModifier = 0.0f;
        return;
    }
    else if (spFleetStr >= comparestrength2)
    {
        spFleetModifier = 1.0f;
        return;
    }
    else
    {
        spFleetModifier = (spFleetStr - comparestrength) / (comparestrength2 - comparestrength);
        if (spFleetModifier > 1.0f)
        {
            spFleetModifier = 1.0f;
        }
        else if (spFleetModifier < 0.0f)
        {
            spFleetModifier = 0.0f;
        }
    }
}

// this function doesn't need to be a function, its only called in one place
// and it changes the state variable of the case statement its called from, very confusing
// and its only 4 lines - 07/02/99 SA
//void spHyperspaceCommence()
//{
    //FocusCommand focusone;

//    singlePlayerGameInfo.hyperspaceState = HYPERSPACE_SHIPSLEAVING;
//    singlePlayerGameInfo.hyperspaceSubState = FOCUSING_ON_MOTHERSHIP;
//    dbgAssertOrIgnore(universe.curPlayerPtr->PlayerMothership != NULL);
//    focusone.numShips = 1;
//    focusone.ShipPtr[0] = universe.curPlayerPtr->PlayerMothership;
//    ccFocus(&universe.mainCameraCommand,&focusone);
//    singlePlayerGameInfo.hyperspaceTimeStamp = universe.totaltimeelapsed; // + HYPERSPACE_WAIT_FOR_MOTHERSHIPFOCUS_TIME;

//    speechEventFleet(STAT_F_Hyper_TakingOff, 0, universe.curPlayerIndex);
//}

void TellShipsToAbortDockForHyperspace(void)
{
    SelectCommand *selectAll;

    selectAll = selectAllCurrentPlayersNonHyperspacingShips();
    clHalt(&universe.mainCommandLayer,selectAll);
    memFree(selectAll);
}

void spAbortHyperspaceCB(char *string, featom *atom)
{
    if (singlePlayerGameInfo.hyperspaceState == HYPERSPACE_WAITINGROLLCALL)
    {
        // Let's abort hyperspace
        speechEventFleet(COMM_F_Hyper_Abort, 0, universe.curPlayerIndex);

        // give player refund on RU's
//        universe.curPlayerPtr->resourceUnits += singlePlayerGameInfo.RUNeededForNextHyperSpaceJump;
        clAutoLaunch(singlePlayerGameInfo.oldAutoLaunch,universe.curPlayerIndex);
        singlePlayerGameInfo.hyperspaceState = NO_HYPERSPACE;

        TellShipsToAbortDockForHyperspace();

        feScreenDelete(spHyperspaceRollCallScreen);
        spHyperspaceRollCallScreen = NULL;
    }
}

void spGoNowHyperspaceCB(char *string, featom *atom)
{
    if (singlePlayerGameInfo.hyperspaceState == HYPERSPACE_WAITINGROLLCALL)
    {
        SelectCommand *selectAll;
        CommandToDo *militaryGroup;
        sdword i;
        Ship *ship;
        Ship *mothership = universe.curPlayerPtr->PlayerMothership;

        dbgAssertOrIgnore(mothership);

        selectAll = selectAllCurrentPlayersNonHyperspacingShips();
        clDock(&universe.mainCommandLayer,selectAll,CAT_DOCK_STAY_INSTANT,NULL);
        memFree(selectAll);

        selectAll = selectAllCurrentPlayersHyperspacingShips();
        if(universe.curPlayerPtr->PlayerMothership->shiprace == R1)
        {
            mothershipCleanDoorForHSInstant(universe.curPlayerPtr->PlayerMothership);
        }
        for (i=0;i<selectAll->numShips;i++)
        {
            ship = selectAll->ShipPtr[i];
            if (ship->shiptype != Mothership)
            {
                SelectCommand selectone;
//                if (!(ship->shipOrDerelict.specialFlags & SPECIAL_SinglePlayerWithinHyperSpaceRange))
//                {
//putinparade:
                    selectone.ShipPtr[0] = ship;
                    selectone.numShips = 1;
                    RemoveShipsFromDoingStuff(&universe.mainCommandLayer,&selectone,0x23);   // CAT: reason 0x23 as the exe (spGoNowHyperspaceCB 0x00517408) [audit: M2-R5 command-tactics]
                    aitrackForceSteadyShip(ship);
                    GroupShipIntoMilitaryParade(&universe.mainCommandLayer,ship,mothership);
                    bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_SinglePlayerWithinHyperSpaceRange|SPECIAL_SinglePlayerInParade);
/*                }
                else
                {
                    if (!(ship->shipOrDerelict.specialFlags & SPECIAL_SinglePlayerInParade))
                    {
                        goto putinparade;
                    }
                }
*/
            }
            // CAT: no DDDFrigateDockAllDronesInstantly, Cataclysm has no drone frigate (spGoNowHyperspaceCB 0x005173a0)
        }
        memFree(selectAll);
        militaryGroup = GetMilitaryGroupAroundShip(&universe.mainCommandLayer,universe.curPlayerPtr->PlayerMothership);
        if (militaryGroup) setMilitaryParade(militaryGroup);

        feScreenDelete(spHyperspaceRollCallScreen);
        spHyperspaceRollCallScreen = NULL;
    }
}

void spShipsRemainingDrawCB(featom *atom, regionhandle region)
{
    fonthandle fhSave;
    char numstr[10];

    sprintf(numstr,"%d",singlePlayerGameInfo.rollcallShipsRemaining);

    fhSave = fontCurrentGet();                              //save the current font
    fontMakeCurrent(selGroupFont2);             //select the appropriate font
    fontPrintCentreCentreRectangle(&region->rect,colWhite,numstr);
    fontMakeCurrent(fhSave);
}

void spShipsDockedDrawCB(featom *atom, regionhandle region)
{
    fonthandle fhSave;
    char numstr[10];

    sprintf(numstr,"%d",singlePlayerGameInfo.rollcallShipsDocked);

    fhSave = fontCurrentGet();                  //save the current font
    fontMakeCurrent(selGroupFont2);             //select the appropriate font
    fontPrintCentreCentreRectangle(&region->rect,colWhite,numstr);
    fontMakeCurrent(fhSave);
}

void CalculateRollCall(void)
{
    Node *objnode = universe.ShipList.head;
    Ship *ship;

    singlePlayerGameInfo.rollcallShipsDocked = 0;
    singlePlayerGameInfo.rollcallShipsRemaining = 0;

    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (allianceIsShipAlly(ship,universe.curPlayerPtr) && !(ship->header.flags & SOF_Disabled))
        {
            if (ship->shipsInsideMe)
            {
                // CAT: every ship inside counts, no DDDFrigate exemption (CalculateRollCall 0x005176e0, 0x00517707)
                singlePlayerGameInfo.rollcallShipsDocked += ship->shipsInsideMe->insideList.num;
            }

            if (!ShipCanHyperspace(ship))
            {
                singlePlayerGameInfo.rollcallShipsRemaining++;
            }
            else if (ship->shiptype != Mothership)  // non-mothership hyperspacecapable ships must get in range
            {
                // is ship in range?
                if (ship->shipOrDerelict.specialFlags & SPECIAL_SinglePlayerWithinHyperSpaceRange)
                {
                    singlePlayerGameInfo.rollcallShipsDocked++;
                }
                else
                {
                    singlePlayerGameInfo.rollcallShipsRemaining++;
                }
            }

        }
        objnode = objnode->next;
    }


}

void TellShipsToDockForHyperspace(void)
{
    Node *objnode;
    Ship *ship;
    SelectCommand *selectAll;

    selectAll = selectAllCurrentPlayersNonHyperspacingShips();
    //cancel all the launch orders so we can then tell all ships to dock permanently.
    clCancelAllLaunchOrdersFromPlayer(universe.curPlayerPtr);
    if (clDock(&universe.mainCommandLayer,selectAll,CAT_DOCK_STAY,NULL) > 0)
    {
        // moved this here cause I don't want it said if no ships are docking
        speechEventFleet(STAT_F_Hyper_Autodock, 0, universe.curPlayerIndex);
    }
    memFree(selectAll);

    objnode = universe.ShipList.head;
    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (allianceIsShipAlly(ship,universe.curPlayerPtr) && !(ship->header.flags & SOF_Disabled))
        {
            // CAT: no DDDFrigate / ResearchShip hyperspace preparation, Cataclysm has neither type (M2-R4 ships-b retire)
        }

        objnode = objnode->next;
    }
}

bool32 AreNotInParadeConstraint(Ship *ship)
{
    if (ship->shipOrDerelict.specialFlags & SPECIAL_SinglePlayerInParade)
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}
void MakeShipsNotIncludeLaunchingShips(SelectCommand *selection)
{
    sdword i;
    CommandToDo *shipcommand;
    for(i=0;i<selection->numShips;)
    {
        if(ShipCanHyperspace(selection->ShipPtr[i]))
        {
            //ship can hyperspace
            shipcommand = getShipAndItsCommand(&universe.mainCommandLayer,selection->ShipPtr[i]);
            if(shipcommand != NULL)
            {
                if(shipcommand->ordertype.order == COMMAND_LAUNCH_SHIP)
                {
                    //launching ship can hyperspace...lets not cancel its
                    //orders
                    selection->numShips--;
                    selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
                    continue;
                }
            }
        }
        i++;
    }

}
void TellHyperspacingShipsToReady(void)
{
    SelectCommand *selectAll;
    sdword i;
    Ship *ship;

    selectAll = selectAllCurrentPlayersHyperspacingShips();
    for (i=0;i<selectAll->numShips;i++)
    {
        ship = selectAll->ShipPtr[i];
        if (ship->shiptype == Mothership)
        {
            bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_SinglePlayerWithinHyperSpaceRange);
        }
        else
        {
            if (shipInMilitaryParade(ship))
            {
                bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_SinglePlayerWithinHyperSpaceRange|SPECIAL_SinglePlayerInParade);
            }
            else
            {
                bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_SinglePlayerWithinHyperSpaceRange);
            }
        }
    }

    //MakeShipsOnlyFollowConstraints(selectAll,AreNotInParadeConstraint);
    MakeShipsNotIncludeLaunchingShips(selectAll);
    RemoveShipsFromDoingStuff(&universe.mainCommandLayer,selectAll,0x24);   // CAT: reason 0x24 as the exe (TellHyperspacingShipsToReady 0x0051783f) [audit: M2-R5 command-tactics]
    clSetMilitaryParade(&universe.mainCommandLayer,selectAll);
    memFree(selectAll);
}

sdword HyperspaceRollCallBegin(regionhandle region, sdword ID, udword event, udword data)
{
    if (!spHyperspaceRollCallHandle)
    {
        feCallbackAddMultiple(spHyperspaceCallback);      //add in the callbacks
        feDrawCallbackAddMultiple(spHyperspaceDrawCallback);
#ifdef _WIN32
        spHyperspaceRollCallHandle = feScreensLoad("FEMan\\Hyperspace_Roll_Call.fib");   //load in the screen
#else
        spHyperspaceRollCallHandle = feScreensLoad("FEMan/Hyperspace_Roll_Call.fib");   //load in the screen
#endif
    }

    CalculateRollCall();
    spHyperspaceRollCallScreen = feScreenStart(region, "HyperspaceRollCall");

    return 0;
}

ShipSinglePlayerGameInfo *spNewShipSinglePlayerGameInfo(void)
{
    ShipSinglePlayerGameInfo *newone = memAlloc(sizeof(ShipSinglePlayerGameInfo),"shipsingleGame",0);
    memset(newone,0,sizeof(*newone));
    newone->shipHyperspaceState = SHIPHYPERSPACE_NONE;
    newone->midlevelHyperspaceIn = FALSE;
    newone->midlevelHyperspaceOut = FALSE;
    newone->staticHyperspaceGate = FALSE;
    return newone;
}

void CalculateHyperspaceTimeForShip(Ship *ship,Ship *mothership)
{
    ShipSinglePlayerGameInfo *shipSinglePlayerGameInfo = ship->shipSinglePlayerGameInfo;
    vector mothershipheading;
    vector mothershipright;
    vector mothershipup;
    vector mothershipposition;
    vector mothershipToShip;

    if (mothership != NULL)
    {
        matGetVectFromMatrixCol3(mothershipheading,mothership->rotatable.rotinfo.coordsys);
        matGetVectFromMatrixCol2(mothershipright,mothership->rotatable.rotinfo.coordsys);
        matGetVectFromMatrixCol1(mothershipup,mothership->rotatable.rotinfo.coordsys);
        mothershipposition = mothership->obj.posinfo.position;
    }

    if ((mothership != NULL) && (ship != mothership))
    {
        vecSub(mothershipToShip,ship->obj.posinfo.position,mothershipposition);
        shipSinglePlayerGameInfo->forbehind = vecDotProduct(mothershipheading,mothershipToShip);
        shipSinglePlayerGameInfo->rightleft = vecDotProduct(mothershipright,mothershipToShip);
        shipSinglePlayerGameInfo->updown = vecDotProduct(mothershipup,mothershipToShip);
    }
    else
    {
        shipSinglePlayerGameInfo->forbehind = 0.0f;
        shipSinglePlayerGameInfo->rightleft = 0.0f;
        shipSinglePlayerGameInfo->updown = 0.0f;
    }

    shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_WAITING;
    shipSinglePlayerGameInfo->hsState = HS_INACTIVE;
    shipSinglePlayerGameInfo->hsFlags = 0;

    if ((mothership == NULL) || (ship == mothership))
    {
        shipSinglePlayerGameInfo->timeToDelayHyperspace = 0.0f;
    }
    else
    {
        shipSinglePlayerGameInfo->timeToDelayHyperspace =
            HYPERSPACE_BASE_WAITTIME + frandom(HYPERSPACE_RANDOM_WAITTIME);
    }
    shipSinglePlayerGameInfo->timeToHyperspace = universe.totaltimeelapsed + shipSinglePlayerGameInfo->timeToDelayHyperspace;

    bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_Hyperspacing);
    shipHasJustDisappeared(ship);
}

void CalculateHyperspaceTimesForShips(void)
{
    Node *objnode;
    Ship *ship;
    Ship *mothership = universe.curPlayerPtr->PlayerMothership;

    dbgAssertOrIgnore(mothership != NULL);

    objnode = universe.ShipList.head;
    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (allianceIsShipAlly(ship,universe.curPlayerPtr) && !(ship->header.flags & SOF_Disabled))
        {
            if (ShipCanMakeHyperspace(ship))
            {
                CalculateHyperspaceTimeForShip(ship,mothership);
            }
        }

        objnode = objnode->next;
    }
}

void AddShipToHyperspace(Ship *ship)
{
    InsideShip *insideShip;

    insideShip = memAlloc(sizeof(InsideShip),"InsideHyper",0);
    insideShip->ship = ship;
    listAddNode(&singlePlayerGameInfo.ShipsInHyperspace,&insideShip->node,insideShip);
    bitSet(ship->header.flags,SOF_Hyperspace);

    if(singlePlayerGame)
    {
        // ships in hyperspace are instantly healed/refueled:
        //ONLY in single player game!
        ship->targetable.health = ship->staticinfo->health.maxhealth;
        ship->fuel = ship->staticinfo->maxfuel;
    }

    // and any ships inside as well:
    if (ship->shipsInsideMe != NULL)
    {
        // check ships inside too
        Node *insidenode = ship->shipsInsideMe->insideList.head;
        InsideShip *insideship2;
        Ship *iship;

        while (insidenode != NULL)
        {
            insideship2 = (InsideShip *)listGetStructOfNode(insidenode);
            iship = insideship2->ship;
            dbgAssertOrIgnore(iship->header.objtype == OBJ_ShipType);

            iship->targetable.health = iship->staticinfo->health.maxhealth;
            iship->fuel = iship->staticinfo->maxfuel;

            insidenode = insidenode->next;
        }
    }
}

bool32 NoneDoing(udword state)
{
    Node* objnode;
    Ship* ship;

    objnode = universe.ShipList.head;
    while (objnode != NULL)
    {
        ship = (Ship*)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (allianceIsShipAlly(ship,universe.curPlayerPtr) &&
            ShipCanMakeHyperspace(ship) && !(ship->header.flags & SOF_Disabled))
        {
            if (ship->shipSinglePlayerGameInfo->hsState == state)
            {
                return FALSE;
            }
        }

        objnode = objnode->next;
    }

    return TRUE;
}

bool32 AllDoingExceptMe(udword state, Ship* me)
{
    Node* objnode;
    Ship* ship;

    objnode = universe.ShipList.head;
    while (objnode != NULL)
    {
        ship = (Ship*)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (allianceIsShipAlly(ship,universe.curPlayerPtr) &&
            ShipCanMakeHyperspace(ship) && !(ship->header.flags & SOF_Disabled))
        {
            if (ship != me)
            {
                if (ship->shipSinglePlayerGameInfo->hsState != state)
                {
                    return FALSE;
                }
            }
        }

        objnode = objnode->next;
    }

    return TRUE;
}

bool32 NoneDoingHS(udword state)
{
    InsideShip* insideship;
    Node* objnode;
    Ship* ship;

    objnode = singlePlayerGameInfo.ShipsInHyperspace.head;
    while (objnode != NULL)
    {
        insideship = (InsideShip*)listGetStructOfNode(objnode);
        ship = insideship->ship;
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (allianceIsShipAlly(ship,universe.curPlayerPtr) &&
            ShipCanMakeHyperspace(ship))
        {
            if (ship->shipSinglePlayerGameInfo->hsState == state)
            {
                return FALSE;
            }
        }

        objnode = objnode->next;
    }

    return TRUE;
}

void InstantlyHyperspaceOut(Ship *ship,Ship *mothership)
{
    CalculateHyperspaceTimeForShip(ship,mothership);

    //disappear ...
    ship->shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_GONE;
    if ((ship->header.flags & SOF_Hyperspace) == 0)        // extra safety for hyperspacing ships already hyperspacing
    {
        univRemoveShipFromOutside(ship, FALSE);             // CAT: the exe's 0: the FOW cube stays (the inlined copies 0x005189bf, 0x00518be9, 0x00519ce9, 0x0051ade4, 0x0051bd17)
        // ... into the hyperspace ether
        AddShipToHyperspace(ship);
    }
}

bool32 UpdateHyperspacingShip(Ship *ship,bool32 midLevel)
{
    bool32 hyperspacedout = FALSE;
    ShipSinglePlayerGameInfo *shipSinglePlayerGameInfo = ship->shipSinglePlayerGameInfo;
    static sdword hsSFXHandle = -1;

    ship->autostabilizeship = FALSE;        // ship is doing something, don't want to autostabilize
    ship->shipidle = FALSE;

    shipSinglePlayerGameInfo->midlevelHyperspaceOut = midLevel;

    dbgAssertOrIgnore(shipSinglePlayerGameInfo);

    if ((singlePlayerGameInfo.hyperspaceFails) && (!midLevel))
    {
        // do effect state machine for hyperspace fizzling here
        switch (shipSinglePlayerGameInfo->hsState)
        {
            case HS_INACTIVE:
                if (universe.totaltimeelapsed >= shipSinglePlayerGameInfo->timeToHyperspace)
                {
                    shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_ACCELERATINGAWAY;
                    shipSinglePlayerGameInfo->timeToHyperspace = universe.totaltimeelapsed + HYPERSPACE_ACCELERATE_TIME;
                    clRemoveShipFromSelection((SelectAnyCommand *)&selSelected,ship);
                    clRemoveShipFromSelection((SelectAnyCommand *)&selSelecting,ship);
//                    hyperspacedout = TRUE;
//                    shipSinglePlayerGameInfo->hsState = HS_FINISHED;
                    hsStart(ship, HYPERSPACE_SLICE_RATE, TRUE, hsShouldDisplayEffect(ship));
                    if (ship == universe.curPlayerPtr->PlayerMothership)
                    {
                        hsSFXHandle = soundEvent(ship, Ship_SinglePlayerHyperspace);
                    }
                }
                break;
            case HS_SLICING_INTO:
                if ( midLevel || (NoneDoing(HS_POPUP_INTO) && NoneDoing(HS_INACTIVE)) )
                {
                    shipSinglePlayerGameInfo->hsState = HS_COLLAPSE_INTO;
                }
                break;
            case HS_COLLAPSE_INTO:
                if (hsSFXHandle != SOUND_NOTINITED)
                {
                    if (ship == universe.curPlayerPtr->PlayerMothership)
                    {
                        soundEventStopSound(hsSFXHandle, 1);
                        hsSFXHandle = SOUND_NOTINITED;
                    }
                }
                hsUpdate(ship);
                break;
            case HS_FINISHED:
                hyperspacedout = TRUE;
                break;
            default:
                hsUpdate(ship);
                break;
        }
    }
    else
    switch (shipSinglePlayerGameInfo->hsState)
    {
    case HS_INACTIVE:
        if (universe.totaltimeelapsed >= shipSinglePlayerGameInfo->timeToHyperspace)
        {
            shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_ACCELERATINGAWAY;
            shipSinglePlayerGameInfo->timeToHyperspace = universe.totaltimeelapsed + HYPERSPACE_ACCELERATE_TIME;
            clRemoveShipFromSelection((SelectAnyCommand *)&selSelected,ship);
            clRemoveShipFromSelection((SelectAnyCommand *)&selSelecting,ship);
            hsStart(ship, HYPERSPACE_SLICE_RATE, TRUE, hsShouldDisplayEffect(ship));
            if (!singlePlayerGame || (singlePlayerGame && (ship->playerowner != universe.curPlayerPtr)))
            {
                soundEvent(ship, Ship_Hyperdrive);
            }
        }
        break;
    case HS_SLICING_INTO:
        if ( midLevel || (NoneDoing(HS_POPUP_INTO) && NoneDoing(HS_INACTIVE)) )
        {
            hsUpdate(ship);
        }
        break;
    case HS_COLLAPSE_INTO:
        if (ship->soundevent.hyperspaceHandle != SOUND_NOTINITED)
        {
            soundEventStopSound(ship->soundevent.hyperspaceHandle, 1);
            ship->soundevent.hyperspaceHandle = SOUND_NOTINITED;
        }
        if ( (!midLevel) && (ship == universe.curPlayerPtr->PlayerMothership))
        {
            if (mrWhiteOut || AllDoingExceptMe(HS_FINISHED, ship))
            {
                mrWhiteOut = TRUE;
                mrWhiteOutT = 1.0f - shipSinglePlayerGameInfo->clipt;
                hsUpdate(ship);
            }
        }
        else
        {
            hsUpdate(ship);
        }
        break;
    case HS_FINISHED:
        //disappear ...
        shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_GONE;
        if ((ship->header.flags & SOF_Hyperspace) == 0)        // extra safety for hyperspacing ships already hyperspacing
        {
            univRemoveShipFromOutside(ship, TRUE);          // CAT: the cube goes (UpdateHyperspacingShip 0x00517e90)

            if ((!midLevel) && (ship->playerowner != universe.curPlayerPtr))
            {
                ship->howDidIDie = DEATH_Left_Behind_At_Hyperspace;
                univDeleteWipeInsideShipOutOfExistence(ship);   // kill non player ships that went along for the ride
            }
            else
            {
                // ... into the hyperspace ether
                AddShipToHyperspace(ship);
            }
        }
        hyperspacedout = TRUE;
        break;
    default:
        hsUpdate(ship);
    }

    return hyperspacedout;
}

bool32 UpdateHyperspacingShips(void)
{
    Node *objnode;
    Ship *ship;
    bool32 shipsAllHyperspacedAway = TRUE;
    bool32 result;

    objnode = universe.ShipList.head;
    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        objnode = objnode->next;
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (allianceIsShipAlly(ship,universe.curPlayerPtr) && !(ship->header.flags & SOF_Disabled))
        {
            if (ShipCanMakeHyperspace(ship))
            {
                result = UpdateHyperspacingShip(ship,FALSE);
                if (singlePlayerGameInfo.hyperspaceFails)
                {
                    if (!result)
                    {
                        shipsAllHyperspacedAway = FALSE;
                    }
                }
                else
                {
                    shipsAllHyperspacedAway = FALSE;
                }
            }
        }
    }

    return shipsAllHyperspacedAway;
}

bool32 OrientHyperspacingShips(void)
{
    Node *objnode;
    Ship *ship;
    vector mothershipheading;
    bool32 shipsoriented = TRUE;
    Ship *mothership = universe.curPlayerPtr->PlayerMothership;
    udword shipsNotInHyperspaceRange = 0;

    dbgAssertOrIgnore(mothership != NULL);

    matGetVectFromMatrixCol3(mothershipheading,mothership->rotatable.rotinfo.coordsys);

    objnode = universe.ShipList.head;
    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (allianceIsShipAlly(ship,universe.curPlayerPtr) && !(ship->header.flags & SOF_Disabled))
        {
            if (ShipCanHyperspace(ship))
            {
                ship->autostabilizeship = FALSE;        // ship is doing something, don't want to autostabilize
                ship->shipidle = FALSE;

                if (ship->shiptype != Mothership)
                {
                    if (ship->shipOrDerelict.specialFlags & SPECIAL_SinglePlayerWithinHyperSpaceRange)
                    {
                        //if (!(ship->shipOrDerelict.specialFlags & SPECIAL_SinglePlayerInParade))
                        {
                            sdword shipready=FALSE;
                            shipready = aitrackHeadingWithBank(ship,&mothershipheading,HYPERSPACE_HEADINGACCURACY,ship->staticinfo->shipOrDerelict.sinbank);
                            if(!shipready)
                            {
                                singlePlayerGameInfo.rollcallShipsRemaining++;  //add to count because we really aren't ready
                                shipsNotInHyperspaceRange++;
                            }
                            shipsoriented &= shipready;

                            //don't track zero velocity
                            //if in parade...which is everyone...lets check though

                            //aitrackSteadyShipDriftOnly(ship);
                        }
                    }
                    else
                    {
                        shipsNotInHyperspaceRange++;
                        if (aiuFindDistanceSquared(ship->obj.posinfo.position,mothership->obj.posinfo.position) < HYPERSPACE_RANGE)
                        {
                            CommandToDo *shipcommand = getShipAndItsCommand(&universe.mainCommandLayer,ship);
                            if(shipcommand !=NULL)
                            {
                                if(shipcommand->ordertype.order == COMMAND_LAUNCH_SHIP)
                                {
                                    goto notreadyhscrud;
                                }
                            }
                            bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_SinglePlayerWithinHyperSpaceRange);
notreadyhscrud:;
                        }
                        else
                        {
                            aishipFlyToShipAvoidingObjs(ship,mothership,AISHIP_PointInDirectionFlying|AISHIP_FirstPointInDirectionFlying|
                                                                        AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured,0.0f);
                        }
                    }
                }
            }
        }

        objnode = objnode->next;
    }

    return shipsoriented;
}

void PutShipOutsideAt(Ship *ship,vector *createat,vector *createvelocity,real32 angle)
{
    sdword i;

    collAddSpaceObjToCollBlobs(&ship->obj);
    listAddNode(&universe.SpaceObjList,&(ship->header.objlink),ship);
    listAddNode(&universe.ShipList,&(ship->shiplink),ship);
    listAddNode(&universe.ImpactableList,&(ship->impactable.impactablelink),ship);
    bitClear(ship->header.flags, SOF_Hide);
    ship->obj.posinfo.position = *createat;
    cat_univShipFOWActivate(ship);                          // CAT: the cube comes back (PutShipOutsideAt 0x005180a0, M2-R6 nis-fow-camera)
    ship->obj.posinfo.velocity = *createvelocity;
    ship->rotatable.rotinfo.coordsys = defaultshipmatrix;

    univAddObjToRenderList(&ship->obj);

    univRotateObjYaw(&ship->rotatable,angle);

    for (i = 0; i < MAX_NUM_TRAILS; i++)
    {
        if (ship->trail[i])
        {
            trailZeroLength(ship->trail[i]);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_spGetHyperspaceModuleTypes
    Description : CAT (M2-R6 nis-fow-camera): the types of the module ships in the hyperspace ether that are clamped
                  to an sMothership (cat_spGetHyperspaceModuleTypes 0x0051c0c0; the NIS start, nisNewObjectCreate
                  0x004cccb0, gives an NIS sMothership the modules the player's mothership took into hyperspace)
    Inputs      : types - room for every ship in the ether (the exe's caller passes 30)
    Outputs     : count - number of types written
----------------------------------------------------------------------------*/
void cat_spGetHyperspaceModuleTypes(sdword *count, ShipType *types)
{
    Node *node;
    Ship *ship;
    SpaceObjRotImpTarg *host;

    *count = 0;
    for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = node->next)
    {
        ship = ((InsideShip *)listGetStructOfNode(node))->ship;
        if ((ship->guidance.clampInfo != NULL) && ((host = ship->guidance.clampInfo->host) != NULL) &&
            (host->header.objtype == OBJ_ShipType) && (((Ship *)host)->shiptype == sMothership))
        {
            types[*count] = ship->shiptype;
            (*count)++;
        }
    }
}

void CalculateArrivingTimesForShips(void)
{
    InsideShip *insideShip;
    Ship *ship;
    Node *node = singlePlayerGameInfo.ShipsInHyperspace.head;
    struct ShipSinglePlayerGameInfo *shipSinglePlayerGameInfo;

    dbgAssertOrIgnore(node != NULL);
    while (node != NULL)
    {
        insideShip = (InsideShip *)listGetStructOfNode(node);
        ship = insideShip->ship;
        shipSinglePlayerGameInfo = ship->shipSinglePlayerGameInfo;
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if ((ship->shiptype == Mothership) && (HYPERSPACE_MOTHERSHIP_ALWAYS_ARRIVES_FIRST))
        {
            shipSinglePlayerGameInfo->timeToDelayHyperspace = universe.totaltimeelapsed + HYPERSPACE_BASE_MOTHERSHIPARRIVETIME;
        }
        else
        {
            shipSinglePlayerGameInfo->timeToDelayHyperspace = universe.totaltimeelapsed + HYPERSPACE_BASE_ARRIVETIME + shipSinglePlayerGameInfo->timeToDelayHyperspace;
        }

        node = node->next;
    }
}

bool32 UpdateArrivingShip(Ship *ship,hvector *topoint,bool32 midLevel)
{
    ShipSinglePlayerGameInfo *shipSinglePlayerGameInfo = ship->shipSinglePlayerGameInfo;
    bool32 thisShipArrived = FALSE;

    dbgAssertOrIgnore(shipSinglePlayerGameInfo);

    ship->autostabilizeship = FALSE;        // ship is doing something, don't want to autostabilize
    ship->shipidle = FALSE;

    switch (shipSinglePlayerGameInfo->shipHyperspaceState)
    {
        case SHIPHYPERSPACE_GONE:
            {
                real32 forbehind;
                real32 rightleft;
                real32 updown;
                vector tempvec;
                vector createposition;
                vector createvelocity;
                real32 angle;
                matrix rotzmat;

                // calculate position based on forbehind, rightleft
                forbehind = shipSinglePlayerGameInfo->forbehind;
                rightleft = shipSinglePlayerGameInfo->rightleft;
                updown = shipSinglePlayerGameInfo->updown;

                vecSet(tempvec,forbehind,-rightleft,updown);
                vecSet(createvelocity,0.0f,0.0f,0.0f);

                angle = DEG_TO_RAD(topoint->w);

                matMakeRotAboutZ(&rotzmat,(real32)cos(angle),(real32)sin(angle));
                matMultiplyMatByVec(&createposition,&rotzmat,&tempvec);

                vecAddTo(createposition,*topoint);

                PutShipOutsideAt(ship,&createposition,&createvelocity,angle);
                bitClear(ship->header.flags,SOF_Hyperspace);

                shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_DECELERATE;
                {
                    if (midLevel)
                    {
                        shipSinglePlayerGameInfo->timeToHyperspace = 0.0f;
                    }
                    else
                    {
                        real32 waittime;
                        if (ship == universe.curPlayerPtr->PlayerMothership)
                        {
                            waittime = 2.0f * HYPERSPACE_BASE_WAITTIME;
                        }
                        else
                        {
                            waittime = 3.0f * HYPERSPACE_BASE_WAITTIME + frandom(HYPERSPACE_RANDOM_WAITTIME);
                        }
                        shipSinglePlayerGameInfo->timeToHyperspace = universe.totaltimeelapsed + waittime;
                    }
                }

                if ((ship == universe.curPlayerPtr->PlayerMothership) && (!midLevel))
                {
                    //FocusCommand focusone;
                    //TransitionType trans;

                    //focusone.numShips = 1;
                    //focusone.ShipPtr[0] = ship;
                    //trans = universe.mainCameraCommand.transition;
                    //universe.mainCameraCommand.transition = CAMERA_SET;

                    //dbgAssertOrIgnore(spSavedCameraYet);
                    //ccFocus(&universe.mainCameraCommand, &focusone);
                    //universe.mainCameraCommand.currentCameraStack->remembercam = spSavedCamera;

                    //universe.mainCameraCommand.transition = trans;
                    ccFocusOnPlayersMothership(&universe.mainCameraCommand,0);
                }
            }
            if (!midLevel)
            {
                break;
            }
            // if midLevel, deliberately fall through

        case SHIPHYPERSPACE_DECELERATE:
            if (universe.totaltimeelapsed > shipSinglePlayerGameInfo->timeToHyperspace)
            {
                vecZeroVector(ship->obj.posinfo.velocity);
                shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_DONE;
                shipSinglePlayerGameInfo->timeToHyperspace = universe.totaltimeelapsed + HYPERSPACE_DECELERATE_TIME;
                hsStart(ship, HYPERSPACE_SLICE_RATE, FALSE, hsShouldDisplayEffect(ship));
                if (!singlePlayerGame || (singlePlayerGame && (ship->playerowner != universe.curPlayerPtr)))
                {
                    soundEvent(ship, Ship_HyperdriveOff);
                }
            }
            break;

        case SHIPHYPERSPACE_DONE:
            mrWhiteOutT -= 3.0f * HYPERSPACE_SLICE_RATE;
            if (mrWhiteOutT < 0.0f)
            {
                mrWhiteOut = FALSE;
            }
            switch (shipSinglePlayerGameInfo->hsState)
            {
                case HS_SLICING_OUTOF:
                    if ( midLevel || (NoneDoingHS(HS_POPUP_OUTOF) && NoneDoingHS(HS_FINISHED)) )
                    {
                        if(!singlePlayerGame)
                        {
                            CommandToDo *command = getShipAndItsCommand(&universe.mainCommandLayer,ship);
                            if(command != NULL)
                            {
                                if(command->ordertype.order == COMMAND_MP_HYPERSPACING)
                                {
                                    //if its a capital ship (docked ships won't have this command)
                                    if(universe.totaltimeelapsed - command->hyperSpaceingTime < TW_HYPERSPACE_TELEGRAPH_WAIT_TIME)
                                    {
                                        //ship hasn't waited long enough for incomming hs...
                                        //so don't
                                        goto noupdateonincomminghs;
                                    }
                                }
                            }
                        }
                        hsUpdate(ship);
    noupdateonincomminghs:;
                    }
                    break;
                    
                case HS_COLLAPSE_OUTOF:
                    if (ship->soundevent.hyperspaceHandle != SOUND_NOTINITED)
                    {
                        soundEventStopSound(ship->soundevent.hyperspaceHandle, 1);
                        ship->soundevent.hyperspaceHandle = SOUND_NOTINITED;
                    }
                    if ((!midLevel) && (ship == universe.curPlayerPtr->PlayerMothership))
                    {
                        if (AllDoingExceptMe(HS_INACTIVE, ship))
                        {
                            hsUpdate(ship);
                        }
                    }
                    else
                    {
                        if(!singlePlayerGame)
                        {
                            if(gravwellIsShipStuckForHyperspaceing(ship))
                            {
                                //ship is stuck here because of a gravwell gen somewhere
                                goto noupdateHSMP;
                            }
                        }
                        hsUpdate(ship);
    noupdateHSMP:;
                    }
                    break;
                    
                case HS_INACTIVE:
                    if (universe.totaltimeelapsed > shipSinglePlayerGameInfo->timeToHyperspace)
                    {
                        thisShipArrived = TRUE;
                    }
                    break;
                    
                default:
                    hsUpdate(ship);
                    break;
            }

        default:
            break;
    }

    if (thisShipArrived)
    {
        bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_Hyperspacing);
    }

    return thisShipArrived;
}

bool32 UpdateArrivingShips(void)
{
    InsideShip *insideShip;
    Ship *ship;
    Node *node = singlePlayerGameInfo.ShipsInHyperspace.head;
    bool32 allShipsArrived = TRUE;
    hvector startpoint;

    GetStartPointPlayer(&startpoint);

    dbgAssertOrIgnore(node != NULL);
    while (node != NULL)
    {
        insideShip = (InsideShip *)listGetStructOfNode(node);
        ship = insideShip->ship;
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (allianceIsShipAlly(ship,universe.curPlayerPtr))
        {
            allShipsArrived &= UpdateArrivingShip(ship,&startpoint,FALSE);
        }

        node = node->next;
    }

    return allShipsArrived;
}

void GetShipsClampedToInHS(Ship *clampedto,GrowSelection *clampedships)
{
    InsideShip *insideShip;
    Ship *ship;
    Node *node = singlePlayerGameInfo.ShipsInHyperspace.head;

//    dbgAssertOrIgnore(node != NULL);
    while (node != NULL)
    {
        insideShip = (InsideShip *)listGetStructOfNode(node);
        ship = insideShip->ship;
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (ship->guidance.clampInfo)
        {
            if ((ship->guidance.clampInfo->host == &clampedto->targetable)
                && ship->shipSinglePlayerGameInfo->midlevelHyperspaceIn != TRUE)
            {
                //add ship to the ships to hs in
                growSelectAddShip(clampedships,ship);
            }
        }
        node = node->next;
    }
}

void spHyperspaceSelectionIn(SelectCommand *selection,hvector *destination)
{
    ShipSinglePlayerGameInfo *ssinfo;
    sdword i;
    sdword numShips = selection->numShips;
    GrowSelection clampedShips;

    if (numShips < 1)
    {
        return;
    }

    for (i=0;i<numShips;i++)
    {
        growSelectInit(&clampedShips);
        GetShipsClampedToInHS(selection->ShipPtr[i],&clampedShips);
        if (clampedShips.selection->numShips > 0)
        {
            spHyperspaceSelectionIn(clampedShips.selection,destination);
        }
        growSelectClose(&clampedShips);

        ssinfo = selection->ShipPtr[i]->shipSinglePlayerGameInfo;
        ssinfo->midlevelHyperspaceIn = TRUE;
        ssinfo->staticHyperspaceGate = FALSE;
        ssinfo->midlevelHyperspaceTo = *destination;
        ssinfo->timeToHyperspace += spHyperspaceDelay;
    }
}

void spHyperspaceSelectionInStatic(SelectCommand *selection,hvector *destination)
{
    ShipSinglePlayerGameInfo *ssinfo;
    sdword i;
    sdword numShips = selection->numShips;

    if (numShips < 1)
    {
        return;
    }

    for (i=0;i<numShips;i++)
    {
        ssinfo = selection->ShipPtr[i]->shipSinglePlayerGameInfo;
        ssinfo->midlevelHyperspaceIn = TRUE;
        ssinfo->staticHyperspaceGate = TRUE;
        ssinfo->midlevelHyperspaceTo = *destination;
    }
}

void GetShipsClampedTo(Ship *clampedto,GrowSelection *clampedships)
{
    Node *objnode = universe.ShipList.head;
    Ship *ship;

    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (ship->guidance.clampInfo)
        {
            if ((ship->guidance.clampInfo->host == &clampedto->targetable) && (!ShipInSelection(singlePlayerGameInfo.ShipsToHyperspace.selection,ship)))
            {
                growSelectAddShip(clampedships,ship);
            }
        }

        objnode = objnode->next;
    }
}

bool32 ShipAlreadyHyperspaceOut(Ship *ship)
{
    if ((ship->header.flags & SOF_Hyperspace) ||
        ShipInSelection(singlePlayerGameInfo.ShipsToHyperspace.selection,ship))
    {
        return TRUE;
    }

    return FALSE;
}

void spHyperspaceSelectionOut(SelectCommand *selection)
{
    Ship *ship;
    Ship *defactoMothership;
    sdword i;
    sdword numShips = selection->numShips;
    GrowSelection clampedShips;

    if (numShips < 1)
    {
        return;
    }

    defactoMothership = selection->ShipPtr[0];
    for (i=0;i<numShips;i++)
    {
        ship = selection->ShipPtr[i];
        if (!ShipAlreadyHyperspaceOut(ship))
        {
            if (ship->obj.attributes & ATTRIBUTES_StartInHS)
            {
                InstantlyHyperspaceOut(ship,defactoMothership);
                bitClear(ship->obj.attributes, ATTRIBUTES_StartInHS);
            }
            else
            {
                CalculateHyperspaceTimeForShip(ship,defactoMothership);
                ship->shipSinglePlayerGameInfo->timeToHyperspace += spHyperspaceDelay;
                if(ship->guidance.clampInfo != NULL)
                {
                    //ship is clamped to another ship...
                    if (ship->guidance.clampInfo->host->header.objtype == OBJ_ShipType) {
                        Ship* clampHost = (Ship*)ship->guidance.clampInfo->host;
                        if (clampHost->shipOrDerelict.specialFlags & SPECIAL_Hyperspacing) {
                            // host has been told to hyperspace
                            // so lets set the clamped ships hyperspace time, to the same time as its hose
                            ship->shipSinglePlayerGameInfo->timeToHyperspace =
                                clampHost->shipSinglePlayerGameInfo->timeToHyperspace;
                        }
                    }
                }
                growSelectAddShip(&singlePlayerGameInfo.ShipsToHyperspace,ship);
            }
            growSelectInit(&clampedShips);
            GetShipsClampedTo(ship,&clampedShips);
            if (clampedShips.selection->numShips > 0)
            {
                spHyperspaceSelectionOut(clampedShips.selection);
            }
            growSelectClose(&clampedShips);
        }
#ifdef HW_BUILD_FOR_DEBUGGING
        else
        {
            char warningmsg[200];
            sprintf(warningmsg,"Hyperspaced %s tried to hyperspace",ShipTypeToStr(ship->shiptype));
            clCommandMessage(warningmsg, 0xffffffff);
        }
#endif
    }
}

void spHyperspaceSelectionOutStatic(SelectCommand *selection)
{
    Ship *ship;
    Ship *defactoMothership;
    sdword i;
    sdword numShips = selection->numShips;
    GrowSelection clampedShips;

    if (numShips < 1)
    {
        return;
    }

    defactoMothership = selection->ShipPtr[0];
    for (i=0;i<numShips;i++)
    {
        ship = selection->ShipPtr[i];
        if (!ShipAlreadyHyperspaceOut(ship))
        {
            growSelectInit(&clampedShips);
            GetShipsClampedTo(ship,&clampedShips);
            if (clampedShips.selection->numShips > 0)
            {
                spHyperspaceSelectionOutStatic(clampedShips.selection);
            }
            growSelectClose(&clampedShips);
            if (ship->obj.attributes & ATTRIBUTES_StartInHS)
            {
                InstantlyHyperspaceOut(ship,defactoMothership);
            }
            else
            {
                ShipSinglePlayerGameInfo* ssinfo = ship->shipSinglePlayerGameInfo;
                if (ssinfo != NULL)
                {
                    ssinfo->staticHyperspaceGate = TRUE;
                }
                CalculateHyperspaceTimeForShip(ship,defactoMothership);
                growSelectAddShip(&singlePlayerGameInfo.ShipsToHyperspace,ship);
            }
        }
#ifdef HW_BUILD_FOR_DEBUGGING
        else
        {
            char warningmsg[200];
            sprintf(warningmsg,"Hyperspaced %s tried to hyperspace",ShipTypeToStr(ship->shiptype));
            clCommandMessage(warningmsg, 0xffffffff);
        }
#endif
    }
}

void singlePlayerInit(void)
{
    if (singlePlayerGame)
    {
        warpLevel = WarpToLevelEnabled();

        if (warpLevel)
        {
            singlePlayerGameInfo.currentMission = warpLevel;
        }
        else
        {
            singlePlayerGameInfo.currentMission = missionSequence[singlePlayerCurrentMissionIndex];
        }
    }

    singlePlayerGameInfo.hyperspaceState = NO_HYPERSPACE;
    singlePlayerGameInfo.resourceUnitsCollected = 0;
    singlePlayerGameInfo.giveComputerFleetControl = FALSE;
    singlePlayerGameInfo.asteroid0sCanMove = FALSE;
    singlePlayerGameInfo.hyperspaceFails = FALSE;
    singlePlayerGameLoadNewLevelFlag = FALSE;

    singlePlayerGameInfo.playerCanHyperspace = FALSE;
    smUpdateHyperspaceStatus(FALSE);

    listInit(&singlePlayerGameInfo.ShipsInHyperspace);
    growSelectInit(&singlePlayerGameInfo.ShipsToHyperspace);

    if (singlePlayerGame)
    {
        objectiveStartup();
        kasInit();
    }

    HYPERSPACE_FARCLIPPLANESQR = HYPERSPACE_FARCLIPPLANE * HYPERSPACE_FARCLIPPLANE;

    HYPERSPACE_DECELERATE_DIST = 0.5f * HYPERSPACE_DECELERATION * HYPERSPACE_DECELERATE_TIME*HYPERSPACE_DECELERATE_TIME;
    HYPERSPACE_DECELERATE_INITIAL_VEL = HYPERSPACE_DECELERATION * HYPERSPACE_DECELERATE_TIME;
    dbgAssertOrIgnore(HYPERSPACE_DECELERATE_DIST < HYPERSPACE_FARCLIPPLANE);
    HYPERSPACE_WHIPTOWARDSDIST = HYPERSPACE_FARCLIPPLANE - HYPERSPACE_DECELERATE_DIST;
    dbgAssertOrIgnore(HYPERSPACE_DECELERATE_INITIAL_VEL < HYPERSPACE_WHIP_VELOCITY);
}

static udword spLockoutFlags = 0;

void spMainScreen(void)
{
    if (smSensorsActive)
    {
        smSensorsCloseForGood();
    }
    cmCloseIfOpen();
    rmCloseIfOpen();
    lmCloseIfOpen();
    feAllMenusDelete();                                     //make sure no menus stay up during NIS
    // CAT: no task bar to force down (spMainScreen 0x00518d80 ends with feAllMenusDelete; M2-R8 kasf library)
}

void spLockout(udword flags)
{
    spLockoutFlags = flags;
//comment out the rest of this function to use NIS debugging keys
#if NIS_PRINT_INFO
    if (nisNoLockout)
    {
        return;
    }
#endif
    if (flags & SPLOCKOUT_DESELECT)
    {
        selSelectNone();
    }

    smSensorsDisable = TRUE;
    // CAT: no task bar lockout (the exe never writes tbDisable 0x00a6b254; spLockout 0x00518db0)
    if (flags & SPLOCKOUT_MR)
    {
        mrDisable();
    }
    if (flags & SPLOCKOUT_MOUSE)
    {
        mouseDisable();
    }
}

void spMainScreenAndLockout(udword flags)
{
    spMainScreen();
    spLockout(flags);
}

/*-----------------------------------------------------------------------------
    Name        : cat_spMainScreenAndLockoutKeepSensors             0x00518e90
    Description : CAT (M2-R8 kasf library): spMainScreenAndLockout without closing the sensors manager (the KAS
                  letterbox's finish, cat_kasWideScreenInFinish 0x004973d0): the managers and menus close and the
                  lockout is set. The exe's body is spMainScreenAndLockout 0x00518e10's without its first test;
                  this tree's spLockout does the lockout part (SinglePlayer.c's port aligns both, M2-R8 producer 4)
----------------------------------------------------------------------------*/
void cat_spMainScreenAndLockoutKeepSensors(udword flags)
{
    // CAT-TODO(stage 6 single player): the exe's lockout part (0x00518ea4 ..: spUnlockout first when locked, flag 2 also hides the cursor) is spMainScreenAndLockout 0x00518e10's; this tree's spLockout differs (audit: the hand-off had no tag)
    cmCloseIfOpen();
    rmCloseIfOpen();
    lmCloseIfOpen();
    feAllMenusDelete();
    spLockout(flags);
}

/*-----------------------------------------------------------------------------
    Name        : cat_spFlagDeleteWhenMissionEnds                   0x0051c0b0
    Description : CAT (M2-R8 kasf library): the ship is deleted when the mission ends (attribute 0x40,
                  ATTRIBUTES_DeleteAfterHSOut's bit; kasfDeleteShipsWhenMissionEnds 0x00497ed0)
----------------------------------------------------------------------------*/
void cat_spFlagDeleteWhenMissionEnds(Ship *ship)
{
    bitSet(ship->obj.attributes, ATTRIBUTES_DeleteAfterHSOut);
}

void spUnlockout(void)
{
//comment out the rest of this function to use NIS debugging keys
    smSensorsDisable = FALSE;
    // CAT: no task bar lockout (the exe never writes tbDisable 0x00a6b254; spUnlockout 0x00518f00)
    if (spLockoutFlags & SPLOCKOUT_DESELECT)
    {
        bitClear(spLockoutFlags,SPLOCKOUT_DESELECT);
    }
    if (spLockoutFlags & SPLOCKOUT_MR)
    {
        mrEnable();
        bitClear(spLockoutFlags,SPLOCKOUT_MR);
    }
    if (spLockoutFlags & SPLOCKOUT_MOUSE)
    {
        mouseEnable();
        bitClear(spLockoutFlags,SPLOCKOUT_MOUSE);
    }
}

void spHyperspaceButtonPushed(void)
{
    dbgAssertOrIgnore(singlePlayerGame);

    if (!singlePlayerGameInfo.playerCanHyperspace && !hyperspaceOverride)
    {
        return;
    }

    hyperspaceOverride = FALSE;

    singlePlayerGameInfo.playerCanHyperspace = FALSE;
    smUpdateHyperspaceStatus(FALSE);

    if (singlePlayerGameInfo.hyperspaceState != NO_HYPERSPACE)
    {
        return;
    }

    if (universe.curPlayerPtr->PlayerMothership == NULL)
    {
        return;
    }

    if (universePause)
    {
        universePause = FALSE;      // unpause game
    }

    ccFocusOnMyMothership(&universe.mainCameraCommand);

    singlePlayerGameInfo.hyperspaceState = HYPERSPACE_WAITINGROLLCALL;
    singlePlayerGameInfo.oldAutoLaunch = universe.curPlayerPtr->autoLaunch;     // restore autolaunch flag after hyperspace
    clAutoLaunch(FALSE,universe.curPlayerIndex);
    TellShipsToDockForHyperspace();
    TellHyperspacingShipsToReady();
    spMainScreenAndLockout(SPLOCKOUT_MR);

    HyperspaceRollCallBegin(ghMainRegion,0,0,0);

    if (spCollectResourcesAtEndOfMission) {
        SelectCommand *playerFleet = selectAllPlayersShips(universe.curPlayerPtr);

        // player must have a resource collector available
        if (ShiptypeInSelection(playerFleet, ResourceCollector)) {
            SelectCommand *shipsRemaining = shipLinkedListAsSelectCommand(&universe.ShipList, "remaining ships");

            MakeShipsNotIncludeTheseShips(shipsRemaining, playerFleet);          // = enemy ships remaining
            MakeShipsOnlyFollowConstraints(shipsRemaining, aiuIsShipDangerous);  // = offensive enemy ships remaining

            // player must have eliminated all offensive enemy vessels to harvest automatically safely
            if (shipsRemaining->numShips == 0) {
                sdword resourcesLeft = NumberOfEasilyAccesibleRUs(NULL);

                if (resourcesLeft > 0) {
                    char *resources_collected_strings[] = {
                        "Resources collected",
                        "Ressources collect�es",
                        "Ressourcen gesammelt",
                        "Los recursos recaudados",
                        "Le risorse raccolte",
                    };

                    universe.curPlayerPtr->resourceUnits += resourcesLeft;

                    kasfLocationCard(5000, resources_collected_strings[strCurLanguage]);
                }
            }

            memFree(shipsRemaining);
        }

        memFree(playerFleet);
    }
}

void spTaskBarHyperspaceCB(struct taskbutton *button, ubyte *userData)
{
    spHyperspaceButtonPushed();
}

void singlePlayerPostInit(bool32 loadingSaveGame)
{
    if (!loadingSaveGame)
    {
        if (spGetCurrentMission() == MISSION_1_KHARAK_SYSTEM)
        {
            // doing this using relative missions probably doesn't make much
            // sense but I'm doing it anyway for consistency
            animAviPlay(spGetPreviousMission(), spGetCurrentMission());
        }
    }
    //hyperspacetbbutton = tbButtonCreate("Hyperspace",spTaskBarHyperspaceCB,(ubyte *)0x00,0);
    //tbButtonListRefresh();
}

void singlePlayerClose(void)
{
    // CAT: no task bar buttons to refresh (singlePlayerClose 0x005193f0 starts with kasClose 0x004905f0)
    kasClose();

    if (spHyperspaceRollCallHandle)
    {
        feScreensDelete(spHyperspaceRollCallHandle);
        spHyperspaceRollCallHandle = NULL;
    }
    smSensorWeirdness = FALSE;
    // turns on Fleet command again, if it's been turned off
    soundEventSetActorFlag(SPEECH_ACTOR_FLEET_COMMAND, TRUE);
#if SP_DEBUGLEVEL2
    mrKASDebugDrawStates = FALSE;
    mrKASDebugDrawTimers = FALSE;
    mrKASDebugDrawVars = FALSE;
    mrKASDebugDrawVolumes = FALSE;
    mrKASSkipFactor = 0;
#endif
}

void singlePlayerSetMissionAttributes(char *directory,char *filename)
{
    singlePlayerGameInfo.onCompleteHyperspace = TRUE;     // default to TRUE
    singlePlayerGameInfo.onLoadPlayNIS = 0;
    singlePlayerGameInfo.onCompletePlayNIS = 0;
    singlePlayerGameInfo.resourceUnitsCollected = 0;
    if (universe.players[0].race == R1)
    {
        universe.players[1].race = R2;              // default to enemy race
    }
    else
    {
        universe.players[1].race = R1;              // default to enemy race
    }
    scriptSet(directory,filename,SinglePlayerScriptTable);

    //reset nebulae
    nebReset();
}

void singlePlayerLevelLoaded(void)
{
    // need to fix up the mission sequence index to match the currentMission
    singlePlayerCurrentMissionIndex
        = convertMissionToSequenceIndex(spGetCurrentMission());
    
    if (spGetCurrentMission() == MISSION_10_SUPER_NOVA_STATION)
    {
        Node *objnode;
        Ship *ship;

        objnode = universe.ShipList.head;
        while (objnode != NULL)
        {
            ship = (Ship *)listGetStructOfNode(objnode);
            objnode = objnode->next;
            dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

            if ((ship->playerowner->playerIndex == 1) && (ship->shiptype == Carrier))
            {
                bitSet(ship->shipOrDerelict.specialFlags2,SPECIAL_2_DontPlowThroughEnemyShips);
            }
        }
    }
}

void CleanupDamageEffects(void)
{
    InsideShip* insideShip;
    Ship* ship;
    Node* node = singlePlayerGameInfo.ShipsInHyperspace.head;

    while (node != NULL)
    {
        insideShip = (InsideShip*)listGetStructOfNode(node);
        ship = insideShip->ship;
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
        node = node->next;

        dmgForgetEffects(ship);
    }
}

void CleanupHyperspacingShip(Ship *ship)
{
    dbgAssertOrIgnore(ship->shipSinglePlayerGameInfo);
    ship->shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_NONE;
}

void CleanupHyperspacingShipsThatDidntHyperspace(void)
{
    Node *objnode;
    Ship *ship;

    objnode = universe.ShipList.head;
    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        objnode = objnode->next;
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (allianceIsShipAlly(ship,universe.curPlayerPtr) && !(ship->header.flags & SOF_Disabled))
        {
            if (ShipCanMakeHyperspace(ship))
            {
                CleanupHyperspacingShip(ship);
            }
        }
    }
}

void CleanupHyperspacingShips(bool32 FreeShips,bool32 cleanupAllPlayersShips)
{
    InsideShip *insideShip;
    Ship *ship;
    Node *node = singlePlayerGameInfo.ShipsInHyperspace.head;
    Node *deletenode;

    while (node != NULL)
    {
        insideShip = (InsideShip *)listGetStructOfNode(node);
        ship = insideShip->ship;
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
        deletenode = node;
        node = node->next;

        if (cleanupAllPlayersShips || allianceIsShipAlly(ship,universe.curPlayerPtr))
        {
            listDeleteNode(deletenode);     // put before so univFreeShipContents doesn't try to delete this twice
            if ((FreeShips) && (ship->shipSinglePlayerGameInfo->shipHyperspaceState == SHIPHYPERSPACE_GONE))
            {
                univFreeShipContents(ship);
                memFree(ship);
            }
            else
            {
                CleanupHyperspacingShip(ship);
            }
        }
    }

    if (singlePlayerGameInfo.hyperspaceFails)
    {
        CleanupHyperspacingShipsThatDidntHyperspace();
    }
}

void singlePlayerShipDied(Ship *ship)
{
    growSelectRemoveShip(&singlePlayerGameInfo.ShipsToHyperspace,ship);

    if (singlePlayerGameInfo.ShipsInHyperspace.num)
    {
        InsideShip *insideShip;
        Ship *checkship;
        Node *node = singlePlayerGameInfo.ShipsInHyperspace.head;

        while (node != NULL)
        {
            insideShip = (InsideShip *)listGetStructOfNode(node);
            checkship = insideShip->ship;
            dbgAssertOrIgnore(checkship->header.objtype == OBJ_ShipType);

            if (checkship == ship)
            {
                listDeleteNode(node);
                break;
            }

            node = node->next;
        }
    }
}

void singlePlayerGameCleanup(void)
{
    if (!singlePlayerGame)
    {
        CleanupHyperspacingShips(TRUE,TRUE);
        return;
    }

    // delete all objectives
    objectiveShutdown();

    // remove ship lists, paths, volumes, etc.
    kasLabelledEntitiesDestroy();

    pingAllPingsDelete();

    if (singlePlayerGameLoadNewLevelFlag)
    {
        return;
    }

    spUnlockout();

    CleanupHyperspacingShips(TRUE,TRUE);

    if (singlePlayerGameInfo.hyperspaceState != NO_HYPERSPACE)
    {
        if (spHyperspaceRollCallScreen != NULL)
        {
            feScreenDelete(spHyperspaceRollCallScreen);
            spHyperspaceRollCallScreen = NULL;
        }

        ccClearModeFlag(&universe.mainCameraCommand,
            CCMODE_LOCK_OUT_USER_INPUT |
            CCMODE_FOCUS_CHANGES_LOOKATPOINT_ONLY |
            CCMODE_FOCUS_CHANGES_NOTHING);

        mrWhiteOut = FALSE;
        mrWhiteOutT = 0.0f;
    }
}

void UpdateMidLevelHyperspacingShips(void)
{
    bool32 addedShipToHyperspace = FALSE;
    sdword i;

    // update any mid-level hyperspacing away ships
    for (i=0;i<singlePlayerGameInfo.ShipsToHyperspace.selection->numShips; )
    {
        if (UpdateHyperspacingShip(singlePlayerGameInfo.ShipsToHyperspace.selection->ShipPtr[i],TRUE))
        {
            addedShipToHyperspace = TRUE;
            growSelectRemoveShipIndex(&singlePlayerGameInfo.ShipsToHyperspace,i);   // deliberate don't increment i
        }
        else
        {
            i++;
        }
    }

    // update any mid-level hyperspacing in ships
    {
        Node *node = singlePlayerGameInfo.ShipsInHyperspace.head;
        Node *nextnode;
        InsideShip *insideShip;
        Ship *ship;

        while (node != NULL)
        {
            insideShip = (InsideShip *)listGetStructOfNode(node);
            ship = insideShip->ship;
            dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
            nextnode = node->next;

            if ((ship->obj.attributes & ATTRIBUTES_DeleteAfterHSOut) && (ship->header.flags & SOF_Hyperspace))
            {
                ship->howDidIDie = DEATH_Left_Behind_At_Hyperspace;
                univDeleteWipeInsideShipOutOfExistence(ship);   // will wipe out of ShipsInHyperspace list
            }
            else if (ship->shipSinglePlayerGameInfo->midlevelHyperspaceIn)
            {
                if (UpdateArrivingShip(ship,&ship->shipSinglePlayerGameInfo->midlevelHyperspaceTo,TRUE))
                {
                    ship->shipSinglePlayerGameInfo->midlevelHyperspaceIn = FALSE;
                    CleanupHyperspacingShip(ship);
                    listDeleteNode(node);
                }
            }

            node = nextnode;
        }
    }
}

/*-----------------------------------------------------------------------------
    CAT (M2-R8 kas-runtime-mission01): the player's start point, the KAS labelled vector "StartPointPlayer" (the zero
        vector without one; singlePlayerGameUpdate 0x005197d0 / singlePlayerStartNis 0x0051b670 read it inline)
-----------------------------------------------------------------------------*/
static void catSpStartPoint(hvector *point)
{
    hvector *vector = kasVectorPtr(cat_kasHashString("StartPointPlayer"));

    if (vector != NULL)
    {
        *point = *vector;
    }
    else
    {
        point->x = point->y = point->z = point->w = 0.0f;
    }
}

void singlePlayerGameUpdate(void)
{
    // CAT: a KAS script asks for an NIS or a nislet through the globals G_PlayNis / G_PlayNislet (the hashed variable
    //      store; HW1 read the AIVars "PlayNis" / "PlayNislet") (singlePlayerGameUpdate 0x005197d0)
    CatKasVar *nisVariable = cat_kasVarFind(0, cat_kasHashString("G_PlayNis"));
    CatKasVar *nisletVariable = cat_kasVarFind(0, cat_kasHashString("G_PlayNislet"));
    CatKasVar *tgui;
    sdword value;
    static real32 hyperspaceStartTime;
    // CAT: the nislet's file names (0x00a67028 / 0x00a670c8)
    static char nisletFile[160];
    static char nisletScript[160];

    // CAT: the black fade after a level load runs down at 0.5 a second of game time (0x00a6ac70; 0x0072bcb0 = 0.5)
    if (catSpLevelFade > 0.0f)
    {
        real32 last = catSpLevelFadeLastTime;

        if (catSpLevelFadeRestart)
        {
            catSpLevelFadeRestart = FALSE;
            last = universe.totaltimeelapsed;
        }
        catSpLevelFadeLastTime = universe.totaltimeelapsed;
        catSpLevelFade -= (universe.totaltimeelapsed - last) * 0.5f;
    }

    //play an nis when the time is right
    if (nisVariable)
    {
        value = nisVariable->value;
        if (value != 0)
        {
            hvector startpoint;

            catSpStartPoint(&startpoint);
            singlePlayerNisNumber = value;
            singlePlayerNISName = catSpNisNames[value][0];
            // CAT: the staging level loads first when the mission is one entered from its NIS (0x008944bc)
            singlePlayerStartNis(catSpNisNames[value][0], catSpNisNames[value][1], &startpoint,
                                 catSpMissionStartsInNis[singlePlayerGameInfo.currentMission], FALSE);
        }
        cat_kasVarDestroyEntry(nisVariable);
    }

    //play an nislet when the time is right
    if (nisletVariable)
    {
        value = nisletVariable->value;
        if (value != 0)
        {
            hvector startpoint;
            char *name;

            catSpStartPoint(&startpoint);
            singlePlayerNisletNumber = value;
            name = catSpNisletName(singlePlayerGameInfo.currentMission, value);
            // CAT-FIX: the exe copies whatever its table holds (NULL for a nislet the mission has none of)
            if (name == NULL)
            {
                dbgFatalf(DBG_Loc, "Mission %d has no nislet %d", singlePlayerGameInfo.currentMission, value);
            }
            strcpy(nisletFile, name);
            strcpy(nisletScript, nisletFile);
            strcat(nisletFile, ".nis");
            strcat(nisletScript, ".script");
            if (!fileExists(nisletFile, 0) || !fileExists(nisletScript, 0))
            {
                dbgFatalf(DBG_Loc, "'%s' and/or '%s' do not exist.", nisletFile, nisletScript);
            }
            singlePlayerNISName = nisletFile;
            singlePlayerStartNis(nisletFile, nisletScript, &startpoint, FALSE, FALSE);
            if (singlePlayerGameInfo.currentMission == 1)
            {                                           // CAT: Mission01's nislet precaches the ship textures
                cat_trPrecacheShipTextures();
            }
        }
        cat_kasVarDestroyEntry(nisletVariable);
    }

    //pause or unpause the NIS task.  This has to be called from within a task,
    //not the loading code, in case the player alt-tabs during loading.
    //Can you think of a better spot for it?
    nisTaskPauseOrResume();

    UpdateMidLevelHyperspacingShips();

    if (universe.curPlayerPtr->PlayerMothership == NULL)
    {
        if (singlePlayerGameInfo.hyperspaceState == HYPERSPACE_WAITINGROLLCALL)
        {
            spAbortHyperspaceCB(NULL,NULL);
        }
        return;
    }


    // CAT: the traders' screen opens when the "Traders" FSM sets its PopUpGUI variable (scope hash "Traders")
    tgui = cat_kasVarFind(cat_kasHashString("Traders"), cat_kasHashString("PopUpGUI"));
    if (tgui)
    {
        if (tgui->value)
        {
            tgui->value = 0;
            tmTradeBegin(ghMainRegion, 0, 0, 0);
        }
    }

    switch (singlePlayerGameInfo.hyperspaceState)
    {
        case HYPERSPACE_WAITINGROLLCALL:
            CalculateRollCall();
            OrientHyperspacingShips();
            if (singlePlayerGameInfo.rollcallShipsRemaining == 0)
            {
                if (spHyperspaceRollCallScreen != NULL)
                {
                    feScreenDelete(spHyperspaceRollCallScreen);
                    spHyperspaceRollCallScreen = NULL;
                }

                //spHyperspaceCommence();   // moved code from this function to here
//              singlePlayerGameInfo.hyperspaceState = HYPERSPACE_SHIPSLEAVING;
                singlePlayerGameInfo.hyperspaceState = HYPERSPACE_WAITINGFORFLEETCOMMAND;
                singlePlayerGameInfo.hyperspaceSubState = FOCUSING_ON_MOTHERSHIP;
                singlePlayerGameInfo.hyperspaceTimeStamp = universe.totaltimeelapsed; // + HYPERSPACE_WAIT_FOR_MOTHERSHIPFOCUS_TIME;
                speechEventFleet(STAT_F_Hyper_TakingOff, 0, universe.curPlayerIndex);
                // this isn't the nicest way to do this, but it works.  I'd rather write a callback for this
                hyperspaceStartTime = universe.totaltimeelapsed + SPEECH_HYPERSPACE_DELAY;
//                soundEventStopTrack(SongNumber, SPEECH_HYPERSPACE_DELAY + 1.5f);
            }
            break;

        case HYPERSPACE_WAITINGFORFLEETCOMMAND:
            // this isn't the nicest way to do this, but it works.  I'd rather write a callback for this
            if (universe.totaltimeelapsed >= hyperspaceStartTime)
            {
                singlePlayerGameInfo.hyperspaceState = HYPERSPACE_SHIPSLEAVING;
            }
            break;

        case HYPERSPACE_SHIPSLEAVING:
            if (singlePlayerGameInfo.hyperspaceSubState == FOCUSING_ON_MOTHERSHIP)
            {
                bool32 shipsoriented = OrientHyperspacingShips();
                if ((universe.totaltimeelapsed >= singlePlayerGameInfo.hyperspaceTimeStamp) && (shipsoriented))
                {
                    CalculateHyperspaceTimesForShips();
                    singlePlayerGameInfo.hyperspaceSubState = LOCKED_FOCUS_FOR_HOP;
                    tutGameMessage("Game_BeginHyperspace");
//                    ccSetModeFlag(&universe.mainCameraCommand, CCMODE_FOCUS_CHANGES_NOTHING);
//                    spSavedCameraYet = FALSE;
                    if (!singlePlayerGameInfo.hyperspaceFails)
                    {
                        soundEvent(universe.curPlayerPtr->PlayerMothership, Ship_SinglePlayerHyperspace);
                        soundEventStopTrack(musicEventCurrentTrack(), 1.5f);
                    }
                }

            }
            else if (singlePlayerGameInfo.hyperspaceSubState == LOCKED_FOCUS_FOR_HOP)
            {
                if (UpdateHyperspacingShips())
                {
                    if (singlePlayerGameInfo.hyperspaceFails)
                    {
                        singlePlayerGameInfo.hyperspaceState = HYPERSPACE_FAILED;
                        goto hyperspacefailed;
                    }

                    // all ships have hyperspaced out.
                    spBinkPlay = TRUE;
                    singlePlayerGameInfo.hyperspaceState = HYPERSPACE_WHITEOUT;
                    singlePlayerGameInfo.hyperspaceTimeStamp = universe.totaltimeelapsed + HYPERSPACE_WHITEOUTTIME;

                    speechEventStopAll(0.5f);

                    // screen should fade out
                    mrWhiteOut = TRUE;
                    mrWhiteOutT = 1.0f;

                    // save camera
                    //if (!spSavedCameraYet)
                    //{
                    //    Ship* mo = universe.curPlayerPtr->PlayerMothership;
//                        spSavedCamera = universe.mainCameraCommand.currentCameraStack->remembercam;
  //                      vecSubFrom(spSavedCamera.eyeposition, mo->obj.posinfo.position);
    //                    vecSubFrom(spSavedCamera.lookatpoint, mo->obj.posinfo.position);
                        //spSavedCameraYet = TRUE;
                    //}
                }
            }
            break;

        case HYPERSPACE_WHITEOUT:
            if (universe.totaltimeelapsed >= singlePlayerGameInfo.hyperspaceTimeStamp)
            {
                // playback level transition animatic
                if (spBinkPlay)
                {
#if defined(HW_GAME_RAIDER_RETREAT)
                    //no animatic between Missions 4 and 5 and thereafter
                    if (spGetCurrentMission() < MISSION_4_GREAT_WASTELANDS_TRADERS)
#endif
                    {
                        animAviPlay(spGetCurrentMission(), spGetNextMission());
                    }
                    
// was: HW_COMPUTER_GAMING_WORLD_DEMO but the standard demo
// had a short single player campaign surely? If not get rid of this
#ifdef HW_GAME_DEMO
                    if (spGetCurrentMission() == MISSION_2_OUTSKIRTS_OF_KHARAK_SYSTEM)
                    {
                        universe.quittime = universe.totaltimeelapsed;
                        utyPlugScreens = TRUE;
                    }
#elif defined(HW_GAME_RAIDER_RETREAT)
                    if (spGetCurrentMission() == MISSION_5B_TURANIC_RAIDER_PLANETOID)
                    {
                        universe.quittime = universe.totaltimeelapsed;
                        utyPlugScreens = TRUE;
                    }
#else
                    if (spGetCurrentMission() == MISSION_16_HIIGARA)
                    {
                        universe.quittime = universe.realtimeelapsed;     // CAT: quit times are real time (singlePlayerGameUpdate 0x005197d0; Cataclysm's last-mission test is stage 6)
                        utyCreditsSequence = TRUE;
                    }
#endif
                    spBinkPlay = FALSE;
                    mrWhiteOut = FALSE;
                }
                else
                {
#if 0   /* Bink...the VOICES...MAKE IT STOP!#@$1 */
                    if (!binkDonePlaying)
                    {
                        break;
                    }
#endif

                    // clear damage effects from ships
                    CleanupDamageEffects();

                    // rebuild level now
                    catSpLevelFromNisStop = FALSE;      // CAT: the next mission (0x00a6ac74 cleared, 0x00519...)
                    singlePlayerGameLoadNewLevelFlag = TRUE;
                    singlePlayerHyperspacingInto = TRUE;
//                    singlePlayerGameInfo.hyperspaceState = HYPERSPACE_SHIPSARRIVING;
                    singlePlayerGameInfo.hyperspaceState = HYPERSPACE_LOADDONE;
                }
            }
            break;

        case HYPERSPACE_LOADDONE:
            // CAT: the level the fleet arrives in starts its KAS script here, before the fleet arrives (the mission
            //      flow's entry, singlePlayerGameUpdate 0x005197d0: kasMissionStart("missionNN") for missions 1 .. 17;
            //      HW1 started it once every ship had arrived); the screens close and the player is locked out
            singlePlayerGameInfo.hyperspaceState = HYPERSPACE_SHIPSARRIVING;
            singlePlayerKasMissionStart(spGetCurrentMission());
            nisFullyScissored = TRUE;                               // 0x00a4cb58
            spMainScreenAndLockout(SPLOCKOUT_MR);
            mouseCursorHide();
            if (singlePlayerGameInfo.currentMission == 1)
            {                                                       // Mission01 keeps the letterbox for its nislet
                catSmHideViewport = TRUE;                           // 0x00a5b228
                catNisKeepScissored = TRUE;                         // 0x00a4cacc
            }
            if (catSpMissionArrivesAtOnce[singlePlayerGameInfo.currentMission])
            {
                catSpArriveAtOnce = TRUE;
                catSpArriveAtOnceTime = universe.realtimeelapsed + 5.0f;   // 0x0072bcb8
                return;
            }
            break;

        case HYPERSPACE_SHIPSARRIVING:
            // CAT: the level waits for its KAS script's StartLevelByHyperspacingShips (no case in the exe)
            break;

        case HYPERSPACE_CATSHIPSHYPERSPACINGIN:
            {
                // CAT: the fleet arrives at the start point (singlePlayerGameUpdate 0x005197d0, its case 9)
                bool32 allArrived;
                hvector startpoint;
                Node *node, *nextnode;
                Ship *ship;

                if (!catSpMissionArrivesAtOnce[singlePlayerGameInfo.currentMission])
                {
                    // the white flash of Mission01's nislet runs down after 3 s of real time (0x00a75280; 0x0072a5f0
                    //      0.0001, 0x0072bcc0 3.0 (a double), 0x0072bcb0 0.5)
                    if (catSpWhiteFlash > 0.0f)
                    {
                        static real32 flashLast, flashStart;

                        if (catSpWhiteFlash == 1.0f)
                        {
                            catSpWhiteFlash -= 0.0001f;
                            flashLast = universe.realtimeelapsed;
                            flashStart = universe.realtimeelapsed + 3.0f;
                        }
                        if (flashStart < universe.realtimeelapsed)
                        {
                            catSpWhiteFlash -= (universe.realtimeelapsed - flashLast) * 0.5f;
                        }
                        flashLast = universe.realtimeelapsed;
                    }
                    allArrived = TRUE;
                    catSpStartPoint(&startpoint);
                    for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = node->next)
                    {
                        ship = ((InsideShip *)listGetStructOfNode(node))->ship;
                        if (allianceIsShipAlly(ship, universe.curPlayerPtr))
                        {
                            allArrived &= UpdateArrivingShip(ship, &startpoint, FALSE);
                        }
                    }
                }
                else
                {
                    // CAT-TODO(stage 6 single player): the missions of 0x00894470 put the fleet at the start point at once when catSpArriveAtOnce is set (0x00519f90 ..: PutShipOutsideAt with the ship's formation offset, the parade, cat_spLaunchDockedShipsOnArrival, the focus) and then wait for the black fade; this tree flies them in (UpdateArrivingShip)
                    catSpArriveAtOnce = FALSE;
                    allArrived = TRUE;
                    catSpStartPoint(&startpoint);
                    for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = node->next)
                    {
                        ship = ((InsideShip *)listGetStructOfNode(node))->ship;
                        if (allianceIsShipAlly(ship, universe.curPlayerPtr))
                        {
                            allArrived &= UpdateArrivingShip(ship, &startpoint, FALSE);
                        }
                    }
                }

                if (allArrived)
                {
                    // the scripts learn it through G_ShipsHaveArrived; the fleet leaves the ether's list
                    cat_kasVarFindOrCreate(0, cat_kasHashString("G_ShipsHaveArrived"))->value = 1;
                    catSpFleetArriving = FALSE;
                    singlePlayerGameInfo.hyperspaceState = NO_HYPERSPACE;
                    for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = nextnode)
                    {
                        nextnode = node->next;
                        ship = ((InsideShip *)listGetStructOfNode(node))->ship;
                        if (allianceIsShipAlly(ship, universe.curPlayerPtr))
                        {
                            listDeleteNode(node);
                            ship->shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_NONE;
                            bitClear(ship->shipOrDerelict.specialFlags, SPECIAL_SinglePlayerWithinHyperSpaceRange);
                        }
                    }
                    if (singlePlayerGameInfo.hyperspaceFails)
                    {
                        for (node = universe.ShipList.head; node != NULL; node = node->next)
                        {
                            ship = (Ship *)listGetStructOfNode(node);
                            if (cat_spShipCanHyperspace(ship) &&
                                (ship->staticinfo->shipIsCapital || (ship->staticinfo->shipclass == CLASS_Component)) &&
                                bitTest(ship->shipOrDerelict.specialFlags, SPECIAL_SinglePlayerWithinHyperSpaceRange))
                            {
                                ship->shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_NONE;
                                bitClear(ship->shipOrDerelict.specialFlags, SPECIAL_SinglePlayerWithinHyperSpaceRange);
                            }
                        }
                    }
                    cat_soundEventLetterboxVolume(TRUE);
                    for (node = universe.ShipList.head; node != NULL; node = node->next)
                    {
                        ship = (Ship *)listGetStructOfNode(node);
                        cat_rmResearchModuleActivated(ship->playerowner, ship->shiptype, FALSE);
                    }
                    clAutoLaunch(singlePlayerGameInfo.oldAutoLaunch,universe.curPlayerIndex);
                    LaunchAllInternalShipsOfPlayerThatMustBeLaunched(universe.curPlayerPtr);
                    aiplayerGameStart(universe.players[1].aiPlayer);

                    // a mission not entered from its NIS plays it now, where the fleet arrived
                    if ((catSpMissionNis[singlePlayerGameInfo.currentMission] != -1) &&
                        !catSpMissionStartsInNis[singlePlayerGameInfo.currentMission])
                    {
                        catSpStartPoint(&startpoint);
                        singlePlayerNisNumber = catSpMissionNis[singlePlayerGameInfo.currentMission];
                        singlePlayerNISName = catSpNisNames[singlePlayerNisNumber][0];
                        singlePlayerStartNis(catSpNisNames[singlePlayerNisNumber][0],
                                             catSpNisNames[singlePlayerNisNumber][1], &startpoint, FALSE, FALSE);
                        return;
                    }
                }
            }
            break;
hyperspacefailed:
        case HYPERSPACE_FAILED:
            {
                // all ships have arrived and are in starting position
                singlePlayerGameInfo.hyperspaceState = NO_HYPERSPACE;
                CleanupHyperspacingShips(FALSE,FALSE);

                spUnlockout();

                //launch all necessary ships first
                LaunchAllInternalShipsOfPlayerThatMustBeLaunched(universe.curPlayerPtr);

                clAutoLaunch(singlePlayerGameInfo.oldAutoLaunch,universe.curPlayerIndex);

                tutGameMessage("Game_HyperspaceFailed");
                singlePlayerGameInfo.hyperspaceFails = FALSE;
            }
            break;
            
        default:
            break;
    }
}

static void SetOnMissionCompleteInfo(char *directory,char *field,void *dataToFillIn)
{
    char str[30];
    SinglePlayerGameInfo *info = (SinglePlayerGameInfo *)dataToFillIn;

    RemoveCommasFromString(field);

    sscanf(field,"%d %s",&info->onCompletePlayNIS,str);

    if (strcmp(str,"NOHYPERSPACE") == 0)
    {
        info->onCompleteHyperspace = FALSE;
    }
    else
    {
        info->onCompleteHyperspace = TRUE;
    }
}

void DeleteAllRemainingShips(void)
{
    Ship *ship;
    Node *node = universe.ShipList.head;

    while (node != NULL)
    {
        ship = listGetStructOfNode(node);
        ship->howDidIDie = DEATH_Left_Behind_At_Hyperspace;
        univReallyDeleteThisShipRightNow(ship);

        node = universe.ShipList.head;  // walk the list like this because univReallyDeleteThisShipRightNow is always
                                        // modifying the list of ships as it deletes stuff
    }
}

void DeleteAnyMidLevelHyperspacingShips(void)
{
    Node *node = singlePlayerGameInfo.ShipsInHyperspace.head;
    Node *nextnode;
    InsideShip *insideShip;
    Ship *ship;

    while (node != NULL)
    {
        insideShip = (InsideShip *)listGetStructOfNode(node);
        ship = insideShip->ship;
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
        nextnode = node->next;

        if (ship->shipSinglePlayerGameInfo->midlevelHyperspaceOut)
        {
            ship->howDidIDie = DEATH_Left_Behind_At_Hyperspace;
            univDeleteWipeInsideShipOutOfExistence(ship);   // will wipe out of ShipsInHyperspace list
        }

        node = nextnode;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_spDeleteLeftBehindShips                       0x0051abb0
    Description : CAT (M2-R8 kas-runtime-mission01): the mothership modules in the ether that the next level's
                  mothership does not have stay behind (deleted): a component that is not one of the fixed modules
                  (Special, Engineering, Armour, Micro, Weapons, Support, BigGun, DockingBay, CarrierSupport) and not one
                  of the level's module types (cat_univMothershipModuleTypesGet); in Mission 4 the docking bay too
----------------------------------------------------------------------------*/
void cat_spDeleteLeftBehindShips(void)
{
    Node *node = singlePlayerGameInfo.ShipsInHyperspace.head;
    CatModuleTypeList modules;
    Ship *ship;
    bool32 deleteIt;
    sdword i;

    while (node != NULL)
    {
        ship = ((InsideShip *)listGetStructOfNode(node))->ship;
        node = node->next;
        if ((ship->staticinfo->shipclass != CLASS_Component) || (ship->shiptype == sMothershipSpecial) ||
            (ship->shiptype == sMothershipEngineering) || (ship->shiptype == sMothershipArmour) ||
            (ship->shiptype == sMothershipMicro) || (ship->shiptype == sMothershipWeapons) ||
            (ship->shiptype == sMothershipSupport) || (ship->shiptype == sMothershipBigGun) ||
            (ship->shiptype == sMothershipDockingBay) || (ship->shiptype == sCarrierSupport))
        {
            deleteIt = (singlePlayerGameInfo.currentMission == 4) && (ship->shiptype == sMothershipDockingBay);
        }
        else
        {
            cat_univMothershipModuleTypesGet(&modules);
            deleteIt = TRUE;
            for (i = 0; i < modules.num; i++)
            {
                if (modules.types[i] == ship->shiptype)
                {
                    deleteIt = FALSE;
                }
            }
        }
        if (deleteIt)
        {
            ship->howDidIDie = DEATH_Left_Behind_At_Hyperspace;
            univDeleteWipeInsideShipOutOfExistence(ship);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : singlePlayerLoadNewLevel                          0x0051ac80
    Description : CAT (M2-R8 kas-runtime-mission01): Cataclysm's level change: the mission's level, or with nisLevel
                  its NIS staging level (GetMissionsDirAndFile); the mission number advances unless keepMission or
                  the level was asked for by the end of its opening NIS (catSpLevelFromNisStop). Entering Mission01
                  from its opening NIS, the NIS's fleet docks in the mothership and the capital ships and modules go
                  into the hyperspace ether, to arrive in the new level; every other ship is deleted. HW1 kept a
                  mission sequence index and computed the fleet strength by HW1's mothership type.
    Inputs      : nisLevel, keepMission
----------------------------------------------------------------------------*/
void singlePlayerLoadNewLevel(sdword nisLevel, sdword keepMission)
{
    real32 savephystime = universe.phystimeelapsed;
    real32 savetottime = universe.totaltimeelapsed;
    real32 saverealtime = universe.realtimeelapsed;     // CAT: the real time survives too
    Ship *mothership = universe.players[0].PlayerMothership;
    real32 base, advanced;
    Node *node, *insidenode;
    Ship *ship;

    if (!nisLevel)
    {                                                   // the black fade comes up over the new level
        catSpLevelFade = 1.0f;
        catSpLevelFadeRestart = TRUE;
    }
    else
    {
        catSpLevelFade = 0.0f;
    }
    catSpBigGunAllowed = TRUE;                          // 0x00894108
    universeSwitchToPlayer(0);
    singlePlayerGameLoadNewLevelFlag = TRUE;            // 0x00c0a5ac, cleared at the end
    if (!keepMission && !catSpLevelFromNisStop)
    {
        spSetCurrentMission((MissionEnum)(singlePlayerGameInfo.currentMission + 1));
    }

    if ((singlePlayerGameInfo.currentMission == 1) && !nisLevel && catSpMissionStartsInNis[1])
    {                                                   // the opening NIS's fleet into the ether
        SelectCommand *selection = selectAllCurrentPlayersNonHyperspacingShips();
        sdword i;

        for (i = 0; i < selection->numShips; i++)
        {
            univRemoveShipFromOutside(selection->ShipPtr[i], TRUE);     // CAT: 1 (0x0051ad42)
            dockPutShipInside(selection->ShipPtr[i], universe.curPlayerPtr->PlayerMothership);
        }
        memFree(selection);
        clAutoLaunch(15, 0);
        node = universe.ShipList.head;
        while (node != NULL)
        {
            ship = (Ship *)listGetStructOfNode(node);
            if ((ship->playerowner == universe.curPlayerPtr) && cat_spShipCanHyperspace(ship) &&
                (ship->staticinfo->shipIsCapital || (ship->staticinfo->shipclass == CLASS_Component)))
            {
                CalculateHyperspaceTimeForShip(ship, universe.curPlayerPtr->PlayerMothership);
                ship->shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_GONE;
                if (!bitTest(ship->header.flags, SOF_Hyperspace))
                {
                    univRemoveShipFromOutside(ship, FALSE);                 // CAT: 0 (0x0051ade1)
                    AddShipToHyperspace(ship);
                    ship->shipSinglePlayerGameInfo->midlevelHyperspaceOut = FALSE;
                    node = universe.ShipList.head;      // the list changed: start over
                }
                else
                {
                    node = node->next;                  // CAT-FIX: the exe starts over here too and never ends
                }
            }
            else
            {
                node = node->next;
            }
        }
    }

    // the ships of a mid-level hyperspace jump stay behind; every ship left in the universe goes
    node = singlePlayerGameInfo.ShipsInHyperspace.head;
    while (node != NULL)
    {
        ship = ((InsideShip *)listGetStructOfNode(node))->ship;
        node = node->next;
        if (ship->shipSinglePlayerGameInfo->midlevelHyperspaceOut)
        {
            ship->howDidIDie = DEATH_Left_Behind_At_Hyperspace;
            univDeleteWipeInsideShipOutOfExistence(ship);
        }
    }
    DeleteAllRemainingShips();

    rmResetStaticInfo();
    universeReset();
    univupdateReset();

    universe.phystimeelapsed = savephystime;
    universe.totaltimeelapsed = savetottime;
    universe.realtimeelapsed = saverealtime;
    universe.players[0].PlayerMothership = mothership;

    // restart computer AI player
    universe.players[1].aiPlayer = aiCurrentAIPlayer = aiplayerInit(&universe.players[1],AI_ADVANCED);
    aiCurrentAIPlayer->primaryEnemyPlayer = &universe.players[0];

    // remove sensor weirdness if it's on
    kasfSensorsWeirdness(FALSE);
    cat_spDeleteLeftBehindShips();

    singlePlayerGameInfo.giveComputerFleetControl = FALSE;      // reset any computer control
    singlePlayerGameInfo.asteroid0sCanMove = FALSE;

    GetMissionsDirAndFile(spGetCurrentMission(), nisLevel); //what mission file to load?
    universe.players[1].race = RACE_Beast;                  // CAT: the enemy is always race 1 (the Beast)
    if (!catSpStartInLoadingScreen)                         // 0x00a6ac68: gameStart's loading screen is up
    {
        horseRaceInit();
    }
    levelPreInit(spMissionsDir,spMissionsFile);                 //see what ships need to be loaded and what ships we can toss

    // pause sound engine
    soundEventPause(TRUE);
    //load in all the ships, register all the textures
    universeStaticInit();

    if (hrAbortLoadingGame)
    {
        goto abortload;
    }

    trailsRecolorize();
    mistrailsRecolorize();

    // CAT: the fleet modifier from the strength of player 0's fleet in the ether (the ships and the ships inside them,
    //      a mothership counting nothing, by ship class) and its RUs
    base = BaseFleetStrengthForLevel[singlePlayerGameInfo.currentMission];
    advanced = AdvancedFleetStrengthForLevel[singlePlayerGameInfo.currentMission];
    spFleetModifier = 0.0f;
    if ((base != 0.0f) && (base < advanced))
    {
        spFleetStr = 0.0f;
        for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = node->next)
        {
            ship = ((InsideShip *)listGetStructOfNode(node))->ship;
            if (ship->playerowner == &universe.players[0])
            {
                spFleetStr += (ship->staticinfo->shipclass != CLASS_Mothership) ? (real32)ship->staticinfo->buildCost : 0.0f;
                if (ship->shipsInsideMe != NULL)
                {
                    for (insidenode = ship->shipsInsideMe->insideList.head; insidenode != NULL; insidenode = insidenode->next)
                    {
                        Ship *inside = ((InsideShip *)listGetStructOfNode(insidenode))->ship;
                        spFleetStr += (inside->staticinfo->shipclass != CLASS_Mothership) ? (real32)inside->staticinfo->buildCost : 0.0f;
                    }
                }
            }
        }
        spFleetStr = (real32)universe.players[0].resourceUnits + spFleetStr;
        if (spFleetStr > base)
        {
            if (spFleetStr < advanced)
            {
                spFleetModifier = (spFleetStr - base) / (advanced - base);
                if (spFleetModifier > 1.0f)
                {
                    spFleetModifier = 1.0f;
                }
                else if (spFleetModifier < 0.0f)
                {
                    spFleetModifier = 0.0f;
                }
            }
            else
            {
                spFleetModifier = 1.0f;
            }
        }
    }

    levelStartNext(spMissionsDir,spMissionsFile);
#if HWI_ORACLE
    {                                                   // HWI-TEST: the mission self-test's flow line
        char line[100];
        snprintf(line, sizeof(line), "level loaded %s%s", spMissionsDir, spMissionsFile);
        catMissionSelfTestEvent(line);
    }
#endif
    clPresetShipsToPosition(&universe.mainCommandLayer);
    // CAT: the effects module starts on the first level (later levels reset it), then the pings, subtitles and
    //      textures (0x0051b0d0 ..)
    if (!etgHasBeenStarted)                                 // 0x00ab7668 (utility.c)
    {
        etgStartup();
        utySet(SSA_Effect);                                 // 0x00ab7631
        etgHasBeenStarted = TRUE;
    }
    else
    {
        etgReloadReset();
    }
    pingReset();
    subReset();
    subTexturesReset();
    trRegistryRefresh();                                    //refresh registry - load in all textures
    etgFixupUV();
    utySet(SSA_RegistryRefreshed);                          // 0x00ab7639

abortload:
    //start the sound engine playing again
    soundEventPause(FALSE);

    if (!catSpStartInLoadingScreen)
    {
        horseRaceShutdown();
    }

    if (!hrAbortLoadingGame)
    {
        univUpdateRenderList();
        if (!nisLevel)
        {                                                   // the arrival times of the ships in the ether
            for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = node->next)
            {
                ShipSinglePlayerGameInfo *info;

                ship = ((InsideShip *)listGetStructOfNode(node))->ship;
                info = ship->shipSinglePlayerGameInfo;
                if ((ship->staticinfo->shipclass == CLASS_Mothership) && HYPERSPACE_MOTHERSHIP_ALWAYS_ARRIVES_FIRST)
                {
                    info->timeToDelayHyperspace = HYPERSPACE_BASE_MOTHERSHIPARRIVETIME + universe.totaltimeelapsed;
                }
                else
                {
                    info->timeToDelayHyperspace = HYPERSPACE_BASE_ARRIVETIME + info->timeToDelayHyperspace + universe.totaltimeelapsed;
                }
            }
        }
    }

    bobResetProperties();
    // CAT (M2-R6 nis-fow-camera): the local player's fog-of-war grid is made again for the new level
    //      (singlePlayerLoadNewLevel 0x0051ac80 at 0x0051b1f4, after bobResetProperties)
    cat_fowCreatePlayerGrids();
    // CAT (M2-R4 combat): the blast lists start empty on a new level
    cat_blastInit();
    // CAT (M2-R6 mainrgn-popup): the mimic lists start over with the level (singlePlayerLoadNewLevel 0x0051b1fe)
    cat_mimicInitDisguiseTables();
    singlePlayerGameLoadNewLevelFlag = FALSE;
    // CAT (M2-R5 hud): the overlay's mission timer restarts with the level (cat_missionTimerReset 0x004acb80)
    cat_missionTimerReset();
    cat_playerUpdateUpgradesFromResearch(&universe.players[0]);
}

Ship *FindPlayersMothership(Player *player)
{
    Node *objnode;
    Ship *ship;

    objnode = universe.ShipList.head;
    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (ship->playerowner == player)
        {
            if (ship->shiptype == Mothership)
            {
                return ship;
            }
        }

        objnode = objnode->next;
    }

    return NULL;
}

SelectCommand *GetAllPlayersShipsExceptMothership(Player *player)
{
    Node *objnode;
    Ship *ship;
    SelectCommand *selection;
    sdword numShips = 0;

    selection = memAlloc(sizeofSelectCommand(universe.ShipList.num),"ToDoSelection",0);

    objnode = universe.ShipList.head;
    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (ship->playerowner == player)
        {
            if (ship->shiptype != Mothership)
            {
                selection->ShipPtr[numShips++] = ship;
            }
        }

        objnode = objnode->next;
    }

    selection->numShips = numShips;

    return selection;
}

// temporary hack, primarily for the NIS stuff which has some weird
// array index offsetting based on mission number
udword spOldMissionNumber(MissionEnum missionEnum)
{
    udword oldMissionValue = (udword) missionEnum;

    if (missionEnum == MISSION_5B_TURANIC_RAIDER_PLANETOID)
    {
        oldMissionValue = (udword) MISSION_5_GREAT_WASTELANDS_REVENGE;  
    }
    
    return oldMissionValue;
}

/*-----------------------------------------------------------------------------
    CAT (M2-R8 kas-runtime-mission01): Cataclysm's KAS scripts by the exe's index (0 .. 16 = Mission01 .. Mission17,
        17 = Tutorial1; MissionEnumToWatchFunction 0x0051b340): the kasMissionStart name, the generated Init / Watch
        and function list (src/Missions, tools/m2_kas2c_gen.py) and the exe address of the script's Watch function (the
        saves' function offsets are taken from it, kasConvertFuncPtrToOffset 0x00491800)
-----------------------------------------------------------------------------*/
typedef struct
{
    char *name;
    KASInitFunction init;
    KASWatchFunction watch;
    const CatKasFunction *functions;
    const udword *count;
    udword watchExe;
} CatKasScript;

#define CAT_KAS_SCRIPTS         CAT_SP_KAS_SCRIPTS
#define CAT_KAS_SCRIPT_TUTORIAL 17
// CAT-TODO(M2-R9 part B): the generated code of Mission02 .. Mission17 (docs/mission-code-map.md 8; tools/m2_kas2c_gen.py --scripts): until then those missions start no KAS script
static const CatKasScript catKasScripts[CAT_KAS_SCRIPTS] =
{
    { "mission01", Init_Mission01, Watch_Mission01, Mission01_Functions, &Mission01_FunctionCount, CAT_MISSION01_WATCH_EXE },
    { "mission02", NULL, NULL, NULL, NULL, 0x005e0bb0 },
    { "mission03", NULL, NULL, NULL, NULL, 0x005e50d0 },
    { "mission04", NULL, NULL, NULL, NULL, 0x005ea950 },
    { "mission05", NULL, NULL, NULL, NULL, 0x005f4420 },
    { "mission06", NULL, NULL, NULL, NULL, 0x005fb6d0 },
    { "mission07", NULL, NULL, NULL, NULL, 0x00602280 },
    { "mission08", NULL, NULL, NULL, NULL, 0x006056a0 },
    { "mission09", NULL, NULL, NULL, NULL, 0x0060db20 },
    { "mission10", NULL, NULL, NULL, NULL, 0x00613360 },
    { "mission11", NULL, NULL, NULL, NULL, 0x00620240 },
    { "mission12", NULL, NULL, NULL, NULL, 0x00623e90 },
    { "mission13", NULL, NULL, NULL, NULL, 0x0062a720 },
    { "mission14", NULL, NULL, NULL, NULL, 0x006378e0 },
    { "mission15", NULL, NULL, NULL, NULL, 0x0063a1a0 },
    { "mission16", NULL, NULL, NULL, NULL, 0x00648530 },
    { "mission17", NULL, NULL, NULL, NULL, 0x00668f20 },
    { "tutorial1", Init_Tutorial1, Watch_Tutorial1, Tutorial1_Functions, &Tutorial1_FunctionCount, CAT_TUTORIAL1_WATCH_EXE },
};

// CAT: the mission's KAS script starts (singlePlayerGameUpdate 0x005197d0: kasMissionStart("missionNN") for the missions
//      1 .. 17; any other number is fatal)
void singlePlayerKasMissionStart(MissionEnum mission)
{
    const CatKasScript *script;

    if ((mission < MISSION_1_KHARAK_SYSTEM) || (mission > MISSION_17))
    {
        dbgFatalf(DBG_Loc, "Unsupported mission: %d", mission);
    }
    script = &catKasScripts[mission - 1];
    if (script->init == NULL)
    {
        return;                                 // not generated yet (CAT-TODO above)
    }
    kasMissionStart(script->name, script->init, script->watch);
}

// CAT: 0x0051b230: the index of the script whose Watch function this is, -1 for none
sdword WatchFunctionToMissionEnum(KASWatchFunction watchFunction)
{
    sdword index;

    for (index = 0; index < CAT_KAS_SCRIPTS; index++)
    {
        if ((catKasScripts[index].watch != NULL) && (watchFunction == catKasScripts[index].watch))
        {
            return index;
        }
    }
    return -1;
}

// CAT: 0x0051b340: the script's Watch function, NULL for -1 or an unknown index
KASWatchFunction MissionEnumToWatchFunction(sdword missionIndex)
{
    if ((missionIndex < 0) || (missionIndex >= CAT_KAS_SCRIPTS))
    {
        return NULL;
    }
    return catKasScripts[missionIndex].watch;
}

// CAT (M2-R8 kas-runtime-mission01): the script's function list and its Watch function's exe address (the saves' base)
const CatKasFunction *cat_spKasScriptFunctions(sdword missionIndex, udword *count, udword *watchExe)
{
    if ((missionIndex < 0) || (missionIndex >= CAT_KAS_SCRIPTS))
    {
        *count = 0;
        *watchExe = 0;
        return NULL;
    }
    *watchExe = catKasScripts[missionIndex].watchExe;   // [audit: M2-R8 kas-runtime-mission01] also for a script not generated yet
    if (catKasScripts[missionIndex].functions == NULL)
    {
        *count = 0;
        return NULL;
    }
    *count = *catKasScripts[missionIndex].count;
    *watchExe = catKasScripts[missionIndex].watchExe;
    return catKasScripts[missionIndex].functions;
}

// CAT (M2-R8 kas-runtime-mission01): HW1's race-suffixed NIS tables (nisR1Names / nisR2Names, nisletNames,
//      nisPlayRelativeToMothership) and their lookups are gone: Cataclysm's are the tables catSpNisNames /
//      catSpNisletName at the top of this file (0x00894550, 0x008945a0), read by the callers themselves
char *singlePlayerNISName = NULL;                       // 0x00a6ada4

sdword singlePlayerNisNumber    = -1;                   // 0x00894694
sdword singlePlayerNisletNumber = -1;                   // 0x00894698

/*-----------------------------------------------------------------------------
    Name        : singlePlayerNisStoppedCB                          0x0051b430
    Description : CAT: the end of a single-player NIS or nislet: after the opening NIS the player's mothership (its
                  ship of class Mothership) is known and its recons take the custom formation; the scripts learn it
                  through G_Nis<n>Complete / G_Nislet<n>Complete; the lockout ends (or stays, catNisKeepScissored);
                  an NIS that opens its mission then loads the mission's level (the fleet goes into the ether and
                  arrives there: singlePlayerGameUpdate's HYPERSPACE_LOADDONE starts the mission's KAS script)
----------------------------------------------------------------------------*/
void singlePlayerNisStoppedCB(void)
{
    char bufferstr[32];
    Node *node;
    Ship *ship;

#if HWI_ORACLE
    {                                                   // HWI-TEST: the mission self-test's flow line
        char line[64];
        snprintf(line, sizeof(line), "nis stopped nis=%d nislet=%d", (int)singlePlayerNisNumber, (int)singlePlayerNisletNumber);
        catMissionSelfTestEvent(line);
    }
#endif

    if (singlePlayerNisNumber == 1)
    {
        GrowSelection growselect;

        universe.curPlayerPtr->PlayerMothership = NULL;
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            ship = (Ship *)listGetStructOfNode(node);
            if ((ship->playerowner == universe.curPlayerPtr) && (ship->staticinfo->shipclass == CLASS_Mothership))
            {
                universe.curPlayerPtr->PlayerMothership = ship;
                break;
            }
        }

        growSelectInit(&growselect);
        // CAT: the player's Recon ships (sRecon for a Sect player, bRecon for a Beast one) go into the custom
        //      formation, not HW1's LightInterceptors (singlePlayerNisStoppedCB 0x0051b430, 0x0051b4a0 .. 0x0051b4f0)
        if (universe.curPlayerPtr->race == RACE_Sect)
        {
            FindShipsOfShipTypeOfPlayer(&growselect,sRecon,universe.curPlayerPtr);
        }
        else if (universe.curPlayerPtr->race == RACE_Beast)
        {
            FindShipsOfShipTypeOfPlayer(&growselect,bRecon,universe.curPlayerPtr);
        }
        if (growselect.selection->numShips > 0)
        {
            clFormation(&universe.mainCommandLayer,growselect.selection,CUSTOM_FORMATION,0);   // CAT: level 0 (singlePlayerNisStoppedCB 0x0051b51d)
        }
        growSelectClose(&growselect);
    }

    // CAT: no music and no level-1 save here (HW1's); the KAS globals G_Nis%dComplete / G_Nislet%dComplete
    if (singlePlayerNisletNumber == -1)
    {
        sprintf(bufferstr,"G_Nis%dComplete",singlePlayerNisNumber);
        cat_kasVarFindOrCreate(0, cat_kasHashString(bufferstr))->value = 1;
    }
    else
    {
        sprintf(bufferstr,"G_Nislet%dComplete",singlePlayerNisletNumber);
        cat_kasVarFindOrCreate(0, cat_kasHashString(bufferstr))->value = 1;
        if (singlePlayerGameInfo.currentMission == 1)
        {
            catSpWhiteFlash = 1.0f;                     // Mission01's nislet ends in a white flash
        }
    }

    spUnlockout();
    if (catNisKeepScissored)
    {                                                   // the letterbox stays: so does the lockout
        spLockout(SPLOCKOUT_MR);
        mouseCursorHide();
    }

    if (hyperspaceAtEndOfSinglePlayerNIS)               // 0x00a6adcc
    {
        hyperspaceAtEndOfSinglePlayerNIS = FALSE;
        spHyperspaceButtonPushed();
    }

    if (singlePlayerNisNumber != -1)
    {
        if (catSpMissionStartsInNis[singlePlayerGameInfo.currentMission])
        {                                               // the opening NIS: now the mission's own level
            catSpLevelFromNisStop = TRUE;
            singlePlayerGameLoadNewLevelFlag = TRUE;
            catSpFleetArriving = TRUE;
            singlePlayerGameInfo.hyperspaceState = HYPERSPACE_LOADDONE;
            singlePlayerLoadNewLevel(FALSE, FALSE);
        }
        catSpLevelFade = 1.0f;
    }
    singlePlayerNisletNumber = -1;
    singlePlayerNisNumber = -1;
    singlePlayerNISName = NULL;
}

/*-----------------------------------------------------------------------------
    Name        : singlePlayerStartNis                              0x0051b670
    Description : CAT: plays a single-player NIS or nislet: with loadLevel the mission's NIS staging level is loaded
                  first (singlePlayerLoadNewLevel(TRUE, keepMission)); the NIS plays at positionAndAngle (NULL: the
                  KAS point "StartPointPlayer"; w = the heading in degrees), or about the KAS selection
                  "NisCentreShip"'s first ship, or about the derelict of type G_NisCentreDerelict (instance
                  G_NisCentreInstance) where the NIS has that ship. Cataclysm plays no NIS about the mothership.
    Inputs      : nisname, scriptname, positionAndAngle, loadLevel, keepMission
----------------------------------------------------------------------------*/
void singlePlayerStartNis(char *nisname, char *scriptname, hvector *positionAndAngle, bool32 loadLevel, bool32 keepMission)
{
    matrix thisIdentityMatrix;
    vector position;
    real32 angle;
    matrix rotz;
    matrix nisMatrix;
    vector motherVector;
    matrix motherMatrix;
    vector tempvec;
    hvector start;
    Ship *nisCentreShip = NULL;
    bool32 centreShipFound = FALSE;
    GrowSelection *nisSelection;
    ShipRace raceOfCentreShip = (ShipRace)-1;
    ShipType typeOfCentreShip;
    sdword instanceOfCentreShip = -1;
    CatKasVar *nisVar;
    Node *node;
    Derelict *derelict;

    if (loadLevel)
    {
        catSpLevelFromNisStop = FALSE;
        singlePlayerLoadNewLevel(TRUE, keepMission);
    }
    if (positionAndAngle == NULL)
    {
        catSpStartPoint(&start);
        positionAndAngle = &start;
    }
    vecGrabVecFromHVec(position,*positionAndAngle);
    angle = DEG_TO_RAD(positionAndAngle->w);
    typeOfCentreShip = ShipType_Uninitialized;

    spMainScreenAndLockout(SPLOCKOUT_EVERYTHING);
    mouseCursorHide();

    thisNisDoneCB = singlePlayerNisStoppedCB;
    thisNisHeader = nisLoad(nisname, scriptname);
#if HWI_ORACLE
    {                                                   // HWI-TEST: the mission self-test's flow line
        char line[200];
        snprintf(line, sizeof(line), "nis start %s", nisname);
        catMissionSelfTestEvent(line);
    }
#endif

    //see if we should centre NIS around a ship
    nisSelection = kasGetGrowSelectionPtrIfExists(cat_kasHashString("NisCentreShip"));    // CAT: by hash (singlePlayerStartNis 0x0051b670)
    if ((nisSelection) && (nisSelection->selection->numShips > 0))
    {
        nisCentreShip = nisSelection->selection->ShipPtr[0];
        kasGrowSelectionClear(nisSelection);
        typeOfCentreShip = nisCentreShip->shiptype;
    }

    //see if we should centre NIS around a derelict
    nisVar = cat_kasVarFind(0, cat_kasHashString("G_NisCentreDerelict"));     // CAT: a KAS global (was the AIVar)
    if (nisVar != NULL)
    {
        raceOfCentreShip = NSR_Derelict;
        typeOfCentreShip = (ShipType)nisVar->value;
        nisCentreShip = NULL;
        //find a derelict of the specified type
        for (node = universe.DerelictList.head; node != NULL; node = node->next)
        {
            derelict = (Derelict *)listGetStructOfNode(node);
            if (derelict->derelicttype == (DerelictType)typeOfCentreShip)
            {                                               //if this is the derelict we're looking for
                nisCentreShip = (Ship *)derelict;
                break;
            }
        }
    }
    //see if a ship instance was specified
    nisVar = cat_kasVarFind(0, cat_kasHashString("G_NisCentreInstance"));
    if (nisVar != NULL)
    {
        instanceOfCentreShip = nisVar->value;
    }

    if (nisCentreShip)
    {
        vector heading;
        vector normal = {1.0, 0.0, 0.0};
        real32 cross;

        centreShipFound = nisShipStartPosition(&motherVector, &motherMatrix, thisNisHeader, raceOfCentreShip, typeOfCentreShip, instanceOfCentreShip);
        position = nisCentreShip->obj.posinfo.position;
        //to get the ships rotation abouts Z, get it's heading on the XY plane,
        //cross it with a 'normal' (zero degree) vector and check sign of Y to determine what to do with cross product
        matGetVectFromMatrixCol3(heading, nisCentreShip->rotatable.rotinfo.coordsys);
        heading.z = 0;
        vecNormalize(&heading);
        cross = vecDotProduct(heading, normal);
        if (heading.y < 0.0f)
        {
            angle = TWOPI - (real32)acos((double)cross);
        }
        else
        {
            angle = (real32)acos((double)cross);
        }
    }

    if (centreShipFound)
    {
        // we found the centre ship, so let's play the NIS relative to it:
        matCopyAndTranspose(&motherMatrix, &thisIdentityMatrix);
        matMultiplyMatByMat(&nisMatrix,&nisCentreShip->rotatable.rotinfo.coordsys,&thisIdentityMatrix);

        matMultiplyMatByVec(&tempvec,&nisMatrix,&motherVector);
        vecSubFrom(position,tempvec);
    }
    else
    {                                                       //play about the point
        thisIdentityMatrix = IdentityMatrix;
        matMakeRotAboutZ(&rotz,(real32)cos(angle),(real32)sin(angle));
        matMultiplyMatByMat(&nisMatrix,&thisIdentityMatrix,&rotz);
    }

    thisNisPlaying = nisStart(thisNisHeader, &position, &nisMatrix);
    nisTaskResume();
}

void RotatePlayersFleetAbout(Player *player,vector about,real32 deg)
{
    Node *objnode;
    Ship *ship;
    vector position;
    vector rotatedposition;
    matrix rotzmat;
    real32 rot = DEG_TO_RAD(deg);

    objnode = universe.ShipList.head;
    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (ship->playerowner == player)
        {
            vecSub(position,ship->obj.posinfo.position,about);

            matMakeRotAboutZ(&rotzmat,(real32)cos(rot),(real32)sin(rot));

            matMultiplyMatByVec(&rotatedposition,&rotzmat,&position);
            vecAdd(ship->obj.posinfo.position,about,rotatedposition);

            univRotateObjYaw(&ship->rotatable,rot);
        }

        objnode = objnode->next;
    }
}

/*-----------------------------------------------------------------------------
    Name        : FindShipsOfShipTypeOfPlayer
    Description : finds ships of shiptype and owned by player, puts them in growselect
    Inputs      : growselect, shiptype, player
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void FindShipsOfShipTypeOfPlayer(GrowSelection *growselect,ShipType shiptype,Player *player)
{
    Node *node;
    Ship *ship;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if ((ship->playerowner == player) && (ship->shiptype == shiptype))
        {
            growSelectAddShip(growselect,ship);
        }
    }
}


#define MAXLINENEED 100

sdword WarpToLevelEnabled(void)
{
    filehandle fh;
    char line[MAXLINENEED];
    char tempbuf[30];
    sdword status;
    sdword warpLevel;

    if (fileExists(WARP_FILENAME,0))
    {
        fh = fileOpen(WARP_FILENAME,FF_TextMode);

        status = fileLineRead(fh,line,MAXLINENEED);

        fileClose(fh);

        if (status == FR_EndOfFile)
        {
            return 0;
        }

        if (sscanf(line,"%s %d",tempbuf,&warpLevel) == 2)
        {
            if (strcasecmp("WarpTo",tempbuf) == 0)
            {
                if (warpLevel > 1)
                {
                    return warpLevel;
                }
            }
            else
            {
                return 0;
            }
        }
    }

    return 0;
}

void WarpFleetStrengthCB(char *directory,char *field,void *dataToFillIn)
{
    ShipType shiptype;
    ShipRace shiprace;
    char shiptypestr[50];
    sdword num = 0;
    ShipStaticInfo *shipstatic;

    RemoveCommasFromString(field);

    sscanf(field,"%s %d",shiptypestr,&num);

    shiptype = StrToShipType(shiptypestr);
    if (!(shiptype < TOTAL_NUM_SHIPS))
    {
        dbgFatalf(DBG_Loc,"Bad shiptype in %s",field);
    }

    if (num <= 0)
    {
        return;
    }

    if (shiptype > STD_LAST_SHIP)
    {
        shiprace = GetValidRaceForShipType(shiptype);
    }
    else
    {
        shiptype = GetAppropriateShipTypeForRace(shiptype,universe.players[0].race);
        dbgAssertOrIgnore(shiptype != -1);
        shiprace = universe.players[0].race;
    }

    shipstatic = GetShipStaticInfo(shiptype,shiprace);
    dbgAssertOrIgnore(bitTest(shipstatic->common.infoFlags, IF_InfoLoaded));

    spFleetStr += (GetStrengthOfShipStatic(shipstatic)*num);
}

void WarpPreLoadCB(char *directory,char *field,void *dataToFillIn)
{
    ShipType shiptype;
    ShipRace shiprace;
    char shiptypestr[50];
    sdword num = 0;

    RemoveCommasFromString(field);

    sscanf(field,"%s %d",shiptypestr,&num);

    shiptype = StrToShipType(shiptypestr);
    if (!(shiptype < TOTAL_NUM_SHIPS))
    {
        dbgFatalf(DBG_Loc,"Bad shiptype in %s",field);
    }

    if (num <= 0)
    {
        return;
    }

    if (shiptype > STD_LAST_SHIP)
    {
        shiprace = GetValidRaceForShipType(shiptype);
    }
    else
    {
        shiptype = GetAppropriateShipTypeForRace(shiptype,universe.players[0].race);
        dbgAssertOrIgnore(shiptype != -1);
        shiprace = universe.players[0].race;
    }

    SetInfoNeededForShipAndRelatedStaticInfo(shiptype,shiprace,TRUE);
}

void WarpFleetRUStrengthCB(char *directory,char *field,void *dataToFillIn)
{
    sdword num = 0;

    sscanf(field,"%d",&num);
    spFleetStr += GetStrengthOfRUs(num);
}

void SetWarpFleetCB(char *directory,char *field,void *dataToFillIn)
{
    Ship *ship;
    ShipType shiptype;
    ShipRace shiprace;
    char shiptypestr[50];
    sdword num = 0;
    sdword i;
    vector zeroVector = { 0.0f, 0.0f, 0.0f };
    Player *player = &universe.players[0];

    RemoveCommasFromString(field);

    sscanf(field,"%s %d",shiptypestr,&num);

    shiptype = StrToShipType(shiptypestr);
    if (!(shiptype < TOTAL_NUM_SHIPS))
    {
        dbgFatalf(DBG_Loc,"Bad shiptype in %s",field);
    }

    if (num <= 0)
    {
        return;
    }

    if (shiptype > STD_LAST_SHIP)
    {
        shiprace = GetValidRaceForShipType(shiptype);
    }
    else
    {
        shiptype = GetAppropriateShipTypeForRace(shiptype,universe.players[0].race);
        dbgAssertOrIgnore(shiptype != -1);
        shiprace = universe.players[0].race;
    }

    dbgAssertOrIgnore(universe.players[0].PlayerMothership);

    for (i=0;i<num;i++)
    {
        ship = univAddShip(shiptype,shiprace,&zeroVector,&universe.players[0],0);
        gameStatsAddShip(ship,0);
        // Update ship totals for player
        unitCapCreateShip(ship,player);

        // CAT: no ResearchShip lab activation, Cataclysm has no research ship type (M2-R4 ships-b retire)

        GroupShipIntoMilitaryParade(&universe.mainCommandLayer,ship,universe.players[0].PlayerMothership);
    }
}

void WarpInFleet(hvector *startPoint)
{
    // Create Mothership
    Ship *ship;
    Player *player = &universe.players[0];

    ship = univAddShip(Mothership,universe.players[0].race,(vector *)startPoint,&universe.players[0],0);
    gameStatsAddShip(ship,0);
    univRotateObjYaw(&ship->rotatable,DEG_TO_RAD(startPoint->w));

    // Update ship totals for player
    unitCapCreateShip(ship,player);

    universe.players[0].PlayerMothership = ship;

    // Now add rest of fleet in parade formation
    scriptSet(NULL,WARP_FILENAME,WarpScriptTable);

    clPresetShipsToPosition(&universe.mainCommandLayer);
}

/*-----------------------------------------------------------------------------
    Name        : singleHackResearchWarp
    Description : A function that hard codes the research a player is has already
                  had based on the settings daly told me they would be in each level
                  ugly stuff... replace with an elegant solution later
    Inputs      :
    Outputs     : Allows the player to research certain topics
    Return      : void
----------------------------------------------------------------------------*/
void singleHackResearchWarp(void)
{
    switch (warpLevel)
    {
        // no breaks and reverse mission order so that you inherit
        // technology gains from previous levels
        // FIXME: this doesn't play nicely with the mission sequence
        // if that is altered
        case MISSION_16_HIIGARA:
        case MISSION_15_CHAPEL_PERILOUS:
            PlayerAcquiredTechnology("HeavyGuns");
        
        case MISSION_14_BRIDGE_OF_SIGHS:
            AllowPlayerToResearch("HeavyGuns");
            PlayerAcquiredTechnology("CloakDefenseFighter");
        
        case MISSION_13_THE_KAROS_GRAVEYARD:
            PlayerAcquiredTechnology("MissileWeapons");
            AllowPlayerToResearch("CloakDefenseFighter");
        
        case MISSION_12_GALACTIC_CORE:
            PlayerAcquiredTechnology("GravityWellGeneratorTech");
        
        case MISSION_11_TENHAUSER_GATE:
            AllowPlayerToResearch("GravityWellGeneratorTech");
            PlayerAcquiredTechnology("ProximityDetector");
            PlayerAcquiredTechnology("RepairTech");
        
        case MISSION_10_SUPER_NOVA_STATION:
            AllowPlayerToResearch("RepairTech");
            AllowPlayerToResearch("MissileWeapons");
        
        case MISSION_9_SEA_OF_LOST_SOULS:
            PlayerAcquiredTechnology("DDDFDFGFTech");
            PlayerAcquiredTechnology("TargetingSystems");
        
        case MISSION_8_THE_CATHEDRAL_OF_KADESH:
            AllowPlayerToResearch("TargetingSystems");
        
        case MISSION_7_THE_GARDENS_OF_KADESH:
            AllowPlayerToResearch("DDDFDFGFTech");
            PlayerAcquiredTechnology("MassDrive1Mt");
            PlayerAcquiredTechnology("FireControl");
            
        case MISSION_6_DIAMOND_SHOALS:
            PlayerAcquiredTechnology("PlasmaWeapons");
            AllowPlayerToResearch("MassDrive1Mt");
            AllowPlayerToResearch("FireControl");
            
        case MISSION_5_GREAT_WASTELANDS_REVENGE:
        case MISSION_5B_TURANIC_RAIDER_PLANETOID:
            AllowPlayerToResearch("PlasmaWeapons");
            PlayerAcquiredTechnology("Chassis3");
            PlayerAcquiredTechnology("IonWeapons");
            
        case MISSION_4_GREAT_WASTELANDS_TRADERS:
            PlayerAcquiredTechnology("MassDrive100Kt");
            AllowPlayerToResearch("Chassis3");
            
        case MISSION_3_RETURN_TO_KHARAK:
            PlayerAcquiredTechnology("Chassis2");
            PlayerAcquiredTechnology("MediumGuns");
            
        case MISSION_2_OUTSKIRTS_OF_KHARAK_SYSTEM:
            PlayerAcquiredTechnology("Chassis1");
            PlayerAcquiredTechnology("MassDrive10Kt");
            
        case MISSION_1_KHARAK_SYSTEM:
            PlayerAcquiredTechnology("MassDrive1Kt");
            AllowPlayerToResearch("Chassis1");
    }
}


/*-----------------------------------------------------------------------------
    Name        : singlePlayerStartGame                             0x0051bdb0
    Description : CAT (M2-R8 kas-runtime-mission01): a new single-player game starts in Mission01's opening NIS
                  (nis\n01.nis, nis\n01.script) at the origin, played in the staging level Mission01_NIS01 that
                  singlePlayerStartNis loads first (the mission number stays 1); the universe is shown when the NIS
                  ends (singlePlayerNisStoppedCB loads Mission01 and the fleet hyperspaces in)
----------------------------------------------------------------------------*/
void singlePlayerStartGame(void)
{
    hvector startPoint;

    universe.players[1].resourceUnits = 0;

    if (warpLevel)
    {
        char *nis, *script;

        // CAT-TODO(stage 6 single player): cat_spWarpToLevel 0x0051b9c0 (the exe's warp start, after tmTechInit / universeSwitchToPlayer); this tree keeps HW1's warp start
        dbgAssertOrIgnore(spGetCurrentMission() == warpLevel);

        universeSwitchToPlayer(0);

        GetMissionsDirAndFile(spGetCurrentMission(), FALSE);

        SetFleetModifier(spGetCurrentMission(), GetFleetStrengthWarpScript);
        levelStartNext(spMissionsDir,spMissionsFile);

        GetStartPointPlayer(&startPoint);
        WarpInFleet(&startPoint);

        singlePlayerLevelLoaded();

        singleHackResearchWarp();

        ccFocusOnPlayersMothership(&universe.mainCameraCommand,0);

        soundEventPlayMusic(SongNumber);

        singlePlayerKasMissionStart(spGetCurrentMission());

        if (singlePlayerGameInfo.onLoadPlayNIS > 0)
        {
            hvector startpoint;

            GetStartPointPlayer(&startpoint);
            nis = catSpNisNames[singlePlayerGameInfo.onLoadPlayNIS][0];
            script = catSpNisNames[singlePlayerGameInfo.onLoadPlayNIS][1];
            singlePlayerNisNumber = singlePlayerGameInfo.onLoadPlayNIS;
            singlePlayerNISName = nis;
            singlePlayerStartNis(nis, script, &startpoint, FALSE, FALSE);
        }

        return;
    }

    tmTechInit();
    spFleetModifier = 0.0f;                                 // 0x00a6ad84

#if HWI_ORACLE
    // HWI-ORACLE: the headless mission run (/oracleMission and the self-tests riding on it) loads Mission01 at once,
    //      without the opening NIS and with no KAS script: the mission stream's start since M2-R4
    //      (docs/m2-r4-rest-oracle.md); /catMissionTest takes the exe's start below
    if (oracleMissionDirectStart())
    {
        GetMissionsDirAndFile(spGetCurrentMission(), FALSE);
        levelInit(spMissionsDir,spMissionsFile);
        singlePlayerLevelLoaded();
        GetStartPointPlayer(&startPoint);
        return;
    }
#endif

    universeSwitchToPlayer(0);
    singlePlayerNisNumber = 1;
    singlePlayerNISName = catSpNisNames[1][0];
    startPoint.x = startPoint.y = startPoint.z = 0.0f;
    startPoint.w = 0.0f;
    singlePlayerStartNis(catSpNisNames[1][0], catSpNisNames[1][1], &startPoint, catSpMissionStartsInNis[1], TRUE);
}

void singlePlayerNextLevel(void)
{
    hvector startnisAt = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (spGetCurrentMission() == MISSION_17)            // CAT: Cataclysm's last mission
    {
        singlePlayerMissionCompleteCB();
        return;
    }

    if (singlePlayerGameInfo.onCompletePlayNIS > 0)
    {
        // CAT (M2-R8 kas-runtime-mission01): Cataclysm's NIS table (HW1's race-suffixed names are gone)
        singlePlayerNisNumber = singlePlayerGameInfo.onCompletePlayNIS;
        singlePlayerNISName = catSpNisNames[singlePlayerNisNumber][0];
        singlePlayerStartNis(catSpNisNames[singlePlayerNisNumber][0], catSpNisNames[singlePlayerNisNumber][1],
                             &startnisAt, FALSE, FALSE);
        if (singlePlayerGameInfo.onCompleteHyperspace)
        {
            hyperspaceAtEndOfSinglePlayerNIS = TRUE;
        }
        else
        {
            dbgFatal(DBG_Loc,"\nNot hyperspacing at end of level not supported yet");
        }
        return;
    }

    if (singlePlayerGameInfo.onCompleteHyperspace)
    {
        if (hyperspaceOverride)
        {
            spHyperspaceButtonPushed();
        }
    }
}

void singlePlayerMissionCompleteCB(void)
{
    selSelected.numShips = 0;
    ioUpdateShipTotals();
    mrRenderMainScreen = FALSE;
    universe.quittime = universe.realtimeelapsed;           // CAT: real time (singlePlayerMissionCompleteCB 0x0051be40)
    // this will prevent the shiplist from being displayed!
    utyCreditsSequence = TRUE;
}

void singlePlayerMissionFailedCB(void)
{
    univKillPlayer(0,PLAYERKILLED_LOSTMISSION,-1);   // CAT: nobody credited (singlePlayerMissionFailedCB 0x0051be8d pushes -1, 2, 0)
    CheckPlayerWin();
}

#if SP_DEBUGKEYS
void singlePlayerCheckDebugKeys(sdword ID)
{
#if SP_DEBUGLEVEL2
    sdword state;
#endif

    switch (ID)                 // general stuff
    {
        case RKEY:
            {
                sdword total;
                sdword numHarvestables;
                sdword numAsteroid0s;
                char totalstr[50];
                univGetResourceStatistics(&total,&numHarvestables,&numAsteroid0s);
                sprintf(totalstr,"RV=%d #R=%d #A0=%d",total,numHarvestables,numAsteroid0s);
                clCommandMessage(totalstr, 0xffffffff);
            }
            return;
    }

    if (singlePlayerGame)       // singlePlayerGame specific stuff
    switch (ID)
    {
        case CKEY:
            hyperspaceOverride = TRUE;
            singlePlayerNextLevel();
            break;

        case FKEY:
            singlePlayerMissionFailedCB();
            break;

        case EKEY:
            {
                char totalstr[50];
                GetFleetStrengthInUniverseOfPlayer(&universe.players[0]);
                sprintf(totalstr,"Fleet Str = %f",spFleetStr);
                clCommandMessage(totalstr, 0xffffffff);
            }
            break;
        case DKEY:
            kasTakeADump();
            break;

#if SP_DEBUGLEVEL2
        // debug overlays
        case SKEY: // states
            mrKASDebugDrawStates ^= TRUE;
            break;
        case TKEY: // timers
            mrKASDebugDrawTimers ^= TRUE;
            break;
        case VKEY: // vars
            mrKASDebugDrawVars ^= TRUE;
            break;
        case ZKEY: // volumes
            mrKASDebugDrawVolumes ^= TRUE;
            break;
        case AKEY: // all
            state = !(mrKASDebugDrawStates && mrKASDebugDrawTimers && mrKASDebugDrawVars && mrKASDebugDrawVolumes);
            mrKASDebugDrawStates = state;
            mrKASDebugDrawTimers = state;
            mrKASDebugDrawVars = state;
            mrKASDebugDrawVolumes = state;
            break;
        case PAGEDOWNKEY:
            mrKASSkipFactor++;
            break;
#endif //SP_DEBUGLEVEL2
    }
}
#endif //SP_DEBUGKEYS

/*-----------------------------------------------------------------------------
    Name        : spNISletTestAttempt
    Description : Attempt to test an nislet for the current level
    Inputs      : index - number of NISlet for the current level (0..2)
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
#if SP_NISLET_TEST
void spNISletTestAttempt(sdword index)
{
    hvector startpoint;
    static char nis[160], script[160];
    char *name;

    // CAT (M2-R8 kas-runtime-mission01): Cataclysm's nislet table (catSpNisletName), nislets 1 .. 3
    name = catSpNisletName(singlePlayerGameInfo.currentMission, index + 1);
    if (name == NULL)
    {
        dbgMessagef("Mission %d has no nislet %d", singlePlayerGameInfo.currentMission, index + 1);
        return;
    }
    GetStartPointPlayer(&startpoint);
    strcpy(nis, name);
    strcat(nis, ".nis");
    strcpy(script, name);
    strcat(script, ".script");

    if (!fileExists(nis, 0))
    {
        dbgMessagef("'%s' not found.", nis);
        return;
    }
    if (!fileExists(script, 0))
    {
        dbgMessagef("'%s' not found.", script);
        return;
    }
    singlePlayerNisletNumber = index + 1;
    singlePlayerNISName = nis;
    singlePlayerStartNis(nis, script, &startpoint, FALSE, FALSE);
}
#endif

/*-----------------------------------------------------------------------------
    Name        : spFindCameraAttitude
    Description : Find the level's default camera poistion, if any.
    Inputs      :
    Outputs     : position - position for the camera's eye point
    Return      : TRUE if the camera position could be computed, FALSE otherwise
----------------------------------------------------------------------------*/
bool32 spFindCameraAttitude(vector *position)
{
    hvector *hPosition = kasVectorPtrIfExists(cat_kasHashString(SP_CAMERA_POINT_NAME));  // CAT: by hash (spFindCameraAttitude 0x0051bea0)

    if (hPosition == NULL)
    {
        return(FALSE);
    }
    position->x = hPosition->x;
    position->y = hPosition->y;
    position->z = hPosition->z;
    return(TRUE);
}

/*=============================================================================
    Save Game Stuff
=============================================================================*/

void SaveSinglePlayerGame(void)
{
    dbgAssertOrIgnore(singlePlayerGameLoadNewLevelFlag == FALSE);

    SaveStructureOfSize(&singlePlayerGameInfo,sizeof(SinglePlayerGameInfo));

    SaveLinkedListOfInsideShips(&singlePlayerGameInfo.ShipsInHyperspace);
    SaveGrowSelection(&singlePlayerGameInfo.ShipsToHyperspace);

    // CAT: SaveSinglePlayerGame 0x0051bee0 goes on with ten numbers: the lockout flags (0x00a6adb0), the NIS letterbox
    //      and black fade (0x00a4cb38 .. 0x00a4cb58), the sensors manager's mine display (0x00a5b1ac), then the mimic
    //      tables (cat_spSaveMiscInfo 0x005b9df0) and the big gun's single-player switch (0x00894108)
    SaveInfoNumber((sdword)spLockoutFlags);
    SaveInfoNumber(Real32ToSdword(nisScissorFadeIn));
    SaveInfoNumber(Real32ToSdword(nisScissorFadeOut));
    SaveInfoNumber(Real32ToSdword(nisScissorFade));
    SaveInfoNumber(Real32ToSdword(nisBlackFade));
    SaveInfoNumber((sdword)catNisBlackFadeFlag);
    SaveInfoNumber(Real32ToSdword(nisBlackFadeDest));
    SaveInfoNumber(Real32ToSdword(nisBlackFadeRate));
    SaveInfoNumber((sdword)nisFullyScissored);
    SaveInfoNumber((sdword)catSmShowMines);
    cat_spSaveMiscInfo();
    SaveInfoNumber((sdword)catSpBigGunAllowed);
}

void LoadSinglePlayerGame(void)
{
    SaveChunk *chunk;

    singlePlayerGameLoadNewLevelFlag = FALSE;

    if (saveCat32Loading)
    {                                           // CAT (M2-R6 oracle-mode-a): the exe's 0x54-byte struct
        chunk = saveCat32ChunkStruct(LoadNextChunk(), SAVECAT32_LAYOUT(SinglePlayerGameInfo));
        VerifyChunk(chunk,BASIC_STRUCTURE,sizeof(SinglePlayerGameInfo));
        memcpy(&singlePlayerGameInfo,chunkContents(chunk),sizeof(SinglePlayerGameInfo));
        memFree(chunk);
    }
    else
    {
        LoadStructureOfSizeToAddress(&singlePlayerGameInfo,sizeof(SinglePlayerGameInfo));
    }

    LoadLinkedListOfInsideShips(&singlePlayerGameInfo.ShipsInHyperspace);
    FixLinkedListOfInsideShips(&singlePlayerGameInfo.ShipsInHyperspace);

    LoadGrowSelectionAndFix(&singlePlayerGameInfo.ShipsToHyperspace);

    // CAT: the numbers SaveSinglePlayerGame wrote (LoadSinglePlayerGame 0x0051bf90): the lockout comes back as the
    //      exe restores it (selection cleared and ship totals redrawn for SPLOCKOUT_DESELECT, the sensors disabled when
    //      any flag is set, the main region and the cursor for SPLOCKOUT_MR, the mouse for SPLOCKOUT_MOUSE), then the
    //      NIS state, which nisTaskResume picks up, the mine display, the mimic tables and the big gun's switch
    spLockoutFlags = (udword)LoadInfoNumber();
    if (spLockoutFlags & SPLOCKOUT_DESELECT)
    {
        selSelected.numShips = 0;
        ioUpdateShipTotals();
    }
    if (spLockoutFlags != 0)
    {
        smSensorsDisable = TRUE;
    }
    if (spLockoutFlags & SPLOCKOUT_MR)
    {
        mrDisable();
        mouseCursorHide();
    }
    if (spLockoutFlags & SPLOCKOUT_MOUSE)
    {
        mouseDisable();
    }
    nisScissorFadeIn = SdwordToReal32(LoadInfoNumber());
    nisScissorFadeOut = SdwordToReal32(LoadInfoNumber());
    nisScissorFade = SdwordToReal32(LoadInfoNumber());
    nisBlackFade = SdwordToReal32(LoadInfoNumber());
    catNisBlackFadeFlag = (ubyte)LoadInfoNumber();
    nisBlackFadeDest = SdwordToReal32(LoadInfoNumber());
    nisBlackFadeRate = SdwordToReal32(LoadInfoNumber());
    nisFullyScissored = (bool32)LoadInfoNumber();
    nisTaskResume();
    catSmShowMines = (bool32)LoadInfoNumber();
    cat_spLoadMiscInfo();
    catSpBigGunAllowed = (bool32)LoadInfoNumber();
}


/*=============================================================================
    CAT (M2-R6 mainrgn-popup): the single-player bits the popup menu reads
=============================================================================*/
// 0x00894108: the mothership's big gun may fire (data default 1; a KAS function at 0x0049ac30 clears it: stage 6).
//      Name chosen here
bool32 catSpBigGunAllowed = TRUE;

// 0x00894424: per mission, the hyperspace row says "next level" (1) or "hyperspace" (0) (cat_spMissionFlagValue)
static const bool32 catSpMissionNextLevel[] = { 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0 };

sdword cat_spPlayerCanHyperspace(void)                  // 0x005193b0
{
    return (singlePlayerGame && singlePlayerGameInfo.playerCanHyperspace) ? 1 : 0;
}

char *cat_spMissionFlagValue(void)                      // 0x0051c120: the popup menu's hyperspace row's text
{
    sdword mission = singlePlayerGameInfo.currentMission;

    // CAT-FIX: the exe indexes its table by the mission unchecked
    if ((mission >= 0) && (mission < (sdword)(sizeof(catSpMissionNextLevel) / sizeof(catSpMissionNextLevel[0]))) &&
        catSpMissionNextLevel[mission])
    {
        return strGetString(strSinglePlayerNextLevel);
    }
    return strGetString(strSinglePlayerHyperspace);
}
