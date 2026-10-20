/*=============================================================================
    Name    : MissionSelfTest.c
    Purpose : HWI-TEST (M2-R8 kas-runtime-mission01), port diagnostic, not in
              the exe: /catMissionTest <ticks> (harness mode missiontest).
              A new single-player game started as the front end starts one
              (utySinglePlayerGameStart), on the virtual clock of /oracleMission
              (one universe tick a frame, the oracle stream written with
              /oracleDump) but with the exe's NIS-first start: Mission01's
              opening NIS plays in its staging level, the fleet hyperspaces into
              Mission01, the generated kas2c script runs. The test logs the flow
              and the KAS FSMs' states and plays the player's part with
              scripted events through the game's own interfaces (no input).
              /catMissionTestTutorial <ticks>: the tutorial's first screen.

    Created 2026-10-19 for Homeworld-Infection (M2-R8 kas-runtime-mission01).
    This tree is REULA-licensed, see src/COPYING.REULA.
=============================================================================*/

#include "MissionSelfTest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AIPlayer.h"
#include "AITeam.h"
#include "Debug.h"
#include "KAS.h"
#include "KASVar.h"
#include "NIS.h"
#include "Objectives.h"
#include "Subtitle.h"
#include "CommandWrap.h"
#include "KASFunc.h"
#include "UnivUpdate.h"
#include "ShipSelect.h"
#include "ObjTypes.h"
#include "SpaceObj.h"
#include "OracleDump.h"
#include "SinglePlayer.h"
#include "Universe.h"
#include "utility.h"
#include "GamePick.h"
#include "SaveGame.h"
#include "StringSupport.h"
#include "File.h"
#include "Tutor.h"
#include "UIControls.h"
#include "FEFlow.h"
#include "Region.h"
#include "ConsMgr.h"
#include "ResearchAPI.h"
#include "CommandLayer.h"
#include "Memory.h"
#include "mainrgn.h"
#include "Select.h"
#include "Worker.h"
#include "Mimic.h"
#include "Alliance.h"
#include "Salvage.h"

extern char SinglePlayerSavedGamesPath[];               // GamePick.c
extern char CurrentMissionName[];                       // KAS.c
extern KASWatchFunction CurrentMissionWatchFunction;    // KAS.c

static sdword catMissionTestTicks = 0;          // > 0: /catMissionTest given
static sdword catMissionTestTutorialTicks = 0;  // > 0: /catMissionTestTutorial given
static bool32 catMissionTestDone = FALSE;
static bool32 catMissionTestInit = FALSE;
// the flow events the single-player code reports (catMissionSelfTestEvent)
static bool32 catMtNisStarted = FALSE, catMtNisEnded = FALSE, catMtLevelLoaded = FALSE, catMtKasStarted = FALSE;
static bool32 catMtNisletEnded = FALSE, catMtArrived = FALSE;
// HWI-TEST (M2-R9 missions-02-09): the mission under test (/catMissionTestMission N; 1 = Mission01 from its NIS-first
//      start, 2 .. 17 from the warp start) and the flow of a mission after the first: the next level's load after the jump
static sdword catMtMission = 1;
static sdword catMtNextLevelTick = -1;
static udword catMtLevelTicks = 0;      // the mission level's last tick (the next level starts its counter at 0)

#define CAT_MT_TEAMS 64
static udword catMtTeamLabel[CAT_MT_TEAMS];
static udword catMtTeamFsm[CAT_MT_TEAMS];
static udword catMtTeamState[CAT_MT_TEAMS];
static sdword catMtTeamsSeen = 0;

void catMissionSelfTestSet(char *string)
{
    catMissionTestTicks = (sdword)strtol(string, NULL, 10);
    if (catMissionTestTicks <= 0)
    {
        catMissionTestTicks = 20000;
    }
    oracleSetMission(string);                   // the virtual clock and the quit after <ticks>
}

void catMissionSelfTestMissionSet(char *string)
{
    sdword mission = (sdword)strtol(string, NULL, 10);

    if ((mission >= MISSION_1_KHARAK_SYSTEM) && (mission <= MISSION_17))
    {
        catMtMission = mission;
    }
}

sdword catMissionSelfTestWarpLevel(void)
{
    return ((catMissionTestTicks > 0) && (catMtMission >= 2)) ? catMtMission : 0;
}

void catMissionSelfTestTutorialSet(char *string)
{
    catMissionTestTutorialTicks = (sdword)strtol(string, NULL, 10);
    if (catMissionTestTutorialTicks <= 0)
    {
        catMissionTestTutorialTicks = 2000;
    }
    oracleSetMission(string);
}

bool32 catMissionSelfTestRequested(void)
{
    return (catMissionTestTicks > 0);
}

bool32 catMissionSelfTestTutorialRequested(void)
{
    return (catMissionTestTutorialTicks > 0);
}

void catMissionSelfTestEvent(const char *what)
{
    if (catMissionTestTicks > 0 || catMissionTestTutorialTicks > 0)
    {
        dbgMessagef("missiontest: flow tick=%u %s", (unsigned)universe.univUpdateCounter, what);
        if ((catMissionTestTicks > 0) && (catMtMission >= 2))
        {
            char name[48];

            // M2-R9 missions-02-09: the mission's level (the warp start loads it at once), its KAS script, then, after
            //      the jump, the next mission's level or the staging level of its opening NIS
            snprintf(name, sizeof(name), "Mission%02d.mission", (int)catMtMission);
            if ((strncmp(what, "level loaded", 12) == 0) && (strstr(what, name) != NULL))
            {
                catMtLevelLoaded = TRUE;
            }
            snprintf(name, sizeof(name), "kas start mission%02d", (int)catMtMission);
            if (strcmp(what, name) == 0)
            {
                catMtKasStarted = TRUE;
            }
            snprintf(name, sizeof(name), "Mission%02d", (int)catMtMission + 1);
            if ((strncmp(what, "level loaded", 12) == 0) && (strstr(what, name) != NULL) && (catMtNextLevelTick < 0))
            {
                catMtNextLevelTick = (sdword)universe.univUpdateCounter;
                dbgMessagef("missiontest: next level tick=%u %s", (unsigned)universe.univUpdateCounter, what + 13);
            }
            return;
        }
        if (strncmp(what, "nis start nis", 13) == 0 && strstr(what, "n01.nis") != NULL)
        {
            catMtNisStarted = TRUE;
        }
        else if (strcmp(what, "nis stopped nis=1 nislet=-1") == 0)
        {
            catMtNisEnded = TRUE;
        }
        else if (strstr(what, "level loaded SinglePlayer") != NULL && strstr(what, "Mission01.mission") != NULL)
        {
            catMtLevelLoaded = TRUE;
        }
        else if ((strcmp(what, "kas start mission01") == 0) || (strcmp(what, "kas start tutorial1") == 0))
        {
            catMtKasStarted = TRUE;
        }
        else if (strcmp(what, "nis stopped nis=-1 nislet=1") == 0)
        {
            catMtNisletEnded = TRUE;
        }
    }
}

// the KAS FSMs' states, each change logged with its team's label hash
static void catMtWatchFsms(void)
{
    sdword i, k;
    AITeam *team;

    if (universe.players[1].aiPlayer == NULL)
    {
        return;
    }
    for (i = 0; i < universe.players[1].aiPlayer->teamsUsed; i++)
    {
        team = universe.players[1].aiPlayer->teams[i];
        if (team->teamType != ScriptTeam)
        {
            continue;
        }
        for (k = 0; k < catMtTeamsSeen; k++)
        {
            if (catMtTeamLabel[k] == team->kasLabel)
            {
                break;
            }
        }
        if (k == catMtTeamsSeen)
        {
            if (k >= CAT_MT_TEAMS)
            {
                continue;
            }
            catMtTeamLabel[k] = team->kasLabel;
            catMtTeamFsm[k] = catMtTeamState[k] = 0xffffffff;
            catMtTeamsSeen++;
        }
        if ((catMtTeamFsm[k] != team->kasFSMName) || (catMtTeamState[k] != team->kasStateName))
        {
            catMtTeamFsm[k] = team->kasFSMName;
            catMtTeamState[k] = team->kasStateName;
            dbgMessagef("missiontest: fsm tick=%u team=%07x fsm=%07x state=%07x ships=%d", (unsigned)universe.univUpdateCounter,
                        (unsigned)team->kasLabel, (unsigned)team->kasFSMName, (unsigned)team->kasStateName,
                        (int)team->shipList.selection->numShips);
        }
    }
}

// the objectives the script has created, each new one and each one gone logged
#define CAT_MT_OBJ 16
static char catMtObj[CAT_MT_OBJ][MAX_OBJECTIVE_LABEL_LENGTH + 1];
static sdword catMtObjUsed = 0;
static sdword catMtCounter = -1;
extern Objective **objectives;
extern sdword objectivesUsed;

static void catMtWatchObjectives(void)
{
    sdword i, k;
    CatKasVar *var;

    for (i = 0; i < objectivesUsed; i++)
    {
        for (k = 0; k < catMtObjUsed; k++)
        {
            if (strcmp(catMtObj[k], objectives[i]->label) == 0)
            {
                break;
            }
        }
        if ((k == catMtObjUsed) && (catMtObjUsed < CAT_MT_OBJ))
        {
            strcpy(catMtObj[catMtObjUsed++], objectives[i]->label);
            dbgMessagef("missiontest: objective created tick=%u %s", (unsigned)universe.univUpdateCounter, objectives[i]->label);
        }
    }
    for (k = 0; k < catMtObjUsed; k++)
    {
        if (catMtObj[k][0] == 0)
        {
            continue;
        }
        for (i = 0; i < objectivesUsed; i++)
        {
            if (strcmp(catMtObj[k], objectives[i]->label) == 0)
            {
                break;
            }
        }
        if (i == objectivesUsed)
        {
            dbgMessagef("missiontest: objective gone tick=%u %s", (unsigned)universe.univUpdateCounter, catMtObj[k]);
            catMtObj[k][0] = 0;
        }
    }
    var = (catMtMission == 1) ? cat_kasVarFind(0x098ddc43, 0x083f97b5) : NULL;  // the Objectives FSM's counter (Watch_Mission01_Objectives)
    if ((var != NULL) && (var->value != catMtCounter))
    {
        catMtCounter = var->value;
        dbgMessagef("missiontest: counter tick=%u %d", (unsigned)universe.univUpdateCounter, (int)catMtCounter);
        {
            Node *node;
            Ship *ship;
            sdword counts[TOTAL_NUM_SHIPS];
            sdword t;

            memset(counts, 0, sizeof(counts));
            for (node = universe.ShipList.head; node != NULL; node = node->next)
            {
                ship = (Ship *)listGetStructOfNode(node);
                if ((ship->playerowner == &universe.players[0]) && (ship->shiptype >= 0) && (ship->shiptype < TOTAL_NUM_SHIPS))
                {
                    counts[ship->shiptype]++;
                }
            }
            for (t = 0; t < TOTAL_NUM_SHIPS; t++)
            {
                if (counts[t] != 0)
                {
                    dbgMessagef("missiontest: fleet tick=%u type=%s n=%d RUs=%d", (unsigned)universe.univUpdateCounter,
                                ShipTypeToStr((ShipType)t), (int)counts[t], (int)universe.players[0].resourceUnits);
                }
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    The scripted player: Mission01's objectives (Watch_Mission01_Objectives 0x005d7050 creates them through
    ObjectiveCreate when FleetIntel's messages ask, in the order of its counter 0x083f97b5) and the event the test plays
    for each in place of the player's input, the moment the objective appears:
      DestroyBombers        the Taiidan bombers (team TaiidanBombers) are destroyed
      ExploreRegion         a recon launched from the mothership flies to the Taiidan resourcers (a launch and a move
                            through the command layer, as the player's launch manager and move order do) until they
                            are discovered (HasShipBeenDiscovered: the player's fog of war)
      DestroyResourcers     the resourcers (team TaiidanResourcers) are destroyed
      DestroyProxAttackers  the three proximity attack teams are destroyed
      DestroyDestroyer      the fleeing heavy cruiser (team TaiidanDestroyer) is destroyed
      Hyperspace            the player's hyperspace command (spHyperspaceButtonPushed, the hyperspace button's): the
                            KAS end of the mission, the hyperspace jump out
    The player's fleet is not flown into battle: a destroy objective's ships are destroyed with the mission language's
    own ShipsDamage (kasfShipsDamage 0x004960b0: ApplyDamageToTarget, the ships die through the game's death path), one
    call per 64 ticks while the objective stands.
-----------------------------------------------------------------------------*/
typedef enum { CAT_MT_DESTROY, CAT_MT_EXPLORE, CAT_MT_HYPERSPACE, CAT_MT_WAIT, CAT_MT_CUSTOM } CatMtKind;
#define CAT_MT_TEAMS_PER_OBJECTIVE 4
typedef struct
{
    const char *label;
    CatMtKind kind;
    udword teams[CAT_MT_TEAMS_PER_OBJECTIVE];
    sdword numTeams;
    bool32 held;            // M2-R9: a protect / escort objective the script never closes by name: it is held when it
                            //      stands to the mission's end (the Hyperspace message's ObjectivesDestroyAll)
} CatMtObjective;

// M2-R9 missions-02-09: each mission's objectives, in the order its Objectives FSM creates them (catMtMissions below)
#define CAT_MT_MAX_OBJECTIVES 16
static const CatMtObjective catMtMission01[] =
{
    { "DestroyBombers",       CAT_MT_DESTROY,    { 0x04fcebd3 }, 1 },                           // TaiidanBombers
    { "ExploreRegion",        CAT_MT_EXPLORE,    { 0x0cbd4dc3 }, 1 },                           // TaiidanResourcers
    { "DestroyResourcers",    CAT_MT_DESTROY,    { 0x0cbd4dc3 }, 1 },
    { "DestroyProxAttackers", CAT_MT_DESTROY,    { 0x0b25c8d1, 0x06a0d9b1, 0x0ebc0bc1 }, 3 },  // the TaiidanProxAttack teams
    { "DestroyDestroyer",     CAT_MT_DESTROY,    { 0x0518e0d2 }, 1 },                           // TaiidanDestroyer
    { "Hyperspace",           CAT_MT_HYPERSPACE, { 0 }, 0 },
};
static const CatMtObjective *catMtObjectives = catMtMission01;
static sdword catMtNumObjectives = (sdword)(sizeof(catMtMission01) / sizeof(catMtMission01[0]));
static sdword catMtCreatedTick[CAT_MT_MAX_OBJECTIVES];
static sdword catMtDoneTick[CAT_MT_MAX_OBJECTIVES];
static sdword catMtCreatedOrder[CAT_MT_MAX_OBJECTIVES];
static sdword catMtDoneOrder[CAT_MT_MAX_OBJECTIVES];
static sdword catMtCreatedCount = 0, catMtDoneCount = 0;
static udword catMtReconNumber = 0xffffffff;
static bool32 catMtHyperspaced = FALSE;
static bool32 catMtFailed = FALSE;
static sdword catMtEndTick = -1;
static sdword catMtLastDamaged[CAT_MT_MAX_OBJECTIVES][CAT_MT_TEAMS_PER_OBJECTIVE];
static sdword catMtHyperspaceTick = -1;


static sdword catMtObjectiveIndex(const char *label)
{
    sdword i;

    for (i = 0; i < catMtNumObjectives; i++)
    {
        if (strcmp(catMtObjectives[i].label, label) == 0)
        {
            return i;
        }
    }
    return -1;
}

static bool32 catMtObjectiveStands(sdword index)
{
    sdword i;

    for (i = 0; i < objectivesUsed; i++)
    {
        if (strcmp(objectives[i]->label, catMtObjectives[index].label) == 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}

// the destroy event: ShipsDamage on every ship of the objective's teams
static void catMtDestroyTeams(sdword index)
{
    sdword t;
    GrowSelection *ships;

    for (t = 0; t < catMtObjectives[index].numTeams; t++)
    {
        ships = kasAITeamShipsPtr(catMtObjectives[index].teams[t]);
        if ((ships != NULL) && (ships->selection->numShips > 0))
        {
            if (ships->selection->numShips != catMtLastDamaged[index][t])
            {
                catMtLastDamaged[index][t] = ships->selection->numShips;
                dbgMessagef("missiontest: event tick=%u %s: ShipsDamage on team %07x (%d ships)", (unsigned)universe.univUpdateCounter,
                            catMtObjectives[index].label, (unsigned)catMtObjectives[index].teams[t], (int)ships->selection->numShips);
            }
            kasfShipsDamage(ships, 1000000);
        }
    }
}

// the explore event: a recon is launched from the mothership and flies to the first resourcer (the recon is found by
//      its ship number every time: it may be destroyed)
static Ship *catMtFindPlayerShip(udword shipNumber)
{
    Node *node;
    Ship *ship;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner == &universe.players[0]) && (ship->shipID.shipNumber == shipNumber))
        {
            return ship;
        }
    }
    return NULL;
}

static Ship *catMtFindDockedRecon(udword shipNumber)
{
    Ship *mothership = universe.players[0].PlayerMothership;
    Node *node;
    Ship *ship;

    if ((mothership == NULL) || (mothership->shipsInsideMe == NULL))
    {
        return NULL;
    }
    for (node = mothership->shipsInsideMe->insideList.head; node != NULL; node = node->next)
    {
        ship = ((InsideShip *)listGetStructOfNode(node))->ship;
        if (ship->shipID.shipNumber == shipNumber)
        {
            return ship;
        }
    }
    return NULL;
}

static void catMtExplore(sdword index)
{
    GrowSelection *targets = kasAITeamShipsPtr(catMtObjectives[index].teams[0]);
    Ship *mothership = universe.players[0].PlayerMothership;
    Ship *recon = NULL;
    Node *node;
    SelectCommand selection;
    vector destination;

    if ((targets == NULL) || (targets->selection->numShips == 0) || (mothership == NULL))
    {
        return;
    }
    // a recon outside (the one launched before, else any), else one docked in the mothership is launched (the launch
    //      manager's order)
    if (catMtReconNumber != 0xffffffff)
    {
        recon = catMtFindPlayerShip(catMtReconNumber);
    }
    for (node = universe.ShipList.head; (recon == NULL) && (node != NULL); node = node->next)
    {
        Ship *ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner == &universe.players[0]) && (ship->shiptype == sRecon))
        {
            recon = ship;
        }
    }
    if ((recon == NULL) && (catMtReconNumber == 0xffffffff) && (mothership->shipsInsideMe != NULL))
    {
        for (node = mothership->shipsInsideMe->insideList.head; node != NULL; node = node->next)
        {
            Ship *ship = ((InsideShip *)listGetStructOfNode(node))->ship;
            if (ship->shiptype == sRecon)
            {
                selection.numShips = 1;
                selection.ShipPtr[0] = ship;
                dbgMessagef("missiontest: event tick=%u %s: launch recon %u", (unsigned)universe.univUpdateCounter,
                            catMtObjectives[index].label, (unsigned)ship->shipID.shipNumber);
                clWrapLaunchMultipleShips(&universe.mainCommandLayer, &selection, mothership);
                catMtReconNumber = ship->shipID.shipNumber;
                return;
            }
        }
    }
    if ((recon == NULL) || bitTest(recon->header.flags, SOF_Dead) || !univSpaceObjInRenderList((SpaceObj *)recon))
    {
        if ((recon == NULL) && (catMtReconNumber != 0xffffffff) && (catMtFindDockedRecon(catMtReconNumber) == NULL))
        {
            catMtReconNumber = 0xffffffff;      // the launched recon is gone: launch another
        }
        return;                                 // none, or still launching
    }
    catMtReconNumber = recon->shipID.shipNumber;
    destination = targets->selection->ShipPtr[0]->obj.posinfo.position;
    selection.numShips = 1;
    selection.ShipPtr[0] = recon;
    dbgMessagef("missiontest: event tick=%u %s: move recon %u to (%.0f, %.0f, %.0f)", (unsigned)universe.univUpdateCounter,
                catMtObjectives[index].label, (unsigned)recon->shipID.shipNumber, destination.x, destination.y, destination.z);
    clWrapMove(&universe.mainCommandLayer, &selection, 0, recon->obj.posinfo.position, 1, &destination);
}

/*-----------------------------------------------------------------------------
    M2-R9 missions-02-09: the scripted player's toolbox for the missions after the first. Every action goes through
    the game's own interfaces, as Mission01's events do (decision 77; no input): the construction manager's job queue
    (cmBuildJobsAdd, as the build button), the research manager's order (clWrapResearch with a free lab, as the
    research screen's button), the command layer (moves, launches, docks, harvests, the special action, the ship
    upgrade, guard, the mimic) and, for the fights the test does not fly, the mission language's own ShipsDamage. Each
    action is logged as a "missiontest: event" line.
-----------------------------------------------------------------------------*/
static bool32 catMtStands(const char *label)
{
    sdword i;

    for (i = 0; i < objectivesUsed; i++)
    {
        if (strcmp(objectives[i]->label, label) == 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}

// the player's ships of a type (outside, not dead, not hyperspacing); type -1: any. Returns the count
static sdword catMtPlayerShips(ShipType type, SelectCommand *out, sdword max)
{
    Node *node;
    Ship *ship;

    out->numShips = 0;
    for (node = universe.ShipList.head; (node != NULL) && (out->numShips < max); node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner != &universe.players[0]) || bitTest(ship->header.flags, SOF_Dead) ||
            bitTest(ship->header.flags, SOF_Hyperspace) || ((type >= 0) && (ship->shiptype != type)))
        {
            continue;
        }
        out->ShipPtr[out->numShips++] = ship;
    }
    return out->numShips;
}

// the player's ships of the type that carry out a special order (a worker repairing or salvaging): the order is
//      given again only when none does (a new order would restart a tow under way)
static sdword catMtPlayerShipsOnSpecial(ShipType type)
{
    Node *node;
    Ship *ship;
    CommandToDo *command;
    sdword n = 0;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner == &universe.players[0]) && (ship->shiptype == type) && !bitTest(ship->header.flags, SOF_Dead) &&
            ((command = getShipAndItsCommand(&universe.mainCommandLayer, ship)) != NULL) &&
            (command->ordertype.order == COMMAND_SPECIAL))
        {
            n++;
        }
    }
    return n;
}

static sdword catMtPlayerShipCount(ShipType type)
{
    Node *node;
    Ship *ship;
    sdword n = 0;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner == &universe.players[0]) && (ship->shiptype == type) && !bitTest(ship->header.flags, SOF_Dead))
        {
            n++;
        }
    }
    return n;
}

static bool32 catMtHasTech(const char *techname)
{
    TechnologyType tech = StrToTechType(techname);

    return (tech >= 0) && cat_rmTechSetHas(universe.players[0].researchinfo.HasTechnology, tech);
}

// the construction manager's build button: one job of the type at the mothership's factory, if it can be built
static bool32 catMtBuild(const char *why, ShipType type)
{
    Ship *mothership = universe.players[0].PlayerMothership;
    shipsinprogress *factory = (mothership != NULL) ? cmFindFactory(mothership) : NULL;

    if ((factory == NULL) || !cmCanBuildShipType(mothership, type, TRUE))
    {
        return FALSE;
    }
    dbgMessagef("missiontest: event tick=%u %s: build %s (the construction manager's job; RUs=%d)",
                (unsigned)universe.univUpdateCounter, why, ShipTypeToStr(type), (int)universe.players[0].resourceUnits);
    cmBuildJobsAdd(factory, GetShipStaticInfo(type, universe.players[0].race), 1);
    return TRUE;
}

// the research manager's button: the technology in a free lab of its kind (once; FALSE while no lab is free)
static bool32 catMtResearch(const char *why, const char *techname)
{
    TechnologyType tech = StrToTechType(techname);
    sdword lab;

    if ((tech < 0) || catMtHasTech(techname) || (Researching(&universe.players[0], tech) != NULL))
    {
        return TRUE;
    }
    lab = rmFindFreeLab(&universe.players[0], tech);
    if (lab < 0)
    {
        return FALSE;
    }
    dbgMessagef("missiontest: event tick=%u %s: research %s (lab %d)", (unsigned)universe.univUpdateCounter, why,
                techname, (int)lab);
    clWrapResearch(0, 0, (udword)lab, (udword)tech);
    return TRUE;
}

// the destroy event: the mission language's ShipsDamage on a selection (the fight the test does not fly)
static void catMtDamage(const char *why, GrowSelection *ships)
{
    if ((ships == NULL) || (ships->selection == NULL) || (ships->selection->numShips == 0))
    {
        return;
    }
    dbgMessagef("missiontest: event tick=%u %s: ShipsDamage (%d ships)", (unsigned)universe.univUpdateCounter, why,
                (int)ships->selection->numShips);
    kasfShipsDamage(ships, 1000000);
}

// ships the escort's fight leaves alone (a mission's own flow needs them alive, NULL: none)
static GrowSelection *catMtSpared = NULL;

// ShipsDamage that leaves each ship of the selection at the fraction of its hull (the points above it; a ship already
//      near it is left alone)
static void catMtDamageTo(const char *why, GrowSelection *ships, real32 fraction)
{
    GrowSelection one;
    Ship *ship;
    real32 above;
    sdword i;

    for (i = 0; (ships != NULL) && (ships->selection != NULL) && (i < ships->selection->numShips); i++)
    {
        ship = ships->selection->ShipPtr[i];
        above = ship->targetable.health - fraction * ship->staticinfo->health.maxhealth;
        if (above > 0.05f * ship->staticinfo->health.maxhealth)
        {
            dbgMessagef("missiontest: event tick=%u %s: ShipsDamage %d on %s#%u (to %d%% of the hull)",
                        (unsigned)universe.univUpdateCounter, why, (int)above, ShipTypeToStr(ship->shiptype),
                        (unsigned)ship->shipID.shipNumber, (int)(fraction * 100.0f));
            growSelectInit(&one);
            growSelectAddShip(&one, ship);
            kasfShipsDamage(&one, (sdword)above);
            growSelectClose(&one);
        }
    }
}

// the escort's fight around a point (the player's defence the test does not fly): every hostile ship (but a
//      proximity sensor, which some objectives name, and the spared ships) and mine within the radius is destroyed
//      with ShipsDamage
static void catMtDamageAround(const char *why, vector *centre, real32 radius)
{
    GrowSelection hostile;
    Node *node;
    SpaceObj *obj;
    vector diff;

    growSelectInit(&hostile);
    for (node = universe.SpaceObjList.head; node != NULL; node = node->next)
    {
        obj = (SpaceObj *)listGetStructOfNode(node);
        if (bitTest(obj->header.flags, SOF_Dead) || !bitTest(obj->header.flags, SOF_Targetable))
        {
            continue;
        }
        if (((obj->header.objtype == OBJ_ShipType) && !allianceIsShipAlly((Ship *)obj, &universe.players[0]) &&
             (((Ship *)obj)->shiptype != kProximitySensor) && (((Ship *)obj)->shiptype != tProximitySensor)) ||
            ((obj->header.objtype == OBJ_MissileType) && (((Missile *)obj)->missileType == MISSILE_Mine) &&
             (((Missile *)obj)->playerowner != &universe.players[0])))
        {
            vecSub(diff, obj->posinfo.position, *centre);
            if ((obj->header.objtype == OBJ_ShipType) && (catMtSpared != NULL) && (catMtSpared->selection != NULL) &&
                ShipInSelection(catMtSpared->selection, (Ship *)obj))
            {
                continue;
            }
            if (vecMagnitudeSquared(diff) < radius * radius)
            {
                growSelectAddShip(&hostile, (Ship *)obj);
            }
        }
    }
    catMtDamage(why, &hostile);
    growSelectClose(&hostile);
}

// the harvest order (the command layer's collect order, the [H] key): each worker of the player that is not
//      harvesting, salvaging, repairing or docking collects the asteroid nearest to it
static void catMtHarvest(const char *why)
{
    Node *node, *rnode;
    Ship *ship;
    Resource *resource, *best;
    SelectCommand one;
    real32 bestdist, dist;
    vector diff;
    sdword ordered = 0;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        CommandToDo *command;

        ship = (Ship *)listGetStructOfNode(node);
        command = getShipAndItsCommand(&universe.mainCommandLayer, ship);
        if ((ship->playerowner != &universe.players[0]) || (ship->shiptype != sWorker) ||
            bitTest(ship->header.flags, SOF_Dead) ||
            ((command != NULL) && ((command->ordertype.order == COMMAND_COLLECT_RESOURCES) ||
                                   (command->ordertype.order == COMMAND_SPECIAL) || (command->ordertype.order == COMMAND_DOCK))))
        {
            continue;
        }
        best = NULL;
        bestdist = REALlyBig;
        for (rnode = universe.ResourceList.head; rnode != NULL; rnode = rnode->next)
        {
            resource = (Resource *)listGetStructOfNode(rnode);
            if ((resource->header.objtype != OBJ_AsteroidType) || bitTest(resource->header.flags, SOF_Dead))
            {
                continue;
            }
            vecSub(diff, resource->obj.posinfo.position, ship->obj.posinfo.position);
            dist = vecMagnitudeSquared(diff);
            if (dist < bestdist)
            {
                bestdist = dist;
                best = resource;
            }
        }
        if (best != NULL)
        {
            one.numShips = 1;
            one.ShipPtr[0] = ship;
            clWrapCollectResource(&universe.mainCommandLayer, &one, best);
            ordered++;
        }
    }
    if (ordered > 0)
    {
        dbgMessagef("missiontest: event tick=%u %s: harvest (%d workers, the collect order)", (unsigned)universe.univUpdateCounter,
                    why, (int)ordered);
    }
}

// a team's ships (NULL when the team or its ships are gone)
static GrowSelection *catMtTeam(udword teamHash)
{
    GrowSelection *ships = (universe.players[1].aiPlayer != NULL) ? kasAITeamShipsPtr(teamHash) : NULL;

    return ((ships != NULL) && (ships->selection != NULL) && (ships->selection->numShips > 0)) ? ships : NULL;
}

// the command layer's move order to a point for the selection
static void catMtMove(const char *why, SelectCommand *selection, vector *destination)
{
    if (selection->numShips == 0)
    {
        return;
    }
    dbgMessagef("missiontest: event tick=%u %s: move %d ships (%s ..) to (%.0f, %.0f, %.0f)",
                (unsigned)universe.univUpdateCounter, why, (int)selection->numShips,
                ShipTypeToStr(selection->ShipPtr[0]->shiptype), destination->x, destination->y, destination->z);
    clWrapMove(&universe.mainCommandLayer, selection, 0, selection->ShipPtr[0]->obj.posinfo.position, 1, destination);
}

// the special action menu ([Z]; cat_uiSpecialTargetCommand 0x00564060, which acts on the current selection): the
//      ships are selected through the selection API, then the ability (0 salvage, 1 repair, 2 leech, 3 swarm, 4 EMP,
//      5 infect, 6 ram, 7 the linked missiles) is ordered on a copy of the targets (NULL: each ship's nearest)
static void catMtSpecial(const char *why, sdword ability, SelectCommand *selection, GrowSelection *targets)
{
    SelectCommand *copy = NULL;
    sdword i;

    if ((selection->numShips == 0) || ((targets != NULL) && (targets->selection->numShips == 0)))
    {
        return;
    }
    dbgMessagef("missiontest: event tick=%u %s: special action %d of %d %s on %d targets (%s ..)",
                (unsigned)universe.univUpdateCounter, why, (int)ability, (int)selection->numShips,
                ShipTypeToStr(selection->ShipPtr[0]->shiptype), (targets != NULL) ? (int)targets->selection->numShips : 0,
                (targets != NULL) ? ShipTypeToStr(targets->selection->ShipPtr[0]->shiptype) : "nearest");
    selSelected.numShips = 0;
    for (i = 0; i < selection->numShips; i++)
    {
        selSelectionAddSingleShip(&selSelected, selection->ShipPtr[i]);
    }
    if (targets != NULL)
    {
        copy = memAlloc(sizeofSelectCommand(targets->selection->numShips), "catMtSpecial", 0);
        memcpy(copy, targets->selection, sizeofSelectCommand(targets->selection->numShips));
    }
    cat_uiSpecialTargetCommand(copy, ability);
    if (copy != NULL)
    {
        memFree(copy);
    }
}

// the ship upgrade button for every ship of the type
static void catMtUpgrade(const char *why, ShipType type)
{
    SelectCommand *selection = memAlloc(sizeofSelectCommand(COMMAND_MAX_SHIPS), "catMtUpgrade", 0);

    if (catMtPlayerShips(type, selection, COMMAND_MAX_SHIPS) > 0)
    {
        dbgMessagef("missiontest: event tick=%u %s: upgrade %d %s", (unsigned)universe.univUpdateCounter, why,
                    (int)selection->numShips, ShipTypeToStr(type));
        cat_clWrapUpgradeShips(&universe.mainCommandLayer, selection, FALSE);
    }
    memFree(selection);
}

// a recon of the player flies to the point (a docked one is launched from the mothership first): the explore event
//      of Mission01 for any point
static udword catMtScoutNumber = 0xffffffff;
static sdword catMtScoutBuildTick = -100000;

static void catMtScout(const char *why, vector *destination)
{
    Ship *mothership = universe.players[0].PlayerMothership;
    Ship *recon = NULL;
    Node *node;
    SelectCommand selection;

    if (catMtScoutNumber != 0xffffffff)
    {
        recon = catMtFindPlayerShip(catMtScoutNumber);
    }
    for (node = universe.ShipList.head; (recon == NULL) && (node != NULL); node = node->next)
    {
        Ship *ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner == &universe.players[0]) && (ship->shiptype == sRecon) &&
            !bitTest(ship->header.flags, SOF_Dead) && univSpaceObjInRenderList((SpaceObj *)ship))
        {
            recon = ship;
        }
    }
    if ((recon == NULL) && (mothership != NULL) && (mothership->shipsInsideMe != NULL))
    {
        for (node = mothership->shipsInsideMe->insideList.head; node != NULL; node = node->next)
        {
            Ship *ship = ((InsideShip *)listGetStructOfNode(node))->ship;
            if (ship->shiptype == sRecon)
            {
                selection.numShips = 1;
                selection.ShipPtr[0] = ship;
                dbgMessagef("missiontest: event tick=%u %s: launch recon %u", (unsigned)universe.univUpdateCounter, why,
                            (unsigned)ship->shipID.shipNumber);
                clWrapLaunchMultipleShips(&universe.mainCommandLayer, &selection, mothership);
                catMtScoutNumber = ship->shipID.shipNumber;
                return;
            }
        }
    }
    if ((recon == NULL) || bitTest(recon->header.flags, SOF_Dead) || !univSpaceObjInRenderList((SpaceObj *)recon))
    {
        // none left (the mines and patrols take them): a new one through the construction manager, as the player
        //      would build one
        if ((recon == NULL) && (catMtPlayerShipCount(sRecon) == 0) && (catMtScoutBuildTick + 2048 < (sdword)universe.univUpdateCounter))
        {
            catMtScoutBuildTick = (sdword)universe.univUpdateCounter;
            catMtScoutNumber = 0xffffffff;
            catMtBuild(why, sRecon);
        }
        return;
    }
    catMtScoutNumber = recon->shipID.shipNumber;
    selection.numShips = 1;
    selection.ShipPtr[0] = recon;
    catMtMove(why, &selection, destination);
}

// the KAS variable's value (scope hash, name hash), 0 when it does not exist
static sdword catMtVar(udword scope, udword name)
{
    CatKasVar *var = cat_kasVarFind(scope, name);

    return (var != NULL) ? var->value : 0;
}

/*-----------------------------------------------------------------------------
    Mission02 ("Rescue Kith Destroyer And Find Pod"; docs/mission-code-map.md 8): Watch_Mission02_Objectives
    0x005dcbd0 creates the objectives on FleetIntel's messages. The player's part:
      FindDestroyer    a recon flies to the BUSHAN-RE (team kDestroyer 0x0893b502) until it is discovered
                       (Watch_Mission02_Destroyer_Find: HasShipBeenDiscovered; the destroyer then joins the player)
      Build            the engineering module (sMothershipMicro, built once FleetIntel enables it) through the
                       construction manager; then TECH_REPAIR and TECH_SALVAGE through the research manager and the
                       workers upgraded (the objectives' texts: "the WORKERS ... must be upgraded after researching")
      RepairDestroyer  every worker's special action on the destroyer (a worker's repair) until its health is 90%
                       (Watch_Mission02_Destroyer_Repair; capped at 75% until the pTricky ambush has struck); the
                       hostile ships within 10000 of her are destroyed with ShipsDamage (the escort the text asks for)
      SalvagePod       two workers' special action (salvage) on the message pod (team dMessagePod 0x0712bfb4)
                       until it is salvaged (GetShipCountSalvaged("dMessagePod"))
      DestroyPirates   ShipsDamage on the "Pirates" selection 0x04e668f3 (Watch_Mission02_Carrier_Null: when it
                       is empty; the raiders come for the pod after it appears)
      Hyperspace       the hyperspace button
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission02[] =
{
    { "FindDestroyer",   CAT_MT_CUSTOM,     { 0 }, 0 },
    { "Build",           CAT_MT_CUSTOM,     { 0 }, 0 },
    { "RepairDestroyer", CAT_MT_CUSTOM,     { 0 }, 0 },
    { "SalvagePod",      CAT_MT_CUSTOM,     { 0 }, 0 },
    { "DestroyPirates",  CAT_MT_CUSTOM,     { 0 }, 0 },
    { "Hyperspace",      CAT_MT_HYPERSPACE, { 0 }, 0 },
};

static void catMtPlayMission02(void)
{
    static bool32 microQueued = FALSE, workersUpgraded = FALSE;
    GrowSelection *team;
    SelectCommand *workers;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    if (catMtStands("FindDestroyer") && ((team = catMtTeam(0x0893b502)) != NULL) && ((universe.univUpdateCounter % 64) == 8))
    {
        catMtScout("FindDestroyer", &team->selection->ShipPtr[0]->obj.posinfo.position);
    }
    if (!microQueued && (catMtPlayerShipCount(sMothershipMicro) == 0))
    {
        microQueued = catMtBuild("Build", sMothershipMicro);
    }
    if (catMtPlayerShipCount(sMothershipMicro) > 0)
    {
        catMtResearch("Build", "TECH_REPAIR");
        catMtResearch("Build", "TECH_SALVAGE");
    }
    if (!workersUpgraded && catMtHasTech("TECH_REPAIR") && catMtHasTech("TECH_SALVAGE"))
    {
        workersUpgraded = TRUE;
        catMtUpgrade("RepairDestroyer", sWorker);
    }
    workers = memAlloc(sizeofSelectCommand(COMMAND_MAX_SHIPS), "catMtWorkers", 0);
    if (catMtStands("RepairDestroyer") && ((universe.univUpdateCounter % 256) == 8) && ((team = catMtTeam(0x0893b502)) != NULL) &&
        (catMtPlayerShipsOnSpecial(sWorker) == 0) && (catMtPlayerShips(sWorker, workers, COMMAND_MAX_SHIPS) > 0))
    {
        catMtSpecial("RepairDestroyer", 1, workers, team);
    }
    if (catMtStands("RepairDestroyer") && ((universe.univUpdateCounter % 64) == 40) && ((team = catMtTeam(0x0893b502)) != NULL))
    {
        catMtDamageAround("RepairDestroyer (the escort)", &team->selection->ShipPtr[0]->obj.posinfo.position, 10000.0f);
    }
    if (catMtStands("SalvagePod") && ((universe.univUpdateCounter % 256) == 136) && ((team = catMtTeam(0x0712bfb4)) != NULL) &&
        (catMtPlayerShipsOnSpecial(sWorker) == 0) && (catMtPlayerShips(sWorker, workers, 2) > 0))
    {
        catMtSpecial("SalvagePod", 0, workers, team);
    }
    memFree(workers);
    if (catMtStands("DestroyPirates") && ((universe.univUpdateCounter % 64) == 24))
    {
        catMtDamage("DestroyPirates", kasGrowSelectionPtr(0x04e668f3));
    }
}

/*-----------------------------------------------------------------------------
    Mission03 ("Rescue Ecclesiam From Pirate Blockade"): Watch_Mission03_Objectives 0x005e21d0. The player's part:
      BuildHangarModule  the docking bay module (sMothershipDockingBay; the warp start's level 3 record has none, so
                         the script asks for it; the Hyperspace objective needs it too) through the construction
                         manager
      Find               a mimic disguised as a pirate ship (the mimic order on an enemy ship, cat_mrMimicTarget: the
                         mines do not track it; the recons die in the minefield) flies to the CLEE-SAN (team Cleesan
                         0x0809a71e) until it is discovered (Watch_Mission03_Cleesan_Wait: she joins the player)
      Mimic              the mimic stays at the CLEE-SAN (Watch_Mission03_Cleesan_Hold: a mimic or MCV within 2000)
      Lure               the mines near the CLEE-SAN (FindMinesNearTeam 14000, the script's selection 0x0e8e2f1e) are
                         destroyed with ShipsDamage (a destroy objective: the acolytes the text sends die in the
                         minefield in the test's hands), as are the hostile ships and mines within 8000 of her (her
                         escort); the command ship flies to meet her and she docks with it (Watch_Mission03_Cleesan_Go /
                         IonsSame: TeamDockedReadyForLaunch)
      Hyperspace         the hyperspace button; the roll call
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission03[] =
{
    { "BuildHangarModule", CAT_MT_CUSTOM,     { 0 }, 0 },
    { "Find",              CAT_MT_CUSTOM,     { 0 }, 0 },
    { "Mimic",             CAT_MT_CUSTOM,     { 0 }, 0 },
    { "Lure",              CAT_MT_CUSTOM,     { 0 }, 0 },
    { "Hyperspace",        CAT_MT_HYPERSPACE, { 0 }, 0 },
};

// the command layer's attack order of the selection on the targets (any objects: ships, mines)
static void catMtAttack(const char *why, SelectCommand *selection, GrowSelection *targets)
{
    if ((selection->numShips == 0) || (targets == NULL) || (targets->selection->numShips == 0))
    {
        return;
    }
    dbgMessagef("missiontest: event tick=%u %s: attack of %d %s on %d targets", (unsigned)universe.univUpdateCounter, why,
                (int)selection->numShips, ShipTypeToStr(selection->ShipPtr[0]->shiptype), (int)targets->selection->numShips);
    clWrapAttack(&universe.mainCommandLayer, selection, (AttackCommand *)targets->selection);
}

// the mimic order on an enemy ship (the UI's: cat_mrMimicTarget on the selection): the first ship of player 1 that is
//      not a capital ship
static void catMtMimicEnemy(const char *why, SelectCommand *mimics)
{
    Node *node;
    Ship *ship;
    sdword i;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner == &universe.players[1]) && !bitTest(ship->header.flags, SOF_Dead) &&
            ((ship->staticinfo->shipclass == CLASS_Fighter) || (ship->staticinfo->shipclass == CLASS_Corvette)))
        {
            dbgMessagef("missiontest: event tick=%u %s: %d mimics mimic %s#%u (the mimic order)", (unsigned)universe.univUpdateCounter,
                        why, (int)mimics->numShips, ShipTypeToStr(ship->shiptype), (unsigned)ship->shipID.shipNumber);
            selSelected.numShips = 0;
            for (i = 0; i < mimics->numShips; i++)
            {
                selSelectionAddSingleShip(&selSelected, mimics->ShipPtr[i]);
            }
            cat_mrMimicTarget((SpaceObj *)ship);
            return;
        }
    }
}

static void catMtPlayMission03(void)
{
    static bool32 bayQueued = FALSE;
    GrowSelection *team;
    SelectCommand *ships;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    if (catMtStands("BuildHangarModule") && !bayQueued && (catMtPlayerShipCount(sMothershipDockingBay) == 0))
    {
        bayQueued = catMtBuild("BuildHangarModule", sMothershipDockingBay);
    }
    ships = memAlloc(sizeofSelectCommand(COMMAND_MAX_SHIPS), "catMtShips", 0);
    if ((catMtStands("Find") || catMtStands("Mimic")) && ((universe.univUpdateCounter % 256) == 8) &&
        ((team = catMtTeam(0x0809a71e)) != NULL) && (catMtPlayerShips(sMimic, ships, 1) > 0))
    {
        if (!((MimicSpec *)ships->ShipPtr[0]->ShipSpecifics)->started)
        {
            catMtMimicEnemy(catMtStands("Find") ? "Find" : "Mimic", ships);
        }
        else
        {
            catMtMove(catMtStands("Find") ? "Find" : "Mimic", ships, &team->selection->ShipPtr[0]->obj.posinfo.position);
        }
    }
    if (catMtStands("Lure") && ((universe.univUpdateCounter % 64) == 24))
    {
        catMtDamage("Lure", kasGrowSelectionPtr(0x0e8e2f1e));
        if ((team = catMtTeam(0x0809a71e)) != NULL)
        {
            catMtDamageAround("Lure (the escort)", &team->selection->ShipPtr[0]->obj.posinfo.position, 8000.0f);
        }
    }
    if (catMtStands("Lure") && ((universe.univUpdateCounter % 256) == 40) && ((team = catMtTeam(0x0809a71e)) != NULL) &&
        (universe.players[0].PlayerMothership != NULL))
    {
        SelectCommand mothership;

        mothership.numShips = 1;
        mothership.ShipPtr[0] = universe.players[0].PlayerMothership;
        catMtMove("Lure (the command ship meets the CLEE-SAN)", &mothership, &team->selection->ShipPtr[0]->obj.posinfo.position);
    }
    memFree(ships);
}

/*-----------------------------------------------------------------------------
    Mission04: Watch_Mission04_Objectives 0x005e66b0. The player's part:
      Replace     the hangar (docking bay) module through the construction manager (the warp start's level 4 record
                  has none)
      Escort      the ACVs (5 or more) and the acolytes are ordered to guard the CLEE-SAN (team Ecclesiam 0x080a342d;
                  the command layer's protect order, the [G] key; Watch_Mission04_Ecclesiam_NeedEscort counts the ships
                  guarding her within 6000: 10 acolytes or 5 ACVs)
      Research    TECH_RESEARCH_CLEESAN_ATTACK through the research manager once the script allows it (the objective is
                  then set complete, ObjectiveSet 1)
      Destroy     the infected ships that attack the command ship are destroyed with ShipsDamage (hostile ships within
                  12000 of it); a protect objective the script ends with the mission (held)
      Hyperspace  the hyperspace button; the roll call
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission04[] =
{
    { "Replace",    CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Escort",     CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Research",   CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Destroy",    CAT_MT_CUSTOM,     { 0 }, 0, TRUE  },
    { "Hyperspace", CAT_MT_HYPERSPACE, { 0 }, 0, FALSE },
};

// the command layer's protect (guard) order of the selection on the ships
static void catMtGuard(const char *why, SelectCommand *selection, GrowSelection *ships)
{
    if ((selection->numShips == 0) || (ships == NULL) || (ships->selection->numShips == 0))
    {
        return;
    }
    dbgMessagef("missiontest: event tick=%u %s: %d %s guard %d ships (%s ..)", (unsigned)universe.univUpdateCounter, why,
                (int)selection->numShips, ShipTypeToStr(selection->ShipPtr[0]->shiptype), (int)ships->selection->numShips,
                ShipTypeToStr(ships->selection->ShipPtr[0]->shiptype));
    clWrapProtect(&universe.mainCommandLayer, selection, (ProtectCommand *)ships->selection);
}

static void catMtPlayMission04(void)
{
    static bool32 bayQueued = FALSE;
    GrowSelection *team;
    SelectCommand *ships;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    if (catMtStands("Replace") && !bayQueued && (catMtPlayerShipCount(sMothershipDockingBay) == 0))
    {
        bayQueued = catMtBuild("Replace", sMothershipDockingBay);
    }
    ships = memAlloc(sizeofSelectCommand(COMMAND_MAX_SHIPS), "catMtShips", 0);
    if (catMtStands("Escort") && ((universe.univUpdateCounter % 512) == 8) && ((team = catMtTeam(0x080a342d)) != NULL))
    {
        if (catMtPlayerShips(sACV, ships, COMMAND_MAX_SHIPS) > 0)
        {
            catMtGuard("Escort", ships, team);
        }
        if (catMtPlayerShips(sAcolyte, ships, COMMAND_MAX_SHIPS) > 0)
        {
            catMtGuard("Escort", ships, team);
        }
    }
    memFree(ships);
    if (catMtStands("Research"))
    {
        catMtResearch("Research", "TECH_RESEARCH_CLEESAN_ATTACK");
    }
    if (catMtStands("Destroy") && ((universe.univUpdateCounter % 64) == 24) && (universe.players[0].PlayerMothership != NULL))
    {
        catMtDamageAround("Destroy", &universe.players[0].PlayerMothership->obj.posinfo.position, 12000.0f);
    }
}

/*-----------------------------------------------------------------------------
    Mission05 ("Beast Attacks Bentusi, Station Scuttled"): Watch_Mission05_Objectives 0x005ebd50. The player's part:
      InterceptBentusi  a recon flies to the Bentusi station (team trCity 0x05967e99) until it is discovered
                        (Watch_Mission05_Bentusi_UnFound)
      SaveBentusi       the hostile ships within 10000 of the station are destroyed with ShipsDamage (its defence);
                        the script ends the objective when the station is scuttled (Watch_Mission05_Bentusi_Dead)
      ProtectCaalShto   the hostile ships within 10000 of the CAAL-SHTO (team CaalShto 0x056179ff) are destroyed
                        with ShipsDamage (her defence)
      DestroyBeast      ShipsDamage on the script's selection of the remaining Beast ships (0x0593fe0c: the enemy
                        ships but the Beast workers and processors; Watch_Mission05_Objectives closes both when it is
                        empty)
      BuildEngineering  the advanced engineering module (sMothershipSpecial, once the script enables it) through the
                        construction manager; the workers harvest the nearest asteroids throughout (the collect
                        order) for its resources
      Hyperspace        the hyperspace button (a catSpMissionNextLevel mission: the letterbox jump)
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission05[] =
{
    { "InterceptBentusi", CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "SaveBentusi",      CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "ProtectCaalShto",  CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "DestroyBeast",     CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "BuildEngineering", CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Hyperspace",       CAT_MT_HYPERSPACE, { 0 }, 0, FALSE },
};

static void catMtPlayMission05(void)
{
    static bool32 specialQueued = FALSE;
    GrowSelection *team;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    if (catMtStands("InterceptBentusi") && ((universe.univUpdateCounter % 64) == 8) && ((team = catMtTeam(0x05967e99)) != NULL))
    {
        catMtScout("InterceptBentusi", &team->selection->ShipPtr[0]->obj.posinfo.position);
    }
    if ((universe.univUpdateCounter % 512) == 8)
    {
        catMtHarvest("BuildEngineering (the resources)");
    }
    if (catMtStands("SaveBentusi") && ((universe.univUpdateCounter % 64) == 24) && ((team = catMtTeam(0x05967e99)) != NULL))
    {
        catMtDamageAround("SaveBentusi", &team->selection->ShipPtr[0]->obj.posinfo.position, 10000.0f);
    }
    if (catMtStands("ProtectCaalShto") && ((universe.univUpdateCounter % 64) == 40) && ((team = catMtTeam(0x056179ff)) != NULL))
    {
        catMtDamageAround("ProtectCaalShto", &team->selection->ShipPtr[0]->obj.posinfo.position, 10000.0f);
    }
    if (catMtStands("DestroyBeast") && ((universe.univUpdateCounter % 64) == 56))
    {
        catMtDamage("DestroyBeast", kasGrowSelectionPtr(0x0593fe0c));
    }
    if (catMtStands("BuildEngineering") && !specialQueued && (catMtPlayerShipCount(sMothershipSpecial) == 0))
    {
        specialQueued = catMtBuild("BuildEngineering", sMothershipSpecial);
    }
}

/*-----------------------------------------------------------------------------
    Mission06 ("Escourt Callisto Into Nebulae"): Watch_Mission06_Objectives 0x005f5770. The player's part:
      Explore     a recon flies to the Taiidan research station (team 0x088689ae) until it is discovered
                  (Watch_Mission06)
      Scout       a recon flies to the nav points the script pings, Nav1 .. Nav4 in turn (vectors 0x00052671 ..;
                  Watch_Mission06_CaalShto: a recon within 5000; the CAAL-SHTO follows the scouted route)
      Escort      the command ship guards the CAAL-SHTO (the command layer's protect order, the [G] key); the
                  hostile ships and mines within 16000 of her (the radius in which Watch_Mission06_CaalShto_Wait
                  stops her for mines) are destroyed with ShipsDamage (the escort the text advises)
      Mines       the minelayers (team Minelayer 0x02a03c42) and the mines the script found (selection 0x0051e2a3)
                  are destroyed with ShipsDamage
      HvCruiser   the heavy cruiser group (the script's selection 0x0879aa02) is spared by the escort's fight until
                  the player has discovered it (Watch_Mission06_HvCruiser: the discovery creates the objective and
                  tells the CAAL-SHTO the last nav point is clear, 0x080966d4); then ShipsDamage leaves each of its ships
                  at 45% of the hull: below 51% the scene of the cruiser runs (kasfTeamHealthLowest, the message
                  0x0879aa02 to FleetIntel) and creates Leech; then the group is destroyed with ShipsDamage and the
                  objective closes once the selection is empty
      Leech       the cruiser scene creates it (0x0a796318); TECH_PHASED_TELEMETRY through the research manager
      Hyperspace  the hyperspace button; the roll call
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission06[] =
{
    { "Explore",    CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Scout",      CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Escort",     CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Mines",      CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "HvCruiser",  CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Leech",      CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Hyperspace", CAT_MT_HYPERSPACE, { 0 }, 0, FALSE },
};

static void catMtPlayMission06(void)
{
    GrowSelection *team;
    SelectCommand mothership;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    catMtSpared = kasGrowSelectionPtr(0x0879aa02);    // the heavy cruiser group (see HvCruiser above)
    if (catMtStands("Explore") && ((universe.univUpdateCounter % 64) == 8) && ((team = catMtTeam(0x088689ae)) != NULL))
    {
        catMtScout("Explore", &team->selection->ShipPtr[0]->obj.posinfo.position);
    }
    else if ((catMtStands("Scout") || catMtStands("Escort")) && ((universe.univUpdateCounter % 64) == 8))
    {
        // the nav points Nav1 .. Nav4 (vectors 0x00052671 ..) in turn: the CaalShto FSM sets bit n of its variable
        //      0x000526b3 once a recon is within 5000 of nav point n + 1 (Watch_Mission06_CaalShto)
        sdword navs = catMtVar(0x056179ff, 0x000526b3), n;
        hvector *nav;

        for (n = 0; n < 4; n++)
        {
            if (!(navs & (1 << n)) && ((nav = kasVectorPtr(0x00052671 + n)) != NULL))
            {
                catMtScout(catMtStands("Scout") ? "Scout" : "Escort (the nav points ahead)", (vector *)nav);
                break;
            }
        }
    }
    if (catMtStands("Escort") && ((universe.univUpdateCounter % 512) == 24) && ((team = catMtTeam(0x056179ff)) != NULL) &&
        (universe.players[0].PlayerMothership != NULL))
    {
        mothership.numShips = 1;
        mothership.ShipPtr[0] = universe.players[0].PlayerMothership;
        catMtGuard("Escort", &mothership, team);
    }
    if (catMtStands("Escort") && ((universe.univUpdateCounter % 64) == 24) && ((team = catMtTeam(0x056179ff)) != NULL))
    {
        catMtDamageAround("Escort (her escort)", &team->selection->ShipPtr[0]->obj.posinfo.position, 16000.0f);
    }
    if (catMtStands("Mines") && ((universe.univUpdateCounter % 64) == 40))
    {
        catMtDamage("Mines (the minelayers)", catMtTeam(0x02a03c42));
        catMtDamage("Mines", kasGrowSelectionPtr(0x0051e2a3));
    }
    if (catMtStands("Leech"))
    {
        catMtResearch("Leech", "TECH_PHASED_TELEMETRY");
    }
    if (catMtStands("HvCruiser") && ((universe.univUpdateCounter % 64) == 56))
    {
        if (catMtCreatedTick[catMtObjectiveIndex("Leech")] < 0)
        {
            catMtDamageTo("HvCruiser", catMtSpared, 0.45f);     // the hull below 51%: the Leech scene
        }
        else
        {
            catMtDamage("HvCruiser", catMtSpared);              // then the group is destroyed
        }
    }
}

/*-----------------------------------------------------------------------------
    Mission07 ("Rescue Convoy"): Watch_Mission07_Objectives 0x005fc8d0. The player's part:
      Mayday      a recon flies to the convoy (team Convoy01 0x043b4e91, what is attacking it)
      Protect     the Beast ships attacking the convoy (the script's selection 0x06da2da9, the enemy ships) are
                  destroyed with ShipsDamage once the first convoy ship is infected (the selection
                  FirstInfectedConvoyShip, which the Beast weapon fills: the test lets the first wave strike so that
                  the Destroy objective's flow runs; at the latest 8000 ticks after Protect appears); the objective
                  closes once the waves are over and convoy ships have reached the slipgate
                  (Watch_Mission07_Objectives: 0x01ffc154 and 0x073d25a4)
      Destroy     the infected convoy ships (the script's selection 0x089f03c3) are destroyed with ShipsDamage
      Hyperspace  the hyperspace button (a catSpMissionNextLevel mission: the letterbox jump)
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission07[] =
{
    { "Mayday",     CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Protect",    CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Destroy",    CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Hyperspace", CAT_MT_HYPERSPACE, { 0 }, 0, FALSE },
};

static bool32 catMtProtectStarted = FALSE;

static void catMtPlayMission07(void)
{
    GrowSelection *team;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    if (catMtStands("Mayday") && ((universe.univUpdateCounter % 64) == 8) && ((team = catMtTeam(0x043b4e91)) != NULL))
    {
        catMtScout("Mayday", &team->selection->ShipPtr[0]->obj.posinfo.position);
    }
    if (catMtStands("Protect") && ((universe.univUpdateCounter % 64) == 24))
    {
        GrowSelection *infected = kasGrowSelectionPtr(0x0b7e32d0);    // FirstInfectedConvoyShip

        if (!catMtProtectStarted && (catMtStands("Destroy") || ((infected != NULL) && (infected->selection->numShips > 0)) ||
                                     ((sdword)universe.univUpdateCounter > catMtCreatedTick[1] + 8000)))
        {
            catMtProtectStarted = TRUE;
            dbgMessagef("missiontest: event tick=%u Protect: the defence begins (first infection %s)",
                        (unsigned)universe.univUpdateCounter, catMtStands("Destroy") ? "seen" : "not seen");
        }
        if (catMtProtectStarted)
        {
            catMtDamage("Protect", kasGrowSelectionPtr(0x06da2da9));
        }
    }
    if (catMtStands("Destroy") && ((universe.univUpdateCounter % 64) == 40))
    {
        catMtDamage("Destroy", kasGrowSelectionPtr(0x089f03c3));
    }
}

/*-----------------------------------------------------------------------------
    Mission09 ("Retrieve Info From Taidan Research Planet"): Watch_Mission09_Objectives 0x00606e40. The player's
    part:
      MoveTransports  the transports (the disguised resource collectors the script launches, selection Resourcers
                      0x04a310f3) are moved to the rendezvous the script pings (PingOnResourcers, vector 0x02df4973)
                      through the command layer; the
                      hostile ships and mines within 10000 of each are destroyed with ShipsDamage (their escort)
      DestroySensor   one proximity sensor of the grid (team Sensors 0x07a38423) is destroyed with ShipsDamage
      Wait            nothing: the transports infiltrate on the script's own clock
      Engage          the hostile ships within 12000 of the command ship are destroyed with ShipsDamage (the
                      engagement that covers the insertion)
      Protect         the hostile ships within 10000 of each transport are destroyed with ShipsDamage while they
                      come back to dock; a protect objective the script ends with the mission (held)
      Hyperspace      the hyperspace button; the roll call
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission09[] =
{
    { "MoveTransports", CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "DestroySensor",  CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Wait",           CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Engage",         CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Protect",        CAT_MT_CUSTOM,     { 0 }, 0, TRUE  },
    { "Hyperspace",     CAT_MT_HYPERSPACE, { 0 }, 0, FALSE },
};

static void catMtPlayMission09(void)
{
    GrowSelection *ships, *team, one;
    SelectCommand *selection;
    sdword i;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    if (catMtStands("MoveTransports") && ((universe.univUpdateCounter % 512) == 8))
    {
        ships = kasGrowSelectionPtr(0x04a310f3);
        if ((ships != NULL) && (ships->selection->numShips > 0) && (kasVectorPtr(0x02df4973) != NULL))
        {
            selection = memAlloc(sizeofSelectCommand(ships->selection->numShips), "catMtTransports", 0);
            selection->numShips = 0;
            for (i = 0; i < ships->selection->numShips; i++)
            {
                if (ships->selection->ShipPtr[i]->playerowner == &universe.players[0])
                {
                    selection->ShipPtr[selection->numShips++] = ships->selection->ShipPtr[i];
                }
            }
            catMtMove("MoveTransports", selection, (vector *)kasVectorPtr(0x02df4973));
            memFree(selection);
        }
    }
    if (catMtStands("DestroySensor") && ((universe.univUpdateCounter % 64) == 24) && ((team = catMtTeam(0x07a38423)) != NULL))
    {
        growSelectInit(&one);
        growSelectAddShip(&one, team->selection->ShipPtr[0]);
        catMtDamage("DestroySensor", &one);
        growSelectClose(&one);
    }
    if (catMtStands("Engage") && ((universe.univUpdateCounter % 64) == 40) && (universe.players[0].PlayerMothership != NULL))
    {
        catMtDamageAround("Engage", &universe.players[0].PlayerMothership->obj.posinfo.position, 12000.0f);
    }
    if ((catMtStands("MoveTransports") || catMtStands("Protect")) && ((universe.univUpdateCounter % 64) == 56))
    {
        ships = kasGrowSelectionPtr(0x04a310f3);
        for (i = 0; (ships != NULL) && (i < ships->selection->numShips); i++)
        {
            catMtDamageAround("Protect", &ships->selection->ShipPtr[i]->obj.posinfo.position, 10000.0f);
        }
    }
}

/*-----------------------------------------------------------------------------
    Mission08 ("Rendezvous With Spy"): Watch_Mission08_Objectives 0x00603560. The player's part:
      Resource           two more workers through the construction manager (two pairs for the captures) and the
                         harvest order; the script closes the objective
      Protect            the spy's pursuers (teams SpyChasers 0x07c3ffa3, SpyChasersCorvettes 0x085094d3,
                         SpyChasersDefenders 0x05788ed3) are destroyed with ShipsDamage
      CaptureResourcers  three Taiidan resource collectors (team Resourcers 0x04a310f3) are each damaged below the
                         salvage limit (the mission language's ShipsDamage by the points above 40% of the hull; the text:
                         "the Collectors must be damaged first") and salvaged by two workers (the special action
                         menu's salvage, the workers with the salvage upgrade): towed to the command ship, they become
                         the player's (set complete by the script, ObjectiveSet 1). A capture needs free support units:
                         the player scuttles recons for them (the warp start's fleet uses 219 of its 220)
      CaptureFrigate     one frigate of team Frigates 0x06db6c13 likewise
      NoSurvivors        once both captures are made, the other enemy ships (the script's selection 0x06da2da9) are
                         destroyed with ShipsDamage
      Hyperspace         the hyperspace button; the roll call
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission08[] =
{
    { "Resource",          CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Protect",           CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "CaptureResourcers", CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "CaptureFrigate",    CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "NoSurvivors",       CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Hyperspace",        CAT_MT_HYPERSPACE, { 0 }, 0, FALSE },
};

// a capture: two workers of the player salvage one ship of the enemy's. A slot remembers its target and its pair of
//      workers: while the pair carries out its order the order is not given again (a new one would restart the tow);
//      the target is kept below the salvage limit (ShipsDamage by the points above 40% of its hull: its support ships
//      repair it) until the tow begins (SPECIAL_2_CatSalvageTargeted)
typedef struct
{
    udword target;          // ship number, 0xffffffff: none
    udword workers[2];
} CatMtCaptureSlot;

static bool32 catMtWorkerBusy(udword shipNumber)
{
    Ship *worker = catMtFindPlayerShip(shipNumber);
    CommandToDo *command = (worker != NULL) ? getShipAndItsCommand(&universe.mainCommandLayer, worker) : NULL;

    return (command != NULL) && (command->ordertype.order == COMMAND_SPECIAL);
}

static void catMtCapture(const char *why, CatMtCaptureSlot *slot, Ship *target)
{
    GrowSelection one;
    SelectCommand *workers;
    real32 above;
    Node *node;
    Ship *ship;
    CommandToDo *command;
    sdword k;

    growSelectInit(&one);
    growSelectAddShip(&one, target);
    above = target->targetable.health - 0.4f * target->staticinfo->health.maxhealth;
    if ((above > 0.1f * target->staticinfo->health.maxhealth) &&
        !bitTest(target->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvageTargeted))
    {
        dbgMessagef("missiontest: event tick=%u %s: ShipsDamage %d on %s#%u (below the salvage limit)",
                    (unsigned)universe.univUpdateCounter, why, (int)above, ShipTypeToStr(target->shiptype),
                    (unsigned)target->shipID.shipNumber);
        kasfShipsDamage(&one, (sdword)above);
    }
    if ((slot->target == target->shipID.shipNumber) && (catMtWorkerBusy(slot->workers[0]) || catMtWorkerBusy(slot->workers[1])))
    {
        growSelectClose(&one);
        return;                                     // the pair is on it
    }
    workers = memAlloc(sizeofSelectCommand(2), "catMtCapture", 0);
    workers->numShips = 0;
    for (node = universe.ShipList.head; (node != NULL) && (workers->numShips < 2); node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        command = getShipAndItsCommand(&universe.mainCommandLayer, ship);
        if ((ship->playerowner == &universe.players[0]) && (ship->shiptype == sWorker) && !bitTest(ship->header.flags, SOF_Dead) &&
            cat_shipHasUpgrade(ship, 0x80000) && ((command == NULL) || (command->ordertype.order != COMMAND_SPECIAL)))
        {
            workers->ShipPtr[workers->numShips++] = ship;
        }
    }
    if (workers->numShips == 2)
    {
        slot->target = target->shipID.shipNumber;
        for (k = 0; k < 2; k++)
        {
            slot->workers[k] = workers->ShipPtr[k]->shipID.shipNumber;
        }
        catMtSpecial(why, 0, workers, &one);
    }
    memFree(workers);
    growSelectClose(&one);
}

// the slot's target while it is still the enemy's, else the team's first ship that is the enemy's and in no slot
static Ship *catMtCaptureTarget(udword teamHash, CatMtCaptureSlot *slot, CatMtCaptureSlot *others, sdword numOthers)
{
    GrowSelection *team = catMtTeam(teamHash);
    Ship *ship;
    sdword i, k;

    for (i = 0; (team != NULL) && (i < team->selection->numShips); i++)
    {
        ship = team->selection->ShipPtr[i];
        if ((ship->shipID.shipNumber == slot->target) && (ship->playerowner != &universe.players[0]))
        {
            return ship;
        }
    }
    for (i = 0; (team != NULL) && (i < team->selection->numShips); i++)
    {
        ship = team->selection->ShipPtr[i];
        if (ship->playerowner == &universe.players[0])
        {
            continue;
        }
        for (k = 0; k < numOthers; k++)
        {
            if (others[k].target == ship->shipID.shipNumber)
            {
                break;
            }
        }
        if (k == numOthers)
        {
            return ship;
        }
    }
    return NULL;
}

// a capture needs the player's free support units for the captured type (cat_salvageCheckTarget 0x004f7eb0 drops a
//      target of a type cat_cmHasSupportUnits 0x00459b40 refuses; the warp start's level 8 fleet uses 219 of 220): the
//      player scuttles recons (the command layer's scuttle, the [S] key menu's), the ships already scuttling counted
static void catMtFreeSupport(const char *why, ShipType type)
{
    Player *player = &universe.players[0];
    sdword deficit = (sdword)player->supportUnitsUsed + GetShipStaticInfo(type, player->race)->supportUnits -
                     (sdword)player->supportUnitsMax;
    SelectCommand *recons = memAlloc(sizeofSelectCommand(COMMAND_MAX_SHIPS), "catMtFreeSupport", 0);
    Node *node;
    Ship *ship;

    recons->numShips = 0;
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner == player) && !bitTest(ship->header.flags, SOF_Dead) && (ship->deathtime != 0.0f))
        {
            deficit -= ship->staticinfo->supportUnits;      // scuttling
        }
    }
    for (node = universe.ShipList.head; (node != NULL) && (deficit > 0); node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner == player) && (ship->shiptype == sRecon) && !bitTest(ship->header.flags, SOF_Dead) &&
            (ship->deathtime == 0.0f))
        {
            recons->ShipPtr[recons->numShips++] = ship;
            deficit -= ship->staticinfo->supportUnits;
        }
    }
    if (recons->numShips > 0)
    {
        dbgMessagef("missiontest: event tick=%u %s: scuttle %d sRecon (used %d of %d, %s needs %d)",
                    (unsigned)universe.univUpdateCounter, why, (int)recons->numShips, (int)player->supportUnitsUsed,
                    (int)player->supportUnitsMax, ShipTypeToStr(type), (int)GetShipStaticInfo(type, player->race)->supportUnits);
        clWrapScuttle(&universe.mainCommandLayer, recons);
    }
    memFree(recons);
}

static CatMtCaptureSlot catMtCaptureSlots[4] =
{
    { 0xffffffff, { 0xffffffff, 0xffffffff } }, { 0xffffffff, { 0xffffffff, 0xffffffff } },
    { 0xffffffff, { 0xffffffff, 0xffffffff } }, { 0xffffffff, { 0xffffffff, 0xffffffff } },
};

static void catMtPlayMission08(void)
{
    static sdword workersQueued = 0;
    Ship *target;
    sdword i;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    if ((workersQueued < 2) && ((universe.univUpdateCounter % 128) == 8) && catMtBuild("Resource", sWorker))
    {
        workersQueued++;
    }
    if ((catMtStands("CaptureResourcers") || catMtStands("CaptureFrigate")) && ((universe.univUpdateCounter % 1024) == 520) &&
        (catMtPlayerShipCount(sWorker) < 4))
    {
        catMtBuild("CaptureResourcers (a lost worker)", sWorker);
    }
    if ((catMtStands("CaptureResourcers") || catMtStands("CaptureFrigate")) && ((universe.univUpdateCounter % 64) == 56))
    {
        // the escort of the capture pairs: the hostile ships around each target are destroyed with ShipsDamage; spared
        //      are the resource collectors, the frigate to be captured and the spy (team Spy 0x00005859): the mission
        //      is lost when the spy dies, when fewer than three collectors or no frigate are left to capture
        //      (Watch_Mission08: the messages 0x085d8904, 0x00f32a44, 0x0c115234 to FleetIntel)
        GrowSelection spare;
        GrowSelection *team;
        Ship *ship;
        sdword k;

        growSelectInit(&spare);
        if ((target = catMtCaptureTarget(0x06db6c13, &catMtCaptureSlots[3], catMtCaptureSlots, 3)) != NULL)
        {
            growSelectAddShip(&spare, target);
        }
        if ((team = catMtTeam(0x04a310f3)) != NULL)
        {
            for (k = 0; k < team->selection->numShips; k++)
            {
                growSelectAddShipNoDuplication(&spare, team->selection->ShipPtr[k]);
            }
        }
        if ((team = catMtTeam(0x00005859)) != NULL)
        {
            for (k = 0; k < team->selection->numShips; k++)
            {
                growSelectAddShipNoDuplication(&spare, team->selection->ShipPtr[k]);
            }
        }
        catMtSpared = &spare;
        for (k = 0; k < spare.selection->numShips; k++)
        {
            ship = spare.selection->ShipPtr[k];
            if ((ship->playerowner != &universe.players[0]) && !bitTest(ship->header.flags, SOF_Dead) &&
                (ship->shiptype != sMothership) && ((team == NULL) || !ShipInSelection(team->selection, ship)))
            {
                catMtDamageAround("CaptureResourcers (the escort)", &ship->obj.posinfo.position, 5000.0f);
            }
        }
        catMtSpared = NULL;
        growSelectClose(&spare);
    }
    if ((catMtStands("CaptureResourcers") || catMtStands("CaptureFrigate")) && ((universe.univUpdateCounter % 256) == 104))
    {
        catMtFreeSupport("CaptureResourcers (support units)", catMtStands("CaptureFrigate") ? tStandardFrigate : tResourceCollector);
    }
    if ((catMtStands("CaptureResourcers") || catMtStands("CaptureFrigate")) && ((universe.univUpdateCounter % 512) == 136))
    {
        // the salvage upgrade (cat_salvageSpecialTarget 0x004faad0 ends the order of a worker without upgrade
        //      0x80000): TECH_SALVAGE, then the ship upgrade button for the workers that lack it
        SelectCommand *workers = memAlloc(sizeofSelectCommand(COMMAND_MAX_SHIPS), "catMtPlayMission08", 0);

        if (!catMtHasTech("TECH_SALVAGE"))
        {
            catMtResearch("CaptureResourcers", "TECH_SALVAGE");
        }
        else if (catMtPlayerShips(sWorker, workers, COMMAND_MAX_SHIPS) > 0)
        {
            for (i = 0; i < workers->numShips; i++)
            {
                if (!cat_shipHasUpgrade(workers->ShipPtr[i], 0x80000) && !workers->ShipPtr[i]->catUpgrading)
                {
                    catMtUpgrade("CaptureResourcers", sWorker);
                    break;
                }
            }
        }
        memFree(workers);
    }
    if (((universe.univUpdateCounter % 512) == 264) && !catMtStands("CaptureResourcers") && !catMtStands("CaptureFrigate"))
    {
        catMtHarvest("Resource");
    }
    if (catMtStands("Protect") && ((universe.univUpdateCounter % 64) == 24))
    {
        catMtDamage("Protect", catMtTeam(0x07c3ffa3));
        catMtDamage("Protect", catMtTeam(0x085094d3));
        catMtDamage("Protect", catMtTeam(0x05788ed3));
    }
    if (catMtStands("CaptureResourcers") && (objectiveGet("CaptureResourcers") == 0) && ((universe.univUpdateCounter % 256) == 40))
    {
        for (i = 0; i < 3; i++)
        {
            if ((target = catMtCaptureTarget(0x04a310f3, &catMtCaptureSlots[i], catMtCaptureSlots, 4)) != NULL)
            {
                catMtCapture("CaptureResourcers", &catMtCaptureSlots[i], target);
            }
        }
    }
    if (catMtStands("CaptureFrigate") && (objectiveGet("CaptureFrigate") == 0) && ((universe.univUpdateCounter % 256) == 168) &&
        ((target = catMtCaptureTarget(0x06db6c13, &catMtCaptureSlots[3], catMtCaptureSlots, 4)) != NULL))
    {
        catMtCapture("CaptureFrigate", &catMtCaptureSlots[3], target);
    }
    if (catMtStands("NoSurvivors") && (objectiveGet("CaptureResourcers") != 0) && (objectiveGet("CaptureFrigate") != 0) &&
        ((universe.univUpdateCounter % 64) == 56))
    {
        catMtDamage("NoSurvivors", kasGrowSelectionPtr(0x06da2da9));
    }
}

// each mission's objectives table and scripted player, by mission number (NULL: none yet)
typedef struct
{
    const CatMtObjective *list;
    sdword num;
    void (*play)(void);
} CatMtMissionScript;
#define CAT_MT_SCRIPT(list, play) { list, (sdword)(sizeof(list) / sizeof(list[0])), play }
static const CatMtMissionScript catMtMissions[MISSION_17 + 1] =
{
    { NULL, 0, NULL },
    CAT_MT_SCRIPT(catMtMission01, NULL),
    CAT_MT_SCRIPT(catMtMission02, catMtPlayMission02),
    CAT_MT_SCRIPT(catMtMission03, catMtPlayMission03),
    CAT_MT_SCRIPT(catMtMission04, catMtPlayMission04),
    CAT_MT_SCRIPT(catMtMission05, catMtPlayMission05),
    CAT_MT_SCRIPT(catMtMission06, catMtPlayMission06),
    CAT_MT_SCRIPT(catMtMission07, catMtPlayMission07),
    CAT_MT_SCRIPT(catMtMission08, catMtPlayMission08),
    CAT_MT_SCRIPT(catMtMission09, catMtPlayMission09),
};

/*-----------------------------------------------------------------------------
    M2-R9 missions-02-09: the roll call (the missions whose jump is not catSpMissionNextLevel's letterbox one): the
    screen spHyperspaceButtonPushed puts up waits for the fleet to dock; 64 ticks after the command the test clicks its
    enabled button through the front end's callback (feFunctionExecute: Go Now, or Emergency Hyperspace while one of the
    player's ships is under fire; the other is disabled, RSF_RegionDisabled)
-----------------------------------------------------------------------------*/
static sdword catMtRollCallTick = -1;
static const char *catMtRollCallButton = "none";

static void catMtRollCall(void)
{
    fescreen *screen;
    featom *atom;
    static char *const buttons[2] = { "GoNowHyperspace", "EmergencyHyperspace" };
    sdword b;

    if (catMtRollCallTick < 0)
    {
        catMtRollCallTick = (sdword)universe.univUpdateCounter;
        if (spHyperspaceRollCallScreen != NULL)
        {
            dbgMessagef("missiontest: rollcall tick=%u up (remaining=%d docked=%d)", (unsigned)universe.univUpdateCounter,
                        (int)singlePlayerGameInfo.rollcallShipsRemaining, (int)singlePlayerGameInfo.rollcallShipsDocked);
        }
        else
        {
            dbgMessagef("missiontest: letterbox jump tick=%u (no roll call: catSpMissionNextLevel)", (unsigned)universe.univUpdateCounter);
        }
        return;
    }
    if (((sdword)universe.univUpdateCounter != catMtRollCallTick + 64) || (spHyperspaceRollCallScreen == NULL))
    {
        return;                                 // (a catSpMissionNextLevel mission's letterbox jump has no roll call)
    }
    screen = feScreenFind("HyperspaceRollCall");
    if (screen == NULL)
    {
        return;
    }
    for (b = 0; b < 2; b++)
    {
        atom = feAtomFindInScreen(screen, buttons[b]);
        if ((atom != NULL) && (atom->region != NULL) && !bitTest(((regionhandle)atom->region)->status, RSF_RegionDisabled))
        {
            catMtRollCallButton = buttons[b];
            dbgMessagef("missiontest: event tick=%u rollcall: %s clicked (its callback; remaining=%d docked=%d)",
                        (unsigned)universe.univUpdateCounter, buttons[b], (int)singlePlayerGameInfo.rollcallShipsRemaining,
                        (int)singlePlayerGameInfo.rollcallShipsDocked);
            feFunctionExecute(buttons[b], atom, FALSE);
            return;
        }
    }
}

void catMissionSelfTestObjective(const char *what, const char *label, sdword status)
{
    sdword i;

    if ((catMissionTestTicks <= 0) || (catMtMission < 2) || !catMissionTestInit || catMissionTestDone)
    {
        return;
    }
    dbgMessagef("missiontest: kas objective %s tick=%u %s %d", what, (unsigned)universe.univUpdateCounter, label, (int)status);
    if (strcmp(what, "create") == 0)
    {
        for (i = 0; i < catMtNumObjectives; i++)
        {
            if ((strcmp(catMtObjectives[i].label, label) == 0) && (catMtCreatedTick[i] < 0))
            {
                catMtCreatedTick[i] = (sdword)universe.univUpdateCounter;
                catMtCreatedOrder[catMtCreatedCount++] = i;
                dbgMessagef("missiontest: %s tick=%u created", catMtObjectives[i].label, (unsigned)universe.univUpdateCounter);
                if (catMtObjectives[i].kind == CAT_MT_HYPERSPACE)
                {
                    catMtEndTick = (sdword)universe.univUpdateCounter + 64;     // the end-of-mission save, then the jump
                }
            }
        }
        return;
    }
    if (strcmp(what, "destroyall") == 0)
    {
        // the mission's end (the Hyperspace message's clean-up): a protect / escort objective of the table (held) that
        //      stood to the end is held (e.g. Mission04's Destroy, which the script never closes by name); any other
        //      objective still standing is not reached
        for (i = 0; i < catMtNumObjectives; i++)
        {
            if ((catMtCreatedTick[i] >= 0) && (catMtDoneTick[i] < 0) && catMtObjectives[i].held)
            {
                catMtDoneTick[i] = (sdword)universe.univUpdateCounter;
                catMtDoneOrder[catMtDoneCount++] = i;
                dbgMessagef("missiontest: %s tick=%u completed (held to the mission's end: ObjectivesDestroyAll)",
                            catMtObjectives[i].label, (unsigned)universe.univUpdateCounter);
            }
        }
        return;
    }
    if ((strcmp(what, "destroy") != 0) && !((strcmp(what, "set") == 0) && (status != 0)))
    {
        return;
    }
    for (i = 0; i < catMtNumObjectives; i++)
    {
        if ((strcmp(catMtObjectives[i].label, label) == 0) && (catMtCreatedTick[i] >= 0) && (catMtDoneTick[i] < 0) &&
            (catMtObjectives[i].kind != CAT_MT_HYPERSPACE))
        {
            catMtDoneTick[i] = (sdword)universe.univUpdateCounter;
            catMtDoneOrder[catMtDoneCount++] = i;
            dbgMessagef("missiontest: %s tick=%u completed", catMtObjectives[i].label, (unsigned)universe.univUpdateCounter);
        }
    }
}

static void catMtPlay(void)
{
    sdword i;

    if ((catMtMission >= 2) && (catMtMissions[catMtMission].play != NULL))
    {
        catMtMissions[catMtMission].play();
    }

    for (i = 0; i < catMtNumObjectives; i++)
    {
        bool32 stands = catMtObjectiveStands(i);

        if ((catMtMission == 1) && stands && (catMtCreatedTick[i] < 0))
        {
            catMtCreatedTick[i] = (sdword)universe.univUpdateCounter;
            catMtCreatedOrder[catMtCreatedCount++] = i;
            dbgMessagef("missiontest: %s tick=%u created", catMtObjectives[i].label, (unsigned)universe.univUpdateCounter);
            if (catMtObjectives[i].kind == CAT_MT_HYPERSPACE)
            {
                catMtEndTick = (sdword)universe.univUpdateCounter + 64;     // the end-of-mission save, then the jump
            }
        }
        else if ((catMtMission == 1) && !stands && (catMtCreatedTick[i] >= 0) && (catMtDoneTick[i] < 0) &&
                 (catMtObjectives[i].kind != CAT_MT_HYPERSPACE))
        {
            catMtDoneTick[i] = (sdword)universe.univUpdateCounter;
            catMtDoneOrder[catMtDoneCount++] = i;
            dbgMessagef("missiontest: %s tick=%u completed", catMtObjectives[i].label, (unsigned)universe.univUpdateCounter);
        }
        if (stands && ((universe.univUpdateCounter - catMtCreatedTick[i]) % 64) == 16)
        {
            switch (catMtObjectives[i].kind)
            {
                case CAT_MT_DESTROY:
                    catMtDestroyTeams(i);
                    break;
                case CAT_MT_EXPLORE:
                    catMtExplore(i);
                    break;
                default:
                    break;
            }
        }
    }
    // the end: the player's hyperspace command once the Hyperspace objective has stood a moment
    if ((catMtEndTick >= 0) && ((sdword)universe.univUpdateCounter >= catMtEndTick) && !catMtHyperspaced)
    {
        catMtHyperspaced = TRUE;
        dbgMessagef("missiontest: event tick=%u Hyperspace: the hyperspace command (playerCanHyperspace=%d)",
                    (unsigned)universe.univUpdateCounter, (int)singlePlayerGameInfo.playerCanHyperspace);
        spHyperspaceButtonPushed();
    }
    if ((catMtMission >= 2) && catMtHyperspaced && (singlePlayerGameInfo.hyperspaceState == HYPERSPACE_WAITINGROLLCALL))
    {
        catMtRollCall();
    }
    if (catMtHyperspaced && (catMtHyperspaceTick < 0) && (singlePlayerGameInfo.hyperspaceState != NO_HYPERSPACE))
    {
        catMtHyperspaceTick = (sdword)universe.univUpdateCounter;
        dbgMessagef("missiontest: Hyperspace tick=%u jump begun (hyperspaceState=%d)", (unsigned)universe.univUpdateCounter,
                    (int)singlePlayerGameInfo.hyperspaceState);
    }
    if (!catMtArrived && (cat_kasVarFind(0, cat_kasHashString("G_ShipsHaveArrived")) != NULL))
    {
        catMtArrived = TRUE;
        dbgMessagef("missiontest: fleet arrived tick=%u (G_ShipsHaveArrived)", (unsigned)universe.univUpdateCounter);
    }
    if (!catMtFailed && (universe.players[0].playerState == PLAYER_DEAD))
    {
        catMtFailed = TRUE;
        dbgMessagef("missiontest: FAIL tick=%u the player lost the mission", (unsigned)universe.univUpdateCounter);
    }
}

/*-----------------------------------------------------------------------------
    The tutorial's first prompts (/catMissionTestTutorial): Tutorial1's KAS script starts (tutInitTutorial), its
    Tutorial FSM reaches Lesson1 (state hash 0x00a883a1) and creates the lesson's two objectives (L1Dummy1, L1Dummy2),
    the SpeechEvents FSM puts up the tutorial's Next, previous and quit buttons (Init_Tutorial1_SpeechEvents:
    TutShowNextButton, TutShowPrevButton, TutShowQuitButton) and plays the lesson's first three prompts (subtitle cards
    on screen, each a new text: "LESSON 01" (SubtitleSimulate), then the narration of speech events 0x80012d3 and
    0x80012d4, the third with the objective list's pointer "ptr", cat_tutSetPointerTargetObj; this test keeps speech on,
    on SDL's dummy audio driver). M2-R8 singleplayer-tutorial: then the Next button is clicked through its own region's
    click callback (tutProcessNextButton with CM_ButtonClick, no input) and the Tutorial FSM must move to Lesson2
    (0x00a883a2: Watch_Tutorial1_Tutorial counts the lesson up on TutNextButtonClicked)
-----------------------------------------------------------------------------*/
#define CAT_MT_TUT_PROMPTS 3
static sdword catMtTutLessonTick = -1, catMtTutObjTick = -1, catMtTutButtonsTick = -1, catMtTutLesson2Tick = -1;
static sdword catMtTutPromptTick[CAT_MT_TUT_PROMPTS] = { -1, -1, -1 };
static sdword catMtTutPrompts = 0;
static char catMtTutLastPrompt[96] = "";
static sdword catMtTutClickTick = -1;
static bool32 catMtTutObjPointer = FALSE;
extern sdword tutNextVisible, tutPrevVisible, catTutQuitVisible;    // Tutor.c
extern buttonhandle tutNextRegion;

static void catMtTutorialWatch(void)
{
    sdword i, r, found = 0;
    AITeam *team = (universe.players[1].aiPlayer != NULL) ? kasAITeamPtr(0x0a9468cc) : NULL;    // team "Tutorial"

    if ((catMtTutLessonTick < 0) && (team != NULL) && (team->kasStateName == 0x00a883a1))
    {
        catMtTutLessonTick = (sdword)universe.univUpdateCounter;
        dbgMessagef("missiontest: tutorial Lesson1 tick=%u", (unsigned)universe.univUpdateCounter);
    }
    if ((catMtTutLesson2Tick < 0) && (catMtTutClickTick >= 0) && (team != NULL) && (team->kasStateName == 0x00a883a2))
    {
        catMtTutLesson2Tick = (sdword)universe.univUpdateCounter;
        dbgMessagef("missiontest: tutorial Lesson2 tick=%u (after the Next button)", (unsigned)universe.univUpdateCounter);
    }
    for (i = 0; i < objectivesUsed; i++)
    {
        if ((strcmp(objectives[i]->label, "L1Dummy1") == 0) || (strcmp(objectives[i]->label, "L1Dummy2") == 0))
        {
            found++;
        }
    }
    if ((catMtTutObjTick < 0) && (found == 2))
    {
        catMtTutObjTick = (sdword)universe.univUpdateCounter;
        dbgMessagef("missiontest: tutorial objectives tick=%u L1Dummy1 L1Dummy2", (unsigned)universe.univUpdateCounter);
    }
    if ((catMtTutButtonsTick < 0) && tutNextVisible && tutPrevVisible && catTutQuitVisible)
    {
        catMtTutButtonsTick = (sdword)universe.univUpdateCounter;
        dbgMessagef("missiontest: tutorial buttons tick=%u next prev quit", (unsigned)universe.univUpdateCounter);
    }
    if ((catMtTutPrompts < CAT_MT_TUT_PROMPTS) && subAnyCardsOnScreen())
    {
        for (r = 0; r < SUB_NumberRegions; r++)
        {
            if ((subRegion[r].cardIndex > 0) && (subRegion[r].card != NULL) && (subRegion[r].card[0].text != NULL) &&
                (strncmp(catMtTutLastPrompt, subRegion[r].card[0].text, sizeof(catMtTutLastPrompt) - 1) != 0))
            {
                strncpy(catMtTutLastPrompt, subRegion[r].card[0].text, sizeof(catMtTutLastPrompt) - 1);
                catMtTutPromptTick[catMtTutPrompts] = (sdword)universe.univUpdateCounter;
                catMtTutPrompts++;
                dbgMessagef("missiontest: tutorial prompt %d tick=%u region=%d text='%.80s'", (int)catMtTutPrompts,
                            (unsigned)universe.univUpdateCounter, (int)r, subRegion[r].card[0].text);
                break;
            }
        }
    }
    // the third prompt points at the objective list (cat_tutSetPointerTargetObj "ptr")
    if (!catMtTutObjPointer && (catMtTutPrompts >= 2) && tutPointerIsSet("ptr"))
    {
        catMtTutObjPointer = TRUE;
        dbgMessagef("missiontest: tutorial objective pointer tick=%u", (unsigned)universe.univUpdateCounter);
    }
    // then the Next button is clicked through its region's own callback
    if ((catMtTutClickTick < 0) && (catMtTutPrompts >= CAT_MT_TUT_PROMPTS) && catMtTutObjPointer && (tutNextRegion != NULL))
    {
        catMtTutClickTick = (sdword)universe.univUpdateCounter;
        dbgMessagef("missiontest: event tick=%u tutorial: the Next button clicked (its click callback)",
                    (unsigned)universe.univUpdateCounter);
        tutNextRegion->processFunction((regionhandle)tutNextRegion, 0, CM_ButtonClick, 0);
    }
}

// M2-R9 missions-02-09: the judgement of a mission after the first: its level loaded by the warp start, its KAS
//      script started, the fleet arrived, every objective of its table created and completed in order (the last, the
//      jump, by the hyperspace command), no mission loss, the jump begun and the next level loaded
static void catMtMissionEnd(void)
{
    sdword i, reached = 0;
    bool32 inOrder = TRUE, pass;

    // in order: the Objectives FSM created them in the table's order (the mission's order) and each was completed
    //      after it was created; the completions may come in another order (Mission02's raiders are destroyed before
    //      the pod they came for is salvaged)
    for (i = 0; i < catMtCreatedCount; i++)
    {
        if (catMtCreatedOrder[i] != i)
        {
            inOrder = FALSE;
        }
    }
    for (i = 0; i < catMtNumObjectives; i++)
    {
        if ((catMtDoneTick[i] >= 0) && (catMtDoneTick[i] < catMtCreatedTick[i]))
        {
            inOrder = FALSE;
        }
    }
    for (i = 0; i < catMtNumObjectives; i++)
    {
        if ((catMtCreatedTick[i] >= 0) &&
            ((catMtDoneTick[i] >= 0) || ((catMtObjectives[i].kind == CAT_MT_HYPERSPACE) && catMtHyperspaced)))
        {
            reached++;
        }
    }
    pass = (catMtNumObjectives > 0) && (reached == catMtNumObjectives) && inOrder && !catMtFailed && catMtLevelLoaded &&
           catMtKasStarted && catMtArrived && (catMtHyperspaceTick >= 0) && (catMtNextLevelTick >= 0);
    dbgMessagef("missiontest: end mission=%d tick=%u level=%d kas=%d arrived=%d inOrder=%d jump=%d rollcall=%s next=%d failed=%d",
                (int)catMtMission, (unsigned)universe.univUpdateCounter, (int)catMtLevelLoaded, (int)catMtKasStarted,
                (int)catMtArrived, (int)inOrder, (int)catMtHyperspaceTick, catMtRollCallButton, (int)catMtNextLevelTick,
                (int)catMtFailed);
    // ticks: the mission level's ticks and the next level's (the run's ticks)
    dbgMessagef("missiontest: done mission=%d ticks=%u objectives=%d/%d result=%s", (int)catMtMission,
                (unsigned)((catMtNextLevelTick >= 0) ? catMtLevelTicks + universe.univUpdateCounter : universe.univUpdateCounter),
                (int)reached, (int)catMtNumObjectives,
                pass ? "PASS" : "FAIL");
    oracleMissionStopAfterThisTick();
}

void catMissionSelfTestPlay(void)
{
    if (catMissionTestInit && !catMissionTestDone)
    {
        catMtPlay();
    }
}

void catMissionSelfTestTick(void)
{
    sdword limit = (catMissionTestTicks > 0) ? catMissionTestTicks : catMissionTestTutorialTicks;

    if (catMissionTestDone)
    {
        return;
    }
    if (catMtNextLevelTick < 0)
    {
        catMtLevelTicks = universe.univUpdateCounter;
    }
    if (!catMissionTestInit)
    {
        sdword i;
        catMissionTestInit = TRUE;
        if ((catMissionTestTicks > 0) && (catMtMission >= 2))
        {
            catMtObjectives = catMtMissions[catMtMission].list;
            catMtNumObjectives = catMtMissions[catMtMission].num;
            dbgMessagef("missiontest: start mission=%d (the warp start, Warp.script's level %d record) objectives=%d",
                        (int)catMtMission, (int)catMtMission, (int)catMtNumObjectives);
        }
        for (i = 0; i < CAT_MT_MAX_OBJECTIVES; i++)
        {
            catMtCreatedTick[i] = catMtDoneTick[i] = -1;
        }
    }
    catMtWatchFsms();
    catMtWatchObjectives();
    if (catMissionTestTutorialTicks > 0)
    {
        catMtTutorialWatch();
        if ((catMtTutLesson2Tick >= 0) && (catMtTutObjTick >= 0) && (catMtTutLessonTick >= 0) && (catMtTutButtonsTick >= 0))
        {
            catMissionTestDone = TRUE;
            dbgMessagef("missiontest: done ticks=%u tutorial kas=%d lesson1=%d objectives=%d buttons=%d prompts=%d/%d/%d "
                        "pointer=%d next=%d lesson2=%d result=PASS", (unsigned)universe.univUpdateCounter,
                        (int)catMtKasStarted, (int)catMtTutLessonTick, (int)catMtTutObjTick, (int)catMtTutButtonsTick,
                        (int)catMtTutPromptTick[0], (int)catMtTutPromptTick[1], (int)catMtTutPromptTick[2],
                        (int)catMtTutObjPointer, (int)catMtTutClickTick, (int)catMtTutLesson2Tick);
            oracleMissionStopAfterThisTick();
            return;
        }
    }

    if ((universe.univUpdateCounter % 256) == 0)
    {
        dbgMessagef("missiontest: status tick=%u mission=%d hyperspaceState=%d ships=%u nis=%s", (unsigned)universe.univUpdateCounter,
                    (int)singlePlayerGameInfo.currentMission, (int)singlePlayerGameInfo.hyperspaceState,
                    (unsigned)universe.ShipList.num, (thisNisPlaying != NULL) ? "playing" : "none");
    }
    if (((sdword)universe.univUpdateCounter >= limit - 2) ||
        ((catMissionTestTicks > 0) && (catMtMission == 1) && (catMtHyperspaceTick >= 0) &&
         ((sdword)universe.univUpdateCounter >= catMtHyperspaceTick + 32)) ||
        ((catMissionTestTicks > 0) && (catMtMission >= 2) && (catMtNextLevelTick >= 0) &&
         ((sdword)universe.univUpdateCounter >= catMtNextLevelTick + 32)))
    {
        sdword i, reached = 0;
        bool32 inOrder = TRUE;

        catMissionTestDone = TRUE;
        if ((catMissionTestTicks > 0) && (catMtMission >= 2))
        {
            catMtMissionEnd();
        }
        else if (catMissionTestTicks > 0)
        {
            for (i = 0; i < catMtCreatedCount; i++)
            {
                if (catMtCreatedOrder[i] != i)
                {
                    inOrder = FALSE;
                }
            }
            for (i = 0; i < catMtDoneCount; i++)
            {
                if (catMtDoneOrder[i] != i)
                {
                    inOrder = FALSE;
                }
            }
            for (i = 0; i < catMtNumObjectives; i++)
            {
                if ((catMtCreatedTick[i] >= 0) &&
                    ((catMtDoneTick[i] >= 0) || (catMtObjectives[i].kind == CAT_MT_HYPERSPACE && catMtHyperspaced)))
                {
                    reached++;
                }
            }
            dbgMessagef("missiontest: end tick=%u nis=%d/%d level=%d kas=%d nislet=%d arrived=%d counter=%d inOrder=%d jump=%d failed=%d",
                        (unsigned)universe.univUpdateCounter, (int)catMtNisStarted, (int)catMtNisEnded, (int)catMtLevelLoaded,
                        (int)catMtKasStarted, (int)catMtNisletEnded, (int)catMtArrived, (int)catMtCounter, (int)inOrder,
                        (int)(catMtHyperspaceTick >= 0), (int)catMtFailed);
            dbgMessagef("missiontest: done mission=1 ticks=%u objectives=%d/%d result=%s", (unsigned)universe.univUpdateCounter,
                        (int)reached, (int)catMtNumObjectives,
                        ((reached == catMtNumObjectives) && inOrder && !catMtFailed && (catMtCounter == 6) &&
                         catMtNisStarted && catMtNisEnded && catMtLevelLoaded && catMtKasStarted && catMtNisletEnded &&
                         catMtArrived && (catMtHyperspaceTick >= 0)) ? "PASS" : "FAIL");
            oracleMissionStopAfterThisTick();   // the mission run's own quit, with its stream complete
        }
        else
        {
            dbgMessagef("missiontest: done ticks=%u tutorial kas=%d lesson1=%d objectives=%d buttons=%d prompts=%d "
                        "pointer=%d next=%d lesson2=%d result=FAIL", (unsigned)universe.univUpdateCounter,
                        (int)catMtKasStarted, (int)catMtTutLessonTick, (int)catMtTutObjTick, (int)catMtTutButtonsTick,
                        (int)catMtTutPrompts, (int)catMtTutObjPointer, (int)catMtTutClickTick, (int)catMtTutLesson2Tick);
        }
    }
}

/*-----------------------------------------------------------------------------
    HWI-TEST (M2-R8 singleplayer-tutorial): the single-player save load (/catLoadGameTest <ticks>, /catLoadGameName
    <name>, harness mode loadgame). Once the start-up has put up the main screen, SavedGames/SinglePlayer/<name> (a save
    written by cataclysm.exe: the mode-A loader converts its ILP32 chunks, SaveCat32.c) is loaded as the front end's
    Load Game does (gpLoadGivenGame: VerifySaveFile, gameEnd, utyLoadSinglePlayerGameGivenFilename), and the universe
    runs <ticks> ticks on the mission run's virtual clock (the oracle stream written with /oracleDump). Lines:
    "loadgametest: loaded ...", "loadgametest: status ..." every 256 ticks, "loadgametest: done ... result=PASS|FAIL".
    PASS: the load verified and finished as a single-player game in the save's mission (Mission01 for "01 - Hiigara",
    the KAS script's own save at the mission's start), with the player's mothership, the KAS mission running (its watch
    function restored and its script teams' FSMs seen: "missiontest: fsm" lines) and the ticks run with the player
    alive; the objectives loaded and present at the end are reported.
-----------------------------------------------------------------------------*/
static sdword catLoadGameTicks = 0;
static char catLoadGameName[128] = "";
static bool32 catLoadGameLoaded = FALSE, catLoadGameDone = FALSE;
static sdword catLoadGameStartTick = -1;
static sdword catLoadGameObjectives = 0;

void catLoadGameSelfTestSet(char *string)
{
    catLoadGameTicks = (sdword)strtol(string, NULL, 10);
    if (catLoadGameTicks <= 0)
    {
        catLoadGameTicks = 400;
    }
    oracleSetMission("2000000000");             // the virtual clock; the run stops itself (oracleMissionStopAfterThisTick)
}

void catLoadGameSelfTestNameSet(char *string)
{
    strncpy(catLoadGameName, string, sizeof(catLoadGameName) - 1);
    catLoadGameName[sizeof(catLoadGameName) - 1] = '\0';
}

bool32 catLoadGameSelfTestRequested(void)
{
    return (catLoadGameTicks > 0);
}

void catLoadGameSelfTestStart(void)
{
    char filename[256];
    sdword verify;
    Node *node;
    sdword ships = 0;

    snprintf(filename, sizeof(filename), "%s%s", SinglePlayerSavedGamesPath, catLoadGameName);
    verify = fileExists(filename, FF_UserSettingsPath) ? VerifySaveFile(filename) : -1;
    dbgMessagef("loadgametest: load '%s' verify=%d", filename, (int)verify);
    if (verify != VERIFYSAVEFILE_OK)
    {
        catLoadGameDone = TRUE;
        dbgMessagef("loadgametest: done ticks=0 result=FAIL (the save did not verify)");
        utyGameQuit(NULL, NULL);
        return;
    }
    gpLoadSinglePlayerGame = TRUE;
    gpLoadTutorial = FALSE;
    gpLoadGivenGame(filename);
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        if (((Ship *)listGetStructOfNode(node))->playerowner == &universe.players[0])
        {
            ships++;
        }
    }
    catLoadGameObjectives = objectivesUsed;
    catLoadGameLoaded = singlePlayerGame && (universe.ShipList.num > 0);
    dbgMessagef("loadgametest: loaded singlePlayer=%d mission=%d tick=%u ships=%u player0ships=%d objectives=%d kas='%s' "
                "watch=%d level='%s' hyperspaceState=%d difficulty=%u", (int)singlePlayerGame,
                (int)singlePlayerGameInfo.currentMission, (unsigned)universe.univUpdateCounter,
                (unsigned)universe.ShipList.num, (int)ships, (int)objectivesUsed, CurrentMissionName,
                (int)(CurrentMissionWatchFunction != NULL), CurrentLevelName, (int)singlePlayerGameInfo.hyperspaceState,
                (unsigned)catSpDifficultySetting);
}

void catLoadGameSelfTestTick(void)
{
    if (catLoadGameDone || !catLoadGameLoaded)
    {
        return;
    }
    if (catLoadGameStartTick < 0)
    {
        catLoadGameStartTick = (sdword)universe.univUpdateCounter;
    }
    catMtWatchFsms();
    catMtWatchObjectives();
    if (((sdword)universe.univUpdateCounter - catLoadGameStartTick) % 256 == 0)
    {
        dbgMessagef("loadgametest: status tick=%u ships=%u objectives=%d hyperspaceState=%d", (unsigned)universe.univUpdateCounter,
                    (unsigned)universe.ShipList.num, (int)objectivesUsed, (int)singlePlayerGameInfo.hyperspaceState);
    }
    if ((sdword)universe.univUpdateCounter - catLoadGameStartTick >= catLoadGameTicks)
    {
        bool32 pass = singlePlayerGame && (singlePlayerGameInfo.currentMission == MISSION_1_KHARAK_SYSTEM) &&
                      (universe.players[0].playerState != PLAYER_DEAD) && (universe.players[0].PlayerMothership != NULL) &&
                      (CurrentMissionWatchFunction != NULL) && (catMtTeamsSeen > 0);

        catLoadGameDone = TRUE;
        dbgMessagef("loadgametest: done ticks=%d from=%d to=%u mission=%d ships=%u objectives=%d/%d kasTeams=%d result=%s",
                    (int)catLoadGameTicks, (int)catLoadGameStartTick, (unsigned)universe.univUpdateCounter,
                    (int)singlePlayerGameInfo.currentMission, (unsigned)universe.ShipList.num,
                    (int)catLoadGameObjectives, (int)objectivesUsed, (int)catMtTeamsSeen, pass ? "PASS" : "FAIL");
        oracleMissionStopAfterThisTick();
    }
}
