/*=============================================================================
    Name    : ObjTypes.c
    Purpose : Provides conversions for datatypes found in objtypes.h

    Created 6/24/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "ObjTypes.h"

#include <string.h>

#include "ClassDefs.h"
#include "StatScript.h"
#include "StringSupport.h"

#ifdef _MSC_VER
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif

// CAT: the enum sizes the exe's tables are built for (shiptypeinfo 0x00889530 has 145 rows, shipraceinfo 0x008899c0 7,
//      shipclassinfo 0x00889a80 12; LastShipTypeOfRace 0x00889504 = {32,59,73,91,97,108,144})
_Static_assert(TOTAL_NUM_SHIPS == 145, "Cataclysm has 145 ship types");
_Static_assert(NUM_RACES == 7, "Cataclysm has 7 races");
_Static_assert(NUM_CLASSES == 12, "Cataclysm has 12 ship classes");
_Static_assert(SECT_LAST_SHIP == 32 && BEAST_LAST_SHIP == 59 && KUSHAN_LAST_SHIP == 73 && TAIIDAN_LAST_SHIP == 91 &&
               P1_LAST_SHIP == 97 && TRADERS_LAST_SHIP == 108 && DERELICT_LAST_SHIP == 144,
               "race ranges as LastShipTypeOfRace 0x00889504");
_Static_assert(NSR_Asteroid == 7 && NSR_Missile == 14, "NIS object races as nisraceinfo 0x00889a00");
_Static_assert(HW1LEGACY_FIRST_SHIP > TOTAL_NUM_SHIPS + 2 && HW1LEGACY_LAST_SHIP < 256,
               "HW1-only type ids stay outside every type table and fit in a ubyte");

/*=============================================================================
    Public Data:
=============================================================================*/

// CAT: per-race ship-type ranges of the 145-type enum, in Cataclysm race order (FirstShipTypeOfRace 0x008894e8 =
//      {0,33,60,74,92,98,109}, LastShipTypeOfRace 0x00889504 = {32,59,73,91,97,108,144})
ShipType FirstShipTypeOfRace[NUM_RACES] =
{
    SECT_FIRST_SHIP,
    BEAST_FIRST_SHIP,
    KUSHAN_FIRST_SHIP,
    TAIIDAN_FIRST_SHIP,
    P1_FIRST_SHIP,
    TRADERS_FIRST_SHIP,
    DERELICT_FIRST_SHIP
};

ShipType LastShipTypeOfRace[NUM_RACES] =
{
    SECT_LAST_SHIP,
    BEAST_LAST_SHIP,
    KUSHAN_LAST_SHIP,
    TAIIDAN_LAST_SHIP,
    P1_LAST_SHIP,
    TRADERS_LAST_SHIP,
    DERELICT_LAST_SHIP
};

uword NumShipTypesInRace[NUM_RACES] =
{
    SECT_LAST_SHIP - SECT_FIRST_SHIP + 1,
    BEAST_LAST_SHIP - BEAST_FIRST_SHIP + 1,
    KUSHAN_LAST_SHIP - KUSHAN_FIRST_SHIP + 1,
    TAIIDAN_LAST_SHIP - TAIIDAN_FIRST_SHIP + 1,
    TOTAL_P1_SHIPS,
    TOTAL_TRADERS_SHIPS,
    DERELICT_LAST_SHIP - DERELICT_FIRST_SHIP + 1
};

/*=============================================================================
    Private Data:
=============================================================================*/

// CAT: the 145 Cataclysm ship names in id order (shiptypeinfo 0x00889530, read by StrToShipType 0x004d6e30 and
//      ShipTypeToStr 0x004d6bc0)
static NumStrXlate shiptypeinfo[] =
{
    { (uword)sRecon, strdollar(sRecon) },
    { (uword)sAcolyte, strdollar(sAcolyte) },
    { (uword)sMimic, strdollar(sMimic) },
    { (uword)sLeech, strdollar(sLeech) },
    { (uword)sACV, strdollar(sACV) },
    { (uword)sMCV, strdollar(sMCV) },
    { (uword)sRammingFrigate, strdollar(sRammingFrigate) },
    { (uword)sMultiBeamFrigate, strdollar(sMultiBeamFrigate) },
    { (uword)sHiveFrigate, strdollar(sHiveFrigate) },
    { (uword)sSwarmer, strdollar(sSwarmer) },
    { (uword)sDreadnought, strdollar(sDreadnought) },
    { (uword)sDreadnoughtCannon, strdollar(sDreadnoughtCannon) },
    { (uword)sCarrier, strdollar(sCarrier) },
    { (uword)sCarrierSupport, strdollar(sCarrierSupport) },
    { (uword)sWorker, strdollar(sWorker) },
    { (uword)sRepairDroid, strdollar(sRepairDroid) },
    { (uword)sProcessor, strdollar(sProcessor) },
    { (uword)sSentinel, strdollar(sSentinel) },
    { (uword)sMothership, strdollar(sMothership) },
    { (uword)sMothershipSpecial, strdollar(sMothershipSpecial) },
    { (uword)sMothershipEngineering, strdollar(sMothershipEngineering) },
    { (uword)sMothershipArmour, strdollar(sMothershipArmour) },
    { (uword)sMothershipMicro, strdollar(sMothershipMicro) },
    { (uword)sMothershipWeapons, strdollar(sMothershipWeapons) },
    { (uword)sMothershipDockingBay, strdollar(sMothershipDockingBay) },
    { (uword)sDestroyer, strdollar(sDestroyer) },
    { (uword)sMothershipSupport, strdollar(sMothershipSupport) },
    { (uword)sMothershipCommand, strdollar(sMothershipCommand) },
    { (uword)sMothershipEnginesLower, strdollar(sMothershipEnginesLower) },
    { (uword)sMothershipEnginesUpper, strdollar(sMothershipEnginesUpper) },
    { (uword)sMothershipLowerSpine, strdollar(sMothershipLowerSpine) },
    { (uword)sMothershipBigGun, strdollar(sMothershipBigGun) },
    { (uword)sMothershipOreCanArmour, strdollar(sMothershipOreCanArmour) },
    { (uword)bRecon, strdollar(bRecon) },
    { (uword)bAcolyte, strdollar(bAcolyte) },
    { (uword)bInterceptor, strdollar(bInterceptor) },
    { (uword)bCloakedFighter, strdollar(bCloakedFighter) },
    { (uword)bAttackBomber, strdollar(bAttackBomber) },
    { (uword)bACV, strdollar(bACV) },
    { (uword)bMultiGunCorvette, strdollar(bMultiGunCorvette) },
    { (uword)bHeavyCorvette, strdollar(bHeavyCorvette) },
    { (uword)bMultiBeamFrigate, strdollar(bMultiBeamFrigate) },
    { (uword)bHiveFrigate, strdollar(bHiveFrigate) },
    { (uword)bSwarmer, strdollar(bSwarmer) },
    { (uword)bIonArrayFrigate, strdollar(bIonArrayFrigate) },
    { (uword)bDFGFrigate, strdollar(bDFGFrigate) },
    { (uword)bHeavyCruiser, strdollar(bHeavyCruiser) },
    { (uword)bCarrier, strdollar(bCarrier) },
    { (uword)bRammingFrigate, strdollar(bRammingFrigate) },
    { (uword)bWorker, strdollar(bWorker) },
    { (uword)bRepairDroid, strdollar(bRepairDroid) },
    { (uword)bProcessor, strdollar(bProcessor) },
    { (uword)bSentinel, strdollar(bSentinel) },
    { (uword)bMothership, strdollar(bMothership) },
    { (uword)bMothershipDockingBay, strdollar(bMothershipDockingBay) },
    { (uword)bMothershipEnginesLower, strdollar(bMothershipEnginesLower) },
    { (uword)bSectMoship, strdollar(bSectMoship) },
    { (uword)bMothershipSupport, strdollar(bMothershipSupport) },
    { (uword)bCruiseMissile, strdollar(bCruiseMissile) },
    { (uword)bMissileCorvette, strdollar(bMissileCorvette) },
    { (uword)kAdvanceSupportFrigate, strdollar(kAdvanceSupportFrigate) },
    { (uword)kAttackBomber, strdollar(kAttackBomber) },
    { (uword)kCarrier, strdollar(kCarrier) },
    { (uword)kCloakGenerator, strdollar(kCloakGenerator) },
    { (uword)kGravWellGenerator, strdollar(kGravWellGenerator) },
    { (uword)kHeavyCorvette, strdollar(kHeavyCorvette) },
    { (uword)kHeavyCruiser, strdollar(kHeavyCruiser) },
    { (uword)kHeavyDefender, strdollar(kHeavyDefender) },
    { (uword)kHeavyInterceptor, strdollar(kHeavyInterceptor) },
    { (uword)kIonCannonFrigate, strdollar(kIonCannonFrigate) },
    { (uword)kMultiGunCorvette, strdollar(kMultiGunCorvette) },
    { (uword)kProximitySensor, strdollar(kProximitySensor) },
    { (uword)kSalCapCorvette, strdollar(kSalCapCorvette) },
    { (uword)kStandardDestroyer, strdollar(kStandardDestroyer) },
    { (uword)tAdvanceSupportFrigate, strdollar(tAdvanceSupportFrigate) },
    { (uword)tAttackBomber, strdollar(tAttackBomber) },
    { (uword)tCarrier, strdollar(tCarrier) },
    { (uword)tCloakGenerator, strdollar(tCloakGenerator) },
    { (uword)tDFGFrigate, strdollar(tDFGFrigate) },
    { (uword)tGravWellGenerator, strdollar(tGravWellGenerator) },
    { (uword)tHeavyCruiser, strdollar(tHeavyCruiser) },
    { (uword)tHeavyDefender, strdollar(tHeavyDefender) },
    { (uword)tHeavyInterceptor, strdollar(tHeavyInterceptor) },
    { (uword)tIonCannonFrigate, strdollar(tIonCannonFrigate) },
    { (uword)tMinelayerCorvette, strdollar(tMinelayerCorvette) },
    { (uword)tMissileDestroyer, strdollar(tMissileDestroyer) },
    { (uword)tMultiGunCorvette, strdollar(tMultiGunCorvette) },
    { (uword)tProximitySensor, strdollar(tProximitySensor) },
    { (uword)tResourceCollector, strdollar(tResourceCollector) },
    { (uword)tSalCapCorvette, strdollar(tSalCapCorvette) },
    { (uword)tStandardDestroyer, strdollar(tStandardDestroyer) },
    { (uword)tStandardFrigate, strdollar(tStandardFrigate) },
    { (uword)p1Outpost, strdollar(p1Outpost) },
    { (uword)p1Fighter, strdollar(p1Fighter) },
    { (uword)p1IonArrayFrigate, strdollar(p1IonArrayFrigate) },
    { (uword)p1MissileCorvette, strdollar(p1MissileCorvette) },
    { (uword)p1Mothership, strdollar(p1Mothership) },
    { (uword)p1StandardCorvette, strdollar(p1StandardCorvette) },
    { (uword)trFloatingCity, strdollar(trFloatingCity) },
    { (uword)trCargoBarge, strdollar(trCargoBarge) },
    { (uword)trMiningBase, strdollar(trMiningBase) },
    { (uword)trResearchStation, strdollar(trResearchStation) },
    { (uword)trJunkYardDawg, strdollar(trJunkYardDawg) },
    { (uword)trJunkYardHQ, strdollar(trJunkYardHQ) },
    { (uword)trGhostship, strdollar(trGhostship) },
    { (uword)trJunk_LGun, strdollar(trJunk_LGun) },
    { (uword)trJunk_SGun, strdollar(trJunk_SGun) },
    { (uword)trResearchStationBridge, strdollar(trResearchStationBridge) },
    { (uword)trResearchStationTower, strdollar(trResearchStationTower) },
    { (uword)dRKO, strdollar(dRKO) },
    { (uword)dBigGun, strdollar(dBigGun) },
    { (uword)dBigGunUnit, strdollar(dBigGunUnit) },
    { (uword)dSlipgateSmall, strdollar(dSlipgateSmall) },
    { (uword)dSlipgateMedium, strdollar(dSlipgateMedium) },
    { (uword)dSlipgateLarge, strdollar(dSlipgateLarge) },
    { (uword)dCrate, strdollar(dCrate) },
    { (uword)dJunk1PartA, strdollar(dJunk1PartA) },
    { (uword)dJunk1Shell, strdollar(dJunk1Shell) },
    { (uword)dJunk1Strut, strdollar(dJunk1Strut) },
    { (uword)dJunk2Shipwreck, strdollar(dJunk2Shipwreck) },
    { (uword)dJunk3Boiler, strdollar(dJunk3Boiler) },
    { (uword)dJunk3BoilerCasing, strdollar(dJunk3BoilerCasing) },
    { (uword)dMessagePod, strdollar(dMessagePod) },
    { (uword)dBuoy, strdollar(dBuoy) },
    { (uword)dFirstPlanet, strdollar(dFirstPlanet) },
    { (uword)dDeadPlanet, strdollar(dDeadPlanet) },
    { (uword)dCrystal0, strdollar(dCrystal0) },
    { (uword)dCrystal1, strdollar(dCrystal1) },
    { (uword)dBeastDockingBay, strdollar(dBeastDockingBay) },
    { (uword)dNISBuoy, strdollar(dNISBuoy) },
    { (uword)dBeastMothership, strdollar(dBeastMothership) },
    { (uword)dNomadMoon, strdollar(dNomadMoon) },
    { (uword)dNomadFieldEmitter, strdollar(dNomadFieldEmitter) },
    { (uword)dAcolyte, strdollar(dAcolyte) },
    { (uword)dPrisonShip, strdollar(dPrisonShip) },
    { (uword)dPrisonShipInfected, strdollar(dPrisonShipInfected) },
    { (uword)dSlipgateNode, strdollar(dSlipgateNode) },
    { (uword)dKushanCarrier, strdollar(dKushanCarrier) },
    { (uword)dIonArrayFrigate, strdollar(dIonArrayFrigate) },
    { (uword)dIonCannonFrigate, strdollar(dIonCannonFrigate) },
    { (uword)dEcclesiam, strdollar(dEcclesiam) },
    { (uword)dEcclesiamInfected, strdollar(dEcclesiamInfected) },
    { (uword)nisLightSource1, strdollar(nisLightSource1) },
    { (uword)nisLightSource2, strdollar(nisLightSource2) },
    { (uword)nisLensflareSource1, strdollar(nisLensflareSource1) },
    { 0,NULL }
};

// CAT: race names are the RACE_* strings (shipraceinfo 0x008899c0, StrToShipRace 0x004d6db0, ShipRaceToStr
//      0x004d6b90); they are also the race directory names (race_sect\, ...). Note the exe's spelling "RACE_TAIDAN".
static NumStrXlate shipraceinfo[] =
{
    { (uword)RACE_Sect,     "RACE_SECT" },
    { (uword)RACE_Beast,    "RACE_BEAST" },
    { (uword)RACE_Kushan,   "RACE_KUSHAN" },
    { (uword)RACE_Taiidan,  "RACE_TAIDAN" },
    { (uword)RACE_P1,       "RACE_P1" },
    { (uword)RACE_Traders,  "RACE_TRADERS" },
    { (uword)RACE_Derelict, "RACE_DERELICT" },
    { 0,NULL }
};

// CAT: NIS object races (nisraceinfo 0x00889a00, StrToNisRace 0x004d6bf0): the 7 RACE_* names, then the NSR_* rows
static NumStrXlate nisraceinfo[] =
{
    { (uword)RACE_Sect,     "RACE_SECT" },
    { (uword)RACE_Beast,    "RACE_BEAST" },
    { (uword)RACE_Kushan,   "RACE_KUSHAN" },
    { (uword)RACE_Taiidan,  "RACE_TAIDAN" },
    { (uword)RACE_P1,       "RACE_P1" },
    { (uword)RACE_Traders,  "RACE_TRADERS" },
    { (uword)RACE_Derelict, "RACE_DERELICT" },
    { (uword)NSR_Asteroid , "Asteroids" },
    { (uword)NSR_DustCloud, "DustClouds" },
    { (uword)NSR_GasCloud , "GasClouds" },
    { (uword)NSR_Nebula   , "Nebulae" },
    { (uword)NSR_Derelict , "Derelicts" },
    { (uword)NSR_Effect   , "ETG" },
    { (uword)NSR_Generic  , "Misc" },
    { (uword)NSR_Missile  , "Missile" },
    { 0,NULL }
};
// CAT: 12 classes (shipclassinfo 0x00889a80, StrToShipClass 0x004d6d70, ShipClassToStr 0x004d6b40)
static NumStrXlate shipclassinfo[] =
{
    { (uword)CLASS_Mothership,     strdollar(CLASS_Mothership) },
    { (uword)CLASS_HeavyCruiser,   strdollar(CLASS_HeavyCruiser) },
    { (uword)CLASS_Carrier,        strdollar(CLASS_Carrier) },
    { (uword)CLASS_Destroyer,      strdollar(CLASS_Destroyer) },
    { (uword)CLASS_Frigate,        strdollar(CLASS_Frigate) },
    { (uword)CLASS_Corvette,       strdollar(CLASS_Corvette) },
    { (uword)CLASS_Fighter,        strdollar(CLASS_Fighter) },
    { (uword)CLASS_Resource,       strdollar(CLASS_Resource) },
    { (uword)CLASS_NonCombat,      strdollar(CLASS_NonCombat) },
    { (uword)CLASS_MonsterShip,    strdollar(CLASS_MonsterShip) },
    { (uword)CLASS_Component,      strdollar(CLASS_Component) },
    { (uword)CLASS_Micro,          strdollar(CLASS_Micro) },
    { 0,NULL }
};

/*static NumStrXlate shipclassniceinfo[] =
{
    { (uword)CLASS_Mothership,     "MOTHERSHIP"},
    { (uword)CLASS_HeavyCruiser,   "CRUISER"},
    { (uword)CLASS_Carrier,        "CARRIER"},
    { (uword)CLASS_Destroyer,      "DESTROYER"},
    { (uword)CLASS_Frigate,        "FRIGATE"},
    { (uword)CLASS_Corvette,       "CORVETTE"},
    { (uword)CLASS_Fighter,        "FIGHTER"},
    { (uword)CLASS_Resource,       "RESOURCE"},
    { (uword)CLASS_NonCombat,      "NON-COMBAT"},
    { 0,NULL }
};*/

static NumStrXlate asteroidtypeinfo[] =
{
    { (uword)Asteroid0, strdollar(Asteroid0) },
    { (uword)Asteroid1, strdollar(Asteroid1) },
    { (uword)Asteroid2, strdollar(Asteroid2) },
    { (uword)Asteroid3, strdollar(Asteroid3) },
    { (uword)Asteroid4, strdollar(Asteroid4) },
    { 0,NULL }
};

static NumStrXlate dustcloudtypeinfo[] =
{
    { (uword)DustCloud0, strdollar(DustCloud0) },
    { (uword)DustCloud1, strdollar(DustCloud1) },
    { (uword)DustCloud2, strdollar(DustCloud2) },
    { (uword)DustCloud3, strdollar(DustCloud3) },
    { 0,NULL }
};

static NumStrXlate gascloudtypeinfo[] =
{
    { (uword)GasCloud0, strdollar(GasCloud0) },
    { (uword)GasCloud1, strdollar(GasCloud1) },
    { 0,NULL }
};

static NumStrXlate nebulatypeinfo[] =
{
    { (uword)Nebula0, strdollar(Nebula0) },
    { 0, NULL }
};

// CAT: the exe's 8 rows (derelicttypeinfo 0x00889b68: Crate 0 .. HyperspaceGate 7); replaces HW1's 55 and the
//      CAT-PROVISIONAL appends of M2-R2
static NumStrXlate derelicttypeinfo[] =
{
    { (uword)Crate,                  "Crate"},
    { (uword)Homeworld,              "Homeworld"},
    { (uword)PlanetOfOrigin,         "PlanetOfOrigin"},
    { (uword)PlanetOfOrigin_scarred, "PlanetOfOrigin_scarred"},
    { (uword)Galaxy,                 "Galaxy"},
    { (uword)LavaBall,               "LavaBall"},
    { (uword)MattWorld,              "MattWorld"},
    { (uword)HyperspaceGate,         "HyperspaceGate"},
    { 0,NULL }
};

static NumStrXlate explosiontypeinfo[] =
{
    { ET_Explosion,           "ET_Explosion"           },

    { ET_LightLaserHit,       "ET_LightLaserHit"       },
    { ET_MedLaserHit,         "ET_MedLaserHit"         },
    { ET_HeavyLaserHit,       "ET_HeavyLaserHit"       },
    { ET_PlasmaBombHit,       "ET_PlasmaBombHit"       },

    { ET_InterceptorExplosion,"ET_InterceptorExplosion"},
    { ET_CorvetteExplosion,   "ET_CorvetteExplosion"   },
    { ET_FrigateExplosion,    "ET_FrigateExplosion"    },
    { ET_CarrierExplosion,    "ET_CarrierExplosion"    },
    { ET_ResourcerExplosion,  "ET_ResourcerExplosion"  },

    { 0,NULL }
};

static NumStrXlate guntypeinfo[] =
{
    { (uword)GUN_Fixed,  strdollar(GUN_Fixed) },
    { (uword)GUN_Gimble, strdollar(GUN_Gimble) },
    { (uword)GUN_NewGimble, strdollar(GUN_NewGimble) },
    { (uword)GUN_MissileLauncher, strdollar(GUN_MissileLauncher) },
    { (uword)GUN_MineLauncher, strdollar(GUN_MineLauncher) },
    // CAT: Cataclysm's two gimbal names (guntypeinfo 0x00889c08 rows 5-6, values 1 and 2)
    { (uword)GUN_HiddenGimble, "GUN_HiddenGimble" },
    { (uword)GUN_VisibleGimble, "GUN_VisibleGimble" },
    { 0,NULL }
};

// CAT: Cataclysm's 35 dock point names in the exe's order and values (dockpointtypeinfo 0x00889c48); HW1's three
//      names are not in it
static NumStrXlate dockpointtypeinfo[] =
{
    { (uword)DPT_Small_In, strdollar(DPT_Small_In) },
    { (uword)DPT_Small_Out, strdollar(DPT_Small_Out) },
    { (uword)DPT_Small_In_Engineering, strdollar(DPT_Small_In_Engineering) },
    { (uword)DPT_Small_Out_Engineering, strdollar(DPT_Small_Out_Engineering) },
    { (uword)DPT_Fighter_Latch, strdollar(DPT_Fighter_Latch) },
    { (uword)DPT_Corvette_Latch, strdollar(DPT_Corvette_Latch) },
    { (uword)DPT_Med_In, strdollar(DPT_Med_In) },
    { (uword)DPT_Med_Out, strdollar(DPT_Med_Out) },
    { (uword)DPT_Med_In_Engineering, strdollar(DPT_Med_In_Engineering) },
    { (uword)DPT_Med_Out_Engineering, strdollar(DPT_Med_Out_Engineering) },
    { (uword)DPT_Large_In, strdollar(DPT_Large_In) },
    { (uword)DPT_Large_Out, strdollar(DPT_Large_Out) },
    { (uword)DPT_Leech, strdollar(DPT_Leech) },
    { (uword)DPT_Leech_RU_Transfer, strdollar(DPT_Leech_RU_Transfer) },
    { (uword)DPT_Worker_RU_Transfer, strdollar(DPT_Worker_RU_Transfer) },
    { (uword)DPT_Tow, strdollar(DPT_Tow) },
    { (uword)DPT_Link, strdollar(DPT_Link) },
    { (uword)DPT_Repairbot, strdollar(DPT_Repairbot) },
    { (uword)DPT_Destroyer_EB, strdollar(DPT_Destroyer_EB) },
    { (uword)DPT_Module_EB, strdollar(DPT_Module_EB) },
    { (uword)DPT_Support_EB, strdollar(DPT_Support_EB) },
    { (uword)DPT_Swarmer_In, strdollar(DPT_Swarmer_In) },
    { (uword)DPT_Swarmer_Out, strdollar(DPT_Swarmer_Out) },
    { (uword)DPT_Crystal_Process, strdollar(DPT_Crystal_Process) },
    { (uword)DPT_Crystal_Effect, strdollar(DPT_Crystal_Effect) },
    { (uword)DPT_ForceFieldOrigin, strdollar(DPT_ForceFieldOrigin) },
    { (uword)DPT_Resourcer_RU_Transfer, strdollar(DPT_Resourcer_RU_Transfer) },
    { (uword)DPT_Salvage, strdollar(DPT_Salvage) },
    { (uword)DPT_SalvageLatch, strdollar(DPT_SalvageLatch) },
    { (uword)DPT_SalvageProcess, strdollar(DPT_SalvageProcess) },
    { (uword)DPT_RKOWeaponSource, strdollar(DPT_RKOWeaponSource) },
    { (uword)DPT_AutoRepair, strdollar(DPT_AutoRepair) },
    { (uword)DPT_CleesanBoarding, strdollar(DPT_CleesanBoarding) },
    { (uword)DPT_Mothership_In, strdollar(DPT_Mothership_In) },
    { (uword)DPT_Mothership_Out, strdollar(DPT_Mothership_Out) },

    { 0,NULL }
};

// CAT: gun upgrade classes (cat_weapontypeinfo 0x00889ea8)
static NumStrXlate weapontypeinfo[] =
{
    { (uword)WEAPON_PROJECTILE, strdollar(WEAPON_PROJECTILE) },
    { (uword)WEAPON_ENERGY, strdollar(WEAPON_ENERGY) },
    { (uword)WEAPON_ION, strdollar(WEAPON_ION) },
    { (uword)WEAPON_PLASMABOMB, strdollar(WEAPON_PLASMABOMB) },
    { (uword)WEAPON_LASER, strdollar(WEAPON_LASER) },
    { (uword)WEAPON_MISSILE, strdollar(WEAPON_MISSILE) },
    { (uword)WEAPON_MINE, strdollar(WEAPON_MINE) },
    { (uword)WEAPON_BIGGUN, strdollar(WEAPON_BIGGUN) },
    { (uword)WEAPON_BEASTINFECT, strdollar(WEAPON_BEASTINFECT) },
    { 0,NULL }
};

// CAT: gun size classes (gunsizeinfo 0x00889e80)
static NumStrXlate gunsizeinfo[] =
{
    { (uword)GUNSIZE_SMALL, strdollar(GUNSIZE_SMALL) },
    { (uword)GUNSIZE_MEDIUM, strdollar(GUNSIZE_MEDIUM) },
    { (uword)GUNSIZE_LARGE, strdollar(GUNSIZE_LARGE) },
    { (uword)GUNSIZE_VERYLARGE, strdollar(GUNSIZE_VERYLARGE) },
    { 0,NULL }
};


static NumStrXlate navlighttypeinfo[] =
{
    { (uword)NAVLIGHT_Default, strdollar(NAVLIGHT_Default) },
    { 0,NULL }
};

static NumStrXlate gunsoundtypeinfo[] =
{
    { (uword)GS_LargeEnergyCannon, strdollar(GS_LargeEnergyCannon) },
    { (uword)GS_LargeIonCannon, strdollar(GS_LargeIonCannon) },
    { (uword)GS_LargePlasmaBomb, strdollar(GS_LargePlasmaBomb) },
    { (uword)GS_LargeProjectile, strdollar(GS_LargeProjectile) },
    { (uword)GS_MediumEnergyCannon, strdollar(GS_MediumEnergyCannon) },
    { (uword)GS_MediumIonCannon, strdollar(GS_MediumIonCannon) },
    { (uword)GS_MediumPlasmaBomb, strdollar(GS_MediumPlasmaBomb) },
    { (uword)GS_MediumProjectile, strdollar(GS_MediumProjectile) },
    { (uword)GS_MineLauncher, strdollar(GS_MineLauncher) },
    { (uword)GS_MissleLauncher, strdollar(GS_MissleLauncher) },
    { (uword)GS_SmallEnergyCannon, strdollar(GS_SmallEnergyCannon) },
    { (uword)GS_SmallIonCannon, strdollar(GS_SmallIonCannon) },
    { (uword)GS_SmallPlasmaBomb, strdollar(GS_SmallPlasmaBomb) },
    { (uword)GS_SmallProjectile, strdollar(GS_SmallProjectile) },
    { (uword)GS_VeryLargeEnergyCannon, strdollar(GS_VeryLargeEnergyCannon) },
    { (uword)GS_VeryLargeIonCannon, strdollar(GS_VeryLargeIonCannon) },
    { (uword)GS_VeryLargePlasmaBomb, strdollar(GS_VeryLargePlasmaBomb) },
    { (uword)GS_Laser, strdollar(GS_Laser) },
    { (uword)GS_BigGun, strdollar(GS_BigGun) },                 // CAT: the exe's table 0x00889da0 has 20 rows
    { (uword)GS_BeastWeapon, strdollar(GS_BeastWeapon) },
    { 0,NULL }
};

static NumStrXlate bullettypeinfo[] =
{
    { (uword)BULLET_Projectile, strdollar(BULLET_Projectile) },
    { (uword)BULLET_PlasmaBomb, strdollar(BULLET_PlasmaBomb) },
    { (uword)BULLET_Beam, strdollar(BULLET_Beam) },
    { 0,NULL }
};

static NumStrXlate missiletypeinfo[] =
{
    { (uword)MISSILE_Regular, strdollar(MISSILE_Regular) },
    { (uword)MISSILE_Mine, strdollar(MISSILE_Mine) },
    { 0,NULL }
};

static NumStrXlate objtypeinfo[] =
{
    { (uword)OBJ_ShipType, "Ship" },
    { (uword)OBJ_BulletType, "Bullet" },
    { (uword)OBJ_AsteroidType, "Asteroid" },
    { (uword)OBJ_NebulaType, "Nebula" },
    { (uword)OBJ_GasType, "GasCloud" },
    { (uword)OBJ_DustType, "DustCloud" },
    { (uword)OBJ_DerelictType, "Derelict" },
    { (uword)OBJ_EffectType, "Effect" },
    { (uword)OBJ_MissileType, "Missile" },
    { 0,NULL }
};

static NumStrXlate SpecialEffectinfo[] =
{
    { (uword)EGT_MINE_WALL_EFFECT, "EGT_MINE_WALL_EFFECT"},
    { (uword)EGT_BURST_CHARING_EFFECT, "EGT_BURST_CHARING_EFFECT"},
    { (uword)EGT_BURST_MUZZLE_EFFECT, "EGT_BURST_MUZZLE_EFFECT"},
    { (uword)EGT_BURST_BULLET_EFFECT, "EGT_BURST_BULLET_EFFECT"},
    { (uword)EGT_BURST_HIT_EFFECT, "EGT_BURST_HIT_EFFECT"},
    { (uword)EGT_BURST_EXPLOSION_EFFECT, "EGT_BURST_EXPLOSION_EFFECT"},
    { (uword)EGT_CLOAK_ON      , "EGT_CLOAK_ON"},
    { (uword)EGT_CLOAK_OFF     , "EGT_CLOAK_OFF"},
    { (uword)EGT_GRAVWELL_ON   , "EGT_GRAVWELL_ON"},
    { (uword)EGT_GRAVWELL_OFF  , "EGT_GRAVWELL_OFF"},
    { (uword)EGT_LIGHTNING_GLOW, "EGT_LIGHTNING_GLOW"},
    { (uword)EGT_BULLET_DEFLECT, "EGT_BULLET_DEFLECT"},
    { (uword)EGT_CRATE_GENERATED, "EGT_CRATE_GENERATED"},
    { (uword)EGT_CRATE_IS_FOUND_SHIP, "EGT_CRATE_IS_FOUND_SHIP"},
    { (uword)EGT_CRATE_IS_FOUND_MONEY, "EGT_CRATE_IS_FOUND_MONEY"},
    { (uword)EGT_CRATE_IS_FOUND_RESEARCH, "EGT_CRATE_IS_FOUND_RESEARCH"},
    { (uword)EGT_CRATE_TIME_OUT, "EGT_CRATE_TIME_OUT"},
    { (uword)EGT_REPAIR_BEAM, "EGT_REPAIR_BEAM"},
    { (uword)EGT_CAUGHT_GRAVWELL, "EGT_CAUGHT_GRAVWELL"},
    // CAT: the exe's rows 19-42 in its order (0x0088a0e0-0x0088a198); EGT_LINKING_EFFECT is listed twice there
    { (uword)EGT_WELDING_EFFECT, "EGT_WELDING_EFFECT"},
    { (uword)EGT_LINKING_EFFECT, "EGT_LINKING_EFFECT"},
    { (uword)EGT_RESOURCE_SOLID, "EGT_RESOURCE_SOLID"},
    { (uword)EGT_VORTEX_EFFECT, "EGT_VORTEX_EFFECT"},
    { (uword)EGT_BIG_GUN_BULLET_EXPLOSION, "EGT_BIG_GUN_BULLET_EXPLOSION"},
    { (uword)EGT_BIG_GUN_CHARGING, "EGT_BIG_GUN_CHARGING"},
    { (uword)EGT_ELECTRO_MAGNETIC_PULSE, "EGT_ELECTRO_MAGNETIC_PULSE"},
    { (uword)EGT_SUCK_SHIP, "EGT_SUCK_SHIP"},
    { (uword)EGT_MOTHER_EXPLOSION_SHOCKWAVE, "EGT_MOTHER_EXPLOSION_SHOCKWAVE"},
    { (uword)EGT_DISTORTION_FIELD, "EGT_DISTORTION_FIELD"},
    { (uword)EGT_AFTER_BURNER, "EGT_AFTER_BURNER"},
    { (uword)EGT_RAM_VENT, "EGT_RAM_VENT"},
    { (uword)EGT_CRYSTAL_SHOCK_WAVE, "EGT_CRYSTAL_SHOCK_WAVE"},
    { (uword)EGT_BULLET_CUTTING_BEAM, "EGT_BULLET_CUTTING_BEAM"},
    { (uword)EGT_RESOURCE_SHIP, "EGT_RESOURCE_SHIP"},
    { (uword)EGT_SHOCKWAVE_STAR, "EGT_SHOCKWAVE_STAR"},
    { (uword)EGT_LEECH_COLLECT_EFFECT, "EGT_LEECH_COLLECT_EFFECT"},
    { (uword)EGT_LEECH_VENT_EFFECT, "EGT_LEECH_VENT_EFFECT"},
    { (uword)EGT_LINKING_EFFECT, "EGT_LINKING_EFFECT"},
    { (uword)EGT_SLIPGATE_OPENING_EFFECT, "EGT_SLIPGATE_OPENING_EFFECT"},
    { (uword)EGT_SLIPGATE_CLOSING_EFFECT, "EGT_SLIPGATE_CLOSING_EFFECT"},
    { (uword)EGT_SLIPGATE_PERMANENT_EFFECT, "EGT_SLIPGATE_PERMANENT_EFFECT"},
    { (uword)EGT_RAMMING_FRIGATE_IMPACT_EFFECT, "EGT_RAMMING_FRIGATE_IMPACT_EFFECT"},
    { (uword)EGT_WORKER_RESOURCE_SOLID, "EGT_WORKER_RESOURCE_SOLID"},

    { 0,NULL }
};

static NumStrXlate tacticsinfo[] =
{
    { (uword)Evasive, strdollar(Evasive) },
    { (uword)Neutral, strdollar(Neutral) },
    { (uword)Aggressive, strdollar(Aggressive) },
    { 0,NULL }
};

/*=============================================================================
    Functions:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : RaceBitsToRace
    Description : Converts bit positions representing race (e.g. RACE1_VALID)
                  to race enumeration (e.g. R1)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
ShipRace RaceBitsToRace(uword raceBits)
{
    if (raceBits & R1_VALID)
    {
        return R1;
    }
    else if (raceBits & R2_VALID)
    {
        return R2;
    }
    else if (raceBits & P1_VALID)
    {
        return P1;
    }
    else if (raceBits & P2_VALID)
    {
        return P2;
    }
    else if (raceBits & P3_VALID)
    {
        return P3;
    }
    else if (raceBits & Traders_VALID)
    {
        return Traders;
    }

    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : NumToStr
    Description : Converts a DEFINE to a string
    Inputs      : numstrtab - table cross-referencing numbers and strings
                  num - number to convert
    Outputs     :
    Return      : pointer to converted string if found, NULL otherwise
----------------------------------------------------------------------------*/
char *NumToStr(NumStrXlate numstrtab[],uword num)
{
    uword i;
    NumStrXlate *curnumstr;

    for (i=0,curnumstr=numstrtab;curnumstr->str != NULL;i++,curnumstr++)
    {
        if (num == curnumstr->number)
        {
            return curnumstr->str;
        }
    }
    return NULL;
}

/*-----------------------------------------------------------------------------
    Name        : StrToNum
    Description : Converts a string to a DEFINE
    Inputs      : numstrtab - table cross-referencing numbers and strings
                  str - string to convert
    Outputs     :
    Return      : number if converted correctly, -1 otherwise
----------------------------------------------------------------------------*/
sdword StrToNum(NumStrXlate numstrtab[],char *str)
{
    sdword i;
    NumStrXlate *curnumstr;

    for (i=0,curnumstr=numstrtab;curnumstr->str != NULL;i++,curnumstr++)
    {
        if (strcasecmp(str,curnumstr->str) == 0)
        {
            return curnumstr->number;
        }
    }
    return -1;
}

/*=============================================================================
    The following functions convert between ShipClass, ShipRace, ShipType
    and the strings representing them and vice-versa.
=============================================================================*/

char *TacticsTypeToStr(TacticsType tactics)
{
    return NumToStr(tacticsinfo,(uword)tactics);
}

TacticsType StrToTacticsType(char *str)
{
    return StrToNum(tacticsinfo,str);
}

char *ShipClassToStr(ShipClass shipclass)
{
    return NumToStr(shipclassinfo,(uword)shipclass);
}

char *ShipClassToNiceStr(ShipClass shipclass)
{
    return(strGetString((uword) shipclass+strClassOffset));
}

char *ShipRaceToStr(ShipRace shiprace)
{
    return NumToStr(shipraceinfo,(uword)shiprace);
}

char *ShipTypeToStr(ShipType shiptype)
{
    return NumToStr(shiptypeinfo,(uword)shiptype);
}

char *ShipStatToNiceStr(ShipStatsType stattype)
{
    // There are 6 stat types for each ship type in the ship view stat window
    return(strGetString((uword) (stattype + strShipStatNameOffset)));
}

/*
char *ShipTypeStatToNiceStr(ShipType shiptype, ShipStatsType stattype)
{
    // There are 6 stat types for each ship type in the ship view stat window
    return(strGetString((uword) ((shiptype * 6 + stattype) + strShipStatOffset)));
}
*/

ShipRace StrToNisRace(char *string)
{
    return((ShipRace)StrToNum(nisraceinfo, string));
}

char *NisRaceToStr(ShipRace race)
{
    return(NumToStr(nisraceinfo, (uword)race));
}

char *ShipTypeToNiceStr(ShipType shiptype)
{
    return(strGetString((uword) shiptype + strShipOffset));
}

char *GunTypeToStr(GunType guntype)
{
    return NumToStr(guntypeinfo,(uword)guntype);
}

char *GunSoundTypeToStr(GunSoundType gunsoundtype)
{
    return NumToStr(gunsoundtypeinfo,(uword)gunsoundtype);
}

char *DockPointTypeToStr(DockPointType dockpointtype)
{
    return NumToStr(dockpointtypeinfo,(uword)dockpointtype);
}

char *ExplosionTypeToStr(EffectType type)
{
    return NumToStr(explosiontypeinfo,(uword)type);
}

char *AsteroidTypeToStr(AsteroidType asteroidtype)
{
    return NumToStr(asteroidtypeinfo,(uword)asteroidtype);
}

char *DustCloudTypeToStr(DustCloudType dustcloudtype)
{
    return NumToStr(dustcloudtypeinfo,(uword)dustcloudtype);
}

char *GasCloudTypeToStr(GasCloudType gascloudtype)
{
    return NumToStr(gascloudtypeinfo,(uword)gascloudtype);
}

char *NebulaTypeToStr(NebulaType nebulatype)
{
    return NumToStr(nebulatypeinfo, (uword)nebulatype);
}

char *DerelictTypeToStr(DerelictType derelicttype)
{
    return NumToStr(derelicttypeinfo,(uword)derelicttype);
}

char *BulletTypeToStr(BulletType bulletType)
{
    return NumToStr(bullettypeinfo,(uword)bulletType);
}

char *MissileTypeToStr(MissileType missileType)
{
    return NumToStr(missiletypeinfo,(uword)missileType);
}

char *NAVLightTypeToStr(NAVLightType navlightType)
{
    return(NumToStr(navlighttypeinfo, (uword)navlightType));
}

char *ObjTypeToStr(ObjType objtype)
{
    return(NumToStr(objtypeinfo,(uword)objtype));
}

ShipClass StrToShipClass(char *str)
{
    return StrToNum(shipclassinfo,str);
}

ShipRace StrToShipRace(char *str)
{
    return StrToNum(shipraceinfo,str);
}

SpecialEffectType StrToEffectNum(char *str)
{
    return StrToNum(SpecialEffectinfo,str);
}

ShipType StrToShipType(char *str)
{
    return StrToNum(shiptypeinfo,str);
}

AsteroidType StrToAsteroidType(char *str)
{
    return StrToNum(asteroidtypeinfo,str);
}

DustCloudType StrToDustCloudType(char *str)
{
    return StrToNum(dustcloudtypeinfo, str);
}

GasCloudType StrToGasCloudType(char *str)
{
    return StrToNum(gascloudtypeinfo, str);
}

NebulaType StrToNebulaType(char *str)
{
    return StrToNum(nebulatypeinfo, str);
}

DerelictType StrToDerelictType(char *str)
{
    return StrToNum(derelicttypeinfo,str);
}

EffectType StrToExplosionType(char *str)
{
    return StrToNum(explosiontypeinfo,str);
}

GunType StrToGunType(char *str)
{
    return StrToNum(guntypeinfo,str);
}

GunSoundType StrToGunSoundType(char *str)
{
    return StrToNum(gunsoundtypeinfo,str);
}

DockPointType StrToDockPointType(char *str)
{
    return StrToNum(dockpointtypeinfo,str);
}

// CAT: WEAPON_* name -> value, -1 if unknown (cat_StrToWeaponType 0x004d7130)
sdword cat_StrToWeaponType(char *str)
{
    return StrToNum(weapontypeinfo,str);
}

// CAT: GUNSIZE_* name -> value, -1 if unknown; case-insensitive like every StrToNum (cat_StrToGunSize 0x004d70f0;
//      six shipped guns spell GUNSIZE_Medium)
sdword cat_StrToGunSize(char *str)
{
    return StrToNum(gunsizeinfo,str);
}

BulletType StrToBulletType(char *str)
{
    return StrToNum(bullettypeinfo,str);
}

MissileType StrToMissileType(char *str)
{
    return StrToNum(missiletypeinfo,str);
}

NAVLightType StrToNAVLightType(char *str)
{
    return StrToNum(navlighttypeinfo, str);
}

ObjType StrToObjType(char *str)
{
    return StrToNum(objtypeinfo, str);
}

