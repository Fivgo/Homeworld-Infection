/*=============================================================================
    Name    : render.h
    Purpose : Initialise rendering system and provide a scene render entry point.

    Created 6/21/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___RENDER_H
#define ___RENDER_H

#include "glinc.h"
#include "Camera.h"
#include "Color.h"
#include "Matrix.h"
#include "SpaceObj.h"
#include "Types.h"
#include "Task.h"

/*=============================================================================
    Switches:
=============================================================================*/
#ifdef HW_BUILD_FOR_DEBUGGING
    #define RND_BACKGROUND_CHECK    1    // check if we should display background
    #define RND_BACKGROUND_STATIC   1    // attach background to camera so it doesn't move at all
    #define RND_CAMERA_OFFSET       1    // offset camera by a certain amount to avoid lighting problems
    #define RND_DOCKLIGHT_TWEAK     0    // allow adjusting (and printing) of dock lights
    #define RND_ERROR_CHECKING      1    // basic error checking
    #define RND_FRAME_RATE          1    // display frame rate
    #define RND_GL_STATE_DEBUG      1    // dump the GL state to a file
    #define RND_GL_STATE_WINDOW     1    // dump the GL state to debug window
    #define RND_POLY_STATS          1    // display polygon stats
    #define RND_SCALECAP_TWEAK      0    // allow adjusting (and printing) of the scaling cap crap
    #define RND_VERBOSE_LEVEL       1    // control verbose printing
    #define RND_VISUALIZATION       1    // enable special visualization options
    #define RND_XYZ                 1    // display position of selected ship
#else
    #define RND_BACKGROUND_CHECK    1
    #define RND_BACKGROUND_STATIC   1
    #define RND_CAMERA_OFFSET       1
    #define RND_DOCKLIGHT_TWEAK     0
    #define RND_ERROR_CHECKING      0
    #define RND_FRAME_RATE          0
    #define RND_GL_STATE_DEBUG      0
    #define RND_GL_STATE_WINDOW     0
    #define RND_POLY_STATS          0
    #define RND_SCALECAP_TWEAK      0
    #define RND_VERBOSE_LEVEL       0
    #define RND_VISUALIZATION       0
    #define RND_XYZ                 0
#endif

/*=============================================================================
    Definitions:
=============================================================================*/
#define RND_StackSize           (1024 * 10)

#define RND_StarColor           colRGB(180, 180, 200)
#define RND_Race1BulletColor    colRGB(254, 189, 39)
#define RND_Race2BulletColor    colRGB(4, 251, 66)

#define RND_CameraOffsetX       3.0f

#define RTE_Modulate        GL_MODULATE
#define RTE_Replace         GL_REPLACE
#define RTE_Decal           GL_DECAL
#define RTE_Blend           GL_BLEND

//scale cap debug stuff
#define RND_CapScaleCapKey          NUMPAD4
#define RND_CapScaleCapKeyDec       NUMPAD2
#define RND_CapScaleCapKeyInc       NUMPAD3
#define rndCapScaleCapSlopeDelta    0.000001f

/*=============================================================================
    Type definitions:
=============================================================================*/

//structure for info passed to the rndInit() functions
typedef struct
{
    sdword width;
    sdword height;
    void   *hWnd;    // don't want to include windows.h just for this!
}
rndinitdata;

//user-defined render callback functions
typedef void (*rendercallback)(void);
typedef void (*renderfunction)(Camera *camera);

/*=============================================================================
    Data:
=============================================================================*/
extern rendercallback rndPreObjectCallback, rndPostObjectCallback;
extern udword rndFrameCount;
extern real32 rndAspectRatio;                                      //aspect ratio of rendering context
extern bool8  rndFogOn;
extern hmatrix rndCameraMatrix;
extern hmatrix rndProjectionMatrix;
extern bool32 rndNormalization;
extern bool32 rndTakeScreenshot;
extern udword rndLightingEnabled;
extern bool32 rndScissorEnabled;
extern GLfloat rndClearColor[4];

#if RND_POLY_STATS
extern sdword rndDisplayPolyStats;
extern sdword rndNumberPolys;
extern sdword rndNumberTextured;
extern sdword rndNumberSmoothed;
extern sdword rndNumberDots;
extern sdword rndNumberLines;
#endif

extern renderfunction rndMainViewRender;

/*=============================================================================
    Functions:
=============================================================================*/

//startup/shutdown the rendering module.
sdword rndInit(rndinitdata *initData);
sdword rndSmallInit(rndinitdata* initData, bool32 GL);
void rndClose(void);

//render a mission sphere using a specific camera.  Or don't render; it's your call.
void rndMainViewRenderFunction(Camera *camera);             //normal rendering mode
void rndMainViewRenderNothingFunction(Camera *camera);      //don't do anything; just return
void rndMainViewAllButRenderFunction(Camera *camera);       //compute selection info but don't render
void rndBackgroundRender(real32 radius, Camera *camera, bool32 bDrawStars);

//main render task
DECLARE_TASK(rndRenderTask);

//enable certain renderer options
void rndHintInc(void);
void rndBackFaceCullEnable(sdword bEnable);
sdword rndLightingEnable(sdword bEnable);
sdword rndTextureEnable(sdword bEnable);
void rndBillboardEnable(vector *centre);
void rndBillboardDisable(void);
udword rndTextureEnvironment(udword mode);
sdword rndNormalizeEnable(sdword bEnable);
sdword rndAdditiveBlends(sdword bAdditive);
sdword rndMaterialfv(sdword face, sdword pname, real32* params);
void rndSetClearColor(color c);
void rndGetClearColor(GLfloat c[4]);

real32 rndComputeOverlap(Ship* ship, real32 scalar);

void rndEnvironmentMap(vector* camera, vector* A, vector* B, vector* C,
                       vector* U, vector* V, vector* W);
void rndEnvironmentMapConvex(vector* camera, vector* A, vector* B, vector* C,
                       real32 convex, vector* U, vector* V, vector* W);

void rndSetScreenFill(sdword count, color c);


void rndResetGLState(void);

void rndClearToBlack(void);
void rndAllClearToBlack(void);
void rndClear(void);
void rndFlush(void);

//render utility functions
void rndRenderAHomeworld(void* camera, void *world);
bool32 rndShipVisible(SpaceObj* spaceobj, Camera* camera);
bool32 rndShipVisibleUsingCoordSys(SpaceObj* spaceobj, Camera* camera);
void rndDrawScissorBars(bool32 scissorEnabled);

// CAT (M2-R5 render): the Cataclysm object-list renderer (cat_rndRenderObjectList 0x005776f0) and its per-object
//      render state (0x00c32c00..0x00c32c9b; render.c)
#define CAT_RND_FADE_CLIP           1       // the build-in fade: a clip plane at the build progress, then its other half
#define CAT_RND_FADE_VIEW           2       // the ship view's fade-in
#define CAT_RND_CRYSTAL_FADE        0.001f  // 0x008bab20: a crystal's fade channel 2
#define CAT_RND_MAX_VISIBLE_SHIPTYPE 0x8d   // 0x00576b59: ship types above it are never drawn
#define CAT_RND_ATTR_ASTEROID_FAR   0x0200  // an asteroid attribute: drawn and faded at the carrier distances, no
                                            //      red/green ambient (rndFade 0x005753d0, 0x00579a8a); name chosen here
#define CAT_BULLET_TYPE_8           8       // a bullet type drawn by its effect only (0x005778c7, 0x00577a14)
#define CAT_UPGRADE_RepairDroids    0x00200000  // cat_shipHasUpgrade(ship, 0x200000) at 0x00579717 (the Sect self-repair)
extern bool8  catRndEdgeEffect;
extern bool8  catRndFadeFlag;
extern ubyte  catRndFadeMode;
extern sdword catRndConsoleStyle;           // CAT: 0x00878168, the console's "constyle" (the build-in fade mode)
extern bool8  catRndDrawTrails;             // CAT: 0x00878171, the console's "TRAILS"
extern bool8  catRndDrawBTG;                // CAT: 0x0087816e, the console's "BTG"
extern bool8  catRndFadeFlip;
extern real32 catRndFadeT;
extern bool8  catRndBeastStun;
extern real32 catRndBeastStunT;
extern sbyte  catRndTimedPhase;
extern real32 catRndTimedT;
extern real32 catRndTimedFrac;
extern bool8  catRndDissolve;
extern bool8  catRndRaceTint;
extern bool8  catRndAsteroidTint;
extern ubyte  catRndEMPFlicker;
extern bool8  catRndMimic;
extern real32 catRndShipViewSelectTime;
typedef struct CatRndCounts                 // port diagnostic (the render self-test), not in the exe
{
    udword frames, listed, sorted, ships, objects, clouds, bullets, effects, trails;
    udword lensFlares, lensFlareElements, fogLayers, fieldSegments, bolts, blasts, edgeEffects, warped;
    udword droids;                          // M2-R6 nis-fow-camera: repair droids drawn (cat_droidRenderArray)
} CatRndCounts;
extern CatRndCounts catRndCounts;
struct Ship;
struct CommandToDo;
struct lod;
bool32 cat_kasNaggarokEMPHitShip(struct Ship *ship);                        // 0x00575b30
bool32 cat_rndShipEMPdAndMoving(struct Ship *ship);                         // 0x00575ae0
void cat_rndSetupDissolveFlag(SpaceObj *obj);                               // 0x005759f0
void cat_rndSetupFadeFlag(SpaceObj *obj);                                   // 0x00575a20
void cat_rndSetupTimedEffectPhase(SpaceObj *obj);                           // 0x00575b70
void cat_rndSetupMimicFlag(SpaceObj *obj);                                  // 0x00575c60
void cat_rndSetupObjectEffectParams(SpaceObj *obj);                         // 0x00575f40
void cat_rndObjectEffectsSetup(SpaceObj *obj);                              // 0x00575c90
void cat_rndSortObjectsByDepth(sdword mode);                                // 0x00577660
bool32 cat_rndDepthTestEnable(bool32 enable);                               // 0x0057ae40
void cat_rndLightningBoltDraw(real32 x1, real32 y1, real32 z1, real32 x2, real32 y2, real32 z2, sdword segments,
                              real32 jitter);                               // 0x00576010
void cat_rndLinkLightningDraw(struct Ship *ship);                           // 0x00576330
void cat_rndRenderObjectList(Camera *camera, SpaceObj *obj, sdword notEffect);  // 0x005776f0
bool32 rndFade(SpaceObj* spaceobj, Camera* camera);
void rndUnFade(void);
bool32 rndCameraInside(SpaceObj* spaceobj, Camera* camera);
real32 rndDockScalar(struct Ship* ship, struct Ship* dockship, real32 nearVal, real32 farVal);
// the fog layer and the lens flares (render.c / rglu.c in the exe)
extern bool8  catRndFogOn;                                                  // 0x00c32c14
extern real32 catRndFogDensity;                                             // 0x00c32c24
void cat_rndFogLayerUpdate(void);                                           // 0x0057ce00
void cat_rndFogLayerRender(void);                                           // 0x0057d2c0
void cat_rndFogLayerTextureUnload(void);                                    // 0x0057b7b0
void cat_rndFogLayerTextureLoad(void);                                      // 0x0057b7d0
void cat_rndStateBlockGet(udword *block);                                   // CAT: the save's 0x00c32c00 block (M2-R6)
void cat_rndStateBlockSet(const udword *block);
void cat_scriptSetFogCB(char *directory, char *field, void *dataToFillIn);  // 0x0057cb70
void cat_rndFogLayerOff(void);                                              // 0x0057cd20
void cat_rndFogLayerFadeTo(real32 time, color c1, color c2, real32 speed, real32 density);   // 0x0057cd40
void cat_rndLensFlaresRender(void);                                         // 0x0057bec0
void cat_rndLensFlareTextureUnload(void);                                   // 0x0057b800
void cat_scriptSetLensFlareCB(char *directory, char *field, void *dataToFillIn);   // 0x0057b8c0
void cat_rndLensFlareNISSourceAdd(void);                                    // 0x0057cac0
void cat_rndLensFlareNISSourceRemove(void);                                 // 0x0057cb10
void cat_rndLensFlareNISSourceSetPosition(sdword unused, vector *position); // 0x0057cb20
void cat_rndLensFlareNISSourceEnable(sdword unused, sdword show);           // 0x0057cb40

// CAT: port diagnostic, not in the exe: the render self-test (/catRenderTest <frames>, harness mode render)
void catRenderSelfTestSet(char *string);
bool32 catRenderSelfTestRequested(void);
void catRenderSelfTestTick(void);

// CAT: lens flare sources (particle systems of type PART_LENSFLARES own one per particle; Particle.h). The list and
//      its add/remove are ported with the particles; the lens flare renderer (cat_rndLensFlaresRender 0x0057bec0)
//      and its scripts are stage 3.
struct catlensflaresource;
extern LinkedList catRndLensFlareSourceList;
void cat_rndLensFlareSourceAdd(struct catlensflaresource *source);
void cat_rndLensFlareSourceRemove(struct catlensflaresource *source);

#if RND_GL_STATE_DEBUG
extern bool32 rndGLStateSaving;
void rndGLStateLogFunction(char *location);
#define rndGLStateLog(s) if (rndGLStateSaving) rndGLStateLogFunction(s);
#else
#define rndGLStateLog(s)
#endif

#endif
