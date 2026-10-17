/*=============================================================================
    Name    : ShipView.c
    Purpose : Render a specific ship to a window.

    Created 7/27/1998 by pgrant
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "ShipView.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "Camera.h"
#include "CameraCommand.h"
#include "ConsMgr.h"
#include "FastMath.h"
#include "FEColour.h"
#include "FEReg.h"
#include "font.h"
#include "FontReg.h"
#include "glinc.h"
#include "Gun.h"
#include "Light.h"
#include "Mesh.h"
#include "mouse.h"
#include "prim2d.h"
#include "Options.h"
#include "render.h"
#include "rglu.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "StatScript.h"
#include "StringSupport.h"
#include "Types.h"
#include "Tweak.h"
#include "Universe.h"
#include "Task.h"


/*=============================================================================
    CAT (M2-R5 hud): Cataclysm's ship view (cataclysm.exe 0x00514c20..0x005158b0) is the construction manager's
    rotating model of the selected type with its name, cost and hot key, and with CM_ExtendedInfo the type's
    description; HW1's six stat panels (SV_Firepower .. SV_Mass, svFirepowerRender .. svMassRender, svShipCoverage,
    svShipManeuverability) and svDirtyShipView are gone from the exe and retired here.
=============================================================================*/

/*=============================================================================
    Private Types:
=============================================================================*/

#define SV_ShipViewName             "SV_ShipView"

#define SV_InitialCameraDistance    900.0f

#define SV_FontNameLength           64
#define SV_DefaultFont              "default.hff"
#define SV_StatFont                 "HW_EuroseCond_11.hff" //"Small_Fonts_8.hff"

#define SV_ViewMargin               8
#define SV_TextMargin               10              // CAT: the text starts 10 pixels in (HW1 2 + SV_ViewMargin)
#define SV_TextColour               colRGB(0xff, 0xc8, 0x00)    // 0xff00c8ff
#define SV_DescriptionColour        colRGB(0x79, 0xff, 0xee)    // 0xffeeff79
#define SV_NearClip                 5.0             // CAT: the exe's near plane (a literal, not svCamera.clipPlaneNear)
#define SV_SpinFeedback             0.5             // 0x0072a480
#define SV_AutoRotateRate           (1.0f / 6.0f)   // 0x0072afd4: one turn in 6 s
#define SV_TWOPI                    6.2831854820251465  // 0x0072a500

regionhandle svShipViewRegion  = NULL;              // 0x00a5e254

fonthandle svShipViewFont = 0;                      // 0x00a5e258
fonthandle svShipStatFont = 0;                      // 0x00a5e25c

bool32 ReZoom=TRUE;                                 // 0x00893c50

real32 savecamMouseX = 0.0f;                        // 0x00a5e260

char svShipStatFontName[SV_FontNameLength] = SV_StatFont;

/*=============================================================================
    Data:
=============================================================================*/

Camera svCamera;                                    // 0x00c0a740

real32 svAngle         = -150.0;
real32 svDeclination   = -20.0;
real32 svZoomOutScalar = 1.1f;                      // 0x00893cdc
real32 svZoomInScalar  = 1.05f;                     // 0x00893ce0

scriptEntry ShipViewTweaks[] =
{
    makeEntry(svAngle, scriptSetReal32CB),
    makeEntry(svDeclination, scriptSetReal32CB),
    makeEntry(svZoomInScalar, scriptSetReal32CB),
    makeEntry(svZoomOutScalar,scriptSetReal32CB),

    END_SCRIPT_ENTRY
};

ShipType svShipType = DefaultShip;                  // 0x00893d24

sdword svMouseCentreX = 0;                          // 0x00a5e26c
sdword svMouseCentreY = 0;                          // 0x00a5e270
sdword svMouseLastX   = 0;                          // 0x00a5e274
sdword svMouseLastY   = 0;                          // 0x00a5e278

bool8 svMouseInside     = FALSE;                    // 0x00a5e27c
bool8 svMousePressLeft  = FALSE;                    // 0x00a5e27d
bool8 svMousePressRight = FALSE;                    // 0x00a5e27e

// CAT: the automatic turn: the camera angle at the selection (0 = the user turned the view; 1e-5 stands for an
//      angle of exactly 0) and, in render.c, the selection's real time (catRndShipViewSelectTime 0x00a5e264, which
//      also times the fade-in)
static real32 svAutoRotateStart = 0.0f;             // 0x00a5e268

static GLfloat lightPosition0[] = {10000.0f, 1.0f, 1.0f, 0.1f};    // 0x00893d28

// ResearchGUI.c's word splitter (no prototype in a header)
extern char *getWord(char *dest, char *source);

// Mesh.c's wireframe switch (0x00a4c456): the second pass of the fade-in is drawn as lines
extern bool8 g_WireframeHack;

/*=============================================================================
    Functions:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : svSelectShip                                      0x00514c20
    Description : Show a type; a new type restarts the automatic turn and the fade-in from the current angle
    Inputs      : type - the type, DefaultShip for none
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void svSelectShip(ShipType type)
{
    if (svShipType != type)
    {
        svAutoRotateStart = svCamera.angle;
        if (svCamera.angle == 0.0f)
        {
            svAutoRotateStart = 1e-05f;
        }
        catRndShipViewSelectTime = cat_taskRealTimeElapsed;
    }
    svShipType = type;
    ReZoom = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : svClose                                           0x00514c70
    Description : The construction manager closed: no region, no type
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void svClose(void)
{
    svShipViewRegion = NULL;
    svShipType = DefaultShip;
}

/*-----------------------------------------------------------------------------
    Name        : svReadMouseEvent
    Description : Callback for reading mouse events for use by svShipViewRender
    Inputs      : standard region callback
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
sdword svReadMouseEvent(regionhandle region, sdword ID, udword event, udword data)
{
    rectangle *rect = &region->rect;
    rectangle defaultRect = {0, 0, MAIN_WindowWidth, MAIN_WindowHeight};

    switch(event)
    {
    case RPE_WheelUp:
        wheel_up = TRUE;
        break;

    case RPE_WheelDown:
        wheel_down = TRUE;
        break;

    case RPE_Enter:
        svMouseInside = TRUE;
        break;

    case RPE_Exit:
        svMouseInside     = FALSE;
        svMousePressRight = FALSE;
        svMousePressLeft  = FALSE;
        break;

    case RPE_PressRight:
        // Save last known position of cursor
        svMouseLastX = mouseCursorX();
        svMouseLastY = mouseCursorY();

        // Centre mouse
        svMouseCentreX = (rect->x0 + rect->x1) >> 1;
        svMouseCentreY = (rect->y0 + rect->y1) >> 1;
        mousePositionSet(svMouseCentreX, svMouseCentreY);
        mouseClipToRect(rect);

        svMousePressRight = TRUE;
        break;

    case RPE_PressLeft:
        svMousePressLeft = TRUE;
        break;

    case RPE_ReleaseRight:
        mousePositionSet(svMouseLastX, svMouseLastY);
        mouseClipToRect(&defaultRect);
        svMousePressRight = FALSE;
        break;

    case RPE_ReleaseLeft:
        svMousePressLeft = FALSE;
        break;
    }

    return 0;
}


/*-----------------------------------------------------------------------------
    Name        : cat_svShipDescriptionDraw                         0x00514dd0
    Description : With CM_ExtendedInfo on, the type's description word-wrapped under the cost (the line breaking of
                  HW1's rmTechInfoDraw: getWord, 15 pixels short of the width, a '\n' ends a line)
    Inputs      : rect - the view's rectangle, y - the last text line's y
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void cat_svShipDescriptionDraw(rectangle *rect, sdword y)
{
    fonthandle oldFont = fontMakeCurrent(svShipViewFont);
    sdword x = rect->x0;
    sdword width = rect->x1 - rect->x0 - 15;
    char text[652], line[100], oldline[100];
    char *pos, *oldpos;
    bool32 done, justified;

    y += fontHeight(" ") * 2;
    if (svShipType == -1)
    {
        fontMakeCurrent(oldFont);
        return;
    }
    // the descriptions follow the type order from strsReconInfo; dAcolyte has its own string
    snprintf(text, sizeof(text), "%s", strGetString((svShipType == dAcolyte) ? strdAcolyteInfo : (strsReconInfo + svShipType)));
    pos = text;
    done = FALSE;
    while (!done)
    {
        justified = FALSE;
        line[0] = '\0';
        while (!justified)
        {
            strcpy(oldline, line);
            oldpos = pos;
            pos = getWord(line, pos);
            if (pos[0] == '\n')
            {
                if (fontWidth(line) > width)
                {
                    strcpy(line, oldline);
                    pos = oldpos;
                    while (pos[0] == ' ')
                    {
                        pos++;
                    }
                }
                else
                {
                    pos++;
                    while (pos[0] == ' ')
                    {
                        pos++;
                    }
                }
                justified = TRUE;
            }
            else
            {
                if (fontWidth(line) > width)
                {
                    strcpy(line, oldline);
                    pos = oldpos;
                    while (pos[0] == ' ')
                    {
                        pos++;
                    }
                    justified = TRUE;
                }
                if (pos[0] == '\0')
                {
                    justified = TRUE;
                    done = TRUE;
                }
            }
        }
        fontPrint(x + SV_TextMargin, y, SV_DescriptionColour, line);
        y += fontHeight(" ");
        if (y > rect->y1 + fontHeight(" "))
        {
            done = TRUE;
        }
    }
    fontMakeCurrent(oldFont);
}

/*-----------------------------------------------------------------------------
    Name        : svShipViewRender                                  0x00515010
    Description : Callback which draws the main ship view: the model turning by itself (or by the right mouse
                  button / the wheel), faded in by a clip plane for 1.5 s after a selection (a solid pass and a
                  wireframe pass, cat_rndSetupFadeFlag), then the name, the cost, the hot key and the description.
                  CAT: no glcompat in the SDL build (cat_glcActive 0x0055c480 is FALSE), so the exe's glc branches
                  (draw every other frame into the saved region) are left out.
    Inputs      : standard FE callback
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
void svShipViewRender(featom* atom, regionhandle region)
{
    rectangle drawRect;
    rectangle viewRect;
    rectangle darkRect;
    fonthandle currentFont;
    GLint viewPort[4];
    GLint box[4];
    GLdouble clipPlane[4] = { -1.0, 0.0, 0.0, -1.0 };
    ShipStaticInfo* info;
    sdword width, height;
    sdword x, y, index, cost;
    ubyte key;
    char* keystring;
    real64 whole;

    (void)atom;
    viewRect.x0 = 0;
    viewRect.y0 = 0;
    viewRect.x1 = MAIN_WindowWidth  - 1;
    viewRect.y1 = MAIN_WindowHeight - 1;
    info = NULL;

    if (svShipType != DefaultShip)
    {
        if (universe.curPlayerPtr != NULL)
        {
            info = GetShipStaticInfoSafe(svShipType, 0);
        }
        if (info == NULL)
        {
            info = GetShipStaticInfoSafe(svShipType, 0);
        }
        if (info == NULL)
        {
            return;
        }
    }

    svShipViewRegion = region;
    ferDrawFocusWindow(region, svMouseInside ? lw_focus : lw_normal);

    soundEventUpdate();

    currentFont = fontMakeCurrent(svShipViewFont);

    if (region->flags == 0 || region->flags == RPE_DrawFunctionAdded)
    {                                         //if region not processed yet
        region->flags =
            RPE_Enter | RPE_Exit |
            RPE_WheelDown | RPE_WheelUp |
            RPE_PressLeft | RPE_ReleaseLeft |
            RPE_PressRight | RPE_ReleaseRight;

        regFunctionSet(region, (regionfunction) svReadMouseEvent);               //set new region handler function
    }

    if (svShipType != DefaultShip)
    {
        svCamera.closestZoom = svZoomInScalar * info->minimumZoomDistance;
        svCamera.farthestZoom = (svCamera.closestZoom + info->common.staticCollInfo.approxcollspheresize) * svZoomOutScalar;
        if (ReZoom)
        {
            ReZoom = FALSE;
            cameraZoom(&svCamera, 1.0f, FALSE);
        }
        if (svMouseInside)
        {
            if (wheel_up || wheel_down)
            {                                   // the wheel zooms and stops the automatic turn
                svAutoRotateStart = 0.0f;
                cameraControl(&svCamera, FALSE);
            }
            else if (svMousePressRight)
            {                                   // the right button turns the model
                camMouseX = (svMouseCentreX - mouseCursorX()) * 4;
                camMouseY = (svMouseCentreY - mouseCursorY()) * 4;
                svAutoRotateStart = 0.0f;
                savecamMouseX = (real32)(((real32)camMouseX + savecamMouseX) * SV_SpinFeedback);
                cameraControl(&svCamera, FALSE);
                mouseCursorHide();
                mousePositionSet(svMouseCentreX, svMouseCentreY);   // Reset position so it doesn't walk off region
            }
            else
            {
                mouseCursorShow();
            }
        }
    }
    if (svAutoRotateStart != 0.0f)
    {                                           // one turn in 6 s from the angle at the selection
        cat_cameraSetAngle(&svCamera, (real32)(svAutoRotateStart +
            modf((real64)((cat_taskRealTimeElapsed - catRndShipViewSelectTime) * SV_AutoRotateRate), &whole) * SV_TWOPI));
    }

    drawRect.x0 = region->rect.x0 + SV_ViewMargin;
    drawRect.y0 = region->rect.y0 + SV_ViewMargin;
    drawRect.x1 = region->rect.x1 - SV_ViewMargin;
    drawRect.y1 = region->rect.y1 - SV_ViewMargin;

    width  = drawRect.x1 - drawRect.x0;
    height = drawRect.y1 - drawRect.y0;

    glGetIntegerv(GL_VIEWPORT, viewPort);

    // GoK: Since we request a fullscreen window and not true fullscreen the MAIN_WindowX values do not represent
    // the actual render resolution; only the ship view sets the viewport and trips over this.
    // original code: glViewport(drawRect.x0, MAIN_WindowHeight - drawRect.y1, width, height);
    float factorX = (float)viewPort[2]/(float)MAIN_WindowWidth;
    float factorY = (float)viewPort[3]/(float)MAIN_WindowHeight;
    glViewport(drawRect.x0 * factorX, viewPort[3] - drawRect.y1 * factorY, width * factorX, height * factorY);

    primModeSet2();
    primRectSolid2(&viewRect, FEC_Background);
    primModeClear2();

    glEnable(GL_SCISSOR_TEST);
    glGetIntegerv(GL_SCISSOR_BOX, box);
    glScissor(drawRect.x0, MAIN_WindowHeight - drawRect.y1, width, height);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    // CAT: rndPerspectiveCorrection(TRUE) is not issued (render.c: the hint has no effect on a current GL)
    cat_rndDepthTestEnable(TRUE);

    if (svShipType == DefaultShip)
    {
        svCamera.lookatpoint.x = 0.0f;
        svCamera.lookatpoint.y = 0.0f;
        svCamera.lookatpoint.z = 0.0f;
    }
    else
    {                                           // CAT: the exe negates only z (HW1 negated all three)
        svCamera.lookatpoint.x = -info->common.staticCollInfo.collsphereoffset.z;
        svCamera.lookatpoint.y = info->common.staticCollInfo.collsphereoffset.y;
        svCamera.lookatpoint.z = info->common.staticCollInfo.collsphereoffset.x;
    }

    cameraSetEyePosition(&svCamera);

    rndLightingEnable(TRUE);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    rgluPerspective(svCamera.fieldofview, (double)width / (double)height, SV_NearClip, svCamera.clipPlaneFar);

    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();

    rgluLookAt(
        svCamera.eyeposition.x,
        svCamera.eyeposition.y,
        svCamera.eyeposition.z,
        svCamera.lookatpoint.x,
        svCamera.lookatpoint.y,
        svCamera.lookatpoint.z,
        svCamera.upvector.x,
        svCamera.upvector.y,
        svCamera.upvector.z);

    glPushMatrix();

    glRotatef(-90.0f, 0.0f, 1.0f, 0.0f);

    glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat *)(&rndCameraMatrix));
    glGetFloatv(GL_PROJECTION_MATRIX, (GLfloat *)(&rndProjectionMatrix));

    glEnable(GL_NORMALIZE);

    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition0);      //position light(s) within world

    glScalef(1.0f, 1.0f, 1.0f);

    if (svShipType != DefaultShip)
    {
        //try player index colours first
        index = universe.curPlayerIndex;
        if (info->shipOrDerelict.teamColor[index] == 0)
        {
            //colour scheme doesn't exist, search for something valid
            for (index = 0; index < MAX_MULTIPLAYER_PLAYERS; index++)
            {
                if (info->shipOrDerelict.teamColor[index] != 0)
                {
                    break;
                }
            }
            if (index == MAX_MULTIPLAYER_PLAYERS)
            {
                index = universe.curPlayerIndex;
            }
        }
        // CAT: the effect state of no object: only the fade-in (cat_rndSetupFadeFlag)
        cat_rndObjectEffectsSetup(NULL);
        cat_rndSetupObjectEffectParams(NULL);
        if (catRndFadeFlag)
        {                                       // the solid part: x below the fade's height up the model
            clipPlane[3] = catRndFadeT * info->common.staticCollInfo.uplength +
                           info->common.staticCollInfo.collrectoffset.x;
            glClipPlane(GL_CLIP_PLANE0, clipPlane);
            glEnable(GL_CLIP_PLANE0);
            g_WireframeHack = FALSE;
        }
        meshRender((meshdata *)info->common.LOD->level[0].pData, index);
        if (catRndFadeFlag)
        {                                       // and the rest as a wireframe
            g_WireframeHack = TRUE;
            catRndFadeFlip = TRUE;
            glDisable(GL_CLIP_PLANE0);
            clipPlane[0] = -clipPlane[0];
            clipPlane[3] = -clipPlane[3];
            glClipPlane(GL_CLIP_PLANE0, clipPlane);
            glEnable(GL_CLIP_PLANE0);
            meshRender((meshdata *)info->common.LOD->level[0].pData, index);
            glDisable(GL_CLIP_PLANE0);
            g_WireframeHack = FALSE;
        }
    }

    glDisable(GL_NORMALIZE);

    glPopMatrix();

    primModeSet2();

    glScissor(box[0], box[1], box[2], box[3]);
    glViewport(viewPort[0], viewPort[1], viewPort[2], viewPort[3]);

    rndLightingEnable(FALSE);

    x = region->rect.x0 + SV_TextMargin;
    y = region->rect.y0 + SV_ViewMargin;

    if (catCmExtendedInfo)
    {                                           // the description gets a darkened panel
        darkRect.x0 = region->rect.x0 + 3;
        darkRect.y0 = region->rect.y0 + 3;
        darkRect.x1 = region->rect.x1 - 3;
        darkRect.y1 = region->rect.y1 - 3;
        primRectTranslucent2(&darkRect, colRGBA(0, 0, 0, 0x80));
    }

    if (svShipType != DefaultShip)
    {
        fontPrint(x, y, SV_TextColour, ShipTypeToNiceStr(svShipType));

        y += fontHeight(" ");

        cost = singlePlayerGame ? info->buildCost / catCmSinglePlayerCostFactor : info->buildCost;
        fontPrintf(x, y, SV_TextColour, "%s %d %s", strGetString(strSVCost), cost, strGetString(strSVRUs));

        if (cmPrintHotKey)
        {
            x = region->rect.x1 - SV_TextMargin;
            y = region->rect.y0 + SV_ViewMargin;

            // CAT: the exe keeps the key's low byte, so its "[SHIFT-%s]" branch (key & CM_SHIFT) is dead code
            key = (ubyte)cmShipTypeToKey(svShipType);
            keystring = opKeyToNiceString((keyindex)key);
            if (key != 0)
            {
                width = fontWidthf("[%s]", keystring);
                fontPrintf(x - width, y, SV_TextColour, "[%s]", keystring);
            }
        }
    }
    if (catCmExtendedInfo)
    {
        cat_svShipDescriptionDraw(&region->rect, y);
    }
    fontMakeCurrent(currentFont);

    // CAT: HW1's svDirtyShipView, inlined (only the one region is left)
    if (svShipViewRegion != NULL)
    {
        svShipViewRegion->status |= RSF_DrawThisFrame;
    }
}

/*-----------------------------------------------------------------------------
    Name        : svStartup                                         0x005158b0
    Description : The camera, the SV_ShipView callback and the two fonts (the view's is always default.hff)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void svStartup(void)
{
    cameraInit(&svCamera, SV_InitialCameraDistance);

    svCamera.angle = DEG_TO_RAD(svAngle);
    svCamera.declination = DEG_TO_RAD(svDeclination);

    feDrawCallbackAdd(SV_ShipViewName, svShipViewRender);   //add render callbacks

    svMouseInside     = FALSE;
    svMousePressLeft  = FALSE;
    svMousePressRight = FALSE;

    svShipViewFont = frFontRegister(SV_DefaultFont);
    svShipStatFont = frFontRegister(svShipStatFontName);
}

/*-----------------------------------------------------------------------------
    Name        :  svShutdown()
    Description : CAT: not in the exe (utility.c's HW1 shutdown still calls it)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void svShutdown(void)
{
}
