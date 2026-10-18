// =============================================================================
//  AIFleetMan.c
//  - Controls construction of ships requested from combatman, defenseman, resourceman
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 1998/05/28 by gshaw
// =============================================================================

#include "AIFleetMan.h"

#include "AIAttackMan.h"
#include "AIDefenseMan.h"
#include "AIFeatures.h"
#include "Alliance.h"
#include "AIResourceMan.h"
#include "CommandWrap.h"
#include "MultiplayerGame.h"
#include "Randy.h"
#include "SinglePlayer.h"


/*-----------------------------------------------------------------------------
    Name        : aifInit
    Description : initialize resource manager stuff in aiplayer
    Inputs      : aiplayer
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void aifInit(AIPlayer *aiplayer)
{
    aiplayer->firstTurn         = TRUE;
    aiplayer->recalculateAllies = TRUE;
    aiplayer->ResearchDelay     = 0;
    aiplayer->ScriptCreator     = aiplayer->player->PlayerMothership;
    aiplayer->AICreator         = aiplayer->player->PlayerMothership;

    switch (aiplayer->aiplayerDifficultyLevel)
    {
        case AI_ADVANCED:
            if (singlePlayerGame ||
                ((!singlePlayerGame) && bitTest(tpGameCreated.flag, MG_Hyperspace)))
            {
                aiuEnableResourceFeature(AIF_HYPERSPACING);
            }
        case AI_INTERMEDIATE:
        case AI_BEGINNER:
            break;
        default:
            dbgAssertOrIgnore(FALSE);
    }

}

/*-----------------------------------------------------------------------------
    Name        : airClose
    Description : Closes resource manager stuff in aiplayer
    Inputs      : 
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void aifClose(void)
{
    // Nothing ??
}

// CAT: no aifShipDied: cataclysm.exe has none and nothing calls it (aiplayerShipDied 0x00411970 does not; HW1's body
//      tested the HW1 Carrier / Mothership types; M2-R7 ai-player-skirmish removed it)

void BuildShip(ShipStaticInfo *shipstatic,Player *player,sdword *incRUs,sdword *decRUs, ShipPtr creator)
{
#if(0)	
	Node *shipnode;
	Ship *ship;
	sdword shipcount,classcount;
	CommandToDo *command;
	ShipStaticInfo *tempstat;
#endif

    aiplayerLog((player->playerIndex,"Building ship %s; Total %i",ShipTypeToStr(shipstatic->shiptype),player->shiptotals[shipstatic->shiptype]));

	// unit caps checking code
#if(0)	
	shipnode = universe.ShipList.head;
	shipcount = 0;
	classcount = 0;
	while(shipnode != NULL)
	{
		ship = (Ship *)listGetStructOfNode(shipnode);

        if((ship->playerowner->playerIndex == player->playerIndex) &&
		   (!bitTest(ship->header.flags, SOF_Dead)))
		{
		  if(ship->shiptype == shipstatic->shiptype)
			{
				shipcount++;
			}
			if(ship->staticinfo->shipclass == shipstatic->shipclass)
			{
				classcount++;
			}
		}
		
		shipnode = shipnode->next;
	}

	shipnode = universe.mainCommandLayer.todolist.head;
	while(shipnode != NULL)
	{
	
		command = (CommandToDo *)listGetStructOfNode(shipnode);
	
		if(command->ordertype.order == COMMAND_BUILDING_SHIP)
		{
			if(command->buildingship.playerIndex == player->playerIndex)
			{
				if(command->buildingship.shipType == shipstatic->shiptype)
				{
					shipcount++;
				}

				tempstat = GetShipStaticInfo(command->buildingship.shipType, player->race);
				if(tempstat->shipclass == shipstatic->shipclass)
				{
					classcount++;
				}
			}
		}
		shipnode = shipnode->next;
	}
	if((cdLimitCaps[shipstatic->shiptype] != -1) && (shipcount+1 > cdLimitCaps[shipstatic->shiptype]))
	{
        dbgNonFatal(DBG_Loc,"shipstatic->shipclass: No Operation.");
	}
	if(classcount+1 > cdClassCaps[shipstatic->shipclass])	
	{
        dbgNonFatal(DBG_Loc,"shipstatic->shipclass: No Operation.");
	}
#endif

    // CAT: in a multiplayer game nothing is ordered once the support units are used up (BuildShip 0x00404470; the
    //      unsigned compare of the exe)
    if (!singlePlayerGame && (shipstatic->supportUnits != 0) &&
        ((udword)player->supportUnitsUsed >= (udword)player->supportUnitsMax))
    {
        return;
    }

    // CAT: Cataclysm's resource and support ships are counted by type (BuildShip 0x00404498: the Workers and the
    //      Taiidan collector, the processors in HW1's resource-controller count, the advance support frigates, the
    //      support modules in their own count)
    switch (shipstatic->shiptype)
    {
        case sWorker:
        case bWorker:
        case tResourceCollector:
            aiCurrentAIPlayer->NumRCollectorsBeingBuilt++;
            break;
        case kAdvanceSupportFrigate:
        case tAdvanceSupportFrigate:
            aiCurrentAIPlayer->NumASFBeingBuilt++;
            break;
        case sProcessor:
        case bProcessor:
            aiCurrentAIPlayer->NumRControllersBeingBuilt++;
            break;
        case sCarrierSupport:
        case sMothershipSupport:
        case bMothershipSupport:
            aiCurrentAIPlayer->catNumSupportModulesBeingBuilt++;
            break;
        default:
            break;
    }

    if (!creator)
    {
        creator = player->PlayerMothership;
    }
    if (creator == NULL)
    {
        return;                                 // CAT: no mothership, no order (0x004044f3)
    }

    // CAT: an external ship is created at the creator (0x00404516)
    if (shipstatic->externalBuild)
    {
        clWrapCreateShip(&universe.mainCommandLayer, shipstatic->shiptype, creator->shiprace, player->playerIndex, creator);
    }
    else
    {
        clWrapBuildShip(&universe.mainCommandLayer, shipstatic->shiptype, creator->shiprace, player->playerIndex, creator);
    }

    *incRUs += shipstatic->buildCost;
    *decRUs -= shipstatic->buildCost;
}

// CAT: no GetSubstituteShipStatic: cataclysm.exe has no such function and nothing calls it (HW1's substitute for the
//      R1/R2 races was the dropped LightInterceptor; M2-R7 ai-player-skirmish removed it and its stage-4 tag)

/*-----------------------------------------------------------------------------
    Name        : aifBuildRequestedShips
    Description : Decides which build request to process depending on
                  which is more important - attack, defense, or resources
                  CAT (0x004045b0): no orders without a live mothership; an external ship (not a Beast's, not a
                  module) waits for the Sect docking bay module; the resource manager's support module request is
                  the race's own (an sMothershipSupport asked for a non-mothership becomes an sCarrierSupport and is
                  dropped, a bMothershipSupport asked for a non-bMothership is dropped) and every drop or failed
                  support-module build clears the request flag (+0xB8E); the resource ships wait for their cost;
                  the defense manager's budget overflow is compared unrounded (the exe keeps it on the x87)
    Inputs      :
    Outputs     :
    Return      : returns the chosen request
----------------------------------------------------------------------------*/

// a request's ship static and cost, or the request dropped (num_ships -1): the race and research tests of the
//      script, attack and defense requests (0x00404621..0x004047d7)
static ShipStaticInfo *aifRequestStatic(RequestShips *request, Player *player, ShipRace race, sdword *numships,
                                        sdword *totalcost)
{
    ShipStaticInfo *shipstatic;

    if ((request == NULL) || (request->num_ships <= 0))
    {
        return NULL;
    }
    if (RacesAllowedForGivenShip[request->shiptype] & RaceToRaceBits(race))
    {
        shipstatic = GetShipStaticInfo(request->shiptype, (ShipRace)cat_ShipTypeToRace((udword)request->shiptype));
        if ((shipstatic != NULL) && bitTest(shipstatic->common.infoFlags, IF_InfoLoaded) &&
            (!shipstatic->externalBuild || (race == RACE_Beast) || (shipstatic->shipclass == CLASS_Component) ||
             cat_playerHasCompletedShip(sMothershipDockingBay, player)))
        {
            *numships = request->num_ships;
            *totalcost = request->num_ships * shipstatic->buildCost;
            return shipstatic;
        }
    }
    aiplayerLog((player->playerIndex,"Warning could not build shiprace %d shiptype %d",race,request->shiptype));
    request->num_ships = -1;
    return NULL;
}

// a failed support-module build of the resource manager clears its request flag (0x00404be4)
static void aifSupportModuleNotBuilt(RequestShips *resourcerequest, sdword resourcenumships)
{
    if ((resourcenumships > 0) &&
        ((resourcerequest->shiptype == sMothershipSupport) || (resourcerequest->shiptype == sCarrierSupport) ||
         (resourcerequest->shiptype == bMothershipSupport)))
    {
        aiCurrentAIPlayer->catSupportModuleRequested = FALSE;
    }
}

void aifBuildRequestedShips(RequestShips *scriptrequest,RequestShips *attrequest,RequestShips *defrequest,RequestShips *resourcerequest)
{
    ShipStaticInfo *scriptshipstatic = NULL;
    sdword scriptnumships = 0;
    sdword scripttotalcost = 0;

    ShipStaticInfo *attshipstatic = NULL;
    sdword attnumships = 0;
    sdword atttotalcost = 0;

    ShipStaticInfo *defshipstatic = NULL;
    sdword defnumships = 0;
    sdword deftotalcost = 0;

    ShipStaticInfo *resourceshipstatic = NULL;
    sdword resourcenumships = 0;
    sdword resourcetotalcost = 0;

    Player *player = aiCurrentAIPlayer->player;
    Ship *playerMothership = player->PlayerMothership;
    sdword i;
    sdword RUsLeft = player->resourceUnits;
    ShipRace race;
    ShipType type;

    if ((playerMothership == NULL) || bitTest(playerMothership->header.flags, SOF_Dead))
    {
        return;     // can't build anything, return
    }

    race = playerMothership->shiprace;

    scriptshipstatic = aifRequestStatic(scriptrequest, player, race, &scriptnumships, &scripttotalcost);
    attshipstatic = aifRequestStatic(attrequest, player, race, &attnumships, &atttotalcost);
    defshipstatic = aifRequestStatic(defrequest, player, race, &defnumships, &deftotalcost);

    if ((resourcerequest != NULL) && (resourcerequest->num_ships > 0))
    {
        type = resourcerequest->shiptype;
        if (RacesAllowedForGivenShip[type] & RaceToRaceBits(race))
        {
            if ((type == sMothershipSupport) && (playerMothership->shiptype != sMothership))
            {
                resourcerequest->shiptype = sCarrierSupport;        // and dropped below, as the exe (0x00404827)
            }
            else if ((type == bMothershipSupport) && (playerMothership->shiptype != bMothership))
            {
                resourceshipstatic = NULL;
            }
            else
            {
                resourceshipstatic = GetShipStaticInfo(type, (ShipRace)cat_ShipTypeToRace((udword)type));
                if ((resourceshipstatic != NULL) && !bitTest(resourceshipstatic->common.infoFlags, IF_InfoLoaded))
                {
                    resourceshipstatic = NULL;
                }
            }
        }
        if (resourceshipstatic)
        {
            resourcenumships = (sdword)resourcerequest->num_ships;
            resourcetotalcost = resourcenumships * resourceshipstatic->buildCost;
        }
        else
        {
            aiplayerLog((player->playerIndex,"Warning Resource could not build shiprace %d shiptype %d",race,resourcerequest->shiptype));
            resourcerequest->num_ships = -1;
            aiCurrentAIPlayer->catSupportModuleRequested = FALSE;
        }
    }

    if ((scripttotalcost + resourcetotalcost + atttotalcost + deftotalcost) <= RUsLeft)
    {
        // we can build everything, so let's do that

        // build everything resourceman wants
        for (i=0;i<resourcenumships;i++)
        {
            BuildShip(resourceshipstatic,player,&aiCurrentAIPlayer->NumRUsSpentOnResourceman,&RUsLeft,resourcerequest->creator);
        }

        // build everything scriptman wants
        for (i=0;i<scriptnumships;i++)
        {
            BuildShip(scriptshipstatic,player,&aiCurrentAIPlayer->NumRUsSpentOnScriptman,&RUsLeft,scriptrequest->creator);
            scriptrequest->num_ships--;
        }

        // build everything attman wants
        for (i=0;i<attnumships;i++)
        {
            BuildShip(attshipstatic,player,&aiCurrentAIPlayer->NumRUsSpentOnAttman,&RUsLeft,attrequest->creator);
            attrequest->num_ships--;
        }

        // build everything defman wants
        for (i=0;i<defnumships;i++)
        {
            BuildShip(defshipstatic,player,&aiCurrentAIPlayer->NumRUsSpentOnDefman,&RUsLeft,defrequest->creator);
            defrequest->num_ships--;
        }
    }
    else
    {
        // now choose who gets to go first, second, and third

        real32 totalRUsSpent = (real32) (aiCurrentAIPlayer->NumRUsSpentOnAttman +
                                  aiCurrentAIPlayer->NumRUsSpentOnDefman +
                                  aiCurrentAIPlayer->NumRUsSpentOnResourceman);

        // CAT: the x87 evaluates each overflow at 53 bits; the attack and resource ones are stored as floats, the
        //      defense one stays on the stack (0x00404a19..0x00404a47)
        real32 attmanbudgetoverflow = (real32)(((double)aiCurrentAIPlayer->NumRUsSpentOnAttman / (double)totalRUsSpent) / (double)ATTMAN_BUILDPRIORITY_RATIO);
        double defmanbudgetoverflow = ((double)aiCurrentAIPlayer->NumRUsSpentOnDefman / (double)totalRUsSpent) / (double)DEFMAN_BUILDPRIORITY_RATIO;
        real32 resourcemanbudgetoverflow = (real32)(((double)aiCurrentAIPlayer->NumRUsSpentOnResourceman / (double)totalRUsSpent) / (double)RESMAN_BUILDPRIORITY_RATIO);

        if ((resourcenumships) && (resourcerequest->priority & REQUESTSHIPS_HIPRI))
        {
            resourcemanbudgetoverflow = -1.0f;
        }

        if ((attnumships) && (attrequest->priority & REQUESTSHIPS_HIPRI))
        {
            attmanbudgetoverflow = -1.0f;
        }

        if ((defnumships) && (defrequest->priority & REQUESTSHIPS_HIPRI))
        {
            defmanbudgetoverflow = -1.0;
        }

        if (attnumships == 0) attmanbudgetoverflow = REALlyBig;
        if (defnumships == 0) defmanbudgetoverflow = (double)REALlyBig;
        if (resourcenumships == 0) resourcemanbudgetoverflow = REALlyBig;

        if (scriptnumships)
        {
            for (i=0;(i<scriptnumships) && (RUsLeft >= scriptshipstatic->buildCost);i++)
            {
                BuildShip(scriptshipstatic,player,&aiCurrentAIPlayer->NumRUsSpentOnScriptman,&RUsLeft,scriptrequest->creator);
                scriptrequest->num_ships--;
            }
            return;
        }

        // CAT: the x87 compares treat an unordered pair (the 0 / 0 overflows before any RUs were spent) as "<=", so
        //      each "a <= b" of HW1 is "!(a > b)" here (0x00404b13, 0x00404b24, 0x00404b33, 0x00404cae, 0x00404cc1)
        if (!((double)attmanbudgetoverflow > defmanbudgetoverflow))
        {
            // attackman goes before defenseman
            if (!(attmanbudgetoverflow > resourcemanbudgetoverflow))
            {
                for (i=0;(i<attnumships) && (RUsLeft >= attshipstatic->buildCost);i++)
                {
                    BuildShip(attshipstatic,player,&aiCurrentAIPlayer->NumRUsSpentOnAttman,&RUsLeft,attrequest->creator);
                    attrequest->num_ships--;
                }
                aifSupportModuleNotBuilt(resourcerequest, resourcenumships);
                return;
            }
        }
        else
        {
            // defenseman goes before attackman
            if (!(defmanbudgetoverflow > (double)resourcemanbudgetoverflow))
            {
                for (i=0;(i<defnumships) && (RUsLeft >= defshipstatic->buildCost);i++)
                {
                    BuildShip(defshipstatic,player,&aiCurrentAIPlayer->NumRUsSpentOnDefman,&RUsLeft,defrequest->creator);
                    defrequest->num_ships--;
                }
                aifSupportModuleNotBuilt(resourcerequest, resourcenumships);
                return;
            }
        }

        // the resource manager goes first: CAT: only while its ship is affordable (0x00404c1a, 0x00404d7a)
        if (RUsLeft < resourceshipstatic->buildCost)
        {
            aifSupportModuleNotBuilt(resourcerequest, resourcenumships);
            return;
        }
        for (i=0;(i<resourcenumships) && (RUsLeft >= resourceshipstatic->buildCost);i++)
        {
            BuildShip(resourceshipstatic,player,&aiCurrentAIPlayer->NumRUsSpentOnResourceman,&RUsLeft,resourcerequest->creator);
        }
    }

    return;
}

void aifAttackManRequestsShipsCB(ShipType shiptype,sdword number,sdword priority)
{
    // add requests to Q
    RequestShips *requestShips = memAlloc(sizeof(RequestShips),"attackreqships",0);

    dbgAssertOrIgnore(number > 0);

    requestShips->shiptype  = shiptype;
    requestShips->num_ships = number;
    requestShips->priority  = priority;
    requestShips->creator   = aiCurrentAIPlayer->AICreator;

    if (priority & REQUESTSHIPS_HIPRI)
    {
        listAddNodeBeginning(&aiCurrentAIPlayer->AttackManRequestShipsQ,&requestShips->node,requestShips);
    }
    else
    {
        listAddNode(&aiCurrentAIPlayer->AttackManRequestShipsQ,&requestShips->node,requestShips);
    }
}

void aifDefenseManRequestsShipsCB(ShipType shiptype,sdword number,sdword priority)
{
    // add requests to Q
    RequestShips *requestShips = memAlloc(sizeof(RequestShips),"defreqships",0);

    dbgAssertOrIgnore(number > 0);

    requestShips->shiptype  = shiptype;
    requestShips->num_ships = number;
    requestShips->priority  = priority;
    requestShips->creator   = aiCurrentAIPlayer->AICreator;

    if (priority & REQUESTSHIPS_HIPRI)
    {
        listAddNodeBeginning(&aiCurrentAIPlayer->DefenseManRequestShipsQ,&requestShips->node,requestShips);
    }
    else
    {
        listAddNode(&aiCurrentAIPlayer->DefenseManRequestShipsQ,&requestShips->node,requestShips);
    }
}

void aifScriptManRequestsShipsCB(ShipType shiptype,sdword number,sdword priority)
{
    // add requests to Q
    RequestShips *requestShips = memAlloc(sizeof(RequestShips),"defreqships",0);

    dbgAssertOrIgnore(number > 0);

    requestShips->shiptype  = shiptype;
    requestShips->num_ships = number;
    requestShips->priority  = priority;
    requestShips->creator   = aiCurrentAIPlayer->ScriptCreator;

    if (priority & REQUESTSHIPS_HIPRI)
    {
        listAddNodeBeginning(&aiCurrentAIPlayer->ScriptManRequestShipsQ,&requestShips->node,requestShips);
    }
    else
    {
        listAddNode(&aiCurrentAIPlayer->ScriptManRequestShipsQ,&requestShips->node,requestShips);
    }
}

void aifResourceManRequestsShipsCB(ShipType shiptype,sdword number,sdword priority)
{
    dbgAssertOrIgnore(number > 0);

    aiCurrentAIPlayer->ResourceManRequestShips.shiptype = shiptype;
    aiCurrentAIPlayer->ResourceManRequestShips.num_ships = number;
    aiCurrentAIPlayer->ResourceManRequestShips.priority = priority;
    aiCurrentAIPlayer->ResourceManRequestShips.creator  = aiCurrentAIPlayer->AICreator;
}

#define constructTeamWaiting(twaiting,stype,nships,tm,dSetVar)   \
            (twaiting) = memAlloc(sizeof(TeamWaitingForTheseShips),"teamwaitingships",0);   \
            (twaiting)->shiptype = (stype);                                                 \
            (twaiting)->num_ships = (nships);                                               \
            (twaiting)->team = (tm);                                                        \
            dbgAssertOrIgnore(strlen(dSetVar) <= AIVAR_LABEL_MAX_LENGTH);                           \
            strcpy((twaiting)->doneSetVarStr,(dSetVar))

void aifTeamRequestsShipsCB(ShipType shiptype,sdword number,AITeam *team,char *doneSetVar, sdword priority)
{
    TeamWaitingForTheseShips *teamWaiting;

	aiplayerLog((aiIndex, "%x Requesting %i %ss", team, number, ShipTypeToStr(shiptype)));

    switch (team->teamType)
    {
        case AttackTeam:
            // put request in Q under attackman
            aifAttackManRequestsShipsCB(shiptype,number,priority);
            // add to AttackManTeamWaitingForShipsQ
            constructTeamWaiting(teamWaiting,shiptype,number,team,doneSetVar);
            if (priority & REQUESTSHIPS_HIPRI)
            {
                listAddNodeBeginning(&aiCurrentAIPlayer->AttackManTeamsWaitingForShipsQ,&teamWaiting->node,teamWaiting);
            }
            else
            {
                listAddNode(&aiCurrentAIPlayer->AttackManTeamsWaitingForShipsQ,&teamWaiting->node,teamWaiting);
            }
            break;

        case DefenseTeam:
            // put request in Q under defenseman
            aifDefenseManRequestsShipsCB(shiptype,number,priority);
            // add to DefenseManTeamWaitingForShipsQ
            constructTeamWaiting(teamWaiting,shiptype,number,team,doneSetVar);
            if (priority & REQUESTSHIPS_HIPRI)
            {
                listAddNodeBeginning(&aiCurrentAIPlayer->DefenseManTeamsWaitingForShipsQ,&teamWaiting->node,teamWaiting);
            }
            else
            {
                listAddNode(&aiCurrentAIPlayer->DefenseManTeamsWaitingForShipsQ,&teamWaiting->node,teamWaiting);
            }
            break;

        case ScriptTeam:
            // put request in Q under scriptman
            aifScriptManRequestsShipsCB(shiptype,number,priority);
            // add to DefenseManTeamWaitingForShipsQ
            constructTeamWaiting(teamWaiting,shiptype,number,team,doneSetVar);
            if (priority & REQUESTSHIPS_HIPRI)
            {
                listAddNodeBeginning(&aiCurrentAIPlayer->ScriptManTeamsWaitingForShipsQ,&teamWaiting->node,teamWaiting);
            }
            else
            {
                listAddNode(&aiCurrentAIPlayer->ScriptManTeamsWaitingForShipsQ,&teamWaiting->node,teamWaiting);
            }
            break;

        case ResourceTeam:
        default:
            dbgAssertOrIgnore(FALSE);       // not supported yet
            break;
    }
}

void RemoveTeamFromTeamWaitingQ(LinkedList *TeamWaitingQ,AITeam *team)
{
    Node *node = TeamWaitingQ->head;
    Node *nextnode;
    TeamWaitingForTheseShips *teamWaiting;

    while (node != NULL)
    {
        teamWaiting = (TeamWaitingForTheseShips *)listGetStructOfNode(node);
        nextnode = node->next;

        if (teamWaiting->team == team)
        {
            listDeleteNode(node);
        }

        node = nextnode;
    }
}

void aifTeamDied(AIPlayer *aiplayer,AITeam *team, bool32 removeAllReferencesToTeam)
{
    sdword i, j;
    MsgQueue *msgQP;

    RemoveTeamFromTeamWaitingQ(&aiplayer->AttackManTeamsWaitingForShipsQ,team);
    RemoveTeamFromTeamWaitingQ(&aiplayer->DefenseManTeamsWaitingForShipsQ,team);
    RemoveTeamFromTeamWaitingQ(&aiplayer->ScriptManTeamsWaitingForShipsQ,team);

    if (!removeAllReferencesToTeam)
    {
        //don't do anything else
        return;
    }

    // remove any messages from team that may still be queued up
    // CAT: the messages are the missions' constants, not copies: cleared, not freed (aifTeamDied 0x00405100)
    for (i = 0; i < aiCurrentAIPlayer->teamsUsed; ++i)
    {
        msgQP = aiCurrentAIPlayer->teams[i]->msgQueue;
        if (!msgQP) continue;
        for (j = 0; j < MSG_QUEUE_MAX_MSGS; ++j)
            if (msgQP->msgSenders[j] == team)
            {
                msgQP->msgs[j] = NULL;
                msgQP->msgSenders[j] = NULL;
            }
    }

    if (team->teamType == AttackTeam)
    {
        aiaTeamDied(aiplayer,team);
    }
    else if (team->teamType == DefenseTeam)
    {
        aidTeamDied(aiplayer,team);
    }
    else if (team->teamType == ResourceTeam)
    {
        airTeamDied(aiplayer,team);
    }
}

bool32 checkAddToTeam(LinkedList *TeamWaitingQ,Ship *ship)
{
    Node *node = TeamWaitingQ->head;
    TeamWaitingForTheseShips *teamWaiting;

    while (node != NULL)
    {
        teamWaiting = (TeamWaitingForTheseShips *)listGetStructOfNode(node);

        dbgAssertOrIgnore(teamWaiting->num_ships > 0);
        if (teamWaiting->shiptype == ship->shiptype)
        {
            teamWaiting->num_ships--;
            aitAddShip(teamWaiting->team,ship);
            if (teamWaiting->num_ships == 0)
            {
                aivarValueSet(aivarFind(teamWaiting->doneSetVarStr),TRUE);
                listDeleteNode(node);
            }
            return TRUE;
        }

        node = node->next;
    }

    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : cat_aifRequestSupportModule
    Description : CAT (0x00405210): in a multiplayer game the computer player asks its resource manager for a
                  support module of its mothership's kind (sMothershipSupport for an sMothership, else
                  sCarrierSupport; bMothershipSupport for a bMothership only) while below the unit cap and not
                  already asked; with none built yet and its research missing, the research is started instead
    Inputs      :
    Outputs     :
    Return      : TRUE when the request was made
----------------------------------------------------------------------------*/
bool32 cat_aifRequestSupportModule(void)
{
    Player *player;
    Ship *mothership;
    ShipType type;

    if (singlePlayerGame)
    {
        return FALSE;
    }
    player = aiCurrentAIPlayer->player;
    mothership = player->PlayerMothership;
    if ((mothership == NULL) || aiCurrentAIPlayer->catSupportModuleRequested)
    {
        return FALSE;
    }
    if (player->race == RACE_Sect)
    {
        type = (mothership->shiptype == sMothership) ? sMothershipSupport : sCarrierSupport;
    }
    else if ((player->race == RACE_Beast) && (mothership->shiptype == bMothership))
    {
        type = bMothershipSupport;
    }
    else
    {
        return FALSE;
    }
    if (player->shiptotals[type] >= cdLimitCaps[type])
    {
        return FALSE;
    }
    if ((player->shiptotals[type] == 0) && rmTechRequiredForShip(player, type))
    {
        rmResearchTechForShip(aiCurrentAIPlayer->player, type);
        return FALSE;
    }
    aiCurrentAIPlayer->ResourceManRequestShips.shiptype = type;
    aiCurrentAIPlayer->ResourceManRequestShips.num_ships = 1;
    aiCurrentAIPlayer->ResourceManRequestShips.priority = REQUESTSHIPS_HIPRI;
    aiCurrentAIPlayer->ResourceManRequestShips.creator = aiCurrentAIPlayer->AICreator;
    aiCurrentAIPlayer->catSupportModuleRequested = TRUE;
    return TRUE;
}

// a request a multiplayer computer player can act on now: a Beast ship its research has enabled, a module or a ship
//      its factory can build, with its research done (else started, outside the research delay); an external ship
//      (not a module) found here holds back the other queues (aifProcessShipBuildRequests 0x00405370..0x00405466,
//      the same at each queue)
static RequestShips *aifCatRequestUsable(RequestShips *request, Player *player, ubyte research_delay, bool32 *external)
{
    ShipStaticInfo *shipstatic;

    if ((player->race == RACE_Beast) && !cat_playerShipTypeEnabled(player, request->shiptype))
    {
        return NULL;
    }
    shipstatic = GetShipStaticInfo(request->shiptype, (ShipRace)cat_ShipTypeToRace((udword)request->shiptype));
    if ((shipstatic->shipclass != CLASS_Component) && !cmCanBuildShipType(player->PlayerMothership, request->shiptype, FALSE))
    {
        return NULL;
    }
    if (rmTechRequiredForShip(player, request->shiptype))
    {
        if (!research_delay)
        {
            rmResearchTechForShip(player, request->shiptype);
        }
        return NULL;
    }
    if (cat_rmTechRequiredForShipUpgrade(player, request->shiptype) && !research_delay)
    {
        rmResearchTechForShip(player, request->shiptype);
    }
    if ((shipstatic->shipclass != CLASS_Component) && shipstatic->externalBuild)
    {
        *external = TRUE;
    }
    return request;
}

// the first request of a queue (single player) or the first one usable (multiplayer)
static RequestShips *aifCatQueueRequest(LinkedList *queue, Player *player, ubyte research_delay, bool32 *external)
{
    RequestShips *request = NULL;
    Node *node;

    for (node = queue->head; (node != NULL) && (request == NULL); node = node->next)
    {
        request = (RequestShips *)listGetStructOfNode(node);
        if (!singlePlayerGame)
        {
            request = aifCatRequestUsable(request, player, research_delay, external);
        }
    }
    return request;
}

/*-----------------------------------------------------------------------------
    Name        : aifProcessShipBuildRequests
    Description : Goes through the attack, defense, resource and script ship build queues
                  CAT (0x004052f0): a single-player computer player takes each queue's first request as it is; a
                  multiplayer one asks for a support module (outside the research delay) and takes from each queue
                  the first request it can act on (aifCatRequestUsable), an external ship in one queue holding back
                  the queues after it, then researches its mothership / carrier upgrade (Sect) or Infection (Beast)
                  in a free lab; HW1's research of later queue entries and its technology deficit are gone (the
                  deficit is only cleared)
    Inputs      :
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aifProcessShipBuildRequests(void)
{
    RequestShips *attrequest = NULL,*defrequest = NULL,*resourcerequest = NULL,*scriptrequest = NULL;
    sdword olddefnumships    = 0;
    sdword oldattnumships    = 0;
    sdword oldscriptnumships = 0;
    Player *player = aiCurrentAIPlayer->player;
    ubyte research_delay;
    bool32 external = FALSE;
    sdword labnum;

    if (aiCurrentAIPlayer->ResearchDelay)
    {
        research_delay = --aiCurrentAIPlayer->ResearchDelay;
    }
    else
    {
        research_delay = 0;
        aiCurrentAIPlayer->ResearchDelay = AIF_RESEARCH_DELAY[aiCurrentAIPlayer->aiplayerDifficultyLevel];
    }

    aiCurrentAIPlayer->TechnologyDeficit = 0;

    if (!research_delay)
    {
        cat_aifRequestSupportModule();
    }

    if (aiCurrentAIPlayer->ResourceManRequestShips.num_ships > 0)
    {
        resourcerequest = &aiCurrentAIPlayer->ResourceManRequestShips;
        if (!singlePlayerGame)
        {
            resourcerequest = aifCatRequestUsable(resourcerequest, player, research_delay, &external);
        }
    }

    if ((aiCurrentAIPlayer->AttackManRequestShipsQ.head != NULL) && !external)
    {
        attrequest = aifCatQueueRequest(&aiCurrentAIPlayer->AttackManRequestShipsQ, player, research_delay, &external);
        if (attrequest)
        {
            oldattnumships = attrequest->num_ships;
        }
    }

    if ((aiCurrentAIPlayer->DefenseManRequestShipsQ.head != NULL) && !external)
    {
        defrequest = aifCatQueueRequest(&aiCurrentAIPlayer->DefenseManRequestShipsQ, player, research_delay, &external);
        if (defrequest)
        {
            olddefnumships = defrequest->num_ships;
        }
    }

    //it is assumed that any ship requested by the script is researched
    //therefore no research is done for the script system and no checking is done either
    if ((aiCurrentAIPlayer->ScriptManRequestShipsQ.head != NULL) && !external)
    {
        scriptrequest     = (RequestShips *)listGetStructOfNode(aiCurrentAIPlayer->ScriptManRequestShipsQ.head);
        oldscriptnumships = scriptrequest->num_ships;
    }

    // CAT: a multiplayer computer player researches its mothership's (then its carrier's) upgrade, or Infection for
    //      the Beast (0x00405701)
    if (!singlePlayerGame && ((labnum = rmFindFreeLab(player, TECH_NONE)) != -1))
    {
        if (player->race == RACE_Sect)
        {
            if (cat_rmTechRequiredForShipUpgrade(player, sMothership))
            {
                rmResearchTechForShip(player, sMothership);
            }
            else if (cat_rmTechRequiredForShipUpgrade(player, sCarrier))
            {
                rmResearchTechForShip(player, sCarrier);
            }
        }
        else if ((player->race == RACE_Beast) && !cat_rmTechSetHas(player->researchinfo.HasTechnology, TECH_INFECT))
        {
            clWrapResearch(0, player->playerIndex, (udword)labnum, TECH_INFECT);
        }
    }

    aifBuildRequestedShips(scriptrequest,attrequest,defrequest,resourcerequest);

    if (scriptrequest != NULL)
    {
        // check and see if we built any ships, and if we should remove from Q
        if (oldscriptnumships != scriptrequest->num_ships)
        {
            aiCurrentAIPlayer->ScriptManShipsBeingBuilt.NumShipsOfType[scriptrequest->shiptype] += (ubyte)(oldscriptnumships - scriptrequest->num_ships);
        }

        if (scriptrequest->num_ships <= 0)
        {
            // remove from Q
            listDeleteNode(&scriptrequest->node);
        }
    }

    if (attrequest != NULL)
    {
        // check and see if we built any ships, and if we should remove from Q
        if (oldattnumships != attrequest->num_ships)
        {
            aiCurrentAIPlayer->AttackManShipsBeingBuilt.NumShipsOfType[attrequest->shiptype] += (ubyte)(oldattnumships - attrequest->num_ships);
        }

        if (attrequest->num_ships <= 0)
        {
            // remove from Q
            listDeleteNode(&attrequest->node);
        }
    }

    if (defrequest != NULL)
    {
        // check and see if we built any ships, and if we should remove from Q
        if (olddefnumships != defrequest->num_ships)
        {
            aiCurrentAIPlayer->DefenseManShipsBeingBuilt.NumShipsOfType[defrequest->shiptype] += (ubyte)(olddefnumships - defrequest->num_ships);
        }

        if (defrequest->num_ships <= 0)
        {
            // remove from Q
            listDeleteNode(&defrequest->node);
        }
    }
}



/*-----------------------------------------------------------------------------
    Name        : aifAssignNewShips
    Description : Checks who is requesting new ships and assigns them to the proper manager
    Inputs      :
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aifAssignNewShips(void)
{
    GrowSelection *newships = &aiCurrentAIPlayer->newships;
    sdword i;
    ShipPtr ship;

    for (i=0;i<newships->selection->numShips;)
    {
        ship = newships->selection->ShipPtr[i];

        if ( (aiCurrentAIPlayer->ScriptManShipsBeingBuilt.NumShipsOfType[ship->shiptype] > 0) &&
             checkAddToTeam(&aiCurrentAIPlayer->ScriptManTeamsWaitingForShipsQ,ship) )

        {
            // script manager requested this ship to be built, so give it to scriptman.
            aiCurrentAIPlayer->ScriptManShipsBeingBuilt.NumShipsOfType[ship->shiptype]--;
            growSelectRemoveShip(newships,ship);
            continue;       // check same index again because we removed a ship from selection we are scanning
        }
        else if ( (aiCurrentAIPlayer->AttackManShipsBeingBuilt.NumShipsOfType[ship->shiptype] > 0) &&
                  (checkAddToTeam(&aiCurrentAIPlayer->AttackManTeamsWaitingForShipsQ,ship)) )
        {
            // attack manager requested this ship to be built, so give it to attackman.
            aiCurrentAIPlayer->AttackManShipsBeingBuilt.NumShipsOfType[ship->shiptype]--;
            growSelectRemoveShip(newships,ship);
            continue;       // check same index again because we removed a ship from selection we are scanning
        }
        else if ( (aiCurrentAIPlayer->DefenseManShipsBeingBuilt.NumShipsOfType[ship->shiptype] > 0) &&
                  (checkAddToTeam(&aiCurrentAIPlayer->DefenseManTeamsWaitingForShipsQ,ship)) )
        {
            // defense manager requested this ship to be built, so give it to defenseman.
            aiCurrentAIPlayer->DefenseManShipsBeingBuilt.NumShipsOfType[ship->shiptype]--;
            growSelectRemoveShip(newships,ship);
            continue;       // check same index again because we removed a ship from selection we are scanning
        }
        // CAT: the resource manager takes Cataclysm's motherships, carriers and modules by class (aifAssignNewShips
        //      0x00405997: classes Resource, NonCombat, Mothership, Carrier and Component) and the Kushan and Taiidan
        //      advance support frigates
        else if ((ship->staticinfo->shipclass == CLASS_Resource) ||
            (ship->staticinfo->shipclass == CLASS_NonCombat) ||
            (ship->staticinfo->shipclass == CLASS_Mothership) ||
            (ship->staticinfo->shipclass == CLASS_Carrier) ||
            (ship->staticinfo->shipclass == CLASS_Component) ||
            (ship->shiptype == kAdvanceSupportFrigate) ||
            (ship->shiptype == tAdvanceSupportFrigate))
        {
            growSelectRemoveShip(newships,ship);
            airAddNewShip(ship);
            continue;       // check same index again because we removed a ship from selection we are scanning
        }
        else
        {
            // leave in reserves
        }
        i++;
    }
}


/*=============================================================================
    Hyperspacing Logic Code:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : aifHyperspaceInit
    Description : Initializes any hyperspacing stuff
    Inputs      : aiplayer - the CPU to do hyperspacing initialization for
    Outputs     : Whatever's needed to initialize hyperspacing
    Return      : void
----------------------------------------------------------------------------*/
void aifHyperspaceInit(AIPlayer *aiplayer)
{
    //determine what percentage of RUs to skim for hyperspacing
    //for now, just take off 8%
    aiplayer->aifHyperSkimming = 0.08f;
}


/*-----------------------------------------------------------------------------
    Name        : aifSkimHyperspaceRUs
    Description : Banks RUs for Hyperspacing
    Inputs      : none
    Outputs     : transfers some new RUs to the aiplayer structure
    Return      : void
----------------------------------------------------------------------------*/
void aifSkimHyperspaceRUs(void)
{
    udword newResources, skimResources;
    Player *player = aiCurrentAIPlayer->player;

    if (aiCurrentAIPlayer->aifHyperSavings < 0)
    {   // if the CPU is in debt, all new resources go to pay off the debt
        if (player->resourceUnits)
        {
            if (player->resourceUnits < (-(aiCurrentAIPlayer->aifHyperSavings)))
            {   // the players resources do not exceed the debt.  Pay off as much as possible
                aiplayerLog((aiIndex, "Paying off the debt.  Savings = %i, deposit = %i", aiCurrentAIPlayer->aifHyperSavings, player->resourceUnits));
                aiCurrentAIPlayer->aifHyperSavings += player->resourceUnits;
                player->resourceUnits = 0;
            }
            else
            {   // the players resources exceed the debt.  Pay off everything.
                aiplayerLog((aiIndex, "Paying off the entire debt."));
                player->resourceUnits += aiCurrentAIPlayer->aifHyperSavings;
                aiCurrentAIPlayer->aifHyperSavings = 0;
            }
        }
    }
    else if (player->resourceUnits > aiCurrentAIPlayer->aifLastRUCount)
    {   // if the CPU has acquired new resources, skim some off
        newResources  = player->resourceUnits - aiCurrentAIPlayer->aifLastRUCount;
        skimResources = (udword)((real32)newResources * aiCurrentAIPlayer->aifHyperSkimming);
        aiCurrentAIPlayer->aifHyperSavings += skimResources;
        player->resourceUnits              -= skimResources;

        aiplayerLog((aiIndex, "Skimming %i RUs for Hyperspacing.  Savings = %i, RUs = %i", skimResources, aiCurrentAIPlayer->aifHyperSavings, player->resourceUnits));
    }
    aiCurrentAIPlayer->aifLastRUCount = player->resourceUnits;
}


/*=============================================================================
    Ally Logic:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : aifFindAllies
    Description : Looks at the other CPUs and allies with those that
                  have the same primary enemy
    Inputs      :
    Outputs     : Sets up some alliances
    Return      : void
----------------------------------------------------------------------------*/
extern udword AIPLAYER_BIGOTRY[];

void aifFindAllies(void)
{
    udword i;
    AIPlayer *OtherCPU;
    udword HowMuchDoILoveMyFriends;

    HowMuchDoILoveMyFriends = randyrandom(RANDOM_AI_PLAYER, 100);

    for (i=0;i<universe.numPlayers;i++)
    {
        if (universe.players[i].aiPlayer &&
            (universe.players[i].playerState == PLAYER_ALIVE) &&
            (universe.players[i].aiPlayer != aiCurrentAIPlayer))
        {                   // if the player is a living CPU but not this CPU
            OtherCPU = universe.players[i].aiPlayer;

            if (OtherCPU->primaryEnemyPlayer == aiCurrentAIPlayer->primaryEnemyPlayer)
            {               // if their primary enemies are the same
                if ((HowMuchDoILoveMyFriends < AIPLAYER_BIGOTRY[i]) &&
                    !allianceArePlayersAllied(OtherCPU->player, aiCurrentAIPlayer->player))
                {           // if the CPU wants to be allied if they aren't allied
                    //create an alliance
                    clWrapSetAlliance(ALLIANCE_FORM_NEW_ALLIANCE,
                                      OtherCPU->player->playerIndex,
                                      aiCurrentAIPlayer->player->playerIndex);
                }
            }
            else
            {               // their primary enemies aren't the same
                if (allianceArePlayersAllied(OtherCPU->player, aiCurrentAIPlayer->player))
                {           // if they are already allied
                    //break the alliance
                    clWrapSetAlliance(ALLIANCE_BREAK_ALLIANCE,
                                      OtherCPU->player->playerIndex,
                                      aiCurrentAIPlayer->player->playerIndex);
                }
            }
        }
    }
    aiCurrentAIPlayer->recalculateAllies = FALSE;
}


/*=============================================================================
    Fleet Commands:
=============================================================================*/



/*-----------------------------------------------------------------------------
    Name        : aifFleetCommand
    Description : Logic for the top of the computer player command structure
    Inputs      : aiplayer - the computer player structure
    Outputs     : lotsa stuff
    Return      : void
----------------------------------------------------------------------------*/
void aifFleetCommand(void)
{
    Player *player = aiCurrentAIPlayer->player;
    bool32 hasFleetControl = (singlePlayerGame) ? singlePlayerGameInfo.giveComputerFleetControl : TRUE;

    aiIndex = aiCurrentAIPlayer->player->playerIndex;

    // CAT: no HW1 P2 pirate fleet command (the P2 race and its swarm ships are gone) (M2-R4 ships-b)

    if (aiCurrentAIPlayer->recalculateAllies)
    {
        aifFindAllies();
    }

    aiuCreateBlobArrays(player);
    aiuUpdateKnowledgeOfEnemyShips(aiCurrentAIPlayer);
    aidClearDistressSignal(aiCurrentAIPlayer);

    aifAssignNewShips();

    if (aiCurrentAIPlayer->firstTurn)
    {
        if (hasFleetControl)
        {
            aiaProcessSpecialTeams();
//        airProcessSpecialTeams();
        }
        aiCurrentAIPlayer->firstTurn = FALSE;
    }
    else
    {
        if (hasFleetControl)
        {
            if (aiuResourceFeatureEnabled(AIF_HYPERSPACING))
            {
                aifSkimHyperspaceRUs();
            }

            aiCurrentAIPlayer->ResourceManRequestShips.num_ships = 0;

            //call AttackMan - any ships requested will result in call to aifAttackManRequestsShipsCB
            aiaAttackManager();

            //call DefenseMan - any ships requested will result in call to aifDefenseManRequestsShipsCB
            aidDefenseManager();

            //call Resource Manager - any ships requested will result in call to aifResourceManRequestsShipsCB
            airResourceManager();
        }
    }

    // call Team Manager - any ships requested will result in call to aifAttackManRequestsShipsCB or aifDefenseManRequestsShipsCB
    aitExecute();

    aifProcessShipBuildRequests();

    aiuDeleteBlobArrays();
}




