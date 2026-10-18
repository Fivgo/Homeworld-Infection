// =============================================================================
//  AIPlayer.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/31/1998 by gshaw
// =============================================================================

#ifndef ___AIPLAYER_H
#define ___AIPLAYER_H

#include "AITeam.h"
#include "AIVar.h"
#include "LinkedList.h"
#include "ShipDefs.h"
#include "ShipSelect.h"
#include "SpaceObj.h"
#include "Universe.h"

typedef enum
{
    AI_BEGINNER,
    AI_INTERMEDIATE,
    AI_ADVANCED,
    AI_NUM_LEVELS
} AIPlayerLevel;


#define REQUESTSHIPS_HIPRI      1
typedef struct RequestShips
{
    Node node;
    ShipType shiptype;
    sdword num_ships;
    sdword priority;
    ShipPtr creator;    //the ship where the request will be fulfilled
} RequestShips;

typedef struct TeamWaitingForTheseShips
{
    Node node;
    ShipType shiptype;
    sdword num_ships;
    struct AITeam *team;
    char doneSetVarStr[AIVAR_LABEL_MAX_LENGTH+1];
} TeamWaitingForTheseShips;

typedef struct ShipTypesBeingBuilt
{
    ubyte NumShipsOfType[TOTAL_NUM_SHIPS];
} ShipTypesBeingBuilt;

// CAT: the exe keeps the armada's targets in the AIPlayer itself, a MaxSelection at +0xCF8 that aiuFindArmadaTarget
//      0x0041e860 fills (cat_selSelectionCopy) and LoadThisAIPlayer 0x00412190 loads with LoadMaxSelectionAndFix; HW1
//      allocated it (0x14 bytes in HW1, 0xFB4 in cataclysm.exe)
typedef struct ArmadaType
{
    ubyte          numTeams;        //how many teams are needed to go for an armada run
    MaxSelection   targets;         // CAT: embedded (HW1: SelectCommand *)
    sdword         visibility;
    struct AITeam *recon_team;
} ArmadaType;

typedef enum
{
    ATTACK_FLEET_FAST,
    ATTACK_FLEET_GUARD,
    ATTACK_FLEET_BIG,
    ATTACK_FLEET_HUGE,
    TAKEOUT_TARGET,
    FANCY_TAKEOUT_TARGET,
    FIGHTER_STRIKE,
    CORVETTE_STRIKE,
    FRIGATE_STRIKE,
    HARASS_SMALL,
    HARASS_BIG,
    CAPTURE,
    MINE,
    CLOAKGEN,
    GRAVWELL,
    NUM_ATTACK_TYPES
} AttackType;

#define AIPLAYER_NUM_SUPPORTTEAMS   12
#define AIPLAYER_NUM_ATTACKTEAMS    24
#define AIPLAYER_MAX_NUM_GUARDTEAMS 12

#define AIPLAYER_NUM_RECONTEAMS     5

#define ALERT_CLOAK_YELLOW          0x00000001
#define ALERT_CLOAK_RED             0x00000002
#define ALERT_SWARMER_TARGETS       0x00000100


typedef struct AIPlayer
{
    Player *player;
    AIPlayerLevel aiplayerDifficultyLevel;

    udword ResourceFeatures;
    udword AttackFeatures;
    udword DefenseFeatures;

    // pre-calculated values for this AI Player
    udword aiplayerUpdateRate;

    // store information for a given AI player here:
    udword aiplayerFrameCount;

    //enemy player variables
    udword enemyPlayerCount;
    Player *primaryEnemyPlayer;

    GrowSelection newships;
    GrowSelection enemyShipsIAmAwareOf[TOTAL_NUM_SHIPS];
    GrowSelection primaryEnemyShipsIAmAwareOf[TOTAL_NUM_SHIPS];
    sdword NumRUDockPoints;
    sdword NumRCollectorsBeingBuilt;
    sdword NumRControllersBeingBuilt;
    sdword NumASFBeingBuilt;
    sdword NumResearchShipsBeingBuilt;
    sdword catNumSupportModulesBeingBuilt;      // CAT: +0x950 the support modules ordered (BuildShip 0x00404470 counts
                                                //      s/bMothershipSupport and sCarrierSupport here; airAddNewShip
                                                //      0x00412950 takes them off; name chosen here)

    ShipPtr ScriptCreator;                      // construction ship for Script controlled ships
    ShipPtr AICreator;                          // construction ship for AI controlled ships

    sdword NumRUsSpentOnResourceman;
    sdword NumRUsSpentOnAttman;
    sdword NumRUsSpentOnDefman;
    sdword NumRUsSpentOnScriptman;
    RequestShips ResourceManRequestShips;
    LinkedList AttackManRequestShipsQ;
    LinkedList DefenseManRequestShipsQ;
    LinkedList ScriptManRequestShipsQ;
    LinkedList AttackManTeamsWaitingForShipsQ;
    LinkedList DefenseManTeamsWaitingForShipsQ;
    LinkedList ScriptManTeamsWaitingForShipsQ;
    ShipTypesBeingBuilt AttackManShipsBeingBuilt;
    ShipTypesBeingBuilt DefenseManShipsBeingBuilt;
    ShipTypesBeingBuilt ScriptManShipsBeingBuilt;
    udword TechnologyDeficit;
    ubyte ResourcersToBuild;
    ubyte ResearchersToBuild;
    ubyte catSupportModuleRequested;    // CAT: +0xB8E a support module is requested (cat_aifRequestSupportModule
                                        //      0x00405210 sets it, aifBuildRequestedShips 0x004045b0 / airAddNewShip
                                        //      0x00412950 clear it; name chosen here)
    ubyte ResearchDelay;            //delay for research depending on difficulty level (CAT: +0xB8F)

    bool32 firstTurn;
    bool32 recalculateAllies;

    //hyperspacing stuff
    sdword aifHyperSavings;         //the amount of RUs saved up for hyperspacing
    real32 aifHyperSkimming;        //the amount of RUs skimmed each time new RUs arrive
    sdword aifLastRUCount;          //the amount of RUs the CPU had last turn

    // resourceman stuff
    GrowSelection airResourceReserves;
    GrowSelection airResourceCollectors;
    sdword airEasilyAccesibleRUsInWorld;
    sdword airNumRCollectors;
    sdword airNumRControllers;
    sdword airNumASF;
    sdword airNumResearchShips;
    sdword catAirNumSupportModules;             // CAT: +0xBC8 the support modules built (airAddNewShip 0x00412950,
                                                //      airShipDied 0x00412590; name chosen here)
    udword numSupportTeams;
    struct AITeam *supportTeam[AIPLAYER_NUM_SUPPORTTEAMS];

    // attackman stuff
    //GrowSelection    newattackships;
    struct AITeam *harassTeam;
    struct AITeam *reconTeam[AIPLAYER_NUM_RECONTEAMS];
    udword         numReconTeams;
    struct AITeam *attackTeam[AIPLAYER_NUM_ATTACKTEAMS];
    udword         numAttackTeams;
    sdword         haveAttackedMothership;
    char           attackVarLabel[AIVAR_LABEL_MAX_LENGTH+1];
    udword         aiaAttackProbability[NUM_ATTACK_TYPES];
    ubyte          numLeaders;
    ArmadaType     aiaArmada;
    SelectCommand  *Targets;            //swarmer specific, but doesn't need to be
	real32		   aiaTimeout;

    // defenseman stuff
    //GrowSelection    newdefenseships;
    SelectCommand    *shipsattackingmothership;
    bool32             MothershipUnderAttack;             //later change to an AlertStatus bit
    udword           AlertStatus;
    udword           NumProxSensorsRequested;
    SelectCommand    *aidProximitySensors;
    SelectCommand    *aidDefenseTargets;
    SelectCommand    *aidInvadingShips;
    SelectCommand    *aidDistressShips;
    real32           sphereofinfluence;                 //radius squared of defense sphere around mothership
    sdword mothershipdefteam;
    sdword numGuardTeams;
    struct AITeam *guardTeams[AIPLAYER_MAX_NUM_GUARDTEAMS];

    // team stuff
    struct AITeam **teams;
    sdword teamsAllocated;
    sdword teamsUsed;

} AIPlayer;

// CAT: cataclysm.exe's AIPlayer is 0x1D18 bytes (aiplayerInit 0x00411010 allocates and clears 0x746 dwords; the save
//      chunk SaveThisAIPlayer 0x00411cd0 is 0x1D18); the offsets are the ones its AI code reads
#if defined(__i386__)
_Static_assert(sizeof(AIPlayer) == 0x1D18, "AIPlayer is 0x1D18 bytes in cataclysm.exe");
_Static_assert(__builtin_offsetof(AIPlayer, newships) == 0x24, "AIPlayer.newships at +0x24");
_Static_assert(__builtin_offsetof(AIPlayer, primaryEnemyShipsIAmAwareOf) == 0x4B4, "AIPlayer.primaryEnemyShipsIAmAwareOf at +0x4B4");
_Static_assert(__builtin_offsetof(AIPlayer, NumRUDockPoints) == 0x93C, "AIPlayer.NumRUDockPoints at +0x93C");
_Static_assert(__builtin_offsetof(AIPlayer, catNumSupportModulesBeingBuilt) == 0x950, "AIPlayer support modules ordered at +0x950");
_Static_assert(__builtin_offsetof(AIPlayer, ScriptCreator) == 0x954, "AIPlayer.ScriptCreator at +0x954");
_Static_assert(__builtin_offsetof(AIPlayer, NumRUsSpentOnResourceman) == 0x95C, "AIPlayer.NumRUsSpentOnResourceman at +0x95C");
_Static_assert(__builtin_offsetof(AIPlayer, AttackManRequestShipsQ) == 0x98C, "AIPlayer.AttackManRequestShipsQ at +0x98C");
_Static_assert(__builtin_offsetof(AIPlayer, AttackManShipsBeingBuilt) == 0x9D4, "AIPlayer.AttackManShipsBeingBuilt at +0x9D4");
_Static_assert(__builtin_offsetof(AIPlayer, TechnologyDeficit) == 0xB88, "AIPlayer.TechnologyDeficit at +0xB88");
_Static_assert(__builtin_offsetof(AIPlayer, ResourcersToBuild) == 0xB8C, "AIPlayer.ResourcersToBuild at +0xB8C");
_Static_assert(__builtin_offsetof(AIPlayer, catSupportModuleRequested) == 0xB8E, "AIPlayer support module request at +0xB8E");
_Static_assert(__builtin_offsetof(AIPlayer, ResearchDelay) == 0xB8F, "AIPlayer.ResearchDelay at +0xB8F");
_Static_assert(__builtin_offsetof(AIPlayer, firstTurn) == 0xB90, "AIPlayer.firstTurn at +0xB90");
_Static_assert(__builtin_offsetof(AIPlayer, aifHyperSavings) == 0xB98, "AIPlayer.aifHyperSavings at +0xB98");
_Static_assert(__builtin_offsetof(AIPlayer, airResourceReserves) == 0xBA4, "AIPlayer.airResourceReserves at +0xBA4");
_Static_assert(__builtin_offsetof(AIPlayer, airEasilyAccesibleRUsInWorld) == 0xBB4, "AIPlayer.airEasilyAccesibleRUsInWorld at +0xBB4");
_Static_assert(__builtin_offsetof(AIPlayer, catAirNumSupportModules) == 0xBC8, "AIPlayer support modules built at +0xBC8");
_Static_assert(__builtin_offsetof(AIPlayer, numSupportTeams) == 0xBCC, "AIPlayer.numSupportTeams at +0xBCC");
_Static_assert(__builtin_offsetof(AIPlayer, harassTeam) == 0xC00, "AIPlayer.harassTeam at +0xC00");
_Static_assert(__builtin_offsetof(AIPlayer, attackTeam) == 0xC1C, "AIPlayer.attackTeam at +0xC1C");
_Static_assert(__builtin_offsetof(AIPlayer, numLeaders) == 0xCF0, "AIPlayer.numLeaders at +0xCF0");
_Static_assert(__builtin_offsetof(AIPlayer, aiaArmada) == 0xCF4, "AIPlayer.aiaArmada at +0xCF4");
_Static_assert(__builtin_offsetof(AIPlayer, aiaArmada.targets) == 0xCF8, "AIPlayer.aiaArmada.targets at +0xCF8");
_Static_assert(__builtin_offsetof(AIPlayer, aiaArmada.visibility) == 0x1CA0, "AIPlayer.aiaArmada.visibility at +0x1CA0");
_Static_assert(__builtin_offsetof(AIPlayer, Targets) == 0x1CA8, "AIPlayer.Targets at +0x1CA8");
_Static_assert(__builtin_offsetof(AIPlayer, shipsattackingmothership) == 0x1CB0, "AIPlayer.shipsattackingmothership at +0x1CB0");
_Static_assert(__builtin_offsetof(AIPlayer, aidProximitySensors) == 0x1CC0, "AIPlayer.aidProximitySensors at +0x1CC0");
_Static_assert(__builtin_offsetof(AIPlayer, numGuardTeams) == 0x1CD8, "AIPlayer.numGuardTeams at +0x1CD8");
_Static_assert(__builtin_offsetof(AIPlayer, teams) == 0x1D0C, "AIPlayer.teams at +0x1D0C");
_Static_assert(__builtin_offsetof(AIPlayer, teamsUsed) == 0x1D14, "AIPlayer.teamsUsed at +0x1D14");
#endif

/*=============================================================================
    Global Variables:
=============================================================================*/
extern bool32 aiplayerLogEnable;

extern AIPlayer *aiCurrentAIPlayer;
extern uword    aiIndex;

/*=============================================================================
    Multiplayer Defines:
=============================================================================*/
//Note: not implemented yet... implement later...
typedef enum
{
    Multi_RandomTarget,
    Multi_RandomTarget_NoOverlap,
    Multi_PlayerTarget,
    Multi_NoTarget
} aiMultiplayer_Config;

/*=============================================================================
    Public Data:
=============================================================================*/

extern real32 ATTMAN_BUILDPRIORITY_RATIO;
extern real32 DEFMAN_BUILDPRIORITY_RATIO;
extern real32 RESMAN_BUILDPRIORITY_RATIO;
extern real32 AIPLAYER_OVERKILL_FACTOR;

extern ubyte  AIF_RESEARCH_DELAY[AI_NUM_LEVELS];

//attackman tweaks
//out of 255
extern udword AIA_ATTACK_FLEET_FAST_PROB[AI_NUM_LEVELS];
extern udword AIA_ATTACK_FLEET_FAST_RANGE[AI_NUM_LEVELS];
extern udword AIA_ATTACK_FLEET_GUARD_PROB[AI_NUM_LEVELS];
extern udword AIA_ATTACK_FLEET_GUARD_RANGE[AI_NUM_LEVELS];
extern udword AIA_ATTACK_FLEET_BIG_PROB[AI_NUM_LEVELS];
extern udword AIA_ATTACK_FLEET_BIG_RANGE[AI_NUM_LEVELS];
extern udword AIA_ATTACK_FLEET_HUGE_PROB[AI_NUM_LEVELS];
extern udword AIA_ATTACK_FLEET_HUGE_RANGE[AI_NUM_LEVELS];
extern udword AIA_TAKEOUT_TARGET_PROB[AI_NUM_LEVELS];
extern udword AIA_TAKEOUT_TARGET_RANGE[AI_NUM_LEVELS];
extern udword AIA_FANCY_TAKEOUT_TARGET_PROB[AI_NUM_LEVELS];
extern udword AIA_FANCY_TAKEOUT_TARGET_RANGE[AI_NUM_LEVELS];
extern udword AIA_FIGHTER_STRIKE_PROB[AI_NUM_LEVELS];
extern udword AIA_FIGHTER_STRIKE_RANGE[AI_NUM_LEVELS];
extern udword AIA_CORVETTE_STRIKE_PROB[AI_NUM_LEVELS];
extern udword AIA_CORVETTE_STRIKE_RANGE[AI_NUM_LEVELS];
extern udword AIA_FRIGATE_STRIKE_PROB[AI_NUM_LEVELS];
extern udword AIA_FRIGATE_STRIKE_RANGE[AI_NUM_LEVELS];
extern udword AIA_HARASS_BIG_PROB[AI_NUM_LEVELS];
extern udword AIA_HARASS_BIG_RANGE[AI_NUM_LEVELS];
extern udword AIA_HARASS_SMALL_PROB[AI_NUM_LEVELS];
extern udword AIA_HARASS_SMALL_RANGE[AI_NUM_LEVELS];
extern udword AIA_CAPTURE_PROB[AI_NUM_LEVELS];
extern udword AIA_CAPTURE_RANGE[AI_NUM_LEVELS];
extern udword AIA_MINE_PROB[AI_NUM_LEVELS];
extern udword AIA_MINE_RANGE[AI_NUM_LEVELS];
extern udword AIA_CLOAKGEN_PROB[AI_NUM_LEVELS];
extern udword AIA_CLOAKGEN_RANGE[AI_NUM_LEVELS];
extern udword AIA_GRAVWELL_PROB[AI_NUM_LEVELS];
extern udword AIA_GRAVWELL_RANGE[AI_NUM_LEVELS];

//out of 100
extern udword AIA_KAMIKAZE_PROB[AI_NUM_LEVELS];
extern udword AIA_KAMIKAZE_RANGE[AI_NUM_LEVELS];

extern ubyte  AIA_NUM_ARMADA_TEAMS[AI_NUM_LEVELS];

extern udword AIA_ENEMYBLOBS_PER_RECON;

//defenseman tweaks
extern real32 AID_MOTHERSHIP_DEFENSE_FLEET_STRENGTH;
extern udword AID_RESCON_ASF_DEFENDABLE_MULTIPLIER;
extern udword AID_SPHERE_OF_INFLUENCE_INTERVAL;
extern udword AID_ROVING_DEFENSE_LOW_COUNT_CUTOFF;
extern udword AID_ROVING_DEFENSE_LOW_COUNT_DIVISOR;
extern udword AID_ROVING_DEFENSE_HIGH_COUNT_DIVISOR;
extern udword AID_DEFEND_MOTHERSHIP_FUEL_LOW;
extern real32 AID_MOTHERSHIP_ATTACKER_FLEET_RADIUS;

//team tweaks
extern udword           AIT_TEAM_MOVE_DELAY[NUM_TEAMLEVEL_TYPES];
extern TypeOfFormation  AIT_DEFAULT_FORMATION;
extern real32           AIT_DEFTEAM_NEEDS_HELP_STRENGTH;

//order tweaks
extern udword           AIO_GUARD_SHIPS_NUM_CAPITAL;
extern udword           AIO_GUARD_SHIPS_NUM_NONCAPITAL;
extern udword           AIO_GUARD_SHIPS_FUEL_LOW;
extern TypeOfFormation  AIO_HARASS_INITIAL_MOVE_FORMATION;
extern udword           AIO_HARASS_FUEL_LOW;
extern udword           AIO_HARASS_NUMBERS_LOW;
extern udword           AIO_DEFMOTHERSHIP_FUEL_LOW;
extern udword           AIO_PATROL_FUEL_LOW;
extern udword           AIO_PATROL_NUMBERS_LOW;
extern udword           AIO_FASTROVING_FUEL_LOW;
extern udword           AIO_FASTROVING_NUMBERS_LOW;
extern udword           AIO_SLOWROVING_FUEL_LOW;
extern udword           AIO_SLOWROVING_NUMBERS_LOW;
extern TypeOfFormation  AIO_TOUT_MSHIP_FAST_TGRD_FORMATION;
extern TacticsType      AIO_TOUT_MSHIP_FAST_TGRD_TACTICS;
extern TypeOfFormation  AIO_TOUT_MSHIP_BIG_TGRD_FORMATION;
extern TacticsType      AIO_TOUT_MSHIP_BIG_TGRD_TACTICS;
extern TypeOfFormation  AIO_TOUT_MSHIP_GUARD_TGRD_FORMATION;
extern TacticsType      AIO_TOUT_MSHIP_GUARD_TGRD_TACTICS;
extern TypeOfFormation  AIO_TOUT_TARG_WCUR_FORMATION;
extern TacticsType      AIO_TOUT_TARG_WCUR_TACTICS;
extern TypeOfFormation  AIO_TOUT_TARG_TGUARD_FORMATION;
extern TacticsType      AIO_TOUT_TARG_TGUARD_TACTICS;
extern TypeOfFormation  AIO_TOUT_TARG_FANCY_TGRD_FORMATION;
extern TacticsType      AIO_TOUT_TARG_FANCY_TGRD_TACTICS;
extern udword           AIO_FIGHTER_KAMIKAZE_HEALTH;
extern udword           AIO_CORVETTE_KAMIKAZE_HEALTH;

//move and handler tweaks
extern real32          AIH_GENERIC_GETTINGROCKED_STRENGTH;
extern TypeOfFormation AIH_GENERIC_GETTINGROCKED_FORMATION;
extern real32          AIM_DIST_MOVE_END;
extern udword          AIM_SQUADRON_NUM_SHIPS;
extern real32          AIM_INTERCEPT_FINISH_DISTSQ;
extern real32          AIM_INTERCEPT_RECALC_DISTSQ;
//harass move tweaks
extern real32          AIM_MIN_HARASS_ATTACK_DIST_SQ;
extern real32          AIM_HARASS_STANDOFF_DIST;
extern real32          AIM_HARASS_PROTECTION_RADIUS;
extern TypeOfFormation AIM_HARASS_MOVE_FORMATION;
extern TypeOfFormation AIM_HARASS_SINGLETARGET_FORMATION;
extern TypeOfFormation AIM_HARASS_MULTITARGET_FORMATION;
extern TypeOfFormation AIH_HARASS_SINGLEATTACK_FORMATION;
extern real32          AIH_HARASS_NUMLOW_STANDOFF_DIST;
extern TypeOfFormation AIH_HARASS_NUMLOW_FORMATION;
//flank move tweaks
extern real32          AIM_FLANK_ATTACK_RADIUS;
extern TypeOfFormation AIM_FLANK_MOVE_FORMATION;
extern TypeOfFormation AIM_FLANK_ATTACK_FORMATION;
//dock move tweaks
extern udword          AIM_DOCK_REPAIRHEALTH_TRIGGER;
extern real32          AIM_DOCK_FINISH_FUEL_THRESHOLD;
extern TypeOfFormation AIM_DOCK_DEFAULT_FORMATION;
//defend mothership move tweaks
extern TypeOfFormation AIM_DEF_MOTHERSHIP_ATTACK_FORMATION;
//active patrol move tweaks
extern TypeOfFormation AIM_PATROL_FIRST_FORMATION;
extern TypeOfFormation AIM_PATROL_GENERAL_FORMATION;
extern real32          AIH_PATROL_ENEMYNEARBY_STRENGTH;
extern TypeOfFormation AIH_PATROL_ENEMYNEARBY_FORMATION;
extern real32          AIH_FASTDEF_NUMLOW_STANDOFF_DIST;
extern TypeOfFormation AIH_FASTDEF_NUMLOW_FORMATION;
extern real32          AIH_FASTDEF_INVADER_ENEMYDIST;
extern udword          AIH_FASTDEF_INVADER_LOTSFEW_LIMIT;
extern TypeOfFormation AIH_FASTDEF_INVADER_LOTS_FORMATION;
extern TypeOfFormation AIH_FASTDEF_INVADER_FEW_FORMATION;
extern real32          AIH_SLOWDEF_NUMLOW_STANDOFF_DIST;
extern TypeOfFormation AIH_SLOWDEF_NUMLOW_FORMATION;
extern real32          AIH_SLOWDEF_RESPONSE_MAXDISTSQ;
extern real32          AIH_SLOWDEF_INVADER_ENEMYDIST;
extern udword          AIH_SLOWDEF_INVADER_LOTSFEW_LIMIT;
extern TypeOfFormation AIH_SLOWDEF_INVADER_LOTS_FORMATION;
extern TypeOfFormation AIH_SLOWDEF_INVADER_FEW_FORMATION;
//temp guard move tweaks
extern real32          AIM_TEMP_GUARD_MOVE_LOCATION_MULT;
extern TypeOfFormation AIM_TEMP_GUARD_MOVE_FORMATION;
extern TacticsType     AIM_TEMP_GUARD_MOVE_TACTICS;
extern TypeOfFormation AIM_TEMP_GUARD_INVADER_FORMATION;
//support move tweaks
extern real32          AIM_SUPPORT_SAFE_DISTANCE;

//utilities tweaks
extern real32          AIU_STANDOFF_NEG_ERROR_ANGLE;
extern real32          AIU_STANDOFF_POS_ERROR_ANGLE;
extern real32          AIU_STANDOFF_DIST_ERROR_LOW;
extern real32          AIU_STANDOFF_DIST_ERROR_HIGH;
extern udword          AIU_RESCUE_MULTIPLE_MINSHIPS;
extern udword          AIU_RESCUE_ENEMYMULTIPLE_MINSHIPS;
extern TypeOfFormation AIU_RESCUE_NOMULTIPLE_FORMATION;
extern TypeOfFormation AIU_RESCUE_TEAMMULTIPLE_FORMATION;
extern TypeOfFormation AIU_RESCUE_BOTHMULTIPLE_FORMATION;
extern real32          AIU_TAKEOUT_ENEMYFLEET_RADIUS;
extern udword          AIU_TAKEOUT_MULTIPLE_MINSHIPS;
extern udword          AIU_TAKEOUT_ENEMYMULTIPLE_MINSHIPS;
extern TypeOfFormation AIU_TAKEOUT_NOMULTIPLE_FORMATION;
extern TypeOfFormation AIU_TAKEOUT_TEAMMULTIPLE_FORMATION;
extern TypeOfFormation AIU_TAKEOUT_BOTHMULTIPLE_FORMATION;
extern real32          AIU_SAFEST_STANDOFF_DIST_MULT;
extern real32          AIU_FINDUNARMED_PROTECTION_STRENGTH;
extern real32          AIU_FINDATTACKING_ENEMYFLEET_RADIUS;
extern real32          AIU_HARASS_PROTECTION_RADIUS;
extern real32          AIU_UNARMED_UNDEFENDED_MODIFIER;
extern real32          AIU_UNARMED_MODIFIER;
extern real32          AIU_FIGHTER_VULNERABLE_MODIFIER;


/*=============================================================================
    Functions
=============================================================================*/

AIPlayer *aiplayerInit(Player *player,AIPlayerLevel aiplayerLevel);
void aiplayerClose(AIPlayer *aiplayer);
void aiplayerPlay(AIPlayer *aiplayer);
void aiplayerUpdateAll(void);
void aiplayerGameStart(AIPlayer *aiplayer);

void aiplayerStartup(udword num_players, udword num_human_players, udword num_comp_players);
void aiplayerShutdown(void);

#ifdef HW_BUILD_FOR_DEBUGGING
#define aiplayerLog(x)    aiplayerDebugLog x
#else
#define aiplayerLog(x)    {;}
#endif

void aiplayerDebugLog(uword playerIndex, char *format, ...);

void aiplayerShipDied(ShipPtr ship);

void aiplayerResourceDied(Resource *resource);

void aiplayerShipLaunchedCallback(Ship *ship);

void aiplayerPlayerDied(Player *player);

void aiplayerChangeBigotry(udword newvalue);
void aiplayerAddLeader(AIPlayer *aiplayer, ShipPtr ship);

/*=============================================================================
    Save Game stuff
=============================================================================*/

struct Path;

void aiplayerSave(void);
void aiplayerLoad(void);

void SavePath(struct Path *path);
struct Path *LoadPath(void);

AIPlayer *NumberToAIPlayer(sdword number);
sdword AIPlayerToNumber(AIPlayer *aiplayer);
struct AITeam *AITeamIndexToTeam(AIPlayer *aiplayer,sdword index);
sdword AITeamToTeamIndex(struct AITeam *team);

extern AIPlayer *fixingThisAIPlayer;

#endif
