/*============================================================================
    MAIN.C: Main Windows interface for Homeworld game code, including message
        handling and window set-up.

        Created June 1997 by Luke Moloney.
============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL2/SDL.h>

#include "AIPlayer.h"
#include "AutoLOD.h"
#include "avi.h"
// #include "bink.h"
#include "BTG.h"
#include "Camera.h"
#include "Captaincy.h"
#include "ColPick.h"
#include "CommandLayer.h"
#include "ConsMgr.h"
#include "AISelfTest.h"     // HWI-TEST: /catAiTest (M2-R7 ai-orders-moves)
#include "Debug.h"
#include "Demo.h"
#include "FastMath.h"     // HWI-EXPERIMENT: /oracleLibmSqrt (M2-R9 oracle-rung-physics; was /oracleTableFsqrt, M2-R7)
#include "FEReg.h"
#include "File.h"
#include "FontReg.h"
#include "Formation.h"
#include "glinc.h"
#include "Globals.h"
#include "HorseRace.h"
#include "Key.h"
#include "LaunchMgr.h"
#include "LevelLoad.h"      // HWI-TEST (skirmish-start): /catSkirmishTest, /catSkirmishMap
#include "MissionSelfTest.h" // HWI-TEST (M2-R8 kas-runtime-mission01): /catMissionTest
#include "main.h"
#include "mainrgn.h"
#include "Memory.h"
#include "mouse.h"
#include "MultiplayerGame.h"
#include "NIS.h"
#include "MotionTest.h"     // HWI-FEATURE(motion-interp): the motion / nis measurement switches
#include "TeamColourTest.h" // HWI-TEST (M3-R1 team-colours-warp)
#include "MotionInterp.h"   // HWI-FEATURE(motion-interp): /noInterp, /noNISInterp
#include "LOD.h"            // HWI-FEATURE(full-detail): /lodBands
#include "HS.h"             // HWI-FEATURE(hyperspace-assembly): /noHsAssembly
#include "ObjTypes.h"
#include "Options.h"
#include "OverlayIcons.h"
#include "InfoOverlay.h"
#include "PopupMenu.h"
#include "Particle.h"
#include "PiePlate.h"
#include "regkey.h"
#include "render.h"
#include "ResearchAPI.h"
#include "ResearchGUI.h"
#include "resource.h"
#include "rinit.h"
#include "Sensors.h"
#include "SoundEvent.h"
#include "soundlow.h"
#include "StringSupport.h"
#include "Subtitle.h"
#include "Tactics.h"
#include "Task.h"
#include "TitanNet.h"
#include "utility.h"
#include "Universe.h"
#include "ETG.h"            // CAT: /catEtgParseTest, /catEtgEffectTest (port diagnostics)
#include "Mesh.h"           // CAT: /catMeshLoadTest (port diagnostic)
#include "Trails.h"         // HWI-FEATURE(engine-trails): /exeTrails
#include "AIShip.h"         // HWI-TEST (M3-R3 mines): /catMineLog
#include "Dock.h"           // CAT: /catDockTest (port diagnostic, M2-R5 dock)
#include "KeyBindings.h"    // CAT: /catKeysTest (port diagnostic, M2-R6 startup-keys)
#include "Console.h"        // CAT: the developer console's keys, /catConsoleTest (M2-R6 startup-keys)
#include "UnivUpdate.h"     // CAT: catMultiplayerAutoSave (/SaveMPGames)
#include "texreg.h"         // CAT: /catTexturesTest (port diagnostic)
#include "OracleDump.h"     // HWI-ORACLE: /oracleDump, /oracleSelfTest
#include "uiscale.h"         // HWI-FEATURE(ui-scale): /uiScale, /testUsableBounds; the window request
#include "UiScaleTest.h"     // HWI-TEST (M3-R1 ui-scale): /catUiScaleTest
#include "Instrument.h"      // HWI-FEATURE(instrumentation): /overlay, the overlay's key; HWI-TEST /frameLog
#include "datasetup.h"       // HWI-FEATURE(data-setup): the player quit the first-run step; HWI-TEST /catDataSetupScript

#ifdef _WIN32
    #define strcasecmp _stricmp
    #include <windows.h>
    #include <winreg.h>
    #include "debugwnd.h"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif


/*=============================================================================
    Data:
=============================================================================*/

bool32 mainSafeGL = FALSE;

udword* devTable = NULL;
sdword  devTableLength = 0;

static bool32 mainActuallyQuit = TRUE;

bool32 selectedRES = FALSE;
bool32 selectedDEVICE = TRUE;
bool32 selectedGL = TRUE;

sdword mainReinitRenderer = 0;
static bool32 reinitInProgress = FALSE;

bool32 bMustFree = TRUE;

bool32 mainPlayAVIs;

bool32 windowNeedsDeleting = FALSE;

bool32 mainNoPerspective = FALSE;

bool32 systemActive = FALSE;              //active flag for the program

#ifndef __APPLE__
static char windowTitle[] = "Emergence";   // CAT: the exe's window class and title (0x008b85b8 / 0x008b85c4; GoK "HomeworldSDL")
#endif

//error strings
char ersWindowInit[] = "Error creating window";

//screen width, height
int MAIN_WindowWidth = 1024;
int MAIN_WindowHeight = 768;
int MAIN_WindowDepth = 32;
int MAIN_WindowWidthActual;
int MAIN_WindowHeightActual;

sdword mainWidthAdd = 0;
sdword mainHeightAdd = 0;

int mainWindowWidth;
int mainWindowHeight;
int mainWindowDepth;
#ifdef _WIN32
void *ghMainWindow = NULL;
void *ghInstance = NULL;
#endif

extern bool32 CompareBigfiles;
extern bool32 IgnoreBigfiles;
extern bool32 LogFileLoads;

//command-line switches and parameters
bool32 mainNoDrawPixels = FALSE;
bool32 mainOutputCRC = FALSE;
bool32 mainNoPalettes = TRUE;
bool32 mainSoftwareDirectDraw = FALSE;
bool32 mainDirectDraw = FALSE;
bool32 mainRasterSkip = FALSE;
bool32 mainDoubleIsTriple = FALSE;
#ifdef __GNUC__
bool32 mainFastFrontend = FALSE;
#else
bool32 mainFastFrontend = TRUE;
#endif
bool32 mainForceSoftware = FALSE;
bool32 mainAutoRenderer = TRUE;
bool32 mainForceKatmai = FALSE;
#ifdef _WINDOWS
bool32 mainAllowKatmai = TRUE;
#else
bool32 mainAllowKatmai = FALSE;
#endif
bool32 mainAllow3DNow = FALSE;
bool32 enableAVI = TRUE;
bool32 mainAllowPacking = TRUE;
bool32 mainOnlyPacking = FALSE;
bool32 gShowDamage = TRUE;
bool32 DebugWindow = FALSE;
sdword MemoryHeapSize = MEM_HeapSizeDefault;
#if MAIN_MOUSE_FREE
bool32 startupClipMouse = TRUE;
#endif
bool32 mouseClipped = FALSE;
sdword showBackgrounds = TRUE;
bool32 showBorder = TRUE;
sdword enableTextures = TRUE;
sdword enableSmoothing = TRUE;
sdword enableCPV = FALSE;   // CAT: per-vertex colour (0x00c52378, .bss; set by Cataclysm.cfg "enableCPV", /noCPV, the video options)
sdword enableLensFlare = FALSE;     // CAT (M2-R5 render): lens flares (0x00c52380, .bss; Cataclysm.cfg "enableLensFlare",
                                    //      the video options; cat_rndLensFlaresRender 0x0057bec0)
sdword enableRepairbots = FALSE;    // CAT (M2-R5 render): the repair droids (0x00c52374, .bss; Cataclysm.cfg
                                    //      "enableRepairbots"; cat_rndRenderObjectList 0x00579727)
sdword enableStipple = FALSE;
sdword enableTrails = TRUE;
#if TR_NIL_TEXTURE
bool32 GLOBAL_NO_TEXTURES = FALSE;
#endif

bool32 enableMSAA = FALSE;

// turn fullscreen off when debugging so that if the debugger kicks in
// after a crash you don't find yourself locked out and have to reboot...
#if defined(HW_BUILD_FOR_DEBUGGING) 
bool32 fullScreen = FALSE;
#else
bool32 fullScreen = TRUE;
#endif

bool32 slowBlits = FALSE;
#if RND_VISUALIZATION
bool32 dockLines = FALSE;
bool32 gunLines = FALSE;
bool8 RENDER_BOXES;
bool8 RENDER_LIGHTLINES;
#endif
bool32 enableSFX = TRUE;
bool32 enableSpeech = TRUE;
bool32 reverseStereo = FALSE;
bool32 useWaveout = FALSE;
bool32 useDSound = FALSE;
bool32 coopDSound = FALSE;
bool32 accelFirst = FALSE;
bool8 RENDER_BOXES = FALSE;
bool8 RENDER_LIGHTLINES = FALSE;
#if CL_TEXTFEEDBACK
bool32 enableTextFeedback = FALSE;
#endif

#if FEF_TEXTURES_DISABLABLE
bool32 fetEnableTextures = TRUE;
#endif
bool32 noDefaultComputerPlayer = FALSE;
bool8 ComputerPlayerEnabled[MAX_MULTIPLAYER_PLAYERS] = { FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE };
//udword ComputerPlayerLevel[MAX_MULTIPLAYER_PLAYERS] = {2,2,2,2,2,2,2,2};
bool32 gatherStats = FALSE;
bool32 showStatsFight = FALSE;
udword showStatsFightI = 0;
udword showStatsFightJ = 0;
bool32 showStatsFancyFight = FALSE;
char showStatsFancyFightScriptFile[50];
bool32 SecretWON = FALSE;
bool32 forceLAN = FALSE;
bool32 ShortCircuitWON = FALSE;
#if DBW_TO_FILE
bool32 debugToFile = FALSE;
#endif

bool32 debugPacketRecord = FALSE;

bool32 recordPackets = FALSE;
bool32 playPackets = FALSE;
bool32 recordplayPacketsInGame = FALSE;
bool32 recordFakeSendPackets = FALSE;
// CAT (M2-R6 oracle-mode-a): the recorded-game state words of Cataclysm (cleared by globalsInit 0x0048a060):
//      0x00a7519c is set while a game is recorded (recPackInGameStartCBSafeToStart 0x004cbbb0) or played back
//      (LoadRecordedGame 0x00487cf0); 0x00a75190 then says the recording is a multiplayer one (set by the record start
//      in a multiplayer game, and by the playback when a player's name is not the computer's "<name> <n>"), and the
//      simulation's network paths test both (univCreateShip, univAddMissile, missileShoot, processCollectResource,
//      processAttackToDo, the construction manager); 0x00a751a0 is set by the playback only (CheckPlayerWin
//      0x005485e0 then decides no winner). Names chosen here.
bool32 recordedGameActive = FALSE;          // 0x00a7519c
bool32 recordedGameMultiplayer = FALSE;     // 0x00a75190
bool32 recordedGamePlayback = FALSE;        // 0x00a751a0
char recordPacketFileName[MAX_RECORDPACKETFILENAME_STRLEN];
char recordPacketSaveFileName[MAX_RECORDPACKETFILENAME_STRLEN];

bool32 autoSaveDebug = FALSE;

#ifdef DEBUG_TACTICS
    bool32 tacticsOn = TRUE;
#endif
bool32 noRetreat = FALSE;

bool32 noPauseAltTab = FALSE;
bool32 noMinimizeAltTab = FALSE;

//options altered by a password function:
bool32 mainCDCheckEnabled = TRUE;

// CAT: the exe's network version and minor build (0x008b8850 "Emergence 1", 0x008b8860 "01"; versionNumDraw
//      0x005a8250 prints "<Version> Emergence 1.01", utyGameSystemsPreInit 0x005ab810 builds versionString from them;
//      GoK "GardensOfKadesh" and HOMEWORLD_BUILD_VERSION). The network version is what a LAN peer compares.
char networkVersion[MAX_NETWORKVERSION_STRING_LEN] = "Emergence 1";
char minorBuildVersion[] = "01";

char languageVersion[50] = "";    // constructed at beginning of program

// The version string is now constructed as follows:
//
// Network Version TAB Language TAB MinorBuild Version
//
// for example
// "DownloadableRC2\tEnglish\tBuild1"
//
//

// For purposes of games able to play each other on the network, just the networkVersion is consulted
//
// For purposes of whether a patch is required (valid version), the entire versionString (network language build) is consulted
//
// For finding out what directory to get the patch from, only the languageVersion is consulted

char versionString[MAX_VERSION_STRING_LEN] = "";        // constructed at beginning of program


sdword mainWindowTotalWidth = 0;
sdword mainWindowTotalHeight = 0;

bool32 noAuthorization = FALSE;

bool32 determCompPlayer = FALSE;

#if MAIN_SENSOR_LEVEL
udword initialSensorLevel = 0;
#endif

bool32 pilotView = FALSE;

bool32 debugScreenshots = FALSE;
bool32 vanillaMode = FALSE;
// CAT: the universe runs at 16 Hz by default (Universe.c UNIVERSE_UPDATE_RATE_SHIFT); GoK's 64 Hz only with /gok64Hz
bool32 gok64HzMode = FALSE;
extern ubyte UNIVERSE_UPDATE_RATE_SHIFT;

/*=============================================================================
    Functions:
=============================================================================*/

                // guess what?  The game code defines HKEY to 'H' which messes up the registry code.  So the
                // registry code gets to go here
#if 0	/* Not registering command line... */
int RegisterCommandLine(char *commandLine)
{
    HKEY key;

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, BASEKEYNAME,
                        0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
    {
        return FALSE;
    }

    if ((commandLine == NULL) || (commandLine[0] == 0))
    {
        if (RegSetValueEx(key, "CmdLine", 0, REG_SZ, "", 1) != ERROR_SUCCESS)
        {
            RegCloseKey(key);
            return FALSE;
        }
    }
    else
    {
        if (RegSetValueEx(key, "CmdLine", 0, REG_SZ, (BYTE *)commandLine, strlen(commandLine)+1) != ERROR_SUCCESS)
        {
            RegCloseKey(key);
            return FALSE;
        }
    }

    RegCloseKey(key);

    return TRUE;
}
#endif


/*-----------------------------------------------------------------------------
    Command-line parsing functions called when a certain flags are set
-----------------------------------------------------------------------------*/
// CAT: port diagnostic: /catShipStaticsSkip <type,type,...> leaves these ship types out of /catShipStaticsTest
bool32 CatShipStaticsSkipSet(char *string)
{
    char *s = string;

    while (*s && catShipStaticsSkipCount < CAT_SHIPSTATICS_MAXSKIP)
    {
        catShipStaticsSkip[catShipStaticsSkipCount++] = (sdword)strtol(s, &s, 10);
        while (*s == ',' || *s == ' ')
        {
            s++;
        }
    }
    return TRUE;
}

// CAT: port diagnostic: /catMeshLoadTest <listfile> (Mesh.c catMeshRunSelfTest)
bool32 CatMeshLoadTestSet(char *string)
{
    strncpy(catMeshSelfTestList, string, FL_Path - 1);
    catMeshSelfTestList[FL_Path - 1] = '\0';
    return TRUE;
}

// CAT: port diagnostic: /catEtgParseTest <listfile> (ETG.c catEtgRunParseSelfTest)
bool32 CatEtgParseTestSet(char *string)
{
    strncpy(catEtgSelfTestList, string, PATH_MAX - 1);
    catEtgSelfTestList[PATH_MAX - 1] = '\0';
    return TRUE;
}

// CAT (M2-R6 nis-fow-camera): port diagnostic: /catNisParseTest <listfile> (NIS.c catNisRunParseSelfTest)
bool32 CatNisParseTestSet(char *string)
{
    strncpy(catNisParseTestList, string, PATH_MAX - 1);
    catNisParseTestList[PATH_MAX - 1] = '\0';
    return TRUE;
}

// CAT: port diagnostic: /catEtgEffectTest <ticks> (ETG.c catEtgRunEffectSelfTest, after the parse test)
bool32 CatEtgEffectTestSet(char *string)
{
    catEtgEffectSelfTestTicks = (sdword)strtol(string, NULL, 10);
    return TRUE;
}

// CAT: port diagnostic: /catDockTest <ticks> (Dock.c catDockSelfTestTick: the scripted dock self-test on the headless
//      Mission01 run, M2-R5 dock)
bool32 CatDockTestSet(char *string)
{
    catDockSelfTestSet(string);
    return TRUE;
}

// HWI-TEST: port diagnostic: /catBuildTest <ticks> (ConsMgr.c catBuildSelfTestTick: the construction manager builds an
//      externally built and an internal ship on the headless Mission01 run, M2-R6 managers)
bool32 CatBuildTestSet(char *string)
{
    catBuildSelfTestSet(string);
    return TRUE;
}

// HWI-TEST: port diagnostic: /catAiTest <ticks> (AISelfTest.c catAiSelfTestTick: the computer player gets a scripted
//      factory, RUs and fleet on the headless Mission01 run and must form teams and issue attack, defence and recon
//      moves, M2-R7 ai-orders-moves)
bool32 CatAiTestSet(char *string)
{
    catAiSelfTestSet(string);
    return TRUE;
}

// HWI-TEST (skirmish-start): port diagnostic: /catSkirmishTest <ticks>, /catSkirmishMap <name> (LevelLoad.c
//      catSkirmishSelfTestStart / Tick: the front end's skirmish start with no input, judged after <ticks> ticks)
// HWI-TEST (M2-R8 kas-runtime-mission01): /catMissionTest <ticks> (MissionSelfTest.c: Mission01 from its NIS-first
//      start through its KAS objectives, headless, no input); /catMissionTestTutorial <ticks> (Tutorial1's first screen).
//      The mission test runs without sound and speech (as /oracleReplay: the mixer and speech threads run on the wall
//      clock); the tutorial test keeps speech (its prompt is a narration's subtitle) and is not a stream comparison
// HWI-TEST (M2-R10 tests-leftovers): every /catMissionTest run (missiontest Mission01 .. 17, the warp start) is on the
//      virtual audio clock as the tutorial leg is (decision 94's default): the mixer and the stream would be stepped by the
//      game thread, never by SDL's audio thread or the stream thread. With sound and speech off soundinit never runs, so
//      the switch changes nothing in these streams today; Mission06's rng_all-only pairs were the nebula tendrils'
//      uninitialised bounding sphere (Nebulae.c nebInitTendril, CAT-FIX), not the mixer
bool32 CatMissionTestSet(char *string)
{
    catMissionSelfTestSet(string);
    enableSFX = FALSE;
    enableSpeech = FALSE;
    soundVirtualClock = TRUE;
    return TRUE;
}

// HWI-TEST (M2-R9 missions-02-09): /catMissionTestMission <N> with /catMissionTest: mission N (2 .. 17) from the exe's
//      warp start (cat_spWarpToLevel 0x0051b9c0, Warp.script's record for level N) instead of Mission01's NIS-first start
bool32 CatMissionTestMissionSet(char *string)
{
    catMissionSelfTestMissionSet(string);
    return TRUE;
}

// HWI-TEST (M2-R9 missions-10-17): /catMissionTestTrace: the KAS message trace of the mission self-test (a diagnostic)
bool32 CatMissionTestTraceSet(char *string)
{
    (void)string;
    catMissionSelfTestTraceSet();
    return TRUE;
}

// HWI-TEST (M2-R10 tests-leftovers): /catMissionTestFailJump with /catMissionTest <ticks> /catMissionTestMission N: the
//      deliberate-loss test of the failed jump (MissionSelfTest.c catMtFailJumpWatch)
bool32 CatMissionTestFailJumpSet(char *string)
{
    (void)string;
    catMissionSelfTestFailJumpSet();
    return TRUE;
}

// HWI-TEST (M3-R3 mission02): /catMissionTestHoldScout <tick> with /catMissionTest <ticks> /catMissionTestMission 2:
//      the FindDestroyer recon waits until <tick> (MissionSelfTest.c: the BUSHAN-RE hint's timer can run out)
bool32 CatMissionTestHoldScoutSet(char *string)
{
    catMissionSelfTestHoldScoutSet(string);
    return TRUE;
}

// HWI-TEST (M2-R9 missions-10-17): /catWarpStartTest with /catMissionTest <ticks>: the warp start from the test copy of
//      warp.script the harness puts in the run's data dir (MissionSelfTest.c, SinglePlayer.c cat_spWarpStartSelfTestCheck)
bool32 CatWarpStartTestSet(char *string)
{
    (void)string;
    catWarpStartSelfTestSet();
    return TRUE;
}

bool32 CatMissionTestTutorialSet(char *string)
{
    catMissionSelfTestTutorialSet(string);
    enableSFX = FALSE;                  // speech stays on (SDL's dummy driver): the first prompt is the narration's subtitle
    // HWI-TEST (M2-R9 recorder-leftovers): the speech runs on the virtual audio clock (soundlow.c soundVirtualClock), so
    //      the narration's subtitles start and end at the same frame in every run and the leg's stream is a pair
    soundVirtualClock = TRUE;
    return TRUE;
}

// HWI-TEST (M2-R9 recorder-leftovers): /catVirtualAudio: the mixer and the speech stream advanced by the virtual clock on
//      the game thread instead of SDL's audio thread and the stream thread (only with a virtual-clock run)
bool32 CatVirtualAudioSet(char *string)
{
    (void)string;
    soundVirtualClock = TRUE;
    return TRUE;
}

// HWI-TEST (M2-R8 singleplayer-tutorial): /catLoadGameTest <ticks> /catLoadGameName <name> (MissionSelfTest.c: a
//      single-player save loaded through the mode-A loader and run, headless); without sound and speech, as the mission test
bool32 CatLoadGameTestSet(char *string)
{
    catLoadGameSelfTestSet(string);
    enableSFX = FALSE;
    enableSpeech = FALSE;
    return TRUE;
}

// HWI-TEST (M2-R9 missions-10-17): /catLoadGameTraining with /catLoadGameTest: the save is a tutorial lesson's
//      (SavedGames/Training), loaded as the front end's tutorial load does (gpLoadTutorial: tutorial 1)
bool32 CatLoadGameTrainingSet(char *string)
{
    (void)string;
    catLoadGameSelfTestTrainingSet();
    return TRUE;
}

bool32 CatLoadGameNameSet(char *string)
{
    catLoadGameSelfTestNameSet(string);
    return TRUE;
}
bool32 CatInfectAtSet(char *string)                 // HWI-TEST (M3-R4 infect-convert): /catInfectAt <tick>:<ShipType>
{
    catInfectAtSet(string);
    return TRUE;
}

bool32 CatTooltipAtSet(char *string)                // HWI-TEST (M3-R4 crystal-tooltip)
{
    catTooltipAtSet(string);
    return TRUE;
}

bool32 CatMadProbeEnable(char *string)      // HWI-TEST (M3-R4 mesh-anim-interp)
{
    extern bool32 catMadProbeOn;
    (void)string;
    catMadProbeOn = TRUE;
    return TRUE;
}

bool32 CatSoundNoiseAtSet(char *string)     // HWI-TEST (M3-R4 sound-burst-log)
{
    extern udword catSoundNoiseAt;
    catSoundNoiseAt = (udword)strtoul(string, NULL, 10);
    return TRUE;
}

bool32 CatNaNProbeEnable(char *string)      // HWI-TEST (M3-R4 nan-positions)
{
    extern bool32 catNaNProbeOn;
    (void)string;
    catNaNProbeOn = TRUE;
    return TRUE;
}

bool32 CatSkirmishTestSet(char *string)
{
    catSkirmishSelfTestSet(string);
    return TRUE;
}

bool32 CatSkirmishMapSet(char *string)
{
    catSkirmishSelfTestSetMap(string);
    return TRUE;
}

// HWI-TEST (M2-R10 tests-leftovers): /catSkirmishBigotry <0..2>: the skirmish test presses the CPU-attacks radio button
//      (opCPUAttacks: tpGameCreated.aiplayerBigotry) before StartNewGame
bool32 CatSkirmishBigotrySet(char *string)
{
    catSkirmishSelfTestSetBigotry(string);
    return TRUE;
}

// CAT: port diagnostic: /catCommandTest <ticks> (CommandLayer.c catCommandSelfTestTick: the scripted command self-test
//      on the headless Mission01 run, M2-R5 command-tactics)
bool32 CatCommandTestSet(char *string)
{
    catCommandSelfTestSet(string);
    return TRUE;
}

// CAT: port diagnostic: /catRenderTest <frames> (render.c catRenderSelfTestTick: the render self-test on the headless
//      Mission01 run, M2-R5 render)
bool32 CatRenderTestSet(char *string)
{
    catRenderSelfTestSet(string);
    return TRUE;
}

// HWI-FEATURE(motion-interp) measurement: /catMotionTest <ticks>, /catNisMotionTest <nis>[:<from>:<to>], /catMotionDump
//      <file>, /catMotionFps <fps> (MotionTest.c: the motion and nis harness modes, docs/m2-r6-motion-interp.md)
// HWI-FEATURE(motion-interp): /noInterp turns both interpolation flags off (MotionInterp.c); after the options file
//      (utyOptionsFileRead runs before ProcessCommandLine), so the switch wins for the run; M2-R7 r6-leftovers: run-only,
//      the options file keeps its own values (miSwitchOff, MotionInterp.c)
bool32 NoInterpSet(char *string)                    // HWI-FEATURE(motion-interp)
{                                                   // HWI-FEATURE(motion-interp)
    miSwitchOff(TRUE, TRUE);                        // HWI-FEATURE(motion-interp)
    return TRUE;                                    // HWI-FEATURE(motion-interp)
}                                                   // HWI-FEATURE(motion-interp)
bool32 NoNISInterpSet(char *string)                 // HWI-FEATURE(motion-interp): /noNISInterp, run-only as /noInterp
{                                                   // HWI-FEATURE(motion-interp)
    miSwitchOff(FALSE, TRUE);                       // HWI-FEATURE(motion-interp)
    return TRUE;                                    // HWI-FEATURE(motion-interp)
}                                                   // HWI-FEATURE(motion-interp)
// HWI-FEATURE(letterbox-setting): /letterboxBehind: the old look (the letterbox bars depth-tested behind near ships) for
//      this run only; after the options file (read before ProcessCommandLine), run-only as /noInterp: the saves keep the
//      file's letterboxBehind row (render.c rndLetterboxFileValues)
bool32 LetterboxBehindSet(char *string)             // HWI-FEATURE(letterbox-setting)
{                                                   // HWI-FEATURE(letterbox-setting)
    rndLetterboxBehindSwitch();                     // HWI-FEATURE(letterbox-setting)
    return TRUE;                                    // HWI-FEATURE(letterbox-setting)
}                                                   // HWI-FEATURE(letterbox-setting)
// HWI-FEATURE(ui-scale): /uiScale N: the UI scale in percent (0 = by the window height; 100 .. 400) for this run only;
//      after the options file (read before ProcessCommandLine), run-only as /noInterp: the saves keep the file's uiScale
//      row (uiscale.c uiScaleFileValues)
bool32 UiScaleSet(char *string)                     // HWI-FEATURE(ui-scale)
{                                                   // HWI-FEATURE(ui-scale)
    uiScaleSwitchSet((sdword)strtol(string, NULL, 10)); // HWI-FEATURE(ui-scale)
    return TRUE;                                    // HWI-FEATURE(ui-scale)
}                                                   // HWI-FEATURE(ui-scale)
// HWI-TEST (M3-R1 ui-scale): /testUsableBounds WxH[@DWxDH]: the desktop's usable bounds (and display size) the windowed
//      mode's fit uses for this run (uiscale.c uiScaleWindowFit), for the harness, whose offscreen driver has no desktop;
//      /catUiScaleTest: the UI-scale self-test (Game/UiScaleTest.c)
bool32 TestUsableBoundsSet(char *string)
{
    uiScaleTestBoundsSet(string);
    return TRUE;
}
// HWI-FEATURE(full-detail): /lodBands: FULL DETAIL (the options file's noLOD row, on by default) off for this run only: the
//      exe's size-on-screen bands and frame-time passes pick the levels drawn; run-only as /noInterp (LOD.c catLodBandsSwitch:
//      the saves keep the file's noLOD row)
bool32 LodBandsSet(char *string)                    // HWI-FEATURE(full-detail)
{                                                   // HWI-FEATURE(full-detail)
    catLodBandsSwitch();                            // HWI-FEATURE(full-detail)
    return TRUE;                                    // HWI-FEATURE(full-detail)
}                                                   // HWI-FEATURE(full-detail)
// HWI-FEATURE(hyperspace-assembly): /noHsAssembly: this run only, a modular ship's hyperspace as the exe draws it (the
//      gate and the clip's sweep from the core hull's box times hyperspaceBoxScale, a hidden part shown only at its own
//      exit): an A/B for the captures and the user's comparison; no options-file row (HS.c hsAssemblyEnabled)
bool32 NoHsAssemblySet(char *string)                // HWI-FEATURE(hyperspace-assembly)
{                                                   // HWI-FEATURE(hyperspace-assembly)
    hsAssemblyEnabled = FALSE;                      // HWI-FEATURE(hyperspace-assembly)
    return TRUE;                                    // HWI-FEATURE(hyperspace-assembly)
}                                                   // HWI-FEATURE(hyperspace-assembly)
// HWI-FEATURE(mesh-batching): /meshImmediate: this run only, the CPU-lit mesh renderers submit their corners one GL call
//      at a time (GoK's immediate mode) instead of in arrays (Mesh.c meshImmediate): the exact A/B of the batched path;
//      no options-file row (the console row meshImmediate sets it for the run too)
// HWI-FEATURE(sharp-textures): /noFilter (the exe's switch: "disable bi-linear filtering of textures") and its alias
//      /sharpTextures: SHARP TEXTURES (texLinearFiltering 0) for this run only, run-only as /noInterp (texreg.c
//      trFilterSwitchSet: the saves keep the file's texLinearFiltering row) and kept over the start's effects preset
//      (opUpdateSettings -> opEffectsHelper's HIGH sets 1: in the exe the switch was lost at every start)
bool32 NoFilterSet(char *string)                    // HWI-FEATURE(sharp-textures)
{                                                   // HWI-FEATURE(sharp-textures)
    trFilterSwitchSet();                            // HWI-FEATURE(sharp-textures)
    return TRUE;                                    // HWI-FEATURE(sharp-textures)
}                                                   // HWI-FEATURE(sharp-textures)
bool32 MeshImmediateSet(char *string)               // HWI-FEATURE(mesh-batching)
{                                                   // HWI-FEATURE(mesh-batching)
    meshImmediateSwitch();                          // HWI-FEATURE(mesh-batching)
    return TRUE;                                    // HWI-FEATURE(mesh-batching)
}                                                   // HWI-FEATURE(mesh-batching)
// HWI-FEATURE(engine-trails): /exeTrails: this run only, the missile corvettes' two ribbons as cataclysm.exe draws them
//      (one per mex Eng chunk) instead of the four jet streams from the engine circles (Trails.c hwiEngineTrails); no
//      options-file row (the console row engineTrails sets it for the run too)
bool32 ExeTrailsSet(char *string)                   // HWI-FEATURE(engine-trails)
{                                                   // HWI-FEATURE(engine-trails)
    hwiEngineTrailsExeSwitch();                     // HWI-FEATURE(engine-trails)
    return TRUE;                                    // HWI-FEATURE(engine-trails)
}                                                   // HWI-FEATURE(engine-trails)
// HWI-FEATURE(sensors-points): /exePoints: this run only, the sensors manager's points at cataclysm.exe's sizes (the Dust
//      1 pixel, the resource / meteor / mine / crate markers 2 above the collision radius 45 and 1 under it) instead of
//      the Dust 2 and the markers 3 (Sensors.c hwiSensorsPoints); no options-file row (the console row sensorsPoints
//      sets it for the run too)
bool32 ExePointsSet(char *string)                   // HWI-FEATURE(sensors-points)
{                                                   // HWI-FEATURE(sensors-points)
    hwiSensorsPointsExeSwitch();                    // HWI-FEATURE(sensors-points)
    return TRUE;                                    // HWI-FEATURE(sensors-points)
}                                                   // HWI-FEATURE(sensors-points)
// HWI-TEST (M3-R1 full-detail): /catLodTest <tick>[:far][:fleet] and /catLodStats <from>-<to>[:fleet] (render.c: "lodtest:" / "lodstats:" lines)
bool32 CatLodTestSet(char *string)
{
    catLodTestSet(string);
    return TRUE;
}
bool32 CatLodStatsSet(char *string)
{
    catLodStatsSet(string);
    return TRUE;
}
// HWI-FEATURE(instrumentation): /overlay: the on-screen overlay on for this run (Instrument.c; the console row "overlay"
//      and the Menu key toggle it too; it has no options-file row, so nothing is saved)
bool32 OverlaySet(char *string)                     // HWI-FEATURE(instrumentation)
{                                                   // HWI-FEATURE(instrumentation)
    hwiOverlaySwitch();                             // HWI-FEATURE(instrumentation)
    return TRUE;                                    // HWI-FEATURE(instrumentation)
}                                                   // HWI-FEATURE(instrumentation)
// HWI-TEST (M3-R3 settings-and-keys): scripted inputs through the engine's own paths for the captures (Instrument.c):
//      /catOverlayKeyAt <tick>[,..] (F1 to HandleEvent), /catConsoleAt <tick>:<row>=<value>[,..] (a console row),
//      /catFeScreen <screen>[@<tick>] (a front-end screen pushed)
bool32 CatOverlayKeyAtSet(char *string)
{
    hwiOverlayKeyAtSet(string);
    return TRUE;
}
bool32 CatConsoleAtSet(char *string)
{
    hwiConsoleAtSet(string);
    return TRUE;
}
bool32 CatFeScreenSet(char *string)
{
    hwiFeScreenSet(string);
    return TRUE;
}
bool32 CatTexListSet(char *string)                  // HWI-TEST (M3-R3): /catTexList <substring> (texreg.c trFilterCount)
{
    trFilterListSet(string);
    return TRUE;
}
bool32 CatMeshTexLogSet(char *string)               // HWI-TEST (M3-R3 mines): /catMeshTexLog <substring> (Mesh.c)
{
    catMeshTexLogSet(string);
    return TRUE;
}
bool32 CatMineLogSet(char *string)                  // HWI-TEST (M3-R3 mines): /catMineLog <ticks> (AIShip.c)
{
    catMineLogSet(string);
    return TRUE;
}
bool32 CatMineChaseSet(char *string)                // HWI-TEST (M3-R3 mines): /catMineChase <tick>[:<shiptype>] (AIShip.c)
{
    catMineChaseSet(string);
    return TRUE;
}
bool32 CatTrailViewSet(char *string)                // HWI-TEST (M3-R3 engine-trails): /catTrailView <tick>:<ShipType>[:d<distance>][:e<elevation>][:s<side>] (render.c)
{
    catTrailViewSet(string);
    return TRUE;
}
bool32 CatMineViewSet(char *string)                 // HWI-TEST (M3-R3 mines): /catMineView <tick>[:dropped][:<distance>] (render.c)
{
    catMineViewSet(string);
    return TRUE;
}
// HWI-TEST (M3-R2 instrumentation): /frameLog <file>: one line a second of frame times, universe ticks, the heap and the
//      vertices drawn (Instrument.c; tools/frame_log_report.py)
bool32 FrameLogSet(char *string)
{
    hwiFrameLogSet(string);
    return TRUE;
}
// HWI-TEST (M3-R2 instrumentation) [audit: the frame log's build= field]: the build version meson gives only the SDL
//      library (src/SDL/meson.build -DHOMEWORLD_BUILD_VERSION); src/Game/Instrument.c got mainswitches.h's fallback
//      "UNSUPPORTED_BUILD"
const char *hwiBuildVersion(void)                   // HWI-TEST
{
    return HOMEWORLD_BUILD_VERSION;
}
// HWI-TEST (M3-R2 instrumentation): /catModeQuitTest <W>x<H>: a resolution change in the front end, then one quit
bool32 CatModeQuitTestSet(char *string)
{
    catModeQuitTestSet(string);
    return TRUE;
}
extern bool32 catHiddenWindow;              // HWI-TEST (M3-R4 mode-switch-fix): render.c, /catHiddenWindow
sdword catMovieTestLevel = -1;              // HWI-TEST (M3-R4 intro-movies): /catMovieTest <level>[:<frames>] (utility.c)
bool32 CatMovieTestSet(char *string)
{
    int level = -1, frames = 0;

    if (sscanf(string, "%d:%d", &level, &frames) < 1 || level < 0)
    {
        return FALSE;
    }
    catMovieTestLevel = level;
    aviFrameCap = (frames > 0) ? frames : 0;
    return TRUE;
}
bool32 CatDataSetupScriptSet(char *string)  // HWI-TEST (data-setup): /catDataSetupScript <answers> (datasetup.c)
{
    dsTestScriptSet(string);
    return TRUE;
}

bool32 CatMovieShotSet(char *string)        // HWI-TEST (M3-R4 intro-movies): /catMovieShot <frame>[,<frame>...]
{
    aviMovieShotSet(string);
    return TRUE;
}
bool32 CatMovieEventSet(char *string)       // HWI-TEST (M3-R4 intro-movies): /catMovieEventAt <frame>:quit|<W>x<H>
{
    aviMovieEventSet(string);
    return TRUE;
}
bool32 CatResizeAtSet(char *string)         // HWI-TEST (M3-R4 window-modes): /catResizeAt <tick>:<W>x<H>
{
    catResizeAtSet(string);
    return TRUE;
}
// HWI-TEST (M3-R1 letterbox-front): /catShot <spec>, /catShotFile <prefix>, /catShotLetterbox (MotionTest.c catShotFrameEnd:
//      the any-mode frame capture of the harness's --shot)
bool32 CatShotSet(char *string)
{
    catShotSet(string);
    return TRUE;
}
bool32 CatShotFileSet(char *string)
{
    catShotFileSet(string);
    return TRUE;
}
bool32 CatShotLetterboxSet(char *string)
{
    catShotLetterboxSet();
    return TRUE;
}
// HWI-TEST (M3-R1 team-colours-warp): /catTeamColourTest (Game/TeamColourTest.c: the team-colour dump and self-test)
bool32 CatTeamColourTestSet(char *string)
{
    catTeamColourTest = TRUE;
    return TRUE;
}
bool32 CatTeamColourTicksSet(char *string)
{
    sscanf(string, "%d", &catTeamColourNextTicks);
    return TRUE;
}
// HWI-TEST (M3-R2 nis-flicker): /catFrameSig <file> (MotionTest.c catFrameSigEnd: the per-frame signal, any mode)
bool32 CatFrameSigSet(char *string)
{
    catFrameSigSet(string);
    return TRUE;
}
// HWI-TEST (M3-R2 nis-flicker): /catVirtualFps <fps> (MotionTest.c catVirtualFpsSet: the virtual frame rate, any mode)
bool32 CatVirtualFpsSet(char *string)
{
    catVirtualFpsSet(string);
    return TRUE;
}
bool32 CatNisScissorOutSet(char *string)
{
    catNisScissorOutSet(string);
    return TRUE;
}
bool32 CatMotionTestSet(char *string)               // HWI-FEATURE(motion-interp)
{                                                   // HWI-FEATURE(motion-interp)
    catMotionTestSet(string);                       // HWI-FEATURE(motion-interp)
    return TRUE;                                    // HWI-FEATURE(motion-interp)
}                                                   // HWI-FEATURE(motion-interp)
bool32 CatNisMotionTestSet(char *string)            // HWI-FEATURE(motion-interp)
{                                                   // HWI-FEATURE(motion-interp)
    catNisMotionTestSet(string);                    // HWI-FEATURE(motion-interp)
    return TRUE;                                    // HWI-FEATURE(motion-interp)
}                                                   // HWI-FEATURE(motion-interp)
bool32 CatMotionDumpSet(char *string)               // HWI-FEATURE(motion-interp)
{                                                   // HWI-FEATURE(motion-interp)
    catMotionTestDumpSet(string);                   // HWI-FEATURE(motion-interp)
    return TRUE;                                    // HWI-FEATURE(motion-interp)
}                                                   // HWI-FEATURE(motion-interp)
bool32 CatMotionShotSet(char *string)               // HWI-FEATURE(motion-interp)
{                                                   // HWI-FEATURE(motion-interp)
    catMotionTestShotSet(string);                   // HWI-FEATURE(motion-interp)
    return TRUE;                                    // HWI-FEATURE(motion-interp)
}                                                   // HWI-FEATURE(motion-interp)
bool32 CatMotionFpsSet(char *string)                // HWI-FEATURE(motion-interp)
{                                                   // HWI-FEATURE(motion-interp)
    catMotionTestFpsSet(string);                    // HWI-FEATURE(motion-interp)
    return TRUE;                                    // HWI-FEATURE(motion-interp)
}                                                   // HWI-FEATURE(motion-interp)
// HWI-FEATURE(motion-interp) measurement (M3-R1 motion-jitter): /catMotionScene, /catMotionRealClock (MotionTest.c)
bool32 CatMotionSceneSet(char *string)              // HWI-FEATURE(motion-interp)
{                                                   // HWI-FEATURE(motion-interp)
    catMotionSceneSet(string);                      // HWI-FEATURE(motion-interp)
    return TRUE;                                    // HWI-FEATURE(motion-interp)
}                                                   // HWI-FEATURE(motion-interp)
bool32 CatMotionPauseSet(char *string)              // HWI-TEST (M3-R3 pause-freeze): /catMotionPause (MotionTest.c)
{                                                   // HWI-TEST
    catMotionPauseSet(string);                      // HWI-TEST
    return TRUE;                                    // HWI-TEST
}                                                   // HWI-TEST
bool32 CatMotionRealClockSet(char *string)          // HWI-FEATURE(motion-interp)
{                                                   // HWI-FEATURE(motion-interp)
    catMotionRealClockSet(string);                  // HWI-FEATURE(motion-interp)
    return TRUE;                                    // HWI-FEATURE(motion-interp)
}                                                   // HWI-FEATURE(motion-interp)

// CAT: port diagnostic: /catHudTest <frames> (InfoOverlay.c catHudSelfTestTick: the HUD self-test on the headless
//      Mission01 run, M2-R5 hud)
bool32 CatHudTestSet(char *string)
{
    catHudSelfTestSet(string);
    return TRUE;
}

// CAT: port diagnostic: /catUiTest <ticks> (PopupMenu.c catUiSelfTestTick: the popup menu, the cat_ui* commands and
//      the game-stats screen on the headless Mission01 run, M2-R6 mainrgn-popup)
bool32 CatUiTestSet(char *string)
{
    catUiSelfTestSet(string);
    return TRUE;
}

// CAT: port diagnostic: /catConsoleTest <ticks> (Console.c catConsoleSelfTestTick: the developer console's command
//      table on the headless Mission01 run, M2-R6 startup-keys)
bool32 CatConsoleTestSet(char *string)
{
    catConsoleSelfTestSet(string);
    return TRUE;
}

// CAT: port diagnostic: /catFeScreensTest <listfile> (FEFlow.c catFeScreensRunSelfTest: push and pop every screen of
//      the install's .fib files, M2-R6 frontend)
bool32 CatFeScreensTestSet(char *string)
{
    strncpy(catFeScreensSelfTestList, string, PATH_MAX - 1);
    catFeScreensSelfTestList[PATH_MAX - 1] = '\0';
    return TRUE;
}

#if HWI_ORACLE
// HWI-ORACLE: /oracleDump <file>, /oracleSelfTest <ticks>, /oracleSelfTestPerturb <tick> (Game/OracleDump.c,
// docs/m2-r2-oracle.md): checksum-oracle instrumentation, not a Cataclysm option
bool32 OracleDumpSet(char *string)
{
    oracleSetDumpFile(string);
    return TRUE;
}

bool32 OracleSelfTestSet(char *string)
{
    oracleSetSelfTest(string);
    return TRUE;
}

bool32 OracleSelfTestPerturbSet(char *string)
{
    oracleSetSelfTestPerturb(string);
    return TRUE;
}

// HWI-ORACLE (M2-R8 oracle-rung-physics): the permanent per-ship / per-packet dump
bool32 OracleShipDumpSet(char *string)
{
    oracleSetShipDump(string);
    return TRUE;
}

bool32 OracleMissionSet(char *string)
{
    oracleSetMission(string);
    return TRUE;
}

// HWI-ORACLE (M2-R9 recorder-leftovers): the mode-B recording (OracleDump.c oracleRecordTick)
bool32 OracleRecordSet(char *string)
{
    oracleSetRecord(string);
    return TRUE;
}

bool32 OracleResaveSet(char *string)
{
    oracleSetResave(string);
    return TRUE;
}

bool32 OracleRecordStopSet(char *string)
{
    oracleSetRecordStop(string);
    return TRUE;
}

bool32 OracleReplaySet(char *string)
{
    oracleSetReplay(string);
    // HWI (M2-R6 oracle-mode-a): a replay is a headless data run with no sound and no speech (/noSound /noSpeech exist
    //      only with SE_DEBUG): the mixer thread ends an ambience sound on the wall clock and SoundEvent.c then draws
    //      RANDOM_SOUND_GAME_THREAD (stream 11), which made the stream's rng_all differ run to run from about tick 476
    enableSFX = FALSE;
    enableSpeech = FALSE;
    return TRUE;
}
#endif

bool32 HeapSizeSet(char *string)
{
    sscanf(string, "%d", &MemoryHeapSize);
    if (MemoryHeapSize <= MEM_BlockSize * 2)
    {
        MemoryHeapSize = MEM_HeapSizeDefault;
    }
    return TRUE;
}


bool32 EnableFileLoadLog(char *string)
{
    logfileClear(FILELOADSLOG);
    return TRUE;
}

bool32 SelectDevice(char* string)
{
    selectedDEVICE = TRUE;
    return TRUE;
}

bool32 SelectMSGL(char* string)
{
    return TRUE;
}

bool32 EnableRasterSkip(char* string)
{
    mainRasterSkip = TRUE;
    return TRUE;
}

bool32 EnableDoubleIsTriple(char* string)
{
    mainDoubleIsTriple = TRUE;
    return TRUE;
}

bool32 DisableFastFrontend(char* string)
{
    mainFastFrontend = FALSE;
    return TRUE;
}

bool32 EnableGatherStats(char *string)
{
    noDefaultComputerPlayer = TRUE;
    return TRUE;
}

bool32 EnableShowStatsFancyFight(char *string)
{
    showStatsFancyFight = TRUE;
    strcpy(showStatsFancyFightScriptFile,string);
    noDefaultComputerPlayer = TRUE;
    return TRUE;
}

bool32 EnableShowStatsFight(char *string)
{
    sscanf(string, "%d", &showStatsFightI);

    if ((string = strtok(NULL, TS_Delimiters)) == NULL)
        return TRUE;        //error??
    sscanf(string, "%d", &showStatsFightJ);

    showStatsFight = TRUE;

    noDefaultComputerPlayer = TRUE;
    return TRUE;
}

bool32 SpecifyLogFilePath(char *string)
{
    strcpy(logFilePath,string);
    return TRUE;
}


#if NIS_TEST
extern char *nisTestNIS;
extern char *nisTestScript;
bool32 TestNISSet(char *string)
{
    static char staticString[256];

    if ((string = strtok(NULL, TS_Delimiters)) == NULL)
        return TRUE;
    memStrncpy(staticString, string, 255);
    nisTestNIS = staticString;
    return TRUE;
}
bool32 TestNISScriptSet(char *string)
{
    static char staticString[256];

    if ((string = strtok(NULL, TS_Delimiters)) == NULL)
        return TRUE;
    memStrncpy(staticString, string, 255);
    nisTestScript = staticString;
    return TRUE;
}
#endif

#if MAIN_SENSOR_LEVEL
bool32 InitialSensorLevelSet(char *string)
{
    sscanf(string, "%d", &initialSensorLevel);
    return TRUE;
}
#endif

#if LOD_SCALE_DEBUG
bool32 EnableLodScaleDebug(char *string)
{
    sscanf(string, "%f", &lodDebugScaleFactor);
    return TRUE;
}
#endif

bool32 EnableDemoRecord(char *string)
{
    if (!demDemoPlaying)
    {
        demDemoRecording = TRUE;
        strcpy(demDemoFilename, string);
    }
    return TRUE;
}

bool32 EnableDemoPlayback(char *string)
{
    if (!demDemoRecording)
    {
        wasDemoPlaying = demDemoPlaying = TRUE;
        noPauseAltTab = TRUE;
        strcpy(demDemoFilename, string);
    }
    return TRUE;
}

#if DEM_AUTO_DEMO
bool32 AutoDemoWaitSet(char *string)
{
    sscanf(string , "%f", &demAutoDemoWaitTime);
    return TRUE;
}
#endif

// CAT (M2-R6 startup-keys): the exe's command-line setters that GoK's table lacks
//      cat_mainPrependPathSet 0x0055f3d0 (/prepath: filePrependPathSet and its flag FilePathPrepended): GoK renamed
//      HW1's /prepath to /bigoverride (fileOverrideBigPathSet); /prepath sets the same path
static bool32 cat_mainPrependPathSet(char *string)
{
    return fileOverrideBigPathSet(string);
}

//      cat_mainCDROMPathSet 0x0055f3f0 (/CDpath): fileCDROMPathSet and its flag CDROMPathPrepended, which stops
//      utyGameSystemsPreInit's CD-ROM search. HWI: the Win32 drive test and its message ("'%s' Is not a valid
//      CD-ROM; path ignored." 0x008b9c10) are GoK's fileCDROMPathSet's; a set path stops the search there too
static bool32 cat_mainCDROMPathSet(char *string)
{
    return fileCDROMPathSet(string);
}

//      cat_mainLoadMPGameSet 0x0055f500 (/LoadMPGame <file>): "savedGames\multiplayer\" + the file, loaded by the
//      next multiplayer gameStart (utility.c). HWI: GoK's multiplayer saves directory
char catMainLoadMPGamePath[0x100] = "";                     // 0x00b3a020

static bool32 cat_mainLoadMPGameSet(char *string)
{
    snprintf(catMainLoadMPGamePath, sizeof(catMainLoadMPGamePath), "%s%s", "SavedGames/MultiPlayer/", string);
    return TRUE;
}

bool32 EnablePacketPlay(char *string)
{
    transferCaptaincyDisabled = TRUE;
    strcpy(recordPacketFileName, strtok(NULL, TS_Delimiters));
    return TRUE;
}

bool32 EnablePacketRecord(char *string)
{
    debugPacketRecord = TRUE;
    return TRUE;
}

bool32 EnableDebugSync(char *string)
{
    recordPackets = TRUE;
    logEnable = LOG_VERBOSE;
    autoSaveDebug = TRUE;
    return TRUE;
}

bool32 EnableAutoSaveDebug(char *string)
{
    autoSaveDebug = TRUE;
    return TRUE;
}

bool32 EnableMiniRes(char* string)
{
    selectedRES = TRUE;
    mainWindowWidth  = 320;
    mainWindowHeight = 240;
    return TRUE;
}

bool32 EnableLoRes(char *string)
{
    selectedRES = TRUE;
    mainWindowWidth  = 640;
    mainWindowHeight = 480;
    return TRUE;
}

bool32 EnableHiRes(char *string)
{
    selectedRES = TRUE;
    mainWindowWidth  = 800;
    mainWindowHeight = 600;
    return TRUE;
}

bool32 EnableMegaRes(char *string)
{
    selectedRES = TRUE;
    mainWindowWidth  = 1024;
    mainWindowHeight = 768;
    return TRUE;
}

bool32 EnableUltraRes(char* string)
{
    selectedRES = TRUE;
    mainWindowWidth  = 1280;
    mainWindowHeight = 1024;
    return TRUE;
}

bool32 EnableInsaneRes(char* string)
{
    selectedRES = TRUE;
    mainWindowWidth  = 1600;
    mainWindowHeight = 1200;
    return TRUE;
}

bool32 Enable32Bit(char* string)
{
    selectedRES = TRUE;
    MAIN_WindowDepth = 32;
    return TRUE;
}

bool32 Enable16Bit(char* string)
{
    selectedRES = TRUE;
    MAIN_WindowDepth = 16;
    return TRUE;
}

bool32 Enable24Bit(char* string)
{
    selectedRES = TRUE;
    MAIN_WindowDepth = 24;
    return TRUE;
}

#ifdef GOD_LIKE_SYNC_CHECKING

#define GUESS_NUM_SHIPS     400
bool32 syncDumpInit(char *string1)
{
    if(sscanf(string1,"%d!%d",&syncDumpWindowSize,&syncDumpGranularity) != 2)
    {
        return FALSE;
    }
    syncDumpOn = TRUE;
    syncDumpWindowPos=0;
    syncDumpGranTrack=0;

    //MemoryHeapSize += sizeof(Ship)*400*syncDumpWindowSize;
    return TRUE;
}
#endif


/*-----------------------------------------------------------------------------
    Structures used for command paramters and help
-----------------------------------------------------------------------------*/
#define entryFn(command, func, help)        {COF_Visible, command, func, NULL, 0, help}
#define entryVr(command, var, value, help)  {COF_Visible, command, NULL, &var, value, help}
#define entryFV(command, func, var, value, help)   {COF_Visible, command, func, &var, value, help}
#define entryFnParam(command, func, help)        {COF_Visible | COF_NextToken, command, func, NULL, 0, help}
#define entryFVParam(command, func, var, help)   {COF_Visible | COF_NextToken, command, func, &var, value, help}
#define entryFnHidden(command, func, help)        {0, command, func, NULL, 0, help}
#define entryVrHidden(command, var, value, help)  {0, command, NULL, &var, value, help}
#define entryFVHidden(command, func, var, value, help)   {0, command, func, &var, value, help}
#define entryFnParamHidden(command, func, help)        {COF_NextToken, command, func, NULL, 0, help}
#define entryFVParamHidden(command, func, var, help)   {COF_NextToken, command, func, &var, value, help}
#define entryComment(comment)               {COF_Visible, comment, NULL, NULL, 0, NULL}
commandoption commandOptions[] =
{
#ifdef HW_BUILD_FOR_DEBUGGING
    entryComment("DEBUGGING OPTIONS"),//-----------------------------------------------------
    entryVr("/debug",               DebugWindow, TRUE,                  " - Enable debug window."),
    entryVr("/nodebugInt",          dbgAllowInterrupts, FALSE,          " - Fatal errors don't generate an interrupt before exiting."),
#if DBW_TO_FILE
    entryVr("/debugToFile",         debugToFile, TRUE,                  " - output debugging info to a file."),
#endif
#else
    entryVrHidden("/debug",         DebugWindow, TRUE,                  " - Enable debug window."),
    entryVrHidden("/nodebugInt",    dbgAllowInterrupts, FALSE,          " - Fatal errors don't generate an interrupt before exiting."),
#if DBW_TO_FILE
    entryVrHidden("/debugToFile",   debugToFile, TRUE,                  " - output debugging info to a file."),
#endif
#endif
#if RANDOM_DEBUG_CALL_SEQ
    entryVr("/ranCallerDebug",      ranCallerDebug, TRUE,               " - debug non-deterministic calling of random numbers."),
#endif
#ifdef HW_BUILD_FOR_DEBUGGING
    entryFn("/autosavedebug",       EnableAutoSaveDebug,                " autosaves game frequently"),
#endif

    entryComment("SYSTEM OPTIONS"), //-----------------------------------------------------
    entryFnParam("/heap",           HeapSizeSet,                        " <n> - Sets size of global memory heap to [n]."),
    entryFnParam("/bigoverride",    fileOverrideBigPathSet,             " <path> - Sets path to search for opening files."),
    // CAT: the exe's name of that switch (row 0x008b88e8, cat_mainPrependPathSet 0x0055f3d0)
    entryFnParam("/prepath",        cat_mainPrependPathSet,             " <path> - Sets path to search for opening files."),
    entryFnParam("/CDpath",         cat_mainCDROMPathSet,               " <path> - Sets path to CD-ROM in case of ambiguity."),
    entryFnParam("/settingspath",   fileUserSettingsPathSet,            " <path> - Sets the path to store settings, saved games, and screenshots (defaults to ~/.homeworld)."),
#if MAIN_MOUSE_FREE
#ifdef HW_BUILD_FOR_DEBUGGING
    entryVr("/freemouse",           startupClipMouse, FALSE,            " - Mouse free to move about entire screen at startup.  Use <CTRL>F11 to toggle during play."),
#else
    entryVrHidden("/freemouse",     startupClipMouse, FALSE,            " - Mouse free to move about entire screen at startup.  Use <CTRL>F11 to toggle during play."),
#endif
#endif
    entryVr("/ignoreBigfiles",      IgnoreBigfiles, TRUE,               " - don't use anything from bigfile(s)"),
#ifdef HW_BUILD_FOR_DEBUGGING
    entryFV("/logFileLoads",        EnableFileLoadLog,LogFileLoads,TRUE," - create log of data files loaded"),
#endif

    entryComment("PROCESSOR OPTIONS"),//-----------------------------------------------------
    entryVr("/enableSSE",           mainAllowKatmai, TRUE,              " - allow use of SSE if support is detected."),
    entryVr("/forceSSE",            mainForceKatmai, TRUE,              " - force usage of SSE even if determined to be unavailable."),
    entryVr("/enable3DNow",         mainAllow3DNow, TRUE,               " - allow use of 3DNow! if support is detected."),

    entryComment("SOUND OPTIONS"),  //-----------------------------------------------------
#if SE_DEBUG
    entryVr("/noSound",             enableSFX, FALSE,                   " - turn all sound effects off."),
    entryVr("/noSpeech",            enableSpeech, FALSE,                " - turn all speech off."),
#endif
    entryVr("/dsound",              useDSound, TRUE,                    " - forces mixer to write to DirectSound driver, even if driver reports not certified."),
    entryVr("/dsoundCoop",          coopDSound, TRUE,                   " - switches to co-operative mode of DirectSound (if supported) to allow sharing with other applications."),
    entryVr("/waveout",             useWaveout, TRUE,                   " - forces mixer to write to Waveout even if a DirectSound supported object is available."),
    entryVr("/reverseStereo",       reverseStereo, TRUE,                " - swap the left and right audio channels."),

    entryComment("DETAIL OPTIONS"), //-----------------------------------------------------
    entryFn("/rasterSkip",          EnableRasterSkip,                   " - enable interlaced display with software renderer."),
    entryVr("/noBG",                showBackgrounds, FALSE,             " - disable display of galaxy backgrounds."),
    entryFn("/noFilter",            NoFilterSet,                        " - disable bi-linear filtering of textures."),   // HWI-FEATURE(sharp-textures): run-only (the exe's entryVr: the start's effects preset undid it and the save kept the preset's)
    entryVr("/noSmooth",            enableSmoothing, FALSE,             " - do not use polygon smoothing."),
    // CAT: Cataclysm's switch (command-line table row 0x008b8a80, help string 0x008b9694)
    entryVr("/noCPV",               enableCPV, FALSE,                   " - do not use vertex CPV."),
#if TR_NIL_TEXTURE
    entryVr("/nilTexture",          GLOBAL_NO_TEXTURES,TRUE,            " - don't ever load textures at all."),
#endif
#if ETG_DISABLEABLE
    entryVr("/noEffects",           etgEffectsEnabled,FALSE,            " - disable all effects (Debug only)."),
#endif
#if FEF_TEXTURES_DISABLABLE
    entryVr("/NoFETextures",        fetEnableTextures, FALSE,           " - turns off front end textures"),
#endif
    entryVr("/stipple",             enableStipple, TRUE,                " - enable stipple alpha with software renderer."),
    entryVr("/noShowDamage",        gShowDamage, FALSE,                 " - Disables showing ship damage effects."),
    entryVr("/msaa",                enableMSAA, TRUE,                   " - Enable multisample anti-aliasing (MSAA)."),

    entryComment("VIDEO MODE OPTIONS"),//-----------------------------------------------------
    entryVr("/safeGL",              mainSafeGL, TRUE,                   " - don't use possibly buggy optimized features of OpenGL for rendering."),
    entryFn("/triple",              EnableDoubleIsTriple,               " - use when frontend menus are flickering madly."),
    entryVr("/nodrawpixels",        mainNoDrawPixels, TRUE,             " - use when background images don't appear while loading."),
    entryVr("/noswddraw",           mainSoftwareDirectDraw, FALSE,      " - don't use DirectDraw for the software renderer."),
    entryVr("/noglddraw",           mainDirectDraw, FALSE,              " - don't use DirectDraw to setup OpenGL renderers."),
    entryVr("/sw",                  mainForceSoftware, TRUE,            " - reset rendering system to defaults at startup."),
    entryVrHidden("/noSavedMode",   mainAutoRenderer, FALSE,            " - disable recovery of previous display mode."),
    entryFn("/noFastFE",            DisableFastFrontend,                " - disable fast frontend rendering."),
    entryVr("/fullscreen",          fullScreen, TRUE,                   " - display fullscreen with software renderer (default)."),
    entryVr("/window",              fullScreen, FALSE,                  " - display in a window."),
    entryVr("/noBorder",            showBorder, FALSE,                  " - no border on window."),
    entryFnHidden("/minny",           EnableMiniRes,                      " - run at 320x240 resolution."),
    entryFn("/640",                 EnableLoRes,                        " - run at 640x480 resolution (default)."),
    entryFn("/800",                 EnableHiRes,                        " - run at 800x600 resolution."),
    entryFn("/1024",                EnableMegaRes,                      " - run at 1024x768 resolution."),
    entryFn("/1280",                EnableUltraRes,                     " - run at 1280x1024 resolution."),
    entryFn("/1600",                EnableInsaneRes,                    " - run at 1600x1200 resolution."),
//    entryFn("/d16",                 Enable16Bit,                        " - run in 16 bits of colour."),
//    entryFn("/d24",                 Enable24Bit,                        " - run in 24 bits of colour."),
//    entryFn("/d32",                 Enable32Bit,                        " - run in 32 bits of colour."),
//    entryVr("/truecolor",           trueColor, TRUE,                    " - try 24bit modes before 15/16bit."),
//    entryVr("/slowBlits",           slowBlits, TRUE,                    " - use slow screen blits if the default is buggy."),
    entryFnParam("/device",         SelectDevice,                       " <dev> - select an rGL device by name, eg. sw, fx."),
//    entryFV("/gl",                  SelectMSGL, selectedGL, TRUE,       " - select default OpenGL as renderer."),
    entryVr("/nohint",              mainNoPerspective, TRUE,            " - disable usage of OpenGL perspective correction hints."),
    entryVrHidden("/noPause",             noPauseAltTab, TRUE,                " - don't pause when you alt-tab."),
    entryVrHidden("/noMinimize",          noMinimizeAltTab, TRUE,             " - don't minimize when you alt-tab."),
    // CAT: the exe's hidden /d3dDeviceCRC row (0x008b8c00: the device CRC file switch, rinit.c's mainOutputCRC)
    entryVrHidden("/d3dDeviceCRC",  mainOutputCRC, TRUE,                " - generate d3dDeviceCRC.txt for video troubleshooting."),

#ifdef HW_BUILD_FOR_DEBUGGING
    entryComment("CHEATS AND SHORTCUTS"),         //-----------------------------------------------------
#if CM_CHEAP_SHIPS
    entryVr("/cheapShips",          cmCheapShips, TRUE,                 " - ships only cost 1 RU."),
#endif
#if MAIN_SENSOR_LEVEL
    entryFnParam("/sensorLevel" ,   InitialSensorLevelSet,              " <n> - set initial sensors level (0.. 2).  Default is 0."),
#endif
    entryVr("/noCompPlayer",        noDefaultComputerPlayer, TRUE,      " - disable default computer players"),
#ifdef DEBUG_TACTICS
    entryVr("/notactics",           tacticsOn, FALSE,                   " - Disables tactics."),
#endif
    entryVr("/noretreat",           noRetreat, TRUE,                    " - disables the 'retreat' feature of tactics"),
#endif
    entryVrHidden("/disableAVI",    enableAVI,FALSE,                    " - don't display intro sequences."),
    // CAT: the exe's multiplayer save switches (rows 0x008b8d38 / 0x008b8d50: the autosave switch 0x00a74dac of
    //      univUpdate, UnivUpdate.c; the file of cat_mainLoadMPGameSet 0x0055f500)
#ifndef HW_BUILD_FOR_DEBUGGING
    // CAT: the exe's release table has /aiplayerLog (row 0x008b8d20, visible; HW1 only in debug builds)
    entryVr("/aiplayerLog",         aiplayerLogEnable, TRUE,            " - enable AI Player Logging"),
#endif
    entryVr("/SaveMPGames",         catMultiplayerAutoSave, TRUE,       " - periodically save multiplayer games"),
    entryFnParam("/LoadMPGame",     cat_mainLoadMPGameSet,              "..."),

#if RND_VISUALIZATION
    entryComment("VISUALIZATION"),  //-----------------------------------------------------
    entryVr("/dockLines",           dockLines, TRUE,                    " - show dock lines."),
    entryVr("/gunLines",            gunLines, TRUE,                     " - show gun lines."),
    entryVr("/lightLines",          RENDER_LIGHTLINES, TRUE,            " - show light lines (Debug only)."),
    entryVr("/boxes",               RENDER_BOXES, TRUE,                 " - render bounding bowties on the ships."),
#endif
#if CL_TEXTFEEDBACK
    entryVr("/textFeedback",        enableTextFeedback, TRUE,           " - enable text feedback for in game commands."),
#endif
#if TR_DEBUG_TEXTURES
    entryVr("/specialTextures",     trSpecialTextures, TRUE,            " - enable special debugging textures."),
#endif
#if MESH_MORPH_DEBUG
    entryVr("/morphDebug",          meshMorphDebug, TRUE,               " - enable debugging of morphed mesh rendering code."),
#endif
#if LOD_SCALE_DEBUG
    entryFnParam("/lodScaleDebug",  EnableLodScaleDebug,                " - enable fixing a LOD scale factor."),
#endif
#if MR_CAN_FOCUS_ROIDS
    entryVr("/focusRoids",          mrCanFocusRoids, TRUE,              " - enable focussing on asteroids and dust clouds."),
#endif
#if PIE_VISUALIZE_EXTENTS
    entryVr("/showExtents",         pieVisualizeExtents, TRUE,          " - draw elliptical universe extents."),
#endif
#if UNIV_SHIP_LOADFREE_LOG
    entryVr("/loadFreeLog",         univLoadFreeLog, TRUE,              " - enable logging of what was loaded and freed between missions."),
#endif
    // CAT: no /NoBind (the exe's table 0x008b8870 has none; HW1's key remap, bkDisableKeyRemap, is gone with its tables)

#ifdef HW_BUILD_FOR_DEBUGGING
    entryComment("COMPUTER PLAYER AND STATS"),//-----------------------------------------------------
//    {"/compPlayer",  EnableComputerPlayers, "=01234567 to enable all computer players"},
    entryVr("/aiplayerLog",         aiplayerLogEnable, TRUE,            " - enable AI Player Logging"),
    entryVr("/determCompPlayer",    determCompPlayer, TRUE,             " - makes computer players deterministic"),
    entryFV("/gatherStats",         EnableGatherStats, gatherStats, TRUE,"- enable gathering of stats"),
    entryFnParam("/showStatsFight", EnableShowStatsFight,               "=<i,j> to show stats fight i,j"),
    entryFnParam("/showStatsFancyFight", EnableShowStatsFancyFight,     "=filename.script"),
#endif

#ifdef HW_BUILD_FOR_DEBUGGING
    entryComment("NETWORK PLAY"),   //-----------------------------------------------------
    //entryVr("/captaincyLogOff",     captaincyLogEnable, FALSE,          " - turns off captaincy log file" ),
    //entryVr("/captaincyLogOn",      captaincyLogEnable, TRUE,           " - turns on captaincy log file" ),
    entryVr("/logOff",              logEnable, LOG_OFF,                 " - turns of network logging file"),
    entryVr("/logOn",               logEnable, LOG_ON,                  " - turns network logging file on"),
    entryVr("/logOnVerbose",        logEnable, LOG_VERBOSE,             " - turns verbose network logging file on"),
    entryFnParam("/logFilePath",    SpecifyLogFilePath,                 "=filepath.txt"),
    entryFn("/debugSync",           EnableDebugSync,                    " autosaves game frequently, records packets, logonverbose" ),
    entryVrHidden("/noWon",         SecretWON, TRUE,                    " - no WON stuff" ),
    entryVr("/forceLAN",            forceLAN, TRUE,                     " - allow LAN play regardless of version" ),
    entryVrHidden("/noAuth",        noAuthorization, TRUE,              " - Disables WON Login"),
    entryVrHidden("/shortWon",      ShortCircuitWON, TRUE,              " - short circuit WON stuff" ),
#else
    entryVrHidden("/logOff",        logEnable, LOG_OFF,                 " - turns of network logging file"),
    entryVrHidden("/logOn",         logEnable, LOG_ON,                  " - turns network logging file on"),
    entryVrHidden("/logOnVerbose",  logEnable, LOG_VERBOSE,             " - turns verbose network logging file on"),
    entryFnParamHidden("/logFilePath",SpecifyLogFilePath,               "=filepath.txt"),
//!!!shortwon in release mode!!!    entryVrHidden("/shortWon",      ShortCircuitWON, TRUE,              " - short circuit WON stuff" ),
#endif
#ifdef DEBUG_GAME_STATS
    entryVr("/statLogOn",           statLogOn, TRUE,                    " - generates game stats log file"),
#endif
#ifdef GOD_LIKE_SYNC_CHECKING
    entryFnParam("/BryceAndDrewAreGods", syncDumpInit,                  "=<X>!<Y>   X = size of SyncDumpWindow  Y = granularity in universe Frames"),
#endif

#if NIS_PRINT_INFO
    entryComment("NIS OPTIONS"),    //-----------------------------------------------------
#endif
#if NIS_TEST
    entryFn("/testNIS" ,            TestNISSet,                         " <nisFile> - enables NIS testing mode using [nisFile]."),
    entryFn("/testNISScript",       TestNISScriptSet,                   " <scriptFile> - enables NIS testing mode using [scriptFile]."),
#endif
#if NIS_PRINT_INFO
    entryVr("/nisCounter",          nisPrintInfo,TRUE,                  " - display nis time index info by default."),
    entryVr("/nisNoLockout",        nisNoLockout, TRUE,                 " - don't lock out the interface when playing an NIS."),
#endif

/*
    entryComment("RECORDED DEMOS"), //-----------------------------------------------------
    entryFnParam("/demoRecord",     EnableDemoRecord,                   " <fileName> - record a demo."),
    entryFnParam("/demoPlay",       EnableDemoPlayback,                 " <fileName> - play a demo."),
*/
#ifdef HW_BUILD_FOR_DEBUGGING
    entryFV("/packetRecord",        EnablePacketRecord, recordPackets, TRUE, " - record packets of this multiplayer game"),
    entryFV("/packetPlay",          EnablePacketPlay, playPackets, TRUE," <fileName> - play back packet recording"),
#else
    entryFVHidden("/packetRecord",  EnablePacketRecord, recordPackets, TRUE, " - record packets of this multiplayer game"),
    entryFVHidden("/packetPlay",    EnablePacketPlay, playPackets, TRUE," <fileName> - play back packet recording"),
#endif

    //entryVr("/compareBigfiles",     CompareBigfiles, TRUE,              " - file by file, use most recent (bigfile/filesystem)"),
#if DEM_AUTO_DEMO
    entryVr("/disableAutoDemos",    demAutoDemo,FALSE,                  " - don't automatically play demos."),
    entryFnParam("/autoDemoWait",   AutoDemoWaitSet,                    " <seconds> - time to wait on main screen before starting a demo."),
#endif
#if DEM_FAKE_RENDER_SWITCH
    entryVr("/disableFakeRenders",  demFakeRenders,FALSE,               " - disable feature where playback will try to keep up with recorded demo."),
#endif

    entryComment("TEXTURES"),       //-----------------------------------------------------
    entryVr("/nopal",               mainNoPalettes, TRUE,               " - disable paletted texture support."),
/*
    entryVrHidden("/allowPacking",  mainAllowPacking, TRUE,             " - use the packed textures if available (default)."),
    entryVr("/disablePacking",      mainAllowPacking, FALSE,            " - don't use the packed textures if available."),
#ifdef HW_BUILD_FOR_DEBUGGING
    entryVr("/onlyPacking",         mainOnlyPacking, TRUE,              " - only display packed textures."),
#endif
*/

    entryComment("MISC OPTIONS"),   //-----------------------------------------------------
    entryVrHidden("/smCentreCamera",      smCentreWorldPlane, FALSE,          " - centres the SM world plane about 0,0,0 rather than the camera."),

    entryVrHidden("/closeCaptioned",      subCloseCaptionsEnabled, TRUE,      " - close captioned for the hearing impared."),
    entryVr("/pilotView",           pilotView, TRUE, " - enable pilot view.  Focus on single ship and hit Q to toggle."),

    entryVr("/debugScreenshots",    debugScreenshots, TRUE, " - take screenshots from all single-player missions for render debugging then exit."),
    entryVr("/vanilla",              vanillaMode, TRUE, " - restore original engine behaviour (gun gimbals; the universe runs at the original 16 Hz unless /gok64Hz)."),
    // CAT: 16 Hz universe by default (Universe.c); GoK's 64 Hz only behind this switch (user decision 2, docs/m2-r2-oracle.md)
    entryVr("/gok64Hz",             gok64HzMode, TRUE, " - run the universe at 64 Hz (Gardens of Kadesh default) instead of the original 16 Hz."),
    // CAT: port diagnostic, not a Cataclysm option (Universe.c catShipStaticsRunSelfTest)
    entryVr("/catShipStaticsTest",  catShipStaticsSelfTest, TRUE, " - load the .shp of every ship type, log one line per file, then exit."),
    entryFnParam("/catShipStaticsSkip", CatShipStaticsSkipSet, " <n,n,...> - ship types /catShipStaticsTest leaves out."),
    entryVr("/catShipStaticsStaticInit", catShipStaticsRunStaticInit, TRUE, " - with /catShipStaticsTest, also run each type's CustShipStaticInit."),
    // CAT: port diagnostic, not a Cataclysm option (Mesh.c catMeshRunSelfTest)
    entryFnParam("/catMeshLoadTest",    CatMeshLoadTestSet, " <listfile> - load every mesh named in listfile, log one line per file, then exit."),
    // CAT: port diagnostics, not Cataclysm options (ETG.c catEtgRunParseSelfTest, catEtgRunEffectSelfTest)
    entryFnParam("/catEtgParseTest",    CatEtgParseTestSet, " <listfile> - start the effects module and load every etg script and effect named in listfile, then exit."),
    entryFnParam("/catEtgEffectTest",   CatEtgEffectTestSet, " <ticks> - with /catEtgParseTest: create a sample of the effects and run them for <ticks> universe ticks."),
    // CAT: port diagnostic, not a Cataclysm option (NIS.c catNisRunParseSelfTest, M2-R6 nis-fow-camera)
    entryFnParam("/catNisParseTest",    CatNisParseTestSet, " <listfile> - load every NIS with its script and every animatic script named in listfile, check them, then exit."),
    entryFnParam("/catDockTest",        CatDockTestSet, " <ticks> - headless Mission01 run: dock, deposit, harvest and launch orders with no input, exit after <ticks> ticks."),
    entryFnParam("/catAiTest",          CatAiTestSet, " <ticks> - headless Mission01 run: the computer player (player 1) gets a scripted factory, RUs and fleet and fleet control with no input, logs aitest lines (teams, orders, moves, commands), exit after <ticks> ticks."),
    entryFnParam("/catBuildTest",       CatBuildTestSet, " <ticks> - headless Mission01 run: the construction manager builds a docking bay module, a destroyer (both built externally) and an acolyte at a scripted mothership with no input, logs buildtest lines, exit after <ticks> ticks."),
    entryFnParam("/catMissionTest",     CatMissionTestSet, " <ticks> - headless single-player game from Mission01's NIS-first start: the opening NIS, the KAS objectives with scripted events in place of input, logs missiontest lines, exit after the mission's end or <ticks> ticks."),
    entryFnParam("/catLoadGameTest",    CatLoadGameTestSet, " <ticks> - headless load of the single-player save /catLoadGameName names (SavedGames/SinglePlayer), as the front end's Load Game, then <ticks> ticks; logs loadgametest lines, exit."),
    entryFnParam("/catLoadGameName",    CatLoadGameNameSet, " <name> - the save /catLoadGameTest loads."),
    entryFnParam("/catInfectAt",        CatInfectAtSet, " <tick>:<ShipType> - test: with /catLoadGameTest, that many ticks after the load infect the player's first ship of that type as a Beast infection missile's hit would (infecttest: lines)."),   // HWI-TEST (M3-R4 infect-convert)
    entryFnParam("/catTooltipAt",       CatTooltipAtSet, " <tick>:<ShipType> - test: with /catMissionTest, at that universe tick put the first ship of that type under the cursor as the mouse code does and log its tooltip text (tooltiptest: lines)."),   // HWI-TEST (M3-R4 crystal-tooltip)
    entryFn("/catNaNProbe",         CatNaNProbeEnable,                  " - test: log the first NaN in any live ship's physics state after each phase of univUpdate (nanprobe: lines)."),   // HWI-TEST (M3-R4 nan-positions)
    entryFnParam("/catSoundNoiseAt",   CatSoundNoiseAtSet, " <block> - test: from that mixer block, 12 blocks of white noise into the first playing stream or channel (the soundlog: burst detector's fence)."),   // HWI-TEST (M3-R4 sound-burst-log)
    entryFn("/catMadProbe",         CatMadProbeEnable,                  " - test: log a mesh animation's simulation pose and its render-time pose (madprobe: lines, the first 600) in every interpolated frame"),
    entryFn("/catLoadGameTraining",     CatLoadGameTrainingSet, " - with /catLoadGameTest: the save is a tutorial lesson's (SavedGames/Training)."),
    entryFnParam("/catMissionTestMission", CatMissionTestMissionSet, " <N> - with /catMissionTest: mission N (2 .. 17) from the exe's warp start (Warp.script's level N record) through its KAS objectives and the jump to the next level."),
    entryFn("/catWarpStartTest",      CatWarpStartTestSet, " - with /catMissionTest: the warp start of the data dir's warp.script (WarpEnabled 1) checked against its record, through the fleet's arrival."),
    entryFn("/catMissionTestTrace",   CatMissionTestTraceSet, " - with /catMissionTest: every new KAS message logged (missiontest: msg lines; a diagnostic)."),
    entryFnParam("/catMissionTestHoldScout", CatMissionTestHoldScoutSet, " <tick> - with /catMissionTest and /catMissionTestMission 2: the FindDestroyer recon waits until universe tick <tick> (the BUSHAN-RE hint's timer can run out; missiontest: hint lines)."),
    entryFn("/catMissionTestFailJump", CatMissionTestFailJumpSet, " - with /catMissionTest and /catMissionTestMission N (a roll-call jump): the first jump fails (DisablePlayerHyperspace through the console's KAS call), its return is judged and the run ends 32 ticks later (no second jump)."),
    entryFn("/catVirtualAudio",       CatVirtualAudioSet, " - with a virtual-clock run: the mixer and the speech stream advanced by the virtual clock on the game thread (the tutorial test sets it)."),
    entryFnParam("/catMissionTestTutorial", CatMissionTestTutorialSet, " <ticks> - headless tutorial start: Tutorial1's KAS script runs until its first prompt shows, logs missiontest lines, exit."),
    entryFnParam("/catSkirmishTest",    CatSkirmishTestSet, " <ticks> - start a 2-player skirmish (Sect against a computer player) through the front end's own callbacks with no input, run <ticks> universe ticks on the virtual clock, judge the universe, log skirmishtest lines, exit."),
    entryFnParam("/catSkirmishMap",     CatSkirmishMapSet, " <name> - the map of /catSkirmishTest (a MultiPlayer directory name; default LiveFireExercise2)."),
    entryFnParam("/catSkirmishBigotry", CatSkirmishBigotrySet, " <0..2> - with /catSkirmishTest: the skirmish screen's CPU-attacks radio button (tpGameCreated.aiplayerBigotry) pressed before the start; 2 makes the computers ally (cat_aisSetupAlliances)."),
    entryFnParam("/catCommandTest",     CatCommandTestSet, " <ticks> - headless Mission01 run: link, unlink, mimic, research, point orders and force-field formations with no input, exit after <ticks> ticks."),
    entryFnParam("/catHudTest",         CatHudTestSet, " <frames> - headless Mission01 run with the player's ships selected through the selection API: status bars, ship info panel, main overlay and pie plate drawn offscreen, logs the HUD counts, exit after <frames> ticks."),
    // CAT: port diagnostics, not Cataclysm options (FEFlow.c catFeScreensRunSelfTest, StringSupport.c
    //      catStringsRunSelfTest; M2-R6 frontend)
    entryFnParam("/catFeScreensTest",   CatFeScreensTestSet, " <listfile> - push, draw and pop every screen of the .fib files named in listfile (archive<TAB>name lines), then exit."),
    entryVr("/catStringsTest",      catStringsSelfTest, TRUE, " - resolve every string id of the language script against the script file, then exit."),
    // CAT: port diagnostic, not a Cataclysm option (KeyBindings.c catKeysRunSelfTest, M2-R6 startup-keys)
    entryFnParam("/catConsoleTest",     CatConsoleTestSet, " <ticks> - headless Mission01 run: the developer console opened, every row of its command table shown, set and run, typed lines, TAB and the history, its config file, logs consoletest lines and quits."),
    entryVr("/catKeysTest",         catKeysSelfTest, TRUE, " - load, save, reload and reset the key bindings from the install's default_keys.cfg, then exit."),
    entryFnParam("/catUiTest",          CatUiTestSet, " <ticks> - headless Mission01 run: the player's fleet selected, the popup menu opened, every cat_ui* command run once, the game-stats screen shown, logs uitest lines and quits."),
    entryFn("/noInterp",                NoInterpSet, " - render-time motion interpolation off (objects, camera and NIS; the Smooth motion option)."),   // HWI-FEATURE(motion-interp)
    entryFn("/noNISInterp",             NoNISInterpSet, " - render-time interpolation of NIS cutscenes off (gameplay interpolation stays)."),   // HWI-FEATURE(motion-interp)
    entryFn("/noHsAssembly",            NoHsAssemblySet, " - this run only: a modular ship (the Kuun-Lan) hyperspaces as the exe draws it: the gate and the reveal sized from its core hull, its modules shown at their own exits."),   // HWI-FEATURE(hyperspace-assembly)
    entryFn("/meshImmediate",           MeshImmediateSet, " - this run only: the ship meshes drawn one GL call per vertex (the old immediate-mode path) instead of in vertex arrays: the A/B of the mesh batching (the console row meshImmediate toggles it)."),   // HWI-FEATURE(mesh-batching)
    entryFn("/exeTrails",               ExeTrailsSet, " - this run only: the missile corvettes' two engine ribbons as cataclysm.exe draws them (one per mex Eng chunk) instead of the four jet streams from the engine circles (the console row engineTrails toggles it)."),   // HWI-FEATURE(engine-trails)
    entryFn("/exePoints",               ExePointsSet, " - this run only: the sensors manager's points at cataclysm.exe's sizes (the Dust 1 pixel, the resource, mine and crate markers 2) instead of the Dust 2 and the markers 3 (the console row sensorsPoints toggles it)."),   // HWI-FEATURE(sensors-points)
    entryFn("/sharpTextures",           NoFilterSet, " - this run only: SHARP TEXTURES (the ship, derelict and asteroid textures magnified as square texels: texLinearFiltering 0, as /noFilter; the options file keeps its row)."),   // HWI-FEATURE(sharp-textures)
    entryFn("/lodBands",                LodBandsSet, " - this run only: FULL DETAIL off (the exe's levels of detail by size on screen and frame time; the options file's noLOD row keeps its value)."),   // HWI-FEATURE(full-detail)
    entryFnParam("/catLodTest",         CatLodTestSet, " <tick>[:far][:fleet] - log every object's level of detail (the exe's and the one drawn) in the main view's first frame of that universe tick (\"lodtest:\" lines); :far pulls the camera out to its farthest zoom, :fleet closes the sensors manager at tick 16 and focuses the camera on every ship."),   // HWI-TEST
    entryFn("/overlay",                 OverlaySet, " - this run only: the on-screen overlay on (frame time, universe tick, interpolation alpha, the settings, the vertices drawn; the console row overlay, F1 and the Menu key toggle it)."),   // HWI-FEATURE(instrumentation)
    entryFnParam("/catOverlayKeyAt",    CatOverlayKeyAtSet, " <tick>[,<tick>..] - test: F1 (the overlay's key) handed to the event handler at those universe ticks."),   // HWI-TEST
    entryFnParam("/catConsoleAt",       CatConsoleAtSet, " <tick>:<row>=<value>[,..] - test: a developer-console row set at those universe ticks."),   // HWI-TEST
    entryFnParam("/catTexList",         CatTexListSet, " <substring> - test: at each texture-filter pass, list the live textures whose name has it (on-disk and loaded size)."),   // HWI-TEST
    entryFnParam("/catMeshTexLog",      CatMeshTexLogSet, " <substring> - test: log the texture registration and the first draw per colour scheme of the meshes whose file name has it (meshtex: lines)."),   // HWI-TEST (M3-R3 mines)
    entryFnParam("/catMineView",        CatMineViewSet, " <tick>[:dropped][:<distance>] - test: from that universe tick close the sensors manager and keep the camera on the level mine (or with :dropped a laid mine) nearest the player's mothership, at that camera distance (mineview: lines)."),   // HWI-TEST (M3-R3 mines)
    entryFnParam("/catTrailView",       CatTrailViewSet, " <tick>:<ShipType>[:d<distance>][:e<elevation>][:s<side>][:r<ratio>][:m<move>] - test: from that universe tick close the sensors manager and keep the camera behind the first ship of that type moving at over <ratio> (default 0.4; 0: any) of its top speed, :m orders it <move> units ahead (trailview: lines with each trail's head and engine circle on the screen)."),   // HWI-TEST (M3-R3 engine-trails)
    entryFnParam("/catMineLog",         CatMineLogSet, " <ticks> - test: log every mine state change with its reason and <ticks> ticks of state, target, distance and speeds for the first 8 mines that start an attack run on a target (minelog: lines)."),   // HWI-TEST (M3-R3 mines)
    entryFnParam("/catMineChase",       CatMineChaseSet, " <tick>[:<shiptype>] - test: at that universe tick order up to 4 of player 0's ships of that type (default sRecon) past the nearest level mine, 2000 beside it and 20000 beyond (minechase: line)."),   // HWI-TEST (M3-R3 mines)
    entryFnParam("/catFeScreen",        CatFeScreenSet, " <screen>[@<tick>] - test: a front-end screen pushed once (in the front end, or in game at the tick)."),   // HWI-TEST
    entryFnParam("/catModeQuitTest",    CatModeQuitTestSet, " <W>x<H> - test: once the front end is up, the Video screen's render switch to that resolution, then one quit."),   // HWI-TEST
    entryFnParam("/frameLog",           FrameLogSet, " <file> - test: one line a second to <file>: frame-time percentiles, frames over the refresh interval, universe ticks and their cost, the main heap, the vertices drawn."),   // HWI-TEST
    entryFnParam("/catLodStats",        CatLodStatsSet, " <from>-<to>[:fleet] - per frame of those universe ticks: the render time (to the flip, glFinish), the objects and the mesh vertices and polygons drawn (\"lodstats:\" lines, the median and 99th percentile at the end)."),   // HWI-TEST
    entryFnParam("/uiScale",            UiScaleSet, " <percent> - this run only: the UI scale (0 = by the window height: 200 at 2160 rows or more, 150 at 1440 or more, 100 below; else 100 .. 400; the options file's uiScale row keeps its value)."),   // HWI-FEATURE(ui-scale)
    entryFnParam("/testUsableBounds",   TestUsableBoundsSet, " <W>x<H>[@<DW>x<DH>] - test: the desktop's usable bounds (and display size) the windowed mode's fit uses."),   // HWI-TEST
    entryVr("/catUiScaleTest",          catUiScaleSelfTest, TRUE, " - the UI scale's self-test in the front end: the fit's table, the scripted mouse at the physical bottom row and corner, a click on a front-end button and just outside it, the Video screens' control, the options-file row and the console row, then exit."),   // HWI-TEST
    entryVr("/catResizeTest",           catResizeSelfTest, TRUE, " - the resizable window's self-test in the front end: the Video screen's resolution presets and aspects, scripted size changes (the scale, the logical size, the layout, the aspect, the mouse, the request, the list's own entry and the options file), then exit."),   // HWI-TEST (M3-R4 window-modes)
    entryVr("/catHiddenWindow",         catHiddenWindow, TRUE, " - test: every window is created hidden (never mapped) and the pointer is never warped or grabbed: a run on the real display server (the x11 driver) that nobody sees (the harness's --hidden-x11)."),   // HWI-TEST (M3-R4 mode-switch-fix)
    entryFnParam("/catMovieTest",       CatMovieTestSet, " <level>[:<frames>] - test: once the front end is up, that level's animatic (0 = Animatic01) plays (at most <frames> frames), then quit."),   // HWI-TEST (M3-R4 intro-movies)
    entryFnParam("/catMovieEventAt",    CatMovieEventSet, " <frame>:quit|<W>x<H> - test: once, after that frame of a movie, the window's close request or a new window size (no input event)."),   // HWI-TEST (M3-R4 intro-movies)
    entryFnParam("/catDataSetupScript", CatDataSetupScriptSet, " <answers> - test: the game-data setup's dialogs and pickers answered from this ;-separated list and logged, never shown: folder[:<path>], installer[:<path>], unpack, other, open, retry, quit (show: the unpack's progress window is not hidden)."),   // HWI-TEST (data-setup)
    entryFnParam("/catMovieShot",       CatMovieShotSet, " <frame>[,<frame>...] - test: those frames of every movie (the intro logos, an animatic) are read back into the settings folder as movie-<name>-<frame>.ppm."),   // HWI-TEST (M3-R4 intro-movies)
    entryFnParam("/catResizeAt",        CatResizeAtSet, " <tick>:<W>x<H> - test: at that universe tick the window is treated as resized to WxH (the in-game relayout)."),   // HWI-TEST (M3-R4 window-modes)
    entryFn("/letterboxBehind",         LetterboxBehindSet, " - this run only: the cutscene letterbox bars drawn behind ships near the camera (the old look; the options file's letterboxBehind row keeps its value)."),   // HWI-FEATURE(letterbox-setting)
    entryFnParam("/catMotionTest",      CatMotionTestSet, " <ticks> - headless Mission01 run at the virtual frame rate (/catMotionFps, default 144): a focused, a crossing, a receding and a resting ship, per-frame screen positions to /catMotionDump, exit after <ticks> ticks."),   // HWI-FEATURE(motion-interp)
    entryFnParam("/catNisMotionTest",   CatNisMotionTestSet, " <nis>[:<from>:<to>] - headless Mission01 start, then nis\\<nis>.nis plays at the virtual frame rate; the frames between NIS times <from> and <to> go to /catMotionDump, then exit."),   // HWI-FEATURE(motion-interp)
    entryVr("/catToggleTest",           catToggleSelfTest, TRUE, " - the Smooth motion checkbox clicked, cancelled and accepted, its options-file rows read back, its console rows set, then exit."),   // HWI-FEATURE(motion-interp)
    entryFnParam("/catMotionDump",      CatMotionDumpSet, " <file> - the per-frame dump of /catMotionTest and /catNisMotionTest."),   // HWI-FEATURE(motion-interp)
    entryFnParam("/catMotionShot",      CatMotionShotSet, " <frame> - /catMotionTest, /catNisMotionTest: save that frame of the window as <dump>.<frame>.ppm."),   // HWI-FEATURE(motion-interp)
    entryFnParam("/catMotionFps",       CatMotionFpsSet, " <fps> - the virtual frame rate of /catMotionTest and /catNisMotionTest (default 144)."),   // HWI-FEATURE(motion-interp)
    entryFnParam("/catMotionScene",     CatMotionSceneSet, " <kind>[:<distance>[:<move>[:<ticks>[:<start>]]]] - with another mode's game (e.g. the skirmish test): the camera on a Recon at its zenith, a second Recon moved <move> units ahead (recon; follow: the camera on the mover), the object kinds tracked, per-frame screen positions to /catMotionDump."),   // HWI-FEATURE(motion-interp)
    entryFnParam("/catMotionRealClock", CatMotionRealClockSet, " <fps> - a motion measurement on the wall clock, each frame paced to <fps> (the dump gets the frame times, the task ticks and alpha)."),   // HWI-FEATURE(motion-interp)
    entryFnParam("/catMotionPause",     CatMotionPauseSet, " <frame>:<frames>[:key|univ|nis] - /catMotionTest, /catNisMotionTest: after dump frame <frame> the game is paused (key: the Pause key P; univ: universePause, as the console / Escape menu; nis: the NIS's nisPause) and <frames> dump frames later released; the dump lines end with pause=<universePause><Pause key><nisPaused>."),   // HWI-TEST (M3-R3 pause-freeze)
    entryFnParam("/catShot",            CatShotSet, " <spec> - save frames as PPM (the physical framebuffer before the flip, top row first): <spec> = comma list of <tick>[:<frame>] or <from>-<to>[/<step>][:<frame>] (universe ticks, the n-th frame of the tick)."),   // HWI-TEST
    entryFnParam("/catShotFile",        CatShotFileSet, " <prefix> - /catShot's files are <prefix>.<tick>.<frame>.ppm (default shot in the settings path)."),   // HWI-TEST
    entryFn("/catTeamColourTest",       CatTeamColourTestSet, " - log the player's team-colour state (\"teamcolour:\" lines) before / after each level change and check it (Game/TeamColourTest.c)."),   // HWI-TEST
    entryFnParam("/catTeamColourTicks", CatTeamColourTicksSet, " <ticks> - with /catTeamColourTest in the mission self-test: the ticks run in the next mission level (default 32)."),   // HWI-TEST
    entryFn("/catShotLetterbox",        CatShotLetterboxSet, " - /catShot: only the frames with the NIS / KAS letterbox up."),   // HWI-TEST
    entryFnParam("/catFrameSig",        CatFrameSigSet, " <file> - one line per rendered frame: the main view's object counts, the late letterbox bars, the NIS fade / time / alpha, the frame's wall-clock period and (on the virtual clock only: never in an attended run or on /catMotionRealClock) a read-back's bar / mid pixel counts, luma and its difference to the two frames before (the one-frame outlier scan, tools/dump_outliers.py); the motion / nis dump lines get the same after crc=."),   // HWI-TEST (M3-R2 nis-flicker)
    entryFnParam("/catVirtualFps",      CatVirtualFpsSet, " <fps> - any virtual-clock run (the mission self-test, a mission, a replay): <fps>/16 frames a universe tick instead of one, so the render-time interpolation runs between ticks (a scan: the extra frames draw the shared random stream)."),   // HWI-TEST (M3-R2 nis-flicker)
    entryFnParam("/catNisScissorOut",   CatNisScissorOutSet, " <tick>:<seconds> - /catNisMotionTest: the letterbox fades out from that universe tick as an NIS's ScissorOut event does (a capture of the fade; not for baselines)."),   // HWI-TEST
    entryFnParam("/catRenderTest",      CatRenderTestSet, " <frames> - headless Mission01 run through the Cataclysm object-list renderer with a lens flare source and a fog layer, logs the render counts, exit after <frames> ticks."),
    // CAT: port diagnostic, not a Cataclysm option (SpeechEvent.c catSoundRunSelfTest, M2-R5 sound) [audit: own comment]
    entryVr("/catSoundTest",        catSoundSelfTest, TRUE, " - decode a stream of Music4Cat.wxd, SectSpeech.vce and BeastSpeech.vce into the audio device, look up the 88 battle-chatter events, then exit."),
    // CAT: port diagnostic, not a Cataclysm option (texreg.c catTexturesRunSelfTest)
    entryVr("/catTexturesTest",     catTexturesSelfTest, TRUE, " - load textures.ll and register/load every texture it lists, then exit."),
#if HWI_ORACLE
    // HWI-ORACLE: checksum-oracle stream (Game/OracleDump.c, docs/m2-r2-oracle.md), not a Cataclysm option
    entryFnParam("/oracleDump",     OracleDumpSet, " <file> - write the checksum-oracle stream (.hwo) to file."),
    entryFnParam("/oracleSelfTest", OracleSelfTestSet, " <ticks> - run the oracle's scripted lockstep start for <ticks> universe ticks, then exit."),
    entryFnParam("/oracleSelfTestPerturb", OracleSelfTestPerturbSet, " <tick> - oracle self-test or mission run: draw one extra gamerand() at <tick>."),
    entryFnParam("/oracleShipDump", OracleShipDumpSet, " <ships>[@<from>-<to>] - write shipdump.txt next to the /oracleDump file: per tick each listed ship (numbers, comma-separated, or all) with position, velocity, rotation, coordsys, order and health as bits, and every sync-packet command."),
    entryFnParam("/oracleMission",  OracleMissionSet, " <ticks> - headless oracle run: start single player at once, no animatic, one universe tick per frame, exit after <ticks> ticks."),
    entryVr("/oracleLibmSqrt",      catFsqrtTable, FALSE, " - HWI-EXPERIMENT: fsqrt, fmathSqrt and catFsqrtD use libm's sqrtf instead of the exe's table square root (fsqrt 0x00472860, the default since decision 70)."),
    entryFnParam("/oracleRecord",   OracleRecordSet, " <name> - oracle mode B: record the game from its first universe tick as SavedGames/RecordedGames/<name> (+ .pkts), the game's own in-game recording."),
    entryFnParam("/oracleRecordStop", OracleRecordStopSet, " <tick> - /oracleRecord: stop the recording at this universe tick (default 9600)."),
    entryFnParam("/oracleResave",   OracleResaveSet, " <name> - with /oracleReplay: save the loaded game again at once, in cataclysm.exe's format, as SavedGames/RecordedGames/<name> (a check of the save writer)."),
    entryFnParam("/oracleReplay",   OracleReplaySet, " <name> - oracle mode A: replay SavedGames/RecordedGames/<name> (+ .pkts) as LoadRecordedGame does, one universe tick per frame, exit at the end of the packets."),
#endif

    END_COMMAND_OPTION,
};

/*-----------------------------------------------------------------------------
    Name        : DebugHelpDefault
    Description : Print out command-line usage help to a dialog box
    Inputs      : string - the unrecognized command-line switch that causes this help.
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
char *gHelpString;
void DebugHelpDefault(char *string)
{
    sdword index, length;

    //calc length of help string
    length = strlen("Invalid or unrecognised command line option: '%s'\n");
    length += strlen(string);
    for (index = 0; commandOptions[index].parameter; index++)
    {
        if (!bitTest(commandOptions[index].flags, COF_Visible))
        {
            continue;
        }
        if (commandOptions[index].helpString == NULL)
        {                                                   //no help string: it's a comment
            length += strlen(commandOptions[index].parameter) + 2;//just length of comment
        }
        else
        {                                                   //else it's a real command line option
            length += strlen(commandOptions[index].helpString); //length of help string
            length += strlen(commandOptions[index].parameter) + MCL_IndentSpace;//parameter string
        }
        length += 2;                                        //and a newline
    }

    if ((gHelpString = malloc(length)) == NULL)              //allocate string
    {
        /*MessageBox(NULL, "Cannot allocate memory for help.", "Command-Line Usage", MB_OK | MB_APPLMODAL);*/
        fprintf(stderr, "Cannot allocate memory for help.\n");
        return;
    }

    //copy all help strings into one
    sprintf(gHelpString, "Invalid or unrecognised command line option: '%s'\n", string);
    for (index = 0; commandOptions[index].parameter; index++)
    {
        if (!bitTest(commandOptions[index].flags, COF_Visible))
        {
            continue;
        }
        if (commandOptions[index].helpString == NULL)
        {                                                   //no help string: it's a comment
            strcat(gHelpString, "\n");
            strcat(gHelpString, commandOptions[index].parameter);
            strcat(gHelpString, "\n");
        }
        else
        {                                                   //else it's a real command line option
            sprintf(gHelpString + strlen(gHelpString), "    %s%s\n", commandOptions[index].parameter, commandOptions[index].helpString);
            //                                            ^ MCL_IndentSpace spaces
        }
    }

    /*DialogBox(ghInstance, MAKEINTRESOURCE(IDD_CommandLine), NULL, CommandLineFunction);*/
    printf("%s", gHelpString);
    free(gHelpString);                                       //done with string, free it
}

/*-----------------------------------------------------------------------------
    Name        : ProcessCommandLine
    Description : Process all defined command-lien flags
    Inputs      : argc - Number of command-line arguments
                  argv - Array of command-line arguments
    Outputs     : sets command-line switch variables
    Return      : void
    Remarks:
        Here are the command-line switches for Homeworld:
            -debug: Enable debug window by setting the DebugWindow flag
----------------------------------------------------------------------------*/
sdword ProcessCommandLine (int argc, char* argv[])
{
    char* string;
    sdword index;
    int i;

    for (i = 1; i < argc; i++)  // Skip the executable name.
    {
        string = argv[i];
#ifdef __APPLE__
        if (strncmp(string, "-psn_", 5) == 0)
        {
            continue;                                       // HWI-FIX(macos): the process serial number older Finders pass
        }
#endif
        if (string[0] == '-')
        {
            string[0] = '/';
        }
        for (index = 0; commandOptions[index].parameter; index++)
        {
            if (commandOptions[index].helpString == NULL)
            {                                               //don't compare against comment lines
                continue;
            }
            if (!strcasecmp(string, commandOptions[index].parameter))
            {                                               //if this is the correct option
                dbgAssertOrIgnore(commandOptions[index].variableToModify || commandOptions[index].function);
                if (commandOptions[index].variableToModify != NULL)
                {                                           //set a variable if applicable
                    *((udword *)(commandOptions[index].variableToModify)) = commandOptions[index].valueToSet;
                }
                if (commandOptions[index].function != NULL)
                {                                           //call the function associated, if applicable
                    if (bitTest(commandOptions[index].flags, COF_NextToken))
                    {                                       //does the function take next token as a parameter?
                        if (++i == argc)                    //get next token
                        {                                   //if no next token
                            // CAT: a missing parameter prints the help and ends the start-up (ProcessCommandLine
                            //      0x0055f880; HW1 went on with the next token)
                            DebugHelpDefault(string);
                            return(-1);
                        }
                        string = argv[i];
                    }
                    if(!commandOptions[index].function(string))
                    {
                        //error occured in parsing function
                        // CAT: so does a function that fails (0x0055f880: the help names the failed parameter)
                        DebugHelpDefault(string);
                        return(-1);
                    }
                }
                break;
            }
        }

        if (!commandOptions[index].parameter)
        {
            DebugHelpDefault(string);                       //no string found, print help
            return(-1);
        }
    }
    return(OKAY);
}

/*-----------------------------------------------------------------------------
    Name        : CommandProcess
    Description : processes command messages sent to WindowProc
    Inputs      : Same as WindowProc without the message WM_COMMAND integer
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
/*void CommandProcess(HWND hWnd, WPARAM wParam, LPARAM lParam)*/
void CommandProcess(int code)
{
    SDL_Event e;

    switch (code)
    {
        case CID_ExitError:
            e.quit.type = SDL_QUIT;
            SDL_PushEvent(&e);

            break;
        case CID_ExitOK:
            e.quit.type = SDL_QUIT;
            SDL_PushEvent(&e);

            break;
    }
}

filehandle mainGetDevStatsHandle(char *filepath) {
    if (!fopen (filepath, "r")) {
        return 0;
    }
    return fileOpen(filepath, FF_IgnorePrepend | FF_TextMode | FF_IgnoreBIG);
}

/*-----------------------------------------------------------------------------
devstat init startup removed
----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------

    Name        : mainDevStatsShutdown
    Description : release memory used by the devstats table
    Inputs      :
    Outputs     :
    Return      :

fixme: This section was called devstats shutdown but seems to be more. Cleanup required...
----------------------------------------------------------------------------*/
void mainDevStatsShutdown(void)
{
    if (devTable != NULL)
    {
        free(devTable);
        devTable = NULL;
    }
}

void IntroDeactivateMe()
{
    sounddeactivate(TRUE);

#if 0	/* Bink, no? */
    if (!binkDonePlaying)
    {
        binkPause(TRUE);
        hwSetRes(0, 0, 0);
    }
#endif

    wasDemoPlaying = demDemoPlaying;                        //save demo playback state
    demDemoPlaying = FALSE;                                 //stop the demo playback for now

#if MAIN_MOUSE_FREE
    if (utySystemStarted)
    {
        utyClipMouse(FALSE);
    }
#endif
    utyTaskTimerClear();                                    //clear pending timer ticks
    keyClearAll();
    keyBufferClear();
    systemActive = FALSE;
}

void IntroActivateMe()
{
    sounddeactivate(FALSE);

    /*hwSetRes(-1, -1, -1);*/

    demDemoPlaying = wasDemoPlaying;                        //keep playing demo if it was playing when we minimized
#if MAIN_MOUSE_FREE
    if (utySystemStarted)
    {
        utyClipMouse(startupClipMouse);                     //optionally trap the mouse
    }
#endif
    systemActive = TRUE;

    utyForceTopmost(fullScreen);
    SDL_ShowCursor(SDL_DISABLE);
    //make sure that the mouse is rotating properly when we come back
    utyMouseButtonsClear();

    keyClearAll();
    keyBufferClear();
    // CAT (M2-R6 startup-keys): no caps-lock sync of the tactical overlay (the exe's activation in WindowProc 0x00561f10
    //      has none; HW1's mrTacticalOverlayState is gone, Toggle_Tactical_Display toggles it: KeyBindings.c)

#if 0	/* More bink... */
    if (!binkDonePlaying)
    {
        binkPause(FALSE);
    }
#endif
}

/*-----------------------------------------------------------------------------
    Name        : DeactivateMe
    Description : deactivates this application and suspends processing
    Inputs      : hWnd
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void DeactivateMe()
{
    sounddeactivate(TRUE);

    wasDemoPlaying = demDemoPlaying;                        //save demo playback state
    demDemoPlaying = FALSE;                                 //stop the demo playback for now

#if MAIN_MOUSE_FREE
    utyClipMouse(FALSE);
#endif
    utyTaskTimerClear();                                    //clear pending timer ticks
    if (multiPlayerGame || noPauseAltTab)
    {                                                       //only stop rendering if it's single player
        taskSavePauseStatus();
        taskPause(utyRenderTask);
        taskPause(regTaskHandle);
    }
    else
    {
        taskFreezeAll();                                    //stop all tasks
    }
    keyClearAll();
    keyBufferClear();
    systemActive = FALSE;

    // CAT: DeactivateMe 0x0055fb90 also minimises a full-screen window unless /noMinimize, deactivates the renderer
    //      (hwActivate / rGL) and pauses a playing Bink movie (binkPause): SDL and GoK's movie player (no pause)
    //      handle those (decision 28)
#if 0	/* Bink stuff... */
    if (!binkDonePlaying)
    {
        binkPause(TRUE);
    }
#endif
    /*
    if (utyTest2(SS2_ToggleKeys))
    {
        utyToggleKeyStatesRestore();
    }
    */
}

/*-----------------------------------------------------------------------------
    Name        : ActivateMe
    Description : activates this application and resumes processing
    Inputs      : hWnd
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ActivateMe()
{
    sounddeactivate(FALSE);

    demDemoPlaying = wasDemoPlaying;                        //keep playing demo if it was playing when we minimized

    feRenderEverything = TRUE;

#if MAIN_MOUSE_FREE
    if (utySystemStarted)
    {
        utyClipMouse(startupClipMouse);                     //optionally trap the mouse
    }
#endif
    if (utySystemStarted)
    {                                                       //if game has started
        taskResumeAll();                                    //resume all tasks
    }
    systemActive = TRUE;

    utyForceTopmost(fullScreen);
    SDL_ShowCursor(SDL_DISABLE);
    //make sure that the mouse is rotating proper when we come back
    utyMouseButtonsClear();

    keyClearAll();
    keyBufferClear();

    hrBackgroundReinit = TRUE;
    /*
    if (utyTest2(SS2_ToggleKeys))
    {
        utyToggleKeyStatesSave();
    }
    */
    // CAT (M2-R6 startup-keys): no caps-lock sync of the tactical overlay (the exe's activation in WindowProc 0x00561f10
    //      has none; HW1's mrTacticalOverlayState is gone, Toggle_Tactical_Display toggles it: KeyBindings.c)

#if 0	/* Bink stuff... */
    if (!binkDonePlaying)
    {
        binkPause(FALSE);
    }
#endif
}

/*-----------------------------------------------------------------------------
    Name        : mainFreeLibraries
    Description : Release references to possible GL libraries.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mainFreeLibraries(void)
{
}

/*-----------------------------------------------------------------------------
    Name        : mainRescaleMainWindow
    Description : rescale the main window (ghMainWindow) and call fn to
                  reinit game systems that require it
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mainRescaleMainWindow(void)
{
    /* Maybe? */
#if 0
    bool32 wasClipped;

    mainWindowTotalWidth  = MAIN_WindowWidth  + mainWidthAdd;
    mainWindowTotalHeight = MAIN_WindowHeight + mainHeightAdd;

    wasClipped = mouseClipped;
    utyClipMouse(FALSE);

    SetWindowPos(ghMainWindow, HWND_TOP,
                 0, 0,
                 mainWindowTotalWidth, mainWindowTotalHeight,
                 (showBorder) ? SWP_DRAWFRAME : 0);

    if (wasClipped)
    {
        utyClipMouse(TRUE);
    }
#endif

    (void)utyChangeResolution(MAIN_WindowWidth, MAIN_WindowHeight, MAIN_WindowDepth);
}

/*-----------------------------------------------------------------------------
    Name        : mainStartupGL
    Description : startup an OpenGL renderer
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 mainStartupGL(char* data)
{
    rndinitdata renderData;

    mainRescaleMainWindow();

    renderData.width = MAIN_WindowWidth;
    renderData.height = MAIN_WindowHeight;
#ifdef _WIN32
    renderData.hWnd = ghMainWindow;
#endif
    renderData.hWnd = 0;
    if (!rndSmallInit(&renderData, TRUE))
    {
        return FALSE;
    }

    utyForceTopmost(fullScreen);

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : mainMemAlloc
    Description : memory allocation wrapper (for rGL's benefit)
    Inputs      : len - amount of memory to allocate (bytes)
                  name - name of the memory handle
                  flags - flags for allocation, currently ignored
    Outputs     :
    Return      : pointer to new memory, or NULL if none available
----------------------------------------------------------------------------*/
void* mainMemAlloc(GLint len, char* name, GLuint flags)
{
    return memAllocAttempt(len, name, NonVolatile | ExtendedPool);
}

/*-----------------------------------------------------------------------------
    Name        : mainMemFree
    Description : memory free wrapper (for rGL's benefit)
    Inputs      : pointer - the memory pointer to free
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mainMemFree(void* pointer)
{
    memFree(pointer);
}

/*-----------------------------------------------------------------------------
    Name        : mainStartupParticularRGL
    Description : startup rGL as a renderer and activate a particular device
    Inputs      : device - rGL device name
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 mainStartupParticularRGL(char* device, char* data)
{
    rndinitdata renderData;

    mainRescaleMainWindow();

    renderData.width = MAIN_WindowWidth;
    renderData.height = MAIN_WindowHeight;
#ifdef _WIN32
    renderData.hWnd = ghMainWindow;
#endif
    renderData.hWnd = 0;
    if (!rndSmallInit(&renderData, FALSE))
    {
        return FALSE;
    }

    utyForceTopmost(fullScreen);

    return TRUE;
}

void mainDestroyWindow(void)
{
    mainActuallyQuit = FALSE;
    // HWI-FIX (M3-R2 instrumentation; GoK's fault, M3-R1's finding docs/audits/m3-r1-ui-scale.md): cataclysm.exe clears
    //      the flag and at once destroys the window (mainDestroyWindow 0x00560350: DAT_008b85b0 = 0; DestroyWindow), and
    //      the window's own WM_DESTROY consumes the clear (WindowProc 0x00561f10 case 2: the flag 0 -> set back to 1, no
    //      quit), so the user's next quit quits. SDL's window goes in rndClose (SDL_DestroyWindow), which posts no
    //      SDL_QUIT: the clear stayed, the next quit's SDL_QUIT was swallowed (HandleEvent) and the main loop ended without
    //      WindowsCleanup (rc 1, no shutdown). The destroyed window's WM_DESTROY, as the exe's window takes it:
    mainActuallyQuit = TRUE;                        // HWI-FIX

    mainWindowTotalWidth  = MAIN_WindowWidth  + mainWidthAdd;
    mainWindowTotalHeight = MAIN_WindowHeight + mainHeightAdd;

#ifdef _WIN32
    if (DebugWindow)
    {
        //dbwClose();
    }
#endif

//    mainUnregisterClass(ghInstance);
//    mainRegisterClass(ghInstance);

    utyForceTopmost(fullScreen);

#ifdef _WIN32
    if (DebugWindow)
    {
        /* If porting this back to Windows, you'll need to get a hold of
           values for these (hWnd is in the SDL_SysWMinfo structure, hInst
           would need to be acquired through Windows API calls). */
        //dbwStart((udword)ghInstance, (udword)ghMainWindow);
    }
#endif
}

/*-----------------------------------------------------------------------------
    Name        : mainShutdownGL
    Description : shutdown a GL renderer
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mainShutdownGL(void)
{
    rndClose();

    /*hwDeleteWindow();*/
    mainDestroyWindow();
}

/*-----------------------------------------------------------------------------
    Name        : mainShutdownRGL
    Description : shutdown rGL as a renderer
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mainShutdownRGL(void)
{
    rndClose();
    mainDestroyWindow();
}

/*-----------------------------------------------------------------------------
    Name        : mainResetRender
    Description : inform necessary modules that the renderer is switching
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mainResetRender(void)
{
    rndResetGLState();
    feRenderEverything = TRUE;
    frReset();
    ferReset();
    mouseReset();
    feReset();
    // CAT: mainResetRender 0x00560420 (also inlined in mainOpenRender 0x00560470): then the overlay icons' textures, the
    //      lens flares' and the fog layer's go (each loads again when it is next drawn) [M2-R6 startup-keys: the order
    //      and the two unloads; the icons were first here]
    cat_overlayIconsDiscardGL();
    cat_rndLensFlareTextureUnload();
    cat_rndFogLayerTextureUnload();
    if (!reinitInProgress)
    {
        trReload();
    }
}

/*-----------------------------------------------------------------------------
    Name        : mainCloseRender
    Description : free module textures, call reset render
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mainCloseRender(void)
{
    // CAT: the exe's copies (inlined in mainLoadGL 0x00561570, mainRestoreRender 0x00560ba0, WindowProc 0x00561f10)
    //      also free the glcompat textures (glcFreeTextures: not in the SDL build) and, after cpPreviewImageDelete,
    //      the plug texture (rndLoadShamelessPlug(FALSE) 0x00579fa0, below)
    partShutdown();
    ferReset();
    cpTexturesPurge();
    lmFreeTextures();
    cpPreviewImageDelete();
    rndLoadShamelessPlug(FALSE);    // CAT: the plug texture (mainShutdownRenderer 0x00561288, mainLoadGL 0x005615a8)
    btgCloseTextures();
    cmCloseTextures();
    rmGUIShutdown();
    mainResetRender();

    if (!reinitInProgress)
    {
        trSetAllPending(FALSE);
        trNoPalShutdown();
    }
}

/*-----------------------------------------------------------------------------
    Name        : mainOpenRender
    Description : call reset render
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mainOpenRender(void)
{
    trNoPalStartup();
    mainResetRender();
    rmGUIStartup();
    cmLoadTextures();
    btgLoadTextures();
    lmLoadTextures();
    frReloadGL();
}

sdword saveMAIN_WindowWidth;
sdword saveMAIN_WindowHeight;
sdword saveMAIN_WindowDepth;

/*-----------------------------------------------------------------------------
    Name        : mainSaveRender
    Description : save current renderer info in case a mode switch doesn't work
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mainSaveRender(void)
{
    // HWI-FEATURE(ui-scale): MAIN_Window* hold the logical size once the window exists; the mode to restore is the request
    uiScaleWindowRequestGet(&saveMAIN_WindowWidth, &saveMAIN_WindowHeight);    // HWI-FEATURE(ui-scale)
    saveMAIN_WindowDepth  = MAIN_WindowDepth;
}

void mainSetupSoftware(void)
{
    uiScaleWindowRequestGet(&mainWindowWidth, &mainWindowHeight);  // HWI-FEATURE(ui-scale): the window's request
    mainWindowDepth  = MAIN_WindowDepth;

    opDeviceIndex = -1;
}

/*-----------------------------------------------------------------------------
    Name        : mainRestoreSoftware
    Description : load 640x480@16, rGL+software rendering system
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mainRestoreSoftware(void)
{
    mainSetupSoftware();

    mainRescaleMainWindow();

    bMustFree = FALSE;
    if (!mainLoadParticularRGL("sw", ""))
    {
        SDL_Event e;
/*
        MessageBox(NULL,
                   "couldn't initialize default rendering system",
                   windowTitle, MB_APPLMODAL | MB_OK);
*/
        fprintf(stderr, "Fatal Error: Couldn't initialize default rendering system.\n");
        e.user.type = SDL_USEREVENT;
        e.user.code = CID_ExitError;
        SDL_PushEvent(&e);
    }
    bMustFree = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : mainRestoreRender
    Description : restore saved rendering system (mainSaveRender())
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mainRestoreRender(void)
{
    mainWindowWidth  = saveMAIN_WindowWidth;
    mainWindowHeight = saveMAIN_WindowHeight;
    uiScaleWindowRequest(saveMAIN_WindowWidth, saveMAIN_WindowHeight);   // HWI-FEATURE(ui-scale): MAIN_Window* = the request
    mainWindowDepth  = MAIN_WindowDepth  = saveMAIN_WindowDepth;

    mainRescaleMainWindow();

    bMustFree = FALSE;
    if (!mainLoadGL(NULL))
    {
        //couldn't restore, try basic software
        mainRestoreSoftware();
    }
    bMustFree = TRUE;

    feRenderEverything = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : mainShutdownRenderer
    Description : shutdown the existing renderer
    Inputs      :
    Outputs     :
    Return      : TRUE or FALSE
----------------------------------------------------------------------------*/
bool32 mainShutdownRenderer(void)
{
    dbgMessage("mainShutdownRenderer");

    mainCloseRender();      // CAT: mainShutdownRenderer 0x00561260 inlines it; the overlay icons, lens-flare and fog
                            //      textures go in its mainResetRender (M2-R6 startup-keys: the icons were first here)
    mainShutdownGL();
    mainFreeLibraries();

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : mainLoadGL
    Description : close existing renderer, startup a GL
    Inputs      : data - possible name of GL .DLL
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 mainLoadGL(char* data)
{
    dbgMessage("-- load OpenGL --");

    if (bMustFree)
    {
        mainCloseRender();
        mainShutdownGL();
        mainFreeLibraries();
    }

    if (!mainStartupGL(data))
    {
        return FALSE;
    }

    mainOpenRender();

    lodScaleFactor = 1.0f;
    alodStartup();

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : mainLoadParticularRGL
    Description : close existing render, startup rGL w/ specified device
    Inputs      : device - device name {sw, fx}
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 mainLoadParticularRGL(char* device, char* data)
{
    dbgMessagef("-- load rGL device %s --", device);

    if (bMustFree)
    {
        mainCloseRender();
        mainShutdownGL();
        mainFreeLibraries();
    }

    if (!mainStartupParticularRGL(device, data))
    {
        return FALSE;
    }

    mainOpenRender();

    lodScaleFactor = 1.0f;
    alodStartup();

    mainReinitRenderer = 2;

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : keyLanguageTranslate
    Description : This function translates the virtual key into the ASCII character.
    Inputs      : keysym
    Outputs     : converted character
    Return      : Uh-huh
----------------------------------------------------------------------------*/
udword keyLanguageTranslate(udword wParam)
{
    switch (strCurKeyboardLanguage)
    {
        case languageEnglish:
            return(wParam);
        break;
        case languageFrench:
            if (wParam < KEY_TOTAL_KEYS)
                return(keyFrenchToEnglish(wParam));
            else
                return(0);
        break;
        case languageGerman:
            if (wParam < KEY_TOTAL_KEYS)
                return(keyGermanToEnglish(wParam));
            else
                return(0);
        break;
        case languageSpanish:
            if (wParam < KEY_TOTAL_KEYS)
                return(keySpanishToEnglish(wParam));
            else
                return(0);
        break;
        case languageItalian:
            if (wParam < KEY_TOTAL_KEYS)
                return(keyItalianToEnglish(wParam));
            else
                return(0);
        break;
    }

    return wParam;
/*    char   buff[256], trankey[2];
    sdword i,ret;

    for (i=0;i<256;i++) buff[i] = 0;

    ret = ToAscii(wParam, 0, buff, (uword *)trankey, 0);

    if (ret==0)
    {
        return((udword)wParam);
    }
    else if (ret==1)
    {
        if ((trankey[0] >= 'a') && (trankey[0] <= 'z'))
        {
            trankey[0]-='a'-'A';
        }
        return ((udword)((ubyte)trankey[0]));
    }

    return (0);*/
}

/*-----------------------------------------------------------------------------
    Name        : HandleEvent
    Description : SDL event handling for the application
    Inputs      : pEvent - Pointer to the event to handle
----------------------------------------------------------------------------*/
void HandleEvent(SDL_Event const* pEvent) {
    extern bool32 utilPlayingIntro;

    /* Mouse button press times for double-click support. */
    static Uint32 mbDownTime[3] = { 0, 0, 0 };
    static Uint8 mbDouble[3] = { 0, 0, 0 };

    dbgAssertOrIgnore(pEvent);

#if MAIN_PRINT_MESSAGES
    dbgMessagef("Event type = 0x%hhx", pEvent->type);
#endif //MAIN_PRINT_MESSAGES

    switch (pEvent->type)
    {
        case SDL_APP_DIDENTERFOREGROUND:
            if (systemActive == FALSE)
            {                                               //we're being activated
                if (utilPlayingIntro)
                {
                    IntroActivateMe();
                }
                else
                {
                    ActivateMe();
                }
            } else return;
        case SDL_APP_WILLENTERBACKGROUND:
            if (systemActive == TRUE)
            {                                               //we're being deactivated
                if (utilPlayingIntro)
                {
                    IntroDeactivateMe();
                }
                else
                {
                    DeactivateMe();
                }
            } else return; // per documentation

        case SDL_KEYUP:                                     //keys up/down
            switch (pEvent->key.keysym.sym)
            {
                case SDLK_ESCAPE:
#if 0	/* Bink */
                    if (!binkDonePlaying)
                    {
                        binkStop();
                    }
                    else
#endif
                    {
                        keyPressUp(pEvent->key.keysym.scancode);
                    }
                    break;      // CAT: Escape ends there (WindowProc 0x00561f10; GoK fell through to F11's case).
                                //      The exe stops a playing Bink movie on Escape: GoK's movie player stays (28)
#if MAIN_MOUSE_FREE
                case SDLK_F11:                              //toggle clipped cursor
                    if (keyIsHit(CONTROLKEY))
                    {
                        utyClipMouse(mouseClipped ^ TRUE);
                    }
                    else
                    {
                        keyPressUp(pEvent->key.keysym.scancode);
                    }
                    break;
#endif
                case SDLK_F12:
                    if (keyIsHit(SHIFTKEY) && keyIsHit(CONTROLKEY))
                    {
                        mainCloseRender();
                        mainShutdownGL();
                        mainRestoreSoftware();
                        mainOpenRender();
                        lodScaleFactor = LOD_ScaleFactor;
                        alodStartup();
                    }
                    break;
                default:
                    keyPressUp(keyLanguageTranslate(pEvent->key.keysym.scancode));//keyPressUp(KeyMapFromWindows(wParam));
            }
            return;

        case SDL_KEYDOWN:
            if (pEvent->key.repeat != 0) //if it's a repeating key
            {
                keyRepeat(keyLanguageTranslate(pEvent->key.keysym.scancode));
            }
            else                         //else it's a freshly-pressed key
            {
                if ((pEvent->key.keysym.scancode == HWI_OVERLAY_KEY) ||                // HWI-FEATURE(instrumentation): the overlay's keys: Menu,
                    ((pEvent->key.keysym.scancode == HWI_OVERLAY_FKEY) && hwiKbOverlayFKeyFree()))   // HWI-FEATURE(instrumentation): and F1 while free (M3-R3)
                {                                                       // HWI-FEATURE(instrumentation)
                    hwiOverlayKey();                                    // HWI-FEATURE(instrumentation)
                }                                                       // HWI-FEATURE(instrumentation)
                keyPressDown(keyLanguageTranslate(pEvent->key.keysym.scancode));
            }
            // CAT: the exe's WM_KEYDOWN gives every key down (and repeat) to the developer console too, untranslated
            //      (WindowProc 0x00561f10 -> cat_consoleKeyPress 0x0045a700, which returns while it is closed); HWI:
            //      as the exe's virtual-key code (Key.c catKeyVkFromKey)
            cat_consoleKeyPress(catKeyVkFromKey(pEvent->key.keysym.scancode));
            return;

        case SDL_USEREVENT:
            CommandProcess(pEvent->user.code);
            return;

        case SDL_MOUSEBUTTONDOWN:
            if (!mouseDisabled)
            {
                Uint32 curr_time = SDL_GetTicks();

                switch (pEvent->button.button)
                {
                    case SDL_BUTTON_LEFT:
                        if (!mbDouble[0] && curr_time - mbDownTime[0] <= 500)
                        {
                            keyPressDown(LMOUSE_DOUBLE);
                            mbDouble[0] = 1;
                        }
                        else
                        {
                            mouseLClick();
                            keyPressDown(LMOUSE_BUTTON);
                            mbDouble[0] = 0;
                        }
                        mbDownTime[0] = curr_time;
                    break;

                    case SDL_BUTTON_MIDDLE:
                        if (!mbDouble[1] && curr_time - mbDownTime[1] <= 500)
                        {
                            keyPressDown(MMOUSE_DOUBLE);
                            mbDouble[1] = 1;
                        }
                        else
                        {
                            keyPressDown(MMOUSE_BUTTON);
                            mbDouble[1] = 0;
                        }
                        mbDownTime[1] = curr_time;
                    break;

                    case SDL_BUTTON_RIGHT:
                        if (!mbDouble[2] && curr_time - mbDownTime[2] <= 500)
                        {
                            keyPressDown(RMOUSE_DOUBLE);
                            mbDouble[2] = 1;
                        }
                        else
                        {
                            keyPressDown(RMOUSE_BUTTON);
                            mbDouble[2] = 0;
                        }
                        mbDownTime[2] = curr_time;
                    break;

               }
            }
            break;

        case SDL_MOUSEWHEEL:
            if (!mouseDisabled)
            {
                if(pEvent->wheel.y == 1)
                    keyPressDown(FLYWHEEL_UP);
                else if(pEvent->wheel.y == -1)
                    keyPressDown(FLYWHEEL_DOWN);
                break;
            }
 
        case SDL_MOUSEBUTTONUP:
            if (!mouseDisabled)
            {
                switch (pEvent->button.button)
                {
                    case SDL_BUTTON_LEFT:
                        keyPressUp(LMOUSE_BUTTON);
                        keyPressUp(LMOUSE_DOUBLE);
                    break;

                    case SDL_BUTTON_MIDDLE:
                        keyPressUp(MMOUSE_BUTTON);
                        keyPressUp(MMOUSE_DOUBLE);
                    break;

                    case SDL_BUTTON_RIGHT:
                        keyPressUp(RMOUSE_BUTTON);
                        keyPressUp(RMOUSE_DOUBLE);
                    break;
                }
            }
            break;
            
        // Currently written with use of SpaceNavigator in mind (since that's
        // what I've got). Needs to be rewritten to be a little more generic
        // using mappings of registered joystick axes to functionality.
        case SDL_JOYAXISMOTION:
            #define DEBUG_JOYSTICK_CAMERA            FALSE
            #define JOYSTICK_SENSITIVITY_THRESHOLD  (10 * SWORD_Max / 100)

            if (pEvent->jaxis.value > -JOYSTICK_SENSITIVITY_THRESHOLD
            &&  pEvent->jaxis.value <  JOYSTICK_SENSITIVITY_THRESHOLD)
            {
                break;
            }
            
            switch (pEvent->jaxis.axis) 
            {
                // zoom: +ve multiplier = zoom out
                
                case 1: // translation (+/-) forwards-backwards 
                    // camJoyZoom = -1 * pEvent->jaxis.value;
                    break;
                    
                case 2: // translation (+/-) up-down
                    camJoyZoom = -1 * pEvent->jaxis.value;
                    break;

                // declination: +ve multiplier = move toward north pole
                
                case 3: // pitch (+/-) back-forward  - declination
                    camJoyDeclination = +1 * pEvent->jaxis.value;
                    break;

                // right ascension: +ve multiplier = anti clockwise about z-axis

                case 0: // translation (+/-) left-right 
                    // camJoyRightAscension = -1 * pEvent->jaxis.value;
                    break;

                case 4: // roll  (+/-) left-right    - rotate
                    // camJoyRightAscension = -1 * pEvent->jaxis.value;
                    break;

                case 5: // yaw   (+/-) right-left    - rotate
                    camJoyRightAscension = -1 * pEvent->jaxis.value;
                    break;

                default: 
                    break; 
            }

#if DEBUG_JOYSTICK_CAMERA            
            dbgMessagef("joystick: dec(%6d) asc(%6d) zoom(%6d)", camJoyDeclination, camJoyRightAscension, camJoyZoom);
#endif
            break; 
    
    
        case SDL_WINDOWEVENT:                               // HWI-FEATURE(window-resize) (M3-R4 window-modes): a dragged frame
            if (pEvent->window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                extern SDL_Window *sdlwindow;
                if (sdlwindow != NULL && pEvent->window.windowID == SDL_GetWindowID(sdlwindow))
                {
                    uiScaleWindowResized(sdlwindow);
                }
            }
            break;

        case SDL_QUIT:
            if (mainActuallyQuit)
            {
                SDL_Event e;
                WindowsCleanup();
                e.quit.type = SDL_QUIT;
                SDL_PushEvent(&e);
            }
            else
            {
                mainActuallyQuit = TRUE;
            }
            return;
    }

} // WindowProc

/*-----------------------------------------------------------------------------
    Name        : InitWindow
    Description : Initialize main window...stuff...
    Inputs      :
    Outputs     :
    Return      : TRUE if successful, FALSE if not
----------------------------------------------------------------------------*/
static bool32 InitWindow ()
{
    unsigned int rinDevCRC;

    /*
     * create a window
     */

    if (mainAutoRenderer)
    {
        if (selectedRES)
        {
            MAIN_WindowWidth  = mainWindowWidth;
            MAIN_WindowHeight = mainWindowHeight;
            MAIN_WindowDepth  = mainWindowDepth;
        }
        else
        {
            mainWindowWidth  = MAIN_WindowWidth;
            mainWindowHeight = MAIN_WindowHeight;
            mainWindowDepth  = MAIN_WindowDepth;
        }
    }

    mainWindowTotalWidth  = MAIN_WindowWidth  + mainWidthAdd;
    mainWindowTotalHeight = MAIN_WindowHeight + mainHeightAdd;

    /* Window created in renderer initialization.. */

    rinEnumerateDevices();

	rinDevCRC = rinDeviceCRC();
    if (opDeviceCRC != rinDevCRC)
    {
        opDeviceIndex = -1;
        if (mainAutoRenderer)
        {
            opDeviceCRC = rinDeviceCRC();
            mainForceSoftware = TRUE;
        }
    }

    if (mainForceSoftware)
    {
        utyForceTopmost(fullScreen);

        mainRescaleMainWindow();

        opDeviceIndex = -1;
    }

    lodScaleFactor = 1.0f;

    return TRUE;
} /* doInit */

/*-----------------------------------------------------------------------------
    Name        : WindowsCleanup
    Description : Closes all windows, frees memory etc.
    Inputs      : void
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
void WindowsCleanup(void)
{
    utyGameSystemsShutdown();
    rinFreeDevices();
}

/*-----------------------------------------------------------------------------
    Name        : mainCleanupAfterVideo
    Description : possibly delete main DDraw window after playing an AVI if a renderer
                  is about to be initialized that wants to create its own
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mainCleanupAfterVideo(void)
{
    if (windowNeedsDeleting)
    {
        windowNeedsDeleting = FALSE;
    }
}

#ifdef __EMSCRIPTEN__
void main_loop() {
    int i;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        HandleEvent(&event);

        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                printf("Resolution change event: %dx%d!\n", event.window.data1, event.window.data2);
                if ((event.window.data1 > 320) && (event.window.data2 > 240))
                {
                    MAIN_WindowWidthActual = event.window.data1;
                    MAIN_WindowHeightActual = event.window.data2;

                    MAIN_WindowWidth = event.window.data1;
                    MAIN_WindowHeight = event.window.data2;

                    // try to find an integer down scaled resolution for UI rendering
                    for (i = 10; i > 0; i--)
                    {
                        // Since the browser window can be resized to any size, we need to allow
                        // for a little more 'fuzziness' in the resolution selection.
                        // Instead of requiring perfect pixel scaling, we allow for a 1 pixel difference.
                        if ((MAIN_WindowWidth % i <= 1) && (MAIN_WindowHeight % i <= 1))
                        {
                            if ((MAIN_WindowWidth / i >= 640) && (MAIN_WindowHeight / i >= 480))
                            {
                                MAIN_WindowWidth /= i;
                                MAIN_WindowHeight /= i;

                                // Calculate the actual render resolution. This might be off by a few pixels
                                MAIN_WindowWidthActual = MAIN_WindowWidth * i;
                                MAIN_WindowHeightActual = MAIN_WindowHeight * i;
                                break;
                            }
                        }
                    }

                    printf("UI resolution: %dx%d\n", MAIN_WindowWidth, MAIN_WindowHeight);
                    printf("Render resolution: %dx%d\n", MAIN_WindowWidthActual, MAIN_WindowHeightActual);

                    (void)utyChangeResolution(MAIN_WindowWidthActual, MAIN_WindowHeightActual, MAIN_WindowDepth);
                }
            }
        }

        if (event.type == SDL_QUIT) {
            SDL_Quit();
            EM_ASM(
                // Send back to root URL
                location.href = "/";
            );
        }
    }

    utyTasksDispatch(); // execute all tasks

    if (opTimerActive) {
        if (taskTimeElapsed > (opTimerStart + opTimerLength)) {
            opTimerExpired();
        }
    }
}

int main (int argc, char* argv[])
{
        // create persistent storage
        EM_ASM(
            // Make a directory other than '/'
            FS.mkdir('/home/web_user/.homeworld/');
            FS.mkdir('/home/web_user/.homeworld/SavedGames/');
            FS.mkdir('/home/web_user/.homeworldDownloadable/');
            FS.mkdir('/home/web_user/.homeworldDownloadable/SavedGames/');
            FS.mkdir('/assets/');

            // Then mount with IDBFS type
            FS.mount(IDBFS, {}, '/home/web_user/.homeworld/SavedGames/');
            FS.mount(IDBFS, {}, '/home/web_user/.homeworldDownloadable/SavedGames/');
            FS.mount(IDBFS, {}, '/assets/');

            // create symlinks to assets
            FS.symlink('/assets/Homeworld.big', 'Homeworld.big');
            FS.symlink('/assets/HW_Comp.vce', 'HW_Comp.vce');
            FS.symlink('/assets/HW_Music.wxd', 'HW_Music.wxd');

            // Then sync
            FS.syncfs(true, function (err) {
                if (err) console.log("FS.syncfs error " + err);

                // continue with main after filesystem is synced
                Module._main_postinit(0, 0);
            });

        );
}

int EMSCRIPTEN_KEEPALIVE main_postinit(int argc, char* argv[]) {
  // file system sync is now complete
  // continue on

#else
int main (int argc, char* argv[])
{
#endif
    static char* errorString = NULL;
#ifdef _WIN32
    static HANDLE hMapping;
#endif
    static bool32 preInit;

#ifdef _WIN32
    //check to see if a copy of the program already running and just exit if so.
    //This messy hack is required because hPrevInstance is not reliable under Win32.
    hMapping = CreateFileMapping( (HANDLE) 0xffffffff,
                    NULL,
                    PAGE_READONLY,
                    0,
                    32,
                    "LSMap" );

    if( hMapping )
    {
        if( GetLastError() == ERROR_ALREADY_EXISTS )
        {
            HWND hWndPrev;

            // Already an instance running. If it has a window yet, restore it.
            hWndPrev = FindWindow(windowTitle, NULL);
            if (hWndPrev)
            {
                OutputDebugString("\r\nProgram already running.\r\n");
                ShowWindow(hWndPrev, SW_RESTORE);
                BringWindowToTop(hWndPrev);
                UpdateWindow(hWndPrev);
            }
            return 0;
        }
    }
#endif  /* Support for other platforms? */

    // CAT (M2-R6 startup-keys): the start-up order is WinMain 0x005631f0's: the single-instance check (above, Win32
    //      only), the options file (Cataclysm.cfg), opKeyboardLoad, the command line, utyGameSystemsPreInit, the
    //      window, utyGameSystemsInit, the message loop dispatching tasks and the options timer. Not applicable to
    //      SDL: the "CloseCataclysm" window message, the registry copy of the command line (the CmdLine value under
    //      SOFTWARE/Sierra On-Line/Cataclysm), the OS tests (glCapNT / glCap95 / cat_glCapWin2K), the device enumeration and rGL /
    //      Glide selection (GoK's GL backend, decision 28), the loop's patch launch (CataclysmPatch.exe) and the
    //      error message box titled "Emergence" (stderr here)

    // HWI-FIX: a crash in a build without ASan (the release build) leaves a line on stderr (Debug.c dbgFatalSignalsInstall)
    dbgFatalSignalsInstall();

    // set audio resampling to highest quality
    SDL_SetHint(SDL_HINT_AUDIO_RESAMPLING_MODE, "3");

    //load in options from the options file
    utyOptionsFileRead();

    //copy keyboard redefinitions
    opKeyboardLoad();

    /*
    RegisterCommandLine(commandLine);       // make sure you call this before ProcessCommandLine because
                                            // ProcessCommandLine modifies commandLine
    */

    //process the command line, setting flags to be used later
    if (ProcessCommandLine(argc, argv) != OKAY)
    {
        return 0;
    }

    // CAT: 16 Hz is the default (Universe.c); GoK's 64 Hz universe only on request, never together with /vanilla
    if (gok64HzMode && !vanillaMode)
    {
        UNIVERSE_UPDATE_RATE_SHIFT = 2;
    }

    // set everything back to original behaviour
    if (vanillaMode)
    {
        UNIVERSE_UPDATE_RATE_SHIFT = 0;
    }

    if (selectedRES)
    {
        MAIN_WindowWidth  = mainWindowWidth;
        MAIN_WindowHeight = mainWindowHeight;
        MAIN_WindowDepth  = mainWindowDepth;
    }

    //initial game systems startup
    preInit = FALSE;
    if (errorString == NULL)
    {
        errorString = utyGameSystemsPreInit();
    }

    //startup the game window
    if (errorString == NULL)
    {
#ifdef _WIN32
        if (DebugWindow)
        {
            //dbwClose();
        }
#endif
        preInit = TRUE;
        if (!InitWindow())
        {
            errorString = ersWindowInit;
        }
#ifdef _WIN32
        if (DebugWindow)
        {
            /* If porting this back to Windows, you'll need to get a hold of
               values for these (hWnd is in the SDL_SysWMinfo structure, hInst
               would need to be acquired through Windows API calls). */
            //dbwStart((udword)ghInstance, (udword)ghMainWindow);
            utySet(SSA_DebugWindow);
        }
#endif
    }

    mainPlayAVIs = FALSE;
    if (errorString == NULL)
    {
#ifndef HW_GAME_DEMO
        if (enableAVI && fullScreen)
        {
            windowNeedsDeleting = TRUE;
            //set display mode
            /*
            if (!hwSetRes(640, 480, 32))
            {
                hwSetRes(640, 480, 16);
            }
            */
            utyForceTopmost(TRUE);
        }
        else
        {
            windowNeedsDeleting = FALSE;
            /*(void)hwCreateWindow((int)ghMainWindow, MAIN_WindowWidth, MAIN_WindowHeight, MAIN_WindowDepth);*/
            utyForceTopmost(fullScreen);
        }
#else
        windowNeedsDeleting = FALSE;
        /*(void)hwCreateWindow((int)ghMainWindow, MAIN_WindowWidth, MAIN_WindowHeight, MAIN_WindowDepth);*/
        utyForceTopmost(fullScreen);
#endif
        //startup game systems
        if (errorString == NULL)
        {
            errorString = utyGameSystemsInit();
        }
    }

    if (errorString == NULL) {
        preInit = FALSE;

#ifdef __EMSCRIPTEN__
        // 0 fps means to use requestAnimationFrame; non-0 means to use setTimeout.
        emscripten_set_main_loop(main_loop, 0, 1);
#else

        bool32 breakMainLoop = FALSE;

        while (TRUE) {
            // Give sound a break :)
            SDL_Delay(0);

            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                HandleEvent(&event);

                if (event.type == SDL_QUIT) {
                    breakMainLoop = TRUE;
                    break;
                }
            }
            if (breakMainLoop) break;

            uiScaleResizeFlush();                       // HWI-FEATURE(window-resize) (M3-R4 window-modes): the in-game relayout once no screen is open
            utyTasksDispatch(); // execute all tasks

            if (opTimerActive) {
                if (taskTimeElapsed > (opTimerStart + opTimerLength)) {
                    opTimerExpired();
                }
            }
        }
#endif
    } else { // some error on startup, either from preInit or Init()
        if (preInit) {
            (void)utyGameSystemsPreShutdown();
        }
        if (dsQuitChosen())                         // HWI-FEATURE(data-setup): the player quit the first-run step
        {
            return 0;
        }
        fprintf(stderr, "%s\n", errorString);
        return ERR_ErrorStart;
    }

    /* Stay clean and fresh... */
    if (SDL_WasInit(SDL_INIT_EVERYTHING))
    {
        SDL_Quit();
    }

    return 0;
}
