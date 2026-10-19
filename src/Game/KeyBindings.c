/*=============================================================================
    Name    : KeyBindings.c
    Purpose : This file handles all of the logic for the key bindings

    Created 8/04/1999 by Drew Dunlop
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "KeyBindings.h"

#include "Debug.h"
#include "FEColour.h"
#include "FontReg.h"
#include "Globals.h"
#include "StringsOnly.h"
#include "StringSupport.h"
#include "Types.h"
#include "UIControls.h"
#include "File.h"
#include "Key.h"
#include "StatScript.h"
#include "prim2d.h"
#include "utility.h"

/*=============================================================================
    CAT (M2-R6 startup-keys): Cataclysm's key bindings. HW1's 19-command key table (kbBoundKeys, the per-language
    default tables, kbCanMapKey, the key pool list, kbCheckBindings, kbRestoreSavedSettings / kbSaveSettings) is gone:
    the exe binds its 150 key commands (1 .. 0x96) from default_keys.cfg (the install's, read only) and keys.cfg (the
    player's copy; cat_kbLoadKeyBindings 0x0054da10), up to three keys each with their SHIFT / ALT / CTRL state, and
    the options screen's list window rebinds them (kbListWindowCB 0x0049bca0, cat_kbSetNewKey 0x0054e410).

    HWI: the exe opens both files in the current directory (the install). default_keys.cfg is read from the data path
    (FF_HomeworldDataPath: the install) and keys.cfg is read and written in the user settings path
    (FF_UserSettingsPath), as GoK keeps every file the game writes out of the install.
=============================================================================*/

#define CAT_KB_DEFAULT_FILE         "default_keys.cfg"      // 0x008b6a30
#define CAT_KB_FILE                 "keys.cfg"              // 0x008b6a24
#define CAT_KB_LINE_LENGTH          0x200                   // the exe's fgets buffers

bool32 parseLine(char *line, char **returnName, char **returnValue);   // StatScript.c

// fonthandle for the keyboard list window font (kbInitKeyBindings)
fonthandle kbKeyListFont;                                   // 0x00c36a44

// the list window of the options screen and the command whose key it waits for (CAT_KB_NUMBER_COMMANDS: none)
static listwindowhandle kbListWindow = NULL;                // 0x00a44444
static sdword kbCommandBeingSet = CAT_KB_NUMBER_COMMANDS;   // 0x00a44440

// the bindings of each command
CatKbCommandBindings catKbBindings[CAT_KB_NUMBER_COMMANDS]; // 0x00a6ee90
bool32 catKbBindingsLoaded = FALSE;                         // 0x00a70d3c: cat_kbSaveKeyBindings writes only after a load
static sdword catKbVersion = 0;                             // 0x00a70d40: default_keys.cfg's version ("v 84")

// the commands' name and description string ids (the exe's table 0x008b5e58: {name, description} per command; the
//      names are the rows of default_keys.cfg / keys.cfg; row 0 is {-1, -1})
static const sdword catKbCommandNames[CAT_KB_NUMBER_COMMANDS][2] =
{
    /* 0x00 */ { -1, -1 },
    /* 0x01 */ { str_UI_MoveShips_KeyBindingName, str_UI_MoveShips_Description },
    /* 0x02 */ { str_UI_DockShips_KeyBindingName, str_UI_DockShips_Description },
    /* 0x03 */ { str_UI_RetireShips_KeyBindingName, str_UI_RetireShips_Description },
    /* 0x04 */ { str_UI_CancelShipOrders_KeyBindingName, str_UI_CancelShipOrders_Description },
    /* 0x05 */ { str_UI_ScuttleShips_KeyBindingName, str_UI_ScuttleShips_Description },
    /* 0x06 */ { str_UI_Kamikaze_KeyBindingName, str_UI_Kamikaze_Description },
    /* 0x07 */ { str_UI_ShipSpecialFunction_KeyBindingName, str_UI_ShipSpecialFunction_Description },
    /* 0x08 */ { str_UI_Hyperspace_KeyBindingName, str_UI_Hyperspace_Description },
    /* 0x09 */ { str_UI_ToggleWaypointGeneration_KeyBindingName, str_UI_ToggleWaypointGeneration_Description },
    /* 0x0A */ { str_UI_ToggleWaypointModes_KeyBindingName, str_UI_ToggleWaypointModes_Description },
    /* 0x0B */ { str_UI_SetWaypointTypeNormal_KeyBindingName, str_UI_SetWaypointTypeNormal_Description },
    /* 0x0C */ { str_UI_SetWaypointTypeEndToEnd_KeyBindingName, str_UI_SetWaypointTypeEndToEnd_Description },
    /* 0x0D */ { str_UI_SetWaypointTypeCircular_KeyBindingName, str_UI_SetWaypointTypeCircular_Description },
    /* 0x0E */ { str_UI_CancelLastWaypoint_KeyBindingName, str_UI_CancelLastWaypoint_Description },
    /* 0x0F */ { str_UI_UpgradeShips_KeyBindingName, str_UI_UpgradeShips_Description },
    /* 0x10 */ { str_UI_EnterSlipgate_KeyBindingName, str_UI_EnterSlipgate_Description },
    /* 0x11 */ { str_UI_Harvest_KeyBindingName, str_UI_Harvest_Description },
    /* 0x12 */ { str_UI_Repair_KeyBindingName, str_UI_Repair_Description },
    /* 0x13 */ { str_UI_Salvage_KeyBindingName, str_UI_Salvage_Description },
    /* 0x14 */ { str_UI_SelectLeechType_KeyBindingName, str_UI_SelectLeechType_Description },
    /* 0x15 */ { str_UI_LeechSpy_KeyBindingName, str_UI_LeechSpy_Description },
    /* 0x16 */ { str_UI_LeechLeech_KeyBindingName, str_UI_LeechLeech_Description },
    /* 0x17 */ { str_UI_LeechVent_KeyBindingName, str_UI_LeechVent_Description },
    /* 0x18 */ { str_UI_Leech_KeyBindingName, str_UI_Leech_Description },
    /* 0x19 */ { str_UI_UnLeech_KeyBindingName, str_UI_UnLeech_Description },
    /* 0x1A */ { str_UI_Link_KeyBindingName, str_UI_Link_Description },
    /* 0x1B */ { str_UI_Unlink_KeyBindingName, str_UI_Unlink_Description },
    /* 0x1C */ { str_UI_SelectMimicType_KeyBindingName, str_UI_SelectMimicType_Description },
    /* 0x1D */ { str_UI_SelectMimicItem_KeyBindingName, str_UI_SelectMimicItem_Description },
    /* 0x1E */ { str_UI_CancelMimic_KeyBindingName, str_UI_CancelMimic_Description },
    /* 0x1F */ { str_UI_ForceField_KeyBindingName, str_UI_ForceField_Description },
    /* 0x20 */ { str_UI_ForceFieldUnlink_KeyBindingName, str_UI_ForceFieldUnlink_Description },
    /* 0x21 */ { str_UI_Cloak_KeyBindingName, str_UI_Cloak_Description },
    /* 0x22 */ { str_UI_CancelCloak_KeyBindingName, str_UI_CancelCloak_Description },
    /* 0x23 */ { str_UI_LaunchSwarmers_KeyBindingName, str_UI_LaunchSwarmers_Description },
    /* 0x24 */ { str_UI_CancelSwarmers_KeyBindingName, str_UI_CancelSwarmers_Description },
    /* 0x25 */ { str_UI_BigGunAttack_KeyBindingName, str_UI_BigGunAttack_Description },
    /* 0x26 */ { str_UI_CancelBigGun_KeyBindingName, str_UI_CancelBigGun_Description },
    /* 0x27 */ { str_UI_BeastAttack_KeyBindingName, str_UI_BeastAttack_Description },
    /* 0x28 */ { str_UI_EMP_KeyBindingName, str_UI_EMP_Description },
    /* 0x29 */ { str_UI_Ram_KeyBindingName, str_UI_Ram_Description },
    /* 0x2A */ { str_UI_Repulse_KeyBindingName, str_UI_Repulse_Description },
    /* 0x2B */ { str_UI_FireMissiles_KeyBindingName, str_UI_FireMissiles_Description },
    /* 0x2C */ { str_UI_SetShipsTacticLevelEvasive_KeyBindingName, str_UI_SetShipsTacticLevelEvasive_Description },
    /* 0x2D */ { str_UI_SetShipsTacticLevelNeutral_KeyBindingName, str_UI_SetShipsTacticLevelNeutral_Description },
    /* 0x2E */ { str_UI_SetShipsTacticLevelAggresive_KeyBindingName, str_UI_SetShipsTacticLevelAggresive_Description },
    /* 0x2F */ { str_UI_ChangeShipsTacticLevelForward_KeyBindingName, str_UI_ChangeShipsTacticLevelForward_Description },
    /* 0x30 */ { str_UI_ChangeShipsTacticLevelBackward_KeyBindingName, str_UI_ChangeShipsTacticLevelBackward_Description },
    /* 0x31 */ { str_UI_SetShipsFormationDelta_KeyBindingName, str_UI_SetShipsFormationDelta_Description },
    /* 0x32 */ { str_UI_SetShipsFormationBroad_KeyBindingName, str_UI_SetShipsFormationBroad_Description },
    /* 0x33 */ { str_UI_SetShipsFormationDelta3D_KeyBindingName, str_UI_SetShipsFormationDelta3D_Description },
    /* 0x34 */ { str_UI_SetShipsFormationClaw_KeyBindingName, str_UI_SetShipsFormationClaw_Description },
    /* 0x35 */ { str_UI_SetShipsFormationWall_KeyBindingName, str_UI_SetShipsFormationWall_Description },
    /* 0x36 */ { str_UI_SetShipsFormationSphere_KeyBindingName, str_UI_SetShipsFormationSphere_Description },
    /* 0x37 */ { str_UI_SetShipsFormationCustom_KeyBindingName, str_UI_SetShipsFormationCustom_Description },
    /* 0x38 */ { str_UI_SetShipsFormationParade_KeyBindingName, str_UI_SetShipsFormationParade_Description },
    /* 0x39 */ { str_UI_NextShipsFormation_KeyBindingName, str_UI_NextShipsFormation_Description },
    /* 0x3A */ { str_UI_PrevShipsFormation_KeyBindingName, str_UI_PrevShipsFormation_Description },
    /* 0x3B */ { str_UI_Focus_KeyBindingName, str_UI_Focus_Description },
    /* 0x3C */ { str_UI_FocusOnMothership_KeyBindingName, str_UI_FocusOnMothership_Description },
    /* 0x3D */ { str_UI_FocusOnLastShipToSpeak_KeyBindingName, str_UI_FocusOnLastShipToSpeak_Description },
    /* 0x3E */ { str_UI_CancelFocus_KeyBindingName, str_UI_CancelFocus_Description },
    /* 0x3F */ { str_UI_ForwardFocus_KeyBindingName, str_UI_ForwardFocus_Description },
    /* 0x40 */ { str_UI_FleetView_KeyBindingName, str_UI_FleetView_Description },
    /* 0x41 */ { str_UI_PanSensorsLeft_KeyBindingName, str_UI_PanSensorsLeft_Description },
    /* 0x42 */ { str_UI_PanSensorsRight_KeyBindingName, str_UI_PanSensorsRight_Description },
    /* 0x43 */ { str_UI_PanSensorsUp_KeyBindingName, str_UI_PanSensorsUp_Description },
    /* 0x44 */ { str_UI_PanSensorsDown_KeyBindingName, str_UI_PanSensorsDown_Description },
    /* 0x45 */ { str_UI_ZoomIn_KeyBindingName, str_UI_ZoomIn_Description },
    /* 0x46 */ { str_UI_ZoomOut_KeyBindingName, str_UI_ZoomOut_Description },
    /* 0x47 */ { str_UI_ToggleChaseMode_KeyBindingName, str_UI_ToggleChaseMode_Description },
    /* 0x48 */ { str_UI_SelectAllVisibleShips_KeyBindingName, str_UI_SelectAllVisibleShips_Description },
    /* 0x49 */ { str_UI_SelectNone_KeyBindingName, str_UI_SelectNone_Description },
    /* 0x4A */ { str_UI_BandboxGuard_KeyBindingName, str_UI_BandboxGuard_Description },
    /* 0x4B */ { str_UI_BandboxAttack_KeyBindingName, str_UI_BandboxAttack_Description },
    /* 0x4C */ { str_UI_BandboxForceAttack_KeyBindingName, str_UI_BandboxForceAttack_Description },
    /* 0x4D */ { str_UI_BandboxFocus_KeyBindingName, str_UI_BandboxFocus_Description },
    /* 0x4E */ { str_UI_MultipleSelect_KeyBindingName, str_UI_MultipleSelect_Description },
    /* 0x4F */ { str_UI_ToggleSensorsManager_KeyBindingName, str_UI_ToggleSensorsManager_Description },
    /* 0x50 */ { str_UI_OpenBuildManager_KeyBindingName, str_UI_OpenBuildManager_Description },
    /* 0x51 */ { str_UI_OpenResearchManager_KeyBindingName, str_UI_OpenResearchManager_Description },
    /* 0x52 */ { str_UI_OpenLaunchManager_KeyBindingName, str_UI_OpenLaunchManager_Description },
    /* 0x53 */ { str_UI_ToggleTacticalDisplay_KeyBindingName, str_UI_ToggleTacticalDisplay_Description },
    /* 0x54 */ { str_UI_ToggleShipInfo_KeyBindingName, str_UI_ToggleShipInfo_Description },
    /* 0x55 */ { str_UI_NextOverlayView_KeyBindingName, str_UI_NextOverlayView_Description },
    /* 0x56 */ { str_UI_OpenMenu_KeyBindingName, str_UI_OpenMenu_Description },
    /* 0x57 */ { str_UI_EnableHeightMovement_KeyBindingName, str_UI_EnableHeightMovement_Description },
    /* 0x58 */ { str_UI_PageDown_GameChat_KeyBindingName, str_UI_PageDown_GameChat_Description },
    /* 0x59 */ { str_UI_PageUp_GameChat_KeyBindingName, str_UI_PageUp_GameChat_Description },
    /* 0x5A */ { str_UI_Escape_KeyBindingName, str_UI_Escape_Description },
    /* 0x5B */ { str_UI_StartAllieMessage_KeyBindingName, str_UI_StartAllieMessage_Description },
    /* 0x5C */ { str_UI_StartGlobalMessage_KeyBindingName, str_UI_StartGlobalMessage_Description },
    /* 0x5D */ { str_UI_AccelerateTime_KeyBindingName, str_UI_AccelerateTime_Description },
    /* 0x5E */ { str_UI_DecelerateTime_KeyBindingName, str_UI_DecelerateTime_Description },
    /* 0x5F */ { str_UI_ToggleGameSpeed_KeyBindingName, str_UI_ToggleGameSpeed_Description },
    /* 0x60 */ { str_UI_NextMusicTrack_KeyBindingName, str_UI_NextMusicTrack_Description },
    /* 0x61 */ { str_UI_PrevMusicTrack_KeyBindingName, str_UI_PrevMusicTrack_Description },
    /* 0x62 */ { str_UI_ToggleOverlayOptions_KeyBindingName, str_UI_ToggleOverlayOptions_Description },
    /* 0x63 */ { str_UI_ChangeOverlayBackdropsToggle_KeyBindingName, str_UI_ChangeOverlayBackdropsToggle_Description },
    /* 0x64 */ { str_UI_ShowKeys1_KeyBindingName, str_UI_ShowKeys1_Description },
    /* 0x65 */ { str_UI_ShowKeys2_KeyBindingName, str_UI_ShowKeys2_Description },
    /* 0x66 */ { str_UI_ShowKeys3_KeyBindingName, str_UI_ShowKeys3_Description },
    /* 0x67 */ { str_UI_GammaUp_KeyBindingName, str_UI_GammaUp_Description },
    /* 0x68 */ { str_UI_GammaDown_KeyBindingName, str_UI_GammaDown_Description },
    /* 0x69 */ { str_UI_Pause_KeyBindingName, str_UI_Pause_Description },
    /* 0x6A */ { str_UI_TakeScreenshot_KeyBindingName, str_UI_TakeScreenshot_Description },
    /* 0x6B */ { str_UI_SkipSensors_KeyBindingName, str_UI_SkipSensors_Description },
    /* 0x6C */ { str_UI_CreateShipGroup1_KeyBindingName, str_UI_CreateShipGroup1_Description },
    /* 0x6D */ { str_UI_CreateShipGroup2_KeyBindingName, str_UI_CreateShipGroup2_Description },
    /* 0x6E */ { str_UI_CreateShipGroup3_KeyBindingName, str_UI_CreateShipGroup3_Description },
    /* 0x6F */ { str_UI_CreateShipGroup4_KeyBindingName, str_UI_CreateShipGroup4_Description },
    /* 0x70 */ { str_UI_CreateShipGroup5_KeyBindingName, str_UI_CreateShipGroup5_Description },
    /* 0x71 */ { str_UI_CreateShipGroup6_KeyBindingName, str_UI_CreateShipGroup6_Description },
    /* 0x72 */ { str_UI_CreateShipGroup7_KeyBindingName, str_UI_CreateShipGroup7_Description },
    /* 0x73 */ { str_UI_CreateShipGroup8_KeyBindingName, str_UI_CreateShipGroup8_Description },
    /* 0x74 */ { str_UI_CreateShipGroup9_KeyBindingName, str_UI_CreateShipGroup9_Description },
    /* 0x75 */ { str_UI_CreateShipGroup10_KeyBindingName, str_UI_CreateShipGroup10_Description },
    /* 0x76 */ { str_UI_SelectShipGroup1_KeyBindingName, str_UI_SelectShipGroup1_Description },
    /* 0x77 */ { str_UI_SelectShipGroup2_KeyBindingName, str_UI_SelectShipGroup2_Description },
    /* 0x78 */ { str_UI_SelectShipGroup3_KeyBindingName, str_UI_SelectShipGroup3_Description },
    /* 0x79 */ { str_UI_SelectShipGroup4_KeyBindingName, str_UI_SelectShipGroup4_Description },
    /* 0x7A */ { str_UI_SelectShipGroup5_KeyBindingName, str_UI_SelectShipGroup5_Description },
    /* 0x7B */ { str_UI_SelectShipGroup6_KeyBindingName, str_UI_SelectShipGroup6_Description },
    /* 0x7C */ { str_UI_SelectShipGroup7_KeyBindingName, str_UI_SelectShipGroup7_Description },
    /* 0x7D */ { str_UI_SelectShipGroup8_KeyBindingName, str_UI_SelectShipGroup8_Description },
    /* 0x7E */ { str_UI_SelectShipGroup9_KeyBindingName, str_UI_SelectShipGroup9_Description },
    /* 0x7F */ { str_UI_SelectShipGroup10_KeyBindingName, str_UI_SelectShipGroup10_Description },
    /* 0x80 */ { str_UI_AddToShipGroup1_KeyBindingName, str_UI_AddToShipGroup1_Description },
    /* 0x81 */ { str_UI_AddToShipGroup2_KeyBindingName, str_UI_AddToShipGroup2_Description },
    /* 0x82 */ { str_UI_AddToShipGroup3_KeyBindingName, str_UI_AddToShipGroup3_Description },
    /* 0x83 */ { str_UI_AddToShipGroup4_KeyBindingName, str_UI_AddToShipGroup4_Description },
    /* 0x84 */ { str_UI_AddToShipGroup5_KeyBindingName, str_UI_AddToShipGroup5_Description },
    /* 0x85 */ { str_UI_AddToShipGroup6_KeyBindingName, str_UI_AddToShipGroup6_Description },
    /* 0x86 */ { str_UI_AddToShipGroup7_KeyBindingName, str_UI_AddToShipGroup7_Description },
    /* 0x87 */ { str_UI_AddToShipGroup8_KeyBindingName, str_UI_AddToShipGroup8_Description },
    /* 0x88 */ { str_UI_AddToShipGroup9_KeyBindingName, str_UI_AddToShipGroup9_Description },
    /* 0x89 */ { str_UI_AddToShipGroup10_KeyBindingName, str_UI_AddToShipGroup10_Description },
    /* 0x8A */ { str_UI_RemoveSelectedShipGroup_KeyBindingName, str_UI_RemoveSelectedShipGroup_Description },
    /* 0x8B */ { str_UI_ToggleSV_Asteroids_KeyBindingName, str_UI_ToggleSV_Asteroids_Description },
    /* 0x8C */ { str_UI_ToggleSV_FOWRanges_KeyBindingName, str_UI_ToggleSV_FOWRanges_Description },
    /* 0x8D */ { str_UI_ToggleSV_AreasVisited_KeyBindingName, str_UI_ToggleSV_AreasVisited_Description },
    /* 0x8E */ { str_UI_ToggleSV_AreasOccupied_KeyBindingName, str_UI_ToggleSV_AreasOccupied_Description },
    /* 0x8F */ { str_UI_ToggleSV_Icons_KeyBindingName, str_UI_ToggleSV_Icons_Description },
    /* 0x90 */ { str_UI_ToggleSV_ShipMeshes_KeyBindingName, str_UI_ToggleSV_ShipMeshes_Description },
    /* 0x91 */ { str_UI_ToggleSV_PieplateLines_KeyBindingName, str_UI_ToggleSV_PieplateLines_Description },
    /* 0x92 */ { str_UI_ToggleSV_Background_KeyBindingName, str_UI_ToggleSV_Background_Description },
    /* 0x93 */ { str_UI_ToggleSV_DecorativeAsteroids_KeyBindingName, str_UI_ToggleSV_DecorativeAsteroids_Description },
    /* 0x94 */ { str_UI_ToggleSV_Compass_KeyBindingName, str_UI_ToggleSV_Compass_Description },
    /* 0x95 */ { str_UI_ToggleSV_ObjectivesPointer_KeyBindingName, str_UI_ToggleSV_ObjectivesPointer_Description },
    /* 0x96 */ { str_UI_ToggleSV_SlipgatePaths_KeyBindingName, str_UI_ToggleSV_SlipgatePaths_Description },
};

// the debug-key list's group of each command (0x008b67c8; cat_moDebugKeyGet)
sdword catKbDebugKeyGroups[CAT_KB_NUMBER_COMMANDS] =
{
    8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 6, 6,
    6, 6, 6, 6, 6, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 0,
};

// the keyboard tables of cat_kbKeyToString (0x008b0ca8 + language * 0x200): per virtual-key code the character and
//      its shifted character
static const ubyte catKbKeyChars[5][256][2] =
{
    {   // English (0x008b0ca8)
        [0x20] = {0x20, 0x20}, [0x2d] = {0x2d, 0x5f}, [0x30] = {0x30, 0x29}, [0x31] = {0x31, 0x21}, [0x32] = {0x32, 0x40},
        [0x33] = {0x33, 0x23}, [0x34] = {0x34, 0x24}, [0x35] = {0x35, 0x25}, [0x36] = {0x36, 0x5e}, [0x37] = {0x37, 0x26},
        [0x38] = {0x38, 0x2a}, [0x39] = {0x39, 0x28}, [0x3a] = {0x3a, 0x00}, [0x3b] = {0x3b, 0x00}, [0x3c] = {0x3c, 0x00},
        [0x3d] = {0x3d, 0x2b}, [0x3e] = {0x3e, 0x00}, [0x3f] = {0x3f, 0x00}, [0x40] = {0x40, 0x00}, [0x41] = {0x61, 0x41},
        [0x42] = {0x62, 0x42}, [0x43] = {0x63, 0x43}, [0x44] = {0x64, 0x44}, [0x45] = {0x65, 0x45}, [0x46] = {0x66, 0x46},
        [0x47] = {0x67, 0x47}, [0x48] = {0x68, 0x48}, [0x49] = {0x69, 0x49}, [0x4a] = {0x6a, 0x4a}, [0x4b] = {0x6b, 0x4b},
        [0x4c] = {0x6c, 0x4c}, [0x4d] = {0x6d, 0x4d}, [0x4e] = {0x6e, 0x4e}, [0x4f] = {0x6f, 0x4f}, [0x50] = {0x70, 0x50},
        [0x51] = {0x71, 0x51}, [0x52] = {0x72, 0x52}, [0x53] = {0x73, 0x53}, [0x54] = {0x74, 0x54}, [0x55] = {0x75, 0x55},
        [0x56] = {0x76, 0x56}, [0x57] = {0x77, 0x57}, [0x58] = {0x78, 0x58}, [0x59] = {0x79, 0x59}, [0x5a] = {0x7a, 0x5a},
        [0x60] = {0x30, 0x00}, [0x61] = {0x31, 0x00}, [0x62] = {0x32, 0x00}, [0x63] = {0x33, 0x00}, [0x64] = {0x34, 0x00},
        [0x65] = {0x35, 0x00}, [0x66] = {0x36, 0x00}, [0x67] = {0x37, 0x00}, [0x68] = {0x38, 0x00}, [0x69] = {0x39, 0x00},
        [0x6a] = {0x2a, 0x00}, [0x6b] = {0x2b, 0x00}, [0x6d] = {0x2d, 0x00}, [0x6e] = {0x2e, 0x00}, [0x6f] = {0x2f, 0x00},
        [0xba] = {0x3b, 0x3a}, [0xbb] = {0x3d, 0x2b}, [0xbc] = {0x2c, 0x3c}, [0xbd] = {0x2d, 0x5f}, [0xbe] = {0x2e, 0x3e},
        [0xbf] = {0x2f, 0x3f}, [0xc0] = {0x60, 0x7e}, [0xdb] = {0x5b, 0x7b}, [0xdc] = {0x5c, 0x7c}, [0xdd] = {0x5d, 0x7d},
        [0xde] = {0x27, 0x22},
    },
    {   // French (0x008b0ea8)
        [0x20] = {0x20, 0x20}, [0x2c] = {0x2c, 0x3f}, [0x2d] = {0x2d, 0x5f}, [0x30] = {0xe0, 0x30}, [0x31] = {0x26, 0x31},
        [0x32] = {0xe9, 0x32}, [0x33] = {0x22, 0x33}, [0x34] = {0x27, 0x34}, [0x35] = {0x28, 0x35}, [0x36] = {0x2d, 0x36},
        [0x37] = {0xe8, 0x37}, [0x38] = {0x5f, 0x38}, [0x39] = {0xe7, 0x39}, [0x3a] = {0x3a, 0x00}, [0x3b] = {0x3b, 0x00},
        [0x3c] = {0x3c, 0x00}, [0x3d] = {0x3d, 0x2b}, [0x3e] = {0x3e, 0x00}, [0x3f] = {0x3f, 0x00}, [0x40] = {0x40, 0x00},
        [0x41] = {0x61, 0x41}, [0x42] = {0x62, 0x42}, [0x43] = {0x63, 0x43}, [0x44] = {0x64, 0x44}, [0x45] = {0x65, 0x45},
        [0x46] = {0x66, 0x46}, [0x47] = {0x67, 0x47}, [0x48] = {0x68, 0x48}, [0x49] = {0x69, 0x49}, [0x4a] = {0x6a, 0x4a},
        [0x4b] = {0x6b, 0x4b}, [0x4c] = {0x6c, 0x4c}, [0x4d] = {0x6d, 0x4d}, [0x4e] = {0x6e, 0x4e}, [0x4f] = {0x6f, 0x4f},
        [0x50] = {0x70, 0x50}, [0x51] = {0x71, 0x51}, [0x52] = {0x72, 0x52}, [0x53] = {0x73, 0x53}, [0x54] = {0x74, 0x54},
        [0x55] = {0x75, 0x55}, [0x56] = {0x76, 0x56}, [0x57] = {0x77, 0x57}, [0x58] = {0x78, 0x58}, [0x59] = {0x79, 0x59},
        [0x5a] = {0x7a, 0x5a}, [0x60] = {0x30, 0x00}, [0x61] = {0x31, 0x00}, [0x62] = {0x32, 0x00}, [0x63] = {0x33, 0x00},
        [0x64] = {0x34, 0x00}, [0x65] = {0x35, 0x00}, [0x66] = {0x36, 0x00}, [0x67] = {0x37, 0x00}, [0x68] = {0x38, 0x00},
        [0x69] = {0x39, 0x00}, [0x6a] = {0x2a, 0x00}, [0x6b] = {0x2b, 0x00}, [0x6d] = {0x2d, 0x00}, [0x6e] = {0x2e, 0x00},
        [0x6f] = {0x2f, 0x00}, [0xa7] = {0x21, 0xa7}, [0xba] = {0x3b, 0x3a}, [0xbb] = {0x3d, 0x2b}, [0xbc] = {0x3b, 0x2e},
        [0xbd] = {0x29, 0xb0}, [0xbe] = {0x3a, 0x2f}, [0xbf] = {0x2f, 0x3f}, [0xc0] = {0xb2, 0x00}, [0xdb] = {0x88, 0xa8},
        [0xdc] = {0x2a, 0xb5}, [0xdd] = {0x24, 0xa3}, [0xde] = {0x27, 0x22}, [0xf9] = {0xf9, 0x25},
    },
    {   // German (0x008b10a8)
        [0x20] = {0x20, 0x20}, [0x2c] = {0x2c, 0x3f}, [0x30] = {0x30, 0x3d}, [0x31] = {0x31, 0x21}, [0x32] = {0x32, 0x22},
        [0x33] = {0x33, 0xa7}, [0x34] = {0x34, 0x24}, [0x35] = {0x35, 0x25}, [0x36] = {0x36, 0x26}, [0x37] = {0x37, 0x2f},
        [0x38] = {0x38, 0x28}, [0x39] = {0x39, 0x29}, [0x3a] = {0x3a, 0x00}, [0x3b] = {0x3b, 0x00}, [0x3c] = {0x3c, 0x00},
        [0x3d] = {0x3d, 0x00}, [0x3e] = {0x3e, 0x00}, [0x3f] = {0x3f, 0x00}, [0x40] = {0x40, 0x00}, [0x41] = {0x61, 0x41},
        [0x42] = {0x62, 0x42}, [0x43] = {0x63, 0x43}, [0x44] = {0x64, 0x44}, [0x45] = {0x65, 0x45}, [0x46] = {0x66, 0x46},
        [0x47] = {0x67, 0x47}, [0x48] = {0x68, 0x48}, [0x49] = {0x69, 0x49}, [0x4a] = {0x6a, 0x4a}, [0x4b] = {0x6b, 0x4b},
        [0x4c] = {0x6c, 0x4c}, [0x4d] = {0x6d, 0x4d}, [0x4e] = {0x6e, 0x4e}, [0x4f] = {0x6f, 0x4f}, [0x50] = {0x70, 0x50},
        [0x51] = {0x71, 0x51}, [0x52] = {0x72, 0x52}, [0x53] = {0x73, 0x53}, [0x54] = {0x74, 0x54}, [0x55] = {0x75, 0x55},
        [0x56] = {0x76, 0x56}, [0x57] = {0x77, 0x57}, [0x58] = {0x78, 0x58}, [0x59] = {0x79, 0x59}, [0x5a] = {0x7a, 0x5a},
        [0x60] = {0x30, 0x00}, [0x61] = {0x31, 0x00}, [0x62] = {0x32, 0x00}, [0x63] = {0x33, 0x00}, [0x64] = {0x34, 0x00},
        [0x65] = {0x35, 0x00}, [0x66] = {0x36, 0x00}, [0x67] = {0x37, 0x00}, [0x68] = {0x38, 0x00}, [0x69] = {0x39, 0x00},
        [0x6a] = {0x2a, 0x00}, [0x6b] = {0x2b, 0x00}, [0x6d] = {0x2d, 0x00}, [0x6e] = {0x2e, 0x00}, [0x6f] = {0x2f, 0x00},
        [0xba] = {0x3b, 0x3a}, [0xbb] = {0xb4, 0x60}, [0xbc] = {0x2c, 0x3b}, [0xbd] = {0xdf, 0x3f}, [0xbe] = {0x2e, 0x3a},
        [0xbf] = {0x2f, 0x3f}, [0xc0] = {0x88, 0xb0}, [0xc8] = {0x2d, 0x5f}, [0xdb] = {0xfc, 0xdc}, [0xdc] = {0x23, 0x27},
        [0xdd] = {0x2b, 0x2a}, [0xde] = {0x27, 0x22}, [0xe4] = {0xe4, 0xc4}, [0xf6] = {0xf6, 0xd6},
    },
    {   // Spanish (0x008b12a8)
        [0x20] = {0x20, 0x20}, [0x2d] = {0x2d, 0x5f}, [0x30] = {0x30, 0x3d}, [0x31] = {0x31, 0x21}, [0x32] = {0x32, 0x22},
        [0x33] = {0x33, 0xb7}, [0x34] = {0x34, 0x24}, [0x35] = {0x35, 0x25}, [0x36] = {0x36, 0x26}, [0x37] = {0x37, 0x2f},
        [0x38] = {0x38, 0x28}, [0x39] = {0x39, 0x29}, [0x3a] = {0x3a, 0x00}, [0x3b] = {0x3b, 0x00}, [0x3c] = {0x3c, 0x00},
        [0x3d] = {0x3d, 0x2b}, [0x3e] = {0x3e, 0x00}, [0x3f] = {0x3f, 0x00}, [0x40] = {0x40, 0x00}, [0x41] = {0x61, 0x41},
        [0x42] = {0x62, 0x42}, [0x43] = {0x63, 0x43}, [0x44] = {0x64, 0x44}, [0x45] = {0x65, 0x45}, [0x46] = {0x66, 0x46},
        [0x47] = {0x67, 0x47}, [0x48] = {0x68, 0x48}, [0x49] = {0x69, 0x49}, [0x4a] = {0x6a, 0x4a}, [0x4b] = {0x6b, 0x4b},
        [0x4c] = {0x6c, 0x4c}, [0x4d] = {0x6d, 0x4d}, [0x4e] = {0x6e, 0x4e}, [0x4f] = {0x6f, 0x4f}, [0x50] = {0x70, 0x50},
        [0x51] = {0x71, 0x51}, [0x52] = {0x72, 0x52}, [0x53] = {0x73, 0x53}, [0x54] = {0x74, 0x54}, [0x55] = {0x75, 0x55},
        [0x56] = {0x76, 0x56}, [0x57] = {0x77, 0x57}, [0x58] = {0x78, 0x58}, [0x59] = {0x79, 0x59}, [0x5a] = {0x7a, 0x5a},
        [0x60] = {0x30, 0x00}, [0x61] = {0x31, 0x00}, [0x62] = {0x32, 0x00}, [0x63] = {0x33, 0x00}, [0x64] = {0x34, 0x00},
        [0x65] = {0x35, 0x00}, [0x66] = {0x36, 0x00}, [0x67] = {0x37, 0x00}, [0x68] = {0x38, 0x00}, [0x69] = {0x39, 0x00},
        [0x6a] = {0x2a, 0x00}, [0x6b] = {0x2b, 0x00}, [0x6d] = {0x2d, 0x00}, [0x6e] = {0x2e, 0x00}, [0x6f] = {0x2f, 0x00},
        [0xb4] = {0xb4, 0xa8}, [0xba] = {0x3b, 0x3a}, [0xbb] = {0xa1, 0xbf}, [0xbc] = {0x2c, 0x3b}, [0xbd] = {0x27, 0x3f},
        [0xbe] = {0x2e, 0x3a}, [0xbf] = {0x2f, 0x3f}, [0xc0] = {0xba, 0xaa}, [0xc8] = {0x2d, 0x5f}, [0xdb] = {0x60, 0x5e},
        [0xdc] = {0xe7, 0xc7}, [0xdd] = {0x2b, 0x2a}, [0xde] = {0x27, 0x22}, [0xe7] = {0xe7, 0xc7}, [0xf1] = {0xf1, 0xd1},
    },
    {   // Italian (0x008b14a8)
        [0x20] = {0x20, 0x20}, [0x2d] = {0x2d, 0x5f}, [0x30] = {0x30, 0x3d}, [0x31] = {0x31, 0x21}, [0x32] = {0x32, 0x22},
        [0x33] = {0x33, 0xa3}, [0x34] = {0x34, 0x24}, [0x35] = {0x35, 0x25}, [0x36] = {0x36, 0x26}, [0x37] = {0x37, 0x2f},
        [0x38] = {0x38, 0x28}, [0x39] = {0x39, 0x29}, [0x3a] = {0x3a, 0x00}, [0x3b] = {0x3b, 0x00}, [0x3c] = {0x3c, 0x00},
        [0x3d] = {0x3d, 0x2b}, [0x3e] = {0x3e, 0x00}, [0x3f] = {0x3f, 0x00}, [0x40] = {0x40, 0x00}, [0x41] = {0x61, 0x41},
        [0x42] = {0x62, 0x42}, [0x43] = {0x63, 0x43}, [0x44] = {0x64, 0x44}, [0x45] = {0x65, 0x45}, [0x46] = {0x66, 0x46},
        [0x47] = {0x67, 0x47}, [0x48] = {0x68, 0x48}, [0x49] = {0x69, 0x49}, [0x4a] = {0x6a, 0x4a}, [0x4b] = {0x6b, 0x4b},
        [0x4c] = {0x6c, 0x4c}, [0x4d] = {0x6d, 0x4d}, [0x4e] = {0x6e, 0x4e}, [0x4f] = {0x6f, 0x4f}, [0x50] = {0x70, 0x50},
        [0x51] = {0x71, 0x51}, [0x52] = {0x72, 0x52}, [0x53] = {0x73, 0x53}, [0x54] = {0x74, 0x54}, [0x55] = {0x75, 0x55},
        [0x56] = {0x76, 0x56}, [0x57] = {0x77, 0x57}, [0x58] = {0x78, 0x58}, [0x59] = {0x79, 0x59}, [0x5a] = {0x7a, 0x5a},
        [0x60] = {0x30, 0x00}, [0x61] = {0x31, 0x00}, [0x62] = {0x32, 0x00}, [0x63] = {0x33, 0x00}, [0x64] = {0x34, 0x00},
        [0x65] = {0x35, 0x00}, [0x66] = {0x36, 0x00}, [0x67] = {0x37, 0x00}, [0x68] = {0x38, 0x00}, [0x69] = {0x39, 0x00},
        [0x6a] = {0x2a, 0x00}, [0x6b] = {0x2b, 0x00}, [0x6d] = {0x2d, 0x00}, [0x6e] = {0x2e, 0x00}, [0x6f] = {0x2f, 0x00},
        [0xba] = {0x3b, 0x3a}, [0xbb] = {0xec, 0x5e}, [0xbc] = {0x2c, 0x3b}, [0xbd] = {0x27, 0x3f}, [0xbe] = {0x2e, 0x3a},
        [0xbf] = {0x2f, 0x3f}, [0xc0] = {0x5c, 0x7c}, [0xc8] = {0x2d, 0x5f}, [0xdb] = {0xe8, 0xe9}, [0xdc] = {0xf9, 0xa7},
        [0xdd] = {0x2b, 0x2a}, [0xde] = {0xe0, 0xb0}, [0xf1] = {0xf2, 0xe7},
    },
};

/*-----------------------------------------------------------------------------
    Name        : kbListTitleDraw                                   0x0049b830
    Description : The title row of the key-binding list window: "COMMAND" and "BOUND KEY(S)" over a translucent bar.
----------------------------------------------------------------------------*/
static void kbListTitleDraw(rectangle *rect)
{
    fonthandle oldfont;
    rectangle r = *rect;

    oldfont = fontMakeCurrent(kbKeyListFont);

    fontPrintf(rect->x0 + 6, rect->y0, colWhite, strGetString(strCommandToBind));
    fontPrintf(rect->x0 + (((rect->x1 - rect->x0) * 3) / 5), rect->y0, colWhite, strGetString(strKeyBound));

    r.x0 -= 2;
    r.x1++;
    r.y1--;

    primRectTranslucent2(&r, colRGBA(0, 160, 100, 63));     // 0x3f64a000

    fontMakeCurrent(oldfont);
}

/*-----------------------------------------------------------------------------
    Name        : kbListItemDraw                                    0x0049b8f0
    Description : One command of the key-binding list: its name (the first command of a group after the group's
                  header, the others indented by its width) and its keys ("??????" while waiting for a key, "NONE"
                  when unbound: red).
----------------------------------------------------------------------------*/
static void kbListItemDraw(rectangle *rect, listitemhandle data)
{
    fonthandle oldfont;
    sdword command = (sdword)(size_t)data->data;
    CatKbCommandBindings bindings;
    color c = FEC_ListItemStandard;
    char name[0x200], header[0x200], binding[0x100];
    sdword index;

    oldfont = fontMakeCurrent(kbKeyListFont);
    if (bitTest(data->flags, UICLI_Selected))
    {
        c = FEC_ListItemSelected;
    }
    cat_kbBindingGet(command, &bindings);
    if (bindings.count == 0)
    {
        c = 0xff1414ff;                                     // the exe's colour words (colRGB(255,20,20))
    }
    else if (kbCommandBeingSet == command)
    {
        c = 0xff14ffff;                                     // (colRGB(255,255,20))
    }

    switch (command)
    {                                                       // the first command of each group (0x0049bc14)
        case 0x01: case 0x11: case 0x2c: case 0x31: case 0x3b: case 0x48: case 0x4f: case 0x56: case 0x6c: case 0x8b:
            sprintf(name, "%s %s", cat_moDebugKeyValue(cat_moDebugKeyGet(command)), cat_kbCommandNameGet(command));
            break;
        default:
            sprintf(name, " ");
            sprintf(header, "%s ", cat_moDebugKeyValue(cat_moDebugKeyGet(command)));
            while (fontWidth(name) < fontWidth(header))
            {
                strcat(name, " ");
            }
            strcat(name, cat_kbCommandNameGet(command));
            break;
    }
    fontPrintf(rect->x0 + 6, rect->y0, c, name);

    if (kbCommandBeingSet == command)
    {
        fontPrintf(rect->x0 + (((rect->x1 - rect->x0) * 3) / 5), rect->y0, c, "??????");
    }
    else if (bindings.count == 0)
    {
        fontPrintf(rect->x0 + (((rect->x1 - rect->x0) * 3) / 5), rect->y0, c, "%s", strGetString(strNoKeyBound));
    }
    else
    {
        name[0] = '\0';
        for (index = 0; index < bindings.count; index++)
        {
            cat_kbBindingToString(&bindings.binding[index], binding, TRUE);
            strcat(name, binding);
            if (index + 1 < bindings.count)
            {
                strcat(name, ",");
            }
        }
        fontPrintf(rect->x0 + (((rect->x1 - rect->x0) * 3) / 5), rect->y0, c, "%s", name);
    }

    fontMakeCurrent(oldfont);
}

/*-----------------------------------------------------------------------------
    Name        : kbListWindowCB                                    0x0049bca0
    Description : The options screen's key-binding list: the 150 commands; Enter or a double click waits for a key,
                  the captured key is bound to the command (cat_kbSetNewKey: its SHIFT / ALT / CTRL state with it).
----------------------------------------------------------------------------*/
void kbListWindowCB(char *string, featom *atom)
{
    fonthandle oldfont;
    sdword command;
    udword vk;

    (void)string;
    if (FEFIRSTCALL(atom))
    {
        oldfont = fontMakeCurrent(kbKeyListFont);
        kbListWindow = (listwindowhandle)atom->pData;

        uicListWindowInit(kbListWindow,
                          kbListTitleDraw,                                  // title draw
                          NULL,                                             // title click process
                          fontHeight(" ") + (fontHeight(" ") >> 1),         // title height
                          kbListItemDraw,                                   // item draw function
                          fontHeight(" ") + (fontHeight(" ") >> 1),         // item height
                          UICLW_CanSelect | UICLW_CanHaveFocus);

        for (command = CAT_KB_NUMBER_COMMANDS - 1; command > 0; command--)
        {
            uicListAddItem(kbListWindow, (ubyte *)(size_t)command, UICLI_CanSelect, UICLW_AddToHead);
        }

        fontMakeCurrent(oldfont);
        return;
    }
    else if (FELASTCALL(atom))
    {
        bitClear(kbListWindow->windowflags, UICLW_GetKeyPressed);
        kbListWindow = NULL;
        return;
    }
    switch (kbListWindow->message)
    {
        case CM_LoseFocus:
        case CM_NewItemSelected:
            bitClear(kbListWindow->windowflags, UICLW_GetKeyPressed);
            kbCommandBeingSet = CAT_KB_NUMBER_COMMANDS;
            break;
        case CM_AcceptText:
        case CM_DoubleClick:
            bitSet(kbListWindow->windowflags, UICLW_GetKeyPressed);
#ifdef DEBUG_STOMP
            regVerify((regionhandle)&kbListWindow->reg);
#endif
            bitSet(kbListWindow->reg.status, RSF_DrawThisFrame);
            kbCommandBeingSet = (sdword)(size_t)kbListWindow->CurLineSelected->data;
            break;
        case CM_KeyCaptured:
            // HWI: the captured key is this tree's key index; the exe's (a virtual-key code) always names a key
            //      (cat_kbKeyToString never returns NULL for it), a key the exe has no code for shows the
            //      invalid-key screen the exe keeps for that case
            vk = catKeyVkFromKey(kbListWindow->keypressed);
            if ((vk != 0) && (cat_kbKeyToString(vk) != NULL))
            {
                cat_kbSetNewKey(kbCommandBeingSet, vk & 0xff);
                bitSet(kbListWindow->reg.status, RSF_DrawThisFrame);
                kbCommandBeingSet = CAT_KB_NUMBER_COMMANDS;
                break;
            }
            if (!gameIsRunning)
            {
                feScreenStart(feStack[feStackIndex].baseRegion, "Invalid_Key");
            }
            else
            {
                feScreenStart(feStack[feStackIndex].baseRegion, "In_Game_Invalid_Key");
            }
            kbCommandBeingSet = CAT_KB_NUMBER_COMMANDS;
            break;
    }
}

/*-----------------------------------------------------------------------------
    Name        : kbKeyResetToDefault                               0x0049be80
    Description : The options screen's "default" button: the bindings back to default_keys.cfg.
----------------------------------------------------------------------------*/
void kbKeyResetToDefault(char *string, featom *atom)
{
    (void)string;
    (void)atom;
    cat_kbReloadKeyBindings(TRUE);
#ifdef DEBUG_STOMP
    regVerify(&kbListWindow->reg);
#endif
    bitSet(kbListWindow->reg.status, RSF_DrawThisFrame);
}

/*-----------------------------------------------------------------------------
    Name        : kbInitKeyBindings                                 0x0049bea0
    Description : The key-binding list's font (the bindings load in cat_kbLoadKeyBindings).
----------------------------------------------------------------------------*/
void kbInitKeyBindings(void)
{
    kbKeyListFont = frFontRegister("hw_eurosecond_11.hff");
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbKeyCharParse                                0x0049b570
    Description : A key field's number (the virtual-key code): TRUE unless it is 0.
----------------------------------------------------------------------------*/
bool32 cat_kbKeyCharParse(char *string, char *key)
{
    *key = (char)atoi(string);
    return (*key != '\0');
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbKeyToString                                 0x0049b590
    Description : A virtual-key code's name: the key-name string of the named keys (modifiers, arrows, F keys, ...),
                  else the character of the current keyboard's table (the shifted one, but for the digits).
    Return      : a static buffer (0x00a44404)
----------------------------------------------------------------------------*/
char *cat_kbKeyToString(udword vk)
{
    static char buffer[0x40];                               // 0x00a44404
    ubyte key = (ubyte)vk;
    sdword ch;
    udword language;

    switch (key)
    {
        case 0x10: case 0x12: case 0x11: case 0x24: case 0x22: case 0x21: case 0xdc: case 0x91: case 0x2a: case 0x2d:
        case 0x2e: case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77: case 0x78:
        case 0x79: case 0x7a: case 0x7b: case 0x0d: case 0x08: case 0x09: case 0x25: case 0x27: case 0x26: case 0x28:
        case 0x23: case 0x14: case 0x20: case 0x1b:
            sprintf(buffer, "%s", strGetString(catKeyNameStrings[key]));
            return buffer;
        default:
            break;
    }
    ch = key;
    language = strCurKeyboardLanguage;
    if (language <= 4)
    {
        if ((catKbKeyChars[language][key][1] != 0) && ((key < '0') || (key > '9')))
        {
            ch = (sbyte)catKbKeyChars[language][key][1];
        }
        else if (catKbKeyChars[language][key][0] != 0)
        {
            ch = (sbyte)catKbKeyChars[language][key][0];
        }
    }
    sprintf(buffer, "%c", ch);
    return buffer;
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbCommandsCanShareKey                         0x0054d6a0
    Description : Commands that may have the same key: Enable_Height_Move with Multiple_Select, Toggle_SensorsManager
                  with Skip_Sensors, and the seven "cancel" commands (UnLeech, Unlink, Cancel_Mimic, Force_Field_Unlink,
                  Cancel_Cloak, Cancel_Swarmers, Cancel_BigGun) with each other; swapped tries the other order too.
----------------------------------------------------------------------------*/
static bool32 catKbIsCancelCommand(sdword command)
{
    return (command == 0x19) || (command == 0x1b) || (command == 0x1e) || (command == 0x20) || (command == 0x22) ||
           (command == 0x24) || (command == 0x26);
}

bool32 cat_kbCommandsCanShareKey(sdword command1, sdword command2, bool32 swapped)
{
    bool32 result = FALSE;

    if (((command1 == 0x57) && (command2 == 0x4e)) || ((command1 == 0x4f) && (command2 == 0x6b)) ||
        (catKbIsCancelCommand(command1) && catKbIsCancelCommand(command2)))
    {
        result = TRUE;
    }
    if (swapped && cat_kbCommandsCanShareKey(command2, command1, FALSE))
    {
        return TRUE;
    }
    return result;
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbBindingToString                             0x0054d750
    Description : A binding as text: the localised "ALT+", "SHIFT+", "CTRL+" and the key's name, or (for the keys
                  file) "ALT+", "SHIFT+", "CTRL+" and the virtual-key code.
----------------------------------------------------------------------------*/
void cat_kbBindingToString(CatKbBinding *binding, char *out, bool32 useKeyName)
{
    char alt[0x80], shift[0x80], ctrl[0x80];

    if (useKeyName)
    {
        sprintf(alt, "%s+", strGetString(strALTKEY));
        sprintf(shift, "%s+", strGetString(strSHIFTKEY));
        sprintf(ctrl, "%s+", strGetString(strCTRLKEY));
        sprintf(out, "%s%s%s%s", binding->alt ? alt : "", binding->shift ? shift : "", binding->ctrl ? ctrl : "",
                cat_kbKeyToString(binding->key & 0xff));
        return;
    }
    sprintf(out, "%s%s%s%d", binding->alt ? "ALT+" : "", binding->shift ? "SHIFT+" : "", binding->ctrl ? "CTRL+" : "",
            (sdword)(binding->key & 0xff));
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbCheckBindingConflict                        0x0054d880
    Description : May the binding go to the command: no other command (unless the two may share it) has it.
    Outputs     : message - the reason when it may not
    Return      : 0 bound elsewhere, 1 a SHIFT / ALT / CTRL key used as a normal key (allowed), 2 free
----------------------------------------------------------------------------*/
sdword cat_kbCheckBindingConflict(CatKbBinding *binding, sdword command, sdword unused, char *message)
{
    sdword other, index;
    CatKbBinding *b;
    ubyte key = (ubyte)binding->key;

    (void)unused;
    for (other = 0; other < CAT_KB_NUMBER_COMMANDS; other++)
    {
        for (index = 0; index < catKbBindings[other].count; index++)
        {
            b = &catKbBindings[other].binding[index];
            if (((ubyte)b->key == key) && (b->alt == binding->alt) && (b->ctrl == binding->ctrl) &&
                (b->shift == binding->shift))
            {
                if (!cat_kbCommandsCanShareKey(command, other, FALSE) && !cat_kbCommandsCanShareKey(other, command, FALSE))
                {
                    sprintf(message, "'%s' is already bound to '%s'", cat_kbKeyToString(key), cat_kbCommandNameGet(other));
                    return 0;
                }
            }
        }
    }
    if ((key != 0x10) && (key != 0x12) && (key != 0x11))
    {
        return 2;
    }
    sprintf(message, "%s should not be used for a normal key...", cat_kbKeyToString(key));
    return 1;
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbKeySetCB                                    0x0054d460
    Description : One "key <command> <keys>" line of keys.cfg: "[ALT+][SHIFT+][CTRL+]<virtual-key code>" added to the
                  command's bindings (a command name not in the table is skipped: the install's file has 48 developer
                  rows the exe has no command for).
----------------------------------------------------------------------------*/
static void cat_kbKeySetCB(char *directory, char *field, void *dataToFillIn)
{
    char name[0x100], keys[0x100], message[0x100];
    char *string, *plus;
    sdword command = -1, index, result;
    CatKbBinding binding;
    char key;

    (void)directory;
    (void)dataToFillIn;
    if (sscanf(field, "%255s%255s", name, keys) != 2)       // HWI: bounded (the exe's "%s%s" into 256-byte buffers)
    {
        dbgFatalf(DBG_Loc, "\nERROR - Parsing failed of line in '%s'.  Line='%s'", CAT_KB_FILE, field);
        return;
    }
    for (index = 1; index < CAT_KB_NUMBER_COMMANDS; index++)
    {                                                       // the last matching command wins, as the exe's loop
        if (strcasecmp(name, strGetString(catKbCommandNames[index][0])) == 0)
        {
            command = index;
        }
    }
    if (command == -1)
    {
        return;
    }
    if (catKbBindings[command].count > CAT_KB_MAX_BINDINGS - 1)
    {
        dbgFatalf(DBG_Loc, "\nERROR - To many keys assigned to User Input Type '%s'!", name);
        return;
    }
    memset(&binding, 0, sizeof(binding));
    binding.alt = (strstr(keys, "ALT+") != NULL);
    binding.shift = (strstr(keys, "SHIFT+") != NULL);
    binding.ctrl = (strstr(keys, "CTRL+") != NULL);
    string = keys;
    plus = strstr(string, "+");
    while ((plus != NULL) && (strcasecmp(string, "+") != 0) && (strcasecmp(string, "NUM+") != 0))
    {                                                       // past the modifiers to the code
        string = strstr(string, "+") + 1;
        plus = strstr(string, "+");
    }
    if (!cat_kbKeyCharParse(string, &key))
    {
        dbgFatalf(DBG_Loc, "\nERROR - Key Name not recognized:  %s", string);
        return;
    }
    binding.key = (ubyte)key;
    result = cat_kbCheckBindingConflict(&binding, command, 0, message);
    if (result == 0)
    {
        dbgFatalf(DBG_Loc, "\nERROR - Key Binding not assigned: %s", message);
        return;
    }
    catKbBindings[command].binding[catKbBindings[command].count] = binding;
    catKbBindings[command].count++;
}

static scriptEntry catKbKeysTable[] =                       // 0x008b6a48
{
    {"key", cat_kbKeySetCB, NULL},
    END_SCRIPT_ENTRY
};

// HWI: the two files' paths (see the block comment)
static char *catKbDefaultPath(void)
{
    static char path[PATH_MAX];

    strncpy(path, filePathPrepend(CAT_KB_DEFAULT_FILE, FF_HomeworldDataPath), PATH_MAX - 1);
    path[PATH_MAX - 1] = '\0';
    return path;
}

static char *catKbUserDirectory(void)
{
    static char path[PATH_MAX];

    strncpy(path, filePathPrepend("", FF_UserSettingsPath), PATH_MAX - 1);
    path[PATH_MAX - 1] = '\0';
    return path;
}

static char *catKbUserPath(void)
{
    static char path[PATH_MAX];

    strncpy(path, filePathPrepend(CAT_KB_FILE, FF_UserSettingsPath), PATH_MAX - 1);
    path[PATH_MAX - 1] = '\0';
    return path;
}

// the version line of a keys file: "v <number>" (dbgFatalf if it is not there)
static sdword catKbVersionRead(char *path, char *what)
{
    FILE *f = fopen(path, "rt");
    char line[CAT_KB_LINE_LENGTH], *name, *value;
    sdword version;

    if (f == NULL)
    {
        dbgFatalf(DBG_Loc, "\nCould not open %s for reading the default keys!", what);
        return 0;
    }
    line[0] = '\0';
    if ((fgets(line, CAT_KB_LINE_LENGTH, f) == NULL) || !parseLine(line, &name, &value) || (strcmp(name, "v") != 0) ||
        (sscanf(value, "%d", &version) != 1))
    {
        fclose(f);
        dbgFatalf(DBG_Loc, "\nCould not read version number of '%s'.", what);
        return 0;
    }
    fclose(f);
    return version;
}

// keys.cfg made a copy of default_keys.cfg, line for line
static void catKbCopyDefaults(void)
{
    FILE *in = fopen(catKbDefaultPath(), "rt");
    FILE *out;
    char line[CAT_KB_LINE_LENGTH];

    if (in == NULL)
    {
        dbgFatalf(DBG_Loc, "Could not open %s for reading the default keys!", CAT_KB_DEFAULT_FILE);
        return;
    }
    out = fopen(catKbUserPath(), "wt");
    if (out == NULL)
    {
        fclose(in);
        dbgFatalf(DBG_Loc, "Could not open %s for writing!", CAT_KB_FILE);
        return;
    }
    while (fgets(line, CAT_KB_LINE_LENGTH, in) != NULL)
    {
        fprintf(out, "%s", line);
    }
    fclose(in);
    fclose(out);
}

static bool32 catKbUserFileExists(void)
{
    FILE *f = fopen(catKbUserPath(), "rt");

    if (f == NULL)
    {
        return FALSE;
    }
    fclose(f);
    return TRUE;
}

// the common part of the load and the reload: keys.cfg older than default_keys.cfg is replaced, then parsed
static void catKbReadUserFile(void)
{
    if (catKbVersionRead(catKbUserPath(), CAT_KB_FILE) < catKbVersion)
    {
        if (remove(catKbUserPath()) == -1)
        {
            dbgFatalf(DBG_Loc, "Could not delete %s!", CAT_KB_FILE);
        }
        catKbCopyDefaults();
    }
    scriptSetFileSystem(catKbUserDirectory(), CAT_KB_FILE, catKbKeysTable);
    catKbBindingsLoaded = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbLoadKeyBindings                             0x0054da10
    Description : At start-up (utyGameSystemsInit): the bindings from keys.cfg, made from default_keys.cfg when it is
                  missing or older than it.
----------------------------------------------------------------------------*/
void cat_kbLoadKeyBindings(void)
{
    sdword command;

    for (command = 0; command < CAT_KB_NUMBER_COMMANDS; command++)
    {
        catKbBindings[command].count = 0;
    }
    catKbVersion = catKbVersionRead(catKbDefaultPath(), CAT_KB_DEFAULT_FILE);
    if (!catKbUserFileExists())
    {
        catKbCopyDefaults();
    }
    catKbReadUserFile();
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbReloadKeyBindings                           0x0054de10
    Description : The bindings again from keys.cfg (the options screen's cancel), or (resetToDefaults) from a fresh
                  copy of default_keys.cfg (the "default" button).
----------------------------------------------------------------------------*/
void cat_kbReloadKeyBindings(bool32 resetToDefaults)
{
    sdword command;

    for (command = 0; command < CAT_KB_NUMBER_COMMANDS; command++)
    {
        catKbBindings[command].count = 0;
    }
    catKbVersion = catKbVersionRead(catKbDefaultPath(), CAT_KB_DEFAULT_FILE);
    if (resetToDefaults || !catKbUserFileExists())
    {
        catKbCopyDefaults();
    }
    catKbReadUserFile();
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbSaveKeyBindings                             0x0054e2d0
    Description : keys.cfg written from the bindings (the options screen's accept): the version, the header and one
                  "key  <command>  [ALT+][SHIFT+][CTRL+]<code>" line per binding in command order.
----------------------------------------------------------------------------*/
void cat_kbSaveKeyBindings(void)
{
    FILE *f;
    sdword command, index;
    char binding[0x100];

    if (!catKbBindingsLoaded)
    {
        return;
    }
    f = fopen(catKbUserPath(), "wt");
    if (f == NULL)
    {
        dbgFatalf(DBG_Loc, "Could not open %s for writing!", CAT_KB_FILE);
        return;
    }
    fprintf(f, "%s %d\n", "v", catKbVersion);
    fprintf(f, "; Generated Keys Configuration File - DO NOT EDIT!\n\n");
    for (command = 0; command < CAT_KB_NUMBER_COMMANDS; command++)
    {
        for (index = 0; index < catKbBindings[command].count; index++)
        {
            cat_kbBindingToString(&catKbBindings[command].binding[index], binding, FALSE);
            fprintf(f, "key  %s  %s\n", strGetString(catKbCommandNames[command][0]), binding);
        }
    }
    fclose(f);
}

// the modifier keys held (the exe's keyScanCode[0x10 / 0x12 / 0x11])
#define CAT_KB_SHIFT_HELD()         catKeyVkIsHit(0x10)
#define CAT_KB_ALT_HELD()           catKeyVkIsHit(0x12)
#define CAT_KB_CTRL_HELD()          catKeyVkIsHit(0x11)

/*-----------------------------------------------------------------------------
    Name        : cat_kbSetNewKey                                   0x0054e410
    Description : The key (with the SHIFT / ALT / CTRL keys held now) bound to the command: the command loses all its
                  keys if it had this one, the commands that have it (and may not share it) lose it, a command with three
                  keys loses them, then the key is added.
----------------------------------------------------------------------------*/
void cat_kbSetNewKey(sdword command, udword vk)
{
    bool32 alt = CAT_KB_ALT_HELD(), shift = CAT_KB_SHIFT_HELD(), ctrl = CAT_KB_CTRL_HELD();
    ubyte key = (ubyte)vk;
    sdword commands[CAT_KB_NUMBER_COMMANDS];
    sdword count, index, found, other, j;
    CatKbBinding *b;

    if ((command < 0) || (command >= CAT_KB_NUMBER_COMMANDS))
    {                                                       // CAT-FIX: the exe indexes its table unchecked
        return;
    }
    for (index = 0; index < catKbBindings[command].count; index++)
    {
        b = &catKbBindings[command].binding[index];
        if ((b->alt == alt) && (b->ctrl == ctrl) && (b->shift == shift) && ((ubyte)b->key == key))
        {
            catKbBindings[command].count = 0;
        }
    }
    count = cat_kbCommandsForKey(key, commands);
    for (j = 0; j < count; j++)
    {
        other = commands[j];
        if (cat_kbCommandsCanShareKey(other, command, FALSE) || cat_kbCommandsCanShareKey(command, other, FALSE))
        {
            continue;
        }
        found = -1;
        for (index = 0; index < catKbBindings[other].count; index++)
        {
            b = &catKbBindings[other].binding[index];
            if (((ubyte)b->key == key) && (b->alt == alt) && (b->ctrl == ctrl) && (b->shift == shift))
            {
                found = index;
            }
        }
        if (found == -1)
        {                                                   // CAT-FIX: the exe then shifts from an unset index
            continue;
        }
        for (index = found; index < catKbBindings[other].count - 1; index++)
        {
            catKbBindings[other].binding[index] = catKbBindings[other].binding[index + 1];
        }
        catKbBindings[other].count--;
    }
    if (catKbBindings[command].count > CAT_KB_MAX_BINDINGS - 1)
    {
        catKbBindings[command].count = 0;
    }
    b = &catKbBindings[command].binding[catKbBindings[command].count];
    memset(b, 0, sizeof(*b));
    b->key = key;
    b->shift = shift;
    b->alt = alt;
    b->ctrl = ctrl;
    catKbBindings[command].count++;
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbCommandsForKey                              0x0054e680
    Description : The commands bound to the virtual key with the SHIFT / ALT / CTRL keys held now (a modifier bound as
                  the key itself does not have to match its own state; a binding without modifiers does not match while
                  one is held).
    Outputs     : commands - the commands
    Return      : how many
----------------------------------------------------------------------------*/
sdword cat_kbCommandsForKey(udword vk, sdword *commands)
{
    bool32 alt = CAT_KB_ALT_HELD(), shift = CAT_KB_SHIFT_HELD(), ctrl = CAT_KB_CTRL_HELD();
    sdword command, index, count = 0;
    CatKbBinding *b;
    bool32 matches, isModifier;
    ubyte key;

    for (command = 0; command < CAT_KB_NUMBER_COMMANDS; command++)
    {
        for (index = 0; index < catKbBindings[command].count; index++)
        {
            b = &catKbBindings[command].binding[index];
            key = (ubyte)b->key;
            if (key != vk)
            {
                continue;
            }
            isModifier = FALSE;
            matches = TRUE;
            if (key == 0x12)
            {
                isModifier = TRUE;
            }
            else if ((b->alt == 0) ? alt : !alt)
            {
                matches = FALSE;
            }
            if (key == 0x10)
            {
                isModifier = TRUE;
            }
            else if ((b->shift == 0) ? shift : !shift)
            {
                matches = FALSE;
            }
            if (key != 0x11)
            {
                if ((b->ctrl == 0) ? ctrl : !ctrl)
                {
                    matches = FALSE;
                }
                if (!isModifier && (b->ctrl == 0) && (b->alt == 0) && (b->shift == 0) && (alt || shift || ctrl))
                {
                    continue;
                }
            }
            if (matches)
            {
                commands[count] = command;
                count++;
            }
        }
    }
    return count;
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbBindingGet                                  0x0054e910
    Description : A copy of a command's bindings.
----------------------------------------------------------------------------*/
void cat_kbBindingGet(sdword command, CatKbCommandBindings *bindings)
{
    *bindings = catKbBindings[command];
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbCommandNameGet                              0x0054e940
    Description : A command's name (its default_keys.cfg row, from the language script).
----------------------------------------------------------------------------*/
char *cat_kbCommandNameGet(sdword command)
{
    if ((command <= 0) || (command >= CAT_KB_NUMBER_COMMANDS))
    {                                                       // CAT-FIX: the exe reads MessageStrings[-1] for row 0
        return "";
    }
    return strGetString(catKbCommandNames[command][0]);
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbModifiersMatch                              0x0054eb40
    Description : Are the SHIFT / ALT / CTRL keys held exactly as the binding wants (a binding of a modifier key itself
                  does not test that key).
----------------------------------------------------------------------------*/
bool32 cat_kbModifiersMatch(CatKbBinding *binding)
{
    ubyte key = (ubyte)binding->key;
    bool32 result = TRUE;

    if ((key != 0x12) && ((binding->alt == 0) ? CAT_KB_ALT_HELD() : !CAT_KB_ALT_HELD()))
    {
        result = FALSE;
    }
    if ((key != 0x10) && ((binding->shift == 0) ? CAT_KB_SHIFT_HELD() : !CAT_KB_SHIFT_HELD()))
    {
        result = FALSE;
    }
    if (key != 0x11)
    {
        if (binding->ctrl == 0)
        {
            if (CAT_KB_CTRL_HELD())
            {
                result = FALSE;
            }
        }
        else if (!CAT_KB_CTRL_HELD())
        {
            return FALSE;
        }
    }
    return result;
}

/*-----------------------------------------------------------------------------
    Name        : kbCommandKeyIsHit                                 0x0054ec60
    Description : Is a key of the command held, with its modifiers (the camera, sensors and band-box keys).
----------------------------------------------------------------------------*/
bool32 kbCommandKeyIsHit(udword command)
{
    sdword index;

    if (command >= CAT_KB_NUMBER_COMMANDS)
    {                                                       // CAT-FIX: the exe indexes its table unchecked
        return FALSE;
    }
    for (index = 0; index < catKbBindings[command].count; index++)
    {
        if (catKeyVkIsHit(catKbBindings[command].binding[index].key) &&
            cat_kbModifiersMatch(&catKbBindings[command].binding[index]))
        {
            return TRUE;
        }
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_moDebugKeyGet / cat_moDebugKeyValue           0x0054d3c0 / 0x0054d3d0
    Description : The key-binding list's group of a command, and the group's header ("Actions ----", ...; group 10 is
                  "DEBUG", any other none).
----------------------------------------------------------------------------*/
sdword cat_moDebugKeyGet(sdword command)
{
    return catKbDebugKeyGroups[command];
}

char *cat_moDebugKeyValue(sdword group)
{
    static const sdword headers[11] =
    {
        strUIGroup_ShipActions, strUIGroup_ShipSpecialActions, strUIGroup_ShipTactics, strUIGroup_ShipFormations,
        strUIGroup_Camera, strUIGroup_Selection, strUIGroup_Views, strUIGroup_ShipGroupings, strUIGroup_Misc,
        strUIGroup_SensorsViewOptions, strUIGroup_Debug
    };

    if ((group < 0) || (group > 10))
    {
        return NULL;                                        // the exe's default: 0
    }
    return strGetString(headers[group]);
}

/*=============================================================================
    CAT (M2-R6 mainrgn-popup): Cataclysm's key commands. The exe's table 0x008b6310 gives each command (the rows of
    default_keys.cfg, 1 = Move .. 0x96) two callbacks, both called with the sensors-manager state
    (cat_kbCommandExecute 0x0054eb10); the popup menu, the overlay's buttons and the key bindings run commands
    through it (the callbacks are the exe's small functions at 0x0054ecc0 .. 0x00550760, which the export does not
    list; named here by their commands). [M2-R6 startup-keys: every row of the exe's table is filled; the rows the
    exe leaves empty (Pan_Sensors_*, Zoom_*, Bandbox_*, Toggle_Overlay_Options, Show_Keys_*) are polled with
    kbCommandKeyIsHit by their users]
=============================================================================*/
#include "CameraCommand.h"
#include "CommandWrap.h"
#include "InfoOverlay.h"
#include "mainrgn.h"
#include "PopupMenu.h"
#include "Select.h"
#include "Sensors.h"
#include "ShipPrint.h"
#include "ShipSelect.h"
#include "SoundEvent.h"
#include "SpeechEvent.h"
#include "Tutor.h"
#include "Universe.h"

#include "GameChat.h"
#include "MultiplayerGame.h"
#include "NIS.h"
#include "PiePlate.h"
#include "render.h"
#include "Shader.h"
#include "Task.h"
#include "UnivUpdate.h"

typedef void (*CatKbCommandCB)(udword state);

// the Pause command's flag 0x00a6ed74 (its callback 0x00550640 toggles it; univUpdate stops while it is set: UnivUpdate.c
//      catUnivFrameStep; LoadUniverse, univUpdateReset and gameEnd clear it)
#define CAT_KB_PAUSED()             catUnivFrameStep

// the callbacks (each the exe's function of that row)
static void catKbMove(udword state)             { (void)state; cat_mrMoveShips(); }                  // 0x0054ecc0
static void catKbDock(udword state)             { (void)state; cat_mrDockShips(); }                  // 0x0054ecd0
static void catKbRetire(udword state)           { (void)state; mrRetire(NULL, NULL); }               // 0x0054ece0
static void catKbCancelOrders(udword state)                                                          // 0x0054ecf0
{
    (void)state;
    if ((tutorial != TUTORIAL_ONLY) || ((*(udword *)&tutEnable & 0x8000) != 0))
    {
        cat_uiHaltSelection();
        soundEventPlay(NULL, 0x108, NULL);
    }
}
static void catKbScuttle(udword state)          { (void)state; mrScuttle(NULL, NULL); }              // 0x0054ed30
static void catKbKamikaze(udword state)                                                              // 0x0054ed40
{
    sdword index;
    CommandToDo *command;

    (void)state;
    if (!MakeSelectionKamikazeCapable((SelectCommand *)&selSelected))
    {
        return;
    }
    for (index = 0; index < selSelected.numShips; index++)
    {                                                   // a ship already attacking or ramming answers no
        command = getShipAndItsCommand(&universe.mainCommandLayer, selSelected.ShipPtr[index]);
        if ((command != NULL) && ((command->ordertype.order == COMMAND_ATTACK) ||
                                  (command->ordertype.order == COMMAND_SPECIAL)))
        {
            break;
        }
    }
    if (index == selSelected.numShips)
    {
        if (selSelected.numShips > 0)
        {
            speechEventQueue(selSelected.ShipPtr[0], 0x4000809, selSelected.ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);
        }
        return;
    }
    soundEventPlay(NULL, 0x108, NULL);
    speechEventQueue(selSelected.ShipPtr[0], 0x40008a7, selSelected.ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);
    clWrapSetKamikaze(&universe.mainCommandLayer, (SelectCommand *)&selSelected);
}
static void catKbShipSpecialClear(udword state) { (void)state; mrSpecialBandBox = FALSE; }           // 0x0054ed20
static void catKbShipSpecial(udword state)                                                           // 0x0054ee20
{
    (void)state;
    if ((selSelected.numShips > 0) && !catMrSelecting && !mrSpecialBandBox)
    {                                                   // the popup menu of the special actions
        cat_popupMenuOpen(CAT_PM_TARGET_KEYCOMMAND, NULL);
    }
}
static void catKbHyperspace(udword state)       { (void)state; cat_mrHyperspace(); }                 // 0x005505f0
static void catKbWaypoints(udword state)        { (void)state; cat_mrWaypointsToggle(); }            // 0x0054ef60
static void catKbWaypointModes(udword state)    { (void)state; cat_mrWaypointModeCycle(); }          // 0x0054ef70
static void catKbWaypointNormal(udword state)   { (void)state; cat_uiSetWaypointMode(0); }           // 0x0054ef80
static void catKbWaypointEndToEnd(udword state) { (void)state; cat_uiSetWaypointMode(2); }           // 0x0054ef90
static void catKbWaypointCircular(udword state) { (void)state; cat_uiSetWaypointMode(1); }           // 0x0054efa0
static void catKbCancelLastWaypoint(udword state) { (void)state; cat_mrWaypointRemoveLast(); }       // 0x0054efb0
static void catKbUpgradeShips(udword state)     { (void)state; cat_mrUpgradeShips(); }               // 0x0054f190
static void catKbEnterSlipgate(udword state)    { (void)state; cat_mrEnterSlipgate(); }              // 0x005505e0
static void catKbHarvest(udword state)          { (void)state; mrHarvestResources(NULL, NULL); }     // 0x0054efc0
static void catKbRepair(udword state)           { (void)state; cat_uiSpecialRepair(NULL); }          // 0x0054efd0
static void catKbSalvage(udword state)          { (void)state; cat_uiSpecialSalvage(NULL); }         // 0x0054efe0
static void catKbLeechSpy(udword state)         { (void)state; cat_uiSetLeechAction(6); }            // 0x00550570
static void catKbLeechLeech(udword state)       { (void)state; cat_uiSetLeechAction(7); }            // 0x00550580
static void catKbLeechVent(udword state)        { (void)state; cat_uiSetLeechAction(8); }            // 0x00550590
static void catKbLeech(udword state)            { (void)state; cat_uiSpecialLeech(NULL); }           // 0x005505a0
static void catKbUnleech(udword state)          { (void)state; cat_uiUnleech(); }                    // 0x005505b0
static void catKbLink(udword state)             { (void)state; cat_uiLink(); }                       // 0x0054f040
static void catKbUnlink(udword state)           { (void)state; cat_uiUnlink(); }                     // 0x0054f050
static void catKbCancelMimic(udword state)      { (void)state; cat_uiUnmimic(); }                    // 0x0054f180
static void catKbForceField(udword state)       { (void)state; cat_uiProtectFormation(NULL); }       // 0x0054f120
static void catKbForceFieldUnlink(udword state) { (void)state; cat_uiProtectFormationCancel(); }     // 0x00550560
static void catKbCloak(udword state)            { (void)state; cat_uiCloakShips(); }                 // 0x0054f0b0
static void catKbCancelCloak(udword state)      { (void)state; cat_uiDecloakShips(); }               // 0x0054f0c0
static void catKbLaunchSwarmers(udword state)   { (void)state; cat_uiSpecialSwarm(NULL); }           // 0x0054f090
static void catKbCancelSwarmers(udword state)   { (void)state; cat_uiReleaseHiveSwarm(); }           // 0x0054f0a0
static void catKbBigGunAttack(udword state)     { (void)state; cat_uiFireSiegeCannon(); }            // 0x0054f1a0
static void catKbCancelBigGun(udword state)     { (void)state; cat_uiSiegeCannonCancel(); }          // 0x0054f1b0
static void catKbBeastAttack(udword state)      { (void)state; cat_uiSpecialInfect(NULL); }          // 0x005505c0
static void catKbEMP(udword state)              { (void)state; cat_uiSpecialEMP(NULL); }             // 0x0054f060
static void catKbRam(udword state)              { (void)state; cat_uiSpecialRam(NULL); }             // 0x005505d0
static void catKbRepulse(udword state)          { (void)state; cat_uiFireDreadnoughtCannon(); }      // 0x0054f070
static void catKbFireMissiles(udword state)     { (void)state; cat_uiSpecialFireMissile(NULL); }     // 0x0054f080
static void catKbTacticsEvasive(udword state)   { (void)state; cat_mrSetTactics(Evasive, TRUE); }    // 0x0054ee70
static void catKbTacticsNeutral(udword state)   { (void)state; cat_mrSetTactics(Neutral, TRUE); }    // 0x0054ee80
static void catKbTacticsAggressive(udword state) { (void)state; cat_mrSetTactics(Aggressive, TRUE); } // 0x0054ee90
static void catKbNextTactic(udword state)       { (void)state; cat_mrTacticsCycle(FALSE); }          // 0x0054eea0
static void catKbPrevTactic(udword state)       { (void)state; cat_mrTacticsCycle(TRUE); }           // 0x0054eeb0
static void catKbFormationDelta(udword state)   { (void)state; mrSetTheFormation(DELTA_FORMATION, TRUE, FALSE); }    // 0x0054eec0
static void catKbFormationBroad(udword state)   { (void)state; mrSetTheFormation(BROAD_FORMATION, TRUE, FALSE); }    // 0x0054eed0
static void catKbFormationDelta3D(udword state) { (void)state; mrSetTheFormation(DELTA3D_FORMATION, TRUE, FALSE); }  // 0x0054eee0
static void catKbFormationClaw(udword state)    { (void)state; mrSetTheFormation(CLAW_FORMATION, TRUE, FALSE); }     // 0x0054eef0
static void catKbFormationWall(udword state)    { (void)state; mrSetTheFormation(WALL_FORMATION, TRUE, FALSE); }     // 0x0054ef00
static void catKbFormationSphere(udword state)  { (void)state; mrSetTheFormation(SPHERE_FORMATION, TRUE, FALSE); }   // 0x0054ef10
static void catKbFormationCustom(udword state)  { (void)state; mrSetTheFormation(CUSTOM_FORMATION, TRUE, FALSE); }   // 0x0054ef20
static void catKbFormationParade(udword state)  { (void)state; mrSetTheFormation(SAME_FORMATION, TRUE, FALSE); }     // 0x0054ef30
static void catKbNextFormation(udword state)    { (void)state; cat_mrFormationCycle(FALSE); }        // 0x0054ef40
static void catKbPrevFormation(udword state)    { (void)state; cat_mrFormationCycle(TRUE); }         // 0x0054ef50
static void catKbFocus(udword state)                                                                 // cat_kbFocus 0x0054f2c0
{
    FocusCommand focus;

    if (state == 0)
    {
        if (selSelected.numShips != 0)
        {
            tutGameMessage("KB_Focus");
            soundEventPlay(NULL, 0x137, NULL);
            memcpy(&focus, &selSelected, sizeof(focus));
            ccFocus(&universe.mainCameraCommand, &focus);
        }
    }
    else if (state == 1)
    {
        soundEventPlay(NULL, 0x137, NULL);
        selSelectionCopy((MaxAnySelection *)&selSelecting, (MaxAnySelection *)&selSelected);
        smCullAndFocusSelecting();
    }
}
static void catKbFocusOnMothership(udword state) { (void)state; cat_mrFocusOnMothership(); }        // 0x0054f350
static void catKbFocusLastShipToSpeak(udword state) { (void)state; cat_ioFocusLastMessageShip(); }  // 0x0054f360
static void catKbCancelFocus(udword state)                                                           // 0x0054f370
{
    (void)state;
    ccCancelFocus(&universe.mainCameraCommand);
    soundEventPlay(NULL, 0x10a, NULL);
}
static void catKbForwardFocus(udword state)                                                          // 0x0054f390
{
    (void)state;
    ccForwardFocus(&universe.mainCameraCommand);
    soundEventPlay(NULL, 0x109, NULL);
}
static void catKbFleetView(udword state)        { (void)state; ccFocusOnFleetSmooth(&universe.mainCameraCommand); } // 0x0054f3b0
static void catKbChaseMode(udword state)                                                             // 0x0054f1c0
{
    FocusCommand focus;

    if (selSelected.numShips == 0)
    {
        bitClear(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP);
        return;
    }
    if (state == 0)
    {
        tutGameMessage("KB_Focus");
        soundEventPlay(NULL, 0x137, NULL);
        memcpy(&focus, &selSelected, sizeof(focus));
        ccFocus(&universe.mainCameraCommand, &focus);
    }
    else if (state == 1)
    {
        soundEventPlay(NULL, 0x137, NULL);
        selSelectionCopy((MaxAnySelection *)&selSelecting, (MaxAnySelection *)&selSelected);
        smCullAndFocusSelecting();
    }
    // CAT: the exe toggles its camera's chase bit (mainCameraCommand +0x8C bit 8, CCMODE_PILOT_SHIP here)
    if (bitTest(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP))
    {
        bitClear(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP);
    }
    else
    {
        bitSet(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP);
    }
    tutGameMessage("Chase");
}
static void catKbSelectNone(udword state)                                                            // 0x0054f4a0
{
    (void)state;
    soundEventPlay(NULL, 0x108, NULL);
    selSelected.numShips = 0;
    ioUpdateShipTotals();
}
static void catKbSensorsManager(udword state)                                                        // cat_kbSensorsManager 0x0054f530
{
    if (state == 0)
    {
        tutGameMessage("KB_Sensors");
        smSensorsBegin(NULL, NULL);
    }
    else if (!smSensorsDisable)
    {
        smSensorsClose(NULL, NULL);
    }
}
static void catKbBuildManager(udword state)                                                          // 0x0054f570
{
    (void)state;
    if (!playPackets && !CAT_KB_PAUSED())
    {
        tutGameMessage("KB_Build");
        mrBuildShips(NULL, NULL);
    }
}
static void catKbResearchManager(udword state)                                                       // 0x0054f5a0
{
    (void)state;
    if (!playPackets && !CAT_KB_PAUSED())
    {
        tutGameMessage("KB_Research");
        mrResearch(NULL, NULL);
    }
}
static void catKbLaunchManager(udword state)                                                         // 0x0054f5d0
{
    (void)state;
    if (!playPackets && !CAT_KB_PAUSED())
    {
        mrLaunch(NULL, NULL);
    }
}
static void catKbTacticalDisplay(udword state)                                                       // 0x00550450
{
    (void)state;
    mrDrawTactical ^= TRUE;
    if (mrDrawTactical)
    {
        soundEventPlay(NULL, 0x11f, NULL);
        tutGameMessage("KB_TacticalDisplayOn");
    }
    else
    {
        soundEventPlay(NULL, 0x120, NULL);
        tutGameMessage("KB_TacticalDisplayOff");
    }
}
static void catKbShipInfo(udword state)                                                              // 0x00550490
{
    (void)state;
    catShipPrintOn = !catShipPrintOn;
    soundEventPlay(NULL, catShipPrintOn ? 0x138 : 0x139, NULL);
}
static void catKbOpenMenu(udword state)         { (void)state; cat_popupMenuOpen(NULL, NULL); }      // 0x0054f4c0
static void catKbEscape(udword state)           { (void)state; cat_mrEscape(); }                     // 0x0054f5f0
static void catKbNextMusicTrack(udword state)   { (void)state; musicEventNextTrack(); }              // 0x00550510 -> 0x00523c50
static void catKbPrevMusicTrack(udword state)   { (void)state; musicEventPrevTrack(); }              // 0x00550520 -> 0x00523ca0
// the sensors-view toggles (0x00550270 .. 0x005503d0)
static void catKbSVAsteroids(udword state)      { (void)state; catSmViewResources = !catSmViewResources; }
static void catKbSVFOWRanges(udword state)      { (void)state; catSmViewFOWRanges = !catSmViewFOWRanges; }
static void catKbSVAreasVisited(udword state)   { (void)state; catSmViewAreasVisited = !catSmViewAreasVisited; tutGameMessage("AreasVisitedToggle"); }
static void catKbSVAreasOccupied(udword state)  { (void)state; catSmViewAreasOccupied = !catSmViewAreasOccupied; }
static void catKbSVIcons(udword state)          { (void)state; catSmViewIcons = !catSmViewIcons; }
static void catKbSVShipMeshes(udword state)     { (void)state; catSmViewShipMeshes = !catSmViewShipMeshes; }
static void catKbSVPieplateLines(udword state)  { (void)state; catSmViewPieplateLines = !catSmViewPieplateLines; }
static void catKbSVBackground(udword state)     { (void)state; catSmViewBackground = !catSmViewBackground; }
static void catKbSVDecorativeAsteroids(udword state) { (void)state; catSmViewDecorativeAsteroids = !catSmViewDecorativeAsteroids; }
static void catKbSVCompass(udword state)        { (void)state; catSmViewCompass = !catSmViewCompass; }
static void catKbSVObjectivePointers(udword state) { (void)state; catSmViewObjectivePointers = !catSmViewObjectivePointers; }
static void catKbSVSlipgatePaths(udword state)  { (void)state; catSmViewSlipgatePaths = !catSmViewSlipgatePaths; }

// CAT (M2-R6 startup-keys): the other rows' callbacks
static void catKbSelectAllVisible(udword state)                                                      // 0x0054f3c0
{
    MaxAnySelection previous;
    rectangle rect;
    Node *list;

    if ((tutorial == TUTORIAL_ONLY) && ((*(udword *)&tutEnable & 0x80000) == 0))
    {
        return;
    }
    rect.x0 = 0;
    rect.y0 = 0;
    rect.x1 = MAIN_WindowWidth;
    rect.y1 = MAIN_WindowHeight;
    selSelectionCopy(&previous, (MaxAnySelection *)&selSelected);
    list = (state == 0) ? universe.RenderList.head : universe.SpaceObjList.head;
    selRectDragFunction(list, &universe.mainCameraCommand.actualcamera, &rect, (SpaceObjRotImpTarg **)selSelected.ShipPtr,
                        &selSelected.numShips, TRUE, FALSE, FALSE, state != 0);
    ioUpdateShipTotals();
    if ((previous.numTargets != 0) && (selSelectionCompare((MaxAnySelection *)&selSelected, &previous) == 0))
    {                                                       // pressed again: nothing selected
        selSelected.numShips = 0;
    }
    selSelecting.numTargets = 0;
    ioUpdateShipTotals();
    soundEventPlay(NULL, 0x108, NULL);
}
static void catKbMultipleSelectRelease(udword state) { (void)state; cat_ioMultipleSelectRelease(); } // 0x00550530 -> 0x004ac4e0
static void catKbNextOverlayView(udword state)  { (void)state; cat_ioCycleMode(TRUE); }              // 0x00550550
static void catKbHeightMoveOn(udword state)                                                          // 0x0054f4d0
{
    (void)state;
    if (catMrRightMode != 2)
    {
        piePointModeToggle(TRUE);
    }
}
static void catKbHeightMoveOff(udword state)                                                         // 0x0054f4f0
{
    (void)state;
    if (catMrRightMode != 2)
    {
        piePointModeToggle(FALSE);
    }
}
static void catKbPageDownGameChat(udword state) { (void)state; cat_ioMessageScrollUp(); }            // 0x0054f510 -> 0x004aca90
static void catKbPageUpGameChat(udword state)   { (void)state; cat_ioMessageScrollDown(); }          // 0x0054f520 -> 0x004acad0
static void catKbStartMessage(bool32 toAllies)
{
    if (multiPlayerGame && (universe.curPlayerPtr != NULL) && (universe.curPlayerPtr->playerState != PLAYER_DEAD))
    {
        if (smSensorsActive)
        {
            smSensorsCloseForGood();
        }
        gcChatEntryStart(toAllies);
    }
}
static void catKbStartAllyMessage(udword state)   { (void)state; catKbStartMessage(TRUE); }          // 0x005503f0
static void catKbStartGlobalMessage(udword state) { (void)state; catKbStartMessage(FALSE); }         // 0x00550420
// CAT: no time key or pause while the NIS / KAS letterbox is fading in (0x005504f0 .. : fld nisScissorFadeIn
//      0x00a4cb38, compared with 0; kasfWideScreenIn and the NIS player write it)
#define CAT_KB_LETTERBOX_IN()       (nisScissorFadeIn != 0.0f)
static void catKbAccelerateTime(udword state)                                                        // 0x005504d0
{
    (void)state;
    if (!CAT_KB_LETTERBOX_IN())
    {
        cat_taskTimeCompressionIncrease();
    }
}
static void catKbDecelerateTime(udword state)                                                        // 0x005504f0
{
    (void)state;
    if (!CAT_KB_LETTERBOX_IN())
    {
        cat_taskTimeCompressionDecrease();
    }
}
static void catKbToggleGameSpeed(udword state)                                                       // 0x00550720
{
    (void)state;
    if (!multiPlayerGame && !CAT_KB_LETTERBOX_IN())
    {                                                       // normal speed goes to the fastest, any other to normal
        cat_taskTimeCompressionIndexSet((cat_timeCompressionFactor == 1.0f) ? (ubyte)cat_taskTimeCompressionMaxIndex() :
                                                                              cat_taskTimeCompressionDefaultIndex());
        cat_taskTimeCompressionApply();
        cat_taskTimeCompressionPrint();
    }
}
static void catKbOverlayBackdrops(udword state) { (void)state; cat_moPrintBoxesToggle(); }           // 0x00550540 -> 0x004acb90
static void catKbGammaUp(udword state)          { (void)state; shGammaUp(); }                        // 0x00550600
static void catKbGammaDown(udword state)        { (void)state; shGammaDown(); }                      // 0x00550620
// (the exe first tells a Glide device about the gamma, 0x6000 / 0x6001 through 0x00b3a70c: no Glide here, decision 28)
static void catKbPause(udword state)                                                                 // 0x00550640
{
    (void)state;
    if (multiPlayerGame || recordplayPacketsInGame || playPackets)
    {
        return;
    }
    tutGameMessage("KB_Pause");
    if ((tutorial == TUTORIAL_ONLY) && ((*(udword *)&tutEnable & 0x20) == 0))
    {
        return;
    }
    if (CAT_KB_LETTERBOX_IN() || nisFullyScissored)
    {
        return;
    }
    if (!catUnivFrameStep && (piePointSpecMode != PSM_Idle))
    {
        piePointModeOnOff(0, NULL);
    }
    catUnivFrameStep = !catUnivFrameStep;                   // univUpdate stops while it is set (UnivUpdate.c)
    dbgMessage(catUnivFrameStep ? "\nPause ON" : "\nPause OFF");    // (an empty stub in the exe)
    soundEventPlay(NULL, 0x108, NULL);
}
static void catKbTakeScreenshot(udword state)                                                        // 0x005506f0
{
    (void)state;
    soundEventPlay(NULL, 0x108, NULL);
    rndTakeScreenshot = TRUE;                               // 0x00a75d58: rndRenderTask takes it
}
static void catKbSkipSensors(udword state)      { (void)state; smSensorsSkip(NULL, NULL); }          // 0x00550710
// the hot-key groups: Create_ShipGroup_1..10 (0x0054f740 .. 0x0054fd70, one function per group)
static void catKbCreateHotKeyGroup(sdword group)
{
    bool32 reinforced;

    if ((group < 0) || (group >= SEL_NumberHotKeyGroups))
    {                                                       // CAT-FIX: group 10 indexes past the exe's ten groups
        return;
    }
    if (selSelected.numShips == 0)
    {
        return;
    }
    tutGameMessage("KB_GroupAssign");
    selHotKeyGroupRemoveReferences(group);
    reinforced = selSelectionIsReinforced((MaxAnySelection *)&selHotKeyGroup[group], (MaxAnySelection *)&selSelected);
    selSelectionCopy((MaxAnySelection *)&selHotKeyGroup[group], (MaxAnySelection *)&selSelected);
    selHotKeyNumbersSet(group);
    soundEventPlay(NULL, 0x109, NULL);
    speechEventQueue(NULL, reinforced ? 0x4001064 : 0x4001063, group, -1, 0,
                     selSelected.ShipPtr[0]->playerowner->playerIndex, -1, 5.0f, -1);
}
static void catKbCreateGroup1(udword state)  { (void)state; catKbCreateHotKeyGroup(1); }
static void catKbCreateGroup2(udword state)  { (void)state; catKbCreateHotKeyGroup(2); }
static void catKbCreateGroup3(udword state)  { (void)state; catKbCreateHotKeyGroup(3); }
static void catKbCreateGroup4(udword state)  { (void)state; catKbCreateHotKeyGroup(4); }
static void catKbCreateGroup5(udword state)  { (void)state; catKbCreateHotKeyGroup(5); }
static void catKbCreateGroup6(udword state)  { (void)state; catKbCreateHotKeyGroup(6); }
static void catKbCreateGroup7(udword state)  { (void)state; catKbCreateHotKeyGroup(7); }
static void catKbCreateGroup8(udword state)  { (void)state; catKbCreateHotKeyGroup(8); }
static void catKbCreateGroup9(udword state)  { (void)state; catKbCreateHotKeyGroup(9); }
static void catKbCreateGroup10(udword state) { (void)state; catKbCreateHotKeyGroup(10); }
// cat_kbSelectHotKeyGroup1..10 / cat_kbAddHotKeyGroup1..10 (0x0054fe20 .. 0x005501d0): cat_kbSelectHotKeyGroupX and
//      cat_kbAddHotKeyGroupX with the group
static void catKbSelectGroup1(udword state)  { cat_kbSelectHotKeyGroupX(state, 1); }
static void catKbSelectGroup2(udword state)  { cat_kbSelectHotKeyGroupX(state, 2); }
static void catKbSelectGroup3(udword state)  { cat_kbSelectHotKeyGroupX(state, 3); }
static void catKbSelectGroup4(udword state)  { cat_kbSelectHotKeyGroupX(state, 4); }
static void catKbSelectGroup5(udword state)  { cat_kbSelectHotKeyGroupX(state, 5); }
static void catKbSelectGroup6(udword state)  { cat_kbSelectHotKeyGroupX(state, 6); }
static void catKbSelectGroup7(udword state)  { cat_kbSelectHotKeyGroupX(state, 7); }
static void catKbSelectGroup8(udword state)  { cat_kbSelectHotKeyGroupX(state, 8); }
static void catKbSelectGroup9(udword state)  { cat_kbSelectHotKeyGroupX(state, 9); }
static void catKbSelectGroup10(udword state) { cat_kbSelectHotKeyGroupX(state, 10); }
static void catKbAddGroup1(udword state)     { cat_kbAddHotKeyGroupX(state, 1); }
static void catKbAddGroup2(udword state)     { cat_kbAddHotKeyGroupX(state, 2); }
static void catKbAddGroup3(udword state)     { cat_kbAddHotKeyGroupX(state, 3); }
static void catKbAddGroup4(udword state)     { cat_kbAddHotKeyGroupX(state, 4); }
static void catKbAddGroup5(udword state)     { cat_kbAddHotKeyGroupX(state, 5); }
static void catKbAddGroup6(udword state)     { cat_kbAddHotKeyGroupX(state, 6); }
static void catKbAddGroup7(udword state)     { cat_kbAddHotKeyGroupX(state, 7); }
static void catKbAddGroup8(udword state)     { cat_kbAddHotKeyGroupX(state, 8); }
static void catKbAddGroup9(udword state)     { cat_kbAddHotKeyGroupX(state, 9); }
static void catKbAddGroup10(udword state)    { cat_kbAddHotKeyGroupX(state, 10); }
static void catKbRemoveSelectedGroup(udword state)                                                   // 0x005501e0
{
    sdword index = 0, group = SEL_InvalidHotKey;

    (void)state;
    while (index < selSelected.numShips)
    {                                                       // the first selected ship that is in a group
        group = selHotKeyGroupNumberTest(selSelected.ShipPtr[index]);
        index++;
        if (group != SEL_InvalidHotKey)
        {
            break;
        }
    }
    if (group == SEL_InvalidHotKey)
    {
        return;
    }
    if ((index == 1) && (selSelectionCompare((MaxAnySelection *)&selSelected, (MaxAnySelection *)&selHotKeyGroup[group]) == 0))
    {                                                       // the selection is that group: it goes
        soundEventPlay(NULL, 0x108, NULL);
        selHotKeyGroupRemoveReferences(group);
        selHotKeyGroup[group].numShips = 0;
        return;
    }
    selHotKeyGroupRemoveReferencesFromAllGroups();
}

// cat_kbLeechCB 0x0054eff0 / cat_kbMimicShipsCB 0x0054f0d0 / cat_kbMimicPlayersCB 0x0054f130: the popup menu opens on
//      the submenu when a selected ship can leech / mimic
static void catKbOpenSubmenu(udword upgrade, void (*open)(void))
{
    MaxSelection selection;

    cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
    cat_selMakeShipsHaveUpgrade((SelectCommand *)&selection, upgrade);
    MakeShipsSpecialActivateCapable((SelectCommand *)&selection, TRUE);
    if (selection.numShips > 0)
    {
        open();
    }
}
void cat_kbLeechCB(udword state)                { (void)state; catKbOpenSubmenu(CAT_UPGRADE_Leech, cat_popupMenuOpenLeech); }
void cat_kbMimicShipsCB(udword state)           { (void)state; catKbOpenSubmenu(CAT_UPGRADE_Mimic, cat_popupMenuOpenMimicShips); }
void cat_kbMimicPlayersCB(udword state)         { (void)state; catKbOpenSubmenu(CAT_UPGRADE_Mimic, cat_popupMenuOpenMimicPlayers); }

// the exe's table 0x008b6310: {first callback, second callback} per command
static CatKbCommandCB catKbCommandTable[CAT_KB_NUMBER_COMMANDS][2] =
{
    [0x01] = { catKbMove, NULL },               [0x02] = { catKbDock, NULL },
    [0x03] = { catKbRetire, NULL },             [0x04] = { catKbCancelOrders, NULL },
    [0x05] = { catKbScuttle, NULL },            [0x06] = { catKbKamikaze, NULL },
    [0x07] = { catKbShipSpecialClear, catKbShipSpecial },
    [0x08] = { catKbHyperspace, NULL },         [0x09] = { catKbWaypoints, NULL },
    [0x0A] = { catKbWaypointModes, NULL },      [0x0B] = { catKbWaypointNormal, NULL },
    [0x0C] = { catKbWaypointEndToEnd, NULL },   [0x0D] = { catKbWaypointCircular, NULL },
    [0x0E] = { catKbCancelLastWaypoint, NULL }, [0x0F] = { catKbUpgradeShips, NULL },
    [0x10] = { catKbEnterSlipgate, NULL },      [0x11] = { catKbHarvest, NULL },
    [0x12] = { catKbRepair, NULL },             [0x13] = { catKbSalvage, NULL },
    [0x14] = { cat_kbLeechCB, NULL },           [0x15] = { catKbLeechSpy, NULL },
    [0x16] = { catKbLeechLeech, NULL },         [0x17] = { catKbLeechVent, NULL },
    [0x18] = { catKbLeech, NULL },              [0x19] = { catKbUnleech, NULL },
    [0x1A] = { catKbLink, NULL },               [0x1B] = { catKbUnlink, NULL },
    [0x1C] = { cat_kbMimicPlayersCB, NULL },    [0x1D] = { cat_kbMimicShipsCB, NULL },
    [0x1E] = { catKbCancelMimic, NULL },        [0x1F] = { catKbForceField, NULL },
    [0x20] = { catKbForceFieldUnlink, NULL },   [0x21] = { catKbCloak, NULL },
    [0x22] = { catKbCancelCloak, NULL },        [0x23] = { catKbLaunchSwarmers, NULL },
    [0x24] = { catKbCancelSwarmers, NULL },     [0x25] = { catKbBigGunAttack, NULL },
    [0x26] = { catKbCancelBigGun, NULL },       [0x27] = { catKbBeastAttack, NULL },
    [0x28] = { catKbEMP, NULL },                [0x29] = { catKbRam, NULL },
    [0x2A] = { catKbRepulse, NULL },            [0x2B] = { catKbFireMissiles, NULL },
    [0x2C] = { catKbTacticsEvasive, NULL },     [0x2D] = { catKbTacticsNeutral, NULL },
    [0x2E] = { catKbTacticsAggressive, NULL },  [0x2F] = { catKbNextTactic, NULL },
    [0x30] = { catKbPrevTactic, NULL },         [0x31] = { catKbFormationDelta, NULL },
    [0x32] = { catKbFormationBroad, NULL },     [0x33] = { catKbFormationDelta3D, NULL },
    [0x34] = { catKbFormationClaw, NULL },      [0x35] = { catKbFormationWall, NULL },
    [0x36] = { catKbFormationSphere, NULL },    [0x37] = { catKbFormationCustom, NULL },
    [0x38] = { catKbFormationParade, NULL },    [0x39] = { catKbNextFormation, NULL },
    [0x3A] = { catKbPrevFormation, NULL },      [0x3B] = { catKbFocus, NULL },
    [0x3C] = { catKbFocusOnMothership, NULL },  [0x3D] = { catKbFocusLastShipToSpeak, NULL },
    [0x3E] = { catKbCancelFocus, NULL },        [0x3F] = { catKbForwardFocus, NULL },
    [0x40] = { catKbFleetView, NULL },          [0x47] = { catKbChaseMode, NULL },
    [0x48] = { catKbSelectAllVisible, NULL },
    [0x49] = { catKbSelectNone, NULL },         [0x4E] = { NULL, catKbMultipleSelectRelease },
    [0x4F] = { catKbSensorsManager, NULL },
    [0x50] = { catKbBuildManager, NULL },       [0x51] = { catKbResearchManager, NULL },
    [0x52] = { catKbLaunchManager, NULL },      [0x53] = { catKbTacticalDisplay, NULL },
    [0x54] = { catKbShipInfo, NULL },           [0x55] = { catKbNextOverlayView, NULL },
    [0x56] = { catKbOpenMenu, NULL },           [0x57] = { catKbHeightMoveOn, catKbHeightMoveOff },
    [0x58] = { catKbPageDownGameChat, NULL },   [0x59] = { catKbPageUpGameChat, NULL },
    [0x5A] = { catKbEscape, NULL },             [0x5B] = { catKbStartAllyMessage, NULL },
    [0x5C] = { catKbStartGlobalMessage, NULL }, [0x5D] = { catKbAccelerateTime, NULL },
    [0x5E] = { catKbDecelerateTime, NULL },     [0x5F] = { catKbToggleGameSpeed, NULL },
    [0x60] = { catKbNextMusicTrack, NULL },     [0x61] = { catKbPrevMusicTrack, NULL },
    [0x63] = { catKbOverlayBackdrops, NULL },
    [0x67] = { catKbGammaUp, NULL },            [0x68] = { catKbGammaDown, NULL },
    [0x69] = { catKbPause, NULL },              [0x6A] = { catKbTakeScreenshot, NULL },
    [0x6B] = { catKbSkipSensors, NULL },
    [0x6C] = { catKbCreateGroup1, NULL },       [0x6D] = { catKbCreateGroup2, NULL },
    [0x6E] = { catKbCreateGroup3, NULL },       [0x6F] = { catKbCreateGroup4, NULL },
    [0x70] = { catKbCreateGroup5, NULL },       [0x71] = { catKbCreateGroup6, NULL },
    [0x72] = { catKbCreateGroup7, NULL },       [0x73] = { catKbCreateGroup8, NULL },
    [0x74] = { catKbCreateGroup9, NULL },       [0x75] = { catKbCreateGroup10, NULL },
    [0x76] = { catKbSelectGroup1, NULL },       [0x77] = { catKbSelectGroup2, NULL },
    [0x78] = { catKbSelectGroup3, NULL },       [0x79] = { catKbSelectGroup4, NULL },
    [0x7A] = { catKbSelectGroup5, NULL },       [0x7B] = { catKbSelectGroup6, NULL },
    [0x7C] = { catKbSelectGroup7, NULL },       [0x7D] = { catKbSelectGroup8, NULL },
    [0x7E] = { catKbSelectGroup9, NULL },       [0x7F] = { catKbSelectGroup10, NULL },
    [0x80] = { catKbAddGroup1, NULL },          [0x81] = { catKbAddGroup2, NULL },
    [0x82] = { catKbAddGroup3, NULL },          [0x83] = { catKbAddGroup4, NULL },
    [0x84] = { catKbAddGroup5, NULL },          [0x85] = { catKbAddGroup6, NULL },
    [0x86] = { catKbAddGroup7, NULL },          [0x87] = { catKbAddGroup8, NULL },
    [0x88] = { catKbAddGroup9, NULL },          [0x89] = { catKbAddGroup10, NULL },
    [0x8A] = { catKbRemoveSelectedGroup, NULL },
    [0x8B] = { catKbSVAsteroids, NULL },        [0x8C] = { catKbSVFOWRanges, NULL },
    [0x8D] = { catKbSVAreasVisited, NULL },     [0x8E] = { catKbSVAreasOccupied, NULL },
    [0x8F] = { catKbSVIcons, NULL },            [0x90] = { catKbSVShipMeshes, NULL },
    [0x91] = { catKbSVPieplateLines, NULL },    [0x92] = { catKbSVBackground, NULL },
    [0x93] = { catKbSVDecorativeAsteroids, NULL }, [0x94] = { catKbSVCompass, NULL },
    [0x95] = { catKbSVObjectivePointers, NULL },   [0x96] = { catKbSVSlipgatePaths, NULL },
};

/*-----------------------------------------------------------------------------
    Name        : cat_kbCommandExecute                              0x0054eb10
    Description : Runs a key command's two callbacks with the sensors-manager state.
----------------------------------------------------------------------------*/
void cat_kbCommandExecute(udword state, sdword command)
{
    if ((command < 0) || (command >= CAT_KB_NUMBER_COMMANDS))
    {                                                   // CAT-FIX: the exe indexes its table unchecked
        return;
    }
    if (catKbCommandTable[command][0] != NULL)
    {
        catKbCommandTable[command][0](state);
    }
    if (catKbCommandTable[command][1] != NULL)
    {
        catKbCommandTable[command][1](state);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbCommandKeyString                            0x0054e960
    Description : The names of a command's keys, " , " between them ("No Binding" for command 0); an unbound
                  command's list is empty.
----------------------------------------------------------------------------*/
void cat_kbCommandKeyString(sdword command, char *out)
{
    static char one[0x100];                                 // 0x00a6ed90
    char alt[0x80], shift[0x80], ctrl[0x80];
    CatKbBinding *b;
    sdword index;

    if (command == 0)
    {
        strcpy(out, strGetString(str_UI_NoKeyBinding));
        return;
    }
    out[0] = '\0';
    if ((command < 0) || (command >= CAT_KB_NUMBER_COMMANDS))
    {                                                       // CAT-FIX: the exe indexes its table unchecked
        return;
    }
    for (index = 0; index < catKbBindings[command].count; index++)
    {
        b = &catKbBindings[command].binding[index];
        sprintf(alt, "%s+", strGetString(strALTKEY));
        sprintf(shift, "%s+", strGetString(strSHIFTKEY));
        sprintf(ctrl, "%s+", strGetString(strCTRLKEY));
        sprintf(one, "%s%s%s%s", b->alt ? alt : "", b->shift ? shift : "", b->ctrl ? ctrl : "",
                cat_kbKeyToString(b->key & 0xff));
        if (index != 0)
        {
            strcat(out, " , ");
        }
        strcat(out, one);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbCommandKeyMatch                             0x0054ebc0
    Description : Is 'key' (with the SHIFT / ALT / CTRL keys held now) one of the command's keys; execute runs it.
----------------------------------------------------------------------------*/
bool32 cat_kbCommandKeyMatch(udword state, sdword command, udword key, bool32 execute)
{
    udword vk = catKeyVkFromKey(key);
    bool32 result = FALSE;
    sdword index;

    if ((command < 0) || (command >= CAT_KB_NUMBER_COMMANDS))
    {                                                       // CAT-FIX: the exe indexes its table unchecked
        return FALSE;
    }
    for (index = 0; index < catKbBindings[command].count; index++)
    {
        if (((catKbBindings[command].binding[index].key & 0xff) == vk) &&
            cat_kbModifiersMatch(&catKbBindings[command].binding[index]))
        {
            if (execute)
            {
                if (catKbCommandTable[command][0] != NULL)
                {
                    catKbCommandTable[command][0](state);
                }
                if (catKbCommandTable[command][1] != NULL)
                {
                    catKbCommandTable[command][1](state);
                }
            }
            result = TRUE;
        }
    }
    return result;
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbKeyPress                                    0x0054e7c0
    Description : A key down in the main region: with the overlay-options key held, '0'..'9' switch the ship info
                  panel's lines or the overlay's elements; else the popup menu's keys first, then the first callback of
                  every command bound to the key (in the fleet-intel sensors view only Skip_Sensors and Escape, which
                  skip it).
----------------------------------------------------------------------------*/
void cat_kbKeyPress(udword state, udword key)
{
    udword vk = catKeyVkFromKey(key);
    sdword commands[CAT_KB_NUMBER_COMMANDS];
    sdword count, index;
    CatKbCommandBindings *overlay = &catKbBindings[CAT_KB_ToggleOverlayOptions];

    for (index = 0; index < overlay->count; index++)
    {
        if (catKeyVkIsHit(overlay->binding[index].key) && cat_kbModifiersMatch(&overlay->binding[index]))
        {
            if ((vk > 0x2f) && (vk < 0x3a))
            {
                if (cat_shipPrintActive())
                {
                    cat_shipPrintToggle((char)vk);
                    return;
                }
                cat_moPrintOptionToggle((char)vk);
                return;
            }
            break;
        }
    }
    if (cat_popupMenuKeyPress(key))
    {
        return;
    }
    count = cat_kbCommandsForKey(vk, commands);
    for (index = 0; index < count; index++)
    {
        if (catKbCommandTable[commands[index]][0] != NULL)
        {
            if (!smFleetIntel)
            {
                catKbCommandTable[commands[index]][0](state);
            }
            else if ((commands[index] == CAT_KB_SkipSensors) || (commands[index] == CAT_KB_Escape))
            {
                smSensorsSkip(NULL, NULL);
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbKeyRelease                                  0x0054e8c0
    Description : A key up: the second callback of every command bound to the key.
----------------------------------------------------------------------------*/
void cat_kbKeyRelease(udword state, udword key)
{
    sdword commands[CAT_KB_NUMBER_COMMANDS];
    sdword count, index;

    count = cat_kbCommandsForKey(catKeyVkFromKey(key), commands);
    for (index = 0; index < count; index++)
    {
        if (catKbCommandTable[commands[index]][1] != NULL)
        {
            catKbCommandTable[commands[index]][1](state);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbSelectHotKeyGroupX                          0x0054fe30
    Description : Select_ShipGroup_N: the group becomes the selection; selecting the same group again focuses it (or,
                  in the sensors manager, focuses the sensors view on it).
----------------------------------------------------------------------------*/
void cat_kbSelectHotKeyGroupX(udword state, sdword group)
{
    MaxAnySelection previous;

    if ((group < 0) || (group >= SEL_NumberHotKeyGroups))
    {                                                       // CAT-FIX: group 10 indexes past the exe's ten groups
        return;
    }
    if (selHotKeyGroup[group].numShips == 0)
    {
        return;
    }
    selSelectionCopy(&previous, (MaxAnySelection *)&selSelected);
    selSelectHotKeyGroup(&selHotKeyGroup[group]);
    selHotKeyNumbersSet(group);
    ioUpdateShipTotals();
    if (selSelected.numShips <= 0)
    {
        return;
    }
    if ((previous.numTargets != 0) && (selSelectionCompare((MaxAnySelection *)&selSelected, &previous) == 0))
    {
        if (state == 0)
        {
            ccFocus(&universe.mainCameraCommand, (FocusCommand *)&selSelected);
            soundEventPlay(NULL, 0x108, NULL);
            speechEventQueue(NULL, 0x400105c, group, -1, 0, selSelected.ShipPtr[0]->playerowner->playerIndex, -1, 5.0f, -1);
            return;
        }
        if (state != 1)
        {
            return;
        }
        soundEventPlay(NULL, 0x108, NULL);
        speechEventQueue(NULL, 0x400105c, group, -1, 0, selSelected.ShipPtr[0]->playerowner->playerIndex, -1, 5.0f, -1);
        selSelectionCopy((MaxAnySelection *)&selSelecting, (MaxAnySelection *)&selSelected);
        smCullAndFocusSelecting();
        return;
    }
    soundEventPlay(NULL, 0x108, NULL);
    speechEventQueue(NULL, 0x400105c, group, -1, 0, selSelected.ShipPtr[0]->playerowner->playerIndex, -1, 5.0f, -1);
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbAddHotKeyGroupX                             0x00550060
    Description : AddTo_ShipGroup_N: the group's selectable ships (in a blob) added to the selection.
----------------------------------------------------------------------------*/
void cat_kbAddHotKeyGroupX(udword state, sdword group)
{
    sdword index;
    Ship *ship;

    (void)state;
    if ((group < 0) || (group >= SEL_NumberHotKeyGroups))
    {                                                       // CAT-FIX: group 10 indexes past the exe's ten groups
        return;
    }
    if ((tutorial == TUTORIAL_ONLY) && (selHotKeyGroup[group].numShips != 0))
    {
        tutGameMessage("KB_GroupAddSelect");
    }
    for (index = selHotKeyGroup[group].numShips - 1; index >= 0; index--)
    {
        ship = selHotKeyGroup[group].ShipPtr[index];
        if ((ship->obj.collMyBlob != NULL) && bitTest(ship->header.flags, SOF_Selectable))
        {
            selSelectionAddSingleShip(&selSelected, ship);
        }
    }
    selHotKeyNumbersSet(group);
    ioUpdateShipTotals();
    if (selHotKeyGroup[group].numShips > 0)
    {
        soundEventPlay(NULL, 0x108, NULL);
        speechEventQueue(NULL, 0x400105c, group, -1, 0, selHotKeyGroup[group].ShipPtr[0]->playerowner->playerIndex, -1,
                         5.0f, -1);
    }
}

/*=============================================================================
    HWI-TEST (M2-R6 startup-keys), port diagnostic, not in the exe: /catKeysTest (harness mode keys). After the
    start-up has loaded the bindings (cat_kbLoadKeyBindings: no keys.cfg in the run's settings path yet, so it is made
    from the install's default_keys.cfg), the test
      1. reads default_keys.cfg again line by line on its own and checks every "key" row whose command the exe knows
         against the loaded bindings (same key, same SHIFT / ALT / CTRL, in the file's order; the other rows, the
         install's 48 developer commands, must be unbound);
      2. keeps the keys.cfg the start-up left (made from default_keys.cfg, then saved again by opUpdateSettings ->
         opOptionsSaveSettings -> cat_kbSaveKeyBindings, as in the exe);
      3. saves (cat_kbSaveKeyBindings): the file must not change, and it is checked line for line against
         default_keys.cfg's known rows (the exe's save drops the developer rows and keeps the order) and its version;
      4. reloads from keys.cfg (cat_kbReloadKeyBindings(FALSE)) and resets (TRUE): the binding table must equal the
         loaded one both times;
      5. binds a new key to a command (cat_kbSetNewKey, no modifier held), saves, reloads and finds it, then resets;
      6. names every bound key (cat_kbKeyToString, cat_kbCommandKeyString, cat_kbBindingToString), finds every
         binding's commands for its key (cat_kbCommandsForKey with no modifier held, for the unmodified bindings),
         checks the conflict test (cat_kbCheckBindingConflict: an existing binding of another command is refused,
         cancel commands may share) and the debug-key groups;
    then saves once more (keys.cfg as the start-up left it), "keystest: done ... result=PASS|FAIL" and the engine
    quits. No key is pressed: only the binding code runs.
=============================================================================*/
bool32 catKeysSelfTest = FALSE;

static sdword catKeysFailures = 0;

#define CAT_KEYS_FAIL(...)  do { catKeysFailures++; dbgMessagef("keystest: FAIL " __VA_ARGS__); } while (0)

// a "key" row of a keys file as the exe's cat_kbKeySetCB reads it; FALSE for any other line
static bool32 catKeysRowParse(char *line, char *name, CatKbBinding *binding)
{
    char copy[CAT_KB_LINE_LENGTH], keys[0x100], *rowName, *value, *string;

    strncpy(copy, line, CAT_KB_LINE_LENGTH - 1);
    copy[CAT_KB_LINE_LENGTH - 1] = '\0';
    if (!parseLine(copy, &rowName, &value) || (strcmp(rowName, "key") != 0))
    {
        return FALSE;
    }
    if (sscanf(value, "%255s%255s", name, keys) != 2)
    {
        return FALSE;
    }
    memset(binding, 0, sizeof(*binding));
    binding->alt = (strstr(keys, "ALT+") != NULL);
    binding->shift = (strstr(keys, "SHIFT+") != NULL);
    binding->ctrl = (strstr(keys, "CTRL+") != NULL);
    string = strrchr(keys, '+');
    binding->key = (ubyte)atoi((string != NULL) ? string + 1 : keys);
    return TRUE;
}

static sdword catKeysCommandOf(char *name)
{
    sdword command, found = -1;

    for (command = 1; command < CAT_KB_NUMBER_COMMANDS; command++)
    {
        if (strcasecmp(name, strGetString(catKbCommandNames[command][0])) == 0)
        {
            found = command;
        }
    }
    return found;
}

// the known rows of a keys file, as "name  binding" strings in the file's order
static sdword catKeysKnownRows(char *path, char rows[][0x100], sdword maxRows, sdword *unknown)
{
    FILE *f = fopen(path, "rt");
    char line[CAT_KB_LINE_LENGTH], name[0x100], text[0x100];
    CatKbBinding binding;
    sdword count = 0;

    *unknown = 0;
    if (f == NULL)
    {
        return -1;
    }
    while (fgets(line, CAT_KB_LINE_LENGTH, f) != NULL)
    {
        if (!catKeysRowParse(line, name, &binding))
        {
            continue;
        }
        if (catKeysCommandOf(name) == -1)
        {
            (*unknown)++;
            continue;
        }
        cat_kbBindingToString(&binding, text, FALSE);
        if (count < maxRows)
        {
            snprintf(rows[count], 0x100, "%s  %s", strGetString(catKbCommandNames[catKeysCommandOf(name)][0]), text);
        }
        count++;
    }
    fclose(f);
    return count;
}

static bool32 catKeysFilesEqual(char *a, char *b)
{
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    sdword ca, cb, offset = 0;
    bool32 equal = (fa != NULL) && (fb != NULL);

    while (equal)
    {
        ca = fgetc(fa);
        cb = fgetc(fb);
        if (ca != cb)
        {
            equal = FALSE;
            dbgMessagef("keystest: files differ at byte %d (%d / %d)", offset, ca, cb);
        }
        if (ca == EOF)
        {
            break;
        }
        offset++;
    }
    if (fa != NULL)
    {
        fclose(fa);
    }
    if (fb != NULL)
    {
        fclose(fb);
    }
    return equal;
}

// two binding tables alike: the same counts and the same bindings up to them (a reload clears only the counts, as the
//      exe's loops: the slots past them keep old keys)
static bool32 catKeysTablesEqual(CatKbCommandBindings *a, CatKbCommandBindings *b)
{
    sdword command, index;

    for (command = 0; command < CAT_KB_NUMBER_COMMANDS; command++)
    {
        if (a[command].count != b[command].count)
        {
            dbgMessagef("keystest: command 0x%x has %d / %d bindings", command, a[command].count, b[command].count);
            return FALSE;
        }
        for (index = 0; index < a[command].count; index++)
        {
            if (memcmp(&a[command].binding[index], &b[command].binding[index], sizeof(CatKbBinding)) != 0)
            {
                dbgMessagef("keystest: command 0x%x binding %d differs", command, index);
                return FALSE;
            }
        }
    }
    return TRUE;
}

// a copy of a file (to compare a later version with)
static bool32 catKeysFileCopy(char *from, char *to)
{
    FILE *in = fopen(from, "rb"), *out;
    sdword c;

    if (in == NULL)
    {
        return FALSE;
    }
    out = fopen(to, "wb");
    if (out == NULL)
    {
        fclose(in);
        return FALSE;
    }
    while ((c = fgetc(in)) != EOF)
    {
        fputc(c, out);
    }
    fclose(in);
    fclose(out);
    return TRUE;
}

void catKeysRunSelfTest(void)
{
    static char defaultRows[256][0x100], savedRows[256][0x100];
    static CatKbCommandBindings loaded[CAT_KB_NUMBER_COMMANDS];
    FILE *f;
    char line[CAT_KB_LINE_LENGTH], name[0x100], text[0x200], message[0x100];
    CatKbBinding binding, *b;
    sdword expectedIndex[CAT_KB_NUMBER_COMMANDS];
    sdword commands[CAT_KB_NUMBER_COMMANDS];
    sdword rows = 0, known = 0, unknown = 0, mismatches = 0, command, index, count, defaultCount, savedCount;
    sdword boundCommands = 0, bindings = 0, named = 0, found = 0, unknownSaved, conflicts = 0, shares = 0, groups = 0;
    bool32 copied, savedSame, reloadSame, resetSame, newKeyKept, newKeyReset;

    dbgMessagef("keystest: start default=%s keys=%s loaded=%d", catKbDefaultPath(), catKbUserPath(), catKbBindingsLoaded);
    memset(expectedIndex, 0, sizeof(expectedIndex));

    // 1. default_keys.cfg against the loaded bindings
    f = fopen(catKbDefaultPath(), "rt");
    if (f == NULL)
    {
        CAT_KEYS_FAIL("cannot open %s", catKbDefaultPath());
    }
    else
    {
        while (fgets(line, CAT_KB_LINE_LENGTH, f) != NULL)
        {
            if (!catKeysRowParse(line, name, &binding))
            {
                continue;
            }
            rows++;
            command = catKeysCommandOf(name);
            if (command == -1)
            {
                unknown++;
                continue;
            }
            known++;
            index = expectedIndex[command]++;
            b = (index < catKbBindings[command].count) ? &catKbBindings[command].binding[index] : NULL;
            if ((b == NULL) || ((b->key & 0xff) != binding.key) || (b->shift != binding.shift) || (b->alt != binding.alt) ||
                (b->ctrl != binding.ctrl))
            {
                mismatches++;
                CAT_KEYS_FAIL("row %s binding %d differs", name, index);
            }
        }
        fclose(f);
    }
    for (command = 0; command < CAT_KB_NUMBER_COMMANDS; command++)
    {
        if (catKbBindings[command].count != expectedIndex[command])
        {
            mismatches++;
            CAT_KEYS_FAIL("command 0x%x has %d bindings, the file %d", command, catKbBindings[command].count,
                          expectedIndex[command]);
        }
        if (catKbBindings[command].count > 0)
        {
            boundCommands++;
            bindings += catKbBindings[command].count;
        }
    }
    dbgMessagef("keystest: loaded rows=%d known=%d unknown=%d commands=%d bindings=%d mismatches=%d", rows, known, unknown,
                boundCommands, bindings, mismatches);
    memcpy(loaded, catKbBindings, sizeof(loaded));

    // 2. the start-up's keys.cfg: made from default_keys.cfg by cat_kbLoadKeyBindings, then saved again by the
    //    start-up's opUpdateSettings (opOptionsSaveSettings 0x004d8d00 calls cat_kbSaveKeyBindings, as the exe's): it
    //    must hold default_keys.cfg's known rows (step 3 checks them) and an explicit save must not change it
    strncpy(text, catKbUserPath(), sizeof(text) - 8);
    text[sizeof(text) - 8] = '\0';
    strcat(text, ".start");
    copied = catKeysFileCopy(catKbUserPath(), text);

    // 3. the save
    cat_kbSaveKeyBindings();
    copied = copied && catKeysFilesEqual(text, catKbUserPath());
    if (!copied)
    {
        CAT_KEYS_FAIL("the start-up's keys.cfg differs from a save of the loaded bindings");
    }
    remove(text);
    defaultCount = catKeysKnownRows(catKbDefaultPath(), defaultRows, 256, &unknown);
    savedCount = catKeysKnownRows(catKbUserPath(), savedRows, 256, &unknownSaved);
    savedSame = (defaultCount == savedCount) && (defaultCount > 0) && (unknownSaved == 0) && (defaultCount <= 256);
    for (index = 0; savedSame && (index < defaultCount); index++)
    {
        if (strcmp(defaultRows[index], savedRows[index]) != 0)
        {
            savedSame = FALSE;
            CAT_KEYS_FAIL("saved row %d '%s', default '%s'", index, savedRows[index], defaultRows[index]);
        }
    }
    f = fopen(catKbUserPath(), "rt");
    if ((f == NULL) || (fgets(line, CAT_KB_LINE_LENGTH, f) == NULL) || (strncmp(line, "v ", 2) != 0) ||
        (atoi(line + 2) != catKbVersion))
    {
        savedSame = FALSE;
        CAT_KEYS_FAIL("saved version line");
    }
    if (f != NULL)
    {
        fclose(f);
    }
    if (!savedSame)
    {
        CAT_KEYS_FAIL("saved rows %d (unknown %d), default known rows %d", savedCount, unknownSaved, defaultCount);
    }
    dbgMessagef("keystest: saved rows=%d default=%d version=%d same=%d", savedCount, defaultCount, catKbVersion, savedSame);

    // 4. reload and reset
    cat_kbReloadKeyBindings(FALSE);
    reloadSame = catKeysTablesEqual(loaded, catKbBindings);
    cat_kbReloadKeyBindings(TRUE);
    resetSame = catKeysTablesEqual(loaded, catKbBindings) && catKeysFilesEqual(catKbDefaultPath(), catKbUserPath());
    if (!reloadSame || !resetSame)
    {
        CAT_KEYS_FAIL("reload %d reset %d", reloadSame, resetSame);
    }
    dbgMessagef("keystest: reload same=%d reset same=%d", reloadSame, resetSame);

    // 5. a new key: F1 (VK 0x70) for Retire (3), saved and reloaded, then the defaults again
    cat_kbSetNewKey(3, 0x70);
    cat_kbSaveKeyBindings();
    cat_kbReloadKeyBindings(FALSE);
    newKeyKept = (cat_kbCommandsForKey(0x70, commands) >= 1);
    for (index = 0, found = 0; index < cat_kbCommandsForKey(0x70, commands); index++)
    {
        found |= (commands[index] == 3);
    }
    newKeyKept = newKeyKept && found;
    cat_kbReloadKeyBindings(TRUE);
    newKeyReset = catKeysTablesEqual(loaded, catKbBindings);
    if (!newKeyKept || !newKeyReset)
    {
        CAT_KEYS_FAIL("new key kept %d reset %d", newKeyKept, newKeyReset);
    }
    dbgMessagef("keystest: new key kept=%d reset=%d (%s)", newKeyKept, newKeyReset, cat_kbKeyToString(0x70));

    // 6. names, commands for keys, conflicts, groups
    found = 0;
    for (command = 1; command < CAT_KB_NUMBER_COMMANDS; command++)
    {
        for (index = 0; index < catKbBindings[command].count; index++)
        {
            b = &catKbBindings[command].binding[index];
            if ((cat_kbKeyToString(b->key) != NULL) && (cat_kbKeyToString(b->key)[0] != '\0'))
            {
                named++;
            }
            if (!b->shift && !b->alt && !b->ctrl)
            {
                count = cat_kbCommandsForKey(b->key & 0xff, commands);
                while (count-- > 0)
                {
                    if (commands[count] == command)
                    {
                        found++;
                        break;
                    }
                }
            }
            else
            {
                found++;
            }
        }
        cat_kbCommandKeyString(command, text);
        if ((catKbBindings[command].count > 0) && (text[0] == '\0'))
        {
            CAT_KEYS_FAIL("no key string for command 0x%x", command);
        }
        if ((cat_moDebugKeyValue(cat_moDebugKeyGet(command)) != NULL) && (cat_kbCommandNameGet(command)[0] != '\0'))
        {
            groups++;
        }
    }
    if ((named != bindings) || (found != bindings))
    {
        CAT_KEYS_FAIL("named %d found %d of %d bindings", named, found, bindings);
    }
    binding = catKbBindings[1].binding[0];                  // Move's key, for Dock: refused
    if (cat_kbCheckBindingConflict(&binding, 2, 0, message) == 0)
    {
        conflicts++;
        dbgMessagef("keystest: conflict '%s'", message);
    }
    binding = catKbBindings[0x19].binding[0];               // UnLeech's key, for Unlink: shared
    if (cat_kbCheckBindingConflict(&binding, 0x1b, 0, message) != 0)
    {
        shares++;
    }
    if ((conflicts != 1) || (shares != 1) || (groups != CAT_KB_NUMBER_COMMANDS - 1))
    {
        CAT_KEYS_FAIL("conflicts %d shares %d groups %d", conflicts, shares, groups);
    }
    cat_kbCommandKeyString(1, text);
    cat_kbBindingToString(&catKbBindings[5].binding[0], message, TRUE);
    dbgMessagef("keystest: names named=%d found=%d groups=%d Move='%s' %s='%s' Pause='%s'", named, found, groups, text,
                cat_kbCommandNameGet(5), message, cat_kbKeyToString(catKbBindings[0x69].binding[0].key));
    cat_kbSaveKeyBindings();                                // the run leaves keys.cfg as the start-up's save made it
    dbgMessagef("keystest: done rows=%d known=%d unknown=%d commands=%d bindings=%d startsave=%d saved=%d reload=%d reset=%d "
                "newkey=%d conflicts=%d shares=%d failures=%d result=%s", rows, known, rows - known, boundCommands,
                bindings, copied, savedSame, reloadSame, resetSame, newKeyKept && newKeyReset, conflicts, shares,
                catKeysFailures, (catKeysFailures == 0) ? "PASS" : "FAIL");
}

