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
#include "Debug.h"
#include "glinc.h"
#include "Globals.h"
#include "Console.h"
#include "FEFlow.h"
#include "File.h"
#include "LevelLoad.h"
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

/*=============================================================================
    Data:
=============================================================================*/

#define MT_MODE_NONE        0
#define MT_MODE_MOTION      1
#define MT_MODE_NIS         2

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
    Hooks:
=============================================================================*/

void catMotionTestTick(void)
{
    if (mtMode == MT_MODE_MOTION)
    {
        mtMotionTick();
    }
    else if (mtMode == MT_MODE_NIS)
    {
        mtNisTick();
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
    else if (mtNisPlaying != NULL && thisNisPlaying == mtNisPlaying)
    {
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
    size = (size_t)MAIN_WindowWidth * (size_t)MAIN_WindowHeight * 4;
    if (mtPixels == NULL)
    {
        mtPixels = malloc(size);
    }
    if (mtPixels != NULL)
    {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, MAIN_WindowWidth, MAIN_WindowHeight, GL_RGBA, GL_UNSIGNED_BYTE, mtPixels);
        crc = mtCrc(mtPixels, size);
        if ((sdword)mtFrame == mtShotFrame)
        {                                           // <dump>.<frame>.ppm, top row first
            char name[1100];
            FILE *f;
            sdword row;

            snprintf(name, sizeof(name), "%s.%u.ppm", mtDumpName, (unsigned)mtFrame);
            if ((f = fopen(name, "wb")) != NULL)
            {
                fprintf(f, "P6\n%d %d\n255\n", (int)MAIN_WindowWidth, (int)MAIN_WindowHeight);
                for (row = MAIN_WindowHeight - 1; row >= 0; row--)
                {
                    ubyte *p = mtPixels + (size_t)row * (size_t)MAIN_WindowWidth * 4;
                    sdword x;
                    for (x = 0; x < MAIN_WindowWidth; x++, p += 4)
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

static void mtToggleConsole(char *name, char *argument, bool32 *flag, bool32 expect)
{
    char buffer[512] = "", line[64], *reply = buffer, what[128];

    snprintf(line, sizeof(line), "%s %s", name, argument);
    cat_consoleRunCommand(name, argument, &reply, line);
    dbgMessagef("toggletest: console '%s' -> %s=%d", line, name, (int)*flag);
    snprintf(what, sizeof(what), "console %s", line);
    mtToggleCheck(*flag == expect, what);
}

void catToggleRunSelfTest(void)
{
    bool32 startObj = objInterpolate, startNis = nisInterpolate;
    featom *box, *label;

    dbgMessagef("toggletest: start objInterpolate=%d nisInterpolate=%d (the options file, then the switches)",
                (int)startObj, (int)startNis);
    opOptionsSaveSettings();                        // the Cancel copies, as the options screens keep them

    // Cancel: a click, then the saved flags back
    mtToggleClick(!miSmoothMotionGet(), "cancel");
    opRestoreSavedSettings();
    dbgMessagef("toggletest: cancel restored objInterpolate=%d nisInterpolate=%d", (int)objInterpolate, (int)nisInterpolate);
    mtToggleCheck(objInterpolate == startObj && nisInterpolate == startNis, "cancel restores the flags");

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

    objInterpolate = startObj;                      // as the start-up left them
    nisInterpolate = startNis;
    opOptionsSaveSettings();
    dbgMessagef("toggletest: done start=%d/%d fails=%d result=%s", (int)startObj, (int)startNis, (int)mtToggleFails,
                (mtToggleFails == 0) ? "PASS" : "FAIL");
}
