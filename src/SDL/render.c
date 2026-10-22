/*=============================================================================
    Name    : render.c
    Purpose : Render module initialization and render a specific scene.

    Created 6/21/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "render.h"
#include "KASVar.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AIShip.h"
#include "Animatic.h"
#include "AutoLOD.h"
//#include "bink.h"
#include "BTG.h"
#include "CameraCommand.h"
#include "Console.h"          // CAT: catMotionCues (M2-R6 nis-fow-camera)
#include "Clipper.h"
#include "Clouds.h"
#include "Collision.h"
#include "CommandNetwork.h"
#include "Debug.h"
#include "Demo.h"
#include "Dock.h"
#include "FastMath.h"
#include "File.h"
#include "font.h"
#include "glinc.h"
#include "Globals.h"
#include "Gun.h"
#include "HS.h"
#include "Key.h"
#include "LagPrint.h"
#include "LaunchMgr.h"
#include "Light.h"
#include "main.h"
#include "mainrgn.h"
#include "Memory.h"
#include "Mesh.h"
#include "mouse.h"
#include "MotionTest.h"     // HWI-FEATURE(motion-interp): the measurement frame end
#include "TeamColourTest.h" // HWI-TEST (M3-R1 team-colours-warp)
#include "MotionInterp.h"   // HWI-FEATURE(motion-interp): miRenderBegin / miRenderEnd
#include "NavLights.h"
#include "Nebulae.h"
#include "NetCheck.h"
#include "NIS.h"
#include "Objectives.h"
#include "Particle.h"           // CAT: lens flare sources (catlensflaresource)
// CAT (M2-R5 render): the Cataclysm object-list renderer's helpers
#include "ACV.h"
#include "BeastWeapon.h"
#include "Blast.h"
#include "Cloak.h"
#include "CommandLayer.h"
#include "Dissolve.h"
#include "Dreadnought.h"
#include "FormationDefs.h"
#include "LOD.h"
#include "Mimic.h"
#include "ModuleShip.h"
#include "NomadMoon.h"
#include "RepairDroid.h"
#include "Salvage.h"
#include "Sentinel.h"
#include "ShipSelect.h"
#include "Attributes.h"
#include "Options.h"
#include "StatScript.h"
#define CAT_LF_TRANSPARENT_SHIPTYPE 0x90    // 0x0057bc41: a ship of this type does not block a lens flare
#include "PiePlate.h"
#include "prim2d.h"
#include "prim3d.h"
#include "ProfileTimers.h"
#include "ProximitySensor.h"
#include "rglu.h"
#include "Region.h"
#include "screenshot.h"
#include "Select.h"
#include "Shader.h"
#include "Sensors.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "StringsOnly.h"
#include "Subtitle.h"
#include "Switches.h"
#include "Tactical.h"
#include "Task.h"
#include "Teams.h"
#include "texreg.h"
#include "Tracking.h"
#include "Tutor.h"
#include "Tweak.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "utility.h"
#include "uiscale.h"         // HWI-FEATURE(ui-scale): the logical / physical sizes, the window fit
#include "Instrument.h"      // HWI-FEATURE(instrumentation): the overlay, the frame's clocks
#ifdef HW_ENABLE_GLES
#include "SDL_syswm.h"
#endif

#if defined _MSC_VER
	#define isnan(x) _isnan(x)
#endif

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
#endif


#define DEBUG_COLLISIONS            0
#define DEBUG_DISPLAY_LOD_SCALE     0
#define DEBUG_FONT_CHECK_SPECIAL    0     // special define for testing extended characters
#define DEBUG_RND_DOCK_LIGHT_TWEAK  0
#define DEBUG_VERBOSE_SHIP_STATS    0
#define DEBUG_VISIBLE_POLYS         0

#define SHOW_TRAIL_STATS      0
#define RND_WILL_PANIC        0
#define USE_RND_HINT          0
#define WILL_TWO_PASS         0
#define DISABLE_RANDOM_STARS  0     // turn off drawing of random stars over background


extern bool32 debugScreenshots;
extern bool32 hyperspaceOverride;

bool8 rndFogOn = FALSE;

#if RND_VISUALIZATION
extern bool32 dockLines;
extern bool32 gunLines;
extern bool8 RENDER_BOXES;
extern bool8 RENDER_LIGHTLINES;
#endif

extern bool8 g_ReplaceHack;
extern bool8 g_WireframeHack;
extern bool8 g_HiddenRemoval;

extern color HorseRaceDropoutColor;

vector g_RndPosition;

//CAT (M2-R6 startup-keys): the window's title is the exe's "Emergence" (WinMain 0x005631f0: CreateWindowExA with the
//     class and title 0x008b85b8 / 0x008b85c4; GoK "HomeworldSDL")
#define CAT_RND_WINDOW_TITLE        "Emergence"
bool32 rndTakeScreenshot = FALSE;

extern bool32 enableMSAA;

/*=============================================================================
    Private Types:
=============================================================================*/

/*=============================================================================
    Data:
=============================================================================*/
//this function pointer is what to call to render the main view
renderfunction rndMainViewRender = rndMainViewRenderFunction;

#if USE_RND_HINT
static sdword rndHint = 0;
#endif

SDL_Window *sdlwindow=NULL;
SDL_GLContext glcontext;
#ifdef HW_ENABLE_GLES
static EGLDisplay *egl_display = EGL_NO_DISPLAY;
static EGLSurface egl_surface = EGL_NO_SURFACE;
static EGLContext egl_context = EGL_NO_CONTEXT;
static EGLConfig egl_config;
#else

#ifndef __EMSCRIPTEN__
PFNGLBINDBUFFERPROC glBindBuffer = 0;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = 0;
PFNGLGENBUFFERSPROC glGenBuffers = 0;
PFNGLBUFFERDATAPROC glBufferData = 0;
PFNGLBUFFERSUBDATAPROC glBufferSubData = 0;
PFNGLTEXSTORAGE2DPROC glTexStorage2D = 0;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap = 0;
#endif
#endif

static char const* gl_extensions = 0;

static bool32 useVBO = FALSE;
static GLuint vboStars;

/* Should remove this stuff after cleaning up rgl functions. */
/*
HGLRC hGLRenderContext;
HDC hGLDeviceContext;
HWND hGLWindow;
*/
udword hGLRenderContext;
udword hGLDeviceContext;
udword hGLWindow;

udword meshRenders;

real32 rndAspectRatio;                                      //aspect ratio of rendering context

static sdword rndFillCounter = 0;
static color rndFillColour = 0;

GLfloat rndClearColor[4];

//callback functions optionally called during rendering a mission sphere
rendercallback rndPreObjectCallback = NULL, rndPostObjectCallback = NULL;

udword rndTextureEnviron = RTE_Modulate;

bool32 rndAdditiveBlending   = FALSE;
bool32 rndLightingEnabled    = TRUE;
bool32 rndNormalization      = FALSE;
bool32 rndScissorEnabled     = FALSE;
bool32 letterboxBehind       = FALSE;                // HWI-FEATURE(letterbox-setting): default 0, the exe's bars in front
static bool32 rndLbSwitched  = FALSE;                // HWI-FEATURE(letterbox-setting): /letterboxBehind stands for this run
static bool32 rndLbFileValue = FALSE;                // HWI-FEATURE(letterbox-setting): the options file's value under it
static bool32 rndLbRunValue  = FALSE;                // HWI-FEATURE(letterbox-setting): the run's value while the file is written
bool32 rndTextureEnabled     = FALSE;

//data for billboard geometry
hmatrix rndCameraMatrix;
hmatrix rndProjectionMatrix;
hvector rndBillboardCentre;
#if RND_ERROR_CHECKING
sdword rndBillboardEnabled = FALSE;
#endif


typedef struct c4ub_v3f_s
{
    ubyte  c[4];
    real32 v[3];
} c4ub_v3f;
c4ub_v3f* stararray = NULL;

//asteroid0 stuff
typedef struct asteroid0data
{
    color c;
    vector* position;
} asteroid0data;

#define ASTEROID0DATA_SIZE 80
asteroid0data asteroid0Data[ASTEROID0DATA_SIZE];


//frame counter
udword rndFrameCount;

//frame rate data and definitions
#if RND_FRAME_RATE
#define RND_FrameRatePeriod         2.0f
#define RND_FrameRateSeconds        2.0f
#define RND_FrameRateMaxRate        10
#define RND_FrameRateStack          1024
#define RND_FrameRateKey            NUMPAD7
udword rndFrameRateStart;
udword rndFrameRate;
udword rndFrameCountMax, rndFrameCountMin;
udword rndPrintCount;
sdword rndDisplayFrameRate = 0;
taskhandle rndFrameRateTaskHandle;
extern regionhandle ghMainRegion;
#endif //RND_FRAME_RATE
#if defined(COLLISION_CHECK_STATS) || defined(PROFILE_TIMERS)
sdword rndDisplayCollStats = 0;
#define RND_CollStatsKey            NUMPADSLASH
#endif

//polygon statistics measurements
#if RND_POLY_STATS
#define RND_PolyStatsPeriod         2.0f
#define RND_PolyStatsSeconds        2.0f
#define RND_PolyStatsStack          1024
#define RND_PolyStatsKey            NUMPAD8
sdword rndDisplayPolyStats = 0;
sdword rndNumberPolys;
sdword rndNumberTextured;
sdword rndNumberSmoothed;
sdword rndNumberDots;
sdword rndNumberLines;
taskhandle rndPolyStatsTaskHandle;
char rndPolyStatsString[256];
color rndPolyStatsColor = colReddish;
sdword rndPolyStatFrameCounter = 0;
#endif //RND_POLY_STATS

#if RND_XYZ
#define RND_XYZKey                  NUMPAD9
bool32 rndXYZPrint = FALSE;
#endif

//scaling minimum cap crap
char rndCapScaleCapStatsString[256];
#if RND_SCALECAP_TWEAK
#define scaleCapSlopeDelta  0.000001f                       //adjust the scaling cap
static sdword RND_CAPSCALECAP_STATS = 0;
static char scaleCapString[256] = "";
#endif //RND_SCALECAP_TWEAK

#define RND_FOV_TWEAK 0

real32 btgFieldOfView = 50.0f;

#if RND_FOV_TWEAK
#define btgFieldOfViewDelta 1.0f
static char fovString[256] = "";
#endif

#if DEBUG_VERBOSE_SHIP_STATS
static sdword shipsAtLOD[5];
static sdword polysAtLOD[5];
#endif

//data for debugging the GL state by dumping it's contents
#if RND_GL_STATE_DEBUG
typedef struct
{
    char *name;
    GLenum enumeration;
    bool32 bDefault;
}
enumentry;
#define enumEntry(string, n)            {string, n, FALSE}
#define enumDefaultEntry(string, n)     {string, n, TRUE}
#define enumEnd                         {NULL, 0, FALSE}
#define enumError                       {"ERROR", 0xffffff8, TRUE}
enumentry rndBoolEnums[] =
{
    enumEntry("GL_TRUE", GL_TRUE),
    enumDefaultEntry("GL_FALSE", GL_FALSE),
    enumEnd
};
enumentry rndAlphaTestFuncEnum[] =
{
    enumEntry("GL_NEVER", GL_NEVER),
    enumEntry("GL_LESS", GL_LESS),
    enumEntry("GL_EQUAL", GL_EQUAL),
    enumEntry("GL_LEQUAL", GL_LEQUAL),
    enumEntry("GL_GREATER", GL_GREATER),
    enumEntry("GL_NOTEQUAL", GL_NOTEQUAL),
    enumEntry("GL_GEQUAL", GL_GEQUAL),
    enumEntry("GL_ALWAYS", GL_ALWAYS),
    enumError,
    enumEnd
};
enumentry rndBlendFuncEnums[] =
{
    enumEntry("GL_ZERO", GL_ZERO),
    enumEntry("GL_ONE", GL_ONE),
    enumEntry("GL_DST_COLOR", GL_DST_COLOR),
    enumEntry("GL_ONE_MINUS_DST_COLOR", GL_ONE_MINUS_DST_COLOR),
    enumEntry("GL_SRC_ALPHA", GL_SRC_ALPHA),
    enumEntry("GL_ONE_MINUS_SRC_ALPHA", GL_ONE_MINUS_SRC_ALPHA),
    enumEntry("GL_DST_ALPHA", GL_DST_ALPHA),
    enumEntry("GL_ONE_MINUS_DST_ALPHA", GL_ONE_MINUS_DST_ALPHA),
    enumEntry("GL_SRC_ALPHA_SATURATE", GL_SRC_ALPHA_SATURATE),
    enumError,
    enumEnd
};
enumentry rndFaceEnums[] =
{
    enumEntry("GL_FRONT", GL_FRONT),
    enumEntry("GL_BACK", GL_BACK),
    enumEntry("GL_FRONT_AND_BACK", GL_FRONT_AND_BACK),
    enumError,
    enumEnd
};
enumentry rndMatrixEnums[] =
{
    enumEntry("GL_MODELVIEW", GL_MODELVIEW),
    enumEntry("GL_PROJECTION", GL_PROJECTION),
    enumEntry("GL_TEXTURE", GL_TEXTURE),
    enumError,
    enumEnd
};
enumentry rndHintEnums[] =
{
    enumEntry("GL_FASTEST", GL_FASTEST),
    enumEntry("GL_NICEST", GL_NICEST),
    enumEntry("GL_DONT_CARE", GL_DONT_CARE),
    enumError,
    enumEnd
};
enumentry rndShadeModelEnums[] =
{
    enumEntry("GL_FLAT", GL_FLAT),
    enumEntry("GL_SMOOTH", GL_SMOOTH),
    enumError,
    enumEnd
};

enumentry rndTextureEnvEnums[] =
{
    enumEntry("GL_MODULATE", GL_MODULATE),
    enumEntry("GL_DECAL", GL_DECAL),
    enumEntry("GL_BLEND", GL_BLEND),
    enumEntry("GL_REPLACE", GL_REPLACE),
    enumError,
    enumEnd
};

typedef struct
{
    char *heading;
    GLenum enumeration;
    sdword type;
    sdword nValues;
    enumentry *enumTable;
}
glstateentry;

#define stateEntry(string, enumb, type, nValues, table)  {string, enumb, type, nValues, table}
#define G_Bool                  0
#define G_GetBool               1
#define G_Integer               2
#define G_Float                 3
#define G_FloatByte             4
#define G_IntFunc               5

//definitions for special-case functions
#define G_TextureEnv            0

glstateentry rndStateSaveTable[] =
{
    stateEntry("GL_ALPHA_TEST", GL_ALPHA_TEST,                                  G_Bool, 1, rndBoolEnums),
    stateEntry("GL_BLEND", GL_BLEND,                                            G_Bool, 1, rndBoolEnums),
    stateEntry("GL_CULL_FACE", GL_CULL_FACE,                                    G_Bool, 1, rndBoolEnums),
    stateEntry("GL_DEPTH_TEST", GL_DEPTH_TEST,                                  G_Bool, 1, rndBoolEnums),
    stateEntry("GL_FOG", GL_FOG,                                                G_Bool, 1, rndBoolEnums),
    stateEntry("GL_LIGHT0", GL_LIGHT0,                                          G_Bool, 1, rndBoolEnums),
    stateEntry("GL_LIGHT1", GL_LIGHT1,                                          G_Bool, 1, rndBoolEnums),
    stateEntry("GL_LIGHTING", GL_LIGHTING,                                      G_Bool, 1, rndBoolEnums),
    stateEntry("GL_LINE_SMOOTH", GL_LINE_SMOOTH,                                G_Bool, 1, rndBoolEnums),
    stateEntry("GL_LINE_STIPPLE", GL_LINE_STIPPLE,                              G_Bool, 1, rndBoolEnums),
    stateEntry("GL_NORMALIZE", GL_NORMALIZE,                                    G_Bool, 1, rndBoolEnums),
    stateEntry("GL_POINT_SMOOTH", GL_POINT_SMOOTH,                              G_Bool, 1, rndBoolEnums),
    stateEntry("GL_POLYGON_SMOOTH", GL_POLYGON_SMOOTH,                          G_Bool, 1, rndBoolEnums),
    stateEntry("GL_POLYGON_STIPPLE", GL_POLYGON_STIPPLE,                        G_Bool, 1, rndBoolEnums),
    stateEntry("GL_SCISSOR_TEST", GL_SCISSOR_TEST,                              G_Bool, 1, rndBoolEnums),
    stateEntry("GL_TEXTURE_2D", GL_TEXTURE_2D,                                  G_Bool, 1, rndBoolEnums),

    //stateEntry("GL_TEXTURE_ENV", G_TextureEnv,                                  G_IntFunc,   1, rndTextureEnvEnums),
    //stateEntry("GL_TEXTURE_2D_BINDING", GL_TEXTURE_2D_BINDING,                  G_Integer,   1, NULL),
    stateEntry("GL_ALPHA_TEST_FUNC", GL_ALPHA_TEST_FUNC,                        G_Integer,   1, rndAlphaTestFuncEnum),
    stateEntry("GL_ALPHA_TEST_REF", GL_ALPHA_TEST_REF,                          G_Float,     1, NULL),
    stateEntry("GL_BLEND_DST", GL_BLEND_DST,                                    G_Integer,   1, rndBlendFuncEnums),
    stateEntry("GL_BLEND_SRC", GL_BLEND_SRC,                                    G_Integer,   1, rndBlendFuncEnums),
    stateEntry("GL_BLUE_BIAS", GL_BLUE_BIAS,                                    G_FloatByte, 1, NULL),
    stateEntry("GL_COLOR_CLEAR_VALUE", GL_COLOR_CLEAR_VALUE,                    G_Float,     4, NULL),
    stateEntry("GL_COLOR_MATERIAL_FACE", GL_COLOR_MATERIAL_FACE,                G_Integer,   1, rndFaceEnums),
    stateEntry("GL_CULL_FACE_MODE", GL_CULL_FACE_MODE,                          G_Integer,   1, rndFaceEnums),
    stateEntry("GL_CURRENT_COLOR", GL_CURRENT_COLOR,                            G_FloatByte, 4, NULL),
    stateEntry("GL_CURRENT_RASTER_COLOR", GL_CURRENT_RASTER_COLOR,              G_FloatByte, 4, NULL),
    stateEntry("GL_CURRENT_RASTER_POSITION", GL_CURRENT_RASTER_POSITION,        G_Float,     4, NULL),
    stateEntry("GL_CURRENT_TEXTURE_COORDS", GL_CURRENT_TEXTURE_COORDS,          G_Float,     4, NULL),
    stateEntry("GL_DEPTH_BITS", GL_DEPTH_BITS,                                  G_Integer,   1, NULL),
    stateEntry("GL_DEPTH_WRITEMASK", GL_DEPTH_WRITEMASK,                        G_GetBool,   1, rndBoolEnums),
    stateEntry("GL_FOG_COLOR", GL_FOG_COLOR,                                    G_FloatByte, 4, NULL),
    stateEntry("GL_FOG_DENSITY", GL_FOG_DENSITY,                                G_Float,     1, NULL),
    stateEntry("GL_GREEN_BIAS", GL_GREEN_BIAS,                                  G_FloatByte, 1, NULL),
    stateEntry("GL_LIGHT_MODEL_AMBIENT", GL_LIGHT_MODEL_AMBIENT,                G_FloatByte, 4, NULL),
    stateEntry("GL_LIGHT_MODEL_TWO_SIDE", GL_LIGHT_MODEL_TWO_SIDE,              G_GetBool,   1, rndBoolEnums),
    stateEntry("GL_LINE_WIDTH", GL_LINE_WIDTH,                                  G_Float,     1, NULL),
    stateEntry("GL_LINE_WIDTH_GRANULARITY", GL_LINE_WIDTH_GRANULARITY,          G_Float,     1, NULL),
    stateEntry("GL_MATRIX_MODE", GL_MATRIX_MODE,                                G_Integer,   1, rndMatrixEnums),
    stateEntry("GL_MAX_TEXTURE_SIZE", GL_MAX_TEXTURE_SIZE,                      G_Integer,   1, NULL),
    stateEntry("GL_MAX_VIEWPORT_DIMS", GL_MAX_VIEWPORT_DIMS,                    G_Integer,   2, NULL),
    stateEntry("GL_MODELVIEW_MATRIX", GL_MODELVIEW_MATRIX,                      G_Float,     16, NULL),
    stateEntry("GL_PERSPECTIVE_CORRECTION_HINT", GL_PERSPECTIVE_CORRECTION_HINT,G_Integer,   1, rndHintEnums),
    stateEntry("GL_POINT_SIZE", GL_POINT_SIZE,                                  G_Float,     1, NULL),
    stateEntry("GL_POINT_SIZE_GRANULARITY", GL_POINT_SIZE_GRANULARITY,          G_Float,     1, NULL),
    stateEntry("GL_POLYGON_MODE", GL_POLYGON_MODE,                              G_Integer,   1, rndFaceEnums),
    stateEntry("GL_PROJECTION_MATRIX", GL_PROJECTION_MATRIX,                    G_Float,     16, NULL),
    stateEntry("GL_RED_BIAS", GL_RED_BIAS,                                      G_FloatByte, 1, NULL),
    stateEntry("GL_SCISSOR_BOX", GL_SCISSOR_BOX,                                G_Integer,   4, NULL),
    stateEntry("GL_SHADE_MODEL", GL_SHADE_MODEL,                                G_Integer,   1, rndShadeModelEnums),
    stateEntry("GL_SUBPIXEL_BITS", GL_SUBPIXEL_BITS,                            G_Integer,   1, NULL),
    stateEntry("GL_VIEWPORT", GL_VIEWPORT,                                      G_Integer,   1, NULL),
    {NULL, 0, 0, 0, NULL}
};
bool32 rndGLStateSaving = FALSE;
char rndGLStateLogFileName[128];
sdword rndGLStateLogIndex = 0;
#endif //RND_GL_STATE_DEBUG

/*=============================================================================
    Functions:
=============================================================================*/
#if RND_GL_STATE_DEBUG

/*-----------------------------------------------------------------------------
    Name        : rndIntToString
    Description : Convert a GL enumeration to a string
    Inputs      : enumb - enumeration to convert
                  table - table of enumerations/strings
    Outputs     :
    Return      : name of enumeration
----------------------------------------------------------------------------*/
char *rndIntToString(GLenum enumb, enumentry *table)
{
    sdword index;

    for (index = 0; table[index].name != NULL; index++)
    {
        if (enumb == table[index].enumeration || table[index].bDefault)
        {
            return(table[index].name);
        }
    }
    dbgAssertOrIgnore(FALSE);
    return NULL;
}

/*-----------------------------------------------------------------------------
    Name        : rndGLStateLog
    Description : Log the state of the GL machine.
    Inputs      : location - user-specified name for where this function is called from
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
#define MAX_FLOATS          16
#define MAX_INTS            4
#define MAX_BOOLS           1
void rndGLStateLogFunction(char *location)
{
    sdword index, j;
    GLfloat floats[MAX_FLOATS];
    GLint ints[MAX_INTS];
    GLboolean bools[MAX_BOOLS];
    char totalString[256];
    char valueString[128];
    char *fileNameFull;
    FILE *f;

    fileNameFull = filePathPrepend(rndGLStateLogFileName, FF_UserSettingsPath);

    if (!fileMakeDestinationDirectory(fileNameFull))
    {
        dbgMessagef(
            "\nError creating directory for '%s' for GL state logging.",
            rndGLStateLogFileName);

        return;
    }

    f = fopen(fileNameFull, "at");
    if (f == NULL)
    {
        dbgMessagef("Error opening '%s' for GL state logging.", rndGLStateLogFileName);
        return;
    }

#if RND_GL_STATE_WINDOW
    dbgMessagef("******************** %s", location);
#endif
    fprintf(f, "******************** %s\n", location);
    for (index = 0; rndStateSaveTable[index].heading != NULL; index++)
    {
        sprintf(totalString, "%32s:", rndStateSaveTable[index].heading);
        switch (rndStateSaveTable[index].type)
        {
            case G_Bool:
                bools[0] = glIsEnabled(rndStateSaveTable[index].enumeration);
                for (j = 0; j < rndStateSaveTable[index].nValues; j++)
                {
                    if (rndStateSaveTable[index].enumTable != NULL)
                    {
                        sprintf(valueString, "%s", rndIntToString(bools[j], rndStateSaveTable[index].enumTable));
                    }
                    else
                    {
                        sprintf(valueString, "%d", bools[j]);
                    }
                    strcat(totalString, valueString);
                    if (j + 1 < rndStateSaveTable[index].nValues)
                    {
                        strcat(totalString, ", ");
                    }
                }
                break;
            case G_GetBool:
                glGetBooleanv(rndStateSaveTable[index].enumeration, bools);
                for (j = 0; j < rndStateSaveTable[index].nValues; j++)
                {
                    if (rndStateSaveTable[index].enumTable != NULL)
                    {
                        sprintf(valueString, "%s", rndIntToString(bools[j], rndStateSaveTable[index].enumTable));
                    }
                    else
                    {
                        sprintf(valueString, "%d", bools[j]);
                    }
                    strcat(totalString, valueString);
                    if (j + 1 < rndStateSaveTable[index].nValues)
                    {
                        strcat(totalString, ", ");
                    }
                }
                break;
            case G_Integer:
                glGetIntegerv(rndStateSaveTable[index].enumeration, ints);
                for (j = 0; j < rndStateSaveTable[index].nValues; j++)
                {
                    if (rndStateSaveTable[index].enumTable != NULL)
                    {
                        sprintf(valueString, "%s", rndIntToString(ints[j], rndStateSaveTable[index].enumTable));
                    }
                    else
                    {
                        sprintf(valueString, "%d", ints[j]);
                    }
                    strcat(totalString, valueString);
                    if (j + 1 < rndStateSaveTable[index].nValues)
                    {
                        strcat(totalString, ", ");
                    }
                }
                break;
            case G_Float:
                glGetFloatv(rndStateSaveTable[index].enumeration, floats);
                for (j = 0; j < rndStateSaveTable[index].nValues; j++)
                {
                    sprintf(valueString, "%.2f", floats[j]);
                    strcat(totalString, valueString);
                    if (j + 1 < rndStateSaveTable[index].nValues)
                    {
                        strcat(totalString, ", ");
                    }
                }
                break;
            case G_FloatByte:
                glGetFloatv(rndStateSaveTable[index].enumeration, floats);
                for (j = 0; j < rndStateSaveTable[index].nValues; j++)
                {
                    sprintf(valueString, "%d", colRealToUbyte(floats[j]));
                    strcat(totalString, valueString);
                    if (j + 1 < rndStateSaveTable[index].nValues)
                    {
                        strcat(totalString, ", ");
                    }
                }
                break;
            case G_IntFunc:
                switch (rndStateSaveTable[index].enumeration)
                {
                    case G_TextureEnv:
//                        glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, ints);
                        break;
                    default:
                        dbgAssertOrIgnore(FALSE);
                }
                for (j = 0; j < rndStateSaveTable[index].nValues; j++)
                {
                    if (rndStateSaveTable[index].enumTable != NULL)
                    {
                        sprintf(valueString, "%s", rndIntToString(ints[j], rndStateSaveTable[index].enumTable));
                    }
                    else
                    {
                        sprintf(valueString, "%d", ints[j]);
                    }
                    strcat(totalString, valueString);
                    if (j + 1 < rndStateSaveTable[index].nValues)
                    {
                        strcat(totalString, ", ");
                    }
                }
                break;
            default:
                dbgAssertOrIgnore(FALSE);
        }
#if RND_GL_STATE_WINDOW
        dbgMessagef("%s", totalString);
#endif
        fprintf(f, "%s\n", totalString);
    }
    fclose(f);
}

#endif //RND_GL_STATE_DEBUG
#if RND_FRAME_RATE
/*-----------------------------------------------------------------------------
    Name        : rndFrameRateTaskFunction
    Description : Task function for the frame rate
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
DEFINE_TASK(rndFrameRateTaskFunction)
{
    taskBegin;

    rndFrameRateStart = rndFrameRate = rndPrintCount = 0;   //it counters
    rndFrameCountMax = 0;                                   //set min/max counters
    rndFrameCountMin = SDWORD_Max;                          //to rediculous values

    taskYield(0);

    while (1)
    {
        rndFrameRate = rndFrameCount - rndFrameRateStart;   //number of frames since last printed
        rndFrameRateStart = rndFrameCount;                  //reset start frame
        rndFrameCountMax = max(rndFrameCountMax, rndFrameRate);//update min/max
        rndFrameCountMin = min(rndFrameCountMin, rndFrameRate);

        if (rndDisplayFrameRate)
        {                                                   //display frame rate
            dbgMessagef("Frame rate: %.2f <= %.2f <= %.2f",
                       (real32)(rndFrameCountMin) / RND_FrameRateSeconds,
                       (real32)(rndFrameRate) / RND_FrameRateSeconds,
                       (real32)(rndFrameCountMax) / RND_FrameRateSeconds);
        }
        rndPrintCount++;
        if (rndPrintCount >= RND_FrameRateMaxRate)
        {                                                   //if time to reset min/max
            rndPrintCount = 0;
            rndFrameCountMax = 0;                           //set min/max counters
            rndFrameCountMin = SDWORD_Max;                  //to rediculous values
        }
        taskYield(0);
    }
    taskEnd;
}

//toggle frame rate on/off
sdword rndFrameRateToggle(regionhandle region, sdword ID, udword event, udword data)
{
    rndDisplayFrameRate ^= TRUE;
    dbgMessagef("Frame rate printing %s", rndDisplayFrameRate ? "ON" : "OFF");
    return(0);
}

#endif //RND_FRAME_RATE

#if defined(COLLISION_CHECK_STATS) || defined(PROFILE_TIMERS)
udword rndCollStatsToggle(regionhandle region, sdword ID, udword event, udword data)
{
    rndDisplayCollStats ^= TRUE;
    dbgMessagef("Collision stats printing %s", rndDisplayCollStats ? "ON" : "OFF");
    return(0);
}
#endif //COLLISION_CHECK_STATS

#if RND_POLY_STATS
DEFINE_TASK(rndPolyStatsTaskFunction)
{
    taskBegin;

    rndNumberPolys    = 0;
    rndNumberTextured = 0;
    rndNumberSmoothed = 0;
    rndNumberDots     = 0;
    rndNumberLines    = 0;

    taskYield(0);

    while (1)
    {
        if (rndDisplayPolyStats && (rndPolyStatFrameCounter != 0 && rndNumberPolys != 0))
        {                                                   //display frame rate
            sprintf(rndPolyStatsString, "\n%d, (%.2f, %.2f), %d, %d",
                    rndNumberPolys / rndPolyStatFrameCounter,
                    100.0f * rndNumberTextured / rndNumberPolys,
                    100.0f * rndNumberSmoothed / rndNumberPolys, 10, 11);
            //print the poly count of the current LOD
            if (selSelected.numShips == 1)
            {
                static char polyCountString[256];
                static Ship *ship;
                static lod *lodInfo;
                static sdword index, polyCount, vertexCount, lodIndex;
                static meshdata *mesh;

                ship = selSelected.ShipPtr[0];
                if (lodTuningMode)
                {
                    lodIndex = rndLOD;
                }
                else
                {
                    lodIndex = ship->obj.currentLOD;
                }
                if (lodIndex < ship->staticinfo->common.LOD->nLevels)
                {
                    lodInfo = &ship->staticinfo->common.LOD->level[lodIndex];
                    if ((lodInfo->flags & LM_LODType) == LT_Mesh)
                    {                                       //if it's a mesh type lod
                        mesh = (meshdata *)lodInfo->pData;  //get the mesh pointer
                        for (index = vertexCount = polyCount = 0; index < mesh->nPolygonObjects; index++)
                        {                                   //count vertices and polygons
                            polyCount += mesh->object[index].nPolygons;
                            vertexCount += mesh->object[index].nVertices;
                        }
                        sprintf(polyCountString, ", %d, %d, %d", lodIndex, vertexCount, polyCount);
                        strcat(rndPolyStatsString, polyCountString);
                    }
                }
            }
            dbgMessage(rndPolyStatsString);
#if MESH_MATERIAL_STATS
            sprintf(meshMaterialStatsString, "\n%.2f / %.2f / %.2f",
                    (real32)meshMaterialChanges / (real32)rndPolyStatFrameCounter,
                    (real32)meshTotalMaterials / (real32)rndPolyStatFrameCounter,
                    (real32)meshMaterialChanges / (real32)meshTotalMaterials);
            dbgMessage(meshMaterialStatsString);
#endif //MESH_MATERIAL_STATS
        }
        rndPrintCount++;
        rndPolyStatsColor = colReddish;
        rndNumberPolys    = 0;
        rndNumberTextured = 0;
        rndNumberSmoothed = 0;
        rndNumberDots     = 0;
        rndNumberLines    = 0;
        rndPolyStatFrameCounter = 0;
#if MESH_MATERIAL_STATS
        meshMaterialChanges = 0;
        meshTotalMaterials = 0;
#endif //MESH_MATERIAL_STATS
        taskYield(0);
    }
    taskEnd;
}

//toggle frame rate on/off
sdword rndPolyStatsToggle(regionhandle region, sdword ID, udword event, udword data)
{
    rndDisplayPolyStats ^= TRUE;
    dbgMessagef("Frame rate printing %s", rndDisplayPolyStats ? "ON" : "OFF");
    if (rndDisplayPolyStats)
    {
        strcpy(rndPolyStatsString, "\nnPolys (%textured, %smoothed), nDots, nLines, shipLod, nVertices, nPolys");
        dbgMessage(rndPolyStatsString);
        rndPolyStatsColor = colWhite;
#if MESH_MATERIAL_STATS
        strcpy(meshMaterialStatsString, "\nnMaterialChanges / nMaterials / factor");
        dbgMessage(meshMaterialStatsString);
#endif //MESH_MATERIAL_STATS
    }
    return(0);
}

#endif //RND_POLY_STATS

static real32 scalar;
void rndCapScaleCapStatsTaskFunction(void)
{
    if (selSelected.numShips == 1)
    {
        Ship* ship = selSelected.ShipPtr[0];

        if (ship->trail[0] == NULL)
        {
            scalar = 0.0f;
        }
        else
        {
            scalar = ship->trail[0]->scalecap;
            if (scalar == -1.0f)
            {
                scalar = 0.0f;
            }
        }
    }
    else
    {
        scalar = 0.0f;
    }

    sprintf(rndCapScaleCapStatsString, "\n trailScaleCap %f", scalar);
}

/*bool32 setupPixelFormat(HDC hDC)*/
bool32 setupPixelFormat()
{
	Uint32 flags;
	static Uint32 lastWidth  = 0;
	static Uint32 lastHeight = 0;
	static Uint32 lastDepth  = 0;
	static bool32   lastFull   = FALSE;
	int MSAA = 0;
    extern bool32 showBorder;                               // HWI-FIX: /noBorder (main.c)
    sdword reqW = MAIN_WindowWidth, reqH = MAIN_WindowHeight;   // HWI-FEATURE(ui-scale): the window request
    sdword createW = MAIN_WindowWidth, createH = MAIN_WindowHeight;  // HWI-FIX: the size created (the fit)
    sdword createX = SDL_WINDOWPOS_UNDEFINED, createY = SDL_WINDOWPOS_UNDEFINED;   // HWI-FIX: on the display it fits
#ifdef HW_ENABLE_GLES
    SDL_SysWMinfo info;
    EGLint num_config = 1;
    EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_DEPTH_SIZE, 16,
        EGL_NONE
    };
    EGLContext new_context = EGL_NO_CONTEXT;
    EGLSurface new_surface = EGL_NO_SURFACE;
#endif

    // HWI-FEATURE(ui-scale): once the window exists MAIN_Window* hold its logical size; the request is the window's own
    (void)uiScaleWindowCurrent(&reqW, &reqH);               // HWI-FEATURE(ui-scale)

    // don't bother doing anything if nothing's actually changed
	if(lastWidth  == (Uint32)reqW                           // HWI-FEATURE(ui-scale): the request, not MAIN_Window*
    && lastHeight == (Uint32)reqH                           // HWI-FEATURE(ui-scale)
    && lastDepth  == MAIN_WindowDepth
    && lastFull   == fullScreen)
    {
		return TRUE;
    }

	/* SDL video initialized? */
	flags = SDL_WasInit(SDL_INIT_EVERYTHING);
	if (!flags)
	{
		if (SDL_Init(SDL_INIT_VIDEO) == -1)
			return FALSE;
	}
	else if (!(flags & SDL_INIT_VIDEO))
	{
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) == -1)
			return FALSE;
	}

    /* Create OpenGL window. */
    flags = SDL_WINDOW_OPENGL;

#ifdef _WIN32
    HRESULT hr = SetProcessDPIAware();
#endif

#ifndef HW_ENABLE_GLES
    /* Set attributes. */
    SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE,  MAIN_WindowDepth);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#endif

#ifndef __EMSCRIPTEN__
    if (/* main */ fullScreen)
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    // HWI-FIX (M3-R1 ui-scale, the user's item 6): a windowed request as large as the desktop is fitted to it
    //      (uiscale.c uiScaleWindowFit: the display's size -> fullscreen desktop; larger than the usable bounds ->
    //      shrunk, aspect kept); the exe's windowed mode puts the requested client size at the screen's origin
    //      whatever the desktop (WinMain 0x005631f0)
    uiScaleWindowFit(reqW, reqH, fullScreen, showBorder, (udword *)&flags, &createW, &createH, &createX, &createY);    // HWI-FIX
#else
    flags |= SDL_WINDOW_RESIZABLE;
#endif

#ifdef HW_ENABLE_GLES
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    if (!(sdlwindow=SDL_CreateWindow(CAT_RND_WINDOW_TITLE, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        MAIN_WindowWidth, MAIN_WindowHeight, flags)))
        return FALSE;

    if(!(glcontext = SDL_GL_CreateContext(sdlwindow)))
        return FALSE;

    SDL_VERSION(&info.version);
    if (SDL_GetWMInfo(&info) != 1) {
        fprintf(stderr, "EGL cannot use this SDL version\n");
        return FALSE;
    }

    egl_display = eglGetDisplay((EGLNativeDisplayType)info.info.x11.gfxdisplay);
    if (egl_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "EGL found no available displays\n");
        return FALSE;
    }

    if (!eglInitialize(egl_display, NULL, NULL)) {
        fprintf(stderr, "EGL failed to initialize: code 0x%x\n", eglGetError());
        return FALSE;
    }

    if (!eglChooseConfig(egl_display, attribs, &egl_config, 1, &num_config) || num_config < 1) {
        fprintf(stderr, "EGL failed to find any valid config with required attributes: code 0x%x\n", eglGetError());
        return FALSE;
    }

    new_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, NULL);
    if (new_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "EGL failed to create context: code 0x%x\n", eglGetError());
        return FALSE;
    }
    
    new_surface = eglCreateWindowSurface(egl_display, egl_config, (EGLNativeWindowType)info.info.x11.window, NULL);
    if (new_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "EGL failed to create a window surface: 0x%x\n", eglGetError());
        return FALSE;
    }

    if (!eglMakeCurrent(egl_display, new_surface, new_surface, new_context)) {
        fprintf(stderr, "EGL failed to change current surface: 0x%x\n", eglGetError());
        return FALSE;
    }

    if (egl_context != EGL_NO_CONTEXT) {
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(egl_display, egl_context);
        eglDestroySurface(egl_display, egl_surface);
    }
    egl_context = new_context;
    egl_surface = new_surface;
#else
	if (enableMSAA) {
        MSAA = 8;
	    SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
	    SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, MSAA );
	}

	while (!(sdlwindow=SDL_CreateWindow(CAT_RND_WINDOW_TITLE, createX, createY,     // HWI-FIX
		createW, createH, flags)))                          // HWI-FIX: the fitted size (= the request but at the desktop's limits)
	{
        if (MSAA < 2)
        {
            SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 0 );
            SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 0 );

            if (!(sdlwindow=SDL_CreateWindow(CAT_RND_WINDOW_TITLE, createX, createY,     // HWI-FIX
                createW, createH, flags)))                  // HWI-FIX
            {
            fprintf (stderr, "Couldn't set video mode: %s\n", SDL_GetError ());
            //exit (1);
                    return FALSE;
            }
        }

	    fprintf (stderr, "Couldn't set %dx MSAA video mode: %s\n", MSAA, SDL_GetError ());

        MSAA /= 2;
        
        SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
        SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, MSAA );
	}

    if(!(glcontext = SDL_GL_CreateContext(sdlwindow)))
        return FALSE;

    uiScaleWindowPlace(sdlwindow);                          // HWI-FIX: a shrunk window less its borders, in the usable bounds
#endif

    SDL_GL_SetSwapInterval(1);

    SDL_GL_MakeCurrent(sdlwindow, glcontext);

	SDL_ShowCursor(SDL_DISABLE);

    // query actual resolution we render into (e.g. in fullscreen desktop mode it may be different from requested)
    SDL_GL_GetDrawableSize(sdlwindow, &MAIN_WindowWidthActual, &MAIN_WindowHeightActual);
    // HWI-FEATURE(ui-scale): the drawable is the physical size; MAIN_Window* become the logical size physical / scale
    uiScaleWindowCreated(sdlwindow, reqW, reqH);            // HWI-FEATURE(ui-scale)
    printf("UI resolution: %dx%d\n", MAIN_WindowWidth, MAIN_WindowHeight);
    printf("Render resolution: %dx%d\n", MAIN_WindowWidthActual, MAIN_WindowHeightActual);

	lastWidth  = (Uint32)reqW;                              // HWI-FEATURE(ui-scale): the request
	lastHeight = (Uint32)reqH;                              // HWI-FEATURE(ui-scale)
	lastDepth  = MAIN_WindowDepth;
	lastFull   = fullScreen;

#ifndef __EMSCRIPTEN__
    glBindBuffer = SDL_GL_GetProcAddress("glBindBuffer");
    glDeleteBuffers = SDL_GL_GetProcAddress("glDeleteBuffers");
    glGenBuffers = SDL_GL_GetProcAddress("glGenBuffers");
    glBufferData = SDL_GL_GetProcAddress("glBufferData");
    glBufferSubData = SDL_GL_GetProcAddress("glBufferSubData");
    glTexStorage2D = SDL_GL_GetProcAddress("glTexStorage2D");
    glGenerateMipmap = SDL_GL_GetProcAddress("glGenerateMipmap");
#endif

    gl_extensions = glGetString(GL_EXTENSIONS);
    printf("GL Extensions:\n%s\n", gl_extensions);

    useVBO = glCheckExtension("GL_ARB_vertex_buffer_object");

	return TRUE;
}

int glCheckExtension(char const* ext) {
        bool32 gotext = gl_extensions && strstr(gl_extensions, ext) != NULL;
        if (strcmp(ext, "GL_ARB_vertex_buffer_object") == 0) {
#ifdef HW_ENABLE_GLES
        /* part of the standard in GLES */
        return 1;
#else
        return gotext && glBindBuffer && glDeleteBuffers && glGenBuffers && glBufferData && glBufferSubData;
#endif
        } else if (strcmp(ext, "GL_OES_draw_texture") == 0) {
        return gotext;
        }
    return gotext;
}

/*bool32 setupPalette(HDC hDC)*/
bool32 setupPalette()
{
	int pix_size;

#ifndef HW_ENABLE_GLES
	if (SDL_GL_GetAttribute(SDL_GL_BUFFER_SIZE, &pix_size) == -1)
	{
#endif
		/* Hoping it will work... */
		pix_size = MAIN_WindowDepth;
#ifndef HW_ENABLE_GLES
	}
#endif
	if (pix_size >= 15)
		return TRUE;

	dbgMessage("rndInit: needs paletted display");
	return FALSE;
}

typedef int (*AUXINITPOSITIONPROC)(GLuint, GLuint, GLuint, GLuint, GLuint);

sdword rndSmallInit(rndinitdata* initData, bool32 GL)
{
    Uint32 flags;

    /*
    hGLWindow = initData->hWnd;
    hGLDeviceContext = GetDC(initData->hWnd);
    */
    /*if (!setupPixelFormat(hGLDeviceContext))*/
    if (!setupPixelFormat())
    {
        return FALSE;
    }
    /*if (!setupPalette(hGLDeviceContext))*/
    if (!setupPalette())
    {
        /* Kill the window we created. */
        flags = SDL_WasInit(SDL_INIT_EVERYTHING);
#ifdef HW_ENABLE_GLES
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(egl_display, egl_context);
        egl_config = EGL_NO_CONTEXT;
        eglDestroySurface(egl_display, egl_surface);
        egl_surface = EGL_NO_SURFACE;
        eglTerminate(egl_display);
        egl_display = EGL_NO_DISPLAY;
#endif
        if (flags & ~SDL_INIT_VIDEO)
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        else
            SDL_Quit();
        return FALSE;
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : rndInit
    Description : Initialize the rendering system.
    Inputs      : initData - pointer to a structure containing initialization
                    info.
    Outputs     : ??
    Return      : ERROR if an error occurred.
----------------------------------------------------------------------------*/
static LinkedList catRndFieldList;               // CAT: below (the Sentinel fields)
sdword rndInit(rndinitdata *initData)
{
    static GLfloat  ambientProperties[] = {0.9f, 0.5f, 0.5f, 1.0f};
    static GLfloat  diffuseProperties[] = {0.8f, 0.8f, 0.8f, 1.0f};
    static GLfloat  specularProperties[] = {1.0f, 1.0f, 1.0f, 1.0f};

    if (!setupPixelFormat())
    {
        dbgMessage("rndInit: GL couldn't setupPixelFormat");
        return !OKAY;
    }
    if (!setupPalette())
    {
        dbgMessage("rndInit: GL couldn't setupPalette");
        return !OKAY;
    }

#if RND_VERBOSE_LEVEL >= 1
    dbgMessagef("rndInit: OpenGL Vendor     :%s", glGetString(GL_VENDOR));
    dbgMessagef("rndInit: OpenGL Renderer   :%s", glGetString(GL_RENDERER));
    dbgMessagef("rndInit: OpenGL Version    :%s", glGetString(GL_VERSION));
    dbgMessagef("rndInit: OpenGL Extensions :%s", glGetString(GL_EXTENSIONS));
#endif
    rndSetClearColor(colRGBA(0,0,0,255));
#ifdef HW_ENABLE_GLES
    glClearDepthf( 1.0f );
#else
    glClearDepth( 1.0 );
#endif

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    rndAdditiveBlending = FALSE;

    glEnable(GL_DEPTH_TEST);

    glEnable(GL_LIGHTING);

    glLightfv( GL_LIGHT0, GL_AMBIENT, ambientProperties);
    glLightfv( GL_LIGHT0, GL_DIFFUSE, diffuseProperties);
    glLightfv( GL_LIGHT0, GL_SPECULAR, specularProperties);
    glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, 1.0f);
//    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientProperties);

    glEnable( GL_LIGHT0 );

//    glShadeModel(GL_FLAT);

    glCullFace(GL_BACK);                                    //enable culling of back-facing polys
    glEnable(GL_CULL_FACE);

    rndPreObjectCallback = rndPostObjectCallback = NULL;    //no render callbacks yet

    rndAspectRatio = (GLfloat)initData->width / (GLfloat)initData->height;
    if (MAIN_WindowWidthActual > 0 && MAIN_WindowHeightActual > 0)  // HWI-FEATURE(ui-scale): the 3D projection's aspect is
    {                                                               // HWI-FEATURE(ui-scale): the physical size's (the
        rndAspectRatio = (GLfloat)MAIN_WindowWidthActual / (GLfloat)MAIN_WindowHeightActual;   // HWI-FEATURE(ui-scale):
    }                                                               // HWI-FEATURE(ui-scale): logical one is rounded)

    rndFrameCount = 0;

#if RND_FRAME_RATE
    rndFrameRateTaskHandle = taskStart(rndFrameRateTaskFunction, RND_FrameRatePeriod, 0);//start frame rate task
    regKeyChildAlloc(ghMainRegion, RND_FrameRateKey, RPE_KeyDown, (regionfunction) rndFrameRateToggle, 1, RND_FrameRateKey);
#endif//RND_FRAME_RATE
#if defined(COLLISION_CHECK_STATS) || defined(PROFILE_TIMERS)
    regKeyChildAlloc(ghMainRegion, RND_CollStatsKey, RPE_KeyDown, (regionfunction) rndCollStatsToggle, 1, RND_CollStatsKey);
#endif
#if RND_POLY_STATS
    rndPolyStatsTaskHandle = taskStart(rndPolyStatsTaskFunction, RND_PolyStatsPeriod, 0);//start frame rate task
    regKeyChildAlloc(ghMainRegion, RND_PolyStatsKey, RPE_KeyDown, (regionfunction) rndPolyStatsToggle, 1, RND_PolyStatsKey);
#endif //RND_POLY_STATS

    glDepthFunc(GL_LEQUAL);

    // CAT (M2-R5 render): the lens flare source list and the Sentinel field queue start empty (rndInit 0x005747d0 at
    //      0x00574a05: listInit 0x00b37880, 0x00b378a0)
    listInit(&catRndLensFlareSourceList);
    listInit(&catRndFieldList);

    return(OKAY);
}

/*-----------------------------------------------------------------------------
    Name        : rndClose
    Description : Closes the rendering module.
    Inputs      : void
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
void rndClose(void)
{
    Uint32 flags = SDL_WasInit(SDL_INIT_EVERYTHING);
    if (!(flags & SDL_INIT_VIDEO))
        return;
    if (stararray) {
        if (useVBO)
            glDeleteBuffers(1, &vboStars);
        else
            memFree(stararray);
    }
#ifdef HW_ENABLE_GLES
    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(egl_display, egl_context);
    egl_config = EGL_NO_CONTEXT;
    eglDestroySurface(egl_display, egl_surface);
    egl_surface = EGL_NO_SURFACE;
    eglTerminate(egl_display);
    egl_display = EGL_NO_DISPLAY;
#endif
    if (flags ^ SDL_INIT_VIDEO)
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    else
        SDL_Quit();
}

/*-----------------------------------------------------------------------------
    Name        : rndBillboardEnable
    Description : Enable billboard mode
    Inputs      : centre - location of the centre or 'pivot' point of the
                    billboard geometry.
    Outputs     : Loads identity for modelview matrix and translates by
                    (cameramatrix * centre) which is stored in rndBillboardCentre.
    Return      : void
    Note        : Billboard geometry should be drawn on the z = 0 plane
                    about x = y = 0.
----------------------------------------------------------------------------*/
void rndBillboardEnable(vector *centre)
{
    hvector hCentre;

#if RND_ERROR_CHECKING
    if (rndBillboardEnabled)
    {
        dbgFatal(DBG_Loc, "Billboarding already enabled");
    }
    rndBillboardEnabled = TRUE;
#endif
    glLoadIdentity();
    hCentre.x = centre->x;
    hCentre.y = centre->y;
    hCentre.z = centre->z;
    hCentre.w = 1.0f;
    hmatMultiplyHMatByHVec(&rndBillboardCentre, &rndCameraMatrix, &hCentre);
    glTranslatef(rndBillboardCentre.x, rndBillboardCentre.y, rndBillboardCentre.z);
}

/*-----------------------------------------------------------------------------
    Name        : rndBillboardDisable
    Description : Disable billboard mode.
    Inputs      : void
    Outputs     : Loads rndCameraMatrix into modelview (should only be 1
                    level on the model-view matrix stack).
    Return      : void
----------------------------------------------------------------------------*/
void rndBillboardDisable(void)
{
#if RND_ERROR_CHECKING
    if (rndBillboardEnabled == FALSE)
    {
        dbgFatal(DBG_Loc, "Billboarding already disabled.");
    }
    rndBillboardEnabled = FALSE;
#endif
    glLoadMatrixf((GLfloat *)(&rndCameraMatrix));    //restore previous camera matrix
}

void rndFilter(bool32 on)
{
}

static real64 rndNear(real64 in)
{
    return in;
}

/*-----------------------------------------------------------------------------
    Name        : rndBackgroundRender
    Description : Render the background of a mission sphere
    Inputs      : radius - radius of mission sphere
                  galaxy - pointer to galaxyinfo structure describing background
    Outputs     : renders each segment as a quadrangle about the mission sphere
    Return      : void
----------------------------------------------------------------------------*/
void rndBackgroundRender(real32 radius, Camera* camera, bool32 bDrawStars)
{
    real32 projection[16];
    sdword index;

#if DISABLE_RANDOM_STARS
    bDrawStars = FALSE;
#endif

    glGetFloatv(GL_PROJECTION_MATRIX, projection);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    rgluPerspective(btgFieldOfView, rndAspectRatio,     //set projection matrix
                    rndNear(camera->clipPlaneNear), camera->clipPlaneFar);
    glMatrixMode(GL_MODELVIEW);

    if (showBackgrounds && gameIsRunning && catRndDrawBTG)
    {                                                       // CAT: and the console's "BTG" switch (0x0087816e)
        btgRender();
    }

    //draw the stars in the mission sphere
    trClearCurrent();
    glBindTexture(GL_TEXTURE_2D, 0);
    rndTextureEnable(FALSE);
    if (bDrawStars && gameIsRunning)
    {
        bool32 blends, pointSize;

        blends = TRUE;
        pointSize = TRUE;

        glPointSize(1.0f);

        if (blends)
        {
            glEnable(GL_POINT_SMOOTH);
            glEnable(GL_BLEND);
            //additively blend only if background are enabled
            rndAdditiveBlends(showBackgrounds);
            if (pointSize && (MAIN_WindowWidthActual > 1024))   // HWI-FEATURE(ui-scale): points are physical pixels
            {
                glPointSize(1.2f);
            }
        }

        //need to reset the projection matrix
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        rgluPerspective(btgFieldOfView, rndAspectRatio,
                        rndNear(camera->clipPlaneNear), camera->clipPlaneFar);

        //draw small stars
        glEnableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_VERTEX_ARRAY);
        if (useVBO) {
            glBindBuffer(GL_ARRAY_BUFFER, vboStars);
            glColorPointer(4, GL_UNSIGNED_BYTE, 16, 0);
            glVertexPointer(3, GL_FLOAT, 16, (GLubyte*)4);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        } else {
            glColorPointer(4, GL_UNSIGNED_BYTE, 16, (GLubyte*)stararray);
            glVertexPointer(3, GL_FLOAT, 16, ((GLubyte*)stararray) + 4);
        }
        glDrawArrays(GL_POINTS, 0, universe.star3dinfo->Num3dStars - NUM_BIG_STARS);

        if (blends && pointSize)
        {
            switch (MAIN_WindowWidthActual)                     // HWI-FEATURE(ui-scale): the physical width (100%: the same)
            {
            case 640:
            case 800:
                glPointSize(2.1f);
                break;
            case 1024:
                glPointSize(2.5f);
                break;
            case 1280:
                glPointSize(2.65f);
                break;
            default:
                glPointSize(2.8f);
            }
        }

        //draw big stars
        glDrawArrays(GL_POINTS, (universe.star3dinfo->Num3dStars - NUM_BIG_STARS), NUM_BIG_STARS);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);

        if (blends)
        {
            glDisable(GL_BLEND);
            glDisable(GL_POINT_SMOOTH);
            if (pointSize)
            {
                glPointSize(1.0f);
            }
        }
    }

    rndAdditiveBlends(FALSE);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projection);
    glMatrixMode(GL_MODELVIEW);
}

/*-----------------------------------------------------------------------------
    Name        : rndCameraInside
    Description : decides whether the camera is inside a ship or not
    Inputs      : spaceobj - the object to test
                  camera - the current camera object
    Outputs     :
    Return      : TRUE if camera is inside, FALSE otherwise
----------------------------------------------------------------------------*/
bool32 rndCameraInside(SpaceObj* spaceobj, Camera* camera)
{
    if ((spaceobj->header.flags & SOF_Rotatable) && (spaceobj->header.flags & SOF_Impactable))
    {
        vector dist;
        real32 collspheresize;

        collspheresize = spaceobj->staticinfo->staticCollInfo.collspheresize;
        collspheresize *= collspheresize;

        //sphere test
        // CAT (M2-R5 render): a ship's sphere is centred on its collision position (rndCameraInside 0x00574f80 at
        //      0x00574fa7: +0xE0), other objects' on their position
        if (spaceobj->header.objtype == OBJ_ShipType)
        {
            vecSub(dist, ((SpaceObjRotImp *)spaceobj)->collInfo.collPosition, camera->eyeposition);
        }
        else
        {
            vecSub(dist, spaceobj->posinfo.position, camera->eyeposition);
        }
        if (vecMagnitudeSquared(dist) > collspheresize)
        {
            //further away than the bounding sphere, trivially reject
            return FALSE;
        }

        //rect test (final)
        return collCheckRectPoint((SpaceObjRotImp*)spaceobj, &camera->eyeposition);
    }
    else
    {
        return FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : rndInsideShip
    Description : determines whether the camera is inside a ship
    Inputs      : spaceobj - the object to consider
                  camera - current camera object
    Outputs     :
    Return      : TRUE if camera inside, else FALSE
----------------------------------------------------------------------------*/
bool32 rndInsideShip(SpaceObj* spaceobj, Camera* camera)
{
    if (nisIsRunning)
    {
        return FALSE;
    }
    if (bitTest(universe.mainCameraCommand.ccMode,CCMODE_PILOT_SHIP))
    {
        return FALSE;
    }
    return rndCameraInside(spaceobj, camera);
}

/*-----------------------------------------------------------------------------
    Name        : rndShipVisible
    Description : determines if a SpaceObj is within the viewing frustum via
                  a few different means
    Inputs      : spaceobj - the spaceobject to consider
                  camera - current camera object
    Outputs     :
    Return      : TRUE if ship is visible, else FALSE
----------------------------------------------------------------------------*/
bool32 rndShipVisible(SpaceObj* spaceobj, Camera* camera)
{
    vector veye, vobj;

    vecSub(veye, camera->eyeposition, camera->lookatpoint);
    vecSub(vobj, camera->eyeposition, spaceobj->posinfo.position);

    if ((spaceobj->header.flags & SOF_Rotatable) && (spaceobj->header.flags & SOF_Impactable))
    {
        //bbox frustum culling
        StaticCollInfo* sinfo = &spaceobj->staticinfo->staticCollInfo;

        return !clipBBoxIsClipped((real32*)&sinfo->collrectoffset,
                                  sinfo->uplength, sinfo->rightlength,
                                  sinfo->forwardlength);
    }
    else
    {
        //sphere test

        if (spaceobj->header.objtype == OBJ_ShipType)
        {
            //account for ship's radius
            real32 radius = spaceobj->staticinfo->staticCollInfo.approxcollspheresize;
            return (vecDotProduct(veye, vobj) > -radius);
        }
        else
        {
            return (vecDotProduct(veye, vobj) > 0.0f);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : rndShipVisibleUsingCoordSys
    Description : calls rndShipVisible after setting up appropriate matrices in the GL
    Inputs      : spaceobj - the spaceobject to consider
                  camera - current camera object
    Outputs     :
    Return      : TRUE if visible, FALSE otherwise
----------------------------------------------------------------------------*/
bool32 rndShipVisibleUsingCoordSys(SpaceObj* spaceobj, Camera* camera)
{
    GLfloat projection[16], modelview[16];
    hmatrix coordMatrixForGL;
    bool32    result;
    real32 scale;

    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetFloatv(GL_MODELVIEW_MATRIX,  modelview);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    rgluPerspective(camera->fieldofview, rndAspectRatio,
                    rndNear(camera->clipPlaneNear), camera->clipPlaneFar);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    rgluLookAt(camera->eyeposition.x, camera->eyeposition.y, camera->eyeposition.z,
               camera->lookatpoint.x, camera->lookatpoint.y, camera->lookatpoint.z,
               camera->upvector.x, camera->upvector.y, camera->upvector.z);

    //TODO(type safety, 2026-04-24): Take SpaceObjRot* as argument instead of SpaceObj*
    hmatMakeHMatFromMat(&coordMatrixForGL, &((SpaceObjRot*)spaceobj)->rotinfo.coordsys);
    hmatPutVectIntoHMatrixCol4(spaceobj->posinfo.position, coordMatrixForGL);
    glMultMatrixf((GLfloat*)&coordMatrixForGL);

    // CAT: the selection circle and the N-LIPS size cap are computed here for every visible object
    //      (rndShipVisibleUsingCoordSys 0x005750c0; HW1 did both in the main view's ship case): a ship's cap
    //      scales its mesh (Ship.magnitudeSquared, from a mimic's disguise when it mimics) and its selection
    //      circle, and the ships clamped to it get the same scale; a clamped ship keeps its host's. A missile's cap
    //      is its own (Missile.catMissileScale).
    selCircleCompute(&rndCameraMatrix, &rndProjectionMatrix, (SpaceObjRotImpTarg *)spaceobj);
    if (spaceobj->header.objtype == OBJ_ShipType)
    {
        Ship *ship = (Ship *)spaceobj;
        ClampInfo *clamp = ship->guidance.clampInfo;

        if (clamp == NULL || clamp->host == NULL)
        {
            if (selCameraSpace.z < 0.0f && ship->staticinfo->scaleCap != 0.0f)
            {
                real32 cap = bitTest(spaceobj->header.catFlags18, CAT_SHIPFLAG_Mimic) ?
                             cat_mimicGetApparentScale(ship) : ship->staticinfo->scaleCap;
                scale = 1.0f - (twnLipsBaseOffset + selCameraSpace.z) * cap;
                if (scale < 1.0f)
                {
                    scale = 1.0f;
                }
                ship->magnitudeSquared = scale;
                ship->impactable.collInfo.selCircleRadius *= scale;
            }
            else
            {
                ship->magnitudeSquared = 1.0f;
            }
            if (clamp != NULL && clamp->clampedObjs != NULL)
            {
                sdword i;
                for (i = 0; i < clamp->clampedObjs->numShips; i++)
                {
                    Ship *clamped = clamp->clampedObjs->ShipPtr[i];
                    clamped->magnitudeSquared = ship->magnitudeSquared;
                    clamped->impactable.collInfo.selCircleRadius *= ship->magnitudeSquared;
                }
            }
        }
    }
    else if (spaceobj->header.objtype == OBJ_MissileType)
    {
        Missile *missile = (Missile *)spaceobj;
        real32 cap = ((MissileStaticInfo *)missile->staticinfo)->scaleCap;

        if (selCameraSpace.z < 0.0f && cap != 0.0f)
        {
            scale = 1.0f - (twnLipsBaseOffset + selCameraSpace.z) * cap;
            if (scale < 1.0f)
            {
                scale = 1.0f;
            }
            missile->catMissileScale = scale;
        }
        else
        {
            missile->catMissileScale = 1.0f;
        }
    }

    result = rndShipVisible(spaceobj, camera);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projection);

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(modelview);

    return result;
}

/*-----------------------------------------------------------------------------
    Name        : rndFade
    Description : configures the rendering pipeline for fading ships out of existence
                  at the edge of the renderlist
    Inputs      : spaceobj - the object
                  camera - the camera
    Outputs     :
    Return      : TRUE if the ship is not visible (totally faded), FALSE otherwise
    Note        : CAT: the exe's body (rndFade 0x005753d0): HW1's 1.3 multiplier on both distances (0x0072be90; GoK
                  had set it to 1.0), the new classes (MonsterShip as a mothership, Micro as a fighter, a Component
                  by its type), an asteroid with attribute 0x200 at the carrier distances, a derelict's
                  CustShipInit slot as its fade (the exe reads the derelict static's +0x114 there); fade channel 0;
                  no far-clip fade (a GoK addition)
----------------------------------------------------------------------------*/
#define RND_FADE_MULT   1.3f                            // CAT: 0x0072be90
bool32 rndFade(SpaceObj* spaceobj, Camera* camera)
{
    vector distvec;
    real32 distsqr0, distsqr1, distsqr;
    real32 fadedist, maxdist, mindist;

    vecSub(distvec, camera->lookatpoint, spaceobj->posinfo.position);
    distsqr0 = vecMagnitudeSquared(distvec);

    vecSub(distvec, camera->eyeposition, spaceobj->posinfo.position);
    distsqr1 = vecMagnitudeSquared(distvec);

    distsqr = (distsqr0 < distsqr1) ? distsqr0 : distsqr1;

    if (spaceobj->header.objtype == OBJ_ShipType)
    {
        ShipStaticInfo *info = (ShipStaticInfo *)spaceobj->staticinfo;
        switch (info->shipclass)
        {
        case CLASS_Mothership:
        case CLASS_MonsterShip:
        mothership:
            fadedist = RENDER_FADE_MOTHERSHIP;
            maxdist  = RENDER_LIMIT_MOTHERSHIP;
            break;
        case CLASS_HeavyCruiser:
            fadedist = RENDER_FADE_HEAVYCRUISER;
            maxdist  = RENDER_LIMIT_HEAVYCRUISER;
            break;
        case CLASS_Carrier:
            fadedist = RENDER_FADE_CARRIER;
            maxdist  = RENDER_LIMIT_CARRIER;
            break;
        case CLASS_Destroyer:
            fadedist = RENDER_FADE_DESTROYER;
            maxdist  = RENDER_LIMIT_DESTROYER;
            break;
        case CLASS_Corvette:
            fadedist = RENDER_FADE_CORVETTE;
            maxdist  = RENDER_LIMIT_CORVETTE;
            break;
        case CLASS_Fighter:
        case CLASS_Micro:
            fadedist = RENDER_FADE_FIGHTER;
            maxdist  = RENDER_LIMIT_FIGHTER;
            break;
        case CLASS_Resource:
            fadedist = RENDER_FADE_RESOURCE;
            maxdist  = RENDER_LIMIT_RESOURCE;
            break;
        case CLASS_NonCombat:
            fadedist = RENDER_FADE_NONCOMBAT;
            maxdist  = RENDER_LIMIT_NONCOMBAT;
            break;
        case CLASS_Component:
            // CAT: a carrier's module as a carrier, the Dreadnought's cannon as a destroyer, the rest as motherships
            if (info->shiptype == sCarrierSupport)
            {
                fadedist = RENDER_FADE_CARRIER;
                maxdist  = RENDER_LIMIT_CARRIER;
            }
            else if (info->shiptype == sDreadnoughtCannon)
            {
                fadedist = RENDER_FADE_DESTROYER;
                maxdist  = RENDER_LIMIT_DESTROYER;
            }
            else
            {
                goto mothership;
            }
            break;
        case CLASS_Frigate:
        default:
            fadedist = RENDER_FADE_FRIGATE;     //happy medium
            maxdist  = RENDER_LIMIT_FRIGATE;
        }

        if (info->renderlistFade != 0.0f)
        {
            fadedist = info->renderlistFade;
            maxdist  = fsqrt(info->renderlistLimitSqr);
        }
        fadedist *= RND_FADE_MULT;
    }
    else if (spaceobj->header.objtype == OBJ_DerelictType)
    {
        DerelictStaticInfo *info = (DerelictStaticInfo *)spaceobj->staticinfo;
        if (info->renderlistFade != 0.0f)
        {
            fadedist = info->renderlistFade * RND_FADE_MULT;
            maxdist  = fsqrt(info->renderlistLimitSqr);
        }
        else if (spaceobj->header.flags & SOF_BigObject)
        {
            fadedist = RENDER_MAXVIEWABLE_FADE * RND_FADE_MULT;
            maxdist  = fsqrt(RENDER_MAXVIEWABLE_DISTANCE_SQR);
        }
        else
        {
            fadedist = RENDER_VIEWABLE_FADE * RND_FADE_MULT;
            maxdist  = fsqrt(RENDER_VIEWABLE_DISTANCE_SQR);
        }
    }
    else if (spaceobj->header.objtype == OBJ_AsteroidType && bitTest(spaceobj->attributes, CAT_RND_ATTR_ASTEROID_FAR))
    {
        fadedist = RENDER_FADE_CARRIER * RND_FADE_MULT;
        maxdist  = RENDER_LIMIT_CARRIER;
    }
    else
    {
        if (spaceobj->header.flags & SOF_BigObject)
        {
            fadedist = RENDER_MAXVIEWABLE_FADE * RND_FADE_MULT;
            maxdist  = fsqrt(RENDER_MAXVIEWABLE_DISTANCE_SQR);
        }
        else
        {
            fadedist = RENDER_VIEWABLE_FADE * RND_FADE_MULT;
            maxdist  = fsqrt(RENDER_VIEWABLE_DISTANCE_SQR);
        }
    }

    mindist = maxdist * RND_FADE_MULT - fadedist;
    mindist *= mindist;

    if (distsqr < mindist || g_ReplaceHack)
    {
        meshSetFade(0, 0.0f);
    }
    else if (mindist + fadedist * fadedist <= distsqr)
    {
        meshSetFade(0, 1.0f);
        return TRUE;
    }
    else
    {
        meshSetFade(0, (distsqr - mindist) / (fadedist * fadedist));
        rndAdditiveBlends(FALSE);
    }

    return FALSE;
}

void rndUnFade()
{
    meshSetFade(0, 0.0f);
}

/*-----------------------------------------------------------------------------
    Name        : rndRenderAHomeworld
    Description : Special 'world-render' code for drawing planets
    Inputs      : camera - what camera we're rendering from
                  world - what planet we're drawing
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
//TODO(type safety, 2026-04-24): Get rid of these
#define camera ((Camera *)voidCamera)
#define world  ((Derelict *)voidWorld)
void rndRenderAHomeworld(void* voidCamera, void *voidWorld)
{
    real32 planetScale;
    vector *position;
    meshdata *worldMesh;
    hmatrix coordMatrixForGL;
    GLboolean depthOn;
    sdword colorScheme = world->colorScheme;

    planetScale = world->staticinfo->scaleFactor;
    position = &world->obj.posinfo.position;
    worldMesh = (meshdata *)world->staticinfo->common.LOD->level[0].pData;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    rgluPerspective(btgFieldOfView, rndAspectRatio, rndNear(camera->clipPlaneNear), CAMERA_CLIP_FAR_PLANET);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    hmatMakeHMatFromMat(&coordMatrixForGL,&world->rotatable.rotinfo.coordsys);
    hmatPutVectIntoHMatrixCol4(*position, coordMatrixForGL);
    glMultMatrixf((float *)&coordMatrixForGL);//ship's rotation matrix
    glScalef(planetScale, planetScale, planetScale);

    rndNormalizeEnable(TRUE);
    // CAT (M2-R5 render): the depth test is put back as it was, not switched on (rndRenderAHomeworld 0x005757f0 at
    //      0x005758e2 / 0x0057594e)
    depthOn = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    meshObjectRender(&worldMesh->object[0], worldMesh->localMaterial, colorScheme);
    if (depthOn)
    {
        glEnable(GL_DEPTH_TEST);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
    rndNormalizeEnable(FALSE);

    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((GLfloat*)&rndProjectionMatrix);
    glMatrixMode(GL_MODELVIEW);
}
#undef world
#undef camera

/*-----------------------------------------------------------------------------
    Name        : rndRenderAWorldEffect
    Description : Render an effect in the manner of a homeworld.
    Inputs      : camera - camera we're looking at it from
                  effect - effect we are to render
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void rndRenderAWorldEffect(Camera *camera, Effect *effect)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    rgluPerspective(btgFieldOfView, rndAspectRatio, rndNear(camera->clipPlaneNear), CAMERA_CLIP_FAR_PLANET);

    glMatrixMode(GL_MODELVIEW);

    rndNormalizeEnable(TRUE);
    glDisable(GL_DEPTH_TEST);

    etgEffectDraw(effect);

    glEnable(GL_DEPTH_TEST);
    rndNormalizeEnable(FALSE);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((GLfloat*)&rndProjectionMatrix);
    glMatrixMode(GL_MODELVIEW);
}

static real32 lastTime0 = 0.0f;
static real32 lastTime1 = 0.0f;

void rndResetFloatTime(void)
{
    lastTime0 = taskTimeElapsed;
    lastTime1 = taskTimeElapsed;
}

/*-----------------------------------------------------------------------------
    Name        : rndCameraOffset0
    Description : camera offset fn 0
    Inputs      :
    Outputs     :
    Return      : a value to be added to a camera coordinate
----------------------------------------------------------------------------*/
real32 rndCameraOffset0(void)
{
    real32 et;
    static real32 t = 0.0f;

    t += taskTimeElapsed - lastTime0;
    lastTime0 = taskTimeElapsed;

    et = t * CAMERA_FLOAT_SCALAR0;

    return (real32)(sin((real64)et) - cos(0.5f * (real64)et));
}

/*-----------------------------------------------------------------------------
    Name        : rndCameraOffset1
    Description : camera offset fn 1
    Inputs      :
    Outputs     :
    Return      : a value to be added to a camera coordinate
----------------------------------------------------------------------------*/
real32 rndCameraOffset1(void)
{
    real32 et;
    static real32 t = 0.0f;

    t += taskTimeElapsed - lastTime1;
    lastTime1 = taskTimeElapsed;

    et = t * CAMERA_FLOAT_SCALAR1;

    return (real32)(sin((real64)et) + cos(0.5f * (real64)et));
}

/*-----------------------------------------------------------------------------
    Name        : rndScaleCameraOffsets
    Description : apply scaling to the camera float offsets to fudge a slightly
                  more linear falloff with distance
    Inputs      : camera - the camera
                  offset - offsets, 2 of 'em
    Outputs     : scaledOffset - scaled versions of the offsets
    Return      :
----------------------------------------------------------------------------*/
void rndScaleCameraOffsets(real32 *scaledOffset, Camera* camera, real32* offset)
{
    real32 dist;

    dist = camera->distance * CAMERA_FLOAT_DIST_SCALAR;
    if (dist > 1.0f)
    {
        scaledOffset[0] = offset[0] * dist;
        scaledOffset[1] = offset[1] * dist;
        scaledOffset[2] = offset[2] * dist;
    }
    else
    {
        scaledOffset[0] = offset[0];
        scaledOffset[1] = offset[1];
        scaledOffset[2] = offset[2];
    }
}

/*-----------------------------------------------------------------------------
    Name        : rndDockScalar
    Description : returns a value [0..1] indicating the amount of docking light
                  being received
    Inputs      : ship - ship to light
                  dockship - the ship that 'ship' is docking with
                  nearVal, farVal - range of light
    Outputs     :
    Return      : scalar [0..1] indicating amount of light being received
----------------------------------------------------------------------------*/
real32 rndDockScalar(Ship* ship, Ship* dockship, real32 nearVal, real32 farVal)
{
    vector veclen;
    real32 dist;

    vecSub(veclen, dockship->obj.posinfo.position, ship->obj.posinfo.position);
    dist = fsqrt(vecMagnitudeSquared(veclen));
    if (dist < nearVal) dist = nearVal;
    if (dist > farVal)  dist = farVal;
    dist -= nearVal;
    dist /= (farVal - nearVal);
    return 1.0f - dist;
}

/*-----------------------------------------------------------------------------
    Name        : rndDrawAsteroid0
    Description : renders asteroid0's (small asteroids that provide visible
                  structure to a level)
    Inputs      : n - number of points to render
    Outputs     : displays the asteroid0Data list
    Return      :
----------------------------------------------------------------------------*/
void rndDrawAsteroid0(sdword n)
{
    sdword  index;
    color   c;
    vector* v;

    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (index = 0; index < n; index++)
    {
        c = asteroid0Data[index].c;
        v = asteroid0Data[index].position;
        glColor3ub(colRed(c), colGreen(c), colBlue(c));
        glVertex3fv((GLfloat*)v);
    }
    glEnd();
    glPointSize(1.0f);
}

/*-----------------------------------------------------------------------------
    Name        : rndPreRenderDebugStuff
    Description : A bunch of debug stuff from the beginning of the render
                    function.
    Inputs      : camera - same as parameter to rndMainViewRender
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void rndPreRenderDebugStuff(Camera *camera)
{
#if GUN_TUNE_MODE
    if (gunTuningMode && (selSelected.numShips == 1))
    {
        if (keyIsStuck(INSERTKEY))
        {
            tuningGun++;

            keyClearSticky(INSERTKEY);
        }
    }
#endif

#if LOD_PRINT_DISTANCE
    if (keyIsStuck(INSERTKEY))
    {
        if (rndLOD < 10)
        {
            rndLOD++;
            dbgMessagef("Level of detail = %d", rndLOD);
        }
        keyClearSticky(INSERTKEY);
    }
    if (keyIsStuck(HOMEKEY))
    {
        if (rndLOD > 0)
        {
            rndLOD--;
            dbgMessagef("Level of detail = %d", rndLOD);
        }
        keyClearSticky(HOMEKEY);
    }
    if (keyIsStuck(DELETEKEY))
    {
        lodDrawingMode ^= TRUE;
        if (selSelected.numShips == 1)
        {
            vector length;

            length.x = camera->eyeposition.x - selSelected.ShipPtr[0]->obj.posinfo.position.x;
            length.y = camera->eyeposition.y - selSelected.ShipPtr[0]->obj.posinfo.position.y;
            length.z = camera->eyeposition.z - selSelected.ShipPtr[0]->obj.posinfo.position.z;
            dbgMessagef("Camera distance = %f", vecMagnitudeSquared(length));
        }
        else
        {
            dbgMessagef("Please select one and only one object for LOD tuning.");
        }
        keyClearSticky(DELETEKEY);
    }
    if (keyIsStuck(ENDKEY))
    {
        lodTuningMode ^= TRUE;
        dbgMessagef("LOD tuning %s", lodTuningMode ? "ON" : "OFF");
        keyClearSticky(ENDKEY);
    }
#endif
#if RND_XYZ
    if (keyIsStuck(RND_XYZKey))
    {
        keyClearSticky(RND_XYZKey);
        rndXYZPrint ^= TRUE;
    }
#endif //RND_XYZ
}

/*-----------------------------------------------------------------------------
    Name        : rndPostRender3DStuff
    Description : Post-object rendering of 3D debugging stuff
    Inputs      : camera - same as parameter to rndMainViewRender
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rndPostRenderDebug3DStuff(Camera *camera)
{
#if NIS_DRAW_CAMERA
    if (nisIsRunning && nisCamera != NULL && nisCamera != camera)
    {                                                       //if we should draw the NIS camera
        vector upVector = nisCamera->eyeposition;
        vector cameraVector;
        static bool32 flashFlag;

        rndLightingEnable(FALSE);
        vecSub(cameraVector, nisCamera->lookatpoint, nisCamera->eyeposition);
        vecMultiplyByScalar(cameraVector, 200);
        vecAdd(cameraVector, cameraVector, nisCamera->lookatpoint);
        primLine3(&nisCamera->eyeposition, &cameraVector, flashFlag ? colWhite : colFuscia);
        vecScalarMultiply(upVector, nisCamera->upvector, 200);
        vecAdd(upVector, nisCamera->eyeposition, upVector);
//        upVector.z -= ABS(nisCamera->eyeposition.z - cameraVector.z) +
//            ABS(nisCamera->eyeposition.y - cameraVector.y);
        primLine3(&nisCamera->eyeposition, &upVector, colWhite);
//      if (selSelected.numShips > 0)
//      {
//          primLine3(&nisCamera->eyeposition, &selSelected.ShipPtr[0]->obj.posinfo.position, flashFlag ? colWhite : colFuscia);
//      }
        flashFlag ^= TRUE;
    }
#endif
}


#if DEBUG_FONT_CHECK_SPECIAL
fonthandle testing=1;
extern fontregistry frFontRegistry[FR_NumberFonts];
#endif

/*-----------------------------------------------------------------------------
    Name        : rndPostRenderDebug2DStuff
    Description : Post-object rendering of 2D debug stuff
    Inputs      : camera - same as parameter to rndMainViewRender
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rndPostRenderDebug2DStuff(Camera *camera)
{
#if LOD_PRINT_DISTANCE
    if (lodDrawingMode && selSelected.numShips == 1)
    {
        vector length;
        vector shippos = selSelected.ShipPtr[0]->impactable.collInfo.collPosition;
        sdword nCurrentLOD;

        if (!lodTuningMode)
        {
            nCurrentLOD = selSelected.ShipPtr[0]->obj.currentLOD;
        }
        else
        {
            nCurrentLOD = rndLOD;
        }

        vecSub(length,camera->eyeposition,shippos);

        fontPrintf(0, MAIN_WindowHeight - fontHeight(" ") - 1, colWhite,
                   "Current LOD = %d, distance = %f, x = %.1f y = %.1f z = %.1f",
                   nCurrentLOD, vecMagnitudeSquared(length),shippos.x,shippos.y,shippos.z);
    }
#endif
#if GUN_TUNE_MODE
    if (gunTuningMode && (selSelected.numShips == 1))
    {
        gunTuneGun(selSelected.ShipPtr[0]);
    }
#endif

#if RND_SCALECAP_TWEAK
    fontPrint(0, 0, colWhite, scaleCapString);
#endif

#if SHOW_TRAIL_STATS
    {
        char buf[256];
        static lastUpdated;
        static lastNotUpdated;

        if (trailsUpdated == 0)
        {
            sprintf(buf, "ren %3d up %3d not %3d",
                    trailsRendered, lastUpdated, lastNotUpdated);
        }
        else
        {
            sprintf(buf, "ren %3d up %3d not %3d",
                    trailsRendered, trailsUpdated, trailsNotUpdated);
            lastUpdated = trailsUpdated;
            lastNotUpdated = trailsNotUpdated;
        }

        fontPrint(0, 32, colWhite, buf);
        trailsUpdated = trailsNotUpdated = 0;

        {
            extern sdword bTrailRender, bTrailDraw;

            sprintf(buf, "render %d draw %d", bTrailRender, bTrailDraw);
            fontPrint(0, 48, colWhite, buf);

            if (keyIsHit(SHIFTKEY))
            {
                if (keyIsHit(NINEKEY)) bTrailRender = 1;
                if (keyIsHit(ZEROKEY)) bTrailDraw = 1;
            }
            else
            {
                if (keyIsHit(NINEKEY)) bTrailRender = 0;
                if (keyIsHit(ZEROKEY)) bTrailDraw = 0;
            }
        }
    }
#endif

#if USE_RND_HINT
    {
        char buf[32];
        switch (rndHint)
        {
        case 0:
            sprintf(buf, "hint [default]");
            break;
        case 1:
            sprintf(buf, "hint [on]");
            break;
        case 2:
            sprintf(buf, "hint [off]");
            break;
        default:
            sprintf(buf, "hint [?]");
        }
        fontPrint(MAIN_WindowWidth - fontWidth(buf), fontHeight(buf), colWhite, buf);
    }
#endif

#if RND_FOV_TWEAK
    if (keyIsHit(NINEKEY))
    {
        btgFieldOfView -= btgFieldOfViewDelta;
        sprintf(fovString, "fov = %f", btgFieldOfView);
    }
    if (keyIsHit(ZEROKEY))
    {
        btgFieldOfView += btgFieldOfViewDelta;
        sprintf(fovString, "fov = %f", btgFieldOfView);
    }
    fontPrint(MAIN_WindowWidth - fontWidth(fovString), 0, colWhite, fovString);
#endif //RND_FOV_TWEAK

#if RND_SCALECAP_TWEAK
    if (keyIsHit(RND_CapScaleCapKey))
    {
        if (keyIsHit(CONTROLKEY))
        {
            RND_CAPSCALECAP_STATS = TRUE;
        }
        else
        {
            RND_CAPSCALECAP_STATS = FALSE;
        }
    }
    if (RND_CAPSCALECAP_STATS)
    {
        rndCapScaleCapStatsTaskFunction();
    }
#endif //RND_SCALECAP_TWEAK

#if DEBUG_RND_DOCK_LIGHT_TWEAK
    if (selSelected.numShips == 1)
    {
        Ship* ship = selSelected.ShipPtr[0];
        if (ship->dockvars.catDockPointIndex != -1 &&
            ship->dockvars.dockship != NULL)
        {
            vector veclen;
            real32 dist;
            Ship* dockship;
            char string[64];

            dockship = ship->dockvars.dockship;
            vecSub(veclen, ship->obj.posinfo.position, dockship->obj.posinfo.position);
            dist = fsqrt(vecMagnitudeSquared(veclen));
            sprintf(string, "%0.2f m", dist);
            fontPrint(0, 0, colWhite, string);
        }
    }
#endif

#if RND_FRAME_RATE
    if (rndDisplayFrameRate)
    {
        char   string[256];
#if DEBUG_VERBOSE_SHIP_STATS
        sdword i;
#endif
        extern sdword trTextureChanges, trAvoidedChanges;

        sprintf(string, "(%d . %d) (%u) . %u",
                trTextureChanges, trAvoidedChanges,
                alodGetPolys(), meshRenders);

        trTextureChanges = 0;
        trAvoidedChanges = 0;

        meshRenders = 0;
        fontPrint(MAIN_WindowWidth - fontWidth(string), 0, colWhite, string);
#if DEBUG_VERBOSE_SHIP_STATS
        for (i = 0; i < 5; i++)
        {
            sprintf(string, "%d: %d / %d", i, shipsAtLOD[i], polysAtLOD[i]);
            shipsAtLOD[i] = 0;
            polysAtLOD[i] = 0;
            fontPrint(MAIN_WindowWidth - fontWidth(string), (i+1)*fontHeight(string), colWhite, string);
        }
#endif
#if DEBUG_DISPLAY_LOD_SCALE
        sprintf(string, "scale %f", lodScaleFactor);
        fontPrintf(0, fontHeight(string), colWhite, string);
#endif
        if (alodGetPanic())
        {
            sprintf(string, "PANIC!");
            fontPrint((MAIN_WindowWidth - fontWidth(string)) / 2, 0, colRGB(255,0,0), string);
        }

        if (universe.mainCameraCommand.currentCameraStack->focus.numShips == 1)
        {
            Ship* ship = universe.mainCameraCommand.currentCameraStack->focus.ShipPtr[0];
            vector veclen;
            real32 dist;

//            vecSub(veclen, ship->obj.posinfo.position, camera->eyeposition);
            vecSub(veclen, ship->impactable.collInfo.collPosition, camera->eyeposition);
            dist = fsqrt(vecMagnitudeSquared(veclen));
            sprintf(string, "dist %0.2f m", dist);
            fontPrint((MAIN_WindowWidth - fontWidth(string)) / 2,
                      fontHeight(" "),
                      colWhite, string);
        }
    }
#endif
#if MEM_STATISTICS
        if (memStatsLogging)
        {
            sdword y = 0, x = 0, maxWidth = 0, width, index;
            fontPrint(0, y, colWhite, memStatString);
            for (index = 0; index < MS_NumberCookieNames; index++)
            {
                if (memStatsCookieNames[index].outputString[0] == 0)
                {
                    continue;
                }
                y += fontHeight(" ") + 1;
                width = fontWidth(memStatsCookieNames[index].outputString);
                maxWidth = max(maxWidth, width);
                if (y >= MAIN_WindowHeight)
                {
                    y = fontHeight(" ") + 1;
                    x += maxWidth + 1;
                    maxWidth = 0;
                    if (x > MAIN_WindowWidth)
                    {
                        break;
                    }
                }
                fontPrint(x, y, colWhite, memStatsCookieNames[index].outputString);
            }
        }
#endif
#if DEM_CHECKSUM
    if (demChecksumError)
    {
        fontPrint(2, MAIN_WindowHeight - 60, colWhite, demChecksumString);
    }
#endif

#if DEBUG_FONT_CHECK_SPECIAL
    fontMakeCurrent(testing);
    fontPrint(10,50,colWhite,"����������������������������������������������������������������");

    if ((keyIsStuck(CONTROLKEY)) && (keyIsHit(SHIFTKEY)))
    {
        testing--;
        if (testing < 1) testing = 1;
        keyClearSticky(CONTROLKEY);
    }
    else if (keyIsStuck(CONTROLKEY))
    {
        testing++;
        if (frFontRegistry[testing].name == NULL)
        {
            testing--;
        }
        keyClearSticky(CONTROLKEY);
    }

#endif

    trkTrackValuesDisplay();
}

/*-----------------------------------------------------------------------------
    Name        : rndMainViewRenderNothingFunction
    Description : Don't do anything; just return.  For when something else is
                    already rendering the screen.
    Inputs      : camera - it's not even used
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void rndMainViewRenderNothingFunction(Camera *camera)
{
    return;
}

/*-----------------------------------------------------------------------------
    Name        : rndMainViewAllButRenderFunction
    Description : Compute selection info for space obj's but don't render them
    Inputs      :
    Outputs     :
    Return      :
    Note        : Because this function mirrors much of the functionality of
                    rndMainViewRenderFunction, any drastic changes to
                    rndMainViewRenderFunction will have to be made this function.
----------------------------------------------------------------------------*/
void rndMainViewAllButRenderFunction(Camera *camera)
{
    ;
}

/*=============================================================================
    CAT: the per-object render state of the Cataclysm renderer (cataclysm.exe 0x00c32c00..0x00c32c9b; the object
    pointer at 0x00c32c00 is catShCurrentObject in Shader.c). cat_rndRenderObjectList 0x005776f0 and
    cat_rndObjectEffectsSetup 0x00575c90 set it per object; the mesh renderers (Mesh.c) read the fade, dissolve and
    tint flags. Names chosen here.
=============================================================================*/
bool8  catRndEdgeEffect = FALSE;            // 0x00c32c04: the Beast-stun edge lightning (Ship +0x4D0..+0x4D8)
bool8  catRndFadeFlag = FALSE;              // 0x00c32c08: a fade by clip plane (build-in) or the ship-view fade
ubyte  catRndFadeMode = 0;                  // 0x00c32c09: CAT_RND_FADE_*
bool8  catRndFadeFlip = FALSE;              // 0x00c32c0a: the second pass of the build-in fade
real32 catRndFadeT = 0.0f;                  // 0x00c32c10
bool8  catRndBeastStun = FALSE;             // 0x00c32c50: a ship turning into a Beast ship (Ship +0x4E4)
real32 catRndBeastStunT = 0.0f;             // 0x00c32c54: 0 .. 1 of the change
sbyte  catRndTimedPhase = 0;                // 0x00c32c58: the salvage conversion's phase 1..3 (Ship +0x508 / +0x50C)
real32 catRndTimedT = 0.0f;                 // 0x00c32c5c: the phase's progress
real32 catRndTimedFrac = 0.0f;              // 0x00c32c60: the conversion's progress
bool8  catRndDissolve = FALSE;              // 0x00c32c64: the dissolve cut plane is on (Dissolve.c)
bool8  catRndRaceTint = FALSE;              // 0x00c32c78: a Beast ship's red ambient (single player)
bool8  catRndAsteroidTint = FALSE;          // 0x00c32c79: an asteroid with attribute 0x200 (no red/green ambient)
ubyte  catRndEMPFlicker = 0;                // 0x00c32c7a: 1 = a moving ship the Naggarok EMP hit, 2 = flicker running
bool8  catRndMimic = FALSE;                 // 0x00c32c7c: a mimic that has started a disguise (MimicSpec.started)
real32 catRndMimicFade = 0.0f;              // 0x00c32c80: MimicSpec.fade
lodinfo *catRndMimicLOD = NULL;             // 0x00c32c84: MimicSpec.apparentLOD
shipbindings *catRndMimicBindings = NULL;   // 0x00c32c88: MimicSpec.apparentBindings
sdword catRndMimicColor = 0;                // 0x00c32c8c: MimicSpec.apparentColor
sdword catRndMimicRace = 0;                 // 0x00c32c90: MimicSpec.apparentRace
real32 catRndMimicRadius = 0.0f;            // 0x00c32c94: MimicSpec.apparentRadius
bool32 catRndMimicOn = FALSE;               // 0x00c32c98: MimicSpec.mode == MIMIC_On
//CAT: the ship view's selection time (0x00a5e264, set by svSelectShip; the ship view is the HUD's, M2-R5 hud):
//     cat_rndSetupFadeFlag fades a NULL object in over 1.5 s from it
real32 catRndShipViewSelectTime = 0.0f;

//CAT: the exe's build-in fade mode (0x00878168: 1 in .data, the console's "constyle" row writes it) and its console
//     switches (0x00878171 trails pass, "TRAILS"; 0x0087816e the backgrounds, "BTG"; 0x008bab28 reset of the wireframe
//     hacks after a ship: 1 in .data, no writer) [M2-R6 startup-keys: the console's rows (Console.c) write the three]
sdword catRndConsoleStyle = CAT_RND_FADE_CLIP;
#define CAT_RND_FADE_MODE_BUILD     ((ubyte)catRndConsoleStyle)
bool8 catRndDrawTrails = TRUE;
bool8 catRndDrawBTG = TRUE;
static const bool32 catRndResetHacksAfterShip = TRUE;
//CAT: the exe's renderer tests RGL (0x00b3a6d8: the rgl.dll device is in use) and RGLtype (0x00b3a700: SWtype 0,
//     GLIDEtype 1, D3Dtype 2, GLtype 3). This tree draws with OpenGL only (as GoK), i.e. RGL FALSE and GLtype; the
//     RGL-only calls (glPixelTransferf flash, the rgl fog, rglFeature) and the D3D near-plane halving drop out.
#define CAT_RND_RGLTYPE_GL          3
#define CAT_RND_RGLTYPE             CAT_RND_RGLTYPE_GL
//CAT: glCapFeatureExists(RGL_BROKEN_MIXED_DEPTHTEST 0x9003) is TRUE only on a D3D device with broken depth
#define CAT_RND_BROKEN_MIXED_DEPTH  FALSE
//CAT: the perspective-correction hints of the exe's inlined rndPerspectiveCorrection (glHint
//     GL_PERSPECTIVE_CORRECTION_HINT, mainNoPerspective 0x00a74d5c) are not issued: GoK dropped
//     rndPerspectiveCorrection, and the hint has no effect on a current GL

//CAT: the depth-sorted list of the objects drawn after the opaque pass (0x00a75d10..0x00a75d20)
static sdword catRndSortMode = 0;           // 0x00a75d10: 0 none, 1 the opaque pass (collect), 2 the sorted pass
static SpaceObj **catRndSortList = NULL;    // 0x00a75d14
static sdword catRndSortCount = 0;          // 0x00a75d18
static sdword catRndSortMax = 0;            // 0x00a75d1c
static sdword catRndSortIndex = 0;          // 0x00a75d20

//CAT: the Sentinel force-field formations already queued this frame (0x00b378a0; nodes of 0x18 bytes:
//     {formation, Node, queued})
typedef struct CatRndFieldNode
{
    struct CommandToDo *formation;
    Node link;
    bool32 queued;
} CatRndFieldNode;

//CAT: a module (CLASS_Component) answers for its host (0x00577aeb, 0x00578049, 0x005793ca)
static Ship *cat_rndShipOrClampHost(Ship *ship)
{
    ClampInfo *clamp = ship->guidance.clampInfo;

    if (ship->staticinfo->shipclass == CLASS_Component && clamp != NULL && clamp->host != NULL)
    {
        return (Ship *)clamp->host;
    }
    return ship;
}

//CAT: counters for the render self-test (port diagnostic, not in the exe: /catRenderTest, catRenderSelfTestFrame)
CatRndCounts catRndCounts;

/*-----------------------------------------------------------------------------
    Name        : cat_kasNaggarokEMPHitShip (Cataclysm 0x00575b30)
    Description : TRUE if the mission's Naggarok EMP (KAS variable G_NaggarokEMP) has hit the ship more often than
                  its EMP shock count (Ship +0x52C)
    Inputs      : ship
    Outputs     :
    Return      : TRUE / FALSE
----------------------------------------------------------------------------*/
bool32 cat_kasNaggarokEMPHitShip(Ship *ship)
{
    // CAT: the global KAS variable G_NaggarokEMP (cat_kasVarFind 0x00491b20) above the ship's EMP shock count,
    //      compared unsigned (M2-R8 kasf library)
    CatKasVar *var = cat_kasVarFind(0, cat_kasHashString("G_NaggarokEMP"));

    return (var != NULL) && ((udword)ship->catEmpShockCount < (udword)var->value);
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndShipEMPdAndMoving (Cataclysm 0x00575ae0)
    Description : TRUE for a ship the Naggarok EMP hit that is still moving (|v|^2 > 0.01)
----------------------------------------------------------------------------*/
bool32 cat_rndShipEMPdAndMoving(Ship *ship)
{
    vector *v = &ship->obj.posinfo.velocity;

    return cat_kasNaggarokEMPHitShip(ship) && (v->z * v->z + v->y * v->y + v->x * v->x > 0.01f);
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndSetupDissolveFlag (Cataclysm 0x005759f0)
----------------------------------------------------------------------------*/
void cat_rndSetupDissolveFlag(SpaceObj *obj)
{
    catRndDissolve = FALSE;
    if (obj != NULL && obj->header.objtype == OBJ_ShipType && cat_shipDissolveIsActive((Ship *)obj))
    {
        catRndDissolve = TRUE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndSetupFadeFlag (Cataclysm 0x00575a20)
    Description : A ship being built (SPECIAL_2_CatBeingBuilt) is cut by a clip plane at its build progress; with
                  no object, the ship view fades its ship in over 1.5 s after the selection
----------------------------------------------------------------------------*/
void cat_rndSetupFadeFlag(SpaceObj *obj)
{
    real32 t;
    real64 whole;

    catRndFadeFlag = FALSE;
    if (obj == NULL)
    {
        t = (cat_taskRealTimeElapsed - catRndShipViewSelectTime) * (2.0f / 3.0f);   // 0x0072be94
        if (t < 1.0)
        {
            catRndFadeFlag = TRUE;
            catRndFadeT = (real32)modf((real64)t, &whole);
            catRndFadeMode = CAT_RND_FADE_VIEW;
            catRndFadeFlip = FALSE;
        }
    }
    else if (obj->header.objtype == OBJ_ShipType && bitTest(((Ship *)obj)->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
    {
        catRndFadeFlag = TRUE;
        catRndFadeMode = CAT_RND_FADE_MODE_BUILD;
        catRndFadeFlip = FALSE;
        catRndFadeT = 1.0f - ((Ship *)obj)->catBuildProgress;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndSetupTimedEffectPhase (Cataclysm 0x00575b70)
    Description : The salvage conversion's phase (Ship catSalvageConvertStart/End): 1 for the first tenth (an
                  eased rise), 2 to nine tenths (linear), 3 for the last tenth (eased)
----------------------------------------------------------------------------*/
void cat_rndSetupTimedEffectPhase(SpaceObj *obj)
{
    Ship *ship = (Ship *)obj;
    real64 x;

    catRndTimedPhase = 0;
    if (obj != NULL && obj->header.objtype == OBJ_ShipType && universe.totaltimeelapsed < ship->catSalvageConvertEnd)
    {
        catRndTimedFrac = (universe.totaltimeelapsed - ship->catSalvageConvertStart) /
                          (ship->catSalvageConvertEnd - ship->catSalvageConvertStart);
        if (catRndTimedFrac < 0.1f)
        {
            catRndTimedPhase = 1;
            x = (real64)catRndTimedFrac * (real64)10.0f;                         // 0x0072bea0
        }
        else if (catRndTimedFrac < 0.9f)
        {
            catRndTimedPhase = 2;
            catRndTimedT = (catRndTimedFrac - 0.1f) * 1.2500001f;               // 0x0072be9c
            return;
        }
        else
        {
            catRndTimedPhase = 3;
            x = ((real64)catRndTimedFrac - (real64)0.9f) * (real64)9.999998f;    // 0x0072be98
        }
        catRndTimedT = (real32)(1.0 - (cos((x + 1.0) * (real64)1.5707964f) + 1.0));
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndSetupMimicFlag (Cataclysm 0x00575c60)
----------------------------------------------------------------------------*/
void cat_rndSetupMimicFlag(SpaceObj *obj)
{
    catRndMimic = FALSE;
    if (obj != NULL && bitTest(obj->header.catFlags18, CAT_SHIPFLAG_Mimic) &&
        ((MimicSpec *)((Ship *)obj)->ShipSpecifics)->started)
    {
        catRndMimic = TRUE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndBeastStunSetup / cat_rndMimicParamsSetup
    Description : the two halves of cat_rndSetupObjectEffectParams 0x00575f40 (inlined into
                  cat_rndRenderObjectList 0x00577f1e)
----------------------------------------------------------------------------*/
static void cat_rndSetupObjectEffectParamsInline(Ship *ship)
{
    real32 duration;
    MimicSpec *spec;

    if (catRndBeastStun)
    {
        duration = cat_beastTimeSpentStunned(ship->shiptype);
        if (nisIsRunning)
        {
            duration = 4.0f;                                                    // 0x0072a530
        }
        catRndBeastStunT = 1.0f - (ship->catBeastConvertTime - universe.totaltimeelapsed) / duration;
        if (catRndBeastStunT > 1.0f)
        {
            catRndBeastStunT = 1.0f;
        }
    }
    if (catRndMimic)
    {
        spec = (MimicSpec *)ship->ShipSpecifics;
        catRndMimicFade = spec->fade;
        catRndMimicLOD = spec->apparentLOD;
        catRndMimicRadius = spec->apparentRadius;
        catRndMimicBindings = spec->apparentBindings;
        catRndMimicColor = spec->apparentColor;
        catRndMimicRace = spec->apparentRace;
        catRndMimicOn = (spec->mode == MIMIC_On);
    }
}

void cat_rndSetupObjectEffectParams(SpaceObj *obj)
{
    if (obj != NULL)
    {
        cat_rndSetupObjectEffectParamsInline((Ship *)obj);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndSetupObjectFlags
    Description : the flag block every object renderer starts with (cat_rndRenderObjectList 0x005776f0,
                  cat_rndObjectEffectsSetup 0x00575c90, the trail pass of rndMainViewRenderFunction 0x00576f10):
                  edge lightning, Beast race, Naggarok EMP, build fade, Beast stun, conversion phase, mimic, dissolve
----------------------------------------------------------------------------*/
static void cat_rndSetupObjectFlags(SpaceObj *obj)
{
    Ship *ship = (Ship *)obj;
    bool32 isShip = (obj != NULL && obj->header.objtype == OBJ_ShipType);

    catShCurrentObject = obj;
    catRndEdgeEffect = FALSE;
    if (isShip && !bitTest(obj->header.catFlags18, CAT_SHIPFLAG_Mimic) && ship->catBeastStunState != 0 &&
        ship->catBeastStunStart <= universe.totaltimeelapsed && universe.totaltimeelapsed < ship->catBeastStunEnd)
    {
        catRndEdgeEffect = TRUE;
    }
    catRndRaceTint = (isShip && ship->shiprace == RACE_Beast);
    catRndEMPFlicker = (isShip && ship->shiptype == dRKO && cat_rndShipEMPdAndMoving(ship)) ? 1 : 0;
    cat_rndSetupFadeFlag(obj);
    catRndBeastStun = FALSE;
    if (isShip && (universe.totaltimeelapsed < ship->catBeastConvertTime ||
                   (ship->catBeastConvertTime != 0.0f && nisIsRunning)))
    {
        catRndBeastStun = TRUE;
    }
    cat_rndSetupTimedEffectPhase(obj);
    cat_rndSetupMimicFlag(obj);
    cat_rndSetupDissolveFlag(obj);
    catRndAsteroidTint = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndObjectEffectsSetup (Cataclysm 0x00575c90)
    Description : Set the render state for an object outside the main view (the ship view, the bullet-hit glow)
----------------------------------------------------------------------------*/
void cat_rndObjectEffectsSetup(SpaceObj *obj)
{
    cat_rndSetupObjectFlags(obj);
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndSortObjectsByDepth (Cataclysm 0x00577660)
    Description : mode 1 empties the deferred list (the opaque pass collects into it), mode 2 sorts it farthest
                  first (SpaceObj.cameraDistanceSquared) and starts the sorted pass, mode 0 ends it
----------------------------------------------------------------------------*/
void cat_rndSortObjectsByDepth(sdword mode)
{
    sdword i, j;
    SpaceObj *swap;

    catRndSortMode = mode;
    if (mode == 1)
    {
        catRndSortCount = 0;
    }
    else if (mode == 2)
    {
        catRndSortIndex = 0;
        for (i = 0; i < catRndSortCount; i++)
        {
            for (j = i + 1; j < catRndSortCount; j++)
            {
                if (catRndSortList[i]->cameraDistanceSquared < catRndSortList[j]->cameraDistanceSquared)
                {
                    swap = catRndSortList[i];
                    catRndSortList[i] = catRndSortList[j];
                    catRndSortList[j] = swap;
                }
            }
        }
    }
}

static void cat_rndSortListAdd(SpaceObj *obj)
{
    if (catRndSortCount == catRndSortMax)
    {
        catRndSortList = memRealloc(catRndSortList, (catRndSortMax + 16) * sizeof(SpaceObj *), "catRndSortList",
                                    NonVolatile);
        catRndSortMax += 16;
    }
    catRndSortList[catRndSortCount++] = obj;
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndDepthTestEnable (Cataclysm 0x0057ae40)
    Description : Enable or disable the depth test (and, on a device with broken mixed depth tests, the depth mask)
    Return      : the previous depth test state
----------------------------------------------------------------------------*/
bool32 cat_rndDepthTestEnable(bool32 enable)
{
    bool32 old = glIsEnabled(GL_DEPTH_TEST);

    if (enable)
    {
        glEnable(GL_DEPTH_TEST);
        if (CAT_RND_BROKEN_MIXED_DEPTH)
        {
            glDepthMask(GL_TRUE);
        }
    }
    else if (CAT_RND_RGLTYPE != 0 && CAT_RND_BROKEN_MIXED_DEPTH)
    {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
    return old;
}

/*-----------------------------------------------------------------------------
    Name        : cat_meshHierarchyCount / cat_lodHierarchyCount (Cataclysm 0x00575ea0 / 0x00575ee0)
    Description : the polygons of an object and its children / of a level's mesh (every top-level object)
----------------------------------------------------------------------------*/
sdword cat_meshHierarchyCount(polygonobject *object)
{
    sdword count = 0;
    polygonobject *child;

    for (child = object->pDaughter; child != NULL; child = child->pSister)
    {
        count += cat_meshHierarchyCount(child);
    }
    return count + object->nPolygons;
}

sdword cat_lodHierarchyCount(lod *level)
{
    meshdata *mesh = (meshdata *)level->pData;
    polygonobject *object, *child;
    sdword count = 0, sub;

    if (mesh == NULL)
    {
        return 0;
    }
    for (object = &mesh->object[0]; object != NULL; object = object->pSister)
    {
        sub = 0;
        for (child = object->pDaughter; child != NULL; child = child->pSister)
        {
            sub += cat_meshHierarchyCount(child);
        }
        count += sub + object->nPolygons;
    }
    return count;
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndLightningBoltDraw (Cataclysm 0x00576010)
    Description : A jagged line strip of <segments> segments from p1 to p2: every inner vertex moves by a random
                  offset in [-a, a] per axis (stream 0), a = |step * jitter| / 2; the first fifth of the segments
                  is pulled back toward the straight line
    Inputs      : x1..z2 - the ends; segments; jitter - the offset scale
----------------------------------------------------------------------------*/
void cat_rndLightningBoltDraw(real32 x1, real32 y1, real32 z1, real32 x2, real32 y2, real32 z2, sdword segments,
                              real32 jitter)
{
    vector step, line, bolt;
    real32 length, amp, twoAmp, negAmp, k;
    sdword i, remaining, pullCount;
    vector pull;

    step.x = x2 - x1;
    step.y = y2 - y1;
    step.z = z2 - z1;
    length = fsqrt(step.z * step.z + step.y * step.y + step.x * step.x);
    vecNormalize(&step);
    if (step.x == 0.0f && step.y == 0.0f && step.z == 0.0f)
    {
        step.z = 1.0f;
    }
    k = length / (real32)segments;
    step.x *= k;
    step.y *= k;
    step.z *= k;
    {
        real32 sx = step.x * jitter, sy = step.y * jitter, sz = step.z * jitter;
        amp = fsqrt(sz * sz + sy * sy + sx * sx) * 0.5f;
    }

    glBegin(GL_LINE_STRIP);
    line.x = bolt.x = x1;
    line.y = bolt.y = y1;
    line.z = bolt.z = z1;
    glVertex3f(x1, y1, z1);
    pullCount = segments / 5;                                                   // 0x0072be5c
    remaining = segments;
    for (i = 0; i < segments; i++, remaining--)
    {
        line.x += step.x;
        line.y += step.y;
        line.z += step.z;
        bolt.x += step.x;
        bolt.y += step.y;
        bolt.z += step.z;
        if (i < segments - 1)
        {
            negAmp = -amp;
            twoAmp = amp - negAmp;
            bolt.x = (real32)((real64)ranRandom(RANDOM_STREAM_0) * twoAmp * (1.0 / 4294967296.0) + negAmp + bolt.x);
            bolt.y = (real32)((real64)ranRandom(RANDOM_STREAM_0) * twoAmp * (1.0 / 4294967296.0) + negAmp + bolt.y);
            bolt.z = (real32)((real64)ranRandom(RANDOM_STREAM_0) * twoAmp * (1.0 / 4294967296.0) + negAmp + bolt.z);
            if (remaining < pullCount)
            {
                k = (real32)remaining;
                pull.x = (line.x - bolt.x) / k;
                pull.y = (line.y - bolt.y) / k;
                pull.z = (line.z - bolt.z) / k;
                bolt.x += pull.x;
                bolt.y += pull.y;
                bolt.z += pull.z;
            }
        }
        glVertex3f(bolt.x, bolt.y, bolt.z);
    }
    glEnd();
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndLinkLightningDraw (Cataclysm 0x00576330)
    Description : An ACV that is linking draws lightning to each ship of its EMP list, in 1 of 5 frames per ship
                  (stream 0); segments and jitter by LOD from 0x0072be60
----------------------------------------------------------------------------*/
static const struct { sdword segments; real32 jitter; } catRndLinkBolt[4] =
    { {320, 1.5f}, {140, 2.0f}, {80, 3.0f}, {80, 3.0f} };
#define CAT_RND_LINK_BOLT_CHANCE    5                                           // 0x0072be80
void cat_rndLinkLightningDraw(Ship *ship)
{
    bool32 lighting, texture;
    GrowSelection *targets;
    sdword lod, i;
    Ship *target;

    if (!cat_acvIsLinking(ship))
    {
        return;
    }
    lighting = rndLightingEnable(FALSE);
    texture = rndTextureEnable(FALSE);
    targets = cat_acvGetSpecifics(ship);
    glColor3f(1.0f, 1.0f, 0.9f);                                                // 0x008bab14..0x008bab1c
    lod = ship->obj.currentLOD;
    if (lod >= 3)
    {
        lod = 3;
    }
    if (targets->selection != NULL)
    {
        for (i = 0; i < targets->selection->numShips; i++)
        {
            if ((CAT_RND_LINK_BOLT_CHANCE < 1 || (ranRandom(RANDOM_STREAM_0) % CAT_RND_LINK_BOLT_CHANCE) == 0) &&
                (target = targets->selection->ShipPtr[i]) != NULL)
            {
                cat_rndLightningBoltDraw(ship->obj.posinfo.position.x, ship->obj.posinfo.position.y,
                                         ship->obj.posinfo.position.z, target->obj.posinfo.position.x,
                                         target->obj.posinfo.position.y, target->obj.posinfo.position.z,
                                         catRndLinkBolt[lod].segments, catRndLinkBolt[lod].jitter);
            }
        }
    }
    rndLightingEnable(lighting);
    rndTextureEnable(texture);
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndConversionBoxDraw
    Description : the salvage conversion's scan box (the part of cat_rndRenderObjectList 0x00578645..0x00579045
                  drawn in the sorted pass): the ship's bounding box grows (phase 1), holds (2) or shrinks (3);
                  green ticks on its 8 corners with lines to the centre, a cross and ticks on the plane that
                  sweeps along x, and 19 magenta bands on that plane (lit by a bouncing pulse in phase 2, at
                  random in phase 3)
    Inputs      : ship - the ship being converted
----------------------------------------------------------------------------*/
static void cat_rndConversionBoxDraw(Ship *ship)
{
    StaticCollInfo *sci = &ship->staticinfo->common.staticCollInfo;
    bool32 texture, lighting, cull;
    ubyte band[20];
    real32 sweep, scale, tick, half[3], o[3], s[3], c[3], x1, yc, zc, xa, xb, y, z, bandHeight, y0;
    ubyte green;
    sdword i, j, pos, dir, corner;

    texture = rndTextureEnable(FALSE);
    lighting = rndLightingEnable(FALSE);
    cull = (glIsEnabled(GL_CULL_FACE) != 0);

    switch (catRndTimedPhase)
    {
        case 1:
            sweep = 1.0f;
            break;
        case 2:
            sweep = 1.0f - catRndTimedT;
            break;
        case 3:
        default:
            sweep = 0.0f;
            break;
    }

    // the pulse position (truncated as __ftol): (1 * uplength * 36 * T) mod 36 + 1 (0x008bab24 = 1, 0x0072beb0 =
    //      36), folded to bounce between 1 and 19
    pos = (sdword)((real64)(real32)1 * (real64)sci->uplength * 36.0 * (real64)catRndTimedT) % 0x24 + 1;
    dir = 1;
    if (pos > 0x13)
    {
        pos = 0x25 - pos;
        dir = -1;
    }
    switch (catRndTimedPhase)
    {
        case 1:
            memset(band, 0, sizeof(band));
            break;
        case 2:
            memset(band, 0x10, sizeof(band));
            for (i = 0; i < 10; i++)
            {
                band[pos] = (ubyte)(band[pos] + (ubyte)(i * 0xe));
                pos += dir;
                if (pos == 0x14)
                {
                    pos = 0x12;
                    dir = -dir;
                }
                else if (pos == 0)
                {
                    pos = 2;
                    dir = -dir;
                }
            }
            break;
        case 3:
            band[0] = 0;
            band[0x13] = 0;
            for (i = 1; i < 0x13; i++)
            {
                ubyte r = (ubyte)ranRandom(RANDOM_STREAM_0);
                ubyte limit = (ubyte)(sdword)((real64)catRndTimedT * 255.0);           // 0x0072a588
                if (r > limit)
                {
                    ubyte r2 = (ubyte)ranRandom(RANDOM_STREAM_0);
                    ubyte range = (ubyte)(sdword)((real64)(1.0f - catRndTimedT) * 9.0);  // 0x0072beac
                    band[i] = (ubyte)((((r2 % (range + 1)) & 0xff) * 0xff) / 0x11);
                }
                else
                {
                    band[i] = 0;
                }
            }
            break;
    }

    switch (catRndTimedPhase)
    {
        case 1:
            scale = 2.0f - catRndTimedT;
            break;
        case 2:
            scale = 1.0f;
            break;
        case 3:
        default:
            scale = 1.0f - catRndTimedT * 0.25f;
            break;
    }
    if (scale == 1.0f)
    {
        o[0] = sci->collrectoffset.x;
        o[1] = sci->collrectoffset.y - 100.0f;
        o[2] = sci->collrectoffset.z - 100.0f;
        s[0] = sci->uplength;
        s[1] = sci->rightlength + 200.0f;                                      // 0x0072a440
        s[2] = sci->forwardlength + 200.0f;
    }
    else
    {
        half[0] = sci->uplength * 0.5f;
        half[1] = sci->rightlength * 0.5f;
        half[2] = sci->forwardlength * 0.5f;
        c[0] = half[0] * scale;
        c[1] = half[1] * scale + 100.0f;
        c[2] = half[2] * scale + 100.0f;
        o[0] = half[0] + sci->collrectoffset.x - c[0];
        o[1] = half[1] + sci->collrectoffset.y - c[1];
        o[2] = half[2] + sci->collrectoffset.z - c[2];
        s[0] = c[0] + c[0];
        s[1] = c[1] + c[1];
        s[2] = c[2] + c[2];
    }
    switch (catRndTimedPhase)
    {
        case 1:
            tick = (1.0f - catRndTimedT) * 100.0f + 15.0f;                       // 0x0072ab44
            green = (ubyte)(sdword)((real64)catRndTimedT * 252.0);                // 0x0072bea8
            break;
        case 2:
            tick = 15.0f;
            green = 0xfc;
            break;
        case 3:
        default:
            tick = 15.0f;
            green = 0;
            break;
    }

    glDisable(GL_ALPHA_TEST);
    rndAdditiveBlends(TRUE);
    glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glBegin(GL_LINES);
    for (corner = 0; corner < 8; corner++)
    {
        if (green == 0)
        {
            continue;
        }
        c[0] = ((corner & 4) ? s[0] : 0.0f) + o[0];
        c[1] = ((corner & 2) ? s[1] : 0.0f) + o[1];
        c[2] = ((corner & 1) ? s[2] : 0.0f) + o[2];
        glColor4ub(0, 0, 0, 0xff);
        for (j = 0; j < 3; j++)
        {
            glVertex3f(c[0] + (j == 0 ? tick : 0.0f), c[1] + (j == 1 ? tick : 0.0f), c[2] + (j == 2 ? tick : 0.0f));
            glColor4ub(0, green, 0, 0xff);
            glVertex3f(c[0], c[1], c[2]);
            glVertex3f(c[0], c[1], c[2]);
            glColor4ub(0, 0, 0, 0xff);
            glVertex3f(c[0] - (j == 0 ? tick : 0.0f), c[1] - (j == 1 ? tick : 0.0f), c[2] - (j == 2 ? tick : 0.0f));
        }
        glVertex3f(s[0] * 0.5f + o[0], s[1] * 0.5f + o[1], s[2] * 0.5f + o[2]);
        glColor4ub(0, (ubyte)(green >> 2), 0, 0xff);
        glVertex3f(c[0], c[1], c[2]);
    }
    if (green != 0)
    {
        glColor4ub(0, (ubyte)(green >> 2), 0, 0xff);
        x1 = s[0] * sweep + o[0];
        zc = s[2] * 0.5f + o[2];
        yc = s[1] * 0.5f + o[1];
        for (i = 0; i < 4; i++)
        {
            y = ((i & 2) ? s[1] : 0.0f) + o[1];
            z = ((i & 1) ? s[2] : 0.0f) + o[2];
            glVertex3f(x1, yc, zc);
            glVertex3f(x1, y, z);
        }
        xa = x1 - tick;
        xb = s[0] * sweep + tick + o[0];
        for (i = 0; i < 4; i++)
        {
            y = ((i & 1) ? s[1] : 0.0f) + o[1];
            z = ((i & 2) ? s[2] : 0.0f) + o[2];
            glColor4ub(0, 0, 0, 0xff);
            glVertex3f(xa, y, z);
            glColor4ub(0, green, 0, 0xff);
            glVertex3f(x1, y, z);
            glVertex3f(x1, y, z);
            glColor4ub(0, 0, 0, 0xff);
            glVertex3f(xb, y, z);
        }
    }
    glEnd();

    glBegin(GL_QUADS);
    x1 = s[0] * sweep + o[0];
    bandHeight = s[1] * (1.0f / 19.0f);                                         // 0x0072bea4
    for (i = 0; i < 0x13; i++)
    {
        y0 = (real32)i * s[1] * (1.0f / 19.0f) + o[1];
        glColor4ub(band[i], 0, band[i], 0xff);
        glVertex3f(x1, y0, o[2]);
        glVertex3f(x1, y0, o[2] + s[2]);
        glColor4ub(band[i + 1], 0, band[i + 1], 0xff);
        glVertex3f(x1, bandHeight + y0, o[2] + s[2]);
        glVertex3f(x1, bandHeight + y0, o[2]);
    }
    glEnd();

    rndTextureEnable(texture);
    rndLightingEnable(lighting);
    if (cull)
    {
        glEnable(GL_CULL_FACE);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndShipMeshDraw
    Description : the mesh call of cat_rndRenderObjectList: through the ship's bindings
                  (meshRenderShipHierarchy) or as one mesh (meshRender with the LOD's slots); a NULL state for the
                  Beast, conversion and mimic meshes
----------------------------------------------------------------------------*/
static void cat_rndShipMeshDraw(struct CatMeshState *state, shipbindings *bindings, sdword lod, meshdata *mesh,
                                sdword colorScheme)
{
    if (bindings != NULL)
    {
        meshRenderShipHierarchy(state, bindings, lod, mesh, colorScheme);
    }
    else
    {
        cat_meshRenderState((state != NULL) ? state->level[lod] : NULL, mesh, colorScheme);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndRenderObjectList (Cataclysm 0x005776f0)
    Description : Render one object of the main view's render list. Ships pass through up to three passes of the
                  same LOD: the cloak's fade and wireframe, the build-in clip plane and its flipped half, the
                  Beast-stun change to the Beast mesh, the dissolve cut plane, the salvage conversion (clip plane
                  in the opaque pass, scan box in the sorted pass), the mimic's crossfade to its disguise. Objects
                  that need blending (fading, stunned, mimicking, converting, crystal, at a slipgate) go to the
                  depth-sorted list during the opaque pass (cat_rndSortObjectsByDepth mode 1) and are drawn by the
                  sorted pass (mode 2); effects always wait for it.
    Inputs      : camera - the main camera; obj - the object; notEffect - nonzero skips an effect in the sorted pass
    Outputs     : catShCurrentObject and the render state flags
    Return      :
----------------------------------------------------------------------------*/
void cat_rndRenderObjectList(Camera *camera, SpaceObj *obj, sdword notEffect)
{
    Ship *ship = (Ship *)obj;
    Ship *host;
    lod *level;
    hmatrix coordMatrixForGL;
    GLdouble plane[4];
    bool32 deferred, renderNow, displayEffect = FALSE, rendered, again, cloakFlag;
    real32 cloakFade, fade;
    sdword pass, i, colorScheme, playerIndex;
    ShipSinglePlayerGameInfo *ssinfo;
    CatRndFieldNode *field;
    ShipStaticInfo *beastStatic;
    lightning *l;
    udword drawLOD;                                 // HWI-FEATURE(full-detail): the level drawn (cat_lodDrawLevel)

    cat_rndSetupObjectFlags(obj);
    catRndAsteroidTint = FALSE;
    g_WireframeHack = FALSE;
    meshSetFade(7, 0.0f);

    switch (obj->header.objtype)
    {
        case OBJ_ShipType:
            deferred = FALSE;
            renderNow = TRUE;
            // CAT: the mimic test reads the "disguise on" copy (0x00c32c98) before this object sets it further down
            //      (0x00577fcf): the value the previous mimic drawn left, as the exe
            if (catRndBeastStun || catRndFadeFlag || (catRndMimic && !catRndMimicOn))
            {
                deferred = TRUE;
                renderNow = FALSE;
            }
            if (catRndTimedPhase != 0)
            {
                deferred = TRUE;
                renderNow = TRUE;
            }
            if (cat_shipIsCrystal(ship))
            {
                deferred = TRUE;
                renderNow = FALSE;
            }
            host = cat_rndShipOrClampHost(ship);
            if (host->dockvars.dockship != NULL && !host->staticinfo->externalBuild &&
                host->dockvars.dockship->shiptype >= dSlipgateSmall && host->dockvars.dockship->shiptype <= dSlipgateLarge)
            {
                deferred = TRUE;
                renderNow = FALSE;
            }
            if (catRndSortMode != 2 && deferred)
            {
                cat_rndSortListAdd(obj);
                if (!renderNow)
                {
                    break;
                }
            }
            if (ship->staticinfo->common.LOD == NULL)
            {
                ship->impactable.collInfo.selCircleRadius = -1.0f;
                break;
            }
            if (cat_shipIsCloakedFully(ship) && ship->playerowner != universe.curPlayerPtr &&
                !cat_shipDetectedByPlayer(ship, universe.curPlayerPtr->playerIndex))
            {
                break;
            }
            playerIndex = ship->playerowner->playerIndex;
            if (ship->shiptype == sACV || ship->shiptype == bACV)
            {
                cat_rndLinkLightningDraw(ship);
            }
            if (ship->shiptype == dNomadMoon)
            {
                cat_nomadMoonRenderBeam(ship);
            }
            cat_dreadnoughtRenderCharge(ship);
            if ((ship->shiptype == sSentinel || ship->shiptype == bSentinel) && ship->command != NULL &&
                bitTest(ship->command->ordertype.attributes, COMMAND_MASK_FORMATION) &&
                (ship->command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION ||
                 ship->command->formation.formationtype == FORCEFIELD_WALL_FORMATION))
            {
                // CAT: each force-field formation is queued once per frame (0x00b378a0)
                for (field = NULL, i = 0; ; )
                {
                    Node *node;
                    for (node = catRndFieldList.head; node != NULL; node = node->next)
                    {
                        field = (CatRndFieldNode *)listGetStructOfNode(node);
                        if (field->formation == ship->command)
                        {
                            break;
                        }
                    }
                    if (node != NULL)
                    {
                        if (!field->queued)
                        {
                            cat_sentinelFieldQueue(camera, ship->command);
                            field->queued = TRUE;
                        }
                    }
                    else
                    {
                        field = memAlloc(sizeof(CatRndFieldNode), "catRndField", 0);
                        listAddNode(&catRndFieldList, &field->link, field);
                        field->formation = ship->command;
                        field->queued = TRUE;
                        cat_sentinelFieldQueue(camera, ship->command);
                    }
                    break;
                }
            }

            glPushMatrix();
            level = cat_lodDrawLevelGet(ship, &drawLOD);    // HWI-FEATURE(full-detail): LOD 0 under FULL DETAIL (the exe: cat_lodCurrentLevelGet)
            if (cat_taskRealTimeElapsed - ship->targetable.flashtimer < FLASH_TIMER)
            {
                g_ReplaceHack = TRUE;                   // CAT: the RGL glPixelTransferf flash is not drawn (RGL)
            }
            switch (level->flags & LM_LODType)
            {
                case LT_Invalid:
                case LT_NULL:
                    break;
                case LT_Mesh:
                    displayEffect = hsShouldDisplayEffect(ship);
                    hmatMakeHMatFromMat(&coordMatrixForGL, &ship->rotatable.rotinfo.coordsys);
                    hmatPutVectIntoHMatrixCol4(obj->posinfo.position, coordMatrixForGL);
                    glMultMatrixf((GLfloat *)&coordMatrixForGL);
                    if (ship->staticinfo->shipclass == CLASS_Component && ((ModuleShipSpec *)ship->ShipSpecifics)->spinning)
                    {
                        // CAT: an ejected module spins about its collision sphere centre (0x00577d92)
                        vector *centre = &ship->staticinfo->common.staticCollInfo.collsphereoffset;
                        ModuleShipSpec *spec = (ModuleShipSpec *)ship->ShipSpecifics;
                        glTranslatef(centre->x, centre->y, centre->z);
                        glRotatef(spec->spinAngle * 57.295776f, spec->spinAxis.x, spec->spinAxis.y, spec->spinAxis.z);
                        glTranslatef(-centre->x, -centre->y, -centre->z);
                    }
                    shPushLightMatrix(&coordMatrixForGL);
                    if (ship->staticinfo->scaleCap != 0.0f)
                    {
                        // CAT: the N-LIPS scale rndShipVisibleUsingCoordSys computed (Ship.magnitudeSquared)
                        glEnable(GL_RESCALE_NORMAL);
                        rndNormalization = TRUE;
                        glScalef(ship->magnitudeSquared, ship->magnitudeSquared, ship->magnitudeSquared);
                    }
                    colorScheme = ship->colorScheme;
                    pass = 0;
                    plane[0] = 0.0;
                    plane[1] = 0.0;
                    plane[2] = -1.0;
                    plane[3] = -1.0;
                    rendered = rndFade(obj, camera);
                    ssinfo = ship->shipSinglePlayerGameInfo;
                    cat_lodTimingStart(level);
                    if (ssinfo != NULL && ssinfo->shipHyperspaceState == SHIPHYPERSPACE_NONE)
                    {
                        ssinfo = NULL;
                    }
                    cat_rndSetupObjectEffectParamsInline(ship);
                    do
                    {
                        cloakFade = 0.0f;
                        again = FALSE;
                        meshSetFade(6, cat_salvageTargetHasBeam(ship) ? cat_salvageDepotSlotProgress(ship) : 0.0f);
                        meshSetFade(2, cat_shipIsCrystal(ship) ? CAT_RND_CRYSTAL_FADE : 0.0f);
                        if (obj->header.objtype == OBJ_ShipType)
                        {
                            // CAT: a ship docking or launching fades in or out over 1.5 s (a module by its host)
                            host = cat_rndShipOrClampHost(ship);
                            if (host->command != NULL && host->command->ordertype.order == COMMAND_DOCK &&
                                (host->dockvars.dockstate == 0x10 || host->dockvars.dockstate == 0xd ||
                                 host->dockvars.dockstate == 0x11 || host->dockvars.dockstate == 0xe))
                            {
                                fade = (universe.totaltimeelapsed - host->catDockStateTime) * (2.0f / 3.0f);
                                if (!(fade < 1.0f))
                                {
                                    fade = 1.0f;
                                }
                                else if (fade < 0.0f)
                                {
                                    fade = 0.0f;
                                }
                                meshSetFade(7, (host->dockvars.dockstate == 0x10 || host->dockvars.dockstate == 0xd) ?
                                               fade : 1.0f - fade);
                            }
                        }
                        cloakFlag = cat_cloakGetFadeState(ship, &cloakFade);
                        if (cloakFlag)
                        {
                            if (pass == 0)
                            {
                                meshSetFade(5, cloakFade);
                                if (cloakFade > 0.1f)
                                {
                                    again = TRUE;
                                }
                            }
                            else
                            {
                                meshSetFade(5, 0.0f);
                                g_WireframeHack = TRUE;
                            }
                        }
                        else
                        {
                            meshSetFade(5, 0.0f);
                            if (cat_shipIsCloakedFully(ship) && ship->playerowner == universe.curPlayerPtr)
                            {
                                g_WireframeHack = TRUE;
                            }
                        }
                        if (catRndFadeFlag && catRndFadeMode == CAT_RND_FADE_CLIP)
                        {
                            if (!catRndFadeFlip)
                            {
                                plane[3] = (GLdouble)(ship->staticinfo->common.staticCollInfo.forwardlength *
                                                      ((Ship *)catShCurrentObject)->catBuildProgress +
                                                      ship->staticinfo->common.staticCollInfo.collrectoffset.z);
                            }
                            else
                            {
                                plane[2] = -plane[2];
                                plane[3] = -plane[3];
                                if (CAT_RND_RGLTYPE == 0)
                                {
                                    g_WireframeHack = TRUE;
                                    g_HiddenRemoval = FALSE;
                                }
                            }
                            glClipPlane(GL_CLIP_PLANE0, plane);
                            if (!catRndFadeFlip)
                            {
                                glEnable(GL_CLIP_PLANE0);
                            }
                        }
                        if (catRndBeastStun)
                        {
                            if (pass == 0)
                            {
                                if (catRndBeastStunT == 1.0f || CAT_RND_RGLTYPE == 0)
                                {
                                    rendered = TRUE;
                                }
                            }
                            else if (pass == 1)
                            {
                                // the Beast version of the ship over it, 1% larger
                                catRndRaceTint = TRUE;
                                if (CAT_RND_RGLTYPE != 0)
                                {
                                    meshSetFade(3, 1.0f - catRndBeastStunT);
                                }
                                beastStatic = GetShipStaticInfo(cat_beastGetAttackStaticInfo(ship)->convertTo, R2);
                                meshRenders++;
                                if (!nisIsRunning && !bitTest(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP) &&
                                    rndCameraInside(obj, camera))
                                {
                                    g_WireframeHack = TRUE;
                                    g_HiddenRemoval = FALSE;
                                }
                                glScalef(1.01f, 1.01f, 1.01f);                  // 0x3f8147ae
                                cat_rndShipMeshDraw(NULL, ship->catBeastBindings, drawLOD,     // HWI-FEATURE(full-detail): drawLOD (the exe: currentLOD)
                                                    (meshdata *)beastStatic->common.LOD->level[drawLOD].pData,   // HWI-FEATURE(full-detail)
                                                    ship->catBeastColor);
                                meshSetFade(3, 0.0f);
                                rendered = TRUE;
                            }
                        }
                        if (catRndDissolve)
                        {
                            real32 dissolvePlane[4];
                            cat_shipDissolveComputePlane(ship, dissolvePlane);
                            plane[0] = (GLdouble)dissolvePlane[0];
                            plane[1] = (GLdouble)dissolvePlane[1];
                            plane[2] = (GLdouble)dissolvePlane[2];
                            plane[3] = (GLdouble)dissolvePlane[3];
                            glClipPlane(GL_CLIP_PLANE0, plane);
                            glEnable(GL_CLIP_PLANE0);
                            glDisable(GL_CULL_FACE);
                        }
                        if (catRndTimedPhase != 0)
                        {
                            if (catRndSortMode == 1)
                            {
                                // the opaque pass: the ship cut at the scan plane, then the converted ship's half
                                if (catRndTimedPhase == 2 && pass == 0)
                                {
                                    plane[0] = -1.0;
                                    plane[1] = 0.0;
                                    plane[2] = 0.0;
                                    plane[3] = (GLdouble)((1.0f - catRndTimedT) * ship->staticinfo->common.staticCollInfo.uplength +
                                                          ship->staticinfo->common.staticCollInfo.collrectoffset.x);
                                    glClipPlane(GL_CLIP_PLANE0, plane);
                                    glEnable(GL_CLIP_PLANE0);
                                }
                                else if ((catRndTimedPhase == 2 && pass == 1) || catRndTimedPhase == 3)
                                {
                                    ShipStaticInfo *convertStatic;
                                    catRndRaceTint = FALSE;
                                    convertStatic = GetShipStaticInfo(cat_salvageConvertType(ship, ship->catSalvageDepot),
                                                                      ship->shiprace);
                                    if (catRndTimedPhase == 2)
                                    {
                                        plane[3] = -plane[3];
                                        plane[0] = -plane[0];
                                        glClipPlane(GL_CLIP_PLANE0, plane);
                                    }
                                    meshRenders++;
                                    if (!nisIsRunning && !bitTest(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP) &&
                                        rndCameraInside(obj, camera))
                                    {
                                        g_WireframeHack = TRUE;
                                        g_HiddenRemoval = FALSE;
                                    }
                                    cat_rndShipMeshDraw(NULL, ship->catSalvageConvertBindings, drawLOD,   // HWI-FEATURE(full-detail): drawLOD
                                                        (meshdata *)convertStatic->common.LOD->level[drawLOD].pData,   // HWI-FEATURE(full-detail)
                                                        ship->catSalvageConvertColor);
                                    rendered = TRUE;
                                }
                            }
                            else
                            {
                                cat_rndConversionBoxDraw(ship);
                                rendered = TRUE;
                            }
                        }

                        meshSetFade(4, 0.0f);
                        if (catRndMimic)
                        {
                            lodinfo *mimicLOD = catRndMimicLOD;
                            sdword mimicLevel = (sdword)drawLOD;        // HWI-FEATURE(full-detail): drawLOD (the exe: currentLOD)
                            meshdata *mimicMesh;
                            if (mimicLevel >= mimicLOD->nLevels)
                            {
                                mimicLevel = mimicLOD->nLevels - 1;
                            }
                            mimicMesh = (meshdata *)mimicLOD->level[mimicLevel].pData;
                            if (mimicMesh == NULL)
                            {
                                mimicLevel--;
                                mimicMesh = (meshdata *)mimicLOD->level[mimicLevel].pData;
                            }
                            if (catRndMimicOn)
                            {
                                if (catRndMimicRace == RACE_Beast)
                                {
                                    catRndRaceTint = TRUE;
                                }
                                cat_rndShipMeshDraw(NULL, catRndMimicBindings, mimicLevel, mimicMesh, catRndMimicColor);
                                meshRenders++;
                                rendered = TRUE;
                                catRndRaceTint = FALSE;
                            }
                            else
                            {
                                // the crossfade: its own mesh fading out, the disguise warping in
                                if (catRndMimicFade > 0.25f)
                                {
                                    meshSetFade(4, 1.0f - catRndMimicFade);
                                    cat_rndShipMeshDraw(NULL, ship->bindings, drawLOD, (meshdata *)level->pData,   // HWI-FEATURE(full-detail): drawLOD
                                                        colorScheme);
                                    meshRenders++;
                                }
                                meshSetFade(4, catRndMimicFade);
                                if (catRndMimicRace == RACE_Beast)
                                {
                                    catRndRaceTint = TRUE;
                                }
                                if (catRndMimicBindings != NULL)
                                {
                                    cat_meshRenderShipHierarchyWarped(NULL, catRndMimicBindings, mimicLevel, mimicMesh,
                                                                      &ship->staticinfo->common.staticCollInfo.collsphereoffset,
                                                                      catRndMimicFade, catRndMimicRadius, catRndMimicColor);
                                }
                                else
                                {
                                    cat_meshObjectRenderWarped(NULL, &mimicMesh->object[0], mimicMesh->localMaterial,
                                                               &ship->staticinfo->common.staticCollInfo.collsphereoffset,
                                                               catRndMimicFade, catRndMimicRadius, catRndMimicColor);
                                }
                                meshRenders++;
                                meshSetFade(4, 0.0f);
                                rendered = TRUE;
                                catRndRaceTint = FALSE;
                            }
                        }
                        else if (!rendered)
                        {
                            meshRenders++;
                            if (!nisIsRunning && !bitTest(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP) &&
                                rndCameraInside(obj, camera))
                            {
                                g_WireframeHack = TRUE;
                                g_HiddenRemoval = FALSE;
                            }
                            if (ssinfo != NULL && ssinfo->hsState != HS_INACTIVE && ssinfo->hsState != HS_FINISHED)
                            {
                                if (ssinfo->staticHyperspaceGate)
                                {
                                    hsNoGate(TRUE);
                                }
                                hsContinue(ship, displayEffect);
                                if (ssinfo->staticHyperspaceGate)
                                {
                                    hsNoGate(FALSE);
                                }
                            }
                            // lighting while docking (a module by its host)
                            host = cat_rndShipOrClampHost(ship);
                            if (host->dockvars.dockship != NULL && !host->staticinfo->externalBuild)
                            {
                                Ship *dockship = host->dockvars.dockship;
                                real32 nearVal = dockship->staticinfo->dockLightNear;
                                real32 farVal = dockship->staticinfo->dockLightFar;
                                real32 t = rndDockScalar(host, dockship, nearVal, farVal);
                                if (host->dockvars.catDockPointIndex != -1)
                                {
                                    switch (dockship->staticinfo->dockStaticInfo->dockstaticpoints[host->dockvars.catDockPointIndex].type)
                                    {
                                        case 0: case 1: case 2: case 3: case 6: case 7: case 8: case 9: case 10:
                                        case 0xb: case 0x15: case 0x16: case 0x21: case 0x22:
                                            shDockLightColor(dockship->staticinfo->dockLightColor);
                                            shDockLight(t);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }
                            if (ssinfo == NULL || ssinfo->hsState != HS_FINISHED || singlePlayerGameInfo.hyperspaceFails)
                            {
                                cat_rndShipMeshDraw(ship->obj.catMeshState, ship->bindings, drawLOD,   // HWI-FEATURE(full-detail): drawLOD
                                                    (meshdata *)level->pData, colorScheme);
                                g_SpecificPoly = FALSE;     // CAT: 0x00a4c44c cleared after the mesh
                                g_WireframeHack = FALSE;
                                // HWI-FEATURE(full-detail): the exe's level, not the one drawn: KAS RenderedShips /
                                //      RenderedDerelicts and the AI's aitCheckAmIBeingWatched read these bits; and only
                                //      when the exe's level is a mesh (the exe sets the bit in this mesh path only)
                                //      [audit: M3-R1 full-detail: the guard added; a non-mesh exe level drawn at LOD 0
                                //      set a bit the exe never sets]
                                if ((cat_lodCurrentLevelGet(ship)->flags & LM_LODType) == LT_Mesh)   // HWI-FEATURE(full-detail)
                                {                                                                   // HWI-FEATURE(full-detail)
                                    obj->renderedLODs |= (ubyte)(1 << obj->currentLOD);
                                }                                                                   // HWI-FEATURE(full-detail)
                                if (pass == 0 && (!deferred || !renderNow || catRndSortMode == 2))
                                {
                                    RenderNAVLights(ship);
                                }
                                if (catRndResetHacksAfterShip)
                                {
                                    g_WireframeHack = FALSE;
                                    g_HiddenRemoval = TRUE;
                                }
                            }
                            shDockLight(0.0f);
                        }

                        // the next pass?
                        if (catRndBeastStun && pass == 0 && catRndBeastStunT > 0.0f)
                        {
                            again = TRUE;
                        }
                        if (catRndFadeFlag && catRndFadeMode == CAT_RND_FADE_CLIP)
                        {
                            if (pass == 0)
                            {
                                catRndFadeFlip = TRUE;
                                again = TRUE;
                            }
                            else
                            {
                                glDisable(GL_CLIP_PLANE0);
                            }
                        }
                        if (catRndDissolve)
                        {
                            glEnable(GL_CULL_FACE);
                            glDisable(GL_CLIP_PLANE0);
                        }
                        if (catRndTimedPhase == 2 && catRndSortMode == 1)
                        {
                            if (pass == 0)
                            {
                                again = TRUE;
                            }
                            else
                            {
                                glDisable(GL_CLIP_PLANE0);
                            }
                        }
                        pass++;
                    }
                    while (again);

                    meshSetFade(0, 0.0f);
                    cat_lodTimingStop(level);
                    shPopLightMatrix();
                    glDisable(GL_RESCALE_NORMAL);
                    rndNormalization = FALSE;
                    if (cat_shipHasUpgrade(ship, CAT_UPGRADE_RepairDroids) && enableRepairbots &&
                        ship->staticinfo->shiprace == R1 && ship->catRepairDroids != NULL && drawLOD < 3)   // HWI-FEATURE(full-detail): drawLOD
                    {
                        cat_droidRenderArray((CatRepairDroidArray *)ship->catRepairDroids);
                    }
                    break;
                case LT_SubPixel:
                    rndTextureEnable(FALSE);
                    rndLightingEnable(FALSE);
                    primPointSize3(&obj->posinfo.position, ship->impactable.collInfo.selCircleRadius,
                                   toPlayerColor[playerIndex]);
                    rndLightingEnable(TRUE);
                    break;
                default:
                    dbgFatalf(DBG_Loc, "Unsupported LOD type 0x%x", level->flags);
                    break;
            }
            g_ReplaceHack = FALSE;
            for (i = 0; i < 2; i++)
            {
                l = (lightning *)ship->lightning[i];
                if (l != NULL)
                {
                    cloudRenderAndUpdateLightning(l, obj->currentLOD);
                    l->countdown--;
                    if (l->countdown == 0)
                    {
                        cloudKillLightning(l);
                        ship->lightning[i] = NULL;
                    }
                }
            }
            // HWI-FEATURE(full-detail): the Beast-stun edge lightning stays on the exe's level: each bolt is a particle
            //      whose segments draw stream 0 (cat_partRenderEBoltSystem), and the bolts count the level's polygons
            //      and segments, so a LOD-0 lightning would move stream 0 (Mission04 / 10 / 14). With FULL DETAIL
            //      drawing another level it is placed by the objects' rest matrices (the exe's level's animated
            //      binding matrices are only refreshed when that level is drawn)
            if (catRndEdgeEffect && drawLOD != obj->currentLOD)                         // HWI-FEATURE(full-detail)
            {                                                                           // HWI-FEATURE(full-detail)
                lod *edgeLevel = cat_lodCurrentLevelGet(ship);                          // HWI-FEATURE(full-detail)
                if ((edgeLevel->flags & LM_LODType) == LT_Mesh && edgeLevel->pData != NULL)   // HWI-FEATURE(full-detail)
                {                                                                       // HWI-FEATURE(full-detail)
                    cat_meshRenderEdgeEffect(ship, (meshdata *)edgeLevel->pData);       // HWI-FEATURE(full-detail)
                }                                                                       // HWI-FEATURE(full-detail)
            }                                                                           // HWI-FEATURE(full-detail)
            else if (catRndEdgeEffect && level->pData != NULL)                          // HWI-FEATURE(full-detail): "else" (the exe: if)
            {
                if (ship->bindings == NULL)
                {
                    cat_meshRenderEdgeEffect(ship, (meshdata *)level->pData);
                }
                else
                {
                    cat_meshRenderShipHierarchyEdgeEffect(ship, (meshdata *)level->pData);
                }
            }
            glPopMatrix();
            ssinfo = ship->shipSinglePlayerGameInfo;
            if (ssinfo != NULL && ssinfo->hsState != HS_INACTIVE && ssinfo->hsState != HS_FINISHED)
            {
                if (ssinfo->staticHyperspaceGate)
                {
                    hsNoGate(TRUE);
                }
                hsEnd(ship, displayEffect);
                if (ssinfo->staticHyperspaceGate)
                {
                    hsNoGate(FALSE);
                }
            }
            catRndCounts.ships++;
            break;

        case OBJ_BulletType:
        {
            Bullet *bullet = (Bullet *)obj;
            vector to;

            if (bullet->effect != NULL)
            {
                break;
            }
            rndLightingEnable(FALSE);
            rndTextureEnable(FALSE);
            // CAT: bullet type 4 is the HW1 defense fighter's laser (cat_defenseFighterRenderBeams 0x005b04a0 moves
            //      it onto its owner's gun): the type is dropped (M2-R4 ships-b), the line is drawn as is
            if (bullet->bulletType != CAT_BULLET_TYPE_8 &&
                !(bullet->bulletType == BULLET_Beam && bullet->timelived <= UNIVERSE_UPDATE_PERIOD))
            {
                vecAdd(to, obj->posinfo.position, bullet->lengthvec);
                primLine3(&obj->posinfo.position, &to, bullet->bulletColor);
            }
            rndLightingEnable(TRUE);
            catRndCounts.bullets++;
            break;
        }

        case OBJ_EffectType:
        {
            Effect *effect = (Effect *)obj;
            SpaceObj *owner = (SpaceObj *)effect->owner;

            if (catRndSortMode != 2)
            {
                cat_rndSortListAdd(obj);                // effects are drawn in the sorted pass
                break;
            }
            if (notEffect)
            {
                break;
            }
            if (owner != NULL && owner->header.objtype == OBJ_BulletType)
            {
                Bullet *bullet = (Bullet *)owner;
                if (bullet->bulletType == CAT_BULLET_TYPE_8)
                {
                    etgEffectUpdate(effect, 0.0f);
                    etgEffectDraw(effect);
                    catRndCounts.effects++;
                    break;
                }
                if (bullet->bulletType == BULLET_Beam && bullet->timelived <= UNIVERSE_UPDATE_PERIOD)
                {
                    break;
                }
            }
            etgEffectDraw(effect);
            catRndCounts.effects++;
            break;
        }

        case OBJ_DerelictType:
            if (((DerelictStaticInfo *)obj->staticinfo)->worldRender || ((Derelict *)obj)->derelicttype == HyperspaceGate)
            {
                break;
            }
            // fall through
        case OBJ_AsteroidType:
        case OBJ_GasType:
        case OBJ_DustType:
        case OBJ_MissileType:
        {
            SpaceObjRotImpTarg *targetable = (SpaceObjRotImpTarg *)obj;

            if (obj->staticinfo->LOD == NULL)
            {
                break;
            }
            glPushMatrix();
            level = cat_lodDrawLevelGet(obj, &drawLOD);     // HWI-FEATURE(full-detail): LOD 0 under FULL DETAIL (the exe: cat_lodCurrentLevelGet)
            if (cat_taskRealTimeElapsed - targetable->flashtimer < FLASH_TIMER)
            {
                g_ReplaceHack = TRUE;
            }
            switch (level->flags & LM_LODType)
            {
                case LT_Invalid:
                case LT_NULL:
                    break;
                case LT_Mesh:
                    g_RndPosition = obj->posinfo.position;
                    hmatMakeHMatFromMat(&coordMatrixForGL, &targetable->rotatable.rotinfo.coordsys);
                    hmatPutVectIntoHMatrixCol4(obj->posinfo.position, coordMatrixForGL);
                    glMultMatrixf((GLfloat *)&coordMatrixForGL);
                    shPushLightMatrix(&coordMatrixForGL);
                    if (obj->header.objtype == OBJ_AsteroidType)
                    {
                        real32 scaling = ((Asteroid *)obj)->scaling;
                        if (scaling != 1.0f)
                        {
                            glScalef(scaling, scaling, scaling);
                        }
                        if (bitTest(obj->attributes, CAT_RND_ATTR_ASTEROID_FAR))
                        {
                            catRndAsteroidTint = TRUE;
                        }
                    }
                    colorScheme = 0;
                    if (obj->header.objtype == OBJ_GasType || obj->header.objtype == OBJ_DustType)
                    {
                        cat_lodTimingStart(level);
                        rndFade(obj, camera);
                        meshRenders++;
                        cloudRenderSystem(level->pData, ((DustCloud *)obj)->stub, (sdword)drawLOD);   // HWI-FEATURE(full-detail): drawLOD
                        meshSetFade(0, 0.0f);
                        cat_lodTimingStop(level);
                        shPopLightMatrix();
                        catRndCounts.clouds++;
                        break;
                    }
                    if (obj->header.objtype == OBJ_MissileType)
                    {
                        colorScheme = ((Missile *)obj)->colorScheme;
                    }
                    else if (obj->header.objtype == OBJ_DerelictType)
                    {
                        colorScheme = ((Derelict *)obj)->colorScheme;
                    }
                    cat_lodTimingStart(level);
                    rndFade(obj, camera);
                    meshRenders++;
                    if (obj->header.objtype == OBJ_MissileType && ((MissileStaticInfo *)obj->staticinfo)->scaleCap != 0.0f)
                    {
                        real32 s = ((Missile *)obj)->catMissileScale;
                        glEnable(GL_RESCALE_NORMAL);
                        rndNormalization = TRUE;
                        glScalef(s, s, s);
                    }
                    cat_meshRenderState(NULL, (meshdata *)level->pData, colorScheme);
                    // HWI-FEATURE(full-detail): the exe's level (KAS RenderedDerelictType reads it), and only when the exe's
                    //      level is a mesh: the planet derelicts' level 1 is SubPixel (derelicts/*.lod), which the exe
                    //      draws as a point and marks nothing [audit: M3-R1 full-detail: the guard added]
                    if ((cat_lodCurrentLevelGet(obj)->flags & LM_LODType) == LT_Mesh)                   // HWI-FEATURE(full-detail)
                    {                                                                                   // HWI-FEATURE(full-detail)
                        obj->renderedLODs |= (ubyte)(1 << obj->currentLOD);
                    }                                                                                   // HWI-FEATURE(full-detail)
                    meshSetFade(0, 0.0f);
                    cat_lodTimingStop(level);
                    shPopLightMatrix();
                    if (bitTest(obj->attributes, CAT_RND_ATTR_ASTEROID_FAR))
                    {
                        catRndAsteroidTint = FALSE;
                    }
                    catRndCounts.objects++;
                    break;
                case LT_SubPixel:
                    rndTextureEnable(FALSE);
                    rndLightingEnable(FALSE);
                    primPointSize3(&obj->posinfo.position, targetable->impactable.collInfo.selCircleRadius,
                                   obj->staticinfo->LOD->pointColor);
                    rndLightingEnable(TRUE);
                    break;
                default:
                    dbgFatalf(DBG_Loc, "Unsupported LOD type 0x%x", level->flags);
                    break;
            }
            g_ReplaceHack = FALSE;
            glPopMatrix();
            break;
        }

        case OBJ_NebulaType:
            break;
        default:
            dbgFatalf(DBG_Loc, "Undefined object type %d", obj->header.objtype);
    }
    catShCurrentObject = NULL;
}

/*-----------------------------------------------------------------------------
    Name        : rndMainViewRenderFunction (Cataclysm 0x005764e0)
    Description : Render a mission sphere as referenced by a specific camera.
    Inputs      : camera - pointer to a camera structure which includes a
                    mission sphere.
    Outputs     : ??
    Return      : void
    Note        : CAT: Cataclysm's main view: the visible objects are collected first, their levels of detail
                  chosen (and raised pass by pass while the frame's render time estimate exceeds the budget,
                  catLodTimeThreshold), then drawn through cat_rndRenderObjectList: an opaque pass, the minor
                  render list's points, a depth-sorted pass of the blended objects with the Sentinel fields behind
                  them, then a trail pass, the motion cues, blasts, hyperspace gates, nebulae, the fog layer and the
                  lens flares. Because much of the functionality of this function is mirrored in
                  rndMainViewAllButRenderFunction, any drastic changes to rndMainViewRenderFunction will have to be
                  made to rndMainViewAllButRenderFunction.
----------------------------------------------------------------------------*/
static void catLodTestMainView(LinkedList *visible);       // HWI-TEST (M3-R1 full-detail)
void rndMainViewRenderFunction(Camera *camera)
{
    Node *objnode;
    SpaceObj *spaceobj;
    udword i, numstars;
    Star3d *star;
    LinkedList visible;
    udword pass, total;
    sdword asteroid0Count;
    real32 declination, distScale, offset[3];
    static real32 cameraOffset[3] = {0.0f, 0.0f, 0.0f};             // 0x00a75db0..0x00a75db8
    static real32 floatTime0 = 0.0f, floatTime1 = 0.0f;             // 0x00a75d9c / 0x00a75da0
    static real32 lastFloat0 = 0.0f, lastFloat1 = 0.0f;             // 0x00a75d94 / 0x00a75d98
    static bool32 cameraFloating = FALSE;                           // 0x00a75dbc
    extern sdword trailsRendered;

    mouseCursorObjPtr  = NULL;               //Falko: got an obscure crash where mouseCursorObjPtr was mangled, will this work?
    meshSetFade(3, 0.0f);                    // CAT: the Beast-stun channel starts clear

    dbgAssertOrIgnore(camera != NULL); //verify parameters

    // CAT: the exe returns while a Bink movie plays (binkDonePlaying 0x008b6d94); GoK plays its movies elsewhere
    primModeClear2();                                       //go to 3D rendering mode

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    rgluPerspective(camera->fieldofview, rndAspectRatio,    //set projection matrix
                    rndNear(camera->clipPlaneNear), camera->clipPlaneFar);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // CAT: the camera float (0x0057659a): the offsets scale with the camera's declination and stop with the
    //      universe paused (HW1's rndCameraOffset0/1 inlined)
    if (gameIsRunning && !nisIsRunning && piePointSpecMode == PSM_Idle && !universePause)
    {
        if (!cameraFloating)
        {
            lastFloat0 = cat_taskRealTimeElapsed;
            lastFloat1 = cat_taskRealTimeElapsed;
        }
        cameraFloating = TRUE;
    }
    else
    {
        cameraFloating = FALSE;
        if (nisIsRunning)
        {
            cameraOffset[0] = cameraOffset[1] = cameraOffset[2] = 0.0f;
        }
    }
    if (bitTest(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP))
    {
        cameraFloating = FALSE;
        cameraOffset[0] = cameraOffset[1] = cameraOffset[2] = 0.0f;
    }
    else if (cameraFloating)
    {
        real64 et;
        declination = (real32)(cos((real64)camera->declination) * (real64)0.8f + (real64)0.2f);  // 0x0072a5d4 / 0x0072a510
        if (!universePause)
        {
            floatTime0 += cat_taskRealTimeElapsed - lastFloat0;
        }
        lastFloat0 = cat_taskRealTimeElapsed;
        et = (real64)CAMERA_FLOAT_SCALAR0 * (real64)floatTime0;
        cameraOffset[0] = (real32)((sin(et) - cos(et * 0.5)) * (real64)CAMERA_FLOAT_OFFSET_SCALAR0 * (real64)declination);
        if (!universePause)
        {
            floatTime1 += cat_taskRealTimeElapsed - lastFloat1;
        }
        lastFloat1 = cat_taskRealTimeElapsed;
        et = (real64)CAMERA_FLOAT_SCALAR1 * (real64)floatTime1;
        cameraOffset[1] = (real32)((cos(et * 0.5) + sin(et)) * (real64)CAMERA_FLOAT_OFFSET_SCALAR1 * (real64)declination);
        cameraOffset[2] = cameraOffset[0] * 0.5f + cameraOffset[1] * 0.5f;
    }
    distScale = CAMERA_FLOAT_DIST_SCALAR * camera->distance;
    if (distScale > 1.0f)
    {
        offset[0] = distScale * cameraOffset[0];
        offset[1] = distScale * cameraOffset[1];
        offset[2] = distScale * cameraOffset[2];
    }
    else
    {
        offset[0] = cameraOffset[0];
        offset[1] = cameraOffset[1];
        offset[2] = cameraOffset[2];
    }
    rgluLookAt(camera->eyeposition.x + offset[2],
               camera->eyeposition.y + offset[0],
               camera->eyeposition.z + offset[1],
               camera->lookatpoint.x, camera->lookatpoint.y, camera->lookatpoint.z,
               camera->upvector.x, camera->upvector.y, camera->upvector.z);
    //get local copy of matrices
    glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat *)(&rndCameraMatrix));
    glGetFloatv(GL_PROJECTION_MATRIX, (GLfloat *)(&rndProjectionMatrix));

    //position light(s) in world
    lightPositionSet();

    if (feRenderEverything)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        feRenderEverything = FALSE;
    }

    cat_rndFogLayerUpdate();                                // CAT: the fog layer follows the camera (0x00576808)

//draw the texture-mapped background
#if RND_BACKGROUND_STATIC
    glPushMatrix();
        glTranslatef(camera->eyeposition.x, camera->eyeposition.y, camera->eyeposition.z);
#endif
        primErrorMessagePrint();

        numstars=universe.star3dinfo->Num3dStars;
        if (stararray == NULL)
        {
            c4ub_v3f* ptr;

            stararray = (c4ub_v3f*)memAlloc(numstars * sizeof(c4ub_v3f), "stararray", NonVolatile);

            ptr = stararray;
            for (i = 0, star = universe.star3dinfo->Stars; i < numstars; i++, star++, ptr++)
            {
                ptr->c[0] = star->r;
                ptr->c[1] = star->g;
                ptr->c[2] = star->b;
                ptr->c[3] = 200;
                ptr->v[0] = star->position.x;
                ptr->v[1] = star->position.y;
                ptr->v[2] = star->position.z;
            }

            if (useVBO) {
                glGenBuffers(1, &vboStars);
                glBindBuffer(GL_ARRAY_BUFFER, vboStars);
                glBufferData(GL_ARRAY_BUFFER, numstars * sizeof(c4ub_v3f), stararray, GL_STATIC_DRAW);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                memFree(stararray);
            }
        }

        rndLightingEnable(FALSE);                           //stars are self-illuminated
        primErrorMessagePrint();
        glDisable(GL_DEPTH_TEST);                           //and infinitely far away
        primErrorMessagePrint();
        // CAT: the stars are always drawn (HW1's Karos graveyard exception is gone, 0x0057692e)
        rndBackgroundRender(BG_RADIUS, camera, TRUE);

        rndTextureEnvironment(RTE_Modulate);
        glEnable(GL_DEPTH_TEST);                            //re-enable depth test
        glDepthMask(GL_TRUE);
        primErrorMessagePrint();
        rndLightingEnable(TRUE);                            //and lighting
        primErrorMessagePrint();
#if RND_BACKGROUND_STATIC
    glPopMatrix();
#endif
    lightSetLighting();

    //render all worlds in the universe
    // CAT: only with the backgrounds option on (showBackgrounds 0x008b8624); a world effect keeps the depth test
    //      state it found (cat_rndDepthTestEnable 0x0057ae40)
    if (showBackgrounds)
    {
        for (i = 0; i < UNIV_NUMBER_WORLDS; i++)
        {
            if (universe.world[i] == NULL)
            {
                break;
            }
            if (universe.world[i]->header.objtype == OBJ_DerelictType)
            {
                rndRenderAHomeworld(camera, universe.world[i]);
            }
            else
            {
                rndRenderAWorldEffect(camera, (Effect *)universe.world[i]);
            }
        }
    }
    if (rndPreObjectCallback != NULL)
    {                                                       //render the pre-object callback if needed
        rendercallback rc = rndPreObjectCallback;
        rndPreObjectCallback = NULL;
        rc();
    }

    trailsRendered = 0;
    alodSetPolys(0);

    // CAT: collect the visible objects (0x00576b22): ships of a type below 0x8E not omitted from the render list
    //      and every object but bullets and effects pass the view test (rndShipVisibleUsingCoordSys, which also
    //      sets the selection circle); bullets and effects always go
    listInit(&visible);
    for (objnode = universe.RenderList.head; objnode != NULL; objnode = objnode->next)
    {
        spaceobj = (SpaceObj *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(spaceobj->renderlink.belongto != NULL);
        if (spaceobj->header.objtype == OBJ_ShipType)
        {
            if (((Ship *)spaceobj)->shiptype > CAT_RND_MAX_VISIBLE_SHIPTYPE || spaceobj->header.catOmitFromRenderList)
            {
                continue;
            }
        }
        if (spaceobj->header.objtype != OBJ_BulletType && spaceobj->header.objtype != OBJ_EffectType &&
            !rndShipVisibleUsingCoordSys(spaceobj, camera))
        {
            continue;
        }
        listAddNode(&visible, &spaceobj->catVisibleLink, spaceobj);
    }

    // CAT: the levels of detail (0x00576bac): pass 0 chooses them (lodLevelGet), each further pass raises the ones
    //      at that pass's level while the estimated render time exceeds the budget (at most 10 passes)
    pass = 0;
    do
    {
        total = 0;
        for (objnode = visible.head; objnode != NULL; objnode = objnode->next)
        {
            spaceobj = (SpaceObj *)listGetStructOfNode(objnode);
            switch (spaceobj->header.objtype)
            {
                case OBJ_ShipType:
                case OBJ_AsteroidType:
                case OBJ_GasType:
                case OBJ_DustType:
                case OBJ_DerelictType:
                case OBJ_MissileType:
                    if (spaceobj->staticinfo->LOD != NULL)
                    {
                        if (pass == 0)
                        {
                            total += cat_lodLevelTime(lodLevelGet(spaceobj, &camera->eyeposition,
                                                    &((SpaceObjRotImpTarg *)spaceobj)->impactable.collInfo.collPosition));
                        }
                        else
                        {
                            total += cat_lodLevelGetTimed(spaceobj, (sdword)pass);
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }
    while (!opNoLODVal && total > catLodTimeThreshold && ++pass < 11);
    catLodTestMainView(&visible);                           // HWI-TEST (M3-R1 full-detail): /catLodTest, /catLodStats

    // the opaque pass
    cat_rndSortObjectsByDepth(1);
    for (objnode = visible.head; objnode != NULL; objnode = objnode->next)
    {
        cat_rndRenderObjectList(camera, (SpaceObj *)listGetStructOfNode(objnode), 0);
        catRndCounts.listed++;
    }

    //
    // minor renderlist (asteroid0 list)
    //
    rndTextureEnable(FALSE);
    rndLightingEnable(FALSE);
    g_WireframeHack = FALSE;
    glPointSize(1.0f);
    meshSetFade(0, 0.0f);

    asteroid0Count = 0;
    objnode = universe.MinorRenderList.head;
    while (objnode != NULL)
    {
        spaceobj = (SpaceObj*)listGetStructOfNode(objnode);

        switch (spaceobj->header.objtype)
        {
        case OBJ_AsteroidType:
            asteroid0Data[asteroid0Count].c = spaceobj->staticinfo->LOD->pointColor;
            asteroid0Data[asteroid0Count].position = &spaceobj->posinfo.position;
            asteroid0Count++;
            if (asteroid0Count == ASTEROID0DATA_SIZE)
            {
                rndDrawAsteroid0(asteroid0Count);
                asteroid0Count = 0;
            }
            break;

        default:
            dbgFatalf(DBG_Loc, "MinorRenderList contains invalid object type %d", spaceobj->header.objtype);
        }

        objnode = objnode->next;
    }
    if (asteroid0Count != 0)
    {
        rndDrawAsteroid0(asteroid0Count);
    }
    rndLightingEnable(TRUE);

    // CAT: the sorted pass (0x00576e15): farthest first, each after the Sentinel field segments behind it
    cat_rndSortObjectsByDepth(2);
    while (catRndSortIndex < catRndSortCount)
    {
        spaceobj = catRndSortList[catRndSortIndex++];
        if (spaceobj == NULL)
        {
            break;
        }
        cat_sentinelFieldsRenderBehind(camera, spaceobj);
        cat_rndRenderObjectList(camera, spaceobj, 0);
        catRndCounts.sorted++;
    }
    cat_sentinelFieldsRenderRest();
    cat_rndSortObjectsByDepth(0);

    // CAT: the trail pass (0x00576e8b): ship and missile trails, after every mesh
    if (catRndDrawTrails)
    {
        for (objnode = universe.RenderList.head; objnode != NULL; objnode = objnode->next)
        {
            spaceobj = (SpaceObj *)listGetStructOfNode(objnode);
            if (spaceobj->header.objtype == OBJ_ShipType)
            {
                Ship *ship = (Ship *)spaceobj;
                cat_rndSetupObjectFlags(spaceobj);
                if (!bitTest(spaceobj->header.flags, SOF_Cloaked) &&
                    (!cat_shipIsCloakedFully(ship) || ship->playerowner == universe.curPlayerPtr ||
                     cat_shipDetectedByPlayer(ship, universe.curPlayerPtr->playerIndex)))
                {
                    udword idx;
                    rndFade(spaceobj, camera);
                    for (idx = 0; idx < MAX_NUM_TRAILS; idx++)
                    {
                        if (ship->trail[idx] != NULL)
                        {
                            trailDraw(&ship->enginePosition, ship->trail[idx], spaceobj->currentLOD, ship->colorScheme);
                            catRndCounts.trails++;
                        }
                    }
                }
            }
            else if (spaceobj->header.objtype == OBJ_MissileType)
            {
                Missile *mis = (Missile *)spaceobj;
                cat_rndSetupObjectFlags(spaceobj);
                if (mis->trail != NULL)
                {
                    rndFade(spaceobj, camera);
                    mistrailDraw(&mis->enginePosition, mis->trail, spaceobj->currentLOD, mis->colorScheme);
                    catRndCounts.trails++;
                }
            }
        }
    }
    catShCurrentObject = NULL;
    meshSetFade(0, 0.0f);

    // CAT (M2-R6 nis-fow-camera): the motion cues, when the switch is on (rndMainViewRenderFunction 0x005764e0 at 0x00577360:
    //      cat_mcRender 0x0042c630 with the state 0x00c49400 while catMotionCues 0x008b8844; kasfSetMotionCues and the
    //      console's "cameracues" set it). A requested reset scatters the points with random stream 0.
    if (catMotionCues)
    {
        cat_mcRender(camera, &catMotionCueState);
    }
    cat_blastRenderAll();

    // CAT: forget the force-field formations not queued this frame (0x0057736d)
    {
        Node *node = catRndFieldList.head, *next;
        while (node != NULL)
        {
            CatRndFieldNode *field = (CatRndFieldNode *)listGetStructOfNode(node);
            next = node->next;
            if (field->queued)
            {
                field->queued = FALSE;
            }
            else
            {
                listDeleteNode(node);
            }
            node = next;
        }
    }

    //hyperspace
    hsStaticRender();

    nebRender();
    cat_rndFogLayerRender();                                // CAT (0x005773f8)
    cat_rndLensFlaresRender();                              // CAT (0x005773fd)

    // CAT: measure the seconds per polygon once 20 or more ship levels have been timed (0x00577402)
    if (catLodTimeScale == 0.0f)
    {
        real32 frequency = (real32)(real64)SDL_GetPerformanceFrequency();
        real32 time = 0.0f;
        udword levels = 0, polys = 0;
        for (objnode = universe.RenderList.head; objnode != NULL; objnode = objnode->next)
        {
            spaceobj = (SpaceObj *)listGetStructOfNode(objnode);
            if (spaceobj->header.objtype == OBJ_ShipType)
            {
                lodinfo *info = spaceobj->staticinfo->LOD;
                sdword lv;
                for (lv = 0; lv < info->nLevels; lv++)
                {
                    if (info->level[lv].catTimeMin != 0)
                    {
                        polys += (udword)cat_lodHierarchyCount(&info->level[lv]);
                        levels++;
                        time = (real32)(real64)info->level[lv].catTimeMin + time;
                    }
                }
            }
        }
        if (levels >= 20)
        {
            catLodTimeScale = (time / frequency) / (real32)polys;
            dbgMessagef("LOD timing: %u levels, %u polygons, %g s per polygon", levels, polys, (double)catLodTimeScale);
        }
    }

    if (rndPostObjectCallback != NULL)
    {                                                       //render the post-object callback if needed
        rndPostObjectCallback();
        rndPostObjectCallback = NULL;
    }
    primModeSet2();
    // CAT: the exe has no HW1 debug overlays here (rndPreRenderDebugStuff / rndPostRenderDebug3DStuff /
    //      rndPostRenderDebug2DStuff); its "Pilot View Offset" print (switch 0x008dfbcc, 0 in .data) is a tuning aid
    catRndCounts.frames++;
}

GLuint plug_handle = 0;
sdword plug_width, plug_height;

typedef struct tagTGAHeader
{
    unsigned char idLength;
    unsigned char colorMapType;
    unsigned char imageType;
    // Color Map information.
    unsigned short colorMapStartIndex;
    unsigned short colorMapNumEntries;
    unsigned char colorMapBitsPerEntry;
    // Image Map information.
    signed short imageOffsetX;
    signed short imageOffsetY;
    unsigned short imageWidth;
    unsigned short imageHeight;
    unsigned char pixelDepth;
    unsigned char imageDescriptor;
} TGAHeader;

//rndLoadTarga
udword rndLoadTarga(char* filename, sdword* width, sdword* height)
{
    TGAHeader head;
    ubyte* data;
    ubyte* pdata;
    ubyte* bp;
    uword* psdata;
    sdword i;
    GLuint handle;

    fileLoadAlloc(filename, (void**)&data, 0);

    pdata = data;
    head.idLength = *pdata++;
    head.colorMapType = *pdata++;
    head.imageType = *pdata++;
    psdata = (uword*)pdata;

#if FIX_ENDIAN
    head.colorMapStartIndex = FIX_ENDIAN_INT_16(*psdata);
    pdata += 2;
    psdata = (uword*)pdata;
    head.colorMapNumEntries = FIX_ENDIAN_INT_16(*psdata);
    pdata += 2;
    head.colorMapBitsPerEntry = *pdata++;
    psdata = (uword*)pdata;
    head.imageOffsetX = FIX_ENDIAN_INT_16((sword)*psdata);
    pdata += 2;
    psdata = (uword*)pdata;
    head.imageOffsetY = FIX_ENDIAN_INT_16((sword)*psdata);
    pdata += 2;
    psdata = (uword*)pdata;
    head.imageWidth = FIX_ENDIAN_INT_16(*psdata);
    pdata += 2;
    psdata = (uword*)pdata;
    head.imageHeight = FIX_ENDIAN_INT_16(*psdata);
#else
    // HWI (M2-R7 r6-leftovers): the colour-map words sit at file offsets 3 and 5, so they are read through memcpy (the
    //      same values; the exe reads them in place on x86, UBSan reports a misaligned uword load here). The plug is
    //      this function's first caller in the tree.
    memcpy(&head.colorMapStartIndex, pdata, sizeof(uword));
    pdata += 2;
    psdata = (uword*)pdata;
    memcpy(&head.colorMapNumEntries, pdata, sizeof(uword));
    pdata += 2;
    head.colorMapBitsPerEntry = *pdata++;
    psdata = (uword*)pdata;
    head.imageOffsetX = (sword)*psdata;
    pdata += 2;
    psdata = (uword*)pdata;
    head.imageOffsetY = (sword)*psdata;
    pdata += 2;
    psdata = (uword*)pdata;
    head.imageWidth = *psdata;
    pdata += 2;
    psdata = (uword*)pdata;
    head.imageHeight = *psdata;
#endif

    pdata += 2;
    head.pixelDepth = *pdata++;
    head.imageDescriptor = *pdata++;

    dbgAssertOrIgnore(head.pixelDepth == 32);

    pdata += head.idLength;

    *width  = (sdword)head.imageWidth;
    *height = (sdword)head.imageHeight;

    for (i = 0, bp = pdata; i < 4*(*width)*(*height); i += 4, bp += 4)
    {
        ubyte r, b;
        r = bp[0];
        b = bp[2];
        bp[0] = b;
        bp[2] = r;
        bp[3] = (ubyte)(255 - (int)bp[3]);
    }

    glGenTextures(1, &handle);
    trClearCurrent();
    glBindTexture(GL_TEXTURE_2D, handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, *width, *height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, pdata);

    memFree(data);

    return handle;
}

/*-----------------------------------------------------------------------------
    CAT (M2-R7 r6-leftovers): the shameless plug. HW1's source has no body for it (GoK dropped it); both bodies are
    read from cataclysm.exe's disassembly. The exe keeps the texture handle at 0x00a75dc0 and its size, as
    rndLoadTarga writes it, at 0x00b37800 (width) / 0x00b377a0 (height).
-----------------------------------------------------------------------------*/
static udword rndPlugTexture = 0;                           // CAT: 0x00a75dc0
static sdword rndPlugWidth = 0;                             // CAT: 0x00b37800
static sdword rndPlugHeight = 0;                            // CAT: 0x00b377a0

/*-----------------------------------------------------------------------------
    Name        : rndLoadShamelessPlug
    Description : CAT: rndLoadShamelessPlug 0x00579fa0: loads plug.tga (on) or deletes its texture (off). Every caller
                  in the exe passes FALSE (mainShutdownRenderer, mainLoadGL, mainRestoreRender, ...: the texture
                  frees of mainCloseRender here); rndShamelessPlug loads it on first use.
    Inputs      : on - TRUE loads, FALSE frees
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rndLoadShamelessPlug(bool32 on)
{
    if (on)
    {
        rndPlugTexture = rndLoadTarga("plug.tga", &rndPlugWidth, &rndPlugHeight);
    }
    else if (rndPlugTexture != 0)
    {
        GLuint handle = (GLuint)rndPlugTexture;
        glDeleteTextures(1, &handle);
        rndPlugTexture = 0;
    }
}

/*-----------------------------------------------------------------------------
    Name        : rndShamelessPlug
    Description : CAT: rndShamelessPlug 0x00579ff0: draws plug.tga at its own size against the right edge of the
                  screen, at the bottom (an ortho projection of the window, y up), textured, alpha-blended, without
                  lighting or depth test, and puts the matrices and the states back. The exe's software-renderer
                  branch (RGLtype 0x00b3a700 == 0: glDrawPixels at a raster position) is not in this build (decision
                  28); this is its GL branch (RGLtype != 0).
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rndShamelessPlug(void)
{
    GLint matrixMode;
    GLfloat projection[16];
    real32 width, height, plugWidth, plugHeight, left;
    bool32 blendOn, depthOn, lighting;

    if (rndPlugTexture == 0)
    {
        rndPlugTexture = rndLoadTarga("plug.tga", &rndPlugWidth, &rndPlugHeight);
    }
    plugWidth  = (real32)rndPlugWidth;
    plugHeight = (real32)rndPlugHeight;
    width  = (real32)MAIN_WindowWidth;
    height = (real32)MAIN_WindowHeight;

    glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    rgluOrtho2D(0.0f, width, 0.0f, height);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    blendOn = glIsEnabled(GL_BLEND);
    lighting = rndLightingEnable(FALSE);
    depthOn = glIsEnabled(GL_DEPTH_TEST);
    if (!blendOn)
    {
        glEnable(GL_BLEND);
    }
    if (depthOn)
    {
        glDisable(GL_DEPTH_TEST);
    }

    width -= 1.0f;                                          // the exe's right edge: MAIN_WindowWidth - 1.0
    left = width - plugWidth;
    rndTextureEnable(TRUE);
    rndAdditiveBlends(FALSE);
    trClearCurrent();
    glBindTexture(GL_TEXTURE_2D, rndPlugTexture);
    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(left, plugHeight);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(left, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(width, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(width, plugHeight);
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projection);
    if (matrixMode == GL_MODELVIEW)
    {
        glMatrixMode(GL_MODELVIEW);
    }
    rndLightingEnable(lighting);
    if (!blendOn)
    {
        glDisable(GL_BLEND);
    }
    if (depthOn)
    {
        glEnable(GL_DEPTH_TEST);
    }
}

extern udword receivedPacketNumber;

static udword printLagMinFrames = 0;

extern udword PRINTLAG_IFGREATERTHAN;
extern udword PRINTLAG_MINFRAMES;

/*-----------------------------------------------------------------------------
    Name        : rndDrawOnScreenSyncStatus
    Description : Prints sync status to screen
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rndDrawOnScreenSyncStatus(void)
{
#if SYNC_CHECK
    if ((multiPlayerGame|playPackets) && (gameIsRunning))
    {
        sdword y;
        udword rsyncpkts = queueNumberEntries(ProcessSyncPktQ);

        if (randSyncErr)
        {
            fontPrintf(400,10,colWhite,"RAND SYNC ERR: Frame %d",randSyncErrFrame);
        }
        if (univSyncErr)
        {
            fontPrintf(400,20,colWhite,"UNIV SYNC ERR: Frame %d",univSyncErrFrame);
        }
        if ((blobSyncErr) && (multiPlayerGame)) // only print cheat detect in multiPlayerGame
        {
            fontPrintf(400,30,colWhite,strGetString(strCheatDetect),blobSyncErrFrame);
        }
        if (pktSyncErr)
        {
            fontPrint(400,40,colWhite,"PKT SYNC ERR");
        }

        y = 50;

        if ((printLagMinFrames) || (rsyncpkts > PRINTLAG_IFGREATERTHAN))
        {
            if (rsyncpkts > PRINTLAG_IFGREATERTHAN)
                printLagMinFrames = PRINTLAG_MINFRAMES;
            else if (printLagMinFrames)
                printLagMinFrames--;

            //fontPrintf(400,y,colWhite,"Lag >= %3.3f",(real32)(rsyncpkts * CAPTAINSERVER_PERIOD));
            lagSlowComputerIcon();
        }

        lagSlowInternetIcon();

        if ((playPackets) && (!recordplayPacketsInGame))
        {
            fontPrintf(400,y,colWhite,"%d %f",receivedPacketNumber,universe.totaltimeelapsed);
            y += 10;
        }

/*        if (multiPlayerGame)
        {
            sdword i;

            for (i=0;i<sigsNumPlayers;i++)
            {
                if (playersReadyToGo[i] == PLAYER_DROPPED_OUT)
                {
                    fontPrintf(400,y,HorseRaceDropoutColor,"%s %s",playerNames[i],strGetString(strDroppedOut));
                    y += 10;
                }
            }
        }*/
    }
#endif
}

/*-----------------------------------------------------------------------------
    Name        : rndDrawOnScreenDebugInfo
    Description : Handle render-time debugging info to print on the screen
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
#if RND_VERBOSE_LEVEL >= 1
extern sdword numchans[];
extern sdword channelsinuse;

void rndDrawOnScreenDebugInfo(void)
{
    sdword y = 0;

#if DEBUG_VISIBLE_POLYS
    static color c;
    static sdword counter = 0;
#endif

#if MR_TEST_HPB
    if (mrTestHPBMode)
    {
        if (selSelected.numShips == 1)
        {
            sdword width = fontWidth("H:  -9999.00");
            fontPrintf(MAIN_WindowWidth - width, 0, colWhite, "H:  %.2f", mrHeading * 180.0f / PI);
            fontPrintf(MAIN_WindowWidth - width, fontHeight(" ") + 1, colWhite, "P:  %.2f", mrPitch * 180.0f / PI);
            fontPrintf(MAIN_WindowWidth - width, (fontHeight(" ") + 1) * 2, colWhite, "B:  %.2f", mrBank * 180.0f / PI);
            fontPrintf(MAIN_WindowWidth - width, (fontHeight(" ") + 1) * 3, colWhite, "I: %d", madTestHPBIndex);
            if (keyIsHit(ALTKEY) || keyIsHit(RALTKEY))
            {
                mrHeading = mrPitch = mrBank = 0.0f;
            }
        }
        mouseCursorHide();
    }
    mrTestHPBMode = FALSE;
#endif
#if PIE_VISUALIZE_VERTICAL
    if (pieOnScreen)
    {
        static bool32 pieFlashFlag = FALSE;
        pieFlashFlag ^= TRUE;
        if (pieFlashFlag)
        {
            primLine2((sdword)pieScreen0.x, 479 - (sdword)pieScreen0.y, (sdword)pieScreen1.x, 479 - (sdword)pieScreen1.y, colWhite);
        }
    }
#endif //PIE_VISUALIZE_VERTICAL

    y = 0;

#if DEBUG_VISIBLE_POLYS
    if (selSelected.numShips == 1)
    {
        extern sdword visiblePoly, visibleSegment, visibleDirection, visibleWhich;
        extern real32 visibleUV[3][2];
        if (counter > 0)
        {
            counter--;
        }
        else
        {
            counter = 4;
            if (keyIsHit(NINEKEY))
            {
                //dec
                if (visiblePoly >= 0)
                {
                    if (keyIsHit(SHIFTKEY))
                    {
                        visiblePoly -= 10;
                        if (visiblePoly < -1)
                        {
                            visiblePoly = -1;
                        }
                    }
                    else
                    {
                        visiblePoly--;
                    }
                }
            }
            else if (keyIsHit(ZEROKEY))
            {
                //inc
                if (keyIsHit(SHIFTKEY))
                {
                    visiblePoly += 10;
                }
                else
                {
                    visiblePoly++;
                }
            }
            else if (keyIsHit(ONEKEY))
            {
                visibleDirection = -1;
                visibleWhich = 0;
            }
            else if (keyIsHit(TWOKEY))
            {
                visibleDirection = +1;
                visibleWhich = 0;
            }
            else if (keyIsHit(THREEKEY))
            {
                visibleDirection = -1;
                visibleWhich = 1;
            }
            else if (keyIsHit(FOURKEY))
            {
                visibleDirection = +1;
                visibleWhich = 1;
            }
            else if (keyIsHit(FIVEKEY))
            {
                visibleSegment++;
                if (visibleSegment > 2)
                {
                    visibleSegment = 0;
                }
            }
            else if (keyIsHit(SEVENKEY))
            {
                extern bool32 g_Output;
                g_Output = TRUE;
            }
        }
        fontPrintf(0, 0, colWhite, "%d poly %d", visibleSegment, visiblePoly);
        c = (visibleSegment == 0) ? colRGB(255,0,0) : colWhite;
        fontPrintf(0, 16, c, "%0.4f %0.4f", visibleUV[0][0], visibleUV[0][1]);
        c = (visibleSegment == 1) ? colRGB(0,255,0) : colWhite;
        fontPrintf(0, 32, c, "%0.4f %0.4f", visibleUV[1][0], visibleUV[1][1]);
        c = (visibleSegment == 2) ? colRGB(0,0,255) : colWhite;
        fontPrintf(0, 48, c, "%0.4f %0.4f", visibleUV[2][0], visibleUV[2][1]);
    }
#endif

#if RND_FRAME_RATE
    if (rndDisplayFrameRate)
    {                                                   //display frame rate
        cmDeterministicBuildDisplay();
        if (rndFrameCountMin == SDWORD_Max)
        {
            fontPrintf(0, 0, colReddish, "%.2f <= %.2f <= %.2f",
                       (real32)(rndFrameRate) / RND_FrameRateSeconds,
                       (real32)(rndFrameRate) / RND_FrameRateSeconds,
                       (real32)(rndFrameRate) / RND_FrameRateSeconds);
        }
        else
        {
            fontPrintf(0, 0, colWhite, "%.2f <= %.2f <= %.2f",
                       (real32)(rndFrameCountMin) / RND_FrameRateSeconds,
                       (real32)(rndFrameRate) / RND_FrameRateSeconds,
                       (real32)(rndFrameCountMax) / RND_FrameRateSeconds);
        }
        y += 10;

        fontPrintf(0,y += 20,colRGB(255,255,0),"Channels in use:%d Guns:%d Ships:%d SFX:%d UI:%d",channelsinuse,numchans[0],numchans[1],numchans[2],numchans[3]);

    }
#endif//RND_FRAME_RATE

#ifdef COLLISION_CHECK_STATS
    if (rndDisplayCollStats)
    {
        fontPrintf(0,y += 20,colWhite, "Ship:%d  %d  Resources:%d  %d  Derelicts:%d %d  Total:%d  %d",shipshipwalks,shipshipchecks,
                   shipresourcewalks,shipresourcechecks,shipderelictwalks,shipderelictchecks,
                   shipshipwalks+shipresourcewalks+shipderelictwalks,shipshipchecks+shipresourcechecks+shipderelictchecks);

        fontPrintf(0,y += 20,colWhite, "Bullet:%d  %d  Missile:%d  %d  Mine:%d  %d  Total:%d  %d",
                   bulletwalks,bulletchecks,missilewalks,missilechecks,minewalks,minechecks,
                   bulletwalks+missilewalks+minewalks,bulletchecks+missilechecks+minechecks);

        fontPrintf(0,y += 20,colWhite, "Ships Avoiding Stuff:%d   Avoided Walks:%d   Avoided Checks:%d",
                   shipsavoidingstuff,shipsavoidedwalks,shipsavoidedchecks);
#if BOB_STATS
        if (bobStats.statsValid)
        {
            sdword timeDuration = bobStats.timeDuration / (1000L);
            fontPrintf(0,y += 20,colWhite, "Blobtype %d Time: %d numPasses:%d numWalks: %d",
                       bobStats.subBlobs, timeDuration, bobStats.numPasses, bobStats.numWalks);
            fontPrintf(0,y += 20,colWhite,"numChecks:%d trivialRejects:%d InitialBlobs: %d FinalBlobs: %d",
                       bobStats.numChecks, bobStats.trivialRejects, bobStats.initialBlobs, bobStats.finalBlobs);
        }
#endif
#if AISHIP_STATS
        aishipStatsPrint(&y);
#endif
    }
#endif
#if RND_POLY_STATS
    if (rndDisplayPolyStats)
    {                                                   //display frame rate
        fontPrint(0, MAIN_WindowHeight - fontHeight(rndPolyStatsString),
                  rndPolyStatsColor, rndPolyStatsString);
        rndPolyStatFrameCounter++;
#if MESH_MATERIAL_STATS
        fontPrint(MAIN_WindowWidth - fontWidth(meshMaterialStatsString),
                  MAIN_WindowHeight - fontHeight(rndPolyStatsString),
                  rndPolyStatsColor, meshMaterialStatsString);
#endif //MESH_MATERIAL_STATS
    }
#endif//RND_POLY_STATS
#if RND_XYZ
    if (rndXYZPrint && selSelected.numShips == 1)
    {
        static char string[256];
        if (selSelecting.numTargets == 1)
        {
            sprintf(string, "deltaXYZ = (%.2f, %.2f, %.2f)", selSelecting.TargetPtr[0]->obj.posinfo.position.x - selSelected.ShipPtr[0]->obj.posinfo.position.x, selSelecting.TargetPtr[0]->obj.posinfo.position.y - selSelected.ShipPtr[0]->obj.posinfo.position.y, selSelecting.TargetPtr[0]->obj.posinfo.position.z - selSelected.ShipPtr[0]->obj.posinfo.position.z);
        }
        else
        {
            sprintf(string, "XYZ = (%.2f, %.2f, %.2f)", selSelected.ShipPtr[0]->obj.posinfo.position.x, selSelected.ShipPtr[0]->obj.posinfo.position.y, selSelected.ShipPtr[0]->obj.posinfo.position.z);
        }
        fontPrint(MAIN_WindowWidth - fontWidth(string), 0, colWhite, string);
    }
#endif
#if NIS_PRINT_INFO
    if (nisPrintInfo)
    {
        fontPrintf(MAIN_WindowWidth - fontWidth(nisInfoString) - 10,
                   MAIN_WindowHeight - fontHeight(nisInfoString) - 1,
                   colWhite, nisInfoString);
    }
#endif

#if RND_SCALECAP_TWEAK
    if (RND_CAPSCALECAP_STATS)
    {
        if (selSelected.numShips == 1)
        {
            static Ship* ship;
            static shiptrail* trail;

            ship = selSelected.ShipPtr[0];
            trail = ship->trail[0];

            if (trail != NULL)
            {
                if (keyIsHit(RND_CapScaleCapKeyDec))
                {
                    //decrement
                    if (trail->scalecap == -1.0f)
                    {
                        trail->scalecap = 0.0f;
                    }
                    trail->scalecap -= rndCapScaleCapSlopeDelta;
                    if (trail->scalecap < 0.0f)
                    {
                        trail->scalecap = 0.0f;
                    }
                }
                if (keyIsHit(RND_CapScaleCapKeyInc))
                {
                    //increment
                    if (trail->scalecap == -1.0f)
                    {
                        trail->scalecap = 0.0f;
                    }
                    trail->scalecap += rndCapScaleCapSlopeDelta;
                }
            }
            fontPrint(0, MAIN_WindowHeight - fontHeight(rndCapScaleCapStatsString),
                      colWhite, rndCapScaleCapStatsString);
        }
    }
#endif
}
#endif

/*-----------------------------------------------------------------------------
    Name        : rndLetterboxBehindSwitch .. rndLetterboxFileValues
    Description : HWI-FEATURE(letterbox-setting): the /letterboxBehind switch is run-only, as /noInterp
                    (MotionInterp.c miSwitchOff): the start-up's opOptionsSaveSettings and every Accept write the
                    options file's own value, so a one-off comparison run never changes the saved option
----------------------------------------------------------------------------*/
void rndLetterboxBehindSwitch(void)                 // HWI-FEATURE(letterbox-setting)
{
    if (!rndLbSwitched)
    {
        rndLbFileValue = letterboxBehind;
        rndLbSwitched = TRUE;
    }
    letterboxBehind = TRUE;
}
void rndLetterboxSwitchEnd(void)                    // HWI-FEATURE(letterbox-setting)
{
    if (rndLbSwitched)
    {
        letterboxBehind = rndLbFileValue;
        rndLbSwitched = FALSE;
    }
}
bool32 rndLetterboxSwitchStands(void)               // HWI-FEATURE(letterbox-setting)
{
    return rndLbSwitched;
}
bool32 rndLetterboxFileValue(void)                  // HWI-FEATURE(letterbox-setting)
{
    return rndLbSwitched ? rndLbFileValue : letterboxBehind;
}
void rndLetterboxFileValues(bool32 begin)           // HWI-FEATURE(letterbox-setting)
{
    if (begin)
    {
        rndLbRunValue = letterboxBehind;
        if (rndLbSwitched)
        {
            letterboxBehind = rndLbFileValue;
        }
    }
    else
    {
        letterboxBehind = rndLbRunValue;
    }
}

/*-----------------------------------------------------------------------------
    Name        : rndDrawScissorBars
    Description : Draw the scissor window bars top and bottom for when
                    scissoring is only partial or not supported.
    Inputs      : scissorEnabled - does the renderer support scissor windows?
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rndDrawScissorBars(bool32 scissorEnabled)
{
    sdword y;
    real32 GLy, oneGLy;
    sdword oldAdditive = rndAdditiveBlending;

    if (nisScissorFade == 0.0f && !scissorEnabled)
    {
        y = NIS_LetterHeight;
        GLy = primScreenToGLY(y);
        oneGLy = primScreenToGLY(MAIN_WindowHeight - y);
        glColor3f(0.0f, 0.0f, 0.0f);
    }
    else
    {
        y = (sdword)((real32)NIS_LetterHeight * nisScissorFade);
        GLy = primScreenToGLY(y);
        oneGLy = primScreenToGLY(MAIN_WindowHeight - y);
        dbgAssertOrIgnore(!nisFullyScissored || !scissorEnabled);
        dbgAssertOrIgnore((nisScissorFade > 0.0f && nisScissorFade <= 1.0f) || !scissorEnabled);
        glColor3f(MR_LetterboxGrey);
        glBegin(GL_TRIANGLES);
        //top grey part
        glVertex2f(primScreenToGLX(MAIN_WindowWidth), primScreenToGLY(0));
        glVertex2f(primScreenToGLX(-1), primScreenToGLY(0));
        glVertex2f(primScreenToGLX(-1), GLy);
        
        glVertex2f(primScreenToGLX(MAIN_WindowWidth), primScreenToGLY(0));
        glVertex2f(primScreenToGLX(-1), GLy);
        glVertex2f(primScreenToGLX(MAIN_WindowWidth), GLy);
        
        //bottom scissor part
        glVertex2f(primScreenToGLX(-1), oneGLy);
        glVertex2f(primScreenToGLX(-1), primScreenToGLY(MAIN_WindowHeight));
        glVertex2f(primScreenToGLX(MAIN_WindowWidth), primScreenToGLY(MAIN_WindowHeight));
        
        glVertex2f(primScreenToGLX(-1), oneGLy);
        glVertex2f(primScreenToGLX(MAIN_WindowWidth), primScreenToGLY(MAIN_WindowHeight));
        glVertex2f(primScreenToGLX(MAIN_WindowWidth), oneGLy);
        glEnd();

        // CAT: the fading bars blend normally: the exe turns additive blending off for them and back after
        //      (rndDrawScissorBars 0x0057a410: 0x0057a6ee .. 0x0057a703 and 0x0057a8c8 ..; the main overlay leaves it on,
        //      cat_moRender 0x004aafb0), else the black at alpha = fade adds nothing and the bars stay grey
        if (!letterboxBehind)                       // HWI-FEATURE(letterbox-setting): 1 keeps the M2 frames
        {
            oldAdditive = rndAdditiveBlends(FALSE);
        }
        glEnable(GL_BLEND);
        glColor4f(0.0f, 0.0f, 0.0f, nisScissorFade);
    }
    glBegin(GL_TRIANGLES);
    //top scissor part
    glVertex2f(primScreenToGLX(-1), primScreenToGLY(0));
    glVertex2f(primScreenToGLX(-1), GLy);
    glVertex2f(primScreenToGLX(MAIN_WindowWidth), GLy);
    
    glVertex2f(primScreenToGLX(-1), primScreenToGLY(0));
    glVertex2f(primScreenToGLX(MAIN_WindowWidth), GLy);
    glVertex2f(primScreenToGLX(MAIN_WindowWidth), primScreenToGLY(0));
    
    //bottom scissor part
    glVertex2f(primScreenToGLX(-1), oneGLy);
    glVertex2f(primScreenToGLX(-1), primScreenToGLY(MAIN_WindowHeight));
    glVertex2f(primScreenToGLX(MAIN_WindowWidth), primScreenToGLY(MAIN_WindowHeight));
    
    glVertex2f(primScreenToGLX(-1), oneGLy);
    glVertex2f(primScreenToGLX(MAIN_WindowWidth), primScreenToGLY(MAIN_WindowHeight));
    glVertex2f(primScreenToGLX(MAIN_WindowWidth), oneGLy);
    glEnd();
    glDisable(GL_BLEND);
    // CAT-FIX: the exe's full-bars branch restores a wrong value into rndAdditiveBlending at 0x0057a8c8: ebx, loaded at
    //      0x0057a4ba with the bar's GL y (float bits, nonzero), so the exe leaves additive blending on after the full
    //      bars; the port puts back the state it found (the fade branch's restore) [audit: M3-R1 letterbox-front: ebx is
    //      set, not uninitialised]
    rndAdditiveBlends(oldAdditive);
}

/*-----------------------------------------------------------------------------
    Name        : rndRenderTask
    Description : Main render task
    Inputs      : void
    Outputs     : Clears frame buffer, renders regions, draws mouse, swaps buffers.
    Return      : void
----------------------------------------------------------------------------*/
static void catLodStatsFrameBegin(void);                    // HWI-TEST (M3-R1 full-detail)
static void catLodStatsFrameEnd(void);                      // HWI-TEST (M3-R1 full-detail)
DEFINE_TASK(rndRenderTask)
{
    static sdword index;
#ifdef PROFILE_TIMERS
    static sdword y;
#endif

    taskBegin;

    taskYield(0);

    while (1)
    {
        primErrorMessagePrint();
        catLodStatsFrameBegin();                            // HWI-TEST (M3-R1 full-detail): /catLodStats
        hwiFrameBegin();                                    // HWI-FEATURE(instrumentation): the render task's top

        speechEventUpdate();
        subTitlesUpdate();

#if RND_GL_STATE_DEBUG
        if (keyIsHit(GKEY) && keyIsStuck(LKEY))
        {                                                   //if starting a new round of state saving
            static char *fileNameFull;
            static FILE *f;
            keyClearSticky(LKEY);
            rndGLStateSaving = TRUE;
            if (rndGLStateLogIndex < 10)
            {
                sprintf(rndGLStateLogFileName, "gl0%d.state", rndGLStateLogIndex);
            }
            else
            {
                sprintf(rndGLStateLogFileName, "gl%d.state", rndGLStateLogIndex);
            }
            rndGLStateLogIndex++;

            fileNameFull = filePathPrepend(
                rndGLStateLogFileName, FF_UserSettingsPath);
            if (fileMakeDestinationDirectory(fileNameFull))
            {
                f = fopen(fileNameFull, "wt");              //open the file to clean it out
                if (f != NULL)
                {
                    fclose(f);                              //close the file
                }
            }
        }
#endif
        glColor3ub(colRed(RND_StarColor), colGreen(RND_StarColor), colBlue(RND_StarColor));
        if (lmActive)
        {
            rndClearToBlack();
            glClear(GL_DEPTH_BUFFER_BIT);
        }
        else
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //clear the buffers
        }

        rndScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);   //can we do scissoring?
        primErrorMessagePrint();
        //default rendering scheme is primitives on. any
        //functions which want it off should set it back on when done
        miRenderBegin();                                    // HWI-FEATURE(motion-interp): the render transforms in
        regFunctionsDraw();                                 //render all regions
        miRenderEnd();                                      // HWI-FEATURE(motion-interp): the simulation's back, bit for bit
        primErrorMessagePrint();
        rndFrameCount++;                                    //update frame count
        //draw partial scissor window, if applicable
        if (nisScissorFade != 0.0f || (nisFullyScissored && !rndScissorEnabled))
        {
            rndDrawScissorBars(rndScissorEnabled);
        }
        glDisable(GL_SCISSOR_TEST);                         //in case the scene was rendered with scissoring, turn it off properly.


        /* need to update audio event layer */
        soundEventUpdate();

        //draw the subtitle text.  It's done here because it has to draw over everything
        for (index = 0; index < SUB_NumberRegions; index++)
        {
            if ((!tutPointersDrawnThisFrame) && index == STR_LetterboxBar)
            {                                           //if this is the subtitle region
                tutDrawTextPointers(&subRegion[STR_LetterboxBar].rect);//draw any active pointers there may be
            }
            if (subRegion[index].bEnabled && subRegion[index].cardIndex > 0)
            {
                if (index == STR_NIS)
                {
                    subTimeElapsed = &thisNisPlaying->timeElapsed;
                }
                else
                {
                    subTimeElapsed = &universe.totaltimeelapsed;
                }
                subTitlesDraw(&subRegion[index]);
            }
        }

        rndDrawOnScreenSyncStatus();        // always print sync status, even in HW_BUILD_FOR_DISTRIBUTION mode

#if RND_VERBOSE_LEVEL >= 1
        rndDrawOnScreenDebugInfo();                         //draw a bunch of debugging crapola
#endif
#ifdef PROFILE_TIMERS
        y = 300;
        if (rndDisplayCollStats) profTimerStatsPrint(&y);        // compiled out automatically if PROFILE_TIMERS not defined
#endif

        // Automatic screenshot generation for render debugging
        if (debugScreenshots) {
            mouseCursorHide();
            
            char screenshotName[64];
            static MissionEnum screenshotMission = 0;
            static sdword screenshotTimer = 0;

            // Reset the screenshot timer if we've changed missions
            if (screenshotMission != spGetCurrentMission()) {
                screenshotTimer = 128 * UNIVERSE_UPDATE_RATE_FACTOR;
                screenshotMission = spGetCurrentMission();
            }

            // Take a screenshot every 112 universe ticks
            if (universe.univUpdateCounter >= screenshotTimer) {
                printf("debugScreenshots univUpdateCounter: %d\n", universe.univUpdateCounter);
                screenshotTimer += 112 * UNIVERSE_UPDATE_RATE_FACTOR;
                sprintf(screenshotName, "golden-%02d-%05d.jpg", spGetCurrentMission(),
                        universe.univUpdateCounter / UNIVERSE_UPDATE_RATE_FACTOR);
                ssTakeScreenshot(screenshotName);
            }

            // We can't hyperspace early while a NIS is playing - abort any playing NIS
            if (universe.univUpdateCounter > 800 * UNIVERSE_UPDATE_RATE_FACTOR) {
                if (thisNisPlaying) {
                    printf("debugScreenshots nisGoToEnd()\n");
                    nisGoToEnd(thisNisPlaying);
                }
            }
            
            // Enough screenshots taken for current mission
            if (universe.univUpdateCounter > 816 * UNIVERSE_UPDATE_RATE_FACTOR) {
                // Exit game if this is last mission
                if (spGetCurrentMission() == MISSION_16_HIIGARA) {
                    utyGameQuit(NULL, NULL);    
                }

                // Engage docking and hyperspace routine to jump to next mission
                // HWI (M2-R8 singleplayer-tutorial): only into a mission whose kas2c script is generated (M2-R9
                //      missions-02-09: Mission01 .. Mission09 are, so the screenshot loop runs on through them; the
                //      later missions' scripts are M2-R9 part B's producer 3's); missions 02 .. 09's own jumps are
                //      exercised by the missiontest mode's --mission N
                if ((singlePlayerGameInfo.hyperspaceState == NO_HYPERSPACE) &&
                    cat_spKasScriptGenerated((MissionEnum)(spGetCurrentMission() + 1))) {
                    printf("debugScreenshots singlePlayerNextLevel()\n");
                    hyperspaceOverride = TRUE;
                    singlePlayerNextLevel();
                }
            }
            
            // Keep pressing spacebar to press the quick dock button
            // Sometimes needs multiple presses to work
            if (universe.univUpdateCounter > 832 * UNIVERSE_UPDATE_RATE_FACTOR) {
                if ((universe.univUpdateCounter % (16 * UNIVERSE_UPDATE_RATE_FACTOR) == 0) &&
                    (singlePlayerGameInfo.hyperspaceState == HYPERSPACE_WAITINGROLLCALL)) {
                    printf("debugScreenshots keyPressDown()\n");
                    keyPressDown(SDL_SCANCODE_SPACE);
                }
            }
        }

        // CAT (M2-R6 mainrgn-popup): rndRenderTask 0x0057a910 sets the cursor without mouseSelectCursorSetting (gone
        //      from Cataclysm). The front end's saved cursor (feShouldSaveMouseCursor, cat_mouseSaveCursorUnder /
        //      cat_mouseRestoreCursorUnder around the flip) is the Win32 GL front buffer's (decision 28: SDL redraws the
        //      whole frame). M2-R6 frontend: while the front end asks for that save the exe neither clears the buffers
        //      nor flips them (the same front-buffer scheme: not applicable); its screenshot flag (glReadPixels into
        //      ssSaveScreenshot) and the texture-pool resize after mainReinitRGL (trNoPalResizePool of the pool
        //      cat_trTextureMemorySet set) are GoK's screenshot and renderer-reinit paths (decision 28). The render half
        //      (clears, regions, scissor bars, subtitles, sync status, the fill counter) is as here.
        mouseSetCursorSetting();
        
        mouseDraw();                                        //draw mouse atop everything

        // CAT (M2-R7 r6-leftovers): rndRenderTask 0x0057ab78 draws the shameless plug after the mouse while the
        //      universe is paused (0x00a6c5b4: the console, the game-stats screen, a single-player pause) or the Pause
        //      key's flag 0x00a6ed74 is set (rndShamelessPlug 0x00579ff0)
        if (universePause || catUnivFrameStep)
        {
            rndShamelessPlug();
        }

        if (universePause || !gameIsRunning) {
            fontMakeCurrent(selGroupFont2);
            char buildVersion[] = HOMEWORLD_BUILD_VERSION;
            sdword x = MAIN_WindowWidth - fontWidthN(buildVersion, sizeof(buildVersion)) - 2;
            sdword y = MAIN_WindowHeight - fontHeight(" ") - 2;
            fontPrint(x, y, colWhite, buildVersion);
            x -= fontWidthN(networkVersion, MAX_NETWORKVERSION_STRING_LEN) + 6;
            fontPrint(x, y, colWhite, networkVersion);
        }

        //take a screenshot or sequence thereof
        if (keyIsStuck(SS_SCREENSHOT_KEY)
#ifdef __APPLE__
        ||  keyIsStuck(SS_SCREENSHOT_KEY_2)
        ||  keyIsStuck(SS_SCREENSHOT_KEY_3)
#endif
            )
        {
            rndTakeScreenshot = TRUE;

            keyClearSticky(SS_SCREENSHOT_KEY);
#ifdef __APPLE__
            keyClearSticky(SS_SCREENSHOT_KEY_2);
            keyClearSticky(SS_SCREENSHOT_KEY_3);
#endif
        }
        else if (keyIsStuck(PAUSEKEY))
        {
            keyClearSticky(PAUSEKEY);
        }

        if (rndTakeScreenshot)
        {
            rndTakeScreenshot = FALSE;
            ssTakeScreenshot(NULL);
        }

        if (rndFillCounter)
        {
            static rectangle r;
            r.x0 = 0;
            r.y0 = 0;
            r.x1 = MAIN_WindowWidth - 1;
            r.y1 = MAIN_WindowHeight - 1;
            primRectSolid2(&r, rndFillColour);
            rndFillCounter--;
        }
#if 0
#if defined (HW_GAME_DEMO)
        ;
#else
        {
            extern bool32 mainPlayAVIs;

            if (mainPlayAVIs)
            {
                mainPlayAVIs = FALSE;
                animSmackPlay(-1, (sdword)"Movies\\sierra.smk");
                animSmackPlay(-1, (sdword)"Movies\\relicintro.smk");
            }
        }
#endif
#endif
        hwiOverlayDraw();                                   // HWI-FEATURE(instrumentation): the overlay (off by default), on top
        catShotFrameEnd();                                  // HWI-TEST (M3-R1 letterbox-front): /catShot, the any-mode capture
        catTeamColourFrameEnd();                            // HWI-TEST (M3-R1 team-colours-warp): /catTeamColourTest
        catMotionTestFrameEnd();                            // HWI-FEATURE(motion-interp) measurement: the frame's dump line + CRC

        //demo-specific update code
        if (demDemoPlaying && gameIsRunning)
        {                                                   //if a demo is playing and we're in the actual game
            if (rndMainViewRender != rndMainViewRenderFunction)
            {                                               //this was a fake render; don't do a swap or flush
                goto afterTheSwap;
            }
        }
        if (mainReinitRenderer > 0)
        {
            mainReinitRenderer--;
        }
        else
        {
            if (animaticJustPlayed > 0)
            {
                animaticJustPlayed--;
            }
            else if (!feDontFlush)
            {
                rndFlush();
            }
            feDontFlush = FALSE;
        }
        primErrorMessagePrint();
afterTheSwap:
        catLodStatsFrameEnd();                              // HWI-TEST (M3-R1 full-detail): /catLodStats
        hwiFrameEnd();                                      // HWI-FEATURE(instrumentation): the frame's clocks, /frameLog

#if RND_GL_STATE_DEBUG
        rndGLStateSaving = FALSE;                           //done saving the file for now
#endif //RND_GL_STATE_DEBUG
        tutPointersDrawnThisFrame = FALSE;
        taskYield(0);                                       //hold off to next frame
    }
    taskEnd;
}

/*-----------------------------------------------------------------------------
    Name        : rndLightingEnable
    Description : Enable or disable back-face culling
    Inputs      : bEnable - TRUE enables, FALSE disables
    Outputs     : just calls glEnable/glDisable
    Return      : void
----------------------------------------------------------------------------*/
sdword rndLightingEnable(sdword bEnable)
{
    sdword oldStatus = rndLightingEnabled;

    if (bEnable)
    {
        if (!rndLightingEnabled)
        {
            glEnable(GL_LIGHTING);
            rndLightingEnabled = TRUE;
        }
    }
    else
    {
        if (rndLightingEnabled)
        {
            glDisable(GL_LIGHTING);
            rndLightingEnabled = FALSE;
        }
    }

    return oldStatus;
}

/*-----------------------------------------------------------------------------
    Name        : rndBackFaceCullEnable
    Description : Enable or disable back-face culling
    Inputs      : bEnable - TRUE enables, FALSE disables
    Outputs     : just calls glEnable/glDisable
    Return      : void
----------------------------------------------------------------------------*/
void rndBackFaceCullEnable(sdword bEnable)
{
    if (bEnable)
    {
        glEnable(GL_CULL_FACE);
    }
    else
    {
        glDisable(GL_CULL_FACE);
    }
}

/*-----------------------------------------------------------------------------
    Name        : rndTextureEnable
    Description : Enable or disable texturing
    Inputs      : bEnable - TRUE enables, FALSE disables
    Outputs     : toggles rndTextureEnabled
    Return      : previous enabled status
----------------------------------------------------------------------------*/
sdword rndTextureEnable(sdword bEnable)
{
    sdword enabled = rndTextureEnabled;
    if (bEnable)
    {
        if (!rndTextureEnabled)
        {
            glEnable(GL_TEXTURE_2D);
            rndTextureEnabled = TRUE;
        }
    }
    else
    {
        if (rndTextureEnabled)
        {
            glDisable(GL_TEXTURE_2D);
            rndTextureEnabled = FALSE;
        }
    }
    return(enabled);
}

/*-----------------------------------------------------------------------------
    Name        : rndTextureEnvironment
    Description : changes the current texture environment
    Inputs      : textureMode - one of RTE_Modulate, RTE_Replace, RTE_Decal, RTE_Blend
    Outputs     : changes the current texture environment mode
    Return      : old mode
----------------------------------------------------------------------------*/
udword rndTextureEnvironment(udword textureMode)
{
    udword oldMode = rndTextureEnviron;
    
    if (rndTextureEnviron != textureMode)
    {
        rndTextureEnviron = textureMode;
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, rndTextureEnviron);
    }
    
    return oldMode;
}

/*-----------------------------------------------------------------------------
    Name        : rndNormalizeEnable
    Description : enables or disables normalization
    Inputs      : bEnable - TRUE enables, FALSE disables
    Outputs     : toggles rndNormalization
    Return      : previous status
----------------------------------------------------------------------------*/
sdword rndNormalizeEnable(sdword bEnable)
{
    sdword oldStatus = rndNormalization;
    if (bEnable)
    {
        glEnable(GL_NORMALIZE);
        rndNormalization = TRUE;
    }
    else
    {
        glDisable(GL_NORMALIZE);
        rndNormalization = FALSE;
    }
    return(oldStatus);
}

/*-----------------------------------------------------------------------------
    Name        : rndHintInc
    Description : changes the glHint mode of operation
    Inputs      :
    Outputs     : alters rndHint
    Return      :
----------------------------------------------------------------------------*/
void rndHintInc()
{
#if USE_RND_HINT
    rndHint++;
    if (rndHint >= 3) rndHint = 0;
#endif
}

/*-----------------------------------------------------------------------------
    Name        : rndAdditiveBlends
    Description : enables or disables additive blending mode
    Inputs      : bAdditive - TRUE or FALSE
    Outputs     :
    Return      : previous status
----------------------------------------------------------------------------*/
sdword rndAdditiveBlends(sdword bAdditive)
{
    sdword oldStatus;

    oldStatus = rndAdditiveBlending;
    if (bAdditive != rndAdditiveBlending)
    {
        rndAdditiveBlending = bAdditive;
        if (bAdditive)
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }
        else
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    }

    return(oldStatus);
}

#define vcSub(a,b,c) \
    (a)->x = (b)->x - (c)->x; \
    (a)->y = (b)->y - (c)->y; \
    (a)->z = (b)->z - (c)->z

#define vcCrossProduct(result,a,b) \
    (result)->x = ((a)->y * (b)->z) - ((a)->z * (b)->y); \
    (result)->y = ((a)->z * (b)->x) - ((a)->x * (b)->z); \
    (result)->z = ((a)->x * (b)->y) - ((a)->y * (b)->x)

#define vcAddToScalarMultiply(dstvec,vec,k) \
    (dstvec)->x += ((vec)->x * (k));       \
    (dstvec)->y += ((vec)->y * (k));       \
    (dstvec)->z += ((vec)->z * (k))

#define vcAddToScalarMultiply(dstvec,vec,k) \
    (dstvec)->x += ((vec)->x * (k));       \
    (dstvec)->y += ((vec)->y * (k));       \
    (dstvec)->z += ((vec)->z * (k))

#define vcScalarMultiply(dstvec,vec,k) \
    (dstvec)->x = (vec)->x * (k);       \
    (dstvec)->y = (vec)->y * (k);       \
    (dstvec)->z = (vec)->z * (k)



#define vcDotProduct(a,b) \
    ( ((a)->x * (b)->x) + ((a)->y * (b)->y) + ((a)->z * (b)->z) )


/*-----------------------------------------------------------------------------
    Name        : rndEnvironmentMap
    Description : calculates parameters for rendering a reflective poly
    Inputs      : vector camera : location of camera
                  vectors A,B,C : vertices of poly


    Outputs     : vectors U,V,W : corners on texture map (.x and .y only!)
    Return      :
----------------------------------------------------------------------------*/

void rndEnvironmentMap(vector* camera, vector* A, vector* B, vector* C,
                       vector* U, vector* V, vector* W)
{
    vector v1,v2,normal;
    real32 length;

    //get normal of triangle
    vcSub(&v1,B,A);
    vcSub(&v2,C,A);
    vcCrossProduct(&normal,&v1,&v2);
    vecNormalizeToLength(&normal, (real32)1.0f);

    //vertex A:

    vcSub(&v1,A,camera);        //v1 is incoming ray
    length = vcDotProduct(&v1,&normal);
    vcAddToScalarMultiply(&v1,&normal,2*length);  //possibly "-2*length"

    //v1 is now reflected ray
    U->x = atan(v1.y/v1.x);                             //azimuth
    U->y = atan(v1.z/(sqrt(v1.x*v1.x + v1.y*v1.y)));    //elevation;


    //vertex B:

    vcSub(&v1,B,camera);
    length = vcDotProduct(&v1,&normal);
    vcAddToScalarMultiply(&v1,&normal,2*length);  //possibly "-2*length"

    V->x = atan(v1.y/v1.x);                             //azimuth
    V->y = atan(v1.z/(sqrt(v1.x*v1.x + v1.y*v1.y)));    //elevation;


    //vertex C:

    vcSub(&v1,C,camera);
    length = vcDotProduct(&v1,&normal);
    vcAddToScalarMultiply(&v1,&normal,2*length);  //possibly "-2*length"

    W->x = atan(v1.y/v1.x);                             //azimuth
    W->y = atan(v1.z/(sqrt(v1.x*v1.x + v1.y*v1.y)));    //elevation;

}

/*-----------------------------------------------------------------------------
    Name        : rndEnvironmentMapConvex
    Description : calculates parameters for rendering a reflective poly with
                    a convex or concave surface
    Inputs      : vector camera : location of camera
                  vectors A,B,C : vertices of poly
                  convex: curvature of surface
                        convex < 1 : concave
                        convex = 1 : flat
                        convex > 1 : convex


    Outputs     : vectors U,V,W : corners on texture map (.x and .y only!)
    Return      :
----------------------------------------------------------------------------*/

void rndEnvironmentMapConvex(vector* camera, vector* A, vector* B, vector* C,
                       real32 convex, vector* U, vector* V, vector* W)
{
    vector v1,v2,normal;
    real32 length;

    //get normal of triangle
    vcSub(&v1,B,A);
    vcSub(&v2,C,A);
    vcCrossProduct(&normal,&v1,&v2);
    vecNormalizeToLength(&normal, (real32)1.0f);

    //vertex A:

    vcSub(&v1,A,camera);        //v1 is incoming ray
    length = vcDotProduct(&v1,&normal);
    vcScalarMultiply(&v2,&normal,length);
    vcAddToScalarMultiply(&v1,&v2,1+convex);

    //v1 is now reflected ray
    U->x = atan(v1.y/v1.x);                             //azimuth
    U->y = atan(v1.z/(sqrt(v1.x*v1.x + v1.y*v1.y)));    //elevation;


    //vertex B:

    vcSub(&v1,B,camera);
    length = vcDotProduct(&v1,&normal);
    vcScalarMultiply(&v2,&normal,2*length);
    vcAddToScalarMultiply(&v1,&v2,1+convex);

    V->x = atan(v1.y/v1.x);                             //azimuth
    V->y = atan(v1.z/(sqrt(v1.x*v1.x + v1.y*v1.y)));    //elevation;


    //vertex C:

    vcSub(&v1,C,camera);
    length = vcDotProduct(&v1,&normal);
    vcScalarMultiply(&v2,&normal,2*length);
    vcAddToScalarMultiply(&v1,&v2,1+convex);

    W->x = atan(v1.y/v1.x);                             //azimuth
    W->y = atan(v1.z/(sqrt(v1.x*v1.x + v1.y*v1.y)));    //elevation;



}

/*-----------------------------------------------------------------------------
    Name        : rndSetScreenFill
    Description : set a flag to fill the screen entirely with a rectangle of
                  specified colour immediately before the next Flush
    Inputs      : count - number of frames for fill to occur
                  c - color to fill screen with
    Outputs     : globals are set
    Return      :
-----------------------------------------------------------------------------*/
void rndSetScreenFill(sdword count, color c)
{
    rndFillCounter = count;
    rndFillColour = c;
}

/*-----------------------------------------------------------------------------
    Name        : rndSetClearColor
    Description : set the background clear color, if such a thing is supported by
                  underlying GL
    Inputs      : c - the color to set
    Outputs     : GL_COLOR_CLEAR_VALUE is perhaps modified
    Return      :
----------------------------------------------------------------------------*/
void rndSetClearColor(color c)
{
    rndClearColor[0] = colReal32(colRed(c));
    rndClearColor[1] = colReal32(colGreen(c));
    rndClearColor[2] = colReal32(colBlue(c));
    rndClearColor[3] = 1.0f;

    glClearColor(colReal32(colRed(c)),
                    colReal32(colGreen(c)),
                    colReal32(colBlue(c)),
                    1.0f);
}

void rndGetClearColor(GLfloat c[4])
{
    c[0] = rndClearColor[0];
    c[1] = rndClearColor[1];
    c[2] = rndClearColor[2];
    c[3] = rndClearColor[3];
}

/*-----------------------------------------------------------------------------
    Name        : rndResetGLState
    Description : reset the GL's state after switching renderers
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rndResetGLState(void)
{
    trClearCurrent();

    if (rndAdditiveBlending)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    }
    else
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    rndAdditiveBlends(rndAdditiveBlending);

    if (rndNormalization)
    {
        glEnable(GL_NORMALIZE);
    }
    else
    {
        glDisable(GL_NORMALIZE);
    }
    rndNormalizeEnable(rndNormalization);
    
    rndTextureEnvironment(RTE_Modulate);

    if (rndTextureEnabled)
    {
        glEnable(GL_TEXTURE_2D);
    }
    else
    {
        glDisable(GL_TEXTURE_2D);
    }
    rndTextureEnable(rndTextureEnabled);

    rndBackFaceCullEnable(TRUE);

    if (rndLightingEnabled)
    {
        glEnable(GL_LIGHTING);
    }
    else
    {
        glDisable(GL_LIGHTING);
    }
    rndLightingEnable(rndLightingEnabled);

    rndSetClearColor(colBlack);
}

/*-----------------------------------------------------------------------------
    Name        : rndComputeOverlap
    Description : compute amount of overlap of collision spheres wrt ships in renderlist
    Inputs      : ship - ship to test
                  scalar - multiplier for strike craft collision radii
    Outputs     :
    Return      : [0..1] amount of obscuring that's going on
----------------------------------------------------------------------------*/
real32 rndComputeOverlap(Ship* ship, real32 scalar)
{
    Node* objnode;
    SpaceObj* spaceobj;
    ShipStaticInfo* shipStaticInfo;
    Ship* testship;
    vector vobj;
    real32 overlap;
    CollInfo* acoll;
    CollInfo* bcoll;
    rectangle arect, brect, crect;
    real32 aarea, carea;
    real32 ratio;
    real32 radius;

    //looking for our ship
    objnode = universe.RenderList.head;
    while (objnode != NULL)
    {
        testship = (Ship*)listGetStructOfNode(objnode);
        if (testship == ship)
        {
            break;
        }
        objnode = objnode->next;
    }

    if (objnode == NULL)
    {
        //couldn't find it
        return 0.0f;
    }

    //move backwards computing overlap
//    objnode = objnode->prev;
    objnode = objnode->next;
    overlap = 0.0f;
    //make screenspace rectangle
    acoll = &ship->impactable.collInfo;
    radius = scalar * acoll->selCircleRadius;
    arect.x0 = primGLToScreenX(acoll->selCircleX - radius);
    arect.x1 = primGLToScreenX(acoll->selCircleX + radius);
    arect.y0 = primGLToScreenY(acoll->selCircleY + radius);
    arect.y1 = primGLToScreenY(acoll->selCircleY - radius);
    aarea = (real32)(2 * (arect.x1 - arect.x0) +
                     2 * (arect.y1 - arect.y0));
    while (objnode != NULL)
    {
        spaceobj = (SpaceObj*)listGetStructOfNode(objnode);
        if (spaceobj->header.objtype == OBJ_ShipType)
        {
            //useful stuff
            testship = (Ship*)spaceobj;
            shipStaticInfo = (ShipStaticInfo*)spaceobj->staticinfo;

            if (ship->staticinfo->shipclass <= CLASS_Frigate)
            {
                if (testship->staticinfo->shipclass > CLASS_Frigate)
                {
                    // don't calculate strikecraft overlapping capital ships
                    goto nextobj;
                }
            }

            //position in cameraspace
            vecSub(vobj, mrCamera->eyeposition, spaceobj->posinfo.position);

            //make screenspace rectangle
            bcoll = &testship->impactable.collInfo;
            radius = scalar * bcoll->selCircleRadius;
            brect.x0 = primGLToScreenX(bcoll->selCircleX - radius);
            brect.x1 = primGLToScreenX(bcoll->selCircleX + radius);
            brect.y0 = primGLToScreenY(bcoll->selCircleY + radius);
            brect.y1 = primGLToScreenY(bcoll->selCircleY - radius);

            //find union of ab
            primRectUnion2(&crect, &arect, &brect);
            if (crect.x0 == crect.x1 || crect.y0 == crect.y1)
            {
                carea = 0.0f;
            }
            else
            {
                carea = (real32)(2 * (crect.x1 - crect.x0) +
                                 2 * (crect.y1 - crect.y0));
            }

            //compute overlap
            ratio = carea / aarea;
            overlap += ratio;
            if (overlap >= 1.0f)
            {
                overlap = 1.0f;
                break;
            }
        }

//        objnode = objnode->prev;
nextobj:
        objnode = objnode->next;
    }

    return overlap;
}

/*-----------------------------------------------------------------------------
    Name        : rndClearToBlack
    Description : clear the screen to black, guaranteed
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rndClearToBlack(void)
{
    real32 bgcolor[4];

    //get current clearcolor
    glGetFloatv(GL_COLOR_CLEAR_VALUE, (GLfloat*)bgcolor);
    //set clearcolor to black
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    //clear the screen
    glClear(GL_COLOR_BUFFER_BIT);
    //reset clearcolor
    glClearColor(bgcolor[0], bgcolor[1], bgcolor[2], bgcolor[3]);
}

/*-----------------------------------------------------------------------------
    Name        : rndAllClearToBlack
    Description : clear all buffers to black
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rndAllClearToBlack(void)
{
    real32 bgcolor[4];

    //get current clearcolor
    glGetFloatv(GL_COLOR_CLEAR_VALUE, (GLfloat*)bgcolor);
    //set clearcolor to black
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    //clear the screen
    rndClear();
    //reset clearcolor
    glClearColor(bgcolor[0], bgcolor[1], bgcolor[2], bgcolor[3]);
}

/*-----------------------------------------------------------------------------
    Name        : rndClear
    Description : clear front and back buffers
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rndClear(void)
{
    sdword i;

    for (i = 0; i < 3; i++)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        rndFlush();
    }
}

/*-----------------------------------------------------------------------------
    Name        : rndFlush
    Description : flush the GL and swap buffers if necessary
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rndFlush(void)
{
    glFlush();
    primErrorMessagePrint();
#ifdef HW_ENABLE_GLES
    eglSwapBuffers(egl_display, egl_surface);
#else
    SDL_GL_SwapWindow(sdlwindow);
#endif
}

/*-----------------------------------------------------------------------------
    CAT: lens flare source list (new in Cataclysm). The exe keeps every source on one list at 0x00b37880;
    cat_rndLensFlaresRender 0x0057bec0 (below) walks it.
-----------------------------------------------------------------------------*/
LinkedList catRndLensFlareSourceList;

/*-----------------------------------------------------------------------------
    Name        : cat_rndLensFlareSourceAdd
    Description : CAT: new (0x0057baa0). Puts a lens flare source on the list, visibility 0, state 2.
    Inputs      : source - source to add (owned by a PART_LENSFLARES particle)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void cat_rndLensFlareSourceAdd(struct catlensflaresource *source)
{
    listAddNode(&catRndLensFlareSourceList, &source->link, source);
    source->fade = 0.0f;
    source->state = 2;
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndLensFlareSourceRemove
    Description : CAT: new (0x0057ba90). Takes a lens flare source off the list.
    Inputs      : source - source to remove
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void cat_rndLensFlareSourceRemove(struct catlensflaresource *source)
{
    listRemoveNode(&source->link);
}

/*=============================================================================
    CAT (M2-R5 render): the lens flares. A mission's "LensFlare <name>" line loads LensFlares\<name>.script
    (cat_scriptSetLensFlareCB 0x0057b8c0): a texture, its cells (Texture) and the flare's elements (Element), each
    drawn along the line from the source's screen position through the screen centre. Up to 8 sources are drawn a
    frame, the nearest ones; a source fades in while nothing blocks its line to the eye and out while something does.
=============================================================================*/
#define CAT_LF_MAX_ELEMENTS         48          // 0x00a75ad0 .. 0x00a75dd0
#define CAT_LF_MAX_TEXTURES         17          // 0x00a759b8 .. 0x00a75ad0
#define CAT_LF_MAX_VISIBLE          8           // 0x00b37820
#define CAT_LF_PIXEL_OFFSET         0.325f      // 0x0072a858: HW1's pixel-centre offset of the 2D primitives
typedef struct CatLensFlareElement
{
    real32 radius;                  // +0x00 a part of the screen height
    real32 offset;                  // +0x04 along the line: 1 the source, 0 the centre, -1 its reflection
    sbyte  texture;                 // +0x08 a cell, or -1 hex, -2 circle, -3 light-centre circle (the rim a quarter),
                                    //       -4 dark-centre circle (the centre a quarter) [audit: -3 / -4 were swapped]
    ubyte  pad[3];
    color  c;                       // +0x0C
} CatLensFlareElement;
typedef struct CatLensFlareCell
{
    real32 u, v, w, h;              // texture coordinates (pixels / 256)
} CatLensFlareCell;
static trhandle catLfTexture = 0;                               // 0x00a75d4c (0 = none, as the exe)
static CatLensFlareElement catLfElements[CAT_LF_MAX_ELEMENTS];  // 0x00a75ad0
static sdword catLfNumElements = 0;                             // 0x00a75dd0
static CatLensFlareCell catLfCells[CAT_LF_MAX_TEXTURES];        // 0x00a759b8
static sdword catLfNumCells = 0;                                // 0x00a75dcc
static real32 catLfFadeoutFrameSize = 1.2f;                     // 0x008baba8
static catlensflaresource *catLfVisible[CAT_LF_MAX_VISIBLE];    // 0x00b37820
static sdword catLfNumVisible = 0;                              // 0x00a75dc8
static catlensflaresource catLfNISSource;                       // 0x00b37b60: the NIS's light source

static void cat_rndSplitPathName(char *path, char *base);
static void cat_lfTextureFileSetCB(char *directory, char *field, void *dataToFillIn)      // 0x0057b820
{
    char name[256], base[256], path[512];

    if (sscanf(field, "%s", name) == 1)
    {
        cat_rndSplitPathName(name, base);
        snprintf(path, sizeof(path), "LensFlares\\%s", base);
        cat_rndLensFlareTextureUnload();
        catLfTexture = trTextureRegister(path, NULL, &catLfTexture, TRUE);
    }
}

static void cat_lfElementSetCB(char *directory, char *field, void *dataToFillIn)          // 0x0057b970
{
    real32 radius, offset;
    sdword texture, r, g, b;

    if (sscanf(field, "%f %f %d %d %d %d", &radius, &offset, &texture, &r, &g, &b) == 6)
    {
        // CAT-FIX: the exe does not check the element count (48 fit before the next variable)
        if (catLfNumElements >= CAT_LF_MAX_ELEMENTS)
        {
            return;
        }
        catLfElements[catLfNumElements].radius = radius;
        catLfElements[catLfNumElements].offset = offset;
        catLfElements[catLfNumElements].texture = (sbyte)texture;
        catLfElements[catLfNumElements].c = ((((udword)b | 0xffffff00) << 8 | (udword)g) << 8) | (udword)r;
        catLfNumElements++;
    }
}

static void cat_lfTextureSetCB(char *directory, char *field, void *dataToFillIn)          // 0x0057ba00
{
    sdword x, y, w, h;

    if (sscanf(field, "%d %d %d %d", &x, &y, &w, &h) == 4)
    {
        if (catLfNumCells >= CAT_LF_MAX_TEXTURES)           // CAT-FIX: as cat_lfElementSetCB
        {
            return;
        }
        catLfCells[catLfNumCells].u = (real32)x * (1.0f / 256.0f);
        catLfCells[catLfNumCells].v = (real32)y * (1.0f / 256.0f);
        catLfCells[catLfNumCells].w = (real32)w * (1.0f / 256.0f);
        catLfCells[catLfNumCells].h = (real32)h * (1.0f / 256.0f);
        catLfNumCells++;
    }
}

static scriptEntry catLensFlareScriptTable[] =                  // 0x008babb0
{
    { "Element",          cat_lfElementSetCB,  NULL },
    { "FadeoutFrameSize", scriptSetReal32CB,   &catLfFadeoutFrameSize },
    { "Texture",          cat_lfTextureSetCB,  NULL },
    { "TextureFile",      cat_lfTextureFileSetCB, NULL },
    END_SCRIPT_ENTRY
};

// the base name of a path (the exe's _splitpath + _makepath with no directory and no extension)
static void cat_rndSplitPathName(char *path, char *base)
{
    char *s = path, *slash;
    char *dot;

    slash = strrchr(s, '\\');
    if (slash == NULL || (strrchr(s, '/') != NULL && strrchr(s, '/') > slash))
    {
        slash = strrchr(s, '/');
    }
    if (slash != NULL)
    {
        s = slash + 1;
    }
    strcpy(base, s);
    dot = strrchr(base, '.');
    if (dot != NULL)
    {
        *dot = '\0';
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_scriptSetLensFlareCB (Cataclysm 0x0057b8c0, a mission script callback with no name in the
                  map; name chosen here)
    Description : "LensFlare <name>": read LensFlares\<name>.script
----------------------------------------------------------------------------*/
void cat_scriptSetLensFlareCB(char *directory, char *field, void *dataToFillIn)
{
    char name[256], base[256], path[512];

    catLfNumElements = 0;
    cat_rndLensFlareTextureUnload();
    if (sscanf(field, "%s", name) == 1)
    {
        cat_rndSplitPathName(name, base);
        snprintf(path, sizeof(path), "LensFlares\\%s.script", base);
        catLfNumCells = 0;
        scriptSet(NULL, path, catLensFlareScriptTable);
    }
}

void cat_rndLensFlareTextureUnload(void)                                                  // 0x0057b800
{
    if (catLfTexture != 0)
    {
        trTextureUnregister(catLfTexture);
        catLfTexture = 0;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndLensFlareSourceVisibility (Cataclysm 0x0057bad0)
    Description : Fade a source in or out by its line of sight and put it on the frame's visible list (8 slots: a
                  new slot while there are fewer, else the farthest source farther than this one, else slot 0)
----------------------------------------------------------------------------*/
static void cat_rndLensFlareSourceVisibility(catlensflaresource *src)
{
    vector diff, dir;
    real32 dist, sx, sy, radius, a, b, e;
    sdword slot, i, side;
    Node *node;
    bool32 occluded = FALSE;

    if (!src->enabled)
    {
        src->drawn = FALSE;
        return;
    }
    src->drawn = TRUE;
    diff.x = mrCamera->eyeposition.x - src->position.x;
    diff.y = mrCamera->eyeposition.y - src->position.y;
    diff.z = mrCamera->eyeposition.z - src->position.z;
    dist = getVectDistSloppy(diff);
    if (catLfNumVisible < CAT_LF_MAX_VISIBLE)
    {
        slot = catLfNumVisible++;
    }
    else
    {
        slot = 0;
        for (i = 1; i < catLfNumVisible; i++)
        {
            if (catLfVisible[i] == NULL || !catLfVisible[i]->drawn)
            {
                slot = i;
                break;
            }
            if (catLfVisible[i]->distance > catLfVisible[slot]->distance && dist < catLfVisible[i]->distance)
            {
                slot = i;
            }
        }
    }

    if (src->state == 2)
    {
        dir = diff;
        vecNormalize(&dir);
        for (node = universe.ShipList.head; node != NULL && !occluded; node = node->next)
        {
            Ship *ship = (Ship *)listGetStructOfNode(node);
            if (ship->header.objtype == OBJ_ShipType && ship->shiptype >= dSlipgateSmall && ship->shiptype <= dSlipgateLarge)
            {
                continue;
            }
            if (collCheckRectLine((SpaceObjRotImp *)ship, &src->position, &dir, dist, &side) != -1.0f &&
                (ship->header.objtype != OBJ_ShipType || ship->shiptype != CAT_LF_TRANSPARENT_SHIPTYPE))
            {
                occluded = TRUE;
            }
        }
        for (node = universe.ResourceList.head; node != NULL && !occluded; node = node->next)
        {
            if (collCheckRectLine((SpaceObjRotImp *)listGetStructOfNode(node), &src->position, &dir, dist, &side) != -1.0f)
            {
                occluded = TRUE;
            }
        }
    }
    else if (src->state == 1)
    {
        occluded = TRUE;
    }
    if (occluded)
    {
        if (src->fade > 0.0f)
        {
            src->fade *= 0.6f;                                                  // 0x0072af48
            if (src->fade < 1e-9)                                               // 0x0072beb8
            {
                src->drawn = FALSE;
            }
        }
    }
    else if (src->fade < 1.0f)
    {
        src->fade = (1.0f - src->fade) * 0.4f + src->fade;                      // 0x0072a674
        if (src->fade > 1.0f)
        {
            src->fade = 1.0f;
        }
    }

    if (src->drawn)
    {
        selCircleComputeGeneral(&rndCameraMatrix, &rndProjectionMatrix, &src->position, 1.0f, &sx, &sy, &radius);
        if (radius > 0.0f)
        {
            if (ABS(sx) <= 1.0 && ABS(sy) <= 1.0)
            {
                src->edgeFade = 1.0f;
            }
            else
            {
                a = catLfFadeoutFrameSize - ABS(sx);
                b = catLfFadeoutFrameSize - ABS(sy);
                e = (a < b) ? a : b;
                src->edgeFade = (e > 0.0f) ? e / (catLfFadeoutFrameSize - 1.0f) : 0.0f;
            }
            if (src->edgeFade > 0.0f)
            {
                src->screenX = (real32)(MAIN_WindowWidth / 2 + (sdword)lrint((double)((real32)MAIN_WindowWidth * sx * 0.5f)));
                src->screenY = (real32)(MAIN_WindowHeight / 2 - (sdword)lrint((double)((real32)MAIN_WindowHeight * sy * 0.5f)));
                src->distance = dist;
                catLfVisible[slot] = src;
                return;
            }
        }
        src->drawn = FALSE;
    }
    catLfVisible[slot] = src;
}

// a screen position (pixels) in the 2D mode's coordinates (-1..1, y up)
#define CAT_LF_X(x)     ((((x) + CAT_LF_PIXEL_OFFSET) / (real32)MAIN_WindowWidth) * 2.0f - 1.0f)
#define CAT_LF_Y(y)     (1.0f - (((y) + CAT_LF_PIXEL_OFFSET) / (real32)MAIN_WindowHeight) * 2.0f)

static real32 cat_lfClampColour(real32 v)
{
    if (v < 0.0f)
    {
        return 0.0f;
    }
    if (!(v <= 255.0f))
    {
        return 255.0f;
    }
    return v;
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndLensFlaresRender (Cataclysm 0x0057bec0)
    Description : Draw the lens flares of the visible sources (with the lens flare option on)
----------------------------------------------------------------------------*/
void cat_rndLensFlaresRender(void)
{
    bool32 lighting = rndLightingEnable(FALSE);
    bool32 texture = rndTextureEnable(TRUE);
    Node *node;
    sdword i, j, segs, n;
    catlensflaresource *src;
    CatLensFlareElement *el;
    real32 dx, dy, intensity, size, x, y, r, g, b, outer[3];
    bool32 oldTexture, oldLighting, cull, blend, stipple;
    udword oldEnv;
    real64 angle, step;

    if (catLfTexture != 0 && enableLensFlare)
    {
        catLfNumVisible = 0;
        for (node = catRndLensFlareSourceList.head; node != NULL; node = node->next)
        {
            cat_rndLensFlareSourceVisibility((catlensflaresource *)listGetStructOfNode(node));
        }
        primModeSet2();
        glDepthMask(GL_FALSE);
        for (i = 0; i < catLfNumVisible; i++)
        {
            src = catLfVisible[i];
            if (src == NULL || !src->drawn)
            {
                continue;
            }
            dx = src->screenX - (real32)(MAIN_WindowWidth / 2);
            dy = src->screenY - (real32)(MAIN_WindowHeight / 2);
            intensity = src->edgeFade * src->fade * src->brightness;
            if (intensity != 0.0f)
            {
                catRndCounts.lensFlares++;
                for (j = 0; j < catLfNumElements; j++)
                {
                    el = &catLfElements[j];
                    size = (real32)MAIN_WindowHeight * el->radius;
                    x = dx * el->offset + (real32)(MAIN_WindowWidth / 2);
                    y = dy * el->offset + (real32)(MAIN_WindowHeight / 2);
                    oldTexture = rndTextureEnabled;
                    blend = FALSE;
                    stipple = FALSE;
                    if (el->texture >= 0)
                    {
                        rndTextureEnable(TRUE);
                        oldLighting = rndLightingEnable(FALSE);
                        trMakeCurrent(catLfTexture);
                        if (bitTest(trStructureGet(catLfTexture)->flags, TRF_Alpha))
                        {
                            glEnable(GL_BLEND);
                            glDisable(GL_ALPHA_TEST);
                            rndAdditiveBlends(TRUE);
                        }
                        oldEnv = rndTextureEnvironment(RTE_Modulate);
                        cull = glIsEnabled(GL_CULL_FACE);
                    }
                    else
                    {
                        rndTextureEnable(FALSE);
                        oldLighting = rndLightingEnable(FALSE);
                        glEnable(GL_BLEND);
                        glDisable(GL_ALPHA_TEST);
                        rndAdditiveBlends(TRUE);
                        oldEnv = rndTextureEnvironment(RTE_Modulate);
                        cull = glIsEnabled(GL_CULL_FACE);
                        blend = glIsEnabled(GL_BLEND);
                        stipple = glIsEnabled(GL_POLYGON_STIPPLE);
                        if (!blend)
                        {
                            glEnable(GL_BLEND);
                        }
                        if (stipple)
                        {
                            glDisable(GL_POLYGON_STIPPLE);
                        }
                    }
                    glDisable(GL_CULL_FACE);
                    r = cat_lfClampColour((real32)colRed(el->c) * intensity);
                    g = cat_lfClampColour((real32)colGreen(el->c) * intensity);
                    b = cat_lfClampColour((real32)colBlue(el->c) * intensity);
                    if (el->texture >= 0)
                    {
                        CatLensFlareCell *cell = &catLfCells[(sdword)el->texture];
                        glColor3f(r * (1.0f / 256.0f), g * (1.0f / 256.0f), b * (1.0f / 256.0f));
                        glBegin(GL_QUADS);
                        glTexCoord2f(cell->u, cell->v);
                        glVertex2f(CAT_LF_X(x - size), CAT_LF_Y(y - size));
                        glTexCoord2f(cell->u + cell->w, cell->v);
                        glVertex2f(CAT_LF_X(x + size), CAT_LF_Y(y - size));
                        glTexCoord2f(cell->u + cell->w, cell->v + cell->h);
                        glVertex2f(CAT_LF_X(x + size), CAT_LF_Y(y + size));
                        glTexCoord2f(cell->u, cell->v + cell->h);
                        glVertex2f(CAT_LF_X(x - size), CAT_LF_Y(y + size));
                    }
                    else
                    {
                        // the drawn shapes (jump table 0x0057caac, indexed by type + 4): -1 hex; -2 circle; -3 light
                        // centre (the rim a quarter); -4 dark centre (the centre a quarter) [audit: was "-3 dark
                        // centre, -4 light centre"; the code below, as the exe's cases 0x0057c597 / 0x0057c616, is right]
                        segs = 6;
                        outer[0] = r;
                        outer[1] = g;
                        outer[2] = b;
                        if (el->texture != -1)
                        {
                            segs = (sdword)(el->radius * 40.0f * 4.0f);                // 0x0072bec0, 0x0072a530
                            if (segs < 6)
                            {
                                segs = 6;
                            }
                            else if (segs > 32)
                            {
                                segs = 32;
                            }
                            if (el->texture == -3)
                            {
                                r *= 0.25f;
                                g *= 0.25f;
                                b *= 0.25f;
                            }
                            else if (el->texture == -4)
                            {
                                outer[0] = r * 0.25f;
                                outer[1] = g * 0.25f;
                                outer[2] = b * 0.25f;
                            }
                        }
                        glColor3f(outer[0] * (1.0f / 256.0f), outer[1] * (1.0f / 256.0f), outer[2] * (1.0f / 256.0f));
                        glBegin(GL_TRIANGLE_FAN);
                        glVertex2f(CAT_LF_X(x), CAT_LF_Y(y));
                        glColor3f(r * (1.0f / 256.0f), g * (1.0f / 256.0f), b * (1.0f / 256.0f));
                        step = 6.2831854820251465 / (real64)segs;                  // 0x0072a500
                        angle = 0.0;
                        for (n = 0; n < segs; n++)
                        {
                            glVertex2f((real32)((((real64)x + cos(angle) * (real64)size + (real64)CAT_LF_PIXEL_OFFSET) /
                                                 (real64)MAIN_WindowWidth) * 2.0 - 1.0),
                                       (real32)(1.0 - (((real64)y + sin(angle) * (real64)size + (real64)CAT_LF_PIXEL_OFFSET) /
                                                       (real64)MAIN_WindowHeight) * 2.0));
                            angle += step;
                        }
                        glVertex2f(CAT_LF_X(x + size), CAT_LF_Y(y));
                    }
                    glEnd();
                    catRndCounts.lensFlareElements++;
                    glDisable(GL_BLEND);
                    rndTextureEnvironment(oldEnv);
                    rndAdditiveBlends(FALSE);
                    rndTextureEnable(oldTexture);
                    // CAT: the exe restores the lighting state onto the texture state (0x0057c886 / 0x0057c94e: its
                    //      "lighting" restore enables or disables GL_TEXTURE_2D); kept
                    rndTextureEnable(oldLighting);
                    if (cull)
                    {
                        glEnable(GL_CULL_FACE);
                    }
                    if (el->texture < 0)
                    {
                        if (!blend)
                        {
                            glDisable(GL_BLEND);
                        }
                        if (stipple)
                        {
                            glEnable(GL_POLYGON_STIPPLE);
                        }
                    }
                }
            }
            if (src->visible)
            {
                src->state = 1;
            }
        }
        glDepthMask(GL_TRUE);
        primModeClear2();
    }
    rndLightingEnable(lighting);
    rndTextureEnable(texture);
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndLensFlareNISSource* (Cataclysm 0x0057cac0 .. 0x0057cb40)
    Description : the NIS's own light source (nisStart adds it, nisUpdate moves it, nisObjectHide / nisObjectShow
                  hide and show it, nisStop removes it)
----------------------------------------------------------------------------*/
void cat_rndLensFlareNISSourceAdd(void)                                                   // 0x0057cac0
{
    catLfNISSource.visible = 0;
    listAddNode(&catRndLensFlareSourceList, &catLfNISSource.link, &catLfNISSource);
    catLfNISSource.fade = 0.0f;
    catLfNISSource.enabled = 0;
    catLfNISSource.brightness = 1.0f;
    catLfNISSource.state = 0;
}

void cat_rndLensFlareNISSourceRemove(void)                                                // 0x0057cb10
{
    listRemoveNode(&catLfNISSource.link);
}

void cat_rndLensFlareNISSourceSetPosition(sdword unused, vector *position)                // 0x0057cb20
{
    catLfNISSource.position = *position;
}

void cat_rndLensFlareNISSourceEnable(sdword unused, sdword show)                          // 0x0057cb40
{
    if (enableLensFlare && CAT_RND_RGLTYPE != 0)
    {
        catLfNISSource.state = (show == 0);
        catLfNISSource.enabled = 1;
    }
}

/*=============================================================================
    CAT (M2-R5 render): the fog layer: two scrolling cloud textures over the screen, tinted by the mission's fog
    colour ("Fog r g b r g b speed density", cat_scriptSetFogCB 0x0057cb70); the NIS fades it (cat_rndFogLayerFadeTo)
    and switches it off (cat_rndFogLayerOff). Its fade channel of the background multiplier darkens the BTG while
    the fog fades to or from density 0. The state block is the render state's 0x00c32c14..0x00c32c4c.
=============================================================================*/
#define CAT_FOG_TEXTURE     "race_derelict\\dplasmacloud\\rl0\\lod0\\cloud"     // 0x008bab7c
#define CAT_FOG_DENSITY_SCALE   1.0e7f          // 0x0072afa0 (the script's density)
#define CAT_FOG_INTENSITY_SCALE 3.3333333e-08f  // 0x0072bed0 (density to colour scale)
bool8  catRndFogOn = FALSE;                     // 0x00c32c14
color  catRndFogColor[2] = {0, 0};              // 0x00c32c18 / 0x00c32c1c (the second is not drawn)
real32 catRndFogSpeed = 0.0f;                   // 0x00c32c20
real32 catRndFogDensity = 0.0f;                 // 0x00c32c24
static color  catRndFogToColor[2];              // 0x00c32c28 / 0x00c32c2c
static real32 catRndFogToSpeed;                 // 0x00c32c30
static real32 catRndFogToDensity;               // 0x00c32c34
static color  catRndFogFromColor[2];            // 0x00c32c38 / 0x00c32c3c
static real32 catRndFogFromSpeed;               // 0x00c32c40
static real32 catRndFogFromDensity;             // 0x00c32c44
static real32 catRndFogFadeStart;               // 0x00c32c48
static real32 catRndFogFadeEnd = 0.0f;          // 0x00c32c4c: 0 no fade running
static trhandle catRndFogTexture = 0;           // 0x00a75d50
// the two layers (0x00a75cf8 .. 0x00a75d3c): scroll speeds, tile sizes and offsets; set on the first draw
static real32 catRndFogUSpeed[2], catRndFogVSpeed[2], catRndFogTile[2], catRndFogU[2], catRndFogV[2];
static bool32 catRndFogLayersInit = TRUE;       // 0x008babe0 (1 in .data)
static real32 catRndFogLastTime = 0.0f;         // 0x00a75dd4

void cat_rndFogLayerTextureUnload(void)                                                   // 0x0057b7b0
{
    if (catRndFogTexture != 0)
    {
        trTextureUnregister(catRndFogTexture);
        catRndFogTexture = 0;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_rndStateBlockGet / cat_rndStateBlockSet
    Description : CAT (M2-R6 oracle-mode-a): the 39 dwords at 0x00c32c00 a save holds (SaveGame 0x004fdaf0 writes
                  them after clearing +0x00, +0x84 and +0x88; LoadGame 0x004fe0e0 reads them back). Of these this tree
                  keeps the fog layer's words (+0x14 .. +0x4c, the variables above); the others are the renderer's
                  per-object scratch words (the object being drawn, its effect parameters), rewritten every frame
                  before they are read, so they are written as 0 and ignored on load.
----------------------------------------------------------------------------*/
void cat_rndStateBlockGet(udword *block)
{
    block[0x14 / 4] = (udword)catRndFogOn;
    block[0x18 / 4] = (udword)catRndFogColor[0];
    block[0x1C / 4] = (udword)catRndFogColor[1];
    block[0x20 / 4] = Real32ToSdword(catRndFogSpeed);
    block[0x24 / 4] = Real32ToSdword(catRndFogDensity);
    block[0x28 / 4] = (udword)catRndFogToColor[0];
    block[0x2C / 4] = (udword)catRndFogToColor[1];
    block[0x30 / 4] = Real32ToSdword(catRndFogToSpeed);
    block[0x34 / 4] = Real32ToSdword(catRndFogToDensity);
    block[0x38 / 4] = (udword)catRndFogFromColor[0];
    block[0x3C / 4] = (udword)catRndFogFromColor[1];
    block[0x40 / 4] = Real32ToSdword(catRndFogFromSpeed);
    block[0x44 / 4] = Real32ToSdword(catRndFogFromDensity);
    block[0x48 / 4] = Real32ToSdword(catRndFogFadeStart);
    block[0x4C / 4] = Real32ToSdword(catRndFogFadeEnd);
}

void cat_rndStateBlockSet(const udword *block)
{
    catRndFogOn = (bool8)block[0x14 / 4];
    catRndFogColor[0] = (color)block[0x18 / 4];
    catRndFogColor[1] = (color)block[0x1C / 4];
    catRndFogSpeed = SdwordToReal32((sdword)block[0x20 / 4]);
    catRndFogDensity = SdwordToReal32((sdword)block[0x24 / 4]);
    catRndFogToColor[0] = (color)block[0x28 / 4];
    catRndFogToColor[1] = (color)block[0x2C / 4];
    catRndFogToSpeed = SdwordToReal32((sdword)block[0x30 / 4]);
    catRndFogToDensity = SdwordToReal32((sdword)block[0x34 / 4]);
    catRndFogFromColor[0] = (color)block[0x38 / 4];
    catRndFogFromColor[1] = (color)block[0x3C / 4];
    catRndFogFromSpeed = SdwordToReal32((sdword)block[0x40 / 4]);
    catRndFogFromDensity = SdwordToReal32((sdword)block[0x44 / 4]);
    catRndFogFadeStart = SdwordToReal32((sdword)block[0x48 / 4]);
    catRndFogFadeEnd = SdwordToReal32((sdword)block[0x4C / 4]);
}

void cat_rndFogLayerTextureLoad(void)                                                     // 0x0057b7d0
{
    if (catRndFogTexture == 0)
    {
        catRndFogTexture = trTextureRegister(CAT_FOG_TEXTURE, NULL, &catRndFogOn, TRUE);
    }
}

static color cat_rndFogColorFromInts(sdword r, sdword g, sdword b)
{
    return (((((udword)b & 0xff) | 0xffffff00) << 8 | ((udword)g & 0xff)) << 8) | ((udword)r & 0xff);
}

void cat_scriptSetFogCB(char *directory, char *field, void *dataToFillIn)                 // 0x0057cb70
{
    sdword r1, g1, b1, r2, g2, b2;
    real32 speed, density;

    catRndFogOn = FALSE;
    btgSetColourMultiplier(1, 1.0f);
    if (field[0] == '\0')
    {
        cat_rndFogLayerTextureUnload();
        catRndFogFadeEnd = 0.0f;
        catRndFogSpeed = 0.0f;
        catRndFogDensity = 0.0f;
        catRndFogColor[0] = 0;
        catRndFogColor[1] = 0;
    }
    else if (sscanf(field, "%d %d %d %d %d %d %f %f", &r1, &g1, &b1, &r2, &g2, &b2, &speed, &density) == 8)
    {
        // CAT: the exe's sscanf is stopped by the commas of the mission files ("Fog 0,0,0,0,0,0,0,0"): only r1 is
        //      read and nothing changes (the mission scripts' fog lines are all zeros); a later NIS sets the fog
        density = (real32)((real64)density * 10000000.0);
        catRndFogSpeed = speed;
        catRndFogDensity = density;
        catRndFogColor[0] = cat_rndFogColorFromInts(r1, g1, b1);
        catRndFogColor[1] = cat_rndFogColorFromInts(r2, g2, b2);
        catRndFogFadeEnd = 0.0f;
        if (density <= 0.0f)
        {
            cat_rndFogLayerTextureUnload();
            catRndFogOn = FALSE;
        }
        else
        {
            cat_rndFogLayerTextureLoad();
            catRndFogOn = TRUE;
        }
    }
}

void cat_rndFogLayerOff(void)                                                             // 0x0057cd20
{
    catRndFogOn = FALSE;
    btgSetColourMultiplier(1, 1.0f);
}

void cat_rndFogLayerFadeTo(real32 time, color c1, color c2, real32 speed, real32 density)   // 0x0057cd40
{
    if (time > 0.0f)
    {
        catRndFogFromColor[0] = catRndFogColor[0];
        catRndFogFadeEnd = universe.totaltimeelapsed + time;
        catRndFogFromColor[1] = catRndFogColor[1];
        catRndFogFromSpeed = catRndFogSpeed;
        catRndFogFromDensity = catRndFogDensity;
        catRndFogFadeStart = universe.totaltimeelapsed;
        catRndFogToSpeed = speed;
        catRndFogToDensity = density;
        catRndFogToColor[0] = c1;
        catRndFogToColor[1] = c2;
    }
    else
    {
        catRndFogSpeed = speed;
        catRndFogFadeEnd = 0.0f;
        catRndFogDensity = density;
        catRndFogColor[0] = c1;
        catRndFogColor[1] = c2;
    }
}

// a channel blended between two colours by t, truncated (the exe's __ftol)
static color cat_rndFogBlendColor(color from, color to, real32 t)
{
    sdword r = (sdword)(((real32)colRed(to) - (real32)colRed(from)) * t + (real32)colRed(from));
    sdword g = (sdword)(((real32)colGreen(to) - (real32)colGreen(from)) * t + (real32)colGreen(from));
    sdword b = (sdword)(((real32)colBlue(to) - (real32)colBlue(from)) * t + (real32)colBlue(from));

    return cat_rndFogColorFromInts(r, g, b);
}

void cat_rndFogLayerUpdate(void)                                                          // 0x0057ce00
{
    real32 t, to, from;

    if (!catRndFogOn)
    {
        btgSetColourMultiplier(1, 1.0f);
    }
    else if (catRndFogFadeEnd != 0.0f)
    {
        if (catRndFogFadeEnd <= universe.totaltimeelapsed)
        {
            catRndFogSpeed = catRndFogToSpeed;
            catRndFogColor[0] = catRndFogToColor[0];
            catRndFogColor[1] = catRndFogToColor[1];
            catRndFogDensity = catRndFogToDensity;
            catRndFogFadeEnd = 0.0f;
            btgSetColourMultiplier(1, (catRndFogToDensity == 0.0f) ? 1.0f : 0.0f);
            return;
        }
        t = (universe.totaltimeelapsed - catRndFogFadeStart) / (catRndFogFadeEnd - catRndFogFadeStart);
        catRndFogSpeed = (catRndFogToSpeed - catRndFogFromSpeed) * t + catRndFogFromSpeed;
        catRndFogDensity = (catRndFogToDensity - catRndFogFromDensity) * t + catRndFogFromDensity;
        catRndFogColor[0] = cat_rndFogBlendColor(catRndFogFromColor[0], catRndFogToColor[0], t);
        catRndFogColor[1] = cat_rndFogBlendColor(catRndFogFromColor[1], catRndFogToColor[1], t);
        to = (catRndFogToDensity != 0.0f) ? 0.0f : 1.0f;
        from = (catRndFogFromDensity != 0.0f) ? 0.0f : 1.0f;
        btgSetColourMultiplier(1, (real32)(((real64)to - (real64)from) * (real64)t + (real64)from));
    }
}

void cat_rndFogLayerRender(void)                                                          // 0x0057d2c0
{
    real32 s, d, r, g, b, x0, x1, y0, y1;
    sdword i;
    bool32 lighting, texture, cull;
    udword oldEnv;

    if (!catRndFogOn || CAT_RND_RGLTYPE == 0 || catRndFogTexture == 0)
    {
        return;
    }
    s = catRndFogDensity * CAT_FOG_INTENSITY_SCALE;
    if (!(s <= 1.0f))
    {
        s = 1.0f;
    }
    else if (s == 0.0f)
    {
        return;
    }
    if (catRndFogLayersInit)
    {
        for (i = 0; i < 2; i++)
        {
            catRndFogU[i] = 0.0f;
            catRndFogV[i] = 0.5f;
            catRndFogUSpeed[i] = (i & 1) ? 0.05f : -0.0505f;                    // 0x0072a654 / 0x0072becc
            catRndFogVSpeed[i] = (i & 1) ? 0.005f : -0.015f;                    // 0x0072bec8 / 0x0072bec4
            catRndFogTile[i] = (i & 1) ? 0.25f : 0.2f;                          // 0x0072a450 / 0x0072a510
        }
        catRndFogLayersInit = FALSE;
    }
    else
    {
        d = universe.totaltimeelapsed - catRndFogLastTime;
        catRndFogLastTime = universe.totaltimeelapsed;
        d = catRndFogSpeed * d;
        for (i = 0; i < 2; i++)
        {
            catRndFogU[i] = d * catRndFogUSpeed[i] + catRndFogU[i];
            catRndFogV[i] = d * catRndFogVSpeed[i] + catRndFogV[i];
            if (catRndFogU[i] > 100.0f)
            {
                catRndFogU[i] -= 100.0f;
                catRndFogV[i] -= 100.0f;
            }
        }
    }
    primModeSet2();
    trMakeCurrent(catRndFogTexture);
    glEnable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    rndAdditiveBlends(TRUE);
    glDepthMask(GL_FALSE);
    oldEnv = rndTextureEnvironment(RTE_Modulate);
    cull = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);
    r = cat_lfClampColour((real32)colRed(catRndFogColor[0]) * s);
    g = cat_lfClampColour((real32)colGreen(catRndFogColor[0]) * s);
    b = cat_lfClampColour((real32)colBlue(catRndFogColor[0]) * s);
    lighting = rndLightingEnable(FALSE);
    texture = rndTextureEnable(TRUE);
    glColor3f(r * (1.0f / 256.0f), g * (1.0f / 256.0f), b * (1.0f / 256.0f));
    x0 = CAT_LF_X(0.0f);
    x1 = CAT_LF_X((real32)MAIN_WindowWidth);
    y0 = CAT_LF_Y(0.0f);
    y1 = CAT_LF_Y((real32)MAIN_WindowHeight);
    for (i = 0; i < 2; i++)
    {
        glBegin(GL_QUADS);
        glTexCoord2f(catRndFogU[i], catRndFogV[i]);
        glVertex2f(x0, y0);
        glTexCoord2f(catRndFogU[i], catRndFogTile[i] + catRndFogV[i]);
        glVertex2f(x0, y1);
        glTexCoord2f(catRndFogTile[i] + catRndFogU[i], catRndFogTile[i] + catRndFogV[i]);
        glVertex2f(x1, y1);
        glTexCoord2f(catRndFogTile[i] + catRndFogU[i], catRndFogV[i]);
        glVertex2f(x1, y0);
        glEnd();
        catRndCounts.fogLayers++;
    }
    glDisable(GL_BLEND);
    rndAdditiveBlends(FALSE);
    rndTextureEnvironment(oldEnv);
    if (cull)
    {
        glEnable(GL_CULL_FACE);
    }
    glDepthMask(GL_TRUE);
    primModeClear2();
    rndLightingEnable(lighting);
    rndTextureEnable(texture);
}

/*=============================================================================
    CAT: port diagnostic, not in the exe: /catRenderTest <frames> (harness mode render). The headless Mission01 run
    of /oracleMission (one universe tick a frame, no input) renders through the Cataclysm object-list renderer; at
    tick 16 it closes the sensors manager the headless start opens (the player has no ships before the mission
    script runs, so the camera has no focus) and focuses the camera on the first 20 ships, switches the lens flare
    option on (Cataclysm.cfg "enableLensFlare", off in the harness's config), shows the NIS light source ahead of
    the camera (cat_rndLensFlareNISSource*, as an NIS does) and sets a fog line (cat_scriptSetFogCB, as a mission's
    "Fog" line; Mission01's is all zeros); at the end it logs the
    renderer's counts: "rendertest: done ... result=PASS|FAIL" (PASS: objects, sorted objects, lens flares and fog
    layers all drawn). None of these touches the simulation (no random draw, no game state); the camera focus does
    change which objects are on the render list, so this run is not a stream reference (the mission mode is).
=============================================================================*/
/*=============================================================================
    HWI-TEST (M3-R1 full-detail): the level-of-detail check and the frame statistics of HWI-FEATURE(full-detail).
    /catLodTest <tick>[:far]: in the main view's first frame of universe tick <tick>, after the levels are chosen,
    one "lodtest: obj ..." line per visible object with a LOD table (its type, ship type or object number, the
    exe's level currentLOD = band, the level drawn = drawn, the table's levels and the selection circle radius the
    bands read), then "lodtest: frame ..." and "lodtest: done ... result=PASS|FAIL" (FULL DETAIL on: every drawn
    level 0 and at least one object; off: every drawn level the exe's and at least one above 0). ":far" pulls the
    camera out to its farthest zoom from tick 16 on (the current focus's remembered camera), so the bands pick their
    low levels. ":fleet" (either switch): at tick 16 the sensors manager the headless start opens is closed and the
    camera focuses on every ship (as the render self-test focuses the player's), so the modes that never draw the
    main view (ai, skirmish, mission) draw it with the whole fleet in view. /catLodStats <from>-<to>: one "lodstats: tick=.. ms=.." line per main-view frame of those ticks (the
    render task from its top to the flip, a glFinish before the clock is read), the objects with a LOD table drawn
    and the vertices / polygons of the levels drawn; at the end the median and the 99th percentile. Read only: no
    game state, no random draw (the camera pull moves the view, so a ":far" run is no stream reference).
=============================================================================*/
static sdword catLodTestTick = -1;
static bool32 catLodTestFar = FALSE;
static bool32 catLodTestDone = FALSE;
static sdword catLodStatsFrom = -1, catLodStatsTo = -1;
static bool32 catLodStatsActive = FALSE, catLodStatsDone = FALSE;
static Uint64 catLodStatsStart = 0;
static udword catLodStatsObjects = 0, catLodStatsVerts = 0, catLodStatsPolys = 0;
#define CAT_LOD_STATS_MAX   8192
static real32 *catLodStatsMs = NULL, *catLodStatsVertList = NULL;
static sdword catLodStatsCount = 0;
static bool32 catLodFleetFocus = FALSE, catLodFleetFocused = FALSE;

void catLodTestSet(char *string)
{
    catLodTestTick = (sdword)strtol(string, NULL, 10);
    catLodTestFar = (strstr(string, ":far") != NULL);
    catLodFleetFocus |= (strstr(string, ":fleet") != NULL);
}

void catLodStatsSet(char *string)
{
    char *dash = strchr(string, '-');

    catLodStatsFrom = (sdword)strtol(string, NULL, 10);
    catLodStatsTo = (dash != NULL) ? (sdword)strtol(dash + 1, NULL, 10) : catLodStatsFrom;
    catLodFleetFocus |= (strstr(string, ":fleet") != NULL);
}

// ":fleet": out of the sensors manager, the camera on every ship
static void catLodFleetFocusTick(void)
{
    FocusCommand *focus;
    Node *node;

    if (!catLodFleetFocus || catLodFleetFocused || !gameIsRunning || universe.univUpdateCounter < 16 || nisIsRunning)
    {
        return;
    }
    catLodFleetFocused = TRUE;
    focus = memAlloc(sizeofSelectCommand(universe.ShipList.num + 1), "lodtestfocus", 0);
    focus->numShips = 0;
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        focus->ShipPtr[focus->numShips++] = (Ship *)listGetStructOfNode(node);
    }
    if (smSensorsActive)
    {
        smSensorsCloseForGood();
    }
    if (focus->numShips > 0)
    {
        ccCameraTimeoutOverride = TRUE;
        ccFocusGeneral(&universe.mainCameraCommand, focus, FALSE);
    }
    dbgMessagef("lodtest: fleet focus tick=%u ships=%d", (unsigned)universe.univUpdateCounter, (int)focus->numShips);
    memFree(focus);
}

// the vertices of a level's mesh (every polygon object)
static udword catLodLevelVerts(lod *level)
{
    meshdata *mesh;
    udword n = 0;
    sdword i;

    if ((level->flags & LM_LODType) != LT_Mesh || (mesh = (meshdata *)level->pData) == NULL)
    {
        return 0;
    }
    for (i = 0; i < mesh->nPolygonObjects; i++)
    {
        n += (udword)mesh->object[i].nVertices;
    }
    return n;
}

// the main view, after the levels are chosen: the camera pull, the per-object lines and the frame's counts
static void catLodTestMainView(LinkedList *visible)
{
    Node *node;
    SpaceObj *obj;
    lodinfo *info;
    lod *level;
    udword drawn, objects = 0, drawnAbove0 = 0, bandAbove0 = 0, mismatch = 0, maxBand = 0, verts = 0, polys = 0;
    bool32 logIt = (catLodTestTick >= 0 && !catLodTestDone && (sdword)universe.univUpdateCounter >= catLodTestTick);
    bool32 pass;

    if (catLodTestFar && !catLodTestDone && universe.univUpdateCounter >= 16 &&
        universe.mainCameraCommand.currentCameraStack != NULL)
    {
        Camera *remember = &universe.mainCameraCommand.currentCameraStack->remembercam;
        remember->distance = remember->farthestZoom;
        cameraSetEyePosition(remember);
    }
    if (!logIt && !catLodStatsActive && !hwiWantVerts())   // HWI-FEATURE(instrumentation): the overlay's / frame log's count
    {
        return;
    }
    for (node = visible->head; node != NULL; node = node->next)
    {
        obj = (SpaceObj *)listGetStructOfNode(node);
        switch (obj->header.objtype)
        {
            case OBJ_ShipType: case OBJ_AsteroidType: case OBJ_GasType: case OBJ_DustType: case OBJ_DerelictType:
            case OBJ_MissileType:
                break;
            default:
                continue;
        }
        if (obj->staticinfo->LOD == NULL)
        {
            continue;
        }
        info = (obj->header.objtype == OBJ_ShipType) ? cat_mimicGetStaticInfo((Ship *)obj) : obj->staticinfo->LOD;
        level = cat_lodDrawLevelGet(obj, &drawn);
        objects++;
        verts += catLodLevelVerts(level);
        polys += ((level->flags & LM_LODType) == LT_Mesh) ? (udword)cat_lodHierarchyCount(level) : 0;
        if (drawn > 0) drawnAbove0++;
        if (obj->currentLOD > 0) bandAbove0++;
        if (obj->currentLOD > maxBand) maxBand = obj->currentLOD;
        if (!opNoLODVal && drawn != obj->currentLOD) mismatch++;
        if (logIt)
        {
            dbgMessagef("lodtest: obj type=%s name=%s id=%u band=%u drawn=%u levels=%d radius=%.5f dist=%.0f",
                        ObjTypeToStr(obj->header.objtype),
                        (obj->header.objtype == OBJ_ShipType) ? ShipTypeToStr(((Ship *)obj)->shiptype) : "-",
                        (obj->header.objtype == OBJ_ShipType) ? (unsigned)((Ship *)obj)->shipID.shipNumber : 0u,
                        (unsigned)obj->currentLOD, (unsigned)drawn, (int)info->nLevels,
                        (double)((SpaceObjRotImpTarg *)obj)->impactable.collInfo.selCircleRadius,
                        (double)fsqrt(obj->cameraDistanceSquared));
        }
    }
    hwiVertsDrawn(objects, verts);                  // HWI-FEATURE(instrumentation)
    if (catLodStatsActive)
    {
        catLodStatsObjects = objects;
        catLodStatsVerts = verts;
        catLodStatsPolys = polys;
    }
    if (logIt)
    {
        catLodTestDone = TRUE;
        pass = (objects > 0) && (opNoLODVal ? (drawnAbove0 == 0) : (mismatch == 0 && bandAbove0 > 0));
        dbgMessagef("lodtest: frame tick=%u fullDetail=%d switch=%d camera=%.0f objects=%u drawnAbove0=%u bandAbove0=%u "
                    "maxBand=%u verts=%u polys=%u", (unsigned)universe.univUpdateCounter, (int)opNoLODVal,
                    (int)catLodSwitchStands(), (mrCamera != NULL) ? (double)mrCamera->distance : -1.0, objects,
                    drawnAbove0, bandAbove0, maxBand, verts, polys);
        dbgMessagef("lodtest: done tick=%u fullDetail=%d objects=%u drawnAbove0=%u bandAbove0=%u result=%s",
                    (unsigned)universe.univUpdateCounter, (int)opNoLODVal, objects, drawnAbove0, bandAbove0,
                    pass ? "PASS" : "FAIL");
    }
}

// the render task's top and its end after the flip (rndRenderTask)
static void catLodStatsFrameBegin(void)
{
    sdword tick = (sdword)universe.univUpdateCounter;

    catLodFleetFocusTick();
    catLodStatsActive = (catLodStatsFrom >= 0 && !catLodStatsDone && gameIsRunning && tick >= catLodStatsFrom &&
                         tick <= catLodStatsTo);
    if (catLodStatsActive)
    {
        catLodStatsStart = SDL_GetPerformanceCounter();
        catLodStatsObjects = catLodStatsVerts = catLodStatsPolys = 0;
    }
}

static int catLodStatsCompare(const void *a, const void *b)
{
    real32 x = *(const real32 *)a, y = *(const real32 *)b;
    return (x < y) ? -1 : ((x > y) ? 1 : 0);
}

static void catLodStatsFrameEnd(void)
{
    real32 ms;

    if (catLodStatsActive)
    {
        glFinish();
        ms = (real32)((real64)(SDL_GetPerformanceCounter() - catLodStatsStart) * 1000.0 /
                      (real64)SDL_GetPerformanceFrequency());
        if (catLodStatsMs == NULL)
        {
            catLodStatsMs = memAlloc(sizeof(real32) * CAT_LOD_STATS_MAX, "catLodStatsMs", NonVolatile);
            catLodStatsVertList = memAlloc(sizeof(real32) * CAT_LOD_STATS_MAX, "catLodStatsVerts", NonVolatile);
        }
        if (catLodStatsCount < CAT_LOD_STATS_MAX)
        {
            catLodStatsMs[catLodStatsCount] = ms;
            catLodStatsVertList[catLodStatsCount] = (real32)catLodStatsVerts;
            catLodStatsCount++;
        }
        dbgMessagef("lodstats: tick=%u ms=%.3f objects=%u verts=%u polys=%u fullDetail=%d",
                    (unsigned)universe.univUpdateCounter, (double)ms, catLodStatsObjects, catLodStatsVerts,
                    catLodStatsPolys, (int)opNoLODVal);
        catLodStatsActive = FALSE;
    }
    if (!catLodStatsDone && catLodStatsFrom >= 0 && gameIsRunning && (sdword)universe.univUpdateCounter > catLodStatsTo &&
        catLodStatsCount > 0)
    {
        sdword p99 = (catLodStatsCount * 99 + 99) / 100 - 1;
        catLodStatsDone = TRUE;
        qsort(catLodStatsMs, (size_t)catLodStatsCount, sizeof(real32), catLodStatsCompare);
        qsort(catLodStatsVertList, (size_t)catLodStatsCount, sizeof(real32), catLodStatsCompare);
        dbgMessagef("lodstats: done frames=%d median=%.3f p99=%.3f max=%.3f verts_median=%.0f verts_max=%.0f fullDetail=%d "
                    "size=%dx%d", (int)catLodStatsCount, (double)catLodStatsMs[catLodStatsCount / 2],
                    (double)catLodStatsMs[p99], (double)catLodStatsMs[catLodStatsCount - 1],
                    (double)catLodStatsVertList[catLodStatsCount / 2], (double)catLodStatsVertList[catLodStatsCount - 1],
                    (int)opNoLODVal, (int)MAIN_WindowWidthActual, (int)MAIN_WindowHeightActual);   // HWI-FEATURE(ui-scale): the physical size (the cost)
    }
}

static sdword catRenderSelfTestTicks = 0;
static Ship *catRenderSelfTestDroidShip = NULL;    // M2-R6 nis-fow-camera: the damaged ship whose droids are drawn
static bool32 catRenderSelfTestStarted = FALSE;
static bool32 catRenderSelfTestDone = FALSE;
static sdword catRenderSelfTestPlayerShips = 0;
#define CAT_RENDER_TEST_START_TICK  16
#define CAT_RENDER_TEST_FOG         "60 80 120 30 40 60 1.0 3.0"

void catRenderSelfTestSet(char *string)
{
    catRenderSelfTestTicks = (sdword)strtol(string, NULL, 10);
    if (catRenderSelfTestTicks <= 0)
    {
        catRenderSelfTestTicks = 600;
    }
    oracleSetMission(string);                       // the headless Mission01 run with the virtual clock and the quit
}

bool32 catRenderSelfTestRequested(void)
{
    return (catRenderSelfTestTicks > 0);
}

void catRenderSelfTestTick(void)
{
    char fog[] = CAT_RENDER_TEST_FOG;
    vector dir, pos;
    bool32 pass;

    if (!catRenderSelfTestStarted && (sdword)universe.univUpdateCounter >= CAT_RENDER_TEST_START_TICK && mrCamera != NULL)
    {
        catRenderSelfTestStarted = TRUE;
        memset(&catRndCounts, 0, sizeof(catRndCounts));
        enableLensFlare = TRUE;
        // M2-R6 nis-fow-camera: the repair droids: the option on, and the first ship with droids (the player's if it
        //      has one; Mission01's ships at this tick are all player 1's) damaged to half health with the self-repair
        //      upgrade (0x200000), so they launch (droidUpdateArray) and are drawn; the camera focuses it below
        enableRepairbots = TRUE;
        {
            Node *node;
            udword withDroids = 0;
            for (node = universe.ShipList.head; node != NULL; node = node->next)
            {
                Ship *ship = (Ship *)listGetStructOfNode(node);
                if (ship->catRepairDroids != NULL)
                {
                    withDroids++;
                    if ((catRenderSelfTestDroidShip == NULL) ||
                        ((ship->playerowner == universe.curPlayerPtr) && (catRenderSelfTestDroidShip->playerowner != universe.curPlayerPtr)))
                    {
                        catRenderSelfTestDroidShip = ship;
                    }
                }
            }
            if (catRenderSelfTestDroidShip == NULL)
            {                                   // none in the universe (the player's Sect fleet is still in the
                                                // hyperspace ether at this tick): a scripted sProcessor (5 droids)
                vector at = { 10000.0f, 10000.0f, 0.0f };
                catRenderSelfTestDroidShip = univAddShip(sProcessor, RACE_Sect, &at, universe.curPlayerPtr, 0);
            }
            if ((catRenderSelfTestDroidShip != NULL) && (catRenderSelfTestDroidShip->catRepairDroids != NULL))
            {
                catRenderSelfTestDroidShip->catUpgradeFlags |= CAT_UPGRADE_RepairDroids;
                catRenderSelfTestDroidShip->targetable.health = catRenderSelfTestDroidShip->staticinfo->health.maxhealth * 0.5f;
            }
            dbgMessagef("rendertest: droids shipsWithDroids=%u ship=%s#%u numDroids=%d", (unsigned)withDroids,
                        (catRenderSelfTestDroidShip != NULL) ? ShipTypeToStr(catRenderSelfTestDroidShip->shiptype) : "none",
                        (catRenderSelfTestDroidShip != NULL) ? (unsigned)catRenderSelfTestDroidShip->shipID.shipNumber : 0u,
                        ((catRenderSelfTestDroidShip != NULL) && (catRenderSelfTestDroidShip->catRepairDroids != NULL)) ? (int)((CatRepairDroidArray *)catRenderSelfTestDroidShip->catRepairDroids)->numDroids : 0);
        }
        // the main view: out of the sensors manager (the headless start has no camera focus and opens it) and the
        // camera on the player's ships (any ships when it has none)
        {
            FocusCommand *focus = memAlloc(sizeofSelectCommand(20), "rendertestfocus", 0);
            Node *node;
            focus->numShips = 0;
            for (node = universe.ShipList.head; node != NULL && focus->numShips < 20; node = node->next)
            {
                Ship *ship = (Ship *)listGetStructOfNode(node);
                if (ship->playerowner == universe.curPlayerPtr)
                {
                    focus->ShipPtr[focus->numShips++] = ship;
                }
            }
            catRenderSelfTestPlayerShips = focus->numShips;
            if ((focus->numShips == 0) && (catRenderSelfTestDroidShip != NULL))
            {                                   // M2-R6 nis-fow-camera: close on the droids' ship
                focus->ShipPtr[focus->numShips++] = catRenderSelfTestDroidShip;
            }
            for (node = universe.ShipList.head; node != NULL && focus->numShips == 0; node = node->next)
            {
                focus->ShipPtr[focus->numShips++] = (Ship *)listGetStructOfNode(node);
            }
            if (smSensorsActive)
            {
                smSensorsCloseForGood();
            }
            ccCameraTimeoutOverride = TRUE;     // a focus on ships not the player's stays (no sensors manager)
            ccFocusGeneral(&universe.mainCameraCommand, focus, FALSE);
            memFree(focus);
        }
        cat_rndLensFlareNISSourceAdd();
        cat_rndLensFlareNISSourceEnable(0, TRUE);
        cat_scriptSetFogCB(NULL, fog, NULL);
        dbgMessagef("rendertest: started tick=%u lensflare=%d fog=%d density=%g",
                    (unsigned)universe.univUpdateCounter, (int)enableLensFlare, (int)catRndFogOn, (double)catRndFogDensity);
        dbgMessagef("rendertest: camera eye=(%.0f %.0f %.0f) lookat=(%.0f %.0f %.0f) distance=%.0f ships=%d player ships=%d renderlist=%d",
                    (double)mrCamera->eyeposition.x, (double)mrCamera->eyeposition.y, (double)mrCamera->eyeposition.z,
                    (double)mrCamera->lookatpoint.x, (double)mrCamera->lookatpoint.y, (double)mrCamera->lookatpoint.z,
                    (double)mrCamera->distance, (int)universe.ShipList.num, (int)catRenderSelfTestPlayerShips, (int)universe.RenderList.num);
    }
    if (catRenderSelfTestStarted && !catRenderSelfTestDone && mrCamera != NULL)
    {
        // the source stays 60000 units ahead of the camera, up and to the side of the view centre (as an NIS moves
        // its source every update)
        vecSub(dir, mrCamera->lookatpoint, mrCamera->eyeposition);
        vecNormalize(&dir);
        vecScalarMultiply(pos, dir, 60000.0f);
        vecAddTo(pos, mrCamera->eyeposition);
        pos.x += mrCamera->upvector.x * 8000.0f;
        pos.y += mrCamera->upvector.y * 8000.0f;
        pos.z += mrCamera->upvector.z * 8000.0f;
        cat_rndLensFlareNISSourceSetPosition(0, &pos);
    }
    if (catRenderSelfTestStarted && !catRenderSelfTestDone && (sdword)universe.univUpdateCounter + 1 >= catRenderSelfTestTicks)
    {
        catRenderSelfTestDone = TRUE;
        pass = (catRndCounts.frames > 0 && catRndCounts.objects + catRndCounts.ships > 0 &&
                catRndCounts.lensFlares > 0 && catRndCounts.lensFlareElements > 0 && catRndCounts.fogLayers > 0 &&
                catRndCounts.droids > 0);           // M2-R6 nis-fow-camera: the repair droids drawn
        dbgMessagef("rendertest: counts frames=%u listed=%u sorted=%u ships=%u objects=%u clouds=%u bullets=%u effects=%u "
                    "trails=%u", catRndCounts.frames, catRndCounts.listed, catRndCounts.sorted, catRndCounts.ships,
                    catRndCounts.objects, catRndCounts.clouds, catRndCounts.bullets, catRndCounts.effects,
                    catRndCounts.trails);
        dbgMessagef("rendertest: counts lensflares=%u lensflareElements=%u foglayers=%u fieldSegments=%u bolts=%u "
                    "blasts=%u edgeEffects=%u warped=%u lodTimeScale=%g", catRndCounts.lensFlares,
                    catRndCounts.lensFlareElements, catRndCounts.fogLayers, catRndCounts.fieldSegments,
                    catRndCounts.bolts, catRndCounts.blasts, catRndCounts.edgeEffects, catRndCounts.warped,
                    (double)catLodTimeScale);
        dbgMessagef("rendertest: counts droids=%u droidUpdates=%u", catRndCounts.droids, catDroidUpdates);
        dbgMessagef("rendertest: done tick=%u frames=%u objects=%u lensflares=%u foglayers=%u result=%s",
                    (unsigned)universe.univUpdateCounter, catRndCounts.frames, catRndCounts.objects + catRndCounts.ships,
                    catRndCounts.lensFlares, catRndCounts.fogLayers, pass ? "PASS" : "FAIL");
    }
}
