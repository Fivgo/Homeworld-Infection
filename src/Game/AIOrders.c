// =============================================================================
//  AIOrders.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#include "AIOrders.h"

#include "AIEvents.h"
#include "AIHandler.h"
#include "AIMoves.h"
#include "AIPlayer.h"
#include "AIVar.h"
#include "GravWellGenerator.h"
#include "Randy.h"
#include "ResearchAPI.h"
#include "Select.h"
#include "Stats.h"

/*=============================================================================
    Orders Constant Definitions:
=============================================================================*/

// CAT (M2-R7 ai-orders-moves): every order of cataclysm.exe (0x0040e160 .. 0x00410d20) asks for its ships by the
//      computer player's race: the Sect and Beast tables below are the exe's (types and equivalents as the exe stores
//      them on the stack), the Beast rows that name a type its research has not enabled yet fall back to another type
//      (aioCatEnabledOr), and no order has a fuel-low handler (no fuel in Cataclysm). A race the exe has no table for
//      gets no get-ships move, as in the exe.

// CAT: the tactics of a move an order creates for a team: a single-player script team keeps its own (AITeam +0x74,
//      kasTactics), every other team the order's default (the tempguard / armada sites of every strike order)
#define aioCatTeamTactics(team, deflt) \
    ((singlePlayerGame && ((team)->teamType == ScriptTeam)) ? (team)->kasTactics : (deflt))

/*-----------------------------------------------------------------------------
    Name        : aioCreateGuardShips
    Description : Creates an order to get a team to guard ships
    Inputs      : team - the team that will execute the order, ships - the ships to guard
    Outputs     : Creates a whole bunch of orders
    Return      : void
----------------------------------------------------------------------------*/
void aioCreateGuardShips(AITeam *team, SelectCommand *ships)
{
    AlternativeShips alternatives;
    sbyte numShipsToUse;
    AITeamMove *move;
    ShipType shiptype;

    aiplayerLog((aiIndex, "%x Issuing Guard Ships Order", team));

    // CAT: aioCreateGuardShips 0x0040e160: the guards by race (Kushan kHeavyDefender, Taiidan tHeavyDefender with no
    //      alternatives: CAT-FIX, the exe passes its uninitialised stack alternatives for those two races)
    SetNumAlternatives(alternatives,0);
    switch (aiCurrentAIPlayer->player->race)
    {
        case RACE_Sect:
            SetNumAlternativesFlags(alternatives,6,ALTERNATIVE_RANDOM);
            SetAlternative(alternatives,0,sRecon,20);
            SetAlternative(alternatives,1,sMCV,15);
            SetAlternative(alternatives,2,sLeech,10);
            SetAlternative(alternatives,3,sAcolyte,18);
            SetAlternative(alternatives,4,sACV,24);
            SetAlternative(alternatives,5,sSentinel,24);
            break;
        case RACE_Beast:
            SetNumAlternativesFlags(alternatives,9,ALTERNATIVE_RANDOM);
            SetAlternative(alternatives,0,bRecon,20);
            SetAlternative(alternatives,1,bCloakedFighter,15);
            SetAlternative(alternatives,2,bAttackBomber,10);
            SetAlternative(alternatives,3,bInterceptor,18);
            SetAlternative(alternatives,4,bHeavyCorvette,24);
            SetAlternative(alternatives,5,bMultiGunCorvette,24);
            SetAlternative(alternatives,6,aioCatEnabledOr(bAcolyte, bInterceptor),18);
            SetAlternative(alternatives,7,aioCatEnabledOr(bACV, bAttackBomber),24);
            SetAlternative(alternatives,8,aioCatEnabledOr(bSentinel, bCloakedFighter),24);
            break;
        default:
            break;
    }

    if (aiuAnyShipsAreCapitalShips(ships))
    {
        numShipsToUse = AIO_GUARD_SHIPS_NUM_CAPITAL;
    }
    else
    {
        numShipsToUse = AIO_GUARD_SHIPS_NUM_NONCAPITAL;
    }

    switch (aiCurrentAIPlayer->player->race)
    {
        case RACE_Kushan:   shiptype = kHeavyDefender;          break;
        case RACE_Taiidan:  shiptype = tHeavyDefender;          break;
        case RACE_Sect:     shiptype = sAcolyte;                break;
        case RACE_Beast:    shiptype = bInterceptor;            break;
        default:            shiptype = ShipType_Uninitialized;  break;
    }
    if (shiptype != ShipType_Uninitialized)
    {
        aimCreateFancyGetShips(team, shiptype, numShipsToUse, &alternatives, 0, TRUE, FALSE);
    }

    aimCreateFormation(team, SPHERE_FORMATION, FALSE, FALSE);  // vary the formation?
    move = aimCreateGuardShips(team, ships, FALSE, FALSE);
    aieHandlerSetTeamDied(move, aihGuardShipsTeamDiedHandler);
    aimCreateMoveDone(team, FALSE, FALSE);
}


// CAT: the single recon ship of the active recon orders (aioCreateReconaissance 0x0040e3a0 types 1 and 2): one sAcolyte
//      or an sMimic, one bInterceptor or a bCloakedFighter; none for another race
static void aioCatReconGetShip(AITeam *team)
{
    AlternativeShips alternatives;

    if (aiCurrentAIPlayer->player->race == RACE_Sect)
    {
        SetNumAlternatives(alternatives,1);
        SetAlternative(alternatives,0,sMimic,10);
        aimCreateFancyGetShips(team, sAcolyte, 1, &alternatives, REQUESTSHIPS_HIPRI, TRUE, FALSE);
    }
    else if (aiCurrentAIPlayer->player->race == RACE_Beast)
    {
        SetNumAlternatives(alternatives,1);
        SetAlternative(alternatives,0,bCloakedFighter,10);
        aimCreateFancyGetShips(team, bInterceptor, 1, &alternatives, REQUESTSHIPS_HIPRI, TRUE, FALSE);
    }
}

/*-----------------------------------------------------------------------------
    Name        : aioCreateReconaissance
    Description : Creates a the moves for a team to do a reconaissance mission
    Inputs      : team - the team to do the reconaissancing
    Outputs     : Creates a whole bunch of moves for the team
    Return      : void
----------------------------------------------------------------------------*/
void aioCreateReconaissance(AITeam *team, ReconType type)
{
    vector destination, origin = ORIGIN_VECTOR;
    AlternativeShips alternatives;
    AITeamMove *move;

    aiplayerLog((aiIndex, "%x Issuing Reconaissance Order", team));

    switch (type)
    {
        case RECON_MOTHERSHIP:
            //later if there's more than one enemy, need to find both of their mo'ships
            destination = aiuFindEnemyMothershipCoords(aiCurrentAIPlayer->player);
            destination = aiuGenerateRandomStandoffPoint(destination, RECON_STANDOFF_DISTANCE, origin, RSP_NEAR);

            // CAT: one sAcolyte or one of six fighters / corvettes, one bInterceptor or one of eight (0x0040e3a0)
            if (aiCurrentAIPlayer->player->race == RACE_Sect)
            {
                SetNumAlternativesFlags(alternatives,6,ALTERNATIVE_RANDOM);
                SetAlternative(alternatives,0,sACV,14);
                SetAlternative(alternatives,1,sACV,14);
                SetAlternative(alternatives,2,sLeech,14);
                SetAlternative(alternatives,3,sLeech,14);
                SetAlternative(alternatives,4,sRecon,20);
                SetAlternative(alternatives,5,sMimic,10);
                aimCreateFancyGetShips(team, sAcolyte, 1, &alternatives, REQUESTSHIPS_HIPRI, TRUE, FALSE);
            }
            else if (aiCurrentAIPlayer->player->race == RACE_Beast)
            {
                SetNumAlternativesFlags(alternatives,8,ALTERNATIVE_RANDOM);
                SetAlternative(alternatives,0,bHeavyCorvette,14);
                SetAlternative(alternatives,1,bHeavyCorvette,14);
                SetAlternative(alternatives,2,bAttackBomber,14);
                SetAlternative(alternatives,3,bAttackBomber,14);
                SetAlternative(alternatives,4,bRecon,20);
                SetAlternative(alternatives,5,bCloakedFighter,10);
                SetAlternative(alternatives,6,aioCatEnabledOr(bACV, bAttackBomber),14);
                SetAlternative(alternatives,7,aioCatEnabledOr(bACV, bAttackBomber),14);
                aimCreateFancyGetShips(team, bInterceptor, 1, &alternatives, REQUESTSHIPS_HIPRI, TRUE, FALSE);
            }
            aimCreateMoveTeam(team, destination, NO_FORMATION, TRUE, FALSE);
            aimCreateCountShips(team, FALSE, FALSE);
            move = aimCreateActiveRecon(team, TRUE, NO_FORMATION, Evasive, TRUE, FALSE);
            aieHandlerSetTeamDied(move, aihReconaissanceTeamDiedHandler);
            aimCreateMoveDone(team, FALSE, FALSE);
            break;

        case RECON_ACTIVE_GENERAL:
            aioCatReconGetShip(team);
            move = aimCreateActiveRecon(team, FALSE, NO_FORMATION, Evasive, TRUE, FALSE);
            aieHandlerSetTeamDied(move, aihReconaissanceTeamDiedHandler);
            aimCreateMoveDone(team, FALSE, FALSE);
            break;

        case RECON_ACTIVE_ENEMY:
            aioCatReconGetShip(team);
            move = aimCreateActiveRecon(team, TRUE, NO_FORMATION, Evasive, TRUE, FALSE);
            aieHandlerSetTeamDied(move, aihReconaissanceTeamDiedHandler);
            aimCreateMoveDone(team, FALSE, FALSE);
            break;

        default:
            break;
    }


}



/*-----------------------------------------------------------------------------
    Name        : aioCreateHarass
    Description : Creates the moves for a team to perform the harass order
    Inputs      : team - the team to do the harassing
    Outputs     : Creates a whole bunch of moves for the team
    Return      : void
----------------------------------------------------------------------------*/
void aioCreateHarass(AITeam *team)
{
    vector destination, origin = ORIGIN_VECTOR;
    AlternativeShips alternatives;
    AITeamMove *move;

    aiplayerLog((aiIndex, "%x Issuing Harass Order", team));

    bitSet(team->teamFlags, HARASS_TEAM);

    destination = aiuFindEnemyMothershipCoords(aiCurrentAIPlayer->player);
    destination = aiuFindRangeStandoffPoint(destination, origin, HARASS_STANDOFF_DISTANCE);

    // CAT: 15 sAcolytes or sACVs / sLeeches / sMimics, 15 bInterceptors or bMultiGunCorvettes / bAttackBombers /
    //      bCloakedFighters / bACVs (aioCreateHarass 0x0040e740)
    if (aiCurrentAIPlayer->player->race == RACE_Sect)
    {
        SetNumAlternativesFlags(alternatives,3,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives,0,sACV,14);
        SetAlternative(alternatives,1,sLeech,14);
        SetAlternative(alternatives,2,sMimic,10);
        aimCreateFancyGetShips(team, sAcolyte, HARASS_LIGHTINT_INITNUM, &alternatives, REQUESTSHIPS_HIPRI, TRUE, FALSE);
    }
    else if (aiCurrentAIPlayer->player->race == RACE_Beast)
    {
        SetNumAlternativesFlags(alternatives,4,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives,0,bMultiGunCorvette,14);
        SetAlternative(alternatives,1,bAttackBomber,14);
        SetAlternative(alternatives,2,bCloakedFighter,10);
        SetAlternative(alternatives,3,aioCatEnabledOr(bACV, bAttackBomber),14);
        aimCreateFancyGetShips(team, bInterceptor, HARASS_LIGHTINT_INITNUM, &alternatives, REQUESTSHIPS_HIPRI, TRUE, FALSE);
    }

    move = aimCreateMoveTeam(team, destination, AIO_HARASS_INITIAL_MOVE_FORMATION, TRUE, FALSE);
    aieHandlerSetNumbersLow(move, AIO_HARASS_NUMBERS_LOW, 0, TRUE, aihHarassNumbersLowHandler);
    aieHandlerSetGettingRocked(move, FALSE, aihGenericGettingRockedHandler);
    aieHandlerSetTeamDied(move, aihHarassTeamDiedHandler);

//    aimCreateFormation(team, CLAW_FORMATION, FALSE, FALSE);

    move = aimCreateHarassAttack(team, TRUE, FALSE);
    aieHandlerSetNumbersLow(move, AIO_HARASS_NUMBERS_LOW, 0, TRUE, aihHarassNumbersLowHandler);
    aieHandlerSetGettingRocked(move, FALSE, aihGenericGettingRockedHandler);
    aieHandlerSetTeamDied(move, aihHarassTeamDiedHandler);

    aimCreateMoveDone(team, FALSE, FALSE);
}


// CAT: no aioCreateReconHarass (not in cataclysm.exe; the tree never called it)

/*-----------------------------------------------------------------------------
    Name        : aioCreateDefendMothership
    Description : Creates the moves for a team to defend the mothership
                  - this order is not a long term one, it is there for
                    immediate defense of the mothership when it's
                    under attack
    Inputs      : team - the team the order is meant for
                  enemy - the dirty rotten scum sucking low lifes attacking
                  the poor mothership
    Outputs     : Creates a few moves
    Return      : void
----------------------------------------------------------------------------*/
void aioCreateDefendMothership(AITeam *team)
{
    aiplayerLog((aiIndex, "%x Issuing Defend Mothership Order", team));

    //note: ships are assigned to these teams by the defense manager
    //      so there is no getships move
    // CAT: no fuel-low handler (aioCreateDefendMothership 0x0040e950)
    aimCreateDefendMothership(team, TRUE, FALSE);
    aimCreateMoveDone(team, FALSE, FALSE);
}


// CAT: no aioCreatePatrol (not in cataclysm.exe; HW1's note said it was not used)

/*-----------------------------------------------------------------------------
    Name        : aioCreateFastRovingDefense
    Description : Creates a team of roving ships that guard against baddies
    Inputs      : team - the team to do the roving
    Outputs     : Creates a bunch of moves
    Return      : void
----------------------------------------------------------------------------*/
void aioCreateFastRovingDefense(AITeam *team)
{
    AlternativeShips alternatives;
    AITeamMove *move;

    aiplayerLog((aiIndex, "%x Issuing Fast Roving Defense Order", team));

    // CAT: 12 sAcolytes or six strike craft, 12 bInterceptors or eight (aioCreateFastRovingDefense 0x0040e970)
    if (aiCurrentAIPlayer->player->race == RACE_Sect)
    {
        SetNumAlternativesFlags(alternatives,6,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives,0,sACV,14);
        SetAlternative(alternatives,1,sACV,14);
        SetAlternative(alternatives,2,sLeech,14);
        SetAlternative(alternatives,3,sRecon,20);
        SetAlternative(alternatives,4,sMimic,15);
        SetAlternative(alternatives,5,sMCV,15);
        aimCreateFancyGetShips(team, sAcolyte, 12, &alternatives, 0, TRUE, FALSE);
    }
    else if (aiCurrentAIPlayer->player->race == RACE_Beast)
    {
        SetNumAlternativesFlags(alternatives,8,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives,0,bHeavyCorvette,14);
        SetAlternative(alternatives,1,bHeavyCorvette,14);
        SetAlternative(alternatives,2,bMultiGunCorvette,14);
        SetAlternative(alternatives,3,bRecon,20);
        SetAlternative(alternatives,4,bAttackBomber,15);
        SetAlternative(alternatives,5,bMissileCorvette,15);
        SetAlternative(alternatives,6,aioCatEnabledOr(bACV, bAttackBomber),14);
        SetAlternative(alternatives,7,aioCatEnabledOr(bACV, bMultiGunCorvette),14);
        aimCreateFancyGetShips(team, bInterceptor, 12, &alternatives, 0, TRUE, FALSE);
    }
//    aimCreateFormation(team, WALL_FORMATION, FALSE, FALSE);
    move = aimCreateActivePatrol(team, AIT_FAST_PATROL, TRUE, FALSE);
    aieHandlerSetTeamDied(move, aihFastDefenseTeamDiedHandler);
    aieHandlerSetNumbersLow(move, AIO_FASTROVING_NUMBERS_LOW, 0, FALSE, aihFastDefenseNumbersLowHandler);
    aieHandlerSetGettingRocked(move, FALSE, aihGenericGettingRockedHandler);
    aieHandlerSetInterrupt(move, (udword *)&(aiCurrentAIPlayer->aidDefenseTargets), 0, FALSE, aihFastDefenseDistressHandler);

//    aieHandlerSetEnemyNearby(move, 2000, FALSE, aihPatrolEnemyNearbyHandler);
    aimCreateMoveDone(team, FALSE, FALSE);
}


/*-----------------------------------------------------------------------------
    Name        : aioCreateSlowRovingDefense
    Description : Creates a team of slowly roving ships that guard against baddies
    Inputs      : team - the team to the roving
    Outputs     : Creates a bunch of moves
    Return      : void
----------------------------------------------------------------------------*/
void aioCreateSlowRovingDefense(AITeam *team)
{
    AlternativeShips alternatives;
    AITeamMove *move;

    aiplayerLog((aiIndex, "%x Issuing Slow Roving Defense Order", team));

    // CAT: 3 sMultiBeamFrigates or eight others, 3 bMultiGunCorvettes or ten (aioCreateSlowRovingDefense 0x0040eb40)
    if (aiCurrentAIPlayer->player->race == RACE_Sect)
    {
        SetNumAlternativesFlags(alternatives,8,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives,0,sACV,10);
        SetAlternative(alternatives,1,sLeech,10);
        SetAlternative(alternatives,2,sMCV,10);
        SetAlternative(alternatives,3,sSentinel,10);
        SetAlternative(alternatives,4,sRammingFrigate,7);
        SetAlternative(alternatives,5,sDreadnought,30);
        SetAlternative(alternatives,6,sDestroyer,30);
        SetAlternative(alternatives,7,sHiveFrigate,30);
        aimCreateFancyGetShips(team, sMultiBeamFrigate, 3, &alternatives, 0, TRUE, FALSE);
    }
    else if (aiCurrentAIPlayer->player->race == RACE_Beast)
    {
        SetNumAlternativesFlags(alternatives,10,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives,0,bHeavyCorvette,10);
        SetAlternative(alternatives,1,bAttackBomber,10);
        SetAlternative(alternatives,2,bCloakedFighter,10);
        SetAlternative(alternatives,3,bMultiGunCorvette,10);
        SetAlternative(alternatives,4,bIonArrayFrigate,7);
        SetAlternative(alternatives,5,bHeavyCruiser,30);
        SetAlternative(alternatives,6,aioCatEnabledOr(bACV, bAttackBomber),10);
        SetAlternative(alternatives,7,aioCatEnabledOr(bSentinel, bMultiGunCorvette),10);
        SetAlternative(alternatives,8,aioCatEnabledOr(bRammingFrigate, bHeavyCorvette),7);
        SetAlternative(alternatives,9,aioCatEnabledOr(bHiveFrigate, bIonArrayFrigate),30);
        aimCreateFancyGetShips(team, bMultiGunCorvette, 3, &alternatives, 0, TRUE, FALSE);
    }
//    aimCreateFormation(team, DELTA_FORMATION, FALSE, FALSE);
    move = aimCreateActivePatrol(team, AIT_SLOW_PATROL, TRUE, FALSE);
    aieHandlerSetTeamDied(move, aihSlowDefenseTeamDiedHandler);
    aieHandlerSetNumbersLow(move, AIO_SLOWROVING_NUMBERS_LOW, 0, FALSE, aihSlowDefenseNumbersLowHandler);
    aieHandlerSetGettingRocked(move, FALSE, aihGenericGettingRockedHandler);
    aieHandlerSetInterrupt(move, (udword *)&(aiCurrentAIPlayer->aidDefenseTargets), 3, FALSE, aihSlowDefenseDistressHandler);

    aimCreateMoveDone(team, FALSE, FALSE);
}

// CAT: no aioCreateReinforcements (not in cataclysm.exe; the tree never called it)


// CAT: the race whose support ship an empty support team asks for: the exe reads the race of the team's first ship
//      even when the team has none (aioCreateActiveSupport 0x0040ed80 tests numShips == 0, then reads ShipPtr[0]
//      +0x2B4); CAT-FIX: an empty team takes the computer player's race (its ships are that player's)
static ShipRace aioCatSupportTeamRace(AITeam *team)
{
    if (team->shipList.selection->numShips > 0)
    {
        return team->shipList.selection->ShipPtr[0]->shiprace;
    }
    return aiCurrentAIPlayer->player->race;
}

/*-----------------------------------------------------------------------------
    Name        : aioCreateActiveSupport
    Description : Creates the moves to get a team to support another team (i.e. AdvanceSupportFrigate and Repair Corvette)
    Inputs      : team - the team doing the supporting,
                  ships - the ships to support
                    NOTE: ships can be NULL.
    Outputs     : Creates a whole whack of moves
    Return      : void
----------------------------------------------------------------------------*/
void aioCreateActiveSupport(AITeam *team, SelectCommand *ships, SupportType type)
{
    ShipRace race;

    aiplayerLog((aiIndex, "%x Issuing Active Support Order", team));

    if (type == SUPPORT_STRIKECRAFT)
    {
        // CAT: an empty team asks for one sCarrier / bCarrier (0x0040ed98)
        if (!team->shipList.selection->numShips)
        {
            race = aioCatSupportTeamRace(team);
            if (race == RACE_Sect)
            {
                aimCreateGetShips(team, sCarrier, 1, 0, TRUE, FALSE);
            }
            else if (race == RACE_Beast)
            {
                aimCreateGetShips(team, bCarrier, 1, 0, TRUE, FALSE);
            }
        }

        aimCreateSupport(team, ships, BROAD_FORMATION, Evasive, TRUE, FALSE);
    }
    else if (type == SUPPORT_RESOURCE)
    {
        // CAT: an empty team asks for one sProcessor / bProcessor (0x0040edf7; HW1 asked for a ResourceController)
        if (!team->shipList.selection->numShips)
        {
            race = aioCatSupportTeamRace(team);
            if (race == RACE_Sect)
            {
                aimCreateGetShips(team, sProcessor, 1, 0, TRUE, FALSE);
            }
            else if (race == RACE_Beast)
            {
                aimCreateGetShips(team, bProcessor, 1, 0, TRUE, FALSE);
            }
        }

        aimCreateControlResources(team, ships, TRUE, FALSE);
    }
    else if (type == SUPPORT_MOTHERSHIP)
    {
        dbgAssertOrIgnore(team->shipList.selection->numShips);

        aimCreateMothershipMove(team, TRUE, FALSE);
    }
    else
    {
        aiplayerLog((aiIndex, "Error - unknown support type issued"));
        dbgAssertOrIgnore(FALSE);
    }

    aimCreateMoveDone(team, FALSE, FALSE);
}


/*-----------------------------------------------------------------------------
    Name        : aioCreateFighterStrike
    Description : Creates the moves to create and do stuff with a fighter
                  strike force
    Inputs      : team
    Outputs     : Creates a bunch'a moves
    Return      : void
----------------------------------------------------------------------------*/
void aioCreateFighterStrike(AITeam *team)
{
    AlternativeShips alternatives;
    AITeamMove *newmove;

    aiplayerLog((aiIndex, "%x Issuing Fighter Strike Order", team));

    // CAT: 13 sAcolytes or nine others, 15 bAttackBombers or twelve (aioCreateFighterStrike 0x0040ee70)
    if (aiCurrentAIPlayer->player->race == RACE_Sect)
    {
        SetNumAlternativesFlags(alternatives,9,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives,0,sAcolyte,12);
        SetAlternative(alternatives,1,sRecon,20);
        SetAlternative(alternatives,2,sACV,7);
        SetAlternative(alternatives,3,sACV,7);
        SetAlternative(alternatives,4,sMCV,10);
        SetAlternative(alternatives,5,sLeech,10);
        SetAlternative(alternatives,6,sMCV,10);
        SetAlternative(alternatives,7,sMimic,10);
        SetAlternative(alternatives,8,sMimic,10);
        aimCreateFancyGetShips(team, sAcolyte, 13, &alternatives, 0, TRUE, FALSE);
    }
    else if (aiCurrentAIPlayer->player->race == RACE_Beast)
    {
        SetNumAlternativesFlags(alternatives,12,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives,0,bInterceptor,12);
        SetAlternative(alternatives,1,bMultiGunCorvette,7);
        SetAlternative(alternatives,2,bMissileCorvette,7);
        SetAlternative(alternatives,3,bCruiseMissile,7);
        SetAlternative(alternatives,4,bRecon,20);
        SetAlternative(alternatives,5,bAttackBomber,10);
        SetAlternative(alternatives,6,bAttackBomber,10);
        SetAlternative(alternatives,7,bHeavyCorvette,8);
        SetAlternative(alternatives,8,bCloakedFighter,10);
        SetAlternative(alternatives,9,aioCatEnabledOr(bAcolyte, bInterceptor),12);
        SetAlternative(alternatives,10,aioCatEnabledOr(bACV, bMultiGunCorvette),7);
        SetAlternative(alternatives,11,aioCatEnabledOr(bACV, bAttackBomber),7);
        aimCreateFancyGetShips(team, bAttackBomber, 15, &alternatives, 0, TRUE, FALSE);
    }

    aimCreateVarDec(team, aiCurrentAIPlayer->attackVarLabel, TRUE, FALSE);
//later put formation in aiscript
    newmove = aimCreateTempGuard(team, DELTA_FORMATION, aioCatTeamTactics(team, Aggressive), TRUE, FALSE);
    aieHandlerSetGettingRocked(newmove, TRUE, aihGenericGettingRockedHandler);
    aieHandlerSetNumbersLow(newmove, 20, 0, TRUE, aihHarassNumbersLowHandler);

    newmove = aimCreateArmada(team, CLAW_FORMATION, aioCatTeamTactics(team, Aggressive), TRUE, FALSE);
    aieHandlerSetGettingRocked(newmove, TRUE, aihGenericGettingRockedHandler);
    aieHandlerSetNumbersLow(newmove, 20, 0, TRUE, aihHarassNumbersLowHandler);

    aimCreateMoveDone(team, FALSE, FALSE);
}


/*-----------------------------------------------------------------------------
    Name        : aioCreateCorvetteStrike
    Description : Creates the moves to create and do stuff with a fighter
                  strike force
    Inputs      : team
    Outputs     : Creates a bunch'a moves
    Return      : void
----------------------------------------------------------------------------*/
void aioCreateCorvetteStrike(AITeam *team)
{
    AlternativeShips alternatives;
    AITeamMove *newmove;

    aiplayerLog((aiIndex, "%x Issuing Corvette Strike Order", team));

    // CAT: 9 sACVs or seven others, 9 bHeavyCorvettes or twelve (aioCreateCorvetteStrike 0x0040f110)
    if (aiCurrentAIPlayer->player->race == RACE_Sect)
    {
        SetNumAlternativesFlags(alternatives,7,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives,0,sAcolyte,13);
        SetAlternative(alternatives,1,sACV,10);
        SetAlternative(alternatives,2,sACV,10);
        SetAlternative(alternatives,3,sACV,10);
        SetAlternative(alternatives,4,sACV,10);
        SetAlternative(alternatives,5,sMCV,7);
        SetAlternative(alternatives,6,sLeech,7);
        aimCreateFancyGetShips(team, sACV, 9, &alternatives, 0, TRUE, FALSE);
    }
    else if (aiCurrentAIPlayer->player->race == RACE_Beast)
    {
        SetNumAlternativesFlags(alternatives,12,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives,0,bInterceptor,13);
        SetAlternative(alternatives,1,bHeavyCorvette,10);
        SetAlternative(alternatives,2,bHeavyCorvette,10);
        SetAlternative(alternatives,3,bHeavyCorvette,10);
        SetAlternative(alternatives,4,bHeavyCorvette,10);
        SetAlternative(alternatives,5,bMultiGunCorvette,7);
        SetAlternative(alternatives,6,bAttackBomber,7);
        SetAlternative(alternatives,7,bAcolyte,13);
        SetAlternative(alternatives,8,aioCatEnabledOr(bACV, bHeavyCorvette),10);
        SetAlternative(alternatives,9,aioCatEnabledOr(bACV, bMultiGunCorvette),10);
        SetAlternative(alternatives,10,aioCatEnabledOr(bACV, bAttackBomber),10);
        SetAlternative(alternatives,11,aioCatEnabledOr(bACV, bAttackBomber),10);
        aimCreateFancyGetShips(team, bHeavyCorvette, 9, &alternatives, 0, TRUE, FALSE);
    }

    aimCreateVarDec(team, aiCurrentAIPlayer->attackVarLabel, TRUE, FALSE);
//later put formation in aiscript
    newmove = aimCreateTempGuard(team, DELTA3D_FORMATION, aioCatTeamTactics(team, Aggressive), TRUE, FALSE);
    aieHandlerSetGettingRocked(newmove, TRUE, aihGenericGettingRockedHandler);
    aieHandlerSetNumbersLow(newmove, 20, 0, TRUE, aihHarassNumbersLowHandler);

    newmove = aimCreateArmada(team, WALL_FORMATION, aioCatTeamTactics(team, Aggressive), TRUE, FALSE);
    aieHandlerSetGettingRocked(newmove, TRUE, aihGenericGettingRockedHandler);
    aieHandlerSetNumbersLow(newmove, 20, 0, TRUE, aihHarassNumbersLowHandler);

    aimCreateMoveDone(team, FALSE, FALSE);
}


/*-----------------------------------------------------------------------------
    Name        : aioCreateFrigateStrike
    Description : Creates the moves to create and do stuff with a fighter
                  strike force
    Inputs      : team
    Outputs     : Creates a bunch'a moves
    Return      : void
----------------------------------------------------------------------------*/
void aioCreateFrigateStrike(AITeam *team)
{
    AlternativeShips alternatives;
    AITeamMove *newmove;

    aiplayerLog((aiIndex, "%x Issuing Frigate Strike Order", team));

    // CAT: 5 sHiveFrigates or sMultiBeamFrigates, 5 bIonArrayFrigates or the researched Beast frigates
    //      (aioCreateFrigateStrike 0x0040f3a0)
    if (aiCurrentAIPlayer->player->race == RACE_Sect)
    {
        SetNumAlternativesFlags(alternatives,1,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives,0,sMultiBeamFrigate,10);
        aimCreateFancyGetShips(team, sHiveFrigate, 5, &alternatives, 0, TRUE, FALSE);
    }
    else if (aiCurrentAIPlayer->player->race == RACE_Beast)
    {
        SetNumAlternativesFlags(alternatives,3,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives,0,bIonArrayFrigate,10);
        SetAlternative(alternatives,1,aioCatEnabledOr(bMultiBeamFrigate, bIonArrayFrigate),10);
        SetAlternative(alternatives,2,aioCatEnabledOr(bHiveFrigate, bIonArrayFrigate),10);
        aimCreateFancyGetShips(team, bIonArrayFrigate, 5, &alternatives, 0, TRUE, FALSE);
    }

    aimCreateVarDec(team, aiCurrentAIPlayer->attackVarLabel, TRUE, FALSE);
//later put formation in aiscript
    newmove = aimCreateTempGuard(team, WALL_FORMATION, aioCatTeamTactics(team, Aggressive), TRUE, FALSE);
    aieHandlerSetGettingRocked(newmove, TRUE, aihGenericGettingRockedHandler);
    aieHandlerSetNumbersLow(newmove, 20, 0, TRUE, aihHarassNumbersLowHandler);

    newmove = aimCreateArmada(team, CLAW_FORMATION, aioCatTeamTactics(team, Aggressive), TRUE, FALSE);
    aieHandlerSetGettingRocked(newmove, TRUE, aihGenericGettingRockedHandler);
    aieHandlerSetNumbersLow(newmove, 20, 0, TRUE, aihHarassNumbersLowHandler);

    aimCreateMoveDone(team, FALSE, FALSE);

    //this team can hyperspace attack
    bitSet(team->teamFlags, TEAM_Hyperspaceable);

}


/*-----------------------------------------------------------------------------
    Name        : aioCreateResourcer
    Description : Creats the moves to have a team collect resources autonomously
    Inputs      : team - the team
    Outputs     : Creates sum moooooves
    Return      : void
----------------------------------------------------------------------------*/
void aioCreateResourcer(AITeam *team)
{
    aiplayerLog((aiIndex, "%x Issuing Resourcer Order", team));

    //assumes that ships will be added, therefore does not request them
    aimCreateActiveResource(team, TRUE, FALSE);
    aimCreateMoveDone(team, FALSE, FALSE);
}


/*-----------------------------------------------------------------------------
    Name        : aioCreateCapture
    Description : Creates a team to do some capturin'
    Inputs      : team - the team to do some capturin'
    Outputs     : Creates some new moves
    Return      : void
----------------------------------------------------------------------------*/
void aioCreateCapture(AITeam *team)
{
    aiplayerLog((aiIndex, "%x Issuing Capture Order", team));

    // CAT: 3 to 7 kSalCapCorvettes (Kushan) or tSalCapCorvettes (Taiidan), none for another race (aioCreateCapture
    //      0x0040f550)
    if (aiCurrentAIPlayer->player->race == RACE_Kushan)
    {
        aimCreateGetShips(team, kSalCapCorvette, (sbyte)(randyrandombetween(RANDOM_AI_PLAYER, 3, 7)), 0, TRUE, FALSE);
    }
    else if (aiCurrentAIPlayer->player->race == RACE_Taiidan)
    {
        aimCreateGetShips(team, tSalCapCorvette, (sbyte)(randyrandombetween(RANDOM_AI_PLAYER, 3, 7)), 0, TRUE, FALSE);
    }

    aimCreateActiveCapture(team, TRUE, FALSE);
    aimCreateMoveDone(team, FALSE, FALSE);
}


/*-----------------------------------------------------------------------------
    Name        : aioCreateMine
    Description : Creates a team to do some minin'
    Inputs      : team - the minin' team
    Outputs     : Creates some new moves
    Return      : void
----------------------------------------------------------------------------*/
void aioCreateMine(AITeam *team)
{
    aiplayerLog((aiIndex, "%x Issuing Mine Order", team));

    // CAT: only a Taiidan player asks for its 2 to 5 tMinelayerCorvettes (aioCreateMine 0x0040f5d0)
    if (aiCurrentAIPlayer->player->race == RACE_Taiidan)
    {
        aimCreateGetShips(team, tMinelayerCorvette, (sbyte)(randyrandombetween(RANDOM_AI_PLAYER, 2, 5)), 0, TRUE, FALSE);
    }

    aimCreateActiveMine(team, TRUE, FALSE);
    aimCreateMoveDone(team, FALSE, FALSE);
}


/*-----------------------------------------------------------------------------
    Name        : aioCreateSpecialDefense
    Description : Creates a team to do some cloaking
    Inputs      : team - the team to do the cloaking
    Outputs     : Creates some new moves
    Return      : void
----------------------------------------------------------------------------*/
void aioCreateSpecialDefense(AITeam *team, ShipType type)
{
    AITeamMove *move;
    ShipStaticInfo *gravstatics;

    aiplayerLog((aiIndex, "%x Issuing Special Defense Order", team));

    aimCreateGetShips(team, type, 1, 0, TRUE, FALSE);

    move = aimCreateSpecialDefense(team, TRUE, FALSE);

    // CAT: either race's grav well generator (aioCreateSpecialDefense 0x0040f620: 0x40 kGravWellGenerator, 0x4f
    //      tGravWellGenerator); a missing static is skipped as in the exe (no assertion)
    if ((type == kGravWellGenerator) || (type == tGravWellGenerator))
    {
        gravstatics = GetShipStaticInfo(type, cat_ShipTypeToRace(type));

        if (gravstatics)
        {
            aieHandlerSetEnemyNearby(move,
                                     (((GravWellGeneratorStatics *) gravstatics->custstatinfo)->GravWellRadius)*0.8,
                                     FALSE, aihGravWellEnemyNearbyHandler);
            aieHandlerSetTeamDied(move, aihRemoveTeamDiedHandler);
        }
    }

    aimCreateMoveDone(team, FALSE, FALSE);
}


// CAT: no P2 orders (aioCreateSwarmAttack, aioCreateSwarmDefense, aioCreateSwarmSupport, aioCreateMultiBeamAttack,
//      aioCreateP2MothershipAttack): cataclysm.exe has none of them and no P2 race (M2-R7 ai-orders-moves)


void aioCreateTakeoutMothershipFast(struct AITeam *team,Ship *mothership)
{
    AlternativeShips alternatives;
    AITeamMove *attackmove;

    aiplayerLog((aiIndex, "%x Issuing Takeout Mothership Fast Order", team));

    // CAT: 3 sMultiBeamFrigates or eleven others, 3 bIonArrayFrigates or eighteen (aioCreateTakeoutMothershipFast
    //      0x0040f6b0)
    if (aiCurrentAIPlayer->player->race == RACE_Sect)
    {
        SetNumAlternativesFlags(alternatives,11,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives,0,sMultiBeamFrigate,10);
        SetAlternative(alternatives,1,sMultiBeamFrigate,10);
        SetAlternative(alternatives,2,sMultiBeamFrigate,10);
        SetAlternative(alternatives,3,sMultiBeamFrigate,30);
        SetAlternative(alternatives,4,sDreadnought,30);
        SetAlternative(alternatives,5,sDestroyer,30);
        SetAlternative(alternatives,6,sHiveFrigate,10);
        SetAlternative(alternatives,7,sHiveFrigate,10);
        SetAlternative(alternatives,8,sACV,7);
        SetAlternative(alternatives,9,sMCV,3);
        SetAlternative(alternatives,10,sLeech,3);
        aimCreateFancyGetShips(team, sMultiBeamFrigate, 3, &alternatives, 0, TRUE, FALSE);
    }
    else if (aiCurrentAIPlayer->player->race == RACE_Beast)
    {
        SetNumAlternativesFlags(alternatives,18,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives,0,bIonArrayFrigate,10);
        SetAlternative(alternatives,1,bIonArrayFrigate,10);
        SetAlternative(alternatives,2,bIonArrayFrigate,10);
        SetAlternative(alternatives,3,bIonArrayFrigate,30);
        SetAlternative(alternatives,4,bHeavyCruiser,30);
        SetAlternative(alternatives,5,bHeavyCruiser,30);
        SetAlternative(alternatives,6,bMissileCorvette,10);
        SetAlternative(alternatives,7,bMissileCorvette,10);
        SetAlternative(alternatives,8,bAttackBomber,7);
        SetAlternative(alternatives,9,bHeavyCorvette,3);
        SetAlternative(alternatives,10,bMultiGunCorvette,3);
        SetAlternative(alternatives,11,aioCatEnabledOr(bMultiBeamFrigate, bIonArrayFrigate),10);
        SetAlternative(alternatives,12,aioCatEnabledOr(bMultiBeamFrigate, bIonArrayFrigate),10);
        SetAlternative(alternatives,13,aioCatEnabledOr(bMultiBeamFrigate, bIonArrayFrigate),10);
        SetAlternative(alternatives,14,aioCatEnabledOr(bMultiBeamFrigate, bHeavyCruiser),30);
        SetAlternative(alternatives,15,aioCatEnabledOr(bHiveFrigate, bMissileCorvette),10);
        SetAlternative(alternatives,16,aioCatEnabledOr(bHiveFrigate, bMissileCorvette),10);
        SetAlternative(alternatives,17,aioCatEnabledOr(bACV, bAttackBomber),7);
        aimCreateFancyGetShips(team, bIonArrayFrigate, 3, &alternatives, 0, TRUE, FALSE);
    }

    aimCreateVarDec(team, aiCurrentAIPlayer->attackVarLabel, TRUE, FALSE);
//    aimCreateVarWait(team, aiCurrentAIPlayer->attackVarLabel, -1, TRUE, FALSE);
    aimCreateTempGuard(team, AIO_TOUT_MSHIP_FAST_TGRD_FORMATION, aioCatTeamTactics(team, AIO_TOUT_MSHIP_FAST_TGRD_TACTICS), TRUE, FALSE);

    attackmove = aimCreateArmada(team, BROAD_FORMATION, aioCatTeamTactics(team, Aggressive), TRUE, FALSE);
    aieHandlerSetGettingRocked(attackmove, TRUE, aihGenericGettingRockedHandler);

    aimCreateMoveDone(team, FALSE, FALSE);

    //this team can hyperspace attack
    bitSet(team->teamFlags, TEAM_Hyperspaceable);
}

void aioCreateTakeoutMothershipBig(struct AITeam *team,Ship *mothership, bool32 ForceBig)
{
    AlternativeShips alternatives1,alternatives2;
    AITeamMove *attackmove;
    ShipType firstType, secondType, bigShip, frigate, bigMothership, carrier, mediumShip;
    ShipType mothershipType;

    if (!aiCurrentAIPlayer->player->PlayerMothership)
    {
        return;
    }

    aiplayerLog((aiIndex, "%x Issuing Takeout Mothership Big Order", team));

    // CAT: aioCreateTakeoutMothershipBig 0x0040f9c0: the two get-ships moves by race and by the kind of the player's
    //      mothership (the race's mothership or carrier); CAT-FIX: no order for another race (the exe compares the
    //      mothership type with uninitialised locals)
    if (aiCurrentAIPlayer->player->race == RACE_Sect)
    {
        firstType = sMultiBeamFrigate; bigShip = sDreadnought; bigMothership = sMothership; carrier = sCarrier;
        mediumShip = sDestroyer; secondType = sRammingFrigate;
    }
    else if (aiCurrentAIPlayer->player->race == RACE_Beast)
    {
        firstType = bIonArrayFrigate; bigShip = bHeavyCruiser; bigMothership = bMothership; carrier = bCarrier;
        mediumShip = bHeavyCorvette; secondType = bMultiGunCorvette;
    }
    else
    {
        aiplayerLog((aiIndex, "Unknown Mothership Type"));
        return;
    }
    frigate = firstType;

    mothershipType = aiCurrentAIPlayer->player->PlayerMothership->shiptype;
    if ((mothershipType == bigMothership) || ForceBig)
    {
        SetNumAlternativesFlags(alternatives1,2,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives1,0,mediumShip,30);
        SetAlternative(alternatives1,1,bigShip,30);

        SetNumAlternativesFlags(alternatives2,2,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives2,0,bigShip,30);
        SetAlternative(alternatives2,1,frigate,30);
    }
    else if (mothershipType == carrier)
    {
        SetNumAlternativesFlags(alternatives1,1,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives1,0,secondType,10);

        SetNumAlternativesFlags(alternatives2,1,ALTERNATIVE_RANDOM);
        SetAlternative(alternatives2,0,frigate,10);
    }
    else
    {
        aiplayerLog((aiIndex, "Unknown Mothership Type"));
        return;
    }

    aimCreateFancyGetShips(team, firstType, 2, &alternatives1, 0, TRUE, FALSE);
    aimCreateFancyGetShips(team, secondType, 2, &alternatives2, REQUESTSHIPS_HIPRI, TRUE, FALSE);

    aimCreateVarDec(team, aiCurrentAIPlayer->attackVarLabel, TRUE, FALSE);
//    aimCreateVarWait(team, aiCurrentAIPlayer->attackVarLabel, -1, TRUE, FALSE);
    aimCreateTempGuard(team, AIO_TOUT_MSHIP_BIG_TGRD_FORMATION, aioCatTeamTactics(team, AIO_TOUT_MSHIP_BIG_TGRD_TACTICS), TRUE, FALSE);

    attackmove = aimCreateArmada(team, WALL_FORMATION, aioCatTeamTactics(team, Aggressive), TRUE, FALSE);
    aieHandlerSetGettingRocked(attackmove, TRUE, aihGenericGettingRockedHandler);

    aimCreateMoveDone(team, FALSE, FALSE);

    //this team can hyperspace attack
    bitSet(team->teamFlags, TEAM_Hyperspaceable);
}

void aioCreateTakeoutMothershipHuge(struct AITeam *team,Ship *mothership)
{
    AlternativeShips guardalternatives;
    AlternativeShips alternatives;
    AITeam *secondaryteam;
    AIVar *Var0;
    AIVar *Var1;
    char label[AIVAR_LABEL_MAX_LENGTH+1];
    AITeamMove *move;
    ShipType bigShip;

    aiplayerLog((aiIndex, "%x Issuing Takeout Mothership Huge Order", team));

    secondaryteam = aitCreate(team->teamType);
    secondaryteam->cooperatingTeam = team;

    Var0 = aivarCreate(aivarLabelGenerate(label));
    aivarValueSet(Var0, 0);

    Var1 = aivarCreate(aivarLabelGenerate(label));
    aivarValueSet(Var1, 0);

    // CAT: the guard team: 10 sACVs or seven others, 10 bAttackBombers or ten (aioCreateTakeoutMothershipHuge
    //      0x0040fbc0)
    if (aiCurrentAIPlayer->player->race == RACE_Sect)
    {
        SetNumAlternativesFlags(guardalternatives,7,ALTERNATIVE_RANDOM);
        SetAlternative(guardalternatives,0,sRecon,20);
        SetAlternative(guardalternatives,1,sAcolyte,15);
        SetAlternative(guardalternatives,2,sACV,20);
        SetAlternative(guardalternatives,3,sMultiBeamFrigate,25);
        SetAlternative(guardalternatives,4,sHiveFrigate,25);
        SetAlternative(guardalternatives,5,sDestroyer,25);
        SetAlternative(guardalternatives,6,sDreadnought,100);
        aimCreateFancyGetShips(secondaryteam, sACV, (sbyte)10, &guardalternatives, 0, TRUE, FALSE);
    }
    else if (aiCurrentAIPlayer->player->race == RACE_Beast)
    {
        SetNumAlternativesFlags(guardalternatives,10,ALTERNATIVE_RANDOM);
        SetAlternative(guardalternatives,0,bInterceptor,10);
        SetAlternative(guardalternatives,1,bAttackBomber,10);
        SetAlternative(guardalternatives,2,bAttackBomber,15);
        SetAlternative(guardalternatives,3,bHeavyCorvette,20);
        SetAlternative(guardalternatives,4,bMultiGunCorvette,25);
        SetAlternative(guardalternatives,5,bHeavyCruiser,100);
        SetAlternative(guardalternatives,6,aioCatEnabledOr(bAcolyte, bInterceptor),15);
        SetAlternative(guardalternatives,7,aioCatEnabledOr(bACV, bAttackBomber),20);
        SetAlternative(guardalternatives,8,aioCatEnabledOr(bMultiBeamFrigate, bIonArrayFrigate),25);
        SetAlternative(guardalternatives,9,aioCatEnabledOr(bHiveFrigate, bMissileCorvette),25);
        aimCreateFancyGetShips(secondaryteam, bAttackBomber, (sbyte)10, &guardalternatives, 0, TRUE, FALSE);
    }

    aimCreateVarSet(secondaryteam, aivarLabelGet(Var1), TRUE, FALSE, FALSE);

    move = aimCreateVarWait(secondaryteam, aivarLabelGet(Var0), TRUE, TRUE, FALSE);
    aieHandlerSetTeamDied(move, aihRemoveTeamDiedHandler);

    aimCreateVarDestroy(secondaryteam, aivarLabelGet(Var0), FALSE, FALSE);

    aimCreateFormation(secondaryteam, SPHERE_FORMATION, FALSE, FALSE);

    aimCreateGuardCooperatingTeam(secondaryteam, FALSE, FALSE);

    aimCreateMoveDone(secondaryteam, FALSE, FALSE);

    // CAT: the main team: one heavy cruiser of the player's race (kHeavyCruiser, tHeavyCruiser, sDreadnought,
    //      bHeavyCruiser), none for another race
    SetNumAlternatives(alternatives,0);
    switch (aiCurrentAIPlayer->player->race)
    {
        case RACE_Kushan:   bigShip = kHeavyCruiser;            break;
        case RACE_Taiidan:  bigShip = tHeavyCruiser;            break;
        case RACE_Sect:     bigShip = sDreadnought;             break;
        case RACE_Beast:    bigShip = bHeavyCruiser;            break;
        default:            bigShip = ShipType_Uninitialized;   break;
    }
    if (bigShip != ShipType_Uninitialized)
    {
        aimCreateFancyGetShips(team, bigShip, 1, &alternatives, 0, TRUE, FALSE);
    }

    aimCreateVarSet(team, aivarLabelGet(Var0), TRUE, FALSE, FALSE);

    move = aimCreateVarWait(team, aivarLabelGet(Var1), TRUE, TRUE, FALSE);
    aieHandlerSetTeamDied(move, aihRemoveTeamDiedHandler);

    aimCreateVarDestroy(team, aivarLabelGet(Var1), FALSE, FALSE);

    aimCreateVarDec(team, aiCurrentAIPlayer->attackVarLabel, TRUE, FALSE);
//    aimCreateVarWait(team, aiCurrentAIPlayer->attackVarLabel, -1, TRUE, FALSE);
    aimCreateTempGuard(team, NO_FORMATION, aioCatTeamTactics(team, AIO_TOUT_MSHIP_GUARD_TGRD_TACTICS), TRUE, FALSE);

    aimCreateArmada(team, NO_FORMATION, aioCatTeamTactics(team, Aggressive), TRUE, FALSE);

    aimCreateMoveDone(team, FALSE, FALSE);

    //this team can hyperspace attack
    bitSet(team->teamFlags, TEAM_Hyperspaceable);
}

void aioCreateTakeoutMothershipGuard(struct AITeam *team,Ship *mothership)
{
    AlternativeShips alternatives;
    AlternativeShips guardalternatives;
    AITeam *secondaryteam;
    AIVar *Var0;
    AIVar *Var1;
    char label[AIVAR_LABEL_MAX_LENGTH+1];
    AITeamMove *move;
    ShipType mainType;

    if (!aiCurrentAIPlayer->player->PlayerMothership)
    {
        return;
    }

    aiplayerLog((aiIndex, "%x Issuing Takeout Mothership Guard Order", team));

    secondaryteam = aitCreate(team->teamType);
    secondaryteam->cooperatingTeam = team;

    Var0 = aivarCreate(aivarLabelGenerate(label));
    aivarValueSet(Var0, 0);

    Var1 = aivarCreate(aivarLabelGenerate(label));
    aivarValueSet(Var1, 0);

    // CAT: the guard team: 10 sACVs or five others, 10 bAttackBombers or nine (aioCreateTakeoutMothershipGuard
    //      0x0040ff90)
    if (aiCurrentAIPlayer->player->race == RACE_Sect)
    {
        SetNumAlternativesFlags(guardalternatives,5,ALTERNATIVE_RANDOM);
        SetAlternative(guardalternatives,0,sLeech,10);
        SetAlternative(guardalternatives,1,sRecon,20);
        SetAlternative(guardalternatives,2,sAcolyte,15);
        SetAlternative(guardalternatives,3,sACV,20);
        SetAlternative(guardalternatives,4,sMultiBeamFrigate,25);
        aimCreateFancyGetShips(secondaryteam, sACV, (sbyte)10, &guardalternatives, 0, TRUE, FALSE);
    }
    else if (aiCurrentAIPlayer->player->race == RACE_Beast)
    {
        SetNumAlternativesFlags(guardalternatives,9,ALTERNATIVE_RANDOM);
        SetAlternative(guardalternatives,0,bInterceptor,10);
        SetAlternative(guardalternatives,1,bRecon,20);
        SetAlternative(guardalternatives,2,bAttackBomber,10);
        SetAlternative(guardalternatives,3,bCloakedFighter,15);
        SetAlternative(guardalternatives,4,bHeavyCorvette,20);
        SetAlternative(guardalternatives,5,bMultiGunCorvette,25);
        SetAlternative(guardalternatives,6,aioCatEnabledOr(bAcolyte, bInterceptor),15);
        SetAlternative(guardalternatives,7,aioCatEnabledOr(bACV, bAttackBomber),20);
        SetAlternative(guardalternatives,8,aioCatEnabledOr(bMultiBeamFrigate, bMultiGunCorvette),25);
        aimCreateFancyGetShips(secondaryteam, bAttackBomber, (sbyte)10, &guardalternatives, 0, TRUE, FALSE);
    }

    aimCreateVarSet(secondaryteam, aivarLabelGet(Var1), TRUE, FALSE, FALSE);

    move = aimCreateVarWait(secondaryteam, aivarLabelGet(Var0), TRUE, TRUE, FALSE);
    aieHandlerSetTeamDied(move, aihRemoveTeamDiedHandler);

    aimCreateVarDestroy(secondaryteam, aivarLabelGet(Var0), FALSE, FALSE);

    aimCreateFormation(secondaryteam, SPHERE_FORMATION, FALSE, FALSE);

    aimCreateGuardCooperatingTeam(secondaryteam, FALSE, FALSE);

    aimCreateMoveDone(secondaryteam, FALSE, FALSE);

    // CAT: the main team by the player's mothership type: the six Cataclysm motherships and carriers
    switch (aiCurrentAIPlayer->player->PlayerMothership->shiptype)
    {
        case kCarrier:
            SetNumAlternativesFlags(alternatives,1,ALTERNATIVE_RANDOM);
            SetAlternative(alternatives,0,kHeavyCorvette,7);
            mainType = kIonCannonFrigate;
            break;
        case tCarrier:
            SetNumAlternativesFlags(alternatives,2,ALTERNATIVE_RANDOM);
            SetAlternative(alternatives,0,tStandardFrigate,10);
            SetAlternative(alternatives,1,tMinelayerCorvette,7);
            mainType = tIonCannonFrigate;
            break;
        case sMothership:
            SetNumAlternativesFlags(alternatives,6,ALTERNATIVE_RANDOM);
            SetAlternative(alternatives,0,sDreadnought,30);
            SetAlternative(alternatives,1,sDestroyer,30);
            SetAlternative(alternatives,2,sHiveFrigate,30);
            SetAlternative(alternatives,3,sMultiBeamFrigate,10);
            SetAlternative(alternatives,4,sACV,7);
            SetAlternative(alternatives,5,sACV,7);
            mainType = sRammingFrigate;
            break;
        case sCarrier:
            SetNumAlternativesFlags(alternatives,5,ALTERNATIVE_RANDOM);
            SetAlternative(alternatives,0,sAcolyte,30);
            SetAlternative(alternatives,1,sMimic,30);
            SetAlternative(alternatives,2,sLeech,10);
            SetAlternative(alternatives,3,sACV,7);
            SetAlternative(alternatives,4,sMCV,7);
            mainType = sMultiBeamFrigate;
            break;
        case bMothership:
            SetNumAlternativesFlags(alternatives,9,ALTERNATIVE_RANDOM);
            SetAlternative(alternatives,0,bCruiseMissile,30);
            SetAlternative(alternatives,1,bHeavyCruiser,30);
            SetAlternative(alternatives,2,bMissileCorvette,10);
            SetAlternative(alternatives,3,bMultiGunCorvette,7);
            SetAlternative(alternatives,4,bHeavyCorvette,7);
            SetAlternative(alternatives,5,aioCatEnabledOr(bHiveFrigate, bMissileCorvette),30);
            SetAlternative(alternatives,6,aioCatEnabledOr(bMultiBeamFrigate, bIonArrayFrigate),10);
            SetAlternative(alternatives,7,aioCatEnabledOr(bACV, bMultiGunCorvette),7);
            SetAlternative(alternatives,8,aioCatEnabledOr(bACV, bCruiseMissile),7);
            mainType = bIonArrayFrigate;
            break;
        case bCarrier:
            SetNumAlternativesFlags(alternatives,7,ALTERNATIVE_RANDOM);
            SetAlternative(alternatives,0,bACV,10);
            SetAlternative(alternatives,1,bMultiGunCorvette,7);
            SetAlternative(alternatives,2,bHeavyCorvette,7);
            SetAlternative(alternatives,3,aioCatEnabledOr(bHiveFrigate, bMissileCorvette),30);
            SetAlternative(alternatives,4,aioCatEnabledOr(bMultiBeamFrigate, bIonArrayFrigate),10);
            SetAlternative(alternatives,5,aioCatEnabledOr(bACV, bMultiGunCorvette),7);
            SetAlternative(alternatives,6,aioCatEnabledOr(bACV, bAttackBomber),7);
            mainType = bAttackBomber;
            break;
        default:
            aiplayerLog((aiIndex, "Unknown Mothership Type"));
            return;
    }

    aimCreateFancyGetShips(team, mainType, 3, &alternatives, 0, TRUE, FALSE);

//    aimCreateFormation(team, WALL_FORMATION, FALSE, FALSE);

    aimCreateVarSet(team, aivarLabelGet(Var0), TRUE, FALSE, FALSE);

    move = aimCreateVarWait(team, aivarLabelGet(Var1), TRUE, TRUE, FALSE);
    aieHandlerSetTeamDied(move, aihRemoveTeamDiedHandler);

    aimCreateVarDestroy(team, aivarLabelGet(Var1), FALSE, FALSE);

    aimCreateVarDec(team, aiCurrentAIPlayer->attackVarLabel, TRUE, FALSE);
//    aimCreateVarWait(team, aiCurrentAIPlayer->attackVarLabel, -1, TRUE, FALSE);
    aimCreateTempGuard(team, AIO_TOUT_MSHIP_GUARD_TGRD_FORMATION, aioCatTeamTactics(team, AIO_TOUT_MSHIP_GUARD_TGRD_TACTICS), TRUE, FALSE);

//    aimCreateFormation(team, WALL_FORMATION, FALSE, FALSE);

    aimCreateArmada(team, WALL_FORMATION, aioCatTeamTactics(team, Aggressive), TRUE, FALSE);

    aimCreateMoveDone(team, FALSE, FALSE);
}

void aioCreateTakeoutTargetsWithCurrentTeam(struct AITeam *team,SelectCommand *targets)
{
    AITeamMove *attackmove;

    dbgAssertOrIgnore(team->shipList.selection->numShips > 0);

    aiplayerLog((aiIndex, "%x Issuing Takeout Targets With Current Team Order", team));

    attackmove = aimCreateAttack(team, targets, AIO_TOUT_TARG_WCUR_FORMATION, /*AIO_TOUT_TARG_WCUR_TACTICS*/  TRUE, FALSE);
    aieHandlerSetGettingRocked(attackmove, TRUE, aihGenericGettingRockedHandler);
    aimCreateMoveDone(team, FALSE, FALSE);
}

void aioCreateTakeoutTargetWithCurrentTeam(struct AITeam *team,Ship *ship)
{
    SelectCommand *selectone;
    AITeamMove *attackmove;

    dbgAssertOrIgnore(team->shipList.selection->numShips > 0);

    aiplayerLog((aiIndex, "%x Issuing Takeout Target With Current Team Order", team));

    selectone = memAlloc(sizeofSelectCommand(1),"takeoutsel",0);
    selectone->numShips = 1;
    selectone->ShipPtr[0] = ship;

    attackmove = aimCreateAttack(team,selectone, AIO_TOUT_TARG_WCUR_FORMATION, /*AIO_TOUT_TARG_WCUR_TACTICS*/ TRUE, FALSE);
    aieHandlerSetGettingRocked(attackmove, TRUE, aihGenericGettingRockedHandler);
    aimCreateMoveDone(team, FALSE, FALSE);
}

// CAT: the ships a takeout order buys when the stats find none (the switch of aioCreateTakeoutTarget 0x00410700 and
//      aioCreateFancyTakeoutTarget 0x00410920): an sMultiBeamFrigate (the exe logs "Temporarily set to MultiBeam"),
//      a bIonArrayFrigate, a tStandardFrigate, a p1MissileCorvette; any other race keeps what the stats returned
static ShipStaticInfo *aioCatGuessShipToBuy(ShipRace race, ShipStaticInfo *shipsToBuy)
{
    switch (race)
    {
        case RACE_Sect:     return GetShipStaticInfo(sMultiBeamFrigate, race);
        case RACE_Beast:    return GetShipStaticInfo(bIonArrayFrigate, race);
        case RACE_Taiidan:  return GetShipStaticInfo(tStandardFrigate, race);
        case RACE_P1:       return GetShipStaticInfo(p1MissileCorvette, race);
        default:            return shipsToBuy;
    }
}

// CAT: the reserve or build half of both takeout orders (0x00410700 / 0x00410920): the reserve ships when they are good
//      enough or when the player cannot build (no mothership, a dead one (SOF_Dead), a Traders one), else the stats'
//      best ship by the mothership's class (CLASS_Mothership: the fighting-ships constraints, else the carrier's)
static void aioCatTakeoutGetShips(AITeam *team, Ship *target)
{
    ShipStaticInfo *shipsToBuy;
    sdword numShipsToBuy;
    bool32 goodEnough;
    Ship *playerMothership;
    ShipRace race;
    SelectCommand *useTheseShips;

    // check reserves first:
    useTheseShips = statsBestShipsToUseToKillTarget(aiCurrentAIPlayer->newships.selection,target->staticinfo,&goodEnough);
    playerMothership = aiCurrentAIPlayer->player->PlayerMothership;

    if (goodEnough || (playerMothership == NULL) || bitTest(playerMothership->header.flags, SOF_Dead) ||
        (playerMothership->shiprace == RACE_Traders))
    {
        if (useTheseShips)
        {
            // let's put useTheseShips into the team from reserve
            sdword i;
            Ship *ship;
            sdword numShips = useTheseShips->numShips;

            for (i=0;i<numShips;i++)
            {
                ship = useTheseShips->ShipPtr[i];

                growSelectRemoveShip(&aiCurrentAIPlayer->newships,ship);
                aitAddShip(team,ship);
            }
        }
    }
    else
    {
        race = playerMothership->shiprace;

        // we have to build them
        if (playerMothership->staticinfo->shipclass == CLASS_Mothership)
        {
            shipsToBuy = statsBestShipToBuyToKillShip(race,statShipConstraintsFightingShipsCB,target->staticinfo);
        }
        else
        {
            shipsToBuy = statsBestShipToBuyToKillShip(race,statShipConstraintsCarrierFightingShipsCB,target->staticinfo);
        }
        numShipsToBuy = statsNumShipsNeededToKillTarget(shipsToBuy,target->staticinfo);

        if (numShipsToBuy == 0)
        {
            // we don't know what ships to buy, so just arbitrarily pick some.
            shipsToBuy = aioCatGuessShipToBuy(race, shipsToBuy);
            numShipsToBuy = 1;
            aiplayerLog((aiIndex,"Taking out unknown target %d.  Guessing on ship to use",target->shiptype));
        }

        dbgAssertOrIgnore(shipsToBuy);
        aiplayerLog((aiIndex,"Taking out with %i of shiptype %i", numShipsToBuy, shipsToBuy->shiptype));
        aimCreateGetShips(team, shipsToBuy->shiptype, (sbyte)numShipsToBuy, 0, TRUE, FALSE);
    }

    if (useTheseShips != NULL)
    {
        memFree(useTheseShips);
    }
}

void aioCreateTakeoutTarget(struct AITeam *team,Ship *target)
{
    SelectCommand *selectone;
    AITeamMove *attackmove;

    // CAT: only a Sect or Beast target is taken out (aioCreateTakeoutTarget 0x00410700 returns at once otherwise)
    if ((target->shiprace != RACE_Sect) && (target->shiprace != RACE_Beast))
    {
        return;
    }

    aiplayerLog((aiIndex, "%x Issuing Takeout Target Order - Target Type %i", target->shiptype, team));

    aioCatTakeoutGetShips(team, target);

    aimCreateVarDec(team, aiCurrentAIPlayer->attackVarLabel, TRUE, FALSE);
//    aimCreateVarWait(team, aiCurrentAIPlayer->attackVarLabel, -1, TRUE, FALSE);
    aimCreateTempGuard(team, AIO_TOUT_TARG_TGUARD_FORMATION, aioCatTeamTactics(team, AIO_TOUT_TARG_TGUARD_TACTICS), TRUE, FALSE);

    selectone = memAlloc(sizeofSelectCommand(1),"takeoutsel",0);
    selectone->numShips = 1;
    selectone->ShipPtr[0] = target;
    attackmove = aimCreateAttack(team, selectone, BROAD_FORMATION, TRUE, FALSE);
    aieHandlerSetGettingRocked(attackmove, TRUE, aihGenericGettingRockedHandler);
    aimCreateMoveDone(team, FALSE, FALSE);
}

void aioCreateFancyTakeoutTarget(struct AITeam *team,Ship *target)
{
    SelectCommand *selectone;
    AITeam *secondaryteam;
    SelectCommand *nearbydangerousships;
    AITeamMove *move;

    aiplayerLog((aiIndex, "%x Issuing Fancy Takeout Target Order - Target %i", target->shiptype, team));

    aioCatTakeoutGetShips(team, target);

    // we now have primary team.  Do we need a secondary team?

    // CAT: within 16,000 (HW1 1,600; 0x00410a48 pushes 16000.0f)
    nearbydangerousships = aiuFindNearbyDangerousEnemyShips(target,16000.0f);

    if (nearbydangerousships->numShips == 0)
    {
nosecondaryteamnecessary:
        {
        AITeamMove *attackmove;

        aimCreateVarDec(team, aiCurrentAIPlayer->attackVarLabel, TRUE, FALSE);
//        aimCreateVarWait(team, aiCurrentAIPlayer->attackVarLabel, -1, TRUE, FALSE);
        aimCreateTempGuard(team, AIO_TOUT_TARG_FANCY_TGRD_FORMATION, AIO_TOUT_TARG_FANCY_TGRD_TACTICS, TRUE, FALSE);

        selectone = memAlloc(sizeofSelectCommand(1),"takeoutsel",0);
        selectone->numShips = 1;
        selectone->ShipPtr[0] = target;
        attackmove = aimCreateAttack(team, selectone, BROAD_FORMATION, TRUE, FALSE);
        aieHandlerSetGettingRocked(attackmove, TRUE, aihGenericGettingRockedHandler);

        aimCreateMoveDone(team, FALSE, FALSE);
        }
    }
    else
    {
        AIVar *Var0;
        AIVar *Var1;
        sdword secondary_num_ships_to_buy;
        ShipStaticInfo *secondary_ships_to_buy;
        char label[AIVAR_LABEL_MAX_LENGTH+1];

        // CAT: the secondary team's ship is a random attack ship of the player's race (cat_statsRandomAttackShipStatic
        //      0x00524d70, a stream-6 draw; the exe still tests the mothership's class first, both branches alike),
        //      drawn again once when more than 20 would be needed (0x00410a96, 0x00410ad1)
        secondary_ships_to_buy = cat_statsRandomAttackShipStatic(aiCurrentAIPlayer->player->race);
        secondary_num_ships_to_buy = statsNumShipsNeededToKillFleet(secondary_ships_to_buy,nearbydangerousships);

        if (secondary_num_ships_to_buy == 0)
        {
            goto nosecondaryteamnecessary;
        }

        if (secondary_num_ships_to_buy > 20)
        {
            secondary_ships_to_buy = cat_statsRandomAttackShipStatic(aiCurrentAIPlayer->player->race);

            secondary_num_ships_to_buy = statsNumShipsNeededToKillFleet(secondary_ships_to_buy,nearbydangerousships);

            if (secondary_num_ships_to_buy == 0)
            {
                goto nosecondaryteamnecessary;
            }

            if (secondary_num_ships_to_buy > 20)
            {
                secondary_num_ships_to_buy = 20;
            }
        }

        secondaryteam = aitCreate(team->teamType);
        secondaryteam->cooperatingTeam = team;

        Var0 = aivarCreate(aivarLabelGenerate(label));
        aivarValueSet(Var0, 0);

        Var1 = aivarCreate(aivarLabelGenerate(label));
        aivarValueSet(Var1, 0);

        aimCreateGetShips(secondaryteam, secondary_ships_to_buy->shiptype, (sbyte)secondary_num_ships_to_buy, 0, TRUE, FALSE);

        aimCreateVarSet(secondaryteam, aivarLabelGet(Var1), TRUE, FALSE, FALSE);

        move = aimCreateVarWait(secondaryteam, aivarLabelGet(Var0), TRUE, TRUE, FALSE);
        aieHandlerSetTeamDied(move, aihRemoveTeamDiedHandler);

        aimCreateVarDestroy(secondaryteam, aivarLabelGet(Var0), FALSE, FALSE);

        aimCreateGuardCooperatingTeam(secondaryteam, FALSE, FALSE);

        aimCreateMoveDone(secondaryteam, FALSE, FALSE);


        aimCreateVarSet(team, aivarLabelGet(Var0), TRUE, FALSE, FALSE);

        move = aimCreateVarWait(team, aivarLabelGet(Var1), TRUE, TRUE, FALSE);
        aieHandlerSetTeamDied(move, aihRemoveTeamDiedHandler);

        aimCreateVarDestroy(team, aivarLabelGet(Var1), FALSE, FALSE);

        aimCreateVarDec(team, aiCurrentAIPlayer->attackVarLabel, TRUE, FALSE);
//        aimCreateVarWait(team, aiCurrentAIPlayer->attackVarLabel, -1, TRUE, FALSE);
        aimCreateTempGuard(team, AIO_TOUT_TARG_FANCY_TGRD_FORMATION, AIO_TOUT_TARG_FANCY_TGRD_TACTICS, TRUE, FALSE);

        selectone = memAlloc(sizeofSelectCommand(1),"takeoutsel",0);
        selectone->numShips = 1;
        selectone->ShipPtr[0] = target;
        aimCreateAttack(team, selectone, BROAD_FORMATION, TRUE, FALSE);

        aimCreateMoveDone(team, FALSE, FALSE);
    }

    aiumemFree(nearbydangerousships);
}
