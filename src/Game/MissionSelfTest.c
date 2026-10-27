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
#include "BeastWeapon.h"                 // HWI-TEST (M3-R4 infect-convert)
#include "SpaceObj.h"
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
#include "mainswitches.h"
#include "Select.h"
#include "Worker.h"
#include "Mimic.h"
#include "Alliance.h"
#include "Salvage.h"
#include "ShipLink.h"
#include "CatMissions.h"
#include "ResearchGUI.h"
#include "CameraCommand.h"
#include "Camera.h"
#include "Sensors.h"
#include "PopupMenu.h"
#include "KeyBindings.h"     // M2-R9 missions-10-17: Watch_Tutorial1 (the tutorial save's load)
#include "Mothership.h"
#include "Volume.h"
#include "NomadMoon.h"
#include "TradeMgr.h"
#include "TeamColourTest.h"   // HWI-TEST (M3-R1 team-colours-warp): the team-colour checks and the longer run
#include "Attributes.h"       // HWI-TEST (M3-R3 mission02): CAT_ATTRIBUTES_KillerCollision (the meteors)
#ifdef _WIN32
#undef near     // HWI-FIX(win64): <windows.h> (through SDL) defines near as an empty macro; the self-tests use it as a name
#endif
// M2-R10 tests-leftovers: the KAS dispatcher by name (KASFuncTable.h; not included: its CatKasFunction meets
//      SinglePlayer.h's), the developer console's KAS call that the failed-jump test takes
sdword cat_kasCallFunctionByName(char *name, void **args);     // 0x0045b9a0

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
// HWI-TEST (M3-R3 mission02): the tick the mission's KAS script started (the "kas start missionNN" flow event)
static sdword catMtKasStartTick = -1;
// HWI-TEST (M3-R1 team-colours-warp): with /catTeamColourTest the run goes on past its jump to the next MISSION level
//      (Mission01: Mission02; mission N: Mission N+1 after its opening NIS's staging level, if it has one) and ends 32
//      ticks (/catTeamColourTicks N: N) after that level has loaded, so the team colours are checked after a jump and
//      after an NIS level
static sdword catMtTcNextTick = -1;
static udword catMtLevelTicks = 0;      // the mission level's last tick (the next level starts its counter at 0)
// HWI-TEST (M2-R9 missions-10-17): the end of the game (Mission17: kasfMissionCompleted, then univUpdate's quit into
//      the credits, utyGameQuitToMain 0x005ab6b0)
static sdword catMtCompletedTick = -1, catMtGameEndTick = -1;
static void catMtGameEnd(const char *credits);
// HWI-TEST (M2-R9 missions-10-17): the warp start under a test (/catWarpStartTest): the mission comes from the test
//      copy of warp.script (singlePlayerInit), the record is checked twice (SinglePlayer.c cat_spWarpStartSelfTestCheck:
//      the fleet in the ether, then out of it after the arrival); no mission is played
static bool32 catMtWarpTest = FALSE;
static sdword catMtWarpScriptLevel = -1, catMtWarpApplied = -1, catMtWarpArrived = -1, catMtWarpArrivedTick = -1;

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

static bool32 catMtFailJump = FALSE;       // M2-R10 tests-leftovers: /catMissionTestFailJump (catMtFailJumpWatch below)

void catMissionSelfTestFailJumpSet(void)
{
    catMtFailJump = TRUE;
}

void catWarpStartSelfTestSet(void)
{
    catMtWarpTest = TRUE;
}

bool32 catWarpStartSelfTestRequested(void)
{
    return catMtWarpTest && (catMissionTestTicks > 0);
}

void catWarpStartSelfTestScript(bool32 enabled, sdword level)
{
    if (!catWarpStartSelfTestRequested())
    {
        return;
    }
    catMtWarpScriptLevel = enabled ? level : 0;
    dbgMessagef("warpstarttest: warp.script WarpEnabled=%d LevelToWarpTo=%d", (int)enabled, (int)level);
    if (enabled && (level >= 2) && (level <= MISSION_17))
    {
        catMtMission = level;           // the flow lines of mission <level> (its level load, its KAS start)
    }
}

void catWarpStartSelfTestApplied(bool32 pass)
{
    if (catWarpStartSelfTestRequested())
    {
        catMtWarpApplied = pass ? 1 : 0;
    }
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
        if (catTeamColourTest && (catMissionTestTicks > 0) && (catMtTcNextTick < 0) && (strncmp(what, "level loaded", 12) == 0))
        {                                                   // HWI-TEST (M3-R1 team-colours-warp)
            char next[48];

            snprintf(next, sizeof(next), "Mission%02d.mission", (int)catMtMission + 1);
            if (strstr(what, next) != NULL)
            {
                catMtTcNextTick = (sdword)universe.univUpdateCounter;
                dbgMessagef("missiontest: teamcolour next mission level tick=%u %s", (unsigned)universe.univUpdateCounter,
                            what + 13);
            }
        }
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
                if (!catMtKasStarted)
                {
                    catMtKasStartTick = (sdword)universe.univUpdateCounter;     // HWI-TEST (M3-R3 mission02)
                }
                catMtKasStarted = TRUE;
            }
            // M2-R9 missions-10-17: the mission's own end (MissionCompleted) and the credits after it
            if ((strcmp(what, "mission completed") == 0) && (catMtCompletedTick < 0))
            {
                catMtCompletedTick = (sdword)universe.univUpdateCounter;
            }
            if (strncmp(what, "credits ", 8) == 0)
            {
                catMtGameEnd(what + 8);
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

// HWI-TEST (M2-R9 missions-10-17): the KAS message trace (/catMissionTestTrace, a diagnostic for developing a mission's
//      player: which message the scripts wait for); a team's re-queue of a message it did not handle (aitMsgSend of a
//      message to itself that it just removed) is logged once
static bool32 catMtTrace = FALSE;
static udword catMtTraceLast[8][2];
static sdword catMtTraceNext = 0;

void catMissionSelfTestTraceSet(void)
{
    catMtTrace = TRUE;
}

void catMissionSelfTestMessage(struct AITeam *from, struct AITeam *to, udword msg)
{
    sdword k;

    if (!catMtTrace || (to == NULL))
    {
        return;
    }
    for (k = 0; k < 8; k++)
    {
        if ((catMtTraceLast[k][0] == to->kasLabel) && (catMtTraceLast[k][1] == msg))
        {
            return;
        }
    }
    catMtTraceLast[catMtTraceNext][0] = to->kasLabel;
    catMtTraceLast[catMtTraceNext][1] = msg;
    catMtTraceNext = (catMtTraceNext + 1) % 8;
    dbgMessagef("missiontest: msg tick=%u %07x -> %07x %07x", (unsigned)universe.univUpdateCounter,
                (from != NULL) ? (unsigned)from->kasLabel : 0u, (unsigned)to->kasLabel, (unsigned)msg);
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
static sdword catMtRollCallTick = -1;                   // the roll call (catMtRollCall below)
static const char *catMtRollCallButton = "none";


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
/*-----------------------------------------------------------------------------
    HWI-TEST (M3-R3 mission02): the BUSHAN-RE hint (the user's play-test item 2), cataclysm.exe's Mission02 script
    (src/Missions/SinglePlayer/Mission02.c, generated from the exe and verified against its machine code):
      Init_Mission02 0x005e05a0: kasfMissionSkillGet 0x00493b90 (catSpDifficultySetting 0x008941a0, the single-player
          difficulty 0 VERY EASY .. 4 VERY HARD) through the jump table 0x005e0b9c: skill 0 (0x005e084d) starts the
          timer 0x0004ce34 at 120 s, skill 1 (0x005e08b1) at 180 s, skill 2 (0x005e08f5) at 360 s; skill 3
          (0x005e0929) and 4 (ja 0x005e093c past the table) create no timer
      Watch_Mission02 0x005e0bb0 (0x005e1102 .. 0x005e1155): once (the global 0x443ccb8), while the global 0x849928e4
          is 0 (Watch_Mission02_Destroyer_Find 0x005df130 sets it when HasShipBeenDiscovered is 1) and the timer has
          expired (TimerExpiredDestroy 0x004920a0; a timer that does not exist never expires): message 0x0004ce34 to
          FleetIntel (0x0a8a880c)
      Watch_Mission02_FleetIntel_WaitMessage 0x005dbac0: 0x0004ce34: WideScreenInDontCloseSensors, a 3 s wait, message
          0x004ce352 to itself; 0x004ce352: the other scripts and the universe paused, the sensors manager opened,
          PingHint1 on the BUSHAN-RE and PingHint2 / PingHint3 at random points within 20000 of it, the sensors' look-at
          on her, speech event 0x10002169
    So the hint is a timer (no direction or distance test): 2, 3 or 6 minutes of game time after the mission's script
    starts on VERY EASY, EASY or NORMAL, if no ship of the player's has found her by then; never on HARD or VERY HARD.
    /catMissionTestHoldScout <tick>: the FindDestroyer recon waits until <tick> (a path on which the timer runs out).
    The judgement (catMtHintJudge): the speech fires if and only if the skill has a timer and the BUSHAN-RE was not
    found by the timer's end, and not before that end. "speech: event=.. tick=.." lines (catMissionSelfTestSpeech, from
    kasfSpeechEvent: the mission test runs with speech off, so the call is logged, not the sound)
-----------------------------------------------------------------------------*/
#define CAT_MT_HINT_SPEECH  0x10002169
#define CAT_MT_FOUND_VAR    0x049928e4      // the global 0x849928e4 (the high bit: the mission scope)
static sdword catMtHoldScoutTick = -1;      // /catMissionTestHoldScout <tick>
static sdword catMtHintSpeechTick = -1;     // the first speech event 0x10002169 of Mission02's level
static sdword catMtDestroyerFoundTick = -1; // the first tick the global 0x849928e4 is set
static sdword catMtHintDueTick = -1;        // the first tick the hint timer's time has run out
static sdword catMtSpeechEvents = 0;

void catMissionSelfTestHoldScoutSet(char *string)
{
    catMtHoldScoutTick = (sdword)strtol(string, NULL, 10);
}

// kasfSpeechEvent's call (KASFunc.c): every speech event of a mission test run, logged with its tick
void catMissionSelfTestSpeech(sdword event)
{
    if ((catMissionTestTicks <= 0) || catMissionTestDone)
    {
        return;
    }
    catMtSpeechEvents++;
    dbgMessagef("speech: event=0x%08x tick=%u (kasfSpeechEvent)", (unsigned)event, (unsigned)universe.univUpdateCounter);
    if ((catMtMission == 2) && (event == CAT_MT_HINT_SPEECH) && (catMtHintSpeechTick < 0) && (catMtNextLevelTick < 0))
    {
        catMtHintSpeechTick = (sdword)universe.univUpdateCounter;
        dbgMessagef("missiontest: hint speech tick=%u (FleetIntel 0x004ce352: sensors open on the BUSHAN-RE, PingHint1-3)",
                    (unsigned)universe.univUpdateCounter);
    }
}

// the hint's seconds for the difficulty (Init_Mission02 0x005e05a0's jump table 0x005e0b9c), -1: no timer
static sdword catMtHintSeconds(void)
{
    switch (catSpDifficultySetting)
    {
        case 0: return 120;
        case 1: return 180;
        case 2: return 360;
        default: return -1;
    }
}

// every universe tick of Mission02's level: the found flag and the timer's end, observed (nothing is changed)
static void catMtHintWatch(void)
{
    static real32 startTime = -1.0f;
    sdword seconds = catMtHintSeconds();

    if (!catMtKasStarted || (catMtNextLevelTick >= 0))
    {
        return;
    }
    if (startTime < 0.0f)
    {
        startTime = universe.totaltimeelapsed;
    }
    if ((catMtDestroyerFoundTick < 0) && (catMtVar(0, CAT_MT_FOUND_VAR) != 0))
    {
        catMtDestroyerFoundTick = (sdword)universe.univUpdateCounter;
        dbgMessagef("missiontest: hint watch tick=%u the BUSHAN-RE found (the global 0x849928e4 set)",
                    (unsigned)universe.univUpdateCounter);
    }
    if ((seconds > 0) && (catMtHintDueTick < 0) && (universe.totaltimeelapsed - startTime >= (real32)seconds))
    {
        catMtHintDueTick = (sdword)universe.univUpdateCounter;
        dbgMessagef("missiontest: hint watch tick=%u the hint timer's %d s have run out (skill %u)",
                    (unsigned)universe.univUpdateCounter, (int)seconds, (unsigned)catSpDifficultySetting);
    }
}

// the judgement at the mission's end (catMtMissionEnd)
static bool32 catMtHintJudge(void)
{
    sdword seconds = catMtHintSeconds();
    bool32 expected, fired, pass;

    expected = (seconds > 0) && (catMtHintDueTick >= 0) &&
               ((catMtDestroyerFoundTick < 0) || (catMtDestroyerFoundTick > catMtHintDueTick));
    fired = (catMtHintSpeechTick >= 0);
    pass = (expected == fired) && (!fired || (catMtHintSpeechTick >= catMtHintDueTick));
    dbgMessagef("missiontest: hint skill=%u timer=%d kasstart=%d due=%d found=%d hold=%d speech=%d expected=%s fired=%s"
                " speechevents=%d result=%s", (unsigned)catSpDifficultySetting, (int)seconds, (int)catMtKasStartTick,
                (int)catMtHintDueTick, (int)catMtDestroyerFoundTick, (int)catMtHoldScoutTick, (int)catMtHintSpeechTick,
                expected ? "yes" : "no", fired ? "yes" : "no", (int)catMtSpeechEvents, pass ? "PASS" : "FAIL");
    return pass;
}

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

    catMtHintWatch();                                       // HWI-TEST (M3-R3 mission02)
    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    // HWI-TEST (M3-R3 mission02): /catMissionTestHoldScout <tick>: the recon waits until <tick>
    if (catMtStands("FindDestroyer") && ((team = catMtTeam(0x0893b502)) != NULL) && ((universe.univUpdateCounter % 64) == 8) &&
        ((sdword)universe.univUpdateCounter >= catMtHoldScoutTick))
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
    sdword k, needed;
    bool32 busy0, busy1;

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
    busy0 = (slot->target == target->shipID.shipNumber) && catMtWorkerBusy(slot->workers[0]);
    busy1 = (slot->target == target->shipID.shipNumber) && catMtWorkerBusy(slot->workers[1]);
    if (busy0 && busy1)
    {
        Ship *w0 = catMtFindPlayerShip(slot->workers[0]);
        Ship *w1 = catMtFindPlayerShip(slot->workers[1]);
        vector d;

        if (w0 != NULL)
        {
            vecSub(d, w0->obj.posinfo.position, target->obj.posinfo.position);
        }
        dbgMessagef("missiontest: event tick=%u %s: the pair is on %s#%u (health %d of %d, targeted %d, worker 0 at %d, "
                    "states %d/%d, player %d, depot %s at %d, speed %d)",
                    (unsigned)universe.univUpdateCounter, why, ShipTypeToStr(target->shiptype),
                    (unsigned)target->shipID.shipNumber, (int)target->targetable.health,
                    (int)target->staticinfo->health.maxhealth,
                    bitTest(target->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvageTargeted) ? 1 : 0,
                    (w0 != NULL) ? (int)fsqrt(vecMagnitudeSquared(d)) : -1,
                    (w0 != NULL) ? (int)cat_salvageGetSalvagerInfo(w0)->state : -1,
                    (w1 != NULL) ? (int)cat_salvageGetSalvagerInfo(w1)->state : -1, (int)target->catSalvagePlayer,
                    ((w0 != NULL) && (cat_salvageGetSalvagerInfo(w0)->dockwith != NULL)) ?
                        ShipTypeToStr(cat_salvageGetSalvagerInfo(w0)->dockwith->shiptype) : "none",
                    ((w0 != NULL) && (cat_salvageGetSalvagerInfo(w0)->dockwith != NULL)) ?
                        (int)cat_vecDistance(&target->obj.posinfo.position, &cat_salvageGetSalvagerInfo(w0)->dockwith->obj.posinfo.position) : -1,
                    (int)fsqrt(vecMagnitudeSquared(target->obj.posinfo.velocity)));
        growSelectClose(&one);
        return;                                     // the pair is on it
    }
    // one of the pair lost (Mission14's escort fights): a free worker joins the one still on it
    needed = (busy0 || busy1) ? 1 : 2;
    workers = memAlloc(sizeofSelectCommand(2), "catMtCapture", 0);
    workers->numShips = 0;
    for (node = universe.ShipList.head; (node != NULL) && (workers->numShips < needed); node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        command = getShipAndItsCommand(&universe.mainCommandLayer, ship);
        if ((ship->playerowner == &universe.players[0]) && (ship->shiptype == sWorker) && !bitTest(ship->header.flags, SOF_Dead) &&
            cat_shipHasUpgrade(ship, 0x80000) && ((command == NULL) || (command->ordertype.order != COMMAND_SPECIAL)))
        {
            workers->ShipPtr[workers->numShips++] = ship;
        }
    }
    if ((workers->numShips == 2) && (needed == 2))
    {
        slot->target = target->shipID.shipNumber;
        for (k = 0; k < 2; k++)
        {
            slot->workers[k] = workers->ShipPtr[k]->shipID.shipNumber;
        }
        catMtSpecial(why, 0, workers, &one);
    }
    else if ((workers->numShips == 1) && (needed == 1))
    {
        slot->workers[busy0 ? 1 : 0] = workers->ShipPtr[0]->shipID.shipNumber;
        catMtSpecial(why, 0, workers, &one);
    }
    else if ((needed == 1) && ((universe.univUpdateCounter % 1024) == 296))
    {
        catMtBuild(why, sWorker);                   // no free worker: one more from the command ship
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
    // the recons first; when they are not enough (Mission14's fleet arrives over its cap), the small craft
    static const ShipType order[] = { sRecon, sAcolyte, sACV, sSentinel, sLeech, sMimic };
    sdword k;

    for (k = 0; (k < (sdword)(sizeof(order) / sizeof(order[0]))) && (deficit > 0); k++)
    {
        for (node = universe.ShipList.head; (node != NULL) && (deficit > 0) && (recons->numShips < COMMAND_MAX_SHIPS);
             node = node->next)
        {
            ship = (Ship *)listGetStructOfNode(node);
            if ((ship->playerowner == player) && (ship->shiptype == order[k]) && !bitTest(ship->header.flags, SOF_Dead) &&
                (ship->deathtime == 0.0f))
            {
                recons->ShipPtr[recons->numShips++] = ship;
                deficit -= ship->staticinfo->supportUnits;
            }
        }
    }
    if (recons->numShips > 0)
    {
        dbgMessagef("missiontest: event tick=%u %s: scuttle %d small craft (used %d of %d, %s needs %d)",
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

/*-----------------------------------------------------------------------------
    M2-R9 missions-10-17: the missions after the ninth. Their tables (the order the Objectives FSMs create the
    objectives; docs/mission-code-map.md 8) and players follow (the same toolbox as 02 .. 09)
-----------------------------------------------------------------------------*/
// the escort's fight in a ring around a ship (the hostile ships between inner and outer from it, but the spared ones):
//      a ship destroyed close to the escorted one would damage it with its blast (MakeExplosionRockBumpables)
static void catMtDamageRing(const char *why, vector *centre, real32 inner, real32 outer)
{
    GrowSelection hostile;
    Node *node;
    Ship *ship;
    vector diff;
    real32 d2;

    growSelectInit(&hostile);
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (bitTest(ship->header.flags, SOF_Dead) || !bitTest(ship->header.flags, SOF_Targetable) ||
            allianceIsShipAlly(ship, &universe.players[0]) ||
            ((catMtSpared != NULL) && (catMtSpared->selection != NULL) && ShipInSelection(catMtSpared->selection, ship)))
        {
            continue;
        }
        vecSub(diff, ship->obj.posinfo.position, *centre);
        d2 = vecMagnitudeSquared(diff);
        if ((d2 >= inner * inner) && (d2 < outer * outer))
        {
            growSelectAddShip(&hostile, ship);
        }
    }
    catMtDamage(why, &hostile);
    growSelectClose(&hostile);
}

// the player's mothership as a selection of one (NULL when it is gone)
static bool32 catMtMothership(SelectCommand *one)
{
    Ship *mothership = universe.players[0].PlayerMothership;

    one->numShips = 0;
    if ((mothership == NULL) || bitTest(mothership->header.flags, SOF_Dead))
    {
        return FALSE;
    }
    one->numShips = 1;
    one->ShipPtr[0] = mothership;
    return TRUE;
}

// the defence of the command ship the test does not fly: every hostile ship within the radius is destroyed with
//      ShipsDamage (the player's fleet would fight them off)
static void catMtDefendMothership(const char *why, real32 radius)
{
    if (universe.players[0].PlayerMothership != NULL)
    {
        catMtDamageAround(why, &universe.players[0].PlayerMothership->obj.posinfo.position, radius);
    }
}

// the big gun's order (the Fire big gun button's: cat_uiFireSiegeCannon 0x00566340, cat_bigGunFireCommand 0x005bc0c0,
//      then its point mode's choice, cat_bigGunFirePoint 0x005bbce0): the command layer's point order of the mothership
//      at the target's position, when the gun may fire, is charged and is not busy with the last order
static void catMtFireBigGun(const char *why, vector *target)
{
    SelectCommand one;

    if (!catMtMothership(&one) || !catSpBigGunAllowed || !cat_mothershipHasBigGunModule(one.ShipPtr[0]) ||
        !cat_bigGunIsCharged(one.ShipPtr[0]) || cat_mothershipBigGunActive(one.ShipPtr[0]))
    {
        return;
    }
    dbgMessagef("missiontest: event tick=%u %s: the big gun fires at (%.0f, %.0f, %.0f) (its point order)",
                (unsigned)universe.univUpdateCounter, why, target->x, target->y, target->z);
    cat_clWrapPointOrder(&universe.mainCommandLayer, &one, target);
}

/*-----------------------------------------------------------------------------
    Mission10: Watch_Mission10_Objectives 0x0060f680 creates the objectives on FleetIntel's messages. The player's
    part:
      SALVAGE_BIGGUN   every worker's special action (salvage) on the derelict big gun (team BigGun 0x046dbc9e,
                       dBigGunUnit) until it is towed in (Watch_Mission10_BigGun: the team empty and
                       GetShipCountSalvaged("dBigGunUnit")); the pirate salvage teams and their escort within 8000 of
                       it are destroyed with ShipsDamage (the gun itself spared)
      RESEARCH_BIGGUN  TECH_BIG_GUN through the research manager (the script allows it)
      BUILD_BIGGUN     the big gun module (sMothershipBigGun) through the construction manager
      FIRE_BIG_GUN     the big gun fires at the Beast mothership (team 0x0a2e1ef0; its point order) until a blast
                       has hit it (Watch_Mission10_BeastMothership_*: the team's attribute CAT_ATTRIBUTES_BlastHit)
      HYPERSPACE       the hyperspace button
    Throughout, the hostile ships within 10000 of the command ship are destroyed with ShipsDamage (its defence).
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission10[] =
{
    { "SALVAGE_BIGGUN",  CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "RESEARCH_BIGGUN", CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "BUILD_BIGGUN",    CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "FIRE_BIG_GUN",    CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "HYPERSPACE",      CAT_MT_HYPERSPACE, { 0 }, 0, FALSE },
};

static void catMtPlayMission10(void)
{
    static bool32 gunQueued = FALSE;
    GrowSelection *team;
    SelectCommand *workers;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    catMtSpared = kasAITeamShipsPtr(0x046dbc9e);            // the big gun is salvaged, not destroyed
    workers = memAlloc(sizeofSelectCommand(COMMAND_MAX_SHIPS), "catMtWorkers", 0);
    if (catMtStands("SALVAGE_BIGGUN") && ((universe.univUpdateCounter % 256) == 8) && ((team = catMtTeam(0x046dbc9e)) != NULL) &&
        (catMtPlayerShipsOnSpecial(sWorker) == 0) && (catMtPlayerShips(sWorker, workers, COMMAND_MAX_SHIPS) > 0))
    {
        catMtSpecial("SALVAGE_BIGGUN", 0, workers, team);
    }
    memFree(workers);
    if (catMtStands("SALVAGE_BIGGUN") && ((universe.univUpdateCounter % 64) == 24) && ((team = catMtTeam(0x046dbc9e)) != NULL))
    {
        catMtDamageAround("SALVAGE_BIGGUN (the pirates)", &team->selection->ShipPtr[0]->obj.posinfo.position, 8000.0f);
    }
    if (catMtStands("RESEARCH_BIGGUN"))
    {
        catMtResearch("RESEARCH_BIGGUN", "TECH_BIG_GUN");
    }
    if (catMtStands("BUILD_BIGGUN") && !gunQueued && (catMtPlayerShipCount(sMothershipBigGun) == 0))
    {
        gunQueued = catMtBuild("BUILD_BIGGUN", sMothershipBigGun);
    }
    if (catMtStands("FIRE_BIG_GUN") && ((universe.univUpdateCounter % 64) == 40) && ((team = catMtTeam(0x0a2e1ef0)) != NULL))
    {
        catMtFireBigGun("FIRE_BIG_GUN", &team->selection->ShipPtr[0]->obj.posinfo.position);
    }
    if ((universe.univUpdateCounter % 64) == 56)
    {
        catMtDefendMothership("the command ship's defence", 10000.0f);
    }
}

/*-----------------------------------------------------------------------------
    Mission11 ("The Factorem Attacks"): Watch_Mission11_Objectives_Attacking 0x00614250, _S_xa2dd597 0x006143c0,
    _BeastAttacking 0x006144c0. The player's part:
      DESTROY_ATTACKERS  the attackers (the script's selection 0x0a636424) are destroyed with ShipsDamage
      REPAIR_FACTORUM    every worker's special action (repair) on the FAALCORUM (team Factorum 0x0589433d) until
                         its health averages above 64% (Watch_Mission11_Factorum_Friends)
      DESTROY_BEAST      the enemy ships (selection 0x06da2da9) are destroyed with ShipsDamage; the script then ends
      PROTECT_FACTORUM   both with ObjectivesDestroyAll (held)
      Hyperspace         the hyperspace button
    Throughout, the hostile ships within 10000 of the command ship are destroyed with ShipsDamage (its defence).
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission11[] =
{
    { "DESTROY_ATTACKERS", CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "REPAIR_FACTORUM",   CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "DESTROY_BEAST",     CAT_MT_CUSTOM,     { 0 }, 0, TRUE  },
    { "PROTECT_FACTORUM",  CAT_MT_CUSTOM,     { 0 }, 0, TRUE  },
    { "Hyperspace",        CAT_MT_HYPERSPACE, { 0 }, 0, FALSE },
};

static void catMtPlayMission11(void)
{
    GrowSelection *team;
    SelectCommand *workers;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    if (catMtStands("DESTROY_ATTACKERS") && ((universe.univUpdateCounter % 64) == 24))
    {
        catMtDamage("DESTROY_ATTACKERS", kasGrowSelectionPtr(0x0a636424));
    }
    workers = memAlloc(sizeofSelectCommand(COMMAND_MAX_SHIPS), "catMtWorkers", 0);
    if (catMtStands("REPAIR_FACTORUM") && ((universe.univUpdateCounter % 256) == 8) && ((team = catMtTeam(0x0589433d)) != NULL) &&
        (catMtPlayerShipsOnSpecial(sWorker) == 0) && (catMtPlayerShips(sWorker, workers, COMMAND_MAX_SHIPS) > 0))
    {
        catMtSpecial("REPAIR_FACTORUM", 1, workers, team);
    }
    memFree(workers);
    if (catMtStands("DESTROY_BEAST") && ((universe.univUpdateCounter % 64) == 40))
    {
        catMtDamage("DESTROY_BEAST", kasGrowSelectionPtr(0x06da2da9));
    }
    if ((universe.univUpdateCounter % 64) == 56)
    {
        catMtDefendMothership("the command ship's defence", 10000.0f);
    }
}

/*-----------------------------------------------------------------------------
    Mission12 ("Negotiations With Pirates And Help From Contras"): Watch_Mission12_Objectives 0x00621370 creates the
    objectives on FleetIntel's messages. The player's part:
      DefendMothership  the hostile ships within 12000 of the command ship are destroyed with ShipsDamage (its
                        defence; the outpost spared)
      AttackOutpost     ShipsDamage leaves each ship of the pirate outpost (team 0x0886896e) at 20% of the hull: below
                        26% (Watch_Mission12_PirateOutpost_UnderAttack: TeamHealthAverage < 0x1a) the outpost gives in,
                        FleetIntel's scene sends 0x076a7002 and both objectives close; the outpost becomes the player's
      Hyperspace        the hyperspace button; the roll call
    WorkerDock / WorkerReturn are not in the table: WorkerDock's message 0x008d9fd3 comes from FleetIntel's 0x04f3cd02,
    which no script body sends (grep of the generated scripts), and WorkerReturn's 0x0d00b093 from the SecurityTeam FSM
    (team 0x0be7033d), whose one layout ship (resourcesphere_*.missphere: "Ships SecurityTeam,..,RACE_SECT,sWorker,0,..",
    a ship of player 0) the level does not create when the fleet arrives: the team stays empty ("missiontest: fsm ..
    team=be7033d .. ships=0"), as Mission06's Kill.
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission12[] =
{
    { "DefendMothership", CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "AttackOutpost",    CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Hyperspace",       CAT_MT_HYPERSPACE, { 0 }, 0, FALSE },
};

static void catMtPlayMission12(void)
{
    GrowSelection *team;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    if (catMtStands("AttackOutpost") && ((universe.univUpdateCounter % 64) == 24) && ((team = catMtTeam(0x0886896e)) != NULL))
    {
        catMtDamageTo("AttackOutpost", team, 0.20f);
    }
    if ((universe.univUpdateCounter % 64) == 56)
    {
        catMtSpared = kasAITeamShipsPtr(0x0886896e);    // the outpost is captured, not destroyed
        catMtDefendMothership("DefendMothership (the command ship's defence)", 12000.0f);
    }
}

/*-----------------------------------------------------------------------------
    Mission13 ("Knocked From Hyperspace"): Watch_Mission13_Objectives 0x00625250.
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission13[] =
{
    { "DESTROY_ENEMY",     CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "PROTECT_FAALCORUM", CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Hyperspace",        CAT_MT_HYPERSPACE, { 0 }, 0, FALSE },
};

static void catMtPlayMission13(void)
{
    static const udword enemies[4] = { 0x06da2da9, 0x0789fc45, 0x042ccf25, 0x0e69ee65 };
    sdword i;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    // the four enemy selections Watch_Mission13 counts (both objectives close when all are empty): destroyed once
    //      the second objective stands (the FAALCORUM's escort the text asks for)
    if (catMtStands("DESTROY_ENEMY") && catMtStands("PROTECT_FAALCORUM") && ((universe.univUpdateCounter % 64) == 24))
    {
        for (i = 0; i < 4; i++)
        {
            catMtDamage("DESTROY_ENEMY", kasGrowSelectionPtr(enemies[i]));
        }
    }
}

/*-----------------------------------------------------------------------------
    Mission14: Watch_Mission14_Objectives 0x0062bf30.
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission14[] =
{
    { "PROTECT_MOTHERSHIP", CAT_MT_CUSTOM,     { 0 }, 0, TRUE  },
    { "GET_SAMPLE",         CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "CAPTURE_TAIIDAN",    CAT_MT_CUSTOM,     { 0 }, 0, TRUE  },
    { "Hyperspace",         CAT_MT_HYPERSPACE, { 0 }, 0, FALSE },
};

static CatMtCaptureSlot catMtCaptureSlot14 = { 0xffffffff, { 0xffffffff, 0xffffffff } };

static void catMtPlayMission14(void)
{
    GrowSelection *team;
    SelectCommand *ships;
    Ship *target = NULL;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    // GET_SAMPLE (Watch_Mission14_Naggarok_PreDock 0x0062c3f0): a mimic of the player, seen as the player's (the
    //      script's selection 0x004a2a29 of the enemy ships in the volume), comes within 40000 of the NAGGAROK; then,
    //      out of the enemies' view, it imitates an enemy ship and, disguised, flies to the NAGGAROK, where it is taken
    //      in (the DockMimic team) and launched with the sample
    ships = memAlloc(sizeofSelectCommand(COMMAND_MAX_SHIPS), "catMtShips", 0);
    if (catMtStands("GET_SAMPLE") && ((universe.univUpdateCounter % 64) == 8) && ((team = catMtTeam(0x05bb641b)) != NULL) &&
        (catMtPlayerShips(sMimic, ships, 1) > 0))
    {
        vector diff, close;
        real32 dist;

        vecSub(diff, ships->ShipPtr[0]->obj.posinfo.position, team->selection->ShipPtr[0]->obj.posinfo.position);
        dist = fsqrt(vecMagnitudeSquared(diff));
        // enemies in view of the mimic (the text: none in visible range when the mimic command is given)
        Node *node;
        sdword near = 0;
        vector d;

        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            Ship *other = (Ship *)listGetStructOfNode(node);

            if (!bitTest(other->header.flags, SOF_Dead) && (other->playerowner != &universe.players[0]) &&
                !allianceIsShipAlly(other, &universe.players[0]))
            {
                vecSub(d, other->obj.posinfo.position, ships->ShipPtr[0]->obj.posinfo.position);
                if (vecMagnitudeSquared(d) < 12000.0f * 12000.0f)
                {
                    near++;
                }
            }
        }
        dbgMessagef("missiontest: event tick=%u GET_SAMPLE: the mimic #%u is %.0f from the NAGGAROK (mimicking %d, "
                    "detected %d, enemies within 12000 %d)", (unsigned)universe.univUpdateCounter,
                    (unsigned)ships->ShipPtr[0]->shipID.shipNumber, dist,
                    (int)((MimicSpec *)ships->ShipPtr[0]->ShipSpecifics)->started,
                    (int)((MimicSpec *)ships->ShipPtr[0]->ShipSpecifics)->detected[1], (int)near);
        if (((MimicSpec *)ships->ShipPtr[0]->ShipSpecifics)->started)
        {
            catMtMove("GET_SAMPLE (disguised)", ships, &team->selection->ShipPtr[0]->obj.posinfo.position);
        }
        else if (catMtVar(0x05bb641b, 0x095c1df5) == 0)
        {
            // first a mimic seen as the player's within 40000 (the PreDock's variable 0x095c1df5, its message
            //      0x07f232f5 to FleetIntel)
            vecMultiplyByScalar(diff, 30000.0f / dist);
            vecAdd(close, team->selection->ShipPtr[0]->obj.posinfo.position, diff);
            catMtMove("GET_SAMPLE (within 40000)", ships, &close);
        }
        else if (near > 0)
        {
            // then out of the enemies' view before the disguise: back towards the command ship
            if (universe.players[0].PlayerMothership != NULL)
            {
                catMtMove("GET_SAMPLE (out of the enemies' view)", ships,
                          &universe.players[0].PlayerMothership->obj.posinfo.position);
            }
        }
        else
        {
            catMtMimicEnemy("GET_SAMPLE", ships);
        }
    }
    // the detectors that see through the disguise (ships with catTypeFlags 0x10000 and upgrade 0x800000 whose fog-of-war
    //      radius reaches the mimic: cat_shipStealthDetectionUpdate 0x0053fd90 sets its detected flag, and a detected
    //      mimic stays in the script's enemy selection 0x004a2a29, so the NAGGAROK never takes it in) are destroyed with
    //      ShipsDamage once it is disguised
    if (catMtStands("GET_SAMPLE") && ((universe.univUpdateCounter % 64) == 40) && (catMtPlayerShips(sMimic, ships, 1) > 0) &&
        ((MimicSpec *)ships->ShipPtr[0]->ShipSpecifics)->started)
    {
        GrowSelection detectors;
        Node *node;
        Ship *other;
        vector d;
        real32 r;

        growSelectInit(&detectors);
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            other = (Ship *)listGetStructOfNode(node);
            if (bitTest(other->header.flags, SOF_Dead) || allianceIsShipAlly(other, &universe.players[0]) ||
                !bitTest(other->staticinfo->catTypeFlags, 0x10000) || !cat_shipHasUpgrade(other, 0x800000))
            {
                continue;
            }
            r = cat_shipGetFOWRadius(other, 0) + 2000.0f;
            vecSub(d, other->obj.posinfo.position, ships->ShipPtr[0]->obj.posinfo.position);
            if (vecMagnitudeSquared(d) < r * r)
            {
                growSelectAddShip(&detectors, other);
            }
        }
        catMtDamage("GET_SAMPLE (the detectors near the disguised mimic)", &detectors);
        growSelectClose(&detectors);
    }
    // the mimic's escort on its way, while it is not disguised (the enemy patrols destroy it otherwise; disguised, it
    //      flies unescorted, as the text asks): the hostile ships 3000 .. 9000 from it (nearer, their blast would hit
    //      it), but the NAGGAROK and team 0x0d7d6102, which Watch_Mission14_Naggarok_PreDock needs alive
    if (catMtStands("GET_SAMPLE") && ((universe.univUpdateCounter % 64) == 24) && (catMtPlayerShips(sMimic, ships, 1) > 0) &&
        !((MimicSpec *)ships->ShipPtr[0]->ShipSpecifics)->started)
    {
        GrowSelection spare;
        sdword k;

        growSelectInit(&spare);
        if ((team = catMtTeam(0x05bb641b)) != NULL)
        {
            growSelectAddShip(&spare, team->selection->ShipPtr[0]);
        }
        if ((team = catMtTeam(0x0d7d6102)) != NULL)
        {
            for (k = 0; k < team->selection->numShips; k++)
            {
                growSelectAddShip(&spare, team->selection->ShipPtr[k]);
            }
        }
        catMtSpared = &spare;
        catMtDamageRing("GET_SAMPLE (the mimic's escort)", &ships->ShipPtr[0]->obj.posinfo.position, 3000.0f, 9000.0f);
        catMtSpared = NULL;
        growSelectClose(&spare);
    }
    memFree(ships);
    if (catMtStands("CAPTURE_TAIIDAN"))
    {
        target = catMtCaptureTarget(0x089b3a01, &catMtCaptureSlot14, NULL, 0);
        if (target == NULL)
        {
            target = catMtCaptureTarget(0x089b3a02, &catMtCaptureSlot14, NULL, 0);
        }
    }
    if ((target != NULL) && ((universe.univUpdateCounter % 256) == 104))
    {
        catMtFreeSupport("CAPTURE_TAIIDAN (support units)", target->shiptype);
    }
    if ((target != NULL) && ((universe.univUpdateCounter % 256) == 40))
    {
        catMtCapture("CAPTURE_TAIIDAN", &catMtCaptureSlot14, target);
    }
    if ((target != NULL) && ((universe.univUpdateCounter % 64) == 40))
    {
        GrowSelection spare;

        growSelectInit(&spare);
        growSelectAddShip(&spare, target);
        catMtSpared = &spare;
        catMtDamageAround("CAPTURE_TAIIDAN (the escort)", &target->obj.posinfo.position, 6000.0f);
        catMtSpared = NULL;
        growSelectClose(&spare);
    }
    if ((universe.univUpdateCounter % 64) == 56)
    {
        // the defence spares the Naggarok (the sample's host) and the two capture teams: the workers tow the
        //      frigate being captured to the command ship, and all of them destroyed is the mission's loss
        //      (Watch_Mission14_Objectives' 0x085dd80e count, 0x0062c314)
        static const udword spareTeams[3] = { 0x05bb641b, 0x089b3a01, 0x089b3a02 };
        GrowSelection spare;
        sdword t, k;

        growSelectInit(&spare);
        for (t = 0; t < 3; t++)
        {
            if ((team = catMtTeam(spareTeams[t])) != NULL)
            {
                for (k = 0; k < team->selection->numShips; k++)
                {
                    growSelectAddShip(&spare, team->selection->ShipPtr[k]);
                }
            }
        }
        catMtSpared = &spare;
        catMtDefendMothership("PROTECT_MOTHERSHIP (the command ship's defence)", 12000.0f);
        catMtSpared = NULL;
        growSelectClose(&spare);
    }
}

/*-----------------------------------------------------------------------------
    Mission15 ("Convince Bentusi To Help"): Watch_Mission15_Objectives 0x00638740 creates the objectives on FleetIntel's
    messages. The player's part:
      DestroyGateNode  the slipgate's nodes (team GateNode 0x068a3735) are destroyed with ShipsDamage, one every 1024
                       ticks (the Bentusi stations leave through the gate meanwhile: all of them gone is the mission's
                       loss, Watch_Mission15's 0x034dbee4)
      Block            the command ship moves into the Block volume 0x0047137b in front of the slipgate
      Hyperspace       the hyperspace button (a catSpMissionNextLevel mission: the letterbox jump)
    Throughout, the hostile ships within 12000 of the command ship are destroyed with ShipsDamage (its defence).
    DefendMothership is not in the table: Watch_Mission15 creates it (0x068ad1c0) when ShipsUnderAttackElsewhere finds
    Bentusi dAcolytes attacking the ships of selection 0x004a2a39, which no script body fills (the player's ships are
    0x004a2a29, one digit off: grep of the generated scripts, the exe's push 0x4a2a39 in Watch_Mission15): unreachable
    in the exe's script too, as Mission06's Kill.
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission15[] =
{
    { "DestroyGateNode",  CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Block",            CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "Hyperspace",       CAT_MT_HYPERSPACE, { 0 }, 0, FALSE },
};

// the centre of a mission volume (a sphere's centre, a box's middle); FALSE when the volume does not exist
static bool32 catMtVolumeCentre(udword volumeHash, vector *centre)
{
    Volume *volume = kasVolumePtr(volumeHash);

    if (volume == NULL)
    {
        return FALSE;
    }
    if (volume->type == VOLUME_SPHERE)
    {
        *centre = volume->attribs.sphere.center;
    }
    else
    {
        centre->x = (volume->attribs.aaBox.x0 + volume->attribs.aaBox.x1) * 0.5f;
        centre->y = (volume->attribs.aaBox.y0 + volume->attribs.aaBox.y1) * 0.5f;
        centre->z = (volume->attribs.aaBox.z0 + volume->attribs.aaBox.z1) * 0.5f;
    }
    return TRUE;
}

static void catMtPlayMission15(void)
{
    GrowSelection *team;
    SelectCommand one;
    vector centre;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    if (catMtStands("Block") && ((universe.univUpdateCounter % 512) == 8) && catMtVolumeCentre(0x0047137b, &centre) &&
        catMtMothership(&one))
    {
        catMtMove("Block", &one, &centre);
    }
    // the gate nodes go one at a time (every 1024 ticks): Watch_Mission15_GateNode_Peace counts its team down and tells
    //      FleetIntel (0x0471a632), whose answer creates Block; the last node collapses the slipgate (0x07137fd4: Block
    //      closes; FleetIntel's 0x08970534: DestroyGateNode closes)
    if (catMtStands("DestroyGateNode") && ((universe.univUpdateCounter % 1024) == 24) && ((team = catMtTeam(0x068a3735)) != NULL))
    {
        GrowSelection node;

        growSelectInit(&node);
        growSelectAddShip(&node, team->selection->ShipPtr[0]);
        catMtDamage("DestroyGateNode (one node)", &node);
        growSelectClose(&node);
    }
    if ((universe.univUpdateCounter % 64) == 56)
    {
        catMtSpared = kasAITeamShipsPtr(0x068a3735);    // the gate nodes (their own pace)
        catMtDefendMothership("the command ship's defence", 12000.0f);
        catMtSpared = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Mission16: Watch_Mission16_Objectives_Wave1 0x0063bb00, _Wave2 0x0063bd50.
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission16[] =
{
    { "LOCATE_CS",     CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "CRIPPLE_CS",    CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "DOCKWITH_CS",   CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "HARVEST",       CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "DESTROY_BEAST", CAT_MT_CUSTOM,     { 0 }, 0, FALSE },
    { "HYPERSPACE",    CAT_MT_HYPERSPACE, { 0 }, 0, FALSE },
};

static void catMtPlayMission16(void)
{
    GrowSelection *team;
    SelectCommand one, *workers;
    vector diff, away;
    real32 dist;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    if (catMtStands("LOCATE_CS") && !catMtStands("CRIPPLE_CS") && ((universe.univUpdateCounter % 64) == 8) &&
        ((team = catMtTeam(0x0809a71e)) != NULL))
    {
        catMtScout("LOCATE_CS", &team->selection->ShipPtr[0]->obj.posinfo.position);
    }
    // CRIPPLE_CS: the siege cannon's shot at the CLEE-SAN from beyond 35000 (Watch_Mission16_Cleesan_Normal: a blast
    //      hit with the command ship nearer than 0x88b9 scuttles her); nearer, the command ship first moves away
    if (catMtStands("CRIPPLE_CS") && ((universe.univUpdateCounter % 64) == 24) && ((team = catMtTeam(0x0809a71e)) != NULL) &&
        catMtMothership(&one))
    {
        vecSub(diff, one.ShipPtr[0]->obj.posinfo.position, team->selection->ShipPtr[0]->obj.posinfo.position);
        dist = fsqrt(vecMagnitudeSquared(diff));
        if (dist < 40000.0f)
        {
            if (((universe.univUpdateCounter % 512) == 24) && (dist > 1.0f))
            {
                vecMultiplyByScalar(diff, 46000.0f / dist);
                vecAdd(away, team->selection->ShipPtr[0]->obj.posinfo.position, diff);
                catMtMove("CRIPPLE_CS (out of the blast's reach)", &one, &away);
            }
        }
        else
        {
            catMtFireBigGun("CRIPPLE_CS", &team->selection->ShipPtr[0]->obj.posinfo.position);
        }
    }
    workers = memAlloc(sizeofSelectCommand(COMMAND_MAX_SHIPS), "catMtWorkers", 0);
    if (catMtStands("DOCKWITH_CS") && ((universe.univUpdateCounter % 256) == 40) && ((team = catMtTeam(0x0809a71e)) != NULL) &&
        (catMtPlayerShips(sWorker, workers, 1) > 0))
    {
        vecSub(diff, workers->ShipPtr[0]->obj.posinfo.position, team->selection->ShipPtr[0]->obj.posinfo.position);
        dbgMessagef("missiontest: event tick=%u DOCKWITH_CS: the worker is %.0f from the CLEE-SAN (%s)",
                    (unsigned)universe.univUpdateCounter, fsqrt(vecMagnitudeSquared(diff)),
                    ShipTypeToStr(team->selection->ShipPtr[0]->shiptype));
        catMtMove("DOCKWITH_CS", workers, &team->selection->ShipPtr[0]->obj.posinfo.position);
    }
    if (catMtStands("DOCKWITH_CS") && ((universe.univUpdateCounter % 64) == 8) && (catMtPlayerShips(sWorker, workers, 1) > 0))
    {
        catMtSpared = kasAITeamShipsPtr(0x0809a71e);    // the CLEE-SAN
        catMtDamageAround("DOCKWITH_CS (the worker's escort)", &workers->ShipPtr[0]->obj.posinfo.position, 6000.0f);
        catMtSpared = NULL;
    }
    memFree(workers);
    if ((catMtStands("HARVEST") || catMtStands("DESTROY_BEAST")) && ((universe.univUpdateCounter % 512) == 264))
    {
        catMtHarvest("HARVEST");
    }
    if (catMtStands("DESTROY_BEAST") && ((universe.univUpdateCounter % 64) == 40))
    {
        catMtDamage("DESTROY_BEAST", kasGrowSelectionPtr(0x0593fe0c));
    }
    if ((universe.univUpdateCounter % 64) == 56)
    {
        catMtSpared = kasAITeamShipsPtr(0x0809a71e);    // the CLEE-SAN
        catMtDefendMothership("the command ship's defence", 10000.0f);
        catMtSpared = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Mission17: Watch_Mission17_Objectives 0x0064a160; the end of the game.
-----------------------------------------------------------------------------*/
static const CatMtObjective catMtMission17[] =
{
    { "DestroyNaggarok",  CAT_MT_CUSTOM, { 0 }, 0, FALSE },
    { "DestroyNomadMoon", CAT_MT_CUSTOM, { 0 }, 0, FALSE },
    { "DestroyEmitters",  CAT_MT_CUSTOM, { 0 }, 0, FALSE },
    { "Recon",            CAT_MT_CUSTOM, { 0 }, 0, FALSE },
};

static void catMtPlayMission17(void)
{
    GrowSelection *team, emitters, spare;
    SelectCommand *ships;
    vector centre;
    NomadMoonSpec *moon;
    sdword k;

    if ((universe.univUpdateCounter % 16) != 8)
    {
        return;
    }
    // the NOMAD MOON is found first (Watch_Mission17: HasShipBeenDiscovered, 0x0471a633: DestroyNomadMoon, then the
    //      emitters' objective); a recon flies to it
    if (!catMtStands("DestroyNomadMoon") && !catMtStands("DestroyEmitters") &&
        (catMtCreatedTick[catMtObjectiveIndex("DestroyNomadMoon")] < 0) && ((universe.univUpdateCounter % 64) == 8) &&
        ((team = catMtTeam(0x0159517e)) != NULL))
    {
        catMtScout("DestroyNomadMoon (the moon is found)", &team->selection->ShipPtr[0]->obj.posinfo.position);
    }
    if (catMtStands("Recon") && ((universe.univUpdateCounter % 64) == 8) && catMtVolumeCentre(0x0479e17c, &centre))
    {
        catMtScout("Recon", &centre);
    }
    // the moon's two repulse emitters (NomadMoonSpec.emitter, the ships Watch_Mission17_NomadMoon counts)
    if (catMtStands("DestroyEmitters") && ((universe.univUpdateCounter % 64) == 24) && ((team = catMtTeam(0x0159517e)) != NULL))
    {
        moon = (NomadMoonSpec *)team->selection->ShipPtr[0]->ShipSpecifics;
        growSelectInit(&emitters);
        for (k = 0; k < 2; k++)
        {
            if ((moon->emitter[k] != NULL) && !bitTest(moon->emitter[k]->header.flags, SOF_Dead))
            {
                growSelectAddShip(&emitters, moon->emitter[k]);
            }
        }
        catMtDamage("DestroyEmitters", &emitters);
        growSelectClose(&emitters);
    }
    // the moon itself once its emitters are gone (its invincibility ends with them): first to half its hull (below
    //      60% Watch_Mission17 sends the Taiidan in, 0x0471a634: the Recon objective), then, once the recon has seen
    //      what came, destroyed
    if (catMtStands("DestroyNomadMoon") && !catMtStands("DestroyEmitters") &&
        (catMtCreatedTick[catMtObjectiveIndex("DestroyEmitters")] >= 0) && ((universe.univUpdateCounter % 64) == 40))
    {
        if (catMtDoneTick[catMtObjectiveIndex("Recon")] < 0)
        {
            catMtDamageTo("DestroyNomadMoon (to half its hull)", catMtTeam(0x0159517e), 0.5f);
        }
        else
        {
            catMtDamage("DestroyNomadMoon", catMtTeam(0x0159517e));
        }
    }
    // the NAGGAROK, awake once the moon is gone: TECH_INERTIALESS_DRIVE through the research manager (the script allows
    //      it when the NAGGAROK has been seen), the ACVs' EMP (the special action 4) until its drive fails
    //      (Watch_Mission17_Naggarok: TimesHitByEMP), then ShipsDamage
    if ((StrToTechType("TECH_INERTIALESS_DRIVE") >= 0) && tmTechResearchable[StrToTechType("TECH_INERTIALESS_DRIVE")])
    {
        catMtResearch("DestroyNaggarok", "TECH_INERTIALESS_DRIVE");
    }
    ships = memAlloc(sizeofSelectCommand(COMMAND_MAX_SHIPS), "catMtShips", 0);
    if (catMtStands("DestroyNaggarok") && !catMtStands("DestroyNomadMoon") &&
        (catMtCreatedTick[catMtObjectiveIndex("DestroyNomadMoon")] >= 0) && ((universe.univUpdateCounter % 256) == 72) &&
        ((team = catMtTeam(0x05bb641b)) != NULL) && (catMtPlayerShipsOnSpecial(sACV) == 0) &&
        (catMtPlayerShips(sACV, ships, COMMAND_MAX_SHIPS) > 0))
    {
        dbgMessagef("missiontest: event tick=%u DestroyNaggarok: times hit by EMP %d", (unsigned)universe.univUpdateCounter,
                    (int)kasfTimesHitByEMP(team));
        catMtSpecial("DestroyNaggarok (EMP)", 4, ships, team);
    }
    memFree(ships);
    if (catMtStands("DestroyNaggarok") && !catMtStands("DestroyNomadMoon") &&
        (catMtCreatedTick[catMtObjectiveIndex("DestroyNomadMoon")] >= 0) && ((universe.univUpdateCounter % 64) == 56))
    {
        catMtDamage("DestroyNaggarok", catMtTeam(0x05bb641b));
    }
    if ((universe.univUpdateCounter % 64) == 56)
    {
        // the moon (and its emitters) and the NAGGAROK are left to their objectives
        growSelectInit(&spare);
        if ((team = catMtTeam(0x0159517e)) != NULL)
        {
            moon = (NomadMoonSpec *)team->selection->ShipPtr[0]->ShipSpecifics;
            growSelectAddShip(&spare, team->selection->ShipPtr[0]);
            for (k = 0; k < 2; k++)
            {
                if ((moon->emitter[k] != NULL) && !bitTest(moon->emitter[k]->header.flags, SOF_Dead))
                {
                    growSelectAddShip(&spare, moon->emitter[k]);
                }
            }
        }
        if ((team = catMtTeam(0x05bb641b)) != NULL)
        {
            growSelectAddShip(&spare, team->selection->ShipPtr[0]);
        }
        catMtSpared = &spare;
        catMtDefendMothership("the command ship's defence", 12000.0f);
        catMtSpared = NULL;
        growSelectClose(&spare);
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
    CAT_MT_SCRIPT(catMtMission10, catMtPlayMission10),
    CAT_MT_SCRIPT(catMtMission11, catMtPlayMission11),
    CAT_MT_SCRIPT(catMtMission12, catMtPlayMission12),
    CAT_MT_SCRIPT(catMtMission13, catMtPlayMission13),
    CAT_MT_SCRIPT(catMtMission14, catMtPlayMission14),
    CAT_MT_SCRIPT(catMtMission15, catMtPlayMission15),
    CAT_MT_SCRIPT(catMtMission16, catMtPlayMission16),
    CAT_MT_SCRIPT(catMtMission17, catMtPlayMission17),
};

/*-----------------------------------------------------------------------------
    M2-R9 missions-02-09: the roll call (the missions whose jump is not catSpMissionNextLevel's letterbox one): the
    screen spHyperspaceButtonPushed puts up waits for the fleet to dock; 64 ticks after the command the test clicks its
    enabled button through the front end's callback (feFunctionExecute: Go Now, or Emergency Hyperspace while one of the
    player's ships is under fire; the other is disabled, RSF_RegionDisabled)
-----------------------------------------------------------------------------*/
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

    // M2-R9 missions-10-17: the tutorial leg logs its lessons' objective calls too (no judgement; a lesson sets an
    //      objective every frame, so a "set" is logged when its status changes)
    if ((catMissionTestTutorialTicks > 0) && !catMissionTestDone)
    {
        static char setLabel[32][MAX_OBJECTIVE_LABEL_LENGTH + 1];
        static sdword setStatus[32];
        static sdword setUsed = 0;
        sdword k;

        if (strcmp(what, "set") == 0)
        {
            for (k = 0; k < setUsed; k++)
            {
                if (strcmp(setLabel[k], label) == 0)
                {
                    break;
                }
            }
            if ((k < setUsed) && (setStatus[k] == status))
            {
                return;
            }
            if ((k == setUsed) && (setUsed < 32))
            {
                memStrncpy(setLabel[setUsed++], (char *)label, MAX_OBJECTIVE_LABEL_LENGTH);
            }
            if (k < 32)
            {
                setStatus[k] = status;
            }
        }
        dbgMessagef("missiontest: kas objective %s tick=%u %s %d", what, (unsigned)universe.univUpdateCounter, label, (int)status);
        return;
    }

    if ((catMissionTestTicks <= 0) || (catMtMission < 2) || !catMissionTestInit || catMissionTestDone)
    {
        return;
    }
    dbgMessagef("missiontest: kas objective %s tick=%u %s %d", what, (unsigned)universe.univUpdateCounter, label, (int)status);
    if (strcmp(what, "create") == 0)
    {
        for (i = 0; i < catMtNumObjectives; i++)
        {
            // M2-R9 missions-10-17: an objective the script creates again after it was completed is open again
            //      (Mission17 closes and re-creates DestroyNomadMoon and DestroyNaggarok as the story turns): it counts
            //      as completed once the script closes it again
            if ((strcmp(catMtObjectives[i].label, label) == 0) && (catMtCreatedTick[i] >= 0) && (catMtDoneTick[i] >= 0))
            {
                catMtDoneTick[i] = -1;
                dbgMessagef("missiontest: %s tick=%u reopened", catMtObjectives[i].label, (unsigned)universe.univUpdateCounter);
            }
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

/*-----------------------------------------------------------------------------
    M2-R10 tests-leftovers: the deliberate-loss test of the failed jump (/catMissionTestFailJump, harness
    --fail-jump). In cataclysm.exe singlePlayerGameInfo.hyperspaceFails is set by kasfDisablePlayerHyperspace
    0x004935e0 alone, which no mission script calls: the KAS dispatcher by name cat_kasCallFunctionByName 0x0045b9a0
    (hash 0x009c2c05) calls it for the developer console's KAS command (cat_consoleKasCommand 0x0045f270). The test
    takes that route at its first hyperspace command (no input: the call the console makes after parsing the typed
    line), so the jump runs the fizzle (UpdateHyperspacingShip's hyperspaceFails branch) and falls into
    singlePlayerGameUpdate's case HYPERSPACE_CATFAILEDRETURN (the exe's 10): no hyperspace, the ether cleaned
    (catSpEtherCleanup(FALSE, FALSE)), the lock-out lifted, the ships that must launch launched, "Game_HyperspaceFailed",
    hyperspaceFails cleared. Judged at the return: the state back at NO_HYPERSPACE with hyperspaceFails clear, the same
    level (no level load), the player's mothership outside and not hyperspacing, none of the player's ships in the
    ether, none lost (counted outside and docked: the roll call's Go Now docks the small ships). The run ends 32 ticks
    after the return: PASS = every objective of the table reached as in the normal run (the jump by the command), the
    level, KAS and arrival, no mission loss, the return judged ok and no next level. There is no second jump: the exe's
    case 10 does not give playerCanHyperspace back (spHyperspaceButtonPushed cleared it; only the mission script's
    kasfPlayerHyperspaceEnable-style calls would), so a failed jump leaves the player in the level, as in the exe. Only a
    roll-call jump can fail (the letterbox jump of catSpMissionNextLevel's missions ignores hyperspaceFails)
-----------------------------------------------------------------------------*/
static sdword catMtFailJumpArmTick = -1, catMtFailJumpBegunTick = -1, catMtFailJumpReturnTick = -1;
static sdword catMtFailJumpEndTick = -1;
static sdword catMtFailJumpShips = -1;
static bool32 catMtFailJumpPass = FALSE;

// the player's live ships outside the ether (in the universe or docked inside one of them: the roll call's Go Now
//      docks the small ships at once), and how many of those outside are hyperspacing (SOF_Hyperspace)
static sdword catMtFailJumpCount(sdword *hyperspacing, sdword *docked)
{
    Node *node;
    Ship *ship;
    sdword n = 0;

    *hyperspacing = 0;
    *docked = 0;
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner != universe.curPlayerPtr) || bitTest(ship->header.flags, SOF_Dead))
        {
            continue;
        }
        n++;
        if (bitTest(ship->header.flags, SOF_Hyperspace))
        {
            (*hyperspacing)++;
        }
        if (ship->shipsInsideMe != NULL)
        {
            *docked += (sdword)ship->shipsInsideMe->insideList.num;
        }
    }
    return n + *docked;
}

static void catMtFailJumpArm(void)
{
    void *args[1] = { NULL };
    sdword ret, hyperspacing, docked;

    catMtFailJumpArmTick = (sdword)universe.univUpdateCounter;
    catMtFailJumpShips = catMtFailJumpCount(&hyperspacing, &docked);
    ret = cat_kasCallFunctionByName("DisablePlayerHyperspace", args);
    dbgMessagef("missiontest: failed jump armed tick=%u (DisablePlayerHyperspace through cat_kasCallFunctionByName, the console's "
                "KAS call; ret=%d hyperspaceFails=%d playerShips=%d docked=%d)", (unsigned)universe.univUpdateCounter, (int)ret,
                (int)singlePlayerGameInfo.hyperspaceFails, (int)catMtFailJumpShips, (int)docked);
}

static void catMtFailJumpWatch(void)
{
    sdword ships, hyperspacing, ether, playerEther = 0, docked;
    Ship *mothership;
    bool32 mothershipOutside;
    Node *node;

    if (!catMtFailJump || (catMtFailJumpArmTick < 0) || (catMtFailJumpReturnTick >= 0))
    {
        return;
    }
    if ((catMtFailJumpBegunTick < 0) && (singlePlayerGameInfo.hyperspaceState != NO_HYPERSPACE))
    {
        catMtFailJumpBegunTick = (sdword)universe.univUpdateCounter;
        dbgMessagef("missiontest: failed jump begun tick=%u (hyperspaceState=%d hyperspaceFails=%d)",
                    (unsigned)universe.univUpdateCounter, (int)singlePlayerGameInfo.hyperspaceState,
                    (int)singlePlayerGameInfo.hyperspaceFails);
        return;
    }
    if ((catMtFailJumpBegunTick < 0) || (singlePlayerGameInfo.hyperspaceState != NO_HYPERSPACE))
    {
        return;
    }
    // the return: the exe's case 10 has run (it leaves NO_HYPERSPACE and clears hyperspaceFails)
    catMtFailJumpReturnTick = (sdword)universe.univUpdateCounter;
    ships = catMtFailJumpCount(&hyperspacing, &docked);
    ether = (sdword)singlePlayerGameInfo.ShipsInHyperspace.num;
    for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = node->next)
    {
        if (((InsideShip *)listGetStructOfNode(node))->ship->playerowner == universe.curPlayerPtr)
        {
            playerEther++;                      // (the level's own ships left in the ether may stay: Mission12)
        }
    }
    mothership = universe.curPlayerPtr->PlayerMothership;
    mothershipOutside = (mothership != NULL) && !bitTest(mothership->header.flags, SOF_Dead) && !bitTest(mothership->header.flags, SOF_Hyperspace);
    catMtFailJumpPass = !singlePlayerGameInfo.hyperspaceFails && (catMtNextLevelTick < 0) && mothershipOutside &&
                        (hyperspacing == 0) && (playerEther == 0) && (ships == catMtFailJumpShips) && !catMtFailed;
    dbgMessagef("missiontest: failed jump return tick=%u (case HYPERSPACE_CATFAILEDRETURN: hyperspaceState=%d hyperspaceFails=%d "
                "level=%s mothership=%s playerShips=%d (armed %d; docked %d) hyperspacing=%d ether=%d (the player's %d)) %s",
                (unsigned)universe.univUpdateCounter, (int)singlePlayerGameInfo.hyperspaceState,
                (int)singlePlayerGameInfo.hyperspaceFails, (catMtNextLevelTick < 0) ? "same" : "changed",
                mothershipOutside ? "outside" : "not outside", (int)ships, (int)catMtFailJumpShips, (int)docked, (int)hyperspacing,
                (int)ether, (int)playerEther, catMtFailJumpPass ? "ok" : "FAIL");
    // the run ends 32 ticks on (no second jump: playerCanHyperspace stays as the exe's case 10 leaves it)
    catMtFailJumpEndTick = (sdword)universe.univUpdateCounter + 32;
    dbgMessagef("missiontest: failed jump: playerCanHyperspace=%d after the return; the run ends at tick=%d",
                (int)singlePlayerGameInfo.playerCanHyperspace, (int)catMtFailJumpEndTick);
}

static void catMtPlay(void)
{
    sdword i;

    if ((catMtMission >= 2) && (catMtMissions[catMtMission].play != NULL) && !catMtWarpTest)
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
        if (catMtFailJump && (catMtFailJumpArmTick < 0) && (catMtMission >= 2))
        {
            catMtFailJumpArm();                 // M2-R10 tests-leftovers: the first command's jump is to fail
        }
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
    catMtFailJumpWatch();
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
    if ((catMtTutClickTick < 0) && (catMtTutPrompts >= CAT_MT_TUT_PROMPTS) && catMtTutObjPointer && (tutNextRegion != NULL) &&
        (objectiveFind("L1ClickHere") != NULL) && objectiveGetAll())     // M2-R9 missions-10-17: Lesson1's objective done
    {
        catMtTutClickTick = (sdword)universe.univUpdateCounter;
        dbgMessagef("missiontest: event tick=%u tutorial: the Next button clicked (its click callback)",
                    (unsigned)universe.univUpdateCounter);
        tutNextRegion->processFunction((regionhandle)tutNextRegion, 0, CM_ButtonClick, 0);
    }
}

/*-----------------------------------------------------------------------------
    M2-R9 missions-10-17: the Tutorial1 leg through Lesson12. The Tutorial FSM (Watch_Tutorial1_Tutorial 0x0066d2f0)
    goes one lesson on per Next click; each lesson's Init creates its objectives and its Watch sets them complete when
    the player has done the lesson's thing, which the game's UI reports through tutGameMessage (the key commands, the
    mouse handlers, the managers) or the script measures itself (the camera, the selection, the ships in the tutorial's
    volume). The test plays each lesson through the game's own handlers, with no input: the key commands through their
    dispatcher (cat_kbCommandExecute 0x0054eb10, as a key press runs them), the UI callbacks (mrRetire, mrScuttle, the
    objective list's click poPopupFleetIntelligence), the command layer, the construction and research managers, the
    camera setters the scripts use (KAS CameraSetAngleDeg ..), the selection API, and, where a mouse handler is the
    only sender, the message that handler sends with the order it gives (tutGameMessage, the tutorial's own hook). Once
    every objective of the lesson is complete, the Next button is clicked through its region's callback (R8's click).
    Lines: "missiontest: tutorial LessonN tick=T objectives=N" (the lesson begins), "missiontest: tutorial LessonN
    complete tick=T objectives=a/b", "missiontest: tutorial prompt ..." (every new subtitle text), "missiontest: event
    ... tutorial LessonN: ..."; the done line counts the lessons reached and completed.
-----------------------------------------------------------------------------*/
#define CAT_MT_TUT_LESSONS 12
static const udword catMtTutLessonState[CAT_MT_TUT_LESSONS + 1] =
{
    0, 0x00a883a1, 0x00a883a2, 0x00a883a3, 0x00a883a4, 0x00a883a5, 0x00a883a6, 0x00a883a7, 0x00a883a8, 0x00a883a9,
    0x0a883a20, 0x0a883a21, 0x0a883a22
};
// each lesson's objectives (some appear as the lesson goes on: L5Focus, L11Upgrade, L11Link): the lesson is complete
//      when that many stand and all are complete
static const sdword catMtTutLessonObjectives[CAT_MT_TUT_LESSONS + 1] = { 0, 1, 3, 5, 2, 3, 3, 2, 1, 3, 4, 3, 5 };
static sdword catMtTutReached[CAT_MT_TUT_LESSONS + 1], catMtTutComplete[CAT_MT_TUT_LESSONS + 1];
static sdword catMtTutObjectives[CAT_MT_TUT_LESSONS + 1], catMtTutPromptsIn[CAT_MT_TUT_LESSONS + 1];
static sdword catMtTutLesson = 0, catMtTutNextClicks = 0, catMtTutLastNextTick = -1;
static bool32 catMtTutLegInit = FALSE;
static sdword catMtTutSalvageTick = -1;

// the player's ships of a type (any type: -1) into the current selection (selSelected), the selection API's way
static sdword catMtTutSelect(ShipType type, sdword max, bool32 add)
{
    Node *node;
    Ship *ship;
    sdword n = 0;

    if (!add)
    {
        selSelected.numShips = 0;
    }
    for (node = universe.ShipList.head; (node != NULL) && (n < max); node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner == &universe.players[0]) && !bitTest(ship->header.flags, SOF_Dead | SOF_Hide | SOF_Hyperspace) &&
            ((type < 0) || (ship->shiptype == type)) && (ship->shiptype != sMothership))
        {
            selSelectionAddSingleShip(&selSelected, ship);
            n++;
        }
    }
    ioUpdateShipTotals();
    return n;
}

static void catMtTutEvent(sdword lesson, const char *what)
{
    dbgMessagef("missiontest: event tick=%u tutorial Lesson%d: %s", (unsigned)universe.univUpdateCounter, (int)lesson, what);
}

static void catMtTutMessage(sdword lesson, char *message)
{
    char line[96];

    snprintf(line, sizeof(line), "the UI's message %s", message);
    catMtTutEvent(lesson, line);
    tutGameMessage(message);
}

static void catMtTutKey(sdword lesson, sdword command, udword state, const char *name)
{
    char line[96];

    snprintf(line, sizeof(line), "the key command 0x%02x %s (state %u)", (unsigned)command, name, (unsigned)state);
    catMtTutEvent(lesson, line);
    cat_kbCommandExecute(state, command);
}

// a lesson's part, step by step (one step every 48 ticks once its objectives exist)
static void catMtTutPlayLesson(sdword lesson, sdword step)
{
    GrowSelection *team;
    SelectCommand *ships;
    vector *point;
    sdword angle;

    switch (lesson)
    {
        case 1:     // L1ClickHere: a click on the objective (the objective list's callback)
            if ((step >= 1) && (objectiveFind("L1ClickHere") != NULL) && !objectiveGet("L1ClickHere"))
            {
                catMtTutEvent(lesson, "a click on the objective L1ClickHere (poPopupFleetIntelligence)");
                poPopupFleetIntelligence(objectiveFind("L1ClickHere"));
            }
            break;
        case 2:     // the camera: round the fleet (> 180 degrees), from below (declination < -70), far (> 12000)
            if ((step >= 1) && (step <= 4))
            {
                angle = kasfCameraGetAngleDeg();
                catMtTutEvent(lesson, "the camera turns 60 degrees (CameraSetAngleDeg)");
                kasfCameraSetAngleDeg(angle + 60);
            }
            else if (step == 5)
            {
                catMtTutEvent(lesson, "the camera looks up from below (CameraSetDeclinationDeg -80)");
                kasfCameraSetDeclinationDeg(-80);
            }
            else if (step == 6)
            {
                catMtTutEvent(lesson, "the camera zooms out (CameraSetDistance 15000)");
                kasfCameraSetDistance(15000);
            }
            break;
        case 3:     // the selection: a formation's click, one ship's click, a band box, the ship list, an added ship
            if (step == 1)
            {
                catMtTutSelect(sRecon, COMMAND_MAX_SHIPS, FALSE);
                catMtTutMessage(lesson, "Game_ClickSelectGroup");
            }
            else if (step == 2)
            {
                catMtTutSelect(sRecon, 1, FALSE);
                catMtTutMessage(lesson, "Game_ClickSelectGroup");
            }
            else if (step == 3)
            {
                catMtTutSelect(sRecon, COMMAND_MAX_SHIPS, FALSE);
                catMtTutSelect(sAcolyte, COMMAND_MAX_SHIPS, TRUE);
                catMtTutMessage(lesson, "Game_BandBoxSelect");
            }
            else if (step == 4)
            {
                catMtTutSelect(sAcolyte, COMMAND_MAX_SHIPS, FALSE);
                catMtTutMessage(lesson, "ShipListClick");
            }
            else if (step == 5)
            {
                catMtTutSelect(sRecon, 1, TRUE);
                catMtTutMessage(lesson, "MultipleSelect");
            }
            break;
        case 4:     // the focus: the Focus key, the alt-click focus (ccFocusClose and the handler's messages)
            if (step == 1)
            {
                catMtTutSelect(sRecon, COMMAND_MAX_SHIPS, FALSE);
                catMtTutKey(lesson, 0x3B, 0, "Focus");
            }
            else if (step == 2)
            {
                catMtTutSelect(sAcolyte, COMMAND_MAX_SHIPS, FALSE);
                catMtTutEvent(lesson, "the alt-click focus (ccFocusClose)");
                ccFocusClose(&universe.mainCameraCommand, (FocusCommand *)&selSelected);
                catMtTutMessage(lesson, "Game_BandBoxFocus");
                catMtTutMessage(lesson, "AltFocus");
            }
            break;
        case 5:     // the right-click menu (its key), the sensors manager opened (its key) and closed again
            if (step == 1)
            {
                catMtTutSelect(sAcolyte, COMMAND_MAX_SHIPS, FALSE);
                catMtTutKey(lesson, 0x56, 0, "Open menu");
            }
            else if (step == 2)
            {
                if (cat_popupMenuIsOpen())
                {
                    catMtTutEvent(lesson, "the menu closes (cat_popupMenuClose)");
                    cat_popupMenuClose();
                }
                catMtTutKey(lesson, 0x4F, 0, "Sensors manager");
            }
            else if ((step >= 5) && smSensorsActive && (objectiveFind("L5Focus") != NULL))
            {
                catMtTutKey(lesson, 0x4F, 1, "Sensors manager (closed)");
            }
            break;
        case 6:     // the move (to the marker, team 0x004bd484), a waypoint path, a looped one
            if ((step == 1) && ((point = (vector *)kasTeamsVectorPtr(0x004bd484)) != NULL) &&
                (catMtTutSelect(sAcolyte, COMMAND_MAX_SHIPS, FALSE) > 0))
            {
                catMtTutMessage(lesson, "MoveStart");
                catMtMove("tutorial Lesson6", (SelectCommand *)&selSelected, point);
            }
            else if (((step == 2) || (step == 3)) && ((point = (vector *)kasTeamsVectorPtr(0x004bd484)) != NULL) &&
                     (catMtTutSelect(sRecon, COMMAND_MAX_SHIPS, FALSE) > 0))
            {
                vector waypoints[2];

                waypoints[0] = *point;
                waypoints[1] = selSelected.ShipPtr[0]->obj.posinfo.position;
                waypoints[1].x += 3000.0f;
                catMtTutMessage(lesson, "WaypointStart");
                catMtTutEvent(lesson, (step == 2) ? "a waypoint path (clWrapMove, once)" : "a looped waypoint path (clWrapMove, loop)");
                clWrapMove(&universe.mainCommandLayer, (SelectCommand *)&selSelected,
                           (step == 2) ? CAT_MOVEPATH_ONCE : CAT_MOVEPATH_LOOP, selSelected.ShipPtr[0]->obj.posinfo.position,
                           2, waypoints);
                catMtTutMessage(lesson, (step == 2) ? "WaypointNormal" : "WaypointCircular");
            }
            break;
        case 7:     // the attack on the targets (the buoys, team 0x0866bac3): a click attack, a band box attack
            if (((step == 1) || (step == 2)) && ((team = catMtTeam(0x0866bac3)) != NULL) &&
                (catMtTutSelect(sAcolyte, COMMAND_MAX_SHIPS, FALSE) > 0))
            {
                catMtAttack("tutorial Lesson7", (SelectCommand *)&selSelected, team);
                if (step == 1)
                {
                    catMtTutMessage(lesson, "ClickAttack");
                }
                else
                {
                    catMtTutMessage(lesson, "Game_BandBoxAttack");
                    catMtTutMessage(lesson, "BandAttack");
                }
            }
            break;
        case 8:     // two workers built (the construction manager's jobs)
            if ((step == 1) || (step == 2))
            {
                catMtBuild("tutorial Lesson8", sWorker);
            }
            break;
        case 9:     // repair the damaged frigate (team 0x046a4072), salvage the ion frigate (team 0x0761fcf5), harvest
            ships = memAlloc(sizeofSelectCommand(COMMAND_MAX_SHIPS), "catMtTutShips", 0);
            if (((step % 8) == 1) && !objectiveGet("L9Repair") && ((team = catMtTeam(0x046a4072)) != NULL) &&
                (catMtPlayerShips(sWorker, ships, 2) > 0))
            {
                catMtSpecial("tutorial Lesson9 (repair)", 1, ships, team);
            }
            // the salvage (a frigate takes two workers): the two workers, once the repair is done (the tutorial's
            //      fleet has the two built in Lesson8); given again only if the salvage has not ended 2048 ticks later
            if (objectiveGet("L9Repair") && !objectiveGet("L9Salvage") && ((team = catMtTeam(0x0761fcf5)) != NULL) &&
                ((catMtTutSalvageTick < 0) || ((sdword)universe.univUpdateCounter > catMtTutSalvageTick + 2048)) &&
                (catMtPlayerShips(sWorker, ships, 2) == 2))
            {
                catMtTutSalvageTick = (sdword)universe.univUpdateCounter;
                catMtSpecial("tutorial Lesson9 (salvage)", 0, ships, team);
            }
            if ((step == 2) && (catMtTutSelect(sWorker, 1, FALSE) > 0))
            {
                catMtTutKey(lesson, 0x11, 0, "Harvest");
            }
            memFree(ships);
            break;
        case 10:    // the delta formation, the aggressive tactics, the band box guard, a hot-key group
            if ((step == 1) && (catMtTutSelect(sAcolyte, COMMAND_MAX_SHIPS, FALSE) > 0))
            {
                catMtTutKey(lesson, 0x31, 0, "Delta formation");
            }
            else if ((step == 2) && (catMtTutSelect(sAcolyte, COMMAND_MAX_SHIPS, FALSE) > 0))
            {
                catMtTutKey(lesson, 0x2E, 0, "Aggressive tactics");
            }
            else if ((step == 3) && (catMtTutSelect(sAcolyte, COMMAND_MAX_SHIPS, FALSE) > 0) &&
                     (universe.players[0].PlayerMothership != NULL))
            {
                GrowSelection guarded;

                growSelectInit(&guarded);
                growSelectAddShip(&guarded, universe.players[0].PlayerMothership);
                catMtGuard("tutorial Lesson10", (SelectCommand *)&selSelected, &guarded);
                growSelectClose(&guarded);
                catMtTutMessage(lesson, "Game_BandBoxGuard");
            }
            else if ((step == 4) && (catMtTutSelect(sRecon, COMMAND_MAX_SHIPS, FALSE) > 0))
            {
                catMtTutSelect(sAcolyte, COMMAND_MAX_SHIPS, TRUE);
                catMtTutKey(lesson, 0x6C, 0, "Create group 1");
            }
            break;
        case 11:    // the research manager opened (its key), TECH_LINK researched through its own button, the acolytes
                    //      upgraded, two linked. TECH_LINK is a technology of the hangar's laboratory (cat_techLabType
                    //      24 = 0, the exe's table 0x00890388) and the tutorial's fleet has no research module (its five
                    //      laboratories closed): the research manager's Research button (rmResearchItem 0x004f4c50)
                    //      starts a topic in a laboratory not built yet when it is the tutorial, so the test selects the
                    //      TECH_LINK row of its list and presses that button (its callback)
            if ((step == 1) && !rmGUIActive)
            {
                catMtTutKey(lesson, 0x51, 0, "Research manager");
            }
            if ((step >= 2) && ((step % 8) == 2) && rmGUIActive && !objectiveGet("L11Research") && !catMtHasTech("TECH_LINK") &&
                (Researching(&universe.players[0], StrToTechType("TECH_LINK")) == NULL) && (rmTechListWindowHandle != NULL) &&
                (PlayerTechList != NULL))
            {
                TechPrintList *row;

                for (row = PlayerTechList; row->itemtype != ITEM_ENDLIST; row++)
                {
                    if (((row->itemtype == CAT_ITEM_TECH2) || (row->itemtype == CAT_ITEM_TECH3)) &&
                        (row->itemID == StrToTechType("TECH_LINK")))
                    {
                        catMtTutEvent(lesson, "the TECH_LINK row selected, the Research button (rmResearchItem)");
                        uicListWindowSetCurItem(rmTechListWindowHandle, uicListFindItemByData(rmTechListWindowHandle, (ubyte *)row));
                        rmResearchItem(NULL, NULL);
                        break;
                    }
                }
            }
            if ((step >= 2) && ((step % 8) == 4) && rmGUIActive && objectiveGet("L11Research"))
            {
                // the research manager's Close button (rmExitMenu): open, it keeps the orders off (mrDisabled)
                catMtTutEvent(lesson, "the research manager's Close button (rmExitMenu)");
                rmExitMenu(NULL, NULL);
            }
            if ((step >= 2) && ((step % 8) == 2) && catMtHasTech("TECH_LINK") && !objectiveGet("L11Upgrade"))
            {
                catMtUpgrade("tutorial Lesson11", sAcolyte);
            }
            if ((step >= 2) && ((step % 8) == 6) && objectiveGet("L11Upgrade") && !objectiveGet("L11Link"))
            {
                // two acolytes whose link upgrade is done (cat_uiLink 0x00565570 keeps only those). Their link flight
                //      (cat_processLinkToDo 0x00467b80) flies with AISHIP_DontFlyToObscuredPoints |
                //      AISHIP_ReturnImmedIfPointObscured: next to the command ship (the tutorial's fleet waits about
                //      2400 from it) the meeting point is inside its obscuring radius and the leader never moves, so
                //      the pair is first moved 6000 out from the command ship, as a player links in open space
                Ship *mothership = universe.players[0].PlayerMothership;
                Node *node;
                sdword upgraded = 0, linking = 0, near = 0;
                char line[96];

                selSelected.numShips = 0;
                for (node = universe.ShipList.head; node != NULL; node = node->next)
                {
                    Ship *ship = (Ship *)listGetStructOfNode(node);
                    if ((ship->playerowner == &universe.players[0]) && (ship->shiptype == sAcolyte) &&
                        !bitTest(ship->header.flags, SOF_Dead | SOF_Hide | SOF_Hyperspace) &&
                        cat_shipHasUpgrade(ship, CAT_UPGRADE_Link) && (upgraded < 2))
                    {
                        CommandToDo *command = getShipAndItsCommand(&universe.mainCommandLayer, ship);

                        if ((command != NULL) && (command->ordertype.order == COMMAND_CAT_LINK))
                        {
                            linking++;
                        }
                        if ((mothership != NULL) &&
                            (cat_vecDistance(&ship->obj.posinfo.position, &mothership->obj.posinfo.position) < 5000.0f))
                        {
                            near++;
                        }
                        selSelectionAddSingleShip(&selSelected, ship);
                        upgraded++;
                    }
                }
                ioUpdateShipTotals();
                snprintf(line, sizeof(line), "acolytes with the link upgrade %d, linking %d, near the command ship %d",
                         (int)upgraded, (int)linking, (int)near);
                catMtTutEvent(lesson, line);
                if ((upgraded == 2) && (linking == 0) && (near > 0))
                {
                    vector out, destination;
                    real32 length;

                    vecSub(out, selSelected.ShipPtr[0]->obj.posinfo.position, mothership->obj.posinfo.position);
                    length = fsqrt(vecMagnitudeSquared(out));
                    if (length < 1.0f)
                    {
                        out.x = 1.0f; out.y = 0.0f; out.z = 0.0f; length = 1.0f;
                    }
                    destination.x = mothership->obj.posinfo.position.x + out.x * (6000.0f / length);
                    destination.y = mothership->obj.posinfo.position.y + out.y * (6000.0f / length);
                    destination.z = mothership->obj.posinfo.position.z + out.z * (6000.0f / length);
                    catMtTutEvent(lesson, "the pair moved 6000 out from the command ship (clWrapMove)");
                    clWrapMove(&universe.mainCommandLayer, (SelectCommand *)&selSelected, 0,
                               selSelected.ShipPtr[0]->obj.posinfo.position, 1, &destination);
                }
                else if ((upgraded == 2) && (linking == 0))
                {
                    catMtTutKey(lesson, 0x1A, 0, "Link");
                }
            }
            break;
        case 12:    // the tactical display (on, off), the chase camera, a retire, a scuttle, the time compression
            if (step == 1)
            {
                catMtTutKey(lesson, 0x53, 0, "Tactical display");
            }
            else if (step == 2)
            {
                catMtTutKey(lesson, 0x53, 0, "Tactical display");
            }
            else if ((step == 3) && (catMtTutSelect(sRecon, 1, FALSE) > 0))
            {
                catMtTutKey(lesson, 0x47, 0, "Chase");
            }
            else if ((step == 4) && (catMtTutSelect(sAcolyte, 2, FALSE) > 0))
            {
                catMtTutEvent(lesson, "the Retire button (mrRetire)");
                mrRetire(NULL, NULL);
            }
            else if ((step == 5) && (catMtTutSelect(sRecon, 1, FALSE) > 0))
            {
                // the confirming press comes within TW_ScuttleReconfirmTime (3 s of universe.realtimeelapsed, which the
                //      virtual clock advances 1/16 s a tick: the next step, 48 ticks on, would be 3.0 s, not under it)
                catMtTutEvent(lesson, "the Scuttle button (mrScuttle)");
                mrScuttle(NULL, NULL);
                catMtTutEvent(lesson, "the Scuttle button again (mrScuttle: confirmed)");
                mrScuttle(NULL, NULL);
            }
            else if (step == 7)
            {
                catMtTutKey(lesson, 0x5D, 0, "Accelerate time");
            }
            else if (step == 8)
            {
                catMtTutKey(lesson, 0x5E, 0, "Decelerate time");
            }
            break;
        default:
            break;
    }
}

// the leg: each lesson's start, its part played, its objectives complete, then the Next button
static void catMtTutorialLeg(void)
{
    AITeam *team = (universe.players[1].aiPlayer != NULL) ? kasAITeamPtr(0x0a9468cc) : NULL;    // team "Tutorial"
    sdword n, since;

    if (!catMtTutLegInit)
    {
        catMtTutLegInit = TRUE;
        for (n = 0; n <= CAT_MT_TUT_LESSONS; n++)
        {
            catMtTutReached[n] = catMtTutComplete[n] = -1;
            catMtTutObjectives[n] = catMtTutPromptsIn[n] = 0;
        }
    }
    if (team == NULL)
    {
        return;
    }
    for (n = 1; n <= CAT_MT_TUT_LESSONS; n++)
    {
        if ((team->kasStateName == catMtTutLessonState[n]) && (catMtTutReached[n] < 0))
        {
            catMtTutReached[n] = (sdword)universe.univUpdateCounter;
            catMtTutLesson = n;
            dbgMessagef("missiontest: tutorial Lesson%d tick=%u", (int)n, (unsigned)universe.univUpdateCounter);
        }
    }
    n = catMtTutLesson;
    if ((n < 1) || (catMtTutReached[n] < 0) || (team->kasStateName != catMtTutLessonState[n]))
    {
        return;
    }
    // every new prompt (a subtitle card with a new text) of the lesson
    {
        static char last[96] = "";
        sdword r;

        for (r = 0; r < SUB_NumberRegions; r++)
        {
            if ((subRegion[r].cardIndex > 0) && (subRegion[r].card != NULL) && (subRegion[r].card[0].text != NULL) &&
                (strncmp(last, subRegion[r].card[0].text, sizeof(last) - 1) != 0))
            {
                strncpy(last, subRegion[r].card[0].text, sizeof(last) - 1);
                catMtTutPromptsIn[n]++;
                dbgMessagef("missiontest: tutorial prompt lesson=%d tick=%u region=%d text='%.80s'", (int)n,
                            (unsigned)universe.univUpdateCounter, (int)r, subRegion[r].card[0].text);
                break;
            }
        }
    }
    if (objectivesUsed > catMtTutObjectives[n])
    {
        catMtTutObjectives[n] = objectivesUsed;
    }
    since = (sdword)universe.univUpdateCounter - catMtTutReached[n];
    if ((objectivesUsed > 0) && (since > 0) && ((since % 48) == 0) && (catMtTutComplete[n] < 0))
    {
        catMtTutPlayLesson(n, since / 48);
    }
    if ((catMtTutComplete[n] < 0) && (objectivesUsed >= catMtTutLessonObjectives[n]) && (objectivesUsed > 0) &&
        objectiveGetAll())
    {
        catMtTutComplete[n] = (sdword)universe.univUpdateCounter;
        dbgMessagef("missiontest: tutorial Lesson%d complete tick=%u objectives=%d/%d", (int)n,
                    (unsigned)universe.univUpdateCounter, (int)objectivesUsed, (int)catMtTutObjectives[n]);
    }
    // the Next button 96 ticks after the lesson's objectives are complete (the lesson's closing prompt meanwhile); a
    //      lesson not complete 9600 ticks after its start is left (the leg then FAILs); Lesson1's click is R8's
    if ((n >= 2) && (n < CAT_MT_TUT_LESSONS) && (tutNextRegion != NULL) && (catMtTutLastNextTick < catMtTutReached[n]) &&
        (((catMtTutComplete[n] >= 0) && ((sdword)universe.univUpdateCounter >= catMtTutComplete[n] + 96)) || (since >= 9600)))
    {
        catMtTutLastNextTick = (sdword)universe.univUpdateCounter;
        catMtTutNextClicks++;
        dbgMessagef("missiontest: event tick=%u tutorial Lesson%d: the Next button clicked (its click callback)",
                    (unsigned)universe.univUpdateCounter, (int)n);
        tutNextRegion->processFunction((regionhandle)tutNextRegion, 0, CM_ButtonClick, 0);
    }
}

/*-----------------------------------------------------------------------------
    HWI-TEST (M3-R3 mission02): the level's resources once the level is up (the user's play-test item 1, the dust
    belts): the asteroids on the resource list by type (the meteors, CAT_ATTRIBUTES_KillerCollision, apart), the
    decorative Asteroid0 on the minor list (univAddAsteroid: never on the resource list; the sensors manager draws
    them as points with no fog-of-war test, cat_smViewportRenderScene 0x0050b4b0), the dust clouds and gas clouds by
    type, the nebula tendrils, and each dust cloud's type and position (read only)
-----------------------------------------------------------------------------*/
static void catMtResourcesLog(void)
{
    Node *node;
    sdword asteroids[NUM_ASTEROIDTYPES] = { 0 }, dust[NUM_DUSTCLOUDTYPES] = { 0 }, gas[NUM_GASCLOUDTYPES] = { 0 };
    sdword meteors = 0, minor0 = 0, minorOther = 0, nebulae = 0, other = 0, listed = 0;
    char dustText[96];

    for (node = universe.ResourceList.head; node != NULL; node = node->next)
    {
        SpaceObj *obj = (SpaceObj *)listGetStructOfNode(node);

        switch (obj->header.objtype)
        {
            case OBJ_AsteroidType:
                if (bitTest(obj->attributes, CAT_ATTRIBUTES_KillerCollision))
                {
                    meteors++;
                }
                else if (((Asteroid *)obj)->asteroidtype < NUM_ASTEROIDTYPES)
                {
                    asteroids[((Asteroid *)obj)->asteroidtype]++;
                }
                break;
            case OBJ_DustType:
                if (((DustCloud *)obj)->dustcloudtype < NUM_DUSTCLOUDTYPES)
                {
                    dust[((DustCloud *)obj)->dustcloudtype]++;
                }
                if (listed < 32)
                {
                    listed++;
                    dbgMessagef("missiontest: dustcloud type=%d pos=(%.0f, %.0f, %.0f)", (int)((DustCloud *)obj)->dustcloudtype,
                                obj->posinfo.position.x, obj->posinfo.position.y, obj->posinfo.position.z);
                }
                break;
            case OBJ_GasType:
                if (((GasCloud *)obj)->gascloudtype < NUM_GASCLOUDTYPES)
                {
                    gas[((GasCloud *)obj)->gascloudtype]++;
                }
                break;
            case OBJ_NebulaType:
                nebulae++;
                break;
            default:
                other++;
                break;
        }
    }
    for (node = universe.MinorSpaceObjList.head; node != NULL; node = node->next)
    {
        SpaceObj *obj = (SpaceObj *)listGetStructOfNode(node);

        if ((obj->header.objtype == OBJ_AsteroidType) && (((Asteroid *)obj)->asteroidtype == Asteroid0))
        {
            minor0++;
        }
        else
        {
            minorOther++;
        }
    }
    snprintf(dustText, sizeof(dustText), "%d/%d/%d/%d", (int)dust[0], (int)dust[1], (int)dust[2], (int)dust[3]);
    dbgMessagef("missiontest: resources tick=%u asteroids=%d/%d/%d/%d/%d meteors=%d minor asteroid0=%d other=%d dustclouds=%s"
                " gasclouds=%d nebulae=%d other=%d volumes=%u", (unsigned)universe.univUpdateCounter, (int)asteroids[0],
                (int)asteroids[1], (int)asteroids[2], (int)asteroids[3], (int)asteroids[4], (int)meteors, (int)minor0,
                (int)minorOther, dustText, (int)(gas[0] + gas[1]), (int)nebulae, (int)other,
                (unsigned)universe.ResourceVolumeList.num);
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
           catMtKasStarted && catMtArrived;
    if (catMtMission == MISSION_17)
    {
        // M2-R9 missions-10-17: the last mission ends the game: its script's MissionCompleted, then the credits (no
        //      hyperspace jump, no next level)
        pass = pass && (catMtCompletedTick >= 0) && (catMtGameEndTick >= 0);
    }
    else if (catMtFailJump)
    {
        // M2-R10 tests-leftovers: the jump begun, failed and returned (judged ok), no next level
        pass = pass && (catMtHyperspaceTick >= 0) && (catMtNextLevelTick < 0) && (catMtFailJumpReturnTick >= 0) &&
               catMtFailJumpPass;
    }
    else
    {
        pass = pass && (catMtHyperspaceTick >= 0) && (catMtNextLevelTick >= 0);
    }
    // HWI-TEST (M3-R3 mission02): the sensors manager's frames over the run: the decorative Asteroid0 points (no
    //      fog-of-war test: every one in every frame) against the resource points (only where the grid has seen them)
    dbgMessagef("missiontest: sensors scenes=%u dustpoints=%u resourcepoints=%u minor=%u", catSmDrawCounts.scenes,
                catSmDrawCounts.dustPoints, catSmDrawCounts.resources, (unsigned)universe.MinorSpaceObjList.num);
    dbgMessagef("missiontest: sensorspoints markers 1px=%u 2px=%u 3px=%u dustsize=%u sensorsPoints=%d",   // HWI-TEST (M3-R4 sensors-points)
                catSmDrawCounts.markerSizes[1], catSmDrawCounts.markerSizes[2], catSmDrawCounts.markerSizes[3],   // HWI-TEST (M3-R4 sensors-points)
                catSmDrawCounts.dustSize, (int)hwiSensorsPoints);                                                 // HWI-TEST (M3-R4 sensors-points)
    if (catMtMission == 2)
    {
        // HWI-TEST (M3-R3 mission02): the BUSHAN-RE hint fired exactly under the exe's conditions
        bool32 hint = catMtHintJudge();

        pass = pass && hint;
    }
    if (catMtFailJump)
    {
        dbgMessagef("missiontest: failed jump judged armed=%d begun=%d return=%d %s", (int)catMtFailJumpArmTick,
                    (int)catMtFailJumpBegunTick, (int)catMtFailJumpReturnTick,
                    ((catMtFailJumpReturnTick >= 0) && catMtFailJumpPass) ? "PASS" : "FAIL");
    }
    dbgMessagef("missiontest: end mission=%d tick=%u level=%d kas=%d arrived=%d inOrder=%d jump=%d rollcall=%s next=%d failed=%d"
                " completed=%d gameend=%d",
                (int)catMtMission, (unsigned)universe.univUpdateCounter, (int)catMtLevelLoaded, (int)catMtKasStarted,
                (int)catMtArrived, (int)inOrder, (int)catMtHyperspaceTick, catMtRollCallButton, (int)catMtNextLevelTick,
                (int)catMtFailed, (int)catMtCompletedTick, (int)catMtGameEndTick);
    catTeamColourDone();                                // HWI-TEST (M3-R1 team-colours-warp)
    // ticks: the mission level's ticks and the next level's (the run's ticks); at the game's end the mission level's
    dbgMessagef("missiontest: done mission=%d ticks=%u objectives=%d/%d result=%s", (int)catMtMission,
                (unsigned)((catMtGameEndTick >= 0) ? catMtLevelTicks :
                           (catMtNextLevelTick >= 0) ? catMtLevelTicks + universe.univUpdateCounter : universe.univUpdateCounter),
                (int)reached, (int)catMtNumObjectives,
                pass ? "PASS" : "FAIL");
    oracleMissionStopAfterThisTick();
}

// M2-R9 missions-10-17: the game's end (Mission17): the credits have begun (utyGameQuitToMain after the quit that
//      MissionCompleted's singlePlayerMissionCompleteCB asked for); the game no longer runs, so the run ends here
static void catMtGameEnd(const char *credits)
{
    if ((catMissionTestTicks <= 0) || (catMtMission < 2) || !catMissionTestInit || catMissionTestDone)
    {
        return;
    }
    catMtGameEndTick = (sdword)catMtLevelTicks;
    dbgMessagef("missiontest: game end tick=%u credits=%s completed=%d (MissionCompleted, the quit, the credits)",
                (unsigned)catMtLevelTicks, credits, (int)catMtCompletedTick);
    catMissionTestDone = TRUE;
    catMtMissionEnd();
    oracleMissionStopNow();
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
        catMtTutorialLeg();
        // M2-R9 missions-10-17: the leg ends with Lesson12's objectives complete (the last lesson has no Next)
        if ((catMtTutLesson2Tick >= 0) && (catMtTutObjTick >= 0) && (catMtTutLessonTick >= 0) && (catMtTutButtonsTick >= 0) &&
            (catMtTutComplete[CAT_MT_TUT_LESSONS] >= 0) &&
            ((sdword)universe.univUpdateCounter >= catMtTutComplete[CAT_MT_TUT_LESSONS] + 96))
        {
            sdword n, reached = 0, complete = 0, inOrder = TRUE;

            for (n = 1; n <= CAT_MT_TUT_LESSONS; n++)
            {
                reached += (catMtTutReached[n] >= 0);
                complete += (catMtTutComplete[n] >= 0);
                if ((n > 1) && ((catMtTutReached[n] < 0) || (catMtTutReached[n] <= catMtTutReached[n - 1])))
                {
                    inOrder = FALSE;
                }
            }
            catMissionTestDone = TRUE;
            dbgMessagef("missiontest: done ticks=%u tutorial kas=%d lesson1=%d objectives=%d buttons=%d prompts=%d/%d/%d "
                        "pointer=%d next=%d lesson2=%d lessons=%d/%d complete=%d/%d inOrder=%d nextClicks=%d result=%s",
                        (unsigned)universe.univUpdateCounter, (int)catMtKasStarted, (int)catMtTutLessonTick, (int)catMtTutObjTick,
                        (int)catMtTutButtonsTick, (int)catMtTutPromptTick[0], (int)catMtTutPromptTick[1],
                        (int)catMtTutPromptTick[2], (int)catMtTutObjPointer, (int)catMtTutClickTick, (int)catMtTutLesson2Tick,
                        (int)reached, CAT_MT_TUT_LESSONS, (int)complete, CAT_MT_TUT_LESSONS, (int)inOrder,
                        (int)catMtTutNextClicks + 1,
                        ((reached == CAT_MT_TUT_LESSONS) && (complete == CAT_MT_TUT_LESSONS) && inOrder) ? "PASS" : "FAIL");
            oracleMissionStopAfterThisTick();
            return;
        }
    }

    // M2-R9 missions-10-17: the level's mission-sphere objects the "Mine" / "Slipgate" readers made (LevelLoad.c
    //      cat_scriptSetMineCB 0x004a3f20, cat_scriptSetSlipgateCB 0x004a1020), counted once the level is up
    if ((catMissionTestTicks > 0) && (catMtMission >= 2) && catMtLevelLoaded && (catMtNextLevelTick < 0))
    {
        static bool32 counted = FALSE;

        if (!counted)
        {
            Node *node;
            sdword mines = 0, gates = 0;

            counted = TRUE;
            for (node = universe.MissileList.head; node != NULL; node = node->next)
            {
                if (((Missile *)listGetStructOfNode(node))->missileType == MISSILE_Mine)
                {
                    mines++;
                }
            }
            for (node = universe.ShipList.head; node != NULL; node = node->next)
            {
                ShipType type = ((Ship *)listGetStructOfNode(node))->shiptype;

                if ((type == dSlipgateSmall) || (type == dSlipgateMedium) || (type == dSlipgateLarge))
                {
                    gates++;
                }
            }
            dbgMessagef("missiontest: level objects tick=%u mines=%d slipgates=%d ships=%u", (unsigned)universe.univUpdateCounter,
                        (int)mines, (int)gates, (unsigned)universe.ShipList.num);
            catMtResourcesLog();                            // HWI-TEST (M3-R3 mission02)
        }
    }
    // M2-R9 missions-10-17: the warp start's self-test: the record against the fleet once it has arrived, the end 64
    //      ticks later (or at the limit, or when the player has lost)
    if (catMtWarpTest && (catMissionTestTicks > 0))
    {
        bool32 pass;

        if (catMtArrived && (catMtWarpArrivedTick < 0))
        {
            catMtWarpArrivedTick = (sdword)universe.univUpdateCounter;
            catMtWarpArrived = cat_spWarpStartSelfTestCheck(TRUE) ? 1 : 0;
        }
        if (((catMtWarpArrivedTick >= 0) && ((sdword)universe.univUpdateCounter >= catMtWarpArrivedTick + 64)) ||
            ((sdword)universe.univUpdateCounter >= catMissionTestTicks - 2) || catMtFailed)
        {
            pass = (catMtWarpScriptLevel >= 2) && (catMtWarpApplied == 1) && catMtLevelLoaded && catMtKasStarted &&
                   catMtArrived && (catMtWarpArrived == 1) && !catMtFailed &&
                   (singlePlayerGameInfo.currentMission == catMtWarpScriptLevel);
            catMissionTestDone = TRUE;
            dbgMessagef("warpstarttest: done level=%d ticks=%u applied=%d level=%d kas=%d arrived=%d fleet=%d failed=%d result=%s",
                        (int)catMtWarpScriptLevel, (unsigned)universe.univUpdateCounter, (int)catMtWarpApplied,
                        (int)catMtLevelLoaded, (int)catMtKasStarted, (int)catMtWarpArrivedTick, (int)catMtWarpArrived,
                        (int)catMtFailed, pass ? "PASS" : "FAIL");
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
        ((catMissionTestTicks > 0) && (catMtMission == 1) && (catMtHyperspaceTick >= 0) && !catTeamColourTest &&
         ((sdword)universe.univUpdateCounter >= catMtHyperspaceTick + 32)) ||
        ((catMissionTestTicks > 0) && (catMtMission >= 2) && (catMtNextLevelTick >= 0) && !catTeamColourTest &&
         ((sdword)universe.univUpdateCounter >= catMtNextLevelTick + 32)) ||
        ((catMissionTestTicks > 0) && catTeamColourTest && (catMtTcNextTick >= 0) &&      // HWI-TEST (M3-R1 team-colours-warp)
         ((sdword)universe.univUpdateCounter >= catMtTcNextTick + catTeamColourNextTicks)) ||
        ((catMissionTestTicks > 0) && (catMtMission >= 2) && catMtFailed) ||     // M2-R9 missions-10-17: a lost mission ends the run
        ((catMissionTestTicks > 0) && (catMtMission >= 2) && (catMtFailJumpEndTick >= 0) &&
         ((sdword)universe.univUpdateCounter >= catMtFailJumpEndTick)))         // M2-R10 tests-leftovers: the failed jump's end
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
            catTeamColourDone();                                // HWI-TEST (M3-R1 team-colours-warp)
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
// HWI-TEST (M3-R4 infect-convert): /catInfectAt <tick>:<ShipType>: in the load-game self-test, that many ticks after
//      the load the current player's first ship of that type is infected by the first ship a Beast player owns
//      (cat_beastInfectShip: what a Beast infection missile's hit does) and the outcome is reported ("infecttest:"
//      lines: the Beast type the statics name, whether its statics are loaded, then the player's and the Beast's
//      counts of the old and new type once the ship has left the list). Read only otherwise.
static sdword catInfectAtTick = -1;
static char catInfectAtType[64] = "";
static Ship *catInfectTarget = NULL;
static ShipType catInfectOldType = 0, catInfectNewType = 0;
static Player *catInfectOwner = NULL;
static bool32 catInfectReported = FALSE;
void catInfectAtSet(char *string)
{
    char *colon = strchr(string, ':');

    if (colon == NULL)
    {
        catInfectAtTick = -1;
        return;
    }
    catInfectAtTick = (sdword)strtol(string, NULL, 10);
    strncpy(catInfectAtType, colon + 1, sizeof(catInfectAtType) - 1);
    catInfectAtType[sizeof(catInfectAtType) - 1] = '\0';
}
static sdword catInfectCountType(Player *player, ShipType type)   // player NULL: any Beast player's
{
    Node *node;
    sdword n = 0;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        Ship *s = (Ship *)listGetStructOfNode(node);

        if ((s->shiptype == type) && (s->playerowner != NULL) &&
            ((player != NULL) ? (s->playerowner == player) : (s->playerowner->race == RACE_Beast)))
        {
            n++;
        }
    }
    return n;
}
static void catInfectTick(sdword sinceLoad)
{
    Node *node;

    if ((catInfectAtTick >= 0) && (sinceLoad == catInfectAtTick) && (catInfectTarget == NULL) && !catInfectReported)
    {
        ShipType type = StrToShipType(catInfectAtType);
        Ship *target = NULL, *infector = NULL;
        ShipStaticInfo *newStatic;

        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            Ship *s = (Ship *)listGetStructOfNode(node);

            if ((target == NULL) && (s->playerowner == universe.curPlayerPtr) && (s->shiptype == type))
            {
                target = s;
            }
            if ((infector == NULL) && (s->playerowner != NULL) && (s->playerowner != universe.curPlayerPtr) &&
                (s->playerowner->race == RACE_Beast))
            {
                infector = s;
            }
        }
        if ((type < 0) || (type >= TOTAL_NUM_SHIPS) || (target == NULL) || (infector == NULL) ||
            (target->staticinfo->beastAttacksRequired < 0))
        {
            dbgMessagef("infecttest: no infection tick=%u type='%s' target=%d infector=%d infectable=%d",
                        (unsigned)universe.univUpdateCounter, catInfectAtType, (int)(target != NULL), (int)(infector != NULL),
                        (int)((target != NULL) && (target->staticinfo->beastAttacksRequired >= 0)));
            catInfectReported = TRUE;
            return;
        }
        catInfectTarget = target;
        catInfectOldType = type;
        catInfectNewType = target->staticinfo->beastAttackConvertTo;
        catInfectOwner = universe.curPlayerPtr;
        newStatic = GetShipStaticInfo(catInfectNewType, RACE_Beast);
        cat_beastInfectShip(target, infector);
        dbgMessagef("infecttest: infected tick=%u ship=%s#%u by %s#%u (player %d) convertTo=%s loaded=%d convertAt=%.2f now=%.2f",
                    (unsigned)universe.univUpdateCounter, ShipTypeToStr(type), (unsigned)target->shipID.shipNumber,
                    ShipTypeToStr(infector->shiptype), (unsigned)infector->shipID.shipNumber, (int)infector->playerowner->playerIndex,
                    ShipTypeToStr(catInfectNewType), (int)(bitTest(newStatic->common.infoFlags, IF_InfoLoaded) ? 1 : 0),
                    target->catBeastConvertTime, universe.totaltimeelapsed);
        return;
    }
    if ((catInfectTarget != NULL) && !catInfectReported)
    {
        bool32 alive = FALSE;

        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            if ((Ship *)listGetStructOfNode(node) == catInfectTarget)
            {
                alive = TRUE;
                break;
            }
        }
        if (!alive)
        {
            catInfectReported = TRUE;
            catInfectTarget = NULL;
            dbgMessagef("infecttest: converted tick=%u old=%s mine=%d new=%s mine=%d beast=%d",
                        (unsigned)universe.univUpdateCounter, ShipTypeToStr(catInfectOldType),
                        (int)catInfectCountType(catInfectOwner, catInfectOldType), ShipTypeToStr(catInfectNewType),
                        (int)catInfectCountType(catInfectOwner, catInfectNewType), (int)catInfectCountType(NULL, catInfectNewType));
        }
    }
}
static sdword catLoadGameObjectives = 0;
static bool32 catLoadGameTraining = FALSE;      // M2-R9 missions-10-17: a tutorial lesson's save (SavedGames/Training)
extern char TutorialSavedGamesPath[];           // GamePick.c

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

void catLoadGameSelfTestTrainingSet(void)
{
    catLoadGameTraining = TRUE;
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

    snprintf(filename, sizeof(filename), "%s%s", catLoadGameTraining ? TutorialSavedGamesPath : SinglePlayerSavedGamesPath,
             catLoadGameName);
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
    catTeamColourLoadedGame = TRUE;                 // HWI-TEST (M3-R1 team-colours-warp): the save brings its colours
    gpLoadTutorial = catLoadGameTraining;           // the tutorial's load: tutorial 1 (gpLoadGame's, GamePick.c)
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
    dbgMessagef("loadgametest: loaded singlePlayer=%d tutorial=%d mission=%d tick=%u ships=%u player0ships=%d objectives=%d kas='%s' "
                "watch=%d level='%s' hyperspaceState=%d difficulty=%u", (int)singlePlayerGame, (int)tutorial,
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
    catInfectTick((sdword)universe.univUpdateCounter - catLoadGameStartTick);     // HWI-TEST (M3-R4 infect-convert)
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
                      (CurrentMissionWatchFunction != NULL) && (catMtTeamsSeen > 0) &&
                      (!catLoadGameTraining || ((tutorial == 1) && (CurrentMissionWatchFunction == Watch_Tutorial1)));

        catLoadGameDone = TRUE;
        catTeamColourDone();                        // HWI-TEST (M3-R1 team-colours-warp)
        dbgMessagef("loadgametest: done ticks=%d from=%d to=%u mission=%d tutorial=%d ships=%u objectives=%d/%d kasTeams=%d "
                    "result=%s", (int)catLoadGameTicks, (int)catLoadGameStartTick, (unsigned)universe.univUpdateCounter,
                    (int)singlePlayerGameInfo.currentMission, (int)tutorial, (unsigned)universe.ShipList.num,
                    (int)catLoadGameObjectives, (int)objectivesUsed, (int)catMtTeamsSeen, pass ? "PASS" : "FAIL");
        oracleMissionStopAfterThisTick();
    }
}
