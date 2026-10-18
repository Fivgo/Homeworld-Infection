// =============================================================================
//  AIHandler.h
//  - Contains the event handler functions for the computer player
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/19/1998 by fpoiker
// =============================================================================

#include "AIHandler.h"

#include "AIFeatures.h"
#include "AIMoves.h"
#include "AIOrders.h"
#include "AIPlayer.h"
#include "GravWellGenerator.h"
#include "Randy.h"
#include "Select.h"
#include "UnivUpdate.h"


// CAT: the exe's table (0x0074a3a4, 20 entries and NULL; aieHandlerToNum 0x00405ce0): no fuel handlers, so the save
//      numbers start at aihHarassNumbersLowHandler. The exe's linker folded aihReconShipTeamDiedHandler into
//      aihReconaissanceTeamDiedHandler (entries 15 and 16 are both 0x00406e10) and aihRemoveTeamDiedHandler into
//      aihPatrolTeamDiedHandler (18 and 19: 0x00406e80): the bodies are the same, the indices are kept
aieHandlerSimple handlerTable[] =
{
    aihHarassNumbersLowHandler,
    aihHarassFiringSingleShipHandler,
    aihHarassDisengageSingleShipHandler,
    aihKamikazeHealthLowHandler,
    aihFastDefenseNumbersLowHandler,
    aihSlowDefenseNumbersLowHandler,
    (aieHandlerSimple)aihGenericGettingRockedHandler,
    (aieHandlerSimple)aihPatrolEnemyNearbyHandler,
    (aieHandlerSimple)aihGravWellEnemyNearbyHandler,
    aihGravWellEnemyNotNearbyHandler,
    (aieHandlerSimple)aihFastDefenseDistressHandler,
    (aieHandlerSimple)aihSlowDefenseDistressHandler,
    aihFastDefenseTeamDiedHandler,
    aihSlowDefenseTeamDiedHandler,
    aihGuardShipsTeamDiedHandler,
    aihReconaissanceTeamDiedHandler,
    aihReconShipTeamDiedHandler,
    aihHarassTeamDiedHandler,
    aihPatrolTeamDiedHandler,
    aihRemoveTeamDiedHandler,
    NULL
};

sdword aieHandlerToNum(aieHandlerSimple handler)
{
    if (handler == NULL)
    {
        return -1;
    }
    else
    {
        sdword i = 0;
        while (handlerTable[i] != NULL)
        {
            if (handlerTable[i] == handler)
            {
                return i;
            }

            i++;
        }

        dbgFatalf(DBG_Loc,"Save Game: Unknown Handler");
        return -1;
    }
}

aieHandlerSimple aieNumToHandler(sdword num)
{
    if (num == -1)
    {
        return NULL;
    }
    else
    {
        dbgAssertOrIgnore(num < sizeof(handlerTable)/sizeof(handlerTable[0]));
        return handlerTable[num];
    }
}

// CAT: no aihGenericEmptyFuelHandler, aihSwarmerEmptyFuelHandler or aihGenericFuelLowHandler (no fuel in Cataclysm;
//      the exe's handler table has none of them)


/*-----------------------------------------------------------------------------
    Name        : aihHarassNumbersLowHandler
    Description : Handles the harass team being decimated
    Inputs      : team - the team being decimated
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aihHarassNumbersLowHandler(AITeam *team)
{
    AITeamMove *thisMove = team->curMove, *newMove;
    AlternativeShips alternatives;
    vector gathering_point;
    vector origin = ORIGIN_VECTOR;

//commented code is the reinforce type numbers low handler
/*    udword i;

    for (i = 0; i < aiCurrentAIPlayer->numGuardTeams; i++)
    {
        //later need to actually remove team from guardTeams array
        if (bitTest(aiCurrentAIPlayer->guardTeams[i]->teamFlags, FAST_ROVING_GUARD) &&
            (aiCurrentAIPlayer->guardTeams[i]->shipList.selection->numShips))
        {
            bitClear(aiCurrentAIPlayer->guardTeams[i]->teamFlags, FAST_ROVING_GUARD);
            aiCurrentAIPlayer->guardTeams[i]->curMove = aimCreateReinforce(aiCurrentAIPlayer->guardTeams[i], team, TRUE, FALSE);
            aimCreateMoveDone(aiCurrentAIPlayer->guardTeams[i], FALSE, FALSE);
            return;
        }
    }
*/
    //later maybe change to "reinforce" move, maybe along with harass team getting out
    //of there to attack someone else.
    //later - very important to get the same ships that were requested in the first place
    //        this isn't all that "generic"
    if (aiCurrentAIPlayer->player->PlayerMothership)
    {
        gathering_point = aiCurrentAIPlayer->player->PlayerMothership->obj.posinfo.position;
    }
    else
    {
        vecZeroVector(gathering_point);
    }

    while ((thisMove->type == MOVE_ATTACK) || (thisMove->type == MOVE_ADVANCEDATTACK))
    {
        if (thisMove->remove)
        {
            aitDeleteCurrentMove(team);
        }
        else
        {
            team->curMove = (AITeamMove *)listGetStructOfNode(team->curMove->listNode.next);
        }

        thisMove = team->curMove;

        dbgAssertOrIgnore(thisMove);
    }

    gathering_point = aiuFindRangeStandoffPoint(gathering_point,origin, AIH_HARASS_NUMLOW_STANDOFF_DIST);

    thisMove->processing = FALSE;
    thisMove->events.numbersLow.triggered = FALSE;

    //add a move, getships, formation
    newMove = aimCreateMoveTeamNoAdd(team, gathering_point, AIH_HARASS_NUMLOW_FORMATION, FALSE, TRUE);
//band-aid solution... change moveteam move so that tactics are there...
    newMove->tactics = Evasive;
    aitAddmoveBeforeAndMakeCurrent(team, newMove, thisMove);

    // CAT: Cataclysm's replacements by race (aihHarassNumbersLowHandler 0x00405d40): a Sect team asks for 16 sAcolytes
    //      or sACVs / sMimics, a Beast team for 16 bHeavyCorvettes or bInterceptors, bAttackBombers, bAcolytes and bACVs
    //      (the last two once researched); no request for any other race (CAT-FIX: the exe then re-adds the move-team
    //      node it has just inserted)
    if (aiCurrentAIPlayer->player->race == RACE_Sect)
    {
        SetNumAlternatives(alternatives,2);
        SetAlternative(alternatives,0,sACV,14);
        SetAlternative(alternatives,1,sMimic,14);

        newMove = aimCreateFancyGetShipsNoAdd(team, sAcolyte, 16, &alternatives, REQUESTSHIPS_HIPRI, TRUE, TRUE);
        listAddNodeBefore(&(thisMove->listNode), &(newMove->listNode), newMove);
    }
    else if (aiCurrentAIPlayer->player->race == RACE_Beast)
    {
        SetNumAlternatives(alternatives,4);
        SetAlternative(alternatives,0,bInterceptor,14);
        SetAlternative(alternatives,1,bAttackBomber,14);
        SetAlternative(alternatives,2,aioCatEnabledOr(bAcolyte, bInterceptor),14);
        SetAlternative(alternatives,3,aioCatEnabledOr(bACV, bAttackBomber),14);

        newMove = aimCreateFancyGetShipsNoAdd(team, bHeavyCorvette, 16, &alternatives, REQUESTSHIPS_HIPRI, TRUE, TRUE);
        listAddNodeBefore(&(thisMove->listNode), &(newMove->listNode), newMove);
    }

    bitClear(team->teamFlags, TEAM_NEEDS_SUPPORT);
}


/*-----------------------------------------------------------------------------
    Name        : aihHarassFiringSingleShipHandler
    Description : Puts the harass team into a sphere formation around the ship it's attacking
    Inputs      : team - the team attacking
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aihHarassFiringSingleShipHandler(AITeam *team)
{
    aiuWrapFormation(team->shipList.selection,AIH_HARASS_SINGLEATTACK_FORMATION);
    aieHandlerSetDisengage(team->curMove, TRUE, aihHarassDisengageSingleShipHandler);
}


/*-----------------------------------------------------------------------------
    Name        : aihHarassDisengageSingleShipHandler
    Description : Puts the harass team back into the default formation
    Inputs      : team - the team disengaging
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aihHarassDisengageSingleShipHandler(AITeam *team)
{
    aiuWrapFormation(team->shipList.selection, team->curMove->formation);
    team->curMove->events.firing.triggered = FALSE;
}



/*-----------------------------------------------------------------------------
    Name        : aihKamikazeHealthLowHandler
    Description : Finds the ship who's health is low and kamikaze's it into the enemy
    Inputs      : team - the team with the suicide bombers
    Outputs     : Forces pilots to kill themselves in the name of greater good
    Return      : void
----------------------------------------------------------------------------*/
void aihKamikazeHealthLowHandler(AITeam *team)
{
    MaxSelection shipsToDie;
    sdword i;
    ShipPtr ship;

    shipsToDie.numShips = 0;

    for (i=0;i<team->shipList.selection->numShips;i++)
    {
        ship = team->shipList.selection->ShipPtr[i];

        if ((isShipOfClass(ship, CLASS_Fighter) || isShipOfClass(ship, CLASS_Corvette)) &&
            ((100 * ship->targetable.health) <
             (team->curMove->events.healthLow.watchPercentage * ship->staticinfo->health.maxhealth)))
        {
            selSelectionAddSingleShip(&shipsToDie, ship);
        }
    }

    // CAT: no kamikaze order when no ship qualifies (aihKamikazeHealthLowHandler 0x00405fd0 tests numShips > 0)
    if (shipsToDie.numShips > 0)
    {
        aiuWrapSetKamikaze((SelectCommand *)&shipsToDie);
    }
}


/*-----------------------------------------------------------------------------
    Name        : aihFastDefenseNumbersLowHandler
    Description : Handles the fast roving defense team being decimated
    Inputs      : team - the team being decimated
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aihFastDefenseNumbersLowHandler(AITeam *team)
{
    AITeamMove *thisMove = team->curMove, *newMove;
    AlternativeShips alternatives;
    vector gathering_point;
    vector origin = ORIGIN_VECTOR;
    SelectCommand *teamsel = team->shipList.selection;
    sdword original_number = thisMove->events.numbersLow.watchBaseCount;

    //later maybe change to "reinforce" move, maybe along with harass team getting out
    //of there to attack someone else.
    if (aiCurrentAIPlayer->player->PlayerMothership)
    {
        gathering_point = aiCurrentAIPlayer->player->PlayerMothership->obj.posinfo.position;
    }
    else
    {
        vecZeroVector(gathering_point);
    }

    gathering_point = aiuFindRangeStandoffPoint(gathering_point,origin, AIH_FASTDEF_NUMLOW_STANDOFF_DIST);

    thisMove->processing = FALSE;
    thisMove->events.numbersLow.triggered = FALSE;

    //add a move, getships, formation
    newMove = aimCreateMoveTeamNoAdd(team, gathering_point, AIH_FASTDEF_NUMLOW_FORMATION, FALSE, TRUE);
    aitAddmoveBeforeAndMakeCurrent(team, newMove, thisMove);

    if (teamsel->numShips)
    {
        if (teamsel->numShips < original_number)
        {
            newMove = aimCreateGetShipsNoAdd(team, teamsel->ShipPtr[0]->shiptype, (sbyte)(original_number - teamsel->numShips), 0, TRUE, TRUE);
        }
        else
        {
            //just add one ship (to prevent infinite loops)
            newMove = aimCreateGetShipsNoAdd(team, teamsel->ShipPtr[0]->shiptype, 1, 0, TRUE, TRUE);
        }

    }
    else
    {
        // CAT: Cataclysm's replacement team by race (aihFastDefenseNumbersLowHandler 0x00406070): Sect 12 sAcolytes or
        //      sRecons / sACVs / sMimics, Beast 12 bInterceptors or bHeavyCorvettes / bRecons / bAttackBombers / bACVs
        //      (once researched); no request for any other race (CAT-FIX: the exe re-adds the move-team node)
        if (aiCurrentAIPlayer->player->race == RACE_Sect)
        {
            SetNumAlternativesFlags(alternatives,3,ALTERNATIVE_RANDOM);
            SetAlternative(alternatives,0,sRecon,20);
            SetAlternative(alternatives,1,sACV,14);
            SetAlternative(alternatives,2,sMimic,15);

            newMove = aimCreateFancyGetShipsNoAdd(team, sAcolyte, 12, &alternatives, 0, TRUE, FALSE);
        }
        else if (aiCurrentAIPlayer->player->race == RACE_Beast)
        {
            SetNumAlternativesFlags(alternatives,4,ALTERNATIVE_RANDOM);
            SetAlternative(alternatives,0,bHeavyCorvette,14);
            SetAlternative(alternatives,1,bRecon,20);
            SetAlternative(alternatives,2,bAttackBomber,15);
            SetAlternative(alternatives,3,aioCatEnabledOr(bACV, bAttackBomber),14);

            newMove = aimCreateFancyGetShipsNoAdd(team, bInterceptor, 12, &alternatives, 0, TRUE, FALSE);
        }
        else
        {
            thisMove->events.numbersLow.watchBaseCount = 0;
            thisMove->processing = FALSE;
            return;
        }
    }

    thisMove->events.numbersLow.watchBaseCount = 0;
    thisMove->processing = FALSE;

    listAddNodeBefore(&(thisMove->listNode), &(newMove->listNode), newMove);

//    newMove = aimCreateFormationNoAdd(team, WALL_FORMATION, FALSE, TRUE);
//    listAddNodeBefore(&(thisMove->listNode), &(newMove->listNode), newMove);
}


/*-----------------------------------------------------------------------------
    Name        : aihSlowDefenseNumbersLowHandler
    Description : Handles the slow roving defense team being decimated
    Inputs      : team - the team being decimated
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aihSlowDefenseNumbersLowHandler(AITeam *team)
{
    AITeamMove *thisMove = team->curMove, *newMove;
    AlternativeShips alternatives;
    vector gathering_point;
    vector origin = ORIGIN_VECTOR;
    SelectCommand *teamsel = team->shipList.selection;
    sdword original_number = thisMove->events.numbersLow.watchBaseCount;

    //later maybe change to "reinforce" move, maybe along with harass team getting out
    //of there to attack someone else.
    if (aiCurrentAIPlayer->player->PlayerMothership)
    {
        gathering_point = aiCurrentAIPlayer->player->PlayerMothership->obj.posinfo.position;
    }
    else
    {
        vecZeroVector(gathering_point);
    }

    gathering_point = aiuFindRangeStandoffPoint(gathering_point,origin, AIH_SLOWDEF_NUMLOW_STANDOFF_DIST);

    thisMove->processing = FALSE;
    thisMove->events.numbersLow.triggered = FALSE;

    //add a move, getships, formation
    newMove = aimCreateMoveTeamNoAdd(team, gathering_point,AIH_SLOWDEF_NUMLOW_FORMATION, FALSE, TRUE);
    aitAddmoveBeforeAndMakeCurrent(team, newMove, thisMove);

    if (teamsel->numShips)
    {
        if (teamsel->numShips < original_number)
        {
            newMove = aimCreateGetShipsNoAdd(team, teamsel->ShipPtr[0]->shiptype, (sbyte)(original_number - teamsel->numShips), 0, TRUE, TRUE);
        }
        else
        {
            newMove = aimCreateGetShipsNoAdd(team, teamsel->ShipPtr[0]->shiptype, 1, 0, TRUE, TRUE);
        }
    }
    else
    {
        // CAT: Cataclysm's replacement team by race (aihSlowDefenseNumbersLowHandler 0x004062d0): Sect 3 sMCVs or
        //      sACVs / sMultiBeamFrigates / sHiveFrigates, Beast 3 bMultiGunCorvettes or bHeavyCorvettes /
        //      bMissileCorvettes / bIonArrayFrigates / bAttackBombers and, once researched, bACVs, bMultiBeamFrigates,
        //      bHiveFrigates; no request for any other race (CAT-FIX: the exe re-adds the move-team node)
        if (aiCurrentAIPlayer->player->race == RACE_Sect)
        {
            SetNumAlternativesFlags(alternatives,4,ALTERNATIVE_RANDOM);
            SetAlternative(alternatives,0,sACV,12);
            SetAlternative(alternatives,1,sMCV,7);
            SetAlternative(alternatives,2,sMultiBeamFrigate,30);
            SetAlternative(alternatives,3,sHiveFrigate,30);

            newMove = aimCreateFancyGetShipsNoAdd(team, sMCV, 3, &alternatives, 0, TRUE, FALSE);
        }
        else if (aiCurrentAIPlayer->player->race == RACE_Beast)
        {
            SetNumAlternativesFlags(alternatives,7,ALTERNATIVE_RANDOM);
            SetAlternative(alternatives,0,bHeavyCorvette,12);
            SetAlternative(alternatives,1,bMissileCorvette,7);
            SetAlternative(alternatives,2,bIonArrayFrigate,30);
            SetAlternative(alternatives,3,bAttackBomber,30);
            SetAlternative(alternatives,4,aioCatEnabledOr(bACV, bAttackBomber),12);
            SetAlternative(alternatives,5,aioCatEnabledOr(bMultiBeamFrigate, bIonArrayFrigate),30);
            SetAlternative(alternatives,6,aioCatEnabledOr(bHiveFrigate, bMissileCorvette),30);

            newMove = aimCreateFancyGetShipsNoAdd(team, bMultiGunCorvette, 3, &alternatives, 0, TRUE, FALSE);
        }
        else
        {
            thisMove->events.numbersLow.watchBaseCount = 0;
            thisMove->processing = FALSE;
            return;
        }
    }

    thisMove->events.numbersLow.watchBaseCount = 0;
    thisMove->processing = FALSE;

    listAddNodeBefore(&(thisMove->listNode), &(newMove->listNode), newMove);
}




/*-----------------------------------------------------------------------------
    Name        : aihGenericGettingRockedHandler
    Description : A generic event handler for the "getting rocked" event
    Inputs      : team - the team that's getting rocked,
                  ships - the ships that are attacking the team
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aihGenericGettingRockedHandler(AITeam *team, SelectCommand *ships)
{
    AITeamMove *thisMove = team->curMove;
    AITeamMove *newMove;
    SelectCommand *attacktargets;

    dbgAssertOrIgnore(ships->numShips > 0);

    if (thisMove->type == MOVE_ATTACK)
    {
        dbgAssertOrIgnore(team->shipList.selection->numShips > 0);
        if (thisMove->params.attack.ships->numShips > 0)
        {
            if (team->shipList.selection->ShipPtr[0]->obj.collMyBlob == thisMove->params.attack.ships->ShipPtr[0]->obj.collMyBlob)
            {
                // we're very close to target, so soldier on without worrying about other attacking ships
                return;
            }
        }
    }

    thisMove->events.gettingRocked.triggered = FALSE;

    attacktargets = getShipAndItsFormation(&universe.mainCommandLayer,ships->ShipPtr[0]);

// later add:
//    if (TargetsAreJustPesky(team->shipList.selection,attacktargets))
//    {
//        memFree(attacktargets);
//        return;
//    }

    if (aiuSelectionNotGoodAtKillingTheseTargets(team->shipList.selection,attacktargets,AIH_GENERIC_GETTINGROCKED_STRENGTH))
    {
        memFree(attacktargets);
        return;
    }

    thisMove->processing = FALSE;

    //add a attack move before
    newMove = aimCreateAttackNoAdd(team, attacktargets,AIH_GENERIC_GETTINGROCKED_FORMATION, TRUE, TRUE);
    newMove->events = thisMove->events;
    aitAddmoveBeforeAndMakeCurrent(team, newMove, thisMove);
}


/*-----------------------------------------------------------------------------
    Name        : aihPatrolEnemyNearbyHandler
    Description : Attacks enemies nearby
    Inputs      : team - the team who the event belongs to, ships - the ships nearby
    Outputs     : Creates new moves
    Return      : void
----------------------------------------------------------------------------*/
void aihPatrolEnemyNearbyHandler(AITeam *team, SelectCommand *ships)
{
    AITeamMove *newMove, *thisMove = team->curMove;

    dbgAssertOrIgnore(ships->numShips > 0);

    if (aiuSelectionNotGoodAtKillingTheseTargets(team->shipList.selection,ships,AIH_PATROL_ENEMYNEARBY_STRENGTH))
    {
        return;
    }

    newMove = aimCreateAttackNoAdd(team, selectMemDupSelection(ships, "duppenh", 0), AIH_PATROL_ENEMYNEARBY_FORMATION, TRUE, TRUE);
    newMove->events = thisMove->events;
    newMove->events.enemyNearby.handler = NULL;
    team->curMove->processing = FALSE;
    aitAddmoveBeforeAndMakeCurrent(team, newMove, thisMove);
}



/*-----------------------------------------------------------------------------
    Name        : aihGravWellEnemyNearbyHandler
    Description : Turns on the grav well generator if fighters are nearby
    Inputs      : team - the grav well generator team, ships - the ships near the gravwell generator
    Outputs     : Turns on the special ability of the gravwell generator
    Return      : void
----------------------------------------------------------------------------*/
void aihGravWellEnemyNearbyHandler(AITeam *team, SelectCommand *ships)
{
    AITeamMove *thisMove = team->curMove, *newMove;
    udword numEnemyFighters = 0, numEnemyCorvettes = 0;
    sdword i;
//    ShipPtr ship;

    dbgAssertOrIgnore(ships->numShips > 0);

    for (i=0;i<ships->numShips;i++)
    {
        if (isShipOfClass(ships->ShipPtr[i],CLASS_Fighter))
        {
            ++numEnemyFighters;
        }
        else if (isShipOfClass(ships->ShipPtr[i], CLASS_Corvette))
        {
            ++numEnemyCorvettes;
        }

        //if there are enough strikecraft
        if ((numEnemyFighters > 2) || numEnemyCorvettes)
        {
            //if the event hasn't happened yet
            if (!thisMove->events.enemyNotNearby.handler)
            {
                //turn on the gravwell
                aiuWrapSpecial(team->shipList.selection, NULL);
                aieHandlerSetEnemyNotNearby(thisMove, ((GravWellGeneratorStatics *) ((ShipStaticInfo *)(team->shipList.selection->ShipPtr[0]->staticinfo))->custstatinfo)->GravWellRadius,
                                            TRUE, aihGravWellEnemyNotNearbyHandler);
                //set to cooperating team to destroy the trapped ships
                if (team->cooperatingTeam)
                {
                    //later maybe decide if the current move is more important or not
                    newMove = aimCreateAdvancedAttackNoAdd(team->cooperatingTeam, selectMemDupSelection(ships, "gwenh", 0), SAME_FORMATION, Aggressive, TRUE, TRUE);    // CAT: Aggressive (0x0040677c)
                    aimInsertMove(team->cooperatingTeam, newMove);
                }

            }
            //if the event has happened, new ships may have been captured,
            //add those to the advanced attack move
            else
            {
                if (team->cooperatingTeam)
                {
                    //if the other team has an advanced attack move
                    if (team->cooperatingTeam->curMove->type == MOVE_ADVANCEDATTACK)
                    {
                        if (!SelectionsAreEquivalent(ships, team->cooperatingTeam->curMove->params.advatt.targets))
                        {
                            team->cooperatingTeam->curMove->params.advatt.targets = selectMergeTwoSelections(ships, team->cooperatingTeam->curMove->params.advatt.targets, DEALLOC2);
                        }
                    }
                    else
                    {
                        newMove = aimCreateAdvancedAttackNoAdd(team->cooperatingTeam, selectMemDupSelection(ships, "gwenh", 0), SAME_FORMATION, Aggressive, TRUE, TRUE);    // CAT: Aggressive (0x004067f6)
                        aimInsertMove(team, newMove);
                    }
                }
            }
            break;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : aihGravWellEnemyNotNearbyHandler
    Description : Turns off the grav well generator no more enemy ships are nearby
    Inputs      : team - the grav well generator team
    Outputs     : Turns off the special ability of the gravwell generator
    Return      : void
----------------------------------------------------------------------------*/
void aihGravWellEnemyNotNearbyHandler(AITeam *team)
{
    AITeamMove *thisMove = team->curMove;

    aiuWrapSpecial(team->shipList.selection, NULL);
    thisMove->events.enemyNotNearby.handler = NULL;
    aieHandlerSetEnemyNearby(thisMove,
                             (((GravWellGeneratorStatics *) ((ShipStaticInfo *)(team->shipList.selection->ShipPtr[0]->staticinfo))->custstatinfo)->GravWellRadius)*0.8,
                             FALSE, aihGravWellEnemyNearbyHandler);
    //later cancel attack move on cooperating team???
}


// CAT: the rescue support type of a distress handler, by the race of the team's first ship (inlined in
//      aihFastDefenseDistressHandler 0x00406880, aihSlowDefenseDistressHandler 0x00406af0 and aimProcessTempGuard
//      0x00409c50); -1 (no rescue) for any
//      other race, which the exe skips without calling aiuRescueShipType
ShipType cat_aihRescueSupportType(AITeam *team)
{
    switch (team->shipList.selection->ShipPtr[0]->shiprace)
    {
        case RACE_Kushan:   return kAdvanceSupportFrigate;
        case RACE_Taiidan:  return tAdvanceSupportFrigate;
        case RACE_Sect:     return sProcessor;
        case RACE_Beast:    return bProcessor;
        default:            return ShipType_Uninitialized;
    }
}


/*-----------------------------------------------------------------------------
    Name        : aihFastDefenseDistressHandler
    Description : Handles distress calls for fast defense teams
    Inputs      : team - the team handling the distress call,
                  intvar - the variable that is being watched,
                  in this case the aiplayer->aidDefenseTargets
    Outputs     : Creates new moves to deal with the poopyhead attacking the defensless ship
    Return      : void
----------------------------------------------------------------------------*/
void aihFastDefenseDistressHandler(AITeam *team, udword *intvar)
{
    AITeamMove *newMove, *thisMove = team->curMove;
    SelectCommand *enemyShips;
    MaxSelection invadingShips, distressShips;
    sdword i;
    TypeOfFormation formation;

    if (aiCurrentAIPlayer->aidDistressShips)
    {
        selSelectionCopy((MaxAnySelection *)&distressShips, (MaxAnySelection *)aiCurrentAIPlayer->aidDistressShips);

        // CAT: the support ship that rescues by the race of the team's first ship (0x004068bd / 0x00406b39): Kushan
        //      kAdvanceSupportFrigate, Taiidan tAdvanceSupportFrigate, Sect sProcessor, Beast bProcessor, none else
        if (aiuRescueShipType((SelectCommand *)&distressShips, team, cat_aihRescueSupportType(team)))
        {
            return;
        }
    }

    if (aiCurrentAIPlayer->aidInvadingShips)
    {
        selSelectionCopy((MaxAnySelection *)&invadingShips, (MaxAnySelection *)aiCurrentAIPlayer->aidInvadingShips);

//    if (aihTakeoutSmallCombatInvaders(invaderShips, team))
//        return;
//    if (aihTakeoutBigCombatInvaders(invaderShips, team))
//        return;

        //later choose between ships on the distress ships list
        //closest/most important/etc.
        for (i = 0; i < invadingShips.numShips; i++)
        {
            enemyShips = aiuFindNearbyDangerousEnemyShips(invadingShips.ShipPtr[i], AIH_FASTDEF_INVADER_ENEMYDIST);
            selSelectionAddSingleShip((MaxSelection *)enemyShips, invadingShips.ShipPtr[i]);

            if (aitCheckIfOtherDefTeamAnsweringSignalNeedsHelp(team, enemyShips))
            {
                if (enemyShips->numShips > AIH_FASTDEF_INVADER_LOTSFEW_LIMIT)
                {
                    formation = AIH_FASTDEF_INVADER_LOTS_FORMATION;
                }
                else
                {
                    formation = AIH_FASTDEF_INVADER_FEW_FORMATION;
                }

                newMove         = aimCreateAdvancedAttackNoAdd(team, selectMemDupSelection(enemyShips, "fddh", 0), formation, aioCatAttackTactics(team), TRUE, TRUE);   // CAT: 0x00406a00
                newMove->events = thisMove->events;
                if (aiuAttackFeatureEnabled(AIA_KAMIKAZE))
                {
                    aieHandlerSetHealthLow(newMove, AIO_FIGHTER_KAMIKAZE_HEALTH, TRUE, FALSE, aihKamikazeHealthLowHandler);
                }
                newMove->events.interrupt.handler  = NULL;
                newMove->events.numbersLow.handler = NULL;
                team->curMove->processing = FALSE;
                aitAddmoveBeforeAndMakeCurrent(team, newMove, thisMove);
                memFree(enemyShips);
                return;
            }
            memFree(enemyShips);
        }
    }

    if ((aiCurrentAIPlayer->aidDistressShips) && (distressShips.numShips))
    {
        // CAT: the resourcer to rescue by the AI player's race (0x004069b2): Taiidan tResourceCollector, Sect sWorker,
        //      Beast bWorker, none else
        switch (aiCurrentAIPlayer->player->race)
        {
            case RACE_Taiidan:  aiuRescueShipType((SelectCommand *)&distressShips, team, tResourceCollector); break;
            case RACE_Sect:     aiuRescueShipType((SelectCommand *)&distressShips, team, sWorker);            break;
            case RACE_Beast:    aiuRescueShipType((SelectCommand *)&distressShips, team, bWorker);            break;
            default:            break;
        }
    }
}


/*-----------------------------------------------------------------------------
    Name        : aihSlowDefenseDistressHandler
    Description : Handles distress calls for slow defense teams
    Inputs      : team - the team handling the distress call,
                  intvar - the variable that is being watched,
                  in this case the aiplayer->aidDefenseTargets
    Outputs     : Creates new moves to deal with the poopyhead attacking the defensless ship
    Return      : void
----------------------------------------------------------------------------*/
void aihSlowDefenseDistressHandler(AITeam *team, udword *intvar)
{
    AITeamMove *newMove, *thisMove = team->curMove;
    SelectCommand *enemyShips;
    MaxSelection distressShips, invadingShips;
    sdword i;
    real32 distsq_to_ships;
    TypeOfFormation formation;

    if (!team->shipList.selection->numShips)
    {
        // just in case
        return;
    }

    if (aiCurrentAIPlayer->aidDistressShips)
    {
        selSelectionCopy((MaxAnySelection *)&distressShips, (MaxAnySelection *)aiCurrentAIPlayer->aidDistressShips);

        // CAT: the support ship that rescues by the race of the team's first ship (0x004068bd / 0x00406b39): Kushan
        //      kAdvanceSupportFrigate, Taiidan tAdvanceSupportFrigate, Sect sProcessor, Beast bProcessor, none else
        if (aiuRescueShipType((SelectCommand *)&distressShips, team, cat_aihRescueSupportType(team)))
        {
            return;
        }
    }

    if (aiCurrentAIPlayer->aidInvadingShips)
    {
        selSelectionCopy((MaxAnySelection *)&invadingShips, (MaxAnySelection *)aiCurrentAIPlayer->aidInvadingShips);

        //later choose between ships on the distress ships list
        //closest/most important/etc.
        for (i = 0; i < invadingShips.numShips; i++)
        {
            enemyShips = aiuFindNearbyDangerousEnemyShips(invadingShips.ShipPtr[i], AIH_SLOWDEF_INVADER_ENEMYDIST);
            selSelectionAddSingleShip((MaxSelection *)enemyShips, invadingShips.ShipPtr[i]);
            distsq_to_ships = aiuFindDistanceSquared(team->shipList.selection->ShipPtr[0]->obj.posinfo.position,
                                                     invadingShips.ShipPtr[i]->obj.posinfo.position);

            //if other teams need help and if the enemy is less than 10 km away...
            if ((aitCheckIfOtherDefTeamAnsweringSignalNeedsHelp(team, enemyShips)) &&
                (distsq_to_ships < AIH_SLOWDEF_RESPONSE_MAXDISTSQ))
            {
                if (enemyShips->numShips > AIH_SLOWDEF_INVADER_LOTSFEW_LIMIT)
                {
                    formation = AIH_SLOWDEF_INVADER_LOTS_FORMATION;
                }
                else
                {
                    formation = AIH_SLOWDEF_INVADER_FEW_FORMATION;
                }

                newMove         = aimCreateAdvancedAttackNoAdd(team, selectMemDupSelection(enemyShips, "sddh", 0), formation, aioCatAttackTactics(team), TRUE, TRUE);   // CAT: 0x00406c9c
                newMove->events = thisMove->events;
                if (aiuAttackFeatureEnabled(AIA_KAMIKAZE))
                {
                    if (aitTeamShipClassIs(CLASS_Corvette, team))
                    {
                        aieHandlerSetHealthLow(newMove, AIO_CORVETTE_KAMIKAZE_HEALTH, TRUE, FALSE, aihKamikazeHealthLowHandler);
                    }
                }
                newMove->events.interrupt.handler  = NULL;
                newMove->events.numbersLow.handler = NULL;
                team->curMove->processing = FALSE;
                aitAddmoveBeforeAndMakeCurrent(team, newMove, thisMove);
                memFree(enemyShips);
                return;
            }
            memFree(enemyShips);
        }
    }
}


/*-----------------------------------------------------------------------------
    Name        : aihFastDefenseTeamDiedHandler
    Description : Restarts the fast defense team
    Inputs      : team - the team that died
    Outputs     : Deletes all the previous moves and creates new ones
    Return      : void
----------------------------------------------------------------------------*/
void aihFastDefenseTeamDiedHandler(AITeam *team)
{
    aitDeleteAllTeamMoves(team);
    aioCreateFastRovingDefense(team);
}


/*-----------------------------------------------------------------------------
    Name        : aihSlowDefenseTeamDiedHandler
    Description : Restarts the slow defense team
    Inputs      : team - the team that died
    Outputs     : Deletes all the previous moves and creates new ones
    Return      : void
----------------------------------------------------------------------------*/
void aihSlowDefenseTeamDiedHandler(AITeam *team)
{
    aitDeleteAllTeamMoves(team);
    aioCreateSlowRovingDefense(team);
}


/*-----------------------------------------------------------------------------
    Name        : aihGuardShipsTeamDiedHandler
    Description : Restarts the guard ships team
    Inputs      : team - the team that died
    Outputs     : Deletes all the previous moves and create new ones
    Return      : void
----------------------------------------------------------------------------*/
void aihGuardShipsTeamDiedHandler(AITeam *team)
{
    MaxSelection tempsel;

    selSelectionCopy((MaxAnySelection *)&tempsel, (MaxAnySelection *)team->curMove->params.guardShips.ships);

    aitDeleteAllTeamMoves(team);
    aioCreateGuardShips(team, selectMemDupSelection((SelectCommand *)&tempsel, "gustdh", 0));
}


/*-----------------------------------------------------------------------------
    Name        : aihReconaissanceTeamDiedHandler
    Description : Nothing yet
    Inputs      : team - the team that died
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void aihReconaissanceTeamDiedHandler(AITeam *team)
{
    aitDeleteAllTeamMoves(team);

    //later maybe make more distinction between general recon and enemy recon
    aioCreateReconaissance(team, RECON_ACTIVE_ENEMY);
}


/*-----------------------------------------------------------------------------
    Name        : aihReconShipTeamDiedHandler
    Description : Restarts the recon team
    Inputs      :
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void  aihReconShipTeamDiedHandler(AITeam *team)
{
    aitDeleteAllTeamMoves(team);

    //later somehow continue the recon for ships
    aioCreateReconaissance(team, RECON_ACTIVE_ENEMY);
}



/*-----------------------------------------------------------------------------
    Name        : aihHarassTeamDiedHandler
    Description : Restarts the harass team
    Inputs      : team - the team that died
    Outputs     : Deletes all the previous moves and creates new ones
    Return      : void
----------------------------------------------------------------------------*/
void aihHarassTeamDiedHandler(AITeam *team)
{
	if (!bitTest(team->teamFlags, AIT_Reissue01))
	{
		bitSet(team->teamFlags, AIT_Reissue01);
	}
	else
	{
		if (!bitTest(team->teamFlags, AIT_Reissue10))
		{
			bitSet(team->teamFlags, AIT_Reissue10);
		}
		else
		{
			// if we've tried harass teams twice, destroy the team
			bitClear(team->teamFlags, AIT_Reissue10|AIT_Reissue01);
			bitSet(team->teamFlags, AIT_DestroyTeam);
			return;
		}
	}
	if (randyrandom(RANDOM_AI_PLAYER, 4))
	{
		// retry this puppy
		aitDeleteAllTeamMoves(team);
		aioCreateHarass(team);
	}
	else
	{
		bitSet(team->teamFlags, AIT_DestroyTeam);
	}
}


/*-----------------------------------------------------------------------------
    Name        : aihPatrolTeamDiedHandler
    Description : Restarts the patrol team
    Inputs      : team - the team that died
    Outputs     : Flags the team to be destroyed
    Return      : void
----------------------------------------------------------------------------*/
void aihPatrolTeamDiedHandler(AITeam *team)
{
    bitSet(team->teamFlags, AIT_DestroyTeam);
//    aitDestroy(aiCurrentAIPlayer, team, TRUE);
}


/*-----------------------------------------------------------------------------
    Name        : aihRemoveTeamDiedHandler
    Description : Removes the team that died
    Inputs      : team - the team that died
    Outputs     : Flags the team to be destroyed
    Return      : void
----------------------------------------------------------------------------*/
void aihRemoveTeamDiedHandler(AITeam *team)
{
    bitSet(team->teamFlags, AIT_DestroyTeam);
//    aitDestroy(aiCurrentAIPlayer, team, TRUE);
}










