/*=============================================================================
    Name    : launchMgr.c
    Purpose : Logic for launch manager

    Created 5/18/1998 by ddunlop
    Copyright Relic Entertainment, Inc.  All rights reserved.

    CAT (M2-R6 managers): ported to Cataclysm's launch manager (cataclysm.exe 0x0049ccd0..0x0049fc70): two carriers,
        a Sect and a Beast screen, the Launch One / Construction / Research buttons, the factory name, the Sect
        mothership's seven module slots with their three systems each, the Beast mothership's systems radio buttons;
        no ship view selection, no taskbar or input lock-out
=============================================================================*/


#include <stdio.h>
#include <string.h>

#include "CommandLayer.h"
#include "CommandWrap.h"
#include "ConsMgr.h"
#include "Debug.h"
#include "FEColour.h"
#include "FEFlow.h"
#include "FEReg.h"
#include "font.h"
#include "FontReg.h"
#include "Globals.h"
#include "LaunchMgr.h"
#include "LinkedList.h"
#include "mainrgn.h"
#include "Memory.h"
#include "Mothership.h"
#include "mouse.h"
#include "ObjTypes.h"
#include "prim2d.h"
#include "render.h"
#include "Select.h"
#include "Sensors.h"
#include "ShipDefs.h"
#include "ShipNames.h"
#include "ShipSelect.h"
#include "ShipView.h"
#include "SoundEvent.h"
#include "SoundEventDefs.h"
#include "StringsOnly.h"
#include "texreg.h"
#include "Tutor.h"
#include "UIControls.h"
#include "Universe.h"

/*=============================================================================
    Defines:
=============================================================================*/

#define LM_FIGHTER_SPACER       TOTAL_NUM_SHIPS + 1
#define LM_CORVETTE_SPACER      TOTAL_NUM_SHIPS + 2
#define LM_SHIP_IMAGE_INSET     4


#define LM_VertSpacing      (fontHeight(" ") >> 1)

// CAT: six images per race, icon1/icon2 in _disabled/_off/_on states (lmLoadTextures 0x0049dd60: icon table
//      .data 0x00883508..0x00883520, image/texture tables 0x00a4471c/0x008834d0 of 2 x 6 entries)
#define NUM_LMTEXTURES      6
// CAT: fifteen launch-system button textures R1_Sys{1,2,3}_{disabled,off,off_mouse,off_focus,on}.lif, new in
//      Cataclysm (lmLoadTextures 0x0049dd60: names table .data 0x0088353c, images 0x00a4474c, textures 0x00883578)
#define CAT_NUM_LMSYSTEXTURES 15
#define CAT_LM_SYS_STATES     5         // per system: disabled, off, off_mouse, off_focus, on
#define CAT_LM_NUM_SYSTEMS    3

// CAT: two carrier buttons (lmCarrierX 0x00a44714..0x00a4471c; HW1 had four)
#define NUM_LMCARRIERS      2

// CAT: the Beast launch screen (lmLaunchBegin 0x0049e1e0: .data 0x00883924 / 0x008838f0)
#ifdef _WIN32
#define CAT_LM_BeastFIBFile         "FEMAN\\Beast_Launch_Manager.fib"
#else
#define CAT_LM_BeastFIBFile         "FEMAN/Beast_Launch_Manager.fib"
#endif
#define CAT_LM_BeastLaunchScreen    "Beast_Launch_Manager"

// CAT: the factory name font (lmStartup 0x0049dfb0: .data 0x00883478)
#define CAT_LM_FactoryNameFont      "EuroseWideHeavy_i13.hff"

// CAT: the count printed next to the docking bars (0x0049d6d9: 0xff000080)
#define CAT_LM_UsedTextColor        colRGB(128, 0, 0)

/*=============================================================================
    Data::
=============================================================================*/

bool32 lmActive = FALSE;                // CAT: 0x00a446e8

static sdword lmRenderEverythingCounter;    // CAT: 0x00a446e4
static bool32 catLmSensorsWereOpen;     // CAT: 0x00a446ec, the sensors manager was open when the screen opened
static sdword catLmScreensRace;         // CAT: 0x00a446f4, the race whose screens lmScreensHandle holds
static regionhandle catLmFactoryNameRegion = NULL;  // CAT: 0x00a44704, the LM_FactoryNameDraw region
static fonthandle catLmFactoryNameFont = 0;         // CAT: 0x00a44710
static Ship *catLmFactoryShip = NULL;   // CAT: 0x00c367e4, the factory whose name LM_FactoryNameDraw prints

// CAT: the module slots (0x00a4478c), filled by cat_lmFillInModuleSlots below and drawn by cat_lmModuleSlot1..7Draw
Ship *cat_lmModuleSlots[CAT_LM_NUM_MODULE_SLOTS];

// CAT: the module system button under the mouse (0x00883604 the slot's module type, 0x00883608 the system, -1 none)
static sdword catLmHoverModuleType = -1;
static sdword catLmHoverSystem = -1;

/*-----------------------------------------------------------------------------
    Name        : cat_lmFillInModuleSlots
    Description : CAT (M2-R4 ships-b): the launch screen's module slots of the local player's sMothership, from the
                  ships clamped to it: docking bay, weapons, armour, micro, special, the weakest support module, big
                  gun; then the empty slots are filled from the right (cat_lmFillInModuleSlots 0x0049e9f0; called by
                  lmLaunchBegin 0x0049e1e0 and cat_lmModuleSlot1Draw 0x0049f050)
    CAT (M2-R6 managers): slots 1 and 3 corrected to the exe's: sMothershipWeapons (0x17) 0x00a44790, sMothershipMicro
        (0x16) 0x00a44798
----------------------------------------------------------------------------*/
void cat_lmFillInModuleSlots(void)
{
    Ship *mothership = universe.curPlayerPtr->PlayerMothership;
    CatClampedList *clamped;
    Ship *module;
    sdword i, j;

    if ((mothership == NULL) || (mothership->shiptype != sMothership))
    {
        return;
    }
    for (i = 0; i < CAT_LM_NUM_MODULE_SLOTS; i++)
    {
        cat_lmModuleSlots[i] = NULL;
    }
    // CAT-FIX: the exe reads the list without testing clampInfo (a Sect mothership always has its modules clamped);
    //      here a mothership that was only created, not added (univAddShip makes the modules), has none
    if ((mothership->guidance.clampInfo == NULL) || (mothership->guidance.clampInfo->clampedObjs == NULL))
    {
        return;
    }
    clamped = mothership->guidance.clampInfo->clampedObjs;
    for (i = 0; i < clamped->numShips; i++)
    {
        module = clamped->ShipPtr[i];
        switch (module->shiptype)
        {
            case sMothershipDockingBay:     cat_lmModuleSlots[0] = module; break;
            case sMothershipWeapons:        cat_lmModuleSlots[1] = module; break;
            case sMothershipArmour:         cat_lmModuleSlots[2] = module; break;
            case sMothershipMicro:          cat_lmModuleSlots[3] = module; break;
            case sMothershipSpecial:        cat_lmModuleSlots[4] = module; break;
            case sMothershipSupport:
                if ((cat_lmModuleSlots[5] == NULL) || (module->targetable.health < cat_lmModuleSlots[5]->targetable.health))
                {
                    cat_lmModuleSlots[5] = module;
                }
                break;
            case sMothershipBigGun:         cat_lmModuleSlots[6] = module; break;
            default: break;
        }
    }
    for (i = 0; i < CAT_LM_NUM_MODULE_SLOTS - 1; i++)
    {
        if (cat_lmModuleSlots[i] == NULL)
        {
            for (j = i + 1; j < CAT_LM_NUM_MODULE_SLOTS; j++)
            {
                if (cat_lmModuleSlots[j] != NULL)
                {
                    cat_lmModuleSlots[i] = cat_lmModuleSlots[j];
                    cat_lmModuleSlots[j] = NULL;
                    break;
                }
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_lmModuleSlotsRemoveType
    Description : CAT: empties the module slots that hold a ship of the given type and packs the rest to the front,
                  keeping their order (cat_lmModuleSlotsRemoveType 0x0049e970)
    Inputs      : type - the ship type that leaves the slots
----------------------------------------------------------------------------*/
void cat_lmModuleSlotsRemoveType(ShipType type)
{
    sdword i, j;

    for (i = 0; i < CAT_LM_NUM_MODULE_SLOTS; i++)
    {
        if ((cat_lmModuleSlots[i] != NULL) && (cat_lmModuleSlots[i]->shiptype == type))
        {
            cat_lmModuleSlots[i] = NULL;
        }
    }
    // each empty slot but the last takes the next filled slot after it
    for (i = 0; i < CAT_LM_NUM_MODULE_SLOTS - 1; i++)
    {
        if (cat_lmModuleSlots[i] == NULL)
        {
            for (j = i + 1; j < CAT_LM_NUM_MODULE_SLOTS; j++)
            {
                if (cat_lmModuleSlots[j] != NULL)
                {
                    cat_lmModuleSlots[i] = cat_lmModuleSlots[j];
                    cat_lmModuleSlots[j] = NULL;
                    break;
                }
            }
        }
    }
}

bool32 lmPaletted;                      // CAT: 0x00c367e0

LaunchShipsAvailable shipsavailable[LM_CORVETTE_SPACER+1];     // CAT: 0x00c36340 (the exe's has 0x91 entries)

// CAT: the launch list in Cataclysm's order (PrintOrder 0x00883340: fighter spacer, 12 strike craft, corvette spacer,
//      9 corvettes and workers, -1)
#define LM_NUMSHIPS 23

sdword PrintOrder[LM_NUMSHIPS + 1] = {
    LM_FIGHTER_SPACER,

    sRecon,
    bRecon,
    sAcolyte,
    bAcolyte,
    sMimic,
    bInterceptor,
    bCloakedFighter,
    bAttackBomber,
    sLeech,
    sSentinel,
    bSentinel,
    dAcolyte,

    LM_CORVETTE_SPACER,

    sACV,
    bACV,
    sMCV,
    bCruiseMissile,
    bMultiGunCorvette,
    bHeavyCorvette,
    bMissileCorvette,
    sWorker,
    bWorker,

    -1, // sentinel value
};

// This is a bool32 that is TRUE if the ship is in the printlist. dynamically calculated
// based on the PrintOrder (CAT: 0x00c36800)
bool32 lmShipInList[TOTAL_NUM_SHIPS];


// callback functions for the launch GUI
void lmLaunch(char *string, featom *atom);
void lmAutoLaunchM(char *string, featom *atom);
void lmAutoLaunchC1(char *string, featom *atom);
void lmAutoLaunchC2(char *string, featom *atom);
void lmAutoLaunchC3(char *string, featom *atom);
void lmClose(char *string, featom *atom);
void lmLaunchAll(char *string, featom *atom);
void lmShipsToLaunchDraw(char *string, featom *atom);
void cat_lmConstruction(char *string, featom *atom);
void cat_lmResearch(char *string, featom *atom);
void cat_lmBeastSystemsSet(char *string, featom *atom);
void cat_lmLaunchOne(char *string, featom *atom);

void lmFighterUsedDraw(featom *atom, regionhandle region);
void lmCorvetteUsedDraw(featom *atom, regionhandle region);

void lmMotherShipDraw(featom *atom, regionhandle region);
void lmCarrier1Draw(featom *atom, regionhandle region);
void lmCarrier2Draw(featom *atom, regionhandle region);
void cat_lmFactoryNameDraw(featom *atom, regionhandle region);
void cat_lmModuleSlot1Draw(featom *atom, regionhandle region);
void cat_lmModuleSlot2Draw(featom *atom, regionhandle region);
void cat_lmModuleSlot3Draw(featom *atom, regionhandle region);
void cat_lmModuleSlot4Draw(featom *atom, regionhandle region);
void cat_lmModuleSlot5Draw(featom *atom, regionhandle region);
void cat_lmModuleSlot6Draw(featom *atom, regionhandle region);
void cat_lmModuleSlot7Draw(featom *atom, regionhandle region);

// CAT: the exe's tables (.data 0x008833a0: 12 callbacks; 0x00883408: 13 draw callbacks)
fecallback lmCallback[] =
{
    {lmLaunch,                  "LM_Launch"             },      // 0x0049ce30
    {lmAutoLaunchM,             "LM_AutoLaunchM"        },      // 0x0049cfd0
    {lmAutoLaunchC1,            "LM_AutoLaunchC1"       },      // 0x0049d140
    {lmAutoLaunchC2,            "LM_AutoLaunchC2"       },      // 0x0049d160
    {lmAutoLaunchC3,            "LM_AutoLaunchC3"       },      // 0x0049d180
    {lmClose,                   "LM_Close"              },      // 0x0049d250
    {lmLaunchAll,               "LM_LaunchAll"          },      // 0x0049d1a0
    {lmShipsToLaunchDraw,       "LM_ShipsToLaunch"      },      // 0x0049d4b0
    {cat_lmConstruction,        "LM_Construction"       },      // 0x0049ccd0
    {cat_lmResearch,            "LM_Research"           },      // 0x0049cd80
    {cat_lmBeastSystemsSet,     "LM_BeastSystemsSet"    },      // 0x0049fbb0
    {cat_lmLaunchOne,           "LM_LaunchOne"          },      // 0x0049cf10
    {NULL,                      NULL                    }
};

fedrawcallback lmDrawCallback[] =
{
    {lmFighterUsedDraw,         "LM_FighterUsed"        },      // 0x0049d5e0
    {lmCorvetteUsedDraw,        "LM_CorvetteUsed"       },      // 0x0049d700
    {lmMotherShipDraw,          "LM_MotherShipDraw"     },      // 0x0049db40
    {lmCarrier1Draw,            "LM_Carrier1Draw"       },      // 0x0049dc60
    {lmCarrier2Draw,            "LM_Carrier2Draw"       },      // 0x0049dce0
    {cat_lmFactoryNameDraw,     "LM_FactoryNameDraw"    },      // 0x0049d8d0
    {cat_lmModuleSlot1Draw,     "LM_Module_Slot1"       },      // 0x0049f050
    {cat_lmModuleSlot2Draw,     "LM_Module_Slot2"       },      // 0x0049f1c0
    {cat_lmModuleSlot3Draw,     "LM_Module_Slot3"       },      // 0x0049f330
    {cat_lmModuleSlot4Draw,     "LM_Module_Slot4"       },      // 0x0049f4a0
    {cat_lmModuleSlot5Draw,     "LM_Module_Slot5"       },      // 0x0049f610
    {cat_lmModuleSlot6Draw,     "LM_Module_Slot6"       },      // 0x0049f780
    {cat_lmModuleSlot7Draw,     "LM_Module_Slot7"       },      // 0x0049f8f0
    {NULL,                      NULL                    }
};


void lmReLaunch(Ship *newship);

//flag indicating loaded status of this screen
fibfileheader *lmScreensHandle = NULL;  // CAT: 0x00a446f0

regionhandle  lmBaseRegion = NULL;      // CAT: 0x00a446f8
ShipsInsideMe *shipsin = NULL;          // CAT: 0x00a446fc
Ship          *launchship = NULL;       // CAT: 0x00a44700

listwindowhandle lmListWindow=NULL;     // CAT: 0x00a44708

fonthandle lmShipListFont=0;            // CAT: 0x00a4470c

//colors for printing selections and stuff (CAT: .data 0x008834b8..0x008834cc; the exe draws with the last two only,
//      the first two stay tweakable)
color lmShipListTextColor       = LM_ShipListTextColor;
color lmShipSelectedTextColor   = LM_ShipSelectedTextColor;
color lmShip2ListTextColor       = LM_Ship2ListTextColor;
color lmShip2SelectedTextColor   = LM_Ship2SelectedTextColor;
color lmUsedColor               = LM_UsedColor;
color lmAvailableColor          = LM_AvailableColor;

Ship *lmCarrierX[NUM_LMCARRIERS] = { NULL, NULL };     // CAT: 0x00a44714


trhandle lmShipTexture[MAX_RACES][NUM_LMTEXTURES] =    // CAT: 0x008834d0
{
    {TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle},
    {TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle}
};

lifheader *lmShipImage[MAX_RACES][NUM_LMTEXTURES] =    // CAT: 0x00a4471c
{
    { NULL, NULL, NULL, NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL }
};


char *lmShipImagePaths[] =
{
#ifdef _WIN32
    "FEMan\\Construction_Manager\\Build_race1_",
    "FEMan\\Construction_Manager\\Build_race2_"
#else
    "FEMan/Construction_Manager/Build_race1_",
    "FEMan/Construction_Manager/Build_race2_"
#endif
};

char *lmShipIcons[] =
{
    // CAT: Cataclysm's names (table .data 0x00883508); HW1: "icon1.lif", "icon2.lif"
    "icon1_disabled.lif",
    "icon1_off.lif",
    "icon1_on.lif",
    "icon2_disabled.lif",
    "icon2_off.lif",
    "icon2_on.lif",
};

// CAT: launch-system button textures (lmLoadTextures 0x0049dd60; path .data 0x00883520)
#ifdef _WIN32
#define CAT_LM_SYS_PATH "FEMan\\Construction_Manager\\"
#else
#define CAT_LM_SYS_PATH "FEMan/Construction_Manager/"
#endif
char *catLmSysIcons[CAT_NUM_LMSYSTEXTURES] =
{
    "R1_Sys1_disabled.lif", "R1_Sys1_off.lif", "R1_Sys1_off_mouse.lif", "R1_Sys1_off_focus.lif", "R1_Sys1_on.lif",
    "R1_Sys2_disabled.lif", "R1_Sys2_off.lif", "R1_Sys2_off_mouse.lif", "R1_Sys2_off_focus.lif", "R1_Sys2_on.lif",
    "R1_Sys3_disabled.lif", "R1_Sys3_off.lif", "R1_Sys3_off_mouse.lif", "R1_Sys3_off_focus.lif", "R1_Sys3_on.lif"
};
trhandle catLmSysTexture[CAT_NUM_LMSYSTEXTURES] =
{
    TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle,
    TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle,
    TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle, TR_InvalidHandle
};
lifheader *catLmSysImage[CAT_NUM_LMSYSTEXTURES];


sdword lmCurrentSelect = 0;             // CAT: 0x00a44788

char lmShipListFontName[LM_FontNameLength] = LM_ShipListFont;     // CAT: 0x008835b4


/*=============================================================================
    Functions:
=============================================================================*/

// CAT (M2-R6 managers): the tutorial's enable words (0x00ba2e58 / 0x00ba2e5c), as ResearchGUI.c's
static bool32 catLmTutEnabled1(udword bit)
{
    return (tutorial != TUTORIAL_ONLY) || ((((udword *)&tutEnable)[0] & bit) != 0);
}

static bool32 catLmTutEnabled2(udword bit)
{
    return (tutorial != TUTORIAL_ONLY) || ((((udword *)&tutEnable)[1] & bit) != 0);
}

// CAT (M2-R6 managers): an atom made live or greyed out (FAF_Disabled 0x1000 and RSF_RegionDisabled 0x4, inlined)
static void catLmAtomEnable(featom *atom, bool32 enable)
{
    if (enable)
    {
        bitClear(atom->flags, FAF_Disabled);
        bitClear(((regionhandle)atom->region)->status, RSF_RegionDisabled);
    }
    else
    {
        bitSet(atom->flags, FAF_Disabled);
        bitSet(((regionhandle)atom->region)->status, RSF_RegionDisabled);
    }
}

static char *catLmScreenName(void)
{
    return (universe.curPlayerPtr->race == RACE_Sect) ? LM_LaunchScreen : CAT_LM_BeastLaunchScreen;
}

/*-----------------------------------------------------------------------------
    Name        : catLmFillInCarrierXs
    Description : CAT: the local player's first two carriers in cmCarriers order (inlined by lmLaunchBegin 0x0049e1e0
                  and lmLaunchCapableShipDied 0x0049e690)
----------------------------------------------------------------------------*/
static void catLmFillInCarrierXs(void)
{
    sdword i, insert = 0;

    for (i = 0; i < NUM_LMCARRIERS; i++)
    {
        lmCarrierX[i] = NULL;
    }
    for (i = 0; i < cmNumCarriers; i++)
    {
        if ((cmCarriers[i].owner == universe.curPlayerPtr) && (insert < NUM_LMCARRIERS))
        {
            lmCarrierX[insert++] = cmCarriers[i].ship;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : catLmCountShipsInside
    Description : CAT: shipsavailable from launchship's ships inside (inlined by lmLaunchBegin 0x0049e1e0, lmReLaunch
                  0x0049e520 and lmUpdateShipsInside 0x0049e070; the last clears only the count and the ship)
----------------------------------------------------------------------------*/
static void catLmCountShipsInside(bool32 clearSelected)
{
    Node *walk;
    Ship *shipinside;
    sdword i;

    shipsin = launchship->shipsInsideMe;
    for (i = 0; i < TOTAL_NUM_SHIPS; i++)
    {
        shipsavailable[i].nShips = 0;
        if (clearSelected)
        {
            shipsavailable[i].bSelected = FALSE;
        }
        shipsavailable[i].ship = NULL;
    }
    for (walk = shipsin->insideList.head; walk != NULL; walk = walk->next)
    {
        shipinside = ((InsideShip *)listGetStructOfNode(walk))->ship;
        dbgAssertOrIgnore(shipinside->header.objtype == OBJ_ShipType);
        shipsavailable[shipinside->shiptype].nShips++;
        shipsavailable[shipinside->shiptype].ship = shipinside;
    }
}

/*-----------------------------------------------------------------------------
    Name        : catLmListAddShips
    Description : CAT: the launch list from PrintOrder walked backwards, each docked type added at the head, a class
                  header after (above) its first ship: corvettes and resource-class ships under the corvette header,
                  fighters and micro-class ships under the fighter header (inlined by lmShipsToLaunchDraw 0x0049d4b0,
                  lmUpdateShipsInside 0x0049e070 and lmReLaunch 0x0049e520)
----------------------------------------------------------------------------*/
static void catLmListAddShips(void)
{
    sdword index, type;
    ShipClass shipclass;
    bool32 is_corv = FALSE, is_fight = FALSE;

    // CAT-FIX: the exe's walk starts on the -1 sentinel (PrintOrder 0x0088339c) and reads shipsavailable[-1], outside
    //      the array; it starts on the last ship here
    for (index = LM_NUMSHIPS - 1; index >= 0; index--)
    {
        type = PrintOrder[index];
        if ((type != LM_FIGHTER_SPACER) && (type != LM_CORVETTE_SPACER) && (shipsavailable[type].nShips > 0))
        {
            uicListAddItem(lmListWindow, (ubyte *)(smemsize)type, UICLI_CanSelect, UICLW_AddToHead);
            shipclass = shipsavailable[type].ship->staticinfo->shipclass;
            if ((shipclass == CLASS_Corvette) || (shipclass == CLASS_Resource))
            {
                is_corv = TRUE;
            }
            else if ((shipclass == CLASS_Fighter) || (shipclass == CLASS_Micro))
            {
                is_fight = TRUE;
            }
        }
        else if (((type == LM_FIGHTER_SPACER) && is_fight) || ((type == LM_CORVETTE_SPACER) && is_corv))
        {
            uicListAddItem(lmListWindow, (ubyte *)(smemsize)type, 0, UICLW_AddToHead);
        }
    }
}

// CAT: the list rebuilt with the selected row kept (lmUpdateShipsInside 0x0049e070, lmReLaunch 0x0049e520)
static void catLmListRebuild(void)
{
    sdword shipnum = -1;

    if (lmListWindow == NULL)
    {
        return;
    }
    if (lmListWindow->CurLineSelected != NULL)
    {
        shipnum = (sdword)(smemsize)lmListWindow->CurLineSelected->data;
    }
    uicListCleanUp(lmListWindow);
    catLmListAddShips();
    if (shipnum != -1)
    {
        uicListWindowSetCurItem(lmListWindow, uicListFindItemByData(lmListWindow, (ubyte *)(smemsize)shipnum));
    }
}

/*-----------------------------------------------------------------------------
    Name        : catLmLaunchSelection
    Description : CAT: the selection launched from launchship with the "launching" speech (0x4001067), as
                  LM_LaunchOne 0x0049cf10 ends (LM_Launch / LM_LaunchAll free their selection in between)
----------------------------------------------------------------------------*/
static void catLmLaunchSelection(SelectCommand *selection)
{
    clWrapLaunchMultipleShips(&universe.mainCommandLayer, selection, launchship);
    speechEventQueue(NULL, 0x4001067, 0, -1, 0, -1, -1, 5.0f, -1);
}

/*-----------------------------------------------------------------------------
    Name        : lmLaunch
    Description : process launch button press
    CAT (M2-R6 managers): the exe's body (0x0049ce30, no Ghidra function): the tutorial gate is bit 0x40000 of its second
        word; every docked ship of the selected row's type launches; the speech is the exe's
----------------------------------------------------------------------------*/
void lmLaunch(char *string, featom*atom)
{
    Node *walk;
    Ship *shipinside;
    sdword countShips = 0;
    sdword i;
    SelectCommand *selection;
    sdword shipnum=-1;

    if (!catLmTutEnabled2(0x40000))
    {
        return;
    }
    if (lmListWindow->CurLineSelected!=NULL)
    {
        shipnum = (sdword)(smemsize)lmListWindow->CurLineSelected->data;
    }
    for (walk = shipsin->insideList.head; walk != NULL; walk = walk->next)
    {
        shipinside = ((InsideShip *)listGetStructOfNode(walk))->ship;
        if (shipnum == (sdword)shipinside->shiptype)
        {
            countShips++;
        }
    }
    if (countShips > 0)
    {
        selection = memAlloc(sizeofSelectCommand(countShips),"launchships",0);
        selection->numShips = countShips;
        i = 0;
        for (walk = shipsin->insideList.head; walk != NULL; walk = walk->next)
        {
            shipinside = ((InsideShip *)listGetStructOfNode(walk))->ship;
            if (shipnum == (sdword)shipinside->shiptype)
            {
                selection->ShipPtr[i++] = shipinside;
            }
        }
        clWrapLaunchMultipleShips(&universe.mainCommandLayer, selection, launchship);
        memFree(selection);
        speechEventQueue(NULL, 0x4001067, 0, -1, 0, -1, -1, 5.0f, -1);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_lmLaunchOne
    Description : CAT: new (LM_LaunchOne 0x0049cf10, no Ghidra function): the first docked ship of the selected row's type
                  launches (the tutorial gate as LM_Launch's)
----------------------------------------------------------------------------*/
void cat_lmLaunchOne(char *string, featom *atom)
{
    Node *walk;
    Ship *shipinside;
    SelectCommand selection;
    sdword shipnum = -1;

    if (!catLmTutEnabled2(0x40000))
    {
        return;
    }
    if (lmListWindow->CurLineSelected != NULL)
    {
        shipnum = (sdword)(smemsize)lmListWindow->CurLineSelected->data;
    }
    for (walk = shipsin->insideList.head; walk != NULL; walk = walk->next)
    {
        shipinside = ((InsideShip *)listGetStructOfNode(walk))->ship;
        if (shipnum == (sdword)shipinside->shiptype)
        {
            selection.numShips = 1;
            selection.ShipPtr[0] = shipinside;
            catLmLaunchSelection(&selection);
            return;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : lmAutoLaunchM / lmAutoLaunchCX
    Description : process autolaunch checkbox
    CAT (M2-R6 managers): the exe's bodies (LM_AutoLaunchM 0x0049cfd0, no Ghidra function; lmAutoLaunchCX 0x0049d080): the
        mothership's box is live for a mothership-class mothership; a carrier's for a carrier in its slot, bit 1 / 2 of
        Player.autoLaunch; the box reads checked when the bit is off (stay docked)
----------------------------------------------------------------------------*/
void lmAutoLaunchM(char *string, featom *atom)
{
    Ship *mothership = universe.curPlayerPtr->PlayerMothership;

    catLmAtomEnable(atom, (mothership != NULL) && (mothership->staticinfo->shipclass == CLASS_Mothership));
    bitSet(((regionhandle)atom->region)->status, RSF_DrawThisFrame);

    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(string, !(universe.curPlayerPtr->autoLaunch & BIT0));
    }
    else
    {
        // DO NOT CHANGE universe.curPlayerPtr->autoLaunch DIRECTLY OR SYNC ERRORS WILL RESULT.  JUST REQUEST
        // WANT YOU WANT via clWrapAutoLaunch
        udword newAutoLaunch = universe.curPlayerPtr->autoLaunch;

        if (!(FECHECKED(atom)))    //sense now reversed!
             //check mark TRUE means stay docked: autolaunch is FALSE
        {
            newAutoLaunch |= BIT0;
        }
        else
        {
            newAutoLaunch &= ~BIT0;
        }
        clWrapAutoLaunch(newAutoLaunch,universe.curPlayerIndex);
    }
}

void lmAutoLaunchCX(char *string, featom *atom, udword x)
{
    static udword bits[NUM_LMCARRIERS] = { BIT1, BIT2 };       // CAT: .data 0x008835f4
    static udword shifts[NUM_LMCARRIERS] = { 1, 2 };           // CAT: .data 0x008835fc
    udword bit, shift;

    // CAT-FIX: LM_AutoLaunchC3 indexes past both tables in the exe (bits[2] is shifts[0], shifts[2] the module hover
    //      word 0x00883604); neither launch screen has the atom, so it never runs
    if ((x < 1) || (x > NUM_LMCARRIERS))
    {
        return;
    }
    bit = bits[x-1];
    shift = shifts[x-1];

    catLmAtomEnable(atom, lmCarrierX[x-1] != NULL);
    bitSet(((regionhandle)atom->region)->status, RSF_DrawThisFrame);

    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(string, !((universe.curPlayerPtr->autoLaunch & bit) >> shift));
    }
    else
    {
        udword newAutoLaunch = universe.curPlayerPtr->autoLaunch;

        if (!(FECHECKED(atom)))
        {
            newAutoLaunch |= bit;
        }
        else
        {
            newAutoLaunch &= ~bit;
        }
        clWrapAutoLaunch(newAutoLaunch,universe.curPlayerIndex);
    }
}

void lmAutoLaunchC1(char *string, featom *atom)
{
    lmAutoLaunchCX(string,atom,1);
}

void lmAutoLaunchC2(char *string, featom *atom)
{
    lmAutoLaunchCX(string,atom,2);
}

void lmAutoLaunchC3(char *string, featom *atom)
{
    lmAutoLaunchCX(string,atom,3);
}

/*-----------------------------------------------------------------------------
    Name        : lmLaunchAll
    Description : process LaunchAll button
    CAT (M2-R6 managers): the exe's body (0x0049d1a0, no Ghidra function): the tutorial gate is bit 0x80000 of its second
        word; every docked ship but a dEcclesiam (0x8c) launches and the screen stays open
----------------------------------------------------------------------------*/
void lmLaunchAll(char *string, featom *atom)
{
    Node *walk;
    Ship *shipinside;
    SelectCommand *selection;
    sdword numShips;
    sdword i;

    if (!catLmTutEnabled2(0x80000))
    {
        return;
    }
    numShips = shipsin->insideList.num;
    if (numShips > 0)
    {
        selection = memAlloc(sizeofSelectCommand(numShips),"launchall",0);
        i = 0;
        for (walk = shipsin->insideList.head; walk != NULL; walk = walk->next)
        {
            shipinside = ((InsideShip *)listGetStructOfNode(walk))->ship;
            if (shipinside->shiptype == dEcclesiam)
            {
                numShips--;
            }
            else
            {
                selection->ShipPtr[i++] = shipinside;
            }
        }
        selection->numShips = numShips;
        clWrapLaunchMultipleShips(&universe.mainCommandLayer,selection,launchship);
        memFree(selection);
        speechEventQueue(NULL, 0x4001067, 0, -1, 0, -1, -1, 5.0f, -1);
    }
}

/*-----------------------------------------------------------------------------
    Name        : lmClose / lmCloseIfOpen / cat_lmConstruction / cat_lmResearch
    Description : close the launch manager
    CAT (M2-R6 managers): the exe's bodies (lmCloseIfOpen 0x0049d300; LM_Close 0x0049d250, LM_Construction 0x0049ccd0,
        LM_Research 0x0049cd80 with no Ghidra function): the tutorial gate bit 0x100000 of its second word; the screen
        deleted, the factory name region forgotten, the sounds 0x130 / 0x100, the main view drawn again, the render
        counter cleared, the ship view closed, the sensors manager reopened when this screen closed it; the two
        buttons then open the construction manager / research manager (even when the gate kept the screen)
----------------------------------------------------------------------------*/
static void catLmClose(void)
{
    feScreenDeleteFlags(lmBaseRegion, 0);
    lmBaseRegion = NULL;
    catLmFactoryNameRegion = NULL;
    soundEventPlay(NULL, 0x130, NULL);                   // CAT: the manager exit
    soundEventPlay(NULL, UI_SoundOfSpace, NULL);         // CAT: 0x100
    mrRenderMainScreen = TRUE;
    lmActive = FALSE;
    lmRenderEverythingCounter = 0;
    svClose();
    if (catLmSensorsWereOpen)
    {
        catSmReopenAfterManager();
    }
    cat_tutButtonsRestoreAfterScreen();      // CAT: the tutorial's buttons come back (0x00534300; lmCloseIfOpen 0x0049d3a2 ..)
}

void lmClose(char *string, featom *atom)
{
    if (catLmTutEnabled2(0x100000))
    {
        catLmClose();
    }
}

void lmCloseIfOpen(void)
{
    if ((lmBaseRegion != NULL) && catLmTutEnabled2(0x100000))
    {
        catLmClose();
    }
}

void cat_lmConstruction(char *string, featom *atom)
{
    lmClose(string, atom);
    mrBuildShips(NULL, NULL);
}

void cat_lmResearch(char *string, featom *atom)
{
    lmClose(string, atom);
    mrResearch(NULL, NULL);
}

/*-----------------------------------------------------------------------------
    Name        : cat_lmBeastSystemsSet
    Description : CAT: new (LM_BeastSystemsSet 0x0049fbb0, no Ghidra function): the Beast mothership's systems radio
                  buttons, live only for a bMothership; they show its system state (MothershipSpec.beastSystemState,
                  Ship+0x650) and a press sends the button's state (the atom's pData byte) by cat_clWrapSetBeastSystem
----------------------------------------------------------------------------*/
void cat_lmBeastSystemsSet(char *string, featom *atom)
{
    Ship *mothership = universe.curPlayerPtr->PlayerMothership;

    if ((mothership == NULL) || (mothership->shiptype != bMothership))
    {
        catLmAtomEnable(atom, FALSE);
        return;
    }
    catLmAtomEnable(atom, TRUE);
    if (FEFIRSTCALL(atom))
    {
        feRadioButtonSet(string, ((MothershipSpec *)mothership->ShipSpecifics)->beastSystemState);
        return;
    }
    cat_clWrapSetBeastSystem(&universe.mainCommandLayer, mothership, (ubyte)(smemsize)atom->pData);
}

/*-----------------------------------------------------------------------------
    Name        : cat_lmBeastSystemsOff
    Description : CAT: new (0x0049fc30): the Beast mothership's systems switched off (beastMothershipSelfDamage at half
                  health); with the launch manager open on the local player's bMothership the radio buttons follow
----------------------------------------------------------------------------*/
void cat_lmBeastSystemsOff(Ship *mothership)
{
    Ship *own;

    cat_clWrapSetBeastSystem(&universe.mainCommandLayer, mothership, 0);
    own = universe.curPlayerPtr->PlayerMothership;
    if (lmActive && (own == mothership) && (own != NULL) && (own->shiptype == bMothership))
    {
        feRadioButtonSet("LM_BeastSystemsSet", 0);
    }
}

/*-----------------------------------------------------------------------------
    Name        : lmShipItemDraw
    Description : one row of the launch list
    CAT (M2-R6 managers): the exe's body (0x0049d3b0, no Ghidra function): the class headers are one string each,
        strLaunchMgrFighterClass / strLaunchMgrCorvetteClass (HW1's Italian word order special case is gone)
----------------------------------------------------------------------------*/
void lmShipItemDraw(rectangle *rect, listitemhandle data)
{
    fonthandle currentFont;
    char       tempstr[64];
    color      col;
    sdword     x, y;
    udword     shipnum = (udword)(smemsize)data->data;

    currentFont = fontMakeCurrent(lmShipListFont);

    col = bitTest(data->flags,UICLI_Selected) ? FEC_ListItemSelected : FEC_ListItemStandard;

    x = rect->x0;
    y = rect->y0;

    if (shipnum == LM_FIGHTER_SPACER)
    {
        fontPrintf(x, y, colRGB(255,200,0), "%s", strGetString(strLaunchMgrFighterClass));
    }
    else if (shipnum == LM_CORVETTE_SPACER)
    {
        fontPrintf(x, y, colRGB(255,200,0), "%s", strGetString(strLaunchMgrCorvetteClass));
    }
    else
    {
        sprintf(tempstr, " %i",shipsavailable[shipnum].nShips);
        fontPrintf(x+fontWidth(" 55 ")-fontWidth(tempstr), y, col,"%s",tempstr);
        fontPrintf(x+fontWidth(" 55 "), y, col," x %s", ShipTypeToNiceStr(shipnum));
    }

    fontMakeCurrent(currentFont);
}


/*-----------------------------------------------------------------------------
    Name        : lmShipsToLaunchDraw
    Description : draw Ships to Launch in box for selection
    CAT (M2-R6 managers): the exe's body (0x0049d4b0): the list built on the first call, forgotten on the last; picking
        a row does nothing more (no ship view)
----------------------------------------------------------------------------*/
void lmShipsToLaunchDraw(char *string, featom *atom)
{
    fonthandle      oldfont;

    if (FEFIRSTCALL(atom))
    {
        oldfont = fontMakeCurrent(lmShipListFont);

        lmListWindow = (listwindowhandle)atom->pData;

        uicListWindowInit(lmListWindow,
                          NULL,                             // title draw, no title
                          NULL,                             // title click process, no title
                          0,                                // title height, no title
                          lmShipItemDraw,              // item draw function
                          fontHeight(" ") + LM_VertSpacing, // item height
                          UICLW_CanSelect);

        fontMakeCurrent(oldfont);

        catLmListAddShips();
        return;
    }
    else if (FELASTCALL(atom))
    {
        lmListWindow = NULL;
        return;
    }
}

/*-----------------------------------------------------------------------------
    Name        : lmBarDraw
    Description : draws a bar with a background and a foreground and percent used
    Inputs      : rect, backcol, forecol, percent
    CAT: inlined by the exe's LM_FighterUsed 0x0049d5e0 / LM_CorvetteUsed 0x0049d700 (the foreground is lmUsedColor)
----------------------------------------------------------------------------*/
static void lmBarDraw(rectangle *rect, color back, real32 percent)
{
    rectangle temp;
    primRectSolid2(rect, back);

    if (percent > 1.0)
    {
        percent = 1.0;
    }

    temp.x0 = rect->x0;
    temp.y0 = rect->y0;
    temp.x1 = rect->x0 + (sdword)((rect->x1-rect->x0)*percent);
    temp.y1 = rect->y1;

    primRectSolid2(&temp, lmUsedColor);
}

/*-----------------------------------------------------------------------------
    Name        : lmFighterUsedDraw / lmCorvetteUsedDraw
    Description : draw the fighter's / corvette's used bar and number
    CAT (M2-R6 managers): the exe's bodies (0x0049d5e0 / 0x0049d700, no Ghidra function): the number in 0xff000080
----------------------------------------------------------------------------*/
void lmFighterUsedDraw(featom *atom, regionhandle region)
{
    real32 percent;
    sdword numused;
    fonthandle currentFont;

    if (lmRenderEverythingCounter > 0)
    {
        lmRenderEverythingCounter--;
        feRenderEverything = TRUE;
    }

    if (launchship->shipsInsideMe == NULL) return;

    currentFont = fontMakeCurrent(lmShipListFont);

    numused = (sdword)launchship->shipsInsideMe->FightersInsideme;

    percent = (real32)(launchship->shipsInsideMe->FightersInsideme) /
              (real32)(launchship->staticinfo->maxDockableFighters);

    if (numused >= launchship->staticinfo->maxDockableFighters)
        numused = launchship->staticinfo->maxDockableFighters;

    lmBarDraw(&region->rect, lmAvailableColor, percent);

    fontPrintf(region->rect.x1,region->rect.y0,CAT_LM_UsedTextColor," %i",numused);

    fontMakeCurrent(currentFont);
}

void lmCorvetteUsedDraw(featom *atom, regionhandle region)
{
    real32 percent;
    sdword numused;
    fonthandle currentFont;

    if (launchship->shipsInsideMe == NULL) return;

    currentFont = fontMakeCurrent(lmShipListFont);

    numused = (sdword)launchship->shipsInsideMe->CorvettesInsideme;

    percent = (real32)(launchship->shipsInsideMe->CorvettesInsideme) /
              (real32)(launchship->staticinfo->maxDockableCorvettes);

    if (numused >= launchship->staticinfo->maxDockableCorvettes)
        numused = launchship->staticinfo->maxDockableCorvettes;

    lmBarDraw(&region->rect, lmAvailableColor, percent);

    fontPrintf(region->rect.x1,region->rect.y0,CAT_LM_UsedTextColor," %i",numused);

    fontMakeCurrent(currentFont);
}

/*-----------------------------------------------------------------------------
    Name        : lmSelectMotherShip / lmSelectCarrier1 / lmSelectCarrier2
    Description : the factory buttons
    CAT (M2-R6 managers): the exe's bodies (0x0049d800 / 0x0049d850 / 0x0049d890, region callbacks with no Ghidra
        function): a left press on another slot shows that ship's docked ships (lmReLaunch); no ship view
----------------------------------------------------------------------------*/
sdword lmSelectMotherShip(regionhandle region, smemsize ID, udword event, udword data)
{
    if ((universe.curPlayerPtr->PlayerMothership != NULL) && (event == RPE_PressLeft) && (lmCurrentSelect != 0))
    {
        lmCurrentSelect = 0;
        bitSet(region->status, RSF_DrawThisFrame);
        lmReLaunch(universe.curPlayerPtr->PlayerMothership);
    }
    return(0);
}

static sdword lmSelectCarrierX(regionhandle region, udword event, udword x)
{
    if ((lmCarrierX[x-1] != NULL) && (event == RPE_PressLeft) && (lmCurrentSelect != (sdword)x))
    {
        lmCurrentSelect = x;
        bitSet(region->status, RSF_DrawThisFrame);
        lmReLaunch(lmCarrierX[x-1]);
    }
    return(0);
}

sdword lmSelectCarrier1(regionhandle region, smemsize ID, udword event, udword data)
{
    return lmSelectCarrierX(region,event,1);
}

sdword lmSelectCarrier2(regionhandle region, smemsize ID, udword event, udword data)
{
    return lmSelectCarrierX(region,event,2);
}

/*-----------------------------------------------------------------------------
    Name        : lmDrawShipImage
    Description : draws the button image
    CAT (M2-R6 managers): the exe's body (0x0049d980): the third argument says whether the slot holds a ship; image base
        0 (mothership) or 3 (carrier), + 0 disabled, + 1 off, + 2 on for the shown factory, which also gets the
        2-pixel white line up to the factory name and marks the screen region dirty (status bit 0x1)
----------------------------------------------------------------------------*/
void lmDrawShipImage(regionhandle region, smemsize shipID, bool32 present)
{
    rectangle rect;
    sdword index = (shipID != 0) ? 3 : 0;
    sdword race = universe.curPlayerPtr->race;

    rect.x0 = region->rect.x0 + LM_SHIP_IMAGE_INSET;
    rect.y0 = region->rect.y0 + LM_SHIP_IMAGE_INSET;
    rect.x1 = region->rect.x1 - LM_SHIP_IMAGE_INSET;
    rect.y1 = region->rect.y1 - LM_SHIP_IMAGE_INSET;

    if (!present)
    {
        ferDrawFocusWindow(region, lw_normal);
    }
    else if ((sdword)shipID == lmCurrentSelect)
    {
        ferDrawFocusWindow(region, lw_focus);
        index += 2;
    }
    else
    {
        ferDrawFocusWindow(region, lw_normal);
        index += 1;
    }
    // CAT: the exe draws the image scaled through glcompat when cat_glcActive 0x0055c480; the SDL build has no glcompat
    //      (as svShipViewRender), so the texture path; a race past the two image sets binds no texture, as the exe
    if ((race == RACE_Sect) || (race == RACE_Beast))
    {
        if (lmPaletted)
        {
            trPalettedTextureMakeCurrent(lmShipTexture[race][index], lmShipImage[race][index]->palette);
        }
        else
        {
            trRGBTextureMakeCurrent(lmShipTexture[race][index]);
        }
    }
    primRectSolidTextured2(&rect);
    if (present && ((sdword)shipID == lmCurrentSelect))
    {
        if (catLmFactoryNameRegion != NULL)
        {
            sdword mid = (rect.x1 - rect.x0) / 2 + rect.x0;

            primLineThick2(mid, catLmFactoryNameRegion->rect.y1, mid, region->rect.y0, 2, colWhite);
        }
        if (lmBaseRegion != NULL)
        {
            lmBaseRegion->status |= RSF_ReallyDirty;      // CAT: status bit 0x1 (0x0049db34)
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : lmMotherShipDraw
    Description : draws the mothership button
    CAT (M2-R6 managers): the exe's body (0x0049db40): live for a mothership-class mothership, which becomes the named
        factory
----------------------------------------------------------------------------*/
void lmMotherShipDraw(featom *atom, regionhandle region)
{
    Ship *mothership = universe.curPlayerPtr->PlayerMothership;

    if ((mothership != NULL) && (mothership->staticinfo->shipclass == CLASS_Mothership))
    {
        if (region->flags == 0 || region->flags == RPE_DrawFunctionAdded)
        {                                         //if region not processed yet
            region->flags = RPE_DrawEveryFrame | RPE_PressLeft;    // CAT: 0x80002000
            regFunctionSet(region, (regionfunction)lmSelectMotherShip); //set new region handler function
        }
        lmDrawShipImage(region, 0, TRUE);
        catLmFactoryShip = mothership;
        return;
    }
    lmDrawShipImage(region, 0, FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : lmCarrierXDraw
    Description : draws the carrierX button
    CAT (M2-R6 managers): the exe's body (0x0049dbb0): the x-th of the local player's carriers in cmCarriers order
----------------------------------------------------------------------------*/
void lmCarrierXDraw(featom *atom, regionhandle region,udword x)
{
    sdword index;
    udword count = 0;

    for (index=0;index<cmNumCarriers;index++)
    {
        if ((cmCarriers[index].owner == universe.curPlayerPtr) && (++count > (x-1)))
        {
            lmCarrierX[x-1] = cmCarriers[index].ship;
            if (region->flags == 0 || region->flags == RPE_DrawFunctionAdded)
            {                                         //if region not processed yet
                region->flags = RPE_DrawEveryFrame | RPE_PressLeft;    // CAT: 0x80002000
                if (x == 1)
                {
                    regFunctionSet(region, (regionfunction)lmSelectCarrier1);
                }
                else if (x == 2)
                {
                    regFunctionSet(region, (regionfunction)lmSelectCarrier2);
                }
            }
            lmDrawShipImage(region, x, TRUE);
            catLmFactoryShip = cmCarriers[index].ship;
            return;
        }
    }
    lmDrawShipImage(region, x, FALSE);
}

// CAT: the exe's LM_Carrier1Draw 0x0049dc60 / LM_Carrier2Draw 0x0049dce0: the carrier's autolaunch box follows the slot
static void catLmCarrierDraw(featom *atom, regionhandle region, udword x, char *boxName)
{
    catLmAtomEnable(feAtomFindInScreen(feScreenFind(catLmScreenName()), boxName), lmCarrierX[x-1] != NULL);
    lmCarrierXDraw(atom, region, x);
}

void lmCarrier1Draw(featom *atom, regionhandle region)
{
    catLmCarrierDraw(atom, region, 1, "LM_AutoLaunchC1");
}

void lmCarrier2Draw(featom *atom, regionhandle region)
{
    catLmCarrierDraw(atom, region, 2, "LM_AutoLaunchC2");
}

/*-----------------------------------------------------------------------------
    Name        : cat_lmFactoryNameDraw
    Description : CAT: new (LM_FactoryNameDraw 0x0049d8d0, no Ghidra function): the shown factory's name (a human player's
                  named ship, cat_shipNamesGetName) above a 2-pixel white line along the region's bottom; nothing until
                  a factory button has drawn
----------------------------------------------------------------------------*/
void cat_lmFactoryNameDraw(featom *atom, regionhandle region)
{
    fonthandle oldfont;
    Ship *factory;

    catLmFactoryNameRegion = region;
    if (catLmFactoryShip == NULL)
    {
        return;
    }
    oldfont = fontMakeCurrent(catLmFactoryNameFont);
    factory = (lmCurrentSelect == 0) ? universe.curPlayerPtr->PlayerMothership : lmCarrierX[lmCurrentSelect - 1];
    catLmFactoryShip = factory;
    if ((factory != NULL) && (factory->playerowner->aiPlayer == NULL))
    {
        // CAT: the exe passes the name as the format string
        fontPrintf(region->rect.x0, region->rect.y1 - fontHeight(" "), colWhite, "%s", cat_shipNamesGetName(factory));
    }
    primLineThick2(region->rect.x0, region->rect.y1, region->rect.x1, region->rect.y1, 2, colWhite);
    fontMakeCurrent(oldfont);
}

/*-----------------------------------------------------------------------------
    Name        : cat_lmModuleSlotXDraw
    Description : CAT: new (0x0049eb40): a module slot's three system buttons at the slot's right end, 32 x 16 pixels
                  4 apart from x1 - 0x6c: disabled until the mothership has the upgrade flag 0x200000 (the flag
                  render.h names CAT_UPGRADE_RepairDroids), then off / off_focus for the module's chosen system
                  (cat_mothershipModuleSlotGet), off_mouse / on under the mouse
    Inputs      : region - the slot's region; moduleType - the slot's module type
----------------------------------------------------------------------------*/
static void cat_lmModuleSlotXDraw(regionhandle region, sdword moduleType)
{
    Ship *mothership = universe.curPlayerPtr->PlayerMothership;
    rectangle rect;
    sdword chosen, system, state;

    if (mothership == NULL)
    {
        return;
    }
    chosen = cat_mothershipModuleSlotGet(mothership, moduleType);
    rect.x0 = region->rect.x1 - 0x6c;
    rect.x1 = region->rect.x1 - 0x4c;
    rect.y0 = region->rect.y0 + 4;
    rect.y1 = region->rect.y0 + 0x14;
    for (system = 0; system < CAT_LM_NUM_SYSTEMS; system++)
    {
        if ((universe.curPlayerPtr->PlayerMothership == NULL) ||
            cat_shipHasUpgrade(universe.curPlayerPtr->PlayerMothership, 0x00200000))
        {
            state = (system == chosen) ? 3 : 1;
            if ((catLmHoverModuleType == moduleType) && (catLmHoverSystem == system))
            {
                state++;
            }
        }
        else
        {
            state = 0;
        }
        // CAT: the texture path (no glcompat in the SDL build, as lmDrawShipImage)
        trRGBTextureMakeCurrent(catLmSysTexture[system * CAT_LM_SYS_STATES + state]);
        primRectSolidTextured2(&rect);
        rect.x0 = rect.x1 + 4;
        rect.x1 = rect.x1 + 0x24;
    }
}

/*-----------------------------------------------------------------------------
    Name        : catLmModuleSlotProcess
    Description : CAT: the module slot regions' callback (0x0049ec60 + 0x90 per slot, region callbacks with no Ghidra
                  function): the system button under the mouse (the 32-pixel columns 0x20 / 0x44 / 0x68 left of the
                  slot's right edge, right to left systems 2, 1, 0) is remembered for the draw; a left release picks
                  it for the module (cat_mothershipModuleSlotSet)
----------------------------------------------------------------------------*/
static sdword catLmModuleSlotProcess(regionhandle region, udword event, sdword slot)
{
    Ship *mothership = universe.curPlayerPtr->PlayerMothership;
    Ship *module = cat_lmModuleSlots[slot];
    sdword edge, system;

    if ((mothership == NULL) || (mothership->shiptype != sMothership) || (module == NULL))
    {
        return 0;
    }
    edge = region->rect.x1 - 0x20;
    if (mouseCursorX() > edge)
    {
        system = 2;
    }
    else if (mouseCursorX() > edge - 0x24)
    {
        system = 1;
    }
    else
    {
        system = (mouseCursorX() > edge - 0x48) ? 0 : -1;
    }
    catLmHoverModuleType = module->shiptype;
    catLmHoverSystem = system;
    if ((system != -1) && (event == RPE_ReleaseLeft))
    {
        cat_mothershipModuleSlotSet(mothership, module->shiptype, system);
    }
    return 0;
}

static sdword catLmModuleSlot1Process(regionhandle region, smemsize ID, udword event, udword data) { return catLmModuleSlotProcess(region, event, 0); }
static sdword catLmModuleSlot2Process(regionhandle region, smemsize ID, udword event, udword data) { return catLmModuleSlotProcess(region, event, 1); }
static sdword catLmModuleSlot3Process(regionhandle region, smemsize ID, udword event, udword data) { return catLmModuleSlotProcess(region, event, 2); }
static sdword catLmModuleSlot4Process(regionhandle region, smemsize ID, udword event, udword data) { return catLmModuleSlotProcess(region, event, 3); }
static sdword catLmModuleSlot5Process(regionhandle region, smemsize ID, udword event, udword data) { return catLmModuleSlotProcess(region, event, 4); }
static sdword catLmModuleSlot6Process(regionhandle region, smemsize ID, udword event, udword data) { return catLmModuleSlotProcess(region, event, 5); }
static sdword catLmModuleSlot7Process(regionhandle region, smemsize ID, udword event, udword data) { return catLmModuleSlotProcess(region, event, 6); }

/*-----------------------------------------------------------------------------
    Name        : cat_lmModuleSlotDraw
    Description : CAT: the body of cat_lmModuleSlot1..7Draw (0x0049f050 + 0x170 per slot, the first also refilling the
                  slots): the region takes every mouse event (flags 0x7ffff) and comes to the front; a filled slot of
                  an sMothership shows the module's name, its health bar (cat_selHealthFactorGet / ColorsGet, 6 pixels
                  high along the bottom, 0x72 pixels short of the right edge) and its system buttons; otherwise the
                  region is cleared to the background colour
----------------------------------------------------------------------------*/
static void cat_lmModuleSlotDraw(regionhandle region, sdword slot, regionfunction process)
{
    Ship *mothership = universe.curPlayerPtr->PlayerMothership;
    Ship *module = cat_lmModuleSlots[slot];
    fonthandle oldfont;
    rectangle rect;
    sdword width;
    real32 factor;
    color c, dark;

    if (region->flags == 0 || region->flags == RPE_DrawFunctionAdded)
    {
        region->flags = 0x0007ffff;            // CAT: RPE_Enter .. RPE_ReleaseCentre
        regFunctionSet(region, process);
        regSiblingMoveToFront(region);
    }
    if ((mothership == NULL) || (mothership->shiptype != sMothership) || (module == NULL))
    {
        primRectSolid2(&region->rect, FEC_Background);
        return;
    }
    oldfont = fontMakeCurrent(lmShipListFont);
    fontPrint(region->rect.x0 + 6, region->rect.y0, FEC_ListItemStandard, ShipTypeToNiceStr(module->shiptype));
    width = (region->rect.x1 - region->rect.x0) - 0x72;
    cat_selHealthFactorGet(module, &factor);
    cat_selHealthColorsGet(&factor, &c, &dark);
    rect.x0 = region->rect.x0 + 6;
    rect.x1 = region->rect.x0 + width;
    rect.y1 = region->rect.y1;
    rect.y0 = rect.y1 - 6;
    primRectSolid2(&rect, dark);
    if (factor < 0.0f)
    {
        factor = 0.0f;
    }
    rect.x1 = (sdword)((real32)width * factor) + region->rect.x0;
    primRectSolid2(&rect, c);
    cat_lmModuleSlotXDraw(region, module->shiptype);
    fontMakeCurrent(oldfont);
}

void cat_lmModuleSlot1Draw(featom *atom, regionhandle region)
{
    cat_lmFillInModuleSlots();
    cat_lmModuleSlotDraw(region, 0, (regionfunction)catLmModuleSlot1Process);
}

void cat_lmModuleSlot2Draw(featom *atom, regionhandle region) { cat_lmModuleSlotDraw(region, 1, (regionfunction)catLmModuleSlot2Process); }
void cat_lmModuleSlot3Draw(featom *atom, regionhandle region) { cat_lmModuleSlotDraw(region, 2, (regionfunction)catLmModuleSlot3Process); }
void cat_lmModuleSlot4Draw(featom *atom, regionhandle region) { cat_lmModuleSlotDraw(region, 3, (regionfunction)catLmModuleSlot4Process); }
void cat_lmModuleSlot5Draw(featom *atom, regionhandle region) { cat_lmModuleSlotDraw(region, 4, (regionfunction)catLmModuleSlot5Process); }
void cat_lmModuleSlot6Draw(featom *atom, regionhandle region) { cat_lmModuleSlotDraw(region, 5, (regionfunction)catLmModuleSlot6Process); }
void cat_lmModuleSlot7Draw(featom *atom, regionhandle region) { cat_lmModuleSlotDraw(region, 6, (regionfunction)catLmModuleSlot7Process); }

/*-----------------------------------------------------------------------------
    Name        : lmLoadTextures
    Description : load the textures used by the launch manager
    CAT: the exe's body (0x0049dd60): IDENTICAL in shape to HW1's but for the six icon names and the second loop
----------------------------------------------------------------------------*/
void lmLoadTextures(void)
{
    char filename[128];
    sdword i, j;

    for(i=0; i < MAX_RACES; i++)
    {
        for(j = 0; j < NUM_LMTEXTURES; j++)
        {
            // Build filename for loading texture from file
            strcpy(filename, lmShipImagePaths[i]);
            strcat(filename, lmShipIcons[j]);

            // Load the image
            lmShipImage[i][j] = trLIFFileLoad(filename, NonVolatile);
            if (bitTest(lmShipImage[i][j]->flags, TRF_Paletted))
            {
                lmPaletted = TRUE;
            }
            else
            {
                lmPaletted = FALSE;
            }

            if (lmPaletted)
            {
                lmShipTexture[i][j] = trPalettedTextureCreate(
                    lmShipImage[i][j]->data,
                    lmShipImage[i][j]->palette,
                    lmShipImage[i][j]->width,
                    lmShipImage[i][j]->height);
            }
            else
            {
                lmShipTexture[i][j] = trRGBTextureCreate(
                    (color*)lmShipImage[i][j]->data,
                    lmShipImage[i][j]->width,
                    lmShipImage[i][j]->height,
                    TRUE);
            }
        }
    }

    // CAT: load the launch-system button textures (lmLoadTextures 0x0049dd60, second loop: RGB textures)
    for (j = 0; j < CAT_NUM_LMSYSTEXTURES; j++)
    {
        strcpy(filename, CAT_LM_SYS_PATH);
        strcat(filename, catLmSysIcons[j]);
        catLmSysImage[j] = trLIFFileLoad(filename, NonVolatile);
        catLmSysTexture[j] = trRGBTextureCreate(
            (color*)catLmSysImage[j]->data,
            catLmSysImage[j]->width,
            catLmSysImage[j]->height,
            TRUE);
    }
}

/*-----------------------------------------------------------------------------
    Name        : lmFreeTextures
    Description : delete the textures used by the launch manager
    CAT: the exe's body (0x0049df00): HW1's plus the second loop
----------------------------------------------------------------------------*/
void lmFreeTextures(void)
{
    sdword i,j;

    for (i=0;i<MAX_RACES;i++)
    {
        for (j=0;j<NUM_LMTEXTURES;j++)
        {
            if (lmShipImage[i][j] != NULL)
            {
                memFree(lmShipImage[i][j]);
                lmShipImage[i][j] = NULL;
            }
            if (lmShipTexture[i][j] != TR_InvalidHandle)
            {
                trRGBTextureDelete(lmShipTexture[i][j]);
                lmShipTexture[i][j] = TR_InvalidHandle;
            }
        }
    }

    // CAT: free the launch-system button textures (lmFreeTextures 0x0049df00, second loop)
    for (j = 0; j < CAT_NUM_LMSYSTEXTURES; j++)
    {
        if (catLmSysImage[j] != NULL)
        {
            memFree(catLmSysImage[j]);
            catLmSysImage[j] = NULL;
        }
        if (catLmSysTexture[j] != TR_InvalidHandle)
        {
            trRGBTextureDelete(catLmSysTexture[j]);
            catLmSysTexture[j] = TR_InvalidHandle;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_lmBackgroundDraw
    Description : CAT: new (0x0049df90, the "dont_draw" draw callback lmStartup registers): the standard rectangle
                  filled with the background colour
----------------------------------------------------------------------------*/
static void cat_lmBackgroundDraw(featom *atom, regionhandle region)
{
    feStaticRectangleDraw(region);
    primRectSolid2(&region->rect, FEC_Background);
}

/*-----------------------------------------------------------------------------
    Name        : lmStartup
    Description : Start up the launch manager
    CAT (M2-R6 managers): the exe's body (0x0049dfb0): the factory name font too, and the "dont_draw" draw callback
----------------------------------------------------------------------------*/
void lmStartup(void)
{
    sdword index;

    lmRenderEverythingCounter = 0;

    lmShipListFont = frFontRegister(lmShipListFontName);
    catLmFactoryNameFont = frFontRegister(CAT_LM_FactoryNameFont);
    lmLoadTextures();

    for (index=0;index < TOTAL_NUM_SHIPS; index++)
    {
        lmShipInList[index] = FALSE;
    }

    for (index=0; PrintOrder[index]!=-1; index++)
    {
        if ((PrintOrder[index]!=LM_FIGHTER_SPACER) && (PrintOrder[index]!=LM_CORVETTE_SPACER))
        {    // ship is in the list so add it in
            lmShipInList[PrintOrder[index]] = TRUE;
        }
    }
    feDrawCallbackAdd("dont_draw", cat_lmBackgroundDraw);
}

/*-----------------------------------------------------------------------------
    Name        : lmShutdown
    Description : shut down the launch manager
    CAT: the exe's body (0x0049e040): IDENTICAL to HW1's
----------------------------------------------------------------------------*/
void lmShutdown(void)
{
    if (lmScreensHandle)
    {
        feScreensDelete(lmScreensHandle);
        lmScreensHandle = NULL;
    }
    lmFreeTextures();
}

/*-----------------------------------------------------------------------------
    Name        : cat_lmShipAutoLaunchBit
    Description : CAT: new (0x0049fa60): host's bit in its owner's autoLaunch word (1 the mothership, 2 / 4 / 8 the three
                  carriers of FillInCarrierMothershipInfo), 0 for any other ship and, in single player, for a ship of a
                  computer player or with SPECIAL_2_CatExempt20 (cmRemoveFactory 0x004546d0 takes a dead carrier's bit
                  out of the word)
    Inputs      : host
----------------------------------------------------------------------------*/
sdword cat_lmShipAutoLaunchBit(Ship *host)
{
    Ship *mothership;
    Ship *carriers[4];      // FillInCarrierMothershipInfo writes three; one spare as in cat_lmShipAutoLaunchOn
    sdword i;
    sdword bit = 2;

    if (singlePlayerGame && ((host->playerowner->playerIndex != 0) || (host->shipOrDerelict.specialFlags2 & SPECIAL_2_CatExempt20)))
    {
        return 0;
    }
    if ((host->staticinfo->shipclass != CLASS_Mothership) && (host->staticinfo->shipclass != CLASS_Carrier))
    {
        return 0;
    }
    mothership = host;
    FillInCarrierMothershipInfo(host->playerowner,&mothership,carriers);
    if (mothership == host)
    {
        return 1;
    }
    for (i = 0; i < 3; i++, bit <<= 1)
    {
        if (carriers[i] == host)
        {
            return bit;
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : cat_lmShipAutoLaunchOn
    Description : CAT: whether ships docked inside host launch again on their own (cataclysm.exe 0x0049fb00;
                  cat_dockShipDocksAtShip asks in dock state 8): TRUE unless host is one of the player's
                  mothership-class or carrier-class ships whose autolaunch bit (Player.autoLaunch: bit 0 the
                  mothership, bits 1..3 the three carriers of FillInCarrierMothershipInfo) is off; in single player
                  only the local player's ships without the SPECIAL_2_CatExempt20 flag are subject to the bits
    Inputs      : host
----------------------------------------------------------------------------*/
bool32 cat_lmShipAutoLaunchOn(Ship *host)
{
    Ship *mothership;
    Ship *carriers[4];      // [M2-R6 managers: FillInCarrierMothershipInfo writes three slots since M2-R5 command
                            //      (CommandLayer.c); the spare is harmless, the exe tests the first three]
    sdword i;
    udword bit = 2;

    if (singlePlayerGame && ((host->playerowner->playerIndex != 0) || (host->shipOrDerelict.specialFlags2 & SPECIAL_2_CatExempt20)))
    {
        return TRUE;
    }
    if ((host->staticinfo->shipclass != CLASS_Mothership) && (host->staticinfo->shipclass != CLASS_Carrier))
    {
        return TRUE;
    }
    mothership = host;
    FillInCarrierMothershipInfo(host->playerowner,&mothership,carriers);
    if (mothership == host)
    {
        return (host->playerowner->autoLaunch & 1) != 0;
    }
    for (i = 0; i < 3; i++, bit <<= 1)
    {
        if (carriers[i] == host)
        {
            return (host->playerowner->autoLaunch & bit) != 0;
        }
    }
    return TRUE;
}


/*-----------------------------------------------------------------------------
    Name        : lmUpdateShipsInside
    Description : update ship display in launch manager
    CAT (M2-R6 managers): the exe's body (0x0049e070): only while the screen is up; the counts and the list rebuilt
----------------------------------------------------------------------------*/
void lmUpdateShipsInside(void)
{
    if (lmBaseRegion!=NULL)
    {
        catLmCountShipsInside(FALSE);
        catLmListRebuild();
    }
}

/*-----------------------------------------------------------------------------
    Name        : lmLaunchBegin
    Description : Begin launch manager
    Inputs      : event handles
    CAT (M2-R6 managers): the exe's body (0x0049e1e0): no replay, pause (without HW1's opPauseOrders), tutorial gate bit
        0x100 of its first word, no mothership, or a single-player game's other players; the sensors manager closes
        and is remembered; the passed ship is used when it is the player's own mothership- or carrier-class ship
        without SPECIAL_2_CatExempt20, else the remembered slot's; the Sect or Beast screen (loaded again when the race
        changed), the module slots, no ship view, no input lock-out, no taskbar flag
----------------------------------------------------------------------------*/
sdword lmLaunchBegin(regionhandle region, smemsize ID, udword event, udword data)
{
    Player *player = universe.curPlayerPtr;
    Ship *ship = (Ship *)ID;
    sdword i;

    if (playPackets || universePause)
    {
        return 0;
    }
    if (!catLmTutEnabled1(0x100))
    {
        return 0;
    }
    if (player->PlayerMothership == NULL)
    {
        return 0;
    }
    if (singlePlayerGame && (player->playerIndex != 0))
    {
        return 0;
    }
    catLmSensorsWereOpen = smSensorsActive;
    if (smSensorsActive)
    {
        smSensorsCloseForGood();
    }
    catLmFillInCarrierXs();

    launchship = ship;
    if ((ship != NULL) && (ship->playerowner->playerIndex == player->playerIndex) &&
        !bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20) &&
        ((ship->staticinfo->shipclass == CLASS_Mothership) || (ship->staticinfo->shipclass == CLASS_Carrier)))
    {
        if (ship->staticinfo->shipclass == CLASS_Mothership)
        {
            lmCurrentSelect = 0;
        }
        else
        {
            for (i = 0; i < cmNumPlayersCarriers; i++)
            {
                // CAT-FIX: the exe compares up to cmNumPlayersCarriers entries of its two-entry array (the next words
                //      are the image table); the two carrier slots here
                if (i >= NUM_LMCARRIERS)
                {
                    break;
                }
                if (ship == lmCarrierX[i])
                {
                    lmCurrentSelect = i + 1;
                    break;
                }
            }
        }
    }
    else
    {
        launchship = NULL;
        if (lmCurrentSelect != 0)
        {
            launchship = lmCarrierX[lmCurrentSelect - 1];
        }
        if (launchship == NULL)
        {
            launchship = player->PlayerMothership;
            if (launchship->staticinfo->shipclass == CLASS_Carrier)
            {
                lmCurrentSelect = 1;
            }
            else if (launchship->staticinfo->shipclass == CLASS_Mothership)
            {
                lmCurrentSelect = 0;
            }
        }
    }
    if (launchship == NULL)
    {
        if (cmNumPlayersCarriers < 1)
        {
            return 0;
        }
        launchship = lmCarrierX[0];
        lmCurrentSelect = 1;
    }

    // disable rendering of main game screen
    mrRenderMainScreen = FALSE;
    lmActive = TRUE;

    lmRenderEverythingCounter = (tutorial == TUTORIAL_ONLY) ? 4 : 0;

    // clear the screen
    rndClear();

    catLmCountShipsInside(TRUE);

    soundEventStopSFX(0.5f);
    soundEventPlay(NULL, 0x12f, NULL);                      // CAT: the manager intro
    soundEventPlay(NULL, 0x102, NULL);                      // CAT: the launch manager's ambient

    if (lmScreensHandle == NULL)
    {
        feCallbackAddMultiple(lmCallback);                  //add in the callbacks
        feDrawCallbackAddMultiple(lmDrawCallback);
    }
    else if (universe.curPlayerPtr->race != catLmScreensRace)
    {
        feScreensDelete(lmScreensHandle);
        lmScreensHandle = NULL;
    }
    if (lmScreensHandle == NULL)
    {
        catLmScreensRace = (universe.curPlayerPtr->race == RACE_Sect) ? RACE_Sect : RACE_Beast;
        lmScreensHandle = feScreensLoad((catLmScreensRace == RACE_Sect) ? LM_FIBFile : CAT_LM_BeastFIBFile);
    }

    lmBaseRegion = feScreenStart(region, catLmScreenName());  //add new regions as siblings of current one

    mouseCursorShow();

    cat_lmFillInModuleSlots();

    cat_tutButtonsHideForScreen();           // CAT: the tutorial's buttons hide while the screen is open (lmLaunchBegin 0x0049e50b)

    return(0);
}

/*-----------------------------------------------------------------------------
    Name        : lmReLaunch
    Description : switch to another carrier or the mothership
    CAT: the exe's body (0x0049e520): IDENTICAL in effect to HW1's but for the list's class headers
----------------------------------------------------------------------------*/
void lmReLaunch(Ship *newship)
{
    launchship = newship;
    catLmCountShipsInside(TRUE);
    catLmListRebuild();
}

/*-----------------------------------------------------------------------
   Name        : lmLaunchCapableShipDied
   Description : This function will remove any launch manager references
                 to the ship that just died.
   CAT (M2-R6 managers): the exe's body (0x0049e690): the two carrier slots refilled, the shown ship replaced (the slot
        before it, else the mothership, else the first carrier), the screen closed when nothing is left (the close's
        tutorial gate as lmCloseIfOpen's); no ship view; the two carrier boxes follow their slots
-----------------------------------------------------------------------*/
void lmLaunchCapableShipDied(Ship *ship)
{
    Player *player = universe.curPlayerPtr;
    sdword i;
    bool32 fromSlot;

    catLmFillInCarrierXs();

    fromSlot = FALSE;
    if (launchship == ship)
    {
        launchship = NULL;
        lmCurrentSelect--;
        if (lmCurrentSelect < 0)
        {
            lmCurrentSelect = 0;
        }
        fromSlot = TRUE;
    }
    else if (launchship == NULL)
    {
        fromSlot = TRUE;
    }
    else if (launchship->staticinfo->shipclass == CLASS_Mothership)
    {
        lmCurrentSelect = 0;
    }
    else if (launchship->staticinfo->shipclass == CLASS_Carrier)
    {
        for (i = 0; i < NUM_LMCARRIERS; i++)
        {
            if (launchship == lmCarrierX[i])
            {
                lmCurrentSelect = i + 1;
                break;
            }
        }
    }
    else
    {
        fromSlot = TRUE;
    }

    if (fromSlot)
    {
        if (lmCurrentSelect != 0)
        {
            launchship = lmCarrierX[lmCurrentSelect - 1];
            if (launchship == NULL)
            {
                launchship = player->PlayerMothership;
                if (launchship != NULL)
                {
                    if (launchship->staticinfo->shipclass == CLASS_Carrier)
                    {
                        lmCurrentSelect = 1;
                    }
                    else if (launchship->staticinfo->shipclass == CLASS_Mothership)
                    {
                        lmCurrentSelect = 0;
                    }
                }
            }
        }
        else if (ship != player->PlayerMothership)
        {
            launchship = player->PlayerMothership;
            if (launchship != NULL)
            {
                if (launchship->staticinfo->shipclass == CLASS_Carrier)
                {
                    lmCurrentSelect = 1;
                }
                else if (launchship->staticinfo->shipclass == CLASS_Mothership)
                {
                    lmCurrentSelect = 0;
                }
            }
        }
    }

    if (launchship == NULL)
    {
        // CAT-FIX: the exe takes the first carrier slot whenever cmNumPlayersCarriers is positive and would read
        //      through a NULL launchship when that slot is empty (the counter not yet down for the dying carrier)
        if ((cmNumPlayersCarriers < 1) || (lmCarrierX[0] == NULL))
        {
            if (catLmTutEnabled2(0x100000))
            {
                catLmClose();
            }
            return;
        }
        launchship = lmCarrierX[0];
        lmCurrentSelect = 1;
    }

    lmUpdateShipsInside();

    catLmAtomEnable(feAtomFindInScreen(feScreenFind(catLmScreenName()), "LM_AutoLaunchC1"), lmCarrierX[0] != NULL);
    catLmAtomEnable(feAtomFindInScreen(feScreenFind(catLmScreenName()), "LM_AutoLaunchC2"), lmCarrierX[1] != NULL);

    regRecursiveSetDirty(lmBaseRegion);
}

/*=============================================================================
    HWI-TEST (M2-R6 managers), port diagnostic, not in the exe: the build self-test's view of the launch screen
    (ConsMgr.c catBuildSelfTestScreens)
=============================================================================*/
bool32 catLmSelfTestDrawn(void)
{
    sdword i, slots = 0;

    for (i = 0; i < CAT_LM_NUM_MODULE_SLOTS; i++)
    {
        slots += (cat_lmModuleSlots[i] != NULL);
    }
    return (catLmFactoryNameRegion != NULL) && (catLmFactoryShip != NULL) && (slots > 0);
}

char *catLmSelfTestState(void)
{
    static char line[160];
    sdword i, slots = 0;

    for (i = 0; i < CAT_LM_NUM_MODULE_SLOTS; i++)
    {
        slots += (cat_lmModuleSlots[i] != NULL);
    }
    snprintf(line, sizeof(line), "factoryname=%d factory=%s select=%d moduleslots=%d rows=%d",
             (int)(catLmFactoryNameRegion != NULL), (catLmFactoryShip != NULL) ? ShipTypeToStr(catLmFactoryShip->shiptype) : "none",
             (int)lmCurrentSelect, (int)slots, (lmListWindow != NULL) ? (int)lmListWindow->ListTotal : -1);
    return line;
}

void catLmSelfTestLaunchOne(void)
{
    cat_lmLaunchOne(NULL, NULL);
}
