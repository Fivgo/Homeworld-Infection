// =============================================================================
//  Crates.c
//  - crate ("goodie box") handling in multiplayer games
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created bryce in november
// =============================================================================

#include "Crates.h"

#include "AITrack.h"
#include "Blobs.h"
#include "LevelLoad.h"
#include "MultiplayerGame.h"
#include "PiePlate.h"
#include "Randy.h"
#include "Sensors.h"
#include "SoundEvent.h"
#include "StatScript.h"
#include "Universe.h"
#include "UnivUpdate.h"


real32 CRATES_DoWeNeedToAddCratesCheckTime =380.0f;  //every X seconds, the crate functions will check to see if it should add a crate to the world

sdword CRATES_ArePlayersNearUpdateMask= 63;  //bitmask to compare with univupdatecounter to see if players are near the crate
sdword CRATES_ArePlayersNearUpdateFrame=52;  //frame to check must be <= CRATES_ArePlayersNearUpdateMask
real32 CRATES_PlayerNearRadius = 2500.0f;     //distance a player needs to get to a crate in order to 'get' the crate

real32 CRATES_MaximumCratesInTheWorldPerPlayer =1;  //don't a   //probability that you get a ship as opposed to something else
real32 CRATES_GetResearch                          = 0.25f;

real32 CRATES_RandomLowDistance  =  7000.0f;
real32 CRATES_RandomHighDistance =  20000.0f;

real32 CRATES_AddACrateProb   =  0.15f;         //probability that we add a crate

sdword CRATES_MAX_RUS_GIVEN =   10000;
sdword CRATES_MIN_RUS_GIVEN =   500 ;

real32 CRATES_GetMotherShipCarrierIfDontHaveOne  = 0.80f;   //probability that you get a carrier/mothership if you didn't have one
real32 CRATES_GetAShip                         =    0.50f;//research probability
//defaults
real32 CrateClassProbCLASS_HeavyCruiser = 0.99f;
real32 CrateClassProbCLASS_Carrier = 0.97f;
real32 CrateClassProbCLASS_Destroyer = 0.90f;
real32 CrateClassProbCLASS_Frigate = 0.75f;
real32 CrateClassProbCLASS_Corvette = 0.45f;
real32 CrateClassProbCLASS_Fighter = 0.10f;
real32 CrateClassProbCLASS_Resource = 0.5f;
real32 CrateClassProbCLASS_NonCombat = 0.0f;
// CAT: indexed by Cataclysm type ids up to bMissileCorvette (59) from crates.script, so sized by the type enum
//      (SHIP_PROBS 0x00c43400 spans 0x180 bytes before the next global; NUM_IN_GROUPS is at 0x00c435c0). HW1's
//      [StandardFrigate+1] would take the ASan out-of-bounds write of data-deltas run7.
sdword NUM_IN_GROUPS[TOTAL_NUM_SHIPS];  //number of ships to be placed in agroup
real32 SHIP_PROBS[TOTAL_NUM_SHIPS];
real32 CRATE_EXPIRY_TIME = 300.0;

///RU's will be given instead
scriptEntry cratesScriptTable[] =
{
    makeEntry(CRATES_DoWeNeedToAddCratesCheckTime,scriptSetReal32CB),
    makeEntry(CRATES_ArePlayersNearUpdateMask,scriptSetSdwordCB),
    makeEntry(CRATES_ArePlayersNearUpdateFrame,scriptSetSdwordCB),
    makeEntry(CRATES_PlayerNearRadius,scriptSetReal32CB),
    makeEntry(CRATES_MaximumCratesInTheWorldPerPlayer,scriptSetReal32CB),
    makeEntry(CRATES_GetResearch,scriptSetReal32CB),
    makeEntry(CRATES_RandomLowDistance,scriptSetReal32CB),
    makeEntry(CRATES_RandomHighDistance,scriptSetReal32CB),
    makeEntry(CRATES_AddACrateProb,scriptSetReal32CB),
    makeEntry(CRATES_MAX_RUS_GIVEN,scriptSetSdwordCB),
    makeEntry(CRATES_MIN_RUS_GIVEN,scriptSetSdwordCB),
    makeEntry(CRATES_GetMotherShipCarrierIfDontHaveOne,scriptSetReal32CB),
    makeEntry(CRATES_GetAShip,scriptSetReal32CB),
    makeEntry(CrateClassProbCLASS_HeavyCruiser,scriptSetReal32CB),
    makeEntry(CrateClassProbCLASS_Carrier,scriptSetReal32CB),
    makeEntry(CrateClassProbCLASS_Destroyer,scriptSetReal32CB),
    makeEntry(CrateClassProbCLASS_Frigate,scriptSetReal32CB),
    makeEntry(CrateClassProbCLASS_Corvette,scriptSetReal32CB),
    makeEntry(CrateClassProbCLASS_Fighter,scriptSetReal32CB),
    makeEntry(CrateClassProbCLASS_Resource,scriptSetReal32CB),
    makeEntry(CrateClassProbCLASS_NonCombat,scriptSetReal32CB),
    makeEntry(CRATE_EXPIRY_TIME,scriptSetReal32CB),

    {"shipProbability", (void(*)(char *,char *,void*))scriptSetShipProbCB, SHIP_PROBS },
    {"shipGroupSize", (void(*)(char *,char *,void*))scriptSetShipGroupSizeCB, NUM_IN_GROUPS },

    END_SCRIPT_ENTRY
};

//crate reward types
#define CRATE_SHIP  0
#define CRATE_MONEY 1
#define CRATE_RESEARCH 2

void cratesPlaceCrateSelectivly(void);
void cratesReportCratePlacement(Derelict *crate);
void cratesPlayerFoundACrate(Derelict *crate, Player *player);
void cratesGetBestCrateLocation(vector *location);
sdword cratesRewardPlayer(Derelict *crate, Player *player);
void expireCrate(Derelict *crate);


void cratesScanForPlayersNearCrateAndReward(Derelict *crate)
{
    blob *crateBlob;
    sdword i;
    Ship *ship;

    crateBlob = crate->obj.collMyBlob;

    if(crateBlob != NULL)
    {
        for(i=0;i<crateBlob->blobShips->numShips;)
        {
            ship = crateBlob->blobShips->ShipPtr[i];
            if(MoveReachedDestinationVariable(ship,&crate->obj.posinfo.position,CRATES_PlayerNearRadius))
            {
                if(ship->playerowner != NULL)
                {
                    cratesPlayerFoundACrate(crate,ship->playerowner);
                    break;
                }
            }
            i++;    //move to next ship
        }
    }
}

void cratesPlaceCrateSelectivly(void)
{
    vector location;
    Derelict *crate;
    //etglod *etgLOD;
    //etgeffectstatic *stat;


    cratesGetBestCrateLocation(&location);

    crate = univAddDerelict(Crate,&location);   //calls reporting code!
    //cratesReportCratePlacement(crate);
    //play etg effect for placement
    //play sound event?
}

void cratesReportCratePlacement(Derelict *crate)
{
    etglod *etgLOD;
    etgeffectstatic *stat;
    vector zero = {0.0f,0.0f,0.0f};
    matrix ident = IdentityMatrix;

    universe.numCratesInWorld++;

    if (gameIsRunning)
    {
        etgLOD = etgSpecialPurposeEffectTable[EGT_CRATE_GENERATED];

        if (etgLOD != NULL)
        {
            if (crate->obj.currentLOD >= etgLOD->nLevels)
            {
                stat = NULL;
            }
            else
            {
                stat = etgLOD->level[crate->obj.currentLOD];
            }
        }
        else
        {
            stat = NULL;
        }
        #if ETG_DISABLEABLE
        if (stat != NULL && etgEffectsEnabled)
        #else
        if (stat != NULL)
        #endif
        {
            etgEffectCreate(stat, NULL, &crate->obj.posinfo.position, &zero, &ident, 1.0f, EAF_AllButNLips, 0);
        }
    }
    dbgAssertOrIgnore(universe.numCratesInWorld <= CRATES_MaximumCratesInTheWorldPerPlayer*MAX_MULTIPLAYER_PLAYERS);

}

void cratesPlayerFoundACrate(Derelict *crate, Player *player)
{
    etglod *etgLOD = NULL;
    etgeffectstatic *stat;
    vector zero = {0.0f,0.0f,0.0f};
    matrix ident = IdentityMatrix;
    sdword type;

    universe.numCratesInWorld--;

    type = cratesRewardPlayer(crate,player);

    // only play sound for this player
    if(player->playerIndex == universe.curPlayerIndex)
    {
        soundEvent(NULL, CAT_UI_CrateFound);        // CAT: Cataclysm's id 0x128 (0x00460dc2)
    }
    /* mabe there should be a different sound for each type of crate */

    switch(type)
    {
    case CRATE_SHIP:
        etgLOD = etgSpecialPurposeEffectTable[EGT_CRATE_IS_FOUND_SHIP];
        if(player->playerIndex == universe.curPlayerIndex)
        {
            speechEventFleet(STAT_F_CrateFoundShips,0,player->playerIndex);
        }
        break;
    case CRATE_MONEY:
        etgLOD = etgSpecialPurposeEffectTable[EGT_CRATE_IS_FOUND_MONEY];
        if(player->playerIndex == universe.curPlayerIndex)
        {
            speechEventFleet(STAT_F_CrateFoundResources,0,player->playerIndex);
        }
        break;
    case CRATE_RESEARCH:
        etgLOD = etgSpecialPurposeEffectTable[EGT_CRATE_IS_FOUND_RESEARCH];
        if(player->playerIndex == universe.curPlayerIndex)
        {
            speechEventFleet(STAT_F_CrateFoundTech,0,player->playerIndex);
        }
        break;
    default:
        dbgFatalf(DBG_Loc,"\nUnknown crate contents!");
        break;
    }

    if (etgLOD != NULL)
    {
        if (crate->obj.currentLOD >= etgLOD->nLevels)
        {
            stat = NULL;
        }
        else
        {
            stat = etgLOD->level[crate->obj.currentLOD];
        }
    }
    else
    {
        stat = NULL;
    }
    #if ETG_DISABLEABLE
    if (stat != NULL && etgEffectsEnabled)
    #else
    if (stat != NULL)
    #endif
    {
        etgEffectCreate(stat, NULL, &crate->obj.posinfo.position, &zero, &ident, 1.0f, EAF_AllButNLips, 0);
    }

    // CAT: 3 arguments; this caller passes 0 (cratesPlayerFoundACrate 0x00460d60, push 0 at 0x00460ebc). The body
    //      never reads the third argument (AddTargetToDeleteList 0x00545dd0) [audit: was 1]
    AddTargetToDeleteList((SpaceObjRotImpTarg *)crate,-1,0);
    }

//maximum number of ships in anyone class
#define MAX_NUM_IN_CLASS 10
// CAT: Cataclysm's crate ship lists per class (exe .data 0x00878934..0x008789bb, read by cratesRewardPlayer
//      0x00460ed0; crateInit 0x00462080 sets NUM_IN_CLASSES to 0,2,2,2,8,5,9,4,2, which these sizes give)
sdword cratesCLASS_HeavyCruiser[] =
{
    sDreadnought,
    bHeavyCruiser
};
sdword cratesCLASS_Carrier[] =
{
    sCarrier,
    bCarrier
};
sdword cratesCLASS_Destroyer[] =
{
    bIonArrayFrigate,
    sDestroyer
};
sdword cratesCLASS_Frigate[] =
{
    sRammingFrigate,
    bRammingFrigate,
    sMultiBeamFrigate,
    sHiveFrigate,
    bMultiBeamFrigate,
    bHiveFrigate,
    bIonArrayFrigate,
    bDFGFrigate
};
sdword cratesCLASS_Corvette[] =
{
    sACV,
    sMCV,
    bACV,
    bMultiGunCorvette,
    bHeavyCorvette
};
sdword cratesCLASS_Fighter[] =
{
    sRecon,
    sAcolyte,
    sMimic,
    sLeech,
    bRecon,
    bAcolyte,
    bInterceptor,
    bCloakedFighter,
    bAttackBomber
};
sdword cratesCLASS_Resource[] =
{
    sWorker,
    sProcessor,
    bWorker,
    bProcessor
};
sdword cratesCLASS_NonCombat[] =
{
    sSentinel,
    bSentinel
};

sdword NUM_IN_CLASSES[NUM_CLASSES];



// CAT: no getShipClass / getShipType (0x00460cb0-0x00462080 has neither; the ladders of cratesRewardPlayer replace them)

// CAT (M2-R4 rest-oracle): cratesRewardPlayer 0x00460ed0. The probabilities are gamerand() / 2^32 (the exe multiplies the
//      32-bit draw by 2.3283064e-10); the ship and technology ladders are the exe's (double thresholds at 0x0072a470 ..
//      0x0072a7b0, read from cataclysm.exe). HW1's SHIP_PROBS / NUM_IN_GROUPS tables, getShipClass / getShipType and the
//      mothership-if-you-have-none gift are gone.
#define CAT_RAN_TO_UNIT     2.3283064365386963e-10f

static real32 cat_cratesProbability(void)
{
    return (real32)gamerand() * CAT_RAN_TO_UNIT;
}

// the ship type and how many of it a crate gives, per race (the ladder of 0x00460ed0)
static ShipType cat_cratesShipReward(ShipRace race, real32 prob, sdword *number)
{
    if (race == RACE_Sect)
    {
        if      (prob < 0.01) { *number = 1;  return sDreadnought; }
        else if (prob < 0.04) { *number = 1;  return sDestroyer; }
        else if (prob < 0.1)  { *number = 1;  return sMultiBeamFrigate; }
        else if (prob < 0.15) { *number = 2;  return sMultiBeamFrigate; }
        else if (prob < 0.2)  { *number = 2;  return sRammingFrigate; }
        else if (prob < 0.3)  { *number = 5;  return sSentinel; }
        else if (prob < 0.4)  { *number = 4;  return sACV; }
        else if (prob < 0.5)  { *number = 4;  return sMCV; }
        else if (prob < 0.6)  { *number = 8;  return sAcolyte; }
        else if (prob < 0.7)  { *number = 8;  return sMimic; }
        else if (prob < 0.8)  { *number = 10; return sRecon; }
        else                  { *number = 1;  return sWorker; }
    }
    if      (prob < 0.01) { *number = 1;  return bHeavyCruiser; }
    else if (prob < 0.04) { *number = 3;  return bIonArrayFrigate; }
    else if (prob < 0.1)  { *number = 2;  return bHiveFrigate; }
    else if (prob < 0.15) { *number = 2;  return bMultiBeamFrigate; }
    else if (prob < 0.2)  { *number = 2;  return bRammingFrigate; }
    else if (prob < 0.3)  { *number = 5;  return bSentinel; }
    else if (prob < 0.4)  { *number = 4;  return bACV; }
    else if (prob < 0.5)  { *number = 3;  return bMultiGunCorvette; }
    else if (prob < 0.6)  { *number = 8;  return bInterceptor; }
    else if (prob < 0.7)  { *number = 3;  return bHeavyCorvette; }
    else if (prob < 0.8)  { *number = 10; return bRecon; }
    else                  { *number = 1;  return bWorker; }
}

// the technology a crate gives, per race (the ladder of 0x00460ed0; HW1 gave the first technology the player lacked)
static TechnologyType cat_cratesTechReward(ShipRace race, real32 prob)
{
    if (race == RACE_Sect)
    {
        if      (prob < 0.05) return TECH_REPAIR;
        else if (prob < 0.1)  return TECH_SALVAGE;
        else if (prob < 0.15) return TECH_DRIVE_DESTROYER;
        else if (prob < 0.2)  return TECH_ARMOUR2;
        else if (prob < 0.25) return TECH_MICRO_SHIP;
        else if (prob < 0.3)  return TECH_ENERGY;
        else if (prob < 0.35) return TECH_ION;
        else if (prob < 0.4)  return TECH_MISSILE;
        else if (prob < 0.45) return TECH_EXPLOSIVE_CHARGE;
        else if (prob < 0.5)  return TECH_BIG_GUN;
        else if (prob < 0.55) return TECH_ADVANCED_SENSORS;
        else if (prob < 0.6)  return TECH_FORCE_FIELD1;
        else if (prob < 0.65) return TECH_REPAIRBOTS;
        else if (prob < 0.7)  return TECH_CRYSTAL_HARVEST;
        else if (prob < 0.75) return TECH_BEAST_VACCINE;
        else if (prob < 0.8)  return TECH_PHASED_TELEMETRY;
        else if (prob < 0.85) return TECH_MIMIC;
        else if (prob < 0.9)  return TECH_LINK;
        else if (prob < 0.95) return TECH_EMP;
        else                  return TECH_REPULSE;
    }
    if      (prob < 0.05) return TECH_CLOAKING;
    else if (prob < 0.1)  return TECH_TARGETING_SYSTEMS;
    else if (prob < 0.15) return TECH_PLASMA_WEAPONS;
    else if (prob < 0.2)  return TECH_MEDIUM_GUNS;
    else if (prob < 0.25) return TECH_DEFENSE_FIELD;
    else if (prob < 0.3)  return TECH_HEAVY_GUNS;
    else if (prob < 0.35) return TECH_REPAIR;
    else if (prob < 0.4)  return TECH_DRIVE_FIGHTER;
    else if (prob < 0.45) return TECH_MICRO_SHIP;
    else if (prob < 0.5)  return TECH_ENERGY;
    else if (prob < 0.55) return TECH_ION;
    else if (prob < 0.6)  return TECH_MISSILE;
    else if (prob < 0.65) return TECH_LINK;
    else if (prob < 0.7)  return TECH_AFTERBURNER;
    else if (prob < 0.75) return TECH_ADVANCED_SENSORS;
    else if (prob < 0.8)  return TECH_INFECT;
    else if (prob < 0.85) return TECH_CRYSTAL_HARVEST;
    else if (prob < 0.9)  return TECH_BIOWARHEAD;
    else if (prob < 0.95) return TECH_DRIVE_CORVETTE;
    else                  return TECH_ARMOUR2;
}

sdword cratesRewardPlayer(Derelict *crate, Player *player)
{
    real32 prob;
    sdword rus;
    sdword shipnumber;
    sdword i, tries;
    Ship *ship;
    ShipStaticInfo *teststatic;
    ShipType shiptype;
    TechnologyType tech;
    vector cratepos;
    SelectCommand *selectcom;

    prob = cat_cratesProbability();
    if(prob <= CRATES_GetAShip)
    {
        cratepos = crate->obj.posinfo.position;
        shiptype = cat_cratesShipReward(player->race, cat_cratesProbability(), &shipnumber);

        teststatic = GetShipStaticInfoSafe(shiptype,player->race);
        if ((teststatic != NULL) && bitTest(teststatic->common.infoFlags, IF_InfoLoaded))
        {
            selectcom = memAlloc(sizeofSelectCommand(shipnumber),"selectform",0);
            selectcom->numShips = shipnumber;

            dbgMessagef("Got a %d ship type  from crate!",shiptype);

            //could go over unit cap limit here...
            for (i=0;i<shipnumber;i++)
            {
                ship = univAddShip(shiptype,player->race,&cratepos,player,0);
                gameStatsAddShip(ship,player->playerIndex);
                // add the "free ships" value to the initial starting value
                universe.players[ship->playerowner->playerIndex].initialShipCost += ship->staticinfo->buildCost;

                                // Update ship totals for player
                unitCapCreateShip(ship,player);

                selectcom->ShipPtr[i] = ship;

                cratepos.z += (ship->staticinfo->common.staticCollInfo.collspheresize * 2.0f);
            }
            // CAT: the exe's clFormation takes a 4th argument, 0 here (cratesRewardPlayer 0x004612ce)
            clFormation(&universe.mainCommandLayer,selectcom,BROAD_FORMATION,0);
            memFree(selectcom);
            return(CRATE_SHIP);
        }
    }

    prob = cat_cratesProbability();
    if(prob <= CRATES_GetResearch)
    {
        //give research: up to 5 draws for a technology the player does not have
        dbgMessagef("Got research from crate");
        tries = 0;
        do
        {
            tech = cat_cratesTechReward(player->race, cat_cratesProbability());
            tries++;
        } while (!cat_rmTechSetHas(player->researchinfo.HasTechnology, tech) && (tries < 5));
        if (!cat_rmTechSetHas(player->researchinfo.HasTechnology, tech))
        {
            rmGiveTechToPlayerByType(player,tech);
            return(CRATE_RESEARCH);
        }
        //no tech to give!
    }

    rus = (sdword)(gamerand() % (udword)(CRATES_MAX_RUS_GIVEN - CRATES_MIN_RUS_GIVEN + 1)) + CRATES_MIN_RUS_GIVEN;
    dbgMessagef("Got %d RU's from crate.",rus);

    player->resourceUnits += rus;
    universe.gameStats.playerStats[player->playerIndex].totalResourceUnitsCollected += rus;
    universe.gameStats.playerStats[player->playerIndex].totalResourceUnits += rus;
    return(CRATE_MONEY);
}

void getPlayerShipCenter(vector *shipCenter,sdword index)
{
    Node *node=universe.ShipList.head;
    Ship *ship;
    shipCenter->x =0.0f;
    shipCenter->y =0.0f;
    shipCenter->z =0.0f;
    while(node!=NULL)
    {
        ship = (Ship *)listGetStructOfNode(node);

        if(ship->playerowner != NULL)
        {
            if(ship->playerowner->playerIndex == index)
            {
                shipCenter->x = ship->obj.posinfo.position.x;
                shipCenter->y = ship->obj.posinfo.position.y;
                shipCenter->z = ship->obj.posinfo.position.z;
                return;
            }
        }

        node = node->next;
    }

}

sdword cratesIsLocationGood(vector *location)
{
    Node *node = universe.collBlobList.head;
    blob *curblob;
    real32 blobradiusSqr,seperationSqr;
    vector seperation;

    while(node != NULL)
    {
        curblob = (blob *) listGetStructOfNode(node);
        blobradiusSqr = curblob->radius*curblob->radius;
        vecSub(seperation,*location,curblob->centre);
        seperationSqr = vecMagnitudeSquared(seperation);
        if(seperationSqr < blobradiusSqr)
        {
            sdword i;
            //in blob..need to do detecting
            for(i=0;i<curblob->blobShips->numShips;i++)
            {
                vecSub(seperation,*location,curblob->blobShips->ShipPtr[i]->impactable.collInfo.collPosition);
                if(vecMagnitudeSquared(seperation) < curblob->blobShips->ShipPtr[i]->staticinfo->common.staticCollInfo.collspheresizeSqr)
                {
                    //WITHIN collision radius of ship
                    return FALSE;
                }
                //fine...so continue
            }
        }

        node = node->next;
    }

    return TRUE;

}
void cratesGetBestCrateLocation(vector *location)
{
    vector destinationVectors[MAX_MULTIPLAYER_PLAYERS];
    vector centerMotherShips = {0.0f,0.0f,0.0f};
    vector mothershipcenter;
    vector displacement = {0.0f,1000.0f,0.0f};
    sdword counted=0;

    sdword i;

    for(i=0;i<universe.numPlayers;i++)
    {
        //could store a players biggest ship here..so as to help them get
        //the crate
        if(universe.players[i].playerState == PLAYER_DEAD)
            continue;

        counted++;
        if(universe.players[i].PlayerMothership == NULL)
        {
            vector shipCenter;
            getPlayerShipCenter(&shipCenter,i);
            vecAddTo(centerMotherShips,shipCenter);
        }
        else
        {
            vecAddTo(centerMotherShips,universe.players[i].PlayerMothership->obj.posinfo.position);
        }
    }
    //get average
    vecScalarMultiply(centerMotherShips,centerMotherShips,1.0f/counted);

    for(i=0;i<universe.numPlayers;i++)
    {
        if(universe.players[i].playerState == PLAYER_DEAD)
            continue;
        if(universe.players[i].PlayerMothership == NULL)
        {
            getPlayerShipCenter(&mothershipcenter,i);
        }
        else
        {
            mothershipcenter = universe.players[i].PlayerMothership->obj.posinfo.position;
        }

        vecSub(destinationVectors[i],mothershipcenter,centerMotherShips);
        vecNormalize(&destinationVectors[i]);
    }

    for(i=0;i<universe.numPlayers;i++)
    {
        if(universe.players[i].playerState == PLAYER_DEAD)
            continue;
        vecScalarMultiply(destinationVectors[i],destinationVectors[i],1.0f/(max(universe.players[i].bounty,1.0f)));
    }




    location->x = 0.0f;
    location->y = 0.0f;
    location->z = 0.0f;

    //set starting vector to favor direction that weaker players are in
    for(i=0;i<universe.numPlayers;i++)
    {
        if(universe.players[i].playerState == PLAYER_DEAD)
            continue;
        vecAddTo(*location,destinationVectors[i]);
    }

    vecNormalize(location);
    location->x *= frandombetween(CRATES_RandomLowDistance,CRATES_RandomHighDistance);
    location->y *= frandombetween(CRATES_RandomLowDistance,CRATES_RandomHighDistance);
    location->z *= frandombetween(CRATES_RandomLowDistance,CRATES_RandomHighDistance);

    //add a large random element to location
    vecAddTo(*location,centerMotherShips);
    location->x += frandombetween(-smUniverseSizeX/3,smUniverseSizeX/3);
    location->y += frandombetween(-smUniverseSizeY/3,smUniverseSizeY/3);
    location->z += frandombetween(-smUniverseSizeZ/3,smUniverseSizeZ/3);

    //add a displacement offset to the location until
    //the location is valid

    //add a bit of randomness to the displacement vector
    displacement.x += frandombetween(-1000.0f,1000.0f);
    displacement.y += frandombetween(-1000.0f,1000.0f);
    displacement.z += frandombetween(-1000.0f,1000.0f);

    while (!cratesIsLocationGood(location))
    {
        vecAddTo(*location,displacement);
        //place check not to exceed game world edge?
        //wrap ..if hit edge...alter displacement value
    }

    pieMovePointClipToLimits(&centerMotherShips, location);  // CAT: the box clip (0x004e76b0, M2-R5 hud)

}



void cratesUpdate(void)
{
    Derelict *crate;
    Node *node;
    sdword numChecked;

    //only allow crates for multiplayer games
    if(singlePlayerGame)
        return;

    if (gatherStats|showStatsFight|showStatsFancyFight)
        return;

    if((tpGameCreated.flag & MG_CratesEnabled) == 0)
        return;

    universe.crateTimer+=universe.phystimeelapsed;
    if(universe.crateTimer >= CRATES_DoWeNeedToAddCratesCheckTime)
    {
        if (universe.numCratesInWorld < (CRATES_MaximumCratesInTheWorldPerPlayer*universe.numPlayers))
        {
            //time to do a crate addition check
            real32 prob;universe.crateTimer = 0.0f;
            prob = frandombetween(0.0f,1.0f);
            if(prob > CRATES_AddACrateProb)
            {
                cratesPlaceCrateSelectivly();
            }
        }
    }

    if(UNIVERSE_WOODPECKER(CRATES_ArePlayersNearUpdateMask, CRATES_ArePlayersNearUpdateFrame))
    {
        //time to do a crate nearity check ;)

        if(universe.numCratesInWorld > 0)
        {
            //lets do the distance checking for all crates;
            node = universe.DerelictList.head;
            numChecked = 0;
            while(node != NULL && numChecked < universe.numCratesInWorld)
            {
                crate = (Derelict *) listGetStructOfNode(node);

                if(crate->derelicttype == Crate)
                {
                    numChecked++;

                    //do time expiry checked here...
                    if((universe.totaltimeelapsed - crate->creationTime) > (CRATE_EXPIRY_TIME * UNIVERSE_UPDATE_RATE_FACTOR))
                    {
                        expireCrate(crate);
                    }
                    else
                    {
                        cratesScanForPlayersNearCrateAndReward(crate);
                    }
                }

                node = node->next;
            }

        }
    }
}
void expireCrate(Derelict *crate)
{
    etglod *etgLOD;
    etgeffectstatic *stat;
    vector zero = {0.0f,0.0f,0.0f};
    matrix ident = IdentityMatrix;

#ifdef HW_BUILD_FOR_DEBUGGING
    //don't drop below 0!
    dbgAssertOrIgnore(universe.numCratesInWorld >= 1);
#endif
    universe.numCratesInWorld--;

    //delete target
    // CAT: 3 arguments; the exe's expireCrate (inlined in cratesUpdate 0x00461e10, push 1 at 0x00461f70) passes 1
    AddTargetToDeleteList((SpaceObjRotImpTarg *)crate,-1,1);

    //play effect
    etgLOD = etgSpecialPurposeEffectTable[EGT_CRATE_TIME_OUT];

    if (etgLOD != NULL)
    {
        if (crate->obj.currentLOD >= etgLOD->nLevels)
        {
            stat = NULL;
        }
        else
        {
            stat = etgLOD->level[crate->obj.currentLOD];
        }
    }
    else
    {
        stat = NULL;
    }
    #if ETG_DISABLEABLE
    if (stat != NULL && etgEffectsEnabled)
    #else
    if (stat != NULL)
    #endif
    {
        etgEffectCreate(stat, NULL, &crate->obj.posinfo.position, &zero, &ident, 1.0f, EAF_AllButNLips, 0);
    }

}

void crateInit(void)
{
    sdword i;
    // CAT: the exe resets the first 0x2e = 46 entries of both tables (crateInit 0x00462080)
    for(i=0;i<0x2e;i++)
    {
        NUM_IN_GROUPS[i] = 1;
        SHIP_PROBS[i] = 0.0f;
    }

    scriptSet(NULL,"Crates.script",cratesScriptTable);

    NUM_IN_CLASSES[0] = 0;    //mothership not applicable
    NUM_IN_CLASSES[CLASS_HeavyCruiser] = sizeof(cratesCLASS_HeavyCruiser)/4;
    NUM_IN_CLASSES[CLASS_Carrier] = sizeof(cratesCLASS_Carrier)/4;
    NUM_IN_CLASSES[CLASS_Destroyer] = sizeof(cratesCLASS_Destroyer)/4;
    NUM_IN_CLASSES[CLASS_Frigate] = sizeof(cratesCLASS_Frigate)/4;
    NUM_IN_CLASSES[CLASS_Corvette] = sizeof(cratesCLASS_Corvette)/4;
    NUM_IN_CLASSES[CLASS_Fighter] = sizeof(cratesCLASS_Fighter)/4;
    NUM_IN_CLASSES[CLASS_Resource] = sizeof(cratesCLASS_Resource)/4;
    NUM_IN_CLASSES[CLASS_NonCombat] = sizeof(cratesCLASS_NonCombat)/4;




}

