// =============================================================================
//  AITeam.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/31/1998 by dstone
// =============================================================================

#include "AITeam.h"
#include "SaveCat32.h"

#include "AIFeatures.h"
#include "AIFleetMan.h"
#include "AIHandler.h"
#include "AIMoves.h"
#include "AIPlayer.h"
#include "CommandDefs.h"
#include "GravWellGenerator.h"
#include "SaveGame.h"
#include "Select.h"
#include "Tactics.h"


/*-----------------------------------------------------------------------------
    Name        : aitCreate
    Description : Creates a new team instance
    Inputs      : teamType - the type of team to create
    Outputs     : Allocates memory for a team
    Return      : the new team created
----------------------------------------------------------------------------*/
AITeam *aitCreate(TeamType teamType)
{
    AITeam *team;

    team           = memAlloc(sizeof(AITeam), "aiteam", 0);
    team->teamType = teamType;

    team->aiplayerowner = aiCurrentAIPlayer;
    growSelectInit(&team->shipList);
    listInit(&team->moves);

    team->teamFlags             = 0x00000000;
    team->newships              = 0;
    team->curMove               = NULL;
    team->custTeamInfo          = NULL;
    team->TeamDiedCB            = NULL;
    team->teamStrength          = 0;
    team->teamValue             = 0;
    team->teamDifficultyLevel   = (AITeamLevel)2*aiCurrentAIPlayer->aiplayerDifficultyLevel;
    team->cooperatingTeam       = NULL;
    team->cooperatingTeamDiedCB = NULL;
    team->msgQueue              = NULL;
    team->msgSender             = NULL;

    team->kasLabel[0]           = 0;
    team->kasFSMName[0]         = 0;
    team->kasStateName[0]       = 0;
    team->kasFSMWatchFunction   = NULL;
    team->kasStateWatchFunction = NULL;
    // CAT: a multiplayer game's teams default to aggressive tactics (aitCreate 0x004168d6)
    team->kasTactics            = singlePlayerGame ? Neutral : Aggressive;
    team->kasFormation          = NO_FORMATION;

    // keep track of all allocations
    if (team->aiplayerowner->teamsUsed >= team->aiplayerowner->teamsAllocated)
    {
        // allocate more if necessary
        team->aiplayerowner->teams = memRealloc(team->aiplayerowner->teams, sizeof(AITeam *) * (team->aiplayerowner->teamsAllocated + AITEAM_ALLOC_INCREMENT), "aiteamlist", 0);
        team->aiplayerowner->teamsAllocated += AITEAM_ALLOC_INCREMENT;
    }
    team->aiplayerowner->teams[team->aiplayerowner->teamsUsed++] = team;

    switch (team->teamDifficultyLevel)
    {
        case TEAM_ADVANCED:
            aiuEnableTeamFeature(team, AIT_ADVANCED_ATTACK);
        case TEAM_INTERMEDIATE_ADVANCED:
            aiuEnableTeamFeature(team, AIT_FLANK_ATTACK);
            aiuEnableTeamFeature(team, AIT_CLOAKING);
            aiuEnableTeamFeature(team, AIT_GRAVWELL);
        case TEAM_INTERMEDIATE:
            aiuEnableTeamFeature(team, AIT_TACTICS);
        case TEAM_BEGINNER_INTERMEDIATE:
        case TEAM_BEGINNER:
            break;
        default:
            break;
    }

    team->teamDelay = AIT_TEAM_MOVE_DELAY[team->teamDifficultyLevel];

    return team;
}


/*-----------------------------------------------------------------------------
    Name        : aitFindMoveOfType
    Description : Returns a move of the type specified
    Inputs      : team - the team to find the move for, type - the movetype to stop at
    Outputs     : Deletes a bunch of moves
    Return      : The move or NULL
----------------------------------------------------------------------------*/
AITeamMove *aitFindMoveOfType(AITeam *team, AIMoveTypes type)
{
    AITeamMove *move = team->curMove;
    Node *nextnode = move->listNode.next;

    while ((move->type != type) && (nextnode != NULL))
    {
        move     = (AITeamMove *)listGetStructOfNode(nextnode);
        nextnode = move->listNode.next;
    }

    if (move->type != type)
    {
        return NULL;
    }
    else
    {
        return move;
    }
}


/*-----------------------------------------------------------------------------
    Name        : aitDeleteCurrentMove
    Description : Delete's the team's current move
    Inputs      : team - the team
    Outputs     : delete's the team's current move
    Return      : nothing
----------------------------------------------------------------------------*/
void aitDeleteCurrentMove(AITeam *team)
{
    AITeamMove *move = team->curMove;
    Node *nextnode = move->listNode.next;

    if (move->moveCloseFunction != NULL)
    {
        move->moveCloseFunction(team,move);
//        aiplayerLog((aiIndex, "%x Deleting Move in aitDeleteCurrentMove", team));
    }

    listDeleteNode(&move->listNode);

    team->curMove = (AITeamMove *)listGetStructOfNode(nextnode);
}


/*-----------------------------------------------------------------------------
    Name        : aitDeleteMovesUntilMoveType
    Description : Deletes all moves starting at current move until a move of type "type" is found
    Inputs      : team - the team to delete the moves from, type - the movetype to stop at
    Outputs     : Deletes a bunch of moves
    Return      : void
----------------------------------------------------------------------------*/
void aitDeleteMovesUntilMoveType(AITeam *team, AIMoveTypes type)
{
    AITeamMove *move = team->curMove;
    Node *nextnode = move->listNode.next;

    while ((move->type != type) && (nextnode != NULL))
    {
        listDeleteNode(&move->listNode);
        move     = (AITeamMove *)listGetStructOfNode(nextnode);
        nextnode = move->listNode.next;
    }

    if (move->type != type)
    {
        //nextnode == NULL
        team->curMove = NULL;
    }
    else
    {
        team->curMove = move;
    }
}



void aitDeleteAllTeamMoves(AITeam *team)
{
    Node *node;
    Node *nextnode;
    AITeamMove *move;

    node = team->moves.head;
    while (node != NULL)
    {
        move = (AITeamMove *)listGetStructOfNode(node);
        nextnode = node->next;

        if (move->moveCloseFunction != NULL)
        {
            move->moveCloseFunction(team,move);
//            aiplayerLog((aiIndex, "%x Deleting Move in aitDeleteAllTeamMoves", team));
        }

        listDeleteNode(node);
        node = nextnode;
    }
    dbgAssertOrIgnore(team->moves.num == 0);

    team->curMove = NULL;
}


void aitSpecialDefenseCoopTeamDiedCB(AITeam *team)
{
    AITeamMove *thisMove = team->curMove, *newMove;

    if (team->shipList.selection->numShips)
        team->cooperatingTeam = aitFindGoodCoopTeam(team->shipList.selection->ShipPtr[0]->shiptype);
    else
        team->cooperatingTeam = NULL;

    if (team->cooperatingTeam)
    {
        // CAT: either race's cloak generator (aitSpecialDefenseCoopTeamDiedCB 0x00416a10: 0x3f / 0x4d)
        if (aitTeamShipTypeIs(kCloakGenerator, team) || aitTeamShipTypeIs(tCloakGenerator, team))
        {
            bitSet(team->cooperatingTeam->teamFlags, TEAM_CloakCoop);
            team->cooperatingTeam->cooperatingTeam = team;
            team->cooperatingTeam->cooperatingTeamDiedCB = GenericCooperatingTeamDiedCB;
        }

        //later - maybe set wait to FALSE, and have this function scan for nearby
        //        cloakables (for the cloak generator) and cloak when appropriate
        newMove = aimCreateGuardCooperatingTeamNoAdd(team, TRUE, FALSE);
        newMove->events = thisMove->events;
        aitAddmoveBeforeAndMakeCurrent(team, newMove, thisMove);
    }
}

void GenericCooperatingTeamDiedCB(AITeam *team)
{
    team->cooperatingTeam = NULL;
}

/*=============================================================================
    Some Save Game here
=============================================================================*/

void PreFixCooperatingTeamDiedCB(AITeam *team)
{
    if (team->cooperatingTeamDiedCB == aitSpecialDefenseCoopTeamDiedCB)
    {
        team->cooperatingTeamDiedCB = (void *)1;
    }
    else if (team->cooperatingTeamDiedCB == GenericCooperatingTeamDiedCB)
    {
        team->cooperatingTeamDiedCB = (void *)2;
    }
    else
    {
        team->cooperatingTeamDiedCB = 0;
    }
}

void FixCooperatingTeamDiedCB(AITeam *team)
{
    switch ((sdword)team->cooperatingTeamDiedCB)
    {
        case 1:
            team->cooperatingTeamDiedCB = aitSpecialDefenseCoopTeamDiedCB;
            break;

        case 2:
            team->cooperatingTeamDiedCB = GenericCooperatingTeamDiedCB;
            break;

        case 0:
        default:
            team->cooperatingTeamDiedCB = NULL;
            break;
    }
}


/*=============================================================================
    End of Some Save Game stuff
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : aitDestroy
    Description : Destroys a team instance
    Inputs      : aiplayer - the player the team belongs to
                  team - the team to destroy
    Outputs     : Deallocates memory being used by the team, modifies a few structures
    Return      : void
----------------------------------------------------------------------------*/
void aitDestroy(struct AIPlayer *aiplayer, AITeam *team, bool32 removeAllReferencesToTeam)
{
    sdword i = 0;
    sdword foundteam = -1;

    aiplayerLog((aiIndex, "%x destroying team", team));

    //find the array slot for the current team
    for (i = 0; i < aiplayer->teamsUsed; i++)
    {
        if (aiplayer->teams[i] == team)
        {
            foundteam = i;
        }
        else
        {
            if (aiplayer->teams[i]->cooperatingTeam == team)
            {
                if (aiplayer->teams[i]->cooperatingTeamDiedCB)
                {
                    aiplayer->teams[i]->cooperatingTeamDiedCB(aiplayer->teams[i]);
                }
                aiplayer->teams[i]->cooperatingTeam = NULL;
            }
        }
    }

    //make sure we found the array slot
    dbgAssertOrIgnore(foundteam != -1);

     //if there are still ship in the team, put them back in the newships list
    if (removeAllReferencesToTeam && team->shipList.selection && team->shipList.selection->numShips)
    {
          while (team->shipList.selection->numShips)
          {
               growSelectAddShipNoDuplication(&aiplayer->newships, team->shipList.selection->ShipPtr[0]);
                growSelectRemoveShipIndex(&team->shipList, 0);
          }
     }

    //delete the team
    growSelectClose(&team->shipList);

    aitDeleteAllTeamMoves(team);

    aitMsgQueueFree(team);              // CAT: the queue is the team's own (aitDestroy 0x00416c5e clears the pointer)

    if (team->TeamDiedCB)
        team->TeamDiedCB(team);

    aifTeamDied(aiplayer, team, removeAllReferencesToTeam);

    //insert a valid team into the now empty slot
    if (aiplayer->teamsUsed > 1)
    {
        aiplayer->teams[foundteam] = aiplayer->teams[aiplayer->teamsUsed - 1];
    }

    if (singlePlayerGame)
    {
        kasTeamDied(team);
    }

    memFree(team);

    aiplayer->teamsUsed--;
}


/*-----------------------------------------------------------------------------
    Name        : aitInit
    Description : Initializes the team related structures in the aiplayer structure
    Inputs      : aiplayer - the computer player to do the initializing in
    Outputs     : sets some pointers, sets some variables to zero.  You know, the usual stuff
    Return      : void
----------------------------------------------------------------------------*/
void aitInit(struct AIPlayer *aiplayer)
{
    aiplayer->teams          = memAlloc(sizeof(AITeam*)*AITEAM_ALLOC_INITIAL, "aiteamlist", 0);
    aiplayer->teamsAllocated = AITEAM_ALLOC_INITIAL;
    aiplayer->teamsUsed      = 0;
}



/*-----------------------------------------------------------------------------
    Name        : aitClose
    Description : Removes all references of teams from the game.
                  Full deallocation type thingy
    Inputs      : aiplayer - the player who's teams this function annihilate
    Outputs     : Deallocates lots and lots of memory
    Return      : void
----------------------------------------------------------------------------*/
void aitClose(struct AIPlayer *aiplayer)
{
    AITeam *team;
    udword i;

    //deallocate any team that's been created
    for (i = 0; i < aiplayer->teamsUsed; i++)
    {
        team = aiplayer->teams[i];

        growSelectClose(&team->shipList);   //deallocate the team's ships
        listDeleteAll(&team->moves);        //deallocate the team's moves
        memFree(team);                      //deallocate the team structure
    }
    memFree(aiplayer->teams);
}


void aitShipDied(struct AIPlayer *aiplayer,ShipPtr ship)
{
    udword i;
    AITeam *team;
    Node *node;
    AITeamMove *move;

//    if ((ship->playerowner == aiplayer->player) || (ship->obj.attributes & ATTRIBUTES_Defector))  // do all checks for defector
//    {
        // go through all teams & ships
        for (i = 0; i < aiplayer->teamsUsed; i++)
        {
            team = aiplayer->teams[i];
            if (growSelectRemoveShip(&team->shipList, ship))
            {
                //later maybe add callback checking
                //later if team is dead, what do we do?
                //Note: aievents has a teamdied handler as well for each move

                //recalculate the strength and value of the team
                if ((aiplayer->numLeaders) && (bitTest(ship->obj.attributes, ATTRIBUTES_TeamLeader)) &&
                    (team->teamDifficultyLevel > 0))
                {
                    if (!aitCheckForLeaderAndMoveToFront(team))
                    {
                        team->teamDifficultyLevel--;
                    }
                    aiplayer->numLeaders--;
                }
                aitSetTeamStrengthValue(team);
//                aiplayerLog((aiplayer->player->playerIndex,"Dead ship removed from team %x shiplist", team));
//                break;      // assuming a ship can only be in 1 team
            }
        }
//    }

    // second pass: go through all team's moves:
    for (i = 0; i < aiplayer->teamsUsed; i++)
    {
        team = aiplayer->teams[i];

        node = team->moves.head;
        while (node != NULL)
        {
            move = (AITeamMove *)listGetStructOfNode(node);

            if (move->moveShipDiedFunction)
            {
                move->moveShipDiedFunction(team,move,ship);
            }

            node = node->next;
        }
    }
}


void aitResourceDied(struct AIPlayer *aiplayer, Resource *resource)
{
    udword i;
    AITeam *team;
    Node *node;
    AITeamMove *move;

    for (i=0; i < aiplayer->teamsUsed; i++)
    {
        team = aiplayer->teams[i];

        node = team->moves.head;
        while (node != NULL)
        {
            move = (AITeamMove *)listGetStructOfNode(node);

            if (move->type == MOVE_RESVOLUME)
            {
                move->moveResourceDiedFunction(team, move, resource);
            }
            node = node->next;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : aitCheckForLeaderAndMoveToFront
    Description : Checks to see if the team has a leader, and moves it to the front of the selection of there is one
    Inputs      : team - the team to check
    Outputs     : Move some ships around the selection
    Return      : TRUE if a leader is found
----------------------------------------------------------------------------*/
bool32 aitCheckForLeaderAndMoveToFront(AITeam *team)
{
    ShipPtr ship;
    udword i;

    for (i=0;i<team->shipList.selection->numShips;i++)
    {
        ship = team->shipList.selection->ShipPtr[i];

        if (bitTest(ship->obj.attributes, ATTRIBUTES_TeamLeader))
        {
            //found a leader, now move him to the front of the selection
            team->shipList.selection->ShipPtr[i] = team->shipList.selection->ShipPtr[0];
            team->shipList.selection->ShipPtr[0] = ship;
            return TRUE;
        }
    }
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : aitCheckAmIBeingWatched
    Description : Checks to see if the player is watching this team
    Inputs      : team - the team being checked, sel - the selection being checked explicitly
    Outputs     :
    Return      : TRUE if the team is being watched
----------------------------------------------------------------------------*/
bool32 aitCheckAmIBeingWatched(AITeam *team, SelectCommand *sel)
{
    if (bitTest(team->teamFlags, TEAM_AmIBeingWatched))
    {
        bitClear(team->teamFlags, TEAM_AmIBeingWatched);
        if (sel->numShips && bitTest(sel->ShipPtr[0]->obj.renderedLODs, 0x7))
        {
            return TRUE;
        }
    }
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : aitSetAmIBeingWatched
    Description : Sets up the team for  checking if it's being watched
    Inputs      : team - the team being setup,
                  sel - the selection being checked explicitly
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aitSetAmIBeingWatched(AITeam *team, SelectCommand *sel)
{
    if (sel->numShips)
    {
        //flag the team as one checking if it's being watched
        bitSet(team->teamFlags, TEAM_AmIBeingWatched);

        //clear the RenderedLODs bitfield in one of the selections ships.
        bitClear(sel->ShipPtr[0]->obj.renderedLODs, 0x7);
    }
}



/*-----------------------------------------------------------------------------
    Name        : aitAddShip
    Description : Adds a ship to the team
    Inputs      : team - the team to add the ship to,
                  ship - the ship to add
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aitAddShip(AITeam *team, ShipPtr ship)
{
    growSelectAddShip(&team->shipList,ship);
    aitSetTeamHomoHetero(team);
    aitSetTeamSpecialFlags(team);
    aitSetTeamStrengthValue(team);
    team->newships++;

    //leader code
    //if there is a leader in the computer player's list of ships
    if (aiCurrentAIPlayer->numLeaders)
    {
        //if this new ship is a leader, increase the level of difficulty
        //of the team
        if (bitTest(ship->obj.attributes, ATTRIBUTES_TeamLeader) &&
            (team->teamDifficultyLevel != TEAM_ADVANCED))
        {
           team->teamDifficultyLevel++;
        }

        //make sure that the leader is actually in front of the formation
        aitCheckForLeaderAndMoveToFront(team);
    }
}

void aitCheckShips(AITeam *team, ShipPtr ship)
{
    udword i;

    for (i=0;i<aiCurrentAIPlayer->teamsUsed;i++)
    {
        if ((aiCurrentAIPlayer->teams[i] != team) && ShipInSelection(aiCurrentAIPlayer->teams[i]->shipList.selection, ship))
        {
            dbgAssertOrIgnore(FALSE);
        }
    }
}


// CAT (M2-R7 ai-orders-moves): no P2 swarm team helpers (aitMoveSwarmShipDefenseToAttack, aitMoveAllSwarmShips*,
//      aitFindNewPod, aitMoveSwarmersToNewPod, aitAllDefenseSwarmersFull): their only callers were the P2 swarm moves,
//      which cataclysm.exe does not have


//
// returns TRUE if removed ship
//
bool32 aitRemoveShip(AITeam *team, ShipPtr ship)
{
    return growSelectRemoveShip(&team->shipList,ship);
}


/*=============================================================================
    Special Team Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : aitSetTeamSpecialFlags
    Description : Analyses the team and sets the team's flags -
                  TEAM_CloakFighters, TEAM_CloakGenerator, TEAM_GravWellGenerator
    Inputs      : team - the team
    Outputs     : Sets some flags
    Return      : void
----------------------------------------------------------------------------*/
void aitSetTeamSpecialFlags(AITeam *team)
{
    //set special flags
    if (!team->shipList.selection->numShips)
    {
        return;
    }

    if ((!bitTest(team->teamFlags, TEAM_CloakFighters)) &&
        (team->shipList.selection->ShipPtr[(team->shipList.selection->numShips-1)]->shiptype == bCloakedFighter))
    {
        bitSet(team->teamFlags, TEAM_CloakFighters);
    }
    // CAT: either race's cloak generator (inlined in aitAddShip 0x00416fc8: 0x3f / 0x4d)
    else if ((!bitTest(team->teamFlags, TEAM_CloakGenerator)) &&
             ((team->shipList.selection->ShipPtr[(team->shipList.selection->numShips-1)]->shiptype == kCloakGenerator) ||
              (team->shipList.selection->ShipPtr[(team->shipList.selection->numShips-1)]->shiptype == tCloakGenerator)))
    {
        bitSet(team->teamFlags, TEAM_CloakGenerator);
    }
/*    else if ((!bitTest(team->teamFlags, TEAM_GravWellGenerator)) &&
             (team->shipList.selection->ShipPtr[(team->shipList.selection->numShips-1)]->shiptype == GravWellGenerator))
    {
        bitSet(team->teamFlags, TEAM_GravWellGenerator);
    }
    */
}


/*-----------------------------------------------------------------------------
    Name        : aitProcessSpecialTeamCloakFighters
    Description : Processes cloak fighters
    Inputs      : team, curMove
    Outputs     : May do some stuff... it may even create some other stuff as well
    Return      : void
----------------------------------------------------------------------------*/
void aitProcessSpecialTeamCloakFighters(AITeam *team, AITeamMove *curMove)
{
    switch (curMove->type)
    {
        case MOVE_MOVETEAM:
        case MOVE_MOVETEAMINDEX:
            //maybe have a special function here to determine if the
            //move is dangerous or not
        case MOVE_SHIPRECON:
        case MOVE_ATTACK:
        case MOVE_ADVANCEDATTACK:
            aiuWrapSpecial(team->shipList.selection, NULL);

            bitClear(team->teamFlags, TEAM_CloakFighters);
            bitSet(team->teamFlags, TEAM_CloakedFighters);
            break;
        default:
            break;
    }
}


/*-----------------------------------------------------------------------------
    Name        : aitProcessSpecialTeamCloakedFighters
    Description : Processes cloak fighters that are already cloaked
    Inputs      : team, curMove
    Outputs     : May do some stuff... it may even create some other stuff as well
    Return      : void
----------------------------------------------------------------------------*/
void aitProcessSpecialTeamCloakedFighters(AITeam *team, AITeamMove *curMove)
{
    switch (curMove->type)
    {
        case MOVE_TEMPGUARD:
            aiuWrapSpecial(team->shipList.selection, NULL);

            bitClear(team->teamFlags, TEAM_CloakedFighters);
            bitSet(team->teamFlags,  TEAM_CloakFighters);
            break;
        default:
            break;
    }
}


// CAT: no cloak generator processing (aitProcessSpecialTeamCloakGenerator / CloakingGenerator) and no
//      aitOptimizeGravWellGenerator / aitOptimizeCloakGenerator: cataclysm.exe has none of them (its aitExecute
//      0x00417bf0 handles the cloaking fighters only); M2-R7 ai-player-skirmish removed them

/*-----------------------------------------------------------------------------
    Name        : aitProcessSpecialTeam
    Description : Processes the special teams, i.e. cloaking team
    Inputs      : team, curMove
    Outputs     : May do some stuff...  May even create moves
    Return      : void
----------------------------------------------------------------------------*/
void aitProcessSpecialTeam(AITeam *team, AITeamMove *curMove)
{
    // CAT: the cloaking fighters only (inlined in aitExecute 0x00417bf0); HW1's cloak generator and cloak cooperation
    //      branches are gone
    if ((aiuTeamFeatureEnabled(team, AIT_CLOAKING)) &&
        (bitTest(team->teamFlags, TEAM_CloakFighters)))
    {
        aitProcessSpecialTeamCloakFighters(team, curMove);
    }
    else if ((aiuTeamFeatureEnabled(team, AIT_CLOAKING)) &&
             (bitTest(team->teamFlags, TEAM_CloakedFighters)))
    {
        aitProcessSpecialTeamCloakedFighters(team, curMove);
    }
}


/*=============================================================================
    General Utility Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : aitAddmoveBeforeAndMakeCurrent
    Description : Adds a move before the current one and makes the new move current
    Inputs      : team - the team to add the new move to,
                  newMove - the new move,
                  thisMove - the current move
    Outputs     : Some linked list manipulation and may change the formation of
                  the team
    Return      : void
----------------------------------------------------------------------------*/
void aitAddmoveBeforeAndMakeCurrentNoSpecial(AITeam *team, AITeamMove *newMove, AITeamMove *thisMove)
{
    listAddNodeBefore(&(thisMove->listNode), &(newMove->listNode), newMove);
    team->curMove = newMove;
}

void aitAddmoveBeforeAndMakeCurrent(AITeam *team, AITeamMove *newMove, AITeamMove *thisMove)
{
    //if the tactics setting of the new move is different from the
    //old move, set tactics
    if ((newMove->tactics != thisMove->tactics)&&(team->shipList.selection->numShips))
    {
        aiuWrapSetTactics(team->shipList.selection,newMove->tactics);
    }

    if (newMove->formation == SAME_FORMATION)
    {
//        if (thisMove->formation < NO_FORMATION)
//        {
            newMove->formation = thisMove->formation;
//        }
//        else
//        {
//            newMove->formation = AIT_DEFAULT_FORMATION;
//        }
    }
    else if ((newMove->formation != NO_FORMATION) &&
             (newMove->formation != thisMove->formation) &&
             (team->shipList.selection->numShips > 1))
    {
        aiuWrapFormation(team->shipList.selection,newMove->formation);
    }
    aitAddmoveBeforeAndMakeCurrentNoSpecial(team, newMove, thisMove);
}


/*-----------------------------------------------------------------------------
    Name        : aitFindNumTeamsWithFlag
    Description : Returns the number of teams with a certain flag set
    Inputs      : flag - the flag referred to in description
    Outputs     : none
    Return      : number of teams with the flag set
----------------------------------------------------------------------------*/
udword aitFindNumTeamsWithFlag(udword flag)
{
    udword i, num_teams_with_flag = 0;

    for (i=0;i<aiCurrentAIPlayer->teamsUsed;i++)
    {
        if (bitTest(aiCurrentAIPlayer->teams[i]->teamFlags,flag))
        {
            num_teams_with_flag++;
        }
    }
    return num_teams_with_flag;
}


/*-----------------------------------------------------------------------------
    Name        : aitFindNextTeamWithFlag
    Description : Looks through the team list (starting with the
                  passed team) for the next team with the passed flag set
    Inputs      : team - the starting team, flag - the flag to look for
    Outputs     :
    Return      : the next team
----------------------------------------------------------------------------*/
AITeam *aitFindNextTeamWithFlag(AITeam *team, udword flag)
{
    udword i=0;

    //find "team"
    if (team)
    {
        for (i=0;(i<aiCurrentAIPlayer->teamsUsed) && (aiCurrentAIPlayer->teams[i] != team); i++);
        i++;
    }

    for (;(i<aiCurrentAIPlayer->teamsUsed) && (!bitTest(aiCurrentAIPlayer->teams[i]->teamFlags, flag)); i++);

    if (i==aiCurrentAIPlayer->teamsUsed)
    {
        return NULL;
    }
    else
    {
        return aiCurrentAIPlayer->teams[i];
    }

}


/*-----------------------------------------------------------------------------
    Name        : aitCountTeamsWaitingForShips
    Description : Counts the number of teams with the Getships and FancyGetships move
    Inputs      :
    Outputs     :
    Return      : the number of teams waiting for ships
----------------------------------------------------------------------------*/
sdword aitCountTeamsWaitingForShips(TeamType type)
{
    sdword i, num_teams_waiting = 0;
    AITeam *team;

    for (i=0;i<aiCurrentAIPlayer->teamsUsed;i++)
    {
        team = aiCurrentAIPlayer->teams[i];

        //if type is AnyTeam or the team's type is type
        //and
        //if the team doesn't have a move
        //or if the team's move is "MOVE_GETSHIPS" or "MOVE_FANCYGETSHIPS"
        //then increment num_teams_waiting
        if (((type == AnyTeam) ||
             (team->teamType == type)) &&
            ((!team->curMove)||
             (team->curMove->type == MOVE_GETSHIPS)||
             (team->curMove->type == MOVE_FANCYGETSHIPS)))
        {
            num_teams_waiting++;
        }
    }
    return num_teams_waiting;
}




bool32 aitTeamIsDone(AITeam *team)
{
    if ((team->curMove == NULL) || (team->curMove->type == MOVE_DONE))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : aitTeamIsIdle
    Description : Returns TRUE if the team is waiting for ships, done or sitting
                  around doing nothing
    Inputs      : team - the team to check
    Outputs     :
    Return      : TRUE if team is being a slacker
----------------------------------------------------------------------------*/
bool32 aitTeamIsIdle(AITeam *team)
{
    if ((team->curMove == NULL) ||
        (team->curMove->type == MOVE_DONE) ||
        (team->curMove->type == MOVE_GETSHIPS) ||
        (team->curMove->type == MOVE_FANCYGETSHIPS))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


/*-----------------------------------------------------------------------------
    Name        : aitTeamIsGuarding
    Description : Returns true if the team is guarding
    Inputs      : team - the team to check
    Outputs     :
    Return      : TRUE if the team is guarding
----------------------------------------------------------------------------*/
bool32 aitTeamIsGuarding(AITeam *team)
{
    AIMoveTypes moveType;

    if (team->curMove == NULL)
        return FALSE;

    moveType = team->curMove->type;

    if ((moveType == MOVE_GUARDSHIPS) ||
        (moveType == MOVE_GUARDCOOPTEAM) ||
        (moveType == MOVE_PATROLMOVE) ||
        (moveType == MOVE_ACTIVEPATROL))
    {
        return TRUE;
    }
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : aitTeamIsAttacking
    Description : Returns TRUE if the team is attacking
    Inputs      : team - the team to check
    Outputs     :
    Return      : TRUE if the team is attacking
----------------------------------------------------------------------------*/
bool32 aitTeamIsAttacking(AITeam *team)
{
    AIMoveTypes moveType;

    if (team->curMove == NULL)
        return FALSE;

    moveType = team->curMove->type;

    if ((moveType == MOVE_ATTACK) ||
        (moveType == MOVE_ADVANCEDATTACK) ||
        (moveType == MOVE_FLANKATTACK))
    {
        return TRUE;
    }
    return FALSE;
}



/*-----------------------------------------------------------------------------
    Name        : aitTeamIsntDefendingMothership
    Description : Returns TRUE if the team is doing anything other than defending the mothership
    Inputs      : team - the team to check, enemyships - the ships attacking the mothership
    Outputs     :
    Return      : TRUE if the team isn't defending the mothership
----------------------------------------------------------------------------*/
bool32 aitTeamIsntDefendingMothership(AITeam *team, SelectCommand *enemyships)
{
    if (team->curMove != NULL)
    {
        if ((team->curMove->type == MOVE_DEFMOSHIP) ||
            (team->curMove->type == MOVE_DOCK))
        {
            return FALSE;
        }
        else if (aitTeamIsAttacking(team))
        {
            if ((team->curMove->params.attack.ships) &&
                TheseShipsAreInSelection(team->curMove->params.attack.ships, enemyships))
            {
                return FALSE;
            }
        }
    }
    else
    {
        //reject teams who have no moves or no ships
        return FALSE;
    }
    return TRUE;
}


/*-----------------------------------------------------------------------------
    Name        : aitTeamIsFinishedMoving
    Description : Checks to see if the team has completed a move
    Inputs      : team - the moving team,
                  destination - where the team is moving to,
                  range - the distance to the destination that determines
                          the end of the move
    Outputs     :
    Return      : TRUE if the team has finished moving
----------------------------------------------------------------------------*/
bool32 aitTeamIsFinishedMoving(AITeam *team, vector destination, real32 range)
{
    vector current_location;
    real32 avg_size;

    if (!team->shipList.selection->numShips)
    {
        //if the team is dead, it's finished moving
        return TRUE;
    }

    current_location = selCentrePointComputeGeneral((MaxSelection *)team->shipList.selection, &avg_size);

    if ((aiuFindDistanceSquared(current_location, destination) < (range*range)) ||
        (vecIsZero(team->shipList.selection->ShipPtr[0]->moveTo)))
    {
        return TRUE;
    }
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : aitTeamIsDoingSpecialOp
    Description : Returns true if the team is preforming a special operation
    Inputs      : team - the team to check
    Outputs     :
    Return      : TRUE if the team is special op'ing
----------------------------------------------------------------------------*/
bool32 aitTeamIsDoingSpecialOp(AITeam *team)
{
//    dbgAssertOrIgnore(aitTeamShipTypeIs(MinelayerCorvette, team));

    if (team->shipList.selection->numShips &&
        team->shipList.selection->ShipPtr[0]->command &&
        (team->shipList.selection->ShipPtr[0]->command->ordertype.order == COMMAND_SPECIAL))
    {
        return TRUE;
    }
    return FALSE;
}





bool32 aitAnyTeamOfPlayerAttackingThisShip(struct AIPlayer *aiplayer,Ship *ship)
{
    sdword i;
    Node *node;
    AITeamMove *move;
    AITeam *team;

    for (i = 0; i < aiplayer->teamsUsed; ++i)
    {
        team = aiplayer->teams[i];

        node = team->moves.head;
        while (node != NULL)
        {
            move = (AITeamMove *)listGetStructOfNode(node);

            if ((move->type == MOVE_ATTACK) ||
                (move->type == MOVE_ADVANCEDATTACK) ||
                (move->type == MOVE_FLANKATTACK))
            {
                if (move->params.attack.ships)
                {
                    if (ShipInSelection(move->params.attack.ships,ship))
                    {
                        return TRUE;
                    }
                }
            }

            node = node->next;
        }
    }

    return FALSE;
}


// CAT: no aitTeamIsInRange: inlined in aitTeamIsInMothershipRange 0x00417470 (its only caller), see there


/*-----------------------------------------------------------------------------
    Name        : aitTeamIsInMothershipRange
    Description : Returns True if the ship is within a certain range of the mothership
                  CAT (0x00417470): HW1's aitTeamIsInRange inlined: the team's speed (its first ship's maximum
                  velocity times its Cataclysm speed multiplier, or the slowest ship's of a mixed team) for 40 s
                  when the player's mothership is of the mothership class, else 30 s; the squared range is
                  evaluated unrounded on the x87 and stored as a float before the compare
    Inputs      : team - the team to check
    Outputs     :
    Return      : TRUE if the team is in range
----------------------------------------------------------------------------*/
bool32 aitTeamIsInMothershipRange(AITeam *team)
{
    ShipPtr mothership = team->aiplayerowner->player->PlayerMothership;
    double velocity;
    real32 rangesq;

    if (!mothership)
    {
        return FALSE;
    }
    if (!team->shipList.selection->numShips)
    {
        return FALSE;
    }

    if (!aitTeamHomogenous(team))
    {
        velocity = (double)aiuFindSlowestShipMaxSpeed(team->shipList.selection);
    }
    else
    {
        velocity = (double)tacticsGetShipsMaxVelocity(team->shipList.selection->ShipPtr[0]) *
                   (double)cat_shipGetSpeedMultiplier(team->shipList.selection->ShipPtr[0]);
    }

    rangesq = (real32)(velocity * velocity *
                       ((((ShipStaticInfo *)mothership->staticinfo)->shipclass != CLASS_Mothership) ? 900.0 : 1600.0));

    return (aiuFindDistanceSquared(team->shipList.selection->ShipPtr[0]->obj.posinfo.position,
                                   mothership->obj.posinfo.position) < rangesq);
}


/*-----------------------------------------------------------------------------
    Name        : aitRecallGuardTeam
    Description : Changes the moves of a guard team so that it returns to it's patrol
    Inputs      : team - the team to recall
    Outputs     : removes a few moves
    Return      : void
----------------------------------------------------------------------------*/
void aitRecallGuardTeam(AITeam *team)
{
    while (aitTeamIsAttacking(team))
    {
        aitDeleteCurrentMove(team);
    }
}


/*-----------------------------------------------------------------------------
    Name        : aitNeedStrikeSupport
    Description : Returns TRUE if there are enough Strike Craft teams to
                  justify building support craft
    Inputs      : minstr - minimum strike craft for TRUE
    Outputs     :
    Return      : TRUE or FALSE (see description)
----------------------------------------------------------------------------*/
bool32 aitNeedStrikeSupport(udword minstr)
{
    udword tally = 0, i;
    AITeam *team;

    for (i=0; i<aiCurrentAIPlayer->teamsUsed;i++)
    {
        team = aiCurrentAIPlayer->teams[i];

        if (aitNumTeamShips(team) &&
            ((aitTeamShipClassIs(CLASS_Fighter, team) ||
             (aitTeamShipClassIs(CLASS_Corvette, team)))))
        {
            if (!aitTeamIsInMothershipRange(team))
            {
                tally += aitNumTeamShips(team);
            }
        }
    }

    if (tally > minstr)
    {
        return TRUE;
    }
    return FALSE;
}



/*-----------------------------------------------------------------------------
    Name        : aitMakeTeamSupportShips
    Description : Sets the team to support ships... or something like that
    Inputs      : team - the team to do the supporting, ships - the ships to support
    Outputs     : Duplicates ships selection into the support move structure
    Return      : void
----------------------------------------------------------------------------*/
void aitMakeTeamSupportShips(AITeam *team, SelectCommand *ships)
{
    AITeamMove *move;

    move = team->curMove;

    if (!move)
    {
        return;
    }

    while ((move->type != MOVE_SUPPORT) && (move->type != MOVE_DONE))
    {
        move = listGetStructOfNode(move->listNode.next);
        if (move == NULL)
        {
            return;
        }
    }

    if (move->type == MOVE_DONE)
    {
        return;
    }
    dbgAssertOrIgnore(move->type == MOVE_SUPPORT);

    aiumemFree(move->params.support.ships);

    move->params.support.ships = selectMemDupSelection(ships, "dupmtss", 0);
}



/*-----------------------------------------------------------------------------
    Name        : aitCheckIfOtherDefTeamAnsweringSignalNeedsHelp
    Description : Look... the name of the function is pretty explicit.  What are you, dumb or something?
    Inputs      : team - the team doing the check, ships - the threat being responded to
    Outputs     :
    Return      : TRUE if the team needs help
----------------------------------------------------------------------------*/
bool32 aitCheckIfOtherDefTeamAnsweringSignalNeedsHelp(AITeam *team, SelectCommand *ships)
{
    udword i;
    AITeamMove *move;
    SelectCommand *teams = NULL;
    bool32 return_value = TRUE;

    for (i = 0; i < aiCurrentAIPlayer->numGuardTeams; i++)
    {
        if (aiCurrentAIPlayer->guardTeams[i] != team)
        {
            move = aiCurrentAIPlayer->guardTeams[i]->curMove;

            if (!move)
            {
                continue;
            }

            if (((move->type == MOVE_ATTACK) &&
                 (SelectionsAreEquivalent(move->params.attack.ships, ships))) ||
                ((move->type == MOVE_ADVANCEDATTACK) &&
                 (SelectionsAreEquivalent(move->params.advatt.targets, ships))) ||
                ((move->type == MOVE_FLANKATTACK) &&
                 (SelectionsAreEquivalent(move->params.flankatt.targets, ships))))
            {
                teams = selectMergeTwoSelections(teams, aiCurrentAIPlayer->guardTeams[i]->shipList.selection, DEALLOC1);
            }
        }
    }

    if (teams != NULL)
    {
        if (teams->numShips)
        {
            return_value = aiuSelectionNotGoodAtKillingTheseTargets(teams, ships, AIT_DEFTEAM_NEEDS_HELP_STRENGTH);
        }
        memFree(teams);
    }
    return return_value;
}


/*-----------------------------------------------------------------------------
    Name        : aitSetTeamHomoHetero
    Description : Sets the team's Homogenous or Heterogenous flags
    Inputs      : team - the team to set
    Outputs     : sets some flags
    Return      : void
----------------------------------------------------------------------------*/
void aitSetTeamHomoHetero(AITeam *team)
{
    SelectCommand *teamShips = team->shipList.selection;
    ShipType firstShipType   = teamShips->ShipPtr[0]->shiptype;
    udword i;

    bitClear(team->teamFlags, TEAM_HETEROGENEOUS);
    bitClear(team->teamFlags, TEAM_HOMOGENEOUS);

    for (i=1;i<teamShips->numShips;i++)
    {
        if (teamShips->ShipPtr[i]->shiptype != firstShipType)
        {
            bitSet(team->teamFlags, TEAM_HETEROGENEOUS);
        }
    }
    bitSet(team->teamFlags, TEAM_HOMOGENEOUS);
}



/*-----------------------------------------------------------------------------
    Name        : aitCheckTeamHomogenous
    Description : Checks if the team is homogenous by checking for the homogenous flag.
                  If the flag isn't set, check shiplist and set flag
    Inputs      : team - the team to check
    Outputs     : Sets some flags
    Return      : TRUE if the team is homogenous
----------------------------------------------------------------------------*/
bool32 aitTeamHomogenous(AITeam *team)
{
    if (bitTest(team->teamFlags, TEAM_HOMOGENEOUS))
    {
        return TRUE;
    }
    else if (bitTest(team->teamFlags, TEAM_HETEROGENEOUS))
    {
        return FALSE;
    }
    else
    {
        aitSetTeamHomoHetero(team);
        return aitTeamHomogenous(team);
    }

}


/*-----------------------------------------------------------------------------
    Name        : aitSetTeamStrengthValue
    Description : Sets the team's strength and value variables
    Inputs      : team - the team to rate
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aitSetTeamStrengthValue(AITeam *team)
{
    udword i, shipstrength, shipvalue;
    SelectCommand *teamShips = team->shipList.selection;

    team->teamStrength = 0;
    team->teamValue    = 0;

    for (i=0;i<teamShips->numShips;i++)
    {
        aiuRateShip(&shipstrength, &shipvalue, teamShips->ShipPtr[i]);
        team->teamStrength += shipstrength;
        team->teamValue    += shipvalue;
    }
}


/*-----------------------------------------------------------------------------
    Name        : aitSetTeamDifficultyLevel
    Description : Sets the difficulty level of the team.
                  Note: team difficulty level stored as the overall difficulty
                        level added to the team's personal difficulty level.
                        i.e. Overall difficulty 2 (advanced),
                             Team difficulty 1 (intermediate),
                             Actual team difficulty 3 (intermediate-advanced)
                        Actual team levels go from 0 (beginner) to 4 (advanced)
                        Whereas setting team and player difficulty, the settings
                        are only between 0 (beginner) to 2 (advanced). I'll stop
                        explaining now...

    Inputs      : team - the team to set the difficulty level for,
                  playerDL - the overall player difficulty level,
                  teamDL - the team's specific difficulty level
    Outputs     : Changes a the difficulty level number in the team structure
    Return      : the team's actual difficulty level
----------------------------------------------------------------------------*/
ubyte aitSetTeamDifficultyLevel(AITeam *team, udword playerDL, udword  teamDL)
{
    team->teamDifficultyLevel = (ubyte)(playerDL + teamDL);

    return team->teamDifficultyLevel;
}



/*-----------------------------------------------------------------------------
    Name        : aitFindMostValuable
    Description : Finds the most valueable team
    Inputs      : valueness - if > 1, find the nth most valueable team,
                              where n is valueness
                  note: valueness cannot be bigger than 10
    Outputs     :
    Return      : The nth most valueable team
----------------------------------------------------------------------------*/
AITeam *aitFindMostValuable(udword valueness)
{
    AITeam *valueArray[10];
    udword i, j, k;

    dbgAssertOrIgnore(valueness <= 10);

    for (i=0;i<10;i++)
    {
        valueArray[i] = NULL;
    }

    for (i=0;i<aiCurrentAIPlayer->teamsUsed;i++)
    {
        if (aiCurrentAIPlayer->teams[i])
        {
            // CAT: the bound before the read (aitFindMostValuable 0x00417920; HW1 read valueArray[10] once full)
            for (j=0;(j<10)&&(valueArray[j]);j++)
            {
                //insertion sort
                if (valueArray[j]->teamValue < aiCurrentAIPlayer->teams[i]->teamValue)
                {
                    for (k=(10-1);k > j;k--)
                    {
                        valueArray[k] = valueArray[k-1];
                    }
                    valueArray[j] = aiCurrentAIPlayer->teams[i];
                    break;
                }
            }

            if ((j<10) && (!valueArray[j]))
            {
                valueArray[j] = aiCurrentAIPlayer->teams[i];
            }
        }
    }

    if (valueness)
    {
        return valueArray[valueness-1];
    }
    else
    {
        return valueArray[0];
    }
}


/*-----------------------------------------------------------------------------
    Name        : aitTeamIsntCoopWithAnotherTeam_Cloak
    Description : Makes sure the team doesn't have another cloakgenerator cooperating team
    Inputs      : testteam - the team to check
    Outputs     :
    Return      : TRUE if the team isn't cooperating with another cloak team
----------------------------------------------------------------------------*/
bool32 aitTeamIsntCoopWithAnotherTeam_Cloak(AITeam *testteam)
{
    udword i;
    AITeam *team;

    for (i=0;i<aiCurrentAIPlayer->teamsUsed;i++)
    {
        team = aiCurrentAIPlayer->teams[i];

        if (team->cooperatingTeam == testteam)
        {
            // CAT: either race's cloak generator (inlined in aitFindCloakGeneratorCoopTeam 0x00417a20)
            if (aitTeamShipTypeIs(kCloakGenerator, team) || aitTeamShipTypeIs(tCloakGenerator, team))
            {
                return FALSE;
            }
        }
    }
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : aitTeamIsntCoopWithAnotherTeam_GravWell
    Description : Makes sure the team doesn't have another cooperating team with
                  strikecraft or with a gravwell generator
    Inputs      : testteam - the team to check
    Outputs     :
    Return      : TRUE if the team isn't cooperating with another cloak team
----------------------------------------------------------------------------*/
bool32 aitTeamIsntCoopWithAnotherTeam_GravWell(AITeam *testteam)
{
    udword i;
    AITeam *team;

    for (i=0;i<aiCurrentAIPlayer->teamsUsed;i++)
    {
        team = aiCurrentAIPlayer->teams[i];

        if ((team) && (team->cooperatingTeam == testteam))
        {
            // CAT: either race's grav well generator (0x00417990: 0x40 / 0x4f)
            if (aitTeamShipClassIs(CLASS_Fighter, team) ||
                aitTeamShipClassIs(CLASS_Corvette, team) ||
                aitTeamShipTypeIs(kGravWellGenerator, team) ||
                aitTeamShipTypeIs(tGravWellGenerator, team))
            {
                return FALSE;
            }
        }
    }
    return TRUE;
}



/*-----------------------------------------------------------------------------
    Name        : aitFindCloakGeneratorCoopTeam
    Description : Finds an appropriate team for the cloak generator to cooperate with
    Inputs      : none
    Outputs     :
    Return      : returns a pointer to a team
----------------------------------------------------------------------------*/
AITeam *aitFindCloakGeneratorCoopTeam(void)
{
    AITeam *resconTeam = NULL, *frigTeam = NULL, *team;
    udword i;

    // CAT: no HW1 ResourceController team to cloak (M2-R4 ships-b)

    //frigates are also good to cloak
    for (i=0;i<aiCurrentAIPlayer->numAttackTeams;i++)
    {
        team = aiCurrentAIPlayer->attackTeam[i];

        // CAT: neither race's grav well or cloak generator (aitFindCloakGeneratorCoopTeam 0x00417a20)
        if (team &&
            team->shipList.selection->numShips &&
            aitTeamShipClassIs(CLASS_Frigate, team) &&
            (!aitTeamShipTypeIs(kGravWellGenerator, team) && !aitTeamShipTypeIs(tGravWellGenerator, team) &&
             !aitTeamShipTypeIs(kCloakGenerator, team) && !aitTeamShipTypeIs(tCloakGenerator, team)) &&
            (aitTeamIsntCoopWithAnotherTeam_Cloak(team)))
        {
            frigTeam = aiCurrentAIPlayer->attackTeam[i];
            break;
        }
    }

    //later may need to check if the chosen team is being defended by someone already
    //and probably should have a nicer method of deciding between these two options
    //but for a first pass this should work out.
    if (frigTeam == NULL)
    {
        return resconTeam;
    }
    else
    {
        return frigTeam;
    }
}


/*-----------------------------------------------------------------------------
    Name        : aitFindGravWellGeneratorCoopTeam
    Description : Finds an appropriate team for the Gravwell generator to cooperate with
    Inputs      : none
    Outputs     :
    Return      : returns a pointer to a team
----------------------------------------------------------------------------*/
AITeam *aitFindGravWellGeneratorCoopTeam(void)
{
    AITeam *capTeam = NULL, *team;
    udword i;

    //frigates are also good to cloak
    for (i=0;i<aiCurrentAIPlayer->numAttackTeams;i++)
    {
        team = aiCurrentAIPlayer->attackTeam[i];

        // CAT: neither race's grav well or cloak generator, and no test for another cooperating team
        //      (aitFindGravWellGeneratorCoopTeam 0x00417b20 does not call aitTeamIsntCoopWithAnotherTeam_GravWell)
        if (team &&
            team->shipList.selection->numShips &&
            (aitTeamShipClassIs(CLASS_Frigate, team) ||
             aitTeamShipClassIs(CLASS_Destroyer, team) ||
             aitTeamShipClassIs(CLASS_HeavyCruiser, team)) &&
            (!aitTeamShipTypeIs(kGravWellGenerator, team) && !aitTeamShipTypeIs(tGravWellGenerator, team) &&
             !aitTeamShipTypeIs(kCloakGenerator, team) && !aitTeamShipTypeIs(tCloakGenerator, team)))
        {
            capTeam = team;
            break;
        }
    }

    //later may need to check if the chosen team is being defended by someone already (particularily fighters)
    //and probably should have a nicer method of deciding between these two options
    //but for a first pass this should work out.
    return capTeam;
}


/*-----------------------------------------------------------------------------
    Name        : aitFindGoodCoopTeam
    Description : Finds a good team to cooperate with based on the shiptype
    Inputs      : type - the type of ship we'll be cooperating with
    Outputs     :
    Return      : the good coop team
----------------------------------------------------------------------------*/
AITeam *aitFindGoodCoopTeam(ShipType type)
{
    switch (type)
    {
        // CAT: either race's generators (aitFindGoodCoopTeam 0x00417ba0)
        case kCloakGenerator:
        case tCloakGenerator:
            //frigate class team (resource controller as well)
            return aitFindCloakGeneratorCoopTeam();
            break;
        case kGravWellGenerator:
        case tGravWellGenerator:
            //frigate or larger team with no fighters guarding
            return aitFindGravWellGeneratorCoopTeam();
            break;
        default:
//            dbgAssertOrIgnore(FALSE);
            break;
    }
    return NULL;
}





//
//  process current move for each team
//
void aitExecute(void)
{
    sdword i;
    AITeam *team;
    AITeamMove *lastMove, *thisMove;

    for (i = 0; i < aiCurrentAIPlayer->teamsUsed; ++i)
    {
        team = aiCurrentAIPlayer->teams[i];

        // handle any events, if applicable first
        // (they may alter curMove)
        aieExecute(team);

        //if the handler set the team to be destroyed
        if (bitTest(team->teamFlags, AIT_DestroyTeam))
        {
            aitDestroy(aiCurrentAIPlayer, team, TRUE);
            i--;
            continue;
        }

        if (team->curMove == NULL)
        {
            if (team->moves.num > 0)
            {
                team->curMove = (AITeamMove *)listGetStructOfNode(team->moves.head);
                dbgAssertOrIgnore(team->curMove);
            }
            else
                continue;
        }

        //if the series of moves have been completed, don't do anything
        if (team->curMove->type == MOVE_DONE)
            continue;
        if (team->curMove->type == MOVE_DELETETEAM)
        {
            aitDestroy(aiCurrentAIPlayer, team, TRUE);
            i--;
            continue;
        }

        if (team->teamDelay)
        {
            team->teamDelay--;
        }
        else if (team->curMove->processFunction(team))
        {
            //if the process function set the team to be destroyed
            if (bitTest(team->teamFlags, AIT_DestroyTeam))
            {
                aitDestroy(aiCurrentAIPlayer, team, TRUE);
                i--;
                continue;
            }

            lastMove = team->curMove;
            if (!team->curMove->listNode.next)
            {
                team->curMove = NULL;
                // skip the tactics & formation stuff
            }
            else
            {
                team->curMove = (AITeamMove *)listGetStructOfNode(team->curMove->listNode.next);
                thisMove      = team->curMove;

                //if the tactics setting of the new move is different from the
                //old move, set tactics
                if ((aiuTeamFeatureEnabled(team, AIT_TACTICS)) &&
                    (thisMove->tactics != lastMove->tactics) && (team->shipList.selection->numShips))
                {
                    aiuWrapSetTactics(team->shipList.selection,thisMove->tactics);
                }

                //set the formation for this move
                //if the formation doesn't change
                if (thisMove->formation == SAME_FORMATION)
                {
                    //set the current move formation to the last move's formation
                    //or to a default
                    thisMove->formation = lastMove->formation;
                }
                else if ((thisMove->formation != NO_FORMATION) &&
                         (thisMove->formation != lastMove->formation) &&
                         (team->shipList.selection->numShips > 1))
                {
                    aiuWrapFormation(team->shipList.selection,thisMove->formation);
                }

                if (team->teamFlags & AIT_SpecialTeamMask)
                {
                    aitProcessSpecialTeam(team, thisMove);
                }

                team->teamDelay = AIT_TEAM_MOVE_DELAY[team->teamDifficultyLevel];
            }

            if (lastMove->remove)
            {
                if (lastMove->moveCloseFunction)
                {
                    lastMove->moveCloseFunction(team, lastMove);
//                    aiplayerLog((aiIndex, "%x Deleting Move in aitExecute due to Remove flag", team));
                }
                listDeleteNode(&lastMove->listNode);
            }
        }
    }
}

//
//  sends a message to a team
//
//  CAT: aitMsgSend 0x00417e10: the message is queued by pointer (no copy, no test for an empty message) in the team's
//       own queue; 64 messages wrap the count to 0
//
void aitMsgSend(AITeam *fromTeamp, AITeam *teamp, char *msg)
{
    MsgQueue *msgQP;

    if (!teamp->msgQueue)
    {
        teamp->msgQueue = &teamp->catMsgQueue;
        memset(teamp->msgQueue, 0, sizeof(MsgQueue));
    }

    msgQP = teamp->msgQueue;
    msgQP->msgSenders[msgQP->head] = fromTeamp;
    msgQP->msgs[msgQP->head] = msg;
    ++msgQP->head;

    if (msgQP->head == MSG_QUEUE_MAX_MSGS)
    {
        msgQP->head = 0;
    }
}

//
//  returns 1 if msg is in the received msgs queue
//  (msg will be removed from queue).
//  returns 0 otherwise.
//
//  CAT: aitMsgReceived 0x00417e60: the message is matched by address among the queued ones and replaced by the last
//       one; the sender is not recorded (HW1 set msgSender)
//
sdword aitMsgReceived(AITeam *teamp, char *msg)
{
    MsgQueue *msgQP;
    sdword i;

    msgQP = teamp->msgQueue;
    if (!msgQP || !msg)
        return 0;

    for (i = 0; i < msgQP->head; ++i)
    {
        if (msgQP->msgs[i] == msg)
        {
            if (msgQP->head - 1 > 0)
            {
                msgQP->msgs[i] = msgQP->msgs[msgQP->head - 1];
                msgQP->msgSenders[i] = msgQP->msgSenders[msgQP->head - 1];
            }
            if (msgQP->head > 0)
            {
                msgQP->head--;
            }
            return 1;
        }
    }

    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : cat_aitMsgQueueRemoveAt
    Description : CAT (0x00417ec0): takes the index-th queued message out (the last one moves into its place) and
                  returns it, NULL when the team, its queue or the message is missing (the kas2c missions' readers)
----------------------------------------------------------------------------*/
char *cat_aitMsgQueueRemoveAt(AITeam *teamp, udword index)
{
    MsgQueue *msgQP;
    char *msg;

    if ((teamp == NULL) || ((msgQP = teamp->msgQueue) == NULL) || ((udword)msgQP->head <= index))
    {
        return NULL;
    }
    msg = msgQP->msgs[index];
    if (msgQP->head - 1 > 0)
    {
        msgQP->msgs[index] = msgQP->msgs[msgQP->head - 1];
        msgQP->msgSenders[index] = msgQP->msgSenders[msgQP->head - 1];
    }
    if (msgQP->head > 0)
    {
        msgQP->head--;
    }
    return msg;
}

/*-----------------------------------------------------------------------------
    Name        : cat_aitMsgQueueFlush
    Description : CAT (0x00417f20): forgets the queue (its storage is the team's; the contents stay)
----------------------------------------------------------------------------*/
void cat_aitMsgQueueFlush(AITeam *team)
{
    if (team->msgQueue != NULL)
    {
        team->msgQueue = NULL;
    }
}

//
//  CAT: the queue is the team's own storage; aitDestroy 0x00416c5e only forgets it (as cat_aitMsgQueueFlush)
//
void aitMsgQueueFree(AITeam *teamp)
{
    cat_aitMsgQueueFlush(teamp);
}

/*=============================================================================
    Save Game stuff here on
=============================================================================*/

AITeam *savingThisAITeam = NULL;

typedef void (*PreFix_Move)(AITeamMove *move);
typedef void (*Save_Move)(AITeamMove *move);
typedef void (*Load_Move)(AITeamMove *move);
typedef void (*Fix_Move)(AITeamMove *move);

typedef struct
{
    AIMoveTypes movetype;
    PreFix_Move preFixMove;
    Save_Move saveMove;
    Load_Move loadMove;
    Fix_Move fixMove;
} SaveMoveTypeFuncTable;

SaveMoveTypeFuncTable theSaveMoveTypeFuncTable[NUMBER_OF_MOVETYPES] =
{
//    move ID                 Pre_fix move           Save move              load move              fix move
    { MOVE_DONE,              NULL,                  NULL,                  NULL,                  aimFix_MoveDone },
    { MOVE_GUARDSHIPS,        NULL,                  aimSave_GuardShips,    aimLoad_GuardShips,    aimFix_GuardShips },
    { MOVE_DEFMOSHIP,         NULL,                  aimSave_DefMoship,     aimLoad_DefMoship,     aimFix_DefMoship },
    { MOVE_TEMPGUARD,         NULL,                  NULL,                  NULL,                  aimFix_TempGuard },
    { MOVE_GETSHIPS,          aimPreFix_GetShips,    NULL,                  NULL,                  aimFix_GetShips },
    { MOVE_FORMATION,         NULL,                  NULL,                  NULL,                  aimFix_Formation },
    { MOVE_MOVETEAM,          NULL,                  NULL,                  NULL,                  aimFix_MoveTeam },
    { MOVE_MOVETEAMINDEX,     NULL,                  NULL,                  NULL,                  aimFix_MoveTeamIndex },
    { MOVE_MOVETEAMSPLIT,     NULL,                  aimSave_MoveTeamSplit, aimLoad_MoveTeamSplit, aimFix_MoveTeamSplit },
    { MOVE_INTERCEPT,         aimPreFix_Intercept,   NULL,                  NULL,                  aimFix_Intercept },
    { MOVE_MOVETO,            NULL,                  NULL,                  NULL,                  aimFix_MoveTo },
    { MOVE_PATROLMOVE,        aimPreFix_PatrolMove,  aimSave_PatrolMove,    aimLoad_PatrolMove,    aimFix_PatrolMove },
    { MOVE_ACTIVEPATROL,      NULL,                  NULL,                  NULL,                  aimFix_ActivePatrol },
    { MOVE_ACTIVERECON,       NULL,                  NULL,                  NULL,                  aimFix_ActiveRecon },
    { MOVE_SHIPRECON,         NULL,                  aimSave_ShipRecon,     aimLoad_ShipRecon,     aimFix_ShipRecon },
    { MOVE_COUNTSHIPS,        NULL,                  NULL,                  NULL,                  aimFix_CountShips },
    { MOVE_SPECIAL,           NULL,                  aimSave_Special,       aimLoad_Special,       aimFix_Special },
    { MOVE_ATTACK,            NULL,                  aimSave_Attack,        aimLoad_Attack,        aimFix_Attack },
    { MOVE_ADVANCEDATTACK,    aimPreFix_AdvancedAttack,aimSave_AdvancedAttack,aimLoad_AdvancedAttack,aimFix_AdvancedAttack },
    { MOVE_FLANKATTACK,       NULL,                  aimSave_FlankAttack,   aimLoad_FlankAttack,   aimFix_FlankAttack },
    { MOVE_MOVEATTACK,        aimPreFix_MoveAttack,  aimSave_MoveAttack,    aimLoad_MoveAttack,    aimFix_MoveAttack },
    { MOVE_HARASSATTACK,      aimPreFix_HarassAttack,NULL,                  NULL,                  aimFix_HarassAttack },
    { MOVE_FANCYGETSHIPS,     aimPreFix_FancyGetShips,NULL,                 NULL,                  aimFix_FancyGetShips },
    { MOVE_DOCK,              aimPreFix_Dock,        aimSave_Dock,          aimLoad_Dock,          aimFix_Dock },
    { MOVE_LAUNCH,            NULL,                  NULL,                  NULL,                  aimFix_Launch },
    { MOVE_REINFORCE,         aimPreFix_Reinforce,   NULL,                  NULL,                  aimFix_Reinforce },
    { MOVE_VARSET,            NULL,                  NULL,                  NULL,                  aimFix_VarSet },
    { MOVE_VARINC,            NULL,                  NULL,                  NULL,                  aimFix_VarInc },
    { MOVE_VARDEC,            NULL,                  NULL,                  NULL,                  aimFix_VarDec },
    { MOVE_VARWAIT,           NULL,                  NULL,                  NULL,                  aimFix_VarWait },
    { MOVE_VARDESTROY,        NULL,                  NULL,                  NULL,                  aimFix_VarDestroy },
    { MOVE_GUARDCOOPTEAM,     NULL,                  NULL,                  NULL,                  aimFix_GuardCoopTeam },
    { MOVE_SUPPORT,           NULL,                  aimSave_Support,       aimLoad_Support,       aimFix_Support },
    { MOVE_ARMADA,            NULL,                  NULL,                  NULL,                  aimFix_Armada },
    { MOVE_CONTROLRESOURCES,  NULL,                  aimSave_ControlResources,aimLoad_ControlResources,aimFix_ControlResources },
    { MOVE_RESVOLUME,         NULL,                  aimSave_ResourceVolume,aimLoad_ResourceVolume,aimFix_ResourceVolume },
    { MOVE_CAPTURE,           NULL,                  NULL,                  NULL,                  aimFix_Capture },
    { MOVE_ACTIVECAPTURE,     NULL,                  NULL,                  NULL,                  aimFix_ActiveCapture },
    { MOVE_ACTIVEMINE,        NULL,                  NULL,                  NULL,                  aimFix_ActiveMine },
    { MOVE_MINEVOLUME,        NULL,                  NULL,                  NULL,                  aimFix_MineVolume },
    { MOVE_SPECIALDEFENSE,    NULL,                  NULL,                  NULL,                  aimFix_SpecialDefense },
    { MOVE_ACTIVERES,         NULL,                  NULL,                  NULL,                  aimFix_ActiveResource },
    { MOVE_MOTHERSHIP,        NULL,                  NULL,                  NULL,                  aimFix_MothershipMove },
    { MOVE_KAMIKAZE,          NULL,                  aimSave_Kamikaze,      aimLoad_Kamikaze,      aimFix_Kamikaze },
    { MOVE_HYPERSPACE,        NULL,                  NULL,                  NULL,                  aimFix_Hyperspace },
    { MOVE_DELETETEAM,        NULL,                  NULL,                  NULL,                  aimFix_DeleteTeam }
};


void SaveMoveCB(void *stuff)
{
#define move ((AITeamMove *)stuff)
    SaveChunk *chunk;
    AITeamMove *sc;
    SaveMoveTypeFuncTable *movetypefunctable;

    chunk = CreateChunk(BASIC_STRUCTURE|AITEAMMOVE,sizeof(AITeamMove),move);
    sc = chunkContents(chunk);

    dbgAssertOrIgnore(move->type < NUMBER_OF_MOVETYPES);
    movetypefunctable = &theSaveMoveTypeFuncTable[move->type];
    dbgAssertOrIgnore(move->type == movetypefunctable->movetype);

    if (movetypefunctable->preFixMove)
    {
        movetypefunctable->preFixMove(sc);
    }

    aiePreFixAIEvents(sc);

    SaveThisChunk(chunk);
    memFree(chunk);
    sc = NULL;

    if (movetypefunctable->saveMove)
    {
        dbgAssertOrIgnore(movetypefunctable->loadMove);     // should have corresponding load function
        movetypefunctable->saveMove(move);
    }
#undef move
}

void FixMoveCB(void *stuff)
{
#define move ((AITeamMove *)stuff)
    SaveMoveTypeFuncTable *movetypefunctable;

    dbgAssertOrIgnore(move->type < NUMBER_OF_MOVETYPES);
    movetypefunctable = &theSaveMoveTypeFuncTable[move->type];
    dbgAssertOrIgnore(move->type == movetypefunctable->movetype);

    if (movetypefunctable->fixMove)
    {
        movetypefunctable->fixMove(move);
    }

    aieFixAIEvents(move);
#undef move
}

void LoadMoveCB(LinkedList *list)
{
    AITeamMove *move;
    SaveChunk *chunk;
    SaveMoveTypeFuncTable *movetypefunctable;

    // CAT (M2-R7 ai-player-skirmish): a cataclysm.exe save's 0x170-byte move is converted field by field first (no-op
    //      for this tree's own saves); the move type ids are the exe's (M2-R7 ai-orders-moves)
    chunk = saveCat32ChunkStruct(LoadNextChunk(), SAVECAT32_LAYOUT(AITeamMove));
    VerifyChunk(chunk,BASIC_STRUCTURE|AITEAMMOVE,sizeof(AITeamMove));
    move = memAlloc(sizeof(AITeamMove), "loadedmove", 0);
    memcpy(move,chunkContents(chunk),sizeof(AITeamMove));

    memFree(chunk);

    dbgAssertOrIgnore(move->type < NUMBER_OF_MOVETYPES);
    movetypefunctable = &theSaveMoveTypeFuncTable[move->type];
    dbgAssertOrIgnore(move->type == movetypefunctable->movetype);

    if (movetypefunctable->loadMove)
    {
        movetypefunctable->loadMove(move);
    }

    listAddNode(list,&move->listNode,move);
}

// CAT: SaveThisAITeam 0x00418040 saves the count and the 64 message words; the exe's words are pointers into its own
//      image (the kas2c message constants), which mean nothing to another process.
// CAT-TODO(stage 6 KAS): save and restore a queued message as its kas2c message id once the kas2c missions are ported; until then the message words are saved as 0 and a loaded queue keeps its count with no messages
void SaveMsgQueue(MsgQueue *msgQueue)
{
    sdword i;

    SaveInfoNumber(msgQueue->head);

    for (i=0;i<MSG_QUEUE_MAX_MSGS;i++)
    {
        SaveInfoNumber(0);
    }
}

// CAT: aitLoad 0x00418180: the queue is the team's own, the senders cleared
MsgQueue *LoadMsgQueue(AITeam *team)
{
    sdword i;
    MsgQueue *msgQueue = &team->catMsgQueue;

    msgQueue->head = LoadInfoNumber();

    for (i=0;i<MSG_QUEUE_MAX_MSGS;i++)
    {
        (void)LoadInfoNumber();
        msgQueue->msgs[i] = NULL;
        msgQueue->msgSenders[i] = NULL;
    }

    return msgQueue;
}

void SaveThisAITeam(AITeam *team)
{
    SaveChunk *chunk;
    AITeam *sc;

    chunk = CreateChunk(BASIC_STRUCTURE|AITEAM,sizeof(AITeam),team);
    sc = chunkContents(chunk);

    sc->aiplayerowner = (struct AIPlayer *)AIPlayerToNumber(team->aiplayerowner);

    sc->curMove = (struct AITeamMove *)ConvertPointerInListToNum(&team->moves,team->curMove);
    dbgAssertOrIgnore(team->custTeamInfo == NULL);      // not supported yet, will not support if no one uses it, fix later
    dbgAssertOrIgnore(team->TeamDiedCB == NULL);        // not supported yet, will not support if no one uses it, fix later
    PreFixCooperatingTeamDiedCB(sc);

    sc->cooperatingTeam = (struct AITeam *)AITeamToTeamIndex(team->cooperatingTeam);
    sc->msgSender = (struct AITeam *)AITeamToTeamIndex(team->msgSender);

    sc->kasFSMWatchFunction = (void *)kasConvertFuncPtrToOffset(team->kasFSMWatchFunction);
    sc->kasStateWatchFunction = (void *)kasConvertFuncPtrToOffset(team->kasStateWatchFunction);

    SaveThisChunk(chunk);
    memFree(chunk);
    sc = NULL;

    SaveGrowSelection(&team->shipList);
    SaveLinkedListOfStuff(&team->moves,SaveMoveCB);

    if (team->msgQueue) SaveMsgQueue(team->msgQueue);
}

AITeam *LoadThisAITeam(void)
{
    SaveChunk *chunk;
    AITeam *team;

    chunk = LoadNextChunk();
    VerifyChunk(chunk,BASIC_STRUCTURE|AITEAM,sizeof(AITeam));

    team = memAlloc(sizeof(AITeam), "aiteam", 0);
    memcpy(team,chunkContents(chunk),sizeof(AITeam));
    memFree(chunk);

    LoadGrowSelectionAndFix(&team->shipList);
    LoadLinkedListOfStuff(&team->moves,LoadMoveCB);

    if (team->msgQueue) team->msgQueue = LoadMsgQueue(team);

    return team;
}

void FixThisAITeam(AITeam *team)
{
    team->aiplayerowner = NumberToAIPlayer((sdword)team->aiplayerowner);

    team->curMove = ConvertNumToPointerInList(&team->moves,(sdword)team->curMove);

    team->cooperatingTeam = AITeamIndexToTeam(team->aiplayerowner,(sdword)team->cooperatingTeam);
    team->msgSender = AITeamIndexToTeam(team->aiplayerowner,(sdword)team->msgSender);

    FixCooperatingTeamDiedCB(team);

    team->kasFSMWatchFunction = kasConvertOffsetToFuncPtr((sdword)team->kasFSMWatchFunction);
    team->kasStateWatchFunction = kasConvertOffsetToFuncPtr((sdword)team->kasStateWatchFunction);

    FixLinkedListOfStuff(&team->moves,FixMoveCB);
}

void aitSave(struct AIPlayer *aiplayer)
{
    sdword i;

    SaveInfoNumber(aiplayer->teamsAllocated);
    SaveInfoNumber(aiplayer->teamsUsed);

    for (i=0;i<aiplayer->teamsUsed;i++)
    {
        savingThisAITeam = aiplayer->teams[i];
        SaveThisAITeam(savingThisAITeam);
        savingThisAITeam = NULL;
    }
}

// the dword at offset off of a cataclysm.exe chunk
static sdword aitCat32Word(const ubyte *contents, udword off)
{
    sdword v;

    memcpy(&v, contents + off, 4);
    return v;
}

/*-----------------------------------------------------------------------------
    Name        : aitCat32LoadTeam
    Description : CAT (M2-R7 ai-player-skirmish): a team of a cataclysm.exe save (aitLoad 0x00418180: a 0x280-byte
                  chunk, the grow selection, the moves, the message queue when +0x50 is set), converted by the exe's
                  offsets (this tree's AITeam keeps HW1's KAS names, so its layout is not the exe's, AITeam.h): the
                  numbers the exe saved for the owner, the current move, the cooperating team and its callback and
                  the message sender are put where FixThisAITeam reads them back.
                  CAT-TODO(stage 6 KAS): the exe's KAS identity (+0x58 label hash, +0x5C / +0x60 FSM and state name pointers, +0x64 / +0x68 the watch functions as kas2c function offsets) comes with the kas2c missions; the team is loaded with empty names and no watch functions
----------------------------------------------------------------------------*/
static AITeam *aitCat32LoadTeam(void)
{
    SaveChunk *chunk;
    const ubyte *c;
    AITeam *team;

    chunk = LoadNextChunk();
    VerifyChunk(chunk,BASIC_STRUCTURE|AITEAM,0x280);
    c = (const ubyte *)chunkContents(chunk);

    team = memAlloc(sizeof(AITeam), "aiteam", 0);
    memset(team, 0, sizeof(AITeam));
    team->teamType              = (TeamType)aitCat32Word(c, 0x00);
    team->teamFlags             = (udword)aitCat32Word(c, 0x04);
    team->catUnknown08          = (udword)aitCat32Word(c, 0x08);
    team->TeamFeatures          = (udword)aitCat32Word(c, 0x0C);
    team->teamDelay             = c[0x10];
    team->newships              = (udword)aitCat32Word(c, 0x14);
    team->aiplayerowner         = (struct AIPlayer *)(size_t)aitCat32Word(c, 0x18);    // the AIPlayer's number
    team->teamStrength          = (udword)aitCat32Word(c, 0x24);
    team->teamValue             = (udword)aitCat32Word(c, 0x28);
    team->teamDifficultyLevel   = c[0x2C];
    team->curMove               = (struct AITeamMove *)(size_t)aitCat32Word(c, 0x3C);  // its position in the moves
    team->cooperatingTeam       = (struct AITeam *)(size_t)aitCat32Word(c, 0x48);      // a team index
    team->cooperatingTeamDiedCB = (void *)(size_t)aitCat32Word(c, 0x4C);               // 0, 1 or 2
    team->msgQueue              = (aitCat32Word(c, 0x50) != 0) ? &team->catMsgQueue : NULL;
    team->msgSender             = (struct AITeam *)(size_t)aitCat32Word(c, 0x54);      // a team index
    team->kasFSMWatchFunction   = (KASWatchFunction)(size_t)(sdword)-1;                // NULL after the fix
    team->kasStateWatchFunction = (KASWatchFunction)(size_t)(sdword)-1;
    team->kasOrigShipsType      = (ShipType)aitCat32Word(c, 0x6C);
    team->kasOrigShipsCount     = aitCat32Word(c, 0x70);
    team->kasTactics            = (TacticsType)aitCat32Word(c, 0x74);
    team->kasFormation          = (TypeOfFormation)aitCat32Word(c, 0x78);
    memFree(chunk);

    LoadGrowSelectionAndFix(&team->shipList);
    LoadLinkedListOfStuff(&team->moves,LoadMoveCB);

    if (team->msgQueue) team->msgQueue = LoadMsgQueue(team);

    return team;
}

void aitLoad(struct AIPlayer *aiplayer)
{
    sdword i;

    aiplayer->teamsAllocated = LoadInfoNumber();
    aiplayer->teamsUsed = LoadInfoNumber();

    aiplayer->teams = memAlloc(sizeof(AITeam*)*aiplayer->teamsAllocated, "aiteamlist", 0);

    for (i=0;i<aiplayer->teamsUsed;i++)
    {
        aiplayer->teams[i] = saveCat32Loading ? aitCat32LoadTeam() : LoadThisAITeam();
    }
}

void aitFix(struct AIPlayer *aiplayer)
{
    sdword i;

    for (i=0;i<aiplayer->teamsUsed;i++)
    {
        savingThisAITeam = aiplayer->teams[i];
        FixThisAITeam(savingThisAITeam);
        savingThisAITeam = NULL;
    }
}

