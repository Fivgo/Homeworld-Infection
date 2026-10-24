/*=============================================================================
        Name    : hs.c
        Purpose : hyperspace rendering & management routines

Created 7/10/1998 by khent
Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include <ctype.h>
#include <math.h>

#include "AIVar.h"
#include "Damage.h"
#include "Debug.h"
#include "FEReg.h"
#include "glinc.h"
#include "Globals.h"
#include "GravWellGenerator.h"
#include "HS.h"
#include "Matrix.h"
#include "Memory.h"
#include "Particle.h"
#include "prim3d.h"
#include "render.h"
#include "SaveGame.h"
#include "SaveCat32.h"     // CAT (M2-R8 singleplayer-tutorial): a cataclysm.exe save's mission tail
#include "SinglePlayer.h"
#include "UnivUpdate.h"

//scalar that determines distance out of bbox the window will travel from / to
#define HS_DIST   1.08f
#define HS_DIST_2 (HS_DIST / 2.0f)

#define SECOND_RECT 1

#define hsDefaultColor colRGB(70,90,255)


//guaranteed to be at least 1 gate when the singleplayer game is running
static sdword hsStaticNumGates = 0;
static hsStaticGate* hsStaticData = NULL;
static bool32 hsGateState = TRUE;

void hsRectangle(vector* origin, real32 rightlength, real32 uplength, ubyte alpha, bool32 outline, color c);

/*-----------------------------------------------------------------------------
    The gate's shape. CAT: cataclysm.exe sizes the gate rectangle and the effect's up / right scales by the static's
    hyperspaceBoxScale (ShipStaticInfo +0x3D0, default 1.0; hsStart 0x0048f1c0, hsOrientEffect 0x0048f070, hsEnd
    0x0048fd10: the sMothership's .shp sets 6.0, "Make the hyperspace around the spine encompass all the
    components"); the clip plane sweeps the collision box's forward length unscaled (hsGetEquation 0x0048f320,
    hsContinue 0x0048f400: +forwardlength .. -forwardlength), and the effect's position follows the plane.
    HWI-FEATURE(hyperspace-assembly): for a host with SOF_CatShipPart parts the sweep also covers the assembly's forward
    extent (the union of the host's and its parts' collision boxes in the host's frame, by the clamp offsets) and the
    rectangle the bounding rectangle of the exe's gate and that union, so the gate and the reveal span every part.
----------------------------------------------------------------------------*/
bool32 hsAssemblyEnabled = TRUE;                    // HWI-FEATURE(hyperspace-assembly): /noHsAssembly clears it (run-only)

typedef struct hsGateShape                          // HWI-FEATURE(hyperspace-assembly)
{
    real32 up, right;       // the rectangle's lengths (hsRectangle's uplength / rightlength; the effect's tumble)
    real32 cx, cy;          // the rectangle's centre from the collision sphere's, in the ship's frame (the exe: 0, 0)
    real32 front, back;     // the clip plane's model z at clipt 1 / -1 (the exe: +forwardlength / -forwardlength)
} hsGateShape;

typedef struct hsAssemblyBox                        // HWI-FEATURE(hyperspace-assembly)
{
    vector min, max;        // the union of the collision boxes, in the host's model frame
    sdword parts;           // the parts counted
} hsAssemblyBox;

// HWI-FEATURE(hyperspace-assembly): the union of host's collision box and its live SOF_CatShipPart parts' boxes, each
//      part's corners put in the host's frame as updateClampedObject places the part (clampOffset + clampCoordsys);
//      FALSE for a ship that is not a host of parts. Reads only.
static bool32 hsAssemblyBoxGet(Ship* host, hsAssemblyBox* box)
{
    StaticCollInfo* sinfo = &host->staticinfo->common.staticCollInfo;
    CatClampedList* clamped;
    sdword i, c;

    if ((host->guidance.clampInfo == NULL) || (host->guidance.clampInfo->host != NULL) ||
        ((clamped = host->guidance.clampInfo->clampedObjs) == NULL))
    {
        return FALSE;
    }
    box->min = sinfo->collrectoffset;
    box->max.x = sinfo->collrectoffset.x + sinfo->uplength;
    box->max.y = sinfo->collrectoffset.y + sinfo->rightlength;
    box->max.z = sinfo->collrectoffset.z + sinfo->forwardlength;
    box->parts = 0;
    for (i = 0; i < clamped->numShips; i++)
    {
        Ship* part = clamped->ShipPtr[i];
        StaticCollInfo* psinfo;

        if ((part == NULL) || (part->header.objtype != OBJ_ShipType) || !bitTest(part->header.flags, SOF_CatShipPart) ||
            bitTest(part->header.flags, SOF_Dead) || (part->guidance.clampInfo == NULL))
        {
            continue;
        }
        psinfo = &part->staticinfo->common.staticCollInfo;
        for (c = 0; c < 8; c++)
        {
            vector corner, inHost;

            corner.x = psinfo->collrectoffset.x + ((c & 1) ? psinfo->uplength : 0.0f);
            corner.y = psinfo->collrectoffset.y + ((c & 2) ? psinfo->rightlength : 0.0f);
            corner.z = psinfo->collrectoffset.z + ((c & 4) ? psinfo->forwardlength : 0.0f);
            matMultiplyMatByVec(&inHost, &part->guidance.clampInfo->clampCoordsys, &corner);
            vecAddTo(inHost, part->guidance.clampInfo->clampOffset);
            box->min.x = min(box->min.x, inHost.x);
            box->min.y = min(box->min.y, inHost.y);
            box->min.z = min(box->min.z, inHost.z);
            box->max.x = max(box->max.x, inHost.x);
            box->max.y = max(box->max.y, inHost.y);
            box->max.z = max(box->max.z, inHost.z);
        }
        box->parts++;
    }
    return (box->parts > 0);
}

// the gate's shape for ship: the exe's (CAT, above), then the assembly's on top (HWI-FEATURE(hyperspace-assembly))
static void hsGateShapeGet(Ship* ship, hsGateShape* g, hsAssemblyBox* boxOut)
{
    StaticCollInfo* sinfo = &ship->staticinfo->common.staticCollInfo;
    real32 boxScale = ship->staticinfo->hyperspaceBoxScale;
    hsAssemblyBox box;

    // CAT: uplength * boxScale and boxScale * rightlength (hsEnd 0x0048fd10: 0x0048fe79 .. 0x0048fe90)
    g->up = sinfo->uplength * boxScale;
    g->right = boxScale * sinfo->rightlength;
    g->cx = 0.0f;
    g->cy = 0.0f;
    g->front = sinfo->forwardlength;
    g->back = -sinfo->forwardlength;
    if (boxOut != NULL)
    {
        boxOut->parts = 0;
    }

    if (!hsAssemblyEnabled || !hsAssemblyBoxGet(ship, &box))                    // HWI-FEATURE(hyperspace-assembly)
    {
        return;
    }
    if (boxOut != NULL)                                                         // HWI-FEATURE(hyperspace-assembly)
    {
        *boxOut = box;                                                          // HWI-FEATURE(hyperspace-assembly)
    }
    // the sweep: the assembly's forward extent, never inside the exe's                HWI-FEATURE(hyperspace-assembly)
    g->front = max(g->front, box.max.z);                                        // HWI-FEATURE(hyperspace-assembly)
    g->back = min(g->back, box.min.z);                                          // HWI-FEATURE(hyperspace-assembly)
    // the rectangle: the bounding rectangle of the exe's (centred on the collision sphere) and the union's, each with
    //      hsRectangle's margin (HS_DIST_2 of a length either side of its centre)        HWI-FEATURE(hyperspace-assembly)
    {
        vector* cso = &sinfo->collsphereoffset;
        real32 xlo = min(cso->x - HS_DIST_2 * g->up, 0.5f * (box.min.x + box.max.x) - HS_DIST_2 * (box.max.x - box.min.x));
        real32 xhi = max(cso->x + HS_DIST_2 * g->up, 0.5f * (box.min.x + box.max.x) + HS_DIST_2 * (box.max.x - box.min.x));
        real32 ylo = min(cso->y - HS_DIST_2 * g->right, 0.5f * (box.min.y + box.max.y) - HS_DIST_2 * (box.max.y - box.min.y));
        real32 yhi = max(cso->y + HS_DIST_2 * g->right, 0.5f * (box.min.y + box.max.y) + HS_DIST_2 * (box.max.y - box.min.y));

        // the exe's rectangle kept bit for bit when it already covers the union (the Kuun-Lan's, the beast's)
        if ((xlo < cso->x - HS_DIST_2 * g->up) || (xhi > cso->x + HS_DIST_2 * g->up) ||  // HWI-FEATURE(hyperspace-assembly)
            (ylo < cso->y - HS_DIST_2 * g->right) || (yhi > cso->y + HS_DIST_2 * g->right))
        {
            g->up = (xhi - xlo) / HS_DIST;                                      // HWI-FEATURE(hyperspace-assembly)
            g->right = (yhi - ylo) / HS_DIST;                                   // HWI-FEATURE(hyperspace-assembly)
            g->cx = 0.5f * (xlo + xhi) - cso->x;                                // HWI-FEATURE(hyperspace-assembly)
            g->cy = 0.5f * (ylo + yhi) - cso->y;                                // HWI-FEATURE(hyperspace-assembly)
        }
    }
}

// the clip plane's model z at clipt over the sweep (the exe: clipt * forwardlength; exact for front = -back)
static real32 hsGatePlaneZ(hsGateShape* g, real32 clipt)
{
    return 0.5f * (g->front + g->back) + clipt * (0.5f * (g->front - g->back));
}

/*-----------------------------------------------------------------------------
    Name        : hsAssemblyPartPending
    Description : HWI-FEATURE(hyperspace-assembly): TRUE for a part (SOF_CatShipPart) that UpdateArrivingShip still holds
                  hidden (SOF_Hide, SHIPHYPERSPACE_DECELERATE: its own wait of 3 x BASE + random runs) while its host is
                  out and exiting (SHIPHYPERSPACE_DONE, shown): the main view draws it under the host's clip (render.c),
                  so the whole assembly is present from the host's first exit frame. Reads only: the simulation keeps
                  the part hidden and its wait, its own exit and every flag as the exe has them.
----------------------------------------------------------------------------*/
bool32 hsAssemblyPartPending(Ship* part)
{
    ShipSinglePlayerGameInfo* pinfo = part->shipSinglePlayerGameInfo;
    Ship* host;

    if (!hsAssemblyEnabled || (pinfo == NULL) || !bitTest(part->header.flags, SOF_CatShipPart) ||
        bitTest(part->header.flags, SOF_Dead) || !bitTest(part->header.flags, SOF_Hide) ||
        (pinfo->shipHyperspaceState != SHIPHYPERSPACE_DECELERATE) || (part->guidance.clampInfo == NULL))
    {
        return FALSE;
    }
    host = (Ship*)part->guidance.clampInfo->host;
    if ((host == NULL) || (host->header.objtype != OBJ_ShipType) || bitTest(host->header.flags, SOF_Hide | SOF_Dead) ||
        (host->shipSinglePlayerGameInfo == NULL))
    {
        return FALSE;
    }
    return (host->shipSinglePlayerGameInfo->shipHyperspaceState == SHIPHYPERSPACE_DONE);
}

/*-----------------------------------------------------------------------------
    Name        : hsAssemblyLogStart
    Description : HWI-TEST (M3-R2 hyperspace-assembly): at hsStart of a host of parts or of a part, one "hsgate:" line:
                  the tick, the ship, into / out of, the core's box, the gate before (the tree's: boxScale not read), the
                  exe's (boxScale) and the assembly's, the sweep and the union box. Reads only.
----------------------------------------------------------------------------*/
void hsAssemblyLogStart(Ship* ship, bool32 into)
{
    StaticCollInfo* sinfo = &ship->staticinfo->common.staticCollInfo;
    hsGateShape g;
    hsAssemblyBox box;
    bool32 saved = hsAssemblyEnabled;
    real32 exeUp, exeRight;

    if (!bitTest(ship->header.flags, SOF_CatShipPart) &&
        ((ship->guidance.clampInfo == NULL) || (ship->guidance.clampInfo->clampedObjs == NULL)))
    {
        return;
    }
    hsAssemblyEnabled = FALSE;
    hsGateShapeGet(ship, &g, NULL);
    exeUp = g.up;
    exeRight = g.right;
    hsAssemblyEnabled = saved;
    hsGateShapeGet(ship, &g, &box);
    dbgMessagef("hsgate: tick=%u ship=%s id=%u %s part=%d hsShouldDisplayEffect=%d core up=%.1f right=%.1f fwd=%.1f "
                "boxScale=%.2f gate tree up=%.1f right=%.1f exe up=%.1f right=%.1f now up=%.1f right=%.1f c=(%.1f,%.1f) "
                "sweep %.1f..%.1f (exe %.1f..%.1f) assembly parts=%d x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f",
                (unsigned)universe.univUpdateCounter, ShipTypeToStr(ship->shiptype), (unsigned)ship->shipID.shipNumber,
                into ? "into" : "outof", bitTest(ship->header.flags, SOF_CatShipPart) ? 1 : 0,
                hsShouldDisplayEffect(ship), sinfo->uplength, sinfo->rightlength, sinfo->forwardlength,
                ship->staticinfo->hyperspaceBoxScale, sinfo->uplength, sinfo->rightlength, exeUp, exeRight, g.up, g.right,
                g.cx, g.cy, g.front, g.back, sinfo->forwardlength, -sinfo->forwardlength, box.parts,
                box.parts ? box.min.x : 0.0f, box.parts ? box.max.x : 0.0f, box.parts ? box.min.y : 0.0f,
                box.parts ? box.max.y : 0.0f, box.parts ? box.min.z : 0.0f, box.parts ? box.max.z : 0.0f);
}

/*-----------------------------------------------------------------------------
    Name        : hsStartup
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsStartup(void)
{
    //nothing here
}

/*-----------------------------------------------------------------------------
    Name        : hsShutdown
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsShutdown(void)
{
    //nothing here
}

/*-----------------------------------------------------------------------------
    Name        : hsOrientEffect
    Description : ensures that effect is pointing in the appropriate direction.
                  *NOTE* worldspace effects only
    Inputs      : ship - the ship whose effect will be oriented
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsOrientEffect(Ship* ship)
{
    sdword    index;
    psysPtr   system;
    particle* part;

    ShipSinglePlayerGameInfo* ssinfo = ship->shipSinglePlayerGameInfo;
    StaticCollInfo* sinfo = &ship->staticinfo->common.staticCollInfo;
    Effect* effect = ssinfo->hyperspaceEffect;
    hsGateShape gate;

    dbgAssertOrIgnore(effect != NULL);
    hsGateShapeGet(ship, &gate, NULL);

    for (index = 0; index < effect->iParticleBlock; index++)
    {
        if (effect->particleBlock[index] != NULL)
        {
            system = (psysPtr) effect->particleBlock[index];
            part = (particle*)((ubyte*)system + partHeaderSize(system));

            bitSet(part->flags, PART_XYZSCALE);

            part->wVel.x = 0.0f;
            part->wVel.y = -1.0f;
            part->wVel.z = 0.0f;

            // CAT: the up and right scales times hyperspaceBoxScale (hsOrientEffect 0x0048f070: boxScale * uplength * 1.08,
            //      boxScale * rightlength * 1.08), the gate rectangle's lengths (hsGateShapeGet; the assembly's with
            //      HWI-FEATURE(hyperspace-assembly): the particle's size only, a render value)
            part->tumble[0] = gate.up * HS_DIST;
            part->tumble[1] = gate.right * HS_DIST;
            part->tumble[2] = 0.25f * sinfo->forwardlength;

            switch (ssinfo->hsState)
            {
            case HS_POPUP_INTO:
            case HS_POPUP_OUTOF:
                part->tumble[0] *= 1.0f - ssinfo->clipt;
                break;

            case HS_COLLAPSE_INTO:
            case HS_COLLAPSE_OUTOF:
                part->tumble[0] *= ssinfo->clipt;
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : hsShouldDisplayEffect
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 hsShouldDisplayEffect(Ship* ship)
{
    //TODO(type safety, 2026-04-23): Take a Guidance pointer instead, for clarity
    bool32  displayEffect;

    if (ship->guidance.clampInfo != NULL)
    {
        if (ship->guidance.clampInfo->host != NULL)// && host->shiptype == AdvanceSupportFrigate)
        {
            displayEffect = FALSE;
        }
        else
        {
            displayEffect = TRUE;
        }
    }
    else
    {
        displayEffect = TRUE;
    }

    return displayEffect;
}

/*-----------------------------------------------------------------------------
    Name        : hsStart
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsStart(Ship* ship, real32 cliptDelta, bool32 into, bool32 displayEffect)
{
    ShipSinglePlayerGameInfo* ssinfo = ship->shipSinglePlayerGameInfo;
    StaticCollInfo* sinfo = &ship->staticinfo->common.staticCollInfo;

    ssinfo->clipt = 1.0f;
    ssinfo->cliptDelta = cliptDelta;
    ssinfo->hsState = into ? HS_POPUP_INTO : HS_POPUP_OUTOF;
    ssinfo->hyperspaceEffect = NULL;

    dmgStopEffect(ship, DMG_All);
    hsAssemblyLogStart(ship, into);         // HWI-TEST (M3-R2 hyperspace-assembly): "hsgate:" (a host of parts or a part)

    if (!displayEffect)
    {
        //don't bother with the tons-of-alpha effect if the horsepower isn't available
        return;
    }

#if ETG_DISABLEABLE
    if (etgEffectsEnabled)
#endif
    {
        etgeffectstatic* stat;
        Effect* effect;
        vector  location;
        real32  floatUpScale, floatRightScale, floatDepthScale;
        sdword  intUpScale, intRightScale, intDepthScale;

        stat = etgHyperspaceEffect->level[0];
        if (stat != NULL)
        {
            vector vecToRotate, vecRotated;

            vecToRotate.x = 0.0f;
            vecToRotate.y = 0.0f;
            vecToRotate.z = (sinfo->forwardlength);
            vecToRotate.z *= ship->magnitudeSquared;
            vecToRotate.z -= sinfo->collsphereoffset.z;

            matMultiplyMatByVec(&vecRotated, &ship->rotatable.rotinfo.coordsys, &vecToRotate);

            location = ship->impactable.collInfo.collPosition;
            vecAddTo(location, vecRotated);

            floatUpScale = 0.0f;//HS_DIST * sinfo->uplength;
            intUpScale = Real32ToSdword(floatUpScale);

            // CAT: the right scale is staticinfo->hyperspaceBoxScale * rightlength * 1.08 (hsStart 0x0048f1c0)
            floatRightScale = ship->staticinfo->hyperspaceBoxScale * sinfo->rightlength * HS_DIST;
            intRightScale = Real32ToSdword(floatRightScale);

            floatDepthScale = 0.25f * sinfo->forwardlength;
            intDepthScale = Real32ToSdword(floatDepthScale);

            dbgAssertOrIgnore(ship->header.objtype != OBJ_BulletType);
            effect = etgEffectCreate(stat, ship,
                                     &location,
                                     &ship->obj.posinfo.velocity,
                                     &ship->rotatable.rotinfo.coordsys, 1.0f,
                                     EAF_NLips, 3, SCALECAST(intUpScale), SCALECAST(intRightScale), SCALECAST(intDepthScale));

            ssinfo->hyperspaceEffect = effect;

            hsOrientEffect(ship);
        }
    }
}

#ifdef HW_ENABLE_GLES
void hsGetEquation(Ship* ship, GLfloat equation[])
#else
void hsGetEquation(Ship* ship, GLdouble equation[])
#endif
{
    ShipSinglePlayerGameInfo* ssinfo = ship->shipSinglePlayerGameInfo;
    hsGateShape gate;

    // CAT: the exe's planes (hsGetEquation 0x0048f320), POPUP_INTO among them (-1, +forwardlength: the whole ship kept
    //      while the window opens; GoK had no case, so its default plane z >= 0 cut a part's back half while its host
    //      popped up into hyperspace); the sweep is the gate's (hsGateShapeGet: the exe's +-forwardlength, the
    //      assembly's with HWI-FEATURE(hyperspace-assembly))
    hsGateShapeGet(ship, &gate, NULL);

    equation[0] = 0.0f;
    equation[1] = 0.0f;
    equation[2] = 1.0f;
    equation[3] = 0.0f;

    switch (ssinfo->hsState)
    {
    case HS_POPUP_INTO:
        equation[2] = -1.0f;
        equation[3] = gate.front;
        break;
    case HS_SLICING_INTO:
        equation[2] = -1.0f;
        equation[3] = hsGatePlaneZ(&gate, ssinfo->clipt);
        break;
    case HS_COLLAPSE_INTO:
        equation[2] = -1.0f;
        equation[3] = gate.back;
        break;
    case HS_POPUP_OUTOF:
        equation[2] = 1.0f;
        equation[3] = -gate.front;
        break;
    case HS_SLICING_OUTOF:
        equation[2] = 1.0f;
        equation[3] = -hsGatePlaneZ(&gate, ssinfo->clipt);
        break;
    case HS_COLLAPSE_OUTOF:
        equation[2] = 1.0f;
        equation[3] = -gate.back;
    }
}

/*-----------------------------------------------------------------------------
    Name        : hsContinue
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsContinue(Ship* ship, bool32 displayEffect)
{
#ifdef HW_ENABLE_GLES
    GLfloat equation[4] = {0.0, 0.0, 1.0, 0.0};
#else
    GLdouble equation[4] = {0.0, 0.0, 1.0, 0.0};
#endif
    StaticCollInfo* sinfo = &ship->staticinfo->common.staticCollInfo;
    ShipSinglePlayerGameInfo* ssinfo = ship->shipSinglePlayerGameInfo;
    hsGateShape gate;

    if (!displayEffect)
    {
        if (ship->guidance.clampInfo != NULL)
        {
            Ship* host = (Ship*)ship->guidance.clampInfo->host;
            // CAT: a clamped ship is clipped by its host's plane only while the host is in hyperspace (hsContinue
            //      0x0048f400 at 0x0048f4a6: the host's hsState 0 disables GL_CLIP_PLANE0 instead); GoK clipped it by
            //      its default plane (z >= 0 in the host's frame: the back half) once the host was out
            if ((host != NULL) && (host->shipSinglePlayerGameInfo != NULL) &&
                (host->shipSinglePlayerGameInfo->hsState == HS_INACTIVE))
            {
                glDisable(GL_CLIP_PLANE0);
                return;
            }
            if (host != NULL)// && host->shiptype == AdvanceSupportFrigate)
            {
                //we're in NLIP'ed shipspace;

                hmatrix coordMatrixForGL, prevModelview;
                real32 nliphak;
                hsGetEquation(host, equation);
                        
                glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat*)&prevModelview);
                glPopMatrix();
                glPushMatrix();
                
                //put glstack info similar state as before
                hmatMakeHMatFromMat(&coordMatrixForGL,&host->rotatable.rotinfo.coordsys);
                hmatPutVectIntoHMatrixCol4(host->obj.posinfo.position,coordMatrixForGL);
                glMultMatrixf((float *)&coordMatrixForGL);//ship's rotation matrix

                nliphak = host->magnitudeSquared;
                glScalef(nliphak,nliphak,nliphak);

#ifdef HW_ENABLE_GLES
                glClipPlanef(GL_CLIP_PLANE0, equation);
#else
                glClipPlane(GL_CLIP_PLANE0, equation);
#endif
                glEnable(GL_CLIP_PLANE0);

                glLoadMatrixf((GLfloat*)&prevModelview);
            }
        }
        return;
    }

    if (ssinfo->hyperspaceEffect != NULL)
    {
        //appropriately orient effect
        hsOrientEffect(ship);
    }

    // the planes over the gate's sweep (hsGateShapeGet: the exe's +-forwardlength; the assembly's with
    //      HWI-FEATURE(hyperspace-assembly), so a host's own clip uncovers its parts' extent too)
    hsGateShapeGet(ship, &gate, NULL);
    switch (ssinfo->hsState)
    {
    case HS_POPUP_INTO:
        return;
    case HS_SLICING_INTO:
        equation[2] = -1.0f;
        equation[3] = hsGatePlaneZ(&gate, ssinfo->clipt);
        break;
    case HS_COLLAPSE_INTO:
        if (!singlePlayerGameInfo.hyperspaceFails)
        {
            equation[2] = -1.0f;
            equation[3] = gate.back;
        }
        break;
    case HS_POPUP_OUTOF:
        equation[2] = 1.0f;
        equation[3] = -gate.front;//0.0f;
        break;
    case HS_SLICING_OUTOF:
        equation[2] = 1.0f;
        equation[3] = -hsGatePlaneZ(&gate, ssinfo->clipt);
        break;
    case HS_COLLAPSE_OUTOF:
        equation[2] = 1.0f;
        equation[3] = -gate.back;
        break;
    default:
        dbgFatalf(DBG_Loc, "unknown hyperspace state %d", ssinfo->hsState);
    }

    if (ssinfo->hyperspaceEffect != NULL)
    {
        Effect*   effect;
        psysPtr   system;
        particle* part;
        sdword    index;
        vector    vecToRotate, vecRotated;

        vecToRotate.x = gate.cx;                        // HWI-FEATURE(hyperspace-assembly): the rectangle's centre (the exe: 0)
        vecToRotate.y = gate.cy;                        // HWI-FEATURE(hyperspace-assembly)
        vecToRotate.z = -equation[2] * equation[3];
        
        {
            vecToRotate.z *= ship->magnitudeSquared;
        }
        
        vecToRotate.z -= sinfo->collsphereoffset.z;

        matMultiplyMatByVec(&vecRotated, &ship->rotatable.rotinfo.coordsys, &vecToRotate);

        effect = ssinfo->hyperspaceEffect;
        effect->obj.posinfo.position = ship->impactable.collInfo.collPosition;
        vecAddTo(effect->obj.posinfo.position, vecRotated);

        //manhandle the effect into position
        for (index = 0; index < effect->iParticleBlock; index++)
        {
            if (effect->particleBlock[index] != NULL)
            {
                system = (psysPtr) effect->particleBlock[index];
                part = (particle*)((ubyte*)system + partHeaderSize(system));
                part->position = effect->obj.posinfo.position;
            }
        }
    }

    if (!singlePlayerGameInfo.hyperspaceFails)
    {
#ifdef HW_ENABLE_GLES
        glClipPlanef(GL_CLIP_PLANE0, equation);
#else
        glClipPlane(GL_CLIP_PLANE0, equation);
#endif
        glEnable(GL_CLIP_PLANE0);
    }
}

/*-----------------------------------------------------------------------------
    Name        : hsRectangle
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsRectangle(vector* origin, real32 rightlength, real32 uplength, ubyte alpha, bool32 outline, color c)
{
    real32 rlen = HS_DIST_2 * uplength;
    real32 ulen = HS_DIST_2 * rightlength;
    real32 v[12] = { origin->x - rlen, origin->y + ulen, origin->z,
                     origin->x + rlen, origin->y + ulen, origin->z,
                     origin->x - rlen, origin->y - ulen, origin->z,
                     origin->x + rlen, origin->y - ulen, origin->z };
    real32 v2[12] = {origin->x - rlen, origin->y + ulen, origin->z,
                     origin->x + rlen, origin->y + ulen, origin->z,
                     origin->x + rlen, origin->y - ulen, origin->z,
                     origin->x - rlen, origin->y - ulen, origin->z};
    ubyte red = colRed(c);
    ubyte green = colGreen(c);
    ubyte blue = colBlue(c);

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, v);

    glColor4ub(red, green, blue, alpha);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (outline) {
        ubyte l[4] = { 0, 1, 3, 2 };
        glLineWidth(2.0f);
        glColor4ub((ubyte)(red + 40), (ubyte)(green + 40), blue, alpha);
        glVertexPointer(3, GL_FLOAT, 0, v2);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
        glLineWidth(1.0f);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
}

/*-----------------------------------------------------------------------------
    Name        : hsDesat
    Description :
    Inputs      : r, g, b - output colours
                  c - input colour
                  scale - scalar for saturation
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsDesat(ubyte* r, ubyte* g, ubyte* b, color c, real32 scale)
{
    real32 h, s, v;
    real32 fr, fg, fb;

    colRGBToHSV(&h, &s, &v,
                colUbyteToReal(colRed(c)),
                colUbyteToReal(colGreen(c)),
                colUbyteToReal(colBlue(c)));
    s *= scale;
    colHSVToRGB(&fr, &fg, &fb, h, s, v);

    *r = colRealToUbyte(fr);
    *g = colRealToUbyte(fg);
    *b = colRealToUbyte(fb);
}

/*-----------------------------------------------------------------------------
    Name        : hsLine
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsLine(vector* origin, real32 rightlength, real32 uplength, ubyte alpha, color c)
{
    real32 rlen = 0.8f * rightlength;
    real32 v[6] = { origin->x, origin->y - rlen, origin->z,
                    origin->x, origin->y + rlen, origin->z };
    ubyte  red, green, blue;

    hsDesat(&red, &green, &blue, c, 0.70f);
    glColor4ub(red, green, blue, alpha);

    glLineWidth(uplength);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, v);
    glDrawArrays(GL_LINES, 0, 2);
    glDisableClientState(GL_VERTEX_ARRAY);
    glLineWidth(1.0f);
}

/*-----------------------------------------------------------------------------
    Name        : hsUpdate
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsUpdate(Ship* ship)
{
    real32 minimum = 0.0, delta;
    ShipSinglePlayerGameInfo* ssinfo = ship->shipSinglePlayerGameInfo;
    extern real32 HS_CLIPT_NEG_THRESH;
    extern real32 HS_CLIPT_NEG_SCALAR;
    extern real32 HS_CLIPT_NEG_FINISH;
    extern real32 HS_CLIPT_POS_THRESH;
    extern real32 HS_CLIPT_POS_SCALAR;
    extern real32 HS_CLIPT_POS_START;

    if (ssinfo->hsState == HS_INACTIVE ||
        ssinfo->hsState == HS_FINISHED)
    {
        return;
    }

    switch (ssinfo->hsState)
    {
    case HS_POPUP_INTO:
    case HS_POPUP_OUTOF:
        ssinfo->clipt -= 2.5f * ssinfo->cliptDelta;
        minimum = 0.0f;
        break;
    case HS_COLLAPSE_INTO:
    case HS_COLLAPSE_OUTOF:
        ssinfo->clipt -= 2.5f * ssinfo->cliptDelta;
        minimum = 0.0f;
        break;
    case HS_SLICING_INTO:
    case HS_SLICING_OUTOF:
        delta = ssinfo->cliptDelta;
        if (ssinfo->clipt < 0.0f)
        {
            if (ssinfo->clipt > HS_CLIPT_NEG_THRESH)
            {
                delta *= HS_CLIPT_NEG_SCALAR;
            }
            else
            {
                delta *= HS_CLIPT_NEG_FINISH;
            }
        }
        else
        {
            if (ssinfo->clipt < HS_CLIPT_POS_THRESH)
            {
                delta *= HS_CLIPT_POS_SCALAR;
            }
            else
            {
                delta *= HS_CLIPT_POS_START;
            }
        }
        ssinfo->clipt -= delta;
        minimum = -1.0f;
        break;
    }

    if (ssinfo->clipt < minimum)
    {
        if (ssinfo->hsFlags & HSF_WAIT)
        {
            ssinfo->clipt = minimum;
        }
        else
        {
            ssinfo->clipt = 1.0f;
            switch (ssinfo->hsState)
            {
            case HS_POPUP_INTO:
                if(!singlePlayerGame)
                {
                    if(gravwellIsShipStuckForHyperspaceing(ship))
                    {
                        //ship is stuck because of gravwell near by
                        ssinfo->clipt = 0.0f;
                        goto noupdateHSMPOUT;
                    }
                }
                ssinfo->hsState = HS_SLICING_INTO;
noupdateHSMPOUT:
                break;
                case HS_POPUP_OUTOF:
                if (singlePlayerGame && spHoldHyperspaceWindow)
                {
                        //ship is stuck 'cause I want it to
                        ssinfo->clipt = 0.0f;
                        break;  // don't change state
                }
                ssinfo->hsState = HS_SLICING_OUTOF;
                break;
            case HS_SLICING_INTO:
                ssinfo->hsState = HS_COLLAPSE_INTO;
                break;
            case HS_SLICING_OUTOF:
                ssinfo->hsState = HS_COLLAPSE_OUTOF;
                break;
            case HS_COLLAPSE_INTO:
            case HS_COLLAPSE_OUTOF:
                hsFinish(ship);
                break;
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : hsEnd
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsEnd(Ship* ship, bool32 displayEffect)
{
    vector origin = {0.0f, 0.0f, 0.0f};
    StaticCollInfo* sinfo = &ship->staticinfo->common.staticCollInfo;
    real32 t;
    ShipSinglePlayerGameInfo* ssinfo = ship->shipSinglePlayerGameInfo;
    bool32 lightEnabled, hasEffect;
    hmatrix hcoordsys;
    color c;
    hsGateShape gate;

    glDisable(GL_CLIP_PLANE0);

    if (!hsGateState)
    {
        return;
    }

    if (!displayEffect)
    {
        return;
    }

    switch (ssinfo->hsState)
    {
    case HS_POPUP_INTO:
    case HS_POPUP_OUTOF:
    case HS_COLLAPSE_INTO:
    case HS_COLLAPSE_OUTOF:
    case HS_SLICING_INTO:
    case HS_SLICING_OUTOF:
        break;

    default:
        return;
    }

    //setup our coordinate system so the rectangles agree w/ the ETG effect
    glPushMatrix();
    hmatMakeHMatFromMat(&hcoordsys, &ship->rotatable.rotinfo.coordsys);
    if (ssinfo->hyperspaceEffect != NULL)
    {
        hasEffect = TRUE;
    //    hmatPutVectIntoHMatrixCol4(ssinfo->hyperspaceEffect->obj.posinfo.position, hcoordsys);
        hmatPutVectIntoHMatrixCol4(ship->impactable.collInfo.collPosition, hcoordsys);
    }
    else
    {
        hasEffect = FALSE;
        hmatPutVectIntoHMatrixCol4(ship->impactable.collInfo.collPosition, hcoordsys);
    }
    glMultMatrixf((GLfloat*)&hcoordsys);

    rndAdditiveBlends(TRUE);
    glDisable(GL_CLIP_PLANE0);

    rndBackFaceCullEnable(FALSE);
    rndTextureEnable(FALSE);
    lightEnabled = rndLightingEnable(FALSE);

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    if (ship->staticinfo->hyperspaceColor != colBlack)
    {
        c = ship->staticinfo->hyperspaceColor;
    }
    else
    {
        c = hsDefaultColor;
    }

    // CAT: the rectangle's lengths are the collision box's times hyperspaceBoxScale (hsEnd 0x0048fd10); its plane and
    //      the sweep are the gate's (hsGateShapeGet: the assembly's with HWI-FEATURE(hyperspace-assembly), centred on
    //      the rectangle the union needs: gate.cx / gate.cy, 0 for the exe's)
    hsGateShapeGet(ship, &gate, NULL);
    origin.x = gate.cx;                                 // HWI-FEATURE(hyperspace-assembly) (the exe: 0)
    origin.y = gate.cy;                                 // HWI-FEATURE(hyperspace-assembly) (the exe: 0)

    switch (ssinfo->hsState)
    {
    case HS_POPUP_INTO:
    case HS_POPUP_OUTOF:
        //origin.z = hasEffect ? 0.0f : (sinfo->forwardlength);
        //test
        origin.z = gate.front;
        t = 1.0f - ssinfo->clipt;
        //monkey with the effect origin so it scales properly and starts in proper spot
        origin.z *= ship->magnitudeSquared;
        origin.z -= sinfo->collsphereoffset.z;
        hsRectangle(&origin, gate.right, gate.up * t, 90, TRUE, c);
        if (ssinfo->hyperspaceEffect != NULL)
        {
            //monkey effect position
            vector vecTemp;
            matMultiplyMatByVec(&vecTemp, &ship->rotatable.rotinfo.coordsys, &origin);
            ssinfo->hyperspaceEffect->obj.posinfo.position = ship->impactable.collInfo.collPosition;
            vecAddTo(ssinfo->hyperspaceEffect->obj.posinfo.position, vecTemp);
        }

#if SECOND_RECT
        hsLine(&origin, sinfo->rightlength, 1.5f,
               (ubyte)(255.0f * ssinfo->clipt), c);
#endif
        break;

    case HS_COLLAPSE_INTO:
    case HS_COLLAPSE_OUTOF:
        if (singlePlayerGameInfo.hyperspaceFails)
        {
            //origin.z = hasEffect ? 0.0f : (sinfo->forwardlength);
            origin.z = gate.front;
        }
        else
        {
            //origin.z = hasEffect ? 0.0f : -(sinfo->forwardlength);
            origin.z = gate.back;
        }
        t = ssinfo->clipt;
        //monkey with the effect origin so it scales properly and starts in proper spot
        origin.z *= ship->magnitudeSquared;
        origin.z -= sinfo->collsphereoffset.z;
        hsRectangle(&origin, gate.right, gate.up * t, 90, TRUE, c);
#if SECOND_RECT
        hsLine(&origin, sinfo->rightlength, 1.5f,
               (ubyte)(255.0f * (1.0f - t)), c);
#endif
        break;

    case HS_SLICING_INTO:
    case HS_SLICING_OUTOF:
        //origin.z = hasEffect ? 0.0f : ssinfo->clipt * (sinfo->forwardlength);
        origin.z = hsGatePlaneZ(&gate, ssinfo->clipt);
        //monkey with the effect origin so it scales properly and starts in proper spot
        origin.z *= ship->magnitudeSquared;
        origin.z -= sinfo->collsphereoffset.z;
        hsRectangle(&origin, gate.right, gate.up, 90, TRUE, c);
        break;
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    rndLightingEnable(lightEnabled);
    rndBackFaceCullEnable(TRUE);
    rndAdditiveBlends(FALSE);

    glPopMatrix();
}

/*-----------------------------------------------------------------------------
    Name        : hsFinish
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsFinish(Ship* ship)
{
    ShipSinglePlayerGameInfo* ssinfo = ship->shipSinglePlayerGameInfo;
    ssinfo->clipt = -2.0f;
    ssinfo->cliptDelta = 0.0f;
    ssinfo->hsState = (ssinfo->hsState == HS_COLLAPSE_INTO) ? HS_FINISHED : HS_INACTIVE;
    if (bitTest(ship->header.flags, SOF_CatShipPart) ||                 // HWI-TEST (M3-R2 hyperspace-assembly)
        ((ship->guidance.clampInfo != NULL) && (ship->guidance.clampInfo->clampedObjs != NULL)))
    {
        dbgMessagef("hsfinish: tick=%u ship=%s id=%u hsState=%s", (unsigned)universe.univUpdateCounter,
                    ShipTypeToStr(ship->shiptype), (unsigned)ship->shipID.shipNumber,
                    (ssinfo->hsState == HS_FINISHED) ? "FINISHED (into)" : "INACTIVE (out of)");
    }

    if (ssinfo->hyperspaceEffect != NULL)
    {
        etgEffectDelete(ssinfo->hyperspaceEffect);
        univDeleteEffect(ssinfo->hyperspaceEffect);
        ssinfo->hyperspaceEffect = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : hsStaticReset
    Description : resets (clears) static gate data
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsStaticReset(void)
{
    if (hsStaticData != NULL)
    {
        memFree(hsStaticData);
        hsStaticData = NULL;
        hsStaticNumGates = 0;
    }
}

//whether a label denotes a gate
bool32 hsIsStaticGate(char* label)
{
    return ((tolower(label[0]) == 'g') &&
            (tolower(label[1]) == 'a') &&
            (tolower(label[2]) == 't') &&
            (tolower(label[3]) == 'e')) ? TRUE : FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : hsStaticInit
    Description : iterates thru KAS's AI point list to find static hyperspace gates
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsStaticInit(void)
{
    // CAT (hsStaticInit 0x004901b0): Cataclysm frees the static gate table and zeroes its count, nothing more (no KAS
    //      labelled-vector scan, no aivar: the exe takes no argument). CAT (M2-R8 KAS runtime): nothing in cataclysm.exe
    //      adds a static gate: the table 0x00a43e84 / 0x00a43e80 is written only by LoadHyperspaceGates 0x00490510 (a
    //      save's gates) and cleared by hsStaticInit / hsStaticReset 0x00490180, so a new game has none
    if (hsStaticData != NULL)
    {
        memFree(hsStaticData);
        hsStaticData = NULL;
    }
    hsStaticNumGates = 0;
}

/*-----------------------------------------------------------------------------
    Name        : hsStaticDestroy
    Description : "destroy" a static gate
    Inputs      : point - used to ID which gate
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsStaticDestroy(hvector* point)
{
    sdword i;
    hsStaticGate* pGate;

    for (i = 0, pGate = (hsStaticGate*)hsStaticData; i < hsStaticNumGates; i++, pGate++)
    {
        if (pGate->position.x == point->x &&
            pGate->position.y == point->y &&
            pGate->position.z == point->z)
        {
            pGate->active = FALSE;
            pGate->state = HS_STATIC_COLLAPSING;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : hsStaticGateRender
    Description : render a single static hyperspace gate
    Inputs      : gate - the static hyperspace gate to render
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT (M2-R5 render): compared with hsStaticGateRender 0x00490240: the same state changes, the derelict's coordsys
//      (+0x98) and collision position (+0xE0), and the gate rectangle cat_hsGateRectangle 0x0048f8b0 is this
//      hsRectangle with the colour fixed at (70, 90, 255) / outline (110, 130, 255), half sizes 0.54 (0x0072a678),
//      drawn as a quad and a line loop in immediate mode (the same pixels as the vertex arrays here)
void hsStaticGateRender(hsStaticGate* gate)
{
    bool32 lightEnabled;
    Derelict* derelict;
    hmatrix hmat;
    vector origin = { 0.0f, 0.0f, 0.0f };
    color c = colRGB(70, 90, 255);


    if (catSpFleetArriving)                 // CAT: 0x00a6ad78 (hsStaticGateRender 0x00490243): no gate while the fleet arrives
    {
        return;
    }

    derelict = gate->derelict;
    if (derelict == NULL)
    {
        return;
    }
    if (!univSpaceObjInRenderList((SpaceObj*)derelict))
    {
        return;
    }
    if (derelict != NULL)
    {
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        hmatMakeHMatFromMat(&hmat, &derelict->rotatable.rotinfo.coordsys);
        hmatPutVectIntoHMatrixCol4(derelict->impactable.collInfo.collPosition, hmat);
        glMultMatrixf((GLfloat*)&hmat);
    }

    rndAdditiveBlends(TRUE);
    glDisable(GL_CLIP_PLANE0);

    rndBackFaceCullEnable(FALSE);
    rndTextureEnable(FALSE);
    lightEnabled = rndLightingEnable(FALSE);

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    hsRectangle(&origin, HYPERSPACEGATE_WIDTH, HYPERSPACEGATE_HEIGHT, 90, TRUE, c);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    if (derelict != NULL)
    {
        glPopMatrix();
    }

    rndLightingEnable(lightEnabled);
    rndBackFaceCullEnable(TRUE);
    rndAdditiveBlends(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : hsStaticRender
    Description : render all static hyperspace gates
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsStaticRender(void)
{
    sdword i;
    hsStaticGate* pGate;

    for (i = 0, pGate = (hsStaticGate*)hsStaticData; i < hsStaticNumGates; i++, pGate++)
    {
        if (pGate->active)
        {
            hsStaticGateRender(pGate);
        }
    }
}

void hsDerelictTakesDamage(Derelict *derelict)
{
    sdword i;
    hsStaticGate* pGate;

    for (i = 0, pGate = (hsStaticGate*)hsStaticData; i < hsStaticNumGates; i++, pGate++)
    {
        if (pGate->derelict == derelict)
        {
            aivarValueSet(aivarFind(pGate->label),(sdword)derelict->targetable.health);
            break;
        }
    }
}

void hsDerelictDied(Derelict *derelict)
{
    sdword i;
    hsStaticGate* pGate;

    for (i = 0, pGate = (hsStaticGate*)hsStaticData; i < hsStaticNumGates; i++, pGate++)
    {
        if (pGate->derelict == derelict)
        {
            aivarValueSet(aivarFind(pGate->label),-1);
            pGate->derelict = NULL;
        }
    }
}

Derelict *GetHyperspaceGateFromVector(vector *compare)
{
    sdword i;
    hsStaticGate* pGate;

    for (i = 0, pGate = (hsStaticGate*)hsStaticData; i < hsStaticNumGates; i++, pGate++)
    {
        if (vecAreEqual(*compare,pGate->position))
        {
            return pGate->derelict;
        }
    }

    return NULL;
}

/*-----------------------------------------------------------------------------
    Name        : hsNoGate
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsNoGate(bool32 state)
{
    hsGateState = !state;
}

/*=============================================================================
    Save Game stuff
=============================================================================*/

void hsPreFixStaticData(ubyte *data)
{
    sdword i;
    hsStaticGate* pGate;

    for (i = 0, pGate = (hsStaticGate*)data; i < hsStaticNumGates; i++, pGate++)
    {
        pGate->derelict = (DerelictPtr) SpaceObjRegistryGetID((SpaceObj *)pGate->derelict);
    }
}

void hsFixStaticData(hsStaticGate *data)
{
    sdword i;
    hsStaticGate* pGate;

    for (i = 0, pGate = data; i < hsStaticNumGates; i++, pGate++)
    {
        pGate->derelict = (Derelict *)SpaceObjRegistryGetObj((sdword)pGate->derelict);
    }
}

/*-----------------------------------------------------------------------------
    Name        : hsSetStaticData
    Description : set static hyperspace gate data, for loading
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void hsSetStaticData(sdword size, ubyte* data)
{
    hsStaticReset();
    if (size == 0)
    {
        return;
    }
    hsStaticData = (hsStaticGate*)memAlloc(size, "hs static", NonVolatile);
    memcpy(hsStaticData, data, size);
    hsStaticNumGates = size / sizeof(hsStaticGate);
}

void SaveHyperspaceGates(void)
{
    sdword size;
    ubyte* data;

    SaveInfoNumber(hsGateState);

    size = hsStaticNumGates * sizeof(hsStaticGate);
    if (size == 0)
    {
        SaveInfoNumber(0);
        return;
    }

    data = memAlloc(size, "temp hs static", Pyrophoric);
    memcpy(data, hsStaticData, size);

    hsPreFixStaticData(data);
    if (saveCat32Saving)
    {
        // CAT (M2-R9 recorder-leftovers): the exe's 0x30-byte ILP32 gates (SaveHyperspaceGates 0x00490470 saves
        //      count * 0x30 and the block [audit: the function is at 0x00490470, not 0x004904a0]; LoadHyperspaceGates 0x00490510 reads size / 0x30 of them)
        SaveChunk *chunk = CreateChunk(BASIC_STRUCTURE, size, data);
        SaveInfoNumber(hsStaticNumGates * 0x30);
        saveCat32SaveArray(chunk, NULL, 0, 0, SAVECAT32_LAYOUT(hsStaticGate), hsStaticNumGates);
        memFree(chunk);
    }
    else
    {
        SaveInfoNumber(size);
        SaveStructureOfSize(data, size);
    }

    memFree(data);
}

void LoadHyperspaceGates(void)
{
    ubyte* data;
    sdword size;

    hsGateState = LoadInfoNumber();

    size = LoadInfoNumber();

    if (size == 0)
    {
        hsStaticReset();
        return;
    }

    if (saveCat32Loading)
    {
        // CAT (M2-R8 singleplayer-tutorial): the exe's gates are 0x30-byte ILP32 records (LoadHyperspaceGates
        //      0x00490510: size / 0x30 of them); converted, they are this tree's
        SaveChunk *chunk = LoadNextChunk();
        sdword count = size / 0x30;

        chunk = saveCat32ChunkArray(chunk, NULL, 0, 0, SAVECAT32_LAYOUT(hsStaticGate), count);
        VerifyChunk(chunk, BASIC_STRUCTURE, count * (sdword)sizeof(hsStaticGate));
        size = count * (sdword)sizeof(hsStaticGate);
        data = memAlloc(size, "loadedstructure", 0);
        memcpy(data, chunkContents(chunk), size);
        memFree(chunk);
    }
    else
    {
        data = LoadStructureOfSize(size);
    }
    hsSetStaticData(size, data);
    hsFixStaticData(hsStaticData);
    memFree(data);
}

