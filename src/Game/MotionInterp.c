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
              compression included; M3-R1 motion-jitter: above 1 in a frame
              whose pass runs the next tick later, the last tick's motion
              continued, MI_ALPHA_MAX); miRenderEnd writes the simulation's values
              back bit for bit. So the main view, the camera's focus centre
              (ccControl runs inside the draw), the selection circles and the
              picking they feed, and the HUD overlays all use the drawn
              positions. Objects created this tick (no entry of the latest
              capture), objects whose memory was freed and reused (memFree
              forgets the entry) and jumps of more than MI_TELEPORT units in one
              tick are drawn at the current tick. Alpha 1 draws the simulation's
              values unchanged. M3-R3 pause-freeze: a paused universe (the
              console, the Escape menu, the Pause key: miUniversePaused) holds
              the last frame's alpha, so every object stands where it was drawn,
              until the first capture after the pause, which starts the next
              tick's blend from that pose (HWI-FEATURE(pause-freeze)).

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
#include "RepairDroid.h"                            // M2-R7 r6-leftovers: CatRepairDroidArray
#include "SpaceObj.h"
#include "Task.h"
#include "Trails.h"
#include "Universe.h"
#include "UnivUpdate.h"                             // HWI-FEATURE(pause-freeze): catUnivFrameStep (the Pause key)
#include "Vector.h"

extern real32 nisPlayFactor;                        // NIS.c (the debug playback speed; applied where nisUpdate does)

/*=============================================================================
    Data:
=============================================================================*/

bool32 objInterpolate = TRUE;
bool32 nisInterpolate = TRUE;
// M2-R7 r6-leftovers: the switches' run-only state (miSwitchOff)
static bool32 miObjSwitched = FALSE;                // /noInterp turned objInterpolate off for this run
static bool32 miNisSwitched = FALSE;                // /noInterp or /noNISInterp turned nisInterpolate off
static bool32 miObjFileValue = TRUE;                // the options file's values under the switches
static bool32 miNisFileValue = TRUE;
static bool32 miObjRunValue = TRUE;                 // the run's values while utyOptionsFileWrite writes
static bool32 miNisRunValue = TRUE;
static bool32 miSavedObjSwitched = FALSE;           // the switch state opOptionsSaveSettings keeps for Cancel
static bool32 miSavedNisSwitched = FALSE;
taskhandle universeTaskHandle = -1;

#define MI_TELEPORT         2000.0f                 // more than this in one tick is a jump (hyperspace, placement)
// M3-R1 motion-jitter: the render task runs before the universe task in each pass of the task manager (utility.c starts
//      it first), so in the frame whose pass will run the next universe tick alpha is above 1 (the time past the
//      current tick); on the wall clock that is one frame in every tick (by up to a frame's task ticks: 1.13 at 144 Hz).
//      Drawing those frames at the current tick held the motion for that frame and doubled the next frame's step, a
//      16 Hz hitch of every interpolated object (the scene measurement: docs/m3-r1-motion-jitter.md). The frame is now
//      drawn at its own time: the last tick's motion continued (gameplay; the NIS curves at the render time) up to this
//      many ticks past the current one. Exactly 1 (the virtual clock's frames) still draws the simulation's values
#define MI_ALPHA_MAX        2.0f

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
    ubyte clamped;                                  // M3-R1 motion-jitter: clamped to a host at the capture
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
static udword miStatBlended = 0, miStatFresh = 0, miStatTeleport = 0;   // M3-R1 motion-jitter: the frame's counts
static Camera miNisCamera;
static bool32 miNisCameraValid = FALSE;

static void miDroids(bool32 render, real32 alpha); // M2-R7 r6-leftovers: the repair droids (capture and frame)
static void miLerp(vector *out, vector *a, vector *b, real32 t);                 // HWI-FEATURE(pause-freeze): the carry
static bool32 miBlendCoordsys(matrix *out, matrix *a, matrix *b, real32 t);     // HWI-FEATURE(pause-freeze)

// HWI-FEATURE(pause-freeze) (M3-R3): while the universe is paused every object is drawn at the pose the last frame
//      before the pause drew (the lerp's alpha held, the capture unchanged since no tick runs), until the first capture
//      after the pause, which starts the next tick's blend from that pose (docs/m3-r3-pause-freeze.md)
static bool32 miFrozen = FALSE;                     // HWI-FEATURE(pause-freeze): the gameplay lerp is held
static real32 miFrozenAlpha = 1.0f;                 // HWI-FEATURE(pause-freeze): its alpha (1: the simulation's values)
static udword miFrozenGen = 0;                      // HWI-FEATURE(pause-freeze): the capture the held alpha belongs to
static real32 miLastAlpha = 1.0f;                   // HWI-FEATURE(pause-freeze): the last frame's alpha (1: none) ..
static udword miLastGen = 0;                        // HWI-FEATURE(pause-freeze): .. and its capture
static udword miCarryGen = 0;                       // HWI-FEATURE(pause-freeze): the capture the droids carry from (0: none)
#if NIS_TIME_CONTROLS
static bool32 miNisFrozen = FALSE;                  // HWI-FEATURE(pause-freeze): the NIS's render time is held (nisPaused)
static real32 miNisFrozenTime = 0.0f;               // HWI-FEATURE(pause-freeze): that render time (NIS time)
static real32 miNisLastTime = -1.0f;                // HWI-FEATURE(pause-freeze): the last frame's NIS render time (-1: none)
static nisplaying *miNisLast = NULL;                // HWI-FEATURE(pause-freeze): .. and its NIS
#endif

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

/*-----------------------------------------------------------------------------
    HWI-FEATURE(pause-freeze) (M3-R3): the universe does not update this frame: universePause (0x00a6c5b4: the console,
    the Escape menu, the game-stats screen; universeUpdateTask 0x0053e460 skips univUpdate) or the Pause key's flag
    (0x00a6ed74, catKbPause 0x00550640 toggles it; univUpdate 0x0054c3f0 returns at once while it is set and no frame
    step is left)
-----------------------------------------------------------------------------*/
bool32 miUniversePaused(void)                       // HWI-FEATURE(pause-freeze)
{                                                   // HWI-FEATURE(pause-freeze)
    return universePause || (catUnivFrameStep && catUnivFrameStepsLeft < 1);   // HWI-FEATURE(pause-freeze)
}                                                   // HWI-FEATURE(pause-freeze)

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

// HWI-FEATURE(pause-freeze): the first capture after a pause: an object the held frames blended starts the next tick
//      from the pose they drew (the held alpha between the frozen capture and the simulation's values, which no tick
//      changed), so the unpause neither jumps nor replays: the next tick's blend runs from there
static bool32 miCarryFrozen(MiEntry *e, SpaceObj *obj, udword frozenGen)
{
    vector pos, coll, engine, d;
    matrix coord;
    ubyte has;

    if (e->gen != frozenGen || e->nisFrame == miFrame)
    {
        return FALSE;                               // drawn at the simulation's values (new, or the NIS's)
    }
    vecSub(d, obj->posinfo.position, e->pos);
    if (vecMagnitudeSquared(d) > MI_TELEPORT * MI_TELEPORT)
    {
        return FALSE;                               // a jump: drawn at the simulation's values
    }
    has = miRead(obj, &pos, &coord, &coll, &engine) & e->has;
    miLerp(&e->pos, &e->pos, &pos, miFrozenAlpha);
    if (!(has & MI_HAS_ROT) || !miBlendCoordsys(&e->coord, &e->coord, &coord, miFrozenAlpha))
    {
        e->coord = coord;
    }
    if (has & MI_HAS_COLL)
    {
        miLerp(&e->coll, &e->coll, &coll, miFrozenAlpha);
    }
    if (has & MI_HAS_ENGINE)
    {
        miLerp(&e->engine, &e->engine, &engine, miFrozenAlpha);
    }
    return TRUE;
}

void miCaptureTick(void)
{
    Node *node;
    SpaceObj *obj;
    MiEntry *e;
    udword frozenGen = miGen;                       // HWI-FEATURE(pause-freeze): the capture the held frames blended
    bool32 carry = miFrozen && miFrozenGen == miGen && miFrozenAlpha != 1.0f;   // HWI-FEATURE(pause-freeze)

    // [audit: the generation now advances before the flags test: with both flags off no capture runs, and an entry of
    //      the last capture before they went off kept the latest generation, so the frames after the flags came back on
    //      (checkbox, console) and before the next tick blended from that stale transform]
    miGen++;
    if (miGen == 0)
    {
        miGen = 1;
    }
    miFrozen = FALSE;                               // HWI-FEATURE(pause-freeze): a tick runs: the hold ends here
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
        if (carry && miCarryFrozen(e, obj, frozenGen))   // HWI-FEATURE(pause-freeze): from the held pose
        {                                           // HWI-FEATURE(pause-freeze)
            e->gen = miGen;                         // HWI-FEATURE(pause-freeze)
            e->clamped = (obj->header.objtype == OBJ_ShipType &&   // HWI-FEATURE(pause-freeze)
                          ((SpaceObjRotImpTargGuidance *)obj)->clampInfo != NULL);
            continue;                               // HWI-FEATURE(pause-freeze)
        }                                           // HWI-FEATURE(pause-freeze)
        e->gen = miGen;
        e->has = miRead(obj, &e->pos, &e->coord, &e->coll, &e->engine);
        e->clamped = (obj->header.objtype == OBJ_ShipType &&
                      ((SpaceObjRotImpTargGuidance *)obj)->clampInfo != NULL);   // M3-R1 motion-jitter
    }
    miCarryGen = carry ? frozenGen : 0;             // HWI-FEATURE(pause-freeze): the droids' carry
    miDroids(FALSE, 0.0f);                          // M2-R7 r6-leftovers: the repair droids' owner-space positions
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

// one clamped object at its clamp offset in its host's (render) frame (cat_clampUpdateAttachedObjects' arithmetic)
static void miClampedWrite(SpaceObj *host, SpaceObj *child)
{
    vector pos, coll, engine, tmp;
    matrix coord;

    if (miSave(child) == NULL)
    {
        return;
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

// the objects clamped to an NIS ship follow its render transform (cat_clampUpdateAttachedObjects' arithmetic), or keep
// the tick's when the NIS is not blended this frame
static void miNisClamped(SpaceObj *host, bool32 write)
{
    ClampInfo *info;
    sdword i;
    SpaceObj *child;

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
        if (write)
        {
            miClampedWrite(host, child);
        }
    }
}

// M2-R7 r6-leftovers: the objects clamped to a host the gameplay lerp blended (the Cataclysm motherships' modules, a
// ship docked by clamping) are drawn at their clamp offset in the host's blended frame, not on their own blended
// transforms (which turned a linear position against the host's blended rotation: a sub-unit arc error while the host
// turns); the saved entries from <first> on are the gameplay lerp's of this frame
static void miGameplayClamped(udword first)
{
    udword index, count = miSavedCount;
    ClampInfo *info;
    SpaceObj *host, *child;
    MiEntry *e;
    sdword i;

    for (index = first; index < count; index++)
    {
        host = miSaved[index].obj;
        if (host == NULL || host->header.objtype != OBJ_ShipType ||
            (info = ((SpaceObjRotImpTargGuidance *)host)->clampInfo) == NULL || info->clampedObjs == NULL)
        {
            continue;
        }
        for (i = 0; i < info->clampedObjs->numShips; i++)
        {
            child = (SpaceObj *)info->clampedObjs->ShipPtr[i];
            if (child == NULL || ((SpaceObjRotImpTargGuidance *)child)->clampInfo == NULL ||
                ((SpaceObjRotImpTargGuidance *)child)->clampInfo->host != (SpaceObjRotImpTarg *)host)
            {
                continue;
            }
            e = miFind(child);
            if (e != NULL && e->nisFrame == miFrame)
            {
                continue;                           // the NIS's this frame
            }
            // M3-R1 motion-jitter: a child clamped during the latest tick (a dock's latch: CAT_DOCKSTATE_LINK_IN; an
            //      externally built ship) was elsewhere at the capture: its own blend slides it from there onto the
            //      clamp point over the tick. The host-frame write put it on the clamp point from the tick's first
            //      frame (the scene's docking Worker: a 5.3-unit pop in one frame, then still)
            if (e != NULL && e->gen == miGen && !e->clamped)
            {
                continue;
            }
            miClampedWrite(host, child);            // the child's own blend, if any, is undone by the earlier save
        }
    }
}

// M2-R7 r6-leftovers: the repair droids (CatRepairDroidArray: Ship objects on no list, positions in the owner's object
// space, drawn inside the owner's model matrix by cat_droidRenderArray, so they follow the blended owner already) also
// blend their own owner-space walk between the last two ticks; a droid inside the ship is neither captured nor drawn
static void miDroids(bool32 render, real32 alpha)
{
    Node *node;
    Ship *ship;
    CatRepairDroidArray *array;
    SpaceObj *droid;
    MiEntry *e;
    MiSaved *s;
    vector pos, coll, engine, d;
    matrix coord;
    ubyte has;
    sdword i;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((array = (CatRepairDroidArray *)ship->catRepairDroids) == NULL || array->droids == NULL)
        {
            continue;
        }
        for (i = 0; i < array->numDroids; i++)
        {
            droid = (SpaceObj *)array->droids[i].ship;
            if (droid == NULL || array->droids[i].state == CAT_DROID_Inside)
            {
                continue;
            }
            if (!render)
            {
                if ((e = miInsert(droid)) != NULL)
                {
                    if (miCarryGen == 0 || !miCarryFrozen(e, droid, miCarryGen))   // HWI-FEATURE(pause-freeze)
                    {
                        e->has = miRead(droid, &e->pos, &e->coord, &e->coll, &e->engine);
                    }
                    e->gen = miGen;
                }
                continue;
            }
            e = miFind(droid);
            if (e == NULL || e->gen != miGen)
            {
                continue;                           // launched this tick
            }
            vecSub(d, droid->posinfo.position, e->pos);
            if (vecMagnitudeSquared(d) > MI_TELEPORT * MI_TELEPORT || (s = miSave(droid)) == NULL)
            {
                continue;
            }
            has = s->has & e->has & (MI_HAS_POS | MI_HAS_ROT);
            miLerp(&pos, &e->pos, &s->pos, alpha);
            if ((has & MI_HAS_ROT) && !miBlendCoordsys(&coord, &e->coord, &s->coord, alpha))
            {
                has &= (ubyte)~MI_HAS_ROT;
            }
            miWriteTransform(droid, has, &pos, &coord, &coll, &engine);
        }
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
            if (alpha > MI_ALPHA_MAX)
            {
                alpha = MI_ALPHA_MAX;               // M3-R1 motion-jitter: past the next NIS tick the curves at the render
            }                                       //      time too (bsCurveEvalAtBeforeJump leaves every jump ahead out)
            dt = alpha * UNIVERSE_UPDATE_PERIOD;
#if NIS_TIME_CONTROLS
            dt *= nisPlayFactor;                    // nisUpdate scales its step only where it has the time controls
#endif
            blend = (dt > 0.0f);                    // a camera cut is never blended across: the curves are evaluated
                                                    // with the next tick's jumps left to that tick (NIS.c)
        }
#if NIS_TIME_CONTROLS
        // HWI-FEATURE(pause-freeze) (M3-R3): the NIS's own pause (nisPaused: the NIS time control of a build with
        //      NIS_TIME_CONTROLS, where nisUpdate then advances 1e-7 s a tick) holds the render time the last frame
        //      drew (not the curves' tick time behind it); after the pause the hold lasts until the render time of
        //      the running NIS passes it, so the curves never step back. The camera's fly-in (nisCameraCutTime) pauses
        //      the curves to fly the camera and is not held. A build without the time controls has no NIS pause
        if (nisPaused && nisCameraCutTime == 0.0f && !miNisFrozen)   // HWI-FEATURE(pause-freeze)
        {                                           // HWI-FEATURE(pause-freeze)
            miNisFrozen = TRUE;                     // HWI-FEATURE(pause-freeze)
            miNisFrozenTime = (miNisLast == nis && miNisLastTime >= 0.0f) ? miNisLastTime : nis->timeElapsed;
        }                                           // HWI-FEATURE(pause-freeze)
        if (miNisFrozen)                            // HWI-FEATURE(pause-freeze)
        {                                           // HWI-FEATURE(pause-freeze)
            real32 held = miNisFrozenTime - nis->timeElapsed;   // HWI-FEATURE(pause-freeze)
            if (miNisLast != nis || held <= 0.0f || (!nisPaused && blend && dt >= held))   // HWI-FEATURE(pause-freeze)
            {                                       // HWI-FEATURE(pause-freeze): the running NIS has passed it
                miNisFrozen = FALSE;                // HWI-FEATURE(pause-freeze)
            }                                       // HWI-FEATURE(pause-freeze)
            else                                    // HWI-FEATURE(pause-freeze)
            {                                       // HWI-FEATURE(pause-freeze)
                dt = held;                          // HWI-FEATURE(pause-freeze)
                blend = TRUE;                       // HWI-FEATURE(pause-freeze)
            }                                       // HWI-FEATURE(pause-freeze)
        }                                           // HWI-FEATURE(pause-freeze)
#endif
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
#if NIS_TIME_CONTROLS
    miNisLast = nis;                                // HWI-FEATURE(pause-freeze): this frame's render time
    miNisLastTime = nis->timeElapsed + (blend ? dt : 0.0f);   // HWI-FEATURE(pause-freeze)
#endif
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
    udword first;
    nisplaying *nis = thisNisPlaying;

    miFrame++;
    miSavedCount = 0;
    miAlpha = -1.0f;
    miStatBlended = miStatFresh = miStatTeleport = 0;
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
    if (!(objInterpolate || (nisInterpolate && nis != NULL)))
    {
        miLastAlpha = 1.0f;                         // HWI-FEATURE(pause-freeze): the simulation's values drawn
        miLastGen = miGen;                          // HWI-FEATURE(pause-freeze)
        return;
    }
    // HWI-FEATURE(pause-freeze) (M3-R3): the universe paused (miUniversePaused: the console, the Escape menu, the Pause
    //      key) draws every object where the last frame drew it: that frame's alpha is held (the capture and the
    //      simulation's values do not change while no tick runs); a tick between that frame and the pause (its capture
    //      newer than the frame) holds alpha 0, the tick's start, which the next frame would have drawn. The hold lasts
    //      past the unpause until the next tick's capture (miCaptureTick carries the held pose into it). Before M3-R3
    //      universePause drew the simulation's values and the Pause key's flag was not seen at all: the lerp kept
    //      replaying the last tick's motion in every tick of the pause (the user's "jitter in place")
    if (!miFrozen && miUniversePaused())            // HWI-FEATURE(pause-freeze)
    {                                               // HWI-FEATURE(pause-freeze)
        miFrozen = TRUE;                            // HWI-FEATURE(pause-freeze)
        miFrozenGen = miGen;                        // HWI-FEATURE(pause-freeze)
        miFrozenAlpha = (miLastGen == miGen) ? miLastAlpha : 0.0f;   // HWI-FEATURE(pause-freeze)
    }                                               // HWI-FEATURE(pause-freeze)
    if (miFrozen)                                   // HWI-FEATURE(pause-freeze)
    {                                               // HWI-FEATURE(pause-freeze)
        alpha = miFrozenAlpha;                      // HWI-FEATURE(pause-freeze)
        if (alpha == 1.0f)                          // HWI-FEATURE(pause-freeze)
        {                                           // HWI-FEATURE(pause-freeze)
            return;                                 // HWI-FEATURE(pause-freeze): held at the simulation's values
        }                                           // HWI-FEATURE(pause-freeze)
    }                                               // HWI-FEATURE(pause-freeze)
    else                                            // HWI-FEATURE(pause-freeze)
    {
        alpha = taskPeriodFraction(universeTaskHandle);
        miLastGen = miGen;                          // HWI-FEATURE(pause-freeze)
        miLastAlpha = 1.0f;                         // HWI-FEATURE(pause-freeze)
        if (alpha < 0.0f || alpha == 1.0f)
        {
            return;                                 // the universe task stopped, or at the current tick: the simulation's values
        }
        if (alpha > MI_ALPHA_MAX)
        {
            alpha = MI_ALPHA_MAX;                   // M3-R1 motion-jitter: past the current tick, the last tick's motion
        }                                           //      continued (miLerp and miBlendCoordsys extrapolate above 1)
        miLastAlpha = alpha;                        // HWI-FEATURE(pause-freeze)
    }
    miAlpha = alpha;
    first = miSavedCount;                           // M2-R7 r6-leftovers: the gameplay lerp's entries start here
    for (node = universe.SpaceObjList.head; node != NULL; node = node->next)
    {
        obj = (SpaceObj *)listGetStructOfNode(node);
        e = miFind(obj);
        if (e == NULL || e->gen != miGen || e->nisFrame == miFrame)
        {
            if (e == NULL || e->nisFrame != miFrame)
            {
                miStatFresh++;                      // M3-R1 motion-jitter: the counts
            }
            continue;                               // new this tick, or the NIS's
        }
        vecSub(d, obj->posinfo.position, e->pos);
        if (vecMagnitudeSquared(d) > MI_TELEPORT * MI_TELEPORT)
        {
            miStatTeleport++;                       // M3-R1 motion-jitter
            continue;
        }
        miStatBlended++;                            // M3-R1 motion-jitter
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
    miGameplayClamped(first);                       // M2-R7 r6-leftovers: clamped objects in their host's blended frame
    miDroids(TRUE, alpha);                          // M2-R7 r6-leftovers: the repair droids' own walk
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

bool32 miRenderDrawing(void)                        // HWI-FEATURE(motion-interp) mesh animations (M3-R4 mesh-anim-interp)
{
    return miDrawing;
}

// M3-R1 motion-jitter: the measurement's counts (MotionTest.c scene dumps): objects blended this frame, objects with
// no entry of the latest capture (new this tick, or none), jumps over MI_TELEPORT
void miRenderStats(udword *blended, udword *fresh, udword *teleport)
{
    *blended = miStatBlended;
    *fresh = miStatFresh;
    *teleport = miStatTeleport;
}

// M3-R1 motion-jitter: TRUE while the frame is drawn with this object's transform written by the interpolation
bool32 miRenderWritten(const void *obj)
{
    udword i;

    if (!miDrawing)
    {
        return FALSE;
    }
    for (i = 0; i < miSavedCount; i++)
    {
        if (miSaved[i].obj == obj)
        {
            return TRUE;
        }
    }
    return FALSE;
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
    miObjSwitched = FALSE;                          // M2-R7 r6-leftovers: a click is the user's choice, saved as it is
    miNisSwitched = FALSE;
}

/*-----------------------------------------------------------------------------
    The switches are run-only (M2-R7 r6-leftovers, docs/m2-r6-motion-interp.md "What is left"): /noInterp and
    /noNISInterp turn the flags off for the run and remember the values the options file gave them; while a switch
    stands (until the Smooth motion checkbox is clicked), utyOptionsFileWrite writes those remembered values, so the
    run's own opOptionsSaveSettings calls (an options screen opened, Accept, the end of a game) never persist the
    switch. The console rows change the run's flags only, as the switch does, while it stands. Cancel puts the
    switch state back with the flags (opRestoreSavedSettings).
-----------------------------------------------------------------------------*/
void miSwitchOff(bool32 obj, bool32 nis)
{
    if (obj)
    {
        if (!miObjSwitched)
        {
            miObjFileValue = objInterpolate;
            miObjSwitched = TRUE;
        }
        objInterpolate = FALSE;
    }
    if (nis)
    {
        if (!miNisSwitched)
        {
            miNisFileValue = nisInterpolate;
            miNisSwitched = TRUE;
        }
        nisInterpolate = FALSE;
    }
}

void miOptionsFileValues(bool32 begin)
{
    if (begin)
    {
        miObjRunValue = objInterpolate;
        miNisRunValue = nisInterpolate;
        if (miObjSwitched)
        {
            objInterpolate = miObjFileValue;
        }
        if (miNisSwitched)
        {
            nisInterpolate = miNisFileValue;
        }
    }
    else
    {
        objInterpolate = miObjRunValue;
        nisInterpolate = miNisRunValue;
    }
}

void miSwitchStateSave(void)
{
    miSavedObjSwitched = miObjSwitched;
    miSavedNisSwitched = miNisSwitched;
}

void miSwitchStateRestore(void)
{
    miObjSwitched = miSavedObjSwitched;
    miNisSwitched = miSavedNisSwitched;
}

bool32 miSwitchStands(void)
{
    return (miObjSwitched || miNisSwitched);
}

bool32 miSwitchObjStands(void)                      // HWI-FEATURE(instrumentation): /noInterp stands (the settings line)
{                                                   // HWI-FEATURE(instrumentation)
    return miObjSwitched;                           // HWI-FEATURE(instrumentation)
}                                                   // HWI-FEATURE(instrumentation)

bool32 miSwitchNisStands(void)                      // HWI-FEATURE(instrumentation): /noInterp or /noNISInterp stands
{                                                   // HWI-FEATURE(instrumentation)
    return miNisSwitched;                           // HWI-FEATURE(instrumentation)
}                                                   // HWI-FEATURE(instrumentation)
