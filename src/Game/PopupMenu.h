// =============================================================================
//  PopupMenu.h
//  - Cataclysm's right-click popup menu: the orders of the selection, the
//    special actions on a target, the submenus (tactics, formations, the
//    sensors view, focus, views, ship names, waypoint types, the mimic and
//    Leech choices, alliances) (cataclysm.exe 0x004eaa60-0x004ef860)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___POPUPMENU_H
#define ___POPUPMENU_H

#include "Color.h"
#include "ShipSelect.h"
#include "SpaceObj.h"
#include "Types.h"
#include "prim2d.h"

// CAT: new file (M2-R6 mainrgn-popup). The exe links these 32 functions right after PlugScreen.c (psCurrentScreenDelete
//      0x004ea960) and before Randy.c (ranRandomFn 0x004ef860): a source file of their own. Names of the file, the
//      types and the variables are chosen here; the item tables are the exe's data (0x0088de88 ..).

// one menu row (0x3C bytes in the exe; the tables are arrays of them)
typedef struct CatPopupItem
{
    char *text;             // +0x00 a text of its own (a ship or player name, a mimic item), else strings[strId]
    sdword strId;           // +0x04
    sdword command;         // +0x08 the key command it runs (cat_kbCommandExecute) and whose key it shows
    sdword submenu;         // +0x0C 0, or the submenu it opens (cat_popupMenuGetSubmenu 1..13)
    sdword group;           // +0x10 rows of one group sit together; the main menu leaves a gap between groups
    bool32 visible;         // +0x14
    bool32 disabled;        // +0x18 shown grey, not selectable
    bool32 flash;           // +0x1C shown flashing (red to yellow)
    bool32 checked;         // +0x20 a check mark
    bool32 hover;           // +0x24 under the mouse (cat_popupMenuHitTestItems)
    rectangle rect;         // +0x28 where the text is drawn (cat_popupMenuLayout)
    sdword id;              // +0x38 the row's index in its table
} CatPopupItem;

#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatPopupItem) == 0x3C, "a popup menu row is 0x3C bytes in cataclysm.exe");
#endif

// the main menu's rows (0x0088de88, 49 rows); rows 0x18.. are the special actions (cat_popupMenuExecuteSpecial)
#define CAT_PM_ShipName                 0x00    // the one selected ship's name (shown, not selectable)
#define CAT_PM_SpecialAction            0x01
#define CAT_PM_Upgrade                  0x02
#define CAT_PM_Move                     0x03
#define CAT_PM_Hyperspace               0x04
#define CAT_PM_CancelMove               0x05
#define CAT_PM_StartWaypoints           0x06
#define CAT_PM_EndWaypoints             0x07
#define CAT_PM_WaypointType             0x08
#define CAT_PM_CancelWaypoints          0x09
#define CAT_PM_RemoveLastWaypoint       0x0A
#define CAT_PM_Dock                     0x0B
#define CAT_PM_Retire                   0x0C
#define CAT_PM_Cancel                   0x0D
#define CAT_PM_Tactics                  0x0E
#define CAT_PM_Formations               0x0F
#define CAT_PM_Focus                    0x10
#define CAT_PM_View                     0x11
#define CAT_PM_SensorsView              0x12
#define CAT_PM_SetShipName              0x13
#define CAT_PM_SPHyperspace             0x14
#define CAT_PM_AllianceForm             0x15
#define CAT_PM_AllianceBreak            0x16
#define CAT_PM_TransferRUs              0x17
#define CAT_PM_NoSpecialAction          0x18
#define CAT_PM_Harvest                  0x19
#define CAT_PM_Repair                   0x1A
#define CAT_PM_Salvage                  0x1B
#define CAT_PM_SelectLeechType          0x1C
#define CAT_PM_Leech                    0x1D
#define CAT_PM_Unleech                  0x1E
#define CAT_PM_Link                     0x1F
#define CAT_PM_Unlink                   0x20
#define CAT_PM_SelectMimicType          0x21
#define CAT_PM_SelectMimicItem          0x22
#define CAT_PM_Unmimic                  0x23
#define CAT_PM_ForceField               0x24
#define CAT_PM_UnlinkForceField         0x25
#define CAT_PM_Cloak                    0x26
#define CAT_PM_CancelCloak              0x27
#define CAT_PM_LaunchSwarmers           0x28
#define CAT_PM_CancelSwarmers           0x29
#define CAT_PM_FireBigGun               0x2A
#define CAT_PM_CancelBigGun             0x2B
#define CAT_PM_BeastAttack              0x2C
#define CAT_PM_EMP                      0x2D
#define CAT_PM_Ram                      0x2E
#define CAT_PM_Repulse                  0x2F
#define CAT_PM_FireMissiles             0x30
#define CAT_PM_NumberItems              0x31

// the submenus (cat_popupMenuGetSubmenu)
#define CAT_PMS_Tactics                 1
#define CAT_PMS_Formations              2
#define CAT_PMS_SensorsView             3
#define CAT_PMS_LeechType               4
#define CAT_PMS_Focus                   5
#define CAT_PMS_View                    6
#define CAT_PMS_ShipNames               7
#define CAT_PMS_WaypointType            8
#define CAT_PMS_MimicPlayers            9
#define CAT_PMS_MimicItems              10
#define CAT_PMS_AllianceForm            11
#define CAT_PMS_AllianceBreak           12
#define CAT_PMS_TransferRUs             13
#define CAT_PMS_Number                  14

// the target of a menu opened for the mimic / Leech choices (cat_popupMenuOpenMimicShips etc. store 1)
#define CAT_PM_TARGET_KEYCOMMAND        ((SpaceObj *)1)

extern CatPopupItem catPmItems[CAT_PM_NumberItems];        // 0x0088de88
extern sdword catPmSelected;                                // 0x0088dde0: the selected main row (-1 none)
extern bool32 catPmLargeFont;                               // 0x00a5749c: the options' LargePopupMenuFonts (OP_SmallFonts)
extern sdword catPmSubSelected;                             // 0x0088dde4: the selected submenu row (-1 none)

void cat_popupMenuFindSpecialTarget(sdword item);                                          // 0x004eaa60
bool32 cat_popupMenuMouseInRect(rectangle *rect);                                          // 0x004eaaf0
void cat_popupMenuSetupSensorsItems(void);                                                 // 0x004eab40
void cat_popupMenuBuildTargetItems(void);                                                  // 0x004eac30
void cat_popupMenuLayout(sdword x, sdword y, CatPopupItem *items, sdword count, rectangle *rect,
                         sdword *checkCol, sdword *keyCol, bool32 forceCheck, bool32 isMain);   // 0x004eb0b0
void cat_popupMenuLayoutAll(void);                                                         // 0x004eb3e0
void cat_popupMenuStartup(void);                                                           // 0x004eb7e0
void cat_popupMenuOpen(SpaceObj *target, MaxSelection *selection);                         // 0x004eb810
void cat_popupMenuBuildSelectionItems(void);                                               // 0x004eb940
void cat_popupMenuOpenMimicShips(void);                                                    // 0x004ec220
void cat_popupMenuOpenMimicPlayers(void);                                                  // 0x004ec3c0
void cat_popupMenuOpenLeech(void);                                                         // 0x004ec560
void cat_popupMenuClose(void);                                                             // 0x004ec6f0
bool32 cat_popupMenuIsOpen(void);                                                          // 0x004ec710
bool32 cat_popupMenuMouseInside(void);                                                     // 0x004ec720
void cat_popupMenuDrawFrame(rectangle *rect, bool32 fill, sdword margin);                  // 0x004ec9a0
void cat_popupMenuDrawItems(CatPopupItem *items, sdword count, sdword checkCol, sdword keyCol, bool32 isMain);  // 0x004ecb30
void cat_popupMenuFilterTargets(bool32 keepEnemies);                                       // 0x004ecf60
bool32 cat_popupMenuExecuteSpecial(sdword item);                                           // 0x004ecfe0
void cat_popupMenuSpecialFireMissile(void);                                                // 0x004ed510
void cat_popupMenuGetSubmenu(sdword submenu, CatPopupItem **items, sdword *count, rectangle *rect);  // 0x004ed550
bool32 cat_popupMenuMimicSelect(sdword item);                                              // 0x004ed8b0
void cat_selShipScreenRectDraw(Ship *ship, color c);                                       // 0x004ed9b0
void cat_popupMenuDrawTargetHighlights(void);                                              // 0x004edc70
void cat_popupMenuDraw(void);                                                              // 0x004ee120
bool32 cat_popupMenuClickIfTarget(void);                                                   // 0x004ee5d0
bool32 cat_popupMenuClick(void);                                                           // 0x004ee5f0
bool32 cat_popupMenuHitTestItems(bool32 isMain, CatPopupItem *items, sdword count, rectangle *rect);  // 0x004eec90
void cat_popupMenuUpdateHover(bool32 force);                                               // 0x004eee30
bool32 cat_popupMenuActivateSelected(void);                                                // 0x004ef010
bool32 cat_popupMenuKeyPress(udword key);                                                  // 0x004ef1c0
sdword cat_popupMenuHotkeyScan(CatPopupItem *items, bool32 isMain, sdword first, sdword last, udword key,
                               bool32 *matched);                                           // 0x004ef670
void cat_popupMenuRemoveShip(Ship *ship);                                                  // 0x004ef840

// HWI-TEST(M2-R6 ui self-test): the main menu rows the last build made visible, for /catUiTest
sdword cat_popupMenuVisibleCount(void);
SpaceObj *cat_popupMenuTarget(void);
void catUiSelfTestSet(char *string);            // /catUiTest <ticks> (harness mode ui)
bool32 catUiSelfTestRequested(void);
void catUiSelfTestTick(void);                   // after each universe tick (OracleDump.c oracleUnivUpdate)
void catUiSelfTestPopupDrawn(void);
void catUiSelfTestStatsFrame(void);             // after each game-stats frame (mrRegionDraw)

#endif
