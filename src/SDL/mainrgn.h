/*=============================================================================
    Name    : mainrgn.h
    Purpose : Logic for handling region events for the main game screen.

    Created 6/27/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___MAINRGN_H
#define ___MAINRGN_H

#include "Camera.h"
#include "FEFlow.h"
#include "Formation.h"
#include "ShipSelect.h"
#include "SpaceObj.h"
#include "Types.h"

/*=============================================================================
    Switches:
=============================================================================*/
#define MR_GUI_SINGLECLICK          1

#ifdef HW_BUILD_FOR_DEBUGGING

#define MR_ERROR_CHECKING           1           //general error checking
#define MR_VERBOSE_LEVEL            2           //control specific output code
#define MR_TEST_HPB                 0           //test heading/pitch/bank interactively
#define MR_RELEASE_MOUSE            1           //allow the mouse to be freed from the window
#define MR_TEST_GUNS                1           //allow the game testing mode
#define MR_SOUND_RELOAD_VOLUMES     1           //permits reloading of volume tables
#define MR_CAN_FOCUS_ROIDS          1           //can focus on asteroids,dust clouds and derelicts with an alt-click
#define MR_KEYBOARD_CHEATS          1           //enable typing in cheats on the keyboard

#else

#define MR_ERROR_CHECKING           0           //general error checking
#define MR_VERBOSE_LEVEL            0           //control specific output code
#define MR_TEST_PING                0           //test ping time
#define MR_TEST_HPB                 0           //test heading/pitch/bank interactively
#define MR_RELEASE_MOUSE            0           //allow the mouse to be freed from the window
#define MR_TEST_GUNS                0           //allow the game testing mode
#define MR_SOUND_RELOAD_VOLUMES     0           //permits reloading of volume tables
#define MR_CAN_FOCUS_ROIDS          0           //can focus on asteroids,dust clouds and derelicts with an alt-click
#define MR_KEYBOARD_CHEATS          0           //enable typing in cheats on the keyboard

#endif

/*=============================================================================
    Definitions:
=============================================================================*/
//message display definitions
#define MR_MESSAGE_LINE_SPACING     12
#define MR_MESSAGE_LINE_START       400
#define MR_MouseMovementClickLimit  12
#define MR_FormationDelay           1.0f
#define MR_NumberDoublePressTime    0.5
#define MR_FastMoveShipClickTime    0.5
#define MR_FastMoveShipClickX       5
#define MR_FastMoveShipClickY       5

#define MR_LetterboxGrey            0.5f, 0.5f, 0.5f

/*=============================================================================
    Data:
=============================================================================*/
//data exported for the benefit of other modules with similar functionality
extern bool32 mrWhiteOut;
extern real32 mrWhiteOutT;
extern sdword mrRenderMainScreen;
extern sdword mrOldMouseX, mrOldMouseY;
extern sdword mrMouseHasMoved;
extern sdword mrMouseMovementClickLimit;

//extern ShipPtr mrLastClosestShip;
extern real32 MAX_INGAME_MOVECOMMAND_DISTANCE;
extern real32 mrClosestDistance;

//camera rendering in the main region
extern Camera *mrCamera;

extern void (*mrHoldLeft)(void);
extern void (*mrHoldRight)(void);


extern bool32 helpinfoactive;

extern real32 mrNumberDoublePressTime;
extern sdword mrLastKeyPressed;
extern real32 mrLastKeyTime;

extern bool32 mrDisabled;
extern bool32 mrDrawTactical;
extern bool32 mrSpecialBandBox;                 // 0x00b39ba0

#if MR_CAN_FOCUS_ROIDS
extern bool32 mrCanFocusRoids;
#endif

/*=============================================================================
    Functions:
=============================================================================*/
//startup/shutdown of main region code
void mrStartup(void);
void mrShutdown(void);
void mrReset(void);

void mrShipDied(struct Ship *ship);

//region-process routine for the main region
udword mrRegionProcess(struct tagRegion *reg, sdword ID, udword event, udword data);
//region-draw routine for the main region
void mrRegionDraw(regionhandle reg);

//right-click callback functions
void mrDockingOrders(Ship *gate);                       // CAT: the slipgate order (0x00567750)
void mrDeltaFormation(char *string, featom *atom);
void mrBroadFormation(char *string, featom *atom);
void mrXFormation(char *string, featom *atom);
void mrClawFormation(char *string, featom *atom);
void mrWallFormation(char *string, featom *atom);
void mrSphereFormation(char *string, featom *atom);
void mrCustomFormation(char *string, featom *atom);
void mrHarvestResources(char *string, featom *atom);
void mrBuildShips(char *string, featom *atom);
void mrTradeStuff(char *string, featom *atom);
void mrMoveShips(char *string, featom *atom);
void mrInfo(char *string, featom *atom);
void mrCancel(char *string, featom *atom);
void mrScuttle(char *string, featom *atom);
void mrRetire(char *string, featom *atom);
void mrUpdateHyperspaceStatus(bool32 goForLaunch);
void mrHyperspace(char *string, featom *atom);
void mrLaunch(char *string, featom *atom);
void mrResearch(char *string, featom *atom);
void mrEvasiveTactics(char *string, featom *atom);
void mrNeutralTactics(char *string, featom *atom);
void mrAgressiveTactics(char *string, featom *atom);
void mrFormAlliance(char *string, featom *atom);
void mrBreakAlliance(char *string, featom *atom);
void mrTransferRUS(char *string, featom *atom);

//misc functions
void mrCameraMotion(void);
void mrPlayerNameDraw(sdword playerIndex, sdword x, sdword y);
void mrSelectRectBuild(rectangle *dest, sdword anchorX, sdword anchorY);
sdword mrCursorText(SpaceObj *cursorobj);
void mrEnable(void);
void mrDisable(void);
void mrNULL(void);
void mrSelectHold(Camera *camera, sdword sensors);      // CAT: 0x0056a1e0
void mrShipDied(Ship *ship);

void bigmessageDisplay(char *msg,sdword position);
void bigmessageErase(sdword position);

//probe hack functions
bool32 mrNeedProbeHack(void);
void mrProbeHack();
void mrRemoveAllProbesFromSelection();

//nis strangeness prevention:
void mrNISStarting(void);
void mrNISStopping(void);

/*=============================================================================
    CAT (M2-R6 mainrgn-popup): Cataclysm's main region (mainrgn.c's "Cataclysm's main region" block)
=============================================================================*/
#define CAT_MR_MAX_WAYPOINTS            20          // 0x00b39bc0 .. 0x00b39cb0: the waypoints of a path

// Cataclysm's key commands the main region names: KeyBindings.h (CAT_KB_*, the rows of the exe's command table
//      0x008b6310)
#include "KeyBindings.h"

// the band box's colour by its modifier (cat_mrSelectRectDraw 0x0056ac00: 0x0072bdec .. 0x0072be00)
#define CAT_MR_BAND_GUARD               0xfffa0000
#define CAT_MR_BAND_FOCUS               0xff646464
#define CAT_MR_BAND_FORCEATTACK         0xff6464fa
#define CAT_MR_BAND_ATTACK              0xff0000ff
#define CAT_MR_BAND_SPECIAL             0xff64644b
#define CAT_MR_BAND_SELECT              0xffffffff

// the developer console is open (0x00a2dcd4, cat_consoleOpen; M2-R6 startup-keys: Console.c)
#include "Console.h"
#define CAT_MR_CONSOLE_OPEN()           cat_consoleIsOpen()

extern bool32 catMrSelecting;                   // 0x00a75284
extern sdword catMrRightMode;                   // 0x00a75288
extern sdword catMrWaypointMode;                // 0x00a752b4
extern vector catMrWaypoints[CAT_MR_MAX_WAYPOINTS];
extern sdword catMrWaypointCount;
extern bool32 catMrHomeworldCSA;                // 0x00a7526c
extern bool32 catMrRightClickCSA;               // 0x008b9ec8
extern bool32 catMrWaypointCentre;              // 0x00a75270
extern bool32 mrMenuDontDisappear;              // 0x00a752b0
extern Ship *mrMothershipPtr;                   // 0x00a752c0

bool32 catMrKbHeld(sdword command);
void cat_mrKeyChildrenAlloc(regionhandle region, regionfunction function);              // 0x00563be0
real32 cat_uiShipDamageFraction(Ship *ship);                                            // 0x005c9c60
Ship *cat_uiFindNearestSpecialTarget(Ship *ship, sdword kind);                          // 0x00563e20
void cat_uiSpecialTargetCommand(SelectCommand *targets, sdword ability);                // 0x00564060
void cat_uiSpecialSalvage(SelectCommand *targets);                                      // 0x00564030
void cat_uiSpecialRepair(SelectCommand *targets);                                       // 0x00564630
void cat_uiSpecialLeech(SelectCommand *targets);                                        // 0x00564660
void cat_uiSpecialEMP(SelectCommand *targets);                                          // 0x00564690
void cat_uiSpecialSwarm(SelectCommand *targets);                                        // 0x005646c0
void cat_uiSpecialInfect(SelectCommand *targets);                                       // 0x005646f0
void cat_uiSpecialRam(SelectCommand *targets);                                          // 0x00564720
void cat_uiSpecialFireMissile(SelectCommand *targets);                                  // 0x00564750
void cat_uiReleaseHiveSwarm(void);                                                      // 0x00564780
void cat_uiSiegeCannonCancel(void);                                                     // 0x00564810
void cat_uiCloakShips(void);                                                            // 0x00564890
void cat_uiDecloakShips(void);                                                          // 0x00564920
void cat_uiSetWaypointMode(sdword mode);                                                // 0x00565100
void cat_uiHaltSelection(void);                                                         // 0x00565190
void cat_uiLink(void);                                                                  // 0x00565570
void cat_uiUnlink(void);                                                                // 0x005655e0
void cat_uiUnmimic(void);                                                               // 0x00565770
void cat_mrSetTactics(sdword tactic, bool32 showMessage);                               // 0x00565c30
void cat_uiProtectFormation(SelectCommand *protectees);                                 // 0x00565f30
void cat_uiProtectFormationCancel(void);                                                // 0x00566250
void cat_uiFireSiegeCannon(void);                                                       // 0x00566340
void cat_uiFireDreadnoughtCannon(void);                                                 // 0x005663f0
void cat_uiSetLeechAction(ubyte action);                                                // 0x00566470
void cat_uiUnleech(void);                                                               // 0x00566510
void cat_mrFormationCycle(bool32 backwards);                                            // 0x00566750
void cat_mrTacticsCycle(bool32 backwards);                                              // 0x00566860
void cat_mrMimicTarget(SpaceObj *target);                                               // 0x00567860
bool32 cat_mrTacticalOverlayGet(void);                                                  // 0x0056a080
bool32 cat_mrBandBoxRelease(Camera *camera, sdword sensors);                            // 0x0056a610
void cat_mrSelectRectDraw(sdword sensors);                                              // 0x0056ac00
void cat_mrClickRelease(sdword sensors, bool32 doubleClick);                            // 0x0056ad50
void cat_mrHoldLeft(sdword sensors);                                                    // 0x0056aee0
void cat_mrPressLeft(void);                                                             // 0x0056af10
void cat_mrReleaseLeft(sdword sensors);                                                 // 0x0056af60
void cat_mrPressRight(void);                                                            // 0x0056afe0
void cat_mrReleaseRight(sdword sensors);                                                // 0x0056b0b0
void cat_mrReleaseLeftCB(sdword sensors);                                               // 0x0056b6a0
// the key commands' orders of the main region (functions the export does not list)
void cat_mrMoveShips(void);                                                             // 0x0056a090 [Move]
void cat_mrMoveCallback(vector *point);                                                 // 0x00564a90
void cat_mrWaypointAdd(vector *point);                                                  // 0x00564d70
void cat_mrWaypointsStart(void);                                                        // 0x00564e20
void cat_mrWaypointsEnd(void);                                                          // 0x00564f40
void cat_mrWaypointsToggle(void);                                                       // 0x00565070 [Waypoints]
void cat_mrWaypointModeCycle(void);                                                     // 0x00565090
void cat_mrWaypointRemoveLast(void);                                                    // 0x00565170
void cat_mrDockShips(void);                                                             // 0x00563c30 [Dock]
void cat_mrUpgradeShips(void);                                                          // 0x00565650 [Upgrade_Ships]
void cat_mrFocusOnMothership(void);                                                     // 0x00565e80
void cat_mrEnterSlipgate(void);                                                         // 0x005665a0
void cat_mrHyperspace(void);                                                            // 0x00566920 [Hyperspace]
void cat_mrEscape(void);                                                                // 0x0054f5f0 [Escape]
void mrSetTheFormation(TypeOfFormation formationtype, bool32 showMessage, bool32 shipVoice);   // CAT: 0x005659d0
void mrObjectClick(Ship *ship, bool32 doubleClick);                                    // CAT: 0x00566b60
void mrFormationShips(MaxSelection *selection, Ship *ship);                             // 0x00566a00
Ship *mrNextMothershipPtr(Ship *mothership);                                            // 0x00565da0
void mrRemoveDerelictsAndAllPlayerShipsFromSelecting(void);                             // 0x00563aa0

#endif
