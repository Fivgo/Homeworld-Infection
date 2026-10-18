/*=============================================================================
    Name    : ResearchAPI.c
    Purpose : logic for the research manager api functions.

    Created 5/27/1998 by ddunlop
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include <stdio.h>

#include "CommandDefs.h"
#include "FOW.h"
#include "CommandLayer.h"
#include "CommandWrap.h"
#include "ConsMgr.h"
#include "Debug.h"
#include "FEReg.h"
#include "LinkedList.h"
#include "Memory.h"
#include "MultiplayerGame.h"
#include "ObjTypes.h"
#include "ResearchAPI.h"
#include "ResearchGUI.h"
#include "ShipDefs.h"
#include "ShipSelect.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "StatScript.h"
#include "StringSupport.h"
#include "TradeMgr.h"
#include "Universe.h"
#include "Mothership.h"
#include "InfoOverlay.h"

// CAT (M2-R4 rest-oracle): the whole file follows cataclysm.exe 0x004f2530-0x004f4790 and 0x005146f0. Technology is a
//      set of 42 sdwords (ResearchAPI.h CatTechSet); HW1's bit masks, labdec[], the research ship's lab activation
//      (rmDeactivateLab / rmActivateFreeLab), rmNumTechRequiredForTech, rmTechRequiredForTech, rmAllTechRequredForShip and
//      rmRemoveAllUnneededTech have no exe counterpart and are gone. Three static tables replace HW1's six: the Sect
//      single-player table (0x00c282a0), the Sect multiplayer table (0x00c18d20) and the Beast table (0x00c207e0, used
//      in single and multiplayer).

#define makeSPRSTechEntry(var) {"SPRS" #var, scriptSetSdwordCB, &cat_SinglePlayerSectTechStatic.TimeToComplete[var]}
#define makeMPRSTechEntry(var) {"MPRS" #var, scriptSetSdwordCB, &cat_MultiPlayerSectTechStatic.TimeToComplete[var]}
#define makeMPRBTechEntry(var) {"MPRB" #var, scriptSetSdwordCB, &cat_BeastTechStatic.TimeToComplete[var]}

/*=============================================================================
    Data:
=============================================================================*/

TechStatics            cat_SinglePlayerSectTechStatic;      // CAT: 0x00c282a0 (SPRS*)
TechStatics            cat_MultiPlayerSectTechStatic;       // CAT: 0x00c18d20 (MPRS*)
TechStatics            cat_BeastTechStatic;                 // CAT: 0x00c207e0 (MPRB*, single and multiplayer)

// CAT: the lab (research module) a Sect technology is researched in, -1 = any (0x00890328, read from cataclysm.exe;
//      rmFindFreeLab 0x004f2870)
static const sdword cat_techLabType[NumTechnologies] =
{
    -1, -1, -1, -1, -1, -1,  2,  2,  0,  0,  0,  0,  0,  0,  1,  1,
     2,  3,  4,  4,  4,  4,  4,  4,  0,  2,  2,  4,  3,  1,  1,  1,
    -1,  3,  3,  2,  3,  3,  3, -1, -1
};
void cat_rsMPRBShipDependSetCB(char *directory, char *field, void *dataToFillIn);   // CAT: 0x004f4430 (below)


// CAT: the script table at 0x0088fb98 (128 rows, read from cataclysm.exe). The Beast table has no
//      MPRBTECH_RESEARCH_CLEESAN_ATTACK row.
scriptEntry TechnologyDependancies[] =
{
    { "SPRSShipDepend", cat_rsShipDependSetCB, &cat_SinglePlayerSectTechStatic.TechNeededToBuildShip[0]},
    { "SPRSTechDepend", cat_rsTechDependSetCB, &cat_SinglePlayerSectTechStatic.TechNeededToResearch[0]},
    makeSPRSTechEntry(TECH_CLOAKING),
    makeSPRSTechEntry(TECH_TARGETING_SYSTEMS),
    makeSPRSTechEntry(TECH_PLASMA_WEAPONS),
    makeSPRSTechEntry(TECH_MEDIUM_GUNS),
    makeSPRSTechEntry(TECH_DEFENSE_FIELD),
    makeSPRSTechEntry(TECH_HEAVY_GUNS),
    makeSPRSTechEntry(TECH_REPAIR),
    makeSPRSTechEntry(TECH_SALVAGE),
    makeSPRSTechEntry(TECH_DRIVE_FIGHTER),
    makeSPRSTechEntry(TECH_ADRIVE_FIGHTER),
    makeSPRSTechEntry(TECH_DRIVE_FRIGATE),
    makeSPRSTechEntry(TECH_ADRIVE_FRIGATE),
    makeSPRSTechEntry(TECH_DRIVE_DESTROYER),
    makeSPRSTechEntry(TECH_ADRIVE_DESTROYER),
    makeSPRSTechEntry(TECH_ARMOUR2),
    makeSPRSTechEntry(TECH_ARMOUR3),
    makeSPRSTechEntry(TECH_SUPPORT_SYSTEMS),
    makeSPRSTechEntry(TECH_MICRO_SHIP),
    makeSPRSTechEntry(TECH_ENERGY),
    makeSPRSTechEntry(TECH_ION),
    makeSPRSTechEntry(TECH_MISSILE),
    makeSPRSTechEntry(TECH_REPULSE),
    makeSPRSTechEntry(TECH_EMP),
    makeSPRSTechEntry(TECH_EXPLOSIVE_CHARGE),
    makeSPRSTechEntry(TECH_LINK),
    makeSPRSTechEntry(TECH_MIMIC),
    makeSPRSTechEntry(TECH_AFTERBURNER),
    makeSPRSTechEntry(TECH_BIG_GUN),
    makeSPRSTechEntry(TECH_ADVANCED_SENSORS),
    makeSPRSTechEntry(TECH_FORCE_FIELD1),
    makeSPRSTechEntry(TECH_FORCE_FIELD2),
    makeSPRSTechEntry(TECH_FORCE_FIELD3),
    makeSPRSTechEntry(TECH_INFECT),
    makeSPRSTechEntry(TECH_REPAIRBOTS),
    makeSPRSTechEntry(TECH_CRYSTAL_HARVEST),
    makeSPRSTechEntry(TECH_RESEARCH_CLEESAN_ATTACK),
    makeSPRSTechEntry(TECH_BEAST_VACCINE),
    makeSPRSTechEntry(TECH_PHASED_TELEMETRY),
    makeSPRSTechEntry(TECH_INERTIALESS_DRIVE),
    makeSPRSTechEntry(TECH_BIOWARHEAD),
    makeSPRSTechEntry(TECH_DRIVE_CORVETTE),

    { "MPRSShipDepend", cat_rsShipDependSetCB, &cat_MultiPlayerSectTechStatic.TechNeededToBuildShip[0]},
    { "MPRSTechDepend", cat_rsTechDependSetCB, &cat_MultiPlayerSectTechStatic.TechNeededToResearch[0]},
    makeMPRSTechEntry(TECH_CLOAKING),
    makeMPRSTechEntry(TECH_TARGETING_SYSTEMS),
    makeMPRSTechEntry(TECH_PLASMA_WEAPONS),
    makeMPRSTechEntry(TECH_MEDIUM_GUNS),
    makeMPRSTechEntry(TECH_DEFENSE_FIELD),
    makeMPRSTechEntry(TECH_HEAVY_GUNS),
    makeMPRSTechEntry(TECH_REPAIR),
    makeMPRSTechEntry(TECH_SALVAGE),
    makeMPRSTechEntry(TECH_DRIVE_FIGHTER),
    makeMPRSTechEntry(TECH_ADRIVE_FIGHTER),
    makeMPRSTechEntry(TECH_DRIVE_FRIGATE),
    makeMPRSTechEntry(TECH_ADRIVE_FRIGATE),
    makeMPRSTechEntry(TECH_DRIVE_DESTROYER),
    makeMPRSTechEntry(TECH_ADRIVE_DESTROYER),
    makeMPRSTechEntry(TECH_ARMOUR2),
    makeMPRSTechEntry(TECH_ARMOUR3),
    makeMPRSTechEntry(TECH_SUPPORT_SYSTEMS),
    makeMPRSTechEntry(TECH_MICRO_SHIP),
    makeMPRSTechEntry(TECH_ENERGY),
    makeMPRSTechEntry(TECH_ION),
    makeMPRSTechEntry(TECH_MISSILE),
    makeMPRSTechEntry(TECH_REPULSE),
    makeMPRSTechEntry(TECH_EMP),
    makeMPRSTechEntry(TECH_EXPLOSIVE_CHARGE),
    makeMPRSTechEntry(TECH_LINK),
    makeMPRSTechEntry(TECH_MIMIC),
    makeMPRSTechEntry(TECH_AFTERBURNER),
    makeMPRSTechEntry(TECH_BIG_GUN),
    makeMPRSTechEntry(TECH_ADVANCED_SENSORS),
    makeMPRSTechEntry(TECH_FORCE_FIELD1),
    makeMPRSTechEntry(TECH_FORCE_FIELD2),
    makeMPRSTechEntry(TECH_FORCE_FIELD3),
    makeMPRSTechEntry(TECH_INFECT),
    makeMPRSTechEntry(TECH_REPAIRBOTS),
    makeMPRSTechEntry(TECH_CRYSTAL_HARVEST),
    makeMPRSTechEntry(TECH_RESEARCH_CLEESAN_ATTACK),
    makeMPRSTechEntry(TECH_BEAST_VACCINE),
    makeMPRSTechEntry(TECH_PHASED_TELEMETRY),
    makeMPRSTechEntry(TECH_INERTIALESS_DRIVE),
    makeMPRSTechEntry(TECH_BIOWARHEAD),
    makeMPRSTechEntry(TECH_DRIVE_CORVETTE),

    // CAT: the Beast ship rows go through cat_rsMPRBShipDependSetCB 0x004f4430 (the script table at 0x0088ffa4), whose
    //      body is cat_rsShipDependSetCB's (M2-R6 managers: its own function)
    { "MPRBShipDepend", cat_rsMPRBShipDependSetCB, &cat_BeastTechStatic.TechNeededToBuildShip[0]},
    { "MPRBTechDepend", cat_rsTechDependSetCB, &cat_BeastTechStatic.TechNeededToResearch[0]},
    makeMPRBTechEntry(TECH_CLOAKING),
    makeMPRBTechEntry(TECH_TARGETING_SYSTEMS),
    makeMPRBTechEntry(TECH_PLASMA_WEAPONS),
    makeMPRBTechEntry(TECH_MEDIUM_GUNS),
    makeMPRBTechEntry(TECH_DEFENSE_FIELD),
    makeMPRBTechEntry(TECH_HEAVY_GUNS),
    makeMPRBTechEntry(TECH_REPAIR),
    makeMPRBTechEntry(TECH_SALVAGE),
    makeMPRBTechEntry(TECH_DRIVE_FIGHTER),
    makeMPRBTechEntry(TECH_ADRIVE_FIGHTER),
    makeMPRBTechEntry(TECH_DRIVE_FRIGATE),
    makeMPRBTechEntry(TECH_ADRIVE_FRIGATE),
    makeMPRBTechEntry(TECH_DRIVE_DESTROYER),
    makeMPRBTechEntry(TECH_ADRIVE_DESTROYER),
    makeMPRBTechEntry(TECH_ARMOUR2),
    makeMPRBTechEntry(TECH_ARMOUR3),
    makeMPRBTechEntry(TECH_SUPPORT_SYSTEMS),
    makeMPRBTechEntry(TECH_MICRO_SHIP),
    makeMPRBTechEntry(TECH_ENERGY),
    makeMPRBTechEntry(TECH_ION),
    makeMPRBTechEntry(TECH_MISSILE),
    makeMPRBTechEntry(TECH_REPULSE),
    makeMPRBTechEntry(TECH_EMP),
    makeMPRBTechEntry(TECH_EXPLOSIVE_CHARGE),
    makeMPRBTechEntry(TECH_LINK),
    makeMPRBTechEntry(TECH_MIMIC),
    makeMPRBTechEntry(TECH_AFTERBURNER),
    makeMPRBTechEntry(TECH_BIG_GUN),
    makeMPRBTechEntry(TECH_ADVANCED_SENSORS),
    makeMPRBTechEntry(TECH_FORCE_FIELD1),
    makeMPRBTechEntry(TECH_FORCE_FIELD2),
    makeMPRBTechEntry(TECH_FORCE_FIELD3),
    makeMPRBTechEntry(TECH_INFECT),
    makeMPRBTechEntry(TECH_REPAIRBOTS),
    makeMPRBTechEntry(TECH_CRYSTAL_HARVEST),
    makeMPRBTechEntry(TECH_BEAST_VACCINE),
    makeMPRBTechEntry(TECH_PHASED_TELEMETRY),
    makeMPRBTechEntry(TECH_INERTIALESS_DRIVE),
    makeMPRBTechEntry(TECH_BIOWARHEAD),
    makeMPRBTechEntry(TECH_DRIVE_CORVETTE),

    { NULL,NULL,0 }
};

// CAT: techtypeinfo 0x008901a8, 41 rows read from cataclysm.exe (the names are the exe's)
static NumStrXlate techtypeinfo[] =
{
    { (udword)TECH_CLOAKING,                strdollar(TECH_CLOAKING)},
    { (udword)TECH_TARGETING_SYSTEMS,       strdollar(TECH_TARGETING_SYSTEMS)},
    { (udword)TECH_PLASMA_WEAPONS,          strdollar(TECH_PLASMA_WEAPONS)},
    { (udword)TECH_MEDIUM_GUNS,             strdollar(TECH_MEDIUM_GUNS)},
    { (udword)TECH_DEFENSE_FIELD,           strdollar(TECH_DEFENSE_FIELD)},
    { (udword)TECH_HEAVY_GUNS,              strdollar(TECH_HEAVY_GUNS)},
    { (udword)TECH_REPAIR,                  strdollar(TECH_REPAIR)},
    { (udword)TECH_SALVAGE,                 strdollar(TECH_SALVAGE)},
    { (udword)TECH_DRIVE_FIGHTER,           strdollar(TECH_DRIVE_FIGHTER)},
    { (udword)TECH_ADRIVE_FIGHTER,          strdollar(TECH_ADRIVE_FIGHTER)},
    { (udword)TECH_DRIVE_FRIGATE,           strdollar(TECH_DRIVE_FRIGATE)},
    { (udword)TECH_ADRIVE_FRIGATE,          strdollar(TECH_ADRIVE_FRIGATE)},
    { (udword)TECH_DRIVE_DESTROYER,         strdollar(TECH_DRIVE_DESTROYER)},
    { (udword)TECH_ADRIVE_DESTROYER,        strdollar(TECH_ADRIVE_DESTROYER)},
    { (udword)TECH_ARMOUR2,                 strdollar(TECH_ARMOUR2)},
    { (udword)TECH_ARMOUR3,                 strdollar(TECH_ARMOUR3)},
    { (udword)TECH_SUPPORT_SYSTEMS,         strdollar(TECH_SUPPORT_SYSTEMS)},
    { (udword)TECH_MICRO_SHIP,              strdollar(TECH_MICRO_SHIP)},
    { (udword)TECH_ENERGY,                  strdollar(TECH_ENERGY)},
    { (udword)TECH_ION,                     strdollar(TECH_ION)},
    { (udword)TECH_MISSILE,                 strdollar(TECH_MISSILE)},
    { (udword)TECH_REPULSE,                 strdollar(TECH_REPULSE)},
    { (udword)TECH_EMP,                     strdollar(TECH_EMP)},
    { (udword)TECH_EXPLOSIVE_CHARGE,        strdollar(TECH_EXPLOSIVE_CHARGE)},
    { (udword)TECH_LINK,                    strdollar(TECH_LINK)},
    { (udword)TECH_MIMIC,                   strdollar(TECH_MIMIC)},
    { (udword)TECH_AFTERBURNER,             strdollar(TECH_AFTERBURNER)},
    { (udword)TECH_BIG_GUN,                 strdollar(TECH_BIG_GUN)},
    { (udword)TECH_ADVANCED_SENSORS,        strdollar(TECH_ADVANCED_SENSORS)},
    { (udword)TECH_FORCE_FIELD1,            strdollar(TECH_FORCE_FIELD1)},
    { (udword)TECH_FORCE_FIELD2,            strdollar(TECH_FORCE_FIELD2)},
    { (udword)TECH_FORCE_FIELD3,            strdollar(TECH_FORCE_FIELD3)},
    { (udword)TECH_INFECT,                  strdollar(TECH_INFECT)},
    { (udword)TECH_REPAIRBOTS,              strdollar(TECH_REPAIRBOTS)},
    { (udword)TECH_CRYSTAL_HARVEST,         strdollar(TECH_CRYSTAL_HARVEST)},
    { (udword)TECH_RESEARCH_CLEESAN_ATTACK, strdollar(TECH_RESEARCH_CLEESAN_ATTACK)},
    { (udword)TECH_BEAST_VACCINE,           strdollar(TECH_BEAST_VACCINE)},
    { (udword)TECH_PHASED_TELEMETRY,        strdollar(TECH_PHASED_TELEMETRY)},
    { (udword)TECH_INERTIALESS_DRIVE,       strdollar(TECH_INERTIALESS_DRIVE)},
    { (udword)TECH_BIOWARHEAD,              strdollar(TECH_BIOWARHEAD)},
    { (udword)TECH_DRIVE_CORVETTE,          strdollar(TECH_DRIVE_CORVETTE)},
    { 0,NULL }
};

// CAT: cat_moduletypeinfo 0x008902f8 (read from cataclysm.exe)
static NumStrXlate cat_moduletypeinfo[] =
{
    { (udword)MODULE_Hangar,                strdollar(MODULE_Hangar)},
    { (udword)MODULE_Armour,                strdollar(MODULE_Armour)},
    { (udword)MODULE_Engineering,           strdollar(MODULE_Engineering)},
    { (udword)MODULE_Advanced_Engineering,  strdollar(MODULE_Advanced_Engineering)},
    { (udword)MODULE_Weapon,                strdollar(MODULE_Weapon)},
    { 0,NULL }
};

/*=============================================================================
    Functions:
=============================================================================*/

char *RaceSpecificTechTypeToNiceString(TechnologyType tech, ShipRace race)
{
    // CAT: no race-specific names (the exe has no counterpart; HW1 renamed DDDFDFGFTech / CloakDefenseFighter per race)
    (void)race;
    return TechTypeToNiceString(tech);
}

char *TechTypeToNiceString(TechnologyType tech)
{
    return (strGetString((uword)tech+strTechOffset));
}

char *TechTypeToString(TechnologyType tech)
{
    return NumToStr(techtypeinfo,(uword)tech);
}

// CAT: 0x004f2570
char *cat_ModuleTypeToString(sdword module)
{
    return NumToStr(cat_moduletypeinfo,(uword)module);
}

TechnologyType StrToTechType(char *tech)
{
    return (StrToNum(techtypeinfo, tech));
}

/*=============================================================================
    CAT: the tech set operations (0x004f3fc0 - 0x004f4300)
=============================================================================*/

// CAT: the technologies that change what a player's ships reveal (cat_rmTechSetAdd 0x004f3fc0 and cat_rmTechSetInit
//      0x004f4210 test -1, 2 and 28): the fog-of-war cubes of every ship with a grid are taken down before the set
//      changes and put back after (cat_shipAffectsFOW 0x00480ce0, cat_univShipFOWDeactivate 0x005422b0 /
//      cat_univShipFOWActivate 0x00542240; M2-R6 nis-fow-camera). Only the local player has a grid.
static bool32 cat_rmTechAffectsFOW(sdword tech)
{
    return ((tech == -1) || (tech == TECH_PLASMA_WEAPONS) || (tech == TECH_ADVANCED_SENSORS));
}

static void cat_rmFOWDeactivateAll(void)
{
    Node *node;
    Ship *ship;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (cat_playerGetFOWGrid(ship->playerowner) != NULL)
        {
            if (cat_shipAffectsFOW(ship))
            {
                cat_univShipFOWDeactivate(ship);
            }
        }
    }
}

static void cat_rmFOWActivateAll(void)
{
    Node *node;
    Ship *ship;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (cat_playerGetFOWGrid(ship->playerowner) != NULL)
        {
            if (cat_shipAffectsFOW(ship))
            {
                cat_univShipFOWActivate(ship);
            }
        }
    }
}

// 0x004f3fc0: techset[tech] = 1
void cat_rmTechSetAdd(sdword *techset, TechnologyType tech)
{
    bool32 fow = cat_rmTechAffectsFOW(tech);

    if (fow)
    {
        cat_rmFOWDeactivateAll();
    }
    techset[tech] = 1;
    if (fow)
    {
        cat_rmFOWActivateAll();
    }
}

// 0x004f4070: out = a AND b, with the "all" flag shortcuts
void cat_rmTechSetAnd(sdword *out, sdword *a, sdword *b)
{
    sdword i;

    if ((a[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL) && (b[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL))
    {
        for (i = 0; i < CAT_TECHSET_SIZE; i++)
        {
            out[i] = 0;
        }
        out[CAT_TECHSET_FLAG] = CAT_TECHSET_ALL;
        return;
    }
    if (a[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL)
    {
        for (i = 0; i < NumTechnologies; i++)
        {
            out[i] = b[i];
        }
        out[CAT_TECHSET_FLAG] = 0;
        return;
    }
    if (b[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL)
    {
        for (i = 0; i < NumTechnologies; i++)
        {
            out[i] = a[i];
        }
        out[CAT_TECHSET_FLAG] = 0;
        return;
    }
    for (i = 0; i < NumTechnologies; i++)
    {
        out[i] = (sdword)((udword)a[i] & (udword)b[i]);
    }
    out[CAT_TECHSET_FLAG] = 0;
}

// 0x004f4130: out = a OR b; "all" if either is
void cat_rmTechSetOr(sdword *out, sdword *a, sdword *b)
{
    sdword i;

    if ((a[CAT_TECHSET_FLAG] != CAT_TECHSET_ALL) && (b[CAT_TECHSET_FLAG] != CAT_TECHSET_ALL))
    {
        for (i = 0; i < NumTechnologies; i++)
        {
            out[i] = (sdword)((udword)a[i] | (udword)b[i]);
        }
        out[CAT_TECHSET_FLAG] = 0;
        return;
    }
    for (i = 0; i < CAT_TECHSET_SIZE; i++)
    {
        out[i] = 0;
    }
    out[CAT_TECHSET_FLAG] = CAT_TECHSET_ALL;
}

// 0x004f41a0
void cat_rmTechSetClear(sdword *techset)
{
    sdword i;

    for (i = 0; i < CAT_TECHSET_SIZE; i++)
    {
        techset[i] = 0;
    }
}

// 0x004f41b0
bool32 cat_rmTechSetHas(sdword *techset, TechnologyType tech)
{
    if ((techset[CAT_TECHSET_FLAG] != CAT_TECHSET_ALL) && (techset[tech] != 1))
    {
        return FALSE;
    }
    return TRUE;
}

// 0x004f41d0
bool32 cat_rmTechSetEqual(sdword *a, sdword *b)
{
    sdword i;

    if ((a[CAT_TECHSET_FLAG] != b[CAT_TECHSET_FLAG]) || (a[CAT_TECHSET_FLAG] != CAT_TECHSET_ALL))
    {
        for (i = 0; i < NumTechnologies; i++)
        {
            if (a[i] != b[i])
            {
                return FALSE;
            }
        }
    }
    return TRUE;
}

// 0x004f4210: techlevel -2 disables, -1 clears the set, 2 gives everything, anything else only clears the flag
void cat_rmTechSetInit(sdword *techset, sdword techlevel)
{
    bool32 fow = cat_rmTechAffectsFOW(techlevel);
    sdword i;

    if (fow)
    {
        cat_rmFOWDeactivateAll();
    }
    if (techlevel == CAT_TECHSET_DISABLED)
    {
        techset[CAT_TECHSET_FLAG] = CAT_TECHSET_DISABLED;
    }
    else if (techlevel == TECH_NOTECHNOLOGY)
    {
        for (i = 0; i < CAT_TECHSET_SIZE; i++)
        {
            techset[i] = 0;
        }
    }
    else if (techlevel == TECH_ALLTECHNOLOGY)
    {
        techset[CAT_TECHSET_FLAG] = CAT_TECHSET_ALL;
    }
    else
    {
        techset[CAT_TECHSET_FLAG] = 0;
    }
    if (fow)
    {
        cat_rmFOWActivateAll();
    }
}

// 0x004f4300: out[i] = (a[i] == 0); empty when a carries a flag
void cat_rmTechSetNot(sdword *out, sdword *a)
{
    sdword i;

    if (a[CAT_TECHSET_FLAG] != 0)
    {
        for (i = 0; i < CAT_TECHSET_SIZE; i++)
        {
            out[i] = 0;
        }
        return;
    }
    for (i = 0; i < NumTechnologies; i++)
    {
        out[i] = (a[i] == 0);
    }
    out[CAT_TECHSET_FLAG] = 0;
}

// CAT: copy helper for the 42-dword sets the exe copies inline (rep movsd)
static void cat_rmTechSetCopy(sdword *out, sdword *a)
{
    sdword i;

    for (i = 0; i < CAT_TECHSET_SIZE; i++)
    {
        out[i] = a[i];
    }
}

/*-----------------------------------------------------------------------------
    Name        : Reseaching
    Description : checks to see if the passed in tech is currently being researched.
    Inputs      : tech
    Outputs     : true false
    Return      : bool32
----------------------------------------------------------------------------*/
ResearchTopic *Researching(Player *player, TechnologyType tech)
{
    PlayerResearchInfo *research=&player->researchinfo;
    ResearchTopic      *topic;
    Node               *walk;

    walk = research->listoftopics.head;

    // walk through list of topics being researched
    while (walk != NULL)
    {
        topic = (ResearchTopic *)listGetStructOfNode(walk);
        if (topic->techresearch == tech)
            return(topic);
        walk = walk->next;
    }

    return(NULL);
}

//
//  returns 1 if the player is currently researching anything
//  returns 0 otherwise
//
sdword rmResearchingAnything(struct Player *player)
{
    PlayerResearchInfo *research=&player->researchinfo;

    return research->listoftopics.head != NULL;
}

// CAT: one technology into the set, then the player's upgrades follow (rmAddTechToPlayer 0x004f2600)
void   rmAddTechToPlayer(struct Player *player, TechnologyType tech)
{
    cat_rmTechSetAdd(player->researchinfo.HasTechnology, tech);
    cat_playerUpdateUpgradesFromResearch(player);
}


/*-----------------------------------------------------------------------------
    Name        : rmCanBuildShip
    Description : returns true if player has tech needed to build that ship.
    Inputs      : player, type, checkHangar (CAT: the 3rd argument of 0x004f2630: a Sect
                  external-build ship also needs a completed hangar module unless it is a
                  module itself)
    Outputs     : true/false
    Return      : bool32
----------------------------------------------------------------------------*/
bool32 rmCanBuildShip(Player *player, ShipType type, bool32 checkHangar)
{
    PlayerResearchInfo *research = &player->researchinfo;
    CatTechSet          needAndHave;
    sdword             *need;
    ShipStaticInfo     *statinfo;
    Ship               *mothership;
    sdword              i;

    // CAT: a Beast computer player in multiplayer reads its enabled flags for six types (0x004f2630: types 0x22, 0x26,
    //      0x29, 0x2a, 0x30, 0x34 read catShipTypeEnabled[1, 4, 7, 8, 6, 0x11])
    if ((!singlePlayerGame) && (player->aiPlayer != NULL) && (player->race == RACE_Beast))
    {
        switch (type)
        {
            case bAcolyte:              return research->catShipTypeEnabled[1];
            case bACV:                  return research->catShipTypeEnabled[4];
            case bMultiBeamFrigate:     return research->catShipTypeEnabled[7];
            case bHiveFrigate:          return research->catShipTypeEnabled[8];
            case bRammingFrigate:       return research->catShipTypeEnabled[6];
            case bSentinel:             return research->catShipTypeEnabled[0x11];
            default: break;
        }
    }

    need = research->techstat->TechNeededToBuildShip[type];
    if (need[CAT_TECHSET_FLAG] == CAT_TECHSET_DISABLED)
    {
        return(FALSE);
    }
    // CAT: the Nomad moon (0x85) is never buildable outside single player
    if ((!singlePlayerGame) && (type == 0x85))
    {
        return(FALSE);
    }
    if (player->race == RACE_Sect)
    {
        statinfo = GetShipStaticInfo(type, player->race);
        if ((checkHangar) && (statinfo->externalBuild) && (statinfo->shipclass != CLASS_Component) &&
            (!cat_playerHasCompletedShip(sMothershipDockingBay, player)))
        {
            return(FALSE);
        }
    }
    // CAT: nothing external is built while the current player's sMothership is hyperspacing (the exe tests
    //      universe.curPlayerPtr, not player)
    statinfo = GetShipStaticInfo(type, player->race);
    if (statinfo->externalBuild)
    {
        mothership = universe.curPlayerPtr->PlayerMothership;
        if ((mothership != NULL) && (mothership->shiptype == sMothership) && (mothership->command != NULL) &&
            (mothership->command->ordertype.order == COMMAND_MP_HYPERSPACING))
        {
            return(FALSE);
        }
    }

    cat_rmTechSetAnd(needAndHave, need, research->HasTechnology);
    if ((needAndHave[CAT_TECHSET_FLAG] != need[CAT_TECHSET_FLAG]) || (needAndHave[CAT_TECHSET_FLAG] != CAT_TECHSET_ALL))
    {
        for (i = 0; i < NumTechnologies; i++)
        {
            if (needAndHave[i] != need[i])
            {
                return(FALSE);
            }
        }
    }
    return(TRUE);
}


/*-----------------------------------------------------------------------------
    Name        : rmFindFreeLab
    Description : finds a free lab
    Inputs      : player, tech (CAT: a Sect technology is researched in the lab its module
                  gives, cat_techLabType; TECH_NONE accepts any lab; a Beast player has lab 0)
    Outputs     : ResearchLab, or -1
    Return      : sdword
----------------------------------------------------------------------------*/
sdword rmFindFreeLab(Player *player, TechnologyType tech)
{
    sdword       labindex;
    ResearchLab *lab;

    if (player->race == RACE_Beast)
    {
        if (player->researchinfo.researchlabs[0].labstatus == LS_NORESEARCHITEM)
        {
            if ( (rmGUIActive) && (player->aiPlayer == NULL) )
                 rmUpdateTechList();
            return (0);
        }
        return (-1);
    }
    for (labindex=0;labindex<NUM_RESEARCHLABS;labindex++)
    {
        lab = &player->researchinfo.researchlabs[labindex];
        if ((lab->labstatus==LS_NORESEARCHITEM) && ((tech == TECH_NONE) || (cat_techLabType[tech] == labindex)))
        {
            if ( (rmGUIActive) && (player->aiPlayer == NULL) )
                 rmUpdateTechList();
            return (labindex);
        }
    }
    return (-1);
}


/*-----------------------------------------------------------------------------
    Name        : rmClearResearchlab
    Description : Clears the specified labs research status.
    Inputs      : lab to clear, and player
    Outputs     : none
    Return      : void
----------------------------------------------------------------------------*/
void rmClearResearchlab(Player *player, sdword labnumber)
{
    ResearchLab   *lab = &player->researchinfo.researchlabs[labnumber];
    ResearchTopic *topic = lab->topic;

    // CAT: no numlabsresearching; a lab waiting for its item (LS_RESEARCHITEMSOON) goes back to free (0x004f28f0)
    if (lab->labstatus==LS_RESEARCHITEM)
    {
        lab->topic = NULL;
        listDeleteNode(&topic->link);
        lab->labstatus = LS_NORESEARCHITEM;
        if ((rmGUIActive) && (player == universe.curPlayerPtr)) rmUpdateTechList();
    }
    else if (lab->labstatus==LS_RESEARCHITEMSOON)
    {
        lab->labstatus = LS_NORESEARCHITEM;
    }
}


/*-----------------------------------------------------------------------------
    Name        : rmAssignPlayersLabToResearch
    Description : assigns a lab to a certain research item
    Inputs      : Lab number, research topic number
    Outputs     : none
    Return      : void
----------------------------------------------------------------------------*/
void rmAssignPlayersLabToResearch(Player *player, sdword labnumber, TechnologyType tech)
{
    ResearchTopic *topic;

    // CAT: a Beast player researches one topic at a time (0x004f2960)
    if ((player->race == RACE_Beast) && (player->researchinfo.listoftopics.num != 0))
    {
        return;
    }
    if ((topic=Researching(player, tech))!=NULL)
    {
        player->researchinfo.researchlabs[labnumber].labstatus = LS_RESEARCHITEM;
        player->researchinfo.researchlabs[labnumber].topic = topic;
    }
    else
    {
        topic = (ResearchTopic *)memAlloc(sizeof(ResearchTopic),"ResearchTopic",NonVolatile);
        memset(topic, 0, sizeof(ResearchTopic));
        listAddNode(&player->researchinfo.listoftopics, &topic->link, topic);
        player->researchinfo.researchlabs[labnumber].labstatus = LS_RESEARCHITEM;
        player->researchinfo.researchlabs[labnumber].topic = topic;
        topic->techresearch       = tech;
        topic->timeleft           = (real32)player->researchinfo.techstat->TimeToComplete[tech];
    }

    if ((rmGUIActive) && (player == universe.curPlayerPtr)) rmUpdateTechList();
}

/*-----------------------------------------------------------------------------
    Name        : rmUpdateResearch
    Description : updates all research
    Inputs      : none
    Outputs     : none
    Return      : void
----------------------------------------------------------------------------*/
void rmUpdateResearch(void)
{
    sdword              index, labindex;
    udword              i;
    ResearchLab        *lab;
    PlayerResearchInfo *research;
    ResearchTopic      *topic;
    Node               *walk;
    LinkedList          deletelist;
    Player             *player;
    ShipRace            race;
    bool32              shipcanbuild[TOTAL_STD_SHIPS];
    sdword              speech;

    listInit(&deletelist);

    // search through list of players
    for (index=0;index<universe.numPlayers;index++)
    {
        player = &universe.players[index];
        research = &player->researchinfo;

        if (research->CanDoResearch)
        {
            walk = research->listoftopics.head;

            // walk through list of topics being researched
            while (walk != NULL)
            {
                topic = (ResearchTopic *)listGetStructOfNode(walk);
                // CAT: one second per tick, 8 in the tutorial, and a Beast mothership's research factor
                //      (rmUpdateResearch 0x004f2a20; HW1 used labdec[numlabsresearching])
                if (tutorial == TUTORIAL_ONLY)
                {
                    topic->timeleft -= 8.0f;
                }
                else if ((player->race == RACE_Beast) && (player->PlayerMothership != NULL))
                {
                    topic->timeleft -= cat_beastMothershipResearchFactor(player->PlayerMothership);
                }
                else
                {
                    topic->timeleft -= 1.0f;
                }
                if (topic->timeleft < 0)
                {
                    // CAT: the player's own race's types only; a Beast type the player has not enabled is "can't"
                    race = player->race;
                    for (i = FirstShipTypeOfRace[race]; i < LastShipTypeOfRace[race]; i++)
                    {
                        if ((race == RACE_Beast) && (research->catShipTypeEnabled[i] == 0))
                        {
                            shipcanbuild[i] = FALSE;
                        }
                        else
                        {
                            shipcanbuild[i] = rmCanBuildShip(player, i, FALSE);
                        }
                        race = player->race;
                    }

                    // CAT: research completed (0x2001033) with the technology linked to it (0x20010bc), and a
                    //      message for the current player
                    speech = speechEventQueue(NULL, 0x2001033, -1, -1, 0, index, -1, 5.0f, -1);
                    if (speech != -1)
                    {
                        speechEventQueue(NULL, 0x20010bc, topic->techresearch, -1, 0, index, speech, 5.0f, -1);
                    }
                    if (index == universe.curPlayerIndex)
                    {
                        // CAT: the exe's clCommandMessage takes a colour (0xffff0000, rmUpdateResearch 0x004f2a20) [audit: the
                        //      stale CAT-TODO closed; the hud port passes it]
                        clCommandMessage(strGetString(strResearchComplete), 0xffff0000);
                    }

                    topic->timeleft = 0;

                    cat_rmTechSetAdd(research->HasTechnology, topic->techresearch);
                    cat_playerUpdateUpgradesFromResearch(player);

                    listRemoveNode(&topic->link);
                    listAddNode(&deletelist,&topic->link,topic);

                    // CAT: a Beast player only has lab 0 (0x004f2a20)
                    for (labindex=0;labindex<((player->race == RACE_Sect) ? NUM_RESEARCHLABS : 1);labindex++)
                    {
                        lab = &research->researchlabs[labindex];
                        if (lab->topic==topic)
                        {
                            lab->labstatus = LS_NORESEARCHITEM;
                            lab->topic     = NULL;
                            // CAT: every lab of the GUI is cleared (rmClearAllLabs; HW1 cleared one)
                            if ((rmGUIActive) && (universe.curPlayerPtr == player))
                                rmClearAllLabs(NULL, NULL);
                        }
                    }

                    race = player->race;
                    for (i = FirstShipTypeOfRace[race]; i < LastShipTypeOfRace[race]; i++)
                    {
                        if (((race != RACE_Beast) || (research->catShipTypeEnabled[i] != 0)) &&
                            (shipcanbuild[i] != rmCanBuildShip(player, i, FALSE)))
                        {
                            speechEventQueue(NULL, 0x2001034, i, -1, 0, index, -1, 5.0f, -1);
                            if ((cmActive) && (universe.curPlayerPtr == player))
                            {
                                cmUpdateShipsAvailable();
                            }
                        }
                        race = player->race;
                    }

                    if (singlePlayerGame && (index == 0))
                    {
                        tmTechForSale[topic->techresearch] = TM_TECH_IS_ALREADY_OWNED;
                    }

                    if ((rmGUIActive) && (universe.curPlayerPtr == player))
                        rmUpdateTechList();
                }

                walk = walk->next;
            }

            listDeleteAll(&deletelist);
        }
    }
}

/*-----------------------------------------------------------------------
   Name        : rmAllTechRequiredforTech
   Description : ORs all of the technology needed to research a tech into out
                 (CAT 0x004f2db0: the set version of HW1's mask walk; the
                 row's own entries reach out through the recursion's copy)
   Inputs      : out, player, tech
   Outputs     : out
   Return      :
-----------------------------------------------------------------------*/
void rmAllTechRequiredforTech(sdword *out, Player *player, TechnologyType tech)
{
    CatTechSet  depend;
    sdword     *row;
    sdword      i, j;

    cat_rmTechSetCopy(depend, player->researchinfo.techstat->TechNeededToResearch[tech]);

    for (i = 0; i < NumTechnologies; i++)
    {
        row = player->researchinfo.techstat->TechNeededToResearch[tech];
        if ((row[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL) || (row[i] == 1))
        {
            rmAllTechRequiredforTech(depend, player, i);
            if ((out[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL) || (depend[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL))
            {
                for (j = 0; j < CAT_TECHSET_SIZE; j++)
                {
                    out[j] = 0;
                }
                out[CAT_TECHSET_FLAG] = CAT_TECHSET_ALL;
            }
            else
            {
                for (j = 0; j < NumTechnologies; j++)
                {
                    out[j] = (sdword)((udword)out[j] | (udword)depend[j]);
                }
                out[CAT_TECHSET_FLAG] = 0;
            }
        }
    }
}

// CAT: the body shared by rmTechRequiredForShip 0x004f2e90 and cat_rmTechRequiredForShipUpgrade 0x004f30e0: the
//      number of technologies in need's closure the player does not have (every technology when the closure is "all")
static sdword cat_rmCountTechRequired(Player *player, sdword *need)
{
    PlayerResearchInfo *research = &player->researchinfo;
    CatTechSet needed, closure, nothave, depend;
    sdword *row;
    sdword i, j, count = 0;

    cat_rmTechSetCopy(needed, need);
    for (i = 0; i < NumTechnologies; i++)
    {
        closure[i] = 0;
    }
    closure[CAT_TECHSET_FLAG] = 0;
    for (i = 0; i < NumTechnologies; i++)
    {
        if ((need[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL) || (need[i] == 1))
        {
            row = research->techstat->TechNeededToResearch[i];
            cat_rmTechSetCopy(depend, row);
            for (j = 0; j < NumTechnologies; j++)
            {
                if ((row[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL) || (row[j] == 1))
                {
                    rmAllTechRequiredforTech(depend, player, j);
                    if ((closure[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL) || (depend[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL))
                    {
                        cat_rmTechSetClear(closure);
                        closure[CAT_TECHSET_FLAG] = CAT_TECHSET_ALL;
                    }
                    else
                    {
                        sdword k;
                        for (k = 0; k < NumTechnologies; k++)
                        {
                            closure[k] = (sdword)((udword)closure[k] | (udword)depend[k]);
                        }
                        closure[CAT_TECHSET_FLAG] = 0;
                    }
                }
            }
            if ((needed[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL) || (closure[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL))
            {
                cat_rmTechSetClear(needed);
                needed[CAT_TECHSET_FLAG] = CAT_TECHSET_ALL;
            }
            else
            {
                for (j = 0; j < NumTechnologies; j++)
                {
                    needed[j] = (sdword)((udword)needed[j] | (udword)closure[j]);
                }
                needed[CAT_TECHSET_FLAG] = 0;
            }
        }
    }
    // the technologies the player does not have (nothing when the player has them all)
    if (research->HasTechnology[CAT_TECHSET_FLAG] == 0)
    {
        for (i = 0; i < NumTechnologies; i++)
        {
            nothave[i] = (research->HasTechnology[i] == 0);
        }
        nothave[CAT_TECHSET_FLAG] = 0;
    }
    else
    {
        cat_rmTechSetClear(nothave);
    }
    cat_rmTechSetAnd(needed, needed, nothave);
    for (i = 0; i < NumTechnologies; i++)
    {
        if ((needed[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL) || (needed[i] == 1))
        {
            count++;
        }
    }
    return count;
}

/*-----------------------------------------------------------------------------
    Name        : rmTechRequiredForShip
    Description : This function returns the number of research topics required
                  to build the ship specified
    Inputs      : player and shiptype
    Outputs     : number of research topics required
    Return      : sdword
----------------------------------------------------------------------------*/
sdword rmTechRequiredForShip(Player *player, ShipType type)
{
    return cat_rmCountTechRequired(player, player->researchinfo.techstat->TechNeededToBuildShip[type]);
}

// CAT: 0x004f30e0: the same count over the technologies that upgrade the ship type
sdword cat_rmTechRequiredForShipUpgrade(Player *player, ShipType type)
{
    return cat_rmCountTechRequired(player, player->researchinfo.catShipUpgradeTechs[type]);
}

// CAT: 0x004f3320: a computer player researches tech if it may and does not have it
void cat_rmAIResearchTech(Player *player, TechnologyType tech)
{
    sdword labnum;

    if ((player->researchinfo.HasTechnology[CAT_TECHSET_FLAG] != CAT_TECHSET_ALL) &&
        (player->researchinfo.HasTechnology[tech] != 1))
    {
        labnum = rmFindFreeLab(player, tech);
        if (labnum != -1)
        {
            clWrapResearch(FALSE, player->playerIndex, labnum, tech);
        }
    }
}

// CAT: the research pass of rmResearchTechForShip 0x004f3380 over one need set: closure of need minus what the player
//      has; every needed technology whose own requirements are met goes to a free lab of its type (clWrapResearch)
static bool32 cat_rmResearchNeededTechs(Player *player, sdword *need, sdword *needed, bool32 *researching)
{
    PlayerResearchInfo *research = &player->researchinfo;
    CatTechSet closure, nothave, canget, missing;
    sdword i, count = 0, lab;
    TechnologyType tech;

    cat_rmTechSetCopy(needed, need);
    cat_rmTechSetClear(closure);
    for (i = 0; i < NumTechnologies; i++)
    {
        if ((need[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL) || (need[i] == 1))
        {
            rmAllTechRequiredforTech(closure, player, i);
            cat_rmTechSetOr(needed, needed, closure);
        }
    }
    cat_rmTechSetNot(nothave, research->HasTechnology);
    cat_rmTechSetAnd(needed, needed, nothave);
    for (i = 0; i < NumTechnologies; i++)
    {
        if ((needed[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL) || (needed[i] == 1))
        {
            count++;
        }
    }
    if (count <= 0)
    {
        return FALSE;
    }

    // the same closure again, into canget (the exe recomputes it)
    cat_rmTechSetCopy(needed, need);
    cat_rmTechSetClear(closure);
    for (i = 0; i < NumTechnologies; i++)
    {
        if ((need[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL) || (need[i] == 1))
        {
            rmAllTechRequiredforTech(closure, player, i);
            cat_rmTechSetOr(needed, needed, closure);
        }
    }
    cat_rmTechSetNot(nothave, research->HasTechnology);
    cat_rmTechSetAnd(canget, needed, nothave);

    for (tech = 0; tech < NumTechnologies; tech++)
    {
        if ((canget[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL) || (canget[tech] == 1))
        {
            if (research->HasTechnology[CAT_TECHSET_FLAG] == 0)
            {
                for (i = 0; i < NumTechnologies; i++)
                {
                    nothave[i] = (research->HasTechnology[i] == 0);
                }
                nothave[CAT_TECHSET_FLAG] = 0;
            }
            else
            {
                cat_rmTechSetClear(nothave);
            }
            cat_rmTechSetAnd(missing, research->techstat->TechNeededToResearch[tech], nothave);
            if (missing[CAT_TECHSET_FLAG] != CAT_TECHSET_ALL)
            {
                for (i = 0; i < NumTechnologies; i++)
                {
                    if (missing[i] > 0)
                    {
                        break;
                    }
                }
                if (i >= NumTechnologies)
                {
                    lab = rmFindFreeLab(player, tech);
                    if (lab != -1)
                    {
                        clWrapResearch(FALSE, player->playerIndex, lab, tech);
                        *researching = TRUE;
                    }
                }
            }
        }
    }
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : rmResearchTechForShip
    Description : This function researches technology required to build a ship.
    Inputs      : player and shiptype
    Outputs     : none
    Return      : void
----------------------------------------------------------------------------*/
bool32 rmResearchTechForShip(struct Player *player, ShipType type)
{
    PlayerResearchInfo *research = &player->researchinfo;
    CatTechSet needed;
    bool32 researching = FALSE;

    // CAT (0x004f3380): a Beast type the player has not enabled is not researched; the orders go through
    //      clWrapResearch (HW1 assigned labs directly); when nothing is needed to build the ship, the technologies
    //      that upgrade it (catShipUpgradeTechs) are researched instead
    if ((player->race == RACE_Beast) && (research->catShipTypeEnabled[type] == 0))
    {
        return FALSE;
    }
    if (!cat_rmResearchNeededTechs(player, research->techstat->TechNeededToBuildShip[type], needed, &researching))
    {
        cat_rmResearchNeededTechs(player, research->catShipUpgradeTechs[type], needed, &researching);
    }
    return researching;
}

/*-----------------------------------------------------------------------------
    Name        : rmGiveTechToPlayerByName
    Description : Gives player technology 'techName' and all previous technologies
                  needed to get it
    Inputs      : player - player to get tech
                  techName - name of technology as defined in
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rmGiveTechToPlayerByName(struct Player *player, char *techName)
{
    rmGiveTechToPlayerByType(player,StrToTechType(techName));
}

/*-----------------------------------------------------------------------------
    Name        : rmGiveTechToPlayerByType
    Description : Gives player technology 'techName' and all previous technologies
                  needed to get it
    Inputs      : player - player to get tech
                  techName - name of technology as defined in
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rmGiveTechToPlayerByType(struct Player *player, TechnologyType techtype)
{
    PlayerResearchInfo *research = &player->researchinfo;
    CatTechSet          missing;
    sdword             *need;
    sdword              i, tech;

    // CAT (0x004f38e0): missing = need XOR (need AND has); the highest missing dependency is given first, the set
    //      refreshes the upgrades each time, then the technology itself
    need = research->techstat->TechNeededToResearch[techtype];
    while(1)
    {
        cat_rmTechSetAnd(missing, need, research->HasTechnology);
        for (i = 0; i < NumTechnologies; i++)
        {
            missing[i] = (sdword)((udword)missing[i] ^ (udword)need[i]);
        }
        missing[CAT_TECHSET_FLAG] = 0;
        for (tech = NumTechnologies - 1; tech >= 0; tech--)
        {
            if (missing[tech] == 1)
            {
                break;
            }
        }
        if (tech <= 0)
        {
            break;
        }
        cat_rmTechSetAdd(research->HasTechnology, tech);
        cat_playerUpdateUpgradesFromResearch(player);
    }
    cat_rmTechSetAdd(research->HasTechnology, techtype);
    cat_playerUpdateUpgradesFromResearch(player);
}

/*-----------------------------------------------------------------------------
    Name        : rmInitializeResearchStatics
    Description : Reset the research statics for a given player
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rmInitializeResearchStatics(struct Player *player)
{
    // CAT: three tables (0x004f39b0): the Sect single-player and multiplayer tables, the Beast table for both; no
    //      MG_CarrierIsMothership tables
    if (!singlePlayerGame)
    {
        if (player->race==RACE_Sect)
        {
            player->researchinfo.techstat   = &cat_MultiPlayerSectTechStatic;
        }
        else if (player->race==RACE_Beast)
        {
            player->researchinfo.techstat   = &cat_BeastTechStatic;
        }
    }
    else
    {
        if (player->race==RACE_Sect)
        {
            player->researchinfo.techstat   = &cat_SinglePlayerSectTechStatic;
        }
        else if (player->race==RACE_Beast)
        {
            player->researchinfo.techstat   = &cat_BeastTechStatic;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : rmInitializeResearchStruct
    Description : initializes research structure for player
    Inputs      : Player
    Outputs     : none
    Return      : void
----------------------------------------------------------------------------*/
void rmInitializeResearchStruct(Player *player, bool32 candoresearch, sdword techlevel)
{
    PlayerResearchInfo *research = &player->researchinfo;
    sdword index, i;
    ShipType type;
    sdword *techset;
    sdword *need;

    research->CanDoResearch = candoresearch;

    cat_rmTechSetInit(research->HasTechnology, techlevel);

    rmInitializeResearchStatics(player);

    for (index=0;index<NUM_RESEARCHLABS;index++)
    {
        research->researchlabs[index].labstatus = LS_NORESEARCHSHIP;
        research->researchlabs[index].topic     = NULL;
    }

    // CAT (0x004f3a00): a Sect player's upgrade technologies per type (types 0..0x20); a Beast player has lab 0 open
    //      and gets per-type upgrade sets and enabled flags for types 0x21..0x3A
    if (player->race == RACE_Sect)
    {
        for (type = 0; type < 0x21; type++)
        {
            cat_shipTypeGetUpgradeTechs(type, research->catShipUpgradeTechs[type]);
        }
    }
    else
    {
        research->researchlabs[0].labstatus = LS_NORESEARCHITEM;
        for (index = 0; index + 0x22 < 0x3c; index++)
        {
            type = (ShipType)(0x21 + index);
            techset = research->catShipUpgradeTechs[type];
            cat_rmTechSetClear(techset);
            switch (index)
            {
                case 0x0:                                   // bRecon
                    cat_rmTechSetAdd(techset, TECH_ADVANCED_SENSORS);
                    research->catShipTypeEnabled[type] = 1;
                    break;
                case 0xb:                                   // bIonArrayFrigate
                    cat_rmTechSetAdd(techset, TECH_CLOAKING);
                    research->catShipTypeEnabled[type] = 1;
                    break;
                case 0xd:                                   // bHeavyCruiser
                case 0x19:                                  // bCruiseMissile
                    cat_rmTechSetAdd(techset, TECH_INFECT);
                    research->catShipTypeEnabled[type] = 1;
                    break;
                case 0xe:                                   // bCarrier
                case 0x14:                                  // bMothership
                    cat_rmTechSetAdd(techset, TECH_CRYSTAL_HARVEST);
                    cat_rmTechSetAdd(techset, TECH_INFECT);
                    research->catShipTypeEnabled[type] = 1;
                    break;
                case 0x10:                                  // bWorker
                    cat_rmTechSetAdd(techset, TECH_SALVAGE);
                    research->catShipTypeEnabled[type] = 1;
                    break;
                case 0x12:                                  // bProcessor
                    research->catShipTypeEnabled[type] = 1;
                    break;
                default:
                    // enabled when the type needs any technology to be built
                    need = research->techstat->TechNeededToBuildShip[type];
                    research->catShipTypeEnabled[type] = 0;
                    if (need[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL)
                    {
                        research->catShipTypeEnabled[type] = 1;
                    }
                    else
                    {
                        for (i = 0; i < NumTechnologies; i++)
                        {
                            if (need[i] > 0)
                            {
                                research->catShipTypeEnabled[type] = 1;
                                break;
                            }
                        }
                    }
                    break;
            }
        }
    }

    listInit(&research->listoftopics);
    cat_playerUpdateUpgradesFromResearch(player);
}


// CAT: the exe zeroes the three tables' TechNeededToResearch (0x1AE8 bytes each) and the race halves of
//      TechNeededToBuildShip (Sect types 0..0x20 of the Sect tables, Beast types 0x21..0x3B of the Beast table), then
//      reads ResearchDepend.script (rmAPIStartup 0x004f3c00 / rmResetStaticInfo 0x004f3d30)
static void cat_rmStaticsClearAndLoad(void)
{
    memset(cat_BeastTechStatic.TechNeededToResearch, 0, sizeof(cat_BeastTechStatic.TechNeededToResearch));
    memset(cat_MultiPlayerSectTechStatic.TechNeededToResearch, 0, sizeof(cat_MultiPlayerSectTechStatic.TechNeededToResearch));
    memset(cat_SinglePlayerSectTechStatic.TechNeededToResearch, 0, sizeof(cat_SinglePlayerSectTechStatic.TechNeededToResearch));
    memset(cat_MultiPlayerSectTechStatic.TechNeededToBuildShip, 0, 0x21 * sizeof(CatTechSet));
    memset(cat_SinglePlayerSectTechStatic.TechNeededToBuildShip, 0, 0x21 * sizeof(CatTechSet));
    memset(cat_BeastTechStatic.TechNeededToBuildShip[0x21], 0, (0x3c - 0x21) * sizeof(CatTechSet));

    scriptSet(NULL,"ResearchDepend.script",TechnologyDependancies);

    rmSetPrintList(RM_SPRS,&cat_SinglePlayerSectTechStatic);
    rmSetPrintList(RM_MPRS,&cat_MultiPlayerSectTechStatic);
    rmSetPrintList(RM_MPRB,&cat_BeastTechStatic);
}

/*-----------------------------------------------------------------------------
    Name        : rmAPIStartup
    Description : starts up the API for the research manager loading all dependancies
    Inputs      : none
    Outputs     : none
    Return      : void
----------------------------------------------------------------------------*/
void rmAPIStartup(void)
{
    cat_rmStaticsClearAndLoad();
    rmGUIStartup();
}


/*-----------------------------------------------------------------------------
    Name        : rmAPIShutdown
    Description : shut down the API for the research manager
    Inputs      : none
    Outputs     : none
    Return      : void
----------------------------------------------------------------------------*/
void rmAPIShutdown(void)
{
    rmGUIShutdown();
}


/*-----------------------------------------------------------------------------
    Name        : rmEnableShipFunction
    Description : disables a ship so that a player cannot build it
    Inputs      : race - race of shipt to disable
                  ship - type of ship to disable
                  bEnable - TRUE to enable, FALSE to disable
    Outputs     : none
    Return      : none
----------------------------------------------------------------------------*/
void rmEnableShip(ShipRace race, ShipType ship, bool32 bEnabled)
{
    // CAT (0x004f3ca0): the set's flag word: 0 enabled, -2 disabled, for the standard types and the Nomad moon
    //      (0x85, always disabled in the multiplayer Sect table)
    sdword techlevel = bEnabled ? 0 : CAT_TECHSET_DISABLED;

    if ((ship < 0x3c) || (ship == 0x85))
    {
        if (race == RACE_Sect)
        {
            cat_rmTechSetInit(cat_SinglePlayerSectTechStatic.TechNeededToBuildShip[ship], techlevel);
            if (ship == 0x85)
            {
                techlevel = CAT_TECHSET_DISABLED;
            }
            cat_rmTechSetInit(cat_MultiPlayerSectTechStatic.TechNeededToBuildShip[ship], techlevel);
        }
        else if (race == RACE_Beast)
        {
            cat_rmTechSetInit(cat_BeastTechStatic.TechNeededToBuildShip[ship], techlevel);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : rmResetStaticInfo
    Description : resets the static info to what was in the script file.
    Inputs      : none
    Outputs     : none
    Return      : void
----------------------------------------------------------------------------*/
void rmResetStaticInfo(void)
{
    cat_rmStaticsClearAndLoad();
}

/*=============================================================================
    CAT: the research modules of a Sect mothership (0x004f4530 - 0x004f4790)
=============================================================================*/

// CAT: module ship type -> lab index (cat_rmResearchModuleActivated / cat_rmResearchModuleDied), -1 for any other type
static sdword cat_rmModuleLab(ShipType moduleType)
{
    switch (moduleType)
    {
        case sMothershipSpecial:        return MODULE_Advanced_Engineering;     // 0x13 -> 3
        case sMothershipArmour:         return MODULE_Armour;                   // 0x15 -> 1
        case sMothershipMicro:          return MODULE_Engineering;              // 0x16 -> 2
        case sMothershipWeapons:        return MODULE_Weapon;                   // 0x17 -> 4
        case sMothershipDockingBay:     return MODULE_Hangar;                   // 0x18 -> 0
        default:                        return -1;
    }
}

// 0x004f4530: a finished module opens its lab (and says so for the current player)
void cat_rmResearchModuleActivated(Player *player, ShipType moduleType, bool32 announce)
{
    sdword lab = cat_rmModuleLab(moduleType);

    if (lab == -1)
    {
        return;
    }
    if ((announce) && (universe.curPlayerPtr == player) &&
        ((lab != MODULE_Hangar) || (!singlePlayerGame) || (singlePlayerGameInfo.currentMission != 4)))
    {
        speechEventQueue(NULL, 0x2001030, lab, -1, 0, player->playerIndex, -1, 5.0f, -1);
        // CAT (M2-R5 hud): and the overlay message (cat_ioMessageAdd 0x004ac940, type 2: the research)
        cat_ioMessageAdd(strGetString(strResearchModuleActivated), 0xffff0000, 2, NULL);
    }
    if (player->researchinfo.researchlabs[lab].labstatus == LS_NORESEARCHSHIP)
    {
        player->researchinfo.researchlabs[lab].labstatus = LS_NORESEARCHITEM;
        if ((rmGUIActive) && (player == universe.curPlayerPtr))
        {
            rmUpdateTechList();
        }
    }
}

// 0x004f4620 / 0x004f4700 (lab 0): a lost module closes its lab, dropping the topic it was researching
static void cat_rmLabLost(Player *player, sdword lab)
{
    ResearchLab *researchlab = &player->researchinfo.researchlabs[lab];

    if (researchlab->labstatus != LS_NORESEARCHSHIP)
    {
        if (researchlab->labstatus == LS_RESEARCHITEM)
        {
            ResearchTopic *topic = researchlab->topic;
            researchlab->topic = NULL;
            listDeleteNode(&topic->link);
            researchlab->labstatus = LS_NORESEARCHITEM;
            if ((rmGUIActive) && (player == universe.curPlayerPtr))
            {
                rmUpdateTechList();
            }
        }
        else if (researchlab->labstatus == LS_RESEARCHITEMSOON)
        {
            researchlab->labstatus = LS_NORESEARCHITEM;
        }
    }
    researchlab->labstatus = LS_NORESEARCHSHIP;
    if ((rmGUIActive) && (player == universe.curPlayerPtr))
    {
        rmUpdateTechList();
    }
}

void cat_rmResearchModuleDied(Player *player, ShipType moduleType)
{
    sdword lab = cat_rmModuleLab(moduleType);

    if (lab != -1)
    {
        cat_rmLabLost(player, lab);
    }
}

// 0x004f4700: a Beast mothership's lab is lab 0
void cat_rmMothershipLabLost(Player *player)
{
    cat_rmLabLost(player, 0);
}

// 0x004f4780
bool32 cat_playerShipTypeEnabled(Player *player, ShipType type)
{
    return player->researchinfo.catShipTypeEnabled[type];
}

/*=============================================================================
    Script callbacks
=============================================================================*/

// CAT: the body shared by cat_rsShipDependSetCB 0x004f3dc0, cat_rsMPRBShipDependSetCB 0x004f4430 and
//      cat_rsTechDependSetCB 0x004f3ec0: "<name> tech, tech, ..." fills the set of row <index> of dataToFillIn
static void cat_rsDependSetCB(char *field, void *dataToFillIn, bool32 shipRow)
{
    CatTechSet techset;
    char temp[256];
    sdword index, row;
    TechnologyType tech;

    cat_rmTechSetClear(techset);

    for (index=0;(*field != ' ') && (*field != '\t');index++,field++)
    {
        temp[index]=*field;
    }
    temp[index]=0;

    while (((*field == ' ') || (*field == '\t')) && (*field != 0))
        field++;

    if (*field == 0) return;

    RemoveCommasFromString(field);

    row = shipRow ? (sdword)StrToShipType(temp) : StrToNum(techtypeinfo, temp);

    while (*field != 0)
    {
        for (index=0;(*field != ' ')&&(*field != 0)&&(*field != '\t');index++,field++)
        {
            temp[index]=*field;
        }
        temp[index]=0;

        tech = StrToNum(techtypeinfo, temp);
        cat_rmTechSetAdd(techset, tech);

        while ((*field == ' ') || (*field == '\t'))
            field++;
    }

    cat_rmTechSetCopy(((CatTechSet *)dataToFillIn)[row], techset);
}

void cat_rsShipDependSetCB(char *directory, char *field, void *dataToFillIn)
{
    (void)directory;
    cat_rsDependSetCB(field, dataToFillIn, TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : cat_rsMPRBShipDependSetCB
    Description : CAT: new (0x004f4430): the multiplayer Beast table's ship rows ("MPRBShipDepend"); the exe's body is
                  cat_rsShipDependSetCB's, a separate copy
----------------------------------------------------------------------------*/
void cat_rsMPRBShipDependSetCB(char *directory, char *field, void *dataToFillIn)
{
    (void)directory;
    cat_rsDependSetCB(field, dataToFillIn, TRUE);
}

void cat_rsTechDependSetCB(char *directory, char *field, void *dataToFillIn)
{
    (void)directory;
    cat_rsDependSetCB(field, dataToFillIn, FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : cat_rmCollectTopicsByLab                          0x004f4380
    Description : CAT (M2-R5 hud): the player's research topics in technology order, one line per technology (the
                  first topic of it found): the main overlay's research list (cat_moResearchDraw 0x004a9a00). The total
                  time is the current player's TimeToComplete (the exe reads universe.curPlayerPtr's tech statics).
----------------------------------------------------------------------------*/
sdword cat_rmCollectTopicsByLab(struct Player *player, CatRmTopic *topics)
{
    static sdword lineOfTech[NumTechnologies];      // 0x00a57648
    sdword lines = 0, tech;
    Node *node;
    ResearchTopic *topic;

    for (tech = 0; tech < NumTechnologies; tech++)
    {
        lineOfTech[tech] = -1;
    }
    for (tech = 0; tech < NumTechnologies; tech++)
    {
        for (node = player->researchinfo.listoftopics.head; node != NULL; node = node->next)
        {
            topic = (ResearchTopic *)listGetStructOfNode(node);
            if (topic->techresearch == (TechnologyType)tech)
            {
                if (topic != NULL)
                {
                    CatRmTopic *line;
                    if (lineOfTech[tech] == -1)
                    {
                        lineOfTech[tech] = lines;
                        line = &topics[lines];
                        line->tech = (TechnologyType)tech;
                        lines++;
                    }
                    else
                    {
                        line = &topics[lineOfTech[tech]];
                    }
                    line->timeLeft = topic->timeleft;
                    line->totalTime = (sdword)(real32)universe.curPlayerPtr->researchinfo.techstat->TimeToComplete[tech];
                }
                break;
            }
        }
    }
    return lines;
}
