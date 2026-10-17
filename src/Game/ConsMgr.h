// =============================================================================
//  ConsMgr.h
//  - construction manager
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 7/18/1997 by lmoloney
// =============================================================================

#ifndef ___CONSMGR_H
#define ___CONSMGR_H

#include "ClassDefs.h"
#include "ObjTypes.h"
#include "Region.h"
#include "ShipDefs.h"
#include "SpaceObj.h"
#include "Task.h"

/*=============================================================================
    Switches:
=============================================================================*/
#ifdef HW_BUILD_FOR_DEBUGGING
    #define CM_ERROR_CHECKING       1    // general error checking
    #define CM_VERBOSE_LEVEL        2    // print extra info
    #define CM_CAP_JOBS_PER_CLASS   0
    #define CM_CHEAP_SHIPS          1    // enable the 'cheapShips' flag
#else
    #define CM_ERROR_CHECKING       0
    #define CM_VERBOSE_LEVEL        0
    #define CM_CAP_JOBS_PER_CLASS   0
    #define CM_CHEAP_SHIPS          0
#endif

/*=============================================================================
    Type definitions:
=============================================================================*/
//structure for a ship under construction
typedef struct
{
    sdword nJobs;
    sdword nJobsTotal;
    sdword timeStart;
    sdword timeLeft;
    sdword timeFraction; // Simulates floating point so that fraction is accounted for in decrement
    sdword selected;
    sdword paused;
    sdword costSoFar;
    ShipStaticInfo *info;
    // CAT (M2-R4 rest-oracle): Cataclysm's entry is 0x30 bytes (cmBuildJobsAdd 0x004586c0 and unitCapCanCreateShip
    //      0x0053ece0 index by type * 0x30; LoadConsMgr 0x004598a0 fixes up +0x20 info and +0x2C ship for 0x91 entries);
    //      the three dwords after info: +0x24 has no reader in the r6 export, +0x28 is tested by unitCapCanCreateShip for
    //      an externally built type (one queued job of the type is already a ship), +0x2C is the ship under construction
    //      (SaveConsMgr 0x00459780 saves it by ID). Names chosen here.
    sdword catUnknown24;
    bool32 catExternalShipStarted;
    struct Ship *catExternalShip;
}
shipinprogress;

#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(shipinprogress) == 0x30, "shipinprogress is 0x30 bytes in cataclysm.exe (cmBuildJobsAdd 0x004586c0)");
_Static_assert(offsetof(shipinprogress, catExternalShipStarted) == 0x28, "shipinprogress +0x28 (unitCapCanCreateShip 0x0053ece0)");
_Static_assert(offsetof(shipinprogress, catExternalShip) == 0x2C, "shipinprogress +0x2C (SaveConsMgr 0x00459780)");
#endif
//structure for all ships under construction at a certain site
typedef struct
{
    Node node;
    struct Ship *ship;
    bool32 canBuildBigShips;
    shipinprogress progress[TOTAL_STD_SHIPS];
}
shipsinprogress;

//ships available
typedef struct
{
    sdword nJobs;
    sbyte  itemtype;
    sbyte  itemstat;
    ShipType type;
    ShipStaticInfo *info;
}
shipavailable;



typedef struct
{
    sword       key;
    ShipType    ship;
}
KeysToShips;


typedef struct
{
    struct Ship   *ship;
    struct Player *owner;
}
CarrierInfo;


/*=============================================================================
    Data:
=============================================================================*/

extern CarrierInfo *cmCarriers;
extern sdword cmNumCarriers;
extern sdword cmNumPlayersCarriers;


#if CM_CHEAP_SHIPS
extern bool32 cmCheapShips;
#endif

#define CM_SHIFT 256

extern KeysToShips cmShipTypes[];
extern bool32 cmPrintHotKey;

extern sdword shiplagtotals[TOTAL_NUM_SHIPS];

extern bool32 cmActive;

extern sdword cmMaxJobsPerClass[NUM_CLASSES];

/*=============================================================================
    Functions:
=============================================================================*/
void cmStartup(void);
void cmShutdown(void);
void cmLoadTextures(void);
void cmCloseTextures(void);

// cancels all jobs of a certain shiptype
void cmForceBuildShipType(ShipType type);

void cmReset(void); // Free carrier ship pointers used by construction manager
void cmDirtyShipInfo(void);   // CAT: declared for cat_clExternalBuildUpdate 0x0044dfc0 (CommandLayer.c)
bool32 cat_cmHasSupportUnits(ShipType type, struct Player *player);   // CAT: 0x00459b40 (CAT-PROVISIONAL, M2-R6 ConsMgr)

//start the construction manager.  It will kill itself when you hit the quit button.
sdword cmConstructionBegin(regionhandle region, smemsize ID, udword event, udword data);

//start a new build job
void cmBuildJobsAdd(shipsinprogress *factory, ShipStaticInfo *info, sdword index);

void cmAddFactory(struct Ship *ship,bool32 canBuildBigShips);
void cmRemoveFactory(struct Ship *ship);

void cmCloseIfOpen(void);

bool32 cmCanBuildShipType(Ship *factoryship,ShipType shiptype,bool32 checkResearch);

bool32 cmBuildHotKey(keyindex key, bool32 shift);
ShipType cmKeyToShipType(uword key);
uword cmShipTypeToKey(ShipType ship);

void cmUpdateShipsAvailable(void);

void cmPauseAllJbos(void);
void cmUnPauseAllJobs(void);

#define CMD_START   1
#define CMD_PAUSE   2
#define CMD_UNPAUSE 3
#define CMD_CANCEL  4

void cmDeterministicBuild(udword command, sdword numShips,
                          ShipType shipType, ShipRace shipRace,
                          uword playerIndex, ShipPtr creator);
void cmDeterministicBuildProcess(void);
void cmDeterministicBuildDisplay(void);
void cmDeterministicReset(void);
// Not really a task anymore.
void cmBuildTaskFunction(void);

void LoadConsMgrDetermOptional(void);
void SaveConsMgrDetermOptional(void);

// CAT (M2-R5 hud): one line of the main overlay's construction list (0x28 bytes in the exe): a ship type and, per
//      factory slot (0 the mothership, then the player's carriers in factory-list order), the jobs and time left
typedef struct CatCmJob
{
    ShipType type;              // +0x00
    sdword nJobs[4];            // +0x04
    sdword totalTime;           // +0x14 the type's buildTime (divided by the single-player factor in a single-player game)
    sdword timeLeft[4];         // +0x18
} CatCmJob;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatCmJob) == 0x28, "a construction list line is 0x28 bytes in cataclysm.exe");
#endif
sdword cat_cmCountJobsForPlayer(struct Player *player, CatCmJob *jobs);     // 0x00459ce0

// CAT (M2-R5 hud): the ship view's readers of the construction manager (names chosen here)
extern sdword catCmSinglePlayerCostFactor;      // 0x0089419c: the single-player cost divisor (1)
extern bool32 catCmExtendedInfo;                // 0x00a2dbf0: the CM_ExtendedInfo button's state (the ship descriptions)
struct tagfeatom;
void cat_cmExtendedInfoToggle(char *string, struct tagfeatom *atom);                // 0x00454620 (CM_ExtendedInfo)

#endif
