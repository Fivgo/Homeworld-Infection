/*=============================================================================
    Name    : MotionInterp.c
    Purpose : HWI-FEATURE(motion-interp): render-time motion interpolation of
              every drawn object and the camera, in gameplay and in NIS
              cutscenes (docs/features/motion-interpolation.md, the spec;
              docs/m2-r6-motion-interp.md, the report). Not in cataclysm.exe.

              The simulation writes every object's transform once per universe
              tick (16 Hz) and the renderer draws it as-is, while the camera is
              recomputed every frame around its focus: the focus looks smooth
              and everything else moves on one frame in nine at 144 Hz.

              Gameplay (spec item 0): miCaptureTick, at the start of every
              universe tick (univUpdate), copies each object's position,
              coordinate system, collision position and engine position into a
              side table keyed by the object (the exe-layout structs, Ship 0x63C
              and the rest, are not touched). miRenderBegin, before the regions
              draw, writes lerp(prev, cur, alpha) into every object captured at
              the latest tick (a normalised blend of the coordinate systems),
              where alpha is the fraction of the current universe tick elapsed
              (Task.c taskPeriodFraction: the scheduler's leftover ticks, time
              compression included); miRenderEnd writes the simulation's values
              back bit for bit. So the main view, the camera's focus centre
              (ccControl runs inside the draw), the selection circles and the
              picking they feed, and the HUD overlays all use the drawn
              positions. Objects created this tick (no entry of the latest
              capture), objects whose memory was freed and reused (memFree
              forgets the entry) and jumps of more than MI_TELEPORT units in one
              tick are drawn at the current tick. A paused universe (or alpha 1)
              draws the simulation's values unchanged.

              NIS (spec items 1-2): while an NIS plays, its curve-driven objects
              and its camera are evaluated at tRender = curve time + alpha_nis *
              UNIVERSE_UPDATE_PERIOD (* nisPlayFactor where nisUpdate applies it),
              alpha_nis from the NIS task's own scheduler state, with
              bsCurveEvalAt (B-Spline.c), which does not advance the curves; the
              objects clamped to an NIS ship follow its render transform; a
              camera cut (or any jump of a curve) is never blended across: it
              still shows at the tick that takes it, and the frames before it
              continue the motion of the key before it
              (bsCurveEvalAtBeforeJump).
              nisUpdate, the events, the cuts and the speech stay at tick
              granularity. Objects without curves take the gameplay path.

              The flags: objInterpolate (gameplay) and nisInterpolate (NIS),
              both on by default; with both off nothing here runs.

    Created 2026-10-18 for Homeworld-Infection (M2-R6 motion-interp). Not Relic
    or Cataclysm code: the whole file is HWI-FEATURE(motion-interp). This tree
    is REULA-licensed, see src/COPYING.REULA.
=============================================================================*/

#include "MotionInterp.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Camera.h"
#include "avi.h"
#include "Clamp.h"
#include "Globals.h"
#include "LinkedList.h"
#include "Matrix.h"
#include "NIS.h"
#include "SpaceObj.h"
#include "Task.h"
#include "Trails.h"
#include "Universe.h"
#include "Vector.h"

extern real32 nisPlayFactor;                        // NIS.c (the debug playback speed; applied where nisUpdate does)

/*=============================================================================
    Data:
=============================================================================*/

bool32 objInterpolate = TRUE;
bool32 nisInterpolate = TRUE;
taskhandle universeTaskHandle = -1;

#define MI_TELEPORT         2000.0f                 // more than this in one tick is a jump (hyperspace, placement)

#define MI_HAS_POS          0x01
#define MI_HAS_ROT          0x02
#define MI_HAS_COLL         0x04
#define MI_HAS_ENGINE       0x08                    // Ship.enginePosition or Missile.enginePosition
#define MI_HAS_TRAIL        0x10                    // a missile trail's newest segment hidden for the frame

// the side table: the transforms of the latest capture, keyed by the object (open addressing, linear probing)
typedef struct
{
    SpaceObj *obj;
    udword gen;                                     // the capture that wrote this entry (0: none)
    udword nisFrame;                                // the frame this object was an NIS object in
    ubyte has;
    vector pos, coll, engine;
    matrix coord;
} MiEntry;

// what the frame changed, to be written back
typedef struct
{
    SpaceObj *obj;                                  // NULL: freed during the frame, nothing to write back
    ubyte has;
    vector pos, coll, engine;
    matrix coord;
    sdword trailHead, trailLength;
} MiSaved;

static MiEntry *miTable = NULL;
static udword miCap = 0, miCount = 0;
static udword miGen = 0;                            // the latest capture
static udword miFrame = 0;
static MiSaved *miSaved = NULL;
static udword miSavedCap = 0, miSavedCount = 0;
static bool32 miDrawing = FALSE;                    // between miRenderBegin and miRenderEnd
static real32 miAlpha = -1.0f;
static Camera miNisCamera;
static bool32 miNisCameraValid = FALSE;

/*=============================================================================
    The side table:
=============================================================================*/

static udword miHash(const void *p)
{
    uint64_t k = (uint64_t)(uintptr_t)p;

    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    return (udword)k;
}

static MiEntry *miFind(const void *obj)
{
    udword i;

    if (miCount == 0)
    {
        return NULL;
    }
    for (i = miHash(obj) & (miCap - 1); miTable[i].obj != NULL; i = (i + 1) & (miCap - 1))
    {
        if (miTable[i].obj == obj)
        {
            return &miTable[i];
        }
    }
    return NULL;
}

static void miGrow(void)
{
    MiEntry *old = miTable;
    udword oldCap = miCap, i, j;

    miCap = (miCap == 0) ? 1024 : miCap * 2;
    miTable = calloc(miCap, sizeof(MiEntry));
    if (miTable == NULL)
    {
        miTable = old;                              // out of memory: keep the old table (interpolation degrades)
        miCap = oldCap;
        return;
    }
    for (i = 0; i < oldCap; i++)
    {
        if (old[i].obj != NULL)
        {
            for (j = miHash(old[i].obj) & (miCap - 1); miTable[j].obj != NULL; j = (j + 1) & (miCap - 1))
                ;
            miTable[j] = old[i];
        }
    }
    free(old);
}

static MiEntry *miInsert(SpaceObj *obj)
{
    MiEntry *e = miFind(obj);
    udword i;

    if (e != NULL)
    {
        return e;
    }
    if ((miCount + 1) * 2 > miCap)
    {
        miGrow();
        if ((miCount + 1) * 2 > miCap && (miCount + 1) >= miCap)
        {
            return NULL;
        }
    }
    for (i = miHash(obj) & (miCap - 1); miTable[i].obj != NULL; i = (i + 1) & (miCap - 1))
        ;
    memset(&miTable[i], 0, sizeof(MiEntry));
    miTable[i].obj = obj;
    miCount++;
    return &miTable[i];
}

// removal with backward shift (no tombstones)
static void miRemove(MiEntry *e)
{
    udword i = (udword)(e - miTable), j, k;

    miTable[i].obj = NULL;
    miCount--;
    for (j = (i + 1) & (miCap - 1); miTable[j].obj != NULL; j = (j + 1) & (miCap - 1))
    {
        k = miHash(miTable[j].obj) & (miCap - 1);
        if ((j > i && (k <= i || k > j)) || (j < i && (k <= i && k > j)))
        {
            miTable[i] = miTable[j];
            miTable[j].obj = NULL;
            i = j;
        }
    }
}

void miForget(void *pointer)
{
    MiEntry *e;
    udword i;

    if (miCount == 0 && miSavedCount == 0)
    {
        return;
    }
    if ((e = miFind(pointer)) != NULL)
    {
        miRemove(e);
    }
    if (miDrawing)
    {
        for (i = 0; i < miSavedCount; i++)
        {
            if (miSaved[i].obj == pointer)
            {
                miSaved[i].obj = NULL;
            }
        }
    }
}

/*=============================================================================
    Capture (start of every universe tick):
=============================================================================*/

static ubyte miRead(SpaceObj *obj, vector *pos, matrix *coord, vector *coll, vector *engine)
{
    ubyte has = MI_HAS_POS;

    *pos = obj->posinfo.position;
    if (bitTest(obj->header.flags, SOF_Rotatable))
    {
        *coord = ((SpaceObjRot *)obj)->rotinfo.coordsys;
        has |= MI_HAS_ROT;
    }
    if (bitTest(obj->header.flags, SOF_Impactable))
    {
        *coll = ((SpaceObjRotImp *)obj)->collInfo.collPosition;
        has |= MI_HAS_COLL;
    }
    if (obj->header.objtype == OBJ_ShipType)
    {
        *engine = ((Ship *)obj)->enginePosition;
        has |= MI_HAS_ENGINE;
    }
    else if (obj->header.objtype == OBJ_MissileType)
    {
        *engine = ((Missile *)obj)->enginePosition;
        has |= MI_HAS_ENGINE;
    }
    return has;
}

void miCaptureTick(void)
{
    Node *node;
    SpaceObj *obj;
    MiEntry *e;

    // [audit: the generation now advances before the flags test: with both flags off no capture runs, and an entry of
    //      the last capture before they went off kept the latest generation, so the frames after the flags came back on
    //      (checkbox, console) and before the next tick blended from that stale transform]
    miGen++;
    if (miGen == 0)
    {
        miGen = 1;
    }
    if (!objInterpolate && !nisInterpolate)
    {
        return;
    }
    for (node = universe.SpaceObjList.head; node != NULL; node = node->next)
    {
        obj = (SpaceObj *)listGetStructOfNode(node);
        if ((e = miInsert(obj)) == NULL)
        {
            continue;
        }
        e->gen = miGen;
        e->has = miRead(obj, &e->pos, &e->coord, &e->coll, &e->engine);
    }
}

/*=============================================================================
    The frame:
=============================================================================*/

static MiSaved *miSave(SpaceObj *obj)
{
    MiSaved *s;

    if (miSavedCount >= miSavedCap)
    {
        udword cap = (miSavedCap == 0) ? 512 : miSavedCap * 2;
        MiSaved *grown = realloc(miSaved, cap * sizeof(MiSaved));
        if (grown == NULL)
        {
            return NULL;
        }
        miSaved = grown;
        miSavedCap = cap;
    }
    s = &miSaved[miSavedCount++];
    s->obj = obj;
    s->has = miRead(obj, &s->pos, &s->coord, &s->coll, &s->engine);
    s->trailHead = s->trailLength = 0;
    return s;
}

static void miWriteTransform(SpaceObj *obj, ubyte has, vector *pos, matrix *coord, vector *coll, vector *engine)
{
    if (has & MI_HAS_POS)
    {
        obj->posinfo.position = *pos;
    }
    if ((has & MI_HAS_ROT) && bitTest(obj->header.flags, SOF_Rotatable))
    {
        ((SpaceObjRot *)obj)->rotinfo.coordsys = *coord;
    }
    if ((has & MI_HAS_COLL) && bitTest(obj->header.flags, SOF_Impactable))
    {
        ((SpaceObjRotImp *)obj)->collInfo.collPosition = *coll;
    }
    if (has & MI_HAS_ENGINE)
    {
        if (obj->header.objtype == OBJ_ShipType)
        {
            ((Ship *)obj)->enginePosition = *engine;
        }
        else if (obj->header.objtype == OBJ_MissileType)
        {
            ((Missile *)obj)->enginePosition = *engine;
        }
    }
}

static void miLerp(vector *out, vector *a, vector *b, real32 t)
{
    out->x = a->x + (b->x - a->x) * t;
    out->y = a->y + (b->y - a->y) * t;
    out->z = a->z + (b->z - a->z) * t;
}

// a normalised blend of two coordinate systems (heading, then right, then up re-orthonormalised); FALSE when they are
// too far apart for a blend (more than 90 degrees of heading in one tick), the caller then keeps the current one
static bool32 miBlendCoordsys(matrix *out, matrix *a, matrix *b, real32 t)
{
    vector ua, ra, ha, ub, rb, hb, u, r, h, tmp;
    real32 d;

    matGetVectFromMatrixCol1(ua, *a);
    matGetVectFromMatrixCol2(ra, *a);
    matGetVectFromMatrixCol3(ha, *a);
    matGetVectFromMatrixCol1(ub, *b);
    matGetVectFromMatrixCol2(rb, *b);
    matGetVectFromMatrixCol3(hb, *b);
    if (vecDotProduct(ha, hb) <= 0.0f || vecDotProduct(ua, ub) <= 0.0f)
    {
        return FALSE;
    }
    miLerp(&h, &ha, &hb, t);
    miLerp(&r, &ra, &rb, t);
    miLerp(&u, &ua, &ub, t);
    if (vecMagnitudeSquared(h) < 1e-6f)
    {
        return FALSE;
    }
    vecNormalize(&h);
    d = vecDotProduct(r, h);
    vecScalarMultiply(tmp, h, d);
    vecSubFrom(r, tmp);
    if (vecMagnitudeSquared(r) < 1e-6f)
    {
        return FALSE;
    }
    vecNormalize(&r);
    d = vecDotProduct(u, h);
    vecScalarMultiply(tmp, h, d);
    vecSubFrom(u, tmp);
    d = vecDotProduct(u, r);
    vecScalarMultiply(tmp, r, d);
    vecSubFrom(u, tmp);
    if (vecMagnitudeSquared(u) < 1e-6f)
    {
        return FALSE;
    }
    vecNormalize(&u);
    *out = *b;
    matPutVectIntoMatrixCol1(u, *out);
    matPutVectIntoMatrixCol2(r, *out);
    matPutVectIntoMatrixCol3(h, *out);
    return TRUE;
}

// an object whose render transform is fixed by the NIS this frame (an NIS-driven object or one clamped to it)
static void miMarkNis(SpaceObj *obj)
{
    MiEntry *e = miInsert(obj);

    if (e != NULL)
    {
        e->nisFrame = miFrame;
    }
}

// the objects clamped to an NIS ship follow its render transform (cat_clampUpdateAttachedObjects' arithmetic), or keep
// the tick's when the NIS is not blended this frame
static void miNisClamped(SpaceObj *host, bool32 write)
{
    ClampInfo *info;
    sdword i;
    SpaceObj *child;
    vector pos, coll, engine, tmp;
    matrix coord;

    if (host->header.objtype != OBJ_ShipType ||
        (info = ((SpaceObjRotImpTargGuidance *)host)->clampInfo) == NULL || info->clampedObjs == NULL)
    {
        return;
    }
    for (i = 0; i < info->clampedObjs->numShips; i++)
    {
        child = (SpaceObj *)info->clampedObjs->ShipPtr[i];
        if (child == NULL || ((SpaceObjRotImpTargGuidance *)child)->clampInfo == NULL)
        {
            continue;
        }
        miMarkNis(child);
        if (!write || miSave(child) == NULL)
        {
            continue;
        }
        matMultiplyMatByVec(&tmp, &((SpaceObjRot *)host)->rotinfo.coordsys,
                            &((SpaceObjRotImpTargGuidance *)child)->clampInfo->clampOffset);
        vecAdd(pos, host->posinfo.position, tmp);
        matMultiplyMatByMat(&coord, &((SpaceObjRot *)host)->rotinfo.coordsys,
                            &((SpaceObjRotImpTargGuidance *)child)->clampInfo->clampCoordsys);
        if (bitTest(child->header.flags, SOF_Impactable))
        {
            matMultiplyMatByVec(&tmp, &coord, &child->staticinfo->staticCollInfo.collsphereoffset);
            vecAdd(coll, pos, tmp);
        }
        if (child->header.objtype == OBJ_ShipType)
        {
            matMultiplyMatByVec(&tmp, &coord, &((Ship *)child)->staticinfo->engineNozzleOffset[0]);
            vecAdd(engine, pos, tmp);
        }
        miWriteTransform(child, MI_HAS_POS | MI_HAS_ROT | MI_HAS_COLL | MI_HAS_ENGINE, &pos, &coord, &coll, &engine);
    }
}

// the NIS part: the curve-driven objects and the camera at tRender
static void miRenderNis(nisplaying *nis)
{
    sdword i;
    objectmotion *path;
    SpaceObj *obj;
    real32 alpha = 0.0f, dt = 0.0f;
    bool32 blend = FALSE;
    vector pos, coll, engine, tmp;
    matrix coord;

    // every object the NIS moves is the NIS's this frame, blended or not (the gameplay lerp would put it on the
    // universe task's phase, which is not the NIS task's)
    for (i = 0; i < nis->nObjects; i++)
    {
        path = &nis->objectsInMotion[i];
        if (path->spaceobj != NULL && path->curve[0] != NULL && !bitTest(path->flags, OMF_CurvesDeleted))
        {
            miMarkNis((SpaceObj *)path->spaceobj);
        }
    }
    if (nisInterpolate && nisCamera != NULL)
    {
        alpha = taskPeriodFraction(nisTaskHandle);
        if (alpha > 0.0f && !aviIsPlaying
#if NIS_TIME_CONTROLS
            && !nisPaused
#endif
            )
        {
            if (alpha > 1.0f)
            {
                alpha = 1.0f;
            }
            dt = alpha * UNIVERSE_UPDATE_PERIOD;
#if NIS_TIME_CONTROLS
            dt *= nisPlayFactor;                    // nisUpdate scales its step only where it has the time controls
#endif
            blend = (dt > 0.0f);                    // a camera cut is never blended across: the curves are evaluated
                                                    // with the next tick's jumps left to that tick (NIS.c)
        }
    }
    for (i = 0; i < nis->nObjects; i++)
    {
        path = &nis->objectsInMotion[i];
        obj = (SpaceObj *)path->spaceobj;
        if (obj == NULL || path->curve[0] == NULL || bitTest(path->flags, OMF_CurvesDeleted))
        {
            continue;
        }
        if (blend && nisRenderObjectAt(nis, path, dt, &pos, &coord) && miSave(obj) != NULL)
        {
            if (bitTest(obj->header.flags, SOF_Impactable))
            {
                matMultiplyMatByVec(&tmp, &coord, &obj->staticinfo->staticCollInfo.collsphereoffset);
                vecAdd(coll, pos, tmp);
            }
            if (obj->header.objtype == OBJ_ShipType)
            {
                matMultiplyMatByVec(&tmp, &coord, &((Ship *)obj)->staticinfo->engineNozzleOffset[0]);
                vecAdd(engine, pos, tmp);
            }
            miWriteTransform(obj, MI_HAS_POS | MI_HAS_ROT | MI_HAS_COLL | MI_HAS_ENGINE, &pos, &coord, &coll, &engine);
            miNisClamped(obj, TRUE);
        }
        else
        {
            miNisClamped(obj, FALSE);
        }
    }
    if (blend)
    {
        miNisCamera = *nisCamera;
        if (nisCameraCutTime != 0.0f)
        {                                           // the fly-in steps UNIVERSE_UPDATE_PERIOD a tick (nisUpdateTask)
            miNisCameraValid = nisRenderCameraFlyAt(alpha * UNIVERSE_UPDATE_PERIOD, &miNisCamera);
        }
        else if (nis->nCameras > 0 && nis->camerasInMotion[0].cam == nisCamera)
        {
            miNisCameraValid = nisRenderCameraAt(nis, &nis->camerasInMotion[0], dt, &miNisCamera);
        }
    }
}

void miRenderBegin(void)
{
    Node *node;
    SpaceObj *obj;
    MiEntry *e;
    MiSaved *s;
    real32 alpha;
    vector pos, coll, engine, d;
    matrix coord;
    ubyte has;
    nisplaying *nis = thisNisPlaying;

    miFrame++;
    miSavedCount = 0;
    miAlpha = -1.0f;
    miNisCameraValid = FALSE;
    miDrawing = TRUE;
    if (!gameIsRunning || (!objInterpolate && !nisInterpolate))
    {
        return;
    }
    if (nis != NULL)
    {
        miRenderNis(nis);
    }
    // the gameplay lerp: every object (objInterpolate), or during an NIS the objects without curves (nisInterpolate,
    // spec item 2)
    if (!(objInterpolate || (nisInterpolate && nis != NULL)) || universePause)
    {
        return;
    }
    alpha = taskPeriodFraction(universeTaskHandle);
    if (alpha < 0.0f || alpha >= 1.0f)
    {
        return;                                     // paused, or at the current tick: the simulation's values
    }
    miAlpha = alpha;
    for (node = universe.SpaceObjList.head; node != NULL; node = node->next)
    {
        obj = (SpaceObj *)listGetStructOfNode(node);
        e = miFind(obj);
        if (e == NULL || e->gen != miGen || e->nisFrame == miFrame)
        {
            continue;                               // new this tick, or the NIS's
        }
        vecSub(d, obj->posinfo.position, e->pos);
        if (vecMagnitudeSquared(d) > MI_TELEPORT * MI_TELEPORT)
        {
            continue;
        }
        if ((s = miSave(obj)) == NULL)
        {
            break;
        }
        has = s->has & e->has;
        miLerp(&pos, &e->pos, &s->pos, alpha);
        if ((has & MI_HAS_ROT) && !miBlendCoordsys(&coord, &e->coord, &s->coord, alpha))
        {
            has &= (ubyte)~MI_HAS_ROT;
        }
        if (has & MI_HAS_COLL)
        {
            miLerp(&coll, &e->coll, &s->coll, alpha);
        }
        if (has & MI_HAS_ENGINE)
        {
            miLerp(&engine, &e->engine, &s->engine, alpha);
        }
        miWriteTransform(obj, has, &pos, &coord, &coll, &engine);
        // a missile's trail gets its newest point at the end of the tick's move (physUpdateObjPosVelMissile): drawn
        // from the blended missile it would stick out ahead of it, so it is hidden for the frame (ships' trails take
        // theirs before the move, univMinorSetupShipForControl, and need nothing)
        if (obj->header.objtype == OBJ_MissileType && ((Missile *)obj)->trail != NULL)
        {
            missiletrail *trail = ((Missile *)obj)->trail;
            sdword n = trail->staticInfo->nSegments, newest, second;
            if (trail->grainCounter == trail->staticInfo->granularity && trail->nLength >= 3 && n > 2)
            {
                s->has |= MI_HAS_TRAIL;
                s->trailHead = trail->iHead;
                s->trailLength = trail->nLength;
                newest = (trail->iHead <= 0) ? n - 1 : trail->iHead - 1;
                second = (newest <= 1) ? n - 1 : newest - 1;    // mistrailDraw's walk
                trail->iHead = (second + 1) % n;
                trail->nLength--;
            }
        }
    }
}

void miRenderEnd(void)
{
    MiSaved *s;
    sdword i;

    for (i = (sdword)miSavedCount - 1; i >= 0; i--)
    {
        s = &miSaved[i];
        if (s->obj == NULL)
        {
            continue;
        }
        miWriteTransform(s->obj, s->has, &s->pos, &s->coord, &s->coll, &s->engine);
        if ((s->has & MI_HAS_TRAIL) && s->obj->header.objtype == OBJ_MissileType && ((Missile *)s->obj)->trail != NULL)
        {
            ((Missile *)s->obj)->trail->iHead = s->trailHead;
            ((Missile *)s->obj)->trail->nLength = s->trailLength;
        }
    }
    miSavedCount = 0;
    miDrawing = FALSE;
}

Camera *miRenderCamera(Camera *camera)
{
    if (miNisCameraValid && camera != NULL && camera == nisCamera)
    {
        return &miNisCamera;
    }
    return camera;
}

real32 miRenderAlpha(void)
{
    return miAlpha;
}

/*=============================================================================
    The toggle:
=============================================================================*/

bool32 miSmoothMotionGet(void)
{
    return (objInterpolate || nisInterpolate);
}

void miSmoothMotionSet(bool32 on)
{
    objInterpolate = on ? TRUE : FALSE;
    nisInterpolate = on ? TRUE : FALSE;
}
