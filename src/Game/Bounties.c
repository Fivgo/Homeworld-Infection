// =============================================================================
//  Bounties.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created oct 6th, bpasechn
// =============================================================================

#include "Bounties.h"

#include "MultiplayerGame.h"
#include "Tweak.h"



//variables of import: tpGameCreated.flag, MG_BountiesEnabled bitmask
//                     tpGameCreated.bountySize

/*-----------------------------------------------------------------------------
    Name        : getPlayerBounty
    Description : returns the 'worth' of a players ships...
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/

// CAT (M2-R4 rest-oracle): Cataclysm keeps a per-player worth (8 floats at 0x008dfa28) and the raw bounty setting
//      (0x008dfa48) instead of HW1's bounty percentage per player; names chosen here
static real32 cat_playerBountyWorth[MAX_MULTIPLAYER_PLAYERS];
static sdword cat_bountySetting;

// 0x00428050: clears the worth array and stores the setting (HW1 stored TW_BountySizes[size] in universe.bountySize)
void BountyInit(sdword bountySettingSize)
{
    sdword i;

    for (i = 0; i < MAX_MULTIPLAYER_PLAYERS; i++)
    {
        cat_playerBountyWorth[i] = 0.0f;
    }
    cat_bountySetting = bountySettingSize;
}

// 0x00428250: the player's worth as an integer (a 16-byte __ftol wrapper; HW1 returned player->bounty). The read of
// the worth array is the reading of the evidence (callers cat_moPlayerNamesDraw, cat_aisUpdateTeams), confidence medium
sdword getPlayerBountyRender(Player *player)
{
    return((sdword)cat_playerBountyWorth[player->playerIndex]);
}

//called ONLY deterministically...
// CAT: 0x00428070: per player, the build cost of every live, enabled ship plus the resource units (the ships inside
//      carriers, the human/computer RU averaging and the bounty percentage of HW1 are gone); a worth of 0 or less reads 1
void calculatePlayerBounties(void)
{
    real32 shipworth[MAX_MULTIPLAYER_PLAYERS];
    real32 worth;
    sdword i;
    Node *objnode;
    Ship *ship;
    udword playerindex;

    if (singlePlayerGame)
    {
        return;
    }

    for (i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
    {
        shipworth[i] = 0.0f;
    }

    for (objnode = universe.ShipList.head; objnode != NULL; objnode = objnode->next)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        if (((ship->header.flags & SOF_Disabled) == 0) && ((ship->header.flags & SOF_Dead) == 0))
        {
            playerindex = ship->playerowner->playerIndex;
            if (playerindex < universe.numPlayers)
            {
                shipworth[playerindex] += (real32)ship->staticinfo->buildCost;
            }
        }
    }

    for (i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
    {
        worth = (real32)universe.players[i].resourceUnits + shipworth[i];
        cat_playerBountyWorth[i] = (worth <= 0.0f) ? 1.0f : worth;
    }
}

// CAT: no getPlayerBountyWorthDeterm (its only caller, HW1's calculatePlayerBounties, is gone)

/*-----------------------------------------------------------------------------
    Name        : getBountyOnShip
    Description : returns the RU worth of this ship.
                  value is based on the getPlayerBounty function
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
sdword getBountyOnShip(Ship *ship)
{
    //return buildcost times bounty multiplier.
    return((sdword)(ship->staticinfo->buildCost*((((real32)ship->playerowner->bounty)/100.0f))));
}

/*-----------------------------------------------------------------------------
    Name        : bountyShipWasKilled
    Description : deals with a ship that died in the case of bounties
                  This function gets called when a ship is KILLED by something
                  (via apply damage to target)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void bountyShipWasKilled(Ship *ship)
{
    if((!singlePlayerGame) && (tpGameCreated.bountySize != MG_BountiesOff))
    {
        sdword bounty;
        //bounties enabled!!!
        switch(ship->howDidIDie)
        {
        case DEATH_Killed_By_Player:
        case DEATH_Killed_By_Player_Explosion:
        case DEATH_Killed_By_Kamikaze:
#ifdef HW_BUILD_FOR_DEBUGGING
            dbgAssertOrIgnore(ship->whoKilledMe != 99);
#endif
            //reimburse player for their troubles
            if(ship->playerowner->playerIndex != ship->whoKilledMe)
            {
                //make sure it was NOT the ship owner who killed the ship
                bounty = getBountyOnShip(ship);
                universe.players[ship->whoKilledMe].resourceUnits += bounty;
                //update statistic
                universe.gameStats.playerStats[ship->whoKilledMe].totalResourceUnitsViaBounties += bounty;
                universe.gameStats.playerStats[ship->whoKilledMe].totalResourceUnits += bounty;
            }
            break;
        }
    }
}

