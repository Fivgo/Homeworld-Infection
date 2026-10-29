/*=============================================================================
    UTILITY.C: General utilities for Windows interface of Homeworld

    Created June 1997 by Luke Moloney
=============================================================================*/

#include "utility.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include "AIPlayer.h"
#include "AISkirmish.h"
#include "Animatic.h"
#include "AutoDownloadMap.h"
#include "AutoLOD.h"
#include "Battle.h"
#include "BigFile.h"
//#include "bink.h"
#include "Bounties.h"
#include "BTG.h"
#include "Captaincy.h"
#include "Clouds.h"
#include "ColPick.h"
#include "CommandNetwork.h"
#include "ConsMgr.h"
#include "Damage.h"
#include "Debug.h"
#include "Demo.h"
#include "ETG.h"
#include "FastMath.h"
#include "FEReg.h"
#include "File.h"
#include "font.h"
#include "FontReg.h"
#include "GameChat.h"
#include "GamePick.h"
#include "glinc.h"
#include "Globals.h"
#include "Gun.h"
#include "HorseRace.h"
#include "HS.h"
#include "InfoOverlay.h"
#include "Key.h"
#include "KeyBindings.h"
#include "Console.h"
#include "Clamp.h"
#include "LaunchMgr.h"
#include "LevelLoad.h"
#include "MissionSelfTest.h"   // HWI-TEST (M2-R8 kas-runtime-mission01): /catMissionTestTutorial
#include "Light.h"
#include "mainrgn.h"
#include "Memory.h"
#include "mouse.h"
#include "MultiplayerGame.h"
#include "MultiplayerLANGame.h"
#include "Nebulae.h"
#include "NetCheck.h"
#include "NIS.h"
#include "Options.h"
#include "OverlayIcons.h"
#include "ShipNames.h"
#include "ShipPrint.h"
#include "Particle.h"
#include "PiePlate.h"
#include "Ping.h"
#include "PlugScreen.h"
#include "prim3d.h"
#include "Randy.h"
#include "regkey.h"
#include "render.h"
#include "ResearchAPI.h"
#include "ResearchGUI.h"
#include "resource.h"
#include "SaveGame.h"
#include "ScenPick.h"
#include "Select.h"
#include "Sensors.h"
#include "Shader.h"
#include "ShipView.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "soundlow.h"                                      // HWI-FIX: soundstopthreads (quit)
#include "SpaceObj.h"
#include "Stats.h"
#include "StatScript.h"
#include "StringSupport.h"
#include "Subtitle.h"
#include "Tactical.h"
#include "Tactics.h"
#include "Task.h"
#include "TaskBar.h"
#include "Teams.h"
#include "texreg.h"
#include "TitanNet.h"
#include "TradeMgr.h"
#include "Trails.h"
#include "Tutor.h"
#include "Tweak.h"
#include "UIControls.h"
#include "Undo.h"
#include "Universe.h"
#include "Blast.h"
#include "UnivUpdate.h"
#include "Mesh.h"           // CAT: /catMeshLoadTest (port diagnostic)
#include "OracleDump.h"     // HWI-ORACLE: gameStart exit record, /oracleSelfTest

#include "main.h"
#include "avi.h"
#include "Mimic.h"
#include "PopupMenu.h"
#include "LOD.h"
#include "ShipSelect.h"
#include "MotionInterp.h"   // HWI-FEATURE(motion-interp): universeTaskHandle
#include "MotionTest.h"     // HWI-FEATURE(motion-interp): /catToggleTest
#include "uiscale.h"         // HWI-FEATURE(ui-scale): the uiScale row, the window rows
#include "UiScaleTest.h"     // HWI-TEST (M3-R1 ui-scale): /catUiScaleTest
#include "Instrument.h"      // HWI-FEATURE(instrumentation): the settings line's options-file rows

#ifdef _WIN32
    #include <windows.h>
    #include <winreg.h>
    #include <shellapi.h>
#elif defined(_MACOSX)
    #include <sys/types.h>
    #include <sys/mman.h>
    #include <unistd.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
    #include <X11/Xlib.h>
    #include <X11/keysym.h>
#endif

#ifdef _MSC_VER
	#define strcasecmp _stricmp
	#define stat _stat
	#define S_ISDIR(mode) ((mode) & _S_IFDIR)
	#define mkdir(p) _mkdir(p)
    
    #include <direct.h>  // for _mkdir
#else
    #include <strings.h>
#endif

#ifdef _WIN32
    #include "debugwnd.h"
#endif

extern bool32 debugScreenshots;

// #define REG_MAGIC_STR  "D657E436967616D4"   // used for CD-checking code

#define CD_VALIDATION_ENABLED  0            // toggle checking CD is in drive and anti-piracy checks

#define CAT_MUSIC_CREDITS    0x29                // CAT: Cataclysm's credits track (HW1 RollCredits 28; SpeechEvent.c's CAT_MUSIC_NIS_VOLUME)
#define CAT_MUSIC_TUTORIAL   0x12                // CAT: the tutorial's track (gameStart 0x005a9290)
#define UTY_CONFIG_FILENAME  "Cataclysm.cfg"      // CAT: the exe's options file (string 0x008b6f80, utyOptionsFileRead 0x005a83a0 / utyOptionsFileWrite 0x005a84f0; HW1 "Homeworld.cfg")

extern SDL_Window *sdlwindow;
extern int MAIN_WindowWidth, MAIN_WindowHeight, MAIN_WindowDepth;

extern udword aviPlayIntros;

/*-----------------------------------------------------------------------------
    Name        : utyBrowserExec
    Description : Spawns the default web browser with the specified URL
    Inputs      :
    Outputs     :
    Return      : TRUE if the browser was started OK, FALSE otherwise
----------------------------------------------------------------------------*/
bool32 utyBrowserExec(char *URL) { return FALSE; }

//
//  for bigfiles
//
extern bool32 CompareBigfiles;
extern bool32 IgnoreBigfiles;
extern bigTOC mainTOC;
extern bigTOC updateTOC;
extern FILE *updateFP;     // for NULL checking
extern unsigned char *mainNewerAvailable;
extern unsigned char *updateNewerAvailable;

/*=============================================================================
    Data:
=============================================================================*/

bool32 utilPlayingIntro = FALSE;

void *utyMemoryHeap;
#ifndef _WIN32
// HWI-FIX: munmap() needs the mapped length (length 0 is EINVAL on Linux), so every quit after a growth heap was created
//          ended with the utility.c "result != -1" assertion (GoK 3af31fb defect, docs/m2-r3-closeout-headers.md "quit").
//          The lengths of the main heap and of each growth heap are remembered here. Not a Cataclysm delta: the exe
//          frees its heaps with VirtualFree(heap, 0, MEM_RELEASE), which takes no length.
static size_t utyMemoryHeapLength = 0;
static struct { void *heap; size_t length; } utyGrowthHeaps[MGH_NumberGrowthHeaps];
#endif
char errorString[UTY_ErrorStringLength];

// used to skip the tutorial warning after you have played a non tutorial mission
bool32 needtutorial = TRUE;

//data for the timing of tasks
Uint32 utyTimerDivisor;
Uint32 utyTimerLast;

//flag stating system has started properly
sdword utySystemStarted = FALSE;

//bit-field flags stating which system has started properly
bool8 utyStartedBits[SSA_NumberBits];

udword utyNFrameTicks = 0;

//global handle of default font
fonthandle ghDefaultFont = 0;

// name of music data file
#ifdef HW_GAME_DEMO
char utyMusicFilename[] = "DL_Music.wxd";
#elif defined(HW_GAME_RAIDER_RETREAT)
char utyMusicFilename[] = "OEM_Music.wxd";
#else
// CAT: Cataclysm's music stream (utyGameSystemsPreInit 0x005ab810, .data 0x008c0f08 "Music4Cat.wxd")
char utyMusicFilename[] = "Music4Cat.wxd";
#endif

// name of voice file
#ifdef HW_GAME_DEMO
char utyVoiceFilename[] = "DL_Demo.vce";
#else
// CAT: one voice file per playable race: Somtaaw (Sect) in utyVoiceFilename, Beast in cat_utyBeastVoiceFilename
//      (utyGameSystemsPreInit 0x005ab810 checks both, cat_speechEventLoadRaceVoice 0x005217f0 opens one;
//      .data 0x008c0f18 "SectSpeech.vce", 0x008c0f28 "BeastSpeech.vce")
char utyVoiceFilename[] = "SectSpeech.vce";
char cat_utyBeastVoiceFilename[] = "BeastSpeech.vce";
#endif

// name of other files
char utyPadBigFilename[] = "Extra.big"; // name of pad file
char utyLockFilename[] = "AutoLock.txt"; // name of autorun lock file

filehandle utyLockFilehandle; // handle of autorun lock file

// global string for the player name
char utyName[MAX_PERSONAL_NAME_LEN]     = "";
char utyPassword[MAX_PERSONAL_NAME_LEN] = "";

char levelfile[80];
char dirfile[100];

//basic global front end data
char *utyFrontEndFiles[] =
{                                                           //basic front-end screens
    "FEMan\\Front_end.fib",
    "FEMan\\Sensors_manager.fib",
//    "FEMan\\Sensors_manager_SingleP.fib",
    "FEMan\\Research_manager.fib",
    "FEMan\\In_game_ESC_menu.fib",
    "FEMan\\CSM-All.fib",
    "FEMan\\TaskBar.fib",
    "FEMan\\single_player_objective.fib",
    //"FEMan\\Single_player_objective.fib",
    NULL
};

void scriptSetHomeworldCRC(char *directory,char *field,void *dataToFillIn);

bool32 onlygetfirstcrc = FALSE;

scriptEntry WonStuffSet[] =
{
    makeEntry(HomeworldCRC,scriptSetHomeworldCRC),
    
    END_SCRIPT_ENTRY
};

//render task
taskhandle utyRenderTask;

//TRUE if tutorial savegame check is not to be performed
static bool32 forceSP = FALSE;

//global flag for demo functionality
#if defined (HW_GAME_DEMO) || defined(HW_GAME_RAIDER_RETREAT)
bool32 utyPlugScreens = FALSE;
#endif
bool32 utyCreditsSequence = FALSE;

//front-end callback functions
void utySinglePlayerOptions(char *name, featom *atom);
void utySinglePlayerGame(char *name, featom *atom);
void utySelectSinglePlayerGame(char *name, featom *atom);
void utyLoadMultiSinglePlayerGame(char *name, featom *atom);
void utyLoadSinglePlayerGame(char *name, featom *atom);
void utySelectMultiPlayerGame(char *name, featom *atom);
void utyLoadMultiPlayerGame(char *name, featom *atom);
void utySinglePlayerGameStart(char *name, featom *atom);
void utyBackFromSinglePlayer(char *name, featom *atom);
void utyBackFromSinglePlayerOptions(char *name, featom *atom);
void utyGameQuit(char *name, featom *atom);
void utyMultiplayerGameStart(char *name, featom *atom);
void utyShowCredits(char *name, featom *atom);
void utyStartInternetGame(char *name, featom *atom);
void utyBackgroundToggle(char *name, featom *atom);
void utyBackgroundBitmapToggle(char *name, featom *atom);
void utyFilterToggle(char *name, featom *atom);
void utyMagFilterToggle(char *name, featom *atom);
void utyFilterBitmapToggle(char *name, featom *atom);
void utySmoothingToggle(char *name, featom *atom);
void utySmoothingBitmapToggle(char *name, featom *atom);
void utyStippleToggle(char* name, featom* atom);
void utyStippleBitmapToggle(char* name, featom* atom);
void utyTrailsToggle(char* name, featom* atom);
void utyTrailsBitmapToggle(char* name, featom* atom);
void utyLanguageToggle(char* name, featom* atom);
void utySensorsBlobsToggle(char* name, featom* atom);
void utyInstantTransitionToggle(char *name, featom *atom);
void utyInstantTransitionBitmapToggle(char* name, featom* atom);
void utySensorsBlobsBitmapToggle(char* name, featom* atom);
void cat_utyDynamicCPVToggle(char* name, featom* atom);                 // CAT: 0x005a8cb0
void cat_utyDynamicCPVBitmapToggle(char* name, featom* atom);           // CAT: 0x005a8d40
void cat_utyLensFlareToggle(char* name, featom* atom);                  // CAT: 0x005a8db0
void cat_utyLensFlareBitmapToggle(char* name, featom* atom);            // CAT: 0x005a8e10
void cat_utyRepairbotsToggle(char* name, featom* atom);                 // CAT: 0x005a8e60
void cat_utyRepairbotsBitmapToggle(char* name, featom* atom);           // CAT: 0x005a8ec0
void cat_utyGameQuitToMainOrStats(char *name, featom *atom);            // CAT: 0x005ab5b0
void utyBulletBitmapToggle(char* name, featom* atom);
void utyBulletToggle(char* name, featom* atom);
void utyMuzzleBitmapToggle(char* name, featom* atom);
void utyMuzzleToggle(char* name, featom* atom);
void utyDamageBitmapToggle(char* name, featom* atom);
void utyDamageToggle(char* name, featom* atom);
void utyHitBitmapToggle(char* name, featom* atom);
void utyHitToggle(char* name, featom* atom);

void utyPickColors(char *name, featom *atom);
//void utyScenarioPick(char *name, featom *atom);
void utyHideMenuOrGotoQuitGame(char *name, featom *atom);
//void utyNewGameRace(char *name, featom *atom);
//void utyTeaserEnd(void);

void utyInGameCancel(char *name, featom *atom);
void utyInGameLoad(char *name, featom *atom);
void utyInGameSave(char *name, featom *atom);

void utySaveRecordedGame(char *name, featom *atom);
//from options.c

void opKeyCancel(char* name, featom* atom);

void opDrawNumChannels(featom *atom, regionhandle region);

void opOptionsAccept(char* name, featom* atom);
void opOptionsInit(void);
void opOptionsStart(char* name, featom* atom);
void opOptionsCancel(char* name, featom* atom);

void opInGameOptionsStart(char* name, featom* atom);
void opInGameOptionsCancel(char* name, featom* atom);
void opInGameOptionsAccept(char* name, featom* atom);

void opEqualizerToAudioAccept(char* name, featom* atom);
void opEqualizerToAudioCancel(char* name, featom* atom);
void opEqualizerToInGameAudioAccept(char* name, featom* atom);
void opEqualizerToInGameAudioCancel(char* name, featom* atom);
void opAdvancedToAudioAccept(char* name, featom* atom);
void opAdvancedToAudioCancel(char* name, featom* atom);
void opAdvancedToInGameAudioCancel(char* name, featom* atom);
void opAdvancedToInGameAudioAccept(char* name, featom* atom);

void opEffectsToVideoAccept(char* name, featom* atom);
void opEffectsToVideoCancel(char* name, featom* atom);
void opEffectsToInGameVideoAccept(char* name, featom* atom);
void opEffectsToInGameVideoCancel(char* name, featom* atom);

void opMusicVolume(char* name, featom* atom);
void opSFXVolume(char* name, featom* atom);
void opSpeechVolume(char* name, featom* atom);
void opNumberChannels(char* name, featom* atom);
void opAutoChannels(char* name, featom* atom);
void opSpeaker(char* name, featom* atom);
void opSoundQualitySet(char* name, featom* atom);
void opAdvancedButton(char* name, featom* atom);
void opEqualizerButton(char* name, featom* atom);
void opEqualizer1(char* name, featom* atom);
void opEqualizer2(char* name, featom* atom);
void opEqualizer3(char* name, featom* atom);
void opEqualizer4(char* name, featom* atom);
void opEqualizer5(char* name, featom* atom);
void opEqualizer6(char* name, featom* atom);
void opEqualizer7(char* name, featom* atom);
void opEqualizer8(char* name, featom* atom);
//advanced:
void opVoiceCommands(char* name, featom* atom);
void opVoiceStatus(char* name, featom* atom);
void opVoiceChatter(char* name, featom* atom);
//game
void opMouseSensitivity(char* name, featom* atom);
void opBattleChatterCB(char* name, featom* atom);
void opNumberEffects(char* name, featom* atom);
void opCPUDifficulty(char* name, featom* atom);
void opCPUAttacks(char* name, featom* atom);
void opInfoOverlay(char* name, featom* atom);
void opTaskbarUp(char* name, featom* atom);
void opIncreaseChatting(char* name, featom* atom);
void opDecreaseChatting(char* name, featom* atom);
void opNumberChatlines(char* name, featom* atom);
void opAutodockHealth(char* name, featom* atom);
void opAutodockFuel(char* name, featom* atom);
void opAutodockHealthValue(char* name, featom* atom);
void opAutodockFuelValue(char* name, featom* atom);

//video
void opNoLOD(char* name, featom* atom);
void opDetailThreshold(char* name, featom* atom);
void opBrightness(char* name, featom* atom);
void opLighting(char* name, featom* atom);
void opEffects(char* name, featom* atom);
void opCustomEffects(char* name, featom* atom);
void opResolution(char* name, featom* atom);
void opRender(char* name, featom* atom);

void versionNumDraw(featom *atom, regionhandle region);

void utyVideoPlay(char* name, featom* atom);

void utyTutorialAlertCancel(char* name, featom* atom);

void tbSensorsBegin(char* name, featom* atom);

// CAT (M2-R6 frontend): an option row the exe registers with an empty function (nullsub), so its atom is found
static void utyNullCallback(char *name, featom *atom)
{
    (void)name; (void)atom;
}

void utyStartTutorial(char* name, featom* atom);
void utyStartSinglePlayer(char* name, featom* atom);
void utyStartSkirmish(char* name, featom* atom);

void opTroubleShoot(char* name, featom* atom);

void spRestartLevel(char *name, featom *atom);

// these callbacks are for the drop player dialog
void utyYesDropPlayer(char *name, featom *atom);
void utyDontDropPlayer(char *name, featom *atom);
void utyDrawDroppedPlayer(featom *atom, regionhandle region);

void opCountdownBoxDraw(featom* atom, regionhandle region);
void opCountdownYes(char* name, featom* atom);
void opCountdownNo(char* name, featom* atom);

sdword utyPlayerDroppedDisplay=-1;

// RACE_Sect or RACE_Beast, modified in utyNewGameRace callback (CAT: HW1 aliases R1 / R2 retired)
ShipRace whichRaceSelected = RACE_Sect;

fecallback utyCallbacks[] =
{                                                           //base front-end callbacks
    {opTroubleShoot,            "OP_TroubleShoot"},
    {utyStartTutorial,          "TA_StartTutorial"},
    {utyStartSinglePlayer,      "TA_StartSinglePlayer"},
    {utyStartSkirmish,          "TA_StartSkirmish"},
    {utyVideoPlay,              "VideoPlay"},
    {utyTutorialAlertCancel,    "TutorialAlertCancel"},
    {utyTutorialAlertCancel,    "TutorialSkirmishAlertCancel"},
    {spRestartLevel,            "SP_Restart_Level"},
    {utySinglePlayerOptions,    "SinglePlayerOptions"},     //load saved game or start new game
    {utySinglePlayerGame,       "SinglePlayerGame"},        //start new game
    {utySelectSinglePlayerGame, "SelectSinglePlayerGame"},  //select from saved games
    {utyLoadMultiSinglePlayerGame,   "LoadMultiSinglePlayerGameFromMain"},    //load a saved game & start
    {utySelectMultiPlayerGame,  "SelectMultiPlayerGame"},  //select from saved games
    {utySinglePlayerGameStart,  "StartSinglePlayerGame"},
    {utyBackFromSinglePlayer,   "BackFromSinglePlayer"},
    {utyBackFromSinglePlayerOptions, "BackFromSinglePlayerOptions"},
    {utyNewGameStart,           "StartNewGame"},
    {tutTutorial1,              "Tutorial1"},
    {utyMultiplayerGameStart,   "FE_Multiplayer_Game"},
    {utyShowCredits,            "FE_SHOWCREDITS"},
    {utyGameQuit,               "QuitGame"},
    {cat_utyGameQuitToMainOrStats, "QuitToMain"},           // CAT: 0x005ab5b0 (HW1 utyGameQuitToMain)
    {utyInGameCancel,           "IG_Cancel"},
//    {utyInGameLoad,             "IG_LoadGame"},
//    {utyInGameSave,             "IG_SaveGame"},
//    {utySaveRecordedGame,       "IG_SaveRecordedGame"},
    // CAT (M2-R6 startup-keys): no CSM_* / PLM_* rows (HW1's right-click menu; the exe's table 0x008c0f80 goes from
    //      IG_Cancel to VO_Filter: Cataclysm's right click is the popup menu, PopupMenu.c; "CSM_Hyperspace" is the
    //      task bar's own string, tbSetupHyperspace)
    {utyFilterToggle,           "VO_Filter"},               //options screen controls
    {utyMagFilterToggle,        "VO_MagFilter"},
    {utyFilterBitmapToggle,     "VO_BitmapFilter"},
    {utyBackgroundToggle,       "VO_Background"},
    {utyBackgroundBitmapToggle, "VO_BitmapBackground"},
    {utySmoothingToggle,        "VO_Smoothing"},
    {utySmoothingBitmapToggle,  "VO_BitmapSmoothing"},
    {utyStippleToggle,          "VO_Stipple"},
    {utyStippleBitmapToggle,    "VO_BitmapStipple"},
    {utyTrailsToggle,           "VO_Trails"},
    {utyTrailsBitmapToggle,     "VO_BitmapTrails"},

    {utyHitToggle,           "VO_Hit"},
    {utyHitBitmapToggle,     "VO_BitmapHitEffects"},

    {utyDamageToggle,           "VO_Damage"},
    {utyDamageBitmapToggle,     "VO_BitmapDamage"},

    {utyMuzzleToggle,           "VO_Muzzle"},
    {utyMuzzleBitmapToggle,     "VO_BitmapMuzzle"},

    {utyBulletToggle,           "VO_Bullet"},
    {utyBulletBitmapToggle,     "VO_BitmapBullet"},

    {utyLanguageToggle,         "VO_Language"},
    {utySensorsBlobsToggle,     "VO_SensorsBlobs"},
    {utySensorsBlobsBitmapToggle,"VO_BitmapSensorsBlobs"},
    // CAT (M2-R6 startup-keys): Cataclysm's video options (rows 0x008c10e8 .. 0x008c1110)
    {cat_utyDynamicCPVToggle,   "VO_DynamicCPV"},
    {cat_utyDynamicCPVBitmapToggle, "VO_BitmapDynamicCPV"},
    {cat_utyLensFlareToggle,    "VO_LensFlare"},
    {cat_utyLensFlareBitmapToggle, "VO_BitmapLensFlare"},
    {cat_utyRepairbotsToggle,   "VO_Repairbots"},
    {cat_utyRepairbotsBitmapToggle, "VO_BitmapRepairbots"},
    {smSensorsBegin,            "SensorsManager"},
    {tbSensorsBegin,            "TB_SensorsManager"},
    {utyPickColors,             "NewGameSelectColour"},
    {utyHideMenuOrGotoQuitGame, "FE_HideMenuOrGotoQuitGame"},
    {utyInstantTransitionToggle,"VO_InstantTransition"},
    {utyInstantTransitionBitmapToggle, "VO_BitmapInstantTransition"},
/*    {utyPickColors,             "NewMultiGameSelectColour"},
    {utyPickColors,             "JoinMultiGameSelectColour"},
    {utyScenarioPick,           "CS_ChooseScenario"},
    {spDonePicking,             "CS_Done"},
    {spBackPicking,             "CS_Back"},
    {spScroller,                "CS_Scroller"},             //scenario picker scrollbar
    {tpStartGame,               "T_StartGame"},
    {tpBackPicking,             "T_Back"},
    {tpScroller,                "T_Scroller"},              //titan scrollbar
    {tpCreateGame,              "T_CreateGame"},
    {tpJoinGame,                "T_JoinGame"},
    {tpLeaveGame,               "T_LeaveGame"},
    {tpRefreshGame,             "T_RefreshGame"},
    {tpTextEntry,               "T_TextEntry"},*/
    {opOptionsStart,            "OP_Options_Start"},
    {opOptionsCancel,           "OP_Options_Cancel"},
    {opOptionsAccept,           "OP_Options_Accept"},
    {opInGameOptionsStart,      "OP_InGameOptions_Start"},
    {opInGameOptionsCancel,     "OP_InGameOptions_Cancel"},
    {opInGameOptionsAccept,     "OP_InGameOptions_Accept"},
    {opEqualizerToAudioCancel,  "OP_Equalizer_To_Audio_Cancel"},
    {opEqualizerToAudioAccept,  "OP_Equalizer_To_Audio_Accept"},
    {opEqualizerToInGameAudioCancel, "OP_Equalizer_To_InGameAudio_Cancel"},
    {opEqualizerToInGameAudioAccept, "OP_Equalizer_To_InGameAudio_Accept"},
    {opAdvancedToAudioCancel,   "OP_Advanced_To_Audio_Cancel"},
    {opAdvancedToAudioAccept,   "OP_Advanced_To_Audio_Accept"},
    {opAdvancedToInGameAudioCancel, "OP_Advanced_To_InGameAudio_Cancel"},
    {opAdvancedToInGameAudioAccept, "OP_Advanced_To_InGameAudio_Accept"},
    {opEffectsToVideoCancel,    "OP_Effects_To_Video_Cancel"},
    {opEffectsToVideoAccept,    "OP_Effects_To_Video_Accept"},
    {opEffectsToInGameVideoCancel, "OP_Effects_To_InGameVideo_Cancel"},
    {opEffectsToInGameVideoAccept, "OP_Effects_To_InGameVideo_Accept"},
    {opKeyCancel,               "OP_Key_Cancel"},
    {kbKeyResetToDefault,       "OP_KeyResetToDefault"},
    {opMusicVolume,             "OP_Music_Volume"},
    {opSFXVolume,               "OP_SFX_Volume"},
    {opSpeechVolume,            "OP_Speech_Volume"},
    {opNumberChannels,          "OP_Num_Channels"},
    {opAutoChannels,            "OP_AutoChannels"},
    {opSpeaker,                 "OP_Speaker"},
    {opSoundQualitySet,         "OP_DCTQuality"},
    {opAdvancedButton,          "OP_Advanced_Button"},
    {opEqualizerButton,         "OP_Equalizer_Button"},
    {opDetailThreshold,         "OP_Detail_Threshold"},
    {opBrightness,              "OP_Brightness"},
    {opLighting,                "OP_Lighting"},
    {opEffects,                 "OP_Effects"},
    {opCustomEffects,           "OP_Custom_Effects"},
    {opResolution,              "OP_Resolution"},
    {opRender,                  "OP_Render"},
    {opEqualizer1,              "OP_Equalizer1"},
    {opEqualizer2,              "OP_Equalizer2"},
    {opEqualizer3,              "OP_Equalizer3"},
    {opEqualizer4,              "OP_Equalizer4"},
    {opEqualizer5,              "OP_Equalizer5"},
    {opEqualizer6,              "OP_Equalizer6"},
    {opEqualizer7,              "OP_Equalizer7"},
    {opEqualizer8,              "OP_Equalizer8"},
    // CAT: no voice actors and no voice preview in Cataclysm's options (the exe's table 0x008c1148 .. 0x008c1438 has
    //      no OP_Voice* / OP_Hear_Voice_* rows: soundEventHearActor's callers are gone)
    {opVoiceCommands,           "OP_Voice_Commands"},
    {opVoiceStatus,             "OP_Voice_Status"},
    {opVoiceChatter,            "OP_Voice_Chatter"},
    {opMouseSensitivity,        "OP_Mouse_Sensitivity"},
    {opBattleChatterCB,         "OP_BattleChatter"},
    {opNumberEffects,           "OP_NumEffects"},
    {opCPUDifficulty,           "OP_CPU_Difficulty"},
    {opCPUAttacks,              "OP_CPU_Attacks"},
    {opInfoOverlay,             "OP_Info_Overlay"},
    {opTaskbarUp,               "OP_Taskbar_Up"},
    {opIncreaseChatting,        "OP_Increase_Chatting"},
    {opDecreaseChatting,        "OP_Dencrease_Chatting"},
    {opNumberChatlines,         "OP_Number_Chatlines"},
    {opAutodockHealth,          "OP_Autodock_Health"},
    {opAutodockFuel,            "OP_Autodock_Fuel"},
    {opAutodockHealthValue,     "OP_Autodock_Health_Value"},
    {opAutodockFuelValue,       "OP_Autodock_Fuel_Value"},
    {opNoLOD,                   "OP_NoLOD"},
    {opSmoothMotion,            "OP_SmoothMotion"},     // HWI-FEATURE(motion-interp): the Smooth motion checkbox (Options.c)
    {opSharpTextures,           "OP_SharpTextures"},    // HWI-FEATURE(sharp-textures): the SHARP TEXTURES checkbox (Options.c)
    {opUiScale,                 "OP_UiScale"},          // HWI-FEATURE(ui-scale): the UI SCALE radio buttons (Options.c)
    {kbListWindowCB,            "OP_KeyBindingsList"},
    // CAT: no OP_KeyBindingsPoolList row (the exe has none: HW1's key-pool list window, kbPoolListWindowCB)
    {utyYesDropPlayer,          "UTY_DropPlayer"},
    {utyDontDropPlayer,         "UTY_DontDropPlayer"},
    {opCountdownYes,            "OP_Countdown_Yes"},
    {opCountdownNo,             "OP_Countdown_No"},
    // CAT (M2-R6 frontend): Cataclysm's options (the exe's rows 0x008c1360 .. 0x008c1438, Options.c)
    {cat_opSmallFonts,          "OP_SmallFonts"},
    {cat_opHomeworldClassic,    "OP_HomeworldClassic"},
    {cat_opCataclysmDefault,    "OP_CataclysmDefault"},
    {cat_opHWTaskbar,           "OP_HWTaskbar"},
    {cat_opTaskbarButtons,      "OP_TaskbarButtons"},
    {cat_opTaskbarBackdrop,     "OP_TaskbarBackdrop"},
    {cat_opHomeworldCSA,        "OP_HomeworldCSA"},
    {cat_opRightClickCSA,       "OP_RightClickCSA"},
    {utyNullCallback,           "OP_SpecialActionBeforeAttack"},
    {cat_opAutoFocusOnWaypoints,"OP_AutoFocusOnWaypoints"},
    {cat_opExperienceMessages,  "OP_ExperienceMessages"},
    {cat_opAreaExplored,        "OP_AreaExplored"},
    {cat_opSensorsSphere,       "OP_SensorsSphere"},
    {cat_opSensorsWireframe,    "OP_SensorsWireframe"},
    {cat_opResources,           "OP_Resources"},
    {cat_opDust,                "OP_Dust"},
    {cat_opBackground,          "OP_Background"},
    {cat_opCompass,             "OP_Compass"},
    {cat_opObjectivePointers,   "OP_ObjectivePointers"},
    {cat_opSlipgatePaths,       "OP_SlipgatePaths"},
    {cat_opCapitalShipOverlay,  "OP_CapitalShipOverlay"},
    {utyNullCallback,           "OP_HardwareTransformLighting"},
    {cat_opTextureDetail,       "OP_Texture_Detail"},
    {cat_opSpeechTest,          "OP_Speech_Test"},
    {cat_opSoundFXTest,         "OP_SoundFX_Test"},
    {cat_spGameDifficulty,      "SP_GameDifficulty"},      // CAT: 0x0051c140 (SinglePlayer.c)
    {cat_opCataclysmFramerate,  "OP_CataclysmFramerate"},
    {cat_opLODDetail,           "OP_LODDetail"},

    {NULL, NULL}
};

fedrawcallback utyDrawCallbacks[] =
{                                                           //base front-end callbacks
    {opDrawNumChannels,         "OP_NumChannels"},
    {versionNumDraw,            "FE_DrawVersionNum"},
    // CAT: no NP_Decrease / NP_Increase / NP_Draw (the exe's draw table 0x008c1448 has none: the texture detail
    //      replaces HW1's no-palette pool display)
    {utyDrawDroppedPlayer,      "UTY_PlayerDropped"},
    {opCountdownBoxDraw,        "OP_DrawCountdownBox"},
    {NULL, NULL}
};


extern sdword opEqualizerSettings[];
extern unsigned long firewallButton;

color versionColor = colWhite;

// 1) if you change any of the scriptSet*CB function references here
//    then check that utyOptionsFileWrite() supports the type!
// 2) remember to remove the setting of the default value, by removing
//    the equivalent entry from Tweaks[] (defined in Tweak.c)

// CAT: the exe keeps two multiplayer words of tpGameCreated in the file as dwords: "MPGameFlags" is the dword at
//      0x008860ea (CaptainGameInfo's pad2 word and its flag word above it) and "MultiPlayerGameSetting" the dword at
//      0x008860d4 (numComputers, startingFleet, bountySize, startingResources). HWI: this tree's LP64 CaptainGameInfo
//      has other offsets, so the two words are staged here and copied in after a read / out before a write.
static udword catUtyMPGameFlags = 0;                // 0x008860ea
static udword catUtyMPGameSetting = 0;              // 0x008860d4
// CAT: the difficulty word of the options table's last row (0x008941a0, 2 in .data); written by the single-player
//      difficulty screen (cat_spGameDifficulty 0x0051c140, which sets the modifiers through cat_spGetDifficultyModifiers);
//      it is also the KAS skill level (kasfMissionSkillSet / Get 0x00493b80 / 0x00493b90) and kasSave's difficulty
udword catSpDifficultySetting = 2;                  // 0x008941a0

scriptEntry utyOptionsList[] =
{
    // CAT: the exe's options table utyOptionsList 0x008c1478 .. 0x008c18e0, row for row in its order and with its
    //      names (Cataclysm.cfg has no sections: utyOptionsFileWrite 0x005a84f0 writes every row as "%s    %d" or
    //      "%s    %s"). The HW1 / GoK rows the exe does not have follow the exe's rows.
    {"effectsLevel",            scriptSetUdwordCB, &opEffectsVal},
    {"speakerSetting",          scriptSetUdwordCB, &opSpeakerSetting},
    {"brightnessVal",           scriptSetUdwordCB, &opBrightnessVal},
    {"detailThreshold",         scriptSetUdwordCB, &opDetailThresholdVal},
    {"noLOD",                   scriptSetUdwordCB, &opNoLODVal},     // FULL DETAIL; HWI-FEATURE(full-detail): 1 by default, every object drawn at LOD 0
    {"enableSmoothing",         scriptSetUdwordCB, &enableSmoothing},
    {"enableCPV",               scriptSetUdwordCB, &enableCPV},        // 0x008c14c0
    {"enableLensFlare",         scriptSetUdwordCB, &enableLensFlare},  // 0x008c14cc (M2-R5 render)
    {"enableRepairbots",        scriptSetUdwordCB, &enableRepairbots}, // 0x008c14d8 (M2-R5 render)
    {"showBackgrounds",         scriptSetUdwordCB, &showBackgrounds},
    {"texLinearFiltering",      scriptSetUdwordCB, &texLinearFiltering},
    {"enableStipple",           scriptSetUdwordCB, &enableStipple},
    {"enableTrails",            scriptSetUdwordCB, &enableTrails},
    {"language",                scriptSetUdwordCB, &strCurLanguage},
    {"fuzzyBlobs",              scriptSetUdwordCB, &smFuzzyBlobs},
    {"PlayerBaseColor",         scriptSetUdwordCB, &utyBaseColor},
    {"PlayerStripeColor",       scriptSetUdwordCB, &utyStripeColor},
    {"ColorsPicked",            scriptSetUdwordCB, &cpColorsPicked},
    {"PlayerRace",              scriptSetUdwordCB, &whichRaceSelected},
    {"InstantTransition",       scriptSetUdwordCB, &smInstantTransition},
    {"PrevColor0.base",         scriptSetUdwordCB, &colPreviousColors[0].base},
    {"PrevColor0.detail",       scriptSetUdwordCB, &colPreviousColors[0].detail},  // CAT: HW1 "PrevColor0.stripe"
    {"PrevColor1.base",         scriptSetUdwordCB, &colPreviousColors[1].base},
    {"PrevColor1.detail",       scriptSetUdwordCB, &colPreviousColors[1].detail},
    {"PrevColor2.base",         scriptSetUdwordCB, &colPreviousColors[2].base},
    {"PrevColor2.detail",       scriptSetUdwordCB, &colPreviousColors[2].detail},
    {"PrevColor3.base",         scriptSetUdwordCB, &colPreviousColors[3].base},
    {"PrevColor3.detail",       scriptSetUdwordCB, &colPreviousColors[3].detail},
    {"PrevColor4.base",         scriptSetUdwordCB, &colPreviousColors[4].base},
    {"PrevColor4.detail",       scriptSetUdwordCB, &colPreviousColors[4].detail},
    {"PlayerName",              scriptSetStringCB, &utyName},
    {"PlayerPwd",               scriptSetStringCB, &utyPassword},      // CAT: HW1 "PlayerPassword"
    {"Equalizer0",              scriptSetUdwordCB, &opEqualizerSettings[0]},
    {"Equalizer1",              scriptSetUdwordCB, &opEqualizerSettings[1]},
    {"Equalizer2",              scriptSetUdwordCB, &opEqualizerSettings[2]},
    {"Equalizer3",              scriptSetUdwordCB, &opEqualizerSettings[3]},
    {"Equalizer4",              scriptSetUdwordCB, &opEqualizerSettings[4]},
    {"Equalizer5",              scriptSetUdwordCB, &opEqualizerSettings[5]},
    {"Equalizer6",              scriptSetUdwordCB, &opEqualizerSettings[6]},
    {"Equalizer7",              scriptSetUdwordCB, &opEqualizerSettings[7]},
    {"MusicVolume",             scriptSetUdwordCB, &opMusicVol},
    {"SFXVolume",               scriptSetUdwordCB, &opSFXVol},
    {"SpeechVolume",            scriptSetUdwordCB, &opSpeechVol},
    {"VoiceCommands",           scriptSetUdwordCB, &opVoiceComm},
    {"VoiceStatus",             scriptSetUdwordCB, &opVoiceStat},
    {"VoiceChatter",            scriptSetUdwordCB, &opVoiceChat},
    {"opMouseSens",             scriptSetUdwordCB, &opMouseSens},
    {"InfoOverlay",             scriptSetUdwordCB, &opInfoOverlayVar},
    {"opBattleChatter",         scriptSetUdwordCB, &opBattleChatter},
    {"spCurrentSelected",       scriptSetUdwordCB, &spCurrentSelected},  // CAT: HW1 "MultiPlayerLastMapID"
    {"etgDamageEffectsEnabled", scriptSetUdwordCB, &etgDamageEffectsEnabled},
    {"etgHitEffectsEnabled",    scriptSetUdwordCB, &etgHitEffectsEnabled},
    {"etgFireEffectsEnabled",   scriptSetUdwordCB, &etgFireEffectsEnabled},
    {"etgBulletEffectsEnabled", scriptSetUdwordCB, &etgBulletEffectsEnabled},
    {"etgHistoryScalar",        scriptSetUdwordCB, &etgHistoryScalar},
    {"NumChannels",             scriptSetUdwordCB, &opNumChannels},
    {"AutoChannel",             scriptSetUdwordCB, &opAutoChannel},
    {"SoundQuality",            scriptSetUdwordCB, &opSoundQuality},
    {"FirewallDetect",          scriptSetUdwordCB, &firewallButton},
    {"TutorialNeeded",          scriptSetUdwordCB, &needtutorial},
    {"MPGameFlags",             scriptSetUdwordCB, &catUtyMPGameFlags},             // 0x008860ea (staged)
    {"MultiPlayerGameSetting",  scriptSetUdwordCB, &catUtyMPGameSetting},           // 0x008860d4 (staged)
    {"ResInjInterval",          scriptSetUdwordCB, &tpGameCreated.resourceInjectionInterval},
    {"ResInjAmmount",           scriptSetUdwordCB, &tpGameCreated.resourceInjectionsAmount},
    {"ResLumpSumTime",          scriptSetUdwordCB, &tpGameCreated.resourceLumpSumTime},
    {"ResLumpSumAmmount",       scriptSetUdwordCB, &tpGameCreated.resourceLumpSumAmount},
    // CAT (M2-R6 frontend): the exe's rows of Cataclysm's options (0x008c1790 .. 0x008c18c8; Options.c and the
    //      words' owners)
    {"LargePopupMenuFonts",            scriptSetUdwordCB, &catPmLargeFont},
    {"Homeworld_CSA",                  scriptSetUdwordCB, &catMrHomeworldCSA},
    {"RightClick_CSA",                 scriptSetUdwordCB, &catMrRightClickCSA},
    {"AutoFocusOnWaypoints",           scriptSetUdwordCB, &catMrWaypointCentre},
    {"ShowTaskbarBackdrop",            scriptSetUdwordCB, &catMoTaskbarBackdrop},
    {"ShowTaskbarButtons",             scriptSetUdwordCB, &catMoTaskbarButtons},
    {"ExperienceMessages",             scriptSetUdwordCB, &catOpExperienceMessages},
    {"HomeworldTaskbar",               scriptSetUdwordCB, &catMoHWTaskbar},
    {"ShowAsteriod0",                  scriptSetUdwordCB, &catSmViewDecorativeAsteroids},
    {"ShowResources",                  scriptSetUdwordCB, &catSmViewResources},
    {"ShowFOWBlobs",                   scriptSetUdwordCB, &catSmViewFOWRanges},
    {"ShowBackground",                 scriptSetUdwordCB, &catSmViewBackground},
    {"ShowAreaVisited",                scriptSetUdwordCB, &catSmViewAreasVisited},
    {"ShowAreaOccupied",               scriptSetUdwordCB, &catSmViewAreasOccupied},
    {"ShowIcons",                      scriptSetUdwordCB, &catSmViewIcons},
    {"ShowEnemyShips",                 scriptSetUdwordCB, &catSmViewShipMeshes},
    {"ShowFriendlyShips",              scriptSetUdwordCB, &catSmViewPieplateLines},
    {"ShowUniverseExtents",            scriptSetUdwordCB, &catSmViewUniverseExtents},
    {"ShowMeteors",                    scriptSetUdwordCB, &catSmViewMeteors},
    {"ShowCompass",                    scriptSetUdwordCB, &catSmViewCompass},
    {"ShowObjectivesPointer",          scriptSetUdwordCB, &catSmViewObjectivePointers},
    {"ShowSlipgatePaths",              scriptSetUdwordCB, &catSmViewSlipgatePaths},
    {"HardwareTransformLighting",      scriptSetUdwordCB, &catOpHardwareTL},
    {"TextureDetail",                  scriptSetUdwordCB, &catOpTextureDetail},
    {"GameFramerateOption",            scriptSetUdwordCB, &catOpGameFramerate},
    {"DetailQuality",                  scriptSetUdwordCB, &catLodDetail},
    {"SetTextureRequirements",         scriptSetUdwordCB, &catOpTextureRequirementsSet},
    {"DifficultySetting",              scriptSetUdwordCB, &catSpDifficultySetting},     // 0x008c18d4 -> 0x008941a0

    // HWI: the rows the exe keeps in the registry instead (its regOptionsList 0x008c0e58: deviceCRC, deviceIndex,
    //      screenWidth / Height / Depth; SDL has no registry, so GoK keeps them in the options file), then GoK's own
    //      rows (its data path, the window mode and its "New HWSDL Options": PlayIntros is the switch the harness's
    //      options file sets to 0). No exe counterpart, so they follow the exe's rows. HW1's "noPalMB", "Voice0..2" and
    //      key rows (kbKeySavedKeys) are gone: the exe has no such rows (Cataclysm's key bindings live in keys.cfg).
    {"deviceCRC",                      scriptSetUdwordCB, &opDeviceCRC},
    {"deviceIndex",                    scriptSetUdwordCB, &opDeviceIndex},
    {"screenWidth",                    scriptSetUdwordCB, &MAIN_WindowWidth},
    {"screenHeight",                   scriptSetUdwordCB, &MAIN_WindowHeight},
    {"screenDepth",                    scriptSetUdwordCB, &MAIN_WindowDepth},
    {"HomeworldDataPath",              scriptSetStringCB, &fileHomeworldDataPath},
    {"fullScreen",                     scriptSetUdwordCB, &fullScreen},
    {"CollectResourcesAtEndOfMission", scriptSetBool,     &spCollectResourcesAtEndOfMission},
    {"PauseOrders",                    scriptSetUdwordCB, &opPauseOrders},
    {"PlayIntros",                     scriptSetUdwordCB, &aviPlayIntros},
    {"ShipRecoil",                     scriptSetUdwordCB, &opShipRecoil},
    {"ShipsAlwaysUseOwnerColors",      scriptSetBool,     &utyShipsAlwaysUseOwnerColors},
    // HWI-FEATURE(motion-interp): render-time motion interpolation (MotionInterp.c), 0 | 1, both 1 by default; the
    //      Smooth motion checkbox writes both, a hand edit can set them one by one
    {"objInterpolate",                 scriptSetUdwordCB, &objInterpolate},     // HWI-FEATURE(motion-interp)
    {"nisInterpolate",                 scriptSetUdwordCB, &nisInterpolate},     // HWI-FEATURE(motion-interp)
    // HWI-FEATURE(letterbox-setting): 0 (default) the exe's letterbox bars in front of the scene, 1 the old look
    {"letterboxBehind",                scriptSetUdwordCB, &letterboxBehind},    // HWI-FEATURE(letterbox-setting)
    // HWI-FEATURE(ui-scale): the scale of everything drawn in 2D, percent: 0 (default) = by the window height (200 at
    //      2160 rows or more, 150 at 1440 or more, 100 below), else 100 .. 400 (the Video screens offer 100 / 150 / 200)
    {"uiScale",                        scriptSetUdwordCB, &uiScale},            // HWI-FEATURE(ui-scale)
    {"TimeCompressionFactor",          scriptSetUbyteCB,  &turboTimeCompressionFactor},

    END_SCRIPT_ENTRY
};
/*=============================================================================
    Functions:
=============================================================================*/

void utyVideoPlay(char* name, featom* atom)
{
    // CAT: animBinkPlay 0x00421860 (0, 1) at 0x005a81ba (utyVideoPlay 0x005a81a0); the tree's port of 0x00421860 is
    //      animAviPlay (M2-R10 render-animatics-sp; GoK's animBinkPlay is a stub). The exe's cat_glcActive /
    //      glcActivate around it are the rGL backend's (decision 28)
    animAviPlay(0,1);

#ifdef DEBUG_STOMP
    regVerify((regionhandle)&regRootRegion);
#endif
    bitSet(regRootRegion.status, RSF_DrawThisFrame);
    regDirtyEverythingUpwards(atom->region);
    feRenderEverything = TRUE;
}

void utyTutorialAlertCancel(char* name, featom* atom)
{
    while (feStackIndex)
    {
        feScreenDisappear(NULL, NULL);
    }
    feScreenStart(ghMainRegion, "Main_game_screen");
#ifdef DEBUG_STOMP
    regVerify((regionhandle)&regRootRegion);
#endif
    bitSet(regRootRegion.status, RSF_DrawThisFrame);
    regDirtyEverythingUpwards(atom->region);
    feRenderEverything = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : versionNumDraw
    Description : draws version number of Homeworld
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void versionNumDraw(featom *atom, regionhandle region)
{
    rectangle pos=region->rect;
    sdword fontheight;
    fonthandle oldfont;
    char versionstr[70];

    primRectSolid2(&region->rect, colBlack);

    strcpy(versionstr, strGetString(strVersion));
    strcat(versionstr, " ");
    strcat(versionstr, networkVersion);
    strcat(versionstr, ".");
    strcat(versionstr, minorBuildVersion);                  // CAT: "Emergence 1" "." "01" (versionNumDraw 0x005a8250)

    dbgAssertOrIgnore(strlen(versionstr) < 70);

    oldfont = fontMakeCurrent(selGroupFont2);
    fontheight = fontHeight(" ");

    pos.y0 += fontheight;

    fontPrint(pos.x0, pos.y0, versionColor, versionstr);

#if 0
    pos.y0 += fontheight;
    fontPrint(pos.x0, pos.y0, versionColor, (char*)GLC_VENDOR);
    pos.y0 += fontheight;
    fontPrint(pos.x0, pos.y0, versionColor, (char*)GLC_RENDERER);
#endif

    fontMakeCurrent(oldfont);
}

/*-----------------------------------------------------------------------------
    Name        : catUtyOptionsStagedWordsSet / catUtyOptionsStagedWordsGet
    Description : HWI: copy the two staged multiplayer dwords of the options file into tpGameCreated after a read
                    and out of it before a write (see catUtyMPGameFlags: the exe reads and writes them in place)
----------------------------------------------------------------------------*/
static void catUtyOptionsStagedWordsSet(void)
{
    tpGameCreated.pad2 = (unsigned short)(catUtyMPGameFlags & 0xffff);
    tpGameCreated.flag = (unsigned short)(catUtyMPGameFlags >> 16);
    tpGameCreated.numComputers = (unsigned char)(catUtyMPGameSetting & 0xff);
    tpGameCreated.startingFleet = (unsigned char)((catUtyMPGameSetting >> 8) & 0xff);
    tpGameCreated.bountySize = (unsigned char)((catUtyMPGameSetting >> 16) & 0xff);
    tpGameCreated.startingResources = (unsigned char)(catUtyMPGameSetting >> 24);
}

static void catUtyOptionsStagedWordsGet(void)
{
    catUtyMPGameFlags = (udword)tpGameCreated.pad2 | ((udword)tpGameCreated.flag << 16);
    catUtyMPGameSetting = (udword)tpGameCreated.numComputers | ((udword)tpGameCreated.startingFleet << 8) |
                          ((udword)tpGameCreated.bountySize << 16) | ((udword)tpGameCreated.startingResources << 24);
}

/*-----------------------------------------------------------------------------
    Name        : utyOptionsFileRead
    Description : Read a set of options from the options file (CAT: Cataclysm.cfg, 0x005a83a0). HWI: GoK reads it
                    from $HOME/CONFIGDIR (the exe: the current directory, the install)
    Inputs      : void
    Outputs     : reads in the switches listed in utyOptionsList array
    Return      :
----------------------------------------------------------------------------*/
void utyOptionsFileRead(void)
{
#ifdef _WIN32
    // HWI-FIX(win64): the options file lives in %APPDATA%\Homeworld-Infection (GoK's dead _WINDOWS branch read
    //      %APPDATA%/.homeworld; meson defines _WIN32, not _WINDOWS), where utyGameSystemsPreInit puts the user
    //      settings path the file is written to
    char *home_dir = getenv("APPDATA");
#else
    char *home_dir = getenv("HOME");
#endif
    char ch_buf[PATH_MAX];

    if (home_dir)
    {
        strcpy(ch_buf, home_dir);
#ifdef _WIN32
        strcat(ch_buf, "/" HWI_WIN_CONFIGDIR "/");
#else
        strcat(ch_buf, "/" CONFIGDIR "/");
#endif
    }
    else
    {
        ch_buf[0] = '\0';
    }
    scriptSetFileSystem(ch_buf, UTY_CONFIG_FILENAME, utyOptionsList);
    hwiSettingsRowsRead(ch_buf);                    // HWI-FEATURE(instrumentation): which HWI rows the file has (the settings line's sources)
    catUtyOptionsStagedWordsSet();

    //call any functions that need to acknowledge a change due to loading
    cameraSensitivitySet(opMouseSens);
    battleChatterFrequencySet(opBattleChatter);
    // CAT: the exe then reads its registry rows (regOptionsList 0x008c0e58 under SOFTWARE\Sierra On-Line\Cataclysm);
    //      SDL has no registry: GoK keeps those rows in this file (utyOptionsList's HWI rows)
}

/*-----------------------------------------------------------------------------
    Name        : utyOptionsFileWrite
    Description : Write a set of options to the options file (CAT: Cataclysm.cfg, 0x005a84f0). HWI: GoK writes it
                    to the user settings path (the exe: the current directory, the install)
    Inputs      : void
    Outputs     : writes out the switches listed in utyOptionsList array
    Return      :
----------------------------------------------------------------------------*/
void utyOptionsFileWrite(void)
{
    filehandle cfgFile = fileOpen(UTY_CONFIG_FILENAME, FF_WriteMode|FF_IgnoreBIG|FF_UserSettingsPath);
    FILE *f = fileStream(cfgFile);

    sdword index;
    catUtyOptionsStagedWordsGet();
    miOptionsFileValues(TRUE);                      // HWI-FEATURE(motion-interp): a switch's run-only flags stay out of the file
    rndLetterboxFileValues(TRUE);                   // HWI-FEATURE(letterbox-setting): /letterboxBehind stays out of the file
    catLodFileValues(TRUE);                         // HWI-FEATURE(full-detail): /lodBands stays out of the file
    uiScaleFileValues(TRUE);                        // HWI-FEATURE(ui-scale): /uiScale stays out; screenWidth / Height = the request
    trFilterFileValues(TRUE);                       // HWI-FEATURE(sharp-textures): /noFilter, /sharpTextures stay out of the file
    for (index = 0; utyOptionsList[index].name != NULL; index++)
    {
        if (utyOptionsList[index].setVarCB == scriptSetUbyteCB) {
            fprintf(f, "%s    %u\n", utyOptionsList[index].name,
                *((ubyte *)utyOptionsList[index].dataPtr));
        }
        else if (utyOptionsList[index].setVarCB == scriptSetUwordCB) {
            fprintf(f, "%s    %u\n", utyOptionsList[index].name,
                *((uword *)utyOptionsList[index].dataPtr));
        }
        else if (utyOptionsList[index].setVarCB == scriptSetUdwordCB) {
            fprintf(f, "%s    %d\n", utyOptionsList[index].name,         // CAT: the exe's "%s    %d" (0x008c2e7c)
                *((sdword *)utyOptionsList[index].dataPtr));
        }
        else if (utyOptionsList[index].setVarCB == scriptSetStringCB) {
            fprintf(f, "%s    %s\n", utyOptionsList[index].name,
                (char*)utyOptionsList[index].dataPtr);
        }
        else if (utyOptionsList[index].setVarCB == scriptSetBool) {
            fprintf(f, "%s    %s\n", utyOptionsList[index].name,
                    *((bool32 *)utyOptionsList[index].dataPtr) ? "TRUE" : "FALSE");
        }
    }
    miOptionsFileValues(FALSE);                     // HWI-FEATURE(motion-interp): the run's flags back
    rndLetterboxFileValues(FALSE);                  // HWI-FEATURE(letterbox-setting): the run's value back
    catLodFileValues(FALSE);                        // HWI-FEATURE(full-detail): the run's value back
    uiScaleFileValues(FALSE);                       // HWI-FEATURE(ui-scale): the run's value and the logical size back
    trFilterFileValues(FALSE);                      // HWI-FEATURE(sharp-textures): the run's value back
    
    fileClose(cfgFile);
}

/*-----------------------------------------------------------------------------
    Name        : catUtyOptionsFileReadBack
    Description : HWI: port diagnostic, not in the exe (the options round trip of /catFeScreensTest, Options.c
                    catOptionsRunSelfTest): read the options file utyOptionsFileWrite wrote (the user settings path)
                    through utyOptionsList, as utyOptionsFileRead parses it (which reads $HOME/CONFIGDIR instead:
                    GoK ignores /settingspath for reading, docs/m2-r1-data-deltas.md 4)
    Inputs      :
    Outputs     : the options of the file
    Return      : the file's path
----------------------------------------------------------------------------*/
char *catUtyOptionsFileReadBack(void)
{
    static char path[PATH_MAX];
    char directory[PATH_MAX], *slash;

    strncpy(path, filePathPrepend(UTY_CONFIG_FILENAME, FF_UserSettingsPath), PATH_MAX - 1);
    path[PATH_MAX - 1] = '\0';
    strcpy(directory, path);
    slash = strrchr(directory, '/');
    if (slash != NULL)
    {
        slash[1] = '\0';                                   //the directory and the bare name, as utyOptionsFileRead
    }                                                       //passes them (scriptSetFileSystem keeps the name)
    else
    {
        directory[0] = '\0';
    }
    uiScaleWindowRows(TRUE);                        // HWI-FEATURE(ui-scale): the file's screenWidth / Height are a request
    scriptSetFileSystem(directory, UTY_CONFIG_FILENAME, utyOptionsList);
    uiScaleWindowRows(FALSE);                       // HWI-FEATURE(ui-scale): the window's logical size back
    catUtyOptionsStagedWordsSet();
    return path;
}

/*-----------------------------------------------------------------------------
    Name        : utyPickColors
    Description : Pick some colors (just testing for now)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
color utyBaseColor;
color utyStripeColor;
bool32 utyShipsAlwaysUseOwnerColors = FALSE;
// bool32 utyColorsPicked = FALSE;

void utyPickColors(char *name, featom *atom)
{
    cpColorsPick(&utyBaseColor, &utyStripeColor, &whichRaceSelected);
    //utyColorsPicked = TRUE;
//    cpColorsPick(&teColorSchemes[0].textureColor.base,
                 //&teColorSchemes[0].textureColor.detail);
}

/*-----------------------------------------------------------------------------
    Control handlers for the options screen:
-----------------------------------------------------------------------------*/
void utySetCustomEffects(void)
{
    opCustomEffectsToggled = TRUE;
}
void utyUnsetCustomEffects(void)
{
    opOptionsSaveCustomEffectsSettings();
    opCustomEffectsToggled = FALSE;
}

void utyMagFilterToggle(char* name, featom* atom)
{
    extern sdword texLinearMag;

    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, texLinearMag);
        utyUnsetCustomEffects();
    }
    else
    {
        if (FECHECKED(atom))
        {
            texLinearMag = TRUE;
        }
        else
        {
            texLinearMag = FALSE;
        }
    }

    trFilterEnable(texLinearFiltering);
}

void utyFilterToggle(char *name, featom *atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, texLinearFiltering);
        utyUnsetCustomEffects();
    }
    else
    {
        trFilterSwitchEnd();                        // HWI-FEATURE(sharp-textures): a click is the user's choice, saved as it is
        if (FECHECKED(atom))
            texLinearFiltering = TRUE;
        else
            texLinearFiltering = FALSE;
        feToggleButtonSet("VO_BitmapFilter",texLinearFiltering);
    }

    trFilterEnable(texLinearFiltering);

    utySetCustomEffects();
}
void utyFilterBitmapToggle(char *name, featom *atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, texLinearFiltering);
    }
    else
    {
        trFilterSwitchEnd();                        // HWI-FEATURE(sharp-textures): a click is the user's choice, saved as it is
        if (FECHECKED(atom))
            texLinearFiltering = TRUE;
        else
            texLinearFiltering = FALSE;
        feToggleButtonSet("VO_Filter",texLinearFiltering);
    }

    trFilterEnable(texLinearFiltering);
}
void utyBackgroundToggle(char *name, featom *atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, showBackgrounds);
    }
    else
    {
        if (FECHECKED(atom))
            showBackgrounds = TRUE;
        else
            showBackgrounds = FALSE;
        feToggleButtonSet("VO_BitmapBackground",showBackgrounds);
    }

    utySetCustomEffects();
}
void utyBackgroundBitmapToggle(char *name, featom *atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, showBackgrounds);
    }
    else
    {
        if (FECHECKED(atom))
            showBackgrounds = TRUE;
        else
            showBackgrounds = FALSE;
        feToggleButtonSet("VO_Background",showBackgrounds);
    }
}
void utyInstantTransitionToggle(char *name, featom *atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, smInstantTransition);
    }
    else
    {
        if (FECHECKED(atom))
            smInstantTransition = TRUE;
        else
            smInstantTransition = FALSE;
        feToggleButtonSet("VO_BitmapInstantTransition",smInstantTransition);
    }

    utySetCustomEffects();
}
void utyInstantTransitionBitmapToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, smInstantTransition);
    }
    else
    {
        if (FECHECKED(atom))
            smInstantTransition = TRUE;
        else
            smInstantTransition = FALSE;
        feToggleButtonSet("VO_InstantTransition", smInstantTransition);
    }
}
void utySmoothingToggle(char *name, featom *atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, enableSmoothing);
    }
    else
    {
        if (FECHECKED(atom))
            enableSmoothing = TRUE;
        else
            enableSmoothing = FALSE;
        feToggleButtonSet("VO_BitmapSmoothing",enableSmoothing);
    }

    utySetCustomEffects();
}
void utySmoothingBitmapToggle(char *name, featom *atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, enableSmoothing);
    }
    else
    {
        if (FECHECKED(atom))
            enableSmoothing = TRUE;
        else
            enableSmoothing = FALSE;
        feToggleButtonSet("VO_Smoothing",enableSmoothing);
    }
}
void utyStippleToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, enableStipple);
    }
    else
    {
        if (FECHECKED(atom))
            enableStipple = TRUE;
        else
            enableStipple = FALSE;
        feToggleButtonSet("VO_BitmapStipple",enableStipple);
    }

    utySetCustomEffects();
}
void utyStippleBitmapToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, enableStipple);
    }
    else
    {
        if (FECHECKED(atom))
            enableStipple = TRUE;
        else
            enableStipple = FALSE;
        feToggleButtonSet("VO_Stipple",enableStipple);
    }
}
void utyTrailsToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, enableTrails);
    }
    else
    {
        if (FECHECKED(atom))
            enableTrails = TRUE;
        else
            enableTrails = FALSE;
        feToggleButtonSet("VO_BitmapTrails", enableTrails);
    }

    utySetCustomEffects();
}
void utyTrailsBitmapToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, enableTrails);
    }
    else
    {
        if (FECHECKED(atom))
            enableTrails = TRUE;
        else
            enableTrails = FALSE;
        feToggleButtonSet("VO_Trails", enableTrails);
    }
}
void utyHitToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, etgHitEffectsEnabled);
    }
    else
    {
        if (FECHECKED(atom))
            etgHitEffectsEnabled = TRUE;
        else
            etgHitEffectsEnabled = FALSE;
        feToggleButtonSet("VO_BitmapHitEffects", etgHitEffectsEnabled);
    }

    utySetCustomEffects();
}
void utyHitBitmapToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, etgHitEffectsEnabled);
    }
    else
    {
        if (FECHECKED(atom))
            etgHitEffectsEnabled = TRUE;
        else
            etgHitEffectsEnabled = FALSE;
        feToggleButtonSet("VO_Hit", etgHitEffectsEnabled);
    }
}
void utyDamageToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, etgDamageEffectsEnabled);
    }
    else
    {
        if (FECHECKED(atom))
            etgDamageEffectsEnabled = TRUE;
        else
            etgDamageEffectsEnabled = FALSE;
        feToggleButtonSet("VO_BitmapDamage", etgDamageEffectsEnabled);
    }

    utySetCustomEffects();
}
void utyDamageBitmapToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, etgDamageEffectsEnabled);
    }
    else
    {
        if (FECHECKED(atom))
            etgDamageEffectsEnabled = TRUE;
        else
            etgDamageEffectsEnabled = FALSE;
        feToggleButtonSet("VO_Damage", etgDamageEffectsEnabled);
    }
}
void utyMuzzleToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, etgFireEffectsEnabled);
    }
    else
    {
        if (FECHECKED(atom))
            etgFireEffectsEnabled = TRUE;
        else
            etgFireEffectsEnabled = FALSE;
        feToggleButtonSet("VO_BitmapMuzzle", etgFireEffectsEnabled);
    }

    utySetCustomEffects();
}
void utyMuzzleBitmapToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, etgFireEffectsEnabled);
    }
    else
    {
        if (FECHECKED(atom))
            etgFireEffectsEnabled = TRUE;
        else
            etgFireEffectsEnabled = FALSE;
        feToggleButtonSet("VO_Muzzle", etgFireEffectsEnabled);
    }
}
void utyBulletToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, etgBulletEffectsEnabled);
    }
    else
    {
        if (FECHECKED(atom))
            etgBulletEffectsEnabled = TRUE;
        else
            etgBulletEffectsEnabled = FALSE;
        feToggleButtonSet("VO_BitmapBullet", etgBulletEffectsEnabled);
    }

    utySetCustomEffects();
}
void utyBulletBitmapToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, etgBulletEffectsEnabled);
    }
    else
    {
        if (FECHECKED(atom))
            etgBulletEffectsEnabled = TRUE;
        else
            etgBulletEffectsEnabled = FALSE;
        feToggleButtonSet("VO_Bullet", etgBulletEffectsEnabled);
    }
}


void utySensorsBlobsToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, smFuzzyBlobs);
    }
    else
    {
        if (FECHECKED(atom))
            smFuzzyBlobs = TRUE;
        else
            smFuzzyBlobs = FALSE;
        feToggleButtonSet("VO_BitmapSensorsBlobs",smFuzzyBlobs);
    }

    utySetCustomEffects();
}
void utySensorsBlobsBitmapToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, smFuzzyBlobs);
    }
    else
    {
        if (FECHECKED(atom))
            smFuzzyBlobs = TRUE;
        else
            smFuzzyBlobs = FALSE;
        feToggleButtonSet("VO_SensorsBlobs",smFuzzyBlobs);
    }
}

/*-----------------------------------------------------------------------------
    CAT (M2-R6 startup-keys): Cataclysm's three video-option toggles and their bitmap twins (0x005a8cb0 .. 0x005a8ec0),
    as the HW1 pairs: the dynamic CPV (enableCPV 0x00c52378; with the low-memory switch 0x00c5236c set it stays off and
    its button is disabled), the lens flares (enableLensFlare 0x00c52380), the repair droids (enableRepairbots
    0x00c52374); the text toggles mark the custom effects (0x00c30a60, opCustomEffectsToggled).
----------------------------------------------------------------------------*/
void cat_utyDynamicCPVToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        if (catMeshStateDisabled)
        {
            enableCPV = FALSE;
            bitSet(((regionhandle)atom->region)->status, RSF_RegionDisabled);
        }
        feToggleButtonSet(name, enableCPV);
    }
    else if (!catMeshStateDisabled)
    {
        enableCPV = FECHECKED(atom) ? TRUE : FALSE;
        feToggleButtonSet("VO_BitmapDynamicCPV", enableCPV);
    }
    utySetCustomEffects();
}
void cat_utyDynamicCPVBitmapToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        if (catMeshStateDisabled)
        {
            enableCPV = FALSE;
            bitSet(((regionhandle)atom->region)->status, RSF_RegionDisabled);
        }
        feToggleButtonSet(name, enableCPV);
    }
    else if (!catMeshStateDisabled)
    {
        enableCPV = FECHECKED(atom) ? TRUE : FALSE;
        feToggleButtonSet("VO_DynamicCPV", enableCPV);
    }
}
void cat_utyLensFlareToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, enableLensFlare);
    }
    else
    {
        enableLensFlare = FECHECKED(atom) ? TRUE : FALSE;
        feToggleButtonSet("VO_BitmapLensFlare", enableLensFlare);
    }
    utySetCustomEffects();
}
void cat_utyLensFlareBitmapToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, enableLensFlare);
    }
    else
    {
        enableLensFlare = FECHECKED(atom) ? TRUE : FALSE;
        feToggleButtonSet("VO_LensFlare", enableLensFlare);
    }
}
void cat_utyRepairbotsToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, enableRepairbots);
    }
    else
    {
        enableRepairbots = FECHECKED(atom) ? TRUE : FALSE;
        feToggleButtonSet("VO_BitmapRepairbots", enableRepairbots);
    }
    utySetCustomEffects();
}
void cat_utyRepairbotsBitmapToggle(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, enableRepairbots);
    }
    else
    {
        enableRepairbots = FECHECKED(atom) ? TRUE : FALSE;
        feToggleButtonSet("VO_Repairbots", enableRepairbots);
    }
}

void utyLanguageToggle(char* name, featom* atom)
{
    sdword index;

    if (FEFIRSTCALL(atom))
    {
        feRadioButtonSet(name, strCurLanguage);
    }
    else
    {
        strLoadLanguage((strLanguageType)atom->pData);
        frReloadFonts();

#ifdef DEBUG_STOMP
        regVerify((regionhandle)&regRootRegion);
#endif

        bitSet(regRootRegion.status, RSF_DrawThisFrame);           //flag everything to be redrawn
        regDirtyEverythingUpwards(atom->region);
        for (index = 0; index <= feStackIndex; index++)
        {
            feScreenAllHotKeysUpdate(feStack[feStackIndex].screen);
        }
    }
}

//void utyScenarioPick(char *name, featom *atom)
//{
//    spScenarioPick(NULL);//!!! make some real pointer
//}

extern bool32 nisScreenStarted;
void utyHideMenuOrGotoQuitGame(char *name, featom *atom)
{
/*
    if (nisEnabled && utyTeaserPlaying != NULL && atom->hotKey[strCurLanguage] == ESCKEY)
    {
        keyClearAll();
        feScreenDisappear(NULL, NULL);
        mouseCursorHide();
        nisScreenStarted = FALSE;
    }
    else
*/
    {
        feScreenStart(feStack[feStackIndex].baseRegion, "Quit_game");
    }
}

/*-----------------------------------------------------------------------------
    Name        : utyCloseOK
    Description : generates an exit command message
    Inputs      : generic region callback
    Outputs     : ..
    Return      : generic region callback
----------------------------------------------------------------------------*/
udword utyCloseOK(regionhandle region, sdword ID, udword event, udword data)
{
    SDL_Event e;
    e.user.type = SDL_USEREVENT;                            //exit the game
    e.user.code = CID_ExitOK;
    e.user.data1 = 0;
    e.user.data2 = 0;
    SDL_PushEvent(&e);

    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : utyFatalErrorWaitLoop
    Description : Handles messages in case of fatal errors.  Also sends message
                    to create a message box.
    Inputs      : exitCode - code passed to exit()
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
void utyFatalErrorWaitLoop(int exitCode)
{
    SDL_Event e;

    while (SDL_PollEvent(&e)) { }                           //remove all messages from Queue

    e.user.type = SDL_USEREVENT;                            //tell app to exit
    e.user.code = CID_ExitError;
    e.user.data1 = 0;
    e.user.data2 = 0;
    SDL_PushEvent(&e);

    //e.quit.type = SDL_QUIT;
    //SDL_PushMessage(&e);                                    //???

    exit(exitCode);
}

/*-----------------------------------------------------------------------------
    Name        : utyNonFatalErrorWaitLoop
    Description : Handles messages in case of non-fatal errors.  Also sends message
                    to create a message box.
    Outputs     : ..
    Return      : TRUE if the CANCEL button (debug) was selected
----------------------------------------------------------------------------*/
sdword utyNonFatalErrorWaitLoop(void)
{
    //int returnValue;

#ifdef HW_BUILD_FOR_DEBUGGING
    return TRUE;
#else
    return FALSE;
#endif
}

/*-----------------------------------------------------------------------------
    Name        : gameStart
    Description : starts the game
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
//needed to be made global
bool8 etgHasBeenStarted;
void FancyFightPreLoad(void);

// CAT: the start-position table 0x00ab77e0 (.bss; name chosen): gameStart fills it in every non-tutorial new game, the
//      MissionSphere readers map a sphere's player number through it outside a network game (LevelLoad.c
//      catMissionSphereStartPositions)
sdword catStartPositions[MAX_MULTIPLAYER_PLAYERS];

void gameStart(char *loadfilename)
{
    char *useScenarioFile = 0;
    Player *player = NULL;
    uword i;
    //udword numCompPlayers = 0;

#if MAIN_CDCheck
    if (mainCDCheckEnabled)
    {
        if (!fileExists(utyMusicFilename, FF_CDROM))
        {
            dbgFatalf(DBG_Loc, "Homeworld CD not detected.");
        }
    }
#endif
    // CAT (M2-R6 startup-keys): gameStart 0x005a9290 first clears every player's support-unit budget (Player +0xC /
    //      +0x10), and a multiplayer game loads the /LoadMPGame file when the command line named one (0x00b3a020,
    //      cat_mainLoadMPGameSet, main.c)
    for (i = 0; i < MAX_MULTIPLAYER_PLAYERS; i++)
    {
        universe.players[i].supportUnitsMax = 0;
        universe.players[i].supportUnitsUsed = 0;
    }
    if (multiPlayerGame && (catMainLoadMPGamePath[0] != '\0'))
    {
        loadfilename = catMainLoadMPGamePath;
    }
    soundEventStopTrack(SOUND_FRONTEND_TRACK, 1.0f);                          //stop it

    if (loadfilename != NULL)
    {
        PreLoadGame(loadfilename);                          // sets numPlayers, races of players, static info needed
    }

    dbgAssertOrIgnore(numPlayers > 0);
    dbgAssertOrIgnore(numPlayers <= MAX_MULTIPLAYER_PLAYERS);
    universe.numPlayers = numPlayers;

    unitCapInitStatics(numPlayers);
/*
    if (nisEnabled && utyTeaserPlaying != NULL)
    {
        mouseCursorShow();
        utyTeaserEnd();
    }
*/
    // CAT: gameStart 0x005a9290 starts no task bar (the exe has no tbStartup / tbShutdown: TaskBar.c keeps
    //      tbBumperProcess, tbForceTaskbar and the helpers, never set up; M2-R8 kasf library removed the last users)
    //      and clears the trade flag only (0x00c37494); its glcActivate(FALSE) is the glcompat layer's (not in the
    //      SDL build, decision 28)
    wkTradeStuffActive = FALSE;

    //reset deterministic build process
    cmDeterministicReset();

    //free the alternate list
    partShutdown();

    if (loadfilename == NULL)
    {
        if (singlePlayerGame)
        {
            aiplayerStartup(2,1,1);     // tpGameCreated not valid for singlePlayerGame
        }
        else
        {
            aiplayerChangeBigotry(tpGameCreated.aiplayerBigotry);
            // CAT: gameStart 0x005a9371 passes the exe's numPlayers word (0x00a43cac), tpGameCreated.numPlayers (the
            //      word at 0x008860ee, the human players: cat_aisSetupAlliances takes it as the first computer's index)
            //      and tpGameCreated.numComputers (the byte at 0x008860d4); this tree's arguments are the same three
            aiplayerStartup(numPlayers, tpGameCreated.numPlayers, tpGameCreated.numComputers);
        }


        utySet2(SS2_AIPlayer);

        cpColorsAddToPreviousList(utyBaseColor, utyStripeColor);

        for (i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
        {
            // initialize player alliance information
            universe.players[i].Allies = 0;
            universe.players[i].AllianceProposals = 0;
            universe.players[i].AllianceProposalsTimeout = 0;
        }
    }

    if (multiPlayerGame)
    {
        dbgAssertOrIgnore(numPlayers == tpGameCreated.numPlayers+tpGameCreated.numComputers);
        dbgAssertOrIgnore(loadfilename == NULL);                    // don't allow loading of games for network games

#if UTY_PLAYER_LOGGING
        logfileLogf("PlayerStart.log", "Starting '%s'\n", tpGameCreated.Name);
#endif
        for (i=0;i<numPlayers;i++)
        {
            // later check if Colors Picked?  Fix later
            if (i <numPlayers-tpGameCreated.numComputers)
            {
                // human player setup
                if (tpGameCreated.playerInfo[i].colorsPicked)
                {                                           //if this player picked their color properly
                    teTeamColorsSet(i, tpGameCreated.playerInfo[i].baseColor, tpGameCreated.playerInfo[i].stripeColor);
                }
                else
                {
                    teTeamColorsSet(i, teColorSchemes[i].textureColor.base,teColorSchemes[i].textureColor.detail);
                }
                universe.players[i].race = tpGameCreated.playerInfo[i].race;
                dbgAssertOrIgnore(strlen(tpGameCreated.playerInfo[i].PersonalName) < MAX_PERSONAL_NAME_LEN);
                strcpy(playerNames[i],tpGameCreated.playerInfo[i].PersonalName);
            }
            else
            {
                universe.players[i].race = tpGameCreated.playerInfo[i].race;
                if (i == 1 && cpColorsPicked)
                {                                           //if second player is computer and player picked colors
                    if (universe.players[1].race == RACE_Sect)
                    {                                       //choose color appropriate to that race
                        teTeamColorsSet(1, teRace1Default.base, teRace1Default.detail);
                    }
                    else if (universe.players[1].race == RACE_Beast)
                    {                                       // CAT: no colour for another race (0x005a9290)
                        teTeamColorsSet(1, teRace2Default.base, teRace2Default.detail);
                    }
                }
                else
                {
                    teTeamColorsSet(i, teColorSchemes[i].textureColor.base,teColorSchemes[i].textureColor.detail);
                }
                sprintf(playerNames[i], "%s %i", strGetString(strComputerName), i-sigsNumPlayers+1);
            }
#if UTY_PLAYER_LOGGING
            logfileLogf("PlayerStart.log", "%u %u 0x%x 0x%x %s\n", i, tpGameCreated.playerInfo[i].race, teColorSchemes[i].textureColor.base, teColorSchemes[i].textureColor.detail, tpGameCreated.playerInfo[i].PersonalName);
#endif
        }

        useScenarioFile = tpGameCreated.MapName;
    }
    else
    {
        ShipRace otherRace;

        if (loadfilename == NULL)
        {
            if (cpColorsPicked)
            {
                teTeamColorsSet(0, utyBaseColor, utyStripeColor);
            }
            else
            {
                teTeamColorsSet(0, teColorSchemes[0].textureColor.base, teColorSchemes[0].textureColor.detail);
            }

            if (tutorial==1)
            {
                // CAT: the tutorial is always the Sect against the Beast (gameStart 0x005a9290; HW1 took the race
                //      picked in the front end)
                universe.players[0].race = RACE_Sect;
                universe.players[1].race = RACE_Beast;
                ComputerPlayerEnabled[1] = TRUE;
            }
            else
            {
                useScenarioFile = spScenarios[spCurrentSelected].fileSpec;

                // CAT: gameStart 0x005a9290: a single-player game is always the Sect (player 0) against the Beast; a
                //      skirmish gives player 0 the picked race and the others the other race and the picked one by
                //      turns (HW1: every other player the other race)
                if (singlePlayerGame)
                {
                    universe.players[0].race = RACE_Sect;
                    otherRace = RACE_Beast;
                }
                else
                {
                    universe.players[0].race = (whichRaceSelected != RACE_Sect) ? RACE_Beast : RACE_Sect;
                    otherRace = (whichRaceSelected == RACE_Sect) ? RACE_Beast : RACE_Sect;
                }

#ifdef HW_BUILD_FOR_DEBUGGING
                dbgMessagef("player is race %u", universe.players[0].race);
#endif

                // CAT: gameStart 0x005a9290 sets the start-position table to the 8 indices 0..7 here (read by the MissionSphere
                //      readers scriptSetMissionSphereCB 0x004a2420 / scriptPreMissionSphereCB 0x004a2820), then shuffles it
                //      with 20 swaps of two stream-0 picks modulo the picked scenario's player count (0x005a9644 ..
                //      0x005a96a7: both picks are drawn even when they are equal, then nothing is swapped); every
                //      non-tutorial new game draws these 40 (M2-R8 kas-runtime-mission01: retires the identity table)
                for (i = 0; i < MAX_MULTIPLAYER_PLAYERS; i++) catStartPositions[i] = i;
                {
                    udword maxplayers = spScenarios[spCurrentSelected].maxplayers;
                    udword a, b, swap;
                    sdword n;

                    for (n = 0; n < 20; n++)
                    {
                        a = ranRandom(0);
                        b = ranRandom(0);
                        if (maxplayers == 0)
                        {
                            continue;       // CAT-FIX: the exe divides by the count unchecked (a scenario of no player)
                        }
                        a %= maxplayers;
                        b %= maxplayers;
                        if ((a != b) && (a < MAX_MULTIPLAYER_PLAYERS) && (b < MAX_MULTIPLAYER_PLAYERS))   // CAT-FIX: within the table
                        {
                            swap = (udword)catStartPositions[a];
                            catStartPositions[a] = catStartPositions[b];
                            catStartPositions[b] = (sdword)swap;
                        }
                    }
                }
                for (i = 1; i < numPlayers; i++)
                {
                    universe.players[i].race = otherRace;
                    otherRace = (otherRace == RACE_Sect) ? RACE_Beast : RACE_Sect;
                }
                if (universe.players[1].race == RACE_Sect)
                {                                       //choose color appropriate to that race
                    teTeamColorsSet(1, teRace1Default.base, teRace1Default.detail);
                }
                else if (universe.players[1].race == RACE_Beast)
                {                                       // CAT: neither Sect nor Beast: no colour set (0x005a9290)
                    teTeamColorsSet(1, teRace2Default.base, teRace2Default.detail);
                }

                if (!noDefaultComputerPlayer)
                {
                    if ((numPlayers == 2) && (!singlePlayerGame))
                    {
                        ComputerPlayerEnabled[1] = TRUE;
                    }
                }

                if (singlePlayerGame)
                {
                    ComputerPlayerEnabled[1] = TRUE;
                }
            }
        }
        else
        {
            for (i=0;i<universe.numPlayers;i++)
            {
                teTeamColorsSet(i, teColorSchemes[i].textureColor.base, teColorSchemes[i].textureColor.detail);
            }
        }
    }

    if (multiPlayerGame|playPackets)
    {
        gcStartup();
        utySet2(SS2_GameChat);
    }

    //universeDefaultTeamColors(); NO LONGER NEEDED - levelPreInit calls this.  Also, SaveGame doesn't need this.

    if (tutorial==1)
    {
        tutPreInitTutorial(dirfile, levelfile);
    }
    else if (!singlePlayerGame)
    {
        if (loadfilename == NULL)
        {
            sprintf(levelfile,useScenarioFile,numPlayers);
            strcpy(dirfile,"MultiPlayer\\");
            strcat(dirfile,levelfile);
            strcat(dirfile,"\\");
            strcat(levelfile,".level");

            //perform a level pass to see what ships we need to load
            levelPreInit(dirfile, levelfile);
        }
        // else PreLoadGame already figures out what ships we need to load
    }
    else
    {
        //loadfilename == NULL then spGetCurrentMission() set from singlePlayerInit called earlier
        //else loadfilename != NULL then spGetCurrentMission() will be set from PreLoadGame
        //!!!do a pass on the first SP level and find what ships/textures are needed
        if (loadfilename == NULL)
        {
            // CAT: a new game at Mission01 preloads its NIS staging level, the level it starts in (gameStart
            //      0x005a9290: GetMissionsDirAndFile(currentMission, currentMission == 1))
            bool32 nisLevel = (spGetCurrentMission() == MISSION_1_KHARAK_SYSTEM);
#if HWI_ORACLE
            if (oracleMissionDirectStart())
            {
                nisLevel = FALSE;                           // HWI-ORACLE: the mission run starts in Mission01 itself
            }
#endif
            GetMissionsDirAndFile(spGetCurrentMission(), nisLevel);
            levelPreInit(spMissionsDir, spMissionsFile);
        }
        // else PreLoadGame already figures out what ships we need to load
    }

    if (gatherStats|showStatsFight|showStatsFancyFight)
    {
        dbgAssertOrIgnore(loadfilename == NULL);        // can't load games when doing stats stuff
        if (showStatsFancyFight)
            FancyFightPreLoad();
        // CAT: no universeFlagEverythingNeeded for the other two (gameStart 0x005a9290)
    }

    /* pause sound engine */
    soundEventPause(TRUE);

    //load in all the ships, register all the textures
    universeStaticInit();
    if (hrAbortLoadingGame)
    {
        goto abortloading;
    }

    if (loadfilename == NULL)
    {
        universe.numPlayers = numPlayers;

        //initialize stats gathering variables
        initGameStats(&universe);
        BountyInit(tpGameCreated.bountySize);
        memset(&universe.players[MAX_MULTIPLAYER_PLAYERS],0,sizeof(Player));
        universe.players[MAX_MULTIPLAYER_PLAYERS].playerIndex = MAX_MULTIPLAYER_PLAYERS;
        for (i=0;i<universe.numPlayers;i++)
        {
            player = &universe.players[i];

            if (player == universe.curPlayerPtr)
            {                                               // CAT: the current player's managers closed first
                rmCloseIfOpen();                            //      (gameStart 0x005a9290)
                cmCloseIfOpen();
                lmCloseIfOpen();
            }

            player->PlayerMothership = NULL;
            player->resourceUnits = 0;
            player->autoLaunch = BIT0 | BIT1 | BIT2 | BIT3 | BIT4;
            player->playerState = PLAYER_ALIVE;
            player->bounty        = TW_BountySizes[tpGameCreated.bountySize];

                        // Initialize units to zero for this player
            unitCapInitialize(player);

            // Initialize research structure, only for computer players, and human players on this machine

            // ****** IMPORTANT we have changed the research so that it is deterministic on all machines.
            // CAT: gameStart 0x005a9290: a human player gets everything when the word at 0x00a75240 is set, a computer
            //      player everything when it is set or in a non-tutorial single-player game; otherwise an empty set
            //      (-1). M2-R6 startup-keys: that word has no writer in the exe (three reads, all in gameStart), so it is
            //      0 here; the research-off case is the tech-set pass after the level starts (below)
            {
                const bool32 researchOff = FALSE;           // 0x00a75240
                sdword techLevel;
                if (!ComputerPlayerEnabled[i])
                    techLevel = researchOff ? TECH_ALLTECHNOLOGY : TECH_NOTECHNOLOGY;
                else if (!researchOff)
                    techLevel = ((!singlePlayerGame) || (tutorial != 0)) ? TECH_NOTECHNOLOGY : TECH_ALLTECHNOLOGY;
                else
                    techLevel = TECH_ALLTECHNOLOGY;
                rmInitializeResearchStruct(player, TRUE, techLevel);
            }

            player->sensorLevel = 0;                        // CAT: always 0 (gameStart 0x005a9290; no /sensorLevel)
            player->playerIndex = (uword)i;
            player->aiPlayer = NULL;
            if (ComputerPlayerEnabled[i])
            {
                if (singlePlayerGame)
                    player->aiPlayer = aiplayerInit(player,AI_ADVANCED);
                else                                        // CAT: the level itself (gameStart 0x005a9290: the
                                                            //      low byte; opCPUDifficulty sets it by radio button)
                    player->aiPlayer = aiplayerInit(player,(AIPlayerLevel)tpGameCreated.aiplayerDifficultyLevel);

            }
            player->timeMoShipAttacked = universe.totaltimeelapsed;
        }
    }

    trailsRecolorize();
    mistrailsRecolorize();

    tmClearTechs();

    if (loadfilename == NULL)
    {
        univupdateReset();                                      //reset univupdate
        // CAT: no ranStartup here (gameStart 0x005a9290, 0x005a9a98): a new non-single-player game resets streams
        //      1..26 with cat_ranResetGameStreams; stream 0 keeps its start-up wall-clock offset, and a new
        //      single-player game keeps every stream as it is (incl. aiplayerStartup's ranRandomize of stream 6)
        if (!singlePlayerGame)
        {
            cat_ranResetGameStreams();
        }
    }
    else
    {
        LoadGame(loadfilename);
    }

    universeSwitchToPlayer(curPlayer);                      // CAT: gameStart 0x005a9290 (new or loaded game)

#ifndef HW_GAME_DEMO
    // CAT: switch the speech set to the current player's race (gameStart 0x005a9290: after universeSwitchToPlayer,
    //      if speech is on, cat_speechEventLoadRaceVoice(universe.players[curPlayerIndex].race))
    if (enableSpeech)
    {
        cat_speechEventLoadRaceVoice(universe.players[universe.curPlayerIndex].race);
    }
#endif
    // CAT (M2-R4 combat): a new game starts with empty blast lists and fresh lightning tables (gameStart 0x005a9290;
    //      a loaded game rebuilds them in its loader, cat_loadUniverseList5e288: the save format is the user's saves')
    if (loadfilename == NULL)
    {
        cat_blastInit();
    }

    //now start the level
    if (tutorial==1)
    {
        aiCurrentAIPlayer = universe.players[1].aiPlayer;
        aiCurrentAIPlayer->primaryEnemyPlayer = &universe.players[0];
        if (loadfilename == NULL)
            tutInitTutorial(dirfile, levelfile);
        tutInitialize();
    }
    else if (singlePlayerGame)
    {
        aiCurrentAIPlayer = universe.players[1].aiPlayer;
        aiCurrentAIPlayer->primaryEnemyPlayer = &universe.players[0];
        if (loadfilename == NULL)
        {
            catSpStartInLoadingScreen = TRUE;               // CAT: 0x00a6ac68 around the start (gameStart 0x005a9290)
            singlePlayerStartGame();
            catSpStartInLoadingScreen = FALSE;
        }
    }
    else
    {
        if (showStatsFancyFight)
        {
            smGhostMode = TRUE;
            dbgAssertOrIgnore(loadfilename == NULL);
            statsLoadFightStats();
            statsShowFancyFight(showStatsFightI,showStatsFightJ);
        }
        else if (showStatsFight)
        {
            dbgAssertOrIgnore(loadfilename == NULL);
            statsLoadFightStats();
            statsShowFight(showStatsFightI,showStatsFightJ);
        }
        else
        {
            if (gatherStats)
            {
                dbgAssertOrIgnore(loadfilename == NULL);
                statsGatherFightStats();
            }
            else
            {
                statsLoadFightStats();
            }
            if (loadfilename == NULL)
                levelInit(dirfile,levelfile);
        }

        if (loadfilename == NULL)
            for (i=0;i<universe.numPlayers;i++)
            {
                // CAT: gameStart 0x005a9290: a computer player (an index past the humans) gets five times the
                //      starting resources (2000 for the "none" setting) when flag bit 5 is set (HW1's
                //      MG_HarvestinEnabled bit; Cataclysm has no harvesting switch and no rmEnableShip pass here)
                sdword factor = 1;
                universe.gameStats.startingResources = 10;
                if ((i >= numPlayers - tpGameCreated.numComputers) && bitTest(tpGameCreated.flag,MG_HarvestinEnabled))
                {
                    factor = 5;
                    universe.gameStats.startingResources = 2000;
                }
                switch (tpGameCreated.startingResources)
                {
                    case MG_Small :
                        universe.gameStats.startingResources = resourceStartSmall * factor;
                    break;
                    case MG_Medium :
                        universe.gameStats.startingResources = resourceStartMedium * factor;
                    break;
                    case MG_Large :
                        universe.gameStats.startingResources = resourceStartLarge * factor;
                    break;
                    default:
                    break;
                }
                universe.players[i].resourceUnits = universe.gameStats.startingResources;
            }
    }

    if (recordplayPacketsInGame)
    {
        smGhostMode = TRUE;
    }

    if (loadfilename == NULL)
    {
        dbgMessagef("SYNC NUMBER: %u",gamerand());

        // CAT: no rmAddTechToPlayer pass; instead every player's tech set is set again (gameStart 0x005a9290, M2-R6
        //      startup-keys): everything in a non-tutorial skirmish / multiplayer game with research off (flag bit 3
        //      clear), else nothing (the 0x00a75240 word that would give everything has no writer); the human player
        //      keeps its set while the single-player warp is on; then the upgrades the set gives
        {
            sdword techlevel = TECH_NOTECHNOLOGY;
            if ((!singlePlayerGame) && (tutorial == 0) && !bitTest(tpGameCreated.flag, MG_ResearchEnabled))
                techlevel = TECH_ALLTECHNOLOGY;
            for (i=0;i<universe.numPlayers;i++)
            {
                if ((i != 0) || !cat_spWarpToLevelEnabled())
                {
                    cat_rmTechSetInit(universe.players[i].researchinfo.HasTechnology, techlevel);
                    cat_playerUpdateUpgradesFromResearch(&universe.players[i]);
                }
            }
        }
        for (i=0;i<universe.numPlayers;i++)
        {
            player = &universe.players[i];
            if (player->aiPlayer != NULL)
            {
                aiplayerGameStart(player->aiPlayer);
            }
        }
    }
    else
    {
        soundEventPlayMusic(SongNumber);
    }

    if (etgHasBeenStarted)
    {                                                       //if effects already started
        etgReloadReset();
    }
    else
    {
        etgStartup();
        utySet(SSA_Effect);
        etgHasBeenStarted = TRUE;
    }
    pingReset();                                            //clear out all pings

    //clear out any persistent subtitles
    subReset();
    subTexturesReset();

    cat_consoleStartup();                                   // CAT: the developer console (gameStart 0x005a9290: after
    utySet2(SS2_CatConsole);                                //      subTexturesReset; Console.c)

    trRegistryRefresh();                                    //refresh registry - load in all textures
    if (hrAbortLoadingGame)
    {
        goto abortloading;
    }
    etgFixupUV();
    utySet(SSA_RegistryRefreshed);

    for (;;)
    {
        if (hrAbortLoadingGame)
        {
            break;
        }

        if (HorseRaceNext(1.0f))
        {
            break;
        }
    }

abortloading:

    if (hrAbortLoadingGame)
    {                                                       //if loading was aborted
        trSetAllPending(TRUE);
    }

//    rmRemoveAllUnneededTech();

    bobInitProperties();                                    // CAT: 0x00425ca0, a jump to bobResetProperties (Blobs.c)

    if (loadfilename == NULL)
    {                                                       // CAT: a new game's mimic lists (gameStart 0x005a9a.. -> 0x005b9c60)
        cat_mimicInitDisguiseTables();
    }

    if (!hrAbortLoadingGame)
    {
        rndSetClearColor(universe.backgroundColor|0xff000000);
    }
    
    /* restore sound engine */
    soundEventPause(FALSE);

    if(tutorial==TUTORIAL_ONLY)
    {
        soundEventPlayMusic(CAT_MUSIC_TUTORIAL);            // CAT: track 0x12 (gameStart 0x005a9290; HW1 SongNumber)
    }

    // CAT: a new game applies the upgrades to every ship and moves what is clamped to a host (gameStart 0x005a9290:
    //      cat_shipApplyUpgrades, then updateClampedObject for a ship whose clampInfo holds a clamped-object list)
    if (loadfilename == NULL)
    {
        Node *shipnode;
        for (shipnode = universe.ShipList.head; shipnode != NULL; shipnode = shipnode->next)
        {
            Ship *ship = (Ship *)listGetStructOfNode(shipnode);
            cat_shipApplyUpgrades(ship);
            ClampInfo *clampInfo = ((SpaceObjRotImpTargGuidance *)ship)->clampInfo;
            if ((clampInfo != NULL) && (clampInfo->clampedObjs != NULL))
            {
                updateClampedObject((SpaceObjRotImpTargGuidance *)ship);
            }
        }
    }
    // CAT: the skirmish AI's 8 play timers and its alliance flag cleared (gameStart 0x005a9ef4 calls cat_aisReset
    //      0x00433d30 after the ships' upgrades)
    cat_aisReset();
    // CAT-TODO(stage 5 LAN): a multiplayer game sums the eight colour schemes' first words into 0x00ab7658, the fourth word of netcheckShow's ring that netcheckFillInChecksum sends (gameStart 0x005a9290)

    // HWI: GoK's joystick camera (not in the exe; GoK's SDL input layer stays, decision 28)
    cameraJoystickReset();

    gameIsRunning = TRUE;

#if HWI_ORACLE
    // HWI-ORACLE: 'E' + tick-0 'T' record of the state right after start/load (hook H7 samples gameStart's exit)
    oracleEmitGameStart(loadfilename);
#endif
}

/*-----------------------------------------------------------------------------
    Name        : gameEnd
    Description : ends the game
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void gameEnd(void)
{
    sdword index;

    // CAT: gameEnd 0x005a9f40 first re-activates the glcompat layer when a software renderer runs (glcActivate):
    //      not in the SDL build (decision 28)

    //reset the colors schemes to prevent re-entrant bugs
    teReset();

    //clear out any persistent subtitles
    subReset();

    if (singlePlayerGame)
        tutRemoveAllPointers();

    if (multiPlayerGame)
    {
        SendInGameQuittingPacket(sigsPlayerIndex);
        TransferCaptaincyGameEnded();
        if (TitanActive)
        {
            titanPumpEngine();
        }
        SDL_Delay(100);     // Give titan some time to handle this

        if (TitanActive)
        {
            titanGameEnded();
        }
        else
        {
            // later add other non-titan game ended callbacks
        }
    }

    // reset player alliance information
    for (index=0;index<MAX_MULTIPLAYER_PLAYERS;index++)
    {
        universe.players[index].Allies = 0;
        universe.players[index].AllianceProposals = 0;
        universe.players[index].AllianceProposalsTimeout = 0;
    }

    bobResetProperties();

    if (utyTest2(SS2_GameChat))
    {
        gcShutdown();
        utyClear2(SS2_GameChat);
    }

    // CAT: no writeGameStatsToFile (gameEnd 0x005a9f40)

    universeClosePlayers();

    rmResetStaticInfo();

    hsStaticReset();

    if (singlePlayerGame)
    {
        singlePlayerClose();
    }

    if (utyTest2(SS2_GamePicker))
    {
        gpReset();
    }

    tacticsShutDown();

//    soundEventReset();

    cloudReset();

    nebReset();

    btgReset();

    alodReset();

    universeReset();

    if (utyTest2(SS2_AIPlayer))
    {
        aiplayerShutdown();
        utyClear2(SS2_AIPlayer);
    }

    netcheckClose();

#ifdef GOD_LIKE_SYNC_CHECKING
    if(syncDumpOn)
    {
        godSyncShutDown();
    }
#endif

    if (playPackets)
    {
        recPackPlayClose();
    }

    if(tutorial==1)
        tutUnInitialize();

    pingAllPingsDelete();

    universeStaticClose();                                  // CAT: an empty function in the exe too (0x0049ae30)

    universeSetZeroPlayers();

    selReset();

    ioUpdateShipTotals();

    globalsReset();
    if (utyTest2(SS2_NetworkQueues))
    {
        autodownloadmapReset();
        KeepAliveReset();
        ResetQueue(&ProcessCmdPktQ);
        ResetQueue(&ProcessSyncPktQ);
        ResetQueue(&ProcessRequestedSyncPktQ);
        ResetQueue(&ProcessCaptaincyPktQ);
        ResetLastSyncPktsQ();
    }

    rndSetClearColor(colBlack);

    trTextureDeleteAllUnregistered();
    mrReset();                                              //reset the state of the main viewport region

    // CAT: no task bar to shut down (the exe has no tbShutdown; gameStart starts none)

    //reset the scissor bars and fade out maintained by NIS's
    nisScissorBarsReset();

    //make sure the mouse is not erroneously clipped to anything
    mouseClipToRect(NULL);

    // CAT: gameEnd 0x005a9f40 ends with the options saved (opOptionsSaveSettings: the options file and keys.cfg) and
    //      the pause flag cleared (0x00a6ed74, the Pause key's: UnivUpdate.c)
    opOptionsSaveSettings();
    catUnivFrameStep = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : utyMultiplayerGameStart
    Description : Starts up the screen for multplayer game.
    Inputs      : name
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    Name        : cat_utyMultiplayerQuitNotify
    Description : CAT: the end of a multiplayer game told to the others, as gameEnd does it (0x005aa0f0; the game
                  stats screen's only call, cat_gameStatsBegin)
----------------------------------------------------------------------------*/
void cat_utyMultiplayerQuitNotify(void)
{
    if (multiPlayerGame)
    {
        SendInGameQuittingPacket(sigsPlayerIndex);
        TransferCaptaincyGameEnded();
        if (TitanActive)
        {
            titanPumpEngine();
        }
        SDL_Delay(100);     // Give titan some time to handle this
        if (TitanActive)
        {
            titanGameEnded();
        }
    }
}

void utyMultiplayerGameStart(char *name, featom *atom)
{
    mgStartMultiPlayerGameScreens(ghMainRegion,0,0,0, FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : utyShowCredits
    Description : Starts the Credits a showin!!
    Inputs      : atom name and atom
    Outputs     : Why Credits of course
    Return      : void
----------------------------------------------------------------------------*/
void utyShowCredits(char *name, featom *atom)
{
    soundEventPlayMusic(CAT_MUSIC_CREDITS);                 // CAT: utyShowCredits 0x005aa160 (HW1 RollCredits 28)
    psModeBegin("Credits\\", PMF_Credits | PMF_LanguageSpecific | PMF_MusicTrack);
    psScreenStart("Credits-button.cred");
}

/*-----------------------------------------------------------------------------
    Name        : utyNewGameRace
    Description : Race select callback
    Inputs      : name - name of entry to envoke this function
    Outputs     : modifies whichRaceSelected (== RACE_Sect or RACE_Beast)
    Return      :
----------------------------------------------------------------------------*/
#if 0
void utyNewGameRace(char *name, featom *atom)
{
    udword raceOne, raceTwo;
    ShipRace oldRace = whichRaceSelected;
    tecolorscheme tempScheme;

    if (!FEFIRSTCALL(atom))
    {
        whichRaceSelected = (!strcmp(name, "CB_NewGameRaceOne")) ? RACE_Sect : RACE_Beast;
        if (whichRaceSelected != oldRace && (!cpColorsPicked))
        {                                                   //if new race selected and no colors picked
            swap(teColorSchemes[0], teColorSchemes[1], tempScheme);//swap default Sect/Beast color schemes
            utyBaseColor = teColorSchemes[0].textureColor.base;
            utyStripeColor = teColorSchemes[0].textureColor.detail;
            cpColorsReset(&utyBaseColor, &utyStripeColor);
            //cpColorsReset(&teColorSchemes[0].textureColor.base, &teColorSchemes[0].textureColor.detail);
        }
    }
    // ensure non-selected button Off, selected On
    raceOne = (whichRaceSelected == RACE_Sect) ? TRUE : FALSE;
    raceTwo = (whichRaceSelected == RACE_Beast) ? TRUE : FALSE;
/*    if (!FEFIRSTCALL(atom))
    {
        // ... or we'll be toggled elsewhere again
        if (raceOne) raceOne = FALSE;
        if (raceTwo) raceTwo = FALSE;
    }*/
    feToggleButtonSet("CB_NewGameRaceOne", raceOne);
    feToggleButtonSet("CB_NewGameRaceTwo", raceTwo);
    cpPreviewImageDelete();
}
#endif

/*-----------------------------------------------------------------------------
    This is a table of state information that will be saved/restored surrounding
    demo playback.  This list is designed to be expandable and backward-compatable
    as long as new entries are added at the end of the list and the order is not
    changed.
-----------------------------------------------------------------------------*/

#define DST_Dword               0
#define DST_Bool                1
#define DST_Real32              2
#define DST_Byte                3
#define DST_Bool8               4
#define DST_StringOfSize        BIT8
#define DST_StructOfSize        BIT16
struct
{
    udword type;
    void *data;
}
utyDemoStateTable[] =
{
    {DST_Bool, &smInstantTransition},
    {DST_Real32, &taskTimeElapsed},
    {DST_Bool, &determCompPlayer},
    {DST_Bool, &noDefaultComputerPlayer},
    {DST_Real32, &CAMERA_MOUSE_SENS},
    {DST_Dword, &batGlobalFrequencyModifier},
    {0, NULL},
};

/*-----------------------------------------------------------------------------
    Name        : utyPreDemoStateSaveCB
    Description : A demo recording callback, this function saves the state of
                    various user options before the demo starts recording.
    Inputs      :
    Outputs     : buffer - pointer allocated by memAlloc() where the state info
                    is stored.  This will be freed by the caller of this function.
                  length - length of aforementioned buffer.
    Return      :
----------------------------------------------------------------------------*/
#define BUFFER_GRANULARITY      256
void utyPreDemoStateSaveCB(ubyte **buffer, sdword *length)
{
    ubyte *stateBuffer, *newPointer;
    sdword bufferLength = BUFFER_GRANULARITY, bufferPos, index, newType;
    sdword newSize = 0;

    stateBuffer = memAlloc(BUFFER_GRANULARITY, "PreDemoState", Pyrophoric);

    for (index = bufferPos = 0; utyDemoStateTable[index].data != NULL; index++)
    {
        newPointer = utyDemoStateTable[index].data;         //default pointer
        newType = utyDemoStateTable[index].type;            //default type
        if (utyDemoStateTable[index].type >= DST_StructOfSize)
        {                                                   //save a structure of a given size
            newSize = utyDemoStateTable[index].type - DST_StructOfSize;
        }
        else
        {
            switch (utyDemoStateTable[index].type)
            {                                               //save default types of know sizes
                case DST_Dword:
                case DST_Bool:
                case DST_Real32:
                    newSize = sizeof(udword);
                    break;
                case DST_Byte:
                case DST_Bool8:
                    newSize = sizeof(ubyte);
                    break;
                case DST_StringOfSize:
                    newPointer = *((ubyte **)utyDemoStateTable[index].data);
                    newSize = strlen((char *)newPointer) + 1;
                    newType = DST_StringOfSize + newSize;
                    break;
                default:
                    dbgAssertOrIgnore(FALSE);
            }
        }
        if (bufferPos + newSize + sizeof(udword) >= bufferLength)
        {                                                   //we may bave to grow out the buffer
            bufferLength = max(bufferLength + BUFFER_GRANULARITY, bufferPos + newSize + sizeof(udword));
            stateBuffer = memRealloc(stateBuffer, bufferLength, "REDemoState", Pyrophoric);
        }
        *((sdword *)(stateBuffer + bufferPos)) = newType;
        bufferPos += sizeof(udword);
        memcpy(stateBuffer + bufferPos, newPointer, newSize);//copy the data of given type
        bufferPos += newSize;
    }

    *buffer = stateBuffer;
    *length = bufferPos;
}

/*-----------------------------------------------------------------------------
    Name        : utyPreDemoStateLoad
    Description : Load a bunch of options as defined in utyDemoStateTable from
                    a buffer.
    Inputs      : stateBuffer - buffer to load options fron
                  stateSize - length of that buffer.
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyPreDemoStateLoad(ubyte *stateBuffer, sdword stateSize)
{
    sdword index, statePos, type, sizeToRead = 0;
    ubyte *readTo;

    for (index = statePos = 0; utyDemoStateTable[index].data != NULL && statePos < stateSize; index++)
    {                                                       //scan through items in table
        type = *((sdword *)(stateBuffer + statePos));       //read in the type dword
        statePos += sizeof(sdword);
        readTo = utyDemoStateTable[index].data;             //default read address
        if (type > DST_StructOfSize)
        {                                                   //it's a structure; figure a size
            sizeToRead = type - DST_StructOfSize;
        }
        else if (type > DST_StringOfSize)
        {                                                   //it's a string; figure a size
            sizeToRead = type - DST_StringOfSize;
            readTo = *((ubyte **)readTo);
        }
        else
        {                                                   //default types (dwords and bytes)
            switch (type)
            {
                case DST_Dword:
                case DST_Bool:
                case DST_Real32:
                    sizeToRead = sizeof(udword);
                    break;
                case DST_Byte:
                case DST_Bool8:
                    sizeToRead = sizeof(ubyte);
                    break;
            }
        }
        memcpy(readTo, stateBuffer + statePos, sizeToRead); //copy the data to where it should go
        statePos += sizeToRead;                             //increment the buffer index
    }
}

/*-----------------------------------------------------------------------------
    Name        : utyPreDemoStateLoadCB
    Description : A demo recording callback to load in a user-defined state.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
//these variables store the backup of options saved for demo playback
ubyte *utyDemoOptionsSaved = NULL;
sdword utyDemoOptionsLength = 0;
//position of mouse when the baby callback was created
sdword utyDemoWaitMouseX;
sdword utyDemoWaitMouseY;
void utyPreDemoStateLoadCB(ubyte *stateBuffer, sdword stateSize)
{
    utyPreDemoStateSaveCB(&utyDemoOptionsSaved, &utyDemoOptionsLength);//put options somewhere to be restored later
    utyPreDemoStateLoad(stateBuffer, stateSize);            //now load the options from disk
}

/*-----------------------------------------------------------------------------
    Name        : utyDemoFinishedCB
    Description : A demo callback, this function performs cleanup at the end of a demo.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyDemoFinishedCB(void)
{
    utyPreDemoStateLoad(utyDemoOptionsSaved, utyDemoOptionsLength);
    memFree(utyDemoOptionsSaved);
    utyDemoOptionsSaved = NULL;
    utyDemoOptionsLength = 0;
#if DEM_INTERRUPTABLE
    if (!demPlaybackInterrupted)
#endif
    {                                                       //if demo ended normally, i.e. was not interrupted
        if (gameIsRunning)
        {
            //... if game is running, end the game
            utyGameQuitToMain(NULL, NULL);
        }
        else
        {
            //... if not at main screen, go to main screen
            feAllScreensDelete();
            feScreenStart(ghMainRegion, "Main_game_screen");
        }
    }
}

#if DEM_AUTO_DEMO
/*-----------------------------------------------------------------------------
    Name        : utyDemoAutoPlay
    Description : Baby timer callback to start a demo after chilling at the
                    startup screen for a while.
    Inputs      :
    Outputs     :
    Return      : TRUE - don't try this again, dag nabbit!
                  FALSE - we'd like to try this again later.
----------------------------------------------------------------------------*/
bool32 utyDemoAutoPlay(udword num, void *data, struct BabyCallBack *baby) {
    sdword index, attempt, chosen;
    char *names[DEM_NumberNames];
    char string[256];
    filehandle handle;

    if (demDemoRecording || demDemoPlaying || (!demAutoDemo))
    {                                                       //don't play any demos if we're already playing one
        return(TRUE);
    }
#if DEM_VERBOSE_LEVEL >= 1
    dbgMessage("Attempting to play a demo...");
#endif
    if (mouseCursorX() != utyDemoWaitMouseX || mouseCursorY() != utyDemoWaitMouseY || taskTimeElapsed - keyLastTimeHit < demAutoDemoWaitTime)
    {
        utyDemoWaitMouseX = mouseCursorX();                 //re-save the mouse position
        utyDemoWaitMouseY = mouseCursorY();
        return(FALSE);                                      //the user was moving the mouse or hitting keys.  Let's try this again later.
    }
    if (feStackIndex >= 0 && feStack[feStackIndex].screen != NULL)
    {
        if (feAtomFindInScreen(feStack[feStackIndex].screen, "SinglePlayerOptions"))
        {                                                   //if we really are on the main screen when this sucker elapses
            handle = fileOpen("demos\\AutoDemos.list", FF_TextMode);//open the demo listing file
            for (index = 0; index < DEM_NumberNames; index++)
            {                                               //read in a bunch of demo file names
                if (fileLineRead(handle, string, 256) == FR_EndOfFile)
                {
                    break;
                }
                names[index] = memStringDupe(string);
            }
            fileClose(handle);                              //close the file

            for (attempt = 0; attempt < DEM_NumberTries; attempt++)
            {
                chosen = ranRandom(RANDOM_SOUND) % index;      //choose a demo to play (sound random stream is non-deterministic)
                sprintf(string, "%s%ux%u.dem", names[chosen],//get the resolution-dependent name
                        MAIN_WindowWidth, MAIN_WindowHeight);

#if DEM_VERBOSE_LEVEL >= 1
                dbgMessagef("Trying to play '%s'...", string);
#endif
                if (fileExists(string, 0))
                {                                           //see if this demo exists in this resolution
#if DEM_VERBOSE_LEVEL >= 1
                    dbgMessagef("playing...", string);
#endif
                    demDemoPlaying = TRUE;
                    demPlayStart(string, utyPreDemoStateLoadCB, utyDemoFinishedCB);
                    break;
                }
#if DEM_VERBOSE_LEVEL >= 1
                else
                {
                    dbgMessagef("Doesn't exist.  Try again.", string);
                }
#endif
            }
            for (index--; index >= 0; index--)
            {                                               //free all the name strings
                memFree(names[index]);
            }

        }
    }
    return(TRUE);                                           //don't call me here anymore
}
#endif //DEM_AUTO_DEMO

/*-----------------------------------------------------------------------------
    Name        : utySinglePlayerOptions
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utySinglePlayerOptions(char *name, featom *atom)
{
#if defined(HW_GAME_DEMO)
    featom *bitchatom;

    bitchatom = feAtomFindInScreen(feScreenFind("Main_game_screen"),"FE_SHOWCREDITS");

    bitSet(bitchatom->flags, FAF_Disabled);
    bitSet(((region *)bitchatom->region)->status, RSF_RegionDisabled);
#endif

    if (FEFIRSTCALL(atom))
    {
#if DEM_AUTO_DEMO
        //prepare to auto-play a demo after a period of time
        utyDemoWaitMouseX = mouseCursorX();
        utyDemoWaitMouseY = mouseCursorY();
        taskCallBackRegister(utyDemoAutoPlay, 0, NULL, demAutoDemoWaitTime);
#endif
        return;
    }
    
    feScreenDisappear(NULL,NULL);
    feScreenStart(ghMainRegion, "Create_new_game");
    whichRaceSelected = RACE_Sect;                          // CAT: the race picked back to the Sect (0x005aa580)
}

/*-----------------------------------------------------------------------------
    Name        : utySinglePlayerGame
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utySinglePlayerGame(char *name, featom *atom)
{
    feScreenDisappear(NULL,NULL);

    feScreenStart(ghMainRegion, "Create_new_game");
}

/*-----------------------------------------------------------------------------
    Name        : utySelectSinglePlayerGame
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utySelectSinglePlayerGame(char *name, featom *atom)
{
    gpGamePick("S");
}

void utyLoadSinglePlayerGameGivenFilename(char *filename)
{
    cpResetRegions();

    dbgMessagef("New single player game being loaded");
    feAllScreensDelete();
/*
    if (nisEnabled && utyTeaserPlaying != NULL)
    {
        mouseCursorShow();
        utyTeaserEnd();
    }
*/
    singlePlayerGame = TRUE;
    //objectivesShown = TRUE;

    if (!(tutorial==TUTORIAL_ONLY))
    {
        tutorial = TUTORIAL_SINGLEPLAYER;
    }

    singlePlayerInit();

    horseRaceInit();
    gameStart(filename);
    horseRaceShutdown();

    if (hrAbortLoadingGame)
    {
        utyGameQuitToMain(NULL,NULL);
        hrAbortLoadingGame = FALSE;
    }
    else
        singlePlayerPostInit(TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : utyLoadSinglePlayerGame
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyLoadSinglePlayerGame(char *name, featom *atom)
{
    char filename[100];
    sdword verifysavename;

    if (!gpGetGameName(name,atom,filename))
    {
        return;
    }

    if ((verifysavename = VerifySaveFile(filename)) != VERIFYSAVEFILE_OK)
    {
        if (verifysavename == VERIFYSAVEFILE_BADVERSION)
            GeneralMessageBox(strGetString(strErrorInvalidSaveGameFileVersion),NULL);
        else
            GeneralMessageBox(strGetString(strErrorInvalidSaveGameFile),NULL);
        return;
    }

    utyLoadSinglePlayerGameGivenFilename(filename);
}

/*-----------------------------------------------------------------------------
    Name        : utyLoadSinglePlayerGame
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyLoadMultiSinglePlayerGame(char *name, featom *atom)
{
    if (gpLoadSinglePlayerGame)
    {
        utyLoadSinglePlayerGame(name,atom);
    }
    else
    {
        utyLoadMultiPlayerGame(name,atom);
    }
}

/*-----------------------------------------------------------------------------
    Name        : utySelectMultiPlayerGame
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utySelectMultiPlayerGame(char *name, featom *atom)
{
    gpGamePick("M");
}

void utyLoadMultiPlayerGameGivenFilename(char *filename)
{
    dbgAssertOrIgnore(startingGame == FALSE);

#ifdef HW_BUILD_FOR_DEBUGGING
    dbgMessagef("New game started");
#endif

    startingGame = TRUE;

    if (mgRunning)
    {
        mgShutdownMultiPlayerGameScreens();
    }

    feAllScreensDelete();

    dbgAssertOrIgnore(!multiPlayerGame);        // don't allow loading of network games

    singlePlayerInit();

    horseRaceInit();
    gameStart(filename);
    horseRaceShutdown();

    if (hrAbortLoadingGame)
    {
        utyGameQuitToMain(NULL,NULL);
        hrAbortLoadingGame = FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : utyLoadMultiPlayerGame
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyLoadMultiPlayerGame(char *name, featom *atom)
{
    char filename[100];
    sdword verifysavename;

    if (!gpGetGameName(name,atom,filename))
    {
        return;
    }

    if ((verifysavename = VerifySaveFile(filename)) != VERIFYSAVEFILE_OK)
    {
        if (verifysavename == VERIFYSAVEFILE_BADVERSION)
            GeneralMessageBox(strGetString(strErrorInvalidSaveGameFileVersion),NULL);
        else
            GeneralMessageBox(strGetString(strErrorInvalidSaveGameFile),NULL);
        return;
    }

    utyLoadMultiPlayerGameGivenFilename(filename);
}

/*-----------------------------------------------------------------------------
    Name        : utyBackFromSinglePlayerOptions
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyBackFromSinglePlayerOptions(char *name, featom *atom)
{
    cpResetRegions();

    feScreenDisappear(NULL,NULL);

    feScreenStart(ghMainRegion, "Main_game_screen");
}

/*-----------------------------------------------------------------------------
    Name        : utyStartTutorial
    Description : called from the Tutorial Alert popup
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyStartTutorial(char* name, featom* atom)
{
    forceSP = TRUE;
    tutorial = 1;
    utySinglePlayerGameStart(name, atom);
}

/*-----------------------------------------------------------------------------
    Name        : utyStartSinglePlayer
    Description : called from the Tutorial Alert popup
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyStartSinglePlayer(char* name, featom* atom)
{
    forceSP = TRUE;
    needtutorial = FALSE;
    utySinglePlayerGameStart(name, atom);
}

/*-----------------------------------------------------------------------------
    Name        : utyStartSkirmish
    Description : called from the Training Alert popup
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyStartSkirmish(char* name, featom* atom)
{
    forceSP = TRUE;
    needtutorial = FALSE;
    utyNewGameStart(name, atom);
}

/*-----------------------------------------------------------------------------
    Name        : utySinglePlayerGameStart
    Description : Starts a new single player game
    Inputs      : name - name of entry to envoke this function
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utySinglePlayerGameStart(char *name, featom *atom)
{
    cpResetRegions();

    if (strcasecmp(name, "Tutorial1") == 0)
    {
        forceSP = TRUE;
    }

    if ((!forceSP) &&
        (gpCountTrainingSavegames() == 0) &&
        (needtutorial))
    {
        feScreenStart(ghMainRegion, "Tutorial_Alert");
        return;
    }

    dbgMessagef("New single player game started");
    feAllScreensDelete();

    forceSP = FALSE;

    singlePlayerGame = TRUE;

    if (!(tutorial==TUTORIAL_ONLY))
    {
        tutorial = TUTORIAL_SINGLEPLAYER;
    }

    numPlayers = 2;     // player 1 is always computer (pirates, opposite race, whatever)
    curPlayer = 0;      // player 0 is human
    strcpy(playerNames[0],"Player");

    // CAT: no mission-sequence reset (utySinglePlayerGameStart 0x005aaf80): singlePlayerInit 0x00518c50 starts at mission 1
    singlePlayerInit();

    horseRaceInit();
    gameStart(NULL);
    horseRaceShutdown();

    if (hrAbortLoadingGame)
    {
        utyGameQuitToMain(NULL,NULL);
        hrAbortLoadingGame = FALSE;
    }
    else
    {
        singlePlayerPostInit(FALSE);
    }
}

/*-----------------------------------------------------------------------------
    Name        : utyBackFromSinglePlayer
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyBackFromSinglePlayer(char *name, featom *atom)
{
    cpResetRegions();

    feScreenDisappear(NULL,NULL);

    feScreenStart(ghMainRegion, "Single_player_options");
}

/*-----------------------------------------------------------------------------
    Name        : utyNewGameStart
    Description : Starts new game
    Inputs      : name - name of entry to envoke this function
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyNewGameStart(char *name, featom *atom)
{
    udword i,j;

    if (FEFIRSTCALL(atom))
    {
        return;
    }
    if ((!forceSP) &&
        (!multiPlayerGame) &&
        (gpCountTrainingSavegames() == 0) &&
        (needtutorial))
    {
        feScreenStart(ghMainRegion, "Tutorial_Skirmish_Alert");
        return;
    }

    if (!multiPlayerGame)       // moved up so that check if minplayers too low can be made before feAllScreensDelete, etc
    {
        if ((!recordplayPacketsInGame) && (playPackets))
        {
            netcheckInit();
            recPackPlayInit();      // sets numPlayers, curPlayer
        }
        else if (!noDefaultComputerPlayer)
        {
            numPlayers = 1 + tpGameCreated.numComputers;
            for (i=1;i<tpGameCreated.numComputers+1;i++)
            {
                ComputerPlayerEnabled[i] = TRUE;
            }
            curPlayer = 0;
        }
        else
        {
            numPlayers = singleMachineNumPlayers;
            curPlayer = 0;
        }

        if ((!singlePlayerGame) && (spScenarios[spCurrentSelected].minplayers > (numPlayers)))
        {
            char str[200];

            sprintf(str,strGetString(strMinNumPlayersRequired),spScenarios[spCurrentSelected].minplayers);
            mgPrepareMessageBox(str,NULL);
            mgShowScreen(MGS_Message_Box,FALSE);
            return;
        }
    }

    forceSP = FALSE;

    dbgAssertOrIgnore(startingGame == FALSE);

#ifdef HW_BUILD_FOR_DEBUGGING
    dbgMessagef("New game started");
#endif
    startingGame = TRUE;

    if (mgRunning)
    {
        mgShutdownMultiPlayerGameScreens();
    }

    feAllScreensDelete();
    for (i=0;i<TOTAL_NUM_SHIPS;i++)
    {                                                       // CAT: in every game (utyNewGameStart 0x005ab180)
        shiplagtotals[i] = 0;
    }
    if (multiPlayerGame)
    {
        dbgMessagef("sigsPlayerIndex %u",sigsPlayerIndex);
        for (i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
        {
            playersReadyToGo[i] = FALSE;
        }
        numPlayers = (uword)sigsNumPlayers;
        curPlayer = (uword)sigsPlayerIndex;
        netcheckInit();

#ifdef GOD_LIKE_SYNC_CHECKING
        if(syncDumpOn)
        {
            godSyncInit();
        }
#endif

        if (!recordplayPacketsInGame)
        {
            if (playPackets)
            {
                recPackPlayInit();      // sets numPlayers, curPlayer
            }
            else if (recordPackets)
            {
                recPackRecordInit();
            }
        }

//        if (!noDefaultComputerPlayer)
//        {
            if (tpGameCreated.numComputers>0)
            {
                // ditribute the computer players in a round robin fashion
                j=0;
                for (i=numPlayers; i<numPlayers + tpGameCreated.numComputers; i++)
                {
                    if (j==sigsPlayerIndex)
                    {
                        ComputerPlayerEnabled[i] = TRUE;
                    }
                    ComputerPlayerOn[i] = j;
                    j++;
                    if (j > numPlayers-1)
                    {
                        j = 0;
                    }
                }

                numPlayers += tpGameCreated.numComputers;
            }
  //      }

        singlePlayerInit();

        startingGameState = CHECK_AUTODOWNLOAD_MAP;
        if (numPlayers == 1)
        {
            multiPlayerGameUnderWay = TRUE;
        }
    }
    else
    {
        for (i=0;i<numPlayers;i++)
        {
            if (ComputerPlayerEnabled[i])
            {
                sprintf(playerNames[i], "%s %i", strGetString(strComputerName), i);
            }
            else
            {
                strcpy(playerNames[i],utyName);
            }
        }

        singlePlayerInit();

        horseRaceInit();
        gameStart(NULL);
        horseRaceShutdown();

        if (hrAbortLoadingGame)
        {
            utyGameQuitToMain(NULL,NULL);
            hrAbortLoadingGame = FALSE;
        }
    }
#ifdef DEBUG_GAME_STATS
        if(statLogOn)
        {
            if(multiPlayerGame)
            {
                gameStatsInitLog();
            }
        }
#endif

    // CAT (M2-R6 nis-fow-camera): with the motion cues on (catMotionCues 0x008b8844) utyNewGameStart 0x005ab180 ends
    //      with cat_mcSetMode(&cues 0x00c49400, 1) and cat_mcResetRequest(&cues)
    if (catMotionCues)
    {
        cat_mcSetMode(&catMotionCueState, 1);
        cat_mcResetRequest(&catMotionCueState);
    }
}

/*-----------------------------------------------------------------------------
    Name        : utyGameQuit
    Description : Quit game
    Inputs      : name - name of entry to envoke this function
    Outputs     : Calls utyCloseOK
    Return      : void
----------------------------------------------------------------------------*/
void utyGameQuit(char *name, featom *atom)
{
#ifdef HW_BUILD_FOR_DEBUGGING
    dbgMessagef("Quit game, baby!");
#endif

    // CAT: utyGameQuit 0x005ab570 fades the music out and pauses the sound first, then posts the quit command
    //      (WM_COMMAND 0x65 -> utyCloseOK)
    soundEventStopMusic(0.5f);
    soundEventPause(TRUE);
#if defined(HW_GAME_DEMO) || defined(HW_GAME_RAIDER_RETREAT)
    if (enableAVI)
    {
        psModeBegin("Plugscreens\\", PMF_CanSkip);
        psScreenStart("BuyHomeworld0.plug");
    }
    else
    {
        utyCloseOK(NULL, 0, 0, 0);
    }
#else
        utyCloseOK(NULL, 0, 0, 0);
#endif
}

/*-----------------------------------------------------------------------------
    Name        : utyGameQuitToMain
    Description : Quit game
    Inputs      : name - name of entry to envoke this function
    Outputs     : Calls utyCloseOK
    Return      : void
----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    Name        : cat_utyGameQuitToMainOrStats                      0x005ab5b0
    Description : CAT (M2-R6 startup-keys): "QuitToMain" (and univUpdate's end of a game): outside a single-player
                  game the game-stats screen begins (the screens gone, the music stopped, the game no longer running;
                  cat_gameStatsBegin, GamePick.c), in one it is utyGameQuitToMain (the exe inlines it).
----------------------------------------------------------------------------*/
void cat_utyGameQuitToMainOrStats(char *name, featom *atom)
{
    if (!singlePlayerGame)
    {
        feAllScreensDelete();
        soundEventStopMusic(1.0f);
        soundEventPause(TRUE);
        gameIsRunning = FALSE;
        cat_gameStatsBegin();
        return;
    }
    utyGameQuitToMain(name, atom);
}

void utyGameQuitToMain(char *name, featom *atom)
{
    bool32 networkgame = multiPlayerGame;
    dbgMessagef("Quit to main menu!");

    feAllScreensDelete();

    soundEventStopMusic(1.0f);
    soundEventPause(TRUE);

    gameEnd();

#if defined (HW_GAME_DEMO) || defined(HW_GAME_RAIDER_RETREAT)
    if (utyPlugScreens && enableAVI)
    {
        psModeBegin("Plugscreens\\", PMF_CanSkip);
        psScreenStart("BuyHomeworld0.plug");
        soundEventPlayMusic(SOUND_FRONTEND_TRACK);
        utyPlugScreens = FALSE;
        return;
    }
    else
    {
        soundEventPlayMusic(SOUND_FRONTEND_TRACK);
    }
#else
    if (utyCreditsSequence)
    {
        soundEventPlayMusic(CAT_MUSIC_CREDITS);             // CAT: utyGameQuitToMain 0x005ab6b0 (HW1 RollCredits 28)
        psModeBegin("Credits\\", PMF_Credits | PMF_LanguageSpecific | PMF_MusicTrack);
        psScreenStart("Credits-GameEnd.cred");
        utyCreditsSequence = FALSE;
#if HWI_ORACLE
        catMissionSelfTestEvent("credits Credits-GameEnd.cred");    // HWI-TEST (M2-R9 missions-10-17): the game's end
#endif
    }
    else
    {
        soundEventPlayMusic(SOUND_FRONTEND_TRACK);
    }
#endif
    if (networkgame)
    {
        if (LANGame)
        {
            lgStartMultiPlayerLANGameScreens(ghMainRegion, 0, 0, 0, TRUE);
        }
        else
        {
            mgStartMultiPlayerGameScreens(ghMainRegion, 0, 0, 0, TRUE);
        }
    }
    else
    {
        feScreenStart(ghMainRegion, "Main_game_screen");
    }
    soundEventPause(FALSE);
    soundEventUpdate();
}

/*-----------------------------------------------------------------------------
    Name        : utyInGameCancel
    Description : call back function for when in-game menu is deleted
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyInGameCancel(char *name, featom *atom)
{
    if (!multiPlayerGame)
    {
        universePause = FALSE;      // unpause game
    }

    feScreenDisappear(name,atom);
}

/*-----------------------------------------------------------------------------
    Name        : utyInGameLoad
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyInGameLoad(char *name, featom *atom)
{
/* This is where we actually load the game */
}

/*-----------------------------------------------------------------------------
    Name        : utyInGameSave
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyInGameSave(char *name, featom *atom)
{
/* This is where we actually save the game */
}



/*-----------------------------------------------------------------------------
    Name        : utyInGameSave
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utySaveRecordedGame(char *name, featom *atom)
{
/* This is where we start the recording */
}



/*-----------------------------------------------------------------------------
    Name        : utyFrontEndDataLoad
    Description : Load data required for front end.  Other modules can load
                    their own front-end data.
    Inputs      : void
    Outputs     : Does some stuff.  Or something.
    Return      : void
----------------------------------------------------------------------------*/
void utyFrontEndDataLoad(void)
{
    sdword index;

    ghDefaultFont = frFontRegister("default.hff");          //load in the default font
    fontMakeCurrent(ghDefaultFont);                         //make default font current

    for (index = 0; utyFrontEndFiles[index] != NULL; index++)
    {
        utyFrontEndFiles[index] = (char *)feScreensLoad(utyFrontEndFiles[index]);
    }
    feCallbackAddMultiple(utyCallbacks);
    feDrawCallbackAddMultiple(utyDrawCallbacks);

    opOptionsInit();
}

/*-----------------------------------------------------------------------------
    Name        : utyFrontEndDataFree
    Description : Free data loaded for front-end screens.
    Inputs      : void
    Outputs     : Does some stuff.  Or something.
    Return      : void
----------------------------------------------------------------------------*/
void utyFrontEndDataFree(void)
{
    sdword index;

    for (index = 0; utyFrontEndFiles[index] != NULL; index++)
    {                                                       //for each screen
        feScreensDelete((fibfileheader *)utyFrontEndFiles[index]);//free it
    }
}

/*-----------------------------------------------------------------------------
    Name        : utyTeaserStart
    Description : Start the teaser NIS to play in the background of the menu
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
//file names for Non-Interactive NIS Sequences
/*
#define NIS_TeaserName          "nis\\Teaser.nis", "nis\\Teaser.script"
#define NIS_Radius      10000.0f
void utyTeaserStart(void)
{
    sdword index;
    ShipStaticInfo *info;
    vector centre = {0.0f, 0.0f, 0.0f};
    Player *player;
    ShipType shiptype;
    ShipRace shiprace;
    sdword i;
    matrix thisIdentityMatrix;

    universe.numPlayers = numPlayers = 2;
    for (i=0;i<universe.numPlayers;i++)
    {
        player = &universe.players[i];

        player->PlayerMothership = NULL;
        player->resourceUnits = 0;
        player->playerIndex = 0;
        player->race = i;
    }

    universeDefaultTeamColors();                            //set default team colors for all ships

    universe.curPlayerPtr = &universe.players[0];

    utyTeaserHeader = nisLoad(NIS_TeaserName);
    //utyTeaserHeader->loop = 0.0f;                           //make NIS loop

    for (index = 0; index < utyTeaserHeader->nObjectPaths; index++)
    {                                                       //load in the ship statics needed for NIS
        shiptype = utyTeaserHeader->objectPath[index].type;
        shiprace = utyTeaserHeader->objectPath[index].race;
        info = GetShipStaticInfo(shiptype,shiprace);
        bitSet(info->common.infoFlags, IF_InfoNeeded);
        // CAT: no drone statics for a DDDFrigate, Cataclysm has neither type (M2-R4 ships-b retire)
    }
    universeStaticInit();
    if (etgHasBeenStarted)
    {                                                       //if effects already started
        etgReloadReset();
    }
    else
    {
        etgStartup();
        utySet(SSA_Effect);
        etgHasBeenStarted = TRUE;
    }

    trRegistryRefresh();                                    //load textures for NIS
    thisIdentityMatrix = IdentityMatrix;
    utyTeaserPlaying = nisStart(utyTeaserHeader, &centre, &thisIdentityMatrix);

    nisFullyScissored = TRUE;
//    soundEventPlayCD(1);
    soundEventPlayMusic(11);
    nisTaskResume();
}
*/
/*-----------------------------------------------------------------------------
    Name        : utyTeaserEnd
    Description : Stop the teaser from playing
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
/*
void utyTeaserEnd(void)
{
    nisStop(utyTeaserPlaying);
    nisDelete(utyTeaserHeader);
    utyTeaserPlaying = NULL;
    nisTaskPause();
    nisCamera = NULL;
    nisIsRunning = nisCaptureCamera = FALSE;
    univupdateCloseAllObjectsAndMissionSpheres();
//    soundEventStopCD();
    soundEventStopMusic(1.0f);
    trTextureDeleteAllUnregistered();
    utyTeaserHeader = NULL;
    nisFullyScissored = FALSE;
}
*/

void scriptSetHomeworldCRC(char *directory,char *field,void *dataToFillIn)
{
    if (onlygetfirstcrc)
    {
        return;
    }

    {
        // HomeworldCRC is unsigned long; crc is unsigned int
        // if they are defined to be the same size then direct assignment is possible;
        // if they are not then "fun" can ensue which is why the long winded setup is here
        unsigned int crc[4] = { 0, 0, 0, 0 };
        sscanf(field,"%x %x %x %x", &crc[0], &crc[1], &crc[2], &crc[3]);
        HomeworldCRC[0] = crc[0];
        HomeworldCRC[1] = crc[1];
        HomeworldCRC[2] = crc[2];
        HomeworldCRC[3] = crc[3];
    }
    
    onlygetfirstcrc = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : utyGetFirstCDPath
    Description : Gets path to first CD-ROM drive containing the HW CD ("D:\",
                  say)
                  On Unix-based platforms, this will use the value in the
                  HW_CDROM environment variable.  If the value does not specify
                  a valid directory, or HW_CDROM is not defined, "/mnt/cdrom"
                  will be used.
    Inputs      : N/A
    Outputs     : szPath - Path to first CD-ROM drive continaning the HW CD (at
                  least 128 chars)
    Return      : N/A
----------------------------------------------------------------------------*/
void utyGetFirstCDPath(char *szPath)
{
#ifdef _WIN32
    char nLetter,szFile[255];

    HANDLE handle;

    /* Cycle thru drive letters */
    for(nLetter='A';nLetter<='Z';nLetter++)
    {
        /* Initialize path */
        strcpy(szPath," :");
        szPath[0]=nLetter;

        /* Check drive letter */
        if(GetDriveType(szPath) == DRIVE_CDROM)
        {
            /* Append \ */
            strcat(szPath,"\\");

            /* Set file to find */
            strcpy(szFile,szPath);
            strcat(szFile,utyMusicFilename);

            /* Find file */
            handle = CreateFile(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if(handle != INVALID_HANDLE_VALUE)
            {
                /* File found, exit */
                CloseHandle(handle);
                return;
            }

            /* File not found, continue */
        }
    }

    /* No CD-ROM available */
    strcpy(szPath,"");
    return;
#else
    char* env_path;
    struct stat dir_stat;

    /* Check for CD path environment variable. */
    env_path = getenv("HW_CDROM");
    if (env_path)
    {
        strncpy(szPath, env_path, 126);
        szPath[126] = '\0';
        unsigned int str_len = strlen(szPath);
        if (szPath[str_len - 1] != '/')
        {
            strcat(szPath, "/");
        }


        /* Does it exist? */
        if (!stat(szPath, &dir_stat) && S_ISDIR(dir_stat.st_mode))
        {
            return;
        }
    }

    /* Default to "/mnt/cdrom". */
    if (stat("/mnt/cdrom/", &dir_stat) || !S_ISDIR(dir_stat.st_mode))
    {
        /* Guess not... */
        szPath[0] = '\0';
        return;
    }

    /* Path seems okay. */
    strcpy(szPath, "/mnt/cdrom/");
    return;
#endif
}

/*-----------------------------------------------------------------------------
    Name        : PrepareVersionStringStuff
    Description : makes sure that networkVersion, languageVersion and the combined versionString are all set
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void PrepareVersionStringStuff()
{
    switch (strCurLanguage)
    {
        default:
        case languageEnglish:       strcpy(languageVersion,"English"); break;
        case languageFrench:        strcpy(languageVersion,"French"); break;
        case languageGerman:        strcpy(languageVersion,"German"); break;
        case languageSpanish:       strcpy(languageVersion,"Spanish"); break;
        case languageItalian:       strcpy(languageVersion,"Italian"); break;
        // can also put stuff like "EnglishOEM", etc.
    }

    strcpy(versionString,networkVersion);
    strcat(versionString,"\t");
    strcat(versionString,languageVersion);
    strcat(versionString,"\t");
    strcat(versionString,minorBuildVersion);                // CAT: the exe's minor build (utyGameSystemsPreInit 0x005ab810)
}

/*-----------------------------------------------------------------------------
    Name        : utyGrowthHeapAlloc
    Description : Memory manager callback to allocate a new growth heap.  The
                    pointer returned here will be passed to utyGrowthHeapFree
                    at shutdown time.
    Inputs      : size - size of heap to allocate
    Outputs     :
    Return      : pointer to newly allocated memory
----------------------------------------------------------------------------*/
void *utyGrowthHeapAlloc(sdword size)
{
    dbgAssertOrIgnore(size > 0);
#ifdef _WIN32
    return((void *)VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE));
#else
    void *heap = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    sdword i;

    if (heap == MAP_FAILED)
    {
        return NULL;                                        // memNewGrowthPoolAlloc reports a NULL heap
    }
    for (i = 0; i < MGH_NumberGrowthHeaps; i++)
    {                                                       // HWI-FIX: remember the length for utyGrowthHeapFree
        if (utyGrowthHeaps[i].heap == NULL)
        {
            utyGrowthHeaps[i].heap = heap;
            utyGrowthHeaps[i].length = (size_t)size;
            break;
        }
    }
    dbgAssertOrIgnore(i < MGH_NumberGrowthHeaps);
    return heap;
#endif
}

/*-----------------------------------------------------------------------------
    Name        : utyGrowthHeapFree
    Description : Memory manager shutdown callback for freeing a growth heap.
    Inputs      : heap - heap to free
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void utyGrowthHeapFree(void *heap)
{
#ifdef _WIN32
    BOOL result;
    result = VirtualFree(heap, 0, MEM_RELEASE);
    dbgAssertOrIgnore(result);
#else
    int result = -1;
    sdword i;

    for (i = 0; i < MGH_NumberGrowthHeaps; i++)
    {                                                       // HWI-FIX: unmap with the length it was mapped with
        if (utyGrowthHeaps[i].heap == heap)
        {
            result = munmap(heap, utyGrowthHeaps[i].length);
            utyGrowthHeaps[i].heap = NULL;
            utyGrowthHeaps[i].length = 0;
            break;
        }
    }
    dbgAssertOrIgnore(result != -1);
#endif
}

char *utyMissingCDMessages[] =
{
    "Invalid or missing Homeworld CD. Please insert valid CD.",
    "CD Homeworld non valide. Veuillez insérer un CD valide.",
    "Ungültige oder fehlende Homeworld-CD. Bitte gültige CD einlegen.",
    "El CD de Homeworld no es correcto o no se encuentra en la unidad. Introduce el CD correcto.",
    "CD di Homeworld mancante o non valido. Inserisci un CD valido.",
};
char *utyInvalidCDMessages[] =
{
    "Invalid Homeworld CD.",
    "CD Homeworld invalide.",
    "Ungültige Homeworld-CD.",
    "CD de Homeworld incorrecto.",
    "CD di Homeworld non valido.",
};
char *utyCannotOpenFileMessages[] =
{
    "Unable to open file: %s",
    "Impossible d’ouvrir le fichier: %s",
    "Datei kann nicht geöffnet werden: %s",
    "Imposible abrir archivo: %s",
    "Impossibile aprire il file: %s",
};

/*-----------------------------------------------------------------------------
    Name        : utyGameSystemsPreInit
    Description : Starts up all miscellaneous game systems.  call occurs before the
                  main window (hWnd) is created
    Inputs      : void
    Outputs     :
    Return      : NULL on success, string pointing to error message on error
----------------------------------------------------------------------------*/
char* utyGameSystemsPreInit(void)
{
#if CD_VALIDATION_ENABLED
#ifdef HW_BUILD_FOR_DISTRIBUTION
#ifdef _WIN32
    HANDLE syshandle;

    filehandle handle;

    char drive[256], volumename[256], filesysname[64], tmpfile[256], data[4];

    udword filesize, datasize, totalsize;
    udword serialnum = 0, componentlen, filesysflags;
    udword sectspclust, bytespsect, freeclusts, clusts, bytes;
#endif
#endif
#endif

    memset(utyStartedBits, 0, SSA_NumberBits);              //clear all module startup bits
    globalsInit();

    // CAT: utyGameSystemsPreInit 0x005ab810 then fills the fast square-root tables (fmathInit 0x004728c0); not
    //      needed here: fsqrt stays sqrtf (decision 23)

    keyInit();

    // Initialize the font registry
    frStartup();
    utySet(SSA_FontReg);

    // where the Homeworld data files are

    char *dataPath = getenv("HW_Data");
    // default to the current directory
    if (dataPath == NULL)
    {
        // Check to see if fileHomeworldDataPath is set
        if (fileHomeworldDataPath[0] == '\0')
        {
            getcwd(filePathTempBuffer, PATH_MAX);
            dataPath = filePathTempBuffer;

            fileHomeworldDataPathSet(dataPath);
        }
    }
    else
    {
        // HW_Data is set. overwrite current setting.
        // But make sure that there isn't a trailing slash
        
        if (dataPath[(strlen(dataPath) - 1 )] == '/') 
        {
            dataPath[(strlen(dataPath) - 1 )] = '\0';
        }

        fileHomeworldDataPathSet(dataPath);
    }


    // local filesystem override of .big archive contents
    if (fileOverrideBigPath[0] == '\0')
    {
        char *overrideBigPath = NULL;

#ifdef _MACOSX
        // let's not have files sprawling everywhere; besides it makes it easier to
        // create an unofficial patch .big should we ever decide to. (NB: this is a
        // directory, not a real .big file)
        strcpy(filePathTempBuffer, fileHomeworldDataPath);
        strcat(filePathTempBuffer, "/Override.big");
        
        overrideBigPath = filePathTempBuffer;
#else
        // in absence of environment vars (like in a retail install), assume
        // data file structure will start alongside the EXE
        overrideBigPath = fileHomeworldDataPath;
#endif

        fileOverrideBigPathSet(overrideBigPath);
    }

	// Attempt to set the user settings path.
	if (fileUserSettingsPath[0] == '\0')
	{
#ifdef _WIN32
		// HWI-FIX(win64): %APPDATA%\Homeworld-Infection holds the options file, keys.cfg, the saved games and the
		//      screenshots: a GOG install under Program Files is not writable by a 64-bit program (no UAC file
		//      virtualisation), and GoK's choice, the game folder, would also mix them with the GOG game's own
		char *appData = getenv("APPDATA");

		if (appData != NULL && appData[0] != '\0')
		{
			snprintf(filePathTempBuffer, PATH_MAX, "%s\\" HWI_WIN_CONFIGDIR, appData);
			fileMakeDirectory(filePathTempBuffer);
			fileUserSettingsPathSet(filePathTempBuffer);
		}
		// Use the same directory as the Homeworld data (similar to the
		// functionality in the original Homeworld).
		else if (fileHomeworldDataPath[0] != '\0')
		{
			fileUserSettingsPathSet(fileHomeworldDataPath);
		}
		else
		{	//HACKHACK: This is a temporary hack. Somebody needs to fix this!
			fileUserSettingsPathSet("C:\\HomeworldData");
		}
#else
		// Use the user's own Homeworld configuration dir if possible or
		// else use the Homeworld data directory itself.
        char *homeDir = getenv("HOME");
        
		if (homeDir != NULL)
		{
			snprintf(filePathTempBuffer, PATH_MAX, "%s/" CONFIGDIR, homeDir);
			fileUserSettingsPathSet(filePathTempBuffer);
		}
		else
		{
			fileUserSettingsPathSet(fileHomeworldDataPath);
		}
#endif
	}

    // CD-ROM path
    if (fileCDROMPath[0] == '\0')
    {
        char drivePath[PATH_MAX];

#if !defined _MSC_VER && !defined __MINGW32__
        // Find the first CD-ROM drive containing the HW CD
        utyGetFirstCDPath(drivePath);

        // If found, set the CD-ROM path
        if (strlen(drivePath) > 0)
        {
            fileCDROMPathSet(drivePath);
        }
#endif
    }

#if MAIN_CDCheck
    if (mainCDCheckEnabled)
    {
        if (!fileExists(utyMusicFilename, FF_CDROM))
        {
            return("Homeworld CD not detected.");
        }
    }
#endif

#if CD_VALIDATION_ENABLED
    #ifdef HW_BUILD_FOR_DISTRIBUTION
        #ifdef _WIN32

    // check pad file
    if (!fileExists(utyPadBigFilename, FF_CDROM))
    // if (!fileExists(utyPadBigFilename, FF_IgnoreBIG))
    {
        return(utyMissingCDMessages[strCurLanguage]);
    }

    filesize = (udword)fileSizeGet(utyPadBigFilename, FF_CDROM);
    // filesize = (udword)fileSizeGet(utyPadBigFilename, FF_IgnoreBIG);

    handle = fileOpen(utyPadBigFilename, FF_CDROM);
    // handle = fileOpen(utyPadBigFilename, FF_IgnoreBIG);
    fileSeek(handle,filesize-sizeof(udword),FS_Start);
    fileBlockRead(handle, &datasize, sizeof(udword));
    fileClose(handle);

    // check pad file size
    if (filesize != datasize)
    {
        return(utyInvalidCDMessages[strCurLanguage]);
    }

    // get more info about CD
    strcpy(drive,filePathPrepend("",FF_CDROM));
    GetVolumeInformation(drive, volumename, 255, &serialnum, &componentlen, &filesysflags, filesysname, 63);

    // check for CDFS file system on CD
    if (_stricmp(filesysname, "CDFS") != 0)
    {
        return(utyInvalidCDMessages[strCurLanguage]);
    }

    // check for 0 bytes left on CD
    freeclusts = 4; // confuse them
    GetDiskFreeSpace(drive, &sectspclust, &bytespsect, &freeclusts, &clusts);
    if (freeclusts != 0)
    {
        return(utyInvalidCDMessages[strCurLanguage]);
    }

    // not only must the CD be > 670MB but it must also be < 685MB - the CD must not exceed 685MB as
    // manufacturing cuases problems with anything bigger. Anyway, it's good to check for an upper size
    // limit, in case someone has managed to fake all other aspects of a CD on a hard drive.
    totalsize  = (bytespsect * sectspclust) * clusts;
    if ((totalsize < 702545920) || (totalsize > 718274560))
    {
        totalsize = 738197504; // confuse them
        return(utyInvalidCDMessages[strCurLanguage]);
    }

    // attempt to create a temporary file on the CD
    sprintf(tmpfile,"%s~hw.tmp",drive);
    strcpy(data,"~hw");
    syshandle = CreateFile(tmpfile, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (syshandle != INVALID_HANDLE_VALUE)
    {
        if (!WriteFile(syshandle, data, 4, &bytes, NULL))
        {
            if (bytes != 0)
            {
                CloseHandle(syshandle);
                DeleteFile(tmpfile);
                return(utyInvalidCDMessages[strCurLanguage]);
            }
        }

        CloseHandle(syshandle);
        DeleteFile(tmpfile);
        return(utyInvalidCDMessages[strCurLanguage]);
    }

        #endif
    #endif
#endif

    // check data dir on HD...
    if (!fileExists(utyMusicFilename, 0))
    {
        // check first CDROM drive...
        if (!fileExists(utyMusicFilename, FF_CDROM))
        {
            sprintf(errorString, utyCannotOpenFileMessages[strCurLanguage], utyMusicFilename);
            return(errorString);
        }
    }

    // check data dir on HD...
    if (!fileExists(utyVoiceFilename, 0))
    {
        // check first CDROM drive...
        if (!fileExists(utyVoiceFilename, FF_CDROM))
        {
            sprintf(errorString, utyCannotOpenFileMessages[strCurLanguage], utyVoiceFilename);
            return(errorString);
        }
    }

#ifndef HW_GAME_DEMO
    // CAT: the Beast voice file is checked the same way (utyGameSystemsPreInit 0x005ab810)
    if (!fileExists(cat_utyBeastVoiceFilename, 0))
    {
        if (!fileExists(cat_utyBeastVoiceFilename, FF_CDROM))
        {
            sprintf(errorString, utyCannotOpenFileMessages[strCurLanguage], cat_utyBeastVoiceFilename);
            return(errorString);
        }
    }
#endif

    //start up the memory manager
    if (MemoryHeapSize == MEM_HeapSizeDefault)
    {                                                       //if the user has not specified a heap size
        sdword newSize = 0;
#ifdef _WIN32
        MEMORYSTATUS memStat;
        GlobalMemoryStatus(&memStat);
        newSize = (sdword)((real32)memStat.dwTotalPhys * MEM_HeapDefaultScalar);
#elif defined(_MACOSX)
		int phys_pages = 32768;
		int page_size = getpagesize();
		newSize = (sdword)((real32)phys_pages * (real32)page_size
            * MEM_HeapDefaultScalar);
#else
        long phys_pages = sysconf(_SC_PHYS_PAGES);
        long page_size = sysconf (_SC_PAGE_SIZE);
        newSize = (sdword)((real32)phys_pages * (real32)page_size
            * MEM_HeapDefaultScalar);
#endif
        if (newSize > MEM_HeapSizeDefault)
        {
            MemoryHeapSize = min(newSize, MEM_HeapDefaultMax);
        }
    }
    
#ifdef _WIN32
    utyMemoryHeap = (void *)VirtualAlloc(NULL, MemoryHeapSize + sizeof(memcookie) * 4, MEM_COMMIT, PAGE_READWRITE);
#else
    utyMemoryHeapLength = (size_t)MemoryHeapSize + sizeof(memcookie) * 4;
    utyMemoryHeap = mmap(0, utyMemoryHeapLength,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (utyMemoryHeap == MAP_FAILED)
    {
        utyMemoryHeap = NULL;                               // HWI-FIX: mmap reports failure as MAP_FAILED, not NULL
    }
#endif

    if (utyMemoryHeap == NULL)
    {
        sprintf(errorString, "Error allocating heap of size %u", MemoryHeapSize);
        return(errorString);
    }
    utySet(SSA_MemoryHeap);
    if (memStartup(utyMemoryHeap, MemoryHeapSize, utyGrowthHeapAlloc) != OKAY)
    {
        sprintf(errorString, "Error starting memory manager with heap size %u at 0x%x", MemoryHeapSize, (unsigned int)utyMemoryHeap);
        return(errorString);
    }
    utySet(SSA_MemoryModule);

    bigOpenAllBigFiles();

#if 0       // ShortCircuitWON done in titaninterface.cpp now
    if (ShortCircuitWON)
    {
        onlygetfirstcrc = FALSE;
        scriptSet(NULL,"WONstuff.txt",WonStuffSet);     // later, take this out before game is released, fix later
    }
    else
    {
#endif

        HomeworldCRC[0] = 0;  // originally stored CRC for code block (WON hacked client check)
        HomeworldCRC[1] = 0;
        HomeworldCRC[2] = 0;
        HomeworldCRC[3] = 0;

        // CAT: no bigCRC of the big files' tables of contents (utyGameSystemsPreInit 0x005ab810 has none: the CRCs
        //      stay 0)
//  }

    PrepareVersionStringStuff();

    if (SecretWON)
    {
        if (IgnoreBigfiles)
        {
            return("Big files must be on for this feature");
        }
        else
        {
            if (titanSaveWonstuff())
                return("WON stuff saved");
            else
                return("WON stuff save error");
        }
    }

    scriptSetTweakableGlobals();   // Must be called after file stuff is initialized

    return NULL;
}

/*-----------------------------------------------------------------------------
    Name        : utyGameSystemsInit
    Description : Starts up all miscellaneous game systems.  call occurs after the
                  main window (hWnd) is created
    Inputs      : void
    Outputs     :
    Return      : NULL on success, string pointing to error message on error
----------------------------------------------------------------------------*/
char *utyGameSystemsInit(void)
{
    rndinitdata renderData;
    Uint32 sdlSubsystemFlags;

    utyToggleKeyStatesSave();
    utySet2(SS2_ToggleKeys);

    strLoadLanguage((strLanguageType) strCurLanguage); // load in the language strings for messages to player
                                                       // must be called after file stuff and memory stuff

    // set the default player name based on the current language
    if (utyName[0] == 0)
    {
        strcpy(utyName, strGetString(strDefaultPlayerName));
    }

    utySet2(SS2_Strings);

    dbgMessagef(
        "Homeworld CRCs:\n"
        "%22s = 0x%x\n"    
        "%22s = 0x%x\n"    
        "%22s = 0x%x"    
        ,
        "Update.big TOC",       HomeworldCRC[1],
        "Cataclysm.big TOC",    HomeworldCRC[2],  // CAT: log label only; the main archive is Cataclysm.big
        "(not used)",           HomeworldCRC[3]  // never been used
    );
    
    // startup any SDL systems we want that haven't already been kicked off
    sdlSubsystemFlags = SDL_WasInit(SDL_INIT_EVERYTHING);

    if (!(sdlSubsystemFlags & SDL_INIT_TIMER))
    {
        if (SDL_InitSubSystem(SDL_INIT_TIMER) == -1)
        {
            return "Unable to initialize SDL Timer.";
        }
    }
    
#if 0
    // Joystick used for controlling the 3D camera view. It can be any old
    // joystick but this is primarily intended to support devices used for
    // 3D CAD applications which have more degrees of freedom (6) than typical
    // joysticks (2). For example: 3Dconnexion's SpaceNavigator.
    // http://www.3dconnexion.com/products/3a1d.php
    if (!(sdlSubsystemFlags & SDL_INIT_JOYSTICK))
    {
        int joystick_i = 0;
        
        if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) == -1)
        {
            return "Unable to initialize SDL Joystick.";
        }
        
        for (joystick_i = 0; joystick_i < SDL_NumJoysticks(); ++joystick_i)
        {
            if (strcmp(SDL_JoystickName(joystick_i), "SpaceNavigator") == 0)
            {
                SDL_Joystick *joystick;
                
                SDL_JoystickEventState(SDL_ENABLE);
                joystick = SDL_JoystickOpen(joystick_i);
                
                dbgMessagef("SpaceNavigator found at index %d", joystick_i);
            }
        }
    }
#endif
    
    // CAT-TODO(M3 soak): the exe's task clock is QueryPerformanceCounter / (QPF / 240) (utyGameSystemsInit 0x005abe70 sets utyTimerDivisor, utyTasksDispatch 0x005acd10 divides 64-bit counts): 240 task ticks a second and a 62.5 ms universe tick; GoK's SDL_GetTicks / 4 ms gives 250 a second, a 60 ms universe tick (16.67 Hz on the wall clock, 4% fast) and whole-millisecond frames (measured by the motion-jitter scene, docs/m3-r1-motion-jitter.md: the render's alpha takes the sub-millisecond part from taskClockSample); porting it changes taskTimeDelta / taskTimeElapsed in every run, the virtual-clock streams too, so it is the soak round's measured change
    utyTimerDivisor = 1000 / UTY_TimerResolutionMax;
    utySet(SSA_Timer);
                                                            //start the task manager
    taskStartup((udword)(1000 / utyTimerDivisor));
    utySet(SSA_Task);

#if MEM_STATISTICS
    if (memStatsTaskHandle == 0xffffffff)
    {
        memStatsTaskHandle = taskStart(memStatsTaskFunction, MEM_TaskStatsPeriod, 0);//start frame rate task
    }
#endif

    // create the autorun lock file
    utyLockFilehandle = 0;
    utyLockFilehandle = fileOpen(utyLockFilename, FF_WriteMode|FF_IgnoreBIG|FF_UserSettingsPath);

    if (regStartup() != OKAY)                               //startup the region processor
    {
        return("Unable to start the region processor.");
    }
    utySet(SSA_Region);

    // initialize sound engine
    soundEventInit(whichRaceSelected);  // CAT: utyGameSystemsInit 0x005abe70 passes the selected race
    utySet2(SS2_SoundEngine);

#ifndef HW_GAME_DEMO
    /* Intro playing requires a window, which we have not made yet thanks to
       how the code's been butchered. */
#if 0
    if (enableAVI)
    {
        SDL_Event e;
        bool32 gotMsg;
        sdword intro;

        utilPlayingIntro = TRUE;

        for (intro = 0;;)
        {
            SDL_Delay(0);
            if (systemActive)
            {
                gotMsg = SDL_PollEvent(&e);
            }
            else
            {
                gotMsg = SDL_WaitEvent(&e);
            }
            if (gotMsg)
            {
                HandleEvent(&e);
            }
            else
            {
                switch (intro)
                {
                case 0:
                    /*binkInit(-1);*/
                    intro++;
                    break;
                case 1:
                    /*binkPlay("Movies\\sierra.bik", NULL, NULL, S_RGB555, TRUE, ANIM00_Sierra);*/
                    intro++;
                    break;
                case 2:
                    /*binkPlay("Movies\\relicintro.bik", NULL, NULL, S_RGB555, TRUE, ANIM00_Relic);*/
                    intro++;
                    break;
                case 3:
                    /*binkCleanup();*/
                    mainCleanupAfterVideo();
                    intro++;
                    break;
                case 4:
                    goto DONE_INTROS;
                }
            }
        }
    }
#endif
#else
    //show the opening plugscreens later
#endif

#ifndef _MACOSX_FIX_ANIM
#if 0
DONE_INTROS:
#endif
#endif

    utilPlayingIntro = FALSE;

    renderData.width = MAIN_WindowWidth;                    //setup data for
    renderData.height = MAIN_WindowHeight;                  //initializing the
    /*renderData.hWnd = ghMainWindow;*/                         //rendering system
    renderData.hWnd = 0;                                    //rendering system
    if (rndInit(&renderData) != OKAY)                       //startup the rendering system
    {
        //fallback to 640x480@16 rGL+sw, and fatally exit if that doesn't work either
        mainRestoreSoftware();
    }

    if (mouseStartup() != OKAY)
    {
        return("Unable to start mouse.");
    }
    utySet(SSA_Mouse);

    //create a region for the main window
    mrStartup();
    utySet(SSA_MainRegion);

    cat_shipPrintStartup();                                 // CAT (M2-R5 hud): the ship info panel's font, as utyGameSystemsInit

    // Startup the information overlay, must occur after mrStartup()
    ioStartup();
    utySet2(SS2_InfoOverlay);
    cat_popupMenuStartup();                                 // CAT: the popup menu, after ioStartup (utyGameSystemsInit 0x005abe70)

    utySet(SSA_Render);
    lightStartup();
    utySet(SSA_Lights);
    utyRenderTask = taskStart(rndRenderTask, 1.0f, TF_OncePerFrame);
    trStartup();
    utySet(SSA_TextureRegistry);

    taskCallBackInit();
    utySet2(SS2_BabyCallBackRegistry);


    aviIntroPlay();  // PlaceHolder for playing the intro Movies (Relic && Sierra)

    universeInit();
    utySet(SSA_Universe);

    universeTaskHandle = taskStart(universeUpdateTask, UNIVERSE_UPDATE_PERIOD, 0);  // HWI-FEATURE(motion-interp): the handle kept (MotionInterp.c's alpha)

    autodownloadmapStartup();
    KeepAliveStartup();
    InitQueue(&ProcessSyncPktQ,50000);
    InitQueue(&ProcessCmdPktQ,50000);
    InitQueue(&ProcessRequestedSyncPktQ,20000);
    InitQueue(&ProcessCaptaincyPktQ,10000);
    InitLastSyncPktsQ();
    utySet2(SS2_NetworkQueues);
    taskStart(captainServerTask, CAPTAINSERVER_PERIOD, 0);

    gunStartup();
    utySet2(SS2_Guns);

    selStartup();
    utySet(SSA_Selection);

    uicStartup();                                           //start up UI controls
    utySet(SSA_UIControls);

    meshStartup();

    if (feStartup() != OKAY)
    {                                                       //start the front end
        return("Unable to start front end.");
    }
    utySet(SSA_FEFlow);
#if MAIN_MOUSE_FREE
    utyClipMouse(startupClipMouse);                         //clip mouse to window if needed
#endif
    utySet(SSA_MouseClipped);

    utyFrontEndDataLoad();
    utySet(SSA_FrontEndData);

    tacticsStartUp();

    lmStartup();
    utySet2(SS2_LaunchMgr);

    tmStartup();

    rmAPIStartup();
    utySet2(SS2_ResearchMgr);

#if defined(HW_GAME_DEMO) || defined(HW_GAME_RAIDER_RETREAT)
    psStartup();
    utySet2(SS2_PlugScreen);
#endif

    smStartup();                                            //start the sensors manager
    utySet(SSA_SensorsManager);

    svStartup();
    utySet2(SS2_ShipView);

    pingStartup();
    utySet2(SS2_Ping);

    battleChatterStartup();
    utySet2(SS2_BattleChatter);

    teStartup();                                            //start the team-specific stuff
    if (!cpColorsPicked)
    {
        utyBaseColor   = teColorSchemes[0].textureColor.base;
        utyStripeColor = teColorSchemes[0].textureColor.detail;
    }
    utySet(SSA_Teams);

    cpStartup(&utyBaseColor, &utyStripeColor);                //start the color picker
    utySet(SSA_ColorPicker);

    subStartup();
    utySet2(SS2_SubTitle);

    tutStartup();
    utySet2(SS2_Tutorial);

    animStartup();      //start the animatics module

    spStartup();                                            //start the scenario picker
    utySet(SSA_ScenarioPicker);

    gpStartup();
    utySet2(SS2_GamePicker);

    mgStartup();
    lgStartup();
    mgGameTypeScriptInit();
    utySet2(SS2_MultiplayerGame);

    cmStartup();                                            //start the construction manager
    utySet(SSA_ConstructionManager);

    ranStartup();                                           //start the random-number generator
    utySet(SSA_RandomNumbers);

    bsStartup();
    utySet2(SS2_BSpline);

    nisStartup();
    utySet2(SS2_NIS);

    udStartup();                                            //start the undo module
    utySet(SSA_Undo);

    toStartup();                                            //start the TO module
    utySet(SSA_TacticalOverlay);

    dmgStartup();

    cloudStartup();

    shStartup();

    nebStartup();

    btgStartup();

    alodStartup();

    partStartup();

    pieStartup();

    trailStartup();                                         //start the trails module
    utySet(SSA_ShipTrails);

    //clear out the task timer.  Make sure this is the last call in this function.
    utyTaskTimerClear();

#if FEF_TEXTURES_DISABLABLE
    if (fetEnableTextures)
#endif
    {
        ferStartup();                                       //start front end texture registry
    }

    prim3dStartup();                                        //start the prim 3d stuff
    utySet2(SS2_Prim3D);

    // CAT (M2-R5 hud): the overlay icons (OverlayIcons\OverlayIcons.script), after prim3dStartup as utyGameSystemsInit
    //      0x005abe70
    cat_overlayIconsStartup();
    // CAT (M2-R6 startup-keys): the key bindings (keys.cfg, made from default_keys.cfg: cat_kbLoadKeyBindings
    //      0x0054da10), between the icons and the names as utyGameSystemsInit 0x005abe70
    cat_kbLoadKeyBindings();
    // CAT (M2-R5 hud): then the carrier and mothership names (shipnames.script, cat_shipNamesInit 0x0050efd0) and the
    //      game stats screen's fonts and timer (cat_gameStatsStartup 0x00489e50)
    cat_shipNamesInit();
    cat_gameStatsStartup();
    // CAT (M2-R6 frontend): HW1's key-binding start-up runs here (utyGameSystemsInit 0x005abe70: after
    //      cat_gameStatsStartup), not in opOptionsInit
    kbInitKeyBindings();
    // CAT (M2-R6 frontend): the exe's memory test (GlobalMemoryStatus; SDL_GetSystemRAM here): under 42,000,001 bytes of
    //      RAM the texture detail goes to 0, under 74,000,001 to 25 (unless the options file set it:
    //      SetTextureRequirements), and under 128,000,000 the dynamic CPV goes off and the low-memory mesh switch on
    {
        Uint64 ram = (Uint64)SDL_GetSystemRAM() * 1024 * 1024;

        if (ram < 42000001)
        {
            if (!catOpTextureRequirementsSet)
            {
                catOpTextureDetail = 0;
                cat_trTextureMemorySet(0);
            }
        }
        else if (ram < 74000001)
        {
            if (!catOpTextureRequirementsSet)
            {
                catOpTextureDetail = 25;
                cat_trTextureMemorySet(25);
            }
        }
        if (ram < 128000000)
        {
            enableCPV = FALSE;
            catMeshStateDisabled = TRUE;
        }
    }

    utySystemStarted = TRUE;

    opUpdateSettings();

    soundEventPlayMusic(SOUND_FRONTEND_TRACK);

//    if (!nisEnabled)
//    {
        feScreenStart(ghMainRegion, "Main_game_screen");
        mouseCursorShow();
//    }

#ifdef HW_GAME_DEMO
#ifndef __EMSCRIPTEN__
    if (enableAVI)
    {
        primModeSetFunction2();
        psModeBegin("Plugscreens\\", PMF_CanSkip);
        psScreenStart("SierraIntro.plug");
    }
#endif
#endif
    if (demDemoRecording)
    {                                                       //if recording a demo
        sprintf(demDemoFilename + strlen(demDemoFilename), "%ux%u.dem", MAIN_WindowWidth, MAIN_WindowHeight);
#if DEM_VERBOSE_LEVEL >= 1
        dbgMessagef("Recording demo '%s'.", demDemoFilename);
#endif
        determCompPlayer = TRUE;                            //computer player must be deterministic to record demos
        demRecordStart(demDemoFilename, utyPreDemoStateSaveCB);
    }
    else if (demDemoPlaying)
    {                                                       //if playing a demo
        sprintf(demDemoFilename + strlen(demDemoFilename), "%ux%u.dem", MAIN_WindowWidth, MAIN_WindowHeight);
        if (fileExists(demDemoFilename, 0))
        {
            demPlayStart(demDemoFilename, utyPreDemoStateLoadCB, utyDemoFinishedCB);
        }
        else
        {
#if DEM_VERBOSE_LEVEL >= 1
            dbgMessagef("Demo '%s' not found.", demDemoFilename);
#endif
            demDemoPlaying = FALSE;
        }
    }

/*
    if (nisEnabled)
    {
        mouseCursorHide();
        utyTeaserStart();
    }
*/

    utySet(SS2_SystemStarted);                              //!!! leave this at the end of this function

    // CAT: /catMeshLoadTest (port diagnostic): load every mesh in the list once, log it, and quit
    if (catMeshSelfTestList[0] != '\0')
    {
        catMeshRunSelfTest();
        utyGameQuit(NULL, NULL);
        return(NULL);
    }

    // CAT: /catEtgParseTest (port diagnostic): start the effects module (the exe's 8 scripts), load every other etg
    //      script and effect of the list, optionally run a sample of effects (/catEtgEffectTest), and quit
    if (catEtgSelfTestList[0] != '\0')
    {
        horseRaceInit();                                    //the effects load under the loading screen, as in gameStart
        catEtgRunParseSelfTest();                           //calls etgStartup
        horseRaceShutdown();
        utySet(SSA_Effect);                                 //so the quit runs etgShutdown, as after a game
        etgHasBeenStarted = TRUE;
        if (catEtgEffectSelfTestTicks > 0)
        {
            catEtgRunEffectSelfTest(catEtgEffectSelfTestTicks);
        }
        utyGameQuit(NULL, NULL);
        return(NULL);
    }

    // CAT: /catFeScreensTest and /catStringsTest (port diagnostics, M2-R6 frontend): push, draw and pop every screen of
    //      the install's .fib files; resolve the string ids against the language script; then quit
    if ((catFeScreensSelfTestList[0] != '\0') || catStringsSelfTest)
    {
        if (catStringsSelfTest)
        {
            catStringsRunSelfTest();
        }
        if (catFeScreensSelfTestList[0] != '\0')
        {
            catOptionsRunSelfTest();                        //first: the screens' callbacks may set options
            catFeScreensRunSelfTest();
            feScreenStart(ghMainRegion, "Main_game_screen");
        }
        utyGameQuit(NULL, NULL);
        return(NULL);
    }

    // CAT: /catNisParseTest (port diagnostic, M2-R6 nis-fow-camera): every NIS with its script and every animatic
    //      script of the install loaded and checked; then quit
    if (catNisParseTestList[0] != '\0')
    {
        catNisRunParseSelfTest();
        utyGameQuit(NULL, NULL);
        return(NULL);
    }

    // HWI-TEST (M3-R1 ui-scale): /catUiScaleTest: the UI scale, the window fit's rule, the mouse at the edges, a scripted
    //      click on a front-end button, the Video screens' control, the options-file and console rows (UiScaleTest.c)
    if (catUiScaleSelfTest)                                 // HWI-TEST
    {
        catUiScaleRunSelfTest();
        utyGameQuit(NULL, NULL);
        return(NULL);
    }

    // HWI-TEST (M3-R4 window-modes): /catResizeTest: the resolution presets and the resizable window (UiScaleTest.c)
    if (catResizeSelfTest)                                  // HWI-TEST
    {
        catResizeRunSelfTest();
        utyGameQuit(NULL, NULL);
        return(NULL);
    }

    // HWI-TEST (M3-R2 instrumentation): /catModeQuitTest <W>x<H>: a resolution change, then one quit (UiScaleTest.c)
    if (catModeQuitSelfTest)                                // HWI-TEST
    {
        catModeQuitRunSelfTest();
        if (!catModeQuitAwaitFrames())                      // HWI-TEST (M3-R4 mode-switch-fix): or frames first
        {
            utyGameQuit(NULL, NULL);
            return(NULL);
        }
    }

    // HWI-FEATURE(motion-interp): /catToggleTest: the Smooth motion checkbox, its options-file rows and console rows
    //      (MotionTest.c); then quit
    if (catToggleSelfTest)                                  // HWI-FEATURE(motion-interp)
    {                                                       // HWI-FEATURE(motion-interp)
        catToggleRunSelfTest();                             // HWI-FEATURE(motion-interp)
        utyGameQuit(NULL, NULL);                            // HWI-FEATURE(motion-interp)
        return(NULL);                                       // HWI-FEATURE(motion-interp)
    }                                                       // HWI-FEATURE(motion-interp)

    // CAT: /catKeysTest (port diagnostic, M2-R6 startup-keys): the install's default_keys.cfg loaded, saved, reloaded
    //      and reset through the Cataclysm key-binding code; then quit
    if (catKeysSelfTest)
    {
        catKeysRunSelfTest();
        utyGameQuit(NULL, NULL);
        return(NULL);
    }

    // CAT: /catSoundTest (port diagnostic, M2-R5 sound): decode a stream of each audio bank into the (dummy) audio
    //      device, look up the battle-chatter events, and quit
    if (catSoundSelfTest)
    {
        catSoundRunSelfTest();
        utyGameQuit(NULL, NULL);
        return(NULL);
    }

    // CAT: /catTexturesTest (port diagnostic): load textures.ll, register and load every listed texture, and quit
    if (catTexturesSelfTest)
    {
        catTexturesRunSelfTest();
        utyGameQuit(NULL, NULL);
        return(NULL);
    }

    // CAT: /catShipStaticsTest (port diagnostic): load every ship static once, log it, and quit
    if (catShipStaticsSelfTest)
    {
        catShipStaticsRunSelfTest();
        utyGameQuit(NULL, NULL);
        return(NULL);
    }

#if HWI_ORACLE
    // HWI-ORACLE: /oracleSelfTest <ticks>: the oracle's scripted lockstep start (OracleDump.c), then quit
    if (oracleSelfTestRequested())
    {
        oracleRunSelfTest();
        utyGameQuit(NULL, NULL);
        return(NULL);
    }
#endif

    // when we want to take debug screenshots directly start the singleplayer campaing
    if (debugScreenshots)
    {
        utySinglePlayerGameStart("", NULL);
    }
#if HWI_ORACLE
    // HWI-TEST (skirmish-start): /catSkirmishTest <ticks> starts a skirmish through the front end's own callbacks
    //      (LevelLoad.c catSkirmishSelfTestStart); it runs on the mission run's clock, so it comes first
    else if (catSkirmishSelfTestRequested())
    {
        catSkirmishSelfTestStart();
    }
    // HWI-TEST (M2-R8 kas-runtime-mission01): /catMissionTestTutorial starts the tutorial as the front end's Tutorial
    //      Alert does (utyStartTutorial: tutorial 1, the single-player start)
    else if (catMissionSelfTestTutorialRequested())
    {
        utyStartTutorial("", NULL);
    }
    // HWI-TEST (M2-R8 singleplayer-tutorial): /catLoadGameTest loads a single-player save as the front end's Load Game
    else if (catLoadGameSelfTestRequested())
    {
        catLoadGameSelfTestStart();
    }
    // HWI-ORACLE: /oracleMission <ticks> starts single player at once too (no screenshots, no next-level loop)
    else if (oracleMissionRequested())
    {
        utySinglePlayerGameStart("", NULL);
    }
    // HWI-ORACLE (M2-R6 oracle-mode-a): /oracleReplay <name> loads the recorded game as the front end does
    else if (oracleReplayRequested())
    {
        oracleStartReplay();
    }
#endif

    return(NULL);                                           //success, return no error
}

/*-----------------------------------------------------------------------------
    Name        : utyGameSystemsPreShutdown
    Description : Closes miscellaneous game systems.  ONLY CALL THIS if utyGameSystemsInit
                  failed but utyGameSystemsPreInit was successful
    Inputs      : void
    Outputs     :
    Return      : NULL on success, string pointing to error message on error
----------------------------------------------------------------------------*/
char* utyGameSystemsPreShutdown(void)
{
    if (utyTest(SSA_FontReg))
    {
        frShutdown();
        utyClear(SSA_FontReg);
    }

    bigCloseAllBigFiles();

    keyClose();

#if MEM_ANALYSIS
    memAnalysisCreate();
#endif
    if (utyTest(SSA_MemoryModule))
    {
        memClose(utyGrowthHeapFree);
        utyClear(SSA_MemoryModule);
    }
    if (utyTest(SSA_MemoryHeap))
    {
#ifdef _WIN32
        bool32 result = VirtualFree(utyMemoryHeap, 0, MEM_RELEASE);
        dbgAssertOrIgnore(result);
#else
        dbgAssertAlwaysDo(munmap(utyMemoryHeap, utyMemoryHeapLength) != -1);   // HWI-FIX: mapped length, not 0
#endif
        utyClear(SSA_MemoryHeap);
    }

    if (DebugWindow)
    {
        if (utyTest(SSA_DebugWindow))
        {
            /* Debug window disabled...search for other dbw*() functions if
               reenabling. */
            /*dbwClose();*/
            utyClear(SSA_DebugWindow);
        }
    }
    return NULL;
}

/*-----------------------------------------------------------------------------
    Name        : utyGameSystemsShutdown
    Description : Closes all miscellaneous game systems
    Inputs      : void
    Outputs     :
    Return      : NULL on success, string pointing to error message on error
----------------------------------------------------------------------------*/
char *utyGameSystemsShutdown(void)
{
    // close the autorun lock file
    if(utyLockFilehandle != 0) fileClose(utyLockFilehandle);

    // save the game options
    utyOptionsFileWrite();

    // write the debug window's options if so required
    if (DebugWindow)
    {
        if (utyTest(SSA_DebugWindow))
        {
            /* Debug window disabled...search for other dbw*() functions if
               reenabling. */
            /*dbwWriteOptions();*/
        }
    }

    if (demDemoRecording)
    {                                                   //if recording a demo
        demRecordEnd();
    }
    else if (demDemoPlaying)
    {
        demPlayEnd();
    }

    if (utyTest2(SS2_Strings))
    {
        strFreeLanguage();
        utyClear2(SS2_Strings);
    }

    if (utyTest(SSA_Universe))
    {
        universeClose();
        utyClear(SSA_Universe);
    }

    if (utyTest2(SS2_AIPlayer))
    {
        aiplayerShutdown();
        utyClear2(SS2_AIPlayer);
    }

    if (utyTest2(SS2_SoundEngine))
    {
        // shutdown sound engine
#ifndef _LINUX_FIX_ME
        soundEventClose();
#else
        soundstopthreads();     // HWI-FIX: no sound thread may read the heap after it is unmapped below (soundlow.c)
#endif
        utyClear2(SS2_SoundEngine);
    }

    if(utyTest2(SS2_BabyCallBackRegistry))
    {
        taskCallBackShutDown();
    }

    if (utyTest2(SS2_NetworkQueues))
    {
        autodownloadmapShutdown();
        KeepAliveShutdown();
        CloseQueue(&ProcessCmdPktQ);
        CloseQueue(&ProcessSyncPktQ);
        CloseQueue(&ProcessRequestedSyncPktQ);
        CloseQueue(&ProcessCaptaincyPktQ);
        CloseLastSyncPktsQ();
        utyClear2(SS2_NetworkQueues);
    }
    dmgShutdown();
    cloudShutdown();
    shShutdown();
    nebShutdown();
    btgShutdown();
    alodShutdown();
    partShutdown();
    if (utyTest(SSA_ShipTrails))
    {
        trailShutdown();                                    //shut down the TO module
        utyClear(SSA_ShipTrails);
    }
    if (utyTest(SSA_TacticalOverlay))
    {
        toShutdown();                                       //shut down the TO module
        utyClear(SSA_TacticalOverlay);
    }
    if (utyTest(SSA_Undo))
    {
        udShutdown();                                       //shut down the undo module
        utyClear(SSA_Undo);
    }
    if (utyTest(SSA_ColorPicker))
    {
        cpShutdown();                                       //shut down the color picker
        utyClear(SSA_ColorPicker);
    }
    if (utyTest(SSA_RandomNumbers))
    {
        ranShutdown();                                      //shut down the random-number genrator
        utyClear(SSA_RandomNumbers);
    }
    if (utyTest2(SS2_BSpline))
    {
        bsShutdown();                                       //shut down the random-number genrator
        utyClear2(SS2_BSpline);
    }
    if (utyTest2(SS2_NIS))
    {
        nisShutdown();                                      //shut down the random-number genrator
        utyClear2(SS2_NIS);
    }
    if (utyTest(SSA_ScenarioPicker))
    {
        spShutdown();                                       //shut down the scenario picker
        utyClear(SSA_ScenarioPicker);
    }
    animShutdown();     //shut down the animatics module
    if (utyTest2(SS2_GamePicker))
    {
        gpShutdown();
        utyClear2(SS2_GamePicker);
    }
    if (utyTest2(SS2_MultiplayerGame))
    {
        mgShutdown();
        lgShutdown();
        utyClear2(SS2_MultiplayerGame);
    }
    if(preSetGames != NULL)
    {
        memFree(preSetGames);
        preSetGames = NULL;
    }
    if (utyTest(SSA_Teams))
    {
        teShutdown();                                       //shut down the team-specifics
        utyClear(SSA_Teams);
    }
    if (utyTest(SSA_SensorsManager))
    {
        smShutdown();                                       //shut down the sensors manager
        utyClear(SSA_SensorsManager);
    }
    if (utyTest2(SS2_SubTitle))
    {
        subShutdown();
        utyClear2(SS2_SubTitle);
    }
    if (utyTest2(SS2_Tutorial))
    {
        tutShutdown();
        utyClear2(SS2_Tutorial);
    }
    if (utyTest2(SS2_BattleChatter))
    {
        battleChatterShutdown();
        utyClear2(SS2_BattleChatter);
    }
    if (utyTest2(SS2_Ping))
    {
        pingShutdown();
        utyClear2(SS2_Ping);
    }
    if (utyTest2(SS2_ShipView))
    {
        svShutdown();
        utyClear2(SS2_ShipView);
    }
    // CAT: no task bar to shut down (utyGameSystemsShutdown 0x005ac710; the exe has no tbShutdown)
    if (utyTest2(SS2_CatConsole))
    {                                                            // CAT: the developer console (utyGameSystemsShutdown
        cat_consoleShutdown();                                   //      0x005ac710: before the construction manager)
        utyClear2(SS2_CatConsole);
    }
    if (utyTest(SSA_ConstructionManager))
    {
        cmShutdown();                                            //shut down the construction manager
        utyClear(SSA_ConstructionManager);
    }
    if (utyTest2(SS2_LaunchMgr))
    {
        lmShutdown();
        utyClear2(SS2_LaunchMgr);
    }

    tmShutdown();

#if defined(HW_GAME_DEMO) || defined(HW_GAME_RAIDER_RETREAT)
    if (utyTest2(SS2_PlugScreen))
    {
        psShutdown();
        utyClear2(SS2_PlugScreen);
    }
#endif
    if (utyTest2(SS2_ResearchMgr))
    {
        rmAPIShutdown();
        utyClear2(SS2_ResearchMgr);
    }
    if (utyTest(SSA_Lights))
    {
        lightShutdown();
        utyClear(SSA_Lights);
    }
    if (utyTest(SSA_FrontEndData))
    {
        utyFrontEndDataFree();
        utyClear(SSA_FrontEndData);
    }
    if (utyTest(SSA_FontReg))
    {
        frShutdown();
        utyClear(SSA_FontReg);
    }
    if (utyTest(SSA_FEFlow))
    {
        feShutdown();
        utyClear(SSA_FEFlow);
    }
    if (utyTest(SSA_UIControls))
    {
        uicShutdown();
        utyClear(SSA_UIControls);
    }
    if (utyTest(SSA_Effect))
    {
        etgShutdown();
        //effShutdown();
        utyClear(SSA_Effect);
    }
    if (utyTest2(SS2_InfoOverlay))
    {
        ioShutdown();
        utyClear2(SS2_InfoOverlay);
    }
    if (utyTest(SSA_TextureRegistry))
    {
        trShutdown();
        utyClear(SSA_TextureRegistry);
    }

    if (utyTest(SSA_Render))
    {
        rndClose();
        //the GL has now SHUTDOWN
        utyClear(SSA_Render);
    }

    if (utyTest(SSA_MainRegion))
    {
        mrShutdown();
        utyClear(SSA_MainRegion);
    }

    if (utyTest(SSA_Region))
    {
        regShutdown();
        utyClear(SSA_Region);
    }
    if (utyTest2(SS2_Guns))
    {
        gunShutdown();
        utyClear2(SS2_Guns);
    }
    if (utyTest(SSA_Selection))
    {
        selShutdown();
        utyClear(SSA_Selection);
    }
    if (utyTest(SSA_Mouse))
    {
#if !defined _LINUX_FIX_ME && !defined _WIN32_FIXME
        mouseShutdown();
#endif
        utyClear(SSA_Mouse);
    }

    titanShutdown();        // safe to call multiple times

    //cleans up the prim3d structures
    if (utyTest2(SS2_Prim3D))
    {
        prim3dShutdown();
        utyClear2(SS2_Prim3D);
    }
    //shutdown front end texture registry
#if FEF_TEXTURES_DISABLABLE
    if (fetEnableTextures)
#endif
    {
#if !defined _LINUX_FIX_ME && !defined _WIN32_FIXME
        ferShutdown();
#endif
    }
    if (utyTest(SSA_Task))
    {
        taskShutdown();
        utyClear(SSA_Task);
    }

    //shutdown mesh.c
    meshShutdown();

    bigCloseAllBigFiles();

    keyClose();

#if MEM_ANALYSIS
    memAnalysisCreate();
#endif
    if (utyTest(SSA_MemoryModule))
    {
        memClose(utyGrowthHeapFree);
        utyClear(SSA_MemoryModule);
    }
    if (utyTest(SSA_MemoryHeap))
    {
#ifdef _WIN32
        bool32 result;
        result = VirtualFree(utyMemoryHeap, 0, MEM_RELEASE);
        dbgAssertOrIgnore(result);
#else
        int result;
        result = munmap(utyMemoryHeap, utyMemoryHeapLength);   // HWI-FIX: mapped length, not 0
        dbgAssertOrIgnore(result != -1);
#endif
        utyClear(SSA_MemoryHeap);
    }

    // close down the debug window if so required
    if (DebugWindow)
    {
        if (utyTest(SSA_DebugWindow))
        {
            /* Debug window disabled...search for other dbw*() functions if
               reenabling. */
            /*dbwClose();*/
            utyClear(SSA_DebugWindow);
        }
    }

    globalsClose();

    if (utyTest2(SS2_ToggleKeys))
    {
#ifndef _LINUX_FIX_ME
        utyToggleKeyStatesRestore();
#endif
    }

    utySystemStarted = FALSE;
    return(NULL);
}

/*-----------------------------------------------------------------------------
    Name        : utyTasksDispatch
    Description : Query system timer and call task manager with appropriate
                    number of ticks.
    Inputs      : void
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
void utyTasksDispatch(void)
{
    Uint32 timer;
    Uint32 difference, remainder;

#if HWI_ORACLE
    // HWI-ORACLE: a headless mission run (/oracleMission) advances the task clock by exactly one universe tick per frame
    // instead of by the wall clock, so the run is a function of the frame count alone (OracleDump.c)
    if (oracleVirtualFrameTicks() != 0 && !catMotionRealClock())    // HWI-FEATURE(motion-interp): /catMotionRealClock takes the wall clock
    {
        utyNFrameTicks = oracleVirtualFrameStep();  // HWI-FEATURE(motion-interp): N frames a tick in a motion run (else oracleVirtualFrameTicks())
        taskSubTick = oracleVirtualSubTick();       // HWI-FEATURE(motion-interp): the virtual clock's leftover
        taskSubTickRender = -1.0f;                  // HWI-FEATURE(motion-interp): M3-R1, the virtual clock's is exact
        taskExecuteAllPending(utyNFrameTicks);
        soundVirtualAdvance(utyNFrameTicks, (udword)taskFrequency);    // HWI-TEST (M2-R9): the virtual audio clock, if on
        utyTimerLast = SDL_GetTicks();
        return;
    }
#endif
#if HWI_ORACLE
    if (catMotionRealClock())                               // HWI-FEATURE(motion-interp) measurement [audit: tagged]
    {
        catMotionRealClockPace();                           // HWI-FEATURE(motion-interp) measurement: the frame's slot
    }
#endif
    timer = SDL_GetTicks();                                 //get counter, let's assume it works
    difference = timer - utyTimerLast;                      //get difference of this frame to last frame
    utyNFrameTicks = (udword)(difference / utyTimerDivisor);
    remainder = difference % utyTimerDivisor;               //get remainder of the division
    taskSubTick = (real32)remainder / (real32)utyTimerDivisor;  // HWI-FEATURE(motion-interp): the milliseconds kept for the next frame
    taskClockSample((real64)SDL_GetPerformanceCounter() * 1000.0 / (real64)SDL_GetPerformanceFrequency(), timer, remainder,   // HWI-FEATURE(motion-interp) [audit: tagged]
                    utyTimerDivisor);               // HWI-FEATURE(motion-interp): M3-R1 motion-jitter, the render's sub-millisecond part
#if HWI_ORACLE
    catMotionRealClockStamp(utyNFrameTicks, taskSubTick);   // HWI-FEATURE(motion-interp) measurement
#endif
    taskExecuteAllPending(utyNFrameTicks);                  //execute tasks for the correct number of ticks
    utyTimerLast = timer - remainder;                       //save timer for differencing next frame
}

/*-----------------------------------------------------------------------------
    Name        : utyTaskTimerClear
    Description : Cancel all previous task timer ticks to the present.
    Inputs      : void
    Outputs     : re-querys the system timer and storts result for later
                    differencing
    Return      : void
----------------------------------------------------------------------------*/
void utyTaskTimerClear(void)
{
    Uint32 timer = SDL_GetTicks();                          //get counter, let's assume it works
    utyTimerLast = timer;                                   //save timer for differencing next frame

}

/*-----------------------------------------------------------------------------
    Name        : utyClientRectGet
    Description : Gets the rectangle corresponding to the client rectange
                    _in screen coords_
    Inputs      : rect - rectangle to fill with new coords
    Outputs     : Calls GetClientRect() and ClientToScreen() on both corners.
                    rect gets the new rectangle.
    Return      :
----------------------------------------------------------------------------*/
#if 0
void utyClientRectGet(rectangle *rect)
{
#if 0
    RECT clientRect;
    POINT point;
    GetClientRect(ghMainWindow, &clientRect);               //get location of window
    point.x = clientRect.left;
    point.y = clientRect.top;
    ClientToScreen(ghMainWindow, &point);
    rect->x0 = point.x;
    rect->y0 = point.y;
    /*
    point.x = clientRect.right;
    point.y = clientRect.bottom;
    ClientToScreen(ghMainWindow, &point);
    rect->x1 = point.x;
    rect->y1 = point.y;
    */
    rect->x1 = rect->x0 + MAIN_WindowWidth - 1;
    rect->y1 = rect->y0 + MAIN_WindowHeight - 1;
#endif
    SDL_Surface* pSurface;

    /* Don't know screen coordinates with SDL... */
    rect->x0 = 0;
    rect->y0 = 0;
    pSurface = SDL_GetVideoSurface();
    if (pSurface)
    {
        rect->x1 = pSurface->w - 1;
        rect->y1 = pSurface->h - 1;
    }
    else
    {
        rect->x1 = 0;
        rect->y1 = 0;
    }
}
#endif

/*-----------------------------------------------------------------------------
    Name        : utyForceTopmost
    Description : Determine if the window should be forced topmost
    Inputs      : bTopMost - if TRUE, window will be forced topmost (and above the taskbar)
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void utyForceTopmost(bool32 bTopMost) {
#if 0
    SetWindowPos(ghMainWindow, bTopMost ? HWND_TOPMOST : HWND_NOTOPMOST,
        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOSENDCHANGING);
#endif
}

/*-----------------------------------------------------------------------------
    Name        : utyClipMouse
    Description : Sets or frees the mouse clipped to the client area of main window
    Inputs      : clip - boolean for clip state.  TRUE = clip, FALSE = free
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
#if MAIN_MOUSE_FREE
void utyClipMouse(sdword clip)
{
    SDL_ShowCursor(clip ? SDL_DISABLE : SDL_ENABLE);
    //SDL_WM_GrabInput(clip ? SDL_GRAB_ON : SDL_GRAB_OFF);
    mouseClipped = clip;
}
#endif

/*-----------------------------------------------------------------------------
    Name        : utyMouseButtonsClear
    Description : Resets the internal mouse button states.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyMouseButtonsClear(void)
{
    Uint8 mouse_state;

    mousePoll();
    mouse_state = SDL_GetMouseState(0, 0);
    if (mouse_state & SDL_BUTTON_LMASK)
    {
        keyPressDown(LMOUSE_BUTTON);
    }
    else
    {
        keyPressUp(LMOUSE_BUTTON);
    }
    if (mouse_state & SDL_BUTTON_RMASK)
    {
        keyPressDown(RMOUSE_BUTTON);
    }
    else
    {
        keyPressUp(RMOUSE_BUTTON);
    }
    if (mouse_state & SDL_BUTTON_MMASK)
    {
        keyPressDown(MMOUSE_BUTTON);
    }
    else
    {
        keyPressUp(MMOUSE_BUTTON);
    }
}

/*-----------------------------------------------------------------------------
    Name        : utyCaretBlinkRate
    Description : Get the rate of the cursor blink action from Windows.
    Inputs      :
    Outputs     :
    Return      : Blink rate, in seconds.
----------------------------------------------------------------------------*/
real32 utyCaretBlinkRate(void)
{
#ifdef _WIN32
    return((real32)GetCaretBlinkTime() / 500.0f);
#else
    return 0.25f;  /* Works for me... */
#endif
}

/*-----------------------------------------------------------------------------
    Name        : utyDoubleClick
    Description : Process a double-click event
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyDoubleClick(void)
{
    if (!mouseLDoubleClick())
    {
        //this is needed because the second button click is
        //not registered as a WM_LBUTTONDOWN click.  Only call this
        //function if mouseLDoubleClick didn't do any special processing
        keyPressDown(LMOUSE_BUTTON);
    }
    keyPressUp(LMOUSE_BUTTON);
}

/*-----------------------------------------------------------------------------
    Name        : utyChangeResolution
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 utyChangeResolution(sdword width, sdword height, sdword depth) {
    if (!utyTest(SSA_Render))
    {
        return FALSE;
    }

#ifdef __EMSCRIPTEN__
    rndAspectRatio = (GLfloat)width/(GLfloat)height;
    glViewport(0, 0, width, height);
#endif

    ghMainRegion->rect.x1 = width;
    ghMainRegion->rect.y1 = height;

    (void)feAllScreensReposition();

    //rescale the subtitle regions
    subRegionsRescale();

    //reposition the info overlay regions
    ioResolutionChange();

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : utyToggleKeyStatesSave
    Description : Save the state of all toggle keys.
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
static sdword utyCapsLockState;
static sdword utyNumLockState;
static sdword utyScrollLockState;
void utyToggleKeyStatesSave(void)
{
    const Uint8* state = SDL_GetKeyboardState(NULL);
    utyCapsLockState = state[SDL_SCANCODE_CAPSLOCK] & 1;
    utyNumLockState = state[SDL_SCANCODE_NUMLOCKCLEAR] & 1;
    utyScrollLockState = state[SDL_SCANCODE_SCROLLLOCK] & 1;
}

/*-----------------------------------------------------------------------------
    Name        : utyToggleKeyStatesRestore
    Description : Restore the caps, scroll and num lock keys to what they were
                    at when you started the game or whence last you alt-tabbed
                    back to the game.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void utyToggleKeyStatesRestore(void)
{
    SDL_Keymod target = 0;
    const Uint8* state = SDL_GetKeyboardState(NULL);
#if !defined(_WIN32) && !defined(_MACOSX) && !defined(__EMSCRIPTEN__)
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(sdlwindow,&info);
    XEvent xe;
    xe.xkey.display = info.info.x11.display;
    xe.xkey.window = info.info.x11.window;
    xe.xkey.root = RootWindow(info.info.x11.display,
        DefaultScreen(info.info.x11.display));
    xe.xkey.subwindow = None;
    xe.xkey.time = CurrentTime;
    xe.xkey.x = 1;
    xe.xkey.y = 1;
    xe.xkey.x_root = 1;
    xe.xkey.y_root = 1;
    xe.xkey.state = 0;
    xe.xkey.same_screen = TRUE;
#endif
    if((utyCapsLockState && !(state[SDL_SCANCODE_NUMLOCKCLEAR] & 1)) || (!utyCapsLockState && (state[SDL_SCANCODE_CAPSLOCK] & 1)) )
    {
#ifdef _WIN32
        // Simulate a key press
        keybd_event( VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0 );

        // Simulate a key release
        keybd_event( VK_CAPITAL, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
#elif !defined(_MACOSX) && !defined(__EMSCRIPTEN__)
        xe.xkey.keycode = XKeysymToKeycode(info.info.x11.display, XK_Caps_Lock);

        // Simulate a key press
        xe.xkey.type = KeyPress;
        XSendEvent(info.info.x11.display, info.info.x11.window, TRUE,
            KeyPressMask, &xe);

        // Simulate a key release
        xe.xkey.type = KeyRelease;
        XSendEvent(info.info.x11.display, info.info.x11.window, TRUE,
            KeyPressMask, &xe);
#endif
        target |= KMOD_CAPS;
    }
    if((utyScrollLockState && !(state[SDL_SCANCODE_NUMLOCKCLEAR] & 1)) || (!utyScrollLockState && (state[SDL_SCANCODE_SCROLLLOCK] & 1)) )
    {
#ifdef _WIN32
        // Simulate a key press
        keybd_event( VK_SCROLL, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0 );

        // Simulate a key release
        keybd_event( VK_SCROLL, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
#elif !defined(_MACOSX) && !defined(__EMSCRIPTEN__)
        xe.xkey.keycode = XKeysymToKeycode(info.info.x11.display, XK_Scroll_Lock);

        // Simulate a key press
        xe.xkey.type = KeyPress;
        XSendEvent(info.info.x11.display, info.info.x11.window, TRUE,
            KeyPressMask, &xe);

        // Simulate a key release
        xe.xkey.type = KeyRelease;
        XSendEvent(info.info.x11.display, info.info.x11.window, TRUE,
            KeyPressMask, &xe);
#endif
    }
    if((utyNumLockState && !(state[SDL_SCANCODE_NUMLOCKCLEAR] & 1)) || (!utyNumLockState && (state[SDL_SCANCODE_NUMLOCKCLEAR] & 1)) )
    {
#ifdef _WIN32
        // Simulate a key press
        keybd_event( VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0 );

        // Simulate a key release
        keybd_event( VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
#elif !defined(_MACOSX) && !defined(__EMSCRIPTEN__)
        xe.xkey.keycode = XKeysymToKeycode(info.info.x11.display, XK_Num_Lock);

        // Simulate a key press
        xe.xkey.type = KeyPress;
        XSendEvent(info.info.x11.display, info.info.x11.window, TRUE,
            KeyPressMask, &xe);

        // Simulate a key release
        xe.xkey.type = KeyRelease;
        XSendEvent(info.info.x11.display, info.info.x11.window, TRUE,
            KeyPressMask, &xe);
#endif
        target |= KMOD_NUM;
    }
    /* Modifying internal keystate array.  Original HW source code did this
       using the Windows API SetKeyboardState() function, so hopefully we get
       the same result here ("state" already points to the internal array used
       by SDL). */
    //state[SDL_SCANCODE_CAPSLOCK]  = (state[SDL_SCANCODE_CAPSLOCK]  & 0xfe) | (utyCapsLockState);
    //state[SDL_SCANCODE_NUMLOCKCLEAR]   = (state[SDL_SCANCODE_NUMLOCKCLEAR]   & 0xfe) | (utyNumLockState);
    //state[SDL_SCANCODE_SCROLLLOCK] = (state[SDL_SCANCODE_SCROLLLOCK] & 0xfe) | (utyScrollLockState);
    SDL_SetModState(target);
}

/*-----------------------------------------------------------------------------
    Name        : utyCapsLockToggleState
    Description : Returns the toggle state of the caps lock key
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 utyCapsLockToggleState(void) { return SDL_GetModState() & KMOD_CAPS; }

void utyStartDroppedDialog(sdword playernum)
{
    cmCloseIfOpen();
    rmCloseIfOpen();
    lmCloseIfOpen();
    mrReset();
    utyPlayerDroppedDisplay = playernum;
    mrDisable();
    feScreenStart(ghMainRegion, "Player_Drop");
}

void utyYesDropPlayer(char *name, featom *atom)
{
    KeepAliveDropPlayerCB(utyPlayerDroppedDisplay);
    utyPlayerDroppedDisplay = -1;

    mrEnable();
    feScreenDisappear(NULL,NULL);
}

void utyDontDropPlayer(char *name, featom *atom)
{
    KeepAliveDontDropPlayerCB(utyPlayerDroppedDisplay);
    utyPlayerDroppedDisplay = -1;

    mrEnable();
    feScreenDisappear(NULL,NULL);
}

void utyDrawDroppedPlayer(featom *atom, regionhandle region)
{
    fonthandle fhsave;

    if (FELASTCALL(atom))
    {
        KeepAliveDontDropPlayerCB(utyPlayerDroppedDisplay);
        utyPlayerDroppedDisplay = -1;

        mrEnable();

        return;
    }

    if (utyPlayerDroppedDisplay != -1)
    {
        fhsave = fontMakeCurrent(selGroupFont0);

        fontPrint(region->rect.x0, region->rect.y0, colRGB(255,200,0), playerNames[utyPlayerDroppedDisplay]);

        fontMakeCurrent(fhsave);
    }
}

