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
// CAT (M2-R5 hud): 0x00a5b1ac, mines shown in the sensors manager (kasfShowMinesInSensors 0x00498490 sets it;
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
bool32 catSmHideViewport = FALSE;   // 0x00a5b228: the viewport draws nothing (singlePlayerGameUpdate sets it when
                                    //      Mission01's script starts, nisStart clears it)
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

/*=============================================================================
    CAT (M2-R7 r6-leftovers, decision 42): HW1's blob render is gone (smBlobsDraw, smBlobDrawClear, smBlobDrawCloudy,
    smBlobsSortToCamera, smBlobListSort, smBlobDropLineDistance, smEnemyFogOfWarBlobCallback, smBlobSphereDraw,
    smCursorTextDraw, the sensor weirdness smSensorWeirdnessInit / smSensorWeirdnessDraw / smwGenerate*, the
    commented-out smBlobRenderSelected, smAllBlobsPiePlateDraw, smMovePlaneDraw, smBabyRattleDraw,
    smTacticalOverlayDraw) with the derelict mesh table smDerelictTypeMesh that only it read: cataclysm.exe has none
    of them (cat_smViewportRenderScene draws the scene, below) and nothing in this tree called them since the M2-R6
    sensors port. The blob tweakables of sensors.script stay in smTweaks (the script sets them), as do the
    variables other modules set (smSensorWeirdness: kasfSensorsWeirdness).
=============================================================================*/

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
// CAT (M2-R8 singleplayer-tutorial): HW1's commented-out smEyeFactorGet / smLookFactorGet removed (the exe has neither)
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

// CAT (M2-R8 singleplayer-tutorial): HW1's smClickedOnPlayer and smPlayerNamesDraw (the multiplayer player list in the
//      sensors manager) are gone: cataclysm.exe has neither, and nothing called them since the M2-R6 sensors port
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
            soundEvent(NULL, UI_MovementGUIoff);   // CAT: HW1's id; no exe body (the Cataclysm screen has no SM_* buttons)
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
                    soundEvent(NULL, UI_MovementGUIoff);   // CAT: HW1's id; no exe body (the Cataclysm screen has no SM_* buttons)
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
                            soundEvent(NULL, UI_MovementGUIoff);   // CAT: HW1's id; no exe body (the Cataclysm screen has no SM_* buttons)
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
        soundEvent(NULL, UI_MovementGUIoff);   // CAT: HW1's id; no exe body (the Cataclysm screen has no SM_* buttons)
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
