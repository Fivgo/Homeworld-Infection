// =============================================================================
//  ConsMgr.c
//  - construction manager
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 7/18/1997 by lmoloney
// =============================================================================

#include "ConsMgr.h"
#include "SaveCat32.h"

#include "Battle.h"
#include "CommandLayer.h"
#include "CommandWrap.h"
#include "FEColour.h"
#include "FEFlow.h"
#include "FEReg.h"
#include "font.h"
#include "FontReg.h"
#include "glinc.h"
#include "InfoOverlay.h"
#include "mainrgn.h"
#include "mouse.h"
#include "Mothership.h"
#include "MultiplayerGame.h"
#include "Options.h"
#include "OracleDump.h"     // HWI-TEST: the build self-test rides on the headless mission run (/oracleMission)
#include "PiePlate.h"
#include "Ping.h"
#include "Randy.h"
#include "SaveGame.h"
#include "Scroller.h"
#include "Sensors.h"
#include "ShipView.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "StringSupport.h"
#include "TaskBar.h"
#include "Tutor.h"
#include "Tweak.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "Dock.h"
#include "Dreadnought.h"
#include "Carrier.h"
#include "Key.h"
#include "LaunchMgr.h"
#include "utility.h"
#include "LevelLoad.h"
#include "ShipNames.h"
#include "ResearchAPI.h"
#include "ResearchGUI.h"     // HWI-TEST: the build self-test opens the research screen
#include "uiscale.h"         // HWI-FEATURE(ui-scale): glScissor in physical pixels

/*=============================================================================
    Definitions:
=============================================================================*/
#ifdef _WIN32
#define CM_FIBFile              "FEMan\\Construction_Manager.fib"
#else
#define CM_FIBFile              "FEMan/Construction_Manager.fib"
#endif
#define CM_ConstructionScreen   "Construction_manager"
// CAT: the Beast player's screen (cmConstructionBegin 0x00458390: .data 0x00754f20 / 0x00754eec)
#ifdef _WIN32
#define CAT_CM_BeastFIBFile             "FEMan\\Beast_Construction_Manager.fib"
#else
#define CAT_CM_BeastFIBFile             "FEMan/Beast_Construction_Manager.fib"
#endif
#define CAT_CM_BeastConstructionScreen  "Beast_Construction_manager"
//#define CM_BuildScreen          "Build_screen"
#define CM_DefaultFont          "default.hff"
#define CM_ShipListFont         "HW_EuroseCond_11.hff"
#define CM_FontNameLength       64
#define CM_HeadingColorFactor   1.5f

//margins for printing RU's
#define CM_RUMarginRight        2
#define CM_RUMarginTop          2

//margins for available ships
#define CM_ASHeadingMarginLeft  3
#define CM_ASMarginLeft         9
#define CM_ASMarginTop          2
#define CM_ASMarginRight        2
#define CM_ASInterSpacing       2

#define CM_NumberJobsPerShip    128
#define CM_NumberJobsPerClass   10


#define CM_PausedColor          colRGB(80,120,80)
#define CM_SelectedColorSolid   colRGB(60,030,0)
#define CM_SelectedColorOutline colRGB(120,070,0)
#define CM_BackgroundColor      colRGB(0,35,79)
#define CM_SelectBoxX0          0
#define CM_SelectBoxX1          190
#define CM_SelectBoxY0          0
#define CM_SelectBoxY1          12

sdword cmSelectTopAdjust = 0;
sdword cmSelectBottomAdjust = 0;
sdword cmSelectLeftAdjust = 0;
sdword cmSelectRightAdjust = 0;

#define CM_FIVEKEY              CONTROLKEY      //hold to increase/decrease by 5

//task parameters
#define CM_TaskStackSize        4096
#define CM_TaskServicePeriod      UNIVERSE_UPDATE_PERIOD

//parameter defaults
#define CM_SelectionTextColor     colRGB(255, 255, 105)
#define CM_BuildingTextColor      colRGB(0, 255, 0)
#define CM_SelectionRectColor     colRGB(50, 120, 20)
#define CM_ProgressShipColor      colRGB(100, 100, 55)
#define CM_ProgressTotalColor     colRGB(180, 180, 80)
#define CM_StandardTextColor      colRGB(0,110,172)
#define CM_ClassHeadingTextColor  colRGB(255, 200, 0)
#define CM_CapsExceededTextColor  colRGB(255,25,25)
#define CM_RefundRatio          39321

#define CM_FloatRefundRatio     1.0f

// defines for shipavailable status word

#define ITEM_CLASS          0
#define ITEM_SHIP           1
#define CAT_ITEM_SHIPENABLED 2      // CAT: a ship row listed only when cat_playerShipTypeEnabled (cmUpdateShipAvailable
                                    //      0x00455410 tests kind 2); every other test is "not a class row" (kind != 0)

#define STAT_CANBUILD       0
#define STAT_CANTBUILD      1
#define STAT_PRINT          2
#define STAT_DONTPRINT      3
#define CAT_STAT_UNAVAILABLE 4      // CAT: listed but not buildable now: the type's cap, the support units, or a factory
                                    //      that cannot build (cmUpdateShipAvailable 0x00455410; drawn in grey)

// frame count for red flash when ship limits exceeded
#define CAPS_REDFRAMECOUNT 5;

//margins for printing construction progress
#define CM_ProgressMarginLeft   2
#define CM_ProgressMarginRight  90
#define CM_ProgressMarginTop    2
#define CM_ProgressInterSpacing 2
#define CM_SHIP_IMAGE_INSET     4

// CAT: six images per race, icon1/icon2 in _disabled/_off/_on states (cmLoadTextures 0x00455040: icon table
//      .data 0x00754a20..0x00754a38, image/texture tables 0x00a2dc58/0x007549e8 of 2 x 6 entries)
#define NUM_CMTEXTURES          6
// CAT: five decorative support-unit masks/icons, new in Cataclysm (cmLoadTextures 0x00455040, names .data 0x00754a68)
#define CAT_NUM_CMSUPPORTTEXTURES 5

// CAT: three carrier slots in Cataclysm (cmCarrierX 0x00a2dc4c..0x00a2dc58, the carrier regions 0x00a2dc24..0x00a2dc30;
//      cmConstructionBegin 0x00458390 fills them inline); the screen has two carrier buttons (CM_Carrier1Draw / 2Draw)
#define NUM_CMCARRIERS          3


/*=============================================================================
    Data:
=============================================================================*/
static sdword cmRenderEverythingCounter;

bool32 cmPaletted;

#if CM_CHEAP_SHIPS
bool32 cmCheapShips = FALSE;
#endif

void cmDeterministicStartup(void);
void cmDeterministicShutdown(void);

//callback tables for this screen
void cmClose(char *string, featom *atom);
void cmCancelJobs(char *string, featom *atom);
void cmPauseJobs(char *string, featom *atom);
void cmBuildShips(char *string, featom *atom);
void cmNumberRUsDraw(featom *atom, regionhandle region);
void cmShipCostsDraw(featom *atom, regionhandle region);
void cmShipInfoDraw(featom *atom, regionhandle region);
void cmShipNumbersDraw(featom *atom, regionhandle region);
void cmTotalRUsDraw(featom *atom, regionhandle region);
void cmTotalShipsDraw(featom *atom, regionhandle region);
void cmMotherShipDraw(featom *atom, regionhandle region);
void cmCarrier1Draw(featom *atom, regionhandle region);
void cmCarrier2Draw(featom *atom, regionhandle region);
void cat_cmSUBarDraw(featom *atom, regionhandle region);
void cat_cmFactoryNameDraw(featom *atom, regionhandle region);
void cat_cmResearch(char *string, featom *atom);
void cat_cmLaunch(char *string, featom *atom);
void cmLeftArrowDraw(featom *atom, regionhandle region);
void cmRightArrowDraw(featom *atom, regionhandle region);

void cmScroller(char *string, featom *atom);
// CAT (M2-R6 managers): used before their definitions since the list and factory functions moved
void cmUpdateShipAvailable(void);
sdword cmSelectAvailable(regionhandle region, sdword ID, udword event, udword data);
sdword cmRightArrowProcess(regionhandle region, sdword ID, udword event, udword data);
sdword cmLeftArrowProcess(regionhandle region, sdword ID, udword event, udword data);
sdword cmSelectMotherShip(regionhandle region, sdword ID, udword event, udword data);
sdword cmSelectCarrier1(regionhandle region, sdword ID, udword event, udword data);
sdword cmSelectCarrier2(regionhandle region, sdword ID, udword event, udword data);

void cmFillInCarrierXs(void);

ShipType cmKeyToShipType(uword key);

LinkedList listofShipsInProgress;

fecallback cmCallback[] =
{
    {cmClose,               "CM_Close"      },
    {cmBuildShips,          "CM_BuildShips" },
    {cmCancelJobs,          "CM_CancelJobs" },
    {cmPauseJobs,           "CM_PauseJobs"  },
    {cmScroller,            "CM_Scroller"   },
    // CAT: the exe's table 0x00753a18 also has CM_Research 0x004545e0 and CM_Launch 0x00454600 (M2-R6 managers: close
    //      this screen and open the research / launch manager) and CM_ExtendedInfo (M2-R5 hud)
    {cat_cmResearch,        "CM_Research"   },
    {cat_cmLaunch,          "CM_Launch"     },
    {cat_cmExtendedInfoToggle, "CM_ExtendedInfo" },
    {NULL,                  NULL            }
};
fedrawcallback cmDrawCallback[] =
{
    {cmNumberRUsDraw,           "CM_RUs"                },
    {cmShipCostsDraw,           "CM_ShipCost"           },
    {cmShipInfoDraw,            "CM_Ships"              },
    {cmShipNumbersDraw,         "CM_ShipNumbers"        },
    {cmTotalRUsDraw,            "CM_TotalCost"          },
    {cmTotalShipsDraw,          "CM_TotalShips"         },
    // CAT (M2-R6 managers): the exe's table 0x00753a60: no CM_ConnectorDraw, two carrier buttons, the support-unit bar
    //      and the factory's name
    {cmMotherShipDraw,          "CM_MotherShipDraw"     },
    {cmCarrier1Draw,            "CM_Carrier1Draw"       },
    {cmCarrier2Draw,            "CM_Carrier2Draw"       },
    {cmLeftArrowDraw,           "CM_LeftArrowDraw"      },
    {cmRightArrowDraw,          "CM_RightArrowDraw"     },
    {cat_cmSUBarDraw,           "CM_SUBarDraw"          },
    {cat_cmFactoryNameDraw,     "CM_FactoryNameDraw"    },
    {NULL,                      NULL                    }
};

extern ShipType svShipType;

// CAT (M2-R6 managers): the exe's six rows (.data 0x00753ad0, virtual-key codes there; this tree's key indices here):
//      the F keys build a Beast fighter or corvette, F12 a Worker of either race. HW1's rows (with its aliases) are gone
KeysToShips cmShipTypes[]=
{
    {F5KEY,          bAttackBomber},
    {F4KEY,          bCloakedFighter},
    {F7KEY,          bHeavyCorvette},
    {F11KEY,         bMultiGunCorvette},
    {F12KEY,         bWorker},
    {F12KEY,         sWorker},
    {-1,             0}             // CAT: the exe's table has no end row (its loops run into cmMaxIndex 18 after it)
};

bool32 cmPrintHotKey = FALSE;



//index of first displayed element
sword cmUpperIndex = 0;
//maximum number of displayable elements in our list
uword cmMaxIndex = 18;

//For drawing accelerator arrows
sdword cmArrowIndex = -1;

static scrollbarhandle g_shandle;

//flag indicating loaded status of this screen
fibfileheader *cmScreensHandle = NULL;

//handle to construction task
taskhandle cmBuildTask = -1;

regionhandle cmBaseRegion        = NULL;
regionhandle cmShipInfoRegion    = NULL;
regionhandle cmShipNumbersRegion = NULL;
regionhandle cmNumberRUsRegion   = NULL;
regionhandle cmTotalRUsRegion    = NULL;
regionhandle cmTotalShipsRegion  = NULL;
regionhandle cmShipCostsRegion   = NULL;
regionhandle cmMotherShipRegion  = NULL;
regionhandle cmCarrierRegions[NUM_CMCARRIERS] = { NULL, NULL, NULL };
regionhandle catCmSUBarRegion    = NULL;   // CAT: 0x00a2dc30, the CM_SUBarDraw region (cat_cmFactoryMarkDirty 0x00453c10 redraws it)
regionhandle cmFactoryNameRegion = NULL;   // CAT: 0x00a2dc34, the CM_FactoryNameDraw region (cat_cmFactoryNameDraw 0x00458920)

// CAT (M2-R6 managers): the exe's construction lists (cmUpdateFactory 0x00458180 picks one by the factory's race and
//      canBuildBigShips). Rows of kind CAT_ITEM_SHIPENABLED are listed only once the player may build the type
//      (cat_playerShipTypeEnabled 0x004f4780, a Beast ship's technologies: cat_rmAcquireShipTechs)
// CAT: race 0 (Sect), a factory without canBuildBigShips (.data 0x00753b10, 19 rows)
shipavailable catCmShipsAvailableSect[] =
{
    {0,ITEM_CLASS           ,0, CLASS_Fighter          , NULL},
    {0,ITEM_SHIP            ,0, sRecon                 , NULL},
    {0,ITEM_SHIP            ,0, sAcolyte               , NULL},
    {0,ITEM_SHIP            ,0, dAcolyte               , NULL},
    {0,ITEM_SHIP            ,0, sMimic                 , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Corvette         , NULL},
    {0,ITEM_SHIP            ,0, sACV                   , NULL},
    {0,ITEM_SHIP            ,0, sMCV                   , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Frigate          , NULL},
    {0,ITEM_SHIP            ,0, sRammingFrigate        , NULL},
    {0,ITEM_SHIP            ,0, sMultiBeamFrigate      , NULL},
    {0,ITEM_SHIP            ,0, sHiveFrigate           , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Micro            , NULL},
    {0,ITEM_SHIP            ,0, sLeech                 , NULL},
    {0,ITEM_SHIP            ,0, sSentinel              , NULL},
    {0,ITEM_CLASS           ,0, CLASS_NonCombat        , NULL},
    {0,ITEM_SHIP            ,0, sWorker                , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Component        , NULL},
    {0,ITEM_SHIP            ,0, sCarrierSupport        , NULL},
    {-1,0,0, 0                      , NULL}
};

// CAT: race 0 (Sect), canBuildBigShips (the sMothership) (.data 0x00753c50, 30 rows)
shipavailable catCmShipsAvailableSectBig[] =
{
    {0,ITEM_CLASS           ,0, CLASS_Fighter          , NULL},
    {0,ITEM_SHIP            ,0, sRecon                 , NULL},
    {0,ITEM_SHIP            ,0, sAcolyte               , NULL},
    {0,ITEM_SHIP            ,0, dAcolyte               , NULL},
    {0,ITEM_SHIP            ,0, sMimic                 , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Corvette         , NULL},
    {0,ITEM_SHIP            ,0, sACV                   , NULL},
    {0,ITEM_SHIP            ,0, sMCV                   , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Frigate          , NULL},
    {0,ITEM_SHIP            ,0, sRammingFrigate        , NULL},
    {0,ITEM_SHIP            ,0, sMultiBeamFrigate      , NULL},
    {0,ITEM_SHIP            ,0, sHiveFrigate           , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Destroyer        , NULL},
    {0,ITEM_SHIP            ,0, sDestroyer             , NULL},
    {0,ITEM_SHIP            ,0, sDreadnought           , NULL},
    {0,ITEM_SHIP            ,0, sCarrier               , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Micro            , NULL},
    {0,ITEM_SHIP            ,0, sLeech                 , NULL},
    {0,ITEM_SHIP            ,0, sSentinel              , NULL},
    {0,ITEM_CLASS           ,0, CLASS_NonCombat        , NULL},
    {0,ITEM_SHIP            ,0, sWorker                , NULL},
    {0,ITEM_SHIP            ,0, sProcessor             , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Component        , NULL},
    {0,ITEM_SHIP            ,0, sMothershipSupport     , NULL},
    {0,ITEM_SHIP            ,0, sMothershipDockingBay  , NULL},
    {0,ITEM_SHIP            ,0, sMothershipMicro       , NULL},
    {0,ITEM_SHIP            ,0, sMothershipSpecial     , NULL},
    {0,ITEM_SHIP            ,0, sMothershipArmour      , NULL},
    {0,ITEM_SHIP            ,0, sMothershipWeapons     , NULL},
    {0,ITEM_SHIP            ,0, sMothershipBigGun      , NULL},
    {-1,0,0, 0                      , NULL}
};

// CAT: race 1 (Beast) (.data 0x00753e40, 22 rows)
shipavailable catCmShipsAvailableBeast[] =
{
    {0,ITEM_CLASS           ,0, CLASS_Fighter          , NULL},
    {0,ITEM_SHIP            ,0, bRecon                 , NULL},
    {0,ITEM_SHIP            ,0, bInterceptor           , NULL},
    {0,ITEM_SHIP            ,0, bCloakedFighter        , NULL},
    {0,CAT_ITEM_SHIPENABLED ,0, bAcolyte               , NULL},
    {0,ITEM_SHIP            ,0, bAttackBomber          , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Corvette         , NULL},
    {0,ITEM_SHIP            ,0, bMultiGunCorvette      , NULL},
    {0,ITEM_SHIP            ,0, bHeavyCorvette         , NULL},
    {0,ITEM_SHIP            ,0, bMissileCorvette       , NULL},
    {0,ITEM_SHIP            ,0, bCruiseMissile         , NULL},
    {0,CAT_ITEM_SHIPENABLED ,0, bACV                   , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Frigate          , NULL},
    {0,ITEM_SHIP            ,0, bIonArrayFrigate       , NULL},
    {0,ITEM_SHIP            ,0, bDFGFrigate            , NULL},
    {0,CAT_ITEM_SHIPENABLED ,0, bRammingFrigate        , NULL},
    {0,CAT_ITEM_SHIPENABLED ,0, bMultiBeamFrigate      , NULL},
    {0,CAT_ITEM_SHIPENABLED ,0, bHiveFrigate           , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Micro            , NULL},
    {0,CAT_ITEM_SHIPENABLED ,0, bSentinel              , NULL},
    {0,ITEM_CLASS           ,0, CLASS_NonCombat        , NULL},
    {0,ITEM_SHIP            ,0, bWorker                , NULL},
    {-1,0,0, 0                      , NULL}
};

// CAT: race 1 (Beast), canBuildBigShips (.data 0x00753fb0, 28 rows)
shipavailable catCmShipsAvailableBeastBig[] =
{
    {0,ITEM_CLASS           ,0, CLASS_Fighter          , NULL},
    {0,ITEM_SHIP            ,0, bRecon                 , NULL},
    {0,ITEM_SHIP            ,0, bInterceptor           , NULL},
    {0,ITEM_SHIP            ,0, bCloakedFighter        , NULL},
    {0,CAT_ITEM_SHIPENABLED ,0, bAcolyte               , NULL},
    {0,ITEM_SHIP            ,0, bAttackBomber          , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Corvette         , NULL},
    {0,ITEM_SHIP            ,0, bMultiGunCorvette      , NULL},
    {0,ITEM_SHIP            ,0, bHeavyCorvette         , NULL},
    {0,ITEM_SHIP            ,0, bMissileCorvette       , NULL},
    {0,ITEM_SHIP            ,0, bCruiseMissile         , NULL},
    {0,CAT_ITEM_SHIPENABLED ,0, bACV                   , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Frigate          , NULL},
    {0,ITEM_SHIP            ,0, bIonArrayFrigate       , NULL},
    {0,ITEM_SHIP            ,0, bDFGFrigate            , NULL},
    {0,CAT_ITEM_SHIPENABLED ,0, bRammingFrigate        , NULL},
    {0,CAT_ITEM_SHIPENABLED ,0, bMultiBeamFrigate      , NULL},
    {0,CAT_ITEM_SHIPENABLED ,0, bHiveFrigate           , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Destroyer        , NULL},
    {0,ITEM_SHIP            ,0, bCarrier               , NULL},
    {0,ITEM_SHIP            ,0, bHeavyCruiser          , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Micro            , NULL},
    {0,CAT_ITEM_SHIPENABLED ,0, bSentinel              , NULL},
    {0,ITEM_CLASS           ,0, CLASS_NonCombat        , NULL},
    {0,ITEM_SHIP            ,0, bWorker                , NULL},
    {0,ITEM_SHIP            ,0, bProcessor             , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Component        , NULL},
    {0,ITEM_SHIP            ,0, bMothershipSupport     , NULL},
    {-1,0,0, 0                      , NULL}
};

// CAT: race 2 (Kushan) (.data 0x00754180, 15 rows)
shipavailable catCmShipsAvailableKushan[] =
{
    {0,ITEM_CLASS           ,0, CLASS_Fighter          , NULL},
    {0,ITEM_SHIP            ,0, kHeavyDefender         , NULL},
    {0,ITEM_SHIP            ,0, kHeavyInterceptor      , NULL},
    {0,ITEM_SHIP            ,0, kAttackBomber          , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Corvette         , NULL},
    {0,ITEM_SHIP            ,0, kHeavyCorvette         , NULL},
    {0,ITEM_SHIP            ,0, kSalCapCorvette        , NULL},
    {0,ITEM_SHIP            ,0, kMultiGunCorvette      , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Frigate          , NULL},
    {0,ITEM_SHIP            ,0, kAdvanceSupportFrigate , NULL},
    {0,ITEM_SHIP            ,0, kIonCannonFrigate      , NULL},
    {0,ITEM_CLASS           ,0, CLASS_NonCombat        , NULL},
    {0,ITEM_SHIP            ,0, kGravWellGenerator     , NULL},
    {0,ITEM_SHIP            ,0, kCloakGenerator        , NULL},
    {0,ITEM_SHIP            ,0, kProximitySensor       , NULL},
    {-1,0,0, 0                      , NULL}
};

// CAT: race 2 (Kushan), canBuildBigShips (.data 0x00754340, 19 rows)
shipavailable catCmShipsAvailableKushanBig[] =
{
    {0,ITEM_CLASS           ,0, CLASS_Fighter          , NULL},
    {0,ITEM_SHIP            ,0, kHeavyDefender         , NULL},
    {0,ITEM_SHIP            ,0, kHeavyInterceptor      , NULL},
    {0,ITEM_SHIP            ,0, kAttackBomber          , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Corvette         , NULL},
    {0,ITEM_SHIP            ,0, kHeavyCorvette         , NULL},
    {0,ITEM_SHIP            ,0, kSalCapCorvette        , NULL},
    {0,ITEM_SHIP            ,0, kMultiGunCorvette      , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Frigate          , NULL},
    {0,ITEM_SHIP            ,0, kAdvanceSupportFrigate , NULL},
    {0,ITEM_SHIP            ,0, kIonCannonFrigate      , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Destroyer        , NULL},
    {0,ITEM_SHIP            ,0, kStandardDestroyer     , NULL},
    {0,ITEM_SHIP            ,0, kHeavyCruiser          , NULL},
    {0,ITEM_SHIP            ,0, kCarrier               , NULL},
    {0,ITEM_CLASS           ,0, CLASS_NonCombat        , NULL},
    {0,ITEM_SHIP            ,0, kProximitySensor       , NULL},
    {0,ITEM_SHIP            ,0, kGravWellGenerator     , NULL},
    {0,ITEM_SHIP            ,0, kCloakGenerator        , NULL},
    {-1,0,0, 0                      , NULL}
};

// CAT: race 3 (Taiidan) (.data 0x00754560, 17 rows)
shipavailable catCmShipsAvailableTaiidan[] =
{
    {0,ITEM_CLASS           ,0, CLASS_Fighter          , NULL},
    {0,ITEM_SHIP            ,0, tHeavyDefender         , NULL},
    {0,ITEM_SHIP            ,0, tHeavyInterceptor      , NULL},
    {0,ITEM_SHIP            ,0, tAttackBomber          , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Corvette         , NULL},
    {0,ITEM_SHIP            ,0, tSalCapCorvette        , NULL},
    {0,ITEM_SHIP            ,0, tMinelayerCorvette     , NULL},
    {0,ITEM_SHIP            ,0, tMultiGunCorvette      , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Frigate          , NULL},
    {0,ITEM_SHIP            ,0, tStandardFrigate       , NULL},
    {0,ITEM_SHIP            ,0, tAdvanceSupportFrigate , NULL},
    {0,ITEM_SHIP            ,0, tDFGFrigate            , NULL},
    {0,ITEM_SHIP            ,0, tIonCannonFrigate      , NULL},
    {0,ITEM_CLASS           ,0, CLASS_NonCombat        , NULL},
    {0,ITEM_SHIP            ,0, tGravWellGenerator     , NULL},
    {0,ITEM_SHIP            ,0, tCloakGenerator        , NULL},
    {0,ITEM_SHIP            ,0, tProximitySensor       , NULL},
    {-1,0,0, 0                      , NULL}
};

// CAT: race 3 (Taiidan), canBuildBigShips (.data 0x00754720, 22 rows)
shipavailable catCmShipsAvailableTaiidanBig[] =
{
    {0,ITEM_CLASS           ,0, CLASS_Fighter          , NULL},
    {0,ITEM_SHIP            ,0, tHeavyDefender         , NULL},
    {0,ITEM_SHIP            ,0, tHeavyInterceptor      , NULL},
    {0,ITEM_SHIP            ,0, tAttackBomber          , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Corvette         , NULL},
    {0,ITEM_SHIP            ,0, tSalCapCorvette        , NULL},
    {0,ITEM_SHIP            ,0, tMinelayerCorvette     , NULL},
    {0,ITEM_SHIP            ,0, tMultiGunCorvette      , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Frigate          , NULL},
    {0,ITEM_SHIP            ,0, tStandardFrigate       , NULL},
    {0,ITEM_SHIP            ,0, tAdvanceSupportFrigate , NULL},
    {0,ITEM_SHIP            ,0, tDFGFrigate            , NULL},
    {0,ITEM_SHIP            ,0, tIonCannonFrigate      , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Destroyer        , NULL},
    {0,ITEM_SHIP            ,0, tStandardDestroyer     , NULL},
    {0,ITEM_SHIP            ,0, tMissileDestroyer      , NULL},
    {0,ITEM_SHIP            ,0, tHeavyCruiser          , NULL},
    {0,ITEM_SHIP            ,0, tCarrier               , NULL},
    {0,ITEM_CLASS           ,0, CLASS_NonCombat        , NULL},
    {0,ITEM_SHIP            ,0, tProximitySensor       , NULL},
    {0,ITEM_SHIP            ,0, tGravWellGenerator     , NULL},
    {0,ITEM_SHIP            ,0, tCloakGenerator        , NULL},
    {-1,0,0, 0                      , NULL}
};

// CAT: race 4 (P1), one list (.data 0x00754940, 7 rows)
shipavailable catCmShipsAvailableP1[] =
{
    {0,ITEM_CLASS           ,0, CLASS_Fighter          , NULL},
    {0,ITEM_SHIP            ,0, p1Fighter              , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Corvette         , NULL},
    {0,ITEM_SHIP            ,0, p1StandardCorvette     , NULL},
    {0,ITEM_SHIP            ,0, p1MissileCorvette      , NULL},
    {0,ITEM_CLASS           ,0, CLASS_Frigate          , NULL},
    {0,ITEM_SHIP            ,0, p1IonArrayFrigate      , NULL},
    {-1,0,0, 0                      , NULL}
};

shipsinprogress *curshipsInProgress;
sdword NUMBER_SHIPS_AVAILABLE = 0;
shipavailable *cmShipsAvailable;

CarrierInfo *cmCarriers = NULL;
sdword cmNumCarriers = 0;
sdword cmNumPlayersCarriers = 0;

static Ship *cmCarrierX[NUM_CMCARRIERS] = { NULL, NULL, NULL };     // don't export these for sync reasons
static ShipRace catCmScreensRace = RACE_Sect;   // CAT: 0x00a2dc00: the race whose screen file is loaded
Ship *catCmFactoryShip = NULL;      // CAT: 0x00c439b4, the factory the screen shows (set by its draws; name chosen here)
static bool32 catCmSensorsWereOpen = FALSE;     // CAT: 0x00a2dc38: the sensors manager was open (cmConstructionBegin)

//sdword cmSelectedShip = -1;

//colors for printing selections and stuff
color cmSelectionTextColor = CM_SelectionTextColor;
color cmBuildingTextColor = CM_BuildingTextColor;
color cmProgressShipColor = CM_ProgressShipColor;
color cmProgressTotalColor = CM_ProgressTotalColor;
color cmStandardTextColor = CM_StandardTextColor;
color cmClassHeadingTextColor = CM_ClassHeadingTextColor;
color cmCapsExceededTextColor = CM_CapsExceededTextColor;
color catCmUnavailableTextColor = colRGB(128, 128, 128);   // CAT: 0x007549dc: a listed type that cannot be built now (stat 4)
udword cmRefundRatio = CM_RefundRatio;
real32 cmFloatRefundRatio = CM_FloatRefundRatio;

bool32 cmIoSaveState;

udword cmShipTexture[MAX_RACES][NUM_CMTEXTURES] =
{
    {TR_InvalidInternalHandle, TR_InvalidInternalHandle, TR_InvalidInternalHandle,
     TR_InvalidInternalHandle, TR_InvalidInternalHandle, TR_InvalidInternalHandle},
    {TR_InvalidInternalHandle, TR_InvalidInternalHandle, TR_InvalidInternalHandle,
     TR_InvalidInternalHandle, TR_InvalidInternalHandle, TR_InvalidInternalHandle}
};

lifheader *cmShipImage[MAX_RACES][NUM_CMTEXTURES] =
{
    { NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL }
};


char *ShipImagePaths[] =
{
#ifdef _WIN32
    "FEMan\\Construction_Manager\\Build_race1_",
    "FEMan\\Construction_Manager\\Build_race2_"
#else
    "FEMan/Construction_Manager/Build_race1_",
    "FEMan/Construction_Manager/Build_race2_"
#endif
};

char *ShipIcons[] =
{
    // CAT: Cataclysm's names (.data 0x00754cdc..0x00754c88, table 0x00754a20); HW1: "icon1.lif", "icon2.lif"
    "icon1_disabled.lif",
    "icon1_off.lif",
    "icon1_on.lif",
    "icon2_disabled.lif",
    "icon2_off.lif",
    "icon2_on.lif"
};

// CAT: decorative support-unit textures (cmLoadTextures 0x00455040; path .data 0x00754eac "Feman\texDecorative\",
//      names table 0x00754a68; images 0x00a2dca0, textures 0x00754a7c)
#ifdef _WIN32
#define CAT_CM_SUPPORT_PATH "Feman\\texDecorative\\"
#else
#define CAT_CM_SUPPORT_PATH "Feman/texDecorative/"
#endif
char *catCmSupportIcons[CAT_NUM_CMSUPPORTTEXTURES] =
{
    "b_Build_SupportMask_Top.lif",
    "b_Build_SupportMask_Mid.lif",
    "b_Build_SupportMask_Low.lif",
    "b_Build_SupportIcon_Top.lif",
    "b_Build_SupportIcon_Low.lif"
};
udword catCmSupportTexture[CAT_NUM_CMSUPPORTTEXTURES] =
{
    TR_InvalidInternalHandle, TR_InvalidInternalHandle, TR_InvalidInternalHandle,
    TR_InvalidInternalHandle, TR_InvalidInternalHandle
};
lifheader *catCmSupportImage[CAT_NUM_CMSUPPORTTEXTURES] =
{
    NULL, NULL, NULL, NULL, NULL
};

udword cmArrowTexture[6] =
{
    TR_InvalidInternalHandle, TR_InvalidInternalHandle,TR_InvalidInternalHandle,
    TR_InvalidInternalHandle, TR_InvalidInternalHandle,TR_InvalidInternalHandle
};

lifheader *cmArrowIcon[6] =
{
    NULL, NULL, NULL, NULL, NULL, NULL
};


#ifdef _WIN32
#define ARROW_ICON_PATH "FEMan\\Textures\\"
#else
#define ARROW_ICON_PATH "FEMan/Textures/"
#endif

char *ArrowIcons[] =
{
    "Build_arrow_left_mouse.lif",
    "Build_Arrow_Left_on.lif",
    "Build_Arrow_Left_off.lif",
    "Build_Arrow_right_mouse.lif",
    "Build_Arrow_right_on.lif",
    "Build_Arrow_right_off.lif",
};




sdword cmCurrentSelect = 0;



//font for the ship listing
fonthandle cmDefaultFont = 0;
fonthandle cmShipListFont = 0;
fonthandle catCmFactoryNameFont = 0;       // CAT: 0x00a2dcc0, "EuroseWideHeavy_i13.hff" (cmStartup 0x004552b0)
char cmDefaultFontName[CM_FontNameLength]  = CM_DefaultFont;
char cmShipListFontName[CM_FontNameLength] = CM_ShipListFont;

// frame count for red flash when ship limits exceeded
#define CAPS_REDFRAMECOUNT 5;

// Ship Caps exceeded flags
real32 ExceededCapsFrames = 0.0;
uword ShipNumberExceeded = 0;

bool8 cmLeftArrowActive  = FALSE;
bool8 cmRightArrowActive = FALSE;

// Variable for storing the last viewed ship
ShipType curshipview=DefaultShip;

// Hack to fix Johan's hack ...
#define CM_WAIT_ACC 0.25f
extern real32 mgAccTime;

// Fix for unit caps in internet game ...
sdword shiplagtotals[TOTAL_NUM_SHIPS];

bool32 cmReallyPausedAll = FALSE;

bool32 cmActive=FALSE;

typedef struct cmDetermProgress_t
{
    Node     node;
    sdword   numShips;
    ShipType shipType;
    ShipRace shipRace;
    sdword   costPerShip;
    real32   costPerTick;
    real32   costSoFar;
    sdword   ticksForOne;
    sdword   ticksLeft;
    sdword   playerIndex;
    ShipPtr  creator;
    bool32     paused;
    // CAT (M2-R6 managers): the node is 0x48 bytes in Cataclysm (cmDeterministicBuildStart 0x00458e70 allocates 0x48);
    //      HW1's fields keep their offsets and three words follow for an externally built type (names chosen here):
    //      +0x3C its ship has reached its build position (cat_cmExternalBuildAttach 0x00455af0), +0x40 its ship
    //      exists, +0x44 the ship (set by cmDeterministicBuildProcess 0x004592a0)
    bool32   catAttached;
    bool32   catShipStarted;
    struct Ship *catShip;
} cmDetermProgress_t;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(cmDetermProgress_t) == 0x48, "a deterministic build node is 0x48 bytes in cataclysm.exe");
_Static_assert(offsetof(cmDetermProgress_t, costPerShip) == 0x1C, "cmDetermProgress_t.costPerShip +0x1C");
_Static_assert(offsetof(cmDetermProgress_t, playerIndex) == 0x30, "cmDetermProgress_t.playerIndex +0x30");
_Static_assert(offsetof(cmDetermProgress_t, catAttached) == 0x3C, "cmDetermProgress_t +0x3C");
#endif

LinkedList cmDetermProgress;

// CAT (M2-R6 managers): HW1's per-class job caps (cmClassCapRemap, cmMaxJobsPerClass, cmClassCapCanCreateShip) are gone:
//      the exe's cmIncrement 0x004565a0 asks only the unit caps (unitCapCanCreateShip) and the support units

/*=============================================================================
    Private functions:
=============================================================================*/

void updateShipView(void)
{
    udword index;
    bool32   set=TRUE;

    // CAT (M2-R6 managers): updateShipView 0x00453de0 takes any listed row that is not unbuildable (stat 0 or 4: a type at
    //      its cap still shows its ship), and keeps the viewed type unless its row is unbuildable
    if (curshipview == DefaultShip)
    {
        for (index = 0; cmShipsAvailable[index].nJobs != -1; index++)
        {                                                       //for all available ships
            if ((cmShipsAvailable[index].itemtype != ITEM_CLASS) && (cmShipsAvailable[index].itemstat != STAT_CANTBUILD))
            {
                svSelectShip(cmShipsAvailable[index].info->shiptype);
                curshipview = cmShipsAvailable[index].info->shiptype;
                break;
            }
        }
    }
    else
    {
        for (index = 0; cmShipsAvailable[index].nJobs != -1; index++)
        {                                                       //for all available ships
            if ( (cmShipsAvailable[index].itemtype != ITEM_CLASS) &&
                 (cmShipsAvailable[index].info->shiptype==curshipview) )
            {
                if (cmShipsAvailable[index].itemstat == STAT_CANTBUILD)
                {
                    break;
                }
                else
                {
                    svSelectShip(curshipview);
                    set = FALSE;
                }
            }
        }
        if (set)
        {
            for (index = 0; cmShipsAvailable[index].nJobs != -1; index++)
            {                                                       //for all available ships
                if ((cmShipsAvailable[index].itemtype != ITEM_CLASS) && (cmShipsAvailable[index].itemstat != STAT_CANTBUILD))
                {
                    svSelectShip(cmShipsAvailable[index].info->shiptype);
                    curshipview = cmShipsAvailable[index].info->shiptype;
                    break;
                }
            }
        }
    }
}


//return the progress pointed to by index
shipinprogress *cmSIP(udword index)
{
    //return &curshipsInProgress->progress[index];
    shipsinprogress *factory;
    factory = curshipsInProgress;

    return &factory->progress[index];
}


void cmWheelNegative(void)
{
    sdword ui = (sdword)cmUpperIndex-3;

    if (ui < 0)
        ui = 0;
    cmUpperIndex = (uword)ui;
}

void cmWheelPositive(void)
{
    sdword ui = (sdword)cmUpperIndex + 3;

    while (ui + cmMaxIndex > NUMBER_SHIPS_AVAILABLE)
    {
        ui--;
        if (ui < 0)
        {
            ui = 0;
            break;
        }
    }
    cmUpperIndex = (sword)ui;
}

// CAT (M2-R6 managers): the screen's region redraw the exe inlines in a dozen places (cmScroller 0x00453f30, cmAddFactory
//      0x004542f0, the build / cancel / pause buttons, the arrows, the list click ...): every info region and the
//      mothership and carrier buttons, without cmDirtyShipInfo's list refresh
static void catCmDirtyRegions(void)
{
    sdword i;

    if (cmShipInfoRegion != NULL)
    {
        cmShipInfoRegion->status |= RSF_DrawThisFrame;
    }
    if (cmNumberRUsRegion != NULL)
    {
        cmNumberRUsRegion->status |= RSF_DrawThisFrame;
    }
    if (cmTotalRUsRegion != NULL)
    {
        cmTotalRUsRegion->status |= RSF_DrawThisFrame;
    }
    if (cmTotalShipsRegion != NULL)
    {
        cmTotalShipsRegion->status |= RSF_DrawThisFrame;
    }
    if (cmShipCostsRegion != NULL)
    {
        cmShipCostsRegion->status |= RSF_DrawThisFrame;
    }
    if (cmShipNumbersRegion != NULL)
    {
        cmShipNumbersRegion->status |= RSF_DrawThisFrame;
    }
    if (cmMotherShipRegion != NULL)
    {
        cmMotherShipRegion->status |= RSF_DrawThisFrame;
    }
    for (i=0;i<NUM_CMCARRIERS;i++)
    {
        if (cmCarrierRegions[i] != NULL)
        {
            cmCarrierRegions[i]->status |= RSF_DrawThisFrame;
        }
    }
}

// CAT (M2-R6 managers): the local player's carriers into the three carrier slots (the exe inlines this in cmAddFactory,
//      cmReset, cmRemoveFactory, cmShutdown, cmConstructionBegin and LoadConsMgr)
void cmFillInCarrierXs(void)
{
    sdword i;

    for (i = 0; i < NUM_CMCARRIERS; i++)
    {
        cmCarrierX[i] = NULL;
    }
    cmNumPlayersCarriers = 0;
    for (i = 0; i < cmNumCarriers; i++)
    {
        if ((cmCarriers[i].owner == universe.curPlayerPtr) && (cmNumPlayersCarriers < NUM_CMCARRIERS))
        {
            cmCarrierX[cmNumPlayersCarriers++] = cmCarriers[i].ship;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cmDirtyShipInfo
    Description : refresh the construction screen
    CAT (M2-R6 managers): only while the screen is open (cmDirtyShipInfo 0x00454550), and the list is refreshed first
        (cmUpdateShipAvailable)
----------------------------------------------------------------------------*/
void cmDirtyShipInfo(void)
{
    if (cmBaseRegion != NULL)
    {
        cmUpdateShipAvailable();
        catCmDirtyRegions();
    }
}


void cmScroller(char *string, featom *atom)
{
    scrollbarhandle shandle;
    sword ind;

    if (FEFIRSTCALL(atom))
    {
        g_shandle = scSetupThumbwheel(cmUpperIndex, cmMaxIndex, (uword)NUMBER_SHIPS_AVAILABLE, atom);
        return;
    }

    shandle = (scrollbarhandle)atom->pData;

    if (shandle->mouseMoved != 0)
    {
        sword divs, up;
        real32 mm;

        catCmDirtyRegions();

        mm = (real32)shandle->mouseMoved;
        divs = (sword)(mm / shandle->divSize);
        up = (sword)(cmUpperIndex + divs);
        if (up < 0)
            up = 0;
        else if (up > NUMBER_SHIPS_AVAILABLE - cmMaxIndex)
            up = (sword)(NUMBER_SHIPS_AVAILABLE - cmMaxIndex);

        if (up <0)
            up = 0;

        cmUpperIndex = (uword)up;

        scAdjustThumbwheel(shandle, cmUpperIndex, cmMaxIndex, (uword)NUMBER_SHIPS_AVAILABLE);
        shandle->mouseMoved = 0;    //clear flag
        return;
    }

    switch (shandle->event)
    {
        case SC_Other:
        ind = (sword)cmUpperIndex;
        if (mouseCursorY() < shandle->thumb.y0)
        {
            ind = (sword)(ind - cmMaxIndex);
            if (ind < 0) ind = 0;
            cmUpperIndex = (uword)ind;
        }
        else
        {
            ind = (sword)(ind + cmMaxIndex);
            if (ind > NUMBER_SHIPS_AVAILABLE - cmMaxIndex)
                ind = (sword)(NUMBER_SHIPS_AVAILABLE - cmMaxIndex);

                        if (ind <0)
                                ind = 0;

            cmUpperIndex = (uword)ind;
        }
        catCmDirtyRegions();
        break;
    case SC_Thumb:
        break;
    case SC_Negative:
        cmWheelNegative();
        catCmDirtyRegions();
        break;
    case SC_Positive:
        cmWheelPositive();
        catCmDirtyRegions();
        break;
    default:
        dbgMessagef("unhandled consMgr scroll event %d", shandle->clickType);
    }

    scAdjustThumbwheel(shandle, cmUpperIndex, cmMaxIndex, (uword)NUMBER_SHIPS_AVAILABLE);
}

shipsinprogress *cmFindFactory(struct Ship *ship)
{
    Node *node = listofShipsInProgress.head;
    shipsinprogress *sinprogress;

    while (node != NULL)
    {
        sinprogress = (shipsinprogress *)listGetStructOfNode(node);
        if (sinprogress->ship == ship)
        {
            return sinprogress;
        }
        node = node->next;
    }
    return NULL;
}

void cmAddFactory(struct Ship *ship,bool32 canBuildBigShips)
{
    shipsinprogress *sinprogress;

    // CAT (M2-R6 managers): cmAddFactory 0x004542f0: in single player a disabled ship (catDisabled +0x210) is no
    //      factory; a carrier is recognised by its class (CLASS_Carrier), not HW1's type; the screen is redrawn only for
    //      the local player's factory (the regions, no list refresh) and the carrier slots are filled again
    if (singlePlayerGame && ship->shipOrDerelict.catDisabled)
    {
        return;
    }
    sinprogress = memAlloc(sizeof(shipsinprogress),"sinprogress",NonVolatile);
    memset(sinprogress,0,sizeof(shipsinprogress));

    if (ship->staticinfo->shipclass == CLASS_Carrier)       // add even if not for the player, because we want cmCarriers to be
                                            // deterministic for all machines to prevent sync problems
    {
        cmNumCarriers++;
        cmCarriers = memRealloc(cmCarriers, sizeof(CarrierInfo) * cmNumCarriers, "cmCarriers", NonVolatile);
        cmCarriers[cmNumCarriers - 1].ship  = ship;
        cmCarriers[cmNumCarriers - 1].owner = ship->playerowner;
    }
    sinprogress->ship = ship;
    sinprogress->canBuildBigShips = canBuildBigShips;
    listAddNode(&listofShipsInProgress,&sinprogress->node,sinprogress);
    if (universe.curPlayerPtr == ship->playerowner)
    {
        catCmDirtyRegions();
    }

    cmFillInCarrierXs();
}

void cmReset(void)
{
    if (!singlePlayerGameLoadNewLevelFlag)      // preserve this stuff if inbetween single player levels
    {
        listDeleteAll(&listofShipsInProgress);
        if (cmCarriers != NULL)
        {
            memFree(cmCarriers);
            cmCarriers = NULL;
            cmNumCarriers = 0;
        }
    }

    cmFillInCarrierXs();
}

void cmCloseIfOpen(void)
{
    if (cmBaseRegion != NULL)
    {
        cmClose(NULL,NULL);
    }
}

void cmPauseAllJbos(void)
{
    cmReallyPausedAll = TRUE;
}

void cmUnPauseAllJobs(void)
{
    cmReallyPausedAll = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cmRemoveFactory
    Description : a factory ship is gone
    CAT (M2-R6 managers): cmRemoveFactory 0x004546d0: a removed carrier takes its auto-launch bit out of the owner's word
        (cat_lmShipAutoLaunchBit 0x0049fa60: the higher bits move down one place) and frees its carrier slot; the screen
        is redrawn for the local player (the regions, no list refresh) and the slots are filled again only when a carrier
        went
----------------------------------------------------------------------------*/
void cmRemoveFactory(struct Ship *ship)
{
    sdword i, j, k;
    udword bit, autolaunch;
    Node *node;
    Node *nextnode;
    cmDetermProgress_t *dprog;
    bool32 carrierRemoved = FALSE;
    shipsinprogress *sinprogress = cmFindFactory(ship);
    if (sinprogress != NULL)
    {
        listDeleteNode(&sinprogress->node);
        if (curshipsInProgress == sinprogress)
        {
            curshipsInProgress = NULL;
            cmCloseIfOpen();
        }
    }

    node = cmDetermProgress.head;
    while (node != NULL)
    {
        dprog = (cmDetermProgress_t *)listGetStructOfNode(node);
        nextnode = node->next;

        if (dprog->creator == ship)
        {
            listDeleteNode(node);
        }

        node = nextnode;
    }

    for (i = 0; i < cmNumCarriers; i++)
    {
        if (cmCarriers[i].ship == ship)
        {
            bit = (udword)cat_lmShipAutoLaunchBit(ship);
            autolaunch = ship->playerowner->autoLaunch;
            ship->playerowner->autoLaunch = ((~((bit - 1) | bit) & autolaunch) >> 1) | (autolaunch & (bit - 1));

            for (k=0;k<NUM_CMCARRIERS;k++)
            {
                if (cmCarriers[i].ship == cmCarrierX[k])
                    cmCarrierX[k] = NULL;
            }

            // Resize the cmCarriers array
            for (j = i + 1; j < cmNumCarriers; j++)
            {
                cmCarriers[j - 1] = cmCarriers[j];
            }
            cmNumCarriers--;

            if (cmNumCarriers > 0)
            {
                cmCarriers = memRealloc(cmCarriers, sizeof(CarrierInfo) * cmNumCarriers, "cmCarriers", NonVolatile);
            }
            else
            {
                memFree(cmCarriers);
                cmCarriers = NULL;
            }
            carrierRemoved = TRUE;
            break;
        }
    }
    if (universe.curPlayerPtr == ship->playerowner)
    {
        catCmDirtyRegions();
    }
    if (carrierRemoved)
    {
        cmFillInCarrierXs();
    }
}


static bool32 resourcesHaveBeenAboveBefore = TRUE;      // 0x00754b54

// CAT: the exe's tutorial gates test bits of its own tutEnable word 0x00ba2e58 (as mainrgn.c's catMrTutEnabled): outside
//      the tutorial everything is enabled (tutGameEnableFlags has the exe's layout since M2-R8 singleplayer-tutorial)
static bool32 catCmTutEnabled(udword bit)
{
    return (tutorial != TUTORIAL_ONLY) || ((*(udword *)&tutEnable & bit) != 0);
}

/*-----------------------------------------------------------------------------
    CAT (M2-R6 managers): the factory list's helpers for the external build (names chosen here)
-----------------------------------------------------------------------------*/
// CAT: the factory entry of type `type` at factory ship `creator` (the loop the exe inlines in cmDeterministicBuildStart
//      0x00458e70, cmDeterministicBuildProcess 0x004592a0, cat_cmExternalBuildAttach 0x00455af0 ...); NULL when the
//      ship is no factory (the exe then works on the entry at address 0: those sites carry a CAT-FIX)
static shipinprogress *catCmFactoryEntry(Ship *creator, ShipType type)
{
    shipsinprogress *factory = cmFindFactory(creator);

    return (factory != NULL) ? &factory->progress[type] : NULL;
}

/*-----------------------------------------------------------------------------
    Name        : cat_cmFactoryMarkDirty
    Description : CAT: new (0x00453c10): redraw the construction screen's support-unit bar (0x00a2dc30)
----------------------------------------------------------------------------*/
void cat_cmFactoryMarkDirty(void)
{
    if (catCmSUBarRegion != NULL)
    {
        catCmSUBarRegion->status |= RSF_DrawThisFrame;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_cmFactoryHasFreeSlot
    Description : CAT: new (0x00454920): may the factory start an externally built ship of this type now (a free
                  external dock point of the build purpose, cat_dockHasFreeSlotForShip 0x00464910)
----------------------------------------------------------------------------*/
bool32 cat_cmFactoryHasFreeSlot(Ship *factoryship, ShipType shiptype, ShipClass shipclass)
{
    return cat_dockHasFreeSlotForShip(factoryship, shipclass, shiptype, TRUE, 0);
}

/*-----------------------------------------------------------------------------
    Name        : cat_cmBuildJobFinished
    Description : CAT: new (0x00453c20): one job of a factory entry is done. An externally built ship gets its last
                  progress step (done, cat_clWrapExternalBuildUpdate mode 0) and the entry forgets it; any other type
                  is created now (clWrapCreateShip; the ship-created count 0x00c43740 counts it in multiplayer). Then
                  the job count, the cost so far and the next job's clock, the welcome speech of a capital class
                  (battleLaunchWelcomeExchange 0x00424ef0, classes 0..4) and the new-ship ping after the last job.
                  Replaces the end of HW1's cmBuildTaskFunction; cmDeterministicBuildProcess calls it for an
                  externally built ship of the local player too
----------------------------------------------------------------------------*/
void cat_cmBuildJobFinished(shipinprogress *progress, Ship *factory)
{
    ShipStaticInfo *info;
    ShipClass shipclass;

    if (!gameIsRunning || cmReallyPausedAll || playPackets)
    {
        return;
    }
    if (progress->info->externalBuild == 1)
    {
        // CAT: not in a network game, recorded multiplayer games included (0x00a7519c && 0x00a75190)
        if ((progress->catExternalShipStarted == 1) && !catNetworkGame())
        {
            if (progress->catExternalShip != NULL)
            {
                cat_clWrapExternalBuildUpdate(&universe.mainCommandLayer, universe.curPlayerIndex, factory,
                                              progress->catExternalShip, 1.0f, CAT_EXTBUILD_DONE);
            }
            progress->catExternalShipStarted = FALSE;
            progress->catExternalShip = NULL;
        }
    }
    else
    {
        if (!multiPlayerGame)
        {
            clWrapCreateShip(&universe.mainCommandLayer, progress->info->shiptype, universe.curPlayerPtr->race,
                             universe.curPlayerIndex, factory);
        }
        if (multiPlayerGame)
        {
            shiplagtotals[progress->info->shiptype]++;          // 0x00c43740
        }
    }
    cat_cmFactoryMarkDirty();
    if (progress->nJobs > 0)
    {
        info = progress->info;
        progress->nJobs--;
        progress->costSoFar -= singlePlayerGame ? info->buildCost / catCmSinglePlayerCostFactor : info->buildCost;
        if (progress->nJobs > 0)
        {
            if (info->shipclass < CLASS_Corvette)
            {
                battleLaunchWelcomeExchange(info->shipclass, info->shiptype);
            }
            progress->timeFraction = 0;
            progress->timeLeft = progress->timeStart =
                singlePlayerGame ? progress->info->buildTime / catCmSinglePlayerBuildFactor : progress->info->buildTime;
            return;
        }
        progress->nJobsTotal = 0;
        pingNewShipPingCreate(&factory->obj.posinfo.position);
        shipclass = progress->info->shipclass;
        if (shipclass < CLASS_Corvette)
        {
            battleLaunchWelcomeExchange(shipclass, progress->info->shiptype);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cmBuildTaskFunction
    Description : advances every job of the local player's factories one universe tick
    Inputs      :
    Outputs     :
    Return      :
    CAT (M2-R6 managers): the exe's body (0x00454940): the backspace key skips the sensors manager's speech first; a
        single-player cost per tick is (buildCost << (17 - the cost divisor 0x0089419c)) / timeStart; nothing is
        charged or counted down while the universe is paused (0x00a6c5b4); a Beast mothership builds by its build
        factor (cat_beastMothershipBuildFactor 0x005bca90); an externally built type advances only once its ship
        exists and has reached its build position (catExternalShipStarted / catExternalShipAttached), and its ship
        gets the progress (cat_clWrapExternalBuildUpdate mode 2); the end of a job is cat_cmBuildJobFinished. A job of
        an externally built type that has no ship yet asks for a free build slot (cat_cmFactoryHasFreeSlot) and, but
        for a module (CLASS_Component) or at a Beast mothership, the docking bay module (cat_playerHasCompletedShip
        sMothershipDockingBay); with both its ship is created (clWrapCreateShip, at health 0 by univCreateShip) and
        the job restarts its clock, else only the clock restarts. The speech ids are the exe's
----------------------------------------------------------------------------*/
void cmBuildTaskFunction(void)
{
    static sdword index;                    // 0x00a2dbd4
    static shipinprogress *progress;        // 0x00a2dbcc
    static Node *node;                      // 0x00a2dbd0
    static shipsinprogress *sinprogress;    // 0x00a2dbc0
    static Ship *factoryship;               // 0x00a2dbc4
    static sdword decrement;                // 0x00a2dbd8

    if ((!gameIsRunning) || (cmReallyPausedAll) || playPackets)
    {
        return;     // CAT: playPackets as the exe (0x00a7518c); no record playback build
    }
    if (keyIsHit(BACKSPACEKEY))
    {
        dbgMessagef("speechlog: skip by backspace (build task) cm=%d", (int)cmActive);     // HWI-TEST (M3-R4 speech-log)
        smSensorsSkip(NULL, NULL);          // CAT: keyScanCode[VK_BACK] (0x00c36b88) skips the sensors speech
    }
    if (universe.curPlayerPtr->resourceUnits >= 300)
    {
        resourcesHaveBeenAboveBefore = TRUE;
    }

    node = listofShipsInProgress.head;
    while (node != NULL)
    {
        sinprogress = (shipsinprogress *)listGetStructOfNode(node);
        factoryship = sinprogress->ship;

        //this is a patch to avoid a very intermittent crash.
        if (!factoryship)
        {
            node = node->next;
            continue;
        }

        if (factoryship->playerowner == universe.curPlayerPtr)
        {
            for (index = 0, progress = &sinprogress->progress[0];
                 (index < TOTAL_NUM_SHIPS) && (universe.curPlayerPtr->resourceUnits > 0); index++, progress++)
            {
                if (progress->paused)
                {
                    continue;
                }
                if ((progress->nJobs > 0) && (progress->timeLeft > 0) &&
                    ((progress->info->externalBuild != 1) ||
                     (progress->catExternalShipStarted && progress->catExternalShipAttached)))
                {
                    if (singlePlayerGame)
                    {
                        decrement = ((progress->info->buildCost << ((17 - catCmSinglePlayerCostFactor) & 31)) / progress->timeStart) +
                                    progress->timeFraction;
                    }
                    else
                    {
                        decrement = ((progress->info->buildCost << 16) / progress->timeStart) + progress->timeFraction;
                    }
                    progress->timeFraction = decrement & 0x0000FFFF; // save fraction bits
                    decrement >>= 16; // Convert back to integer

                    if (!universePause)
                    {
                        if (factoryship->shiptype == bMothership)
                        {
                            decrement *= cat_beastMothershipBuildFactor(factoryship);
                        }
                        progress->costSoFar += decrement;
                        if (!multiPlayerGame)
                        {
                            universe.curPlayerPtr->resourceUnits -= decrement;
                        }
                        if (universe.curPlayerPtr->resourceUnits <= 0)
                        {
                            if (resourcesHaveBeenAboveBefore)
                            {
                                speechEventQueue(NULL, 0x2001037, 0, -1, 0, factoryship->playerowner->playerIndex, -1, 5.0f, -1);
                                resourcesHaveBeenAboveBefore = FALSE;
                            }
                            if (!multiPlayerGame)
                            {
                                universe.curPlayerPtr->resourceUnits = 0;
                            }
                        }
                        if (factoryship->shiptype == bMothership)
                        {
                            progress->timeLeft -= cat_beastMothershipBuildFactor(factoryship);
                        }
                        else
                        {
                            progress->timeLeft--;
                        }
                    }

                    if (progress->timeLeft <= 0)
                    {                                           //if job has ended
                        if (progress->timeFraction > 32767) // > 0.5, round up and reduce the resourceUnits by 1
                        {
                            if (!multiPlayerGame)
                            {
                                if (factoryship->shiptype == bMothership)
                                {
                                    universe.curPlayerPtr->resourceUnits -= cat_beastMothershipBuildFactor(factoryship);
                                }
                                else
                                {
                                    universe.curPlayerPtr->resourceUnits--;
                                }
                            }
                            if (universe.curPlayerPtr->resourceUnits <= 0)
                            {
                                if (resourcesHaveBeenAboveBefore)
                                {
                                    speechEventQueue(NULL, 0x2001047, -1, -1, 0, factoryship->playerowner->playerIndex, -1, 5.0f, -1);
                                    resourcesHaveBeenAboveBefore = FALSE;
                                }
                                if (!multiPlayerGame)
                                {
                                    universe.curPlayerPtr->resourceUnits = 0;
                                }
                            }
                        }
                        cat_cmBuildJobFinished(progress, factoryship);
                    }
                    else if (!multiPlayerGame && (progress->info->externalBuild == 1))
                    {
                        real32 fraction = (real32)(1.0 - (double)progress->timeLeft / (double)progress->timeStart);   // the double 1.0 at 0x0072a548, x87

                        // CAT: the exe calls a compiled-out function for sMothershipSpecial here (0x00454d9c)
                        if (progress->catExternalShip != NULL)
                        {
                            cat_clWrapExternalBuildUpdate(&universe.mainCommandLayer, universe.curPlayerIndex, factoryship,
                                                          progress->catExternalShip, fraction, CAT_EXTBUILD_PROGRESS);
                        }
                    }
                }
                // CAT: an externally built job without a ship (0x00454dcc)
                if ((progress->nJobs > 0) &&
                    ((progress->timeLeft == progress->timeStart) || (progress->timeLeft <= 0)) &&
                    (progress->info->externalBuild == 1) && !progress->catExternalShipStarted)
                {
                    if (!cat_cmFactoryHasFreeSlot(factoryship, progress->info->shiptype, progress->info->shipclass) ||
                        ((progress->info->shipclass != CLASS_Component) && (factoryship->shiptype != bMothership) &&
                         !cat_playerHasCompletedShip(sMothershipDockingBay, universe.curPlayerPtr)))
                    {
                        progress->timeFraction = 0;
                        progress->timeLeft = progress->timeStart =
                            singlePlayerGame ? progress->info->buildTime / catCmSinglePlayerBuildFactor : progress->info->buildTime;
                    }
                    else
                    {
                        if (!multiPlayerGame)
                        {
                            progress->catExternalShip = clWrapCreateShip(&universe.mainCommandLayer, progress->info->shiptype,
                                                                         universe.curPlayerPtr->race, universe.curPlayerIndex,
                                                                         factoryship);
                            cat_cmFactoryMarkDirty();
                        }
                        progress->timeFraction = 0;
                        progress->timeLeft = progress->timeStart =
                            singlePlayerGame ? progress->info->buildTime / catCmSinglePlayerBuildFactor : progress->info->buildTime;
                        if (!multiPlayerGame)
                        {
                            progress->catExternalShipStarted = TRUE;
                            progress->catExternalShipAttached = FALSE;
                        }
                    }
                }
            }
        }

        node = node->next;
    }
}

/*=============================================================================
    Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : cmLoadTextures
    Description : load textures used by the construction manager
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void cmLoadTextures(void)
{
    sdword race, index;
    char filename[128];

    for (race = 0; race < MAX_RACES; race++)
    {
        for (index = 0; index < NUM_CMTEXTURES; index++)
        {
            // build filename for loading texture from file
            strcpy(filename, ShipImagePaths[race]);
            strcat(filename, ShipIcons[index]);

            // load the image
            cmShipImage[race][index] = trLIFFileLoad(filename, NonVolatile);

            if (bitTest(cmShipImage[race][index]->flags, TRF_Paletted))
            {
                cmPaletted = TRUE;
            }
            else
            {
                cmPaletted = FALSE;
            }

            if (cmPaletted)
            {
                cmShipTexture[race][index] = trPalettedTextureCreate(
                    cmShipImage[race][index]->data,
                    cmShipImage[race][index]->palette,
                    cmShipImage[race][index]->width,
                    cmShipImage[race][index]->height);
            }
            else
            {
                cmShipTexture[race][index] = trRGBTextureCreate(
                    (color*)cmShipImage[race][index]->data,
                    cmShipImage[race][index]->width,
                    cmShipImage[race][index]->height,
                    TRUE);
            }
        }
    }

    for (index = 0; index < 6; index++)
    {
#if 1
        cmArrowIcon[index] = NULL;
        cmArrowTexture[index] = TR_InvalidInternalHandle;
#else
        strcpy(filename, ARROW_ICON_PATH);
        strcat(filename, ArrowIcons[index]);

        cmArrowIcon[index] = trLIFFileLoad(filename, NonVolatile);
        dbgAssertOrIgnore(cmArrowIcon[index]);

        cmArrowTexture[index] = trRGBTextureCreate(
            (color*)cmArrowIcon[index]->data,
            cmArrowIcon[index]->width,
            cmArrowIcon[index]->height,
            TRUE);
#endif
    }

    // CAT: load the support-unit textures (cmLoadTextures 0x00455040, last loop: RGB textures)
    for (index = 0; index < CAT_NUM_CMSUPPORTTEXTURES; index++)
    {
        strcpy(filename, CAT_CM_SUPPORT_PATH);
        strcat(filename, catCmSupportIcons[index]);
        catCmSupportImage[index] = trLIFFileLoad(filename, NonVolatile);
        catCmSupportTexture[index] = trRGBTextureCreate(
            (color*)catCmSupportImage[index]->data,
            catCmSupportImage[index]->width,
            catCmSupportImage[index]->height,
            TRUE);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cmCloseTextures
    Description : free memory and handles associated with any textures we might be using
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void cmCloseTextures(void)
{
    sdword race, index;

    for (race = 0; race < MAX_RACES; race++)
    {
        for (index = 0; index < NUM_CMTEXTURES; index++)
        {
            if (cmShipImage[race][index] != NULL)
            {
                memFree(cmShipImage[race][index]);
                cmShipImage[race][index] = NULL;
            }
            if (cmShipTexture[race][index] != TR_InvalidInternalHandle)
            {
                trRGBTextureDelete(cmShipTexture[race][index]);
                cmShipTexture[race][index] = TR_InvalidInternalHandle;
            }
        }
    }

    for (index = 0; index < 6; index++)
    {
        if (cmArrowIcon[index] != NULL)
        {
            memFree(cmArrowIcon[index]);
            cmArrowIcon[index] = NULL;
        }
        if (cmArrowTexture[index] != TR_InvalidInternalHandle)
        {
            trRGBTextureDelete(cmArrowTexture[index]);
            cmArrowTexture[index] = TR_InvalidInternalHandle;
        }
    }

    // CAT: free the support-unit textures (cmCloseTextures 0x004551e0, third loop)
    for (index = 0; index < CAT_NUM_CMSUPPORTTEXTURES; index++)
    {
        if (catCmSupportImage[index] != NULL)
        {
            memFree(catCmSupportImage[index]);
            catCmSupportImage[index] = NULL;
        }
        if (catCmSupportTexture[index] != TR_InvalidInternalHandle)
        {
            trRGBTextureDelete(catCmSupportTexture[index]);
            catCmSupportTexture[index] = TR_InvalidInternalHandle;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cmStartup
    Description : Start the construction manager module.
    Inputs      : void
    Outputs     : Starts a task to take care of construction.
    Return      : void
----------------------------------------------------------------------------*/
void cmStartup(void)
{
    sdword index;

    cmDeterministicStartup();

    listInit(&listofShipsInProgress);
//    cmBuildTask = taskStart(cmBuildTaskFunction, CM_TaskServicePeriod, 0);
    cmDefaultFont = frFontRegister(cmDefaultFontName);
    cmShipListFont = frFontRegister(cmShipListFontName);
    catCmFactoryNameFont = frFontRegister("EuroseWideHeavy_i13.hff");   // CAT: the third font (cmStartup 0x004552b0)

    cmStandardTextColor = FEC_ListItemStandard;

    cmLoadTextures();

    for (index=0;index<TOTAL_NUM_SHIPS;index++)
    {
        shiplagtotals[index] = 0;
    }

    cmRenderEverythingCounter = 0;

    /*
    textureRect.x0 = rect.x0+SP_TEXTURE_INSET;
    textureRect.y0 = rect.y0+SP_TEXTURE_INSET;
    textureRect.x1 = rect.x1-SP_TEXTURE_INSET;
    textureRect.y1 = rect.y1-SP_TEXTURE_INSET;

    primRectSolidTextured2(&textureRect);
    */
}

/*-----------------------------------------------------------------------------
    Name        : cmShutdown
    Description : Shut down the task manager
    Inputs      : void
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
void cmShutdown(void)
{
    cmDeterministicShutdown();

    if (cmScreensHandle)
    {
        feScreensDelete(cmScreensHandle);
        cmScreensHandle = NULL;
    }

    if (cmBuildTask != -1)
    {
        taskStop(cmBuildTask);
    }

    cmCloseTextures();

    cmReset();
}


/*-----------------------------------------------------------------------------
    Name        : cmShipDisabled
    Description : determines whether a given shiptype has been disabled for building or not
    Inputs      : type - ship type
    Outputs     :
    Return      : TRUE or FALSE
----------------------------------------------------------------------------*/
/*
bool32 cmShipDisabled(ShipType type)
{
    sdword index;

    for (index = 0; cmShipsDisabled[index].disabled != -1; index++)
    {
        if (cmShipsDisabled[index].type == type &&
            cmShipsDisabled[index].disabled)
        {
            return TRUE;
        }
    }

    return FALSE;
}
*/
/*-----------------------------------------------------------------------------
    Name        : cmUpdateShipAvailable
    Description : updates the ship build list for technology state.
    Inputs      : none
    Outputs     : none
    Return      : void
    CAT (M2-R6 managers): the exe's body (0x00455410). A ship row is unbuildable (STAT_CANTBUILD) when its kind needs the
        type enabled and it is not (cat_playerShipTypeEnabled), or the research does not allow it (rmCanBuildShip, its
        third argument 1 when the shown factory is the sMothership) or its static is not loaded; a buildable row counts in
        NUMBER_SHIPS_AVAILABLE and is STAT_CANBUILD, or CAT_STAT_UNAVAILABLE at its cap (cdLimitCaps; for the carrier's
        support module the shown carrier's modules count, cat_carrierCountSupportModules); a row with no cap
        (cdLimitCaps < 0) is CAT_STAT_UNAVAILABLE too when the player's support units do not cover its supportUnits
        and the queue (cat_cmQueuedSupportUnits; for the local player or in multiplayer); and every row but a module's
        is CAT_STAT_UNAVAILABLE while the shown factory is missing, switched off (CAT_ATTRIBUTES_FactoryOff) or
        hyperspacing. Then the class headers from the end: STAT_PRINT (counted) when any row of the class is listed
----------------------------------------------------------------------------*/
static Ship *catCmShownFactoryShip(void)
{
    return (cmCurrentSelect == 0) ? universe.curPlayerPtr->PlayerMothership : cmCarrierX[cmCurrentSelect - 1];
}

void cmUpdateShipAvailable(void)
{
    sdword index;
    sdword isclass=STAT_DONTPRINT;
    bool32 bigFactory;
    ShipType type;
    ShipStaticInfo *info;
    Player *player;
    Ship *factoryship;

    NUMBER_SHIPS_AVAILABLE=0;

    for (index = 0; cmShipsAvailable[index].nJobs != -1; index++)
    {
        if (cmShipsAvailable[index].itemtype == ITEM_CLASS)
        {
            continue;
        }
        type = cmShipsAvailable[index].type;
        bigFactory = (curshipsInProgress != NULL) && (curshipsInProgress->ship->shiptype == sMothership);
        if ((cmShipsAvailable[index].itemtype == CAT_ITEM_SHIPENABLED) && !cat_playerShipTypeEnabled(universe.curPlayerPtr, type))
        {
            cmShipsAvailable[index].itemstat = STAT_CANTBUILD;
        }
        else if (!rmCanBuildShip(universe.curPlayerPtr, type, bigFactory) ||
                 !bitTest(cmShipsAvailable[index].info->common.infoFlags, IF_InfoLoaded))
        {
            cmShipsAvailable[index].itemstat = STAT_CANTBUILD;
        }
        else
        {
            NUMBER_SHIPS_AVAILABLE++;
            if (cdLimitCaps[type] < 1)
            {
                cmShipsAvailable[index].itemstat = STAT_CANBUILD;
            }
            else if ((type == sCarrierSupport) && (cmCurrentSelect > 0))
            {
                cmShipsAvailable[index].itemstat =
                    (cat_carrierCountSupportModules(cmCarrierX[cmCurrentSelect - 1]) < cdLimitCaps[type]) ? STAT_CANBUILD : CAT_STAT_UNAVAILABLE;
            }
            else
            {
                cmShipsAvailable[index].itemstat =
                    (universe.curPlayerPtr->shiptotals[type] < cdLimitCaps[type]) ? STAT_CANBUILD : CAT_STAT_UNAVAILABLE;
            }
        }
        player = universe.curPlayerPtr;
        if ((cmShipsAvailable[index].itemstat == STAT_CANBUILD) && (cdLimitCaps[type] < 0))
        {
            info = GetShipStaticInfo(type, player->race);
            if (((player->playerIndex == 0) || !singlePlayerGame) && (info->supportUnits != 0))
            {
                if (((udword)player->supportUnitsMax <= (udword)player->supportUnitsUsed) ||
                    ((udword)player->supportUnitsMax - (udword)player->supportUnitsUsed <
                     info->supportUnits + (udword)cat_cmQueuedSupportUnits(player)))
                {
                    cmShipsAvailable[index].itemstat = CAT_STAT_UNAVAILABLE;
                }
            }
        }
        factoryship = catCmShownFactoryShip();
        if (((factoryship == NULL) || bitTest(factoryship->obj.attributes, CAT_ATTRIBUTES_FactoryOff) ||
             bitTest(factoryship->shipOrDerelict.specialFlags, SPECIAL_Hyperspacing)) &&
            (cmShipsAvailable[index].itemstat == STAT_CANBUILD) &&
            (cmShipsAvailable[index].info->shipclass != CLASS_Component))
        {
            cmShipsAvailable[index].itemstat = CAT_STAT_UNAVAILABLE;
        }
    }

    for (index--; index >= 0; index--)
    {
        if (cmShipsAvailable[index].itemtype == ITEM_CLASS)
        {
            cmShipsAvailable[index].itemstat = (sbyte)isclass;
            if (isclass==STAT_PRINT) NUMBER_SHIPS_AVAILABLE++;
            isclass=STAT_DONTPRINT;
        }
        else if (cmShipsAvailable[index].itemstat != STAT_CANTBUILD)
        {
            isclass=STAT_PRINT;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cmClose
    Description : Callback function to close the construction manager
    Inputs      :
    Outputs     : Deletes all regions associated with construction manager
    Return      :
    CAT (M2-R6 managers): the exe's body (0x00455680): the tutorial gate is its own word's bit 0x2000000; the main view
        draws again (mrRenderMainScreen), the sounds 0x130 / 0x100, every region forgotten, the ship view closed, the
        key detour off; the sensors manager opens again when the screen closed it (its saved camera restored first,
        0x00c0aac0 -> 0x00c0a7e0, 0x17 dwords, and the return flag 0x00a5b230: catSmReopenAfterManager, Sensors.c). No
        info overlay restore, no taskbar flag
----------------------------------------------------------------------------*/
void cmClose(char *string, featom *atom)
{                                                           //close the construction manager
    sdword i;

    if (!catCmTutEnabled(0x2000000))
    {
        return;
    }
    if (cmBaseRegion != NULL)
    {
        dbgMessagef("speechlog: build manager closes tick=%u", (unsigned)universe.univUpdateCounter);     // HWI-TEST (M3-R4 speech-log)
    }

    cmRenderEverythingCounter = 0;
    curshipsInProgress = NULL;
    cmUpperIndex=0;
    feScreenDeleteFlags(cmBaseRegion,FE_DONT_DELETE_REGION_IF_SCREEN_NOT_FOUND);
    cmBaseRegion = NULL;

    // enable rendering of main game screen
    mrRenderMainScreen = TRUE;

    soundEventPlay(NULL, 0x130, NULL);                   // CAT: the manager exit
    soundEventPlay(NULL, UI_SoundOfSpace, NULL);         // CAT: 0x100

    spUnlockout();

    cmShipInfoRegion    = NULL;
    cmNumberRUsRegion   = NULL;
    cmTotalRUsRegion    = NULL;
    cmTotalShipsRegion  = NULL;
    cmShipCostsRegion   = NULL;
    cmShipNumbersRegion = NULL;
    cmMotherShipRegion  = NULL;
    catCmSUBarRegion    = NULL;
    cmFactoryNameRegion = NULL;
    for (i=0;i<NUM_CMCARRIERS;i++)
    {
        cmCarrierRegions[i] = NULL;
    }

    svClose();

    opKeyDetour = 0; //no key detours

    cmPrintHotKey = FALSE;

    cmActive = FALSE;

    if (catCmSensorsWereOpen)
    {
        catSmReopenAfterManager();
    }
    cat_tutButtonsRestoreAfterScreen();      // CAT: the tutorial's buttons come back (cmClose 0x00455780)
}



/*-----------------------------------------------------------------------------
    Name        : cmBuildShips
    Description : Callback function to build selected ships.
    Inputs      :
    Outputs     : Start building all the selected ships
    Return      :
    CAT (M2-R6 managers): the exe's body (0x00455790, CM_BuildShips, no Ghidra function): the tutorial gate bit
        0x400000; every queued row becomes jobs at the factory (cmBuildJobsAdd); then the speech: one capital type
        (classes 0..4) alone gets its own line 0x400103e with the type, anything else the general 0x4001035; the screen
        regions are redrawn
----------------------------------------------------------------------------*/
void cmBuildShips(char *string, featom *atom)
{
    sdword index;
    bool32 builtSomething = FALSE;
    bool32 capShipBuilt = FALSE;
    bool32 otherType = FALSE;
    ShipType capShipType = 0;

    if (!catCmTutEnabled(0x400000))
    {
        return;
    }

    for (index = 0; cmShipsAvailable[index].nJobs != -1; index++)
    {                                                       //for all available ships
        if (cmShipsAvailable[index].nJobs > 0)
        {
            cmBuildJobsAdd(curshipsInProgress, cmShipsAvailable[index].info, cmShipsAvailable[index].nJobs);
            builtSomething = TRUE;
            if (capShipBuilt)
            {
                if (capShipType != cmShipsAvailable[index].info->shiptype)
                {
                    otherType = TRUE;
                }
            }
            else if (cmShipsAvailable[index].info->shipclass <= CLASS_Frigate)
            {
                capShipType = cmShipsAvailable[index].info->shiptype;
                capShipBuilt = TRUE;
            }
        }
    }

    for (index = 0; cmShipsAvailable[index].nJobs != -1; index++)
    {                                                       //for all available ships
        cmShipsAvailable[index].nJobs = 0;                    //no jobs queued yet
    }

    if (builtSomething)
    {
        if (capShipBuilt && !otherType)
        {
            speechEventQueue(NULL, 0x400103e, capShipType, -1, 0, universe.curPlayerIndex, -1, 5.0f, -1);
        }
        else
        {
            speechEventQueue(NULL, 0x4001035, 0, -1, 0, universe.curPlayerIndex, -1, 5.0f, -1);
        }
    }

    catCmDirtyRegions();
}

/*-----------------------------------------------------------------------------
    Name        : cmCancelJobs
    Description : Callback to cancel all current jobs
    CAT (M2-R6 managers): the exe's body (0x00455c10, CM_CancelJobs): the tutorial gate bit 0x1000000; the selected
        rows' queue cleared (cancelled in the deterministic list in multiplayer); each selected entry of the 0x91
        refunds its cost so far times cmFloatRefundRatio (single player), deletes its externally built ship
        (cat_clWrapExternalBuildUpdate mode 1) and is cleared, the external words too; the speech 0x400103b; the regions
        redrawn. The selection stays
----------------------------------------------------------------------------*/
void cmCancelJobs(char *string, featom *atom)
{
    sdword index;
    shipinprogress *progress;
    bool32 canceled = FALSE;

    if (!catCmTutEnabled(0x1000000))
    {
        return;
    }

    progress = &curshipsInProgress->progress[0];

    for (index = 0; cmShipsAvailable[index].nJobs != -1; index++)
    {                                                       //for all available ships
        if ((cmShipsAvailable[index].itemtype != ITEM_CLASS) && progress[cmShipsAvailable[index].type].selected)
        {
            cmShipsAvailable[index].nJobs = 0;
            canceled = TRUE;
            // CAT: also in a recorded multiplayer game (0x00a7519c && 0x00a75190)
            if (catNetworkGame() && (cmShipsAvailable[index].info != NULL))
            {
                clWrapDeterministicBuild(CMD_CANCEL, &universe.mainCommandLayer, 0, cmShipsAvailable[index].type,
                                         cmShipsAvailable[index].info->shiprace, universe.curPlayerIndex,
                                         curshipsInProgress->ship);
            }
        }
    }

    for (index = 0; index < TOTAL_NUM_SHIPS; index++, progress++)
    {                                                   //for all classes of ships
        if (!progress->selected)
        {
            continue;
        }
        if (progress->nJobsTotal != 0)
        {
            if (!multiPlayerGame)
            {
                universe.curPlayerPtr->resourceUnits += (sdword)((real32)progress->costSoFar * cmFloatRefundRatio);
            }
            if (progress->info->externalBuild && (progress->catExternalShipStarted == 1) && !multiPlayerGame)
            {
                if (progress->catExternalShip != NULL)
                {
                    cat_clWrapExternalBuildUpdate(&universe.mainCommandLayer, universe.curPlayerIndex, curshipsInProgress->ship,
                                                  progress->catExternalShip, 0.0f, CAT_EXTBUILD_CANCEL);
                }
                progress->catExternalShipStarted = FALSE;
                progress->catExternalShip = NULL;
            }
        }
        progress->timeFraction = 0; // No fixed point fraction bits (used for decrementing RUs)
        progress->timeStart = progress->timeLeft = 0;   //job all done
        progress->nJobsTotal = progress->nJobs = 0;     //cancel building jobs
        progress->catExternalShipAttached = FALSE;
        progress->catExternalShipStarted = FALSE;
        progress->catExternalShip = NULL;
        progress->costSoFar = 0;
        canceled = TRUE;
        if (multiPlayerGame && (progress->info != NULL))
        {
            clWrapDeterministicBuild(CMD_CANCEL, &universe.mainCommandLayer, 0, progress->info->shiptype,
                                     progress->info->shiprace, universe.curPlayerIndex, curshipsInProgress->ship);
        }
    }

    if (canceled)
    {
        speechEventQueue(NULL, 0x400103b, 0, -1, 0, universe.curPlayerIndex, -1, 5.0f, -1);
    }

    catCmDirtyRegions();
}

/*-----------------------------------------------------------------------------
    Name        : cmPauseJobs
    Description : Callback to pause/unpause all current jobs
    CAT (M2-R6 managers): the exe's body (0x00456060, CM_PauseJobs): the tutorial gate bit 0x800000; the 0x91 entries;
        the speech 0x4001036 (paused) or 0x400103c (resumed) when a running job changed; the regions redrawn
----------------------------------------------------------------------------*/
void cmPauseJobs(char *string, featom *atom)
{
    sdword index;
    shipinprogress *progress;
    bool32 allpaused = TRUE, changed=FALSE;
    udword command;

    if (!catCmTutEnabled(0x800000))
    {
        return;
    }

    //all paused?
    progress = &curshipsInProgress->progress[0];
    for (index = 0; index < TOTAL_NUM_SHIPS; index++, progress++)
    {                                                   //for all classes of ships
        if (allpaused && progress->selected && !progress->paused)
        {
            allpaused = FALSE;
        }
    }

    //set all jobs to opposite of allpaused
    progress = &curshipsInProgress->progress[0];
    for (index = 0; index < TOTAL_NUM_SHIPS; index++, progress++)
    {                                                   //for all classes of ships
        if (progress->selected)
        {
            progress->paused = !allpaused;
            if (progress->nJobs>0) changed = TRUE;

            command = progress->paused ? CMD_PAUSE : CMD_UNPAUSE;
            // CAT: also in a recorded multiplayer game (0x00a7519c && 0x00a75190)
            if (catNetworkGame() && (progress->info != NULL))
            {
                clWrapDeterministicBuild(command, &universe.mainCommandLayer, 0, progress->info->shiptype,
                                         progress->info->shiprace, universe.curPlayerIndex, curshipsInProgress->ship);
            }
        }
    }

    if (changed)
    {
        speechEventQueue(NULL, allpaused ? 0x400103c : 0x4001036, 0, -1, 0, universe.curPlayerIndex, -1, 5.0f, -1);
    }
    catCmDirtyRegions();
}

/*-----------------------------------------------------------------------------
    Name        : cat_cmResearch / cat_cmLaunch
    Description : CAT: new (0x004545e0 CM_Research / 0x00454600 CM_Launch): close the construction manager and open the
                  research manager (mrResearch) or the launch manager (mrLaunch)
----------------------------------------------------------------------------*/
void cat_cmResearch(char *string, featom *atom)
{
    cmClose(NULL, NULL);
    mrResearch(NULL, NULL);
}

void cat_cmLaunch(char *string, featom *atom)
{
    cmClose(NULL, NULL);
    mrLaunch(NULL, NULL);
}

/*-----------------------------------------------------------------------------
    Name        : cat_cmIsOpen
    Description : CAT: new (0x0045a250): is the construction manager open (the subtitles and the tutorial pointers ask)
----------------------------------------------------------------------------*/
sdword cat_cmIsOpen(sdword flags)
{
    (void)flags;
    return (cmBaseRegion != NULL);
}

/*-----------------------------------------------------------------------------
    Name        : cat_cmSUBarDraw
    Description : CAT: new (0x00459f50, the CM_SUBarDraw callback): the support-unit gauge: the region's top and bottom
                  halves (SupportIcon_Top / _Low), then in a 16-pixel column 4 pixels from the right edge the free share
                  of the player's support units (1 - used / cap) as a translucent column (SupportMask_Top / _Mid /
                  _Low blended with 0xc0500000) and over it the share left after the screen's queue
                  (cat_cmQueuedSupportUnits / cap less) in solid; the column's height is the region's less 22 pixels.
                  The region redraws itself every frame
----------------------------------------------------------------------------*/
static real32 catCmClampUnit(real32 f)
{
    if (!(f >= 0.0f))
    {
        return 0.0f;        // below 0 or NaN (no support units at all: 0 / 0), as the exe's x87 compare
    }
    return (f > 1.0f) ? 1.0f : f;
}

static void catCmSUBarColumn(sdword x0, sdword x1, sdword top, sdword bottom, bool32 blended)
{
    rectangle rect;
    sdword k;
    static const sdword tex[3] = { 1, 2, 0 };     // Mask_Mid, Mask_Low, Mask_Top

    for (k = 0; k < 3; k++)
    {
        rect.x0 = x0;
        rect.x1 = x1;
        if (k == 0)
        {
            rect.y0 = top + 6;
            rect.y1 = bottom;
        }
        else if (k == 1)
        {
            rect.y0 = bottom - 1;
            rect.y1 = bottom + 7;
        }
        else
        {
            rect.y0 = top;
            rect.y1 = top + 7;
        }
        trRGBTextureMakeCurrent(catCmSupportTexture[tex[k]]);
        if (blended)
        {
            cat_primRectSolidTexturedFullRectBlendC2(&rect, 0xc0500000);
        }
        else
        {
            primRectSolidTextured2(&rect);
        }
    }
}

void cat_cmSUBarDraw(featom *atom, regionhandle region)
{
    rectangle rect;
    Player *player = universe.curPlayerPtr;
    sdword top, height, x0, x1;
    real32 freeShare, queuedShare;

    catCmSUBarRegion = region;
    rect.x0 = region->rect.x0;
    rect.y0 = region->rect.y0;
    rect.x1 = region->rect.x1;
    rect.y1 = (region->rect.y1 - region->rect.y0) / 2 + 1 + region->rect.y0;
    trRGBTextureMakeCurrent(catCmSupportTexture[3]);
    primRectSolidTextured2(&rect);
    rect.x0 = region->rect.x0;
    rect.y0 = (region->rect.y1 - region->rect.y0) / 2 - 1 + region->rect.y0;
    rect.x1 = region->rect.x1;
    rect.y1 = region->rect.y1;
    trRGBTextureMakeCurrent(catCmSupportTexture[4]);
    primRectSolidTextured2(&rect);

    top = region->rect.y0 + 4;
    height = (region->rect.y1 - 4) - top - 14;
    x0 = region->rect.x1 - 20;
    x1 = region->rect.x1 - 4;
    // the x87 divides the unsigned use (a 64-bit fild) by the signed cap
    freeShare = 1.0f - catCmClampUnit((real32)((double)(udword)player->supportUnitsUsed / (double)player->supportUnitsMax));
    catCmSUBarColumn(x0, x1, top, (sdword)((real32)height * freeShare) + top + 6, TRUE);
    queuedShare = catCmClampUnit((real32)((double)(udword)cat_cmQueuedSupportUnits(player) / (double)player->supportUnitsMax));
    catCmSUBarColumn(x0, x1, top, (sdword)((real32)height * catCmClampUnit(freeShare - queuedShare)) + top + 6, FALSE);

    region->status |= RSF_DrawThisFrame;
}



/*-----------------------------------------------------------------------------
    Name        : cmCancelShipType
    Description : Cancels all jobs that refer to a certain shiptype
    Inputs      : type - the shiptype to cancel
    Outputs     : Cancels the build requests of a shiptype
    Return      : void
----------------------------------------------------------------------------*/
void cmForceBuildShipType(ShipType type)
{
//    udword i;

    curshipsInProgress = cmFindFactory(universe.curPlayerPtr->PlayerMothership);

    curshipsInProgress->progress[type].timeLeft = 0;
}






// CAT (M2-R6 managers): a list row the screen shows (every list function of the exe tests the same: a ship row that is
//      not unbuildable, or a printed class header)
static bool32 catCmRowShown(sdword index)
{
    return ((cmShipsAvailable[index].itemtype != ITEM_CLASS) && (cmShipsAvailable[index].itemstat != STAT_CANTBUILD)) ||
           ((cmShipsAvailable[index].itemtype == ITEM_CLASS) && (cmShipsAvailable[index].itemstat == STAT_PRINT));
}

// CAT (M2-R6 managers): the first row of the scrolled list: the (cmUpperIndex + 1)th shown row, else row 0
static sdword catCmFirstShownRow(void)
{
    sdword index, numlines = 0;

    for (index = 0; cmShipsAvailable[index].nJobs != -1; index++)
    {
        if (catCmRowShown(index))
        {
            numlines++;
            if (numlines == cmUpperIndex + 1)
            {
                return index;
            }
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : cmSelectShipType
    Description : Find what type of ship was selected
    Inputs      : reg - region clicked on
                  yClicked - vertical location clicked on.
    Outputs     :
    Return      : type of ship clicked on or -1 if none
    CAT (M2-R6 managers): cmSelectShipType 0x00456430: a shown ship row that is not unbuildable (a type at its cap is
        still picked) and not restricted by the tutorial
----------------------------------------------------------------------------*/
sdword cmSelectShipType(regionhandle region, sdword yClicked)
{
    sdword index, y;
    fonthandle currentFont;
    bool32 newline = FALSE;

    currentFont = fontMakeCurrent(cmDefaultFont);

    y = region->rect.y0 + CM_ASMarginTop;

    for (index = catCmFirstShownRow(); cmShipsAvailable[index].nJobs != -1; index++)
    {
        if (y + fontHeight(" ") >= region->rect.y1)
        {
            break;
        }
        if (catCmRowShown(index))
        {
            newline = TRUE;
        }
        if ((yClicked < y + fontHeight(" ") + CM_ASInterSpacing) && newline)
        {
            if ((cmShipsAvailable[index].itemtype != ITEM_CLASS) && (cmShipsAvailable[index].itemstat != STAT_CANTBUILD))
            {
                if ((tutorial==TUTORIAL_ONLY) && tutIsBuildShipRestricted((sdword)cmShipsAvailable[index].type))
                {
                    return -1;
                }
                return index;
            }
            return -1;
        }
        if (newline)
        {
            newline = FALSE;
            y += fontHeight(" ") + CM_ASInterSpacing;
        }
    }

    fontMakeCurrent(currentFont);

    return (-1);
}

/*-----------------------------------------------------------------------------
    Name        : cmIncrement
    Description : one more job of a row on the screen
    CAT (M2-R6 managers): cmIncrement 0x004565a0: the unit caps (unitCapCanCreateShip), then the support units (the local
        player or multiplayer, a type that uses them: room for its supportUnits and the queue, cat_cmQueuedSupportUnits),
        then the shown factory must be able to build (not missing, switched off or hyperspacing; a module always can);
        a row with jobs already running adds the job to the factory at once (and to the deterministic list in
        multiplayer), else to the screen's queue. A refused job flashes its row. The viewed ship follows the row and the
        list is refreshed. HW1's per-class job cap is gone
----------------------------------------------------------------------------*/
bool32 cmIncrement(shipsinprogress *factory, uword index)
{
    shipinprogress *progress = &factory->progress[cmShipsAvailable[index].type];
    Player *player;
    ShipStaticInfo *info;
    Ship *factoryship;
    bool32 added = FALSE;

    if (unitCapCanCreateShip(cmShipsAvailable[index].type, factory, cmShipsAvailable))
    {
        player = factory->ship->playerowner;
        info = GetShipStaticInfo(cmShipsAvailable[index].type, player->race);
        if (((player->playerIndex == 0) || !singlePlayerGame) && (info->supportUnits != 0) &&
            (((udword)player->supportUnitsMax <= (udword)player->supportUnitsUsed) ||
             ((udword)player->supportUnitsMax - (udword)player->supportUnitsUsed <
              (udword)cat_cmQueuedSupportUnits(player) + info->supportUnits)))
        {
            goto refused;
        }
        factoryship = catCmShownFactoryShip();
        if (((factoryship != NULL) && !bitTest(factoryship->obj.attributes, CAT_ATTRIBUTES_FactoryOff) &&
             !bitTest(factoryship->shipOrDerelict.specialFlags, SPECIAL_Hyperspacing)) ||
            (cmShipsAvailable[index].info->shipclass == CLASS_Component))
        {
            added = TRUE;
            if (progress->nJobsTotal < 1)
            {
                cmShipsAvailable[index].nJobs++;
            }
            else
            {
                progress->nJobsTotal++;
                progress->nJobs++;
                // CAT: also in a recorded multiplayer game (0x00a7519c && 0x00a75190)
                if (catNetworkGame())
                {
                    clWrapDeterministicBuild(CMD_START, &universe.mainCommandLayer, 1, progress->info->shiptype,
                                             universe.curPlayerPtr->race, universe.curPlayerIndex, factory->ship);
                }
            }
            goto done;
        }
    }
refused:
    ShipNumberExceeded = index;
    ExceededCapsFrames = cat_taskRealTimeElapsed;
done:
    svSelectShip(cmShipsAvailable[index].type);
    curshipview = cmShipsAvailable[index].type;
    cmUpdateShipAvailable();
    return added;
}

/*-----------------------------------------------------------------------------
    Name        : cmDecrement
    Description : one job less of a row on the screen
    CAT (M2-R6 managers): cmDecrement 0x00456760: the screen's queue first; else a running batch of more than one job
        loses one (0x4001 to the deterministic list in multiplayer); the viewed ship follows the row, the list is refreshed
        and the result is always TRUE
----------------------------------------------------------------------------*/
bool32 cmDecrement(shipsinprogress *factory, uword index)
{
    shipinprogress *progress = &factory->progress[cmShipsAvailable[index].type];

    if (cmShipsAvailable[index].nJobs < 1)
    {
        if (progress->nJobs > 1)
        {
            progress->nJobs--;
            progress->nJobsTotal--;
            // CAT: also in a recorded multiplayer game (0x00a7519c && 0x00a75190)
            if (catNetworkGame())
            {
                clWrapDeterministicBuild(CMD_START, &universe.mainCommandLayer, 0x4001, progress->info->shiptype,
                                         universe.curPlayerPtr->race, universe.curPlayerIndex, factory->ship);
            }
        }
    }
    else
    {
        cmShipsAvailable[index].nJobs--;
    }
    svSelectShip(cmShipsAvailable[index].type);
    curshipview = cmShipsAvailable[index].type;
    cmUpdateShipAvailable();
    return TRUE;
}










/*-----------------------------------------------------------------------------
    Name        : cmRightArrowProcess
    Description :
    Inputs      : standard region callback
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
sdword cmRightArrowProcess(regionhandle region, sdword ID, udword event, udword data)
{
    shipsinprogress *factory;
    //shipinprogress *progress;

    if (multiPlayerGame)
    {
        if (!multiPlayerGameUnderWay)
        {
            return (0);
        }
    }

    if (event == RPE_PressLeft)
    {
        mgAccTime = taskTimeElapsed - CM_WAIT_ACC;
        return (0);
    }

    if (event == RPE_ReleaseLeft)
    {
        cmArrowIndex = -1;
//        mgAcceleratorRelease();
        return (0);
    }
    //if(!mouseInRect(&region->rect))
    //{
    //    return (0);
    //}

    cmArrowIndex = cmSelectShipType(region, mouseCursorY());

    factory = curshipsInProgress;

    if ( (event == RPE_HoldLeft) && (cmArrowIndex!=-1)  && mgAccelerator() )
    {
        if ((tutorial==TUTORIAL_ONLY) && !tutEnable.bBuildArrows)
            return(0);

        if( (tutorial==TUTORIAL_ONLY) && tutIsBuildShipRestricted(cmShipsAvailable[cmArrowIndex].info->shiptype) )
            return(0);

        if (keyIsHit(CM_FIVEKEY))
        {
            uword j;
            for (j=0; j<5; j++)
            {
                if (cmIncrement(factory,(uword)cmArrowIndex))
                    cmRightArrowActive = TRUE;
                else
                    break;
            }
        }
        else
        {
            if (cmIncrement(factory,(uword)cmArrowIndex))
                cmRightArrowActive = TRUE;
        }

    }
    else if (event == RPE_WheelUp)
    {
        cmWheelNegative();
        scAdjustThumbwheel(g_shandle, cmUpperIndex, cmMaxIndex, (uword)NUMBER_SHIPS_AVAILABLE);
    }
    else if (event == RPE_WheelDown)
    {
        cmWheelPositive();
        scAdjustThumbwheel(g_shandle, cmUpperIndex, cmMaxIndex, (uword)NUMBER_SHIPS_AVAILABLE);
    }
    catCmDirtyRegions();
    return(0);
}


/*-----------------------------------------------------------------------------
    Name        : cmLeftArrowProcess
    Description :
    Inputs      : standard region callback
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
sdword cmLeftArrowProcess(regionhandle region, sdword ID, udword event, udword data)
{
    shipsinprogress *factory;
    //shipinprogress *progress;

    if (multiPlayerGame)
    {
        if (!multiPlayerGameUnderWay)
        {
            return (0);
        }
    }

    if (event == RPE_PressLeft)
    {
        mgAccTime = taskTimeElapsed - CM_WAIT_ACC;
        return (0);
    }

    if (event == RPE_ReleaseLeft)
    {
        cmArrowIndex = -1;
//        mgAcceleratorRelease();
        return (0);
    }
    //if(!mouseInRect(&region->rect))
    //{
    //    return (0);
    //}

    cmArrowIndex = cmSelectShipType(region, mouseCursorY());

    factory = curshipsInProgress;

    if ( (event == RPE_HoldLeft) && (cmArrowIndex!=-1) && mgAccelerator())
    {
        if ((tutorial==TUTORIAL_ONLY) && !tutEnable.bBuildArrows)
            return(0);

        cmLeftArrowActive = TRUE;

        if (keyIsHit(CM_FIVEKEY))
        {
            uword j;
            for (j=0; j<5; j++)
            {
                if (cmDecrement(factory,(uword)cmArrowIndex))
                    cmLeftArrowActive = TRUE;
            }
        }
        else
        {
            if (cmDecrement(factory,(uword)cmArrowIndex))
                cmLeftArrowActive = TRUE;
        }
    }
    else if (event == RPE_WheelUp)
    {
        cmWheelNegative();
        scAdjustThumbwheel(g_shandle, cmUpperIndex, cmMaxIndex, (uword)NUMBER_SHIPS_AVAILABLE);
    }
    else if (event == RPE_WheelDown)
    {
        cmWheelPositive();
        scAdjustThumbwheel(g_shandle, cmUpperIndex, cmMaxIndex, (uword)NUMBER_SHIPS_AVAILABLE);
    }
    catCmDirtyRegions();
    return(0);
}

/*-----------------------------------------------------------------------------
    Name        : cmDrawArrow
    Description : the arrows next to the listed ship rows
    CAT (M2-R6 managers): cmDrawArrow 0x00456ca0: an arrow per shown ship row that is not unbuildable, a filled one for
        the row under the pressed arrow (cleared once drawn); the exe's colours
----------------------------------------------------------------------------*/
void cmDrawArrow(regionhandle region, bool32 leftArrow)
{
    sdword index, y, height;
    fonthandle currentFont;
    triangle tri;
    bool8 active;

    height = region->rect.x1 - region->rect.x0 - 4;
    currentFont = fontMakeCurrent(cmDefaultFont);
    y = region->rect.y0 + CM_ASMarginTop;
    if (leftArrow)
    {
        tri.x0 = region->rect.x1 - 2;
        tri.x1 = region->rect.x0 + 2;
        tri.x2 = tri.x0;
    }
    else
    {
        tri.x0 = region->rect.x0 + 2;
        tri.x1 = tri.x0;
        tri.x2 = region->rect.x1 - 2;
    }
    for (index = catCmFirstShownRow(); cmShipsAvailable[index].nJobs != -1; index++)
    {
        if (y + fontHeight(" ") >= region->rect.y1)
        {
            break;
        }
        if ((cmShipsAvailable[index].itemtype != ITEM_CLASS) && (cmShipsAvailable[index].itemstat != STAT_CANTBUILD))
        {
            tri.y0 = y;
            if (leftArrow)
            {
                tri.y1 = y + height / 2;
                tri.y2 = y + height;
                active = cmLeftArrowActive;
            }
            else
            {
                tri.y1 = y + height;
                tri.y2 = y + height / 2;
                active = cmRightArrowActive;
            }
            if (active && (index == cmArrowIndex))
            {
                cmLeftArrowActive = FALSE;
                cmRightArrowActive = FALSE;
                cmArrowIndex = -1;
                primTriSolid2(&tri, colRGB(200, 0, 0));
            }
            primTriOutline2(&tri, 1, colRGB(200, 200, 0));
            y += fontHeight(" ") + CM_ASInterSpacing;
        }
        else if (catCmRowShown(index))
        {
            y += fontHeight(" ") + CM_ASInterSpacing;
        }
    }
    fontMakeCurrent(currentFont);
}

void cmLeftArrowDraw(featom *atom, regionhandle region)
{
    if (region->flags == 0 || region->flags == RPE_DrawFunctionAdded)
    {                                                       //if region not processed yet
        region->flags = RPE_PressLeft | RPE_ReleaseLeft | RPE_HoldLeft |
                        RPE_WheelUp | RPE_WheelDown;        //receive mouse presses from now on
        regFunctionSet(region, (regionfunction)cmLeftArrowProcess);         //set new region handler function
        regSiblingMoveToFront(region);                      // CAT: cmLeftArrowDraw 0x00456e80
    }
    cmDrawArrow(region, TRUE);
}

void cmRightArrowDraw(featom *atom, regionhandle region)
{
    if (region->flags == 0 || region->flags == RPE_DrawFunctionAdded)
    {                                                       //if region not processed yet
        region->flags = RPE_PressLeft | RPE_ReleaseLeft | RPE_HoldLeft |
                        RPE_WheelUp | RPE_WheelDown;        //receive mouse presses from now on
        regFunctionSet(region, (regionfunction)cmRightArrowProcess);        //set new region handler function
        regSiblingMoveToFront(region);                      // CAT: cmRightArrowDraw 0x00456ec0
    }
    cmDrawArrow(region, FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : cmShipNumbersDraw
    Description : the number of ships queued and building per row
    CAT (M2-R6 managers): cmShipNumbersDraw 0x00456f00: the exe's background and left line, the grey of an unavailable
        row, the queue plus the running jobs right-aligned 12 pixels in
----------------------------------------------------------------------------*/
void cmShipNumbersDraw(featom *atom, regionhandle region)
{
    sdword index, y, number;
    fonthandle currentFont;
    shipsinprogress *factory = curshipsInProgress;
    color c;

    currentFont = fontCurrentGet();
    fontMakeCurrent(cmDefaultFont);
    feStaticRectangleDraw(region);
    primRectSolid2(&region->rect, CM_BackgroundColor);
    primLine2(region->rect.x0, region->rect.y0, region->rect.x0, region->rect.y1, colRGB(0, 100, 160));
    cmShipNumbersRegion = region;

    y = region->rect.y0 + CM_ASMarginTop;
    for (index = catCmFirstShownRow(); cmShipsAvailable[index].nJobs != -1; index++)
    {
        if (y + fontHeight(" ") >= region->rect.y1)
        {
            break;
        }
        if ((cmShipsAvailable[index].itemtype != ITEM_CLASS) && (cmShipsAvailable[index].itemstat != STAT_CANTBUILD))
        {
            if (cmShipsAvailable[index].itemstat == CAT_STAT_UNAVAILABLE)
            {
                c = catCmUnavailableTextColor;
            }
            else
            {
                c = (cmShipsAvailable[index].nJobs > 0) ? cmSelectionTextColor : cmStandardTextColor;
            }
            number = factory->progress[cmShipsAvailable[index].type].nJobs;
            if (number > 0)
            {
                c = cmBuildingTextColor;
            }
            number += cmShipsAvailable[index].nJobs;
            fontPrintf(region->rect.x1 - fontWidthf("%d", number) - 12, y, c, "%d", number);
            y += fontHeight(" ") + CM_ASInterSpacing;
        }
        else if (catCmRowShown(index))
        {
            y += fontHeight(" ") + CM_ASInterSpacing;
        }
    }
    fontMakeCurrent(currentFont);
}

/*-----------------------------------------------------------------------------
    Name        : cmShipCostsDraw
    Description : Draw the cost of building selected ships.
    Inputs      : feflow callback
    Outputs     : ..
    Return      : void
    CAT (M2-R6 managers): the exe's body (0x00456210, the CM_ShipCost callback, no Ghidra function): the background and
        left line of cmShipNumbersDraw, each listed ship row's build cost (divided by the single-player cost divisor),
        right-aligned 7 pixels in, in the row's colour
----------------------------------------------------------------------------*/
void cmShipCostsDraw(featom *atom, regionhandle region)
{
    sdword index, y, cost;
    fonthandle currentFont;
    shipsinprogress *factory = curshipsInProgress;
    color c;

    currentFont = fontCurrentGet();
    fontMakeCurrent(cmDefaultFont);
    feStaticRectangleDraw(region);
    primRectSolid2(&region->rect, CM_BackgroundColor);
    primLine2(region->rect.x0, region->rect.y0, region->rect.x0, region->rect.y1, colRGB(0, 100, 160));
    cmShipCostsRegion = region;

    y = region->rect.y0 + CM_ASMarginTop;
    for (index = catCmFirstShownRow(); cmShipsAvailable[index].nJobs != -1; index++)
    {
        if (y + fontHeight(" ") >= region->rect.y1)
        {
            break;
        }
        if ((cmShipsAvailable[index].itemtype != ITEM_CLASS) && (cmShipsAvailable[index].itemstat != STAT_CANTBUILD))
        {
            if (cmShipsAvailable[index].itemstat == CAT_STAT_UNAVAILABLE)
            {
                c = catCmUnavailableTextColor;
            }
            else
            {
                c = (cmShipsAvailable[index].nJobs > 0) ? cmSelectionTextColor : cmStandardTextColor;
            }
            if (factory->progress[cmShipsAvailable[index].type].nJobs > 0)
            {
                c = cmBuildingTextColor;
            }
            cost = singlePlayerGame ? cmShipsAvailable[index].info->buildCost / catCmSinglePlayerCostFactor :
                                      cmShipsAvailable[index].info->buildCost;
            fontPrintf(region->rect.x1 - fontWidthf("%d", cost) - 7, y, c, "%d", cost);
            y += fontHeight(" ") + CM_ASInterSpacing;
        }
        else if (catCmRowShown(index))
        {
            y += fontHeight(" ") + CM_ASInterSpacing;
        }
    }
    fontMakeCurrent(currentFont);
}

/*-----------------------------------------------------------------------------
    Name        : cmShipInfoDraw
    Description : Draw the available ships and construction progress
    CAT (M2-R6 managers): the exe's body (0x004573d0): the class header line ("SuperCapital" for the destroyer class),
        the grey of an unavailable row, the selection box, the two
        progress bars in the exe's colours 0xfff9a891 / 0xffef5325 (a job whose clock is at 0 shows 100%), a paused
        job's green, the refused row's flash; the first row of the scroll counts as shown whatever it is (the exe's
        loop does that)
----------------------------------------------------------------------------*/
void cmShipInfoDraw(featom *atom, regionhandle region)
{
    sdword y, index, percent, numlines = 0, startind = 0;
    rectangle rect = region->rect;
    color c;
    shipsinprogress *factory = curshipsInProgress;
    shipinprogress  *progress;
    fonthandle currentFont;
    bool32 first = TRUE;
    char *classString, *classWord;

    if (cmRenderEverythingCounter > 0)
    {
        cmRenderEverythingCounter--;
        feRenderEverything = TRUE;
    }

    cmShipInfoRegion = region;

    currentFont = fontMakeCurrent(cmShipListFont);

    if (region->flags == 0 || region->flags == RPE_DrawFunctionAdded)
    {                                                       //if region not processed yet
        region->flags = RPE_PressLeft | RPE_PressRight |
                        RPE_WheelUp | RPE_WheelDown;        //receive mouse presses from now on
        regFunctionSet(region, (regionfunction)cmSelectAvailable);          //set new region handler function
        regFilterSet(region, region->flags | RPE_DoubleLeft | RPE_DoubleRight);
    }
    feStaticRectangleDraw(region);                          //draw standard rectangle

    rect.x1 -= 92;                                          // CAT: 0x5c (HW1 CM_ProgressMarginRight 90)
    primRectSolid2(&rect, CM_BackgroundColor);

    for (index = 0; cmShipsAvailable[index].nJobs != -1; index++)
    {
        if (catCmRowShown(index) || first)
        {
            first = FALSE;
            numlines++;
            if (numlines == cmUpperIndex + 1)
            {
                startind = index;
                break;
            }
        }
    }

    y = region->rect.y0 + CM_ASMarginTop;
    for (index = startind; cmShipsAvailable[index].nJobs != -1; index++)
    {
        fontShadowSet(FS_NONE, colBlack);
        if (y + fontHeight(" ") >= region->rect.y1)
        {
            break;
        }
        if ((cmShipsAvailable[index].itemtype != ITEM_CLASS) && (cmShipsAvailable[index].itemstat != STAT_CANTBUILD))
        {
            progress = &factory->progress[cmShipsAvailable[index].type];
            if (cmShipsAvailable[index].itemstat == CAT_STAT_UNAVAILABLE)
            {
                c = catCmUnavailableTextColor;
            }
            else if (cmShipsAvailable[index].nJobs > 0)
            {                                                   //if this ship already selected
                c = cmSelectionTextColor;
            }
            else
            {
                c = progress->selected ? FEC_ListItemSelected : cmStandardTextColor;
            }
            if (progress->selected)
            {
                rect.x0 = region->rect.x0 + CM_SelectBoxX0 + cmSelectLeftAdjust;
                rect.y0 = y + CM_SelectBoxY0 + cmSelectTopAdjust;
                rect.x1 = region->rect.x0 + CM_SelectBoxX1 + cmSelectRightAdjust;
                rect.y1 = y + CM_SelectBoxY1 + cmSelectBottomAdjust;
                primRectTranslucent2(&rect, FEC_ListItemTranslucent);
                primRectOutline2(&rect, 1, FEC_ListItemTranslucentBorder);
            }
            if (progress->nJobs > 0)
            {                                                   //if working on this class
                c = progress->paused ? CM_PausedColor : cmBuildingTextColor;
                if ((progress->timeLeft == 0) || (progress->timeStart == 0))
                {
                    percent = 100;
                }
                else
                {
                    percent = 100 - progress->timeLeft * 100 / progress->timeStart;
                }
                rect.x0 = region->rect.x0 + 2 + cmSelectLeftAdjust; //increment past border of window
                rect.y0 = y + CM_SelectBoxY0 + cmSelectTopAdjust;   //draw top bar (progress of current ship)
                rect.y1 = y + fontHeight(" ") / 2 + cmSelectTopAdjust;
                rect.x1 = rect.x0 - 1 + cmSelectRightAdjust + (region->rect.x1 - region->rect.x0 - 94) * percent / 100;   // CAT: 0x5e
                primRectSolid2(&rect, 0xfff9a891);

                if (progress->nJobsTotal * progress->timeStart > 0)
                {
                    percent = 100 - (progress->timeLeft + (progress->nJobs - 1) * progress->timeStart) * 100 /
                                    (progress->timeStart * progress->nJobsTotal);
                }
                rect.y0 = rect.y1;                               //draw bottom bar (progress of total job)
                rect.y1 = y + CM_SelectBoxY1 + cmSelectBottomAdjust - 1;
                rect.x1 = rect.x0 - 1 + cmSelectRightAdjust + (region->rect.x1 - region->rect.x0 - 94) * percent / 100;
                primRectSolid2(&rect, 0xffef5325);
                fontShadowSet(FS_SE, colBlack);
            }
            if ((cat_taskRealTimeElapsed - ExceededCapsFrames < FLASH_TIMER) && (ShipNumberExceeded == index))
            {
                c = FEC_ListItemInvalid;
            }
            fontPrint(region->rect.x0 + 7, y, c, ShipTypeToNiceStr(cmShipsAvailable[index].type));   // CAT: 7 (HW1 9)
            catCmDirtyRegions();
            y += fontHeight(" ") + CM_ASInterSpacing;
        }
        else if ((cmShipsAvailable[index].itemtype == ITEM_CLASS) && (cmShipsAvailable[index].itemstat == STAT_PRINT))
        {
            if (cmShipsAvailable[index].type == CLASS_Destroyer)
            {
                classString = strGetString(strCLASS_SuperCapital);
            }
            else
            {
                classString = ShipClassToNiceStr(cmShipsAvailable[index].type);
            }
            classWord = strGetString(strCLASS_Class);
            // CAT: the exe prints "%s %s" in the other order when g_bNeedReverseWordsForOtherLanguage (0x00a6c538) is
            //      set; nothing in the exe writes it, so the class name comes first as here
            fontPrintf(region->rect.x0 + CM_ASHeadingMarginLeft, y, cmClassHeadingTextColor, "%s %s", classString, classWord);
            y += fontHeight(" ") + CM_ASInterSpacing;
        }
    }

    fontMakeCurrent(currentFont);
}

/*-----------------------------------------------------------------------------
    Name        : cmTotalRUsDraw
    Description : Callback to draw total cost of build queue for all ship types
    CAT (M2-R6 managers): cmTotalRUsDraw 0x00457a00 (IDENTICAL in the map, found modified): the costs divided by the
        single-player cost divisor and the cost already paid of a running job by the single-player build-time divisor;
        a running job's remaining cost of every other factory of the local player counts too
----------------------------------------------------------------------------*/
static sdword catCmJobRemainingCost(shipinprogress *progress)
{
    sdword cost = singlePlayerGame ? progress->info->buildCost / catCmSinglePlayerCostFactor : progress->info->buildCost;
    sdword buildTime = singlePlayerGame ? progress->info->buildTime / catCmSinglePlayerBuildFactor : progress->info->buildTime;
    real32 already = (real32)(progress->timeStart - progress->timeLeft) * (real32)cost / (real32)buildTime;

    return progress->nJobs * cost - (sdword)already;
}

void cmTotalRUsDraw(featom *atom, regionhandle region)
{
    sdword width;
    sdword index, RUs;
    shipsinprogress *factory, *other;
    shipinprogress *progress;
    rectangle rect = region->rect;
    fonthandle oldfont;
    Node *node;

    cmTotalRUsRegion = region;

    oldfont = fontMakeCurrent(cmShipListFont);

    factory = curshipsInProgress;

    primModeSet2();
    primRectSolid2(&rect, CM_BackgroundColor);

    glEnable(GL_SCISSOR_TEST);
    uiScaleScissor(rect.x0, MAIN_WindowHeight - rect.y1, rect.x1 - rect.x0, rect.y1 - rect.y0);   // HWI-FEATURE(ui-scale): physical pixels
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    for (index = RUs = 0; cmShipsAvailable[index].nJobs != -1; index++)
    {
        if (cmShipsAvailable[index].itemtype==ITEM_CLASS)
        {
            continue;
        }
        progress = &factory->progress[cmShipsAvailable[index].type];
        RUs += (singlePlayerGame ? cmShipsAvailable[index].info->buildCost / catCmSinglePlayerCostFactor :
                                   cmShipsAvailable[index].info->buildCost) * cmShipsAvailable[index].nJobs;
        if (progress->nJobs > 0)
        {
            RUs += catCmJobRemainingCost(progress);
        }
    }
    for (node = listofShipsInProgress.head; node != NULL; node = node->next)
    {
        other = (shipsinprogress *)listGetStructOfNode(node);
        if ((other->ship == factory->ship) || (other->ship->playerowner != universe.curPlayerPtr))
        {
            continue;
        }
        for (index = 0, progress = &other->progress[0]; index < TOTAL_NUM_SHIPS; index++, progress++)
        {
            if (progress->nJobs > 0)
            {
                RUs += catCmJobRemainingCost(progress);
            }
        }
    }
    width = fontWidthf("%d", RUs);                          //width of number
    feStaticRectangleDraw(region);                          //draw regular rectangle as backdrop
    fontPrintf(region->rect.x1 - width - CM_RUMarginRight,
               (region->rect.y1 - region->rect.y0 - fontHeight(NULL)) / 2 + region->rect.y0,
               atom->borderColor, "%d", RUs);
    fontMakeCurrent(oldfont);
}







/*-----------------------------------------------------------------------------
    Name        : cmSelectAvailable
    Description : Callback for selecting/deselecting ships
    Inputs      : standard region callback
    Outputs     : selects or deselects a ship for construction
    Return      :
----------------------------------------------------------------------------*/
sdword cmSelectAvailable(regionhandle region, sdword ID, udword event, udword data)
{
    sdword index;
    shipsinprogress *factory;
    shipinprogress *progress, *kprogress;

    if (multiPlayerGame)
    {
        if (!multiPlayerGameUnderWay)
        {
            return (0);
        }
    }

    index = cmSelectShipType(region, mouseCursorY());

    factory = curshipsInProgress;

    if ( (event == RPE_PressLeft || event == RPE_DoubleLeft) && (index!=-1) )
    {
        progress = &factory->progress[cmShipsAvailable[index].info->shiptype];

        if (progress->selected && !keyIsHit(SHIFTKEY))
        {
                        //left press (select/add job)
            if((tutorial==TUTORIAL_ONLY) && tutIsBuildShipRestricted(cmShipsAvailable[index].info->shiptype) )
                return(0);

            if (keyIsHit(CM_FIVEKEY))
            {
                uword j;
                for (j=0; j<5; j++)
                {
                    if (cmIncrement(factory,(uword)index))
                        cmRightArrowActive = TRUE;
                }
            }
            else
            {
                if (cmIncrement(factory,(uword)index))
                    cmRightArrowActive = TRUE;
            }
        }
        else
        {
            if (keyIsHit(SHIFTKEY))
            {
                progress->selected = !progress->selected;
            }
            else
            {
                udword i;

                kprogress = progress;
                progress = &curshipsInProgress->progress[0];
                for (i = 0; i < TOTAL_NUM_SHIPS; i++, progress++)
                {                                                   //for all classes of ships
                    progress->selected = FALSE;
                }

                kprogress->selected = TRUE;

                /*
                for (i=0; cmShipsAvailable[i].nJobs!=-1; i++)
                {

                    sip = cmSIP(index);
                    if (i == index)
                        sip->selected = TRUE;
                    else
                        sip->selected = FALSE;*/
            }
        }

        svSelectShip(cmShipsAvailable[index].info->shiptype);
        curshipview = cmShipsAvailable[index].info->shiptype;
    }
    else if ( (event == RPE_PressRight || event == RPE_DoubleRight) && (index!=-1) )
    {
        progress = &factory->progress[cmShipsAvailable[index].info->shiptype];

        //cmDecrement(factory, (uword)index);
        if (keyIsHit(CM_FIVEKEY))
        {
            uword j;
            for (j=0; j<5; j++)
            {
                if (cmDecrement(factory,(uword)index))
                    cmLeftArrowActive = TRUE;
            }
        }
        else
        {
            if (cmDecrement(factory,(uword)index))
                cmLeftArrowActive = TRUE;

            if (keyIsHit(SHIFTKEY))
            {
                progress->selected = !progress->selected;
            }
            else
            {
                udword i;

                kprogress = progress;
                progress = &curshipsInProgress->progress[0];
                for (i = 0; i < TOTAL_NUM_SHIPS; i++, progress++)
                {                                                   //for all classes of ships
                    progress->selected = FALSE;
                }

                kprogress->selected = TRUE;
            }
        }

        svSelectShip(cmShipsAvailable[index].info->shiptype);
        curshipview = cmShipsAvailable[index].info->shiptype;
    }
    else if (event == RPE_WheelUp)
    {
        cmWheelNegative();
        scAdjustThumbwheel(g_shandle, cmUpperIndex, cmMaxIndex, (uword)NUMBER_SHIPS_AVAILABLE);
    }
    else if (event == RPE_WheelDown)
    {
        cmWheelPositive();
        scAdjustThumbwheel(g_shandle, cmUpperIndex, cmMaxIndex, (uword)NUMBER_SHIPS_AVAILABLE);
    }
    catCmDirtyRegions();
    return(0);
}


/*-----------------------------------------------------------------------------
    Name        : cmNumberRUsDraw
    Description : Callback to draw number of RU's available
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void cmNumberRUsDraw(featom *atom, regionhandle region)
{
    sdword width;
    fonthandle oldfont;
    rectangle rect = region->rect;

    cmNumberRUsRegion = region;

    oldfont = fontMakeCurrent(cmShipListFont);

    primModeSet2();
    primRectSolid2(&rect, CM_BackgroundColor);

    glEnable(GL_SCISSOR_TEST);
    uiScaleScissor(rect.x0, MAIN_WindowHeight - rect.y1, rect.x1 - rect.x0, rect.y1 - rect.y0);   // HWI-FEATURE(ui-scale): physical pixels
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    width = fontWidthf("%d", universe.curPlayerPtr->resourceUnits);//width of number
    feStaticRectangleDraw(region);                          //draw regular rectangle as backdrop
    fontPrintf(region->rect.x1 - width - CM_RUMarginRight,
               (region->rect.y1 - region->rect.y0 - fontHeight(NULL)) / 2 + region->rect.y0,
               atom->borderColor, "%d", universe.curPlayerPtr->resourceUnits);

    fontMakeCurrent(oldfont);
}


/*-----------------------------------------------------------------------------
    Name        : cmTotalShipsDraw
    Description : Callback to draw total number of ships queued
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void cmTotalShipsDraw(featom *atom, regionhandle region)
{
    sdword width;
    sdword index, nShips;
    shipsinprogress *factory;
    shipinprogress *progress;
    rectangle rect = region->rect;
    fonthandle oldfont;

    cmTotalShipsRegion = region;

    oldfont = fontMakeCurrent(cmShipListFont);

    factory = curshipsInProgress;

    primModeSet2();
    primRectSolid2(&rect, CM_BackgroundColor);

    glEnable(GL_SCISSOR_TEST);
    uiScaleScissor(rect.x0, MAIN_WindowHeight - rect.y1, rect.x1 - rect.x0, rect.y1 - rect.y0);   // HWI-FEATURE(ui-scale): physical pixels
    glClear(GL_DEPTH_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    for (index = nShips = 0; cmShipsAvailable[index].nJobs != -1; index++)
    {
        if (cmShipsAvailable[index].itemtype != ITEM_CLASS)    // CAT: kind != 0 (cmTotalShipsDraw 0x00457d00)
        {
            progress = &factory->progress[cmShipsAvailable[index].type];
            nShips += cmShipsAvailable[index].nJobs;
            nShips += progress->nJobs;                          //active jobs, too
        }
    }
    width = fontWidthf("%d", nShips);                       //width of number
    feStaticRectangleDraw(region);                          //draw regular rectangle as backdrop
    fontPrintf(region->rect.x1 - width - CM_RUMarginRight,
               (region->rect.y1 - region->rect.y0 - fontHeight(NULL)) / 2 + region->rect.y0,
               atom->borderColor, "%d", nShips);
    fontMakeCurrent(oldfont);
}

// CAT (M2-R6 managers): the construction list of a factory (cmUpdateFactory 0x00458180, cmUpdateFactoryNoReset
//      0x00457fc0, cmCanBuildShipType 0x00457e50 choose it inline): by the owner's race (cmUpdate*) or the factory's
//      race (cmCanBuildShipType), canBuildBigShips picking the big list; NULL for no list
static shipavailable *catCmShipsAvailableFor(ShipRace race, bool32 big)
{
    switch (race)
    {
        case RACE_Sect:     return big ? catCmShipsAvailableSectBig : catCmShipsAvailableSect;
        case RACE_Beast:    return big ? catCmShipsAvailableBeastBig : catCmShipsAvailableBeast;
        case RACE_Kushan:   return big ? catCmShipsAvailableKushanBig : catCmShipsAvailableKushan;
        case RACE_Taiidan:  return big ? catCmShipsAvailableTaiidanBig : catCmShipsAvailableTaiidan;
        case RACE_P1:       return catCmShipsAvailableP1;
        default:            return NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cmCanBuildShipType
    Description : can this factory build this type
    CAT (M2-R6 managers): the exe's body (0x00457e50): the list of the factory's own race (canBuildBigShips of its
        static); an externally built type only at a mothership-class factory with a free build slot
        (cat_dockHasFreeSlotForShip); with checkResearch the research (rmCanBuildShip, its third argument 1 when the
        screen's factory is the sMothership) and a kind-2 row's cat_playerShipTypeEnabled
----------------------------------------------------------------------------*/
bool32 cmCanBuildShipType(Ship *factoryship,ShipType shiptype,bool32 checkResearch)
{
    shipavailable *shipsavail;
    ShipStaticInfo *info;
    sdword index;
    bool32 bigFactory;

    if (factoryship == NULL)
    {
        return FALSE;
    }
    shipsavail = catCmShipsAvailableFor(factoryship->staticinfo->shiprace, factoryship->staticinfo->canBuildBigShips);
    if (shipsavail == NULL)
    {
        return FALSE;
    }
    for (index = 0; shipsavail[index].nJobs != -1; index++)
    {
        if ((shipsavail[index].itemtype == ITEM_CLASS) || (shipsavail[index].type != shiptype))
        {
            continue;
        }
        info = GetShipStaticInfo(shiptype, factoryship->staticinfo->shiprace);
        if (info->externalBuild)
        {
            if (factoryship->staticinfo->shipclass != CLASS_Mothership)
            {
                return FALSE;
            }
            if (!cat_dockHasFreeSlotForShip(factoryship, info->shipclass, shiptype, TRUE, 0))
            {
                return FALSE;
            }
        }
        if (checkResearch)
        {
            bigFactory = (curshipsInProgress != NULL) && (curshipsInProgress->ship->shiptype == sMothership);
            if (!rmCanBuildShip(factoryship->playerowner, shiptype, bigFactory))
            {
                return FALSE;
            }
            if ((shipsavail[index].itemtype == CAT_ITEM_SHIPENABLED) &&
                !cat_playerShipTypeEnabled(factoryship->playerowner, shiptype))
            {
                return FALSE;
            }
        }
        return TRUE;
    }
    return FALSE;
}

// CAT (M2-R6 managers): the common part of cmUpdateFactory and cmUpdateFactoryNoReset: the factory (any ship that is no
//      factory falls back to the local player's mothership; a non-builder fails), its entry and its list
static Ship *catCmFactorySelect(Ship *shipfactory)
{
    if ((shipfactory == NULL) || !shipfactory->staticinfo->canBuildShips)
    {
        shipfactory = universe.curPlayerPtr->PlayerMothership;
    }
    if ((shipfactory == NULL) || !shipfactory->staticinfo->canBuildShips)
    {
        return NULL;
    }
    curshipsInProgress = cmFindFactory(shipfactory);
    // CAT-FIX: the exe reads the canBuildBigShips of a NULL entry when the ship is not in the factory list
    cmShipsAvailable = catCmShipsAvailableFor(shipfactory->playerowner->race,
                                              (curshipsInProgress != NULL) && curshipsInProgress->canBuildBigShips);
    return shipfactory;
}

/*-----------------------------------------------------------------------------
    Name        : cmUpdateFactoryNoReset
    Description : show a factory without resetting the screen's queue
    CAT (M2-R6 managers): the exe's body (0x00457fc0)
----------------------------------------------------------------------------*/
sdword cmUpdateFactoryNoReset(Ship* shipfactory)
{
    if ((catCmFactorySelect(shipfactory) == NULL) || (cmShipsAvailable == NULL))
    {
        return 0;
    }
    catCmDirtyRegions();
    cmUpdateShipAvailable();
    return 1;
}

void cmUpdateShipsAvailable(void)
{
    // CAT: the shown factory's slot (cmUpdateShipsAvailable 0x00458150)
    cmUpdateFactoryNoReset(catCmShownFactoryShip());
}

/*-----------------------------------------------------------------------------
    Name        : cmUpdateFactory
    Description : show a factory: its list with an empty queue
    CAT (M2-R6 managers): the exe's body (0x00458180): the queue cleared, every row's static, the entries' selection
        cleared (one per list row, from type 0: the exe's loop)
----------------------------------------------------------------------------*/
sdword cmUpdateFactory(Ship* shipfactory)
{
    sdword index;

    if ((catCmFactorySelect(shipfactory) == NULL) || (cmShipsAvailable == NULL))
    {
        return 0;
    }
    for (index = 0; cmShipsAvailable[index].nJobs != -1; index++)
    {
        cmShipsAvailable[index].nJobs = 0;
        if (cmShipsAvailable[index].itemtype != ITEM_CLASS)
        {
            cmShipsAvailable[index].info = GetShipStaticInfo(cmShipsAvailable[index].type, universe.curPlayerPtr->race);
        }
    }
    if (curshipsInProgress != NULL)
    {
        for (index = 0; cmShipsAvailable[index].nJobs != -1; index++)
        {
            curshipsInProgress->progress[index].selected = FALSE;
        }
    }
    catCmDirtyRegions();
    cmUpdateShipAvailable();
    return 1;
}

/*-----------------------------------------------------------------------------
    Name        : cmConstructionBegin
    Description : open the construction manager
    CAT (M2-R6 managers): the exe's body (0x00458390): not during a playback or while the game is paused (0x00a6c5b4,
        the console / game-stats pause) or in a tutorial without its build manager bit; only with a mothership and, in
        single player, for player 0; an open sensors manager is closed for good and reopened by cmClose; the factory is
        the clicked ship when it is the player's own non-exempt mothership- or carrier-class ship, else the slot shown
        last (the mothership; a carrier-class mothership is slot 1); the Beast player gets its own screen file
        (FEMan\\Beast_Construction_Manager.fib, screen "Beast_Construction_manager"); the sounds 0x12f / 0x104; the
        main view stops drawing. No info overlay save, no taskbar flag
----------------------------------------------------------------------------*/
sdword cmConstructionBegin(regionhandle region, smemsize ID, udword event, udword data)
{
    sdword status;
    Ship *bship = (Ship *)ID;
    Player *player = universe.curPlayerPtr;
    sdword i;

    if (playPackets || universePause || !catCmTutEnabled(0x4))
    {
        return 0;
    }
    if ((player->PlayerMothership == NULL) || (singlePlayerGame && (player->playerIndex != 0)))
    {
        return 0;
    }
    dbgMessagef("speechlog: build manager opens tick=%u sensors=%d", (unsigned)universe.univUpdateCounter, (int)smSensorsActive);     // HWI-TEST (M3-R4 speech-log)
    catCmSensorsWereOpen = smSensorsActive;
    if (smSensorsActive)
    {
        smSensorsCloseForGood();
    }
    cmFillInCarrierXs();

    if ((bship != NULL) && (bship->playerowner->playerIndex == player->playerIndex) &&
        !bitTest(bship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20) &&
        ((bship->staticinfo->shipclass == CLASS_Mothership) || (bship->staticinfo->shipclass == CLASS_Carrier)))
    {
        if (bship->staticinfo->shipclass == CLASS_Mothership)
        {
            cmCurrentSelect = 0;
        }
        else
        {
            for (i = 0; i < NUM_CMCARRIERS; i++)
            {
                if (bship == cmCarrierX[i])
                {
                    cmCurrentSelect = i + 1;
                    break;
                }
            }
        }
    }
    else
    {
        bship = NULL;
        if (cmCurrentSelect != 0)
        {
            bship = cmCarrierX[cmCurrentSelect - 1];
        }
        if (bship == NULL)
        {
            bship = player->PlayerMothership;
            if (bship->staticinfo->shipclass == CLASS_Carrier)
            {
                cmCurrentSelect = 1;
            }
            else if (bship->staticinfo->shipclass == CLASS_Mothership)
            {
                cmCurrentSelect = 0;
            }
        }
    }
    if (bship == NULL)
    {
        return 0;
    }

    tutGameMessage("Start_BuildManager");

    status = cmUpdateFactory(bship);
    if (status == 0)
    {
        return 0;
    }

    // disable rendering of main screen
    mrRenderMainScreen = FALSE;

    cmRenderEverythingCounter = (tutorial == TUTORIAL_ONLY) ? 4 : 0;

    // clear the screen
    rndClear();

    spLockout(SPLOCKOUT_MR);

    if (piePointSpecMode != PSM_Idle)
    {
        piePointModeOnOff(PIE_SPEC_None, NULL);   // CAT: off, mode 0 as the exe
    }

    if (cmScreensHandle == NULL)
    {
        feCallbackAddMultiple(cmCallback);                  //add in the callbacks
        feDrawCallbackAddMultiple(cmDrawCallback);
    }
    else if (universe.curPlayerPtr->race != catCmScreensRace)
    {
        feScreensDelete(cmScreensHandle);
        cmScreensHandle = NULL;
    }
    if (cmScreensHandle == NULL)
    {
        catCmScreensRace = (universe.curPlayerPtr->race == RACE_Sect) ? RACE_Sect : RACE_Beast;
        cmScreensHandle = feScreensLoad((catCmScreensRace == RACE_Sect) ? CM_FIBFile : CAT_CM_BeastFIBFile);
    }

    soundEventStopSFX(0.5f);
    soundEventPlay(NULL, 0x12f, NULL);                      // CAT: the manager intro
    soundEventPlay(NULL, 0x104, NULL);                      // CAT: the construction manager's ambient

    cmBaseRegion = feScreenStart(region, (universe.curPlayerPtr->race == RACE_Sect) ? CM_ConstructionScreen : CAT_CM_BeastConstructionScreen);

    cmArrowIndex = -1;

    cmLeftArrowActive  = FALSE;
    cmRightArrowActive = FALSE;

    mouseCursorShow();

    opKeyDetour = 0;
    cmPrintHotKey = FALSE;

    updateShipView();

    cmActive = TRUE;
    ExceededCapsFrames = 0.0f;
    ShipNumberExceeded = 0;

    cat_tutButtonsHideForScreen();           // CAT: the tutorial's buttons hide while the screen is open (cmConstructionBegin 0x004586a6)

    return(status);
}

/*-----------------------------------------------------------------------------
    Name        : cmSelectMotherShip / cmSelectCarrier1 / cmSelectCarrier2
    Description : the factory buttons
    CAT (M2-R6 managers): the exe's bodies (0x00458780 / 0x00458800 / 0x00458890, region callbacks with no Ghidra
        function): a left press on another slot shows that factory (cmUpdateFactory), redraws the button and speaks the
        factory's name (0x400101c with the race's mothership or carrier type by whichRaceSelected), then the viewed ship
----------------------------------------------------------------------------*/
static void catCmFactorySpeech(ShipType sectType, ShipType beastType)
{
    speechEventQueue(NULL, 0x400101c, (whichRaceSelected == 0) ? sectType : beastType, 0, 0, universe.curPlayerIndex, -1, 5.0f, -1);
}

sdword cmSelectMotherShip(regionhandle region, sdword ID, udword event, udword data)
{
    if ((event == RPE_PressLeft) && (cmCurrentSelect != 0))
    {
        cmCurrentSelect = 0;
        cmUpdateFactory(universe.curPlayerPtr->PlayerMothership);
        region->status |= RSF_DrawThisFrame;
        catCmFactorySpeech(sMothership, bMothership);
        updateShipView();
    }
    return 0;
}

sdword cmSelectCarrierX(regionhandle region, sdword ID, udword event, udword data, udword x)
{
    if ((event == RPE_PressLeft) && (cmCarrierX[x - 1] != NULL) && (cmCurrentSelect != (sdword)x))
    {
        cmCurrentSelect = x;
        cmUpdateFactory(cmCarrierX[x - 1]);
        region->status |= RSF_DrawThisFrame;
        catCmFactorySpeech(sCarrier, bCarrier);
        updateShipView();
    }
    return 0;
}

sdword cmSelectCarrier1(regionhandle region, sdword ID, udword event, udword data)
{
    return cmSelectCarrierX(region, ID, event, data, 1);
}

sdword cmSelectCarrier2(regionhandle region, sdword ID, udword event, udword data)
{
    return cmSelectCarrierX(region, ID, event, data, 2);
}

/*-----------------------------------------------------------------------------
    Name        : cmDrawShipImage
    Description : a factory button's picture
    CAT (M2-R6 managers): the exe's body (0x004589c0) with its third argument: the icon is chosen by slot (0 the
        mothership's icon1, else icon2) and state (disabled for an empty slot; off; on for the shown factory); the focus
        window; the texture (no glcompat path in the SDL build); the shown factory's
        button gets a 2-pixel line up to the name region (cmFactoryNameRegion) and the screen is redrawn whole (RSF_ReallyDirty)
----------------------------------------------------------------------------*/
void cmDrawShipImage(regionhandle region, sdword shipID, bool32 present)
{
    rectangle rect;
    sdword index = (shipID != 0) ? 3 : 0;
    lifheader *image;
    udword texture;
    sdword race = (universe.curPlayerPtr->race == RACE_Sect) ? 0 : 1;

    rect.x0 = region->rect.x0 + 2;
    rect.y0 = region->rect.y0 + 2;
    rect.x1 = region->rect.x1 - 2;
    rect.y1 = region->rect.y1 - 2;

    if (!present)
    {
        ferDrawFocusWindow(region, lw_normal);
    }
    else if (shipID == cmCurrentSelect)
    {
        ferDrawFocusWindow(region, lw_focus);
        index += 2;
    }
    else
    {
        ferDrawFocusWindow(region, lw_normal);
        index += 1;
    }
    image = cmShipImage[race][index];
    texture = cmShipTexture[race][index];
    // CAT: the exe draws the image scaled through glcompat when cat_glcActive 0x0055c480; the SDL build has no glcompat
    //      (as svShipViewRender), so the texture path
    if (cmPaletted)
    {
        trPalettedTextureMakeCurrent(texture, image->palette);
    }
    else
    {
        trRGBTextureMakeCurrent(texture);
    }
    primRectSolidTextured2(&rect);
    if (present && (shipID == cmCurrentSelect))
    {
        if (cmFactoryNameRegion != NULL)
        {
            sdword mid = (rect.x1 - rect.x0) / 2 + rect.x0;

            primLineThick2(mid, cmFactoryNameRegion->rect.y1, mid, region->rect.y0, 2, colWhite);   // name line to the button
        }
        if (cmBaseRegion != NULL)
        {
            cmBaseRegion->status |= RSF_ReallyDirty;      // CAT: status bit 0x1 (0x00458b74)
        }
    }
}

void cmMotherShipDraw(featom *atom, regionhandle region)
{
    Ship *mothership = universe.curPlayerPtr->PlayerMothership;

    // CAT (M2-R6 managers): the exe's body (0x00458b80): a mothership-class mothership makes the button live
    cmMotherShipRegion = region;
    if ((mothership != NULL) && (mothership->staticinfo->shipclass == CLASS_Mothership))
    {
        if (region->flags == 0 || region->flags == RPE_DrawFunctionAdded)
        {
            region->flags = RPE_DrawEveryFrame | RPE_PressLeft;    // CAT: 0x80002000
            regFunctionSet(region, (regionfunction)cmSelectMotherShip);
        }
        cmDrawShipImage(region, 0, TRUE);
        catCmFactoryShip = mothership;
        return;
    }
    cmDrawShipImage(region, 0, FALSE);
}

void cmCarrierXDraw(featom *atom, regionhandle region,udword x)
{
    sdword i;
    udword found = 0;

    // CAT (M2-R6 managers): the exe's body (0x00458bf0): the x-th of the local player's carriers in cmCarriers order
    cmCarrierRegions[x - 1] = region;
    for (i = 0; i < cmNumCarriers; i++)
    {
        if ((cmCarriers[i].owner == universe.curPlayerPtr) && (++found >= x))
        {
            cmCarrierX[x - 1] = cmCarriers[i].ship;
            if (region->flags == 0 || region->flags == RPE_DrawFunctionAdded)
            {
                region->flags = RPE_DrawEveryFrame | RPE_PressLeft;    // CAT: 0x80002000
                if (x == 1)
                {
                    regFunctionSet(region, (regionfunction)cmSelectCarrier1);
                }
                else if (x == 2)
                {
                    regFunctionSet(region, (regionfunction)cmSelectCarrier2);
                }
            }
            cmDrawShipImage(region, x, TRUE);
            catCmFactoryShip = cmCarriers[i].ship;
            return;
        }
    }
    cmDrawShipImage(region, x, FALSE);
}

void cmCarrier1Draw(featom *atom, regionhandle region)
{
    cmCarrierXDraw(atom, region, 1);
}

void cmCarrier2Draw(featom *atom, regionhandle region)
{
    cmCarrierXDraw(atom, region, 2);
}

/*-----------------------------------------------------------------------------
    Name        : cat_cmFactoryNameDraw
    Description : CAT: new (0x00458920, the CM_FactoryNameDraw callback): the shown factory's name (a human player's
                  named ship, cat_shipNamesGetName) above a 2-pixel white line along the region's bottom
----------------------------------------------------------------------------*/
void cat_cmFactoryNameDraw(featom *atom, regionhandle region)
{
    fonthandle oldfont;
    Ship *factory;

    cmFactoryNameRegion = region;
    oldfont = fontMakeCurrent(catCmFactoryNameFont);
    factory = catCmShownFactoryShip();
    catCmFactoryShip = factory;
    if ((factory != NULL) && (factory->playerowner->aiPlayer == NULL))
    {
        fontPrintf(region->rect.x0, region->rect.y1 - fontHeight(" "), colWhite, "%s", cat_shipNamesGetName(factory));
    }
    primLineThick2(region->rect.x0, region->rect.y1, region->rect.x1, region->rect.y1, 2, colWhite);
    fontMakeCurrent(oldfont);
}






/*-----------------------------------------------------------------------------
    Name        : cmBuildJobsAdd
    Description : Create a number of new build jobs on the specified ship
    Inputs      : factory - where the ship is to be built
                  info - information on the ship to build
                  nJobs - number of these ships to build
    Outputs     : adds a new entry into the list of ships in progress
    Return      : void
----------------------------------------------------------------------------*/
void cmBuildJobsAdd(shipsinprogress *factory, ShipStaticInfo *info, sdword nJobs)
{
    shipinprogress *progress;
    sdword prevJobsTotal;
//    sdword index;

#if CM_VERBOSE_LEVEL >= 1
    dbgMessagef("cmBuildJobAdd: Building %s", ShipTypeToNiceStr(info->shiptype));
#endif
    dbgAssertOrIgnore(info->shiptype < TOTAL_NUM_SHIPS);                 //make sure we can add a job (CAT: 0x91 entries)
    progress = &factory->progress[info->shiptype];         //update the progress
#if CM_CAP_JOBS_PER_CLASS
    if (factory->progress[info->shiptype].nJobs >= CM_NumberJobsPerClass)
    {                                                       //only so much we can do man!
        return;                                             //??? do we need this???
    }
#endif
    if (progress->nJobs == 0)
    {                                                       //if no jobs currently
        // CAT: the single-player build-time divisor 0x00894198 (cmBuildJobsAdd 0x004586c0)
        progress->timeLeft = progress->timeStart =
            singlePlayerGame ? info->buildTime / catCmSinglePlayerBuildFactor : info->buildTime;
        progress->timeFraction = 0; // no fractional bits in fixed point for RU decrement
        progress->paused = FALSE;
        progress->costSoFar = 0;
    }
    progress->info = info;                                  //init the progress structure
#if CM_CAP_JOBS_PER_CLASS
    progress->nJobs = min(progress->nJobs + nJobs, CM_NumberJobsPerClass);//add the new jobs
#else
    progress->nJobs = progress->nJobs + nJobs;              //add the new jobs
#endif
    prevJobsTotal = progress->nJobsTotal;
    progress->nJobsTotal += nJobs;

    // CAT: also in a recorded multiplayer game (0x00a7519c && 0x00a75190)
    if (catNetworkGame())
    {
        //xxx
        clWrapDeterministicBuild(CMD_START, &universe.mainCommandLayer,
                                 progress->nJobsTotal | 0x8000, info->shiptype,
                                 universe.curPlayerPtr->race,
                                 universe.curPlayerIndex,
                                 factory->ship);
    }
}















ShipType cmKeyToShipType(uword key)
{
    uword i;

    for (i=0; cmShipTypes[i].key!=-1; i++)
    {
        if (cmShipTypes[i].key == key)
        {
            return (ShipType)cmShipTypes[i].ship;

        }

    }
    return 0;
}


uword cmShipTypeToKey(ShipType ship)
{
    uword i;



    for (i=0; cmShipTypes[i].key!=-1; i++)
    {
        if (cmShipTypes[i].ship == ship)
        {
            return cmShipTypes[i].key;

        }

    }
    return 0;
}


//detoured keypresses:

//detoured keypresses:

/*-----------------------------------------------------------------------------
    Name        : cmBuildHotKey
    Description : a build hot key while the screen's key detour is on
    CAT (M2-R6 managers): the exe's body (0x00458d40): the key test is on the virtual-key code (A..Z but B, P and C;
        F1..F12; 1..3); a key in the range that builds nothing is still taken; a found row must be a listed ship row
        (not unbuildable: a type at its cap still tries); the job goes straight to the factory. CAT-FIX: the exe walks
        0x91 rows whatever the list's length (past its end row)
----------------------------------------------------------------------------*/
bool32 cmBuildHotKey(keyindex key, bool32 shift)
{
    uword index;
    sdword i;
    sword skey;
    ShipType ship;
    udword vk = catKeyVkFromKey(key);

    if (!(((vk >= 'A') && (vk <= 'Z') && (vk != 'B') && (vk != 'P') && (vk != 'C')) ||
          ((vk >= 0x70) && (vk <= 0x7b)) || ((vk >= '1') && (vk <= '3'))))
    {
        return FALSE;      //don't bypass - it's a standard region key
    }
    skey = (sword)(shift ? (key | CM_SHIFT) : key);
    for (i = 0; cmShipTypes[i].key != -1; i++)
    {
        if (cmShipTypes[i].key == skey)
        {
            break;
        }
    }
    if (cmShipTypes[i].key == -1)
    {
        return TRUE;
    }
    ship = cmShipTypes[i].ship;
    if (ship != 0)
    {
        for (index = 0; (index <= 0x90) && (cmShipsAvailable[index].nJobs != -1); index++)
        {
            if ((cmShipsAvailable[index].itemtype != ITEM_CLASS) && (cmShipsAvailable[index].itemstat != STAT_CANTBUILD) &&
                (cmShipsAvailable[index].type == ship))
            {
                if (cmIncrement(curshipsInProgress, index))
                {
                    cmDecrement(curshipsInProgress, index);
                    cmBuildJobsAdd(curshipsInProgress, cmShipsAvailable[index].info, 1);    //start building now
                }
                break;
            }
        }
    }
    return TRUE;
}


void cmDeterministicStartup(void)
{
    listInit(&cmDetermProgress);
}

void cmDeterministicShutdown(void)
{
    listDeleteAll(&cmDetermProgress);
}

void cmDeterministicReset(void)
{
    listDeleteAll(&cmDetermProgress);
}

void cmDeterministicBuildDisplay(void)
{
    Node* node;
    cmDetermProgress_t* dprog;
    sdword y, height;

    height = fontHeight(NULL);

    y = 4 * height;
    node = cmDetermProgress.head;
    while (node != NULL)
    {
        dprog = (cmDetermProgress_t*)listGetStructOfNode(node);

        fontPrintf(
            0, y, colReddish,
            "pl %d, num %d, type %s, race %s, tleft %d, ru %d",
            (sdword)dprog->playerIndex,
            dprog->numShips,
            ShipTypeToStr(dprog->shipType),
            ShipRaceToStr(dprog->shipRace),
            dprog->ticksLeft,
            universe.players[dprog->playerIndex].resourceUnits
            );

        //next line, next node
        y += height;
        if ((y + height) > MAIN_WindowHeight)
        {
            //don't bother w/ off-screen lines
            break;
        }
        node = node->next;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cmDeterministicBuildStart
    Description : adds ships to the deterministic build list (multiplayer and recorded games)
    CAT (M2-R6 managers): the exe's body (0x00458e70): a new node is built from the static of the type alone
        (GetShipStaticInfo with one argument), the build time and cost are divided by the single-player factors
        (0x00894198 / 0x0089419c) in a single-player game, the factory entry of the type at the creator loses its
        external-build attach and started words, and the node is added only for a buildable type (below 0x3c) of the
        player's race (HW1's cmDeterministicSanityCheck folded in). A "subtract" count (0x4000) for a type with no
        node is still allocated in the exe and then dropped without being freed: the leak is not reproduced
        (CAT-FIX), nor the write through the NULL factory entry when the creator is no factory (CAT-FIX)
----------------------------------------------------------------------------*/
void cmDeterministicBuildStart(
    sdword numShips, ShipType shipType, ShipRace shipRace, sdword playerIndex, ShipPtr creator)
{
    Node* node;
    cmDetermProgress_t* dprog;
    ShipStaticInfo* info;
    shipinprogress* progress;

    //look for existing match in cmDetermProgress
    node = cmDetermProgress.head;
    while (node != NULL)
    {
        dprog = (cmDetermProgress_t*)listGetStructOfNode(node);
        if (dprog->shipType == shipType &&
            dprog->shipRace == shipRace &&
            dprog->playerIndex == playerIndex &&
            dprog->creator == creator)
        {
            if (bitTest(numShips, 0x8000))
            {
                //hi-bit (of uword) set, we're an absolute number
                bitClear(numShips, 0x8000);
                dprog->numShips = numShips;
                if (numShips <= 0)
                {
                    //we've run out of jobs in the batch
                    listDeleteNode(node);
                }
                return;
            }
            else
            {
                //match found, increment count & exit
                if (bitTest(numShips, 0x4000))
                {
                    bitClear(numShips, 0x4000);
                    if (dprog->numShips > 1)
                    {
                        dprog->numShips -= numShips;
                    }
                }
                else
                {
                    dprog->numShips += numShips;
                }
                return;
            }
        }

        //move on
        node = node->next;
    }

    //not found, create new node
    if (bitTest(numShips, 0x8000))
    {
        bitClear(numShips, 0x8000);
    }
    if (bitTest(numShips, 0x4000))
    {
        //we're subtracting from nothing (CAT-FIX: the exe allocates the node first and loses it here)
        return;
    }
    info = GetShipStaticInfo(shipType, shipRace);
    dprog = (cmDetermProgress_t*)memAlloc(sizeof(cmDetermProgress_t), "dprog_t", NonVolatile);
    dprog->numShips = numShips;
    dprog->shipType = shipType;
    dprog->shipRace = shipRace;

    dprog->ticksForOne = singlePlayerGame ? info->buildTime / catCmSinglePlayerBuildFactor : info->buildTime;
    dprog->ticksLeft = dprog->ticksForOne;

    dprog->costPerShip = singlePlayerGame ? info->buildCost / catCmSinglePlayerCostFactor : info->buildCost;
    dprog->costPerTick = (real32)dprog->costPerShip / (real32)dprog->ticksForOne;
    dprog->costSoFar = 0.0f;

    dprog->playerIndex = playerIndex;
    dprog->creator = creator;
    dprog->paused = FALSE;
    dprog->catAttached = FALSE;
    dprog->catShipStarted = FALSE;
    dprog->catShip = NULL;

    progress = catCmFactoryEntry(creator, shipType);
    if (progress != NULL)       // CAT-FIX: the exe writes through the entry at address 0 when creator is no factory
    {
        progress->catExternalShipAttached = FALSE;
        progress->catExternalShipStarted = FALSE;
    }

    if ((dprog->shipType < TOTAL_STD_SHIPS) && (dprog->shipRace == universe.players[dprog->playerIndex].race))
    {
        listAddNode(&cmDetermProgress, &dprog->node, dprog);
    }
    else
    {
        memFree(dprog);     // CAT-FIX: the exe keeps the unlisted node allocated
    }
}

void cmAdjustRU(cmDetermProgress_t* dprog, sdword amount)
{
    Player* player = &universe.players[dprog->playerIndex];
    player->resourceUnits += amount;

    universe.gameStats.playerStats[player->playerIndex].totalResourceUnitsSpent += min(player->resourceUnits, ABS(amount));

    if (player->resourceUnits < 0)
    {
        player->resourceUnits = 0;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cmDeterministicBuildCancel
    Description : cancels the deterministic build of a type at a creator and refunds what it cost so far
    CAT (M2-R6 managers): the exe's body (0x00459040): the refund is the whole cost so far (no refund ratio); an
        externally built type whose ship exists has that ship deleted (cat_clExternalBuildUpdate mode 1, cancel) and
        the creator's factory entry forgets it (started word; CAT-FIX: not through the NULL entry of a creator that
        is no factory)
----------------------------------------------------------------------------*/
void cmDeterministicBuildCancel(ShipType shipType, ShipRace shipRace, sdword playerIndex, ShipPtr creator)
{
    Node* node;
    cmDetermProgress_t* dprog;
    shipinprogress* progress;

    //look for match
    node = cmDetermProgress.head;
    while (node != NULL)
    {
        dprog = (cmDetermProgress_t*)listGetStructOfNode(node);
        if (dprog->shipType == shipType &&
            dprog->shipRace == shipRace &&
            dprog->playerIndex == playerIndex &&
            dprog->creator == creator)
        {
            //match found, cancel & refund RUs
            cmAdjustRU(dprog, (sdword)dprog->costSoFar);
            if (GetShipStaticInfo(dprog->shipType, dprog->shipRace)->externalBuild && (dprog->catShipStarted == 1))
            {
                cat_clExternalBuildUpdate(&universe.mainCommandLayer, (uword)dprog->playerIndex, dprog->creator,
                                          dprog->catShip, 0.0f, CAT_EXTBUILD_CANCEL);
                dprog->catShipStarted = FALSE;
                dprog->catShip = NULL;
                progress = catCmFactoryEntry(dprog->creator, dprog->shipType);
                if (progress != NULL)
                {
                    progress->catExternalShipStarted = FALSE;
                }
            }
            listDeleteNode(node);
            return;
        }

        //move on
        node = node->next;
    }
}

void cmDeterministicBuildPause(
    bool32 pause, ShipType shipType, ShipRace shipRace, sdword playerIndex, ShipPtr creator)
{
    Node* node;
    cmDetermProgress_t* dprog;

    //look for match
    node = cmDetermProgress.head;
    while (node != NULL)
    {
        dprog = (cmDetermProgress_t*)listGetStructOfNode(node);
        if (dprog->shipType == shipType &&
            dprog->shipRace == shipRace &&
            dprog->playerIndex == playerIndex &&
            dprog->creator == creator)
        {
            //match found, adjust pause state
            dprog->paused = pause;
        }

        //next node
        node = node->next;
    }
}

void cmDeterministicBuild(udword command, sdword numShips,
                          ShipType shipType, ShipRace shipRace,
                          uword playerIndex, ShipPtr creator)
{
    switch (command)
    {
    case CMD_START:
        cmDeterministicBuildStart(numShips, shipType, shipRace, playerIndex, creator);
        break;

    case CMD_CANCEL:
        cmDeterministicBuildCancel(shipType, shipRace, playerIndex, creator);
        break;

    case CMD_PAUSE:
        cmDeterministicBuildPause(TRUE, shipType, shipRace, playerIndex, creator);
        break;

    case CMD_UNPAUSE:
        cmDeterministicBuildPause(FALSE, shipType, shipRace, playerIndex, creator);
        break;

    default:
        dbgFatalf(DBG_Loc, "unknown dbuild command: %u", command);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cmDeterministicBuildProcess
    Description : advances the deterministic build list one universe tick
    CAT (M2-R6 managers): the exe's body (0x004592a0). An externally built type first waits, at a full clock and with
        no ship yet, for a free build slot at the creator and (but for a module or at a Beast mothership) the docking
        bay module; then its ship is created (clCreateShip, counted in shiplagtotals for the local player) and the
        node waits again until the ship has reached its build position (catAttached). Every other type, and an
        attached external one, pays its cost per tick (by the Beast mothership's build factor), counts down (by that
        factor), and at the end an external ship gets its last step (cat_clExternalBuildUpdate done) and the local
        player's factory entry is finished (cat_cmBuildJobFinished), any other type is created (clCreateShip). An
        attached external ship gets the progress every tick (mode 2). A paused node, or one whose player has no
        resource units, waits
----------------------------------------------------------------------------*/
void cmDeterministicBuildProcess(void)
{
    Node* node;
    Node* curNode;
    cmDetermProgress_t* dprog;
    Player* player;
    ShipStaticInfo* info;
    shipinprogress* progress;
    sdword factor;

    if (universePause)
    {
        //universe is paused, we won't be building anything
        return;
    }

    node = cmDetermProgress.head;
    while (node != NULL)
    {
        dprog = (cmDetermProgress_t*)listGetStructOfNode(node);
        player = &universe.players[dprog->playerIndex];
        if (player->playerState != PLAYER_ALIVE)
        {
            //dead player, clear batch
            curNode = node;
            node = node->next;
            listDeleteNode(curNode);
            continue;
        }

        //advance
        curNode = node;
        node = node->next;

        if ((sdword)dprog->numShips <= 0)
        {
            listDeleteNode(curNode);
            continue;
        }

        if (dprog->paused || (player->resourceUnits == 0))
        {
            continue;
        }
        info = GetShipStaticInfo(dprog->shipType, dprog->shipRace);

        if ((dprog->ticksLeft == dprog->ticksForOne) && (info->externalBuild == 1) && (dprog->catShipStarted == 0))
        {
            // CAT: the external ship is created once its slot is free (0x004592f5)
            if (cat_dockHasFreeSlotForShip(dprog->creator, info->shipclass, dprog->shipType, TRUE, 0) &&
                ((info->shipclass == CLASS_Component) || (dprog->creator->shiptype == bMothership) ||
                 cat_playerHasCompletedShip(sMothershipDockingBay, dprog->creator->playerowner)))
            {
                if (dprog->creator->playerowner == universe.curPlayerPtr)
                {
                    shiplagtotals[dprog->shipType]++;
                }
                dprog->catShip = clCreateShip(&universe.mainCommandLayer, dprog->shipType, dprog->shipRace,
                                              (uword)dprog->playerIndex, dprog->creator);
                dprog->catShipStarted = TRUE;
                dprog->catAttached = FALSE;
                progress = catCmFactoryEntry(dprog->creator, dprog->shipType);
                if (progress != NULL)       // CAT-FIX: the exe writes through the entry at address 0 (0x00459447)
                {
                    progress->catExternalShipStarted = TRUE;
                    progress->catExternalShipAttached = FALSE;
                }
            }
            continue;
        }

        if (info->externalBuild && (dprog->catAttached != 1))
        {
            continue;
        }
        progress = catCmFactoryEntry(dprog->creator, dprog->shipType);

        //increment cost, decrement RUs
        {
            sdword lastCost, newCost;
            real32 costThisTick = dprog->costPerTick;

            lastCost = (sdword)dprog->costSoFar;
            if (dprog->creator->shiptype == bMothership)
            {
                costThisTick = (real32)cat_beastMothershipBuildFactor(dprog->creator) * dprog->costPerTick;
            }
            newCost = (sdword)(costThisTick + dprog->costSoFar);
            if (newCost != lastCost)
            {
                cmAdjustRU(dprog, lastCost - newCost);
            }
            dprog->costSoFar += costThisTick;
        }
        if ((dprog->creator == NULL) || (dprog->creator->shiptype != bMothership))
        {
            dprog->ticksLeft--;
        }
        else
        {
            dprog->ticksLeft -= cat_beastMothershipBuildFactor(dprog->creator);
        }

        if (dprog->ticksLeft <= 0)
        {
            //finished, make the ship appear
            if (info->externalBuild == 1)
            {
                if (dprog->catShipStarted == 1)
                {
                    cat_clExternalBuildUpdate(&universe.mainCommandLayer, (uword)dprog->playerIndex, dprog->creator,
                                              dprog->catShip, 1.0f, CAT_EXTBUILD_DONE);
                }
                dprog->catShipStarted = FALSE;
                dprog->catShip = NULL;
                if (progress != NULL)       // CAT-FIX: NULL when the creator is no factory (0x0045963c)
                {
                    progress->catExternalShipStarted = FALSE;
                    progress->catExternalShip = NULL;
                    if (dprog->creator->playerowner == universe.curPlayerPtr)
                    {
                        cat_cmBuildJobFinished(progress, dprog->creator);
                    }
                }
            }
            else
            {
                clCreateShip(&universe.mainCommandLayer,
                             dprog->shipType,
                             dprog->shipRace,
                             (uword)dprog->playerIndex,
                             dprog->creator);
            }

            if (player->resourceUnits > 0)
            {
                if (dprog->creator->shiptype == bMothership)
                {
                    factor = cat_beastMothershipBuildFactor(dprog->creator);
                    if (factor <= player->resourceUnits)
                    {
                        cmAdjustRU(dprog, -factor);
                    }
                }
                else
                {
                    cmAdjustRU(dprog, -1);
                }
            }

            //back to 0 cost for current ship
            dprog->costSoFar = 0.0f;

            dprog->numShips--;
            if (dprog->numShips == 0)
            {
                //no more ships, delete progress node
                listDeleteNode(curNode);
            }
            else
            {
                //still more to go, reset tick counter for next ship
                dprog->ticksLeft = dprog->ticksForOne;
            }
        }
        else if ((dprog->catAttached != 0) && (info->externalBuild == 1))
        {
            cat_clExternalBuildUpdate(&universe.mainCommandLayer, (uword)dprog->playerIndex, dprog->creator, dprog->catShip,
                                      (real32)(1.0 - (double)dprog->ticksLeft / (double)dprog->ticksForOne),
                                      CAT_EXTBUILD_PROGRESS);
        }
    }
}

/*=============================================================================
    Save Game Stuff:
=============================================================================*/

#define RaceAndTypeToRaceType(race,type) ( ((race)<<16) + (type) )
#define GetRaceFromRaceType(racetype) ( ((racetype)>>16) & 0x0000ffff )
#define GetTypeFromRaceType(racetype) ( (racetype) & 0x0000ffff )

sdword ConvertShipStaticInfoToRaceType(ShipStaticInfo *shipstatic)
{
    if (shipstatic == NULL)
    {
        return -1;
    }
    else
    {
        return RaceAndTypeToRaceType(shipstatic->shiprace,shipstatic->shiptype);
    }
}

ShipStaticInfo *ConvertRaceTypeToShipStaticInfo(sdword racetype)
{
    if (racetype == -1)
    {
        return NULL;
    }
    else
    {
        return GetShipStaticInfo(GetTypeFromRaceType(racetype),GetRaceFromRaceType(racetype));
    }
}

void Save_shipsinprogress(shipsinprogress *sinprogress)
{
    SaveChunk *chunk;
    shipsinprogress *savecontents;
    sdword i;

    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_SHIPSINPROGRESS,sizeof(shipsinprogress),sinprogress);
    savecontents = (shipsinprogress *)chunkContents(chunk);

    savecontents->ship = (Ship *)SpaceObjRegistryGetID((SpaceObj *)savecontents->ship);
    dbgAssertOrIgnore((sdword)savecontents->ship != -1);
    // CAT (M2-R6 managers): 0x91 entries, and the externally built ship by its ID (SaveConsMgr 0x00459780)
    for (i=0;i<TOTAL_NUM_SHIPS;i++)
    {
        savecontents->progress[i].info = (ShipStaticInfo *)ConvertShipStaticInfoToRaceType(savecontents->progress[i].info);
        savecontents->progress[i].catExternalShip = (Ship *)SpaceObjRegistryGetID((SpaceObj *)savecontents->progress[i].catExternalShip);
    }

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(shipsinprogress));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
    memFree(chunk);
}


void SaveConsMgr(void)
{
    sdword num = listofShipsInProgress.num;
    sdword cur = 0;
    Node *node = listofShipsInProgress.head;

    SaveInfoNumber(num);

    while (node != NULL)
    {
        cur++;
        Save_shipsinprogress(listGetStructOfNode(node));

        node = node->next;
    }

    dbgAssertOrIgnore(cur == num);

    SaveInfoNumber(cmNumCarriers);
    if (cmNumCarriers > 0)
    {
        SaveChunk *chunk;
        CarrierInfo *savecontents;
        sdword i;

        chunk = CreateChunk(BASIC_STRUCTURE,sizeof(CarrierInfo) * cmNumCarriers,cmCarriers);
        savecontents = (CarrierInfo *)chunkContents(chunk);

        for (i=0;i<cmNumCarriers;i++)
        {
            savecontents[i].ship = (Ship *)SpaceObjRegistryGetID((SpaceObj *)savecontents[i].ship);
            dbgAssertOrIgnore((sdword)savecontents[i].ship != -1);
            savecontents[i].owner = (Player *)SavePlayerToPlayerIndex(savecontents[i].owner);
        }

        saveCat32SaveArray(chunk, NULL, 0, 0, NULL, 2 * cmNumCarriers);   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save: a ship ID and a player index a carrier
        memFree(chunk);
    }
}

shipsinprogress *Load_shipsinprogress(void)
{
    SaveChunk *chunk;
    shipsinprogress *sinprogress;
    sdword i;

    chunk = LoadNextChunk();
    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(shipsinprogress));    // CAT (M2-R6 oracle-mode-a): the exe's 0x1B48
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_SHIPSINPROGRESS,sizeof(shipsinprogress));
    sinprogress = memAlloc(sizeof(shipsinprogress),"sinprogress",NonVolatile);
    memcpy(sinprogress,chunkContents(chunk),sizeof(shipsinprogress));

    memFree(chunk);

    sinprogress->ship = SpaceObjRegistryGetShip((sdword)sinprogress->ship);
    dbgAssertOrIgnore(sinprogress->ship != NULL);

    // CAT (M2-R6 managers): 0x91 entries, and the externally built ship from its ID (LoadConsMgr 0x004598a0)
    for (i=0;i<TOTAL_NUM_SHIPS;i++)
    {
        sinprogress->progress[i].info = ConvertRaceTypeToShipStaticInfo((sdword)sinprogress->progress[i].info);
        sinprogress->progress[i].catExternalShip = SpaceObjRegistryGetShip((sdword)sinprogress->progress[i].catExternalShip);
    }

    return sinprogress;
}


void LoadConsMgr(void)
{
    sdword num;
    sdword i;
    shipsinprogress *sinprogress;

    num = LoadInfoNumber();

    dbgAssertOrIgnore(listofShipsInProgress.num == 0);
    listInit(&listofShipsInProgress);

    for (i=0;i<num;i++)
    {
        sinprogress = Load_shipsinprogress();
        listAddNode(&listofShipsInProgress,&sinprogress->node,sinprogress);
    }

    cmNumCarriers = LoadInfoNumber();
    if (cmNumCarriers > 0)
    {
        SaveChunk *chunk;
        sdword size = sizeof(CarrierInfo) * cmNumCarriers;

        chunk = LoadNextChunk();
        // CAT (M2-R6 oracle-mode-a): the exe's entries are a ship ID and a player index, 4 bytes each (LoadConsMgr 0x004598a0)
        chunk = saveCat32ChunkArray(chunk, NULL, 0, 0, NULL, 2 * cmNumCarriers);
        VerifyChunk(chunk,BASIC_STRUCTURE,size);

        cmCarriers = memAlloc(size, "cmCarriers", NonVolatile);
        memcpy(cmCarriers,chunkContents(chunk),size);

        for (i=0;i<cmNumCarriers;i++)
        {
            cmCarriers[i].ship = SpaceObjRegistryGetShip((sdword)cmCarriers[i].ship);
            cmCarriers[i].owner = SavePlayerIndexToPlayer(cmCarriers[i].owner);
        }

        memFree(chunk);
    }

    cmFillInCarrierXs();
}

void SaveConsMgrDetermListCB(void *stuff)
{
    cmDetermProgress_t copy = *((cmDetermProgress_t *)stuff);

    copy.creator = (ShipPtr)SpaceObjRegistryGetID((SpaceObj *)copy.creator);
    copy.catShip = (Ship *)SpaceObjRegistryGetID((SpaceObj *)copy.catShip);    // CAT: the exe's callback 0x00459e50

    SaveStructureOfSize(&copy,sizeof(copy));
}

void LoadConsMgrDetermListCB(LinkedList *list)
{
    cmDetermProgress_t *dp = LoadStructureOfSize(sizeof(cmDetermProgress_t));
    dp->creator = SpaceObjRegistryGetShip((sdword)dp->creator);
    dp->catShip = SpaceObjRegistryGetShip((sdword)dp->catShip);              // CAT: the exe's callback 0x00459ea0

    listAddNode(list,&dp->node,dp);
}



#define BUILD_OPTIONAL_FLAG     0xb11db11d

void SaveConsMgrDetermOptional(void)
{
    if (cmDetermProgress.num == 0)
    {
        return;         // don't save anything - optional block
    }

    SaveInfoNumber(BUILD_OPTIONAL_FLAG);

    SaveLinkedListOfStuff(&cmDetermProgress,SaveConsMgrDetermListCB);
}

void LoadConsMgrDetermOptional(void)
{
    sdword optional;

    if (LoadInfoNumberOptional(&optional) && (optional == BUILD_OPTIONAL_FLAG))
    {
        LoadLinkedListOfStuff(&cmDetermProgress,LoadConsMgrDetermListCB);
    }
    else
    {
        listInit(&cmDetermProgress);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_cmHasSupportUnits
    Description : CAT: does player have the support units for one more ship of type? (cat_cmHasSupportUnits
                  0x00459b40; the salvage check asks it before a salvager takes a target)
    Inputs      : type, player
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 cat_cmHasSupportUnits(ShipType type, Player *player)
{
    ShipStaticInfo *info = GetShipStaticInfo(type, player->race);
    ShipStaticInfo *jobinfo;
    Node *node;
    cmDetermProgress_t *dprog;
    udword queued = 0;
    sdword jobs;

    if ((player->playerIndex != 0) && singlePlayerGame)
    {
        return TRUE;        // a computer player of a single-player game
    }
    if (info->supportUnits == 0)
    {
        return TRUE;
    }
    if ((udword)player->supportUnitsMax <= (udword)player->supportUnitsUsed)
    {
        return FALSE;
    }
    // the support units of the player's deterministic build jobs (an externally built job whose ship is attached
    //      already counts as a ship)
    for (node = cmDetermProgress.head; node != NULL; node = node->next)
    {
        dprog = (cmDetermProgress_t *)listGetStructOfNode(node);
        if (dprog->creator->playerowner == player)
        {
            jobs = dprog->numShips;
            jobinfo = GetShipStaticInfo(dprog->shipType, dprog->shipRace);
            if ((jobs > 0) && (jobinfo->externalBuild == 1) && (dprog->catAttached == 1))
            {
                jobs--;
            }
            queued += jobinfo->supportUnits * jobs;
        }
    }
    return ((udword)player->supportUnitsMax - (udword)player->supportUnitsUsed) >= info->supportUnits + queued;
}

// CAT (M2-R5 hud): 0x00894198, the single-player build-time divisor (1; cat_spGameDifficulty 0x0051c16b and kasLoad
//      set it, stage 6; the build task's other readers come with M2-R6 ConsMgr). Name chosen here
sdword catCmSinglePlayerBuildFactor = 1;
// CAT (M2-R5 hud): 0x0089419c, the single-player cost divisor next to it (1 in .data; the same writers); the ship view
//      divides the build cost it prints by it
sdword catCmSinglePlayerCostFactor = 1;
// CAT (M2-R5 hud): 0x00a2dbf0, toggled by the CM_ExtendedInfo button (0x00454620, the exe's callback table 0x00753a18);
//      the ship view then darkens its panel and prints the type's description (cat_svShipDescriptionDraw)
bool32 catCmExtendedInfo = FALSE;

/*-----------------------------------------------------------------------------
    Name        : cat_cmExtendedInfoToggle                          0x00454620
    Description : The CM_ExtendedInfo button: the ship descriptions on or off, and the screen's info regions redrawn
                  (the exe dirties the regions cmDirtyShipInfo does, without its cmUpdateShipAvailable)
----------------------------------------------------------------------------*/
void cat_cmExtendedInfoToggle(char *string, featom *atom)
{
    (void)string;
    (void)atom;
    catCmExtendedInfo = !catCmExtendedInfo;
    catCmDirtyRegions();
}

/*-----------------------------------------------------------------------------
    Name        : cat_cmCountJobsForPlayer                          0x00459ce0
    Description : CAT (M2-R5 hud): the player's ships in construction, one line per type in first-seen order, with the
                  jobs and time left per factory slot (the mothership's factory is slot 0, the carriers 1, 2, ..):
                  the main overlay's construction list (cat_moConstructionDraw 0x004a91c0). Returns the lines.
----------------------------------------------------------------------------*/
sdword cat_cmCountJobsForPlayer(struct Player *player, CatCmJob *jobs)
{
    sdword lineOfType[TOTAL_NUM_SHIPS];
    sdword lines = 0, slot, nextSlot = 1, type, line;
    Node *node;
    shipsinprogress *factory;
    ShipType factoryType;

    for (type = 0; type < TOTAL_NUM_SHIPS; type++)
    {
        lineOfType[type] = -1;
    }
    for (node = listofShipsInProgress.head; node != NULL; node = node->next)
    {
        factory = (shipsinprogress *)listGetStructOfNode(node);
        if (factory->ship->playerowner != player)
        {
            continue;
        }
        factoryType = factory->ship->shiptype;
        if (factoryType == sMothership || factoryType == bMothership)
        {
            slot = 0;
        }
        else
        {
            slot = nextSlot++;
        }
        for (type = 0; type < TOTAL_NUM_SHIPS; type++)
        {
            if (factory->progress[type].nJobs > 0)
            {
                if (lineOfType[type] == -1)
                {
                    line = lines++;
                    jobs[line].type = factory->progress[type].info->shiptype;
                    lineOfType[type] = line;
                    jobs[line].totalTime = singlePlayerGame ? factory->progress[type].info->buildTime / catCmSinglePlayerBuildFactor :
                                           factory->progress[type].info->buildTime;
                    jobs[line].nJobs[0] = jobs[line].nJobs[1] = jobs[line].nJobs[2] = jobs[line].nJobs[3] = 0;
                }
                line = lineOfType[type];
                // the exe indexes the slot without a bound (a fifth factory writes into the next line)
                ((sdword *)&jobs[line].nJobs[0])[slot] = factory->progress[type].nJobs;
                ((sdword *)&jobs[line].timeLeft[0])[slot] = factory->progress[type].timeLeft;
            }
        }
    }
    return lines;
}

/*=============================================================================
    CAT (M2-R6 managers): the external build's construction-manager side and the support units
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : cat_cmExternalBuildAttach
    Description : CAT: new (0x00455af0): an externally built ship has reached its build position at its creator
                  (Dock.c's CAT_DOCKSTATE_LAUNCH, cat_dockShipDocksAtShip 0x00465860). In a single-player game the
                  local human player's factory entry of the type takes the ship and starts its clock
                  (catExternalShipAttached); in a multiplayer game the creator's deterministic build node of the type
                  does (catAttached), and the factory entry too
----------------------------------------------------------------------------*/
void cat_cmExternalBuildAttach(Ship *ship)
{
    shipinprogress *progress = NULL;
    Node *node;
    cmDetermProgress_t *dprog;

    // CAT: a recorded multiplayer game (0x00a7519c && 0x00a75190) counts as a multiplayer one here (0x00455b49 ff.)
    if (!playPackets && (ship->playerowner == universe.curPlayerPtr) && (ship->playerowner->aiPlayer == NULL))
    {
        progress = catCmFactoryEntry(ship->catCreator, ship->shiptype);
        if (!catNetworkGame())
        {
            if (progress != NULL)       // CAT-FIX: the exe writes through the entry at address 0 when the creator is no factory
            {
                progress->catExternalShip = ship;
                progress->catExternalShipAttached = TRUE;
            }
            return;
        }
    }
    else if (!catNetworkGame())
    {
        return;
    }
    for (node = cmDetermProgress.head; node != NULL; node = node->next)
    {
        dprog = (cmDetermProgress_t *)listGetStructOfNode(node);
        if ((dprog->creator == ship->catCreator) && (dprog->shipType == ship->shiptype))
        {
            dprog->catAttached = TRUE;
            progress = catCmFactoryEntry(dprog->creator, dprog->shipType);
            if (progress != NULL)       // CAT-FIX: as above
            {
                progress->catExternalShipAttached = TRUE;
            }
            return;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_cmExternalBuildShipDied
    Description : CAT: new (0x00455960): an externally built ship died (univDeleteDeadShip 0x00547d00) while its build
                  was running. The local human player's factory entry of its type at its creator loses the job (one
                  job and one of the total less, the clock and fraction cleared, the external words cleared) if it
                  still held this ship; in a multiplayer game the creator's deterministic build node waits for a new
                  ship (one ship less, the clock reset, the node's and the factory entry's external words cleared)
----------------------------------------------------------------------------*/
void cat_cmExternalBuildShipDied(Ship *ship)
{
    shipinprogress *progress;
    Node *node;
    cmDetermProgress_t *dprog;

    if (ship->guidance.clampInfo == NULL)
    {
        return;         // not clamped to its creator: not being built there
    }
    if (!playPackets && (ship->playerowner == universe.curPlayerPtr) && (ship->playerowner->aiPlayer == NULL))
    {
        progress = catCmFactoryEntry(ship->catCreator, ship->shiptype);
        if ((progress != NULL) && (progress->nJobs > 0) && (progress->catExternalShip == ship))
        {
            progress->timeFraction = 0;
            progress->timeLeft = 0;
            progress->timeStart = 0;
            progress->nJobsTotal--;
            progress->nJobs--;
            // CAT: not in a recorded multiplayer game either (0x00a7519c && 0x00a75190)
            if (!catNetworkGame())
            {
                progress->catExternalShipStarted = FALSE;
                progress->catExternalShipAttached = FALSE;
            }
            progress->catExternalShip = NULL;
            cmDirtyShipInfo();
        }
    }
    if (!catNetworkGame())
    {
        return;
    }
    for (node = cmDetermProgress.head; node != NULL; node = node->next)
    {
        dprog = (cmDetermProgress_t *)listGetStructOfNode(node);
        if ((dprog->creator == ship->catCreator) && (dprog->catShip == ship))
        {
            dprog->numShips--;
            dprog->ticksLeft = dprog->ticksForOne;
            dprog->catShipStarted = FALSE;
            dprog->catAttached = FALSE;
            progress = catCmFactoryEntry(dprog->creator, dprog->shipType);
            if (progress != NULL)       // CAT-FIX: the exe writes through the entry at address 0 when the creator is no factory
            {
                progress->catExternalShipAttached = FALSE;
                progress->catExternalShipStarted = FALSE;
            }
            dprog->catShip = NULL;
            return;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_cmShipDockedReclampCannon
    Description : CAT: new (0x00455ad0): a Dreadnought that ends its build clamps its cannon again
                  (cat_dreadnoughtReclampCannon 0x005b13b0; Dock.c's CAT_DOCKSTATE_BUILD_ATTACHED)
----------------------------------------------------------------------------*/
void cat_cmShipDockedReclampCannon(Ship *ship)
{
    if (ship->shiptype == sDreadnought)
    {
        cat_dreadnoughtReclampCannon(ship);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_cmCancelJobsOfDeadFactory
    Description : CAT: new (0x00455ea0): the sMothership lost its docking bay module (ModuleShipDied 0x005afe30): every
                  job of an externally built type but a module at that factory is cancelled. A running job refunds
                  its cost so far to the local player (single player) and deletes its ship once it exists (but for a
                  docking bay job; cat_clWrapExternalBuildUpdate mode 1); the entry is cleared; in a multiplayer game
                  the deterministic build is cancelled too (clWrapDeterministicBuild CMD_CANCEL). Then the "jobs
                  cancelled" speech (0x400103b) if any job was, and the screen is redrawn. Not in a tutorial unless
                  its build manager is enabled (tutEnable bit 0x1000000)
----------------------------------------------------------------------------*/
void cat_cmCancelJobsOfDeadFactory(Ship *creator)
{
    shipsinprogress *factory;
    shipinprogress *progress;
    ShipStaticInfo *info;
    sdword index;
    bool32 cancelled = FALSE;

    if (!catCmTutEnabled(0x1000000))
    {
        return;
    }
    factory = cmFindFactory(creator);
    if (factory == NULL)
    {
        return;
    }
    for (index = 0, progress = &factory->progress[0]; index < TOTAL_NUM_SHIPS; index++, progress++)
    {
        info = progress->info;
        if ((info == NULL) || !info->externalBuild || (info->shipclass == CLASS_Component))
        {
            continue;
        }
        if (progress->nJobsTotal != 0)
        {
            // CAT: not in a recorded multiplayer game either (0x00a7519c && 0x00a75190)
            if (!catNetworkGame())
            {
                universe.curPlayerPtr->resourceUnits += (sdword)progress->costSoFar;
            }
            if (progress->info->externalBuild && (progress->info->shiptype != sMothershipDockingBay) &&
                (progress->catExternalShipStarted == 1) && !catNetworkGame())
            {
                if (progress->catExternalShip != NULL)
                {
                    cat_clWrapExternalBuildUpdate(&universe.mainCommandLayer, universe.curPlayerIndex, creator,
                                                  progress->catExternalShip, 0.0f, CAT_EXTBUILD_CANCEL);
                }
                progress->catExternalShipStarted = FALSE;
                progress->catExternalShip = NULL;
            }
        }
        progress->timeFraction = 0;
        progress->timeLeft = 0;
        progress->timeStart = 0;
        progress->nJobs = 0;
        progress->nJobsTotal = 0;
        progress->catExternalShipAttached = FALSE;
        progress->catExternalShipStarted = FALSE;
        progress->catExternalShip = NULL;
        progress->costSoFar = 0;
        cancelled = TRUE;
        if (catNetworkGame() && (progress->info != NULL))
        {
            clWrapDeterministicBuild(CMD_CANCEL, &universe.mainCommandLayer, 0, progress->info->shiptype,
                                     progress->info->shiprace, universe.curPlayerIndex, creator);
        }
    }
    if (cancelled)
    {
        speechEventQueue(NULL, 0x400103b, 0, -1, 0, universe.curPlayerIndex, -1, 5.0f, -1);
    }
    cmDirtyShipInfo();
}

/*-----------------------------------------------------------------------------
    Name        : cat_suShipProvidesSU / cat_suShipAdded / cat_suShipRemoved
    Description : CAT: new (0x00459a60 / 0x00459a90 / 0x00459ae0): the player's support units (Player +0x0C the cap,
                  +0x10 the use). A ship that is not SU exempt (SPECIAL_2_CatExempt20) uses its type's supportUnits;
                  once it is no longer being built it adds its type's providesSU to the cap and is marked
                  SPECIAL_2_CatSUProvided (an externally built ship does that when its build ends, Dock.c
                  CAT_DOCKSTATE_BUILD_ATTACHED); a removed ship gives both back
----------------------------------------------------------------------------*/
void cat_suShipProvidesSU(Ship *ship)
{
    if (!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
    {
        ship->playerowner->supportUnitsMax += ship->staticinfo->providesSU;
        bitSet(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSUProvided);
    }
}

void cat_suShipAdded(Ship *ship, struct Player *player)
{
    if (!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20))
    {
        player->supportUnitsUsed += ship->staticinfo->supportUnits;
        if (!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
        {
            player->supportUnitsMax += ship->staticinfo->providesSU;
            bitSet(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSUProvided);
        }
    }
}

void cat_suShipRemoved(Ship *ship)
{
    Player *player = ship->playerowner;

    if (!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20))
    {
        player->supportUnitsUsed -= ship->staticinfo->supportUnits;
        if (bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSUProvided))
        {
            player->supportUnitsMax -= ship->staticinfo->providesSU;
            bitClear(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSUProvided);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_cmQueuedSupportUnits
    Description : CAT: new (0x00459c20): the support units the open construction screen's queue will use: the jobs
                  set on the screen (cmShipsAvailable's nJobs) and every job of the player's factories (an externally
                  built job whose ship is attached counts as a ship already). 0 while no factory is shown
----------------------------------------------------------------------------*/
sdword cat_cmQueuedSupportUnits(struct Player *player)
{
    shipsinprogress *factory;
    shipinprogress *progress;
    shipavailable *avail;
    Node *node;
    sdword total = 0;
    sdword jobs;
    sdword type;

    if (curshipsInProgress == NULL)
    {
        return 0;
    }
    for (avail = cmShipsAvailable; avail->nJobs != -1; avail++)
    {
        total += GetShipStaticInfo(avail->type, player->race)->supportUnits * avail->nJobs;
    }
    for (node = curshipsInProgress->node.belongto->head; node != NULL; node = node->next)
    {
        factory = (shipsinprogress *)listGetStructOfNode(node);
        if (factory->ship->playerowner != player)
        {
            continue;
        }
        for (type = 0, progress = &factory->progress[0]; type < TOTAL_NUM_SHIPS; type++, progress++)
        {
            jobs = progress->nJobs;
            if ((jobs > 0) && (progress->info->externalBuild == 1) && (progress->catExternalShipAttached == 1))
            {
                jobs--;
            }
            total += GetShipStaticInfo((ShipType)type, player->race)->supportUnits * jobs;
        }
    }
    return total;
}

/*=============================================================================
    HWI-TEST (M2-R6 managers), port diagnostic, not in the exe: /catBuildTest <ticks> (harness mode build;
    docs/m2-r6-managers.md). It runs the headless Mission01 start of /oracleMission (virtual clock, no animatic, the
    oracle stream written, quits after <ticks>) and builds ships through the construction manager with no input:
      tick 16    the local player gets an sMothership (the opening NIS, which the direct start of /oracleMission skips,
                 would give it the Kuun-Lan) at the grid point farthest from the level's ships, made its mothership,
                 and 30,000 RUs; the factory list must hold it (univCreateShip's factory block). Then three jobs are
                 queued at its factory exactly as the construction screen's build button does (cmBuildJobsAdd): the
                 docking bay module if the player has none (an externally built module), an sDestroyer (an externally
                 built ship: it needs the docking bay) and an sAcolyte (built inside)
      every tick an externally built ship is checked when it appears (health 0, progress 0, SPECIAL_2_CatBeingBuilt,
                 clamped to its creator once it has reached its build position) and while its build runs (the health
                 never falls, at most maxhealth); a finished one has full health, progress 1.0, the flag clear and its
                 job gone from the factory; the internally built ship appears at full health when its job ends
      every 256  one "buildtest:" line per job (jobs, time left, ship, health, dock state)
      screens    once every job is finished, the three managers open and close in turn at the mothership, each drawn
                 for 8 frames (offscreen): the construction screen (its factory name drawn), the research screen (its
                 technology list rows counted, a lab drawn), the launch screen (its list rows checked against the ships
                 docked inside, the factory name and the module slots drawn; with a ship in the list, Launch One
                 launches it and it must leave the mothership within 256 ticks); the player's autolaunch bits are
                 cleared at tick 16 (as the dock self-test does) so the internally built acolyte stays docked
      "buildtest: done ... result=PASS|FAIL" after the screens (or at ticks-2), then the mission run's own quit
=============================================================================*/
#define CAT_BUILDTEST_ORDER_TICK    16
#define CAT_BUILDTEST_RUS           30000
#define CAT_BUILDTEST_JOBS          3

static sdword catBuildSelfTestTicks = 0;
static bool32 catBuildSelfTestOrdered = FALSE;
static bool32 catBuildSelfTestDone = FALSE;
static bool32 catBuildSelfTestPass = TRUE;
static Ship *catBuildSelfTestFactory = NULL;
static sdword catBuildSelfTestScreensTick = -1;     // the tick the screens phase began
static Ship *catBuildSelfTestLaunched = NULL;       // the ship Launch One launched
static struct
{
    ShipType type;
    bool32 queued;
    Ship *ship;             // the ship once it exists
    real32 lastHealth;
    bool32 seenAttached;    // an external ship: seen clamped to its creator
    bool32 finished;
    sdword finishTick;
} catBuildSelfTestJob[CAT_BUILDTEST_JOBS];

// a type the level excluded (Mission01 does not load the sDestroyer): its static is loaded now as universeStaticInit
//      would have (the test's own action, not the game's; the dock self-test uses it for its sCarrier)
void catBuildSelfTestLoadStatic(ShipType type)
{
    ShipStaticInfo *info = GetShipStaticInfo(type, RACE_Sect);

    if (bitTest(info->common.infoFlags, IF_InfoLoaded))
    {
        return;
    }
    SetInfoNeededForShipAndRelatedStaticInfo(type, RACE_Sect, TRUE);
    InitStatShipInfo(info, type, RACE_Sect);
    InitStatShipInfoPost(info);
    bitSet(info->common.infoFlags, IF_InfoLoaded);
    cat_univSetShipMaxHealth(info);
    dbgMessagef("buildtest: loaded the static of %s (the level excludes it)", ShipTypeToStr(type));
}

void catBuildSelfTestSet(char *string)
{
    catBuildSelfTestTicks = (sdword)strtol(string, NULL, 10);
    if (catBuildSelfTestTicks <= 0)
    {
        catBuildSelfTestTicks = 7000;
    }
    oracleSetMission(string);                               // the headless Mission01 run with the virtual clock and the quit
}

bool32 catBuildSelfTestRequested(void)
{
    return (catBuildSelfTestTicks > 0);
}

static void catBuildSelfTestExpect(bool32 condition, const char *what, Ship *ship)
{
    if (!condition)
    {
        catBuildSelfTestPass = FALSE;
        dbgMessagef("buildtest: FAIL tick=%u %s (%s#%u)", (unsigned)universe.univUpdateCounter, what,
                    (ship != NULL) ? ShipTypeToStr(ship->shiptype) : "-", (ship != NULL) ? (unsigned)ship->shipID.shipNumber : 0u);
    }
}

// a ship of the type that is the player's, made by the factory and not yet tracked (in the ShipList or inside the factory)
static Ship *catBuildSelfTestFindNew(ShipType type)
{
    Node *node;
    Ship *ship;
    sdword j;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->shiptype != type) || (ship->playerowner != universe.curPlayerPtr) || (ship->catCreator != catBuildSelfTestFactory))
        {
            continue;
        }
        for (j = 0; j < CAT_BUILDTEST_JOBS; j++)
        {
            if (catBuildSelfTestJob[j].ship == ship)
            {
                break;
            }
        }
        if (j == CAT_BUILDTEST_JOBS)
        {
            return ship;
        }
    }
    if ((catBuildSelfTestFactory != NULL) && (catBuildSelfTestFactory->shipsInsideMe != NULL))
    {
        for (node = catBuildSelfTestFactory->shipsInsideMe->insideList.head; node != NULL; node = node->next)
        {
            ship = ((InsideShip *)listGetStructOfNode(node))->ship;
            if ((ship->shiptype == type) && (ship->catCreator == catBuildSelfTestFactory))
            {
                for (j = 0; j < CAT_BUILDTEST_JOBS; j++)
                {
                    if (catBuildSelfTestJob[j].ship == ship)
                    {
                        break;
                    }
                }
                if (j == CAT_BUILDTEST_JOBS)
                {
                    return ship;
                }
            }
        }
    }
    return NULL;
}

static void catBuildSelfTestLog(const char *tag, sdword j)
{
    shipsinprogress *factory = cmFindFactory(catBuildSelfTestFactory);
    shipinprogress *progress = (factory != NULL) ? &factory->progress[catBuildSelfTestJob[j].type] : NULL;
    Ship *ship = catBuildSelfTestJob[j].ship;

    dbgMessagef("buildtest: %s tick=%u %s jobs=%d timeLeft=%d/%d started=%d attached=%d ship=%s#%u health=%.1f/%.1f progress=%.4f built=%d dockstate=%d clamped=%d RUs=%d",
                tag, (unsigned)universe.univUpdateCounter, ShipTypeToStr(catBuildSelfTestJob[j].type),
                (progress != NULL) ? (int)progress->nJobs : -1, (progress != NULL) ? (int)progress->timeLeft : -1,
                (progress != NULL) ? (int)progress->timeStart : -1,
                (progress != NULL) ? (int)progress->catExternalShipStarted : -1, (progress != NULL) ? (int)progress->catExternalShipAttached : -1,
                (ship != NULL) ? ShipTypeToStr(ship->shiptype) : "none", (ship != NULL) ? (unsigned)ship->shipID.shipNumber : 0u,
                (ship != NULL) ? ship->targetable.health : 0.0f, (ship != NULL) ? ship->staticinfo->health.maxhealth : 0.0f,
                (ship != NULL) ? ship->catBuildProgress : 0.0f,
                (ship != NULL) ? (int)!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt) : 0,
                (ship != NULL) ? (int)ship->dockvars.dockstate : -1,
                (ship != NULL) ? (int)(ship->guidance.clampInfo != NULL) : 0, (int)universe.curPlayerPtr->resourceUnits);
}

static void catBuildSelfTestFinish(sdword tick)
{
    Player *player = universe.curPlayerPtr;

    catBuildSelfTestDone = TRUE;
    dbgMessagef("buildtest: done tick=%u driver=%d SU=%d/%d RUs=%d result=%s", (unsigned)tick, (int)CAT_EXTERNAL_BUILD_DRIVER,
                (int)player->supportUnitsUsed, (int)player->supportUnitsMax, (int)player->resourceUnits,
                catBuildSelfTestPass ? "PASS" : "FAIL");
}

// the launch list's rows expected from the ships docked inside the mothership: each listed type once, a class header
//      for the strike craft (fighter / micro) and one for the corvettes (corvette / resource) when any is docked
static sdword catBuildSelfTestLaunchRowsExpected(Ship **firstListed)
{
    bool32 seen[TOTAL_NUM_SHIPS];
    bool32 fighters = FALSE, corvettes = FALSE;
    Node *node;
    Ship *ship;
    sdword rows = 0;
    ShipClass shipclass;

    memset(seen, 0, sizeof(seen));
    *firstListed = NULL;
    for (node = catBuildSelfTestFactory->shipsInsideMe->insideList.head; node != NULL; node = node->next)
    {
        ship = ((InsideShip *)listGetStructOfNode(node))->ship;
        if (!lmShipInList[ship->shiptype] || seen[ship->shiptype])
        {
            continue;
        }
        seen[ship->shiptype] = TRUE;
        rows++;
        if (*firstListed == NULL)
        {
            *firstListed = ship;
        }
        shipclass = ship->staticinfo->shipclass;
        if ((shipclass == CLASS_Corvette) || (shipclass == CLASS_Resource))
        {
            corvettes = TRUE;
        }
        else if ((shipclass == CLASS_Fighter) || (shipclass == CLASS_Micro))
        {
            fighters = TRUE;
        }
    }
    return rows + (fighters ? 1 : 0) + (corvettes ? 1 : 0);
}

static bool32 catBuildSelfTestIsInside(Ship *ship)
{
    Node *node;

    for (node = catBuildSelfTestFactory->shipsInsideMe->insideList.head; node != NULL; node = node->next)
    {
        if (((InsideShip *)listGetStructOfNode(node))->ship == ship)
        {
            return TRUE;
        }
    }
    return FALSE;
}

// the screens phase: s = ticks since it began
static void catBuildSelfTestScreens(sdword tick)
{
    sdword s = tick - catBuildSelfTestScreensTick;
    Ship *first;
    sdword expected;

    if (tick >= catBuildSelfTestTicks - 2)
    {
        catBuildSelfTestExpect(FALSE, "the screens phase ended in time", NULL);
        catBuildSelfTestFinish(tick);
        return;
    }
    switch (s)
    {
        case 0:
            cmConstructionBegin(ghMainRegion, (smemsize)catBuildSelfTestFactory, 0, 0);
            dbgMessagef("buildtest: screens construction open tick=%u base=%d active=%d", (unsigned)tick,
                        (int)(cmBaseRegion != NULL), (int)cmActive);
            catBuildSelfTestExpect((cmBaseRegion != NULL) && cmActive, "the construction screen opens", catBuildSelfTestFactory);
            break;
        case 8:
            dbgMessagef("buildtest: screens construction drawn tick=%u factoryname=%d subar=%d", (unsigned)tick,
                        (int)(cmFactoryNameRegion != NULL), (int)(catCmSUBarRegion != NULL));
            catBuildSelfTestExpect(cmFactoryNameRegion != NULL, "the construction screen draws its factory name", NULL);
            cmClose(NULL, NULL);
            catBuildSelfTestExpect((cmBaseRegion == NULL) && !cmActive && mrRenderMainScreen, "the construction screen closes", NULL);
            break;
        case 9:
            // the player holds one technology (the test's own grant) and has no research module: in single player
            //      the list shows the held row under its lab's header and nothing else (rmUpdateTechList)
            cat_rmTechSetAdd(universe.curPlayerPtr->researchinfo.HasTechnology, TECH_DRIVE_FIGHTER);
            rmResearchGUIBegin(ghMainRegion, 0, 0, 0);
            {
                TechPrintList *row = ((rmTechListWindowHandle != NULL) && (rmTechListWindowHandle->ListTotal == 2) &&
                                      (rmTechListWindowHandle->listofitems.tail != NULL)) ?
                                     (TechPrintList *)((listitemhandle)listGetStructOfNode(rmTechListWindowHandle->listofitems.tail))->data : NULL;

                dbgMessagef("buildtest: screens research open tick=%u base=%d active=%d rows=%d last=%d/%d/%d", (unsigned)tick,
                            (int)(rmBaseRegion != NULL), (int)rmGUIActive,
                            (rmTechListWindowHandle != NULL) ? (int)rmTechListWindowHandle->ListTotal : -1,
                            (row != NULL) ? (int)row->itemtype : -1, (row != NULL) ? (int)row->itemID : -1,
                            (row != NULL) ? (int)row->itemstat : -1);
                catBuildSelfTestExpect((rmBaseRegion != NULL) && (row != NULL) && (row->itemID == TECH_DRIVE_FIGHTER) &&
                                       (row->itemstat == STAT_ALREADYHAVE), "the research list shows the held technology under its header", NULL);
            }
            break;
        case 17:
            dbgMessagef("buildtest: screens research drawn tick=%u modulename=%d lab0=%d", (unsigned)tick,
                        (int)(catRmModuleNameRegion != NULL), (int)(rmLabRegion[0] != NULL));
            catBuildSelfTestExpect(rmLabRegion[0] != NULL, "the research screen draws its labs", NULL);
            rmCloseIfOpen();
            catBuildSelfTestExpect((rmBaseRegion == NULL) && mrRenderMainScreen, "the research screen closes", NULL);
            break;
        case 18:
            lmLaunchBegin(ghMainRegion, (smemsize)catBuildSelfTestFactory, 0, 0);
            expected = catBuildSelfTestLaunchRowsExpected(&first);
            dbgMessagef("buildtest: screens launch open tick=%u base=%d active=%d rows=%d expected=%d first=%s", (unsigned)tick,
                        (int)(lmBaseRegion != NULL), (int)lmActive, (lmListWindow != NULL) ? (int)lmListWindow->ListTotal : -1,
                        (int)expected, (first != NULL) ? ShipTypeToStr(first->shiptype) : "none");
            catBuildSelfTestExpect((lmBaseRegion != NULL) && lmActive && (lmListWindow != NULL), "the launch screen opens", NULL);
            catBuildSelfTestExpect((lmListWindow != NULL) && (lmListWindow->ListTotal == expected), "the launch list holds the docked ships", NULL);
            break;
        case 26:
            expected = catBuildSelfTestLaunchRowsExpected(&first);
            dbgMessagef("buildtest: screens launch drawn tick=%u %s", (unsigned)tick, catLmSelfTestState());
            catBuildSelfTestExpect(catLmSelfTestDrawn(), "the launch screen draws its factory name and module slots", NULL);
            if ((first != NULL) && (lmListWindow != NULL))
            {
                uicListWindowSetCurItem(lmListWindow, uicListFindItemByData(lmListWindow, (ubyte *)(smemsize)first->shiptype));
                catBuildSelfTestLaunched = first;
                catLmSelfTestLaunchOne();
                dbgMessagef("buildtest: screens launch one %s#%u", ShipTypeToStr(first->shiptype), (unsigned)first->shipID.shipNumber);
            }
            lmCloseIfOpen();
            catBuildSelfTestExpect((lmBaseRegion == NULL) && !lmActive && mrRenderMainScreen, "the launch screen closes", NULL);
            break;
        default:
            if (s > 26)
            {
                bool32 inside = (catBuildSelfTestLaunched != NULL) && catBuildSelfTestIsInside(catBuildSelfTestLaunched);

                if (!inside || (s >= 26 + 256))
                {
                    if (catBuildSelfTestLaunched != NULL)
                    {
                        dbgMessagef("buildtest: screens launched %s left=%d after %d ticks dockstate=%d", ShipTypeToStr(catBuildSelfTestLaunched->shiptype),
                                    (int)!inside, (int)(s - 26), (int)catBuildSelfTestLaunched->dockvars.dockstate);
                        catBuildSelfTestExpect(!inside, "the launched ship leaves the mothership", catBuildSelfTestLaunched);
                    }
                    catBuildSelfTestFinish(tick);
                }
            }
            break;
    }
}

void catBuildSelfTestTick(void)
{
    sdword tick = (sdword)universe.univUpdateCounter;
    Player *player = universe.curPlayerPtr;
    shipsinprogress *factory;
    ShipStaticInfo *info;
    Ship *ship;
    sdword j, finished = 0;

    if (catBuildSelfTestDone || (player == NULL))
    {
        return;
    }
    if (!catBuildSelfTestOrdered && (tick >= CAT_BUILDTEST_ORDER_TICK))
    {
        vector base, pos;
        Node *node;
        real32 limit = universe.radius * 0.8f;
        real32 bestdist = -1.0f;
        sdword gx, gy, gz;

        catBuildSelfTestOrdered = TRUE;
        // the grid point (step 10,000) inside 80% of the world sphere farthest from every ship the level put down
        base.x = base.y = base.z = 0.0f;
        for (gx = -10; gx <= 10; gx++)
        {
            for (gy = -10; gy <= 10; gy++)
            {
                for (gz = -3; gz <= 3; gz++)
                {
                    vector cand, diff;
                    real32 nearest = REALlyBig;

                    cand.x = gx * 10000.0f; cand.y = gy * 10000.0f; cand.z = gz * 10000.0f;
                    if (vecMagnitudeSquared(cand) > limit * limit)
                    {
                        continue;
                    }
                    for (node = universe.ShipList.head; node != NULL; node = node->next)
                    {
                        ship = (Ship *)listGetStructOfNode(node);
                        vecSub(diff,ship->obj.posinfo.position,cand);
                        if (vecMagnitudeSquared(diff) < nearest)
                        {
                            nearest = vecMagnitudeSquared(diff);
                        }
                    }
                    if (nearest > bestdist)
                    {
                        bestdist = nearest;
                        base = cand;
                    }
                }
            }
        }
        pos = base;
        catBuildSelfTestFactory = univAddShip(sMothership, RACE_Sect, &pos, player, 0);
        player->PlayerMothership = catBuildSelfTestFactory;
        player->resourceUnits = CAT_BUILDTEST_RUS;
        player->autoLaunch = 0;                             // scripted: the acolyte stays docked for the launch screen
        factory = cmFindFactory(catBuildSelfTestFactory);
        dbgMessagef("buildtest: orders tick=%u sMothership#%u at (%.0f %.0f %.0f) factory=%d dockingbay=%d SU=%d/%d RUs=%d ticks=%d",
                    (unsigned)tick, (unsigned)catBuildSelfTestFactory->shipID.shipNumber, pos.x, pos.y, pos.z, (int)(factory != NULL),
                    (int)cat_playerHasCompletedShip(sMothershipDockingBay, player), (int)player->supportUnitsUsed,
                    (int)player->supportUnitsMax, (int)player->resourceUnits, (int)catBuildSelfTestTicks);
        catBuildSelfTestExpect(factory != NULL, "the sMothership is a factory", catBuildSelfTestFactory);
        if (factory == NULL)
        {
            catBuildSelfTestDone = TRUE;
            dbgMessagef("buildtest: done tick=%u result=FAIL", (unsigned)tick);
            return;
        }
        catBuildSelfTestJob[0].type = sMothershipDockingBay;
        catBuildSelfTestJob[1].type = sDestroyer;
        catBuildSelfTestJob[2].type = sAcolyte;
        for (j = 0; j < CAT_BUILDTEST_JOBS; j++)
        {
            if ((catBuildSelfTestJob[j].type == sMothershipDockingBay) && cat_playerHasCompletedShip(sMothershipDockingBay, player))
            {
                catBuildSelfTestJob[j].finished = TRUE;     // the mothership came with one
                continue;
            }
            catBuildSelfTestLoadStatic(catBuildSelfTestJob[j].type);
            info = GetShipStaticInfo(catBuildSelfTestJob[j].type, RACE_Sect);
            cmBuildJobsAdd(factory, info, 1);
            catBuildSelfTestJob[j].queued = TRUE;
            dbgMessagef("buildtest: queued %s external=%d buildTime=%d cost=%d", ShipTypeToStr(info->shiptype),
                        (int)info->externalBuild, (int)info->buildTime, (int)info->buildCost);
        }
        return;
    }
    if (!catBuildSelfTestOrdered)
    {
        return;
    }
    if (catBuildSelfTestScreensTick >= 0)
    {
        catBuildSelfTestScreens(tick);
        return;
    }
    factory = cmFindFactory(catBuildSelfTestFactory);
    for (j = 0; j < CAT_BUILDTEST_JOBS; j++)
    {
        if (catBuildSelfTestJob[j].finished)
        {
            finished++;
            continue;
        }
        info = GetShipStaticInfo(catBuildSelfTestJob[j].type, RACE_Sect);
        ship = catBuildSelfTestJob[j].ship;
        if (ship == NULL)
        {
            ship = catBuildSelfTestFindNew(catBuildSelfTestJob[j].type);
            if (ship == NULL)
            {
                continue;
            }
            catBuildSelfTestJob[j].ship = ship;
            catBuildSelfTestJob[j].lastHealth = ship->targetable.health;
            catBuildSelfTestLog("appeared", j);
            if (info->externalBuild)
            {
                catBuildSelfTestExpect(ship->targetable.health == 0.0f, "an external ship appears at health 0", ship);
                catBuildSelfTestExpect(ship->catBuildProgress == 0.0f, "an external ship appears at progress 0", ship);
                catBuildSelfTestExpect(bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt), "an external ship appears being built", ship);
            }
            else
            {
                catBuildSelfTestExpect(ship->targetable.health == info->health.maxhealth, "an internal ship appears at full health", ship);
                catBuildSelfTestExpect((factory != NULL) && (factory->progress[info->shiptype].nJobs == 0), "the internal job is gone", ship);
                catBuildSelfTestJob[j].finished = TRUE;
                catBuildSelfTestJob[j].finishTick = tick;
                finished++;
                continue;
            }
        }
        if (info->externalBuild)
        {
            catBuildSelfTestExpect(ship->targetable.health >= catBuildSelfTestJob[j].lastHealth, "the health never falls", ship);
            catBuildSelfTestExpect(ship->targetable.health <= info->health.maxhealth, "the health stays at most maxhealth", ship);
            catBuildSelfTestJob[j].lastHealth = ship->targetable.health;
            if (bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt) && (ship->guidance.clampInfo != NULL) &&
                (ship->guidance.clampInfo->host == (SpaceObjRotImpTarg *)catBuildSelfTestFactory))
            {
                if (!catBuildSelfTestJob[j].seenAttached)
                {
                    catBuildSelfTestJob[j].seenAttached = TRUE;
                    catBuildSelfTestLog("attached", j);
                }
            }
            if (!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
            {
                catBuildSelfTestLog("finished", j);
                catBuildSelfTestExpect(catBuildSelfTestJob[j].seenAttached, "the ship was clamped to its creator while built", ship);
                // the health is the sum of the per-tick steps (cat_clExternalBuildUpdate), so a float rounding short
                //      of maxhealth (the exe adds the same steps)
                catBuildSelfTestExpect(ship->targetable.health >= info->health.maxhealth * 0.999f, "a finished ship has full health", ship);
                catBuildSelfTestExpect(ship->catBuildProgress == 1.0f, "a finished ship has progress 1.0", ship);
                catBuildSelfTestExpect((factory != NULL) && (factory->progress[info->shiptype].nJobs == 0) &&
                                       !factory->progress[info->shiptype].catExternalShipStarted &&
                                       (factory->progress[info->shiptype].catExternalShip == NULL), "the external job is gone", ship);
                catBuildSelfTestJob[j].finished = TRUE;
                catBuildSelfTestJob[j].finishTick = tick;
                finished++;
            }
        }
    }
    if ((tick % 256) == 0)
    {
        for (j = 0; j < CAT_BUILDTEST_JOBS; j++)
        {
            if (catBuildSelfTestJob[j].queued)
            {
                catBuildSelfTestLog("tick", j);
            }
        }
    }
    if ((finished == CAT_BUILDTEST_JOBS) || (tick >= catBuildSelfTestTicks - 2))
    {
        for (j = 0; j < CAT_BUILDTEST_JOBS; j++)
        {
            catBuildSelfTestExpect(catBuildSelfTestJob[j].finished, "every job finished", catBuildSelfTestJob[j].ship);
            if (catBuildSelfTestJob[j].queued)
            {
                dbgMessagef("buildtest: job %s finished=%d at tick %d", ShipTypeToStr(catBuildSelfTestJob[j].type),
                            (int)catBuildSelfTestJob[j].finished, (int)catBuildSelfTestJob[j].finishTick);
            }
        }
        if (finished == CAT_BUILDTEST_JOBS)
        {
            catBuildSelfTestScreensTick = tick + 1;
            return;
        }
        catBuildSelfTestFinish(tick);
    }
}
