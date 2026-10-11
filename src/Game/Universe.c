/*=============================================================================
    Name    : Universe.c
    Purpose : The universe represents the entire game world, including any
              mission spheres contained within.

    Created 6/19/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "Universe.h"
#include "Salvage.h"
#include "Blast.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "AIPlayer.h"
#include "AIShip.h"
#include "AutoDownloadMap.h"
#include "Bounties.h"
#include "Captaincy.h"
#include "Collision.h"
#include "CommandNetwork.h"
#include "Debug.h"
#include "Dock.h"
#include "ETG.h"
#include "FastMath.h"
#include "File.h"
#include "Formation.h"
#include "GameChat.h"
#include "Gun.h"
#include "HorseRace.h"
#include "LagPrint.h"
#include "LevelLoad.h"
#include "LinkedList.h"
#include "Memory.h"
#include "MeshAnim.h"
#include "MEX.h"
#include "MultiplayerGame.h"
#include "NavLights.h"
#include "NetCheck.h"
#include "Ping.h"
#include "render.h"
#include "SaveGame.h"
#include "ScenPick.h"
#include "Ships.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "Stats.h"
#include "StatScript.h"
#include "StringSupport.h"
#include "Task.h"
#include "Teams.h"
#include "Tutor.h"
#include "Tweak.h"
#include "UnivUpdate.h"
#include "OracleDump.h"     // HWI-ORACLE: ORACLE_UNIV_UPDATE at the univUpdate call sites of universeUpdateTask (hook H1; 3 since M2-R3, as in the exe)
#include "utility.h"

#ifdef _MSC_VER
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif



extern bool32 cheapShips;

extern real32 GLOBAL_SHIP_HEALTH_MODIFIER;
extern real32 GLOBAL_SHIP_SPEED_MODIFIER;

/*=============================================================================
    Public Data:
=============================================================================*/

// CAT: 16 Hz universe by default (universeUpdateTask 0x0053e460 calls univUpdate(0.0625f), push 0x3D800000 at
//      0x0053e8a5, 0x0053e8db, 0x0053e9b5; user decision 2 of 2026-10-10). GoK defaults to 2 (64 Hz); /gok64Hz restores it.
ubyte UNIVERSE_UPDATE_RATE_SHIFT = 0;

Universe universe;
AsteroidStaticInfo asteroidStaticInfos[NUM_ASTEROIDTYPES];
DustCloudStaticInfo dustcloudStaticInfos[NUM_DUSTCLOUDTYPES];
GasCloudStaticInfo gascloudStaticInfos[NUM_GASCLOUDTYPES];
NebulaStaticInfo nebulaStaticInfos[NUM_NEBULATYPES];

DerelictStaticInfo derelictStaticInfos[NUM_DERELICTTYPES];

MissileStaticInfo missileStaticInfos[NUM_RACES];
// CAT: indexed by any race (SetInfoNeededForShipAndRelatedStaticInfo 0x004a29f0 sets mineStaticInfos[race] for
//      tMinelayerCorvette, race RACE_Taiidan); HW1 had [2] ("only race 1 and race 2 have mines")
MissileStaticInfo mineStaticInfos[NUM_RACES];

GrowSelection ClampedShipList;       // special GrowSelect which can have NULL's in it

struct SphereStaticInfo *sphereStaticInfo = NULL;

Camera defaultCamera;

real32 maxMineCollSphereSize;
//real32 maxShipCollSphereSize;
//real32 maxResourceCollSphereSize;
//real32 maxDerelictCollSphereSize;
//real32 maxCollSphereSize;

meshdata *defaultmesh = NULL;

ubyte turboTimeCompressionFactor = 8;  // same as Homeworld Cataclysm
bool32 universeTurbo = FALSE;
bool32 universePlaybackNoPacket = FALSE;    // CAT: 0x00a6c5c8, set when a recorded-game playback pass finds no packet
bool32 universePause = FALSE;

sdword cdMaxShipsAllowed;            // max number of ships allowed
sdword cdLimitCaps[TOTAL_NUM_SHIPS]; // max number of ships per player allowed
sdword cdClassCaps[NUM_CLASSES];     // max number of ships per class allowed
bool32   cdEnabled=TRUE;               // flag specifying whether unit caps enabled

// CAT: which race may own each type (RacesAllowedForGivenShip 0x008b1814, read by aiuCanBuildShipType 0x00412cb0 and
//      aifBuildRequestedShips 0x004045b0): each type's own race, except trGhostship (Sect), dAcolyte (Traders) and
//      the three nis* light sources (none)
uword RacesAllowedForGivenShip[TOTAL_NUM_SHIPS] =
{
    RaceToRaceBits(RACE_Sect),         //sRecon
    RaceToRaceBits(RACE_Sect),         //sAcolyte
    RaceToRaceBits(RACE_Sect),         //sMimic
    RaceToRaceBits(RACE_Sect),         //sLeech
    RaceToRaceBits(RACE_Sect),         //sACV
    RaceToRaceBits(RACE_Sect),         //sMCV
    RaceToRaceBits(RACE_Sect),         //sRammingFrigate
    RaceToRaceBits(RACE_Sect),         //sMultiBeamFrigate
    RaceToRaceBits(RACE_Sect),         //sHiveFrigate
    RaceToRaceBits(RACE_Sect),         //sSwarmer
    RaceToRaceBits(RACE_Sect),         //sDreadnought
    RaceToRaceBits(RACE_Sect),         //sDreadnoughtCannon
    RaceToRaceBits(RACE_Sect),         //sCarrier
    RaceToRaceBits(RACE_Sect),         //sCarrierSupport
    RaceToRaceBits(RACE_Sect),         //sWorker
    RaceToRaceBits(RACE_Sect),         //sRepairDroid
    RaceToRaceBits(RACE_Sect),         //sProcessor
    RaceToRaceBits(RACE_Sect),         //sSentinel
    RaceToRaceBits(RACE_Sect),         //sMothership
    RaceToRaceBits(RACE_Sect),         //sMothershipSpecial
    RaceToRaceBits(RACE_Sect),         //sMothershipEngineering
    RaceToRaceBits(RACE_Sect),         //sMothershipArmour
    RaceToRaceBits(RACE_Sect),         //sMothershipMicro
    RaceToRaceBits(RACE_Sect),         //sMothershipWeapons
    RaceToRaceBits(RACE_Sect),         //sMothershipDockingBay
    RaceToRaceBits(RACE_Sect),         //sDestroyer
    RaceToRaceBits(RACE_Sect),         //sMothershipSupport
    RaceToRaceBits(RACE_Sect),         //sMothershipCommand
    RaceToRaceBits(RACE_Sect),         //sMothershipEnginesLower
    RaceToRaceBits(RACE_Sect),         //sMothershipEnginesUpper
    RaceToRaceBits(RACE_Sect),         //sMothershipLowerSpine
    RaceToRaceBits(RACE_Sect),         //sMothershipBigGun
    RaceToRaceBits(RACE_Sect),         //sMothershipOreCanArmour
    RaceToRaceBits(RACE_Beast),        //bRecon
    RaceToRaceBits(RACE_Beast),        //bAcolyte
    RaceToRaceBits(RACE_Beast),        //bInterceptor
    RaceToRaceBits(RACE_Beast),        //bCloakedFighter
    RaceToRaceBits(RACE_Beast),        //bAttackBomber
    RaceToRaceBits(RACE_Beast),        //bACV
    RaceToRaceBits(RACE_Beast),        //bMultiGunCorvette
    RaceToRaceBits(RACE_Beast),        //bHeavyCorvette
    RaceToRaceBits(RACE_Beast),        //bMultiBeamFrigate
    RaceToRaceBits(RACE_Beast),        //bHiveFrigate
    RaceToRaceBits(RACE_Beast),        //bSwarmer
    RaceToRaceBits(RACE_Beast),        //bIonArrayFrigate
    RaceToRaceBits(RACE_Beast),        //bDFGFrigate
    RaceToRaceBits(RACE_Beast),        //bHeavyCruiser
    RaceToRaceBits(RACE_Beast),        //bCarrier
    RaceToRaceBits(RACE_Beast),        //bRammingFrigate
    RaceToRaceBits(RACE_Beast),        //bWorker
    RaceToRaceBits(RACE_Beast),        //bRepairDroid
    RaceToRaceBits(RACE_Beast),        //bProcessor
    RaceToRaceBits(RACE_Beast),        //bSentinel
    RaceToRaceBits(RACE_Beast),        //bMothership
    RaceToRaceBits(RACE_Beast),        //bMothershipDockingBay
    RaceToRaceBits(RACE_Beast),        //bMothershipEnginesLower
    RaceToRaceBits(RACE_Beast),        //bSectMoship
    RaceToRaceBits(RACE_Beast),        //bMothershipSupport
    RaceToRaceBits(RACE_Beast),        //bCruiseMissile
    RaceToRaceBits(RACE_Beast),        //bMissileCorvette
    RaceToRaceBits(RACE_Kushan),       //kAdvanceSupportFrigate
    RaceToRaceBits(RACE_Kushan),       //kAttackBomber
    RaceToRaceBits(RACE_Kushan),       //kCarrier
    RaceToRaceBits(RACE_Kushan),       //kCloakGenerator
    RaceToRaceBits(RACE_Kushan),       //kGravWellGenerator
    RaceToRaceBits(RACE_Kushan),       //kHeavyCorvette
    RaceToRaceBits(RACE_Kushan),       //kHeavyCruiser
    RaceToRaceBits(RACE_Kushan),       //kHeavyDefender
    RaceToRaceBits(RACE_Kushan),       //kHeavyInterceptor
    RaceToRaceBits(RACE_Kushan),       //kIonCannonFrigate
    RaceToRaceBits(RACE_Kushan),       //kMultiGunCorvette
    RaceToRaceBits(RACE_Kushan),       //kProximitySensor
    RaceToRaceBits(RACE_Kushan),       //kSalCapCorvette
    RaceToRaceBits(RACE_Kushan),       //kStandardDestroyer
    RaceToRaceBits(RACE_Taiidan),      //tAdvanceSupportFrigate
    RaceToRaceBits(RACE_Taiidan),      //tAttackBomber
    RaceToRaceBits(RACE_Taiidan),      //tCarrier
    RaceToRaceBits(RACE_Taiidan),      //tCloakGenerator
    RaceToRaceBits(RACE_Taiidan),      //tDFGFrigate
    RaceToRaceBits(RACE_Taiidan),      //tGravWellGenerator
    RaceToRaceBits(RACE_Taiidan),      //tHeavyCruiser
    RaceToRaceBits(RACE_Taiidan),      //tHeavyDefender
    RaceToRaceBits(RACE_Taiidan),      //tHeavyInterceptor
    RaceToRaceBits(RACE_Taiidan),      //tIonCannonFrigate
    RaceToRaceBits(RACE_Taiidan),      //tMinelayerCorvette
    RaceToRaceBits(RACE_Taiidan),      //tMissileDestroyer
    RaceToRaceBits(RACE_Taiidan),      //tMultiGunCorvette
    RaceToRaceBits(RACE_Taiidan),      //tProximitySensor
    RaceToRaceBits(RACE_Taiidan),      //tResourceCollector
    RaceToRaceBits(RACE_Taiidan),      //tSalCapCorvette
    RaceToRaceBits(RACE_Taiidan),      //tStandardDestroyer
    RaceToRaceBits(RACE_Taiidan),      //tStandardFrigate
    RaceToRaceBits(RACE_P1),           //p1Outpost
    RaceToRaceBits(RACE_P1),           //p1Fighter
    RaceToRaceBits(RACE_P1),           //p1IonArrayFrigate
    RaceToRaceBits(RACE_P1),           //p1MissileCorvette
    RaceToRaceBits(RACE_P1),           //p1Mothership
    RaceToRaceBits(RACE_P1),           //p1StandardCorvette
    RaceToRaceBits(RACE_Traders),      //trFloatingCity
    RaceToRaceBits(RACE_Traders),      //trCargoBarge
    RaceToRaceBits(RACE_Traders),      //trMiningBase
    RaceToRaceBits(RACE_Traders),      //trResearchStation
    RaceToRaceBits(RACE_Traders),      //trJunkYardDawg
    RaceToRaceBits(RACE_Traders),      //trJunkYardHQ
    RaceToRaceBits(RACE_Sect),         //trGhostship
    RaceToRaceBits(RACE_Traders),      //trJunk_LGun
    RaceToRaceBits(RACE_Traders),      //trJunk_SGun
    RaceToRaceBits(RACE_Traders),      //trResearchStationBridge
    RaceToRaceBits(RACE_Traders),      //trResearchStationTower
    RaceToRaceBits(RACE_Derelict),     //dRKO
    RaceToRaceBits(RACE_Derelict),     //dBigGun
    RaceToRaceBits(RACE_Derelict),     //dBigGunUnit
    RaceToRaceBits(RACE_Derelict),     //dSlipgateSmall
    RaceToRaceBits(RACE_Derelict),     //dSlipgateMedium
    RaceToRaceBits(RACE_Derelict),     //dSlipgateLarge
    RaceToRaceBits(RACE_Derelict),     //dCrate
    RaceToRaceBits(RACE_Derelict),     //dJunk1PartA
    RaceToRaceBits(RACE_Derelict),     //dJunk1Shell
    RaceToRaceBits(RACE_Derelict),     //dJunk1Strut
    RaceToRaceBits(RACE_Derelict),     //dJunk2Shipwreck
    RaceToRaceBits(RACE_Derelict),     //dJunk3Boiler
    RaceToRaceBits(RACE_Derelict),     //dJunk3BoilerCasing
    RaceToRaceBits(RACE_Derelict),     //dMessagePod
    RaceToRaceBits(RACE_Derelict),     //dBuoy
    RaceToRaceBits(RACE_Derelict),     //dFirstPlanet
    RaceToRaceBits(RACE_Derelict),     //dDeadPlanet
    RaceToRaceBits(RACE_Derelict),     //dCrystal0
    RaceToRaceBits(RACE_Derelict),     //dCrystal1
    RaceToRaceBits(RACE_Derelict),     //dBeastDockingBay
    RaceToRaceBits(RACE_Derelict),     //dNISBuoy
    RaceToRaceBits(RACE_Derelict),     //dBeastMothership
    RaceToRaceBits(RACE_Derelict),     //dNomadMoon
    RaceToRaceBits(RACE_Derelict),     //dNomadFieldEmitter
    RaceToRaceBits(RACE_Traders),      //dAcolyte
    RaceToRaceBits(RACE_Derelict),     //dPrisonShip
    RaceToRaceBits(RACE_Derelict),     //dPrisonShipInfected
    RaceToRaceBits(RACE_Derelict),     //dSlipgateNode
    RaceToRaceBits(RACE_Derelict),     //dKushanCarrier
    RaceToRaceBits(RACE_Derelict),     //dIonArrayFrigate
    RaceToRaceBits(RACE_Derelict),     //dIonCannonFrigate
    RaceToRaceBits(RACE_Derelict),     //dEcclesiam
    RaceToRaceBits(RACE_Derelict),     //dEcclesiamInfected
    0,                                 //nisLightSource1
    0,                                 //nisLightSource2
    0                                  //nisLensflareSource1
};

bool32 universeForceDefaultShip = FALSE;   //flag for forcing ships to be loaded as rave borgs

// CAT: /catShipStaticsTest (port diagnostic, not in the exe): load the .shp of every ship type at start-up, log one
//      line per ship and exit; see catShipStaticsRunSelfTest
bool32 catShipStaticsSelfTest = FALSE;
// CAT: port diagnostic (M2-R3 ships-a): with /catShipStaticsTest, also run each type's CustShipStaticInit, so every
//      StaticInit of the 145 types is exercised once (logged "staticinit=next" instead of "skipped")
bool32 catShipStaticsRunStaticInit = FALSE;
sdword catShipStaticsSkip[CAT_SHIPSTATICS_MAXSKIP];    // CAT: /catShipStaticsSkip (types the self-test leaves out)
sdword catShipStaticsSkipCount = 0;

#if UNIV_SHIP_LOADFREE_LOG
bool32 univLoadFreeLog = FALSE;
#endif

/*=============================================================================
    Private Function prototypes:
=============================================================================*/

static void loadMexDataCB(char *directory,char *field,void *dataToFillIn);
//static void setRaceCB(char *directory,char *field,void *dataToFillIn);
static void setClassCB(char *directory,char *field,void *dataToFillIn);
static void cat_scriptSetBeastAttackConvertToCB(char *directory,char *field,void *dataToFillIn);   // CAT: new
static void scriptSetFramesCB(char *directory,char *field,void *dataToFillIn);
static void LODScriptLoad(char *directory,char *field,void *dataToFillIn);
static void setGunBindInfo(char *directory,char *field,void *dataToFillIn);
static void setAnimationBindInfo(char *directory,char *field,void *dataToFillIn);
static void setMadStartInfo(char *directory,char *field,void *dataToFillIn);
static void cat_madNumberOfAnimsSetCB(char *directory,char *field,void *dataToFillIn);   // CAT: new (0x00539e40)
static void setMadAnim(char *directory,char *field,void *dataToFillIn);                  // CAT: new (0x00539f70)
static void setGunAnimInfo(char *directory,char *field,void *dataToFillIn);
static void setDockAnimInfo(char *directory,char *field,void *dataToFillIn);
static void setMothershipDoorAnimInfo(char *directory,char *field,void *dataToFillIn);
static void setSpecialCaseAnimInfo(char *directory,char *field,void *dataToFillIn);

/*=============================================================================
    Private Data:
=============================================================================*/

lodinfo *defaultlod = NULL;         // remove later when all stubs are taken out
void *defaultmex = NULL;            // remove later when all stubs are taken out

udword universeUpdateCounter = 0;

// CAT: one flat ShipStaticInfo array indexed by type alone (ShipStaticInfos 0x00b3b420, 145 entries; GetShipStaticInfo
//      0x0053ea50 takes no race). HW1 had one array per race.
ShipStaticInfo ShipStaticInfos[TOTAL_NUM_SHIPS];

// CAT: one CustShipHeader per type (shipHeaders 0x008b1938). Where the exe uses a header HW1 also has, it is used here;
//      the 22 Cataclysm-only headers (Acolyte, Mimic, Leech, ACV, MCV, ...) are in their own Ships/ files since M2-R4
//      ships-b. The two cloak generators keep DefaultShipHeader where the exe has NULL (see their rows).
CustShipHeader *shipHeaders[TOTAL_NUM_SHIPS] =
{
    &GenericInterceptorHeader,         //sRecon
    &AcolyteHeader,                    //sAcolyte
    &MimicHeader,                      //sMimic
    &LeechHeader,                      //sLeech
    &ACVHeader,                        //sACV
    &MCVHeader,                        //sMCV
    &RammingFrigateHeader,             //sRammingFrigate
    &MultiBeamFrigateHeader,           //sMultiBeamFrigate
    &HiveFrigateHeader,                //sHiveFrigate
    &SwarmerHeader,                    //sSwarmer
    &DreadnoughtHeader,                //sDreadnought
    &ModuleShipHeader,                 //sDreadnoughtCannon
    &CarrierHeader,                    //sCarrier
    &ModuleShipHeader,                 //sCarrierSupport
    &WorkerHeader,                     //sWorker
    &RepairDroidHeader,                //sRepairDroid
    &ProcessorHeader,                  //sProcessor
    &SentinelHeader,                   //sSentinel
    &MothershipHeader,                 //sMothership
    &ModuleShipHeader,                 //sMothershipSpecial
    &ModuleShipHeader,                 //sMothershipEngineering
    &ModuleShipHeader,                 //sMothershipArmour
    &ModuleShipHeader,                 //sMothershipMicro
    &ModuleShipHeader,                 //sMothershipWeapons
    &ModuleShipHeader,                 //sMothershipDockingBay
    &StandardDestroyerHeader,          //sDestroyer
    &ModuleShipHeader,                 //sMothershipSupport
    &ModuleShipHeader,                 //sMothershipCommand
    &ModuleShipHeader,                 //sMothershipEnginesLower
    &ModuleShipHeader,                 //sMothershipEnginesUpper
    &ModuleShipHeader,                 //sMothershipLowerSpine
    &ModuleShipHeader,                 //sMothershipBigGun
    &ModuleShipHeader,                 //sMothershipOreCanArmour
    &GenericInterceptorHeader,         //bRecon
    &AcolyteHeader,                    //bAcolyte
    &GenericInterceptorHeader,         //bInterceptor
    &CloakedFighterHeader,             //bCloakedFighter
    &AttackBomberHeader,               //bAttackBomber
    &ACVHeader,                        //bACV
    &MultiGunCorvetteHeader,           //bMultiGunCorvette
    &HeavyCorvetteHeader,              //bHeavyCorvette
    &MultiBeamFrigateHeader,           //bMultiBeamFrigate
    &HiveFrigateHeader,                //bHiveFrigate
    &SwarmerHeader,                    //bSwarmer
    &P1IonArrayFrigateHeader,          //bIonArrayFrigate
    &DFGFrigateHeader,                 //bDFGFrigate
    &HeavyCruiserHeader,               //bHeavyCruiser
    &CarrierHeader,                    //bCarrier
    &RammingFrigateHeader,             //bRammingFrigate
    &WorkerHeader,                     //bWorker
    &RepairDroidHeader,                //bRepairDroid
    &ProcessorHeader,                  //bProcessor
    &SentinelHeader,                   //bSentinel
    &MothershipHeader,                 //bMothership
    &ModuleShipHeader,                 //bMothershipDockingBay
    &ModuleShipHeader,                 //bMothershipEnginesLower
    &ModuleShipHeader,                 //bSectMoship
    &ModuleShipHeader,                 //bMothershipSupport
    &CruiseMissileHeader,              //bCruiseMissile
    &P1MissileCorvetteHeader,          //bMissileCorvette
    &AdvanceSupportFrigateHeader,      //kAdvanceSupportFrigate
    &AttackBomberHeader,               //kAttackBomber
    &CarrierHeader,                    //kCarrier
    &DefaultShipHeader,              //kCloakGenerator: NULL in the exe (InitStatShipInfo would crash on it); DefaultShipHeader keeps the port safe
    &GravWellGeneratorHeader,          //kGravWellGenerator
    &HeavyCorvetteHeader,              //kHeavyCorvette
    &HeavyCruiserHeader,               //kHeavyCruiser
    &GenericDefenderHeader,            //kHeavyDefender
    &GenericInterceptorHeader,         //kHeavyInterceptor
    &IonCannonFrigateHeader,           //kIonCannonFrigate
    &MultiGunCorvetteHeader,           //kMultiGunCorvette
    &ProximitySensorHeader,            //kProximitySensor
    &SalCapCorvetteHeader,             //kSalCapCorvette
    &StandardDestroyerHeader,          //kStandardDestroyer
    &AdvanceSupportFrigateHeader,      //tAdvanceSupportFrigate
    &AttackBomberHeader,               //tAttackBomber
    &CarrierHeader,                    //tCarrier
    &DefaultShipHeader,              //tCloakGenerator: NULL in the exe (InitStatShipInfo would crash on it); DefaultShipHeader keeps the port safe
    &DFGFrigateHeader,                 //tDFGFrigate
    &GravWellGeneratorHeader,          //tGravWellGenerator
    &HeavyCruiserHeader,               //tHeavyCruiser
    &GenericDefenderHeader,            //tHeavyDefender
    &GenericInterceptorHeader,         //tHeavyInterceptor
    &IonCannonFrigateHeader,           //tIonCannonFrigate
    &MinelayerCorvetteHeader,          //tMinelayerCorvette
    &MissileDestroyerHeader,           //tMissileDestroyer
    &MultiGunCorvetteHeader,           //tMultiGunCorvette
    &ProximitySensorHeader,            //tProximitySensor
    &ResourceCollectorHeader,          //tResourceCollector
    &SalCapCorvetteHeader,             //tSalCapCorvette
    &StandardDestroyerHeader,          //tStandardDestroyer
    &StandardFrigateHeader,            //tStandardFrigate
    &DoNothingShipHeader,              //p1Outpost
    &GenericInterceptorHeader,         //p1Fighter
    &P1IonArrayFrigateHeader,          //p1IonArrayFrigate
    &P1MissileCorvetteHeader,          //p1MissileCorvette
    &P1MothershipHeader,               //p1Mothership
    &P1StandardCorvetteHeader,         //p1StandardCorvette
    &FloatingCityHeader,               //trFloatingCity
    &DefaultShipHeader,                //trCargoBarge
    &MiningBaseHeader,                 //trMiningBase
    &DefaultShipHeader,                //trResearchStation
    &DoNothingShipHeader,              //trJunkYardDawg
    &DoNothingShipHeader,              //trJunkYardHQ
    &MothershipHeader,                 //trGhostship
    &StationaryGunHeader,              //trJunk_LGun
    &StationaryGunHeader,              //trJunk_SGun
    &DoNothingShipHeader,              //trResearchStationBridge
    &DoNothingShipHeader,              //trResearchStationTower
    &RKOHeader,                        //dRKO
    &DoNothingShipHeader,              //dBigGun
    &DoNothingShipHeader,              //dBigGunUnit
    &SlipgateHeader,                   //dSlipgateSmall
    &SlipgateHeader,                   //dSlipgateMedium
    &SlipgateHeader,                   //dSlipgateLarge
    &DoNothingShipHeader,              //dCrate
    &DoNothingShipHeader,              //dJunk1PartA
    &DoNothingShipHeader,              //dJunk1Shell
    &DoNothingShipHeader,              //dJunk1Strut
    &DoNothingShipHeader,              //dJunk2Shipwreck
    &DoNothingShipHeader,              //dJunk3Boiler
    &DoNothingShipHeader,              //dJunk3BoilerCasing
    &DoNothingShipHeader,              //dMessagePod
    &DoNothingShipHeader,              //dBuoy
    &DoNothingShipHeader,              //dFirstPlanet
    &DoNothingShipHeader,              //dDeadPlanet
    &CrystalHeader,                    //dCrystal0
    &CrystalHeader,                    //dCrystal1
    &DoNothingShipHeader,              //dBeastDockingBay
    &DoNothingShipHeader,              //dNISBuoy
    &dBeastMothershipHeader,           //dBeastMothership
    &NomadMoonHeader,                  //dNomadMoon
    &NomadFieldEmitterHeader,          //dNomadFieldEmitter
    &AcolyteHeader,                    //dAcolyte
    &DoNothingShipHeader,              //dPrisonShip
    &DoNothingShipHeader,              //dPrisonShipInfected
    &DoNothingShipHeader,              //dSlipgateNode
    &CarrierHeader,                    //dKushanCarrier
    &IonCannonFrigateHeader,           //dIonArrayFrigate
    &IonCannonFrigateHeader,           //dIonCannonFrigate
    &DoNothingShipHeader,              //dEcclesiam
    &DoNothingShipHeader,              //dEcclesiamInfected
    &DoNothingShipHeader,              //nisLightSource1
    &DoNothingShipHeader,              //nisLightSource2
    &DoNothingShipHeader               //nisLensflareSource1
};

// CAT: values per Cataclysm type (rowAvoidBy 0x008b1b7c)
real32 rowAvoidBy[TOTAL_NUM_SHIPS] =
{
    0.0,             //sRecon
    0.0,             //sAcolyte
    0.0,             //sMimic
    0.0,             //sLeech
    0.0,             //sACV
    0.0,             //sMCV
    1000.0,          //sRammingFrigate
    1000.0,          //sMultiBeamFrigate
    1000.0,          //sHiveFrigate
    0.0,             //sSwarmer
    1000.0,          //sDreadnought
    1000.0,          //sDreadnoughtCannon
    1600.0,          //sCarrier
    1600.0,          //sCarrierSupport
    1000.0,          //sWorker
    0.0,             //sRepairDroid
    1000.0,          //sProcessor
    0.0,             //sSentinel
    2000.0,          //sMothership
    2000.0,          //sMothershipSpecial
    2000.0,          //sMothershipEngineering
    2000.0,          //sMothershipArmour
    2000.0,          //sMothershipMicro
    2000.0,          //sMothershipWeapons
    2000.0,          //sMothershipDockingBay
    800.0,           //sDestroyer
    2000.0,          //sMothershipSupport
    2000.0,          //sMothershipCommand
    2000.0,          //sMothershipEnginesLower
    2000.0,          //sMothershipEnginesUpper
    2000.0,          //sMothershipLowerSpine
    2000.0,          //sMothershipBigGun
    2000.0,          //sMothershipOreCanArmour
    0.0,             //bRecon
    0.0,             //bAcolyte
    0.0,             //bInterceptor
    0.0,             //bCloakedFighter
    0.0,             //bAttackBomber
    0.0,             //bACV
    0.0,             //bMultiGunCorvette
    0.0,             //bHeavyCorvette
    1000.0,          //bMultiBeamFrigate
    1000.0,          //bHiveFrigate
    0.0,             //bSwarmer
    1000.0,          //bIonArrayFrigate
    1000.0,          //bDFGFrigate
    1500.0,          //bHeavyCruiser
    1600.0,          //bCarrier
    1000.0,          //bRammingFrigate
    1000.0,          //bWorker
    0.0,             //bRepairDroid
    1000.0,          //bProcessor
    0.0,             //bSentinel
    2000.0,          //bMothership
    2000.0,          //bMothershipDockingBay
    2000.0,          //bMothershipEnginesLower
    2000.0,          //bSectMoship
    2000.0,          //bMothershipSupport
    0.0,             //bCruiseMissile
    2000.0,          //bMissileCorvette
    1000.0,          //kAdvanceSupportFrigate
    0.0,             //kAttackBomber
    1600.0,          //kCarrier
    1000.0,          //kCloakGenerator
    1000.0,          //kGravWellGenerator
    0.0,             //kHeavyCorvette
    1500.0,          //kHeavyCruiser
    0.0,             //kHeavyDefender
    0.0,             //kHeavyInterceptor
    1000.0,          //kIonCannonFrigate
    0.0,             //kMultiGunCorvette
    0.0,             //kProximitySensor
    0.0,             //kSalCapCorvette
    1500.0,          //kStandardDestroyer
    1000.0,          //tAdvanceSupportFrigate
    0.0,             //tAttackBomber
    1600.0,          //tCarrier
    1000.0,          //tCloakGenerator
    1000.0,          //tDFGFrigate
    1000.0,          //tGravWellGenerator
    1500.0,          //tHeavyCruiser
    0.0,             //tHeavyDefender
    0.0,             //tHeavyInterceptor
    1000.0,          //tIonCannonFrigate
    0.0,             //tMinelayerCorvette
    1500.0,          //tMissileDestroyer
    0.0,             //tMultiGunCorvette
    0.0,             //tProximitySensor
    1000.0,          //tResourceCollector
    0.0,             //tSalCapCorvette
    1500.0,          //tStandardDestroyer
    1000.0,          //tStandardFrigate
    1000.0,          //p1Outpost
    0.0,             //p1Fighter
    1000.0,          //p1IonArrayFrigate
    0.0,             //p1MissileCorvette
    1700.0,          //p1Mothership
    0.0,             //p1StandardCorvette
    2000.0,          //trFloatingCity
    1000.0,          //trCargoBarge
    1700.0,          //trMiningBase
    1700.0,          //trResearchStation
    1000.0,          //trJunkYardDawg
    1000.0,          //trJunkYardHQ
    1000.0,          //trGhostship
    2000.0,          //trJunk_LGun
    1000.0,          //trJunk_SGun
    2000.0,          //trResearchStationBridge
    2000.0,          //trResearchStationTower
    2500.0,          //dRKO
    1000.0,          //dBigGun
    1000.0,          //dBigGunUnit
    0.0,             //dSlipgateSmall
    0.0,             //dSlipgateMedium
    0.0,             //dSlipgateLarge
    0.0,             //dCrate
    500.0,           //dJunk1PartA
    500.0,           //dJunk1Shell
    500.0,           //dJunk1Strut
    500.0,           //dJunk2Shipwreck
    500.0,           //dJunk3Boiler
    500.0,           //dJunk3BoilerCasing
    500.0,           //dMessagePod
    0.0,             //dBuoy
    0.0,             //dFirstPlanet
    0.0,             //dDeadPlanet
    500.0,           //dCrystal0
    500.0,           //dCrystal1
    2000.0,          //dBeastDockingBay
    0.0,             //dNISBuoy
    2000.0,          //dBeastMothership
    4000.0,          //dNomadMoon
    500.0,           //dNomadFieldEmitter
    0.0,             //dAcolyte
    1700.0,          //dPrisonShip
    1700.0,          //dPrisonShipInfected
    500.0,           //dSlipgateNode
    1600.0,          //dKushanCarrier
    1000.0,          //dIonArrayFrigate
    1000.0,          //dIonCannonFrigate
    1000.0,          //dEcclesiam
    1000.0,          //dEcclesiamInfected
    0.0,             //nisLightSource1
    0.0,             //nisLightSource2
    0.0              //nisLensflareSource1
};

// rightOfWay for ships.
// -1 indicates lowest priority of the low (e.g. pebbles in space)
// 0 indicates strikecraft, probes, etc.
// 1 indicates frigates
// 2 indicates cruisers/destroyers
// 3 indicates carriers
// 4 indicates mothership

// CAT: values per Cataclysm type (rightOfWays 0x008b1dc0, read by InitStatShipInfo 0x0053bab0)
sbyte rightOfWays[TOTAL_NUM_SHIPS] =
{
    0,               //sRecon
    0,               //sAcolyte
    0,               //sMimic
    0,               //sLeech
    0,               //sACV
    0,               //sMCV
    1,               //sRammingFrigate
    1,               //sMultiBeamFrigate
    1,               //sHiveFrigate
    0,               //sSwarmer
    2,               //sDreadnought
    2,               //sDreadnoughtCannon
    3,               //sCarrier
    3,               //sCarrierSupport
    1,               //sWorker
    0,               //sRepairDroid
    2,               //sProcessor
    0,               //sSentinel
    4,               //sMothership
    4,               //sMothershipSpecial
    4,               //sMothershipEngineering
    4,               //sMothershipArmour
    4,               //sMothershipMicro
    4,               //sMothershipWeapons
    4,               //sMothershipDockingBay
    2,               //sDestroyer
    4,               //sMothershipSupport
    4,               //sMothershipCommand
    4,               //sMothershipEnginesLower
    4,               //sMothershipEnginesUpper
    4,               //sMothershipLowerSpine
    4,               //sMothershipBigGun
    4,               //sMothershipOreCanArmour
    0,               //bRecon
    0,               //bAcolyte
    0,               //bInterceptor
    0,               //bCloakedFighter
    0,               //bAttackBomber
    0,               //bACV
    0,               //bMultiGunCorvette
    0,               //bHeavyCorvette
    1,               //bMultiBeamFrigate
    1,               //bHiveFrigate
    0,               //bSwarmer
    1,               //bIonArrayFrigate
    1,               //bDFGFrigate
    2,               //bHeavyCruiser
    3,               //bCarrier
    1,               //bRammingFrigate
    1,               //bWorker
    0,               //bRepairDroid
    2,               //bProcessor
    0,               //bSentinel
    4,               //bMothership
    4,               //bMothershipDockingBay
    4,               //bMothershipEnginesLower
    4,               //bSectMoship
    4,               //bMothershipSupport
    0,               //bCruiseMissile
    0,               //bMissileCorvette
    1,               //kAdvanceSupportFrigate
    0,               //kAttackBomber
    3,               //kCarrier
    1,               //kCloakGenerator
    1,               //kGravWellGenerator
    0,               //kHeavyCorvette
    2,               //kHeavyCruiser
    0,               //kHeavyDefender
    0,               //kHeavyInterceptor
    1,               //kIonCannonFrigate
    0,               //kMultiGunCorvette
    0,               //kProximitySensor
    0,               //kSalCapCorvette
    2,               //kStandardDestroyer
    1,               //tAdvanceSupportFrigate
    0,               //tAttackBomber
    3,               //tCarrier
    1,               //tCloakGenerator
    1,               //tDFGFrigate
    1,               //tGravWellGenerator
    2,               //tHeavyCruiser
    0,               //tHeavyDefender
    0,               //tHeavyInterceptor
    1,               //tIonCannonFrigate
    0,               //tMinelayerCorvette
    2,               //tMissileDestroyer
    0,               //tMultiGunCorvette
    0,               //tProximitySensor
    1,               //tResourceCollector
    0,               //tSalCapCorvette
    2,               //tStandardDestroyer
    1,               //tStandardFrigate
    4,               //p1Outpost
    0,               //p1Fighter
    1,               //p1IonArrayFrigate
    0,               //p1MissileCorvette
    4,               //p1Mothership
    0,               //p1StandardCorvette
    4,               //trFloatingCity
    1,               //trCargoBarge
    4,               //trMiningBase
    4,               //trResearchStation
    1,               //trJunkYardDawg
    2,               //trJunkYardHQ
    1,               //trGhostship
    1,               //trJunk_LGun
    0,               //trJunk_SGun
    4,               //trResearchStationBridge
    4,               //trResearchStationTower
    4,               //dRKO
    2,               //dBigGun
    2,               //dBigGunUnit
    0,               //dSlipgateSmall
    0,               //dSlipgateMedium
    0,               //dSlipgateLarge
    0,               //dCrate
    2,               //dJunk1PartA
    2,               //dJunk1Shell
    2,               //dJunk1Strut
    2,               //dJunk2Shipwreck
    2,               //dJunk3Boiler
    2,               //dJunk3BoilerCasing
    2,               //dMessagePod
    0,               //dBuoy
    0,               //dFirstPlanet
    0,               //dDeadPlanet
    2,               //dCrystal0
    2,               //dCrystal1
    4,               //dBeastDockingBay
    0,               //dNISBuoy
    4,               //dBeastMothership
    4,               //dNomadMoon
    4,               //dNomadFieldEmitter
    0,               //dAcolyte
    3,               //dPrisonShip
    3,               //dPrisonShipInfected
    3,               //dSlipgateNode
    2,               //dKushanCarrier
    1,               //dIonArrayFrigate
    1,               //dIonCannonFrigate
    3,               //dEcclesiam
    1,               //dEcclesiamInfected
    0,               //nisLightSource1
    0,               //nisLightSource2
    0                //nisLensflareSource1
};

// CAT: Cataclysm's unit-cap keys (ShipStaticCapLimits 0x008b1e58: 110 per-type rows keyed by the ship name with the
//      race prefix kept except for Sect, "Caps<name>", plus dAcolyte, then the 12 CLASS_* rows; the MaxJobsPerClass
//      rows are gone; read from UnitLimitCaps<n>.script)
scriptEntry ShipStaticCapLimits[] =
{
    { "ShipTotalCaps",                   scriptSetSdwordCB, &cdMaxShipsAllowed },
    { "CapsRecon",                       scriptSetSdwordCB, &cdLimitCaps[sRecon] },
    { "CapsAcolyte",                     scriptSetSdwordCB, &cdLimitCaps[sAcolyte] },
    { "CapsMimic",                       scriptSetSdwordCB, &cdLimitCaps[sMimic] },
    { "CapsLeech",                       scriptSetSdwordCB, &cdLimitCaps[sLeech] },
    { "CapsACV",                         scriptSetSdwordCB, &cdLimitCaps[sACV] },
    { "CapsMCV",                         scriptSetSdwordCB, &cdLimitCaps[sMCV] },
    { "CapsRammingFrigate",              scriptSetSdwordCB, &cdLimitCaps[sRammingFrigate] },
    { "CapsMultiBeamFrigate",            scriptSetSdwordCB, &cdLimitCaps[sMultiBeamFrigate] },
    { "CapsHiveFrigate",                 scriptSetSdwordCB, &cdLimitCaps[sHiveFrigate] },
    { "CapsSwarmer",                     scriptSetSdwordCB, &cdLimitCaps[sSwarmer] },
    { "CapsDreadnought",                 scriptSetSdwordCB, &cdLimitCaps[sDreadnought] },
    { "CapsDreadnoughtCannon",           scriptSetSdwordCB, &cdLimitCaps[sDreadnoughtCannon] },
    { "CapsCarrier",                     scriptSetSdwordCB, &cdLimitCaps[sCarrier] },
    { "CapsCarrierSupport",              scriptSetSdwordCB, &cdLimitCaps[sCarrierSupport] },
    { "CapsWorker",                      scriptSetSdwordCB, &cdLimitCaps[sWorker] },
    { "CapsRepairDroid",                 scriptSetSdwordCB, &cdLimitCaps[sRepairDroid] },
    { "CapsProcessor",                   scriptSetSdwordCB, &cdLimitCaps[sProcessor] },
    { "CapsSentinel",                    scriptSetSdwordCB, &cdLimitCaps[sSentinel] },
    { "CapsMothership",                  scriptSetSdwordCB, &cdLimitCaps[sMothership] },
    { "CapsMothershipSpecial",           scriptSetSdwordCB, &cdLimitCaps[sMothershipSpecial] },
    { "CapsMothershipEngineering",       scriptSetSdwordCB, &cdLimitCaps[sMothershipEngineering] },
    { "CapsMothershipArmour",            scriptSetSdwordCB, &cdLimitCaps[sMothershipArmour] },
    { "CapsMothershipMicro",             scriptSetSdwordCB, &cdLimitCaps[sMothershipMicro] },
    { "CapsMothershipWeapons",           scriptSetSdwordCB, &cdLimitCaps[sMothershipWeapons] },
    { "CapsMothershipDockingBay",        scriptSetSdwordCB, &cdLimitCaps[sMothershipDockingBay] },
    { "CapsDestroyer",                   scriptSetSdwordCB, &cdLimitCaps[sDestroyer] },
    { "CapsMothershipSupport",           scriptSetSdwordCB, &cdLimitCaps[sMothershipSupport] },
    { "CapsMothershipCommand",           scriptSetSdwordCB, &cdLimitCaps[sMothershipCommand] },
    { "CapsMothershipEnginesLower",      scriptSetSdwordCB, &cdLimitCaps[sMothershipEnginesLower] },
    { "CapsMothershipEnginesUpper",      scriptSetSdwordCB, &cdLimitCaps[sMothershipEnginesUpper] },
    { "CapsMothershipLowerSpine",        scriptSetSdwordCB, &cdLimitCaps[sMothershipLowerSpine] },
    { "CapsMothershipBigGun",            scriptSetSdwordCB, &cdLimitCaps[sMothershipBigGun] },
    { "CapsMothershipOreCanArmour",      scriptSetSdwordCB, &cdLimitCaps[sMothershipOreCanArmour] },
    { "CapbRecon",                       scriptSetSdwordCB, &cdLimitCaps[bRecon] },
    { "CapbAcolyte",                     scriptSetSdwordCB, &cdLimitCaps[bAcolyte] },
    { "CapbInterceptor",                 scriptSetSdwordCB, &cdLimitCaps[bInterceptor] },
    { "CapbCloakedFighter",              scriptSetSdwordCB, &cdLimitCaps[bCloakedFighter] },
    { "CapbAttackBomber",                scriptSetSdwordCB, &cdLimitCaps[bAttackBomber] },
    { "CapbACV",                         scriptSetSdwordCB, &cdLimitCaps[bACV] },
    { "CapbMultiGunCorvette",            scriptSetSdwordCB, &cdLimitCaps[bMultiGunCorvette] },
    { "CapbHeavyCorvette",               scriptSetSdwordCB, &cdLimitCaps[bHeavyCorvette] },
    { "CapbMultiBeamFrigate",            scriptSetSdwordCB, &cdLimitCaps[bMultiBeamFrigate] },
    { "CapbHiveFrigate",                 scriptSetSdwordCB, &cdLimitCaps[bHiveFrigate] },
    { "CapbSwarmer",                     scriptSetSdwordCB, &cdLimitCaps[bSwarmer] },
    { "CapbIonArrayFrigate",             scriptSetSdwordCB, &cdLimitCaps[bIonArrayFrigate] },
    { "CapbDFGFrigate",                  scriptSetSdwordCB, &cdLimitCaps[bDFGFrigate] },
    { "CapbHeavyCruiser",                scriptSetSdwordCB, &cdLimitCaps[bHeavyCruiser] },
    { "CapbCarrier",                     scriptSetSdwordCB, &cdLimitCaps[bCarrier] },
    { "CapbRammingFrigate",              scriptSetSdwordCB, &cdLimitCaps[bRammingFrigate] },
    { "CapbWorker",                      scriptSetSdwordCB, &cdLimitCaps[bWorker] },
    { "CapbRepairDroid",                 scriptSetSdwordCB, &cdLimitCaps[bRepairDroid] },
    { "CapbProcessor",                   scriptSetSdwordCB, &cdLimitCaps[bProcessor] },
    { "CapbSentinel",                    scriptSetSdwordCB, &cdLimitCaps[bSentinel] },
    { "CapbMothership",                  scriptSetSdwordCB, &cdLimitCaps[bMothership] },
    { "CapbMothershipDockingBay",        scriptSetSdwordCB, &cdLimitCaps[bMothershipDockingBay] },
    { "CapbMothershipEnginesLower",      scriptSetSdwordCB, &cdLimitCaps[bMothershipEnginesLower] },
    { "CapbSectMoship",                  scriptSetSdwordCB, &cdLimitCaps[bSectMoship] },
    { "CapbMothershipSupport",           scriptSetSdwordCB, &cdLimitCaps[bMothershipSupport] },
    { "CapbCruiseMissile",               scriptSetSdwordCB, &cdLimitCaps[bCruiseMissile] },
    { "CapbMissileCorvette",             scriptSetSdwordCB, &cdLimitCaps[bMissileCorvette] },
    { "CapkAdvanceSupportFrigate",       scriptSetSdwordCB, &cdLimitCaps[kAdvanceSupportFrigate] },
    { "CapkAttackBomber",                scriptSetSdwordCB, &cdLimitCaps[kAttackBomber] },
    { "CapkCarrier",                     scriptSetSdwordCB, &cdLimitCaps[kCarrier] },
    { "CapkCloakGenerator",              scriptSetSdwordCB, &cdLimitCaps[kCloakGenerator] },
    { "CapkGravWellGenerator",           scriptSetSdwordCB, &cdLimitCaps[kGravWellGenerator] },
    { "CapkHeavyCorvette",               scriptSetSdwordCB, &cdLimitCaps[kHeavyCorvette] },
    { "CapkHeavyCruiser",                scriptSetSdwordCB, &cdLimitCaps[kHeavyCruiser] },
    { "CapkHeavyDefender",               scriptSetSdwordCB, &cdLimitCaps[kHeavyDefender] },
    { "CapkHeavyInterceptor",            scriptSetSdwordCB, &cdLimitCaps[kHeavyInterceptor] },
    { "CapkIonCannonFrigate",            scriptSetSdwordCB, &cdLimitCaps[kIonCannonFrigate] },
    { "CapkMultiGunCorvette",            scriptSetSdwordCB, &cdLimitCaps[kMultiGunCorvette] },
    { "CapkProximitySensor",             scriptSetSdwordCB, &cdLimitCaps[kProximitySensor] },
    { "CapkSalCapCorvette",              scriptSetSdwordCB, &cdLimitCaps[kSalCapCorvette] },
    { "CapkStandardDestroyer",           scriptSetSdwordCB, &cdLimitCaps[kStandardDestroyer] },
    { "CaptAdvanceSupportFrigate",       scriptSetSdwordCB, &cdLimitCaps[tAdvanceSupportFrigate] },
    { "CaptAttackBomber",                scriptSetSdwordCB, &cdLimitCaps[tAttackBomber] },
    { "CaptCarrier",                     scriptSetSdwordCB, &cdLimitCaps[tCarrier] },
    { "CaptCloakGenerator",              scriptSetSdwordCB, &cdLimitCaps[tCloakGenerator] },
    { "CaptDFGFrigate",                  scriptSetSdwordCB, &cdLimitCaps[tDFGFrigate] },
    { "CaptGravWellGenerator",           scriptSetSdwordCB, &cdLimitCaps[tGravWellGenerator] },
    { "CaptHeavyCruiser",                scriptSetSdwordCB, &cdLimitCaps[tHeavyCruiser] },
    { "CaptHeavyDefender",               scriptSetSdwordCB, &cdLimitCaps[tHeavyDefender] },
    { "CaptHeavyInterceptor",            scriptSetSdwordCB, &cdLimitCaps[tHeavyInterceptor] },
    { "CaptIonCannonFrigate",            scriptSetSdwordCB, &cdLimitCaps[tIonCannonFrigate] },
    { "CaptMinelayerCorvette",           scriptSetSdwordCB, &cdLimitCaps[tMinelayerCorvette] },
    { "CaptMissileDestroyer",            scriptSetSdwordCB, &cdLimitCaps[tMissileDestroyer] },
    { "CaptMultiGunCorvette",            scriptSetSdwordCB, &cdLimitCaps[tMultiGunCorvette] },
    { "CaptProximitySensor",             scriptSetSdwordCB, &cdLimitCaps[tProximitySensor] },
    { "CaptResourceCollector",           scriptSetSdwordCB, &cdLimitCaps[tResourceCollector] },
    { "CaptSalCapCorvette",              scriptSetSdwordCB, &cdLimitCaps[tSalCapCorvette] },
    { "CaptStandardDestroyer",           scriptSetSdwordCB, &cdLimitCaps[tStandardDestroyer] },
    { "CaptStandardFrigate",             scriptSetSdwordCB, &cdLimitCaps[tStandardFrigate] },
    { "Capp1Outpost",                    scriptSetSdwordCB, &cdLimitCaps[p1Outpost] },
    { "Capp1Fighter",                    scriptSetSdwordCB, &cdLimitCaps[p1Fighter] },
    { "Capp1IonArrayFrigate",            scriptSetSdwordCB, &cdLimitCaps[p1IonArrayFrigate] },
    { "Capp1MissileCorvette",            scriptSetSdwordCB, &cdLimitCaps[p1MissileCorvette] },
    { "Capp1Mothership",                 scriptSetSdwordCB, &cdLimitCaps[p1Mothership] },
    { "Capp1StandardCorvette",           scriptSetSdwordCB, &cdLimitCaps[p1StandardCorvette] },
    { "CaptrFloatingCity",               scriptSetSdwordCB, &cdLimitCaps[trFloatingCity] },
    { "CaptrCargoBarge",                 scriptSetSdwordCB, &cdLimitCaps[trCargoBarge] },
    { "CaptrMiningBase",                 scriptSetSdwordCB, &cdLimitCaps[trMiningBase] },
    { "CaptrResearchStation",            scriptSetSdwordCB, &cdLimitCaps[trResearchStation] },
    { "CaptrJunkYardDawg",               scriptSetSdwordCB, &cdLimitCaps[trJunkYardDawg] },
    { "CaptrJunkYardHQ",                 scriptSetSdwordCB, &cdLimitCaps[trJunkYardHQ] },
    { "CaptrGhostShip",                  scriptSetSdwordCB, &cdLimitCaps[trGhostship] },
    { "CaptrJunk_LGun",                  scriptSetSdwordCB, &cdLimitCaps[trJunk_LGun] },
    { "CaptrJunk_SGun",                  scriptSetSdwordCB, &cdLimitCaps[trJunk_SGun] },
    { "CaptrResearchStationBridge",      scriptSetSdwordCB, &cdLimitCaps[trResearchStationBridge] },
    { "CaptrResearchStationTower",       scriptSetSdwordCB, &cdLimitCaps[trResearchStationTower] },
    { "CapdAcolyte",                     scriptSetSdwordCB, &cdLimitCaps[dAcolyte] },
    { "CapCLASS_Mothership",             scriptSetSdwordCB, &cdClassCaps[CLASS_Mothership] },
    { "CapCLASS_HeavyCruiser",           scriptSetSdwordCB, &cdClassCaps[CLASS_HeavyCruiser] },
    { "CapCLASS_Carrier",                scriptSetSdwordCB, &cdClassCaps[CLASS_Carrier] },
    { "CapCLASS_Destroyer",              scriptSetSdwordCB, &cdClassCaps[CLASS_Destroyer] },
    { "CapCLASS_Frigate",                scriptSetSdwordCB, &cdClassCaps[CLASS_Frigate] },
    { "CapCLASS_Corvette",               scriptSetSdwordCB, &cdClassCaps[CLASS_Corvette] },
    { "CapCLASS_Fighter",                scriptSetSdwordCB, &cdClassCaps[CLASS_Fighter] },
    { "CapCLASS_Resource",               scriptSetSdwordCB, &cdClassCaps[CLASS_Resource] },
    { "CapCLASS_NonCombat",              scriptSetSdwordCB, &cdClassCaps[CLASS_NonCombat] },
    { "CapCLASS_MonsterShip",            scriptSetSdwordCB, &cdClassCaps[CLASS_MonsterShip] },
    { "CapCLASS_Component",              scriptSetSdwordCB, &cdClassCaps[CLASS_Component] },
    { "CapCLASS_Micro",                  scriptSetSdwordCB, &cdClassCaps[CLASS_Micro] },

    END_SCRIPT_ENTRY
};

// CAT: Cataclysm's ShipStaticScriptTable, the 178 entries of the exe's table (0x008b2428) in the exe's order.
//      29 keys are new (marked); HW1's maxfuel, lowfuelpoint, fuelburnrate, attackfuelburnrate, pumpFuelRate,
//      NUMBER_OF_SALVAGE_POINTS and the debug-only scaleFactor are gone (their fields, where kept, stay 0), and
//      HW1's commented-out scaleCap/NumRepairDroids stay out (repairDroids is the new key). Callbacks of keys HW1
//      already had are HW1's; the exe's scriptSetU*CB entries are the linker-folded S* bodies (same "%d" scan).
scriptStructEntry ShipStaticScriptTable[] =
{
    { "LODFile",                                   LODScriptLoad,                       &(ShipStaticInfos[0].common.LOD), &(ShipStaticInfos[0]) },
    { "pMexData",                                  loadMexDataCB,                       &(ShipStaticInfos[0].common.pMexData), &(ShipStaticInfos[0]) },
    { "mass",                                      scriptSetReal32CB,                   &(ShipStaticInfos[0].common.mass), &(ShipStaticInfos[0]) },
    { "momentOfInertiaX",                          scriptSetReal32CB,                   &(ShipStaticInfos[0].common.momentOfInertiaX), &(ShipStaticInfos[0]) },
    { "momentOfInertiaY",                          scriptSetReal32CB,                   &(ShipStaticInfos[0].common.momentOfInertiaY), &(ShipStaticInfos[0]) },
    { "momentOfInertiaZ",                          scriptSetReal32CB,                   &(ShipStaticInfos[0].common.momentOfInertiaZ), &(ShipStaticInfos[0]) },
    { "collBoxScaleRight",                         scriptSetReal32CB,                   &(ShipStaticInfos[0].common.staticCollInfo.collBoxScaleRight), &(ShipStaticInfos[0]) },
    { "collBoxScaleUp",                            scriptSetReal32CB,                   &(ShipStaticInfos[0].common.staticCollInfo.collBoxScaleUp), &(ShipStaticInfos[0]) },
    { "collBoxScaleForward",                       scriptSetReal32CB,                   &(ShipStaticInfos[0].common.staticCollInfo.collBoxScaleForward), &(ShipStaticInfos[0]) },
    { "collBoxOffsetX",                            scriptSetReal32CB,                   &(ShipStaticInfos[0].common.staticCollInfo.collBoxOffsetX), &(ShipStaticInfos[0]) },
    { "collBoxOffsetY",                            scriptSetReal32CB,                   &(ShipStaticInfos[0].common.staticCollInfo.collBoxOffsetY), &(ShipStaticInfos[0]) },
    { "collBoxOffsetZ",                            scriptSetReal32CB,                   &(ShipStaticInfos[0].common.staticCollInfo.collBoxOffsetZ), &(ShipStaticInfos[0]) },
    { "immobile",                                  scriptSetBool8,                      &(ShipStaticInfos[0].common.immobile), &(ShipStaticInfos[0]) },
    { "rightOfWay",                                scriptSetSbyteCB,                    &(ShipStaticInfos[0].common.rightOfWay), &(ShipStaticInfos[0]) },
    { "collspheresize",                            scriptSetReal32CB,                   &(ShipStaticInfos[0].common.staticCollInfo.collspheresize), &(ShipStaticInfos[0]) },
    { "approxcollmodifier",                        scriptSetReal32CB,                   &(ShipStaticInfos[0].common.staticCollInfo.approxcollmodifier), &(ShipStaticInfos[0]) },
    { "avoidcollmodifier",                         scriptSetReal32CB,                   &(ShipStaticInfos[0].common.staticCollInfo.avoidcollmodifier), &(ShipStaticInfos[0]) },
    { "maxvelocity",                               scriptSetReal32CB,                   &(ShipStaticInfos[0].common.maxvelocity), &(ShipStaticInfos[0]) },
    { "maxrot",                                    scriptSetReal32CB,                   &(ShipStaticInfos[0].common.maxrot), &(ShipStaticInfos[0]) },
    { "bulletRange[0]",                            scriptSetReal32CB,                   &(ShipStaticInfos[0].bulletRange[0]), &(ShipStaticInfos[0]) },
    { "bulletRange[1]",                            scriptSetReal32CB,                   &(ShipStaticInfos[0].bulletRange[1]), &(ShipStaticInfos[0]) },
    { "bulletRange[2]",                            scriptSetReal32CB,                   &(ShipStaticInfos[0].bulletRange[2]), &(ShipStaticInfos[0]) },
    { "minBulletRange[0]",                         scriptSetReal32CB,                   &(ShipStaticInfos[0].minBulletRange[0]), &(ShipStaticInfos[0]) },
    { "minBulletRange[1]",                         scriptSetReal32CB,                   &(ShipStaticInfos[0].minBulletRange[1]), &(ShipStaticInfos[0]) },
    { "minBulletRange[2]",                         scriptSetReal32CB,                   &(ShipStaticInfos[0].minBulletRange[2]), &(ShipStaticInfos[0]) },
    { "blastRadiusShockWave",                      scriptSetReal32CB,                   &(ShipStaticInfos[0].blastRadiusShockWave), &(ShipStaticInfos[0]) },
    { "blastRadiusDamage",                         scriptSetReal32CB,                   &(ShipStaticInfos[0].blastRadiusDamage), &(ShipStaticInfos[0]) },
    { "passiveRetaliateZone",                      scriptSetReal32CB,                   &(ShipStaticInfos[0].passiveRetaliateZone), &(ShipStaticInfos[0]) },
    { "shipclass",                                 setClassCB,                          &(ShipStaticInfos[0].shipclass), &(ShipStaticInfos[0]) },
    { "rotateToRetaliate",                         scriptSetBool8,                      &(ShipStaticInfos[0].rotateToRetaliate), &(ShipStaticInfos[0]) },
    { "canTargetMultipleTargets",                  scriptSetBool8,                      &(ShipStaticInfos[0].canTargetMultipleTargets), &(ShipStaticInfos[0]) },
    { "canBuildShips",                             scriptSetBool8,                      &(ShipStaticInfos[0].canBuildShips), &(ShipStaticInfos[0]) },
    { "canReceiveShips",                           scriptSetBool8,                      &(ShipStaticInfos[0].canReceiveShips), &(ShipStaticInfos[0]) },
    { "canReceiveResources",                       scriptSetBool8,                      &(ShipStaticInfos[0].canReceiveResources), &(ShipStaticInfos[0]) },
    { "canReceiveShipsPermanently",                scriptSetBool8,                      &(ShipStaticInfos[0].canReceiveShipsPermanently), &(ShipStaticInfos[0]) },
    { "canReceiveShipsForRetire",                  scriptSetBool8,                      &(ShipStaticInfos[0].canReceiveShipsForRetire), &(ShipStaticInfos[0]) },
    { "canBuildBigShips",                          scriptSetBool8,                      &(ShipStaticInfos[0].canBuildBigShips), &(ShipStaticInfos[0]) },
    { "canReceiveFighters",                        scriptSetBool8,                      &(ShipStaticInfos[0].canReceiveTheseShips[THISSHIPIS_FIGHTER]), &(ShipStaticInfos[0]) },
    { "canReceiveCorvettes",                       scriptSetBool8,                      &(ShipStaticInfos[0].canReceiveTheseShips[THISSHIPIS_CORVETTE]), &(ShipStaticInfos[0]) },
    { "canReceiveResourceCollectors",              scriptSetBool8,                      &(ShipStaticInfos[0].canReceiveTheseShips[THISSHIPIS_RESOURCER]), &(ShipStaticInfos[0]) },
    { "canReceiveOtherNonCapitalShips",            scriptSetBool8,                      &(ShipStaticInfos[0].canReceiveTheseShips[THISSHIPIS_OTHERNONCAPITALSHIP]), &(ShipStaticInfos[0]) },
    { "canHandleNumShipsDocking",                  scriptSetSdwordCB,                   &(ShipStaticInfos[0].canHandleNumShipsDocking), &(ShipStaticInfos[0]) },
    { "canHandleNumShipsDepositingRU",             scriptSetSdwordCB,                   &(ShipStaticInfos[0].canHandleNumShipsDepositingRU), &(ShipStaticInfos[0]) },
    { "svManeuverability",                         scriptSetSdwordCB,                   &(ShipStaticInfos[0].svManeuverability), &(ShipStaticInfos[0]) },
    { "svCoverage",                                scriptSetSdwordCB,                   &(ShipStaticInfos[0].svCoverage), &(ShipStaticInfos[0]) },
    { "svFirePower",                               scriptSetSdwordCB,                   &(ShipStaticInfos[0].svFirePower), &(ShipStaticInfos[0]) },
    { "maxDockableFighters",                       scriptSetSwordCB,                    &(ShipStaticInfos[0].maxDockableFighters), &(ShipStaticInfos[0]) },
    { "maxDockableCorvettes",                      scriptSetSwordCB,                    &(ShipStaticInfos[0].maxDockableCorvettes), &(ShipStaticInfos[0]) },
    { "specialActivateIsContinuous",               scriptSetBool8,                      &(ShipStaticInfos[0].specialActivateIsContinuous), &(ShipStaticInfos[0]) },
    { "canSpecialBandBoxFriendlies",               scriptSetBool8,                      &(ShipStaticInfos[0].canSpecialBandBoxFriendlies), &(ShipStaticInfos[0]) },
    { "canSingleClickSpecialActivate",             scriptSetBool8,                      &(ShipStaticInfos[0].canSingleClickSpecialActivate), &(ShipStaticInfos[0]) },
    { "buildCost",                                 scriptSetSdwordCB,                   &(ShipStaticInfos[0].buildCost), &(ShipStaticInfos[0]) },
    { "supportUnits",                              scriptSetUdwordCB,                   &(ShipStaticInfos[0].supportUnits), &(ShipStaticInfos[0]) },   // CAT: new key
    { "providesSU",                                scriptSetUdwordCB,                   &(ShipStaticInfos[0].providesSU), &(ShipStaticInfos[0]) },   // CAT: new key
    { "TimeShockedByEMP",                          scriptSetReal32CB,                   &(ShipStaticInfos[0].TimeShockedByEMP), &(ShipStaticInfos[0]) },   // CAT: new key
    { "EMP_Range",                                 scriptSetReal32CB,                   &(ShipStaticInfos[0].EMP_Range), &(ShipStaticInfos[0]) },   // CAT: new key
    { "EMP_Max_Combine_Distance",                  scriptSetReal32CB,                   &(ShipStaticInfos[0].EMP_Max_Combine_Distance), &(ShipStaticInfos[0]) },   // CAT: new key
    { "RamDamage",                                 scriptSetReal32CB,                   &(ShipStaticInfos[0].RamDamage), &(ShipStaticInfos[0]) },   // CAT: new key
    { "cuttingBeamDamagePerSecond",                scriptSetReal32CB,                   &(ShipStaticInfos[0].cuttingBeamDamagePerSecond), &(ShipStaticInfos[0]) },   // CAT: new key
    { "cuttingBeamUpgradedDamagePerSecond",        scriptSetReal32CB,                   &(ShipStaticInfos[0].cuttingBeamUpgradedDamagePerSecond), &(ShipStaticInfos[0]) },   // CAT: new key
    { "RamPushTime",                               scriptSetReal32CB,                   &(ShipStaticInfos[0].RamPushTime), &(ShipStaticInfos[0]) },   // CAT: new key
    { "RamWaitAfterPushTime",                      scriptSetReal32CB,                   &(ShipStaticInfos[0].RamWaitAfterPushTime), &(ShipStaticInfos[0]) },   // CAT: new key
    { "UpgradeByDocking",                          scriptSetBool,                       &(ShipStaticInfos[0].UpgradeByDocking), &(ShipStaticInfos[0]) },   // CAT: new key
    { "pilotViewOffsetX",                          scriptSetReal32CB,                   &(ShipStaticInfos[0].pilotViewOffsetX), &(ShipStaticInfos[0]) },   // CAT: new key
    { "pilotViewOffsetY",                          scriptSetReal32CB,                   &(ShipStaticInfos[0].pilotViewOffsetY), &(ShipStaticInfos[0]) },   // CAT: new key
    { "pilotViewOffsetZ",                          scriptSetReal32CB,                   &(ShipStaticInfos[0].pilotViewOffsetZ), &(ShipStaticInfos[0]) },   // CAT: new key
    { "RKOGetsMissiles",                           scriptSetUdwordCB,                   &(ShipStaticInfos[0].RKOGetsMissiles), &(ShipStaticInfos[0]) },   // CAT: new key
    { "buildTime",                                 scriptSetFramesCB,                   &(ShipStaticInfos[0].buildTime), &(ShipStaticInfos[0]) },
    { "fowRadius",                                 scriptSetReal32CB,                   &(ShipStaticInfos[0].fowRadius), &(ShipStaticInfos[0]) },   // CAT: new key
    { "resourcesAtOneTime",                        scriptSetSdwordCB,                   &(ShipStaticInfos[0].resourcesAtOneTime), &(ShipStaticInfos[0]) },
    { "RKOEatMeTime",                              scriptSetReal32CB,                   &(ShipStaticInfos[0].RKOEatMeTime), &(ShipStaticInfos[0]) },   // CAT: new key
    { "maxresources",                              scriptSetSdwordCB,                   &(ShipStaticInfos[0].maxresources), &(ShipStaticInfos[0]) },
    { "harvestAmount",                             scriptSetSdwordCB,                   &(ShipStaticInfos[0].harvestAmount), &(ShipStaticInfos[0]) },
    { "harvestRate",                               scriptSetSdwordCB,                   &(ShipStaticInfos[0].harvestRate), &(ShipStaticInfos[0]) },
    { "maxhealth",                                 scriptSetReal32CB,                   &(ShipStaticInfos[0].health.maxhealth), &(ShipStaticInfos[0]) },
    { "minCollDamage",                             scriptSetReal32CB,                   &(ShipStaticInfos[0].health.minCollDamage), &(ShipStaticInfos[0]) },
    { "maxCollDamage",                             scriptSetReal32CB,                   &(ShipStaticInfos[0].health.maxCollDamage), &(ShipStaticInfos[0]) },
    { "groupSize",                                 scriptSetSdwordCB,                   &(ShipStaticInfos[0].groupSize), &(ShipStaticInfos[0]) },
    { "repairTime",                                scriptSetReal32CB,                   &(ShipStaticInfos[0].repairTime), &(ShipStaticInfos[0]) },
    { "repairDamage",                              scriptSetReal32CB,                   &(ShipStaticInfos[0].repairDamage), &(ShipStaticInfos[0]) },
    { "repairDroids",                              scriptSetUdwordCB,                   &(ShipStaticInfos[0].nRepairDroids), &(ShipStaticInfos[0]) },   // CAT: new key
    { "repairCombatTime",                          scriptSetReal32CB,                   &(ShipStaticInfos[0].repairCombatTime), &(ShipStaticInfos[0]) },
    { "repairCombatDamage",                        scriptSetReal32CB,                   &(ShipStaticInfos[0].repairCombatDamage), &(ShipStaticInfos[0]) },
    { "repairOtherShipRate",                       scriptSetReal32CB,                   &(ShipStaticInfos[0].repairOtherShipRate), &(ShipStaticInfos[0]) },
    { "repairResourceCollectorRate",               scriptSetReal32CB,                   &(ShipStaticInfos[0].repairResourceCollectorRate), &(ShipStaticInfos[0]) },
    { "dockShipRange",                             scriptSetReal32CB,                   &(ShipStaticInfos[0].dockShipRange), &(ShipStaticInfos[0]) },
    { "clearanceDistance",                         scriptSetReal32CB,                   &(ShipStaticInfos[0].clearanceDistance), &(ShipStaticInfos[0]) },
    { "clearanceDirection",                        scriptSetUdwordCB,                   &(ShipStaticInfos[0].clearanceDirection), &(ShipStaticInfos[0]) },
    { "formationPaddingModifier",                  scriptSetReal32CB,                   &(ShipStaticInfos[0].formationPaddingModifier), &(ShipStaticInfos[0]) },
    { "cannotForceAttackIfOwnShip",                scriptSetBool,                       &(ShipStaticInfos[0].cannotForceAttackIfOwnShip), &(ShipStaticInfos[0]) },
    { "passiveAttackPenaltyExempt",                scriptSetBool,                       &(ShipStaticInfos[0].passiveAttackPenaltyExempt), &(ShipStaticInfos[0]) },
    { "isCapitalShip",                             scriptSetBool,                       &(ShipStaticInfos[0].shipIsCapital), &(ShipStaticInfos[0]) },
    { "cantMoveAndAttack",                         scriptSetBool,                       &(ShipStaticInfos[0].cantMoveAndAttack), &(ShipStaticInfos[0]) },
    { "repairBeamCapable",                         scriptSetBool,                       &(ShipStaticInfos[0].repairBeamCapable), &(ShipStaticInfos[0]) },
    { "healthPerSecond",                           scriptSetReal32CB,                   &(ShipStaticInfos[0].healthPerSecond), &(ShipStaticInfos[0]) },
    { "CapitalDistanceRepairStart2",               scriptSetReal32CB,                   &(ShipStaticInfos[0].CapitalDistanceRepairStart2), &(ShipStaticInfos[0]) },
    { "CapitalDistanceRepairStart",                scriptSetReal32CB,                   &(ShipStaticInfos[0].CapitalDistanceRepairStart), &(ShipStaticInfos[0]) },
    { "AngleDotProdThreshold",                     scriptSetReal32CB,                   &(ShipStaticInfos[0].AngleDotProdThreshold), &(ShipStaticInfos[0]) },
    { "collSideModifiers[TRANS_UP]",               scriptSetReal32CB,                   &(ShipStaticInfos[0].collSideModifiers[TRANS_UP]), &(ShipStaticInfos[0]) },
    { "collSideModifiers[TRANS_DOWN]",             scriptSetReal32CB,                   &(ShipStaticInfos[0].collSideModifiers[TRANS_DOWN]), &(ShipStaticInfos[0]) },
    { "collSideModifiers[TRANS_RIGHT]",            scriptSetReal32CB,                   &(ShipStaticInfos[0].collSideModifiers[TRANS_RIGHT]), &(ShipStaticInfos[0]) },
    { "collSideModifiers[TRANS_LEFT]",             scriptSetReal32CB,                   &(ShipStaticInfos[0].collSideModifiers[TRANS_LEFT]), &(ShipStaticInfos[0]) },
    { "collSideModifiers[TRANS_FORWARD]",          scriptSetReal32CB,                   &(ShipStaticInfos[0].collSideModifiers[TRANS_FORWARD]), &(ShipStaticInfos[0]) },
    { "collSideModifiers[TRANS_BACK]",             scriptSetReal32CB,                   &(ShipStaticInfos[0].collSideModifiers[TRANS_BACKWARD]), &(ShipStaticInfos[0]) },
    { "thruststrength[TRANS_UP]",                  scriptSetReal32CB,                   &(ShipStaticInfos[0].guidance.thruststrengthstat[TRANS_UP]), &(ShipStaticInfos[0]) },
    { "thruststrength[TRANS_DOWN]",                scriptSetReal32CB,                   &(ShipStaticInfos[0].guidance.thruststrengthstat[TRANS_DOWN]), &(ShipStaticInfos[0]) },
    { "thruststrength[TRANS_RIGHT]",               scriptSetReal32CB,                   &(ShipStaticInfos[0].guidance.thruststrengthstat[TRANS_RIGHT]), &(ShipStaticInfos[0]) },
    { "thruststrength[TRANS_LEFT]",                scriptSetReal32CB,                   &(ShipStaticInfos[0].guidance.thruststrengthstat[TRANS_LEFT]), &(ShipStaticInfos[0]) },
    { "thruststrength[TRANS_FORWARD]",             scriptSetReal32CB,                   &(ShipStaticInfos[0].guidance.thruststrengthstat[TRANS_FORWARD]), &(ShipStaticInfos[0]) },
    { "thruststrength[TRANS_BACK]",                scriptSetReal32CB,                   &(ShipStaticInfos[0].guidance.thruststrengthstat[TRANS_BACKWARD]), &(ShipStaticInfos[0]) },
    { "rotstrength[ROT_YAWLEFT]",                  scriptSetReal32CB,                   &(ShipStaticInfos[0].guidance.rotstrengthstat[ROT_YAWLEFT]), &(ShipStaticInfos[0]) },
    { "rotstrength[ROT_YAWRIGHT]",                 scriptSetReal32CB,                   &(ShipStaticInfos[0].guidance.rotstrengthstat[ROT_YAWRIGHT]), &(ShipStaticInfos[0]) },
    { "rotstrength[ROT_PITCHUP]",                  scriptSetReal32CB,                   &(ShipStaticInfos[0].guidance.rotstrengthstat[ROT_PITCHUP]), &(ShipStaticInfos[0]) },
    { "rotstrength[ROT_PITCHDOWN]",                scriptSetReal32CB,                   &(ShipStaticInfos[0].guidance.rotstrengthstat[ROT_PITCHDOWN]), &(ShipStaticInfos[0]) },
    { "rotstrength[ROT_ROLLRIGHT]",                scriptSetReal32CB,                   &(ShipStaticInfos[0].guidance.rotstrengthstat[ROT_ROLLRIGHT]), &(ShipStaticInfos[0]) },
    { "rotstrength[ROT_ROLLLEFT]",                 scriptSetReal32CB,                   &(ShipStaticInfos[0].guidance.rotstrengthstat[ROT_ROLLLEFT]), &(ShipStaticInfos[0]) },
    { "turnspeed[TURN_YAW]",                       scriptSetReal32CB,                   &(ShipStaticInfos[0].guidance.turnspeedstat[TURN_YAW]), &(ShipStaticInfos[0]) },
    { "turnspeed[TURN_PITCH]",                     scriptSetReal32CB,                   &(ShipStaticInfos[0].guidance.turnspeedstat[TURN_PITCH]), &(ShipStaticInfos[0]) },
    { "turnspeed[TURN_ROLL]",                      scriptSetReal32CB,                   &(ShipStaticInfos[0].guidance.turnspeedstat[TURN_ROLL]), &(ShipStaticInfos[0]) },
    { "N-LIPS",                                    scriptSetReal32CB,                   &(ShipStaticInfos[0].scaleCap), &(ShipStaticInfos[0]) },
    { "trailStyle",                                scriptSetUdwordCB,                   &(ShipStaticInfos[0].trailStyle[0]), &(ShipStaticInfos[0]) },
    { "trailWidth",                                scriptSetReal32CB,                   &(ShipStaticInfos[0].trailWidth[0]), &(ShipStaticInfos[0]) },
    { "trailHeight",                               scriptSetReal32CB,                   &(ShipStaticInfos[0].trailHeight[0]), &(ShipStaticInfos[0]) },
    { "trailAngle",                                scriptSetReal32CB,                   &(ShipStaticInfos[0].trailAngle[0]), &(ShipStaticInfos[0]) },
    { "trailRibbonAdjust",                         scriptSetReal32CB,                   &(ShipStaticInfos[0].trailRibbonAdjust[0]), &(ShipStaticInfos[0]) },
    { "trailLength",                               scriptSetReal32CB,                   &(ShipStaticInfos[0].trailLength[0]), &(ShipStaticInfos[0]) },
    { "trailScaleCap",                             scriptSetReal32CB,                   &(ShipStaticInfos[0].trailScaleCap[0]), &(ShipStaticInfos[0]) },
    { "trailStyle2",                               scriptSetUdwordCB,                   &(ShipStaticInfos[0].trailStyle[1]), &(ShipStaticInfos[0]) },
    { "trailWidth2",                               scriptSetReal32CB,                   &(ShipStaticInfos[0].trailWidth[1]), &(ShipStaticInfos[0]) },
    { "trailHeight2",                              scriptSetReal32CB,                   &(ShipStaticInfos[0].trailHeight[1]), &(ShipStaticInfos[0]) },
    { "trailAngle2",                               scriptSetReal32CB,                   &(ShipStaticInfos[0].trailAngle[1]), &(ShipStaticInfos[0]) },
    { "trailRibbonAdjust2",                        scriptSetReal32CB,                   &(ShipStaticInfos[0].trailRibbonAdjust[1]), &(ShipStaticInfos[0]) },
    { "trailLength2",                              scriptSetReal32CB,                   &(ShipStaticInfos[0].trailLength[1]), &(ShipStaticInfos[0]) },
    { "trailScaleCap2",                            scriptSetReal32CB,                   &(ShipStaticInfos[0].trailScaleCap[1]), &(ShipStaticInfos[0]) },
    { "trailStyle3",                               scriptSetUdwordCB,                   &(ShipStaticInfos[0].trailStyle[2]), &(ShipStaticInfos[0]) },
    { "trailWidth3",                               scriptSetReal32CB,                   &(ShipStaticInfos[0].trailWidth[2]), &(ShipStaticInfos[0]) },
    { "trailHeight3",                              scriptSetReal32CB,                   &(ShipStaticInfos[0].trailHeight[2]), &(ShipStaticInfos[0]) },
    { "trailAngle3",                               scriptSetReal32CB,                   &(ShipStaticInfos[0].trailAngle[2]), &(ShipStaticInfos[0]) },
    { "trailRibbonAdjust3",                        scriptSetReal32CB,                   &(ShipStaticInfos[0].trailRibbonAdjust[2]), &(ShipStaticInfos[0]) },
    { "trailLength3",                              scriptSetReal32CB,                   &(ShipStaticInfos[0].trailLength[2]), &(ShipStaticInfos[0]) },
    { "trailScaleCap3",                            scriptSetReal32CB,                   &(ShipStaticInfos[0].trailScaleCap[2]), &(ShipStaticInfos[0]) },
    { "trailStyle4",                               scriptSetUdwordCB,                   &(ShipStaticInfos[0].trailStyle[3]), &(ShipStaticInfos[0]) },
    { "trailWidth4",                               scriptSetReal32CB,                   &(ShipStaticInfos[0].trailWidth[3]), &(ShipStaticInfos[0]) },
    { "trailHeight4",                              scriptSetReal32CB,                   &(ShipStaticInfos[0].trailHeight[3]), &(ShipStaticInfos[0]) },
    { "trailAngle4",                               scriptSetReal32CB,                   &(ShipStaticInfos[0].trailAngle[3]), &(ShipStaticInfos[0]) },
    { "trailRibbonAdjust4",                        scriptSetReal32CB,                   &(ShipStaticInfos[0].trailRibbonAdjust[3]), &(ShipStaticInfos[0]) },
    { "trailLength4",                              scriptSetReal32CB,                   &(ShipStaticInfos[0].trailLength[3]), &(ShipStaticInfos[0]) },
    { "trailScaleCap4",                            scriptSetReal32CB,                   &(ShipStaticInfos[0].trailScaleCap[3]), &(ShipStaticInfos[0]) },
    { "trailSpriteRadius",                         scriptSetReal32CB,                   &(ShipStaticInfos[0].trailSpriteRadius[0]), &(ShipStaticInfos[0]) },
    { "trailSpriteRadius2",                        scriptSetReal32CB,                   &(ShipStaticInfos[0].trailSpriteRadius[1]), &(ShipStaticInfos[0]) },
    { "trailSpriteRadius3",                        scriptSetReal32CB,                   &(ShipStaticInfos[0].trailSpriteRadius[2]), &(ShipStaticInfos[0]) },
    { "trailSpriteRadius4",                        scriptSetReal32CB,                   &(ShipStaticInfos[0].trailSpriteRadius[3]), &(ShipStaticInfos[0]) },
    { "trailSpriteOffset",                         scriptSetReal32CB,                   &(ShipStaticInfos[0].trailSpriteOffset[0]), &(ShipStaticInfos[0]) },
    { "trailSpriteOffset2",                        scriptSetReal32CB,                   &(ShipStaticInfos[0].trailSpriteOffset[1]), &(ShipStaticInfos[0]) },
    { "trailSpriteOffset3",                        scriptSetReal32CB,                   &(ShipStaticInfos[0].trailSpriteOffset[2]), &(ShipStaticInfos[0]) },
    { "trailSpriteOffset4",                        scriptSetReal32CB,                   &(ShipStaticInfos[0].trailSpriteOffset[3]), &(ShipStaticInfos[0]) },
    { "minimumZoomDistance",                       scriptSetReal32CB,                   &(ShipStaticInfos[0].minimumZoomDistance), &(ShipStaticInfos[0]) },
    { "renderlistFade",                            scriptSetReal32CB,                   &(ShipStaticInfos[0].renderlistFade), &(ShipStaticInfos[0]) },
    { "renderlistLimit",                           scriptSetReal32SqrCB,                &(ShipStaticInfos[0].renderlistLimitSqr), &(ShipStaticInfos[0]) },
    { "dockLightNear",                             scriptSetReal32CB,                   &(ShipStaticInfos[0].dockLightNear), &(ShipStaticInfos[0]) },
    { "dockLightFar",                              scriptSetReal32CB,                   &(ShipStaticInfos[0].dockLightFar), &(ShipStaticInfos[0]) },
    { "dockLightColor",                            scriptSetRGBCB,                      &(ShipStaticInfos[0].dockLightColor), &(ShipStaticInfos[0]) },
    { "hyperspaceColor",                           scriptSetRGBCB,                      &(ShipStaticInfos[0].hyperspaceColor), &(ShipStaticInfos[0]) },
    { "hyperspaceBoxScale",                        scriptSetReal32CB,                   &(ShipStaticInfos[0].hyperspaceBoxScale), &(ShipStaticInfos[0]) },   // CAT: new key
    { "preciseSelection",                          scriptSetUdwordCB,                   &(ShipStaticInfos[0].common.staticCollInfo.preciseSelection), &(ShipStaticInfos[0]) },
    { "madGunOpenDamagedHealthThreshold",          scriptSetReal32CB,                   &(ShipStaticInfos[0].madGunOpenDamagedHealthThreshold), &(ShipStaticInfos[0]) },
    { "madWingOpenDamagedHealthThreshold",         scriptSetReal32CB,                   &(ShipStaticInfos[0].madWingOpenDamagedHealthThreshold), &(ShipStaticInfos[0]) },
    { "externalBuild",                             scriptSetBool8,                      &(ShipStaticInfos[0].externalBuild), &(ShipStaticInfos[0]) },   // CAT: new key
    { "leechRUsPerSecond",                         scriptSetUdwordCB,                   &(ShipStaticInfos[0].leechRUsPerSecond), &(ShipStaticInfos[0]) },   // CAT: new key
    { "leechHPsPerSecond",                         scriptSetReal32CB,                   &(ShipStaticInfos[0].leechHPsPerSecond), &(ShipStaticInfos[0]) },   // CAT: new key
    { "Overlay_Selection_Scalar",                  scriptSetReal32CB,                   &(ShipStaticInfos[0].tacticalSelectionScale), &(ShipStaticInfos[0]) },
    { "beastAttackCascadeCost",                    scriptSetReal32CB,                   &(ShipStaticInfos[0].beastAttackCascadeCost), &(ShipStaticInfos[0]) },   // CAT: new key
    { "beastAttacksRequired",                      scriptSetSdwordCB,                   &(ShipStaticInfos[0].beastAttacksRequired), &(ShipStaticInfos[0]) },   // CAT: new key
    { "beastAttackConvertTo",                      cat_scriptSetBeastAttackConvertToCB, &(ShipStaticInfos[0].beastAttackConvertTo), &(ShipStaticInfos[0]) },   // CAT: new key
    { "linkDistance",                              scriptSetReal32CB,                   &(ShipStaticInfos[0].linkDistance), &(ShipStaticInfos[0]) },   // CAT: new key
    { "forceFieldProtectionDistanceScale",         scriptSetReal32CB,                   &(ShipStaticInfos[0].forceFieldProtectionDistanceScale), &(ShipStaticInfos[0]) },   // CAT: new key
    { "bUseRayTriangleIntersectionForHitLocation", scriptSetBool,                       &(ShipStaticInfos[0].bUseRayTriangleIntersectionForHitLocation), &(ShipStaticInfos[0]) },   // CAT: new key
    { "upgradeTimeModifier",                       scriptSetReal32CB,                   &(ShipStaticInfos[0].upgradeTimeModifier), &(ShipStaticInfos[0]) },   // CAT: new key

    END_SCRIPT_STRUCT_ENTRY
};

scriptStructEntry HierarchyBindingTable[] =
{
    { "BindGun",                        setGunBindInfo,     0, 0 },
    { "BindAnimations",                 setAnimationBindInfo,0, 0 },

    END_SCRIPT_STRUCT_ENTRY
};

scriptStructEntry MadMaxMadInfoLoad[] =
{
    { "MadStartInfo",                   setMadStartInfo,0, 0 },
    { "MadGunAnims",                    setGunAnimInfo,0, 0 },
    { "MadDockAnims",                   setDockAnimInfo,0, 0 },
    { "MadMothershipDockingAnims",      setMothershipDoorAnimInfo,0,0 },
    { "MadSpecialCaseAnims",      setSpecialCaseAnimInfo,0,0 },
    // CAT: two new keys (table 0x008b2f88 rows 5 and 6)
    { "NUMBER_OF_ANIMS",                cat_madNumberOfAnimsSetCB,0,0 },
    { "ANIM",                           setMadAnim,0,0 },

    END_SCRIPT_STRUCT_ENTRY
};

scriptStructEntry AsteroidStaticScriptTable[] =
{
    { "LODFile",        LODScriptLoad,       &(asteroidStaticInfos[0].common.LOD),  &(asteroidStaticInfos[0]) },
    { "pMexData",       loadMexDataCB,       &(asteroidStaticInfos[0].common.pMexData),  &(asteroidStaticInfos[0]) },
    { "mass",           scriptSetReal32CB,   &(asteroidStaticInfos[0].common.mass),       &(asteroidStaticInfos[0]) },
    { "momentOfInertiaX",scriptSetReal32CB,  &(asteroidStaticInfos[0].common.momentOfInertiaX),  &(asteroidStaticInfos[0]) },
    { "momentOfInertiaY",scriptSetReal32CB,  &(asteroidStaticInfos[0].common.momentOfInertiaY),  &(asteroidStaticInfos[0]) },
    { "momentOfInertiaZ",scriptSetReal32CB,  &(asteroidStaticInfos[0].common.momentOfInertiaZ),  &(asteroidStaticInfos[0]) },
    { "collBoxScaleRight",scriptSetReal32CB,  &(asteroidStaticInfos[0].common.staticCollInfo.collBoxScaleRight),  &(asteroidStaticInfos[0]) },
    { "collBoxScaleUp",scriptSetReal32CB,  &(asteroidStaticInfos[0].common.staticCollInfo.collBoxScaleUp),  &(asteroidStaticInfos[0]) },
    { "collBoxScaleForward",scriptSetReal32CB,  &(asteroidStaticInfos[0].common.staticCollInfo.collBoxScaleForward),  &(asteroidStaticInfos[0]) },
    { "collBoxOffsetX",scriptSetReal32CB,  &(asteroidStaticInfos[0].common.staticCollInfo.collBoxOffsetX),  &(asteroidStaticInfos[0]) },
    { "collBoxOffsetY",scriptSetReal32CB,  &(asteroidStaticInfos[0].common.staticCollInfo.collBoxOffsetY),  &(asteroidStaticInfos[0]) },
    { "collBoxOffsetZ",scriptSetReal32CB,  &(asteroidStaticInfos[0].common.staticCollInfo.collBoxOffsetZ),  &(asteroidStaticInfos[0]) },

    { "immobile",        scriptSetBool8,     &(asteroidStaticInfos[0].common.immobile),  &(asteroidStaticInfos[0]) },
    { "rightOfWay",      scriptSetSbyteCB,   &(asteroidStaticInfos[0].common.rightOfWay),  &(asteroidStaticInfos[0]) },
    { "approxcollmodifier", scriptSetReal32CB,   &(asteroidStaticInfos[0].common.staticCollInfo.approxcollmodifier),   &(asteroidStaticInfos[0]) },
    { "avoidcollmodifier", scriptSetReal32CB,   &(asteroidStaticInfos[0].common.staticCollInfo.avoidcollmodifier),   &(asteroidStaticInfos[0]) },
    { "maxvelocity",    scriptSetReal32CB,   &(asteroidStaticInfos[0].common.maxvelocity),   &(asteroidStaticInfos[0]) },
    { "maxrot",         scriptSetReal32CB,   &(asteroidStaticInfos[0].common.maxrot),   &(asteroidStaticInfos[0]) },
    { "maxhealth",      scriptSetReal32CB,   &(asteroidStaticInfos[0].health.maxhealth),  &(asteroidStaticInfos[0]) },
    { "minCollDamage",      scriptSetReal32CB,   &(asteroidStaticInfos[0].health.minCollDamage),  &(asteroidStaticInfos[0]) },
    { "maxCollDamage",      scriptSetReal32CB,   &(asteroidStaticInfos[0].health.maxCollDamage),  &(asteroidStaticInfos[0]) },
    { "resourcevalue",  scriptSetSdwordCB,   &(asteroidStaticInfos[0].resource.resourcevalue),  &(asteroidStaticInfos[0]) },
    { "harvestableByMultipleShips",  scriptSetUdwordCB,   &(asteroidStaticInfos[0].resource.harvestableByMultipleShips),  &(asteroidStaticInfos[0]) },
    // CAT: no asteroidCanShrink/asteroidCanBreak/asteroidMinShrink/breakinto[] keys (the exe's table 0x008b3008 ends
    //      with harvestableByMultipleShips and preciseSelection); breaking is NUMBER_OF_ASTEROID_PIECES/AsteroidBreakInfo
    //      (cat_scriptAsteroidBreakInfoLoad 0x00528190, called by InitStatAsteroidInfo)
    { "preciseSelection",scriptSetUdwordCB, &(ShipStaticInfos[0].common.staticCollInfo.preciseSelection),  &(ShipStaticInfos[0]) },

    END_SCRIPT_STRUCT_ENTRY
};

scriptStructEntry DustCloudStaticScriptTable[] =
{
    { "LODFile",        LODScriptLoad,       &(dustcloudStaticInfos[0].common.LOD),  &(dustcloudStaticInfos[0]) },
    { "pMexData",       loadMexDataCB,       &(dustcloudStaticInfos[0].common.pMexData),  &(dustcloudStaticInfos[0]) },
    { "mass",           scriptSetReal32CB,   &(dustcloudStaticInfos[0].common.mass),      &(dustcloudStaticInfos[0]) },
    { "momentOfInertiaX",scriptSetReal32CB,  &(dustcloudStaticInfos[0].common.momentOfInertiaX),  &(dustcloudStaticInfos[0]) },
    { "momentOfInertiaY",scriptSetReal32CB,  &(dustcloudStaticInfos[0].common.momentOfInertiaY),  &(dustcloudStaticInfos[0]) },
    { "momentOfInertiaZ",scriptSetReal32CB,  &(dustcloudStaticInfos[0].common.momentOfInertiaZ),  &(dustcloudStaticInfos[0]) },
    { "collBoxScaleRight",scriptSetReal32CB,  &(dustcloudStaticInfos[0].common.staticCollInfo.collBoxScaleRight),  &(dustcloudStaticInfos[0]) },
    { "collBoxScaleUp",scriptSetReal32CB,  &(dustcloudStaticInfos[0].common.staticCollInfo.collBoxScaleUp),  &(dustcloudStaticInfos[0]) },
    { "collBoxScaleForward",scriptSetReal32CB,  &(dustcloudStaticInfos[0].common.staticCollInfo.collBoxScaleForward),  &(dustcloudStaticInfos[0]) },

    { "collBoxOffsetX",scriptSetReal32CB,  &(dustcloudStaticInfos[0].common.staticCollInfo.collBoxOffsetX),  &(dustcloudStaticInfos[0]) },
    { "collBoxOffsetY",scriptSetReal32CB,  &(dustcloudStaticInfos[0].common.staticCollInfo.collBoxOffsetY),  &(dustcloudStaticInfos[0]) },
    { "collBoxOffsetZ",scriptSetReal32CB,  &(dustcloudStaticInfos[0].common.staticCollInfo.collBoxOffsetZ),  &(dustcloudStaticInfos[0]) },

    { "immobile",        scriptSetBool8,     &(dustcloudStaticInfos[0].common.immobile),  &(dustcloudStaticInfos[0]) },
    { "approxcollmodifier", scriptSetReal32CB,  &(dustcloudStaticInfos[0].common.staticCollInfo.approxcollmodifier),   &(dustcloudStaticInfos[0]) },
    { "avoidcollmodifier", scriptSetReal32CB,  &(dustcloudStaticInfos[0].common.staticCollInfo.avoidcollmodifier),   &(dustcloudStaticInfos[0]) },
    { "maxvelocity",    scriptSetReal32CB,   &(dustcloudStaticInfos[0].common.maxvelocity),   &(dustcloudStaticInfos[0]) },
    { "maxrot",         scriptSetReal32CB,   &(dustcloudStaticInfos[0].common.maxrot),   &(dustcloudStaticInfos[0]) },
    { "maxhealth",      scriptSetReal32CB,   &(dustcloudStaticInfos[0].health.maxhealth),  &(dustcloudStaticInfos[0]) },
    { "minCollDamage",      scriptSetReal32CB,   &(dustcloudStaticInfos[0].health.minCollDamage),  &(dustcloudStaticInfos[0]) },
    { "maxCollDamage",      scriptSetReal32CB,   &(dustcloudStaticInfos[0].health.maxCollDamage),  &(dustcloudStaticInfos[0]) },
    { "resourcevalue",  scriptSetSdwordCB,   &(dustcloudStaticInfos[0].resource.resourcevalue),  &(dustcloudStaticInfos[0]) },
    { "harvestableByMultipleShips",  scriptSetUdwordCB,  &(dustcloudStaticInfos[0].resource.harvestableByMultipleShips),  &(dustcloudStaticInfos[0]) },

    { "dustcloudCanShrink",scriptSetBool8,  &(dustcloudStaticInfos[0].dustcloudCanShrink),  &(dustcloudStaticInfos[0]) },
    { "dustcloudMinShrink",scriptSetReal32CB,  &(dustcloudStaticInfos[0].dustcloudMinShrink),  &(dustcloudStaticInfos[0]) },
    { "preciseSelection",scriptSetUdwordCB, &(ShipStaticInfos[0].common.staticCollInfo.preciseSelection),  &(ShipStaticInfos[0]) },

    END_SCRIPT_STRUCT_ENTRY
};

scriptStructEntry NebulaStaticScriptTable[] =
{
    { "LODFile", LODScriptLoad, &(nebulaStaticInfos[0].common.LOD), &(nebulaStaticInfos[0]) },
    { "pMexData", loadMexDataCB, &(nebulaStaticInfos[0].common.pMexData), &(nebulaStaticInfos[0]) },
    { "mass", scriptSetReal32CB, &(nebulaStaticInfos[0].common.mass), &(nebulaStaticInfos[0]) },
    { "momentOfInertiaX", scriptSetReal32CB, &(nebulaStaticInfos[0].common.momentOfInertiaX), &(nebulaStaticInfos[0]) },
    { "momentOfInertiaY", scriptSetReal32CB, &(nebulaStaticInfos[0].common.momentOfInertiaY), &(nebulaStaticInfos[0]) },
    { "momentOfInertiaZ", scriptSetReal32CB, &(nebulaStaticInfos[0].common.momentOfInertiaZ), &(nebulaStaticInfos[0]) },
    { "collBoxScaleRight",scriptSetReal32CB,  &(nebulaStaticInfos[0].common.staticCollInfo.collBoxScaleRight),  &(nebulaStaticInfos[0]) },
    { "collBoxScaleUp",scriptSetReal32CB,  &(nebulaStaticInfos[0].common.staticCollInfo.collBoxScaleUp),  &(nebulaStaticInfos[0]) },
    { "collBoxScaleForward",scriptSetReal32CB,  &(nebulaStaticInfos[0].common.staticCollInfo.collBoxScaleForward),  &(nebulaStaticInfos[0]) },
    { "collBoxOffsetX",scriptSetReal32CB,  &(nebulaStaticInfos[0].common.staticCollInfo.collBoxOffsetX),  &(nebulaStaticInfos[0]) },
    { "collBoxOffsetY",scriptSetReal32CB,  &(nebulaStaticInfos[0].common.staticCollInfo.collBoxOffsetY),  &(nebulaStaticInfos[0]) },
    { "collBoxOffsetZ",scriptSetReal32CB,  &(nebulaStaticInfos[0].common.staticCollInfo.collBoxOffsetZ),  &(nebulaStaticInfos[0]) },
    { "immobile", scriptSetBool8, &(nebulaStaticInfos[0].common.immobile), &(nebulaStaticInfos[0]) },
    { "approxcollmodifier", scriptSetReal32CB,  &(nebulaStaticInfos[0].common.staticCollInfo.approxcollmodifier),   &(nebulaStaticInfos[0]) },
    { "avoidcollmodifier", scriptSetReal32CB,  &(nebulaStaticInfos[0].common.staticCollInfo.avoidcollmodifier),   &(nebulaStaticInfos[0]) },
    { "maxvelocity",    scriptSetReal32CB,   &(nebulaStaticInfos[0].common.maxvelocity),   &(nebulaStaticInfos[0]) },
    { "maxrot",         scriptSetReal32CB,   &(nebulaStaticInfos[0].common.maxrot),   &(nebulaStaticInfos[0]) },
    { "maxhealth",      scriptSetReal32CB,   &(nebulaStaticInfos[0].health.maxhealth),  &(nebulaStaticInfos[0]) },
    { "minCollDamage",      scriptSetReal32CB,   &(nebulaStaticInfos[0].health.minCollDamage),  &(nebulaStaticInfos[0]) },
    { "maxCollDamage",      scriptSetReal32CB,   &(nebulaStaticInfos[0].health.maxCollDamage),  &(nebulaStaticInfos[0]) },
    { "resourcevalue",  scriptSetSdwordCB,   &(nebulaStaticInfos[0].resource.resourcevalue),  &(nebulaStaticInfos[0]) },
    { "harvestableByMultipleShips",  scriptSetUdwordCB,  &(nebulaStaticInfos[0].resource.harvestableByMultipleShips),  &(nebulaStaticInfos[0]) },

    { "nebulaCanShrink",scriptSetBool8,  &(nebulaStaticInfos[0].nebulaCanShrink),  &(nebulaStaticInfos[0]) },
    { "nebulaMinShrink",scriptSetReal32CB,  &(nebulaStaticInfos[0].nebulaMinShrink),  &(nebulaStaticInfos[0]) },
    { "preciseSelection",scriptSetUdwordCB, &(ShipStaticInfos[0].common.staticCollInfo.preciseSelection),  &(ShipStaticInfos[0]) },

    END_SCRIPT_STRUCT_ENTRY
};

scriptStructEntry GasCloudStaticScriptTable[] =
{
    { "LODFile",        LODScriptLoad,       &(gascloudStaticInfos[0].common.LOD),  &(gascloudStaticInfos[0]) },
    { "pMexData",       loadMexDataCB,       &(gascloudStaticInfos[0].common.pMexData),  &(gascloudStaticInfos[0]) },
    { "mass",           scriptSetReal32CB,   &(gascloudStaticInfos[0].common.mass),      &(gascloudStaticInfos[0]) },
    { "momentOfInertiaX",scriptSetReal32CB,  &(gascloudStaticInfos[0].common.momentOfInertiaX),  &(gascloudStaticInfos[0]) },
    { "momentOfInertiaY",scriptSetReal32CB,  &(gascloudStaticInfos[0].common.momentOfInertiaY),  &(gascloudStaticInfos[0]) },
    { "momentOfInertiaZ",scriptSetReal32CB,  &(gascloudStaticInfos[0].common.momentOfInertiaZ),  &(gascloudStaticInfos[0]) },
    { "collBoxScaleRight",scriptSetReal32CB,  &(gascloudStaticInfos[0].common.staticCollInfo.collBoxScaleRight),  &(gascloudStaticInfos[0]) },
    { "collBoxScaleUp",scriptSetReal32CB,  &(gascloudStaticInfos[0].common.staticCollInfo.collBoxScaleUp),  &(gascloudStaticInfos[0]) },
    { "collBoxScaleForward",scriptSetReal32CB,  &(gascloudStaticInfos[0].common.staticCollInfo.collBoxScaleForward),  &(gascloudStaticInfos[0]) },
    { "collBoxOffsetX",scriptSetReal32CB,  &(gascloudStaticInfos[0].common.staticCollInfo.collBoxOffsetX),  &(gascloudStaticInfos[0]) },
    { "collBoxOffsetY",scriptSetReal32CB,  &(gascloudStaticInfos[0].common.staticCollInfo.collBoxOffsetY),  &(gascloudStaticInfos[0]) },
    { "collBoxOffsetZ",scriptSetReal32CB,  &(gascloudStaticInfos[0].common.staticCollInfo.collBoxOffsetZ),  &(gascloudStaticInfos[0]) },

    { "immobile",        scriptSetBool8,     &(gascloudStaticInfos[0].common.immobile),  &(gascloudStaticInfos[0]) },
    { "approxcollmodifier", scriptSetReal32CB,  &(gascloudStaticInfos[0].common.staticCollInfo.approxcollmodifier),   &(gascloudStaticInfos[0]) },
    { "avoidcollmodifier", scriptSetReal32CB,  &(gascloudStaticInfos[0].common.staticCollInfo.avoidcollmodifier),   &(gascloudStaticInfos[0]) },
    { "maxvelocity",    scriptSetReal32CB,   &(gascloudStaticInfos[0].common.maxvelocity),   &(gascloudStaticInfos[0]) },
    { "maxrot",         scriptSetReal32CB,   &(gascloudStaticInfos[0].common.maxrot),   &(gascloudStaticInfos[0]) },
    { "maxhealth",      scriptSetReal32CB,   &(gascloudStaticInfos[0].health.maxhealth),  &(gascloudStaticInfos[0]) },
    { "minCollDamage",      scriptSetReal32CB,   &(gascloudStaticInfos[0].health.minCollDamage),  &(gascloudStaticInfos[0]) },
    { "maxCollDamage",      scriptSetReal32CB,   &(gascloudStaticInfos[0].health.maxCollDamage),  &(gascloudStaticInfos[0]) },
    { "resourcevalue",  scriptSetSdwordCB,   &(gascloudStaticInfos[0].resource.resourcevalue),  &(gascloudStaticInfos[0]) },
    { "harvestableByMultipleShips",  scriptSetUdwordCB,  &(gascloudStaticInfos[0].resource.harvestableByMultipleShips),  &(gascloudStaticInfos[0]) },

    { "gascloudCanShrink",scriptSetBool8,  &(gascloudStaticInfos[0].gascloudCanShrink),  &(gascloudStaticInfos[0]) },
    { "gascloudMinShrink",scriptSetReal32CB,  &(gascloudStaticInfos[0].gascloudMinShrink),  &(gascloudStaticInfos[0]) },
    { "preciseSelection",scriptSetUdwordCB, &(ShipStaticInfos[0].common.staticCollInfo.preciseSelection),  &(ShipStaticInfos[0]) },

    END_SCRIPT_STRUCT_ENTRY
};

scriptStructEntry DerelictStaticScriptTable[] =
{
    { "LODFile",        LODScriptLoad,       &(derelictStaticInfos[0].common.LOD),  &(derelictStaticInfos[0]) },
    { "pMexData",       loadMexDataCB,       &(derelictStaticInfos[0].common.pMexData),  &(derelictStaticInfos[0]) },
    { "mass",           scriptSetReal32CB,   &(derelictStaticInfos[0].common.mass),       &(derelictStaticInfos[0]) },
    { "momentOfInertiaX",scriptSetReal32CB,  &(derelictStaticInfos[0].common.momentOfInertiaX),  &(derelictStaticInfos[0]) },
    { "momentOfInertiaY",scriptSetReal32CB,  &(derelictStaticInfos[0].common.momentOfInertiaY),  &(derelictStaticInfos[0]) },
    { "momentOfInertiaZ",scriptSetReal32CB,  &(derelictStaticInfos[0].common.momentOfInertiaZ),  &(derelictStaticInfos[0]) },
    { "collBoxScaleRight",scriptSetReal32CB,  &(derelictStaticInfos[0].common.staticCollInfo.collBoxScaleRight),  &(derelictStaticInfos[0]) },
    { "collBoxScaleUp",scriptSetReal32CB,  &(derelictStaticInfos[0].common.staticCollInfo.collBoxScaleUp),  &(derelictStaticInfos[0]) },
    { "collBoxScaleForward",scriptSetReal32CB,  &(derelictStaticInfos[0].common.staticCollInfo.collBoxScaleForward),  &(derelictStaticInfos[0]) },
    { "collBoxOffsetX",scriptSetReal32CB,  &(derelictStaticInfos[0].common.staticCollInfo.collBoxOffsetX),  &(derelictStaticInfos[0]) },
    { "collBoxOffsetY",scriptSetReal32CB,  &(derelictStaticInfos[0].common.staticCollInfo.collBoxOffsetY),  &(derelictStaticInfos[0]) },
    { "collBoxOffsetZ",scriptSetReal32CB,  &(derelictStaticInfos[0].common.staticCollInfo.collBoxOffsetZ),  &(derelictStaticInfos[0]) },
    { "rightOfWay",      scriptSetSbyteCB,   &(derelictStaticInfos[0].common.rightOfWay),  &(derelictStaticInfos[0]) },
    { "immobile",        scriptSetBool8,     &(derelictStaticInfos[0].common.immobile),  &(derelictStaticInfos[0]) },
    { "approxcollmodifier", scriptSetReal32CB,   &(derelictStaticInfos[0].common.staticCollInfo.approxcollmodifier),   &(derelictStaticInfos[0]) },
    { "avoidcollmodifier", scriptSetReal32CB,   &(derelictStaticInfos[0].common.staticCollInfo.avoidcollmodifier),   &(derelictStaticInfos[0]) },
    { "maxvelocity",    scriptSetReal32CB,   &(derelictStaticInfos[0].common.maxvelocity),   &(derelictStaticInfos[0]) },
    { "maxrot",         scriptSetReal32CB,   &(derelictStaticInfos[0].common.maxrot),   &(derelictStaticInfos[0]) },
    { "maxhealth",      scriptSetReal32CB,   &(derelictStaticInfos[0].health.maxhealth),  &(derelictStaticInfos[0]) },
    { "minCollDamage",      scriptSetReal32CB,   &(derelictStaticInfos[0].health.minCollDamage),  &(derelictStaticInfos[0]) },
    { "maxCollDamage",      scriptSetReal32CB,   &(derelictStaticInfos[0].health.maxCollDamage),  &(derelictStaticInfos[0]) },

    { "thruststrength[TRANS_UP]",       scriptSetReal32CB,   &(derelictStaticInfos[0].guidance.thruststrengthstat[TRANS_UP]),  &(derelictStaticInfos[0]) },
    { "thruststrength[TRANS_DOWN]",     scriptSetReal32CB,   &(derelictStaticInfos[0].guidance.thruststrengthstat[TRANS_DOWN]),  &(derelictStaticInfos[0]) },
    { "thruststrength[TRANS_RIGHT]",    scriptSetReal32CB,   &(derelictStaticInfos[0].guidance.thruststrengthstat[TRANS_RIGHT]),  &(derelictStaticInfos[0]) },
    { "thruststrength[TRANS_LEFT]",     scriptSetReal32CB,   &(derelictStaticInfos[0].guidance.thruststrengthstat[TRANS_LEFT]),  &(derelictStaticInfos[0]) },
    { "thruststrength[TRANS_FORWARD]",  scriptSetReal32CB,   &(derelictStaticInfos[0].guidance.thruststrengthstat[TRANS_FORWARD]),  &(derelictStaticInfos[0]) },
    { "thruststrength[TRANS_BACK]",     scriptSetReal32CB,   &(derelictStaticInfos[0].guidance.thruststrengthstat[TRANS_BACKWARD]),  &(derelictStaticInfos[0]) },
    { "rotstrength[ROT_YAWLEFT]",       scriptSetReal32CB,   &(derelictStaticInfos[0].guidance.rotstrengthstat[ROT_YAWLEFT]),  &(derelictStaticInfos[0]) },
    { "rotstrength[ROT_YAWRIGHT]",      scriptSetReal32CB,   &(derelictStaticInfos[0].guidance.rotstrengthstat[ROT_YAWRIGHT]),  &(derelictStaticInfos[0]) },
    { "rotstrength[ROT_PITCHUP]",       scriptSetReal32CB,   &(derelictStaticInfos[0].guidance.rotstrengthstat[ROT_PITCHUP]),  &(derelictStaticInfos[0]) },
    { "rotstrength[ROT_PITCHDOWN]",     scriptSetReal32CB,   &(derelictStaticInfos[0].guidance.rotstrengthstat[ROT_PITCHDOWN]),  &(derelictStaticInfos[0]) },
    { "rotstrength[ROT_ROLLRIGHT]",     scriptSetReal32CB,   &(derelictStaticInfos[0].guidance.rotstrengthstat[ROT_ROLLRIGHT]),  &(derelictStaticInfos[0]) },
    { "rotstrength[ROT_ROLLLEFT]",      scriptSetReal32CB,   &(derelictStaticInfos[0].guidance.rotstrengthstat[ROT_ROLLLEFT]),  &(derelictStaticInfos[0]) },
    { "turnspeed[TURN_YAW]",            scriptSetReal32CB,   &(derelictStaticInfos[0].guidance.turnspeedstat[TURN_YAW]),  &(derelictStaticInfos[0]) },
    { "turnspeed[TURN_PITCH]",          scriptSetReal32CB,   &(derelictStaticInfos[0].guidance.turnspeedstat[TURN_PITCH]),  &(derelictStaticInfos[0]) },
    { "turnspeed[TURN_ROLL]",           scriptSetReal32CB,   &(derelictStaticInfos[0].guidance.turnspeedstat[TURN_ROLL]),  &(derelictStaticInfos[0]) },

    { "pitchdescend",               scriptSetReal32CB,   &(derelictStaticInfos[0].shipOrDerelict.pitchdescend),  &(derelictStaticInfos[0]) },
    { "sinbank",                    scriptSetReal32CB,   &(derelictStaticInfos[0].shipOrDerelict.sinbank),  &(derelictStaticInfos[0]) },
    { "pitchturn",                  scriptSetReal32CB,   &(derelictStaticInfos[0].shipOrDerelict.pitchturn),  &(derelictStaticInfos[0]) },

    { "preciseSelection",           scriptSetUdwordCB, &(ShipStaticInfos[0].common.staticCollInfo.preciseSelection),  &(ShipStaticInfos[0]) },
    // CAT: NUMBER_OF_SALVAGE_POINTS is gone (the string is not in cataclysm.exe; no HW1-style salvage statics)

    { "scaleFactor",          scriptSetReal32CB,   &(derelictStaticInfos[0].scaleFactor),  &(derelictStaticInfos[0]) },
    { "worldRender",                scriptSetBool,   &(derelictStaticInfos[0].worldRender),  &(derelictStaticInfos[0]) },

    { "salvageable",                scriptSetBool,   &(derelictStaticInfos[0].salvageable),  &(derelictStaticInfos[0]) },

    { "minimumZoomDistance",  scriptSetReal32CB,     &(derelictStaticInfos[0].minimumZoomDistance),  &(derelictStaticInfos[0]) },
    { "renderlistFade",       scriptSetReal32CB,     &(derelictStaticInfos[0].renderlistFade),       &(derelictStaticInfos[0]) },
    { "renderlistLimit",      scriptSetReal32SqrCB,  &(derelictStaticInfos[0].renderlistLimitSqr),   &(derelictStaticInfos[0]) },

    END_SCRIPT_STRUCT_ENTRY
};

scriptStructEntry MissileStaticScriptTable[] =
{
    { "LODFile",        LODScriptLoad,       &(missileStaticInfos[0].common.LOD),  &(missileStaticInfos[0]) },
    { "pMexData",       loadMexDataCB,       &(missileStaticInfos[0].common.pMexData),  &(missileStaticInfos[0]) },
    { "mass",           scriptSetReal32CB,   &(missileStaticInfos[0].common.mass),       &(missileStaticInfos[0]) },
    { "momentOfInertiaX",scriptSetReal32CB,  &(missileStaticInfos[0].common.momentOfInertiaX),  &(missileStaticInfos[0]) },
    { "momentOfInertiaY",scriptSetReal32CB,  &(missileStaticInfos[0].common.momentOfInertiaY),  &(missileStaticInfos[0]) },
    { "momentOfInertiaZ",scriptSetReal32CB,  &(missileStaticInfos[0].common.momentOfInertiaZ),  &(missileStaticInfos[0]) },
    { "collBoxScaleRight",scriptSetReal32CB,  &(missileStaticInfos[0].common.staticCollInfo.collBoxScaleRight),  &(missileStaticInfos[0]) },
    { "collBoxScaleUp",scriptSetReal32CB,  &(missileStaticInfos[0].common.staticCollInfo.collBoxScaleUp),  &(missileStaticInfos[0]) },
    { "collBoxScaleForward",scriptSetReal32CB,  &(missileStaticInfos[0].common.staticCollInfo.collBoxScaleForward),  &(missileStaticInfos[0]) },
    { "collBoxOffsetX",scriptSetReal32CB,  &(missileStaticInfos[0].common.staticCollInfo.collBoxOffsetX),  &(missileStaticInfos[0]) },
    { "collBoxOffsetY",scriptSetReal32CB,  &(missileStaticInfos[0].common.staticCollInfo.collBoxOffsetY),  &(missileStaticInfos[0]) },
    { "collBoxOffsetZ",scriptSetReal32CB,  &(missileStaticInfos[0].common.staticCollInfo.collBoxOffsetZ),  &(missileStaticInfos[0]) },
    { "maxvelocity",    scriptSetReal32CB,   &(missileStaticInfos[0].common.maxvelocity),   &(missileStaticInfos[0]) },
    { "maxrot",         scriptSetReal32CB,   &(missileStaticInfos[0].common.maxrot),   &(missileStaticInfos[0]) },
    { "maxhealth",      scriptSetReal32CB,   &(missileStaticInfos[0].health.maxhealth),  &(missileStaticInfos[0]) },
    { "minCollDamage",      scriptSetReal32CB,   &(missileStaticInfos[0].health.minCollDamage),  &(missileStaticInfos[0]) },
    { "maxCollDamage",      scriptSetReal32CB,   &(missileStaticInfos[0].health.maxCollDamage),  &(missileStaticInfos[0]) },
    { "thruststrength[TRANS_UP]",       scriptSetReal32CB,   &(missileStaticInfos[0].guidance.thruststrengthstat[TRANS_UP]),  &(missileStaticInfos[0]) },
    { "thruststrength[TRANS_DOWN]",     scriptSetReal32CB,   &(missileStaticInfos[0].guidance.thruststrengthstat[TRANS_DOWN]),  &(missileStaticInfos[0]) },
    { "thruststrength[TRANS_RIGHT]",    scriptSetReal32CB,   &(missileStaticInfos[0].guidance.thruststrengthstat[TRANS_RIGHT]),  &(missileStaticInfos[0]) },
    { "thruststrength[TRANS_LEFT]",     scriptSetReal32CB,   &(missileStaticInfos[0].guidance.thruststrengthstat[TRANS_LEFT]),  &(missileStaticInfos[0]) },
    { "thruststrength[TRANS_FORWARD]",  scriptSetReal32CB,   &(missileStaticInfos[0].guidance.thruststrengthstat[TRANS_FORWARD]),  &(missileStaticInfos[0]) },
    { "thruststrength[TRANS_BACK]",     scriptSetReal32CB,   &(missileStaticInfos[0].guidance.thruststrengthstat[TRANS_BACKWARD]),  &(missileStaticInfos[0]) },
    { "rotstrength[ROT_YAWLEFT]",       scriptSetReal32CB,   &(missileStaticInfos[0].guidance.rotstrengthstat[ROT_YAWLEFT]),  &(missileStaticInfos[0]) },
    { "rotstrength[ROT_YAWRIGHT]",      scriptSetReal32CB,   &(missileStaticInfos[0].guidance.rotstrengthstat[ROT_YAWRIGHT]),  &(missileStaticInfos[0]) },
    { "rotstrength[ROT_PITCHUP]",       scriptSetReal32CB,   &(missileStaticInfos[0].guidance.rotstrengthstat[ROT_PITCHUP]),  &(missileStaticInfos[0]) },
    { "rotstrength[ROT_PITCHDOWN]",     scriptSetReal32CB,   &(missileStaticInfos[0].guidance.rotstrengthstat[ROT_PITCHDOWN]),  &(missileStaticInfos[0]) },
    { "rotstrength[ROT_ROLLRIGHT]",     scriptSetReal32CB,   &(missileStaticInfos[0].guidance.rotstrengthstat[ROT_ROLLRIGHT]),  &(missileStaticInfos[0]) },
    { "rotstrength[ROT_ROLLLEFT]",      scriptSetReal32CB,   &(missileStaticInfos[0].guidance.rotstrengthstat[ROT_ROLLLEFT]),  &(missileStaticInfos[0]) },
    { "turnspeed[TURN_YAW]",            scriptSetReal32CB,   &(missileStaticInfos[0].guidance.turnspeedstat[TURN_YAW]),  &(missileStaticInfos[0]) },
    { "turnspeed[TURN_PITCH]",          scriptSetReal32CB,   &(missileStaticInfos[0].guidance.turnspeedstat[TURN_PITCH]),  &(missileStaticInfos[0]) },
    { "turnspeed[TURN_ROLL]",           scriptSetReal32CB,   &(missileStaticInfos[0].guidance.turnspeedstat[TURN_ROLL]),  &(missileStaticInfos[0]) },
    { "preciseSelection",scriptSetUdwordCB, &(ShipStaticInfos[0].common.staticCollInfo.preciseSelection),  &(ShipStaticInfos[0]) },

    END_SCRIPT_STRUCT_ENTRY
};

scriptStructEntry MineStaticScriptTable[] =
{
    { "LODFile",        LODScriptLoad,       &(mineStaticInfos[0].common.LOD),  &(mineStaticInfos[0]) },
    { "pMexData",       loadMexDataCB,       &(mineStaticInfos[0].common.pMexData),  &(mineStaticInfos[0]) },
    { "mass",           scriptSetReal32CB,   &(mineStaticInfos[0].common.mass),       &(mineStaticInfos[0]) },
    { "momentOfInertiaX",scriptSetReal32CB,  &(mineStaticInfos[0].common.momentOfInertiaX),  &(mineStaticInfos[0]) },
    { "momentOfInertiaY",scriptSetReal32CB,  &(mineStaticInfos[0].common.momentOfInertiaY),  &(mineStaticInfos[0]) },
    { "momentOfInertiaZ",scriptSetReal32CB,  &(mineStaticInfos[0].common.momentOfInertiaZ),  &(mineStaticInfos[0]) },
    { "collBoxScaleRight",scriptSetReal32CB,  &(mineStaticInfos[0].common.staticCollInfo.collBoxScaleRight),  &(asteroidStaticInfos[0]) },
    { "collBoxScaleUp",scriptSetReal32CB,  &(mineStaticInfos[0].common.staticCollInfo.collBoxScaleUp),  &(asteroidStaticInfos[0]) },
    { "collBoxScaleForward",scriptSetReal32CB,  &(mineStaticInfos[0].common.staticCollInfo.collBoxScaleForward),  &(asteroidStaticInfos[0]) },
    { "maxvelocity",    scriptSetReal32CB,   &(mineStaticInfos[0].common.maxvelocity),   &(mineStaticInfos[0]) },
    { "collBoxOffsetX",scriptSetReal32CB,  &(mineStaticInfos[0].common.staticCollInfo.collBoxOffsetX),  &(mineStaticInfos[0]) },
    { "collBoxOffsetY",scriptSetReal32CB,  &(mineStaticInfos[0].common.staticCollInfo.collBoxOffsetY),  &(mineStaticInfos[0]) },
    { "collBoxOffsetZ",scriptSetReal32CB,  &(mineStaticInfos[0].common.staticCollInfo.collBoxOffsetZ),  &(mineStaticInfos[0]) },
    { "maxvelocity_FORCED",    scriptSetReal32CB,   &(mineStaticInfos[0].maxvelocity_FORCED),   &(mineStaticInfos[0]) },
    { "maxrot",         scriptSetReal32CB,   &(mineStaticInfos[0].common.maxrot),   &(mineStaticInfos[0]) },
    { "maxhealth",      scriptSetReal32CB,   &(mineStaticInfos[0].health.maxhealth),  &(mineStaticInfos[0]) },
    { "minCollDamage",      scriptSetReal32CB,   &(mineStaticInfos[0].health.minCollDamage),  &(mineStaticInfos[0]) },
    { "maxCollDamage",      scriptSetReal32CB,   &(mineStaticInfos[0].health.maxCollDamage),  &(mineStaticInfos[0]) },
    { "thruststrength[TRANS_UP]",       scriptSetReal32CB,   &(mineStaticInfos[0].guidance.thruststrengthstat[TRANS_UP]),  &(mineStaticInfos[0]) },
    { "thruststrength[TRANS_DOWN]",     scriptSetReal32CB,   &(mineStaticInfos[0].guidance.thruststrengthstat[TRANS_DOWN]),  &(mineStaticInfos[0]) },
    { "thruststrength[TRANS_RIGHT]",    scriptSetReal32CB,   &(mineStaticInfos[0].guidance.thruststrengthstat[TRANS_RIGHT]),  &(mineStaticInfos[0]) },
    { "thruststrength[TRANS_LEFT]",     scriptSetReal32CB,   &(mineStaticInfos[0].guidance.thruststrengthstat[TRANS_LEFT]),  &(mineStaticInfos[0]) },
    { "thruststrength[TRANS_FORWARD]",  scriptSetReal32CB,   &(mineStaticInfos[0].guidance.thruststrengthstat[TRANS_FORWARD]),  &(mineStaticInfos[0]) },
    { "thruststrength[TRANS_BACK]",     scriptSetReal32CB,   &(mineStaticInfos[0].guidance.thruststrengthstat[TRANS_BACKWARD]),  &(mineStaticInfos[0]) },
    { "rotstrength[ROT_YAWLEFT]",       scriptSetReal32CB,   &(mineStaticInfos[0].guidance.rotstrengthstat[ROT_YAWLEFT]),  &(mineStaticInfos[0]) },
    { "rotstrength[ROT_YAWRIGHT]",      scriptSetReal32CB,   &(mineStaticInfos[0].guidance.rotstrengthstat[ROT_YAWRIGHT]),  &(mineStaticInfos[0]) },
    { "rotstrength[ROT_PITCHUP]",       scriptSetReal32CB,   &(mineStaticInfos[0].guidance.rotstrengthstat[ROT_PITCHUP]),  &(mineStaticInfos[0]) },
    { "rotstrength[ROT_PITCHDOWN]",     scriptSetReal32CB,   &(mineStaticInfos[0].guidance.rotstrengthstat[ROT_PITCHDOWN]),  &(mineStaticInfos[0]) },
    { "rotstrength[ROT_ROLLRIGHT]",     scriptSetReal32CB,   &(mineStaticInfos[0].guidance.rotstrengthstat[ROT_ROLLRIGHT]),  &(mineStaticInfos[0]) },
    { "rotstrength[ROT_ROLLLEFT]",      scriptSetReal32CB,   &(mineStaticInfos[0].guidance.rotstrengthstat[ROT_ROLLLEFT]),  &(mineStaticInfos[0]) },
    { "turnspeed[TURN_YAW]",            scriptSetReal32CB,   &(mineStaticInfos[0].guidance.turnspeedstat[TURN_YAW]),  &(mineStaticInfos[0]) },
    { "turnspeed[TURN_PITCH]",          scriptSetReal32CB,   &(mineStaticInfos[0].guidance.turnspeedstat[TURN_PITCH]),  &(mineStaticInfos[0]) },
    { "turnspeed[TURN_ROLL]",           scriptSetReal32CB,   &(mineStaticInfos[0].guidance.turnspeedstat[TURN_ROLL]),  &(mineStaticInfos[0]) },
    { "MINE_STOP_FRICTION",           scriptSetReal32CB,   &(mineStaticInfos[0].MINE_STOP_FRICTION),  &(mineStaticInfos[0]) },
    { "MINE_RANGESQR",           scriptSetReal32SqrCB,   &(mineStaticInfos[0].MINE_RANGESQR),  &(mineStaticInfos[0]) },
    { "MINE_RANGESQR_FORCED",           scriptSetReal32SqrCB,   &(mineStaticInfos[0].MINE_RANGESQR_FORCED),  &(mineStaticInfos[0]) },
    { "preciseSelection",scriptSetUdwordCB, &(ShipStaticInfos[0].common.staticCollInfo.preciseSelection),  &(ShipStaticInfos[0]) },
    // CAT: new key (table 0x008b3b88 row 37; the exe takes the offset from missileStaticInfos, the same struct)
    { "N-LIPS",                         scriptSetReal32CB,      &(missileStaticInfos[0].scaleCap),  &(missileStaticInfos[0]) },

    END_SCRIPT_STRUCT_ENTRY
};

/*=============================================================================
    Private Functions:
=============================================================================*/

/*=============================================================================
    The following functions are callback functions which fill in
    information in the static structures.  They are called by scriptSetStruct
=============================================================================*/
//bind a hierarchy element to a gun and set the hierarchy size
static void setGunBindInfo(char *directory,char *field,void *dataToFillIn)
{
    ShipStaticInfo *info = (ShipStaticInfo *)dataToFillIn;
    char *hString, *string, *gString;
    sdword gunIndex, nScanned, index, nBound;
    mhbinding *binding;
    lod *level;
    udword flags = 0;
    sdword frequency;

    //first string is always name of hierarchy member.
    hString = strtok(field, " ,");
    //second string is the gun mex name
    gString = strtok(NULL, " ,");
    nScanned = sscanf(gString, "Gun%d", &gunIndex);         //get gun #
    dbgAssertOrIgnore(nScanned == 1);                               //verify it
    dbgAssertOrIgnore(gunIndex >= 0 && gunIndex < info->gunStaticInfo->numGuns);

    //parse the modifiers
    while ((string = strtok(NULL, " ,")) != NULL)
    {
        if (strcasecmp(string, "Azimuth") == 0)
        {
            flags |= HBF_Azimuth;
        }
        else if (strcasecmp(string, "Declination") == 0)
        {
            flags |= HBF_Declination;
        }
        else if (strcasecmp(string, "Recoil") == 0)
        {
            flags |= HBF_Recoil;
        }
        else if (strcasecmp(string, "Frequency") == 0)
        {
            string = strtok(NULL, " ,");
            dbgAssertOrIgnore(string);
            nScanned = sscanf(string, "%d", &frequency);
            dbgAssertOrIgnore(nScanned == 1);
            dbgAssertOrIgnore(frequency >= 1 && frequency < HB_UpdateFrequencyMax);
            flags |= frequency;
        }
    }
    info->hierarchySize = lodHierarchySizeCompute(info->common.LOD);
    nBound = 0;
    for (index = 0; index < info->common.LOD->nLevels; index++)
    {                                                       //for all levels of detail
        level = &info->common.LOD->level[index];
        binding = NULL;
        if ((level->flags & LM_LODType) != LT_Mesh)
        {
            continue;
        }
        while ((binding = meshBindingFindByName(binding, level->hBindings, (meshdata *)level->pData, hString)) != NULL)
        {
            binding->function = gunMatrixUpdate;            //bind all named members to this gun
            binding->userData = &info->gunStaticInfo->gunstatics[gunIndex];
            binding->userID = gunIndex;
            binding->flags = flags;
            nBound++;
        }
    }
#if MESH_VERBOSE_LEVEL >= 1
    if (nBound == 0)
    {
        dbgWarningf(DBG_Loc, "Could not bind '%s' to '%s' because object not found.", hString, gString);
    }
#endif
#if MESH_VERBOSE_LEVEL >= 2
    dbgMessagef("setGunBindInfo: bound '%s' to '%s' %d times", hString, gString, nBound++);
#endif
}
static void setAnimationBindInfo(char *directory,char *field,void *dataToFillIn)
{
    ShipStaticInfo *info = (ShipStaticInfo *)dataToFillIn;
    char *shipName;
    char fileName[256];

    shipName = ShipTypeToStr(info->shiptype);
    sprintf(fileName, "%s%s%s", directory, shipName, MAD_Extension);//make path to file
    info->madStatic = memAlloc(sizeof(madstatic), "madBindings", NonVolatile);
    memset(info->madStatic,0,sizeof(madstatic));
    info->madStatic->header = madFileLoad(fileName);
    info->hierarchySize = lodHierarchySizeCompute(info->common.LOD);
}

static void setMadStartInfo(char *directory,char *field,void *dataToFillIn)
{
    ShipStaticInfo *info = (ShipStaticInfo *)dataToFillIn;
    char madAnimName[256];
    udword startFlag;
    char madAnimName2[256];
    udword startFlag2;
    sscanf(field,"%d , %s , %d , %s",&startFlag,madAnimName,&startFlag2,madAnimName2);

    if(startFlag == 1)
    {
        info->madStatic->needStartPlacedAnimation = TRUE;
        info->madStatic->startPlacedAnimationIndex = madAnimIndexFindByName(info->madStatic->header, madAnimName);
    }
    else
    {
        //no need to start and then pause an animation on creation
        info->madStatic->needStartPlacedAnimation = FALSE;
    }

    if(startFlag2 == 1)
    {
        info->madStatic->needStartBuiltAnimation = TRUE;
        info->madStatic->startBuiltAnimationIndex = madAnimIndexFindByName(info->madStatic->header, madAnimName2);
    }
    else
    {
        //no need to start and then pause an animation on creation
        info->madStatic->needStartBuiltAnimation = FALSE;
    }


}
 /*
    sdword *gunOpenIndexs;                      //arrays for gunopening/closing animation indexes both damaged and undamaged.  So we can support a number of different animationsfor the same ship
    sdword *gunOpenDamagedIndexes;
    sdword *gunCloseIndexes;
    sdword *gunCloseDamagedIndexes;
 */
static void setGunAnimInfo(char *directory,char *field,void *dataToFillIn)
{
    ShipStaticInfo *info = (ShipStaticInfo *)dataToFillIn;
    sdword i;
    char madAnimName[256];   //maximum of 4 animations so far
    udword numAnims;
    char *token;
    char seps[] = ", ";

    token = strtok(field,seps);

    //do normal gun opening animations
    sscanf(token,"%d",&numAnims);
    info->madStatic->numGunOpenIndexes = numAnims;
    if(numAnims > 0)
    {
        info->madStatic->gunOpenIndexes = (sdword *) memAlloc(sizeof(sdword)*numAnims,"GunOpenMadAnimIndex",NonVolatile);
        for(i=0;i<numAnims;i++)
        {
            token = strtok(NULL,seps);
            sscanf(token,"%s",madAnimName);
            info->madStatic->gunOpenIndexes[i] = madAnimIndexFindByName(info->madStatic->header, madAnimName);
        }
    }

    token = strtok(NULL,seps);
    //do normal gun closing animations
    sscanf(token,"%d",&numAnims);
    info->madStatic->numGunCloseIndexes = numAnims;
    if(numAnims > 0)
    {
        info->madStatic->gunCloseIndexes = (sdword *) memAlloc(sizeof(sdword)*numAnims,"GunOpenMadAnimIndex",NonVolatile);
        for(i=0;i<numAnims;i++)
        {
            token = strtok(NULL,seps);
            sscanf(token,"%s",madAnimName);
            info->madStatic->gunCloseIndexes[i] = madAnimIndexFindByName(info->madStatic->header, madAnimName);
        }
    }


    token = strtok(NULL,seps);
    //do damaged gun opening animations
    sscanf(token,"%d",&numAnims);
    info->madStatic->numGunOpenDamagedIndexes = numAnims;
    if(numAnims > 0)
    {
        info->madStatic->gunOpenDamagedIndexes = (sdword *) memAlloc(sizeof(sdword)*numAnims,"GunOpenMadAnimIndex",NonVolatile);
        for(i=0;i<numAnims;i++)
        {
            token = strtok(NULL,seps);
            sscanf(token,"%s",madAnimName);
            info->madStatic->gunOpenDamagedIndexes[i] = madAnimIndexFindByName(info->madStatic->header, madAnimName);
        }
    }


    token = strtok(NULL,seps);
    //do damaged gun closing animations
    sscanf(token,"%d",&numAnims);
    info->madStatic->numGunCloseDamagedIndexes = numAnims;
    if(numAnims > 0)
    {
        info->madStatic->gunCloseDamagedIndexes = (sdword *) memAlloc(sizeof(sdword)*numAnims,"GunOpenMadAnimIndex",NonVolatile);
        for(i=0;i<numAnims;i++)
        {
            token = strtok(NULL,seps);
            sscanf(token,"%s",madAnimName);
            info->madStatic->gunCloseDamagedIndexes[i] = madAnimIndexFindByName(info->madStatic->header, madAnimName);
        }
    }
}

static void setDockAnimInfo(char *directory,char *field,void *dataToFillIn)
{
    ShipStaticInfo *info = (ShipStaticInfo *)dataToFillIn;
    sdword i;
    char madAnimName[256];   //maximum of 4 animations so far
    udword numAnims;
    char *token;
    char seps[] = ", ";

    token = strtok(field,seps);

    //do normal gun opening animations
    sscanf(token,"%d",&numAnims);
    info->madStatic->numDockIndexes = numAnims;
    if(numAnims > 0)
    {
        info->madStatic->PostDockIndexes = (sdword *) memAlloc(sizeof(sdword)*numAnims,"GunOpenMadAnimIndex",NonVolatile);
        for(i=0;i<numAnims;i++)
        {
            token = strtok(NULL,seps);
            sscanf(token,"%s",madAnimName);
            info->madStatic->PostDockIndexes[i] = madAnimIndexFindByName(info->madStatic->header, madAnimName);
        }
    }

    token = strtok(NULL,seps);
    //do normal gun closing animations
    sscanf(token,"%d",&numAnims);
    info->madStatic->numPostDockIndexes = numAnims;
    if(numAnims > 0)
    {
        info->madStatic->DockIndexes = (sdword *) memAlloc(sizeof(sdword)*numAnims,"GunOpenMadAnimIndex",NonVolatile);
        for(i=0;i<numAnims;i++)
        {
            token = strtok(NULL,seps);
            sscanf(token,"%s",madAnimName);
           info->madStatic->DockIndexes[i] = madAnimIndexFindByName(info->madStatic->header, madAnimName);
        }
    }


       token = strtok(NULL,seps);
    //do damaged gun closing animations
    sscanf(token,"%d",&numAnims);
    info->madStatic->numPostDockDamagedIndexes = numAnims;
    if(numAnims > 0)
    {
        info->madStatic->PostDockDamagedIndexes = (sdword *) memAlloc(sizeof(sdword)*numAnims,"GunOpenMadAnimIndex",NonVolatile);
        for(i=0;i<numAnims;i++)
        {
            token = strtok(NULL,seps);
            sscanf(token,"%s",madAnimName);
            info->madStatic->PostDockDamagedIndexes[i] = madAnimIndexFindByName(info->madStatic->header, madAnimName);
        }
    }

    token = strtok(NULL,seps);
    //do damaged gun opening animations
    sscanf(token,"%d",&numAnims);
    info->madStatic->numDockDamagedIndexes = numAnims;
    if(numAnims > 0)
    {
        info->madStatic->DockDamagedIndexes = (sdword *) memAlloc(sizeof(sdword)*numAnims,"GunOpenMadAnimIndex",NonVolatile);
        for(i=0;i<numAnims;i++)
        {
            token = strtok(NULL,seps);
            sscanf(token,"%s",madAnimName);
            info->madStatic->DockDamagedIndexes[i] = madAnimIndexFindByName(info->madStatic->header, madAnimName);
        }
    }

}
void setMothershipDoorAnimInfo(char *directory,char *field,void *dataToFillIn)
{
     ShipStaticInfo *info = (ShipStaticInfo *)dataToFillIn;
     sdword i;
     char madAnimName[256];   //maximum of 4 animations so far
     udword numAnims;
     char *token;
     char seps[] = ", ";

     token = strtok(field,seps);

     //do normal gun opening animations
     sscanf(token,"%d",&numAnims);
     info->madStatic->numDoorOpenIndexes = numAnims;
     if(numAnims > 0)
     {
         info->madStatic->DoorOpenIndexes = (sdword *) memAlloc(sizeof(sdword)*numAnims,"DoorOpenMadAnimIndex",NonVolatile);
         for(i=0;i<numAnims;i++)
         {
             token = strtok(NULL,seps);
             sscanf(token,"%s",madAnimName);
             info->madStatic->DoorOpenIndexes[i] = madAnimIndexFindByName(info->madStatic->header, madAnimName);
         }
     }

     token = strtok(NULL,seps);
     //do normal gun closing animations
     sscanf(token,"%d",&numAnims);
     info->madStatic->numDoorCloseIndexes = numAnims;
     if(numAnims > 0)
     {
         info->madStatic->DoorCloseIndexes = (sdword *) memAlloc(sizeof(sdword)*numAnims,"DoorCloseMadAnimIndex",NonVolatile);
         for(i=0;i<numAnims;i++)
         {
             token = strtok(NULL,seps);
             sscanf(token,"%s",madAnimName);
            info->madStatic->DoorCloseIndexes[i] = madAnimIndexFindByName(info->madStatic->header, madAnimName);
         }
     }
}
void setSpecialCaseAnimInfo(char *directory,char *field,void *dataToFillIn)
{
    ShipStaticInfo *info = (ShipStaticInfo *)dataToFillIn;
    sdword i;
    char madAnimName[256];   //maximum of 4 animations so far
    udword numAnims;
    char *token;
    char seps[] = ", ";

    token = strtok(field,seps);

    //do normal gun opening animations
    sscanf(token,"%d",&numAnims);
    info->madStatic->numSpecialOpenIndexes = numAnims;
    if(numAnims > 0)
    {
        info->madStatic->specialOpenIndexes = (sdword *) memAlloc(sizeof(sdword)*numAnims,"SpecialMadAnimIndex",NonVolatile);
        for(i=0;i<numAnims;i++)
        {
            token = strtok(NULL,seps);
            sscanf(token,"%s",madAnimName);
            info->madStatic->specialOpenIndexes[i] = madAnimIndexFindByName(info->madStatic->header, madAnimName);
        }
    }

    token = strtok(NULL,seps);
    //do normal gun closing animations
    sscanf(token,"%d",&numAnims);
    info->madStatic->numSpecialCloseIndexes = numAnims;
    if(numAnims > 0)
    {
        info->madStatic->specialCloseIndexes = (sdword *) memAlloc(sizeof(sdword)*numAnims,"SpecialMadAnimIndex",NonVolatile);
        for(i=0;i<numAnims;i++)
        {
            token = strtok(NULL,seps);
            sscanf(token,"%s",madAnimName);
            info->madStatic->specialCloseIndexes[i] = madAnimIndexFindByName(info->madStatic->header, madAnimName);
        }
    }


    token = strtok(NULL,seps);
    //do damaged gun opening animations
    sscanf(token,"%d",&numAnims);
    info->madStatic->numSpecialOpenDamagedIndexes = numAnims;
    if(numAnims > 0)
    {
        info->madStatic->specialOpenDamagedIndexes = (sdword *) memAlloc(sizeof(sdword)*numAnims,"SpecialMadAnimIndex",NonVolatile);
        for(i=0;i<numAnims;i++)
        {
            token = strtok(NULL,seps);
            sscanf(token,"%s",madAnimName);
            info->madStatic->specialOpenDamagedIndexes[i] = madAnimIndexFindByName(info->madStatic->header, madAnimName);
        }
    }


    token = strtok(NULL,seps);
    //do damaged gun closing animations
    sscanf(token,"%d",&numAnims);
    info->madStatic->numSpecialCloseDamagedIndexes = numAnims;
    if(numAnims > 0)
    {
        info->madStatic->specialCloseDamagedIndexes = (sdword *) memAlloc(sizeof(sdword)*numAnims,"SpecialMadAnimIndex",NonVolatile);
        for(i=0;i<numAnims;i++)
        {
            token = strtok(NULL,seps);
            sscanf(token,"%s",madAnimName);
            info->madStatic->specialCloseDamagedIndexes[i] = madAnimIndexFindByName(info->madStatic->header, madAnimName);
        }
    }

}
static void LODScriptLoad(char *directory,char *field,void *dataToFillIn)
{
    if (strcmp(field,"DefaultShip") == 0)
    {
        *((lodinfo **)dataToFillIn) = defaultlod;
    }
    else
    {
        *((lodinfo **)dataToFillIn) = lodTableReadScript(directory, field);
    }
}
static void loadMexDataCB(char *directory,char *field,void *dataToFillIn)
{
    char fullfilename[80];

    if (strcmp(field,"DefaultShip") == 0)
    {
        *((void **)dataToFillIn) = defaultmex;
    }
    else
    {
        if (directory != NULL)
        {
            strcpy(fullfilename,directory);
            strcat(fullfilename,field);
        }
        else
        {
            strcpy(fullfilename,field);
        }

        if (fileExists(fullfilename,0))
        {
            *((void **)dataToFillIn) = mexLoad(fullfilename);
        }
        else
        {
            dbgMessagef("WARNING: Could not locate mex %s",fullfilename);
            *((void **)dataToFillIn) = defaultmex;
        }
    }
}

static void setClassCB(char *directory,char *field,void *dataToFillIn)
{
    *((ShipClass *)dataToFillIn) = StrToShipClass(field);
}

// CAT: key NUMBER_OF_ANIMS n (cat_madNumberOfAnimsSetCB 0x00539e40): loads the ship's .mad like BindAnimations, then
//      allocates an n-entry animation table, all -1 until ANIM lines fill it; n <= 0 does nothing
static void cat_madNumberOfAnimsSetCB(char *directory,char *field,void *dataToFillIn)
{
    ShipStaticInfo *info = (ShipStaticInfo *)dataToFillIn;
    sdword numAnims = 0;
    char fileName[256];
    sdword i;

    sscanf(field,"%i",&numAnims);
    if (numAnims > 0)
    {
        sprintf(fileName, "%s%s%s", directory, ShipTypeToStr(info->shiptype), MAD_Extension);
        info->madStatic = memAlloc(sizeof(madstatic), "madBindings", NonVolatile);
        memset(info->madStatic,0,sizeof(madstatic));
        info->madStatic->header = madFileLoad(fileName);
        info->hierarchySize = lodHierarchySizeCompute(info->common.LOD);
        info->madStatic->numAnims = numAnims;
        info->madStatic->animIndexes = memAlloc(numAnims * sizeof(sdword), "madAnimIndexes", NonVolatile);
        memset(info->madStatic->animIndexes, 0, numAnims * sizeof(sdword));
        for (i = 0; i < info->madStatic->numAnims; i++)
        {
            info->madStatic->animIndexes[i] = -1;
        }
        info->madStatic->animsByNumber = 1;
    }
}

// CAT: key ANIM "n name" (setMadAnim 0x00539f70): animation n is the .mad animation of that name (-1 if none; the exe
//      then warns into a stripped debug print). Like the exe, it trusts NUMBER_OF_ANIMS to have come first and n to
//      be in range.
static void setMadAnim(char *directory,char *field,void *dataToFillIn)
{
    ShipStaticInfo *info = (ShipStaticInfo *)dataToFillIn;
    sdword index = 0;
    char madAnimName[256];

    madAnimName[0] = 0;
    sscanf(field,"%i %s",&index,madAnimName);
    info->madStatic->animIndexes[index] = madAnimIndexFindByName(info->madStatic->header, madAnimName);
    if (info->madStatic->animIndexes[index] == -1)
    {
        dbgMessagef("setMadAnim(): Animation %s not found in %s", madAnimName, ShipTypeToStr(info->shiptype));
    }
}

// CAT: key beastAttackConvertTo, a ship type name (cat_scriptSetBeastAttackConvertToCB 0x0053ac50)
static void cat_scriptSetBeastAttackConvertToCB(char *directory,char *field,void *dataToFillIn)
{
    *((ShipType *)dataToFillIn) = StrToShipType(field);
}

static void scriptSetFramesCB(char *directory,char *field,void *dataToFillIn)
{
    sscanf(field,"%d",(sdword *)dataToFillIn);
    *((sdword *)dataToFillIn) *= UNIVERSE_UPDATE_RATE;
}

void gunGetDefaultOrientationFromBindings(GunStatic *gunstatic,ShipStaticInfo *statinfo)
{
    lod *level = &statinfo->common.LOD->level[0];
    //udword numHierarchys = ((meshdata *)level->pData)->nPolygonObjects;
    hmatrix concatenated, nonConcatenated;
//    mhbinding *binding;

    if (meshFindHierarchyMatrixByUserData(&concatenated, &nonConcatenated, level->hBindings, gunstatic) != FALSE)
    {
        matGetMatFromHMat(&gunstatic->defaultgunorientation,&concatenated);
        matGetMatFromHMat(&gunstatic->defaultGunOrientationNonConcat,&nonConcatenated);
        if (memcmp(&gunstatic->defaultGunOrientationNonConcat,
                   &gunstatic->defaultgunorientation, sizeof(matrix)))
        {                                                   //if these guys are different
            bitSet(gunstatic->flags, GF_MultiLevelMatrix);
        }
    }
    else
    {
        dbgMessagef("WARNING: could not set gunbindings for ship %d",statinfo->shiptype);
    }

/*
    binding = level->hBindings;

    // later, take into account multiple hierarchies for a single gun
    while (numHierarchys > 0)
    {
        if (binding->userData == gunstatic)
        {
            matGetMatFromHMat(&gunstatic->defaultgunorientation,&binding->object->localMatrix);
            return;
        }
        binding++;
        numHierarchys--;
    }
*/
}

void gunSetStaticGunInfoFromBindings(ShipStaticInfo *statinfo)
{
    sdword i;
    GunStaticInfo *gunStaticInfo = statinfo->gunStaticInfo;
    sdword numGuns = gunStaticInfo->numGuns;
    GunStatic *gunstatic;

    for (i=0;i<numGuns;i++)
    {
        gunstatic = &gunStaticInfo->gunstatics[i];

        switch (gunstatic->guntype)
        {
            case GUN_NewGimble:
                gunGetDefaultOrientationFromBindings(gunstatic,statinfo);
                break;

            default:
                // ignore
                break;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : precalcStaticHeader
    Description : precalculates certain useful information in the static header
                  so these calculations don't have to be done every frame
    Inputs      : *staticheader
    Outputs     : *staticheader updated appropriately
    Return      :
----------------------------------------------------------------------------*/
void precalcStaticHeader(StaticInfo *staticheader)
{
    real32 avoidcollsize;

    mexSetCollisionInfo(&staticheader->staticCollInfo,staticheader->pMexData,
                        staticheader->staticCollInfo.collBoxScaleForward,
                        staticheader->staticCollInfo.collBoxScaleUp,
                        staticheader->staticCollInfo.collBoxScaleRight,
                        staticheader->staticCollInfo.collBoxOffsetX,
                        staticheader->staticCollInfo.collBoxOffsetY,
                        staticheader->staticCollInfo.collBoxOffsetZ);

    if (staticheader->staticCollInfo.approxcollmodifier != 0.0f)
    {
        staticheader->staticCollInfo.approxcollspheresize *= staticheader->staticCollInfo.approxcollmodifier;
    }

    if (staticheader->staticCollInfo.avoidcollmodifier != 0.0f)
    {
        staticheader->staticCollInfo.avoidcollspheresize *= staticheader->staticCollInfo.avoidcollmodifier;
    }

    avoidcollsize = staticheader->staticCollInfo.avoidcollspheresize;
    staticheader->staticCollInfo.avoidcollspherepad = avoidcollsize * ((avoidcollsize > AISHIP_BIGSHIP) ? AVOID_OBJ_PADDING_SCALE_BIG : AVOID_OBJ_PADDING_SCALE);

    staticheader->oneOverMass =             1.0f /      staticheader->mass;
    staticheader->oneOverMomentOfInertiaX = 1.0f /      staticheader->momentOfInertiaX;
    staticheader->oneOverMomentOfInertiaY = 1.0f /      staticheader->momentOfInertiaY;
    staticheader->oneOverMomentOfInertiaZ = 1.0f /      staticheader->momentOfInertiaZ;
}

void precalcStaticInfoHealth(StaticInfoHealth *staticinfo)
{
    if ((staticinfo->minCollDamage == 0.0f) && (staticinfo->maxCollDamage == 0.0f))
    {
        // put in defaults:
        staticinfo->minCollDamage = ((staticinfo->maxhealth) * COLLDAMAGE_DEFAULTMIN_FRACTION);
        staticinfo->maxCollDamage = ((staticinfo->maxhealth) * COLLDAMAGE_DEFAULTMAX_FRACTION);
    }
    if(staticinfo->maxhealth != 0.0f)
    {
        //precalc 1/maxhealth..duh
        staticinfo->oneOverMaxHealth = 1.0f/staticinfo->maxhealth;
    }
}

// CAT: the exe has three orders of the thrust product (mass, thrust and GLOBAL_SHIP_SPEED_MODIFIER), one per site;
//      HW1 multiplied thrust by (mass * modifier) everywhere. The rest of the guidance precalc is the same at every site.
#define CAT_THRUST_MOD_THRUST_MASS  0   // precalcStaticInfoGuidance 0x0053ad70 (universeStaticInit's derelicts), InitStatDerelictInfoByPath 0x0053b2a0
#define CAT_THRUST_MASS_THRUST_MOD  1   // InitStatShipInfo 0x0053bab0 inline (0x0053c05d: fld mass; fmul thrust; fmul modifier)
#define CAT_THRUST_THRUST_MASS_MOD  2   // InitStatMissileInfo 0x0053b400, InitStatMineInfo 0x0053b660 inline

static void catPrecalcStaticInfoGuidanceOrder(StaticInfoHealthGuidance *staticinfo, sdword order)
{
    sdword i;

    for (i=0;i<NUM_TRANS_DEGOFFREEDOM;i++)
    {
        switch (order)
        {
            case CAT_THRUST_MASS_THRUST_MOD:
                staticinfo->thruststrengthstat[i] = staticinfo->common.mass * staticinfo->thruststrengthstat[i] * GLOBAL_SHIP_SPEED_MODIFIER;
                break;
            case CAT_THRUST_THRUST_MASS_MOD:
                staticinfo->thruststrengthstat[i] = staticinfo->thruststrengthstat[i] * staticinfo->common.mass * GLOBAL_SHIP_SPEED_MODIFIER;
                break;
            default:
                staticinfo->thruststrengthstat[i] = GLOBAL_SHIP_SPEED_MODIFIER * staticinfo->thruststrengthstat[i] * staticinfo->common.mass;
                break;
        }
    }

    staticinfo->common.maxvelocity *= GLOBAL_SHIP_SPEED_MODIFIER;

    staticinfo->rotstrengthstat[ROT_ABOUTXCCW] *= staticinfo->common.momentOfInertiaX;
    staticinfo->rotstrengthstat[ROT_ABOUTXCW]  *= staticinfo->common.momentOfInertiaX;
    staticinfo->rotstrengthstat[ROT_ABOUTYCCW] *= staticinfo->common.momentOfInertiaY;
    staticinfo->rotstrengthstat[ROT_ABOUTYCW]  *= staticinfo->common.momentOfInertiaY;
    staticinfo->rotstrengthstat[ROT_ABOUTZCCW] *= staticinfo->common.momentOfInertiaZ;
    staticinfo->rotstrengthstat[ROT_ABOUTZCW]  *= staticinfo->common.momentOfInertiaZ;
}

void precalcStaticInfoGuidance(StaticInfoHealthGuidance *staticinfo)
{
    catPrecalcStaticInfoGuidanceOrder(staticinfo, CAT_THRUST_MOD_THRUST_MASS);   // CAT: 0x0053ad70's order
}

/*-----------------------------------------------------------------------------
    Name        : InitStatAsteroidInfo
    Description : initializes static asteroid information
    Inputs      : asteroidType
    Outputs     : asteroidStatInfo is filled in
    Return      :
----------------------------------------------------------------------------*/
void InitStatAsteroidInfo(AsteroidStaticInfo *asteroidStatInfo,AsteroidType asteroidtype)
{
    char shipname[80];
    char directory[80];
    char fullshipname[160];
    char *asteroidtypestr;

    asteroidStatInfo->asteroidtype = asteroidtype;

    asteroidtypestr = AsteroidTypeToStr(asteroidtype);

#ifdef _WIN32
    strcpy(directory,"Resources\\Asteroids\\");
#else
    strcpy(directory,"Resources/Asteroids/");
#endif
//    strcat(directory,asteroidtypestr);
//    strcat(directory,"\\");

    strcpy(shipname,asteroidtypestr);
    strcat(shipname,".shp");

    strcpy(fullshipname,directory);
    strcat(fullshipname,shipname);

    if (fileExists(fullshipname,0))
    {
        scriptSetStruct(directory,shipname,AsteroidStaticScriptTable,(ubyte *)asteroidStatInfo);
        cat_scriptAsteroidBreakInfoLoad(directory,shipname,asteroidStatInfo);   // CAT: new (0x00528190)

        precalcStaticHeader(&asteroidStatInfo->common);
        precalcStaticInfoHealth((StaticInfoHealth *)asteroidStatInfo);

        // optimization for Asteroid0
        if (asteroidtype == Asteroid0) collZeroRectInfo(&asteroidStatInfo->common.staticCollInfo);
    }
}

/*-----------------------------------------------------------------------------
    Name        : InitStatDustCloudInfo
    Description : initializes static dustcloud information
    Inputs      : dustcloudType
    Outputs     : dustcloudStatInfo is filled in
    Return      :
----------------------------------------------------------------------------*/
void InitStatDustCloudInfo(DustCloudStaticInfo *dustcloudStatInfo, DustCloudType dustcloudtype)
{
    char shipname[80];
    char directory[80];
    char fullshipname[160];
    char *dustcloudtypestr;

    dbgAssertOrIgnore(dustcloudtype < NUM_DUSTCLOUDTYPES);

    dustcloudStatInfo->dustcloudtype = dustcloudtype;

    dustcloudtypestr = DustCloudTypeToStr(dustcloudtype);

#ifdef _WIN32
    strcpy(directory,"Resources\\DustClouds\\");
#else
    strcpy(directory,"Resources/DustClouds/");
#endif
//    strcat(directory,dustcloudtypestr);
//    strcat(directory,"\\");

    strcpy(shipname,dustcloudtypestr);
    strcat(shipname,".shp");

    strcpy(fullshipname,directory);
    strcat(fullshipname,shipname);

    if (fileExists(fullshipname,0))
    {
        StaticCollInfo *coll;
        real32 r = DUSTCLOUD_RADII[(sdword)dustcloudtype] * DUSTCLOUD_RADIAL_SCALE;

        scriptSetStruct(directory,shipname,DustCloudStaticScriptTable,(ubyte *)dustcloudStatInfo);

        precalcStaticHeader(&dustcloudStatInfo->common);
        precalcStaticInfoHealth((StaticInfoHealth *)dustcloudStatInfo);

        coll = &dustcloudStatInfo->common.staticCollInfo;
        collZeroRectInfo(coll);

        coll->originalCollSphereSize = r;
        coll->approxcollspheresize = r;
        coll->avoidcollspheresize = r;
        coll->avoidcollspherepad = r;
        coll->collspheresize = r;
        coll->collspheresizeSqr = r * r;
    }
}

/*-----------------------------------------------------------------------------
    Name        : InitStatGasCloudInfo
    Description : initializes static gascloud information
    Inputs      : gascloudType
    Outputs     : gascloudStatInfo is filled in
    Return      :
----------------------------------------------------------------------------*/
void InitStatGasCloudInfo(GasCloudStaticInfo *gascloudStatInfo, GasCloudType gascloudtype)
{
    char shipname[80];
    char directory[80];
    char fullshipname[160];
    char *gascloudtypestr;

    gascloudStatInfo->gascloudtype = gascloudtype;

    gascloudtypestr = GasCloudTypeToStr(gascloudtype);

#ifdef _WIN32
    strcpy(directory,"Resources\\GasClouds\\");
#else
    strcpy(directory,"Resources/GasClouds/");
#endif
//    strcat(directory,gascloudtypestr);
//    strcat(directory,"\\");

    strcpy(shipname,gascloudtypestr);
    strcat(shipname,".shp");

    strcpy(fullshipname,directory);
    strcat(fullshipname,shipname);

    if (fileExists(fullshipname,0))
    {
        scriptSetStruct(directory,shipname,GasCloudStaticScriptTable,(ubyte *)gascloudStatInfo);

        precalcStaticHeader(&gascloudStatInfo->common);
        precalcStaticInfoHealth((StaticInfoHealth *)gascloudStatInfo);
    }
}

/*-----------------------------------------------------------------------------
    Name        : InitStatNebulaInfo
    Description : initializes static nebula information
    Inputs      : nebulaType
    Outputs     : nebulaStatInfo is filled in
    Return      :
----------------------------------------------------------------------------*/
void InitStatNebulaInfo(NebulaStaticInfo* nebulaStatInfo, NebulaType nebulatype)
{
    char shipname[80];
    char directory[80];
    char fullshipname[160];
    char* nebulatypestr;

    dbgAssertOrIgnore(nebulatype < NUM_NEBULATYPES);

    nebulaStatInfo->nebulatype = nebulatype;

    nebulatypestr = NebulaTypeToStr(nebulatype);

#ifdef _WIN32
    strcpy(directory,"Resources\\Nebulae\\");
#else
    strcpy(directory,"Resources/Nebulae/");
#endif
//    strcat(directory,nebulatypestr);
//    strcat(directory,"\\");

    strcpy(shipname,nebulatypestr);
    strcat(shipname,".shp");

    strcpy(fullshipname,directory);
    strcat(fullshipname,shipname);

    if (fileExists(fullshipname,0))
    {
        StaticCollInfo *coll;

        scriptSetStruct(directory, shipname, NebulaStaticScriptTable, (ubyte*)nebulaStatInfo);

        precalcStaticHeader(&nebulaStatInfo->common);
        precalcStaticInfoHealth((StaticInfoHealth*)nebulaStatInfo);

        coll = &nebulaStatInfo->common.staticCollInfo;
        collZeroRectInfo(coll);

        coll->originalCollSphereSize = NEB_RADIUS;
        coll->approxcollspheresize = NEB_RADIUS;
        coll->avoidcollspheresize = NEB_RADIUS;
        coll->avoidcollspherepad = NEB_RADIUS;
        coll->collspheresize = NEB_RADIUS;
        coll->collspheresizeSqr = NEB_RADIUS * NEB_RADIUS;
    }
}

void InitStatHyperspaceGateInfo(DerelictStaticInfo *derelictStatInfo)
{
    StaticCollInfo *sci;
    real32 r;

    memset(derelictStatInfo, 0, sizeof(DerelictStaticInfo));

    derelictStatInfo->health.maxhealth = HYPERSPACEGATE_HEALTH;
    derelictStatInfo->health.oneOverMaxHealth = (1.0f / derelictStatInfo->health.maxhealth);

    derelictStatInfo->renderlistLimitSqr = 5.0e8f;

    sci = &derelictStatInfo->common.staticCollInfo;

    sci->uplength = HYPERSPACEGATE_HEIGHT;
    sci->rightlength = HYPERSPACEGATE_WIDTH;
    sci->forwardlength = 50.0f;     // give it a small thickness for collision's sake

    vecSet(sci->collrectoffset,-HYPERSPACEGATE_HEIGHT/2,-HYPERSPACEGATE_WIDTH/2,-50.0/2);

    r = max(HYPERSPACEGATE_HEIGHT,HYPERSPACEGATE_WIDTH);

    sci->approxcollspheresize = r;
    sci->avoidcollspheresize = r;
    sci->avoidcollspherepad = r;
    sci->avoidcollmodifier = 1.0f;
    sci->approxcollmodifier = 1.0f;

    sci->originalCollSphereSize = r;
    sci->collspheresize = r;
    sci->collspheresizeSqr = r*r;

    vecSet(sci->collsphereoffset,0.0f,0.0f,0.0f);
}

/*-----------------------------------------------------------------------------
    Name        : InitStatDerelictInfo
    Description : initializes static derelict information
    Inputs      : derelicttype
    Outputs     : derelictStatInfo is filled in
    Return      :
----------------------------------------------------------------------------*/
void InitStatDerelictInfo(DerelictStaticInfo *derelictStatInfo, DerelictType derelicttype)
{
    char shipname[80];
    char directory[80];
    char fullshipname[160];
    char *derelicttypestr;

    if (derelicttype == HyperspaceGate)
    {
        InitStatHyperspaceGateInfo(derelictStatInfo);
        return;
    }

    derelicttypestr = DerelictTypeToStr(derelicttype);

#ifdef _WIN32
    strcpy(directory,"Derelicts\\");
#else
    strcpy(directory,"Derelicts/");
#endif
//    strcat(directory,derelicttypestr);
//    strcat(directory,"\\");

    strcpy(shipname,derelicttypestr);
    strcat(shipname,".shp");

    strcpy(fullshipname,directory);
    strcat(fullshipname,shipname);

    derelictStatInfo->minimumZoomDistance = 0.0f;
    derelictStatInfo->renderlistFade = 0.0f;
    derelictStatInfo->renderlistLimitSqr = 0.0f;

    if (fileExists(fullshipname,0))
    {
        InitStatDerelictInfoByPath(derelictStatInfo, derelicttype, directory,shipname);
    }
    scriptSetNAVLightStatics(directory, shipname, (ShipStaticInfo *)derelictStatInfo);
    if (derelictStatInfo->navlightStaticInfo != NULL)
    {
        mexGetNAVLightStaticInfo(derelictStatInfo->navlightStaticInfo,&derelictStatInfo->common);   // CAT: the header (0x004b7680)
    }
}

/*-----------------------------------------------------------------------------
    Name        : InitStatDerelictInfoByPath
    Description : Init the static info for a derelict based on a type AND a path.
                  Mostly used for creating generic objects for NIS's.
    Inputs      : derelictStatInfo - where to load the data to.
                  derelictType - type of derelict.  If only used in NIS, this
                    field is not used.
                  directory - what directory the .shp file is in.
                  shipFile - what .shp file to load in.
    Outputs     : reads in the representative .shp file into derelictStatInfo.
    Return      : void
----------------------------------------------------------------------------*/
void InitStatDerelictInfoByPath(DerelictStaticInfo *derelictStatInfo, DerelictType derelicttype, char *directory, char *shipFile)
{
    bool8 teamColor[MAX_MULTIPLAYER_PLAYERS];

    memcpy(teamColor, derelictStatInfo->shipOrDerelict.teamColor, sizeof(teamColor));//save the default team colors
    derelictStatInfo->derelicttype = derelicttype;
    memset(derelictStatInfo, 0, sizeof(DerelictStaticInfo));
    memcpy(derelictStatInfo->shipOrDerelict.teamColor, teamColor, sizeof(teamColor));
    derelictStatInfo->common.rightOfWay = 1;      // default to derelicts having a right of way same as frigates
    derelictStatInfo->salvageable = FALSE;              //default salvageability to FALSE!
    scriptSetStruct(directory,shipFile,DerelictStaticScriptTable,(ubyte *)derelictStatInfo);

    precalcStaticHeader(&derelictStatInfo->common);
    precalcStaticInfoHealth((StaticInfoHealth *)derelictStatInfo);
    precalcStaticInfoGuidance((StaticInfoHealthGuidance *)derelictStatInfo);
}

/*-----------------------------------------------------------------------------
    Name        : InitStatMissileInfo
    Description : initializes static missile information
    Inputs      : race
    Outputs     : missileStatInfo is filled in
    Return      :
----------------------------------------------------------------------------*/
void InitStatMissileInfo(MissileStaticInfo *missileStatInfo,ShipRace race)
{
    char directory[80];
    char fullshipname[100];
    char *shipname = "Missile.shp";

    strcpy(directory,ShipRaceToStr(race));
//    strcat(directory,"\\Missile\\");
#ifdef _WIN32
    strcat(directory, "\\");
#else
    strcat(directory, "/");
#endif

    strcpy(fullshipname,directory);
    strcat(fullshipname,shipname);

    if (!fileExists(fullshipname,0))
    {
        dbgMessagef("WARNING: Could not find Missile for %s",ShipRaceToStr(race));
        return;
    }

    missileStatInfo->guidance.turnspeedstat[TURN_YAW]   = 1.0f;
    missileStatInfo->guidance.turnspeedstat[TURN_PITCH] = 1.0f;
    missileStatInfo->guidance.turnspeedstat[TURN_ROLL]  = 1.0f;

    missileStatInfo->common.rightOfWay = -1;

    scriptSetStruct(directory,shipname,MissileStaticScriptTable,(ubyte *)missileStatInfo);

    precalcStaticHeader(&missileStatInfo->common);
    precalcStaticInfoHealth((StaticInfoHealth *)missileStatInfo);
    catPrecalcStaticInfoGuidanceOrder((StaticInfoHealthGuidance *)missileStatInfo, CAT_THRUST_THRUST_MASS_MOD);   // CAT: inline order

    //missile trail stuff
    missileStatInfo->trailWidth = -1.0f;
    missileStatInfo->trailHeight = -1.0f;
    missileStatInfo->trailStatic = trailStaticInfoParse(directory, shipname);
}

/*-----------------------------------------------------------------------------
    Name        : InitStatMineInfo
    Description : initializes static missile-mine information
    Inputs      : race
    Outputs     : missileStatInfo is filled in
    Return      :
----------------------------------------------------------------------------*/
void InitStatMineInfo(MissileStaticInfo *mineStatInfo,ShipRace race)
{
    char directory[80];
    char fullshipname[100];
    char *shipname = "Mine.shp";

    strcpy(directory,ShipRaceToStr(race));
//    strcat(directory,"\\Mine\\");
#ifdef _WIN32
    strcat(directory, "\\");
#else
    strcat(directory, "/");
#endif

    strcpy(fullshipname,directory);
    strcat(fullshipname,shipname);

    if (!fileExists(fullshipname,0))
    {
        dbgMessagef("WARNING: Could not find Mine for %s",ShipRaceToStr(race));
        return;
    }

    mineStatInfo->guidance.turnspeedstat[TURN_YAW]   = 1.0f;
    mineStatInfo->guidance.turnspeedstat[TURN_PITCH] = 1.0f;
    mineStatInfo->guidance.turnspeedstat[TURN_ROLL]  = 1.0f;

    mineStatInfo->common.rightOfWay = -1;

    scriptSetStruct(directory,shipname,MineStaticScriptTable,(ubyte *)mineStatInfo);

    mineStatInfo->mineRange = fsqrt(mineStatInfo->MINE_RANGESQR);
    mineStatInfo->mineRangeForced = fsqrt(mineStatInfo->MINE_RANGESQR_FORCED);

    precalcStaticHeader(&mineStatInfo->common);
    precalcStaticInfoHealth((StaticInfoHealth *)mineStatInfo);
    catPrecalcStaticInfoGuidanceOrder((StaticInfoHealthGuidance *)mineStatInfo, CAT_THRUST_THRUST_MASS_MOD);   // CAT: inline order
}

/*-----------------------------------------------------------------------------
    Name        : isStrikeCraft
    Description : returns TRUE if ship is a strikecraft
    Inputs      : shiptype
    Outputs     :
    Return      : returns TRUE if ship is a strikecraft
----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    Name        : cat_univInitShipTypeFlags
    Description : CAT: Cataclysm-only (cat_univInitShipTypeFlags 0x0053b8c0). Sets the per-type category bits
                  at ShipStaticInfo+0x1EC from the ship type; InitStatShipInfo calls it after the .shp is read.
                  The bits' meanings are not identified yet; their readers are stage-2 code.
    Inputs      : statinfo (shiptype already set)
    Outputs     : statinfo->catTypeFlags
    Return      :
----------------------------------------------------------------------------*/
void cat_univInitShipTypeFlags(ShipStaticInfo *statinfo)
{
    statinfo->catTypeFlags = 0;
    switch (statinfo->shiptype)
    {
        case sRecon: case sSentinel: case bRecon: case bSentinel: case kProximitySensor: case tProximitySensor:
        case trFloatingCity: case trResearchStation: case dAcolyte:
            statinfo->catTypeFlags |= 0x10000;
            break;
        case sAcolyte: case bAcolyte:
            statinfo->catTypeFlags |= 0x20;
            break;
        case sMimic:
            statinfo->catTypeFlags |= 0x20020;
            break;
        case sLeech:
            statinfo->catTypeFlags |= 0x20300;
            break;
        case sACV: case bACV:
            statinfo->catTypeFlags |= 0x40;
            break;
        case sMCV:
            statinfo->catTypeFlags |= 0x20040;
            break;
        case sMultiBeamFrigate: case bMultiBeamFrigate:
            statinfo->catTypeFlags |= 0x80;
            break;
        case sHiveFrigate: case bHiveFrigate: case dRKO:
            statinfo->catTypeFlags |= 0x8000;
            break;
        case sDreadnought: case sDestroyer: case tMissileDestroyer: case p1MissileCorvette:
            statinfo->catTypeFlags |= 0x400;
            break;
        case sCarrier: case bCarrier: case kCarrier: case tCarrier:
            statinfo->catTypeFlags |= 0x8012;
            break;
        case sWorker: case bWorker: case tResourceCollector:
            statinfo->catTypeFlags |= 0x1;
            break;
        case sMothership: case bMothership: case dBeastMothership:
            statinfo->catTypeFlags |= 0x800e;
            break;
        case sMothershipSpecial: case sMothershipEngineering: case sMothershipArmour: case sMothershipMicro:
        case sMothershipWeapons: case sMothershipDockingBay: case sMothershipSupport: case sMothershipCommand:
        case sMothershipEnginesLower: case sMothershipEnginesUpper: case sMothershipLowerSpine:
        case sMothershipBigGun: case sMothershipOreCanArmour: case bMothershipDockingBay:
        case bMothershipEnginesLower: case bSectMoship: case bMothershipSupport:
            statinfo->catTypeFlags |= 0x1000;
            break;
        case bCloakedFighter: case bIonArrayFrigate: case p1IonArrayFrigate: case p1StandardCorvette:
            statinfo->catTypeFlags |= 0x20020;
            break;
        case tMinelayerCorvette:
            statinfo->catTypeFlags |= 0x800;
            break;
        case dSlipgateSmall: case dSlipgateMedium: case dSlipgateLarge:
            statinfo->catTypeFlags |= 0xc000;
            break;
        default:
            break;
    }
}

bool32 isStrikeCraft(ShipType shiptype)
{
    // CAT: Cataclysm's 27 strike craft (isStrikeCraft 0x0053ba20; InitStatShipInfo 0x0053bab0 inlines the same list
    //      for the formation padding default)
    switch (shiptype)
    {
        case sRecon:
        case sAcolyte:
        case sMimic:
        case sLeech:
        case sACV:
        case sMCV:
        case bRecon:
        case bAcolyte:
        case bInterceptor:
        case bCloakedFighter:
        case bAttackBomber:
        case bACV:
        case kAttackBomber:
        case kHeavyCorvette:
        case kHeavyDefender:
        case kHeavyInterceptor:
        case kMultiGunCorvette:
        case kSalCapCorvette:
        case tAttackBomber:
        case tHeavyDefender:
        case tHeavyInterceptor:
        case tMinelayerCorvette:
        case tMultiGunCorvette:
        case tSalCapCorvette:
        case p1Fighter:
        case p1MissileCorvette:
        case p1StandardCorvette:
            return TRUE;

        default:
            return FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : InitStatShipInfo
    Description : Initializes static ship information for all ship types.
    Inputs      : type, race
    Outputs     : statinfo is filled in
    Return      :
----------------------------------------------------------------------------*/
void InitStatShipInfo(ShipStaticInfo *statinfo,ShipType type,ShipRace race)
{
    char shipname[80];
    char directory[80];
    char fullshipname[160];
    char *shiptypestr;
    sdword i;
    bool32 stubbedOut;
    udword shipKeysSet;

    // CAT: defaults of the Cataclysm-only fields, set before the file check (InitStatShipInfo 0x0053bab0)
    statinfo->bUseRayTriangleIntersectionForHitLocation = TRUE;
    statinfo->forceFieldProtectionDistanceScale = -1.0f;
    statinfo->upgradeTimeModifier = 1.0f;
    statinfo->shiptype = type;
    statinfo->shiprace = race;
    statinfo->leechRUsPerSecond = 1;
    statinfo->leechHPsPerSecond = 20.0f;
    statinfo->nRepairDroids = 0;
    statinfo->fowRadius = 0.0f;
    statinfo->linkDistance = 0.0f;
    statinfo->externalBuild = FALSE;

    // CAT: the 0x8C-byte header has no shiptype field any more, so HW1's header/type assertion is gone
    statinfo->custshipheader = *(shipHeaders[type]);

    shiptypestr = ShipTypeToStr(type);

    strcpy(directory,ShipRaceToStr(race));
//    strcat(directory,"\\");
//    strcat(directory,shiptypestr);
#ifdef _WIN32
    strcat(directory,"\\");
#else
    strcat(directory,"/");
#endif

    strcpy(shipname,shiptypestr);
    strcat(shipname,".shp");

    strcpy(fullshipname,directory);
    strcat(fullshipname,shipname);

#ifdef HW_GAME_DEMO
    // The demo assets don't contain everything necessary for these ships, disable them
    if ( (strcmp(shipname, "CloakedFighter.shp") == 0) || (strcmp(shipname, "MinelayerCorvette.shp") == 0) ) {
        SetInfoNeededForShipAndRelatedStaticInfo(type,race,FALSE);
        rmEnableShip(race, type, FALSE);
        return;
    }
#endif

    if (!fileExists(fullshipname,0) || universeForceDefaultShip)
    {
        //we now disable this ship if we can't find it...
        // CAT: the exe disables the type silently (InitStatShipInfo 0x0053bab0); this port says so in the log
        dbgMessagef("shipstatic: %s not found%s, ship type %d (%s) disabled", fullshipname,
                    universeForceDefaultShip ? " (universeForceDefaultShip)" : "", type, shiptypestr);
        SetInfoNeededForShipAndRelatedStaticInfo(type,race,FALSE);
        rmEnableShip(race, type, FALSE);
        return;
/*

#ifdef HW_BUILD_FOR_DEBUGGING
        dbgMessagef("Stubbing out %s\\%s", ShipRaceToStr(race), ShipTypeToStr(type));
#endif
        strcpy(directory,"DefaultShip\\");
        strcpy(shipname,"DefaultShip.shp");

        strcpy(fullshipname,directory);
        strcat(fullshipname,shipname);
        stubbedOut = TRUE;
*/
    }
    else
    {
        stubbedOut = FALSE;
    }

    // CAT: no scaleFactor in Cataclysm's ShipStaticInfo (SO_CLOOGE_SCALE default removed)

    statinfo->guidance.turnspeedstat[TURN_YAW]   = 1.0f;
    statinfo->guidance.turnspeedstat[TURN_PITCH] = 1.0f;
    statinfo->guidance.turnspeedstat[TURN_ROLL]  = 1.0f;

    statinfo->minimumZoomDistance = 0.0f;

    statinfo->renderlistFade = 0.0f;
    statinfo->renderlistLimitSqr = 0.0f;

    statinfo->dockLightNear = 0.0f;
    statinfo->dockLightFar  = 0.1f;
    statinfo->dockLightColor = colRGB(120, 194, 255);   //default blue glow

    statinfo->hyperspaceColor = colRGB(0,0,0);  //default is to use the effect's default
    statinfo->hyperspaceBoxScale = 1.0f;        // CAT: default (InitStatShipInfo 0x0053bab0, +0x3D0)

    statinfo->tacticalSelectionScale = 1.0f;  // This is used to scale the R1 probes tactiacl overlay size, and the defenders selection box size

    if (isStrikeCraft(type))
        statinfo->formationPaddingModifier = STRIKECRAFT_PADDING_MODIFIER;          // default formation padding modifier
    else
        statinfo->formationPaddingModifier = 1.0f;          // default formation padding modifier

    //default to no explosions at all
//    statinfo->smallExplosion = statinfo->bigExplosion = statinfo->specialExplosion = NULL;
//    statinfo->bigExplosionFactor = 1.0f;

    //engine trail defaults
    for (i = 0; i < MAX_NUM_TRAILS; i++)
    {
        statinfo->trailStyle[i] = 1;
        statinfo->trailWidth[i] = -1.0f;
        statinfo->trailHeight[i] = -1.0f;
        statinfo->trailAngle[i] = -1.0f;
        statinfo->trailRibbonAdjust[i] = -1.0f;
        statinfo->trailLength[i] = -1.0f;
        statinfo->trailScaleCap[i] = -1.0f;
        statinfo->trailSpriteRadius[i] = -1.0f;
        statinfo->trailSpriteOffset[i] = 0.0f;
    }
    statinfo->common.staticCollInfo.preciseSelection = 0xff;//default is no precise selection

    // collSideModifiers reset
    for (i=0;i<NUM_TRANS_DEGOFFREEDOM;i++)
    {
        statinfo->collSideModifiers[i] = 0.0f;
    }

    statinfo->common.rightOfWay = rightOfWays[type];      // defaults for right of way

    // CAT: more defaults of Cataclysm fields, written just before the .shp is read (InitStatShipInfo 0x0053bab0)
    statinfo->beastAttackCascadeCost = 1.0f;
    statinfo->beastAttacksRequired = -1;
    statinfo->beastAttackConvertTo = bWorker;
    statinfo->UpgradeByDocking = FALSE;
    statinfo->pilotViewOffsetX = 0.0f;
    statinfo->pilotViewOffsetY = 0.0f;
    statinfo->pilotViewOffsetZ = 0.0f;
    statinfo->RKOGetsMissiles &= ~(udword)0xff;     // the exe clears the low byte only (byte store to +0x454)
    statinfo->dockShipRange = 0.0f;
    statinfo->canSpecialBandBoxFriendlies = FALSE;
    statinfo->unknownFlag212 = TRUE;
    statinfo->unknownFlag213 = FALSE;

    scriptSetStruct(directory,shipname,ShipStaticScriptTable,(ubyte *)statinfo);
    shipKeysSet = scriptStructEntriesSet;           // CAT: diagnostic count for the per-ship log line below

    // CAT: the exe warns (into a stripped debug print) when fowRadius is 0; nothing else happens

    if ((statinfo->repairTime) && (statinfo->repairCombatTime == 0.0f))
    {
        statinfo->repairCombatTime = statinfo->repairTime;
    }

    if ((statinfo->repairDamage) && (statinfo->repairCombatDamage == 0.0f))
    {
        // CAT: combat repair defaults to a fifth of the normal rate (HW1: the same rate); 0.2f at 0x0072a510
        statinfo->repairCombatDamage = statinfo->repairDamage * 0.2f;
    }

    cat_univInitShipTypeFlags(statinfo);            // CAT: new (cat_univInitShipTypeFlags 0x0053b8c0)

    if (statinfo->collSideModifiers[0] == 0.0f)
    {
        // collSideModifiers haven't been explicitly set, so set them:
        if (isCapitalShipStatic(statinfo))
        {
            for (i=0;i<NUM_TRANS_DEGOFFREEDOM;i++)
            {
                statinfo->collSideModifiers[i] = capitalSideModifiers[i];
            }
        }
        else
        {
            for (i=0;i<NUM_TRANS_DEGOFFREEDOM;i++)
            {
                statinfo->collSideModifiers[i] = 1.0f;
            }
        }
    }
/*
#ifdef HW_BUILD_FOR_DEBUGGING
    if ((cheapShips) && (!multiPlayerGame))
    {
        statinfo->buildTime = 1;
        statinfo->buildCost = 1;
    }
#endif
*/
    // CAT: single-player build costs of three Beast/Sect ships (InitStatShipInfo 0x0053bab0)
    if (singlePlayerGame)
    {
        switch (statinfo->shiptype)
        {
            case bHeavyCruiser:         statinfo->buildCost = 3000; break;
            case bIonArrayFrigate:      statinfo->buildCost = 600;  break;
            case sMothershipBigGun:     statinfo->buildCost = 5000; break;
            default: break;
        }
    }
    if (statinfo->blastRadiusShockWave == 0.0f)
    {
        statinfo->blastRadiusShockWave = BLAST_RADIUS_MULTIPLE_DEFAULT;
    }



    if (statinfo->passiveRetaliateZone == 0.0f)
    {
        statinfo->passiveRetaliateZone = RETALIATE_ZONE;
    }
    else if (statinfo->passiveRetaliateZone < 0.0f)
    {
        statinfo->passiveRetaliateZone = 0.0f;       // turn of retaliation zone feature if -ve
    }

    if (statinfo->attackfuelburnrate == 0.0f)
    {
        // CAT: the exe stores 0 here (no fuel keys in Cataclysm; HW1 copied fuelburnrate)
        statinfo->attackfuelburnrate = 0.0f;
    }

    statinfo->canReceiveSomething = (bool8)(statinfo->canReceiveResources | statinfo->canReceiveShips | statinfo->canReceiveShipsPermanently | statinfo->canReceiveShipsForRetire);

    for (i = 0; i < MAX_NUM_TRAILS; i++)
    {
        statinfo->trailStatic[i] = trailStaticInfoParse(directory, shipname);
    }
    if (mexGetChunk(statinfo->common.pMexData, "Eng", "Eng1") != NULL)
    {
        statinfo->multipleEngines = (udword)TRUE;
    }
    else
    {
        statinfo->multipleEngines = FALSE;
    }

    // CAT: no lowfuelpoint scaling (no fuel keys; the exe has no such step)

    precalcStaticHeader(&statinfo->common);
#if 1  // comment out when MiningBase properly moved hack,  fix later
    if (type == trMiningBase)   // CAT: type 100 (InitStatShipInfo 0x0053bab0)
    {
        StaticCollInfo *staticCollInfo = &statinfo->common.staticCollInfo;
        staticCollInfo->collsphereoffset.x += MINING_BASE_VERTICAL_DIST_HACK;
        staticCollInfo->collrectoffset.x += MINING_BASE_VERTICAL_DIST_HACK;
    }
#endif
    precalcStaticInfoHealth((StaticInfoHealth *)statinfo);
    catPrecalcStaticInfoGuidanceOrder((StaticInfoHealthGuidance *)statinfo, CAT_THRUST_MASS_THRUST_MOD);   // CAT: inline order

    // CAT: dockShipRange is kept as read (InitStatShipInfo 0x0053bab0 neither pads nor squares it; Cataclysm's dock
    //      code adds the collision radius at the point of use). HW1's Dock.c does the old conversion where it reads it.

    aishipPrecalcInfo(statinfo);

    for (i = 0; i < MAX_NUM_TRAILS; i++)
    {
        mexSetEngineNozzle(statinfo, statinfo->common.pMexData, i);     // CAT: statinfo (0x004b7c20)
    }

    if (stubbedOut == FALSE)
    {
        scriptSetDockStatics(directory,shipname,statinfo);
        scriptSetDockOverideStatics(directory,shipname,statinfo);
    }

    if (statinfo->dockStaticInfo != NULL)
    {
        mexGetDockStaticInfo(statinfo->dockStaticInfo,statinfo->common.pMexData);
        // CAT: the "center" dock point goes into the DockStaticInfo header (InitStatShipInfo 0x0053bab0,
        //      cat_dockFindDockStaticPoint 0x00463d00; NULL when the ship has none)
        statinfo->dockStaticInfo->centerDockPoint = cat_dockFindDockStaticPoint("center",statinfo->dockStaticInfo);
    }

    // CAT: InitStatShipInfo 0x0053bab0 has no dockInitializeCustomFunctions step (the exe has no such function): HW1's
    //      per-type dock set-up looks up HW1 dock-point names ("In", ...) that Cataclysm's ships do not have and
    //      stops with a fatal error (run ss-r2-statics-iso, sMothership). The two dock callbacks get HW1's default
    //      handlers as safe defaults so the unported dock code never calls through NULL; Cataclysm's docking
    //      (DPT_* points, the CustShipDockCommand slot) is stage 2.
    statinfo->ShipXDocksAtShipY = ShipDocksAtDefaultShip;
    statinfo->LaunchShipXFromShipY = LaunchShipFromDefaultShip;

    if (ShipCanHarvest(statinfo))
    {
        // find resource collection nozzle
        statinfo->resNozzleStatic = mexGetResNozzleStatic(statinfo->common.pMexData);
    }

    if(statinfo->repairBeamCapable)
    {
        //find repair beam nozzle
        statinfo->repairNozzleStatic = mexGetRepairNozzleStatic(statinfo->common.pMexData);
    }

    // CAT: no tractor beam emitter static in Cataclysm (no field, no mexGetTractorBeamStatic in the exe)

    scriptSetNAVLightStatics(directory, shipname, statinfo);
    if (statinfo->navlightStaticInfo != NULL)
    {
        mexGetNAVLightStaticInfo(statinfo->navlightStaticInfo,&statinfo->common);   // CAT: the header (0x004b7680)
    }

    scriptSetGunStatics(directory,shipname,statinfo);
    if (statinfo->gunStaticInfo != NULL)
    {
        GunStaticInfo *gunstaticinfo = statinfo->gunStaticInfo;
        for (i=0;i<gunstaticinfo->numGuns;i++)
        {
            gunstaticinfo->gunstatics[i].gunindex = i;
        }
        mexGetGunStaticInfo(gunstaticinfo,statinfo->common.pMexData);
    }

    if (statinfo->bulletRange[0] != 0.0f)
    {
        for(i=0;i<NUM_TACTICS_TYPES;i++)
        {
            statinfo->bulletRangeSquared[i] = statinfo->bulletRange[i] * statinfo->bulletRange[i];
        }
    }


    //load in the hierarchy binding information
    scriptSetStruct(directory,shipname,HierarchyBindingTable,(ubyte *)statinfo);

    //load in More Mesh Animation info
    //must be AFTER we've loaded in all other MAD stuff, otherwise
    //I'll get MAD!
    if(statinfo->madStatic != NULL)
    {
        //default these to 0
        statinfo->madStatic->needStartPlacedAnimation = 0;
        statinfo->madStatic->needStartBuiltAnimation = 0;
        statinfo->madStatic->numGunOpenIndexes=0;
        statinfo->madStatic->numGunOpenDamagedIndexes=0;
        statinfo->madStatic->numGunCloseIndexes=0;
        statinfo->madStatic->numGunCloseDamagedIndexes=0;
        statinfo->madStatic->numDockIndexes=0;                   //number of below indices
        statinfo->madStatic->numDockDamagedIndexes=0;                   //number of below indices
        statinfo->madStatic->numPostDockIndexes=0;                   //number of below indices
        statinfo->madStatic->numPostDockDamagedIndexes=0;                   //number of below indices
        statinfo->madStatic->numDoorOpenIndexes=0;
        statinfo->madStatic->numDoorCloseIndexes=0;
        statinfo->madStatic->numSpecialOpenIndexes=0;
        statinfo->madStatic->numSpecialCloseIndexes=0;
        statinfo->madStatic->numSpecialOpenDamagedIndexes=0;
        statinfo->madStatic->numSpecialCloseDamagedIndexes=0;

    }

    scriptSetStruct(directory,shipname,MadMaxMadInfoLoad,(ubyte *)statinfo);

    if (statinfo->gunStaticInfo != NULL && stubbedOut == FALSE)
    {
        gunSetStaticGunInfoFromBindings(statinfo);

    }

    // CAT: the exe logs "SHP file loaded: %s" (stripped in retail); this port logs what it parsed, one line per ship
    //      (before the slot-1 call, so a stage-2 StaticInit that stops still leaves the parse in the log), in the form
    //      tools/formats/shipstatic_engine_check.py compares with tools/formats/shipstatic_probe.py
    dbgMessagef("shipstatic: loaded %s type=%d race=%d keys=%u guns=%d docks=%d navs=%d lod=%s staticinit=%s",
                fullshipname, type, race, shipKeysSet,
                statinfo->gunStaticInfo ? statinfo->gunStaticInfo->numGuns : 0,
                statinfo->dockStaticInfo ? statinfo->dockStaticInfo->numDockPoints : 0,
                statinfo->navlightStaticInfo ? statinfo->navlightStaticInfo->numNAVLights : 0,
                statinfo->common.LOD ? "yes" : "no",
                statinfo->custshipheader.CustShipStaticInit == NULL ? "none" :
                    ((catShipStaticsSelfTest && !catShipStaticsRunStaticInit) ? "skipped" : "next"));

    if (statinfo->custshipheader.CustShipStaticInit != NULL)
    {
        if (catShipStaticsSelfTest && !catShipStaticsRunStaticInit)
        {
            // CAT: /catShipStaticsTest loads every type and by default stops before the slot-1 bodies (stage 2);
            //      /catShipStaticsStaticInit runs them too (M2-R3 ships-a)
        }
        else
        {
            statinfo->custshipheader.CustShipStaticInit(directory,shipname,statinfo);
        }
    }

    // CAT: forceFieldProtectionDistanceScale <= 1.0 after the load becomes 2.0 (InitStatShipInfo 0x0053bab0)
    //      [audit: the threshold is 1.0, not 0: 0x0053c3b5 fcomp qword [0x0072a548] (double 1.0), and the stripped
    //      warning 0x008b58c8 reads "... Distance Scale is smaller than or equal to 1.0 ... Defaulting to 2.0"]
    if (statinfo->forceFieldProtectionDistanceScale <= 1.0f)
    {
        statinfo->forceFieldProtectionDistanceScale = 2.0f;
    }
}

/*-----------------------------------------------------------------------------
    Name        : catShipStaticsRunSelfTest
    Description : CAT: port diagnostic, not in the exe (command line /catShipStaticsTest). Loads the .shp of every
                  one of the 145 ship types the way universeStaticInit does (race = the type's race range), except
                  that CustShipStaticInit is skipped (the slot-1 bodies are stage 2). InitStatShipInfo logs one
                  "shipstatic: loaded ..." line per ship, or "shipstatic: ... not found ... disabled". Then it runs
                  the other .shp loaders this tree has (missile and mine per race, asteroids, dust clouds, nebulae,
                  HW1 derelict types whose file exists) and logs "shipstatic: other ..." per file. Every static is
                  closed again afterwards. tools/formats/shipstatic_engine_check.py compares the log with the probe.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
static void catSelfTestLogOther(char *kind, char *path)
{
    dbgMessagef("shipstatic: other %s %s loaded", kind, path);
}

void catShipStaticsRunSelfTest(void)
{
    static MissileStaticInfo missileScratch, mineScratch;
    static AsteroidStaticInfo asteroidScratch;
    static DustCloudStaticInfo dustcloudScratch;
    static NebulaStaticInfo nebulaScratch;
    static DerelictStaticInfo derelictScratch;
    char path[160];
    ShipRace race;
    ShipType type;
    sdword i;
    udword loaded = 0;

#ifdef _WIN32
#define CAT_SEP "\\"
#else
#define CAT_SEP "/"
#endif
    dbgMessagef("shipstatic: self-test start, %d ship types", TOTAL_NUM_SHIPS);
    // the shared defaults universeStaticInit loads before any static (a missing LOD level falls back to defaultmesh)
    if (sphereStaticInfo == NULL)
    {
        sphereStaticInfo = createSphereStaticInfo();
    }
    if (defaultmesh == NULL)
    {
        defaultmesh = meshLoad("DefaultShip" CAT_SEP "DefaultShip.geo");
    }
    if (defaultlod == NULL)
    {
        defaultlod = lodTableReadScript("DefaultShip" CAT_SEP, "DefaultShip.lod");
    }
    if (defaultmex == NULL)
    {
        defaultmex = mexLoad("DefaultShip" CAT_SEP "DefaultShip.mex");
    }
    for (race = 0; race < NUM_RACES; race++)
    {
        for (type = FirstShipTypeOfRace[race]; type <= LastShipTypeOfRace[race]; type++)
        {
            ShipStaticInfo *statinfo = GetShipStaticInfo(type, race);

            for (i = 0; i < catShipStaticsSkipCount; i++)
            {
                if (catShipStaticsSkip[i] == (sdword)type)
                {
                    break;
                }
            }
            if (i < catShipStaticsSkipCount)
            {
                dbgMessagef("shipstatic: skipped type %d (%s): /catShipStaticsSkip", type, ShipTypeToStr(type));
                continue;
            }
            InitStatShipInfo(statinfo, type, race);
            if (statinfo->common.LOD != NULL)     // every shipped .shp has a LODFile key
            {
                loaded++;
            }
            CloseStatShipInfo(statinfo);
        }
    }

    for (race = 0; race < NUM_RACES; race++)
    {
        sprintf(path, "%s" CAT_SEP "Missile.shp", ShipRaceToStr(race));
        if (fileExists(path, 0))
        {
            memset(&missileScratch, 0, sizeof(missileScratch));
            InitStatMissileInfo(&missileScratch, race);
            catSelfTestLogOther("missile", path);
            CloseStatMissileInfo(&missileScratch);
        }
        sprintf(path, "%s" CAT_SEP "Mine.shp", ShipRaceToStr(race));
        if (fileExists(path, 0))
        {
            memset(&mineScratch, 0, sizeof(mineScratch));
            InitStatMineInfo(&mineScratch, race);
            catSelfTestLogOther("mine", path);
            CloseStatMineInfo(&mineScratch);
        }
    }
    for (i = 0; i < NUM_ASTEROIDTYPES; i++)
    {
        sprintf(path, "Resources" CAT_SEP "Asteroids" CAT_SEP "%s.shp", AsteroidTypeToStr((AsteroidType)i));
        if (fileExists(path, 0))
        {
            memset(&asteroidScratch, 0, sizeof(asteroidScratch));
            InitStatAsteroidInfo(&asteroidScratch, (AsteroidType)i);
            catSelfTestLogOther("asteroid", path);
            CloseStatAsteroidInfo(&asteroidScratch);
        }
    }
    for (i = 0; i < NUM_DUSTCLOUDTYPES; i++)
    {
        sprintf(path, "Resources" CAT_SEP "DustClouds" CAT_SEP "%s.shp", DustCloudTypeToStr((DustCloudType)i));
        if (fileExists(path, 0))
        {
            memset(&dustcloudScratch, 0, sizeof(dustcloudScratch));
            InitStatDustCloudInfo(&dustcloudScratch, (DustCloudType)i);
            catSelfTestLogOther("dustcloud", path);
            CloseStatDustCloudInfo(&dustcloudScratch);
        }
    }
    for (i = 0; i < NUM_NEBULATYPES; i++)
    {
        sprintf(path, "Resources" CAT_SEP "Nebulae" CAT_SEP "%s.shp", NebulaTypeToStr((NebulaType)i));
        if (fileExists(path, 0))
        {
            memset(&nebulaScratch, 0, sizeof(nebulaScratch));
            InitStatNebulaInfo(&nebulaScratch, (NebulaType)i);
            catSelfTestLogOther("nebula", path);
            CloseStatNebulaInfo(&nebulaScratch);
        }
    }
    for (i = 0; i < NUM_DERELICTTYPES; i++)
    {
        if ((DerelictType)i == HyperspaceGate)
        {
            continue;
        }
        sprintf(path, "Derelicts" CAT_SEP "%s.shp", DerelictTypeToStr((DerelictType)i));
        if (fileExists(path, 0))     // HW1's derelict enum: only the names Cataclysm still ships are loaded here
        {
            memset(&derelictScratch, 0, sizeof(derelictScratch));
            InitStatDerelictInfo(&derelictScratch, (DerelictType)i);
            catSelfTestLogOther("derelict", path);
            CloseStatDerelictInfo(&derelictScratch);
        }
    }
#undef CAT_SEP

    dbgMessagef("shipstatic: self-test done, %u of %d ship types loaded", loaded, TOTAL_NUM_SHIPS);
}

/*-----------------------------------------------------------------------------
    Name        : InitStatShipInfoPost
    Description : Initializes static ship information for all ship types. Post operation
                  uses information from previous pass to complete the initialization.
    Inputs      : type, race
    Outputs     : statinfo is filled in
    Return      :
----------------------------------------------------------------------------*/
void InitStatShipInfoPost(ShipStaticInfo *statinfo)
{
    // CAT: the zero test and 1/maxhealth run whatever the modifier, and the result is kept in maxhealthBase for the
    //      single-player difficulty factor (universeStaticInit 0x0053d2a0 inlines this function)
    if (GLOBAL_SHIP_HEALTH_MODIFIER != 1.0f)
    {
        statinfo->health.maxhealth = (GLOBAL_SHIP_HEALTH_MODIFIER * (statinfo->health.maxhealth));
    }
    if (statinfo->health.maxhealth == 0.0f)
    {
        statinfo->health.maxhealth = 1.0f;
    }
    statinfo->maxhealthBase = statinfo->health.maxhealth;
    statinfo->health.oneOverMaxHealth = 1.0f/statinfo->health.maxhealth;

    if (statinfo->custshipheader.CustShipStaticInitPost != NULL)
    {
        statinfo->custshipheader.CustShipStaticInitPost(statinfo);
    }
}

/*-----------------------------------------------------------------------------
    Name        : CloseStatInfo
    Description : Close base static info
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void CloseStatInfo(StaticInfo *statinfo)
{
    if ((statinfo->pMexData != defaultmex) && (statinfo->pMexData != NULL))
    {
        mexFree(statinfo->pMexData);
        statinfo->pMexData = NULL;
    }

    if ((statinfo->LOD != defaultlod) && (statinfo->LOD != NULL))
    {
        lodFree(statinfo->LOD);
        statinfo->LOD = NULL;
    }
    //statinfo->common.infoLoaded = statinfo->common.infoNeeded = FALSE;
    statinfo->infoFlags = 0;
    memset(statinfo, 0, sizeof(StaticInfo));
}

/*-----------------------------------------------------------------------------
    Name        : CloseStatShipInfo
    Description : Closes static ship info
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void CloseStatShipInfo(ShipStaticInfo *statinfo)
{
    sdword i;

    CloseStatInfo((StaticInfo *)statinfo);

    for (i = 0; i < MAX_NUM_TRAILS; i++)
    {
        if (statinfo->trailStatic[i] != NULL)
        {
            trailStaticDelete(statinfo->trailStatic[i]);
            statinfo->trailStatic[i] = NULL;
        }
    }

    if (statinfo->madStatic != NULL)
    {
        if(statinfo->madStatic->gunOpenIndexes != NULL)
        {
            memFree(statinfo->madStatic->gunOpenIndexes);
        }
        if(statinfo->madStatic->gunCloseIndexes != NULL)
        {
            memFree(statinfo->madStatic->gunCloseIndexes);
        }
        if(statinfo->madStatic->gunOpenDamagedIndexes != NULL)
        {
            memFree(statinfo->madStatic->gunOpenDamagedIndexes);
        }
        if(statinfo->madStatic->gunCloseDamagedIndexes != NULL)
        {
            memFree(statinfo->madStatic->gunCloseDamagedIndexes);
        }
        //dock anims
        if(statinfo->madStatic->PostDockIndexes != NULL)
        {
            memFree(statinfo->madStatic->PostDockIndexes);
        }
        if(statinfo->madStatic->DockIndexes != NULL)
        {
            memFree(statinfo->madStatic->DockIndexes);
        }
        if(statinfo->madStatic->PostDockDamagedIndexes != NULL)
        {
            memFree(statinfo->madStatic->PostDockDamagedIndexes);
        }
        if(statinfo->madStatic->DockDamagedIndexes != NULL)
        {
            memFree(statinfo->madStatic->DockDamagedIndexes);
        }
        //mothership door anims
        if(statinfo->madStatic->DoorOpenIndexes != NULL)
        {
            memFree(statinfo->madStatic->DoorOpenIndexes);
        }
        if(statinfo->madStatic->DoorCloseIndexes != NULL)
        {
            memFree(statinfo->madStatic->DoorCloseIndexes);
        }
        //special case
        if(statinfo->madStatic->specialOpenIndexes != NULL)
        {
            memFree(statinfo->madStatic->specialOpenIndexes);
        }
        if(statinfo->madStatic->specialCloseIndexes != NULL)
        {
            memFree(statinfo->madStatic->specialCloseIndexes);
        }
        if(statinfo->madStatic->specialOpenDamagedIndexes != NULL)
        {
            memFree(statinfo->madStatic->specialOpenDamagedIndexes);
        }
        if(statinfo->madStatic->specialCloseDamagedIndexes != NULL)
        {
            memFree(statinfo->madStatic->specialCloseDamagedIndexes);
        }


        // CAT-FIX: the exe never frees the NUMBER_OF_ANIMS table (CloseStatShipInfo 0x0053c500 frees the 14 HW1 index
        //      arrays only, a leak per close); the port frees it
        if(statinfo->madStatic->animIndexes != NULL)
        {
            memFree(statinfo->madStatic->animIndexes);
        }

        madHeaderDelete(statinfo->madStatic->header);
        //... other mesh binding freeing stuffs
        memFree(statinfo->madStatic);
        statinfo->madStatic = NULL;             // CAT: the exe clears it (0x0053c500)

    }
    if(statinfo->dockOverideInfo != NULL)
    {
        memFree(statinfo->dockOverideInfo);
        statinfo->dockOverideInfo = NULL;       // CAT: the exe clears it (0x0053c500)
    }
    if (statinfo->resNozzleStatic != NULL)
    {
        memFree(statinfo->resNozzleStatic);
        statinfo->resNozzleStatic = NULL;
    }
    if(statinfo->repairNozzleStatic != NULL)
    {
        memFree(statinfo->repairNozzleStatic);
        statinfo->repairNozzleStatic = NULL;
    }

    // CAT: no salvageStaticInfo and no tractorEmitterStatic to free (neither exists in Cataclysm's ShipStaticInfo;
    //      CloseStatShipInfo 0x0053c500 frees +0x3F4, +0x3F8, +0x3FC, +0x3F0, +0x104, +0x3EC only)


    if (statinfo->dockStaticInfo != NULL)
    {
        memFree(statinfo->dockStaticInfo);
        statinfo->dockStaticInfo = NULL;
    }

    if (statinfo->navlightStaticInfo != NULL)
    {
        navLightStaticInfoDelete(statinfo->navlightStaticInfo);
        statinfo->navlightStaticInfo = NULL;
    }

    if (statinfo->gunStaticInfo != NULL)
    {
        memFree(statinfo->gunStaticInfo);
        statinfo->gunStaticInfo = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : CloseStatAsteroidInfo
    Description : Closes static asteroid info
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void CloseStatAsteroidInfo(AsteroidStaticInfo *asteroidStatInfo)
{
    CloseStatInfo((StaticInfo *)asteroidStatInfo);
}

/*-----------------------------------------------------------------------------
    Name        : CloseStatDustCloudInfo
    Description : Closes static dustcloud info
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void CloseStatDustCloudInfo(DustCloudStaticInfo *dustcloudStatInfo)
{
    CloseStatInfo((StaticInfo *)dustcloudStatInfo);
}

/*-----------------------------------------------------------------------------
    Name        : CloseStatGasCloudInfo
    Description : Closes static gascloud info
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void CloseStatGasCloudInfo(GasCloudStaticInfo *gascloudStatInfo)
{
    CloseStatInfo((StaticInfo *)gascloudStatInfo);
}

/*-----------------------------------------------------------------------------
    Name        : CloseStatNebulaInfo
    Description : Closes static nebula info
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void CloseStatNebulaInfo(NebulaStaticInfo *nebulaStatInfo)
{
//    CloseStatInfo((StaticInfo *)nebulaStatInfo);
}

/*-----------------------------------------------------------------------------
    Name        : CloseStatDerelictInfo
    Description : Closes static derelict info
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void CloseStatDerelictInfo(DerelictStaticInfo *derelictStatInfo)
{
    // CAT: no salvageStaticInfo in Cataclysm (no NUMBER_OF_SALVAGE_POINTS key anywhere in the exe)
    CloseStatInfo((StaticInfo *)derelictStatInfo);
}

/*-----------------------------------------------------------------------------
    Name        : CloseStatMissileInfo
    Description : Closes static missile info
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void CloseStatMissileInfo(MissileStaticInfo *missileStatInfo)
{
    CloseStatInfo((StaticInfo *)missileStatInfo);
}
/*-----------------------------------------------------------------------------
    Name        : CloseStatMineInfo
    Description : Closes static mine info
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void CloseStatMineInfo(MissileStaticInfo *mineStatInfo)
{
    CloseStatInfo((StaticInfo *)mineStatInfo);
}

void CloseStatEffectInfo(etgeffectstatic *effectStatInfo)
{
//    CloseStatInfo((EffectStaticInfo *)effectStatInfo);
}

/*-----------------------------------------------------------------------------
    Name        : universeInfoNeededSetCB
    Description : Static info scan callback to flag a static info as needed or
                    not needed.
    Inputs      : info - static info to look at
                  objType - type of object to work with
    Outputs     : modifies the IF_InfoNeeded bit of infoFlags to bNeededParam
    Return      : void
----------------------------------------------------------------------------*/
static sdword bNeededParam;
void universeInfoNeededSetCB(StaticInfo *info, ObjType objType)
{
    bitSetTo(info->infoFlags, IF_InfoNeeded, bNeededParam);
}

/*=============================================================================
    Public Functions:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : universeFlagRaceNeeded
    Description : Marks an entire race of staticinfo's as needed or not
    Inputs      : shiprace - what race to mark
                  bNeeded - TRUE if we need this static info loaded this level
    Outputs     :
    Return      : void
    Note        : Missiles/mines will be turned on/off with the associated ships.
----------------------------------------------------------------------------*/
void universeFlagRaceNeeded(ShipRace shiprace, bool8 bNeeded)
{
    bNeededParam = bNeeded;
    universeRaceStaticsScan(shiprace, universeInfoNeededSetCB, universeInfoNeededSetCB);
}

/*-----------------------------------------------------------------------------
    Name        : universeDefaultTeamColors
    Description : Sets the defualts for what colors the ships can be in -
                    whatever players are that race.
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void universeDefaultTeamColors(void)
{
    bool8 racePlayerTable[NUM_RACES][MAX_MULTIPLAYER_PLAYERS];
    sdword index;
    ShipRace shiprace;
    ShipType shiptype;
    ShipType firstshiptype;
    ShipType lastshiptype;
    DerelictType derelicttype;
    ShipStaticInfo *shipstaticinfo;
    MissileStaticInfo *missileInfo;

    //make a boolean table of color schemes by race
    memset(racePlayerTable, 0, sizeof(racePlayerTable));
    for (index = 0; index < numPlayers; index++)
    {
        racePlayerTable[universe.players[index].race][index] = TRUE;
    }

    // CAT: the default colour schemes of the non-player races depend on the game: the tutorial, multiplayer, and each
    //      single-player mission have their own set (universeDefaultTeamColors 0x0053c940, racePlayerTable [7][8] on
    //      the stack; HW1 set Traders [7] and P1/P2/P3 for every game). Every set but missions 5, 7, 11 and 15 also
    //      sets [RACE_Derelict][0]; a mission number outside 1..17 adds nothing to the three Sect/Beast entries.
    if (tutorial == 1)
    {
        racePlayerTable[RACE_Sect][0] = TRUE;
        racePlayerTable[RACE_Sect][4] = TRUE;
        racePlayerTable[RACE_Taiidan][2] = TRUE;
        racePlayerTable[RACE_Taiidan][0] = TRUE;
        racePlayerTable[RACE_Derelict][0] = TRUE;
    }
    else if (!singlePlayerGame)
    {
        racePlayerTable[RACE_Taiidan][2] = TRUE;
        racePlayerTable[RACE_Kushan][6] = TRUE;
        racePlayerTable[RACE_P1][5] = TRUE;
        racePlayerTable[RACE_Traders][5] = TRUE;
        racePlayerTable[RACE_Derelict][0] = TRUE;
    }
    else
    {
        racePlayerTable[RACE_Sect][0] = TRUE;
        racePlayerTable[RACE_Sect][1] = TRUE;
        racePlayerTable[RACE_Beast][1] = TRUE;
        if ((singlePlayerGameInfo.currentMission >= 8) && (singlePlayerGameInfo.currentMission <= 17))
        {
            // missions 8..17 all set these four (the exe's default case, a mission above 17, does not)
            // [audit: was ">= 8" alone, which also set them for a mission above 17]
            racePlayerTable[RACE_P1][0] = TRUE;
            racePlayerTable[RACE_P1][1] = TRUE;
            racePlayerTable[RACE_Taiidan][0] = TRUE;
            racePlayerTable[RACE_Taiidan][1] = TRUE;
        }
        switch (singlePlayerGameInfo.currentMission)
        {
            case 1:
                racePlayerTable[RACE_Taiidan][2] = TRUE;
                racePlayerTable[RACE_Kushan][4] = TRUE;
                racePlayerTable[RACE_Kushan][6] = TRUE;
                racePlayerTable[RACE_Kushan][7] = TRUE;
                racePlayerTable[RACE_Derelict][0] = TRUE;
                break;
            case 2:
                racePlayerTable[RACE_Kushan][4] = TRUE;
                racePlayerTable[RACE_P1][5] = TRUE;
                racePlayerTable[RACE_Derelict][0] = TRUE;
                break;
            case 3:
                racePlayerTable[RACE_P1][5] = TRUE;
                racePlayerTable[RACE_P1][0] = TRUE;
                racePlayerTable[RACE_Derelict][0] = TRUE;
                racePlayerTable[RACE_Derelict][1] = TRUE;
                racePlayerTable[RACE_Taiidan][5] = TRUE;
                break;
            case 4:
                racePlayerTable[RACE_Beast][0] = TRUE;
                racePlayerTable[RACE_P1][5] = TRUE;
                racePlayerTable[RACE_P1][0] = TRUE;
                racePlayerTable[RACE_P1][1] = TRUE;
                racePlayerTable[RACE_Taiidan][5] = TRUE;
                racePlayerTable[RACE_Derelict][0] = TRUE;
                break;
            case 5:
                racePlayerTable[RACE_P1][0] = TRUE;
                racePlayerTable[RACE_P1][1] = TRUE;
                racePlayerTable[RACE_Traders][5] = TRUE;
                racePlayerTable[RACE_Kushan][4] = TRUE;
                break;
            case 6:
                racePlayerTable[RACE_P1][0] = TRUE;
                racePlayerTable[RACE_Taiidan][0] = TRUE;
                racePlayerTable[RACE_Taiidan][2] = TRUE;
                racePlayerTable[RACE_Traders][2] = TRUE;
                racePlayerTable[RACE_Kushan][4] = TRUE;
                racePlayerTable[RACE_Derelict][0] = TRUE;
                racePlayerTable[RACE_Derelict][1] = TRUE;
                break;
            case 7:
                racePlayerTable[RACE_P1][0] = TRUE;
                racePlayerTable[RACE_P1][1] = TRUE;
                racePlayerTable[RACE_Taiidan][0] = TRUE;
                racePlayerTable[RACE_Taiidan][1] = TRUE;
                racePlayerTable[RACE_Kushan][4] = TRUE;
                racePlayerTable[RACE_Derelict][1] = TRUE;
                racePlayerTable[RACE_Derelict][3] = TRUE;
                break;
            case 8:
                racePlayerTable[RACE_Taiidan][2] = TRUE;
                racePlayerTable[RACE_Taiidan][3] = TRUE;
                racePlayerTable[RACE_Derelict][0] = TRUE;
                break;
            case 9:
                racePlayerTable[RACE_Taiidan][2] = TRUE;
                racePlayerTable[RACE_Derelict][0] = TRUE;
                break;
            case 10:
                racePlayerTable[RACE_Taiidan][2] = TRUE;
                racePlayerTable[RACE_P1][5] = TRUE;
                racePlayerTable[RACE_Derelict][0] = TRUE;
                racePlayerTable[RACE_Derelict][1] = TRUE;
                break;
            case 11:
                racePlayerTable[RACE_Derelict][1] = TRUE;
                racePlayerTable[RACE_Traders][0] = TRUE;
                break;
            case 12:
                racePlayerTable[RACE_Sect][5] = TRUE;
                racePlayerTable[RACE_Taiidan][5] = TRUE;
                racePlayerTable[RACE_P1][5] = TRUE;
                racePlayerTable[RACE_Derelict][0] = TRUE;
                racePlayerTable[RACE_Derelict][1] = TRUE;
                racePlayerTable[RACE_Taiidan][2] = TRUE;
                break;
            case 13:
                racePlayerTable[RACE_Derelict][0] = TRUE;
                racePlayerTable[RACE_Kushan][0] = TRUE;
                racePlayerTable[RACE_Taiidan][2] = TRUE;
                racePlayerTable[RACE_Traders][0] = TRUE;
                break;
            case 14:
                racePlayerTable[RACE_Kushan][0] = TRUE;
                racePlayerTable[RACE_P1][5] = TRUE;
                racePlayerTable[RACE_Taiidan][2] = TRUE;
                racePlayerTable[RACE_Derelict][0] = TRUE;
                racePlayerTable[RACE_Derelict][1] = TRUE;
                break;
            case 15:
                racePlayerTable[RACE_Kushan][0] = TRUE;
                racePlayerTable[RACE_Traders][5] = TRUE;
                racePlayerTable[RACE_Derelict][5] = TRUE;
                break;
            case 16:
                racePlayerTable[RACE_Kushan][4] = TRUE;
                racePlayerTable[RACE_Kushan][0] = TRUE;
                racePlayerTable[RACE_Derelict][1] = TRUE;
                racePlayerTable[RACE_Derelict][0] = TRUE;
                break;
            case 17:
                racePlayerTable[RACE_Kushan][0] = TRUE;
                racePlayerTable[RACE_Derelict][1] = TRUE;
                racePlayerTable[RACE_Derelict][0] = TRUE;
                racePlayerTable[RACE_Taiidan][2] = TRUE;
                racePlayerTable[RACE_Taiidan][3] = TRUE;
                racePlayerTable[RACE_Traders][5] = TRUE;
                break;
            default:
                break;
        }
    }

    //now plug that default table into all the ship static infos
    for (shiprace=0;shiprace<NUM_RACES;shiprace++)
    {
        firstshiptype = FirstShipTypeOfRace[shiprace];
        lastshiptype = LastShipTypeOfRace[shiprace];
        for (shiptype=firstshiptype;shiptype<=lastshiptype;shiptype++)
        {
            shipstaticinfo = GetShipStaticInfo(shiptype,shiprace);
            memcpy(shipstaticinfo->shipOrDerelict.teamColor, racePlayerTable[shiprace], sizeof(shipstaticinfo->shipOrDerelict.teamColor));
        }
        missileInfo = &missileStaticInfos[shiprace];
        memcpy(missileInfo->teamColor, racePlayerTable[shiprace], sizeof(missileInfo->teamColor));
        // CAT: mines of every race (HW1: R1 and R2 only)
        missileInfo = &mineStaticInfos[shiprace];
        memcpy(missileInfo->teamColor, racePlayerTable[shiprace], sizeof(missileInfo->teamColor));
    }
    // CAT: derelicts take the Derelict race's row (HW1: the Traders' row)
    for (derelicttype = 0; derelicttype < NUM_DERELICTTYPES; derelicttype++)
    {
        memcpy(derelictStaticInfos[derelicttype].shipOrDerelict.teamColor, racePlayerTable[RACE_Derelict], sizeof(racePlayerTable[RACE_Derelict]));
    }
}

/*-----------------------------------------------------------------------------
    Name        : universeFlagEverythingNeeded
    Description : Flags all static infos to be reloaded during the next level
    Inputs      : void
    Outputs     : sets the infoNeeded flag true for all static infos
    Return      : void
----------------------------------------------------------------------------*/
void universeFlagEverythingNeeded(void)
{
    bNeededParam = TRUE;
    universeAllStaticsScan(universeInfoNeededSetCB, universeInfoNeededSetCB, universeInfoNeededSetCB, universeInfoNeededSetCB);
}

/*-----------------------------------------------------------------------------
    Name        : universeRaceStaticsScan
    Description : Scans all static infos needed for a race and calls appropriate
                    callbacks.
    Inputs      : shiprace - what race to scan
                  forShips - called for ship static infos
                  forMisc - called for other statics, like mines and missiles
    Outputs     :
    Return      : void
    Note        : Missiles/mines will be turned on/off with the associated ships.
----------------------------------------------------------------------------*/
void universeRaceStaticsScan(ShipRace shiprace, univstatcallback forShips, univstatcallback forMisc)
{
    ShipType shiptype;
    ShipType firstshiptype;
    ShipType lastshiptype;
    ShipStaticInfo *shipstaticinfo;

    firstshiptype = FirstShipTypeOfRace[shiprace];
    lastshiptype = LastShipTypeOfRace[shiprace];
    for (shiptype=firstshiptype;shiptype<=lastshiptype;shiptype++)
    {
        shipstaticinfo = GetShipStaticInfo(shiptype,shiprace);
        if (forShips)
        {
            forShips((StaticInfo *)shipstaticinfo, OBJ_ShipType);
        }
        if (forMisc)
        {
            // CAT: Cataclysm's missile and mine carriers (universeFlagRaceNeeded 0x0053c870, which inlines this scan:
            //      types 0x55, 10, 0x5f, 0x19 -> missileStaticInfos[race]; 0x54 -> mineStaticInfos[race])
            if (shiptype == tMissileDestroyer || shiptype == sDreadnought ||
                shiptype == p1MissileCorvette || shiptype == sDestroyer)
            {
                forMisc((StaticInfo *)&missileStaticInfos[shiprace], OBJ_MissileType);
            }
            else if (shiptype == tMinelayerCorvette)
            {
                forMisc((StaticInfo *)&mineStaticInfos[shiprace], OBJ_MissileType);
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : universeAllStaticsScan
    Description : Scans through the static info structures for all spaceobj
                    types and calls a callback with that static info.
    Inputs      : forShips - called for ship static infos
                  forDerelicts, forResources, forMisc - same, but for derelicts,
                  resources and whatnot.
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void universeAllStaticsScan(univstatcallback forShips, univstatcallback forDerelicts, univstatcallback forResources, univstatcallback forMisc)
{
    ShipRace shiprace;
    AsteroidType asteroidtype;
    DustCloudType dustcloudtype;
    GasCloudType gascloudtype;
    NebulaType nebulatype;
    DerelictType derelicttype;

    for (shiprace=0;shiprace<NUM_RACES;shiprace++)
    {
        universeRaceStaticsScan(shiprace, forShips, forMisc);
    }
    if (forResources != NULL)
    {
        for (asteroidtype=0;asteroidtype<NUM_ASTEROIDTYPES;asteroidtype++)
        {
            forResources((StaticInfo *)&asteroidStaticInfos[asteroidtype], OBJ_AsteroidType);
        }
        for (dustcloudtype = 0; dustcloudtype < NUM_DUSTCLOUDTYPES; dustcloudtype++)
        {
            forResources((StaticInfo *)&dustcloudStaticInfos[dustcloudtype], OBJ_DustType);
        }
        for (gascloudtype = 0; gascloudtype < NUM_GASCLOUDTYPES; gascloudtype++)
        {
            forResources((StaticInfo *)&gascloudStaticInfos[gascloudtype], OBJ_GasType);
        }
        for (nebulatype = 0; nebulatype < NUM_NEBULATYPES; nebulatype++)
        {
            forResources((StaticInfo *)&nebulaStaticInfos[nebulatype], OBJ_NebulaType);
        }
    }
    if (forDerelicts != NULL)
    {
        for (derelicttype=0;derelicttype<NUM_DERELICTTYPES;derelicttype++)
        {
            forDerelicts((StaticInfo *)&derelictStaticInfos[derelicttype], OBJ_DerelictType);
        }
    }
    if (forMisc != NULL)
    {
        for (shiprace=0;shiprace<NUM_RACES;shiprace++)
        {
            forMisc((StaticInfo *)&missileStaticInfos[shiprace], OBJ_MissileType);
        }
/*
    for (shiprace=0;shiprace<2;shiprace++)
    {
        bitSet(mineStaticInfos[shiprace].common.infoFlags, IF_InfoNeeded);
    }
*/
    }
}

/*-----------------------------------------------------------------------------
    Name        : universeFlagEverythingNeeded
    Description : Flags all static infos to be reloaded during the next level
    Inputs      : void
    Outputs     : sets the infoNeeded flag FALSE for all static infos
    Return      : void
----------------------------------------------------------------------------*/
void universeFlagNothingNeeded(void)
{
    bNeededParam = FALSE;
    universeAllStaticsScan(universeInfoNeededSetCB, universeInfoNeededSetCB, universeInfoNeededSetCB, universeInfoNeededSetCB);
}

ubyte bitpackTeamColor(bool8 teamColor[])
{
    sdword i;
    ubyte bitmask = 0;

    for (i=0;i<8;i++)
    {
        bitmask <<= 1;
        if (teamColor[i])
        {
            bitmask |= 1;
        }
    }

    return bitmask;
}

void unbitpackTeamColor(bool8 teamColor[],ubyte pack)
{
    sdword i;

    for (i=0;i<8;i++)
    {
        if (pack & 128)
        {
            teamColor[i] = TRUE;
        }
        else
        {
            teamColor[i] = FALSE;
        }
        pack <<= 1;
    }
}

#define INFO_NEEDED_FLAGS_DELIMITER         0xf1a5deed

void universeSaveEverythingNeeded(void)
{
    ShipRace shiprace;
    AsteroidType asteroidtype;
    DustCloudType dustcloudtype;
    GasCloudType gascloudtype;
    NebulaType nebulatype;
    DerelictType derelicttype;

    ShipType shiptype;
    ShipType firstshiptype;
    ShipType lastshiptype;
    ShipStaticInfo *shipstatic;

    ubyte data[4];

    SaveInfoNumber(INFO_NEEDED_FLAGS_DELIMITER);

    for (shiprace=0;shiprace<NUM_RACES;shiprace++)
    {
        firstshiptype = FirstShipTypeOfRace[shiprace];
        lastshiptype = LastShipTypeOfRace[shiprace];
        for (shiptype=firstshiptype;shiptype<=lastshiptype;shiptype++)
        {
            shipstatic = GetShipStaticInfo(shiptype,shiprace);
            data[0] = bitTest(shipstatic->common.infoFlags, IF_InfoLoaded);      // anything loaded is needed
            data[1] = bitpackTeamColor(shipstatic->shipOrDerelict.teamColor);
            data[2] = 0;
            data[3] = 0;
            SaveInfoNumber(TreatAsUdword(data[0]));
        }
    }

    for (asteroidtype=0;asteroidtype<NUM_ASTEROIDTYPES;asteroidtype++)
    {
        SaveInfoNumber(bitTest(asteroidStaticInfos[asteroidtype].common.infoFlags, IF_InfoLoaded));
    }
    for (dustcloudtype = 0; dustcloudtype < NUM_DUSTCLOUDTYPES; dustcloudtype++)
    {
        SaveInfoNumber(bitTest(dustcloudStaticInfos[dustcloudtype].common.infoFlags, IF_InfoLoaded));
    }
    for (gascloudtype = 0; gascloudtype < NUM_GASCLOUDTYPES; gascloudtype++)
    {
        SaveInfoNumber(bitTest(gascloudStaticInfos[gascloudtype].common.infoFlags, IF_InfoLoaded));
    }
    for (nebulatype = 0; nebulatype < NUM_NEBULATYPES; nebulatype++)
    {
        SaveInfoNumber(bitTest(nebulaStaticInfos[nebulatype].common.infoFlags, IF_InfoLoaded));
    }
    for (derelicttype=0;derelicttype<NUM_DERELICTTYPES;derelicttype++)
    {
        DerelictStaticInfo *dstaticinfo = &derelictStaticInfos[derelicttype];
        data[0] = bitTest(dstaticinfo->common.infoFlags, IF_InfoLoaded);
        data[1] = bitpackTeamColor(dstaticinfo->shipOrDerelict.teamColor);
        data[2] = 0;
        data[3] = 0;
        SaveInfoNumber(TreatAsUdword(data[0]));
    }
    for (shiprace=0;shiprace<NUM_RACES;shiprace++)
    {
        MissileStaticInfo *misstaticinfo = &missileStaticInfos[shiprace];
        data[0] = bitTest(misstaticinfo->common.infoFlags, IF_InfoLoaded);
        data[1] = bitpackTeamColor(misstaticinfo->teamColor);
        data[2] = 0;
        data[3] = 0;
        SaveInfoNumber(TreatAsUdword(data[0]));
    }
    for (shiprace=0;shiprace<NUM_RACES;shiprace++)      // CAT: 7 mines (universeSaveEverythingNeeded 0x0053ce80; HW1 2)
    {
        MissileStaticInfo *misstaticinfo = &mineStaticInfos[shiprace];
        data[0] = bitTest(misstaticinfo->common.infoFlags, IF_InfoLoaded);
        data[1] = bitpackTeamColor(misstaticinfo->teamColor);
        data[2] = 0;
        data[3] = 0;
        SaveInfoNumber(TreatAsUdword(data[0]));
    }

    SaveInfoNumber(INFO_NEEDED_FLAGS_DELIMITER);
}

void universeLoadEverythingNeeded(void)
{
    ShipRace shiprace;
    AsteroidType asteroidtype;
    DustCloudType dustcloudtype;
    GasCloudType gascloudtype;
    NebulaType nebulatype;
    DerelictType derelicttype;

    ShipType shiptype;
    ShipType firstshiptype;
    ShipType lastshiptype;
    ShipStaticInfo *shipstatic;

    udword data;
    ubyte *datab;

    datab = (ubyte *)&data;

    data = LoadInfoNumber();                        // sanity check
    dbgAssertOrIgnore(data == INFO_NEEDED_FLAGS_DELIMITER);

    for (shiprace=0;shiprace<NUM_RACES;shiprace++)
    {
        firstshiptype = FirstShipTypeOfRace[shiprace];
        lastshiptype = LastShipTypeOfRace[shiprace];
        for (shiptype=firstshiptype;shiptype<=lastshiptype;shiptype++)
        {
            shipstatic = GetShipStaticInfo(shiptype,shiprace);
            data = LoadInfoNumber();
            bitSetTo(shipstatic->common.infoFlags, IF_InfoNeeded, datab[0]);
            rmEnableShip(shiprace, shiptype, datab[0]);
            unbitpackTeamColor(shipstatic->shipOrDerelict.teamColor,datab[1]);
        }
    }

    for (asteroidtype=0;asteroidtype<NUM_ASTEROIDTYPES;asteroidtype++)
    {
        bitSetTo(asteroidStaticInfos[asteroidtype].common.infoFlags, IF_InfoNeeded, LoadInfoNumber());
    }
    for (dustcloudtype = 0; dustcloudtype < NUM_DUSTCLOUDTYPES; dustcloudtype++)
    {
        bitSetTo(dustcloudStaticInfos[dustcloudtype].common.infoFlags, IF_InfoNeeded, LoadInfoNumber());
    }
    for (gascloudtype = 0; gascloudtype < NUM_GASCLOUDTYPES; gascloudtype++)
    {
        bitSetTo(gascloudStaticInfos[gascloudtype].common.infoFlags, IF_InfoNeeded, LoadInfoNumber());
    }
    for (nebulatype = 0; nebulatype < NUM_NEBULATYPES; nebulatype++)
    {
        bitSetTo(nebulaStaticInfos[nebulatype].common.infoFlags, IF_InfoNeeded, LoadInfoNumber());
    }
    for (derelicttype=0;derelicttype<NUM_DERELICTTYPES;derelicttype++)
    {
        DerelictStaticInfo *dstaticinfo = &derelictStaticInfos[derelicttype];
        data = LoadInfoNumber();
        bitSetTo(dstaticinfo->common.infoFlags, IF_InfoNeeded, datab[0]);
        unbitpackTeamColor(dstaticinfo->shipOrDerelict.teamColor,datab[1]);
    }
    for (shiprace=0;shiprace<NUM_RACES;shiprace++)
    {
        MissileStaticInfo *misstaticinfo = &missileStaticInfos[shiprace];
        data = LoadInfoNumber();
        bitSetTo(misstaticinfo->common.infoFlags, IF_InfoNeeded, datab[0]);
        unbitpackTeamColor(misstaticinfo->teamColor,datab[1]);
    }
    for (shiprace=0;shiprace<NUM_RACES;shiprace++)      // CAT: 7 mines (universeLoadEverythingNeeded 0x0053d090; HW1 2)
    {
        MissileStaticInfo *misstaticinfo = &mineStaticInfos[shiprace];
        data = LoadInfoNumber();
        bitSetTo(misstaticinfo->common.infoFlags, IF_InfoNeeded, datab[0]);
        unbitpackTeamColor(misstaticinfo->teamColor,datab[1]);
    }

    data = LoadInfoNumber();                        // sanity check
    dbgAssertOrIgnore(data == INFO_NEEDED_FLAGS_DELIMITER);
}

/*-----------------------------------------------------------------------------
    Name        : universeStaticInit
    Description : Initializes static data for the Universe.  Only the ships,
                    resources, missiles etc. that are needed will be loaded.
                    Anything that was previously loaded and is no longer
                    needed will be deleted.
    Inputs      : none
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
extern void *utyTeaserHeader;
void universeStaticInit(void)
{
    ShipRace shiprace;
    ShipType shiptype;
    ShipType firstshiptype;
    ShipType lastshiptype;
    AsteroidType asteroidtype;
    DustCloudType dustcloudtype;
    NebulaType nebulatype;
    DerelictType derelicttype;
    ShipStaticInfo *shipstaticinfo;
    //TODO(type safety, 2026-04-24): Replace with scoped variables of the appropriate type
    StaticInfo *staticinfo;
    udword max,count;
#if UNIV_SHIP_LOADFREE_LOG
    char *fileNameFull;
    FILE *logFile = NULL;

    if (univLoadFreeLog)
    {
        fileNameFull = filePathPrepend(
            UNIV_LOAD_FREE_FILENAME,
            FF_UserSettingsPath);

        if (fileMakeDestinationDirectory(fileNameFull))
            logFile = fopen(fileNameFull, "at");
    }
    if (logFile)
    {
        if (singlePlayerGame)
        {
            fprintf(logFile, "\n-----------\nCurrent level = %d\n", spGetCurrentMission());
        }
    }
#endif

#ifdef _WIN32
#define TMP_DEFSHIP_PATH "DefaultShip\\"
#else
#define TMP_DEFSHIP_PATH "DefaultShip/"
#endif

    //load some things shared by everyone
    if (sphereStaticInfo == NULL)
    {
        sphereStaticInfo = createSphereStaticInfo();
    }
    if (defaultmesh == NULL)
    {
        defaultmesh = meshLoad(TMP_DEFSHIP_PATH "DefaultShip.geo");
    }
    if (defaultlod == NULL)
    {
        defaultlod = lodTableReadScript(TMP_DEFSHIP_PATH, "DefaultShip.lod");
    }
    if (defaultmex == NULL)
    {
        defaultmex = mexLoad(TMP_DEFSHIP_PATH "DefaultShip.mex");
    }

#undef TMP_DEFSHIP_PATH

    max = 0;
    for (shiprace=0;shiprace<NUM_RACES;shiprace++)
    {
        max += (LastShipTypeOfRace[shiprace] - FirstShipTypeOfRace[shiprace]);
    }
    max += NUM_ASTEROIDTYPES + NUM_DUSTCLOUDTYPES + NUM_NEBULATYPES + NUM_DERELICTTYPES + NUM_RACES + 2;

    HorseRaceBeginBar(UNIVERSE_BAR);
    count = 0;
    for (shiprace=0;shiprace<NUM_RACES;shiprace++)
    {
        firstshiptype = FirstShipTypeOfRace[shiprace];
        lastshiptype = LastShipTypeOfRace[shiprace];
        for (shiptype=firstshiptype;shiptype<=lastshiptype;shiptype++)
        {
            HorseRaceNext(((real32)count)/((real32)max));
            if (hrAbortLoadingGame)
            {
                goto abortloadinggame;
            }
            count++;
            shipstaticinfo = GetShipStaticInfo(shiptype,shiprace);
            if (bitTest(shipstaticinfo->common.infoFlags, IF_InfoNeeded))
            {                                               //if this info needed
                if (!bitTest(shipstaticinfo->common.infoFlags, IF_InfoLoaded))
                {                                           //if this info not loaded
                    InitStatShipInfo(shipstaticinfo,shiptype,shiprace);
                    bitSet(shipstaticinfo->common.infoFlags, IF_NotPostLoaded);
#if UNIV_SHIP_LOADFREE_LOG
                    if (logFile)
                    {
                        fprintf(logFile, "Loading ship: %8s %s\n", ShipRaceToStr(shiprace), ShipTypeToStr(shiptype));
                    }
#endif
                }
                else
                {
                    if (shipstaticinfo->common.LOD != NULL)
                    {
                        lodAllMeshesRecolorize(shipstaticinfo->common.LOD);
                    }
                }
            }
            else
            {                                               //static info not needed this round
                if (bitTest(shipstaticinfo->common.infoFlags, IF_InfoLoaded))
                {                                           //if it's been loaded
                    CloseStatShipInfo(shipstaticinfo);
#if UNIV_SHIP_LOADFREE_LOG
                    if (logFile)
                    {
                        fprintf(logFile, "Freeing ship: %8s %s\n", ShipRaceToStr(shiprace), ShipTypeToStr(shiptype));
                    }
#endif
                }
            }

        }
    }

    for (asteroidtype=0;asteroidtype<NUM_ASTEROIDTYPES;asteroidtype++)
    {
        HorseRaceNext(((real32)count)/((real32)max));
        if (hrAbortLoadingGame)
        {
            goto abortloadinggame;
        }
        staticinfo = (StaticInfo *)&asteroidStaticInfos[asteroidtype];
        if (bitTest(staticinfo->infoFlags, IF_InfoNeeded))
        {                                                   //if info needed
            if (!bitTest(staticinfo->infoFlags, IF_InfoLoaded))
            {                                               //info needed and not loaded
                InitStatAsteroidInfo((AsteroidStaticInfo *)staticinfo,asteroidtype);
                bitSet(staticinfo->infoFlags, IF_InfoLoaded);
#if UNIV_SHIP_LOADFREE_LOG
                if (logFile)
                {
                    fprintf(logFile, "Loading asteroid: %s\n", AsteroidTypeToStr(asteroidtype));
                }
#endif
            }
        }
        else
        {                                                   //info not needed
            if (bitTest(staticinfo->infoFlags, IF_InfoLoaded))
            {                                               //info loaded
                CloseStatAsteroidInfo((AsteroidStaticInfo *)staticinfo);
#if UNIV_SHIP_LOADFREE_LOG
                if (logFile)
                {
                    fprintf(logFile, "Freeing asteroid: %s\n", AsteroidTypeToStr(asteroidtype));
                }
#endif
            }
        }
        count++;
    }

    for (dustcloudtype = 0; dustcloudtype < NUM_DUSTCLOUDTYPES; dustcloudtype++)
    {
        HorseRaceNext(((real32)count)/((real32)max));
        if (hrAbortLoadingGame)
        {
            goto abortloadinggame;
        }
        staticinfo = (StaticInfo *)&dustcloudStaticInfos[dustcloudtype];
        if (bitTest(staticinfo->infoFlags, IF_InfoNeeded))
        {                                                   //if info needed
            if (!bitTest(staticinfo->infoFlags, IF_InfoLoaded))
            {                                               //info needed and not loaded
                InitStatDustCloudInfo((DustCloudStaticInfo *)staticinfo, dustcloudtype);
                bitSet(staticinfo->infoFlags, IF_InfoLoaded);
#if UNIV_SHIP_LOADFREE_LOG
                if (logFile)
                {
                    fprintf(logFile, "Loading ship: %s\n", DustCloudTypeToStr(dustcloudtype));
                }
#endif
            }
        }
        else
        {                                                   //info not needed
            if (bitTest(staticinfo->infoFlags, IF_InfoLoaded))
            {                                               //info loaded
                CloseStatDustCloudInfo((DustCloudStaticInfo *)staticinfo);
#if UNIV_SHIP_LOADFREE_LOG
                if (logFile)
                {
                    fprintf(logFile, "Freeing dustcloud: %s\n", DustCloudTypeToStr(dustcloudtype));
                }
#endif
            }
        }
        count++;
    }

    for (nebulatype = 0; nebulatype < NUM_NEBULATYPES; nebulatype++)
    {
        HorseRaceNext(((real32)count)/((real32)max));
        if (hrAbortLoadingGame)
        {
            goto abortloadinggame;
        }
        staticinfo = (StaticInfo *)&nebulaStaticInfos[nebulatype];
        if (bitTest(staticinfo->infoFlags, IF_InfoNeeded))
        {                                                   //if info needed
            if (!bitTest(staticinfo->infoFlags, IF_InfoLoaded))
            {                                               //info needed and not loaded
                InitStatNebulaInfo((NebulaStaticInfo *)staticinfo, nebulatype);
                bitSet(staticinfo->infoFlags, IF_InfoLoaded);
#if UNIV_SHIP_LOADFREE_LOG
                if (logFile)
                {
                    fprintf(logFile, "Loading nebula: %s\n", NebulaTypeToStr(nebulatype));
                }
#endif
            }
        }
        // CAT: a nebula static no longer needed is not closed (universeStaticInit 0x0053d2a0 has no close branch for
        //      nebulae; HW1's CloseStatNebulaInfo did nothing either)
        count++;
    }

    for (derelicttype=0;derelicttype<NUM_DERELICTTYPES;derelicttype++)
    {
        HorseRaceNext(((real32)count)/((real32)max));
        if (hrAbortLoadingGame)
        {
            goto abortloadinggame;
        }
        staticinfo = (StaticInfo *)&derelictStaticInfos[derelicttype];
        if (bitTest(staticinfo->infoFlags, IF_InfoNeeded))
        {                                                   //if info needed
            if (!bitTest(staticinfo->infoFlags, IF_InfoLoaded))
            {                                               //info needed and not loaded
                InitStatDerelictInfo((DerelictStaticInfo *)staticinfo,derelicttype);
                bitSet(staticinfo->infoFlags, IF_InfoLoaded);
#if UNIV_SHIP_LOADFREE_LOG
                if (logFile)
                {
                    fprintf(logFile, "Loading derelict: %s\n", DerelictTypeToStr(derelicttype));
                }
#endif
            }
            else
            {
                if (staticinfo->LOD != NULL)
                {
                    lodAllMeshesRecolorize(staticinfo->LOD);
                }
            }
        }
        else
        {                                                   //info not needed
            if (bitTest(staticinfo->infoFlags, IF_InfoLoaded))
            {                                               //info loaded
                CloseStatDerelictInfo((DerelictStaticInfo *)staticinfo);
#if UNIV_SHIP_LOADFREE_LOG
                if (logFile)
                {
                    fprintf(logFile, "Freeing derelict: %s\n", DerelictTypeToStr(derelicttype));
                }
#endif
            }
        }
        count++;
    }

    for (shiprace=0;shiprace<NUM_RACES;shiprace++)
    {
        HorseRaceNext(((real32)count)/((real32)max));
        if (hrAbortLoadingGame)
        {
            goto abortloadinggame;
        }
        staticinfo = (StaticInfo *)&missileStaticInfos[shiprace];
        if (bitTest(staticinfo->infoFlags, IF_InfoNeeded))
        {                                                   //if info needed
            if (!bitTest(staticinfo->infoFlags, IF_InfoLoaded))
            {                                               //info needed and not loaded
                InitStatMissileInfo((MissileStaticInfo *)staticinfo,shiprace);
                bitSet(staticinfo->infoFlags, IF_InfoLoaded);
#if UNIV_SHIP_LOADFREE_LOG
                if (logFile)
                {
                    fprintf(logFile, "Loading missile: %8s\n", ShipRaceToStr(shiprace));
                }
#endif
            }
        }
        else
        {                                                   //info not needed
            if (bitTest(staticinfo->infoFlags, IF_InfoLoaded))
            {                                               //info loaded
                CloseStatMissileInfo((MissileStaticInfo *)staticinfo);
#if UNIV_SHIP_LOADFREE_LOG
                if (logFile)
                {
                    fprintf(logFile, "Freeing missile: %8s\n", ShipRaceToStr(shiprace));
                }
#endif
            }
        }
        count++;
    }

    maxMineCollSphereSize = 0.0f;
    // CAT: mines of all 7 races (HW1: R1 and R2), and the Derelict race's mine is loaded whether needed or not
    //      (universeStaticInit 0x0053d2a0 skips the not-needed branch for mineStaticInfos[6], 0x00b3b318)
    for (shiprace=0;shiprace<NUM_RACES;shiprace++)
    {
        HorseRaceNext(((real32)count)/((real32)max));
        if (hrAbortLoadingGame)
        {
            goto abortloadinggame;
        }
        staticinfo = (StaticInfo *)&mineStaticInfos[shiprace];
        if (bitTest(staticinfo->infoFlags, IF_InfoNeeded) || (shiprace == RACE_Derelict))
        {                                                   //if info needed
            if (!bitTest(staticinfo->infoFlags, IF_InfoLoaded))
            {                                               //info needed and not loaded
                InitStatMineInfo((MissileStaticInfo *)staticinfo,shiprace);
                bitSet(staticinfo->infoFlags, IF_InfoLoaded);
                if (mineStaticInfos[shiprace].common.staticCollInfo.collspheresize > maxMineCollSphereSize)
                {
                    maxMineCollSphereSize = mineStaticInfos[shiprace].common.staticCollInfo.collspheresize;
                }
#if UNIV_SHIP_LOADFREE_LOG
                if (logFile)
                {
                    fprintf(logFile, "Loading mine: %8s\n", ShipRaceToStr(shiprace));
                }
#endif
            }
        }
        else
        {                                                   //info not needed
            if (bitTest(staticinfo->infoFlags, IF_InfoLoaded))
            {                                               //info loaded
                CloseStatMineInfo((MissileStaticInfo *)staticinfo);
#if UNIV_SHIP_LOADFREE_LOG
                if (logFile)
                {
                    fprintf(logFile, "Freeing mine: %8s\n", ShipRaceToStr(shiprace));
                }
#endif
            }
        }
        count++;
    }

    maxMineCollSphereSize *= 2.0f;

//    InitStatEffectInfo(&effectStaticInfo);

abortloadinggame:

    for (shiprace=0;shiprace<NUM_RACES;shiprace++)
    {
        firstshiptype = FirstShipTypeOfRace[shiprace];
        lastshiptype = LastShipTypeOfRace[shiprace];
        for (shiptype=firstshiptype;shiptype<=lastshiptype;shiptype++)
        {
            shipstaticinfo = GetShipStaticInfo(shiptype,shiprace);
            if (bitTest(shipstaticinfo->common.infoFlags, IF_InfoNeeded))
            {
                if (bitTest(shipstaticinfo->common.infoFlags, IF_NotPostLoaded))
                {                                           //if ship just now loaded
                    InitStatShipInfoPost(shipstaticinfo);
                    //shipstaticinfo->common.infoFlags, IF_InfoLoaded) = TRUE;
                    bitClear(shipstaticinfo->common.infoFlags, IF_NotPostLoaded);
                    bitSet(shipstaticinfo->common.infoFlags, IF_InfoLoaded);
                }
            }
            // CAT: every loaded ship static gets its health for this game: the single-player difficulty factor for
            //      the Somtaaw (universeStaticInit 0x0053d2a0 inlines cat_univSetShipMaxHealth 0x0053c4a0)
            if (bitTest(shipstaticinfo->common.infoFlags, IF_InfoLoaded))
            {
                cat_univSetShipMaxHealth(shipstaticinfo);
            }
        }
    }


#if UNIV_SHIP_LOADFREE_LOG
    if (logFile)
    {
        fclose(logFile);
    }
#endif

    // CAT: no Mothership immobile step (universeStaticInit 0x0053d2a0 ends with the health loop; HW1 set the R1/R2
    //      Mothership statics immobile in single player)
}

/*-----------------------------------------------------------------------------
    Name        : cat_univSetShipMaxHealth
    Description : CAT: a ship static's maxhealth for this game: maxhealthBase, times the single-player difficulty's
                  health factor for the Somtaaw (race Sect) in a single-player game (cat_univSetShipMaxHealth
                  0x0053c4a0; called by universeStaticInit, FixShip and LoadGame)
    Inputs      : statinfo
    Outputs     : maxhealth, oneOverMaxHealth
    Return      :
----------------------------------------------------------------------------*/
void cat_univSetShipMaxHealth(ShipStaticInfo *statinfo)
{
    if (singlePlayerGame && (statinfo->shiprace == RACE_Sect))
    {
        statinfo->health.maxhealth = spDifficultyHealthModifier * statinfo->maxhealthBase;
    }
    else
    {
        statinfo->health.maxhealth = statinfo->maxhealthBase;
    }
    statinfo->health.oneOverMaxHealth = 1.0f / statinfo->health.maxhealth;
}

/*-----------------------------------------------------------------------------
    Name        : universeStaticInit
    Description : Reinitializes some static data:
                                                    *.Shp variables
    Inputs      : none
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/

void universeRealTimeTweak(SelectCommand *selection)
{
    ShipRace shiprace;
    ShipType shiptype;
    ShipStaticInfo *shipstaticinfo;
    udword shiparray[100];
    udword shiptypedone;
    udword shipdoneptr=0;
    udword index;
    sdword flag = FALSE;

    for(shiptypedone = 0;shiptypedone < 100; shiptypedone++)
    {
        shiparray[shiptypedone] = 99999;    //fill with garbage ships
    }
    for (index = 0;index < selection->numShips;index++)
    {
        shiprace = selection->ShipPtr[index]->staticinfo->shiprace;
        shiptype = selection->ShipPtr[index]->shiptype;

        for(shiptypedone = 0;shiptypedone <=shipdoneptr; shiptypedone++)
        {
            if(shiparray[shiptypedone] == shiptype)
            {
                //shiptype already loaded, so don't load it
                flag = TRUE;
                break;
            }
        }
        if(flag == TRUE)
        {
            flag = FALSE;
            continue;
        }

        shiparray[shipdoneptr] = shiptype;
        shipdoneptr++;
        shipstaticinfo = GetShipStaticInfo(shiptype,shiprace);
        InitStatShipInfo(shipstaticinfo,shiptype,shiprace);
    }
}
void universeSetZeroPlayers(void)
{
    universe.numPlayers = 0;
    cameraInit(&defaultCamera,5000.0f);
}

/*-----------------------------------------------------------------------------
    Name        : universeNoStaticLoadedOrNeeded
    Description : Marks all static static infos as not loaded and not needed
                  in fact, it sets the entire static infos to NULL
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void universeNoStaticLoadedOrNeeded(void)
{
    ShipRace shiprace;
    ShipType shiptype;
    ShipStaticInfo *shipStaticInfo;
    AsteroidStaticInfo *asteroidStaticInfo;
    DustCloudStaticInfo *dustcloudStaticInfo;
    GasCloudStaticInfo *gascloudStaticInfo;
    NebulaStaticInfo *nebulaStaticInfo;
    DerelictStaticInfo *derelictStaticInfo;
    MissileStaticInfo *missileStaticInfo;
    AsteroidType asteroidtype;
    DustCloudType dustcloudtype;
    GasCloudType gascloudtype;
    NebulaType nebulatype;
    DerelictType derelicttype;
    ShipType firstshiptype;
    ShipType lastshiptype;

    for (shiprace=0;shiprace<NUM_RACES;shiprace++)
    {
        firstshiptype = FirstShipTypeOfRace[shiprace];
        lastshiptype = LastShipTypeOfRace[shiprace];
        for (shiptype=firstshiptype;shiptype<=lastshiptype;shiptype++)
        {
            shipStaticInfo = GetShipStaticInfo(shiptype,shiprace);
            memset(shipStaticInfo,0,sizeof(ShipStaticInfo));
        }
    }

    for (asteroidtype=0;asteroidtype<NUM_ASTEROIDTYPES;asteroidtype++)
    {
        asteroidStaticInfo = &asteroidStaticInfos[asteroidtype];
        memset(asteroidStaticInfo,0,sizeof(*asteroidStaticInfo));
    }

    for (dustcloudtype = 0; dustcloudtype < NUM_DUSTCLOUDTYPES; dustcloudtype++)
    {
        dustcloudStaticInfo = &dustcloudStaticInfos[dustcloudtype];
        memset(dustcloudStaticInfo,0,sizeof(*dustcloudStaticInfo));
    }

    for (gascloudtype = 0; gascloudtype < NUM_GASCLOUDTYPES; gascloudtype++)
    {
        gascloudStaticInfo = &gascloudStaticInfos[gascloudtype];
        memset(gascloudStaticInfo,0,sizeof(*gascloudStaticInfo));
    }

    for (nebulatype = 0; nebulatype < NUM_NEBULATYPES; nebulatype++)
    {
        nebulaStaticInfo = &nebulaStaticInfos[nebulatype];
    }

    for (derelicttype=0;derelicttype<NUM_DERELICTTYPES;derelicttype++)
    {
        derelictStaticInfo = &derelictStaticInfos[derelicttype];
        memset(derelictStaticInfo,0,sizeof(*derelictStaticInfo));
    }

    for (shiprace=0;shiprace<NUM_RACES;shiprace++)
    {
        missileStaticInfo = &missileStaticInfos[shiprace];
        memset(missileStaticInfo,0,sizeof(*missileStaticInfo));
        // CAT: the mines of the first four races only, Sect..Taiidan (universeNoStaticLoadedOrNeeded 0x0053dd90
        //      clears mineStaticInfos at offsets 0, 300, 600, 900; HW1 R1 and R2)
        if (shiprace <= RACE_Taiidan)
        {
            missileStaticInfo = &mineStaticInfos[shiprace];
            memset(missileStaticInfo,0,sizeof(*missileStaticInfo));
        }
    }
}

void universeInit(void)
{
    sdword index;

    univupdateInit();

    clInit(&universe.mainCommandLayer);
    ccInit(&universe.mainCameraCommand);

    universeSetZeroPlayers();

    universeNoStaticLoadedOrNeeded();

    // CAT: every player's fog-of-war grid pointer starts NULL (universeInit 0x0053de80 clears Player+0x6500 of players 0..7)
    for (index = 0; index < MAX_MULTIPLAYER_PLAYERS; index++)
    {
        universe.players[index].catFOWGrid = NULL;
    }

    for (index = 0; index < UNIV_NUMBER_WORLDS; index++)
    {
        universe.world[index] = NULL;
    }
}

// CAT: 0x00480d10: the grid a player's ships are seen through; in multiplayer an ally of the local player shares the local
//      player's grid. The grids themselves are stage 3 (cat_fowCreatePlayerGrids 0x004a38c0), so this is NULL until then.
void *cat_playerGetFOWGrid(Player *player)
{
    if (!multiPlayerGame)
    {
        return player->catFOWGrid;
    }
    if (allianceArePlayersAllied(universe.curPlayerPtr, player))
    {
        return universe.curPlayerPtr->catFOWGrid;
    }
    return player->catFOWGrid;
}

void universeClosePlayers(void)
{
    udword i;

    for (i=0;i<universe.numPlayers;i++)
    {
        if (universe.players[i].aiPlayer != NULL)
        {
            aiplayerClose(universe.players[i].aiPlayer);
            universe.players[i].aiPlayer = NULL;
        }
        // CAT: the current player's research, construction and launch managers close here (universeClosePlayers
        //      0x0053def0; universeReset 0x0053df80 and universeClose 0x0053e3b0 inline the same loop)
        if (&universe.players[i] == universe.curPlayerPtr)
        {
            rmCloseIfOpen();
            cmCloseIfOpen();
            lmCloseIfOpen();
        }
        universe.players[i].PlayerMothership = NULL;
        // CAT: the player's upgrades are reset, cat_playerUpgradesReset 0x00514100, except for player 0 while a
        //      single-player warp-to-level is on (universeClosePlayers 0x0053def0, cat_spWarpToLevelEnabled 0x0051bd90)
        if ((i != 0) || (!cat_spWarpToLevelEnabled()))
        {
            cat_playerUpgradesReset(&universe.players[i]);
        }
    }
}

void universeReset(void)
{
    sdword index;

    // CAT (M2-R4 combat): universeReset 0x0053df80 first calls cat_salvageResetGlobals 0x004f7a90
    cat_salvageResetGlobals();
    // CAT (M2-R4 combat): then cat_blastReset 0x00516390 (the blast lists)
    cat_blastReset();
    // CAT: then each player's fog-of-war grid goes (universeReset 0x0053df80: fowDestroy on Player+0x6500 of players
    //      0..7). CAT-TODO(stage 3 FOW): fowDestroy; the pointer is NULL until stage 3 creates the grids.
    for (index = 0; index < MAX_MULTIPLAYER_PLAYERS; index++)
    {
        universe.players[index].catFOWGrid = NULL;
    }
    // CAT-TODO(stage 3): after clReset/ccReset the exe sets the multiplayer camera mode in a recorded-game playback
    //      (cat_mcSetMode/cat_mcResetRequest)
    clReset(&universe.mainCommandLayer);
    ccReset(&universe.mainCameraCommand);

    pingAllPingsDelete();

    cmReset(); // Free carrier factory pointers in construction manager

    universeClosePlayers();

    univupdateCloseAllObjectsAndMissionSpheres();
    // don't do univupdateReset() because it is done in gameStart

    for (index = 0; index < UNIV_NUMBER_WORLDS; index++)
    {
        universe.world[index] = NULL;
    }

    // CAT: the time compression goes back to the default (cat_taskTimeCompressionReset 0x0052a420, universeReset
    //      0x0053df80). CAT-TODO: cat_ioReset 0x004ab590 (info overlay), cat_shipPrintReset 0x00511500, four UI globals
    //      (0x00a762dc..0x00a762e4, 0x00a5b1ac) and cat_tutPointerListReset 0x0052f890 (tutorial pointers) around it;
    //      their subsystems are not ported yet (stage 3 UI, stage 6 tutorial)
    cat_taskTimeCompressionReset();
}

/*-----------------------------------------------------------------------------
    Name        : universeStaticCloseAllStatics
    Description : Closes all universe static structures for good.
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void universeStaticCloseAllStatics(void)
{
    ShipRace shiprace;
    ShipType shiptype;
    ShipType firstshiptype;
    ShipType lastshiptype;
    AsteroidType asteroidtype;
    DustCloudType dustcloudtype;
    GasCloudType gascloudtype;
    DerelictType derelicttype;
    //TODO(type safety, 2026-04-27): Replace with scoped variables of the appropriate type
    StaticInfo *staticInfo;

//    CloseStatEffectInfo(&effectStaticInfo);

    if (sphereStaticInfo != NULL)
    {
        freeSphereStaticInfo(sphereStaticInfo);
        sphereStaticInfo = NULL;
    }

    for (shiprace=0;shiprace<NUM_RACES;shiprace++)
    {
        staticInfo = (StaticInfo *)&missileStaticInfos[shiprace];
        if (bitTest(staticInfo->infoFlags, IF_InfoLoaded))
        {
            CloseStatMissileInfo((MissileStaticInfo *)staticInfo);
        }
    }

    for (shiprace=0;shiprace<2;shiprace++)        //only two races have mines
    {
        staticInfo = (StaticInfo *)&mineStaticInfos[shiprace];
        if (bitTest(staticInfo->infoFlags, IF_InfoLoaded))
        {
            CloseStatMineInfo((MissileStaticInfo *)staticInfo);
        }
    }

    for (derelicttype=0;derelicttype<NUM_DERELICTTYPES;derelicttype++)
    {
        staticInfo = (StaticInfo *)&derelictStaticInfos[derelicttype];
        if (bitTest(staticInfo->infoFlags, IF_InfoLoaded))
        {
            CloseStatDerelictInfo((DerelictStaticInfo *)staticInfo);
        }
    }

    for (asteroidtype=0;asteroidtype<NUM_ASTEROIDTYPES;asteroidtype++)
    {
        staticInfo = (StaticInfo *)&asteroidStaticInfos[asteroidtype];
        if (bitTest(staticInfo->infoFlags, IF_InfoLoaded))
        {
            CloseStatAsteroidInfo((AsteroidStaticInfo *)staticInfo);
        }
    }

    for (dustcloudtype = 0; dustcloudtype < NUM_DUSTCLOUDTYPES; dustcloudtype++)
    {
        staticInfo = (StaticInfo *)&dustcloudStaticInfos[dustcloudtype];
        if (bitTest(staticInfo->infoFlags, IF_InfoLoaded))
        {
            CloseStatDustCloudInfo((DustCloudStaticInfo *)staticInfo);
        }
    }

    for (gascloudtype = 0; gascloudtype < NUM_GASCLOUDTYPES; gascloudtype++)
    {
        staticInfo = (StaticInfo *)&gascloudStaticInfos[gascloudtype];
        if (bitTest(staticInfo->infoFlags, IF_InfoLoaded))
        {
            CloseStatGasCloudInfo((GasCloudStaticInfo *)staticInfo);
        }
    }

    for (shiprace=0;shiprace<NUM_RACES;shiprace++)
    {
        firstshiptype = FirstShipTypeOfRace[shiprace];
        lastshiptype = LastShipTypeOfRace[shiprace];
        for (shiptype=firstshiptype;shiptype<=lastshiptype;shiptype++)
        {
            staticInfo = (StaticInfo *)GetShipStaticInfo(shiptype,shiprace);
            if (bitTest(staticInfo->infoFlags, IF_InfoLoaded))
            {
                CloseStatShipInfo((ShipStaticInfo *)staticInfo);
            }
        }
    }

    if (defaultlod != NULL)
    {
        lodFree(defaultlod);
        defaultlod = NULL;
    }

    if (defaultmex != NULL)
    {
        mexFree(defaultmex);
        defaultmex = NULL;
    }

    if (defaultmesh != NULL)
    {
        meshFree(defaultmesh);
        defaultmesh = NULL;
    }
}

void universeClose(void)
{
    // CAT: universeClose 0x0053e3b0 starts with the time-compression reset (cat_taskTimeCompressionReset 0x0052a420)
    cat_taskTimeCompressionReset();
    clClose(&universe.mainCommandLayer);
    ccClose(&universe.mainCameraCommand);

    universeClosePlayers();

    univupdateClose();

    universeStaticCloseAllStatics();
}

/*-----------------------------------------------------------------------------
    Name        : universeStaticClose
    Description : Closes static data of the Universe
    Inputs      : none
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void universeStaticClose(void)
{
}

/*-----------------------------------------------------------------------------
    Name        : universeUpdateTask
    Description : the main loop of the task which updates the universe (all
                  mission spheres) and executes all of the physics stuff
                  and moves all of the ships.  This task runs independent of
                  the screen refresh rate.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
DEFINE_TASK(universeUpdateTask)
{
    static WaitPacketStatus waitpacketstatus;
    static sdword repeatuniv;
    static sdword repeattimes;

    taskBegin;

    taskYield(0);

    for(;;)
    {
        universePlaybackNoPacket = FALSE;       // CAT: cleared every pass (universeUpdateTask 0x0053e460, 0x00a6c5c8)

        if ((multiPlayerGame) && (startingGame) && (gameIsRunning) && ((IAmCaptain) && (!multiPlayerGameUnderWay)))
        {
            if (checkPlayersReady())
            {
                multiPlayerGameUnderWay = TRUE;
            }
        }

        if (startingGameState == CHECK_AUTODOWNLOAD_MAP)
        {
            universeUpdateCounter++;

            if (!LANGame) chatClose();
            if (sigsPlayerIndex == 0)
                autodownloadmapGotMapName(spScenarios[spCurrentSelected].fileSpec,numPlayers,spScenarios[spCurrentSelected].minplayers,spScenarios[spCurrentSelected].maxplayers);
            else
                autodownloadmapGotMapName(tpGameCreated.MapName,numPlayers,0,0);
            horseRaceInit();
            if (autodownloadmapRequired())
            {
                HorseRaceBeginBar(DOWNLOADMAP_BAR);

                if (sigsPlayerIndex == 0)
                {
                    autodownloadmapGetFilesOfMap();
                }

                autodownloadmapPrintStatusStart();
                startingGameState = AUTODOWNLOAD_MAP;
            }
            else
            {
                startingGameState = PROCESS_TEXTURES;
                goto processtextures;
            }
        }
        else if (startingGameState == AUTODOWNLOAD_MAP)
        {
repeatagainwithoutyielding:         // for horse race hack we need this silly hack
            universeUpdateCounter++;

            if (sigsPlayerIndex == 0)
            {
                // for now, lets just bang everything through:
                if (autodownloadmapSendAFile())
                {
                    HorseRaceNext(1.0f);
                    startingGameState = PROCESS_TEXTURES;
                    goto processtextures;
                }
                else
                {
                    HorseRaceNext(autodownloadmapPercentSentFiles());
                    // don't check for aborting - will mess up everyone since files must be sent!
                }
            }
            else
            {
                if (autodownloadmapReceivedAllFiles())
                {
                    HorseRaceNext(1.0f);
                    startingGameState = PROCESS_TEXTURES;
                    goto processtextures;
                }
                else
                {
                    HorseRaceNext(autodownloadmapPercentReceivedFiles());
                    if (hrAbortLoadingGame)
                    {
                        startingGameState = PROCESS_TEXTURES;
                        goto processtextures;
                    }
                }
            }

            goto repeatagainwithoutyielding;
        }
        else if (startingGameState == PROCESS_TEXTURES)
        {
            universeUpdateCounter++;
processtextures:
            if (!hrAbortLoadingGame)
            {
                gameStart(NULL);
            }
            horseRaceShutdown();

            startingGameState = GAME_STARTED;

            if (hrAbortLoadingGame)
            {
                utyGameQuitToMain(NULL,NULL);
                hrAbortLoadingGame = FALSE;
            }
            else if (multiPlayerGame)
            {
                TransferCaptaincyGameStarted();
            }

        }
        else if (gameIsRunning)
        {
            universeUpdateCounter++;

            if ((multiPlayerGame) || (playPackets|recordFakeSendPackets))
            {
                if (!universe.dontUpdateRenderList)
                {
                    if ((universe.univUpdateCounter == 1) || ((universeUpdateCounter & REFRESH_RENDERLIST_RATE) == REFRESH_RENDERLIST_FRAME))
                    {
                        univUpdateRenderList();
                    }

                    if ((universe.univUpdateCounter == 1) || ((universeUpdateCounter & REFRESH_MINORRENDERLIST_RATE) == REFRESH_MINORRENDERLIST_FRAME))        // make it 3 to mix things up a bit
                    {
                        univUpdateMinorRenderList();
                    }
                }

                if (!(playPackets|recordFakeSendPackets))
                {
                    if ((!multiPlayerGameUnderWay) && (!IAmCaptain))
                    {
                        if (!HaveSentNonCaptainReadyPacket)
                        {
                            SendNonCaptainReadyPacket(sigsPlayerIndex);
                            HaveSentNonCaptainReadyPacket = TRUE;
                        }
                    }
                }

                if (multiPlayerGame)
                {
                    gcPollForNewChat();
                    TransferCaptaincyUpdate();
                    KeepAliveUpdate();

                    if ((universeUpdateCounter & INTERNET_LAG_UPDATERATE) == 0)
                    {
                        lagUpdateInternetLag();
                    }
                }

                if (playPackets|recordFakeSendPackets)
                {
                    if (universePause)
                    {
                        // CAT: consume the paused real time (universeUpdateTask 0x0053e460 calls
                        //      cat_univRealTimeDelta 0x0054c360 in the paused branch, so realtimeelapsed does not jump)
                        cat_univRealTimeDelta();
                        goto alldone;
                    }
                }

                if ((playPackets|recordFakeSendPackets) && universeTurbo)
                {
                    // CAT: a fixed 9 updates per pass (universeUpdateTask 0x0053e460: mov repeattimes,9), Relic HW1's
                    //      UNIVERSE_TURBO_REPEAT 19 >> 1 (docs/m2-checksum-oracle.md); GoK's
                    //      (turboTimeCompressionFactor-1)>>1 gave 3, and the exe has no TimeCompressionFactor key
                    repeattimes = 9;
                }
                else
                {
                    repeattimes = 1;
                }


                for (repeatuniv=0;repeatuniv<repeattimes;repeatuniv++)
                {
                    waitpacketstatus = clWaitSyncPacket(&universe.mainCommandLayer);
                    if (waitpacketstatus == NO_PACKET)
                    {
                        // CAT: a playback that ran out of packets this pass is flagged for the overlay
                        //      (universeUpdateTask 0x0053e460; read by cat_moRender 0x004aafb0)
                        if (playPackets)
                        {
                            universePlaybackNoPacket = TRUE;
                        }
                    }
                    else
                    {
                        if (multiPlayerGame)
                        {
                            multiPlayerGameUnderWay = TRUE;
                            TransferCaptaincySyncPacketReceivedNotify();
                        }

                        netCheck();

                        if (ORACLE_UNIV_UPDATE(UNIVERSE_UPDATE_PERIOD))
                        {
                            goto alldone;
                        }
                        if (! ((waitpacketstatus == TOO_MANY_PACKETS) ||        // if TOO_MANY_PACKETS, don't yield, and have throughput at twice normal to catch up
                              (repeattimes > 1)) )
                        {
                            taskYield(0);
                        }
                        if (!gameIsRunning)
                        {
                            goto alldone;
                        }
                        if (ORACLE_UNIV_UPDATE(UNIVERSE_UPDATE_PERIOD))
                        {
                            goto alldone;
                        }
                    }
                }
    alldone:;

            }
            else
            {
                if (!universe.dontUpdateRenderList)
                {
                    if ((universe.univUpdateCounter == 1) || ((universeUpdateCounter & REFRESH_RENDERLIST_RATE) == REFRESH_RENDERLIST_FRAME))
                    {
                        univUpdateRenderList();
                    }

                    if ((universe.univUpdateCounter == 1) || ((universeUpdateCounter & REFRESH_MINORRENDERLIST_RATE) == REFRESH_MINORRENDERLIST_FRAME))        // make it 3 to mix things up a bit
                    {
                        univUpdateMinorRenderList();
                    }
                }

                if (universePause)
                {
                    cat_univRealTimeDelta();        // CAT: as above (universeUpdateTask 0x0053e460, free-running branch)
                    goto alldone2;
                }

                if((tutorial==TUTORIAL_ONLY) && !tutEnable.bGameRunning)
                {
                    tutEnable.bKASFrame ^= 1;
                    if(tutEnable.bKASFrame)
                        kasExecute();       // Make sure KAS gets called

                    goto alldone2;
                }

                // CAT: one update per pass, no universeTurbo repeat (universeUpdateTask 0x0053e460 calls univUpdate once
                //      here; Cataclysm's time compression changes the task rate instead, cat_taskTimeCompressionApply
                //      0x0052a370, not ported yet)
                ORACLE_UNIV_UPDATE(UNIVERSE_UPDATE_PERIOD);
alldone2:;
#ifdef HW_BUILD_FOR_DEBUGGING
                if (ShowFancyFights) statsShowFancyFightUpdate();
#endif

            }
        }
        else if (!startingGame)
        {
            if (sigsPressedStartGame)
            {
                utyNewGameStart(NULL, NULL);
            }
        }

        if (startRecordingGameWhenSafe)
        {
            recPackInGameStartCBSafeToStart();
            startRecordingGameWhenSafe = FALSE;
        }

        taskYield(0);
    }

    taskEnd;
}

/*-----------------------------------------------------------------------------
    Name        : universeSwitchToNextPlayer
    Description : switches current player to the next player (wraps around)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void universeSwitchToNextPlayer(void)
{
    universe.curPlayerIndex++;
    if (universe.curPlayerIndex >= universe.numPlayers)
    {
        universe.curPlayerIndex = 0;
    }
    universe.curPlayerPtr = &universe.players[universe.curPlayerIndex];

    universe.collUpdateAllBlobs = TRUE;                              //next update will update all blobs

    dbgMessagef("Current player: %d %x",universe.curPlayerIndex,universe.curPlayerPtr);
}

/*-----------------------------------------------------------------------------
    Name        : universeSwitchToPlayer
    Description : switches current player to player indicated by playerIndex
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void universeSwitchToPlayer(uword playerIndex)
{
    dbgAssertOrIgnore(playerIndex < universe.numPlayers);
    universe.curPlayerIndex = playerIndex;
    universe.curPlayerPtr = &universe.players[universe.curPlayerIndex];
    // CAT-TODO(stage 3 FOW): universeSwitchToPlayer 0x0053ea10 also selects the player's fog-of-war map
    //      (fowSelect(Player+0x6500))
}

/*-----------------------------------------------------------------------------
    Name        : GetShipStaticInfo
    Description : returns ShipStaticInfo of shiptype, shiprace
    Inputs      : shiptype, shiprace
    Outputs     :
    Return      : returns ShipStaticInfo of shiptype, shiprace
----------------------------------------------------------------------------*/
ShipStaticInfo *GetShipStaticInfo(ShipType shiptype,ShipRace shiprace)
{
    // CAT: one flat array indexed by type; the race no longer selects a table (GetShipStaticInfo 0x0053ea50). The
    //      race parameter stays until stage 2 changes the callers (the exe's function takes the type only).
    (void)shiprace;
    dbgAssertOrIgnore(shiptype < TOTAL_NUM_SHIPS);

    return &ShipStaticInfos[shiptype];
}

/*-----------------------------------------------------------------------------
    Name        : GetShipStaticInfoSafe
    Description : returns ShipStaticInfo of shiptype, shiprace, NULL if not valid
    Inputs      : shiptype, shiprace
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
ShipStaticInfo *GetShipStaticInfoSafe(ShipType shiptype,ShipRace shiprace)
{
    ShipStaticInfo *shipstatic;

    // CAT: type range and the loaded flag only, no race check (GetShipStaticInfoSafe 0x0053ea70: type <= 0x90 and
    //      (infoFlags & IF_InfoLoaded))
    // [audit: the exe's range test is unsigned (cmp ecx,0x91; jb), so a negative type such as
    //      ShipType_Uninitialized returns NULL, as HW1's offset < 0 test did; ShipType is signed here]
    (void)shiprace;
    if (((udword)shiptype >= TOTAL_NUM_SHIPS))
    {
        return NULL;
    }

    shipstatic = &ShipStaticInfos[shiptype];
    if (!bitTest(shipstatic->common.infoFlags, IF_InfoLoaded))
    {
        return NULL;
    }

    return shipstatic;
}

/*-----------------------------------------------------------------------------
    Name        : GetValidRaceForShipType
    Description : returns valid race for shiptype, -1 if none found
    Inputs      : shiptype
    Outputs     :
    Return      : returns valid race for shiptype, -1 if none found
----------------------------------------------------------------------------*/
ShipRace GetValidRaceForShipType(ShipType shiptype)
{
    ShipRace race;

    dbgAssertOrIgnore(shiptype < TOTAL_NUM_SHIPS);

    // CAT: first race allowed by RacesAllowedForGivenShip (0x008b1814), over all 7 Cataclysm races. HW1's version
    //      tested its 6 races one by one; no exe counterpart was found (the exe derives a type's race from
    //      LastShipTypeOfRace, cat_ShipTypeToRace 0x004d7170), so this keeps HW1's rule on the new races.
    for (race = 0; race < NUM_RACES; race++)
    {
        if (RacesAllowedForGivenShip[shiptype] & RaceToRaceBits(race))
        {
            return race;
        }
    }

    dbgAssertOrIgnore(FALSE);
    return RACE_Sect;       // CAT: HW1's R1 alias retired (same id 0)
}

/*-----------------------------------------------------------------------------
    Name        : GetShipStaticInfoValidate
    Description : returns ShipStaticInfo of shiptype, shiprace.  Makes sure data loaded
    Inputs      : shiptype, shiprace
    Outputs     :
    Return      : returns ShipStaticInfo of shiptype, shiprace
----------------------------------------------------------------------------*/
ShipStaticInfo *GetShipStaticInfoValidate(ShipType shiptype,ShipRace shiprace)
{
    ShipStaticInfo *shipstatic = GetShipStaticInfo(shiptype,shiprace);
    if (!bitTest(shipstatic->common.infoFlags, IF_InfoLoaded))
    {
        dbgFatalf(DBG_Loc,"Ship %s %s not loaded",ShipRaceToStr(shiprace),ShipTypeToStr(shiptype));
    }
    return shipstatic;
}

/*  Unit Capping functions */

/*-----------------------------------------------------------------------------
    Name        : unitCapCreateShip
    Description : Updates ship totals for player
    Inputs      : uword shiptype, Player *player
    Outputs     : none
    Return      : void
-----------------------------------------------------------------------------*/
void unitCapCreateShip(Ship *ship, Player *player)
{
    CatModuleTypeList modules;
    sdword i;

    // CAT: no exempt types (HW1: Drone, TargetDrone, HeadShotAsteroid, CryoTray), and a mothership brings its modules
    //      into the caps: each module type counts as a ship of its type and of CLASS_Component (Player+0x29c =
    //      classtotals[10]; unitCapCreateShip 0x0053eab0; the exe inlines cat_univBeastMothershipModuleTypesGet for
    //      bMothership)
    if (!cdEnabled)
    {
        return;
    }
    if (bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_InUnitCaps))   // CAT: specialFlags in the exe
    {
        return;
    }
    player->totalships++;
    player->shiptotals[ship->shiptype]++;
    player->classtotals[ship->staticinfo->shipclass]++;
    bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_InUnitCaps);   // CAT: specialFlags in the exe

    if (ship->shiptype == sMothership)
    {
        cat_univMothershipModuleTypesGet(&modules);
        for (i = 0; i < modules.num; i++)
        {
            player->totalships++;
            player->shiptotals[modules.types[i]]++;
            player->classtotals[CLASS_Component]++;
        }
    }
    if (ship->shiptype != bMothership)
    {
        return;
    }
    cat_univBeastMothershipModuleTypesGet(&modules);
    for (i = 0; i < modules.num; i++)
    {
        player->totalships++;
        player->shiptotals[modules.types[i]]++;
        player->classtotals[CLASS_Component]++;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_univFindNearestShip
    Description : CAT: the ship of universe.ShipList nearest to an object (squared distance, strictly below 1e20; the
                  first of equal distances wins), among the ships the filter accepts (all if NULL)
                  (cat_univFindNearestShip 0x0053f590; called by cat_beastWeaponHouseKeep 0x0042a900, M2-R4)
    Inputs      : from, filter
    Outputs     :
    Return      : the ship or NULL
----------------------------------------------------------------------------*/
Ship *cat_univFindNearestShip(SpaceObj *from, bool32 (*filter)(Ship *ship))
{
    Node *node;
    Ship *ship;
    Ship *nearest = NULL;
    real32 nearestDistSqr = 1e20f;
    real32 dx, dy, dz, distSqr;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((filter != NULL) && !filter(ship))
        {
            continue;
        }
        dx = ship->obj.posinfo.position.x - from->posinfo.position.x;
        dy = ship->obj.posinfo.position.y - from->posinfo.position.y;
        dz = ship->obj.posinfo.position.z - from->posinfo.position.z;
        distSqr = dx * dx + dy * dy + dz * dz;
        if (distSqr < nearestDistSqr)
        {
            nearestDistSqr = distSqr;
            nearest = ship;
        }
    }
    return nearest;
}

/*-----------------------------------------------------------------------------
    Name        : cat_playerHasCompletedShip
    Description : CAT: TRUE when player owns a ship of the given type that is not still being built externally
                  (cat_playerHasCompletedShip 0x0053f620; the exe tests the player's per-type count first)
    Inputs      : type, player
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 cat_playerHasCompletedShip(ShipType type, Player *player)
{
    Node *node;
    Ship *ship;

    if (player->shiptotals[type] == 0)
    {
        return FALSE;
    }
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->staticinfo->shiptype == type) && (ship->playerowner == player) &&
            !bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
        {
            return TRUE;
        }
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_univMothershipModuleTypesGet
    Description : CAT: the module types a Somtaaw mothership starts with (cat_univMothershipModuleTypesGet 0x0053f680):
                  the tutorial, single-player missions 1-2 and 3, and the multiplayer maps DemoBigGun2 and DemoSect2
                  (case-insensitive) have their own lists; every other game gets EnginesUpper, Command, Engineering
    Inputs      :
    Outputs     : modules
    Return      :
----------------------------------------------------------------------------*/
void cat_univMothershipModuleTypesGet(CatModuleTypeList *modules)
{
    static const ShipType tutorialModules[] = { sMothershipEnginesLower, sMothershipEnginesUpper, sMothershipCommand,
        sMothershipLowerSpine, sMothershipOreCanArmour, sMothershipEngineering, sMothershipDockingBay };
    static const ShipType mission12Modules[] = { sMothershipEnginesLower, sMothershipEnginesUpper, sMothershipCommand,
        sMothershipLowerSpine, sMothershipOreCanArmour, sMothershipEngineering };
    static const ShipType mission3Modules[] = { sMothershipEnginesLower, sMothershipEnginesUpper, sMothershipCommand,
        sMothershipLowerSpine, sMothershipEngineering };
    static const ShipType demoBigGunModules[] = { sMothershipEnginesLower, sMothershipEnginesUpper, sMothershipCommand,
        sMothershipEngineering, sMothershipSpecial, sMothershipBigGun, sMothershipLowerSpine };
    // CAT-FIX: for DemoSect2 the exe writes these 11 types but sets the count to 12 (mov [list+0x50],0xc at
    //      0x0053f81c), so its callers (unitCapCreateShip, cat_spDeleteLeftBehindShips, cat_univCreateMothershipModules)
    //      read a 12th, never-written entry of their uninitialised stack array; the port counts the 11 written ones
    //      [audit: the divergence was undocumented]
    static const ShipType demoSectModules[] = { sMothershipEnginesLower, sMothershipEnginesUpper, sMothershipCommand,
        sMothershipEngineering, sMothershipSpecial, sMothershipBigGun, sMothershipLowerSpine, sMothershipArmour,
        sMothershipMicro, sMothershipWeapons, sMothershipDockingBay };
    static const ShipType defaultModules[] = { sMothershipEnginesUpper, sMothershipCommand, sMothershipEngineering };
    const ShipType *list = defaultModules;
    sdword num = sizeof(defaultModules) / sizeof(ShipType);
    sdword i;

    if (tutorial == 1)
    {
        list = tutorialModules; num = sizeof(tutorialModules) / sizeof(ShipType);
    }
    else if (singlePlayerGame == 1)
    {
        if ((singlePlayerGameInfo.currentMission == 1) || (singlePlayerGameInfo.currentMission == 2))
        {
            list = mission12Modules; num = sizeof(mission12Modules) / sizeof(ShipType);
        }
        else if (singlePlayerGameInfo.currentMission == 3)
        {
            list = mission3Modules; num = sizeof(mission3Modules) / sizeof(ShipType);
        }
    }
    else if (strcasecmp(spScenarios[spCurrentSelected].fileSpec, "DemoBigGun2") == 0)
    {
        list = demoBigGunModules; num = sizeof(demoBigGunModules) / sizeof(ShipType);
    }
    else if (strcasecmp(spScenarios[spCurrentSelected].fileSpec, "DemoSect2") == 0)
    {
        list = demoSectModules; num = sizeof(demoSectModules) / sizeof(ShipType);
    }

    for (i = 0; i < num; i++)
    {
        modules->types[i] = list[i];
    }
    modules->num = num;
}

/*-----------------------------------------------------------------------------
    Name        : cat_univBeastMothershipModuleTypesGet
    Description : CAT: the module types a Beast mothership starts with (cat_univBeastMothershipModuleTypesGet
                  0x0053f840): none in single-player missions 1-3, otherwise its docking bay and lower engines. In
                  multiplayer the exe compares the map name with "DemoBeastAttack2" and ignores the result.
    Inputs      :
    Outputs     : modules
    Return      :
----------------------------------------------------------------------------*/
void cat_univBeastMothershipModuleTypesGet(CatModuleTypeList *modules)
{
    if ((singlePlayerGame == 1) &&
        ((singlePlayerGameInfo.currentMission == 1) || (singlePlayerGameInfo.currentMission == 2) ||
         (singlePlayerGameInfo.currentMission == 3)))
    {
        modules->num = 0;
        return;
    }
    modules->types[0] = bMothershipDockingBay;
    modules->types[1] = bMothershipEnginesLower;
    modules->num = 2;
}

/*-----------------------------------------------------------------------------
    Name        : unitCapDeleteShip
    Description : Updates ship totals for player
    Inputs      : uword shiptype, Player *player
    Outputs     : none
    Return      : void
-----------------------------------------------------------------------------*/
void unitCapDeleteShip(Ship *ship, Player *player)
{

    // don't keep tabs on these ship types
//    if (player->aiPlayer)
//    {
//        return;
//    }
    // CAT: no exempt types (unitCapDeleteShip 0x0053ec00; HW1 skipped Drone, TargetDrone, HeadShotAsteroid, CryoTray)
    if ((cdEnabled) &&
        ( (bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_InUnitCaps)) ))   // CAT: specialFlags in the exe
    {
        player->totalships--;

//        dbgAssertOrIgnore(player->totalships >= 0);
        if (player->totalships < 0)
        {
            player->totalships = 0;
            dbgMessagef("WARNING: total shipcount in unit caps went below 0");
        }

        player->shiptotals[ship->shiptype]--;

//        dbgAssertOrIgnore(player->shiptotals[ship->shiptype] >= 0);
        if (player->shiptotals[ship->shiptype] < 0)
        {
            player->shiptotals[ship->shiptype] = 0;
            dbgMessagef("WARNING: shipcount of shiptype %d went below 0");
        }

        player->classtotals[ship->staticinfo->shipclass]--;

//        dbgAssertOrIgnore(player->classtotals[ship->staticinfo->shipclass] >= 0);
        if (player->classtotals[ship->staticinfo->shipclass] < 0)
        {
            player->classtotals[ship->staticinfo->shipclass] = 0;
            dbgMessagef("WARNING: classtotals of class %d went below 0",ship->staticinfo->shipclass);
        }
        bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_InUnitCaps);   // CAT: specialFlags in the exe
    }
}

/*-----------------------------------------------------------------------------
    Name        : unitCapCanCreateShip
    Description : Can we build this ship ??
    Inputs      : uword shiptype, Player *player
    Outputs     : TRUE if ship is within limit caps
                  FLASE if ship will exceed limit caps
    Return      : bool8
-----------------------------------------------------------------------------*/
bool32 unitCapCanCreateShip(ShipType ship, shipsinprogress *factory, shipavailable *cmShipsAvail)
{
    Player *player=factory->ship->playerowner;
    shipsinprogress *allfactories=(shipsinprogress *)factory->node.belongto->head;
    udword   totalships=1, typeships=1, classships=1, carrierTypeShips=0, index, count;
    sdword   jobs, lag;
    ShipStaticInfo *statinfo, *shipstatic;

    // CAT (M2-R4 rest-oracle): the body follows unitCapCanCreateShip 0x0053ece0. The caps apply with the unit-caps
    //      game option or in single player (no HW1 ResearchShip exception); the queue counts ships only (itemtype 1);
    //      the factory loop counts the player's race's types and, for an externally built type, one job less when its
    //      ship already exists (shipinprogress +0x28); the lag total of the type (shiplagtotals, ConsMgr.c) is added to
    //      each limit test once; sCarrierSupport's limit counts the carrier's attached support modules
    //      (cat_carrierCountSupportModules) plus the factory's own queue; the class cap is skipped when it is -1
    if ((!bitTest(tpGameCreated.flag,MG_UnitCapsEnabled)) && (!singlePlayerGame))
    {
        return(TRUE);
    }

    shipstatic = GetShipStaticInfo(ship, player->race);

    // total up all ships in the queue but not yet started
    for (index=0;cmShipsAvail[index].nJobs != -1; index++)
    {
        if (cmShipsAvail[index].itemtype == 1)      // ITEM_SHIP (ConsMgr.c)
        {
            statinfo = GetShipStaticInfo(cmShipsAvail[index].type, player->race);
            totalships+=cmShipsAvail[index].nJobs;
            if (cmShipsAvail[index].type == shipstatic->shiptype) typeships+=cmShipsAvail[index].nJobs;
            if (statinfo->shipclass == shipstatic->shipclass) classships+=cmShipsAvail[index].nJobs;
        }
    }
    if (shipstatic->shiptype == sCarrierSupport)
    {
        carrierTypeShips = typeships;
    }

    // walk through list of factories
    do
    {
        // if factory is owned by player then total all ships being built
        if (allfactories->ship->playerowner==player)
        {
            for (index=FirstShipTypeOfRace[player->race];index<=(udword)LastShipTypeOfRace[player->race];index++)
            {
                statinfo = GetShipStaticInfo(index, player->race);
                jobs = allfactories->progress[index].nJobs;
                totalships+=jobs;
                if (index==shipstatic->shiptype)
                {
                    typeships+=jobs;
                    if ((shipstatic->externalBuild) && (jobs > 0) && (allfactories->progress[index].catExternalShipStarted) &&
                        (typeships != 0))
                    {
                        typeships--;
                    }
                }
                if (statinfo->shipclass == shipstatic->shipclass) classships+=jobs;
            }
        }
        // next factory
        allfactories=(shipsinprogress *)allfactories->node.next;
    }
    while (allfactories != NULL);

    lag = shiplagtotals[shipstatic->shiptype];
    if ((udword)(player->totalships+lag+totalships) > (udword)cdMaxShipsAllowed)
    {
        speechEventQueue(NULL, 0x2001039, -1, -1, 0, player->playerIndex, -1, 5.0f, -1);   // total limit
        // player already has too many ships cannot build anymore
        return(FALSE);
    }

    if (cdLimitCaps[shipstatic->shiptype] != -1)
    {
        if (shipstatic->shiptype == sCarrierSupport)
        {
            count = cat_carrierCountSupportModules(factory->ship) + factory->progress[shipstatic->shiptype].nJobs + carrierTypeShips;
        }
        else
        {
            count = player->shiptotals[shipstatic->shiptype]+typeships+lag;
        }
        if (count > (udword)cdLimitCaps[shipstatic->shiptype])
        {
            speechEventQueue(NULL, 0x2001038, shipstatic->shiptype, -1, 0, player->playerIndex, -1, 5.0f, -1);   // unit limit
            // player has to many of this type of ship
            return(FALSE);
        }
    }

    if ((cdClassCaps[shipstatic->shipclass] != -1) &&
        ((udword)cdClassCaps[shipstatic->shipclass] < (udword)(player->classtotals[shipstatic->shipclass]+classships+lag)))
    {
        speechEventQueue(NULL, 0x2001038, shipstatic->shiptype, -1, 0, player->playerIndex, -1, 5.0f, -1);
        // player has to many of this class of ship
        return(FALSE);
    }

    return(TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : unitCapInitialize
    Description : Initialize the unit totals to zero
    Inputs      : Player *player
    Outputs     : none
    Return      : void
-----------------------------------------------------------------------------*/
void unitCapInitialize(Player *player)
{
    player->totalships=0;
    memset(player->shiptotals, 0,sizeof(sdword)*TOTAL_NUM_SHIPS);
    memset(player->classtotals,0,sizeof(sdword)*NUM_CLASSES);
}

/*-----------------------------------------------------------------------------
    Name        : unitCapEnable
    Description : Enables unit cap tracking
    Inputs      : none
    Outputs     : none
    Return      : void
----------------------------------------------------------------------------*/
void unitCapEnable(void)
{
    cdEnabled = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : unitCapDisable
    Description : disables unit cap tracking
    Inputs      : none
    Outputs     : none
    Return      : void
----------------------------------------------------------------------------*/
void unitCapDisable(void)
{
    cdEnabled = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : resourceInjectionTask
    Description : this is setup to be called at a specified time if it was enabled.
    Inputs      :
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void unitCapInitStatics(udword numplayers)
{
    char temp[128];

    sprintf(temp,"UnitLimitCaps%i.script", numplayers);
    scriptSet(NULL,temp,ShipStaticCapLimits);
}

/*-----------------------------------------------------------------------------
    Name        : resourceInjectionTask
    Description : this is setup to be called at a specified time if it was enabled.
    Inputs      :
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void resourceInjectionTask(void)
{
    sdword index;

    for (index=0;index<universe.numPlayers;index++)
    {
        if (universe.players[index].playerState != PLAYER_DEAD)
        {
            universe.players[index].resourceUnits += tpGameCreated.resourceInjectionsAmount;
            universe.gameStats.playerStats[index].totalResourceUnits += tpGameCreated.resourceInjectionsAmount;
            universe.gameStats.playerStats[index].totalInjectedResources += tpGameCreated.resourceInjectionsAmount;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : resourceLumpSum
    Description : this will give each player a lump sum amount of resources, once during the game
    Inputs      :
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void resourceLumpSum(void)
{
    sdword index;

    for (index=0;index<universe.numPlayers;index++)
    {
        if (universe.players[index].playerState != PLAYER_DEAD)
        {
            universe.players[index].resourceUnits += tpGameCreated.resourceLumpSumAmount;
            universe.gameStats.playerStats[index].totalResourceUnits += tpGameCreated.resourceLumpSumAmount;
            universe.gameStats.playerStats[index].totalInjectedResources += tpGameCreated.resourceLumpSumAmount;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : initPlayerStats
    Description : zeros the players inital multiplayer statistics
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/

void initGameStats(Universe *universe)
{
    sdword j,i;

    universe->gameStats.totalResourcesInGame = 0;
    universe->gameStats.totalShipsInGame = 0;
    universe->gameStats.totalKills = 0;
    universe->gameStats.totalRUsInAllShips = 0;
    universe->gameStats.totalRUsKilled = 0;
    universe->gameStats.totalResourceUnitsCollected = 0;
    universe->gameStats.totalRUsLost = 0;
    universe->gameStats.updatedRUValuesTime = 0.0f;
    universe->gameStats.universeRUWorth = 0;


    for(i=0;i<universe->numPlayers;i++)
    {
        universe->gameStats.playerStats[i].timeOfDeath = 0;
        universe->gameStats.playerStats[i].typeOfPlayerDeath = 0;
        universe->gameStats.playerStats[i].totalResourceUnits = 0;
        universe->gameStats.playerStats[i].totalResourceUnitsCollected = 0;                 //[]
        universe->gameStats.playerStats[i].totalResourceUnitsSpent = 0;                     //[]
        universe->gameStats.playerStats[i].totalResourceUnitsGiven = 0;                     //[]
        universe->gameStats.playerStats[i].totalResourceUnitsRecieved = 0;
        universe->gameStats.playerStats[i].totalRegeneratedResourceUnits = 0;               //[x]
        universe->gameStats.playerStats[i].totalInjectedResources = 0;               //[x]
        universe->gameStats.playerStats[i].totalResourceUnitsViaBounties = 0;               //[x]



        universe->gameStats.playerStats[i].totalRUsInCurrentShips = 0;
        universe->gameStats.playerStats[i].totalShips = 0;

        universe->gameStats.playerStats[i].totalKills = 0;
        universe->gameStats.playerStats[i].totalRUsKilled = 0;

        universe->gameStats.playerStats[i].totalLosses = 0;
        universe->gameStats.playerStats[i].totalRUsLost = 0;
        // CAT: the three Cataclysm additions are zeroed too (initGameStats 0x0053f100: dwords 9, 0xa9.. and 0x1f3)
        universe->gameStats.playerStats[i].catStat28 = 0;
        universe->gameStats.playerStats[i].catStat7D0 = 0;


        for(j=0;j<universe->numPlayers;j++)
        {
            universe->gameStats.playerStats[i].pointsOfDamageDoneToWho[j] = 0.0f;
        }

        for(j=0;j<TOTAL_NUM_SHIPS;j++)
        {
            universe->gameStats.playerStats[i].totalForEachShip[j]=0;
            universe->gameStats.playerStats[i].totalKillsForEachShip[j]=0;
            universe->gameStats.playerStats[i].totalLossesForEachShip[j]=0;
        }
        for(j=0;j<NUM_CLASSES;j++)
        {
            universe->gameStats.playerStats[i].totalForEachClass[j]=0;
            universe->gameStats.playerStats[i].totalKillsForEachClass[j]=0;
            universe->gameStats.playerStats[i].totalLossesForEachClass[j]=0;
            universe->gameStats.playerStats[i].catBuiltForEachClass[j]=0;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : gameStatsAddShip
    Description : Updates stats for ship
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/

void gameStatsAddShip(Ship *ship,sdword playerIndex)
{
    universe.gameStats.playerStats[playerIndex].totalShips++;
    universe.gameStats.playerStats[playerIndex].totalForEachShip[ship->shiptype]++;
    universe.gameStats.playerStats[playerIndex].totalForEachClass[ship->staticinfo->shipclass]++;
    universe.gameStats.totalRUsInAllShips += ship->staticinfo->buildCost;
    universe.gameStats.playerStats[playerIndex].totalRUsInCurrentShips += ship->staticinfo->buildCost;
    // CAT: no totalShipsInGame++ (gameStatsAddShip 0x0053f220 ends with the player's totalRUsInCurrentShips)
}


void gameStatsShipDied(Ship *ship)
{
    sdword shipownerindex,killerPlayerIndex;

    if(ship->playerowner != NULL)
        shipownerindex = ship->playerowner->playerIndex;
    else
        dbgFatalf(DBG_Loc,"\nShip player pointer NULL...shouldn't be");
    // CAT: no Drone exception (gameStatsShipDied 0x0053f290)
    switch(ship->howDidIDie)
    {
    case DEATH_Killed_By_Player:
    case DEATH_Killed_By_Player_Explosion:
    case DEATH_I_DIDNT_DIE_I_WAS_SALAVAGED:
    case DEATH_Killed_By_Kamikaze:
        killerPlayerIndex = ship->whoKilledMe;
        universe.gameStats.totalRUsKilled += ship->staticinfo->buildCost;
        universe.gameStats.totalKills++;
        break;
    case DEATH_BY_GAME_END:
        return;

    //non player depenedant:  Only register losses, not kills
    case DEATH_Killed_By_Collision:
    case DEATH_Killed_By_Cloud:
    case DEATH_Killed_By_Dead_Player:
    case DEATH_BY_RETIREMENT:
    case DEATH_BY_AFTER_DEAD:
    case DEATH_Left_Behind_At_Hyperspace:
    default:
        killerPlayerIndex = 99;
        break;
    }

    // CAT: the killer is any index below 8 other than the owner (gameStatsShipDied 0x0053f290: signed compare with 8;
    //      HW1 tested != 99). The class counters skip the Swarmers and count the ACVs and the MCV twice.
    if(killerPlayerIndex != ship->playerowner->playerIndex && killerPlayerIndex < MAX_MULTIPLAYER_PLAYERS)
    {
        //Didn't kill own ships...and we need to register the kills
        universe.gameStats.playerStats[killerPlayerIndex].totalRUsKilled += ship->staticinfo->buildCost;
        universe.gameStats.playerStats[killerPlayerIndex].totalKills++;
        universe.gameStats.playerStats[killerPlayerIndex].totalKillsForEachShip[ship->shiptype]++;
        if ((ship->shiptype != sSwarmer) && (ship->shiptype != bSwarmer))
        {
            universe.gameStats.playerStats[killerPlayerIndex].totalKillsForEachClass[ship->staticinfo->shipclass]++;
            if ((ship->shiptype == sACV) || (ship->shiptype == sMCV) || (ship->shiptype == bACV))
            {
                universe.gameStats.playerStats[killerPlayerIndex].totalKillsForEachClass[ship->staticinfo->shipclass]++;
            }
        }
    }

    universe.gameStats.playerStats[ship->playerowner->playerIndex].totalLosses++;
    universe.gameStats.playerStats[ship->playerowner->playerIndex].totalLossesForEachShip[ship->shiptype]++;
    if ((ship->shiptype != sSwarmer) && (ship->shiptype != bSwarmer))      // CAT: as above
    {
        universe.gameStats.playerStats[ship->playerowner->playerIndex].totalLossesForEachClass[ship->staticinfo->shipclass]++;
        if ((ship->shiptype == sACV) || (ship->shiptype == sMCV) || (ship->shiptype == bACV))
        {
            universe.gameStats.playerStats[ship->playerowner->playerIndex].totalLossesForEachClass[ship->staticinfo->shipclass]++;
        }
    }
    universe.gameStats.playerStats[ship->playerowner->playerIndex].totalRUsLost += ship->staticinfo->buildCost;
    universe.gameStats.playerStats[ship->playerowner->playerIndex].totalRUsInCurrentShips -= ship->staticinfo->buildCost;

    universe.gameStats.totalRUsLost += ship->staticinfo->buildCost;
    universe.gameStats.totalRUsInAllShips -= ship->staticinfo->buildCost;
}
//DEBUG FUNCTION

#ifdef DEBUG_GAME_STATS

char gamestatsfilename[] = "gamestats.log";
sdword needToInit=FALSE;

void gameStatsInitLog()
{
    needToInit=TRUE;
}
void gameStatsInitForReal()
{
    filehandle fh;
    FILE *fp;
    sdword i;
    GameStatsDebugHeader header;
    fileDelete(gamestatsfilename);

    fh = fileOpen(gamestatsfilename, FF_AppendMode | FF_UserSettingsPath);
    dbgAssertOrIgnore(!fileUsingBigfile(fh));
    fp = fileStream(fh);

    header.numPlayers = universe.numPlayers;
    for(i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
    {
        strcpy(header.names[i],playerNames[i]);
    }
    //now create and save header file
    fwrite(&header,sizeof(GameStatsDebugHeader),1,fp);

    fileClose(fh);
}
void gameStatsUpdateLogFile(sdword framenum)
{
    filehandle fh;
    FILE *fp;
    //sdword i;

    if(needToInit)
    {
        gameStatsInitForReal();
        needToInit=FALSE;
    }
    fh = fileOpen(gamestatsfilename, FF_AppendMode | FF_UserSettingsPath);
    dbgAssertOrIgnore(!fileUsingBigfile(fh));
    fp = fileStream(fh);

    //write frame number
    fwrite(&framenum,sizeof(sdword),1,fp);
    //write global game stats
    fwrite(&universe.gameStats,sizeof(GameStats)-sizeof(PlayerStats)*(MAX_MULTIPLAYER_PLAYERS+1-universe.numPlayers),1,fp);
    //write individual player stats
    //for(i=0;i<universe.numPlayers;i++)
    //{
    //    fwrite(&universe.gameStats.playerStats[i],sizeof(PlayerStats),1,fp);
    //}
    fileClose(fh);
}

#endif

// this function calculates the total ship cost for each player (typically at game start)
void gameStatsCalcShipCostTotals(void)
{
    ShipStaticInfo *pInfo;

    sdword i,j;

    // check for single player game
    if(singlePlayerGame) return;

    for(i=0;i<universe.numPlayers;i++)
    {
        // reset cost
        universe.players[i].initialShipCost=0;

        // calculate cost
        for(j=0;j<TOTAL_NUM_SHIPS;j++)
        {
            if(universe.gameStats.playerStats[i].totalForEachShip[j] > 0)
            {
                pInfo=GetShipStaticInfoSafe(j,universe.players[i].race);
                if(pInfo != NULL)
                {
                    universe.players[i].initialShipCost+=universe.gameStats.playerStats[i].totalForEachShip[j]*
                        pInfo->buildCost;
                }
            }
        }
    }

    return;
}

// this function returns the actual ship class based on ship type, -1 otherwise
// CAT: from the loaded ship static (the HW1 type table named HW1 types only; this game-stats log check is GoK debug
//      code, absent from cataclysm.exe)
sdword gameStatsGetShipClass(sdword nShipType)
{
    ShipStaticInfo *statinfo = GetShipStaticInfoSafe(nShipType, 0);

    if (statinfo == NULL)
    {
        return(-1);
    }
    return(statinfo->shipclass);
}

#define isPlayerIndexCmptPlayer(i)  ((i) >= tpGameCreated.numPlayers)

// this function checks the bounds for RUs
bool32 verifyGameStatsResUnitsBounds(FILE *pStatsFile,sdword nPlayerNum,sdword nResourceUnits)
{
    // check for single player game
    if(singlePlayerGame) return(TRUE);

    // check computer player flag
    if(isPlayerIndexCmptPlayer(nPlayerNum)) return(TRUE);

    if((nResourceUnits < 0) || (nResourceUnits >= 100000))
    {
        // write warning to stats file
        fprintf(pStatsFile,"\t%s",strGetString(strStatsBadResUnitsBounds));
        return(FALSE);
    }

    return(TRUE);
}

// this function checks the totals for RUs
bool32 verifyGameStatsResUnitsTotals(FILE *pStatsFile,sdword nPlayerNum)
{
    sdword nStartingResources=0;

    // check for single player game
    if(singlePlayerGame) return(TRUE);

    // check computer player flag
    if(isPlayerIndexCmptPlayer(nPlayerNum)) return(TRUE);

    switch(tpGameCreated.startingResources)
    {
        case MG_Small :
            nStartingResources=resourceStartSmall;
            break;
        case MG_Medium :
            nStartingResources=resourceStartMedium;
            break;
        case MG_Large :
            nStartingResources=resourceStartLarge;
            break;
    }

    if(universe.gameStats.playerStats[nPlayerNum].totalResourceUnitsSpent > (
        universe.gameStats.playerStats[nPlayerNum].totalInjectedResources+
        universe.gameStats.playerStats[nPlayerNum].totalRegeneratedResourceUnits+
        universe.gameStats.playerStats[nPlayerNum].totalResourceUnitsCollected+
        universe.gameStats.playerStats[nPlayerNum].totalResourceUnitsGiven+
        universe.gameStats.playerStats[nPlayerNum].totalResourceUnitsRecieved+
        universe.gameStats.playerStats[nPlayerNum].totalResourceUnitsViaBounties+
        nStartingResources
        ))
    {
        // write warning to stats file
        fprintf(pStatsFile,"\t%s",strGetString(strStatsBadResUnitTotals));
        return(FALSE);
    }

    return(TRUE);
}

// this function checks the bounds for the number of ships (based on the number of players)
bool32 verifyGameStatsShipsBounds(FILE *pStatsFile,sdword nPlayerNum,sdword nNumShips,sdword nNumPlayers)
{
    // check for single player game
    if(singlePlayerGame) return(TRUE);

    // check computer player flag
    if(isPlayerIndexCmptPlayer(nPlayerNum)) return(TRUE);

    if((nNumShips < 0) || (nNumShips >= 1000*nNumPlayers))
    {
        // write warning to stats file
        fprintf(pStatsFile,"\t%s",strGetString(strStatsBadShipBounds));
        return(FALSE);
    }

    return(TRUE);
}

// this function checks the ship cost totals
bool32 verifyGameStatsShipCostTotals(FILE *pStatsFile,sdword nPlayerNum)
{
    ShipStaticInfo *pInfo;

    sdword n,nShipTotal,nStartingResources=0;

    // check for single player game
    if(singlePlayerGame) return(TRUE);

    // check computer player flag
    if(isPlayerIndexCmptPlayer(nPlayerNum)) return(TRUE);

    switch(tpGameCreated.startingResources)
    {
        case MG_Small :
            nStartingResources=resourceStartSmall;
            break;
        case MG_Medium :
            nStartingResources=resourceStartMedium;
            break;
        case MG_Large :
            nStartingResources=resourceStartLarge;
            break;
    }

    // reset total
    nShipTotal=-universe.players[nPlayerNum].initialShipCost;

    // calculate total
    for(n=0;n<TOTAL_NUM_SHIPS;n++)
    {
        if(universe.gameStats.playerStats[nPlayerNum].totalForEachShip[n] > 0)
        {
            pInfo=GetShipStaticInfoSafe(n,universe.players[nPlayerNum].race);
            if(pInfo != NULL)
            {
                nShipTotal+=universe.gameStats.playerStats[nPlayerNum].totalForEachShip[n]*
                    pInfo->buildCost;
            }
        }
    }

    // compare totals
    if(((nShipTotal - nShipTotal/50 ) > (
        universe.gameStats.playerStats[nPlayerNum].totalInjectedResources+
        universe.gameStats.playerStats[nPlayerNum].totalRegeneratedResourceUnits+
        universe.gameStats.playerStats[nPlayerNum].totalResourceUnitsCollected+
        universe.gameStats.playerStats[nPlayerNum].totalResourceUnitsGiven+
        universe.gameStats.playerStats[nPlayerNum].totalResourceUnitsRecieved+
        universe.gameStats.playerStats[nPlayerNum].totalResourceUnitsViaBounties+
        nStartingResources
        )) || ((nShipTotal - nShipTotal/50 ) > universe.gameStats.playerStats[nPlayerNum].totalResourceUnitsSpent))
    {
        // write warning to stats file
        fprintf(pStatsFile,"\t%s",strGetString(strStatsBadShipCostTotals));
        return(FALSE);
    }

    return(TRUE);
}

// this function checks the total number of ships acquired
bool32 verifyGameStatsShipsAcquiredTotals(FILE *pStatsFile,sdword nPlayerNum)
{
    sdword n,nShipTotal,nClassTotal;

    // check for single player game
    if(singlePlayerGame) return(TRUE);

    // check computer player flag
    if(isPlayerIndexCmptPlayer(nPlayerNum)) return(TRUE);

    // reset total
    nShipTotal=0;

    // calculate total
    for(n=0;n<TOTAL_NUM_SHIPS;n++)
    {
        if(universe.gameStats.playerStats[nPlayerNum].totalForEachShip[n] > 0)
        {
            nShipTotal+=universe.gameStats.playerStats[nPlayerNum].totalForEachShip[n];
        }
    }

    // reset total
    nClassTotal=0;

    // calculate total
    for(n=0;n<NUM_CLASSES;n++)
    {
        if(universe.gameStats.playerStats[nPlayerNum].totalForEachClass[n] > 0)
        {
            nClassTotal+=universe.gameStats.playerStats[nPlayerNum].totalForEachClass[n];
        }
    }

    // compare total
    if((universe.gameStats.playerStats[nPlayerNum].totalShips != nShipTotal) ||
        (universe.gameStats.playerStats[nPlayerNum].totalShips != nClassTotal))
    {
        // write warning to stats file
        fprintf(pStatsFile,"\t%s",strGetString(strStatsBadShipsAcquiredTotals));
        return(FALSE);
    }

    return(TRUE);
}

// this function checks the total number of ships killed
bool32 verifyGameStatsShipsKilledTotals(FILE *pStatsFile,sdword nPlayerNum)
{
    sdword n,nShipTotal,nClassTotal;

    // check for single player game
    if(singlePlayerGame) return(TRUE);

    // check computer player flag
    if(isPlayerIndexCmptPlayer(nPlayerNum)) return(TRUE);

    // reset total
    nShipTotal=0;

    // calculate total
    for(n=0;n<TOTAL_NUM_SHIPS;n++)
    {
        if(universe.gameStats.playerStats[nPlayerNum].totalKillsForEachShip[n] > 0)
        {
            nShipTotal+=universe.gameStats.playerStats[nPlayerNum].totalKillsForEachShip[n];
        }
    }

    // reset total
    nClassTotal=0;

    // calculate total
    for(n=0;n<NUM_CLASSES;n++)
    {
        if(universe.gameStats.playerStats[nPlayerNum].totalKillsForEachClass[n] > 0)
        {
            nClassTotal+=universe.gameStats.playerStats[nPlayerNum].totalKillsForEachClass[n];
        }
    }

    // compare total
    if((universe.gameStats.playerStats[nPlayerNum].totalKills != nShipTotal) ||
        (universe.gameStats.playerStats[nPlayerNum].totalKills != nClassTotal))
    {
        // write warning to stats file
        fprintf(pStatsFile,"\t%s",strGetString(strStatsBadShipsKilledTotals));
        return(FALSE);
    }

    return(TRUE);
}

// this function checks the total number of ships lost
bool32 verifyGameStatsShipsLostTotals(FILE *pStatsFile,sdword nPlayerNum)
{
    sdword n,nShipTotal,nClassTotal;

    // check for single player game
    if(singlePlayerGame) return(TRUE);

    // check computer player flag
    if(isPlayerIndexCmptPlayer(nPlayerNum)) return(TRUE);

    // reset total
    nShipTotal=0;

    // calculate total
    for(n=0;n<TOTAL_NUM_SHIPS;n++)
    {
        if(universe.gameStats.playerStats[nPlayerNum].totalLossesForEachShip[n] > 0)
        {
            nShipTotal+=universe.gameStats.playerStats[nPlayerNum].totalLossesForEachShip[n];
        }
    }

    // reset total
    nClassTotal=0;

    // calculate total
    for(n=0;n<NUM_CLASSES;n++)
    {
        if(universe.gameStats.playerStats[nPlayerNum].totalLossesForEachClass[n] > 0)
        {
            nClassTotal+=universe.gameStats.playerStats[nPlayerNum].totalLossesForEachClass[n];
        }
    }

    // compare total
    if((universe.gameStats.playerStats[nPlayerNum].totalLosses != nShipTotal) ||
        (universe.gameStats.playerStats[nPlayerNum].totalLosses != nClassTotal))
    {
        // write warning to stats file
        fprintf(pStatsFile,"\t%s",strGetString(strStatsBadShipsLostTotals));
        return(FALSE);
    }

    return(TRUE);
}

// this function checks the number of ships acquired per class
bool32 verifyGameStatsShipsAcquiredClassTotals(FILE *pStatsFile,sdword nPlayerNum,sdword nShipClass)
{
    sdword n,nShipTotal,nActualClass;

    // check for single player game
    if(singlePlayerGame) return(TRUE);

    // check computer player flag
    if(isPlayerIndexCmptPlayer(nPlayerNum)) return(TRUE);

    // reset total
    nShipTotal=0;

    // calculate total
    for(n=0;n<TOTAL_NUM_SHIPS;n++)
    {
        if(universe.gameStats.playerStats[nPlayerNum].totalForEachShip[n] > 0)
        {
            // get actual ship class
            nActualClass=gameStatsGetShipClass(n);
            if((nActualClass >= 0) && (nActualClass == nShipClass))
            {
                nShipTotal+=universe.gameStats.playerStats[nPlayerNum].totalForEachShip[n];
            }
        }
    }

    // compare totals
    if(nShipTotal != universe.gameStats.playerStats[nPlayerNum].totalForEachClass[nShipClass])
    {
        // write warning to stats file
        fprintf(pStatsFile,"\t%s",strGetString(strStatsBadShipsAcquiredClassTotals));
        return(FALSE);
    }

    return(TRUE);
}

// this function checks the number of ships killed per class
bool32 verifyGameStatsShipsKilledClassTotals(FILE *pStatsFile,sdword nPlayerNum,sdword nShipClass)
{
    sdword n,nShipTotal,nActualClass;

    // check for single player game
    if(singlePlayerGame) return(TRUE);

    // check computer player flag
    if(isPlayerIndexCmptPlayer(nPlayerNum)) return(TRUE);

    // reset total
    nShipTotal=0;

    // calculate total
    for(n=0;n<TOTAL_NUM_SHIPS;n++)
    {
        if(universe.gameStats.playerStats[nPlayerNum].totalKillsForEachShip[n] > 0)
        {
            // get actual ship class
            nActualClass=gameStatsGetShipClass(n);
            if((nActualClass >= 0) && (nActualClass == nShipClass))
            {
                nShipTotal+=universe.gameStats.playerStats[nPlayerNum].totalKillsForEachShip[n];
            }
        }
    }

    // compare totals
    if(nShipTotal != universe.gameStats.playerStats[nPlayerNum].totalKillsForEachClass[nShipClass])
    {
        // write warning to stats file
        fprintf(pStatsFile,"\t%s",strGetString(strStatsBadShipsKilledClassTotals));
        return(FALSE);
    }

    return(TRUE);
}

// this function checks the number of ships lost per class
bool32 verifyGameStatsShipsLostClassTotals(FILE *pStatsFile,sdword nPlayerNum,sdword nShipClass)
{
    sdword n,nShipTotal,nActualClass;

    // check for single player game
    if(singlePlayerGame) return(TRUE);

    // check computer player flag
    if(isPlayerIndexCmptPlayer(nPlayerNum)) return(TRUE);

    // reset total
    nShipTotal=0;

    // calculate total
    for(n=0;n<TOTAL_NUM_SHIPS;n++)
    {
        if(universe.gameStats.playerStats[nPlayerNum].totalLossesForEachShip[n] > 0)
        {
            // get actual ship class
            nActualClass=gameStatsGetShipClass(n);
            if((nActualClass >= 0) && (nActualClass == nShipClass))
            {
                nShipTotal+=universe.gameStats.playerStats[nPlayerNum].totalLossesForEachShip[n];
            }
        }
    }

    // compare totals
    if(nShipTotal != universe.gameStats.playerStats[nPlayerNum].totalLossesForEachClass[nShipClass])
    {
        // write warning to stats file
        fprintf(pStatsFile,"\t%s",strGetString(strStatsBadShipsLostClassTotals));
        return(FALSE);
    }

    return(TRUE);
}

// why are there no comments below?
void writeGameStatsToFile(char *filename)
{
    sdword i,j,startingresources=0;
    filehandle statsFH;
    FILE *statsFile = NULL;

    if (singlePlayerGame)
    {
        return;
    }

    statsFH = fileOpen(filename, FF_WriteMode | FF_TextMode | FF_ReturnNULLOnFail | FF_UserSettingsPath);
    if (!statsFH)
    {
        return;
    }
    dbgAssertOrIgnore(!fileUsingBigfile(statsFH));
    statsFile = fileStream(statsFH);

    if (statsFile)
    {
        if (randSyncErr)
        {
            fprintf(statsFile,"\nRAND SYNC ERR: Frame %d",randSyncErrFrame);
        }
        if (univSyncErr)
        {
            fprintf(statsFile,"\nUNIV SYNC ERR: Frame %d",univSyncErrFrame);
        }
        if (multiPlayerGame)        // only print cheat detect in multiplayer game
        {
            if (blobSyncErr)
            {
                fprintf(statsFile,"\n");
                fprintf(statsFile,strGetString(strCheatDetect),blobSyncErrFrame);
            }
        }
        if (pktSyncErr)
        {
            fprintf(statsFile,"\nPKT SYNC ERR");
        }

        for(i=0;i<universe.numPlayers;i++)
        {
            fprintf(statsFile,"\n*************");
            fprintf(statsFile,"\n*************");
            fprintf(statsFile,"\n");
            fprintf(statsFile,strGetString(strStatsPlayerName),playerNames[i]);
            fprintf(statsFile,"\n");
            fprintf(statsFile,strGetString(strStatsPlayerRace),ShipRaceToStr(universe.players[i].race));
            fprintf(statsFile,"\n___________________\n");

            if (universe.gameStats.playerStats[i].timeOfDeath)
            {
                fprintf(statsFile,"\n");
                fprintf(statsFile,strGetString(strStatsTimeOfDeath), universe.gameStats.playerStats[i].timeOfDeath);
                if (universe.gameStats.playerStats[i].typeOfPlayerDeath)
                {
                    fprintf(statsFile,"\n");
                    switch (universe.gameStats.playerStats[i].typeOfPlayerDeath)
                    {
                        case PLAYERKILLED_DROPPEDOUT:
                            fprintf(statsFile, "%s", strGetString(strStatsDeathByDropout));
                            break;
                        case PLAYERKILLED_CAPTUREDSHIP:
                            fprintf(statsFile, "%s", strGetString(strStatsDeathByShipCapture));
                            break;
                        case PLAYERKILLED_LOSTMISSION:
                            fprintf(statsFile, "%s", strGetString(strStatsDeathByLossOfMission));
                            break;
                    }
                }
            }
            else
            {
                fprintf(statsFile,"\n");
                fprintf(statsFile, "%s", strGetString(strStatsDeathStillAlive));
            }

            switch(tpGameCreated.startingResources)
            {
                case MG_Small:
                    startingresources=resourceStartSmall;
                    break;
                case MG_Medium:
                    startingresources=resourceStartMedium;
                    break;
                case MG_Large:
                    startingresources=resourceStartLarge;
                    break;
            }

            fprintf(statsFile,"\n");    fprintf(statsFile,strGetString(strStatsRUsStart)    ,startingresources);
                // check stats
                verifyGameStatsResUnitsBounds(statsFile,i,startingresources);
            fprintf(statsFile,"\n");    fprintf(statsFile,strGetString(strStatsRUsCollected),universe.gameStats.playerStats[i].totalResourceUnitsCollected);
                // check stats
                verifyGameStatsResUnitsBounds(statsFile,i,universe.gameStats.playerStats[i].totalResourceUnitsCollected);
            fprintf(statsFile,"\n");    fprintf(statsFile,strGetString(strStatsRUsSpent)    ,universe.gameStats.playerStats[i].totalResourceUnitsSpent);
                // check stats
                if(verifyGameStatsResUnitsBounds(statsFile,i,universe.gameStats.playerStats[i].totalResourceUnitsSpent) == TRUE)
                    verifyGameStatsResUnitsTotals(statsFile,i);
            fprintf(statsFile,"\n");    fprintf(statsFile,strGetString(strStatsRUsGiven)    ,universe.gameStats.playerStats[i].totalResourceUnitsGiven);
                // check stats
                verifyGameStatsResUnitsBounds(statsFile,i,universe.gameStats.playerStats[i].totalResourceUnitsGiven);
            fprintf(statsFile,"\n");    fprintf(statsFile,strGetString(strStatsRUsReceived) ,universe.gameStats.playerStats[i].totalResourceUnitsRecieved);
                // check stats
                verifyGameStatsResUnitsBounds(statsFile,i,universe.gameStats.playerStats[i].totalResourceUnitsRecieved);
            fprintf(statsFile,"\n");    fprintf(statsFile,strGetString(strStatsRUsInjected) ,universe.gameStats.playerStats[i].totalInjectedResources);
                // check stats
                verifyGameStatsResUnitsBounds(statsFile,i,universe.gameStats.playerStats[i].totalInjectedResources);
            fprintf(statsFile,"\n");    fprintf(statsFile,strGetString(strStatsRUsByBounties),universe.gameStats.playerStats[i].totalResourceUnitsViaBounties);
                // check stats
                verifyGameStatsResUnitsBounds(statsFile,i,universe.gameStats.playerStats[i].totalResourceUnitsViaBounties);
            fprintf(statsFile,"\n");    fprintf(statsFile,strGetString(strStatsRUsGenerated),universe.gameStats.playerStats[i].totalRegeneratedResourceUnits);
                // check stats
                verifyGameStatsResUnitsBounds(statsFile,i,universe.gameStats.playerStats[i].totalRegeneratedResourceUnits);
            fprintf(statsFile,"\n");    fprintf(statsFile,strGetString(strStatsCurrentRUs),universe.players[i].resourceUnits);
                // check stats
                verifyGameStatsResUnitsBounds(statsFile,i,universe.players[i].resourceUnits);
            fprintf(statsFile,"\n___________________\n");

            for (j=0;j<universe.numPlayers;j++)
            {
                if (j!=i)
                {
                    fprintf(statsFile,"\n");
                    fprintf(statsFile, strGetString(strStatsTotalDmgAgainstPlayer), j, universe.gameStats.playerStats[i].pointsOfDamageDoneToWho[j]);
                }
            }

            fprintf(statsFile,"\n\n");
            fprintf(statsFile,strGetString(strStatsTotalAcquiredShips),universe.gameStats.playerStats[i].totalShips);
                // check stats
                if(verifyGameStatsShipsBounds(statsFile,i,universe.gameStats.playerStats[i].totalShips,1) == TRUE)
                    verifyGameStatsShipsAcquiredTotals(statsFile,i);
            fprintf(statsFile,"\n\n");
            fprintf(statsFile, "%s", strGetString(strStatsTotalAcqShipsByType));
            fprintf(statsFile,"\n");

            for(j=0;j<TOTAL_NUM_SHIPS;j++)
            {
                if(universe.gameStats.playerStats[i].totalForEachShip[j] > 0)
                {
                    fprintf(statsFile,"\n%-40s:%d",ShipTypeToNiceStr(j),universe.gameStats.playerStats[i].totalForEachShip[j]);
                        // check stats
                        verifyGameStatsShipsBounds(statsFile,i,universe.gameStats.playerStats[i].totalForEachShip[j],1);
                }
            }
            fprintf(statsFile,"\n___________________\n\n\n");
            fprintf(statsFile, "%s", strGetString(strStatsTotalAcqShipsByClass));
            for(j=0;j<NUM_CLASSES;j++)
            {
                if(universe.gameStats.playerStats[i].totalForEachClass[j] > 0)
                {
                    fprintf(statsFile,"\n%-40s:%d",ShipClassToNiceStr(j),universe.gameStats.playerStats[i].totalForEachClass[j]);
                        // check stats
                        if(verifyGameStatsShipsBounds(statsFile,i,universe.gameStats.playerStats[i].totalForEachClass[j],1) == TRUE)
                            verifyGameStatsShipsAcquiredClassTotals(statsFile,i,j);
                }
            }
            fprintf(statsFile,"\n___________________\n\n");

            fprintf(statsFile,strGetString(strStatsTotalKills),universe.gameStats.playerStats[i].totalKills);
                // check stats
                if(verifyGameStatsShipsBounds(statsFile,i,universe.gameStats.playerStats[i].totalKills,universe.numPlayers) == TRUE)
                    verifyGameStatsShipsKilledTotals(statsFile,i);
            for(j=0;j<TOTAL_NUM_SHIPS;j++)
            {
                if(universe.gameStats.playerStats[i].totalKillsForEachShip[j] > 0)
                {
                    fprintf(statsFile,"\n%s %s\t\t%d",ShipTypeToNiceStr(j),strGetString(strStatsKilled),universe.gameStats.playerStats[i].totalKillsForEachShip[j]);
                        // check stats
                        verifyGameStatsShipsBounds(statsFile,i,universe.gameStats.playerStats[i].totalKillsForEachShip[j],universe.numPlayers);
                }
            }
            for(j=0;j<NUM_CLASSES;j++)
            {
                if(universe.gameStats.playerStats[i].totalKillsForEachClass[j] > 0)
                {
                    fprintf(statsFile,"\n%s %s\t\t%d",ShipClassToNiceStr(j),strGetString(strStatsKilled),universe.gameStats.playerStats[i].totalKillsForEachClass[j]);
                        // check stats
                        if(verifyGameStatsShipsBounds(statsFile,i,universe.gameStats.playerStats[i].totalKillsForEachClass[j],universe.numPlayers) == TRUE)
                            verifyGameStatsShipsKilledClassTotals(statsFile,i,j);
                }
            }
            fprintf(statsFile,"\n___________________\n\n");

            fprintf(statsFile,strGetString(strStatsTotalLosses),universe.gameStats.playerStats[i].totalLosses);
                // check stats
                if(verifyGameStatsShipsBounds(statsFile,i,universe.gameStats.playerStats[i].totalLosses,universe.numPlayers) == TRUE)
                    verifyGameStatsShipsLostTotals(statsFile,i);
            for(j=0;j<TOTAL_NUM_SHIPS;j++)
            {
                if(universe.gameStats.playerStats[i].totalLossesForEachShip[j] > 0)
                {
                    fprintf(statsFile,"\n%s %s\t\t %d",ShipTypeToNiceStr(j),strGetString(strStatsLost),universe.gameStats.playerStats[i].totalLossesForEachShip[j]);
                        // check stats
                        verifyGameStatsShipsBounds(statsFile,i,universe.gameStats.playerStats[i].totalLossesForEachShip[j],universe.numPlayers);
                }
            }
            for(j=0;j<NUM_CLASSES;j++)
            {
                if(universe.gameStats.playerStats[i].totalLossesForEachClass[j] > 0)
                {
                    fprintf(statsFile,"\n%s %s\t\t %d",ShipClassToNiceStr(j),strGetString(strStatsLost),universe.gameStats.playerStats[i].totalLossesForEachClass[j]);
                        // check stats
                        if(verifyGameStatsShipsBounds(statsFile,i,universe.gameStats.playerStats[i].totalLossesForEachClass[j],universe.numPlayers) == TRUE)
                            verifyGameStatsShipsLostClassTotals(statsFile,i,j);
                }
            }
            fprintf(statsFile,"\n___________________\n");
            fprintf(statsFile,"\n___________________\n");
        }
    }

    fileClose(statsFH);
}
