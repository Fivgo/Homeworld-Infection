/*=============================================================================
    Name    : mainrgn.c
    Purpose : Logic for handling region events for the main game screen.

    Created 6/27/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "mainrgn.h"
#include "FOW.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "AIVar.h"
#include "Alliance.h"
#include "Battle.h"
//#include "bink.h"
#include "CameraCommand.h"
#include "Captaincy.h"
#include "Chatting.h"
#include "CommandDefs.h"
#include "CommandNetwork.h"
#include "CommandWrap.h"
#include "ConsMgr.h"
#include "Debug.h"
#include "ETG.h"
#include "FastMath.h"
#include "FlightMan.h"
#include "font.h"
#include "FontReg.h"
#include "GameChat.h"
#include "GamePick.h"
#include "glinc.h"
#include "Globals.h"
#include "Gun.h"
#include "InfoOverlay.h"
#include "KAS.h"
#include "KASFunc.h"
#include "KeyBindings.h"
#include "LaunchMgr.h"
#include "main.h"
#include "Memory.h"
#include "MeshAnim.h"
#include "mouse.h"
#include "MotionTest.h"     // HWI-FEATURE(motion-interp): the measurement frame sample
#include "MotionInterp.h"   // HWI-FEATURE(motion-interp): miRenderCamera
#include "NetCheck.h"
#include "NIS.h"
#include "avi.h"            // CAT: aviIsPlaying (the Bink player's binkDonePlaying, mrRegionProcess)
#include "Objectives.h"
#include "Options.h"
#include "Particle.h"
#include "Mimic.h"
#include "ModuleShip.h"
#include "Mothership.h"
#include "HiveFrigate.h"
#include "ACV.h"
#include "Salvage.h"
#include "Cloak.h"
#include "Leech.h"
#include "Vector.h"
#include "PiePlate.h"
#include "PopupMenu.h"
#include "prim2d.h"
#include "prim3d.h"
#include "render.h"
#include "ResearchGUI.h"
#include "screenshot.h"
#include "Select.h"
#include "ShipPrint.h"
#include "Sensors.h"
#include "Shader.h"
#include "ShipView.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "soundlow.h"
#include "StringSupport.h"
#include "Switches.h"
#include "Tactical.h"
#include "Tactics.h"
#include "TaskBar.h"
#include "Teams.h"
#include "TradeMgr.h"
#include "Tutor.h"
#include "Tweak.h"
#include "Undo.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "utility.h"

#ifdef _WIN32
    #include <windows.h>
#endif

#if defined _MSC_VER
	#define isnan(x) _isnan(x)
#endif

#undef UTY_SCREEN_SHOT

/*=============================================================================
    Data:
=============================================================================*/

//global flag controlling rendering of main game screen
sdword mrRenderMainScreen = TRUE;

//camera for main screen
Camera *mrCamera = NULL;

bool32 mrWhiteOut = FALSE;
real32 mrWhiteOutT = 0.0f;

sdword mrOldMouseX, mrOldMouseY;

//logic handlers for main region
void mrNULL(void);
// CAT (M2-R7 r6-leftovers): no mrHoldLeft / mrHoldRight (HW1's hold handlers): the exe keeps the mouse state in its
//      state words catMrSelecting 0x00a75284 and catMrRightMode 0x00a75288, which every tester reads (decision 36)

//flag indicating mouse has moved since right mouse button pressed
sdword mrMouseHasMoved;
sdword mrMouseMovementClickLimit = MR_MouseMovementClickLimit;

//global handle to the main region
regionhandle ghMainRegion = NULL;

//tactical overlay on/off flag
bool32 mrDrawTactical = FALSE, mrSaveTactical = FALSE;

//help info screen up or not
bool32 helpinfoactive = FALSE;

regionhandle helpinforegion;

//debug for turning off enemy AI
bool32 mrNoAI = FALSE;

//fonthandle for Big Font
static fonthandle mrBigFont = 0;

//key definitions
struct
{
    keyindex key[REG_NumberKeys];
    ubyte nKeys;
    udword filter;
}
mrKeyFunction[] =
{
    {{ESCKEY,    0,      0,      0}, 1, RPE_KeyDown},
    {{TABKEY,    0,      0,      0}, 1, RPE_KeyDown},
    {{ARRLEFT,   0,      0,      0}, 1, RPE_KeyDown},
    {{ARRRIGHT,  0,      0,      0}, 1, RPE_KeyDown},
    {{BKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{AKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{CKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{DKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{EKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{FKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{RKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{MMOUSE_BUTTON,0,   0,      0}, 1, RPE_KeyDown},
    {{MMOUSE_DOUBLE,0,   0,      0}, 1, RPE_KeyDown},
    {{HKEY2,      0,      0,      0}, 1, RPE_KeyDown},
    {{IKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{JKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{KKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{LKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{MKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{LBRACK,    0,      0,      0}, 1, RPE_KeyDown},
    {{RBRACK,    0,      0,      0}, 1, RPE_KeyDown},
    {{SKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{VKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{ZKEY,      0,      0,      0}, 1, RPE_KeyDown | RPE_KeyUp},
    {{CAPSLOCKKEY,0,     0,      0}, 1, RPE_KeyDown},
    {{F1KEY,     0,      0,      0}, 1, RPE_KeyDown},
    {{F2KEY,     0,      0,      0}, 1, RPE_KeyDown},
    {{F3KEY,     0,      0,      0}, 1, RPE_KeyDown},
    {{F4KEY,     0,      0,      0}, 1, RPE_KeyDown},
    {{TILDEKEY,  0,      0,      0}, 1, RPE_KeyDown},
    {{ZEROKEY,   0,      0,      0}, 1, RPE_KeyDown},
    {{ONEKEY,    0,      0,      0}, 1, RPE_KeyDown},
    {{TWOKEY,    0,      0,      0}, 1, RPE_KeyDown},
    {{THREEKEY,  0,      0,      0}, 1, RPE_KeyDown},
    {{FOURKEY,   0,      0,      0}, 1, RPE_KeyDown},
    {{FIVEKEY,   0,      0,      0}, 1, RPE_KeyDown},
    {{XKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{SIXKEY,    0,      0,      0}, 1, RPE_KeyDown},
    {{SEVENKEY,  0,      0,      0}, 1, RPE_KeyDown},
    {{EIGHTKEY,  0,      0,      0}, 1, RPE_KeyDown},
    {{NINEKEY,   0,      0,      0}, 1, RPE_KeyDown},
    {{SPACEKEY,  0,      0,      0}, 1, RPE_KeyDown},
    {{ENTERKEY,  0,      0,      0}, 1, RPE_KeyDown},
    {{F5KEY,     0,      0,      0}, 1, RPE_KeyDown},
    {{F6KEY,     0,      0,      0}, 1, RPE_KeyDown},
    {{F7KEY,     0,      0,      0}, 1, RPE_KeyDown},
    {{F8KEY,     0,      0,      0}, 1, RPE_KeyDown},
    {{F9KEY,     0,      0,      0}, 1, RPE_KeyDown},
    {{F10KEY,    0,      0,      0}, 1, RPE_KeyDown},
    {{F11KEY,    0,      0,      0}, 1, RPE_KeyDown},
    {{HOMEKEY,   0,      0,      0}, 1, RPE_KeyDown},
    {{PAGEDOWNKEY,0,     0,      0}, 1, RPE_KeyDown},
    {{PAGEUPKEY, 0,      0,      0}, 1, RPE_KeyDown},
    {{BACKSLASHKEY,      0,      0,      0}, 1, RPE_KeyDown},
    {{PKEY,              0,      0,      0}, 1, RPE_KeyDown},
#if MR_TEST_GUNS
    {{TKEY,      0,      0,      0}, 1, RPE_KeyDown},
#else
    {{TKEY,      0,      0,      0}, 1, RPE_KeyDown},
#endif

#if GUN_TUNE_MODE
    {{YKEY,      0,      0,      0}, 1, RPE_KeyDown},
#endif

    {{SHIFTKEY,  0,      0,      0}, 1, RPE_KeyDown | RPE_KeyUp},
#if ETG_RELOAD_KEY
    {{ETG_RELOAD_KEY,0,  0,      0}, 1, RPE_KeyDown | RPE_KeyUp},
#endif
#if NIS_TEST
    {{NUMPAD1,   0,      0,      0}, 1, RPE_KeyDown | RPE_KeyUp},
#endif
#if MR_TEST_HPB
    {{OKEY,   0,      0,      0}, 1, RPE_KeyDown},
#endif
    //{{PLUSMINUS,    0,      0,      0},     1,  RPE_KeyDown            },
    {{NUMPLUSKEY,   0,      0,      0},     1,  RPE_KeyDown | RPE_KeyUp},
    {{PLUSKEY,      0,      0,      0},     1,  RPE_KeyDown | RPE_KeyUp},
    {{NUMMINUSKEY,  0,      0,      0},     1,  RPE_KeyDown | RPE_KeyUp},
    {{MINUSKEY,     0,      0,      0},     1,  RPE_KeyDown | RPE_KeyUp},

// Keys for music selection in multiplayer game
    {{LESSTHAN    , 0,      0,      0}, 1, RPE_KeyDown},
    {{GREATERTHAN , 0,      0,      0}, 1, RPE_KeyDown},

// Extra keys that are mappable that the mainregion must capture
#ifdef HW_BUILD_FOR_DISTRIBUTION  //only add these for release builds so that random debug keys will work
    {{PAUSEKEY    , 0,      0,      0}, 1, RPE_KeyDown},
    {{PRINTKEY    , 0,      0,      0}, 1, RPE_KeyDown},
    {{ARRUP       , 0,      0,      0}, 1, RPE_KeyDown},
    {{ARRDOWN     , 0,      0,      0}, 1, RPE_KeyDown},
    {{ENDKEY      , 0,      0,      0}, 1, RPE_KeyDown},
    {{INSERTKEY   , 0,      0,      0}, 1, RPE_KeyDown},
    {{DELETEKEY   , 0,      0,      0}, 1, RPE_KeyDown},
    {{NKEY        , 0,      0,      0}, 1, RPE_KeyDown},
    {{OKEY        , 0,      0,      0}, 1, RPE_KeyDown},
    {{QKEY        , 0,      0,      0}, 1, RPE_KeyDown},
    {{UKEY        , 0,      0,      0}, 1, RPE_KeyDown},
    {{WKEY        , 0,      0,      0}, 1, RPE_KeyDown},
    {{XKEY        , 0,      0,      0}, 1, RPE_KeyDown},
    {{YKEY        , 0,      0,      0}, 1, RPE_KeyDown},
    {{NUMPAD0     , 0,      0,      0}, 1, RPE_KeyDown},
    {{NUMPAD1     , 0,      0,      0}, 1, RPE_KeyDown},
    {{NUMPAD2     , 0,      0,      0}, 1, RPE_KeyDown},
    {{NUMPAD3     , 0,      0,      0}, 1, RPE_KeyDown},
    {{NUMPAD4     , 0,      0,      0}, 1, RPE_KeyDown},
    {{NUMPAD5     , 0,      0,      0}, 1, RPE_KeyDown},
    {{NUMPAD6     , 0,      0,      0}, 1, RPE_KeyDown},
    {{NUMPAD7     , 0,      0,      0}, 1, RPE_KeyDown},
    {{NUMPAD8     , 0,      0,      0}, 1, RPE_KeyDown},
    {{NUMPAD9     , 0,      0,      0}, 1, RPE_KeyDown},
    {{NUMSTARKEY  , 0,      0,      0}, 1, RPE_KeyDown},
    {{NUMSLASHKEY , 0,      0,      0}, 1, RPE_KeyDown},
    {{NUMDOTKEY   , 0,      0,      0}, 1, RPE_KeyDown},
#else
    {{UKEY        , 0,      0,      0}, 1, RPE_KeyDown},
    {{YKEY        , 0,      0,      0}, 1, RPE_KeyDown},
    {{QKEY        , 0,      0,      0}, 1, RPE_KeyDown},
#endif

    {{0,         0,      0,      0}, 0, RPE_KeyDown}
};

//stuff for changing formations
// CAT: HW1's formation selection state and font (mrNewFormation, mrFormationName, mrDrawFormation, mrFormationFont) are
//      gone: Cataclysm cycles the formations with cat_mrFormationCycle 0x00566750

#if MAD_TEST_ANIMATION
sdword mrTestAnimationIndex = 0;                //index of mesh animation we're testing
#endif

// for kas text display
extern sdword popupTextNumLines;

// number indicates which player was right clicked on in the player list
sdword playerClickedOn=-1;

//for handling bot z-bandbox and just pressing z
bool32 mrSpecialBandBox;

//for double-press number keys to focus on ships
real32 mrNumberDoublePressTime = MR_NumberDoublePressTime;
sdword mrLastKeyPressed = -1;
real32 mrLastKeyTime = 0;

//to help clicking on fast-moving ships:
real32 mrFastMoveShipClickTime = MR_FastMoveShipClickTime;
sdword mrFastMoveShipClickX = MR_FastMoveShipClickX;
sdword mrFastMoveShipClickY = MR_FastMoveShipClickY;

#if MR_CAN_FOCUS_ROIDS
bool32 mrCanFocusRoids = FALSE;
#endif

bool32 mrMenuDontDisappear = FALSE;       //forces the context menus to stay on the screen

#define TIMEOUT_TIME 15.0 // timer for the printing of player dropped messages

#ifdef HW_BUILD_FOR_DEBUGGING
extern real32 pilotupoffset;
#endif

/*=============================================================================
    Private Function prototypes:
=============================================================================*/

//sdword mrClickedOnPlayer(void);

/*=============================================================================
    Functions:
=============================================================================*/

static udword originalMrFilter;
bool32   mrDisabled = FALSE;

void mrDisable(void)
{
    if (!mrDisabled)
    {
        dbgAssertOrIgnore(ghMainRegion);
        originalMrFilter = ghMainRegion->flags;
    //    regFilterSet(ghMainRegion, 0);
        regFilterSet(ghMainRegion, RPM_MouseEvents);
        mrDisabled = TRUE;
    }
}

void mrEnable(void)
{
    if (mrDisabled)
    {
        dbgAssertOrIgnore(ghMainRegion);
        regFilterSet(ghMainRegion,originalMrFilter);
        mrDisabled = FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : mrNeedProbeHack()
    Description : checks if a single probe is selcted and if so returns true,
                  otherwise removes it from the selection.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 mrNeedProbeHack(void)
{
    // CAT: no HW1 Probe, so never (M2-R4 ships-b)
    return FALSE;
}
/*-----------------------------------------------------------------------------
    Name        : mrRemoveAllProbesFromSelection
    Description : removes any probes from the current selection
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mrRemoveAllProbesFromSelection()
{
    // CAT: no HW1 Probe to remove (M2-R4 ships-b)
}

/*-----------------------------------------------------------------------------
    Name        : salCapCorvetteInSelection
    Description : returns TRUE if a salcap corvette is in the selection
                  otherwise returns false
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 salCapCorvetteInSelection(SelectCommand *selection)
{
    sdword i;

    for(i=0;i<selection->numShips;i++)
    {
        // CAT (M2-R4 combat): the Cataclysm salvager mark (SPECIAL_CatSalvager, set by cat_salvageInit 0x004f9010);
        //      HW1's SPECIAL_IsASalvager is retired. The rest of this right-click code is stage 3's.
        if(selection->ShipPtr[i]->shipOrDerelict.specialFlags & SPECIAL_CatSalvager)
        {
            return TRUE;
        }
    }
    return FALSE;
}
/*-----------------------------------------------------------------------------
    Name        : mrRemoveDerelictsAndAllPlayerShipsFromSelecting
    Description : removes any of player's own ships from the current selection
                  also removes any drones from selection too
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mrRemoveDerelictsAndAllPlayerShipsFromSelecting(void)
{
    sdword i;
    SpaceObjRotImpTarg *target;
    Ship *ship;

    // CAT: mrRemoveDerelictsAndAllPlayerShipsFromSelecting 0x00563aa0: a derelict goes unless it is a hyperspace gate
    //      or a salvager is selected (cat_shipIsSalvager); an own or allied ship goes unless it is of the derelict
    //      race (RACE_Derelict ships are kept), and a ship flagged 0x10 (catFlags18) goes always
    for (i = 0; i < selSelecting.numTargets; )
    {
        target = selSelecting.TargetPtr[i];
        ship = (Ship *)target;
        if (((target->header.objtype == OBJ_DerelictType) && (((Derelict *)target)->derelicttype != HyperspaceGate) &&
             !DoAnyShipsFollowConstraints((SelectCommand *)&selSelected, cat_shipIsSalvager)) ||
            ((target->header.objtype == OBJ_ShipType) &&
             ((ship->staticinfo->shiprace != RACE_Derelict) || bitTest(ship->header.catFlags18, 0x10)) &&
             ((ship->playerowner == universe.curPlayerPtr) || allianceIsShipAlly(ship, universe.curPlayerPtr) ||
              bitTest(ship->header.catFlags18, 0x10))))
        {
            selSelecting.numTargets--;
            selSelecting.TargetPtr[i] = selSelecting.TargetPtr[selSelecting.numTargets];
            continue;
        }
        i++;
    }
}

/*-----------------------------------------------------------------------------
    Name        :  mrProbeHack
    Description :  Called when a single probe was selected and the Movement mech
                    was being started...
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mrProbeHack()
{
    if(!smSensorsActive)
    {   //start sensors manager if it isn't active already
        smSensorsBegin(NULL, NULL);     //step one complete!
    }
    if(!smSensorsDisable && !smZoomingIn)
    {   //if sensors aren't disabled AND we aren't zooming in
        piePointModeOnOff(PIE_SPEC_Move, NULL);
    }
}

/*-----------------------------------------------------------------------------
    Name        : mrStartup
    Description : Start main region processor logic
    Inputs      : void
    Outputs     : Many many things...
    Return      : void
----------------------------------------------------------------------------*/
void mrStartup(void)
{
    // CAT: mrStartup 0x00563b50: the main region with HW1's mouse events (0x2836f6c), a key child for each of the
    //      256 key ids, key down and up (the key bindings decide what a key does; cat_mrKeyChildrenAlloc's loop), and
    //      the big font. HW1's key table (mrKeyFunction) and the formation font are gone
    ghMainRegion = regChildAlloc(NULL, 0, 0, 0,             //create main region
                   MAIN_WindowWidth, MAIN_WindowHeight, 0,
                   RPE_LeftClickButton | RPE_HoldLeft |
                   RPE_RightClickButton | RPE_HoldRight |
                   RPE_WheelUp | RPE_WheelDown | RPE_DoubleLeft | RPE_DoubleCentre);
    regDrawFunctionSet(ghMainRegion, mrRegionDraw);         //set it's functions
    regFunctionSet(ghMainRegion, (regionfunction) mrRegionProcess);
    cat_mrKeyChildrenAlloc(ghMainRegion, (regionfunction) mrRegionProcess);

    mrBigFont = frFontRegister("Arial_b17.hff");
}

/*-----------------------------------------------------------------------------
    Name        : mrShutdown
    Description : Shuts down main region
    Inputs      : void
    Outputs     : Destroys ghMainRegion and all child windows
    Return      : void
----------------------------------------------------------------------------*/
void mrShutdown(void)
{
    regRegionDelete(ghMainRegion);
}

/*=============================================================================
    CAT (M2-R6 mainrgn-popup): Cataclysm's main region. The mouse handlers of mrRegionProcess 0x00567930
    (cat_mrPressLeft .. cat_mrReleaseLeftCB 0x0056af10 .. 0x0056b6a0), the band box (cat_mrBandBoxRelease
    0x0056a610, cat_mrSelectRectDraw 0x0056ac00, mrSelectHold 0x0056a1e0), the special commands the popup menu and the
    key commands run (cat_ui* 0x00563e20 .. 0x00566510) and the orders the exe reaches through its key-command table
    (move, dock, waypoints, upgrade, the slipgate, hyperspace; functions the export does not list, named here by
    their key commands). Names of the variables are chosen here from their uses.
=============================================================================*/
bool32 catMrSelecting = FALSE;                  // 0x00a75284: the left button is down for a band box
sdword catMrRightMode = 0;                      // 0x00a75288: 2 while the right button turns the camera
sdword catMrWaypointMode = 0;                   // 0x00a752b4: 0 once, 1 a loop, 2 end to end (cat_uiSetWaypointMode)
vector catMrWaypoints[CAT_MR_MAX_WAYPOINTS];    // 0x00b39bc0
sdword catMrWaypointCount = 0;                  // 0x00b39cb0
bool32 catMrHomeworldCSA = FALSE;               // 0x00a7526c: the options' OP_HomeworldCSA (cat_opHomeworldCSA 0x004d9f30)
bool32 catMrRightClickCSA = TRUE;               // 0x008b9ec8: the options' OP_RightClickCSA (default 1)
static sdword catMrFormationCycleType = 0;      // 0x00a75264
static real32 catMrFormationCycleTime = 0.0f;   // 0x00a75268
static bool32 catMrFormationCyclePending = FALSE;   // 0x00a752a4: mrRegionDraw sets it after the delay
static sdword catMrTacticsCycleType = 0;        // 0x00b39cb4
static real32 catMrTacticsCycleTime = 0.0f;     // 0x00a7525c
static bool32 catMrTacticsCyclePending = FALSE; // 0x00a752ac
bool32 catMrWaypointCentre = FALSE;             // 0x00a75270: an option (Options.c 0x004d9fde, M2-R6 front end)
#define CAT_MR_CYCLE_DELAY          1.0f        // 0x0072a4c4: a cycled formation / tactic is set 1 s after the key

// CAT: the exe's tutorial gates test bits of its own tutEnable word 0x00ba2e58 (and 0x00ba2e5c); the Cataclysm
//      tutorial flags are stage 6's, so outside the tutorial everything is enabled and inside it the word is read raw.
//      CAT-TODO(stage 6 tutorial): the exe's tutGameEnableFlags layout
static bool32 catMrTutEnabled(udword bit)
{
    return (tutorial != TUTORIAL_ONLY) || ((*(udword *)&tutEnable & bit) != 0);
}

// CAT: the exe asks its key bindings whether a command's key is held (kbCommandKeyIsHit 0x0054ec60 with Cataclysm's
//      command numbers; M2-R6 startup-keys: the loaded bindings, KeyBindings.c)
bool32 catMrKbHeld(sdword command)
{
    return kbCommandKeyIsHit(command);
}

// the exe's guard of every order: no recording playing, not paused, the main region enabled
#define CAT_MR_ORDERS_ALLOWED()     (!playPackets && !universePause && !mrDisabled)

/*-----------------------------------------------------------------------------
    Name        : cat_mrKeyChildrenAlloc                            0x00563be0
    Description : Key children for the 256 key ids (down and up) of a region, as mrStartup does inline.
----------------------------------------------------------------------------*/
void cat_mrKeyChildrenAlloc(regionhandle region, regionfunction function)
{
    sdword ID;

    for (ID = 0; ID < 0x100; ID++)
    {
        regKeyChildAlloc(region, ID, RPE_KeyDown | RPE_KeyUp, function, 1, ID, 0, 0, 0);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_uiShipDamageFraction                          0x005c9c60
    Description : The health fraction of a ship's most damaged part (itself or a module).
----------------------------------------------------------------------------*/
real32 cat_uiShipDamageFraction(Ship *ship)
{
    Ship *module, *worst = ship;
    real32 fraction, worstFraction;

    worstFraction = ship->targetable.health / ship->staticinfo->health.maxhealth;
    for (module = cat_moduleIterFirst(ship); module != NULL; module = cat_moduleIterNext())
    {
        fraction = module->targetable.health / module->staticinfo->health.maxhealth;
        if (fraction < worstFraction)
        {
            worst = module;
            worstFraction = fraction;
        }
    }
    return worst->targetable.health / worst->staticinfo->health.maxhealth;
}

/*-----------------------------------------------------------------------------
    Name        : cat_uiFindNearestSpecialTarget                    0x00563e20
    Description : The nearest ship for a special order without a target: kind 0 a salvage target, 1 an ally to repair
                  (within 1e6 of the nearest the most damaged), 2 an enemy, 3 an enemy the EMP can affect; never the
                  ship itself, a dead or hidden ship, or a point the fog of war hides.
----------------------------------------------------------------------------*/
Ship *cat_uiFindNearestSpecialTarget(Ship *ship, sdword kind)
{
    Ship *other, *nearest = NULL;
    Node *node;
    real32 distance, nearestDistance = 1e21f;
    bool32 take;

    // CAT: the exe returns NULL when the player has no fog-of-war grid (Player+0x6500; cat_fowCreatePlayerGrids gives
    //      the local player one at levelInit) [M2-R6 nis-fow-camera]
    if (universe.curPlayerPtr->catFOWGrid == NULL)
    {
        return NULL;
    }
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        other = (Ship *)listGetStructOfNode(node);
        if ((other == ship) || bitTest(other->header.flags, SOF_Dead | SOF_Hide))
        {
            continue;
        }
        take = FALSE;
        switch (kind)
        {
            case 0:
                take = cat_salvageTargetAllowed(other) && !cat_selPositionHiddenByFOW(&other->obj.posinfo.position, FALSE);
                break;
            case 1:
                take = allianceIsShipAlly(other, universe.curPlayerPtr);
                break;
            case 2:
                take = !allianceIsShipAlly(other, universe.curPlayerPtr) && !cat_shipIsCrystal(other) &&
                       !cat_shipIsUntargetable(other) && !cat_selPositionHiddenByFOW(&other->obj.posinfo.position, FALSE);
                // CAT: the exe falls through into kind 3's test (no break); the result is kind 2's or kind 3's
                take = take || (!allianceIsShipAlly(other, universe.curPlayerPtr) && !cat_shipIsCrystal(other) &&
                                !cat_shipIsUntargetable(other) && cat_empCanAffectShip(other, NULL) &&
                                !cat_selPositionHiddenByFOW(&other->obj.posinfo.position, FALSE));
                break;
            case 3:
                take = !allianceIsShipAlly(other, universe.curPlayerPtr) && !cat_shipIsCrystal(other) &&
                       !cat_shipIsUntargetable(other) && cat_empCanAffectShip(other, NULL) &&
                       !cat_selPositionHiddenByFOW(&other->obj.posinfo.position, FALSE);
                break;
            default:
                break;
        }
        if (!take)
        {
            continue;
        }
        distance = (other->obj.posinfo.position.x - ship->obj.posinfo.position.x) * (other->obj.posinfo.position.x - ship->obj.posinfo.position.x) +
                   (other->obj.posinfo.position.y - ship->obj.posinfo.position.y) * (other->obj.posinfo.position.y - ship->obj.posinfo.position.y) +
                   (other->obj.posinfo.position.z - ship->obj.posinfo.position.z) * (other->obj.posinfo.position.z - ship->obj.posinfo.position.z);
        if (distance < nearestDistance)
        {
            nearest = other;
            nearestDistance = distance;
        }
        else if ((kind == 1) && (distance - nearestDistance < 1000000.0f) &&
                 (cat_uiShipDamageFraction(other) < cat_uiShipDamageFraction(nearest)))
        {                                                   // a nearly as near ally that is more damaged
            nearest = other;
            nearestDistance = distance;
        }
    }
    return nearest;
}

/*-----------------------------------------------------------------------------
    Name        : cat_uiSpecialTargetCommand                        0x00564060
    Description : A special order of the (not busy) selection: 0 salvage, 1 repair, 2 leech, 3 swarm, 4 EMP,
                  5 infect, 6 ram, 7 fire the linked missiles; on the given targets, else each ship on its nearest one.
----------------------------------------------------------------------------*/
void cat_uiSpecialTargetCommand(SelectCommand *targets, sdword ability)
{
    MaxSelection selection;
    SelectCommand single;
    SelectCommand oneTarget;
    sword reasons[9];
    bool32 speak = TRUE, said, saidCrystal = FALSE, saidInfection = FALSE, saidSupport = FALSE, saidTech = FALSE;
    sdword index, kind;
    Ship *ship, *target;

    cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
    cat_selMakeShipsNotBusy((SelectCommand *)&selection);
    switch (ability)
    {
        case 0:                                             // salvage: drop the targets it may not take, saying why
            if (targets != NULL)
            {
                cat_selMakeShipsHaveUpgrade((SelectCommand *)&selection, CAT_UPGRADE_Salvage);
                index = 0;
                while (index < targets->numShips)
                {
                    memset(reasons, 0, sizeof(reasons));
                    if (cat_salvageCheckTargetReasons(universe.curPlayerPtr, targets->ShipPtr[index], reasons))
                    {
                        index++;
                        continue;
                    }
                    if (reasons[CAT_SALVAGE_REASON_NoSupport] > 0)
                    {
                        if (!saidTech)
                        {
                            clCommandMessage(strGetString(strSalvageTargetHasIncompatibleTech), 0xff0000ff);
                            saidTech = TRUE;
                        }
                    }
                    else if ((reasons[CAT_SALVAGE_REASON_NoSupportType] > 0) &&
                             (reasons[CAT_SALVAGE_REASON_NoSupportType] != reasons[CAT_SALVAGE_REASON_Friendly]))
                    {
                        speak = FALSE;
                        speechEventQueue(NULL, 0x200103a, -1, -1, 0, -1, -1, 5.0f, -1);
                        if (!saidSupport)
                        {
                            clCommandMessage(strGetString(strNotEnoughSupportUnitsToSalvage), 0xff0000ff);
                            saidSupport = TRUE;
                        }
                    }
                    else if ((reasons[CAT_SALVAGE_REASON_NoBeastUpgrade] > 0) &&
                             (reasons[CAT_SALVAGE_REASON_NoBeastUpgrade] != reasons[CAT_SALVAGE_REASON_Friendly]))
                    {
                        speak = FALSE;
                        if (!saidInfection)
                        {
                            clCommandMessage(strGetString(strDepotNeedsInfectionTech), 0xff0000ff);
                            saidInfection = TRUE;
                        }
                    }
                    else if (reasons[CAT_SALVAGE_REASON_NoCrystalUpgrade] > 0)
                    {
                        speak = FALSE;
                        if (!saidCrystal)
                        {
                            clCommandMessage(strGetString(strWorkerNeedsCrystalTech), 0xff0000ff);
                            saidCrystal = TRUE;
                        }
                    }
                    targets->numShips--;
                    targets->ShipPtr[index] = targets->ShipPtr[targets->numShips];
                }
            }
            break;
        case 1:
            cat_selMakeShipsRepairCapable((SelectCommand *)&selection);
            break;
        case 2:
            cat_selMakeShipsLeechCapable((SelectCommand *)&selection, TRUE);
            break;
        case 3:
            cat_selMakeShipsHaveUpgrade((SelectCommand *)&selection, 0x800);
            break;
        case 4:
            cat_selMakeShipsHaveUpgrade((SelectCommand *)&selection, CAT_UPGRADE_ACVEMP);
            if (selection.numShips < 1)
            {
                return;
            }
            cat_selMakeShipsNotLinkedACVs((SelectCommand *)&selection);
            if (selection.numShips < 1)
            {
                clCommandMessage(strGetString(strCantEMPTargets), 0xff0000ff);
                return;
            }
            break;
        case 5:
            cat_selMakeShipsHaveUpgrade((SelectCommand *)&selection, 0x10000);
            break;
        case 6:
            cat_selMakeShipsRammingFrigates((SelectCommand *)&selection, TRUE);
            break;
        case 7:
            cat_selMakeShipsAcolyteLinkFire((SelectCommand *)&selection);
            break;
        default:
            break;
    }
    if (selection.numShips <= 0)
    {
        return;
    }
    if (targets != NULL)
    {
        switch (ability)
        {
            case 0:
                cat_clWrapSpecialTarget(&universe.mainCommandLayer, (SelectCommand *)&selection, (SpecialCommand *)targets, TRUE);
                if (speak)
                {
                    cat_clSpecialTargetSpeech((SelectCommand *)&selection);
                }
                break;
            case 1:
                cat_clWrapSpecialTarget(&universe.mainCommandLayer, (SelectCommand *)&selection, (SpecialCommand *)targets, FALSE);
                break;
            default:
                clWrapSpecial(&universe.mainCommandLayer, (SelectCommand *)&selection, (SpecialCommand *)targets);
                break;
        }
        return;
    }
    said = FALSE;
    for (index = 0; index < selection.numShips; index++)
    {                                                       // each ship on its own nearest target
        ship = selection.ShipPtr[index];
        switch (ability)
        {
            case 0:  kind = 0; break;
            case 1:  kind = 1; break;
            case 4:  kind = 3; break;
            default: kind = 2; break;
        }
        target = cat_uiFindNearestSpecialTarget(ship, kind);
        if (target == NULL)
        {
            if (!said)
            {
                switch (ability)
                {
                    case 0: cat_ioMessageAdd(strGetString(strNoShipToSalvageFound), 0xff0000ff, 1, ship); break;
                    case 1: cat_ioMessageAdd(strGetString(strNoShipToRepairFound), 0xff0000ff, 1, ship); break;
                    case 2: cat_ioMessageAdd(strGetString(strNoShipToLeechFound), 0xff0000ff, 1, ship); break;
                    case 3: cat_ioMessageAdd(strGetString(strNoShipToSwarmFound), 0xff0000ff, 1, ship); break;
                    case 4: cat_ioMessageAdd(strGetString(strNoShipToEMPFound), 0xff0000ff, 1, ship); break;
                    case 5: cat_ioMessageAdd(strGetString(strNoShipToInfectFound), 0xff0000ff, 1, ship); break;
                    case 6: cat_ioMessageAdd(strGetString(strNoShipToRamFound), 0xff0000ff, 1, ship); break;
                    case 7: cat_ioMessageAdd(strGetString(strNoShipToFireMissileAtFound), 0xff0000ff, 1, ship); break;
                    default: break;
                }
                said = TRUE;
            }
            continue;
        }
        single.numShips = 1;
        single.ShipPtr[0] = ship;
        oneTarget.numShips = 1;
        oneTarget.ShipPtr[0] = target;
        switch (ability)
        {
            case 0:
                cat_clWrapSpecialTarget(&universe.mainCommandLayer, &single, (SpecialCommand *)&oneTarget, TRUE);
                if (speak)
                {
                    cat_clSpecialTargetSpeech(&single);
                }
                break;
            case 1:
                cat_clWrapSpecialTarget(&universe.mainCommandLayer, &single, (SpecialCommand *)&oneTarget, FALSE);
                break;
            default:
                clWrapSpecial(&universe.mainCommandLayer, &single, (SpecialCommand *)&oneTarget);
                break;
        }
    }
}

// the special orders on targets (NULL: the nearest ones), each with the exe's guard (0x00564030 .. 0x00564750)
void cat_uiSpecialSalvage(SelectCommand *targets)           // 0x00564030
{
    if (CAT_MR_ORDERS_ALLOWED())
    {
        cat_uiSpecialTargetCommand(targets, 0);
    }
}

void cat_uiSpecialRepair(SelectCommand *targets)            // 0x00564630
{
    if (CAT_MR_ORDERS_ALLOWED())
    {
        cat_uiSpecialTargetCommand(targets, 1);
    }
}

void cat_uiSpecialLeech(SelectCommand *targets)             // 0x00564660
{
    if (CAT_MR_ORDERS_ALLOWED())
    {
        cat_uiSpecialTargetCommand(targets, 2);
    }
}

void cat_uiSpecialEMP(SelectCommand *targets)               // 0x00564690
{
    if (CAT_MR_ORDERS_ALLOWED())
    {
        cat_uiSpecialTargetCommand(targets, 4);
    }
}

void cat_uiSpecialSwarm(SelectCommand *targets)             // 0x005646c0
{
    if (CAT_MR_ORDERS_ALLOWED())
    {
        cat_uiSpecialTargetCommand(targets, 3);
    }
}

void cat_uiSpecialInfect(SelectCommand *targets)            // 0x005646f0
{
    if (CAT_MR_ORDERS_ALLOWED())
    {
        cat_uiSpecialTargetCommand(targets, 5);
    }
}

void cat_uiSpecialRam(SelectCommand *targets)               // 0x00564720
{
    if (CAT_MR_ORDERS_ALLOWED())
    {
        cat_uiSpecialTargetCommand(targets, 6);
    }
}

void cat_uiSpecialFireMissile(SelectCommand *targets)       // 0x00564750
{
    if (CAT_MR_ORDERS_ALLOWED())
    {
        cat_uiSpecialTargetCommand(targets, 7);
    }
}

// a special activation of each selected ship that passes 'test', one ship per order (0x00564780 .. 0x00564920)
static void catUiActivateEach(bool32 (*test)(Ship *ship))
{
    SelectCommand single;
    sdword index;

    if (!CAT_MR_ORDERS_ALLOWED())
    {
        return;
    }
    for (index = 0; index < selSelected.numShips; index++)
    {
        if (test(selSelected.ShipPtr[index]))
        {
            single.numShips = 1;
            single.ShipPtr[0] = selSelected.ShipPtr[index];
            clWrapSpecial(&universe.mainCommandLayer, &single, NULL);
        }
    }
}

static bool32 catUiHiveReady(Ship *ship)
{
    return cat_shipHasUpgrade(ship, 0x800) && cat_hiveFrigateSwarmReady(ship);
}

static bool32 catUiCanCloak(Ship *ship)
{
    return cat_shipHasUpgrade(ship, 0x20000) && !cat_shipIsCloakedFully(ship);
}

static bool32 catUiCanDecloak(Ship *ship)
{
    return cat_shipHasUpgrade(ship, 0x20000) && cat_shipIsCloakedFully(ship);
}

void cat_uiReleaseHiveSwarm(void)                           // 0x00564780: the ready hives release their swarms
{
    catUiActivateEach(catUiHiveReady);
}

void cat_uiSiegeCannonCancel(void)                          // 0x00564810: the motherships stop the big gun
{
    catUiActivateEach(cat_mothershipBigGunActive);
}

void cat_uiCloakShips(void)                                 // 0x00564890
{
    catUiActivateEach(catUiCanCloak);
}

void cat_uiDecloakShips(void)                               // 0x00564920
{
    catUiActivateEach(catUiCanDecloak);
}

void cat_uiSetWaypointMode(sdword mode)                     // 0x00565100
{
    switch (mode)
    {
        case 0:
            catMrWaypointMode = 0;
            clCommandMessage(strGetString(strWaypointModeNormal), 0xffffffff);
            break;
        case 1:
            catMrWaypointMode = 1;
            clCommandMessage(strGetString(strWaypointModePatrolCircular), 0xffffffff);
            break;
        case 2:
            catMrWaypointMode = 2;
            clCommandMessage(strGetString(strWaypointModePatrolEndToEnd), 0xffffffff);
            break;
        default:
            break;
    }
}

void cat_uiHaltSelection(void)                              // 0x00565190: halt, and cancel the upgrades
{
    if (!CAT_MR_ORDERS_ALLOWED())
    {
        return;
    }
    if (!catMrTutEnabled(0x8000))
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    cat_selMakeShipsHaltable((SelectCommand *)&selSelected, TRUE);
    if (selSelected.numShips > 0)
    {
        clWrapHalt(&universe.mainCommandLayer, (SelectCommand *)&selSelected);
        cat_clWrapUpgradeShips(&universe.mainCommandLayer, (SelectCommand *)&selSelected, TRUE);
    }
}

void cat_uiLink(void)                                       // 0x00565570
{
    MaxSelection selection;

    if (CAT_MR_ORDERS_ALLOWED())
    {
        cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
        cat_selMakeShipsHaveUpgrade((SelectCommand *)&selection, CAT_UPGRADE_Link);
        cat_selMakeShipsNotBusy((SelectCommand *)&selection);
        if (selection.numShips > 0)
        {
            cat_clWrapLink(&universe.mainCommandLayer, (SelectCommand *)&selection);
        }
    }
}

void cat_uiUnlink(void)                                     // 0x005655e0
{
    MaxSelection selection;

    if (CAT_MR_ORDERS_ALLOWED())
    {
        cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
        cat_selMakeShipsHaveUpgrade((SelectCommand *)&selection, CAT_UPGRADE_MimicSmall);
        cat_selMakeShipsNotBusy((SelectCommand *)&selection);
        if (selection.numShips > 0)
        {
            cat_clWrapUnlink(&universe.mainCommandLayer, (SelectCommand *)&selection);
        }
    }
}

void cat_uiUnmimic(void)                                    // 0x00565770: the mimicking ships drop their disguises
{
    MaxSelection notMimicking, selection;
    sdword index;
    Ship *ship;

    if (!CAT_MR_ORDERS_ALLOWED())
    {
        return;
    }
    notMimicking.numShips = 0;
    for (index = 0; index < selSelected.numShips; index++)
    {
        ship = selSelected.ShipPtr[index];
        if (!cat_shipHasUpgrade(ship, CAT_UPGRADE_Mimic) || !cat_shipIsMimicking(ship))
        {
            cat_selSelectionAddShip(&notMimicking, ship);
        }
    }
    cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
    MakeShipsNotIncludeTheseShips((SelectCommand *)&selection, (SelectCommand *)&notMimicking);
    cat_selMakeShipsNotBusy((SelectCommand *)&selection);
    if (selection.numShips > 0)
    {
        cat_clWrapMimic(&universe.mainCommandLayer, (SelectCommand *)&selection, 0, 0xffffffff, 0xffffffff, 0xffffffff);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_mrSetTactics                                  0x00565c30
    Description : The (not busy) selection's tactics, with the speech and the message (evasive only for small ships).
----------------------------------------------------------------------------*/
void cat_mrSetTactics(sdword tactic, bool32 showMessage)
{
    MaxSelection selection;

    if (!CAT_MR_ORDERS_ALLOWED())
    {
        return;
    }
    cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
    cat_selMakeShipsNotBusy((SelectCommand *)&selection);
    if (tactic == Evasive)
    {
        cat_selMakeShipsNotBigShips((SelectCommand *)&selection);
    }
    if (selection.numShips <= 0)
    {
        return;
    }
    if (tactic == Evasive)
    {
        speechEventQueue(selSelected.ShipPtr[0], 0x400080d, selSelected.ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);
        tutGameMessage("KB_TacticsEvasive");
    }
    else if (tactic == Neutral)
    {
        speechEventQueue(selSelected.ShipPtr[0], 0x400080e, selSelected.ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);
        tutGameMessage("KB_TacticsNeutral");
    }
    else if (tactic == Aggressive)
    {
        speechEventQueue(selSelected.ShipPtr[0], 0x400080f, selSelected.ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);
        tutGameMessage("KB_TacticsAggressive");
    }
    clWrapSetTactics(&universe.mainCommandLayer, (SelectCommand *)&selection, tactic);
    if (showMessage && (tactic >= Evasive) && (tactic <= Aggressive))
    {                                                       // CAT-FIX: the exe reads an unset string index otherwise
        clCommandMessage(strGetString(str_GM_TacticsSetToEvasive + tactic), 0xffffffff);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_mrFormationCycle / cat_mrTacticsCycle          0x00566750 / 0x00566860
    Description : The next / previous formation (delta .. sphere) or tactic; the message now, the order 1 s later
                  (mrRegionDraw sets it once the keys stop).
----------------------------------------------------------------------------*/
void cat_mrFormationCycle(bool32 backwards)
{
    char *message = NULL;

    if (selSelected.numShips < 2)
    {
        return;
    }
    if (catMrFormationCycleTime < universe.realtimeelapsed)
    {
        catMrFormationCycleType = clSelectionAlreadyInFormation(&universe.mainCommandLayer, (SelectCommand *)&selSelected);
    }
    if ((catMrFormationCycleType == NO_FORMATION) || (catMrFormationCycleType == SAME_FORMATION))
    {
        catMrFormationCycleType = DELTA_FORMATION;
    }
    else if (!backwards)
    {
        catMrFormationCycleType++;
        if (catMrFormationCycleType > SPHERE_FORMATION)
        {
            catMrFormationCycleType = DELTA_FORMATION;
        }
    }
    else if (catMrFormationCycleType == DELTA_FORMATION)
    {
        catMrFormationCycleType = SPHERE_FORMATION;
    }
    else
    {
        catMrFormationCycleType--;
    }
    catMrFormationCycleTime = universe.realtimeelapsed + CAT_MR_CYCLE_DELAY;
    catMrFormationCyclePending = TRUE;
    if ((catMrFormationCycleType >= DELTA_FORMATION) && (catMrFormationCycleType <= CUSTOM_FORMATION))
    {
        message = strGetString(str_GM_DeltaFormation + catMrFormationCycleType);
        clCommandMessage(message, 0xffffffff);
    }
    soundEventPlay(NULL, 0x108, NULL);
}

void cat_mrTacticsCycle(bool32 backwards)
{
    if (selSelected.numShips <= 0)
    {
        return;
    }
    if (catMrTacticsCycleTime < universe.realtimeelapsed)
    {
        catMrTacticsCycleType = selSelected.ShipPtr[0]->tacticstype;
    }
    if (!backwards)
    {
        catMrTacticsCycleType++;
        if (catMrTacticsCycleType > 2)
        {
            catMrTacticsCycleType = 0;
        }
    }
    else
    {
        catMrTacticsCycleType--;
        if (catMrTacticsCycleType < 0)
        {
            catMrTacticsCycleType = 2;
        }
    }
    catMrTacticsCycleTime = universe.realtimeelapsed + CAT_MR_CYCLE_DELAY;
    catMrTacticsCyclePending = TRUE;
    clCommandMessage(strGetString(str_GM_TacticsSetToEvasive + catMrTacticsCycleType), 0xffffffff);
    soundEventPlay(NULL, 0x108, NULL);
}

// mrRegionDraw 0x00569260: a cycled formation / tactic is ordered once its delay has passed
static void catMrCyclesApply(void)
{
    if (catMrFormationCyclePending && catMrTutEnabled(0x80) && (catMrFormationCycleTime < universe.realtimeelapsed))
    {
        mrSetTheFormation(catMrFormationCycleType, TRUE, FALSE);
        catMrFormationCyclePending = FALSE;
        catMrFormationCycleType = DELTA_FORMATION;
    }
    if (catMrTacticsCyclePending && catMrTutEnabled(0x80) && (catMrTacticsCycleTime < universe.realtimeelapsed))
    {
        cat_mrSetTactics(catMrTacticsCycleType, TRUE);
        catMrTacticsCyclePending = FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_uiProtectFormation                            0x00565f30
    Description : The force-field ships of the selection: without protectees a force-field wall (3 or more ships);
                  else a force-field sphere of up to 12 round each protectee (not a Sentinel), joining the sphere
                  already round it and breaking a plain sphere it is in.
----------------------------------------------------------------------------*/
void cat_uiProtectFormation(SelectCommand *protectees)
{
    MaxSelection selection, group;
    SelectCommand protect;
    SelectCommand *existing;
    CommandToDo *command;
    Node *node;
    sdword index, i;
    Ship *ship;

    if (!CAT_MR_ORDERS_ALLOWED() || !catMrTutEnabled(0x80))
    {
        return;
    }
    cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
    cat_selMakeShipsHaveAbilityUpgrade((SelectCommand *)&selection);
    if (selection.numShips <= 0)
    {
        return;
    }
    if (protectees == NULL)
    {
        soundEventPlay(NULL, 0x108, NULL);
        if (selection.numShips > 2)
        {
            speechEventQueue(selection.ShipPtr[1], 0x400088f, selection.ShipPtr[1]->shiptype, -1, -1, -1, -1, 0.0f, -1);
            clWrapFormation(&universe.mainCommandLayer, (SelectCommand *)&selection, FORCEFIELD_WALL_FORMATION, 0);
            clCommandMessage(strGetString(str_GM_ChangeToForceFieldWall), 0xffffffff);
        }
        else
        {
            speechEventQueue(selection.ShipPtr[0], 0x4000810, selection.ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);
        }
        return;
    }
    for (index = 0; (index < protectees->numShips) && (selection.numShips > 0); index++)
    {
        ship = protectees->ShipPtr[index];
        if ((ship->shiptype == sSentinel) || (ship->shiptype == bSentinel))
        {
            continue;
        }
        protect.numShips = 1;
        protect.ShipPtr[0] = ship;
        existing = NULL;
        for (node = universe.mainCommandLayer.todolist.head; node != NULL; node = node->next)
        {                                                   // the formations protecting it
            command = (CommandToDo *)listGetStructOfNode(node);
            if (bitTest(command->ordertype.attributes, COMMAND_MASK_PROTECTING) &&
                ShipInSelection(command->protect, ship) && bitTest(command->ordertype.attributes, COMMAND_MASK_FORMATION))
            {
                if (command->formation.formationtype == SPHERE_FORMATION)
                {
                    if (command->selection->numShips > 0)
                    {
                        clWrapFormation(&universe.mainCommandLayer, command->selection, DELTA_FORMATION, 0);
                    }
                }
                else if (command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION)
                {
                    existing = command->selection;
                }
            }
        }
        if (existing != NULL)
        {
            cat_selSelectionCopy(&group, (MaxSelection *)existing);
        }
        else
        {
            group.numShips = 0;
        }
        for (i = 0; (i < selection.numShips) && (group.numShips < 12); i++)
        {
            if (!ShipInSelection((SelectCommand *)&group, selection.ShipPtr[i]))
            {
                cat_selSelectionAddShip(&group, selection.ShipPtr[i]);
            }
        }
        MakeShipsNotIncludeTheseShips((SelectCommand *)&selection, (SelectCommand *)&group);
        soundEventPlay(NULL, 0x108, NULL);
        if (group.numShips > 2)
        {
            speechEventQueue(group.ShipPtr[1], 0x400088f, group.ShipPtr[1]->shiptype, -1, -1, -1, -1, 0.0f, -1);
            clWrapProtect(&universe.mainCommandLayer, (SelectCommand *)&group, &protect);
            clWrapFormation(&universe.mainCommandLayer, (SelectCommand *)&group, FORCEFIELD_SPHERE_FORMATION, 0);
            clCommandMessage(strGetString(str_GM_ChangeToForceFieldSphereFront), 0xffffffff);
        }
        else
        {
            speechEventQueue(group.ShipPtr[0], 0x4000810, group.ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_uiProtectFormationCancel                      0x00566250
    Description : The force-field formations the selected ships lead fall back: a sphere to a sphere, a wall to a
                  wall.
----------------------------------------------------------------------------*/
void cat_uiProtectFormationCancel(void)
{
    MaxAnySelection selection;
    CommandToDo *command;
    sdword index;
    Ship *ship;

    if (!CAT_MR_ORDERS_ALLOWED())
    {
        return;
    }
    selSelectionCopy(&selection, (MaxAnySelection *)&selSelected);
    cat_selMakeShipsHaveAbilityUpgrade((SelectCommand *)&selection);
    for (index = 0; index < selection.numTargets; index++)
    {
        ship = (Ship *)selection.TargetPtr[index];
        command = ship->command;
        if ((command == NULL) || !bitTest(command->ordertype.attributes, COMMAND_MASK_FORMATION) ||
            (command->selection->ShipPtr[0] != ship))
        {
            continue;
        }
        if (command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION)
        {
            soundEventPlay(NULL, 0x108, NULL);
            clWrapFormation(&universe.mainCommandLayer, ship->command->selection, SPHERE_FORMATION, 0);
        }
        else if (command->formation.formationtype == FORCEFIELD_WALL_FORMATION)
        {
            soundEventPlay(NULL, 0x108, NULL);
            clWrapFormation(&universe.mainCommandLayer, ship->command->selection, WALL_FORMATION, 0);
        }
    }
}

void cat_uiFireSiegeCannon(void)                            // 0x00566340: the mothership's big gun aims
{
    Ship *mothership;

    if (!CAT_MR_ORDERS_ALLOWED() || (universe.curPlayerPtr->PlayerMothership == NULL))
    {
        return;
    }
    if (singlePlayerGame && !catSpBigGunAllowed)
    {
        clCommandMessage(strGetString(strCantFireBigGun), 0xffffffff);
        return;
    }
    mothership = universe.curPlayerPtr->PlayerMothership;
    if (cat_mothershipHasBigGunModule(mothership))
    {                                                       // the mothership becomes the selection
        selSelected.numShips = 1;
        selSelected.ShipPtr[0] = mothership;
        ioUpdateShipTotals();
        cat_bigGunFireCommand(mothership);
    }
}

void cat_uiFireDreadnoughtCannon(void)                      // 0x005663f0: the dreadnoughts' repulse
{
    MaxSelection selection;

    cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
    if (CAT_MR_ORDERS_ALLOWED())
    {
        cat_selMakeShipsHaveUpgrade((SelectCommand *)&selection, CAT_UPGRADE_DreadnoughtCannon);
        cat_selMakeShipsNotBusy((SelectCommand *)&selection);
        if (selection.numShips > 0)
        {
            clWrapSpecial(&universe.mainCommandLayer, (SelectCommand *)&selection, NULL);
        }
    }
}

void cat_uiSetLeechAction(ubyte action)                     // 0x00566470: spy (6), leech (7), vent (8)
{
    MaxSelection selection;

    cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
    if (CAT_MR_ORDERS_ALLOWED())
    {
        cat_selMakeShipsLeechCapable((SelectCommand *)&selection, TRUE);
        cat_selMakeShipsNotBusy((SelectCommand *)&selection);
        if (selection.numShips > 0)
        {
            cat_clWrapSetLeechAction(&universe.mainCommandLayer, (SelectCommand *)&selection, action);
        }
    }
}

void cat_uiUnleech(void)                                    // 0x00566510
{
    MaxSelection selection;

    cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
    if (CAT_MR_ORDERS_ALLOWED())
    {
        cat_selMakeShipsLeeching((SelectCommand *)&selection);
        cat_selMakeShipsNotBusy((SelectCommand *)&selection);
        if (selection.numShips > 0)
        {
            cat_clWrapUnleech(&universe.mainCommandLayer, (SelectCommand *)&selection);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_mrMimicTarget                                 0x00567860
    Description : The mimics pose as the clicked ship, or (for anything else) as an asteroid: a random one, the MCVs
                  a large one.
----------------------------------------------------------------------------*/
void cat_mrMimicTarget(SpaceObj *target)
{
    MaxSelection selection;
    SelectCommand single;
    sdword index;
    Ship *ship;

    cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
    if (selection.numShips <= 0)
    {
        return;
    }
    if (target->header.objtype == OBJ_ShipType)
    {
        cat_clWrapMimic(&universe.mainCommandLayer, (SelectCommand *)&selection, 1, ((Ship *)target)->shiptype,
                        ((Ship *)target)->playerowner->playerIndex, 0);
        return;
    }
    for (index = 0; index < selection.numShips; index++)
    {
        ship = selection.ShipPtr[index];
        single.numShips = 1;
        single.ShipPtr[0] = ship;
        cat_clWrapMimic(&universe.mainCommandLayer, &single, 2, 0, CAT_MIMIC_PLAYER_OBJECTS, (ship->shiptype == sMCV) ? 3 : 1);
    }
}

bool32 cat_mrTacticalOverlayGet(void)                       // 0x0056a080
{
    return mrDrawTactical;
}

/*-----------------------------------------------------------------------------
    The orders of the key-command table that live in the main region (functions the export does not list; the key
    command each serves in brackets).
-----------------------------------------------------------------------------*/
// [Move] 0x0056a090: the movement pie plate with the move callback (the sensors manager for a far point)
void cat_mrMoveShips(void)
{
    vector dist;

    tutGameMessage("KB_Move");
    if (playPackets || mrDisabled)
    {
        return;
    }
    if (!catMrTutEnabled(0x200))
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    cat_selMakeShipsMobileNotBusy((SelectCommand *)&selSelected);
    makeShipsControllable((SelectCommand *)&selSelected, COMMAND_MOVE);
    if ((catMrRightMode != 0) || catMrSelecting || (selSelected.numShips < 1))
    {
        return;
    }
    selCentrePointCompute();
    vecSub(dist, selCentrePoint, universe.mainCameraCommand.actualcamera.eyeposition);
    if (piePointSpecMode == PSM_Idle)
    {
        if (vecMagnitudeSquared(dist) > (real32)MAX_MOVE_DISTANCE)
        {                                                   // too far: move in the sensors manager
            if (!smSensorsActive)
            {
                smSensorsBegin(NULL, NULL);
                piePointModeOnOff(PIE_SPEC_Move, cat_mrMoveCallback);
                return;
            }
        }
        piePointModeOnOff(PIE_SPEC_Move, cat_mrMoveCallback);
        return;
    }
    soundEventPlay(NULL, 0x117, NULL);
    piePointModeOnOff(PIE_SPEC_Move, cat_mrMoveCallback);
}

// 0x00564a90: the pie plate's point: an MP hyperspace jump, or a move there (the ships with
//      evasive tactics and a move order switch to neutral)
void cat_mrMoveCallback(vector *point)
{
    MaxSelection selection;
    vector destination = *point;
    sdword index;
    Ship *ship;

    MakeShipsMobile((SelectCommand *)&selSelected, TRUE);
    if ((selSelected.numShips > 0) && !isnan((double)destination.x) && !isnan((double)destination.y) &&
        !isnan((double)destination.z))
    {
        cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
        index = 0;
        while (index < selection.numShips)
        {                                                   // CAT: keeps the aggressive ships moving already
            ship = selection.ShipPtr[index];
            if ((ship->tacticstype == Aggressive) && (ship->command != NULL) &&
                (ship->command->ordertype.order == COMMAND_ATTACK))
            {
                index++;
            }
            else
            {
                selection.numShips--;
                selection.ShipPtr[index] = selection.ShipPtr[selection.numShips];
            }
        }
        if (selection.numShips > 0)
        {
            clWrapSetTactics(&universe.mainCommandLayer, (SelectCommand *)&selection, Neutral);
        }
        if (MP_HyperSpaceFlag)
        {
            vector dist;
            vecSub(dist, selCentrePoint, destination);
            if (universe.curPlayerPtr->resourceUnits < (sdword)hyperspaceCost(fsqrt(vecMagnitudeSquared(dist)),
                                                                              (SelectCommand *)&selSelected))
            {
                if (battleCanChatterAtThisTime(BCE_CannotComply, selSelected.ShipPtr[0]))
                {
                    battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_CannotComply, selSelected.ShipPtr[0], SOUND_EVENT_DEFAULT);
                }
            }
            else
            {
                clWrapMpHyperspace(&universe.mainCommandLayer, (SelectCommand *)&selSelected, selCentrePoint, destination);
            }
        }
        else
        {
            clWrapMove(&universe.mainCommandLayer, (SelectCommand *)&selSelected, CAT_MOVEPATH_ONCE, selCentrePoint, 1,
                       &destination);
            if (selSelected.ShipPtr[0]->staticinfo->shipclass == CLASS_Mothership)
            {
                speechEventQueue(NULL, 0x4000801, selSelected.ShipPtr[0]->shiptype, -1, 0,
                                 selSelected.ShipPtr[0]->playerowner->playerIndex, -1, 5.0f, -1);
            }
            else
            {
                speechEventQueue(selSelected.ShipPtr[0], 0x4000801, selSelected.ShipPtr[0]->shiptype, -1, -1, -1, -1,
                                 0.0f, -1);
            }
        }
    }
    soundEventPlay(NULL, 0x117, NULL);
    piePointModeOnOff(PIE_SPEC_None, NULL);
}

// 0x00564d70: a waypoint added (at most 20; the last one is replaced beyond that)
void cat_mrWaypointAdd(vector *point)
{
    if (catMrWaypointCount >= CAT_MR_MAX_WAYPOINTS - 1)
    {
        catMrWaypoints[CAT_MR_MAX_WAYPOINTS - 1] = *point;
    }
    else
    {
        catMrWaypoints[catMrWaypointCount] = *point;
        catMrWaypointCount++;
        soundEventPlay(NULL, 0x118, NULL);
    }
    if (catMrWaypointCentre && smSensorsActive)
    {
        // CAT (M2-R6 managers, sensors): the sensors manager pans to the last waypoint (smCameraLookatPoint
        //      0x00a5b1f0); the option is off by default (0x00a75270) and set by the options screen (M2-R6 front end)
        smCameraLookatPoint = catMrWaypoints[catMrWaypointCount - 1];
    }
}

// [Waypoints] 0x00564e20: waypoint mode on
void cat_mrWaypointsStart(void)
{
    vector dist;

    if (!catMrTutEnabled(0x800))
    {
        return;
    }
    if (piePointSpecMode != PSM_Idle)
    {
        piePointModeOnOff(PIE_SPEC_None, NULL);
    }
    MakeShipsMobile((SelectCommand *)&selSelected, TRUE);
    if ((catMrRightMode != 0) || catMrSelecting || (selSelected.numShips < 1))
    {
        return;
    }
    selCentrePointCompute();
    vecSub(dist, selCentrePoint, universe.mainCameraCommand.actualcamera.eyeposition);
    if (piePointSpecMode != PSM_Idle)
    {
        return;
    }
    if (catMrRightClickCSA && (vecMagnitudeSquared(dist) > (real32)MAX_MOVE_DISTANCE))
    {
        smSensorsBegin(NULL, NULL);
    }
    piePointModeOnOff(PIE_SPEC_Waypoint, cat_mrWaypointAdd);
    catMrWaypointCount = 0;
    catPieWaypointMode = TRUE;
}

// [Waypoints] 0x00564f40: the path ordered (normal, a loop or end to end)
void cat_mrWaypointsEnd(void)
{
    sdword pathType;

    if (!catMrTutEnabled(0x1000))
    {
        return;
    }
    if ((selSelected.numShips > 0) && (catMrWaypointCount > 0))
    {
        MakeShipsMobile((SelectCommand *)&selSelected, TRUE);
        switch (catMrWaypointMode)
        {
            case 0:  pathType = CAT_MOVEPATH_ONCE; break;
            case 1:  pathType = CAT_MOVEPATH_LOOP; break;
            case 2:  pathType = CAT_MOVEPATH_PATROL_OUT; break;
            default: pathType = CAT_MOVEPATH_ONCE; break;   // CAT-FIX: the exe passes an uninitialised word
        }
        clWrapMove(&universe.mainCommandLayer, (SelectCommand *)&selSelected, pathType, selCentrePoint,
                   catMrWaypointCount, catMrWaypoints);
        speechEventQueue(selSelected.ShipPtr[0], 0x4000801, selSelected.ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);
        soundEventPlay(NULL, 0x118, NULL);
        if (tutorial == TUTORIAL_ONLY)
        {
            switch (catMrWaypointMode)
            {
                case 0: tutGameMessage("WaypointNormal"); break;
                case 1: tutGameMessage("WaypointCircular"); break;
                case 2: tutGameMessage("WaypointEndToEnd"); break;
                default: break;
            }
        }
    }
    if (piePointSpecMode != PSM_Idle)
    {
        piePointModeOnOff(PIE_SPEC_None, NULL);
    }
}

// [Waypoints] 0x00565070: start or end the waypoints
void cat_mrWaypointsToggle(void)
{
    if (catPieWaypointMode)
    {
        cat_mrWaypointsEnd();
    }
    else
    {
        cat_mrWaypointsStart();
    }
}

// [Toggle_Waypoint_Modes] 0x00565090: normal -> loop -> end to end -> normal
void cat_mrWaypointModeCycle(void)
{
    switch (catMrWaypointMode)
    {
        case 0:
            catMrWaypointMode = 1;
            clCommandMessage(strGetString(strWaypointModePatrolCircular), 0xffffffff);
            break;
        case 1:
            catMrWaypointMode = 2;
            clCommandMessage(strGetString(strWaypointModePatrolEndToEnd), 0xffffffff);
            break;
        case 2:
            catMrWaypointMode = 0;
            clCommandMessage(strGetString(strWaypointModeNormal), 0xffffffff);
            break;
        default:
            break;
    }
}

// [Cancel_Last_Waypoint] 0x00565170
void cat_mrWaypointRemoveLast(void)
{
    catMrWaypointCount--;
    if (catMrWaypointCount < 0)
    {
        catMrWaypointCount = 0;
    }
}

// [Dock] 0x00563c30: dock for support at the nearest host (purpose 0)
void cat_mrDockShips(void)
{
    if (!CAT_MR_ORDERS_ALLOWED())
    {
        return;
    }
    if (!catMrTutEnabled(0x40))
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    MakeShipsGuardCapable((SelectCommand *)&selSelected, TRUE);
    if (selSelected.numShips > 0)
    {
        soundEventPlay(NULL, 0x108, NULL);
        clWrapDock(&universe.mainCommandLayer, (SelectCommand *)&selSelected, CAT_DOCK_SUPPORT, NULL);
    }
    else
    {
        soundEventPlay(NULL, 0x10a, NULL);
    }
}

// [Upgrade_Ships] 0x00565650: the upgradable ships upgrade (those that upgrade by docking dock for it, purpose 0)
void cat_mrUpgradeShips(void)
{
    MaxSelection inPlace, byDocking, notUpgradable;
    sdword index;
    Ship *ship;

    if (!CAT_MR_ORDERS_ALLOWED())
    {
        return;
    }
    inPlace.numShips = 0;
    byDocking.numShips = 0;
    notUpgradable.numShips = 0;
    for (index = 0; index < selSelected.numShips; index++)
    {
        ship = selSelected.ShipPtr[index];
        if (ship->shipOrDerelict.catDisabled)
        {
            continue;
        }
        if (!cat_shipIsUpgradable(ship))
        {
            cat_selSelectionAddShip(&notUpgradable, ship);
        }
        else if (ship->staticinfo->UpgradeByDocking)
        {
            cat_selSelectionAddShip(&byDocking, ship);
        }
        else
        {
            cat_selSelectionAddShip(&inPlace, ship);
        }
    }
    (void)notUpgradable;
    if (inPlace.numShips > 0)
    {
        cat_clWrapUpgradeShips(&universe.mainCommandLayer, (SelectCommand *)&inPlace, FALSE);
    }
    if (byDocking.numShips > 0)
    {
        cat_ioMessageAdd(strGetString(strUpgradingByDockingNotification), 0xffffffff, 1, byDocking.ShipPtr[0]);
        clWrapDock(&universe.mainCommandLayer, (SelectCommand *)&byDocking, CAT_DOCK_SUPPORT, NULL);
    }
}

// [Focus_On_Mothership] 0x00565e80: the next mothership (a carrier counts) is focused
void cat_mrFocusOnMothership(void)
{
    FocusCommand focus;

    if ((tutorial == TUTORIAL_ONLY) && !((*((udword *)&tutEnable + 1) & 0x10000) != 0))
    {                                                       // CAT-TODO(stage 6 tutorial): the second flag word
        return;
    }
    if ((mrMothershipPtr == NULL) ||
        ((universe.mainCameraCommand.currentCameraStack != NULL) &&
         (universe.mainCameraCommand.currentCameraStack->focus.numShips == 1) &&
         (universe.mainCameraCommand.currentCameraStack->focus.ShipPtr[0] == mrMothershipPtr)))
    {
        mrMothershipPtr = mrNextMothershipPtr(mrMothershipPtr);
    }
    if ((mrMothershipPtr == NULL) || bitTest(mrMothershipPtr->header.flags, SOF_Dead))
    {
        return;
    }
    focus.numShips = 1;
    focus.ShipPtr[0] = mrMothershipPtr;
    if (smSensorsActive)
    {
        selSelectionCopy((MaxAnySelection *)&selSelecting, (MaxAnySelection *)&focus);
        smCullAndFocusSelecting();
    }
    else
    {
        ccFocus(&universe.mainCameraCommand, &focus);
    }
    soundEventPlay(NULL, 0x108, NULL);
}

// [Enter_Slipgate] 0x005665a0: the selection docks with the nearest visited slipgate (purpose 10)
void cat_mrEnterSlipgate(void)
{
    vector centre = { 0.0f, 0.0f, 0.0f };
    real32 distance, nearestDistance = 1e21f;
    Ship *gate = NULL, *ship;
    Node *node;
    sdword index;

    if (!CAT_MR_ORDERS_ALLOWED())
    {
        return;
    }
    for (index = 0; index < selSelected.numShips; index++)
    {
        vecAddTo(centre, selSelected.ShipPtr[index]->obj.posinfo.position);
    }
    vecDivideByScalar(centre, (real32)selSelected.numShips, distance);
    if (!singlePlayerGame || (singlePlayerGameInfo.currentMission != 15))
    {
        // CAT (M2-R6 nis-fow-camera): the player's grid selected (fowSelect, 0x00566674), the slipgates whose cell was
        //      visited (fowGetVisitedW == 1; no grid: none)
        fowSelect(universe.curPlayerPtr->catFOWGrid);
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            ship = (Ship *)listGetStructOfNode(node);
            if (bitTest(ship->staticinfo->catTypeFlags, 0x4000) &&
                (fowGetVisitedW(ship->obj.posinfo.position) == 1))
            {
                distance = cat_vecDistance(&centre, &ship->obj.posinfo.position);
                if (distance < nearestDistance)
                {
                    nearestDistance = distance;
                    gate = ship;
                }
            }
        }
    }
    if (gate == NULL)
    {
        clCommandMessage(strGetString(strNoSlipgateFoundNotify), 0xff0000ff);
        return;
    }
    cat_selMakeShipsDockCapableFor((SelectCommand *)&selSelected, gate->shiptype, TRUE);
    if (selSelected.numShips > 0)
    {
        clWrapDock(&universe.mainCommandLayer, (SelectCommand *)&selSelected, CAT_DOCK_SLIPGATE, gate);
    }
}

// [Hyperspace] 0x00566920: the single-player hyperspace button, or the multiplayer jump through the pie plate
void cat_mrHyperspace(void)
{
    if (singlePlayerGame)
    {
        if (singlePlayerGameInfo.playerCanHyperspace)
        {
            spHyperspaceButtonPushed();
        }
        return;
    }
    // CAT-TODO(stage 5 multiplayer options): the multiplayer jump needs bit 0x4000 of the game options word
    //      0x008860ec (the Cataclysm tpGameCreated layout); it counts as clear, as with the option off
    if (TRUE)
    {
        return;
    }
    cat_selMakeShipsMobileNotBusy((SelectCommand *)&selSelected);
    makeShipsControllable((SelectCommand *)&selSelected, COMMAND_MP_HYPERSPACING);
    makeSelectionHyperspaceCapable((SelectCommand *)&selSelected);
    if (selSelected.numShips == 0)
    {
        if (piePointSpecMode != PSM_Idle)
        {
            soundEventPlay(NULL, 0x117, NULL);
            piePointModeOnOff(PIE_SPEC_None, NULL);
        }
        clCommandMessage(strGetString(strNoHyperspacingShipsAvailable), 0xff0000ff);
        return;
    }
    if (!smSensorsActive)
    {
        smSensorsBegin(NULL, NULL);
    }
    if (!smSensorsDisable && !smZoomingIn)
    {
        MP_HyperSpaceFlag = TRUE;
        if (piePointSpecMode == PSM_Idle)
        {
            soundEventPlay(NULL, 0x116, NULL);
            piePointModeOnOff(PIE_SPEC_Move, cat_mrMoveCallback);
        }
    }
}

// [Escape] 0x0054f5f0 (the key-command table's callback): the first
//      thing that is up goes away (the fleet-intelligence text, the chat buffer, the NIS, the point mode, the band
//      box, the popup menu, the selection, the sensors manager), else the in-game menu opens
void cat_mrEscape(void)
{
    if (singlePlayerGame && popupTextNumLines)
    {
        popupTextNumLines = 0;
        return;
    }
    if (multiPlayerGame && ViewingBuffer)
    {
        gcCancelViewingBuffer();
        return;
    }
    if (thisNisPlaying != NULL)
    {
        if ((nisCameraCutTime == 0.0f) && (nisScissorFadeIn == 0.0f))
        {
            nisGoToEnd(thisNisPlaying);
        }
        return;
    }
    if (piePointSpecMode != PSM_Idle)
    {
        if (catMrTutEnabled(0x8000))
        {
            piePointModeOnOff(PIE_SPEC_None, NULL);
            soundEventPlay(NULL, 0x117, NULL);
        }
        return;
    }
    if (catMrSelecting)
    {
        catMrSelecting = FALSE;
        selSelecting.numTargets = 0;
        return;
    }
    if (cat_popupMenuIsOpen())
    {
        cat_popupMenuClose();
        return;
    }
    if ((selSelected.numShips > 0) && catMrTutEnabled(0x100000))
    {
        soundEventPlay(NULL, 0x108, NULL);
        selSelected.numShips = 0;
        ioUpdateShipTotals();
        return;
    }
    if (smSensorsActive)
    {
        smSensorsClose(NULL, NULL);
        return;
    }
    if (!multiPlayerGame)
    {
        universePause = TRUE;
    }
    soundEventPlay(NULL, 0x108, NULL);
    gpStartInGameEscapeMenu();
}

/*-----------------------------------------------------------------------------
    The mouse (mrRegionProcess's handlers and the band box)
-----------------------------------------------------------------------------*/
// the rectangle from the anchor to the mouse (+1 on the far sides), as each band-box function builds it
static void catMrBandRect(rectangle *rect)
{
    rect->x0 = mrOldMouseX;
    rect->x1 = mouseCursorX();
    if (mouseCursorX() < mrOldMouseX)
    {
        rect->x0 = mouseCursorX();
        rect->x1 = mrOldMouseX;
    }
    rect->y0 = mrOldMouseY;
    rect->y1 = mouseCursorY();
    if (mouseCursorY() < mrOldMouseY)
    {
        rect->y0 = mouseCursorY();
        rect->y1 = mrOldMouseY;
    }
    rect->x1++;
    rect->y1++;
}

static bool32 catMrMouseMovedFromAnchor(void)
{
    return (abs(mouseCursorX() - mrOldMouseX) >= selClickBoxWidth) || (abs(mouseCursorY() - mrOldMouseY) >= selClickBoxHeight);
}

void cat_mrHoldLeft(sdword sensors)                         // 0x0056aee0
{
    if (!mrDisabled && catMrSelecting)
    {
        mrSelectHold(&universe.mainCameraCommand.actualcamera, sensors);
    }
}

void cat_mrPressLeft(void)                                  // 0x0056af10: the band box's anchor
{
    if (!cat_popupMenuIsOpen() && !cat_shipPrintDragStart() && (catMrRightMode == 0) && (piePointSpecMode == PSM_Idle))
    {
        mrOldMouseX = mouseCursorX();
        mrOldMouseY = mouseCursorY();
        catMrSelecting = TRUE;
    }
}

void cat_mrReleaseLeft(sdword sensors)                      // 0x0056af60
{
    if (cat_popupMenuIsOpen())
    {
        cat_popupMenuClick();
        return;
    }
    cat_shipPrintDragEnd();
    if (mouseRightButton())
    {
        return;
    }
    if ((piePointSpecMode == PSM_XY) || (piePointSpecMode == PSM_Z))
    {
        cat_piePointSpecExecute();
        return;
    }
    if (catMrSelecting)
    {
        if (!cat_mrBandBoxRelease(&universe.mainCameraCommand.actualcamera, sensors))
        {
            cat_mrClickRelease(sensors, FALSE);
        }
        catMrSelecting = FALSE;
        selSelecting.numTargets = 0;
        piePointModePause(FALSE);
    }
}

void cat_mrPressRight(void)                                 // 0x0056afe0: the camera turns while it is held
{
    if (cat_shipPrintIsDragging() || cat_gameStatsIsActive())
    {
        return;
    }
    if (cat_popupMenuIsOpen())
    {
        if (!cat_popupMenuClickIfTarget() && cat_popupMenuMouseInside())
        {
            return;
        }
        cat_popupMenuClose();
    }
    if (catMrSelecting)
    {
        catMrSelecting = FALSE;
        selSelecting.numTargets = 0;
    }
    piePointModePause(TRUE);
    catMrRightMode = 2;
    mrOldMouseX = mouseCursorX();
    mrOldMouseY = mouseCursorY();
    mouseCursorHide();
    if (helpinfoactive)
    {
        mousePositionSet(helpinforegion->rect.x0 / 2, MAIN_WindowHeight / 2);
    }
    else
    {
        mousePositionSet(MAIN_WindowWidth / 2, MAIN_WindowHeight / 2);
    }
    mrMouseHasMoved = 0;
}

/*-----------------------------------------------------------------------------
    Name        : cat_mrReleaseRight                                0x0056b0b0
    Description : The right button up: the camera stops turning; a click (the mouse hardly moved) on an object the
                  selection can act on gives the context order (cat_mouseGetContextAction), any other click the
                  popup menu of the selection's orders.
----------------------------------------------------------------------------*/
void cat_mrReleaseRight(sdword sensors)
{
    CatTargetActions selectionActions, targetActions;
    MaxSelection targets, selection;
    sdword action, ability;
    Node *node;
    Ship *clicked, *found;

    if (cat_shipPrintIsDragging() || cat_popupMenuMouseInside() || cat_gameStatsIsActive())
    {
        return;
    }
    piePointModePause(FALSE);
    if (catMrRightMode == 2)
    {
        mousePositionSet(mrOldMouseX, mrOldMouseY);
        if (!mrDisabled || (spHyperspaceRollCallScreen != NULL))
        {
            mouseCursorShow();
        }
        catMrRightMode = 0;
        mouseClipToRect(NULL);
    }
    if (mrDisabled || (mrMouseHasMoved > MR_MouseMovementClickLimit))
    {
        return;
    }
    if (piePointSpecMode != PSM_Idle)
    {
        cat_popupMenuOpen(NULL, NULL);
        return;
    }
    clicked = selSelectionClick(sensors ? universe.SpaceObjList.head : universe.RenderList.head,
                                &universe.mainCameraCommand.actualcamera, mouseCursorX(), mouseCursorY(), TRUE, TRUE,
                                sensors != 0);
    if ((clicked == NULL) || smSensorsActive || (!catMrRightClickCSA && (tutorial != TUTORIAL_ONLY)) ||
        ShipInSelection((SelectCommand *)&selSelected, clicked))
    {
        cat_popupMenuOpen(NULL, NULL);
        return;
    }
    cat_selComputeTargetActions(&selectionActions, (MaxSelection *)&selSelected);
    targets.numShips = 1;
    targets.ShipPtr[0] = clicked;
    cat_selComputeTargetActions(&targetActions, &targets);
    action = cat_mouseGetContextAction(&selectionActions, &targetActions, (MaxSelection *)&selSelected, clicked);
    if (singlePlayerGame && (singlePlayerGameInfo.currentMission == 10) && (clicked->shiptype == dBigGun))
    {                                                       // mission 10: the big gun is salvaged through its unit
        found = NULL;
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            if (((Ship *)listGetStructOfNode(node))->shiptype == dBigGunUnit)
            {
                found = (Ship *)listGetStructOfNode(node);
            }
        }
        if (found == NULL)
        {
            cat_popupMenuOpen(NULL, NULL);
            return;
        }
        if (CAT_MR_ORDERS_ALLOWED())
        {
            cat_uiSpecialTargetCommand((SelectCommand *)&targets, 0);
        }
        soundEventPlay(NULL, 0x109, NULL);
        return;
    }
    switch (action)
    {
        case CAT_CA_None:
            cat_popupMenuOpen(NULL, NULL);
            return;
        case CAT_CA_Harvest:
            mrHarvestResources((char *)clicked, NULL);
            soundEventPlay(NULL, 0x109, NULL);
            return;
        case CAT_CA_Salvage:
            ability = 0;
            break;
        case CAT_CA_Attack:
            if (targets.numShips > 0)
            {
                MakeShipsAttackCapable((SelectCommand *)&selection, (SelectCommand *)&selSelected, TRUE);
                cat_selSelectionCopy((MaxSelection *)&selSelected, &selection);
                if (selSelected.numShips > 0)
                {
                    cat_selMakeTargetsAttackable((SelectCommand *)&targets, FALSE);
                    MakeShipsNotIncludeTheseShips((SelectCommand *)&targets, (SelectCommand *)&selSelected);
                    if (targets.numShips > 0)
                    {
                        speechEventAttack();
                        clWrapAttack(&universe.mainCommandLayer, (SelectCommand *)&selSelected, (AttackCommand *)&targets);
                        tutGameMessage("Game_BandBoxForceAttack");
                    }
                }
            }
            soundEventPlay(NULL, 0x109, NULL);
            tutGameMessage("ClickAttack");
            return;
        case CAT_CA_Repair:
            ability = 1;
            break;
        case CAT_CA_ProtectFormation:
            cat_uiProtectFormation((SelectCommand *)&targets);
            soundEventPlay(NULL, 0x109, NULL);
            return;
        case CAT_CA_SlipgateSmall:
        case CAT_CA_SlipgateMedium:
        case CAT_CA_SlipgateLarge:
            mrDockingOrders(clicked);
            soundEventPlay(NULL, 0x109, NULL);
            return;
        case CAT_CA_Dock:
            cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
            MakeShipsGuardCapable((SelectCommand *)&selection, TRUE);
            clWrapDock(&universe.mainCommandLayer, (SelectCommand *)&selection, CAT_DOCK_SUPPORT, clicked);
            soundEventPlay(NULL, 0x109, NULL);
            return;
        case CAT_CA_Leech:
            ability = 2;
            break;
        case CAT_CA_Link:
            cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
            cat_selSelectionAddShip(&selection, clicked);
            if (selection.numShips > 0)
            {
                cat_clWrapLink(&universe.mainCommandLayer, (SelectCommand *)&selection);
            }
            soundEventPlay(NULL, 0x109, NULL);
            return;
        case CAT_CA_Mimic:
            cat_mrMimicTarget((SpaceObj *)clicked);
            soundEventPlay(NULL, 0x109, NULL);
            return;
        case CAT_CA_EMP:            ability = 4; break;
        case CAT_CA_Swarm:          ability = 3; break;
        case CAT_CA_Ram:            ability = 6; break;
        case CAT_CA_FireMissiles:   ability = 7; break;
        case CAT_CA_Infect:         ability = 5; break;
        default:
            return;
    }
    if (CAT_MR_ORDERS_ALLOWED())
    {
        cat_uiSpecialTargetCommand((SelectCommand *)&targets, ability);
    }
    soundEventPlay(NULL, 0x109, NULL);
}

/*-----------------------------------------------------------------------------
    Name        : cat_mrClickRelease                                0x0056ad50
    Description : A click (no band box): the overlay first, then the object under the mouse (or the one that was
                  under it a moment ago), else the selection is cleared (not with the multiple-select key).
    Inputs      : sensors - 0 the main view (the render list), 1 the sensors manager (all objects);
                  doubleClick - passed on to mrObjectClick
----------------------------------------------------------------------------*/
void cat_mrClickRelease(sdword sensors, bool32 doubleClick)
{
    Node *list = (sensors == 0) ? universe.RenderList.head : universe.SpaceObjList.head;
    Ship *ship;

    if (mrDisabled)
    {                                                       // the stats screen takes the click
        cat_kbCommandExecute(sensors, CAT_KB_SkipSensors);
        cat_gameStatsClick();
        return;
    }
    if (cat_ioClick(catMrKbHeld(CAT_KB_MultipleSelect)) || (catMrRightMode != 0))
    {
        return;
    }
    ship = selSelectionClick(list, &universe.mainCameraCommand.actualcamera, mouseCursorX(), mouseCursorY(), TRUE, TRUE,
                             sensors != 0);
    if (ship != NULL)
    {
        if (catMrTutEnabled(0x40000))
        {
            mrObjectClick(ship, doubleClick);
        }
        return;
    }
    if ((mouseCursorObjLast != NULL) && (universe.realtimeelapsed <= mrFastMoveShipClickTime + mouseCursorLastObjTime) &&
        (abs(mouseCursorX() - mouseCursorLastX) < mrFastMoveShipClickX) &&
        (abs(mouseCursorY() - mouseCursorLastY) < mrFastMoveShipClickY))
    {                                                       // the object was under the mouse a moment ago
        if (catMrTutEnabled(0x40000) && bitTest(mouseCursorObjLast->header.flags, SOF_Selectable))
        {
            mrObjectClick((Ship *)mouseCursorObjLast, doubleClick);
        }
        return;
    }
    if (!catMrKbHeld(CAT_KB_MultipleSelect) && catMrTutEnabled(0x100000))
    {
        selSelected.numShips = 0;
        ioUpdateShipTotals();
    }
}

void cat_mrReleaseLeftCB(sdword sensors)                    // 0x0056b6a0: a double click
{
    if (!cat_shipPrintIsDragging() && !cat_popupMenuIsOpen() && !cat_gameStatsIsActive())
    {
        cat_mrClickRelease(sensors, TRUE);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_mrBandBoxRelease                              0x0056a610
    Description : The left button up after a band box (the mouse moved from the anchor): by the modifier key a
                  force attack, add to the selection, guard, the special popup on the boxed objects, a focus, an
                  attack, or (none) the boxed ships become the selection.
    Return      : FALSE if it was no band box (a click)
----------------------------------------------------------------------------*/
bool32 cat_mrBandBoxRelease(Camera *camera, sdword sensors)
{
    Node *list;
    rectangle rect;
    MaxSelection selection;
    sdword index;
    char *eventName;
    Ship *ship;

    if (universePause || mrDisabled)
    {
        return FALSE;
    }
    sensors = (sensors == 1);
    if ((abs(mouseCursorX() - mrOldMouseX) < selClickBoxWidth) && (abs(mouseCursorY() - mrOldMouseY) < selClickBoxHeight))
    {
        return FALSE;
    }
    catMrBandRect(&rect);
    mrSelectHold(camera, sensors);
    list = sensors ? universe.SpaceObjList.head : universe.RenderList.head;
    if (catMrKbHeld(CAT_KB_BandboxForceAttack))
    {
        if (!playPackets && (selSelecting.numTargets > 0))
        {
            MakeShipsAttackCapable((SelectCommand *)&selection, (SelectCommand *)&selSelected, TRUE);
            cat_selSelectionCopy((MaxSelection *)&selSelected, &selection);
            if (selSelected.numShips > 0)
            {
                cat_selMakeTargetsAttackable((SelectCommand *)&selSelecting, FALSE);
                MakeShipsNotIncludeTheseShips((SelectCommand *)&selSelecting, (SelectCommand *)&selSelected);
                if (selSelecting.numTargets > 0)
                {
                    speechEventAttack();
                    clWrapAttack(&universe.mainCommandLayer, (SelectCommand *)&selSelected, (AttackCommand *)&selSelecting);
                    tutGameMessage("Game_BandBoxForceAttack");
                }
            }
        }
        goto done;
    }
    if (catMrKbHeld(CAT_KB_MultipleSelect))
    {
        if ((piePointSpecMode != PSM_XY) && catMrTutEnabled(0x80000))
        {
            selRectDragAddFunction(list, camera, &rect, sensors);
            ioUpdateShipTotals();
            tutGameMessage("MultipleSelect");
        }
        goto done;
    }
    if (catMrKbHeld(CAT_KB_BandboxGuard))
    {
        if (!playPackets && (selSelecting.numTargets > 0))
        {
            cat_selMakeShipsProtectCapable((SelectCommand *)&selSelected, TRUE);
            if (selSelected.numShips > 0)
            {
                MakeShipsFriendlyAndAlliesShips((SelectCommand *)&selSelecting, universe.curPlayerPtr);
                MakeShipsNotIncludeTheseShips((SelectCommand *)&selSelecting, (SelectCommand *)&selSelected);
                if (selSelecting.numTargets > 0)
                {
                    speechEventQueue(NULL, 0x4001061, -1, -1, 0, selSelected.ShipPtr[0]->playerowner->playerIndex, -1,
                                     5.0f, -1);
                    clWrapProtect(&universe.mainCommandLayer, (SelectCommand *)&selSelected, (ProtectCommand *)&selSelecting);
                    tutGameMessage("Game_BandBoxGuard");
                }
            }
        }
        goto done;
    }
    if (catMrKbHeld(CAT_KB_ShipSpecial))
    {
        if (!playPackets)
        {
            MakeShipsSpecialTargetCapable((SelectCommand *)&selSelected, FALSE, TRUE);
            if (selSelected.numShips > 0)
            {
                mrSpecialBandBox = TRUE;
                MakeShipsNotIncludeTheseShips((SelectCommand *)&selSelecting, (SelectCommand *)&selSelected);
                MakeTargetsNotIncludeMissiles((SelectAnyCommand *)&selSelecting);
                if (selSelecting.numTargets > 0)
                {
                    cat_popupMenuOpen(CAT_PM_TARGET_KEYCOMMAND, (MaxSelection *)&selSelecting);
                    tutGameMessage("Game_BandBoxSpecial");
                }
            }
        }
        goto done;
    }
    if (catMrKbHeld(CAT_KB_BandboxFocus))
    {
        if (selSelecting.numTargets > 0)
        {
            if (sensors != 1)
            {
                ccFocusClose(&universe.mainCameraCommand, (FocusCommand *)&selSelecting);
                selSelecting.numTargets = 0;
                tutGameMessage("Game_BandBoxFocus");
                eventName = "AltFocus";
                goto message;
            }
            index = 0;
            while (index < selSelecting.numTargets)
            {                                               // the sensors manager focuses on what it may
                ship = (Ship *)selSelecting.TargetPtr[index];
                if ((ship->header.objtype == OBJ_ShipType) && !cat_ccShipIsLegalToFocusOn(ship))
                {
                    selSelecting.numTargets--;
                    selSelecting.TargetPtr[index] = selSelecting.TargetPtr[selSelecting.numTargets];
                }
                else
                {
                    index++;
                }
            }
            if (selSelecting.numTargets > 0)
            {
                smCullAndFocusSelecting();
            }
        }
        eventName = "AltFocus";
        goto message;
    }
    if (catMrKbHeld(CAT_KB_BandboxAttack))
    {
        if (playPackets)
        {
            goto done;
        }
        if (selSelecting.numTargets > 0)
        {
            MakeShipsAttackCapable((SelectCommand *)&selection, (SelectCommand *)&selSelected, TRUE);
            cat_selSelectionCopy((MaxSelection *)&selSelected, &selection);
            if (selSelected.numShips > 0)
            {
                cat_selMakeTargetsAttackable((SelectCommand *)&selSelecting, FALSE);
                MakeShipsNotIncludeTheseShips((SelectCommand *)&selSelecting, (SelectCommand *)&selSelected);
                MakeTargetsOnlyNonForceAttackTargets((SelectAnyCommand *)&selSelecting, universe.curPlayerPtr);
                if (selSelecting.numTargets > 0)
                {
                    speechEventAttack();
                    clWrapAttack(&universe.mainCommandLayer, (SelectCommand *)&selSelected, (AttackCommand *)&selSelecting);
                    tutGameMessage("Game_BandBoxAttack");
                }
            }
        }
        eventName = "BandAttack";
        goto message;
    }
    // no modifier: the boxed ships are the selection
    if ((piePointSpecMode == PSM_XY) ||
        ((tutorial == TUTORIAL_ONLY) && (!catMrTutEnabled(0x80000) || ((selSelecting.numTargets == 0) && !catMrTutEnabled(0x100000)))))
    {
        goto done;
    }
    selRectDragFunction(list, &universe.mainCameraCommand.actualcamera, &rect, (SpaceObjRotImpTarg **)selSelected.ShipPtr,
                        &selSelected.numShips, TRUE, FALSE, FALSE, sensors);
    ioUpdateShipTotals();
    if (selSelected.numShips < 1)
    {
        goto done;
    }
    if (!playPackets)
    {
        if ((universe.curPlayerPtr->PlayerMothership != NULL) &&
            selShipInSelection(selSelected.ShipPtr, selSelected.numShips, universe.curPlayerPtr->PlayerMothership) &&
            (universe.curPlayerPtr->PlayerMothership->staticinfo->shipclass == CLASS_Mothership))
        {                                                   // the mothership's fleet voice
            speechEventQueue(NULL, 0x4000800, universe.curPlayerPtr->PlayerMothership->shiptype, -1, 0,
                             selSelected.ShipPtr[0]->playerowner->playerIndex, -1, 5.0f, -1);
        }
        else
        {
            speechEventQueue(selSelected.ShipPtr[0], 0x4000800, selSelected.ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);
        }
    }
    eventName = "Game_BandBoxSelect";
message:
    tutGameMessage(eventName);
done:
    ioUpdateShipTotals();
    selSelecting.numTargets = 0;
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_mrSelectRectDraw                              0x0056ac00
    Description : The band box in the modifier's colour, with the boxed objects; a small box outlines the object
                  under the mouse instead.
----------------------------------------------------------------------------*/
void cat_mrSelectRectDraw(sdword sensors)
{
    rectangle rect;
    color c;
    Ship *ship;

    if (catMrKbHeld(CAT_KB_BandboxGuard))
    {
        c = CAT_MR_BAND_GUARD;
    }
    else if (catMrKbHeld(CAT_KB_BandboxFocus))
    {
        c = CAT_MR_BAND_FOCUS;
    }
    else if (catMrKbHeld(CAT_KB_BandboxAttack))
    {
        c = CAT_MR_BAND_ATTACK;
    }
    else if (catMrKbHeld(CAT_KB_BandboxForceAttack))
    {
        c = CAT_MR_BAND_FORCEATTACK;
    }
    else if (catMrKbHeld(CAT_KB_ShipSpecial))
    {
        c = CAT_MR_BAND_SPECIAL;
    }
    else
    {
        c = CAT_MR_BAND_SELECT;
    }
    // CAT: the exe's own rectangle (not catMrBandRect's order of tests, the same box)
    rect.x1 = mouseCursorX();
    rect.x0 = mrOldMouseX;
    if (mouseCursorX() < mrOldMouseX)
    {
        rect.x1 = mrOldMouseX;
        rect.x0 = mouseCursorX();
    }
    rect.y1 = mouseCursorY();
    rect.y0 = mrOldMouseY;
    if (mouseCursorY() < mrOldMouseY)
    {
        rect.y1 = mrOldMouseY;
        rect.y0 = mouseCursorY();
    }
    rect.x1++;
    rect.y1++;
    if ((mrFastMoveShipClickX < rect.x1 - rect.x0) || (mrFastMoveShipClickY < rect.y1 - rect.y0))
    {
        if (sensors == 0)
        {
            selSelectingDraw();
        }
        primRectOutline2(&rect, 1, c);
    }
    else if ((sensors == 0) &&
             ((ship = selSelectionClick(universe.RenderList.head, &universe.mainCameraCommand.actualcamera, mouseCursorX(),
                                        mouseCursorY(), TRUE, TRUE, FALSE)) != NULL) &&
             (cat_moMouseOverObjective() == 0))
    {
        cat_selShipScreenRectDraw(ship, c);
    }
}

/*-----------------------------------------------------------------------------
    Callback functions for right-click menus:
-----------------------------------------------------------------------------*/
void mrDockingOrders(Ship *gate)
{
    MaxSelection selection;
    sdword index;

    // CAT: mrDockingOrders 0x00567750 is Cataclysm's slipgate order (the HW1 menu callback is gone): the ships that
    //      may enter the gate dock with it (purpose 10); the others are told why
    cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
    cat_selMakeShipsDockCapableFor((SelectCommand *)&selection, gate->shiptype, TRUE);
    if (selection.numShips != 0)
    {
        clWrapDock(&universe.mainCommandLayer, (SelectCommand *)&selection, CAT_DOCK_SLIPGATE, gate);
    }
    if (selection.numShips != selSelected.numShips)
    {
        if (singlePlayerGame && (singlePlayerGameInfo.currentMission == 15))
        {
            clCommandMessage(strGetString(strNotAllShipsCanEnterSlipgateBecauseOfMission15), 0xff0000ff);
            return;
        }
        for (index = 0; index < selSelected.numShips; index++)
        {
            if (!cat_shipClassCanUseSlipgate(selSelected.ShipPtr[index]->staticinfo->shipclass, gate->shiptype))
            {
                clCommandMessage(strGetString(strNotAllShipsCanEnterSlipgate), 0xff0000ff);
                return;
            }
        }
        clCommandMessage(strGetString(strNotAllShipsCanEnterSlipgateBusy), 0xff0000ff);
    }
}


void mrDeltaFormation(char *string, featom *atom)
{
    if ((tutorial==TUTORIAL_ONLY) && !tutEnable.bContextFormDelta)
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    mrSetTheFormation(DELTA_FORMATION, TRUE, FALSE);
}


void mrBroadFormation(char *string, featom *atom)
{
    if ((playPackets) || (universePause && !opPauseOrders) || (mrDisabled))
    {
        return;
    }

    if ((tutorial==TUTORIAL_ONLY) && !tutEnable.bContextFormBroad)
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    mrSetTheFormation(BROAD_FORMATION, TRUE, FALSE);
}


void mrXFormation(char *string, featom *atom)
{
    if ((playPackets) || (universePause && !opPauseOrders) || (mrDisabled))
    {
        return;
    }

    if ((tutorial==TUTORIAL_ONLY) && !tutEnable.bContextFormX)
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    mrSetTheFormation(DELTA3D_FORMATION, TRUE, FALSE);
}


void mrClawFormation(char *string, featom *atom)
{
    if ((playPackets) || (universePause && !opPauseOrders) || (mrDisabled))
    {
        return;
    }

    if ((tutorial==TUTORIAL_ONLY) && !tutEnable.bContextFormClaw)
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    mrSetTheFormation(CLAW_FORMATION, TRUE, FALSE);
}


void mrWallFormation(char *string, featom *atom)
{
    if ((playPackets) || (universePause && !opPauseOrders) || (mrDisabled))
    {
        return;
    }

    if ((tutorial==TUTORIAL_ONLY) && !tutEnable.bContextFormWall)
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    mrSetTheFormation(WALL_FORMATION, TRUE, FALSE);
}


void mrSphereFormation(char *string, featom *atom)
{
    if ((playPackets) || (universePause && !opPauseOrders) || (mrDisabled))
    {
        return;
    }

    if ((tutorial==TUTORIAL_ONLY) && !tutEnable.bContextFormSphere)
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    mrSetTheFormation(SPHERE_FORMATION, TRUE, FALSE);
}


void mrCustomFormation(char *string, featom *atom)
{
    if ((playPackets) || (universePause && !opPauseOrders) || (mrDisabled))
    {
        return;
    }

    if ((tutorial==TUTORIAL_ONLY) && !tutEnable.bContextFormCustom)
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    mrSetTheFormation(CUSTOM_FORMATION, TRUE, FALSE);
}


void mrHarvestResources(char *string, featom *atom)
{
    MaxSelection workers;
    SelectCommand single;
    Resource *resource = (Resource *)string;
    bool32 first;
    sdword index;

    // CAT: mrHarvestResources 0x00563cc0 takes the resource to harvest in place of the menu string (the popup menu,
    //      the right-click order and the Harvest key pass it, NULL for the nearest one per worker); a hidden mimic
    //      posing as a resource gives its fake asteroid
    (void)atom;
    if (!CAT_MR_ORDERS_ALLOWED())
    {
        return;
    }
    if (!catMrTutEnabled(0x4000))
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    tutGameMessage("KB_Harvest");
    if (selSelected.numShips <= 0)
    {
        return;
    }
    MakeShipsHarvestCapable((SelectCommand *)&workers, (SelectCommand *)&selSelected, TRUE);
    if (workers.numShips <= 0)
    {
        return;
    }
    if (resource == NULL)
    {                                                       // each worker to its nearest resource
        first = TRUE;
        for (index = 0; index < workers.numShips; index++)
        {
            single.numShips = 1;
            single.ShipPtr[0] = workers.ShipPtr[index];
            cat_clHarvestNearestResource(&universe.mainCommandLayer, &single, TRUE, first);
            first = FALSE;
        }
        return;
    }
    if ((((SpaceObj *)resource)->header.objtype == OBJ_ShipType) &&
        cat_mimicAppearsAsResource((Ship *)resource, universe.curPlayerPtr))
    {
        resource = cat_mimicGetFakeResource((Ship *)resource);
    }
    speechEventQueue(workers.ShipPtr[0], 0x40008a3, workers.ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);
    clWrapCollectResource(&universe.mainCommandLayer, (SelectCommand *)&workers, resource);
    soundEventPlay(workers.ShipPtr[0], 0x400, NULL);
    tutGameMessage("Game_ClickHarvest");
}


void mrBuildShips(char *string, featom *atom)
{
    Ship *ship;

    // CAT: mrBuildShips 0x005649b0: a plain pause test that returns (GoK's lost its return), no clearing of the
    //      atom's region; the one selected ship builds only when it is a Sect or Beast mothership or carrier, not
    //      being built itself and not disabled
    if ((playPackets) || (universePause) || (mrDisabled))
    {
        return;
    }

    if ((tutorial==TUTORIAL_ONLY) && !catMrTutEnabled(0x4))
    {
        mrMenuDontDisappear = TRUE;
        return;
    }

    soundEvent(NULL, UI_Click);                    // CAT: the exe's id is HW1's 0x108 (mrBuildShips 0x005649fd)
    ship = selSelected.ShipPtr[0];
    if ((selSelected.numShips == 1) && !bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt) &&
        !ship->shipOrDerelict.catDisabled &&
        ((ship->shiptype == sMothership) || (ship->shiptype == sCarrier) || (ship->shiptype == bMothership) ||
         (ship->shiptype == bCarrier)))
    {
        cmConstructionBegin(ghMainRegion,(smemsize)ship, 0, 0);
    }
    else
    {
        cmConstructionBegin(ghMainRegion, 0, 0, 0);
    }
}


void mrTradeStuff(char *string, featom *atom)
{
    tmTradeBegin(ghMainRegion, 0, 0, 0);

#if MR_VERBOSE_LEVEL >= 1
    dbgMessage("Trading.");
#endif
}

//nn: this function isn't used when your in the sensors manager (smDispatch is used there).
void mrMoveShips(char *string, featom *atom)
{
    // CAT: Cataclysm's Move command (0x0056a090, cat_mrMoveShips); HW1's movement start is gone
    (void)string;
    (void)atom;
    cat_mrMoveShips();
}


void mrInfo(char *string, featom *atom)
{
    dbgMessagef("Info - stubbed out.");
}


void mrCancel(char *string, featom *atom)
{
    if ((playPackets) || (universePause && !opPauseOrders) || (mrDisabled))
    {
//        return;
    }

    if ((tutorial==TUTORIAL_ONLY) && !tutEnable.bCancelCommand)
    {
        mrMenuDontDisappear = TRUE;
        return;
    }

    if (selSelected.numShips > 0)
    {
        clWrapHalt(&universe.mainCommandLayer,(SelectCommand *)&selSelected);
    }
#if MR_VERBOSE_LEVEL >= 1
    dbgMessage("All stop - cancel orders.");
#endif
}


void mrScuttle(char *string, featom *atom)
{
    static real32 confirmTime = -7.0f;                      // 0x008ba398

    (void)string;
    (void)atom;
    // CAT: mrScuttle 0x00565210: the scuttleable (cat_selMakeShipsScuttleable) and retireable ships; the two presses
    //      within the reconfirm time are on the real-time clock, with the Cataclysm battle chatter and fleet speech
    if (!CAT_MR_ORDERS_ALLOWED())
    {
        return;
    }
    if (!catMrTutEnabled(0x10000))
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    if (selSelected.numShips == 0)
    {
        return;
    }
    if (!singlePlayerGame && bitTest(tpGameCreated.flag, MG_CaptureCapitalShip))
    {
        MakeShipsNonCapital((SelectCommand *)&selSelected);
        if (selSelected.numShips < 1)
        {                                                   // CAT: the exe chatters when nothing is left
            if (battleCanChatterAtThisTime(BCE_CannotComply, selSelected.ShipPtr[0]))
            {
                battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_CannotComply, selSelected.ShipPtr[0], SOUND_EVENT_DEFAULT);
            }
            return;
        }
    }
    cat_selMakeShipsScuttleable((SelectCommand *)&selSelected, TRUE);
    makeShipsRetireable((SelectCommand *)&selSelected);
    if (selSelected.numShips <= 0)
    {
        return;
    }
    if ((universe.realtimeelapsed - confirmTime) < TW_ScuttleReconfirmTime)
    {
        tutGameMessage("Game_ScuttleConfirm");
        soundEventPlay(NULL, 0x108, NULL);
        if (battleCanChatterAtThisTime(0x55, selSelected.ShipPtr[0]))
        {
            battleChatterAttempt(SOUND_EVENT_DEFAULT, 0x55, selSelected.ShipPtr[0], selSelected.ShipPtr[0]->shiptype);
        }
        confirmTime = -TW_SCUTTLE_CONFIRM_TIME;
        clWrapScuttle(&universe.mainCommandLayer, (SelectCommand *)&selSelected);
        return;
    }
    speechEventQueue(NULL, 0x4002088, -1, -1, 0, selSelected.ShipPtr[0]->playerowner->playerIndex, -1, 5.0f, -1);
    tutGameMessage("Game_ScuttleRequest");
    confirmTime = universe.realtimeelapsed;
}


void mrRetire(char *string, featom *atom)
{
    (void)string;
    (void)atom;
    // CAT: mrRetire 0x005653b0: the guard-capable, retireable ships dock to retire (purpose 7)
    if (!CAT_MR_ORDERS_ALLOWED())
    {
        return;
    }
    if (!catMrTutEnabled(0x20000))
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    if (selSelected.numShips == 0)
    {
        return;
    }
    if (!singlePlayerGame && bitTest(tpGameCreated.flag, MG_CaptureCapitalShip))
    {
        MakeShipsNonCapital((SelectCommand *)&selSelected);
        if (selSelected.numShips > 0)
        {
            if (battleCanChatterAtThisTime(BCE_CannotComply, selSelected.ShipPtr[0]))
            {
                battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_CannotComply, selSelected.ShipPtr[0], SOUND_EVENT_DEFAULT);
            }
            return;
        }
    }
    MakeShipsGuardCapable((SelectCommand *)&selSelected, TRUE);
    makeShipsRetireable((SelectCommand *)&selSelected);
    if (selSelected.numShips > 0)
    {
        soundEventPlay(NULL, 0x108, NULL);
        clWrapDock(&universe.mainCommandLayer, (SelectCommand *)&selSelected, CAT_DOCK_RETIRE, NULL);
    }
    tutGameMessage("KB_Retire");
}


void mrUpdateHyperspaceStatus(bool32 goForLaunch)
{
}


void mrHyperspace(char *string, featom *atom)
{
    extern bool32 uicButtonReleased;
    if (FEFIRSTCALL(atom))
    {
        //...
    }
    else if (FELASTCALL(atom) && !uicButtonReleased)
    {
        //...
    }
    else
    {
        if (singlePlayerGame)
        {
            spHyperspaceButtonPushed();
        }
    }
}


void mrLaunch(char *string, featom *atom)
{
    // CAT: mrLaunch 0x005654b0: a plain pause test (no opPauseOrders) and no clearing of the atom's region
    if ((playPackets) || (universePause) || (mrDisabled))
    {
        return;
    }

    soundEvent(NULL, UI_Click);                    // CAT: the exe's id is HW1's 0x108 (mrLaunch 0x005654d4)

    if (selSelected.numShips == 1)
    {
        lmLaunchBegin(ghMainRegion,(smemsize)selSelected.ShipPtr[0], 0, 0);
    }
    else
    {
        lmLaunchBegin(ghMainRegion, 0, 0, 0);
    }

    dbgMessagef("Launch Ships.");
}
void mrResearch(char *string, featom *atom)
{
    // CAT: mrResearch 0x00565530: a plain pause test (no opPauseOrders) and no clearing of the atom's region
    if ((playPackets) || (universePause) || (mrDisabled))
    {
        return;
    }

    soundEvent(NULL, UI_Click);                    // CAT: the exe's id is HW1's 0x108 (mrResearch 0x00565554)

    rmResearchGUIBegin(ghMainRegion, 0, 0, 0);

#if MR_VERBOSE_LEVEL >= 1
    dbgMessage("Research.");
#endif
}
void mrEvasiveTactics(char *string, featom *atom)
{
    if ((playPackets) || (universePause && !opPauseOrders) || (mrDisabled))
    {
        return;
    }

    if ((tutorial==TUTORIAL_ONLY) && !tutEnable.bEvasive)
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    if (selSelected.numShips > 0)
    {
        speechEvent(selSelected.ShipPtr[0], COMM_TacticsEvasive, 0);
    }
    clWrapSetTactics(&universe.mainCommandLayer,(SelectCommand *)&selSelected,Evasive);

    dbgMessagef("Evasive Tactics - stubbed out.");
}
void mrNeutralTactics(char *string, featom *atom)
{
    if ((playPackets) || (universePause && !opPauseOrders) || (mrDisabled))
    {
        return;
    }

    if ((tutorial==TUTORIAL_ONLY) && !tutEnable.bNeutral)
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    if (selSelected.numShips > 0)
    {
        speechEvent(selSelected.ShipPtr[0], COMM_TacticsNeutral, 0);
    }
    clWrapSetTactics(&universe.mainCommandLayer,(SelectCommand *)&selSelected,Neutral);

    dbgMessagef("Neutral Tactics - stubbed out.");
}
void mrAgressiveTactics(char *string, featom *atom)
{
    if ((playPackets) || (universePause && !opPauseOrders) || (mrDisabled))
    {
        return;
    }

    if ((tutorial==TUTORIAL_ONLY) && !tutEnable.bAgressive)
    {
        mrMenuDontDisappear = TRUE;
        return;
    }
    if (selSelected.numShips > 0)
    {
        speechEvent(selSelected.ShipPtr[0], COMM_TacticsAggressive, 0);
    }
    clWrapSetTactics(&universe.mainCommandLayer,(SelectCommand *)&selSelected,Aggressive);

    dbgMessagef("Agressive Tactics - stubbed out.");
}

void mrFormAlliance(char *string, featom *atom)
{
    if (playerClickedOn!=-1)
    {
        allianceFormWith(playerClickedOn);
    }
}

void mrBreakAlliance(char *string, featom *atom)
{
    if (playerClickedOn!=-1)
    {
        allianceBreakWith(playerClickedOn);
    }
}

void mrTransferRUS(char *string, featom *atom)
{
    if (playerClickedOn!=-1)
    {
        gcRUTransferStart((uword)playerClickedOn);
    }
}

/*-----------------------------------------------------------------------------
    Name        : mrSelectRectBuild
    Description : builds a selection rect based upon mouse coords
    Inputs      : dest - where to put the result
                  anchorX, anchorY - coordinates of the 'anchor' point of
                    the selection rect
    Outputs     : fills in mrSelectionRect
    Return      : void
----------------------------------------------------------------------------*/
void mrSelectRectBuild(rectangle *dest, sdword anchorX, sdword anchorY)
{
    dest->x0 = anchorX;                                     //build selection rect
    dest->y0 = anchorY;
    dest->x1 = mouseCursorX();
    dest->y1 = mouseCursorY();
    if (dest->x0 > dest->x1)                                //make sure first corner above and left of second corner
    {
        swapInt(dest->x0, dest->x1);
    }
    if (dest->y0 > dest->y1)
    {
        swapInt(dest->y0, dest->y1);
    }
    dest->x1++;                                             //make sure rect draws inclusively
    dest->y1++;                                             // to lower right corner
}

/*-----------------------------------------------------------------------------
    Name        : mrSelectionUndo
    Description : Undo callback to reset selection
    Inputs      : length, userData - MaxSelection substructure
    Outputs     : Copies a selection of length userID to selSelected
    Return      : TRUE if successful
----------------------------------------------------------------------------*/
/*
bool32 mrSelectionUndo(sdword userID, ubyte *userData, sdword length)
{
    selSelectionCopy((MaxAnySelection *)&selSelected, (MaxAnySelection *)userData);
    ioUpdateShipTotals();
    return TRUE;
}
*/
/*-----------------------------------------------------------------------------
    Name        : mrHotKeyAssignUndo
    Description : Undo callback to reset selection
    Inputs      : length, userData - MaxSelection substructure
                  userID - hot key #
    Outputs     : Copies a selection of length userID to selSelected
    Return      :
----------------------------------------------------------------------------*/
/*
bool32 mrHotKeyAssignUndo(sdword userID, ubyte *userData, sdword length)
{
    selHotKeyNumbersSet(&selHotKeyGroup[userID], SEL_InvalidHotKey);
    selSelectionCopy((MaxAnySelection *)&selHotKeyGroup[userID], (MaxAnySelection *)userData);
    selHotKeyNumbersSet(&selHotKeyGroup[userID], selHotKeyNumber(userID));
    return TRUE;
}
*/
/*-----------------------------------------------------------------------------
    Name        : mrCanZBandBox
    Description : Determine if we can z-bandbox right now
    Inputs      : bFriendlies - we can bandbox friendlies
                  bEnemies - we can bandbox enemies
    Outputs     :
    Return      : we can bandbox at all
----------------------------------------------------------------------------*/
bool32 mrCanZBandBox(bool32 *bFriendlies, bool32 *bEnemies)
{
    sdword index ;
    ShipStaticInfo *shipstatic;
    *bEnemies = *bFriendlies = FALSE;
    if (selSelected.numShips == 0)
    {
        return(FALSE);
    }
    for (index = 0; index < selSelected.numShips; index++)
    {
        shipstatic = (ShipStaticInfo *)selSelected.ShipPtr[index]->staticinfo;
        if (shipstatic->custshipheader.CustShipSpecialTarget != NULL)
        {
            if (shipstatic->canSpecialBandBoxFriendlies)
            {
                *bFriendlies = TRUE;
                return(TRUE);
            }
            else
            {
                *bEnemies = TRUE;
                return(TRUE);
            }
        }
    }
    return(*bFriendlies || *bEnemies);
}

/*-----------------------------------------------------------------------------
    Name        : various
    Description : Main region processor functions, called by function pointer
    Inputs      : void
    Outputs     : various
    Return      : void
----------------------------------------------------------------------------*/
//do nothing
void mrNULL(void)
{
    ;                                                       //do nothing
}
//drag the selection box
void mrSelectHold(Camera *camera, sdword sensors)
{
    MaxSelection selection;
    rectangle rect;
    Node *list;
    bool32 anyRace = FALSE, enemies = FALSE, friends = FALSE;   // the special kinds: anything, others, friends
    sdword index;
    ShipStaticInfo *staticinfo;

    // CAT: mrSelectHold 0x0056a1e0, Cataclysm's: the band box picks into selSelecting what the modifier key's order
    //      may take (force attack, guard, the special order's targets by the selection's special kinds, focus,
    //      attack), the player's ships without a modifier; the multiple-select key adds own ships
    if (universePause || mrDisabled)
    {
        selSelecting.numTargets = 0;
    }
    sensors = (sensors == 1);
    if (mrDisabled)
    {
        return;
    }
    if ((abs(mouseCursorX() - mrOldMouseX) < selClickBoxWidth) && (abs(mouseCursorY() - mrOldMouseY) < selClickBoxHeight))
    {
        selSelecting.numTargets = 0;
        return;
    }
    catMrBandRect(&rect);
    list = sensors ? universe.SpaceObjList.head : universe.RenderList.head;
    if (catMrKbHeld(CAT_KB_BandboxForceAttack))
    {
        if (playPackets)
        {
            return;
        }
        cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
        MakeShipsAttackCapable((SelectCommand *)&selection, (SelectCommand *)&selSelected, TRUE);
        if (selection.numShips < 1)
        {
            selSelecting.numTargets = 0;
            return;
        }
        selRectDragFunction(list, &universe.mainCameraCommand.actualcamera, &rect, selSelecting.TargetPtr,
                            &selSelecting.numTargets, FALSE, TRUE, TRUE, sensors);
        return;
    }
    if (catMrKbHeld(CAT_KB_MultipleSelect))
    {
        selRectDragFunction(list, &universe.mainCameraCommand.actualcamera, &rect, selSelecting.TargetPtr,
                            &selSelecting.numTargets, TRUE, FALSE, FALSE, sensors);
        return;
    }
    if (catMrKbHeld(CAT_KB_BandboxGuard))
    {
        if (playPackets)
        {
            return;
        }
        cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
        cat_selMakeShipsProtectCapable((SelectCommand *)&selection, TRUE);
        if (selection.numShips < 1)
        {
            selSelecting.numTargets = 0;
            return;
        }
        selRectDragFunction(list, camera, &rect, selSelecting.TargetPtr, &selSelecting.numTargets, FALSE, FALSE, FALSE,
                            sensors);
        MakeShipsFriendlyAndAlliesShips((SelectCommand *)&selSelecting, universe.curPlayerPtr);
        MakeShipsNotIncludeTheseShips((SelectCommand *)&selSelecting, (SelectCommand *)&selSelected);
        return;
    }
    if (catMrKbHeld(CAT_KB_ShipSpecial))
    {
        if (playPackets)
        {
            return;
        }
        if (selSelected.numShips < 1)
        {
            selSelecting.numTargets = 0;
            return;
        }
        for (index = 0; index < selSelected.numShips; index++)
        {                                                   // what the selection's special orders target
            staticinfo = selSelected.ShipPtr[index]->staticinfo;
            if (staticinfo->custshipheader.CustShipSpecialTarget != NULL)
            {
                if (!staticinfo->canSpecialBandBoxFriendlies || staticinfo->unknownFlag212)
                {
                    enemies = TRUE;
                }
                if (staticinfo->canSpecialBandBoxFriendlies)
                {
                    friends = TRUE;
                }
                if (staticinfo->unknownFlag213)
                {
                    anyRace = TRUE;
                }
            }
        }
        if (!friends && !enemies && !anyRace)
        {
            selSelecting.numTargets = 0;
            return;
        }
        if (anyRace)
        {                                                   // anything at all
            selRectDragFunction(list, &universe.mainCameraCommand.actualcamera, &rect, selSelecting.TargetPtr,
                                &selSelecting.numTargets, FALSE, TRUE, FALSE, sensors);
        }
        else if (enemies)
        {                                                   // any player's ships
            selRectDragFunction(list, &universe.mainCameraCommand.actualcamera, &rect, selSelecting.TargetPtr,
                                &selSelecting.numTargets, FALSE, FALSE, FALSE, sensors);
        }
        else
        {                                                   // the player's ships
            selRectDragFunction(list, &universe.mainCameraCommand.actualcamera, &rect, selSelecting.TargetPtr,
                                &selSelecting.numTargets, TRUE, FALSE, FALSE, sensors);
        }
        if (!friends)
        {
            mrRemoveDerelictsAndAllPlayerShipsFromSelecting();
        }
        return;
    }
    if (catMrKbHeld(CAT_KB_BandboxFocus))
    {
        selRectDragFunction(list, &universe.mainCameraCommand.actualcamera, &rect, selSelecting.TargetPtr,
                            &selSelecting.numTargets, FALSE, FALSE, FALSE, sensors);
        return;
    }
    if (catMrKbHeld(CAT_KB_BandboxAttack))
    {
        if (playPackets)
        {
            return;
        }
        cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
        MakeShipsAttackCapable((SelectCommand *)&selection, (SelectCommand *)&selSelected, TRUE);
        if (selection.numShips < 1)
        {
            selSelecting.numTargets = 0;
            return;
        }
        selRectDragFunction(list, &universe.mainCameraCommand.actualcamera, &rect, selSelecting.TargetPtr,
                            &selSelecting.numTargets, FALSE, TRUE, TRUE, sensors);
        MakeTargetsOnlyNonForceAttackTargets((SelectAnyCommand *)&selSelecting, universe.curPlayerPtr);
        return;
    }
    selRectDragFunction(list, &universe.mainCameraCommand.actualcamera, &rect, selSelecting.TargetPtr,
                        &selSelecting.numTargets, TRUE, FALSE, FALSE, sensors);
}
//rotate and/or zoom the camera
//nn: mouse wheel zoom doesn't call this code (at least in game and sensors manager)
void mrCameraMotion(void)
{
    rectangle rect;
    sdword cx, cy;
    if (helpinfoactive)
    {
        if (!mouseInRect(&helpinforegion->rect))
        {

            cx = helpinforegion->rect.x0 / 2;
            cy = MAIN_WindowHeight / 2;

            if (mouseCursorX() != cx ||
            mouseCursorY() != cy)
            {
                mrMouseHasMoved += abs(mouseCursorX() - cx);
                mrMouseHasMoved += abs(mouseCursorY() - cy);
                camMouseX = cx - mouseCursorX();
                camMouseY = cy - mouseCursorY();
                mousePositionSet(cx, cy);

                rect.x0 = 0;
                rect.y0 = 0;
                rect.x1 = helpinforegion->rect.x0;
                rect.y1 = MAIN_WindowHeight;
                mouseClipToRect(&rect);
            }
        }
    }
    else
    {
        if (mouseCursorX() != MAIN_WindowWidth / 2 ||
        mouseCursorY() != MAIN_WindowHeight / 2)
        {
            mrMouseHasMoved += abs(mouseCursorX() - MAIN_WindowWidth / 2);
            mrMouseHasMoved += abs(mouseCursorY() - MAIN_WindowHeight / 2);
            camMouseX = MAIN_WindowWidth / 2 - mouseCursorX();
            camMouseY = MAIN_WindowHeight / 2 - mouseCursorY();
            mousePositionSet(MAIN_WindowWidth / 2, MAIN_WindowHeight / 2);
        }
    }
}

// CAT (M2-R6 startup-keys): HW1's mrKeyRelease (the shift / Z / zoom key releases) is gone: the exe's mrRegionProcess
//      sends the key releases to cat_kbKeyRelease 0x0054e8c0 (the second callbacks: Multiple_Select,
//      Enable_Height_Move, Ship_Special); the camera polls its zoom keys (kbCommandKeyIsHit, Camera.c).

#if MR_KEYBOARD_CHEATS
void mrScanDebugCodes(sdword ID);
#endif

bool32 NoModifierKeyPressed(void)
{
    return !(keyIsHit(SHIFTKEY) || keyIsHit(CONTROLKEY) || keyIsHit(ALTKEY) || keyIsHit(RALTKEY));
}

void mrSetTheFormation(TypeOfFormation formationtype, bool32 showMessage, bool32 shipVoice)
{
    // CAT: mrSetTheFormation 0x005659d0: formation 8 is the parade (a mothership or carrier needed), else the
    //      formation with the fleet voice (or the second ship's voice, shipVoice) and the message (showMessage); the
    //      HW1 formation name on the screen is gone
    if (!CAT_MR_ORDERS_ALLOWED() || !catMrTutEnabled(0x80))
    {
        return;
    }
    makeShipsFormationCapable((SelectCommand *)&selSelected);
    if (selSelected.numShips <= 0)
    {
        return;
    }
    if (formationtype == SAME_FORMATION)
    {
        if (MothershipOrCarrierIndexInSelection((SelectCommand *)&selSelected) == -1)
        {
            clCommandMessage(strGetString(str_GM_NeedMothershipOrCarrierForParadeFormation), 0xffffffff);
            return;
        }
        soundEventPlay(NULL, 0x108, NULL);
        speechEventQueue(NULL, 0x4001062, SAME_FORMATION, -1, 0, selSelected.ShipPtr[0]->playerowner->playerIndex, -1,
                         5.0f, -1);
        clWrapSetMilitaryParade(&universe.mainCommandLayer, (SelectCommand *)&selSelected);
        tutGameMessage("Game_Formation");
        if (showMessage)
        {
            clCommandMessage(strGetString(str_GM_ChangeToMilitaryParade), 0xffffffff);
        }
        return;
    }
    soundEventPlay(NULL, 0x108, NULL);
    if (!shipVoice)
    {
        speechEventQueue(NULL, 0x4001062, (formationtype == CUSTOM_FORMATION) ? NO_FORMATION : formationtype, -1, 0,
                         selSelected.ShipPtr[0]->playerowner->playerIndex, -1, 5.0f, -1);
    }
    else
    {
        speechEventQueue(selSelected.ShipPtr[1], 0x4000890, selSelected.ShipPtr[1]->shiptype, -1, -1, -1, -1, 0.0f, -1);
    }
    clWrapFormation(&universe.mainCommandLayer, (SelectCommand *)&selSelected, formationtype, 0);
    if (formationtype == DELTA_FORMATION)
    {
        tutGameMessage("Delta_Formation");
    }
    if (showMessage && (formationtype <= CUSTOM_FORMATION))
    {
        clCommandMessage(strGetString(str_GM_ChangeToDeltaFormation + formationtype), 0xffffffff);
    }
}

/*-----------------------------------------------------------------------------
    Name        : mrMothershipInFocus
    Description : Return TRUE if the specified mothership-class ship is in focus.
    Inputs      : mothership - the specified mothership
    Outputs     :
    Return      : TRUE if motherhip in focus, FALSE otherwise.
----------------------------------------------------------------------------*/
#define MR_ONLY_MOTHERSHIP          1                       //1=focus only on mothership, 0 = focus on mothership along with other ships
// CAT: the exe's eight types (mrMothershipShipTypes 0x008ba39c, memchr(..., 8) in mrNextMothershipPtr 0x00565da0):
//      the four carriers, both motherships, p1Mothership and the floating city (HW1: Carrier, Mothership,
//      P1Mothership, P2Mothership, P3Megaship, FloatingCity)
#define MR_NumberMothershipTypes    8
ubyte mrMothershipShipTypes[MR_NumberMothershipTypes] = {kCarrier, tCarrier, sCarrier, bCarrier, sMothership, bMothership,
                                                         p1Mothership, trFloatingCity};
Ship *mrMothershipPtr = NULL;
bool32 mrMothershipInFocus(Ship *mothership)
{
    FocusCommand *focus;

    dbgAssertOrIgnore(memchr(mrMothershipShipTypes, mothership->shiptype, MR_NumberMothershipTypes));
    focus = &universe.mainCameraCommand.currentCameraStack->focus;
#if MR_ONLY_MOTHERSHIP
    if (focus->numShips == 1 && focus->ShipPtr[0] == mothership)
    {                                                       //if focussed only on the mothership
        return(TRUE);
    }
    return(FALSE);
#else
    return(selShipInSelection(focus->ShipPtr, focus->numShips, mothership));
#endif
}

/*-----------------------------------------------------------------------------
    Name        : mrNextMothershipPtr
    Description : Return a pointer to the next mothership if the player has more
                    than one mothership.
    Inputs      : mothership - the currently focused mothership
    Outputs     :
    Return      : Next player mothership in shiplist (may be NULL or the same as mothership)
----------------------------------------------------------------------------*/
Ship *mrNextMothershipPtr(Ship *mothership)
{
    Node *shipNode;
    Ship *ship, *startingShip;
    bool32 bFirstShip = TRUE;
    sdword count = 0;

    if (mothership == NULL)
    {                                                       //mothership not defined yet
        if (universe.curPlayerPtr->PlayerMothership != NULL)
        {                                                   //get link from player's mothership
            shipNode = &universe.curPlayerPtr->PlayerMothership->shiplink;
        }
        else
        {                                                   //get link from start of shiplist
            shipNode = universe.ShipList.head;
        }
        if (shipNode == NULL) return NULL;
        startingShip = (Ship *)listGetStructOfNode(shipNode);
        if (startingShip == NULL) return NULL;      // believe it or not this check is required or it will crash under certain conditions
    }
    else
    {                                                       //get link from specified mothership
        shipNode = mothership->shiplink.next;
        if (shipNode == NULL)
        {
            shipNode = universe.ShipList.head;
        }
        if (shipNode == NULL) return NULL;
        startingShip = mothership;
    }

    while (1)
    {
        if (shipNode == NULL) return NULL;
        ship = listGetStructOfNode(shipNode);
        if (ship == NULL) return NULL;          // believe it or not this check is required or it will crash under certain conditions
        if (ship == startingShip && (!bFirstShip))
        {                                                   //if we've come all the way back to the player's mothership
            return(NULL);
        }
        // CAT: the exe's eight mothership types and a test that the ship is not disabled (mrNextMothershipPtr
        //      0x00565da0)
        if (ship->playerowner == universe.curPlayerPtr &&
            memchr(mrMothershipShipTypes, ship->shiptype, MR_NumberMothershipTypes) &&
            bitTest(ship->header.flags, SOF_Selectable) && !ship->shipOrDerelict.catDisabled)
        {                                                   //if it's the player's ship and it's a mothership type
            break;                                          //use this one as the mothership
        }
        shipNode = shipNode->next;
        if (shipNode == NULL)
        {                                                   //if reached end of shiplist
            shipNode = universe.ShipList.head;              //go back to start of shiplist
        }
        if (count >= universe.ShipList.num)
        {                                                   //if we've already looked at every ship in list
            return(NULL);
        }
        count++;
        if (shipNode == NULL) return NULL;
        bFirstShip = FALSE;
    }
    return(ship);
}

// CAT (M2-R6 startup-keys): HW1's mrKeyPress (the key switch with the hot-key groups, the formation, tactics, camera
//      and debug keys) is gone: the exe has no such function; its mrRegionProcess 0x00567930 sends the main region's key
//      presses to the key bindings (cat_kbKeyPress 0x0054e7c0, KeyBindings.c), whose commands do that work.

// CAT: HW1's right-click menu (the action masks, mrMenuActionsByShipType, mrMenuDisplay, mrRightClickMenu and the player
//      list menus) is gone: Cataclysm's right click is cat_mrReleaseRight 0x0056b0b0 with the popup menu (PopupMenu.c)

#if MR_TEST_HPB
#define MR_ROTATE_SCALAR        0.007f
extern sdword madTestHPBIndex;
bool32 mrTestHPBMode = FALSE;
real32 mrHeading, mrPitch, mrBank;
void mrHeadingPitchHold(void)
{
    if (mouseCursorX() != MAIN_WindowWidth / 2)
    {
        mrHeading += (real32)(mouseCursorX() - MAIN_WindowWidth / 2) * MR_ROTATE_SCALAR;
        mousePositionSet(MAIN_WindowWidth / 2, mouseCursorY());
    }
    if (mouseCursorY() != MAIN_WindowHeight / 2)
    {
        mrPitch += (real32)(mouseCursorY() - MAIN_WindowHeight / 2) * MR_ROTATE_SCALAR;
        mousePositionSet(mouseCursorX(), MAIN_WindowHeight / 2);
    }
    mrTestHPBMode = TRUE;
}
void mrBankHold(void)
{
    if (mouseCursorX() != MAIN_WindowWidth / 2)
    {
        mrBank += (real32)(MAIN_WindowWidth / 2 - mouseCursorX()) * MR_ROTATE_SCALAR;
        mousePositionSet(MAIN_WindowWidth / 2, mouseCursorY());
    }
    mrTestHPBMode = TRUE;
}
#endif

// later put in shipselect.c
#define ShipIsntSelectable(x)        bitTest((x)->header.flags, SOF_Hide|SOF_Disabled|SOF_Crazy)

/*-----------------------------------------------------------------------------
    Name        : MakeShipsSelectable
    Description : Makes sure that the ships in this selection are capable of
                  attack.  Removes any ships from this selection that are not
                  capable of attacking.
    Inputs      : selection
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeShipsSelectable(SelectCommand *selection)
{
    Ship *ship;
    sdword i;

    for (i=0;i<selection->numShips;)
    {
        ship = selection->ShipPtr[i];
        if (ShipIsntSelectable(ship) || !(ship->header.flags & SOF_Selectable) || ship->playerowner != universe.curPlayerPtr)
        {
            // to remove ShipPtr from list, decrement numShips and put last member
            // of array into slot being deleted.
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];

            continue; // we must check same index again because we put last array entry here
        }
        i++;
    }
}

/*-----------------------------------------------------------------------------
    Name        : mrFormationShips
    Description : Find the ships for a given mouse click, considering the
                    formation it is in.
    Inputs      : ship - ship clicked on.
    Outputs     : selection - where to store the selection
    Return      : void
----------------------------------------------------------------------------*/
void mrFormationShips(MaxSelection *selection, Ship *ship)
{
    SelectCommand *formSelection;
    MaxSelection selectedInFormation;
    bool32 parade;

    // CAT: mrFormationShips 0x00566a00: the clicked ship's formation (the parade's ships of its type), unless it is
    //      already the selection or the ship is selected and the multiple-select key is held; HW1's single-click
    //      build / research / sensors screens of a selected mothership are gone
    formSelection = getShipAndItsCommandSelection(&universe.mainCommandLayer, ship, &parade);
    MakeShipsSelectable(formSelection);
    if (formSelection->numShips > 1)
    {
        if (!parade)
        {
            selSelectionCopy((MaxAnySelection *)selection, (MaxAnySelection *)formSelection);
            selSelectionCopy((MaxAnySelection *)&selectedInFormation, (MaxAnySelection *)&selSelected);
        }
        else
        {
            selSelectionCopyByType(selection, (MaxSelection *)formSelection, ship->shiptype);
            selSelectionCopyByType(&selectedInFormation, (MaxSelection *)&selSelected, ship->staticinfo->shiptype);
        }
        if (selSelectionCompare((MaxAnySelection *)selection, (MaxAnySelection *)&selectedInFormation) == 0)
        {
            selection->numShips = 0;
        }
        if (selShipInSelection(selSelected.ShipPtr, selSelected.numShips, ship) && (keyIsHit(SHIFTKEY) || keyIsHit(RSHIFTKEY)))
        {
            selection->numShips = 0;
        }
        if (selection->numShips == 0)
        {
            selection->ShipPtr[0] = ship;
            selection->numShips = 1;
        }
        tutGameMessage("Game_ClickSelectGroup");
        memFree(formSelection);
        return;
    }
    selection->ShipPtr[0] = ship;
    selection->numShips = 1;
    memFree(formSelection);
}

/*-----------------------------------------------------------------------------
    Name        : mrObjectClick
    Description : Called when someone clicks on a single ship.  Handles modifiers etc.
    Inputs      : ship - object clicked on (can be a ship, asteroid, dust cloud or derelict)
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void mrObjectClick(Ship *ship, bool32 doubleClick)
{
    CatTargetActions selectionActions, targetActions;
    MaxSelection targets, selection;
    sdword action, ability, index;
    Node *node;
    Ship *found;

    // CAT: mrObjectClick 0x00566b60, Cataclysm's: the modifier keys (force attack, multiple select, guard, the special
    //      popup on the ship, focus, attack) first; then, outside the sensors manager and for a ship not selected, the
    //      context order of the Homeworld CSA option (the right-click order of cat_mrReleaseRight, on a left click,
    //      some only on a double click); else the clicked own ship (its formation) is selected
    if (universePause || mrDisabled)
    {
        return;
    }
    targets.numShips = (ship != NULL) ? 1 : 0;
    targets.ShipPtr[0] = ship;
    if (catMrKbHeld(CAT_KB_BandboxForceAttack))
    {
        if (playPackets || (targets.numShips < 1))
        {
            return;
        }
        MakeShipsAttackCapable((SelectCommand *)&selection, (SelectCommand *)&selSelected, TRUE);
        cat_selSelectionCopy((MaxSelection *)&selSelected, &selection);
        if (selSelected.numShips < 1)
        {
            return;
        }
        cat_selMakeTargetsAttackable((SelectCommand *)&targets, FALSE);
        MakeShipsNotIncludeTheseShips((SelectCommand *)&targets, (SelectCommand *)&selSelected);
        if (targets.numShips < 1)
        {
            return;
        }
        speechEventAttack();
        clWrapAttack(&universe.mainCommandLayer, (SelectCommand *)&selSelected, (AttackCommand *)&targets);
        tutGameMessage("Game_BandBoxForceAttack");
        return;
    }
    if (catMrKbHeld(CAT_KB_MultipleSelect))
    {
        if ((ship->header.objtype == OBJ_ShipType) && (ship->playerowner == universe.curPlayerPtr))
        {
            if (!selShipInSelection(selSelected.ShipPtr, selSelected.numShips, ship))
            {
                selSelectionAddSingleShip((MaxSelection *)&selSelected, ship);
            }
            else
            {
                selSelectionRemoveSingleShip((MaxSelection *)&selSelected, ship);
            }
            ioUpdateShipTotals();
        }
        tutGameMessage("MultipleSelect");
        return;
    }
    if (catMrKbHeld(CAT_KB_BandboxGuard))
    {
        if (playPackets || (targets.numShips < 1))
        {
            return;
        }
        cat_selMakeShipsProtectCapable((SelectCommand *)&selSelected, TRUE);
        if (selSelected.numShips < 1)
        {
            return;
        }
        MakeShipsFriendlyAndAlliesShips((SelectCommand *)&targets, universe.curPlayerPtr);
        MakeShipsNotIncludeTheseShips((SelectCommand *)&targets, (SelectCommand *)&selSelected);
        if (targets.numShips < 1)
        {
            return;
        }
        speechEventQueue(NULL, 0x4001061, -1, -1, 0, selSelected.ShipPtr[0]->playerowner->playerIndex, -1, 5.0f, -1);
        clWrapProtect(&universe.mainCommandLayer, (SelectCommand *)&selSelected, (ProtectCommand *)&targets);
        tutGameMessage("Game_BandBoxGuard");
        return;
    }
    if (catMrKbHeld(CAT_KB_ShipSpecial))
    {
        if (playPackets)
        {
            return;
        }
        MakeShipsSpecialTargetCapable((SelectCommand *)&selSelected, FALSE, TRUE);
        if (selSelected.numShips < 1)
        {
            return;
        }
        selSelecting.numTargets = 1;
        selSelecting.TargetPtr[0] = (SpaceObjRotImpTarg *)ship;
        cat_popupMenuOpen(CAT_PM_TARGET_KEYCOMMAND, (MaxSelection *)&selSelecting);
        tutGameMessage("Game_ClickSpecial");
        return;
    }
    if (catMrKbHeld(CAT_KB_BandboxFocus))
    {
        selSelecting.numTargets = 1;
        selSelecting.TargetPtr[0] = (SpaceObjRotImpTarg *)ship;
        if (smSensorsActive)
        {
            index = 0;
            while (index < selSelecting.numTargets)
            {
                found = (Ship *)selSelecting.TargetPtr[index];
                if ((found->header.objtype == OBJ_ShipType) && !cat_ccShipIsLegalToFocusOn(found))
                {
                    selSelecting.numTargets--;
                    selSelecting.TargetPtr[index] = selSelecting.TargetPtr[selSelecting.numTargets];
                }
                else
                {
                    index++;
                }
            }
            if (selSelecting.numTargets > 0)
            {
                smCullAndFocusSelecting();
            }
            tutGameMessage("AltFocus");
            return;
        }
        ccFocusClose(&universe.mainCameraCommand, (FocusCommand *)&selSelecting);
        selSelecting.numTargets = 0;
        tutGameMessage("Game_BandBoxFocus");
        tutGameMessage("AltFocus");
        return;
    }
    if (catMrKbHeld(CAT_KB_BandboxAttack))
    {
        if (playPackets)
        {
            return;
        }
        if (targets.numShips > 0)
        {
            MakeShipsAttackCapable((SelectCommand *)&selection, (SelectCommand *)&selSelected, TRUE);
            cat_selSelectionCopy((MaxSelection *)&selSelected, &selection);
            if (selSelected.numShips > 0)
            {
                cat_selMakeTargetsAttackable((SelectCommand *)&targets, FALSE);
                MakeShipsNotIncludeTheseShips((SelectCommand *)&targets, (SelectCommand *)&selSelected);
                MakeTargetsOnlyNonForceAttackTargets((SelectAnyCommand *)&targets, universe.curPlayerPtr);
                if (targets.numShips > 0)
                {
                    speechEventAttack();
                    clWrapAttack(&universe.mainCommandLayer, (SelectCommand *)&selSelected, (AttackCommand *)&targets);
                    tutGameMessage("Game_BandBoxAttack");
                }
            }
        }
        tutGameMessage("ClickAttack");
        return;
    }
    if (!smSensorsActive && !playPackets && !ShipInSelection((SelectCommand *)&selSelected, ship))
    {                                                       // the context order on a left click
        cat_selComputeTargetActions(&selectionActions, (MaxSelection *)&selSelected);
        cat_selComputeTargetActions(&targetActions, &targets);
        action = cat_mouseGetContextAction(&selectionActions, &targetActions, (MaxSelection *)&selSelected, ship);
        ability = -1;
        if (singlePlayerGame && (singlePlayerGameInfo.currentMission == 10) && (ship->shiptype == dBigGun) && doubleClick)
        {
            found = NULL;
            for (node = universe.ShipList.head; node != NULL; node = node->next)
            {
                if (((Ship *)listGetStructOfNode(node))->shiptype == dBigGunUnit)
                {
                    found = (Ship *)listGetStructOfNode(node);
                }
            }
            if (found == NULL)
            {
                goto selectIt;
            }
            ability = 0;
            goto specialOrder;
        }
        switch (action)
        {
            case CAT_CA_Harvest:
                if (catMrHomeworldCSA)
                {
                    mrHarvestResources((char *)ship, NULL);
                    soundEventPlay(NULL, 0x109, NULL);
                    return;
                }
                break;
            case CAT_CA_Salvage:
                if (catMrHomeworldCSA)
                {
                    ability = 0;
                    goto specialOrder;
                }
                break;
            case CAT_CA_Attack:
            case CAT_CA_EMP:
            case CAT_CA_Swarm:
            case CAT_CA_FireMissiles:
            case CAT_CA_Infect:
                // CAT: a left click on an enemy attacks it whatever its context (the exe's cases 3, 0xd, 0xe, 0x10 and
                //      0x11 all attack; the special orders are the right click's)
                if (targets.numShips > 0)
                {
                    MakeShipsAttackCapable((SelectCommand *)&selection, (SelectCommand *)&selSelected, TRUE);
                    cat_selSelectionCopy((MaxSelection *)&selSelected, &selection);
                    if (selSelected.numShips > 0)
                    {
                        cat_selMakeTargetsAttackable((SelectCommand *)&targets, FALSE);
                        MakeShipsNotIncludeTheseShips((SelectCommand *)&targets, (SelectCommand *)&selSelected);
                        if (targets.numShips > 0)
                        {
                            speechEventAttack();
                            clWrapAttack(&universe.mainCommandLayer, (SelectCommand *)&selSelected, (AttackCommand *)&targets);
                            tutGameMessage("Game_BandBoxForceAttack");
                        }
                    }
                }
                soundEventPlay(NULL, 0x109, NULL);
                tutGameMessage("ClickAttack");
                return;
            case CAT_CA_Repair:
                if (catMrHomeworldCSA && doubleClick)
                {
                    ability = 1;
                    goto specialOrder;
                }
                break;
            case CAT_CA_ProtectFormation:
                if (catMrHomeworldCSA && doubleClick)
                {
                    cat_uiProtectFormation((SelectCommand *)&targets);
                    soundEventPlay(NULL, 0x109, NULL);
                    return;
                }
                break;
            case CAT_CA_SlipgateSmall:
            case CAT_CA_SlipgateMedium:
            case CAT_CA_SlipgateLarge:
                if (catMrHomeworldCSA)
                {
                    mrDockingOrders(ship);
                    soundEventPlay(NULL, 0x109, NULL);
                    return;
                }
                break;
            case CAT_CA_Dock:
                if (catMrHomeworldCSA && doubleClick)
                {
                    cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
                    MakeShipsGuardCapable((SelectCommand *)&selection, TRUE);
                    clWrapDock(&universe.mainCommandLayer, (SelectCommand *)&selection, CAT_DOCK_SUPPORT, ship);
                    soundEventPlay(NULL, 0x109, NULL);
                    return;
                }
                break;
            case CAT_CA_Leech:
                ability = 2;
                goto specialOrder;
            case CAT_CA_Link:
                if (catMrHomeworldCSA && doubleClick)
                {
                    cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
                    cat_selSelectionAddShip(&selection, ship);
                    if (selection.numShips > 0)
                    {
                        cat_clWrapLink(&universe.mainCommandLayer, (SelectCommand *)&selection);
                    }
                    soundEventPlay(NULL, 0x109, NULL);
                    return;
                }
                break;
            case CAT_CA_Mimic:
                if (catMrHomeworldCSA)
                {
                    cat_mrMimicTarget((SpaceObj *)ship);
                    soundEventPlay(NULL, 0x109, NULL);
                    return;
                }
                break;
            case CAT_CA_Ram:
                ability = 6;
                goto specialOrder;
            default:
                break;
        }
    }
    goto selectIt;
specialOrder:
    if (!playPackets && !universePause && !mrDisabled)
    {
        cat_uiSpecialTargetCommand((SelectCommand *)&targets, ability);
    }
    soundEventPlay(NULL, 0x109, NULL);
    return;
selectIt:
    if ((ship->header.objtype == OBJ_ShipType) && (ship->playerowner == universe.curPlayerPtr))
    {                                                       // an own ship: it (its formation) is the selection
        mrFormationShips(&selection, ship);
        selSelectionCopy((MaxAnySelection *)&selSelected, (MaxAnySelection *)&selection);
        ioUpdateShipTotals();
        if (playPackets)
        {
            return;
        }
        if ((universe.curPlayerPtr->PlayerMothership != NULL) &&
            selShipInSelection(selection.ShipPtr, selection.numShips, universe.curPlayerPtr->PlayerMothership) &&
            (universe.curPlayerPtr->PlayerMothership->staticinfo->shipclass == CLASS_Mothership))
        {
            speechEventQueue(NULL, 0x4000800, universe.curPlayerPtr->PlayerMothership->shiptype, -1, 0,
                             universe.curPlayerIndex, -1, 5.0f, -1);
            return;
        }
        speechEventQueue(ship, 0x4000800, ship->shiptype, -1, -1, -1, -1, 0.0f, -1);
    }
}

/*-----------------------------------------------------------------------------
    Name        : mrRegionProcess
    Description : Region logic handler for main region.
    Inputs      : same as any region processor
    Outputs     : ..
    Return      : same as any region processor
----------------------------------------------------------------------------*/
udword mrRegionProcess(regionhandle reg, sdword ID, udword event, udword data)
{
    (void)reg;
    (void)data;
    // CAT: mrRegionProcess 0x00567930 is REWRITTEN in Cataclysm: the mouse events go to cat_mrPressLeft /
    //      cat_mrReleaseLeft / cat_mrPressRight / cat_mrReleaseRight / cat_mrHoldLeft and the band box, the keys to the
    //      key bindings (cat_kbKeyPress 0x0054e7c0, the popup menu's keys first; cat_kbKeyRelease 0x0054e8c0)
    while (feMenuLevel > 0)
    {
        feMenuDisappear(NULL, NULL);
    }
    if (CAT_MR_CONSOLE_OPEN() || smZoomingIn || smZoomingOut)
    {
        return(0);
    }

    switch (event)
    {
        case RPE_WheelUp:
            wheel_up = TRUE;
            break;
        case RPE_WheelDown:
            wheel_down = TRUE;
            break;
        case RPE_HoldLeft:
            cat_mrHoldLeft(0);
            break;
        case RPE_HoldRight:
            if (catMrRightMode == 2)
            {
                mrCameraMotion();
            }
            break;
        case RPE_PressLeft:
            cat_mrPressLeft();
            break;
        case RPE_PressRight:
            cat_mrPressRight();
            break;
        case RPE_ReleaseLeft:
            cat_mrReleaseLeft(0);
            break;
        case RPE_ReleaseRight:
            cat_mrReleaseRight(0);
            break;
        case RPE_DoubleLeft:
            if (!cat_shipPrintIsDragging() && !cat_popupMenuIsOpen() && !cat_gameStatsIsActive())
            {
                cat_mrClickRelease(0, TRUE);
            }
            break;
        case RPE_KeyDown:
            if (gameIsRunning || cat_gameStatsIsActive())
            {
                udword vk = catKeyVkFromKey(ID);            // HWI: the exe's ids are virtual-key codes (Key.c)

                if (nisIsRunning && thisNisPlaying && ((vk == 0x1b) || (vk == 0x20) || (vk == 0x0d)))
                {                                           // Escape, Space or Enter skip the NIS (a single-player
                    if (!singlePlayerGame || !aviIsPlaying) //      game's not during a movie: binkDonePlaying 0x008b6d94)
                    {
                        cat_kbCommandExecute(0, CAT_KB_Escape);
                    }
                }
                else if (!mrDisabled)
                {
                    cat_kbKeyPress(0, ID);
                    if ((vk == 0x04) || (vk == 0xf8))
                    {                                       // the middle button (and its double click) focuses
                        cat_kbCommandExecute(0, CAT_KB_Focus);
                    }
                }
                else
                {
                    if (cat_gameStatsIsActive() && ((vk == 0x1b) || (vk == 0x58)))
                    {                                       // the stats screen ends on Escape or X
                        cat_gameStatsEnd();
                    }
                    if ((vk == 0x20) || (vk == 0x1b))
                    {                                       // Space or Escape skip the fleet intel
                        cat_kbCommandExecute(0, CAT_KB_SkipSensors);
                    }
                }
            }
            break;
        case RPE_KeyUp:
            cat_kbKeyRelease(0, ID);
            break;
        default:
#if MR_VERBOSE_LEVEL >= 2
            dbgMessagef("mrRegionProcess: unprocessed message. reg = 0x%x, ID = 0x%x, event = 0x%x, data = %d", reg, ID, event, data);
#endif
            break;
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : mrPlayerNameDraw
    Description : Draw the name of a particular player
    Inputs      : playerIndex - index of player who's name we are to print
                  x, y - location to print
    Outputs     : Draws names of specified multi-player player
    Return      : void
----------------------------------------------------------------------------*/
/*
void mrPlayerNameDraw(sdword playerIndex, sdword x, sdword y)
{
    color c;

#if TO_STANDARD_COLORS
    if (&universe.players[playerIndex] == universe.curPlayerPtr)
    {
        c = teFriendlyColor;
    }
    else if (allianceArePlayersAllied(&universe.players[playerIndex], universe.curPlayerPtr))
    {
        c = teAlliedColor;
    }
    else
    {
        c = teHostileColor;
    }

#else
    c = teColorSchemes[playerIndex].tacticalColor;
#endif
    fontPrint(x, y, c, playerNames[playerIndex]);
}
*/

// CAT (M2-R5 hud): HW1's mrCommandMessageDraw (the gMessage lines at the top left) is retired: Cataclysm's
//      clCommandMessage feeds the info overlay's message scroller (cat_ioMessagesDraw, cat_moRender)

void bigmessageDisplay(char *msg,sdword position)
{
    dbgAssertOrIgnore(position < MAX_BIGMESSAGES);

    bMessage[position].messageOn = TRUE;
    dbgAssertOrIgnore(strlen(msg) < MAX_BIGMESSAGE_LENGTH);

    strcpy(bMessage[position].message,msg);
}

void bigmessageErase(sdword position)
{
    bMessage[position].messageOn = FALSE;
}

static sdword bigmessageYPosition[MAX_BIGMESSAGES] = { 200,260 };

void mrBigMessageDraw(void)
{
    sdword i;
    fonthandle fhSave;

    dbgAssertOrIgnore(mrBigFont != 0);

    for (i=0;i<MAX_BIGMESSAGES;i++)
    {
        if (bMessage[i].messageOn)
        {
            fhSave = fontCurrentGet();
            fontMakeCurrent(mrBigFont);

            fontPrintCentre(bigmessageYPosition[i],colWhite,bMessage[i].message);

            fontMakeCurrent(fhSave);
        }
    }
}

//
// Move Line Functions
//
// NOTE: This code is supposed to reside in tactical.c, but for some reason nothing
//       gets rendered when it is moved there.  Therefore, these functions should stay
//       here.  Move them at your own risk!
#define TO_MOVE_LINE_COLOR              TW_MOVETO_LINE_COLOR
#define TO_MOVE_LINE_PULSE_COLOR        TW_MOVETO_PULSE_COLOR
#define TO_ATT_MOVE_LINE_COLOR          TW_ATTMOVETO_LINE_COLOR
#define TO_ATT_MOVE_LINE_PULSE_COLOR    TW_ATTMOVETO_PULSE_COLOR
#define TO_PULSE_CLOSE_SHRINK           0.8f
#define TO_FADE_SIZE                    2  //*pulsesize
#define TO_MOVE_LINE_RADIUS_STRETCH     TW_MOVETO_CIRCLE_RADIUS
#define TO_PULSE_SPEED_SCALE            TW_MOVETO_PULSE_SPEED_SCALE
#define TO_MOVETO_ENDCIRCLE_RADIUS      TW_MOVETO_ENDCIRCLE_RADIUS

//a kludgy global variable
bool32 pulse_at_beginning = FALSE;

/*-----------------------------------------------------------------------------
    Name        : toDrawPulsedLine
    Description : Draws a line with a cool ass pulse flying down it at incredible velocities
    Inputs      : linestart  - where the line starts
                  lineend    - where the line ends
                  pulsesize  - how big that cool ass pulse is
                  linecolor  - the color of the line
                  pulsecolor - the color of the cool ass pulse
    Outputs     : See "description"
    Return      : void
----------------------------------------------------------------------------*/
bool32 toDrawPulsedLine(vector linestart, vector lineend, real32 pulsesize, color linecolor, color pulsecolor)
{
    static real32 lasttime = 0.0, pulsestartfraction = 0.0;
    real32 distance, pulsedistance, fadesize;
    vector dirvect, pulsestart, pulseend, fadestart, fadeend;
    bool32 pulse_at_end = FALSE;
    bool32 draw_pulse_beg = TRUE, draw_fadein = FALSE, draw_fadeout = FALSE;

    pulse_at_beginning = FALSE;

    // CAT: toDrawPulsedLine 0x00567d40 runs the pulse on the task's real time (cat_taskRealTimeElapsed), not the
    //      universe's game time
    if (cat_taskRealTimeElapsed != lasttime)
    {
        pulsestartfraction += (cat_taskRealTimeElapsed - lasttime)/TO_PULSE_SPEED_SCALE;
        lasttime            = cat_taskRealTimeElapsed;

        // - first check is for the very first iteration of toDrawPulsedLine where
        //   "universe.totaltimeelapsed" will be much larger that "lasttime" and
        //   therefore "pulsestartfraction" is quite large
        // - the second check is to fix a very obscure bug where "lasttime" somehow
        //   gets set to a value larger than "universe.totaltimeelapsed".  We can't
        //   figure out why "lasttime" does this, though...
        if ((pulsestartfraction > 1.0) || (pulsestartfraction < 0.0))
        {
            pulsestartfraction = 0.0;
            pulse_at_beginning = TRUE;
        }
    }

    //calculate the pulse
    //get the unit direction vector of the pulse line
    vecSub(dirvect, lineend, linestart);
    distance = (real32) fsqrt(vecMagnitudeSquared(dirvect));
    vecNormalize(&dirvect);

    //find the start and end points of the pulse
    pulsedistance = pulsestartfraction * distance;

    while (pulsesize > distance/2)
    {
        pulsesize *= TO_PULSE_CLOSE_SHRINK;
    }

    //if the pulse is at the end of the moveto line
    if (pulsedistance > (distance + pulsesize))
    {
        pulsestartfraction = 0.0;
        pulse_at_beginning = TRUE;
    }

    //calculate the fade regions
    fadesize = TO_FADE_SIZE*pulsesize;

    //if the pulse is far enough away from the beginning of the moveto line to have a fadein region
    if (pulsedistance > fadesize)
    {
        vecScalarMultiply(fadestart, dirvect, pulsedistance - fadesize);
        vecAddTo(fadestart, linestart);
        draw_fadein = TRUE;
    }
    else if (pulsedistance < pulsesize + fadesize)
    {
        pulse_at_beginning = TRUE;

        if (pulsedistance < pulsesize)
        {
            draw_pulse_beg = FALSE;
        }
    }

    //if the pulse is far enough away from the end of the moveto line to have a fadeout region
    if (pulsedistance < (distance - fadesize - pulsesize))
    {
        vecScalarMultiply(fadeend, dirvect, (pulsedistance + pulsesize + fadesize));
        vecAddTo(fadeend, linestart);
        draw_fadeout = TRUE;
    }

    if ((pulsedistance > (distance - fadesize/3 - pulsesize)) ||
        (distance < TW_MOVETO_ENDCIRCLE_RADIUS))
    {
        pulse_at_end = TRUE;
    }

    //find the point locations of the start and end of the pulse
    vecScalarMultiply(pulsestart, dirvect, (pulsedistance /*- (pulsestartfraction * pulsesize)*/));
    vecAddTo(pulsestart, linestart);
    vecScalarMultiply(pulseend, dirvect, (pulsedistance + (pulsestartfraction * pulsesize)));
    vecAddTo(pulseend, linestart);


    glBegin(GL_LINES);
    {
        if (draw_pulse_beg)
        {
            //draw the line from the ship to the pulse
            glColor3ub(colRed(linecolor), colGreen(linecolor), colBlue(linecolor));
            glVertex3fv((GLfloat *)&linestart);

            //draw the fadein
            if (draw_fadein)
            {
                glColor3ub(colRed(linecolor), colGreen(linecolor), colBlue(linecolor));
                glVertex3fv((GLfloat *)&fadestart);
                glColor3ub(colRed(linecolor), colGreen(linecolor), colBlue(linecolor));
                glVertex3fv((GLfloat *)&fadestart);
            }
            glColor3ub(colRed(pulsecolor), colGreen(pulsecolor), colBlue(pulsecolor));
            glVertex3fv((GLfloat *)&pulsestart);

        }
        else
        {
            glColor3ub(colRed(pulsecolor), colGreen(pulsecolor), colBlue(pulsecolor));
            glVertex3fv((GLfloat *)&linestart);
            glColor3ub(colRed(pulsecolor), colGreen(pulsecolor), colBlue(pulsecolor));
            glVertex3fv((GLfloat *)&pulsestart);
        }

        //draw the pulse
        glColor3ub(colRed(pulsecolor), colGreen(pulsecolor), colBlue(pulsecolor));
        glVertex3fv((GLfloat *)&pulsestart);

        if (!pulse_at_end)
        {
            glColor3ub(colRed(pulsecolor), colGreen(pulsecolor), colBlue(pulsecolor));
            glVertex3fv((GLfloat *)&pulseend);

            //draw the line from the pulse to the end of the line
            glColor3ub(colRed(pulsecolor), colGreen(pulsecolor), colBlue(pulsecolor));
            glVertex3fv((GLfloat *)&pulseend);

            //draw the fadeout
            if (draw_fadeout)
            {
                glColor3ub(colRed(linecolor), colGreen(linecolor), colBlue(linecolor));
                glVertex3fv((GLfloat *)&fadeend);
                glColor3ub(colRed(linecolor), colGreen(linecolor), colBlue(linecolor));
                glVertex3fv((GLfloat *)&fadeend);
            }
        }
        glColor3ub(colRed(pulsecolor), colGreen(pulsecolor), colBlue(pulsecolor));
        glVertex3fv((GLfloat *)&lineend);
    }
    glEnd();

    return pulse_at_end;
}


/*-----------------------------------------------------------------------------
    Name        : toDrawMoveToCircle
    Description : Draws a circle at the destination of the ship
    Inputs      : ship - the ship
    Outputs     : See "description"
    Return      : void
----------------------------------------------------------------------------*/
void toDrawMoveToEndCircle(ShipPtr ship, bool32 pulse, color linecolor, color pulsecolor)
{
    if (pulse)
    {
        primCircleOutline3(&ship->moveTo, TO_MOVETO_ENDCIRCLE_RADIUS,
                           32, 0, pulsecolor, Z_AXIS);
    }
    else
    {
        primCircleOutline3(&ship->moveTo, TO_MOVETO_ENDCIRCLE_RADIUS,
                            32, 0, linecolor, Z_AXIS);
    }
}



/*-----------------------------------------------------------------------------
    Name        : toDrawMoveToLine
    Description : Draws a single move line from a ship to it's move point
    Inputs      : ship - the aformentioned ship
    Outputs     : See "description"
    Return      : void
----------------------------------------------------------------------------*/
// CAT: the waypoint path's colours by path type (toDrawMoveToLine 0x00568100: 0x0088d78c once, 0x0088d790 a loop,
//      0x0088d794 a patrol)
#define TO_PATH_ONCE_COLOR              0xfff7b591
#define TO_PATH_LOOP_COLOR              0xffcd59ac
#define TO_PATH_PATROL_COLOR            0xffad7982

// one leg of the path and the circle at its end point (glBegin(GL_LINES) as the exe, no smoothing)
static void toDrawPathLeg(vector *from, vector *to, vector *circle, color c)
{
    glBegin(GL_LINES);
    glColor3ub(colRed(c), colGreen(c), colBlue(c));
    glVertex3fv((GLfloat *)from);
    glVertex3fv((GLfloat *)to);
    glEnd();
    primCircleOutline3(circle, TO_MOVETO_ENDCIRCLE_RADIUS, 32, 0, c, Z_AXIS);
}

/*-----------------------------------------------------------------------------
    Name        : toDrawMovePath
    Description : CAT: the legs of a waypoint move still to fly (toDrawMoveToLine 0x00568100's loop over the
                  move order's points, inline in the exe): once, the legs after the current point; a loop, all but
                  the leg into the current point (the first leg closes the loop); a patrol out or back, the legs on
                  either side of the current point by the direction. A leg that starts at the current point starts at
                  the ship's moveTo (its own point), or for the patrol's turning leg at the ship.
----------------------------------------------------------------------------*/
static void toDrawMovePath(ShipPtr ship)
{
    CommandToDo *command = ship->command;
    MoveCommand *move;
    sdword index, cur, last;

    if ((command == NULL) || (command->ordertype.order != COMMAND_MOVE) || (command->move.numPoints <= 0))
    {
        return;
    }
    move = &command->move;
    for (index = 0; index < move->numPoints; index++)
    {
        cur = move->curPoint;
        last = move->numPoints - 1;
        switch (move->pathType)
        {
            case CAT_MOVEPATH_ONCE:
                if (index > cur)
                {
                    toDrawPathLeg((index - 1 == cur) ? &ship->moveTo : &move->points[index - 1], &move->points[index],
                                  &move->points[index], TO_PATH_ONCE_COLOR);
                }
                break;
            case CAT_MOVEPATH_LOOP:
                if (index != cur)
                {
                    if (index == 0)
                    {
                        toDrawPathLeg((last == cur) ? &ship->moveTo : &move->points[last], &move->points[0],
                                      &move->points[0], TO_PATH_LOOP_COLOR);
                    }
                    else
                    {
                        toDrawPathLeg((index - 1 == cur) ? &ship->moveTo : &move->points[index - 1],
                                      &move->points[index], &move->points[index], TO_PATH_LOOP_COLOR);
                    }
                }
                break;
            case CAT_MOVEPATH_PATROL_OUT:
                if (index < cur - 1)
                {
                    toDrawPathLeg(&move->points[index + 1], &move->points[index], &move->points[index],
                                  TO_PATH_PATROL_COLOR);
                }
                else if (index == cur - 1)
                {
                    toDrawPathLeg(&ship->obj.posinfo.position, &move->points[index], &move->points[index],
                                  TO_PATH_PATROL_COLOR);
                }
                else if ((index >= cur) && (index < last))
                {
                    toDrawPathLeg((index == cur) ? &ship->moveTo : &move->points[index], &move->points[index + 1],
                                  &move->points[index + 1], TO_PATH_PATROL_COLOR);
                }
                break;
            case CAT_MOVEPATH_PATROL_BACK:
                if (index > cur + 1)
                {
                    toDrawPathLeg(&move->points[index - 1], &move->points[index], &move->points[index],
                                  TO_PATH_PATROL_COLOR);
                }
                else if (index == cur + 1)
                {
                    toDrawPathLeg(&ship->obj.posinfo.position, &move->points[index], &move->points[index],
                                  TO_PATH_PATROL_COLOR);
                }
                else if (index < cur)
                {
                    toDrawPathLeg((index + 1 == cur) ? &ship->moveTo : &move->points[index + 1], &move->points[index],
                                  &move->points[index], TO_PATH_PATROL_COLOR);
                }
                break;
            default:
                break;
        }
    }
}

void toDrawMoveToLine(ShipPtr ship, color linecolor, color pulsecolor)
{
    vector shipfront, shipright, dirfrontproj, dirrightproj, dirvect, dirproj,
           zero = {0,0,0}, negdir;
    real32 distance, pulsesize, shipradius;
    bool32 pulse = FALSE;

    //initialize a bunch of vectors and distances
    shipradius  = ship->magnitudeSquared*ship->staticinfo->common.staticCollInfo.approxcollspheresize;
    pulsesize   = 2*shipradius;

    toDrawMovePath(ship);                                   // CAT: the waypoint path's legs first (0x00568100)

    matGetVectFromMatrixCol3(shipfront, ship->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol2(shipright, ship->rotatable.rotinfo.coordsys);

    vecSub(dirvect, ship->moveTo, ship->impactable.collInfo.collPosition);
    distance = (real32) fsqrt(vecMagnitudeSquared(dirvect));

    //find the moveto starting point
    vecScalarMultiply(dirfrontproj, shipfront, vecDotProduct(dirvect, shipfront));
    vecScalarMultiply(dirrightproj, shipright, vecDotProduct(dirvect, shipright));
    vecAdd(dirproj, dirfrontproj, dirrightproj);
    vecNormalize(&dirproj);

    //calculate the edge of the MoveToEndCircle
    //find the negative of the direction
    vecSub(negdir, zero, dirvect);
    negdir.z = 0;
    vecNormalize(&negdir);
    vecMultiplyByScalar(negdir, TO_MOVETO_ENDCIRCLE_RADIUS);
    vecAddTo(negdir, ship->moveTo);

    vecMultiplyByScalar(dirproj, (shipradius)*TO_MOVE_LINE_RADIUS_STRETCH);
    vecAddTo(dirproj, ship->impactable.collInfo.collPosition);


    //if the ship is pretty close to the end of the move, don't draw the line
    if (distance > pulsesize)
    {
        pulse = toDrawPulsedLine(dirproj, negdir, pulsesize, linecolor, pulsecolor);
    }

    if (distance > ((shipradius*TO_MOVE_LINE_RADIUS_STRETCH) + TO_MOVETO_ENDCIRCLE_RADIUS))
    {
        toDrawMoveToEndCircle(ship, pulse, linecolor, pulsecolor);
    }


}


/*-----------------------------------------------------------------------------
    Name        : toDrawMoveFromLine
    Description : Draws a single move line from a ship's movefrom point to the ship
    Inputs      : ship - the aformentioned ship
    Outputs     : See "description"
    Return      : void
----------------------------------------------------------------------------*/
void toDrawMoveFromLine(ShipPtr ship)
{
    vector shipback, shippos, dirvect;
    real32 distance,shipradius;

    shipradius = TO_MOVE_LINE_RADIUS_STRETCH*ship->staticinfo->common.staticCollInfo.approxcollspheresize;

    //calculate the line
    matGetVectFromMatrixCol3(shipback, ship->rotatable.rotinfo.coordsys);
    vecMultiplyByScalar(shipback, (shipradius));
    shippos = ship->impactable.collInfo.collPosition;
    vecSubFrom(shippos, shipback);
    shipback = shippos;

    vecSub(dirvect, shipback, ship->moveFrom);
    distance = (real32) fsqrt(vecMagnitudeSquared(dirvect));

    if (distance < shipradius)
    {
        return;
    }

    glBegin(GL_LINES);
    {
        //draw the line from the ship to the pulse
        glColor3ub(colRed(TO_MOVE_LINE_COLOR), colGreen(TO_MOVE_LINE_COLOR), colBlue(TO_MOVE_LINE_COLOR));
        glVertex3fv((GLfloat *)&shipback);
        glColor3ub(colRed(TO_MOVE_LINE_COLOR), colGreen(TO_MOVE_LINE_COLOR), colBlue(TO_MOVE_LINE_COLOR));
        glVertex3fv((GLfloat *)&ship->moveFrom);
    }
    glEnd();


}



/*-----------------------------------------------------------------------------
    Name        : toDrawMoveCircle
    Description : Draws a circle around the moving ship
    Inputs      : ship - the ship
                  scale - the tactical overlay color scale factor to fade in the move
                          circle (not implemented)
    Outputs     : See "description"
    Return      : void
----------------------------------------------------------------------------*/
void toDrawMoveCircle(ShipPtr ship, real32 scale, color linecolor, color pulsecolor)
{
    real32  shipradius;
    hmatrix rotmat;
    vector origin = {0.0, 0.0, 0.0};

    hmatMakeHMatFromMat(&rotmat, &ship->rotatable.rotinfo.coordsys);
    hmatPutVectIntoHMatrixCol4(ship->impactable.collInfo.collPosition, rotmat);

    glPushMatrix();
    glMultMatrixf((GLfloat *)&rotmat);

    shipradius = ship->magnitudeSquared*TO_MOVE_LINE_RADIUS_STRETCH*ship->staticinfo->common.staticCollInfo.approxcollspheresize;

    if ((cat_shipFormationCommand(ship))&&(cat_shipFormationCommand(ship)->formation.pulse))
    {
        primCircleOutline3(&origin, shipradius, 32, 0, pulsecolor, X_AXIS);
    }
    else
    {
        primCircleOutline3(&origin, shipradius, 32, 0, linecolor, X_AXIS);
    }

    glPopMatrix();

}




/*-----------------------------------------------------------------------------
    Name        : toMoveLineDraw
    Description : Draws a line with a cool pulse from the ship's current position to
                  it's moveTo position.  Also draws a circle around the moving ship
    Inputs      : ship - the ship
                  scale - the tactical overlay color scale factor to fade in the move
                          circle (not used at the moment)
    Outputs     : See "description"
    Return      : void
----------------------------------------------------------------------------*/
void toMoveLineDraw(ShipPtr ship, real32 scale)
{
    bool32   primEnabled;
    color  linecolor;
    color  pulsecolor;

    // CAT: toMoveLineDraw 0x00568e30 draws the move line of every class (HW1 skipped fighters and corvettes)
    //do da graphics setting up stuff
    primEnabled = primModeEnabled;
    if (primEnabled)
    {
        primModeClear2();
    }
    rndLightingEnable(FALSE);
    rndTextureEnable(FALSE);

    //setup color: attack moves different from normal moves
    if ((ship->command != NULL) && (ship->command->ordertype.order == COMMAND_ATTACK))
    {
        linecolor  = TO_ATT_MOVE_LINE_COLOR;
        pulsecolor = TO_ATT_MOVE_LINE_PULSE_COLOR;
    }
    else
    {
        linecolor  = TO_MOVE_LINE_COLOR;
        pulsecolor = TO_MOVE_LINE_PULSE_COLOR;
    }

    //if a ship is in a formation but not the leader
    if ((cat_shipFormationCommand(ship))&&(ship!=cat_shipFormationCommand(ship)->selection->ShipPtr[0]))
    {
        //only draw the moveto circle
        toDrawMoveCircle(ship, scale, linecolor, pulsecolor);
    }
    // CAT: no HW1 ResearchShip line drawing (M2-R4 ships-b)
    else
    {
        //den draw da linez
        toDrawMoveToLine(ship, linecolor, pulsecolor);

        if (cat_shipFormationCommand(ship))   // CAT: no Ship.formationcommand
        {
            cat_shipFormationCommand(ship)->formation.pulse = pulse_at_beginning;
        }

        //toDrawMoveFromLine(ship);
        toDrawMoveCircle(ship, scale, linecolor, pulsecolor);
    }

    //den do sum more graphics non-setting up stuff
    if (primEnabled)
    {
        primModeSet2();
    }
}

/*-----------------------------------------------------------------------------
    Name        : toMoveLineDraw
    Description : Draws a line with a cool pulse from the ship's current position to
                  it's moveTo position.  Also draws a circle around the moving ship
    Inputs      : ship - the ship
                  scale - the tactical overlay color scale factor to fade in the move
                          circle (not used at the moment)
    Outputs     : See "description"
    Return      : void
----------------------------------------------------------------------------*/
void toFieldSphereDraw(ShipPtr ship, real32 radius, real32 scale,color passedColour)
{
    hmatrix rotmat;
    vector origin = {0.0, 0.0, 0.0};
    vector up =    {1.0, 0.0, 0.0};
    vector right = {0.0, 1.0, 0.0};
    vector head =  {0.0, 0.0, 1.0};
    matrix tmpmat;

    //Turn off 2D primmode
    primModeClear2();
    rndLightingEnable(FALSE);
    rndTextureEnable(FALSE);

    //Draw the circles
    //    hmatMakeHMatFromMat(&rotmat, &ship->rotatable.rotinfo.coordsys);
        //hmatPutVectIntoHMatrixCol4(ship->impactable.collInfo.collPosition, rotmat);
    //     hmatPutVectIntoHMatrixCol4(ship->obj.posinfo.position, rotmat);


    matPutVectIntoMatrixCol1(up,tmpmat);
    matPutVectIntoMatrixCol2(right,tmpmat);
    matPutVectIntoMatrixCol3(head,tmpmat);

    hmatMakeHMatFromMat(&rotmat, &tmpmat);
    hmatPutVectIntoHMatrixCol4(ship->obj.posinfo.position, rotmat);


    glPushMatrix();
        glMultMatrixf((GLfloat *)&rotmat);

        primCircleOutline3(&origin, radius, 32, 4, passedColour, Z_AXIS);
        primCircleOutline3(&origin, radius, 32, 4, passedColour, X_AXIS);
        primCircleOutline3(&origin, radius, 32, 4, passedColour, Y_AXIS);

        glPopMatrix();
    //stop drawing circles


    //Turn on 2D primmode
    primModeSet2();
}

void toFieldSphereDrawGeneral(vector position, real32 radius,color passedColour)
{
    hmatrix rotmat;
    vector origin = {0.0, 0.0, 0.0};
    vector up =    {1.0, 0.0, 0.0};
    vector right = {0.0, 1.0, 0.0};
    vector head =  {0.0, 0.0, 1.0};
    matrix tmpmat;

    //Turn off 2D primmode
    primModeClear2();
    rndLightingEnable(FALSE);
    rndTextureEnable(FALSE);

    //Draw the circles
    //    hmatMakeHMatFromMat(&rotmat, &ship->rotatable.rotinfo.coordsys);
        //hmatPutVectIntoHMatrixCol4(ship->impactable.collInfo.collPosition, rotmat);
    //     hmatPutVectIntoHMatrixCol4(ship->obj.posinfo.position, rotmat);


    matPutVectIntoMatrixCol1(up,tmpmat);
    matPutVectIntoMatrixCol2(right,tmpmat);
    matPutVectIntoMatrixCol3(head,tmpmat);

    hmatMakeHMatFromMat(&rotmat, &tmpmat);
    hmatPutVectIntoHMatrixCol4(position, rotmat);


    glPushMatrix();
        glMultMatrixf((GLfloat *)&rotmat);

        primCircleOutline3(&origin, radius, 32, 4, passedColour, Z_AXIS);
        primCircleOutline3(&origin, radius, 32, 4, passedColour, X_AXIS);
        primCircleOutline3(&origin, radius, 32, 4, passedColour, Y_AXIS);

        glPopMatrix();
    //stop drawing circles


    //Turn on 2D primmode
    primModeSet2();
}

real32 pulse1_timeelapsed=0.0f;
real32 pulse1_lasttime=0.0f;
real32 pulse1_timeelapsed_fast=0.0f;
real32 pulse1_lasttime_fast=0.0f;
real32 pulsedistance = 0.0f;
real32 pulsefastdistance = 0.0f;
real32 delay = 0.0f;

#define CROSS_HALF_HIGHT   TO_CROSS_HALF_SIZE


void toPulse1_positionupdate();
void toPulse1(vector linestart, vector lineend, real32 pulsesize, color pulsecolor);


//Draws a special radial display for the TO of special ships: Proximity Sensor
void toDrawRadialIndicator1(ShipPtr ship, real32 radius, real32 scale,color passedColour)
{

   hmatrix rotmat;
   vector up    = {1.0, 0.0, 0.0};
   vector right = {0.0, 1.0, 0.0};
   vector head  = {0.0, 0.0, 1.0};

   matrix tmpmat;
   vector lineend,tmpvec1,tmpvec2,tmpvecup1,tmpvecup2,tmpvecright1,tmpvecright2,tmpvechead1,tmpvechead2;

   //Turn off 2D primmode
   primModeClear2();
   rndLightingEnable(FALSE);
   rndTextureEnable(FALSE);

   matPutVectIntoMatrixCol1(up,tmpmat);
   matPutVectIntoMatrixCol2(right,tmpmat);
   matPutVectIntoMatrixCol3(head,tmpmat);

   hmatMakeHMatFromMat(&rotmat, &tmpmat);
   //hmatPutVectIntoHMatrixCol4(ship->obj.posinfo.position, rotmat);


   glPushMatrix();
   glMultMatrixf((GLfloat *)&rotmat);

   //primCircleOutline3(&ship->obj.posinfo.position, radius, 32, 0, TO_CROSS_COLOR1, Z_AXIS);
   //primCircleOutline3(&ship->obj.posinfo.position, radius, 32, 0, TO_CROSS_COLOR1, X_AXIS);
   //primCircleOutline3(&ship->obj.posinfo.position, radius, 32, 0, TO_CROSS_COLOR1, Y_AXIS);

   vecScalarMultiply(tmpvecup1,up,CROSS_HALF_HIGHT);
   vecScalarMultiply(tmpvecup2,up,-CROSS_HALF_HIGHT);
   vecScalarMultiply(tmpvecright1,right,CROSS_HALF_HIGHT);
   vecScalarMultiply(tmpvecright2,right,-CROSS_HALF_HIGHT);
   vecScalarMultiply(tmpvechead1,head,CROSS_HALF_HIGHT);
   vecScalarMultiply(tmpvechead2,head,-CROSS_HALF_HIGHT);

   toPulse1_positionupdate();                //update time information
    if(delay > 0.0f)
    {
        delay -= pulse1_timeelapsed;
        if(delay <= 0.0f)
            delay = 0.0f;

    }
////
   vecScalarMultiply(lineend,right,radius);
   vecAddTo(lineend,ship->obj.posinfo.position);
   toPulse1(ship->obj.posinfo.position, lineend, TO_PULSE1_SIZE       , TO_PULSE1_COLOR);

   vecAdd(tmpvec1,lineend,tmpvecup1);
   vecAdd(tmpvec2,lineend,tmpvecup2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR1);

   vecAdd(tmpvec1,lineend,tmpvechead1);
   vecAdd(tmpvec2,lineend,tmpvechead2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR1);

   vecScalarMultiply(lineend,right,-radius);
   vecAddTo(lineend,ship->obj.posinfo.position);
   toPulse1(ship->obj.posinfo.position, lineend, TO_PULSE1_SIZE       , TO_PULSE1_COLOR);

   vecAdd(tmpvec1,lineend,tmpvecup1);
   vecAdd(tmpvec2,lineend,tmpvecup2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR1);

   vecAdd(tmpvec1,lineend,tmpvechead1);
   vecAdd(tmpvec2,lineend,tmpvechead2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR1);

////
   vecScalarMultiply(lineend,up, radius);
   vecAddTo(lineend,ship->obj.posinfo.position);
   toPulse1(ship->obj.posinfo.position, lineend, TO_PULSE1_SIZE       , TO_PULSE1_COLOR);

   vecAdd(tmpvec1,lineend,tmpvecright1);
   vecAdd(tmpvec2,lineend,tmpvecright2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR1);

   vecAdd(tmpvec1,lineend,tmpvechead1);
   vecAdd(tmpvec2,lineend,tmpvechead2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR1);

   vecScalarMultiply(lineend,up, -radius);
   vecAddTo(lineend,ship->obj.posinfo.position);
   toPulse1(ship->obj.posinfo.position, lineend, TO_PULSE1_SIZE       , TO_PULSE1_COLOR);

   vecAdd(tmpvec1,lineend,tmpvecright1);
   vecAdd(tmpvec2,lineend,tmpvecright2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR1);

   vecAdd(tmpvec1,lineend,tmpvechead1);
   vecAdd(tmpvec2,lineend,tmpvechead2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR1);

////
   vecScalarMultiply(lineend,head, radius);
   vecAddTo(lineend,ship->obj.posinfo.position);
   toPulse1(ship->obj.posinfo.position, lineend, TO_PULSE1_SIZE       , TO_PULSE1_COLOR);

   vecAdd(tmpvec1,lineend,tmpvecup1);
   vecAdd(tmpvec2,lineend,tmpvecup2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR1);

   vecAdd(tmpvec1,lineend,tmpvecright1);
   vecAdd(tmpvec2,lineend,tmpvecright2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR1);

   vecScalarMultiply(lineend,head, -radius);
   vecAddTo(lineend,ship->obj.posinfo.position);
   toPulse1(ship->obj.posinfo.position, lineend, TO_PULSE1_SIZE       , TO_PULSE1_COLOR);

   vecAdd(tmpvec1,lineend,tmpvecup1);
   vecAdd(tmpvec2,lineend,tmpvecup2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR1);

   vecAdd(tmpvec1,lineend,tmpvecright1);
   vecAdd(tmpvec2,lineend,tmpvecright2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR1);

   glPopMatrix();
   //stop drawing circles


   //Turn on 2D primmode
   primModeSet2();

}

#define TO_PULSE1_SPEED    TO_PULSE1_SPEED
#define TO_DELAY_PULSE1    TO_DELAY_PULSE1

void toPulse1_positionupdate()
{
   pulse1_timeelapsed = universe.totaltimeelapsed - pulse1_lasttime;
   pulse1_lasttime = universe.totaltimeelapsed;
   if(delay == 0.0f)
   {
       pulsedistance += pulse1_timeelapsed*TO_PULSE1_SPEED;
   }

}

/*-----------------------------------------------------------------------------
    Name        : toPulse1
    Description : Draws a pulse from linestart to lineend
    Inputs      : linestart  - where the line starts
                  lineend    - where the line ends
                  pulsesize  - how big that cool ass pulse is
                  linecolor  - the color of the line
                  pulsecolor - the color of the cool ass pulse
    Outputs     : See "description"
    Return      : void
----------------------------------------------------------------------------*/
void toPulse1(vector linestart, vector lineend, real32 pulsesize, color pulsecolor)
{
    real32 distance, pulseenddistance;
    vector dirvect, pulsestart, pulseend;

    if(delay > 0.0f)
    {
       return;
    }

    vecSub(dirvect, lineend, linestart);
    distance = fsqrt(vecMagnitudeSquared(dirvect));
    vecNormalize(&dirvect);


    pulseenddistance = pulsedistance+pulsesize;
   if((pulseenddistance) > distance)
   {
     //pulse has overshot end so
     pulsedistance = 0.0f;
     pulseenddistance = pulsesize;
     delay = TO_DELAY_PULSE1;
     //pulse1_timeelapsed = 0.0f;
     //pulse1_lasttime = 0.0f;
   }
   vecScalarMultiply(pulsestart, dirvect, pulsedistance);

    vecScalarMultiply(pulseend, dirvect, pulseenddistance);
    vecAddTo(pulsestart,linestart);
    vecAddTo(pulseend,linestart);

    glBegin(GL_LINES);
    {
        glColor3ub(colRed(pulsecolor), colGreen(pulsecolor), colBlue(pulsecolor));
        glVertex3fv((GLfloat *)&pulsestart);
        glColor3ub(colRed(pulsecolor), colGreen(pulsecolor), colBlue(pulsecolor));
        glVertex3fv((GLfloat *)&pulseend);

    }
    glEnd();

}

/*-----------------------------------------------------------------------------
Radial Indicator # 2...same as above but different speeds
-----------------------------------------------------------------------------*/

real32 pulse2_timeelapsed=0.0f;
real32 pulse2_lasttime=0.0f;
real32 pulse2_timeelapsed_fast=0.0f;
real32 pulse2_lasttime_fast=0.0f;
real32 pulsedistance2 = 0.0f;
real32 pulsefastdistance2 = 0.0f;
real32 delay2 = 0.0f;


void toPulse2_positionupdate();
void toPulse2(vector linestart, vector lineend, real32 pulsesize, color pulsecolor);


//Draws a special radial display for the TO of special ships: Proximity Sensor
void toDrawRadialIndicator2(ShipPtr ship, real32 radius, real32 scale,color passedColour)
{

   hmatrix rotmat;
   vector up    = {1.0, 0.0, 0.0};
   vector right = {0.0, 1.0, 0.0};
   vector head  = {0.0, 0.0, 1.0};

   matrix tmpmat;
   vector lineend,tmpvec1,tmpvec2,tmpvecup1,tmpvecup2,tmpvecright1,tmpvecright2,tmpvechead1,tmpvechead2;

   //Turn off 2D primmode
   primModeClear2();
   rndLightingEnable(FALSE);
   rndTextureEnable(FALSE);

   matPutVectIntoMatrixCol1(up,tmpmat);
   matPutVectIntoMatrixCol2(right,tmpmat);
   matPutVectIntoMatrixCol3(head,tmpmat);

   hmatMakeHMatFromMat(&rotmat, &tmpmat);
   //hmatPutVectIntoHMatrixCol4(ship->obj.posinfo.position, rotmat);


   glPushMatrix();
   glMultMatrixf((GLfloat *)&rotmat);

   //primCircleOutline3(&ship->obj.posinfo.position, radius, 32, 0, TO_CROSS_COLOR1, Z_AXIS);
   //primCircleOutline3(&ship->obj.posinfo.position, radius, 32, 0, TO_CROSS_COLOR1, X_AXIS);
   //primCircleOutline3(&ship->obj.posinfo.position, radius, 32, 0, TO_CROSS_COLOR1, Y_AXIS);

   vecScalarMultiply(tmpvecup1,up,CROSS_HALF_HIGHT);
   vecScalarMultiply(tmpvecup2,up,-CROSS_HALF_HIGHT);
   vecScalarMultiply(tmpvecright1,right,CROSS_HALF_HIGHT);
   vecScalarMultiply(tmpvecright2,right,-CROSS_HALF_HIGHT);
   vecScalarMultiply(tmpvechead1,head,CROSS_HALF_HIGHT);
   vecScalarMultiply(tmpvechead2,head,-CROSS_HALF_HIGHT);

   toPulse2_positionupdate();                //update time information
    if(delay2 > 0.0f)
    {
        delay2 -= pulse2_timeelapsed;
        if(delay2 <= 0.0f)
            delay2 = 0.0f;

    }
////
   vecScalarMultiply(lineend,right,radius);
   vecAddTo(lineend,ship->obj.posinfo.position);
   toPulse2(ship->obj.posinfo.position, lineend, TO_PULSE2_SIZE       , TO_PULSE2_COLOR);

   vecAdd(tmpvec1,lineend,tmpvecup1);
   vecAdd(tmpvec2,lineend,tmpvecup2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR2);

   vecAdd(tmpvec1,lineend,tmpvechead1);
   vecAdd(tmpvec2,lineend,tmpvechead2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR2);

   vecScalarMultiply(lineend,right,-radius);
   vecAddTo(lineend,ship->obj.posinfo.position);
   toPulse2(ship->obj.posinfo.position, lineend, TO_PULSE2_SIZE       , TO_PULSE2_COLOR);

   vecAdd(tmpvec1,lineend,tmpvecup1);
   vecAdd(tmpvec2,lineend,tmpvecup2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR2);

   vecAdd(tmpvec1,lineend,tmpvechead1);
   vecAdd(tmpvec2,lineend,tmpvechead2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR2);

////
   vecScalarMultiply(lineend,up, radius);
   vecAddTo(lineend,ship->obj.posinfo.position);
   toPulse2(ship->obj.posinfo.position, lineend, TO_PULSE2_SIZE       , TO_PULSE2_COLOR);

   vecAdd(tmpvec1,lineend,tmpvecright1);
   vecAdd(tmpvec2,lineend,tmpvecright2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR2);

   vecAdd(tmpvec1,lineend,tmpvechead1);
   vecAdd(tmpvec2,lineend,tmpvechead2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR2);

   vecScalarMultiply(lineend,up, -radius);
   vecAddTo(lineend,ship->obj.posinfo.position);
   toPulse2(ship->obj.posinfo.position, lineend, TO_PULSE2_SIZE       , TO_PULSE2_COLOR);

   vecAdd(tmpvec1,lineend,tmpvecright1);
   vecAdd(tmpvec2,lineend,tmpvecright2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR2);

   vecAdd(tmpvec1,lineend,tmpvechead1);
   vecAdd(tmpvec2,lineend,tmpvechead2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR2);

////
   vecScalarMultiply(lineend,head, radius);
   vecAddTo(lineend,ship->obj.posinfo.position);
   toPulse2(ship->obj.posinfo.position, lineend, TO_PULSE2_SIZE       , TO_PULSE2_COLOR);

   vecAdd(tmpvec1,lineend,tmpvecup1);
   vecAdd(tmpvec2,lineend,tmpvecup2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR2);

   vecAdd(tmpvec1,lineend,tmpvecright1);
   vecAdd(tmpvec2,lineend,tmpvecright2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR2);

   vecScalarMultiply(lineend,head, -radius);
   vecAddTo(lineend,ship->obj.posinfo.position);
   toPulse2(ship->obj.posinfo.position, lineend, TO_PULSE2_SIZE       , TO_PULSE2_COLOR);

   vecAdd(tmpvec1,lineend,tmpvecup1);
   vecAdd(tmpvec2,lineend,tmpvecup2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR2);

   vecAdd(tmpvec1,lineend,tmpvecright1);
   vecAdd(tmpvec2,lineend,tmpvecright2);
   primLine3(&tmpvec1,&tmpvec2,TO_CROSS_COLOR2);

   glPopMatrix();
   //stop drawing circles

   //Turn on 2D primmode
   primModeSet2();

}

#define TO_PULSE2_SPEED    TO_PULSE2_SPEED
#define TO_DELAY_PULSE2    TO_DELAY_PULSE2

void toPulse2_positionupdate()
{
   pulse2_timeelapsed = universe.totaltimeelapsed - pulse2_lasttime;
   pulse2_lasttime = universe.totaltimeelapsed;
   if(delay2 == 0.0f)
   {
      pulsedistance2 += pulse2_timeelapsed*TO_PULSE2_SPEED;
   }
}

/*-----------------------------------------------------------------------------
    Name        : toPulse2
      -same as above but faster...
      ..need because of different time variables
----------------------------------------------------------------------------*/
void toPulse2(vector linestart, vector lineend, real32 pulsesize, color pulsecolor)
{
    real32 distance, pulseenddistance;
    vector dirvect, pulsestart, pulseend;

    if(delay2 > 0.0f)
    {
       return;
    }

    vecSub(dirvect, lineend, linestart);
    distance = fsqrt(vecMagnitudeSquared(dirvect));
    vecNormalize(&dirvect);


    pulseenddistance = pulsedistance2+pulsesize;
   if((pulseenddistance) > distance)
   {
     //pulse has overshot end so
     pulsedistance2 = 0.0f;
     pulseenddistance = pulsesize;
     delay2 = TO_DELAY_PULSE2;
     //pulse1_timeelapsed = 0.0f;
     //pulse1_lasttime = 0.0f;
   }
   vecScalarMultiply(pulsestart, dirvect, pulsedistance2);

    vecScalarMultiply(pulseend, dirvect, pulseenddistance);
    vecAddTo(pulsestart,linestart);
    vecAddTo(pulseend,linestart);

    glBegin(GL_LINES);
    {
        glColor3ub(colRed(pulsecolor), colGreen(pulsecolor), colBlue(pulsecolor));
        glVertex3fv((GLfloat *)&pulsestart);
        glColor3ub(colRed(pulsecolor), colGreen(pulsecolor), colBlue(pulsecolor));
        glVertex3fv((GLfloat *)&pulseend);

    }
    glEnd();

}

//Radial Indicator 3  Circles pulsing to Outline a Sphere

#define TO_CIRCLE3_SPEED    4500.0f
#define TO_CIRCLE3_DELAY    3.0f
#define TO_CIRCLE3_DELAY_SPACEING 0.05f
#define TO_NUM_CIRCLES_3      1

real32 circle3_timeelapsed[TO_NUM_CIRCLES_3];
real32 circle3_lasttime[TO_NUM_CIRCLES_3];
real32 circle3_timeelapsed_fast[TO_NUM_CIRCLES_3];
real32 circle3_lasttime_fast[TO_NUM_CIRCLES_3];
real32 circle3distance[TO_NUM_CIRCLES_3];
real32 circle3fastdistance[TO_NUM_CIRCLES_3];
real32 circle3delay[TO_NUM_CIRCLES_3];

//Draws a special radial display for the TO of special ships: Proximity Sensor
void toDrawRadialIndicator3(ShipPtr ship, real32 radius, real32 scale,color passedColour)
{

   hmatrix rotmat;
   real32 newrad;
   vector up    = {1.0, 0.0, 0.0};
   vector right = {0.0, 1.0, 0.0};
   vector head  = {0.0, 0.0, 1.0};
   sdword count;
   matrix tmpmat;
   vector circlestart,circlestart2;


   count = 0;

   while(count < TO_NUM_CIRCLES_3)
   {
        vecScalarMultiply(circlestart,head,radius);

        circle3_timeelapsed[count] = universe.totaltimeelapsed - circle3_lasttime[count];
        circle3_lasttime[count] = universe.totaltimeelapsed;
        if(circle3delay[count] <= 0.0f)
        {
            circle3delay[count] = 0.0f;
            circle3distance[count] += circle3_timeelapsed[count]*TO_CIRCLE3_SPEED;
        }
        else
        {
            circle3delay[count] -= circle3_timeelapsed[count];
            count++;
            continue;
        }
        if(circle3distance[count] > 2*radius)      //make faster :)    precalc diameter
        {
            circle3distance[count] = 0.0f;
            if(count == 0)
            {
                circle3delay[count] = TO_CIRCLE3_DELAY;
            }
            else
            {
                circle3delay[count] = TO_CIRCLE3_DELAY_SPACEING + circle3delay[count-1];
            }
            count++;
            continue;
        }

        vecScalarMultiply(circlestart2,head,-circle3distance[count]);
        vecAddTo(circlestart,circlestart2);
        vecAddTo(circlestart,ship->obj.posinfo.position);
        if(circle3distance[count] > radius)
        {
            newrad = 2*radius - circle3distance[count];
        }
        else
        {
            newrad = circle3distance[count];
        }
       newrad = fsqrt(radius*radius - (radius - newrad)*(radius - newrad));

       //Turn off 2D primmode
       primModeClear2();
       rndLightingEnable(FALSE);
       rndTextureEnable(FALSE);

       matPutVectIntoMatrixCol1(up,tmpmat);
       matPutVectIntoMatrixCol2(right,tmpmat);
       matPutVectIntoMatrixCol3(head,tmpmat);

       hmatMakeHMatFromMat(&rotmat, &tmpmat);
       //hmatPutVectIntoHMatrixCol4(ship->obj.posinfo.position, rotmat);


       glPushMatrix();
       glMultMatrixf((GLfloat *)&rotmat);

       primCircleOutline3(&circlestart, newrad, 32, 0, passedColour, Z_AXIS);
       //primCircleOutline3(&ship->obj.posinfo.position, radius, 32, 0, TO_CROSS_COLOR1, X_AXIS);
       //primCircleOutline3(&ship->obj.posinfo.position, radius, 32, 0, TO_CROSS_COLOR1, Y_AXIS);

       glPopMatrix();
       //stop drawing circles


       //Turn on 2D primmode
       primModeSet2();
        count++;
   }

}
//Radial Indicator 4  Circles pulsing to Outline a Sphere

#define TO_CIRCLE4_SPEED    7500.0f
#define TO_CIRCLE4_DELAY    0.5f
#define TO_CIRCLE4_DELAY_SPACEING 0.05f


real32 circle4_timeelapsed[TO_NUM_CIRCLES_3];
real32 circle4_lasttime[TO_NUM_CIRCLES_3];
real32 circle4_timeelapsed_fast[TO_NUM_CIRCLES_3];
real32 circle4_lasttime_fast[TO_NUM_CIRCLES_3];
real32 circle4distance[TO_NUM_CIRCLES_3];
real32 circle4fastdistance[TO_NUM_CIRCLES_3];
real32 circle4delay[TO_NUM_CIRCLES_3];

//Draws a special radial display for the TO of special ships: Proximity Sensor
void toDrawRadialIndicator4(ShipPtr ship, real32 radius, real32 scale,color passedColour)
{

   hmatrix rotmat;
   real32 newrad;
   vector up    = {1.0, 0.0, 0.0};
   vector right = {0.0, 1.0, 0.0};
   vector head  = {0.0, 0.0, 1.0};
   sdword count;
   matrix tmpmat;
   vector circlestart,circlestart2;

   count = 0;

   while(count < TO_NUM_CIRCLES_3)
   {
        vecScalarMultiply(circlestart,head,radius);

        // CAT: toDrawRadialIndicator4 0x00569030 runs the rings on the task's real time (cat_taskRealTimeElapsed)
        circle4_timeelapsed[count] = cat_taskRealTimeElapsed - circle4_lasttime[count];
        circle4_lasttime[count] = cat_taskRealTimeElapsed;
        if(circle4delay[count] <= 0.0f)
        {
            circle4delay[count] = 0.0f;
            circle4distance[count] += circle4_timeelapsed[count]*TO_CIRCLE4_SPEED;
        }
        else
        {
            circle4delay[count] -= circle4_timeelapsed[count];
            count++;
            continue;
        }
        if(circle4distance[count] > 2*radius)      //make faster :)    precalc diameter
        {
            circle4distance[count] = 0.0f;
            if(count == 0)
            {
                circle4delay[count] = TO_CIRCLE4_DELAY;
            }
            else
            {
                circle4delay[count] = TO_CIRCLE4_DELAY_SPACEING + circle4delay[count-1];
            }
            count++;
            continue;
        }

        vecScalarMultiply(circlestart2,head,-circle4distance[count]);
        vecAddTo(circlestart,circlestart2);
        vecAddTo(circlestart,ship->obj.posinfo.position);
        if(circle3distance[count] > radius)
        {
            newrad = 2*radius - circle4distance[count];
        }
        else
        {
            newrad = circle4distance[count];
        }
       newrad = fsqrt(radius*radius - (radius - newrad)*(radius - newrad));

       //Turn off 2D primmode
       primModeClear2();
       rndLightingEnable(FALSE);
       rndTextureEnable(FALSE);

       matPutVectIntoMatrixCol1(up,tmpmat);
       matPutVectIntoMatrixCol2(right,tmpmat);
       matPutVectIntoMatrixCol3(head,tmpmat);

       hmatMakeHMatFromMat(&rotmat, &tmpmat);
       //hmatPutVectIntoHMatrixCol4(ship->obj.posinfo.position, rotmat);


       glPushMatrix();
       glMultMatrixf((GLfloat *)&rotmat);

       primCircleOutline3(&circlestart, newrad, 32, 0, passedColour, Z_AXIS);
       //primCircleOutline3(&ship->obj.posinfo.position, radius, 32, 0, TO_CROSS_COLOR1, X_AXIS);
       //primCircleOutline3(&ship->obj.posinfo.position, radius, 32, 0, TO_CROSS_COLOR1, Y_AXIS);

       glPopMatrix();
       //stop drawing circles


       //Turn on 2D primmode
       primModeSet2();
        count++;
   }

}

/*-----------------------------------------------------------------------------
    Name        : mrRegionDraw
    Description : Region callback for drawing the main region.
    Inputs      : standard region callback crap
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
sdword mrFrameCounter = 0;
void mrRegionDraw(regionhandle reg)
{
    udword shiftBits;
    static bool32 mrClearRenderEverything = FALSE;
    sdword index;

    if (singlePlayerGame)
    {
        mrFrameCounter++;
    }
    if (aivRenderMainScreen)
    {
        //doing this directly for efficiency
//        aivarValueSet(aivRenderMainScreen, mrRenderMainScreen);
        aivRenderMainScreen->value = mrRenderMainScreen;
    }

    if ((!mrRenderMainScreen) || nisBlackFade == 1.0f)
    {
        if (nisIsRunning && nisFullyScissored && rndScissorEnabled)
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor(0, NIS_LetterHeight, MAIN_WindowWidth, MAIN_WindowHeight - NIS_LetterHeight * 2);
        }
        if (mrClearRenderEverything)
        {
            mrClearRenderEverything = FALSE;
            feRenderEverything = FALSE;
        }
        if (feRenderEverything)
        {
            mrClearRenderEverything = TRUE;
        }
        if (nisTextCardIndex)
        {                                                   //render text cards on a black screen
            nisTextCardListDraw();
        }
        if (nisSMPTECounter != NULL)
        {
            nisSMPTECounterDraw(thisNisPlaying, nisSMPTECounter);
        }
        for (index = 0; index < NIS_NumberStatics; index++)
        {
            if (nisStatic[index] != NULL)
            {
                nisStaticDraw(nisStatic[index]);
            }
        }
        if (cat_gameStatsIsActive())
        {                                                   // CAT: the game-stats screen (cat_gameStatsDraw 0x004896b0)
            cat_gameStatsDraw();
            catUiSelfTestStatsFrame();                      // HWI-TEST(M2-R6 ui self-test): counts the frame
        }

        /*
        if (!rndScissorEnabled)
        {                                                   //if we can't scissor properly
            rndDrawScissorBars(rndScissorEnabled);
        }
        */
        return;
    }

    if (piePointSpecMode != PSM_Idle)
    {                                                       //draw point spec
        // CAT (M2-R7 r6-leftovers): mrRegionDraw 0x0056939a sets only the post-object callback; the pie plate's
        //      three draw callbacks stay as piePointModeOnOff set them (HW1 reset them to the move set here, which
        //      replaced the waypoint cursor 0x004e6e70 and restored the lines of PIE_SPEC_MoveNoLines)
        rndPostObjectCallback = piePointSpecDraw;
    }

    if (nisIsRunning && nisFullyScissored && rndScissorEnabled)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, NIS_LetterHeight, MAIN_WindowWidth, MAIN_WindowHeight - NIS_LetterHeight * 2);
    }
    if (nisCaptureCamera)
    {
        mrCamera = nisCamera;
    }
    else if (gameIsRunning)
    {
        mrCamera = &(universe.mainCameraCommand.actualcamera);
        ccControl(&universe.mainCameraCommand);
    }
    else
    {
        mrCamera = &defaultCamera;
    }
    rndMainViewRender(miRenderCamera(mrCamera));            // HWI-FEATURE(motion-interp): the NIS camera at the render time
    catMotionTestFrameSample(miRenderCamera(mrCamera));     // HWI-FEATURE(motion-interp) measurement: the drawn frame's positions

    if (feStack[feStackIndex].screen == NULL)
        mouseCursorTextDraw();

    rndTextureEnable(FALSE);
    glDisable(GL_TEXTURE_2D);

    // CAT (M2-R6 startup-keys): no caps-lock sync of the tactical overlay here (HW1's mrTacticalOverlayState): the
    //      exe's mrRegionDraw has none; its Toggle_Tactical_Display key command toggles mrDrawTactical (KeyBindings.c)
    if (mrDrawTactical)
    {                                                       //need better key mechanism
        rndTextureEnable(FALSE);
        glDisable(GL_TEXTURE_2D);

        toAllShipsDraw();                                   //draw tactical overlays
        // CAT (M2-R5 hud): the legend is drawn by the main overlay (cat_moRender, under the player names)
    }

    selSelectedDraw();                                      //draw overlays for selected ships, if any
    if (catMrSelecting && !mrDisabled)
    {                                                       // CAT: the band box (cat_mrSelectRectDraw 0x0056ac00)
        cat_mrSelectRectDraw(0);
    }

#if SP_DEBUGKEYS
    kasDebugDraw();
#endif

    if (singlePlayerGame)
    {
        objectiveDrawStatus();
        kasfPopupTextDraw();
    }

    if (nisTextCardIndex)
    {
        nisTextCardListDraw();
    }

    if (nisSMPTECounter != NULL)
    {
        nisSMPTECounterDraw(thisNisPlaying, nisSMPTECounter);
    }
    for (index = 0; index < NIS_NumberStatics; index++)
    {
        if (nisStatic[index] != NULL)
        {
            nisStaticDraw(nisStatic[index]);
        }
    }

    //glDisable(GL_SCISSOR_TEST);

    // CAT: HW1's formation name on the screen is gone (Cataclysm gives a message, mrSetTheFormation 0x005659d0)
    if (printCaptainMessage)
    {
        fonthandle fhSave;
        udword y;

        y = mainWindowHeight/2 - ((fontHeight("W") + 4)*numPlayerDropped)/2 - fontHeight("W") - 6;

        fhSave = fontMakeCurrent(mrBigFont);

        fontPrintCentre(y,colWhite,strGetString(strCaptainTransfering));

        fontMakeCurrent(fhSave);
    }

    if ((multiPlayerGame) && (numPlayerDropped>0))
    {
        if (universe.totaltimeelapsed < (printTimeout + TIMEOUT_TIME))
        {
            udword y;
            char   temp[80];
            fonthandle fhSave;

            fhSave = fontMakeCurrent(mrBigFont);
            y = mainWindowHeight/2 - ((fontHeight("W") + 4)*numPlayerDropped)/2;

            for (index=0;index<numPlayerDropped;index++)
            {
                sprintf(temp, "%s %s", playerNames[playersDropped[index]], (playersReadyToGo[index] == PLAYER_QUIT) ? strGetString(strQuit) : strGetString(strDroppedOut));
                fontPrintCentre(y,colWhite,temp);
                y += fontHeight("W") + 4;
            }

            fontMakeCurrent(fhSave);
        }
        else
        {
            numPlayerDropped=0;
        }
    }

    // CAT: a cycled formation or tactic is ordered once its delay has passed (mrRegionDraw 0x00569260; HW1's
    //      formation selection state is gone)
    catMrCyclesApply();

    mrBigMessageDraw();

    //draw the bearing numbers on the horizon line
    if (smTickTextIndex > 0)
    {
        smTickTextDraw();
    }

    // CAT (M2-R5 hud): mrRegionDraw 0x00569260 then draws the main overlay, the ship info panel of the first selected
    //      ship when it is the player's, the popup menu (M2-R6 mainrgn-popup), and updates the overlay's hover state,
    //      the panel's drag and the popup's hover
    cat_moRender();
    if (selSelected.numShips > 0 && selSelected.ShipPtr[0]->playerowner == universe.curPlayerPtr)
    {
        cat_shipPrintDraw(selSelected.ShipPtr[0]);
    }
    cat_popupMenuDraw();
    cat_ioUpdateHover();
    cat_shipPrintDragUpdate();
    cat_popupMenuUpdateHover(FALSE);

    //compute animating stipple pattern every time mainrgn is drawn
    pieLineStippleCounter += pieLineStippleSpeed;
    shiftBits = ((udword)pieLineStippleCounter) % 16;
    pieStipplePattern = pieLineStipple | ((udword)pieLineStipple << 16);
    if ((sdword)shiftBits < 0)
    {
        pieStipplePattern >>= 16 - shiftBits;
    }
    else
    {
        pieStipplePattern >>= shiftBits;
    }

    //draw a big black rectangle to do a fade between the scissor bars
#define NIS_EXCESSSCISSORMARGIN     7       //!!! Keith fix this
    if (nisBlackFade != 0.0f)
    {                                                       //if there is some sort of NIS fade going down
        sdword y;
        if (nisScissorFade != 0.0f)
        {
            y = (sdword)((real32)NIS_LetterHeight * nisScissorFade);
        }
        else
        {
            y = NIS_LetterHeight;
        }
        glEnable(GL_BLEND);
        glBegin(GL_TRIANGLE_STRIP);
        glColor4f(0.0f, 0.0f, 0.0f, nisBlackFade);
        glVertex2f(primScreenToGLX(-1), primScreenToGLY(y - NIS_EXCESSSCISSORMARGIN));
        glColor4f(0.0f, 0.0f, 0.0f, nisBlackFade);
        glVertex2f(primScreenToGLX(-1), primScreenToGLY(MAIN_WindowHeight - y + NIS_EXCESSSCISSORMARGIN));
        glColor4f(0.0f, 0.0f, 0.0f, nisBlackFade);
        glVertex2f(primScreenToGLX(MAIN_WindowWidth), primScreenToGLY(y - NIS_EXCESSSCISSORMARGIN));
        glColor4f(0.0f, 0.0f, 0.0f, nisBlackFade);
        glVertex2f(primScreenToGLX(MAIN_WindowWidth), primScreenToGLY(MAIN_WindowHeight - y + NIS_EXCESSSCISSORMARGIN));
        glEnd();
        glDisable(GL_BLEND);
    }

    if (mrWhiteOut)
    {
        rectangle rect = { -1,0,MAIN_WindowWidth,MAIN_WindowHeight };
        sdword c;
        real32 t;

        if (mrWhiteOutT < 0.5f)
        {
            t = mrWhiteOutT * 2.0f;
            c = (sdword)(t * 255.0f);
            glEnable(GL_BLEND);
            rndAdditiveBlends(TRUE);
            primRectSolid2(&rect, colRGBA(70,70,255,c));
            rndAdditiveBlends(FALSE);
            glDisable(GL_BLEND);
        }
        else
        {
            t = 2.0f * (mrWhiteOutT - 0.5f);
            c = (sdword)((1.0f - t) * 255.0f);
            primRectSolid2(&rect, colRGB((70*c)>>8,(70*c)>>8,(255*c)>>8));
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : mrReset
    Description : Reset the state of the main region, such as turning off camera rotation.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void mrReset(void)
{
    // CAT: mrReset 0x00569f80: the band box and the camera turn stop, the point mode goes off (piePointModeOnOff); the
    //      exe's software-renderer call (RGLtype 0) does not apply to this tree's GL
    catMrSelecting = FALSE;
    catMrRightMode = 0;
    mouseCursorShow();
    if (piePointSpecMode != PSM_Idle)
    {
        piePointModeOnOff(PIE_SPEC_None, NULL);
    }
}

/*-----------------------------------------------------------------------------
    Name        : mrNISStarting
    Description : Called from NIS.c when an NIS, this function will clear out
                    any persistant control variables.
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void mrNISStarting(void)
{
    // CAT: mrNISStarting 0x00569fd0 on Cataclysm's state words: the band box and the camera turn stop
    if (catMrSelecting)
    {
        selRectNone();
        catMrSelecting = FALSE;
    }
    if (catMrRightMode != 0)
    {
        catMrRightMode = 0;
    }
    mrSaveTactical = mrDrawTactical;                        //turn off the tactical overlay
    mrDrawTactical = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : mrNISStopping
    Description : Like the above function, except it's called when an NIS ends.
                    It can optionally reset previously cleared variables.
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void mrNISStopping(void)
{
    // CAT: mrNISStopping 0x0056a020 on Cataclysm's state words
    if (catMrSelecting)
    {
        selRectNone();
        catMrSelecting = FALSE;
    }
    if (catMrRightMode != 0)
    {
        catMrRightMode = 0;
    }
    mrDrawTactical = mrSaveTactical;                        //turn on the tactical overlay if it was on before
}

/*-----------------------------------------------------------------------------
    Name        : mrShipDied
    Description : Called when a ship died to remove any persistant references.
    Inputs      :
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void mrShipDied(Ship *ship)
{
    if (mrMothershipPtr == ship)
    {
        mrMothershipPtr = NULL;
    }
}

// CAT (M2-R6 startup-keys): HW1's mrTacticalOverlayState (the tactical overlay following the caps-lock state when the
//      key was bound to caps lock) is gone: the exe has none; Toggle_Tactical_Display toggles it (KeyBindings.c)

#if MR_KEYBOARD_CHEATS
void mrAddResources(void)
{
    #define MR_RESOURCE_INCREASE 1000

    dbgMessage("Adding Resources");
    universe.curPlayerPtr->resourceUnits += MR_RESOURCE_INCREASE;
}

void mrAllTechnology(void)
{
    dbgMessage("Adding All Technologies");
    rmInitializeResearchStruct(universe.curPlayerPtr,TRUE,TECH_ALLTECHNOLOGY);
}

extern bool32 cmCheapShips;
void mrCheapShips(void)
{
    cmCheapShips = !cmCheapShips;
    if (cmCheapShips)
        dbgMessage("Cheap Ships On");
    else
        dbgMessage("Cheap Ships Off");

}

void mrToggleAI(void)
{
    mrNoAI = !mrNoAI;
    if (mrNoAI)
        dbgMessage("Computer AI On");
    else
        dbgMessage("Computer AI Off");
}


void mrAsteroids(void)
{

    dbgMessage("Asteroids");
    mrTradeStuffTest(NULL, NULL);
}

bool32 gMosaic = 0;
void mrBackground(void)
{
    gMosaic = !gMosaic;
}

void mrFilter(void)
{
    texLinearFiltering = !texLinearFiltering;
    trFilterEnable(texLinearFiltering);
}

#define MR_DEBUG_SIZE 16
#define MR_SINGLEPLAYER     0x0001      //allow in singleplayer
#define MR_NETGAME          0x0002      //allow in netgames

typedef void (*debugFunc)(void);    //callback function


typedef struct debugStructX
{
    char *string;
    debugFunc callback;
    udword mask;
}
debugStruct;

debugStruct mrDebugFunctions[] = {
    //for now, don't use QWIOGJN in the string - they're intercepted elsewhere

    //backwards string,        callback,             permission masks

    {"YMMUY",                   mrAddResources,      MR_SINGLEPLAYER},
    {"HCETLLA",                 mrAllTechnology,     MR_SINGLEPLAYER},
    {"PAEHC",                   mrCheapShips,        MR_SINGLEPLAYER},
    {"YMMUD",                   mrToggleAI,          MR_SINGLEPLAYER},
    {"RETSA",                   mrAsteroids,         0},
    {"YVURJ",                   mrBackground,        MR_SINGLEPLAYER | MR_NETGAME},
    {"RTLF",                    mrFilter,            MR_SINGLEPLAYER | MR_NETGAME},
    {"",                        NULL,                0},
};





char mrDebugKeyBuffer[MR_DEBUG_SIZE];
sdword mrDebugPointer;



bool32 mrPlayerHasCheated = FALSE;

void mrScanDebugCodes(sdword ID)
{
    sdword i,j,k;
    bool32 match;

    mrDebugPointer = (mrDebugPointer+1) & (MR_DEBUG_SIZE-1);
    mrDebugKeyBuffer[mrDebugPointer] = ID;

    for (i=0; mrDebugFunctions[i].callback!=NULL; i++)
    {
        match = TRUE;
        for (k=0,j = mrDebugPointer; mrDebugFunctions[i].string[k]>0; k++,j=(j+MR_DEBUG_SIZE-1) & (MR_DEBUG_SIZE-1))
        {
            if (mrDebugKeyBuffer[j] != mrDebugFunctions[i].string[k])
            {
                match = FALSE;
                break;
            }
        }
        if (match)
        {
            if (singlePlayerGame)
            {
                if (!(mrDebugFunctions[i].mask & MR_SINGLEPLAYER))
                {
                    return;
                }
            }

            if (multiPlayerGame)
            {
                if (!(mrDebugFunctions[i].mask & MR_NETGAME))
                {
                    return;
                }
            }

            mrPlayerHasCheated = TRUE;
            dbgMessage("Debug code entered");
            //add SFX
            (mrDebugFunctions[i].callback)();

        }

    }
}

#endif //MR_KEYBOARD_CHEATS
