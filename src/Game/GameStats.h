// =============================================================================
//  GameStats.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 06/19/1999 by bpasechnik
// =============================================================================

#ifndef ___GAMESTATS_H
#define ___GAMESTATS_H

//[x] beside statistics that are functional

//resource units doesn't handle retired resources...

// CAT (M2-R4 rest-oracle): 0x7F8 bytes per player in cataclysm.exe (initGameStats 0x0053f100 zeroes it dword by dword,
//      gameStatsAddShip 0x0053f220 / gameStatsShipDied 0x0053f290 use the stride 0x1FE dwords): HW1's layout with 145
//      ship types plus three additions, one dword before totalRUsInCurrentShips, a second 12-class array after
//      totalForEachClass and one dword before pointsOfDamageDoneToWho. The additions' readers are the stats screen
//      (cat_gameStatsPlayerRowFill 0x00488c80 reads +0x28 and +0x7D0) and clCreateShip 0x0044a840 (+0x2A8); the names
//      are chosen here.
typedef struct PlayerStats
{
    real32 timeOfDeath;                                 /*+0x000 [x]*/
    sdword typeOfPlayerDeath;                           /*+0x004 [x]*/
    sdword totalResourceUnits;                          /*+0x008 [] this value always goes up...except when giving RU's away*/
    sdword totalResourceUnitsCollected;                 /*+0x00C [x] all resource units collected via harvesting/salvaging of ships*/
    sdword totalResourceUnitsSpent;                     /*+0x010 [x]*/
    sdword totalResourceUnitsGiven;                     /*+0x014 [x]*/
    sdword totalResourceUnitsRecieved;                  /*+0x018 [x]*/
    sdword totalRegeneratedResourceUnits;               /*+0x01C [x]*/
    sdword totalInjectedResources;                      /*+0x020 [x]*/
    sdword totalResourceUnitsViaBounties;               /*+0x024 [x]*/
    sdword catStat28;                                   /*+0x028 CAT: stats screen row 3 (cat_gameStatsPlayerRowFill)*/
    sdword totalRUsInCurrentShips;                      /*+0x02C [x] value of ships player currently has*/
    sdword totalShips;                                  /*+0x030 [x]*/
    sdword totalForEachShip[TOTAL_NUM_SHIPS];           /*+0x034 [x]*/
    sdword totalForEachClass[NUM_CLASSES];              /*+0x278 [x]*/
    sdword catBuiltForEachClass[NUM_CLASSES];           /*+0x2A8 CAT: clCreateShip counts a built ship's class here too*/
    sdword totalKills;                                  /*+0x2D8 [x]*/
    sdword totalRUsKilled;                              /*+0x2DC [x]*/
    sdword totalKillsForEachShip[TOTAL_NUM_SHIPS];      /*+0x2E0 [x]*/
    sdword totalKillsForEachClass[NUM_CLASSES];         /*+0x524 [x]*/
    sdword totalLosses;                                 /*+0x554 [x]*/
    sdword totalRUsLost;                                /*+0x558 [x]*/
    sdword totalLossesForEachShip[TOTAL_NUM_SHIPS];     /*+0x55C [x]*/
    sdword totalLossesForEachClass[NUM_CLASSES];        /*+0x7A0 [x]*/
    sdword catStat7D0;                                  /*+0x7D0 CAT: stats screen row 1 (cat_gameStatsPlayerRowFill)*/
    real32 pointsOfDamageDoneToWho[MAX_MULTIPLAYER_PLAYERS+1];      /*+0x7D4 []*/
}PlayerStats;

typedef struct GameStats
{
    sdword totalResourcesInGame;                        //[]
    sdword totalShipsInGame;                            //[] built/aquired
    sdword totalKills;                                  //[x]
    sdword totalRUsInAllShips;                          //[x]
    sdword totalRUsKilled;                              //[x]
    sdword totalRUsLost;                                //[x]
    sdword totalResourceUnitsCollected;                 //[x]  //via harvesting
    real32 updatedRUValuesTime;
    sdword universeRUWorth;
    sdword startingResources;
    PlayerStats playerStats[MAX_MULTIPLAYER_PLAYERS+1];   //[]
}GameStats;

#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(PlayerStats) == 0x7F8, "PlayerStats is 0x7F8 bytes in cataclysm.exe (initGameStats 0x0053f100)");
_Static_assert(offsetof(PlayerStats, totalShips) == 0x30, "PlayerStats.totalShips at 0x30 (gameStatsAddShip 0x0053f220)");
_Static_assert(offsetof(PlayerStats, totalKills) == 0x2D8, "PlayerStats.totalKills at 0x2D8 (gameStatsShipDied 0x0053f290)");
_Static_assert(offsetof(PlayerStats, totalLossesForEachClass) == 0x7A0, "PlayerStats.totalLossesForEachClass at 0x7A0");
_Static_assert(sizeof(GameStats) == 0x47E0, "GameStats is 0x47E0 bytes in cataclysm.exe (universe.gameStats 0x308..0x4AE8)");
_Static_assert(offsetof(GameStats, universeRUWorth) == 0x20, "GameStats.universeRUWorth at 0x20 (CET universe.h)");
#endif

#endif
