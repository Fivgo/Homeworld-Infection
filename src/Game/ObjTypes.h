/*=============================================================================
    Name    : Objtypes.h
    Purpose : Enumerations of object types, ships, race, class, etc.

    Created 7/8/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___OBJTYPES_H
#define ___OBJTYPES_H

#include "RaceDefs.h"
#include "ShipDefs.h"
#include "Types.h"

/*=============================================================================
    Types:
=============================================================================*/

// CAT: Cataclysm's 35 dock point types (dockpointtypeinfo 0x00889c48, StrToDockPointType 0x004d7030). HW1's
//      DockingCone/LatchPoint/LaunchPoint are not in the exe's table (a .shp line naming them gets -1, as in the exe);
//      they are kept above the Cataclysm range so the unported HW1 dock code compiles and stays dormant (stage 2).
typedef enum
{
    DPT_Small_In,
    DPT_Small_Out,
    DPT_Small_In_Engineering,
    DPT_Small_Out_Engineering,
    DPT_Fighter_Latch,
    DPT_Corvette_Latch,
    DPT_Med_In,
    DPT_Med_Out,
    DPT_Med_In_Engineering,
    DPT_Med_Out_Engineering,
    DPT_Large_In,
    DPT_Large_Out,
    DPT_Leech,
    DPT_Leech_RU_Transfer,
    DPT_Worker_RU_Transfer,
    DPT_Tow,
    DPT_Link,
    DPT_Repairbot,
    DPT_Destroyer_EB,
    DPT_Module_EB,
    DPT_Support_EB,
    DPT_Swarmer_In,
    DPT_Swarmer_Out,
    DPT_Crystal_Process,
    DPT_Crystal_Effect,
    DPT_ForceFieldOrigin,
    DPT_Resourcer_RU_Transfer,
    DPT_Salvage,
    DPT_SalvageLatch,
    DPT_SalvageProcess,
    DPT_RKOWeaponSource,
    DPT_AutoRepair,
    DPT_CleesanBoarding,
    DPT_Mothership_In,
    DPT_Mothership_Out,
    NUM_DOCKPOINT_TYPES,

    DockingCone = 100,      // HW1 only, dormant
    LatchPoint,             // HW1 only, dormant
    LaunchPoint,            // HW1 only, dormant
} DockPointType;

typedef enum
{
    Heading,
    Up,
    AttachPoint,
} SalvagePointType;

typedef enum
{
    GUN_Fixed,
    GUN_Gimble,
    GUN_NewGimble,
    GUN_MissileLauncher,
    GUN_MineLauncher
} GunType;

// CAT: Cataclysm's gimbal type names (guntypeinfo 0x00889c08): GUN_HiddenGimble = 1 and GUN_VisibleGimble = 2, i.e.
//      the same values as GUN_Gimble and GUN_NewGimble; 246 of the 271 shipped guns use them
#define GUN_HiddenGimble    GUN_Gimble
#define GUN_VisibleGimble   GUN_NewGimble

// CAT: gun upgrade class, keys StartUpgradeLevel/EndUpgradeLevel (cat_weapontypeinfo 0x00889ea8)
typedef enum
{
    WEAPON_PROJECTILE,
    WEAPON_ENERGY,
    WEAPON_ION,
    WEAPON_PLASMABOMB,
    WEAPON_LASER,
    WEAPON_MISSILE,
    WEAPON_MINE,
    WEAPON_BIGGUN,
    WEAPON_BEASTINFECT,
    NUM_WEAPON_TYPES
} WeaponType;

// CAT: gun size class, key GunSize (gunsizeinfo 0x00889e80)
typedef enum
{
    GUNSIZE_SMALL,
    GUNSIZE_MEDIUM,
    GUNSIZE_LARGE,
    GUNSIZE_VERYLARGE,
    NUM_GUN_SIZES
} GunSize;

typedef enum
{
    NAVLIGHT_Default,
} NAVLightType;

typedef enum
{
    GS_LargeEnergyCannon,
    GS_LargeIonCannon,
    GS_LargePlasmaBomb,
    GS_LargeProjectile,
    GS_MediumEnergyCannon,
    GS_MediumIonCannon,
    GS_MediumPlasmaBomb,
    GS_MediumProjectile,
    GS_MineLauncher,
    GS_MissleLauncher,
    GS_SmallEnergyCannon,
    GS_SmallIonCannon,
    GS_SmallPlasmaBomb,
    GS_SmallProjectile,
    GS_VeryLargeEnergyCannon,
    GS_VeryLargeIonCannon,
    GS_VeryLargePlasmaBomb,
    GS_Laser,
    NUM_GUN_SOUND_TYPES
} GunSoundType;

typedef enum
{
    BULLET_Projectile,
    BULLET_PlasmaBomb,
    BULLET_Beam,
    BULLET_Laser,
    BULLET_SpecialBurst
} BulletType;

typedef enum
{
    MISSILE_Regular,
    MISSILE_Mine
} MissileType;

typedef enum
{
    OBJ_ShipType,
    OBJ_BulletType,
    OBJ_AsteroidType,
    OBJ_NebulaType,
    OBJ_GasType,
    OBJ_DustType,
    OBJ_DerelictType,
    OBJ_EffectType,
    OBJ_MissileType
} ObjType;

// CLASS enumerations   (changed to defines for speech tool in ClassDefs.h)
typedef udword ShipClass;

// RACE enumerations    (changed to defines for speech tool in RaceDefs.h)
typedef udword ShipRace;

#define RaceToRaceBits(race) (1<<(race))

// RACE bits
#define R1_VALID            (1<<R1)
#define R2_VALID            (1<<R2)
#define P1_VALID            (1<<P1)
#define P2_VALID            (1<<P2)
#define P3_VALID            (1<<P3)
#define Traders_VALID       (1<<Traders)

//special 'races' for different types of objects besides ships
#define NSR_Asteroid                (NUM_RACES + 0)
#define NSR_DustCloud               (NUM_RACES + 1)
#define NSR_GasCloud                (NUM_RACES + 2)
#define NSR_Nebula                  (NUM_RACES + 3)
#define NSR_Derelict                (NUM_RACES + 4)
#define NSR_Effect                  (NUM_RACES + 5)
#define NSR_Generic                 (NUM_RACES + 6)
#define NSR_Missile                 (NUM_RACES + 7)
#define NSR_LastNSR                 (NSR_Missile + 1)

// moved ship defines to ShipDefs.h

typedef enum
{
    Asteroid0,
    Asteroid1,
    Asteroid2,
    Asteroid3,
    Asteroid4,
    NUM_ASTEROIDTYPES
} AsteroidType;

typedef enum
{
    DustCloud0,
    DustCloud1,
    DustCloud2,
    DustCloud3,
    NUM_DUSTCLOUDTYPES
} DustCloudType;

typedef enum
{
    GasCloud0,
    GasCloud1,
    NUM_GASCLOUDTYPES
} GasCloudType;

typedef enum
{
    Nebula0,
    NUM_NEBULATYPES
} NebulaType;

// CAT: Cataclysm's 8 derelict types, ids and names as the exe's table derelicttypeinfo 0x00889b68 (read by
//      StrToDerelictType / DerelictTypeToStr and by meshNameToStaticInfo 0x004ae7b0); HW1 had 55. NUM_DERELICTTYPES 8
//      sizes the per-type tables (derelictStaticInfos, etgDeathEventTableDerelict ...). NIS generic objects keep using
//      ids NUM_DERELICTTYPES + n (NIS.c), as in HW1.
typedef enum
{
    Crate                       = 0,
    Homeworld                   = 1,
    PlanetOfOrigin              = 2,
    PlanetOfOrigin_scarred      = 3,
    Galaxy                      = 4,
    LavaBall                    = 5,
    MattWorld                   = 6,
    HyperspaceGate              = 7,    //hyperspace gate dummy derelict
    NUM_DERELICTTYPES           = 8,

    // CAT: HW1 derelict names Cataclysm dropped, kept only so that HW1 code not yet ported compiles (switch cases in
    //      Select.c, ShipSelect.c, Sensors.c). Their ids are far above every Cataclysm and NIS id and no table maps a
    //      name to them, so code keyed on them is dormant (as ShipDefs.h does for the 22 dropped ship types). Never
    //      index a NUM_DERELICTTYPES table with them.
    HW1LEGACY_FIRST_DERELICT    = 0x1000,
    AngelMoon                   = HW1LEGACY_FIRST_DERELICT,
    AngelMoon_clean,
    FragmentPanel0a,
    FragmentPanel0b,
    FragmentPanel0c,
    FragmentPanel1,
    FragmentPanel2,
    FragmentPanel3,
    FragmentStrut,
    LifeBoat,
    PrisonShipOld,
    PrisonShipNew,
    Scaffold,
    Scaffold_scarred,
    ScaffoldFingerA_scarred,
    ScaffoldFingerB_scarred,
    Shipwreck,
    JunkAdvanceSupportFrigate,
    JunkCarrier,
    JunkDDDFrigate,
    JunkHeavyCorvette,
    JunkHeavyCruiser,
    JunkIonCannonFrigate,
    JunkLightCorvette,
    JunkMinelayerCorvette,
    JunkMultiGunCorvette,
    JunkRepairCorvette,
    JunkResourceController,
    JunkSalCapCorvette,
    JunkStandardFrigate,
    Junk0_antenna,
    Junk0_fin1,
    Junk0_fin2,
    Junk0_GunAmmo,
    Junk0_panel,
    Junk0_sensors,
    Junk1_partA,
    Junk1_partB,
    Junk1_shell,
    Junk1_strut,
    Junk2_panelA,
    Junk2_panelB,
    Junk2_panelC,
    Junk2_panelD,
    Junk2_shipwreck,
    Junk3_Boiler,
    Junk3_BoilerCasing,
    M13PanelA,
    M13PanelB,
    M13PanelC,
    HW1LEGACY_LAST_DERELICT     = M13PanelC
} DerelictType;

typedef enum
{
    ET_Explosion = 0,           //generic explosion
    ET_LightLaserHit,           //weapon impacts
    ET_MedLaserHit,
    ET_HeavyLaserHit,
    ET_PlasmaBombHit,

    ET_InterceptorExplosion,    //ship explosions
    ET_CorvetteExplosion,
    ET_FrigateExplosion,
    ET_CarrierExplosion,
    ET_ResourcerExplosion,

    NUM_EFFECT_TYPES            // must be last
}
EffectType;

typedef enum
{
    Evasive,
    Neutral,
    Aggressive,

    NUM_TACTICS_TYPES
}
TacticsType;

typedef enum
{
    Firepower = 0,
    Coverage,
    Maneuver,
    Armor,
    TopSpeed,
    Mass,
    VeryLow,
    Low,
    Medium,
    High,
    VeryHigh,
    CoverageUnits,

    NUM_SHIP_STATS
}
ShipStatsType;

typedef struct
{
    uword number;
    char *str;
} NumStrXlate;

typedef udword SpecialEffectType;
//special effect var defines
#define EGT_MINE_WALL_EFFECT        0
#define EGT_BURST_CHARING_EFFECT    1
#define EGT_BURST_MUZZLE_EFFECT     2
#define EGT_BURST_BULLET_EFFECT     3
#define EGT_BURST_HIT_EFFECT        4
#define EGT_BURST_EXPLOSION_EFFECT  5   //bullet explosion
#define EGT_CLOAK_ON                6
#define EGT_CLOAK_OFF               7
#define EGT_GRAVWELL_ON             8
#define EGT_GRAVWELL_OFF            9
#define EGT_LIGHTNING_GLOW         10
#define EGT_BULLET_DEFLECT         12
#define EGT_CRATE_GENERATED        13
#define EGT_CRATE_IS_FOUND_SHIP    14
#define EGT_CRATE_IS_FOUND_MONEY   15
#define EGT_CRATE_IS_FOUND_RESEARCH 16
#define EGT_CRATE_TIME_OUT         17
#define EGT_REPAIR_BEAM            18
#define EGT_CAUGHT_GRAVWELL        19
#define EGT_NumberOfSpecialEffects 20

/*=============================================================================
    Data:
=============================================================================*/

extern ShipType FirstShipTypeOfRace[NUM_RACES];
extern ShipType LastShipTypeOfRace[NUM_RACES];
extern uword NumShipTypesInRace[NUM_RACES];

/*=============================================================================
    Support Functions:
=============================================================================*/

char *NumToStr(NumStrXlate numstrtab[],uword num);
sdword StrToNum(NumStrXlate numstrtab[],char *str);

ShipRace RaceBitsToRace(uword raceBits);

char *ShipClassToStr(ShipClass shipclass);
char *ShipClassToNiceStr(ShipClass shipclass);
ShipClass StrToShipClass(char *str);

char *ShipRaceToStr(ShipRace shiprace);
ShipRace StrToShipRace(char *str);

char *TacticsTypeToStr(TacticsType tactics);
TacticsType StrToTacticsType(char *str);

SpecialEffectType StrToEffectNum(char *str);

char *ShipTypeToStr(ShipType shiptype);
char *ShipTypeToNiceStr(ShipType shiptype);
ShipType StrToShipType(char *str);

char *ShipStatToNiceStr(ShipStatsType stattype);
char *ShipTypeStatToNiceStr(ShipType shiptype, ShipStatsType stattype);

char *AsteroidTypeToStr(AsteroidType asteroidtype);
AsteroidType StrToAsteroidType(char *str);

char *DustCloudTypeToStr(DustCloudType dustcloudtype);
DustCloudType StrToDustCloudType(char *str);

char *GasCloudTypeToStr(GasCloudType gascloudtype);
GasCloudType StrToGasCloudType(char *str);

char *NebulaTypeToStr(NebulaType nebulatype);
NebulaType StrToNebulaType(char *str);

char *DerelictTypeToStr(DerelictType derelicttype);
DerelictType StrToDerelictType(char *str);

char *ExplosionTypeToStr(EffectType type);
EffectType StrToExplosionType(char *str);

char *GunTypeToStr(GunType guntype);
GunType StrToGunType(char *str);

DockPointType StrToDockPointType(char *str);
sdword cat_StrToWeaponType(char *str);      // CAT: cat_StrToWeaponType 0x004d7130
sdword cat_StrToGunSize(char *str);         // CAT: cat_StrToGunSize 0x004d70f0
char *DockPointTypeToStr(DockPointType dockpointtype);

SalvagePointType StrToSalvagePointType(char *str);
char *SalvagePointTypeToStr(SalvagePointType salvagepointtype);

char *GunSoundTypeToStr(GunSoundType gunsoundtype);
GunSoundType StrToGunSoundType(char *str);

char *BulletTypeToStr(BulletType bulletType);
BulletType StrToBulletType(char *str);

char *MissileTypeToStr(MissileType missileType);
MissileType StrToMissileType(char *str);

char *NAVLightTypeToStr(NAVLightType navlightType);
NAVLightType StrToNAVLightType(char *str);

char *ObjTypeToStr(ObjType objtype);
ObjType StrToObjType(char *str);

char *NisRaceToStr(ShipRace race);
ShipRace StrToNisRace(char *string);

#endif

