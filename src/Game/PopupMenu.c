// =============================================================================
//  PopupMenu.c
//  - Cataclysm's right-click popup menu: the orders of the selection, the
//    special actions on a target, the submenus (tactics, formations, the
//    sensors view, focus, views, ship names, waypoint types, the mimic and
//    Leech choices, alliances) (cataclysm.exe 0x004eaa60-0x004ef860)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R6 mainrgn-popup). Each body follows the r6 decompile of its exe function (address on each) and,
//      where the decompile lost the arguments (the overlay icon calls, the stack-passed action words), the
//      disassembly. The main rows 0..0x17 run their key command (cat_kbCommandExecute 0x0054eb10); the special rows
//      0x18..0x30 run cat_popupMenuExecuteSpecial, which goes through the cat_ui* commands of mainrgn.c to the wraps.
//      The menu is drawn by mrRegionDraw / the sensors manager (cat_popupMenuDraw) and opened by the right-click
//      order (cat_mrReleaseRight 0x0056b0b0), the Ship_Special key and the Leech / mimic keys.

#include "PopupMenu.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "Alliance.h"
#include "CameraCommand.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "CommandWrap.h"
#include "Debug.h"
#include "font.h"
#include "FontReg.h"
#include "GameChat.h"
#include "Globals.h"
#include "KeyBindings.h"
#include "mainrgn.h"
#include "Mimic.h"
#include "mouse.h"
#include "OverlayIcons.h"
#include "render.h"
#include "Select.h"
#include "Sensors.h"
#include "ShipNames.h"
#include "ShipPrint.h"
#include "SinglePlayer.h"
#include "StringsOnly.h"
#include "Task.h"
#include "Tutor.h"
#include "Tweak.h"
#include "Universe.h"
#include "StatScript.h"
#include "InfoOverlay.h"
#include "Memory.h"
#include "PiePlate.h"
#include "utility.h"
#include "OracleDump.h"
#include "GamePick.h"

/*=============================================================================
    Data
=============================================================================*/
// the static tables: the exe's rows (string id, key command, submenu, group, visible, id); the builders set the rest
CatPopupItem catPmItems[49] =                    // 0x0088de88
{
    { NULL, 0,                                 0x00,  0,  0, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  0 },
    { NULL, strMenuSpecialActionDesc,          0x07,  0,  1, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  1 },
    { NULL, 0,                                 0x0f,  0,  2, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  2 },
    { NULL, strMenuMoveDesc,                   0x01,  0,  3, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  3 },
    { NULL, strMenuHyperspaceDesc,             0x08,  0,  3, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  4 },
    { NULL, strMenuCancelMoveDesc,             0x5a,  0,  3, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  5 },
    { NULL, strMenuStartWaypointsDesc,         0x09,  0,  3, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  6 },
    { NULL, strMenuEndWaypointsDesc,           0x09,  0,  3, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  7 },
    { NULL, strMenuWaypointTypeDesc,           0x00,  8,  3, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  8 },
    { NULL, strMenuCancelWaypointsDesc,        0x5a,  0,  3, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  9 },
    { NULL, strMenuRemoveLastWaypointDesc,     0x0e,  0,  3, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 10 },
    { NULL, strMenuDockDesc,                   0x02,  0,  3, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 11 },
    { NULL, strMenuRetireDesc,                 0x03,  0,  3, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 12 },
    { NULL, strMenuCancelDesc,                 0x04,  0,  3, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 13 },
    { NULL, strMenuTacticsDesc,                0x00,  1,  4, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 14 },
    { NULL, strMenuFormationsDesc,             0x00,  2,  4, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 15 },
    { NULL, strMenuFocusDesc,                  0x00,  5,  5, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 16 },
    { NULL, strMenuViewDesc,                   0x00,  6,  6, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 17 },
    { NULL, strMenuSensorsViewDesc,            0x00,  3,  7, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 18 },
    { NULL, strMenuSetShipNameDesc,            0x00,  7,  8, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 19 },
    { NULL, 0,                                 0x08,  0,  9, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 20 },
    { NULL, strMenuAllianceFormDesc,           0x00, 11, 10, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 21 },
    { NULL, strMenuAllianceBreakDesc,          0x00, 12, 10, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 22 },
    { NULL, strMenuTransferRusDesc,            0x00, 13, 10, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 23 },
    { NULL, strMenuNoSpecialActionDesc,        0x00,  0, 11, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 24 },
    { NULL, strMenuSpecialHarvestDesc,         0x11,  0, 12, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 25 },
    { NULL, strMenuSpecialRepairDesc,          0x12,  0, 12, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 26 },
    { NULL, strMenuSpecialSalvageDesc,         0x13,  0, 12, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 27 },
    { NULL, strMenuSpecialSelectLeechTypeDesc, 0x14,  4, 13, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 28 },
    { NULL, strMenuSpecialLeechDesc,           0x18,  0, 13, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 29 },
    { NULL, strMenuSpecialUnleechDesc,         0x19,  0, 13, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 30 },
    { NULL, strMenuSpecialLinkDesc,            0x1a,  0, 14, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 31 },
    { NULL, strMenuSpecialUnlinkDesc,          0x1b,  0, 14, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 32 },
    { NULL, strMenuSpecialSelectMimicTypeDesc, 0x1c,  9, 15, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 33 },
    { NULL, strMenuSpecialSelectMimicItemDesc, 0x1d, 10, 15, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 34 },
    { NULL, strMenuSpecialUnmimickDesc,        0x1e,  0, 15, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 35 },
    { NULL, strMenuSpecialForceFieldDesc,      0x1f,  0, 16, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 36 },
    { NULL, strMenuSpecialUnlinkForceFieldDesc, 0x20,  0, 16, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 37 },
    { NULL, strMenuSpecialCloakDesc,           0x21,  0, 17, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 38 },
    { NULL, strMenuSpecialCancelCloakDesc,     0x22,  0, 17, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 39 },
    { NULL, strMenuSpecialLaunchSwarmersDesc,  0x23,  0, 18, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 40 },
    { NULL, strMenuSpecialCancelSwarmersDesc,  0x24,  0, 18, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 41 },
    { NULL, strMenuSpecialFireBigGunDesc,      0x25,  0, 18, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 42 },
    { NULL, strMenuSpecialCancelBigGunDesc,    0x26,  0, 18, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 43 },
    { NULL, strMenuSpecialBeastAttackDesc,     0x27,  0, 18, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 44 },
    { NULL, strMenuSpecialEMPDesc,             0x28,  0, 18, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 45 },
    { NULL, strMenuSpecialRamDesc,             0x29,  0, 18, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 46 },
    { NULL, strMenuSpecialRepulseDesc,         0x2a,  0, 18, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 47 },
    { NULL, strMenuSpecialFireMissilesDesc,    0x2b,  0, 18, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 48 },
};

CatPopupItem catPmTactics[3] =                  // 0x0088ea08
{
    { NULL, strMenuEvasiveDesc,                0x2c,  0,  4, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  0 },
    { NULL, strMenuNeutralDesc,                0x2d,  0,  4, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  1 },
    { NULL, strMenuAggressiveDesc,             0x2e,  0,  4, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  2 },
};

CatPopupItem catPmFormations[8] =               // 0x0088eac0
{
    { NULL, strMenuDeltaDesc,                  0x31,  0,  4, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  0 },
    { NULL, strMenuBroadDesc,                  0x32,  0,  4, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  1 },
    { NULL, strMenuDelta3DDesc,                0x33,  0,  4, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  2 },
    { NULL, strMenuClawDesc,                   0x34,  0,  4, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  3 },
    { NULL, strMenuWallDesc,                   0x35,  0,  4, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  4 },
    { NULL, strMenuSphereDesc,                 0x36,  0,  4, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  5 },
    { NULL, strMenuCustomDesc,                 0x37,  0,  4, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  6 },
    { NULL, strMenuParadeDesc,                 0x38,  0,  4, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  7 },
};

CatPopupItem catPmSensorsView[12] =              // 0x0088eca0
{
    { NULL, strMenuSV_AreasVisited,            0x8d,  0,  7, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  0 },
    { NULL, strMenuSV_ShipFOWRange,            0x8c,  0,  7, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  1 },
    { NULL, strMenuSV_AreasOccupied,           0x8e,  0,  7, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  2 },
    { NULL, strMenuSV_Asteriods,               0x8b,  0,  7, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  3 },
    { NULL, strMenuSV_DecorativeAsteroids,     0x93,  0,  7, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  4 },
    { NULL, strMenuSV_Background,              0x92,  0,  7, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  5 },
    { NULL, strMenuSV_Compass,                 0x94,  0,  7, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  6 },
    { NULL, strMenuSV_ObjectivesPointer,       0x95,  0,  7, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  7 },
    { NULL, strMenuSV_Icons,                   0x8f,  0,  7, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  8 },
    { NULL, strMenuSV_SlipgatePaths,           0x96,  0,  7, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  9 },
    { NULL, strMenuSV_ShipMeshes,              0x90,  0,  7, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 10 },
    { NULL, strMenuSV_PieplateLines,           0x91,  0,  7, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 }, 11 },
};

CatPopupItem catPmLeech[4] =                    // 0x0088ef70
{
    { NULL, strMenuSpyDesc,                    0x15,  0, 11, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  0 },
    { NULL, strMenuLeechDesc,                  0x16,  0, 11, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  1 },
    { NULL, strMenuVentDesc,                   0x17,  0, 11, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  2 },
    { NULL, 0,                                 0x00,  0,  0, 0, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  0 },
};

CatPopupItem catPmFocus[4] =                    // 0x0088f060
{
    { NULL, strMenuFocucSelectedDesc,          0x3b,  0,  5, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  0 },
    { NULL, strMenuFocusOnMothershipDesc,      0x3c,  0,  5, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  1 },
    { NULL, strMenuFocusOnLastShipToSpeakDesc, 0x3d,  0,  5, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  2 },
    { NULL, strMenuFocusFleetViewDesc,         0x40,  0,  5, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  3 },
};

CatPopupItem catPmView[8] =                     // 0x0088f150
{
    { NULL, strMenuViewMainRgnDesc,            0x4f,  0,  6, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  0 },
    { NULL, strMenuViewSensorsDesc,            0x4f,  0,  6, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  1 },
    { NULL, strMenuViewBuildManagerDesc,       0x50,  0,  6, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  2 },
    { NULL, strMenuViewResearchManagerDesc,    0x51,  0,  6, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  3 },
    { NULL, strMenuViewLaunchManagerDesc,      0x52,  0,  6, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  4 },
    { NULL, strMenuViewShipOverlayDesc,        0x54,  0,  6, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  5 },
    { NULL, strMenuViewTacticalOverlayDesc,    0x53,  0,  6, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  6 },
    { NULL, strMenuViewChaseModeDesc,          0x47,  0,  6, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  7 },
};

CatPopupItem catPmWaypointType[3] =             // 0x0088f330
{
    { NULL, strMenuWaypointTypeNormal,         0x0b,  0,  3, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  0 },
    { NULL, strMenuWaypointTypeEndToEnd,       0x0c,  0,  3, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  1 },
    { NULL, strMenuWaypointTypeCircular,       0x0d,  0,  3, 1, FALSE, FALSE, FALSE, FALSE, { 0, 0, 0, 0 },  2 },
};

// the tables the builders fill
static CatPopupItem catPmShipNames[50];             // 0x00a51d98 (submenu 7)
static CatPopupItem catPmAllianceForm[8];           // 0x00a52950 (submenu 11)
static CatPopupItem catPmTransferRUs[8];            // 0x00a52b38 (submenu 13)
static CatPopupItem catPmMimicItems[CAT_MIMIC_ITEM_MAX];    // 0x00a53cc0 (submenu 10)
static CatPopupItem catPmAllianceBreak[8];          // 0x00a560e8 (submenu 12)
static CatPopupItem catPmMimicPlayers[CAT_MIMIC_ITEM_PLAYERS];  // 0x00a57270 (submenu 9)
static char catPmMimicItemText[CAT_MIMIC_ITEM_MAX][50];     // 0x00a50d28
static char catPmMimicPlayerText[CAT_MIMIC_ITEM_PLAYERS][50];   // 0x00a55070
static sdword catPmShipNameCount = 0;               // 0x00a575f8
static sdword catPmMimicPlayerCount = 0;            // 0x00a575fc
static sdword catPmMimicItemCount = 0;              // 0x00a57600
static sdword catPmAllianceFormCount = 0;           // 0x00a57604
static sdword catPmAllianceBreakCount = 0;          // 0x00a57608
static sdword catPmTransferRUsCount = 0;            // 0x00a5760c
static bool32 catPmMimicAdded = FALSE;              // 0x00a57610: the mimic items of the full list (an sMCV selected)

sdword catPmSelected = -1;                          // 0x0088dde0
sdword catPmSubSelected = -1;                       // 0x0088dde4

static bool32 catPmOpen = FALSE;                    // 0x00a574a0
static sdword catPmOpenX, catPmOpenY;               // 0x00a574a8 / 0x00a574ac: the mouse where it opened
static SpaceObj *catPmTarget = NULL;                // 0x00a574b0: the clicked object, CAT_PM_TARGET_KEYCOMMAND, or NULL
static MaxSelection catPmTargets;                   // 0x00a562c8: the clicked objects
static SelectCommand catPmSpecialTarget;            // 0x00a52d18: the nearest target of the selected special row

static rectangle catPmMainRect;                     // 0x00a574b8
static rectangle catPmSubRect[CAT_PMS_Number];      // 0x00a574c8 + (submenu - 1) * 0x10
static sdword catPmMainCheckCol, catPmMainKeyCol;   // 0x00a57598 / 0x00a5759c
static sdword catPmSubCheckCol[CAT_PMS_Number];     // 0x0088dde8.. / 0x00a575b8.. (one per submenu)
static sdword catPmSubKeyCol[CAT_PMS_Number];       // 0x00a575a0..

// the metrics, from the width / height of a space in the menu font (cat_popupMenuDraw)
static sdword catPmKeyGap;                          // 0x00a560dc: 4 spaces between the text and the key column
static sdword catPmLeftPad;                         // 0x00a51d90: 0.4 spaces
static sdword catPmRightPad;                        // 0x00a560d8: 0.4 spaces
static sdword catPmMargin;                          // 0x00a52b34: 3 spaces round the frame
static sdword catPmKeyMinWidth;                     // 0x00a57498: 3 spaces
static sdword catPmCheckWidth;                      // 0x00a57494: 3 spaces
static sdword catPmHoverEnd;                        // 0x00a57490: 3 spaces, the hover bar's end caps
static sdword catPmHoverPad;                        // 0x00a52b30: 0.8 spaces
static sdword catPmLineGap;                         // 0x00a5748c: half a line between rows
static sdword catPmGroupGap;                        // 0x00a560e0: 0.6 lines between groups (main menu)

static fonthandle catPmFontLarge;                   // 0x00a575f0 arial_15.hff
static fonthandle catPmFontSmall;                   // 0x00a575f4 arial_12.hff
// CAT: 0x00a5749c, the options' "small fonts" toggle inverted (cat_opSmallFonts 0x004d9df0, M2-R6 front end)
bool32 catPmLargeFont = FALSE;

static real32 catPmFlashStart = 0.0f;               // 0x00a57624
static sdword catPmLastMouseX, catPmLastMouseY;     // 0x00a5762c / 0x00a57630

// the main menu sits 5 px right of and below the mouse; a submenu 10 px right of its row (0x0072b018 .. 0x0072b024)
#define CAT_PM_MAIN_DX                  5
#define CAT_PM_MAIN_DY                  5
#define CAT_PM_SUB_DX                   10
#define CAT_PM_SUB_DY                   0

#define CAT_PM_COLOUR_NORMAL            0xffffffff          // 0x0072b000
#define CAT_PM_COLOUR_DISABLED          0xff787878          // 0x0072affc
#define CAT_PM_COLOUR_TITLE             0xff00c800          // 0x0072b004: the first main row (the ship's name)
#define CAT_PM_COLOUR_FLASH0            0xff0000ff          // 0x0072aff4
#define CAT_PM_COLOUR_FLASH1            0xff00ffff          // 0x0072aff8
#define CAT_PM_FLASH_PERIOD             5.0f                // 0x0072aff0
#define CAT_PM_COLOUR_SELECTION         0xffe6b496          // 0x0072b00c: the ships the selected row orders
#define CAT_PM_COLOUR_TARGET_ALLY       0xff009600          // 0x0072b010
#define CAT_PM_COLOUR_TARGET_ENEMY      0xff000096          // 0x0072b014
#define CAT_PM_COLOUR_FRAME_GL          colRGBA(0, 0, 0x3c, 0x96)  // 0x0088f3e4..0x0088f3f0, translucent
#define CAT_PM_COLOUR_FRAME_SW          colRGBA(0, 0, 0, 0)        // 0x00a57614..0x00a57620 (no writer: black)

// the per-mission hyperspace label of cat_spMissionFlagValue lives in SinglePlayer.c

/*=============================================================================
    Functions
=============================================================================*/

// the rows the selection may pick: visible and not disabled; the first from 'start' on (wrapping), as the exe's
//      loops after every change of catPmSelected
static void catPmSelectFrom(sdword start)
{
    bool32 wrapped = FALSE;

    catPmSelected = start;
    while (((catPmItems[catPmSelected].disabled == TRUE) || !catPmItems[catPmSelected].visible) && !wrapped)
    {
        catPmSelected++;
        if (catPmSelected >= CAT_PM_NumberItems)
        {
            catPmSelected = 0;
        }
        if (catPmSelected == start)
        {
            wrapped = TRUE;
        }
    }
    if ((catPmSelected >= CAT_PM_NoSpecialAction) && (catPmSelected < CAT_PM_NumberItems) && (catPmTargets.numShips == 0))
    {
        cat_popupMenuFindSpecialTarget(catPmSelected);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuFindSpecialTarget                    0x004eaa60
    Description : The nearest target of a special row when the menu has no target of its own (the selection's first
                  ship's nearest: damaged ally to repair, salvage, enemy, EMP-able enemy).
----------------------------------------------------------------------------*/
void cat_popupMenuFindSpecialTarget(sdword item)
{
    sdword kind;
    Ship *target;

    switch (item)
    {
        case CAT_PM_Repair:
            kind = 1;
            break;
        case CAT_PM_Salvage:
            kind = 0;
            break;
        case CAT_PM_Leech:
        case CAT_PM_LaunchSwarmers:
        case CAT_PM_BeastAttack:
        case CAT_PM_Ram:
        case CAT_PM_Repulse:
        case CAT_PM_FireMissiles:
            kind = 2;
            break;
        case CAT_PM_EMP:
            kind = 3;
            break;
        default:
            catPmSpecialTarget.numShips = 0;
            return;
    }
    target = cat_uiFindNearestSpecialTarget(selSelected.ShipPtr[0], kind);
    if (target != NULL)
    {
        catPmSpecialTarget.numShips = 1;
        catPmSpecialTarget.ShipPtr[0] = target;
        return;
    }
    catPmSpecialTarget.numShips = 0;
}

bool32 cat_popupMenuMouseInRect(rectangle *rect)                                           // 0x004eaaf0
{
    return (rect->x0 - catPmMargin <= mouseCursorX()) && (rect->y0 - catPmMargin <= mouseCursorY()) &&
           (mouseCursorX() < rect->x1 + catPmMargin) && (mouseCursorY() < rect->y1 + catPmMargin);
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuSetupSensorsItems                    0x004eab40
    Description : The sensors-view submenu (row 0x12) is shown in the sensors manager only, its rows checked by the
                  sensors view options.
----------------------------------------------------------------------------*/
void cat_popupMenuSetupSensorsItems(void)
{
    sdword index;

    if (smSensorsActive)
    {
        catPmSensorsView[3].checked = catSmViewResources;           // 0x00892b4c
        catPmSensorsView[1].checked = catSmViewFOWRanges;           // 0x00892b50
        catPmSensorsView[0].checked = catSmViewAreasVisited;        // 0x00892b58
        catPmSensorsView[2].checked = catSmViewAreasOccupied;       // 0x00a5b19c
        catPmSensorsView[8].checked = catSmViewIcons;               // 0x00892b5c
        catPmSensorsView[11].checked = catSmViewPieplateLines;      // 0x00892b64
        catPmSensorsView[10].checked = catSmViewShipMeshes;         // 0x00892b60
        catPmSensorsView[5].checked = catSmViewBackground;          // 0x00892b54
        catPmSensorsView[4].checked = catSmViewDecorativeAsteroids; // 0x00892b48
        catPmSensorsView[6].checked = catSmViewCompass;             // 0x00a5b1a4
        catPmSensorsView[7].checked = catSmViewObjectivePointers;   // 0x00892b6c
        catPmSensorsView[9].checked = catSmViewSlipgatePaths;       // 0x00892b70
        catPmItems[CAT_PM_SensorsView].visible = TRUE;
        for (index = 0; index < 12; index++)
        {
            catPmSensorsView[index].visible = TRUE;
        }
        return;
    }
    catPmItems[CAT_PM_SensorsView].visible = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuBuildTargetItems                     0x004eac30
    Description : The rows of a menu opened on a target: the special actions the (not busy) selection can do to the
                  clicked objects (or, without them, to the special row's nearest target).
----------------------------------------------------------------------------*/
void cat_popupMenuBuildTargetItems(void)
{
    CatTargetActions targetActions, selectionActions;
    MaxSelection selection;
    MaxSelection *targets;
    bool32 any = FALSE, noTargets;
    udword *s, *t;
    sdword index, count;
    ubyte player;

    targets = (catPmTargets.numShips == 0) ? (MaxSelection *)&catPmSpecialTarget : &catPmTargets;
    cat_selComputeTargetActions(&targetActions, targets);
    cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
    cat_selMakeShipsNotBusy((SelectCommand *)&selection);
    cat_selComputeTargetActions(&selectionActions, &selection);
    s = selectionActions.flag;
    t = targetActions.flag;
    noTargets = (catPmTargets.numShips == 0);

    for (index = 0; index < CAT_PM_NumberItems; index++)
    {
        catPmItems[index].visible = FALSE;
    }
    if (s[CAT_TA_Harvest] && (noTargets || t[CAT_TA_Resource]))
    {
        catPmItems[CAT_PM_Harvest].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_Repair] && (noTargets || t[CAT_TA_Ally]))
    {
        catPmItems[CAT_PM_Repair].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_Salvage] && (noTargets || t[CAT_TA_Salvageable]))
    {
        catPmItems[CAT_PM_Salvage].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_Hive] && (noTargets || t[CAT_TA_Enemy]))
    {
        catPmItems[CAT_PM_LaunchSwarmers].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_EMP] && (noTargets || t[CAT_TA_Enemy]))
    {
        catPmItems[CAT_PM_EMP].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_BeastWeapon] && (noTargets || t[CAT_TA_Enemy]))
    {
        catPmItems[CAT_PM_BeastAttack].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_Leech] && (noTargets || t[CAT_TA_Enemy]))
    {
        catPmItems[CAT_PM_Leech].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_BigGun] && noTargets)
    {
        catPmItems[CAT_PM_FireBigGun].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_BigGunFiring] && noTargets)
    {
        catPmItems[CAT_PM_CancelBigGun].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_Ram])
    {
        catPmItems[CAT_PM_Ram].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_AcolyteLinkFire])
    {
        catPmItems[CAT_PM_FireMissiles].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_Leech])
    {                                                       // the leech type submenu, the shared action checked
        catPmItems[CAT_PM_SelectLeechType].visible = TRUE;
        any = TRUE;
        if (catPmSelected == CAT_PM_SelectLeechType)
        {
            catPmLeech[0].visible = TRUE;
            catPmLeech[0].checked = (selectionActions.leechAction == 6);
            catPmLeech[1].visible = TRUE;
            catPmLeech[1].checked = (selectionActions.leechAction == 7);
            catPmLeech[2].visible = TRUE;
            catPmLeech[2].checked = (selectionActions.leechAction == 8);
        }
    }
    if (s[CAT_TA_HiveReady] && noTargets)
    {
        catPmItems[CAT_PM_CancelSwarmers].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_Leeching] && noTargets)
    {
        catPmItems[CAT_PM_Unleech].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_Link] && noTargets)
    {
        catPmItems[CAT_PM_Link].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_Unlink] && noTargets)
    {
        catPmItems[CAT_PM_Unlink].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_Mimic] && noTargets)
    {                                                       // the mimic submenus: whose ships, which ship
        catPmItems[CAT_PM_SelectMimicType].visible = TRUE;
        catPmItems[CAT_PM_SelectMimicType].disabled = FALSE;
        if (catPmSelected == CAT_PM_SelectMimicType)
        {
            for (index = 0; index < CAT_MIMIC_ITEM_PLAYERS; index++)
            {
                if (cat_mimicPlayerAvailable((ubyte)index))
                {
                    catPmMimicPlayers[index].disabled = FALSE;
                    catPmMimicPlayers[index].visible = TRUE;
                    catPmMimicPlayers[index].flash = FALSE;
                    catPmMimicPlayers[index].submenu = 0;
                    sprintf(catPmMimicPlayerText[index], "%d. %s", index, cat_mimicPlayerName((ubyte)index));
                    catPmMimicPlayers[index].text = catPmMimicPlayerText[index];
                    catPmMimicPlayers[index].checked = (cat_mimicGetCurrentPlayer() == index);
                }
                else
                {
                    catPmMimicPlayers[index].visible = FALSE;
                }
            }
            catPmMimicPlayerCount = CAT_MIMIC_ITEM_PLAYERS;
        }
        catPmItems[CAT_PM_SelectMimicItem].visible = TRUE;
        catPmItems[CAT_PM_SelectMimicItem].disabled = FALSE;
        if (catPmSelected == CAT_PM_SelectMimicItem)
        {
            player = cat_mimicGetCurrentPlayer();
            count = cat_mimicItemCount(player, s[CAT_TA_MCV]);
            for (index = 0; (index < count) && (index < CAT_MIMIC_ITEM_MAX); index++)
            {
                catPmMimicItems[index].disabled = FALSE;
                catPmMimicItems[index].visible = TRUE;
                catPmMimicItems[index].flash = FALSE;
                catPmMimicItems[index].submenu = 0;
                sprintf(catPmMimicItemText[index], "%d. %s", index, cat_mimicItemName(player, s[CAT_TA_MCV], index));
                catPmMimicItems[index].text = catPmMimicItemText[index];
            }
            catPmMimicItemCount = min(cat_mimicItemCount(player, s[CAT_TA_MCV]), CAT_MIMIC_ITEM_MAX);
        }
        if (s[CAT_TA_Mimicking])
        {
            catPmItems[CAT_PM_Unmimic].disabled = FALSE;
        }
        catPmItems[CAT_PM_Unmimic].visible = (s[CAT_TA_Mimicking] != 0);
        any = TRUE;
        catPmMimicAdded = s[CAT_TA_MCV];
    }
    if (s[CAT_TA_DreadnoughtCannon] && noTargets)
    {
        catPmItems[CAT_PM_Repulse].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_Decloak] && noTargets)
    {
        catPmItems[CAT_PM_CancelCloak].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_Cloak] && noTargets)
    {
        catPmItems[CAT_PM_Cloak].visible = TRUE;
        any = TRUE;
    }
    if (s[CAT_TA_ForceField] == 0)
    {
        if (s[CAT_TA_InForceField] == 0)
        {
            catPmItems[CAT_PM_NoSpecialAction].visible = !any;
            return;
        }
    }
    else if (s[CAT_TA_InForceField] == 0)
    {
        catPmItems[CAT_PM_NoSpecialAction].visible = FALSE;
        catPmItems[CAT_PM_ForceField].visible = TRUE;
        return;
    }
    catPmItems[CAT_PM_NoSpecialAction].visible = FALSE;
    catPmItems[CAT_PM_UnlinkForceField].visible = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuLayout                               0x004eb0b0
    Description : Places a menu's visible rows under (x, y): one line each (the main menu leaves a gap between groups),
                  the text column as wide as the widest text, then the check column and the key column; the menu is
                  moved back onto the screen.
    Inputs      : forceCheck - give the check column even with nothing checked; isMain - the main menu
    Outputs     : rect, checkCol (TRUE if a check column is shown), keyCol (the key column's width)
----------------------------------------------------------------------------*/
void cat_popupMenuLayout(sdword x, sdword y, CatPopupItem *items, sdword count, rectangle *rect,
                         sdword *checkCol, sdword *keyCol, bool32 forceCheck, bool32 isMain)
{
    sdword keyWidth = catPmKeyMinWidth;
    sdword halfGap = catPmLineGap / 2;
    sdword height, width, shiftX, shiftY, gaps = 0, index, w;
    bool32 keyColumn = FALSE, seen;
    sdword groupShift[19], groupUsed[19];
    char keyString[256];
    CatPopupItem *item;

    if (isMain)
    {
        memset(groupShift, 0, sizeof(groupShift));
        memset(groupUsed, 0, sizeof(groupUsed));
    }
    rect->x0 = x;
    rect->y0 = y;
    *checkCol = 0;
    *keyCol = 0;
    height = halfGap;
    for (index = 0; index < count; index++)
    {
        item = &items[index];
        if (!item->visible)
        {
            continue;
        }
        item->rect.x0 = rect->x0;
        if (isMain)
        {
            groupUsed[item->group] = TRUE;
        }
        item->rect.y0 = rect->y0 + height;
        item->rect.y1 = fontHeight(" ") + item->rect.y0;
        height += fontHeight(" ") + halfGap;
        w = fontWidth((item->text == NULL) ? strGetString(item->strId) : item->text);
        if (*keyCol < w)
        {
            *keyCol = w;
        }
        if (item->command != 0)
        {
            cat_kbCommandKeyString(item->command, keyString);
            if (keyWidth < fontWidth(keyString))
            {
                keyWidth = fontWidth(keyString);
            }
            keyColumn = TRUE;
        }
        if (item->submenu != 0)
        {
            keyColumn = TRUE;
        }
        if (item->checked)
        {
            *checkCol = TRUE;
        }
    }
    if (forceCheck)
    {
        *checkCol = TRUE;
    }
    width = *keyCol + catPmRightPad + catPmLeftPad;
    if (*checkCol)
    {
        width += catPmCheckWidth;
    }
    if (keyColumn)
    {
        width += catPmKeyGap + keyWidth;
    }
    if (isMain)
    {                                                       // a gap before every used group but the first
        seen = FALSE;
        for (index = 0; index < 19; index++)
        {
            if (groupUsed[index])
            {
                if (seen)
                {
                    gaps += catPmGroupGap;
                    groupShift[index] = gaps;
                }
                else
                {
                    seen = TRUE;
                }
            }
        }
    }
    rect->x1 = rect->x0 + width;
    rect->y1 = rect->y0 + height + halfGap;
    if (isMain)
    {
        rect->y1 += gaps;
    }
    if (MAIN_WindowWidth < rect->x1)
    {                                                       // a submenu goes to the left of the main menu
        shiftX = isMain ? (MAIN_WindowWidth - rect->x1) : (catPmMainRect.x0 - rect->x1);
        rect->x0 += shiftX;
        rect->x1 += shiftX;
    }
    else
    {
        shiftX = 0;
    }
    if (MAIN_WindowHeight < rect->y1)
    {
        shiftY = MAIN_WindowHeight - rect->y1;
        rect->y0 += shiftY;
        rect->y1 += shiftY;
    }
    else
    {
        shiftY = 0;
    }
    for (index = 0; index < count; index++)
    {
        item = &items[index];
        if (!item->visible)
        {
            continue;
        }
        if (isMain)
        {
            item->rect.y0 += groupShift[item->group];
            item->rect.y1 += groupShift[item->group];
        }
        item->rect.x1 = item->rect.x0 + width;
        item->rect.x0 += shiftX;
        item->rect.x1 += shiftX;
        item->rect.y0 += shiftY;
        item->rect.y1 += shiftY;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuLayoutAll                            0x004eb3e0
    Description : The main menu at the mouse, the selected row's submenu beside it.
----------------------------------------------------------------------------*/
void cat_popupMenuLayoutAll(void)
{
    CatPopupItem *row, *items;
    sdword submenu, count;
    rectangle dummy;
    bool32 forceCheck;

    cat_popupMenuLayout(catPmOpenX + CAT_PM_MAIN_DX, catPmOpenY + CAT_PM_MAIN_DY, catPmItems, CAT_PM_NumberItems,
                        &catPmMainRect, &catPmMainCheckCol, &catPmMainKeyCol, FALSE, TRUE);
    if ((catPmSelected < 0) || (catPmSelected >= CAT_PM_NumberItems))
    {
        return;
    }
    row = &catPmItems[catPmSelected];
    submenu = row->submenu;
    if (submenu == 0)
    {
        return;
    }
    // CAT: the exe lays out each of the 13 submenus by its row index; the row's submenu number gives the same pairs
    //      (0x0E tactics, 0x0F formations, 0x1C Leech, 0x12 sensors view, 0x10 focus, 0x11 view, 0x13 names,
    //      0x08 waypoint type, 0x21 / 0x22 mimic, 0x15 / 0x16 / 0x17 alliances); the check column is forced for
    //      tactics, formations, Leech, sensors view and view
    cat_popupMenuGetSubmenu(submenu, &items, &count, &dummy);
    forceCheck = (submenu == CAT_PMS_Tactics) || (submenu == CAT_PMS_Formations) || (submenu == CAT_PMS_LeechType) ||
                 (submenu == CAT_PMS_SensorsView) || (submenu == CAT_PMS_View);
    cat_popupMenuLayout(row->rect.x1 + CAT_PM_SUB_DX, row->rect.y0 + CAT_PM_SUB_DY, items, count,
                        &catPmSubRect[submenu], &catPmSubCheckCol[submenu], &catPmSubKeyCol[submenu], forceCheck,
                        FALSE);
}

void cat_popupMenuStartup(void)                                                            // 0x004eb7e0
{
    catPmFontLarge = frFontRegister("arial_15.hff");
    catPmFontSmall = frFontRegister("arial_12.hff");
    catPmOpen = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuOpen                                 0x004eb810
    Description : Opens the menu at the mouse: for the selection's orders (target NULL) or the special actions on the
                  clicked objects; the first selectable row is selected.
----------------------------------------------------------------------------*/
void cat_popupMenuOpen(SpaceObj *target, MaxSelection *selection)
{
    sdword index;

    if (playPackets || (catPmOpen && (target == catPmTarget)))
    {
        return;
    }
    catPmOpen = TRUE;
    catPmOpenX = mouseCursorX();
    catPmOpenY = mouseCursorY();
    catPmTarget = target;
    if (selection == NULL)
    {
        catPmTargets.numShips = 0;
    }
    else
    {
        for (index = 0; index < selection->numShips; index++)
        {
            catPmTargets.ShipPtr[index] = selection->ShipPtr[index];
        }
        catPmTargets.numShips = selection->numShips;
    }
    catPmSpecialTarget.numShips = 0;
    if (target == NULL)
    {
        cat_popupMenuBuildSelectionItems();
    }
    else
    {
        cat_popupMenuBuildTargetItems();
    }
    catPmSelectFrom(0);
    catPmSubSelected = -1;
    tutGameMessage("Game_PopupOpen");
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuBuildSelectionItems                  0x004eb940
    Description : The rows of the selection's orders: move / waypoints / hyperspace, dock, retire, cancel, tactics,
                  formations, focus, views, the sensors view, the ship's name, the single-player hyperspace and the
                  multiplayer alliance rows.
----------------------------------------------------------------------------*/
void cat_popupMenuBuildSelectionItems(void)
{
    MaxSelection selection;
    sdword tactics[3] = { 0, 0, 0 };
    sdword formations[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };   // the formation types 0..6, none, a parade
    bool32 selected = FALSE, mpHyperspace = FALSE, cancelMove = FALSE, canMove = FALSE, moving = FALSE;
    bool32 removeWaypoint = FALSE, endWaypoints = FALSE, cancelWaypoints = FALSE, waypointType = FALSE;
    bool32 dock = FALSE, retire = FALSE, cancel = FALSE, setTactics = FALSE, formation = FALSE, upgrade = FALSE;
    sdword own = 0, index, count, player;
    Ship *ship;
    CommandToDo *command;
    Player *other;

    if (!singlePlayerGame && FALSE)
    {
        // CAT-TODO(stage 5 multiplayer options): the multiplayer hyperspace row needs bit 0x4000 of the game options
        //      word 0x008860ec (the Cataclysm tpGameCreated layout); it counts as clear, so the row stays hidden
        cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
        cat_selMakeShipsMobileNotBusy((SelectCommand *)&selection);
        makeShipsControllable((SelectCommand *)&selection, 10);
        makeSelectionHyperspaceCapable((SelectCommand *)&selection);
        mpHyperspace = (selection.numShips != 0);
    }
    if (piePointSpecType == PIE_SPEC_None)
    {
        if (selSelected.numShips != 0)
        {
            selected = TRUE;
            for (index = 0; index < selSelected.numShips; index++)
            {
                ship = selSelected.ShipPtr[index];
                if (ship->shipOrDerelict.catDisabled || (ship->playerowner != universe.curPlayerPtr))
                {
                    continue;
                }
                own++;
                if ((cat_shipCanBeHalted(ship, TRUE) &&
                     (getShipAndItsCommand(&universe.mainCommandLayer, ship) != NULL)) ||
                    (ship->catUpgrading == TRUE))
                {
                    cancel = TRUE;
                }
                if (cat_shipCanSetTactics(ship, TRUE))
                {
                    setTactics = TRUE;
                    tactics[ship->tacticstype] = TRUE;
                }
                if (cat_shipCanMove(ship, TRUE))
                {
                    moving = TRUE;
                    canMove = TRUE;
                }
                if (cat_shipCanDock(ship, TRUE))
                {
                    dock = TRUE;
                    retire = TRUE;
                }
                if (cat_shipCanBeHalted(ship, TRUE))
                {
                    command = ship->command;
                    formation = TRUE;
                    if (command == NULL)
                    {
                        formations[7] = TRUE;
                    }
                    else if (command->ordertype.order == COMMAND_MILITARY_PARADE)
                    {
                        formations[8] = TRUE;
                    }
                    else if (bitTest(command->ordertype.attributes, COMMAND_MASK_FORMATION) &&
                             (command->formation.formationtype < 7))
                    {
                        formations[command->formation.formationtype] = TRUE;
                    }
                    else
                    {                                       // CAT-FIX: the exe indexes its 9 words by any formation
                        formations[7] = TRUE;               //  type; the force-field ones (9, 10) count as none here
                    }
                }
                if (cat_shipIsUpgradable(ship))
                {
                    upgrade = TRUE;
                }
            }
            if (own > 1)
            {
                goto formationsKept;
            }
        }
    }
    else if (piePointSpecType > PIE_SPEC_None)
    {
        if (piePointSpecType < PIE_SPEC_BigGun)
        {
            cancelMove = TRUE;
        }
        else if (piePointSpecType == PIE_SPEC_Waypoint)
        {
            removeWaypoint = TRUE;
            cancelWaypoints = TRUE;
            endWaypoints = TRUE;
            waypointType = TRUE;
        }
    }
    formation = FALSE;
formationsKept:
    for (index = 0; index < CAT_PM_NumberItems; index++)
    {
        catPmItems[index].visible = FALSE;
        catPmItems[index].disabled = FALSE;
        catPmItems[index].checked = FALSE;
        catPmItems[index].flash = FALSE;
    }
    catPmItems[CAT_PM_SpecialAction].visible = selected;
    if (moving)
    {
        catPmItems[CAT_PM_Move].visible = TRUE;
    }
    else if (cancelMove)
    {
        catPmItems[CAT_PM_CancelMove].visible = TRUE;
    }
    if (mpHyperspace)
    {
        catPmItems[CAT_PM_Hyperspace].visible = TRUE;
    }
    if (canMove)
    {
        catPmItems[CAT_PM_StartWaypoints].visible = TRUE;
    }
    if (removeWaypoint)
    {
        catPmItems[CAT_PM_RemoveLastWaypoint].visible = TRUE;
    }
    if (endWaypoints)
    {
        catPmItems[CAT_PM_EndWaypoints].visible = TRUE;
    }
    if (cancelWaypoints)
    {
        catPmItems[CAT_PM_CancelWaypoints].visible = TRUE;
    }
    if (waypointType)
    {                                                       // the waypoint mode checked
        catPmItems[CAT_PM_WaypointType].visible = TRUE;
        for (index = 0; index < 3; index++)
        {
            catPmWaypointType[index].disabled = FALSE;
            catPmWaypointType[index].flash = FALSE;
            catPmWaypointType[index].checked = FALSE;
        }
        if (catMrWaypointMode == 0)
        {
            catPmWaypointType[0].checked = TRUE;
            catPmWaypointType[0].visible = TRUE;
        }
        else if (catMrWaypointMode == 1)
        {
            catPmWaypointType[2].checked = TRUE;
            catPmWaypointType[2].visible = TRUE;
        }
        else if (catMrWaypointMode == 2)
        {
            catPmWaypointType[1].checked = TRUE;
            catPmWaypointType[1].visible = TRUE;
        }
    }
    if (dock)
    {
        catPmItems[CAT_PM_Dock].visible = TRUE;
    }
    if (retire)
    {
        catPmItems[CAT_PM_Retire].visible = TRUE;
    }
    if (cancel)
    {
        catPmItems[CAT_PM_Cancel].visible = TRUE;
    }
    if (setTactics)
    {                                                       // the tactics submenu: the one tactic in use checked
        count = 0;
        for (index = 0; index < 3; index++)
        {
            if (tactics[index] == TRUE)
            {
                count++;
            }
        }
        catPmItems[CAT_PM_Tactics].visible = TRUE;
        if (catPmSelected == CAT_PM_Tactics)
        {
            for (index = 0; index < 3; index++)
            {
                catPmTactics[index].flash = FALSE;
                catPmTactics[index].disabled = FALSE;
                catPmTactics[index].checked = FALSE;
            }
            cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
            cat_selMakeShipsNotBigShips((SelectCommand *)&selection);
            if (selection.numShips == 0)
            {                                               // big ships cannot be evasive
                catPmTactics[0].disabled = TRUE;
            }
            if (count == 1)
            {
                for (index = 0; index < 3; index++)
                {
                    if (tactics[index] == TRUE)
                    {
                        catPmTactics[index].checked = TRUE;
                    }
                }
            }
        }
    }
    if (formation)
    {                                                       // the formations submenu: the one formation checked
        count = 0;
        for (index = 0; index < 9; index++)
        {
            if (formations[index] == TRUE)
            {
                count++;
            }
        }
        catPmItems[CAT_PM_Formations].visible = (count > 0);
        if (catPmSelected == CAT_PM_Formations)
        {
            for (index = 0; index < 8; index++)
            {
                catPmFormations[index].flash = FALSE;
                catPmFormations[index].disabled = FALSE;
                catPmFormations[index].checked = FALSE;
            }
            if (count == 1)
            {
                for (index = 0; index < 7; index++)
                {
                    if (formations[index] == TRUE)
                    {
                        catPmFormations[index].checked = TRUE;
                    }
                }
                if (formations[8] == TRUE)
                {
                    catPmFormations[7].checked = TRUE;
                }
            }
            catPmFormations[7].visible = (MothershipOrCarrierIndexInSelection((SelectCommand *)&selSelected) != -1);
        }
    }
    if (upgrade)
    {
        catPmItems[CAT_PM_Upgrade].visible = TRUE;
        catPmItems[CAT_PM_Upgrade].flash = TRUE;
        catPmItems[CAT_PM_Upgrade].disabled = FALSE;
        catPmItems[CAT_PM_Upgrade].text = strGetString((universe.curPlayerPtr->race == 1) ?
                                                       strMenuUpgradeBeastShipsDesc : strMenuUpgradeSectShipsDesc);
    }
    catPmFocus[0].visible = (selSelected.numShips > 0);
    catPmItems[CAT_PM_Focus].visible = TRUE;
    catPmFocus[1].visible = TRUE;
    catPmFocus[2].visible = TRUE;
    catPmItems[CAT_PM_View].visible = TRUE;
    catPmView[1].visible = !smSensorsActive;
    catPmView[0].visible = smSensorsActive;
    catPmView[5].checked = catShipPrintOn;
    catPmView[6].checked = cat_mrTacticalOverlayGet();
    catPmView[7].checked = universe.mainCameraCommand.ccMode & CCMODE_PILOT_SHIP;
    if ((selSelected.numShips == 1) && cat_shipNamesShipIsNameable(selSelected.ShipPtr[0]))
    {                                                       // the ship's name; the set-name submenu
        catPmItems[CAT_PM_ShipName].visible = TRUE;
        catPmItems[CAT_PM_ShipName].disabled = TRUE;
        catPmItems[CAT_PM_ShipName].text = cat_shipNamesGetName(selSelected.ShipPtr[0]);
        if (!singlePlayerGame || (selSelected.ShipPtr[0]->shiptype != sMothership))
        {
            catPmItems[CAT_PM_SetShipName].visible = TRUE;
            if (catPmItems[CAT_PM_SetShipName].hover == TRUE)
            {
                count = cat_shipNamesCountFree(selSelected.ShipPtr[0]->shiptype);
                for (index = 0; (index < count) && (index < 50); index++)
                {
                    catPmShipNames[index].disabled = FALSE;
                    catPmShipNames[index].visible = TRUE;
                    catPmShipNames[index].flash = FALSE;
                    catPmShipNames[index].group = 8;
                    catPmShipNames[index].submenu = 0;
                    catPmShipNames[index].text = cat_shipNamesGetFreeName(selSelected.ShipPtr[0]->shiptype, index);
                }
                catPmShipNameCount = min(cat_shipNamesCountFree(selSelected.ShipPtr[0]->shiptype), 50);
            }
        }
        else
        {
            catPmItems[CAT_PM_SetShipName].visible = FALSE;
        }
    }
    else
    {
        catPmItems[CAT_PM_ShipName].visible = FALSE;
        catPmItems[CAT_PM_SetShipName].visible = FALSE;
        catPmShipNameCount = 0;
    }
    cat_popupMenuSetupSensorsItems();
    if (cat_spPlayerCanHyperspace() == 0)
    {
        catPmItems[CAT_PM_SPHyperspace].visible = FALSE;
        catPmItems[CAT_PM_SPHyperspace].disabled = TRUE;
        catPmItems[CAT_PM_SPHyperspace].flash = FALSE;
        catPmItems[CAT_PM_SPHyperspace].checked = FALSE;
    }
    else
    {
        catPmItems[CAT_PM_SPHyperspace].visible = TRUE;
        catPmItems[CAT_PM_SPHyperspace].disabled = FALSE;
        catPmItems[CAT_PM_SPHyperspace].flash = TRUE;
        catPmItems[CAT_PM_SPHyperspace].checked = FALSE;
        catPmItems[CAT_PM_SPHyperspace].text = cat_spMissionFlagValue();
    }
    // CAT-TODO(stage 5 multiplayer options): the alliance rows need bit 0x10 of the game options word 0x008860ec (the
    //      Cataclysm tpGameCreated layout); it counts as clear here, so the three rows stay hidden as the exe with it
    //      clear
    if (!multiPlayerGame || TRUE || (universe.curPlayerPtr == NULL) ||
        (universe.curPlayerPtr->playerState == PLAYER_DEAD))
    {
        catPmItems[CAT_PM_AllianceForm].visible = FALSE;
        catPmItems[CAT_PM_AllianceBreak].visible = FALSE;
        catPmItems[CAT_PM_TransferRUs].visible = FALSE;
    }
    else
    {
        count = 0;
        for (player = 0; player < universe.numPlayers; player++)
        {                                                   // the players to ally with
            other = &universe.players[player];
            catPmAllianceForm[player].visible = FALSE;
            if ((player != universe.curPlayerIndex) && (other->playerState != PLAYER_DEAD) &&
                !allianceArePlayersAllied(universe.curPlayerPtr, other))
            {
                catPmAllianceForm[count].disabled = FALSE;
                catPmAllianceForm[count].visible = TRUE;
                catPmAllianceForm[count].flash = FALSE;
                catPmAllianceForm[count].submenu = 0;
                catPmAllianceForm[count].text = playerNames[player];
                count++;
            }
        }
        catPmItems[CAT_PM_AllianceForm].visible = (count != 0);
        catPmAllianceFormCount = count;
        count = 0;
        for (player = 0; player < universe.numPlayers; player++)
        {                                                   // the allies: break with, send RUs to
            other = &universe.players[player];
            catPmAllianceBreak[player].visible = FALSE;
            catPmTransferRUs[player].visible = FALSE;
            if ((player != universe.curPlayerIndex) && (other->playerState != PLAYER_DEAD) &&
                allianceArePlayersAllied(universe.curPlayerPtr, other))
            {
                catPmAllianceBreak[count].disabled = FALSE;
                catPmTransferRUs[count].disabled = FALSE;
                catPmAllianceBreak[count].visible = TRUE;
                catPmTransferRUs[count].visible = TRUE;
                catPmAllianceBreak[count].flash = FALSE;
                catPmTransferRUs[count].flash = FALSE;
                catPmAllianceBreak[count].submenu = 0;
                catPmTransferRUs[count].submenu = 0;
                catPmAllianceBreak[count].text = playerNames[player];
                catPmTransferRUs[count].text = playerNames[player];
                count++;
            }
        }
        catPmAllianceBreakCount = count;
        catPmTransferRUsCount = count;
        catPmItems[CAT_PM_AllianceBreak].visible = (count != 0);
        catPmItems[CAT_PM_TransferRUs].visible = (count != 0);
    }
    for (index = CAT_PM_NoSpecialAction; index < CAT_PM_NumberItems; index++)
    {                                                       // the special rows belong to the target menu
        catPmItems[index].visible = FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuOpenMimicShips / Players / Leech      0x004ec220 / 0x004ec3c0 / 0x004ec560
    Description : The keys of the mimic and Leech choices open the menu (if not open for them) with that row's
                  submenu selected.
----------------------------------------------------------------------------*/
static void catPmOpenForKeyCommand(sdword row, CatPopupItem *subItems, sdword subCount)
{
    sdword start;
    bool32 wrapped;

    if (!playPackets && (!catPmOpen || (catPmTarget != CAT_PM_TARGET_KEYCOMMAND)))
    {
        catPmOpen = TRUE;
        catPmOpenX = mouseCursorX();
        catPmOpenY = mouseCursorY();
        catPmTarget = CAT_PM_TARGET_KEYCOMMAND;
        catPmTargets.numShips = 0;
        catPmSpecialTarget.numShips = 0;
        cat_popupMenuBuildTargetItems();
        catPmSelectFrom(0);
        catPmSubSelected = -1;
        tutGameMessage("Game_PopupOpen");
    }
    catPmSelectFrom(row);
    catPmSubSelected = 0;
    start = 0;
    wrapped = FALSE;
    while ((subCount > 0) && ((subItems[catPmSubSelected].disabled == TRUE) || !subItems[catPmSubSelected].visible) &&
           !wrapped)
    {
        catPmSubSelected++;
        if (catPmSubSelected >= subCount)
        {
            catPmSubSelected = 0;
            wrapped = TRUE;
        }
        else if (catPmSubSelected == start)
        {
            wrapped = TRUE;
        }
    }
}

void cat_popupMenuOpenMimicShips(void)                                                     // 0x004ec220
{
    // CAT: the exe tests the submenu rows' count before building them (the count of the last build)
    catPmOpenForKeyCommand(CAT_PM_SelectMimicItem, catPmMimicItems, catPmMimicItemCount);
}

void cat_popupMenuOpenMimicPlayers(void)                                                   // 0x004ec3c0
{
    catPmOpenForKeyCommand(CAT_PM_SelectMimicType, catPmMimicPlayers, catPmMimicPlayerCount);
}

void cat_popupMenuOpenLeech(void)                                                          // 0x004ec560
{
    catPmOpenForKeyCommand(CAT_PM_SelectLeechType, catPmLeech, 4);
}

void cat_popupMenuClose(void)                                                              // 0x004ec6f0
{
    catPmOpen = FALSE;
}

bool32 cat_popupMenuIsOpen(void)                                                           // 0x004ec710
{
    return catPmOpen;
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuMouseInside                          0x004ec720
    Description : Is the mouse over the main menu or the selected row's submenu (with the frame's margin)?
----------------------------------------------------------------------------*/
bool32 cat_popupMenuMouseInside(void)
{
    rectangle *rect;

    if (!catPmOpen)
    {
        return FALSE;
    }
    if (cat_popupMenuMouseInRect(&catPmMainRect))
    {
        return TRUE;
    }
    // CAT: the exe tests the submenus of the rows 0x0E, 0x12, 0x0F, 0x1C, 0x10, 0x11, 0x13, 0x08, 0x21, 0x22 and 0x15;
    //      for the rows 0x0C / 0x0D (retire, cancel) it tests the alliance-break / transfer rectangles (submenus 12 /
    //      13, by number), and the rows 0x16 / 0x17 have none: kept as the exe
    switch (catPmSelected)
    {
        case CAT_PM_Tactics:            rect = &catPmSubRect[CAT_PMS_Tactics]; break;
        case CAT_PM_SensorsView:        rect = &catPmSubRect[CAT_PMS_SensorsView]; break;
        case CAT_PM_Formations:         rect = &catPmSubRect[CAT_PMS_Formations]; break;
        case CAT_PM_SelectLeechType:    rect = &catPmSubRect[CAT_PMS_LeechType]; break;
        case CAT_PM_Focus:              rect = &catPmSubRect[CAT_PMS_Focus]; break;
        case CAT_PM_View:               rect = &catPmSubRect[CAT_PMS_View]; break;
        case CAT_PM_SetShipName:        rect = &catPmSubRect[CAT_PMS_ShipNames]; break;
        case CAT_PM_WaypointType:       rect = &catPmSubRect[CAT_PMS_WaypointType]; break;
        case CAT_PM_SelectMimicType:    rect = &catPmSubRect[CAT_PMS_MimicPlayers]; break;
        case CAT_PM_SelectMimicItem:    rect = &catPmSubRect[CAT_PMS_MimicItems]; break;
        case CAT_PM_AllianceForm:       rect = &catPmSubRect[CAT_PMS_AllianceForm]; break;
        case CAT_PM_Retire:             return cat_popupMenuMouseInRect(&catPmSubRect[CAT_PMS_AllianceBreak]);
        case CAT_PM_Cancel:             return cat_popupMenuMouseInRect(&catPmSubRect[CAT_PMS_TransferRUs]);
        default:                        return FALSE;
    }
    return cat_popupMenuMouseInRect(rect);
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuDrawFrame                            0x004ec9a0
    Description : A menu's background (translucent under GL) and the eight border icons round it.
----------------------------------------------------------------------------*/
void cat_popupMenuDrawFrame(rectangle *rect, bool32 fill, sdword margin)
{
    rectangle box;

    if (fill)
    {
        box.x0 = rect->x0 - margin;
        box.y0 = rect->y0 - margin;
        box.x1 = rect->x1 + margin;
        box.y1 = rect->y1 + margin;
        // CAT: the exe fills it solid on the software renderer (RGLtype 0x00b3a700 == 0); this tree draws with GL
        primRectTranslucent2(&box, CAT_PM_COLOUR_FRAME_GL);
    }
    cat_overlayIconDraw(CAT_OI_PopupMenu_BorderCorner_TopLeft, rect->y0 - margin, rect->x0 - margin, margin, margin,
                        0xffffffff);
    cat_overlayIconDraw(CAT_OI_PopupMenu_BorderCorner_TopRight, rect->y0 - margin, rect->x1, margin, margin, 0xffffffff);
    cat_overlayIconDraw(CAT_OI_PopupMenu_BorderCorner_BottomRight, rect->y1, rect->x1, margin, margin, 0xffffffff);
    cat_overlayIconDraw(CAT_OI_PopupMenu_BorderCorner_BottomLeft, rect->y1, rect->x0 - margin, margin, margin,
                        0xffffffff);
    cat_overlayIconDraw(CAT_OI_PopupMenu_BorderEdge_Top, rect->y0 - margin, rect->x0, rect->x1 - rect->x0, margin,
                        0xffffffff);
    cat_overlayIconDraw(CAT_OI_PopupMenu_BorderEdge_Bottom, rect->y1, rect->x0, rect->x1 - rect->x0, margin,
                        0xffffffff);
    cat_overlayIconDraw(CAT_OI_PopupMenu_BorderEdge_Left, rect->y0, rect->x0 - margin, margin, rect->y1 - rect->y0,
                        0xffffffff);
    cat_overlayIconDraw(CAT_OI_PopupMenu_BorderEdge_Right, rect->y0, rect->x1, margin, rect->y1 - rect->y0,
                        0xffffffff);
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuDrawItems                            0x004ecb30
    Description : A menu's visible rows: the hover bar of the selected row, the check mark, the text (grey when
                  disabled, flashing red-yellow when flagged, the title green), the key or the submenu arrow.
----------------------------------------------------------------------------*/
void cat_popupMenuDrawItems(CatPopupItem *items, sdword count, sdword checkCol, sdword keyCol, bool32 isMain)
{
    sdword checkWidth = checkCol ? catPmCheckWidth : 0;
    sdword index, halfGap, x;
    real32 t;
    color c;
    char keyString[256];
    CatPopupItem *item;

    for (index = 0; index < count; index++)
    {
        item = &items[index];
        if (!item->visible)
        {
            continue;
        }
        if (item->flash)
        {                                                   // a 5 s cycle red -> yellow -> red
            if (catPmFlashStart + CAT_PM_FLASH_PERIOD < taskTimeElapsed)
            {
                catPmFlashStart = taskTimeElapsed;
            }
            else if (taskTimeElapsed < catPmFlashStart)
            {
                catPmFlashStart = taskTimeElapsed;
            }
            t = (taskTimeElapsed - catPmFlashStart) / CAT_PM_FLASH_PERIOD;
            if (t > 0.5f)
            {
                t = (real32)(((double)t - 1.0) * -2.0);
            }
            else
            {
                t = t + t;
            }
            c = colBlend(CAT_PM_COLOUR_FLASH0, CAT_PM_COLOUR_FLASH1, t);
        }
        else
        {
            c = item->disabled ? CAT_PM_COLOUR_DISABLED : CAT_PM_COLOUR_NORMAL;
        }
        if (isMain && (index == 0))
        {
            c = CAT_PM_COLOUR_TITLE;
        }
        if ((isMain && (index == catPmSelected)) || (!isMain && (index == catPmSubSelected)))
        {                                                   // the hover bar: left end, middle, right end
            halfGap = catPmLineGap / 2;
            cat_overlayIconDraw(CAT_OI_PopupMenu_MouseOverLeftEnd, item->rect.y0 - halfGap, item->rect.x0 - catPmHoverPad,
                                catPmHoverEnd + catPmHoverPad, item->rect.y1 - item->rect.y0 + catPmLineGap, 0xffffffff);
            cat_overlayIconDraw(CAT_OI_PopupMenu_MouseOverMiddle, item->rect.y0 - halfGap, item->rect.x0 + catPmHoverEnd,
                                item->rect.x1 - 2 * catPmHoverEnd - item->rect.x0,
                                item->rect.y1 - item->rect.y0 + catPmLineGap, 0xffffffff);
            cat_overlayIconDraw(CAT_OI_PopupMenu_MouseOverRightEnd, item->rect.y0 - halfGap, item->rect.x1 - catPmHoverEnd,
                                catPmHoverEnd + catPmHoverPad, item->rect.y1 - item->rect.y0 + catPmLineGap, 0xffffffff);
        }
        if (item->checked)
        {
            cat_overlayIconDraw(CAT_OI_PopupMenu_SelectedIndicator, item->rect.y0, item->rect.x0, catPmCheckWidth,
                                item->rect.y1 - item->rect.y0, 0xffffffff);
        }
        rndAdditiveBlends(FALSE);
        fontPrint(item->rect.x0 + checkWidth, item->rect.y0, c,
                  (item->text == NULL) ? strGetString(item->strId) : item->text);
        x = item->rect.x0 + catPmKeyGap + checkWidth + keyCol;
        if (item->submenu == 0)
        {
            if (item->command != 0)
            {
                cat_kbCommandKeyString(item->command, keyString);
                fontPrint(x, item->rect.y0, c, keyString);
            }
        }
        else if (item->text == NULL)
        {                                                   // the submenu arrow (grey when disabled)
            // CAT: the exe reads the arrow's height from the main table's row of the same index (0x0088deb4 /
            //      0x0088debc); only main rows have submenus, so that is the row itself
            cat_overlayIconDraw(item->disabled ? CAT_OI_PopupMenu_SubMenuIndicator_Disabled :
                                CAT_OI_PopupMenu_SubMenuIndicator, item->rect.y0, x, catPmKeyMinWidth,
                                item->rect.y1 - item->rect.y0, 0xffffffff);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuFilterTargets                        0x004ecf60
    Description : Keeps the ships of the clicked objects that are allied (keepEnemies FALSE, the repair row) or that
                  are not (keepEnemies TRUE). [audit: the parameter was read inverted; the exe's 0x004ecf60 keeps
                  the allies for 0 and the non-allies for nonzero]
----------------------------------------------------------------------------*/
void cat_popupMenuFilterTargets(bool32 keepEnemies)
{
    sdword index = 0;
    Ship *ship;

    while (index < catPmTargets.numShips)
    {
        ship = catPmTargets.ShipPtr[index];
        if ((ship->header.objtype == OBJ_ShipType) &&
            (keepEnemies ? !allianceIsShipAlly(ship, universe.curPlayerPtr) : allianceIsShipAlly(ship, universe.curPlayerPtr)))
        {
            index++;
        }
        else
        {
            catPmTargets.numShips--;
            catPmTargets.ShipPtr[index] = catPmTargets.ShipPtr[catPmTargets.numShips];
        }
    }
}

// the clicked objects made attackable, else the special row's nearest target (NULL if neither)
static SelectCommand *catPmSpecialTargets(bool32 attackable, bool32 empOnly, bool32 nonForce)
{
    if (catPmTargets.numShips == 0)
    {
        return (catPmSpecialTarget.numShips > 0) ? &catPmSpecialTarget : NULL;
    }
    if (attackable)
    {
        cat_selMakeTargetsAttackable((SelectCommand *)&catPmTargets, empOnly);
    }
    if (nonForce)
    {
        MakeTargetsOnlyNonForceAttackTargets((SelectAnyCommand *)&catPmTargets, universe.curPlayerPtr);
    }
    return (catPmTargets.numShips > 0) ? (SelectCommand *)&catPmTargets : NULL;
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuExecuteSpecial                       0x004ecfe0
    Description : Runs a special row (0x18..0x30) through the cat_ui* commands of the main region.
    Return      : TRUE if the menu is to close
----------------------------------------------------------------------------*/
bool32 cat_popupMenuExecuteSpecial(sdword item)
{
    CatPopupItem *subItems;
    sdword subCount, index;
    rectangle subRect;
    bool32 harvested;
    Ship *ship;

    switch (item)
    {
        case CAT_PM_NoSpecialAction:
            return TRUE;
        case CAT_PM_Harvest:
            harvested = FALSE;
            for (index = 0; index < catPmTargets.numShips; index++)
            {
                if (harvested)
                {
                    return TRUE;
                }
                ship = catPmTargets.ShipPtr[index];
                if ((ship->header.objtype == OBJ_ShipType) && bitTest(ship->header.catFlags18, 0x8) &&
                    !cat_shipDetectedByPlayer(ship, universe.curPlayerIndex) &&
                    cat_mimicAppearsAsResource(ship, universe.curPlayerPtr))
                {                                           // a hidden mimic posing as a resource: its fake asteroid
                    mrHarvestResources((char *)cat_mimicGetFakeResource(ship), NULL);
                    harvested = TRUE;
                }
                else if ((ship->header.objtype > OBJ_BulletType) && (ship->header.objtype < OBJ_DerelictType))
                {
                    mrHarvestResources((char *)ship, NULL);
                    harvested = TRUE;
                }
            }
            if (harvested)
            {
                return TRUE;
            }
            mrHarvestResources(NULL, NULL);
            return TRUE;
        case CAT_PM_Repair:
            if (catPmTargets.numShips != 0)
            {
                cat_popupMenuFilterTargets(FALSE);
            }
            // CAT: the exe passes the (filtered) targets, or the nearest target, only when there are ships, else
            //      nothing (it does not even fall back to the nearest one)
            if (catPmTargets.numShips == 0)
            {
                if (catPmSpecialTarget.numShips > 0)
                {
                    cat_uiSpecialRepair(&catPmSpecialTarget);
                    return TRUE;
                }
            }
            else if (catPmTargets.numShips > 0)
            {
                cat_uiSpecialRepair((SelectCommand *)&catPmTargets);
                return TRUE;
            }
            return TRUE;
        case CAT_PM_Salvage:
            cat_uiSpecialSalvage(catPmSpecialTargets(FALSE, FALSE, FALSE));
            return TRUE;
        case CAT_PM_SelectLeechType:
        case CAT_PM_SelectMimicType:
        case CAT_PM_SelectMimicItem:
            cat_popupMenuGetSubmenu(catPmItems[catPmSelected].submenu, &subItems, &subCount, &subRect);
            catPmSubSelected = 0;
            if (!((subItems[0].disabled == FALSE) && subItems[0].visible))
            {
                cat_popupMenuKeyPress(ARRDOWN);
            }
            return FALSE;
        case CAT_PM_Leech:
            cat_uiSpecialLeech(catPmSpecialTargets(TRUE, FALSE, TRUE));
            return FALSE;
        case CAT_PM_Unleech:
            cat_uiUnleech();
            return TRUE;
        case CAT_PM_Link:
            cat_uiLink();
            return TRUE;
        case CAT_PM_Unlink:
            cat_uiUnlink();
            return TRUE;
        case CAT_PM_Unmimic:
            cat_uiUnmimic();
            return FALSE;
        case CAT_PM_ForceField:
            if (catPmTargets.numShips != 0)
            {
                MakeShipsFriendlyShips((SelectCommand *)&catPmTargets, universe.curPlayerPtr);
                if (catPmTargets.numShips > 0)
                {
                    cat_uiProtectFormation((SelectCommand *)&catPmTargets);
                    return FALSE;
                }
            }
            cat_uiProtectFormation(NULL);
            return FALSE;
        case CAT_PM_UnlinkForceField:
            cat_uiProtectFormationCancel();
            return FALSE;
        case CAT_PM_Cloak:
            cat_uiCloakShips();
            return TRUE;
        case CAT_PM_CancelCloak:
            cat_uiDecloakShips();
            return TRUE;
        case CAT_PM_LaunchSwarmers:
            cat_uiSpecialSwarm(catPmSpecialTargets(TRUE, FALSE, TRUE));
            return TRUE;
        case CAT_PM_CancelSwarmers:
            cat_uiReleaseHiveSwarm();
            return TRUE;
        case CAT_PM_FireBigGun:
            cat_uiFireSiegeCannon();
            return TRUE;
        case CAT_PM_CancelBigGun:
            cat_uiSiegeCannonCancel();
            return TRUE;
        case CAT_PM_BeastAttack:
            cat_uiSpecialInfect(catPmSpecialTargets(TRUE, FALSE, TRUE));
            return TRUE;
        case CAT_PM_EMP:
            cat_uiSpecialEMP(catPmSpecialTargets(TRUE, TRUE, TRUE));
            return TRUE;
        case CAT_PM_Ram:
            cat_uiSpecialRam(catPmSpecialTargets(TRUE, FALSE, FALSE));
            return TRUE;
        case CAT_PM_Repulse:
            cat_uiFireDreadnoughtCannon();
            return TRUE;
        case CAT_PM_FireMissiles:
            cat_popupMenuSpecialFireMissile();
            return TRUE;
        default:
            return FALSE;
    }
}

void cat_popupMenuSpecialFireMissile(void)                                                 // 0x004ed510
{
    cat_uiSpecialFireMissile(catPmSpecialTargets(TRUE, FALSE, FALSE));
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuGetSubmenu                           0x004ed550
    Description : A submenu's rows, their count and its rectangle.
----------------------------------------------------------------------------*/
void cat_popupMenuGetSubmenu(sdword submenu, CatPopupItem **items, sdword *count, rectangle *rect)
{
    switch (submenu)
    {
        case CAT_PMS_Tactics:       *items = catPmTactics; *count = 3; break;
        case CAT_PMS_Formations:    *items = catPmFormations; *count = 8; break;
        case CAT_PMS_SensorsView:   *items = catPmSensorsView; *count = 12; break;
        case CAT_PMS_LeechType:     *items = catPmLeech; *count = 4; break;
        case CAT_PMS_Focus:         *items = catPmFocus; *count = 4; break;
        case CAT_PMS_View:          *items = catPmView; *count = 8; break;
        case CAT_PMS_ShipNames:     *items = catPmShipNames; *count = catPmShipNameCount; break;
        case CAT_PMS_WaypointType:  *items = catPmWaypointType; *count = 3; break;
        case CAT_PMS_MimicPlayers:  *items = catPmMimicPlayers; *count = catPmMimicPlayerCount; break;
        case CAT_PMS_MimicItems:    *items = catPmMimicItems; *count = catPmMimicItemCount; break;
        case CAT_PMS_AllianceForm:  *items = catPmAllianceForm; *count = catPmAllianceFormCount; break;
        case CAT_PMS_AllianceBreak: *items = catPmAllianceBreak; *count = catPmAllianceBreakCount; break;
        case CAT_PMS_TransferRUs:   *items = catPmTransferRUs; *count = catPmTransferRUsCount; break;
        default:
            // CAT-FIX: the exe leaves the outputs unset for another number; an empty list here
            *items = catPmTactics;
            *count = 0;
            return;
    }
    *rect = catPmSubRect[submenu];
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuMimicSelect                          0x004ed8b0
    Description : The mimic row picked: the selected sMimics (and the sMCVs, for the rows of the base list) are
                  ordered to pose as it (the objects player 8: an asteroid).
----------------------------------------------------------------------------*/
bool32 cat_popupMenuMimicSelect(sdword item)
{
    CatMimicItem *row;
    ubyte player;
    sdword index = 0;
    Ship *ship;

    row = cat_mimicItemGet(cat_mimicGetCurrentPlayer(), catPmMimicAdded, item);
    player = cat_mimicGetCurrentPlayer();
    while (index < selSelected.numShips)
    {
        ship = selSelected.ShipPtr[index];
        if (!ship->shipOrDerelict.catDisabled && ((ship->shiptype == sMimic) || (ship->shiptype == sMCV)) &&
            ((item < cat_mimicItemCount(cat_mimicGetCurrentPlayer(), FALSE)) || (ship->shiptype != sMimic)))
        {                                                   // an sMimic may take a row of the base list only
            index++;
        }
        else
        {
            selSelected.numShips--;
            selSelected.ShipPtr[index] = selSelected.ShipPtr[selSelected.numShips];
        }
    }
    if (selSelected.numShips > 0)
    {
        cat_clWrapMimic(&universe.mainCommandLayer, (SelectCommand *)&selSelected,
                        (player == CAT_MIMIC_PLAYER_OBJECTS) + 1, row->type, cat_mimicGetCurrentPlayer(), row->index);
    }
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_selShipScreenRectDraw                         0x004ed9b0
    Description : An outline round a ship on the screen: its precise box (the 8 projected corners) when the LOD has
                  one, else its selection circle (never smaller than selMinSelectionRadius).
----------------------------------------------------------------------------*/
void cat_selShipScreenRectDraw(Ship *ship, color c)
{
    rectangle rect;
    real32 minX, maxX, minY, maxY, radius;
    real32 *corner;
    sdword index;

    if ((ship->impactable.collInfo.selCircleRadius <= 0.0f) || (ship->impactable.collInfo.selCircleDepth <= 0.0f))
    {
        return;
    }
    corner = (real32 *)ship->impactable.collInfo.precise;
    if ((corner != NULL) && ((udword)ship->obj.currentLOD <= ship->staticinfo->common.staticCollInfo.preciseSelection))
    {                                                       // the projected corners' extents
        minX = 1e21f;
        maxY = -1e21f;
        maxX = -1e21f;
        minY = 1e21f;
        for (index = 0; index < 8; index++, corner += 3)
        {
            if (corner[0] <= minX)
            {
                minX = corner[0];
            }
            if (maxY <= corner[1])
            {
                maxY = corner[1];
            }
            if (maxX <= corner[0])
            {
                maxX = corner[0];
            }
            if (corner[1] <= minY)
            {
                minY = corner[1];
            }
        }
        // CAT: the exe rounds to nearest through its double constant 0x0072a458 (1.5 * 2^52)
        rect.x0 = MAIN_WindowWidth / 2 + (sdword)lrint((real32)MAIN_WindowWidth * minX * 0.5f);
        rect.y0 = MAIN_WindowHeight / 2 - (sdword)lrint((real32)MAIN_WindowHeight * maxY * 0.5f);
        rect.x1 = MAIN_WindowWidth / 2 + (sdword)lrint((real32)MAIN_WindowWidth * maxX * 0.5f);
        rect.y1 = MAIN_WindowHeight / 2 - (sdword)lrint((real32)MAIN_WindowHeight * minY * 0.5f);
    }
    else
    {
        if (ship->header.objtype == OBJ_ShipType)
        {
            radius = ship->staticinfo->tacticalSelectionScale * ship->impactable.collInfo.selCircleRadius;
        }
        else
        {
            radius = ship->impactable.collInfo.selCircleRadius;
        }
        if (radius <= selMinSelectionRadius)
        {
            radius = selMinSelectionRadius;
        }
        rect.x0 = (sdword)lrint((ship->impactable.collInfo.selCircleX - radius) * (real32)MAIN_WindowWidth * 0.5f) + MAIN_WindowWidth / 2;
        rect.x1 = MAIN_WindowWidth / 2 + (sdword)lrint((radius + ship->impactable.collInfo.selCircleX) * (real32)MAIN_WindowWidth * 0.5f);
        rect.y0 = MAIN_WindowHeight / 2 - (sdword)lrint((radius + ship->impactable.collInfo.selCircleY) * (real32)MAIN_WindowHeight * 0.5f);
        rect.y1 = MAIN_WindowHeight / 2 - (sdword)lrint((ship->impactable.collInfo.selCircleY - radius) * (real32)MAIN_WindowHeight * 0.5f);
    }
    primRectOutline2(&rect, 1, c);
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuDrawTargetHighlights                 0x004edc70
    Description : Outlines the ships the selected row would order, and the clicked objects (green allies, red
                  enemies), outside the sensors manager.
----------------------------------------------------------------------------*/
void cat_popupMenuDrawTargetHighlights(void)
{
    MaxSelection ordered;
    SelectCommand *targets;
    bool32 add;
    sdword index;
    Ship *ship;
    color c;

    ordered.numShips = 0;
    if (smSensorsActive)
    {
        return;
    }
    for (index = 0; index < selSelected.numShips; index++)
    {
        ship = selSelected.ShipPtr[index];
        add = FALSE;
        switch (catPmSelected)
        {
            case CAT_PM_Upgrade:
                add = !ship->staticinfo->UpgradeByDocking && cat_shipIsUpgradable(ship);
                break;
            case CAT_PM_Move:
            case CAT_PM_StartWaypoints:
                add = cat_shipCanMove(ship, TRUE);
                break;
            case CAT_PM_Dock:
            case CAT_PM_Retire:
                add = cat_shipCanDock(ship, TRUE);
                break;
            case CAT_PM_Cancel:
                add = cat_shipCanBeHalted(ship, TRUE) && (getShipAndItsCommand(&universe.mainCommandLayer, ship) != NULL);
                break;
            case CAT_PM_Tactics:
                add = cat_shipCanSetTactics(ship, TRUE);
                break;
            case CAT_PM_Formations:
                add = cat_shipCanBeHalted(ship, TRUE);
                break;
            case CAT_PM_Harvest:
                add = cat_shipCanCollectResources(ship, TRUE);
                break;
            case CAT_PM_Repair:
                add = cat_shipCanRepair(ship);
                break;
            case CAT_PM_Salvage:
                add = cat_shipCanSalvage(ship);
                break;
            case CAT_PM_SelectLeechType:
            case CAT_PM_Leech:
                add = cat_shipCanLeech(ship, TRUE);
                break;
            case CAT_PM_Unleech:
                add = cat_shipCanUnleech(ship);
                break;
            case CAT_PM_Link:
                add = cat_shipHasUpgrade(ship, CAT_UPGRADE_Link);
                break;
            case CAT_PM_Unlink:
                add = cat_shipHasUpgrade(ship, CAT_UPGRADE_MimicSmall);
                break;
            case CAT_PM_SelectMimicType:
            case CAT_PM_SelectMimicItem:
                add = cat_shipHasUpgrade(ship, CAT_UPGRADE_Mimic);
                break;
            case CAT_PM_Unmimic:
                add = cat_shipHasUpgrade(ship, CAT_UPGRADE_Mimic) && cat_shipIsMimicking(ship);
                break;
            case CAT_PM_ForceField:
                // CAT: the exe adds the ship and then filters the whole list by the ability upgrades, adding none
                //      here (the add is undone for a ship without them)
                cat_selSelectionAddShip(&ordered, ship);
                cat_selMakeShipsHaveAbilityUpgrade((SelectCommand *)&ordered);
                break;
            case CAT_PM_UnlinkForceField:
                add = (ship->command != NULL) && bitTest(ship->command->ordertype.attributes, COMMAND_MASK_FORMATION) &&
                      ((ship->command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION) ||
                       (ship->command->formation.formationtype == FORCEFIELD_WALL_FORMATION));
                break;
            case CAT_PM_Cloak:
                add = cat_shipHasUpgrade(ship, 0x20000) && !cat_shipIsCloakedFully(ship);
                break;
            case CAT_PM_CancelCloak:
                add = cat_shipHasUpgrade(ship, 0x20000) && cat_shipIsCloakedFully(ship);
                break;
            case CAT_PM_LaunchSwarmers:
                add = cat_shipHasUpgrade(ship, 0x800);
                break;
            case CAT_PM_CancelSwarmers:
                add = cat_shipHasUpgrade(ship, 0x800) && cat_hiveFrigateSwarmReady(ship);
                break;
            case CAT_PM_FireBigGun:
                add = cat_mothershipHasBigGunModule(ship);
                break;
            case CAT_PM_CancelBigGun:
                add = cat_mothershipBigGunFiring(ship);
                break;
            case CAT_PM_BeastAttack:
                add = cat_shipCanUseBeastWeapon(ship);
                break;
            case CAT_PM_EMP:
                add = cat_shipHasUpgrade(ship, CAT_UPGRADE_ACVEMP);
                break;
            case CAT_PM_Ram:
                add = cat_shipHasUpgrade(ship, CAT_UPGRADE_RamFrigate);
                break;
            case CAT_PM_Repulse:
                add = cat_shipHasUpgrade(ship, CAT_UPGRADE_DreadnoughtCannon);
                break;
            case CAT_PM_FireMissiles:
                add = cat_acolyteCanFireLinkedWeapon(ship);
                break;
            default:
                break;
        }
        if (add)
        {
            cat_selSelectionAddShip(&ordered, ship);
        }
    }
    for (index = 0; index < ordered.numShips; index++)
    {
        cat_selShipScreenRectDraw(ordered.ShipPtr[index], CAT_PM_COLOUR_SELECTION);
    }
    if (catPmTarget != NULL)
    {
        targets = (catPmTargets.numShips != 0) ? (SelectCommand *)&catPmTargets : &catPmSpecialTarget;
        for (index = 0; index < targets->numShips; index++)
        {
            ship = targets->ShipPtr[index];
            if (ship->header.objtype != OBJ_ShipType)
            {
                continue;
            }
            if (allianceIsShipAlly(ship, universe.curPlayerPtr))
            {
                c = CAT_PM_COLOUR_TARGET_ALLY;
            }
            else if (!bitTest(ship->header.catFlags18, 0x8) || cat_shipDetectedByPlayer(ship, universe.curPlayerIndex))
            {
                c = CAT_PM_COLOUR_TARGET_ENEMY;
            }
            else if (!cat_mimicAppearsAsResource(ship, universe.curPlayerPtr) &&
                     cat_mimicAppearsAlliedTo(ship, universe.curPlayerPtr))
            {                                               // a hidden mimic posing as an ally
                c = CAT_PM_COLOUR_TARGET_ALLY;
            }
            else
            {
                continue;
            }
            cat_selShipScreenRectDraw(ship, c);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuDraw                                 0x004ee120
    Description : The menu, rebuilt and laid out every frame: the outlines, the main menu and the selected row's
                  submenu, in the menu font, without the depth test.
----------------------------------------------------------------------------*/
void cat_popupMenuDraw(void)
{
    fonthandle oldFont;
    sdword spaceWidth, spaceHeight, submenu, count;
    CatPopupItem *items;
    rectangle dummy;

    oldFont = fontMakeCurrent(catPmLargeFont ? catPmFontLarge : catPmFontSmall);
    cat_rndDepthTestEnable(FALSE);
    if (catPmOpen)
    {
        catUiSelfTestPopupDrawn();                  // HWI-TEST(M2-R6 ui self-test): a counter only
        spaceWidth = fontWidth(" ");
        catPmKeyGap = (sdword)((real32)spaceWidth * 4.0f);
        catPmLeftPad = (sdword)((real32)spaceWidth * 0.4f);
        catPmRightPad = (sdword)((real32)spaceWidth * 0.4f);
        catPmMargin = (sdword)((real32)spaceWidth * 3.0f);
        catPmKeyMinWidth = (sdword)((real32)spaceWidth * 3.0f);
        catPmCheckWidth = (sdword)((real32)spaceWidth * 3.0f);
        catPmHoverEnd = (sdword)((real32)spaceWidth * 3.0f);
        catPmHoverPad = (sdword)((real32)spaceWidth * 0.8f);
        spaceHeight = fontHeight(" ");
        catPmLineGap = (sdword)((real32)spaceHeight * 0.5f);
        catPmGroupGap = (sdword)((real32)spaceHeight * 0.6f);
        if (catPmTarget == NULL)
        {
            cat_popupMenuBuildSelectionItems();
        }
        else
        {
            cat_popupMenuBuildTargetItems();
        }
        cat_popupMenuLayoutAll();
        cat_popupMenuDrawTargetHighlights();
        cat_popupMenuDrawFrame(&catPmMainRect, TRUE, catPmMargin);
        cat_popupMenuDrawItems(catPmItems, CAT_PM_NumberItems, catPmMainCheckCol, catPmMainKeyCol, TRUE);
        if ((catPmSelected >= 0) && (catPmSelected < CAT_PM_NumberItems) &&
            ((submenu = catPmItems[catPmSelected].submenu) != 0))
        {
            cat_popupMenuGetSubmenu(submenu, &items, &count, &dummy);
            cat_popupMenuDrawFrame(&catPmSubRect[submenu], TRUE, catPmMargin);
            cat_popupMenuDrawItems(items, count, catPmSubCheckCol[submenu], catPmSubKeyCol[submenu], FALSE);
        }
    }
    fontMakeCurrent(oldFont);
    cat_rndDepthTestEnable(TRUE);
}

bool32 cat_popupMenuClickIfTarget(void)                                                    // 0x004ee5d0
{
    if (catPmTarget != NULL)
    {
        return cat_popupMenuClick();
    }
    return FALSE;
}

// the hovered rows of a submenu run their key commands; TRUE if one was hovered
static bool32 catPmRunHoveredCommands(CatPopupItem *items, sdword count, bool32 *close, bool32 closeValue)
{
    bool32 ran = FALSE;
    sdword index;

    for (index = 0; index < count; index++)
    {
        if (items[index].hover)
        {
            if (items[index].command != 0)
            {
                cat_kbCommandExecute(smSensorsActive != 0, items[index].command);
            }
            ran = TRUE;
            *close = closeValue;
        }
        else if (!closeValue)
        {
            *close = FALSE;
        }
    }
    return ran;
}

// the index of the n-th other player that is (allied) or is not allied, -1 if none
static sdword catPmNthPlayer(sdword n, bool32 allied)
{
    sdword player, found = 0, index = -1;

    for (player = 0; player < universe.numPlayers; player++)
    {
        if ((player != universe.curPlayerIndex) && (universe.players[player].playerState != PLAYER_DEAD) &&
            ((allianceArePlayersAllied(universe.curPlayerPtr, &universe.players[player]) != 0) == (allied != 0)))
        {
            if (found == n)
            {
                index = player;
            }
            found++;
        }
    }
    return index;
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuClick                                0x004ee5f0
    Description : A click while the menu is open: the hovered row of the open submenu (its key command, the ship
                  name, the mimic player or row, an alliance), else the hovered selectable main row; the menu closes
                  unless the row keeps it open and the mouse is inside it.
----------------------------------------------------------------------------*/
bool32 cat_popupMenuClick(void)
{
    bool32 done = FALSE, close = FALSE;
    sdword index;
    CatPopupItem *row;

    cat_popupMenuUpdateHover(TRUE);
    if (catPmItems[CAT_PM_Tactics].visible && (catPmSelected == CAT_PM_Tactics) && !catPmItems[CAT_PM_Tactics].disabled)
    {
        done = catPmRunHoveredCommands(catPmTactics, 3, &close, TRUE);
    }
    else if (catPmItems[CAT_PM_Formations].visible && (catPmSelected == CAT_PM_Formations) &&
             !catPmItems[CAT_PM_Formations].disabled)
    {
        done = catPmRunHoveredCommands(catPmFormations, 8, &close, TRUE);
    }
    else if (catPmItems[CAT_PM_SelectLeechType].visible && (catPmSelected == CAT_PM_SelectLeechType) &&
             !catPmItems[CAT_PM_SelectLeechType].disabled)
    {
        done = catPmRunHoveredCommands(catPmLeech, 4, &close, TRUE);
    }
    else if (catPmItems[CAT_PM_SensorsView].visible && (catPmSelected == CAT_PM_SensorsView) &&
             !catPmItems[CAT_PM_SensorsView].disabled)
    {                                                       // the sensors view stays open
        done = catPmRunHoveredCommands(catPmSensorsView, 12, &close, FALSE);
    }
    else if (catPmItems[CAT_PM_Focus].visible && (catPmSelected == CAT_PM_Focus) && !catPmItems[CAT_PM_Focus].disabled)
    {
        done = catPmRunHoveredCommands(catPmFocus, 4, &close, TRUE);
    }
    else if (catPmItems[CAT_PM_View].visible && (catPmSelected == CAT_PM_View) && !catPmItems[CAT_PM_View].disabled)
    {
        done = catPmRunHoveredCommands(catPmView, 8, &close, TRUE);
    }
    else if (catPmItems[CAT_PM_SetShipName].visible && (catPmSelected == CAT_PM_SetShipName) &&
             !catPmItems[CAT_PM_SetShipName].disabled)
    {
        if (catPmShipNameCount < 1)
        {
            goto mainRows;
        }
        for (index = 0; index < catPmShipNameCount; index++)
        {
            if (catPmShipNames[index].hover)
            {
                cat_shipNamesRename(selSelected.ShipPtr[0], index);
                close = TRUE;
                done = TRUE;
            }
        }
    }
    else if (catPmItems[CAT_PM_WaypointType].visible && (catPmSelected == CAT_PM_WaypointType) &&
             !catPmItems[CAT_PM_SetShipName].disabled)
    {                                                       // CAT: the exe tests the set-name row's disabled word here
        done = catPmRunHoveredCommands(catPmWaypointType, 3, &close, TRUE);
    }
    else if (catPmItems[CAT_PM_SelectMimicType].visible && (catPmSelected == CAT_PM_SelectMimicType) &&
             !catPmItems[CAT_PM_SelectMimicType].disabled && (catPmMimicPlayerCount > 0))
    {
        for (index = 0; index < catPmMimicPlayerCount; index++)
        {
            if (catPmMimicPlayers[index].hover)
            {
                if (cat_mimicPlayerAvailable((ubyte)index))
                {
                    cat_mimicSetCurrentPlayer((ubyte)index);
                }
                done = TRUE;
            }
            close = FALSE;
        }
    }
    else if (catPmItems[CAT_PM_SelectMimicItem].visible && (catPmSelected == CAT_PM_SelectMimicItem) &&
             !catPmItems[CAT_PM_SelectMimicItem].disabled && (catPmMimicItemCount > 0))
    {
        for (index = 0; index < catPmMimicItemCount; index++)
        {
            if (catPmMimicItems[index].hover)
            {
                close = cat_popupMenuMimicSelect(index);
                done = TRUE;
            }
        }
    }
    else if (catPmItems[CAT_PM_AllianceForm].visible && (catPmSelected == CAT_PM_AllianceForm) &&
             !catPmItems[CAT_PM_AllianceForm].disabled && (catPmAllianceFormCount > 0))
    {
        for (index = 0; index < catPmAllianceFormCount; index++)
        {
            if (catPmAllianceForm[index].hover)
            {
                allianceFormWith((udword)catPmNthPlayer(index, FALSE));
                close = TRUE;
                done = TRUE;
            }
        }
    }
    else if (catPmItems[CAT_PM_AllianceBreak].visible && (catPmSelected == CAT_PM_AllianceBreak) &&
             !catPmItems[CAT_PM_AllianceBreak].disabled && (catPmAllianceBreakCount > 0))
    {
        for (index = 0; index < catPmAllianceBreakCount; index++)
        {
            if (catPmAllianceBreak[index].hover)
            {
                allianceBreakWith((udword)catPmNthPlayer(index, TRUE));
                close = TRUE;
                done = TRUE;
            }
        }
    }
    else if (catPmItems[CAT_PM_TransferRUs].visible && (catPmSelected == CAT_PM_TransferRUs) &&
             !catPmItems[CAT_PM_TransferRUs].disabled && (catPmTransferRUsCount > 0))
    {
        for (index = 0; index < catPmTransferRUsCount; index++)
        {
            if (catPmTransferRUs[index].hover)
            {
                if (!smSensorsActive)
                {
                    gcRUTransferStart((uword)catPmNthPlayer(index, TRUE));
                }
                else
                {
                    smSensorsClose(NULL, NULL);
                }
                close = TRUE;
                done = TRUE;
            }
        }
    }
    if (done)
    {
        goto closeIt;
    }
mainRows:
    done = FALSE;
    for (index = 0; index < CAT_PM_NumberItems; index++)
    {
        row = &catPmItems[index];
        if (!done && row->hover && (row->submenu == 0) && (row->disabled == FALSE) && row->visible)
        {
            if (index < CAT_PM_NoSpecialAction)
            {
                if (row->command != 0)
                {
                    cat_kbCommandExecute(smSensorsActive != 0, row->command);
                }
            }
            else
            {
                cat_popupMenuExecuteSpecial(index);
            }
            close = !((index == CAT_PM_SpecialAction) || (index == CAT_PM_RemoveLastWaypoint));
            done = TRUE;
        }
    }
closeIt:
    if (close || !cat_popupMenuMouseInside())
    {
        cat_popupMenuClose();
    }
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuHitTestItems                         0x004eec90
    Description : Marks the row under the mouse as hovered (and selects it: the main row, skipping unselectable
                  ones, or the submenu row).
    Return      : TRUE if the mouse is over a row or inside the rectangle
----------------------------------------------------------------------------*/
bool32 cat_popupMenuHitTestItems(bool32 isMain, CatPopupItem *items, sdword count, rectangle *rect)
{
    sdword x = mouseCursorX(), y = mouseCursorY();
    bool32 hit = FALSE;
    sdword index;
    CatPopupItem *item;

    for (index = 0; index < count; index++)
    {
        item = &items[index];
        if (!item->visible)
        {
            continue;
        }
        if ((x < item->rect.x0 - catPmMargin) || (y < item->rect.y0 - catPmLineGap / 2) ||
            (item->rect.x1 + catPmMargin <= x) || (item->rect.y1 + catPmLineGap / 2 <= y) || hit)
        {
            item->hover = FALSE;
            continue;
        }
        hit = TRUE;
        item->hover = TRUE;
        if (!item->disabled)
        {
            if (!isMain)
            {
                catPmSubSelected = index;
            }
            else
            {
                catPmSelectFrom(index);
                catPmSubSelected = -1;
            }
        }
    }
    if ((mouseCursorX() < rect->x0 - catPmMargin) || (mouseCursorY() < rect->y0 - catPmMargin) ||
        (rect->x1 + catPmMargin <= mouseCursorX()) || (rect->y1 + catPmMargin <= mouseCursorY()))
    {
        return hit;
    }
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuUpdateHover                          0x004eee30
    Description : When the mouse moved (or forced): the selected row's submenu first, then the main menu.
----------------------------------------------------------------------------*/
void cat_popupMenuUpdateHover(bool32 force)
{
    bool32 moved = force || (catPmLastMouseX != mouseCursorX()) || (catPmLastMouseY != mouseCursorY());
    CatPopupItem *items;
    sdword count, submenu;
    rectangle dummy;

    if (cat_popupMenuIsOpen() && moved)
    {
        // CAT: the exe chains the 13 submenu rows (0x0E, 0x0F, 0x1C, 0x12, 0x10, 0x11, 0x13, 0x08, 0x21, 0x22,
        //      0x15, 0x16, 0x17); the row's submenu number gives the same pairs
        submenu = ((catPmSelected >= 0) && (catPmSelected < CAT_PM_NumberItems)) ? catPmItems[catPmSelected].submenu : 0;
        if (submenu != 0)
        {
            cat_popupMenuGetSubmenu(submenu, &items, &count, &dummy);
            if (cat_popupMenuHitTestItems(FALSE, items, count, &catPmSubRect[submenu]))
            {
                catPmLastMouseX = mouseCursorX();
                catPmLastMouseY = mouseCursorY();
                return;
            }
        }
        cat_popupMenuHitTestItems(TRUE, catPmItems, CAT_PM_NumberItems, &catPmMainRect);
    }
    catPmLastMouseX = mouseCursorX();
    catPmLastMouseY = mouseCursorY();
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuActivateSelected                     0x004ef010
    Description : Enter: the selected submenu row's command (or the selected main row's), the ship name, the mimic
                  choices.
    Return      : TRUE if the menu is to close
----------------------------------------------------------------------------*/
bool32 cat_popupMenuActivateSelected(void)
{
    sdword item = catPmSelected;
    CatPopupItem *items;
    sdword count, command;
    rectangle dummy;
    ubyte player;

    if (catPmSubSelected == -1)
    {
        if (item == -1)
        {
            return FALSE;
        }
        if (item < CAT_PM_NoSpecialAction)
        {
            if (catPmItems[item].command != 0)
            {
                cat_kbCommandExecute(smSensorsActive != 0, catPmItems[item].command);
            }
        }
        else
        {
            cat_popupMenuExecuteSpecial(item);
        }
        return !((item == CAT_PM_SpecialAction) || (item == CAT_PM_RemoveLastWaypoint));
    }
    switch (item)
    {
        case CAT_PM_SetShipName:
            cat_shipNamesRename(selSelected.ShipPtr[0], catPmSubSelected);
            return TRUE;
        case CAT_PM_SelectMimicType:
            player = (ubyte)catPmSubSelected;
            if (cat_mimicPlayerAvailable(player))
            {
                cat_mimicSetCurrentPlayer(player);
            }
            return FALSE;
        case CAT_PM_SelectMimicItem:
            return cat_popupMenuMimicSelect(catPmSubSelected);
        case CAT_PM_Tactics:
        case CAT_PM_Formations:
        case CAT_PM_SelectLeechType:
        case CAT_PM_SensorsView:
        case CAT_PM_Focus:
        case CAT_PM_View:
        case CAT_PM_WaypointType:
            cat_popupMenuGetSubmenu(catPmItems[item].submenu, &items, &count, &dummy);
            command = (catPmSubSelected < count) ? items[catPmSubSelected].command : 0;
            if (command != 0)
            {
                cat_kbCommandExecute(smSensorsActive != 0, command);
            }
            return TRUE;
        default:
            return FALSE;
    }
}

// a digit key's value, -1 for another key (the exe's VK codes 0x30..0x39; GoK keys are SDL scancodes, 1..9 then 0)
static sdword catPmDigit(udword key)
{
    if (key == ZEROKEY)
    {
        return 0;
    }
    if ((key >= ONEKEY) && (key <= NINEKEY))
    {
        return (sdword)(key - ONEKEY) + 1;
    }
    return -1;
}

// up / down in a submenu: the previous / next selectable row, wrapping
static void catPmSubStep(sdword step)
{
    CatPopupItem *items;
    sdword count, start;
    rectangle dummy;
    bool32 wrapped = FALSE;

    cat_popupMenuGetSubmenu(catPmItems[catPmSelected].submenu, &items, &count, &dummy);
    if (count <= 0)
    {
        return;
    }
    start = catPmSubSelected + step;
    if (start < 0)
    {
        start = count - 1;
    }
    if (start >= count)
    {
        start = 0;
    }
    catPmSubSelected = start;
    while ((items[catPmSubSelected].disabled == TRUE) || !items[catPmSubSelected].visible)
    {
        if (wrapped)
        {
            return;
        }
        catPmSubSelected += step;
        if (catPmSubSelected < 0)
        {
            catPmSubSelected = count - 1;
        }
        if (catPmSubSelected >= count)
        {
            catPmSubSelected = 0;
        }
        if (catPmSubSelected == start)
        {
            wrapped = TRUE;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuKeyPress                             0x004ef1c0
    Description : Keys while the menu is open: Enter runs the selection, the arrows move through the rows and into /
                  out of the submenu, the digits pick a mimic row or player, other keys run the special rows' (and the
                  Leech submenu's) key commands.
    Return      : TRUE if the key was used
----------------------------------------------------------------------------*/
bool32 cat_popupMenuKeyPress(udword key)
{
    CatPopupItem *items;
    sdword count, start, digit;
    rectangle subRect;
    bool32 matched = FALSE, wrapped;

    if (!cat_popupMenuIsOpen())
    {
        return FALSE;
    }
    if ((key == RETURNKEY) || (key == ENTERKEY))
    {
        if (cat_popupMenuActivateSelected())
        {
            cat_popupMenuClose();
        }
        return TRUE;
    }
    if ((key == ARRLEFT) || (key == ARRRIGHT))
    {                                                       // into the submenu on the side it opened, out again
        if ((catPmSelected == -1) || (catPmItems[catPmSelected].submenu == 0))
        {
            return TRUE;
        }
        cat_popupMenuGetSubmenu(catPmItems[catPmSelected].submenu, &items, &count, &subRect);
        if (((key == ARRLEFT) && (catPmMainRect.x0 <= subRect.x0)) ||
            ((key == ARRRIGHT) && (subRect.x0 <= catPmMainRect.x0)))
        {
            catPmSubSelected = -1;
            return TRUE;
        }
        catPmSubSelected = 0;
        if (!((items[0].disabled == FALSE) && items[0].visible))
        {
            cat_popupMenuKeyPress(ARRDOWN);
        }
        return TRUE;
    }
    if ((key == ARRUP) || (key == ARRDOWN))
    {
        if ((catPmSelected != -1) && (catPmItems[catPmSelected].submenu != 0) && (catPmSubSelected != -1))
        {
            catPmSubStep((key == ARRUP) ? -1 : 1);
            return TRUE;
        }
        start = catPmSelected + ((key == ARRUP) ? -1 : 1);
        if (start < 0)
        {
            start = CAT_PM_NumberItems - 1;
        }
        if (start >= CAT_PM_NumberItems)
        {
            start = 0;
        }
        catPmSelected = start;
        wrapped = FALSE;
        while (((catPmItems[catPmSelected].disabled == TRUE) || !catPmItems[catPmSelected].visible) && !wrapped)
        {
            catPmSelected += (key == ARRUP) ? -1 : 1;
            if (catPmSelected < 0)
            {
                catPmSelected = CAT_PM_NumberItems - 1;
            }
            if (catPmSelected >= CAT_PM_NumberItems)
            {
                catPmSelected = 0;
            }
            if (catPmSelected == start)
            {
                wrapped = TRUE;
            }
        }
        if ((catPmSelected >= CAT_PM_NoSpecialAction) && (catPmTargets.numShips == 0))
        {
            cat_popupMenuFindSpecialTarget(catPmSelected);
        }
        return TRUE;
    }
    digit = catPmDigit(key);
    if (digit >= 0)
    {
        if (catPmSelected == CAT_PM_SelectMimicItem)
        {
            if (catPmMimicItems[digit].visible)
            {
                cat_popupMenuMimicSelect(digit);
                cat_popupMenuClose();
            }
        }
        else if (catPmSelected == CAT_PM_SelectMimicType)
        {
            if (cat_mimicPlayerAvailable((ubyte)digit))
            {
                cat_mimicSetCurrentPlayer((ubyte)digit);
            }
            cat_popupMenuClose();
        }
        return TRUE;
    }
    if (catPmSelected == CAT_PM_SelectLeechType)
    {
        if (cat_popupMenuHotkeyScan(catPmLeech, FALSE, 0, 3, key, &matched))
        {
            cat_popupMenuClose();
            return TRUE;
        }
        return matched;
    }
    if ((catPmSelected < CAT_PM_NoSpecialAction) || (catPmSelected > CAT_PM_FireMissiles))
    {
        return FALSE;
    }
    if (cat_popupMenuHotkeyScan(catPmItems, TRUE, CAT_PM_NoSpecialAction, CAT_PM_FireMissiles, key, &matched))
    {
        cat_popupMenuClose();
        return TRUE;
    }
    return matched;
}

/*-----------------------------------------------------------------------------
    Name        : cat_popupMenuHotkeyScan                           0x004ef670
    Description : The rows first..last whose key command's key is 'key' (visible, selectable): a row with a
                  submenu selects it and opens it, another runs (the special rows through
                  cat_popupMenuExecuteSpecial).
    Outputs     : matched - TRUE once a row's key matched
    Return      : TRUE if a row ran and wants the menu closed
----------------------------------------------------------------------------*/
sdword cat_popupMenuHotkeyScan(CatPopupItem *items, bool32 isMain, sdword first, sdword last, udword key,
                               bool32 *matched)
{
    sdword item, count, result = FALSE;
    CatPopupItem *subItems;
    rectangle dummy;
    CatPopupItem *row;

    *matched = FALSE;
    for (item = first; item <= last; item++)
    {
        if (*matched)
        {
            return result;
        }
        row = &items[item];
        if ((row->disabled == FALSE) && (row->visible == TRUE) && (row->command != 0))
        {
            *matched = cat_kbCommandKeyMatch(smSensorsActive != 0, row->command, key, 0);
            if (!*matched)
            {
                continue;
            }
            if (row->submenu == 0)
            {
                if (!isMain)
                {
                    cat_kbCommandExecute(smSensorsActive != 0, row->command);
                    result = TRUE;
                }
                else
                {
                    result = cat_popupMenuExecuteSpecial(item);
                }
            }
            else
            {
                catPmSelectFrom(item);
                cat_popupMenuGetSubmenu(catPmItems[catPmSelected].submenu, &subItems, &count, &dummy);
                catPmSubSelected = 0;
                if ((count > 0) && ((subItems[0].disabled != FALSE) || !subItems[0].visible))
                {
                    cat_popupMenuKeyPress(ARRDOWN);
                }
            }
        }
    }
    return result;
}

void cat_popupMenuRemoveShip(Ship *ship)                                                   // 0x004ef840
{
    clRemoveShipFromSelection((SelectAnyCommand *)&catPmTargets, ship);
    clRemoveShipFromSelection((SelectAnyCommand *)&catPmSpecialTarget, ship);
}

/*=============================================================================
    HWI-TEST (M2-R6 mainrgn-popup), port diagnostic, not in the exe: /catUiTest <ticks> (harness mode ui). The
    headless Mission01 run of /oracleMission (one universe tick a frame, no input). At tick 16 the test views the
    game as the owner of Mission01's fleet (as /catHudTest), closes the sensors manager the headless start opens
    and selects that fleet through the selection API; at tick 24 the popup menu opens for the selection
    (cat_popupMenuOpen, no target); at tick 40, after the frames have drawn it, every cat_ui* command runs once
    through the command wraps (the special-target commands on an enemy ship, the force field round the selection),
    the menu is opened once more for an enemy target (a resource when Mission01 has no enemy ship yet) and closed; at tick 48 the game-stats screen begins
    (cat_gameStatsBegin pauses the universe) and its frames are counted in mrRegionDraw; after 8 frames
    "uitest: done ... result=PASS|FAIL" and the screen ends (cat_gameStatsEnd: back to the main menu), then the
    engine quits. The orders change the simulation: this run is no stream reference (the mission mode is).
=============================================================================*/
static sdword catUiTestTicks = 0;
static sdword catUiTestStep = 0;
static sdword catUiTestPopupDraws = 0;
static sdword catUiTestStatsFrames = 0;
static sdword catUiTestSelected = 0;
static sdword catUiTestVisible = 0;
static sdword catUiTestTargetVisible = 0;
static sdword catUiTestCommands = 0;
static bool32 catUiTestOpened = FALSE;
static bool32 catUiTestClosed = FALSE;
static bool32 catUiTestStatsBegun = FALSE;
#define CAT_UI_TEST_SELECT_TICK     16
#define CAT_UI_TEST_OPEN_TICK       24
#define CAT_UI_TEST_COMMAND_TICK    40
#define CAT_UI_TEST_STATS_TICK      48
#define CAT_UI_TEST_STATS_FRAMES    8

void catUiSelfTestSet(char *string)
{
    catUiTestTicks = (sdword)strtol(string, NULL, 10);
    if (catUiTestTicks <= 0)
    {
        catUiTestTicks = 400;
    }
    oracleSetMission(string);                       // the headless Mission01 run with the virtual clock
}

bool32 catUiSelfTestRequested(void)
{
    return (catUiTestTicks > 0);
}

// the cat_ui* commands, each once, on the selection (and an enemy ship as the special commands' target)
static void catUiTestCommandsRun(Ship *enemy)
{
    SelectCommand *targets = memAlloc(sizeofSelectCommand(1), "uitesttargets", 0);
    SelectCommand *protectees = memAlloc(sizeofSelectCommand(COMMAND_MAX_SHIPS), "uitestprotect", 0);
    MaxSelection saved = selSelected;
    sdword index;
    real32 damage = 0.0f;
    Ship *nearest = NULL;

    targets->numShips = (enemy != NULL) ? 1 : 0;
    targets->ShipPtr[0] = enemy;
    protectees->numShips = 0;
    for (index = 0; index < selSelected.numShips && index < COMMAND_MAX_SHIPS; index++)
    {
        protectees->ShipPtr[protectees->numShips++] = selSelected.ShipPtr[index];
    }
    if (selSelected.numShips > 0)
    {
        damage = cat_uiShipDamageFraction(selSelected.ShipPtr[0]);
        nearest = cat_uiFindNearestSpecialTarget(selSelected.ShipPtr[0], 0);
    }
    catUiTestCommands = 2;
    cat_uiSpecialTargetCommand(targets, 0);         catUiTestCommands++;
    cat_uiSpecialSalvage(targets);                  catUiTestCommands++;
    cat_uiSpecialRepair(targets);                   catUiTestCommands++;
    cat_uiSpecialLeech(targets);                    catUiTestCommands++;
    cat_uiSpecialEMP(targets);                      catUiTestCommands++;
    cat_uiSpecialSwarm(targets);                    catUiTestCommands++;
    cat_uiSpecialInfect(targets);                   catUiTestCommands++;
    cat_uiSpecialRam(targets);                      catUiTestCommands++;
    cat_uiSpecialFireMissile(targets);              catUiTestCommands++;
    cat_uiReleaseHiveSwarm();                       catUiTestCommands++;
    cat_uiSiegeCannonCancel();                      catUiTestCommands++;
    cat_uiCloakShips();                             catUiTestCommands++;
    cat_uiDecloakShips();                           catUiTestCommands++;
    cat_uiSetWaypointMode(0);                       catUiTestCommands++;
    cat_uiLink();                                   catUiTestCommands++;
    cat_uiUnlink();                                 catUiTestCommands++;
    cat_uiUnmimic();                                catUiTestCommands++;
    cat_uiProtectFormation(protectees);             catUiTestCommands++;
    cat_uiProtectFormationCancel();                 catUiTestCommands++;
    cat_uiFireSiegeCannon();                        catUiTestCommands++;
    if (piePointSpecMode != PSM_Idle)
    {                                               // the big gun's point mode (and the sensors manager) off again
        piePointModeOnOff(PIE_SPEC_None, NULL);
    }
    if (smSensorsActive)
    {
        smSensorsCloseForGood();
    }
    cat_uiFireDreadnoughtCannon();                  catUiTestCommands++;
    cat_uiSetLeechAction(7);                        catUiTestCommands++;
    cat_uiUnleech();                                catUiTestCommands++;
    cat_uiHaltSelection();                          catUiTestCommands++;
    selSelected = saved;
    dbgMessagef("uitest: commands tick=%u run=%d target=%s damage=%.3f nearest=%s selected=%d",
                (unsigned)universe.univUpdateCounter, (int)catUiTestCommands,
                (enemy == NULL) ? "none" : (enemy->header.objtype == OBJ_ShipType) ? ShipTypeToStr(enemy->shiptype) : "resource",
                (double)damage,
                (nearest != NULL) ? ShipTypeToStr(nearest->shiptype) : "none", (int)selSelected.numShips);
    memFree(protectees);
    memFree(targets);
}

void catUiSelfTestTick(void)
{
    Node *node;
    Ship *ship, *enemy = NULL;
    sdword former;

    if ((catUiTestStep == 0) && ((sdword)universe.univUpdateCounter >= CAT_UI_TEST_SELECT_TICK))
    {
        catUiTestStep = 1;
        former = universe.curPlayerIndex;
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {                                           // view as the fleet's owner (the headless start's player 1)
            ship = (Ship *)listGetStructOfNode(node);
            if ((ship->playerowner != NULL) && (ship->playerowner != universe.curPlayerPtr))
            {
                universe.curPlayerIndex = ship->playerowner->playerIndex;
                universe.curPlayerPtr = ship->playerowner;
                break;
            }
        }
        if (smSensorsActive)
        {
            smSensorsCloseForGood();
        }
        selSelected.numShips = 0;
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            ship = (Ship *)listGetStructOfNode(node);
            if ((ship->playerowner == universe.curPlayerPtr) && !bitTest(ship->header.flags, SOF_Dead) &&
                (selSelected.numShips < COMMAND_MAX_SHIPS))
            {
                selSelectionAddSingleShip(&selSelected, ship);
            }
        }
        ioUpdateShipTotals();
        catUiTestSelected = selSelected.numShips;
        dbgMessagef("uitest: selected tick=%u ships=%d player=%d (was %d) mrDisabled=%d",
                    (unsigned)universe.univUpdateCounter, (int)catUiTestSelected, (int)universe.curPlayerIndex,
                    (int)former, (int)mrDisabled);
    }
    if ((catUiTestStep == 1) && ((sdword)universe.univUpdateCounter >= CAT_UI_TEST_OPEN_TICK))
    {
        catUiTestStep = 2;
        cat_popupMenuOpen(NULL, &selSelected);
        catUiTestOpened = cat_popupMenuIsOpen();
        catUiTestVisible = cat_popupMenuVisibleCount();
        catUiTestPopupDraws = 0;
        dbgMessagef("uitest: popup open tick=%u open=%d rows=%d", (unsigned)universe.univUpdateCounter,
                    (int)catUiTestOpened, (int)catUiTestVisible);
    }
    if ((catUiTestStep == 2) && ((sdword)universe.univUpdateCounter >= CAT_UI_TEST_COMMAND_TICK))
    {
        catUiTestStep = 3;
        dbgMessagef("uitest: popup drawn frames=%d", (int)catUiTestPopupDraws);
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            ship = (Ship *)listGetStructOfNode(node);
            if ((ship->playerowner != NULL) && !allianceIsShipAlly(ship, universe.curPlayerPtr) &&
                !bitTest(ship->header.flags, SOF_Dead))
            {
                enemy = ship;
                break;
            }
        }
        if ((enemy == NULL) && (universe.ResourceList.head != NULL))
        {                                           // Mission01 has no enemy ship this early: a resource is the target
            enemy = (Ship *)listGetStructOfNode(universe.ResourceList.head);
        }
        catUiTestCommandsRun(enemy);
        cat_popupMenuClose();
        if (enemy != NULL)
        {                                           // the target menu once (an enemy ship's actions)
            cat_popupMenuOpen((SpaceObj *)enemy, &selSelected);
            catUiTestTargetVisible = cat_popupMenuVisibleCount();
            cat_popupMenuClose();
        }
        catUiTestClosed = !cat_popupMenuIsOpen();
        dbgMessagef("uitest: popup closed tick=%u closed=%d target rows=%d", (unsigned)universe.univUpdateCounter,
                    (int)catUiTestClosed, (int)catUiTestTargetVisible);
    }
    if ((catUiTestStep == 3) && ((sdword)universe.univUpdateCounter >= CAT_UI_TEST_STATS_TICK))
    {
        catUiTestStep = 4;
        cat_gameStatsBegin();
        catUiTestStatsBegun = cat_gameStatsIsActive();
        catUiTestStatsFrames = 0;
        dbgMessagef("uitest: game stats begin tick=%u active=%d paused=%d players=%d", (unsigned)universe.univUpdateCounter,
                    (int)catUiTestStatsBegun, (int)universePause, (int)universe.numPlayers);
    }
}

// counted by cat_popupMenuDraw while the menu is open, and by mrRegionDraw after each game-stats frame
void catUiSelfTestPopupDrawn(void)
{
    catUiTestPopupDraws++;
}

void catUiSelfTestStatsFrame(void)
{
    bool32 pass;

    if ((catUiTestTicks <= 0) || (catUiTestStep != 4))
    {
        return;
    }
    catUiTestStatsFrames++;
    if (catUiTestStatsFrames < CAT_UI_TEST_STATS_FRAMES)
    {
        return;
    }
    catUiTestStep = 5;
    pass = (catUiTestSelected > 0) && catUiTestOpened && (catUiTestVisible > 0) && (catUiTestPopupDraws > 0) &&
           (catUiTestCommands == 26) && catUiTestClosed && catUiTestStatsBegun;
    dbgMessagef("uitest: done selected=%d rows=%d popupframes=%d commands=%d targetrows=%d statsframes=%d result=%s",
                (int)catUiTestSelected, (int)catUiTestVisible, (int)catUiTestPopupDraws, (int)catUiTestCommands,
                (int)catUiTestTargetVisible, (int)catUiTestStatsFrames, pass ? "PASS" : "FAIL");
    cat_gameStatsEnd();
    dbgMessagef("uitest: game stats ended active=%d paused=%d", (int)cat_gameStatsIsActive(), (int)universePause);
    utyGameQuit(NULL, NULL);
}

// HWI-TEST(M2-R6 ui self-test)
sdword cat_popupMenuVisibleCount(void)
{
    sdword index, count = 0;

    for (index = 0; index < CAT_PM_NumberItems; index++)
    {
        if (catPmItems[index].visible)
        {
            count++;
        }
    }
    return count;
}

SpaceObj *cat_popupMenuTarget(void)
{
    return catPmTarget;
}
