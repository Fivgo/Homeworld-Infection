/*=============================================================================
    Name    : Researchapi.h
    Purpose : definitions for the research manager

    Created 5/25/1998 by ddunlop
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___RESEARCHAPI_H
#define ___RESEARCHAPI_H

#include "LinkedList.h"
#include "ObjTypes.h"
#include "ShipDefs.h"
#include "Types.h"

/*=============================================================================
    Definitions:
=============================================================================*/

// CAT (M2-R4 rest-oracle): Cataclysm keeps technology as a SET of 42 sdwords (0xA8 bytes), not as a bit mask: one
//      entry per technology (1 = has / needed) plus a flag word at index 41 (cat_rmTechSetAnd 0x004f4070 tests a[0x29]):
//      2 = every technology ("all"), -2 = disabled (rmEnableShip 0x004f3ca0 writes it), 0 = plain set. HW1's
//      TechToBit/AllTechnology/RM_Disabled have no exe counterpart and are retired.
#define CAT_TECHSET_SIZE        42
#define CAT_TECHSET_FLAG        41          // index of the flag word (0xA4 into the set)
#define CAT_TECHSET_ALL         2           // flag: the set holds every technology
#define CAT_TECHSET_DISABLED    (-2)        // flag: a ship that can never be built (rmEnableShip FALSE)
typedef sdword CatTechSet[CAT_TECHSET_SIZE];

// lab status defines
#define LS_NORESEARCHSHIP   0
#define LS_NORESEARCHITEM   1
#define LS_RESEARCHITEM     2
#define LS_RESEARCHITEMSOON 3

// number of labs
#define NUM_RESEARCHLABS    5           // CAT: 5 (rmInitializeResearchStruct 0x004f3a00 clears 5 labs at Player+0x360;
                                        //      a Sect lab is a mothership module: cat_rmResearchModuleActivated)

// CAT: the techlevel argument of rmInitializeResearchStruct / cat_rmTechSetInit 0x004f4210: -1 clears the set, 2 gives
//      every technology (the flag word), -2 disables; gameStart 0x005a9290 passes -1 or 2
#define TECH_NOTECHNOLOGY       (-1)
#define TECH_ALLTECHNOLOGY      2

// CAT: a research lab's module (cat_ModuleTypeToString 0x004f2570, table 0x008902f8); the lab index of the module
#define MODULE_Hangar               0
#define MODULE_Armour               1
#define MODULE_Engineering          2
#define MODULE_Advanced_Engineering 3
#define MODULE_Weapon               4

/*=============================================================================
    Type definitions:
=============================================================================*/


// list of all technologies that can be researched
// CAT: Cataclysm's 41 technologies (techtypeinfo 0x008901a8 read from cataclysm.exe); the names are the exe's
typedef enum
{
    TECH_CLOAKING = 0,
    TECH_TARGETING_SYSTEMS,
    TECH_PLASMA_WEAPONS,
    TECH_MEDIUM_GUNS,
    TECH_DEFENSE_FIELD,
    TECH_HEAVY_GUNS,
    TECH_REPAIR,
    TECH_SALVAGE,
    TECH_DRIVE_FIGHTER,
    TECH_ADRIVE_FIGHTER,
    TECH_DRIVE_FRIGATE,
    TECH_ADRIVE_FRIGATE,
    TECH_DRIVE_DESTROYER,
    TECH_ADRIVE_DESTROYER,
    TECH_ARMOUR2,
    TECH_ARMOUR3,
    TECH_SUPPORT_SYSTEMS,
    TECH_MICRO_SHIP,
    TECH_ENERGY,
    TECH_ION,
    TECH_MISSILE,
    TECH_REPULSE,
    TECH_EMP,
    TECH_EXPLOSIVE_CHARGE,
    TECH_LINK,
    TECH_MIMIC,
    TECH_AFTERBURNER,
    TECH_BIG_GUN,
    TECH_ADVANCED_SENSORS,
    TECH_FORCE_FIELD1,
    TECH_FORCE_FIELD2,
    TECH_FORCE_FIELD3,
    TECH_INFECT,
    TECH_REPAIRBOTS,
    TECH_CRYSTAL_HARVEST,
    TECH_RESEARCH_CLEESAN_ATTACK,
    TECH_BEAST_VACCINE,
    TECH_PHASED_TELEMETRY,
    TECH_INERTIALESS_DRIVE,
    TECH_BIOWARHEAD,
    TECH_DRIVE_CORVETTE,
    NumTechnologies
} TechnologyType;

#define TECH_NONE   ((TechnologyType)-1)    // CAT: "any lab" in rmFindFreeLab, "none" in the upgrade tables (0x29 = 41)


// defines for the technology dependancies and ship technology requirements

// CAT: one tech set per ship type (145) and per technology (41), then the research times (0x7AB4 bytes; the tables
//      at 0x00c282a0 / 0x00c18d20 / 0x00c207e0 are 0x7AC0 apart). The row of a technology sits at (0x91 + tech) sets
//      from the start (rmAllTechRequiredforTech 0x004f2db0), its time at 0x7A10 + tech * 4 (rmAssignPlayersLabToResearch)
typedef struct
{
    CatTechSet TechNeededToBuildShip[TOTAL_NUM_SHIPS];  // technologies needed before this ship can be built
    CatTechSet TechNeededToResearch[NumTechnologies];   // technology needed before this can be researched
    sdword TimeToComplete[NumTechnologies];             // time in seconds technology takes to finish
} TechStatics;


// defines for the research labs, and player status.

// CAT: 0x18 bytes: no numlabsresearching (rmAssignPlayersLabToResearch 0x004f2960 allocates 0x18 and copies 6 dwords)
typedef struct
{
    struct Node     link;               // link for linkedlist of research topics
    TechnologyType  techresearch;       // what tech is being researched
    real32          timeleft;           // how much time left in seconds
} ResearchTopic;

typedef struct
{
    sdword          labstatus;          // what is the status of this lab
    ResearchTopic  *topic;              // pointer to topic this lab is researching
} ResearchLab;

// CAT: Player +0x2A4 .. +0x64F4 (0x6250 bytes in the exe)
typedef struct
{
    CatTechSet   HasTechnology;         // +0x2A4 what tech this player has (a set, see CAT_TECHSET_*)
    bool32       CanDoResearch;         // +0x34C are they allowed to do research
    TechStatics *techstat;              // +0x350 statics for that technology
    LinkedList   listoftopics;          // +0x354 list of topics being researched for this player
    ResearchLab  researchlabs[NUM_RESEARCHLABS];  // +0x360 list of labs for this player
    // CAT: per ship type, the technologies that upgrade it: a Sect player's rows come from cat_shipTypeGetUpgradeTechs,
    //      a Beast player's from the per-type cases of rmInitializeResearchStruct 0x004f3a00; read by
    //      cat_rmTechRequiredForShipUpgrade 0x004f30e0 and rmResearchTechForShip 0x004f3380
    CatTechSet   catShipUpgradeTechs[TOTAL_NUM_SHIPS];      // +0x388
    // CAT: per ship type, whether the player may build it (cat_playerShipTypeEnabled 0x004f4780; the Beast cases of
    //      rmCanBuildShip 0x004f2630 read it directly)
    sdword       catShipTypeEnabled[TOTAL_NUM_SHIPS];       // +0x62B0
} PlayerResearchInfo;


/*=============================================================================
    Data:
=============================================================================*/

//extern TechnologyType         SetTechLevel;
extern udword         SetTechLevel;
// CAT: the three research tables (ResearchAPI.c; cat_shipInitResearchUpgrades reads the multiplayer Sect one)
extern TechStatics cat_SinglePlayerSectTechStatic;
extern TechStatics cat_MultiPlayerSectTechStatic;
extern TechStatics cat_BeastTechStatic;

/*=============================================================================
    Function Prototypes:
=============================================================================*/

// Forward declaration to get around circular dependency:
// Universe.h: Player <-> ResearchAPI.h: PlayerResearchInfo
struct Player;

// callback to set dependancies
// CAT: cat_rsShipDependSetCB 0x004f3dc0 / cat_rsTechDependSetCB 0x004f3ec0 (HW1's rmSetShipDependCB/rmSetTechDependCB
//      filled bit masks; these fill tech sets)
void cat_rsShipDependSetCB(char *directory, char *field, void *dataToFillIn);
void cat_rsTechDependSetCB(char *directory, char *field, void *dataToFillIn);
void cat_rsMPRBShipDependSetCB(char *directory, char *field, void *dataToFillIn);    // CAT: 0x004f4430 (M2-R6 managers)

char *RaceSpecificTechTypeToNiceString(TechnologyType tech, ShipRace race);
char *TechTypeToNiceString(TechnologyType tech);
char *TechTypeToString    (TechnologyType tech);
TechnologyType StrToTechType(char *tech);
char *cat_ModuleTypeToString(sdword module);                // CAT: 0x004f2570

// CAT: the tech set operations (0x004f3fc0 - 0x004f4300)
void   cat_rmTechSetAdd(sdword *techset, TechnologyType tech);      // 0x004f3fc0
void   cat_rmTechSetAnd(sdword *out, sdword *a, sdword *b);        // 0x004f4070
void   cat_rmTechSetOr(sdword *out, sdword *a, sdword *b);         // 0x004f4130
void   cat_rmTechSetClear(sdword *techset);                        // 0x004f41a0
bool32 cat_rmTechSetHas(sdword *techset, TechnologyType tech);     // 0x004f41b0
bool32 cat_rmTechSetEqual(sdword *a, sdword *b);                   // 0x004f41d0
void   cat_rmTechSetInit(sdword *techset, sdword techlevel);       // 0x004f4210
void   cat_rmTechSetNot(sdword *out, sdword *a);                   // 0x004f4300

// api function calls
ResearchTopic *Researching(struct Player *player, TechnologyType tech);

sdword rmResearchingAnything(struct Player *player);

void rmGiveTechToPlayerByName(struct Player *player, char *techName);
void rmGiveTechToPlayerByType(struct Player *player, TechnologyType techtype);

void   rmAddTechToPlayer(struct Player *player, TechnologyType tech);   // CAT: one technology, not a mask (0x004f2600)
sdword rmFindFreeLab(struct Player *player, TechnologyType tech);       // CAT: + the technology (its lab type) (0x004f2870)
bool32 rmCanBuildShip(struct Player *player, ShipType type, bool32 checkHangar);   // CAT: + 3rd argument (0x004f2630)
void   rmClearResearchlab(struct Player *player, sdword labnumber);
void   rmAssignPlayersLabToResearch(struct Player *player, sdword labnumber, TechnologyType tech);
void   rmUpdateResearch(void);
void   rmInitializeResearchStruct(struct Player *player, bool32 candoresearch, sdword techlevel);

// CAT: (out, player, tech): ORs the closure of tech's dependencies into out (0x004f2db0; HW1 returned a mask)
void   rmAllTechRequiredforTech(sdword *out, struct Player *player, TechnologyType tech);
sdword rmTechRequiredForShip(struct Player *player, ShipType type);
sdword cat_rmTechRequiredForShipUpgrade(struct Player *player, ShipType type);   // CAT: 0x004f30e0
bool32 cat_rmAIResearchTech(struct Player *player, TechnologyType tech);         // CAT: 0x004f3320
bool32 rmResearchTechForShip(struct Player *player, ShipType type);

void rmInitializeResearchStatics(struct Player *player);

void rmEnableShip(ShipRace race, ShipType ship, bool32 bEnabled);
void rmResetStaticInfo(void);

// CAT: the research modules of a Sect mothership are its labs (0x004f4530 / 0x004f4620)
void cat_rmResearchModuleActivated(struct Player *player, ShipType moduleType, bool32 announce);
void cat_rmResearchModuleDied(struct Player *player, ShipType moduleType);
void cat_rmMothershipLabLost(struct Player *player);                             // CAT: 0x004f4700
bool32 cat_playerShipTypeEnabled(struct Player *player, ShipType type);         // CAT: 0x004f4780
struct Ship;
void cat_rmAcquireShipTechs(struct Ship *ship);                                 // CAT: 0x004f4870 (ResearchGUI.c: it marks the research list)

//start the Research Manager.
void   rmAPIStartup(void);
void   rmAPIShutdown(void);

// CAT: ILP32 layout pins (docs/m2-r4-rest-oracle.md "layout")
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatTechSet) == 0xA8, "a Cataclysm tech set is 0xA8 bytes (cat_rmTechSetAnd 0x004f4070)");
_Static_assert(sizeof(TechStatics) == 0x7AB4, "TechStatics is 0x7AB4 bytes in cataclysm.exe (rows at 0x5F28 and 0x7A10)");
_Static_assert(offsetof(TechStatics, TechNeededToResearch) == 0x5F28, "TechNeededToResearch at 0x5F28 (rmAllTechRequiredforTech)");
_Static_assert(offsetof(TechStatics, TimeToComplete) == 0x7A10, "TimeToComplete at 0x7A10 (rmAssignPlayersLabToResearch)");
_Static_assert(sizeof(ResearchTopic) == 0x18, "ResearchTopic is 0x18 bytes in cataclysm.exe");
_Static_assert(sizeof(PlayerResearchInfo) == 0x6250, "PlayerResearchInfo spans Player +0x2A4..+0x64F4 in cataclysm.exe");
_Static_assert(offsetof(PlayerResearchInfo, CanDoResearch) == 0xA8, "CanDoResearch at Player+0x34C");
_Static_assert(offsetof(PlayerResearchInfo, listoftopics) == 0xB0, "listoftopics at Player+0x354");
_Static_assert(offsetof(PlayerResearchInfo, researchlabs) == 0xBC, "researchlabs at Player+0x360");
_Static_assert(offsetof(PlayerResearchInfo, catShipUpgradeTechs) == 0xE4, "catShipUpgradeTechs at Player+0x388");
_Static_assert(offsetof(PlayerResearchInfo, catShipTypeEnabled) == 0x600C, "catShipTypeEnabled at Player+0x62B0");
#endif

// CAT (M2-R5 hud): one line of the main overlay's research list (0xC bytes in the exe)
typedef struct CatRmTopic
{
    TechnologyType tech;        // +0x00
    sdword totalTime;           // +0x04 the technology's TimeToComplete
    real32 timeLeft;            // +0x08 the topic's time left
} CatRmTopic;
sdword cat_rmCollectTopicsByLab(struct Player *player, CatRmTopic *topics);     // 0x004f4380

#endif
