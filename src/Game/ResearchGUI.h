/*=============================================================================
    Name    : ResearchGUI.h
    Purpose : definitions for the research manager gui

    Created 5/25/1998 by ddunlop
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___RESEARCHGUI_H
#define ___RESEARCHGUI_H

#include "Region.h"
#include "ResearchAPI.h"
#include "texreg.h"
#include "Types.h"

/*=============================================================================
    Definitions:
=============================================================================*/

#ifdef _WIN32
#define RM_FIBFile          "FEMan\\Research_Manager.fib"
#else
#define RM_FIBFile          "FEMan/Research_Manager.fib"
#endif
#define RM_ResearchScreen   "Research_Manager"
#define RM_DefaultFont      "default.hff"
#define RM_FontNameLength   64

#define RM_TechListFont     "HW_EuroseCond_11.hff"
#define RM_TechInfoFont     "Arial_12.hff"

// definitions for the status of a research item

#define RI_GRAYED       0
#define RI_RESEARCHING

// definitions for print list

#define ITEM_ENDLIST        -1
#define ITEM_CLASSHEADER    0
#define ITEM_TECHNOLOGY     1   // CAT: a row that is never listed (rmUpdateTechList 0x004f6d30 sets it STAT_CANTRESEARCH);
                                //      a Beast player's row becomes CAT_ITEM_TECH2 / 3 once a ship brings its technology
                                //      (cat_rmAcquireShipTechs 0x004f4870)
// CAT (M2-R6 managers): the listed technology rows of the exe's research lists are kinds 2 and 3 (rmUpdateTechList,
//      rmTechListWindow 0x004f6ae0; its item draw 0x004f6810 has no Ghidra function). Names chosen here
#define CAT_ITEM_TECH2      2
#define CAT_ITEM_TECH3      3

#define STAT_CANRESEARCH    0
#define STAT_CANTRESEARCH   1
#define STAT_RESEARCHING    2
#define STAT_ALREADYHAVE    4
#define STAT_CANPRINT       5
#define STAT_CANTPRINT      6

// colors for text in the research manager
#define RM_SelectionTextColor     colRGB(255, 255, 105)
#define RM_ResearchingTextColor   colRGB(255, 200, 0)
#define RM_CantResearchTextColor  colRGB(100, 100, 100)
#define RM_StandardTextColor      colRGB(0  , 255, 0)
#define RM_ClassHeadingTextColor  colRGB(255, 200, 0)
#define RM_ProgressToGoColor      colRGB(180, 180, 80)
#define RM_ProgressDoneColor0     colRGB(50, 50, 27)
#define RM_ProgressDoneColor1     colRGB(160, 160, 115)
#define RM_LabActiveColor         colRGB(0  , 160, 20)
#define RM_PulseColor             colRGB(20 , 20 , 255)
#define RM_NoResearchItemColor    colRGB(255, 0, 0)
#define RM_MarqueeOnColor         colRGB(255, 255, 0)
#define RM_MarqueeSemiOnColor     colRGB(175, 175, 0)
#define RM_MarqueeOffColor        colRGB(100, 100, 0)

// CAT: three research tables (rmAPIStartup 0x004f3c00 calls rmSetPrintList(1, SP Sect), (3, MP Sect), (4, Beast));
//      the HW1 ids 2, 5, 6 have no table (M2-R6 managers: rmSetPrintList 0x004f75f0 copies the exe's two static lists)
#define RM_SPRS                   1
#define RM_SPR1                   RM_SPRS
#define RM_SPR2                   2
#define RM_MPRS                   3
#define RM_MPR1                   RM_MPRS
#define RM_MPRB                   4
#define RM_MPR2                   RM_MPRB
#define RM_MPCR1                  5
#define RM_MPCR2                  6

/*=============================================================================
    Type definitions:
=============================================================================*/

// CAT (M2-R6 managers): 12 bytes in Cataclysm (rmSetPrintList 0x004f75f0 copies 3 dwords a row): the fourth word is the
//      lab of a Sect row (rmUpdateTechList reads the lab's state at Player +0x2EC + lab * 8 + 0x74)
typedef struct
{
    sword  itemtype;
    sword  itemstat;
    sdword itemID;
    sdword catLab;
}
TechPrintList;
_Static_assert(sizeof(TechPrintList) == 12, "a research list row is 12 bytes in cataclysm.exe (rmSetPrintList 0x004f75f0)");

// CAT (M2-R6 managers): the research lists (ResearchGUI.c) and their users
#define CAT_RM_TECHLIST_MAX     192     // the exe's lists 0x00c168c0 / 0x00c17b20 / 0x00c18420 are 0x900 bytes apart
extern TechPrintList *PlayerTechList;   // 0x00c17aa8
void cat_rmSelectTechPrintList(void);                       // 0x004f7a40
ShipType cat_rmBeastPrintIndexToShipType(sdword index);     // 0x004f4790

typedef struct
{
    sword  labid;
    sword  selected;
    sdword pulsepos;
    ResearchLab *lab;
}
LabPrintList;

typedef struct
{
    real32 x;
    real32 y;
}
Point;

typedef struct
{
    char *name;
}
TechNames;

typedef struct
{
    lifheader      *techImage;
    udword          techTexture;
    TechnologyType  tech;
    sdword          race;
    real32          timestamp;
}
LRUPicture;

extern bool32 rmGUIActive;

// HWI-TEST (M2-R6 managers): the build self-test's view of the research screen (ConsMgr.c catBuildSelfTestScreens)
extern regionhandle rmBaseRegion;
extern regionhandle rmLabRegion[NUM_RESEARCHLABS];
extern regionhandle catRmModuleNameRegion;
extern struct uiclistwindow *rmTechListWindowHandle;

// LRU defines, so that the trade manager can reuse the same LRU cache
#define RM_TOTALPICS            6

extern LRUPicture pictures[RM_TOTALPICS];


/*=============================================================================
    Function Prototypes:
=============================================================================*/

// function called if something changes in the list stuff
void rmUpdateTechList(void);
void rmClearAllLabs(char *string, featom *atom);   // CAT: rmUpdateResearch 0x004f2a20 clears every lab of the GUI
// CAT (M2-R6 managers): HW1's rmClearLab is gone (the exe's RM_ClearLab stops the selected lab's research)

//start the Research Manager.  It will kill itself when you hit the launch button.
void   rmSetPrintList(udword whichlist, TechStatics *techstat);
sdword rmResearchGUIBegin(regionhandle region, smemsize ID, udword event, udword data);
void   rmGUIStartup(void);
void   rmGUIShutdown(void);
void rmCloseIfOpen(void);
void rmExitMenu(char *string, featom *atom);         // the research manager's Close button

#endif
