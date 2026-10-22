/*=============================================================================
    Name    : MotionTest.c
    Purpose : HWI-FEATURE(motion-interp) measurement: the motion and nis
              harness modes (docs/features/motion-interpolation.md item 4,
              docs/m2-r6-motion-interp.md).

              /catMotionTest <ticks>: the headless Mission01 run of
              /oracleMission with the virtual clock stepping <fps>/16 render
              frames per universe tick (144 fps: 9). At tick 16 the sensors
              manager the headless start opens is closed and four scripted
              sProcessors appear far from Mission01's ships: A (the camera's
              focus, moving at 300 u/s), B (crossing the view from left to
              right), C (receding along the line of sight near the screen
              centre) and D (at rest in the world, carried across the view by
              the camera's motion). Their positions are written
              after every universe tick (a straight line each, so the 16 Hz
              stepping is the only irregularity); from tick 48 B and C move
              relative to the camera's basis of that tick and D stays where
              that basis put it.
              /catNisMotionTest <nis>[:<from>:<to>]: the same headless start,
              then at tick 16 the install's nis\<nis>.nis with its script plays
              at the origin (as the NIS test path nisLoad + nisStart does); the
              frames between NIS times <from> and <to> are dumped (default
              n06:26:36, the fast pans of n06 at 27.3-28.2 s and 33-35.3 s).

              The dump (/catMotionDump <file>) has one line per rendered frame of
              the window: the frame, the universe tick, the camera pose passed to
              the main view's renderer, and each tracked object's screen-space
              position (pixels), depth and on-screen flag, projected with the
              matrices that frame was drawn with, from the position the object
              had while it was drawn, then a CRC-32 of the frame read back from
              the back buffer before the flip. tools/nis_smoothness.py reads it.

              Nothing here changes the simulation's code paths for a normal run;
              in a motion run the scripted ships and NIS are the test's own.

    Created 2026-10-18 for Homeworld-Infection (M2-R6 motion-interp). Not Relic
    or Cataclysm code: the whole file is HWI-FEATURE(motion-interp). This tree
    is REULA-licensed, see src/COPYING.REULA.
=============================================================================*/

#include "MotionTest.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Camera.h"
#include "CameraCommand.h"
#include "CommandWrap.h"    // M3-R1 motion-jitter: the scene's orders (move, formation, dock)
#include "Debug.h"
#include "Dock.h"           // M3-R1 motion-jitter: CAT_DOCK_SUPPORT
#include "FormationDefs.h"  // M3-R1 motion-jitter: DELTA_FORMATION
#include "glinc.h"
#include <SDL2/SDL.h>
#include "Globals.h"
#include "Console.h"
#include "FEFlow.h"
#include "File.h"
#include "LevelLoad.h"
#include "LOD.h"            // HWI-FEATURE(full-detail): the toggle self-test
#include "MotionInterp.h"
#include "Options.h"
#include "main.h"
#include "mainrgn.h"
#include "FastMath.h"
#include "Matrix.h"
#include "Memory.h"
#include "NIS.h"
#include "OracleDump.h"
#include "render.h"
#include "Sensors.h"
#include "ShipSelect.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "utility.h"
#include "Instrument.h"     // HWI-FEATURE(instrumentation): the toggle self-test's overlay and settings-line checks
#include "Key.h"            // HWI-FEATURE(instrumentation): the overlay key has no virtual-key code

/*=============================================================================
    Data:
=============================================================================*/

#define MT_MODE_NONE        0
#define MT_MODE_MOTION      1
#define MT_MODE_NIS         2
#define MT_MODE_SCENE       3           // M3-R1 motion-jitter: /catMotionScene, rides on another mode's game

#define MT_START_TICK       16          // the scripted start (as the render self-test's)
#define MT_BASIS_TICK       48          // the camera has settled on A: B and C get their paths
#define MT_WINDOW_TICK      64          // the motion dump starts
#define MT_NUM_SHIPS        4
#define MT_MAX_NIS_OBJECTS  256
#define MT_LINE_MAX         (64 + 48 * MT_MAX_NIS_OBJECTS)

static sdword mtMode = MT_MODE_NONE;
static sdword mtTicks = 0;                          // motion: the run's last tick
static udword mtFps = 144;
static char mtDumpName[1024] = "";
static FILE *mtDump = NULL;
static bool32 mtWindow = FALSE;                     // frames are being dumped
static bool32 mtDone = FALSE;
static udword mtFrame = 0;                          // frames dumped
static udword mtSampledFrames = 0;
static bool32 mtSampled = FALSE;                    // this frame's main view was sampled
static char mtLine[MT_LINE_MAX];
static ubyte *mtPixels = NULL;
static sdword mtShotFrame = -1;                     // /catMotionShot <frame>: that frame of the window saved as a PPM
static udword mtCrcTable[256];

// motion
static Ship *mtShip[MT_NUM_SHIPS];
static const char *mtRole[MT_NUM_SHIPS] = { "focus", "crossing", "los", "static" };
static const char *mtName[MT_NUM_SHIPS] = { "A", "B", "C", "D" };
static vector mtA0;                                 // A at the start tick
static vector mtVA = { 300.0f, 0.0f, 0.0f };        // A's velocity (units/s)
static vector mtOffB, mtOffC, mtVB, mtVC;           // B and C relative to A (from the basis tick)
static vector mtD0;
static bool32 mtBasisSet = FALSE;

// nis
static char mtNisName[64] = "n06";
static real32 mtNisFrom = 26.0f, mtNisTo = 36.0f;
static nisheader *mtNisHeader = NULL;
static nisplaying *mtNisPlaying = NULL;
static bool32 mtNisStarted = FALSE;

// M3-R1 motion-jitter: the scene (/catMotionScene <spec>): the user's skirmish recipe and the object kinds
#define MT_SCENE_MAX        16
#define MT_SCENE_START      mtSceneStartTick    // pick the objects, focus, the zenith camera (default tick 16)
#define MT_SCENE_ORDER      (mtSceneStartTick + 32) // the orders (the camera has settled)
static char mtSceneKind[16] = "recon";              // recon: focus A (at rest); follow: focus B (the mover)
static real32 mtSceneDistance = 0.0f;               // the camera's distance (0: the focus zoom ccFocus picks)
static real32 mtSceneMove = 500.0f;                 // B's move straight ahead (units)
static sdword mtSceneTicks = 160;                   // the dump's window after the orders (ticks)
static sdword mtSceneStartTick = 16;                // the scene's first tick
static SpaceObj *mtSceneObj[MT_SCENE_MAX];
static const char *mtSceneName[MT_SCENE_MAX];
static const char *mtSceneRole[MT_SCENE_MAX];
static sdword mtSceneN = 0;
static vector mtSceneB0;                            // B where its move began
static Ship *mtSceneHost = NULL;                    // H's dock host
static DockType mtSceneDockPurpose = CAT_DOCK_SUPPORT;  // H's dock purpose (support, or a Worker's deposit)
static udword mtSceneLastCounter = 0;               // the universe tick of the previous dumped frame
static bool32 mtRealClock = FALSE;                  // /catMotionRealClock <fps>: the wall clock, paced to <fps>
static udword mtRealFps = 144;
static real64 mtRealStamp = 0.0, mtRealLast = 0.0;  // this frame's paced dispatch time, the previous one (ms)
static udword mtRealTicks = 0;                      // this frame's task ticks (utyNFrameTicks)
static real32 mtRealSub = 0.0f;                     // this frame's taskSubTick
static udword mtRealFrames = 0, mtRealLate = 0;     // frames paced, frames that missed their slot

/*=============================================================================
    Command line:
=============================================================================*/

void catMotionTestSet(char *string)
{
    mtMode = MT_MODE_MOTION;
    mtTicks = (sdword)strtol(string, NULL, 10);
    if (mtTicks <= MT_WINDOW_TICK)
    {
        mtTicks = 224;                              // a 160-tick (10 s) window
    }
    oracleSetMission(string);                       // the headless Mission01 run: virtual clock, quit after <ticks>
}

void catNisMotionTestSet(char *string)
{
    char *colon;

    mtMode = MT_MODE_NIS;
    strncpy(mtNisName, string, sizeof(mtNisName) - 1);
    mtNisName[sizeof(mtNisName) - 1] = '\0';
    colon = strchr(mtNisName, ':');
    if (colon != NULL)
    {
        *colon = '\0';
        mtNisFrom = (real32)strtod(colon + 1, &colon);
        if (*colon == ':')
        {
            mtNisTo = (real32)strtod(colon + 1, NULL);
        }
    }
    if (mtNisTo <= mtNisFrom)
    {
        mtNisTo = mtNisFrom + 10.0f;
    }
    oracleSetMission("1000000");                    // the headless start; the test quits by itself
}

void catMotionTestDumpSet(char *string)
{
    strncpy(mtDumpName, string, sizeof(mtDumpName) - 1);
    mtDumpName[sizeof(mtDumpName) - 1] = '\0';
}

void catMotionTestShotSet(char *string)
{
    mtShotFrame = (sdword)strtol(string, NULL, 10);
}

void catMotionTestFpsSet(char *string)
{
    mtFps = (udword)strtol(string, NULL, 10);
    if (mtFps < UNIVERSE_UPDATE_RATE)
    {
        mtFps = UNIVERSE_UPDATE_RATE;
    }
}

// M3-R1 motion-jitter: /catMotionScene <kind>[:<distance>[:<move>[:<ticks>[:<start>]]]]: the measured scene on the game another
//      mode starts (the harness's --scene; skirmish: the user's recipe); kind recon (the camera on Recon A at its zenith,
//      Recon B moves <move> units straight ahead) or follow (the same orders, the camera on B)
void catMotionSceneSet(char *string)
{
    char *p = string, *end;

    mtMode = MT_MODE_SCENE;
    end = strchr(p, ':');
    snprintf(mtSceneKind, sizeof(mtSceneKind), "%.*s", (int)((end != NULL) ? (size_t)(end - p) : strlen(p)), p);
    if (end != NULL)
    {
        mtSceneDistance = (real32)strtod(end + 1, &end);
        if (*end == ':')
        {
            mtSceneMove = (real32)strtod(end + 1, &end);
            if (*end == ':')
            {
                mtSceneTicks = (sdword)strtol(end + 1, &end, 10);
                if (*end == ':')
                {
                    mtSceneStartTick = (sdword)strtol(end + 1, NULL, 10);
                }
            }
        }
    }
    if (mtSceneTicks < 16)
    {
        mtSceneTicks = 160;
    }
    if (mtSceneStartTick < 1)
    {
        mtSceneStartTick = 16;
    }
}

// M3-R1 motion-jitter: /catMotionRealClock <fps>: a motion run on the wall clock instead of the virtual one, each
//      frame's dispatch paced to <fps> (as a monitor's vertical sync paces the swap); the dump gets each frame's
//      dispatch time and task ticks
void catMotionRealClockSet(char *string)
{
    mtRealClock = TRUE;
    mtRealFps = (udword)strtol(string, NULL, 10);
    if (mtRealFps < UNIVERSE_UPDATE_RATE)
    {
        mtRealFps = 144;
    }
}

bool32 catMotionRealClock(void)
{
    return mtRealClock;
}

udword catMotionRealClockFps(void)                  // HWI-TEST (M3-R2 instrumentation): /frameLog's refresh in a harness run
{
    return mtRealClock ? mtRealFps : 0;
}

// before utyTasksDispatch reads the clock: wait for this frame's slot of the 1/<fps> grid (SDL's performance
// counter: the sleep to within a millisecond, then a spin); a frame that missed its slot starts at once and the grid
// restarts from it
void catMotionRealClockPace(void)
{
    static Uint64 next = 0;
    Uint64 freq = SDL_GetPerformanceFrequency(), period = freq / mtRealFps, now = SDL_GetPerformanceCounter();

    if (next == 0)
    {
        next = now;
    }
    next += period;
    if (now >= next)
    {
        mtRealLate++;
        next = now;
    }
    else
    {
        Uint64 wait = next - now;
        if (wait * 1000 / freq > 1)
        {
            SDL_Delay((Uint32)(wait * 1000 / freq - 1));
        }
        while (SDL_GetPerformanceCounter() < next)
            ;
    }
    mtRealLast = mtRealStamp;
    mtRealStamp = (real64)SDL_GetPerformanceCounter() * 1000.0 / (real64)freq;
    mtRealFrames++;
}

// after the clock is read: the frame's task ticks and the sub-tick remainder kept
void catMotionRealClockStamp(udword ticks, real32 sub)
{
    mtRealTicks = ticks;
    mtRealSub = sub;
}

bool32 catMotionTestRequested(void)
{
    return (mtMode != MT_MODE_NONE);
}

udword catMotionTestFramesPerTick(void)
{
    if (mtMode == MT_MODE_NONE)
    {
        return 0;
    }
    return mtFps / UNIVERSE_UPDATE_RATE;            // 144 fps: 9
}

/*=============================================================================
    Helpers:
=============================================================================*/

static void mtCrcInit(void)
{
    udword i, j, c;

    for (i = 0; i < 256; i++)
    {
        c = i;
        for (j = 0; j < 8; j++)
        {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        mtCrcTable[i] = c;
    }
}

static udword mtCrc(const ubyte *data, size_t size)
{
    udword c = 0xFFFFFFFFu;
    size_t i;

    for (i = 0; i < size; i++)
    {
        c = mtCrcTable[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

static void mtOpen(void)
{
    if (mtDump != NULL || mtDumpName[0] == '\0')
    {
        return;
    }
    mtDump = fopen(mtDumpName, "w");
    if (mtDump == NULL)
    {
        dbgMessagef("motiontest: FAIL cannot open dump '%s'", mtDumpName);
        return;
    }
    mtCrcInit();
    fprintf(mtDump, "# motion-interp dump v1 mode=%s fps=%u framesPerTick=%u width=%d height=%d\n",
            (mtMode == MT_MODE_NIS) ? "nis" : "motion", (unsigned)mtFps, (unsigned)catMotionTestFramesPerTick(),
            (int)MAIN_WindowWidth, (int)MAIN_WindowHeight);
}

static void mtClose(void)
{
    if (mtDump != NULL)
    {
        fclose(mtDump);
        mtDump = NULL;
    }
    if (mtPixels != NULL)
    {
        free(mtPixels);
        mtPixels = NULL;
    }
}

static bool32 mtShipAlive(Ship *ship)
{
    Node *node;

    if (ship == NULL)
    {
        return FALSE;
    }
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        if ((Ship *)listGetStructOfNode(node) == ship)
        {
            return !bitTest(ship->header.flags, SOF_Dead);
        }
    }
    return FALSE;
}

// a ship at an exact position with the velocity that leads there; no rotation
static void mtShipPlace(Ship *ship, vector *pos)
{
    vector vel;

    vecSub(vel, *pos, ship->obj.posinfo.position);
    vecMultiplyByScalar(vel, (real32)UNIVERSE_UPDATE_RATE);
    ship->obj.posinfo.position = *pos;
    ship->obj.posinfo.velocity = vel;
    vecZeroVector(ship->rotatable.rotinfo.rotspeed);
    vecZeroVector(ship->rotatable.rotinfo.torque);
    vecZeroVector(ship->obj.posinfo.force);
    if (vecMagnitudeSquared(vel) > 0.0f)
    {
        SET_MOVING_LINEARLY(ship->obj.posinfo.isMoving);
    }
    univUpdateObjRotInfo((SpaceObjRot *)ship);
}

// the screen-space position of a world point with the matrices of the frame just drawn
static void mtProject(vector *pos, char *out, size_t size, const char *name)
{
    hvector world, eye, clip;
    real32 x, y;
    sdword visible;

    world.x = pos->x;
    world.y = pos->y;
    world.z = pos->z;
    world.w = 1.0f;
    hmatMultiplyHMatByHVec(&eye, &rndCameraMatrix, &world);
    hmatMultiplyHMatByHVec(&clip, &rndProjectionMatrix, &eye);
    if (clip.w <= 0.0f)
    {
        snprintf(out, size, " %s=nan,nan,%.3f,0", name, (double)clip.w);
        return;
    }
    x = (clip.x / clip.w + 1.0f) * 0.5f * (real32)MAIN_WindowWidth;
    y = (1.0f - clip.y / clip.w) * 0.5f * (real32)MAIN_WindowHeight;
    visible = (x >= 0.0f && x < (real32)MAIN_WindowWidth && y >= 0.0f && y < (real32)MAIN_WindowHeight);
    snprintf(out, size, " %s=%.4f,%.4f,%.3f,%d", name, (double)x, (double)y, (double)clip.w, (int)visible);
}

static void mtFocus(Ship *ship)
{
    FocusCommand *focus = memAlloc(sizeofSelectCommand(1), "motiontestfocus", 0);

    focus->numShips = 1;
    focus->ShipPtr[0] = ship;
    if (smSensorsActive)
    {
        smSensorsCloseForGood();
    }
    ccCameraTimeoutOverride = TRUE;                 // a focus on a scripted ship stays (no sensors manager)
    ccFocusGeneral(&universe.mainCameraCommand, focus, FALSE);
    memFree(focus);
}

/*=============================================================================
    Motion (gameplay) mode:
=============================================================================*/

// A's position at the current tick: a straight line from mtA0 at mtVA
static vector pos0A(void)
{
    vector pos;
    real32 t = (real32)((sdword)universe.univUpdateCounter - MT_START_TICK) / (real32)UNIVERSE_UPDATE_RATE;

    vecScalarMultiply(pos, mtVA, t);
    vecAddTo(pos, mtA0);
    return pos;
}

static void mtMotionTick(void)
{
    sdword tick = (sdword)universe.univUpdateCounter;
    vector pos, off;
    sdword i;

    if (tick == MT_START_TICK)
    {
        mtA0.x = 12000.0f; mtA0.y = -12000.0f; mtA0.z = 0.0f;
        mtD0.x = mtA0.x + 2400.0f; mtD0.y = mtA0.y + 900.0f; mtD0.z = mtA0.z + 300.0f;
        for (i = 0; i < MT_NUM_SHIPS; i++)
        {
            pos = mtA0;
            pos.z += 1500.0f * (real32)i;          // apart until the basis tick
            if (i == 3)
            {
                pos = mtD0;
            }
            mtShip[i] = univAddShip(sProcessor, RACE_Sect, &pos, universe.curPlayerPtr, 0);
        }
        mtFocus(mtShip[0]);
        dbgMessagef("motiontest: started tick=%d ships=%s#%u %s#%u %s#%u %s#%u fps=%u framesPerTick=%u", (int)tick,
                    mtName[0], (mtShip[0] != NULL) ? (unsigned)mtShip[0]->shipID.shipNumber : 0u,
                    mtName[1], (mtShip[1] != NULL) ? (unsigned)mtShip[1]->shipID.shipNumber : 0u,
                    mtName[2], (mtShip[2] != NULL) ? (unsigned)mtShip[2]->shipID.shipNumber : 0u,
                    mtName[3], (mtShip[3] != NULL) ? (unsigned)mtShip[3]->shipID.shipNumber : 0u,
                    (unsigned)mtFps, (unsigned)catMotionTestFramesPerTick());
    }
    if (tick < MT_START_TICK || mtDone)
    {
        return;
    }
    for (i = 0; i < MT_NUM_SHIPS; i++)
    {
        if (!mtShipAlive(mtShip[i]))
        {
            dbgMessagef("motiontest: FAIL ship %s gone at tick %d", mtName[i], (int)tick);
            mtShip[i] = NULL;
        }
    }
    if (tick == MT_BASIS_TICK && mrCamera != NULL)
    {
        vector dir, right, up;
        real32 dist, halfH, halfW;

        vecSub(dir, mrCamera->lookatpoint, mrCamera->eyeposition);
        dist = fsqrt(vecMagnitudeSquared(dir));
        vecNormalize(&dir);
        vecCrossProduct(right, dir, mrCamera->upvector);
        vecNormalize(&right);
        vecCrossProduct(up, right, dir);
        vecNormalize(&up);
        halfH = dist * (real32)tan((double)mrCamera->fieldofview * PI / 360.0);
        halfW = halfH * rndAspectRatio;
        // B: from the left edge to the right edge in 6 s, half a screen above the focus
        vecScalarMultiply(mtOffB, right, -0.9f * halfW);
        vecScalarMultiply(off, up, 0.5f * halfH);
        vecAddTo(mtOffB, off);
        vecScalarMultiply(mtVB, right, 1.8f * halfW / 6.0f);
        // C: below and to the right of the focus, 0.4 of the distance nearer, receding at 0.08 of it per second
        vecScalarMultiply(mtOffC, right, 0.3f * halfW);
        vecScalarMultiply(off, up, -0.5f * halfH);
        vecAddTo(mtOffC, off);
        vecScalarMultiply(off, dir, -0.4f * dist);
        vecAddTo(mtOffC, off);
        vecScalarMultiply(mtVC, dir, 0.08f * dist);
        // D: at rest in the world from now on, 3.5 distances out and to the left; the camera's motion with A carries
        //    it across the view (an asteroid passing by)
        vecScalarMultiply(off, dir, 2.5f * dist);
        mtD0 = pos0A();
        vecAddTo(mtD0, off);
        vecScalarMultiply(off, right, -2.5f * halfW);
        vecAddTo(mtD0, off);
        vecScalarMultiply(off, up, -0.3f * halfH);
        vecAddTo(mtD0, off);
        mtBasisSet = TRUE;
        dbgMessagef("motiontest: basis tick=%d eye=(%.0f %.0f %.0f) lookat=(%.0f %.0f %.0f) distance=%.0f fov=%.1f halfW=%.0f halfH=%.0f",
                    (int)tick, (double)mrCamera->eyeposition.x, (double)mrCamera->eyeposition.y,
                    (double)mrCamera->eyeposition.z, (double)mrCamera->lookatpoint.x, (double)mrCamera->lookatpoint.y,
                    (double)mrCamera->lookatpoint.z, (double)dist, (double)mrCamera->fieldofview, (double)halfW,
                    (double)halfH);
    }

    // A on its straight line; B and C relative to it; D at rest
    pos = pos0A();
    if (mtShip[0] != NULL)
    {
        mtShipPlace(mtShip[0], &pos);
    }
    for (i = 1; i <= 2; i++)
    {
        vector p = pos;
        if (mtShip[i] == NULL)
        {
            continue;
        }
        if (mtBasisSet)
        {
            real32 tb = (real32)(tick - MT_BASIS_TICK) / (real32)UNIVERSE_UPDATE_RATE;
            vecScalarMultiply(off, (i == 1) ? mtVB : mtVC, tb);
            vecAddTo(off, (i == 1) ? mtOffB : mtOffC);
        }
        else
        {
            off.x = 0.0f; off.y = 0.0f; off.z = 1500.0f * (real32)i;
        }
        vecAddTo(p, off);
        mtShipPlace(mtShip[i], &p);
    }
    if (mtShip[3] != NULL)
    {
        mtShipPlace(mtShip[3], &mtD0);
    }

    if (tick == MT_WINDOW_TICK)
    {
        mtOpen();
        if (mtDump != NULL)
        {
            for (i = 0; i < MT_NUM_SHIPS; i++)
            {
                fprintf(mtDump, "# object %s %s %s#%u\n", mtName[i], mtRole[i], "sProcessor",
                        (mtShip[i] != NULL) ? (unsigned)mtShip[i]->shipID.shipNumber : 0u);
            }
        }
        mtWindow = TRUE;
        dbgMessagef("motiontest: window from tick %d to %d", (int)tick, (int)mtTicks);
    }
    if (tick + 1 >= mtTicks && mtWindow)
    {
        mtWindow = FALSE;
        mtDone = TRUE;
        dbgMessagef("motiontest: done tick=%d frames=%u sampled=%u dump=%s result=%s", (int)tick, (unsigned)mtFrame,
                    (unsigned)mtSampledFrames, mtDumpName,
                    (mtSampledFrames > 0 && mtShip[0] != NULL && mtShip[1] != NULL && mtShip[2] != NULL &&
                     mtShip[3] != NULL) ? "PASS" : "FAIL");
        mtClose();
    }
}

/*=============================================================================
    NIS mode:
=============================================================================*/

// the statics of the NIS's ships the level did not load (Mission01 loads only its own types), loaded now as
// universeStaticInit would have (as the build self-test's catBuildSelfTestLoadStatic): the test's own action
static void mtNisLoadStatics(nisheader *header)
{
    sdword i, loaded = 0;
    ShipStaticInfo *info;
    spaceobjpath *path;

    for (i = 0; i < header->nObjectPaths; i++)
    {
        path = &header->objectPath[i];
        if (path->race > RACE_Derelict || path->type < 0 || path->type >= TOTAL_NUM_SHIPS)
        {
            continue;                               // not a ship (generic objects, effects)
        }
        info = GetShipStaticInfo(path->type, (ShipRace)path->race);
        if (info == NULL || bitTest(info->common.infoFlags, IF_InfoLoaded))
        {
            continue;
        }
        SetInfoNeededForShipAndRelatedStaticInfo(path->type, (ShipRace)path->race, TRUE);
        InitStatShipInfo(info, path->type, (ShipRace)path->race);
        InitStatShipInfoPost(info);
        bitSet(info->common.infoFlags, IF_InfoLoaded);
        cat_univSetShipMaxHealth(info);
        loaded++;
    }
    dbgMessagef("motiontest: loaded %d ship statics the level excludes", (int)loaded);
}

static void mtNisTick(void)
{
    sdword tick = (sdword)universe.univUpdateCounter;
    char nisFile[96], scriptFile[96];
    vector pos = { 0.0f, 0.0f, 0.0f };
    sdword i;

    if (mtDone)
    {
        return;
    }
    if (tick == MT_START_TICK && !mtNisStarted)
    {
        mtNisStarted = TRUE;
        if (smSensorsActive)
        {
            smSensorsCloseForGood();
        }
        snprintf(nisFile, sizeof(nisFile), "nis\\%s.nis", mtNisName);
        snprintf(scriptFile, sizeof(scriptFile), "nis\\%s.script", mtNisName);
        mtNisHeader = nisLoad(nisFile, scriptFile);
        if (mtNisHeader == NULL)
        {
            dbgMessagef("motiontest: FAIL nisLoad %s", nisFile);
            mtDone = TRUE;
            utyGameQuit(NULL, NULL);
            return;
        }
        mtNisLoadStatics(mtNisHeader);
        thisNisHeader = mtNisHeader;
        thisNisDoneCB = NULL;
        mtNisPlaying = thisNisPlaying = nisStart(mtNisHeader, &pos, (matrix *)&IdentityMatrix);
        nisTaskResume();
        nisTaskPauseOrResume();
        dbgMessagef("motiontest: nis started %s tick=%d length=%.2f objects=%d cameras=%d window=%.2f-%.2f fps=%u framesPerTick=%u",
                    nisFile, (int)tick, (double)mtNisHeader->length, (int)mtNisHeader->nObjectPaths,
                    (int)mtNisHeader->nCameraPaths, (double)mtNisFrom, (double)mtNisTo, (unsigned)mtFps,
                    (unsigned)catMotionTestFramesPerTick());
        return;
    }
    if (!mtNisStarted)
    {
        return;
    }
    if (thisNisPlaying == NULL || thisNisPlaying != mtNisPlaying)
    {
        dbgMessagef("motiontest: done tick=%d frames=%u sampled=%u dump=%s result=FAIL (the NIS ended before the window did)",
                    (int)tick, (unsigned)mtFrame, (unsigned)mtSampledFrames, mtDumpName);
        mtWindow = FALSE;
        mtDone = TRUE;
        mtClose();
        utyGameQuit(NULL, NULL);
        return;
    }
    if (!mtWindow && mtNisPlaying->timeElapsed >= mtNisFrom)
    {
        mtOpen();
        if (mtDump != NULL)
        {
            fprintf(mtDump, "# nis %s window %.3f-%.3f\n", mtNisName, (double)mtNisFrom, (double)mtNisTo);
            for (i = 0; i < mtNisPlaying->nObjects && i < MT_MAX_NIS_OBJECTS; i++)
            {
                SpaceObjRotImp *obj = mtNisPlaying->objectsInMotion[i].spaceobj;
                fprintf(mtDump, "# object O%d nis %s\n", (int)i,
                        (obj != NULL && obj->header.objtype == OBJ_ShipType) ? ShipTypeToStr(((Ship *)obj)->shiptype) :
                        (obj != NULL ? "object" : "none"));
            }
        }
        mtWindow = TRUE;
        dbgMessagef("motiontest: window from tick %d nisTime=%.3f", (int)tick, (double)mtNisPlaying->timeElapsed);
    }
    if (mtWindow && mtNisPlaying->timeElapsed >= mtNisTo)
    {
        mtWindow = FALSE;
        mtDone = TRUE;
        dbgMessagef("motiontest: done tick=%d nisTime=%.3f frames=%u sampled=%u dump=%s result=%s", (int)tick,
                    (double)mtNisPlaying->timeElapsed, (unsigned)mtFrame, (unsigned)mtSampledFrames, mtDumpName,
                    (mtSampledFrames > 0) ? "PASS" : "FAIL");
        mtClose();
        utyGameQuit(NULL, NULL);
    }
}

/*=============================================================================
    M3-R1 motion-jitter: the scene mode (/catMotionScene). The user's recipe
    (cycle/answers/m3-r1-user.md): a skirmish, the camera focused on the Recon
    nearest the mothership (A) and put at its zenith (looking straight down),
    an adjacent Recon (B) ordered straight ahead 500 units. Every order goes
    through the command layer (clWrapMove / clWrapFormation / clWrapDock: no
    input); the camera through ccFocusGeneral and the camera's own snap
    (Camera.catSnapToFocus / catScriptedDistance). Tracked with A and B: C
    (a ship ordered across the view), an asteroid, a derelict (or a no-owner
    ship), a mothership module (clamped), a formation wingman (three ships in
    delta formation, moved), a docking ship (ordered to dock at the
    mothership for support) and the mothership. From MT_SCENE_ORDER - 8 to
    <ticks> after the orders every frame writes a dump line (motion's format;
    with alpha=, raw=, calls=, mi= and per object <name>m= (1: written by the
    interpolation this frame), and with the real clock ms= / tt= / sub=).
=============================================================================*/

static bool32 mtObjAlive(SpaceObj *obj)
{
    Node *node;

    if (obj == NULL)
    {
        return FALSE;
    }
    for (node = universe.SpaceObjList.head; node != NULL; node = node->next)
    {
        if ((SpaceObj *)listGetStructOfNode(node) == obj)
        {
            return !bitTest(obj->header.flags, SOF_Dead);
        }
    }
    return FALSE;
}

static bool32 mtSceneUsed(SpaceObj *obj)
{
    sdword i;

    for (i = 0; i < mtSceneN; i++)
    {
        if (mtSceneObj[i] == obj)
        {
            return TRUE;
        }
    }
    return FALSE;
}

static void mtSceneAdd(SpaceObj *obj, const char *name, const char *role)
{
    if (mtSceneN >= MT_SCENE_MAX)
    {
        return;
    }
    mtSceneObj[mtSceneN] = obj;
    mtSceneName[mtSceneN] = name;
    mtSceneRole[mtSceneN] = role;
    mtSceneN++;
    if (obj == NULL)
    {
        dbgMessagef("motiontest: scene %s (%s) not found", name, role);
    }
}

// a free ship of the player: not dead, not hidden (docked), not clamped, not the mothership, not taken
static bool32 mtSceneFree(Ship *ship, Player *player)
{
    return ship->playerowner == player && !bitTest(ship->header.flags, SOF_Dead) && !bitTest(ship->header.flags, SOF_Hide) &&
           ship->guidance.clampInfo == NULL && ship != player->PlayerMothership && !mtSceneUsed((SpaceObj *)ship);
}

// the nearest object of <list> to <pos> that <type> (OBJ_*; -1 any) and <shiptype> (-1 any) and <player> (NULL any;
// (Player *)-1 no playing player: none, or a player the game does not have, the map's no-owner ships) allow
static SpaceObj *mtSceneNearest(LinkedList *list, vector *pos, sdword objtype, sdword shiptype, Player *player,
                                bool32 freeShip)
{
    Node *node;
    SpaceObj *obj, *best = NULL;
    real32 d, bestd = REALlyBig;
    vector diff;

    for (node = list->head; node != NULL; node = node->next)
    {
        obj = (SpaceObj *)listGetStructOfNode(node);
        if (bitTest(obj->header.flags, SOF_Dead) || mtSceneUsed(obj) || (objtype >= 0 && (sdword)obj->header.objtype != objtype))
        {
            continue;
        }
        if (obj->header.objtype == OBJ_ShipType)
        {
            Ship *ship = (Ship *)obj;
            if ((shiptype >= 0 && (sdword)ship->shiptype != shiptype) ||
                (player == (Player *)-1 && ship->playerowner != NULL &&
                 ship->playerowner->playerIndex < universe.numPlayers) ||
                (player != NULL && player != (Player *)-1 && (freeShip ? !mtSceneFree(ship, player) : ship->playerowner != player)))
            {
                continue;
            }
        }
        vecSub(diff, obj->posinfo.position, *pos);
        d = vecMagnitudeSquared(diff);
        if (d < bestd)
        {
            bestd = d;
            best = obj;
        }
    }
    return best;
}

static void mtSceneStart(void)
{
    Player *player = universe.curPlayerPtr;
    Ship *mothership = (player != NULL) ? player->PlayerMothership : NULL;
    SpaceObj *a, *b, *obj;
    vector pos;
    CameraStackEntry *entry;
    sdword i, type;

    mtSceneN = 0;
    {
        Node *node;
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            Ship *ship = (Ship *)listGetStructOfNode(node);
            dbgMessagef("motiontest: scene ship %s#%u player=%d flags=0x%x clamped=%d at (%.0f %.0f %.0f)",
                        ShipTypeToStr(ship->shiptype), (unsigned)ship->shipID.shipNumber,
                        (ship->playerowner != NULL) ? (int)ship->playerowner->playerIndex : -1, (unsigned)ship->header.flags,
                        (int)(ship->guidance.clampInfo != NULL), (double)ship->obj.posinfo.position.x,
                        (double)ship->obj.posinfo.position.y, (double)ship->obj.posinfo.position.z);
        }
    }
    if (mothership == NULL)
    {
        dbgMessagef("motiontest: scene FAIL no mothership for player %d", (player != NULL) ? (int)player->playerIndex : -1);
        return;
    }
    pos = mothership->obj.posinfo.position;
    a = mtSceneNearest(&universe.ShipList, &pos, OBJ_ShipType, sRecon, player, TRUE);
    mtSceneAdd(a, "A", strcmp(mtSceneKind, "follow") == 0 ? "atrest" : "focus");
    if (a != NULL)
    {
        pos = a->posinfo.position;
    }
    b = mtSceneNearest(&universe.ShipList, &pos, OBJ_ShipType, sRecon, player, TRUE);
    mtSceneAdd(b, "B", strcmp(mtSceneKind, "follow") == 0 ? "focus" : "mover");
    if (a == NULL || b == NULL)
    {
        dbgMessagef("motiontest: scene FAIL two Recons not found");
        mtSceneN = 0;
        return;
    }
    mtSceneAdd(mtSceneNearest(&universe.ShipList, &pos, OBJ_ShipType, -1, player, TRUE), "C", "crossing");
    mtSceneAdd(mtSceneNearest(&universe.ResourceList, &pos, OBJ_AsteroidType, -1, NULL, FALSE), "D", "asteroid");
    obj = mtSceneNearest(&universe.DerelictList, &pos, OBJ_DerelictType, -1, NULL, FALSE);
    if (obj == NULL)
    {
        obj = mtSceneNearest(&universe.ShipList, &pos, OBJ_ShipType, -1, (Player *)-1, FALSE);
    }
    mtSceneAdd(obj, "E", "derelict");
    obj = NULL;
    if (mothership->guidance.clampInfo != NULL && mothership->guidance.clampInfo->clampedObjs != NULL)
    {
        for (i = 0; i < mothership->guidance.clampInfo->clampedObjs->numShips && obj == NULL; i++)
        {
            Ship *child = mothership->guidance.clampInfo->clampedObjs->ShipPtr[i];
            if (child != NULL && child->guidance.clampInfo != NULL &&
                child->guidance.clampInfo->host == (SpaceObjRotImpTarg *)mothership)
            {
                obj = (SpaceObj *)child;
            }
        }
    }
    mtSceneAdd(obj, "F", "module");
    // the docking ship: the nearest pair of a free ship of the player and a host of the player it can dock with for
    // support or, a Worker, to deposit (ThisShipCanDockWith 0x00464290, the exe's test), within 20,000 units
    {
        Node *node, *node2;
        SpaceObj *best = NULL;
        real32 bestd = 20000.0f * 20000.0f;
        vector diff;
        DockType purpose;

        mtSceneHost = NULL;
        for (purpose = CAT_DOCK_SUPPORT; purpose <= CAT_DOCK_DEPOSIT_RESOURCES; purpose += CAT_DOCK_DEPOSIT_RESOURCES)
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            Ship *host = (Ship *)listGetStructOfNode(node);
            if (host->playerowner != player || bitTest(host->header.flags, SOF_Dead) || host->staticinfo->dockStaticInfo == NULL)
            {
                continue;
            }
            for (node2 = universe.ShipList.head; node2 != NULL; node2 = node2->next)
            {
                Ship *ship = (Ship *)listGetStructOfNode(node2);
                if (ship == host || !mtSceneFree(ship, player) || ship->shiptype == sRecon ||
                    (purpose == CAT_DOCK_DEPOSIT_RESOURCES && ship->staticinfo->shipclass != CLASS_Resource) ||
                    !ThisShipCanDockWith(ship, host, purpose, FALSE))
                {
                    continue;
                }
                vecSub(diff, ship->obj.posinfo.position, host->obj.posinfo.position);
                if (vecMagnitudeSquared(diff) < bestd)
                {
                    bestd = vecMagnitudeSquared(diff);
                    best = (SpaceObj *)ship;
                    mtSceneHost = host;
                    mtSceneDockPurpose = purpose;
                }
            }
        }
        mtSceneAdd(best, "H", "docking");
        if (mtSceneHost != NULL)
        {
            dbgMessagef("motiontest: scene H's host %s#%u at %.0f units, purpose %u", ShipTypeToStr(mtSceneHost->shiptype),
                        (unsigned)mtSceneHost->shipID.shipNumber, (double)fsqrt(bestd), (unsigned)mtSceneDockPurpose);
        }
    }
    // the formation: three free fighters or corvettes of one type, nearest each other (the wingman tracked)
    obj = NULL;
    for (type = 0; type < TOTAL_NUM_SHIPS && obj == NULL; type++)
    {
        SpaceObj *f[3];
        f[0] = mtSceneNearest(&universe.ShipList, &pos, OBJ_ShipType, type, player, TRUE);
        if (f[0] == NULL || (((Ship *)f[0])->staticinfo->shipclass != CLASS_Fighter &&
                             ((Ship *)f[0])->staticinfo->shipclass != CLASS_Corvette))
        {
            continue;
        }
        mtSceneObj[mtSceneN] = f[0]; mtSceneN++;            // taken while the others are looked for
        f[1] = mtSceneNearest(&universe.ShipList, &f[0]->posinfo.position, OBJ_ShipType, type, player, TRUE);
        mtSceneObj[mtSceneN] = f[1]; mtSceneN++;
        f[2] = mtSceneNearest(&universe.ShipList, &f[0]->posinfo.position, OBJ_ShipType, type, player, TRUE);
        mtSceneN -= 2;
        if (f[1] != NULL && f[2] != NULL)
        {
            obj = f[1];
            mtSceneAdd(obj, "G", "formation");
            mtSceneAdd(f[0], "g0", "formation-lead");
            mtSceneAdd(f[2], "g2", "formation-wing");
        }
    }
    if (obj == NULL)
    {
        mtSceneAdd(NULL, "G", "formation");
    }
    // a Worker sent to harvest the asteroid nearest it
    mtSceneAdd(mtSceneNearest(&universe.ShipList, &pos, OBJ_ShipType, sWorker, player, TRUE), "W", "harvest");
    mtSceneAdd((SpaceObj *)mothership, "M", "mothership");

    mtFocus((Ship *)(strcmp(mtSceneKind, "follow") == 0 ? b : a));
    entry = currentCameraStackEntry(&universe.mainCameraCommand);
    entry->remembercam.declination = DEG_TO_RAD(-85.0f);     // the eye straight above the focus (+z): the zenith
    universe.mainCameraCommand.actualcamera.declination = DEG_TO_RAD(-85.0f);
    if (mtSceneDistance > 0.0f)
    {
        universe.mainCameraCommand.actualcamera.catScriptedDistance = mtSceneDistance;
    }
    universe.mainCameraCommand.actualcamera.catSnapToFocus = TRUE;
    for (i = 0; i < mtSceneN; i++)
    {
        obj = mtSceneObj[i];
        dbgMessagef("motiontest: scene %s %s %s#%u at (%.0f %.0f %.0f)", mtSceneName[i], mtSceneRole[i],
                    (obj == NULL) ? "none" : (obj->header.objtype == OBJ_ShipType) ? ShipTypeToStr(((Ship *)obj)->shiptype) :
                    (obj->header.objtype == OBJ_AsteroidType) ? "asteroid" : (obj->header.objtype == OBJ_DerelictType) ? "derelict" : "object",
                    (obj != NULL && obj->header.objtype == OBJ_ShipType) ? (unsigned)((Ship *)obj)->shipID.shipNumber : 0u,
                    (obj != NULL) ? (double)obj->posinfo.position.x : 0.0, (obj != NULL) ? (double)obj->posinfo.position.y : 0.0,
                    (obj != NULL) ? (double)obj->posinfo.position.z : 0.0);
    }
    dbgMessagef("motiontest: scene started kind=%s tick=%u objects=%d distance=%.0f move=%.0f fps=%u framesPerTick=%u realClock=%d",
                mtSceneKind, (unsigned)universe.univUpdateCounter, (int)mtSceneN, (double)mtSceneDistance,
                (double)mtSceneMove, (unsigned)mtFps, (unsigned)catMotionTestFramesPerTick(), (int)mtRealClock);
}

static SpaceObj *mtSceneGet(const char *name)
{
    sdword i;

    for (i = 0; i < mtSceneN; i++)
    {
        if (strcmp(mtSceneName[i], name) == 0)
        {
            return mtObjAlive(mtSceneObj[i]) ? mtSceneObj[i] : NULL;
        }
    }
    return NULL;
}

static void mtSceneOrders(void)
{
    SelectCommand *sel = memAlloc(sizeofSelectCommand(3), "motionscenesel", 0);
    SpaceObj *a = mtSceneGet("A"), *b = mtSceneGet("B"), *c = mtSceneGet("C"), *g[3], *h = mtSceneGet("H");
    SpaceObj *w = mtSceneGet("W");
    Ship *mothership = universe.curPlayerPtr->PlayerMothership;
    vector heading, side, dest;

    g[0] = mtSceneGet("g0"); g[1] = mtSceneGet("G"); g[2] = mtSceneGet("g2");
    if (b != NULL)
    {                                                       // B: straight ahead
        matGetVectFromMatrixCol3(heading, ((SpaceObjRot *)b)->rotinfo.coordsys);
        mtSceneB0 = b->posinfo.position;
        vecScalarMultiply(dest, heading, mtSceneMove);
        vecAddTo(dest, b->posinfo.position);
        sel->numShips = 1; sel->ShipPtr[0] = (Ship *)b;
        clWrapMove(&universe.mainCommandLayer, sel, 0, b->posinfo.position, 1, &dest);
        dbgMessagef("motiontest: scene order B move from (%.0f %.0f %.0f) to (%.0f %.0f %.0f)",
                    (double)mtSceneB0.x, (double)mtSceneB0.y, (double)mtSceneB0.z, (double)dest.x, (double)dest.y, (double)dest.z);
    }
    if (c != NULL && a != NULL)
    {                                                       // C: across, 1,500 units through A's side
        vecSet(side, 1.0f, 0.0f, 0.0f);
        vecScalarMultiply(dest, side, 1500.0f);
        vecAddTo(dest, a->posinfo.position);
        sel->numShips = 1; sel->ShipPtr[0] = (Ship *)c;
        clWrapMove(&universe.mainCommandLayer, sel, 0, c->posinfo.position, 1, &dest);
    }
    if (g[0] != NULL && g[1] != NULL && g[2] != NULL)
    {                                                       // the formation: delta, then 2,000 units along -x
        sel->numShips = 3; sel->ShipPtr[0] = (Ship *)g[0]; sel->ShipPtr[1] = (Ship *)g[1]; sel->ShipPtr[2] = (Ship *)g[2];
        clWrapFormation(&universe.mainCommandLayer, sel, DELTA_FORMATION, 0);
        sel->numShips = 3; sel->ShipPtr[0] = (Ship *)g[0]; sel->ShipPtr[1] = (Ship *)g[1]; sel->ShipPtr[2] = (Ship *)g[2];
        dest = g[0]->posinfo.position;
        dest.x -= 2000.0f;
        clWrapMove(&universe.mainCommandLayer, sel, 0, g[0]->posinfo.position, 1, &dest);
    }
    if (h != NULL && mtSceneHost != NULL && mtObjAlive((SpaceObj *)mtSceneHost))
    {                                                       // H: dock at its host for support
        sel->numShips = 1; sel->ShipPtr[0] = (Ship *)h;
        if (mtSceneDockPurpose == CAT_DOCK_DEPOSIT_RESOURCES)
        {
            ((Ship *)h)->resources = 100;                   // a scripted hold to deposit (as the dock self-test's)
        }
        clWrapDock(&universe.mainCommandLayer, sel, mtSceneDockPurpose, mtSceneHost);
        dbgMessagef("motiontest: scene order H dock: command=%s", (((Ship *)h)->command != NULL &&
                    ((Ship *)h)->command->ordertype.order == COMMAND_DOCK) ? "dock" : "none");
    }
    if (w != NULL)
    {                                                       // W: harvest the asteroid nearest it
        Resource *rock = (Resource *)mtSceneNearest(&universe.ResourceList, &w->posinfo.position, OBJ_AsteroidType, -1,
                                                    NULL, FALSE);
        if (rock != NULL)
        {
            sel->numShips = 1; sel->ShipPtr[0] = (Ship *)w;
            clWrapCollectResource(&universe.mainCommandLayer, sel, rock);
        }
    }
    if (mothership != NULL)
    {                                                       // M: 2,000 units ahead (its modules follow, clamped)
        matGetVectFromMatrixCol3(heading, mothership->rotatable.rotinfo.coordsys);
        vecScalarMultiply(dest, heading, 2000.0f);
        vecAddTo(dest, mothership->obj.posinfo.position);
        sel->numShips = 1; sel->ShipPtr[0] = mothership;
        clWrapMove(&universe.mainCommandLayer, sel, 0, mothership->obj.posinfo.position, 1, &dest);
    }
    memFree(sel);
}

static void mtSceneTick(void)
{
    sdword tick = (sdword)universe.univUpdateCounter, i;
    SpaceObj *b;
    vector d;

    if (mtDone)
    {
        return;
    }
    if (tick == MT_SCENE_START)
    {
        mtSceneStart();
    }
    if (tick == MT_SCENE_ORDER - 8 && mtSceneN > 0)
    {
        mtOpen();
        if (mtDump != NULL)
        {
            fprintf(mtDump, "# scene %s distance=%.0f move=%.0f realClock=%d realFps=%u\n", mtSceneKind,
                    (double)mtSceneDistance, (double)mtSceneMove, (int)mtRealClock, (unsigned)mtRealFps);
            for (i = 0; i < mtSceneN; i++)
            {
                SpaceObj *obj = mtSceneObj[i];
                fprintf(mtDump, "# object %s %s %s#%u\n", mtSceneName[i], mtSceneRole[i],
                        (obj == NULL) ? "none" : (obj->header.objtype == OBJ_ShipType) ? ShipTypeToStr(((Ship *)obj)->shiptype) :
                        (obj->header.objtype == OBJ_AsteroidType) ? "asteroid" : (obj->header.objtype == OBJ_DerelictType) ? "derelict" : "object",
                        (obj != NULL && obj->header.objtype == OBJ_ShipType) ? (unsigned)((Ship *)obj)->shipID.shipNumber : 0u);
            }
        }
        mtWindow = TRUE;
        mtSceneLastCounter = universe.univUpdateCounter;
        dbgMessagef("motiontest: window from tick %d to %d", (int)tick, (int)(MT_SCENE_ORDER + mtSceneTicks));
    }
    if (tick == MT_SCENE_ORDER && mtSceneN > 0)
    {
        mtSceneOrders();
    }
    if (tick >= MT_SCENE_ORDER + mtSceneTicks && mtWindow)
    {
        real32 moved = 0.0f;
        b = mtSceneGet("B");
        if (b != NULL)
        {
            vecSub(d, b->posinfo.position, mtSceneB0);
            moved = fsqrt(vecMagnitudeSquared(d));
        }
        mtWindow = FALSE;
        mtDone = TRUE;
        dbgMessagef("motiontest: scene done tick=%d frames=%u sampled=%u Bmoved=%.0f realFrames=%u realLate=%u dump=%s result=%s",
                    tick, (unsigned)mtFrame, (unsigned)mtSampledFrames, (double)moved, (unsigned)mtRealFrames,
                    (unsigned)mtRealLate, mtDumpName,
                    (mtSampledFrames > 0 && moved > 0.5f * mtSceneMove) ? "PASS" : "FAIL");
        mtClose();
    }
}

/*=============================================================================
    Hooks:
=============================================================================*/

// HWI-TEST (M3-R1 letterbox-front): /catNisScissorOut <tick>:<seconds>: in the nis mode, at that universe tick the
//      letterbox starts to fade out as an NIS's ScissorOut event does (nisScissorOut: nisScissorFadeOut, the fade
//      time from 0), so a fade frame with a ship in the bars can be captured; a measurement run only (never a baseline)
static sdword mtScissorOutTick = -1;
static real32 mtScissorOutSeconds = 1.0f;

void catNisScissorOutSet(char *string)
{
    char *colon;

    mtScissorOutTick = (sdword)strtol(string, &colon, 10);
    if (*colon == ':')
    {
        mtScissorOutSeconds = (real32)strtod(colon + 1, NULL);
    }
}

void catMotionTestTick(void)
{
    if (mtMode == MT_MODE_NIS && (sdword)universe.univUpdateCounter == mtScissorOutTick)
    {
        dbgMessagef("motiontest: scissor out tick=%d seconds=%.3f fullyScissored=%d", mtScissorOutTick,
                    (double)mtScissorOutSeconds, (int)nisFullyScissored);
        nisScissorFadeOut = mtScissorOutSeconds;
        nisScissorFadeTime = 0.0f;
    }
    if (mtMode == MT_MODE_MOTION)
    {
        mtMotionTick();
    }
    else if (mtMode == MT_MODE_NIS)
    {
        mtNisTick();
    }
    else if (mtMode == MT_MODE_SCENE)
    {
        mtSceneTick();                              // M3-R1 motion-jitter
    }
}

void catMotionTestFrameSample(struct Camera *camera)
{
    size_t n;
    sdword i;
    char name[16];

    if (!mtWindow || mtDump == NULL || camera == NULL)
    {
        return;
    }
    n = (size_t)snprintf(mtLine, sizeof(mtLine), " eye=%.4f,%.4f,%.4f look=%.4f,%.4f,%.4f up=%.5f,%.5f,%.5f",
                         (double)camera->eyeposition.x, (double)camera->eyeposition.y, (double)camera->eyeposition.z,
                         (double)camera->lookatpoint.x, (double)camera->lookatpoint.y, (double)camera->lookatpoint.z,
                         (double)camera->upvector.x, (double)camera->upvector.y, (double)camera->upvector.z);
    if (mtMode == MT_MODE_MOTION)
    {
        for (i = 0; i < MT_NUM_SHIPS && n < sizeof(mtLine); i++)
        {
            if (mtShip[i] == NULL)
            {
                continue;
            }
            mtProject(&mtShip[i]->obj.posinfo.position, mtLine + n, sizeof(mtLine) - n, mtName[i]);
            n += strlen(mtLine + n);
        }
    }
    else if (mtMode == MT_MODE_SCENE)
    {                                               // M3-R1 motion-jitter: the frame's clock and the scene's objects
        udword blended = 0, fresh = 0, teleport = 0;
        miRenderStats(&blended, &fresh, &teleport);
        n += (size_t)snprintf(mtLine + n, sizeof(mtLine) - n, " alpha=%.5f raw=%.5f calls=%u mi=%u/%u/%u",
                              (double)miRenderAlpha(), (double)taskPeriodFraction(universeTaskHandle),
                              (unsigned)(universe.univUpdateCounter - mtSceneLastCounter), (unsigned)blended,
                              (unsigned)fresh, (unsigned)teleport);
        mtSceneLastCounter = universe.univUpdateCounter;
        if (mtRealClock && n < sizeof(mtLine))
        {
            n += (size_t)snprintf(mtLine + n, sizeof(mtLine) - n, " ms=%.3f ft=%.3f tt=%u sub=%.4f rsub=%.4f", mtRealStamp,
                                  mtRealStamp - mtRealLast, (unsigned)mtRealTicks, (double)mtRealSub,
                                  (double)taskSubTickRender);
        }
        for (i = 0; i < mtSceneN && n < sizeof(mtLine); i++)
        {
            if (!mtObjAlive(mtSceneObj[i]))
            {
                continue;
            }
            mtProject(&mtSceneObj[i]->posinfo.position, mtLine + n, sizeof(mtLine) - n, mtSceneName[i]);
            n += strlen(mtLine + n);
            if (n < sizeof(mtLine))
            {
                vector *wp = &mtSceneObj[i]->posinfo.position;
                n += (size_t)snprintf(mtLine + n, sizeof(mtLine) - n, " %sm=%d %sw=%.3f,%.3f,%.3f", mtSceneName[i],
                                      (int)miRenderWritten(mtSceneObj[i]), mtSceneName[i], (double)wp->x, (double)wp->y,
                                      (double)wp->z);
            }
        }
    }
    else if (mtNisPlaying != NULL && thisNisPlaying == mtNisPlaying)
    {
        if (mtRealClock && n < sizeof(mtLine))
        {                                           // M3-R1 motion-jitter: the real clock's frame (only then: the virtual
            n += (size_t)snprintf(mtLine + n, sizeof(mtLine) - n,   // nis dumps keep their format)
                                  " alpha=%.5f raw=%.5f calls=0 ms=%.3f ft=%.3f tt=%u sub=%.4f rsub=%.4f",
                                  (double)taskPeriodFraction(nisTaskHandle), (double)taskPeriodFraction(nisTaskHandle),
                                  mtRealStamp, mtRealStamp - mtRealLast, (unsigned)mtRealTicks, (double)mtRealSub,
                                  (double)taskSubTickRender);
        }
        for (i = 0; i < mtNisPlaying->nObjects && i < MT_MAX_NIS_OBJECTS && n < sizeof(mtLine); i++)
        {
            objectmotion *path = &mtNisPlaying->objectsInMotion[i];
            if (path->spaceobj == NULL || path->curve[0] == NULL || bitTest(path->flags, OMF_CurvesDeleted) ||
                bitTest(path->spaceobj->header.flags, SOF_Hide))
            {
                continue;
            }
            snprintf(name, sizeof(name), "O%d", (int)i);
            mtProject(&path->spaceobj->obj.posinfo.position, mtLine + n, sizeof(mtLine) - n, name);
            n += strlen(mtLine + n);
        }
    }
    mtSampled = TRUE;
}

void catMotionTestFrameEnd(void)
{
    udword crc = 0;
    size_t size;

    if (!mtWindow || mtDump == NULL)
    {
        return;
    }
    // HWI-FEATURE(ui-scale): the read-back is the physical framebuffer (MAIN_Window* are the logical size; equal at 100%)
    size = (size_t)MAIN_WindowWidthActual * (size_t)MAIN_WindowHeightActual * 4;  // HWI-FEATURE(ui-scale)
    if (mtPixels == NULL)
    {
        mtPixels = malloc(size);
    }
    if (mtPixels != NULL && !mtRealClock)           // M3-R1 motion-jitter: no read-back on the real clock (crc=0)
    {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, MAIN_WindowWidthActual, MAIN_WindowHeightActual, GL_RGBA, GL_UNSIGNED_BYTE, mtPixels);   // HWI-FEATURE(ui-scale)
        crc = mtCrc(mtPixels, size);
        if ((sdword)mtFrame == mtShotFrame)
        {                                           // <dump>.<frame>.ppm, top row first
            char name[1100];
            FILE *f;
            sdword row;

            snprintf(name, sizeof(name), "%s.%u.ppm", mtDumpName, (unsigned)mtFrame);
            if ((f = fopen(name, "wb")) != NULL)
            {
                fprintf(f, "P6\n%d %d\n255\n", (int)MAIN_WindowWidthActual, (int)MAIN_WindowHeightActual);   // HWI-FEATURE(ui-scale)
                for (row = MAIN_WindowHeightActual - 1; row >= 0; row--)   // HWI-FEATURE(ui-scale)
                {
                    ubyte *p = mtPixels + (size_t)row * (size_t)MAIN_WindowWidthActual * 4;   // HWI-FEATURE(ui-scale)
                    sdword x;
                    for (x = 0; x < MAIN_WindowWidthActual; x++, p += 4)   // HWI-FEATURE(ui-scale)
                    {
                        fwrite(p, 1, 3, f);
                    }
                }
                fclose(f);
                dbgMessagef("motiontest: frame %u saved to %s", (unsigned)mtFrame, name);
            }
        }
    }
    fprintf(mtDump, "F %u tick=%u%s crc=%08x\n", (unsigned)mtFrame, (unsigned)universe.univUpdateCounter,
            mtSampled ? mtLine : " nosample", (unsigned)crc);
    if (mtSampled)
    {
        mtSampledFrames++;
    }
    mtSampled = FALSE;
    mtFrame++;
}

/*=============================================================================
    HWI-TEST (M3-R1 letterbox-front): the any-mode frame capture (/catShot).
    Reads the physical framebuffer (SDL_GL_GetDrawableSize, not the UI size)
    back before the flip, after everything of the frame is drawn (the bars,
    the subtitles, the cursor), and writes a binary PPM, top row first. A
    read-back changes no GL state that is drawn with (GL_PACK_ALIGNMENT only)
    and nothing runs when no shot is due.
=============================================================================*/
#define SHOT_MAX_ITEMS      64
#define SHOT_MAX_FILES      4096

typedef struct
{
    udword from, to, step, frame;
}
shotitem;

static shotitem shotItem[SHOT_MAX_ITEMS];
static sdword shotItems = 0;
static char shotPrefix[1024] = "";
static bool32 shotLetterboxOnly = FALSE;
static udword shotFiles = 0;
static udword shotLastTick = 0xffffffff;
static udword shotSegment = 0;                      // the universe tick counter restarted (a level load): 1, 2, ..
static udword shotFrameInTick = 0;
static ubyte *shotPixels = NULL;
static size_t shotPixelsSize = 0;

void catShotSet(char *string)
{
    char *p = string, *end;

    while (*p != '\0' && shotItems < SHOT_MAX_ITEMS)
    {
        shotitem *item = &shotItem[shotItems];

        item->from = item->to = (udword)strtoul(p, &end, 10);
        item->step = 1;
        item->frame = 0;
        if (end == p)
        {
            break;
        }
        p = end;
        if (*p == '-')
        {
            item->to = (udword)strtoul(p + 1, &end, 10);
            p = end;
            if (*p == '/')
            {
                item->step = (udword)strtoul(p + 1, &end, 10);
                p = end;
                if (item->step == 0)
                {
                    item->step = 1;
                }
            }
        }
        if (*p == ':')
        {
            item->frame = (udword)strtoul(p + 1, &end, 10);
            p = end;
        }
        shotItems++;
        while (*p == ',' || *p == ' ')
        {
            p++;
        }
    }
}

void catShotFileSet(char *string)
{
    strncpy(shotPrefix, string, sizeof(shotPrefix) - 1);
    shotPrefix[sizeof(shotPrefix) - 1] = '\0';
}

void catShotLetterboxSet(void)
{
    shotLetterboxOnly = TRUE;
}

static bool32 shotDue(udword tick, udword frame)
{
    sdword i;

    for (i = 0; i < shotItems; i++)
    {
        if (tick >= shotItem[i].from && tick <= shotItem[i].to && (tick - shotItem[i].from) % shotItem[i].step == 0 &&
            frame == shotItem[i].frame)
        {
            return TRUE;
        }
    }
    return FALSE;
}

void catShotFrameEnd(void)
{
    udword tick = universe.univUpdateCounter;
    int w = 0, h = 0;
    size_t size;
    char name[1200], state[48];
    FILE *f;
    sdword row;
    extern SDL_Window *sdlwindow;

    if (shotItems == 0)
    {
        return;
    }
    if (tick != shotLastTick)
    {
        if (shotLastTick != 0xffffffff && tick < shotLastTick)
        {
            shotSegment++;                          // a new level: its ticks count from 0 again
        }
        shotLastTick = tick;
        shotFrameInTick = 0;
    }
    else
    {
        shotFrameInTick++;
    }
    if (!shotDue(tick, shotFrameInTick) || shotFiles >= SHOT_MAX_FILES)
    {
        return;
    }
    if (nisFullyScissored)
    {
        snprintf(state, sizeof(state), "full");
    }
    else if (nisScissorFade != 0.0f)
    {
        snprintf(state, sizeof(state), "fade=%.4f", (double)nisScissorFade);
    }
    else
    {
        snprintf(state, sizeof(state), "none");
    }
    if (shotLetterboxOnly && !nisFullyScissored && nisScissorFade == 0.0f)
    {
        return;
    }
    if (sdlwindow != NULL)
    {
        SDL_GL_GetDrawableSize(sdlwindow, &w, &h);
    }
    if (w <= 0 || h <= 0)
    {
        w = MAIN_WindowWidth;
        h = MAIN_WindowHeight;
    }
    size = (size_t)w * (size_t)h * 3;
    if (shotPixelsSize < size)
    {
        free(shotPixels);
        shotPixels = malloc(size);
        shotPixelsSize = (shotPixels != NULL) ? size : 0;
    }
    if (shotPixels == NULL)
    {
        return;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, shotPixels);
    snprintf(name, sizeof(name), "%s", (shotPrefix[0] != '\0') ? shotPrefix : filePathPrepend("shot", FF_UserSettingsPath));
    if (shotSegment > 0)
    {                                               // after a level load: <prefix>.s<segment>.<tick>.<frame>.ppm
        snprintf(name + strlen(name), sizeof(name) - strlen(name), ".s%u", (unsigned)shotSegment);
    }
    snprintf(name + strlen(name), sizeof(name) - strlen(name), ".%u.%u.ppm", (unsigned)tick, (unsigned)shotFrameInTick);
    if ((f = fopen(name, "wb")) == NULL)
    {
        dbgMessagef("catshot: FAIL cannot write %s", name);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (row = h - 1; row >= 0; row--)
    {
        fwrite(shotPixels + (size_t)row * (size_t)w * 3, 1, (size_t)w * 3, f);
    }
    fclose(f);
    shotFiles++;
    dbgMessagef("catshot: tick=%u frame=%u segment=%u size=%dx%d ui=%dx%d letterbox=%s letterboxBehind=%d file=%s", (unsigned)tick,
                (unsigned)shotFrameInTick, (unsigned)shotSegment, w, h, (int)MAIN_WindowWidth, (int)MAIN_WindowHeight, state,
                (int)letterboxBehind, name);
}

/*=============================================================================
    The toggle self-test (/catToggleTest, harness mode toggle): after the
    start-up, in the front end, no input. The Smooth motion checkbox on the
    front end's Video_Options screen (its first call shows the flags; a click
    is its callback with the toggled state, as the FE's checkbox handler calls
    it), the Cancel path (opRestoreSavedSettings, the first step of
    opOptionsCancelHelper) and the Accept path (opOptionsSaveSettings, the last
    step of opOptionsAcceptHelper, which writes Cataclysm.cfg), the written rows
    read back through utyOptionsList, the in-game Video screen's copy of the
    control, and the two console rows. "toggletest:" log lines, "toggletest:
    done ... result=PASS|FAIL"; the flags are put back as the start-up left
    them (the options file and the switches /noInterp, /noNISInterp).
    M2-R7 r6-leftovers: with a switch (-- /noInterp or -- /noNISInterp) the
    switch is checked run-only first: the start-up's opOptionsSaveSettings
    writes the options file's values (not the switch's) while the run's flags
    stay off, Cancel brings the override back after a click, and at the end
    the override is set up again as the start-up left it.
=============================================================================*/
bool32 catToggleSelfTest = FALSE;

static sdword mtToggleFails = 0;

static void mtToggleCheck(bool32 ok, const char *what)
{
    if (!ok)
    {
        mtToggleFails++;
        dbgMessagef("toggletest: FAIL %s (objInterpolate=%d nisInterpolate=%d)", what, (int)objInterpolate,
                    (int)nisInterpolate);
    }
}

static featom *mtToggleAtom(char *screenName, featom **label)
{
    fescreen *screen = feScreenFind(screenName);
    featom *box = NULL;
    sdword i;

    *label = NULL;
    if (screen == NULL)
    {
        return NULL;
    }
    for (i = 0; i < screen->nAtoms; i++)
    {
        if (screen->atoms[i].name != NULL && strcmp(screen->atoms[i].name, "OP_SmoothMotion") == 0)
        {
            box = &screen->atoms[i];
        }
        if (screen->atoms[i].type == FA_StaticText && screen->atoms[i].pData != NULL &&
            strcmp((char *)screen->atoms[i].pData, "SMOOTH MOTION") == 0)
        {
            *label = &screen->atoms[i];
        }
    }
    return box;
}

// the screen pushed, the checkbox's region and its first-call state checked, then a click to <on>
static void mtToggleClick(bool32 on, const char *step)
{
    featom *box, *label;
    char what[128];

    feScreenStart(ghMainRegion, "Video_Options");
    box = mtToggleAtom("Video_Options", &label);
    snprintf(what, sizeof(what), "%s: the checkbox on Video_Options", step);
    mtToggleCheck(box != NULL && label != NULL && box->region != NULL && label->region != NULL, what);
    if (box != NULL)
    {
        snprintf(what, sizeof(what), "%s: the checkbox shows the flags", step);
        mtToggleCheck((FECHECKED(box) ? TRUE : FALSE) == miSmoothMotionGet(), what);
        feToggleButtonSet("OP_SmoothMotion", on);       // the FE's checkbox: the state toggled, then the callback
        opSmoothMotion("OP_SmoothMotion", box);
        dbgMessagef("toggletest: %s click to %d: objInterpolate=%d nisInterpolate=%d", step, (int)on, (int)objInterpolate,
                    (int)nisInterpolate);
        snprintf(what, sizeof(what), "%s: the click sets both flags", step);
        mtToggleCheck(objInterpolate == on && nisInterpolate == on, what);
    }
    feCurrentScreenDelete();
}

static sdword mtToggleFileRows(bool32 obj, bool32 nis);

// M3-R1 motion-jitter: the screen's own order. The Smooth motion box is a copy of OP_NoLOD's atom (flags 0x4862 in
// front_end.fib / in_game_esc_menu.fib: FAF_CallOnDelete), so its callback also runs at the screen's deletion; Cancel
// (opOptionsCancelHelper) restores the saved flags BEFORE opOptionsEndHelper's feScreenDisappear deletes the screen,
// and Accept (opOptionsAcceptHelper) saves after it. <click>: a click to the other state first (then Cancel's order:
// the restore, then the deletion); else the screen opened and closed untouched (then Accept's order: the deletion,
// then the save). The flags and a switch's run-only state must be what they were before the screen opened, and the
// untouched close's save must keep the options file's values
static void mtToggleScreenOrder(bool32 click, bool32 fileObj, bool32 fileNis, const char *step)
{
    bool32 obj = objInterpolate, nis = nisInterpolate, switched = miSwitchStands();
    featom *box, *label;
    char what[128];

    feScreenStart(ghMainRegion, "Video_Options");
    box = mtToggleAtom("Video_Options", &label);
    if (click && box != NULL)
    {
        feToggleButtonSet("OP_SmoothMotion", !miSmoothMotionGet());
        opSmoothMotion("OP_SmoothMotion", box);
        opRestoreSavedSettings();                   // Cancel: the restore first,
    }
    feCurrentScreenDelete();                        // then the screen's deletion (its FAS_OnDelete calls)
    if (!click)
    {
        opOptionsSaveSettings();                    // Accept: the save after the deletion
    }
    dbgMessagef("toggletest: %s objInterpolate=%d nisInterpolate=%d switch=%d (before %d/%d switch=%d) rows=%d",
                step, (int)objInterpolate, (int)nisInterpolate, (int)miSwitchStands(), (int)obj, (int)nis,
                (int)switched, (int)mtToggleFileRows(fileObj, fileNis));
    snprintf(what, sizeof(what), "%s: the flags and the switch state as before the screen", step);
    mtToggleCheck(objInterpolate == obj && nisInterpolate == nis && miSwitchStands() == switched, what);
    if (!click)
    {
        snprintf(what, sizeof(what), "%s: the save keeps the options file's values", step);
        mtToggleCheck(mtToggleFileRows(fileObj, fileNis) == 2, what);
    }
}

// the options file the Accept path wrote: both rows at <value>, and read back through utyOptionsList
static void mtToggleFile(bool32 value, const char *step)
{
    char path[PATH_MAX], line[256], what[128];
    FILE *f;
    sdword rows = 0;
    char *read;

    snprintf(path, sizeof(path), "%s", filePathPrepend("Cataclysm.cfg", FF_UserSettingsPath));
    f = fopen(path, "r");
    if (f != NULL)
    {
        while (fgets(line, sizeof(line), f) != NULL)
        {
            int v;
            if ((sscanf(line, "objInterpolate %d", &v) == 1 || sscanf(line, "nisInterpolate %d", &v) == 1) &&
                (bool32)v == value)
            {
                rows++;
            }
        }
        fclose(f);
    }
    snprintf(what, sizeof(what), "%s: Cataclysm.cfg has objInterpolate and nisInterpolate %d", step, (int)value);
    mtToggleCheck(rows == 2, what);
    objInterpolate = !value;                        // scrambled: the file must set them back
    nisInterpolate = !value;
    read = catUtyOptionsFileReadBack();
    dbgMessagef("toggletest: %s file %s rows=%d read back objInterpolate=%d nisInterpolate=%d", step,
                (read != NULL) ? read : path, (int)rows, (int)objInterpolate, (int)nisInterpolate);
    snprintf(what, sizeof(what), "%s: the rows read back", step);
    mtToggleCheck(objInterpolate == value && nisInterpolate == value, what);
}

// M2-R7 r6-leftovers: the options file's objInterpolate / nisInterpolate rows are <obj> / <nis> (no read back)
static sdword mtToggleFileRows(bool32 obj, bool32 nis)
{
    char path[PATH_MAX], line[256];
    FILE *f;
    sdword rows = 0;
    int v;

    snprintf(path, sizeof(path), "%s", filePathPrepend("Cataclysm.cfg", FF_UserSettingsPath));
    f = fopen(path, "r");
    if (f != NULL)
    {
        while (fgets(line, sizeof(line), f) != NULL)
        {
            if ((sscanf(line, "objInterpolate %d", &v) == 1 && (bool32)v == obj) ||
                (sscanf(line, "nisInterpolate %d", &v) == 1 && (bool32)v == nis))
            {
                rows++;
            }
        }
        fclose(f);
    }
    return rows;
}

static void mtToggleConsole(char *name, char *argument, bool32 *flag, bool32 expect)
{
    char buffer[512] = "", line[64], *reply = buffer, what[128];

    snprintf(line, sizeof(line), "%s %s", name, argument);
    cat_consoleRunCommand(name, argument, &reply, line);
    dbgMessagef("toggletest: console '%s' -> %s=%d", line, name, (int)*flag);
    snprintf(what, sizeof(what), "console %s", line);
    mtToggleCheck(*flag == expect, what);
}

// HWI-FEATURE(letterbox-setting) (M3-R1 letterbox-front): the options file's letterboxBehind rows: how many, and the
//      value of the last one (-1: none)
static sdword mtLetterboxFileRow(sdword *count)
{
    char path[PATH_MAX], line[256];
    FILE *f;
    sdword value = -1;
    int v;

    *count = 0;
    snprintf(path, sizeof(path), "%s", filePathPrepend("Cataclysm.cfg", FF_UserSettingsPath));
    f = fopen(path, "r");
    if (f != NULL)
    {
        while (fgets(line, sizeof(line), f) != NULL)
        {
            if (sscanf(line, "letterboxBehind %d", &v) == 1)
            {
                (*count)++;
                value = v;
            }
        }
        fclose(f);
    }
    return value;
}

// HWI-FEATURE(letterbox-setting): the letterboxBehind setting's surfaces: the options-file row written and read back at
//      0 and 1, the switch run-only (/letterboxBehind: the saves keep the file's value), the console row; the start
//      state back at the end. "toggletest: letterbox ..." lines; the failures count into the done line
static void mtToggleLetterbox(bool32 start)          // start: letterboxBehind before the Smooth motion checks, whose
{                                                   // read-backs parse the whole options file again
    bool32 switched = rndLetterboxSwitchStands(), file = rndLetterboxFileValue();
    sdword count, value, i;
    char what[128];

    value = mtLetterboxFileRow(&count);             // the start-up's / this test's earlier saves
    dbgMessagef("toggletest: letterbox start letterboxBehind=%d switch=%d file=%d saved row=%d rows=%d", (int)start,
                (int)switched, (int)file, (int)value, (int)count);
    mtToggleCheck(count == 1 && value == (sdword)file, "letterbox: the saves wrote the options file's letterboxBehind");
    mtToggleCheck(!switched || (start == TRUE && file == FALSE), "letterbox: the switch sets 1 over the harness file's 0");
    rndLetterboxSwitchEnd();                        // the round trip on the option itself
    mtToggleCheck(letterboxBehind == file && !rndLetterboxSwitchStands(), "letterbox: the switch ends at the file's value");
    for (i = 1; i >= 0; i--)
    {
        letterboxBehind = (bool32)i;
        opOptionsSaveSettings();                    // Accept's save: writes Cataclysm.cfg
        value = mtLetterboxFileRow(&count);
        letterboxBehind = !i;                       // scrambled: the file must set it back
        catUtyOptionsFileReadBack();
        dbgMessagef("toggletest: letterbox accept-%d row=%d rows=%d read back letterboxBehind=%d", (int)i, (int)value,
                    (int)count, (int)letterboxBehind);
        snprintf(what, sizeof(what), "letterbox: Cataclysm.cfg has letterboxBehind %d and reads it back", (int)i);
        mtToggleCheck(count == 1 && value == i && letterboxBehind == (bool32)i, what);
    }
    mtToggleConsole("letterboxBehind", "1", &letterboxBehind, TRUE);
    mtToggleConsole("letterboxBehind", "0", &letterboxBehind, FALSE);
    rndLetterboxBehindSwitch();                     // the switch, run-only: the save keeps the file's 0
    opOptionsSaveSettings();
    value = mtLetterboxFileRow(&count);
    dbgMessagef("toggletest: letterbox switch run-only letterboxBehind=%d row=%d rows=%d", (int)letterboxBehind,
                (int)value, (int)count);
    mtToggleCheck(letterboxBehind == TRUE && count == 1 && value == 0, "letterbox: the switch is run-only");
    rndLetterboxSwitchEnd();
    letterboxBehind = file;                         // as the start-up left it
    if (switched)
    {
        rndLetterboxBehindSwitch();
    }
    opOptionsSaveSettings();
    value = mtLetterboxFileRow(&count);
    dbgMessagef("toggletest: letterbox end letterboxBehind=%d switch=%d row=%d", (int)letterboxBehind,
                (int)rndLetterboxSwitchStands(), (int)value);
    mtToggleCheck(letterboxBehind == start && rndLetterboxSwitchStands() == switched && count == 1 &&
                  value == (sdword)file, "letterbox: the start-up's state and file row back");
}

// HWI-FEATURE(full-detail) (M3-R1 full-detail): the options file's noLOD rows: how many, and the last one's value
static sdword mtNoLODFileRow(sdword *count)
{
    char path[PATH_MAX], line[256];
    FILE *f;
    sdword value = -1;
    int v;

    *count = 0;
    snprintf(path, sizeof(path), "%s", filePathPrepend("Cataclysm.cfg", FF_UserSettingsPath));
    f = fopen(path, "r");
    if (f != NULL)
    {
        while (fgets(line, sizeof(line), f) != NULL)
        {
            if (sscanf(line, "noLOD %d", &v) == 1)
            {
                (*count)++;
                value = v;
            }
        }
        fclose(f);
    }
    return value;
}

// HWI-FEATURE(full-detail): the FULL DETAIL setting's surfaces: the start-up's save wrote the options file's noLOD
//      (the default 1 when the file has no row), the Video_Options checkbox (OP_NoLOD) shows it and its click sets it,
//      Accept's save writes the row and it reads back at 0 and 1, the console row, the frame-rate presets
//      (cat_opSetAllDefaults: high 1, low 0), the switch run-only (/lodBands: the saves keep the file's value); the
//      start state back at the end. "toggletest: fulldetail ..." lines; the failures count into the done line
// HWI-FEATURE(instrumentation) (M3-R2): the overlay's three toggles (its console row, /overlay's start state, the Menu
//      key as an SDL_KEYDOWN event handed to main.c HandleEvent, as SDL's event loop does: no OS input) and the settings
//      line at each change (a console row of another setting too); the start state back at the end
static bool32 mtToggleLineHas(const char *a, const char *b)
{
    const char *line = hwiSettingsLast();
    return strstr(line, a) != NULL && (b == NULL || strstr(line, b) != NULL);
}

static void mtToggleKey(void)
{
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    event.type = SDL_KEYDOWN;
    event.key.keysym.scancode = HWI_OVERLAY_KEY;
    event.key.repeat = 0;
    HandleEvent(&event);
    memset(&event, 0, sizeof(event));
    event.type = SDL_KEYUP;
    event.key.keysym.scancode = HWI_OVERLAY_KEY;
    HandleEvent(&event);
}

static void mtToggleOverlay(bool32 switched)
{
    sdword lines = hwiSettingsLines(), start = hwiOverlay;
    char buffer[512] = "", *reply = buffer;
    bool32 ok;

    dbgMessagef("toggletest: overlay start=%d switch=%d (expected %d) line=%s", (int)hwiOverlay, (int)switched,
                (int)(switched ? 1 : 0), mtToggleLineHas(switched ? "overlay=1 (switch)" : "overlay=0 (default)", NULL) ? "yes" : "no");
    mtToggleCheck(hwiOverlay == (switched ? 1 : 0), "overlay: off by default, on under /overlay");
    mtToggleCheck(mtToggleLineHas(switched ? "overlay=1 (switch)" : "overlay=0 (default)", NULL),
                  "overlay: the settings line names its start state and source");
    mtToggleConsole("overlay", "1", (bool32 *)&hwiOverlay, TRUE);
    ok = mtToggleLineHas("overlay=1 (console)", "[console overlay]") && hwiSettingsLines() > lines;
    dbgMessagef("toggletest: overlay console 1 -> %d lines=%d line=%s", (int)hwiOverlay, (int)(hwiSettingsLines() - lines),
                ok ? "yes" : "no");
    mtToggleCheck(ok, "overlay: the console row's change prints the settings line with source console");
    mtToggleKey();
    ok = (hwiOverlay == 0) && mtToggleLineHas("overlay=0 (key)", "[key overlay]");
    dbgMessagef("toggletest: overlay key (scancode %d, vk %u) -> %d line=%s", (int)HWI_OVERLAY_KEY,
                (unsigned)catKeyVkFromKey(HWI_OVERLAY_KEY), (int)hwiOverlay, ok ? "yes" : "no");
    mtToggleCheck(ok, "overlay: the Menu key toggles it off and prints the line with source key");
    mtToggleCheck(catKeyVkFromKey(HWI_OVERLAY_KEY) == 0, "overlay: the key has no virtual-key code (keys.cfg cannot bind it)");
    mtToggleKey();
    mtToggleCheck(hwiOverlay == 1, "overlay: the key toggles it on again");
    mtToggleConsole("overlay", "0", (bool32 *)&hwiOverlay, FALSE);
    // another setting's console row: the line with source console
    cat_consoleRunCommand("letterboxBehind", letterboxBehind ? "0" : "1", &reply, "letterboxBehind");
    ok = mtToggleLineHas(letterboxBehind ? "letterboxBehind=1 (console)" : "letterboxBehind=0 (console)", "[console letterboxBehind]");
    dbgMessagef("toggletest: settings line console letterboxBehind -> %d line=%s", (int)letterboxBehind, ok ? "yes" : "no");
    mtToggleCheck(ok, "settings line: a console row's change prints it with source console");
    cat_consoleRunCommand("letterboxBehind", letterboxBehind ? "0" : "1", &reply, "letterboxBehind");
    if (start)
    {
        hwiOverlaySwitch();                         // /overlay's state back
    }
    hwiSettingsChanged("toggletest end");
    dbgMessagef("toggletest: overlay end=%d lines=%d", (int)hwiOverlay, (int)(hwiSettingsLines() - lines));
    mtToggleCheck(hwiOverlay == start, "overlay: the start state back");
}

static void mtToggleFullDetail(sdword start, bool32 switched)  // start / switched: noLOD and /lodBands before the Smooth
{                                                   // motion checks, whose read-backs parse the whole options file again
    sdword file = catLodFileValueGet(), count, value, i;
    fescreen *screen;
    featom *box = NULL;
    char what[128];

    value = mtNoLODFileRow(&count);
    dbgMessagef("toggletest: fulldetail start noLOD=%d switch=%d file=%d saved row=%d rows=%d", (int)start,
                (int)switched, (int)file, (int)value, (int)count);
    mtToggleCheck(count == 1 && value == file, "fulldetail: the saves wrote the options file's noLOD");
    mtToggleCheck(switched ? (start == 0 && file == 1) : (start == 1), "fulldetail: on by default (the switch: off over the file's 1)");
    catLodSwitchEnd();                              // the round trip on the option itself
    opNoLODVal = file;
    // the checkbox: shown as the option, a click sets it
    feScreenStart(ghMainRegion, "Video_Options");
    screen = feScreenFind("Video_Options");
    for (i = 0; screen != NULL && i < screen->nAtoms; i++)
    {
        if (screen->atoms[i].type == FA_CheckBox && screen->atoms[i].name != NULL &&
            strcmp(screen->atoms[i].name, "OP_NoLOD") == 0)
        {
            box = &screen->atoms[i];
        }
    }
    mtToggleCheck(box != NULL && box->region != NULL, "fulldetail: the FULL DETAIL checkbox on Video_Options");
    if (box != NULL)
    {
        mtToggleCheck((FECHECKED(box) ? 1 : 0) == (opNoLODVal ? 1 : 0), "fulldetail: the checkbox shows noLOD");
        for (i = 0; i <= 1; i++)
        {
            feToggleButtonSet("OP_NoLOD", i);       // the FE's checkbox: the state toggled, then the callback
            opNoLOD("OP_NoLOD", box);
            dbgMessagef("toggletest: fulldetail click to %d: noLOD=%d", (int)i, (int)opNoLODVal);
            snprintf(what, sizeof(what), "fulldetail: the click to %d sets noLOD", (int)i);
            mtToggleCheck(opNoLODVal == i, what);
        }
    }
    feCurrentScreenDelete();
    for (i = 0; i <= 1; i++)
    {
        opNoLODVal = i;
        opOptionsSaveSettings();                    // Accept's save: writes Cataclysm.cfg
        value = mtNoLODFileRow(&count);
        opNoLODVal = !i;                            // scrambled: the file must set it back
        catUtyOptionsFileReadBack();
        dbgMessagef("toggletest: fulldetail accept-%d row=%d rows=%d read back noLOD=%d", (int)i, (int)value, (int)count,
                    (int)opNoLODVal);
        snprintf(what, sizeof(what), "fulldetail: Cataclysm.cfg has noLOD %d and reads it back", (int)i);
        mtToggleCheck(count == 1 && value == i && opNoLODVal == i, what);
    }
    mtToggleConsole("noLOD", "0", (bool32 *)&opNoLODVal, FALSE);
    mtToggleConsole("noLOD", "1", (bool32 *)&opNoLODVal, TRUE);
    // the frame-rate presets (the options screen's defaults): high FULL DETAIL, low the exe's bands
    opOptionsSaveSettings();
    cat_opSetAllDefaults(TRUE);
    value = opNoLODVal;
    cat_opSetAllDefaults(FALSE);
    dbgMessagef("toggletest: fulldetail presets low=%d high=%d", (int)value, (int)opNoLODVal);
    mtToggleCheck(value == 0 && opNoLODVal == 1, "fulldetail: the presets set noLOD 0 (low) and 1 (high)");
    opRestoreSavedSettings();
    // the switch, run-only: the save keeps the file's 1
    opNoLODVal = 1;
    catLodBandsSwitch();
    opOptionsSaveSettings();
    value = mtNoLODFileRow(&count);
    dbgMessagef("toggletest: fulldetail switch run-only noLOD=%d row=%d rows=%d", (int)opNoLODVal, (int)value, (int)count);
    mtToggleCheck(opNoLODVal == 0 && count == 1 && value == 1, "fulldetail: the switch is run-only");
    catLodSwitchEnd();
    opNoLODVal = file;                              // as the start-up left it
    if (switched)
    {
        catLodBandsSwitch();
    }
    opOptionsSaveSettings();
    value = mtNoLODFileRow(&count);
    dbgMessagef("toggletest: fulldetail end noLOD=%d switch=%d row=%d", (int)opNoLODVal, (int)catLodSwitchStands(),
                (int)value);
    mtToggleCheck(opNoLODVal == start && catLodSwitchStands() == switched && count == 1 && value == file,
                  "fulldetail: the start-up's state and file row back");
}

void catToggleRunSelfTest(void)
{
    bool32 startObj = objInterpolate, startNis = nisInterpolate;
    bool32 switched = miSwitchStands(), fileObj, fileNis;
    featom *box, *label;
    sdword rows;
    bool32 startLetterbox = letterboxBehind;        // HWI-FEATURE(letterbox-setting)
    sdword startNoLOD = opNoLODVal;                 // HWI-FEATURE(full-detail)
    bool32 startLodSwitch = catLodSwitchStands();   // HWI-FEATURE(full-detail)
    bool32 startOverlay = hwiOverlay ? TRUE : FALSE;  // HWI-FEATURE(instrumentation): 1 only under /overlay

    miOptionsFileValues(TRUE);                      // M2-R7 r6-leftovers: the values the options file keeps
    fileObj = objInterpolate;
    fileNis = nisInterpolate;
    miOptionsFileValues(FALSE);
    dbgMessagef("toggletest: start objInterpolate=%d nisInterpolate=%d (the options file, then the switches) switch=%d file=%d/%d",
                (int)startObj, (int)startNis, (int)switched, (int)fileObj, (int)fileNis);
    opOptionsSaveSettings();                        // the Cancel copies, as the options screens keep them
    // M2-R7 r6-leftovers: the save wrote the file's values, not the switch's; the run's flags are untouched
    rows = mtToggleFileRows(fileObj, fileNis);
    dbgMessagef("toggletest: run-only save rows=%d objInterpolate=%d nisInterpolate=%d", (int)rows, (int)objInterpolate,
                (int)nisInterpolate);
    mtToggleCheck(rows == 2 && objInterpolate == startObj && nisInterpolate == startNis,
                  "the start-up's save keeps the options file's values");
    mtToggleCheck(!switched || (fileObj != startObj || fileNis != startNis), "a switch differs from the file (the harness file: 1/1)");

    // Cancel: a click, then the saved flags back
    mtToggleClick(!miSmoothMotionGet(), "cancel");
    opRestoreSavedSettings();
    dbgMessagef("toggletest: cancel restored objInterpolate=%d nisInterpolate=%d", (int)objInterpolate, (int)nisInterpolate);
    mtToggleCheck(objInterpolate == startObj && nisInterpolate == startNis, "cancel restores the flags");
    mtToggleCheck(miSwitchStands() == switched, "cancel restores the switch's run-only state");   // M2-R7 r6-leftovers
    // M3-R1 motion-jitter: the screens' own orders (the box's FAS_OnDelete call): Cancel after a click, and the screen
    //      opened and closed untouched (under a switch the box shows off: its deletion must not make that the user's)
    mtToggleScreenOrder(FALSE, fileObj, fileNis, "screen untouched");
    mtToggleScreenOrder(TRUE, fileObj, fileNis, "screen cancel");

    // Accept: off, persisted; then on, persisted
    mtToggleClick(FALSE, "accept-off");
    opOptionsSaveSettings();
    mtToggleFile(FALSE, "accept-off");
    mtToggleClick(TRUE, "accept-on");
    opOptionsSaveSettings();
    mtToggleFile(TRUE, "accept-on");

    // the in-game Video screen has the control too (not pushed: no game runs)
    box = mtToggleAtom("Video", &label);
    dbgMessagef("toggletest: in-game Video screen checkbox=%s label=%s", (box != NULL) ? "yes" : "no",
                (label != NULL) ? "yes" : "no");
    mtToggleCheck(box != NULL && label != NULL && box->type == FA_CheckBox, "the in-game Video screen's checkbox");

    // the console rows, one flag at a time
    mtToggleConsole("objInterpolate", "0", &objInterpolate, FALSE);
    mtToggleCheck(nisInterpolate == TRUE, "objInterpolate 0 leaves nisInterpolate");
    mtToggleConsole("nisInterpolate", "0", &nisInterpolate, FALSE);
    mtToggleConsole("objInterpolate", "1", &objInterpolate, TRUE);
    mtToggleConsole("nisInterpolate", "1", &nisInterpolate, TRUE);

    objInterpolate = switched ? fileObj : startObj; // as the start-up left them (M2-R7: a switch's override again)
    nisInterpolate = switched ? fileNis : startNis;
    if (switched)
    {
        miSwitchOff(startObj != fileObj, startNis != fileNis);
    }
    mtToggleCheck(objInterpolate == startObj && nisInterpolate == startNis && miSwitchStands() == switched,
                  "the start-up's flags and switch state back");
    opOptionsSaveSettings();
    if (switched)
    {
        rows = mtToggleFileRows(fileObj, fileNis);
        dbgMessagef("toggletest: run-only final save rows=%d", (int)rows);
        mtToggleCheck(rows == 2, "the final save keeps the options file's values");
    }
    mtToggleLetterbox(startLetterbox);              // HWI-FEATURE(letterbox-setting)
    mtToggleFullDetail(startNoLOD, startLodSwitch); // HWI-FEATURE(full-detail)
    mtToggleOverlay(startOverlay);                  // HWI-FEATURE(instrumentation)
    dbgMessagef("toggletest: done start=%d/%d fails=%d result=%s", (int)startObj, (int)startNis, (int)mtToggleFails,
                (mtToggleFails == 0) ? "PASS" : "FAIL");
}
