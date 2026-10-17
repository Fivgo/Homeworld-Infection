// =============================================================================
//  mouse.h
//  - Hardware abstraction for the mouse.
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/26/1997 by lmoloney
// =============================================================================

#ifndef ___MOUSE_H
#define ___MOUSE_H

#include "prim2d.h"
#include "ShipSelect.h"
#include "SpaceObj.h"
#include "Types.h"

/*=============================================================================
    Definitions:
=============================================================================*/
#define MB_Left             0x0001
#define MB_Right            0x0002
#define MB_Centre           0x0004

//mouse cursor special modes
#define MCM_Pointmode       0x0001
#define MCM_SingleClick     0x0010
#define MCM_DoubleClick     0x0020
#define MCM_CursorOverObj   0x0040

//mouse cursorover types
#define MC_ENEMY_SHIP   -1
#define MC_RESOURCE     -2
#define MC_NO_SHIP      -3
#define MC_ALLIED_SHIP  -4
#define MC_DERELICT     -5

//mouse cursor hold time (in seconds)
#define MC_MOUSEHOLDTIME 1.0

//cursor colors
#define MC_ContentColor     colRGB( 32, 176,  12)
#define MC_EdgeColor        colRGB(200, 200, 200)

//cursor bitmap filenames
#define MC_PATH                     "CursorBitmaps//"
#define MC_NORMAL_BM                MC_PATH"cursor.LiF"
#define MC_ADD_SHIPS_BM             MC_PATH"add.LiF"
#define MC_FOCUS_NO_SELECT_BM       MC_PATH"eye.LiF"
#define MC_BAND_BOX_ATTACK_BM       MC_PATH"attack.LiF"
#define MC_FORCE_ATTACK_BM          MC_PATH"exclaim.LiF"
#define MC_GUARD_BM                 MC_PATH"shield.LiF"
#define MC_SPECIAL_ATTACK_BM        MC_PATH"special_activate.LiF"
#define MC_SPECIAL_OP_ACTIVATE_BM   MC_PATH"special_activate.LiF"
#define MC_SPECIAL_OP_DEACTIVATE_BM MC_PATH"special_deactivate.LiF"
#define MC_GUI_BM                   MC_PATH"dialog.LiF"
#define MC_RESOURCE_BM              MC_PATH"resource.LiF"
#define MC_DOCKING_BM               MC_PATH"dock.LiF"
#define MC_RESEARCH_BM              MC_PATH"dialog.LiF"
#define MC_SENSORS_BM               MC_PATH"dialog.LiF"
#define MC_BUILD_BM                 MC_PATH"dialog.LiF"
#define MC_SALVAGE_BM               MC_PATH"special_activate.LiF"
#define MC_SUPPORT_BM               MC_PATH"support.LiF"
#define MC_TRADERS_BM               MC_PATH"trader.LiF"

#define MOUSE_IdentifyLODMask       0x3       // LOD required in order to identify a ship

/*=============================================================================
    Data:
=============================================================================*/
extern sdword mouseCursorXPosition, mouseCursorYPosition;
extern sdword lastUnderWidth, lastUnderHeight;
extern uword mouseButtons;
extern bool8 mouseIsVisible;
extern uword mouseWillBeVisible;
extern bool8 mouseDisabled;
extern SpaceObj *mouseCursorObjPtr;
extern SpaceObj *mouseCursorObjLast;
extern MaxSelection mouseCursorSelect;
extern real32 mouseCursorLastObjTime;
extern sdword mouseCursorLastX;
extern sdword mouseCursorLastY;

/*=============================================================================
    Macros:
=============================================================================*/
//enumerated type for different mouse types
typedef enum
{
    no_mouse = -1,
    normal_mouse,
    rotate_camera,
    zoom_camera,
    add_ships,
    focus_no_select,
    movement,
    band_box_attack,
    force_attack,
    m_guard,
    special_attack,
    special_op_activate,
    special_op_deactivate,
    gui,
    resource,
    derelict,
    docking,
    small_docking,
    research,
    sensors,
    build,
    salvage,
    support,
    force_field,                // CAT: new: Cataclysm's cursors (mouseDraw 0x0056ec50 / mouseGetLIF 0x0056e9c0); 22
    traders,                    // CAT: was 22
    enemy_ship,
    allied_ship,
    own_ship,
    slipgate_small,             // CAT: new, 27
    slipgate_medium,
    slipgate_large,
    leech,                      // CAT: new, 30
    link_cursor,                // ("link" would shadow POSIX link())
    mimic,
    emp,
    swarm,
    fire_missile,               // CAT: new, 35
    ramming,
    beast_weapon,
    objectives,                 // CAT: new, 38: the main overlay's objective clicked (cat_moClickedFlagConsume)
    end_cursor_type
} mouseCursor;


#define mouseCursorX()          (mouseCursorXPosition)
#define mouseCursorY()          (mouseCursorYPosition)
#define mouseLeftButton()       (keyIsHit(LMOUSE_BUTTON))   //(bitTest(mouseButtons, MB_Left))
#define mouseRightButton()      (keyIsHit(RMOUSE_BUTTON))   //(bitTest(mouseButtons, MB_Right))
#define mouseCentreButton()     (0)   //(bitTest(mouseButtons, MB_Centre))
#define mouseLeftDouble()       (keyIsHit(LMOUSE_DOUBLE))
#define mouseRightDouble()       (keyIsHit(RMOUSE_DOUBLE))

#define mouseInRect(r)          (mouseCursorXPosition >= (r)->x0 && \
                                 mouseCursorYPosition >= (r)->y0 && \
                                 mouseCursorXPosition < (r)->x1 &&  \
                                 mouseCursorYPosition < (r)->y1)
#define mouseInRectGeneral(x0, y0, x1, y1)  (mouseCursorXPosition >= (x0) && \
                                             mouseCursorYPosition >= (y0) && \
                                             mouseCursorXPosition < (x1) &&  \
                                             mouseCursorYPosition < (y1))

/*=============================================================================
    Functions:
=============================================================================*/
//start/stop mouse module
sdword mouseStartup(void);
void mouseReset(void);
void mouseShutdown(void);

//hide/show the cursor
void mouseCursorHide(void);
void mouseCursorShow(void);
void mouseCursorDelayShow(uword delay);

//disable/enable cursor
void mouseDisable(void);
void mouseEnable(void);

//click special callbacks
bool32 mouseLDoubleClick(void);
bool32 mouseDoubleClickCheck(void);
void mouseLClick(void);
void mouseClickShipDied(ShipPtr deadship);

//set new position for mouse
void mousePositionSet(sdword x, sdword y);
void mouseClipToRect(rectangle *rect);
void mouseClipPointToRect(sdword *x, sdword *y, rectangle *rect);

//draw mouse cursor and update cursor position/button flags
void mouseDraw(void);
void mouseDrawType(mouseCursor cursortype);
void mouseSelectCursorSetting();
void mouseSetCursorSetting();
void mouseCursorSet(udword mode);
void mouseCursorClear(udword mode);

//cursor text code
void mouseCursorTextDraw(void);

//poll current location of mouse and store internally
void mousePoll(void);

/*=============================================================================
    CAT (M2-R6 mainrgn-popup): the context actions (cat_mouseGetContextAction 0x0056f8f0): what a click on an object
    does for the selection. The exe's two jump tables over the action give the cursor (mouseSetCursorSetting
    0x0056f3b0, table 0x0056f70c for 1..17) and the right-click order (cat_mrReleaseRight 0x0056b0b0, table
    0x0056b650 for 0..17); catMouseContextActions is that table (checked entry by entry, docs/m2-r6-mainrgn-popup.md)
=============================================================================*/
typedef enum
{
    CAT_CA_None = 0,                // the popup menu
    CAT_CA_Harvest,                 // 1 a resource for the workers
    CAT_CA_Salvage,                 // 2 a salvage target
    CAT_CA_Attack,                  // 3
    CAT_CA_Repair,                  // 4 an ally to repair
    CAT_CA_ProtectFormation,        // 5 an ally to put a force-field sphere round
    CAT_CA_SlipgateSmall,           // 6 a slipgate to enter
    CAT_CA_SlipgateMedium,          // 7
    CAT_CA_SlipgateLarge,           // 8
    CAT_CA_Dock,                    // 9 an own ship every selected ship may dock with
    CAT_CA_Leech,                   // 10 an enemy to leech
    CAT_CA_Link,                    // 11 an own ship to link with
    CAT_CA_Mimic,                   // 12 a ship (or an asteroid) to pose as
    CAT_CA_EMP,                     // 13 (3 with the Homeworld CSA option)
    CAT_CA_Swarm,                   // 14 (3 with the option)
    CAT_CA_Ram,                     // 15 (3 with the option)
    CAT_CA_FireMissiles,            // 16 (3 with the option)
    CAT_CA_Infect,                  // 17 the Beast weapon (3 with the option)
    CAT_CA_Number
} CatContextAction;

// the right-click order of an action (cat_mrReleaseRight's cases)
#define CAT_CAO_Popup               0       // the popup menu of the selection
#define CAT_CAO_Harvest             1       // mrHarvestResources on it
#define CAT_CAO_Special             2       // cat_uiSpecialTargetCommand(it, ability)
#define CAT_CAO_Attack              3       // clWrapAttack
#define CAT_CAO_Protect             4       // cat_uiProtectFormation
#define CAT_CAO_Slipgate            5       // mrDockingOrders
#define CAT_CAO_Dock                6       // clWrapDock purpose 0
#define CAT_CAO_Link                7       // cat_clWrapLink with it
#define CAT_CAO_Mimic               8       // cat_mrMimicTarget

typedef struct
{
    sdword cursor;                  // the mouseCursorType mouseSetCursorSetting gives (0: none, the action 0)
    sdword order;                   // CAT_CAO_*
    sdword ability;                 // the special order's ability (cat_uiSpecialTargetCommand), -1 for none
} CatContextActionInfo;

extern const CatContextActionInfo catMouseContextActions[CAT_CA_Number];

sdword cat_mouseGetContextAction(CatTargetActions *selection, CatTargetActions *target, MaxSelection *ships, Ship *ship);   // 0x0056f8f0
void cat_mouseSaveCursorUnder(void);                                                    // 0x0056ebb0
void cat_mouseRestoreCursorUnder(void);                                                 // 0x0056eb40

#endif
