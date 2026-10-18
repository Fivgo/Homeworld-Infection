/*=============================================================================
    Name    : Sensors.c
    Purpose : Code for handling and rendering the sensors manager.

    Created 10/8/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "Sensors.h"
#include "FOW.h"
#include "KeyBindings.h"
#include "ShipPrint.h"
#include "InfoOverlay.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "Alliance.h"
#include "Cloak.h"
#include "Console.h"
#include "ETG.h"
#include "Key.h"
#include "LOD.h"
#include "Matrix.h"
#include "Mesh.h"
#include "Slipgate.h"
#include "Task.h"
#include "Battle.h"
#include "Blobs.h"
#include "Bounties.h"
#include "BTG.h"
#include "Camera.h"
#include "CameraCommand.h"
#include "CommandDefs.h"
#include "CommandWrap.h"
#include "Debug.h"
#include "FastMath.h"
#include "font.h"
#include "glinc.h"
#include "GravWellGenerator.h"
#include "InfoOverlay.h"
#include "Light.h"
#include "main.h"
#include "mainrgn.h"
#include "Memory.h"
#include "mouse.h"
#include "MultiplayerGame.h"
#include "NIS.h"
#include "PiePlate.h"
#include "Ping.h"
#include "prim2d.h"
#include "prim3d.h"
#include "ProximitySensor.h"
#include "Randy.h"
#include "Region.h"
#include "render.h"
#include "rglu.h"
#include "SaveGame.h"
#include "Select.h"
#include "Shader.h"
#include "ShipSelect.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "SoundEventDefs.h"
#include "StatScript.h"
#include "StringSupport.h"
#include "Subtitle.h"
#include "Tactical.h"
#include "TaskBar.h"
#include "Teams.h"
#include "Transformer.h"
#include "Tutor.h"
#include "Tweak.h"
#include "Universe.h"
#include "PopupMenu.h"
#include "UnivUpdate.h"
#include "utility.h"
#include "Vector.h"

#if defined _MSC_VER
	#define isnan(x) _isnan(x)
#endif

//located in mainrg.c
//falko's fault...not mine..long story
void toFieldSphereDraw(ShipPtr ship,real32 radius, real32 scale, color passedColour);

void (*smHoldLeft)(void);
void (*smHoldRight)(void);

#if SM_TOGGLE_SENSOR_LEVEL
void smToggleSensorsLevel(void);
#endif

/*=============================================================================
    Data:
=============================================================================*/
//set this to TRUE if you want it to be the Fleet Intel version of the SM
bool32 smFleetIntel = FALSE;

//toggles for what to display
Camera smCamera;
Camera smTempCamera;

sdword smTacticalOverlay = FALSE;
sdword smResources = TRUE;
sdword smNonResources = TRUE;
bool32 smFocusOnMothershipOnClose = FALSE;

//arrays of stars for quick star rendering
extern ubyte* stararray;

extern color HorseRaceDropoutColor;

//region handle for the sensors manager viewport
regionhandle smViewportRegion;
regionhandle smBaseRegion;
rectangle smViewRectangle;

sdword smClicking = FALSE;

//table of functions to draw a mission sphere at varying levels of visibility
void smSelectHold(void);
//point specification stuffs
/*
sdword smPointSpecMode = SPM_Idle;
sdword smCrosshairSize = SM_CrosshairSize;
sdword smMousePauseX;
sdword smMousePauseY;
*/
//colors for differing sphere statuses
color spcNormal   = SPC_Normal;
color spcSelected = SPC_Selected;
color spcTeamColor= SPC_TeamColor;

// color for printing the A for alliances
color alliescolor = colRGB(255,255,0);

//list of blobs encompassing all objects in universe
//LinkedList smBlobList;
//BlobProperties smBlobProperties;

//fog-of-war sub-blob stuff
BlobProperties smFOWBlobProperties;
real32 smFOW_AsteroidK1;
real32 smFOW_AsteroidK2;
real32 smFOW_AsteroidK3;
real32 smFOW_AsteroidK4;
real32 smFOW_DustGasK2;
real32 smFOW_DustGasK3;
real32 smFOW_DustGasK4;
real32 smFOWBlobUpdateTime = SM_FOWBlobUpdateTime;

//global flag which says the sensors manager is active
bool32 smSensorsActive = FALSE;
// CAT (M2-R5 hud): 0x00a5b1ac, mines shown in the sensors manager (kasfShowMinesInSensors 0x00498490 sets it, stage 6;
//      universeReset 0x0053df80 clears it); in mission 3 the band box takes no mine in the sensors manager without it
//      (selRectDragFunction 0x005053a0). Name chosen here
bool32 catSmShowMines = FALSE;
// CAT (M2-R6 mainrgn-popup): the sensors-view options (the popup menu's sensors-view submenu shows them checked, the
//      ToggleSV_* key commands flip them; cat_opHomeworldClassic 0x004da2c0 / cat_opCataclysmDefault set them). The
//      exe's defaults (data 0x00892b48 .. 0x00892b70 all 1; 0x00a5b19c / 0x00a5b1a4 bss 0). Names chosen here from the
//      menu rows. [M2-R6 managers: the sensors scene (cat_smViewportRenderScene and the cat_sm* draws) reads the
//      decorative asteroids, resources, meteors, FOW ranges, background, icons, ship meshes, slipgate paths, compass
//      and universe extents words; the other words have no reader in the scene.] The options screen sets them
//      (Options.c cat_opDust .. cat_opCapitalShipOverlay, M2-R6 frontend) and the options file keeps them (the Show* rows)
bool32 catSmViewDecorativeAsteroids = TRUE;         // 0x00892b48 (OP_Dust)
bool32 catSmViewResources = TRUE;                   // 0x00892b4c (OP_Resources)
bool32 catSmViewFOWRanges = TRUE;                   // 0x00892b50 (OP_SensorsSphere)
bool32 catSmViewBackground = TRUE;                  // 0x00892b54 (OP_Background)
bool32 catSmViewAreasVisited = TRUE;                // 0x00892b58 (OP_AreaExplored)
bool32 catSmViewIcons = TRUE;                       // 0x00892b5c (OP_CapitalShipOverlay)
bool32 catSmViewShipMeshes = TRUE;                  // 0x00892b60
bool32 catSmViewPieplateLines = TRUE;               // 0x00892b64
bool32 catSmViewObjectivePointers = TRUE;           // 0x00892b6c (OP_ObjectivePointers)
bool32 catSmViewSlipgatePaths = TRUE;               // 0x00892b70 (OP_SlipgatePaths)
bool32 catSmViewAreasOccupied = FALSE;              // 0x00a5b19c (OP_SensorsWireframe)
bool32 catSmViewCompass = FALSE;                    // 0x00a5b1a4 (OP_Compass)
// CAT (M2-R6 frontend): two more words of the set, named from the exe's options-file rows (ShowMeteors 0x008c1868,
//      ShowUniverseExtents 0x008c185c): OP_Resources sets the meteors with the resources (cat_opResources 0x004da110),
//      cat_opSetAllDefaults 0x004db7b0 sets them; nothing in the options screen sets the extents word
bool32 catSmViewMeteors = TRUE;                     // 0x00892b68 (.data 1)
bool32 catSmViewUniverseExtents = FALSE;            // 0x00a5b1a0 (.bss)

bool32 smSensorsDisable = FALSE;
// CAT (M2-R6 managers, sensors): the sensors manager's new state words (names chosen here)
bool32 catSmHideViewport = FALSE;   // 0x00a5b228: the viewport draws nothing (singlePlayerGameUpdate sets it at
                                    //      mission 1's end, nisStart clears it; those writers are stage 6's)
bool32 catSmForceOpen = FALSE;      // 0x00a5b22c: kasfOpenSensors 0x00496bc0 opens it past the disable flag
bool32 catSmReopen = FALSE;         // 0x00a5b230: a closing manager reopens it (catSmReopenAfterManager)
Camera catSmSavedCamera;            // 0x00c0a7e0: the sensors camera saved by a closing manager

//for updating blobs
sdword smRenderCount;
color smBackgroundColor = colFuscia, smIntBackgroundColor;
color smBlobColor = SM_BlobColor;
color smBlobColorHigh = SM_BlobColorHigh;
color smBlobColorLow = SM_BlobColorLow;
color smIntBlobColor;
color smIntBlobColorHigh;
color smIntBlobColorLow;

//for zooming
real32 smZoomTime;
region **smScrollListLeft, **smScrollListTop, **smScrollListRight, **smScrollListBottom;
sdword smScrollCountLeft, smScrollCountTop, smScrollCountRight, smScrollCountBottom;
sdword smScrollDistLeft, smScrollDistTop, smScrollDistRight, smScrollDistBottom;
sdword smScrollLeft, smScrollTop, smScrollRight, smScrollBottom;
vector smLookStart, smLookEnd;
vector smEyeStart, smEyeEnd;
bool8 smZoomingIn = FALSE, smZoomingOut = FALSE, smFocus = FALSE, smFocusTransition = FALSE;
FocusCommand *smFocusCommand = NULL;

//for selections
rectangle smSelectionRect;
//blob *smLastClosestBlob = NULL;

blob *closestBlob = NULL;
blob *probeBlob = NULL;
//for enabling 'ghost mode' after a player dies
bool32 smGhostMode = FALSE;
bool32 ioSaveState;

//option for fuzzy sensors blobs
sdword smFuzzyBlobs = TRUE;

smblurry smBlurryArray[SM_BlurryArraySize];
sdword smBlurryIndex = 0;

//cursor text font, from mouse.c
extern fonthandle mouseCursorFont;
color smCurrentWorldPlaneColor;
real32 smCurrentCameraDistance;

//switch for 'instant' transitions to sensors manager
bool32 smInstantTransition = FALSE;
real32 smCurrentZoomLength;
real32 smCurrentMainViewZoomLength;

//for horizon tick marks
smticktext smTickText[SM_MaxTicksOnScreen];
sdword smTickTextIndex;

//for panning the world plane about whilley-nilley
vector smCameraLookatPoint = {0.0f, 0.0f, 0.0f};
vector smCameraLookVelocity = {0.0f, 0.0f, 0.0f};
bool32 smCentreWorldPlane = FALSE;

//info for sorting the blobs
blob **smBlobSortList = NULL;
sdword smBlobSortListLength = 0;
sdword smNumberBlobsSorted = 0;

//for multiplayer hyperspace
sdword MP_HyperSpaceFlag=FALSE;

//for sensors weirdness
#define NUM_WEIRD_POINTS         600
#define MAX_REPLACEMENTS         5
#define WEIRDUNIVERSESTRETCH     5/4
typedef struct
{
    vector location;
    color  color;
    real32 fade;
    bool32   ping;
} weirdStruct;

static weirdStruct smWeird[NUM_WEIRD_POINTS];
udword   smSensorWeirdness=0;

//region for hyperspace button
regionhandle smHyperspaceRegion = NULL;

//what ships have a TO in the SM
// CAT (M2-R6 managers, sensors): the exe's table (.data 0x00892ba8, one byte per type, generated from the exe's data;
//      cat_smShipsDraw reads SM_Mesh, pingListDraw SM_Exclude); replaces the M2-R2 provisional HW1 re-keying
ubyte smShipTypeRenderFlags[TOTAL_NUM_SHIPS] =
{
    0,                                                 //sRecon
    0,                                                 //sAcolyte
    0,                                                 //sMimic
    0,                                                 //sLeech
    0,                                                 //sACV
    0,                                                 //sMCV
    SM_TO,                                             //sRammingFrigate
    SM_TO,                                             //sMultiBeamFrigate
    SM_TO,                                             //sHiveFrigate
    SM_Exclude,                                        //sSwarmer
    SM_TO | SM_Mesh,                                   //sDreadnought
    SM_Exclude,                                        //sDreadnoughtCannon
    SM_TO | SM_Mesh,                                   //sCarrier
    SM_Exclude,                                        //sCarrierSupport
    SM_TO,                                             //sWorker
    SM_Exclude,                                        //sRepairDroid
    SM_TO | SM_Mesh,                                   //sProcessor
    0,                                                 //sSentinel
    SM_Mesh | SM_Exclude,                              //sMothership
    SM_Mesh | SM_Exclude,                              //sMothershipSpecial
    SM_Mesh | SM_Exclude,                              //sMothershipEngineering
    SM_Mesh | SM_Exclude,                              //sMothershipArmour
    SM_Mesh | SM_Exclude,                              //sMothershipMicro
    SM_Mesh | SM_Exclude,                              //sMothershipWeapons
    SM_Mesh | SM_Exclude,                              //sMothershipDockingBay
    SM_Mesh | SM_Exclude,                              //sDestroyer
    SM_Mesh | SM_Exclude,                              //sMothershipSupport
    SM_Mesh | SM_Exclude,                              //sMothershipCommand
    SM_Mesh | SM_Exclude,                              //sMothershipEnginesLower
    SM_Mesh | SM_Exclude,                              //sMothershipEnginesUpper
    SM_Mesh | SM_Exclude,                              //sMothershipLowerSpine
    SM_Mesh | SM_Exclude,                              //sMothershipBigGun
    SM_Mesh | SM_Exclude,                              //sMothershipOreCanArmour
    0,                                                 //bRecon
    0,                                                 //bAcolyte
    0,                                                 //bInterceptor
    0,                                                 //bCloakedFighter
    0,                                                 //bAttackBomber
    0,                                                 //bACV
    0,                                                 //bMultiGunCorvette
    0,                                                 //bHeavyCorvette
    SM_TO,                                             //bMultiBeamFrigate
    SM_TO,                                             //bHiveFrigate
    SM_Exclude,                                        //bSwarmer
    SM_TO,                                             //bIonArrayFrigate
    SM_TO,                                             //bDFGFrigate
    SM_TO | SM_Mesh,                                   //bHeavyCruiser
    SM_TO | SM_Mesh,                                   //bCarrier
    SM_TO,                                             //bRammingFrigate
    SM_TO,                                             //bWorker
    SM_Exclude,                                        //bRepairDroid
    SM_TO | SM_Mesh,                                   //bProcessor
    0,                                                 //bSentinel
    SM_Mesh | SM_Exclude,                              //bMothership
    SM_Mesh | SM_Exclude,                              //bMothershipDockingBay
    SM_Mesh | SM_Exclude,                              //bMothershipEnginesLower
    SM_Mesh | SM_Exclude,                              //bSectMoship
    SM_Mesh | SM_Exclude,                              //bMothershipSupport
    0,                                                 //bCruiseMissile
    0,                                                 //bMissileCorvette
    SM_TO,                                             //kAdvanceSupportFrigate
    0,                                                 //kAttackBomber
    SM_TO | SM_Mesh,                                   //kCarrier
    0,                                                 //kCloakGenerator
    0,                                                 //kGravWellGenerator
    0,                                                 //kHeavyCorvette
    SM_TO | SM_Mesh,                                   //kHeavyCruiser
    0,                                                 //kHeavyDefender
    0,                                                 //kHeavyInterceptor
    SM_TO,                                             //kIonCannonFrigate
    0,                                                 //kMultiGunCorvette
    SM_Exclude,                                        //kProximitySensor
    0,                                                 //kSalCapCorvette
    SM_TO,                                             //kStandardDestroyer
    SM_TO,                                             //tAdvanceSupportFrigate
    0,                                                 //tAttackBomber
    SM_TO | SM_Mesh,                                   //tCarrier
    0,                                                 //tCloakGenerator
    SM_TO,                                             //tDFGFrigate
    0,                                                 //tGravWellGenerator
    SM_TO | SM_Mesh,                                   //tHeavyCruiser
    0,                                                 //tHeavyDefender
    0,                                                 //tHeavyInterceptor
    SM_TO,                                             //tIonCannonFrigate
    0,                                                 //tMinelayerCorvette
    SM_TO,                                             //tMissileDestroyer
    0,                                                 //tMultiGunCorvette
    SM_Exclude,                                        //tProximitySensor
    SM_TO,                                             //tResourceCollector
    0,                                                 //tSalCapCorvette
    SM_TO,                                             //tStandardDestroyer
    SM_TO,                                             //tStandardFrigate
    SM_TO,                                             //p1Outpost
    0,                                                 //p1Fighter
    SM_TO,                                             //p1IonArrayFrigate
    0,                                                 //p1MissileCorvette
    SM_TO | SM_Mesh,                                   //p1Mothership
    0,                                                 //p1StandardCorvette
    SM_TO | SM_Mesh,                                   //trFloatingCity
    SM_TO,                                             //trCargoBarge
    SM_TO | SM_Mesh,                                   //trMiningBase
    SM_TO | SM_Mesh,                                   //trResearchStation
    0,                                                 //trJunkYardDawg
    0,                                                 //trJunkYardHQ
    SM_TO | SM_Mesh,                                   //trGhostship
    0,                                                 //trJunk_LGun
    0,                                                 //trJunk_SGun
    0,                                                 //trResearchStationBridge
    0,                                                 //trResearchStationTower
    SM_Mesh | SM_Exclude,                              //dRKO
    SM_Mesh | SM_Exclude,                              //dBigGun
    SM_Mesh | SM_Exclude,                              //dBigGunUnit
    0,                                                 //dSlipgateSmall
    0,                                                 //dSlipgateMedium
    0,                                                 //dSlipgateLarge
    SM_Mesh | SM_Exclude,                              //dCrate
    SM_Mesh | SM_Exclude,                              //dJunk1PartA
    SM_Mesh | SM_Exclude,                              //dJunk1Shell
    SM_Mesh | SM_Exclude,                              //dJunk1Strut
    SM_Mesh | SM_Exclude,                              //dJunk2Shipwreck
    SM_Mesh | SM_Exclude,                              //dJunk3Boiler
    SM_Mesh | SM_Exclude,                              //dJunk3BoilerCasing
    0,                                                 //dMessagePod
    0,                                                 //dBuoy
    0,                                                 //dFirstPlanet
    0,                                                 //dDeadPlanet
    0,                                                 //dCrystal0
    0,                                                 //dCrystal1
    SM_Mesh | SM_Exclude,                              //dBeastDockingBay
    0,                                                 //dNISBuoy
    SM_Mesh | SM_Exclude,                              //dBeastMothership
    SM_Mesh | SM_Exclude,                              //dNomadMoon
    0,                                                 //dNomadFieldEmitter
    0,                                                 //dAcolyte
    0,                                                 //dPrisonShip
    0,                                                 //dPrisonShipInfected
    0,                                                 //dSlipgateNode
    SM_Mesh | SM_Exclude,                              //dKushanCarrier
    0,                                                 //dIonArrayFrigate
    0,                                                 //dIonCannonFrigate
    0,                                                 //dEcclesiam
    0,                                                 //dEcclesiamInfected
    0,                                                 //nisLightSource1
    0,                                                 //nisLightSource2
    0                                                  //nisLensflareSource1
};
// CAT: re-keyed to Cataclysm's 8 derelict types (ObjTypes.h). The exe has no such table: its sensors scene
//      (cat_smViewportRenderScene, M2-R6 managers) draws derelicts as crate points only; the table is read by HW1's blob
//      render (smBlobDrawClear and its callers), which nothing calls since the sensors port (left in the file, not in
//      the exe). All 0: HW1 drew none of the kept types as a mesh.
ubyte smDerelictTypeMesh[NUM_DERELICTTYPES] =
{
    0,                                          //Crate
    0,                                          //Homeworld
    0,                                          //PlanetOfOrigin
    0,                                          //PlanetOfOrigin_scarred
    0,                                          //Galaxy
    0,                                          //LavaBall
    0,                                          //MattWorld
    0,                                          //HyperspaceGate
};

GLboolean smBigPoints;

/*-----------------------------------------------------------------------------
    tweakables for the sensors manager from sensors.script
-----------------------------------------------------------------------------*/
//for blobs
sdword smBlobUpdateRate         = SM_BlobUpdateRate;
real32 smClosestMargin          = SM_ClosestMargin;
real32 smFarthestMargin         = SM_FarthestMargin;
//for selections
real32 smSelectedFlashSpeed     = SM_SelectedFlashSpeed;
real32 smFocusScalar            = SM_FocusScalar;
real32 smFocusRadius            = SM_FocusRadius;
sdword smSelectionWidth         = SM_SelectionWidth;
sdword smSelectionHeight        = SM_SelectionHeight;
//zooming in/out
real32 smZoomLength             = SM_ZoomLength;
real32 smMainViewZoomLength     = SM_MainViewZoomLength;
//for tactical overlay
color smPlayerListTextColor     = SM_PlayerListTextColor;
color smCursorTextColor         = SM_CursorTextColor;
color smTOColor                 = SM_TOColor;
sdword smPlayerListTextMargin   = SM_PlayerListTextMargin;
sdword smPlayerListMarginX      = SM_PlayerListMarginX;
sdword smPlayerListMarginY      = SM_PlayerListMarginY;
sdword smTOBottomCornerX        = SM_TOBottomCornerX;
sdword smTOBottomCornerY        = SM_TOBottomCornerY;
sdword smTOLineSpacing          = SM_TOLineSpacing;
real32 smTORadius               = 1.0f;
//for world plane
color smWorldPlaneColor         = SM_WorldPlaneColor;
color smMovePlaneColor          = SM_MovePlaneColor;
color smHeightCircleColor       = SM_HeightCircleColor;
real32 smWorldPlaneDistanceFactor = SM_WorldPlaneDistanceFactor;
sdword smWorldPlaneSegments     =  SM_WorldPlaneSegments;
real32 smMovementWorldPlaneDim  = SM_MovementWorldPlaneDim;
real32 smRadialTickStart        = SM_RadialTickStart;
real32 smRadialTickInc          = SM_RadialTickInc;
real32 smTickInnerMult          = SM_TickInnerMult;
real32 smTickOuterMult          = SM_TickOuterMult;
real32 smTickExtentFactor       = SM_TickExtentFactor;
real32 smBlobCircleSize         = SM_BlobCircleSize;
real32 smShortFootLength        = SM_ShortFootLength;
real32 smBackgroundDim          = SM_BackgroundDim;
//for horizon ticks
real32 smHorizTickAngle =       SM_HorizTickAngle;
real32 smHorizTickVerticalFactor = SM_HorizTickVerticalFactor;
real32 smHorizonLineDistanceFactor =  SM_HorizonLineDistanceFactor;
real32 smHorizonTickHorizFactor = SM_HorizonTickHorizFactor;
sdword smTickTextSpacing        = SM_TickTextSpacing;
//for movement/selections
real32 smTickLength = SM_TickLength;
real32 smClosestDistance        = SM_ClosestDistance;
//for rendering
real32 smProjectionScale        = SM_ProjectionScale;
//for hot key groups
sdword smHotKeyOffsetX          = 0;
sdword smHotKeyOffsetY          = 0;
//for panning the world plane about whilley-nilley
real32 smPanSpeedMultiplier     = SM_PanSpeedMultiplier;
real32 smPanTrack               = SM_PanTrack;
real32 smPanEvalThreshold       = SM_PanEvalThreshold;
real32 smPanUnivExtentMult      = SM_PanUnivExtentMult;
sdword smCursorPanX             = SM_CursorPanX;
sdword smCursorPanY             = SM_CursorPanY;
sdword smMaxFrameTicks          = SM_MaxFrameTicks;

/*-----------------------------------------------------------------------------
    tweakables for sensors manager from level files
-----------------------------------------------------------------------------*/
//depth cuing
real32 smDepthCueRadius = SM_DepthCueRadius;
real32 smDepthCueStartRadius = SM_DepthCueStartRadius;
real32 smCircleBorder = SM_CircleBorder;
real32 smZoomMax = SM_ZoomMax;
real32 smZoomMin = SM_ZoomMin;
real32 smZoomMinFactor = SM_ZoomMinFactor;
real32 smZoomMaxFactor = SM_ZoomMaxFactor;
real32 smInitialDistance = SM_InitialDistance;
real32 smUniverseSizeX = SM_UniverseSizeX;
real32 smUniverseSizeY = SM_UniverseSizeY;
real32 smUniverseSizeZ = SM_UniverseSizeZ;

/*-----------------------------------------------------------------------------
    Tweakables for the fleet intel version of the SM
-----------------------------------------------------------------------------*/
real32 smSkipFadeoutTime = SM_SkipFadeoutTime;

/*-----------------------------------------------------------------------------
    Tweak table for sensors.script
-----------------------------------------------------------------------------*/
scriptEntry smTweaks[] =
{
    //sensors manager tweaks
    makeEntry(smBlobUpdateRate      , scriptSetSdwordCB),
    makeEntry(smSelectedFlashSpeed  , scriptSetReal32CB),
    makeEntry(smFocusScalar         , scriptSetReal32CB),
    makeEntry(smFocusRadius         , scriptSetReal32CB),
    makeEntry(smZoomLength          , scriptSetReal32CB),
    makeEntry(smMainViewZoomLength  , scriptSetReal32CB),
    makeEntry(smPlayerListTextColor , scriptSetRGBCB),
    makeEntry(smCursorTextColor     , scriptSetRGBCB),
    makeEntry(smTOColor             , scriptSetRGBCB),
    makeEntry(smWorldPlaneColor     , scriptSetRGBCB),
    makeEntry(smPlayerListTextMargin, scriptSetSdwordCB),
    makeEntry(smPlayerListMarginX   , scriptSetSdwordCB),
    makeEntry(smPlayerListMarginY   , scriptSetSdwordCB),
    makeEntry(smTOBottomCornerX     , scriptSetSdwordCB),
    makeEntry(smTOBottomCornerY     , scriptSetSdwordCB),
    makeEntry(smTOLineSpacing       , scriptSetSdwordCB),
    makeEntry(smTickLength          , scriptSetReal32CB),
    makeEntry(smClosestDistance     , scriptSetReal32CB),
    makeEntry(smProjectionScale     , scriptSetReal32CB),
    makeEntry(smSelectionWidth      , scriptSetSdwordCB),
    makeEntry(smSelectionHeight     , scriptSetSdwordCB),
    makeEntry(smZoomMaxFactor       , scriptSetReal32CB),
    makeEntry(smZoomMinFactor       , scriptSetReal32CB),
    makeEntry(smHotKeyOffsetX       , scriptSetSdwordCB),
    makeEntry(smHotKeyOffsetY       , scriptSetSdwordCB),
    makeEntry(smClosestMargin       , scriptSetReal32CB),
    makeEntry(smFarthestMargin      , scriptSetReal32CB),
    makeEntry(smBlobCircleSize      , scriptSetReal32CB),
    makeEntry(smShortFootLength     , scriptSetReal32CB),
    makeEntry(smBlobColor           , scriptSetRGBCB),
    makeEntry(smBlobColorHigh       , scriptSetRGBCB),
    makeEntry(smBlobColorLow        , scriptSetRGBCB),
    makeEntry(smTORadius            , scriptSetReal32CB),
    makeEntry(smPanSpeedMultiplier  , scriptSetReal32CB),
    makeEntry(smPanTrack            , scriptSetReal32CB),
    makeEntry(smPanEvalThreshold    , scriptSetReal32CB),
    makeEntry(smCursorPanX          , scriptSetSdwordCB),
    makeEntry(smCursorPanY          , scriptSetSdwordCB),
    makeEntry(smMaxFrameTicks       , scriptSetSdwordCB),
    makeEntry(smSkipFadeoutTime     , scriptSetReal32CB),
    makeEntry(smWorldPlaneDistanceFactor, scriptSetReal32CB),
    makeEntry(smPanUnivExtentMult   , scriptSetReal32CB),
//    { "smBobDensityLow",            scriptSetReal32CB, &smBlobProperties.bobDensityLow },
//    { "smBobDensityHigh",           scriptSetReal32CB, &smBlobProperties.bobDensityHigh },
//    { "smBobStartSphereSize",       scriptSetReal32CB, &smBlobProperties.bobStartSphereSize },
//    { "smBobRadiusCombineMargin",   scriptSetReal32CB, &smBlobProperties.bobRadiusCombineMargin },
//    { "smBobOverlapFactor",         scriptSetReal32CB, &smBlobProperties.bobOverlapFactor },
//    { "smBobSmallestRadius",         scriptSetReal32CB, &smBlobProperties.bobSmallestRadius },
//    { "smBobBiggestRadius",         scriptSetReal32CB, &smBlobProperties.bobBiggestRadius },
//    { "smBobDoingCollisionBobs",    scriptSetBool, &smBlobProperties.bobDoingCollisionBobs },
    { "FOWBobDensityLow",           scriptSetReal32CB, &smFOWBlobProperties.bobDensityLow },
    { "FOWBobDensityHigh",          scriptSetReal32CB, &smFOWBlobProperties.bobDensityHigh },
    { "FOWBobStartSphereSize",      scriptSetReal32CB, &smFOWBlobProperties.bobStartSphereSize },
    { "FOWBobRadiusCombineMargin",  scriptSetReal32CB, &smFOWBlobProperties.bobRadiusCombineMargin },
    { "FOWBobOverlapFactor",        scriptSetBlobPropertyOverlap, &smFOWBlobProperties },
    { "FOWBobSmallestRadius",        scriptSetReal32CB, &smFOWBlobProperties.bobSmallestRadius },
    { "FOWBobBiggestRadius",        scriptSetBlobBiggestRadius, &smFOWBlobProperties },
//    { "FOWBobDoingCollisionBobs",   scriptSetBool, &smFOWBlobProperties.bobDoingCollisionBobs },
    { "FOW_AsteroidK1",             scriptSetReal32CB, &smFOW_AsteroidK1 },
    { "FOW_AsteroidK2",             scriptSetReal32CB, &smFOW_AsteroidK2 },
    { "FOW_AsteroidK3",             scriptSetReal32CB, &smFOW_AsteroidK3 },
    { "FOW_AsteroidK4",             scriptSetReal32CB, &smFOW_AsteroidK4 },
    { "FOW_DustGasK2",              scriptSetReal32CB, &smFOW_DustGasK2 },
    { "FOW_DustGasK3",              scriptSetReal32CB, &smFOW_DustGasK3 },
    { "FOW_DustGasK4",              scriptSetReal32CB, &smFOW_DustGasK4 },
    { "smFOWBlobUpdateTime",        scriptSetReal32CB, &smFOWBlobUpdateTime },
    
    END_SCRIPT_ENTRY
};

/*=============================================================================
    Private Functions:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : smStrchr
    Description : Like strchr but returns NULL when searching for a NULL terminator.
    Inputs      :
    Outputs     :
    Return      : NULL
----------------------------------------------------------------------------*/
void *smStrchr(char *string, char character)
{
    while (*string)
    {
        if (character == *string)
        {
            return(string);
        }
        string++;
    }
    return(NULL);
}

#define SM_NumberTickMarkSpacings       4
#define SM_TickSwitchHysteresis         0.9f
#define SM_SpokeLengthFactor            1.05f
typedef struct
{
    real32 radius;                              //'on' radius of the ticks
    real32 spacing;                             //spacing between ticks, meters
    real32 length;                              //factor of the circle radius
}
ticklod;
real32 smTickSwitchHysteresis = SM_TickSwitchHysteresis;
real32 smSpokeLengthFactor = SM_SpokeLengthFactor;
ticklod smTickMarkInfo[SM_NumberTickMarkSpacings] =
{
    {100000.0f, 20000.0f, 0.025f},
    {50000.0f, 10000.0f, 0.015f},
    {10000.0f, 2000.0f, 0.007f},
};
/*-----------------------------------------------------------------------------
    Name        : smWorldPlaneDraw
    Description : Draw the sensors manager world plane.
    Inputs      : z - height to draw it at so we can use same drawing code for
                    the movement pie plate.
                  c - color to use for drawing the beast
                  bDrawSpokes - true if we are to draw spokes
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
static real32 smTickFactorX[4] = {0.0f, 1.0f, 0.0f, -1.0f};
static real32 smTickFactorY[4] = {1.0f, 0.0f, -1.0f, 0.0f};
void smWorldPlaneDraw(vector *centre, bool32 bDrawSpokes, color c)
{
    real32 radius = smCamera.distance * smWorldPlaneDistanceFactor;
    real32 tickExtent = radius * smTickExtentFactor;
    real32 ringRadius;
    real32 angle, sinTheta, cosTheta;
    sdword spoke, LOD, thisLOD;
    vector spokeRim, tickEnd0, tickEnd1, p0, p1;
    real32 tickFactorX = 0.0f, tickFactorY = 1.0f, tickRadius;

    //draw the circle
    rndTextureEnable(FALSE);
    ringRadius = (smZoomMin + smZoomMax) / 2.0f * smWorldPlaneDistanceFactor;
    primCircleOutlineZ(centre, ringRadius, smWorldPlaneSegments, c);

    //draw the radial ticks
    tickEnd0.z = tickEnd1.z = centre->z;
    for (angle = smRadialTickStart; angle < 2.0f * PI; angle += smRadialTickInc)
    {
        sinTheta = (real32)sin((double)angle);
        cosTheta = (real32)cos((double)angle);
        tickEnd0.x = tickEnd1.x = ringRadius * sinTheta;
        tickEnd0.y = tickEnd1.y = ringRadius * cosTheta;
        tickEnd0.x *= smTickInnerMult;
        tickEnd0.y *= smTickInnerMult;
        tickEnd1.x *= smTickOuterMult;
        tickEnd1.y *= smTickOuterMult;
        vecAdd(p0, *centre, tickEnd0);
        vecAdd(p1, *centre, tickEnd1);
        primLine3(&p0, &p1, c);
    }
    //draw the spokes with gradations, if applicable
    if (bDrawSpokes)
    {
        spokeRim.z = centre->z;

        for (LOD = 0; LOD < SM_NumberTickMarkSpacings; LOD++)
        {
            if (smTickMarkInfo[LOD].radius <= radius)
            {
                break;
            }
        }
        for (spoke = 0; spoke < 4; spoke++)
        {
            tickFactorX = smTickFactorX[spoke];
            tickFactorY = smTickFactorY[spoke];
            spokeRim.y = spokeRim.x = ringRadius * smSpokeLengthFactor;
            spokeRim.x *= tickFactorX;
            spokeRim.y *= tickFactorY;
            vecAdd(p0, *centre, spokeRim);
            primLine3(centre, &p0, c);

            for (thisLOD = 0; thisLOD < LOD; thisLOD++)
            {
                tickRadius = smTickMarkInfo[thisLOD].spacing;
                while (tickRadius <= tickExtent)
                {
                    tickEnd0.x = radius * smTickMarkInfo[thisLOD].length * tickFactorX;
                    tickEnd0.y = radius * smTickMarkInfo[thisLOD].length * tickFactorY;
                    tickEnd1.x = -tickEnd0.x;
                    tickEnd1.y = -tickEnd0.y;
                    tickEnd0.x += tickRadius * tickFactorY;
                    tickEnd0.y += tickRadius * tickFactorX;
                    tickEnd1.x += tickRadius * tickFactorY;
                    tickEnd1.y += tickRadius * tickFactorX;
                    vecAdd(p0, *centre, tickEnd0);
                    vecAdd(p1, *centre, tickEnd1);
                    primLine3(&p0, &p1, c);
                    tickRadius += smTickMarkInfo[thisLOD].spacing;
                }
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : makeShipsNotBeDisabled
    Description : removes disabled ships from selection
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/

void makeShipsNotBeDisabled(SelectCommand *selection)
{
    sdword i;
    for(i=0;i<selection->numShips;i++)
    {
        if(selection->ShipPtr[0]->header.flags & SOF_Disabled)
        {
            selection->numShips--;
            selection->ShipPtr[i] = selection->ShipPtr[selection->numShips];
            i--;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : smHorizonLineDraw
    Description : Draw the horizon line with compass tick marks
    Inputs      : cam - context we're rendering from
                  thisBlob - blob we're rendering
                  modelView, projection - current matrices
                  distance - the distance to draw the ring at
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
#define cam     ((Camera *)voidCam)
void smHorizonLineDraw(void *voidCam, hmatrix *modelView, hmatrix *projection, real32 distance)
{
    real32 startAngle, endAngle;
    real32 x, y, radius;
    vector startPoint, horizPoint, endPoint;
    real32 angle, degAngle;
    bool32 isEnabled = FALSE;
    sdword nPrintfed;
    fonthandle oldFont = fontCurrentGet();
//    distance = smCurrentCameraDistance; //!!!
    distance *= smWorldPlaneDistanceFactor;

    startAngle = cam->angle - PI - DEG_TO_RAD(cam->fieldofview) / 2.0f - smHorizTickAngle * 2.0;
    endAngle = cam->angle - PI + DEG_TO_RAD(cam->fieldofview) / 2.0f + smHorizTickAngle * 2.0;

    angle = (real32)floor((double)(startAngle / smHorizTickAngle)) * smHorizTickAngle;
    startPoint.x = cam->lookatpoint.x + (real32)cos((double)angle) * distance;   //position of starting point
    startPoint.y = cam->lookatpoint.y + (real32)sin((double)angle) * distance;
    endPoint.z = startPoint.z = cam->lookatpoint.z;

    fontMakeCurrent(mouseCursorFont);

    isEnabled = cat_rndDepthTestEnable(FALSE);              // CAT: smHorizonLineDraw 0x005097a0
    for (smTickTextIndex = 0; angle < endAngle; angle += smHorizTickAngle)
    {
        endPoint.x = cam->lookatpoint.x + (real32)cos((double)(angle + smHorizTickAngle)) * distance;//position of current point
        endPoint.y = cam->lookatpoint.y + (real32)sin((double)(angle + smHorizTickAngle)) * distance;

        primLine3(&startPoint, &endPoint, smCurrentWorldPlaneColor);//draw the vertical tick
        horizPoint.x = endPoint.x;
        horizPoint.y = endPoint.y;
        if (cam->eyeposition.z > cam->lookatpoint.z)
        {
            horizPoint.z = cam->lookatpoint.z + cam->clipPlaneFar * smHorizTickVerticalFactor;
        }
        else
        {
            horizPoint.z = cam->lookatpoint.z - cam->clipPlaneFar * smHorizTickVerticalFactor;
        }

        //figure out where to draw the tick text
        selCircleComputeGeneral(modelView, projection, &horizPoint, 1.0f, &x, &y, &radius);
        dbgAssertOrIgnore(smTickTextIndex < SM_MaxTicksOnScreen);
        smTickText[smTickTextIndex].x = primGLToScreenX(x);
        smTickText[smTickTextIndex].y = primGLToScreenY(y);
        degAngle = -RAD_TO_DEG(angle - smHorizTickAngle);
        if (degAngle < 0.0f)
        {
            degAngle += 360.0f;
        }
        if (degAngle > 360.0f)
        {
            degAngle -= 360.0f;
        }
        if (degAngle < 10)
        {
            nPrintfed = sprintf(smTickText[smTickTextIndex].text, "00%.0f", degAngle);
        }
        else if (degAngle < 100)
        {
            nPrintfed = sprintf(smTickText[smTickTextIndex].text, "0%.0f", degAngle);
        }
        else
        {
            nPrintfed = sprintf(smTickText[smTickTextIndex].text, "%.0f", degAngle);
        }
        dbgAssertOrIgnore(nPrintfed < SM_TickTextChars);
        if (cam->eyeposition.z > cam->lookatpoint.z)
        {
            smTickText[smTickTextIndex].y -= fontHeight(" ") + smTickTextSpacing;
        }
        else
        {
            smTickText[smTickTextIndex].y += smTickTextSpacing;
        }
        smTickText[smTickTextIndex].x -= fontWidth(smTickText[smTickTextIndex].text) / 2;
        smTickTextIndex++;
        //draw the horizontal tick
        primLine3(&endPoint, &horizPoint, smCurrentWorldPlaneColor);
        horizPoint.x = (endPoint.x - cam->lookatpoint.x) * smHorizonTickHorizFactor + cam->lookatpoint.x;
        horizPoint.y = (endPoint.y - cam->lookatpoint.y) * smHorizonTickHorizFactor + cam->lookatpoint.y;
        horizPoint.z = cam->lookatpoint.z;
        primLine3(&endPoint, &horizPoint, smCurrentWorldPlaneColor);
        startPoint = endPoint;                              //draw from this point to next point next time through
    }
    cat_rndDepthTestEnable(isEnabled);
    fontMakeCurrent(oldFont);
}
#undef cam

/*-----------------------------------------------------------------------------
    Name        : smTickTextDraw
    Description : Draw the tick text created in smHorizonLineDraw
    Inputs      :
    Outputs     :
    Return      : void
    Note        : must be in 2d mode for this function
----------------------------------------------------------------------------*/
void smTickTextDraw(void)
{
    fonthandle oldFont = fontCurrentGet();

    fontMakeCurrent(mouseCursorFont);

    for (smTickTextIndex--; smTickTextIndex >= 0; smTickTextIndex--)
    {
        fontPrint(smTickText[smTickTextIndex].x, smTickText[smTickTextIndex].y,
                  smCurrentWorldPlaneColor, smTickText[smTickTextIndex].text);
    }
    fontMakeCurrent(oldFont);
}

/*-----------------------------------------------------------------------------
    Name        : smBlobDrawClear
    Description : Render all the ships inside a blob.  This would be for blobs
                    with player ships or when player has full sensors researched.
    Inputs      : camera - POV to render from
                  thisBlob - blob we're rendering
                  modelView, projection - current matrices
                  background - color of the blob being drawn to
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
//render utility functions
//bool8 rndShipVisible(SpaceObj* spaceobj, Camera* camera);
//string of ship types to be rendered as meshes
void smBlobDrawClear(Camera *camera, blob *thisBlob, hmatrix *modelView, hmatrix *projection, color background)
{
    sdword index, i;
    SpaceObj **objPtr, *obj;
    color c = colBlack;
    lod *level;
    hmatrix coordMatrixForGL;
    bool32 bFlashOn;
    SpaceObjSelection *blobObjects = thisBlob->blobObjects;
    real32 radius;
    hvector objectPos, cameraSpace, screenSpace;
    smblurry *blurry;
    sdword nShipTOs = 0;
    struct
    {
        real32 x, y;
        real32 radius;
        color c;
        ShipClass shipClass;
    }
    shipTO[SM_NumberTOs];
    sdword nBigDots = 0;
    struct
    {
        real32 x, y;
        color c;
    }
    bigDot[SM_NumberBigDots];
    real32 pointSize = 0.0;
    rectangle rect;

    //flash the selected ships
    if (fmod((double)universe.totaltimeelapsed, (double)smSelectedFlashSpeed) > smSelectedFlashSpeed / 2.0)
    {
        bFlashOn = TRUE;
    }
    else
    {
        bFlashOn = FALSE;
    }
    //draw all objects in the sphere
    for (index = 0, objPtr = blobObjects->SpaceObjPtr; index < blobObjects->numSpaceObjs; index++, objPtr++)
    {
        obj = *objPtr;
//#ifdef DEBUG_COLLBLOBS
        if (obj->header.flags & SOF_Targetable)
//#endif
        {
            selCircleCompute(modelView, projection, (SpaceObjRotImpTarg *)obj);//compute selection circle
        }
        switch (obj->header.objtype)
        {
            case OBJ_ShipType: {
                Ship* ship = (Ship*)obj;
                if (smNonResources == FALSE)
                {
                    break;
                }
                if (bitTest(ship->header.flags, SOF_Hide))
                {                                           //don't show hidden ships
                    break;
                }
                if (smTacticalOverlay)
                {
                    if (ship->playerowner == universe.curPlayerPtr &&
                        bitTest(smShipTypeRenderFlags[ship->shiptype], SM_TO))
                    {
                        if (nShipTOs < SM_NumberTOs)
                        {
                            shipTO[nShipTOs].x = ship->impactable.collInfo.selCircleX;
                            shipTO[nShipTOs].y = ship->impactable.collInfo.selCircleY;
                            shipTO[nShipTOs].radius = ship->impactable.collInfo.selCircleRadius;
                            shipTO[nShipTOs].c = colRGB(colRed(c)/TO_IconColorFade, colGreen(c)/TO_IconColorFade, colBlue(c)/TO_IconColorFade);
                            shipTO[nShipTOs].shipClass = ship->staticinfo->shipclass;
                            nShipTOs++;
                        }
                    }
                    if(ship->playerowner == universe.curPlayerPtr)
                    {
                        if(ship->shiptype == kGravWellGenerator)     // [audit: M2-R6 managers: HW1 alias retired, same value; HW1's blob render, uncalled]
                        {
                            if (((GravWellGeneratorSpec *)ship->ShipSpecifics)->GravFieldOn)
                            {
                                toFieldSphereDraw(ship,((GravWellGeneratorStatics *) ((ShipStaticInfo *)(ship->staticinfo))->custstatinfo)->GravWellRadius, 1.0f, TW_GRAVWELL_SPHERE_COLOUR);
                            }
                        }
                    }
                }
                if (bitTest(smShipTypeRenderFlags[ship->shiptype], SM_Mesh))
                {                                           //if it's a ship type to render as a mesh
                    level = lodLevelGet((void *)obj, &camera->eyeposition, &obj->posinfo.position);
                    if ((level->flags & LM_LODType) == LT_Mesh)
                    {
                        if (!(bFlashOn && (ship->header.flags & SOF_Selected)))
                        {
                            if(!bitTest(ship->header.flags,SOF_Cloaked) || ship->playerowner == universe.curPlayerPtr)
                            {       //if ship isn't cloaked, draw, or if ship is players, draw
                                glPushMatrix();
                                hmatMakeHMatFromMat(&coordMatrixForGL,&ship->rotatable.rotinfo.coordsys);
                                hmatPutVectIntoHMatrixCol4(obj->posinfo.position,coordMatrixForGL);
                                glMultMatrixf((float *)&coordMatrixForGL);//ship's rotation matrix
                                rndLightingEnable(TRUE);
                                shPushLightMatrix(&coordMatrixForGL);
                                i = ship->colorScheme;

                                if (rndShipVisible(obj, camera))
                                {
                                    bool32 wireOn;
                                    extern bool8 g_WireframeHack;

                                    wireOn = g_WireframeHack;
                                    g_WireframeHack = FALSE;
                                    if (ship->bindings != NULL)
                                    {
                                        // CAT: the sensors manager draws with no per-vertex state
                                        //      (cat_smShipMeshDraw 0x0050a0d0 passes 0)
                                        meshRenderShipHierarchy(NULL, ship->bindings,
                                                ship->obj.currentLOD,
                                                (meshdata *)level->pData, i);
                                    }
                                    else
                                    {
                                        meshRender((meshdata *)level->pData, i);
                                    }
                                    g_WireframeHack = (ubyte)wireOn;
                                }
                                shPopLightMatrix();
                                glDisable(GL_BLEND);
                                glDisable(GL_ALPHA_TEST);
                                rndLightingEnable(FALSE);
                                rndTextureEnable(FALSE);
                                glPopMatrix();
                            }
                        }
                    }
                    else
                    {                                       //else no LOD at this level
                        goto justRenderAsDot;
                    }
                }                                           //else draw it as a pixel
                else if ( (!bitTest(obj->header.flags,SOF_Cloaked) || allianceIsShipAlly((Ship *)obj, universe.curPlayerPtr)) &&
                          ((obj->attributes & ATTRIBUTES_SMColorField) != ATTRIBUTES_SMColorInvisible) )
                {       //if ship isn't cloaked, draw, or if ship is players, draw
justRenderAsDot:
#if TO_STANDARD_COLORS
                    if (ship->playerowner == universe.curPlayerPtr)
                    {
                        c = teFriendlyColor;
                    }
                    else if (allianceIsShipAlly((Ship *)obj, universe.curPlayerPtr))
                    {
                        c = teAlliedColor;
                    }
                    else
                    {
                        c = teHostileColor;
                    }
                    if (obj->attributes & ATTRIBUTES_SMColorField)  // overide with special colors
                    {
                        if ((obj->attributes & ATTRIBUTES_SMColorField) == ATTRIBUTES_SMColorYellow)
                        {
                            c = teAlliedColor;
                        }
                        else if ((obj->attributes & ATTRIBUTES_SMColorField) == ATTRIBUTES_SMColorGreen)
                        {
                            c = teFriendlyColor;
                        }
                    }
#else //SM_STANDARD_COLORS
                    c = teColorSchemes[ship->colorScheme].tacticalColor;
#endif //SM_STANDARD_COLORS
                    if (smTacticalOverlay)
                    {
                        if (ship->playerowner == universe.curPlayerPtr &&
                            bitTest(smShipTypeRenderFlags[ship->shiptype], SM_TO))
                        //if (ship->shiptype == ResourceCollector)
                        {
                            if (nShipTOs < SM_NumberTOs)
                            {
                                shipTO[nShipTOs].x = ship->impactable.collInfo.selCircleX;
                                shipTO[nShipTOs].y = ship->impactable.collInfo.selCircleY;
                                shipTO[nShipTOs].radius = ship->impactable.collInfo.selCircleRadius;
                                shipTO[nShipTOs].c = colRGB(colRed(c)/TO_IconColorFade, colGreen(c)/TO_IconColorFade, colBlue(c)/TO_IconColorFade);
                                shipTO[nShipTOs].shipClass = ship->staticinfo->shipclass;
                                nShipTOs++;
                            }
                        }
                    }
                    if (ship->header.flags & SOF_Selected)
                    {
                        if (bFlashOn)
                        {
                            c = background;
                        }
                    }

//                    if ((ship->playerowner->playerIndex == universe.curPlayerIndex))
//                    {                                       //enable double-size points
                        pointSize = 2.0f;
//                        //glPointSize(2.0f);
//                    }
//                    else
//                    {
//                        pointSize = 1.0f;
//                        //glPointSize(1.0f);
//                    }
                    rndTextureEnable(FALSE);
                    if ((!smBigPoints) && pointSize != 1.0f && ship->impactable.collInfo.selCircleRadius > 0.0f)
                    {
                        dbgAssertOrIgnore(nBigDots < SM_NumberBigDots);
                        bigDot[nBigDots].x = ship->impactable.collInfo.selCircleX;
                        bigDot[nBigDots].y = ship->impactable.collInfo.selCircleY;
                        bigDot[nBigDots].c = c;
                        nBigDots++;
                    }
                    else
                    {
                        glPointSize(pointSize);
                        primPoint3(&obj->posinfo.position, c);  //everything is rendered as a point
                        glPointSize(1.0f);
                    }
                }
            } break;
            case OBJ_AsteroidType:
                if (smResources == FALSE)
                {
                    break;
                }
                radius = obj->staticinfo->staticCollInfo.collspheresize;

                if (radius > SM_LargeResourceSize)
                {
                    pointSize = 2.0f;
                    //glPointSize(2.0f);
                }
                rndTextureEnable(FALSE);
#if TO_STANDARD_COLORS
                c = teResourceColor;
#else
                c = obj->staticinfo->LOD->pointColor;
#endif
                Asteroid* asteroid = (Asteroid*)obj;
                if ((!smBigPoints) && pointSize != 1.0f && asteroid->impactable.collInfo.selCircleRadius > 0.0f)
                {
                    dbgAssertOrIgnore(nBigDots < SM_NumberBigDots);
                    bigDot[nBigDots].x = asteroid->impactable.collInfo.selCircleX;
                    bigDot[nBigDots].y = asteroid->impactable.collInfo.selCircleY;
                    bigDot[nBigDots].c = c;
                    nBigDots++;
                }
                else
                {
                    glPointSize(pointSize);
                    primPoint3(&obj->posinfo.position, c);     //everything is rendered as a point
                    glPointSize(1.0f);
                }
                pointSize = 1.0f;
                break;
            case OBJ_NebulaType:
            case OBJ_GasType:
            case OBJ_DustType:
                if (smResources == FALSE)
                {
                    break;
                }
                memcpy(&objectPos, &obj->posinfo.position, sizeof(vector));
                objectPos.w = 1.0f;
//                c = obj->staticinfo->common.LOD->pointColor;
//                c = colWhite;
                hmatMultiplyHMatByHVec(&cameraSpace, modelView, &objectPos);//in camera space
                hmatMultiplyHMatByHVec(&screenSpace, projection, &cameraSpace);//in screen space
//                primBlurryPoint22(primGLToScreenX(screenSpace.x / screenSpace.w),    //everything is rendered as a blurry point
//                                 primGLToScreenY(screenSpace.y / screenSpace.w), c);
                if (screenSpace.z > 0.0f && smBlurryIndex < SM_BlurryArraySize)
                {
#if TO_STANDARD_COLORS
                    smBlurryArray[smBlurryIndex].c = teResourceColor;
#else
                    smBlurryArray[smBlurryIndex].c = obj->staticinfo->common.LOD->pointColor;
#endif
                    smBlurryArray[smBlurryIndex].x = primGLToScreenX(screenSpace.x / screenSpace.w);
                    smBlurryArray[smBlurryIndex].y = primGLToScreenY(screenSpace.y / screenSpace.w);
                    smBlurryIndex++;
                }
                break;
            case OBJ_DerelictType: {
                Derelict* derelict = (Derelict*)obj;
                if (derelict->derelicttype == HyperspaceGate)
                {
                    goto renderDerelictAsDot;
                }
                level = lodLevelGet((void *)obj, &camera->eyeposition, &obj->posinfo.position);
                if (bitTest(smDerelictTypeMesh[derelict->derelicttype], SM_Mesh) && (level->flags & LM_LODType) == LT_Mesh)
                {
                    if ( (!bitTest(obj->header.flags,SOF_Cloaked)) &&
                         ((obj->attributes & ATTRIBUTES_SMColorField) != ATTRIBUTES_SMColorInvisible) )
                    {       //if it isn't cloaked, draw
                        glPushMatrix();
                        hmatMakeHMatFromMat(&coordMatrixForGL,&derelict->rotatable.rotinfo.coordsys);
                        hmatPutVectIntoHMatrixCol4(obj->posinfo.position,coordMatrixForGL);
                        glMultMatrixf((float *)&coordMatrixForGL);//ship's rotation matrix
                        rndLightingEnable(TRUE);
                        shPushLightMatrix(&coordMatrixForGL);
                        i = derelict->colorScheme;

                        if (rndShipVisible(obj, camera))
                        {
                            bool32 wireOn;
                            extern bool8 g_WireframeHack;

                            wireOn = g_WireframeHack;
                            g_WireframeHack = FALSE;
                            meshRender((meshdata *)level->pData, i);
                            g_WireframeHack = (ubyte)wireOn;
                        }
                        shPopLightMatrix();
                        glDisable(GL_BLEND);
                        glDisable(GL_ALPHA_TEST);
                        rndLightingEnable(FALSE);
                        rndTextureEnable(FALSE);
                        glPopMatrix();
                    }
                }
                else
                {
renderDerelictAsDot:
#if TO_STANDARD_COLORS
                        c = teNeutralColor;
#else
                        c = obj->staticinfo->common.LOD->pointColor;
#endif
                    if (singlePlayerGame && (derelict->derelicttype == PrisonShipOld))
                    {
                        c = teFriendlyColor;
                    }

                    rndTextureEnable(FALSE);
                    primPoint3(&obj->posinfo.position,c);   //everything is rendered as a point
                }
            } break;

            default:
                break;
        }
    }

    //display the nebulae tendrils as strings
    rndTextureEnable(FALSE);
    rndLightingEnable(FALSE);

    glEnable(GL_BLEND);
    rndAdditiveBlends(FALSE);
    glShadeModel(GL_FLAT);
    for (index = 0; index < numNebulae; index++)
    {
        sdword t;
        nebulae_t* neb = &nebNebulae[index];

        glBegin(GL_LINES);
        for (t = 0; t < neb->numTendrils; t++)
        {
            color c = neb->tendrilTable[t].colour;
            glColor4ub(colRed(c), colGreen(c), colBlue(c), 192);
            glVertex3fv((GLfloat*)&neb->tendrilTable[t].a->position);
            glColor4ub(colRed(c), colGreen(c), colBlue(c), 192);
            glVertex3fv((GLfloat*)&neb->tendrilTable[t].b->position);
        }
        glEnd();
    }
    glDisable(GL_BLEND);

    if (smBlurryIndex > 0 || nShipTOs > 0 || nBigDots > 0)
    {
        toicon *icon;
        color col;
        real32 radius;

        primModeSet2();
        for (blurry = smBlurryArray; smBlurryIndex > 0; smBlurryIndex--, blurry++)
        {
            primBlurryPoint22(blurry->x, blurry->y, blurry->c);
        }

        for (index = 0; index < nShipTOs; index++)
        {
            icon = toClassIcon[shipTO[index].shipClass];
            toClassUsed[shipTO[index].shipClass][0] = TRUE;

            if (shipTO[index].radius > 0.0f)
            {
                radius = max(shipTO[index].radius, smTORadius);
                col = shipTO[index].c;
                primLineLoopStart2(1, col);

                for (i = icon->nPoints - 1; i >= 0; i--)
                {
                        primLineLoopPoint3F(shipTO[index].x + icon->loc[i].x * radius,
                                       shipTO[index].y + icon->loc[i].y * radius);
                }
            }
            primLineLoopEnd2();

        }
        for (index = 0; index < nBigDots; index++)
        {
            rect.x0 = primGLToScreenX(bigDot[index].x);
            rect.y0 = primGLToScreenY(bigDot[index].y);
            rect.x1 = rect.x0 + 2;
            rect.y1 = rect.y0 + 2;
            primRectSolid2(&rect, bigDot[index].c);
        }
        primModeClear2();
        rndLightingEnable(FALSE);
        rndTextureEnable(FALSE);
    }
}

/*-----------------------------------------------------------------------------
    Name        : SensorsWeirdnessUtilities
    Description : Utility functions for the sensors weirdness in the single player game
    Inputs      : various
    Outputs     : various
    Return      : various
----------------------------------------------------------------------------*/
//generates a random location within (and slightly outside) the bounds of the universe
vector smwGenerateRandomPoint(void)
{
    vector vec;

    vec.x = (real32)frandyrandombetween(RANDOM_AI_PLAYER,-smUniverseSizeX*WEIRDUNIVERSESTRETCH,smUniverseSizeX*WEIRDUNIVERSESTRETCH);
    vec.y = (real32)frandyrandombetween(RANDOM_AI_PLAYER,-smUniverseSizeY*WEIRDUNIVERSESTRETCH,smUniverseSizeY*WEIRDUNIVERSESTRETCH);
    vec.z = (real32)frandyrandombetween(RANDOM_AI_PLAYER,-smUniverseSizeZ*WEIRDUNIVERSESTRETCH,smUniverseSizeZ*WEIRDUNIVERSESTRETCH);

    return vec;
}

//generates a random color for the point
color smwGenerateRandomColor(void)
{
    udword colornum;
    color  col;

    colornum = (udword)randyrandom(RANDOM_AI_PLAYER, 7);

    switch (colornum)
    {
        case 0:
//            col = teFriendlyColor;
//            break;
        case 1:
        case 2:
        case 3:
            col = teHostileColor;
            break;
        case 4:
        case 5:
        case 6:
        default:
            col = teResourceColor;
            break;
    }
    return col;
}

void smwGeneratePing(vector location)
{
    sdword pingcnt;
    char name[20] = "0";

    //very small chance of a new ping
    if (((udword)randyrandom(RANDOM_AI_PLAYER, 200)) <= 1)
    {
        pingcnt = randyrandom(RANDOM_AI_PLAYER, 3);

        sprintf(name, "Weirdness%i", pingcnt);
        pingAnomalyPingRemove(name);
        pingAnomalyPositionPingAdd(name, &location);
    }
}


/*-----------------------------------------------------------------------------
    Name        : smSensorWeirdnessDraw
    Description : Draws the sensor manager malfunction in Missions 7 and 8
    Inputs      :
    Outputs     : Draws a bunch of weirdness on the screen
    Return      : void
----------------------------------------------------------------------------*/
void smSensorWeirdnessDraw(hmatrix *modelView, hmatrix *projection)
{
    udword i,j, num_replaces;
    static udword num_points;
    static udword redraw = 0;
    vector tempvec;
    rectangle rect;

    if (!redraw)
    {
        num_points   = (udword)randyrandom(RANDOM_AI_PLAYER, (smSensorWeirdness/3));
        num_points  += (2*smSensorWeirdness/3);
        redraw       = randyrandombetween(RANDOM_AI_PLAYER, 20, 30);
    }
    else
    {
        redraw--;
    }

    num_replaces = (udword)randyrandom(RANDOM_AI_PLAYER, MAX_REPLACEMENTS);

    for (i=0;i<num_replaces;i++)
    {
        //chose a random point
        j = (udword)randyrandom(RANDOM_AI_PLAYER, smSensorWeirdness);

        //generate a random location for it
        smWeird[j].location = smwGenerateRandomPoint();
        smWeird[j].color    = smwGenerateRandomColor();
        smWeird[j].fade     = 0.0f;
        smwGeneratePing(smWeird[j].location);
    }

    primModeSet2();
    for (i=0;i<num_points;i++)
    {
        // Reimplement the weirdness
        transSingleTotalTransform(&tempvec, modelView, projection, &smWeird[i].location);
        rect.x0 = primGLToScreenX(tempvec.x);
        rect.y0 = primGLToScreenY(tempvec.y);
        rect.x1 = rect.x0 + 2;
        rect.y1 = rect.y0 + 2;
        primRectSolid2(&rect, smWeird[i].color);
    }
    primModeClear2();
}

/*-----------------------------------------------------------------------------
    Name        : smSensorsWeirdnessInit
    Description : Initializes the sensors weirdness stuff
    Inputs      :
    Outputs     : Fills the smWeirdCol and smWeirdLoc arrays
    Return      : void
----------------------------------------------------------------------------*/
void smSensorWeirdnessInit(void)
{
    udword i;

    for (i=0;i<NUM_WEIRD_POINTS;i++)
    {
        smWeird[i].location    = smwGenerateRandomPoint();
        smWeird[i].color       = smwGenerateRandomColor();
        smWeird[i].ping        = FALSE;
    }
}



/*-----------------------------------------------------------------------------
    Name        : smEnemyFogOfWarBlobCallback
    Description : Sub-blob creation callback for creating fog-of-war sub-blobs
    Inputs      : superBlob - the blob we're looking in
                  obj - object to evaluate for inclusion
    Outputs     :
    Return      : TRUE if it's an enemy ship or a foggy resource
----------------------------------------------------------------------------*/
bool32 smEnemyFogOfWarBlobCallback(blob *superBlob, SpaceObj *obj)
{
    switch (obj->header.objtype)
    {
        case OBJ_ShipType:                                  //make sure it's an enemy ship
            if (allianceIsShipAlly((Ship *)obj,universe.curPlayerPtr))
            {                                               //if this is an allied ship (alliance formed while in SM)
                return(FALSE);
            }
            if (obj->header.flags & (SOF_Hide | SOF_Cloaked))
            {                                               //don't show hidden or cloaked ships
                return(FALSE);
            }
            if ((obj->attributes & ATTRIBUTES_SMColorField) == ATTRIBUTES_SMColorInvisible)
            {
                return(FALSE);
            }
            return(TRUE);
        case OBJ_BulletType:
            break;
        case OBJ_AsteroidType:
            return(TRUE);
        case OBJ_NebulaType:
            break;
        case OBJ_GasType:
        case OBJ_DustType:
            return(TRUE);
        case OBJ_DerelictType:
        case OBJ_EffectType:
        case OBJ_MissileType:
            break;
    }
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : smBlobDrawCloudy
    Description : Draw a cloudy sensors blob, such as one in the shroud.
    Inputs      : same as for the clear draw
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
#define SM_VISUALIZE_SUBBLOBS   0
void smBlobDrawCloudy(Camera *camera, blob *thisBlob, hmatrix *modelView, hmatrix *projection, color background)
{
    sdword index;
    SpaceObj **objPtr, *obj;
    color c = colBlack;
    hvector objectPos, cameraSpace, screenSpace;
    SpaceObjSelection *blobObjects = thisBlob->blobObjects;
    Node *subBlobNode;
    blob *subBlob;
    sdword nShips;
    smblurry *blurry;
    sdword nBigDots = 0;
    struct
    {
        real32 x, y;
        color c;
    }
    bigDot[SM_NumberBigDots];
    rectangle rect;
    real32 radius;
    real32 pointSize;
    real32 screenX, screenY;

    glPointSize(2.0f);
    //compute a list of sub-blobs for the enemies.  It will be deleted with the parent blob.
    if (thisBlob->subBlobs.num == BIT31)
    {                                                       //if list not yet created
        bobSubBlobListCreate(&smFOWBlobProperties, &thisBlob->subBlobs, thisBlob, smEnemyFogOfWarBlobCallback);
        thisBlob->subBlobTime = universe.totaltimeelapsed;
    }
    else
    {
        if (universe.totaltimeelapsed - thisBlob->subBlobTime >= smFOWBlobUpdateTime)
        {
            bobListDelete(&thisBlob->subBlobs);
            bobSubBlobListCreate(&smFOWBlobProperties, &thisBlob->subBlobs, thisBlob, smEnemyFogOfWarBlobCallback);
            thisBlob->subBlobTime = universe.totaltimeelapsed;
        }
    }

    //go through all sub-blobs and remove any that are hidden due to too many resources
    thisBlob->nHiddenShips = 0;
    for (subBlobNode = thisBlob->subBlobs.head; subBlobNode != NULL; subBlobNode = subBlobNode->next)
    {
        subBlob = (blob *)listGetStructOfNode(subBlobNode);
        if (subBlob->shipMass == 0.0f)
        {                                                   //if no ships in blob
            continue;                                       //skip it
        }
        // if ((k1 * #rocks + k2 * #rockRUs) / (k3 * #ships + k4 * shipMass) > 1.0)
        nShips = subBlob->nCapitalShips + subBlob->nAttackShips + subBlob->nNonCombat;
        if ((smFOW_AsteroidK1 * (real32)subBlob->nRocks + smFOW_AsteroidK2 * subBlob->nRockRUs) /
            (smFOW_AsteroidK3 * (nShips) + smFOW_AsteroidK4 * subBlob->shipMass) > 1.0f)
        {                                                   //if there are too many asteroids to see these ships
#if SM_VISUALIZE_SUBBLOBS
           primCircleOutlineZ(&subBlob->centre, subBlob->radius, 16, colRGB(0, 255, 0));
#endif
            thisBlob->nHiddenShips += nShips;
            continue;                                       //don't draw them
        }
        // if (k2 * #dustRUs / (k3 * #ships + k4 * shipMass) > 1.0)
        if ((smFOW_DustGasK2 * subBlob->nDustRUs) /
            (smFOW_DustGasK3 * nShips + smFOW_DustGasK4 * subBlob->shipMass) > 1.0f)
        {                                                   //if there's too much dust
#if SM_VISUALIZE_SUBBLOBS
//           selCircleComputeGeneral(modelView, projection, &subBlob->centre, subBlob->radius, &subBlob->screenX, &subBlob->screenY, &subBlob->screenRadius);
           primCircleOutlineZ(&subBlob->centre, subBlob->radius, 16, colRGB(255, 255, 0));
#endif
            thisBlob->nHiddenShips += nShips;
            continue;                                       //don't draw the ships
        }
        //there's not too many resources, draw them as normal
#if SM_VISUALIZE_SUBBLOBS
        primCircleOutlineZ(&subBlob->centre, subBlob->radius, 16, colWhite);
#endif
        c = teHostileColor;
        dbgAssertOrIgnore(nBigDots < SM_NumberBigDots);
//        bigDot[nBigDots].x = ((Ship *)obj)->impactable.collInfo->selCircleX;
//        bigDot[nBigDots].y = ((Ship *)obj)->impactable.collInfo->selCircleY;
        selCircleComputeGeneral(modelView, projection, &subBlob->centre, 1.0f, &screenX, &screenY, &radius);
        for (index = subBlob->blobObjects->numSpaceObjs - 1; index >= 0 ; index--)
        {                                                   //all ships in sub-blob get selection circle of sub-blob
            obj = subBlob->blobObjects->SpaceObjPtr[index];
            if (obj->header.objtype == OBJ_ShipType)
            {
                Ship* ship = (Ship*)obj;
                ship->impactable.collInfo.selCircleX = screenX;
                ship->impactable.collInfo.selCircleY = screenY;
                ship->impactable.collInfo.selCircleRadius = radius;
            }
        }
        if ((!smBigPoints) && radius > 0.0f)
        {
            bigDot[nBigDots].c = c;
            bigDot[nBigDots].x = screenX;
            bigDot[nBigDots].y = screenY;
            nBigDots++;
        }
        else
        {
            primPoint3(&subBlob->centre, c);
        }
    }
    glPointSize(1.0f);
    //draw all objects in the sphere
    for (index = 0, objPtr = blobObjects->SpaceObjPtr; index < blobObjects->numSpaceObjs; index++, objPtr++)
    {
        obj = *objPtr;
//#ifdef DEBUG_COLLBLOBS
        if (obj->header.flags & SOF_Targetable)
//#endif
        {
            selCircleCompute(modelView, projection, (SpaceObjRotImpTarg *)obj);//compute selection circle
        }
        switch (obj->header.objtype)
        {
            case OBJ_ShipType:
                if (((obj->header.flags & (SOF_Cloaked | SOF_Hide))) ||  //if hidden or cloaked
                    ((obj->attributes & ATTRIBUTES_SMColorField) == ATTRIBUTES_SMColorInvisible))
                {
                    thisBlob->nHiddenShips++;
                }
                continue;                                   //ships were drawn from sub-blobs
            case OBJ_NebulaType:
                continue;                                   //don't even draw nebulae
            case OBJ_GasType:
            case OBJ_DustType:
                if (smResources == FALSE)
                {
                    goto dontRenderThisResource;
                }
                memcpy(&objectPos, &obj->posinfo.position, sizeof(vector));
                objectPos.w = 1.0f;
                hmatMultiplyHMatByHVec(&cameraSpace, modelView, &objectPos);//in camera space
                hmatMultiplyHMatByHVec(&screenSpace, projection, &cameraSpace);//in screen space
                if (screenSpace.z > 0.0f && smBlurryIndex < SM_BlurryArraySize)
                {
#if TO_STANDARD_COLORS
                    smBlurryArray[smBlurryIndex].c = teResourceColor;
#else
                    smBlurryArray[smBlurryIndex].c = obj->staticinfo->common.LOD->pointColor;
#endif
                    smBlurryArray[smBlurryIndex].x = primGLToScreenX(screenSpace.x / screenSpace.w);
                    smBlurryArray[smBlurryIndex].y = primGLToScreenY(screenSpace.y / screenSpace.w);
                    smBlurryIndex++;
                }
                break;
            case OBJ_AsteroidType:
#if TO_STANDARD_COLORS
                c = teResourceColor;
#else
                c = obj->staticinfo->common.LOD->pointColor;
#endif
                radius = obj->staticinfo->staticCollInfo.collspheresize;

                if (radius > SM_LargeResourceSize)
                {
                    pointSize = 2.0f;
                }
                else
                {
                    pointSize = 1.0f;
                }
                Asteroid* asteroid = (Asteroid*)obj;
                if ((!smBigPoints) && pointSize != 1.0f && asteroid->impactable.collInfo.selCircleRadius > 0.0f)
                {
                    dbgAssertOrIgnore(nBigDots < SM_NumberBigDots);
                    bigDot[nBigDots].x = asteroid->impactable.collInfo.selCircleX;
                    bigDot[nBigDots].y = asteroid->impactable.collInfo.selCircleY;
                    bigDot[nBigDots].c = c;
                    nBigDots++;
                }
                else
                {
                    glPointSize(pointSize);
                    primPoint3(&obj->posinfo.position, c);
                    glPointSize(1.0f);
                }
                break;
            case OBJ_DerelictType: {
                Derelict* derelict = (Derelict*)obj;
                if (derelict->derelicttype == Crate)
                {                                           //crates are draw as grey dots
                    c = teCrateColor;
                }
                else if ((derelict->derelicttype == PrisonShipOld) &&
                         spGetCurrentMission() == MISSION_8_THE_CATHEDRAL_OF_KADESH)
                {   //!!!  Single Player game mission specific Code
                    //In Mission 8, the PrisonShipOld derelict shows up
                    //as a friendly ship
                    dbgAssertOrIgnore(nBigDots < SM_NumberBigDots);
                    bigDot[nBigDots].x = derelict->impactable.collInfo.selCircleX;
                    bigDot[nBigDots].y = derelict->impactable.collInfo.selCircleY;
                    bigDot[nBigDots].c = teFriendlyColor;
                    nBigDots++;
                }
                else
                {
#if TO_STANDARD_COLORS
                    c = teNeutralColor;
#else
                    c = obj->staticinfo->common.LOD->pointColor;
#endif
                }
                primPoint3(&obj->posinfo.position, c);
            }break;
            default:
//#ifndef DEBUG_COLLBLOBS
//                dbgAssertOrIgnore(FALSE);       // don't assert if DEBUG_COLLBLOBS because debug collblobs can have bullets,etc.
//#endif
                break;
        }

dontRenderThisResource:;
    }

    if (smBlurryIndex > 0 || nBigDots > 0)
    {
        primModeSet2();
        for (blurry = smBlurryArray; smBlurryIndex > 0; smBlurryIndex--, blurry++)
        {
            primBlurryPoint22(blurry->x, blurry->y, blurry->c);
        }
        for (index = 0; index < nBigDots; index++)
        {
            rect.x0 = primGLToScreenX(bigDot[index].x);
            rect.y0 = primGLToScreenY(bigDot[index].y);
            rect.x1 = rect.x0 + 2;
            rect.y1 = rect.y0 + 2;
            primRectSolid2(&rect, bigDot[index].c);
        }
        primModeClear2();
    }
    rndLightingEnable(FALSE);
    rndTextureEnable(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : smBlobListSort
    Description : qsort callback for sorting a list of blob pointers by sortDistance
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
int smBlobListSort(void const* p0, void const* p1) {
    blob *b0 = *((blob **)p0);
    blob *b1 = *((blob **)p1);

    if (b0->cameraSortDistance < b1->cameraSortDistance)
    {
        return(-1);
    }
    else
    {
        return(1);
    }
}

/*-----------------------------------------------------------------------------
    Name        : smBlobsSortToCamera
    Description : list - linked list of blobs
                  camera - camera to sort to
    Inputs      :
    Outputs     : (Re)allocates and fills in smBlobSortList with pointers to
                    the blobs in list, sorted by distance from camera->eyeposition
    Return      :
----------------------------------------------------------------------------*/
void smBlobsSortToCamera(LinkedList *list, Camera *camera)
{
    blob *thisBlob;
    Node *node;
    vector distance;

    //grow the blob sorting list if needed
    if (list->num > smBlobSortListLength)
    {
        smBlobSortListLength = list->num;
        smBlobSortList = memRealloc(smBlobSortList, sizeof(blob **) * smBlobSortListLength, "smBlobSortList", NonVolatile);
    }
    node = list->head;
    //do a pass through the blobs to figure their sorting distances and count the blobs
    for (node = list->head, smNumberBlobsSorted = 0; node != NULL; node = node->next, smNumberBlobsSorted++)
    {
        thisBlob = (blob *)listGetStructOfNode(node);

        vecSub(distance, camera->eyeposition, thisBlob->centre);
        thisBlob->cameraSortDistance = vecMagnitudeSquared(distance);//figure out a distance for sorting
        smBlobSortList[smNumberBlobsSorted] = thisBlob;     //store reference to blob
    }
    dbgAssertOrIgnore(smNumberBlobsSorted == list->num);
    qsort(smBlobSortList, smNumberBlobsSorted, sizeof(blob **), smBlobListSort);
}

/*-----------------------------------------------------------------------------
    Name        : smBlobDropDistance
    Description : Compute the distance from the movement cursor to a drop-down
                    line for a blob when the move mechanism is up.
    Inputs      : thisBlob - blob to use for the drop-down line
    Outputs     :
    Return      : distance from center of blob's plane point to the centre
                    point of the movement plane point.
----------------------------------------------------------------------------*/
real32 smBlobDropLineDistance(blob *thisBlob)
{
    vector planePoint = thisBlob->centre;

    planePoint.z = selCentrePoint.z;
    return(ABS(piePlanePoint.x - planePoint.x) +            //manhattan distance
            ABS(piePlanePoint.y - planePoint.y));
}

/*-----------------------------------------------------------------------------
    Name        : smBlobsDraw
    Description : Draws all blobs for the current sensors screen
    Inputs      : list - linked list of objects
    Outputs     :
    Return      : Pointer to blob most directly under the mouse cursor or NULL if none.
----------------------------------------------------------------------------*/
blob *smBlobsDraw(Camera *camera, LinkedList *list, hmatrix *modelView, hmatrix *projection, sdword sensorLevel)
{
    oval o;
    blob *thisBlob;
//    Node *node;
    blob *closestMoveBlob = NULL;
    real32 closestMoveDistance = REALlyBig, moveDistance;
    sdword index, blobIndex, nSegments, distance, closestDistance = SDWORD_Max;
    real32 distanceToOrigin, distanceToOriginSquared, factor;
    real32 closestSortDistance = REALlyBig, farthestSortDistance = REALlyNegative, highestBlob = 0.0f;
    color c;
    sdword radius;
    vector p0, p1;
    real32 x, y, screenRadius;
#if BOB_VERBOSE_LEVEL >= 2
    sdword nBlobs = 0;
#endif
    bool32 bClosestMove = FALSE;      // used for playing the tick sound when a blob is highligted
    static bool32 bPlayedSound;       // ditto
    sdword carrierHalfWidth = 0, mothershipHalfWidth = 0;
    char *carrier = NULL, *mothership = NULL;
    fonthandle oldFont = 0;

    mouseCursorObjPtr  = NULL;               //Falko: got an obscure crash where mouseCursorObjPtr was mangled, will this work?

    smBigPoints = TRUE;

    memset(toClassUsed, 0, sizeof(toClassUsed));
    closestBlob = NULL;

    distanceToOriginSquared = vecMagnitudeSquared(camera->eyeposition);
    distanceToOrigin = fmathSqrt(distanceToOriginSquared);

    smBlobsSortToCamera(list, camera);
    primModeSet2();

    //go through the list to find the farthest and closest blobs, plus the blob closest
    //to movement cursor
//    node = list->head;
//    while (node != NULL)
//        thisBlob = (blob *)listGetStructOfNode(node);
//        node = node->next;
    for (blobIndex = 0; blobIndex < smNumberBlobsSorted; blobIndex++)
    {
        thisBlob = smBlobSortList[blobIndex];
        closestSortDistance = min(closestSortDistance, thisBlob->cameraSortDistance);
        farthestSortDistance = max(farthestSortDistance, thisBlob->cameraSortDistance);
        highestBlob = max(highestBlob, ABS(thisBlob->centre.z));
        moveDistance = smBlobDropLineDistance(thisBlob);
        if (moveDistance < closestMoveDistance)
        {                                                   //closest blob to movement mechanism
            closestMoveDistance = moveDistance;
            closestMoveBlob = thisBlob;
        }
    }
    closestSortDistance = fmathSqrt(closestSortDistance);
    farthestSortDistance = fmathSqrt(farthestSortDistance);
    if (closestSortDistance >= farthestSortDistance)
    {
        closestSortDistance = farthestSortDistance - 1.0f;
    }
    closestSortDistance -= smClosestMargin;
    farthestSortDistance += smFarthestMargin;

    //do 1 pass (backwards) through all the blobs to render the blobs themselves
    for (blobIndex = smNumberBlobsSorted - 1; blobIndex >= 0; blobIndex--)
    {
        thisBlob = smBlobSortList[blobIndex];

        //compute on-screen location of the blob
        //!!! tune this circle a bit better
        selCircleComputeGeneral(modelView, projection, &thisBlob->centre, thisBlob->radius, &thisBlob->screenX, &thisBlob->screenY, &thisBlob->screenRadius);

        if (thisBlob->screenRadius <= 0.0f)
        {
            continue;
        }

        o.centreX = primGLToScreenX(thisBlob->screenX);
        o.centreY = primGLToScreenY(thisBlob->screenY);
        o.radiusX = o.radiusY = primGLToScreenScaleX(thisBlob->screenRadius);

        if ((thisBlob->flags & (BTF_Explored | BTF_ProbeDroid)) || //if player has ships in this blob
            (sensorLevel == 2 && bitTest(thisBlob->flags, BTF_UncloakedEnemies)))//or you have a sensors array and there are uncloaked enemies
        {
            //v-grad the blob color
            if (thisBlob->centre.z > 0.0f)
            {                                               //if blob is above world plane
                factor = thisBlob->centre.z / highestBlob;
                c = colBlend(smIntBlobColorHigh, smIntBlobColor, factor);
            }
            else
            {                                               //blob below the world plane
                factor = (-thisBlob->centre.z) / highestBlob;
                c = colBlend(smIntBlobColorLow, smIntBlobColor, factor);
            }
            //depth-grad the blob brightness
            factor = fmathSqrt(thisBlob->cameraSortDistance);
            factor = (factor - closestSortDistance) /
                (farthestSortDistance - closestSortDistance);
            if (factor < 0.0f)
            {
                factor = 0.0f;
            }
            if (factor > 1.0f)
            {
                factor = 1.0f;
            }
            c = colBlend(smIntBackgroundColor, c, factor);
            thisBlob->lastColor = c;
            //radius = o.radiusX * smCircleBorder / 65536;
            if (thisBlob->screenRadius <= SM_BlobRadiusMax)
            {
                radius = primGLToScreenScaleX(thisBlob->screenRadius * smCircleBorder);
                /*
                nSegments = radius * SEL_SegmentsMax / MAIN_WindowHeight + SEL_SegmentsMin;
                nSegments = min(nSegments, SEL_SegmentsMax);
                */
                nSegments = pieCircleSegmentsCompute(thisBlob->screenRadius * smCircleBorder);
                if (smFuzzyBlobs)
                {
                    primCircleBorder(o.centreX, o.centreY, o.radiusX, radius, nSegments, c);
                }
                else
                {
                    primCircleSolid2(o.centreX, o.centreY, radius, nSegments, c);
                }
            }
        }
#if BOB_VERBOSE_LEVEL >= 2
        nBlobs++;
#endif
        if (mouseInRectGeneral(o.centreX - o.radiusX, o.centreY - o.radiusY, o.centreX + o.radiusX, o.centreY + o.radiusY))
        {                                                   //if inside bounding box of circle
            distance = ABS(o.centreX - mouseCursorX()) + ABS(o.centreY - mouseCursorY());
            if (distance < closestDistance)
            {                                               //if closest yet
                closestDistance = distance;
                closestBlob = thisBlob;
            }
        }
    }

#if SM_VERBOSE_LEVEL >= 3
    fontPrintf(20, MAIN_WindowHeight / 2, colWhite, "%d Blobs", nBlobs);
#endif
    //set the selected bit on all selected ships.  This is only a temporary flag.
    for (index = 0; index < selSelected.numShips; index++)
    {
        bitSet(selSelected.ShipPtr[index]->header.flags, SOF_Selected);
    }

    lightPositionSet();
    primModeClear2();
    rndLightingEnable(FALSE);
    rndTextureEnable(FALSE);
    //draw the world plane
    //!!! always where the camera is pointing !!!
    if (smCentreWorldPlane)
    {
        p0.x = p0.y = 0.0f;
    }
    else
    {
        p0 = smCamera.lookatpoint;
    }
    p0.z = 0.0f;
    smWorldPlaneDraw(&p0, TRUE, smCurrentWorldPlaneColor);

    //draw the horizon plane
    smHorizonLineDraw(&smCamera, modelView, projection, smCurrentCameraDistance);

    primModeSet2();
    smTickTextDraw();
    primModeClear2();

    rndLightingEnable(FALSE);
    rndTextureEnable(FALSE);
    //another pass through all the blobs to render the objects in the blobs
//    node = list->head;
//    while (node != NULL)
//    {
//        thisBlob = (blob *)listGetStructOfNode(node);
//        node = node->next;
    if (smTacticalOverlay)
    {
        oldFont = fontMakeCurrent(selGroupFont3);
        carrier = ShipTypeToNiceStr(kCarrier);         // [audit: M2-R6 managers: HW1 aliases retired, same values;
        mothership = ShipTypeToNiceStr(sMothership);   //      HW1's blob render, uncalled]
        carrierHalfWidth = fontWidth(carrier) / 2;
        mothershipHalfWidth = fontWidth(mothership) / 2;
    }
    for (blobIndex = 0; blobIndex < smNumberBlobsSorted; blobIndex++)
    {
        thisBlob = smBlobSortList[blobIndex];
        if ((thisBlob->flags & (BTF_Explored | BTF_ProbeDroid)) || //if players in this blob
            (sensorLevel == 2 && bitTest(thisBlob->flags, BTF_UncloakedEnemies)))//or you have a sensors array and there are uncloaked enemies
        {
            smBlobDrawClear(camera, thisBlob, modelView, projection, thisBlob->lastColor);
            if (thisBlob->screenRadius > 0.0f)
            {
                if (smTacticalOverlay)
                {
                    if (bitTest(thisBlob->flags, BTF_Mothership))
                    {
                        primModeSet2();
                        fontPrint(primGLToScreenX(thisBlob->screenX) - mothershipHalfWidth, primGLToScreenY(thisBlob->screenY) + primGLToScreenScaleX(thisBlob->screenRadius), smTOColor, mothership);
                        primModeClear2();
                        rndLightingEnable(FALSE);           //mouse is self-illuminated
                        rndTextureEnable(FALSE);
                    }
                    else if (bitTest(thisBlob->flags, BTF_Carrier))
                    {
                        primModeSet2();
                        fontPrint(primGLToScreenX(thisBlob->screenX) - carrierHalfWidth, primGLToScreenY(thisBlob->screenY) + primGLToScreenScaleX(thisBlob->screenRadius), smTOColor, carrier);
                        primModeClear2();
                        rndLightingEnable(FALSE);           //mouse is self-illuminated
                        rndTextureEnable(FALSE);
                    }
                }
            }
        }
        else
        {
            smBlobDrawCloudy(camera, thisBlob, modelView, projection, thisBlob->lastColor);
        }
    }
    if (smTacticalOverlay)
    {
        fontMakeCurrent(oldFont);
    }

    //single player Sensor Manager malfunction
    //threshold is 10 to avoid divide by zero problems
    if (smSensorWeirdness > 10)
    {
        smSensorWeirdnessDraw(modelView, projection);
    }

    rndTextureEnable(FALSE);
    rndLightingEnable(FALSE);

    {
        Node *node;
        Asteroid *thisAsteroid;

        rndGLStateLog("Minor asteroids");
        for (node = universe.MinorSpaceObjList.head; node != NULL; node = node->next)
        {
            thisAsteroid = (Asteroid *)listGetStructOfNode(node);
            dbgAssertOrIgnore(thisAsteroid->header.objtype == OBJ_AsteroidType && thisAsteroid->asteroidtype == Asteroid0);
#if TO_STANDARD_COLORS
            c = teResourceColor;
#else
            c = thisAsteroid->staticinfo->common.LOD->pointColor;
#endif
            primPoint3(&thisAsteroid->obj.posinfo.position, c);
        }
    }

    //do a pass through the blobs to draw the drop-down lines and blob circles
    if (piePointSpecMode != PSM_Idle)
    {                                                   //if we're in movement mode
//        node = list->head;
//        while (node != NULL)
//        {
//            thisBlob = (blob *)listGetStructOfNode(node);
//            node = node->next;
        for (blobIndex = 0; blobIndex < smNumberBlobsSorted; blobIndex++)
        {
            thisBlob = smBlobSortList[blobIndex];

            //draw the blob cirle on the world plane
            p0 = thisBlob->centre;                              //position of the yellow circle
            c = colBlack;
            if ( (!(thisBlob->flags & (BTF_Explored | BTF_ProbeDroid))) && (sensorLevel != 2))
            {                                               //if it's all enemies
                if (thisBlob->nCapitalShips + thisBlob->nAttackShips + thisBlob->nNonCombat > thisBlob->nHiddenShips)
                {                                           //and there are visible ships here
                    if (closestMoveBlob == thisBlob && closestMoveDistance < closestMoveBlob->radius / SM_BlobClosenessFactor)
                    {
                        c = TW_SHIP_LINE_CLOSEST_COLOR;     //draw in closest move color
                        bClosestMove = TRUE;
                    }
                    else
                    {
                        c = TW_MOVE_ENEMY_COLOR;            //in enemy move color
                    }
                }
            }
            else if (thisBlob->nCapitalShips + thisBlob->nAttackShips + thisBlob->nNonCombat + thisBlob->nFriendlyShips > 0)
            {                                               //else player has ships here
                if (thisBlob->nFriendlyShips == 0)
                {
                    c = TW_MOVE_ENEMY_COLOR;                //in enemy move color
                }
                else if (closestMoveBlob == thisBlob && closestMoveDistance < closestMoveBlob->radius / SM_BlobClosenessFactor)
                {
                    c = TW_SHIP_LINE_CLOSEST_COLOR;         //draw in closest move color
                    bClosestMove = TRUE;
                }
                else
                {
                    c = TW_MOVE_PIZZA_COLOR;                //in move color
                }
            }
            if (c != colBlack)
            {                                               //if a color was specified, whatever it may have been
                p1.x = thisBlob->centre.x;                  //draw from blob to move plane
                p1.y = thisBlob->centre.y;
                p1.z = piePlanePoint.z;
                if (ABS(p1.z - p0.z) >= smShortFootLength)
                {
                    primLine3(&p0, &p1, c);                     //draw line to plane
                    selCircleComputeGeneral(modelView, projection, &p1, smBlobCircleSize, &x, &y, &screenRadius);
                    nSegments = pieCircleSegmentsCompute(screenRadius);
                    primCircleOutlineZ(&p1, smBlobCircleSize, nSegments, c);//draw the circle on plane
                }
            }
        }
    }

    //now remove the selected bit from all selected ships.
    for (index = 0; index < selSelected.numShips; index++)
    {
        bitClear(selSelected.ShipPtr[index]->header.flags, SOF_Selected);
    }

    if (bClosestMove && !bPlayedSound)
    {
        bPlayedSound = TRUE;
        soundEvent(NULL, UI_MoveChimes);
    }
    else if (!bClosestMove)
    {
        bPlayedSound = FALSE;
    }

    primModeSet2();
    pingListDraw(&smCamera, modelView, projection, &smViewRectangle);
    primModeClear2();
    return(closestBlob);
}

/*-----------------------------------------------------------------------------
    Name        : smBlobRenderSelected
    Description : Renders the specifed blob as though it was selected, using a colored outline.
    Inputs      : b - blob to render
                  c - color of outline to draw
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
/*
void smBlobRenderSelected(blob *b, color c)
{
    oval o;
    sdword index;
    SpaceObj **obj;
    SpaceObjSelection *blobObjects = b->blobObjects;

    primModeSet2();
    o.centreX = primGLToScreenX(b->screenX);
    o.centreY = primGLToScreenY(b->screenY);
    o.radiusX = o.radiusY = primGLToScreenScaleX(b->screenRadius);
    primOvalArcOutline2(&o, 0.0f, TWOPI, 1, 16, c);
    primModeClear2();
    rndTextureEnable(FALSE);
    rndLightingEnable(FALSE);

    for (index = 0, obj = blobObjects->SpaceObjPtr; index < blobObjects->numSpaceObjs; index++, obj++)
    {
        rndTextureEnable(FALSE);
        primPoint3(&(*obj)->posinfo.position, c);           //everything is rendered as a point
//            (*obj)->staticinfo->common.staticCollInfo.collspheresize,

    }
}
*/

/*-----------------------------------------------------------------------------
    Name        : smXXXFactorGet
    Description : Gets the current zoom factors based on the current zoom time.
    Inputs      : current - current point in function, in  frames
                  range - extent of function, in frames
    Outputs     :
    Return      : current zoom factor
    Notes       : currently a linear interpolation.
----------------------------------------------------------------------------*/
real32 smScrollFactorGet(real32 current, real32 range)
{
    real32 val;

    val = current / range;
    if (val < 0.0f)
    {
        val = 0.0f;
    }
    if (val > 1.0f)
    {
        val = 1.0f;
    }
    return(val);
}
/*
real32 smEyeFactorGet(real32 current, real32 range)
{
    return(current / range);
}
real32 smLookFactorGet(real32 current, real32 range)
{
    return(current / range);
}
*/
/*-----------------------------------------------------------------------------
    Name        : smZoomUpdate
    Description : Updates the zoom function while transitioning TO sensors manager.
    Inputs      : current - current point in function, in time
                  range - extent of function, in time
                  bUpdateCamera - update camera if true
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void smZoomUpdate(real32 current, real32 range, bool32 bUpdateCamera)
{
    real32 factor;
    vector distvec;

    // CAT (M2-R6 managers, sensors): the exe's body (0x0050b8d0): no screen-side scrolling (the Cataclysm viewport is
    //      the whole screen); the background and blob colours, the clear colour, the BTG dimmed through channel 0
    //      (the exe keeps HW1's dimming here; smSensorsCloseForGood resets it), the world plane colour (undimmed while
    //      the movement pie is up), the far clip blend, the camera blend, and the factor kept for the range circles
    factor = smScrollFactorGet(current, range);
    smIntBackgroundColor = colMultiplyClamped(smBackgroundColor, factor);
    smIntBlobColor = colMultiplyClamped(smBlobColor, factor);
    smIntBlobColorHigh = colMultiplyClamped(smBlobColorHigh, factor);
    smIntBlobColorLow = colMultiplyClamped(smBlobColorLow, factor);
    rndSetClearColor(colRGBA(colRed(smIntBackgroundColor),
                             colGreen(smIntBackgroundColor),
                             colBlue(smIntBackgroundColor),
                             255));
    btgSetColourMultiplier(0, (1.0f - factor) * (1.0f - smBackgroundDim) + smBackgroundDim);
    if (piePointSpecMode != PSM_Idle)
    {
        smCurrentWorldPlaneColor = smWorldPlaneColor;
    }
    else
    {
        smCurrentWorldPlaneColor = colMultiplyClamped(smWorldPlaneColor, factor);
    }
    //blend between the far clip distances of the 2 cameras
    smCurrentCameraDistance = (smCamera.clipPlaneFar - mrCamera->clipPlaneFar) * factor + mrCamera->clipPlaneFar;
    if (bUpdateCamera)
    {
        vecVectorsBlend(&smCamera.lookatpoint, &smLookStart, &smLookEnd, factor);
        vecVectorsBlend(&smCamera.eyeposition, &smEyeStart, &smEyeEnd, factor);
        vecSub(distvec, smCamera.eyeposition, smCamera.lookatpoint);
        GetDistanceAngleDeclination(&smCamera, &distvec);
    }
    catSmZoomFactor = factor;
}

/*-----------------------------------------------------------------------------
    Name        : smSensorsCloseForGood
    Description : Closes sensors manager after zooming in.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void smSensorsCloseForGood(void)
{
    // CAT (M2-R6 managers, sensors): the exe's body (0x0050ba90): no scroll lists, I/O lock or taskbar flag; the main
    //      region's mouse modes cleared (catMrRightMode / catMrSelecting), the BTG multiplier back to 1
    if (tutorial)
    {
        tutGameMessage("Game_SensorsClose");
    }
    mrRenderMainScreen = TRUE;
    feScreenDeleteFlags(smBaseRegion, 0);
    smBaseRegion = NULL;
    rndSetClearColor(colRGBA(colRed(universe.backgroundColor),
                             colGreen(universe.backgroundColor),
                             colBlue(universe.backgroundColor),
                             255));
    smSensorsActive = FALSE;
    universe.dontUpdateRenderList = FALSE;
    // CAT: the exe calls a glcompat hook here (0x00b3a70c with 0x5001 when 0x00b3a6d8); no glcompat in the SDL build
    universe.mainCameraCommand.actualcamera.ignoreZoom = FALSE;
    mouseCursorShow();
    catMrRightMode = 0;
    catMrSelecting = FALSE;
    if (smFocusOnMothershipOnClose)
    {
        ccFocusOnMyMothership(&universe.mainCameraCommand);
        smFocusOnMothershipOnClose = FALSE;
    }
    smZoomingIn = FALSE;
    smZoomingOut = FALSE;
    mouseClipToRect(NULL);                                  //make sure mouse does not stay locked to the SM viewport region
    MP_HyperSpaceFlag = FALSE;
    btgSetColourMultiplier(0, 1.0f);                        //make sure we always come out of the SM with the BTG in good order
    if (catMotionCues)
    {                                                       // CAT: the motion cues restart (smSensorsCloseForGood 0x0050ba90)
        cat_mcSetMode(&catMotionCueState, 1);
        cat_mcResetRequest(&catMotionCueState);
    }
}

void smBlobSphereDraw(blob *closestBlob)
{
    udword angle = 0;

    vector origin = {0.0, 0.0, 0.0};
    hmatrix rotmat;
    while(angle < 180)
    {
        hmatMakeRotAboutZ(&rotmat,cos(DEG_TO_RAD(angle)),sin(DEG_TO_RAD(angle)));
         hmatPutVectIntoHMatrixCol4(closestBlob->centre, rotmat);

        glPushMatrix();
        glMultMatrixf((GLfloat *)&rotmat);

        primCircleOutline3(&origin, closestBlob->radius, 8, 0, colWhite, X_AXIS);

        glPopMatrix();
        angle+=40;
    }

    angle = 0;
    while(angle < 180)
     {
         hmatMakeRotAboutX(&rotmat,cos(DEG_TO_RAD(angle)),sin(DEG_TO_RAD(angle)));
         hmatPutVectIntoHMatrixCol4(closestBlob->centre, rotmat);

         glPushMatrix();
         glMultMatrixf((GLfloat *)&rotmat);

         primCircleOutline3(&origin, closestBlob->radius, 8, 0, colWhite, Z_AXIS);

         glPopMatrix();
         angle+=40;
     }


}

/*-----------------------------------------------------------------------------
    Name        : smAllBlobsPiePlateDraw
    Description : Draws lines from all blobs to the pie plate for the
                    movement mechanism.
    Inputs      : distance -distance from camera to centre of pie plate
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
/*
void smAllBlobsPiePlateDraw(real32 distance)
{
    Node *node;
    blob *thisBlob;
    bool32 dohackloop = TRUE;
    real32 closestDistance = REALlyBig, length;
    vector mouse_pos,tempvec;
    real32 tempreal;
    blob *closestBlob = NULL;

    probeBlob = NULL;


    if(mrNeedProbeHack())
        dohackloop = TRUE;
    else
        dohackloop = FALSE;


    node = smBlobList.head;

    if (piePointSpecZ != 0.0f)
    {                                       //if a height was specified
        mouse_pos = pieHeightPoint;
    }
    else
    {
        mouse_pos = piePlanePoint;         //else move to point on plane
    }


    while (node)
    {
        thisBlob = (blob *)listGetStructOfNode(node);
        node = node->next;

        if(!dohackloop)
        {
            if (!bitTest(thisBlob->flags, BTF_Explored) &&
                universe.curPlayerPtr->sensorLevel != 2)
            {
                continue;
            }
            length = smBlobDropLineDraw(thisBlob, TW_SHIP_LINE_COLOR);
            if (length < closestDistance)
            {                                                   //if this ship closest so far
                closestDistance = length;
                closestBlob = thisBlob;
            }
        }
        else
        {
            //do closestblob checking first in this loop

            vecSub(tempvec,mouse_pos,thisBlob->centre);
            tempreal  = fsqrt(vecMagnitudeSquared(tempvec));
            if(tempreal < closestDistance)
            {
                closestDistance = tempreal;
                closestBlob = thisBlob;
            }
            if (!bitTest(thisBlob->flags, BTF_Explored) &&
                universe.curPlayerPtr->sensorLevel != 2)
            {
                continue;
            }
            length = smBlobDropLineDraw(thisBlob, TW_SHIP_LINE_COLOR);
        }
    }
    glDisable(GL_DEPTH_TEST);
    if(!dohackloop)
    {
        if (closestBlob != NULL && closestDistance < closestBlob->radius / SM_BlobClosenessFactor)
        {
            smBlobDropLineDraw(closestBlob, TW_SHIP_LINE_CLOSEST_COLOR);
        }
    }
    else
    {
        if (closestBlob != NULL)
        {
            probeBlob = closestBlob;
            vecSub(tempvec,mouse_pos,probeBlob->centre);
            tempreal = fsqrt(vecMagnitudeSquared(tempvec));
            if(tempreal < probeBlob->radius)
            {
                smBlobSphereDraw(closestBlob);
            }
            else
            {
                probeBlob = NULL;
            }
        }
    }
    glEnable(GL_DEPTH_TEST);
}
*/

/*-----------------------------------------------------------------------------
    Name        : smMovePlaneDraw
    Description : Draw the movement plane for the sensors manager.
    Inputs      : distance - distance from camera to selection centre
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
/*
void smMovePlaneDraw(real32 distance)
{
    smWorldPlaneDraw(pieHeightPoint.z, FALSE, smMovePlaneColor);
}
*/
/*-----------------------------------------------------------------------------
    Name        : smBabyRattleDraw
    Description : Draw the movement lines and cursor for the movement mechanism
                    while in the sensors manager.
    Inputs      : distance - distance from camera to selection centre
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
/*
void smBabyRattleDraw(real32 distance)
{
    ;
}
*/
/*-----------------------------------------------------------------------------
    Name        : smClickedOnPlayer
    Description : returns the a pointer to the player you right clicked on.  NULL otherwise/
    Inputs      : none
    Outputs     : player clicked on or NULL
    Return      : void
----------------------------------------------------------------------------*/
sdword smClickedOnPlayer(rectangle *viewportRect)
{
    fonthandle oldfont;
    rectangle  playerColorRect;
    sdword     index;

    oldfont = fontMakeCurrent(selGroupFont2);

    playerColorRect.y1 = viewportRect->y1 - smPlayerListMarginY;
    playerColorRect.y0 = playerColorRect.y1 - fontHeight(" ");
    playerColorRect.x0 = viewportRect->x0 + smPlayerListMarginX;
    playerColorRect.x1 = playerColorRect.x0 + fontHeight(" ");

    //draw the list of player names/colors
    for (index = universe.numPlayers - 1; index >= 0; index--)
    {
        if (universe.players[index].playerState==PLAYER_ALIVE)
        {
            playerColorRect.x1 = fontWidth(playerNames[index]) + fontHeight(" ")*2;
            if ((mouseInRect(&playerColorRect)) && (index!=sigsPlayerIndex))
            {
                return (index);
            }
            playerColorRect.y0 -= smPlayerListMarginY + fontHeight(" ");//update the position
            playerColorRect.y1 -= smPlayerListMarginY + fontHeight(" ");
        }
    }

    fontMakeCurrent(oldfont);

    return (-1);
}


/*-----------------------------------------------------------------------------
    Name        : smPlayerNamesDraw
    Description : Draw the names of the players in a multi-player game.
    Inputs      : viewportRect - boundaries of the current viewport
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void smPlayerNamesDraw(rectangle *viewportRect)
{
    fonthandle oldfont;
    sdword index;
    rectangle playerColorRect;
    color c;
    char buffer[50];
    sdword bounty;

    oldfont = fontMakeCurrent(selGroupFont2);

    playerColorRect.y1 = viewportRect->y1 - smPlayerListMarginY;
    playerColorRect.y0 = playerColorRect.y1 - fontHeight(" ");
    playerColorRect.x0 = viewportRect->x0 + smPlayerListMarginX;
    playerColorRect.x1 = playerColorRect.x0 + fontHeight(" ");

    //draw the list of player names/colors
    for (index = universe.numPlayers - 1; index >= 0; index--)
    {
        if (universe.players[index].playerState==PLAYER_ALIVE)
        {
            sdword x;
            bool32 playerhasdroppedOutOrQuit = playerHasDroppedOutOrQuit(index);

            c = teColorSchemes[index].textureColor.base;
            if (playerhasdroppedOutOrQuit)
            {
                c = HorseRaceDropoutColor;
            }

            primRectSolid2(&playerColorRect, c);                 //draw colored rectangle
            x = playerColorRect.x1 + smPlayerListTextMargin;
            fontPrint(x, //print the player name in a stock color
                      playerColorRect.y0, playerhasdroppedOutOrQuit ? HorseRaceDropoutColor : smPlayerListTextColor, playerNames[index]);
            x += fontWidth(playerNames[index])+fontWidth(" ");
            if (index != sigsPlayerIndex)
            {
                if ((allianceArePlayersAllied(&universe.players[sigsPlayerIndex],&universe.players[index])) ||
                    ((universe.players[sigsPlayerIndex].Allies!=0)&&(index == sigsPlayerIndex)))
                {
                    fontPrint(x,
                              playerColorRect.y0, playerhasdroppedOutOrQuit ? HorseRaceDropoutColor : alliescolor, strGetString(strPlayersAllied));
                }
            }

            x += fontWidth(strGetString(strPlayersAllied));
            if((!singlePlayerGame) && (tpGameCreated.bountySize != MG_BountiesOff))
            {
                //draw bounty values
                fontPrint(x,
                          playerColorRect.y0, playerhasdroppedOutOrQuit ? HorseRaceDropoutColor : colWhite, "B: ");
                bounty = getPlayerBountyRender(&universe.players[index]);
                /*itoa(bounty,buffer,10);*/
                sprintf(buffer, "%d", bounty);
                x += fontWidth("B: ");

                fontPrint(x, playerColorRect.y0, playerhasdroppedOutOrQuit ? HorseRaceDropoutColor : colWhite, buffer);

                x += fontWidth(buffer) + fontWidth(" ");
            }

            if (playerhasdroppedOutOrQuit)
            {
                fontPrint(x,playerColorRect.y0, c, (playersReadyToGo[index] == PLAYER_QUIT) ? strGetString(strQuit) : strGetString(strDroppedOut));
            }

            playerColorRect.y0 -= smPlayerListMarginY + fontHeight(" ");//update the position
            playerColorRect.y1 -= smPlayerListMarginY + fontHeight(" ");
        }
    }

    fontMakeCurrent(oldfont);
}
/*-----------------------------------------------------------------------------
    Name        : smCursorTextDraw
    Description : Draw sensors manager tactical overlay.  This includes the
                    player names/colors and a breakdown of what's in a mission sphere.
    Inputs      : viewportRect - boundaries of the current viewport
                  selectedBlob - blob under mouse cursor, or NULL if none.
                  sensorLevel - current sensors level
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void smCursorTextDraw(rectangle *viewportRect, blob *selectedBlob, sdword sensorLevel)
{
    char *cursorText = NULL;
    SpaceObj *object;
    fonthandle fhSave;

    //now draw the cursor text to reflect what is under the mouse cursor
    if (selectedBlob != NULL)
    {                                                       //if the mouse is over a blob
        if ((selectedBlob->flags & (BTF_Explored | BTF_ProbeDroid)))
        {                                                   //if this blob has been explored
            if ((object = selClickFromArray(selectedBlob->blobObjects->SpaceObjPtr,
                selectedBlob->blobObjects->numSpaceObjs, mouseCursorX(),mouseCursorY())) != NULL)
            {
                switch (object->header.objtype)
                {
                    case OBJ_ShipType:
                        if (((Ship *)object)->playerowner == universe.curPlayerPtr)
                        {
//                            cursorText = strGetString(strPlayerUnits);
                            cursorText = ShipTypeToNiceStr(((Ship *)object)->shiptype);
                        }
                        else if (allianceIsShipAlly((Ship *)object, universe.curPlayerPtr))
                        {
                            cursorText = strGetString(strAlliedUnits);
                        }
                        else
                        {
                            cursorText = strGetString(strEnemyUnits);
                        }
                        break;
                    case OBJ_AsteroidType:
                    case OBJ_NebulaType:
                    case OBJ_GasType:
                    case OBJ_DustType:
                        cursorText = strGetString(strResources);
                        break;
                    case OBJ_DerelictType:
                        switch (((Derelict *)object)->derelicttype)
                        {
                            case PrisonShipOld:
                            case PrisonShipNew:
                            case Shipwreck:
                            case JunkAdvanceSupportFrigate:
                            case JunkCarrier:
                            case JunkDDDFrigate:
                            case JunkHeavyCorvette:
                            case JunkHeavyCruiser:
                            case JunkIonCannonFrigate:
                            case JunkLightCorvette:
                            case JunkMinelayerCorvette:
                            case JunkMultiGunCorvette:
                            case JunkRepairCorvette:
                            case JunkResourceController:
                            case JunkSalCapCorvette:
                            case JunkStandardFrigate:
                                cursorText = strGetString(strDerelictShip);
                                break;
                            default:
                                cursorText = NULL;
                                break;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
        else
        {                                                   //else unexplored blob
            if (sensorLevel == 0)
            {                                               //don't know anything on plain blobs
                cursorText = strGetString(strUnexplored);
            }
            else
            {                                               //else we've got some sensors available
                if ((object = selClickFromArray(selectedBlob->blobObjects->SpaceObjPtr,
                    selectedBlob->blobObjects->numSpaceObjs, mouseCursorX(),mouseCursorY())) != NULL)
                {
                    switch (object->header.objtype)
                    {
                        case OBJ_ShipType:
                            cursorText = strGetString(strEnemyUnits);
                            break;
                        case OBJ_AsteroidType:
                            cursorText = strGetString(strAsteroids);
                            break;
                        case OBJ_NebulaType:
                            cursorText = strGetString(strNebula);
                            break;
                        case OBJ_GasType:
                            cursorText = strGetString(strGasClouds);
                            break;
                        case OBJ_DustType:
                            cursorText = strGetString(strDustClouds);
                            break;
                        case OBJ_DerelictType:
                            cursorText = strGetString(strDerelictShip);
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }
    //cursor text chosen, print the cursor text if applicable
    if (cursorText != NULL)
    {
        fhSave = fontMakeCurrent(mouseCursorFont);
        fontPrint(viewportRect->x0 + smPlayerListMarginX,
                  viewportRect->y1 - fontHeight(cursorText) - smPlayerListMarginY,
                  TW_CURSORTEXT_COLOR, cursorText);
        fontMakeCurrent(fhSave);
    }
}

/*-----------------------------------------------------------------------------
    Name        : smTacticalOverlayDraw
    Description : Draw the tactical overlay for sensors manager, including
                  resource types and such
    Inputs      : thisBlob - blob to draw
                  viewportRect - rectangle encompassing the sensors manager
                  sensorLevel - current sensors level
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
/*
void smTacticalOverlayDraw(blob *thisBlob, rectangle *viewportRect, sdword sensorLevel)
{
    sdword x, y, yStart, xSpacing;

    dbgAssertOrIgnore(thisBlob != NULL);
    x = mouseCursorX() + smTOBottomCornerX;
    yStart = y = mouseCursorY() - fontHeight(" ") + smTOBottomCornerY;
    if (thisBlob->RUs != 0)
    {                                                   //if there's resources in the blob
        if ((sensorLevel == 2 && bitTest(thisBlob->flags, BTF_Explored)) || bitTest(thisBlob->flags, BTF_ProbeDroid))
        {                                               //if max sensor level, print breakdown of resources in the blob
            xSpacing = fontWidth(strGetString(strResourcesRes))+fontWidth(" ");
            if (thisBlob->nGasRUs != 0)
            {
                fontPrintf(x + xSpacing, y, smTOColor, "%d %s", thisBlob->nGasRUs, strGetString(strGas));
                y -= fontHeight(" ") + smTOLineSpacing;
            }
            if (thisBlob->nDustRUs != 0)
            {
                fontPrintf(x + xSpacing, y, smTOColor, "%d %s", thisBlob->nDustRUs,strGetString(strDust));
                y -= fontHeight(" ") + smTOLineSpacing;
            }
            if (thisBlob->nRockRUs != 0)
            {
                fontPrintf(x + xSpacing, y, smTOColor, "%d %s", thisBlob->nRockRUs, strGetString(strRock));
                y -= fontHeight(" ") + smTOLineSpacing;
            }
            y += fontHeight(" ") + smTOLineSpacing;
            fontPrintf(x, y, smTOColor, "%s ",strGetString(strResourcesRes));
            y -= fontHeight(" ") + smTOLineSpacing;
        }
        else if (sensorLevel >= 1 || bitTest(thisBlob->flags, BTF_Explored))
        {                                               //else crappy sensors; just print the types
            fontPrintf(x, y, smTOColor, "%s %s %s %s", strGetString(strResourcesRes),
                (thisBlob->flags & (BTF_Asteroid)) ? strGetString(strRock) : "",
                (thisBlob->flags & (BTF_DustCloud)) ? strGetString(strDust) : "",
                (thisBlob->flags & (BTF_GasCloud | BTF_Nebula)) ? strGetString(strGas) : "");
            y -= fontHeight(" ") + smTOLineSpacing;
        }
    }
    //print types and numbers of ships
    if (thisBlob->flags & (BTF_Explored | BTF_ProbeDroid))
    {
        if (sensorLevel == 2 || bitTest(thisBlob->flags, BTF_ProbeDroid))
        {                                                   //level 3 explored
            if (thisBlob->nNonCombat > 0)
            {
                fontPrintf(x, y, smTOColor, "%d %s", thisBlob->nNonCombat, strGetString(strNonCombatShips));
                y -= fontHeight(" ") + smTOLineSpacing;
            }
            if (thisBlob->nAttackShips > 0)
            {
                fontPrintf(x, y, smTOColor, "%d %s", thisBlob->nAttackShips, strGetString(strStrikeCraft));
                y -= fontHeight(" ") + smTOLineSpacing;
            }
            if (thisBlob->nCapitalShips > 0)
            {
                fontPrintf(x, y, smTOColor, "%d %s", thisBlob->nCapitalShips, strGetString(strCapitalShips));
                y -= fontHeight(" ") + smTOLineSpacing;
            }
        }
        else if (sensorLevel == 0)
        {                                                   //level 0 explored
            xSpacing = thisBlob->nCapitalShips + thisBlob->nAttackShips + thisBlob->nNonCombat;
            if (xSpacing > 0)
            {
                fontPrintf(x, y, smTOColor, "%d %s", xSpacing, strGetString(strEnemyUnits));
                y -= fontHeight(" ") + smTOLineSpacing;
            }
        }
        else if (sensorLevel == 1)
        {                                                   //level 1 explored
            xSpacing = thisBlob->nCapitalShips + thisBlob->nAttackShips + thisBlob->nNonCombat;
            if (xSpacing > 0)
            {
                fontPrint(x, y, smTOColor, strGetString(strEnemyUnits));
                y -= fontHeight(" ") + smTOLineSpacing;
            }
        }
    }
    else if (sensorLevel >= 1)
    {                                                       //level 0 unexplored
        xSpacing = thisBlob->nCapitalShips + thisBlob->nAttackShips + thisBlob->nNonCombat;
        if (xSpacing > 0)
        {
            fontPrint(x, y, smTOColor, strGetString(strEnemyUnits));
            y -= fontHeight(" ") + smTOLineSpacing;
        }
    }
    //if nothing was printed, just say it's unexplored
    if (y == yStart)
    {
        fontPrint(x, y, smTOColor, strGetString(strUnexplored));
    }
}
*/

/*-----------------------------------------------------------------------------
    Name        : smHotkeyGroupsDraw
    Description : Draw the location of hotkey groups.
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void smHotkeyGroupsDraw(void)
{
    sdword index;
    real32 averageSize, x, y, radius;
    vector centre;
    fonthandle currentFont = fontCurrentGet();
    MaxSelection selection;

    fontMakeCurrent(selGroupFont3);
    for (index = 0; index < 10; index++)
    {
        MakeShipsNotHidden((SelectCommand *)&selection, (SelectCommand *)&selHotKeyGroup[index]);
        if (selection.numShips > 0)
        {
            centre = selCentrePointComputeGeneral(&selection, &averageSize);
            selCircleComputeGeneral(&rndCameraMatrix, &rndProjectionMatrix, &centre, 1.0f, &x, &y, &radius);
            if (radius > 0.0f)
            {
                fontPrint(primGLToScreenX(x) + smHotKeyOffsetX,
                           primGLToScreenY(y) + smHotKeyOffsetY,
                           selHotKeyNumberColor, selHotKeyString[index]);
            }
        }
    }
    fontMakeCurrent(currentFont);
}

/*-----------------------------------------------------------------------------
    Name        : smPan
    Description : Special-function button that handles both clicks and pan operations.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void smPan(char *name, featom *atom)
{
    if (uicDragX == 0 && uicDragY == 0)
    {                                                       //if it's just a click
        smCameraLookatPoint.x = smCameraLookatPoint.y = smCameraLookatPoint.z = 0.0f;//recentre the camera
    }
    else
    {                                                       //else it's a drag operation
        vector addVector, lookVector;
        real32 mult, temp;

        vecSub(lookVector, smCamera.lookatpoint, smCamera.eyeposition);
        addVector.x = lookVector.x;
        addVector.y = lookVector.y;
        addVector.z = 0.0f;
        vecNormalize(&addVector);
        mult = smCamera.distance * smPanSpeedMultiplier;
        vecScalarMultiply(addVector, addVector, mult);      //into/out of screen vector
        vecScalarMultiply(lookVector, addVector, (real32)uicDragY);
        vecSubFrom(smCameraLookatPoint, lookVector);          //pan into/out of screen

        temp = addVector.x;                                 //negate-swap x,y
        addVector.x = addVector.y;
        addVector.y = -temp;                                //left/right vector

        vecScalarMultiply(lookVector, addVector, (real32)uicDragX);
        vecAddTo(smCameraLookatPoint, lookVector);          //pan left/right

        lookVector.x = lookVector.y = lookVector.z = 0.0f;
        // CAT (M2-R5 hud): the box clip (0x004e76b0) has no extents; smPan 0x0050bd.. clips the look-at point from
        //      the zero vector with it (the HW1 pan extent multiplier is gone)
        pieMovePointClipToLimits(&lookVector, &smCameraLookatPoint);
    }
//    smCamera.lookatpoint = smCameraLookatPoint;
}

/*=============================================================================
    CAT (M2-R6 managers, sensors): Cataclysm's sensors scene. The exe has no blob render (smBlobsDraw,
    smBlobDrawClear, smBlobDrawCloudy and their helpers are absent); cat_smViewportRenderScene 0x0050b4b0 draws, in
    the sensors camera: the background and the worlds, the player's sensor-range circles, the universe bounds, the
    decorative asteroids, the mines, the crates, the resources, the ships (points, meshes for the SM_Mesh types,
    class icons, slipgate paths), the compass (world plane, horizon ticks), the pings and the hot-key numbers. The
    fog of war hides what the player's grid has not seen: cat_selPositionHiddenByFOW (Select.c) is the exe's inlined
    test; the local player's grid (FOW.c, M2-R6 nis-fow-camera) answers it, and fowRender draws its boundary.
=============================================================================*/
// the exe's GL -> screen conversion rounds to the nearest pixel (the 1.5 * 2^52 constant 0x0072a458), as PiePlate.c
#define catSmScreenX(x)         (MAIN_WindowWidth / 2 + (sdword)lrint((double)((x) * (real32)MAIN_WindowWidth * 0.5f)))
#define catSmScreenY(y)         (MAIN_WindowHeight / 2 - (sdword)lrint((double)((y) * (real32)MAIN_WindowHeight * 0.5f)))
#define catSmScreenScaleX(r)    ((sdword)lrint((double)((r) * (real32)MAIN_WindowWidth * 0.5f)))

// the scene's colours (.data 0x00893048 .. 0x0089307c; names chosen here from their users)
static color catSmOwnResourceColor  = 0xff00ff00;      // 0x00893048 the player's resource-class ships
static color catSmOwnColor          = 0xff00ff00;      // 0x0089304c the player's ships (point and icon)
static color catSmAlliedColor       = 0xffffc000;      // 0x00893050 allies (and the player's exempt ships)
static color catSmEnemyColor        = 0xff0000ff;      // 0x00893054
static color catSmDerelictColor     = 0xff787878;      // 0x00893058 a KAS derelict (CAT_SHIPFLAG_KasDerelict)
static color catSmCrystalColor      = 0xffff00ff;      // 0x0089305c
static color catSmCrateColor        = 0xff00ffff;      // 0x00893060
static color catSmAsteroidDustColor = 0xff788ea2;      // 0x0089306c the decorative minor objects
static color catSmMeteorColor       = 0xff9696c8;      // 0x00893070
static color catSmAsteroidColor     = 0xff5a78b4;      // 0x00893074 (also a mimic posing as a resource)
static color catSmDustColor         = 0xff6e8cc8;      // 0x00893078
static color catSmMineColor         = 0xff2070ff;      // 0x0089307c
static color catSmRangeFarColor     = 0xff800000;      // 0x00893080 the farthest sensor-range circle
static color catSmRangeNearColor    = 0xff400000;      // 0x00893084 the nearest
static real32 catSmRangeOverlap     = 0.5f;            // 0x00893098 a circle inside a bigger one of the same blob
static color catSmUniverseBoundsColor = 0xffa26b40;    // 0x00892c90
// the zoom's progress 0..1 (0x00892b74, data 1.0; smZoomUpdate stores it): the range circles fade in by it
real32 catSmZoomFactor = 1.0f;
// HWI: the sensors render self-test's counts (InfoOverlay.c catHudSelfTestTick), not in the exe
CatSmDrawCounts catSmDrawCounts;

extern bool32 rndTextureEnabled;                    // render.c (0x00a75d74)
extern real32 toMinimumSize;                        // Tactical.c (0x00bb35e8)
extern bool8 g_WireframeHack;                       // Mesh.c (0x00a4c456)

#define CAT_SM_SLIPGATE_SEGMENTS    20              // 0x0072b0d0
#define CAT_SM_BIG_POINT_RADIUS     45.0f           // 0x0072b0e4: a collision sphere above draws a 2-pixel point
#define CAT_TYPEFLAG_Slipgate       0x00004000      // StaticInfo.catTypeFlags (as Tactical.c)

// the point size of a resource, mine or crate: 2 pixels above the collision sphere radius 45 (inlined by the exe)
static void catSmBigPoint(SpaceObj *obj, color c)
{
    glPointSize((obj->staticinfo->staticCollInfo.collspheresize > CAT_SM_BIG_POINT_RADIUS) ? 2.0f : 1.0f);
    primPoint3(&obj->posinfo.position, c);
    glPointSize(1.0f);
}

/*-----------------------------------------------------------------------------
    Name        : cat_smShipBlipType
    Description : CAT: new (0x0050a280): the class icon a ship gets in the sensors manager (an index into
                  toClassIcon): 0 a mothership-class ship, 2 the carriers, 1 the heavy cruisers and the dreadnought,
                  8 the three trader structures, 7 the processors, 3 the RKO and the destroyer, else -1 (none)
----------------------------------------------------------------------------*/
sdword cat_smShipBlipType(Ship *ship)
{
    if (ship->staticinfo->shipclass == CLASS_Mothership)
    {
        return 0;
    }
    switch (ship->shiptype)
    {
        case sCarrier: case bCarrier: case tCarrier: case kCarrier:
            return 2;
        case bHeavyCruiser: case kHeavyCruiser: case tHeavyCruiser: case sDreadnought:
            return 1;
        case trFloatingCity: case trMiningBase: case trResearchStation:
            return 8;
        case sProcessor: case bProcessor:
            return 7;
        case dRKO: case sDestroyer:
            return 3;
        default:
            return -1;
    }
}

// the sensor-range circle list's entry (12 bytes in the exe)
typedef struct
{
    sdword shown;
    Ship *ship;
    real32 distance;
} CatSmRangeEntry;

// farthest first (0x00509bc0 never returns 0)
static int catSmRangeCompare(const void *p0, const void *p1)
{
    return (((CatSmRangeEntry *)p0)->distance > ((CatSmRangeEntry *)p1)->distance) ? -1 : 1;
}

/*-----------------------------------------------------------------------------
    Name        : cat_smSensorRangeBlobsDraw
    Description : CAT: new (0x00509be0): with the sensors-sphere option, a screen-space circle of each of the
                  player's visible ships' fog-of-war radius (not a clamped component), but for a circle that lies
                  mostly inside a bigger one of the same collision blob; far circles first, shaded from the near
                  colour to the far one, faded in with the zoom; fuzzy (primCircleBorder) by smFuzzyBlobs
----------------------------------------------------------------------------*/
void cat_smSensorRangeBlobsDraw(hmatrix *modelView, hmatrix *projection)
{
    CatSmRangeEntry *list = NULL;
    Node *node;
    Ship *ship;
    sdword count, i, j, pass, x, y, radius, border, nSlices;
    real32 nearest = 1e21f, farthest = 0.0f;           // 0x6258d727
    real32 ri, dx, dy, dz, d, destX, destY, destRadius, t;
    bool32 stipple = FALSE;
    color c;

    if (!catSmViewFOWRanges)
    {
        return;
    }
    count = 0;
    for (pass = 0; pass < 2; pass++)
    {
        count = 0;
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            ship = (Ship *)listGetStructOfNode(node);
            if ((ship->staticinfo->shipclass == CLASS_Component) && (ship->guidance.clampInfo != NULL) &&
                (ship->guidance.clampInfo->host != NULL))
            {
                continue;
            }
            if (bitTest(ship->header.flags, SOF_Hide) || (ship->playerowner != universe.curPlayerPtr))
            {
                continue;
            }
            if (pass == 1)
            {
                list[count].ship = ship;
                list[count].shown = 1;
            }
            count++;
        }
        if (count == 0)
        {
            break;
        }
        if (pass == 0)
        {
            list = memAlloc(count * sizeof(CatSmRangeEntry), "smRangeBlobs", 0);
        }
    }
    for (i = 0; i < count; i++)
    {
        for (j = 0; j < count; j++)
        {
            if (!list[i].shown)
            {
                break;
            }
            if ((i == j) || (list[j].shown != 1) || (list[i].ship->obj.collMyBlob != list[j].ship->obj.collMyBlob))
            {
                continue;
            }
            if (cat_shipGetFOWRadius(list[i].ship, 0) < cat_shipGetFOWRadius(list[j].ship, 0))
            {
                // the exe compares against the smaller radius twice (its own radius, read again)
                ri = cat_shipGetFOWRadius(list[i].ship, 0);
                dx = list[i].ship->obj.posinfo.position.x - list[j].ship->obj.posinfo.position.x;
                dy = list[i].ship->obj.posinfo.position.y - list[j].ship->obj.posinfo.position.y;
                dz = list[i].ship->obj.posinfo.position.z - list[j].ship->obj.posinfo.position.z;
                if (dx * dx + dy * dy + dz * dz < catSmRangeOverlap * cat_shipGetFOWRadius(list[i].ship, 0) * ri)
                {
                    list[i].shown = 0;
                }
            }
        }
    }
    for (i = 0; i < count; i++)
    {
        if (list[i].shown)
        {
            dx = smCamera.eyeposition.x - list[i].ship->obj.posinfo.position.x;
            dy = smCamera.eyeposition.y - list[i].ship->obj.posinfo.position.y;
            dz = smCamera.eyeposition.z - list[i].ship->obj.posinfo.position.z;
            d = (real32)sqrt((double)(dx * dx + dy * dy + dz * dz));
            list[i].distance = d;
            if (d > farthest)
            {
                farthest = d;
            }
            if (d < nearest)
            {
                nearest = d;
            }
        }
    }
    if (list != NULL)
    {
        qsort(list, count, sizeof(CatSmRangeEntry), catSmRangeCompare);
    }
    primModeSet2();
    if (glIsEnabled(GL_POLYGON_STIPPLE))
    {
        stipple = TRUE;
        glDisable(GL_POLYGON_STIPPLE);
    }
    for (i = 0; i < count; i++)
    {
        if (!list[i].shown)
        {
            continue;
        }
        selCircleComputeGeneral(modelView, projection, &list[i].ship->obj.posinfo.position,
                                cat_shipGetFOWRadius(list[i].ship, 0), &destX, &destY, &destRadius);
        if (destRadius <= 0.0f)
        {
            continue;
        }
        x = catSmScreenX(destX);
        y = catSmScreenY(destY);
        radius = catSmScreenScaleX(destRadius);
        border = catSmScreenScaleX(smCircleBorder * destRadius);
        nSlices = border * 30 / MAIN_WindowHeight + 8;
        if (nSlices > 29)
        {
            nSlices = 30;
        }
        t = (list[i].distance - nearest) / (farthest - nearest);
        if (!(t >= 0.0f))
        {
            t = 0.0f;                                   // also the 0 / 0 of a single circle (NaN), as the exe
        }
        else if (t > 1.0f)
        {
            t = 1.0f;
        }
        c = colBlend(catSmRangeNearColor, catSmRangeFarColor, t);
        c = colBlend(c, smIntBackgroundColor, catSmZoomFactor);
        if (!smFuzzyBlobs)
        {
            primCircleSolid2(x, y, border, nSlices, c);
        }
        else
        {
            primCircleBorder(x, y, radius, border, nSlices, c);
        }
        catSmDrawCounts.rangeCircles++;
    }
    if (stipple)
    {
        glEnable(GL_POLYGON_STIPPLE);
    }
    primModeClear2();
    cat_rndDepthTestEnable(FALSE);
    rndLightingEnable(FALSE);
    rndTextureEnable(FALSE);
    if (list != NULL)
    {
        memFree(list);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_smShipMeshDraw
    Description : CAT: new (0x0050a0d0): a ship drawn as its mesh in the sensors manager (the level chosen by the
                  selection circle, a mimic's apparent mesh: cat_lodLevelGetMimic), lit and textured, with the
                  modules clamped to it, while the wireframe switch is off; FALSE when the level is no mesh
----------------------------------------------------------------------------*/
bool32 cat_smShipMeshDraw(Ship *ship)
{
    lod *level = cat_lodLevelGetMimic((SpaceObj *)ship, &smCamera.eyeposition, &ship->obj.posinfo.position);
    lod *moduleLevel;
    hmatrix coordMatrix;
    sdword lightOn, textureOn, i, scheme;
    CatClampedList *clamped;
    Ship *module;
    bool8 wireframe;

    if ((level->flags & LM_LODType) != LT_Mesh)
    {
        return FALSE;
    }
    scheme = ship->colorScheme;
    glPushMatrix();
    hmatMakeHMatFromMat(&coordMatrix, &ship->rotatable.rotinfo.coordsys);
    hmatPutVectIntoHMatrixCol4(ship->obj.posinfo.position, coordMatrix);
    glMultMatrixf((GLfloat *)&coordMatrix);
    lightOn = rndLightingEnable(TRUE);
    textureOn = rndTextureEnable(TRUE);
    shPushLightMatrix(&coordMatrix);
    if (rndShipVisible((SpaceObj *)ship, &smCamera))
    {
        wireframe = g_WireframeHack;
        g_WireframeHack = FALSE;
        if (ship->bindings != NULL)
        {
            meshRenderShipHierarchy(NULL, ship->bindings, ship->obj.currentLOD, (meshdata *)level->pData, scheme);
        }
        else
        {
            meshRender((meshdata *)level->pData, scheme);
        }
        clamped = (ship->guidance.clampInfo != NULL) ? ship->guidance.clampInfo->clampedObjs : NULL;
        if (clamped != NULL)
        {
            for (i = 0; i < clamped->numShips; i++)
            {
                module = clamped->ShipPtr[i];
                moduleLevel = cat_lodLevelGetMimic((SpaceObj *)module, &smCamera.eyeposition, &module->obj.posinfo.position);
                if (module->bindings != NULL)
                {
                    meshRenderShipHierarchy(NULL, module->bindings, module->obj.currentLOD, (meshdata *)moduleLevel->pData, scheme);
                }
                else
                {
                    meshRender((meshdata *)moduleLevel->pData, scheme);
                }
            }
        }
        g_WireframeHack = wireframe;
        catSmDrawCounts.meshes++;
    }
    shPopLightMatrix();
    rndLightingEnable(lightOn);
    rndTextureEnable(textureOn);
    glPopMatrix();
    return TRUE;
}

// a slipgate's path to its partner: a cubic between the two gates' forward axes, swaying with the real-time clock
//      (inlined by cat_smShipsDraw)
static void catSmSlipgatePathDraw(Ship *gate, Ship *partner, color c)
{
    vector start = gate->obj.posinfo.position, prev = start, point, velocity, local, startVelocity, endVelocity;
    real32 dx = gate->obj.posinfo.position.x - partner->obj.posinfo.position.x;
    real32 dy = gate->obj.posinfo.position.y - partner->obj.posinfo.position.y;
    real32 dz = gate->obj.posinfo.position.z - partner->obj.posinfo.position.z;
    real32 distance = (real32)sqrt((double)(dx * dx + dy * dy + dz * dz));
    real32 t;
    sdword index;

    local.x = 0.0f;
    local.y = 0.0f;
    local.z = ((real32)sin(0.05f * cat_taskRealTimeElapsed * PI) * 0.1f + 1.2f) * distance;
    matMultiplyMatByVec(&endVelocity, &partner->rotatable.rotinfo.coordsys, &local);
    local.z = -distance * ((real32)cos(0.05f * cat_taskRealTimeElapsed * PI) * 0.1f + 1.2f);
    matMultiplyMatByVec(&startVelocity, &gate->rotatable.rotinfo.coordsys, &local);
    for (index = 1; index < CAT_SM_SLIPGATE_SEGMENTS; index++)
    {
        t = (real32)index / (real32)(CAT_SM_SLIPGATE_SEGMENTS - 1);
        velocity = startVelocity;
        point = start;
        cat_ccEvalCubicVel(&point.x, &velocity.x, partner->obj.posinfo.position.x, endVelocity.x, t);
        cat_ccEvalCubicVel(&point.y, &velocity.y, partner->obj.posinfo.position.y, endVelocity.y, t);
        cat_ccEvalCubicVel(&point.z, &velocity.z, partner->obj.posinfo.position.z, endVelocity.z, t);
        primLine3(&prev, &point, c);
        prev = point;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_smShipsDraw
    Description : CAT: new (0x0050a310): every ship of the universe in the sensors manager: the selection circle
                  computed for a selectable/targetable (or untargetable) ship; hidden: SOF_Hide, components,
                  swarmers, an enemy the player may not select while it has a grid, a selected ship in the off half
                  of the flash; a slipgate draws its effect and its path to its partner instead; a crystal is always
                  drawn; a ship of an SM_Mesh type (a mimic's apparent type) as its mesh, else a 2-pixel point in its
                  relation's colour; with the icons option a ship with a class icon gets it in the blip colour
----------------------------------------------------------------------------*/
void cat_smShipsDraw(hmatrix *modelView, hmatrix *projection)
{
    Node *node;
    Ship *ship;
    SlipgateSpec *gate;
    Ship *partner;
    sdword blip, index;
    bool32 visible, needPoint, flashOn;
    color blipColor = 0, c;
    toicon *icon;
    real32 scale;
    ShipType apparent;

    flashOn = (fmod((double)cat_taskRealTimeElapsed, (double)smSelectedFlashSpeed) > smSelectedFlashSpeed * 0.5);
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        visible = TRUE;
        if (bitTest(ship->header.flags, SOF_Selectable | SOF_Targetable) || cat_shipIsUntargetable(ship))
        {
            selCircleCompute(modelView, projection, (SpaceObjRotImpTarg *)ship);
        }
        if (bitTest(ship->header.flags, SOF_Hide))
        {
            visible = FALSE;
        }
        if (!allianceArePlayersAllied(ship->playerowner, universe.curPlayerPtr))
        {
            if ((universe.curPlayerPtr->catFOWGrid != NULL) && (universe.curPlayerPtr->playerState != PLAYER_DEAD) &&
                !cat_selShipSelectableInFOW(ship))
            {
                visible = FALSE;
            }
            blip = cat_smShipBlipType(ship);
            if (blip != -1)
            {
                blipColor = catSmEnemyColor;
            }
        }
        else
        {
            blip = cat_smShipBlipType(ship);
            if (blip != -1)
            {
                blipColor = ((ship->playerowner == universe.curPlayerPtr) &&
                             !bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20)) ? catSmOwnColor : catSmAlliedColor;
            }
        }
        if (ship->staticinfo->shipclass == CLASS_Component)
        {
            visible = FALSE;
        }
        if ((ship->shiptype == sSwarmer) || (ship->shiptype == bSwarmer))
        {
            visible = FALSE;
        }
        if (ShipInSelection((SelectCommand *)&selSelected, ship) && !flashOn)
        {
            visible = FALSE;
        }
        if (ship->staticinfo->catTypeFlags & CAT_TYPEFLAG_Slipgate)
        {
            gate = (SlipgateSpec *)ship->ShipSpecifics;
            if ((gate->effect != NULL) && !cat_selPositionHiddenByFOW(&ship->obj.posinfo.position, TRUE))
            {
                etgEffectUpdate(gate->effect, 0.0f);
                etgEffectDraw(gate->effect);                // CAT: the exe passes 1 (draw as well as update)
            }
            cat_rndDepthTestEnable(FALSE);
            rndTextureEnabled = TRUE;                   // CAT: the exe forces the disable (0x00a75d74 = 1)
            rndTextureEnable(FALSE);
            partner = gate->partner;
            // each pair once: from the gate at the higher address (the exe compares the pointers)
            if (catSmViewSlipgatePaths && (partner != NULL) && ((smemsize)ship > (smemsize)partner) &&
                !cat_selPositionHiddenByFOW(&partner->obj.posinfo.position, TRUE))
            {
                // the per-player "seen" bytes at Ship+0x654 (SlipgateSpec.unknown1C / unknown20)
                if (((ubyte *)&gate->unknown1C)[universe.curPlayerIndex] ||
                    ((ubyte *)&((SlipgateSpec *)partner->ShipSpecifics)->unknown1C)[universe.curPlayerIndex])
                {
                    catSmSlipgatePathDraw(ship, partner, gate->color);
                    catSmDrawCounts.slipgatePaths++;
                }
            }
            visible = FALSE;
        }
        if (cat_shipIsCrystal(ship))
        {
            visible = TRUE;
        }
        else if (!visible)
        {
            continue;
        }
        needPoint = TRUE;
        if (bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic) && cat_shipIsMimicking(ship) &&
            ((sdword)cat_mimicGetApparentShipType(ship) >= 0) && (ship->playerowner != universe.curPlayerPtr))
        {
            apparent = cat_mimicGetApparentShipType(ship);
            if (bitTest(smShipTypeRenderFlags[apparent], SM_Mesh) && catSmViewShipMeshes && cat_smShipMeshDraw(ship))
            {
                needPoint = FALSE;
            }
            cat_rndDepthTestEnable(FALSE);
        }
        else if (bitTest(smShipTypeRenderFlags[ship->shiptype], SM_Mesh) && catSmViewShipMeshes)
        {
            if (cat_smShipMeshDraw(ship))
            {
                needPoint = FALSE;
            }
            cat_rndDepthTestEnable(FALSE);
        }
        if (needPoint)
        {
            if (ship->playerowner == universe.curPlayerPtr)
            {
                if (bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20))
                {
                    c = catSmAlliedColor;
                }
                else
                {
                    c = (ship->staticinfo->shipclass == CLASS_Resource) ? catSmOwnResourceColor : catSmOwnColor;
                }
            }
            else if (allianceArePlayersAllied(ship->playerowner, universe.curPlayerPtr) ||
                     (bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic) && cat_mimicAppearsAlliedTo(ship, universe.curPlayerPtr)))
            {
                c = catSmAlliedColor;
            }
            else if (cat_shipIsCrystal(ship))
            {
                c = catSmCrystalColor;
            }
            else if (bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic) && cat_mimicAppearsAsResource(ship, universe.curPlayerPtr))
            {
                if (!singlePlayerGame)
                {
                    continue;                           // a mimic posing as a resource: nothing in a multiplayer game
                }
                c = catSmAsteroidColor;
            }
            else if (bitTest(ship->header.catFlags18, CAT_SHIPFLAG_KasDerelict))
            {
                c = catSmDerelictColor;
            }
            else
            {
                c = catSmEnemyColor;
            }
            glPointSize(2.0f);
            primPoint3(&ship->obj.posinfo.position, c);
            glPointSize(1.0f);
            catSmDrawCounts.shipPoints++;
        }
        if (catSmViewIcons && (blip != -1) && (ship->impactable.collInfo.selCircleDepth > 0.0f))
        {
            icon = toClassIcon[blip];
            scale = ship->staticinfo->tacticalSelectionScale * ship->impactable.collInfo.selCircleRadius;
            if (!(scale > toMinimumSize))
            {
                scale = toMinimumSize;
            }
            primModeSet2();
            primLineLoopStart2(1, blipColor);
            for (index = icon->nPoints - 1; index >= 0; index--)
            {
                primLineLoopPoint3F(scale * icon->loc[index].x + ship->impactable.collInfo.selCircleX,
                                    scale * icon->loc[index].y + ship->impactable.collInfo.selCircleY);
            }
            primLineLoopEnd2();
            primModeClear2();
            glDisable(GL_DEPTH_TEST);
            rndLightingEnable(FALSE);
            rndTextureEnable(FALSE);
            catSmDrawCounts.icons++;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_smShipHotkeyNumbersDraw
    Description : CAT: new (0x0050ab50): the hot-key group number next to each selected ship of a group, in the
                  group font 3, skipping a ship whose label would sit within five label widths (both axes) of one
                  already printed for the same group
----------------------------------------------------------------------------*/
void cat_smShipHotkeyNumbersDraw(void)
{
    static sdword labelX[COMMAND_MAX_SHIPS], labelY[COMMAND_MAX_SHIPS];
    sdword group, index, j, count, x, y, dx, dy, width;
    Ship *ship;
    bool32 free;

    fontMakeCurrent(selGroupFont3);                     // the exe leaves it current
    for (group = 0; group < SEL_NumberHotKeyGroups; group++)
    {
        count = 0;
        for (index = 0; index < selHotKeyGroup[group].numShips; index++)
        {
            ship = selHotKeyGroup[group].ShipPtr[index];
            if (!ShipInSelection((SelectCommand *)&selSelected, ship))
            {
                continue;
            }
            x = catSmScreenX(ship->impactable.collInfo.selCircleX);
            y = catSmScreenY(ship->impactable.collInfo.selCircleY);
            free = TRUE;
            for (j = 0; (j < count) && free; j++)
            {
                width = fontWidth(selHotKeyString[group]) * 5;
                dx = labelX[j] - x;
                if (dx < 0)
                {
                    dx = -dx;
                }
                if (dx < width)
                {
                    dy = labelY[j] - y;
                    if (dy < 0)
                    {
                        dy = -dy;
                    }
                    if (dy < width)
                    {
                        free = FALSE;
                    }
                }
            }
            if (!free || (count >= COMMAND_MAX_SHIPS))
            {
                continue;
            }
            labelX[count] = x;
            labelY[count] = y;
            fontPrint(x, y - fontHeight(" "), selHotKeyNumberColor, selHotKeyString[group]);
            count++;
            catSmDrawCounts.hotkeyNumbers++;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_smResourcesDraw
    Description : CAT: new (0x0050ae00): the resources as points: meteors (the killer-collision attribute) with the
                  meteors option, asteroids (not one a mimic poses as) and dust clouds with the resources option,
                  where the grid has seen them
----------------------------------------------------------------------------*/
void cat_smResourcesDraw(hmatrix *modelView, hmatrix *projection)
{
    Node *node;
    SpaceObjRotImpTarg *resource;
    bool32 meteor;

    for (node = universe.ResourceList.head; node != NULL; node = node->next)
    {
        resource = (SpaceObjRotImpTarg *)listGetStructOfNode(node);
        meteor = FALSE;
        if (bitTest(resource->header.flags, SOF_Selectable | SOF_Targetable))
        {
            selCircleCompute(modelView, projection, resource);
        }
        if (catSmViewMeteors && bitTest(resource->obj.attributes, CAT_ATTRIBUTES_KillerCollision))
        {
            if (!cat_selPositionHiddenByFOW(&resource->obj.posinfo.position, TRUE))
            {
                catSmBigPoint((SpaceObj *)resource, catSmMeteorColor);
                catSmDrawCounts.resources++;
            }
            meteor = TRUE;
        }
        if (catSmViewResources && !meteor)
        {
            if (resource->header.objtype == OBJ_AsteroidType)
            {
                if ((((Asteroid *)resource)->catMimicShip == NULL) &&
                    !cat_selPositionHiddenByFOW(&resource->obj.posinfo.position, TRUE))
                {
                    catSmBigPoint((SpaceObj *)resource, catSmAsteroidColor);
                    catSmDrawCounts.resources++;
                }
            }
            else if (resource->header.objtype == OBJ_DustType)
            {
                if (!cat_selPositionHiddenByFOW(&resource->obj.posinfo.position, TRUE))
                {
                    catSmBigPoint((SpaceObj *)resource, catSmDustColor);
                    catSmDrawCounts.resources++;
                }
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_smBoundsEdgeDraw / cat_smUniverseBoundsDraw
    Description : CAT: new (0x0050b090 / 0x0050b150): with the universe-extents option, the twelve edges of the
                  universe box as corner ticks (5% of each edge from both ends, fading to black), in the bounds
                  colour faded with the zoom
----------------------------------------------------------------------------*/
static void cat_smBoundsEdgeDraw(vector *p0, vector *p1, color c)
{
    vector d, a, b;

    d.x = (p1->x - p0->x) * 0.05f;                      // 0x0072b0cc
    d.y = 0.05f * (p1->y - p0->y);
    d.z = 0.05f * (p1->z - p0->z);
    a.x = d.x + p0->x;
    a.y = d.y + p0->y;
    a.z = d.z + p0->z;
    b.x = p1->x - d.x;
    b.y = p1->y - d.y;
    b.z = p1->z - d.z;
    cat_primLine3Smooth(p0, &a, c, 0xff000000);
    cat_primLine3Smooth(p1, &b, c, 0xff000000);
}

void cat_smUniverseBoundsDraw(void)
{
    vector p000, p001, p010, p011, p100, p101, p110, p111;
    color c;

    if (!catSmViewUniverseExtents)
    {
        return;
    }
    c = cat_smZoomFadeColor(catSmUniverseBoundsColor, 0);  // inlined in the exe
    if (c == 0xff000000)
    {
        return;
    }
    p000.x = -smUniverseSizeX; p000.y = -smUniverseSizeY; p000.z = -smUniverseSizeZ;
    p001 = p000; p001.z = smUniverseSizeZ;
    p010 = p000; p010.y = smUniverseSizeY;
    p011 = p001; p011.y = smUniverseSizeY;
    p100 = p000; p100.x = smUniverseSizeX;
    p101 = p001; p101.x = smUniverseSizeX;
    p110 = p010; p110.x = smUniverseSizeX;
    p111.x = smUniverseSizeX; p111.y = smUniverseSizeY; p111.z = smUniverseSizeZ;
    cat_smBoundsEdgeDraw(&p000, &p001, c);
    cat_smBoundsEdgeDraw(&p000, &p010, c);
    cat_smBoundsEdgeDraw(&p000, &p100, c);
    cat_smBoundsEdgeDraw(&p011, &p001, c);
    cat_smBoundsEdgeDraw(&p011, &p010, c);
    cat_smBoundsEdgeDraw(&p011, &p111, c);
    cat_smBoundsEdgeDraw(&p101, &p001, c);
    cat_smBoundsEdgeDraw(&p101, &p100, c);
    cat_smBoundsEdgeDraw(&p101, &p111, c);
    cat_smBoundsEdgeDraw(&p110, &p010, c);
    cat_smBoundsEdgeDraw(&p110, &p100, c);
    cat_smBoundsEdgeDraw(&p110, &p111, c);
    catSmDrawCounts.boundsFrames++;
}

/*-----------------------------------------------------------------------------
    Name        : cat_smMinesDraw
    Description : CAT: new (0x0050b380): the mines as points where the grid sees them (live usage), but in single
                  player mission 3 only once the mission shows them (catSmShowMines)
----------------------------------------------------------------------------*/
void cat_smMinesDraw(hmatrix *modelView, hmatrix *projection)
{
    Node *node;
    Missile *missile;

    for (node = universe.MissileList.head; node != NULL; node = node->next)
    {
        missile = (Missile *)listGetStructOfNode(node);
        if (missile->missileType != MISSILE_Mine)
        {
            continue;
        }
        if (bitTest(missile->header.flags, SOF_Selectable | SOF_Targetable))
        {
            selCircleCompute(modelView, projection, (SpaceObjRotImpTarg *)missile);
        }
        if (cat_selPositionHiddenByFOW(&missile->obj.posinfo.position, FALSE))
        {
            continue;
        }
        if (singlePlayerGame && (singlePlayerGameInfo.currentMission == 3) && !catSmShowMines)
        {
            continue;
        }
        catSmBigPoint((SpaceObj *)missile, catSmMineColor);
        catSmDrawCounts.mines++;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_smViewportRenderScene
    Description : CAT: new (0x0050b4b0): the sensors manager's scene in the current matrices (see the section
                  comment above); the depth test is off while it draws and restored after
----------------------------------------------------------------------------*/
void cat_smViewportRenderScene(hmatrix *modelView, hmatrix *projection)
{
    bool32 depth = cat_rndDepthTestEnable(FALSE);
    Node *node;
    Derelict *derelict;
    sdword index;
    udword crates;
    vector centre;
    fonthandle oldFont;

    catSmDrawCounts.scenes++;
    if (catSmViewBackground)
    {
        glPushMatrix();
        glTranslatef(smCamera.eyeposition.x, smCamera.eyeposition.y, smCamera.eyeposition.z);
        rndBackgroundRender(BG_RADIUS, &smCamera, FALSE);
        glPopMatrix();
        lightSetLighting();
        rndLightingEnable(TRUE);
        for (index = 0; index < UNIV_NUMBER_WORLDS; index++)
        {
            if (universe.world[index] == NULL)
            {
                break;
            }
            if (universe.world[index]->header.objtype == OBJ_DerelictType)
            {
                rndRenderAHomeworld(&smCamera, universe.world[index]);
            }
        }
    }
    cat_smSensorRangeBlobsDraw(modelView, projection);
    primModeClear2();
    lightPositionSet();
    rndLightingEnable(FALSE);
    rndTextureEnable(FALSE);
    cat_smUniverseBoundsDraw();
    if (universe.curPlayerPtr->catFOWGrid != NULL)
    {                                                       // CAT: the explored cells' boundary (fowRender 0x0047f810, M2-R6 nis-fow-camera)
        fowSelect(universe.curPlayerPtr->catFOWGrid);
        fowRender();
    }
    if (catSmViewDecorativeAsteroids)
    {
        for (node = universe.MinorSpaceObjList.head; node != NULL; node = node->next)
        {
            primPoint3(&((SpaceObj *)listGetStructOfNode(node))->posinfo.position, catSmAsteroidDustColor);
        }
    }
    cat_smMinesDraw(modelView, projection);
    if (universe.numCratesInWorld != 0)
    {
        crates = 0;
        for (node = universe.DerelictList.head; (node != NULL) && (crates < universe.numCratesInWorld); node = node->next)
        {
            derelict = (Derelict *)listGetStructOfNode(node);
            if (derelict->derelicttype != Crate)
            {
                continue;
            }
            crates++;
            if (!cat_selPositionHiddenByFOW(&derelict->obj.posinfo.position, FALSE))
            {
                catSmBigPoint((SpaceObj *)derelict, catSmCrateColor);
            }
        }
    }
    cat_smResourcesDraw(modelView, projection);
    cat_smShipsDraw(modelView, projection);
    if (catSmViewCompass)
    {
        centre.x = smCentreWorldPlane ? 0.0f : smCamera.lookatpoint.x;
        centre.y = smCentreWorldPlane ? 0.0f : smCamera.lookatpoint.y;
        centre.z = 0.0f;
        smWorldPlaneDraw(&centre, TRUE, smCurrentWorldPlaneColor);
        smHorizonLineDraw(&smCamera, modelView, projection, smCurrentCameraDistance);
        primModeSet2();
        oldFont = fontCurrentGet();                     // smTickTextDraw, inlined
        fontMakeCurrent(mouseCursorFont);
        for (smTickTextIndex--; smTickTextIndex >= 0; smTickTextIndex--)
        {
            fontPrint(smTickText[smTickTextIndex].x, smTickText[smTickTextIndex].y,
                      smCurrentWorldPlaneColor, smTickText[smTickTextIndex].text);
        }
        fontMakeCurrent(oldFont);
        primModeClear2();
        catSmDrawCounts.compassFrames++;
    }
    primModeSet2();
    pingListDraw(&smCamera, modelView, projection, &smViewRectangle);
    cat_smShipHotkeyNumbersDraw();
    primModeClear2();
    cat_rndDepthTestEnable(depth);
}

/*-----------------------------------------------------------------------------
    Name        : smViewportRender
    Description : the sensors manager viewport's draw callback
    CAT (M2-R6 managers, sensors): the exe's body (0x0050be70): nothing while catSmHideViewport is set; the zoom in
        and out as HW1 (the ambient 0x101 at the end of the zoom out, the space ambient 0x100 at the focus
        transition), the pan by the bound keys and the overlay's buttons, the cubic pan; the camera matrices, then the
        scene (cat_smViewportRenderScene: no blobs, no cursor text, no player names), the movement pie, the hot-key
        groups with the tactical overlay, the NIS static, the tactical overlay, the band box (cat_mrSelectRectDraw),
        the main overlay, the ship info panel, the popup menu and the hover updates; the depth test off while it
        draws (the early returns of the zoom leave it off, as the exe)
----------------------------------------------------------------------------*/
void smViewportRender(featom *atom, regionhandle region)
{
    CameraStackEntry *curentry = currentCameraStackEntry(&universe.mainCameraCommand);
    sdword frameTicks;
    real32 frameStep;
    vector rememberVec, diffVec;
    bool32 depth;

    if (catSmHideViewport)
    {
        return;
    }
    depth = cat_rndDepthTestEnable(FALSE);
    frameTicks = utyNFrameTicks;
    if (frameTicks > smMaxFrameTicks)
        frameTicks = smMaxFrameTicks;

    frameStep = (real32)frameTicks / (real32)taskFrequency;
    cameraControl(&smCamera, FALSE);                         //update the camera
    curentry->remembercam.angle = smCamera.angle;
    curentry->remembercam.declination = smCamera.declination;

    if (smZoomingOut)
    {
        smZoomTime += frameStep;
        smZoomUpdate(smZoomTime, smCurrentZoomLength, TRUE);
        if (smZoomTime >= smCurrentZoomLength)
        {                                                   //if done zooming out
            soundEventPlay(NULL, 0x101, NULL);              // CAT: the sensors manager's ambient
            smZoomingOut = FALSE;                           //stop zooming
        }
        if (smZoomTime < smCurrentMainViewZoomLength)
        {                                                   //if still in the regular renderer part of the zoom
            cameraCopyPositionInfo(&universe.mainCameraCommand.actualcamera, &smCamera);
            universe.mainCameraCommand.actualcamera.ignoreZoom = TRUE;
            return;                                         //let mainrgn.c handle the rendering
        }
        mrRenderMainScreen = FALSE;
        universe.mainCameraCommand.actualcamera.ignoreZoom = FALSE;
    }
    else if (smZoomingIn)
    {
        smZoomTime -= frameStep;
        smZoomUpdate(smZoomTime, smCurrentZoomLength, TRUE);
        if (smZoomTime <= 0.0f)
        {                                                   //fully zoomed in
            smZoomingIn = FALSE;
            smSensorsCloseForGood();                        //stop zooming in already!
        }
        if (smZoomTime >= smCurrentMainViewZoomLength)
        {                                                   //if in sm part of zoom
            cameraCopyPositionInfo(&universe.mainCameraCommand.actualcamera, &smCamera);
            universe.mainCameraCommand.actualcamera.ignoreZoom = TRUE;
        }
        else if (!smFocusTransition)
        {                                                   //if at transition point
            smFocusTransition = TRUE;                       //make sure we only do this once a zoom
            if (smFocus)
            {
                universe.mainCameraCommand.actualcamera.ignoreZoom = FALSE;
                ccFocusFar(&universe.mainCameraCommand, smFocusCommand, &universe.mainCameraCommand.actualcamera);
                memFree(smFocusCommand);
                smFocusCommand = NULL;
            }
            mrRenderMainScreen = TRUE;
            soundEventPlay(NULL, UI_SoundOfSpace, NULL);    // CAT: 0x100
        }
        else
        {                                                   //else the transition has already been made
            if (!smFocus && smZoomTime > 0.0f)
            {                                               //if not at end of zoom in with no focus
                cameraCopyPositionInfo(&universe.mainCameraCommand.actualcamera, &smCamera);
                universe.mainCameraCommand.actualcamera.ignoreZoom = TRUE;
            }
            return;
        }
    }
    else
    {
        smCurrentWorldPlaneColor = smWorldPlaneColor;
        smCurrentCameraDistance = smCamera.clipPlaneFar;
    }
    if (piePointSpecMode != PSM_Idle)
    {
        smCurrentWorldPlaneColor = colMultiplyClamped(smCurrentWorldPlaneColor, smMovementWorldPlaneDim);
    }

    //perform some render-time monkey work of the camera position
    if (!smZoomingIn && !smZoomingOut && !cat_popupMenuIsOpen())
    {                                               //don't move the eye point if zooming (CAT: or with the popup menu)
        uicDragX = uicDragY = 0;
        // CAT (M2-R5 hud): the main overlay's four scroll buttons pan as the keys do (cat_moButton1..4Clicked
        //      0x004aaf70.., smViewportRender); the keys are the bound commands Pan_Sensors_Left .. Down 0x41..0x44
        //      (kbCommandKeyIsHit; M2-R6 startup-keys)
        if (kbCommandKeyIsHit(CAT_KB_PanSensorsLeft) || cat_moButton1Clicked())
        {                                                       //pan the camera with the cursor keys
            uicDragX -= smCursorPanX * frameTicks / (udword)taskFrequency;
        }
        if (kbCommandKeyIsHit(CAT_KB_PanSensorsRight) || cat_moButton2Clicked())
        {
            uicDragX += smCursorPanX * frameTicks / (udword)taskFrequency;
        }
        if (kbCommandKeyIsHit(CAT_KB_PanSensorsUp) || cat_moButton3Clicked())
        {
            uicDragY -= smCursorPanY * frameTicks / (udword)taskFrequency;
        }
        if (kbCommandKeyIsHit(CAT_KB_PanSensorsDown) || cat_moButton4Clicked())
        {
            uicDragY += smCursorPanY * frameTicks / (udword)taskFrequency;
        }
        if (uicDragX != 0 || uicDragY != 0)
        {
            smPan(NULL, NULL);
        }
        //perform cubic spline interpolation of the panned camera point
        vecSub(rememberVec, smCamera.lookatpoint, smCameraLookatPoint);
        if (ABS(rememberVec.x) + ABS(rememberVec.y) + ABS(rememberVec.z) > smPanEvalThreshold)
        {                                                   //if there is any distance between pan point and camera point
            while (frameTicks)
            {
                rememberVec = smCamera.lookatpoint;
                EvalCubic(&smCamera.lookatpoint.x, &smCameraLookVelocity.x, smCameraLookatPoint.x, smPanTrack);
                EvalCubic(&smCamera.lookatpoint.y, &smCameraLookVelocity.y, smCameraLookatPoint.y, smPanTrack);
                EvalCubic(&smCamera.lookatpoint.z, &smCameraLookVelocity.z, smCameraLookatPoint.z, smPanTrack);

                vecSub(diffVec, smCamera.lookatpoint, rememberVec);
                vecAddTo(smCamera.eyeposition, diffVec);    //pan the eyeposition with the lookatpoint
                frameTicks--;
            }
        }
    }
    //now on to the actual rendering code:
    cameraSetEyePosition(&smCamera);

    primModeClear2();
    rndLightingEnable(FALSE);
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    rgluPerspective(smCamera.fieldofview, rndAspectRatio,    //set projection matrix
                   smCamera.clipPlaneNear, smCamera.clipPlaneFar);
    glScalef(smProjectionScale, smProjectionScale, smProjectionScale);
    glMatrixMode( GL_MODELVIEW );

    glLoadIdentity();
#if RND_CAMERA_OFFSET
    rgluLookAt(smCamera.eyeposition.x + RND_CameraOffsetX, smCamera.eyeposition.y,
              smCamera.eyeposition.z, smCamera.lookatpoint.x + RND_CameraOffsetX,
              smCamera.lookatpoint.y, smCamera.lookatpoint.z,
              smCamera.upvector.x, smCamera.upvector.y, smCamera.upvector.z);
#else
    rgluLookAt(smCamera.eyeposition.x, smCamera.eyeposition.y,
              smCamera.eyeposition.z, smCamera.lookatpoint.x,
              smCamera.lookatpoint.y, smCamera.lookatpoint.z,
              smCamera.upvector.x, smCamera.upvector.y, smCamera.upvector.z);
#endif
    glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat *)(&rndCameraMatrix));
    glGetFloatv(GL_PROJECTION_MATRIX, (GLfloat *)(&rndProjectionMatrix));

    cat_smViewportRenderScene(&rndCameraMatrix, &rndProjectionMatrix);

    if (piePointSpecMode != PSM_Idle)
    {                                                       //draw point spec
        mrCamera = &smCamera;                               //point spec draw uses mrCamera
        piePointSpecDraw();                                 //draw the movement mechanism
    }

    primModeSet2();

    if (smTacticalOverlay)
    {
        smHotkeyGroupsDraw();
    }
    if (nisStatic[NIS_SMStaticIndex] != NULL)
    {
        nisStaticDraw(nisStatic[NIS_SMStaticIndex]);
    }
    if (mrDrawTactical)
    {
        rndTextureEnable(FALSE);
        glDisable(GL_TEXTURE_2D);
        toAllShipsDraw();
    }
    if (catMrSelecting)
    {
        cat_mrSelectRectDraw(1);                            // the band box, sensors flavour
    }
    cat_moRender();
    if (selSelected.numShips > 0 && selSelected.ShipPtr[0]->playerowner == universe.curPlayerPtr)
    {
        cat_shipPrintDraw(selSelected.ShipPtr[0]);
    }
    cat_popupMenuDraw();
    cat_ioUpdateHover();
    cat_shipPrintDragUpdate();
    cat_popupMenuUpdateHover(FALSE);
    cat_rndDepthTestEnable(depth);
}

/*-----------------------------------------------------------------------------
    Name        : smNULL
    Description : NULL logic handler for when the user's not doing anything with
                    the sensors manager.
    Inputs      : void
    Outputs     : void
    Return      : void
----------------------------------------------------------------------------*/
void smNULL(void)
{
    ;
}

/*-----------------------------------------------------------------------------
    Name        : mrSelectHold
    Description : Function for dragging a selection box
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void smSelectHold(void)
{
    if (ABS(mouseCursorX() - mrOldMouseX) >= selClickBoxWidth ||
        ABS(mouseCursorY() - mrOldMouseY) >= selClickBoxHeight)
    {                                                       //if mouse has moved from anchor point
        mrSelectRectBuild(&smSelectionRect, mrOldMouseX, mrOldMouseY);//create a selection rect
        selRectNone();
    }
    else
    {                                                       //else mouse hasn't moved far enough
        smSelectionRect.x0 = smSelectionRect.x1 =           //select nothing
            smSelectionRect.y0 = smSelectionRect.y1 = 0;
        selRectNone();
    }
}

/*-----------------------------------------------------------------------------
    Name        : smCullAndFocusSelecting
    Description : Culls the current selection list to a reasonable size and
                    focuses on it.
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void smCullAndFocusSelecting(void)
{
    sdword index, nToFocusOn;
    vector centre = {0.0f, 0.0f, 0.0f};

    // CAT (M2-R6 managers, sensors): smCullAndFocusSelecting 0x0050c580: the gate is bit 0x8000000 of the first word
    //      (as smSensorsClose), the exit sound 0x12e
    if ((tutorial == TUTORIAL_ONLY) && !(((udword *)&tutEnable)[0] & 0x8000000))
    {
        return;
    }

    nToFocusOn = ccFocusCullRadiusMean((FocusCommand *)&selSelecting, smFocusRadius * smFocusRadius, &centre);
    if (nToFocusOn == 0)
    {                                                       //nothing to focus on
        return;
    }
    //now we have a full listing of what we can focus on.  Let's focus on it.
    smFocus = TRUE;
    smFocusCommand = memAlloc(ccFocusSize(selSelecting.numTargets), "SensorsMultiFocus", 0);
    for (index = 0; index < selSelecting.numTargets; index++)
    {
        smFocusCommand->ShipPtr[index] = (Ship *)selSelecting.TargetPtr[index];
    }
    smFocusCommand->numShips = selSelecting.numTargets;
    dbgAssertOrIgnore(!smZoomingOut);
    smZoomingIn = TRUE;

    soundEventPlay(NULL, 0x12e, NULL);                      // CAT: the sensors manager's exit

    smFocusTransition = FALSE;
    smEyeEnd = smCamera.eyeposition;
    smLookEnd = smCamera.lookatpoint;
    smLookStart = centre;
    vecSub(smEyeStart, smLookEnd, smEyeEnd);
    vecNormalize(&smEyeStart);
    dbgAssertOrIgnore(smEyeStart.x < (real32)1e19 && smEyeStart.x > (real32)-1e19);
    vecMultiplyByScalar(smEyeStart, CAMERA_FOCUSFAR_DISTANCE);
    vecSub(smEyeStart, smLookStart, smEyeStart);
    smInitialDistance = smCamera.distance;//remember zoom-back distance for next time
    selRectNone();
    //now create a render list as it will be when fully zoomed in
    smTempCamera.lookatpoint = smLookStart;
    smTempCamera.eyeposition = smEyeStart;
    mrCamera = &smTempCamera;
    univUpdateRenderList();
    dbgAssertOrIgnore(smEyeStart.x < (real32)1e19 && smEyeStart.x > (real32)-1e19);

    //the player will get kicked into the sensors manager after several seconds waiting
    //with the camera nowhere near their ships.  This is a security "anti-spy" measure.
    //If when kicked into the SM the player focuses on their own ships explicitly, we
    //don't want them to focus on their mothership, rather on the ships they specified.
    smFocusOnMothershipOnClose = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : smViewportProcess
    Description : process messages for the main viewport of the sensors manager.
    Inputs      : standard region callback parameters
    Outputs     : ..
    Return      : ..
----------------------------------------------------------------------------*/
udword smViewportProcess(regionhandle region, sdword ID, udword event, udword data)
{
    // CAT (M2-R6 managers, sensors): the exe's region callback (0x0050c7e0, no Ghidra function) replaces HW1's
    //      own band box, focus and move handling: nothing while zooming; the wheel flags, the main region's mouse
    //      handlers in their sensors flavour (cat_mrHoldLeft / cat_mrPressLeft / cat_mrReleaseLeft / cat_mrPressRight /
    //      cat_mrReleaseRight / cat_mrReleaseLeftCB with 1), the camera turn while the right button turns it, the keys to
    //      the key bindings (sensors 1); the middle button (key id 4) focuses the selection outside the fleet intel
    if (smZoomingIn || smZoomingOut)
    {
        return 0;
    }
    switch (event)
    {
        case RPE_WheelDown:
            wheel_down = TRUE;
            break;
        case RPE_WheelUp:
            wheel_up = TRUE;
            break;
        case RPE_HoldLeft:
            cat_mrHoldLeft(1);
            break;
        case RPE_HoldRight:
            if (catMrRightMode == 2)
            {
                mrCameraMotion();
            }
            break;
        case RPE_PressLeft:
            cat_mrPressLeft();                              // the exe passes 1, which the function ignores
            break;
        case RPE_PressRight:
            cat_mrPressRight();
            break;
        case RPE_ReleaseLeft:
            cat_mrReleaseLeft(1);
            break;
        case RPE_ReleaseRight:
            cat_mrReleaseRight(1);
            break;
        case RPE_KeyDown:
            cat_kbKeyPress(1, ID);
            if ((catKeyVkFromKey(ID) == 0x04) && !smFleetIntel)
            {                                               // the middle button
                selSelectionCopy((MaxAnySelection *)&selSelecting, (MaxAnySelection *)&selSelected);
                smCullAndFocusSelecting();
            }
            break;
        case RPE_KeyUp:
            cat_kbKeyRelease(1, ID);
            break;
        case RPE_DoubleLeft:
            cat_mrReleaseLeftCB(1);
            break;
        default:
            break;
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : various
    Description : Processor callbacks for FE buttons
    Inputs      : standard FE callback parameters
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void smDispatch(char *name, featom *atom)
{
#if SM_VERBOSE_LEVEL >= 1
    dbgMessage("smDispatch");
#endif
    if (smHoldRight == smNULL)
    {                                                       //cannot bring up MM with RMB held down
        if (!smFleetIntel && ((!(tutorial==TUTORIAL_ONLY)) || tutEnable.bMove))
        {
            if(mrNeedProbeHack())
            {
                mrProbeHack();
            }
            else
            {
                mrRemoveAllProbesFromSelection();
                if(MP_HyperSpaceFlag)
                {
                    MP_HyperSpaceFlag = FALSE;
                }
                else
                {
                    if (piePointSpecMode == PSM_Idle)
                    {                                       //if bringing up the MM
                        makeShipsControllable((SelectCommand *)&selSelected,COMMAND_MP_HYPERSPACING);
                        makeShipsNotIncludeSinglePlayerMotherships((SelectCommand *)&selSelected);
                        if(selSelected.numShips > 0)
                        {                                   //if there are ships we can control
                            piePointModeOnOff(PIE_SPEC_Move, NULL);
                        }
                    }
                    else
                    {                                       //always allow the MM to be turned off
                        piePointModeOnOff(PIE_SPEC_None, NULL);   // CAT: off, mode 0 as the exe
                    }
                }
            }
        }
    }
    feToggleButtonSet(name, piePointSpecMode != PSM_Idle);
}

void smUpdateHyperspaceStatus(bool32 goForLaunch)
{
    mrUpdateHyperspaceStatus(goForLaunch);

    if (smHyperspaceRegion == NULL)
    {
        return;
    }

    if (goForLaunch)
    {
        bitClear(smHyperspaceRegion->status, RSF_RegionDisabled);
    }
    else
    {
        bitSet(smHyperspaceRegion->status, RSF_RegionDisabled);
    }
}

void smHyperspace(char *name, featom *atom)
{
    regionhandle region = (regionhandle)atom->region;

#if SM_VERBOSE_LEVEL >= 1
    dbgMessage("smHyperspace");
#endif

    if (FEFIRSTCALL(atom))
    {
        smHyperspaceRegion = region;

        if (singlePlayerGame)
        {
            if (singlePlayerGameInfo.playerCanHyperspace)
            {
                bitClear(region->status, RSF_RegionDisabled);
            }
            else
            {
                bitSet(region->status, RSF_RegionDisabled);
            }
        }
        else
        {
            //check research capabilities...
            //if not researched...black out button!

            if(!bitTest(tpGameCreated.flag,MG_Hyperspace))
            {
                bitSet(region->status, RSF_RegionDisabled);
            }
            //multiplayer Hyperspace initial Call!

        }
    }
    else if (FELASTCALL(atom))
    {
        smHyperspaceRegion = NULL;
        //check if movement plate for HYPERSPACE is up...
        //close it...
        if(!singlePlayerGame)
        {
            if(!bitTest(tpGameCreated.flag,MG_Hyperspace))
            {
                return;
            }
        }
        if (piePointSpecMode!=PSM_Idle
            && MP_HyperSpaceFlag)
        {
            soundEvent(NULL, UI_MovementGUIoff);
            MP_HyperSpaceFlag = FALSE;
            piePointModeOnOff(PIE_SPEC_None, NULL);   // CAT: off, mode 0 as the exe
        }
    }
    else
    {
        if(!singlePlayerGame)
        {
            if(!bitTest(tpGameCreated.flag,MG_Hyperspace))
            {
                return;
            }
        }
        if (singlePlayerGame && singlePlayerGameInfo.playerCanHyperspace)
        {
            spHyperspaceButtonPushed();
        }
        else
        {
            if (!smFleetIntel)
            {
                if(MP_HyperSpaceFlag)
                {
                    MP_HyperSpaceFlag = FALSE;
                    piePointModeOnOff(PIE_SPEC_Move, NULL);   // CAT: unguarded toggle, kept on Move (mode 0 would be fatal if it turned the plate on)
                    soundEvent(NULL, UI_MovementGUIoff);
                }
                else
                {
                    makeShipsControllable((SelectCommand *)&selSelected,COMMAND_MP_HYPERSPACING);
                    makeSelectionHyperspaceCapable((SelectCommand *)&selSelected);
                    if(selSelected.numShips == 0)
                    {
                        //removed all ships!
                        //if normal gui up..bring it down!
                        if (piePointSpecMode != PSM_Idle)
                        {
                            //turn it off!
                            soundEvent(NULL, UI_MovementGUIoff);
                            MP_HyperSpaceFlag = FALSE;
                            piePointModeOnOff(PIE_SPEC_None, NULL);   // CAT: off, mode 0 as the exe
                        }
                    }
                    else
                    {
                        MP_HyperSpaceFlag = TRUE;
                        if (piePointSpecMode == PSM_Idle)
                        {
                            //plate isn't up...turn it on!
                            piePointModeOnOff(PIE_SPEC_Move, NULL);
                        }
                    }
                }
            }
        }
    }
}

void smTacticalOverlayToggle(char *name, featom *atom)
{
    smTacticalOverlay ^= TRUE;
#if SM_VERBOSE_LEVEL >= 1
    dbgMessagef("smTacticalOverlayToggle: %s", smTacticalOverlay ? "ON" : "OFF");
#endif
}

void smNonResourceToggle(char *name, featom *atom)
{
    smNonResources ^= TRUE;
#if SM_VERBOSE_LEVEL >= 1
    dbgMessagef("smNonResourceToggle: %s", smNonResources ? "ON" : "OFF");
#endif
}

void smResourceToggle(char *name, featom *atom)
{
    smResources ^= TRUE;
#if SM_VERBOSE_LEVEL >= 1
    dbgMessagef("smResourceToggle: %s", smResources ? "ON" : "OFF");
#endif
}

void smSensorsClose(char *name, featom *atom)
{
    vector direction,dist;
    real32 dStart;

    // CAT (M2-R6 managers, sensors): the exe's body (0x0050c950): the tutorial gate is bit 0x8000000 of its first
    //      word; a big-gun aim (pie type 3) is cancelled first; the exit sound 0x12e; HW1's probe hack is a nullsub
    //      (mrNeedProbeHack returns FALSE); no taskbar flag
    if ((tutorial == TUTORIAL_ONLY) && !(((udword *)&tutEnable)[0] & 0x8000000))
    {
        return;
    }
    if (smZoomingIn || smZoomingOut)
    {                                                       //if hit while already zooming
        return;
    }
    if ((piePointSpecType == PIE_SPEC_BigGun) && (piePointSpecMode != PSM_Idle))
    {
        piePointModeOnOff(PIE_SPEC_None, NULL);
    }
    if (smFocusOnMothershipOnClose)
    {
        ccFocusOnMyMothership(&universe.mainCameraCommand);
        smFocusOnMothershipOnClose = FALSE;
    }
    ccLockOnTargetNow(&universe.mainCameraCommand);

    smLookStart = universe.mainCameraCommand.actualcamera.lookatpoint;
    smEyeStart = universe.mainCameraCommand.actualcamera.eyeposition;

    vecSub(direction, smEyeStart, smLookStart);
    dStart = fmathSqrt(vecMagnitudeSquared(direction));
    smEyeEnd = smCamera.eyeposition;
    smLookEnd = smCamera.lookatpoint;
    vecSub(direction, smLookEnd, smEyeEnd);
    vecNormalize(&direction);
    vecMultiplyByScalar(direction, dStart);
    vecSub(smEyeStart, smLookStart, direction);
    smZoomingIn = TRUE;

    soundEventPlay(NULL, 0x12e, NULL);                      // CAT: the sensors manager's exit

    smFocusTransition = FALSE;
    smInitialDistance = smCamera.distance;
    smFocus = FALSE;

    //now create a render list as it will be when fully zoomed in
    smTempCamera.lookatpoint = smLookStart;
    smTempCamera.eyeposition = smEyeStart;
    mrCamera = &smTempCamera;
    univUpdateRenderList();

    // check to see if pieplate active if it is and out of range turn it off.
    if (piePointSpecMode!=PSM_Idle)
    {
        selCentrePointCompute();
        vecSub(dist,selCentrePoint,universe.mainCameraCommand.actualcamera.lookatpoint);
        if (vecMagnitudeSquared(dist) >= MAX_MOVE_DISTANCE)
        {
            MP_HyperSpaceFlag = FALSE;
            piePointModeOnOff(PIE_SPEC_None, NULL);   // CAT: off, mode 0 as the exe
        }
    }
    MP_HyperSpaceFlag = FALSE;
}

void smSensorsSkip(char *name, featom *atom)
{
    // CAT (M2-R6 managers, sensors): smSensorsSkip 0x0050cc60: with close-captioned text showing (its region
    //      enabled, a card in use: 0x00abd2c2 / 0x00abd2d0) only that region fades out and the speech goes on
    if (subRegion[STR_CloseCaptioned].bEnabled && (subRegion[STR_CloseCaptioned].cardIndex > 0))
    {
        subMessageEnded = 2;
        subTitlesFadeOut(&subRegion[STR_CloseCaptioned], smSkipFadeoutTime);
        return;
    }
    subMessageEnded = 2;                                 //the fleet intel thing was skipped
    speechEventActorStop(ACTOR_ALL_ACTORS, smSkipFadeoutTime);
    subTitlesFadeOut(&subRegion[STR_LetterboxBar], smSkipFadeoutTime);
}

void smCancelDispatch(char *name, featom *atom)
{
#if SM_VERBOSE_LEVEL >= 1
    dbgMessage("smCancelDispatch");
#endif
    feToggleButtonSet(SM_TacticalOverlay, FALSE);
    piePointSpecMode = SPM_Idle;
}


#if SM_TOGGLE_SENSOR_LEVEL
void smToggleSensorsLevel(void)
{
    universe.curPlayerPtr->sensorLevel++;
    if (universe.curPlayerPtr->sensorLevel > 2)
    {
        universe.curPlayerPtr->sensorLevel = 0;
    }
#if SM_VERBOSE_LEVEL >= 1
    dbgMessagef("smToggleSensorsLevel: level set to %d", universe.curPlayerPtr->sensorLevel);
#endif
}
#endif //SM_TOGGLE_SENSOR_LEVEL


void smCancelMoveOrClose(char *name, featom *atom)
{
    MP_HyperSpaceFlag = FALSE;          //no matter what, set to FALSE
    if (piePointSpecMode != PSM_Idle)
    {
        piePointModeOnOff(PIE_SPEC_None, NULL);   // CAT: off, mode 0 as the exe
        soundEvent(NULL, UI_MovementGUIoff);
    }
    else
    {
        smSensorsClose(SM_Close, atom);
    }
}
/*=============================================================================
    Functions:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : smStartup
    Description : Start the sensors manager module.
    Inputs      : void
    Outputs     : adds some callbacks, loads some stuff etc, etc...
    Return      : void
----------------------------------------------------------------------------*/
fecallback smCallbacks[] =
{
    {smDispatch, SM_Dispatch},
    {smHyperspace, SM_Hyperspace},
    {smTacticalOverlayToggle, SM_TacticalOverlay},
    {smNonResourceToggle, SM_NonResource},
    {smResourceToggle, SM_Resource},
    {smSensorsClose, SM_Close},
    {smSensorsSkip, SM_Skip},
    {smCancelDispatch, SM_CancelDispatch},
    {smPan, SM_Pan},
    {smCancelMoveOrClose, SM_CancelMoveOrClose},
    {NULL, NULL},
};

void smStartup(void)
{
    // CAT (M2-R6 managers, sensors): the exe's body (0x0050ccc0): the tweaks, the viewport's draw callback, the
    //      camera, the parameters; the Cataclysm screen has only SM_ViewPort, so no button callbacks are added (the
    //      exe has none of HW1's SM_* button names) and there is no hold-function pair
    scriptSet(NULL, "sensors.script", smTweaks);
    feDrawCallbackAdd(SM_ViewportName, smViewportRender);   //add render callback

    cameraInit(&smCamera, SM_InitialCameraDist);
    smUpdateParameters();

    MP_HyperSpaceFlag = FALSE;      //set to FALSE...should manage itself from here on...
}

/*-----------------------------------------------------------------------------
    Name        : smUpdateParameters
    Description : Update the sensors manager parameters after a level has
                    loaded some sensors parameters.
    Inputs      :
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void smUpdateParameters(void)
{
    smCamera.clipPlaneNear = smZoomMin / smZoomMinFactor;
    smCamera.clipPlaneFar = smZoomMax * smZoomMaxFactor;
    smCamera.closestZoom = smZoomMin;
    smCamera.farthestZoom = smZoomMax;
    smCamera.distance = smInitialDistance;
    smCircleBorder = SM_CircleBorder;                       //ignore the value set in the level file
}

/*-----------------------------------------------------------------------------
    Name        : smShutdown
    Description : Shut down the sensors manager module.
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void smShutdown(void)
{
}

/*-----------------------------------------------------------------------------
    Name        : catSmReopenAfterManager
    Description : HWI helper (M2-R6 managers), the sequence the exe inlines in cmClose 0x00455680, lmCloseIfOpen
                  0x0049d300 and rmCloseIfOpen 0x004f4e30 when the closing manager had closed the sensors manager:
                  the return flag 0x00a5b230, the camera saved when the sensors manager opened (0x00c0aac0) copied
                  back (to 0x00c0a7e0), then smSensorsBegin
----------------------------------------------------------------------------*/
void catSmReopenAfterManager(void)
{
    catSmReopen = TRUE;                                     // 0x00a5b230
    catSmSavedCamera = smCamera;                            // 0x00c0aac0 -> 0x00c0a7e0, 0x17 dwords
    smSensorsBegin(NULL, NULL);
}

/*-----------------------------------------------------------------------------
    Name        : smSensorsBegin
    Description : Starts the sensors manager
    Inputs      : name, atom - ignored
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void smSensorsBegin(char *name, featom *atom)
{
    regionhandle reg;
    sdword index;
    fescreen *screen;
    char *screenName;

    // CAT (M2-R6 managers, sensors): the exe's body (0x0050cdb0): opened when the KAS opens it (catSmForceOpen,
    //      kasfOpenSensors 0x00496bc0) or when it is not disabled and the tutorial allows it (bit 0x8 of its first
    //      word); the viewport is the whole screen (no scrolling side regions), its callback the exe's
    //      (smViewportProcess) with the filter 0x36f6c and the main region's key children; the zoom starts from the
    //      main camera, or from the sensors camera a closing manager saved (catSmReopen, catSmReopenAfterManager),
    //      in which case it starts at the main-view end of the zoom; the intro sound 0x12d; no toggle buttons, no
    //      I/O lock, no taskbar hook
    bitClear(ghMainRegion->status, RSF_MouseInside);
    catMrRightMode = 0;
    catMrSelecting = FALSE;

    if (!catSmForceOpen && (smSensorsDisable || ((tutorial == TUTORIAL_ONLY) && !(((udword *)&tutEnable)[0] & 0x8))))
    {
        return;
    }
    screenName = smFleetIntel ? SM_FleetIntelScreenName : SM_ScreenName;
    screen = feScreenFind(screenName);
    smBaseRegion = feScreenStart(ghMainRegion, screenName);
    //now find the user region the sensors manager is rendered in
    reg = smBaseRegion->child;
    smViewportRegion = NULL;
    for (index = 1; index < screen->nAtoms; index++)
    {
        if (((featom *)reg->userID)->pData == (ubyte *)smViewportRender) //if this is a user region
        {
            smBackgroundColor = ((featom *)reg->userID)->contentColor;
            smViewportRegion = reg;                         //this must be the region
            break;
        }
        reg = reg->next;
    }
    regFunctionSet(smViewportRegion, (regionfunction) smViewportProcess);
    regFilterSet(smViewportRegion, 0x36f6c);
    smViewRectangle.x0 = 0;
    smViewRectangle.y0 = 0;
    smViewRectangle.x1 = MAIN_WindowWidth;
    smViewRectangle.y1 = MAIN_WindowHeight;
    smViewportRegion->rect = smViewRectangle;
    smZoomTime = 0.0f;
    if (smInstantTransition && !smFleetIntel)
    {
        smCurrentZoomLength = 0.1f;
        smCurrentMainViewZoomLength = 0.05f;
    }
    else
    {
        smCurrentZoomLength = smZoomLength;
        smCurrentMainViewZoomLength = smMainViewZoomLength;
    }

    //figure out the start/end points for the camera animation
    if (!catSmReopen)
    {
        smEyeStart = universe.mainCameraCommand.actualcamera.eyeposition;
        smCameraLookatPoint = universe.mainCameraCommand.actualcamera.lookatpoint;
    }
    else
    {
        smEyeStart = catSmSavedCamera.eyeposition;
        smCameraLookatPoint = catSmSavedCamera.lookatpoint;
    }
    vecSub(smEyeEnd, smEyeStart, smCameraLookatPoint);
    smCameraLookVelocity.x = smCameraLookVelocity.y = smCameraLookVelocity.z = 0.0f;
    smLookEnd = smCameraLookatPoint;
    smLookStart = smCameraLookatPoint;
    vecNormalize(&smEyeEnd);
    smZoomingIn = FALSE;
    smZoomingOut = TRUE;
    vecMultiplyByScalar(smEyeEnd, smInitialDistance);
    vecAddTo(smEyeEnd, smLookEnd);
    if (catSmReopen)
    {
        smZoomTime = smMainViewZoomLength;
        smCurrentZoomLength = smMainViewZoomLength;
        smZoomUpdate(smMainViewZoomLength, smMainViewZoomLength, TRUE);
        mrRenderMainScreen = FALSE;
        catSmReopen = FALSE;
    }
    mouseClipToRect(NULL);

    smSensorsActive = TRUE;

    soundEventStopSFX(0.5f);
    soundEventPlay(NULL, 0x12d, NULL);                      // CAT: the sensors manager's intro

    universe.dontUpdateRenderList = TRUE;
    // CAT: the exe calls a glcompat hook here (0x00b3a70c with 0x5000 when 0x00b3a6d8); no glcompat in the SDL build
    cat_mrKeyChildrenAlloc(smViewportRegion, (regionfunction) smViewportProcess);
    mouseCursorShow();
    cameraCopyPositionInfo(&smCamera, mrCamera);
}

/*-----------------------------------------------------------------------------
    Name        : smObjectDied
    Description : Called when an object dies, this function removes it from the
                    sensors manager blob in which it exists.
    Inputs      : object - object to delete
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void smObjectDied(void *object)
{
    sdword index;

//    bobObjectDied(object,&smBlobList);

    //remove it from the focus command, if there is one.
    if (smFocusCommand)
    {
        for (index = 0; index < smFocusCommand->numShips; index++)
        {
            if ((SpaceObj *)smFocusCommand->ShipPtr[index] == object)
            {
                for (; index < smFocusCommand->numShips - 1; index++)
                {
                    smFocusCommand->ShipPtr[index] = smFocusCommand->ShipPtr[index + 1];
                }
                smFocusCommand->numShips--;
                if (smFocusCommand->numShips <= 0)
                {
                    memFree(smFocusCommand);
                    smFocusCommand = NULL;
                    smFocus = FALSE;
                }
                return;
            }
        }
    }
}

/*=============================================================================
    Save Game Stuff:
=============================================================================*/

void smSave(void)
{
    SaveInfoNumber(Real32ToSdword(smDepthCueRadius));
    SaveInfoNumber(Real32ToSdword(smDepthCueStartRadius));
    SaveInfoNumber(Real32ToSdword(smCircleBorder));
    SaveInfoNumber(Real32ToSdword(smZoomMax));
    SaveInfoNumber(Real32ToSdword(smZoomMin));
    SaveInfoNumber(Real32ToSdword(smZoomMinFactor));
    SaveInfoNumber(Real32ToSdword(smZoomMaxFactor));
    SaveInfoNumber(Real32ToSdword(smInitialDistance));
    SaveInfoNumber(Real32ToSdword(smUniverseSizeX));
    SaveInfoNumber(Real32ToSdword(smUniverseSizeY));
    SaveInfoNumber(Real32ToSdword(smUniverseSizeZ));

    SaveInfoNumber(smSensorWeirdness);
}

void smLoad(void)
{
    sdword loadnum;

    loadnum = LoadInfoNumber(); smDepthCueRadius = SdwordToReal32(loadnum);
    loadnum = LoadInfoNumber(); smDepthCueStartRadius = SdwordToReal32(loadnum);
    loadnum = LoadInfoNumber(); smCircleBorder = SdwordToReal32(loadnum);
    loadnum = LoadInfoNumber(); smZoomMax = SdwordToReal32(loadnum);
    loadnum = LoadInfoNumber(); smZoomMin = SdwordToReal32(loadnum);
    loadnum = LoadInfoNumber(); smZoomMinFactor = SdwordToReal32(loadnum);
    loadnum = LoadInfoNumber(); smZoomMaxFactor = SdwordToReal32(loadnum);
    loadnum = LoadInfoNumber(); smInitialDistance = SdwordToReal32(loadnum);
    loadnum = LoadInfoNumber(); smUniverseSizeX = SdwordToReal32(loadnum);
    loadnum = LoadInfoNumber(); smUniverseSizeY = SdwordToReal32(loadnum);
    loadnum = LoadInfoNumber(); smUniverseSizeZ = SdwordToReal32(loadnum);

    smSensorWeirdness = LoadInfoNumber();

    smUpdateParameters();
}

/*-----------------------------------------------------------------------------
    Name        : cat_smZoomFadeColor                               0x0050b830
    Description : CAT (M2-R5 hud): a colour faded by the sensors manager's zoom progress (inverse: fade in). The
                  exe's fraction is its zoom clock over the zoom time (0x00c0aa1c / 0x00c0aa50, set by
                  smSensorsBegin); fraction 1 returns the colour, 0 black, else colMultiply. [M2-R6 managers: the
                  sensors manager's clock now (smZoomTime / smCurrentZoomLength)]
----------------------------------------------------------------------------*/
color cat_smZoomFadeColor(color c, sdword inverse)
{
    real32 fraction;

    // CAT (M2-R6 managers, sensors): the zoom clock is the sensors manager's (0x00c0aa1c / 0x00c0aa50), clamped to 0..1;
    //      outside the sensors manager the clock is 0 (and before the first opening 0 / 0), so the colour is black
    //      (the callers draw nothing for it), as the exe
    fraction = (smCurrentZoomLength != 0.0f) ? smZoomTime / smCurrentZoomLength : 0.0f;
    if (!(fraction >= 0.0f))
    {
        fraction = 0.0f;
    }
    else if (fraction > 1.0f)
    {
        fraction = 1.0f;
    }
    if (inverse == 1)
    {
        fraction = 1.0f - fraction;
    }
    if (fraction == 1.0f)
    {
        return c;
    }
    if (fraction == 0.0f)
    {
        return 0xff000000;
    }
    return colMultiply(c, fraction);
}
