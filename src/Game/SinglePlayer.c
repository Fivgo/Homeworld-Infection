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
#include "CameraCommand.h"    // CAT: cat_mcResetRequest / catMotionCueState (UpdateArrivingShip, the fleet's arrival)
#include "Console.h"          // CAT: catMotionCues 0x008b8844
#include "ModuleShip.h"       // CAT: cat_moduleIterFirst / Next (AddShipToHyperspace)
#include "Leech.h"            // CAT: cat_shipIsLeech (cat_spPrepareShipHyperspaceOut)
#include "RammingFrigate.h"   // CAT: cat_rammingFrigateForceRelease
#include "Salvage.h"          // CAT: cat_shipIsSalvager / cat_salvageDetach
#include "Clamp.h"            // CAT: unClampObj
#include "Dock.h"             // CAT: dockRemoveShipFromInside / dockPutShipOutside (the docked ships launched on arrival)
#include "Acolyte.h"          // CAT: cat_acolyteLinkReset
#include "ACV.h"              // CAT: cat_acvLinkReset
#include "avi.h"              // CAT: aviIsPlaying (the Bink player's binkDonePlaying 0x008b6d94)

// CAT: DEG_TO_RAD as the exe's float constant 0x0072a4f8 (0x3c8efa35)
#define CAT_SP_DEG_TO_RAD   0.01745329238474369f

#ifdef _MSC_VER
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif


// CAT: the exe's tests of the jumping fleet (inlined in CalculateRollCall 0x005176e0, UpdateHyperspacingShip
//      0x00517af0, OrientHyperspacingShips 0x00517f30, UpdateArrivingShip 0x00518170, singlePlayerGameCleanup
//      0x00519510, singlePlayerGameUpdate 0x005197d0): a ship the player may take along (cat_spShipCanHyperspace
//      0x00517650, which holds HW1's ally and disabled tests) that jumps by itself (a capital ship or a module, class
//      CLASS_Component) and is in range (specialFlags 0x80). HW1 tested ShipCanHyperspace (capital) and the range flag
#define catSpCapitalOrComponent(ship) (isCapitalShip(ship) || ((ship)->staticinfo->shipclass == CLASS_Component))
#define ShipCanMakeHyperspace(ship) (cat_spShipCanHyperspace(ship) && catSpCapitalOrComponent(ship) && \
                                     ((ship)->shipOrDerelict.specialFlags & SPECIAL_SinglePlayerWithinHyperSpaceRange))

//stores the current mission filename
char CurrentLevelName[256] = "";

// CAT (M2-R8 singleplayer-tutorial): no mission sequence: Cataclysm numbers its missions 1 .. 17 in
//      singlePlayerGameInfo.currentMission (singlePlayerInit 0x00518c50 starts at 1, singlePlayerLoadNewLevel 0x0051ac80
//      adds one); GoK's missionSequence / index (and spResetMissionSequenceToBeginning) are gone

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
bool32 spHoldHyperspaceWindow           = FALSE;
bool32 spBinkPlay                       = FALSE;
bool32 triggerNIS                       = FALSE;
bool32 spCollectResourcesAtEndOfMission = FALSE;

real32 spFleetModifier = 0.0f;

// CAT: the single-player difficulty's health factor for the Somtaaw ships (0x00894194, 1.0 in the exe image; set by
//      cat_spGetDifficultyModifiers 0x00516f90 from the difficulty 0..4 as 6, 3, 1, 0.85, 0.7 and read by
//      cat_univSetShipMaxHealth 0x0053c4a0 and universeStaticInit 0x0053d2a0); the difficulty (catSpDifficultySetting
//      0x008941a0, 2 in .data) is set on the single-player difficulty screen (cat_spGameDifficulty 0x0051c140)
real32 spDifficultyHealthModifier = 1.0f;
// CAT (M2-R6 oracle-mode-a): the factor FixShip 0x00500950 multiplies a loaded single-player Somtaaw ship's health by
//      (0x008941a4, 1.0 in the exe image; kasLoad 0x00491450 sets it to the current difficulty's factor over the
//      saved one's)
real32 spDifficultyHealthLoadFactor = 1.0f;

/*-----------------------------------------------------------------------------
    Name        : cat_spGetDifficultyModifiers                      0x00516f90
    Description : CAT: the single-player difficulty's modifiers: the Somtaaw ships' health factor (0..4: 6, 3, 1,
                  0.85, 0.7), the build-time and the build-cost divisors (4, 2, 1, 1, 1); a NULL output is skipped,
                  an out-of-range difficulty leaves the outputs unchanged
    Inputs      : difficulty - 0 (easiest) .. 4
    Outputs     : healthFactor (0x00894194), buildTimeDivisor (0x00894198), buildCostDivisor (0x0089419c)
----------------------------------------------------------------------------*/
void cat_spGetDifficultyModifiers(real32 *healthFactor, sdword *buildTimeDivisor, sdword *buildCostDivisor,
                                  sdword difficulty)
{
    static const real32 health[5] = { 6.0f, 3.0f, 1.0f, 0.85f, 0.7f };
    static const sdword divisor[5] = { 4, 2, 1, 1, 1 };

    if ((difficulty < 0) || (difficulty > 4))
    {
        return;
    }
    if (healthFactor != NULL)
    {
        *healthFactor = health[difficulty];
    }
    if (buildTimeDivisor != NULL)
    {
        *buildTimeDivisor = divisor[difficulty];
    }
    if (buildCostDivisor != NULL)
    {
        *buildCostDivisor = divisor[difficulty];
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_spGameDifficulty                              0x0051c140
    Description : CAT: the difficulty radio buttons of the single-player game screen ("SP_GameDifficulty"): on its
                  first call the button of the current difficulty is set (and the region redrawn), on a click the
                  button's pData is the difficulty; either way the modifiers follow it
----------------------------------------------------------------------------*/
void cat_spGameDifficulty(char *name, featom *atom)
{
    if ((atom != NULL) && FEFIRSTCALL(atom))
    {
        feRadioButtonSet(name, catSpDifficultySetting);
        cat_spGetDifficultyModifiers(&spDifficultyHealthModifier, &catCmSinglePlayerBuildFactor,
                                     &catCmSinglePlayerCostFactor, catSpDifficultySetting);
        bitSet(((regionhandle)atom->region)->status, RSF_DrawThisFrame);
        return;
    }
    catSpDifficultySetting = (udword)(size_t)atom->pData;
    cat_spGetDifficultyModifiers(&spDifficultyHealthModifier, &catCmSinglePlayerBuildFactor,
                                 &catCmSinglePlayerCostFactor, catSpDifficultySetting);
}
real32 spFleetStr      = 0.0f;

real32 BaseFleetStrengthForLevel    [NUMBER_SINGLEPLAYER_MISSIONS+1];
real32 AdvancedFleetStrengthForLevel[NUMBER_SINGLEPLAYER_MISSIONS+1];

sdword SINGLEPLAYER_DEBUGLEVEL  =   0;
sdword SINGLEPLAYER_STARTINGRUS = 200;

/*-----------------------------------------------------------------------------
    CAT (M2-R8 singleplayer-tutorial): the warp start (a debug start in a later mission with a given fleet). The exe
    reads Warp.script (the install's warp.script: "WarpEnabled 0") in singlePlayerInit 0x00518c50 through the table
    0x00894350: WarpEnabled (0x00a67490) and LevelToWarpTo (0x00a67494), then per "Level N" block (N 1 .. 19) the
    record's Resources, Ships "<type>, <count>" and Tech; cat_spWarpToLevel 0x0051b9c0 builds the fleet. HW1's warp
    (a "WarpTo N" first line and the mission's WarpFleet lines, WarpInFleet, singleHackResearchWarp) is gone.
-----------------------------------------------------------------------------*/
#define CAT_SP_WARP_LEVELS      19
typedef struct
{
    sdword resources;                       // +0x000 (0x00a67498 + level * 0x2F0)
    sdword ships[TOTAL_NUM_SHIPS];          // +0x004 the count of each ship type
    CatTechSet tech;                        // +0x248 the technologies held
} CatSpWarpLevel;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatSpWarpLevel) == 0x2F0, "a warp level record is 0x2F0 bytes in cataclysm.exe");
#endif
static CatSpWarpLevel catSpWarpLevels[CAT_SP_WARP_LEVELS];     // 0x00a67498
static bool32 catSpWarpEnabled = FALSE;                         // 0x00a67490
static sdword catSpWarpLevelTo = 0;                             // 0x00a67494
static sdword catSpWarpLevelParsing = -1;                       // 0x0089434c (-1 in .data): the record being read

static void cat_spWarpLevelSetCB(char *directory, char *field, void *dataToFillIn);
static void cat_spWarpResourcesSetCB(char *directory, char *field, void *dataToFillIn);
static void cat_spWarpShipsSetCB(char *directory, char *field, void *dataToFillIn);
static void cat_spWarpTechSetCB(char *directory, char *field, void *dataToFillIn);

static scriptEntry catSpWarpScriptTable[] =            // 0x00894350
{
    { "WarpEnabled",   scriptSetBool,            &catSpWarpEnabled },
    { "LevelToWarpTo", scriptSetUdwordCB,        &catSpWarpLevelTo },
    { "Level",         cat_spWarpLevelSetCB,     NULL },
    { "Resources",     cat_spWarpResourcesSetCB, NULL },
    { "Ships",         cat_spWarpShipsSetCB,     NULL },
    { "Tech",          cat_spWarpTechSetCB,      NULL },

    END_SCRIPT_ENTRY
};

// 0x00517060: "Level N" selects record N - 1
static void cat_spWarpLevelSetCB(char *directory, char *field, void *dataToFillIn)
{
    RemoveCommasFromString(field);
    sscanf(field, "%d", &catSpWarpLevelParsing);
    catSpWarpLevelParsing--;
    if ((catSpWarpLevelParsing < 0) || (catSpWarpLevelParsing >= CAT_SP_WARP_LEVELS))
    {
        dbgFatalf(DBG_Loc, "\nInvalid Level specified in warp script file: %s", field);
    }
}

// 0x00517130
static void cat_spWarpResourcesSetCB(char *directory, char *field, void *dataToFillIn)
{
    if (catSpWarpLevelParsing == -1)
    {
        dbgFatalf(DBG_Loc, "\nLevel not specified while parsing warp script file. (JS)");
    }
    RemoveCommasFromString(field);
    sscanf(field, "%d", &catSpWarpLevels[catSpWarpLevelParsing].resources);
}

// 0x005170b0
static void cat_spWarpShipsSetCB(char *directory, char *field, void *dataToFillIn)
{
    char shiptypestr[256];
    sdword count = 0;
    ShipType type;

    if (catSpWarpLevelParsing == -1)
    {
        dbgFatalf(DBG_Loc, "\nLevel not specified while parsing warp script file. (JS)");
    }
    RemoveCommasFromString(field);
    sscanf(field, "%s %d", shiptypestr, &count);
    type = StrToShipType(shiptypestr);
    // CAT-FIX: the exe stores at whatever index StrToShipType returns (-1 for an unknown name)
    if ((type >= 0) && (type < TOTAL_NUM_SHIPS))
    {
        catSpWarpLevels[catSpWarpLevelParsing].ships[type] = count;
    }
}

// 0x00517180
static void cat_spWarpTechSetCB(char *directory, char *field, void *dataToFillIn)
{
    char techstr[256];
    TechnologyType tech;

    if (catSpWarpLevelParsing == -1)
    {
        dbgFatalf(DBG_Loc, "\nLevel not specified while parsing warp script file. (JS)");
    }
    RemoveCommasFromString(field);
    sscanf(field, "%s", techstr);
    tech = StrToTechType(techstr);
    // CAT-FIX: as above, an unknown technology (-1) is not stored
    if (tech >= 0)
    {
        cat_rmTechSetAdd(catSpWarpLevels[catSpWarpLevelParsing].tech, tech);
    }
}

// CAT: 0x0051bd90: a single-player game with the warp start on (Warp.script's WarpEnabled). Callers: universeClosePlayers
//      / universeClose (the upgrades of player 0 survive a warp) and gameStart.
bool32 cat_spWarpToLevelEnabled(void)
{
    return (singlePlayerGame && catSpWarpEnabled);
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


static void singlePlayerKasMissionStart(MissionEnum missionnum);
static void cat_spLaunchDockedShipsOnArrival(Ship *host);
static void cat_spForgetDamageEffects(Ship *ship);

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
// 0x00894424: per mission, the jump goes straight to the next level in the letterbox (spHyperspaceButtonPushed: no
//      roll call; singlePlayerGameUpdate: the fleet leaves after 5 s) and the popup menu's hyperspace row says "next
//      level" (1), or the fleet hyperspaces through the roll call (0) (cat_spMissionFlagValue)
static const bool32 catSpMissionNextLevel[] = { 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0 };
// CAT: 0x00a6adb4: the real time at which a catSpMissionNextLevel jump's fleet leaves (5 s after the button);
//      0x00a67488: the game time at which the fleet's jump begins after Fleet Command's line (2.5 s). Names chosen
static real32 catSpWideScreenJumpTime = 0.0f;
static real32 catSpFleetCommandTime = 0.0f;
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
//      Mission01's nislet. Names chosen. mrRegionDraw 0x00569260 draws both over the main view (mainrgn.c)
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
static void cat_spEmergencyHyperspaceCB(char *string, featom *atom);

static void spShipsDockedDrawCB   (featom *atom, regionhandle region);
static void spShipsRemainingDrawCB(featom *atom, regionhandle region);

static void SetBaseFleetStrCB       (char *directory, char *field, void *dataToFillIn);
static void SetOnMissionCompleteInfo(char *directory, char *field, void *dataToFillIn);

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
    { cat_spEmergencyHyperspaceCB, "EmergencyHyperspace" },  // CAT: the exe's third button (table 0x008943f0)
    
    { NULL,                NULL              },
};

fedrawcallback spHyperspaceDrawCallback[] =
{
    { spShipsRemainingDrawCB, "ShipsRemainingDraw" },
    { spShipsDockedDrawCB,    "ShipsDockedDraw"    },
    
    { NULL,                   NULL                 },
};






void spSetCurrentMission(MissionEnum mission)
{
    singlePlayerGameInfo.currentMission = mission;          // CAT: no mission sequence index
}



MissionEnum spGetCurrentMission(void)
{
    return singlePlayerGameInfo.currentMission;
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



/*-----------------------------------------------------------------------------
    Name        : spAbortHyperspaceCB                               0x00517320
    Description : CAT: the roll call's Abort: the fleet is halted, the roll-call screen goes, the lockout is lifted and
                  the player may jump again (HW1 restored the auto-launch setting and kept the lockout). The exe
                  inlines the same body in singlePlayerGameUpdate 0x005197d0 when the mothership is lost.
----------------------------------------------------------------------------*/
void spAbortHyperspaceCB(char *string, featom *atom)
{
    SelectCommand *selectAll;

    if (singlePlayerGameInfo.hyperspaceState == HYPERSPACE_WAITINGROLLCALL)
    {
        // CAT: Fleet Command's abort line by its Cataclysm id (0x4001021; HW1 COMM_F_Hyper_Abort)
        speechEventQueue(NULL, 0x4001021, 0, -1, 0, universe.curPlayerIndex, -1, 5.0f, -1);
        singlePlayerGameInfo.hyperspaceState = NO_HYPERSPACE;

        selectAll = selectAllCurrentPlayersNonHyperspacingShips();
        clHalt(&universe.mainCommandLayer,selectAll);
        memFree(selectAll);

        feScreenDelete(spHyperspaceRollCallScreen);
        spHyperspaceRollCallScreen = NULL;
        spUnlockout();
        singlePlayerGameInfo.playerCanHyperspace = TRUE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : spGoNowHyperspaceCB                               0x005173a0
    Description : CAT: the roll call's Go Now (and the end of the roll call): the letterbox comes in, the small ships
                  dock at once, every jumping ship but the mothership halts, steadies and joins the parade (no door
                  cleaning: the exe has no mothership door; the test is the ship class, not the type)
----------------------------------------------------------------------------*/
void spGoNowHyperspaceCB(char *string, featom *atom)
{
    if (singlePlayerGameInfo.hyperspaceState == HYPERSPACE_WAITINGROLLCALL)
    {
        SelectCommand *selectAll;
        CommandToDo *militaryGroup;
        sdword i;
        Ship *ship;
        Ship *mothership = universe.curPlayerPtr->PlayerMothership;

        cat_kasWideScreenInHyperspace(90);                  // CAT: the letterbox (0x5a frames)

        selectAll = selectAllCurrentPlayersNonHyperspacingShips();
        clDock(&universe.mainCommandLayer,selectAll,CAT_DOCK_STAY_INSTANT,NULL);
        memFree(selectAll);

        selectAll = selectAllCurrentPlayersHyperspacingShips();
        for (i=0;i<selectAll->numShips;i++)
        {
            ship = selectAll->ShipPtr[i];
            if (ship->staticinfo->shipclass != CLASS_Mothership)
            {
                SelectCommand selectone;

                selectone.ShipPtr[0] = ship;
                selectone.numShips = 1;
                RemoveShipsFromDoingStuff(&universe.mainCommandLayer,&selectone,0x23);   // CAT: reason 0x23 as the exe (spGoNowHyperspaceCB 0x00517408) [audit: M2-R5 command-tactics]
                aitrackForceSteadyShip(ship);
                GroupShipIntoMilitaryParade(&universe.mainCommandLayer,ship,mothership);
                bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_SinglePlayerWithinHyperSpaceRange|SPECIAL_SinglePlayerInParade);
            }
        }
        memFree(selectAll);
        militaryGroup = GetMilitaryGroupAroundShip(&universe.mainCommandLayer,universe.curPlayerPtr->PlayerMothership);
        if (militaryGroup) setMilitaryParade(militaryGroup);

        feScreenDelete(spHyperspaceRollCallScreen);
        spHyperspaceRollCallScreen = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_spEmergencyHyperspaceCB                       0x005174b0
    Description : CAT: the roll call's Emergency Hyperspace (offered instead of Go Now while one of the player's
                  ships is under fire, spHyperspaceButtonPushed): the letterbox comes in, the ships that cannot
                  jump by themselves stop and are left behind (catFlags18 0x40, so the roll call no longer waits for
                  them; cat_spDeleteLeftBehindShips removes them with the level), the jumping ships but the mothership
                  halt and steady, flagged in range and in the parade, without being put into it
----------------------------------------------------------------------------*/
static void cat_spEmergencyHyperspaceCB(char *string, featom *atom)
{
    SelectCommand *selectAll;
    SelectCommand selectone;
    Ship *ship;
    sdword i;

    if (singlePlayerGameInfo.hyperspaceState == HYPERSPACE_WAITINGROLLCALL)
    {
        cat_kasWideScreenInHyperspace(90);

        selectAll = selectAllCurrentPlayersNonHyperspacingShips();
        for (i = 0; i < selectAll->numShips; i++)
        {
            ship = selectAll->ShipPtr[i];
            selectone.numShips = 1;
            selectone.ShipPtr[0] = ship;
            RemoveShipsFromDoingStuff(&universe.mainCommandLayer, &selectone, 0x2f);
            bitSet(ship->header.catFlags18, CAT_SHIPFLAG_LeftBehind);
        }
        memFree(selectAll);

        selectAll = selectAllCurrentPlayersHyperspacingShips();
        for (i = 0; i < selectAll->numShips; i++)
        {
            ship = selectAll->ShipPtr[i];
            if (ship->staticinfo->shipclass != CLASS_Mothership)
            {
                selectone.numShips = 1;
                selectone.ShipPtr[0] = ship;
                RemoveShipsFromDoingStuff(&universe.mainCommandLayer, &selectone, 0x23);
                aitrackForceSteadyShip(ship);
                bitSet(ship->shipOrDerelict.specialFlags,
                       SPECIAL_SinglePlayerWithinHyperSpaceRange | SPECIAL_SinglePlayerInParade);
            }
        }
        memFree(selectAll);

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

/*-----------------------------------------------------------------------------
    Name        : CalculateRollCall                                 0x005176e0
    Description : CAT: the roll call counts the ships the player may take (cat_spShipCanHyperspace): the ships docked
                  in any of them, every ship that does not jump by itself as remaining unless an emergency jump left
                  it behind (catFlags18 0x40), and the jumping ships but the mothership and the modules as docked
                  (in range) or remaining. HW1 counted every allied ship that was not a capital ship as remaining.
----------------------------------------------------------------------------*/
void CalculateRollCall(void)
{
    Node *objnode;
    Ship *ship;

    singlePlayerGameInfo.rollcallShipsDocked = 0;
    singlePlayerGameInfo.rollcallShipsRemaining = 0;

    for (objnode = universe.ShipList.head; objnode != NULL; objnode = objnode->next)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (!cat_spShipCanHyperspace(ship))
        {
            continue;
        }
        if (ship->shipsInsideMe)
        {
            // CAT: every ship inside counts, no DDDFrigate exemption (CalculateRollCall 0x005176e0, 0x00517707)
            singlePlayerGameInfo.rollcallShipsDocked += ship->shipsInsideMe->insideList.num;
        }
        if (!catSpCapitalOrComponent(ship))
        {
            if (!bitTest(ship->header.catFlags18, CAT_SHIPFLAG_LeftBehind))
            {
                singlePlayerGameInfo.rollcallShipsRemaining++;
            }
        }
        else if ((ship->staticinfo->shipclass != CLASS_Mothership) && (ship->staticinfo->shipclass != CLASS_Component))
        {
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
}





/*-----------------------------------------------------------------------------
    Name        : TellHyperspacingShipsToReady                      0x00517770
    Description : CAT: the mothership is in range; a tResourceCollector is healed to full (the exe's 0x58 test); the
                  other jumping ships are in range only when in the parade; the capital ships and modules launching
                  keep their order (HW1 tested capital ships only), the rest stop and join the parade
----------------------------------------------------------------------------*/
void TellHyperspacingShipsToReady(void)
{
    SelectCommand *selectAll;
    sdword i;
    Ship *ship;
    CommandToDo *shipcommand;

    selectAll = selectAllCurrentPlayersHyperspacingShips();
    for (i=0;i<selectAll->numShips;i++)
    {
        ship = selectAll->ShipPtr[i];
        if (ship->staticinfo->shipclass == CLASS_Mothership)
        {
            bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_SinglePlayerWithinHyperSpaceRange);
        }
        else
        {
            if (ship->shiptype == tResourceCollector)
            {
                ship->targetable.health = ship->staticinfo->health.maxhealth;
            }
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

    // CAT: HW1's MakeShipsNotIncludeLaunchingShips, inlined with the exe's capital-or-module test
    for (i=0;i<selectAll->numShips;)
    {
        ship = selectAll->ShipPtr[i];
        if (catSpCapitalOrComponent(ship))
        {
            shipcommand = getShipAndItsCommand(&universe.mainCommandLayer,ship);
            if ((shipcommand != NULL) && (shipcommand->ordertype.order == COMMAND_LAUNCH_SHIP))
            {
                selectAll->numShips--;
                selectAll->ShipPtr[i] = selectAll->ShipPtr[selectAll->numShips];
                continue;
            }
        }
        i++;
    }
    RemoveShipsFromDoingStuff(&universe.mainCommandLayer,selectAll,0x24);   // CAT: reason 0x24 as the exe (TellHyperspacingShipsToReady 0x0051783f) [audit: M2-R5 command-tactics]
    clSetMilitaryParade(&universe.mainCommandLayer,selectAll);
    memFree(selectAll);
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
        // CAT: the exe's x87 shape (0x00517a8d ..): the draw of stream 10 is loaded whole (fild of the unsigned
        //      32 bits), multiplied by RANDOM * 2^-32 and BASE added at the FPU's 53-bit precision, then stored once
        //      (HW1's frandom rounds the draw and each product to a float)
        shipSinglePlayerGameInfo->timeToDelayHyperspace = (real32)((real64)HYPERSPACE_RANDOM_WAITTIME *
            (1.0 / 4294967296.0) * (real64)gamerand() + (real64)HYPERSPACE_BASE_WAITTIME);
    }
    shipSinglePlayerGameInfo->timeToHyperspace = universe.totaltimeelapsed + shipSinglePlayerGameInfo->timeToDelayHyperspace;

    bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_Hyperspacing);
    shipHasJustDisappeared(ship);
}



/*-----------------------------------------------------------------------------
    Name        : AddShipToHyperspace                               0x00517a10
    Description : CAT: the ship enters the ether: its trails are cut (cat: trailZeroLength on all four) and in a
                  single-player game it, its modules and the ships inside it are healed and refuelled (HW1: the ship
                  and the ships inside)
----------------------------------------------------------------------------*/
void AddShipToHyperspace(Ship *ship)
{
    InsideShip *insideShip;
    Ship *module;
    sdword i;

    insideShip = memAlloc(sizeof(InsideShip),"InsideHyper",0);
    insideShip->ship = ship;
    listAddNode(&singlePlayerGameInfo.ShipsInHyperspace,&insideShip->node,insideShip);
    bitSet(ship->header.flags,SOF_Hyperspace);

    for (i = 0; i < MAX_NUM_TRAILS; i++)
    {
        if (ship->trail[i])
        {
            trailZeroLength(ship->trail[i]);
        }
    }

    if(singlePlayerGame)
    {
        // ships in hyperspace are instantly healed/refueled:
        //ONLY in single player game!
        ship->targetable.health = ship->staticinfo->health.maxhealth;
        ship->fuel = ship->staticinfo->maxfuel;
        for (module = cat_moduleIterFirst(ship); module != NULL; module = cat_moduleIterNext())
        {
            module->targetable.health = module->staticinfo->health.maxhealth;
            module->fuel = module->staticinfo->maxfuel;
        }
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

/*-----------------------------------------------------------------------------
    Name        : catSpFleetAnyInHsState                            (inlined)
    Description : CAT: TRUE when a ship of the jumping fleet (ShipCanMakeHyperspace) other than except is in the
                  hyperspace effect state (the exe inlines this walk of the ship list in UpdateHyperspacingShip
                  0x00517af0 and UpdateArrivingShip 0x00518170; HW1's NoneDoing / AllDoingExceptMe tested allied
                  capital ships)
----------------------------------------------------------------------------*/
static bool32 catSpFleetAnyInHsState(udword state, Ship *except)
{
    Node *objnode;
    Ship *ship;

    for (objnode = universe.ShipList.head; objnode != NULL; objnode = objnode->next)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        if ((ship != except) && ShipCanMakeHyperspace(ship) && (ship->shipSinglePlayerGameInfo->hsState == state))
        {
            return TRUE;
        }
    }
    return FALSE;
}

// CAT: the same over the ships in the ether
static bool32 catSpEtherAnyInHsState(udword state)
{
    Node *node;
    Ship *ship;

    for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = node->next)
    {
        ship = ((InsideShip *)listGetStructOfNode(node))->ship;
        if (ShipCanMakeHyperspace(ship) && (ship->shipSinglePlayerGameInfo->hsState == state))
        {
            return TRUE;
        }
    }
    return FALSE;
}

// CAT: the sound handle of a failed jump's mothership effect (0x0089469c, -1 in .data): the exe stops it but no code
//      of the exe starts it (HW1 started Ship_SinglePlayerHyperspace into it), so it stays -1
static sdword catSpFailedJumpSoundHandle = SOUND_NOTINITED;

/*-----------------------------------------------------------------------------
    Name        : UpdateHyperspacingShip                            0x00517af0
    Description : CAT: the exe's body: no hyperspace sounds (HW1 played Ship_SinglePlayerHyperspace / Ship_Hyperdrive),
                  the fleet tests are the exe's (catSpFleetAnyInHsState: the player's capital ships and modules in
                  range), the white-out starts with the mothership's collapse once the other ships are finished
----------------------------------------------------------------------------*/
bool32 UpdateHyperspacingShip(Ship *ship,bool32 midLevel)
{
    ShipSinglePlayerGameInfo *shipSinglePlayerGameInfo = ship->shipSinglePlayerGameInfo;
    bool32 whiteOutWasOff;

    ship->autostabilizeship = FALSE;        // ship is doing something, don't want to autostabilize
    ship->shipidle = FALSE;

    shipSinglePlayerGameInfo->midlevelHyperspaceOut = midLevel;

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
                    hsStart(ship, HYPERSPACE_SLICE_RATE, TRUE, hsShouldDisplayEffect(ship));
                }
                return FALSE;
            case HS_SLICING_INTO:
                if (!catSpFleetAnyInHsState(HS_POPUP_INTO, NULL) && !catSpFleetAnyInHsState(HS_INACTIVE, NULL))
                {
                    shipSinglePlayerGameInfo->hsState = HS_COLLAPSE_INTO;
                }
                return FALSE;
            case HS_COLLAPSE_INTO:
                if ((catSpFailedJumpSoundHandle != SOUND_NOTINITED) && (ship == universe.curPlayerPtr->PlayerMothership))
                {
                    soundEventStopSound(catSpFailedJumpSoundHandle, 1);
                    catSpFailedJumpSoundHandle = SOUND_NOTINITED;
                }
                hsUpdate(ship);
                return FALSE;
            case HS_FINISHED:
                return TRUE;
            default:
                hsUpdate(ship);
                return FALSE;
        }
    }

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
        }
        return FALSE;
    case HS_SLICING_INTO:
        if (!midLevel)
        {
            if (catSpFleetAnyInHsState(HS_POPUP_INTO, NULL) || catSpFleetAnyInHsState(HS_INACTIVE, NULL))
            {
                return FALSE;
            }
        }
        hsUpdate(ship);
        return FALSE;
    case HS_COLLAPSE_INTO:
        if (ship->soundevent.hyperspaceHandle != SOUND_NOTINITED)
        {
            soundEventStopSound(ship->soundevent.hyperspaceHandle, 1);
            ship->soundevent.hyperspaceHandle = SOUND_NOTINITED;
        }
        if ((!midLevel) && (ship == universe.curPlayerPtr->PlayerMothership))
        {
            // CAT: the exe clears the white-out first and sets it again once every other ship has finished
            //      (0x00517dd5 ..)
            whiteOutWasOff = !mrWhiteOut;
            mrWhiteOut = FALSE;
            if (whiteOutWasOff)
            {
                Node *objnode;
                Ship *other;

                for (objnode = universe.ShipList.head; objnode != NULL; objnode = objnode->next)
                {
                    other = (Ship *)listGetStructOfNode(objnode);
                    if (ShipCanMakeHyperspace(other) && (other != ship) &&
                        (other->shipSinglePlayerGameInfo->hsState != HS_FINISHED))
                    {
                        return FALSE;
                    }
                }
            }
            mrWhiteOut = TRUE;
            mrWhiteOutT = 1.0f - shipSinglePlayerGameInfo->clipt;
        }
        hsUpdate(ship);
        return FALSE;
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
        return TRUE;
    default:
        hsUpdate(ship);
        return FALSE;
    }
}



/*-----------------------------------------------------------------------------
    Name        : OrientHyperspacingShips                           0x00517f30
    Description : CAT: the player's capital ships and modules (cat_spShipCanHyperspace) stop idling (the mothership
                  excepted); the capital ships but the mothership turn to its heading once in range, else fly to it or
                  are flagged in range when near (a launching ship waits); the modules only stop idling
----------------------------------------------------------------------------*/
bool32 OrientHyperspacingShips(void)
{
    Node *objnode;
    Ship *ship;
    vector mothershipheading;
    bool32 shipsoriented = TRUE;
    bool32 shipready;
    Ship *mothership = universe.curPlayerPtr->PlayerMothership;
    CommandToDo *shipcommand;

    matGetVectFromMatrixCol3(mothershipheading,mothership->rotatable.rotinfo.coordsys);

    for (objnode = universe.ShipList.head; objnode != NULL; objnode = objnode->next)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (!cat_spShipCanHyperspace(ship) || !catSpCapitalOrComponent(ship))
        {
            continue;
        }
        if (ship->staticinfo->shipclass != CLASS_Mothership)
        {
            ship->autostabilizeship = FALSE;        // ship is doing something, don't want to autostabilize
            ship->shipidle = FALSE;
        }
        if ((ship->staticinfo->shipclass == CLASS_Mothership) || (ship->staticinfo->shipclass == CLASS_Component))
        {
            continue;
        }
        if (ship->shipOrDerelict.specialFlags & SPECIAL_SinglePlayerWithinHyperSpaceRange)
        {
            shipready = aitrackHeadingWithBank(ship,&mothershipheading,HYPERSPACE_HEADINGACCURACY,ship->staticinfo->shipOrDerelict.sinbank);
            if(!shipready)
            {
                singlePlayerGameInfo.rollcallShipsRemaining++;  //add to count because we really aren't ready
            }
            shipsoriented &= shipready;
        }
        else if (aiuFindDistanceSquared(ship->obj.posinfo.position,mothership->obj.posinfo.position) < HYPERSPACE_RANGE)
        {
            shipcommand = getShipAndItsCommand(&universe.mainCommandLayer,ship);
            if ((shipcommand == NULL) || (shipcommand->ordertype.order != COMMAND_LAUNCH_SHIP))
            {
                bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_SinglePlayerWithinHyperSpaceRange);
            }
        }
        else
        {
            aishipFlyToShipAvoidingObjs(ship,mothership,AISHIP_PointInDirectionFlying|AISHIP_FirstPointInDirectionFlying|
                                                        AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured,0.0f);
        }
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



/*-----------------------------------------------------------------------------
    Name        : UpdateArrivingShip                                0x00518170
    Description : CAT: the exe's body: in a multiplayer game the arrival point is clipped to the universe; the ship
                  waits hidden (SOF_Hide) until it decelerates in; the mothership and a module whose host is itself
                  arrive first (2 x BASE), the others after 3 x BASE + a random wait; the camera's motion cues restart
                  with the mothership; no Ship_HyperdriveOff sound; the white-out runs down at 3 slice rates a tick; the
                  arrived ship leaves the range flag too; the fleet tests are the exe's (ShipCanMakeHyperspace, a
                  multiplayer module waits on its host's telegraph)
----------------------------------------------------------------------------*/
bool32 UpdateArrivingShip(Ship *ship,hvector *topoint,bool32 midLevel)
{
    ShipSinglePlayerGameInfo *shipSinglePlayerGameInfo = ship->shipSinglePlayerGameInfo;

    dbgAssertOrIgnore(shipSinglePlayerGameInfo);

    ship->autostabilizeship = FALSE;        // ship is doing something, don't want to autostabilize
    ship->shipidle = FALSE;

    switch (shipSinglePlayerGameInfo->shipHyperspaceState)
    {
        case SHIPHYPERSPACE_GONE:
            {
                vector tempvec;
                vector createposition;
                vector createvelocity;
                real64 angle64;
                real32 angle;
                matrix rotzmat;

                // calculate position based on forbehind, rightleft
                vecSet(tempvec,shipSinglePlayerGameInfo->forbehind,-shipSinglePlayerGameInfo->rightleft,
                       shipSinglePlayerGameInfo->updown);
                vecSet(createvelocity,0.0f,0.0f,0.0f);

                // CAT: x87 (0x00518455 ..): the angle is w times the float DEG_TO_RAD at 53 bits, the sine of that
                //      product, the cosine of the angle stored as a float
                angle64 = (real64)topoint->w * (real64)CAT_SP_DEG_TO_RAD;
                angle = (real32)angle64;
                matMakeRotAboutZ(&rotzmat,(real32)cos((real64)angle),(real32)sin(angle64));
                matMultiplyMatByVec(&createposition,&rotzmat,&tempvec);

                vecAddTo(createposition,*topoint);
                if (!singlePlayerGame)
                {
                    // CAT: a multiplayer arrival stays inside the universe (0x005184d2 ..)
                    if (createposition.x > smUniverseSizeX) createposition.x = smUniverseSizeX;
                    if (createposition.x < -smUniverseSizeX) createposition.x = -smUniverseSizeX;
                    if (createposition.y > smUniverseSizeY) createposition.y = smUniverseSizeY;
                    if (createposition.y < -smUniverseSizeY) createposition.y = -smUniverseSizeY;
                    if (createposition.z > smUniverseSizeZ) createposition.z = smUniverseSizeZ;
                    if (createposition.z < -smUniverseSizeZ) createposition.z = -smUniverseSizeZ;
                }

                PutShipOutsideAt(ship,&createposition,&createvelocity,angle);
                bitClear(ship->header.flags,SOF_Hyperspace);

                shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_DECELERATE;
                if (midLevel)
                {
                    shipSinglePlayerGameInfo->timeToHyperspace = 0.0f;
                }
                else
                {
                    real64 waittime;

                    if ((ship == universe.curPlayerPtr->PlayerMothership) ||
                        ((ship->guidance.clampInfo != NULL) && (ship->guidance.clampInfo->host == (SpaceObjRotImpTarg *)ship)))
                    {
                        waittime = (real64)HYPERSPACE_BASE_WAITTIME + (real64)HYPERSPACE_BASE_WAITTIME;
                    }
                    else
                    {
                        // CAT: x87 (0x005185cc ..): RANDOM * 2^-32 times the whole draw, plus BASE * 3, one rounding
                        waittime = (real64)HYPERSPACE_RANDOM_WAITTIME * (1.0 / 4294967296.0) *
                                   (real64)gamerand() + (real64)HYPERSPACE_BASE_WAITTIME * 3.0;
                    }
                    shipSinglePlayerGameInfo->timeToHyperspace = (real32)((real64)universe.totaltimeelapsed + waittime);
                }

                if ((ship == universe.curPlayerPtr->PlayerMothership) && (!midLevel))
                {
                    ccFocusOnPlayersMothership(&universe.mainCameraCommand,0);
                    if (catMotionCues)
                    {
                        cat_mcResetRequest(&catMotionCueState);     // CAT: the motion cues restart (0x00518639)
                    }
                }
                bitSet(ship->header.flags, SOF_Hide);       // CAT: hidden until it decelerates in (0x00518649)
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
                bitClear(ship->header.flags, SOF_Hide);     // CAT: shown (0x00518693)
                hsStart(ship, HYPERSPACE_SLICE_RATE, FALSE, hsShouldDisplayEffect(ship));
                // CAT: no Ship_HyperdriveOff sound
            }
            break;

        case SHIPHYPERSPACE_DONE:
            // CAT: x87 (0x005181b5): the rate times 3 at 53 bits, subtracted and stored once
            mrWhiteOutT = (real32)((real64)mrWhiteOutT - (real64)HYPERSPACE_SLICE_RATE * 3.0);
            if (mrWhiteOutT < 0.0f)
            {
                mrWhiteOut = FALSE;
            }
            switch (shipSinglePlayerGameInfo->hsState)
            {
                case HS_SLICING_OUTOF:
                    if (!midLevel && (catSpEtherAnyInHsState(HS_POPUP_OUTOF) || catSpEtherAnyInHsState(HS_FINISHED)))
                    {
                        break;
                    }
                    if(!singlePlayerGame)
                    {
                        // CAT: a module waits on its host's command (0x00518391 ..)
                        Ship *commandShip = ship;
                        CommandToDo *command;

                        if ((ship->staticinfo->shipclass == CLASS_Component) && (ship->guidance.clampInfo != NULL) &&
                            (ship->guidance.clampInfo->host != NULL))
                        {
                            commandShip = (Ship *)ship->guidance.clampInfo->host;
                        }
                        command = getShipAndItsCommand(&universe.mainCommandLayer,commandShip);
                        if ((command != NULL) && (command->ordertype.order == COMMAND_MP_HYPERSPACING) &&
                            (universe.totaltimeelapsed - command->hyperSpaceingTime < TW_HYPERSPACE_TELEGRAPH_WAIT_TIME))
                        {
                            //ship hasn't waited long enough for incomming hs...
                            break;
                        }
                    }
                    hsUpdate(ship);
                    break;

                case HS_COLLAPSE_OUTOF:
                    if (ship->soundevent.hyperspaceHandle != SOUND_NOTINITED)
                    {
                        soundEventStopSound(ship->soundevent.hyperspaceHandle, 1);
                        ship->soundevent.hyperspaceHandle = SOUND_NOTINITED;
                    }
                    if ((!midLevel) && (ship == universe.curPlayerPtr->PlayerMothership))
                    {
                        // CAT: the mothership waits until every other ship of the fleet is out of its effect
                        Node *objnode;
                        Ship *other;

                        for (objnode = universe.ShipList.head; objnode != NULL; objnode = objnode->next)
                        {
                            other = (Ship *)listGetStructOfNode(objnode);
                            if (ShipCanMakeHyperspace(other) && (other != ship) &&
                                (other->shipSinglePlayerGameInfo->hsState != HS_INACTIVE))
                            {
                                return FALSE;
                            }
                        }
                        hsUpdate(ship);
                    }
                    else
                    {
                        if(!singlePlayerGame && gravwellIsShipStuckForHyperspaceing(ship))
                        {
                            //ship is stuck here because of a gravwell gen somewhere
                            break;
                        }
                        hsUpdate(ship);
                    }
                    break;

                case HS_INACTIVE:
                    if (universe.totaltimeelapsed > shipSinglePlayerGameInfo->timeToHyperspace)
                    {
                        // CAT: the range flag goes with the hyperspacing one (0x0051840c: & 0xffdfff7f)
                        bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_Hyperspacing|SPECIAL_SinglePlayerWithinHyperSpaceRange);
                        return TRUE;
                    }
                    break;

                default:
                    hsUpdate(ship);
                    break;
            }
            break;

        default:
            break;
    }

    return FALSE;
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

/*-----------------------------------------------------------------------------
    Name        : cat_spPrepareShipHyperspaceOut                    0x00518880
    Description : CAT: before a ship hyperspaces out mid-level, every object clamped to it lets go: a ramming
                  frigate releases, a leech stops (RemoveShipsFromDoingStuff 0x25), a salvager detaches, anything but
                  a module is unclamped. The walk restarts when the clamped list changed size, as the exe's does
----------------------------------------------------------------------------*/
static void cat_spPrepareShipHyperspaceOut(Ship *ship)
{
    ClampInfo *clampInfo = ship->guidance.clampInfo;
    sdword index, count;
    Ship *obj;
    SelectCommand selectone;

    if ((clampInfo == NULL) || (clampInfo->clampedObjs == NULL))
    {
        return;
    }
    index = 0;
    for (;;)
    {
        count = clampInfo->clampedObjs->numShips;
        if (index >= count)
        {
            return;
        }
        obj = clampInfo->clampedObjs->ShipPtr[index];
        cat_rammingFrigateForceRelease(obj);
        if (cat_shipIsLeech(obj))
        {
            selectone.numShips = 1;
            selectone.ShipPtr[0] = obj;
            RemoveShipsFromDoingStuff(&universe.mainCommandLayer, &selectone, 0x25);
        }
        if (cat_shipIsSalvager(obj))
        {
            cat_salvageDetach(obj);
        }
        if (obj->staticinfo->shipclass != CLASS_Component)
        {
            unClampObj((SpaceObjRotImpTargGuidance *)obj);
        }
        clampInfo = ship->guidance.clampInfo;
        if (clampInfo == NULL)
        {
            return;
        }
        // CAT-FIX: the exe reads the list through the clamp info again without testing it (it is freed with the last
        //      object); this tree stops when the list is gone
        if (clampInfo->clampedObjs == NULL)
        {
            return;
        }
        index = (count == clampInfo->clampedObjs->numShips) ? (index + 1) : 0;
    }
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
                // CAT (M2-R9 missions-10-17): the exe inlines InstantlyHyperspaceOut and clears the ship's
                //      midlevelHyperspaceOut when it leaves (0x0051899e .. 0x005189d3)
                bool32 leaves = ((ship->header.flags & SOF_Hyperspace) == 0);

                InstantlyHyperspaceOut(ship,defactoMothership);
                if (leaves)
                {
                    ship->shipSinglePlayerGameInfo->midlevelHyperspaceOut = FALSE;
                }
                bitClear(ship->obj.attributes, ATTRIBUTES_StartInHS);
            }
            else
            {
                cat_spPrepareShipHyperspaceOut(ship);      // CAT: what is clamped to it lets go (0x005189e4)
                CalculateHyperspaceTimeForShip(ship,defactoMothership);
                ship->shipSinglePlayerGameInfo->timeToHyperspace += spHyperspaceDelay;
                // CAT (M2-R9 missions-10-17): the clamp's host is tested too (0x00518a05 .. 0x00518a13: the clamp info,
                //      then its host, non-NULL): a ship whose clamp lost its host (Mission14's Beast mothership,
                //      kasfTeamHyperspaceOut) read a NULL host here
                if ((ship->guidance.clampInfo != NULL) && (ship->guidance.clampInfo->host != NULL))
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

/*-----------------------------------------------------------------------------
    Name        : singlePlayerInit                                  0x00518c50
    Description : CAT: the warp records are cleared and Warp.script read (the warp start's mission, else mission 1:
                  no mission sequence); the auto-launch setting to restore defaults to every class (0xF)
----------------------------------------------------------------------------*/
void singlePlayerInit(void)
{
    memset(catSpWarpLevels, 0, sizeof(catSpWarpLevels));
    catSpWarpEnabled = FALSE;
    catSpWarpLevelTo = 0;
    scriptSet(NULL, "Warp.script", catSpWarpScriptTable);
#if HWI_ORACLE
    // HWI-TEST (M2-R9 missions-02-09): /catMissionTest with /catMissionTestMission N (2 .. 17) starts mission N through
    //      the exe's warp start, as a copy of Warp.script saying "WarpEnabled 1" and "LevelToWarpTo N" would (the
    //      install's per-level records: the fleet, resources and technologies Relic set for each mission)
    if (singlePlayerGame && (catMissionSelfTestWarpLevel() > 0))
    {
        catSpWarpEnabled = TRUE;
        catSpWarpLevelTo = catMissionSelfTestWarpLevel();
    }
#endif
#if HWI_ORACLE
    // HWI-TEST (M2-R9 missions-10-17): the warp start's self-test (/catWarpStartTest) reads what Warp.script said
    catWarpStartSelfTestScript(catSpWarpEnabled, catSpWarpLevelTo);
#endif
    if (catSpWarpEnabled)
    {
        singlePlayerGameInfo.currentMission = catSpWarpLevelTo;
    }
    else if (singlePlayerGame)
    {
        singlePlayerGameInfo.currentMission = MISSION_1_KHARAK_SYSTEM;
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
    singlePlayerGameInfo.oldAutoLaunch = 0xf;               // CAT: 0x00518d2d

    HYPERSPACE_FARCLIPPLANESQR = HYPERSPACE_FARCLIPPLANE * HYPERSPACE_FARCLIPPLANE;

    HYPERSPACE_DECELERATE_DIST = 0.5f * HYPERSPACE_DECELERATION * HYPERSPACE_DECELERATE_TIME*HYPERSPACE_DECELERATE_TIME;
    HYPERSPACE_DECELERATE_INITIAL_VEL = HYPERSPACE_DECELERATION * HYPERSPACE_DECELERATE_TIME;
    HYPERSPACE_WHIPTOWARDSDIST = HYPERSPACE_FARCLIPPLANE - HYPERSPACE_DECELERATE_DIST;
}

static udword spLockoutFlags = 0;

/*-----------------------------------------------------------------------------
    Name        : spMainScreen                                      0x00518d80
    Description : CAT: the managers and menus close; the sensors manager too when closeSensors (every caller passes
                  1 but kasfOpenSensors 0x00496bc0, which opens it next). No task bar to force down
----------------------------------------------------------------------------*/
void spMainScreen(bool32 closeSensors)
{
    if (closeSensors && smSensorsActive)
    {
        smSensorsCloseForGood();
    }
    cmCloseIfOpen();
    rmCloseIfOpen();
    lmCloseIfOpen();
    feAllMenusDelete();                                     //make sure no menus stay up during NIS
}

/*-----------------------------------------------------------------------------
    Name        : spLockout                                         0x00518db0
    Description : CAT: a lockout in force is lifted first; the deselect empties the selection (the ship totals
                  refreshed), the main-region lockout also hides the mouse cursor. The exe inlines this body in
                  spMainScreenAndLockout, cat_spMainScreenAndLockoutKeepSensors, spHyperspaceButtonPushed and
                  singlePlayerGameUpdate
----------------------------------------------------------------------------*/
void spLockout(udword flags)
{
    if (spLockoutFlags != 0)
    {
        spUnlockout();
    }
    spLockoutFlags = flags;
    if (flags & SPLOCKOUT_DESELECT)
    {
        selSelected.numShips = 0;
        ioUpdateShipTotals();
    }

    smSensorsDisable = TRUE;
    // CAT: no task bar lockout (the exe never writes tbDisable 0x00a6b254; spLockout 0x00518db0)
    if (flags & SPLOCKOUT_MR)
    {
        mrDisable();
        mouseCursorHide();
    }
    if (flags & SPLOCKOUT_MOUSE)
    {
        mouseDisable();
    }
}

void spMainScreenAndLockout(udword flags)
{
    spMainScreen(TRUE);
    spLockout(flags);
}

/*-----------------------------------------------------------------------------
    Name        : cat_spMainScreenAndLockoutKeepSensors             0x00518e90
    Description : CAT (M2-R8 kasf library): spMainScreenAndLockout without closing the sensors manager (the KAS
                  letterbox's finish, cat_kasWideScreenInFinish 0x004973d0): the managers and menus close and the
                  lockout is set. The exe's body is spMainScreenAndLockout 0x00518e10's without its first test;
                  this tree's spLockout does the lockout part (SinglePlayer.c's port aligns both, M2-R8 producer 4)
----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    Name        : cat_spMainScreenAndLockoutKeepSensors             0x00518e90
    Description : CAT (M2-R8 kasf library): spMainScreenAndLockout without closing the sensors manager (the KAS
                  letterbox's finish, cat_kasWideScreenInFinish 0x004973d0)
----------------------------------------------------------------------------*/
void cat_spMainScreenAndLockoutKeepSensors(udword flags)
{
    spMainScreen(FALSE);
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

/*-----------------------------------------------------------------------------
    Name        : spUnlockout                                       0x00518f00
    Description : CAT: the sensors manager is enabled again and its 0x00a5b1b0 word cleared; the main region and the
                  mouse cursor come back; the flags are cleared whole
----------------------------------------------------------------------------*/
void spUnlockout(void)
{
    smSensorsDisable = FALSE;
    smFleetIntel = FALSE;                                   // CAT: 0x00a5b1b0 (spUnlockout 0x00518f10)
    // CAT: no task bar lockout (the exe never writes tbDisable 0x00a6b254; spUnlockout 0x00518f00)
    if (spLockoutFlags & SPLOCKOUT_MR)
    {
        mrEnable();
        mouseCursorShow();
    }
    if (spLockoutFlags & SPLOCKOUT_MOUSE)
    {
        mouseEnable();
    }
    spLockoutFlags = 0;
}

/*-----------------------------------------------------------------------------
    Name        : spHyperspaceButtonPushed                          0x00518f50
    Description : CAT: the exe's body: not while the Pause key's frame step is on; refused (a red command message)
                  while a ship of the player's is still under construction; the hyperspace sensors status off; the
                  missions of catSpMissionNextLevel (0x00894424) jump in the letterbox at once (5 s of real
                  time); else the fleet docks, the jumping ships ready, the screens close, the player is locked out
                  and the roll-call screen comes up with either Go Now (no ship of player 0 under fire) or Emergency
                  Hyperspace disabled. GoK's end-of-mission resource collection (spCollectResourcesAtEndOfMission)
                  is gone
----------------------------------------------------------------------------*/
void spHyperspaceButtonPushed(void)
{
    SelectCommand *selectAll;
    fescreen *screen;
    featom *atom;
    char *disabledButton;
    Node *node;
    Ship *ship;

    if ((!singlePlayerGameInfo.playerCanHyperspace && !hyperspaceOverride) || catUnivFrameStep)
    {
        return;
    }
    if (cat_univPlayerHasFlaggedShip())
    {
        clCommandMessage(strGetString(strShipsStillUnderConstruction), 0xff0000ff);
        return;
    }

    hyperspaceOverride = FALSE;
    singlePlayerGameInfo.playerCanHyperspace = FALSE;
    smUpdateHyperspaceStatus(FALSE);

    if ((singlePlayerGameInfo.hyperspaceState != NO_HYPERSPACE) || (universe.curPlayerPtr->PlayerMothership == NULL))
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

    if (catSpMissionNextLevel[singlePlayerGameInfo.currentMission])
    {
        cat_kasWideScreenInHyperspace(90);
        catSpWideScreenJumpTime = universe.realtimeelapsed + 5.0f;     // 0x00a6adb4 (0x0072bcb4 = 5.0)
        return;
    }

    // HW1's TellShipsToDockForHyperspace, inlined (CAT: Fleet Command's line 0x200101f; no ship-type preparation)
    selectAll = selectAllCurrentPlayersNonHyperspacingShips();
    clCancelAllLaunchOrdersFromPlayer(universe.curPlayerPtr);
    if (clDock(&universe.mainCommandLayer,selectAll,CAT_DOCK_STAY,NULL) > 0)
    {
        speechEventQueue(NULL, 0x200101f, 0, -1, 0, -1, -1, 5.0f, -1);
    }
    memFree(selectAll);

    TellHyperspacingShipsToReady();
    spMainScreenAndLockout(SPLOCKOUT_MR);

    // HW1's HyperspaceRollCallBegin, inlined
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
    spHyperspaceRollCallScreen = feScreenStart(ghMainRegion, "HyperspaceRollCall");

    // CAT: Go Now is disabled while a ship of player 0 was recently attacked or fired upon, else Emergency
    //      Hyperspace (0x005190f2 ..)
    screen = feScreenFind("HyperspaceRollCall");
    if (screen != NULL)
    {
        disabledButton = "EmergencyHyperspace";
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            ship = (Ship *)listGetStructOfNode(node);
            if ((ship->playerowner->playerIndex == 0) && (ship->recentlyAttacked || ship->recentlyFiredUpon))
            {
                disabledButton = "GoNowHyperspace";
                break;
            }
        }
        atom = feAtomFindInScreen(screen, disabledButton);
        if (atom != NULL)
        {
            bitSet(((regionhandle)atom->region)->status, RSF_RegionDisabled);
        }
    }
}

void spTaskBarHyperspaceCB(struct taskbutton *button, ubyte *userData)
{
    spHyperspaceButtonPushed();
}

void singlePlayerPostInit(bool32 loadingSaveGame)
{
    // CAT: Animatic01 before Mission01 (singlePlayerPostInit 0x005193d0: animBinkPlay(0, 1))
    if (!loadingSaveGame && (singlePlayerGameInfo.currentMission == MISSION_1_KHARAK_SYSTEM))
    {
        animBinkPlay(0, 1);
    }
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
    // CAT: Fleet Command's actor flag is left alone (singlePlayerClose 0x005193f0)
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
    singlePlayerGameInfo.onLoadPlayNIS = 0;
    singlePlayerGameInfo.onCompletePlayNIS = 0;
    singlePlayerGameInfo.resourceUnitsCollected = 0;
    // CAT: the player is the Somtaaw (race 0) and the enemy race 1 before the script's line
    //      (singlePlayerSetMissionAttributes 0x00519440; HW1 picked the race the player's was not)
    universe.players[0].race = R1;
    singlePlayerGameInfo.onCompleteHyperspace = TRUE;     // default to TRUE
    universe.players[1].race = R2;
    scriptSet(directory,filename,SinglePlayerScriptTable);

    //reset nebulae
    nebReset();
}









/*-----------------------------------------------------------------------------
    Name        : catSpEtherCleanup                                 (inlined)
    Description : CAT: the ships in the ether leave its list (all of them, or the current player's allies): one still
                  gone into it is freed when freeShips, any other returns to no hyperspace and out of range; after a
                  failed jump the fleet's ships too (singlePlayerGameCleanup 0x00519510 and singlePlayerGameUpdate
                  0x005197d0 inline these walks; HW1's CleanupHyperspacingShips)
----------------------------------------------------------------------------*/
static void catSpEtherCleanup(bool32 freeShips, bool32 allPlayers)
{
    Node *node, *nextnode;
    Ship *ship;

    for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = nextnode)
    {
        ship = ((InsideShip *)listGetStructOfNode(node))->ship;
        nextnode = node->next;
        if (allPlayers || allianceIsShipAlly(ship, universe.curPlayerPtr))
        {
            listDeleteNode(node);     // put before so univFreeShipContents doesn't try to delete this twice
            if (freeShips && (ship->shipSinglePlayerGameInfo->shipHyperspaceState == SHIPHYPERSPACE_GONE))
            {
                univFreeShipContents(ship);
                memFree(ship);
            }
            else
            {
                ship->shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_NONE;
                bitClear(ship->shipOrDerelict.specialFlags, SPECIAL_SinglePlayerWithinHyperSpaceRange);
            }
        }
    }
    if (singlePlayerGameInfo.hyperspaceFails)
    {
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            ship = (Ship *)listGetStructOfNode(node);
            if (ShipCanMakeHyperspace(ship))
            {
                ship->shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_NONE;
                bitClear(ship->shipOrDerelict.specialFlags, SPECIAL_SinglePlayerWithinHyperSpaceRange);
            }
        }
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

/*-----------------------------------------------------------------------------
    Name        : singlePlayerGameCleanup                           0x00519510
    Description : CAT: the objectives are destroyed (objectiveDestroyAll), the ether cleaned with the exe's tests
                  (the ships returning leave the range flag); after a jump the roll call goes, the camera's mode flag 7
                  clears, the white-out, the black fade and the big-gun permission reset
----------------------------------------------------------------------------*/
void singlePlayerGameCleanup(void)
{
    if (!singlePlayerGame)
    {
        catSpEtherCleanup(TRUE, TRUE);
        return;
    }

    objectiveDestroyAll();

    // remove ship lists, paths, volumes, etc.
    kasLabelledEntitiesDestroy();

    pingAllPingsDelete();

    if (singlePlayerGameLoadNewLevelFlag)
    {
        return;
    }

    spUnlockout();

    catSpEtherCleanup(TRUE, TRUE);

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
    catSpLevelFade = 0.0f;                                  // CAT: 0x00a6ac70
    catSpBigGunAllowed = TRUE;                              // CAT: 0x00894108
}

/*-----------------------------------------------------------------------------
    Name        : UpdateMidLevelHyperspacingShips                   0x00519700
    Description : CAT: a ship that has hyperspaced out mid-level loses the references other ships hold to it
                  (cat_univRemoveObjReferencesFromShips); the ships coming in mid-level arrive and leave the ether
                  and the range flag. HW1 also wiped the ships flagged to be deleted after a jump here; the exe does it
                  at the end of the player's jump (singlePlayerGameUpdate's white-out)
----------------------------------------------------------------------------*/
void UpdateMidLevelHyperspacingShips(void)
{
    sdword i;
    Ship *ship;
    Node *node;
    Node *nextnode;

    // update any mid-level hyperspacing away ships
    for (i=0;i<singlePlayerGameInfo.ShipsToHyperspace.selection->numShips; )
    {
        ship = singlePlayerGameInfo.ShipsToHyperspace.selection->ShipPtr[i];
        if (UpdateHyperspacingShip(ship,TRUE))
        {
            cat_univRemoveObjReferencesFromShips((SpaceObj *)ship);
            growSelectRemoveShipIndex(&singlePlayerGameInfo.ShipsToHyperspace,i);   // deliberate don't increment i
        }
        else
        {
            i++;
        }
    }

    // update any mid-level hyperspacing in ships
    for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = nextnode)
    {
        ship = ((InsideShip *)listGetStructOfNode(node))->ship;
        nextnode = node->next;

        if (ship->shipSinglePlayerGameInfo->midlevelHyperspaceIn &&
            UpdateArrivingShip(ship,&ship->shipSinglePlayerGameInfo->midlevelHyperspaceTo,TRUE))
        {
            ship->shipSinglePlayerGameInfo->midlevelHyperspaceIn = FALSE;
            ship->shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_NONE;
            bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_SinglePlayerWithinHyperSpaceRange);
            listDeleteNode(node);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_spLaunchDockedShipsOnArrival                  0x005191a0
    Description : CAT: when a fleet is put at the start point at once (the missions of catSpMissionArrivesAtOnce), a
                  ship's docked ships come out (unless it is a hive frigate, or auto-launch was off), fully upgraded,
                  unlinked and healed, out of range, in the parade around it; in Mission04 also its dEcclesiams
----------------------------------------------------------------------------*/
static void cat_spLaunchDockedShipsOnArrival(Ship *host)
{
    vector zero = { 0.0f, 0.0f, 0.0f };
    SelectCommand hostSelection, shipSelection;
    GrowSelection hostGrow, shipGrow;
    Node *node;
    Ship *ship;

    hostSelection.numShips = 1;
    hostSelection.ShipPtr[0] = host;
    hostGrow.selection = &hostSelection;
    shipGrow.selection = &shipSelection;

    if ((host->shipsInsideMe != NULL) && (host->shiptype != sHiveFrigate) && (host->shiptype != bHiveFrigate) &&
        (singlePlayerGameInfo.oldAutoLaunch & 1))
    {
        while ((node = host->shipsInsideMe->insideList.head) != NULL)
        {
            ship = ((InsideShip *)listGetStructOfNode(node))->ship;
            shipSelection.numShips = 1;
            shipSelection.ShipPtr[0] = ship;
            dockRemoveShipFromInside(ship, host);
            dockPutShipOutside(ship, host, &zero, 0, 1);
            if (ship->staticinfo->UpgradeByDocking)
            {
                cat_shipApplyUpgrades(ship);
            }
            cat_acolyteLinkReset(ship);
            cat_acvLinkReset(ship);
            ship->targetable.health = ship->staticinfo->health.maxhealth;
            bitClear(ship->shipOrDerelict.specialFlags, SPECIAL_SinglePlayerWithinHyperSpaceRange);
            kasfForceParadeFormation(&hostGrow, &shipGrow);
        }
    }
    if (singlePlayerGame && (singlePlayerGameInfo.currentMission == 4) && (host->shipsInsideMe != NULL))
    {
        node = host->shipsInsideMe->insideList.head;
        while (node != NULL)
        {
            ship = ((InsideShip *)listGetStructOfNode(node))->ship;
            if (ship->shiptype != dEcclesiam)
            {
                node = node->next;
                continue;
            }
            shipSelection.numShips = 1;
            shipSelection.ShipPtr[0] = ship;
            dockRemoveShipFromInside(ship, host);
            dockPutShipOutside(ship, host, &zero, 0, 1);
            if (ship->staticinfo->UpgradeByDocking)
            {
                cat_shipApplyUpgrades(ship);
            }
            cat_acolyteLinkReset(ship);
            cat_acvLinkReset(ship);
            bitClear(ship->shipOrDerelict.specialFlags, SPECIAL_SinglePlayerWithinHyperSpaceRange);
            ship->targetable.health = ship->staticinfo->health.maxhealth;
            kasfForceParadeFormation(&hostGrow, &shipGrow);
            node = host->shipsInsideMe->insideList.head;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_spForgetDamageEffects                         0x00519490
    Description : CAT: a ship in the ether and every ship inside it, recursively, forget their damage effects (the
                  end of the player's jump; HW1's CleanupDamageEffects did the ships in the ether only)
----------------------------------------------------------------------------*/
static void cat_spForgetDamageEffects(Ship *ship)
{
    Node *node;

    if (ship->shipsInsideMe != NULL)
    {
        for (node = ship->shipsInsideMe->insideList.head; node != NULL; node = node->next)
        {
            cat_spForgetDamageEffects(((InsideShip *)listGetStructOfNode(node))->ship);
        }
    }
    dmgForgetEffects(ship);
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

/*-----------------------------------------------------------------------------
    Name        : singlePlayerGameUpdate                            0x005197d0
    Description : CAT: the exe's body. The NIS and nislet requests of the KAS scripts, the traders' screen; the jump:
                  a catSpMissionNextLevel mission's fleet leaves into the ether 5 s after the button (the fade comes
                  in meanwhile), else the roll call, Fleet Command's line, the turn and the jump (the ships left in
                  the ether from before are wiped); the white-out plays the animatic (cur, cur + 1; mission 17 ends
                  the game with the credits), then, once no movie plays, the ether's ships forget their damage
                  effects, the ships flagged to be deleted after the jump go, and the next level loads (or its
                  opening NIS plays in its staging level: state HYPERSPACE_FAILED, no case); the level's KAS script
                  starts (HYPERSPACE_LOADDONE) and waits (HYPERSPACE_SHIPSARRIVING) for StartLevelByHyperspacingShips,
                  then the fleet arrives (HYPERSPACE_CATSHIPSHYPERSPACINGIN: flown in, or put at the start point at
                  once for the catSpMissionArrivesAtOnce missions, which wait for the black fade); a failed jump
                  returns through case HYPERSPACE_CATFAILEDRETURN (reached only by falling through, as the exe's 10)
----------------------------------------------------------------------------*/
void singlePlayerGameUpdate(void)
{
    // CAT: a KAS script asks for an NIS or a nislet through the globals G_PlayNis / G_PlayNislet (the hashed variable
    //      store; HW1 read the AIVars "PlayNis" / "PlayNislet") (singlePlayerGameUpdate 0x005197d0)
    CatKasVar *nisVariable = cat_kasVarFind(0, cat_kasHashString("G_PlayNis"));
    CatKasVar *nisletVariable = cat_kasVarFind(0, cat_kasHashString("G_PlayNislet"));
    CatKasVar *tgui;
    sdword value;
    // CAT: the nislet's file names (0x00a67028 / 0x00a670c8)
    static char nisletFile[160];
    static char nisletScript[160];
    Node *node, *nextnode;
    Ship *ship;
    SelectCommand selectone;
    hvector startpoint;
    bool32 allDone;

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
                cat_trPrecacheShipTextures();           //      (0x00519a96, cat_trPrecacheShipTextures 0x00589910)
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
            spAbortHyperspaceCB(NULL,NULL);                 // CAT: inlined by the exe (0x00519ac1 ..)
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
            if (catSpMissionNextLevel[singlePlayerGameInfo.currentMission])
            {
                // CAT: the letterbox jump (0x00519bc0 ..): the fade comes in over the 5 s, then the ships left in the
                //      ether are wiped, the small ships dock at once and every ship of the fleet goes into the ether
                if (universe.realtimeelapsed <= catSpWideScreenJumpTime)
                {
                    catSpLevelFade = 1.0f - (catSpWideScreenJumpTime - universe.realtimeelapsed) / 5.0f;
                    return;
                }
                for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = nextnode)
                {
                    ship = ((InsideShip *)listGetStructOfNode(node))->ship;
                    nextnode = node->next;
                    ship->howDidIDie = DEATH_Left_Behind_At_Hyperspace;
                    univDeleteWipeInsideShipOutOfExistence(ship);
                }
                {
                    SelectCommand *selectAll = selectAllCurrentPlayersNonHyperspacingShips();

                    clDock(&universe.mainCommandLayer, selectAll, CAT_DOCK_STAY_INSTANT, NULL);
                    memFree(selectAll);
                }
                for (node = universe.ShipList.head; node != NULL; node = nextnode)
                {
                    ship = (Ship *)listGetStructOfNode(node);
                    nextnode = node->next;
                    // CAT: the exe sets the range flag on every ship of the list first (0x00519c5c)
                    bitSet(ship->shipOrDerelict.specialFlags, SPECIAL_SinglePlayerWithinHyperSpaceRange);
                    if (ShipCanMakeHyperspace(ship))
                    {
                        selectone.numShips = 1;
                        selectone.ShipPtr[0] = ship;
                        RemoveShipsFromDoingStuff(&universe.mainCommandLayer, &selectone, 0x27);
                        aitrackForceSteadyShip(ship);
                        if (ship != universe.curPlayerPtr->PlayerMothership)
                        {
                            GroupShipIntoMilitaryParade(&universe.mainCommandLayer, ship,
                                                        universe.curPlayerPtr->PlayerMothership);
                            bitSet(ship->shipOrDerelict.specialFlags, SPECIAL_SinglePlayerInParade);
                        }
                        clPresetShipsToPosition(&universe.mainCommandLayer);
                        CalculateHyperspaceTimeForShip(ship, ship->playerowner->PlayerMothership);
                        ship->shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_GONE;
                        if ((ship->header.flags & SOF_Hyperspace) == 0)
                        {
                            univRemoveShipFromOutside(ship, FALSE);     // CAT: the exe's 0 (inlined InstantlyHyperspaceOut)
                            AddShipToHyperspace(ship);
                            ship->shipSinglePlayerGameInfo->midlevelHyperspaceOut = FALSE;
                        }
                    }
                }
                singlePlayerGameInfo.hyperspaceState = HYPERSPACE_WHITEOUT;
                singlePlayerGameInfo.hyperspaceTimeStamp = (real32)((real64)HYPERSPACE_WHITEOUTTIME +
                    (real64)universe.totaltimeelapsed + (real64)SPEECH_HYPERSPACE_DELAY);
                mrWhiteOut = TRUE;
                mrWhiteOutT = 1.0f;
                spBinkPlay = TRUE;
                speechEventStopAll(0.5f);
                return;
            }
            CalculateRollCall();
            OrientHyperspacingShips();
            if (singlePlayerGameInfo.rollcallShipsRemaining == 0)
            {
                if (spHyperspaceRollCallScreen != NULL)
                {
                    spGoNowHyperspaceCB(NULL, NULL);        // CAT: the roll call ends as Go Now does (0x00519d64)
                    spHyperspaceRollCallScreen = NULL;
                }
                singlePlayerGameInfo.hyperspaceState = HYPERSPACE_WAITINGFORFLEETCOMMAND;
                singlePlayerGameInfo.hyperspaceSubState = FOCUSING_ON_MOTHERSHIP;
                singlePlayerGameInfo.hyperspaceTimeStamp = universe.totaltimeelapsed;
                // CAT: Fleet Command's line by its Cataclysm id (0x2001020; HW1 STAT_F_Hyper_TakingOff)
                speechEventQueue(NULL, 0x2001020, 0, -1, 0, universe.curPlayerIndex, -1, 5.0f, -1);
                catSpFleetCommandTime = universe.totaltimeelapsed + SPEECH_HYPERSPACE_DELAY;
                return;
            }
            break;

        case HYPERSPACE_WAITINGFORFLEETCOMMAND:
            if (universe.totaltimeelapsed >= catSpFleetCommandTime)
            {
                singlePlayerGameInfo.hyperspaceState = HYPERSPACE_SHIPSLEAVING;
                return;
            }
            break;

        case HYPERSPACE_SHIPSLEAVING:
            if (singlePlayerGameInfo.hyperspaceSubState == FOCUSING_ON_MOTHERSHIP)
            {
                bool32 shipsoriented = OrientHyperspacingShips();
                Ship *mothership;

                if ((universe.totaltimeelapsed < singlePlayerGameInfo.hyperspaceTimeStamp) || !shipsoriented)
                {
                    return;
                }
                mothership = universe.curPlayerPtr->PlayerMothership;
                for (node = universe.ShipList.head; node != NULL; node = node->next)
                {
                    ship = (Ship *)listGetStructOfNode(node);
                    if (ShipCanMakeHyperspace(ship))
                    {
                        CalculateHyperspaceTimeForShip(ship,mothership);
                    }
                }
                singlePlayerGameInfo.hyperspaceSubState = LOCKED_FOCUS_FOR_HOP;
                // CAT: no "Game_BeginHyperspace" tutorial message and no Ship_SinglePlayerHyperspace sound
                if (!singlePlayerGameInfo.hyperspaceFails)
                {
                    soundEventStopTrack(musicEventCurrentTrack(), 1.5f);
                }
                // CAT: the ships still in the ether from before are wiped (0x00519e9b ..)
                for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = nextnode)
                {
                    ship = ((InsideShip *)listGetStructOfNode(node))->ship;
                    nextnode = node->next;
                    ship->howDidIDie = DEATH_Left_Behind_At_Hyperspace;
                    univDeleteWipeInsideShipOutOfExistence(ship);
                }
                return;
            }
            if (singlePlayerGameInfo.hyperspaceSubState != LOCKED_FOCUS_FOR_HOP)
            {
                return;
            }
            // CAT: HW1's UpdateHyperspacingShips, inlined with the exe's test: done when no ship of the fleet is left
            //      outside (or, in a failed jump, when each one's effect has finished)
            allDone = TRUE;
            for (node = universe.ShipList.head; node != NULL; node = nextnode)
            {
                ship = (Ship *)listGetStructOfNode(node);
                nextnode = node->next;
                if (ShipCanMakeHyperspace(ship))
                {
                    bool32 result = UpdateHyperspacingShip(ship,FALSE);

                    if (!singlePlayerGameInfo.hyperspaceFails || !result)
                    {
                        allDone = FALSE;
                    }
                }
            }
            if (!allDone)
            {
                return;
            }
            if (!singlePlayerGameInfo.hyperspaceFails)
            {
                // all ships have hyperspaced out.
                singlePlayerGameInfo.hyperspaceTimeStamp = universe.totaltimeelapsed + HYPERSPACE_WHITEOUTTIME;
                spBinkPlay = TRUE;
                singlePlayerGameInfo.hyperspaceState = HYPERSPACE_WHITEOUT;
                speechEventStopAll(0.5f);
                // screen should fade out
                mrWhiteOut = TRUE;
                mrWhiteOutT = 1.0f;
                return;
            }
            // CAT: a failed jump falls through into the exe's case 10
        case HYPERSPACE_CATFAILEDRETURN:
            singlePlayerGameInfo.hyperspaceState = NO_HYPERSPACE;
            catSpEtherCleanup(FALSE, FALSE);
            spUnlockout();
            //launch all necessary ships first
            LaunchAllInternalShipsOfPlayerThatMustBeLaunched(universe.curPlayerPtr);
            clAutoLaunch(singlePlayerGameInfo.oldAutoLaunch,universe.curPlayerIndex);
            tutGameMessage("Game_HyperspaceFailed");
            singlePlayerGameInfo.hyperspaceFails = FALSE;
            break;

        case HYPERSPACE_WHITEOUT:
            if (universe.totaltimeelapsed < singlePlayerGameInfo.hyperspaceTimeStamp)
            {
                break;
            }
            if (spBinkPlay)
            {
                // playback level transition animatic
                catSpLevelFade = 0.0f;
                animBinkPlay(singlePlayerGameInfo.currentMission, singlePlayerGameInfo.currentMission + 1);
                if (singlePlayerGameInfo.currentMission == MISSION_17)
                {
                    // CAT: Cataclysm's last mission ends the game with the credits (0x0051a10d)
                    universe.quittime = universe.realtimeelapsed;
                    utyCreditsSequence = TRUE;
                }
                spBinkPlay = FALSE;
                mrWhiteOut = FALSE;
                catSpLevelFade = 1.0f;
                return;
            }
            if (aviIsPlaying)                               // CAT: binkDonePlaying 0x008b6d94
            {
                break;
            }
            spBinkPlay = FALSE;
            // clear damage effects from ships (and the ships inside them: cat_spForgetDamageEffects)
            for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = node->next)
            {
                cat_spForgetDamageEffects(((InsideShip *)listGetStructOfNode(node))->ship);
            }
            // CAT: the ships flagged to be deleted after the jump (attribute 0x40) go, the ones inside a ship are
            //      taken out of it (0x0051a1a5 ..)
            for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = nextnode)
            {
                ship = ((InsideShip *)listGetStructOfNode(node))->ship;
                nextnode = node->next;
                if (ship->obj.attributes & ATTRIBUTES_DeleteAfterHSOut)
                {
                    univDeleteWipeInsideShipOutOfExistence(ship);
                }
                if (ship->shipsInsideMe != NULL)
                {
                    Node *inside = ship->shipsInsideMe->insideList.head;

                    while (inside != NULL)
                    {
                        Ship *iship = ((InsideShip *)listGetStructOfNode(inside))->ship;

                        inside = inside->next;
                        if (iship->obj.attributes & ATTRIBUTES_DeleteAfterHSOut)
                        {
                            dockRemoveShipFromInside(iship, ship);
                            inside = ship->shipsInsideMe->insideList.head;
                        }
                    }
                }
            }
            if (!catSpMissionStartsInNis[singlePlayerGameInfo.currentMission + 1])
            {
                // rebuild level now
                catSpLevelFromNisStop = FALSE;
                singlePlayerGameLoadNewLevelFlag = TRUE;
                catSpFleetArriving = TRUE;                  // CAT: 0x00a6ad78
                singlePlayerGameInfo.hyperspaceState = HYPERSPACE_LOADDONE;
                catSpLevelFade = 1.0f;
                return;
            }
            // CAT: the next mission is entered from its opening NIS, played in its staging level (0x0051a2a6 ..):
            //      the state waits (no case) until singlePlayerNisStoppedCB loads the mission
            singlePlayerNisNumber = catSpMissionNis[singlePlayerGameInfo.currentMission + 1];
            singlePlayerNISName = catSpNisNames[singlePlayerNisNumber][0];
            singlePlayerStartNis(catSpNisNames[singlePlayerNisNumber][0], catSpNisNames[singlePlayerNisNumber][1],
                                 NULL, TRUE, FALSE);
            singlePlayerGameInfo.hyperspaceState = HYPERSPACE_FAILED;
            catSpLevelFade = 1.0f;
            return;

        case HYPERSPACE_LOADDONE:
            // CAT: the level the fleet arrives in starts its KAS script here, before the fleet arrives (the mission
            //      flow's entry, singlePlayerGameUpdate 0x005197d0: kasMissionStart("missionNN") for missions 1 .. 17;
            //      HW1 started it once every ship had arrived); the screens close and the player is locked out
            singlePlayerGameInfo.hyperspaceState = HYPERSPACE_SHIPSARRIVING;
            singlePlayerKasMissionStart(spGetCurrentMission());
            nisFullyScissored = TRUE;                               // 0x00a4cb58
            spMainScreenAndLockout(SPLOCKOUT_MR);
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
                else if (!catSpArriveAtOnce)
                {
                    // CAT: the fleet is out; the arrival ends with the black fade
                    allArrived = (catSpLevelFade <= 0.0f);
                }
                else
                {
                    // CAT: the fleet is put at the start point at once (0x00519f90 ..), each ship at its offset from
                    //      the mothership turned by the start point's angle, out of the ether and of hyperspace
                    catSpArriveAtOnce = FALSE;
                    for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = nextnode)
                    {
                        ship = ((InsideShip *)listGetStructOfNode(node))->ship;
                        nextnode = node->next;
                        if (allianceIsShipAlly(ship, universe.curPlayerPtr))
                        {
                            ShipSinglePlayerGameInfo *ssinfo = ship->shipSinglePlayerGameInfo;
                            vector offset, position, zero = { 0.0f, 0.0f, 0.0f };
                            matrix rotz;
                            real64 angle64;
                            real32 angle;

                            catSpStartPoint(&startpoint);
                            vecSet(offset, ssinfo->forbehind, -ssinfo->rightleft, ssinfo->updown);
                            angle64 = (real64)startpoint.w * (real64)CAT_SP_DEG_TO_RAD;
                            angle = (real32)angle64;
                            matMakeRotAboutZ(&rotz, (real32)cos((real64)angle), (real32)sin(angle64));
                            matMultiplyMatByVec(&position, &rotz, &offset);
                            position.x += startpoint.x;
                            position.y += startpoint.y;
                            position.z += startpoint.z;
                            PutShipOutsideAt(ship, &position, &zero, angle);
                            bitClear(ship->header.flags, SOF_Hyperspace);
                            bitClear(ship->shipOrDerelict.specialFlags,
                                     SPECIAL_Hyperspacing | SPECIAL_SinglePlayerWithinHyperSpaceRange);
                            ssinfo->shipHyperspaceState = SHIPHYPERSPACE_NONE;
                            // CAT-FIX: the exe deletes the list's head, which is this node unless a ship that is not
                            //      the player's ally came first
                            listDeleteNode(node);
                        }
                    }
                    for (node = universe.ShipList.head; node != NULL; node = node->next)
                    {
                        ship = (Ship *)listGetStructOfNode(node);
                        if ((ship->playerowner == &universe.players[0]) &&
                            ((ship->shiptype == dEcclesiam) || !ship->shipOrDerelict.catDisabled))
                        {
                            selectone.numShips = 1;
                            selectone.ShipPtr[0] = ship;
                            RemoveShipsFromDoingStuff(&universe.mainCommandLayer, &selectone, 0x26);
                            aitrackForceSteadyShip(ship);
                            if (ship != universe.players[0].PlayerMothership)
                            {
                                GroupShipIntoMilitaryParade(&universe.mainCommandLayer, ship,
                                                            universe.players[0].PlayerMothership);
                                bitSet(ship->shipOrDerelict.specialFlags, SPECIAL_SinglePlayerInParade);
                            }
                            if (ship->shipsInsideMe != NULL)
                            {
                                cat_spLaunchDockedShipsOnArrival(ship);
                            }
                        }
                    }
                    clPresetShipsToPosition(&universe.mainCommandLayer);
                    ccFocusOnPlayersMothership(&universe.mainCameraCommand,0);
                    if (catMotionCues)
                    {
                        cat_mcResetRequest(&catMotionCueState);
                    }
                    catSpLevelFadeRestart = TRUE;
                    catSpLevelFade = 1.0f;
                    allArrived = FALSE;
                }

                if (allArrived)
                {
                    // the scripts learn it through G_ShipsHaveArrived; the fleet leaves the ether's list
                    cat_kasVarFindOrCreate(0, cat_kasHashString("G_ShipsHaveArrived"))->value = 1;
                    catSpFleetArriving = FALSE;
                    singlePlayerGameInfo.hyperspaceState = NO_HYPERSPACE;
                    catSpEtherCleanup(FALSE, FALSE);
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

        default:
            // HYPERSPACE_FAILED (the next mission's opening NIS plays) and NO_HYPERSPACE: no case in the exe
            break;
    }
}

static void SetOnMissionCompleteInfo(char *directory,char *field,void *dataToFillIn)
{
    char str[30];
    SinglePlayerGameInfo *info = (SinglePlayerGameInfo *)dataToFillIn;

    RemoveCommasFromString(field);

    sscanf(field,"%d %s",&info->onCompletePlayNIS,str);

    if (strcasecmp(str,"NOHYPERSPACE") == 0)          // CAT: _stricmp 0x00676c90 (SetOnMissionCompleteInfo 0x0051ab50)
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
// CAT (M2-R9 missions-02-09): Mission02 .. Mission09 generated (tools/m2_kas2c_gen.py DEFAULT_SCRIPTS)
// CAT (M2-R9 missions-10-17): Mission10 .. Mission17 generated: every script of the exe's table has its code
static const CatKasScript catKasScripts[CAT_KAS_SCRIPTS] =
{
    { "mission01", Init_Mission01, Watch_Mission01, Mission01_Functions, &Mission01_FunctionCount, CAT_MISSION01_WATCH_EXE },
    { "mission02", Init_Mission02, Watch_Mission02, Mission02_Functions, &Mission02_FunctionCount, CAT_MISSION02_WATCH_EXE },
    { "mission03", Init_Mission03, Watch_Mission03, Mission03_Functions, &Mission03_FunctionCount, CAT_MISSION03_WATCH_EXE },
    { "mission04", Init_Mission04, Watch_Mission04, Mission04_Functions, &Mission04_FunctionCount, CAT_MISSION04_WATCH_EXE },
    { "mission05", Init_Mission05, Watch_Mission05, Mission05_Functions, &Mission05_FunctionCount, CAT_MISSION05_WATCH_EXE },
    { "mission06", Init_Mission06, Watch_Mission06, Mission06_Functions, &Mission06_FunctionCount, CAT_MISSION06_WATCH_EXE },
    { "mission07", Init_Mission07, Watch_Mission07, Mission07_Functions, &Mission07_FunctionCount, CAT_MISSION07_WATCH_EXE },
    { "mission08", Init_Mission08, Watch_Mission08, Mission08_Functions, &Mission08_FunctionCount, CAT_MISSION08_WATCH_EXE },
    { "mission09", Init_Mission09, Watch_Mission09, Mission09_Functions, &Mission09_FunctionCount, CAT_MISSION09_WATCH_EXE },
    { "mission10", Init_Mission10, Watch_Mission10, Mission10_Functions, &Mission10_FunctionCount, CAT_MISSION10_WATCH_EXE },
    { "mission11", Init_Mission11, Watch_Mission11, Mission11_Functions, &Mission11_FunctionCount, CAT_MISSION11_WATCH_EXE },
    { "mission12", Init_Mission12, Watch_Mission12, Mission12_Functions, &Mission12_FunctionCount, CAT_MISSION12_WATCH_EXE },
    { "mission13", Init_Mission13, Watch_Mission13, Mission13_Functions, &Mission13_FunctionCount, CAT_MISSION13_WATCH_EXE },
    { "mission14", Init_Mission14, Watch_Mission14, Mission14_Functions, &Mission14_FunctionCount, CAT_MISSION14_WATCH_EXE },
    { "mission15", Init_Mission15, Watch_Mission15, Mission15_Functions, &Mission15_FunctionCount, CAT_MISSION15_WATCH_EXE },
    { "mission16", Init_Mission16, Watch_Mission16, Mission16_Functions, &Mission16_FunctionCount, CAT_MISSION16_WATCH_EXE },
    { "mission17", Init_Mission17, Watch_Mission17, Mission17_Functions, &Mission17_FunctionCount, CAT_MISSION17_WATCH_EXE },
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
        // [audit: M2-R9 missions-10-17: the comment said "not generated yet (CAT-TODO above)"; that CAT-TODO is closed
        //      and every row of catKasScripts has its generated code, so this return is never taken]
        return;
    }
    kasMissionStart(script->name, script->init, script->watch);
}

// HWI (M2-R8 singleplayer-tutorial): the mission's kas2c script is generated (M2-R9 part B generates missions 02 .. 17);
//      render.c's GoK debugScreenshots mode moves to the next mission only then
bool32 cat_spKasScriptGenerated(MissionEnum mission)
{
    return (mission >= MISSION_1_KHARAK_SYSTEM) && (mission <= MISSION_17) && (catKasScripts[mission - 1].init != NULL);
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











/*-----------------------------------------------------------------------------
    Name        : cat_spWarpToLevel                                 0x0051b9c0
    Description : CAT: the warp start (Warp.script's WarpEnabled): the mission is LevelToWarpTo; an sMothership and
                  the record's ships are created for player 0 at the origin in the parade around it (the first eight
                  sCarriers kept); a module is clamped to the mothership, a carrier's support module
                  (sCarrierSupport) to the first carrier with a support dock point free; a support module takes its
                  host's next DPT_Support_EB point (turned round on every second one) and the research module types are
                  activated; then player 0 gets the record's resources and technologies, every ship of the player goes
                  into the ether, and the level loads with the fleet hyperspacing in (singlePlayerLoadNewLevel(0, 1)
                  in HYPERSPACE_LOADDONE)
----------------------------------------------------------------------------*/
static void cat_spWarpToLevel(void)
{
    CatSpWarpLevel *record;
    Ship *carriers[8];
    sdword numCarriers = 0;
    vector origin = { 0.0f, 0.0f, 0.0f };
    Ship *mothership, *ship, *host;
    ShipType type;
    sdword count, i, first, index;
    DockStaticInfo *dockinfo;
    matrix rotation, oldCoordsys;
    vector xyz;
    Node *node;

    memset(carriers, 0, sizeof(carriers));
    singlePlayerGameInfo.currentMission = catSpWarpLevelTo;
    record = &catSpWarpLevels[catSpWarpLevelTo - 1];

    mothership = univAddShip(sMothership, universe.players[0].race, &origin, &universe.players[0], 0);
    gameStatsAddShip(mothership, 0);
    unitCapCreateShip(mothership, &universe.players[0]);
    universe.players[0].PlayerMothership = mothership;

    for (type = 0; type < TOTAL_NUM_SHIPS; type++)
    {
        for (count = 0; count < record->ships[type]; count++)
        {
            ship = univAddShip(type, universe.players[0].race, &origin, &universe.players[0], 0);
            gameStatsAddShip(ship, 0);
            unitCapCreateShip(ship, &universe.players[0]);
            GroupShipIntoMilitaryParade(&universe.mainCommandLayer, ship, universe.players[0].PlayerMothership);
            if ((ship->shiptype == sCarrier) && (numCarriers < 8))
            {
                carriers[numCarriers++] = ship;
            }
            if (ship->staticinfo->shipclass != CLASS_Component)
            {
                continue;
            }
            host = universe.players[0].PlayerMothership;
            if (ship->shiptype == sCarrierSupport)
            {
                for (i = 0; i < numCarriers; i++)
                {
                    dockinfo = carriers[i]->staticinfo->dockStaticInfo;
                    if (carriers[i]->dockInfo->catNumBusyOfType[DPT_Support_EB] < (sbyte)dockinfo->numOfType[DPT_Support_EB])
                    {
                        break;
                    }
                }
                // (the exe calls an empty debug function 0x006fe9a0 when every carrier is full, then takes the first)
                host = (i < numCarriers) ? carriers[i] : ((numCarriers > 0) ? carriers[0] : host);
            }
            if ((ship->shiptype == sCarrierSupport) || (ship->shiptype == sMothershipSupport))
            {
                dockinfo = host->staticinfo->dockStaticInfo;
                first = (sbyte)dockinfo->firstOfType[DPT_Support_EB];
                if (first != -1)
                {
                    index = first;
                    if (host->dockInfo->catNumBusyOfType[DPT_Support_EB] < (sbyte)dockinfo->numOfType[DPT_Support_EB])
                    {
                        index = host->dockInfo->catNumBusyOfType[DPT_Support_EB] + first;
                        ship->dockvars.dockship = host;
                        dockReserveDockPoint(ship, host, index);
                    }
                    matMultiplyMatByVec(&xyz, &host->rotatable.rotinfo.coordsys,
                                        &dockinfo->dockstaticpoints[index].position);
                    vecAdd(ship->obj.posinfo.position, host->obj.posinfo.position, xyz);
                    if ((index - first) & 1)
                    {
                        matMakeRotAboutX(&rotation, (real32)cos(PI), (real32)sin(PI));
                        oldCoordsys = ship->rotatable.rotinfo.coordsys;
                        matMultiplyMatByMat(&ship->rotatable.rotinfo.coordsys, &oldCoordsys, &rotation);
                    }
                }
            }
            clampObjToObj((SpaceObjRotImpTargGuidance *)ship, (SpaceObjRotImpTargGuidance *)host);
            cat_rmResearchModuleActivated(&universe.players[0], type, FALSE);
        }
    }

    clPresetShipsToPosition(&universe.mainCommandLayer);
    universeSwitchToPlayer(0);
    universe.players[0].resourceUnits = record->resources;
    cat_rmTechSetOr(universe.players[0].researchinfo.HasTechnology, universe.players[0].researchinfo.HasTechnology,
                    record->tech);
    cat_playerUpdateUpgradesFromResearch(&universe.players[0]);

    node = universe.ShipList.head;
    while (node != NULL)
    {
        Node *nextnode = node->next;
        bool32 leaves;

        ship = (Ship *)listGetStructOfNode(node);
        if (ship->playerowner != universe.curPlayerPtr)
        {
            node = nextnode;
            continue;
        }
        CalculateHyperspaceTimeForShip(ship, universe.curPlayerPtr->PlayerMothership);
        ship->shipSinglePlayerGameInfo->shipHyperspaceState = SHIPHYPERSPACE_GONE;
        leaves = ((ship->header.flags & SOF_Hyperspace) == 0);
        if (leaves)
        {
            univRemoveShipFromOutside(ship, FALSE);
            AddShipToHyperspace(ship);
            ship->shipSinglePlayerGameInfo->midlevelHyperspaceOut = FALSE;
        }
        cat_shipApplyUpgrades(ship);
        // CAT-FIX: the exe walks from the list's head again (the ship has left it); a ship of the player already in
        //      the ether (not in the list) would make it loop forever, so then the walk goes on with the next ship
        node = leaves ? universe.ShipList.head : nextnode;
    }
    catSpLevelFromNisStop = FALSE;                          // 0x00a6ac74
#if HWI_ORACLE
    // HWI-TEST (M2-R9 missions-10-17): the warp start's self-test checks the record against the fleet in the ether
    if (catWarpStartSelfTestRequested())
    {
        catWarpStartSelfTestApplied(cat_spWarpStartSelfTestCheck(FALSE));
    }
#endif
    singlePlayerGameInfo.hyperspaceState = HYPERSPACE_LOADDONE;
    singlePlayerLoadNewLevel(0, 1);
}

#if HWI_ORACLE
/*-----------------------------------------------------------------------------
    HWI-TEST (M2-R9 missions-10-17): the warp start's self-test (/catWarpStartTest with /catMissionTest, harness mode
    warpstart: a test copy of the install's warp.script with WarpEnabled 1 and LevelToWarpTo N in the run's own data
    dir). cat_spWarpStartSelfTestCheck compares the record of level N with player 0: the count of every ship type the
    record lists (plus the sMothership cat_spWarpToLevel adds; a type made by its host, the hive frigates' swarmers or
    the mothership's own modules, is reported, not judged) and, after the warp (arrived FALSE), every ship in the ether
    (singlePlayerGameInfo.ShipsInHyperspace), the resource units equal to the record's, the record's technologies held;
    after the fleet's arrival (arrived TRUE) every ship out of the ether (the universe's ship list and the ships
    inside them). "warpstarttest: ..." lines; TRUE when everything matches.
----------------------------------------------------------------------------*/
static void cat_spWarpStartCount(Ship *ship, sdword *counts, sdword *all)
{
    Node *node;

    if (bitTest(ship->header.flags, SOF_Dead) || (ship->shiptype < 0) || (ship->shiptype >= TOTAL_NUM_SHIPS))
    {
        return;
    }
    counts[ship->shiptype]++;
    (*all)++;
    if (ship->shipsInsideMe != NULL)
    {
        for (node = ship->shipsInsideMe->insideList.head; node != NULL; node = node->next)
        {
            cat_spWarpStartCount(((InsideShip *)listGetStructOfNode(node))->ship, counts, all);
        }
    }
}

bool32 cat_spWarpStartSelfTestCheck(bool32 arrived)
{
    CatSpWarpLevel *record;
    sdword counts[TOTAL_NUM_SHIPS];
    sdword expected, types = 0, typesOk = 0, ships = 0, shipsExpected = 0, techs = 0, techsOk = 0, ether = 0, outside = 0;
    sdword all = 0;
    TechnologyType tech;
    ShipType type;
    Node *node;
    Ship *ship;
    bool32 resourcesOk, pass;

    if (!catSpWarpEnabled || (catSpWarpLevelTo < 1) || (catSpWarpLevelTo > CAT_SP_WARP_LEVELS))
    {
        dbgMessagef("warpstarttest: %s no warp start (WarpEnabled=%d LevelToWarpTo=%d)", arrived ? "arrived" : "applied",
                    (int)catSpWarpEnabled, (int)catSpWarpLevelTo);
        return FALSE;
    }
    record = &catSpWarpLevels[catSpWarpLevelTo - 1];
    memset(counts, 0, sizeof(counts));
    for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = node->next)
    {
        ship = ((InsideShip *)listGetStructOfNode(node))->ship;
        if (ship->playerowner == &universe.players[0])
        {
            ether++;
            if (!arrived)
            {
                cat_spWarpStartCount(ship, counts, &all);
            }
        }
    }
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (ship->playerowner == &universe.players[0])
        {
            outside++;
            if (arrived)
            {
                cat_spWarpStartCount(ship, counts, &all);
            }
        }
    }
    for (type = 0; type < TOTAL_NUM_SHIPS; type++)
    {
        expected = record->ships[type] + ((type == sMothership) ? 1 : 0);
        if (expected != 0)
        {
            types++;
            shipsExpected += expected;
            ships += counts[type];
            if (counts[type] == expected)
            {
                typesOk++;
            }
            else
            {
                dbgMessagef("warpstarttest: %s level=%d %s n=%d record=%d", arrived ? "arrived" : "applied",
                            (int)catSpWarpLevelTo, ShipTypeToStr(type), (int)counts[type], (int)expected);
            }
        }
        else if (counts[type] != 0)
        {
            // a type the record does not list: made by its host (the hive frigates' swarmers, the mothership's own
            //      modules: univAddShip's), reported, not judged
            dbgMessagef("warpstarttest: %s level=%d %s n=%d (made by its host)", arrived ? "arrived" : "applied",
                        (int)catSpWarpLevelTo, ShipTypeToStr(type), (int)counts[type]);
        }
    }
    for (tech = 0; tech < CAT_TECHSET_FLAG; tech++)
    {
        if (record->tech[tech] != 0)
        {
            techs++;
            if (cat_rmTechSetHas(universe.players[0].researchinfo.HasTechnology, tech))
            {
                techsOk++;
            }
        }
    }
    resourcesOk = arrived || (universe.players[0].resourceUnits == record->resources);
    pass = (typesOk == types) && (ships == shipsExpected) && (techsOk == techs) && resourcesOk &&
           (arrived ? (ether == 0) : (outside == 0));
    dbgMessagef("warpstarttest: %s level=%d resources=%d/%d types=%d/%d ships=%d/%d (all %d) ether=%d outside=%d tech=%d/%d "
                "result=%s", arrived ? "arrived" : "applied", (int)catSpWarpLevelTo, (int)universe.players[0].resourceUnits,
                (int)record->resources, (int)typesOk, (int)types, (int)ships, (int)shipsExpected, (int)all, (int)ether,
                (int)outside, (int)techsOk, (int)techs, pass ? "PASS" : "FAIL");
    return pass;
}
#endif

/*-----------------------------------------------------------------------------
    Name        : singleHackResearchWarp
    Description : A function that hard codes the research a player is has already
                  had based on the settings daly told me they would be in each level
                  ugly stuff... replace with an elegant solution later
    Inputs      :
    Outputs     : Allows the player to research certain topics
    Return      : void
----------------------------------------------------------------------------*/



/*-----------------------------------------------------------------------------
    Name        : singlePlayerStartGame                             0x0051bdb0
    Description : CAT (M2-R8 kas-runtime-mission01): a new single-player game starts in Mission01's opening NIS
                  (nis\n01.nis, nis\n01.script) at the origin, played in the staging level Mission01_NIS01 that
                  singlePlayerStartNis loads first (the mission number stays 1); the universe is shown when the NIS
                  ends (singlePlayerNisStoppedCB loads Mission01 and the fleet hyperspaces in)
----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    Name        : singlePlayerStartGame                             0x0051bdb0
    Description : CAT (M2-R8 kas-runtime-mission01): a new single-player game starts in Mission01's opening NIS
                  (nis\n01.nis, nis\n01.script) at the origin, played in the staging level Mission01_NIS01 that
                  singlePlayerStartNis loads first (the mission number stays 1); the universe is shown when the NIS
                  ends (singlePlayerNisStoppedCB loads Mission01 and the fleet hyperspaces in). M2-R8
                  singleplayer-tutorial: the warp start (cat_spWarpToLevel) after tmTechInit / universeSwitchToPlayer
----------------------------------------------------------------------------*/
void singlePlayerStartGame(void)
{
    hvector startPoint;

    universe.players[1].resourceUnits = 0;

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
        GetStartPointPlayer(&startPoint);
        return;
    }
#endif

    universeSwitchToPlayer(0);
    if (catSpWarpEnabled)
    {
        cat_spWarpToLevel();
        return;
    }
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

/*-----------------------------------------------------------------------------
    Name        : singlePlayerMissionCompleteCB                     0x0051be40
    Description : CAT: the selection empties, the main screen stops rendering, the game quits at once (real time)
                  into the credits
----------------------------------------------------------------------------*/
void singlePlayerMissionCompleteCB(void)
{
    selSelected.numShips = 0;
    ioUpdateShipTotals();
    mrRenderMainScreen = FALSE;
    universe.quittime = universe.realtimeelapsed;           // CAT: real time (singlePlayerMissionCompleteCB 0x0051be40)
    // this will prevent the shiplist from being displayed!
    utyCreditsSequence = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : singlePlayerMissionFailedCB                       0x0051be70
    Description : CAT: Fleet Command announces the failure (0x2001028, to player 0), player 0 is killed with nobody
                  credited, and the win test runs (the quit follows in real time: CheckPlayerWin)
----------------------------------------------------------------------------*/
void singlePlayerMissionFailedCB(void)
{
    speechEventQueue(NULL, 0x2001028, -1, -1, 0, 0, -1, 5.0f, -1);
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
// 0x00894108: the mothership's big gun may fire (data default 1; kasfDisableBigGunFiring 0x0049ac30 clears it,
//      singlePlayerGameCleanup 0x00519510 sets it again).
//      Name chosen here
bool32 catSpBigGunAllowed = TRUE;



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
