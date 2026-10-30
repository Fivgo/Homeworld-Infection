/*=============================================================================
    Name    : InfoOverlay.c
    Purpose : This file contains the logic for the information overlay.

    Created 6/8/1998 by ddunlop
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

// CAT (M2-R5 hud): Cataclysm replaced HW1's info overlay (one clickable region per selected ship type, top right) with
//      the main overlay: the selected / band-boxed ship lists, the objectives, the tooltip, the game message scroller,
//      the player names, the construction and research progress, the support units and mission timer, the sensors
//      manager's camera buttons, the taskbar buttons, the print options help and the paused / end-of-recording text.
//      The exe's source file holds ioStartup 0x004ab4b0 (REWRITTEN), ioUpdateShipTotals 0x004ab650, the cat_io* and the
//      cat_mo* functions 0x004a5e40-0x004acbc0 (toLegendDraw 0x004a8f00 among them, kept in Tactical.c); every body
//      below follows the r6 decompile and, where it hides an operand, the disassembly. HW1's region list, its click
//      callback and draw callback are gone; ioEnable / ioDisable / ioSetSelection / ioShutdown / ioResolutionChange
//      have no exe counterpart and stay as small stand-ins for their unported HW1 callers (the managers, mainrgn).

#include "InfoOverlay.h"

#include <stdio.h>
#include <string.h>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

#include "Alliance.h"
#include "Bounties.h"
#include "MultiplayerGame.h"
#include "Teams.h"
#include "CameraCommand.h"
#include "ConsMgr.h"
#include "Crystal.h"
#include "FEFlow.h"
#include "FontReg.h"
#include "Globals.h"
#include "glinc.h"
#include "Key.h"
#include "KeyBindings.h"
#include "UnivUpdate.h"
#include "Console.h"
#include "main.h"
#include "mainrgn.h"
#include "mainswitches.h"
#include "Mimic.h"
#include "mouse.h"
#include "NIS.h"
#include "Objectives.h"
#include "OverlayIcons.h"
#include "PiePlate.h"
#include "PopupMenu.h"
#include "prim2d.h"
#include "render.h"
#include "ResearchAPI.h"
#include "SaveGame.h"
#include "Select.h"
#include "Sensors.h"
#include "ShipNames.h"
#include "ShipPrint.h"
#include "GamePick.h"
#include "ShipSelect.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "SpeechEvent.h"
#include "StatScript.h"
#include "StringSupport.h"
#include "Tactical.h"
#include "Task.h"
#include "Tutor.h"
#include "Universe.h"
#include <stdlib.h>
#include "Memory.h"
#include "OracleDump.h"
#include "utility.h"
#include <math.h>    // HWI-FIX(release-1.0): declared on every platform (mingw does not pull it in)

void smCullAndFocusSelecting(void);                 // Sensors.c (no prototype in Sensors.h)
// CAT (M2-R6 oracle-mode-a): the tooltip's object kind is the exe's 0x008ba740, which selShipUnderMouse 0x00506550
//      sets to -1 (enemy ship), -2 (resource), -4 (allied ship), -5 (derelict) or the own ship's type: this tree's
//      mouseCursorOverShipType (mouse.c, MC_*). The tooltip read mouseCursorEnemyType (0x008ba744, the ship type
//      only), so a resource under the cursor kept the kind of the last ship and was drawn as an enemy ship; the
//      replay of the skirmish recording crashed on the asteroid's "player" (tick 3501)
extern sdword mouseCursorOverShipType;              // mouse.c

/*=============================================================================
    Data:
=============================================================================*/

bool32 ioRunning = TRUE;                            // HW1's flag, kept for ioEnable / ioDisable (no exe counterpart)

// CAT: the overlay's fonts (ioStartup 0x004ab4b0 registers 13; two are never used)
static fonthandle catMoPrintOptionsFont;            // 0x00a4b8bc default.hff
static fonthandle catMoShipListFont;                // 0x00a4b8c0 SimplixSSK_13.hff (selected / selecting)
static fonthandle catMoObjectivesFont;              // 0x00a4b8c4 default.hff
static fonthandle catMoSupportUnitsFont;            // 0x00a4b8c8 default.hff
static fonthandle catMoTooltipFont;                 // 0x00a4b8cc SimplixSSK_13.hff
static fonthandle catMoUnusedFont0;                 // 0x00a4b8d0 default.hff
static fonthandle catMoMessagesFont;                // 0x00a4b8d4 default.hff (messages, taskbar buttons)
static fonthandle catMoUnusedFont1;                 // 0x00a4b8d8 default.hff
static fonthandle catMoPlayerNamesFont;             // 0x00a4b8dc default.hff
static fonthandle catMoConstructionFont;            // 0x00a4b8e0 default.hff
static fonthandle catMoResearchFont;                // 0x00a4b8e4 default.hff
static fonthandle catMoPausedFont;                  // 0x00a4b8e8 Arial_b17.hff
static fonthandle catMoDebugKeysFont;               // 0x00a4b8ec default.hff

// CAT: the print options: per game kind (single / multiplayer) and overlay mode (cat_ioCycleMode: 0..2), ten
//      elements of {show, backdrop} (MainOverlay.script; cat_moPrintOptionsSP 0x00a46f70, cat_moPrintOptionsMP
//      0x00a46c80)
typedef struct
{
    bool32 show;
    bool32 box;
} CatMoOption;
typedef struct
{
    CatMoOption selected, selecting, objectives, tooltips, viewOptions, playerInfo, gameMessage, playerNames,
                construction, research;
} CatMoOptions;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatMoOptions) == 0x50, "the main overlay's print options are 0x50 bytes per mode in cataclysm.exe");
#endif
static CatMoOptions cat_moPrintOptionsSP[3];         // 0x00a46f70
static CatMoOptions cat_moPrintOptionsMP[3];         // 0x00a46c80
static sdword catMoMode = 0;                        // 0x00a4b944: the overlay mode (0 the full overlay)
static bool32 catMoPrintBoxes = FALSE;              // 0x00a4b88c: the print option keys switch the backdrops

#define catMoOptions()      ((singlePlayerGame == 1) ? &cat_moPrintOptionsSP[catMoMode] : &cat_moPrintOptionsMP[catMoMode])

// CAT: the taskbar options (Options.c: cat_opHWTaskbar 0x004d9e30, cat_opTaskbarButtons 0x004d9e70,
//      cat_opTaskbarBackdrop 0x004d9eb0; cat_opCataclysmDefault 0x004da460 clears all three, cat_opHomeworldClassic
//      0x004da2c0 sets them; the options file rows HomeworldTaskbar / ShowTaskbarButtons / ShowTaskbarBackdrop)
bool32 catMoHWTaskbar = FALSE;                      // 0x00a4b878: the bottom panel slides in at the screen edge
bool32 catMoTaskbarButtons = FALSE;                 // 0x00a4b87c: the five taskbar buttons
bool32 catMoTaskbarBackdrop = FALSE;                // 0x00a4b880: a translucent backdrop over the bottom panel

// CAT: the ship lists, one entry per ship type (0x20 bytes each, 0x91 types)
typedef struct
{
    bool32 present;             // +0x00
    bool32 hover;               // +0x04 set by cat_ioUpdateHover
    rectangle rect;             // +0x08 the line, set by cat_moSelectedDraw
    sdword count;               // +0x18
    bool32 pressed;             // +0x1C set by cat_ioClick (select)
} CatIoShipEntry;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatIoShipEntry) == 0x20, "a ship list entry is 0x20 bytes in cataclysm.exe");
#endif
static CatIoShipEntry catIoShips[TOTAL_NUM_SHIPS];          // 0x00a45a20: the selection (own ships, others' as seen)
static CatIoShipEntry catIoDisabled[TOTAL_NUM_SHIPS];       // 0x00a44800: own disabled ships
static CatIoShipEntry catIoDisabledExempt[TOTAL_NUM_SHIPS]; // 0x00a4a638: own disabled ships with SPECIAL_2_CatExempt20

// CAT: the objectives list (0x18 bytes each)
#define CAT_IO_MAX_OBJECTIVES   30
typedef struct
{
    bool32 hover;               // +0x00
    rectangle rect;             // +0x04
    Objective *objective;       // +0x14
} CatIoObjective;
static CatIoObjective catIoObjectives[CAT_IO_MAX_OBJECTIVES];  // 0x00a46d90
static sdword catIoObjectiveCount = 0;              // 0x00a4b8f0

// CAT: the game message scroller (0x224 bytes each; a message refers to a ship (type 1) or the research (type 2))
#define CAT_IO_MESSAGE_LENGTH   0x200
#define CAT_IO_MAX_MESSAGES     26
typedef struct
{
    char text[CAT_IO_MESSAGE_LENGTH];   // +0x000
    rectangle rect;             // +0x200
    bool32 hover;               // +0x210
    sdword shownTime;           // +0x214 (sdword)cat_taskRealTimeElapsed of the last line's first draw
    udword type;                // +0x218 1 a ship, 2 the research manager
    void *data;                 // +0x21C the ship
    color colour;               // +0x220
} CatIoMessage;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatIoMessage) == 0x224, "a game message is 0x224 bytes in cataclysm.exe");
#endif
static CatIoMessage catIoMessages[CAT_IO_MAX_MESSAGES];   // 0x00a47070
static sdword catIoMessageCount = 0;                // 0x00a4b8f4
static sdword catIoMessagesShown = 0;               // 0x00a4b8f8: lines shown (at most 5)
static sdword catIoMessageLast = 0;                 // 0x00a4b8fc: the newest line shown
static bool32 catIoMessageUpShown = FALSE;          // 0x00a4b900: the scroll-up button
static rectangle catIoMessageUpRect;                // 0x00a4b868
static bool32 catIoMessageDownShown = FALSE;        // 0x00a4b904: the scroll-down button
static rectangle catIoMessageDownRect;              // 0x00a46c50

// CAT: the construction and research lists' scroll buttons and pages
typedef struct
{
    bool32 shown;               // +0x00
    rectangle rect;             // +0x04
    bool32 hover;               // +0x14
    sdword direction;           // +0x18 0 up, 1 down (cat_moBoxLayout)
} CatMoScrollButton;
static CatMoScrollButton catMoConsUp = { FALSE, { 0, 0, 0, 0 }, FALSE, 0 };       // 0x00a4b908
static CatMoScrollButton catMoConsDown = { FALSE, { 0, 0, 0, 0 }, FALSE, 1 };     // 0x00884f00
static CatMoScrollButton catMoResearchUp = { FALSE, { 0, 0, 0, 0 }, FALSE, 0 };   // 0x00a4b928
static CatMoScrollButton catMoResearchDown = { FALSE, { 0, 0, 0, 0 }, FALSE, 1 }; // 0x00884f20
static sdword catMoConsPage = 0;                    // 0x00a4b8a4
static sdword catMoResearchPage = 0;                // 0x00a4b8a8

// CAT: the view buttons (0x1c bytes each, 0x00884e58: rectangles never set in 1.01, so never hit)
typedef struct
{
    bool32 hover;
    rectangle rect;
    sdword stringId;
    sdword command;
} CatMoViewButton;
static CatMoViewButton catMoViewButtons[6] =
{
    { FALSE, { 0, 0, 0, 0 }, strMO_ViewSensorsManagerDesc, 0x4f }, { FALSE, { 0, 0, 0, 0 }, strMO_ViewMainRgnDesc, 0x4f },
    { FALSE, { 0, 0, 0, 0 }, strMO_ViewResearchManagerDesc, 0x51 }, { FALSE, { 0, 0, 0, 0 }, strMO_ViewBuildManagerDesc, 0x50 },
    { FALSE, { 0, 0, 0, 0 }, strMO_ViewShipInfoOverlayDesc, 0x54 }, { FALSE, { 0, 0, 0, 0 }, strMO_ViewTacticalOverlayDesc, 0x53 },
};

// CAT: the taskbar buttons (cat_moObjectivesFrameDraw): sensors / main view, build, research, systems
static rectangle catMoButtonSensors;                // 0x00a47060
static rectangle catMoButtonBuild;                  // 0x00a46c40
static rectangle catMoButtonResearch;               // 0x00a4a628
static rectangle catMoButtonSystems;                // 0x00a4a5f8

// CAT: the backdrops (each element's translucent box, grown by cat_moPrintWithBox and drawn the next frame)
static rectangle catMoSelectedBox;                  // 0x00a4b858
static bool32 catMoSelectedBoxUsed = FALSE;         // 0x00a4b948
static rectangle catMoSelectingBox;                 // 0x00a46c60
static bool32 catMoSelectingBoxUsed = FALSE;        // 0x00a4b94c
static rectangle catMoObjectivesBox;                // 0x00a4b950
static sdword catMoObjectivesRight = 0;             // 0x00a4b958
static rectangle catMoSupportBox;                   // 0x00a46d80
static rectangle catMoPlayerNamesBox;               // 0x00a4a618
static bool32 catMoPlayerNamesBoxUsed = FALSE;      // 0x00a4b970
static rectangle catMoConsBox;                      // 0x00a4a608
static bool32 catMoConsBoxUsed = FALSE;             // 0x00a4b974
static rectangle catMoResearchBox;                  // 0x00a46c70
static bool32 catMoResearchBoxUsed = FALSE;         // 0x00a4b978

// CAT: the camera buttons of the sensors manager (cat_moButtonsDraw)
static bool32 catMoButtonClicked[4];                // 0x00a4b984 / 0x00a4b988 / 0x00a4b980 / 0x00a4b97c

// CAT: the rest of the overlay's state
static sdword catIoSubtitleOffset = 0;              // 0x00a4b890: the lists move down under a subtitle
static sdword catIoSubtitleTarget = 0;              // 0x00a4b894
static real32 catIoSubtitleTime = 0.0f;             // 0x00a4b898
static bool32 catIoHover = FALSE;                   // 0x00a4b89c: the mouse is over an overlay element
static bool32 catIoHoverHyperspace = FALSE;         // 0x00a4b8a0: over the "hyperspace" / "end mission" objective
static SpaceObj *catMoTooltipObject = NULL;         // 0x00a4b8ac
static real32 catMoTooltipTime = 0.0f;              // 0x00a4b8b0
static real32 catMoTooltipDelay = 2.0f;             // 0x00884b10
static real32 catMissionTimerStart = 0.0f;          // 0x00a4b8b4
static sdword catMoBottomTop = 0;                   // 0x00a4b8b8: top of the bottom panel's lines
static sdword catMoBottomY = 0;                     // 0x00a4b888: the bottom panel's current top (slides)
static bool32 catMoBottomShown = FALSE;             // 0x00a4b884: the mouse brought the HW taskbar in
static real32 catMoSupportBlinkTime = 0.0f;         // 0x00a4b968
rectangle catHudSupportBlinkRect = {0, 0, 0, 0};    // HWI-TEST (M3-R3 pause-freeze): the blinking SU value drawn this frame
// CAT (M2-R6 startup-keys): 0x00a6ed74 is the Pause key's flag (UnivUpdate.c catUnivFrameStep; it was named after
//      cat_kasWideScreenIn here): the overlay prints "paused" while it is set and hides the objectives pointer
#define catMoWideScreen             catUnivFrameStep

// CAT: the layout constants (exe data 0x0072ab88..)
#define CAT_MO_LIST_WIDTH           150     // 0x0072ace4
#define CAT_MO_LIST_TOP             10      // 0x0072ace0
#define CAT_MO_SELECTING_GAP        10      // 0x0072ace8
#define CAT_MO_MARGIN               10      // 0x0072acf4
#define CAT_MO_OBJ_GAP              4       // 0x0072acf8
#define CAT_MO_BOTTOM_GAP           20      // 0x0072acf0
#define CAT_MO_INFO_GAP             10      // 0x0072ad28
#define CAT_MO_MIN_BOTTOM           50      // 0x0072aec4
#define CAT_MO_SLIDE_STEP           2       // 0x0072aec0
#define CAT_MO_BUTTONS_UP           20      // 0x0072aebc
#define CAT_MO_BOX_PAD              5       // 0x0072ad90
#define CAT_MO_OBJ_BOX_PAD          5       // 0x0072ad9c
#define CAT_MO_OBJ_RIGHT_PAD        5       // 0x0072ada0
#define CAT_MO_SUPPORT_RIGHT        10      // 0x0072ad30
#define CAT_MO_SUPPORT_PAD          5       // 0x0072ada4 / 0x0072ada8
#define CAT_MO_NAMES_TOP            20      // 0x0072ad34
#define CAT_MO_NAMES_X              10      // 0x0072ad38
#define CAT_MO_NAMES_GAP            2       // 0x0072ad3c
#define CAT_MO_NAMES_TEXT_GAP       4       // 0x0072ad40
#define CAT_MO_CONS_X               10      // 0x0072ad4c
#define CAT_MO_CONS_TOP             15      // 0x0072ad50
#define CAT_MO_RESEARCH_X           10      // 0x0072ad54
#define CAT_MO_RESEARCH_TOP         15      // 0x0072ad58
#define CAT_MO_SCROLL_X             30      // 0x0072ad60
#define CAT_MO_SCROLL_WIDTH         25      // 0x0072ad64
#define CAT_MO_SCROLL_HEIGHT        17      // 0x0072ad68
#define CAT_MO_BAR_WIDTH            150     // 0x0072ad6c
#define CAT_MO_TRI_PAD              2       // 0x0072adcc
#define CAT_MO_MESSAGES_X           17      // 0x0072adc8
#define CAT_MO_MESSAGES_GAP         10      // 0x0072adc4
#define CAT_MO_MESSAGE_FADE_AFTER   8.0f    // 0x0072ad88
#define CAT_MO_MESSAGE_FADE_TIME    5.0f    // 0x0072ad8c
#define CAT_MO_TB_GAP               6       // 0x0072adac
#define CAT_MO_TB_ROW_GAP           3       // 0x0072adb0
#define CAT_MO_TB_WIDTH             90      // 0x0072adb4
#define CAT_MO_TB_HEIGHT            20      // 0x0072adb8
#define CAT_MO_TB_TOP               5       // 0x0072adbc
#define CAT_MO_TB_TEXT              15      // 0x0072adc0
#define CAT_MO_TOOLTIP_DX           (-5)    // 0x0072ad0c
#define CAT_MO_TOOLTIP_DY           30      // 0x0072ad10
#define CAT_MO_TOOLTIP_THICK        2       // 0x0072ad14
#define CAT_MO_TOOLTIP_ROUND        2       // 0x0072ad18
#define CAT_MO_OPTIONS_X            190     // 0x0072ad04
#define CAT_MO_OPTIONS_Y            20      // 0x0072ad00
#define CAT_MO_SUPPORT_LOW          100.0f  // 0x0072ad7c
#define CAT_MO_SUPPORT_HIGH         100.0f  // 0x0072ad80
#define CAT_MO_SUPPORT_BLINK        3.0f    // 0x0072ad84
#define CAT_MO_PAUSED_Y             0.12f   // 0x0072ad70

// CAT: the colours (exe data 0x0072ab8c..)
#define CAT_MO_TRANSLUCENT          0x64323232
static color catMoOptionOnColour = 0xff00ff00, catMoOptionOffColour = 0xff0000ff;            // 0x0072ab8c / 0x0072ab90
static color catMoOptionsBoxColour = 0xff000000, catMoOptionsOutlineColour = 0xffffffff;      // 0x0072ab94 / 0x0072ab98
static color catMoListColour = 0xffffffff, catMoListHoverColour = 0xff00ff00;                // 0x0072ab9c / 0x0072aba0
static color catMoListExemptColour = 0xffffc000, catMoListPressedColour = 0xffff6464;        // 0x0072aba4 / 0x0072aba8
static color catMoListDisabledColour = 0xff787878;                                           // 0x0072abac
static color catMoSelectingOwnColour = 0xff00ff00, catMoSelectingAlliedColour = 0xffffc000;   // 0x0072abcc / 0x0072abd0
static color catMoSelectingEnemyColour = 0xff0000ff;                                         // 0x0072abd4
static color catMoObjIncompleteColour = 0xff0000ff, catMoObjCompleteColour = 0xff00ff00;     // 0x0072abe0 / 0x0072abe4
static color catMoObjHoverColour = 0xffffffff, catMoObjTitleColour = 0xffffffff;             // 0x0072abe8 / 0x0072abf0
static color catMoSupportLabelColour = 0xffc8c8c8, catMoSupportValueColour = 0xffffffff;     // 0x0072ac38 / 0x0072ac3c
static color catMoSupportLowColour = 0xff00ff00, catMoSupportMidColour = 0xff00ffff;          // 0x0072ac40 / 0x0072ac44
static color catMoSupportHighColour = 0xff0000ff, catMoSupportBlinkColour = 0xff00ffff;       // 0x0072ac48 / 0x0072ac4c
static color catMoMessageShipColour = 0xff00ff00, catMoMessageFadeColour = 0x00000000;       // 0x0072ac64 / 0x0072ac68
static color catMoNamesSelfColour = 0xff00ff00, catMoNamesAllyColour = 0xffff0000;           // 0x0072ac74 / 0x0072ac78
static color catMoNamesEnemyColour = 0xff0000ff, catMoNamesDeadColour = 0xff646464;          // 0x0072ac7c / 0x0072ac80
static color catMoNamesBountyColour = 0xffffffff;                                            // 0x0072ac84
static color catMoConsColour = 0xff25af5b, catMoResearchColour = 0xffbfaf6a;                // 0x0072ac90 / 0x0072aca0
static color catMoBarShadeColour = 0x4b000000;                                               // 0x0072ac9c
static color catMoTriHoverColour = 0xff00ff00, catMoTriOutlineColour = 0xffffffff;          // 0x0072acac / 0x0072acb0
static color catMoTimerColour = 0xffffffff, catMoPausedColour = 0xffffffff;                 // 0x0072acb4 / 0x0072acbc
// the tooltip's {outline, text, backdrop} per kind of object
static color catMoTipObjOutline = 0xffa0a0a0, catMoTipObjText = 0xff3ac1e9, catMoTipObjBox = 0x645fc855;      // 0x0072ac04 ..
static color catMoTipMissileOutline = 0xff0000ff, catMoTipMissileText = 0xff0000ff, catMoTipMissileBox = 0x64646464;   // ..0x0072ac10
static color catMoTipEnemyOutline = 0xffa0a0a0, catMoTipEnemyText = 0xff0000ff, catMoTipEnemyBox = 0x644b6496;   // 0x0072ac1c ..
static color catMoTipAllyOutline = 0xffa0a0a0, catMoTipAllyText = 0xffffc000, catMoTipAllyBox = 0x64000000;      // 0x0072ac28 ..
static color catMoTipOwnOutline = 0xffa0a0a0, catMoTipOwnText = 0xff00ff00, catMoTipOwnBox = 0x644b644b;         // 0x0072ac34 ..

// CAT: the overlay asks for Cataclysm key commands held (kbCommandKeyIsHit 0x0054ec60 with the exe's ids: 0x41..0x44
//      the camera buttons, 0x62 the ship-info key, 100..102 the debug key lists) and tests the developer console
//      (0x00a2dcd4). [M2-R6 startup-keys: the loaded bindings (KeyBindings.c) and the console (Console.c)]
//      [M2-R6 mainrgn-popup: the commands run through the key-command table (cat_kbCommandExecute 0x0054eb10,
//      KeyBindings.c) and the popup menu is PopupMenu.c's]
#define CAT_KB_HELD(command)        kbCommandKeyIsHit(command)
#define CAT_KB_EXECUTE(sensors, command)    cat_kbCommandExecute((sensors) != 0, (command))
#define CAT_CONSOLE_OPEN()          cat_consoleIsOpen()
#define CAT_POPUP_MENU_OPEN()       cat_popupMenuIsOpen()   // CAT (M2-R6 mainrgn-popup): PopupMenu.c

/*=============================================================================
    The HW1 interface the managers still call (no exe counterpart)
=============================================================================*/
void ioSetSelection(bool32 shiftrelease)
{
    (void)shiftrelease;                             // Cataclysm selects from the overlay list (cat_ioClick)
}

bool32 ioDisable(void)
{
    bool32 save = ioRunning;

    ioRunning = FALSE;
    return (save);
}

void ioEnable(void)
{
    ioRunning = TRUE;
    ioUpdateShipTotals();
}

void ioShutdown(void)
{
    ;
}

void ioResolutionChange(void)
{
    ;
}

/*=============================================================================
    The script, start-up and reset
=============================================================================*/
// CAT: the script callback 0x004a5dc0: "show,backdrop"; the entry points at the two flags
typedef struct
{
    bool32 *show;
    bool32 *box;
} CatMoOptionRef;
static CatMoOptionRef catMoOptionRefs[60];

static void catMoOptionSetCB(char *directory, char *field, void *dataToFillIn)
{
    CatMoOptionRef *ref = (CatMoOptionRef *)dataToFillIn;
    sdword show = 0, box = 0;

    (void)directory;
    if (sscanf(field, "%d,%d", &show, &box) != 2)
    {
        dbgMessagef("Could not read Overlay Print Option: %s", field);   // 0x00462ae0 (a debug warning)
    }
    *ref->show = (show != 0);
    *ref->box = (box != 0);
}

static char *catMoOptionNames[10] =
{
    "SELECTED", "SELECTING", "OBJECTIVES", "TOOLTIPS", "VIEWOPTIONS", "PLAYERINFO", "GAMEMESSAGE", "PLAYERNAMES",
    "CONSTRUCTION", "RESEARCH"
};
static char catMoOptionKeys[60][32];
static scriptEntry catMoOptionTweaks[61];

/*-----------------------------------------------------------------------------
    Name        : ioStartup                                         0x004ab4b0
    Description : CAT: registers the overlay's fonts, reads MainOverlay.script (table 0x00885120: SP_MO_1_SELECTED ..
                  MP_MO_3_RESEARCH) and resets the overlay.
----------------------------------------------------------------------------*/
void ioStartup(void)
{
    sdword kind, mode, element, index;
    CatMoOption *option;

    catMoPrintOptionsFont = frFontRegister("default.hff");
    catMoShipListFont = frFontRegister("SimplixSSK_13.hff");
    catMoObjectivesFont = frFontRegister("default.hff");
    catMoSupportUnitsFont = frFontRegister("default.hff");
    catMoTooltipFont = frFontRegister("SimplixSSK_13.hff");
    catMoUnusedFont0 = frFontRegister("default.hff");
    catMoMessagesFont = frFontRegister("default.hff");
    catMoUnusedFont1 = frFontRegister("default.hff");
    catMoPlayerNamesFont = frFontRegister("default.hff");
    catMoConstructionFont = frFontRegister("default.hff");
    catMoResearchFont = frFontRegister("default.hff");
    catMoPausedFont = frFontRegister("Arial_b17.hff");
    catMoDebugKeysFont = frFontRegister("default.hff");
    (void)catMoUnusedFont0;
    (void)catMoUnusedFont1;
    index = 0;
    for (kind = 0; kind < 2; kind++)
    {
        for (mode = 0; mode < 3; mode++)
        {
            for (element = 0; element < 10; element++, index++)
            {
                option = &(kind == 0 ? cat_moPrintOptionsSP : cat_moPrintOptionsMP)[mode].selected + element;
                snprintf(catMoOptionKeys[index], sizeof(catMoOptionKeys[index]), "%s_MO_%d_%s", kind == 0 ? "SP" : "MP",
                         mode + 1, catMoOptionNames[element]);
                catMoOptionRefs[index].show = &option->show;
                catMoOptionRefs[index].box = &option->box;
                catMoOptionTweaks[index].name = catMoOptionKeys[index];
                catMoOptionTweaks[index].setVarCB = catMoOptionSetCB;
                catMoOptionTweaks[index].dataPtr = &catMoOptionRefs[index];
            }
        }
    }
    catMoOptionTweaks[60] = (scriptEntry)END_SCRIPT_ENTRY;
    scriptSet(NULL, "MainOverlay.script", catMoOptionTweaks);
    cat_ioReset();
}

/*-----------------------------------------------------------------------------
    Name        : cat_ioReset                                       0x004ab590
    Description : Clear the lists, the objectives, the messages and the scroll pages (ioStartup, universeReset).
----------------------------------------------------------------------------*/
void cat_ioReset(void)
{
    sdword index;

    for (index = 0; index < TOTAL_NUM_SHIPS; index++)
    {
        catIoShips[index].present = FALSE;
        catIoDisabled[index].present = FALSE;
        catIoDisabledExempt[index].present = FALSE;
    }
    catIoObjectiveCount = 0;
    catIoSubtitleOffset = 0;
    catIoSubtitleTarget = 0;
    catIoMessageCount = 0;
    catIoMessagesShown = 0;
    catIoMessageLast = 0;
    catIoMessageUpShown = FALSE;
    catIoMessageDownShown = FALSE;
    catMoConsPage = 0;
    catMoResearchPage = 0;
    catMoTooltipObject = NULL;
    catIoSubtitleTime = cat_taskRealTimeElapsed;
    catMoTooltipTime = 0.0f;
    catMoBottomY = MAIN_WindowHeight;
}

/*-----------------------------------------------------------------------------
    Name        : cat_ioSubtitleNotify                              0x004ab620
    Description : A subtitle of this height starts (subTitlesDraw): the lists move down while it shows.
----------------------------------------------------------------------------*/
void cat_ioSubtitleNotify(sdword value)
{
    if (value != 0)
    {
        catIoSubtitleOffset = value;
    }
    catIoSubtitleTarget = 0;
    catIoSubtitleTime = cat_taskRealTimeElapsed;
}

/*-----------------------------------------------------------------------------
    Name        : ioUpdateShipTotals                                0x004ab650
    Description : CAT: count the selection per ship type into the overlay's lists: own working ships and other
                  players' ships as the player sees them (a mimic as its disguise; one posing as a resource is left
                  out) in the main list, own disabled ships in the disabled lists; then the hover state.
----------------------------------------------------------------------------*/
void ioUpdateShipTotals(void)
{
    sdword index;
    Ship *ship;
    ShipType type;

    for (index = 0; index < TOTAL_NUM_SHIPS; index++)
    {
        catIoShips[index].present = catIoShips[index].hover = FALSE;
        catIoShips[index].count = 0;
        catIoDisabled[index].present = catIoDisabled[index].hover = FALSE;
        catIoDisabled[index].count = 0;
        catIoDisabledExempt[index].present = catIoDisabledExempt[index].hover = FALSE;
        catIoDisabledExempt[index].count = 0;
    }
    for (index = 0; index < selSelected.numShips; index++)
    {
        ship = selSelected.ShipPtr[index];
        if (ship->playerowner == universe.curPlayerPtr)
        {
            if (!ship->shipOrDerelict.catDisabled)
            {
                catIoShips[ship->shiptype].present = TRUE;
                catIoShips[ship->shiptype].count++;
            }
            else if (!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20))
            {
                catIoDisabled[ship->shiptype].present = TRUE;
                catIoDisabled[ship->shiptype].count++;
            }
            else
            {
                catIoDisabledExempt[ship->shiptype].present = TRUE;
                catIoDisabledExempt[ship->shiptype].count++;
            }
        }
        else if (!bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic))
        {
            catIoShips[ship->shiptype].present = TRUE;
            catIoShips[ship->shiptype].count++;
        }
        else if (cat_mimicAppearsAlliedTo(ship, universe.curPlayerPtr) ||
                 (cat_shipIsMimicking(ship) && (sdword)cat_mimicGetApparentShipType(ship) >= 0))
        {
            type = cat_mimicGetApparentShipType(ship);
            catIoShips[type].present = TRUE;
            catIoShips[cat_mimicGetApparentShipType(ship)].count++;
        }
        else if (!cat_mimicAppearsAsResource(ship, universe.curPlayerPtr))
        {
            catIoShips[ship->shiptype].present = TRUE;
            catIoShips[ship->shiptype].count++;
        }
    }
    cat_ioUpdateHover();
}

/*-----------------------------------------------------------------------------
    Name        : cat_ioRemoveShipReferences                        0x004ab800
    Description : A dying ship's messages lose their reference (univRemoveShipReferences).
----------------------------------------------------------------------------*/
void cat_ioRemoveShipReferences(Ship *ship)
{
    sdword index;

    for (index = 0; index < catIoMessageCount; index++)
    {
        if (catIoMessages[index].type == 1 && catIoMessages[index].data == ship)
        {
            catIoMessages[index].type = 0;
            catIoMessages[index].data = NULL;
        }
    }
}

/*=============================================================================
    The objectives
=============================================================================*/
void cat_ioObjectiveAdd(void *objective)                                    // 0x004ac5e0
{
    catIoObjectives[catIoObjectiveCount].objective = (Objective *)objective;
    catIoObjectiveCount++;
}

void cat_ioObjectiveAddFirst(void *objective)                               // 0x004ac600
{
    sdword index;

    for (index = catIoObjectiveCount; index > 0; index--)
    {
        catIoObjectives[index].objective = catIoObjectives[index - 1].objective;
    }
    catIoObjectives[0].objective = (Objective *)objective;
    catIoObjectiveCount++;
}

void cat_ioObjectiveRemove(void *objective)                                 // 0x004ac650
{
    sdword index, move;

    for (index = 0; index < catIoObjectiveCount; index++)
    {
        if (catIoObjectives[index].objective == objective)
        {
            for (move = index; move < catIoObjectiveCount - 1; move++)
            {
                catIoObjectives[move].objective = catIoObjectives[move + 1].objective;
            }
            catIoObjectiveCount--;
        }
    }
}

void cat_ioObjectivesClear(void)                                            // 0x004ac6a0
{
    catIoObjectiveCount = 0;
}

/*=============================================================================
    The message scroller
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : cat_ioMessageScroll                               0x004ac6b0
    Description : Drop the oldest message.
----------------------------------------------------------------------------*/
void cat_ioMessageScroll(void)
{
    sdword index;

    for (index = 0; index < catIoMessageCount - 1; index++)
    {
        strcpy(catIoMessages[index].text, catIoMessages[index + 1].text);
        catIoMessages[index].type = catIoMessages[index + 1].type;
        catIoMessages[index].data = catIoMessages[index + 1].data;
        catIoMessages[index].shownTime = 0;
        catIoMessages[index].colour = catIoMessages[index + 1].colour;
    }
    catIoMessageCount--;
}

// CAT: the shared body of cat_ioMessageAdd 0x004ac940 and clCommandMessage 0x004ac820: the line goes in after the
//      newest; the view follows it when it showed the newest; a line other than "saved game" (strSavedGame) sounds the
//      message event (soundEventPlay(NULL, 0x12a))
static void catIoMessageInsert(char *text, udword colour)
{
    sdword shown;

    if (catIoMessageCount > 24)
    {
        cat_ioMessageScroll();
    }
    strncpy(catIoMessages[catIoMessageCount].text, text, CAT_IO_MESSAGE_LENGTH - 1);
    catIoMessages[catIoMessageCount].text[CAT_IO_MESSAGE_LENGTH - 1] = '\0';
    catIoMessages[catIoMessageCount].shownTime = 0;
    catIoMessages[catIoMessageCount].colour = colour;
    shown = catIoMessageCount;
    if (catIoMessageLast == catIoMessageCount - 1)
    {
        shown = catIoMessagesShown;
    }
    catIoMessagesShown = shown + 1;
    if (catIoMessagesShown > 5)
    {
        catIoMessagesShown = 5;
    }
    catIoMessageLast = catIoMessageCount;
    catIoMessageCount++;
    if (strcmp(strGetString(strSavedGame), text) != 0)
    {
        soundEventPlay(NULL, 0x12a, NULL);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_ioMessageAdd                                  0x004ac940
    Description : A game message, with what it refers to (type 1: the ship in data; type 2: the research).
----------------------------------------------------------------------------*/
void cat_ioMessageAdd(char *text, udword colour, udword type, void *data)
{
    LockMutex(gMessageMutex);
    catIoMessageInsert(text, colour);
    UnLockMutex(gMessageMutex);
    // the exe stores the reference after the lock, in the new count's previous slot
    catIoMessages[catIoMessageCount - 1].type = type;
    catIoMessages[catIoMessageCount - 1].data = data;
}

/*-----------------------------------------------------------------------------
    Name        : cat_ioCommandMessage
    Description : CAT: clCommandMessage 0x004ac820's body (CommandWrap.c calls it): a message with no reference.
----------------------------------------------------------------------------*/
void cat_ioCommandMessage(char *text, udword colour)
{
    LockMutex(gMessageMutex);
    catIoMessageInsert(text, colour);
    UnLockMutex(gMessageMutex);
}

/*-----------------------------------------------------------------------------
    Name        : cat_ioFocusLastMessageShip                        0x004ac740
    Description : CAT (M2-R6 mainrgn-popup): Focus_On_LastShipToSpeak: the newest shown message line about a ship
                  selects it and focuses on it (the sensors manager culls and focuses). The export does not list
                  the function (the key-command table's 0x3D row points at it); name chosen here.
----------------------------------------------------------------------------*/
void cat_ioFocusLastMessageShip(void)
{
    FocusCommand focus;
    sdword line, index;
    Ship *ship;

    for (line = 0; line < catIoMessagesShown; line++)
    {
        index = catIoMessageCount - line - 1;
        if ((index < 0) || (catIoMessages[index].type != 1))
        {
            continue;
        }
        ship = (Ship *)catIoMessages[index].data;
        if (ship != NULL)
        {
            selSelected.numShips = 1;
            selSelected.ShipPtr[0] = ship;
            selSelecting.numTargets = 1;
            selSelecting.TargetPtr[0] = (SpaceObjRotImpTarg *)ship;
            ioUpdateShipTotals();
            soundEventPlay(NULL, 0x108, NULL);
            if (!smSensorsActive)
            {
                focus.numShips = 1;
                focus.ShipPtr[0] = ship;
                soundEventPlay(NULL, 0x137, NULL);
                ccFocus(&universe.mainCameraCommand, &focus);
            }
            else
            {
                smCullAndFocusSelecting();
            }
        }
        return;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_ioCycleMode                                   0x004acb40
    Description : The overlay modes 0 -> 1 -> 2 (or back).
----------------------------------------------------------------------------*/
void cat_ioCycleMode(bool32 forward)
{
    catMoMode += forward ? 1 : -1;
    if (catMoMode < 0)
    {
        catMoMode = 2;
    }
    else if (catMoMode > 2)
    {
        catMoMode = 0;
    }
}

/*=============================================================================
    The mission timer (shown under the support units)
=============================================================================*/
void cat_missionTimerReset(void)                                            // 0x004acb80
{
    catMissionTimerStart = universe.totaltimeelapsed;
}

// the timer's start goes into the save game as its raw 32-bit pattern (SaveInfoNumber of the dword at 0x00a4b8b4)
void cat_missionTimerSave(void)                                             // 0x004acbb0
{
    sdword bits;

    memcpy(&bits, &catMissionTimerStart, sizeof(bits));
    SaveInfoNumber(bits);
}

void cat_missionTimerLoad(void)                                             // 0x004acbc0
{
    sdword bits = LoadInfoNumber();

    memcpy(&catMissionTimerStart, &bits, sizeof(bits));
}

/*=============================================================================
    The main overlay
=============================================================================*/
// CAT: cat_moAltPrintOptionsActive 0x004aa080: an overlay mode other than the full one
bool8 cat_moAltPrintOptionsActive(void)
{
    return (catMoMode != 0);
}

/*-----------------------------------------------------------------------------
    Name        : cat_moPrintWithBox                                0x004a6af0
    Description : Print a line and grow the element's backdrop around it (drawn the next frame).
----------------------------------------------------------------------------*/
static void cat_moPrintWithBox(rectangle *box, sdword x, sdword y, color c, char *text)
{
    if (x - CAT_MO_BOX_PAD < box->x0)
    {
        box->x0 = x - CAT_MO_BOX_PAD;
    }
    if (box->x1 < fontWidth(text) + CAT_MO_BOX_PAD + x)
    {
        box->x1 = fontWidth(text) + CAT_MO_BOX_PAD + x;
    }
    if (y - CAT_MO_BOX_PAD < box->y0)
    {
        box->y0 = y - CAT_MO_BOX_PAD;
    }
    if (box->y1 < y + fontHeight(text) + CAT_MO_BOX_PAD)
    {
        box->y1 = y + fontHeight(text) + CAT_MO_BOX_PAD;
    }
    glDisable(GL_BLEND);
    fontPrint(x, y, c, text);
    glEnable(GL_BLEND);
}

// CAT: the selection list lines (cat_moSelectedDraw): "<count> x <type>", the special single-player names
static void catMoSelectedLine(CatIoShipEntry *entry, ShipType type, color normal, sdword *y, sdword x, bool32 named)
{
    CatMoOptions *options = catMoOptions();
    char text[256];
    color c;

    c = catMoListPressedColour;
    if (!entry->pressed)
    {
        c = entry->hover ? catMoListHoverColour : normal;
    }
    sprintf(text, "%i x %s", entry->count, ShipTypeToNiceStr(type));
    if (named && singlePlayerGame)
    {   // the named ships of missions 2 (kStandardDestroyer, the Bushan Ray) and 5 / 6 (kCarrier, the Cal Shtow)
        if (singlePlayerGameInfo.currentMission == 2 && type == kStandardDestroyer)
        {
            sprintf(text, "1 x %s", strGetString(strBushanRay));
        }
        else if ((singlePlayerGameInfo.currentMission == 5 || singlePlayerGameInfo.currentMission == 6) && type == kCarrier)
        {
            sprintf(text, "1 x %s", strGetString(strCalShtow));
        }
    }
    if (!options->selected.box)
    {
        fontPrint(x, *y, c, text);
    }
    else
    {
        cat_moPrintWithBox(&catMoSelectedBox, x, *y, c, text);
        catMoSelectedBoxUsed = TRUE;
    }
    catHudCounts.selectedLines++;                           // CAT: port diagnostic (the HUD self-test)
    entry->rect.x0 = x;
    entry->rect.y0 = *y;
    entry->rect.x1 = CAT_MO_LIST_WIDTH + x;
    entry->rect.y1 = *y + fontHeight(" ");
    *y += fontHeight(" ");
}

/*-----------------------------------------------------------------------------
    Name        : cat_moSelectedDraw                                0x004a6bb0
    Description : The selection list top right, one line per type (main list, disabled, disabled exempt).
----------------------------------------------------------------------------*/
void cat_moSelectedDraw(void)
{
    fonthandle font = fontMakeCurrent(catMoShipListFont);
    CatMoOptions *options = catMoOptions();
    sdword x = MAIN_WindowWidth - CAT_MO_LIST_WIDTH;
    sdword y = CAT_MO_LIST_TOP + catIoSubtitleOffset;
    ShipType type;

    if (options->selected.show)
    {
        if (options->selected.box && catMoSelectedBoxUsed)
        {
            primRectTranslucent2(&catMoSelectedBox, CAT_MO_TRANSLUCENT);
        }
        if (options->selected.box)
        {   // the backdrop starts empty at the list's corner and grows with the lines
            catMoSelectedBox.x0 = catMoSelectedBox.x1 = x;
            catMoSelectedBox.y0 = catMoSelectedBox.y1 = y;
        }
        catMoSelectedBoxUsed = FALSE;
        for (type = 0; type < TOTAL_NUM_SHIPS; type++)
        {
            if (catIoShips[type].present == TRUE)
            {
                catMoSelectedLine(&catIoShips[type], type, catMoListColour, &y, x, FALSE);
            }
            if (catIoDisabled[type].present == TRUE)
            {
                catMoSelectedLine(&catIoDisabled[type], type, catMoListDisabledColour, &y, x, FALSE);
            }
            if (catIoDisabledExempt[type].present)
            {
                catMoSelectedLine(&catIoDisabledExempt[type], type, catMoListExemptColour, &y, x, TRUE);
            }
        }
    }
    fontMakeCurrent(font);
}

// CAT: one line of the band-box list (cat_moSelectingDraw)
static void catMoSelectingLine(char *text, color c, sdword x, sdword *y)
{
    if (!catMoOptions()->selecting.box)
    {
        fontPrint(x, *y, c, text);
    }
    else
    {
        cat_moPrintWithBox(&catMoSelectingBox, x, *y, c, text);
        catMoSelectingBoxUsed = TRUE;
    }
    catHudCounts.selectingLines++;                          // CAT: port diagnostic (the HUD self-test)
    *y += fontHeight(" ");
}

/*-----------------------------------------------------------------------------
    Name        : cat_moSelectingDraw                               0x004a6ff0
    Description : Under the selection list, what the band box holds: own ships (and allied-looking mimics) green,
                  allies' yellow, enemies' red (as the player sees them), then the mines.
----------------------------------------------------------------------------*/
void cat_moSelectingDraw(void)
{
    fonthandle font = fontMakeCurrent(catMoShipListFont);
    CatMoOptions *options = catMoOptions();
    sdword x = MAIN_WindowWidth - CAT_MO_LIST_WIDTH;
    sdword y, lines, index, mines;
    sdword own[TOTAL_NUM_SHIPS], allied[TOTAL_NUM_SHIPS], enemy[TOTAL_NUM_SHIPS];
    bool32 anyOwn, anyAllied, anyEnemy;
    char text[256];
    Ship *ship;
    ShipType type;
    color c;

    if (options->selecting.show)
    {
        memset(own, 0, sizeof(own));
        memset(allied, 0, sizeof(allied));
        memset(enemy, 0, sizeof(enemy));
        lines = 0;
        for (index = 0; index < TOTAL_NUM_SHIPS; index++)
        {
            lines += (catIoShips[index].present == 1) + (catIoDisabled[index].present == 1) +
                     (catIoDisabledExempt[index].present == 1);
        }
        y = CAT_MO_LIST_TOP + catIoSubtitleOffset + fontHeight(" ") * lines + CAT_MO_SELECTING_GAP;
        if (options->selecting.box && catMoSelectingBoxUsed)
        {
            primRectTranslucent2(&catMoSelectingBox, CAT_MO_TRANSLUCENT);
        }
        if (options->selecting.box)
        {
            catMoSelectingBox.x0 = catMoSelectingBox.x1 = x;
            catMoSelectingBox.y0 = catMoSelectingBox.y1 = y;
        }
        catMoSelectingBoxUsed = FALSE;
        anyOwn = anyAllied = anyEnemy = FALSE;
        for (index = 0; index < selSelecting.numTargets; index++)
        {
            ship = (Ship *)selSelecting.TargetPtr[index];
            if (ship->header.objtype != OBJ_ShipType)
            {
                continue;
            }
            if (ship->playerowner == universe.curPlayerPtr)
            {
                if (!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20))
                {
                    own[ship->shiptype]++;
                    anyOwn = TRUE;
                }
                else
                {
                    allied[ship->shiptype]++;
                    anyAllied = TRUE;
                }
            }
            else if (bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic) &&
                     cat_mimicAppearsAlliedTo(ship, universe.curPlayerPtr))
            {
                own[cat_mimicGetApparentShipType(ship)]++;
                anyOwn = TRUE;
            }
        }
        if (anyOwn)
        {
            for (type = 0; type < TOTAL_NUM_SHIPS; type++)
            {
                if (own[type] != 0)
                {
                    sprintf(text, "%i x %s", own[type], ShipTypeToNiceStr(type));
                    catMoSelectingLine(text, catMoSelectingOwnColour, x, &y);
                }
            }
        }
        for (index = 0; index < selSelecting.numTargets; index++)
        {
            ship = (Ship *)selSelecting.TargetPtr[index];
            if (ship->header.objtype == OBJ_ShipType && ship->playerowner != universe.curPlayerPtr &&
                allianceArePlayersAllied(ship->playerowner, universe.curPlayerPtr))
            {
                allied[ship->shiptype]++;
                anyAllied = TRUE;
            }
        }
        if (anyAllied)
        {
            c = catMoSelectingAlliedColour;
            for (type = 0; type < TOTAL_NUM_SHIPS; type++)
            {
                if (allied[type] != 0)
                {
                    sprintf(text, "%i x %s", allied[type], ShipTypeToNiceStr(type));
                    if (singlePlayerGame &&
                        ((singlePlayerGameInfo.currentMission == 2 && type == kStandardDestroyer) ||
                         ((singlePlayerGameInfo.currentMission == 5 || singlePlayerGameInfo.currentMission == 6) &&
                          type == kCarrier)))
                    {   // the exe prints the Cal Shtow's name for both (and turns the rest of the list blue)
                        c = 0xffff0000;
                        sprintf(text, "1 x %s", strGetString(strCalShtow));
                    }
                    catMoSelectingLine(text, c, x, &y);
                }
            }
        }
        if (selSelecting.numTargets > 0)
        {
            anyEnemy = FALSE;
            for (index = 0; index < selSelecting.numTargets; index++)
            {
                ship = (Ship *)selSelecting.TargetPtr[index];
                if (ship->header.objtype != OBJ_ShipType || ship->playerowner == universe.curPlayerPtr ||
                    allianceArePlayersAllied(ship->playerowner, universe.curPlayerPtr))
                {
                    continue;
                }
                if (!bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic))
                {
                    enemy[ship->shiptype]++;
                }
                else if (cat_mimicAppearsAlliedTo(ship, universe.curPlayerPtr) ||
                         cat_mimicAppearsAsResource(ship, universe.curPlayerPtr) ||
                         cat_mimicAppearsAsDerelict(ship, universe.curPlayerPtr))
                {
                    continue;
                }
                else if (cat_shipIsMimicking(ship) && (sdword)cat_mimicGetApparentShipType(ship) >= 0)
                {
                    enemy[cat_mimicGetApparentShipType(ship)]++;
                }
                else
                {
                    enemy[ship->shiptype]++;
                }
                anyEnemy = TRUE;
            }
            if (anyEnemy)
            {
                for (type = 0; type < TOTAL_NUM_SHIPS; type++)
                {
                    if (enemy[type] != 0)
                    {
                        sprintf(text, "%i x %s", enemy[type], ShipTypeToNiceStr(type));
                        catMoSelectingLine(text, catMoSelectingEnemyColour, x, &y);
                    }
                }
            }
            // the mines in the band box
            mines = 0;
            for (index = 0; index < selSelecting.numTargets; index++)
            {
                if (selSelecting.TargetPtr[index]->header.objtype == OBJ_MissileType &&
                    ((Missile *)selSelecting.TargetPtr[index])->missileType == MISSILE_Mine)
                {
                    mines++;
                }
            }
            if (mines != 0)
            {
                sprintf(text, "%i x %s", mines, strGetString(strMO_Tooltip_Mine));
                catMoSelectingLine(text, 0xffffffff, x, &y);
            }
        }
    }
    fontMakeCurrent(font);
}

/*-----------------------------------------------------------------------------
    Name        : cat_moObjectivesDraw                              0x004a76b0
    Description : The primary objectives over the bottom panel: complete green, the hovered one white, else red.
----------------------------------------------------------------------------*/
void cat_moObjectivesDraw(void)
{
    fonthandle font = fontMakeCurrent(catMoObjectivesFont);
    CatMoOptions *options = catMoOptions();
    sdword y = catMoBottomY;
    sdword x = CAT_MO_MARGIN;
    sdword index;
    Objective *objective;
    char text[256];
    color c;

    if (options->objectives.show)
    {
        // CAT: the tutorial's objective pointer points at the list as last drawn (cat_tutObjPointerXYSet 0x00531290)
        cat_tutObjPointerXYSet(catMoObjectivesRight, catMoObjectivesBox.y0);
        if (catIoObjectiveCount != 0 && options->objectives.box && (!catMoTaskbarBackdrop || tutorial == 1))
        {
            catMoObjectivesBox.y0 = catMoBottomY - CAT_MO_OBJ_BOX_PAD;
            catMoObjectivesBox.x0 = 0;
            catMoObjectivesBox.y1 = MAIN_WindowHeight;
            catMoObjectivesBox.x1 = catMoObjectivesRight;
            primRectTranslucent2(&catMoObjectivesBox, CAT_MO_TRANSLUCENT);
        }
        catMoObjectivesRight = 0;
        if (catIoObjectiveCount != 0)
        {
            sprintf(text, "%s", strGetString(strMO_PrimaryObjectives));
            fontPrint(x, y, catMoObjTitleColour, text);
            if (catMoObjectivesRight < fontWidth(text) + CAT_MO_MARGIN)
            {
                catMoObjectivesRight = fontWidth(text) + CAT_MO_MARGIN;
            }
            y += CAT_MO_OBJ_GAP + fontHeight(" ");
        }
        for (index = 0; index < catIoObjectiveCount; index++)
        {
            objective = catIoObjectives[index].objective;
            if (objective->primary == 1)
            {
                c = catMoObjCompleteColour;
                if (objective->status != 1)
                {
                    c = (catIoObjectives[index].hover == 1) ? catMoObjHoverColour : catMoObjIncompleteColour;
                }
                if (strcasecmp(objective->label, "hyperspace") == 0)
                {
                    // CAT: the hyperspace objective's row is "next level" or "hyperspace" (cat_spMissionFlagValue 0x0051c120)
                    sprintf(text, "%s", cat_spMissionFlagValue());
                }
                else
                {
                    sprintf(text, "%s - %s", strGetString(objective->status ? strMO_ObjectiveStatus_Complete :
                                                          strMO_ObjectiveStatus_Incomplete),
                            objective->description != NULL ? objective->description : "");
                }
                catIoObjectives[index].rect.x0 = x;
                catIoObjectives[index].rect.y0 = y;
                catIoObjectives[index].rect.x1 = fontWidth(text) + x;
                catIoObjectives[index].rect.y1 = fontHeight(" ") + y;
                fontPrint(x, y, c, text);
                if (catMoObjectivesRight < fontWidth(text) + CAT_MO_MARGIN)
                {
                    catMoObjectivesRight = fontWidth(text) + CAT_MO_MARGIN;
                }
                y += fontHeight(" ");
            }
        }
        catMoObjectivesRight += CAT_MO_OBJ_RIGHT_PAD;
    }
    fontMakeCurrent(font);
}

// CAT: cat_moClickedFlagConsume 0x004a7980 / cat_moMouseOverObjective 0x004a7990
sdword cat_moClickedFlagConsume(void)
{
    sdword value = catIoHoverHyperspace;

    catIoHoverHyperspace = FALSE;
    return value;
}

sdword cat_moMouseOverObjective(void)
{
    sdword index, over = 0;

    for (index = 0; index < catIoObjectiveCount; index++)
    {
        if (catIoObjectives[index].rect.x0 <= mouseCursorXPosition && catIoObjectives[index].rect.y0 <= mouseCursorYPosition &&
            mouseCursorXPosition < catIoObjectives[index].rect.x1 && mouseCursorYPosition < catIoObjectives[index].rect.y1)
        {
            over = 1;
        }
    }
    return over;
}

/*-----------------------------------------------------------------------------
    Name        : cat_moTooltipText
    Description : The tooltip's text and colours for the object under the cursor (obj, classified as
                  mouseCursorOverShipType does: overType). HWI: the exe's 0x004a79d0 builds them inline in the draw;
                  the recreation builds them here so HWI-TEST /catTooltipAt can log the text without a draw.
----------------------------------------------------------------------------*/
void cat_moTooltipText(SpaceObj *obj, sdword overType, char *text, color *outlineOut, color *textOut, color *boxOut)
{
    Ship *ship = (Ship *)obj;
    color outline = catMoTipOwnOutline, textColour = catMoTipOwnText, box = catMoTipOwnBox;

    text[0] = '\0';
    if (overType == -2)
    {   // a resource, cloud or missile
        outline = catMoTipObjOutline; textColour = catMoTipObjText; box = catMoTipObjBox;
        text[0] = '\0';
        if (obj->header.objtype == OBJ_AsteroidType)
        {   // the exe adds a fixed amount by asteroid type (2: 320, 3: 1600, 4: 5920) to the resource value
            sdword amount = ((Resource *)obj)->resourcevalue;
            switch (((Asteroid *)obj)->asteroidtype)
            {
                case 2: amount += 0x140; break;
                case 3: amount += 0x640; break;
                case 4: amount += 0x1720; break;
                default: break;
            }
            sprintf(text, "%s (%d)", strGetString(strMO_Tooltip_Resource), amount);
        }
        else if (obj->header.objtype == OBJ_NebulaType)
        {
            sprintf(text, "%s (%d)", strGetString(strMO_Tooltip_Nebula), ((Resource *)obj)->resourcevalue);
        }
        else if (obj->header.objtype == OBJ_GasType)
        {
            sprintf(text, "%s (%d)", strGetString(strMO_Tooltip_GasCloud), ((Resource *)obj)->resourcevalue);
        }
        else if (obj->header.objtype == OBJ_DustType)
        {
            sprintf(text, "%s (%d)", strGetString(strMO_Tooltip_DustCloud), ((Resource *)obj)->resourcevalue);
        }
        else if (obj->header.objtype == OBJ_MissileType)
        {
            outline = catMoTipMissileOutline; textColour = catMoTipMissileText; box = catMoTipMissileBox;
            if (((Missile *)obj)->missileType == MISSILE_Regular)
            {
                strcpy(text, strGetString(strMO_Tooltip_Missile));
            }
            else if (((Missile *)obj)->missileType == MISSILE_Mine)
            {
                strcpy(text, strGetString(strMO_Tooltip_Mine));
            }
        }
        else
        {
            strcpy(text, strGetString(strMO_Tooltip_Resource));
        }
    }
    else if (overType == -1 && !bitTest(obj->header.flags, SOF_Hide))
    {   // an enemy ship, as the player sees it
        outline = catMoTipEnemyOutline; textColour = catMoTipEnemyText; box = catMoTipEnemyBox;
        if (!bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic))
        {
            if (!singlePlayerGame)
            {
                sprintf(text, "%s (%s)", ShipTypeToNiceStr(ship->shiptype), playerNames[ship->playerowner->playerIndex]);
            }
            else
            {
                sprintf(text, "%s", ShipTypeToNiceStr(ship->shiptype));
            }
        }
        else if (cat_mimicAppearsAlliedTo(ship, universe.curPlayerPtr))
        {
            sprintf(text, "%s", ShipTypeToNiceStr(cat_mimicGetApparentShipType(ship)));
            outline = catMoTipOwnOutline; textColour = catMoTipOwnText; box = catMoTipOwnBox;
        }
        else if (!cat_mimicAppearsAsDerelict(ship, universe.curPlayerPtr) &&
                 cat_mimicAppearsAsResource(ship, universe.curPlayerPtr))
        {
            sprintf(text, "%s (%d)", strGetString(strMO_Tooltip_Resource), cat_mimicGetApparentObject(ship));
            outline = catMoTipObjOutline; textColour = catMoTipObjText; box = catMoTipObjBox;
        }
        else if (!cat_mimicAppearsAsDerelict(ship, universe.curPlayerPtr) &&
                 (!cat_shipIsMimicking(ship) || (sdword)cat_mimicGetApparentShipType(ship) < 0))
        {
            sprintf(text, "%s", ShipTypeToNiceStr(ship->shiptype));
        }
        else
        {
            sprintf(text, "%s", ShipTypeToNiceStr(cat_mimicGetApparentShipType(ship)));
        }
    }
    else if (overType == -4)
    {   // an allied ship
        if (!singlePlayerGame)
        {
            sprintf(text, "%s (%s)", ShipTypeToNiceStr(ship->shiptype), playerNames[ship->playerowner->playerIndex]);
        }
        else
        {
            sprintf(text, "%s", ShipTypeToNiceStr(ship->shiptype));
        }
        outline = catMoTipAllyOutline; textColour = catMoTipAllyText; box = catMoTipAllyBox;
    }
    else
    {   // an own ship (its name when it has one), a crystal, a hidden enemy
        if (!cat_shipIsCrystal(ship))
        {
            if (!cat_shipNamesShipIsNameable(ship) || bitTest(ship->header.flags, SOF_Dead))
            {
                sprintf(text, "%s", ShipTypeToNiceStr(ship->shiptype));
            }
            else if (!singlePlayerGame || universe.curPlayerPtr->playerIndex == 0)
            {
                sprintf(text, "%s (%s)", ShipTypeToNiceStr(ship->shiptype), cat_shipNamesGetName(ship));
            }
            else
            {
                sprintf(text, "dbgAssert()");
            }
            if (singlePlayerGame && ((singlePlayerGameInfo.currentMission == 2 && ship->shiptype == kStandardDestroyer) ||
                ((singlePlayerGameInfo.currentMission == 5 || singlePlayerGameInfo.currentMission == 6) &&
                 ship->shiptype == kCarrier)))
            {
                sprintf(text, "%s", strGetString(strCalShtow));
            }
            outline = catMoTipOwnOutline; textColour = catMoTipOwnText; box = catMoTipOwnBox;
            if (bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20))
            {
                outline = catMoTipAllyOutline; textColour = catMoTipAllyText; box = catMoTipAllyBox;
            }
        }
        else
        {   // HWI-FIX (M3-R4 crystal-tooltip): a crystal's own resource units (CrystalSpec.resourceValue, Ship+0x638: what
            //      the exe prints, 0x004a7dfd mov ecx,[eax+0x638]). The port printed the mimic's apparent asteroid value
            //      (cat_mimicGetApparentObject 0x005b9940: Ship+0x72c behind the mimic flag, 0 for any other ship), so
            //      every crystal read "(0)" while it paid its 6000 / 10000 RUs when dissolved.
            sprintf(text, "%s (%d)", ShipTypeToNiceStr(ship->shiptype), (int)((CrystalSpec *)ship->ShipSpecifics)->resourceValue);
            outline = catMoTipObjOutline; textColour = catMoTipObjText; box = catMoTipObjBox;
        }
    }
    *outlineOut = outline;
    *textOut = textColour;
    *boxOut = box;
}

/*-----------------------------------------------------------------------------
    Name        : cat_moTooltipDraw                                 0x004a79d0
    Description : The tooltip under the cursor after the delay: the object's name (and resource amount), in the
                  colours of its kind (resource, missile / mine, enemy, ally, own).
----------------------------------------------------------------------------*/
void cat_moTooltipDraw(void)
{
    fonthandle font = fontMakeCurrent(catMoTooltipFont);
    SpaceObj *obj = mouseCursorObjPtr;
    rectangle rect;
    char text[256];
    color outline, textColour, box;
    sdword x, y;

    // the exe returns before restoring the font
    if (catMrRightMode != 0 || catIoHover || CAT_POPUP_MENU_OPEN() || obj == NULL || mrDisabled)
    {
        return;
    }
    if (catMoTooltipObject != obj)
    {
        catMoTooltipObject = obj;
        catMoTooltipTime = catMoTooltipDelay + cat_taskRealTimeElapsed;
    }
    if (cat_taskRealTimeElapsed < catMoTooltipTime)
    {
        return;
    }
    if (!catMoOptions()->tooltips.show || mouseCursorOverShipType == -3)
    {
        fontMakeCurrent(font);
        return;
    }
    x = CAT_MO_TOOLTIP_DX + mouseCursorXPosition;
    y = CAT_MO_TOOLTIP_DY + mouseCursorYPosition;
    cat_moTooltipText(obj, mouseCursorOverShipType, text, &outline, &textColour, &box);
    rect.x0 = x - 5;
    rect.y0 = y - 5;
    rect.x1 = fontWidth(text) + 5 + x;
    rect.y1 = fontHeight(" ") + 5 + y;
    primRectTranslucent2(&rect, box);
    primRoundRectOutline(&rect, CAT_MO_TOOLTIP_THICK, outline, CAT_MO_TOOLTIP_ROUND, CAT_MO_TOOLTIP_ROUND);
    fontPrint(x, y, textColour, text);
    fontMakeCurrent(font);
}

/*-----------------------------------------------------------------------------
    Name        : cat_moSupportUnitsDraw                            0x004a7fe0
    Description : Bottom right: the support units (green / yellow / red by use, blinking over the cap), the
                  resource units and the mission timer.
----------------------------------------------------------------------------*/
void cat_moSupportUnitsDraw(void)
{
    fonthandle font = fontMakeCurrent(catMoSupportUnitsFont);
    Player *player = universe.curPlayerPtr;
    CatMoOptions *options = catMoOptions();
    sdword y = catMoBottomY;
    sdword x = MAIN_WindowWidth - CAT_MO_SUPPORT_RIGHT;
    sdword labelWidth, valueWidth, seconds;
    double percent;
    char text[256];
    color c;
    real32 t;

    sprintf(text, "%s 000 / 000", strGetString(strMO_SupportUnits));
    x -= fontWidth(text);
    if (options->playerInfo.show)
    {
        if (options->playerInfo.box && (!catMoTaskbarBackdrop || tutorial == 1))
        {
            catMoSupportBox.x0 = x - CAT_MO_SUPPORT_PAD;
            catMoSupportBox.x1 = MAIN_WindowWidth;
            catMoSupportBox.y0 = catMoBottomY - CAT_MO_SUPPORT_PAD;
            catMoSupportBox.y1 = MAIN_WindowHeight;
            primRectTranslucent2(&catMoSupportBox, CAT_MO_TRANSLUCENT);
        }
        if (player != NULL)
        {
            sprintf(text, "%s ", strGetString(strMO_SupportUnits));
            fontPrint(x, y, catMoSupportLabelColour, text);
            labelWidth = fontWidth(text);
            percent = ((double)(udword)player->supportUnitsUsed / (double)player->supportUnitsMax) * 100.0;
            c = (percent <= CAT_MO_SUPPORT_LOW) ? catMoSupportLowColour :
                (percent <= CAT_MO_SUPPORT_HIGH) ? catMoSupportMidColour : catMoSupportHighColour;
            if (percent > 100.0)
            {
                if (cat_taskRealTimeElapsed > CAT_MO_SUPPORT_BLINK + catMoSupportBlinkTime ||
                    catMoSupportBlinkTime > cat_taskRealTimeElapsed)
                {
                    catMoSupportBlinkTime = cat_taskRealTimeElapsed;
                }
                t = (cat_taskRealTimeElapsed - catMoSupportBlinkTime) / CAT_MO_SUPPORT_BLINK;
                t = (t <= 0.5f) ? t + t : (real32)(((double)t - 1.0) * -2.0);
                c = colBlend(catMoSupportHighColour, catMoSupportBlinkColour, t);
            }
            sprintf(text, "%d", player->supportUnitsUsed);
            fontPrint(labelWidth + x, y, c, text);
            valueWidth = fontWidth(text);
            if (percent > 100.0)                            // HWI-TEST (M3-R3 pause-freeze): where the blink is drawn
            {                                               // HWI-TEST
                catHudSupportBlinkRect.x0 = labelWidth + x; // HWI-TEST
                catHudSupportBlinkRect.y0 = y;              // HWI-TEST
                catHudSupportBlinkRect.x1 = labelWidth + x + valueWidth;   // HWI-TEST
                catHudSupportBlinkRect.y1 = y + fontHeight(text);          // HWI-TEST
            }                                               // HWI-TEST
            sprintf(text, " / %d", player->supportUnitsMax);
            fontPrint(labelWidth + x + valueWidth, y, catMoSupportValueColour, text);
            y += fontHeight("");
            sprintf(text, "%s ", strGetString(strMO_ResourceUnits));
            fontPrint(x, y, catMoSupportLabelColour, text);
            labelWidth = fontWidth(text);
            sprintf(text, "%d", player->resourceUnits);
            fontPrint(labelWidth + x, y, catMoSupportValueColour, text);
            y += fontHeight(" ");
        }
        y += CAT_MO_INFO_GAP;
        seconds = (sdword)lrint((double)(universe.totaltimeelapsed - catMissionTimerStart));
        sprintf(text, "%02d:%02d:%02d", seconds / 60 / 60, (seconds / 60) % 60, seconds % 60);
        fontPrint(x, y, catMoTimerColour, text);
        catHudCounts.supportUnits++;                        // CAT: port diagnostic (the HUD self-test)
    }
    fontMakeCurrent(font);
}

// CAT: a taskbar button (cat_gameStatsButtonDraw 0x00489470, the stats screen's button drawer): the three-part
//      background, the mouse-over parts, the centred text
static void catMoTaskbarButtonDraw(rectangle *rect, char *text, fonthandle font)
{
    bool32 over = (rect->x0 <= mouseCursorXPosition && rect->y0 <= mouseCursorYPosition &&
                   mouseCursorXPosition < rect->x1 && mouseCursorYPosition < rect->y1);

    cat_gameStatsButtonDraw(rect, text, 0xffffffff, font, CAT_MO_TB_TEXT,
                            CAT_OI_Stats_Exit_Button_Left, CAT_OI_Stats_Exit_Button_Left_Mouse,
                            CAT_OI_Stats_Exit_Button_Mid, CAT_OI_Stats_Exit_Button_Mid_Mouse,
                            CAT_OI_Stats_Exit_Button_Right, CAT_OI_Stats_Exit_Button_Right_Mouse, TRUE);
    (void)over;
}

/*-----------------------------------------------------------------------------
    Name        : cat_moObjectivesFrameDraw                         0x004a83b0
    Description : The taskbar buttons over the bottom panel (with the taskbar-buttons option): sensors / main view,
                  build, research, systems.
----------------------------------------------------------------------------*/
void cat_moObjectivesFrameDraw(void)
{
    fonthandle font = fontMakeCurrent(catMoObjectivesFont);
    sdword x, index;
    char text[256];

    if (catMoTaskbarButtons && tutorial != 1)
    {
        x = MAIN_WindowWidth / 2 - (CAT_MO_TB_GAP + CAT_MO_TB_WIDTH * 2) / 2;
        if (singlePlayerGame && catMoOptions()->objectives.show)
        {
            for (index = 0; index < catIoObjectiveCount; index++)
            {
                sprintf(text, "INCOMPLETE - %s", catIoObjectives[index].objective->description);
                if (x < fontWidth(text) + CAT_MO_MARGIN)
                {
                    x = fontWidth(text) + 0x14 + CAT_MO_MARGIN;
                }
            }
        }
        catMoButtonSensors.x0 = x;
        catMoButtonSensors.x1 = CAT_MO_TB_WIDTH + x;
        catMoButtonSensors.y0 = catMoBottomY + CAT_MO_TB_TOP;
        catMoButtonSensors.y1 = CAT_MO_TB_HEIGHT + catMoButtonSensors.y0;
        catMoTaskbarButtonDraw(&catMoButtonSensors, strGetString(smSensorsActive ? str_MOMainViewButton : str_MOSensorsButton),
                               catMoMessagesFont);
        catMoButtonBuild.x0 = x;
        catMoButtonBuild.x1 = CAT_MO_TB_WIDTH + x;
        catMoButtonBuild.y0 = CAT_MO_TB_ROW_GAP + catMoButtonSensors.y1;
        catMoButtonBuild.y1 = catMoButtonBuild.y0 + CAT_MO_TB_HEIGHT;
        catMoTaskbarButtonDraw(&catMoButtonBuild, strGetString(str_MOBuildButton), catMoMessagesFont);
        catMoButtonResearch.x0 = catMoButtonSensors.x1 + CAT_MO_TB_GAP;
        catMoButtonResearch.x1 = catMoButtonResearch.x0 + CAT_MO_TB_WIDTH;
        catMoButtonResearch.y0 = catMoButtonSensors.y0;
        catMoButtonResearch.y1 = catMoButtonSensors.y1;
        catMoTaskbarButtonDraw(&catMoButtonResearch, strGetString(str_MOResearchButton), catMoMessagesFont);
        catMoButtonSystems.x0 = catMoButtonBuild.x1 + CAT_MO_TB_GAP;
        catMoButtonSystems.x1 = catMoButtonSystems.x0 + CAT_MO_TB_WIDTH;
        catMoButtonSystems.y0 = catMoButtonBuild.y0;
        catMoButtonSystems.y1 = catMoButtonBuild.y1;
        catMoTaskbarButtonDraw(&catMoButtonSystems, strGetString(str_MOSystemsButton), catMoMessagesFont);
    }
    fontMakeCurrent(font);
}

/*-----------------------------------------------------------------------------
    Name        : cat_moGameMessagesDraw                            0x004a8650
    Description : The message scroller bottom left: the newest lines up from the bottom panel, the older ones darker,
                  the newest fading out after 8 s; the scroll buttons (Buffer_Up / Buffer_Down icons).
----------------------------------------------------------------------------*/
void cat_moGameMessagesDraw(void)
{
    fonthandle font = fontMakeCurrent(catMoMessagesFont), oldFont;
    CatMoOptions *options = catMoOptions();
    sdword y, index, line, size;
    real32 shade, age, fade;
    CatIoMessage *message;
    color c;

    y = (catMoBottomTop > MAIN_WindowHeight) ? MAIN_WindowHeight : catMoBottomTop;
    y -= fontHeight("");
    if (catIoMessageLast == catIoMessageCount - 1 || catIoMessageCount == 0 || !options->gameMessage.show)
    {
        catIoMessageUpShown = FALSE;
    }
    else
    {   // the button to scroll back to the newest
        catIoMessageUpRect.x1 = CAT_MO_MESSAGES_X - 3;
        catIoMessageUpShown = TRUE;
        catIoMessageUpRect.x0 = 3;
        catIoMessageUpRect.y0 = y - fontHeight("");
        catIoMessageUpRect.y1 = catIoMessageUpRect.y0 - 3 + CAT_MO_MESSAGES_X;
        size = catIoMessageUpRect.x1 - catIoMessageUpRect.x0;
        if (mouseCursorXPosition < catIoMessageUpRect.x0 || mouseCursorYPosition < catIoMessageUpRect.y0 ||
            catIoMessageUpRect.x1 <= mouseCursorXPosition || catIoMessageUpRect.y1 <= mouseCursorYPosition)
        {
            cat_overlayIconDraw(CAT_OI_Buffer_Down, catIoMessageUpRect.y0, catIoMessageUpRect.x0, size,
                                catIoMessageUpRect.y1 - catIoMessageUpRect.y0, 0xffffffff);
        }
        else
        {
            cat_overlayIconDraw(CAT_OI_Buffer_Down_Mouse, catIoMessageUpRect.y0, catIoMessageUpRect.x0, size,
                                catIoMessageUpRect.y1 - catIoMessageUpRect.y0, 0xffffffff);
        }
    }
    if (catIoMessageLast - catIoMessagesShown >= 0 && catIoMessageCount != 0 && options->gameMessage.show)
    {   // the button to scroll back to older lines, above the shown ones
        sdword up;
        catIoMessageDownRect.x1 = CAT_MO_MESSAGES_X - 3;
        catIoMessageDownShown = TRUE;
        catIoMessageDownRect.x0 = 3;
        up = (sdword)lrint((double)fontHeight("") * -0.5);
        oldFont = fontMakeCurrent(catMoMessagesFont);
        size = fontHeight("") * 5 + CAT_MO_MESSAGES_GAP;
        fontMakeCurrent(oldFont);
        catIoMessageDownRect.y0 = (up - size) + y;
        catIoMessageDownRect.y1 = catIoMessageDownRect.y0 - 3 + CAT_MO_MESSAGES_X;
        if (mouseCursorXPosition < catIoMessageDownRect.x0 || mouseCursorYPosition < catIoMessageDownRect.y0 ||
            catIoMessageDownRect.x1 <= mouseCursorXPosition || catIoMessageDownRect.y1 <= mouseCursorYPosition)
        {
            cat_overlayIconDraw(CAT_OI_Buffer_Up_Mouse, catIoMessageDownRect.y0, catIoMessageDownRect.x0,
                                catIoMessageDownRect.x1 - catIoMessageDownRect.x0,
                                catIoMessageDownRect.y1 - catIoMessageDownRect.y0, 0xffffffff);
        }
        else
        {
            cat_overlayIconDraw(CAT_OI_Buffer_Up, catIoMessageDownRect.y0, catIoMessageDownRect.x0,
                                catIoMessageDownRect.x1 - catIoMessageDownRect.x0,
                                catIoMessageDownRect.y1 - catIoMessageDownRect.y0, 0xffffffff);
        }
    }
    else
    {
        catIoMessageDownShown = FALSE;
    }
    LockMutex(gMessageMutex);
    line = 0;
    for (index = catIoMessageLast; catIoMessageLast - catIoMessagesShown < index; index--, line++)
    {
        message = &catIoMessages[index];
        message->rect.x0 = CAT_MO_MESSAGES_X;
        message->rect.x1 = fontWidth(message->text) + message->rect.x0;
        message->rect.y0 = y - fontHeight("");
        message->rect.y1 = y;
        if (options->gameMessage.show)
        {
            if (!message->hover)
            {
                shade = 0.0f;
                switch (line)
                {
                    case 0: shade = 1.0f; break;
                    case 1: shade = 0.8f; break;
                    case 2: shade = 0.7f; break;
                    case 3: shade = 0.6f; break;
                    case 4: shade = 0.5f; break;
                    default: break;
                }
                c = colBlend(message->colour, 0xff000000, shade);
            }
            else
            {
                c = (message->colour == catMoMessageShipColour) ? 0xffffffff : catMoMessageShipColour;
            }
            if (catIoMessageLast == catIoMessageCount - 1 && index == (catIoMessageLast - catIoMessagesShown) + 1)
            {   // the oldest line shown fades out 8 s after its first draw, over 5 s, then goes
                if ((real32)message->shownTime == 0.0f)
                {
                    message->shownTime = (sdword)cat_taskRealTimeElapsed;
                }
                age = cat_taskRealTimeElapsed - (real32)message->shownTime;
                fade = 0.0f;
                if (age > CAT_MO_MESSAGE_FADE_AFTER)
                {
                    fade = (age - CAT_MO_MESSAGE_FADE_AFTER) / CAT_MO_MESSAGE_FADE_TIME;
                    if (fade > 1.0f)
                    {
                        fade = 1.0f;
                    }
                }
                c = cat_colBlendRGBA(c, catMoMessageFadeColour, 1.0f - fade);
                if (age > CAT_MO_MESSAGE_FADE_TIME + CAT_MO_MESSAGE_FADE_AFTER)
                {
                    catIoMessagesShown--;
                    y -= fontHeight("");
                    continue;
                }
            }
            fontPrint(message->rect.x0, message->rect.y0, c, message->text);
            catHudCounts.messageLines++;                    // CAT: port diagnostic (the HUD self-test)
        }
        y -= fontHeight("");
    }
    fontMakeCurrent(font);
    UnLockMutex(gMessageMutex);
}

/*-----------------------------------------------------------------------------
    Name        : cat_moPlayerNamesDraw                             0x004a8b20
    Description : The players' names top left with their colour swatches (self green, allies blue, enemies red, the
                  dead grey) and in multiplayer their bounties; returns the y under the list.
----------------------------------------------------------------------------*/
sdword cat_moPlayerNamesDraw(sdword top)
{
    fonthandle font = fontMakeCurrent(catMoPlayerNamesFont);
    CatMoOptions *options = catMoOptions();
    sdword x = CAT_MO_NAMES_X;
    sdword y = CAT_MO_NAMES_TOP + top;
    sdword index, lineHeight, half;
    rectangle swatch;
    char text[256], bounty[96];
    color c;
    Player *player;

    if (options->playerNames.show)
    {
        lineHeight = fontHeight(" ");
        if (options->playerNames.box)
        {
            if (catMoPlayerNamesBoxUsed)
            {
                primRectTranslucent2(&catMoPlayerNamesBox, CAT_MO_TRANSLUCENT);
            }
            catMoPlayerNamesBox.x0 = catMoPlayerNamesBox.x1 = x;
            catMoPlayerNamesBox.y0 = catMoPlayerNamesBox.y1 = y;
        }
        catMoPlayerNamesBoxUsed = FALSE;
        half = (lineHeight * 2) / 3 + x;
        for (index = 0; index < universe.numPlayers; index++)
        {
            player = &universe.players[index];
            sprintf(text, "%s..%d", playerNames[index], universe.gameStats.playerStats[index].catStat7D0);
            c = catMoNamesDeadColour;
            if (player->playerState != PLAYER_DEAD)
            {
                c = catMoNamesSelfColour;
                if (sigsPlayerIndex != index)
                {
                    c = allianceArePlayersAllied(&universe.players[sigsPlayerIndex], player) ? catMoNamesAllyColour :
                        catMoNamesEnemyColour;
                }
            }
            swatch.x0 = x;
            swatch.y0 = y;
            swatch.x1 = half;
            swatch.y1 = y + fontHeight(" ");
            rndAdditiveBlends(FALSE);
            primRectSolid2(&swatch, teColorSchemes[index].textureColor.base);
            swatch.x0 = half;
            swatch.x1 = fontHeight(" ") + x;
            primRectSolid2(&swatch, teColorSchemes[index].textureColor.detail);
            if (options->playerNames.box && x - CAT_MO_BOX_PAD < catMoPlayerNamesBox.x0)
            {
                catMoPlayerNamesBox.x0 = x - CAT_MO_BOX_PAD;
            }
            if (!options->playerNames.box)
            {
                fontPrint(CAT_MO_NAMES_TEXT_GAP + swatch.x1, y, c, text);
            }
            else
            {
                cat_moPrintWithBox(&catMoPlayerNamesBox, CAT_MO_NAMES_TEXT_GAP + swatch.x1, y, c, text);
                catMoPlayerNamesBoxUsed = TRUE;
            }
            if (!singlePlayerGame && tpGameCreated.bountySize != 3)
            {
                sprintf(bounty, " ( %d )", getPlayerBountyRender(player) / 100);
                if (!options->playerNames.box)
                {
                    fontPrint(swatch.y1 + fontWidth(text) + CAT_MO_NAMES_TEXT_GAP, y, catMoNamesBountyColour, bounty);
                }
                else
                {
                    cat_moPrintWithBox(&catMoPlayerNamesBox, swatch.y1 + fontWidth(text) + CAT_MO_NAMES_TEXT_GAP, y,
                                       catMoNamesBountyColour, bounty);
                    catMoPlayerNamesBoxUsed = TRUE;
                }
            }
            y += fontHeight(" ") + CAT_MO_NAMES_GAP;
        }
    }
    fontMakeCurrent(font);
    return y;
}

/*-----------------------------------------------------------------------------
    Name        : cat_moBoxLayout                                   0x004a90f0
    Description : The triangle of a scroll button in its rectangle: 0 points down, 1 points up.
----------------------------------------------------------------------------*/
static void cat_moBoxLayout(sdword direction, rectangle *rect, triangle *tri)
{
    if (direction == 0)
    {
        tri->x0 = rect->x0 + CAT_MO_TRI_PAD;
        tri->y0 = rect->y1 - CAT_MO_TRI_PAD;
        tri->x1 = (rect->x1 - rect->x0) / 2 + rect->x0;
        tri->y1 = rect->y0 + CAT_MO_TRI_PAD;
        tri->x2 = rect->x1 - CAT_MO_TRI_PAD;
        tri->y2 = rect->y1 - CAT_MO_TRI_PAD;
    }
    else if (direction == 1)
    {
        tri->x0 = rect->x0 + CAT_MO_TRI_PAD;
        tri->y0 = CAT_MO_TRI_PAD + rect->y0;
        tri->x1 = rect->x1 - CAT_MO_TRI_PAD;
        tri->y1 = CAT_MO_TRI_PAD + rect->y0;
        tri->x2 = (rect->x1 - rect->x0) / 2 + rect->x0;
        tri->y2 = rect->y1 - CAT_MO_TRI_PAD;
    }
}

// CAT: the pages of the construction / research lists (the shared code of cat_moConstructionDraw and
//      cat_moResearchDraw): how many lines fit between top and the bottom limit, the page clamped, the scroll buttons
static void catMoListPages(sdword count, sdword top, sdword bottom, sdword *page, CatMoScrollButton *up,
                           CatMoScrollButton *down, sdword *first, sdword *last)
{
    sdword lineHeight = fontHeight(" ");
    sdword limit = bottom - CAT_MO_SCROLL_HEIGHT;
    sdword pages, perPage;
    double avail;

    *first = 0;
    *last = count;
    if (lineHeight * count + top > limit)
    {
        avail = (double)(limit - top);
        pages = (sdword)ceil((double)(fontHeight(" ") * count) / avail);
        perPage = (sdword)floor(avail / (double)fontHeight(" "));
        if (perPage * pages > perPage + count)
        {
            pages--;
        }
        if (*page == 0)
        {
            up->shown = FALSE;
            down->shown = TRUE;
        }
        else if (*page < pages - 1)
        {
            up->shown = TRUE;
            down->shown = TRUE;
        }
        else
        {
            *page = pages - 1;
            up->shown = TRUE;
            down->shown = FALSE;
        }
        *first = *page * perPage;
        *last = perPage + *first;
        if (*last > count)
        {
            *last = count;
        }
        up->rect.x0 = CAT_MO_SCROLL_X;
        up->rect.y0 = top - CAT_MO_SCROLL_HEIGHT;
        up->rect.x1 = CAT_MO_SCROLL_WIDTH + CAT_MO_SCROLL_X;
        up->rect.y1 = top;
        down->rect.x0 = CAT_MO_SCROLL_X;
        down->rect.y0 = bottom - CAT_MO_SCROLL_HEIGHT;
        down->rect.x1 = CAT_MO_SCROLL_WIDTH + CAT_MO_SCROLL_X;
        down->rect.y1 = bottom;
    }
    else
    {
        up->shown = FALSE;
        down->shown = FALSE;
    }
}

// CAT: the scroll buttons' triangles (and the backdrop grown over them)
static void catMoScrollButtonsDraw(CatMoScrollButton *up, CatMoScrollButton *down, bool32 boxed, rectangle *box)
{
    triangle tri;
    CatMoScrollButton *buttons[2] = { up, down };
    sdword index;

    for (index = 0; index < 2; index++)
    {
        CatMoScrollButton *button = buttons[index];
        if (button->shown)
        {
            cat_moBoxLayout(button->direction, &button->rect, &tri);
            if (button->hover)
            {
                primTriSolid2(&tri, catMoTriHoverColour);
            }
            primTriOutline2(&tri, 1, catMoTriOutlineColour);
            if (boxed)
            {
                if (button->direction == 0)
                {
                    if (button->rect.y0 - CAT_MO_BOX_PAD < box->y0)
                    {
                        box->y0 = button->rect.y0 - CAT_MO_BOX_PAD;
                    }
                }
                else if (button->direction == 1 && box->y1 < button->rect.y1 + CAT_MO_BOX_PAD)
                {
                    box->y1 = button->rect.y1 + CAT_MO_BOX_PAD;
                }
            }
        }
    }
}

// CAT: a progress bar: the bar icon over the full width, the unfinished part shaded
static void catMoProgressBarDraw(sdword icon, sdword x, sdword y0, sdword y1, real32 done)
{
    rectangle shade;

    shade.x0 = (sdword)(done * (real32)CAT_MO_BAR_WIDTH) + x;
    shade.y0 = y0;
    shade.x1 = CAT_MO_BAR_WIDTH + x;
    shade.y1 = y1;
    glDisable(GL_BLEND);
    cat_overlayIconDraw(icon, y0, x, CAT_MO_BAR_WIDTH, y1 - y0, 0xffffffff);
    glEnable(GL_BLEND);
    rndAdditiveBlends(TRUE);
    primRectTranslucent2(&shade, catMoBarShadeColour);      // CAT: rgl.dll's software device drew it solid black
}

/*-----------------------------------------------------------------------------
    Name        : cat_moConstructionDraw                            0x004a91c0
    Description : The ships in construction under the player names: per type a progress bar of the first factory
                  that builds it and "<type> (<jobs>/<jobs>..)" per factory; paged with scroll buttons.
----------------------------------------------------------------------------*/
void cat_moConstructionDraw(sdword top, sdword bottom)
{
    fonthandle font = fontMakeCurrent(catMoConstructionFont);
    CatMoOptions *options = catMoOptions();
    sdword x = CAT_MO_CONS_X;
    sdword y = CAT_MO_SCROLL_HEIGHT + CAT_MO_CONS_TOP + top;
    CatCmJob jobs[TOTAL_NUM_SHIPS];
    sdword count, first, last, index, slot, used, slots[4], lineHeight;
    char text[256];

    if (!options->construction.show)
    {
        catMoConsUp.shown = FALSE;
        catMoConsDown.shown = FALSE;
    }
    else
    {
        if (options->construction.box)
        {
            if (catMoConsBoxUsed)
            {
                primRectTranslucent2(&catMoConsBox, CAT_MO_TRANSLUCENT);
            }
            catMoConsBox.x0 = catMoConsBox.x1 = x;
            catMoConsBox.y0 = catMoConsBox.y1 = y;
        }
        catMoConsBoxUsed = FALSE;
        count = cat_cmCountJobsForPlayer(universe.curPlayerPtr, jobs);
        catMoListPages(count, y, bottom, &catMoConsPage, &catMoConsUp, &catMoConsDown, &first, &last);
        for (index = first; index < last; index++)
        {
            lineHeight = fontHeight(" ");
            // the mothership's factory is slot 0; a player whose "mothership" is of another class builds in slot 1
            slot = (universe.curPlayerPtr->PlayerMothership != NULL &&
                    universe.curPlayerPtr->PlayerMothership->staticinfo->shipclass != CLASS_Mothership) ? 1 : 0;
            if (jobs[index].nJobs[slot] == 0)
            {
                continue;
            }
            catMoProgressBarDraw(CAT_OI_Build_ProgressBar, x, y - 1, y - 1 + lineHeight,
                                 (real32)(jobs[index].totalTime - jobs[index].timeLeft[slot]) / (real32)jobs[index].totalTime);
            if (options->construction.box && x + CAT_MO_BAR_WIDTH < CAT_MO_BOX_PAD + x + CAT_MO_BAR_WIDTH)
            {
                catMoConsBox.x1 = CAT_MO_BOX_PAD + x + CAT_MO_BAR_WIDTH;
            }
            used = 0;
            for (slot = 0; slot < 4; slot++)
            {
                if (jobs[index].nJobs[slot] != 0)
                {
                    slots[used++] = slot;
                }
            }
            if (used > 0)
            {
                char *name = ShipTypeToNiceStr(jobs[index].type);
                switch (used)
                {
                    case 1: sprintf(text, "%s (%d)", name, jobs[index].nJobs[slots[0]]); break;
                    case 2: sprintf(text, "%s (%d/%d)", name, jobs[index].nJobs[slots[0]], jobs[index].nJobs[slots[1]]); break;
                    case 3: sprintf(text, "%s (%d/%d/%d)", name, jobs[index].nJobs[slots[0]], jobs[index].nJobs[slots[1]],
                                    jobs[index].nJobs[slots[2]]); break;
                    default: sprintf(text, "%s (%d/%d/%d/%d)", name, jobs[index].nJobs[slots[0]], jobs[index].nJobs[slots[1]],
                                     jobs[index].nJobs[slots[2]], jobs[index].nJobs[slots[3]]); break;
                }
                if (!options->construction.box)
                {
                    fontPrint(x, y, catMoConsColour, text);
                }
                else
                {
                    cat_moPrintWithBox(&catMoConsBox, x, y, catMoConsColour, text);
                    catMoConsBoxUsed = TRUE;
                }
            }
            y += fontHeight(" ");
        }
        glEnable(GL_BLEND);
        rndAdditiveBlends(TRUE);
        catMoScrollButtonsDraw(&catMoConsUp, &catMoConsDown, options->construction.box, &catMoConsBox);
    }
    if (!catMoConsUp.shown)
    {
        catMoConsUp.hover = FALSE;
    }
    if (!catMoConsDown.shown)
    {
        catMoConsDown.hover = FALSE;
    }
    fontMakeCurrent(font);
}

/*-----------------------------------------------------------------------------
    Name        : cat_moResearchDraw                                0x004a9a00
    Description : The research topics under the construction list: a progress bar and the technology's name each.
----------------------------------------------------------------------------*/
void cat_moResearchDraw(sdword top, sdword bottom)
{
    fonthandle font = fontMakeCurrent(catMoResearchFont);
    CatMoOptions *options = catMoOptions();
    sdword x = CAT_MO_RESEARCH_X;
    sdword y = CAT_MO_SCROLL_HEIGHT + CAT_MO_RESEARCH_TOP + top;
    CatRmTopic topics[NumTechnologies];
    sdword count, first, last, index, lineHeight;
    char text[256];

    if (!options->research.show)
    {
        catMoResearchUp.shown = FALSE;
        catMoResearchDown.shown = FALSE;
        catMoResearchUp.hover = FALSE;
    }
    else
    {
        if (options->research.box)
        {
            if (catMoResearchBoxUsed)
            {
                primRectTranslucent2(&catMoResearchBox, CAT_MO_TRANSLUCENT);
            }
            catMoResearchBox.x0 = catMoResearchBox.x1 = x;
            catMoResearchBox.y0 = catMoResearchBox.y1 = y;
        }
        catMoResearchBoxUsed = FALSE;
        count = cat_rmCollectTopicsByLab(universe.curPlayerPtr, topics);
        catMoListPages(count, y, bottom, &catMoResearchPage, &catMoResearchUp, &catMoResearchDown, &first, &last);
        for (index = first; index < last; index++)
        {
            lineHeight = fontHeight(" ");
            catMoProgressBarDraw(CAT_OI_Research_ProgressBar, x, y - 1, y + lineHeight - 4,
                                 ((real32)topics[index].totalTime - topics[index].timeLeft) / (real32)topics[index].totalTime);
            if (options->research.box && catMoResearchBox.x1 < CAT_MO_BAR_WIDTH + x + CAT_MO_BOX_PAD)
            {
                catMoResearchBox.x1 = CAT_MO_BAR_WIDTH + x + CAT_MO_BOX_PAD;
            }
            sprintf(text, "%s", strGetString(strTechOffset + topics[index].tech));
            if (!options->research.box)
            {
                fontPrint(x, y, catMoResearchColour, text);
            }
            else
            {
                cat_moPrintWithBox(&catMoResearchBox, x, y, catMoResearchColour, text);
                catMoResearchBoxUsed = TRUE;
            }
            y += fontHeight(" ");
        }
        catMoScrollButtonsDraw(&catMoResearchUp, &catMoResearchDown, options->research.box, &catMoResearchBox);
        if (!catMoResearchUp.shown)
        {
            catMoResearchUp.hover = FALSE;
        }
    }
    if (!catMoResearchDown.shown)
    {
        catMoResearchDown.hover = FALSE;
    }
    fontMakeCurrent(font);
}

/*-----------------------------------------------------------------------------
    Name        : cat_ioMessageScrollUp / cat_ioMessageScrollDown   0x004aca90 / 0x004acad0
    Description : CAT (M2-R6 startup-keys): the game messages' scroll keys (PageDown_GameChat / PageUp_GameChat), the
                  scroll buttons' code of cat_ioClick: an older line / back towards the newest (all lines shown again).
----------------------------------------------------------------------------*/
void cat_ioMessageScrollUp(void)
{
    if (!catIoMessageUpShown)
    {
        return;
    }
    soundEventPlay(NULL, 0x121, NULL);
    catIoMessageLast++;
    if (catIoMessageLast >= catIoMessageCount)
    {
        catIoMessageLast = catIoMessageCount - 1;
    }
}

void cat_ioMessageScrollDown(void)
{
    sdword index;

    if (!catIoMessageDownShown)
    {
        return;
    }
    soundEventPlay(NULL, 0x122, NULL);
    if (catIoMessagesShown == 5)
    {
        catIoMessageLast--;
        if (catIoMessageLast < 0)
        {
            catIoMessageLast = 0;
        }
    }
    catIoMessagesShown = catIoMessageCount + 1;
    if (catIoMessagesShown > 5)
    {
        catIoMessagesShown = 5;
    }
    for (index = 0; index < catIoMessageCount; index++)
    {
        catIoMessages[index].shownTime = 0;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_moPrintBoxesToggle                            0x004acb90
    Description : CAT (M2-R6 startup-keys): Change_Overlay_Backdrops_Toggle: the print option keys switch the
                  elements' backdrops instead of the elements (and back).
----------------------------------------------------------------------------*/
void cat_moPrintBoxesToggle(void)
{
    catMoPrintBoxes = !catMoPrintBoxes;
}

/*-----------------------------------------------------------------------------
    Name        : cat_ioMultipleSelectRelease                       0x004ac4e0
    Description : CAT (M2-R6 startup-keys): Multiple_Select released: when ship types were picked in the overlay's
                  lists while it was held, only the selected ships of the picked types stay selected (the own ships by
                  their list: active, disabled, disabled exempt); the picks are cleared.
----------------------------------------------------------------------------*/
void cat_ioMultipleSelectRelease(void)
{
    sdword index;
    bool32 picked = FALSE;
    Ship *ship;

    for (index = 0; index < TOTAL_NUM_SHIPS; index++)
    {
        if ((catIoShips[index].pressed == 1) || (catIoDisabled[index].pressed == 1) ||
            (catIoDisabledExempt[index].pressed == 1))
        {
            picked = TRUE;
        }
    }
    if (picked)
    {
        index = 0;
        while (index < selSelected.numShips)
        {
            ship = selSelected.ShipPtr[index];
            if (((ship->shipOrDerelict.catDisabled == 0) && !catIoShips[ship->shiptype].pressed) ||
                ((ship->shipOrDerelict.catDisabled == 1) &&
                 (bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20) ?
                      !catIoDisabledExempt[ship->shiptype].pressed : !catIoDisabled[ship->shiptype].pressed)))
            {
                selSelectionRemoveSingleShip(&selSelected, ship);
                index = 0;
                continue;
            }
            index++;
        }
        ioUpdateShipTotals();
    }
    for (index = 0; index < TOTAL_NUM_SHIPS; index++)
    {
        catIoShips[index].pressed = FALSE;
        catIoDisabled[index].pressed = FALSE;
        catIoDisabledExempt[index].pressed = FALSE;
    }
}

// the debug-key lists (0x0072add0 .. 0x0072aebc): per list up to five runs of commands {count, first[5], last[5]}
typedef struct
{
    sdword count;
    sdword first[5];
    sdword last[5];
} CatMoDebugKeyList;
static const CatMoDebugKeyList catMoDebugKeyLists[5] =
{
    { 3, {   1,  44,  49, 0, 0 }, {  16,  48,  58, 0, 0 } },     // 0x0072add0: actions, tactics, formations
    { 1, {  17,   0,   0, 0, 0 }, {  43,   0,   0, 0, 0 } },     // 0x0072ae00: special actions
    { 3, {  59,  72,  79, 0, 0 }, {  71,  78,  85, 0, 0 } },     // 0x0072ae30: camera, selecting, views
    { 2, {  86, 139,   0, 0, 0 }, { 107, 150,   0, 0, 0 } },     // 0x0072ae60: misc, sensors view
    { 1, { 108,   0,   0, 0, 0 }, { 138,   0,   0, 0, 0 } },     // 0x0072ae90: groupings
};
static const color catMoDebugKeysOnColour = 0xff0000ff;        // 0x0072acc0
static const color catMoDebugKeysBack = 0xd2050505;           // 0x0072acc4
static const color catMoDebugKeyNameColour = 0xffd2d2d2;      // 0x0072acc8
static const color catMoDebugKeyNoneColour = 0xff0000ff;      // 0x0072accc
static const color catMoDebugKeyGroupColour = 0xffffffff;     // 0x0072acd0
static const color catMoDebugKeyBindingColours[3] = { 0xff00ff00, 0xff00ffff, 0xffff0000 };   // 0x0072acd4..
#define CAT_MO_DEBUG_KEYS_TOP       10                          // 0x0072ad74 (also the left column's x)

/*-----------------------------------------------------------------------------
    Name        : cat_moDebugKeyLineDraw                            0x004aa0a0
    Description : CAT (M2-R6 startup-keys): one column of the debug-key lists: per run its group header, then each
                  command's name, ":  " and its keys (each binding in its colour, " , " between them; "NONE").
----------------------------------------------------------------------------*/
static void cat_moDebugKeyLineDraw(const CatMoDebugKeyList *list, sdword x0)
{
    sdword run, command, x, y = CAT_MO_DEBUG_KEYS_TOP, index;
    CatKbCommandBindings bindings;
    char text[0x100];

    for (run = 0; run < list->count; run++)
    {
        command = list->first[run];
        if (run != 0)
        {
            y += fontHeight("") * 2;
        }
        fontPrintf(x0, y, catMoDebugKeyGroupColour, cat_moDebugKeyValue(cat_moDebugKeyGet(command)));
        y += 2 + fontHeight("");
        for (; command <= list->last[run]; command++)
        {
            fontPrintf(x0, y, catMoDebugKeyNameColour, cat_kbCommandNameGet(command));
            x = x0 + fontWidth(cat_kbCommandNameGet(command));
            fontPrintf(x, y, catMoDebugKeyNameColour, ":  ");
            x += fontWidth(":  ");
            cat_kbBindingGet(command, &bindings);
            if (bindings.count == 0)
            {
                fontPrintf(x, y, catMoDebugKeyNoneColour, "%s", strGetString(strNoKeyBound));
            }
            else
            {
                for (index = 0; index < bindings.count; index++)
                {
                    cat_kbBindingToString(&bindings.binding[index], text, TRUE);
                    fontPrintf(x, y, catMoDebugKeyBindingColours[index], "%s", text);
                    x += fontWidth(text);
                    if (index + 1 < bindings.count)
                    {
                        fontPrint(x, y, catMoDebugKeyNameColour, " , ");
                        x += fontWidth(" , ");
                    }
                }
            }
            y += fontHeight("");
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_moDebugKeysDraw                               0x004aa2a0
    Description : With a Show_Keys key held (commands 100..102, the install binds End, SHIFT+End, SHIFT+CTRL+End),
                  the screen darkened and the lists of key bindings drawn (cat_moDebugKeyLineDraw); the "DEBUG KEYS
                  ON" line's test is an empty stub in the exe (never drawn).
----------------------------------------------------------------------------*/
static void catMoDebugKeysBackground(void)
{
    rectangle rect;

    rect.x0 = 0;
    rect.y0 = 0;
    rect.x1 = MAIN_WindowWidth - 1;
    rect.y1 = MAIN_WindowHeight - 1;
    glEnable(GL_BLEND);                                     // (0x00b3a47c / 0x00b3a4b0 with GL_BLEND)
    rndAdditiveBlends(FALSE);
    primRectTranslucent2(&rect, catMoDebugKeysBack);
    rndAdditiveBlends(FALSE);
    glDisable(GL_BLEND);
}

void cat_moDebugKeysDraw(void)
{
    fonthandle font = fontMakeCurrent(catMoDebugKeysFont);

    if (FALSE)
    {                                                       // nullsubReturnFalse 0x006fe9a0
        fontPrintf(MAIN_WindowWidth - fontWidth("DEBUG KEYS ON") - 4, 2, catMoDebugKeysOnColour, "DEBUG KEYS ON");
    }
    if (CAT_KB_HELD(CAT_KB_ShowKeys1))
    {
        catMoDebugKeysBackground();
        cat_moDebugKeyLineDraw(&catMoDebugKeyLists[0], CAT_MO_DEBUG_KEYS_TOP);
        cat_moDebugKeyLineDraw(&catMoDebugKeyLists[1], MAIN_WindowWidth / 2);
    }
    else if (CAT_KB_HELD(CAT_KB_ShowKeys2))
    {
        catMoDebugKeysBackground();
        cat_moDebugKeyLineDraw(&catMoDebugKeyLists[2], CAT_MO_DEBUG_KEYS_TOP);
        cat_moDebugKeyLineDraw(&catMoDebugKeyLists[3], MAIN_WindowWidth / 2);
    }
    else if (CAT_KB_HELD(CAT_KB_ShowKeys3))
    {
        catMoDebugKeysBackground();
        cat_moDebugKeyLineDraw(&catMoDebugKeyLists[4], CAT_MO_DEBUG_KEYS_TOP);
    }
    fontMakeCurrent(font);
}

/*-----------------------------------------------------------------------------
    Name        : cat_moPrintOptionToggle                           0x004aa4f0
    Description : Keys '1'..'9' switch the overlay elements of the current mode (or their backdrops).
----------------------------------------------------------------------------*/
void cat_moPrintOptionToggle(char key)
{
    CatMoOptions *options = catMoOptions();
    CatMoOption *elements[9] =
    {
        &options->selected, &options->selecting, &options->objectives, &options->tooltips, &options->playerInfo,
        &options->gameMessage, &options->playerNames, &options->construction, &options->research
    };

    if (key < '1' || key > '9')
    {
        return;
    }
    if (!catMoPrintBoxes)
    {
        elements[key - '1']->show = !elements[key - '1']->show;
    }
    else
    {
        elements[key - '1']->box = !elements[key - '1']->box;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_moPrintOptionsDraw                            0x004a5e40
    Description : The print options' help (with the ship-info key held and the info panel off): "(1) .. Enabled".
----------------------------------------------------------------------------*/
void cat_moPrintOptionsDraw(void)
{
    fonthandle font = fontMakeCurrent(catMoPrintOptionsFont);
    CatMoOptions *options = catMoOptions();
    sdword lineHeight = fontHeight(" ");
    rectangle rect;
    char text[256];
    sdword index;
    static const sdword showNames[9] =
    {
        strMO_Option_ShowSelectedShips, strMO_Option_ShowShipsWithinBandbox, strMO_Option_ShowObjectives,
        strMO_Option_ShowTooltips, strMO_Option_ShowPlayerInfo, strMO_Option_ShowLastCommand,
        strMO_Option_ShowPlayerNames, strMO_Option_ShowConstruction, strMO_Option_ShowResearch
    };
    static const sdword boxNames[9] =
    {
        strMO_Option_ShowSelectedShipsBackdrop, strMO_Option_ShowShipsWithinBandboxBackdrop,
        strMO_Option_ShowObjectivesBackdrop, strMO_Option_ShowTooltipsBackdrop, strMO_Option_ShowPlayerInfoBackdrop,
        strMO_Option_ShowLastCommandBackdrop, strMO_Option_ShowPlayerNamesBackdrop,
        strMO_Option_ShowConstructionBackdrop, strMO_Option_ShowResearchBackdrop
    };
    CatMoOption *elements[9] =
    {
        &options->selected, &options->selecting, &options->objectives, &options->tooltips, &options->playerInfo,
        &options->gameMessage, &options->playerNames, &options->construction, &options->research
    };
    bool32 on;

    rect.x0 = CAT_MO_OPTIONS_X - 10;
    rect.y0 = catIoSubtitleOffset - 10 + CAT_MO_OPTIONS_Y;
    rect.x1 = fontWidth("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA") + 0x1e + CAT_MO_OPTIONS_X;
    rect.y1 = catIoSubtitleOffset + lineHeight * 9 + 0x14 + CAT_MO_OPTIONS_Y;
    primRectSolid2(&rect, catMoOptionsBoxColour);
    primRoundRectOutline(&rect, 2, catMoOptionsOutlineColour, 2, 2);
    for (index = 0; index < 9; index++)
    {
        on = catMoPrintBoxes ? elements[index]->box : elements[index]->show;
        sprintf(text, "(%c) %s %s", '1' + index, strGetString(catMoPrintBoxes ? boxNames[index] : showNames[index]),
                strGetString(on ? strMO_OptionEnabled : strMO_OptionDisabled));
        fontPrint(CAT_MO_OPTIONS_X, catIoSubtitleOffset + lineHeight * index + CAT_MO_OPTIONS_Y,
                  on ? catMoOptionOnColour : catMoOptionOffColour, text);
    }
    fontMakeCurrent(font);
}

/*-----------------------------------------------------------------------------
    Name        : cat_moButtonsDraw                                 0x004aac70
    Description : The sensors manager's camera buttons bottom centre (left, right, up, down; the mouse-over icons,
                  a press latches the button's flag).
----------------------------------------------------------------------------*/
void cat_moButtonsDraw(void)
{
    sdword width, centre, bottom, top, left, right, index;
    char text[256];
    bool32 pressed = keyIsHit(LMOUSE_BUTTON);

    if (!smSensorsActive)
    {
        return;
    }
    width = 0;
    if (singlePlayerGame && catMoOptions()->objectives.show)
    {
        for (index = 0; index < catIoObjectiveCount; index++)
        {
            sprintf(text, "INCOMPLETE - %s", catIoObjectives[index].objective->description);
            if (width < fontWidth(text) + CAT_MO_MARGIN)
            {
                width = fontWidth(text) + 0x14 + CAT_MO_MARGIN;
            }
        }
    }
    centre = MAIN_WindowWidth / 2;
    for (index = 0; index < 4; index++)
    {
        catMoButtonClicked[index] = FALSE;
    }
    left = centre - 0x20;
    bottom = catMoBottomY;
    if ((!catMoTaskbarButtons || tutorial == 1) && width <= centre - 0x40)
    {
        bottom = MAIN_WindowHeight;
    }
    bottom -= CAT_MO_BUTTONS_UP;
    top = bottom - 0x20;
    right = centre + 0x20;
    if ((mouseCursorXPosition < centre - 0x40 || mouseCursorYPosition < top || left <= mouseCursorXPosition ||
         bottom <= mouseCursorYPosition) && !CAT_KB_HELD(0x41))
    {
        cat_overlayIconDraw(CAT_OI_SensorsScroll_Left, top, centre - 0x40, 0x20, 0x20, 0xffffffff);
    }
    else
    {
        cat_overlayIconDraw(CAT_OI_SensorsScroll_Left_Mouse, top, centre - 0x40, 0x20, 0x20, 0xffffffff);
        catMoButtonClicked[0] = pressed;
    }
    if ((mouseCursorXPosition < right || mouseCursorYPosition < top || centre + 0x40 <= mouseCursorXPosition ||
         bottom <= mouseCursorYPosition) && !CAT_KB_HELD(0x42))
    {
        cat_overlayIconDraw(CAT_OI_SensorsScroll_Right, top, right, 0x20, 0x20, 0xffffffff);
    }
    else
    {
        cat_overlayIconDraw(CAT_OI_SensorsScroll_Right_Mouse, top, right, 0x20, 0x20, 0xffffffff);
        catMoButtonClicked[1] = pressed;
    }
    if ((mouseCursorXPosition < left || mouseCursorYPosition < top || right <= mouseCursorXPosition ||
         bottom - 0x10 <= mouseCursorYPosition) && !CAT_KB_HELD(0x43))
    {
        cat_overlayIconDraw(CAT_OI_SensorsScroll_Up, top, left, 0x40, 0x10, 0xffffffff);
    }
    else
    {
        cat_overlayIconDraw(CAT_OI_SensorsScroll_Up_Mouse, top, left, 0x40, 0x10, 0xffffffff);
        catMoButtonClicked[2] = pressed;
    }
    if ((mouseCursorXPosition < left || mouseCursorYPosition < bottom - 0x10 || right <= mouseCursorXPosition ||
         bottom <= mouseCursorYPosition) && !CAT_KB_HELD(0x44))
    {
        cat_overlayIconDraw(CAT_OI_SensorsScroll_Down, bottom - 0x10, left, 0x40, 0x10, 0xffffffff);
    }
    else
    {
        cat_overlayIconDraw(CAT_OI_SensorsScroll_Down_Mouse, bottom - 0x10, left, 0x40, 0x10, 0xffffffff);
        catMoButtonClicked[3] = pressed;
    }
}

// CAT: cat_moButton1Clicked 0x004aaf70 .. cat_moButton4Clicked 0x004aafa0 (left, right, up, down)
sdword cat_moButton1Clicked(void) { return catMoButtonClicked[0]; }
sdword cat_moButton2Clicked(void) { return catMoButtonClicked[1]; }
sdword cat_moButton3Clicked(void) { return catMoButtonClicked[2]; }
sdword cat_moButton4Clicked(void) { return catMoButtonClicked[3]; }

/*-----------------------------------------------------------------------------
    Name        : cat_moRender                                      0x004aafb0
    Description : Draw the main overlay (mrRegionDraw, smViewportRender): the bottom panel's position, then the
                  elements, the print options help, the subtitle offset, the paused / end-of-recording text.
----------------------------------------------------------------------------*/
void cat_moRender(void)
{
    CatMoOptions *options;
    bool32 drawElements = TRUE;
    sdword subtitle = catIoSubtitleOffset;
    sdword lighting, texture, height, bottom, middle, legend, namesBottom;
    fonthandle oldFont;
    rectangle backdrop;
    char *text;

    if (nisFullyScissored && mrDisabled && gameIsRunning)
    {
        drawElements = FALSE;
    }
    if (!(mrRenderMainScreen || smSensorsActive) || !gameIsRunning || mrWhiteOut || nisCaptureCamera || nisFullyScissored)
    {
        return;
    }
    // the HW taskbar: the bottom panel slides in while the mouse is at the bottom edge
    bottom = catMoBottomTop;
    if (catMoHWTaskbar && tutorial != 1)
    {
        if (!catMoBottomShown)
        {
            bottom = catMoBottomY;
            if (catMoBottomY < MAIN_WindowHeight + 0x14)
            {
                bottom = catMoBottomY + CAT_MO_SLIDE_STEP;
            }
        }
        else
        {
            bottom = catMoBottomTop;
        }
    }
    catMoBottomY = bottom;
    fontShadowSet(FS_SE, 0xff000000);
    lighting = rndLightingEnable(FALSE);
    texture = rndTextureEnable(FALSE);
    cat_rndDepthTestEnable(FALSE);
    rndAdditiveBlends(TRUE);
    glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    options = catMoOptions();
    height = 0;
    if (options->objectives.show)
    {
        oldFont = fontMakeCurrent(catMoObjectivesFont);
        height = fontHeight("") * catIoObjectiveCount;
        if (catIoObjectiveCount > 0)
        {
            height += CAT_MO_OBJ_GAP + fontHeight("") + CAT_MO_BOTTOM_GAP;
        }
        fontMakeCurrent(oldFont);
    }
    catMoBottomTop = MAIN_WindowHeight - height;
    if (options->playerInfo.show)
    {
        oldFont = fontMakeCurrent(catMoObjectivesFont);
        height = (MAIN_WindowHeight - CAT_MO_BOTTOM_GAP) - fontHeight("") - CAT_MO_INFO_GAP;
        if (universe.curPlayerPtr != NULL)
        {
            height -= fontHeight("");
        }
        if (height < catMoBottomTop)
        {
            catMoBottomTop = height;
        }
        fontMakeCurrent(oldFont);
    }
    if (MAIN_WindowHeight - catMoBottomTop <= CAT_MO_MIN_BOTTOM)
    {
        catMoBottomTop = MAIN_WindowHeight - CAT_MO_MIN_BOTTOM;
    }
    if (drawElements)
    {
        catHudCounts.frames++;                              // CAT: port diagnostic (the HUD self-test)
        cat_moButtonsDraw();
        if (catMoTaskbarBackdrop && tutorial != 1)
        {
            backdrop.x0 = 0;
            backdrop.y0 = catMoBottomY;
            backdrop.x1 = MAIN_WindowWidth;
            backdrop.y1 = MAIN_WindowHeight;
            primRectTranslucent2(&backdrop, CAT_MO_TRANSLUCENT);
        }
        cat_moSelectedDraw();
        cat_moSelectingDraw();
        cat_moObjectivesDraw();
        cat_moTooltipDraw();
        cat_moGameMessagesDraw();
        namesBottom = cat_moPlayerNamesDraw(subtitle);
        legend = toLegendDraw(namesBottom);
        oldFont = fontMakeCurrent(catMoMessagesFont);
        height = fontHeight("") * 5 + CAT_MO_MESSAGES_GAP;
        fontMakeCurrent(oldFont);
        height = catMoBottomTop + height;            // the exe measures down from the panel top (as written)
        middle = (height - legend) / 2 + legend;
        cat_moConstructionDraw(legend, middle);
        cat_moResearchDraw(middle, height);
        cat_moSupportUnitsDraw();
        cat_moObjectivesFrameDraw();
    }
    if (CAT_KB_HELD(0x62) && !cat_shipPrintActive() && !CAT_CONSOLE_OPEN() && !mrDisabled &&
        piePointSpecMode == PSM_Idle && feStackIndex == 0)
    {
        cat_moPrintOptionsDraw();
    }
    if (catIoSubtitleTarget < catIoSubtitleOffset && tutorial != 1)
    {   // the lists come back up one pixel per real-time unit after the subtitle
        catIoSubtitleOffset -= (sdword)lrint((double)((cat_taskRealTimeElapsed - catIoSubtitleTime) * 1.0f));
        if (catIoSubtitleOffset < 0)
        {
            catIoSubtitleOffset = 0;
        }
    }
    fontShadowSet(FS_NONE, 0xff000000);
    text = strGetString(strPAUSED_TEXT);
    if ((universePause || catMoWideScreen) && catMoMode == 0)
    {
        oldFont = fontMakeCurrent(catMoPausedFont);
        fontPrint((MAIN_WindowWidth - fontWidth(text)) / 2, (sdword)((real32)MAIN_WindowHeight * CAT_MO_PAUSED_Y),
                  catMoPausedColour, text);
        fontMakeCurrent(oldFont);
    }
    text = strGetString(strEndOfRecording);
    if (text == NULL || *text == '\0')
    {
        text = "End of Recording";
    }
    if (playPackets && universePlaybackNoPacket && catMoMode == 0)     // CAT: 0x00a6c5c8, the recorded game ran out (Universe.c)
    {
        oldFont = fontMakeCurrent(catMoPausedFont);
        fontPrint((MAIN_WindowWidth - fontWidth(text)) / 2,
                  (sdword)((real32)fontHeight(text) * 1.5f + (real32)MAIN_WindowHeight * CAT_MO_PAUSED_Y),
                  catMoPausedColour, text);
        fontMakeCurrent(oldFont);
    }
    cat_moDebugKeysDraw();
    glDisable(GL_BLEND);
    rndLightingEnable(lighting);
    rndTextureEnable(texture);
    cat_rndDepthTestEnable(TRUE);
}

/*=============================================================================
    Hover and click
=============================================================================*/
static bool32 catIoInRect(rectangle *rect, sdword x, sdword y)
{
    return (rect->x0 <= x && rect->y0 <= y && x < rect->x1 && y < rect->y1);
}

/*-----------------------------------------------------------------------------
    Name        : cat_ioUpdateHover                                 0x004ac050
    Description : Which overlay element is under the mouse (ioUpdateShipTotals, mrRegionDraw, smViewportRender):
                  the list lines, the objectives, the view buttons, the scroll buttons, the messages.
----------------------------------------------------------------------------*/
void cat_ioUpdateHover(void)
{
    sdword x = mouseCursorXPosition, y = mouseCursorYPosition;
    sdword index;

    catIoHover = FALSE;
    if (piePointSpecMode != PSM_Idle || (cat_shipPrintActive() && CAT_KB_HELD(0x62)) || CAT_CONSOLE_OPEN() ||
        CAT_POPUP_MENU_OPEN() || mrDisabled || catMrSelecting)
    {
        for (index = 0; index < TOTAL_NUM_SHIPS; index++)
        {
            catIoShips[index].hover = catIoDisabled[index].hover = catIoDisabledExempt[index].hover = FALSE;
        }
        for (index = 0; index < catIoObjectiveCount; index++)
        {
            catIoObjectives[index].hover = FALSE;
        }
        for (index = 0; index < 6; index++)
        {
            catMoViewButtons[index].hover = FALSE;
        }
        for (index = 0; index < catIoMessageCount; index++)
        {
            catIoMessages[index].hover = FALSE;
        }
        catIoHover = FALSE;
        return;
    }
    if (catMoHWTaskbar && tutorial != 1)
    {
        if (!catMoBottomShown)
        {
            if (MAIN_WindowHeight - 2 <= y)
            {
                catMoBottomShown = TRUE;
                catMoBottomY = 0;
            }
        }
        else
        {
            catMoBottomShown = (catMoBottomY - 5 <= y);
        }
    }
    for (index = 0; index < TOTAL_NUM_SHIPS; index++)
    {
        if (catIoShips[index].present == 1)
        {
            catIoShips[index].hover = catIoInRect(&catIoShips[index].rect, x, y);
            catIoHover |= catIoShips[index].hover;
        }
        if (catIoDisabled[index].present == 1)
        {
            catIoDisabled[index].hover = catIoInRect(&catIoDisabled[index].rect, x, y);
            catIoHover |= catIoDisabled[index].hover;
        }
        if (catIoDisabledExempt[index].present == 1)
        {
            catIoDisabledExempt[index].hover = catIoInRect(&catIoDisabledExempt[index].rect, x, y);
            catIoHover |= catIoDisabledExempt[index].hover;
        }
    }
    catIoHoverHyperspace = FALSE;
    for (index = 0; index < catIoObjectiveCount; index++)
    {
        if (!catIoInRect(&catIoObjectives[index].rect, x, y))
        {
            catIoObjectives[index].hover = FALSE;
        }
        else
        {
            catIoObjectives[index].hover = TRUE;
            catIoHover = TRUE;
            // the flag is set over an objective other than "hyperspace" / "end mission" (cat_moClickedFlagConsume)
            catIoHoverHyperspace = (strcasecmp(catIoObjectives[index].objective->label, "hyperspace") != 0 &&
                                    strcasecmp(catIoObjectives[index].objective->label, "end mission") != 0);
        }
    }
    for (index = 0; index < 6; index++)
    {
        catMoViewButtons[index].hover = catIoInRect(&catMoViewButtons[index].rect, x, y);
        catIoHover |= catMoViewButtons[index].hover;
    }
    if (catMoConsUp.shown && catIoInRect(&catMoConsUp.rect, x, y))
    {
        catMoConsUp.hover = TRUE;
        catIoHover = TRUE;
    }
    else
    {
        if (catMoConsUp.shown)
        {
            catMoConsUp.hover = FALSE;
        }
        if (catMoConsDown.shown && catIoInRect(&catMoConsDown.rect, x, y))
        {
            catMoConsDown.hover = TRUE;
            catIoHover = TRUE;
        }
        else
        {
            if (catMoConsDown.shown)
            {
                catMoConsDown.hover = FALSE;
            }
            if (catMoResearchUp.shown && catIoInRect(&catMoResearchUp.rect, x, y))
            {
                catMoResearchUp.hover = TRUE;
                catIoHover = TRUE;
            }
            else
            {
                if (catMoResearchUp.shown)
                {
                    catMoResearchUp.hover = FALSE;
                }
                if (catMoResearchDown.shown)
                {
                    catMoResearchDown.hover = catIoInRect(&catMoResearchDown.rect, x, y);
                    catIoHover |= catMoResearchDown.hover;
                }
            }
        }
    }
    for (index = catIoMessageLast; catIoMessageLast - catIoMessagesShown < index; index--)
    {
        catIoMessages[index].hover = FALSE;
        if (catIoMessages[index].type != 0 && catIoInRect(&catIoMessages[index].rect, x, y))
        {
            catIoMessages[index].hover = TRUE;
            catIoHover = TRUE;
        }
    }
    if (catIoInRect(&catIoMessageUpRect, x, y) || catIoInRect(&catIoMessageDownRect, x, y))
    {
        catIoHover = TRUE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_ioClick                                       0x004ab840
    Description : A click on the overlay (cat_mrClickRelease): a list line keeps only that type in the selection (or
                  marks it pressed), an objective opens its intelligence, a view / taskbar button runs its command, the
                  scroll buttons page, a message selects and focuses its ship; TRUE if the overlay took the click.
----------------------------------------------------------------------------*/
bool32 cat_ioClick(bool32 select)
{
    sdword x = mouseCursorXPosition, y = mouseCursorYPosition;
    sdword index, type = -1;
    bool32 inShips = FALSE, inDisabled = FALSE, inExempt = FALSE, taken = FALSE;
    Ship *ship;

    if (piePointSpecMode != PSM_Idle || (cat_shipPrintActive() && CAT_KB_HELD(0x62)) || CAT_CONSOLE_OPEN() ||
        CAT_POPUP_MENU_OPEN() || mrDisabled)
    {
        return FALSE;
    }
    // the list lines: the first type under the mouse
    for (index = 0; index < TOTAL_NUM_SHIPS && type == -1; index++)
    {
        if (catIoShips[index].present == 1 && catIoInRect(&catIoShips[index].rect, x, y))
        {
            inShips = TRUE;
            type = index;
        }
        if (catIoDisabled[index].present == 1 && catIoInRect(&catIoDisabled[index].rect, x, y))
        {
            inDisabled = TRUE;
            type = index;
        }
        if (catIoDisabledExempt[index].present == 1 && catIoInRect(&catIoDisabledExempt[index].rect, x, y))
        {
            inExempt = TRUE;
            type = index;
        }
    }
    if (type != -1)
    {
        if (!select)
        {   // keep only that type (working or disabled as clicked)
            index = 0;
            while (index < selSelected.numShips)
            {
                ship = selSelected.ShipPtr[index];
                if (ship->shiptype == type && (inShips || ship->shipOrDerelict.catDisabled) &&
                    (inDisabled || ship->shipOrDerelict.catDisabled != 1))
                {
                    index++;
                }
                else
                {
                    selSelectionRemoveSingleShip(&selSelected, ship);
                    index = 0;
                }
            }
        }
        else if (inShips)
        {
            catIoShips[type].pressed = TRUE;
        }
        else if (inDisabled)
        {
            catIoDisabled[type].pressed = TRUE;
        }
        else if (inExempt)
        {
            catIoDisabledExempt[type].pressed = TRUE;
        }
        taken = TRUE;
        ioUpdateShipTotals();
        tutGameMessage("ShipListClick");
        if (selSelected.numShips > 0)
        {
            speechEventQueue(selSelected.ShipPtr[0], 0x4000800, selSelected.ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);
        }
    }
    // the objectives
    if (!catMoWideScreen && catMoOptions()->objectives.show && !taken)
    {
        for (index = 0; index < catIoObjectiveCount && !taken; index++)
        {
            if (catIoInRect(&catIoObjectives[index].rect, x, y))
            {
                soundEventPlay(NULL, 0x109, NULL);
                if (strcasecmp(catIoObjectives[index].objective->label, "hyperspace") == 0)
                {
                    CAT_KB_EXECUTE(0, 8);
                }
                else
                {
                    poPopupFleetIntelligence(catIoObjectives[index].objective);
                }
                taken = TRUE;
            }
        }
    }
    // the view buttons
    if (!taken)
    {
        for (index = 0; index < 6; index++)
        {
            if (catIoInRect(&catMoViewButtons[index].rect, x, y))
            {
                CAT_KB_EXECUTE(smSensorsActive, catMoViewButtons[index].command);
            }
        }
    }
    // the construction / research scroll buttons
    if (catMoConsUp.shown && catIoInRect(&catMoConsUp.rect, x, y))
    {
        catMoConsPage--;
        catMoConsUp.hover = TRUE;
        taken = TRUE;
    }
    else
    {
        if (catMoConsUp.shown)
        {
            catMoConsUp.hover = FALSE;
        }
        if (catMoConsDown.shown && catIoInRect(&catMoConsDown.rect, x, y))
        {
            catMoConsPage++;
            catMoConsDown.hover = TRUE;
            taken = TRUE;
        }
        else
        {
            if (catMoConsDown.shown)
            {
                catMoConsDown.hover = FALSE;
            }
            if (catMoResearchUp.shown && catIoInRect(&catMoResearchUp.rect, x, y))
            {
                catMoResearchUp.hover = TRUE;
                catMoResearchPage--;
                taken = TRUE;
            }
            else
            {
                if (catMoResearchUp.shown)
                {
                    catMoResearchUp.hover = FALSE;
                }
                if (catMoResearchDown.shown)
                {
                    if (catIoInRect(&catMoResearchDown.rect, x, y))
                    {
                        catMoResearchDown.hover = TRUE;
                        catMoResearchPage++;
                        taken = TRUE;
                    }
                    else
                    {
                        catMoResearchDown.hover = FALSE;
                    }
                }
            }
        }
    }
    // the messages: a ship's selects and focuses it, the research's opens the research manager
    if (catMoOptions()->gameMessage.show && !taken)
    {
        for (index = catIoMessageLast; catIoMessageLast - catIoMessagesShown < index; index--)
        {
            CatIoMessage *message = &catIoMessages[index];
            if (message->type != 0 && catIoInRect(&message->rect, mouseCursorXPosition, mouseCursorYPosition))
            {
                if (message->type == 1)
                {
                    if (message->data != NULL)
                    {
                        selSelected.numShips = 1;
                        selSelecting.numTargets = 1;
                        selSelected.ShipPtr[0] = (Ship *)message->data;
                        selSelecting.TargetPtr[0] = (SpaceObjRotImpTarg *)message->data;
                        ioUpdateShipTotals();
                        soundEventPlay(NULL, 0x108, NULL);
                        if (!smSensorsActive)
                        {
                            FocusCommand *focus = memAlloc(sizeofSelectCommand(1), "iomessagefocus", 0);
                            focus->numShips = 1;
                            focus->ShipPtr[0] = selSelected.ShipPtr[0];
                            soundEventPlay(NULL, 0x137, NULL);
                            ccFocus(&universe.mainCameraCommand, focus);
                            memFree(focus);
                        }
                        else
                        {
                            smCullAndFocusSelecting();
                        }
                    }
                }
                else if (message->type == 2)
                {
                    mrResearch(NULL, NULL);
                }
                taken = TRUE;
            }
        }
    }
    if (!taken && catMoTaskbarButtons && tutorial != 1)
    {   // the taskbar buttons
        if (catIoInRect(&catMoButtonSensors, x, y))
        {
            CAT_KB_EXECUTE(smSensorsActive, 0x4f);
            taken = TRUE;
        }
        else if (catIoInRect(&catMoButtonBuild, x, y))
        {
            CAT_KB_EXECUTE(smSensorsActive, 0x50);
            taken = TRUE;
        }
        else if (catIoInRect(&catMoButtonResearch, x, y))
        {
            CAT_KB_EXECUTE(smSensorsActive, 0x51);
            taken = TRUE;
        }
        else if (catIoInRect(&catMoButtonSystems, x, y))
        {
            CAT_KB_EXECUTE(smSensorsActive, 0x52);
            taken = TRUE;
        }
    }
    if (catMoOptions()->gameMessage.show && !taken)
    {   // the message scroll buttons
        if (catIoMessageUpShown && catIoInRect(&catIoMessageUpRect, x, y))
        {
            taken = TRUE;
            soundEventPlay(NULL, 0x121, NULL);
            catIoMessageLast++;
            if (catIoMessageLast >= catIoMessageCount)
            {
                catIoMessageLast = catIoMessageCount - 1;
                return TRUE;
            }
        }
        else if (catIoMessageDownShown && catIoInRect(&catIoMessageDownRect, x, y))
        {
            taken = TRUE;
            soundEventPlay(NULL, 0x122, NULL);
            if (catIoMessagesShown == 5)
            {
                catIoMessageLast--;
                if (catIoMessageLast < 0)
                {
                    catIoMessageLast = 0;
                }
            }
            catIoMessagesShown = catIoMessageCount + 1;
            if (catIoMessagesShown > 5)
            {
                catIoMessagesShown = 5;
            }
            for (index = 0; index < catIoMessageCount; index++)
            {
                catIoMessages[index].shownTime = 0;
            }
        }
    }
    return taken;
}

/*=============================================================================
    CAT: port diagnostic, not in the exe: /catHudTest <frames> (harness mode hud). The headless Mission01 run of
    /oracleMission (one universe tick a frame, no input) with the HUD on: at tick 16 the sensors manager the
    headless start opens is closed and the camera focuses the player's ships (as /catRenderTest); at tick 64 (the
    render list the band box walks is filled by then) the player's ships are band-boxed over the whole screen through
    the selection API (selRectSelect -> selRectDragFunction; those off screen are then added with
    selSelectionAddSingleShip) and the band box is kept as the "selecting"
    list, the ship info panel is switched on, a message refers to the first ship, and the movement pie plate is
    switched on (piePointModeOnOff); then every frame draws the status bars, the info panel, the main overlay and
    the pie plate. The headless start gives Mission01's fleet to player 1, so the test views the HUD as that player
    (universe.curPlayerPtr). HWI-TEST (M2-R6 managers): the last 120 frames are the sensors manager's (the selection
    becomes hot-key group 1, smSensorsBegin with the instant transition), drawn through the Cataclysm scene
    (cat_smViewportRenderScene and the cat_sm* draws, counted in catSmDrawCounts). At the end "hudtest: counts" and
    "hudtest: sensors" lines and "hudtest: done ... result=PASS|FAIL" (PASS: at least one ship band-boxed, status bars,
    info panel, overlay frames, selection lines, messages, support units, pie plate and compass all drawn; the sensors
    frames drew scenes, range circles, ships and hot-key numbers).
    HWI-TEST (M2-R7 r6-leftovers): 60 frames before the sensors frames the waypoint mode starts (cat_mrWaypointsStart,
    as the [Waypoints] key) and three waypoints are added (cat_mrWaypointAdd, as the mode's click does), so the rest
    of the run draws the waypoint cursor 0x004e6e70 (cat_pieWaypointCursorDraw), in the main view and then in the
    sensors manager; "hudtest: waypoints" counts its draws and its stream-0 draws, which must be the first draw's 63
    axis jitters plus one a waypoint and frame (PASS needs waypoint draws > 0 and that count).
    Nothing of this touches the simulation; the pie plate's sound event and the camera focus make this run no
    stream reference (the mission mode is).
=============================================================================*/
CatHudCounts catHudCounts;
static sdword catHudSelfTestTicks = 0;
static bool32 catHudSelfTestStarted = FALSE;
static bool32 catHudSelfTestSelected = FALSE;
static bool32 catHudSelfTestDone = FALSE;
static sdword catHudSelfTestFormerPlayer = -1;
static sdword catHudSelfTestBoxed = 0;
static bool32 catHudSelfTestSensors = FALSE;
static bool32 catHudSelfTestWaypoints = FALSE;      // HWI-TEST (M2-R7 r6-leftovers): the waypoint leg started
static sdword catHudSelfTestWaypointType = -1;
#define CAT_HUD_TEST_WAYPOINT_FRAMES 60             // HWI-TEST (M2-R7 r6-leftovers): the waypoint leg's frames before the sensors
#define CAT_HUD_TEST_SENSORS_FRAMES 120             // HWI-TEST (M2-R6 managers): the last frames are the sensors manager's
#define CAT_HUD_TEST_START_TICK     16
#define CAT_HUD_TEST_SELECT_TICK    64

void catHudSelfTestSet(char *string)
{
    catHudSelfTestTicks = (sdword)strtol(string, NULL, 10);
    if (catHudSelfTestTicks <= 0)
    {
        catHudSelfTestTicks = 600;
    }
    oracleSetMission(string);                       // the headless Mission01 run with the virtual clock and the quit
}

bool32 catHudSelfTestRequested(void)
{
    return (catHudSelfTestTicks > 0);
}

void catHudSelfTestTick(void)
{
    Node *node;
    Ship *ship;
    sdword index, playerShips = 0, boxed = 0;
    rectangle screen;
    bool32 pass;

    if (!catHudSelfTestStarted && (sdword)universe.univUpdateCounter >= CAT_HUD_TEST_START_TICK && mrCamera != NULL)
    {
        FocusCommand *focus = memAlloc(sizeofSelectCommand(20), "hudtestfocus", 0);
        catHudSelfTestStarted = TRUE;
        // the headless Mission01 start gives the level's whole fleet to player 1 and none to the local player (no
        //      opening NIS or mission script in the direct start of /oracleMission); the test views the HUD as the fleet's owner, so the
        //      own-ship paths (info panel, own-ship colours, band box of own ships) run on Mission01's ships
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            ship = (Ship *)listGetStructOfNode(node);
            if (ship->playerowner != NULL && ship->playerowner != universe.curPlayerPtr)
            {
                catHudSelfTestFormerPlayer = universe.curPlayerIndex;
                universe.curPlayerIndex = ship->playerowner->playerIndex;
                universe.curPlayerPtr = ship->playerowner;
                break;
            }
        }
        focus->numShips = 0;
        for (node = universe.ShipList.head; node != NULL && focus->numShips < 20; node = node->next)
        {
            ship = (Ship *)listGetStructOfNode(node);
            if (ship->playerowner == universe.curPlayerPtr)
            {
                focus->ShipPtr[focus->numShips++] = ship;
            }
        }
        for (node = universe.ShipList.head; node != NULL && focus->numShips == 0; node = node->next)
        {
            focus->ShipPtr[focus->numShips++] = (Ship *)listGetStructOfNode(node);
        }
        if (smSensorsActive)
        {
            smSensorsCloseForGood();
        }
        ccCameraTimeoutOverride = TRUE;
        ccFocusGeneral(&universe.mainCameraCommand, focus, FALSE);
        memFree(focus);
        dbgMessagef("hudtest: started tick=%u ships=%d viewing as player %d (was %d)", (unsigned)universe.univUpdateCounter,
                    (int)universe.ShipList.num, (int)universe.curPlayerIndex, (int)catHudSelfTestFormerPlayer);
    }
    if (catHudSelfTestStarted && !catHudSelfTestSelected && (sdword)universe.univUpdateCounter >= CAT_HUD_TEST_SELECT_TICK)
    {
        catHudSelfTestSelected = TRUE;
        screen.x0 = 0;
        screen.y0 = 0;
        screen.x1 = MAIN_WindowWidth;
        screen.y1 = MAIN_WindowHeight;
        // the band box over the whole screen: the player's ships it takes (the selection API, with the HW1 overlap test)
        selRectSelect(&universe.mainCameraCommand.actualcamera, &screen);
        boxed = selSelected.numShips;
        catHudSelfTestBoxed = boxed;
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            ship = (Ship *)listGetStructOfNode(node);
            if (ship->playerowner == universe.curPlayerPtr && !bitTest(ship->header.flags, SOF_Dead))
            {
                playerShips++;
                selSelectionAddSingleShip(&selSelected, ship);
            }
        }
        ioUpdateShipTotals();
        selSelecting.numTargets = 0;
        for (index = 0; index < selSelected.numShips && index < COMMAND_MAX_SHIPS; index++)
        {
            selSelecting.TargetPtr[selSelecting.numTargets++] = (SpaceObjRotImpTarg *)selSelected.ShipPtr[index];
        }
        catShipPrintOn = TRUE;
        if (selSelected.numShips > 0)
        {
            char text[CAT_IO_MESSAGE_LENGTH];
            snprintf(text, sizeof(text), "%s", ShipTypeToNiceStr(selSelected.ShipPtr[0]->shiptype));
            cat_ioMessageAdd(text, 0xff00ff00, 1, selSelected.ShipPtr[0]);
            piePointModeOnOff(PIE_SPEC_Move, NULL);
        }
        memset(&catHudCounts, 0, sizeof(catHudCounts));
        dbgMessagef("hudtest: selected tick=%u band-boxed=%d player ships=%d selected=%d selecting=%d piemode=%d",
                    (unsigned)universe.univUpdateCounter, (int)boxed, (int)playerShips, (int)selSelected.numShips,
                    (int)selSelecting.numTargets, (int)piePointSpecMode);
    }
    // HWI-TEST (M2-R7 r6-leftovers): the waypoint leg (the waypoint cursor 0x004e6e70 and its stream-0 draws)
    if (catHudSelfTestSelected && !catHudSelfTestWaypoints &&
        (sdword)universe.univUpdateCounter >= catHudSelfTestTicks - CAT_HUD_TEST_SENSORS_FRAMES - CAT_HUD_TEST_WAYPOINT_FRAMES)
    {
        vector point;

        catHudSelfTestWaypoints = TRUE;
        cat_mrWaypointsStart();
        catHudSelfTestWaypointType = (piePointSpecMode != PSM_Idle) ? piePointSpecType : -1;
        for (index = 0; index < 3 && catHudSelfTestWaypointType == PIE_SPEC_Waypoint; index++)
        {
            point = selCentrePoint;
            point.x += (index == 1 || index == 2) ? 2000.0f : 0.0f;
            point.y += (index == 0 || index == 1) ? 2000.0f : 0.0f;
            point.z += (index == 2) ? 500.0f : 0.0f;
            cat_mrWaypointAdd(&point);
        }
        dbgMessagef("hudtest: waypoints start tick=%u type=%d waypoints=%d cursor=%s", (unsigned)universe.univUpdateCounter,
                    (int)catHudSelfTestWaypointType, (int)catMrWaypointCount,
                    (pieMovementCursorDrawCallback == cat_pieWaypointCursorDraw) ? "waypoint" : "other");
    }
    // HWI-TEST (M2-R6 managers, sensors): the sensors frames: the selection becomes hot-key group 1 and the sensors
    //      manager opens with the instant transition (the test's settings) for the last frames, drawn through the
    //      Cataclysm scene (cat_smViewportRenderScene and the cat_sm* draws; catSmDrawCounts)
    if (catHudSelfTestSelected && !catHudSelfTestSensors && (sdword)universe.univUpdateCounter >= catHudSelfTestTicks - CAT_HUD_TEST_SENSORS_FRAMES)
    {
        bool32 instant = smInstantTransition;

        catHudSelfTestSensors = TRUE;
        selHotKeyGroup[1].numShips = 0;
        for (index = 0; index < selSelected.numShips; index++)
        {
            selHotKeyGroup[1].ShipPtr[selHotKeyGroup[1].numShips++] = selSelected.ShipPtr[index];
        }
        memset(&catSmDrawCounts, 0, sizeof(catSmDrawCounts));
        if (smSensorsActive)
        {
            smSensorsCloseForGood();
        }
        smFleetIntel = FALSE;
        smInstantTransition = TRUE;
        smSensorsBegin(NULL, NULL);
        smInstantTransition = instant;
        dbgMessagef("hudtest: sensors open tick=%u active=%d hotkeygroup=%d", (unsigned)universe.univUpdateCounter,
                    (int)smSensorsActive, (int)selHotKeyGroup[1].numShips);
    }
    if (catHudSelfTestSelected && !catHudSelfTestDone && (sdword)universe.univUpdateCounter + 1 >= catHudSelfTestTicks)
    {
        catHudSelfTestDone = TRUE;
        pass = (catHudSelfTestBoxed > 0 && catHudCounts.frames > 0 && catHudCounts.statusBars > 0 && catHudCounts.bars > 0 &&
                catHudCounts.infoPanels > 0 && catHudCounts.infoLines > 0 && catHudCounts.selectedLines > 0 &&
                catHudCounts.selectingLines > 0 && catHudCounts.messageLines > 0 && catHudCounts.supportUnits > 0 &&
                catHudCounts.pieDraws > 0 && catHudCounts.compassDraws > 0);
        // the sensors frames drew the scene, the player's range circles, its ships and the hot-key numbers
        pass = pass && (catSmDrawCounts.scenes > 0) && (catSmDrawCounts.rangeCircles > 0) &&
               (catSmDrawCounts.shipPoints + catSmDrawCounts.meshes > 0) && (catSmDrawCounts.hotkeyNumbers > 0);
        // HWI-TEST (M2-R7 r6-leftovers): the waypoint cursor drew, with 63 stream-0 draws for the first draw's axis
        //      jitter and one a waypoint and frame (three waypoints)
        pass = pass && (catHudSelfTestWaypointType == PIE_SPEC_Waypoint) && (catHudCounts.waypointDraws > 0) &&
               (catHudCounts.waypointRandoms == 63 + 3 * catHudCounts.waypointDraws);
        dbgMessagef("hudtest: waypoints draws=%u randoms=%u (expected %u) waypoints=%d mode=%d",
                    catHudCounts.waypointDraws, catHudCounts.waypointRandoms, 63 + 3 * catHudCounts.waypointDraws,
                    (int)catMrWaypointCount, (int)catMrWaypointMode);
        dbgMessagef("hudtest: sensors scenes=%u rangecircles=%u shippoints=%u meshes=%u icons=%u slipgatepaths=%u hotkeys=%u resources=%u mines=%u bounds=%u compass=%u",
                    catSmDrawCounts.scenes, catSmDrawCounts.rangeCircles, catSmDrawCounts.shipPoints, catSmDrawCounts.meshes,
                    catSmDrawCounts.icons, catSmDrawCounts.slipgatePaths, catSmDrawCounts.hotkeyNumbers,
                    catSmDrawCounts.resources, catSmDrawCounts.mines, catSmDrawCounts.boundsFrames, catSmDrawCounts.compassFrames);
        dbgMessagef("hudtest: sensorspoints markers 1px=%u 2px=%u 3px=%u dust=%u dustsize=%u sensorsPoints=%d",   // HWI-TEST (M3-R4 sensors-points)
                    catSmDrawCounts.markerSizes[1], catSmDrawCounts.markerSizes[2], catSmDrawCounts.markerSizes[3],   // HWI-TEST (M3-R4 sensors-points)
                    catSmDrawCounts.dustPoints, catSmDrawCounts.dustSize, (int)hwiSensorsPoints);                     // HWI-TEST (M3-R4 sensors-points)
        dbgMessagef("hudtest: counts frames=%u statusbars=%u bars=%u icons=%u infopanels=%u infolines=%u",
                    catHudCounts.frames, catHudCounts.statusBars, catHudCounts.bars, catHudCounts.icons,
                    catHudCounts.infoPanels, catHudCounts.infoLines);
        dbgMessagef("hudtest: counts selectedlines=%u selectinglines=%u messagelines=%u supportunits=%u pie=%u compass=%u",
                    catHudCounts.selectedLines, catHudCounts.selectingLines, catHudCounts.messageLines,
                    catHudCounts.supportUnits, catHudCounts.pieDraws, catHudCounts.compassDraws);
        dbgMessagef("hudtest: done tick=%u boxed=%d selected=%d frames=%u statusbars=%u infopanels=%u pie=%u result=%s",
                    (unsigned)universe.univUpdateCounter, (int)catHudSelfTestBoxed, (int)selSelected.numShips, catHudCounts.frames,
                    catHudCounts.statusBars, catHudCounts.infoPanels, catHudCounts.pieDraws, pass ? "PASS" : "FAIL");
    }
}
