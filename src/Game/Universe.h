/*=============================================================================
    Name    : Universe.h
    Purpose : Definitions for Universe.c

    This module will be able to create mission spheres, and the universe
    conceptually contains all of the mission spheres in a game.

    It will also contain information global to all mission spheres, such as the
    static properties of the different ship types.

    Created 6/19/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___UNIVERSE_H
#define ___UNIVERSE_H

#include "CameraCommand.h"
#include "CommandLayer.h"
#include "ConsMgr.h"
#include "GameStats.h"
#include "ResearchAPI.h"
#include "Star3d.h"
#include "Task.h"

/*=============================================================================
    Switches:
=============================================================================*/
#ifdef HW_BUILD_FOR_DEBUGGING

#define UNIV_SHIP_LOADFREE_LOG          1       //spit out a list of what ships were loaded and freed
#define UNIV_LOAD_FREE_FILENAME         "loadfree.log"

#else

#define UNIV_SHIP_LOADFREE_LOG          0       //spit out a list of what ships were loaded and freed

#endif

/*=============================================================================
    Defines:
=============================================================================*/

#define UNIVERSE_UPDATE_RATE_ORIG   16          // original update rate
extern ubyte UNIVERSE_UPDATE_RATE_SHIFT;          // define multiplier in shift values (e.g. 2^n) to preserve original timing
                                                // CAT: 0 (16 updates per second, as Relic HW1 and Cataclysm) by default, see Universe.c;
                                                // GoK's 2 (64 Hz) only with the /gok64Hz command line option.
#define UNIVERSE_UPDATE_RATE_FACTOR (1<<UNIVERSE_UPDATE_RATE_SHIFT)
#define UNIVERSE_UPDATE_RATE (UNIVERSE_UPDATE_RATE_ORIG*UNIVERSE_UPDATE_RATE_FACTOR)
#define UNIVERSE_UPDATE_PERIOD (1.0f/(real32)UNIVERSE_UPDATE_RATE)
#define UNIVERSE_UPDATE_STACK_SIZE  100000

// macro to adapt the original update rate to the higher one
#define UNIVERSE_WOODPECKER(r, f) (((universe.univUpdateCounter % UNIVERSE_UPDATE_RATE_FACTOR) == 0) && (((universe.univUpdateCounter >> UNIVERSE_UPDATE_RATE_SHIFT) & r) == (f)))

#define RENDER_STACK_SIZE 100000

#define UNIV_NUMBER_WORLDS          3           //number of planets we can keep track of

/*=============================================================================
    Types:
=============================================================================*/

#define MAXNUM_RESOURCETYPES   10

// for regrowing resources
typedef struct ResourceDistribution
{
    uword probResources[MAXNUM_RESOURCETYPES][2];
    uword cumResources[MAXNUM_RESOURCETYPES][2];
    uword sumTotal[2];
} ResourceDistribution;

typedef enum ResourceVolumeType {
    ResourceSphereType,
    ResourceCylinderType,
    ResourceRectangleType
} ResourceVolumeType;

typedef struct ResourceVolume
{
    Node link;
    ResourceVolumeType resourceVolumeType;
    vector centre;
    real32 radius;
    sdword number;
    sdword actualnumber;
    udword attributes;
    sword attributesParam;
    ubyte attributesPad[2];
    ObjType resourceObjType;
    ResourceDistribution resourceDistribution;

    real32 length;      // only valid for ResourceCylinderType and ResourceRectangleType
    real32 roty;
    real32 rotz;
} ResourceVolume;

typedef enum PlayerState
{
    PLAYER_ALIVE = 0,
    PLAYER_DEAD
} PlayerState;

typedef struct Player
{
    Ship *PlayerMothership;             /*+0x000*/
    sdword resourceUnits;               /*+0x004*/
    // CAT (M2-R4 rest-oracle): the Cataclysm Player is 0x6530 bytes (PreLoadGame's stride, universe.players 0x00b67500
    //      to 0x00b9bfd0); the offsets are the exe's. The three dwords after resourceUnits are CET's (universe.h): +0x8
    //      has no reader in the r6 export, +0xC and +0x10 are the support-unit budget (cmUpdateShipAvailable
    //      0x00455410; statsGetStatChecksum 0x00525170 adds +0xC)
    udword catUnknown08;                /*+0x008 CAT: no reader found in the export (CET: unk) */
    sdword supportUnitsMax;             /*+0x00C CAT */
    sdword supportUnitsUsed;            /*+0x010 CAT */
    ShipRace race;                      /*+0x014*/
    PlayerState playerState;            /*+0x018*/
    struct AIPlayer *aiPlayer;          /*+0x01C if non-null, then player has a computer AI Player */
    udword autoLaunch;                  /*+0x020*/
    uword playerIndex;                  /*+0x024*/
    ubyte sensorLevel;                  /*+0x026*/
    ubyte bounty;                       /*+0x027 range from 0 to 100 */
    real32 timeMoShipAttacked;          /*+0x028*/
    sdword totalships;                  /*+0x02C total ships for player */
    sdword shiptotals[TOTAL_NUM_SHIPS]; /*+0x030 totals for each type of ship (145, crc32Compute(player+0x30, 0x244)) */
    sdword classtotals[NUM_CLASSES];    /*+0x274*/
    PlayerResearchInfo researchinfo;    /*+0x2A4 research information for player (0x6250 bytes, ResearchAPI.h) */
    uword Allies;                       /*+0x64F4 bitmask of players that i am allied with (allianceIsShipAlly 0x00420ec0) */
    uword AllianceProposals;            /*+0x64F6*/
    udword AllianceProposalsTimeout;    /*+0x64F8 timeout counter for proposals to expire! */
    sdword initialShipCost;             /*+0x64FC total ship cost at game start (gameStatsCalcShipCostTotals 0x0053f500) */
    // CAT: the player's fog-of-war grid (cat_playerGetFOWGrid 0x00480d10; created by cat_fowCreatePlayerGrids 0x004a38c0,
    //      destroyed in universeReset 0x0053df80); NULL until stage 3 ports the FOW
    void *catFOWGrid;                   /*+0x6500*/
    // CAT: the upgrade state the player's research gives its ships (cat_playerUpdateUpgradesFromResearch 0x00514140,
    //      cat_playerUpgradesReset 0x00514100): the armour level cap (0: none, 1: TECH_ARMOUR2, 2: TECH_ARMOUR3), which
    //      of the 9 weapon types are available (index = WEAPON_*; [0] projectile is always 1) and the CAT_UPGRADE_*
    //      bits of the technologies the player has
    sdword catArmourLevelCap;           /*+0x6504*/
    sdword catWeaponAvailable[9];       /*+0x6508*/
    udword catUpgradeBits;              /*+0x652C*/
} Player;

typedef struct
{
    CommandLayer mainCommandLayer;      // everything goes through main command layer
    CameraCommand mainCameraCommand;    // camera command (focus stack)

    LinkedList RenderList;      // Linked list of all objects (SpaeceObj's to be rendered)
    LinkedList MinorRenderList;
    bool32 dontUpdateRenderList;
    // CAT (M2-R4 rest-oracle): universe+0xB8 (CET: unknown1): set by smSensorsBegin 0x0050cdb0, cleared by
    //      smSensorsCloseForGood 0x0050ba90 and univUpdateReset 0x0054acd0; universeUpdateTask 0x0053e460 skips
    //      univUpdateRenderList while it is set. Name chosen here.
    bool32 catSensorsHoldRenderList;

    real32 phystimeelapsed;             /*+0x0BC*/
    real32 totaltimeelapsed;            /*+0x0C0*/
    // CAT: wall-clock seconds of game play (universe.realtimeelapsed 0x00B62AC4, right after totaltimeelapsed 0x00B62AC0):
    //      univUpdate 0x0054c3f0 adds QueryPerformanceCounter time each tick, univUpdateReset 0x0054acd0 zeroes it; the
    //      quit/win timers and ~20 other functions read it (docs/m2-checksum-oracle.md 2.5; readers are ported with them)
    real32 realtimeelapsed;             /*+0x0C4*/

    real32 radius;                      /*+0x0C8*/

    LinkedList SpaceObjList;            /*+0x0CC*/
    LinkedList MinorSpaceObjList;
    LinkedList effectList;              /*+0x0E4*/
    LinkedList ShipList;                /*+0x0F0*/
    LinkedList BulletList;
    LinkedList ResourceList;
    LinkedList DerelictList;
    LinkedList ImpactableList;
    LinkedList MissileList;
    LinkedList MineFormationList;

    LinkedList DeleteResourceList;
    LinkedList DeleteDerelictList;
    LinkedList DeleteShipList;
    LinkedList DeleteMissileList;

    LinkedList collBlobList;            /*+0x174*/

    // tactics stuff
    // CAT: one tactics list (universe+0x180): its atoms are HW1's AttackAtoms (SaveRetreatAtom 0x00503290 saves 0x24 +
    //      4 x numShips bytes: Node, retreater +0x10, two reals, SelectCommand attackerList at +0x1C;
    //      tacticsShipDied 0x00529150 and tacticsMakeShipsNotLookForOtherShips 0x005291b0 walk it as attack memory).
    //      HW1's RetreatList (RetreatAtoms) is gone; CET named this list RetreatList
    LinkedList AttackMemory;            /*+0x180*/

    // regrow resources stuff
    LinkedList ResourceVolumeList;      /*+0x18C*/

    GrowSelection HousekeepShipList;    /*+0x198*/
    // CAT: a second universe grow-selection (CET: unk1), saved after HousekeepShipList (SaveUniverse 0x005034c0),
    //      reset with it (univUpdateReset 0x0054acd0). Name chosen here; its users come with their files.
    GrowSelection catSelection1A0;      /*+0x1A0*/

    Star3dInfo *star3dinfo;             /*+0x1A8*/
    udword univUpdateCounter;           /*+0x1AC*/
    color backgroundColor;
    uword resourceNumber;
    uword derelictNumber;
    uword shipNumber;
    uword missileNumber;

    real32 wintime;             /*+0x1BC time to display the win game message */
    real32 quittime;            /*+0x1C0 time to quit the game */

    udword lasttimeadded;       /*+0x1C4 for resource injections */

    uword shipMaxUnits[TOTAL_NUM_SHIPS];    /*+0x1C8*/

    sbyte aiplayerEnemy[MAX_MULTIPLAYER_PLAYERS];   /*+0x2EA*/
    bool32  aiplayerProcessing;                     /*+0x2F4*/

    sdword CapitalShipCaptured;     /*+0x2F8 bit field of players who have had a ship captured */
    sdword PlayerWhoWon;            /*+0x2FC bit field of players who have captured a enemy's ship */

    sdword bounties;                /*+0x300*/
    sdword bountySize;              /*+0x304*/

    GameStats gameStats;            /*+0x308 (0x47E0 bytes, GameStats.h) */
    bool32 DerelictTech;            /*+0x4AE8 variable that needs to be saved that controls the salvagability of technology holding derelicts */

    real32 crateTimer;              /*+0x4AEC*/
    udword numCratesInWorld;        /*+0x4AF0*/

    bool32 collUpdateAllBlobs;      /*+0x4AF4 put in here so it gets saved with SaveGame */

    uword numPlayers;               /*+0x4AF8*/
    uword curPlayerIndex;           /*+0x4AFA*/
    Player *curPlayerPtr;           /*+0x4AFC*/
    Player players[MAX_MULTIPLAYER_PLAYERS+1];      /*+0x4B00 players[MAX_MULTIPLAYER_PLAYER] is a enemy of all players and used for autoguns, etc. */
    Derelict *world[UNIV_NUMBER_WORLDS];        /*+0x3D9B0 list of planets (univAddToWorldList: 3 entries at 0x00ba03b0) */
} Universe;
// CAT (M2-R4 rest-oracle): the Universe (0x3D9BC, the SaveUniverse chunk 0x005034c0 and cdLimitCaps at 0x00ba03c0 right
//      after universe 0x00b62a00) and Player (0x6530) layouts are pinned under ILP32 (tools/m2_layout32.sh); the
//      offsets are CET universe.h's asserts plus the exe reads listed in docs/m2-r4-rest-oracle.md "layout"
_Static_assert(offsetof(Universe, realtimeelapsed) == offsetof(Universe, totaltimeelapsed) + sizeof(real32),
               "realtimeelapsed follows totaltimeelapsed (universe 0x00B62AC4 in cataclysm.exe)");
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(Player) == 0x6530, "Player is 0x6530 bytes in cataclysm.exe");
_Static_assert(offsetof(Player, supportUnitsMax) == 0xC, "Player.supportUnitsMax at 0xC");
_Static_assert(offsetof(Player, race) == 0x14, "Player.race at 0x14");
_Static_assert(offsetof(Player, aiPlayer) == 0x1C, "Player.aiPlayer at 0x1C");
_Static_assert(offsetof(Player, playerIndex) == 0x24, "Player.playerIndex at 0x24");
_Static_assert(offsetof(Player, totalships) == 0x2C, "Player.totalships at 0x2C");
_Static_assert(offsetof(Player, shiptotals) == 0x30, "Player.shiptotals at 0x30");
_Static_assert(offsetof(Player, classtotals) == 0x274, "Player.classtotals at 0x274");
_Static_assert(offsetof(Player, researchinfo) == 0x2A4, "Player.researchinfo at 0x2A4");
_Static_assert(offsetof(Player, Allies) == 0x64F4, "Player.Allies at 0x64F4");
_Static_assert(offsetof(Player, initialShipCost) == 0x64FC, "Player.initialShipCost at 0x64FC");
_Static_assert(offsetof(Player, catFOWGrid) == 0x6500, "Player.catFOWGrid at 0x6500");
_Static_assert(offsetof(Player, catArmourLevelCap) == 0x6504, "Player.catArmourLevelCap at 0x6504");
_Static_assert(offsetof(Player, catWeaponAvailable) == 0x6508, "Player.catWeaponAvailable at 0x6508");
_Static_assert(offsetof(Player, catUpgradeBits) == 0x652C, "Player.catUpgradeBits at 0x652C");
_Static_assert(sizeof(Universe) == 0x3D9BC, "Universe is 0x3D9BC bytes in cataclysm.exe");
_Static_assert(offsetof(Universe, catSensorsHoldRenderList) == 0xB8, "universe+0xB8");
_Static_assert(offsetof(Universe, phystimeelapsed) == 0xBC, "universe.phystimeelapsed at 0xBC");
_Static_assert(offsetof(Universe, totaltimeelapsed) == 0xC0, "universe.totaltimeelapsed at 0xC0");
_Static_assert(offsetof(Universe, effectList) == 0xE4, "universe.effectList at 0xE4");
_Static_assert(offsetof(Universe, ShipList) == 0xF0, "universe.ShipList at 0xF0");
_Static_assert(offsetof(Universe, collBlobList) == 0x174, "universe.collBlobList at 0x174");
_Static_assert(offsetof(Universe, AttackMemory) == 0x180, "universe.AttackMemory (CET: RetreatList) at 0x180");
_Static_assert(offsetof(Universe, HousekeepShipList) == 0x198, "universe.HousekeepShipList at 0x198");
_Static_assert(offsetof(Universe, univUpdateCounter) == 0x1AC, "universe.univUpdateCounter at 0x1AC");
_Static_assert(offsetof(Universe, wintime) == 0x1BC, "universe.wintime at 0x1BC");
_Static_assert(offsetof(Universe, aiplayerEnemy) == 0x2EA, "universe.aiplayerEnemy at 0x2EA");
_Static_assert(offsetof(Universe, bountySize) == 0x304, "universe.bountySize at 0x304");
_Static_assert(offsetof(Universe, gameStats) == 0x308, "universe.gameStats at 0x308");
_Static_assert(offsetof(Universe, numCratesInWorld) == 0x4AF0, "universe.numCratesInWorld at 0x4AF0");
_Static_assert(offsetof(Universe, numPlayers) == 0x4AF8, "universe.numPlayers at 0x4AF8");
_Static_assert(offsetof(Universe, players) == 0x4B00, "universe.players at 0x4B00");
_Static_assert(offsetof(Universe, world) == 0x3D9B0, "universe.world at 0x3D9B0");
#endif

extern GrowSelection ClampedShipList;       // special GrowSelect which can have NULL's in it


typedef void (*univstatcallback)(StaticInfo *info, ObjType objType);

/*=============================================================================
    Public Variables
=============================================================================*/

extern Universe universe;
extern AsteroidStaticInfo asteroidStaticInfos[NUM_ASTEROIDTYPES];
extern DustCloudStaticInfo dustcloudStaticInfos[NUM_DUSTCLOUDTYPES];
extern GasCloudStaticInfo gascloudStaticInfos[NUM_GASCLOUDTYPES];
extern DerelictStaticInfo derelictStaticInfos[NUM_DERELICTTYPES];
extern NebulaStaticInfo nebulaStaticInfos[NUM_NEBULATYPES];

extern Camera defaultCamera;
extern bool32 universePlaybackNoPacket;     // CAT: 0x00a6c5c8 (universeUpdateTask 0x0053e460)

extern MissileStaticInfo missileStaticInfos[NUM_RACES];
extern MissileStaticInfo mineStaticInfos[NUM_RACES];  // CAT: was [2]

extern struct SphereStaticInfo *sphereStaticInfo;

extern real32 maxMineCollSphereSize;
//extern real32 maxShipCollSphereSize;
//extern real32 maxResourceCollSphereSize;
//extern real32 maxDerelictCollSphereSize;
//extern real32 maxCollSphereSize;

extern meshdata *defaultmesh;

extern uword RacesAllowedForGivenShip[TOTAL_NUM_SHIPS];

extern ubyte turboTimeCompressionFactor;
extern bool32 universeTurbo;
extern bool32 universePause;

extern sdword cdMaxShipsAllowed;            // max number of ships allowed
extern sdword cdLimitCaps[TOTAL_NUM_SHIPS]; // max number of ships per player allowed
extern sdword cdClassCaps[NUM_CLASSES];     // max number of ships per class allowed
extern bool32   cdEnabled;                    // flag specifying whether unit caps enabled

extern lodinfo *defaultlod;                 // remove later when all stubs are taken out
extern void *defaultmex;                    // remove later when all stubs are taken out
extern bool32 universeForceDefaultShip;       //flag for forcing ships to be loaded as rave borgs
extern bool32 catShipStaticsSelfTest;          // CAT: /catShipStaticsTest (port diagnostic)
extern bool32 catShipStaticsRunStaticInit;     // CAT: /catShipStaticsStaticInit (port diagnostic, M2-R3 ships-a)
#define CAT_SHIPSTATICS_MAXSKIP 16
extern sdword catShipStaticsSkip[CAT_SHIPSTATICS_MAXSKIP];   // CAT: /catShipStaticsSkip (port diagnostic)
extern sdword catShipStaticsSkipCount;

#if UNIV_SHIP_LOADFREE_LOG
extern bool32 univLoadFreeLog;
#endif

extern real32 rowAvoidBy[TOTAL_NUM_SHIPS];
extern sbyte rightOfWays[TOTAL_NUM_SHIPS];

#ifdef DEBUG_GAME_STATS

typedef struct
{
    sdword numPlayers;
    char names[MAX_MULTIPLAYER_PLAYERS+1][20];
}GameStatsDebugHeader;

void gameStatsInitLog();
void gameStatsUpdateLogFile(sdword framenum);

#endif
/*=============================================================================
    Functions:
=============================================================================*/

void universeClosePlayers(void);
void universeInit(void);
void universeClose(void);
void universeReset(void);
void universeSetZeroPlayers(void);
void universeStaticInit(void);
void cat_univSetShipMaxHealth(struct ShipStaticInfo *statinfo);    // CAT: new (0x0053c4a0)
void universeStaticClose(void);
DECLARE_TASK(universeUpdateTask);
void universeSwitchToNextPlayer(void);
void universeSwitchToPlayer(uword playerIndex);

ShipRace GetValidRaceForShipType(ShipType shiptype);    // returns valid race for shiptype, -1 if none found
ShipStaticInfo *GetShipStaticInfo(ShipType shiptype,ShipRace shiprace);
ShipStaticInfo *GetShipStaticInfoValidate(ShipType shiptype,ShipRace shiprace); // same as above, but makes sure data loaded
ShipStaticInfo *GetShipStaticInfoSafe(ShipType shiptype,ShipRace shiprace); // same as above, but always safe (returns NULL if fails)
void InitStatShipInfo(ShipStaticInfo *statinfo,ShipType type,ShipRace race);
void CloseStatShipInfo(ShipStaticInfo *statinfo);
void CloseStatAsteroidInfo(AsteroidStaticInfo *asteroidStatInfo);
void CloseStatDerelictInfo(DerelictStaticInfo *derelictStatInfo);
void CloseStatDustCloudInfo(DustCloudStaticInfo *dustcloudStatInfo);
void CloseStatGasCloudInfo(GasCloudStaticInfo *gascloudStatInfo);
void CloseStatNebulaInfo(NebulaStaticInfo *nebulaStatInfo);       // CAT: declared for catShipStaticsRunSelfTest
void CloseStatMissileInfo(MissileStaticInfo *missileStatInfo);
void CloseStatMineInfo(MissileStaticInfo *mineStatInfo);
void CloseStatEffectInfo(etgeffectstatic *effectStatInfo);
void InitStatDerelictInfoByPath(DerelictStaticInfo *derelictStatInfo, DerelictType derelicttype, char *directory, char *shipFile);

void unitCapCreateShip(Ship *ship, Player *player);

// CAT: a list of mothership module types (the exe passes sdword[20] plus a count at [20])
typedef struct
{
    ShipType types[20];
    sdword num;
} CatModuleTypeList;
void cat_univMothershipModuleTypesGet(CatModuleTypeList *modules);        // CAT: 0x0053f680
void cat_univBeastMothershipModuleTypesGet(CatModuleTypeList *modules);   // CAT: 0x0053f840
Ship *cat_univFindNearestShip(SpaceObj *from, bool32 (*filter)(Ship *ship));   // CAT: 0x0053f590
bool32 cat_playerHasCompletedShip(ShipType type, Player *player);                 // CAT: 0x0053f620
void *cat_playerGetFOWGrid(Player *player);                                       // CAT: 0x00480d10 (the FOW itself is stage 3)
void unitCapDeleteShip(Ship *ship, Player *player);
bool32 unitCapCanCreateShip(ShipType ship, shipsinprogress *factory, shipavailable *cmShipsAvail);
void unitCapInitialize(Player *player);
void unitCapEnable(void);
void unitCapDisable(void);

void universeRealTimeTweak(SelectCommand *selection);
void universeDefaultTeamColors(void);
extern void universeFlagRaceNeeded(ShipRace shiprace, bool8 bNeeded);
void universeFlagEverythingNeeded(void);
void universeSaveEverythingNeeded(void);
void universeLoadEverythingNeeded(void);
void universeRaceStaticsScan(ShipRace shiprace, univstatcallback forShips, univstatcallback forMisc);
void universeAllStaticsScan(univstatcallback forShips, univstatcallback forDerelicts, univstatcallback forResources, univstatcallback forMisc);

void resourceInjectionTask(void);
void resourceLumpSum(void);

void initGameStats(Universe *universe);
void gameStatsAddShip(Ship *ship,sdword playerIndex);
void writeGameStatsToFile(char *filename);
void gameStatsShipDied(Ship *ship);
void gameStatsCalcShipCostTotals(void);

bool32 isStrikeCraft(ShipType type);
void cat_univInitShipTypeFlags(struct ShipStaticInfo *statinfo);   // CAT: cat_univInitShipTypeFlags 0x0053b8c0
void catShipStaticsRunSelfTest(void);                               // CAT: port diagnostic (/catShipStaticsTest)

void unitCapInitStatics(udword numplayers);

#endif
