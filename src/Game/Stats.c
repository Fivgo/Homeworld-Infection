/*=============================================================================
    Name    : Stats.c
    Purpose : Stats deals with collecting fighting stats for the various ships

    Created 4/1/1998 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "Stats.h"

#include <stdarg.h>
#include <string.h>

#include "CRC32.h"
#include "FastMath.h"
#include "File.h"
#include "Memory.h"
#include "NetCheck.h"
#include "ResearchAPI.h"
#include "SinglePlayer.h"
#include "Tweak.h"
#include "Randy.h"
#include "Universe.h"
#include "UnivUpdate.h"

#define STATLOG_FILENAME "statlog.txt"

#define CONSIDER_SHIPS_EVEN_PERC_HP_LEFT 0.15f

#define REFRESH     5

real32 statsOverkillfactor = 1.0f;
real32 statsOverkillfactorSqr = 1.0f;

TypeOfFormation showStatsFightF[2];
TacticsType showStatsFightT[2];
bool32 ShowFancyFights = FALSE;

static CommandToDo *attackcommands[2];

FightStats FightStatsTable[NUM_SHIPS_TO_GATHER_STATS_FOR][NUM_SHIPS_TO_GATHER_STATS_FOR];
// CAT: statfile.bin must be 98*98*28 = 0x41a70 bytes (statsLoadFightStats 0x00524a50)
_Static_assert(NUM_SHIPS_TO_GATHER_STATS_FOR == 98, "98 stat indices, sRecon..p1StandardCorvette");
_Static_assert(sizeof(FightStats) == 28, "FightStats is 28 bytes in statfile.bin");

FightStatsSum FightStatsColumnSum[NUM_SHIPS_TO_GATHER_STATS_FOR];
FightStatsSum FightStatsRowSum[NUM_SHIPS_TO_GATHER_STATS_FOR];

// CAT: stats are gathered for Cataclysm's first 5 races, Sect, Beast, Kushan, Taiidan and P1 (StatsForRaceCB 0x00523fb0
//      fills a 5x5 table; gatherstatsfor.script names them RACE_SECT..RACE_P1). The stat index of a ship is its type id
//      (StatIndexRaceOffsets 0x008961b8 = {0,33,60,74,92} = FirstShipTypeOfRace), 98 indices in all.
#define NUM_RACES_TO_GATHER_STATS_FOR     5

sdword StatIndexRaceOffsets[NUM_RACES_TO_GATHER_STATS_FOR] =
{
    SECT_FIRST_SHIP,
    BEAST_FIRST_SHIP,
    KUSHAN_FIRST_SHIP,
    TAIIDAN_FIRST_SHIP,
    P1_FIRST_SHIP
};

// CAT: initial values from the exe (0x008961cc); gatherstatsfor.script overrides them
static sbyte CalcFightStatsForRaces[NUM_RACES_TO_GATHER_STATS_FOR][NUM_RACES_TO_GATHER_STATS_FOR] =
{   // Sect  Beast  Kushan Taiidan P1
    { TRUE , TRUE , FALSE, FALSE, FALSE },    // Sect
    { TRUE , TRUE , FALSE, FALSE, FALSE },    // Beast
    { FALSE, FALSE, FALSE, FALSE, FALSE },    // Kushan
    { FALSE, FALSE, FALSE, FALSE, FALSE },    // Taiidan
    { FALSE, FALSE, FALSE, FALSE, FALSE },    // P1
};

// CAT: per-race "gather stats for this type" flags, initial values from the exe (0x008961e8, 0x0089620c, 0x00896228,
//      0x00896238, 0x0089624c); gatherstatsfor.script overrides them
static sbyte SectCalcFightStatsFor[SECT_LAST_SHIP - SECT_FIRST_SHIP + 1] =
{
    TRUE,   //sRecon
    TRUE,   //sAcolyte
    TRUE,   //sMimic
    TRUE,   //sLeech
    TRUE,   //sACV
    TRUE,   //sMCV
    TRUE,   //sRammingFrigate
    TRUE,   //sMultiBeamFrigate
    TRUE,   //sHiveFrigate
    TRUE,   //sSwarmer
    TRUE,   //sDreadnought
    FALSE,  //sDreadnoughtCannon
    TRUE,   //sCarrier
    FALSE,  //sCarrierSupport
    FALSE,  //sWorker
    FALSE,  //sRepairDroid
    FALSE,  //sProcessor
    TRUE,   //sSentinel
    TRUE,   //sMothership
    FALSE,  //sMothershipSpecial
    FALSE,  //sMothershipEngineering
    FALSE,  //sMothershipArmour
    FALSE,  //sMothershipMicro
    FALSE,  //sMothershipWeapons
    FALSE,  //sMothershipDockingBay
    TRUE,   //sDestroyer
    FALSE,  //sMothershipSupport
    FALSE,  //sMothershipCommand
    FALSE,  //sMothershipEnginesLower
    FALSE,  //sMothershipEnginesUpper
    FALSE,  //sMothershipLowerSpine
    FALSE,  //sMothershipBigGun
    FALSE   //sMothershipOreCanArmour
};

static sbyte BeastCalcFightStatsFor[BEAST_LAST_SHIP - BEAST_FIRST_SHIP + 1] =
{
    TRUE,   //bRecon
    TRUE,   //bAcolyte
    TRUE,   //bInterceptor
    TRUE,   //bCloakedFighter
    TRUE,   //bAttackBomber
    TRUE,   //bACV
    TRUE,   //bMultiGunCorvette
    TRUE,   //bHeavyCorvette
    TRUE,   //bMultiBeamFrigate
    TRUE,   //bHiveFrigate
    TRUE,   //bSwarmer
    TRUE,   //bIonArrayFrigate
    FALSE,  //bDFGFrigate
    TRUE,   //bHeavyCruiser
    TRUE,   //bCarrier
    TRUE,   //bRammingFrigate
    FALSE,  //bWorker
    FALSE,  //bRepairDroid
    FALSE,  //bProcessor
    TRUE,   //bSentinel
    TRUE,   //bMothership
    FALSE,  //bMothershipDockingBay
    FALSE,  //bMothershipEnginesLower
    FALSE,  //bSectMoship
    FALSE,  //bMothershipSupport
    TRUE,   //bCruiseMissile
    TRUE    //bMissileCorvette
};

static sbyte KushanCalcFightStatsFor[KUSHAN_LAST_SHIP - KUSHAN_FIRST_SHIP + 1] =
{
    TRUE,   //kAdvanceSupportFrigate
    TRUE,   //kAttackBomber
    TRUE,   //kCarrier
    FALSE,  //kCloakGenerator
    FALSE,  //kGravWellGenerator
    TRUE,   //kHeavyCorvette
    TRUE,   //kHeavyCruiser
    TRUE,   //kHeavyDefender
    TRUE,   //kHeavyInterceptor
    TRUE,   //kIonCannonFrigate
    TRUE,   //kMultiGunCorvette
    FALSE,  //kProximitySensor
    FALSE,  //kSalCapCorvette
    TRUE    //kStandardDestroyer
};

static sbyte TaiidanCalcFightStatsFor[TAIIDAN_LAST_SHIP - TAIIDAN_FIRST_SHIP + 1] =
{
    FALSE,  //tAdvanceSupportFrigate
    TRUE,   //tAttackBomber
    TRUE,   //tCarrier
    FALSE,  //tCloakGenerator
    FALSE,  //tDFGFrigate
    FALSE,  //tGravWellGenerator
    TRUE,   //tHeavyCruiser
    TRUE,   //tHeavyDefender
    TRUE,   //tHeavyInterceptor
    TRUE,   //tIonCannonFrigate
    TRUE,   //tMinelayerCorvette
    TRUE,   //tMissileDestroyer
    TRUE,   //tMultiGunCorvette
    FALSE,  //tProximitySensor
    FALSE,  //tResourceCollector
    FALSE,  //tSalCapCorvette
    TRUE,   //tStandardDestroyer
    TRUE    //tStandardFrigate
};

static sbyte P1CalcFightStatsFor[P1_LAST_SHIP - P1_FIRST_SHIP + 1] =
{
    TRUE,   //p1Outpost
    TRUE,   //p1Fighter
    TRUE,   //p1IonArrayFrigate
    TRUE,   //p1MissileCorvette
    TRUE,   //p1Mothership
    TRUE    //p1StandardCorvette
};

static sbyte *RaceCalcFightStatsFor[NUM_RACES_TO_GATHER_STATS_FOR] =
{
    SectCalcFightStatsFor,
    BeastCalcFightStatsFor,
    KushanCalcFightStatsFor,
    TaiidanCalcFightStatsFor,
    P1CalcFightStatsFor
};

bool32 ForceTotalRefresh = FALSE;

static void StatsForRaceCB(char *directory,char *field,void *dataToFillIn);
static void StatsForCB(char *directory,char *field,void *dataToFillIn);

scriptEntry GatherStatsScriptTable[] =
{
    makeEntry(ForceTotalRefresh,scriptSetBool),

    {"StatsForRace", StatsForRaceCB, NULL},
    {"StatsFor",     StatsForCB, NULL},
    
    END_SCRIPT_ENTRY
};

static void StatsForRaceCB(char *directory,char *field,void *dataToFillIn)
{
    char *param;
    ShipRace race;
    sbyte val;
    sdword i;

    param = strtok(field, ", \t");
    dbgAssertOrIgnore(param);
    race = StrToShipRace(param);
    dbgAssertOrIgnore(race >= 0 && race < NUM_RACES_TO_GATHER_STATS_FOR);

    for (i=0;i<NUM_RACES_TO_GATHER_STATS_FOR;i++)
    {
        param = strtok(NULL, ", \t");
        val = FALSE;
        if (param)
        {
            if (strncmp(param,"TRUE",4) == 0)
            {
                val = TRUE;
            }
            else if (strncmp(param,"REFRESH",7) == 0)
            {
                val = REFRESH;
            }
        }

        CalcFightStatsForRaces[race][i] = val;
    }
}

static void StatsForCB(char *directory,char *field,void *dataToFillIn)
{
    char racestr[15];
    char shipstr[50];
    char truestr[15];
    ShipRace race;
    ShipType shiptype;
    sbyte val;

    RemoveCommasFromString(field);

    sscanf(field,"%s %s %s",racestr,shipstr,truestr);

    race = StrToShipRace(racestr);
    dbgAssertOrIgnore(race >= 0 && race < NUM_RACES_TO_GATHER_STATS_FOR);

    shiptype = StrToShipType(shipstr);
    dbgAssertOrIgnore(shiptype >= 0 && shiptype < TOTAL_NUM_SHIPS);

    val = FALSE;
    if (strncmp(truestr,"TRUE",4) == 0)
    {
        val = TRUE;
    }
    else if (strncmp(truestr,"REFRESH",7) == 0)
    {
        val = REFRESH;
    }

    RaceCalcFightStatsFor[race][shiptype - FirstShipTypeOfRace[race]] = val;
}

void statLog(char *format, ...)
{
    char buffer[200];
    va_list argList;
    va_start(argList, format);                              //get first arg
    vsprintf(buffer, format, argList);                      //prepare output string
    va_end(argList);

    dbgMessage("STATLOG:");
    logfileLog(STATLOG_FILENAME,buffer);
    dbgMessage(buffer);
}


sdword ConvertShipRaceTypeToStatIndex(ShipType shiptype,ShipRace shiprace)
{
    sdword offset;

    if ((shiprace < 0) || (shiprace >= NUM_RACES_TO_GATHER_STATS_FOR))
    {
        return -1;
    }

    if ((shiptype < 0) || (shiptype >= TOTAL_NUM_SHIPS))
    {
        return -1;
    }

    offset = shiptype - FirstShipTypeOfRace[shiprace];

    if ((offset < 0) || (offset >= NumShipTypesInRace[shiprace]))
    {
        return -1;
    }

    return (offset + StatIndexRaceOffsets[shiprace]);
}

void ConvertStatIndexToShipRaceType(sdword index,ShipType *shiptype,ShipRace *shiprace)
{
    sdword j;
    sdword offset;

    for (j=NUM_RACES_TO_GATHER_STATS_FOR-1;j>=0;j--)
    {
        if (index >= StatIndexRaceOffsets[j])
        {
            offset = index - StatIndexRaceOffsets[j];
            dbgAssertOrIgnore(offset >= 0);
            dbgAssertOrIgnore(offset < NumShipTypesInRace[j]);

            *shiprace = j;
            *shiptype = (ShipType) (offset + FirstShipTypeOfRace[j]);
            return;
        }
    }

    dbgFatalf(DBG_Loc,"Bad stats index %d",index);
}

ShipStaticInfo *ConvertStatIndexToShipStatic(sdword i)
{
    ShipType shiptype = ShipType_Uninitialized;
    ShipRace shiprace = R1;
    ShipStaticInfo *shipstatic = NULL;

    ConvertStatIndexToShipRaceType(i,&shiptype,&shiprace);
    shipstatic = GetShipStaticInfo(shiptype,shiprace);
    if (bitTest(shipstatic->common.infoFlags, IF_InfoLoaded))
    {
        return shipstatic;
    }

    return NULL;
}

void SetupShipsForFight(FightStats *fightStats)
{
    sdword i,j;
    SelectCommand *selection[2];
    sdword numShips;
    vector position[2];
    real32 facerotation[2] = { DEG_TO_RAD(0.0f), DEG_TO_RAD(180.0f) };
    Ship *ship;

    vecSet(position[0],0.0f,0.0f,0.0f);
    vecSet(position[1],FACEOFF_DISTANCE,0.0f,0.0f);

    unitCapDisable();

    for (i=0;i<2;i++)
    {
        numShips = (sdword)fightStats->numShips[i];
        dbgAssertOrIgnore(numShips > 0);
        selection[i] = memAlloc(sizeofSelectCommand(numShips),"fightselection",0);

        selection[i]->numShips = numShips;

        for (j=0;j<numShips;j++)
        {
            selection[i]->ShipPtr[j] = ship = univAddShip(fightStats->shiptype[i],fightStats->shiprace[i],&position[i],&universe.players[i],0);
            univRotateObjYaw((SpaceObjRot *)ship,facerotation[i]);
        }

        if (showStatsFancyFight)
        {
            if ((showStatsFightF[i] >= 0) && (showStatsFightF[i] < NO_FORMATION)) clFormation(&universe.mainCommandLayer,selection[i],showStatsFightF[i],0);   // CAT: level 0 [audit: M2-R5 command-tactics]
            clSetTactics(&universe.mainCommandLayer,selection[i],showStatsFightT[i]);
        }
        else
        {
            clFormation(&universe.mainCommandLayer,selection[i],BROAD_FORMATION,0);   // CAT: level 0 (SetupShipsForFight 0x00524244)
        }
    }

    clPresetShipsToPosition(&universe.mainCommandLayer);

    universeSwitchToPlayer(0);

    if (AreAllShipsAttackCapable(selection[0]))
    {
        attackcommands[0] = clAttackThese(&universe.mainCommandLayer,selection[0],(AttackCommand *)selection[1]);
        dbgAssertOrIgnore(attackcommands[0] != NULL);
    }
    else
    {
        statLog("Warning: Ships %s %s could not attack",ShipRaceToStr(fightStats->shiprace[0]),ShipTypeToStr(fightStats->shiptype[0]));
    }

    if (AreAllShipsAttackCapable(selection[1]))
    {
        attackcommands[1] = clAttackThese(&universe.mainCommandLayer,selection[1],(AttackCommand *)selection[0]);
        dbgAssertOrIgnore(attackcommands[1] != NULL);
    }
    else
    {
        statLog("Warning: Ships %s %s could not attack",ShipRaceToStr(fightStats->shiprace[1]),ShipTypeToStr(fightStats->shiptype[1]));
    }

    memFree(selection[0]);
    memFree(selection[1]);
}

bool32 AreShipsDoneFighting()
{
    // keep going until an attack is finished
    if (!CommandInCommandLayer(&universe.mainCommandLayer,attackcommands[0]))
    {
        return TRUE;
    }

    if (!CommandInCommandLayer(&universe.mainCommandLayer,attackcommands[1]))
    {
        return TRUE;
    }

    // or the time expires
    if (universe.totaltimeelapsed >= STATFIGHT_TIMEOUT_TIME)
    {
        return TRUE;
    }

    return FALSE;
}

void HaveShipsFight()
{
    universe.totaltimeelapsed = 0.0f;
    universe.univUpdateCounter = 0;

    for (;;)
    {
        univUpdate(UNIVERSE_UPDATE_PERIOD);

        if (AreShipsDoneFighting())
        {
            break;
        }
    }
}

void CalculateRUKillRatios(FightStats *fightStats)
{
    sdword i;
    ShipStaticInfo *shipstatic[2];

    for (i=0;i<2;i++)
    {
        shipstatic[i] = GetShipStaticInfoValidate(fightStats->shiptype[i],fightStats->shiprace[i]);
    }

    fightStats->fracKillratio = ((real32)fightStats->numShips[1]) / ((real32)fightStats->numShips[0]);
    fightStats->fracRUratio = ((real32)(fightStats->numShips[1] * shipstatic[1]->buildCost)) /
                              ((real32)(fightStats->numShips[0] * shipstatic[0]->buildCost));

    statLog(" >%d Vs %d RUratio Killratio %3.3f %3.3f\n",fightStats->numShips[0],fightStats->numShips[1],fightStats->fracRUratio,fightStats->fracKillratio);
}


void GetPostCombatStats(FightStats *fightStats)
{
    Node *curnode = universe.mainCommandLayer.todolist.head;
    CommandToDo *command;
    SelectCommand *selection;
    sdword numShips;
    sdword i;
    uword playerindex;

    fightStats->numShipsAfter[0] = 0;
    fightStats->numShipsAfter[1] = 0;
    fightStats->totalHPAfter[0] = 0;
    fightStats->totalHPAfter[1] = 0;

    // Go through command layer, and see what ships are left:

    while (curnode != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);
        if ((command == attackcommands[0]) || (command == attackcommands[1]))
        {
            selection = command->selection;
            numShips = selection->numShips;
            dbgAssertOrIgnore(numShips > 0);
            playerindex = selection->ShipPtr[0]->playerowner->playerIndex;
            dbgAssertOrIgnore(playerindex < 2);
            fightStats->numShipsAfter[playerindex] = (uword)numShips;
            for (i=0;i<numShips;i++)
            {
                fightStats->totalHPAfter[playerindex] += selection->ShipPtr[i]->targetable.health;
            }
        }

        curnode = curnode->next;
    }

    fightStats->battleTime = universe.totaltimeelapsed;

    universeReset();
    univupdateReset();
    singlePlayerInit();     // for hyperspace
//    clReset(&universe.mainCommandLayer);
//    ccReset(&universe.mainCameraCommand);
//    univupdateCloseAllObjectsAndMissionSpheres();

//    universe.totaltimeelapsed = 0.0f;
//    universe.univUpdateCounter = 0;
}

void GatherFightStatsFor(sdword i,sdword j,bool32 actuallyDoFight)
{
    FightStats *fightStats = &FightStatsTable[i][j];
    sdword *binsearchtable = NULL;
    sdword bottom,top;
    sdword trynum = 0;
    sdword search,found,verytop;
    sdword powerfulship,nonpowerfulship;
    ShipType shiptype = ShipType_Uninitialized;
    ShipRace shiprace = R1;

    ConvertStatIndexToShipRaceType(i,&shiptype,&shiprace);
    fightStats->shiptype[0] = shiptype;
    fightStats->shiprace[0] = shiprace;
    ConvertStatIndexToShipRaceType(j,&shiptype,&shiprace);
    fightStats->shiptype[1] = shiptype;
    fightStats->shiprace[1] = shiprace;

    if (!actuallyDoFight)
    {
        // later handle this case
    }

    statLog("STATS for %s %s Vs %s %s\n",ShipRaceToStr(fightStats->shiprace[0]),ShipTypeToStr(fightStats->shiptype[0]),ShipRaceToStr(fightStats->shiprace[1]),ShipTypeToStr(fightStats->shiptype[1]));

    // Have first fight 1 on 1, to determine who is more powerful:
    fightStats->numShips[0] = 1;
    fightStats->numShips[1] = 1;

    if (i == j)
    {
        CalculateRUKillRatios(fightStats);
        return;
    }

    SetupShipsForFight(fightStats);
    HaveShipsFight();
    GetPostCombatStats(fightStats);

    if ((fightStats->numShipsAfter[0] | fightStats->numShipsAfter[1]) == 0)
    {
        // simultaneous kill:
        CalculateRUKillRatios(fightStats);
        statLog(" Simultaneous kill\n");
        return;
    }

    if ((fightStats->numShipsAfter[0] == 1) && (fightStats->numShipsAfter[1] == 1))
    {
        // neither ship could kill the other ship:
        fightStats->fracRUratio = 0.0f;
        fightStats->fracKillratio = 0.0f;
        statLog(" Stalemate\n");
        return;
    }

    if (fightStats->numShipsAfter[0] == 1)
    {
        // ship 0 is more powerful

        real32 hpleftperc = ((real32)fightStats->totalHPAfter[0]) /
                            ((real32)GetShipStaticInfoValidate(fightStats->shiptype[0],fightStats->shiprace[0])->health.maxhealth);

        if (hpleftperc < CONSIDER_SHIPS_EVEN_PERC_HP_LEFT)
        {
            // ship 0 isn't that much more powerful.  Consider these ships even
            CalculateRUKillRatios(fightStats);
            statLog(" Ships roughly equal 0\n");
            return;
        }

        powerfulship = 0;

    }
    else
    {
        // ship 1 is more powerful

        real32 hpleftperc = ((real32)fightStats->totalHPAfter[1]) /
                            ((real32)GetShipStaticInfoValidate(fightStats->shiptype[1],fightStats->shiprace[1])->health.maxhealth);

        if (hpleftperc < CONSIDER_SHIPS_EVEN_PERC_HP_LEFT)
        {
            // ship 1 isn't that much more powerful.  Consider these ships even
            CalculateRUKillRatios(fightStats);
            statLog(" Ships roughly equal 1\n");
            return;
        }

        powerfulship = 1;
    }
    nonpowerfulship = 1-powerfulship;

    bottom = 1;
    top = cdLimitCaps[fightStats->shiptype[nonpowerfulship]];
    if (top == -1)
    {
        top = cdClassCaps[GetShipStaticInfoValidate(fightStats->shiptype[nonpowerfulship],fightStats->shiprace[nonpowerfulship])->shipclass];
    }
    if (top >= MAX_SHIPS_TO_EVER_CONSIDER)
    {
        top = MAX_SHIPS_TO_EVER_CONSIDER;
    }
    dbgAssertOrIgnore(top >= 1);
    verytop = top;

    binsearchtable = memAlloc(sizeof(udword)*(top+1),"binsearchtable",0);
    memset(binsearchtable,0,sizeof(udword)*(top+1));

    while (top >= bottom)
    {
        trynum = (bottom+top)>>1;

        if (binsearchtable[trynum])
        {
            statLog(" Cache hit\n");

            // decision in cache
            if (binsearchtable[trynum] < 0)
            {
                // trynum is too small
                bottom = trynum+1;
            }
            else
            {
                // trynum is too big
                top = trynum-1;
            }
        }
        else
        {
            // decision not in cache, so execute fight
            fightStats->numShips[powerfulship] = 1;
            fightStats->numShips[nonpowerfulship] = (uword)trynum;

            statLog(" Binsearch %d %d\n",fightStats->numShips[0],fightStats->numShips[1]);

            SetupShipsForFight(fightStats);
            HaveShipsFight();
            GetPostCombatStats(fightStats);

            if (fightStats->numShipsAfter[powerfulship] == 1)
            {
                // powerful ship hasn't been defeated.  Need more ships.
                binsearchtable[trynum] = -1;
                // trynum is too small
                bottom = trynum+1;
            }
            else
            {
                binsearchtable[trynum] = 1;
                // trynum is too big
                top = trynum-1;
            }
        }
    }

    for (found=-1,search=0;search<=verytop;search++)
    {
        if (binsearchtable[search] > 0)
        {
            // lowest number of ships needed to kill
            found = search;
            break;
        }
    }

    memFree(binsearchtable);

    // at this point trynum contains # of ships needed to kill powerfulship

    fightStats->numShips[powerfulship] = 1;
    fightStats->numShips[nonpowerfulship] = (found >= 0) ? found : trynum;

    CalculateRUKillRatios(fightStats);
}

#define tswap(a,b,abtype) \
{                         \
    abtype typetemp;      \
    typetemp = a;         \
    a = b;                \
    b = typetemp;         \
}

void ReciprocateFightStats(FightStats *fightStats)
{
    tswap(fightStats->shiptype[0],fightStats->shiptype[1],ShipType)
    tswap(fightStats->shiprace[0],fightStats->shiprace[1],ShipRace)
    tswap(fightStats->numShips[0],fightStats->numShips[1],sword)
    tswap(fightStats->numShipsAfter[0],fightStats->numShipsAfter[1],sword)
    tswap(fightStats->totalHPAfter[0],fightStats->totalHPAfter[1],real32)
//    tswap(fightStats->numFracShipsAfter[0],fightStats->numFracShipsAfter[1],sword)
    if (fightStats->fracRUratio != 0.0f)
    {
        fightStats->fracRUratio = 1.0f / fightStats->fracRUratio;
    }
    if (fightStats->fracKillratio != 0.0f)
    {
        fightStats->fracKillratio = 1.0f / fightStats->fracKillratio;
    }
}

/*=============================================================================
    FancyFight Stuff
=============================================================================*/

typedef struct {
    sdword repeat;
    sbyte statindex[2];
    sbyte num[2];
    TypeOfFormation formation[2];
    TacticsType tactics[2];
} FancyFightEntry;

#define MAX_NUM_FANCYFIGHTENTRYS 100

sdword numFancyFightEntrys = 0;
FancyFightEntry fancyFightEntry[MAX_NUM_FANCYFIGHTENTRYS];

sdword currentFancyFightEntry = 0;
sdword currentFancyFightRepeatFight = 0;
bool32 ShowFancyFightsShipsFighting = FALSE;

void SetFlightEntrysCB(char *directory,char *field,void *dataToFillIn);
void SetFlightEntrysPreLoadCB(char *directory,char *field,void *dataToFillIn);

void SetInfoNeededForShipAndRelatedStaticInfo(ShipType type,ShipRace race,bool8 dataToFillIn);
void universeFlagNothingNeeded(void);

scriptEntry FancyFightScriptTable[] =
{
    makeEntry(ShowFancyFights,scriptSetBool),

    { "DoStatsFor", SetFlightEntrysCB, NULL },

    END_SCRIPT_ENTRY
};

scriptEntry FancyFightPreLoadScriptTable[] =
{
    { "DoStatsFor", SetFlightEntrysPreLoadCB, NULL },

    END_SCRIPT_ENTRY
};

void SetFlightEntrysCB(char *directory,char *field,void *dataToFillIn)
{
    sdword repeat;
    sdword num[2];
    char racestr[2][20];
    char shipstr[2][50];
    char formationstr[2][50];
    char tacticsstr[2][50];

    RemoveCommasFromString(field);

    sscanf(field,"%d %d %s %s %s %s %d %s %s %s %s",&repeat,&num[0],racestr[0],shipstr[0],formationstr[0],tacticsstr[0],
                                                            &num[1],racestr[1],shipstr[1],formationstr[1],tacticsstr[1]);

    dbgAssertOrIgnore(numFancyFightEntrys < MAX_NUM_FANCYFIGHTENTRYS);

    fancyFightEntry[numFancyFightEntrys].repeat = repeat;
    fancyFightEntry[numFancyFightEntrys].statindex[0] = ConvertShipRaceTypeToStatIndex(StrToShipType(shipstr[0]),StrToShipRace(racestr[0]));
    fancyFightEntry[numFancyFightEntrys].statindex[1] = ConvertShipRaceTypeToStatIndex(StrToShipType(shipstr[1]),StrToShipRace(racestr[1]));
    fancyFightEntry[numFancyFightEntrys].num[0] = num[0];
    fancyFightEntry[numFancyFightEntrys].num[1] = num[1];
    fancyFightEntry[numFancyFightEntrys].formation[0] = StrToTypeOfFormation(formationstr[0]);
    fancyFightEntry[numFancyFightEntrys].formation[1] = StrToTypeOfFormation(formationstr[1]);
    fancyFightEntry[numFancyFightEntrys].tactics[0] = StrToTacticsType(tacticsstr[0]);
    fancyFightEntry[numFancyFightEntrys].tactics[1] = StrToTacticsType(tacticsstr[1]);
    numFancyFightEntrys++;
}

void SetFlightEntrysPreLoadCB(char *directory,char *field,void *dataToFillIn)
{
    sdword repeat;
    sdword num[2];
    char racestr[2][20];
    char shipstr[2][50];
    char formationstr[2][50];
    char tacticsstr[2][50];

    RemoveCommasFromString(field);

    sscanf(field,"%d %d %s %s %s %s %d %s %s %s %s",&repeat,&num[0],racestr[0],shipstr[0],formationstr[0],tacticsstr[0],
                                                            &num[1],racestr[1],shipstr[1],formationstr[1],tacticsstr[1]);

    SetInfoNeededForShipAndRelatedStaticInfo(StrToShipType(shipstr[0]),StrToShipRace(racestr[0]),TRUE);
    SetInfoNeededForShipAndRelatedStaticInfo(StrToShipType(shipstr[1]),StrToShipRace(racestr[1]),TRUE);
}

void FancyFightPreLoad(void)
{
    universeFlagNothingNeeded();
    scriptSet(NULL,showStatsFancyFightScriptFile,FancyFightPreLoadScriptTable);
}

void FancyFightPrepareFightStats(FightStats *fightStats)
{
    FancyFightEntry *ffe = &fancyFightEntry[currentFancyFightEntry];
    ShipType shiptype = ShipType_Uninitialized;
    ShipRace shiprace = R1;
    sdword i = ffe->statindex[0];
    sdword j = ffe->statindex[1];
    dbgAssertOrIgnore(i >= 0);
    dbgAssertOrIgnore(i < NUM_SHIPS_TO_GATHER_STATS_FOR);
    dbgAssertOrIgnore(j >= 0);
    dbgAssertOrIgnore(j < NUM_SHIPS_TO_GATHER_STATS_FOR);

    ConvertStatIndexToShipRaceType(i,&shiptype,&shiprace);
    fightStats->shiptype[0] = shiptype;
    fightStats->shiprace[0] = shiprace;
    ConvertStatIndexToShipRaceType(j,&shiptype,&shiprace);
    fightStats->shiptype[1] = shiptype;
    fightStats->shiprace[1] = shiprace;

    fightStats->numShips[0] = ffe->num[0];
    fightStats->numShips[1] = ffe->num[1];

    showStatsFightF[0] = ffe->formation[0];
    showStatsFightF[1] = ffe->formation[1];
    showStatsFightT[0] = ffe->tactics[0];
    showStatsFightT[1] = ffe->tactics[1];
}

void PrintFancyFightStats(FightStats *fightStats)
{
    statLog("STATS for %s %s Vs %s %s\n",ShipRaceToStr(fightStats->shiprace[0]),ShipTypeToStr(fightStats->shiptype[0]),ShipRaceToStr(fightStats->shiprace[1]),ShipTypeToStr(fightStats->shiptype[1]));
    statLog("Before: %3d %3d\n",fightStats->numShips[0],fightStats->numShips[1]);
    statLog("After : %3d %3d\n",fightStats->numShipsAfter[0],fightStats->numShipsAfter[1]);
}

void statsShowFancyFight(sdword i,sdword j)
{
    FightStats fightStats;

    // initialize fancyfight stats
    ShowFancyFights = FALSE;        // set defaults
    numFancyFightEntrys = 0;
    currentFancyFightEntry = 0;
    currentFancyFightRepeatFight = 0;
    scriptSet(NULL,showStatsFancyFightScriptFile,FancyFightScriptTable);

    logfileClear(STATLOG_FILENAME);
    statLog("GATHERING FANCYFIGHT STATS\n");

    dbgAssertOrIgnore(universe.numPlayers >= 2);    // need at least 2 players to gather stats

    if (!ShowFancyFights)
    {
        for (currentFancyFightEntry=0;currentFancyFightEntry<numFancyFightEntrys;currentFancyFightEntry++)
        {
            sdword numRepeat = fancyFightEntry[currentFancyFightEntry].repeat;
            for (currentFancyFightRepeatFight=0;currentFancyFightRepeatFight<numRepeat;currentFancyFightRepeatFight++)
            {
                FancyFightPrepareFightStats(&fightStats);
                SetupShipsForFight(&fightStats);
                HaveShipsFight();
                GetPostCombatStats(&fightStats);
                PrintFancyFightStats(&fightStats);
            }
        }
    }
    else
    {
        currentFancyFightEntry = 0;
        currentFancyFightRepeatFight = 0;
        dbgAssertOrIgnore(currentFancyFightEntry < numFancyFightEntrys);
        dbgAssertOrIgnore(currentFancyFightRepeatFight < fancyFightEntry[currentFancyFightEntry].repeat);
        ShowFancyFightsShipsFighting = FALSE;
    }
}

void statsShowFancyFightUpdate(void)
{
    static FightStats tempFightStats;

    dbgAssertOrIgnore(ShowFancyFights);

    if (!ShowFancyFightsShipsFighting)
    {
        FancyFightPrepareFightStats(&tempFightStats);
        SetupShipsForFight(&tempFightStats);

        universe.totaltimeelapsed = 0.0f;
        universe.univUpdateCounter = 0;

        ShowFancyFightsShipsFighting = TRUE;
    }
    else
    {
        if (AreShipsDoneFighting())
        {
            GetPostCombatStats(&tempFightStats);
            PrintFancyFightStats(&tempFightStats);
            ShowFancyFightsShipsFighting = FALSE;

            currentFancyFightRepeatFight++;
            if (currentFancyFightRepeatFight >= fancyFightEntry[currentFancyFightEntry].repeat)
            {
                currentFancyFightRepeatFight = 0;

                currentFancyFightEntry++;
                if (currentFancyFightEntry >= numFancyFightEntrys)
                {
                    // all done!
                    ShowFancyFights = FALSE;
                }
            }
        }
    }
}

void statsShowFight(sdword i,sdword j)
{
    FightStats *fightStats;
    dbgAssertOrIgnore(i >= 0);
    dbgAssertOrIgnore(i < NUM_SHIPS_TO_GATHER_STATS_FOR);
    dbgAssertOrIgnore(j >= 0);
    dbgAssertOrIgnore(j < NUM_SHIPS_TO_GATHER_STATS_FOR);

    dbgAssertOrIgnore(universe.numPlayers >= 2);    // need at least 2 players to gather stats

    fightStats = &FightStatsTable[i][j];
    if ((fightStats->numShips[0] > 0) && (fightStats->numShips[1] > 0))
    {
        SetupShipsForFight(fightStats);
    }
    else
    {
        fightStats->numShips[0] = 1;
        fightStats->numShips[1] = 1;
        SetupShipsForFight(fightStats);
    }
}

void CalculateOverallSums(void)
{
    sdword i,j;

    for (i=0;i<NUM_SHIPS_TO_GATHER_STATS_FOR;i++)
    {
        FightStatsColumnSum[i].RUratio = 0.0f;
        FightStatsColumnSum[i].Killratio = 0.0f;

        for (j=0;j<NUM_SHIPS_TO_GATHER_STATS_FOR;j++)
        {
            if (FightStatsCalculated(FightStatsTable[i][j]))
            {
                FightStatsColumnSum[i].RUratio += FightStatsTable[i][j].fracRUratio;
                FightStatsColumnSum[i].Killratio += FightStatsTable[i][j].fracKillratio;
            }
        }
    }

    for (j=0;j<NUM_SHIPS_TO_GATHER_STATS_FOR;j++)
    {
        FightStatsRowSum[j].RUratio = 0.0f;
        FightStatsRowSum[j].Killratio = 0.0f;

        for (i=0;i<NUM_SHIPS_TO_GATHER_STATS_FOR;i++)
        {
            if (FightStatsCalculated(FightStatsTable[i][j]))
            {
                FightStatsRowSum[j].RUratio += FightStatsTable[i][j].fracRUratio;
                FightStatsRowSum[j].Killratio += FightStatsTable[i][j].fracKillratio;
            }
        }
    }
}

void GatherFightStatsForRaces(ShipRace racei,ShipRace racej)
{
    sdword i,j;
    sdword indexi,indexj;
    sdword raceiNumShips = NumShipTypesInRace[racei];
    sbyte *raceiCalcFightStatsFor = RaceCalcFightStatsFor[racei];
    sdword raceiStatIndexOffset = StatIndexRaceOffsets[racei];
    sdword racejNumShips = NumShipTypesInRace[racej];
    sbyte *racejCalcFightStatsFor = RaceCalcFightStatsFor[racej];
    sdword racejStatIndexOffset = StatIndexRaceOffsets[racej];
#if MEM_ERROR_CHECKING
    sdword memoryFree = memFreeMemGet(&memMainPool);
    sdword memoryLeak;
#endif

    for (i=0;i<raceiNumShips;i++)
    {
        if (raceiCalcFightStatsFor[i])
        {
            for (j=0;j<racejNumShips;j++)
            {
                if (racejCalcFightStatsFor[j])
                {
                    indexi = i+raceiStatIndexOffset;
                    indexj = j+racejStatIndexOffset;
                    if (!FightStatsTable[indexi][indexj].numShips[0])       // check to make sure havent already calculated
                    {
                        GatherFightStatsFor(indexi,indexj,TRUE);
                        dbgAssertOrIgnore(FightStatsTable[indexi][indexj].numShips[0]);
#if MEM_ERROR_CHECKING
                        if (memoryFree != memFreeMemGet(&memMainPool))
                        {
                            memoryLeak = memFreeMemGet(&memMainPool) - memoryFree;
                            memAnalysisCreate();
                            dbgAssertOrIgnore(FALSE);
                        }
#endif
                        if (indexi != indexj)
                        {
                            FightStatsTable[indexj][indexi] = FightStatsTable[indexi][indexj];
                            ReciprocateFightStats(&FightStatsTable[indexj][indexi]);
                        }
                    }
                }
            }
        }
    }
}

void RefreshClearFightStatsTable(void)
{
    ShipRace racei,racej;
    sdword i,j;
    sdword indexi,indexj;

    // for any race vs race sets to refresh, clear them so they get refreshed:

    for (racei=0;racei<NUM_RACES_TO_GATHER_STATS_FOR;racei++)
    {
        for (racej=0;racej<NUM_RACES_TO_GATHER_STATS_FOR;racej++)
        {
            sdword raceiNumShips = NumShipTypesInRace[racei];
            sbyte *raceiCalcFightStatsFor = RaceCalcFightStatsFor[racei];
            sdword raceiStatIndexOffset = StatIndexRaceOffsets[racei];

            sdword racejNumShips = NumShipTypesInRace[racej];
            sbyte *racejCalcFightStatsFor = RaceCalcFightStatsFor[racej];
            sdword racejStatIndexOffset = StatIndexRaceOffsets[racej];

            if (CalcFightStatsForRaces[racei][racej] == REFRESH)
            {
                // force all occurances of racei/racej to 0
                for (i=0;i<raceiNumShips;i++)
                {
                    for (j=0;j<racejNumShips;j++)
                    {
                        indexi = i+raceiStatIndexOffset;
                        indexj = j+racejStatIndexOffset;
                        memset(&FightStatsTable[indexi][indexj],0,sizeof(FightStats));
                    }
                }
            }
            else
            {
                // just force ships with refresh set to 0
                for (i=0;i<raceiNumShips;i++)
                {
                    if (raceiCalcFightStatsFor[i] == REFRESH)
                    {
                        for (j=0;j<racejNumShips;j++)           // force all intersections with it to 0
                        {
                            if (racejCalcFightStatsFor[j])
                            {
                                indexi = i+raceiStatIndexOffset;
                                indexj = j+racejStatIndexOffset;
                                memset(&FightStatsTable[indexi][indexj],0,sizeof(FightStats));
                            }
                        }
                    }
                }

                for (j=0;j<racejNumShips;j++)
                {
                    if (racejCalcFightStatsFor[j] == REFRESH)
                    {
                        for (i=0;i<raceiNumShips;i++)           // force all intersections with it to 0
                        {
                            if (raceiCalcFightStatsFor[i])
                            {
                                indexi = i+raceiStatIndexOffset;
                                indexj = j+racejStatIndexOffset;
                                memset(&FightStatsTable[indexi][indexj],0,sizeof(FightStats));
                            }
                        }
                    }
                }
            }
        }
    }
}

void statsGatherFightStats(void)
{
    ShipRace racei,racej;
    sdword statfilesize = NUM_SHIPS_TO_GATHER_STATS_FOR * NUM_SHIPS_TO_GATHER_STATS_FOR * sizeof(FightStats);

    dbgAssertOrIgnore(universe.numPlayers >= 2);    // need at least 2 players to gather stats

    logfileClear(STATLOG_FILENAME);

    if ((ForceTotalRefresh) || (statfilesize != fileSizeGet("statfile.bin",0)))
    {
        memset(&FightStatsTable[0][0],0,statfilesize);
    }
    else
    {
        fileLoad("statfile.bin",&FightStatsTable[0][0],0);
        RefreshClearFightStatsTable();
    }

    for (racei=0;racei<NUM_RACES_TO_GATHER_STATS_FOR;racei++)
    {
        for (racej=0;racej<NUM_RACES_TO_GATHER_STATS_FOR;racej++)
        {
            if (CalcFightStatsForRaces[racei][racej])
            {
                statLog("GATHERING STATS FOR RACES %s %s\n",ShipRaceToStr(racei),ShipRaceToStr(racej));
                GatherFightStatsForRaces(racei,racej);
                statLog("-------------------------------------\n");

                fileSave("statfile.bin",&FightStatsTable[0][0],statfilesize);
                CalculateOverallSums();
                statsPrintTable();
            }
        }
    }
}

void statsLoadFightStats(void)
{
    sdword correctsize = sizeof(FightStats) * (NUM_SHIPS_TO_GATHER_STATS_FOR * NUM_SHIPS_TO_GATHER_STATS_FOR);
    sdword statfilesize = fileSizeGet("statfile.bin",0);

    if (correctsize != statfilesize)
    {
        dbgMessage("WARNING: Stat file incorrect version");
        memset(&FightStatsTable[0][0],0,correctsize);
    }
    else
    {
        fileLoad("statfile.bin",&FightStatsTable[0][0],0);
    }

    CalculateOverallSums();
}

void statsPrintTableRaceXVsRaceY(FILE *tablefile,ShipRace racei,ShipRace racej)
{
    sdword i,j;
    sdword indexi,indexj;
    sdword raceiNumShips = NumShipTypesInRace[racei];
    sbyte *raceiCalcFightStatsFor = RaceCalcFightStatsFor[racei];
    sdword raceiStatIndexOffset = StatIndexRaceOffsets[racei];
    sdword racejNumShips = NumShipTypesInRace[racej];
    sbyte *racejCalcFightStatsFor = RaceCalcFightStatsFor[racej];
    sdword racejStatIndexOffset = StatIndexRaceOffsets[racej];
    FightStats *fightStats;

    fprintf(tablefile,"\t");
    for (j=0;j<racejNumShips;j++)
    {
        if (racejCalcFightStatsFor[j])
        {
            indexj = j+racejStatIndexOffset;
            fprintf(tablefile,"%dRU\t%dKill\t",indexj,indexj);
        }
    }

    for (i=0;i<raceiNumShips;i++)
    {
        if (raceiCalcFightStatsFor[i])
        {
            indexi = i+raceiStatIndexOffset;
            fprintf(tablefile,"\n%2d %2s %20s\t",indexi,ShipRaceToStr(racei),ShipTypeToStr(i+FirstShipTypeOfRace[racei]));
            for (j=0;j<racejNumShips;j++)
            {
                if (racejCalcFightStatsFor[j])
                {
                    indexj = j+racejStatIndexOffset;
                    fightStats = &FightStatsTable[indexi][indexj];
                    fprintf(tablefile,"%4.3f\t%4.3f\t",fightStats->fracRUratio,fightStats->fracKillratio);
                }
            }
        }
    }
}

void statsPrintTable(void)
{
    ShipRace racei,racej;
    filehandle tablefileFH = fileOpen("stattable.txt", FF_IgnoreBIG | FF_WriteMode | FF_TextMode | FF_UserSettingsPath);
    FILE *tablefile;

    dbgAssertOrIgnore(!fileUsingBigfile(tablefileFH));
    tablefile = fileStream(tablefileFH);

    dbgAssertOrIgnore(tablefile);

    for (racei=0;racei<NUM_RACES_TO_GATHER_STATS_FOR;racei++)
    {
        for (racej=0;racej<NUM_RACES_TO_GATHER_STATS_FOR;racej++)
        {
            if (CalcFightStatsForRaces[racei][racej])
            {
                fprintf(tablefile,"Statistics for %s VS %s\n",ShipRaceToStr(racei),ShipRaceToStr(racej));
                statsPrintTableRaceXVsRaceY(tablefile,racei,racej);
                fprintf(tablefile,"\n\n");
            }
        }
    }

    fileClose(tablefileFH);
}



FightStats *getFightStatsFromShipStatics(ShipStaticInfo *thisShip,ShipStaticInfo *againstShip)
{
    sdword i = ConvertShipRaceTypeToStatIndex(thisShip->shiptype,thisShip->shiprace);
    sdword j = ConvertShipRaceTypeToStatIndex(againstShip->shiptype,againstShip->shiprace);

    if ((i < 0) || (j < 0))
    {
        return NULL;
    }

    return &FightStatsTable[i][j];
}

real32 statsGetShipRURatingAgainstShip(ShipStaticInfo *thisShip,ShipStaticInfo *againstShip)
{
    FightStats *fightStats = getFightStatsFromShipStatics(thisShip,againstShip);

    if (fightStats == NULL)
    {
        return 0.0f;
    }

    return fightStats->fracRUratio;
}

real32 statsGetShipKillRatingAgainstShip(ShipStaticInfo *thisShip,ShipStaticInfo *againstShip)
{
    FightStats *fightStats = getFightStatsFromShipStatics(thisShip,againstShip);

    if (fightStats == NULL)
    {
        return 0.0f;
    }

    return fightStats->fracKillratio;
}

void statsSetOverkillfactor(real32 factor)
{
    statsOverkillfactor = factor;
    statsOverkillfactorSqr = factor*factor;
}

real32 statsGetOverallRURating(ShipStaticInfo *shipstatic)
{
    sdword i = ConvertShipRaceTypeToStatIndex(shipstatic->shiptype,shipstatic->shiprace);

    if (i < 0)
    {
        return 0.0f;
    }

    return FightStatsColumnSum[i].RUratio;
}

real32 statsGetOverallKillRating(ShipStaticInfo *shipstatic)
{
    sdword i = ConvertShipRaceTypeToStatIndex(shipstatic->shiptype,shipstatic->shiprace);

    if (i < 0)
    {
        return 0.0f;
    }

    return FightStatsColumnSum[i].Killratio;
}

bool32 ShipConstraintsNoneCB(Ship *ship)
{
    return TRUE;
}

bool32 statShipConstraintsNoneCB(ShipStaticInfo *shipstatic)
{
    return TRUE;
}

bool32 statShipConstraintsFrigatesOrWorseCB(ShipStaticInfo *shipstatic)
{
    if (shipstatic->shipclass >= CLASS_Frigate)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

bool32 statShipConstraintsFrigatesOrBetterCB(ShipStaticInfo *shipstatic)
{
    if (shipstatic->shipclass <= CLASS_Frigate)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


/*=============================================================================
    CAT (M2-R4 rest-oracle): Cataclysm's fight code reads static tables (read from cataclysm.exe) instead of HW1's
    gathered fight statistics: the kill rating per standard ship type (0x008962d8, 60 rows of 5 reals, the first is the
    rating), the per-class ship-count factor (0x00897078), the 12 x 12 class-against-class count matrix (0x008970a8,
    [ship class][target class]), the counter ship per target type and race (0x00896e2c, {Sect, Beast}) and the
    bytes that say whether a type has a counter (0x008961e8: Sect targets index from 0, Beast targets from +3, as
    statsBestShipToBuyToKillShip 0x00524cf0 does). statsOverkillfactor (0x008961b0) is 1.0.
=============================================================================*/
#define CAT_STATS_NUM_RATED_TYPES   60      // the standard types; anything else rates 1.0 or rolls a die

static const real32 cat_statsKillRating[CAT_STATS_NUM_RATED_TYPES] =
{
    1.0f, 3.0f, 4.0f, 4.0f, 5.0f, 6.0f, 7.0f, 9.0f, 8.0f, 1.0f, 11.0f, 1.0f, 5.0f, 1.0f, 5.0f, 1.0f,     // 0..15
    5.0f, 3.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 3.0f,      // 16..31
    2.0f, 1.0f, 3.0f, 2.0f, 4.0f, 5.0f, 5.0f, 7.0f, 6.0f, 9.0f, 8.0f, 1.0f, 10.0f, 1.0f, 11.0f, 5.0f,    // 32..47
    7.0f, 5.0f, 1.0f, 5.0f, 3.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 5.0f, 4.0f,                              // 48..59
};

static const sdword cat_statsClassFactor[NUM_CLASSES] = { 1, 1, 1, 1, 2, 8, 10, 0, 0, 8, 0, 10 };

static const sdword cat_statsClassMatrix[NUM_CLASSES][NUM_CLASSES] =
{
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 1, 1, 3, 3 },
    { 1, 3, 3, 3, 3, 2, 2, 2, 1, 1, 2, 2 },
    { 3, 3, 3, 3, 3, 2, 2, 2, 1, 1, 2, 2 },
    { 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 2, 1 },
    { 3, 3, 3, 3, 3, 3, 3, 1, 1, 3, 1, 1 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 3, 1, 1, 3, 1, 1 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0 },
};

static const ShipType cat_statsCounterShip[CAT_STATS_NUM_RATED_TYPES][2] =
{
    { 0,33}, { 1,40}, { 1,35}, { 1,35}, { 4,59}, { 4,39}, { 6,58}, { 7,46}, { 8,39}, { 7,39}, {10,46}, {10,46},
    {10,44}, { 8,44}, { 4,58}, { 0,35}, {25,44}, { 8,59}, {10,46}, {10,46}, {10,46}, {10,46}, {10,46}, {10,46},
    {10,46}, {25,46}, {10,46}, {10,46}, {10,46}, {10,46}, {10,46}, {10,46}, {10,46}, { 0,33}, { 1,40}, { 1,35},
    { 1,35}, { 4,37}, { 4,59}, { 4,39}, { 4,40}, { 7,44}, { 8,59}, { 7,39}, { 6,44}, { 8,44}, {10,46}, {25,44},
    { 6,58}, { 4,58}, { 0,59}, {25,44}, { 8,59}, {10,46}, {10,46}, {10,46}, {10,46}, {10,46}, { 1,35}, { 4,37},
};

static const ubyte cat_statsHasCounter[0x4c] =
{
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0,
    1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0,
};

// the kill rating of a type: the table for the standard types, 1.0 for anything else
static real32 cat_statsTypeKillRating(ShipType type)
{
    return (type < CAT_STATS_NUM_RATED_TYPES) ? cat_statsKillRating[type] : 1.0f;
}

// the kill ratio of a ship against a target (statsBestShipsToUseToKillTarget 0x00524ee0): the two ratings' quotient
// for standard-race ships and targets, 1.0 otherwise
static real32 cat_statsKillRatio(ShipStaticInfo *shipstatic, ShipStaticInfo *targetstatic)
{
    if (((shipstatic->shiprace == RACE_Sect) || (shipstatic->shiprace == RACE_Beast)) &&
        ((targetstatic->shiprace == RACE_Sect) || (targetstatic->shiprace == RACE_Beast)))
    {
        return cat_statsKillRating[targetstatic->shiptype] / cat_statsKillRating[shipstatic->shiptype];
    }
    return 1.0f;
}

/*-----------------------------------------------------------------------------
    Name        : statShipConstraintsFightingShipsCB
    Description : Returns true if the staticinfo passed is a ship that can be
                  considered a "fighting ship" (i.e. has guns, can attack
                  effectively, etc.)
    Inputs      : shipstatic - the staticinfo to check
    Outputs     :
    Return      : TRUE if the ship is a fighting ship
----------------------------------------------------------------------------*/
bool32 statShipConstraintsFightingShipsCB(ShipStaticInfo *shipstatic)
{
    // CAT (statShipConstraintsFightingShipsCB 0x00524b30): heavy cruisers, destroyers, fighters and frigates fight;
    //      corvettes except the two salvage corvettes (0x48, 0x59). The four frigate exceptions (0x2d, 0x3c, 0x4a,
    //      0x4e) break into the corvette test in the exe and so fight too (the fall-through is kept as it is)
    switch (shipstatic->shipclass)
    {
        case CLASS_HeavyCruiser:
        case CLASS_Destroyer:
        case CLASS_Fighter:
            return TRUE;
        case CLASS_Frigate:
            if ((shipstatic->shiptype != bDFGFrigate) && (shipstatic->shiptype != kAdvanceSupportFrigate) &&
                (shipstatic->shiptype != tAdvanceSupportFrigate) && (shipstatic->shiptype != tDFGFrigate))
            {
                return TRUE;
            }
            /* fall through */
        case CLASS_Corvette:
            if ((shipstatic->shiptype != kSalCapCorvette) && (shipstatic->shiptype != tSalCapCorvette))
            {
                return TRUE;
            }
            return FALSE;
        default:
            return FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : statShipConstraintsCarrierFightingShipsCB
    Description : Returns true if the staticinfo passed is a ship that can be
                  considered a "fighting ship" (i.e. has guns, can attack effectively, etc.)
                  and that can be built by a Carrier
    Inputs      : shipstatic - the staticinfo to check
    Outputs     :
    Return      : TRUE if the ship is a fighting ship
----------------------------------------------------------------------------*/
bool32 statShipConstraintsCarrierFightingShipsCB(ShipStaticInfo *shipstatic)
{
    // CAT (statShipConstraintsCarrierFightingShipsCB 0x00524bd0): fighters and frigates fight, corvettes except the
    //      salvage corvettes; the four frigate exceptions fall into the corvette test as above
    switch (shipstatic->shipclass)
    {
        case CLASS_Frigate:
            if ((shipstatic->shiptype != bDFGFrigate) && (shipstatic->shiptype != kAdvanceSupportFrigate) &&
                (shipstatic->shiptype != tAdvanceSupportFrigate) && (shipstatic->shiptype != tDFGFrigate))
            {
                return TRUE;
            }
            /* fall through */
        case CLASS_Corvette:
            if ((shipstatic->shiptype != kSalCapCorvette) && (shipstatic->shiptype != tSalCapCorvette))
            {
                return TRUE;
            }
            return FALSE;
        case CLASS_Fighter:
            return TRUE;
        default:
            return FALSE;
    }
}

real32 statsGetKillRatingAgainstFleet(ShipStaticInfo *shipstatic,SelectCommand *fleet)
{
    sdword numShips = fleet->numShips;
    sdword i;
    real32 totalkills = 0.0f;

    dbgAssertOrIgnore(numShips > 0);

    // kill rating against fleet is reciprical of (sum of recipricals of kill ratings)

    for (i=0;i<numShips;i++)
    {
        totalkills += statsGetShipKillRatingAgainstShip(fleet->ShipPtr[i]->staticinfo,shipstatic);
    }

    return (1.0f / totalkills);
}

real32 statsGetKillRatingAgainstFleetStatic(ShipStaticInfo *shipstatic,SelectCommandStatic *fleetStatic)
{
    sdword numShips = fleetStatic->numShips;
    sdword i;
    real32 totalkills = 0;

    dbgAssertOrIgnore(numShips > 0);

    for (i=0;i<numShips;i++)
    {
        totalkills += statsGetShipKillRatingAgainstShip(fleetStatic->ShipStaticPtr[i],shipstatic);
    }

    return (1.0f / totalkills);
}

real32 statsGetRURatingAgainstFleet(ShipStaticInfo *shipstatic,SelectCommand *fleet)
{
    sdword numShips = fleet->numShips;
    sdword i;
    real32 totalrurating = 0;

    dbgAssertOrIgnore(numShips > 0);

    for (i=0;i<numShips;i++)
    {
        totalrurating += statsGetShipRURatingAgainstShip(fleet->ShipPtr[i]->staticinfo,shipstatic);
    }

    return (1.0f / totalrurating);
}

real32 statsGetRURatingAgainstFleetStatic(ShipStaticInfo *shipstatic,SelectCommandStatic *fleetStatic)
{
    sdword numShips = fleetStatic->numShips;
    sdword i;
    real32 totalrurating = 0;

    dbgAssertOrIgnore(numShips > 0);

    for (i=0;i<numShips;i++)
    {
        totalrurating += statsGetShipRURatingAgainstShip(fleetStatic->ShipStaticPtr[i],shipstatic);
    }

    return (1.0f / totalrurating);
}

Ship *statsGetMostDangerousShipNonStatConstraints(SelectCommand *selection,ShipConstraintsCB constraintsCB)
{
    sdword numShips = selection->numShips;
    sdword i;
    Ship *ship;
    Ship *maxship = NULL;
    real32 maxKillratio = -1.0f;
    real32 Killratio;

    // CAT (0x00524c70): a NULL callback accepts every ship; the rating is the type's static kill rating
    for (i=0;i<numShips;i++)
    {
        ship = selection->ShipPtr[i];

        if ((constraintsCB == NULL) || constraintsCB(ship))
        {
            Killratio = cat_statsTypeKillRating(ship->shiptype);

            if (Killratio > maxKillratio)
            {
                maxKillratio = Killratio;
                maxship = ship;
            }
        }
    }

    return maxship;
}

ShipStaticInfo *statsGetMostDangerousShipStaticConstraints(SelectCommandStatic *selection,statShipConstraintsCB constraintsCB)
{
    sdword numShips = selection->numShips;
    sdword i;
    ShipStaticInfo *shipstatic;
    ShipStaticInfo *maxshipstatic = NULL;
    real32 maxKillratio = -1.0f;
    real32 Killratio;

    for (i=0;i<numShips;i++)
    {
        shipstatic = selection->ShipStaticPtr[i];

        if (constraintsCB(shipstatic))
        {
            Killratio = statsGetOverallKillRating(shipstatic);

            if (Killratio > maxKillratio)
            {
                maxKillratio = Killratio;
                maxshipstatic = shipstatic;
            }
        }
    }

    return maxshipstatic;
}

ShipStaticInfo *statsBestShipToBuyToKillShip(ShipRace shipRace,statShipConstraintsCB constraintsCB,ShipStaticInfo *targetstatic)
{
    ShipType type = targetstatic->shiptype;
    ubyte hasCounter;

    // CAT (statsBestShipToBuyToKillShip 0x00524cf0): the counter ship comes from the static table by target type and
    //      buyer's race; a non-standard target gets the race's first strike craft (sAcolyte / bInterceptor), a target
    //      of another race the race's first type; the constraints callback is not consulted
    (void)constraintsCB;
    if (targetstatic->shiprace == RACE_Sect)
    {
        if (type >= CAT_STATS_NUM_RATED_TYPES)
        {
            return GetShipStaticInfo(sAcolyte, RACE_Sect);
        }
        hasCounter = cat_statsHasCounter[type];
    }
    else if (targetstatic->shiprace == RACE_Beast)
    {
        if (type >= CAT_STATS_NUM_RATED_TYPES)
        {
            return GetShipStaticInfo(bInterceptor, RACE_Beast);
        }
        hasCounter = cat_statsHasCounter[type + 3];
    }
    else
    {
        return GetShipStaticInfo(FirstShipTypeOfRace[shipRace], shipRace);
    }
    if (hasCounter == 0)
    {
        return NULL;
    }
    return GetShipStaticInfo(cat_statsCounterShip[type][shipRace], shipRace);
}

ShipStaticInfo *statsBestShipToBuyToKillFleet(ShipRace shipRace,statShipConstraintsCB constraintsCB,SelectCommand *targetFleet)
{
    sdword i;
    sdword indexi;
    ShipStaticInfo *shipstatic;
    ShipStaticInfo *maxshipstatic = NULL;
    real32 maxRUratio = -1.0f;
    real32 RUratio;

    sdword raceiNumShips = NumShipTypesInRace[shipRace];
    sbyte *raceiCalcFightStatsFor = RaceCalcFightStatsFor[shipRace];
    sdword raceiStatIndexOffset = StatIndexRaceOffsets[shipRace];

    for (i=0;i<raceiNumShips;i++)
    {
        if (raceiCalcFightStatsFor[i])
        {
            indexi = i + raceiStatIndexOffset;
            shipstatic = ConvertStatIndexToShipStatic(indexi);
            if ((shipstatic) && constraintsCB(shipstatic))
            {
                dbgAssertOrIgnore(shipstatic->shiprace == shipRace);
                RUratio = statsGetRURatingAgainstFleet(shipstatic,targetFleet);
                if (RUratio > maxRUratio)
                {
                    maxRUratio = RUratio;
                    maxshipstatic = shipstatic;
                }
            }
        }
    }

    return maxshipstatic;
}

// CAT (M2-R7 ai-orders-moves): cat_statsRandomAttackShipStatic 0x00524d70: a random attack ship of the race, one
//      stream-6 draw over the exe's per-race tables (0x0089700c Sect 5, 0x00897020 Beast 7, 0x0089703c Kushan 6,
//      0x00897054 Taiidan 7, 0x00897070 P1 2: the P1 draw takes the low bit); NULL and no draw for any other race. The
//      fancy takeout order picks its secondary team with it (aioCreateFancyTakeoutTarget 0x00410920)
static const ShipType cat_statsRandomAttackSect[5]    = { sRammingFrigate, sMultiBeamFrigate, sHiveFrigate, sDreadnought, sDestroyer };
static const ShipType cat_statsRandomAttackBeast[7]   = { bAttackBomber, bMultiGunCorvette, bHeavyCorvette, bIonArrayFrigate,
                                                          bHeavyCruiser, bCruiseMissile, bMissileCorvette };
static const ShipType cat_statsRandomAttackKushan[6]  = { kAttackBomber, kHeavyCorvette, kHeavyCruiser, kIonCannonFrigate,
                                                          kMultiGunCorvette, kStandardDestroyer };
static const ShipType cat_statsRandomAttackTaiidan[7] = { tAttackBomber, tHeavyCruiser, tIonCannonFrigate, tMissileDestroyer,
                                                          tMultiGunCorvette, tStandardDestroyer, tStandardFrigate };
static const ShipType cat_statsRandomAttackP1[2]      = { p1IonArrayFrigate, p1MissileCorvette };

ShipStaticInfo *cat_statsRandomAttackShipStatic(ShipRace race)
{
    switch (race)
    {
        case RACE_Sect:     return GetShipStaticInfo(cat_statsRandomAttackSect[ranRandom(RANDOM_AI_PLAYER) % 5], race);
        case RACE_Beast:    return GetShipStaticInfo(cat_statsRandomAttackBeast[ranRandom(RANDOM_AI_PLAYER) % 7], race);
        case RACE_Kushan:   return GetShipStaticInfo(cat_statsRandomAttackKushan[ranRandom(RANDOM_AI_PLAYER) % 6], race);
        case RACE_Taiidan:  return GetShipStaticInfo(cat_statsRandomAttackTaiidan[ranRandom(RANDOM_AI_PLAYER) % 7], race);
        case RACE_P1:       return GetShipStaticInfo(cat_statsRandomAttackP1[ranRandom(RANDOM_AI_PLAYER) & 1], race);
        default:            return NULL;
    }
}

ShipStaticInfo *statsBestShipToBuyToKillFleetStatic(ShipRace shipRace,statShipConstraintsCB constraintsCB,SelectCommandStatic *targetFleetStatic)
{
    sdword i;
    ShipStaticInfo *shipstatic;
    ShipStaticInfo *maxshipstatic = NULL;
    real32 maxRUratio = -1.0f;
    real32 RUratio;

    sdword raceiNumShips = NumShipTypesInRace[shipRace];
    sbyte *raceiCalcFightStatsFor = RaceCalcFightStatsFor[shipRace];
    sdword raceiStatIndexOffset = StatIndexRaceOffsets[shipRace];
    sdword indexi;

    for (i=0;i<raceiNumShips;i++)
    {
        if (raceiCalcFightStatsFor[i])
        {
            indexi = i + raceiStatIndexOffset;
            shipstatic = ConvertStatIndexToShipStatic(indexi);
            if ((shipstatic) && constraintsCB(shipstatic))
            {
                RUratio = statsGetRURatingAgainstFleetStatic(shipstatic,targetFleetStatic);
                if (RUratio > maxRUratio)
                {
                    maxRUratio = RUratio;
                    maxshipstatic = shipstatic;
                }
            }
        }
    }

    return maxshipstatic;
}

Ship *statsBestShipToUseToKillShip(SelectCommand *freeShips,ShipStaticInfo *targetstatic)
{
    sdword i;
    Ship *ship;
    Ship *maxship = NULL;
    real32 maxKillratio = -1.0f;
    real32 Killratio;
    sdword numShips = freeShips->numShips;

    for (i=0;i<numShips;i++)
    {
        ship = freeShips->ShipPtr[i];
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        Killratio = statsGetShipKillRatingAgainstShip(ship->staticinfo,targetstatic);

        if (Killratio > maxKillratio)
        {
            maxKillratio = Killratio;
            maxship = ship;
        }
    }

    return maxship;
}

Ship *statsBestShipToUseToKillFleet(SelectCommand *freeShips,SelectCommand *targetFleet)
{
    sdword i;
    Ship *ship;
    Ship *maxship = NULL;
    real32 maxKillratio = -1.0f;
    real32 Killratio;
    sdword numShips = freeShips->numShips;

    for (i=0;i<numShips;i++)
    {
        ship = freeShips->ShipPtr[i];
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        Killratio = statsGetKillRatingAgainstFleet(ship->staticinfo,targetFleet);

        if (Killratio > maxKillratio)
        {
            maxKillratio = Killratio;
            maxship = ship;
        }
    }

    return maxship;
}

sdword statsNumShipsNeededToKillTarget(ShipStaticInfo *shipstatic,ShipStaticInfo *targetstatic)
{
    // CAT (0x00524e40): the class factor times the class-against-class count, or times a 1..9 roll of the AI stream
    //      for a non-standard target
    if (targetstatic->shiptype < CAT_STATS_NUM_RATED_TYPES)
    {
        return cat_statsClassFactor[shipstatic->shipclass] *
               cat_statsClassMatrix[shipstatic->shipclass][targetstatic->shipclass];
    }
    return cat_statsClassFactor[shipstatic->shipclass] * (sdword)(ranRandom(RANDOM_AI_PLAYER) % 9 + 1);
}

sdword statsNumShipsNeededToKillFleet(ShipStaticInfo *shipstatic,SelectCommand *targetFleet)
{
    // CAT (0x00524eb0): three times the class factor, whatever the fleet
    (void)targetFleet;
    return cat_statsClassFactor[shipstatic->shipclass] * 3;
}

// strength of fleet2 against fleet1, e.g. fleet2/fleet1
real32 statsGetRelativeFleetStrengths(SelectCommand *fleet1,SelectCommand *fleet2)
{
    // CAT (0x00524ed0): the ship counts' quotient
    return (real32)fleet2->numShips / (real32)fleet1->numShips;
}

// strength of ship against fleet strength, e.g. targetstatic strength / fleet1 strength
real32 statsGetRelativeFleetStrengthAgainstShip(SelectCommand *fleet1,ShipStaticInfo *targetstatic)
{
    return statsGetKillRatingAgainstFleet(targetstatic,fleet1);
}

SelectCommand *statsBestShipsToUseToKillTarget(SelectCommand *freeships,ShipStaticInfo *targetstatic,bool32 *goodEnough)
{
    SelectCommand *freeshipsleft;
    SelectCommand *useships;
    Ship *bestship;
    Ship *ship;
    sdword num;
    real32 fleetstr, ratio, bestratio;
    sdword i;

    // CAT (statsBestShipsToUseToKillTarget 0x00524ee0): the greedy pick uses the static kill ratings; the count per
    //      pick is the class-against-class table (or a 1..9 roll for a non-standard target); the fleet strength is the
    //      sum of the picked ships' ratios against the target, good enough at statsOverkillfactor or more
    if (freeships->numShips == 0)
    {
        *goodEnough = FALSE;
        return NULL;
    }

    freeshipsleft = selectDupSelection(freeships);

    useships = memAlloc(sizeofSelectCommand(freeships->numShips),"statsuseships",0);
    useships->numShips = 0;

    while (1)
    {
        bestship = NULL;
        bestratio = -1.0f;
        for (i=0;i<freeshipsleft->numShips;i++)
        {
            ratio = cat_statsKillRatio(freeshipsleft->ShipPtr[i]->staticinfo,targetstatic);
            if (ratio > bestratio)
            {
                bestship = freeshipsleft->ShipPtr[i];
                bestratio = ratio;
            }
        }
        if ((bestship->shiprace != RACE_Sect) && (bestship->shiprace != RACE_Beast))
        {
            *goodEnough = FALSE;        // as the exe: the two selections are not freed
            return NULL;
        }
        if (targetstatic->shiptype < CAT_STATS_NUM_RATED_TYPES)
        {
            num = cat_statsClassMatrix[bestship->staticinfo->shipclass][targetstatic->shipclass];
        }
        else
        {
            num = (sdword)(ranRandom(RANDOM_AI_PLAYER) % 9 + 1);
        }
        num *= cat_statsClassFactor[bestship->staticinfo->shipclass];

        clRemoveShipFromSelection(freeshipsleft,bestship);
        useships->ShipPtr[useships->numShips++] = bestship;

        while (--num > 0)
        {
            // find another ship of type bestship->staticinfo
            for (i=0;i<freeshipsleft->numShips;i++)
            {
                ship = freeshipsleft->ShipPtr[i];
                if (ship->staticinfo == bestship->staticinfo)
                {
                    break;
                }
            }
            if (i >= freeshipsleft->numShips)
            {
                break;              // didn't find any more ships
            }
            freeshipsleft->numShips--;
            freeshipsleft->ShipPtr[i] = freeshipsleft->ShipPtr[freeshipsleft->numShips];
            useships->ShipPtr[useships->numShips++] = ship;
        }

        // now check fleet strengths:
        fleetstr = 0.0f;
        for (i=0;i<useships->numShips;i++)
        {
            fleetstr += cat_statsKillRatio(useships->ShipPtr[i]->staticinfo,targetstatic);
        }
        if (statsOverkillfactor <= 1.0f / (1.0f / fleetstr))
        {
            memFree(freeshipsleft);
            *goodEnough = TRUE;
            return useships;
        }
        if (freeshipsleft->numShips == 0)
        {
            memFree(freeshipsleft);
            *goodEnough = FALSE;
            return useships;
        }
    }
}

SelectCommand *statsBestShipsToUseToKillFleet(SelectCommand *freeships,SelectCommand *targetFleet,bool32 *goodEnough)
{
    SelectCommand *freeshipsleft;
    SelectCommand *useships;
    Ship *bestship;
    Ship *ship;
    sdword num;
    real32 fleetstr;
    sdword i;

    if (freeships->numShips == 0)
    {
        *goodEnough = FALSE;
        return NULL;
    }

    freeshipsleft = selectDupSelection(freeships);

    useships = memAlloc(sizeofSelectCommand(freeships->numShips),"statsuseships",0);
    useships->numShips = 0;

repeat:

    bestship = statsBestShipToUseToKillFleet(freeshipsleft,targetFleet);
    num = statsNumShipsNeededToKillFleet(bestship->staticinfo,targetFleet);

    clRemoveShipFromSelection(freeshipsleft,bestship);
    useships->ShipPtr[useships->numShips++] = bestship;

    num--;
    while (num > 0)
    {
        // find another ship of type bestship->staticinfo
        for (i=0;i<freeshipsleft->numShips;i++)
        {
            ship = freeshipsleft->ShipPtr[i];
            if (ship->staticinfo == bestship->staticinfo)
            {
                // found it, remove it from freeshipsleft
                freeshipsleft->numShips--;
                freeshipsleft->ShipPtr[i] = freeshipsleft->ShipPtr[freeshipsleft->numShips];

                // add it to useships
                useships->ShipPtr[useships->numShips++] = ship;
                goto foundit;
            }
        }
        // didn't find any more ships, so break out of loop
        break;
foundit:
        num--;
    }

    // now check fleet strengths:

    // want useships strength relative to targetFleet
    fleetstr = 1.0f / statsGetRelativeFleetStrengths(useships,targetFleet);

    if (fleetstr < statsOverkillfactor)
    {
        // not good enough, so lets exit and not repeat if we don't have any free ships
        if (freeshipsleft->numShips == 0)
        {
            // use all ships
            dbgAssertOrIgnore(useships->numShips == freeships->numShips);
            memFree(freeshipsleft);
            *goodEnough = FALSE;
            return useships;
        }
        goto repeat;
    }

    memFree(freeshipsleft);
    *goodEnough = TRUE;
    return useships;
}

SelectCommand *statsGetMostEfficientShipsToKill(ShipStaticInfo *killer,SelectCommand *fleet,real32 efficiencyFactor)
{
    SelectCommand *returnsel;
    sdword i;

    if (fleet->numShips <= 0)
    {
        return NULL;
    }

    returnsel = selectDupSelection(fleet);

    for (i=0;i<returnsel->numShips; )
    {
        if (statsGetShipRURatingAgainstShip(killer,returnsel->ShipPtr[i]->staticinfo) <= efficiencyFactor)
        {
            // killer ship sucks against this ship, so let's remove it

            returnsel->numShips--;
            returnsel->ShipPtr[i] = returnsel->ShipPtr[returnsel->numShips];

            continue; // we must check same index again because we put last array entry here
        }
        i++;
    }

    if (returnsel->numShips == 0)
    {
        memFree(returnsel);
        return NULL;
    }

    return returnsel;
}

SelectCommand *CreateTestSelectionOfShips(ShipRace shiprace,ShipType shiptype,sdword numShips)
{
    SelectCommand *selection;
    vector position = { 0.0f, 0.0f, 0.0f };
    sdword i;

    selection = memAlloc(sizeofSelectCommand(numShips),"createshipsel",0);

    selection->numShips = numShips;
    for (i=0;i<numShips;i++)
    {
        selection->ShipPtr[i] = univCreateShip(shiptype,shiprace,&position,&universe.players[0],0,NULL);
    }

    return selection;
}

void FreeTestSelectionOfShips(SelectCommand *selection)
{
    sdword numShips = selection->numShips;
    sdword i;

    for (i=0;i<numShips;i++)
    {
        univFreeShipContents(selection->ShipPtr[i]);
        memFree(selection->ShipPtr[i]);
    }

    memFree(selection);
}

/*=============================================================================
    Cheat detection:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : statsGetStatChecksum
    Description : Returns a checksum of the game statistics
    Inputs      :
    Outputs     :
    Return      : The checksum of the game statistics
----------------------------------------------------------------------------*/
udword statsGetStatChecksum(void)
{
    udword i, intCheck;
    udword shiptotals,classtotals;

    for (i=0, intCheck = 0;i<tpGameCreated.numPlayers;i++)
    {
        // add the player's shipcount, resourcecount and sensorlevel
        intCheck += universe.players[i].totalships;
        intCheck += universe.players[i].resourceUnits;
        //intCheck += universe.players[i].sensorLevel;

        // add the player's shipcount for each shiptype
        shiptotals = crc32Compute((ubyte *)&universe.players[i].shiptotals, TOTAL_NUM_SHIPS*sizeof(sdword));
        intCheck += shiptotals;

        // CAT: one CRC only, over the 145-entry shiptotals (crc32Compute(player+0x30, 0x244) in statsGetStatChecksum
        //      0x00525170); Relic's second CRC (meant for classtotals, it hashed shiptotals again) is gone. The exe
        //      adds Player +0x4 (resourceUnits), +0x2C (totalships), +0xC (supportUnitsMax) and +0x35C
        //      (listoftopics.num) [M2-R4 rest-oracle: named by the Player layout, Universe.h]
        classtotals = 0;

        // add the player's support-unit budget and research information
        intCheck += universe.players[i].supportUnitsMax;
        intCheck += universe.players[i].researchinfo.listoftopics.num;

        if ((netlogfile) && (logEnable == LOG_VERBOSE))
        {
#if BINNETLOG
            binnetlogCheatInfo bnc;

            bnc.header = makenetcheckHeader('C','D','E','T');
            bnc.totalships = universe.players[i].totalships;
            bnc.resourceunits = universe.players[i].resourceUnits;
            bnc.shiptotals = shiptotals;
            bnc.classtotals = classtotals;
            bnc.hastechnology = universe.players[i].supportUnitsMax;   // CAT: +0xC, the fifth CDET value
            bnc.listoftopicsnum = universe.players[i].researchinfo.listoftopics.num;
            fwrite(&bnc,sizeof(bnc),1,netlogfile);
#else
            fprintf(netlogfile,"  CDET:%d %d %d %d %d %d\n",universe.players[i].totalships,universe.players[i].resourceUnits,
                                shiptotals,classtotals,universe.players[i].supportUnitsMax,universe.players[i].researchinfo.listoftopics.num);
#endif
        }
    }

    // bounty checking (all bounty values must be deterministic)
    {
#if BINNETLOG
        binnetlogBountyInfo bnb;

        bnb.header = makenetcheckHeader('B','O','U','N');
        for (i=0;i<8;i++)
        {
            bnb.bounties[i] = 0;
        }
#endif

        for (i=0;i<universe.numPlayers;i++)
        {
            intCheck += universe.players[i].bounty;
#if BINNETLOG
            bnb.bounties[i] = universe.players[i].bounty;
#endif
        }

        if ((netlogfile) && (logEnable == LOG_VERBOSE))
        {
#if BINNETLOG
            fwrite(&bnb,sizeof(bnb),1,netlogfile);
#else
            fprintf(netlogfile,"BOUNT:");
            for (i=0;i<universe.numPlayers;i++)
            {
                fprintf(netlogfile," %d",universe.players[i].bounty);
            }
            fprintf(netlogfile,"\n");
#endif
        }

    }

    return intCheck;
}

