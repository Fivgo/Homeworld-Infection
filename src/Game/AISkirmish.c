// =============================================================================
//  AISkirmish.c
//  - Cataclysm's skirmish AI: the computer player of a multiplayer game sorts
//    every ship into 34 teams, builds, researches, harvests and attacks from
//    them (cataclysm.exe 0x0042fa80-0x00434135)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R7 ai-player-skirmish). Cataclysm runs this in place of HW1's managers for every computer player of
//      a multiplayer game (aiplayerUpdateAll 0x00411240 calls cat_aisPlay when singlePlayerGame is 0; single player
//      keeps aifFleetCommand). The exe links the 19 functions between Captaincy.c (TransferCaptaincyStateMachine) and
//      the chat code (cCreateChannel 0x00434140), so they came from a source file of their own; CET reconstructed its
//      structures (AISkirmish.h) and aisFleetUpdate. Each body below follows the disassembly of its exe function
//      (address on each); the r6 decompile lost the per-player enemy filter of cat_aisUpdateTeams, the second
//      selection of cat_aisWorkersUpdate and the arguments of every clWrapMove call. The x87 code rounds each sum
//      and product to float at its store, so float arithmetic here reproduces it; the few comparisons the exe makes on
//      an unrounded 53-bit value are made in double here (said at each).

#include "AISkirmish.h"

#include "AIPlayer.h"
#include "Alliance.h"
#include "Bounties.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "CommandWrap.h"
#include "MultiplayerGame.h"
#include "ObjTypes.h"
#include "Randy.h"
#include "ResearchAPI.h"
#include "Salvage.h"
#include "Sensors.h"
#include "ShipSelect.h"
#include "Universe.h"
#include "Mothership.h"

/*=============================================================================
    Data
=============================================================================*/

AISTeamEntry aisTeams[AIS_NUM_TEAMS];                       // 0x008dfd60
real32 aisLastPlayTime[MAX_MULTIPLAYER_PLAYERS];            // 0x008dfc44
sdword aisAlliancesSet = 0;                                 // 0x00a2d9b8
udword aiBuildingShip[TOTAL_STD_SHIPS];                     // 0x008dfc64
sdword aiSupportUnitsPending = 0;                           // 0x00a2d9b4
Ship *aiCarrier1Ship = NULL;                                // 0x008dfc38
Ship *aiCarrier2Ship = NULL;                                // 0x008dfc3c
bool32 aiHasExternalConstruction = FALSE;                   // 0x00a2d984
bool32 aiHasCarrier1ModuleQueued = FALSE;                   // 0x00a2d98c
bool32 aiHasSupportModuleQueued = FALSE;                    // 0x00a2d994
bool32 aiHasCarrier2ModuleQueued = FALSE;                   // 0x00a2d9a8

// the exe's other per-update words (names chosen here; cat_aisUpdateTeams sets them, the others read them)
static bool32 aisShipBeingBuilt = FALSE;                    // 0x00a2d980: an own ship is being built (written, never read)
static Ship *aisEnemyBigGunActiveShip = NULL;               // 0x00a2d9a4: an enemy sMothership with its big gun active (written, never read)
static Ship *aisEnemyBigGunChargedShip = NULL;              // 0x00a2d998: an enemy sMothership with its big gun charged
static Ship *aisCrystalTarget = NULL;                       // 0x00a2d99c: the crystal an own Worker is salvaging
static Ship *aisOwnBigGunShip = NULL;                       // 0x008dfd54: the own sMothership while its big gun is active
static bool32 aisEnemyDFGNearMothership = FALSE;            // 0x008dfd58: an enemy bDFGFrigate within 3500 of the mothership
static Ship *aisLeastBuiltShip = NULL;                      // 0x00a2d9b0: the own ship furthest from complete
static bool32 aisFirstPlay = TRUE;                          // 0x007525e4: .data 1, cleared by the first play (read nowhere else)

// the attack targets of cat_aisPlay's five cat_aisTeamAttack calls, as team indexes (.data 0x00752578..0x007525e0)
// CAT-FIX (M2-R8 oracle-rung-physics, found by the 4,000-tick ai soak): the five tables are one contiguous block, as in
//      the exe's .data, because cat_aisTeamAttack 0x00432eb5 reads targetTeams[numTargetTeams] (one past its table)
//      when the previous selection found no target in any team: the exe then reads the next table's first entry, or,
//      past the last table, the dword at 0x007525e4 (aisFirstPlay); catAisTargetTeam returns the same values
static const sdword aisTargetTables[8 + 6 + 4 + 4 + 5] = {
    5, 25, 9, 32, 7, 3, 18, 20,                                                     // 0x00752578 fighters
    25, 3, 7, 9, 18, 20,                                                            // 0x00752598 anti-fighter
    18, 9, 7, 20,                                                                   // 0x007525b0 medium combat
    9, 32, 18, 20,                                                                  // 0x007525c0 mimics
    5, 9, 32, 18, 20 };                                                             // 0x007525d0 cruise missiles
#define aisFighterTargets           (&aisTargetTables[0])
#define aisAntiFighterTargets       (&aisTargetTables[8])
#define aisMediumTargets            (&aisTargetTables[14])
#define aisMimicTargets             (&aisTargetTables[18])
#define aisCruiseMissileTargets     (&aisTargetTables[22])
#define AIS_TARGET_TABLES_END       (&aisTargetTables[27])

static sdword catAisTargetTeam(const sdword *targetTeams, sdword index)
{
    if (targetTeams + index == AIS_TARGET_TABLES_END)
    {
        return (sdword)aisFirstPlay;                    // CAT-FIX: the exe's dword after the block, 0x007525e4
    }
    dbgAssertOrIgnore(targetTeams + index < AIS_TARGET_TABLES_END);
    return targetTeams[index];
}

// the research order of cat_aisResearchUpdate (.data 0x007524bc..0x00752574): per Sect module, then the Beast list
static const TechnologyType aisResearchDockingBay[7] = { TECH_DRIVE_FIGHTER, TECH_DRIVE_FRIGATE, TECH_DRIVE_DESTROYER,
    TECH_ADRIVE_DESTROYER, TECH_LINK, TECH_ADRIVE_FIGHTER, TECH_ADRIVE_FRIGATE };                    // 0x007524bc
static const TechnologyType aisResearchArmour[5] = { TECH_ARMOUR2, TECH_ARMOUR3, TECH_FORCE_FIELD1, TECH_FORCE_FIELD2,
    TECH_FORCE_FIELD3 };                                                                             // 0x007524d8
static const TechnologyType aisResearchMicro[5] = { TECH_SUPPORT_SYSTEMS, TECH_SALVAGE, TECH_MIMIC, TECH_REPAIR,
    TECH_AFTERBURNER };                                                                              // 0x007524ec
static const TechnologyType aisResearchSpecial[5] = { TECH_ADVANCED_SENSORS, TECH_CRYSTAL_HARVEST, TECH_MICRO_SHIP,
    TECH_REPAIRBOTS, TECH_BEAST_VACCINE };                                                           // 0x00752500
static const TechnologyType aisResearchWeapons[7] = { TECH_ENERGY, TECH_ION, TECH_MISSILE, TECH_BIG_GUN,
    TECH_EXPLOSIVE_CHARGE, TECH_REPULSE, TECH_EMP };                                                 // 0x00752514
static const TechnologyType aisResearchBeast[18] = { TECH_DRIVE_FIGHTER, TECH_SUPPORT_SYSTEMS, TECH_DRIVE_DESTROYER,
    TECH_ADVANCED_SENSORS, TECH_PLASMA_WEAPONS, TECH_DRIVE_CORVETTE, TECH_TARGETING_SYSTEMS, TECH_INFECT,
    TECH_DRIVE_FRIGATE, TECH_DEFENSE_FIELD, TECH_ION, TECH_MEDIUM_GUNS, TECH_SALVAGE, TECH_CRYSTAL_HARVEST,
    TECH_HEAVY_GUNS, TECH_MISSILE, TECH_BIOWARHEAD, TECH_CLOAKING };                                 // 0x00752530

#define AIS_FAR_AWAY            1e21f       // 0x6258d727, the start of every nearest-selection search
#define AIS_JOIN_DISTANCE       5000.0f     // 0x0072a618: a ship joins the nearest group within this
#define AIS_RICH                50000       // 0xc350: Workers stop harvesting, fewer Workers are wanted

/*=============================================================================
    Helpers (inlined in the exe)
=============================================================================*/

// a group's centre after a ship joined: the exe adds the new ship's position once per ship of the group (not each
//      ship's) and divides by the count, each step stored as a float (cat_aisTeamAddShip 0x0042fa80 and the single-group
//      teams of cat_aisUpdateTeams)
static void aisSelectionRecentre(AISTeamSelection *group, Ship *ship)
{
    sdword n = group->sel.numShips, k;

    group->center.x = 0.0f;
    group->center.y = 0.0f;
    group->center.z = 0.0f;
    for (k = 0; k < n; k++)
    {
        group->center.x += ship->obj.posinfo.position.x;
        group->center.y += ship->obj.posinfo.position.y;
        group->center.z += ship->obj.posinfo.position.z;
    }
    group->center.x /= (real32)n;
    group->center.y /= (real32)n;
    group->center.z /= (real32)n;
}

// the teams the exe keeps in one group (6, 10, 11, 12, 13, 14, 15): the group is opened on the first ship
static void aisTeamAddShipToFirstSelection(AISTeamEntry *team, Ship *ship)
{
    if (team->numselections == 0)
    {
        team->selection[0].sel.numShips = 0;
        team->selection[0].center.x = 0.0f;
        team->selection[0].center.y = 0.0f;
        team->selection[0].center.z = 0.0f;
        team->numselections = 1;
    }
    cat_selSelectionAddShip(&team->selection[0].sel, ship);
    aisSelectionRecentre(&team->selection[0], ship);
    team->totalteamsize++;
}

// the team's index-th ship counting through its groups, NULL past its size: cat_aisTeamGetShip's body, which the exe
//      also inlines at most of its callers (the last match wins; there is one)
static Ship *aisTeamShipAt(sdword index, AISTeamEntry *team)
{
    Ship *found = NULL;
    sdword count = 0, i, j;

    if (index >= team->totalteamsize)
    {
        return NULL;
    }
    for (i = 0; i < (sdword)team->numselections; i++)
    {
        for (j = 0; j < team->selection[i].sel.numShips; j++)
        {
            if (count == index)
            {
                found = team->selection[i].sel.ShipPtr[j];
            }
            count++;
        }
    }
    return found;
}

// the ships of one type in the being-built team plus those in the build queue (aisFleetUpdate's inlined count; CET
//      GetUnassignedShipCount)
static sdword aisNumBeingBuilt(ShipType type)
{
    AISTeamEntry *team = &aisTeams[AIS_TEAM_BeingBuilt];
    sdword count = (sdword)aiBuildingShip[type], i, j;

    for (i = 0; i < (sdword)team->numselections; i++)
    {
        for (j = 0; j < team->selection[i].sel.numShips; j++)
        {
            if (team->selection[i].sel.ShipPtr[j]->shiptype == type)
            {
                count++;
            }
        }
    }
    return count;
}

// the own ship furthest from complete (cat_aisUpdateTeams 0x0043013f, 0x00430407, 0x004304ab, 0x004306d5): the
//      exe keeps the fraction health / maxhealth unrounded on the x87 (53 bits) and compares its difference from the
//      best so far, which it stores as a float; a cheaper ship replaces a dearer one only when it is at least 0.4
//      further behind
static void aisLeastBuiltCheck(Ship *ship, real32 *leastFraction)
{
    double fraction = (double)ship->targetable.health / (double)((ShipStaticInfo *)ship->staticinfo)->health.maxhealth;
    double diff = fraction - (double)*leastFraction;
    real32 diffStored = (real32)diff;

    if (!(diff < 0.0))
    {
        return;
    }
    if ((aisLeastBuiltShip != NULL) &&
        (((ShipStaticInfo *)aisLeastBuiltShip->staticinfo)->buildCost > ((ShipStaticInfo *)ship->staticinfo)->buildCost) &&
        !(diffStored <= -0.4f))
    {
        return;
    }
    *leastFraction = (real32)fraction;
    aisLeastBuiltShip = ship;
}

static bool32 aisShipBeingBuiltFlag(Ship *ship)
{
    return bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt);
}

// an own ship under external construction joins the being-built team (cat_aisUpdateTeams, the medium ships, the
//      carriers and the processors)
static void aisAddExternalConstruction(Ship *ship)
{
    aiHasExternalConstruction = TRUE;
    aisShipBeingBuilt = TRUE;
    aisTeamAddShipToFirstSelection(&aisTeams[AIS_TEAM_BeingBuilt], ship);
}

// the host ship a module or carrier support unit is clamped to (Ship+0x1BC clampInfo, its first word)
static Ship *aisClampHost(Ship *ship)
{
    ClampInfo *clampInfo = ((SpaceObjRotImpTargGuidance *)ship)->clampInfo;

    return (clampInfo != NULL) ? (Ship *)clampInfo->host : NULL;
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisTeamAddShip
    Description : CAT (0x0042fa80): a ship joins the group whose lead ship shares its command, else the nearest
                  group within 5000, else a new group (the last one once there are ten)
----------------------------------------------------------------------------*/
void cat_aisTeamAddShip(AISTeamEntry *team, Ship *ship)
{
    sdword i, best = -1;
    real32 bestDist = AIS_FAR_AWAY, dist;
    bool32 sameCommand = FALSE;
    AISTeamSelection *group;
    vector diff;

    for (i = 0; i < (sdword)team->numselections; i++)
    {
        group = &team->selection[i];
        if ((group->sel.numShips > 0) && (ship->command != NULL) && (ship->command == group->sel.ShipPtr[0]->command))
        {
            sameCommand = TRUE;
            best = i;
        }
        else if (!sameCommand)
        {
            vecSub(diff, group->center, ship->obj.posinfo.position);
            dist = getVectDistSloppy(diff);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = i;
            }
        }
    }

    if ((best != -1) && ((bestDist < AIS_JOIN_DISTANCE) || sameCommand))
    {
        group = &team->selection[best];
    }
    else if (team->numselections < AIS_MAX_TEAM_SELECTIONS)
    {
        group = &team->selection[team->numselections];
        cat_selSelectionAddShip(&group->sel, ship);
        aisSelectionRecentre(group, ship);
        team->numselections++;
        team->totalteamsize++;
        return;
    }
    else
    {
        group = &team->selection[team->numselections - 1];
    }
    cat_selSelectionAddShip(&group->sel, ship);
    aisSelectionRecentre(group, ship);
    team->totalteamsize++;
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisUpdateTeams
    Description : CAT (0x0042fd50): every ship of the universe into the current computer player's teams (own and
                  enemy), the per-update words, then the build queue's counts
----------------------------------------------------------------------------*/
void cat_aisUpdateTeams(void)
{
    Player *player = aiCurrentAIPlayer->player;
    Node *node, *todonode;
    Ship *ship, *host;
    CommandToDo *command;
    CatSalvagerInfo *salvager;
    sdword maxBounty = 0, maxBountyIndex = -1, bounty, i, k;
    real32 leastFraction = 1.0f;
    bool32 own, enemy;

    aiHasExternalConstruction = FALSE;
    aisShipBeingBuilt = FALSE;
    aisCrystalTarget = NULL;
    aiCarrier1Ship = NULL;
    aiCarrier2Ship = NULL;
    aiHasSupportModuleQueued = FALSE;
    aiHasCarrier1ModuleQueued = FALSE;
    aiHasCarrier2ModuleQueued = FALSE;
    aisEnemyBigGunActiveShip = NULL;
    aisEnemyBigGunChargedShip = NULL;
    aisOwnBigGunShip = NULL;
    aisLeastBuiltShip = NULL;
    aisEnemyDFGNearMothership = FALSE;

    // the living player (not this one) with the largest bounty
    for (i = 0; i < MAX_MULTIPLAYER_PLAYERS; i++)
    {
        if (universe.players[i].playerState == PLAYER_DEAD)
        {
            continue;
        }
        if ((getPlayerBountyRender(&universe.players[i]) > maxBounty) && (i != player->playerIndex))
        {
            maxBountyIndex = i;
            maxBounty = getPlayerBountyRender(&universe.players[i]);
        }
    }

    for (k = 0; k < AIS_NUM_TEAMS; k++)
    {
        for (i = 0; i < AIS_MAX_TEAM_SELECTIONS; i++)
        {
            aisTeams[k].selection[i].center.x = 0.0f;
            aisTeams[k].selection[i].center.y = 0.0f;
            aisTeams[k].selection[i].center.z = 0.0f;
            aisTeams[k].selection[i].sel.numShips = 0;
        }
        aisTeams[k].numselections = 0;
        aisTeams[k].totalteamsize = 0;
    }

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        own = FALSE;
        enemy = FALSE;
        if (ship->playerowner->playerIndex < MAX_MULTIPLAYER_PLAYERS)
        {
            if (!allianceArePlayersAllied(ship->playerowner, player))
            {
                // an enemy whose bounty is more than five times smaller than the leader's is left alone
                //      (0x0042fe8f..0x0042fed4: integer division, compared as a float with 5.0)
                enemy = TRUE;
                if ((maxBountyIndex != -1) && (maxBountyIndex != ship->playerowner->playerIndex))
                {
                    bounty = getPlayerBountyRender(ship->playerowner);
                    // CAT-FIX: the exe divides by the bounty unguarded (calculatePlayerBounties keeps it at 1 or more)
                    if ((bounty != 0) && ((real32)(maxBounty / bounty) > 5.0f))
                    {
                        enemy = FALSE;
                    }
                }
            }
            else if (ship->playerowner == player)
            {
                own = TRUE;
            }
        }

        if (bitTest(ship->header.flags, SOF_Dead))
        {
            continue;                           // 0x0042fee7
        }

        switch (ship->shiptype)
        {
            case sRecon:
            case bRecon:
                if (own) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_Scout], ship);
                else if (enemy) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_ScoutEnemy], ship);
                break;

            case sAcolyte:
            case sACV:
            case bAcolyte:
            case bInterceptor:
            case bCloakedFighter:
            case bAttackBomber:
            case bACV:
                if (own) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_Fighter], ship);
                else if (enemy) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_FighterEnemy], ship);
                break;

            case sMimic:
            case sMCV:
                if (own) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_Mimic], ship);
                else if (enemy)
                {
                    cat_aisTeamAddShip(cat_shipDetectedByPlayer(ship, player->playerIndex) ?
                                       &aisTeams[AIS_TEAM_MimicEnemyDetected] : &aisTeams[AIS_TEAM_MimicEnemy], ship);
                }
                break;

            case sLeech:
                if (own) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_Leech], ship);
                else if (enemy) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_LeechEnemy], ship);
                break;

            case sRammingFrigate:
            case sHiveFrigate:
            case sDreadnought:
            case sDestroyer:
            case bHeavyCorvette:
            case bHiveFrigate:
            case bIonArrayFrigate:
            case bHeavyCruiser:
            case bRammingFrigate:
            case bCruiseMissile:
            case bMissileCorvette:
                if (own)
                {
                    if (ship->shiptype == bCruiseMissile)
                    {
                        cat_aisTeamAddShip(&aisTeams[AIS_TEAM_CruiseMissile], ship);
                    }
                    else if (aisShipBeingBuiltFlag(ship))
                    {
                        aisAddExternalConstruction(ship);
                    }
                    else
                    {
                        cat_aisTeamAddShip(&aisTeams[AIS_TEAM_MediumCombat], ship);
                    }
                    switch (ship->shiptype)
                    {
                        case sRammingFrigate:
                        case sHiveFrigate:
                        case sDreadnought:
                        case sDestroyer:
                        case bHiveFrigate:
                        case bIonArrayFrigate:
                        case bHeavyCruiser:
                        case bRammingFrigate:
                            aisLeastBuiltCheck(ship, &leastFraction);
                            break;
                        default:
                            break;
                    }
                }
                else if (enemy) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_MediumCombatEnemy], ship);
                break;

            case sMultiBeamFrigate:
            case bMultiGunCorvette:
            case bMultiBeamFrigate:
                if (own) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_AntiFighter], ship);
                else if (enemy) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_AntiFighterEnemy], ship);
                break;

            case sSentinel:
            case bSentinel:
                if (own) aisTeamAddShipToFirstSelection(&aisTeams[AIS_TEAM_Sentinel], ship);
                else if (enemy) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_SentinelEnemy], ship);
                break;

            case sCarrier:
            case bCarrier:
                if (own)
                {
                    if (aisShipBeingBuiltFlag(ship))
                    {
                        aisAddExternalConstruction(ship);
                    }
                    else
                    {
                        cat_aisTeamAddShip(&aisTeams[AIS_TEAM_Carrier], ship);
                        if (aiCarrier1Ship == NULL)
                        {
                            aiCarrier1Ship = ship;
                        }
                        else if (aiCarrier2Ship == NULL)
                        {
                            aiCarrier2Ship = ship;
                        }
                    }
                    aisLeastBuiltCheck(ship, &leastFraction);
                }
                else if (enemy) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_CarrierEnemy], ship);
                break;

            case sMothership:
            case bMothership:
                if (own)
                {
                    cat_aisTeamAddShip(&aisTeams[AIS_TEAM_Mothership], ship);
                    if ((ship->shiptype == sMothership) && cat_mothershipBigGunActive(ship))
                    {
                        aisOwnBigGunShip = ship;
                    }
                    aisLeastBuiltCheck(ship, &leastFraction);
                }
                else if (enemy)
                {
                    cat_aisTeamAddShip(&aisTeams[AIS_TEAM_MothershipEnemy], ship);
                    if (ship->shiptype == sMothership)
                    {
                        if (cat_mothershipBigGunActive(ship))
                        {
                            aisEnemyBigGunActiveShip = ship;
                        }
                        else if (cat_bigGunIsCharged(ship))
                        {
                            aisEnemyBigGunChargedShip = ship;
                        }
                    }
                }
                break;

            case sProcessor:
            case bProcessor:
                if (own)
                {
                    if (aisShipBeingBuiltFlag(ship))
                    {
                        aisAddExternalConstruction(ship);
                    }
                    else
                    {
                        cat_aisTeamAddShip(&aisTeams[AIS_TEAM_Processor], ship);
                    }
                }
                else if (enemy) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_ProcessorEnemy], ship);
                break;

            case sWorker:
            case bWorker:
                if (own)
                {
                    cat_aisTeamAddShip(&aisTeams[AIS_TEAM_Worker], ship);
                    // a Worker salvaging a crystal (its special order) names the crystal for the processor and carrier
                    command = ship->command;
                    if ((command != NULL) && (command->ordertype.order == COMMAND_SPECIAL) &&
                        ((salvager = cat_salvageGetSalvagerInfo(ship)) != NULL) && (salvager->target != NULL) &&
                        cat_shipIsCrystal((Ship *)salvager->target))
                    {
                        aisCrystalTarget = (Ship *)salvager->target;
                    }
                    aisLeastBuiltCheck(ship, &leastFraction);
                }
                else if (enemy) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_WorkerEnemy], ship);
                break;

            case sCarrierSupport:
                if (own)
                {
                    host = aisClampHost(ship);
                    if (host != NULL)
                    {
                        if (host == aiCarrier1Ship)
                        {
                            aisTeamAddShipToFirstSelection(&aisTeams[AIS_TEAM_CarrierSupport1], ship);
                            if (!aisShipBeingBuiltFlag(ship)) break;
                            aiHasCarrier1ModuleQueued = TRUE;
                        }
                        else if (host == aiCarrier2Ship)
                        {
                            aisTeamAddShipToFirstSelection(&aisTeams[AIS_TEAM_CarrierSupport2], ship);
                            if (!aisShipBeingBuiltFlag(ship)) break;
                            aiHasCarrier2ModuleQueued = TRUE;
                        }
                        else if (aiCarrier1Ship == NULL)
                        {
                            aiCarrier1Ship = host;
                            aisTeamAddShipToFirstSelection(&aisTeams[AIS_TEAM_CarrierSupport1], ship);
                            if (!aisShipBeingBuiltFlag(ship)) break;
                            aiHasCarrier1ModuleQueued = TRUE;
                        }
                        else if (aiCarrier2Ship == NULL)
                        {
                            aiCarrier2Ship = host;
                            aisTeamAddShipToFirstSelection(&aisTeams[AIS_TEAM_CarrierSupport2], ship);
                            if (!aisShipBeingBuiltFlag(ship)) break;
                            aiHasCarrier2ModuleQueued = TRUE;
                        }
                    }
                    if (aisShipBeingBuiltFlag(ship))
                    {
                        aisShipBeingBuilt = TRUE;
                    }
                }
                else if (enemy) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_ModulesEnemy], ship);
                break;

            case sMothershipSupport:
            case bMothershipSupport:
                if (own)
                {
                    if (aisShipBeingBuiltFlag(ship))
                    {
                        aisShipBeingBuilt = TRUE;
                        aiHasSupportModuleQueued = TRUE;
                    }
                    aisTeamAddShipToFirstSelection(&aisTeams[AIS_TEAM_MothershipSupport], ship);
                }
                else if (enemy) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_ModulesEnemy], ship);
                break;

            case sMothershipSpecial:
            case sMothershipEngineering:
            case sMothershipArmour:
            case sMothershipMicro:
            case sMothershipWeapons:
            case sMothershipDockingBay:
            case sMothershipLowerSpine:
            case sMothershipBigGun:
                if (own)
                {
                    if (aisShipBeingBuiltFlag(ship))
                    {
                        aisShipBeingBuilt = TRUE;
                    }
                    aisTeamAddShipToFirstSelection(&aisTeams[AIS_TEAM_MothershipModules], ship);
                    if ((ship->shiptype == sMothershipBigGun) && ((host = aisClampHost(ship)) != NULL) &&
                        cat_bigGunIsCharged(host))
                    {
                        aisTeamAddShipToFirstSelection(&aisTeams[AIS_TEAM_BigGunCharged], ship);
                    }
                }
                else if (enemy) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_ModulesEnemy], ship);
                break;

            case bDFGFrigate:
                if (own) cat_aisTeamAddShip(&aisTeams[AIS_TEAM_DFGFrigate], ship);
                else if (enemy)
                {
                    cat_aisTeamAddShip(&aisTeams[AIS_TEAM_DFGFrigateEnemy], ship);
                    if ((player->PlayerMothership != NULL) &&
                        (cat_vecDistance(&ship->obj.posinfo.position, &player->PlayerMothership->obj.posinfo.position) < 3500.0f))
                    {
                        aisEnemyDFGNearMothership = TRUE;
                    }
                }
                break;

            case dCrystal0:
            case dCrystal1:
                cat_aisTeamAddShip(&aisTeams[AIS_TEAM_Crystal], ship);
                break;

            default:
                break;
        }
    }

    // the build queue: the ships of each type this player's build jobs will make and their support units
    for (i = 0; i < TOTAL_STD_SHIPS; i++)
    {
        aiBuildingShip[i] = 0;
    }
    aiSupportUnitsPending = 0;
    for (todonode = universe.mainCommandLayer.todolist.head; todonode != NULL; todonode = todonode->next)
    {
        command = (CommandToDo *)listGetStructOfNode(todonode);
        if ((command->ordertype.order == COMMAND_BUILDING_SHIP) &&
            (command->buildingship.playerIndex == (udword)player->playerIndex))
        {
            aiBuildingShip[command->buildingship.shipType]++;
            aiSupportUnitsPending += (sdword)GetShipStaticInfo(command->buildingship.shipType,
                                                               (ShipRace)cat_ShipTypeToRace((udword)command->buildingship.shipType))->supportUnits;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisSetupAlliances
    Description : CAT (0x004310a0): the computer players ally by the game's bigotry setting: 0 none, 1 in pairs by
                  how many computers there are (the computers follow the humans: tpGameCreated.numPlayers is the
                  first computer's index), else every computer with every later player index up to 7
----------------------------------------------------------------------------*/
void cat_aisSetupAlliances(void)
{
    uword first = tpGameCreated.numPlayers, i, j;
    sdword computers;

    if (tpGameCreated.aiplayerBigotry == 0)
    {
        return;
    }
    if (tpGameCreated.aiplayerBigotry == 1)
    {
        computers = (sdword)universe.numPlayers - (sdword)first;
        switch (computers)
        {
            case 3:
                clWrapSetAlliance(ALLIANCE_FORM_NEW_ALLIANCE, first + 1, first + 2);
                clWrapSetAlliance(ALLIANCE_FORM_NEW_ALLIANCE, first + 2, first + 1);
                break;
            case 4:
            case 5:
                clWrapSetAlliance(ALLIANCE_FORM_NEW_ALLIANCE, first, first + 1);
                clWrapSetAlliance(ALLIANCE_FORM_NEW_ALLIANCE, first + 1, first);
                clWrapSetAlliance(ALLIANCE_FORM_NEW_ALLIANCE, first + 2, first + 3);
                clWrapSetAlliance(ALLIANCE_FORM_NEW_ALLIANCE, first + 3, first + 2);
                break;
            case 6:
            case 7:
                clWrapSetAlliance(ALLIANCE_FORM_NEW_ALLIANCE, first, first + 1);
                clWrapSetAlliance(ALLIANCE_FORM_NEW_ALLIANCE, first + 1, first);
                clWrapSetAlliance(ALLIANCE_FORM_NEW_ALLIANCE, first + 2, first + 3);
                clWrapSetAlliance(ALLIANCE_FORM_NEW_ALLIANCE, first + 3, first + 2);
                clWrapSetAlliance(ALLIANCE_FORM_NEW_ALLIANCE, first + 4, first + 5);
                clWrapSetAlliance(ALLIANCE_FORM_NEW_ALLIANCE, first + 5, first + 4);
                break;
            default:
                break;                          // one or two computers: no alliance
        }
        return;
    }
    for (i = first; i < MAX_MULTIPLAYER_PLAYERS; i++)
    {
        for (j = i + 1; j < MAX_MULTIPLAYER_PLAYERS; j++)
        {
            clWrapSetAlliance(ALLIANCE_FORM_NEW_ALLIANCE, i, j);
        }
    }
}

// the first technology of a list the player does not have yet, TECH_NONE if it has them all
static TechnologyType aisFirstMissingTech(Player *player, const TechnologyType *list, sdword count)
{
    sdword i;

    for (i = 0; i < count; i++)
    {
        if (!cat_rmTechSetHas(player->researchinfo.HasTechnology, list[i]))
        {
            return list[i];
        }
    }
    return TECH_NONE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisResearchUpdate
    Description : CAT (0x00431290): a Sect player researches through each finished module its module's list; a Beast
                  player, when its labs are idle, through one list; a Beast mothership above 70% health switches
                  system 4 on (intermediate and advanced)
----------------------------------------------------------------------------*/
void cat_aisResearchUpdate(void)
{
    Player *player = aiCurrentAIPlayer->player;
    Ship *module, *mothership = player->PlayerMothership;
    const TechnologyType *list;
    TechnologyType tech;
    sdword i, count;

    if ((mothership == NULL) || (((ShipStaticInfo *)mothership->staticinfo)->shipclass != CLASS_Mothership))
    {
        return;
    }

    if (player->race == RACE_Sect)
    {
        if (aisTeams[AIS_TEAM_MothershipModules].numselections == 1)
        {
            for (i = 0; i < aisTeams[AIS_TEAM_MothershipModules].selection[0].sel.numShips; i++)
            {
                module = aisTeams[AIS_TEAM_MothershipModules].selection[0].sel.ShipPtr[i];
                if (aisShipBeingBuiltFlag(module))
                {
                    continue;
                }
                switch (module->shiptype)
                {
                    case sMothershipSpecial:    list = aisResearchSpecial;      count = 5; break;
                    case sMothershipArmour:     list = aisResearchArmour;       count = 5; break;
                    case sMothershipMicro:      list = aisResearchMicro;        count = 5; break;
                    case sMothershipWeapons:    list = aisResearchWeapons;      count = 7; break;
                    case sMothershipDockingBay: list = aisResearchDockingBay;   count = 7; break;
                    default:                    list = NULL;                    count = 0; break;
                }
                if (list == NULL)
                {
                    continue;
                }
                tech = aisFirstMissingTech(player, list, count);
                if (tech != TECH_NONE)
                {
                    cat_rmAIResearchTech(player, tech);
                }
            }
        }
    }
    else if (player->race == RACE_Beast)
    {
        if (!rmResearchingAnything(player))
        {
            tech = aisFirstMissingTech(player, aisResearchBeast, 18);
            if (tech != TECH_NONE)
            {
                cat_rmAIResearchTech(player, tech);
            }
        }
    }

    // 0x0043142c: the exe compares the unrounded fraction with the double 0.7
    if ((player->race == RACE_Beast) &&
        ((double)mothership->targetable.health / (double)((ShipStaticInfo *)mothership->staticinfo)->health.maxhealth > 0.7) &&
        (aiCurrentAIPlayer->aiplayerDifficultyLevel > AI_BEGINNER))
    {
        cat_clWrapSetBeastSystem(&universe.mainCommandLayer, mothership, 4);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisTeamGetShip
    Description : CAT (0x00431490): the team's index-th ship, NULL past its size
----------------------------------------------------------------------------*/
Ship *cat_aisTeamGetShip(sdword index, AISTeamEntry *team)
{
    return aisTeamShipAt(index, team);
}

/*-----------------------------------------------------------------------------
    Name        : aisRequestShip
    Description : CAT (0x00431510): one ship of a type from the mothership while the support units and the RUs
                  (above buildCost) allow, an external ship created at the mothership instead; then the same type from
                  each carrier in turn while they allow
----------------------------------------------------------------------------*/
void aisRequestShip(Player *player, ShipType type, sdword buildCost)
{
    ShipStaticInfo *statics = GetShipStaticInfo(type, (ShipRace)cat_ShipTypeToRace((udword)type));
    sdword supportUsed = (sdword)statics->supportUnits + player->supportUnitsUsed + aiSupportUnitsPending;
    sdword RUs = player->resourceUnits;
    sdword i;
    Ship *mothership = aisTeams[AIS_TEAM_Mothership].selection[0].sel.ShipPtr[0];
    Ship *carrier;

    if ((supportUsed >= player->supportUnitsMax) || (RUs <= buildCost))
    {
        return;
    }
    if (statics->externalBuild)
    {
        if (aisTeams[AIS_TEAM_Mothership].totalteamsize > 0)
        {
            if (rmCanBuildShip(player, type, TRUE))
            {
                clWrapCreateShip(&universe.mainCommandLayer, type, player->race, player->playerIndex, mothership);
            }
            return;
        }
    }
    else if ((aisTeams[AIS_TEAM_Mothership].totalteamsize > 0) && rmCanBuildShip(player, type, TRUE))
    {
        clWrapBuildShip(&universe.mainCommandLayer, type, player->race, player->playerIndex, mothership);
        supportUsed += (sdword)statics->supportUnits;
        RUs -= buildCost;
    }

    if (!rmCanBuildShip(player, type, FALSE))
    {
        return;
    }
    for (i = 0; supportUsed < player->supportUnitsMax; i++)
    {
        carrier = aisTeamShipAt(i, &aisTeams[AIS_TEAM_Carrier]);
        if ((carrier == NULL) || (RUs <= buildCost))
        {
            return;
        }
        clWrapBuildShip(&universe.mainCommandLayer, type, player->race, player->playerIndex, carrier);
        RUs -= buildCost;
        supportUsed += (sdword)statics->supportUnits;
    }
}

// the per-difficulty tables of aisFleetUpdate (0 for another level, as the exe's switches)
static sdword aisByLevel(sdword beginner, sdword intermediate, sdword advanced)
{
    switch (aiCurrentAIPlayer->aiplayerDifficultyLevel)
    {
        case AI_BEGINNER: return beginner;
        case AI_INTERMEDIATE: return intermediate;
        case AI_ADVANCED: return advanced;
        default:     return 0;
    }
}

/*-----------------------------------------------------------------------------
    Name        : aisFleetUpdate
    Description : CAT (0x00431750): what the computer player builds this update: the Sect modules (the first one
                  missing), the support modules, one capital ship unless one is being built, carrier support, Workers,
                  scouts, DFG frigates, Leeches, mimics, cruise missiles, then anti-fighter ships when the enemy has
                  more than 15 fighters or else a medium ship, and fighters; a computer player that cannot add
                  fighters gives its first fighter group a kamikaze (advanced) or scuttle order
----------------------------------------------------------------------------*/
void aisFleetUpdate(void)
{
    Player *player = aiCurrentAIPlayer->player;
    Ship *mothership = player->PlayerMothership;
    Ship *creator;
    Node *node;
    CommandToDo *command;
    Ship *first;
    ShipType type;
    sdword cost, target, count;
    bool32 capitalPossible = FALSE;
    static const ShipType sectModules[5] = { sMothershipDockingBay, sMothershipMicro, sMothershipWeapons,
                                             sMothershipSpecial, sMothershipArmour };
    sdword i;

    if ((mothership != NULL) && (((ShipStaticInfo *)mothership->staticinfo)->shipclass == CLASS_Mothership))
    {
        if (player->race == RACE_Sect)
        {
            if (player->resourceUnits > 0)
            {
                // the first module neither built nor being built (0x0043179e..0x004318c5)
                type = ShipType_Uninitialized;
                cost = 0;
                for (i = 0; i < 5; i++)
                {
                    if ((selNumShipsInSelection(&aisTeams[AIS_TEAM_BeingBuilt].selection[0].sel, sectModules[i]) == 0) &&
                        (selNumShipsInSelection(&aisTeams[AIS_TEAM_MothershipModules].selection[0].sel, sectModules[i]) == 0))
                    {
                        type = sectModules[i];
                        break;
                    }
                }
                if ((type == ShipType_Uninitialized) &&
                    (selNumShipsInSelection(&aisTeams[AIS_TEAM_BeingBuilt].selection[0].sel, sMothershipBigGun) == 0) &&
                    (selNumShipsInSelection(&aisTeams[AIS_TEAM_MothershipModules].selection[0].sel, sMothershipBigGun) == 0))
                {
                    type = sMothershipBigGun;
                    cost = 5000;
                }
                if (type != ShipType_Uninitialized)
                {
                    aisRequestShip(player, type, cost);
                }
            }
            creator = aisTeams[AIS_TEAM_Mothership].selection[0].sel.ShipPtr[0];
            if ((aisTeams[AIS_TEAM_Mothership].totalteamsize > 0) &&
                (aisTeams[AIS_TEAM_MothershipSupport].totalteamsize < 12) && !aiHasSupportModuleQueued &&
                rmCanBuildShip(player, sMothershipSupport, TRUE))
            {
                clWrapCreateShip(&universe.mainCommandLayer, sMothershipSupport, player->race, player->playerIndex, creator);
            }
        }
        else if (player->race == RACE_Beast)
        {
            creator = aisTeams[AIS_TEAM_Mothership].selection[0].sel.ShipPtr[0];
            if ((aisTeams[AIS_TEAM_Mothership].totalteamsize > 0) &&
                (aisTeams[AIS_TEAM_MothershipSupport].totalteamsize < 12) && !aiHasSupportModuleQueued &&
                rmCanBuildShip(player, bMothershipSupport, TRUE))
            {
                clWrapCreateShip(&universe.mainCommandLayer, bMothershipSupport, player->race, player->playerIndex, creator);
            }
        }

        // an external ship of this player (not a module) under construction holds the capital ship back
        if (!aiHasExternalConstruction)
        {
            for (node = universe.mainCommandLayer.todolist.head; node != NULL; node = node->next)
            {
                command = (CommandToDo *)listGetStructOfNode(node);
                if ((command->ordertype.order == COMMAND_LAUNCH_SHIP) && (command->selection->numShips > 0))
                {
                    first = command->selection->ShipPtr[0];
                    if ((first->playerowner == player) && ((ShipStaticInfo *)first->staticinfo)->externalBuild &&
                        (((ShipStaticInfo *)first->staticinfo)->shipclass != CLASS_Component))
                    {
                        aiHasExternalConstruction = TRUE;
                    }
                }
            }
            if (!aiHasExternalConstruction)
            {
                type = ShipType_Uninitialized;
                cost = 0;
                if (player->race == RACE_Sect)
                {
                    if (rmCanBuildShip(player, sCarrier, TRUE) && (aisTeams[AIS_TEAM_Carrier].totalteamsize < 2))
                    {
                        type = sCarrier; cost = 2000;
                    }
                    else if (rmCanBuildShip(player, sDreadnought, TRUE) && (player->resourceUnits > 2000))
                    {
                        type = sDreadnought; cost = 3000;
                    }
                    else if (rmCanBuildShip(player, sDestroyer, TRUE) && (player->resourceUnits > 1000))
                    {
                        type = sDestroyer; cost = 1500;
                    }
                    else if (rmCanBuildShip(player, sProcessor, TRUE) && (player->resourceUnits > 1000) &&
                             (aisTeams[AIS_TEAM_Processor].totalteamsize == 0))
                    {
                        type = sProcessor; cost = 1000;
                    }
                }
                else if (player->race == RACE_Beast)
                {
                    if (rmCanBuildShip(player, bCarrier, TRUE) && (aisTeams[AIS_TEAM_Carrier].totalteamsize < 2))
                    {
                        type = bCarrier; cost = 2000;
                    }
                    else if (rmCanBuildShip(player, bHeavyCruiser, TRUE) && (player->resourceUnits > 2000))
                    {
                        type = bHeavyCruiser; cost = 3000;
                    }
                    else if (rmCanBuildShip(player, bProcessor, TRUE) && (player->resourceUnits > 1000) &&
                             (aisTeams[AIS_TEAM_Processor].totalteamsize == 0))
                    {
                        type = bProcessor; cost = 1000;
                    }
                }
                if (type != ShipType_Uninitialized)
                {
                    aisRequestShip(player, type, cost);
                }
            }
        }
    }

    // carrier support for a Sect carrier (six each)
    if ((aiCarrier1Ship != NULL) && (aiCarrier1Ship->playerowner->race == RACE_Sect) &&
        (aisTeams[AIS_TEAM_CarrierSupport1].totalteamsize < 6) && !aiHasCarrier1ModuleQueued &&
        rmCanBuildShip(player, sCarrierSupport, TRUE))
    {
        clWrapCreateShip(&universe.mainCommandLayer, sCarrierSupport, player->race, player->playerIndex, aiCarrier1Ship);
    }
    if ((aiCarrier2Ship != NULL) && (aiCarrier2Ship->playerowner->race == RACE_Sect) &&
        (aisTeams[AIS_TEAM_CarrierSupport2].totalteamsize < 6) && !aiHasCarrier2ModuleQueued &&
        rmCanBuildShip(player, sCarrierSupport, TRUE))
    {
        clWrapCreateShip(&universe.mainCommandLayer, sCarrierSupport, player->race, player->playerIndex, aiCarrier2Ship);
    }

    // Workers: 5 / 8 / 12, 2 / 4 / 6 above 50,000 RUs
    count = aisNumBeingBuilt((player->race == RACE_Sect) ? sWorker : bWorker) + aisTeams[AIS_TEAM_Worker].totalteamsize;
    if (player->resourceUnits > AIS_RICH)
    {
        target = aisByLevel(2, 4, 6);
    }
    else
    {
        target = aisByLevel(5, 8, 12);
    }
    if (target > count)
    {
        if (player->race == RACE_Sect) aisRequestShip(player, sWorker, 0);
        else if (player->race == RACE_Beast) aisRequestShip(player, bWorker, 0);
    }

    // scouts: 5 / 8 / 12, no more than the enemy has
    target = aisByLevel(5, 8, 12);
    if (aisTeams[AIS_TEAM_ScoutEnemy].totalteamsize < target)
    {
        target = aisTeams[AIS_TEAM_ScoutEnemy].totalteamsize;
    }
    if (aisTeams[AIS_TEAM_Scout].totalteamsize < target)
    {
        count = aisNumBeingBuilt((player->race == RACE_Sect) ? sRecon : bRecon) + aisTeams[AIS_TEAM_Scout].totalteamsize;
        if (target > count)
        {
            if (player->race == RACE_Sect) aisRequestShip(player, sRecon, 50);
            else if (player->race == RACE_Beast) aisRequestShip(player, bRecon, 50);
        }
    }

    // Beast DFG frigates: 0 / 1 / 2
    if (player->race == RACE_Beast)
    {
        target = aisByLevel(0, 1, 2);
        if ((aisTeams[AIS_TEAM_DFGFrigate].totalteamsize < target) &&
            (aisNumBeingBuilt(bDFGFrigate) + aisTeams[AIS_TEAM_DFGFrigate].totalteamsize < target))
        {
            aisRequestShip(player, bDFGFrigate, 2000);
        }
    }

    // Sect Leeches 0 / 10 / 20 and mimics (with an MCV) 0 / 0 / 20
    if (player->race == RACE_Sect)
    {
        target = aisByLevel(0, 10, 20);
        if ((aisTeams[AIS_TEAM_Leech].totalteamsize < target) &&
            (aisNumBeingBuilt(sLeech) + aisTeams[AIS_TEAM_Leech].totalteamsize < target))
        {
            aisRequestShip(player, sLeech, 100);
        }
        if (player->race == RACE_Sect)
        {
            target = aisByLevel(0, 0, 20);
            if ((aisTeams[AIS_TEAM_Mimic].totalteamsize < target) &&
                (aisNumBeingBuilt(sMimic) + aisTeams[AIS_TEAM_Mimic].totalteamsize < target))
            {
                aisRequestShip(player, sMimic, 150);
                aisRequestShip(player, sMCV, 300);
            }
        }
    }

    // Beast cruise missiles: 0 / 0 / 10
    if (player->race == RACE_Beast)
    {
        target = aisByLevel(0, 0, 10);
        if ((aisTeams[AIS_TEAM_CruiseMissile].totalteamsize < target) &&
            (aisNumBeingBuilt(bCruiseMissile) + aisTeams[AIS_TEAM_CruiseMissile].totalteamsize < target))
        {
            aisRequestShip(player, bCruiseMissile, 400);
        }
    }

    if (aisTeams[AIS_TEAM_FighterEnemy].totalteamsize > 15)
    {
        // anti-fighter ships, then the fighters (0x00432000)
        if (player->race == RACE_Sect)
        {
            aisRequestShip(player, sMultiBeamFrigate, 800);
        }
        else if (player->race == RACE_Beast)
        {
            aisRequestShip(player, bMultiBeamFrigate, 800);
            aisRequestShip(player, bMultiGunCorvette, 400);
        }
    }
    else
    {
        // a medium ship unless a capital ship can be built (0x00432073)
        type = ShipType_Uninitialized;
        if (player->race == RACE_Sect)
        {
            if ((rmCanBuildShip(player, sDreadnought, TRUE) || rmCanBuildShip(player, sDestroyer, TRUE)) &&
                !aiHasExternalConstruction)
            {
                capitalPossible = TRUE;
            }
            else
            {
                type = ((ranRandom(RANDOM_AI_PLAYER) % 6) == 0) ? sRammingFrigate : sHiveFrigate;
            }
        }
        else if (player->race == RACE_Beast)
        {
            if (rmCanBuildShip(player, bHeavyCruiser, TRUE) && !aiHasExternalConstruction)
            {
                capitalPossible = TRUE;
            }
            else
            {
                switch (ranRandom(RANDOM_AI_PLAYER) % 7)
                {
                    case 0:  type = bHeavyCorvette;   break;
                    case 1:  type = bHiveFrigate;     break;
                    case 2:
                    case 3:  type = bIonArrayFrigate; break;
                    case 4:  type = bRammingFrigate;  break;
                    case 5:  type = bCruiseMissile;   break;
                    default: type = bMissileCorvette; break;
                }
            }
        }
        if (type != ShipType_Uninitialized)
        {
            aisRequestShip(player, type, GetShipStaticInfo(type, (ShipRace)cat_ShipTypeToRace((udword)type))->buildCost);
        }
        // 0x00432168: unsigned, as the exe
        if (((udword)(player->supportUnitsUsed + aiSupportUnitsPending + 50) > (udword)player->supportUnitsMax) ||
            capitalPossible)
        {
            if ((sdword)aisTeams[AIS_TEAM_Fighter].numselections <= 0)
            {
                return;
            }
            if (aiCurrentAIPlayer->aiplayerDifficultyLevel >= AI_ADVANCED)
            {
                clWrapSetKamikaze(&universe.mainCommandLayer, (SelectCommand *)&aisTeams[AIS_TEAM_Fighter].selection[0].sel);
            }
            else
            {
                clWrapScuttle(&universe.mainCommandLayer, (SelectCommand *)&aisTeams[AIS_TEAM_Fighter].selection[0].sel);
                clWrapScuttle(&universe.mainCommandLayer, (SelectCommand *)&aisTeams[AIS_TEAM_Fighter].selection[0].sel);
            }
            return;
        }
    }

    // fighters: 25 / 35 / 60 (0x00432020)
    target = aisByLevel(25, 35, 60);
    if (aisTeams[AIS_TEAM_Fighter].totalteamsize >= target)
    {
        return;
    }
    count = aisTeams[AIS_TEAM_Fighter].totalteamsize;
    if (player->race == RACE_Sect)
    {
        count += aisNumBeingBuilt(sAcolyte) + aisNumBeingBuilt(sACV);
    }
    else if (player->race == RACE_Beast)
    {
        count += aisNumBeingBuilt(bAcolyte) + aisNumBeingBuilt(bInterceptor) + aisNumBeingBuilt(bCloakedFighter) +
                 aisNumBeingBuilt(bAttackBomber) + aisNumBeingBuilt(bACV);
    }
    if (count >= target)
    {
        return;
    }
    if (player->race == RACE_Sect)
    {
        aisRequestShip(player, sAcolyte, 150);
        if (aiCurrentAIPlayer->aiplayerDifficultyLevel > AI_BEGINNER)
        {
            aisRequestShip(player, sACV, 300);
        }
    }
    else if (player->race == RACE_Beast)
    {
        aisRequestShip(player, bInterceptor, 100);
        if (aiCurrentAIPlayer->aiplayerDifficultyLevel > AI_BEGINNER)
        {
            aisRequestShip(player, bAttackBomber, 150);
            aisRequestShip(player, bCloakedFighter, 150);
        }
        if (aiCurrentAIPlayer->aiplayerDifficultyLevel > AI_INTERMEDIATE)
        {
            aisRequestShip(player, bAcolyte, 150);
            aisRequestShip(player, bACV, 300);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisShipIsBusy
    Description : CAT (0x004324a0): a ship is busy unless it has no order, or a null or parade order without
                  protecting
----------------------------------------------------------------------------*/
bool32 cat_aisShipIsBusy(Ship *ship)
{
    CommandToDo *command = ship->command;

    if (command == NULL)
    {
        return FALSE;
    }
    if (((command->ordertype.order == COMMAND_NULL) || (command->ordertype.order == COMMAND_MILITARY_PARADE)) &&
        !bitTest(command->ordertype.attributes, COMMAND_MASK_PROTECTING))
    {
        return FALSE;
    }
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisWorkersUpdate
    Description : CAT (0x004324d0): idle Workers harvest while harvesting is on; above 50,000 RUs the Workers instead
                  repair (special-target) the ship furthest from complete
----------------------------------------------------------------------------*/
void cat_aisWorkersUpdate(void)
{
    MaxSelection harvest, repair;
    SpecialCommand target;
    AISTeamEntry *team = &aisTeams[AIS_TEAM_Worker];
    Ship *ship;
    sdword i, j;

    harvest.numShips = 0;
    repair.numShips = 0;
    if ((sdword)team->numselections <= 0)
    {
        return;
    }
    for (i = 0; i < (sdword)team->numselections; i++)
    {
        for (j = 0; j < team->selection[i].sel.numShips; j++)
        {
            ship = team->selection[i].sel.ShipPtr[j];
            if (ship->playerowner->resourceUnits < AIS_RICH)
            {
                if (!cat_aisShipIsBusy(ship) && bitTest(tpGameCreated.flag, MG_HarvestinEnabled))
                {
                    // the exe tests the salvage upgrade here and adds the Worker to the same selection either way
                    //      (0x0043254e..0x00432581)
                    if (cat_shipHasUpgrade(ship, 0x80000) && cat_playerHasSalvageUpgradeShip(ship->playerowner))
                    {
                        cat_selSelectionAddShip(&harvest, ship);
                    }
                    else
                    {
                        cat_selSelectionAddShip(&harvest, ship);
                    }
                }
            }
            else if (ship != aisLeastBuiltShip)
            {
                cat_selSelectionAddShip(&repair, ship);
            }
        }
    }
    if (harvest.numShips > 0)
    {
        clWrapCollectResource(&universe.mainCommandLayer, (SelectCommand *)&harvest, NULL);
    }
    if ((repair.numShips > 0) && (aisLeastBuiltShip != NULL))
    {
        target.numTargets = 1;
        target.timeLastStatus = 0.0f;       // HWI: left uninitialised by the exe
        target.TargetPtr[0] = (TargetPtr)aisLeastBuiltShip;
        cat_clWrapSpecialTarget(&universe.mainCommandLayer, (SelectCommand *)&repair, &target, FALSE);
    }
}

// the processor and the carrier go to the crystal a Worker salvages (when more than 4000 away), else guard the first
//      Worker (cat_aisProcessorUpdate 0x00432630, cat_aisCarrierUpdate 0x00433620)
static void aisResourceShipUpdate(Ship *ship)
{
    SelectCommand selection;
    ProtectCommand protect;
    vector crystal, diff;

    selection.numShips = 1;
    selection.ShipPtr[0] = ship;
    if (aisCrystalTarget != NULL)
    {
        crystal = aisCrystalTarget->obj.posinfo.position;
        vecSub(diff, crystal, ship->obj.posinfo.position);
        if (getVectDistSloppy(diff) > 4000.0f)
        {
            crystal.x += 500.0f;
            crystal.y += 500.0f;
            clWrapMove(&universe.mainCommandLayer, &selection, 0, ship->obj.posinfo.position, 1, &crystal);
        }
    }
    else if (aisTeams[AIS_TEAM_Worker].totalteamsize > 0)
    {
        protect.numShips = 1;
        protect.ShipPtr[0] = aisTeams[AIS_TEAM_Worker].selection[0].sel.ShipPtr[0];
        clWrapProtect(&universe.mainCommandLayer, &selection, &protect);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisProcessorUpdate
    Description : CAT (0x00432630): the only processor follows the Workers
----------------------------------------------------------------------------*/
void cat_aisProcessorUpdate(void)
{
    if (aisTeams[AIS_TEAM_Processor].totalteamsize == 1)
    {
        aisResourceShipUpdate(aisTeams[AIS_TEAM_Processor].selection[0].sel.ShipPtr[0]);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisScoutsUpdate
    Description : CAT (0x00432760): scouts (1 / 3 / 5) shadow the enemy motherships 16,000 away, the next one guards
                  the own mothership (intermediate and advanced), the next ones (0 / 3 / 6) attack the enemy scouts,
                  and an advanced player's next scout attacks the first crate
----------------------------------------------------------------------------*/
void cat_aisScoutsUpdate(void)
{
    AISTeamEntry *scouts = &aisTeams[AIS_TEAM_Scout];
    MaxSelection attackers;
    SelectCommand selection;
    AttackCommand target;
    ProtectCommand protect;
    Ship *scout, *enemy;
    Node *node;
    Derelict *derelict;
    vector diff, dir, dest;
    sdword n = 0, budget;

    budget = aisByLevel(1, 3, 5);
    for (;;)
    {
        scout = aisTeamShipAt(n, scouts);
        if ((scout == NULL) || (budget <= 0) || (n >= aisTeams[AIS_TEAM_MothershipEnemy].totalteamsize))
        {
            break;
        }
        enemy = aisTeamShipAt(n, &aisTeams[AIS_TEAM_MothershipEnemy]);
        vecSub(diff, scout->obj.posinfo.position, enemy->obj.posinfo.position);
        n++;
        if (getVectDistSloppy(diff) > 20000.0f)
        {
            dir = diff;
            vecNormalize(&dir);
            dir.x *= 16000.0f;
            dir.y *= 16000.0f;
            dir.z *= 16000.0f;
            dest.x = dir.x + enemy->obj.posinfo.position.x;
            dest.y = dir.y + enemy->obj.posinfo.position.y;
            dest.z = dir.z + enemy->obj.posinfo.position.z;
            selection.numShips = 1;
            selection.ShipPtr[0] = scout;
            clWrapMove(&universe.mainCommandLayer, &selection, 0, scout->obj.posinfo.position, 1, &dest);
        }
        budget--;
    }

    if ((aiCurrentAIPlayer->aiplayerDifficultyLevel > AI_BEGINNER) && (aisTeams[AIS_TEAM_Mothership].totalteamsize > 0))
    {
        scout = aisTeamShipAt(n, scouts);
        if ((scout != NULL) && !cat_aisShipIsBusy(scout))
        {
            selection.numShips = 1;
            selection.ShipPtr[0] = scout;
            protect.numShips = 1;
            protect.ShipPtr[0] = aisTeams[AIS_TEAM_Mothership].selection[0].sel.ShipPtr[0];
            n++;
            clWrapProtect(&universe.mainCommandLayer, &selection, &protect);
        }
    }

    if (aisTeams[AIS_TEAM_ScoutEnemy].totalteamsize > 0)
    {
        budget = aisByLevel(0, 3, 6);
        attackers.numShips = 0;
        for (;;)
        {
            scout = aisTeamShipAt(n, scouts);
            if ((scout == NULL) || (budget <= 0))
            {
                break;
            }
            if (!cat_aisShipIsBusy(scout))
            {
                cat_selSelectionAddShip(&attackers, scout);
                budget--;
            }
            n++;
        }
        if (attackers.numShips > 0)
        {
            target.numTargets = 1;
            target.TargetPtr[0] = (TargetPtr)aisTeams[AIS_TEAM_ScoutEnemy].selection[0].sel.ShipPtr[0];
            clWrapAttack(&universe.mainCommandLayer, (SelectCommand *)&attackers, &target);
        }
    }

    if (aiCurrentAIPlayer->aiplayerDifficultyLevel == AI_ADVANCED)
    {
        for (node = universe.DerelictList.head; node != NULL; node = node->next)
        {
            derelict = (Derelict *)listGetStructOfNode(node);
            if (derelict->derelicttype == Crate)
            {
                target.numTargets = 1;
                target.TargetPtr[0] = (TargetPtr)derelict;
                scout = aisTeamShipAt(n, scouts);
                if (scout != NULL)
                {
                    selection.numShips = 1;
                    selection.ShipPtr[0] = scout;
                    clWrapAttack(&universe.mainCommandLayer, &selection, &target);
                }
                return;
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisTeamFindNearestSelection
    Description : CAT (0x00432d20): the group of a team whose centre is nearest a point, NULL for an empty team
----------------------------------------------------------------------------*/
AISTeamSelection *cat_aisTeamFindNearestSelection(sdword teamIndex, vector *pos)
{
    AISTeamEntry *team = &aisTeams[teamIndex];
    AISTeamSelection *best = NULL;
    real32 bestDist = AIS_FAR_AWAY, dist;
    vector diff;
    sdword i;

    for (i = 0; i < (sdword)team->numselections; i++)
    {
        vecSub(diff, team->selection[i].center, *pos);
        dist = getVectDistSloppy(diff);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = &team->selection[i];
        }
    }
    return best;
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisTeamAttack
    Description : CAT (0x00432df0): each idle group of a team (a parading group only once it has minSize ships)
                  attacks the nearest group of the current target team; after groupSize ships the next target team
                  is taken. An advanced player with hyperspace on may jump a group more than 10,000 away when it can
                  pay; else the group gets a random formation (setFormation), the aggressive stance (setStance), a
                  kamikaze order against the sixth target team (allowKamikaze) and aggressive tactics
----------------------------------------------------------------------------*/
void cat_aisTeamAttack(AISTeamEntry *team, sdword minSize, sdword groupSize, bool32 setFormation, bool32 setStance,
                       bool32 allowHyperspace, bool32 allowKamikaze, sdword numTargetTeams, const sdword *targetTeams)
{
    sdword i, targetIdx = 0, accumulated = 0, numShips;
    AISTeamSelection *mine, *nearest;
    Ship *lead, *targetShip;
    CommandToDo *command;
    vector diff, to;
    real32 dist;
    double cost;            // CAT (M2-R9 oracle-rung-physics): hyperspaceCost's double (0x00433170 adds it unrounded)
    TypeOfFormation formation;

    for (i = 0; i < (sdword)team->numselections; i++)
    {
        mine = &team->selection[i];
        numShips = mine->sel.numShips;
        if (numShips <= 0)
        {
            continue;
        }
        lead = mine->sel.ShipPtr[0];
        if (cat_aisShipIsBusy(lead))
        {
            continue;
        }
        command = lead->command;
        if ((command != NULL) && (command->ordertype.order == COMMAND_MILITARY_PARADE) && (minSize > numShips))
        {
            continue;
        }

        accumulated += numShips;
        if ((accumulated > groupSize) && (i != 0))
        {
            targetIdx++;
            accumulated = 0;
            if (targetIdx >= numTargetTeams)
            {
                targetIdx = numTargetTeams - 1;
            }
        }

        nearest = cat_aisTeamFindNearestSelection(catAisTargetTeam(targetTeams, targetIdx), &mine->center);
        while (nearest == NULL)
        {
            if (targetIdx >= numTargetTeams)
            {
                break;
            }
            targetIdx++;
            accumulated = 0;
            if (targetIdx < numTargetTeams)
            {
                nearest = cat_aisTeamFindNearestSelection(catAisTargetTeam(targetTeams, targetIdx), &mine->center);
            }
        }
        if (nearest == NULL)
        {
            continue;
        }

        if (bitTest(tpGameCreated.flag, MG_Hyperspace) && allowHyperspace &&
            (aiCurrentAIPlayer->aiplayerDifficultyLevel == AI_ADVANCED))
        {
            targetShip = nearest->sel.ShipPtr[0];
            vecSub(diff, lead->obj.posinfo.position, targetShip->obj.posinfo.position);
            dist = getVectDistSloppy(diff);
            if (dist > 10000.0f)
            {
                cost = hyperspaceCost(dist, (SelectCommand *)&mine->sel);
                // 0x00433170: cost + 10000 on the x87 (53 bits) against the RUs
                if (cost + 10000.0 < (double)aiCurrentAIPlayer->player->resourceUnits)
                {
                    to.x = targetShip->obj.posinfo.position.x + 500.0f;
                    to.y = targetShip->obj.posinfo.position.y + 500.0f;
                    to.z = targetShip->obj.posinfo.position.z + 100.0f;
                    clWrapMpHyperspace(&universe.mainCommandLayer, (SelectCommand *)&mine->sel, mine->center, to);
                    continue;
                }
            }
        }

        if (setFormation)
        {
            formation = (TypeOfFormation)(ranRandom(RANDOM_AI_PLAYER) % 5);
            if ((targetIdx == 9) || (targetIdx == 0x12))
            {
                formation = (TypeOfFormation)5;         // the target index, as the exe (never 9 or 18 with its lists)
            }
            clWrapFormation(&universe.mainCommandLayer, (SelectCommand *)&mine->sel, formation, 0);
        }
        if (setStance)
        {
            cat_clWrapMimic(&universe.mainCommandLayer, (SelectCommand *)&mine->sel, 2, 0, 0, 0);
        }
        if (allowKamikaze && (targetIdx == 5))
        {
            clWrapSetKamikaze(&universe.mainCommandLayer, (SelectCommand *)&mine->sel);
        }
        clWrapSetTactics(&universe.mainCommandLayer, (SelectCommand *)&mine->sel, Aggressive);
        clWrapAttack(&universe.mainCommandLayer, (SelectCommand *)&mine->sel, (AttackCommand *)&nearest->sel);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisMothershipUpdate
    Description : CAT (0x00433310): intermediate (40,000) and advanced (20,000) players keep their mothership that
                  far from the nearest enemy mothership, inside the world less 8000; while its own big gun is charged
                  (team 15, 0x0097ce5c) and no enemy DFG is near, an enemy mothership within 90,000 gets the big gun's
                  point order instead
----------------------------------------------------------------------------*/
void cat_aisMothershipUpdate(void)
{
    AISTeamEntry *own = &aisTeams[AIS_TEAM_Mothership];
    AISTeamEntry *enemies = &aisTeams[AIS_TEAM_MothershipEnemy];
    AISTeamSelection *nearest = NULL;
    Ship *mothership, *target;
    real32 bestDist = AIS_FAR_AWAY, dist;
    sdword standoff, i;
    vector diff, dir, dest;
    double limit;

    if (aiCurrentAIPlayer->aiplayerDifficultyLevel == AI_INTERMEDIATE)
    {
        standoff = 40000;
    }
    else if (aiCurrentAIPlayer->aiplayerDifficultyLevel == AI_ADVANCED)
    {
        standoff = 20000;
    }
    else
    {
        return;
    }
    if (own->totalteamsize <= 0)
    {
        return;
    }
    mothership = own->selection[0].sel.ShipPtr[0];
    for (i = 0; i < (sdword)enemies->numselections; i++)
    {
        vecSub(diff, enemies->selection[i].center, mothership->obj.posinfo.position);
        dist = getVectDistSloppy(diff);
        if (dist < bestDist)
        {
            bestDist = dist;
            nearest = &enemies->selection[i];
        }
    }
    if ((nearest == NULL) || (enemies->totalteamsize <= 0) || (aisOwnBigGunShip != NULL))
    {
        return;
    }
    target = nearest->sel.ShipPtr[0];

    if ((aisTeams[AIS_TEAM_BigGunCharged].totalteamsize > 0) && !aisEnemyDFGNearMothership)     // 0x004333fa
    {
        if (cat_vecDistance(&mothership->obj.posinfo.position, &target->obj.posinfo.position) < 90000.0f)
        {
            dest = target->obj.posinfo.position;
            cat_clWrapPointOrder(&universe.mainCommandLayer, (SelectCommand *)&own->selection[0].sel, &dest);
        }
        return;
    }

    if (cat_aisShipIsBusy(mothership))
    {
        return;
    }
    vecSub(dir, mothership->obj.posinfo.position, target->obj.posinfo.position);
    vecNormalize(&dir);
    dir.x *= (real32)standoff;
    dir.y *= (real32)standoff;
    dir.z *= (real32)standoff;
    dest.x = target->obj.posinfo.position.x + dir.x;
    dest.y = target->obj.posinfo.position.y + dir.y;
    dest.z = target->obj.posinfo.position.z + dir.z;

    // the clamp to the world less 8000, each bound computed unrounded on the x87; the y upper bound is the exe's x
    //      bound (0x00433595)
    limit = 8000.0 - (double)smUniverseSizeX;
    if ((double)dest.x < limit)
    {
        dest.x = (real32)limit;
    }
    else
    {
        limit = (double)smUniverseSizeX - 8000.0;
        if ((double)dest.x > limit)
        {
            dest.x = (real32)limit;
        }
    }
    limit = 8000.0 - (double)smUniverseSizeY;
    if ((double)dest.y < limit)
    {
        dest.y = (real32)limit;
    }
    else if ((double)smUniverseSizeY - 8000.0 < (double)dest.y)
    {
        dest.y = (real32)((double)smUniverseSizeX - 8000.0);
    }
    limit = 8000.0 - (double)smUniverseSizeZ;
    if ((double)dest.z < limit)
    {
        dest.z = (real32)limit;
    }
    else
    {
        limit = (double)smUniverseSizeZ - 8000.0;
        if ((double)dest.z > limit)
        {
            dest.z = (real32)limit;
        }
    }
    clWrapMove(&universe.mainCommandLayer, (SelectCommand *)&own->selection[0].sel, 0, mothership->obj.posinfo.position, 1, &dest);
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisCarrierUpdate
    Description : CAT (0x00433620): with no processor the first carrier follows the Workers
----------------------------------------------------------------------------*/
void cat_aisCarrierUpdate(void)
{
    if ((aisTeams[AIS_TEAM_Processor].totalteamsize == 0) && (aisTeams[AIS_TEAM_Carrier].totalteamsize > 0))
    {
        aisResourceShipUpdate(aisTeams[AIS_TEAM_Carrier].selection[0].sel.ShipPtr[0]);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisSentinelsUpdate
    Description : CAT (0x00433760): a group of 4..12 Sentinels protects the dearest medium ship in formation 10. The
                  exe gives the order only when one of the group already protects that ship in formation 10 (the
                  test at 0x0043386d is the other way round from what the loop suggests); kept as the exe
----------------------------------------------------------------------------*/
void cat_aisSentinelsUpdate(void)
{
    AISTeamEntry *sentinels = &aisTeams[AIS_TEAM_Sentinel];
    AISTeamEntry *medium = &aisTeams[AIS_TEAM_MediumCombat];
    AISTeamSelection *group;
    Ship *dearest, *ship;
    CommandToDo *command;
    ProtectCommand protect;
    bool32 noneProtecting;
    sdword g, i, j, n;

    for (g = 0; g < (sdword)sentinels->numselections; g++)
    {
        group = &sentinels->selection[g];
        n = group->sel.numShips;
        if ((n <= 3) || (n > 12))
        {
            continue;
        }
        dearest = NULL;
        for (i = 0; i < (sdword)medium->numselections; i++)
        {
            for (j = 0; j < medium->selection[i].sel.numShips; j++)
            {
                ship = medium->selection[i].sel.ShipPtr[j];
                if ((dearest == NULL) ||
                    (((ShipStaticInfo *)ship->staticinfo)->buildCost > ((ShipStaticInfo *)dearest->staticinfo)->buildCost))
                {
                    dearest = ship;
                }
            }
        }
        if (dearest == NULL)
        {
            continue;
        }
        noneProtecting = TRUE;
        for (i = 0; i < group->sel.numShips; i++)
        {
            command = group->sel.ShipPtr[i]->command;
            if ((command != NULL) && bitTest(command->ordertype.attributes, COMMAND_MASK_PROTECTING) &&
                ShipInSelection((SelectCommand *)command->protect, dearest) &&
                bitTest(command->ordertype.attributes, COMMAND_MASK_FORMATION) &&
                (command->formation.formationtype == (TypeOfFormation)10))
            {
                noneProtecting = FALSE;
            }
        }
        if (!noneProtecting)
        {
            protect.numShips = 1;
            protect.ShipPtr[0] = dearest;
            clWrapProtect(&universe.mainCommandLayer, (SelectCommand *)&group->sel, &protect);
            clWrapFormation(&universe.mainCommandLayer, (SelectCommand *)&group->sel, (TypeOfFormation)10, 0);
        }
    }
}

// idle Leeches from index *n on into a selection until it holds limit, then their special order on the targets
static void aisLeechesSend(sdword *n, AISTeamSelection *targets, sdword limit)
{
    MaxSelection leeches;
    Ship *leech;

    leeches.numShips = 0;
    for (;;)
    {
        leech = aisTeamShipAt(*n, &aisTeams[AIS_TEAM_Leech]);
        if ((leech == NULL) || (leeches.numShips >= limit))
        {
            break;
        }
        if (!cat_aisShipIsBusy(leech))
        {
            cat_selSelectionAddShip(&leeches, leech);
        }
        (*n)++;
    }
    if (leeches.numShips > 0)
    {
        clWrapSpecial(&universe.mainCommandLayer, (SelectCommand *)&leeches, (SpecialCommand *)&targets->sel);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisLeechesUpdate
    Description : CAT (0x004338e0): an advanced player sends idle Leeches, two per ship, to the enemy Workers nearest
                  its mothership, then ten per ship to the nearest enemy carriers; every player sends the rest to the
                  nearest enemy mothership
----------------------------------------------------------------------------*/
void cat_aisLeechesUpdate(void)
{
    Ship *mothership = aiCurrentAIPlayer->player->PlayerMothership;
    AISTeamSelection *targets;
    sdword n = 0;

    if (aiCurrentAIPlayer->aiplayerDifficultyLevel == AI_ADVANCED)
    {
        targets = cat_aisTeamFindNearestSelection(AIS_TEAM_WorkerEnemy, &mothership->obj.posinfo.position);
        if (targets != NULL)
        {
            aisLeechesSend(&n, targets, targets->sel.numShips * 2);
        }
        targets = cat_aisTeamFindNearestSelection(AIS_TEAM_CarrierEnemy, &mothership->obj.posinfo.position);
        if (targets != NULL)
        {
            aisLeechesSend(&n, targets, targets->sel.numShips * 10);
        }
    }
    targets = cat_aisTeamFindNearestSelection(AIS_TEAM_MothershipEnemy, &mothership->obj.posinfo.position);
    if (targets != NULL)
    {
        aisLeechesSend(&n, targets, COMMAND_MAX_SHIPS);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisReset
    Description : CAT (0x00433d30): a new game clears the 8 play timers and the alliance flag (gameStart 0x005a9290)
----------------------------------------------------------------------------*/
void cat_aisReset(void)
{
    sdword i;

    for (i = 0; i < MAX_MULTIPLAYER_PLAYERS; i++)
    {
        aisLastPlayTime[i] = 0.0f;
    }
    aisAlliancesSet = 0;
}

/*-----------------------------------------------------------------------------
    Name        : cat_aisPlay
    Description : CAT (0x00433d50): the current computer player's skirmish update, every 20 / 10 / 5 seconds of
                  universe time (its play timer): alliances once, the teams, research, the fleet (20% / 50% / 100% of
                  the updates), Workers, the processor, scouts, the DFG jump at a charged big gun, Leeches, the five
                  attacks, the mothership, the carrier, the Sentinels
----------------------------------------------------------------------------*/
void cat_aisPlay(void)
{
    Player *player = aiCurrentAIPlayer->player;
    Ship *mothership;
    real32 interval, chance;
    udword roll;
    sdword minSize, groupSize;
    vector dest;
    SelectCommand dfg;

    switch (aiCurrentAIPlayer->aiplayerDifficultyLevel)
    {
        case AI_BEGINNER: chance = 20.0f;  interval = 20.0f; break;
        case AI_INTERMEDIATE: chance = 50.0f;  interval = 10.0f; break;
        case AI_ADVANCED: chance = 100.0f; interval = 5.0f;  break;
        default:     chance = 100.0f; interval = 0.0f;  break;
    }
    if (player->PlayerMothership == NULL)
    {
        return;
    }
    // 0x00433db6: the elapsed time is taken unrounded on the x87
    if (!((double)universe.totaltimeelapsed - (double)aisLastPlayTime[player->playerIndex] > (double)interval))
    {
        return;
    }
    if (aisFirstPlay)
    {
        aisFirstPlay = FALSE;
    }
    aisLastPlayTime[player->playerIndex] = universe.totaltimeelapsed;
    if (aisAlliancesSet == 0)
    {
        cat_aisSetupAlliances();
        aisAlliancesSet++;
    }
    cat_aisUpdateTeams();
    cat_aisResearchUpdate();
    roll = ranRandom(RANDOM_AI_PLAYER) % 100 + 1;
    if ((real32)roll < chance)
    {
        aisFleetUpdate();
    }
    cat_aisWorkersUpdate();
    cat_aisProcessorUpdate();
    cat_aisScoutsUpdate();

    // an advanced player with hyperspace on jumps its DFG frigates (1 in 10) next to a charged enemy big gun
    if (bitTest(tpGameCreated.flag, MG_Hyperspace) && (aiCurrentAIPlayer->aiplayerDifficultyLevel == AI_ADVANCED) &&
        (aisEnemyBigGunChargedShip != NULL) && (aisTeams[AIS_TEAM_DFGFrigate].totalteamsize > 0) &&
        (aiCurrentAIPlayer->player->resourceUnits > 20000) && ((ranRandom(RANDOM_AI_PLAYER) % 100 + 1) < 10))
    {
        dest.x = aisEnemyBigGunChargedShip->obj.posinfo.position.x + 300.0f;
        dest.y = aisEnemyBigGunChargedShip->obj.posinfo.position.y + 300.0f;
        dest.z = aisEnemyBigGunChargedShip->obj.posinfo.position.z + 300.0f;
        if (cat_vecDistance(&dest, &aisTeams[AIS_TEAM_DFGFrigate].selection[0].center) > 2000.0f)
        {
            dfg.numShips = 1;
            dfg.ShipPtr[0] = aisTeams[AIS_TEAM_DFGFrigate].selection[0].sel.ShipPtr[0];
            clWrapMpHyperspace(&universe.mainCommandLayer, &dfg, aisTeams[AIS_TEAM_DFGFrigate].selection[0].center, dest);
        }
    }

    cat_aisLeechesUpdate();

    // fighters (a ship other than a mothership as the player's mothership, e.g. a carrier, makes groups of 5)
    switch (aiCurrentAIPlayer->aiplayerDifficultyLevel)
    {
        case AI_BEGINNER: minSize = 10; groupSize = 10; break;
        case AI_INTERMEDIATE: minSize = 10; groupSize = 20; break;
        case AI_ADVANCED: minSize = 10; groupSize = 30; break;
        default:     minSize = 0;  groupSize = 0;  break;
    }
    mothership = aiCurrentAIPlayer->player->PlayerMothership;
    if ((mothership != NULL) && (mothership->shiptype != sMothership) && (mothership->shiptype != bMothership))
    {
        minSize = 5;
    }
    cat_aisTeamAttack(&aisTeams[AIS_TEAM_Fighter], minSize, groupSize, TRUE, FALSE, FALSE,
                      aiCurrentAIPlayer->aiplayerDifficultyLevel == AI_ADVANCED, 8, aisFighterTargets);

    // mimics
    minSize = 10;
    mothership = aiCurrentAIPlayer->player->PlayerMothership;
    if ((mothership != NULL) && (mothership->shiptype != sMothership) && (mothership->shiptype != bMothership))
    {
        minSize = 4;
    }
    cat_aisTeamAttack(&aisTeams[AIS_TEAM_Mimic], minSize, 10, TRUE, TRUE, FALSE, FALSE, 4, aisMimicTargets);

    // cruise missiles
    cat_aisTeamAttack(&aisTeams[AIS_TEAM_CruiseMissile], 3, 5, FALSE, FALSE, FALSE, FALSE, 5, aisCruiseMissileTargets);

    // anti-fighter ships
    switch (aiCurrentAIPlayer->aiplayerDifficultyLevel)
    {
        case AI_BEGINNER: minSize = 3; groupSize = 1;  break;
        case AI_INTERMEDIATE: minSize = 3; groupSize = 5;  break;
        case AI_ADVANCED: minSize = 3; groupSize = 10; break;
        default:     minSize = 0; groupSize = 0;  break;
    }
    cat_aisTeamAttack(&aisTeams[AIS_TEAM_AntiFighter], minSize, groupSize, FALSE, FALSE, FALSE, FALSE, 6,
                      aisAntiFighterTargets);

    // medium ships (hyperspace allowed)
    switch (aiCurrentAIPlayer->aiplayerDifficultyLevel)
    {
        case AI_BEGINNER: minSize = 1; groupSize = 1;  break;
        case AI_INTERMEDIATE: minSize = 1; groupSize = 3;  break;
        case AI_ADVANCED: minSize = 1; groupSize = 10; break;
        default:     minSize = 0; groupSize = 0;  break;
    }
    cat_aisTeamAttack(&aisTeams[AIS_TEAM_MediumCombat], minSize, groupSize, FALSE, FALSE, TRUE, FALSE, 4, aisMediumTargets);

    cat_aisMothershipUpdate();
    cat_aisCarrierUpdate();
    cat_aisSentinelsUpdate();
}
