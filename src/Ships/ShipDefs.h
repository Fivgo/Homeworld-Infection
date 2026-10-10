// =============================================================================
//  ShipDefs.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___SHIPDEFS_H
#define ___SHIPDEFS_H

// CAT: Cataclysm's 145 ship types, one type per race-ship, in seven race ranges (shiptypeinfo 0x00889530,
//      FirstShipTypeOfRace 0x008894e8, LastShipTypeOfRace 0x00889504; CET game/Game/ShipDefs.h). HW1 had 59
//      race-neutral types; no Cataclysm type keeps its HW1 id (docs/m2-r1-data-deltas.md section 3).
typedef enum
{
    ShipType_Uninitialized = -1,

    // RACE_Sect: 0-32
    sRecon                      = 0,
    sAcolyte                    = 1,
    sMimic                      = 2,
    sLeech                      = 3,
    sACV                        = 4,
    sMCV                        = 5,
    sRammingFrigate             = 6,
    sMultiBeamFrigate           = 7,
    sHiveFrigate                = 8,
    sSwarmer                    = 9,
    sDreadnought                = 10,
    sDreadnoughtCannon          = 11,
    sCarrier                    = 12,
    sCarrierSupport             = 13,
    sWorker                     = 14,
    sRepairDroid                = 15,
    sProcessor                  = 16,
    sSentinel                   = 17,
    sMothership                 = 18,
    sMothershipSpecial          = 19,
    sMothershipEngineering      = 20,
    sMothershipArmour           = 21,
    sMothershipMicro            = 22,
    sMothershipWeapons          = 23,
    sMothershipDockingBay       = 24,
    sDestroyer                  = 25,
    sMothershipSupport          = 26,
    sMothershipCommand          = 27,
    sMothershipEnginesLower     = 28,
    sMothershipEnginesUpper     = 29,
    sMothershipLowerSpine       = 30,
    sMothershipBigGun           = 31,
    sMothershipOreCanArmour     = 32,

    // RACE_Beast: 33-59
    bRecon                      = 33,
    bAcolyte                    = 34,
    bInterceptor                = 35,
    bCloakedFighter             = 36,
    bAttackBomber               = 37,
    bACV                        = 38,
    bMultiGunCorvette           = 39,
    bHeavyCorvette              = 40,
    bMultiBeamFrigate           = 41,
    bHiveFrigate                = 42,
    bSwarmer                    = 43,
    bIonArrayFrigate            = 44,
    bDFGFrigate                 = 45,
    bHeavyCruiser               = 46,
    bCarrier                    = 47,
    bRammingFrigate             = 48,
    bWorker                     = 49,
    bRepairDroid                = 50,
    bProcessor                  = 51,
    bSentinel                   = 52,
    bMothership                 = 53,
    bMothershipDockingBay       = 54,
    bMothershipEnginesLower     = 55,
    bSectMoship                 = 56,
    bMothershipSupport          = 57,
    bCruiseMissile              = 58,
    bMissileCorvette            = 59,

    // RACE_Kushan: 60-73
    kAdvanceSupportFrigate      = 60,
    kAttackBomber               = 61,
    kCarrier                    = 62,
    kCloakGenerator             = 63,
    kGravWellGenerator          = 64,
    kHeavyCorvette              = 65,
    kHeavyCruiser               = 66,
    kHeavyDefender              = 67,
    kHeavyInterceptor           = 68,
    kIonCannonFrigate           = 69,
    kMultiGunCorvette           = 70,
    kProximitySensor            = 71,
    kSalCapCorvette             = 72,
    kStandardDestroyer          = 73,

    // RACE_Taiidan: 74-91
    tAdvanceSupportFrigate      = 74,
    tAttackBomber               = 75,
    tCarrier                    = 76,
    tCloakGenerator             = 77,
    tDFGFrigate                 = 78,
    tGravWellGenerator          = 79,
    tHeavyCruiser               = 80,
    tHeavyDefender              = 81,
    tHeavyInterceptor           = 82,
    tIonCannonFrigate           = 83,
    tMinelayerCorvette          = 84,
    tMissileDestroyer           = 85,
    tMultiGunCorvette           = 86,
    tProximitySensor            = 87,
    tResourceCollector          = 88,
    tSalCapCorvette             = 89,
    tStandardDestroyer          = 90,
    tStandardFrigate            = 91,

    // RACE_P1: 92-97
    p1Outpost                   = 92,
    p1Fighter                   = 93,
    p1IonArrayFrigate           = 94,
    p1MissileCorvette           = 95,
    p1Mothership                = 96,
    p1StandardCorvette          = 97,

    // RACE_Traders: 98-108
    trFloatingCity              = 98,
    trCargoBarge                = 99,
    trMiningBase                = 100,
    trResearchStation           = 101,
    trJunkYardDawg              = 102,
    trJunkYardHQ                = 103,
    trGhostship                 = 104,
    trJunk_LGun                 = 105,
    trJunk_SGun                 = 106,
    trResearchStationBridge     = 107,
    trResearchStationTower      = 108,

    // RACE_Derelict: 109-144
    dRKO                        = 109,
    dBigGun                     = 110,
    dBigGunUnit                 = 111,
    dSlipgateSmall              = 112,
    dSlipgateMedium             = 113,
    dSlipgateLarge              = 114,
    dCrate                      = 115,
    dJunk1PartA                 = 116,
    dJunk1Shell                 = 117,
    dJunk1Strut                 = 118,
    dJunk2Shipwreck             = 119,
    dJunk3Boiler                = 120,
    dJunk3BoilerCasing          = 121,
    dMessagePod                 = 122,
    dBuoy                       = 123,
    dFirstPlanet                = 124,
    dDeadPlanet                 = 125,
    dCrystal0                   = 126,
    dCrystal1                   = 127,
    dBeastDockingBay            = 128,
    dNISBuoy                    = 129,
    dBeastMothership            = 130,
    dNomadMoon                  = 131,
    dNomadFieldEmitter          = 132,
    dAcolyte                    = 133,
    dPrisonShip                 = 134,
    dPrisonShipInfected         = 135,
    dSlipgateNode               = 136,
    dKushanCarrier              = 137,
    dIonArrayFrigate            = 138,
    dIonCannonFrigate           = 139,
    dEcclesiam                  = 140,
    dEcclesiamInfected          = 141,
    nisLightSource1             = 142,
    nisLightSource2             = 143,
    nisLensflareSource1         = 144,

    // CAT: HW1 type names, kept only so that HW1 code not yet ported compiles (M2-R2 stage 1, docs/m2-r2-identifiers.md).
    //      Each name that has a Cataclysm counterpart is an alias of ONE counterpart (rule: the Kushan k* ship, else the
    //      Taiidan t*, else Sect s*, else Beast b*, else the P1/Traders ship of the same name); HW1 code that tests
    //      such a name therefore only sees that one race variant until stage 2 ports the function. The 22 HW1 types
    //      that Cataclysm dropped get ids >= HW1LEGACY_FIRST_SHIP, which no Cataclysm data can produce (StrToShipType
    //      only returns 0..144), so HW1 code keyed on them is dormant. Never index a TOTAL_NUM_SHIPS table with them.
    AdvanceSupportFrigate       = kAdvanceSupportFrigate,
    AttackBomber                = kAttackBomber,
    Carrier                     = kCarrier,
    CloakedFighter              = bCloakedFighter,
    CloakGenerator              = kCloakGenerator,
    DFGFrigate                  = tDFGFrigate,
    GravWellGenerator           = kGravWellGenerator,
    HeavyCorvette               = kHeavyCorvette,
    HeavyCruiser                = kHeavyCruiser,
    HeavyDefender               = kHeavyDefender,
    HeavyInterceptor            = kHeavyInterceptor,
    IonCannonFrigate            = kIonCannonFrigate,
    MinelayerCorvette           = tMinelayerCorvette,
    MissileDestroyer            = tMissileDestroyer,
    Mothership                  = sMothership,
    MultiGunCorvette            = kMultiGunCorvette,
    ProximitySensor             = kProximitySensor,
    ResourceCollector           = tResourceCollector,
    SalCapCorvette              = kSalCapCorvette,
    StandardDestroyer           = kStandardDestroyer,
    StandardFrigate             = tStandardFrigate,
    P1Fighter                   = p1Fighter,
    P1IonArrayFrigate           = p1IonArrayFrigate,
    P1MissileCorvette           = p1MissileCorvette,
    P1Mothership                = p1Mothership,
    P1StandardCorvette          = p1StandardCorvette,
    FloatingCity                = trFloatingCity,
    CargoBarge                  = trCargoBarge,
    MiningBase                  = trMiningBase,
    ResearchStation             = trResearchStation,
    JunkYardDawg                = trJunkYardDawg,
    JunkYardHQ                  = trJunkYardHQ,
    Ghostship                   = trGhostship,
    Junk_LGun                   = trJunk_LGun,
    Junk_SGun                   = trJunk_SGun,
    ResearchStationBridge       = trResearchStationBridge,
    ResearchStationTower        = trResearchStationTower,

    HW1LEGACY_FIRST_SHIP        = 200,
    DDDFrigate                  = HW1LEGACY_FIRST_SHIP,
    DefenseFighter,
    LightCorvette,
    LightDefender,
    LightInterceptor,
    Probe,
    RepairCorvette,
    ResearchShip,
    ResourceController,
    SensorArray,
    Drone,
    TargetDrone,
    HeadShotAsteroid,
    CryoTray,
    P2AdvanceSwarmer,
    P2FuelPod,
    P2Mothership,
    P2MultiBeamFrigate,
    P2Swarmer,
    P3Destroyer,
    P3Frigate,
    P3Megaship,
    HW1LEGACY_LAST_SHIP         = P3Megaship
} ShipType_enum;

#ifdef HW_PTR_64   //Capture this first, and make sure it's correctly sized.
#include "Types.h"
typedef smemsize  ShipType;
#else
#if defined _LINUX_FIX_ME || defined _WIN32_FIXME
#include "Types.h"
/* It seems that this needs to be unsigned, else something breaks in
   NIS.c and the assault on Kharak NIS in Mission 3 segfaults. */
typedef udword ShipType;
#else
typedef ShipType_enum ShipType;
#endif  // _LINUX_FIX_ME || defined _WIN32_FIXME
#endif // HW_PTR_64

// CAT: race ranges of the 145 types (FirstShipTypeOfRace 0x008894e8 / LastShipTypeOfRace 0x00889504)
#define SECT_FIRST_SHIP       sRecon
#define SECT_LAST_SHIP        sMothershipOreCanArmour
#define BEAST_FIRST_SHIP      bRecon
#define BEAST_LAST_SHIP       bMissileCorvette
#define KUSHAN_FIRST_SHIP     kAdvanceSupportFrigate
#define KUSHAN_LAST_SHIP      kStandardDestroyer
#define TAIIDAN_FIRST_SHIP    tAdvanceSupportFrigate
#define TAIIDAN_LAST_SHIP     tStandardFrigate
#define DERELICT_FIRST_SHIP   dRKO
#define DERELICT_LAST_SHIP    nisLensflareSource1

// CAT: the "standard" (player-buildable) ships are Sect + Beast, 0-59 (CET ShipDefs.h STD_FIRST_SHIP/STD_LAST_SHIP)
#define STD_FIRST_SHIP        sRecon
#define STD_LAST_SHIP         bMissileCorvette

#define P1_FIRST_SHIP         p1Outpost
#define P1_LAST_SHIP          p1StandardCorvette

#define TRADERS_FIRST_SHIP    trFloatingCity
#define TRADERS_LAST_SHIP     trResearchStationTower

// CAT: TOTAL_NUM_SHIPS 145 (CET ShipDefs.h; shiptypeinfo has 145 rows; GetShipStaticInfoSafe tests type <= 0x90)
#define TOTAL_NUM_SHIPS       (DERELICT_LAST_SHIP+1)
#define DefaultShip           TOTAL_NUM_SHIPS    // not really a ship, just a flag indicating we should use Default ship behaviour

#define TOTAL_STD_SHIPS       (STD_LAST_SHIP - STD_FIRST_SHIP + 1)
#define TOTAL_P1_SHIPS        (P1_LAST_SHIP - P1_FIRST_SHIP + 1)
#define TOTAL_TRADERS_SHIPS   (TRADERS_LAST_SHIP - TRADERS_FIRST_SHIP + 1)

#endif
