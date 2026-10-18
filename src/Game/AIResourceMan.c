// =============================================================================
//  AIResourceMan.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/27/1998 by gshaw
// =============================================================================

#include "AIResourceMan.h"

#include "AIDefenseMan.h"
#include "AIFeatures.h"
#include "AIFleetMan.h"
#include "AIOrders.h"
#include "CommandWrap.h"
#include "Dock.h"
#include "MultiplayerGame.h"
#include "Randy.h"
#include "Select.h"
#include "StatScript.h"
#include "UnivUpdate.h"


udword UPDATE_RU_COUNT_RATE = 3;

sdword BUILD_RC_CASH_TABLE_BASE = 400;
sdword BUILD_RC_CASH_TABLE_INC = 200;

sdword MAX_RCOLLECTORS_PER_DOCK_POINT = 3;

sdword MAX_RCOLLECTORS_TO_BUILD = 5;

sdword MIN_WORTHWHILE_RUs = 400;

sdword RUs_PER_RCOLLECTOR = 1000;


sdword MIN_WORTHWHILE_RUs_FOR_CONTROLLER = 3000;

sdword MIN_RCOLLECTORS_FOR_CONTROLLER = 4;

sdword BUILD_RCONTROLLER_CASH_BASE = 1500;

sdword BUILD_ASF_CASH_BASE = 2000;

// CAT: the multiplayer carrier request of airResourceManager 0x00413240 (.data 0x0074d33c / 0x0074d340, tweakable as
//      the exe's table 0x0074d360)
sdword BUILD_CARRIER_CASH_BASE = 6000;
sdword BUILD_CARRIER_SU_POINT = 30;

real32 ASF_POSITION_TO_ENEMY_MOTHERSHIP = 0.4f;

real32 RCONTROLLER_POS_FACTOR_FRIENDLIES_PRESENT = 1.1f;
real32 RCONTROLLER_POS_FACTOR_ENEMIESPRESENT_BUT_OUTNUMBERED = 0.6f;
real32 RCONTROLLER_POS_FACTOR_ENEMIESPRESENT = 0.3f;

real32 RCONTROLLER_POS_FACTOR_INBETWEEN = 1.0f;
real32 RCONTROLLER_POS_FACTOR_NOTINBETWEEN = 0.1f;

scriptEntry AIResourceManTweaks[] =
{
    makeEntry(UPDATE_RU_COUNT_RATE,scriptSetUdwordCB),
    makeEntry(BUILD_RC_CASH_TABLE_BASE,scriptSetSdwordCB),
    makeEntry(BUILD_RC_CASH_TABLE_INC,scriptSetSdwordCB),
    makeEntry(MAX_RCOLLECTORS_PER_DOCK_POINT,scriptSetSdwordCB),
    makeEntry(MAX_RCOLLECTORS_TO_BUILD,scriptSetSdwordCB),
    makeEntry(MIN_WORTHWHILE_RUs,scriptSetSdwordCB),
    makeEntry(RUs_PER_RCOLLECTOR,scriptSetSdwordCB),
    makeEntry(MIN_WORTHWHILE_RUs_FOR_CONTROLLER,scriptSetSdwordCB),
    makeEntry(MIN_RCOLLECTORS_FOR_CONTROLLER,scriptSetSdwordCB),
    makeEntry(BUILD_RCONTROLLER_CASH_BASE,scriptSetSdwordCB),
    makeEntry(ASF_POSITION_TO_ENEMY_MOTHERSHIP,scriptSetReal32CB),
    makeEntry(BUILD_ASF_CASH_BASE,scriptSetSdwordCB),
    makeEntry(BUILD_CARRIER_CASH_BASE,scriptSetSdwordCB),       // CAT
    makeEntry(BUILD_CARRIER_SU_POINT,scriptSetSdwordCB),        // CAT
    makeEntry(RCONTROLLER_POS_FACTOR_FRIENDLIES_PRESENT,scriptSetReal32CB),
    makeEntry(RCONTROLLER_POS_FACTOR_ENEMIESPRESENT_BUT_OUTNUMBERED,scriptSetReal32CB),
    makeEntry(RCONTROLLER_POS_FACTOR_ENEMIESPRESENT,scriptSetReal32CB),
    makeEntry(RCONTROLLER_POS_FACTOR_INBETWEEN,scriptSetReal32CB),
    makeEntry(RCONTROLLER_POS_FACTOR_NOTINBETWEEN,scriptSetReal32CB),

    END_SCRIPT_ENTRY
};

/*-----------------------------------------------------------------------------
    Name        : NumberOfEasilyAccesibleRUs
    Description : returns total number of easily accesible RU's to player
    Inputs      :
    Outputs     :
    Return      : returns total number of easily accesible RU's to player
----------------------------------------------------------------------------*/
sdword NumberOfEasilyAccesibleRUs(Player *player)
{
    // for now, just return the total number of RU's.
    Node *objnode = universe.ResourceList.head;
    Resource *resource;
    sdword numRUs = 0;

    while (objnode != NULL)
    {
        resource = (Resource *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(resource->header.flags & SOF_Resource);

        numRUs += resource->resourcevalue;

        objnode = objnode->next;
    }

    return numRUs;
}

/*-----------------------------------------------------------------------------
    Name        : NumRUDockPointsOnThisShip
    Description : returns number of RU dock points on ship
                  CAT (0x00412490): two for the Kushan and Taiidan carriers, else one (the mothership is no longer
                  listed)
    Inputs      :
    Outputs     :
    Return      : returns number of RU dock points on ship
----------------------------------------------------------------------------*/
sdword NumRUDockPointsOnThisShip(Ship *ship)
{
    if ((ship->shiptype == kCarrier) || (ship->shiptype == tCarrier))
    {
        return 2;
    }
    else
    {
        return 1;
    }
}

/*-----------------------------------------------------------------------------
    Name        : airInit
    Description : initialize resource manager stuff in aiplayer
                  CAT (0x004124b0): also clears the support-module count and request flag
    Inputs      : aiplayer
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void airInit(AIPlayer *aiplayer)
{
    udword i;

    growSelectInit(&aiplayer->airResourceReserves);
    growSelectInit(&aiplayer->airResourceCollectors);
    aiplayer->airEasilyAccesibleRUsInWorld = -1;
    aiplayer->airNumRCollectors            = 0;
    aiplayer->airNumRControllers           = 0;
    aiplayer->airNumASF                    = 0;
    aiplayer->airNumResearchShips          = 0;
    aiplayer->catAirNumSupportModules      = 0;     // CAT: 0x004124ff
    aiplayer->numSupportTeams              = 0;
    aiplayer->ResourcersToBuild            = 0;
    aiplayer->catSupportModuleRequested    = FALSE; // CAT: 0x00412511

    for (i=0;i<AIPLAYER_NUM_SUPPORTTEAMS;i++)
    {
        aiplayer->supportTeam[i] = NULL;
    }

    switch (aiplayer->aiplayerDifficultyLevel)
    {
        case AI_ADVANCED:
            aiuEnableResourceFeature(AIR_ACTIVE_MOTHERSHIP);
            aiuEnableResourceFeature(AIR_ACTIVE_RESOURCE_COLLECTION);
            aiuEnableResourceFeature(AIR_ACTIVE_SUPPORT_FRIGATE);
            aiuEnableResourceFeature(AIR_RESOURCE_DISTRESS_SIGNALS);
            aiuEnableResourceFeature(AIR_SMART_COLLECTOR_REQUESTS);
            aiuEnableResourceFeature(AIR_SMART_RESEARCH_SHIP_REQUESTS);
        case AI_INTERMEDIATE:
            aiuEnableResourceFeature(AIR_AGGRESSIVE_RESOURCING);
            aiuEnableResourceFeature(AIR_RESOURCE_CONTROLLER_REQUESTS);
            aiuEnableResourceFeature(AIR_SUPPORT_FRIGATE_REQUESTS);
        case AI_BEGINNER:
            aiuEnableResourceFeature(AIR_ACTIVE_RESOURCE_CONTROLLER);
            break;
        default:
            dbgAssertOrIgnore(FALSE);
    }
}

/*-----------------------------------------------------------------------------
    Name        : airClose
    Description : Closes resource manager stuff in aiplayer
    Inputs      : aiplayer
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void airClose(AIPlayer *aiplayer)
{
    udword i;

    growSelectClose(&aiplayer->airResourceReserves);
    growSelectClose(&aiplayer->airResourceCollectors);

    for (i=0;i<AIPLAYER_NUM_SUPPORTTEAMS;i++)
    {
        if (aiplayer->supportTeam[i])
        {
            aitDestroy(aiplayer, aiplayer->supportTeam[i], FALSE);
            aiplayer->supportTeam[i] = NULL;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : airShipDied
    Description : Removes a dead ship from the Combat Manager's internal structures
                  CAT (0x00412590): Cataclysm's resource ships by type: the Workers and the Taiidan collector, the
                  processors (HW1's resource-controller count), the advance support frigates and the support
                  modules
    Inputs      : aiplayer, ship - the ship that died
    Outputs     :
    Return      : TRUE if the ship was found and removed
----------------------------------------------------------------------------*/
bool32 airShipDied(AIPlayer *aiplayer,ShipPtr ship)
{
    if (growSelectRemoveShip(&aiplayer->airResourceReserves, ship))
    {
        switch (ship->shiptype)
        {
            case sWorker:
            case bWorker:
            case tResourceCollector:
                growSelectRemoveShip(&aiplayer->airResourceCollectors, ship);
                aiplayer->airNumRCollectors--;
                break;
            case sProcessor:
            case bProcessor:
                aiplayer->airNumRControllers--;
                break;
            case kAdvanceSupportFrigate:
            case tAdvanceSupportFrigate:
                aiplayer->airNumASF--;
                break;
            case sCarrierSupport:
            case sMothershipSupport:
            case bMothershipSupport:
                aiplayer->catAirNumSupportModules--;
                break;
            default:
                break;
        }
        return TRUE;
    }
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : airTeamDied
    Description : Removes a team from the resource stuff and all that jazz
    Inputs      : aiplayer, team
    Outputs     : does some things
    Return      : void
----------------------------------------------------------------------------*/
void airTeamDied(AIPlayer *aiplayer, AITeam *team)
{
    udword i;

    for (i=0;i<aiplayer->numSupportTeams;i++)
    {
        if (aiplayer->supportTeam[i] == team)
        {
            aiplayer->numSupportTeams--;
            aiplayer->supportTeam[i] = aiplayer->supportTeam[aiplayer->numSupportTeams];
            aiplayer->supportTeam[aiplayer->numSupportTeams] = NULL;
        }
    }
}


/*-----------------------------------------------------------------------------
    Name        : airAddedResourceCollector
    Description : called when a resource collector is added
                  CAT (0x004126f0): counted first; in a multiplayer game without harvesting the collector is
                  retired at once (dock purpose 7); the tactics are set either way (aggressive or evasive)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void airAddedResourceCollector(Ship *ship)
{
    Resource *resource;
    SelectCommand selectone;
    udword new_team_num = aiCurrentAIPlayer->numSupportTeams;
    AITeam *team;

    aiCurrentAIPlayer->airNumRCollectors++;
    if (aiCurrentAIPlayer->NumRCollectorsBeingBuilt > 0) aiCurrentAIPlayer->NumRCollectorsBeingBuilt--;

    if (!singlePlayerGame && !bitTest(tpGameCreated.flag, MG_HarvestinEnabled))
    {
        selectone.numShips = 1;
        selectone.ShipPtr[0] = ship;
        clWrapDock(&universe.mainCommandLayer, &selectone, CAT_DOCK_RETIRE, NULL);     // CAT: 0x0041275b
        return;
    }

    growSelectAddShip(&aiCurrentAIPlayer->airResourceCollectors, ship);

    if (aiuResourceFeatureEnabled(AIR_ACTIVE_RESOURCE_COLLECTION))
    {
        //add the resource collector to an existing team, or make a new team
        team = aitFindNextTeamWithFlag(NULL, RESOURCECOLLECT_TEAM);

        while (team)
        {
            if (team->shipList.selection->numShips < 3)
            {
                aitAddShip(team, ship);
                break;
            }
            team = aitFindNextTeamWithFlag(team, RESOURCECOLLECT_TEAM);
        }

        if (!team)
        {
            team = aiCurrentAIPlayer->supportTeam[new_team_num] = aitCreate(ResourceTeam);
            bitSet(aiCurrentAIPlayer->supportTeam[new_team_num]->teamFlags, RESOURCECOLLECT_TEAM);
            growSelectAddShip(&aiCurrentAIPlayer->supportTeam[new_team_num]->shipList, ship);
            aioCreateResourcer(aiCurrentAIPlayer->supportTeam[new_team_num]);
            aiCurrentAIPlayer->numSupportTeams++;
        }
        // CAT: evasive when not aggressive (0x00412850)
        aiuWrapSetTactics(team->shipList.selection, aiuResourceFeatureEnabled(AIR_AGGRESSIVE_RESOURCING) ? Aggressive : Evasive);
    }
    else
    {
        resource = univFindNearestResource(ship, 0, NULL, FALSE);   // CAT: the exe's 4th argument 0 (airAddedResourceCollector 0x00412865)

        selectone.numShips = 1;
        selectone.ShipPtr[0] = ship;

        // CAT: evasive when not aggressive (0x00412899)
        aiuWrapSetTactics(&selectone, aiuResourceFeatureEnabled(AIR_AGGRESSIVE_RESOURCING) ? Aggressive : Evasive);
        aiuWrapCollectResource(&selectone,resource);
    }
}

bool32 FindBestPlaceForAdvanceSupportFrigate(vector *destination)
{
    Ship *myMothership = aiCurrentAIPlayer->player->PlayerMothership;
    Ship *enemyMothership = aiuFindEnemyMothership(aiCurrentAIPlayer->player);
    vector diff;

    if (myMothership == NULL)
    {
        return FALSE;
    }

    if (enemyMothership == NULL)
    {
        return FALSE;
    }

    vecSub(diff,enemyMothership->obj.posinfo.position,myMothership->obj.posinfo.position);

    vecMultiplyByScalar(diff,ASF_POSITION_TO_ENEMY_MOTHERSHIP);

    vecAdd(*destination,myMothership->obj.posinfo.position,diff);

    return TRUE;
}

// a support team of one new ship and its support order (airAddNewShip 0x00412950, inlined at each type)
static void airAddSupportTeam(Ship *ship, sdword supportType)
{
    udword new_team_num = aiCurrentAIPlayer->numSupportTeams;

    aiCurrentAIPlayer->supportTeam[new_team_num] = aitCreate(ResourceTeam);
    growSelectAddShip(&aiCurrentAIPlayer->supportTeam[new_team_num]->shipList, ship);
    aioCreateActiveSupport(aiCurrentAIPlayer->supportTeam[new_team_num], NULL, supportType);
    aiCurrentAIPlayer->numSupportTeams++;
}

/*-----------------------------------------------------------------------------
    Name        : airAddNewShip
    Description : Adds newly assigned ships to the shipreserve structure
                  CAT (0x00412950): HW1's helpers inlined, by Cataclysm's types: every carrier and mothership gets
                  a support team (with AIR_ACTIVE_MOTHERSHIP); a support module is counted (and the module request
                  flag cleared); the Workers and the Taiidan collector go to airAddedResourceCollector; a processor
                  gets a resource support team or is sent to the best resource blob and counts in HW1's
                  resource-controller count; an advance support frigate as in HW1; a proximity sensor joins the
                  defense manager's sensors
    Inputs      : newsel - selection of the new ships
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void airAddNewShip(Ship *ship)
{
    SelectCommand selectone;
    vector destination;

    growSelectAddShip(&aiCurrentAIPlayer->airResourceReserves, ship);

    switch (ship->shiptype)
    {
        case sCarrier:
        case sMothership:
        case bCarrier:
        case bMothership:
        case kCarrier:
        case tCarrier:
            if (aiuResourceFeatureEnabled(AIR_ACTIVE_MOTHERSHIP))
            {
                airAddSupportTeam(ship, SUPPORT_MOTHERSHIP);
            }
            break;

        case sCarrierSupport:
        case sMothershipSupport:
        case bMothershipSupport:
            aiCurrentAIPlayer->catAirNumSupportModules++;
            aiCurrentAIPlayer->catNumSupportModulesBeingBuilt--;        // the exe does not test for 0 (0x00412c22)
            aiCurrentAIPlayer->catSupportModuleRequested = FALSE;
            break;

        case sWorker:
        case bWorker:
        case tResourceCollector:
            airAddedResourceCollector(ship);
            break;

        case sProcessor:
        case bProcessor:
            if (aiuResourceFeatureEnabled(AIR_ACTIVE_RESOURCE_CONTROLLER))
            {
                airAddSupportTeam(ship, SUPPORT_RESOURCE);
            }
            else if (aiuFindBestResourceBlob(&destination))
            {
                selectone.numShips = 1;
                selectone.ShipPtr[0] = ship;
                aiuWrapMove(&selectone,destination);
            }
            aiCurrentAIPlayer->airNumRControllers++;
            if (aiCurrentAIPlayer->NumRControllersBeingBuilt > 0) aiCurrentAIPlayer->NumRControllersBeingBuilt--;
            break;

        case kAdvanceSupportFrigate:
        case tAdvanceSupportFrigate:
            if (aiuResourceFeatureEnabled(AIR_ACTIVE_SUPPORT_FRIGATE))
            {
                airAddSupportTeam(ship, SUPPORT_STRIKECRAFT);
            }
            else if (FindBestPlaceForAdvanceSupportFrigate(&destination))
            {
                selectone.numShips = 1;
                selectone.ShipPtr[0] = ship;
                aiuWrapMove(&selectone,destination);
            }
            aiCurrentAIPlayer->airNumASF++;
            if (aiCurrentAIPlayer->NumASFBeingBuilt > 0) aiCurrentAIPlayer->NumASFBeingBuilt--;
            break;

        case kProximitySensor:
        case tProximitySensor:
            if (!aiCurrentAIPlayer->aidProximitySensors)
            {
                aiuNewSelection(aiCurrentAIPlayer->aidProximitySensors, 10, "dproxsens");
            }
            selSelectionAddSingleShip((MaxSelection *)aiCurrentAIPlayer->aidProximitySensors, ship);
            break;

        default:
            break;
    }
}

/*-----------------------------------------------------------------------------
    Name        : aiuCanBuildShipType
    Description : Returns TRUE if current AI player can build ships of shiptype
                  CAT (0x00412cb0): no orders from a dead mothership; the harvest switch of a multiplayer game
                  holds back every resource-class ship (HW1: the ResourceCollector type)
    Inputs      : shiptype
    Outputs     :
    Return      : Returns TRUE if current AI player can build ships of shiptype
----------------------------------------------------------------------------*/
bool32 aiuCanBuildShipType(ShipType shiptype,bool32 scriptteam)
{
    Ship *playerMothership = aiCurrentAIPlayer->player->PlayerMothership;
    ShipStaticInfo *teststatic;
    ShipRace race;

    if ((playerMothership == NULL) || bitTest(playerMothership->header.flags, SOF_Dead))
    {
        return FALSE;
    }

    race = playerMothership->shiprace;

    if ((RacesAllowedForGivenShip[shiptype] & RaceToRaceBits(race)) == 0)
    {
        // Can't build this shiptype for this race
        return FALSE;
    }

    teststatic = GetShipStaticInfo(shiptype,(ShipRace)cat_ShipTypeToRace((udword)shiptype));
    if (!bitTest(teststatic->common.infoFlags, IF_InfoLoaded))
    {
        return FALSE;       // this ship static info not loaded, so can't build
    }

    if ((teststatic->shipclass == CLASS_Resource) && (!singlePlayerGame) && (!bitTest(tpGameCreated.flag,MG_HarvestinEnabled)))
    {
        return FALSE;
    }

    if (scriptteam)
    {
        return TRUE;
    }
    else
    {
        return cmCanBuildShipType(playerMothership,shiptype,FALSE);     // later set to TRUE to take into account research
    }


}


/*-----------------------------------------------------------------------------
    Name        : airCheckForDistressSignals
    Description : Checks all resource ships to make sure they aren't being attacked.
                  If they are, a distress signal is sent to the defense manager
    Inputs      : None
    Outputs     : Creates a selection of distressed ships, if there are any
    Return      : void
----------------------------------------------------------------------------*/
void airCheckForDistressSignals(void)
{
    udword i, numShips = aiCurrentAIPlayer->airResourceReserves.selection->numShips;
    ShipPtr ship;
    MaxSelection distressed_ships;

    distressed_ships.numShips = 0;

    for (i = 0; i < numShips; i++)
    {
        ship = aiCurrentAIPlayer->airResourceReserves.selection->ShipPtr[i];

        if (ship->gettingrocked)
        {
            selSelectionAddSingleShip(&distressed_ships, ship);
        }
    }

    if (distressed_ships.numShips)
    {
        aidSendDistressSignal((SelectCommand *)&distressed_ships);
    }
}


/*-----------------------------------------------------------------------------
    Name        : airDivideSupportTasks
    Description : Divides the support tasks among the support teams
                  Looks through the team array to find any ship with
                  CAT (0x00412d40): the advance support frigate teams are the Kushan and Taiidan ones (the race
                  and type of their first ship), paired in order with the teams needing support; HW1's per-team
                  division (num_teams / num_support_teams) is gone
    Inputs      : none
    Outputs     : Fiddles with the support team structures
    Return      : void
----------------------------------------------------------------------------*/
void airDivideSupportTasks(void)
{
    udword num_teams, num_support_teams, i;
    udword num_ASF_teams = 0;
    AITeam *teams[AIPLAYER_NUM_SUPPORTTEAMS];
    AITeam *team;
    Ship *ship;

    num_teams         = aitFindNumTeamsWithFlag(TEAM_NEEDS_SUPPORT);
    num_support_teams = aiCurrentAIPlayer->numSupportTeams;

    if ((!num_teams) || (!num_support_teams))
    {
        return;
    }

    for (i=0; i<num_support_teams;i++)
    {
        team = aiCurrentAIPlayer->supportTeam[i];
        if (team->shipList.selection->numShips)
        {
            ship = team->shipList.selection->ShipPtr[0];
            if (((ship->shiprace == RACE_Kushan) && (ship->shiptype == kAdvanceSupportFrigate)) ||
                ((ship->shiprace == RACE_Taiidan) && (ship->shiptype == tAdvanceSupportFrigate)))
            {
                teams[num_ASF_teams++] = team;
            }
        }
    }

    if (!num_ASF_teams)
    {
        return;
    }

    team = aitFindNextTeamWithFlag(NULL, TEAM_NEEDS_SUPPORT);

    i = 0;

    while (team && (i < num_ASF_teams))
    {
        aitMakeTeamSupportShips(teams[i++], team->shipList.selection);
        team = aitFindNextTeamWithFlag(team, TEAM_NEEDS_SUPPORT);
    }
}


// CAT: no airProcessSpecialTeams: cataclysm.exe has none and its only caller in aifFleetCommand was commented out in
//      HW1 already (HW1 asked for a ResearchShip here)


// the race's own resource collector: the Sect and Beast Workers, the Taiidan collector (airDumbResourcerRequests
//      0x00412e10 and airResourceManager 0x00413240 test the race the same way at each request)
static ShipType airCatCollectorType(Player *player)
{
    switch (player->race)
    {
        case RACE_Sect:     return sWorker;
        case RACE_Beast:    return bWorker;
        case RACE_Taiidan:  return tResourceCollector;
        default:            return ShipType_Uninitialized;
    }
}

/*-----------------------------------------------------------------------------
    Name        : airDumbResourcerRequests
    Description : Uses the ResourcersToBuild variable to determine if a new resourcer needs to be built
                  CAT (0x00412e10): the race's own collector (airCatCollectorType)
    Inputs      :
    Outputs     :
    Return      : Whether or not a new resourcer has been requested at high priority
----------------------------------------------------------------------------*/
bool32 airDumbResourcerRequests(void)
{
    ShipType collector;

    if (aiCurrentAIPlayer->ResourcersToBuild)
    {
        sdword totalRCollectors = aiCurrentAIPlayer->airNumRCollectors + aiCurrentAIPlayer->NumRCollectorsBeingBuilt;

        if (aiCurrentAIPlayer->ResourcersToBuild > totalRCollectors)
        {
            collector = airCatCollectorType(aiCurrentAIPlayer->player);
            if ((collector != ShipType_Uninitialized) && aiuCanBuildShipType(collector,FALSE))
            {
                aifResourceManRequestsShipsCB(collector, aiCurrentAIPlayer->ResourcersToBuild - totalRCollectors, REQUESTSHIPS_HIPRI);
                return TRUE;
            }
        }
    }
    else
    {
        aiCurrentAIPlayer->ResourcersToBuild = randyrandombetween(RANDOM_AI_PLAYER, (2 + (udword)aiCurrentAIPlayer->aiplayerDifficultyLevel), (4 + (udword)aiCurrentAIPlayer->aiplayerDifficultyLevel));
        aiplayerLog((aiIndex, "Using Dumb Resourcing - %i Resource Ships", aiCurrentAIPlayer->ResourcersToBuild));
    }
    return FALSE;
}


// one Sect mothership module for the research, unless built (airDumbResearchRequests 0x00413070): the request, the
//      flag for airResourceManager and the 100-update wait
static bool32 airCatRequestModule(ShipType module, bool32 *requestedResearch)
{
    if (cat_playerHasCompletedShip(module, aiCurrentAIPlayer->player))
    {
        return FALSE;
    }
    aifResourceManRequestsShipsCB(module, 1, REQUESTSHIPS_HIPRI);
    *requestedResearch = TRUE;
    aiCurrentAIPlayer->ResearchersToBuild = 100;
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : airDumbResearchRequests
    Description : Uses the Researchers to build variable to determine of a new
                  research ship needs to be built
                  CAT (0x00413070): Cataclysm researches in its mothership's modules: an sMothership's computer
                  player asks for its docking bay, then its micro module, then one of the special, weapons and armour
                  modules at random (stream 0, RANDOM_STREAM_0), each request followed by 100 updates of waiting (ResearchersToBuild
                  is the countdown); 5 in 101 updates it researches advanced sensors, or repair when that is not
                  possible; any other mothership only the sensors (5 in 101). Never a high-priority return
    Inputs      :
    Outputs     :
    Return      : Whether or not a new research ship has been requested at high priority
----------------------------------------------------------------------------*/
bool32 airDumbResearchRequests(bool32 *requestedResearch)
{
    Ship *mothership = aiCurrentAIPlayer->player->PlayerMothership;

    if ((mothership == NULL) || (mothership->shiptype != sMothership))
    {
        if ((ranRandom(RANDOM_STREAM_0) % 101) < 5)
        {
            cat_rmAIResearchTech(aiCurrentAIPlayer->player, TECH_ADVANCED_SENSORS);
        }
        return FALSE;
    }

    if (aiCurrentAIPlayer->ResearchersToBuild)
    {
        aiCurrentAIPlayer->ResearchersToBuild--;
        return FALSE;
    }

    if (!airCatRequestModule(sMothershipDockingBay, requestedResearch) &&
        !airCatRequestModule(sMothershipMicro, requestedResearch))
    {
        switch (ranRandom(RANDOM_STREAM_0) % 3)
        {
            case 0: airCatRequestModule(sMothershipSpecial, requestedResearch); break;
            case 1: airCatRequestModule(sMothershipWeapons, requestedResearch); break;
            case 2: airCatRequestModule(sMothershipArmour, requestedResearch);  break;
            default: break;
        }
    }

    if ((aiCurrentAIPlayer->ResearchersToBuild != 100) && ((ranRandom(RANDOM_STREAM_0) % 101) < 5) &&
        !cat_rmAIResearchTech(aiCurrentAIPlayer->player, TECH_ADVANCED_SENSORS))
    {
        cat_rmAIResearchTech(aiCurrentAIPlayer->player, TECH_REPAIR);
    }
    return FALSE;
}



// CAT: no airSmartResearchRequests: cataclysm.exe has none (airResourceManager 0x00413240 asks airDumbResearchRequests
//      only, in a multiplayer game with research on)




/*-----------------------------------------------------------------------------
    Name        : airResourceManager
    Description : Manages all the resources and non-combat ships for the computer player
                  CAT (0x00413240): research requests only in a multiplayer game with research on (the dumb ones:
                  the Sect modules); collectors of the race's own type (airCatCollectorType) in single player or
                  with harvesting on; a processor instead of HW1's resource controller (a Sect one needs the docking
                  bay); no advance support frigate requests (HW1's fuel burn); a multiplayer carrier when the cash is
                  above BUILD_CARRIER_CASH_BASE and the support units are within BUILD_CARRIER_SU_POINT of the cap
    Inputs      : orders - orders from the fleet manager
    Outputs     : sets resource collectors to collect resources and optimizes resource
                  controller and other non-combat ships
    Return      : requests for new ships
----------------------------------------------------------------------------*/
void airResourceManager(void)
{
    sdword cashGenerated;
    bool32 requestedResearch = FALSE;
    Player *player;
    Ship *mothership;
    ShipType collector;

    // Update continuously updated variables

    if (((aiCurrentAIPlayer->aiplayerFrameCount & UPDATE_RU_COUNT_RATE) == 0)||
        (aiCurrentAIPlayer->airEasilyAccesibleRUsInWorld == -1))
    {
        aiCurrentAIPlayer->airEasilyAccesibleRUsInWorld = NumberOfEasilyAccesibleRUs(aiCurrentAIPlayer->player);
    }

    cashGenerated = aiCurrentAIPlayer->player->resourceUnits + aiCurrentAIPlayer->NumRUsSpentOnAttman + aiCurrentAIPlayer->NumRUsSpentOnDefman + aiCurrentAIPlayer->NumRUsSpentOnScriptman;

    //determine if we should build any research ships
    if ((!singlePlayerGame) && bitTest(tpGameCreated.flag,MG_ResearchEnabled))
    {
        if (airDumbResearchRequests(&requestedResearch))
        {
            //a high priority request for a research ship has been entered
            return;
        }
    }

    //determine if we should build any resource collectors
    if ((singlePlayerGame || bitTest(tpGameCreated.flag,MG_HarvestinEnabled)) &&
        (aiCurrentAIPlayer->airEasilyAccesibleRUsInWorld > MIN_WORTHWHILE_RUs))
    {
        sdword totalRCollectors = aiCurrentAIPlayer->airNumRCollectors + aiCurrentAIPlayer->NumRCollectorsBeingBuilt;

        if (aiuResourceFeatureEnabled(AIR_SMART_COLLECTOR_REQUESTS))
        {
            if ((totalRCollectors < MAX_RCOLLECTORS_TO_BUILD) &&
                (totalRCollectors < aiCurrentAIPlayer->NumRUDockPoints*MAX_RCOLLECTORS_PER_DOCK_POINT))
            {
                collector = airCatCollectorType(aiCurrentAIPlayer->player);
                if (totalRCollectors == 0)
                {
                    // our first RCollector: always wanted
                    if ((collector != ShipType_Uninitialized) && aiuCanBuildShipType(collector,FALSE))
                    {
                        aifResourceManRequestsShipsCB(collector, 1, REQUESTSHIPS_HIPRI);
                        return;
                    }
                }
                else if ((aiCurrentAIPlayer->airEasilyAccesibleRUsInWorld > (totalRCollectors+1)*RUs_PER_RCOLLECTOR) &&
                         (cashGenerated > BUILD_RC_CASH_TABLE_BASE + BUILD_RC_CASH_TABLE_INC * (totalRCollectors-1)))
                {
                    // see if we have enough cash to make it worthwhile.  (The more resource collectors we
                    // already have, the more cash we want before building a new one because the Resource
                    // Manager should always be providing an increasing amount of RU's).
                    if ((collector != ShipType_Uninitialized) && aiuCanBuildShipType(collector,FALSE))
                    {
                        aifResourceManRequestsShipsCB(collector, 1, 0);
                    }
                }
            }
        }
        else
        {
            if (airDumbResourcerRequests())
            {
                //a resource collector has been requested at high priority
                return;
            }
        }

        //determine if we should build any resource controllers (CAT: a processor, 0x00413723)
        if (aiuResourceFeatureEnabled(AIR_RESOURCE_CONTROLLER_REQUESTS) &&
            (totalRCollectors >= MIN_RCOLLECTORS_FOR_CONTROLLER))
        {
            sdword totalRControllers = aiCurrentAIPlayer->airNumRControllers + aiCurrentAIPlayer->NumRControllersBeingBuilt;
            if ((totalRControllers == 0) && (!requestedResearch) &&
                (aiCurrentAIPlayer->airEasilyAccesibleRUsInWorld > (MIN_WORTHWHILE_RUs_FOR_CONTROLLER * universe.numPlayers)) &&
                (cashGenerated > BUILD_RCONTROLLER_CASH_BASE))
            {
                player = aiCurrentAIPlayer->player;
                mothership = player->PlayerMothership;
                if ((player->race == RACE_Sect) && (mothership != NULL) && (mothership->shiptype == sMothership))
                {
                    if (aiuCanBuildShipType(sProcessor,FALSE) && cat_playerHasCompletedShip(sMothershipDockingBay, player))
                    {
                        aifResourceManRequestsShipsCB(sProcessor, 1, 0);
                        return;
                    }
                }
                else if ((player->race == RACE_Beast) && (mothership != NULL) && (mothership->shiptype == bMothership))
                {
                    if (aiuCanBuildShipType(bProcessor,FALSE))
                    {
                        aifResourceManRequestsShipsCB(bProcessor, 1, 0);
                        return;
                    }
                }
            }
        }
    }

    // CAT: a carrier for a multiplayer computer player near its support unit cap (0x00413929; the exe reaches the same
    //      test on every path, the support-unit compare unsigned)
    player = aiCurrentAIPlayer->player;
    if ((!singlePlayerGame) && (cashGenerated > BUILD_CARRIER_CASH_BASE) &&
        !((udword)player->supportUnitsUsed < (udword)(player->supportUnitsMax - BUILD_CARRIER_SU_POINT)))
    {
        mothership = player->PlayerMothership;
        if ((player->race == RACE_Sect) && (mothership != NULL) && (mothership->shiptype == sMothership))
        {
            if (aiuCanBuildShipType(sCarrier,FALSE) && cat_playerHasCompletedShip(sMothershipDockingBay, player) &&
                aiuUnitCapCanBuildShip(aiCurrentAIPlayer, sCarrier, 1))
            {
                aifResourceManRequestsShipsCB(sCarrier, 1, 0);
            }
        }
        else if ((player->race == RACE_Beast) && (mothership != NULL) && (mothership->shiptype == bMothership))
        {
            if (aiuCanBuildShipType(bCarrier,FALSE) && aiuUnitCapCanBuildShip(aiCurrentAIPlayer, bCarrier, 1))
            {
                aifResourceManRequestsShipsCB(bCarrier, 1, 0);
            }
        }
    }

    if (aiCurrentAIPlayer->numSupportTeams)
    {
        airDivideSupportTasks();
    }

    //Distress signals only work with active guard,
    //so that feature has to be activated as well
    if (aiuResourceFeatureEnabled(AIR_RESOURCE_DISTRESS_SIGNALS))
    {
        airCheckForDistressSignals();
    }
}

