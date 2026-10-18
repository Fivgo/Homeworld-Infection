/*=============================================================================
    Name    : AISelfTest.c
    Purpose : HWI-TEST (M2-R7 ai-orders-moves), port diagnostic, not in the exe:
              /catAiTest <ticks> (harness mode ai). It runs the headless
              Mission01 start of /oracleMission (virtual clock, no animatic, the
              oracle stream written with /oracleDump, quits after <ticks>) and
              lets the computer player (player 1, AI_ADVANCED in single player)
              run its fleet with no input:
                tick 16  player 1 gets a scripted factory (its race's mothership,
                         made its PlayerMothership) at the grid point farthest
                         from the level's ships, 30,000 RUs and a scripted fleet
                         of its race around it, handed to the AI as new ships
                         (AIPlayer.newships, where aiplayerShipLaunchedCallback
                         puts a launched ship); fleet control is turned on as the
                         missions' KAS call does (singlePlayerGameInfo.
                         giveComputerFleetControl, kasfGiveComputerFleetControl)
                every tick the checks of catAiTestChecks run on the AI's own
                         structures (AIPlayer team arrays, each team's move list,
                         its current move, its ships' command-layer orders); the
                         first tick a check holds is logged ("aitest: <check>
                         tick=N ...")
                every 256 an "aitest: status" line (teams, moves, ships)
              "aitest: done ... result=PASS|FAIL" once every check has held (or
              at ticks-2), then the mission run's own quit. The checks are one
              table so that later AI ports (build / research / harvest, M2-R7
              producer 3) add rows rather than code paths.

    Created 2026-10-18 for Homeworld-Infection (M2-R7 ai-orders-moves).
    This tree is REULA-licensed, see src/COPYING.REULA.
=============================================================================*/

#include "AISelfTest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AIMoves.h"
#include "AIPlayer.h"
#include "AITeam.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "Debug.h"
#include "LevelLoad.h"
#include "ObjTypes.h"
#include "OracleDump.h"
#include "ShipSelect.h"
#include "SinglePlayer.h"
#include "StatScript.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "Vector.h"

#define CAT_AITEST_ORDER_TICK       16
#define CAT_AITEST_RUS              30000
#define CAT_AITEST_SPACING          400.0f

static sdword catAiTestTicks = 0;
static bool32 catAiTestOrdered = FALSE;
static bool32 catAiTestDone = FALSE;
static Ship *catAiTestFactory = NULL;
static sdword catAiTestFleetShips = 0;
static sdword catAiTestSlot = 0;
static vector catAiTestBase;

/*-----------------------------------------------------------------------------
    The scripted fleet, per race of player 1, large enough that the get-ships
    moves of the teams the managers form in their first update are served
    from the reserves (aimProcessFancyGetShips): strike craft for the attack
    orders, corvettes and frigates for the defence orders, the resourcers and
    the Processor that make the defense manager's roving teams worth having
    (aidCountDefendableShips), recon ships for the recon orders. The ships the
    AI asks to build are not delivered: building is the fleet manager's
    (aifProcessShipBuildRequests, M2-R7 producer 3)
-----------------------------------------------------------------------------*/
typedef struct
{
    ShipType type;
    sdword count;
} CatAiTestFleetRow;

static const CatAiTestFleetRow catAiTestFleetSect[] =
{
    { sAcolyte, 60 }, { sACV, 25 }, { sMimic, 10 }, { sRecon, 6 }, { sLeech, 6 }, { sMCV, 15 },
    { sMultiBeamFrigate, 15 }, { sHiveFrigate, 10 }, { sWorker, 2 }, { sProcessor, 1 }
};
static const CatAiTestFleetRow catAiTestFleetBeast[] =
{
    { bInterceptor, 60 }, { bAttackBomber, 20 }, { bCloakedFighter, 6 }, { bRecon, 6 }, { bHeavyCorvette, 25 },
    { bMultiGunCorvette, 15 }, { bIonArrayFrigate, 15 }, { bMissileCorvette, 6 }, { bWorker, 2 }, { bProcessor, 1 }
};

/*-----------------------------------------------------------------------------
    The checks (one table; later AI ports add rows)
-----------------------------------------------------------------------------*/
typedef bool32 (*CatAiTestCheckFn)(AIPlayer *ai, char *detail, size_t size);

typedef struct
{
    const char *name;
    CatAiTestCheckFn fn;
    sdword firstTick;           // -1 until the check first holds
    char detail[200];
} CatAiTestCheck;

// the attack, defence and recon groups of the attack and defense managers (AIPlayer.attackTeam[] / harassTeam,
// guardTeams[], reconTeam[])
typedef enum { CAT_AITEST_ATTACK, CAT_AITEST_DEFENCE, CAT_AITEST_RECON, CAT_AITEST_GROUPS } CatAiTestGroup;
static const char *catAiTestGroupName[CAT_AITEST_GROUPS] = { "attack", "defence", "recon" };

static sdword catAiTestGroupTeams(AIPlayer *ai, CatAiTestGroup group, AITeam **teams, sdword max)
{
    sdword n = 0, i;

    switch (group)
    {
        case CAT_AITEST_ATTACK:
            for (i = 0; (i < AIPLAYER_NUM_ATTACKTEAMS) && (n < max); i++)
            {
                if (ai->attackTeam[i] != NULL)
                {
                    teams[n++] = ai->attackTeam[i];
                }
            }
            if ((ai->harassTeam != NULL) && (n < max))
            {
                teams[n++] = ai->harassTeam;
            }
            break;
        case CAT_AITEST_DEFENCE:
            for (i = 0; (i < ai->numGuardTeams) && (n < max); i++)
            {
                if (ai->guardTeams[i] != NULL)
                {
                    teams[n++] = ai->guardTeams[i];
                }
            }
            break;
        case CAT_AITEST_RECON:
            for (i = 0; (i < AIPLAYER_NUM_RECONTEAMS) && (n < max); i++)
            {
                if (ai->reconTeam[i] != NULL)
                {
                    teams[n++] = ai->reconTeam[i];
                }
            }
            break;
        default:
            break;
    }
    return n;
}

// the move types that make a team's order an attack, a defence or a recon order (the moves aioCreate* adds after the
// get-ships move)
static bool32 catAiTestMoveIsOfGroup(AIMoveTypes type, CatAiTestGroup group)
{
    switch (group)
    {
        case CAT_AITEST_ATTACK:
            return (type == MOVE_TEMPGUARD) || (type == MOVE_ARMADA) || (type == MOVE_ATTACK) ||
                   (type == MOVE_ADVANCEDATTACK) || (type == MOVE_FLANKATTACK) || (type == MOVE_MOVEATTACK) ||
                   (type == MOVE_HARASSATTACK) || (type == MOVE_KAMIKAZE) || (type == MOVE_SPECIALDEFENSE) ||
                   (type == MOVE_ACTIVECAPTURE) || (type == MOVE_ACTIVEMINE);
        case CAT_AITEST_DEFENCE:
            return (type == MOVE_ACTIVEPATROL) || (type == MOVE_PATROLMOVE) || (type == MOVE_GUARDSHIPS) ||
                   (type == MOVE_DEFMOSHIP);
        case CAT_AITEST_RECON:
            return (type == MOVE_ACTIVERECON) || (type == MOVE_SHIPRECON);
        default:
            return FALSE;
    }
}

static bool32 catAiTestMoveIsGetShips(AIMoveTypes type)
{
    return (type == MOVE_GETSHIPS) || (type == MOVE_FANCYGETSHIPS);
}

// a team of the group whose move list holds a move of the group: the order was given (aioCreate*)
static bool32 catAiTestGroupOrdered(AIPlayer *ai, CatAiTestGroup group, char *detail, size_t size)
{
    AITeam *teams[64];
    sdword n = catAiTestGroupTeams(ai, group, teams, 64), i;
    Node *node;
    AITeamMove *move;

    for (i = 0; i < n; i++)
    {
        for (node = teams[i]->moves.head; node != NULL; node = node->next)
        {
            move = (AITeamMove *)listGetStructOfNode(node);
            if (catAiTestMoveIsOfGroup(move->type, group))
            {
                snprintf(detail, size, "teams=%d team=%d move=%d moves=%d", (int)n, (int)i, (int)move->type,
                         (int)teams[i]->moves.num);
                return TRUE;
            }
        }
    }
    return FALSE;
}

// a team of the group with ships whose current move is past its get-ships move and being processed: the move was
// issued to the team's ships
static bool32 catAiTestGroupIssued(AIPlayer *ai, CatAiTestGroup group, char *detail, size_t size)
{
    AITeam *teams[64];
    sdword n = catAiTestGroupTeams(ai, group, teams, 64), i;
    AITeamMove *move;

    for (i = 0; i < n; i++)
    {
        move = teams[i]->curMove;
        if ((teams[i]->shipList.selection->numShips > 0) && (move != NULL) && move->processing &&
            !catAiTestMoveIsGetShips(move->type) && (move->type != MOVE_DONE))
        {
            snprintf(detail, size, "team=%d ships=%d curmove=%d", (int)i, (int)teams[i]->shipList.selection->numShips,
                     (int)move->type);
            return TRUE;
        }
    }
    return FALSE;
}

// a ship of a team of the group carries a command-layer order (the move went through the aiuWrap* commands)
static bool32 catAiTestGroupCommanded(AIPlayer *ai, CatAiTestGroup group, char *detail, size_t size)
{
    AITeam *teams[64];
    sdword n = catAiTestGroupTeams(ai, group, teams, 64), i, j;
    Ship *ship;

    for (i = 0; i < n; i++)
    {
        for (j = 0; j < teams[i]->shipList.selection->numShips; j++)
        {
            ship = teams[i]->shipList.selection->ShipPtr[j];
            if ((ship->command != NULL) && (ship->command->ordertype.order != COMMAND_NULL))
            {
                snprintf(detail, size, "team=%d ship=%s#%u order=%d curmove=%d", (int)i, ShipTypeToStr(ship->shiptype),
                         (unsigned)ship->shipID.shipNumber, (int)ship->command->ordertype.order,
                         (teams[i]->curMove != NULL) ? (int)teams[i]->curMove->type : -1);
                return TRUE;
            }
        }
    }
    return FALSE;
}

static bool32 catAiTestTeams(AIPlayer *ai, char *detail, size_t size)
{
    snprintf(detail, size, "teamsUsed=%d attack=%d guard=%d recon=%d", (int)ai->teamsUsed, (int)ai->numAttackTeams,
             (int)ai->numGuardTeams, (int)ai->numReconTeams);
    return (ai->numAttackTeams > 0) && (ai->numGuardTeams > 0) && (ai->numReconTeams > 0);
}

static bool32 catAiTestAttackOrdered(AIPlayer *ai, char *d, size_t s)  { return catAiTestGroupOrdered(ai, CAT_AITEST_ATTACK, d, s); }
static bool32 catAiTestDefenceOrdered(AIPlayer *ai, char *d, size_t s) { return catAiTestGroupOrdered(ai, CAT_AITEST_DEFENCE, d, s); }
static bool32 catAiTestReconOrdered(AIPlayer *ai, char *d, size_t s)   { return catAiTestGroupOrdered(ai, CAT_AITEST_RECON, d, s); }
static bool32 catAiTestAttackIssued(AIPlayer *ai, char *d, size_t s)   { return catAiTestGroupIssued(ai, CAT_AITEST_ATTACK, d, s); }
static bool32 catAiTestDefenceIssued(AIPlayer *ai, char *d, size_t s)  { return catAiTestGroupIssued(ai, CAT_AITEST_DEFENCE, d, s); }
static bool32 catAiTestReconIssued(AIPlayer *ai, char *d, size_t s)    { return catAiTestGroupIssued(ai, CAT_AITEST_RECON, d, s); }
static bool32 catAiTestAttackCommanded(AIPlayer *ai, char *d, size_t s)  { return catAiTestGroupCommanded(ai, CAT_AITEST_ATTACK, d, s); }
static bool32 catAiTestDefenceCommanded(AIPlayer *ai, char *d, size_t s) { return catAiTestGroupCommanded(ai, CAT_AITEST_DEFENCE, d, s); }
static bool32 catAiTestReconCommanded(AIPlayer *ai, char *d, size_t s)   { return catAiTestGroupCommanded(ai, CAT_AITEST_RECON, d, s); }

static CatAiTestCheck catAiTestChecks[] =
{
    { "teams",            catAiTestTeams,             -1, "" },
    { "attackorder",      catAiTestAttackOrdered,     -1, "" },
    { "defenceorder",     catAiTestDefenceOrdered,    -1, "" },
    { "reconorder",       catAiTestReconOrdered,      -1, "" },
    { "attackmove",       catAiTestAttackIssued,      -1, "" },
    { "defencemove",      catAiTestDefenceIssued,     -1, "" },
    { "reconmove",        catAiTestReconIssued,       -1, "" },
    { "attackcommand",    catAiTestAttackCommanded,   -1, "" },
    { "defencecommand",   catAiTestDefenceCommanded,  -1, "" },
    { "reconcommand",     catAiTestReconCommanded,    -1, "" },
    // M2-R7 producer 3 adds its rows here (build, research, harvest)
};
#define CAT_AITEST_NUM_CHECKS   ((sdword)(sizeof(catAiTestChecks) / sizeof(catAiTestChecks[0])))

/*-----------------------------------------------------------------------------
    Setup
-----------------------------------------------------------------------------*/
// a type the level excluded: its static is loaded as universeStaticInit would have (the test's own action, as the
//      build self-test's catBuildSelfTestLoadStatic, here for either race)
static void catAiTestLoadStatic(ShipType type)
{
    ShipRace race = (ShipRace)cat_ShipTypeToRace((udword)type);
    ShipStaticInfo *info = GetShipStaticInfo(type, race);

    if (bitTest(info->common.infoFlags, IF_InfoLoaded))
    {
        return;
    }
    SetInfoNeededForShipAndRelatedStaticInfo(type, race, TRUE);
    InitStatShipInfo(info, type, race);
    InitStatShipInfoPost(info);
    bitSet(info->common.infoFlags, IF_InfoLoaded);
    cat_univSetShipMaxHealth(info);
    dbgMessagef("aitest: loaded the static of %s (the level excludes it)", ShipTypeToStr(type));
}

// the grid point (step 10,000) inside 80% of the world sphere farthest from every ship the level put down (the build
//      self-test's choice)
static vector catAiTestFarPoint(void)
{
    vector base = { 0.0f, 0.0f, 0.0f }, cand, diff;
    real32 limit = universe.radius * 0.8f, bestdist = -1.0f, nearest;
    sdword gx, gy, gz;
    Node *node;
    Ship *ship;

    for (gx = -10; gx <= 10; gx++)
    {
        for (gy = -10; gy <= 10; gy++)
        {
            for (gz = -3; gz <= 3; gz++)
            {
                cand.x = gx * 10000.0f; cand.y = gy * 10000.0f; cand.z = gz * 10000.0f;
                if (vecMagnitudeSquared(cand) > limit * limit)
                {
                    continue;
                }
                nearest = REALlyBig;
                for (node = universe.ShipList.head; node != NULL; node = node->next)
                {
                    ship = (Ship *)listGetStructOfNode(node);
                    vecSub(diff, ship->obj.posinfo.position, cand);
                    if (vecMagnitudeSquared(diff) < nearest)
                    {
                        nearest = vecMagnitudeSquared(diff);
                    }
                }
                if (nearest > bestdist)
                {
                    bestdist = nearest;
                    base = cand;
                }
            }
        }
    }
    return base;
}

static void catAiTestSetup(sdword tick)
{
    Player *player = &universe.players[1];
    AIPlayer *ai = player->aiPlayer;
    const CatAiTestFleetRow *fleet;
    sdword rows, r, k, before = player->totalships;
    ShipType factoryType;
    vector base, pos;
    Ship *ship;

    catAiTestOrdered = TRUE;
    if (player->race == RACE_Beast)
    {
        fleet = catAiTestFleetBeast;
        rows = (sdword)(sizeof(catAiTestFleetBeast) / sizeof(catAiTestFleetBeast[0]));
        factoryType = bMothership;
    }
    else
    {
        fleet = catAiTestFleetSect;
        rows = (sdword)(sizeof(catAiTestFleetSect) / sizeof(catAiTestFleetSect[0]));
        factoryType = sMothership;
    }

    base = catAiTestFarPoint();
    catAiTestBase = base;
    catAiTestLoadStatic(factoryType);
    pos = base;
    catAiTestFactory = univAddShip(factoryType, (ShipRace)cat_ShipTypeToRace((udword)factoryType), &pos, player, 0);
    player->PlayerMothership = catAiTestFactory;
    player->resourceUnits = CAT_AITEST_RUS;

    for (r = 0; r < rows; r++)
    {
        catAiTestLoadStatic(fleet[r].type);
        for (k = 0; k < fleet[r].count; k++)
        {
            pos.x = base.x + (real32)((catAiTestSlot % 8) - 4) * CAT_AITEST_SPACING;
            pos.y = base.y + 3000.0f + (real32)(catAiTestSlot / 8) * CAT_AITEST_SPACING;
            pos.z = base.z;
            catAiTestSlot++;
            ship = univAddShip(fleet[r].type, (ShipRace)cat_ShipTypeToRace((udword)fleet[r].type), &pos, player, 0);
            if ((ship != NULL) && !ShipInSelection(ai->newships.selection, ship))
            {
                growSelectAddShip(&ai->newships, ship);
                catAiTestFleetShips++;
            }
        }
    }

    // the human player's mothership: Mission01 under CAT_HW1_MISSION_SCRIPTS 0 gives player 0 none (the kas2c script
    // would bring the Kuun-Lan), and the AI aims its fleet orders, its patrol paths (aiuGeneratePatrolPath) and its
    // sphere of influence (aidCheckSphereOfInfluence) at the enemy mothership: one is put 25,000 from the factory
    if (universe.players[0].PlayerMothership == NULL)
    {
        ShipType enemyType = (universe.players[0].race == RACE_Beast) ? bMothership : sMothership;

        catAiTestLoadStatic(enemyType);
        pos = base;
        pos.y += 25000.0f;
        universe.players[0].PlayerMothership = univAddShip(enemyType, (ShipRace)cat_ShipTypeToRace((udword)enemyType), &pos,
                                                           &universe.players[0], 0);
        dbgMessagef("aitest: enemy mothership tick=%d %s#%u at (%.0f %.0f %.0f)", (int)tick, ShipTypeToStr(enemyType),
                    (unsigned)universe.players[0].PlayerMothership->shipID.shipNumber, pos.x, pos.y, pos.z);
    }

    // as kasfGiveComputerFleetControl: the computer player's managers run (aifFleetCommand)
    singlePlayerGameInfo.giveComputerFleetControl = TRUE;

    dbgMessagef("aitest: orders tick=%d player=1 race=%d level=%d updaterate=%u features=%x/%x/%x factory=%s#%u at (%.0f %.0f %.0f) "
                "RUs=%d fleet=%d shipsbefore=%d enemy=%d enemymothership=%d",
                (int)tick, (int)player->race, (int)ai->aiplayerDifficultyLevel, (unsigned)ai->aiplayerUpdateRate,
                (unsigned)ai->AttackFeatures, (unsigned)ai->DefenseFeatures, (unsigned)ai->ResourceFeatures,
                ShipTypeToStr(factoryType), (unsigned)catAiTestFactory->shipID.shipNumber, base.x, base.y, base.z,
                (int)player->resourceUnits, (int)catAiTestFleetShips, (int)before,
                (ai->primaryEnemyPlayer != NULL) ? (int)ai->primaryEnemyPlayer->playerIndex : -1,
                (ai->primaryEnemyPlayer != NULL) && (ai->primaryEnemyPlayer->PlayerMothership != NULL));
}

/*-----------------------------------------------------------------------------
    Engine interface
-----------------------------------------------------------------------------*/
void catAiSelfTestSet(char *string)
{
    catAiTestTicks = (sdword)strtol(string, NULL, 10);
    if (catAiTestTicks <= 0)
    {
        catAiTestTicks = 400;
    }
    oracleSetMission(string);                               // the headless Mission01 run with the virtual clock and the quit
}

bool32 catAiSelfTestRequested(void)
{
    return (catAiTestTicks > 0);
}

static void catAiTestStatus(sdword tick, AIPlayer *ai)
{
    sdword g, i, n, ships;
    AITeam *teams[64];
    char line[512];
    size_t len;

    len = (size_t)snprintf(line, sizeof(line), "aitest: status tick=%d RUs=%d teamsUsed=%d newships=%d", (int)tick,
                           (int)ai->player->resourceUnits, (int)ai->teamsUsed, (int)ai->newships.selection->numShips);
    for (g = 0; g < CAT_AITEST_GROUPS; g++)
    {
        n = catAiTestGroupTeams(ai, (CatAiTestGroup)g, teams, 64);
        ships = 0;
        for (i = 0; i < n; i++)
        {
            ships += teams[i]->shipList.selection->numShips;
        }
        len += (size_t)snprintf(line + len, sizeof(line) - len, " %s=%d/%d", catAiTestGroupName[g], (int)n, (int)ships);
        for (i = 0; (i < n) && (i < 4) && (len < sizeof(line) - 40); i++)
        {
            AITeamMove *move = teams[i]->curMove;
            len += (size_t)snprintf(line + len, sizeof(line) - len, "%s%d", (i == 0) ? "[" : ",", (move != NULL) ? (int)move->type : -1);
            if ((move != NULL) && catAiTestMoveIsGetShips(move->type))
            {
                // a team waiting for ships: the type and number its get-ships move asked for
                len += (size_t)snprintf(line + len, sizeof(line) - len, ":%s*%d", ShipTypeToStr(move->params.fancyGetShips.shipType),
                                        (int)move->params.fancyGetShips.numShips);
            }
        }
        if ((n > 0) && (len < sizeof(line) - 2))
        {
            len += (size_t)snprintf(line + len, sizeof(line) - len, "]");
        }
    }
    dbgMessagef("%s", line);
}

static void catAiTestFinish(sdword tick)
{
    sdword c, passed = 0;
    char line[512];
    size_t len;

    catAiTestDone = TRUE;
    len = (size_t)snprintf(line, sizeof(line), "aitest: done tick=%d fleet=%d", (int)tick, (int)catAiTestFleetShips);
    for (c = 0; c < CAT_AITEST_NUM_CHECKS; c++)
    {
        if (catAiTestChecks[c].firstTick >= 0)
        {
            passed++;
        }
        else
        {
            dbgMessagef("aitest: FAIL %s never held", catAiTestChecks[c].name);
        }
        len += (size_t)snprintf(line + len, sizeof(line) - len, " %s=%d", catAiTestChecks[c].name,
                                (int)catAiTestChecks[c].firstTick);
    }
    snprintf(line + len, sizeof(line) - len, " checks=%d/%d result=%s", (int)passed, (int)CAT_AITEST_NUM_CHECKS,
             (passed == CAT_AITEST_NUM_CHECKS) ? "PASS" : "FAIL");
    dbgMessagef("%s", line);
}

void catAiSelfTestTick(void)
{
    sdword tick = (sdword)universe.univUpdateCounter;
    Player *player = &universe.players[1];
    AIPlayer *ai = player->aiPlayer;
    sdword c, held = 0;

    if (catAiTestDone)
    {
        return;
    }
    if ((universe.numPlayers < 2) || (ai == NULL))
    {
        catAiTestDone = TRUE;
        dbgMessagef("aitest: done tick=%d players=%d aiplayer=%d result=FAIL", (int)tick, (int)universe.numPlayers,
                    (int)(ai != NULL));
        return;
    }
    if (!catAiTestOrdered)
    {
        if (tick >= CAT_AITEST_ORDER_TICK)
        {
            catAiTestSetup(tick);
        }
        return;
    }

    for (c = 0; c < CAT_AITEST_NUM_CHECKS; c++)
    {
        if (catAiTestChecks[c].firstTick < 0)
        {
            if (catAiTestChecks[c].fn(ai, catAiTestChecks[c].detail, sizeof(catAiTestChecks[c].detail)))
            {
                catAiTestChecks[c].firstTick = tick;
                dbgMessagef("aitest: %s tick=%d %s", catAiTestChecks[c].name, (int)tick, catAiTestChecks[c].detail);
            }
        }
        if (catAiTestChecks[c].firstTick >= 0)
        {
            held++;
        }
    }

    if ((tick % 256) == 0)
    {
        catAiTestStatus(tick, ai);
    }
    if ((held == CAT_AITEST_NUM_CHECKS) || (tick >= catAiTestTicks - 2))
    {
        catAiTestStatus(tick, ai);
        catAiTestFinish(tick);
    }
}
