/*=============================================================================
    Name    : Instrument.c
    Purpose : HWI-FEATURE(instrumentation) (M3-R2 producer 1; docs/m3-r2-instrumentation.md): so that a session's state
              is never in doubt again (the user's 2026-10-21 07:43 play-test ran with Smooth motion OFF and nothing in
              its log said so) and as the instrument of the M3 soak (docs/features/high-refresh-4k.md):

              1. THE SETTINGS LINE: "hwi-settings: <row>=<value> (<source>) ... fit=.. switches=.. [<why>]" in the log
                 once the options file and the command line are applied and the window exists (uiscale.c
                 uiScaleWindowCreated), and again whenever a setting changes (the Options screen's Accept, a console
                 row, the overlay's key or switch, anything else the frame's poll sees). The source of each value:
                 default (the options file has no row), cfg (the file's row, or the row the Options screen's Accept
                 saved), switch (a run-only switch overrides the row), console (a developer-console row), options (an
                 Options-screen control, not saved yet), key (the overlay's key).
              2. THE OVERLAY: off by default; the console row "overlay", the run-only switch /overlay and the Menu
                 (Application) key toggle it. Drawn after the scene, the HUD and the cursor (rndRenderTask), at the UI
                 scale (logical coordinates), in the top-left corner, with the game's default font: the frame time
                 (the last frame, the last second's median and 99th percentile), the universe tick's cost in the last
                 frame that ran one, the interpolation alpha the frame was drawn with, the flags and switches, the
                 vertices drawn (/catLodStats' count), FULL DETAIL and the UI scale.
              3. HWI-TEST /frameLog <file>: one line a second (frame-time percentiles, the frames over the refresh
                 interval and by how many refreshes, the universe ticks and their cost, the main heap's use and growth,
                 the vertices drawn); a header line with the build, the sizes, the refresh and the settings line.
                 tools/frame_log_report.py summarises a log.

              The clocks: SDL's performance counter (as Task.c taskClockSample reads it). The FRAME TIME is the period
              between two successive ends of the render task (after the flip): what the display shows, every task of
              the frame and the wait for the swap included. RND is the render task alone, from its top to after the
              flip, with no glFinish (the GL driver may still be drawing). /catLodStats' ms is RND plus a glFinish
              before the clock is read (render.c catLodStatsFrameEnd): the GPU's work done. Nothing here reads the
              framebuffer back, calls glFinish or draws from a random stream; no simulation field is touched.
    Created 2026-10-22 for Homeworld-Infection.
=============================================================================*/

#include "Instrument.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "Color.h"
#include "Debug.h"
#include "font.h"
#include "Globals.h"
#include "LOD.h"
#include "main.h"
#include "Memory.h"
#include "MotionInterp.h"
#include "MotionTest.h"
#include "prim2d.h"
#include "render.h"
#include "uiscale.h"
#include "Universe.h"
#include "utility.h"
#include "texreg.h"                                 // HWI-FEATURE(sharp-textures): texLinearFiltering, /noFilter's state
#include "Console.h"                                // HWI-TEST: /catConsoleAt
#include "FEFlow.h"                                 // HWI-TEST: /catFeScreen
#include "mainrgn.h"                                // HWI-TEST: /catFeScreen (ghMainRegion)

#if HWI_ORACLE
#include "hwi_oracle_build.h"                       // HWI-TEST: the commit for the frame log's header
#endif

extern fonthandle ghDefaultFont;                    // utility.c
extern SDL_Window *sdlwindow;                       // render.c
extern sdword opNoLODVal;                           // Options.c: FULL DETAIL (the noLOD row)

sdword hwiOverlay = 0;                              // HWI-FEATURE(instrumentation): the console row "overlay"

/*=============================================================================
    The settings line.
=============================================================================*/
typedef enum
{
    HWI_SRC_DEFAULT = 0,                            // no row in the options file
    HWI_SRC_CFG,                                    // the options file's row, or the row the Options screen's Accept saved
    HWI_SRC_SWITCH,                                 // a run-only switch overrides the row
    HWI_SRC_CONSOLE,                                // a developer-console row
    HWI_SRC_OPTIONS,                                // an Options-screen control (not saved yet)
    HWI_SRC_KEY,                                    // the overlay's key
    HWI_SRC_PRESET                                  // HWI-FEATURE(sharp-textures): an effects preset (opEffectsHelper: HIGH / LOW, also at the start)
}
HwiSource;

static const char *hwiSourceNames[] = { "default", "cfg", "switch", "console", "options", "key", "preset" };

// one row per HWI setting. A later producer adds its setting here (a value getter, its run-only switch's test and
//      name, whether the options file has a row of that name); the line, the change detection and the sources follow
typedef struct
{
    const char *name;                               // the options-file / console row
    sdword (*value)(void);
    bool32 (*switched)(void);                       // NULL: no switch
    const char *(*switchName)(void);                // the switch as typed
    bool32 hasRow;                                  // an options-file row of this name exists
}
HwiSetting;

static sdword hwiValObj(void)       { return objInterpolate ? 1 : 0; }
static sdword hwiValNis(void)       { return nisInterpolate ? 1 : 0; }
static sdword hwiValLetterbox(void) { return letterboxBehind ? 1 : 0; }
static sdword hwiValNoLOD(void)     { return opNoLODVal; }
static sdword hwiValUiScale(void)   { return uiScale; }
static sdword hwiValOverlay(void)   { return hwiOverlay ? 1 : 0; }
static sdword hwiValMeshImm(void)   { return meshImmediate ? 1 : 0; }      // HWI-FEATURE(mesh-batching)
static bool32 hwiSwMeshImm(void)    { return meshImmediateSwitched; }        // HWI-FEATURE(mesh-batching)
static const char *hwiSwNameMeshImm(void)   { return "/meshImmediate"; }   // HWI-FEATURE(mesh-batching)

static bool32 hwiOverlaySwitched = FALSE;           // /overlay stands (until a console row or the key changes it)
static bool32 hwiSwOverlay(void)    { return hwiOverlaySwitched; }

static const char *hwiSwNameObj(void)       { return "/noInterp"; }
static const char *hwiSwNameNis(void)       { return miSwitchObjStands() ? "/noInterp" : "/noNISInterp"; }
static const char *hwiSwNameLetterbox(void) { return "/letterboxBehind"; }
static const char *hwiSwNameLod(void)       { return "/lodBands"; }
static const char *hwiSwNameUiScale(void)   { static char name[24]; snprintf(name, sizeof(name), "/uiScale %d", (int)uiScale); return name; }
static const char *hwiSwNameOverlay(void)   { return "/overlay"; }
static sdword hwiValFilter(void)    { return texLinearFiltering ? 1 : 0; }   // HWI-FEATURE(sharp-textures): 0 = SHARP TEXTURES
static const char *hwiSwNameFilter(void)    { return "/noFilter"; }        // HWI-FEATURE(sharp-textures): or /sharpTextures

static const HwiSetting hwiSettings[] =
{
    { "objInterpolate",  hwiValObj,       miSwitchObjStands,        hwiSwNameObj,       TRUE  },   // HWI-FEATURE(motion-interp)
    { "nisInterpolate",  hwiValNis,       miSwitchNisStands,        hwiSwNameNis,       TRUE  },   // HWI-FEATURE(motion-interp)
    { "letterboxBehind", hwiValLetterbox, rndLetterboxSwitchStands, hwiSwNameLetterbox, TRUE  },   // HWI-FEATURE(letterbox-setting)
    { "noLOD",           hwiValNoLOD,     catLodSwitchStands,       hwiSwNameLod,       TRUE  },   // HWI-FEATURE(full-detail)
    { "uiScale",         hwiValUiScale,   uiScaleSwitchStands,      hwiSwNameUiScale,   TRUE  },   // HWI-FEATURE(ui-scale)
    { "overlay",         hwiValOverlay,   hwiSwOverlay,             hwiSwNameOverlay,   FALSE },   // HWI-FEATURE(instrumentation)
    { "meshImmediate",   hwiValMeshImm,   hwiSwMeshImm,             hwiSwNameMeshImm,   FALSE },   // HWI-FEATURE(mesh-batching): no options-file row
    { "texLinearFiltering", hwiValFilter, trFilterSwitchStands,     hwiSwNameFilter,    TRUE  },   // HWI-FEATURE(sharp-textures): 1 the original look, 0 SHARP TEXTURES
};
#define HWI_SETTINGS    ((sdword)(sizeof(hwiSettings) / sizeof(hwiSettings[0])))

static bool32 hwiRowPresent[HWI_SETTINGS];          // the options file read at start-up had the row
static HwiSource hwiSource[HWI_SETTINGS];           // the source last printed
static HwiSource hwiPending[HWI_SETTINGS];          // the source of a change a hook announced (DEFAULT: none)
static sdword hwiLastValue[HWI_SETTINGS];
static bool32 hwiLastSwitched[HWI_SETTINGS];
static sdword hwiLastPercent = -1;                  // the UI scale in use at the last line
static bool32 hwiStarted = FALSE;                   // the start-up line was printed
static char hwiLine[1024];                          // the last line printed
static char hwiSwitchText[160] = "none";            // the switches standing at the last line (the overlay shows them)
static sdword hwiLineCount = 0;

// utyOptionsFileRead: the rows the options file has (the read itself is scriptSetFileSystem's; this only notes the
//      names so the line can tell a row from a default)
void hwiSettingsRowsRead(const char *directory)                 // HWI-FEATURE(instrumentation)
{
    char path[1024], line[256], name[64];
    FILE *f;
    sdword i;

    for (i = 0; i < HWI_SETTINGS; i++)
    {
        hwiRowPresent[i] = FALSE;
    }
    snprintf(path, sizeof(path), "%sCataclysm.cfg", directory);  // utility.c UTY_CONFIG_FILENAME
    f = fopen(path, "rt");
    if (f == NULL)
    {
        return;
    }
    while (fgets(line, sizeof(line), f) != NULL)
    {
        if (sscanf(line, " %63s", name) != 1)
        {
            continue;
        }
        for (i = 0; i < HWI_SETTINGS; i++)
        {
            if (hwiSettings[i].hasRow && strcmp(name, hwiSettings[i].name) == 0)
            {
                hwiRowPresent[i] = TRUE;
            }
        }
    }
    fclose(f);
}

// the window fit's rule and the window's kind, as the "window:" line names them
static void hwiFitText(char *buf, size_t size)
{
    udword flags = (sdlwindow != NULL) ? SDL_GetWindowFlags(sdlwindow) : 0;
    const char *kind = ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP) ? "fullscreen-desktop" :
                       ((flags & SDL_WINDOW_FULLSCREEN) ? "fullscreen" :
                       ((flags & SDL_WINDOW_BORDERLESS) ? "borderless window" : "window"));

    snprintf(buf, size, "%s (%s) %dx%d", kind, uiScaleFitRule(), (int)MAIN_WindowWidthActual, (int)MAIN_WindowHeightActual);
}

static void hwiSettingsPrint(const char *why, bool32 force)
{
    char line[1024], fit[160], switches[160];
    size_t n = 0;
    sdword i, value;
    bool32 sw, changed = force;

    for (i = 0; i < HWI_SETTINGS; i++)
    {
        value = hwiSettings[i].value();
        sw = hwiSettings[i].switched != NULL && hwiSettings[i].switched();
        if (!hwiStarted)
        {
            hwiSource[i] = sw ? HWI_SRC_SWITCH : (hwiRowPresent[i] ? HWI_SRC_CFG : HWI_SRC_DEFAULT);
        }
        else if (sw)
        {
            hwiSource[i] = HWI_SRC_SWITCH;
        }
        else if (hwiPending[i] != HWI_SRC_DEFAULT)
        {
            hwiSource[i] = hwiPending[i];
        }
        else if (value != hwiLastValue[i] || hwiLastSwitched[i])
        {
            hwiSource[i] = HWI_SRC_OPTIONS;         // changed by nothing that announced itself: an Options control
        }
        if (value != hwiLastValue[i] || sw != hwiLastSwitched[i])
        {
            changed = TRUE;
        }
        hwiPending[i] = HWI_SRC_DEFAULT;
        hwiLastValue[i] = value;
        hwiLastSwitched[i] = sw;
    }
    if (uiScalePercent != hwiLastPercent)
    {
        changed = TRUE;
        hwiLastPercent = uiScalePercent;
    }
    if (!changed)
    {
        return;
    }
    hwiStarted = TRUE;
    switches[0] = '\0';
    for (i = 0; i < HWI_SETTINGS; i++)
    {
        if (hwiSource[i] == HWI_SRC_SWITCH && (i != 1 || !miSwitchObjStands()))    // /noInterp once for both flags
        {
            snprintf(switches + strlen(switches), sizeof(switches) - strlen(switches), "%s%s",
                     (switches[0] != '\0') ? "," : "", hwiSettings[i].switchName());
        }
    }
    for (i = 0; i < HWI_SETTINGS && n < sizeof(line); i++)
    {
        value = hwiSettings[i].value();
        if (i == 4)                                 // uiScale: the row and the scale in use
        {
            n += (size_t)snprintf(line + n, sizeof(line) - n, "%s=%d->%d%% (%s%s) ", hwiSettings[i].name, (int)value,
                                  (int)uiScalePercent, hwiSourceNames[hwiSource[i]], (uiScale > 0) ? "" : ", auto");
        }
        else
        {
            n += (size_t)snprintf(line + n, sizeof(line) - n, "%s=%d (%s) ", hwiSettings[i].name, (int)value,
                                  hwiSourceNames[hwiSource[i]]);
        }
    }
    hwiFitText(fit, sizeof(fit));
    if (n < sizeof(line))
    {
        snprintf(line + n, sizeof(line) - n, "fit=%s switches=%s [%s]", fit, (switches[0] != '\0') ? switches : "none",
                 why);
    }
    snprintf(hwiSwitchText, sizeof(hwiSwitchText), "%s", (switches[0] != '\0') ? switches : "none");
    strncpy(hwiLine, line, sizeof(hwiLine) - 1);
    hwiLine[sizeof(hwiLine) - 1] = '\0';
    hwiLineCount++;
    printf("hwi-settings: %s\n", line);
    fflush(stdout);
}

// a window exists (the first: the start-up line; a later one: a mode switch)
void hwiSettingsWindow(void)                                    // HWI-FEATURE(instrumentation)
{
    hwiSettingsPrint(hwiStarted ? "window" : "start", TRUE);
}

void hwiSettingsConsole(const char *row)                        // HWI-FEATURE(instrumentation)
{
    char why[80];
    sdword i;

    for (i = 0; i < HWI_SETTINGS; i++)
    {
        if (strcmp(row, hwiSettings[i].name) == 0)
        {
            hwiPending[i] = HWI_SRC_CONSOLE;
            if (i == 5)
            {
                hwiOverlaySwitched = FALSE;         // the console's choice ends /overlay's
            }
            if (i == 6)                                         // HWI-FEATURE(mesh-batching)
            {                                                   // HWI-FEATURE(mesh-batching)
                meshImmediateSwitched = FALSE;                  // HWI-FEATURE(mesh-batching): the console's choice ends /meshImmediate's
            }                                                   // HWI-FEATURE(mesh-batching)
        }
    }
    snprintf(why, sizeof(why), "console %s", row);
    if (hwiStarted)
    {
        hwiSettingsPrint(why, FALSE);
    }
}

// the Options screen's Accept: every setting not under a switch is now the row the save writes
void hwiSettingsAccepted(void)                                  // HWI-FEATURE(instrumentation)
{
    sdword i;

    for (i = 0; i < HWI_SETTINGS; i++)
    {
        if (hwiSettings[i].hasRow && !(hwiSettings[i].switched != NULL && hwiSettings[i].switched()))
        {
            hwiPending[i] = HWI_SRC_CFG;
            if (hwiSource[i] != HWI_SRC_CFG && hwiStarted)
            {
                hwiSource[i] = HWI_SRC_CFG;
                hwiLastValue[i] = -12345;           // print the line: the source changed
            }
        }
    }
    if (hwiStarted)
    {
        hwiSettingsPrint("options accept", FALSE);
    }
}

void hwiSettingsChanged(const char *why)                        // HWI-FEATURE(instrumentation)
{
    if (hwiStarted)
    {
        hwiSettingsPrint(why, FALSE);
    }
}

// HWI-FEATURE(sharp-textures): a preset (the effects level's HIGH / LOW, opEffectsHelper; at every start too) set the row:
//      the source "preset" when it changed the value (and no switch stands: the switch keeps its label)
void hwiSettingsPreset(const char *row, const char *why)        // HWI-FEATURE(sharp-textures)
{
    sdword i;

    for (i = 0; i < HWI_SETTINGS; i++)
    {
        if (strcmp(row, hwiSettings[i].name) == 0 && hwiSettings[i].value() != hwiLastValue[i])
        {
            hwiPending[i] = HWI_SRC_PRESET;
        }
    }
    if (hwiStarted)
    {
        hwiSettingsPrint(why, FALSE);
    }
}

const char *hwiSettingsLast(void)                               // HWI-FEATURE(instrumentation)
{
    return hwiLine;
}

sdword hwiSettingsLines(void)                                   // HWI-FEATURE(instrumentation)
{
    return hwiLineCount;
}

/*=============================================================================
    The overlay's surfaces.
=============================================================================*/
void hwiOverlaySwitch(void)                                     // HWI-FEATURE(instrumentation): /overlay
{
    hwiOverlay = 1;
    hwiOverlaySwitched = TRUE;
}

void hwiOverlayKey(void)                                        // HWI-FEATURE(instrumentation): the Menu key
{
    hwiOverlay = hwiOverlay ? 0 : 1;
    hwiOverlaySwitched = FALSE;
    hwiPending[5] = HWI_SRC_KEY;
    hwiSettingsChanged("key overlay");
}

/*=============================================================================
    The frame's clocks.
=============================================================================*/
#define HWI_SAMPLES     8192                        // frames kept a second (beyond: counted, not sorted)

static Uint64 hwiFreq = 0;
static Uint64 hwiTop = 0;                           // this render task's top
static Uint64 hwiLastEnd = 0;                       // the last frame's end (after the flip)
static Uint64 hwiSecondStart = 0;                   // the second's start
static Uint64 hwiLogStart = 0;
static Uint64 hwiUnivCallStart = 0;
static real64 hwiUnivFrameMs = 0.0;                 // the universe task's time in this frame
static sdword hwiUnivFrameCalls = 0;
static real64 hwiUnivLastMs = -1.0;                 // the last frame that ran a tick: the universe task's time
static sdword hwiUnivLastCalls = 0;
static udword hwiFrameVerts = 0, hwiFrameObjects = 0;
static bool32 hwiFrameHadView = FALSE;

static real32 hwiFt[HWI_SAMPLES], hwiRnd[HWI_SAMPLES], hwiTick[HWI_SAMPLES], hwiVerts[HWI_SAMPLES];
static sdword hwiNFt = 0, hwiNRnd = 0, hwiNTick = 0, hwiNVerts = 0, hwiFramesSecond = 0, hwiTicksSecond = 0;
static sdword hwiOver[4] = { 0, 0, 0, 0 };          // frames over the interval by 1, 2, 3+ refreshes (index 1..3)
#define HWI_HIST_BINS   1001                        // the second's frame periods in 0.1 ms bins (the last: 100 ms and more)
static sdword hwiHist[HWI_HIST_BINS];

// the overlay's figures (refreshed once a second, the last frame's every frame)
static real32 hwiShowLast = 0.0f, hwiShowMedian = 0.0f, hwiShowP99 = 0.0f;
static real32 hwiShowAlpha = -1.0f;
static bool32 hwiShowSecond = FALSE;                // a whole second was measured (else the median / p99 show "-")

static FILE *hwiLogFile = NULL;
static char hwiLogName[1024] = "";
static sdword hwiLogSecond = 0;
static sdword hwiHeapStart = -1;
static real32 hwiRefreshHz = 144.0f;
static const char *hwiRefreshSource = "default 144";

static real64 hwiMs(Uint64 counts)
{
    return (real64)counts * 1000.0 / (real64)hwiFreq;
}

static int hwiCompare(const void *a, const void *b)
{
    real32 x = *(const real32 *)a, y = *(const real32 *)b;
    return (x < y) ? -1 : ((x > y) ? 1 : 0);
}

// the q-quantile (nearest rank) of n sorted samples
static real32 hwiQuantile(const real32 *v, sdword n, sdword percent)
{
    sdword rank;

    if (n <= 0)
    {
        return 0.0f;
    }
    rank = (n * percent + 99) / 100 - 1;
    rank = (rank < 0) ? 0 : ((rank >= n) ? n - 1 : rank);
    return v[rank];
}

static bool32 hwiActive(void)
{
    return hwiOverlay || hwiLogFile != NULL;
}

bool32 hwiWantVerts(void)                                       // HWI-FEATURE(instrumentation)
{
    return hwiActive();
}

void hwiVertsDrawn(udword objects, udword verts)                // HWI-FEATURE(instrumentation)
{
    hwiFrameObjects = objects;
    hwiFrameVerts = verts;
    hwiFrameHadView = TRUE;
}

void hwiUniverseCallBegin(void)                                 // HWI-FEATURE(instrumentation)
{
    if (hwiFreq != 0)
    {
        hwiUnivCallStart = SDL_GetPerformanceCounter();
    }
}

void hwiUniverseCallEnd(void)                                   // HWI-FEATURE(instrumentation)
{
    real64 ms;

    if (hwiFreq == 0 || hwiUnivCallStart == 0)
    {
        return;
    }
    ms = hwiMs(SDL_GetPerformanceCounter() - hwiUnivCallStart);
    hwiUnivCallStart = 0;
    hwiUnivFrameMs += ms;
    hwiUnivFrameCalls++;
    if (hwiActive() && hwiNTick < HWI_SAMPLES)
    {
        hwiTick[hwiNTick++] = (real32)ms;
    }
    hwiTicksSecond++;
}

/*=============================================================================
    HWI-TEST (M3-R3 settings-and-keys): scripted inputs for the captures, through the engine's own paths (no OS input):
    /catOverlayKeyAt <tick>[,<tick>..] hands F1 to main.c HandleEvent as an SDL_KEYDOWN and an SDL_KEYUP (as SDL's event
    loop does) at the first frame of each universe tick; /catConsoleAt <tick>:<row>=<value>[,..] runs a developer-console
    row at that tick (cat_consoleRunCommand, the console's own command path); /catFeScreen <screen>[@<tick>] pushes a
    front-end screen once (feScreenStart): without a tick once the front end shows Main_game_screen, with one in game
    at that tick. One "hwiscript:" line each.
=============================================================================*/
#define HWI_SCRIPT_MAX  32
static udword hwiKeyTicks[HWI_SCRIPT_MAX];
static sdword hwiKeyTickN = 0, hwiKeyTickNext = 0;
static udword hwiConTicks[HWI_SCRIPT_MAX];
static char hwiConRow[HWI_SCRIPT_MAX][48], hwiConValue[HWI_SCRIPT_MAX][32];
static sdword hwiConN = 0, hwiConNext = 0;
static char hwiFeScreenName[64] = "";
static sdword hwiFeScreenTick = -1;
static bool32 hwiFeScreenDone = TRUE;

void hwiOverlayKeyAtSet(char *spec)                             // HWI-TEST
{
    char *p = spec;

    while (p != NULL && *p != '\0' && hwiKeyTickN < HWI_SCRIPT_MAX)
    {
        hwiKeyTicks[hwiKeyTickN++] = (udword)strtoul(p, &p, 10);
        p = (*p == ',') ? p + 1 : NULL;
    }
}

void hwiConsoleAtSet(char *spec)                                // HWI-TEST
{
    char *p = spec, *colon, *equals, *end;
    size_t n;

    while (p != NULL && *p != '\0' && hwiConN < HWI_SCRIPT_MAX)
    {
        hwiConTicks[hwiConN] = (udword)strtoul(p, &colon, 10);
        equals = (*colon == ':') ? strchr(colon, '=') : NULL;
        if (equals == NULL)
        {
            break;
        }
        end = strchr(equals, ',');
        n = (size_t)(equals - colon - 1);
        snprintf(hwiConRow[hwiConN], sizeof(hwiConRow[0]), "%.*s", (int)n, colon + 1);
        n = (end != NULL) ? (size_t)(end - equals - 1) : strlen(equals + 1);
        snprintf(hwiConValue[hwiConN], sizeof(hwiConValue[0]), "%.*s", (int)n, equals + 1);
        hwiConN++;
        p = (end != NULL) ? end + 1 : NULL;
    }
}

void hwiFeScreenSet(char *spec)                                 // HWI-TEST
{
    char *at = strchr(spec, '@');

    snprintf(hwiFeScreenName, sizeof(hwiFeScreenName), "%.*s", (int)((at != NULL) ? (size_t)(at - spec) : strlen(spec)), spec);
    hwiFeScreenTick = (at != NULL) ? (sdword)strtol(at + 1, NULL, 10) : -1;
    hwiFeScreenDone = FALSE;
}

static void hwiScriptKey(SDL_Scancode scancode)
{
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    event.type = SDL_KEYDOWN;
    event.key.keysym.scancode = scancode;
    event.key.repeat = 0;
    HandleEvent(&event);
    memset(&event, 0, sizeof(event));
    event.type = SDL_KEYUP;
    event.key.keysym.scancode = scancode;
    HandleEvent(&event);
}

static void hwiScriptFrame(void)
{
    udword tick = universe.univUpdateCounter;
    char buffer[512] = "", line[96], *reply = buffer;

    if (gameIsRunning)
    {
        while (hwiKeyTickNext < hwiKeyTickN && tick >= hwiKeyTicks[hwiKeyTickNext])
        {
            hwiScriptKey(HWI_OVERLAY_FKEY);
            printf("hwiscript: tick=%u key F1 (scancode %d) overlay=%d\n", (unsigned)tick, (int)HWI_OVERLAY_FKEY, (int)hwiOverlay);
            hwiKeyTickNext++;
        }
        while (hwiConNext < hwiConN && tick >= hwiConTicks[hwiConNext])
        {
            snprintf(line, sizeof(line), "%s %s", hwiConRow[hwiConNext], hwiConValue[hwiConNext]);
            cat_consoleRunCommand(hwiConRow[hwiConNext], hwiConValue[hwiConNext], &reply, line);
            printf("hwiscript: tick=%u console '%s'\n", (unsigned)tick, line);
            hwiConNext++;
        }
    }
    if (!hwiFeScreenDone && feStackIndex >= 0 && feStack[feStackIndex].screen != NULL &&
        ((hwiFeScreenTick < 0) ? (!gameIsRunning && strcmp(feStack[feStackIndex].screen->name, "Main_game_screen") == 0)
                               : (gameIsRunning && (sdword)tick >= hwiFeScreenTick)))
    {
        hwiFeScreenDone = TRUE;
        feScreenStart(ghMainRegion, hwiFeScreenName);
        printf("hwiscript: tick=%u screen %s pushed\n", (unsigned)tick, hwiFeScreenName);
    }
    else if (!hwiFeScreenDone && hwiFeScreenTick >= 0 && gameIsRunning && (sdword)tick >= hwiFeScreenTick)
    {
        hwiFeScreenDone = TRUE;
        feScreenStart(ghMainRegion, hwiFeScreenName);
        printf("hwiscript: tick=%u screen %s pushed\n", (unsigned)tick, hwiFeScreenName);
    }
    fflush(stdout);
}

void hwiFrameBegin(void)                                        // HWI-FEATURE(instrumentation)
{
    if (hwiFreq == 0)
    {
        hwiFreq = SDL_GetPerformanceFrequency();
    }
    hwiTop = SDL_GetPerformanceCounter();
    hwiFrameHadView = FALSE;
    hwiFrameVerts = hwiFrameObjects = 0;
    hwiScriptFrame();                               // HWI-TEST (M3-R3): /catOverlayKeyAt, /catConsoleAt, /catFeScreen
}

static void hwiHeap(sdword *used, sdword *pools, sdword *dupe)
{
    memHwiUsage(used, pools, dupe);
}

static void hwiLogWriteSecond(real64 seconds)
{
    sdword used = 0, pools = 0, dupe = -1;

    qsort(hwiFt, (size_t)hwiNFt, sizeof(real32), hwiCompare);
    qsort(hwiRnd, (size_t)hwiNRnd, sizeof(real32), hwiCompare);
    qsort(hwiTick, (size_t)hwiNTick, sizeof(real32), hwiCompare);
    qsort(hwiVerts, (size_t)hwiNVerts, sizeof(real32), hwiCompare);
    hwiShowMedian = hwiQuantile(hwiFt, hwiNFt, 50);
    hwiShowP99 = hwiQuantile(hwiFt, hwiNFt, 99);
    hwiShowSecond = TRUE;
    if (hwiLogFile == NULL)
    {
        return;
    }
    hwiHeap(&used, &pools, &dupe);
    if (hwiHeapStart < 0)
    {
        hwiHeapStart = used;
    }
    hwiLogSecond++;
    fprintf(hwiLogFile, "s=%d t=%.3f frames=%d ft_p50=%.3f ft_p95=%.3f ft_p99=%.3f ft_max=%.3f over=%d over1=%d over2=%d "
            "over3p=%d rnd_p50=%.3f rnd_max=%.3f ticks=%d tick_p50=%.3f tick_max=%.3f heap_used=%d heap_growth=%d "
            "growth_pools=%d dupe=%d verts_p50=%.0f verts_max=%.0f objects=%u\n",
            (int)hwiLogSecond, seconds, (int)hwiFramesSecond, (double)hwiQuantile(hwiFt, hwiNFt, 50),
            (double)hwiQuantile(hwiFt, hwiNFt, 95), (double)hwiQuantile(hwiFt, hwiNFt, 99),
            (double)((hwiNFt > 0) ? hwiFt[hwiNFt - 1] : 0.0f), (int)(hwiOver[1] + hwiOver[2] + hwiOver[3]),
            (int)hwiOver[1], (int)hwiOver[2], (int)hwiOver[3], (double)hwiQuantile(hwiRnd, hwiNRnd, 50),
            (double)((hwiNRnd > 0) ? hwiRnd[hwiNRnd - 1] : 0.0f), (int)hwiTicksSecond,
            (double)hwiQuantile(hwiTick, hwiNTick, 50), (double)((hwiNTick > 0) ? hwiTick[hwiNTick - 1] : 0.0f),
            (int)used, (int)(used - hwiHeapStart), (int)pools, (int)dupe,
            (double)hwiQuantile(hwiVerts, hwiNVerts, 50), (double)((hwiNVerts > 0) ? hwiVerts[hwiNVerts - 1] : 0.0f),
            (unsigned)hwiFrameObjects);
    // the second's frame periods as a sparse histogram (0.1 ms bins: "bin:count"), for the whole run's percentiles
    {
        sdword bin, first = TRUE;

        fprintf(hwiLogFile, "h s=%d", (int)hwiLogSecond);
        for (bin = 0; bin < HWI_HIST_BINS; bin++)
        {
            if (hwiHist[bin] > 0)
            {
                fprintf(hwiLogFile, "%s%d:%d", first ? " " : ",", (int)bin, (int)hwiHist[bin]);
                first = FALSE;
            }
        }
        fprintf(hwiLogFile, "\n");
    }
    fflush(hwiLogFile);
}

static void hwiLogHeader(void)
{
    int display;
    SDL_DisplayMode mode;
    udword fps = catMotionRealClockFps();

    if (fps > 0)
    {
        hwiRefreshHz = (real32)fps;
        hwiRefreshSource = "the --real-clock rate";
    }
    else if (sdlwindow != NULL && (display = SDL_GetWindowDisplayIndex(sdlwindow)) >= 0 &&
             SDL_GetCurrentDisplayMode(display, &mode) == 0 && mode.refresh_rate > 0)
    {
        hwiRefreshHz = (real32)mode.refresh_rate;
        hwiRefreshSource = "the display mode";
    }
    else
    {
        hwiRefreshHz = 144.0f;
        hwiRefreshSource = "default 144: no display refresh, no real clock";
    }
    fprintf(hwiLogFile, "# hwi-framelog v1 build=%s %s commit=%08x size=%dx%d logical=%dx%d refresh=%.0f (%s) "
            "heap=%d driver=%s\n", hwiBuildVersion(),     // [audit: was HOMEWORLD_BUILD_VERSION, "UNSUPPORTED_BUILD" here]
#ifdef HW_BUILD_FOR_DEBUGGING
            "debug",
#else
            "release",
#endif
#if HWI_ORACLE
            (unsigned)HWI_GIT_HASH,
#else
            0u,
#endif
            (int)MAIN_WindowWidthActual, (int)MAIN_WindowHeightActual, (int)MAIN_WindowWidth, (int)MAIN_WindowHeight,
            (double)hwiRefreshHz, hwiRefreshSource, (int)MEM_HeapSizeDefault,
            (SDL_GetCurrentVideoDriver() != NULL) ? SDL_GetCurrentVideoDriver() : "none");
    fprintf(hwiLogFile, "# settings: %s\n", hwiLine);
    fprintf(hwiLogFile, "# fields: s second, t seconds since the log began, ft frame period (ms, end of render task to "
            "end, the flip included), over frames longer than one refresh interval (over1 / over2 / over3p: 1, 2, 3+ "
            "refreshes late), rnd render task top to after the flip (ms, no glFinish), ticks universe ticks run, tick "
            "one universe task call (ms), heap_used main + growth heap bytes allocated (small-block heaps counted by "
            "block), heap_growth since the first second, growth_pools growth heaps made, dupe 'dupe trcolorinfo list' "
            "blocks (-1: the release build keeps no names), verts vertices of the levels drawn in the main view\n");
    fflush(hwiLogFile);
}

void hwiFrameEnd(void)                                          // HWI-FEATURE(instrumentation)
{
    Uint64 now = SDL_GetPerformanceCounter();
    real64 ft, rnd, interval;
    sdword late;

    if (hwiFreq == 0)
    {
        return;
    }
    hwiSettingsChanged("frame");                    // any change the hooks did not announce
    rnd = hwiMs(now - hwiTop);
    ft = (hwiLastEnd != 0) ? hwiMs(now - hwiLastEnd) : rnd;
    hwiLastEnd = now;
    hwiShowLast = (real32)ft;
    hwiShowAlpha = miRenderAlpha();
    if (hwiUnivFrameCalls > 0)
    {
        hwiUnivLastMs = hwiUnivFrameMs;
        hwiUnivLastCalls = hwiUnivFrameCalls;
    }
    hwiUnivFrameMs = 0.0;
    hwiUnivFrameCalls = 0;
    if (hwiLogFile != NULL && hwiLogStart == 0)
    {
        hwiLogStart = now;
        hwiSecondStart = now;
        hwiLogHeader();
        hwiNFt = hwiNRnd = hwiNTick = hwiNVerts = hwiFramesSecond = hwiTicksSecond = 0;
        hwiOver[1] = hwiOver[2] = hwiOver[3] = 0;
        memset(hwiHist, 0, sizeof(hwiHist));
        return;
    }
    if (!hwiActive())
    {
        hwiSecondStart = now;
        hwiNFt = hwiNRnd = hwiNTick = hwiNVerts = hwiFramesSecond = hwiTicksSecond = 0;
        hwiOver[1] = hwiOver[2] = hwiOver[3] = 0;
        memset(hwiHist, 0, sizeof(hwiHist));
        return;
    }
    if (hwiSecondStart == 0)
    {
        hwiSecondStart = now;
    }
    hwiFramesSecond++;
    if (hwiNFt < HWI_SAMPLES)
    {
        hwiFt[hwiNFt++] = (real32)ft;
        hwiRnd[hwiNRnd++] = (real32)rnd;
        if (hwiFrameHadView)
        {
            hwiVerts[hwiNVerts++] = (real32)hwiFrameVerts;
        }
    }
    hwiHist[(ft >= 100.0) ? (HWI_HIST_BINS - 1) : (sdword)(ft * 10.0)]++;
    interval = 1000.0 / (real64)hwiRefreshHz;
    late = (sdword)ceil(ft / interval - 0.05) - 1;  // refreshes beyond the first (5% for the clock's jitter)
    if (late > 0)
    {
        hwiOver[(late > 3) ? 3 : late]++;
    }
    if (hwiMs(now - hwiSecondStart) >= 1000.0)
    {
        hwiLogWriteSecond(hwiMs(now - hwiLogStart) / 1000.0);
        hwiSecondStart = now;
        hwiNFt = hwiNRnd = hwiNTick = hwiNVerts = hwiFramesSecond = hwiTicksSecond = 0;
        hwiOver[1] = hwiOver[2] = hwiOver[3] = 0;
        memset(hwiHist, 0, sizeof(hwiHist));
    }
}

/*=============================================================================
    The overlay: the top-left corner (the HUD's task bar and menus are at the bottom and the right; the mission
    timer, the RU count and the objectives draw elsewhere: docs/m3-r2-instrumentation.md "The corner"), logical
    coordinates (the 2D projection maps 0 .. MAIN_Window* onto the window: drawn at the UI scale), the default font.
=============================================================================*/
#define HWI_OVERLAY_X       4
#define HWI_OVERLAY_Y       4
#define HWI_OVERLAY_LINES   5

void hwiOverlayDraw(void)                                       // HWI-FEATURE(instrumentation)
{
    char text[HWI_OVERLAY_LINES][160];
    fonthandle old;
    rectangle box;
    sdword i, width = 0, height, w;
    color c = colRGB(255, 255, 160);

    if (!hwiOverlay || ghDefaultFont == 0)
    {
        return;
    }
    old = fontMakeCurrent(ghDefaultFont);
    if (hwiShowSecond)
    {
        snprintf(text[0], sizeof(text[0]), "FRAME %.2f MS  MED %.2f  P99 %.2f", (double)hwiShowLast,
                 (double)hwiShowMedian, (double)hwiShowP99);
    }
    else
    {
        snprintf(text[0], sizeof(text[0]), "FRAME %.2f MS  MED -  P99 -", (double)hwiShowLast);
    }
    if (hwiUnivLastMs >= 0.0)
    {
        snprintf(text[1], sizeof(text[1]), "TICK %.2f MS X%d  ALPHA %.3f", hwiUnivLastMs, (int)hwiUnivLastCalls,
                 (double)hwiShowAlpha);
    }
    else
    {
        snprintf(text[1], sizeof(text[1]), "TICK -  ALPHA %.3f", (double)hwiShowAlpha);
    }
    snprintf(text[2], sizeof(text[2]), "OBJ %d NIS %d LBB %d NOLOD %d SW %s", (int)(objInterpolate ? 1 : 0),
             (int)(nisInterpolate ? 1 : 0), (int)(letterboxBehind ? 1 : 0), (int)opNoLODVal, hwiSwitchText);
    if (hwiFrameHadView)
    {
        snprintf(text[3], sizeof(text[3]), "VERTS %u  OBJECTS %u", (unsigned)hwiFrameVerts, (unsigned)hwiFrameObjects);
    }
    else
    {
        snprintf(text[3], sizeof(text[3]), "VERTS -");
    }
    snprintf(text[4], sizeof(text[4]), "FULL DETAIL %s  UI %d%%", opNoLODVal ? "ON" : "OFF", (int)uiScalePercent);
    height = fontHeight(" ");
    for (i = 0; i < HWI_OVERLAY_LINES; i++)
    {
        w = fontWidth(text[i]);
        width = (w > width) ? w : width;
    }
    box.x0 = HWI_OVERLAY_X - 2;
    box.y0 = HWI_OVERLAY_Y - 2;
    box.x1 = HWI_OVERLAY_X + width + 2;
    box.y1 = HWI_OVERLAY_Y + height * HWI_OVERLAY_LINES + 2;
    primRectTranslucent2(&box, colRGBA(0, 0, 0, 160));
    for (i = 0; i < HWI_OVERLAY_LINES; i++)
    {
        fontPrint(HWI_OVERLAY_X, HWI_OVERLAY_Y + height * i, c, text[i]);
    }
    fontMakeCurrent(old);
}

/*=============================================================================
    HWI-TEST /frameLog <file>.
=============================================================================*/
void hwiFrameLogSet(char *file)                                 // HWI-TEST
{
    strncpy(hwiLogName, file, sizeof(hwiLogName) - 1);
    hwiLogName[sizeof(hwiLogName) - 1] = '\0';
    hwiLogFile = fopen(hwiLogName, "wt");
    if (hwiLogFile == NULL)
    {
        dbgMessagef("framelog: cannot open %s", hwiLogName);
        return;
    }
    atexit(hwiFrameLogClose);                       // the last line flushed whatever way the run ends
}

void hwiFrameLogClose(void)                                     // HWI-TEST
{
    if (hwiLogFile != NULL)
    {
        fprintf(hwiLogFile, "# end seconds=%d\n", (int)hwiLogSecond);
        fclose(hwiLogFile);
        hwiLogFile = NULL;
        printf("framelog: closed %s seconds=%d\n", hwiLogName, (int)hwiLogSecond);
    }
}
