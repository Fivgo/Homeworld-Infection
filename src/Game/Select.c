/*=============================================================================
    Name    : select.c
    Purpose : Logic for selecting ships and groups of ships.

    Created 7/2/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef SW_Render
#ifdef _WIN32
#include <windows.h>
#endif
#endif
#include <stdlib.h>
#include "Vector.h"
#include "FontReg.h"
#include "utility.h"
#include "Debug.h"
#include "Universe.h"
#include "GravWellGenerator.h"
#include "UnivUpdate.h"
#include "FastMath.h"
#include "mouse.h"
#include "glinc.h"
#include "main.h"
#include "InfoOverlay.h"
#include "mainrgn.h"
#include "render.h"
#include "Alliance.h"
#include "Sensors.h"
#include "Select.h"
#include "ShipSelect.h"
#include "StringSupport.h"
#include "Tweak.h"
#include "Tutor.h"
#include "mainrgn.h"
// CAT (M2-R5 hud): the status bars' sources and the overlay icons
#include <math.h>
#include "ACV.h"
#include "Acolyte.h"
#include "BeastWeapon.h"
#include "CameraCommand.h"
#include "Cloak.h"
#include "Dreadnought.h"
#include "HiveFrigate.h"
#include "InfoOverlay.h"
#include "Mothership.h"
#include "NIS.h"
#include "OverlayIcons.h"
#include "ResearchAPI.h"
#include "Salvage.h"
#include "Sentinel.h"
#include "SinglePlayer.h"
#include "Task.h"

// CAT: the stealth types' bit of ShipStaticInfo.catTypeFlags (cat_univInitShipTypeFlags 0x0053b8c0 sets it for sMimic,
//      sLeech and sMCV; cat_selStatusStealthDetectDraw 0x00507920 tests it). Name chosen here.
#define CAT_TYPEFLAG_Stealth    0x00020000

// CAT: the exe rounds its GL-to-screen conversions to the nearest pixel (fadd 1.5 * 2^52 and the low dword:
//      0x0072a458), where GoK's primGLToScreen* macros truncate
#define catSelRound(v)          ((sdword)lrint((double)(v)))
#define catSelScreenX(x)        (MAIN_WindowWidth / 2 + catSelRound((x) * (real32)MAIN_WindowWidth * 0.5f))
#define catSelScreenY(y)        (MAIN_WindowHeight / 2 - catSelRound((y) * (real32)MAIN_WindowHeight * 0.5f))
#define catSelScreenScaleX(x)   catSelRound((real32)MAIN_WindowWidth * (x) * 0.5f)

#define DEBUG_COLLISION_SPHERES 0

/*=============================================================================
    Data:
=============================================================================*/
MaxSelection selSelected;
MaxAnySelection selSelecting;
MaxSelection selHotKeyGroup[SEL_NumberHotKeyGroups];

//script-adjustable parameters
sdword selClickBoxWidth = SEL_ClickBoxWidth;    //size of box inside which a drag is a click
sdword selClickBoxHeight = SEL_ClickBoxHeight;
sdword selClickMargin = SEL_ClickMargin;        //additional 'margin' around selection for 'fuzzy' 'logic'
sdword selDragMargin = SEL_DragMargin;          //additional 'margin' around selection for 'fuzzy' 'logic'
real32 selBandBoxInsideIn = SEL_BandBoxInsideIn;//selection hysteresis factors
real32 selBandBoxInsideOut = SEL_BandBoxInsideOut;
color selShipResourceColor = colRGB(151, 71, 23);
color selShipDarkResourceColor = colRGB(58, 27, 8);
color selSelectingColor = SEL_SelectedColor;    //color of ships being selected
color selShipHealthGreen = SEL_ShipHealthGreen; //ship health in the green
color selShipHealthYellow = SEL_ShipHealthYellow;//ship health color in the yellow
color selShipHealthRed = SEL_ShipHealthRed;     //ship health color in the yellow
color selShipHealthDarkGreen = SEL_ShipHealthDarkGreen; //ship health in the green
color selShipHealthDarkYellow = SEL_ShipHealthDarkYellow;//ship health color in the yellow
color selShipHealthDarkRed = SEL_ShipHealthDarkRed;     //ship health color in the yellow
color selShipHealthSolidGreen = SEL_ShipHealthSolidGreen; //ship health in the green at LOD3+
color selShipHealthSolidYellow = SEL_ShipHealthSolidYellow;//ship health color in the yellow at LOD3+
color selShipHealthSolidRed = SEL_ShipHealthSolidRed;     //ship health color in the red at LOD3+
color selFuelColorGreen = SEL_FuelGreen;        //color of fuel bar 'in the green'
color selFuelColorRed = SEL_ShipHealthRed;
color selFuelColorDarkGreen = SEL_FuelDarkGreen;        //color of fuel bar 'in the green'
color selFuelColorDarkRed = SEL_ShipHealthDarkRed;
real32 selFuelGreenFactor = SEL_FuelGreenFactor;//amount of fuel required for it to be 'in the green'
real32 selDepthSelectMultiplier = SEL_DepthSelectMultiplier;
//real32 selShipHealthGreenFactor = SEL_ShipHealthGreenFactor;//min for green health
//real32 selShipHealthYellowFactor = SEL_ShipHealthYellowFactor;//min for yellow health
real32 selSelectionWidthFactor0 = SEL_SelectionWidthFactor0;
real32 selAsteroidMoveNeartoSize = SEL_AsteroidMoveNeartoSize;
//centre point of current selection
vector selCentrePoint;
real32 selAverageSize;

//hot-key number stuff
color selHotKeyNumberColor = SEL_HotKeyNumberColor;
udword selNumberMargin = SEL_NumberMargin;
char *selHotKeyString[10] = {"10", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

//camera-space location of centre point of ship most recently considered for selection
hvector selCameraSpace;

//fonts used for printing group numbers
char *selFontName0 = NULL;
char *selFontName1 = NULL;
char *selFontName2 = NULL;
char *selFontName3 = NULL;
fonthandle selGroupFont0;
fonthandle selGroupFont1;
fonthandle selGroupFont2;
fonthandle selGroupFont3;
// CAT: a fifth font, "default.hff", registered by selStartup 0x00505300 (exe 0x00c0bae8); the exe's status bars print
//      the ammo count with it (cat_selStatusAmmoCountDraw 0x00507740), which is ported with the status bars
fonthandle selDefaultFont;

real32 SEL_MinSelectionRadius;
real32 selMinSelectionRadius;

#if PIE_MOVE_NEARTO
//closest ship this frame, used for the movement mechanism's 'move near to' command
SpaceObjRotImpTarg *selClosestTarget = NULL;
sdword selClosestDistance = SDWORD_Max;
#endif //PIE_MOVE_NEARTO

#if RND_VISUALIZATION
extern bool8 RENDER_BOXES;
#endif

/*=============================================================================
    Functions:
=============================================================================*/

void selReset(void)
{
    sdword index;

    selSelected.numShips = 0;
    selSelecting.numTargets = 0;

    for (index = 0; index < SEL_NumberHotKeyGroups; index++)
    {
        selHotKeyGroup[index].numShips = 0;
        selHotKeyGroup[index].timeLastStatus = 0.0f;
    }
}

/*-----------------------------------------------------------------------------
    Name        : selStartup
    Description : Start up the selection module
    Inputs      : void
    Outputs     : clears out the selection lists
    Return      : void
----------------------------------------------------------------------------*/
void selStartup(void)
{
    SEL_MinSelectionRadius = primScreenToGLX(6);
    selMinSelectionRadius = SEL_MinSelectionRadius;
    selReset();
    selGroupFont0 = frFontRegister(selFontName0);
    selGroupFont1 = frFontRegister(selFontName1);
    selGroupFont2 = frFontRegister(selFontName2);
    selGroupFont3 = frFontRegister(selFontName3);
    selDefaultFont = frFontRegister("default.hff");     // CAT: selStartup 0x00505300 (exe global 0x00c0bae8)
}

/*-----------------------------------------------------------------------------
    Name        : selShutdown
    Description : Shut down the selection module
    Inputs      : void
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
void selShutdown(void)
{
    ;
}

/*-----------------------------------------------------------------------------
    Name        : selRectDragFunction
    Description : Update a list of ships being selected based upon
                    specified rectangle and camera.
    Inputs      : startNode - starting of the list to search through
                  screenRect - rectangle in viewport space which the user is dragging.
                  camera - which viewport to select in
                  destList - pointer to an array of ships to put selected list into
                  destCount - pointer to integer to recieve number of selected ships
                  playerSpecific - only select ships of current player if TRUE
                  selectAnything - if TRUE, anything, including missiles, asteroids, etc. can be selected
                  bAttack - TRUE if this is an attack selection operation.  Some objects only respond to attack selections.
    Outputs     : Updates the selected list and count.
                  Also updates the selCentrePoint vector.
    Return      : void
----------------------------------------------------------------------------*/
// CAT (M2-R5 hud): the fog-of-war mode (exe 0x008b8848, default 2; cat_selPositionHiddenByFOW 0x00509400 and
//      cat_selShipSelectableInFOW 0x0050d3a0 read it: 0 off, 1 the unexplored cells hide, 2 also the cells outside
//      the grid). Name chosen here; the option that sets it comes with the FOW port (M2-R6)
sdword catFowMode = 2;

/*-----------------------------------------------------------------------------
    Name        : cat_selPositionHiddenByFOW                        0x00509400
    Description : CAT (M2-R5 hud): TRUE if the current player's fog of war hides this point (visited: test the
                  visited count instead of the live usage count).
----------------------------------------------------------------------------*/
bool32 cat_selPositionHiddenByFOW(vector *position, bool32 visited)
{
    (void)position;
    (void)visited;
    if (catFowMode != 0)
    {
        if (universe.curPlayerPtr->catFOWGrid == NULL)
        {
            return TRUE;
        }
        // CAT-TODO(M2-R6 FOW): with a grid, fowSelect(grid); outside the grid (fowIsValidW) hidden only in mode 2,
        //      else hidden where fowGetUsageCountW (visited: fowGetVisitedW) < 1. The fow* module and the players'
        //      grids (cat_fowCreatePlayerGrids at levelInit) come with the FOW port, so no grid exists before it
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_selShipSelectableInFOW                        0x0050d3a0
    Description : CAT (M2-R5 hud): whether the current player may select this enemy ship in the sensors manager:
                  not fully cloaked and not in a hidden cell; in a multiplayer game also not a mimic that poses as
                  allied, and detected by the player.
----------------------------------------------------------------------------*/
bool32 cat_selShipSelectableInFOW(Ship *ship)
{
    bool32 selectable = !cat_shipIsCloakedFully(ship);

    if (catFowMode != 0)
    {
        if (universe.curPlayerPtr->catFOWGrid == NULL)
        {
            selectable = FALSE;
        }
        // CAT-TODO(M2-R6 FOW): with a grid, fowSelect(grid); outside the grid unselectable in mode 2, inside
        //      unselectable where fowGetUsageCountW(ship position) < 1 (see cat_selPositionHiddenByFOW)
    }
    if (!singlePlayerGame)
    {
        if (bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic) &&
            cat_mimicAppearsAlliedTo(ship, universe.curPlayerPtr) == TRUE)
        {
            return FALSE;
        }
        if (!cat_shipDetectedByPlayer(ship, universe.curPlayerPtr->playerIndex))
        {
            return FALSE;
        }
    }
    return selectable;
}

// CAT: selRectDragFunction 0x005053a0 has a ninth argument: centrePoint (TRUE: a target is in when its selection
//      circle's centre is inside the rectangle; FALSE: HW1's overlap test)
void selRectDragFunction(Node *startNode, Camera *camera, rectangle *screenRect, SpaceObjRotImpTarg **destList, sdword *destCount, sdword playerSpecific, bool32 selectAnything, bool32 bAttack, bool32 centrePoint)
{
    Node *targetnode;
    SpaceObjRotImpTarg *target;
    realrectangle selectRect, unionRect, polyRect, rect;
    real32 areaUnion, areaTotal, val;
    vector *point;
    ubyte *pIndex;
    sdword index, base;
    real32 dragMargin = primScreenToGLScaleX(selDragMargin);

    dbgAssertOrIgnore(camera != NULL);                              //verify parameters

    *destCount = 0;                                         //start off with nothing selected

    rect.x0 = primScreenToGLX(screenRect->x0);
    rect.x1 = primScreenToGLX(screenRect->x1);
    rect.y0 = primScreenToGLY(screenRect->y1);
    rect.y1 = primScreenToGLY(screenRect->y0);

    //this system can later be optimised by only walking through sorted porions
    //of the object list.  However, for now we will walk the entire list.
    selSelecting.numTargets = 0;
    for (targetnode = startNode; targetnode != NULL; targetnode = targetnode->next) {
        target = (SpaceObjRotImpTarg *)listGetStructOfNode(targetnode);
        // CAT: an untargetable ship skips the selectable/targetable and hidden tests (selRectDragFunction 0x005053a0)
        if (!(target->header.objtype == OBJ_ShipType && cat_shipIsUntargetable((Ship *)target)))
        {
            if ((target->header.flags & (SOF_Selectable|SOF_Targetable)) == 0) continue;
            if (target->header.flags & SOF_Hide) continue;
        }

        if (target->impactable.collInfo.selCircleRadius <= 0.0f) continue;

        if (target->header.flags & SOF_Dead) continue;

        // CAT: a KAS derelict ship is out, except the big gun's units
        if (bitTest(target->header.catFlags18, CAT_SHIPFLAG_KasDerelict) &&
            (target->header.objtype != OBJ_ShipType || ((Ship *)target)->shiptype != dBigGunUnit)) continue;

        if (!selectAnything)
        {
            if (target->header.objtype == OBJ_ShipType)
            {
                if (playerSpecific)
                {
                    Ship *ship = (Ship *)target;
                    if (!(target->header.flags & SOF_Selectable)) continue;
                    // only own player should be able to select it
                    if (ship->playerowner != universe.curPlayerPtr)
                    {
                        // CAT: except an enemy mimic that poses as allied (not as a resource or derelict): it is
                        //      selected and gives itself away (cat_shipSetDisableReason(ship, 1, 7))
                        if (!bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic) ||
                            cat_mimicAppearsAsResource(ship, universe.curPlayerPtr) ||
                            cat_mimicAppearsAsDerelict(ship, universe.curPlayerPtr) ||
                            !cat_mimicAppearsAlliedTo(ship, universe.curPlayerPtr)) continue;
                        cat_shipSetDisableReason(ship, 1, 7);
                    }
                }
            }
            else if (target->header.objtype == OBJ_DerelictType)
            {
                if (playerSpecific) continue;
            }
            else if (target->header.objtype == OBJ_MissileType)
            {   // CAT: a mine (not in mission 3's sensors manager before kasfShowMinesInSensors), not hidden by FOW
                if (((Missile *)target)->missileType != MISSILE_Mine) continue;
                if (singlePlayerGame && singlePlayerGameInfo.currentMission == 3 && smSensorsActive &&
                    !catSmShowMines) continue;
                if (playerSpecific) continue;
                if (cat_selPositionHiddenByFOW(&target->obj.posinfo.position, FALSE)) continue;
            }
            else continue;
        }
        if(target->header.objtype == OBJ_ShipType)     //later optimize if ONLY ships are cloaked!
        {
            Ship* ship = (Ship*)target;
            // CAT: in the sensors manager an enemy ship (not a crystal, not dEcclesiamInfected) must be selectable
            //      in the player's fog of war (cat_selShipSelectableInFOW)
            if (smSensorsActive && !allianceArePlayersAllied(ship->playerowner, universe.curPlayerPtr) &&
                !cat_shipIsCrystal(ship) && universe.curPlayerPtr->playerState != PLAYER_DEAD &&
                universe.curPlayerPtr->catFOWGrid != NULL && ship->shiptype != dEcclesiamInfected &&
                !cat_selShipSelectableInFOW(ship)) continue;
            // CAT: a fully cloaked enemy ship the player has not detected is skipped (selRectDragFunction 0x005053a0:
            //      cat_shipIsCloakedFully, sub_53F9F0); HW1 asked the proximity sensors
            if (cat_shipIsCloakedFully(ship) &&          // target is cloaked
                ship->playerowner != universe.curPlayerPtr && // ship isn't players so don't draw box, unless...
                !cat_shipDetectedByPlayer(ship, universe.curPlayerPtr->playerIndex)) {
                continue;
            }
            // CAT: an enemy mimic posing as a resource or derelict only with selectAnything
            if (ship->playerowner != universe.curPlayerPtr && bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic) &&
                (cat_mimicAppearsAsResource(ship, universe.curPlayerPtr) ||
                 cat_mimicAppearsAsDerelict(ship, universe.curPlayerPtr)) && !selectAnything) continue;
            if (bAttack && ship->staticinfo->cannotForceAttackIfOwnShip &&
                (ship->playerowner == universe.curPlayerPtr)) {
                // don't allow anysort of attacking of own cryotrays
                continue;
            }
            // CAT: not the big gun's units nor an untargetable ship when attacking
            if (bAttack && (ship->shiptype == dBigGunUnit || cat_shipIsUntargetable(ship))) continue;
        }
        else if (target->header.objtype == OBJ_AsteroidType)
        {
            // CAT: only with selectAnything; HW1 also took killer-collision asteroids for an attack
            if (playerSpecific || !selectAnything) continue;
        }
        else if (target->header.objtype == OBJ_MissileType)
        {
            if (((Missile *)target)->missileType != MISSILE_Mine) continue;
            if (singlePlayerGame && singlePlayerGameInfo.currentMission == 3 && smSensorsActive &&
                !catSmShowMines) continue;
            if (playerSpecific) continue;
            if (smSensorsActive && cat_selPositionHiddenByFOW(&target->obj.posinfo.position, FALSE)) continue;
        }
        else continue;                                      // CAT: derelicts and the rest are not band-boxed

        if (centrePoint)
        {                                                   // CAT: the centre-point test
            if (!(rect.x0 <= target->impactable.collInfo.selCircleX && target->impactable.collInfo.selCircleX <= rect.x1 &&
                  rect.y0 <= target->impactable.collInfo.selCircleY && target->impactable.collInfo.selCircleY <= rect.y1))
            {
                continue;
            }
        }
        else
        {
        //here we do a test of the overlap of the selection rect
        //and a rectangle representing the target's selection sphere.
        selectRect.x0 = target->impactable.collInfo.selCircleX - target->impactable.collInfo.selCircleRadius - dragMargin;
        selectRect.x1 = target->impactable.collInfo.selCircleX + target->impactable.collInfo.selCircleRadius + dragMargin;
        selectRect.y0 = (target->impactable.collInfo.selCircleY - target->impactable.collInfo.selCircleRadius) - dragMargin;
        selectRect.y1 = (target->impactable.collInfo.selCircleY + target->impactable.collInfo.selCircleRadius) + dragMargin;
        if (target->impactable.collInfo.precise != NULL &&
            target->obj.currentLOD <= target->staticinfo->common.staticCollInfo.preciseSelection)
        {                                               //if we should perform precise collision checking
            point = target->impactable.collInfo.precise->worldRectPos;//list of points
            pIndex = target->impactable.collInfo.precise->corner;    //list of vertices
            areaUnion = areaTotal = 0.0f;
            for (index = base = 0; index < target->impactable.collInfo.precise->nPolys; index++, base += 4)
            {                                           //for each poly
                val = min(point[pIndex[base + 0]].x, min(point[pIndex[base + 1]].x, min(point[pIndex[base + 2]].x, point[pIndex[base + 3]].x)));
                polyRect.x0 = val;
                val = max(point[pIndex[base + 0]].x, max(point[pIndex[base + 1]].x, max(point[pIndex[base + 2]].x, point[pIndex[base + 3]].x)));
                polyRect.x1 = val;
                val = max(point[pIndex[base + 0]].y, max(point[pIndex[base + 1]].y, max(point[pIndex[base + 2]].y, point[pIndex[base + 3]].y)));
                polyRect.y1 = val;
                val = min(point[pIndex[base + 0]].y, min(point[pIndex[base + 1]].y, min(point[pIndex[base + 2]].y, point[pIndex[base + 3]].y)));
                polyRect.y0 = val;
                areaTotal += (polyRect.x1 - polyRect.x0) * (polyRect.y1 - polyRect.y0);
                primRealRectUnion2(&unionRect, &polyRect, &rect);
                areaUnion += (unionRect.x1 - unionRect.x0) * (unionRect.y1 - unionRect.y0);
            }
        }
        else
        {
            primRealRectUnion2(&unionRect, &selectRect, &rect);
            areaTotal = ((selectRect.x1 - selectRect.x0) *
                                 (selectRect.y1 - selectRect.y0));
            areaUnion = ((unionRect.x1 - unionRect.x0) *
                                 (unionRect.y1 - unionRect.y0));
        }
        if (areaUnion / areaTotal < selBandBoxInsideIn)
        {
            continue;
        }
        }
        //if enough of target inside selection box (or its centre inside it)
        if (*destCount < COMMAND_MAX_SHIPS)
        {
            // CAT: a component-class ship is its clamp host; a target already in the list is not added again
            if (target->header.objtype == OBJ_ShipType && ((Ship *)target)->staticinfo->shipclass == CLASS_Component &&
                ((Ship *)target)->guidance.clampInfo != NULL && ((Ship *)target)->guidance.clampInfo->host != NULL)
            {
                target = ((Ship *)target)->guidance.clampInfo->host;
            }
            for (index = 0; index < *destCount; index++)
            {
                if (destList[index] == target)
                {
                    break;
                }
            }
            if (index < *destCount)
            {
                continue;
            }
            destList[*destCount] = target;            //add it to selected list
            (*destCount)++;
        }
        else
        {
            dbgMessage("Warning: Tried to select too many ships");
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : selRectDragAddFunction
    Description : Update a list of ships being selected based upon
                    specified rectangle and camera.
    Inputs      : rect - rectangle in viewport space which the user is dragging.
                  camera - which viewport to select in
                  centrePoint - CAT: passed to selRectDragFunction (0x00505cb0's fourth argument)
    Outputs     : Adds all ships in rectangle to current selection.
                  Also updates the selCentrePoint vector.
    Return      : void
----------------------------------------------------------------------------*/
void selRectDragAddFunction(Node *startNode, Camera *camera, rectangle *rect, bool32 centrePoint)
{
    sdword index, selectCount;
    MaxSelection selection;

    //do a selection into a temporary selection
    selRectDragFunction(startNode, camera, rect, (SpaceObjRotImpTarg **)selection.ShipPtr, &selection.numShips, TRUE, FALSE, FALSE, centrePoint);
    //see if we should be adding or removing these ships
    for (selectCount = index = 0; index < selection.numShips; index++)
    {
        if (selShipInSelection(selSelected.ShipPtr,
                selSelected.numShips, selection.ShipPtr[index]) != FALSE)
        {
            selectCount++;
        }
    }
    //add in or remove the ships from selection.  Warning!!! this is a order n * m search
    if (selectCount == selection.numShips)
    {                                                       //if all ships selected are already selected, remove them
        for (index = 0; index < selection.numShips; index++)
        {
            selSelectionRemoveSingleShip(&selSelected, selection.ShipPtr[index]);
        }
    }
    else
    {                                                       //not all selected, add them
        for (index = 0; index < selection.numShips; index++)
        {
            selSelectionAddSingleShip(&selSelected, selection.ShipPtr[index]);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : selShipInSelection
    Description : Search for a ship in a particular selection
    Inputs      : shipList - list of ships to search
                  nShips - number of ships in list to search
                  ship - ship to search for a match of
    Outputs     : ..
    Return      : TRUE of ship found, false otherwise
----------------------------------------------------------------------------*/
sdword selShipInSelection(ShipPtr *shipList, sdword nShips, ShipPtr ship)
{
    sdword index;

    for (index = 0; index  < nShips; index++, shipList++)
    {
        if (ship == *shipList)
            return(TRUE);
    }
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : selSelectionSetSingleShip
    Description : Select a single ship
    Inputs      : ship - ship to select
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void selSelectionSetSingleShip(Ship *ship)
{
    dbgAssertOrIgnore(ship->playerowner == universe.curPlayerPtr);
    selSelected.numShips = 1;
    selSelected.ShipPtr[0] = ship;
    selCentrePoint = ship->obj.posinfo.position;
}

/*-----------------------------------------------------------------------------
    Name        : selSelectionAddSingleShip
    Description : Add a single ship to current selection
    Inputs      : ship - ship to add to selection
    Outputs     : adds to the selSelected list if not already there
    Return      : void
----------------------------------------------------------------------------*/
void selSelectionAddSingleShip(MaxSelection *dest, Ship *ship)
{
    sdword index;

//    dbgAssertOrIgnore(dest->numShips < COMMAND_MAX_SHIPS - 1);
    if (dest->numShips >= COMMAND_MAX_SHIPS)
    {
        dbgAssertOrIgnore(dest->numShips <= COMMAND_MAX_SHIPS);
        dbgMessage("Warning: Tried to add to many ships to selection");
        return;
    }

    for (index = 0; index < dest->numShips; index++)
    {
        if (dest->ShipPtr[index] == ship)
            return;
    }
    dest->ShipPtr[dest->numShips] = ship;
    dest->numShips++;
}

/*-----------------------------------------------------------------------------
    Name        : selSelectionRemoveSingleShip
    Description : Remove a selected ship from selSelected, if it is even there
    Inputs      :
    Outputs     : Decrements selSelected.numTargets and may move entire rest of
                    list back one to fill in the hole.
    Return      :
----------------------------------------------------------------------------*/
void selSelectionRemoveSingleShip(MaxSelection *dest, Ship *ship)
{
    sdword index;

    for (index = 0; index < dest->numShips; index++)
    {
        if (dest->ShipPtr[index] == ship)              //if ship in selected list
        {
            for (; index < dest->numShips - 1; index++)
            {                                               //move rest of list back 1
                dest->ShipPtr[index] = dest->ShipPtr[index + 1];
            }
            dest->numShips--;                       //one less ship
            return;                                         //all done
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : selSelectionClick
    Description : Select the closest ship the player has clicked on.
    Inputs      : listHead - where to start scanning from
                  camera - specific camera/mission sphere
                  x, y - screen pixel location of the click
                  bIncludeDerelicts - TRUE to include derelicts in click selection possibilities
                  bIncludeResources - TRUE to include resources in click selection possibilities
    Outputs     : ..
    Return      : closest ship clicked on or NULL if none under cursor
    Note        : A margin around each ship's selection sphere is added to
                    permit clicking very distant ships.
----------------------------------------------------------------------------*/
// CAT: selSelectionClick 0x00505ed0 has a seventh argument, pickNothing: when nonzero no object is picked (the exe
//      tests it after the cloak test of every object; name chosen here). Positions round to the nearest pixel.
Ship *selSelectionClick(Node *listHead, Camera *camera, sdword x, sdword y, bool32 bIncludeDerelicts, bool32 bIncludeResources, bool32 pickNothing)
{
    Node *shipnode;
    Ship *ship;
    rectangle selectRect;
    sdword closestDistance = SDWORD_Max, distance;
    Ship *closestShip = NULL;
    vector *point;
    ubyte *pIndex;
    sdword index, base;
    real32 xReal, yReal;
    real32 p0, p1, p2, p3;

    dbgAssertOrIgnore(camera != NULL);                              //verify parameters

    //this system can later be optimised by only walking through sorted porions
    //of the object list.  However, for now we will walk the entire list.
    shipnode = listHead;
    while (shipnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(shipnode);
        if (ship->header.objtype == OBJ_ShipType)
        {
            // CAT: a selectable ship, or an untargetable one, a crystal or dEcclesiamInfected (HW1 took every ship
            //      but its drones)
            if (!bitTest(ship->header.flags, SOF_Selectable) && !cat_shipIsUntargetable(ship) &&
                !cat_shipIsCrystal(ship) && ship->shiptype != dEcclesiamInfected)
            {
                goto nextship;
            }
        }
        else if (bIncludeResources && (ship->header.objtype == OBJ_AsteroidType || ship->header.objtype == OBJ_DustType))
        {
            ;                                               //good
        }
        else if (bIncludeDerelicts && ship->header.objtype == OBJ_DerelictType)
        {
            ;                                               //good
        }
        else
        {                                                   //not anything we want
            goto nextship;                                  //skip this one
        }
        // CAT: fully cloaked and not detected by the player (selSelectionClick 0x00505ed0: cat_shipIsCloakedFully,
        //      sub_53F9F0); HW1 asked the proximity sensors
        if(cat_shipIsCloakedFully(ship) && ship->playerowner != universe.curPlayerPtr)
        {                                                   //ship is cloaked and isn't players so ignore it
            if(!cat_shipDetectedByPlayer(ship, universe.curPlayerPtr->playerIndex))
            {
                goto nextship;
            }
        }
        if (pickNothing)
        {
            goto nextship;
        }
        if ((ship->impactable.collInfo.selCircleRadius > 0) && ((ship->header.flags & SOF_Dead) == 0))   //and it is in front of the camera
        {
            //here we do a test of the overlap of the selection rect
            //and a rectangle representing the ship's selection sphere.
            if (ship->impactable.collInfo.precise != NULL &&
                ship->obj.currentLOD <= ship->staticinfo->common.staticCollInfo.preciseSelection)
            {                                               //if we should perform precise collision checking
                point = ship->impactable.collInfo.precise->worldRectPos;//list of points
                pIndex = ship->impactable.collInfo.precise->corner;    //list of vertices
                xReal = primScreenToGLX(x);                 //floating-point location of points
                yReal = primScreenToGLY(y);
                for (index = base = 0; index < ship->impactable.collInfo.precise->nPolys; index++, base += 4)
                {                                           //for each poly
                    p0 = primPointLineIntersection(xReal, yReal, point[pIndex[base + 0]].x, point[pIndex[base + 0]].y, point[pIndex[base + 1]].x, point[pIndex[base + 1]].y);
                    p1 = primPointLineIntersection(xReal, yReal, point[pIndex[base + 1]].x, point[pIndex[base + 1]].y, point[pIndex[base + 2]].x, point[pIndex[base + 2]].y);
                    p2 = primPointLineIntersection(xReal, yReal, point[pIndex[base + 2]].x, point[pIndex[base + 2]].y, point[pIndex[base + 3]].x, point[pIndex[base + 3]].y);
                    p3 = primPointLineIntersection(xReal, yReal, point[pIndex[base + 3]].x, point[pIndex[base + 3]].y, point[pIndex[base + 0]].x, point[pIndex[base + 0]].y);
                    if (p0 > 0.0f && p1 > 0.0f && p2 > 0.0f && p3 > 0.0f)
                    {                                       //if point inside this poly
                        distance = ABS(catSelScreenX(ship->impactable.collInfo.selCircleX) - x) +
                            ABS(catSelScreenY(ship->impactable.collInfo.selCircleY) - y) +
                            catSelScreenScaleX(ship->impactable.collInfo.selCircleDepth);
                        if (distance < closestDistance)
                        {
                            closestDistance = distance;
                            closestShip = ship;
                        }
                        break;
                    }
                }
            }
            else
            {
                selectRect.x0 = catSelScreenX(ship->impactable.collInfo.selCircleX - ship->impactable.collInfo.selCircleRadius) - selClickMargin;
                selectRect.x1 = catSelScreenX(ship->impactable.collInfo.selCircleX + ship->impactable.collInfo.selCircleRadius) + selClickMargin;
                selectRect.y0 = catSelScreenY(ship->impactable.collInfo.selCircleY + ship->impactable.collInfo.selCircleRadius) - selClickMargin;
                selectRect.y1 = catSelScreenY(ship->impactable.collInfo.selCircleY - ship->impactable.collInfo.selCircleRadius) + selClickMargin;

                //note that this is a rectangular intersection test
                if ((selectRect.x0 <= x) && (x <= selectRect.x1) &&
                    (selectRect.y0 <= y) && (y <= selectRect.y1))
                {                                           //found a matching object, return
                    distance = ABS(catSelScreenX(ship->impactable.collInfo.selCircleX) - x) +
                        ABS(catSelScreenY(ship->impactable.collInfo.selCircleY) - y) +
                            catSelScreenScaleX(ship->impactable.collInfo.selCircleDepth);
                    if (distance < closestDistance)
                    {
                        closestDistance = distance;
                        closestShip = ship;
                    }
    //                return(ship);
                }
            }
        }
        else if (catSelScreenX(ship->impactable.collInfo.selCircleX) == mouseCursorXPosition &&
                 catSelScreenY(ship->impactable.collInfo.selCircleY) == mouseCursorYPosition)
        {   // CAT: a zero-radius (or dead) object right under the cursor pixel is taken, at the best distance so far
            closestShip = ship;
        }
nextship:
        shipnode = shipnode->next;
    }

    // CAT: a component-class ship picks its clamp host (selSelectionClick 0x00505ed0)
    if (closestShip != NULL && closestShip->header.objtype == OBJ_ShipType &&
        closestShip->staticinfo->shipclass == CLASS_Component && closestShip->guidance.clampInfo != NULL &&
        closestShip->guidance.clampInfo->host != NULL)
    {
        return (Ship *)closestShip->guidance.clampInfo->host;
    }
    return(closestShip);
}

/*-----------------------------------------------------------------------------
    Name        : selClickFromArray
    Description : Perform a mouse click from an array of spaceobj pointers
                    instead of a linked list.
    Inputs      : list - list of object pointers
                  length - number of pointers in the array
                  x, y - location of mouse click
    Outputs     :
    Return      : selected object, or NULL
----------------------------------------------------------------------------*/
SpaceObj* selClickFromArray(SpaceObj** list, sdword length, sdword x, sdword y) {
    SpaceObj* object;
    rectangle selectRect;
    sdword closestDistance = SDWORD_Max, distance;
    SpaceObj* closestObject = NULL;

    for (; length > 0; list++, length--) {
        object = *list;
        udword flags = object->header.flags;
        if (bitTest(flags, SOF_Selectable | SOF_Targetable) == 0) continue;
        if (object->header.objtype == OBJ_ShipType) {
            Ship* ship = (Ship*)object;
            // CAT: as selSelectionClick 0x00505ed0 (cat_shipIsCloakedFully, sub_53F9F0); HW1 asked the proximity sensors
            if (cat_shipIsCloakedFully(ship) && ship->playerowner != universe.curPlayerPtr &&
                !cat_shipDetectedByPlayer(ship, universe.curPlayerPtr->playerIndex)) {
                // object is cloaked and isn't players so ignore it
                continue;
            }
        }
        // Object is at least targetable, so it is guaranteed to be impactable
        CollInfo* collInfo = &((SpaceObjRotImp*)object)->collInfo;
        if ((collInfo->selCircleRadius > 0) && (flags & SOF_Dead) == 0) // and it is in front of the camera
        {
            // here we do a test of the overlap of the selection rect
            // and a rectangle representing the object's selection sphere.
            selectRect.x0 = primGLToScreenX(collInfo->selCircleX - collInfo->selCircleRadius) - selClickMargin;
            selectRect.x1 = primGLToScreenX(collInfo->selCircleX + collInfo->selCircleRadius) + selClickMargin;
            selectRect.y0 = primGLToScreenY(collInfo->selCircleY + collInfo->selCircleRadius) - selClickMargin;
            selectRect.y1 = primGLToScreenY(collInfo->selCircleY - collInfo->selCircleRadius) + selClickMargin;
            //!!! note that this is a rectangular intersection test
            if ((selectRect.x0 <= x) && (x <= selectRect.x1) && (selectRect.y0 <= y) &&
                (y <= selectRect.y1)) { // found a matching object, return
                distance = ABS(primGLToScreenX(collInfo->selCircleX) - x) +
                           ABS(primGLToScreenY(collInfo->selCircleY) - y); // manhattan distance
                if (distance < closestDistance) {
                    closestDistance = distance;
                    closestObject = object;
                }
                //  return(object);
            }
        }
    }
    return (closestObject);
}

//I know I shouldn't be making external references like this but hey, Falko put
//these in mouse.c where they shouldn't be in the first place.  And hey, 2
//wrongs make a right, right? (Luke)
// um... sorry... (Falko)
extern mouseCursor  mouseCursorType;
extern udword       mouseCursorMode;
extern MaxSelection mouseCursorSelect;
extern real32       mouseDoubleClickTime;
extern real32       mouseDoubleClickExpire;
extern real32      mouseCursorOverDistance;
extern sdword      mouseCursorOverShipType;
extern real32      mouseCursorTextExpire;
extern SpaceObj    *mouseCursorObjPtr;
extern ubyte        mouseCursorOverLODs;
extern sdword       mouseCursorEnemyType;
extern sdword       mouseCursorEnemyPlayer;

/*-----------------------------------------------------------------------------
    Name        : selShipUnderMouse
    Description : Handle the case of an object being under the mouse
    Inputs      : target - what object is under the mouse
                  distance - distance to centre of selection circle
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void selShipUnderMouse(SpaceObjRotImpTarg *target, sdword distance)
{
#if PIE_MOVE_NEARTO
    //process object for move near to command
    if (piePointSpecMode == PSM_XY || piePointSpecMode == PSM_Z)
    {                                                       //if in point spec mode
        if ((target->header.objtype == OBJ_ShipType && ((Ship*)target)->staticinfo->shipclass != CLASS_Mothership)  ||   // CAT: by class (block is compiled out: PIE_MOVE_NEARTO 0)
            (target->header.objtype == OBJ_AsteroidType/* &&
             target->staticinfo->common.staticCollInfo.collspheresize > selAsteroidMoveNeartoSize*/))
        {                                                   //if it's a valid object type
            if (distance < smClosestDistance)
            {
                selClosestDistance = distance;
                selClosestTarget = target;
            }
        }
    }
#endif
    //process object for mouse-cursor action
    // only the closest ship gets named
    if (mrHoldRight == mrNULL)
    {                                                       //if no mouse action going down
        if (target->obj.cameraDistanceSquared < mouseCursorOverDistance)
        {                                                   //if closer than previous close object
            mouseCursorOverDistance = target->obj.cameraDistanceSquared;

            if (target->header.objtype == OBJ_ShipType)
            {
                Ship* targetShip = (Ship*)target;
                // enemy ships are only identified as "enemy ship"
                if ((targetShip->playerowner == universe.curPlayerPtr) ||
                    (targetShip->shipOrDerelict.specialFlags & SPECIAL_FriendlyStatus) &&
                        (tutorial != TUTORIAL_ONLY)) // ships with special friendly status are still considered enemies
                                                     // in the tutorial
                {
                    mouseCursorOverShipType = targetShip->shiptype;
                    mouseCursorObjPtr = &targetShip->obj;
                    bitSet(mouseCursorMode, MCM_CursorOverObj);
                } else {
                    //the trader ship refuses not to be buggy in these "general" cases,
                    //so I'll just put special case code for the trader, seeing as it's
                    //a special case anyway.
                    // CAT: the exe keeps the trader special case for trFloatingCity (selShipUnderMouse 0x00506550,
                    //      shiptype 0x62)
                    if (targetShip->shiptype == trFloatingCity)
                    {
                        mouseCursorOverShipType = trFloatingCity;
                        mouseCursorObjPtr       = NULL;
                        bitClear(mouseCursorMode, MCM_CursorOverObj);
                    }
                    else if (allianceIsShipAlly(targetShip, universe.curPlayerPtr))
                    {
                        mouseCursorOverShipType = MC_ALLIED_SHIP;
                        mouseCursorObjPtr       = NULL;
                        bitClear(mouseCursorMode, MCM_CursorOverObj);
                    }
                    else
                    {
                        mouseCursorOverShipType = MC_ENEMY_SHIP;
                        mouseCursorObjPtr       = NULL;
                        bitClear(mouseCursorMode, MCM_CursorOverObj);
                    }
                }
                mouseCursorOverLODs     = target->obj.renderedLODs;
                mouseCursorEnemyType    = targetShip->shiptype;
                mouseCursorEnemyPlayer  = targetShip->playerowner->playerIndex;
                mouseCursorTextExpire   = universe.totaltimeelapsed + TW_CURSORTEXT_DELAY_TIME;
            }
            else if (target->header.objtype == OBJ_DerelictType)
            {
                //since this is only used for salvaging,
                //set shiptype only when a salvageable derelict is under the cursor
/*                switch (((Derelict *)target)->derelicttype)
                {
                    case AngelMoon:
                    case AngelMoon_clean:
                    case Homeworld:
                    case PlanetOfOrigin:
                    case PlanetOfOrigin_scarred:
                    case Scaffold:
                    case Scaffold_scarred:
                    case ScaffoldFingerA_scarred:
                    case ScaffoldFingerB_scarred:
                    case HyperspaceGate:
                        //Don't salvage these
                        break;
                    default:
                        mouseCursorOverShipType = MC_DERELICT;
                        mouseCursorObjPtr       = &target->obj;
                        bitSet(mouseCursorMode, MCM_CursorOverObj);
                        break;
                }*/
                Derelict* derelict = (Derelict*)target;
                if ((derelict->derelicttype == HyperspaceGate) || derelict->staticinfo->salvageable) {
                    mouseCursorOverShipType = MC_DERELICT;
                    mouseCursorObjPtr = &derelict->obj;
                    bitSet(mouseCursorMode, MCM_CursorOverObj);
                }
            }
            else
            {
                // CAT: every resource is a cursor object (selShipUnderMouse 0x00506550); HW1 took dust clouds only
                //      while one of its resource-collector ships was selected
                {
                    mouseCursorOverShipType = MC_RESOURCE;
                    mouseCursorObjPtr = &target->obj;
                    bitSet(mouseCursorMode, MCM_CursorOverObj);
                }
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : selCircleCompute
    Description : Compute on-screen size and location of the selection circle
                    for specified ship.
    Inputs      : modelView, projection - matrices specifying the model/view
                    and projection matrices ship is drawn with.
                  target - target (ship, asteroid, etc.) to compute selection circle for.
    Outputs     : stores computed circle in Ship structure
    Return      : void
----------------------------------------------------------------------------*/
void selCircleCompute(hmatrix *modelView, hmatrix *projection, SpaceObjRotImpTarg *target)
{
    hvector centre, cameraSpace, screenSpace, radius, radiusProjected, preciseCorner[8];
    Ship *master, *slave;
    Node *slavenode;
    vector distvec;
    real32 dist;
    sdword distance;
    rectangle selectRect;
    PreciseSelection *precise;
    CollInfo *collision;
    vector v0, v1, cross;
    vector *point;
    ubyte *pIndex;
    sdword index, base;
    real32 xReal, yReal;
    real32 p0, p1, p2, p3;

    collision = &target->impactable.collInfo;

    // CAT (M2-R5 layout): HW1's slaveable branch (a master's circle around its slaves) is gone with the slave trade

    centre.x = collision->collPosition.x;                    //vector at centre of target
    centre.y = collision->collPosition.y;
    centre.z = collision->collPosition.z;
    centre.w = 1.0f;
    hmatMultiplyHMatByHVec(&selCameraSpace, modelView, &centre);//in camera space
    radius = selCameraSpace;                                   //project another point to determine selection sphere size
    radius.x += (target->staticinfo->common.staticCollInfo.collspheresize);
    hmatMultiplyHMatByHVec(&screenSpace, projection, &selCameraSpace);//in screen space
    hmatMultiplyHMatByHVec(&radiusProjected, projection, &radius);
    collision->selCircleX = screenSpace.x / screenSpace.w;
    collision->selCircleY = screenSpace.y / screenSpace.w;
    collision->selCircleDepth = (-selCameraSpace.z) / CAMERA_MAX_ZOOMOUT_DISTANCE * selDepthSelectMultiplier;
    collision->selCircleRadius = (radiusProjected.x - screenSpace.x) / screenSpace.w;

    //compute the precise selection if the ship type demands it
    if (collision->precise != NULL)
    {
        if (target->obj.currentLOD <= target->staticinfo->common.staticCollInfo.preciseSelection)
        {                                                   //if we should do precise selection
            precise = collision->precise;
            point = precise->worldRectPos;

            //transform the corners into camera space and project into screen space
            for (index = 0; index < 8; index++)
            {
                centre.x = collision->rectpos[index].x + target->obj.posinfo.position.x;//make a hvector (w is already 1.0f)
                centre.y = collision->rectpos[index].y + target->obj.posinfo.position.y;
                centre.z = collision->rectpos[index].z + target->obj.posinfo.position.z;
                hmatMultiplyHMatByHVec(&cameraSpace, modelView, &centre);//transform collision rectangle into world space
                hmatMultiplyHMatByHVec(&screenSpace, projection, &cameraSpace);//transform into screen space
                point[index].x = screenSpace.x / screenSpace.w;
                point[index].y = screenSpace.y / screenSpace.w;
                preciseCorner[index].x = screenSpace.x / screenSpace.w;
                preciseCorner[index].y = screenSpace.y / screenSpace.w;
                preciseCorner[index].z = screenSpace.z / screenSpace.w;
            }

            //now we have all points in screen space, let's find what polys point toward camera
            index = 0;                                      //init the corner index to zero
            precise->nPolys = 0;
            pIndex = precise->corner;
            //top poly (4567)
            if (preciseCorner[4].z > 0.0f && preciseCorner[5].z > 0.0f
                && preciseCorner[6].z > 0.0f && preciseCorner[7].z > 0.0f)
            {
                vecSub(v0, preciseCorner[5], preciseCorner[4]);
                vecSub(v1, preciseCorner[7], preciseCorner[4]);
                vecCrossProduct(cross, v0, v1);             //point up from ship
                if (cross.z > 0.0f)
                {                                           //if cross product camera facing
                    pIndex[index + 0] = 4;
                    pIndex[index + 1] = 5;
                    pIndex[index + 2] = 6;
                    pIndex[index + 3] = 7;
                    index += 4;
                    precise->nPolys++;
                }
            }
            //bottom poly (0321)
            if (preciseCorner[0].z > 0.0f && preciseCorner[3].z > 0.0f
                && preciseCorner[2].z > 0.0f && preciseCorner[1].z > 0.0f)
            {
                vecSub(v0, preciseCorner[1], preciseCorner[0]);
                vecSub(v1, preciseCorner[3], preciseCorner[0]);
                vecCrossProduct(cross, v1, v0);             //point down from ship
                if (cross.z > 0.0f)
                {                                           //if cross product camera facing
                    pIndex[index + 0] = 0;
                    pIndex[index + 1] = 3;
                    pIndex[index + 2] = 2;
                    pIndex[index + 3] = 1;
                    index += 4;
                    precise->nPolys++;
                }
            }
            //front poly (7623)
            if (preciseCorner[7].z > 0.0f && preciseCorner[6].z > 0.0f
                && preciseCorner[2].z > 0.0f && preciseCorner[3].z > 0.0f)
            {
                vecSub(v0, preciseCorner[6], preciseCorner[7]);
                vecSub(v1, preciseCorner[3], preciseCorner[7]);
                vecCrossProduct(cross, v0, v1);             //point forward from ship
                if (cross.z > 0.0f)
                {                                           //if cross product camera facing
                    pIndex[index + 0] = 7;
                    pIndex[index + 1] = 6;
                    pIndex[index + 2] = 2;
                    pIndex[index + 3] = 3;
                    index += 4;
                    precise->nPolys++;
                }
            }
            //back poly (4015)
            if (preciseCorner[4].z > 0.0f && preciseCorner[0].z > 0.0f
                && preciseCorner[1].z > 0.0f && preciseCorner[5].z > 0.0f)
            {
                vecSub(v0, preciseCorner[5], preciseCorner[4]);
                vecSub(v1, preciseCorner[0], preciseCorner[4]);
                vecCrossProduct(cross, v1, v0);             //point backward from ship
                if (cross.z > 0.0f)
                {                                           //if cross product camera facing
                    pIndex[index + 0] = 4;
                    pIndex[index + 1] = 0;
                    pIndex[index + 2] = 1;
                    pIndex[index + 3] = 5;
                    index += 4;
                    precise->nPolys++;
                }
            }
            //left poly (7304)
            if (preciseCorner[7].z > 0.0f && preciseCorner[3].z > 0.0f
                && preciseCorner[0].z > 0.0f && preciseCorner[4].z > 0.0f)
            {
                vecSub(v0, preciseCorner[4], preciseCorner[7]);
                vecSub(v1, preciseCorner[3], preciseCorner[7]);
                vecCrossProduct(cross, v1, v0);             //point left from ship
                if (cross.z > 0.0f)
                {                                           //if cross product camera facing
                    pIndex[index + 0] = 7;
                    pIndex[index + 1] = 3;
                    pIndex[index + 2] = 0;
                    pIndex[index + 3] = 4;
                    index += 4;
                    precise->nPolys++;
                }
            }
            //right poly (6512)
            if (preciseCorner[6].z > 0.0f && preciseCorner[5].z > 0.0f
                && preciseCorner[1].z > 0.0f && preciseCorner[2].z > 0.0f)
            {
                vecSub(v0, preciseCorner[5], preciseCorner[6]);
                vecSub(v1, preciseCorner[2], preciseCorner[6]);
                vecCrossProduct(cross, v0, v1);             //point right from ship
                if (cross.z > 0.0f)
                {                                           //if cross product camera facing
                    pIndex[index + 0] = 6;
                    pIndex[index + 1] = 5;
                    pIndex[index + 2] = 1;
                    pIndex[index + 3] = 2;
                    index += 4;
                    precise->nPolys++;
                }
            }
            precise->nPolys = (ubyte)min(precise->nPolys, 3);
            //find the target under the mouse cursor for the move near to command/mouse cursor text
            xReal = primScreenToGLX(mouseCursorX());        //floating-point location of mouse
            yReal = primScreenToGLY(mouseCursorY());
            for (index = base = 0; index < target->impactable.collInfo.precise->nPolys; index++, base += 4)
            {                                               //for each poly
                p0 = primPointLineIntersection(xReal, yReal, point[pIndex[base + 0]].x, point[pIndex[base + 0]].y, point[pIndex[base + 1]].x, point[pIndex[base + 1]].y);
                p1 = primPointLineIntersection(xReal, yReal, point[pIndex[base + 1]].x, point[pIndex[base + 1]].y, point[pIndex[base + 2]].x, point[pIndex[base + 2]].y);
                p2 = primPointLineIntersection(xReal, yReal, point[pIndex[base + 2]].x, point[pIndex[base + 2]].y, point[pIndex[base + 3]].x, point[pIndex[base + 3]].y);
                p3 = primPointLineIntersection(xReal, yReal, point[pIndex[base + 3]].x, point[pIndex[base + 3]].y, point[pIndex[base + 0]].x, point[pIndex[base + 0]].y);
                if (p0 > 0.0f && p1 > 0.0f && p2 > 0.0f && p3 > 0.0f)
                {                                       //if mouse inside this poly
                    if (target->impactable.collInfo.selCircleRadius > 0.0f)
                    {
                        distance = ABS(primGLToScreenX(target->impactable.collInfo.selCircleX) - mouseCursorX()) +
                            ABS(primGLToScreenY(target->impactable.collInfo.selCircleY) - mouseCursorY());
                        selShipUnderMouse(target, distance);
                    }
                    break;
                }
            }
        }
    }
    else
    {
        // CAT (M2-R5 layout): HW1 jumped here (checkUnderMouse) from the slaveable branch, which is gone
        selectRect.x0 = primGLToScreenX(collision->selCircleX - collision->selCircleRadius) - selClickMargin;
        selectRect.x1 = primGLToScreenX(collision->selCircleX + collision->selCircleRadius) + selClickMargin;
        selectRect.y0 = primGLToScreenY(collision->selCircleY + collision->selCircleRadius) - selClickMargin;
        selectRect.y1 = primGLToScreenY(collision->selCircleY - collision->selCircleRadius) + selClickMargin;

        if ((selectRect.x0 <= mouseCursorX()) && (mouseCursorX() <= selectRect.x1) &&
            (selectRect.y0 <= mouseCursorY()) && (mouseCursorY() <= selectRect.y1))
        {                                               //if under mouse
            distance = ABS(primGLToScreenX(collision->selCircleX) - mouseCursorX()) +
                ABS(primGLToScreenY(collision->selCircleY) - mouseCursorY());
            selShipUnderMouse(target, distance);

        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : selDrawBoxes
    Description : Draw the projected selection boxes if the target has a precise selection
    Inputs      : target - target to draw boxes for
    Outputs     :
    Return      :
    Note        : draws in 2d
----------------------------------------------------------------------------*/
#if SEL_DRAW_BOXES
void selDrawBoxes(SpaceObjRotImpTarg *target)
{
    vector *point;
    ubyte *pIndex;
    CollInfo *collision;

    collision = &target->impactable.collInfo;
    if (collision->precise != NULL)
    {
        if (target->obj.currentLOD <= target->staticinfo->common.staticCollInfo.preciseSelection)
        {                                                   //if we should do precise selection
            point = collision->precise->worldRectPos;
            pIndex = collision->precise->corner;

            if (collision->precise->nPolys >= 1)
            {
                fontPrint(primGLToScreenX((point[pIndex[0]].x + point[pIndex[2]].x) / 2.0f),
                           primGLToScreenY((point[pIndex[0]].y + point[pIndex[2]].y) / 2.0f),
                           colWhite, "1");
                primLine2(primGLToScreenX(point[pIndex[0]].x), primGLToScreenY(point[pIndex[0]].y),
                          primGLToScreenX(point[pIndex[1]].x), primGLToScreenY(point[pIndex[1]].y), colWhite);
                primLine2(primGLToScreenX(point[pIndex[1]].x), primGLToScreenY(point[pIndex[1]].y),
                          primGLToScreenX(point[pIndex[2]].x), primGLToScreenY(point[pIndex[2]].y), colWhite);
                primLine2(primGLToScreenX(point[pIndex[2]].x), primGLToScreenY(point[pIndex[2]].y),
                          primGLToScreenX(point[pIndex[3]].x), primGLToScreenY(point[pIndex[3]].y), colWhite);
                primLine2(primGLToScreenX(point[pIndex[3]].x), primGLToScreenY(point[pIndex[3]].y),
                          primGLToScreenX(point[pIndex[0]].x), primGLToScreenY(point[pIndex[0]].y), colWhite);
            }
/*
            dbgMessagef("%3.2f, %3.2f, %3.2f, %3.2f",
                       primPointLineIntersection(primScreenToGLX(mouseCursorX()), primScreenToGLY(mouseCursorY()),
                            point[pIndex[0]].x, point[pIndex[0]].y, point[pIndex[1]].x, point[pIndex[1]].y),
                       primPointLineIntersection(primScreenToGLX(mouseCursorX()), primScreenToGLY(mouseCursorY()),
                            point[pIndex[1]].x, point[pIndex[1]].y, point[pIndex[2]].x, point[pIndex[2]].y),
                       primPointLineIntersection(primScreenToGLX(mouseCursorX()), primScreenToGLY(mouseCursorY()),
                            point[pIndex[2]].x, point[pIndex[2]].y, point[pIndex[3]].x, point[pIndex[3]].y),
                       primPointLineIntersection(primScreenToGLX(mouseCursorX()), primScreenToGLY(mouseCursorY()),
                            point[pIndex[3]].x, point[pIndex[3]].y, point[pIndex[0]].x, point[pIndex[0]].y)
                       );
*/
            if (collision->precise->nPolys >= 2)
            {
                fontPrint(primGLToScreenX((point[pIndex[4]].x + point[pIndex[6]].x) / 2.0f),
                           primGLToScreenY((point[pIndex[4]].y + point[pIndex[6]].y) / 2.0f),
                           colWhite, "2");
                primLine2(primGLToScreenX(point[pIndex[4]].x), primGLToScreenY(point[pIndex[4]].y),
                          primGLToScreenX(point[pIndex[5]].x), primGLToScreenY(point[pIndex[5]].y), colWhite);
                primLine2(primGLToScreenX(point[pIndex[5]].x), primGLToScreenY(point[pIndex[5]].y),
                          primGLToScreenX(point[pIndex[6]].x), primGLToScreenY(point[pIndex[6]].y), colWhite);
                primLine2(primGLToScreenX(point[pIndex[6]].x), primGLToScreenY(point[pIndex[6]].y),
                          primGLToScreenX(point[pIndex[7]].x), primGLToScreenY(point[pIndex[7]].y), colWhite);
                primLine2(primGLToScreenX(point[pIndex[7]].x), primGLToScreenY(point[pIndex[7]].y),
                          primGLToScreenX(point[pIndex[4]].x), primGLToScreenY(point[pIndex[4]].y), colWhite);
            }

            if (collision->precise->nPolys >= 3)
            {
                fontPrint(primGLToScreenX((point[pIndex[8]].x + point[pIndex[10]].x) / 2.0f),
                           primGLToScreenY((point[pIndex[8]].y + point[pIndex[10]].y) / 2.0f),
                           colWhite, "3");
                primLine2(primGLToScreenX(point[pIndex[8]].x), primGLToScreenY(point[pIndex[8]].y),
                          primGLToScreenX(point[pIndex[9]].x), primGLToScreenY(point[pIndex[9]].y), colWhite);
                primLine2(primGLToScreenX(point[pIndex[9]].x), primGLToScreenY(point[pIndex[9]].y),
                          primGLToScreenX(point[pIndex[10]].x), primGLToScreenY(point[pIndex[10]].y), colWhite);
                primLine2(primGLToScreenX(point[pIndex[10]].x), primGLToScreenY(point[pIndex[10]].y),
                          primGLToScreenX(point[pIndex[11]].x), primGLToScreenY(point[pIndex[11]].y), colWhite);
                primLine2(primGLToScreenX(point[pIndex[11]].x), primGLToScreenY(point[pIndex[11]].y),
                          primGLToScreenX(point[pIndex[8]].x), primGLToScreenY(point[pIndex[8]].y), colWhite);
            }
        }
    }
}
#endif //SEL_DRAW_BOXES

/*-----------------------------------------------------------------------------
    Name        : selCircleComputeGeneral
    Description : Compute a selection circle like the previous function, just
                    without being speific to ships.
    Inputs      : modelView, projection - camera/modellig matrices
                  location - position of the sphere to project.
                  worldRadius - radius of sphere to project.
    Outputs     : destX,Y - screen location of projected circle.
                  destRadius - screen size of the projected circle.
    Return      : void
----------------------------------------------------------------------------*/
void selCircleComputeGeneral(hmatrix *modelView, hmatrix *projection, vector *location, real32 worldRadius, real32 *destX, real32 *destY, real32 *destRadius)
{
    hvector centre, screenSpace, radius, radiusProjected;
    centre.x = location->x;                                 //vector at centre of ship
    centre.y = location->y;
    centre.z = location->z;
    centre.w = 1.0f;

    hmatMultiplyHMatByHVec(&selCameraSpace, modelView, &centre);//in camera space
    radius = selCameraSpace;                                //project another point to determine selection sphere size
    radius.x += worldRadius;
    hmatMultiplyHMatByHVec(&screenSpace, projection, &selCameraSpace);//in screen space
    hmatMultiplyHMatByHVec(&radiusProjected, projection, &radius);
    *destX = screenSpace.x / screenSpace.w;
    *destY = screenSpace.y / screenSpace.w;
    *destRadius = (radiusProjected.x - screenSpace.x) / screenSpace.w;
}

#if DEBUG_COLLISION_SPHERES
/*-----------------------------------------------------------------------------
    Name        : selSelectionDraw0..5
    Description : Draw a particular ship's selection circle at a given
                    level-of-detail.
    Inputs      : ship - pointer to ship to draw about
                  radius - pre-converted integer radius
                  segments - number of segments to draw circles with
                  c - color to render health bar in
                  angle - angle of end of health bar
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
#define fixedSin45    ((udword)(0.7071067811865 * 65536.0))
void selSelectionDraw5(Ship *ship)      // PLEASE DON'T COMMENT THIS FUNCTION OUT - I need it for testing/debugging - Gary
{   //0 outline circles 1-pixel dim full health circle
    oval outline;
    sdword x, y;//, offset;
    sdword radius;
    sdword segments = 20;
    color c = selShipHealthGreen;

    x = primGLToScreenX(ship->impactable.collInfo.selCircleX);
    y = primGLToScreenY(ship->impactable.collInfo.selCircleY);
    radius = primGLToScreenScaleX(ship->impactable.collInfo.selCircleRadius);

    outline.centreX = x;                                    //draw inner outline circle
    outline.centreY = y;
    outline.radiusX = outline.radiusY = radius;
    primOvalArcOutline2(&outline, 0.0f, TWOPI, 1, segments, c);
}
#endif

/*-----------------------------------------------------------------------------
    CAT: the Cataclysm status bars (M2-R5 hud). selStatusDraw0-4 are thin wrappers over cat_selStatusRectCompute
    0x00507390 and cat_selStatusBarsDraw 0x00507bc0: one row per bar (health, the ACV EMP charge, resources, the Beast
    weapon, the Dreadnought cannon, the big gun, the cloak charge, the gravwell, the Sentinel field, the upgrade in
    progress), then the upgrade-pending dot, the disabled icon, the stealth-detect dots, the experience stars, the
    hot-key number and the ammo / docked count. HW1's per-LOD bodies (health/fuel bars, the "C" cloak letter) are gone.
----------------------------------------------------------------------------*/
// CAT: the bar geometry (exe data, not script-adjustable: no script table refers to them)
static real32 catSelBarWidthFactor = 1.6f;          // 0x00892ab8: bar width over the selection radius in pixels
static real32 catSelBarHeightFactor = 0.03125f;     // 0x00892abc: bar height over the selection radius in pixels
static real32 catSelBarMinWidth = 8.0f;             // 0x00892a08
static real32 catSelBarMinHeight = 2.0f;            // 0x00892a10
static sdword catSelBarXOffset = -2;                // 0x00892ac0
static sdword catSelBarWidthAdd = 4;                // 0x00892ac4
static real32 catSelBarSpacing = 0.0f;              // 0x00a57948: the gap between rows (bss, never written by the exe)
static real32 catSelUpgradePulsePeriod = 0.5f;      // 0x008929fc
static real32 catSelUpgradePulseStart = 0.0f;       // 0x00a5794c
static sdword catSelIconsSizeFactor = 2;            // 0x0072b0b4: experience-star size over the bar height
// CAT: the bar colours (exe constants)
static color catSelEMPColor = 0xfffc2732, catSelEMPDarkColor = 0xff9d020a;                 // 0x0072b098 / 0x0072b09c
static color catSelBeastColor = 0xffff28ff, catSelBeastDarkColor = 0xff640a64;             // 0x0072b090 / 0x0072b094
static color catSelDreadnoughtColor = 0xff3f8afc, catSelDreadnoughtDarkColor = 0xff0457d5; // 0x00892a18 / 0x00892a1c
static color catSelBigGunColor = 0xfffc2732, catSelBigGunDarkColor = 0xff7f020a;           // 0x0072b0a0 / 0x0072b0a4
static color catSelSentinelColor = 0xffff0000, catSelSentinelDarkColor = 0xffa00000;       // 0x0072b0b8 / 0x0072b0bc
static color catSelUpgradingColor = 0xffb5fdfa, catSelUpgradingDarkColor = 0xff047d77;     // 0x0072b0a8 / 0x0072b0ac
static color catSelUpgradePendingColor0 = 0xff00ffff, catSelUpgradePendingColor1 = 0x000000ff; // 0x0072b074 / 0x0072b078
static color catSelDetectedColor = 0xff0000ff, catSelUndetectedColor = 0xff00ff00;        // 0x0072b080 / 0x0072b084

// CAT: cat_selStatusRectCompute 0x00507390: the rectangle of a ship's top bar (x0 / x1 / y0 / y1, one bar high)
void cat_selStatusRectCompute(Ship *ship, sdword lod, rectangle *rect)
{
    sdword x, y, halfWidth, height;
    real32 radius, pixels, width, fheight;

    (void)lod;
    x = catSelScreenX(ship->impactable.collInfo.selCircleX);
    y = catSelScreenY(ship->impactable.collInfo.selCircleY);
    radius = (ship->impactable.collInfo.selCircleRadius > selMinSelectionRadius) ?
             ship->impactable.collInfo.selCircleRadius : selMinSelectionRadius;
    pixels = (real32)catSelScreenScaleX(radius);
    width = catSelBarWidthFactor * pixels;
    fheight = catSelBarHeightFactor * pixels;
    if (width < catSelBarMinWidth)
    {
        width = catSelBarMinWidth;
    }
    if (fheight < catSelBarMinHeight)
    {
        fheight = catSelBarMinHeight;
    }
    halfWidth = (sdword)(width * 0.5f);
    rect->x0 = catSelBarXOffset - halfWidth + x + 1;
    height = (sdword)fheight;
    rect->y0 = y - (sdword)(pixels + (real32)height);
    rect->x1 = (sdword)(width - (fheight + fheight)) + rect->x0 + catSelBarWidthAdd;
    rect->y1 = rect->y0 + height;
}

// CAT: cat_selStatusBarDraw 0x00507a10: one bar in row <row> under the top bar: the filled part and the backdrop
void cat_selStatusBarDraw(Ship *ship, sdword lod, sdword row, real32 *factor, color *c, color *dark)
{
    rectangle rect, full, empty;
    real32 f = *factor;
    sdword split, offset;

    if (f < 0.0f)
    {
        f = 0.0f;
    }
    else if (f > 1.0f)
    {
        f = 1.0f;
    }
    cat_selStatusRectCompute(ship, lod, &rect);
    catHudCounts.bars++;                                    // CAT: port diagnostic (the HUD self-test)
    // the exe multiplies on the x87 stack (fild (x1 - x0), fmul factor, __ftol at 0x00507a8e..0x00507a96): the product
    //      is not rounded to float first [audit: was a float product]
    split = (sdword)((double)(rect.x1 - rect.x0) * (double)f) + rect.x0;
    offset = (sdword)(((real32)(rect.y1 - rect.y0) + catSelBarSpacing) * (real32)row);
    full.x0 = rect.x0;
    full.x1 = split;
    full.y0 = rect.y0 + offset;
    full.y1 = rect.y1 + offset;
    empty.x0 = split;
    empty.x1 = rect.x1;
    empty.y0 = full.y0;
    empty.y1 = full.y1;
    primRectSolid2(&full, *c);
    primRectSolid2(&empty, *dark);
}

// CAT: cat_selHealthColorsGet 0x00507b00
void cat_selHealthColorsGet(real32 *factor, color *c, color *dark)
{
    if (*factor > selShipHealthGreenFactor)
    {
        *c = selShipHealthGreen;
        *dark = selShipHealthDarkGreen;
    }
    else if (*factor > selShipHealthYellowFactor)
    {
        *c = selShipHealthYellow;
        *dark = selShipHealthDarkYellow;
    }
    else
    {
        *c = selShipHealthRed;
        *dark = selShipHealthDarkRed;
    }
}

// CAT: cat_selHealthFactorGet 0x00507b70: health over the maximum, clamped to [0, 1]
void cat_selHealthFactorGet(Ship *ship, real32 *factor)
{
    *factor = ship->staticinfo->health.oneOverMaxHealth * ship->targetable.health;
    if (*factor < 0.0f)
    {
        *factor = 0.0f;
    }
    else if (*factor > 1.0f)
    {
        *factor = 1.0f;
    }
}

// CAT: cat_selStatusUpgradePendingDraw 0x005074f0: a pulsing dot right of the bars while an allied ship has an
//      upgrade waiting (weapon, armour or a pending flag)
void cat_selStatusUpgradePendingDraw(Ship *ship, sdword lod)
{
    rectangle rect;
    bool32 weapons[NUM_WEAPON_TYPES];
    sdword rad;
    real32 t;

    if (!allianceIsShipAlly(ship, universe.curPlayerPtr))
    {
        return;
    }
    if (!cat_shipGetPendingWeaponUpgrades(ship, weapons) && !cat_shipCanUpgradeArmour(ship) &&
        cat_shipGetPendingUpgradeFlags(ship) == 0)
    {
        return;
    }
    cat_selStatusRectCompute(ship, lod, &rect);
    rad = (rect.y1 - rect.y0) / 2;
    if (cat_taskRealTimeElapsed > catSelUpgradePulsePeriod + catSelUpgradePulseStart ||
        cat_taskRealTimeElapsed < catSelUpgradePulseStart)
    {
        catSelUpgradePulseStart = cat_taskRealTimeElapsed;
    }
    t = (cat_taskRealTimeElapsed - catSelUpgradePulseStart) / catSelUpgradePulsePeriod;
    if (t > 0.5f)
    {
        t = (real32)(((double)t - 1.0) * -2.0);
    }
    else
    {
        t = t + t;
    }
    t = (t > 0.5f) ? 1.0f : t + t;
    cat_primCircleSolidBlend2((rect.y1 - rect.y0) + rect.x1, rad + rect.y0, rad, 20,
                              cat_colBlendRGBA(catSelUpgradePendingColor0, catSelUpgradePendingColor1, t));
}

// CAT: cat_selStatusIconsDraw 0x00507630: one experience star per level (Ship.catExperienceLevel, +0x5F8) above the
//      bars
void cat_selStatusIconsDraw(Ship *ship, sdword lod)
{
    rectangle rect;
    sdword size, x, y, index;

    cat_selStatusRectCompute(ship, lod, &rect);
    size = (rect.y1 - rect.y0) * catSelIconsSizeFactor;
    y = rect.y0 - size;
    x = rect.x0;
    for (index = 0; index < ship->catExperienceLevel; index++)
    {
        cat_overlayIconDraw(CAT_OI_ExpStar, y, x, size, size, 0xffffffff);
        x += size;
    }
}

// CAT: cat_selStatusHotKeyDraw 0x005076a0: the hot-key group number right of the top bar, in the LOD's font
void cat_selStatusHotKeyDraw(Ship *ship, sdword lod)
{
    rectangle rect;
    fonthandle font;

    if (selAnyHotKeyTest(ship))
    {
        cat_selStatusRectCompute(ship, lod, &rect);
        font = (lod == 0) ? selGroupFont0 : (lod == 1) ? selGroupFont1 : (lod == 2) ? selGroupFont2 : selGroupFont3;
        fontMakeCurrent(font);
        fontPrint(rect.x1, rect.y0 - fontHeight(" "), selHotKeyNumberColor,
                  selHotKeyString[selHotKeyGroupNumberTest(ship)]);
    }
}

// CAT: cat_selStatusAmmoCountDraw 0x00507740: under the last bar, the missiles left (sDreadnought, sDestroyer,
//      tMissileDestroyer, tMinelayerCorvette, a linkable Acolyte), a hive's swarmers or the docked ship counts
void cat_selStatusAmmoCountDraw(Ship *ship, sdword lod, sdword barIndex)
{
    rectangle rect;
    sdword count, index, y;

    if (lod > 2)
    {
        return;
    }
    if (ship->shiptype == sDreadnought || ship->shiptype == sDestroyer || ship->shiptype == tMissileDestroyer ||
        ship->shiptype == tMinelayerCorvette || cat_acolyteCanLink(ship))
    {
        // the exe starts the count at -1 ("none") and adds every missile / mine launcher's missiles to it
        count = -1;
        for (index = 0; index < ship->gunInfo->numGuns; index++)
        {
            GunStatic *gunstatic = &ship->staticinfo->gunStaticInfo->gunstatics[index];
            if (gunstatic->guntype == GUN_MissileLauncher || gunstatic->guntype == GUN_MineLauncher)
            {
                count += ship->gunInfo->guns[index].numMissiles;
            }
        }
        if (cat_acolyteCanLink(ship))
        {
            count = cat_acolyteCanLinkFire(ship);
            if (count == 0)
            {
                count = -1;
            }
        }
        fontMakeCurrent(selDefaultFont);
        if (count != -1)
        {
            cat_selStatusRectCompute(ship, lod, &rect);
            fontPrintf(rect.x0, (rect.y1 - rect.y0) * (barIndex - 1) + rect.y1, selHotKeyNumberColor,
                       strGetString(strMissilesLeft), count);
        }
    }
    else if (ship->shipsInsideMe != NULL)
    {
        fontMakeCurrent(selDefaultFont);
        cat_selStatusRectCompute(ship, lod, &rect);
        y = (rect.y1 - rect.y0) * (barIndex - 1) + rect.y1;
        if (ship->shiptype == sHiveFrigate || ship->shiptype == bHiveFrigate)
        {
            fontPrintf(rect.x0, y, selHotKeyNumberColor, strGetString(strSwarmersInside),
                       ((HiveFrigateSpec *)ship->ShipSpecifics)->numSwarmers);
            return;
        }
        fontPrintf(rect.x0, y, selHotKeyNumberColor, strGetString(strFightersCorvettesDocked),
                   (sdword)ship->shipsInsideMe->FightersInsideme, (sdword)ship->shipsInsideMe->CorvettesInsideme);
        if (tutorial && tutPointerShip == ship && tutPointerShipHealthRect)
        {
            tutPointerShipHealthRect->y1 += 5;
        }
    }
}

// CAT: cat_selStatusStealthDetectDraw 0x00507920: for an allied stealth-capable type (ShipStaticInfo +0x1EC bit
//      0x20000), one dot per player under the bars: red where that player has detected it, green where not
void cat_selStatusStealthDetectDraw(Ship *ship, sdword lod)
{
    rectangle rect;
    sdword rad, x, index;
    color c;

    if ((ship->staticinfo->catTypeFlags & CAT_TYPEFLAG_Stealth) == 0 ||
        !allianceIsShipAlly(ship, universe.curPlayerPtr))
    {
        return;
    }
    cat_selStatusRectCompute(ship, lod, &rect);
    rad = (rect.y1 - rect.y0) / 2;
    x = rad + rect.x0;
    for (index = 0; index < universe.numPlayers; index++)
    {
        c = catSelDetectedColor;
        if (allianceArePlayersAllied(ship->playerowner, &universe.players[index]) ||
            !cat_shipDetectedByPlayer(ship, index))
        {
            c = catSelUndetectedColor;
        }
        primCircleSolid2(x, rad + 2 + rect.y1, rad, 20, c);
        x += rad * 2;
    }
}

// CAT: cat_selStatusBarsDraw 0x00507bc0: the rows of a ship's status bars (see the block comment above)
void cat_selStatusBarsDraw(Ship *ship, sdword lod)
{
    real32 factor;
    color c, dark;
    sdword row;
    rectangle rect;

    catHudCounts.statusBars++;                              // CAT: port diagnostic (the HUD self-test)
    cat_selHealthFactorGet(ship, &factor);
    cat_selHealthColorsGet(&factor, &c, &dark);
    cat_selStatusBarDraw(ship, lod, 0, &factor, &c, &dark);
    row = 1;
    if ((ship->shiptype == sACV || ship->shiptype == bACV) && cat_shipHasUpgrade(ship, CAT_UPGRADE_ACVEMP))
    {
        factor = cat_acvEMPChargeFraction(ship);
        cat_selStatusBarDraw(ship, lod, 1, &factor, &catSelEMPColor, &catSelEMPDarkColor);
        row = 2;
    }
    if (ship->staticinfo->maxresources != 0)
    {
        factor = (real32)ship->resources / (real32)ship->staticinfo->maxresources;
        cat_selStatusBarDraw(ship, lod, row, &factor, &selShipResourceColor, &selShipDarkResourceColor);
        row++;
    }
    if (cat_shipCanUseBeastWeapon(ship))
    {
        factor = cat_beastWeaponChargeFraction(ship);
        cat_selStatusBarDraw(ship, lod, row, &factor, &catSelBeastColor, &catSelBeastDarkColor);
        row++;
    }
    if (cat_dreadnoughtHasCannonUpgrade(ship))
    {
        factor = cat_dreadnoughtChargeFraction(ship);
        cat_selStatusBarDraw(ship, lod, row, &factor, &catSelDreadnoughtColor, &catSelDreadnoughtDarkColor);
        row++;
    }
    if (cat_mothershipHasBigGunModule(ship))
    {
        factor = cat_bigGunChargeFraction(ship);
        cat_selStatusBarDraw(ship, lod, row, &factor, &catSelBigGunColor, &catSelBigGunDarkColor);
        row++;
    }
    if (bitTest(ship->header.flags, SOF_CatCloakDevice))
    {
        factor = cat_cloakGetChargeFraction(ship);
        cat_selStatusBarDraw(ship, lod, row, &factor, &selShipResourceColor, &selShipDarkResourceColor);
        row++;
    }
    if (ship->shiptype == kGravWellGenerator || ship->shiptype == tGravWellGenerator)
    {
        real32 operationTime = ((GravWellGeneratorStatics *)ship->staticinfo->custstatinfo)->OperationTime;
        factor = (operationTime - ((GravWellGeneratorSpec *)ship->ShipSpecifics)->TimeOn) / operationTime;
        cat_selStatusBarDraw(ship, lod, row, &factor, &selShipResourceColor, &selShipDarkResourceColor);
        row++;
    }
    if (ship->shiptype == sSentinel || ship->shiptype == bSentinel)
    {
        factor = cat_sentinelFieldChargeFraction((SentinelSpec *)ship->ShipSpecifics);
        if (factor > 1.0f)
        {
            factor = 1.0f;
        }
        else if (factor < 0.0f)
        {
            factor = 0.0f;
        }
        cat_selStatusBarDraw(ship, lod, row, &factor, &catSelSentinelColor, &catSelSentinelDarkColor);
        row++;
    }
    if (ship->catUpgrading)
    {
        factor = (universe.totaltimeelapsed - ship->catUpgradeStart) / (ship->catUpgradeEnd - ship->catUpgradeStart);
        cat_selStatusBarDraw(ship, lod, row, &factor, &catSelUpgradingColor, &catSelUpgradingDarkColor);
        row++;
    }
    cat_selStatusUpgradePendingDraw(ship, lod);
    if (ship->shipOrDerelict.catDisabled && allianceIsShipAlly(ship, universe.curPlayerPtr))
    {                                                       // the disabled icon left of the bars, three bars high
        sdword size;
        cat_selStatusRectCompute(ship, lod, &rect);
        size = (rect.y1 - rect.y0) * 3;
        cat_overlayIconDraw(CAT_OI_X, rect.y0, rect.x0 - size - 1, size, size, 0xffffffff);
    }
    cat_selStatusStealthDetectDraw(ship, lod);
    cat_selStatusIconsDraw(ship, lod);
    cat_selStatusHotKeyDraw(ship, lod);
    cat_selStatusAmmoCountDraw(ship, lod, row);
}

// CAT: the tutorial pointer rectangles of selStatusDraw0-4 (the exe keeps HW1's, sized from the old bar geometry)
static void catSelStatusTutorialRects(Ship *ship, sdword lod, sdword grow, bool32 groupRect)
{
    rectangle rect;
    sdword x, halfWidth;
    real32 radius;

    cat_selStatusRectCompute(ship, lod, &rect);
    x = catSelScreenX(ship->impactable.collInfo.selCircleX);
    radius = (ship->impactable.collInfo.selCircleRadius > selMinSelectionRadius) ?
             ship->impactable.collInfo.selCircleRadius : selMinSelectionRadius;
    halfWidth = catSelRound(selSelectionWidthFactor0 * radius * (real32)MAIN_WindowWidth * 0.5f);
    cat_selStatusBarsDraw(ship, lod);
    if (tutorial && tutPointerShip == ship && tutPointerShipHealthRect)
    {
        tutPointerShipHealthRect->x0 = x - halfWidth - 2;
        tutPointerShipHealthRect->x1 = halfWidth + 2 + x;
        tutPointerShipHealthRect->y0 = rect.y0 - 2;
        tutPointerShipHealthRect->y1 = rect.y1 + 2;
    }
#define CAT_SEL_TUT_GROW()                                                                          \
    if (groupRect) { if (tutorial && tutPointerShip == ship && tutPointerShipGroupRect) tutPointerShipGroupRect->y1 += grow; } \
    else { if (tutorial && tutPointerShip == ship && tutPointerShipHealthRect) tutPointerShipHealthRect->y1 += grow; }
    if ((real32)ship->staticinfo->maxresources != 0.0f)
    {
        CAT_SEL_TUT_GROW();
    }
    if (ship->shiptype == kCloakGenerator || ship->shiptype == tCloakGenerator ||
        ship->shiptype == kGravWellGenerator || ship->shiptype == tGravWellGenerator)
    {
        CAT_SEL_TUT_GROW();
    }
    else if (bitTest(ship->shipOrDerelict.specialFlags, SPECIAL_CatSalvager))
    {
        (void)cat_salvageGetSalvagerInfo(ship);             // the exe still calls it and drops the result (HW1's
    }                                                       // salvage-capture bar is gone)
#undef CAT_SEL_TUT_GROW
}

// CAT: selStatusDraw0 0x00507f10 / selStatusDraw1 0x005080f0 / selStatusDraw2 0x005082c0: the bars at LOD 0-2
void selStatusDraw0(Ship *ship)
{
    catSelStatusTutorialRects(ship, 0, 5, FALSE);
}
void selStatusDraw1(Ship *ship)
{
    catSelStatusTutorialRects(ship, 1, 4, FALSE);
}
void selStatusDraw2(Ship *ship)
{
    catSelStatusTutorialRects(ship, 2, 3, TRUE);
}

// CAT: selStatusDraw3 0x00508480 / selStatusDraw4 0x00508590: the bars at LOD 3-4; the tutorial rectangle is a
//      4-pixel box around the ship's centre
static void catSelStatusDrawFar(Ship *ship, sdword lod)
{
    sdword x, y, halfWidth;
    real32 radius;

    x = catSelScreenX(ship->impactable.collInfo.selCircleX);
    y = catSelScreenY(ship->impactable.collInfo.selCircleY);
    radius = (ship->impactable.collInfo.selCircleRadius > selMinSelectionRadius) ?
             ship->impactable.collInfo.selCircleRadius : selMinSelectionRadius;
    halfWidth = catSelRound(selSelectionWidthFactor0 * radius * (real32)MAIN_WindowWidth * 0.5f);
    cat_selStatusBarsDraw(ship, lod);
    if (tutorial && tutPointerShip == ship && tutPointerShipHealthRect)
    {
        tutPointerShipHealthRect->x0 = x - halfWidth - 2;
        tutPointerShipHealthRect->x1 = x + 2 + halfWidth;
        tutPointerShipHealthRect->y0 = y - 2;
        tutPointerShipHealthRect->y1 = y + 2;
    }
}
void selStatusDraw3(Ship *ship)
{
    catSelStatusDrawFar(ship, 3);
}
void selStatusDraw4(Ship *ship)
{
    catSelStatusDrawFar(ship, 4);
}

void selStatusDrawNULL(Ship *ship)
{
    ;//!!! do nothing
}
typedef void (*seldrawfunction)(Ship *ship);
seldrawfunction selStatusDraw[SEL_NumberLOD] =
{
#if DEBUG_COLLISION_SPHERES
    selSelectionDraw5,
    selSelectionDraw5,
    selSelectionDraw5,
    selSelectionDraw5,
    selSelectionDraw5,
#else
    selStatusDraw0,
    selStatusDraw1,
    selStatusDraw2,
    selStatusDraw3,
    selStatusDraw4,
#endif
    selStatusDrawNULL,
};
/*-----------------------------------------------------------------------------
    Name        : selSelectedDraw/selSelectingDraw
    Description : Draw overlays for the list of selected/selecting ships
    Inputs      : void
    Outputs     : updates the selCentrePoint vector
    Return      : void
    Note        : rendering system must be in 2D primitive mode when this is
                    called.
----------------------------------------------------------------------------*/
void selSelectedDraw(void)
{
    sdword index;
    Ship *ship;
    fonthandle currentFont = fontCurrentGet();
    sdword currentLOD;
    CameraStackEntry *entry;

    // CAT: selSelectedDraw 0x005086a0 draws also with no selection (the focus ship's health bar below)
    rndTextureEnable(FALSE);
    rndLightingEnable(FALSE);

    for (index = 0; index < selSelected.numShips; index++)
    {
        ship = selSelected.ShipPtr[index];
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (univSpaceObjInRenderList((SpaceObj *)ship))
        {
            // CAT: not the ship the camera pilots (cat_ccIsPilotingThisShip 0x0042c310)
            if (ship->impactable.collInfo.selCircleRadius > 0.0f && !cat_ccIsPilotingThisShip(ship))
            {
                currentLOD = ship->obj.currentLOD;
                dbgAssertOrIgnore(currentLOD < SEL_NumberLOD);
                // CAT: any mothership-class ship draws one LOD finer (selSelectedDraw 0x005086a0 tests shipclass 0);
                //      HW1 tested its mothership type
                if ((ship->staticinfo->shipclass == CLASS_Mothership) && (currentLOD > 0))
                {
                    currentLOD--;
                }
                glShadeModel(GL_FLAT);
                selStatusDraw[currentLOD](ship);
#if SEL_DRAW_BOXES
#if RND_VISUALIZATION
                if (RENDER_BOXES)
                {
                    selDrawBoxes((SpaceObjRotImpTarg *)ship);
                }
#endif
#endif
            }
        }
    }
    // CAT: with advanced sensors (TECH_ADVANCED_SENSORS, 0x1c), the health bar of another player's ship the camera
    //      focuses alone, outlined in red, while the overlay's alternate print options are off and no NIS is fully
    //      scissored (selSelectedDraw 0x005086a0)
    entry = universe.mainCameraCommand.currentCameraStack;
    if (!cat_moAltPrintOptionsActive() && entry != NULL && entry->focus.numShips == 1)
    {
        ship = entry->focus.ShipPtr[0];
        if (ship->header.objtype == OBJ_ShipType && ship->playerowner != universe.curPlayerPtr &&
            cat_rmTechSetHas((sdword *)&universe.curPlayerPtr->researchinfo, TECH_ADVANCED_SENSORS) &&
            !nisFullyScissored && !bitTest(ship->header.catFlags18, CAT_SHIPFLAG_KasDerelict) &&
            !bitTest(ship->header.flags, SOF_Dead) && !cat_shipIsUntargetable(ship))
        {
            real32 factor;
            color c, dark;
            rectangle rect, outline;

            currentLOD = ship->obj.currentLOD;
            cat_selHealthFactorGet(ship, &factor);
            if (factor > selShipHealthGreenFactor)
            {
                c = selShipHealthGreen;
                dark = selShipHealthDarkGreen;
            }
            else if (factor > selShipHealthYellowFactor)
            {
                c = selShipHealthYellow;
                dark = selShipHealthDarkYellow;
            }
            else
            {
                c = selShipHealthRed;
                dark = selShipHealthDarkRed;
            }
            cat_selStatusBarDraw(ship, currentLOD, 0, &factor, &c, &dark);
            cat_selStatusRectCompute(ship, currentLOD, &rect);
            outline.x0 = rect.x0 - 1;
            outline.x1 = rect.x1 + 1;
            outline.y0 = rect.y0 - 1;
            outline.y1 = rect.y1 + 1;
            primRectOutline2(&outline, 1, 0xff0000ff);
        }
    }
    fontMakeCurrent(currentFont);
}

void selSelectingDraw(void)
{
    sdword index, j;
    SpaceObjRotImpTarg *target;
    rectangle rect;
    real32 radius;
    real32 x0, y0, x1, y1;
    vector *point;

    for (index = 0; index < selSelecting.numTargets; index++)
    {
        target = selSelecting.TargetPtr[index];

        if (univSpaceObjInRenderList(&target->obj))
        {
            if (target->impactable.collInfo.selCircleRadius > 0.0f)
            {
                if (target->impactable.collInfo.precise != NULL &&
                    target->obj.currentLOD <= target->staticinfo->common.staticCollInfo.preciseSelection)
                {                                           //if we should perform precise collision checking
                    point = target->impactable.collInfo.precise->worldRectPos;//list of points
                    x0 = y1 = REALlyBig;
                    x1 = y0 = REALlyNegative;
                    for (j = 0; j < 8; j++)
                    {                                       //find bounds of selection
                        x0 = min(x0, point[j].x);
                        y0 = max(y0, point[j].y);
                        x1 = max(x1, point[j].x);
                        y1 = min(y1, point[j].y);
                    }
                    // CAT: rounded to the nearest pixel (selSelectingDraw 0x00508900)
                    rect.x0 = catSelScreenX(x0);
                    rect.y0 = catSelScreenY(y0);
                    rect.x1 = catSelScreenX(x1);
                    rect.y1 = catSelScreenY(y1);
                }
                else
                {
                    if (target->header.objtype == OBJ_ShipType)
                        radius = max(target->impactable.collInfo.selCircleRadius * ((Ship *)target)->staticinfo->tacticalSelectionScale, selMinSelectionRadius);
                    else
                        radius = max(target->impactable.collInfo.selCircleRadius, selMinSelectionRadius);
                    rect.x0 = catSelScreenX(target->impactable.collInfo.selCircleX - radius);
                    rect.x1 = catSelScreenX(target->impactable.collInfo.selCircleX + radius);
                    rect.y0 = catSelScreenY(target->impactable.collInfo.selCircleY + radius);
                    rect.y1 = catSelScreenY(target->impactable.collInfo.selCircleY - radius);
                }
                primRectOutline2(&rect, 1, selSelectingColor);//!!! note the hard-coded numbers
                // CAT: no "Game_SelectingRect" tutorial message (selSelectingDraw 0x00508900 sends none)
            }
        }
    }
}


/*-----------------------------------------------------------------------------
    Name        : selCentrePointComputeGeneral
    Description : Compute the centre point of any selection
    Inputs      : selection - the selection to compute the center of
                  average_size - optional return variable - average size of the selection
    Outputs     :
    Return      : vector
----------------------------------------------------------------------------*/
vector selCentrePointComputeGeneral(MaxSelection *selection, real32 *average_size)
{
    sdword index;
    real32 realIndex;
    Ship *ship;
    vector minVector = {REALlyBig, REALlyBig, REALlyBig};
    vector maxVector = {REALlyNegative, REALlyNegative, REALlyNegative};
    vector returnVector;

    real32 radius, scale;

    returnVector.x = returnVector.y = returnVector.z = *average_size = 0.0f;
    for (index = 0; index < selection->numShips; index++)
    {
        ship = selection->ShipPtr[index];

        // CAT: selCentrePointComputeGeneral 0x00508be0 takes ships and missiles (other objects count in the average
        //      but add nothing), a mimic's select radius, and a scale of 1 where magnitudeSquared is 0
        if (ship->header.objtype == OBJ_ShipType)
        {
            radius = bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic) ? cat_mimicGetSelectRadius(ship)
                                                                    : ship->staticinfo->common.staticCollInfo.collspheresize;
            scale = (ship->magnitudeSquared != 0.0f) ? ship->magnitudeSquared : 1.0f;
        }
        else if (ship->header.objtype == OBJ_MissileType)
        {
            radius = ((Missile *)ship)->staticinfo->common.staticCollInfo.collspheresize;
            // CAT-PROVISIONAL: the exe scales by a Cataclysm Missile float at +0x254 when it is nonzero (not
            //      identified; the Missile layout comes with the missile code, M2-R4 #3); 1 until then
            scale = 1.0f;
        }
        else
        {
            continue;
        }

        minVector.x = min(minVector.x, ship->obj.posinfo.position.x);
        minVector.y = min(minVector.y, ship->obj.posinfo.position.y);
        minVector.z = min(minVector.z, ship->obj.posinfo.position.z);
        maxVector.x = max(maxVector.x, ship->obj.posinfo.position.x);
        maxVector.y = max(maxVector.y, ship->obj.posinfo.position.y);
        maxVector.z = max(maxVector.z, ship->obj.posinfo.position.z);
        *average_size += scale * radius;
//        returnVector.x += ship->obj.posinfo.position.x;
//        returnVector.y += ship->obj.posinfo.position.y;
//        returnVector.z += ship->obj.posinfo.position.z;
    }
    realIndex = (real32)index;
    *average_size /= realIndex;
//    returnVector.x /= realIndex;
//    returnVector.y /= realIndex;
//    returnVector.z /= realIndex;
    returnVector.x = (minVector.x + maxVector.x) / 2.0f;
    returnVector.y = (minVector.y + maxVector.y) / 2.0f;
    returnVector.z = (minVector.z + maxVector.z) / 2.0f;

    return returnVector;
}



/*-----------------------------------------------------------------------------
    Name        : selCentrePointCompute
    Description : Compute the centre point of current selection
    Inputs      : void
    Outputs     : updates the centre point of the current selection.
    Return      : void
----------------------------------------------------------------------------*/
void selCentrePointCompute(void)
{
    selCentrePoint = selCentrePointComputeGeneral(&selSelected, &selAverageSize);
}


/*-----------------------------------------------------------------------------
    Name        : selSelectionCopy
    Description : Copy a MaxSelection structure from one place to another.
    Inputs      : source - source MaxSelection structure
                  dest - dest MaxSelection strucure
    Outputs     : guess
    Return      : number of entries copied
----------------------------------------------------------------------------*/
sdword selSelectionCopy(MaxAnySelection *dest, MaxAnySelection *source)
{
    sdword index;
    TargetPtr *sourceP, *destP;

    sourceP = source->TargetPtr;                            //pointers to the base of lists
    destP =   dest->TargetPtr;

    dbgAssertOrIgnore(source->numTargets < COMMAND_MAX_SHIPS);

    for (index = source->numTargets; index > 0; index--)
    {                                                       //for each ship in source
        *destP = *sourceP;                                  //copy a single ShipPtr
        sourceP++;                                          //inc source/dest pointers
        destP++;
    }
    dest->numTargets = source->numTargets;                  //copy number of ships

    return(source->numTargets);                             //return number of ships copied
}

/*-----------------------------------------------------------------------------
    Name        : selSelectionIsReinforced
    Description : See if a group will be is being reinforced by a new selection.
    Inputs      : dest - selection to be reinforced
                  source - selection that will replace this new selection
    Outputs     :
    Return      : TRUE if it's a reinforcement.
----------------------------------------------------------------------------*/
bool32 selSelectionIsReinforced(MaxAnySelection *dest, MaxAnySelection *source)
{
    sdword index;
    sdword nAlreadyInSelection = 0;
    sdword nNeededAlready;

    if (source->numTargets <= dest->numTargets)
    {                                                       //if we're replacing no more ships
        return(FALSE);                                      //it's not reinforcing at all!
    }
    else if (dest->numTargets == 0)
    {                                                       //if starting selection is empty
        return(FALSE);                                      //that's not a reinforcement
    }
    else if (dest->numTargets == 1)
    {                                                       //only reinforcing 1 guy
        nNeededAlready = 1;
    }
    else
    {
        nNeededAlready = dest->numTargets * SEL_ReinforceFactor;
    }

    for (index = 0; index < dest->numTargets; index++)
    {
        if (selShipInSelection((ShipPtr *)source->TargetPtr, source->numTargets, (ShipPtr)dest->TargetPtr[index]))
        {                                                   //if this ship is in the replacement selection
            nAlreadyInSelection++;
        }
    }
    if (nAlreadyInSelection >= nNeededAlready)
    {                                                       //if there is enough reinforcment here to count
        return(TRUE);
    }
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : selSelectionCopyByClass
    Description : Copy a MaxSelection structure from one place to another, copying
                    only the ships which match a specified class.
    Inputs      : source - source MaxSelection structure
                  dest - dest MaxSelection strucure
                  classMask - class to match
    Outputs     : guess
    Return      : number of entries copied
----------------------------------------------------------------------------*/
sdword selSelectionCopyByClass(MaxSelection *dest, MaxSelection *source, ShipClass classMask)
{
    sdword index;
    ShipPtr *sourceP, *destP;

    sourceP = source->ShipPtr;                              //pointers to the base of lists
    destP =   dest->ShipPtr;

    dbgAssertOrIgnore(source->numShips < COMMAND_MAX_SHIPS);

    dest->numShips = 0;

    for (index = source->numShips; index > 0; index--)
    {                                                       //for each ship in source
        dbgAssertOrIgnore((*sourceP)->header.objtype == OBJ_ShipType);
        if ((*sourceP)->staticinfo->shipclass == classMask)
        {                                                   //if classes match
            *destP = *sourceP;                              //copy a single ShipPtr
            destP++;
            dest->numShips++;
        }
        sourceP++;                                          //inc source/dest pointers
    }
    return(dest->numShips);                                 //return number of ships copied
}

/*-----------------------------------------------------------------------------
    Name        : selSelectionCopyByType
    Description : Copy a MaxSelection structure from one place to another, copying
                    only the ships which match a specified class.
    Inputs      : source - source MaxSelection structure
                  dest - dest MaxSelection strucure
                  classMask - class to match
    Outputs     : guess
    Return      : number of entries copied
----------------------------------------------------------------------------*/
sdword selSelectionCopyByType(MaxSelection *dest, MaxSelection *source, ShipType typeMask)
{
    sdword index;
    ShipPtr *sourceP, *destP;

    sourceP = source->ShipPtr;                              //pointers to the base of lists
    destP =   dest->ShipPtr;

    dbgAssertOrIgnore(source->numShips < COMMAND_MAX_SHIPS);

    dest->numShips = 0;

    for (index = source->numShips; index > 0; index--)
    {                                                       //for each ship in source
        dbgAssertOrIgnore((*sourceP)->header.objtype == OBJ_ShipType);
        if ((*sourceP)->shiptype == typeMask)
        {                                                   //if classes match
            *destP = *sourceP;                              //copy a single ShipPtr
            destP++;
            dest->numShips++;
        }
        sourceP++;                                          //inc source/dest pointers
    }
    return(dest->numShips);                                 //return number of ships copied
}

/*-----------------------------------------------------------------------------
    Name        : selSortCompare
    Description : Sort a selection by ship pointer, as a callback from qsort.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
int selSortCompare(void const* p0, void const* p1) {
    if (*((TargetPtr *)p0) > *((TargetPtr *)p1))
    {
        return(1);
    }
    else if (*((TargetPtr *)p0) < *((TargetPtr *)p1))
    {
        return(-1);
    }
    return(0);
}

/*-----------------------------------------------------------------------------
    Name        : selSelectionCompare
    Description : Compare 2 selections
    Inputs      : s0, s1 - pointers to selections to compare
    Outputs     : sorts both selections before comparing them
    Return      : 0 if identical, nonzero otherwise
----------------------------------------------------------------------------*/
sdword selSelectionCompare(MaxAnySelection *s0, MaxAnySelection *s1)
{
    sdword index;
    TargetPtr *sourceP, *destP;

    sourceP = s0->TargetPtr;                               //pointers to the base of lists
    destP =   s1->TargetPtr;

    dbgAssertOrIgnore(s0->numTargets < COMMAND_MAX_SHIPS);
    dbgAssertOrIgnore(s1->numTargets < COMMAND_MAX_SHIPS);

    qsort(s0->TargetPtr, s0->numTargets, sizeof(TargetPtr), selSortCompare);
    qsort(s1->TargetPtr, s1->numTargets, sizeof(TargetPtr), selSortCompare);
    if (s0->numTargets != s1->numTargets)
    {
        return(1);
    }
    for (index = s0->numTargets; index > 0; index--)
    {                                                       //for each ship in both lists
        if ((sdword)(*destP - *sourceP) != 0)
        {
            return(1);
        }
        sourceP++;                                          //inc source/dest pointers
        destP++;
    }
    return(0);                                              //return identical
}

/*-----------------------------------------------------------------------------
    Name        : selShipsInSelection
    Description : Search for a set of ships in a particular selection
    Inputs      : dest - where to search for matching ships
                  list - what ships to match
    Outputs     : sorts both lists for better searching
    Return      : TRUE of ship found, false otherwise
----------------------------------------------------------------------------*/
bool32 selShipsInSelection(MaxSelection *dest, MaxSelection *list)
{
    sdword nMatched, index, lIndex;

    if (list->numShips > dest->numShips)
    {                                                       //if cannot be matched
        return(FALSE);
    }

    qsort(dest->ShipPtr, dest->numShips, sizeof(ShipPtr), selSortCompare);
    qsort(list->ShipPtr, list->numShips, sizeof(ShipPtr), selSortCompare);

    for (nMatched = index = lIndex = 0; index < dest->numShips; index++)
    {
        if (list->ShipPtr[lIndex] == dest->ShipPtr[index])
        {
            nMatched++;
            if (nMatched == list->numShips)
            {                                               //if found all the ships we need
                return(TRUE);                               //return success
            }
            lIndex++;
            if (lIndex >= list->numShips)
            {                                               //if scanned through all ships
                return(FALSE);                              //return failure
            }
        }
    }
    return(FALSE);                                          //return failure
}

/*-----------------------------------------------------------------------------
    Name        : selSelectHotKeyGroup
    Description : selects a given hotkeygroup
    Inputs      : hotkeygroup to select
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void selSelectHotKeyGroup(MaxSelection *hotkeygroup)
{
    sdword i;
    Ship *ship;

    selSelected.numShips = 0;
    for (i=0;i<hotkeygroup->numShips;i++)
    {
        ship = hotkeygroup->ShipPtr[i];
        if ((ship->header.flags & (SOF_Hide|SOF_Disabled|SOF_Hyperspace|SOF_Crazy)) == 0 && bitTest(ship->header.flags, SOF_Selectable))
        {
            selSelected.ShipPtr[selSelected.numShips++] = ship;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : selHotKeyNumbersSet
    Description : Set the hot key group number for all ships in a specified
                    selection.
    Inputs      : group - selection to set numbers for
    Outputs     : Sets the hotKeyGroup of all ships in selection to number.
                    Also removes them from other hot-key groups they may be in.
    Return      : OKAY
----------------------------------------------------------------------------*/
sdword selHotKeyNumbersSet(sdword group)
{
    sdword index;
    ShipPtr *destP;

    destP = selHotKeyGroup[group].ShipPtr;

    for (index = selHotKeyGroup[group].numShips; index > 0; index--)
    {                                                       //for each ship in source
        selHotKeySet(*destP, group);
//        if (selHotKeyGroupNumberTest(*destP) == SEL_InvalidHotKey)
//        {                                                   //if this one belongs to no hot key group
            selHotKeyGroupNumberSet(*destP, group);         //set it to this group
//        }
        destP++;
    }
    return(OKAY);
}

/*-----------------------------------------------------------------------------
    Name        : selHotKeyGroupRemoveReferences
    Description : Remove references to this hot-key group from the ships in this group.
    Inputs      : group - group to remove references from
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void selHotKeyGroupRemoveReferences(sdword group)
{
    sdword index, j;
    ShipPtr *sourceP;
    MaxSelection *destSelection, *thisSelection;

    thisSelection = &selHotKeyGroup[group];
    sourceP = thisSelection->ShipPtr;

    for (index = thisSelection->numShips; index > 0; index--)
    {                                                       //for each ship in source
        if (selHotKeyGroupNumberTest(*sourceP) == group)
        {                                                   //if ship in this hot-key group
            destSelection = &selHotKeyGroup[group];
            if (destSelection != thisSelection)
            {                                               //don't remove it from this selection because this selection is about to be overwritten anyhow
                clRemoveShipFromSelection((SelectAnyCommand *)destSelection, *sourceP);
            }
            selHotKeyClear(*sourceP, group);
            selHotKeyGroupNumberClear(*sourceP);
            for (j = 0; j < SEL_NumberHotKeyGroups; j++)
            {                                               //search for another hotkey group this guy belongs to
                if (selHotKeyTest(*sourceP, j))
                {
//                    selHotKeySet(*sourceP, j);
                    selHotKeyGroupNumberSet(*sourceP, j);
                    goto foundOne;
                }
            }
            //no other hot-key group found, set hot key group number
            selHotKeyGroupNumberClear(*sourceP);
foundOne:;
        }

        sourceP++;
    }
}

/*-----------------------------------------------------------------------------
    Name        : selHotKeyGroupRemoveReferencesFromAllGroups
    Description : Remove references to all hot-key groups from the ships in selSelected.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void selHotKeyGroupRemoveReferencesFromAllGroups(void)
{
    sdword index, j;
    ShipPtr *sourceP;
    MaxSelection *destSelection;

    sourceP = selSelected.ShipPtr;

    for (index = selSelected.numShips; index > 0; index--)
    {                                                       //for each ship in source
        selHotKeyGroupNumberClear(*sourceP);
        for (j = 0; j < SEL_NumberHotKeyGroups; j++)
        {                                                   //see what groups this ship is in
            if (selHotKeyTest(*sourceP, j))
            {                                               //if it's in this group
                destSelection = &selHotKeyGroup[j];         //remove it from this group
                clRemoveShipFromSelection((SelectAnyCommand *)destSelection, *sourceP);
            }
        }
        selHotKeyClearAll(*sourceP);                        //not in any group
//        (*sourceP)->hotKeyGroup = SEL_InvalidHotKey;

        sourceP++;
    }
}

/*-----------------------------------------------------------------------------
    Name        : selHotKeyGroupsVerify
    Description : Verify integrity of hot-key groups
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
#if SEL_ERROR_CHECKING
void selHotKeyGroupsVerify(void)
{
    sdword index, j;

    for (index = 0; index < SEL_NumberHotKeyGroups; index++)
    {                                                       //for all hot-key groups
        for (j = 0; j < selHotKeyGroup[index].numShips; j++)
        {
            dbgAssertOrIgnore(selHotKeyGroup[index].ShipPtr[j]->header.objtype == OBJ_ShipType);
            if (selAnyHotKeyTest(selHotKeyGroup[index].ShipPtr[j]))
            {
                dbgAssertOrIgnore(selHotKeyGroupNumberTest(selHotKeyGroup[index].ShipPtr[j]) != SEL_InvalidHotKey);
                dbgAssertOrIgnore(selHotKeyTest(selHotKeyGroup[index].ShipPtr[j], index));
                dbgAssertOrIgnore(selHotKeyTest(selHotKeyGroup[index].ShipPtr[j], selHotKeyGroupNumberTest(selHotKeyGroup[index].ShipPtr[j])));
            }
            else
            {
                dbgAssertOrIgnore(selHotKeyGroupNumberTest(selHotKeyGroup[index].ShipPtr[j]) == SEL_InvalidHotKey);
            }
        }
    }
}
#endif //SEL_ERROR_CHECKING

/*-----------------------------------------------------------------------------
    Name        : selSelectionDimensions
    Description : Compute the on-screen selection circle for a group of ships
    Inputs      : selection - selection to compute on-screen coordinates of
                  modelView, projection - matrices for camera and projection
    Outputs     : destX, destY, destRad - on-screen positon of the selection of ships
    Return      :
----------------------------------------------------------------------------*/
void selSelectionDimensions(hmatrix *modelView, hmatrix *projection, SelectCommand *selection, real32 *destX, real32 *destY, real32 *destRad)
{
    sdword index;
    real32 averageSize;
    Ship *ship;
    real32 radius, maxRadius = 0;
    vector centre = {0, 0, 0}, distance;

    /*
    if (smSensorsActive)
    {
    }
    else
    */
    {                                                       //not in the SM: plain-on-screen position of ships
        //run through all ships to compute median position.
        centre = selCentrePointComputeGeneral((MaxSelection *)selection, &averageSize);
        /*
        for (index = 0; index < selection->numShips; index++)
        {
            ship = selection->ShipPtr[index];
            vecAddTo(centre, ship->obj.posinfo.position);
        }
        radius = 1.0f / (real32)index;
        vecMultiplyByScalar(centre, radius);                //compute mean position
        */
        //run through again to find max radius
        for (index = 0; index < selection->numShips; index++)
        {
            ship = selection->ShipPtr[index];
            vecSub(distance, centre, ship->obj.posinfo.position);//distance to mean
            // CAT: a mimic's select radius (selSelectionDimensions 0x00509250)
            radius = bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic) ? cat_mimicGetSelectRadius(ship)
                                                                    : ship->staticinfo->common.staticCollInfo.collspheresize;
            radius = vecMagnitudeSquared(distance) + radius * radius;//square of distance
            if (radius > maxRadius)
            {
                maxRadius = radius;
            }
        }
        maxRadius = fsqrt(maxRadius);
        selCircleComputeGeneral(modelView, projection, &centre, maxRadius, destX, destY, destRad);
    }
}

