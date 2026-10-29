/*=============================================================================
    Name    : mouse.c
    Purpose : Hardware abstraction for the mouse.

    Created 6/26/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "mouse.h"

extern bool32 catHiddenWindow;      // HWI-TEST (M3-R4 mode-switch-fix): render.c, /catHiddenWindow

#include <SDL2/SDL.h>

#include "CommandWrap.h"
#include "Debug.h"
#include "Demo.h"
#include "Dock.h"
#include "FEFlow.h"
#include "font.h"
#include "FontReg.h"
#include "glinc.h"
#include "Globals.h"
#include "GravWellGenerator.h"
#include "InfoOverlay.h"
#include "KeyBindings.h"
#include "main.h"
#include "mainrgn.h"
#include "Memory.h"
#include "Mimic.h"
#include "PopupMenu.h"
#include "BeastWeapon.h"
#include "NIS.h"
#include "render.h"
#include "ResearchGUI.h"
#include "Select.h"
#include "Sensors.h"
#include "ShipSelect.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "StringSupport.h"
#include "texreg.h"
#include "Tutor.h"
#include "Tweak.h"
#include "Universe.h"
#include "utility.h"
#include "uiscale.h"         // HWI-FEATURE(ui-scale): window coordinates <-> logical

#ifdef _WIN32
    #include <windows.h>
#endif


/*=============================================================================
    Local Type Definitions:
=============================================================================*/
#ifdef HW_BUILD_FOR_DEBUGGING
    #define MOUSE_CURSOR_CLAMP   1    // clamp location of cursor to bounds of the window
#else
    #define MOUSE_CURSOR_CLAMP   1
#endif

#define GLC_STORE 1

//flag settings
#define MCF_SpecialOp                   0x00000001  //has a special operation
#define MCF_SpecialOpDeployed           0x00000002  //has a special operation already deployed
#define MCF_Dockable                    0x00000004  //can dock (big ship)
#define MCF_SmallDockable               0x00000008  //can dock (small ship)
#define MCF_GUICapable                  0x00000010  //is associated with a GUI
#define MCF_NonCombat                   0x00000020  //ship is non combat ship
#define MCF_SupportOverItself           0x00000040  //the mouse is over a selected repair corvette
#define MCF_SingleClickSpecialActivate  0x00000080  //selection includes 1 or more single-click special activate ships
#define MCF_DisableAll                  0x00000100  //disable all cursor bitmaps
#define MCF_DisableMovementMech         0x00000200  //disable cursors when movement pie plate is activated
#define MCF_ShipSelecting               0x00001000  //ships are being selected
#define MCF_BuildGUI                    0x00002000  //ship opens the build manager
#define MCF_SensorsGUI                  0x00004000  //ship opens the sensors manager
#define MCF_ResearchGUI                 0x00008000  //ship opens the research manager
#define MCF_SpecialAttack               0x00010000  //ship has a special attack
#define MCF_SpecialSCSupport            0x00020000  //ship has a special support function for strikecraft
#define MCF_SpecialSupport              0x00040000  //ship has a special support function
#define MCF_SalvageOnly                 0x00080000  //shiplist only contains salvage ships
#define MCF_Salvage                     0x00100000  //shiplist contains at least one salvage ship

typedef struct
{
    udword ship_selected;
    udword resource;
    udword flags;
    mouseCursor cursorover;
} mouseInfoType;


/*=============================================================================
    Data:
=============================================================================*/
//we don't want the player to be able to salvage cryotrays in mission1
//so we need to include singlePlayerGameInfo
extern SinglePlayerGameInfo singlePlayerGameInfo;

sdword mouseCursorXPosition, mouseCursorYPosition;
uword mouseButtons;
udword mouseModuleInit;
rectangle mouseClipRect;
bool8 mouseClip = FALSE;
bool8 mouseIsVisible = TRUE;
uword mouseWillBeVisible = 0;
bool8 mouseDisabled = FALSE;
uword mouseInsideClient = FALSE;

static bool32 mouseGLInitialized;

mouseCursor  mouseCursorType = normal_mouse;
udword       mouseCursorMode = 0;
MaxSelection mouseCursorSelect;
real32       mouseDoubleClickTime   = 0.0;
real32       mouseDoubleClickExpire = REALlyBig;

real32      mouseCursorOverDistance = REALlyBig;
sdword      mouseCursorOverShipType = MC_NO_SHIP;
sdword      mouseCursorEnemyType = -1;
sdword      mouseCursorEnemyPlayer = 1;
real32      mouseCursorTextExpire   = 0.0f;
SpaceObj    *mouseCursorObjPtr      = NULL;
SpaceObj    *mouseCursorObjLast     = NULL;
real32      mouseCursorLastObjTime  = REALlyNegative;
sdword      mouseCursorLastX        = SDWORD_Max;
sdword      mouseCursorLastY        = SDWORD_Max;
ubyte       mouseCursorOverLODs     = 0;

//cursor colors
static color mouseCursorContentColor = MC_ContentColor;
static color mouseCursorEdgeColor    = MC_EdgeColor;

//cursor font
fonthandle mouseCursorFont = 0;

//cursor bitmaps
// CAT: Cataclysm's 31 cursor bitmaps and their textures (mouseStartup 0x0056d670: 0x00a75810 .., mouseInitGL 0x0056e400:
//      0x00a75890 ..), 32 slots with slot 3 unused (HW1 kept 18 named statics)
#define MC_SLOT_NUMBER              32
static char *mouseBitmapFiles[MC_SLOT_NUMBER] =
{
    MC_PATH"cursor.LiF",                // 0 normal_mouse, movement
    MC_PATH"add.LiF",                   // 1 add_ships
    MC_PATH"eye.LiF",                   // 2 focus_no_select
    NULL,                               // 3 (unused)
    MC_PATH"attack.LiF",                // 4 band_box_attack
    MC_PATH"exclaim.LiF",               // 5 force_attack
    MC_PATH"shield.LiF",                // 6 m_guard
    MC_PATH"special_activate.LiF",      // 7 special_attack
    MC_PATH"special_activate.LiF",      // 8 special_op_activate
    MC_PATH"special_deactivate.LiF",    // 9 special_op_deactivate
    MC_PATH"dialog.LiF",                // 10 gui
    MC_PATH"resource.LiF",              // 11 resource
    MC_PATH"dock.LiF",                  // 12 docking, small_docking
    MC_PATH"dialog.LiF",                // 13 research
    MC_PATH"dialog.LiF",                // 14 sensors
    MC_PATH"dialog.LiF",                // 15 build
    MC_PATH"salvage.LiF",               // 16 salvage (CAT: HW1 drew special_activate.LiF)
    MC_PATH"support.LiF",               // 17 support
    MC_PATH"force_field.LiF",           // 18 force_field (CAT: new from here)
    MC_PATH"trader.LiF",                // 19 traders
    MC_PATH"slipgate_small.LiF",        // 20
    MC_PATH"slipgate_medium.LiF",       // 21
    MC_PATH"slipgate_large.LiF",        // 22
    MC_PATH"leech.LiF",                 // 23
    MC_PATH"link.LiF",                  // 24
    MC_PATH"mimic.LiF",                 // 25
    MC_PATH"emp.LiF",                   // 26
    MC_PATH"swarm.LiF",                 // 27
    MC_PATH"fire_missile.LiF",          // 28
    MC_PATH"ramming.LiF",               // 29
    MC_PATH"beast_weapon.LiF",          // 30
    MC_PATH"objectives.LiF",            // 31
};
static lifheader *mouseBitmaps[MC_SLOT_NUMBER];
static GLuint mouseTextures[MC_SLOT_NUMBER];
// a cursor type's slot (the switches of mouseGetLIF 0x0056e9c0 and mouseDraw 0x0056ec50; -1 none: the triangle)
static const sbyte mouseCursorSlot[end_cursor_type] =
{
    0, -1, -1, 1, 2, 0, 4, 5, 6, 7, 8, 9, 10, 11, -1, 12, 12, 13, 14, 15, 16, 17, 18, 19, -1, -1, -1, 20, 21, 22,
    23, 24, 25, 26, 27, 28, 29, 30, 31
};
// the special-attack cursor stays a second (mouseDraw: 0x00a75920 / 0x00a75924, real time)
static mouseCursor mouseHoldType = normal_mouse;
static real32 mouseHoldExpire = 0.0f;


static mouseInfoType mouseInfo = {0, 0, 0, MC_NO_SHIP};


/*=============================================================================
    Functions:
=============================================================================*/
// ============================================================================
// Utility functions
// ============================================================================
/*-----------------------------------------------------------------------------
    Name        : mouseMirrorBitmap
    Description : Flips a bitmap 180 degrees
    Inputs      : bitmap - pointer to the bitmap data
                  width - width of the bitmap
                  height - height of the bitmap
    Outputs     : replaces "bitmap" with a bitmap flipped 90 degrees clockwise
    Return      :
----------------------------------------------------------------------------*/
#define PIXEL(bm, x, y, offset, width) bm[((x) + (y)*(width))+(offset)]

void mouseMirrorBitmap(udword *bitmap, sdword width, sdword height)
{
    udword i, j;
    udword new_bitmap[1024];

    for (i = 0; i < (udword)width; i++)
    {
        for (j = 0; j < (udword)height; j++)
        {
            PIXEL(new_bitmap, i, (height - 1) - j, 0, width) = PIXEL(bitmap, i, j, 0, width);
        }
    }

    memcpy(bitmap,new_bitmap,sizeof(udword)*width*height);
}
#undef PIXEL

/*-----------------------------------------------------------------------------
    Name        : mouseCreateGLHandle
    Description : creates a GL texture object for a given liff
    Inputs      : lif - the lifheader structure of the bitmap
                  thandle - target for the texture name
    Outputs     : thandle contains the texture name
    Return      :
----------------------------------------------------------------------------*/
void mouseCreateGLHandle(lifheader* lif, GLuint* thandle)
{
    sdword y;
    ubyte* data;
    ubyte* sp;
    ubyte* dp;

    data = (ubyte*)memAlloc(4 * lif->width * lif->height, "temp mouse texture", 0);

    for (y = 0; y < lif->height; y++)
    {
        dp = data + 4 * lif->width * (lif->height - y - 1);
        sp = lif->data + 4 * lif->width * y;
        memcpy(dp, sp, 4 * lif->width);
    }

    if (*thandle != 0)
    {
        glDeleteTextures(1, thandle);
    }

    trClearCurrent();

    glGenTextures(1, thandle);
    glBindTexture(GL_TEXTURE_2D, *thandle);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, lif->width, lif->height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    memFree(data);
}

// ============================================================================
// Mouse functions
// ============================================================================
/*-----------------------------------------------------------------------------
    Name        : mouseStartup
    Description : Start the mouse module
    Inputs      : void
    Outputs     : sets the mouse position by getting information from the
                    Windows mouse system.
    Return      : OKAY
----------------------------------------------------------------------------*/
sdword mouseStartup(void)
{
    sdword slot;

    /*
    RECT clientRect, windowRect;
    POINT mousePoint;

    GetWindowRect(ghMainWindow, &windowRect);
    GetClientRect(ghMainWindow, &clientRect);               //get location of window
    GetCursorPos(&mousePoint);                              //get location of Windows mouse cursor
    mouseCursorXPosition = mousePoint.x - windowRect.left;  //compute relative coords
    mouseCursorYPosition = mousePoint.y - windowRect.top;
    */
    SDL_GetMouseState(&mouseCursorXPosition, &mouseCursorYPosition);

    for (slot = 0; slot < MC_SLOT_NUMBER; slot++)
    {                                                       // CAT: the 31 cursors, each mirrored
        if (mouseBitmapFiles[slot] != NULL)
        {
            mouseBitmaps[slot] = trLIFFileLoad(mouseBitmapFiles[slot], NonVolatile);
        }
    }
    for (slot = 0; slot < MC_SLOT_NUMBER; slot++)
    {
        if (mouseBitmaps[slot] != NULL)
        {
            mouseMirrorBitmap((udword *)mouseBitmaps[slot]->data, mouseBitmaps[slot]->width, mouseBitmaps[slot]->height);
        }
    }

    /*mouseDoubleClickTime = GetDoubleClickTime() / 1000.0f;*/
    mouseDoubleClickTime = 0.5f;  /* Consistent with HandleEvent() in main.c. */
    mouseCursorFont      = frFontRegister(TW_CURSORTEXT_FONT);

    mouseGLInitialized = FALSE;

    return(OKAY);
}

/*-----------------------------------------------------------------------------
    Name        : mouseInitGL
    Description : initialize the GL texture handles for mouse bitmaps
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mouseInitGL(void)
{
    sdword slot;

    for (slot = 0; slot < MC_SLOT_NUMBER; slot++)
    {                                                       // CAT: 31 textures (0x0056e400)
        if (mouseBitmaps[slot] != NULL)
        {
            mouseCreateGLHandle(mouseBitmaps[slot], &mouseTextures[slot]);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : mouseReset
    Description : reset mouse module (delete associated GL texture objects)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mouseReset(void)
{
    sdword slot;

    if (mouseGLInitialized)
    {
        for (slot = 0; slot < MC_SLOT_NUMBER; slot++)
        {                                                   // CAT: 31 textures (0x0056e620)
            if (mouseBitmaps[slot] != NULL)
            {
                glDeleteTextures(1, &mouseTextures[slot]);
            }
            mouseTextures[slot] = 0;
        }
        mouseGLInitialized = FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : mouseShutdown
    Description : Close mouse module
    Inputs      : void
    Outputs     : not much
    Return      : void
----------------------------------------------------------------------------*/
void mouseShutdown(void)
{
    mouseReset();
}

/*-----------------------------------------------------------------------------
    Name        : mouseCursorHide
    Description : Show the cursor if it is hidden
    Inputs      : void
    Outputs     : sets the visibility flag to TRUE.  Subsequent mouse
                    mouseDraw() calls will not display a visible cursor.
    Return      : void
----------------------------------------------------------------------------*/
void mouseCursorHide(void)
{
    mouseIsVisible = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : mouseCursorShow
    Description : Show the cursor if it is hidden
    Inputs      : void
    Outputs     : sets the visibility flag to TRUE.  Subsequent mouse
                    mouseDraw() calls will not display a visible cursor.
    Return      : void
----------------------------------------------------------------------------*/
void mouseCursorShow(void)
{
    mouseIsVisible = TRUE;
}

void mouseCursorDelayShow(uword delay)
{
    mouseWillBeVisible = delay+1;
}

/*-----------------------------------------------------------------------------
    Name        : mousePositionSet
    Description : Set new mouse position.
    Inputs      : x, y - new location of mouse.  May be clamped to the edge of
                    the window.
    Outputs     : Sets the Windows cursor location and the internal cursor location.
    Return      : void
----------------------------------------------------------------------------*/
void mousePositionSet(sdword x, sdword y)
{
    //rectangle clientRect;
//    RECT clientRect, windowRect;
//    POINT mousePoint;

    //utyClientRectGet(&clientRect);                          //get window location
//  GetWindowRect(ghMainWindow, &windowRect);
//  GetClientRect(ghMainWindow, &clientRect);               //get location of window
/*
#if MOUSE_CURSOR_CLAMP
    //clamp specified mouse location
    if (x < 0)
    {
        x = 0;
    }
    if (y < 0)
    {
        y = 0;
    }
    if (x >= clientRect.x1 - clientRect.x0)
    {
        x = clientRect.x1 - clientRect.x0 - 1;
    }
    if (y >= clientRect.y1 - clientRect.y0)
    {
        y = clientRect.y1 - clientRect.y0 - 1;
    }
#endif
*/
    // CAT: the exe clamps the position to the client rectangle (mousePositionSet 0x0056e8d0: utyClientRectGet, then
    //      0 .. width - 1 / height - 1, MOUSE_CURSOR_CLAMP on; GoK commented the clamp out above). The exe's client
    //      rectangle is MAIN_WindowWidth x MAIN_WindowHeight, the tree's logical screen (HWI-FEATURE(ui-scale))
    if (x < 0)
    {
        x = 0;
    }
    if (y < 0)
    {
        y = 0;
    }
    if (x >= MAIN_WindowWidth)
    {
        x = MAIN_WindowWidth - 1;
    }
    if (y >= MAIN_WindowHeight)
    {
        y = MAIN_WindowHeight - 1;
    }
    mouseCursorXPosition = x;                               //set internal mouse location
    mouseCursorYPosition = y;
    /*
    mousePoint.x = clientRect.x0 + x;                       //compute Windows mouse location
    mousePoint.y = clientRect.y0 + y;
    SetCursorPos(mousePoint.x, mousePoint.y);               //set Windows mouse location
    */

    if (!mouseClip && !catHiddenWindow)             // HWI-TEST: a hidden test window never moves the user's pointer
    {
        uiScaleLogicalToMouse(&x, &y);              // HWI-FEATURE(ui-scale): the logical position in SDL's window coordinates
        SDL_WarpMouseInWindow(NULL, x, y);
    }
}

/*-----------------------------------------------------------------------------
    Name        : mouseClipToRect
    Description : Clips the mouse to a given rectangle.
    Inputs      : rect - rectangle to clip to.  NULL means no rectangle.
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mouseClipToRect(rectangle *rect)
{
    sdword x, y;
    if (rect == NULL)
    {
        mouseClip = FALSE;
    }
    else
    {
        mouseClip = TRUE;
        mouseClipRect = *rect;
        SDL_GetRelativeMouseState(&x, &y); // prime the next poll to start at 0,0
    }
    if (!catHiddenWindow)                   // HWI-TEST: a hidden test window never grabs the user's pointer
    {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
}


/*-----------------------------------------------------------------------------
    Name        : mouseLDoubleClick
    Description : Processes a double click on the left mouse button
    Inputs      : void
    Outputs     : Very specific depending on the state of mouseInfo
    Return      : TRUE if some processing has been done (overrides the processing
                  for a single left click)
                  FALSE if no processing was done (processing for the single
                  left click continues)
----------------------------------------------------------------------------*/
bool32 mouseLDoubleClick(void)
{
    MaxSelection tempSelection;

    //if a menu is up, don't do any of this
//    if (feStackIndex > 0)
//    {
//        if (rmGUIActive)
//        {
//            return TRUE;
//        }
//        else
//        {
//            return FALSE;
//        }
//    }

//**** big chunks taken out because this function is being called
//     from mainrgn now.

//    if (bitTest(mouseCursorMode, MCM_SingleClick))
//    {
//        if (bitTest(mouseCursorMode, MCM_DoubleClick))
//        {
//            bitClear(mouseCursorMode, MCM_DoubleClick);
//            bitClear(mouseCursorMode, MCM_SingleClick);
//            mouseCursorSelect.numShips = 0;
//        }
//        else
//        {
//            bitSet(mouseCursorMode, MCM_DoubleClick);
//            mouseDoubleClickExpire = universe.totaltimeelapsed + mouseDoubleClickTime;
//            return FALSE;
//        }
//    }

    if (selSelected.numShips)
    {
#if !MR_GUI_SINGLECLICK
//        if (bitTest(mouseInfo.flags, MCF_BuildGUI))
//        {
//            tutGameMessage("Game_DoubleClickBuild");
//            cmConstructionBegin(ghMainRegion,(sdword)selSelected.ShipPtr[0], 0, 0);
//            return TRUE;
//        }
//        else if (bitTest(mouseInfo.flags, MCF_SensorsGUI))
//        {
//            tutGameMessage("Game_DoubleClickSensors");
//            smSensorsBegin(NULL, NULL);
//            return TRUE;
//        }
//        else if (bitTest(mouseInfo.flags, MCF_ResearchGUI))
//        {
//            tutGameMessage("Game_DoubleClickResearch");
//            rmResearchGUIBegin(ghMainRegion,(sdword)selSelected.ShipPtr[0], 0, 0);
//        }
//        else
#endif  // !MR_GUI_SINGLECLICK
//        if (ShiptypeInSelection((SelectCommand *)&mouseCursorSelect, RepairCorvette) ||
//            ((ShiptypeInSelection((SelectCommand *)&mouseCursorSelect, AdvanceSupportFrigate) ||
//              ShiptypeInSelection((SelectCommand *)&mouseCursorSelect, Carrier)) &&
//             (mouseCursorObjPtr &&
//              (isShipOfClass((Ship *)mouseCursorObjPtr, CLASS_Fighter) ||
//               isShipOfClass((Ship *)mouseCursorObjPtr, CLASS_Corvette)))))

        // This will still act strangely in strange cases: if you have a RCorv and a SF selected and
        // double-click on a carrier, both will support, contrary to what the cursor shows.
        // If you have only the RCorv selected, it will dock.  I'd say that at least in this example
        // the actions are intuitively right.  Unfortunately the cursor shape follows
        // an entirely separate logic (in mouseSetCursorSetting).
        if (mouseCursorObjPtr &&                // if there's an object being clicked on
            (((isShipOfClass(mouseCursorObjPtr, CLASS_Fighter) ||        // Carriers only support strikecraft
               isShipOfClass(mouseCursorObjPtr, CLASS_Corvette)) &&
              ShiptypeInSelection((SelectCommand *)&mouseCursorSelect, Carrier)) ||
            // CAT: no RepairCorvette clause, Cataclysm has no repair corvette type (M2-R4 ships-b retire; stage 3 UI)
            ShiptypeInSelection((SelectCommand *)&mouseCursorSelect, AdvanceSupportFrigate)))
        {
            // make a backup of mouseCursorSelect
            tempSelection.numShips = 0;
            selSelectionCopy((MaxAnySelection *)&tempSelection, (MaxAnySelection *)&mouseCursorSelect);

            MakeShipsSpecialTargetCapable((SelectCommand *)&mouseCursorSelect,TRUE, TRUE);   // CAT: skip-disabled argument (UI callers pass 1, e.g. cat_kbLeechCB 0x0054eff0)
            MakeShipsNotIncludeTheseShips((SelectCommand *)&mouseCursorSelect, (SelectCommand *)&selSelected);

            if (mouseCursorSelect.numShips > 0)
            {
                SelectCommand sel;

                selSelectionCopy((MaxAnySelection *)&selSelected,
                                 (MaxAnySelection *)&mouseCursorSelect);

				ioUpdateShipTotals();

                if (mouseCursorSelect.numShips < tempSelection.numShips)
                {
                    // the selection being activated has other ships as well
                    tutGameMessage("Game_DoubleClickSpecial");
                    sel.numShips = 1;
                    sel.ShipPtr[0] = (Ship *)mouseCursorObjPtr;
                    clWrapSpecial(&universe.mainCommandLayer, (SelectCommand *)&mouseCursorSelect, (SpecialCommand *)&sel);
                    selSelectionCopy((MaxAnySelection *)&mouseCursorSelect,
                                     (MaxAnySelection *)&tempSelection);
                    return FALSE; //we may not be done...
                }
                else
                {
                    // the selection being activated only contains support ships
                    mouseCursorSelect.numShips = 0;
                    tutGameMessage("Game_DoubleClickSpecial");
                    sel.numShips   = 1;
                    sel.ShipPtr[0] = (Ship *)mouseCursorObjPtr;
                    clWrapSpecial(&universe.mainCommandLayer, (SelectCommand *)&selSelected,(SpecialCommand *)&sel);
                    return TRUE;
                }
            }
            bitClear(mouseCursorMode, MCM_DoubleClick);
            bitClear(mouseCursorMode, MCM_SingleClick);
        }
        if (bitTest(mouseInfo.flags, MCF_SpecialOp))
        {
            MakeShipsSpecialActivateCapable((SelectCommand *)&mouseCursorSelect, TRUE);   // CAT: skip-disabled argument (UI callers pass 1, e.g. cat_kbLeechCB 0x0054eff0)

            if (mouseCursorSelect.numShips > 0)
            {
                selSelectionCopy((MaxAnySelection *)&selSelected,
                                 (MaxAnySelection *)&mouseCursorSelect);
                ioUpdateShipTotals();
                mouseCursorSelect.numShips = 0;
                tutGameMessage("Game_DoubleClickSpecial");
                clWrapSpecial(&universe.mainCommandLayer, (SelectCommand *)&selSelected,NULL);
                return TRUE;
            }
            bitClear(mouseCursorMode, MCM_DoubleClick);
            bitClear(mouseCursorMode, MCM_SingleClick);

        }
    }
//    else if ((mouseCursorObjPtr != NULL) &&
//             (mouseCursorObjPtr->header.objtype != OBJ_ShipType))
//    {
//        if (mouseCursorSelect.numShips > 0)
//        {
//            selSelectionCopy((MaxAnySelection *)&selSelected,
//                             (MaxAnySelection *)&mouseCursorSelect);
//            mouseCursorSelect.numShips = 0;
//            tutGameMessage("Game_DoubleClickHarvest");
//            clWrapCollectResource(&universe.mainCommandLayer, (SelectCommand *)&selSelected, (Resource*)mouseCursorObjPtr);
//        }
//        return TRUE;
//    }
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : mouseDoubleClickCheck
    Description : Check to see if the double click time has expired and performs
                  special tasks if it has
    Inputs      : void
    Outputs     : Very specific depending on the state of mouseInfo
    Return      : TRUE if processing was done, FALSE if not
----------------------------------------------------------------------------*/
bool32 mouseDoubleClickCheck(void)
{
    if (mouseDoubleClickExpire == 0.0)
    {
        return FALSE;
    }

    if (mouseDoubleClickExpire < universe.totaltimeelapsed)
    {
        if (bitTest(mouseCursorMode, MCM_DoubleClick))
        {
            bitClear(mouseCursorMode, MCM_DoubleClick);
            bitClear(mouseCursorMode, MCM_SingleClick);
            mouseDoubleClickExpire = 0.0;

            if (mouseCursorObjPtr != NULL)
            {
                if (mouseCursorSelect.numShips > 0)
                {
                    selSelectionCopy((MaxAnySelection *)&selSelected,
                                     (MaxAnySelection *)&mouseCursorSelect);
                    mouseCursorSelect.numShips = 0;
                    makeShipsDockCapable((SelectCommand *)&selSelected); //filter ships that can't dock
                    clWrapDock(&universe.mainCommandLayer, (SelectCommand *)&selSelected,
                               CAT_DOCK_SUPPORT, (Ship *)mouseCursorObjPtr);    // CAT: purpose 0 at a host, as mrObjectClick 0x00566b60 / cat_mrReleaseRight 0x0056b0b0 [audit: M2-R5 layout-dock]
                    mouseDrawType(small_docking);     //may need to have "docking" as well
                }
                return TRUE;
            }
        }
        mouseCursorSelect.numShips = 0;
        mouseDoubleClickExpire     = 0.0;
        bitClear(mouseCursorMode, MCM_SingleClick);
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : mouseLClick
    Description : Special processing a single click on the left mouse button
    Inputs      : void
    Outputs     : Very specific depending on the state of mouseInfo
    Return      : void
----------------------------------------------------------------------------*/
void mouseLClick(void)
{
    //if a menu is up, don't do any of this
    if (feStackIndex > 0)
    {
        return;
    }


    if (!bitTest(mouseCursorMode, MCM_SingleClick))
    {
        if ((mouseCursorType == docking) ||
            (mouseCursorType == small_docking))
        {
            selSelectionCopy((MaxAnySelection *)&mouseCursorSelect, (MaxAnySelection *)&selSelected);
            bitSet(mouseCursorMode, MCM_SingleClick);
            mouseDoubleClickExpire = universe.totaltimeelapsed + mouseDoubleClickTime;
        }
        else if ((mouseCursorType == special_op_activate) ||
                 (mouseCursorType == special_op_deactivate))
        {
            selSelectionCopy((MaxAnySelection *)&mouseCursorSelect, (MaxAnySelection *)&selSelected);
            mouseDoubleClickExpire = universe.totaltimeelapsed + mouseDoubleClickTime;
        }
        else if (mouseCursorType == support)
        {
            selSelectionCopy((MaxAnySelection *)&mouseCursorSelect, (MaxAnySelection *)&selSelected);
            mouseDoubleClickExpire = universe.totaltimeelapsed + mouseDoubleClickTime;
        }
    }
}


/*-----------------------------------------------------------------------------
    Name        : mouseClickShipDied
    Description : Takes care of ships dying at inopportune times by clearing the
                  ship from any file global selection lists or storage areas
    Inputs      : deadship - the ship that has just died
    Outputs     : removes the ship from mouseCursorSelect and the mouseCursorObjPtr
    Return      : void
----------------------------------------------------------------------------*/
void mouseClickShipDied(ShipPtr deadship)
{
    clRemoveShipFromSelection(&mouseCursorSelect, deadship);

    if ((mouseCursorObjPtr != NULL) && (mouseCursorObjPtr == (SpaceObj *)deadship))
    {
        mouseCursorObjPtr = NULL;
    }

    if (mouseCursorObjLast == (SpaceObj *)deadship)
    {
        mouseCursorObjLast = NULL;
    }
}



/*-----------------------------------------------------------------------------
    Name        : mouseDrawTriangle
    Description : Draws the mouse triangle on the screen
    Inputs      : void
    Outputs     : Draws the mouse triangle on the screen
    Return      : void
----------------------------------------------------------------------------*/
void mouseDrawTriangle(void)
{
    triangle mouseTri, mouseTri2;

    mouseTri.x0 = mouseCursorXPosition + 0;
    mouseTri.y0 = mouseCursorYPosition + 0;
    mouseTri.x1 = mouseCursorXPosition + 4;
    mouseTri.y1 = mouseCursorYPosition + 16;
    mouseTri.x2 = mouseCursorXPosition + 14;
    mouseTri.y2 = mouseCursorYPosition + 7;
    mouseTri2 = mouseTri;
    mouseTri2.x2++;
    primTriSolid2(&mouseTri2, mouseCursorContentColor);      //draw mouse cursor
    primTriOutline2(&mouseTri, 1, mouseCursorEdgeColor);
}


/*-----------------------------------------------------------------------------
    Name        : mouseInfoReset
    Description : Resets the mouseInfo structure
    Inputs      : void
    Outputs     : Resets the mouseInfo structure
    Return      : void
----------------------------------------------------------------------------*/
void mouseInfoReset(void)
{
    mouseInfo.ship_selected = 0;
    mouseInfo.resource      = 0;
    mouseInfo.flags         = 0;
    mouseInfo.cursorover    = MC_NO_SHIP;
}

lifheader* mouseGetLIF(void)
{
    // CAT: Cataclysm's cursor types (0x0056e9c0)
    if ((mouseCursorType < 0) || (mouseCursorType >= end_cursor_type) || (mouseCursorSlot[mouseCursorType] < 0))
    {
        return NULL;
    }
    return mouseBitmaps[(sdword)mouseCursorSlot[mouseCursorType]];
}

/*-----------------------------------------------------------------------------
    Name        : mouseDraw
    Description : Update the position of the mouse by polling the system and
                    draw it at it's new location.
    Inputs      : void
    Outputs     : updates mouse cursor position and button states to reflect
                    new mouse status.
    Return      : void
    Note        : because this routine performs drawing operations, it should
                    be called near the end rendering, before a buffer swap.
                  Checks to see if the mouse is inside or outside the client area
----------------------------------------------------------------------------*/
#define SX primScreenToGLX
#define SY primScreenToGLY
void mouseDraw(void)
{
    bool32 primModeOn = TRUE;
    lifheader *texture = NULL;
    GLuint thandle = 0;
    sdword slot;

    if (mouseWillBeVisible > 0)
    {
        mouseWillBeVisible--;
        if (!mouseWillBeVisible)
            mouseIsVisible = TRUE;
    }

    if ((!mouseIsVisible) || (mouseDisabled))
    {
        return;                                             //don't draw mouse if not visible
    }
    // CAT: no cursor while the main region is disabled behind closed NIS scissors in a game (mouseDraw 0x0056ec50)
    if (nisFullyScissored && mrDisabled && gameIsRunning)
    {
        return;
    }

    // CAT: the exe tests its GL-or-software flag (0x00b3a700) here; the SDL port always draws the textured cursor
    if (!mouseGLInitialized)
    {
        mouseInitGL();
        mouseGLInitialized = TRUE;
    }

    if (!primModeEnabled)
    {
        primModeOn = FALSE;
        primModeSetFunction2();
    }

    glEnable(GL_BLEND);
    rndAdditiveBlends(FALSE);
    rndTextureEnvironment(RTE_Modulate);
    cat_rndDepthTestEnable(FALSE);                          // CAT: the cursor draws without the depth test

    // CAT: the special-attack cursor held for a second of real time (HW1: game time, any held type)
    if ((mouseHoldType != normal_mouse) && (mouseCursorType == normal_mouse))
    {
        if (mouseHoldExpire < universe.realtimeelapsed)
        {
            mouseHoldType = normal_mouse;
        }
        else
        {
            mouseCursorType = mouseHoldType;
        }
    }

    //if a front end screen is on (but not the hyperspace roll call screen nor the sensors manager: CAT)
    //all mouse settings are overridden
    //if the movement pieplate is on, all mouse settings except for
    // the focus_no_select setting is overridden
    if (((feStackIndex > 0) && (!spHyperspaceRollCallScreen) && (!smSensorsActive)) ||
        ((bitTest(mouseCursorMode, MCM_Pointmode)) && (mouseCursorType != focus_no_select)))
    {
        mouseCursorType = normal_mouse;
    }
    if (cat_popupMenuIsOpen())
    {                                                       // CAT: the plain cursor over the popup menu
        mouseCursorType = normal_mouse;
    }

    if ((mouseCursorType == no_mouse) || (mouseCursorType == rotate_camera) || (mouseCursorType == zoom_camera))
    {
        goto finish;
    }
    if ((mouseCursorType >= 0) && (mouseCursorType < end_cursor_type) && (mouseCursorSlot[mouseCursorType] >= 0))
    {
        slot = mouseCursorSlot[mouseCursorType];
        texture = mouseBitmaps[slot];
        thandle = mouseTextures[slot];
        if ((mouseCursorType == special_attack) && (mouseHoldType == normal_mouse))
        {
            mouseHoldType = special_attack;
            mouseHoldExpire = universe.realtimeelapsed + (real32)MC_MOUSEHOLDTIME;
        }
    }
    if (texture != NULL)
    {
        bool32 texOn;

        trClearCurrent();
        texOn = rndTextureEnable(TRUE);
        glBindTexture(GL_TEXTURE_2D, thandle);

        glColor3ub(255,255,255);

        // CAT: one quad (GL_QUADS, the exe's corner order); the exe's own 0.325 pixel centre is not taken (decision 29)
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(SX(mouseCursorXPosition), SY(mouseCursorYPosition));
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(SX(mouseCursorXPosition), SY(mouseCursorYPosition + texture->height));
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(SX(mouseCursorXPosition + texture->width), SY(mouseCursorYPosition + texture->height));
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(SX(mouseCursorXPosition + texture->width), SY(mouseCursorYPosition));
        glEnd();

        rndTextureEnable(texOn);
    }
    else
    {
        mouseDrawTriangle();
    }

  finish:

    glDisable(GL_BLEND);

    if (!primModeOn)
    {
        primModeClearFunction2();
    }
    cat_rndDepthTestEnable(FALSE);      // CAT: the exe passes 0x00a75928, a word nothing writes: the test stays off
    mouseCursorOverDistance = REALlyBig;
    mouseCursorOverShipType = MC_NO_SHIP;

    if (bitTest(mouseCursorMode, MCM_CursorOverObj))
    {
        bitClear(mouseCursorMode, MCM_CursorOverObj);
    }
    else
    {
        mouseCursorObjPtr = NULL;
    }
    if (mouseCursorObjPtr != NULL)
    {                                                       //if the mouse is over an object
        mouseCursorObjLast = mouseCursorObjPtr;             //remember where and when it was
        mouseCursorLastObjTime = universe.realtimeelapsed;  // CAT: real time (0x008ba74c)
        mouseCursorLastX = mouseCursorX();
        mouseCursorLastY = mouseCursorY();
    }
}
#undef SX
#undef SY

char *tutMouseDrawMessage[] =
{   "Mouse_NormalMouse",
    "Mouse_RotateCamera",
    "Mouse_ZoomCamera",
    "Mouse_AddShips",
    "Mouse_FocusNoSelect",
    "Mouse_Movement",
    "Mouse_BandBoxAttack",
    "Mouse_ForceAttack",
    "Mouse_Guard",
    "Mouse_SpecialAttack",
    "Mouse_SpecialOpActivate",
    "Mouse_SpecialOpDeactivate",
    "Mouse_Gui",
    "Mouse_Resource",
    "Mouse_Derelict",
    "Mouse_Docking",
    "Mouse_SmallDocking",
    "Mouse_Research",
    "Mouse_Sensors",
    "Mouse_Build",
    "Mouse_Salvage",
    "Mouse_Support",
    "Mouse_Traders",
    "Mouse_EnemyShip",
    "Mouse_AlliedShip",
};



/*-----------------------------------------------------------------------------
    Name        : mouseDrawType
    Description : Sets the type of mouse cursor to be drawn
    Inputs      : cursorType - the type of cursor to draw
    Outputs     : Changes the global mouse cursor type variable
    Return      : void
----------------------------------------------------------------------------*/
void mouseDrawType(mouseCursor cursorType)
{
    //mouse messages only happen in the actual tutorial because
    //the overflow the message queue in the single player game
    if((tutorial==TUTORIAL_ONLY) && cursorType != mouseCursorType)
        tutGameMessage( tutMouseDrawMessage[(long)cursorType] );

    mouseCursorType = cursorType;
}


/*-----------------------------------------------------------------------------
    Name        : mouseCursorSet
    Description : sets the cursor mode for special case cursor settings
    Inputs      : mode - the mode to set cursormode to
    Outputs     : changes the cursormode flag
    Return      : void
----------------------------------------------------------------------------*/
void mouseCursorSet(udword mode)
{
    bitSet(mouseCursorMode, mode);
}

/*-----------------------------------------------------------------------------
    Name        : mouseCursorClear
    Description : Clears the cursor mode for special case cursor settings
    Inputs      : mode - the mode to clear
    Outputs     : changes the cursormode flag
    Return      : void
----------------------------------------------------------------------------*/
void mouseCursorClear(udword mode)
{
    bitClear(mouseCursorMode, mode);
}

/*-----------------------------------------------------------------------------
    Name        : mouseSelectCursorSetting
    Description : Gathers information about the selected ships, and
                  ships under the mouse cursor
                  I agree this function is ugly, but you have to consider
                  that any number of ships can be selected at one time,
                  and the mouse can be over any of those ships, or any
                  other ships.  This function has to take care of all of
                  that.  Poor function...  overworked, underpaid, ugly,
                  unappreciated,  just not having a good life.
                  Be kind to it...
                  CAT: not in Cataclysm (rndRenderTask 0x0057a910 calls mouseSetCursorSetting only, which
                  needs no mouseInfo); no caller since M2-R6 mainrgn-popup, kept for mouseLDoubleClick's mouseInfo
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mouseSelectCursorSetting(void)
{
    sdword i;

    mouseInfoReset();

    //if there are ships selected
    if (selSelected.numShips)
    {
        Ship *ship;

        mouseInfo.ship_selected = selSelected.numShips;

        //go through the selected list
        for (i = 0; i < selSelected.numShips; i++)
        {
            ship = selSelected.ShipPtr[i];

            //count the number of resource collectors selected
            if (ship->shiptype == ResourceCollector)
            {
                ++mouseInfo.resource;
                bitSet(mouseInfo.flags, MCF_Dockable);
            }

            if (ship->staticinfo->canSingleClickSpecialActivate)
            {
                bitSet(mouseInfo.flags, MCF_SingleClickSpecialActivate);
            }

            //-- Set flags for selected ships under the cursor --//
            //if the cursor is over a selected ship
            if ((mouseCursorObjPtr != NULL) &&
                (mouseCursorObjPtr == &ship->obj))
            {
                //set flags for all GUI or special op ships
                switch (ship->shiptype)
                {
                    case Carrier:
                    case Mothership:
                        bitSet(mouseInfo.flags, MCF_GUICapable);
                        bitSet(mouseInfo.flags, MCF_BuildGUI);
                        break;
                    // CAT: the ResearchShip / SensorArray / DDDFrigate cases are gone with the types (M2-R4 ships-b retire;
                    //      Cataclysm has no mouseSelectCursorSetting at all, see its header)
                    case CloakGenerator:
                    case CloakedFighter:
                        if (bitTest(ship->header.flags, SOF_Cloaked))
                        {
                            bitSet(mouseInfo.flags, MCF_SpecialOpDeployed);
                        }
                        bitSet(mouseInfo.flags, MCF_SpecialOp);
                        break;
                    case GravWellGenerator:
                        if (((GravWellGeneratorSpec *)ship->ShipSpecifics)->GravFieldOn == TRUE)
                        {
                            bitSet(mouseInfo.flags, MCF_SpecialOpDeployed);
                        }
                        bitSet(mouseInfo.flags, MCF_SpecialOp);
                        break;
                    case MinelayerCorvette:
                        bitSet(mouseInfo.flags, MCF_SpecialOp);
                        break;
                    case AdvanceSupportFrigate:
                        //we don't want the dock cursor to show up when
                        //the mouse is over a selected repair corvette
                        bitSet(mouseInfo.flags, MCF_SupportOverItself);
                        break;
                    default:
                        break;
                }
            }

            // -- Set flags for ships that are selected but not under the cursor -- //
            //set flags for dockable ships
            if ((ship->staticinfo->shipclass == CLASS_Corvette) ||
                (ship->staticinfo->shipclass == CLASS_Fighter))
            {
                // CAT: every strike craft is small-dockable, no RepairCorvette case (the type is gone; M2-R4 ships-b retire)
                bitSet(mouseInfo.flags, MCF_SmallDockable);
            }
            else if ((ship->staticinfo->shipclass == CLASS_Resource) ||
                     (ship->staticinfo->shipclass == CLASS_NonCombat))
            {
                bitSet(mouseInfo.flags, MCF_NonCombat);
            }
            //set flags for special attack/support ships
            switch (ship->shiptype)
            {
                case MissileDestroyer:
                case HeavyCorvette:
                    bitSet(mouseInfo.flags, MCF_SpecialAttack);
                    break;
                case Carrier:
                    bitSet(mouseInfo.flags, MCF_SpecialSCSupport);
                    break;
                case AdvanceSupportFrigate:
                    bitSet(mouseInfo.flags, MCF_SpecialSupport);
                    break;
                case SalCapCorvette:
                    bitSet(mouseInfo.flags, MCF_Salvage);

                    //if this is the first ship in the list
                    if (i==0)
                    {
                        bitSet(mouseInfo.flags, MCF_SalvageOnly);
                    }
                    break;
                default:
                    break;
            }

            //the special salvage cursor bitmap is displayed
            //instead of the attack bitmap if selselected only contains
            //salvage capture corvettes... checking for this here
            if (i>0 && bitTest(mouseInfo.flags, MCF_SalvageOnly))
            {
                if (ship->shiptype != SalCapCorvette)
                {
                    bitClear(mouseInfo.flags, MCF_SalvageOnly);
                }
            }

        }

        //if there isn't anything under the cursor, then the rest of this
        //function isn't needed
        if (mouseCursorOverShipType == MC_NO_SHIP)
        {
            return;
        }

        //set cursorover variable depending on the ship under cursor and
        //the value of certain flags.
        switch (mouseCursorOverShipType)
        {
            case Carrier:
            case Mothership:
                if (bitTest(mouseInfo.flags, MCF_Dockable) ||
                    bitTest(mouseInfo.flags, MCF_SmallDockable))
                {
                    mouseInfo.cursorover = docking;
                }
                else
                {
                    mouseInfo.cursorover = build;
                }
                break;
                mouseInfo.cursorover = research;
                break;
                mouseInfo.cursorover = sensors;
                break;
                mouseInfo.cursorover = docking;
                break;
            case CloakGenerator:
            case CloakedFighter:
            case GravWellGenerator:
            case MinelayerCorvette:
                mouseInfo.cursorover = special_op_activate;
                break;
            //drone is not considered as a ship in this case
                mouseInfo.cursorover = MC_NO_SHIP;
                break;
            case ResourceCollector:
            case AdvanceSupportFrigate:
            case JunkYardHQ:
                mouseInfo.cursorover = small_docking;
                break;
                if ((spGetCurrentMission() != MISSION_1_KHARAK_SYSTEM) &&
                    (bitTest(mouseInfo.flags, MCF_SalvageOnly)))
                {
                    mouseInfo.cursorover = salvage;
                }
                break;
            case FloatingCity:
                mouseInfo.cursorover = traders;
                break;
            case MC_DERELICT:
                if ((mouseCursorObjPtr) && (((Derelict *)mouseCursorObjPtr)->derelicttype == HyperspaceGate))
                    mouseInfo.cursorover = enemy_ship;
                else
                    mouseInfo.cursorover = derelict;

                break;
            case MC_RESOURCE:
                mouseInfo.cursorover = resource;
                if (mouseCursorObjPtr && (mouseCursorObjPtr->attributes & (ATTRIBUTES_KillerCollDamage|ATTRIBUTES_HeadShotKillerCollDamage)))
                {
                    if (mouseInfo.resource == 0)
                    {
                        mouseInfo.cursorover = enemy_ship;
                    }
                    else
                    {
                        //resouce collectors can't resource killer asteroids
                        mouseInfo.cursorover = MC_NO_SHIP;
                    }
                }
                break;
            case Ghostship:
            case MC_ENEMY_SHIP:
                mouseInfo.cursorover = enemy_ship;
                break;
            case MC_ALLIED_SHIP:
                mouseInfo.cursorover = allied_ship;
                break;
            default:
                if (mouseCursorObjPtr &&
                    ((ShipPtr)mouseCursorObjPtr)->playerowner == selSelected.ShipPtr[0]->playerowner)
                {
                    mouseInfo.cursorover = own_ship;
                }
        }

        //if a support ship is selected, and the mouse is over
        //a ship the support ship is capable of supporting, change
        //the mouse cursor
        if (mouseCursorObjPtr &&
            (bitTest(mouseInfo.flags, MCF_SpecialSCSupport) &&
             (isShipOfClass(mouseCursorObjPtr, CLASS_Fighter) ||
              isShipOfClass(mouseCursorObjPtr, CLASS_Corvette))))
        {
            mouseInfo.cursorover = support;
        }
    }
    //if there are ships being selected
    else if (selSelecting.numTargets)
    {
        bitSet(mouseInfo.flags, MCF_ShipSelecting);
    }
    else
    {
        if ((mouseCursorOverShipType == FloatingCity) ||
            ((mouseCursorOverShipType == MC_ALLIED_SHIP) &&
             (mouseCursorEnemyType == FloatingCity)))
        {
            mouseInfo.cursorover = traders;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : mouseKeyCursorSetting
    Description : Sets the mouse cursor depending on key states and how the
                  mouseInfo structure is set.
    Inputs      : voidorama (uses globals)
    Outputs     : Calls mouseDrawType to change the mouse Draw type
    Return      : nothing
----------------------------------------------------------------------------*/
void mouseSetCursorSetting(void)
{
    // CAT: Cataclysm's (0x0056f3b0): no mouseInfo pass (mouseSelectCursorSetting is gone) and no double-click
    //      check; the band-box modifiers' cursors, then, with a context-sensitive option, the cursor of what a
    //      right click on the object under the mouse would do (cat_mouseGetContextAction, catMouseContextActions)
    CatTargetActions selection, target, scratch;
    MaxSelection one;
    sdword action;

    if (!gameIsRunning)
    {
        return;
    }
    mouseCursorType = normal_mouse;
    if (mrDisabled)
    {
        return;
    }
    selection = *cat_selComputeTargetActions(&scratch, &selSelected);
    memset(&target, 0, sizeof(target));
    if (mouseCursorObjPtr != NULL)
    {
        one.numShips = 1;
        one.ShipPtr[0] = (Ship *)mouseCursorObjPtr;
        target = *cat_selComputeTargetActions(&scratch, &one);
    }
    if (cat_moClickedFlagConsume())
    {                                                       // the main overlay's objective was clicked
        mouseCursorType = objectives;
        return;
    }
    // CAT: the band-box key commands held (kbCommandKeyIsHit through catMrKbHeld; M2-R6 startup-keys: the loaded bindings)
    if (catMrKbHeld(CAT_KB_BandboxForceAttack))
    {
        if (!playPackets)
        {
            mouseCursorType = force_attack;
        }
        return;
    }
    if (catMrKbHeld(CAT_KB_MultipleSelect))
    {
        mouseCursorType = add_ships;
        return;
    }
    if (catMrKbHeld(CAT_KB_BandboxGuard))
    {
        if (!playPackets)
        {
            mouseCursorType = m_guard;
        }
        return;
    }
    if (catMrKbHeld(CAT_KB_ShipSpecial))
    {
        if (!playPackets)
        {
            mouseCursorType = special_attack;
        }
        return;
    }
    if (catMrKbHeld(CAT_KB_BandboxFocus))
    {
        mouseCursorType = focus_no_select;
        return;
    }
    if (catMrKbHeld(CAT_KB_BandboxAttack))
    {
        if (!playPackets)
        {
            mouseCursorType = band_box_attack;
        }
        return;
    }
    if (((tutorial == TUTORIAL_ONLY) || (catMrHomeworldCSA == TRUE) || (catMrRightClickCSA == TRUE)) &&
        (mouseCursorObjPtr != NULL) && (catMrSelecting != TRUE) && !smSensorsActive &&
        !ShipInSelection((SelectCommand *)&selSelected, (Ship *)mouseCursorObjPtr))
    {
        action = cat_mouseGetContextAction(&selection, &target, &selSelected, (Ship *)mouseCursorObjPtr);
        if ((action > CAT_CA_None) && (action < CAT_CA_Number))
        {
            mouseCursorType = (mouseCursor)catMouseContextActions[action].cursor;
        }
    }
}


/*-----------------------------------------------------------------------------
   Cursor Text Code
----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    Name        : mouseCursorTextDraw
    Description : Draws the name of the ship type associated with mouseCursorOverShipType
    Inputs      : void
    Outputs     : Screen text roughly resembling the name of the ship type
    Return      :
----------------------------------------------------------------------------*/
void mouseCursorTextDraw(void)
{
    static sdword cursortexttype = MC_NO_SHIP;
    fonthandle fhSave;

    if ((!mrRenderMainScreen) || (!gameIsRunning) || (mrWhiteOut) || (nisIsRunning) || (keyIsHit(RMOUSE_BUTTON)))
    {
        return;
    }

    // if there's something in that cool global variable
    if ((mouseCursorOverShipType != MC_NO_SHIP) && (mouseCursorOverShipType != MC_RESOURCE) && (mouseCursorOverShipType != MC_DERELICT))
    {
        cursortexttype = mouseCursorOverShipType;
    }
    else
    {
        if (mouseCursorTextExpire < universe.totaltimeelapsed)
        {
            cursortexttype = MC_NO_SHIP;
        }
    }

    if (cursortexttype != MC_NO_SHIP)
    {
        // display that baby!
        fhSave = fontMakeCurrent(mouseCursorFont);

        if (cursortexttype == MC_ENEMY_SHIP)
        {
            if (mouseCursorOverLODs & MOUSE_IdentifyLODMask)
            {                                               //if it's been seen closely
                if (!singlePlayerGame)
                {
                    fontPrintf(TW_CURSORTEXT_X, MAIN_WindowHeight - TW_CURSORTEXT_Y, TW_CURSORTEXT_COLOR, "%s's %s",
                              playerNames[mouseCursorEnemyPlayer], ShipTypeToNiceStr(mouseCursorEnemyType));
                }
                else
                {
                    fontPrintf(TW_CURSORTEXT_X, MAIN_WindowHeight - TW_CURSORTEXT_Y, TW_CURSORTEXT_COLOR,
                               ShipTypeToNiceStr(mouseCursorEnemyType));
                }
            }
            else
            {
                fontPrint(TW_CURSORTEXT_X, MAIN_WindowHeight - TW_CURSORTEXT_Y, TW_CURSORTEXT_COLOR,
                          strGetString(strEnemyShip));
            }
        }
        else if (cursortexttype == MC_ALLIED_SHIP)
        {
            if (!singlePlayerGame)
            {
                fontPrintf(TW_CURSORTEXT_X, MAIN_WindowHeight - TW_CURSORTEXT_Y, TW_CURSORTEXT_COLOR, "%s's %s",
                          playerNames[mouseCursorEnemyPlayer], ShipTypeToNiceStr(mouseCursorEnemyType));
            }
            else
            {
                // allied ships in the single player game (defector and traders) only
                // display their shiptypes and not the player name
                fontPrint(TW_CURSORTEXT_X, MAIN_WindowHeight - TW_CURSORTEXT_Y, TW_CURSORTEXT_COLOR,
                           ShipTypeToNiceStr(mouseCursorEnemyType));
            }
        }
        else
        {
            fontPrint(TW_CURSORTEXT_X, MAIN_WindowHeight - TW_CURSORTEXT_Y, TW_CURSORTEXT_COLOR,
                       ShipTypeToNiceStr(cursortexttype));
        }


        fontMakeCurrent(fhSave);
    }
}


/*-----------------------------------------------------------------------------
    Name        : mouseClipPointToRect
    Description : Clip an x/y coordinate to a rectangle.
    Inputs      : x, y - position to clip
                  rect - rectangle to clip to
    Outputs     : clips x, y to rect
    Return      :
----------------------------------------------------------------------------*/
void mouseClipPointToRect(sdword *x, sdword *y, rectangle *rect)
{
    if (*x < rect->x0)
    {
        *x = rect->x0;
    }
    if (*x >= rect->x1)
    {
        *x = rect->x1 - 1;
    }
    if (*y < rect->y0)
    {
        *y = rect->y0;
    }
    if (*y >= rect->y1)
    {
        *y = rect->y1 - 1;
    }
}

/*=============================================================================
    Name    : mousePoll
    Purpose : Poll current mouse location and store internally

    Created 7/7/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/
void mousePoll(void)
{
    //rectangle clientRect;
    //POINT mousePoint;
    sdword tmpX, tmpY;

    if (mouseDisabled)
    {
        return;
    }
    if (demDemoPlaying)
    {
        return;
    }
    //utyClientRectGet(&clientRect);                          //get window location
    /*
    GetCursorPos(&mousePoint);                              //get location of Windows mouse cursor
    mouseCursorXPosition = mousePoint.x - clientRect.x0;    //compute relative coords
    mouseCursorYPosition = mousePoint.y - clientRect.y0;
    */

    if (mouseClip)
    {
        SDL_GetRelativeMouseState(&tmpX, &tmpY);
        // HWI-FEATURE(ui-scale): SDL's motion (window coordinates) in logical units, the fraction carried over
        uiScaleRelativeToLogical(&tmpX, &tmpY);     // HWI-FEATURE(ui-scale)
        mouseCursorXPosition += tmpX;               // HWI-FEATURE(ui-scale)
        mouseCursorYPosition += tmpY;               // HWI-FEATURE(ui-scale)
        mouseClipPointToRect(&mouseCursorXPosition, &mouseCursorYPosition, &mouseClipRect);
        mousePositionSet(mouseCursorXPosition, mouseCursorYPosition);
    }
    else
    {
        if (!uiScaleTestMouseGet(&mouseCursorXPosition, &mouseCursorYPosition))  // HWI-TEST: UiScaleTest.c's position
        {
            SDL_GetMouseState(&mouseCursorXPosition, &mouseCursorYPosition);
        }

        // scale the mouse position to the UI resolution in case render resolution is different
        // HWI-FEATURE(ui-scale): window coordinates -> logical (the window's last row -> the logical last row), then
        //      clamped to the logical screen (SDL reports positions outside the window while a button is held)
        uiScaleMouseToLogical(&mouseCursorXPosition, &mouseCursorYPosition);    // HWI-FEATURE(ui-scale)
        mouseCursorXPosition = (mouseCursorXPosition < 0) ? 0 :                 // HWI-FEATURE(ui-scale)
            ((mouseCursorXPosition >= MAIN_WindowWidth) ? MAIN_WindowWidth - 1 : mouseCursorXPosition);    // HWI-FEATURE(ui-scale)
        mouseCursorYPosition = (mouseCursorYPosition < 0) ? 0 :                 // HWI-FEATURE(ui-scale)
            ((mouseCursorYPosition >= MAIN_WindowHeight) ? MAIN_WindowHeight - 1 : mouseCursorYPosition);  // HWI-FEATURE(ui-scale)
    }
    //perform client area enter/exit logic to hide/show Windows system cursor
    /*
    if (primPointInRectXY2(&clientRect, mousePoint.x, mousePoint.y))
    {                                                       //mouse inside window's client area
        if (!mouseInsideClient)
        {                                                   //just entered region
            //ShowCursor(FALSE);
            SDL_ShowCursor(SDL_DISABLE);
            mouseInsideClient = TRUE;
        }
    }
    else
    {                                                       //else outside region
        if (mouseInsideClient)
        {                                                   //just exited region
            //ShowCursor(TRUE);
            SDL_ShowCursor(SDL_ENABLE);
            mouseInsideClient = FALSE;
        }
    }
    */
}

/*-----------------------------------------------------------------------------
    Name        : mouseDisable/mouseEnable
    Description : Disable and re-enable the mouse
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void mouseDisable(void)
{
    mouseCursorHide();
    mouseDisabled = TRUE;
}
void mouseEnable(void)
{
    mouseCursorShow();
    mouseDisabled = FALSE;
}



/*=============================================================================
    CAT (M2-R6 mainrgn-popup): the context actions
=============================================================================*/
// the exe's two jump tables over cat_mouseGetContextAction's result: mouseSetCursorSetting's (0x0056f70c, actions
//      1..17) gives the cursor, cat_mrReleaseRight's (0x0056b650, 0..17) the right-click order
const CatContextActionInfo catMouseContextActions[CAT_CA_Number] =
{
    { normal_mouse,    CAT_CAO_Popup,    -1 },      // 0 none: the popup menu
    { resource,        CAT_CAO_Harvest,  -1 },      // 1 harvest
    { salvage,         CAT_CAO_Special,   0 },      // 2 salvage
    { band_box_attack, CAT_CAO_Attack,   -1 },      // 3 attack
    { support,         CAT_CAO_Special,   1 },      // 4 repair
    { force_field,     CAT_CAO_Protect,  -1 },      // 5 the force-field sphere
    { slipgate_small,  CAT_CAO_Slipgate, -1 },      // 6
    { slipgate_medium, CAT_CAO_Slipgate, -1 },      // 7
    { slipgate_large,  CAT_CAO_Slipgate, -1 },      // 8
    { small_docking,   CAT_CAO_Dock,     -1 },      // 9 dock
    { leech,           CAT_CAO_Special,   2 },      // 10 leech
    { link_cursor,     CAT_CAO_Link,     -1 },      // 11 link
    { mimic,           CAT_CAO_Mimic,    -1 },      // 12 mimic
    { emp,             CAT_CAO_Special,   4 },      // 13 EMP
    { swarm,           CAT_CAO_Special,   3 },      // 14 swarm
    { ramming,         CAT_CAO_Special,   6 },      // 15 ram
    { fire_missile,    CAT_CAO_Special,   7 },      // 16 the linked missiles
    { beast_weapon,    CAT_CAO_Special,   5 },      // 17 infect
};

/*-----------------------------------------------------------------------------
    Name        : cat_mouseGetContextAction                         0x0056f8f0
    Description : What a click on 'ship' does for the selection 'ships' (their CatTargetActions 'selection' and the
                  object's 'target'): a resource to harvest; an enemy to leech, EMP, swarm, ram, fire missiles at,
                  infect (when every selected ship can; with the Homeworld CSA option those are a plain attack) or
                  attack; an ally to repair or to protect in a force-field sphere; a slipgate; a salvage target; an
                  own ship to dock with or link with; a ship to mimic (one the disguise list has) or an asteroid.
    Return      : CAT_CA_* (CAT_CA_None for no context action)
----------------------------------------------------------------------------*/
sdword cat_mouseGetContextAction(CatTargetActions *selection, CatTargetActions *target, MaxSelection *ships, Ship *ship)
{
    udword *s = selection->flag, *t = target->flag;
    sdword action = CAT_CA_None, index, count;
    bool32 all;
    ubyte player;
    CatMimicItem *item;

    if (playPackets)
    {
        return CAT_CA_None;
    }
    if (t[CAT_TA_Resource] == 0)
    {
        if (t[CAT_TA_Enemy] == 0)
        {
            if (t[CAT_TA_Ally] == 0)
            {                                               // a slipgate (the selection's own ships), or salvage
                if (t[CAT_TA_SlipgateSmall] != 0)
                {
                    if (s[CAT_TA_Ally] != 0)
                    {
                        return CAT_CA_SlipgateSmall;
                    }
                }
                else if (t[CAT_TA_SlipgateMedium] != 0)
                {
                    if (s[CAT_TA_Ally] != 0)
                    {
                        return CAT_CA_SlipgateMedium;
                    }
                }
                else if (t[CAT_TA_SlipgateLarge] != 0)
                {
                    if (s[CAT_TA_Ally] != 0)
                    {
                        return CAT_CA_SlipgateLarge;
                    }
                }
                else if ((t[CAT_TA_Salvageable] != 0) && (s[CAT_TA_Salvage] != 0))
                {
                    return CAT_CA_Salvage;
                }
            }
            else
            {                                               // an ally: repair (by non-combat ships), protect
                if ((s[CAT_TA_Repair] != 0) && (s[CAT_TA_CanAttack] == 0))
                {
                    return CAT_CA_Repair;
                }
                if ((s[CAT_TA_AllForceField] != 0) && (t[CAT_TA_ForceField] == 0))
                {
                    return CAT_CA_ProtectFormation;
                }
            }
        }
        else
        {                                                   // an enemy: what every selected ship can do
            if (s[CAT_TA_AllLeech] != 0)
            {
                return CAT_CA_Leech;
            }
            if (s[CAT_TA_AllEMP] != 0)
            {
                return catMrHomeworldCSA ? CAT_CA_Attack : CAT_CA_EMP;
            }
            if (s[CAT_TA_AllHive] != 0)
            {
                return catMrHomeworldCSA ? CAT_CA_Attack : CAT_CA_Swarm;
            }
            if (s[CAT_TA_AllRam] != 0)
            {
                return catMrHomeworldCSA ? CAT_CA_Attack : CAT_CA_Ram;
            }
            if (s[CAT_TA_AllAcolyteLinkFire] != 0)
            {
                return catMrHomeworldCSA ? CAT_CA_Attack : CAT_CA_FireMissiles;
            }
            if ((s[CAT_TA_AllBeastWeapon] != 0) && (cat_beastGetAttackStaticInfo(ship) != NULL))
            {
                return catMrHomeworldCSA ? CAT_CA_Attack : CAT_CA_Infect;
            }
        }
    }
    else if ((s[CAT_TA_Harvest] != 0) && (s[CAT_TA_CanAttack] == 0))
    {
        return CAT_CA_Harvest;
    }
    if ((t[CAT_TA_Enemy] != 0) && (s[CAT_TA_CanAttack] != 0))
    {
        return CAT_CA_Attack;
    }
    if ((t[CAT_TA_Salvageable] != 0) && (s[CAT_TA_CanAttack] == 0) && (s[CAT_TA_Salvage] != 0))
    {
        return CAT_CA_Salvage;
    }
    if ((ship->header.objtype == OBJ_ShipType) && (ship->playerowner == universe.curPlayerPtr) && (ships->numShips > 0))
    {                                                       // an own ship: dock with it, link with it
        if (!bitTest(ship->header.flags, SOF_Dead) && !ship->shipOrDerelict.catDisabled && !cat_shipIsUntargetable(ship))
        {
            all = TRUE;
            for (index = 0; index < ships->numShips; index++)
            {
                if (!ThisShipCanDockWith(ships->ShipPtr[index], ship, 0, FALSE))
                {
                    all = FALSE;
                }
            }
            if (all)
            {
                action = CAT_CA_Dock;
            }
        }
        if (cat_shipHasUpgrade(ship, CAT_UPGRADE_Link))
        {
            all = TRUE;
            for (index = 0; index < ships->numShips; index++)
            {
                if (!cat_shipHasUpgrade(ships->ShipPtr[index], CAT_UPGRADE_Link) ||
                    (ships->ShipPtr[index]->shiptype != ship->shiptype))
                {
                    all = FALSE;
                }
            }
            if (all)
            {
                return CAT_CA_Link;
            }
        }
        if (action != CAT_CA_None)
        {
            return action;
        }
    }
    if (s[CAT_TA_AllMimic] != 0)
    {                                                       // a ship the disguise list has, or an asteroid
        if (ship->header.objtype == OBJ_ShipType)
        {
            player = (ubyte)ship->playerowner->playerIndex;
            // CAT: the exe passes the target's sMCV word (CatTargetActions +0x5C) as the list's "added" flag
            count = cat_mimicItemCount(player, t[CAT_TA_MCV]);
            all = FALSE;
            for (index = 0; index < count; index++)
            {
                item = cat_mimicItemGet(player, t[CAT_TA_MCV], index);
                if (item->type == (udword)ship->shiptype)
                {
                    all = TRUE;
                }
            }
            if (all)
            {
                return CAT_CA_Mimic;
            }
        }
        else if (ship->header.objtype == OBJ_AsteroidType)
        {
            action = CAT_CA_Mimic;
        }
    }
    return action;
}

// cat_mouseSaveCursorUnder 0x0056ebb0 / cat_mouseRestoreCursorUnder 0x0056eb40: while the front end asks for it
//      (feShouldSaveMouseCursor) the exe saves the pixels under the cursor before drawing it and puts them back after
//      the page flip (through glcompat's glcCursorUnder, or the software renderer's hooks). GoK's SDL build redraws
//      every frame and has no glcompat ("Decisions" 28), so the front end never asks and there is nothing to save
void cat_mouseSaveCursorUnder(void)
{
}

void cat_mouseRestoreCursorUnder(void)
{
}
