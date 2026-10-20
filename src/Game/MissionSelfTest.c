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

static sdword catMissionTestTicks = 0;          // > 0: /catMissionTest given
static sdword catMissionTestTutorialTicks = 0;  // > 0: /catMissionTestTutorial given
static bool32 catMissionTestDone = FALSE;
static bool32 catMissionTestInit = FALSE;
// the flow events the single-player code reports (catMissionSelfTestEvent)
static bool32 catMtNisStarted = FALSE, catMtNisEnded = FALSE, catMtLevelLoaded = FALSE, catMtKasStarted = FALSE;
static bool32 catMtNisletEnded = FALSE, catMtArrived = FALSE;

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
    var = cat_kasVarFind(0x098ddc43, 0x083f97b5);       // the Objectives FSM's counter (Watch_Mission01_Objectives)
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
typedef enum { CAT_MT_DESTROY, CAT_MT_EXPLORE, CAT_MT_HYPERSPACE } CatMtKind;
typedef struct
{
    const char *label;
    CatMtKind kind;
    udword teams[3];
    sdword numTeams;
} CatMtObjective;

#define CAT_MT_OBJECTIVES 6
static const CatMtObjective catMtObjectives[CAT_MT_OBJECTIVES] =
{
    { "DestroyBombers",       CAT_MT_DESTROY,    { 0x04fcebd3 }, 1 },                           // TaiidanBombers
    { "ExploreRegion",        CAT_MT_EXPLORE,    { 0x0cbd4dc3 }, 1 },                           // TaiidanResourcers
    { "DestroyResourcers",    CAT_MT_DESTROY,    { 0x0cbd4dc3 }, 1 },
    { "DestroyProxAttackers", CAT_MT_DESTROY,    { 0x0b25c8d1, 0x06a0d9b1, 0x0ebc0bc1 }, 3 },  // the TaiidanProxAttack teams
    { "DestroyDestroyer",     CAT_MT_DESTROY,    { 0x0518e0d2 }, 1 },                           // TaiidanDestroyer
    { "Hyperspace",           CAT_MT_HYPERSPACE, { 0 }, 0 },
};
static sdword catMtCreatedTick[CAT_MT_OBJECTIVES];
static sdword catMtDoneTick[CAT_MT_OBJECTIVES];
static sdword catMtCreatedOrder[CAT_MT_OBJECTIVES];
static sdword catMtDoneOrder[CAT_MT_OBJECTIVES];
static sdword catMtCreatedCount = 0, catMtDoneCount = 0;
static udword catMtReconNumber = 0xffffffff;
static bool32 catMtHyperspaced = FALSE;
static bool32 catMtFailed = FALSE;
static sdword catMtEndTick = -1;
static sdword catMtLastDamaged[CAT_MT_OBJECTIVES][3];
static sdword catMtHyperspaceTick = -1;


static sdword catMtObjectiveIndex(const char *label)
{
    sdword i;

    for (i = 0; i < CAT_MT_OBJECTIVES; i++)
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

static void catMtPlay(void)
{
    sdword i;

    for (i = 0; i < CAT_MT_OBJECTIVES; i++)
    {
        bool32 stands = catMtObjectiveStands(i);

        if (stands && (catMtCreatedTick[i] < 0))
        {
            catMtCreatedTick[i] = (sdword)universe.univUpdateCounter;
            catMtCreatedOrder[catMtCreatedCount++] = i;
            dbgMessagef("missiontest: %s tick=%u created", catMtObjectives[i].label, (unsigned)universe.univUpdateCounter);
            if (catMtObjectives[i].kind == CAT_MT_HYPERSPACE)
            {
                catMtEndTick = (sdword)universe.univUpdateCounter + 64;     // the end-of-mission save, then the jump
            }
        }
        else if (!stands && (catMtCreatedTick[i] >= 0) && (catMtDoneTick[i] < 0) && (catMtObjectives[i].kind != CAT_MT_HYPERSPACE))
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
    The tutorial's first screen (/catMissionTestTutorial): Tutorial1's KAS script starts (tutInitTutorial), its
    Tutorial FSM reaches Lesson1 (state hash 0x00a883a1) and creates the lesson's two objectives (L1Dummy1, L1Dummy2), and
    the first prompt shows: the lesson's narration's subtitle card on screen (the speech of the SpeechEvents FSM; this
    test keeps speech on, on SDL's dummy audio driver)
-----------------------------------------------------------------------------*/
static sdword catMtTutLessonTick = -1, catMtTutObjTick = -1, catMtTutPromptTick = -1;

static void catMtTutorialWatch(void)
{
    sdword i, r, found = 0;
    AITeam *team = (universe.players[1].aiPlayer != NULL) ? kasAITeamPtr(0x0a9468cc) : NULL;    // team "Tutorial"

    if ((catMtTutLessonTick < 0) && (team != NULL) && (team->kasStateName == 0x00a883a1))
    {
        catMtTutLessonTick = (sdword)universe.univUpdateCounter;
        dbgMessagef("missiontest: tutorial Lesson1 tick=%u", (unsigned)universe.univUpdateCounter);
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
    if ((catMtTutPromptTick < 0) && subAnyCardsOnScreen())
    {
        for (r = 0; r < SUB_NumberRegions; r++)
        {
            if ((subRegion[r].cardIndex > 0) && (subRegion[r].card != NULL) && (subRegion[r].card[0].text != NULL))
            {
                catMtTutPromptTick = (sdword)universe.univUpdateCounter;
                dbgMessagef("missiontest: tutorial prompt tick=%u region=%d text='%.80s'", (unsigned)universe.univUpdateCounter,
                            (int)r, subRegion[r].card[0].text);
                break;
            }
        }
    }
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
    if (!catMissionTestInit)
    {
        sdword i;
        catMissionTestInit = TRUE;
        for (i = 0; i < CAT_MT_OBJECTIVES; i++)
        {
            catMtCreatedTick[i] = catMtDoneTick[i] = -1;
        }
    }
    catMtWatchFsms();
    catMtWatchObjectives();
    if (catMissionTestTutorialTicks > 0)
    {
        catMtTutorialWatch();
        if ((catMtTutPromptTick >= 0) && (catMtTutObjTick >= 0) && (catMtTutLessonTick >= 0))
        {
            catMissionTestDone = TRUE;
            dbgMessagef("missiontest: done ticks=%u tutorial kas=%d lesson1=%d objectives=%d prompt=%d result=PASS",
                        (unsigned)universe.univUpdateCounter, (int)catMtKasStarted, (int)catMtTutLessonTick,
                        (int)catMtTutObjTick, (int)catMtTutPromptTick);
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
        ((catMissionTestTicks > 0) && (catMtHyperspaceTick >= 0) && ((sdword)universe.univUpdateCounter >= catMtHyperspaceTick + 32)))
    {
        sdword i, reached = 0;
        bool32 inOrder = TRUE;

        catMissionTestDone = TRUE;
        if (catMissionTestTicks > 0)
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
            for (i = 0; i < CAT_MT_OBJECTIVES; i++)
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
            dbgMessagef("missiontest: done ticks=%u objectives=%d/%d result=%s", (unsigned)universe.univUpdateCounter,
                        (int)reached, CAT_MT_OBJECTIVES,
                        ((reached == CAT_MT_OBJECTIVES) && inOrder && !catMtFailed && (catMtCounter == 6) &&
                         catMtNisStarted && catMtNisEnded && catMtLevelLoaded && catMtKasStarted && catMtNisletEnded &&
                         catMtArrived && (catMtHyperspaceTick >= 0)) ? "PASS" : "FAIL");
            oracleMissionStopAfterThisTick();   // the mission run's own quit, with its stream complete
        }
        else
        {
            dbgMessagef("missiontest: done ticks=%u tutorial kas=%d lesson1=%d objectives=%d prompt=%d result=FAIL",
                        (unsigned)universe.univUpdateCounter, (int)catMtKasStarted, (int)catMtTutLessonTick,
                        (int)catMtTutObjTick, (int)catMtTutPromptTick);
        }
    }
}
