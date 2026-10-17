/*=============================================================================
    Name    : Tactical.c
    Purpose : Logic for the tactical overlay

    Created 8/8/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "Tactical.h"

#include <stdio.h>

#include "Alliance.h"
#include "ProximitySensor.h"
#include "GravWellGenerator.h"
#include "DFGFrigate.h"
#include "Mimic.h"
#include "ShipSelect.h"
#include "mainrgn.h"
#include "Blobs.h"
#include "Debug.h"
#include "font.h"
#include "main.h"
#include "Memory.h"
#include "render.h"
#include "Sensors.h"
#include "Ships.h"
#include "SpaceObj.h"
#include "StatScript.h"
#include "Teams.h"
#include "Tweak.h"
#include "Universe.h"
#include "Vector.h"

#ifdef _MSC_VER
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif



extern fonthandle selGroupFont2;
extern bool32 mrDrawTactical;                      // mainrgn.c (CAT: toLegendDraw tests it, M2-R5 hud)

//located in mainrgn.c
void toMoveLineDraw(ShipPtr ship, real32 scale);
void toFieldSphereDraw(ShipPtr ship,real32 radius, real32 scale,color passedColour);
void toDrawRadialIndicator1(ShipPtr ship, real32 radius, real32 scale,color passedColour);
void toDrawRadialIndicator2(ShipPtr ship, real32 radius, real32 scale,color passedColour);
void toDrawRadialIndicator3(ShipPtr ship, real32 radius, real32 scale,color passedColour);
void toDrawRadialIndicator4(ShipPtr ship, real32 radius, real32 scale,color passedColour);

real32 mineMineTOSize;
real32 toMinimumSize;

/*=============================================================================
    Data:
=============================================================================*/
//basic, default TO icon
toicon toDefaultIcon =
{
    0, {{0.0f, 0.0f}}
};

//to icons for all classes of ships + 1 for mines
toicon *toClassIcon[NUM_CLASSES+1];

//colors for tactical overlay of various players
color toPlayerColor[TO_NumPlayers];

//for legend: track if a particular class is currently displayed (for each player)
bool8 toClassUsed[NUM_CLASSES+1][TO_NumPlayers];

void toNewClassSet(char *directory,char *field,void *dataToFillIn);
void toNumberPointsSet(char *directory,char *field,void *dataToFillIn);
void toVertexAdd(char *directory,char *field,void *dataToFillIn);
scriptEntry toIconTweaks[] =
{
    { "IconClass",      toNewClassSet,  NULL},
    { "nPoints",        toNumberPointsSet, NULL},
    { "vertex",         toVertexAdd,    NULL},
    { "playerColor0",   scriptSetRGBCB, &toPlayerColor[0]},
    { "playerColor1",   scriptSetRGBCB, &toPlayerColor[1]},
    { "playerColor2",   scriptSetRGBCB, &toPlayerColor[2]},
    { "playerColor3",   scriptSetRGBCB, &toPlayerColor[3]},
    { "playerColor4",   scriptSetRGBCB, &toPlayerColor[4]},
    { "playerColor5",   scriptSetRGBCB, &toPlayerColor[5]},
    { "playerColor6",   scriptSetRGBCB, &toPlayerColor[6]},
    { "playerColor7",   scriptSetRGBCB, &toPlayerColor[7]},
    { NULL, NULL, 0 }
};

//retained integrals used during loading
ShipClass toCurrentClass = CLASS_Mothership;
sdword toCurrentPoint = 0;

// x,y location for the player list in the main tactical overlay
sdword TO_PLAYERLIST_Y = 480;
sdword TO_PLAYERLIST_X = 10;

/*=============================================================================
    Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : toStartup
    Description : Startup TO module.
    Inputs      : void
    Outputs     : Initializes the toClassIcon array.
    Return      : void
----------------------------------------------------------------------------*/
void toStartup(void)
{
    sdword index;

    mineMineTOSize = primScreenToGLScaleX(TO_MineMinimumScreenSize);
    toMinimumSize  = primScreenToGLScaleX(TO_MinimumScreenSize);

    for (index = 0; index < (NUM_CLASSES+1); index++)
    {                                                       //set all icons to default
        toClassIcon[index] = &toDefaultIcon;
    }
    scriptSet(NULL, "Tactical.script", toIconTweaks);
}

/*-----------------------------------------------------------------------------
    Name        : toShutdown
    Description : Shut down the TO module.
    Inputs      : void
    Outputs     : Frees the toIconTweaks array.
    Return      : void
----------------------------------------------------------------------------*/
void toShutdown(void)
{
    sdword index;

    for (index = 0; index < (NUM_CLASSES+1); index++)
    {                                                       //set all icons to default
        if (toClassIcon[index] != &toDefaultIcon && toClassIcon[index] != NULL)
        {
            memFree(toClassIcon[index]);
        }
    }
}

/*-----------------------------------------------------------------------------
    Script-read functions to load in the TO icons
-----------------------------------------------------------------------------*/
void toNewClassSet(char *directory,char *field,void *dataToFillIn)
{                                                           //set new ship class
    if (strcasecmp(field,"CLASS_Mine") == 0)
    {
        toCurrentClass = NUM_CLASSES;   // use NUM_CLASSES to indicate mine
    }
    else
    {
        toCurrentClass = StrToShipClass(field);
    }
    dbgAssertOrIgnore(toCurrentClass < (NUM_CLASSES+1));
}
void toNumberPointsSet(char *directory,char *field,void *dataToFillIn)
{                                                           //set number of points and allocate structure
    sdword nPoints, nScanned;

    nScanned = sscanf(field, "%d", &nPoints);               //parse number of points
    dbgAssertOrIgnore(nScanned == 1);
    dbgAssertOrIgnore(nPoints > 0 && nPoints < 32);                 //!!! 32 is some arbitrary number
    toClassIcon[toCurrentClass] = memAlloc(toIconSize(nPoints),//allocate the icon structure
                                           "Tactical Overlay Icon", NonVolatile);
    toClassIcon[toCurrentClass]->nPoints = nPoints;
    toCurrentPoint = 0;                                     //no points loaded yet
}
void toVertexAdd(char *directory,char *field,void *dataToFillIn)
{
    real32 x, y;
    sdword nScanned;

    dbgAssertOrIgnore(toCurrentPoint < toClassIcon[toCurrentClass]->nPoints);//make sure not too many points
    nScanned = sscanf(field, "%f,%f", &x, &y);              //scan in the x/y
    dbgAssertOrIgnore(nScanned == 2);                               //make sure we got 2 numbers
    toClassIcon[toCurrentClass]->loc[toCurrentPoint].x =    //copy the points over
        x * TO_VertexScanFactorX;                           //multiplied by a scaling factor
    toClassIcon[toCurrentClass]->loc[toCurrentPoint].y =
        y * TO_VertexScanFactorY;
    toCurrentPoint++;                                       //next point
}

void toFieldSphereDrawGeneral(vector position, real32 radius,color passedColour);

/*-----------------------------------------------------------------------------
    Name        : toAllShipsDraw                                    0x00528aa0
    Description : Draw tactical overlays for all ships.
                  CAT (M2-R5 hud): Cataclysm's: the sensors manager walks the ship list (the main view the render
                  list); mines and ships are drawn only in the main view; no slipgate (type bit 0x4000), crystal or
                  race-6 ship (but dAcolyte); an enemy ship only when detected (sub_53F9F0), an enemy mimic posing as a
                  resource or derelict not at all, one posing as allied with the allied colour and its disguise's
                  class icon; an own SPECIAL_2_CatExempt20 ship in the allied colour; no mothership-class icon; the
                  DFG, gravwell and proximity-sensor spheres for the exe's types (no cloak generator sphere).
    Inputs      :
    Outputs     : Draw a tactical display icon overlay for all on-screen ships
    Return      : void
----------------------------------------------------------------------------*/
// CAT: the slipgates' bit of ShipStaticInfo.catTypeFlags (cat_univInitShipTypeFlags 0x0053b8c0 sets 0xc000 for them)
#define CAT_TYPEFLAG_Slipgate   0x00004000

void toAllShipsDraw(void)
{
    Node *objnode = smSensorsActive ? universe.ShipList.head : universe.RenderList.head;
    real32 radius = 0.0f, scale = 0.0f;
    sdword index = 0, player = 0;
    toicon *icon = NULL;
    color c = colBlack;
    udword intScale = 0;
    ShipClass shipClass;

     // reset usage of classes (for legend)
    for (index = 0; index < (NUM_CLASSES+1); index++)
    {
          for (player = 0; player < TO_NumPlayers; ++player)
          {
               toClassUsed[index][player] = 0;
          }
    }

    rndTextureEnable(FALSE);
    rndLightingEnable(FALSE);

    for (; objnode != NULL; objnode = objnode->next)
    {
        SpaceObjRotImp* impactable = (SpaceObjRotImp *) listGetStructOfNode(objnode);
        CollInfo* collInfo = &impactable->collInfo;

        if ((impactable->header.flags & SOF_Dead) || collInfo->selCircleRadius <= 0.0f)
        {
            continue;
        }

        if (impactable->header.objtype == OBJ_MissileType)
        {
            Missile* mine = (Missile*)impactable;
            radius = collInfo->selCircleRadius;
            if (mine->missileType == MISSILE_Mine && mine->playerowner == universe.curPlayerPtr &&
                radius < TO_MaxFadeSize && !smSensorsActive)
            {
                c = teFriendlyColor;
                if (radius > TO_FadeStart)
                {
                    // scale the color - start fading out the overlay
                    scale = (radius - TO_MinFadeSize) * TO_FadeRate;
                    intScale = (udword)scale;
                    c = colRGB(colRed(c) / intScale, colGreen(c) / intScale, colBlue(c) / intScale);
                }
                icon = toClassIcon[NUM_CLASSES];    // NUM_CLASSES indicates Mine
                radius = max(mineMineTOSize, radius);   //ensure radius not too small
                primLineLoopStart2(1, c);
                for (index = icon->nPoints - 1; index >= 0; index--)
                {
                    primLineLoopPoint3F(radius * icon->loc[index].x + collInfo->selCircleX,
                                        radius * icon->loc[index].y + collInfo->selCircleY);
                }
                primLineLoopEnd2();
            }
        }
        else if (impactable->header.objtype == OBJ_ShipType)
        {
            Ship* ship = (Ship*)impactable;
            bool32 own = (ship->playerowner == universe.curPlayerPtr);
            bool32 mimic = bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic);

            if ((ship->staticinfo->catTypeFlags & CAT_TYPEFLAG_Slipgate) || cat_shipIsCrystal(ship) ||
                (ship->shiprace == 6 && ship->shiptype != dAcolyte))
            {
                continue;
            }
            if (!own && !cat_shipDetectedByPlayer(ship, universe.curPlayerPtr->playerIndex))
            {
                continue;
            }
            if (!own && mimic && (cat_mimicAppearsAsResource(ship, universe.curPlayerPtr) ||
                                  cat_mimicAppearsAsDerelict(ship, universe.curPlayerPtr)))
            {
                continue;
            }

            // tactical overlay (fades out when the ship gets large enough)
            radius = ship->staticinfo->tacticalSelectionScale * collInfo->selCircleRadius;

            if (radius < TO_MaxFadeSize)
            {
                if (own)
                {
                    c = bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20) ? teAlliedColor : teFriendlyColor;
                }
                else if (allianceIsShipAlly(ship, universe.curPlayerPtr) ||
                         (mimic && cat_mimicAppearsAlliedTo(ship, universe.curPlayerPtr)))
                {
                    c = teAlliedColor;
                }
                else
                {
                    c = teHostileColor;
                }

                if (radius > TO_FadeStart)
                {
                    // scale the color - start fading out the overlay
                    scale = 255.0f - ((radius - TO_MinFadeSize) * TO_FadeRate);
                    intScale = (udword)scale;
                    c = colRGB((colRed(c) * intScale) >> 8, (colGreen(c) * intScale) >> 8, (colBlue(c) * intScale) >> 8);
                }

                shipClass = ship->staticinfo->shipclass;
                if (!own && mimic && cat_mimicAppearsAlliedTo(ship, universe.curPlayerPtr))
                {
                    icon = toClassIcon[cat_mimicGetApparentShipClass(ship)];
                }
                else
                {
                    icon = toClassIcon[shipClass];
                }
                if (!smSensorsActive && shipClass != CLASS_Mothership)
                {
                    radius = max(toMinimumSize, radius);   //ensure radius not too small
                    primLineLoopStart2(1, c);
                    for (index = icon->nPoints - 1; index >= 0; index--)
                    {
                        primLineLoopPoint3F(radius * icon->loc[index].x + collInfo->selCircleX,
                                            radius * icon->loc[index].y + collInfo->selCircleY);
                    }
                    primLineLoopEnd2();
                    // mark class as used for this player (for legend)
                    toClassUsed[shipClass][ship->playerowner->playerIndex] = 1;
                }
            }

            //for moving ships that belong to the current player, draw the moveline
            if ((ship->moveTo.x != 0.0f || ship->moveTo.y != 0.0f || ship->moveTo.z != 0.0f) && own)
            {
                toMoveLineDraw(ship, scale);
            }

            //Draw Special TO's for Special Ships
            if (own)
            {
                switch (ship->shiptype)
                {
                    case bDFGFrigate:
                    case tDFGFrigate:
                        toFieldSphereDraw(ship, ((DFGFrigateStatics *)ship->staticinfo->custstatinfo)->DFGFrigateFieldRadius,
                                          scale, TW_DFGF_SPHERE_COLOUR);
                        break;
                    case kGravWellGenerator:
                    case tGravWellGenerator:
                        if (((GravWellGeneratorSpec *)ship->ShipSpecifics)->GravFieldOn)
                        {
                            toFieldSphereDraw(ship, ((GravWellGeneratorStatics *)ship->staticinfo->custstatinfo)->GravWellRadius,
                                              scale, TW_GRAVWELL_SPHERE_COLOUR);
                        }
                        break;
                    case kProximitySensor:
                    case tProximitySensor:
                        if (((ProximitySensorSpec *)ship->ShipSpecifics)->sensorState == SENSOR_SENSED ||
                            ((ProximitySensorSpec *)ship->ShipSpecifics)->sensorState == SENSOR_SENSED2)
                        {
                            toDrawRadialIndicator4(ship, ((ProximitySensorStatics *)ship->staticinfo->custstatinfo)->SearchRadius,
                                                   scale, TW_PROXIMITY_RING_COLOUR);
                            toFieldSphereDraw(ship, ((ProximitySensorStatics *)ship->staticinfo->custstatinfo)->SearchRadius,
                                              scale, TW_PROXIMITY_SPHERE_COLOUR_FOUND);
                        }
                        else
                        {
                            toFieldSphereDraw(ship, ((ProximitySensorStatics *)ship->staticinfo->custstatinfo)->SearchRadius,
                                              scale, TW_PROXIMITY_SPHERE_COLOUR);
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : toLegendDraw                                      0x004a8f00
    Description : Draw tactical overlay legend.
                  CAT (M2-R5 hud): drawn by the main overlay (cat_moRender) under the player names, starting at y;
                  returns the y under it. Only the classes CLASS_HeavyCruiser .. CLASS_NonCombat that some player
                  showed, the icon then the class name in 0xffff4040; no player list (the overlay draws the names).
    Inputs      : toClassUsed is set with a previous call to toAllShipsDraw()
    Outputs     : Draw a legend for the tactical display icons.
    Return      : the y under the legend
----------------------------------------------------------------------------*/
sdword toLegendDraw(sdword y)
{
    sdword rowHeight, shipClass, pl, index, x;
    toicon *icon;
    color col;
    real32 radius;

    if (!mrDrawTactical)
    {
        return y;
    }
    (void)fontCurrentGet();                    // the exe does not restore the font
    fontMakeCurrent(selGroupFont2);

    rowHeight = fontHeight(" "); // used to space the legend
    y += rowHeight;
    radius = primScreenToGLScaleX(rowHeight) * 0.5f;
    x = (sdword)(rowHeight * 2.5);

    col = teNeutralColor;
    col = colRGB(colRed(col)/TO_IconColorFade, colGreen(col)/TO_IconColorFade, colBlue(col)/TO_IconColorFade);

    // draw legend entries for any classes currently displayed
    for (shipClass = CLASS_Mothership+1; shipClass <= CLASS_NonCombat; ++shipClass)
    {
        for (pl = 0; pl < TO_NumPlayers; ++pl)
        {
            if (toClassUsed[shipClass][pl])
                break;
        }
        if (pl < TO_NumPlayers)
        {
            // icon
            icon = toClassIcon[shipClass];
            primLineLoopStart2(1, col);

            for (index = icon->nPoints - 1; index >= 0; index--)
            {
               primLineLoopPoint3F(radius * icon->loc[index].x + primScreenToGLX(rowHeight*1.5),
                                   radius * icon->loc[index].y + primScreenToGLY(y + rowHeight/2));
            }
            primLineLoopEnd2();

            // text
            fontPrint(x, (rowHeight >> 2) + y, 0xffff4040, ShipClassToNiceStr((ShipClass)shipClass));
            y += rowHeight + 1;
        }
    }
    return y;
}
