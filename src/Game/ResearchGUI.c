/*=============================================================================
    Name    : researchgui.c
    Purpose : logic for the research manager

    Created 5/27/1998 by ddunlop
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include <math.h>
#include <stdio.h>

#include "CommandLayer.h"
#include "CommandNetwork.h"
#include "CommandWrap.h"
#include "FEColour.h"
#include "FEFlow.h"
#include "FEReg.h"
#include "font.h"
#include "FontReg.h"
#include "glinc.h"
#include "Globals.h"
#include "InfoOverlay.h"
#include "main.h"
#include "mainrgn.h"
#include "Memory.h"
#include "mouse.h"
#include "MultiplayerGame.h"
#include "ObjTypes.h"
#include "Options.h"
#include "PiePlate.h"
#include "prim2d.h"
#include "Randy.h"
#include "render.h"
#include "ResearchGUI.h"
#include "Scroller.h"
#include "ShipDefs.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "SoundEventDefs.h"
#include "StringSupport.h"
#include "Task.h"
#include "TaskBar.h"
#include "TradeMgr.h"
#include "Tutor.h"
#include "UIControls.h"
#include "Universe.h"
#include "Sensors.h"
#include "ShipSelect.h"
#include "utility.h"
#include <string.h>


#ifndef SW_Render
    #ifdef _WIN32
        #include <windows.h>
    #endif
#endif


/*=============================================================================
    Defines:
=============================================================================*/

// CAT (M2-R6 managers): the research manager is the exe's (0x004f4bd0 .. 0x004f7a40): five lab buttons (the Sect
//      modules, or the Beast's one lab), the module's name over the picture, the Beast's animation ("PussyBlob"), the
//      technology list of the race's exe list, and buttons to the construction and launch managers. HW1's tech brief,
//      lab connectors, sixth lab, marquees, select-all and class headings are gone.
#ifdef _WIN32
#define CAT_RM_BeastFIBFile         "FEMan\\Beast_Research_Manager.fib"
#else
#define CAT_RM_BeastFIBFile         "FEMan/Beast_Research_Manager.fib"
#endif
#define CAT_RM_BeastResearchScreen  "Beast_Research_Manager"
#define CAT_RM_DefaultImage         "Default_Research_Image.lif"   // 0x0089224c

#define RM_TEXTURE_INSET        3

/*=============================================================================
    Data:
=============================================================================*/

static sdword rmRenderEverythingCounter;    // 0x00a576ec

double chop_temp;                           // 0x00c16840: the exe's fast float-to-int (+ 2^52 * 1.5)

bool32 rmPaletted;                          // 0x00c1689c

fonthandle rmTechListFont=0;                // 0x00a576fc
fonthandle rmTechInfoFont=0;                // 0x00a57700
fonthandle catRmHeadingFont = 0;            // 0x00a57704: "EuroseWideHeavy_i8.hff" (.data 0x008912f0)
char rmTechListFontName [RM_FontNameLength] = RM_TechListFont;
char rmTechInfoFontName [RM_FontNameLength] = RM_TechInfoFont;

//flag indicating loaded status of this screen
fibfileheader *rmScreensHandle = NULL;      // 0x00a57708
static ShipRace catRmScreensRace = RACE_Sect;   // 0x00a5770c

regionhandle  rmTechImageRegion = NULL;     // 0x00a57710
regionhandle  rmTechBriefRegion = NULL;     // 0x00a57714: cleared and redrawn by the exe, never set (HW1's brief)
regionhandle  rmBaseRegion      = NULL;     // 0x00a57718
regionhandle  rmLabRegion[NUM_RESEARCHLABS] = { NULL, NULL, NULL, NULL, NULL };   // 0x00a5771c .. 0x00a5772c
regionhandle  catRmModuleNameRegion = NULL; // 0x00a57730
regionhandle  catRmBlobRegion = NULL;       // 0x00a57734

listwindowhandle rmTechListWindowHandle = NULL;     // 0x00a57738

static sdword catRmBlobFrame = 0;           // 0x00a5773c
static real32 catRmBlobTime = 0.0f;         // 0x00a57740
// is TRUE if GUI is active FALSE otherwise.
bool32 rmGUIActive=FALSE;                   // 0x00a57744
static bool32 catRmSensorsWereOpen = FALSE; // 0x00a57748
lifheader      *rmCurTechImage = NULL;      // 0x00a5774c
static real32 catRmLastTime = 0.0f;         // 0x00a57750: the marquee's and the animation's clock (both draws set it)
real32          marqueetime = 0.0f;         // 0x00a57754
bool32 rmExtendedInfoActive = FALSE;        // 0x00a57758

sdword          rmCurrentLab = -1;          // 0x00891ec0: the selected lab (rmClearAllLabs clears it)
TechnologyType  techinfo = -1;              // 0x00891f50: the shown technology (0x29 + lab: the lab's module)
TechnologyType  rmCurTechTexture = -1;      // 0x00891f58
sdword          rmCurIndex = -1;            // 0x00891f4c
udword          rmCurTexture = TR_InvalidInternalHandle;   // 0x00891f54

// the Beast animation's frame sequence (0x00891ee4)
static const sdword catRmBlobFrames[10] = { 1, 2, 3, 4, 5, 4, 3, 2, 1, 0 };

//colors for printing selections and stuff (.data 0x00891f0c)
color rmSelectionTextColor     = RM_SelectionTextColor;
color rmResearchingTextColor   = RM_ResearchingTextColor;
color catRmTech3TextColor      = colRGB(0, 215, 155);      // 0x00891f14: a CAT_ITEM_TECH3 row
color catRmTech3HaveTextColor  = colRGB(0, 130, 94);       // 0x00891f18: a CAT_ITEM_TECH3 row already researched
color rmCantResearchTextColor  = RM_CantResearchTextColor;
color rmStandardTextColor      = RM_StandardTextColor;
color rmClassHeadingTextColor  = RM_ClassHeadingTextColor;
color rmProgressToGoColor      = RM_ProgressToGoColor;
color rmProgressDoneColor0     = RM_ProgressDoneColor0;
color rmProgressDoneColor1     = RM_ProgressDoneColor1;
color rmLabActiveColor         = RM_LabActiveColor;
color rmPulseColor             = RM_PulseColor;
color rmNoResearchItemColor    = RM_NoResearchItemColor;
color rmMarqueeOnColor         = RM_MarqueeOnColor;
color rmMarqueeSemiOnColor     = RM_MarqueeSemiOnColor;
color rmMarqueeOffColor        = RM_MarqueeOffColor;

// List of lab buttons
LabPrintList labbuttons[NUM_RESEARCHLABS];  // 0x00c16860

// image variables for technology bitmap
LRUPicture      pictures[RM_TOTALPICS];     // 0x00c16780

char* TechImagePaths[]=                     // 0x00891e70
{
#ifdef _WIN32
    "ResearchGUI\\Race1\\",
    "ResearchGUI\\Race2\\",
#else
    "ResearchGUI/Race1/",
    "ResearchGUI/Race2/",
#endif
    NULL
};

// CAT: Cataclysm's research art (rmGUIStartup 0x004f76f0). Somtaaw (Race1): 15 lab buttons
//      "ResearchGUI\Race1\" + "Research_icon_" + {Hangar,Armour,Engineering,Special,Weapons}_{disabled,off,on}.lif
//      (.data 0x00891e7c, names table 0x00891e80; images 0x00c17ac0, textures 0x00c16800). Beast (Race2): 6 animation
//      frames "ResearchGUI\Race2\" + "bresearch_ani_" + "01".."06" ".lif" (.data 0x00891ec4, 0x00891ec8; images
//      0x00c168a0, textures 0x00c17b00)
#define CAT_RM_NUM_LABICONS     15
#define CAT_RM_NUM_BEASTFRAMES  6
char *catRmLabIcons[CAT_RM_NUM_LABICONS] =
{
    "Hangar_disabled.lif", "Armour_disabled.lif", "Engineering_disabled.lif", "Special_disabled.lif", "Weapons_disabled.lif",
    "Hangar_off.lif",      "Armour_off.lif",      "Engineering_off.lif",      "Special_off.lif",      "Weapons_off.lif",
    "Hangar_on.lif",       "Armour_on.lif",       "Engineering_on.lif",       "Special_on.lif",       "Weapons_on.lif"
};
char *catRmBeastFrames[CAT_RM_NUM_BEASTFRAMES] = { "01.lif", "02.lif", "03.lif", "04.lif", "05.lif", "06.lif" };
udword          catRmLabTexture[CAT_RM_NUM_LABICONS] =
{
    TR_InvalidInternalHandle, TR_InvalidInternalHandle, TR_InvalidInternalHandle, TR_InvalidInternalHandle,
    TR_InvalidInternalHandle, TR_InvalidInternalHandle, TR_InvalidInternalHandle, TR_InvalidInternalHandle,
    TR_InvalidInternalHandle, TR_InvalidInternalHandle, TR_InvalidInternalHandle, TR_InvalidInternalHandle,
    TR_InvalidInternalHandle, TR_InvalidInternalHandle, TR_InvalidInternalHandle
};
lifheader      *catRmLabImage[CAT_RM_NUM_LABICONS];
udword          catRmBeastTexture[CAT_RM_NUM_BEASTFRAMES] =
{
    TR_InvalidInternalHandle, TR_InvalidInternalHandle, TR_InvalidInternalHandle,
    TR_InvalidInternalHandle, TR_InvalidInternalHandle, TR_InvalidInternalHandle
};
lifheader      *catRmBeastImage[CAT_RM_NUM_BEASTFRAMES];

// callbacks for ui elements (the exe's table 0x00891330)
void rmClearAllLabs(char *string, featom *atom);
void rmClearSelectedLab(char *string, featom *atom);
void rmResearchItem(char *string, featom *atom);
void rmExitMenu(char *string, featom *atom);
void rmTechListWindow(char *string, featom *atom);
void rmExtendedInfo(char *string, featom *atom);
void cat_rmConstruction(char *string, featom *atom);
void cat_rmLaunch(char *string, featom *atom);

// callbacks for user draw areas (the exe's table 0x00891378)
void rmLab1Draw(featom *atom, regionhandle region);
void rmLab2Draw(featom *atom, regionhandle region);
void rmLab3Draw(featom *atom, regionhandle region);
void rmLab4Draw(featom *atom, regionhandle region);
void rmLab5Draw(featom *atom, regionhandle region);
void rmTechImageDraw(featom *atom, regionhandle region);
void cat_rmModuleNameDraw(featom *atom, regionhandle region);
void cat_rmPussyBlobDraw(featom *atom, regionhandle region);

fecallback rmCallback[] =
{
    {rmClearAllLabs         ,   "RM_ClearAllLabs"       },
    {rmClearSelectedLab     ,   "RM_ClearLab"           },
    {rmResearchItem         ,   "RM_ResearchItem"       },
    {rmExitMenu             ,   "RM_ExitMenu"           },
    {rmTechListWindow       ,   "RM_TechListWindow"     },
    {rmExtendedInfo         ,   "RM_ExtendedInfo"       },
    {cat_rmConstruction     ,   "RM_Construction"       },
    {cat_rmLaunch           ,   "RM_Launch"             },
    {NULL,                      NULL                    }
};

fedrawcallback rmDrawCallback[] =
{
    {rmLab1Draw,                "RM_Lab1Draw"           },
    {rmLab2Draw,                "RM_Lab2Draw"           },
    {rmLab3Draw,                "RM_Lab3Draw"           },
    {rmLab4Draw,                "RM_Lab4Draw"           },
    {rmLab5Draw,                "RM_Lab5Draw"           },
    {rmTechImageDraw,           "RM_TechImageDraw"      },
    {cat_rmModuleNameDraw,      "RM_ModuleNameDraw"     },
    {cat_rmPussyBlobDraw,       "RM_PussyBlob"          },
    {NULL,                      NULL                    }
};

// CAT: the Sect research list (.data 0x008913c0, rmSetPrintList 0x004f75f0 lists 1 and 3): a header per lab (itemID the lab's name index, catLab the lab), then its technologies
TechPrintList catRmSectTechListStatic[38] =
{
    {ITEM_CLASSHEADER,  0, 4, 0},
    {CAT_ITEM_TECH2,    0, TECH_DRIVE_FIGHTER, 0},
    {CAT_ITEM_TECH2,    0, TECH_DRIVE_FRIGATE, 0},
    {CAT_ITEM_TECH2,    0, TECH_DRIVE_DESTROYER, 0},
    {CAT_ITEM_TECH2,    0, TECH_LINK, 0},
    {CAT_ITEM_TECH3,    0, TECH_ADRIVE_FIGHTER, 0},
    {CAT_ITEM_TECH3,    0, TECH_ADRIVE_FRIGATE, 0},
    {CAT_ITEM_TECH3,    0, TECH_ADRIVE_DESTROYER, 0},
    {ITEM_CLASSHEADER,  0, 5, 1},
    {CAT_ITEM_TECH3,    0, TECH_ARMOUR2, 1},
    {CAT_ITEM_TECH3,    0, TECH_ARMOUR3, 1},
    {CAT_ITEM_TECH3,    0, TECH_FORCE_FIELD1, 1},
    {CAT_ITEM_TECH3,    0, TECH_FORCE_FIELD2, 1},
    {CAT_ITEM_TECH3,    0, TECH_FORCE_FIELD3, 1},
    {ITEM_CLASSHEADER,  0, 6, 2},
    {CAT_ITEM_TECH2,    0, TECH_MIMIC, 2},
    {CAT_ITEM_TECH2,    0, TECH_SUPPORT_SYSTEMS, 2},
    {CAT_ITEM_TECH2,    0, TECH_AFTERBURNER, 2},
    {CAT_ITEM_TECH3,    0, TECH_REPAIR, 2},
    {CAT_ITEM_TECH3,    0, TECH_SALVAGE, 2},
    {CAT_ITEM_TECH3,    0, TECH_RESEARCH_CLEESAN_ATTACK, 2},
    {ITEM_CLASSHEADER,  0, 7, 3},
    {CAT_ITEM_TECH2,    0, TECH_MICRO_SHIP, 3},
    {CAT_ITEM_TECH2,    0, TECH_PHASED_TELEMETRY, 3},
    {CAT_ITEM_TECH3,    0, TECH_ADVANCED_SENSORS, 3},
    {CAT_ITEM_TECH3,    0, TECH_REPAIRBOTS, 3},
    {CAT_ITEM_TECH3,    0, TECH_BEAST_VACCINE, 3},
    {CAT_ITEM_TECH3,    0, TECH_CRYSTAL_HARVEST, 3},
    {CAT_ITEM_TECH3,    0, TECH_INERTIALESS_DRIVE, 3},
    {ITEM_CLASSHEADER,  0, 8, 4},
    {CAT_ITEM_TECH2,    0, TECH_ION, 4},
    {CAT_ITEM_TECH2,    0, TECH_MISSILE, 4},
    {CAT_ITEM_TECH2,    0, TECH_BIG_GUN, 4},
    {CAT_ITEM_TECH3,    0, TECH_ENERGY, 4},
    {CAT_ITEM_TECH3,    0, TECH_EMP, 4},
    {CAT_ITEM_TECH3,    0, TECH_EXPLOSIVE_CHARGE, 4},
    {CAT_ITEM_TECH3,    0, TECH_REPULSE, 4},
    {ITEM_ENDLIST,       0, 0,  0}
};

// CAT: the Beast research list (.data 0x00891588, rmSetPrintList list 4): a header per ship (itemID a print index, cat_rmBeastPrintIndexToShipType 0x004f4790), then the technologies that upgrade it (-1: an empty row)
TechPrintList catRmBeastTechListStatic[190] =
{
    {ITEM_CLASSHEADER,  0, 9, 0},
    {CAT_ITEM_TECH3,    0, TECH_SUPPORT_SYSTEMS, 0},
    {CAT_ITEM_TECH3,    0, TECH_INFECT, 0},
    {CAT_ITEM_TECH3,    0, TECH_CRYSTAL_HARVEST, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ENERGY, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR3, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_CLASSHEADER,  0, 10, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ENERGY, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ADRIVE_FIGHTER, 0},
    {CAT_ITEM_TECH3,    0, TECH_ADVANCED_SENSORS, 0},
    {ITEM_CLASSHEADER,  0, 11, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR3, 0},
    {ITEM_TECHNOLOGY,   0, TECH_REPAIR, 0},
    {CAT_ITEM_TECH3,    0, TECH_SALVAGE, 0},
    {ITEM_CLASSHEADER,  0, 12, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ENERGY, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR3, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_REPAIR, 0},
    {CAT_ITEM_TECH3,    0, TECH_CRYSTAL_HARVEST, 0},
    {ITEM_CLASSHEADER,  0, 13, 0},
    {ITEM_TECHNOLOGY,   0, TECH_DRIVE_FIGHTER, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ENERGY, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ADRIVE_FIGHTER, 0},
    {ITEM_TECHNOLOGY,   0, TECH_LINK, 0},
    {ITEM_TECHNOLOGY,   0, TECH_MISSILE, 0},
    {ITEM_CLASSHEADER,  0, 14, 0},
    {ITEM_TECHNOLOGY,   0, TECH_DRIVE_FIGHTER, 0},
    {ITEM_TECHNOLOGY,   0, TECH_LINK, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ENERGY, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ADRIVE_FIGHTER, 0},
    {ITEM_TECHNOLOGY,   0, TECH_EMP, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_CLASSHEADER,  0, 29, 0},
    {ITEM_TECHNOLOGY,   0, TECH_AFTERBURNER, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ION, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR3, 0},
    {ITEM_CLASSHEADER,  0, 15, 0},
    {ITEM_TECHNOLOGY,   0, TECH_DRIVE_FRIGATE, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ION, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR3, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ADRIVE_FRIGATE, 0},
    {ITEM_CLASSHEADER,  0, 16, 0},
    {ITEM_TECHNOLOGY,   0, TECH_DRIVE_FRIGATE, 0},
    {ITEM_TECHNOLOGY,   0, TECH_MICRO_SHIP, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ENERGY, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR3, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ADRIVE_FRIGATE, 0},
    {ITEM_CLASSHEADER,  0, 17, 0},
    {ITEM_TECHNOLOGY,   0, TECH_MICRO_SHIP, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ENERGY, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR3, 0},
    {ITEM_TECHNOLOGY,   0, TECH_FORCE_FIELD1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_FORCE_FIELD2, 0},
    {ITEM_TECHNOLOGY,   0, TECH_FORCE_FIELD3, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ADVANCED_SENSORS, 0},
    {ITEM_CLASSHEADER,  0, 18, 0},
    {CAT_ITEM_TECH2,    0, TECH_DRIVE_FIGHTER, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ADRIVE_FIGHTER, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ENERGY, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_CLASSHEADER,  0, 19, 0},
    {CAT_ITEM_TECH2,    0, TECH_DRIVE_FIGHTER, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {CAT_ITEM_TECH2,    0, TECH_CLOAKING, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ADRIVE_FIGHTER, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ENERGY, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_CLASSHEADER,  0, 20, 0},
    {CAT_ITEM_TECH2,    0, TECH_DRIVE_FIGHTER, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {CAT_ITEM_TECH2,    0, TECH_PLASMA_WEAPONS, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ADRIVE_FIGHTER, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_CLASSHEADER,  0, 21, 0},
    {CAT_ITEM_TECH2,    0, TECH_DRIVE_CORVETTE, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {CAT_ITEM_TECH2,    0, TECH_BIOWARHEAD, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR3, 0},
    {CAT_ITEM_TECH3,    0, TECH_INFECT, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_CLASSHEADER,  0, 22, 0},
    {CAT_ITEM_TECH2,    0, TECH_DRIVE_CORVETTE, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {CAT_ITEM_TECH2,    0, TECH_MISSILE, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_CLASSHEADER,  0, 23, 0},
    {CAT_ITEM_TECH2,    0, TECH_DRIVE_CORVETTE, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {CAT_ITEM_TECH2,    0, TECH_TARGETING_SYSTEMS, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ENERGY, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_CLASSHEADER,  0, 24, 0},
    {CAT_ITEM_TECH2,    0, TECH_DRIVE_CORVETTE, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {CAT_ITEM_TECH2,    0, TECH_MEDIUM_GUNS, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ENERGY, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_CLASSHEADER,  0, 25, 0},
    {CAT_ITEM_TECH2,    0, TECH_DRIVE_FRIGATE, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {CAT_ITEM_TECH2,    0, TECH_DEFENSE_FIELD, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ADRIVE_FRIGATE, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR3, 0},
    {ITEM_CLASSHEADER,  0, 26, 0},
    {CAT_ITEM_TECH2,    0, TECH_DRIVE_FRIGATE, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {CAT_ITEM_TECH2,    0, TECH_ION, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {CAT_ITEM_TECH3,    0, TECH_CLOAKING, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ADRIVE_FRIGATE, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR3, 0},
    {ITEM_CLASSHEADER,  0, 27, 0},
    {CAT_ITEM_TECH2,    0, TECH_DRIVE_DESTROYER, 0},
    {CAT_ITEM_TECH2,    0, TECH_HEAVY_GUNS, 0},
    {CAT_ITEM_TECH2,    0, TECH_ION, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ENERGY, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR3, 0},
    {CAT_ITEM_TECH3,    0, TECH_INFECT, 0},
    {ITEM_CLASSHEADER,  0, 28, 0},
    {CAT_ITEM_TECH2,    0, TECH_DRIVE_DESTROYER, 0},
    {CAT_ITEM_TECH2,    0, TECH_SUPPORT_SYSTEMS, 0},
    {ITEM_TECHNOLOGY,   0, -1, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ENERGY, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR2, 0},
    {ITEM_TECHNOLOGY,   0, TECH_ARMOUR3, 0},
    {CAT_ITEM_TECH3,    0, TECH_INFECT, 0},
    {CAT_ITEM_TECH3,    0, TECH_CRYSTAL_HARVEST, 0},
    {ITEM_ENDLIST,       0, 0,  0}
};

// CAT: the lists the game works on (rmSetPrintList fills them; cat_rmSelectTechPrintList picks one into PlayerTechList)
TechPrintList SinglePlayerR1TechList[CAT_RM_TECHLIST_MAX];     // 0x00c17b20 (list 1: single-player Sect)
TechPrintList SinglePlayerR2TechList[CAT_RM_TECHLIST_MAX];     // 0x00c171c0 (a single-player Beast player: nothing fills it)
TechPrintList MultiPlayerR1TechList[CAT_RM_TECHLIST_MAX];      // 0x00c18420 (list 3: multiplayer Sect)
TechPrintList MultiPlayerR2TechList[CAT_RM_TECHLIST_MAX];      // 0x00c168c0 (list 4: Beast)

TechPrintList *PlayerTechList;              // 0x00c17aa8

/*=============================================================================
    Functions:
=============================================================================*/

// CAT (M2-R6 managers): the exe's tutorial gates test bits of its two tutEnable words 0x00ba2e58 / 0x00ba2e5c (as
//      mainrgn.c's catMrTutEnabled); outside the tutorial everything is enabled.
//      CAT-TODO(stage 6 tutorial): the exe's tutGameEnableFlags layout
static bool32 catRmTutEnabled1(udword bit)
{
    return (tutorial != TUTORIAL_ONLY) || ((((udword *)&tutEnable)[0] & bit) != 0);
}

static bool32 catRmTutEnabled2(udword bit)
{
    return (tutorial != TUTORIAL_ONLY) || ((((udword *)&tutEnable)[1] & bit) != 0);
}

// CAT (M2-R6 managers): the mission's research allowance (0x00ba4680, TradeMgr.c's tmTechResearchable: AllowPlayerToResearch)
static bool32 catRmResearchAllowed(TechnologyType tech)
{
    return tmTechResearchable[tech] != 0;
}

// CAT (M2-R6 managers): the multiplayer option that allows the crystal-harvest research (bit 0x20 of the game options word
//      0x008860ec, the crystals option of Salvage.c) [audit: M2-R6 managers: said the Beast infection; the exe tests
//      technology 0x22 = TECH_CRYSTAL_HARVEST, rmUpdateTechList 0x004f6d30]
// CAT-TODO(stage 5 multiplayer options): the Cataclysm tpGameCreated layout; until then the crystals option counts as off
static bool32 catRmGameOptionCrystals(void)
{
    return FALSE;
}

// CAT (M2-R6 managers): the screen's regions the exe redraws after a list change (rmUpdateTechList 0x004f6d30)
static void catRmDirtyRegions(void)
{
    sdword i;

    if (rmTechBriefRegion != NULL)
    {
        rmTechBriefRegion->status |= RSF_DrawThisFrame;
    }
    for (i = 0; i < NUM_RESEARCHLABS; i++)
    {
        if (rmLabRegion[i] != NULL)
        {
            rmLabRegion[i]->status |= RSF_DrawThisFrame;
        }
    }
    if (catRmBlobRegion != NULL)
    {
        catRmBlobRegion->status |= RSF_DrawThisFrame;
    }
}

// CAT (M2-R6 managers): a technology's picture into the LRU cache (the exe inlines this in rmTechImageDraw 0x004f6380 and
//      rmResearchGUIBegin 0x004f7140): a cached one is made current, else the least recently used slot is freed and the
//      file loaded (paletted)
static void catRmPictureUse(sdword index)
{
    if (pictures[index].techTexture == TR_InvalidInternalHandle)
    {
        pictures[index].techTexture = trPalettedTextureCreate(pictures[index].techImage->data, pictures[index].techImage->palette,
                                                              pictures[index].techImage->width, pictures[index].techImage->height);
    }
    else
    {
        trPalettedTextureMakeCurrent(pictures[index].techTexture, pictures[index].techImage->palette);
    }
    rmCurTexture = pictures[index].techTexture;
    rmCurTechTexture = techinfo;
    rmCurIndex = index;
}

static void catRmPictureLoad(char *filename)
{
    sdword index, lru = 0;
    real32 time = (real32)1.0e22;

    for (index = 0; index < RM_TOTALPICS; index++)
    {
        if (pictures[index].timestamp < time)
        {
            time = pictures[index].timestamp;
            lru  = index;
        }
        if ((pictures[index].tech == techinfo) && (pictures[index].race == (sdword)universe.curPlayerPtr->race))
        {
            catRmPictureUse(index);
            rmCurTechImage = pictures[lru].techImage;       // the exe stores the LRU slot's image here
            pictures[index].timestamp = universe.totaltimeelapsed;
            return;
        }
    }
    if (pictures[lru].techImage != NULL)
    {
        memFree(pictures[lru].techImage);
        pictures[lru].techImage = NULL;
    }
    if (pictures[lru].techTexture != TR_InvalidInternalHandle)
    {
        trRGBTextureDelete(pictures[lru].techTexture);
        pictures[lru].techTexture = TR_InvalidInternalHandle;
    }
    pictures[lru].techImage = trLIFFileLoad(filename, NonVolatile);
    catRmPictureUse(lru);
    rmCurTechImage = pictures[lru].techImage;
    pictures[lru].tech = techinfo;
    pictures[lru].race = universe.curPlayerPtr->race;
    pictures[lru].timestamp = universe.totaltimeelapsed;
}

/*-----------------------------------------------------------------------------
    Name        : rmClearAllLabs
    Description : CAT (M2-R6 managers): the exe's body (0x004f4bd0, RM_ClearAllLabs; rmUpdateResearch calls it): no lab
                  selected any more
----------------------------------------------------------------------------*/
void rmClearAllLabs(char *string, featom *atom)
{
    rmCurrentLab = -1;
}

/*-----------------------------------------------------------------------------
    Name        : rmClearSelectedLab
    Description : CAT (M2-R6 managers): the exe's body (0x004f4be0, RM_ClearLab, no Ghidra function): the selected lab
                  stops its research (clWrapResearch type 1) and the speech 0x4001032; the tutorial gate bit 0x1000 of its
                  second word
----------------------------------------------------------------------------*/
void rmClearSelectedLab(char *string, featom *atom)
{
    if ((rmCurrentLab == -1) || (labbuttons[rmCurrentLab].lab->labstatus != LS_RESEARCHITEM))
    {
        return;
    }
    if ((tutorial == TUTORIAL_ONLY) && !catRmTutEnabled2(0x1000))
    {
        return;
    }
    clWrapResearch(1, universe.curPlayerIndex, rmCurrentLab, 0);
    speechEventQueue(NULL, 0x4001032, -1, -1, 0, universe.curPlayerIndex, -1, 5.0f, -1);
}

/*-----------------------------------------------------------------------------
    Name        : rmResearchItem
    Description : research the selected technology
    CAT (M2-R6 managers): the exe's body (0x004f4c50): the row's own lab (catLab) starts researching it when it is free
        (or, in a tutorial, not built yet): clWrapResearch type 0 with the lab and the technology, the speech 0x4001031;
        that lab becomes the selected one; a technology already held is refused; the tutorial gate bit 0x800
----------------------------------------------------------------------------*/
void rmResearchItem(char *string, featom *atom)
{
    TechPrintList *row;
    ResearchLab *lab;

    if ((rmTechListWindowHandle->CurLineSelected == NULL) ||
        ((row = (TechPrintList *)rmTechListWindowHandle->CurLineSelected->data)->itemstat == STAT_ALREADYHAVE))
    {
        return;
    }
    if ((tutorial == TUTORIAL_ONLY) && !catRmTutEnabled2(0x800))
    {
        return;
    }
    lab = labbuttons[row->catLab].lab;
    if ((lab->labstatus == LS_NORESEARCHITEM) || ((tutorial != 0) && (lab->labstatus == LS_NORESEARCHSHIP)))
    {
        lab->labstatus = LS_RESEARCHITEMSOON;
        lab->topic = NULL;
        clWrapResearch(0, universe.curPlayerIndex, row->catLab, row->itemID);
        labbuttons[row->catLab].pulsepos = 0;
        speechEventQueue(NULL, 0x4001031, -1, -1, 0, universe.curPlayerIndex, -1, 5.0f, -1);
    }
    rmCurrentLab = row->catLab;
    if (rmBaseRegion != NULL)
    {
        rmBaseRegion->status |= RSF_ReallyDirty;
    }
    rmTechListWindowHandle->reg.status |= RSF_DrawThisFrame;
}

/*-----------------------------------------------------------------------------
    Name        : rmExitMenu / rmCloseIfOpen / cat_rmConstruction / cat_rmLaunch
    Description : close the research manager
    CAT (M2-R6 managers): the exe's bodies (rmCloseIfOpen 0x004f4e30; RM_ExitMenu 0x004f4d40, RM_Construction 0x004f4f20,
        RM_Launch 0x004f5010 with no Ghidra function): the render counter cleared, the tutorial gate bit 0x2000 of its
        second word, the message "RM_Close", the screen deleted, the main view drawn again, the sounds 0x130 / 0x100,
        the shown technology forgotten, the GUI inactive, the regions forgotten, the sensors manager reopened when this
        screen closed it; the two buttons then open the construction / launch manager
----------------------------------------------------------------------------*/
static bool32 catRmClose(void)
{
    sdword i;

    rmRenderEverythingCounter = 0;
    if ((tutorial == TUTORIAL_ONLY) && !catRmTutEnabled2(0x2000))
    {
        return FALSE;
    }
    tutGameMessage("RM_Close");
    feCurrentScreenDelete();
    rmBaseRegion = NULL;
    mrRenderMainScreen = TRUE;
    soundEventPlay(NULL, 0x130, NULL);
    soundEventPlay(NULL, UI_SoundOfSpace, NULL);
    spUnlockout();
    rmCurTechTexture = -1;
    techinfo = -1;
    rmGUIActive = FALSE;
    rmTechImageRegion = NULL;
    rmTechBriefRegion = NULL;
    for (i = 0; i < NUM_RESEARCHLABS; i++)
    {
        rmLabRegion[i] = NULL;
    }
    catRmModuleNameRegion = NULL;
    catRmBlobRegion = NULL;
    if (catRmSensorsWereOpen)
    {
        catSmReopenAfterManager();
    }
    // CAT-TODO(stage 6 tutorial): cat_tutButtonsRestoreAfterScreen (the tutorial's buttons come back)
    return TRUE;
}

void rmExitMenu(char *string, featom *atom)
{
    catRmClose();
}

void rmCloseIfOpen(void)
{
    if (rmBaseRegion != NULL)
    {
        catRmClose();
    }
}

void cat_rmConstruction(char *string, featom *atom)
{
    catRmClose();
    mrBuildShips(NULL, NULL);       // the exe opens it even when the gate kept the screen
}

void cat_rmLaunch(char *string, featom *atom)
{
    catRmClose();
    mrLaunch(NULL, NULL);
}

/*-----------------------------------------------------------------------------
    Name        : rmDrawLabButton
    Description : a lab button: the module's icon (off, or on for the selected lab), and while it researches the
                  progress bar
    CAT (M2-R6 managers): the exe's body (0x004f5100): an empty slot (no module) is a black box with a red outline; an
        idle lab its icon over a red-outlined bar; a researching lab its icon and the shaded progress bar (rounded with
        the exe's chop), the selected one with the sweeping pulse line; "ResearchAlmost" for the tutorial
----------------------------------------------------------------------------*/
static void catRmLabIconDraw(rectangle *rect, sdword icon)
{
    if (rmPaletted)
    {
        trPalettedTextureMakeCurrent(catRmLabTexture[icon], catRmLabImage[icon]->palette);
    }
    else
    {
        trRGBTextureMakeCurrent(catRmLabTexture[icon]);
    }
    primRectSolidTextured2(rect);       // CAT: no glcompat path in the SDL build (decision 28)
}

void rmDrawLabButton(LabPrintList *labprint, regionhandle region)
{
    rectangle bar, image;
    sdword labid = labprint->labid;
    sdword icon, x;
    ResearchTopic *topic;
    real32 fraction;
    sdword chop[2];
    color c[4];

    image.x0 = region->rect.x0 + 5;
    image.y0 = region->rect.y0 + 5;
    image.x1 = region->rect.x1 - 5;
    image.y1 = region->rect.y1 - 5;
    bar.x0 = image.x0;
    bar.x1 = image.x1;
    bar.y1 = image.y1;
    bar.y0 = region->rect.y1 - 15;

    ferDrawFocusWindow(region, (rmCurrentLab == labid - 1) ? lw_focus : lw_normal);
    if (labprint->lab->labstatus == LS_NORESEARCHSHIP)
    {
        primRectSolid2(&image, colBlack);
        primRectOutline2(&bar, 1, rmNoResearchItemColor);
        return;
    }
    if (labprint->lab->labstatus != LS_RESEARCHITEM)
    {
        primRectSolid2(&image, colBlack);
    }
    primRectOutline2(&bar, 1, colRGB(255, 0, 0));
    image.x0 += 5;
    image.y1 = bar.y0 - 3;
    image.x1 -= 5;
    icon = (rmCurrentLab == labid - 1) ? labid + 9 : labid + 4;
    catRmLabIconDraw(&image, icon);
    if (labprint->lab->labstatus != LS_RESEARCHITEM)
    {
        return;
    }

    topic = labprint->lab->topic;
    fraction = 1.0f - topic->timeleft /
               (real32)universe.curPlayerPtr->researchinfo.techstat->TimeToComplete[topic->techresearch];
    if (fraction > 0.0f)
    {
        tutGameMessage("ResearchAlmost");
    }
    bar.x0 += 1;
    bar.x1 -= 1;
    if (rmCurrentLab == labid - 1)
    {
        x = ((bar.x1 - bar.x0) * labprint->pulsepos >> 16) + bar.x0;
        primLine2(x, bar.y0 + 2, x, bar.y1 - 3, colBlack);
        if (labprint->pulsepos <= 0x10000)
        {
            labprint->pulsepos += 0x800;
        }
        else
        {
            labprint->pulsepos = 0;
        }
        x = ((bar.x1 - bar.x0) * labprint->pulsepos >> 16) + bar.x0;
        primLine2(x, bar.y0 + 2, x, bar.y1 - 3, colRGB(200, 200, 200));
    }
    bar.y0 += 1;
    bar.y1 -= 2;
    chop_temp = (double)((real32)(bar.x1 - bar.x0) * fraction) + 6755399441055744.0;     // 0x0072a458
    memcpy(chop, &chop_temp, sizeof(chop));
    bar.x1 = chop[0] + bar.x0;
    c[0] = rmProgressDoneColor0;
    c[1] = rmProgressDoneColor0;
    c[2] = rmProgressDoneColor1;
    c[3] = rmProgressDoneColor1;
    primRectShaded2(&bar, c);
}

/*-----------------------------------------------------------------------------
    Name        : rmLab1Draw .. rmLab5Draw and their region handlers
    Description : CAT (M2-R6 managers): the exe's bodies (rmLab1Draw 0x004f5af0 with the marquee clock, rmLab2Draw ..
                  rmLab5Draw 0x004f5b90 .. 0x004f5c50; the handlers 0x004f54c0 .. 0x004f59b0): a left press on a lab that
                  exists selects it, shows its module (technology 0x29 + lab) and picks the module's row in the list
                  ("RM_LabSelect <n>" in a tutorial, its gate bit 0x400)
----------------------------------------------------------------------------*/
static void catRmLabSelect(sdword lab, regionhandle region)
{
    TechPrintList *row;
    char message[16];

    region->status |= RSF_DrawThisFrame;
    if (rmTechImageRegion != NULL)
    {
        rmTechImageRegion->status |= RSF_DrawThisFrame;
    }
    if (labbuttons[lab].lab->labstatus == LS_NORESEARCHSHIP)
    {
        return;
    }
    if (tutorial != 0)
    {
        if ((tutorial == TUTORIAL_ONLY) && !catRmTutEnabled2(0x400))
        {
            return;
        }
        sprintf(message, "RM_LabSelect %c", '0' + (char)lab);
        tutGameMessage(message);
    }
    rmCurrentLab = lab;
    techinfo = (TechnologyType)(NumTechnologies + lab);
    if (rmBaseRegion != NULL)
    {
        rmBaseRegion->status |= RSF_ReallyDirty;
    }
    for (row = PlayerTechList; row->itemtype != ITEM_ENDLIST; row++)
    {
        if ((row->itemstat != STAT_CANTPRINT) && (row->itemstat != STAT_CANTRESEARCH) &&
            ((row->itemtype == CAT_ITEM_TECH2) || (row->itemtype == CAT_ITEM_TECH3)) && (row->itemID == techinfo))
        {
            uicListWindowSetCurItem(rmTechListWindowHandle, uicListFindItemByData(rmTechListWindowHandle, (ubyte *)row));
        }
    }
}

#define CAT_RM_LAB_HANDLER(n)                                                                   \
static sdword catRmSelectLab##n(regionhandle region, sdword ID, udword event, udword data)     \
{                                                                                               \
    if (event == RPE_PressLeft)                                                                 \
    {                                                                                           \
        catRmLabSelect(n - 1, region);                                                          \
    }                                                                                           \
    return 0;                                                                                   \
}
CAT_RM_LAB_HANDLER(1)
CAT_RM_LAB_HANDLER(2)
CAT_RM_LAB_HANDLER(3)
CAT_RM_LAB_HANDLER(4)
CAT_RM_LAB_HANDLER(5)

static void catRmLabDraw(regionhandle region, sdword lab, regionfunction handler)
{
    if (region->flags == 0 || region->flags == RPE_DrawFunctionAdded)
    {
        region->flags = RPE_DrawEveryFrame | RPE_PressLeft;    // 0x80002000
        regFunctionSet(region, handler);
    }
    rmLabRegion[lab] = region;
    rmDrawLabButton(&labbuttons[lab], region);
}

void rmLab1Draw(featom *atom, regionhandle region)
{
    if (rmRenderEverythingCounter > 0)
    {
        rmRenderEverythingCounter--;
        feRenderEverything = TRUE;
    }
    marqueetime += (taskTimeElapsed - catRmLastTime) * 3.0f;      // 0x0072a534 (HW1 RM_MARQUEERATE)
    if (marqueetime > 2.0f)                                     // the double 2.0 at 0x0072a5c0
    {
        marqueetime -= 2.0f;
    }
    catRmLastTime = taskTimeElapsed;
    catRmLabDraw(region, 0, (regionfunction)catRmSelectLab1);
}

void rmLab2Draw(featom *atom, regionhandle region)
{
    catRmLabDraw(region, 1, (regionfunction)catRmSelectLab2);
}

void rmLab3Draw(featom *atom, regionhandle region)
{
    catRmLabDraw(region, 2, (regionfunction)catRmSelectLab3);
}

void rmLab4Draw(featom *atom, regionhandle region)
{
    catRmLabDraw(region, 3, (regionfunction)catRmSelectLab4);
}

void rmLab5Draw(featom *atom, regionhandle region)
{
    catRmLabDraw(region, 4, (regionfunction)catRmSelectLab5);
}

/*-----------------------------------------------------------------------------
    Name        : cat_rmModuleNameDraw
    Description : CAT: new (0x004f4b60, RM_ModuleNameDraw, no Ghidra function): the selected lab's module name (the
                  string strHANGAR_HEADING + lab) in the heading font, black shadow 2 pixels down-right, then white
----------------------------------------------------------------------------*/
void cat_rmModuleNameDraw(featom *atom, regionhandle region)
{
    fonthandle oldfont;

    catRmModuleNameRegion = region;
    if (rmCurrentLab == -1)
    {
        return;
    }
    oldfont = fontMakeCurrent(catRmHeadingFont);
    fontPrint(region->rect.x0 + 2, region->rect.y0 + 2, colBlack, strGetString(strHANGAR_HEADING + rmCurrentLab));
    fontPrint(region->rect.x0, region->rect.y0, colWhite, strGetString(strHANGAR_HEADING + rmCurrentLab));
    fontMakeCurrent(oldfont);
}

/*-----------------------------------------------------------------------------
    Name        : cat_rmPussyBlobDraw
    Description : CAT: new (0x004f5c90, RM_PussyBlob): the Beast's research animation: a frame every 0.1 s of the task
                  clock (the sequence 1-5-1-0, the sound 0x13c when it wraps to the start while anything is researched)
                  while the first lab researches; frame 0 when it is idle or starting. The region draws every frame
----------------------------------------------------------------------------*/
void cat_rmPussyBlobDraw(featom *atom, regionhandle region)
{
    rectangle rect = region->rect;
    sdword frame = 0;

    catRmBlobRegion = region;
    region->flags |= RPE_DrawEveryFrame;
    catRmBlobTime += taskTimeElapsed - catRmLastTime;
    if (catRmBlobTime > 0.1f)                                   // 0x0072a4d4
    {
        if ((catRmBlobFrame == 0) && rmResearchingAnything(universe.curPlayerPtr))
        {
            soundEventPlay(NULL, 0x13c, NULL);
        }
        catRmBlobFrame++;
        catRmBlobTime = 0.0f;
        if (catRmBlobFrame > 9)
        {
            catRmBlobFrame = 0;
        }
    }
    catRmLastTime = taskTimeElapsed;
    switch (universe.curPlayerPtr->researchinfo.researchlabs[0].labstatus)
    {
        case LS_NORESEARCHITEM:
        case LS_RESEARCHITEMSOON:
            catRmBlobFrame = 0;
            frame = 0;
            break;
        case LS_RESEARCHITEM:
            frame = catRmBlobFrames[catRmBlobFrame];
            break;
        default:
            break;      // the exe draws frame 0 (its frame variable still holds the region pointer: CAT-FIX)
    }
    if (rmPaletted)
    {
        trPalettedTextureMakeCurrent(catRmBeastTexture[frame], catRmBeastImage[frame]->palette);
    }
    else
    {
        trRGBTextureMakeCurrent(catRmBeastTexture[frame]);
    }
    primRectSolidTextured2(&rect);
}

char *getWord(char *dest, char *source)
{
    sdword index=strlen(dest);

    while ( (source[0] == ' ') || (source[0] == '-') )
    {
        dest[index] = source[0];
        dest[index+1] = 0;
        index++;
        source++;
    }

    while ( (source[0] != ' ') && (source[0] != '-') && (source[0] != 0) && (source[0] != '\n') )
    {
        dest[index] = source[0];
        dest[index+1] = 0;
        index++;
        source++;
    }

    return(source);
}

/*-----------------------------------------------------------------------------
    Name        : getShipBuild
    Description : the ships the shown technology would make buildable, appended to dest
    CAT (M2-R6 managers): the exe's body (0x004f5e70): the race's ship types, a type whose requirement set the held
        technologies plus this one cover but the held ones alone do not (cat_rmTechSetAnd / cat_rmTechSetEqual); only for
        a player of race 2 (the exe's test; no Cataclysm player has it) and never the type 0x4e
----------------------------------------------------------------------------*/
char *getShipBuild(char *dest)
{
    PlayerResearchInfo *researchinfo = &universe.curPlayerPtr->researchinfo;
    CatTechSet hasPlusTech, test;
    sdword *need;
    ShipType type;
    bool32 first = TRUE;

    memcpy(hasPlusTech, researchinfo->HasTechnology, sizeof(CatTechSet));
    cat_rmTechSetAdd(hasPlusTech, techinfo);
    for (type = FirstShipTypeOfRace[universe.curPlayerPtr->race]; type < LastShipTypeOfRace[universe.curPlayerPtr->race]; type++)
    {
        need = researchinfo->techstat->TechNeededToBuildShip[type];
        cat_rmTechSetAnd(test, need, hasPlusTech);
        if (!cat_rmTechSetEqual(test, need))
        {
            continue;
        }
        cat_rmTechSetAnd(test, researchinfo->HasTechnology, need);
        if (cat_rmTechSetEqual(test, need) || (universe.curPlayerPtr->race != 2) || (type == 0x4e))
        {
            continue;
        }
        if (first)
        {
            strcat(dest, "\n\n");
            strcat(dest, strGetString(strCanBuild));
            strcat(dest, "\n");
        }
        else
        {
            strcat(dest, ", ");
        }
        strcat(dest, " ");
        strcat(dest, ShipTypeToNiceStr(type));
        first = FALSE;
    }
    return (dest);
}

/*-----------------------------------------------------------------------------
    Name        : rmExtendedInfo
    Description : CAT (M2-R6 managers): the exe's body (0x004f6090, RM_ExtendedInfo): the description over the picture on
                  or off, the picture redrawn
----------------------------------------------------------------------------*/
void rmExtendedInfo(char *string, featom *atom)
{
    rmExtendedInfoActive = !rmExtendedInfoActive;
    if (rmTechImageRegion != NULL)
    {
        rmTechImageRegion->status |= RSF_DrawThisFrame;
    }
}

/*-----------------------------------------------------------------------------
    Name        : rmTechInfoDraw
    Description : Draws technology text to the window.
    CAT (M2-R6 managers): the exe's body (0x004f60b0): no title line (rmTechImageDraw prints it), the text starts one
        and a half lines and 16 pixels down; a technology's description by race (the R1 / R2 info strings) with the ships
        it enables (getShipBuild), a module's (0x29 + lab) from the R2 table's continuation (the module infos)
----------------------------------------------------------------------------*/
void rmTechInfoDraw(regionhandle region)
{
    fonthandle  currentFont;
    sdword      x,y;
    char       *pos, *oldpos;
    char        oldline[100], line[100];
    char        stringtoprint[650];
    bool32      justified, done;

    if (techinfo == -1)
    {
        return;
    }
    currentFont = fontMakeCurrent(rmTechInfoFont);

    x = region->rect.x0 + 15;
    y = (fontHeight(" ") >> 1) + fontHeight(" ") + 16 + region->rect.y0;

    if (techinfo < NumTechnologies)
    {
        strcpy(stringtoprint, strGetString(techinfo + ((universe.curPlayerPtr->race == RACE_Sect) ? strTechInfoOffsetR1 : strTechInfoOffsetR2)));
        getShipBuild(stringtoprint);
    }
    else
    {
        strcpy(stringtoprint, strGetString(techinfo + strTechInfoOffsetR2));
    }

    pos = stringtoprint;
    done = FALSE;
    while (!done)
    {
        justified = FALSE;
        line[0]=0;
        while (!justified)
        {
            strcpy(oldline, line);
            oldpos = pos;
            pos = getWord(line, pos);

            if (pos[0] == '\n')
            {
                if (fontWidth(line) > region->rect.x1 - region->rect.x0 - 15)
                {
                    strcpy(line, oldline);
                    pos = oldpos;
                    while (pos[0] == ' ') pos++;
                    justified = TRUE;
                }
                else
                {
                    justified = TRUE;
                    pos++;
                    while ( pos[0] == ' ' ) pos++;
                }
            }
            else
            {
                if (fontWidth(line) > region->rect.x1 - region->rect.x0 - 15)
                {
                    strcpy(line, oldline);
                    pos = oldpos;
                    while ( pos[0] == ' ' ) pos++;

                    justified = TRUE;
                }
                if (pos[0]==0)
                {
                    justified = TRUE;
                    done      = TRUE;
                }
            }
        }

        fontPrintf(x,y,rmStandardTextColor,"%s",line);
        y += fontHeight(" ");
        if (y > region->rect.y1 + fontHeight(" ")) done=TRUE;
    }

    fontMakeCurrent(currentFont);
}

/*-----------------------------------------------------------------------------
    Name        : rmTechImageDraw
    Description : the shown technology's picture
    CAT (M2-R6 managers): the exe's body (0x004f6380): the picture of a technology or of a lab's module (0x29 + lab:
        cat_ModuleTypeToString) from the race's directory, through the LRU cache; the description over it when the
        extended info is on; a technology's name as its title (0x00891f24 colour, the info font, 15 / 11 pixels in)
----------------------------------------------------------------------------*/
void rmTechImageDraw(featom *atom, regionhandle region)
{
    char filename[128];
    rectangle textureRect;
    fonthandle oldfont;

    rmTechImageRegion = region;
    feStaticRectangleDraw(region);

    if (techinfo == rmCurTechTexture)
    {
        trPalettedTextureMakeCurrent(pictures[rmCurIndex].techTexture, pictures[rmCurIndex].techImage->palette);
    }
    else
    {
        filename[0] = 0;
        if (universe.curPlayerPtr->race <= RACE_Beast)
        {
            strcpy(filename, TechImagePaths[universe.curPlayerPtr->race]);
        }
        strcat(filename, (techinfo >= NumTechnologies) ? cat_ModuleTypeToString(techinfo - NumTechnologies) : TechTypeToString(techinfo));
        strcat(filename, ".lif");
        catRmPictureLoad(filename);
    }

    textureRect.x0 = region->rect.x0 + RM_TEXTURE_INSET;
    textureRect.y0 = region->rect.y0 + RM_TEXTURE_INSET;
    textureRect.x1 = region->rect.x1 - RM_TEXTURE_INSET;
    textureRect.y1 = region->rect.y1 - RM_TEXTURE_INSET;
    primRectSolidTextured2(&textureRect);       // CAT: no glcompat path in the SDL build (decision 28)

    if (techinfo == -1)
    {
        return;
    }
    if (rmExtendedInfoActive)
    {
        primRectTranslucent2(&textureRect, colRGBA(0, 0, 0, 128));
        rmTechInfoDraw(region);
    }
    if ((techinfo >= 0) && (techinfo < NumTechnologies))
    {
        oldfont = fontMakeCurrent(rmTechInfoFont);
        fontPrint(region->rect.x0 + 15, region->rect.y0 + 11, rmClassHeadingTextColor,
                  RaceSpecificTechTypeToNiceString(techinfo, universe.curPlayerPtr->race));
        fontMakeCurrent(oldfont);
    }
}

/*-----------------------------------------------------------------------------
    Name        : rmDrawTechListItem
    Description : one row of the technology list
    CAT (M2-R6 managers): the exe's item draw (0x004f6810, no Ghidra function): a header (printable) in the heading
        colour by its name string (0xbb39a0 + id: strHANGAR_HEADING - 2 + id); a technology row indented four spaces by
        its race name, its colour by kind and state (researchable: kind 2 the list's standard colour, kind 3 green;
        unresearchable grey; researching the researching colour over a shaded progress bar of the topic; held: kind 2
        the list's 0xba1e88 colour, kind 3 dark green, with a green dot); a selected row is drawn in the standard colours
----------------------------------------------------------------------------*/
void rmDrawTechListItem(rectangle *rect, listitemhandle data)
{
    TechPrintList *row = (TechPrintList *)data->data;
    Player *player = universe.curPlayerPtr;
    sdword x = rect->x0, y = rect->y0;
    char text[100];
    color c = 0;
    fonthandle oldfont;
    ResearchTopic *topic;
    rectangle bar;
    real32 fraction;
    sdword width;
    color shades[4];

    oldfont = fontMakeCurrent(rmTechListFont);
    fontShadowSet(FS_NONE, colBlack);
    text[0] = 0;
    if (row->itemtype == ITEM_CLASSHEADER)
    {
        if (row->itemstat != STAT_CANPRINT)
        {
            fontMakeCurrent(oldfont);
            return;
        }
        sprintf(text, "%s", strGetString(strHANGAR_HEADING - 4 + row->itemID));
        c = rmClassHeadingTextColor;
    }
    else if ((row->itemtype == CAT_ITEM_TECH2) || (row->itemtype == CAT_ITEM_TECH3))
    {
        sprintf(text, "%s", RaceSpecificTechTypeToNiceString(row->itemID, player->race));
        switch (row->itemstat)
        {
            case STAT_CANRESEARCH:
                x += 4 * fontWidth(" ");
                c = (row->itemtype == CAT_ITEM_TECH2) ? FEC_ListItemStandard : catRmTech3TextColor;
                break;
            case STAT_CANTRESEARCH:
                x += 4 * fontWidth(" ");
                c = rmCantResearchTextColor;
                break;
            case STAT_RESEARCHING:
                x += 4 * fontWidth(" ");
                topic = Researching(player, row->itemID);
                if (topic != NULL)
                {
                    fraction = topic->timeleft / (real32)player->researchinfo.techstat->TimeToComplete[row->itemID];
                    bar.x0 = x - 2;
                    bar.y0 = y;
                    bar.x1 = rect->x1 - 2;
                    bar.y1 = fontHeight(" ") + y + 2;
                    primRectSolid2(&bar, 0xff754f09);
                    width = (sdword)((real32)(rect->x1 - x) * fraction);
                    bar.x1 = rect->x1 - 2 - width;
                    shades[0] = 0xffef5325;
                    shades[1] = 0xfff9a891;
                    shades[2] = 0xffef5325;
                    shades[3] = 0xfffbb09b;
                    primRectShaded2(&bar, shades);
                    rmTechListWindowHandle->reg.status |= RSF_DrawThisFrame;
                }
                c = rmResearchingTextColor;
                fontShadowSet(FS_SE, colBlack);
                break;
            case STAT_ALREADYHAVE:
                x += 4 * fontWidth(" ");
                c = (row->itemtype == CAT_ITEM_TECH2) ? FEC_ListItemInactive : catRmTech3HaveTextColor;
                primCircleSolid2(x - 7, y + 5, 3, 20, colRGB(0, 139, 0));
                break;
            default:
                break;      // the exe draws stat 3 in a colour it reads from its argument (never listed)
        }
        if (bitTest(data->flags, UICLI_Selected))
        {
            if (row->itemstat == STAT_RESEARCHING)
            {
                c = rmResearchingTextColor;
                fontShadowSet(FS_SE, colBlack);
            }
            else
            {
                c = (row->itemtype == CAT_ITEM_TECH2) ? FEC_ListItemStandard : catRmTech3TextColor;
            }
        }
    }
    else
    {
        fontMakeCurrent(oldfont);
        return;
    }
    fontPrint(x, y, c, text);
    fontMakeCurrent(oldfont);
}

/*-----------------------------------------------------------------------------
    Name        : rmTechListWindow
    Description : the technology list's callback
    CAT (M2-R6 managers): the exe's body (0x004f6ae0): created with the list font's line and a half; every printable
        row is added (a technology row selectable); a selection shows its technology and lab ("RM_TechSelect_<name>" in
        a tutorial, its gate bit 0x200 of the second word; a module row by cat_ModuleTypeToString); a double click
        researches it ("RM_DoubleClick")
----------------------------------------------------------------------------*/
void rmTechListWindow(char *string, featom *atom)
{
    TechPrintList *row;
    char message[64];
    sdword index;

    if ((atom != NULL) && FEFIRSTCALL(atom))
    {
        fontMakeCurrent(rmTechListFont);
        rmTechListWindowHandle = (listwindowhandle)atom->pData;
        uicListWindowInit(rmTechListWindowHandle, NULL, NULL, 0, rmDrawTechListItem,
                          (fontHeight(" ") >> 1) + fontHeight(" "), UICLW_CanSelect);
        for (index = 0; PlayerTechList[index].itemtype != ITEM_ENDLIST; index++)
        {
            if ((PlayerTechList[index].itemstat == STAT_CANTPRINT) || (PlayerTechList[index].itemstat == STAT_CANTRESEARCH))
            {
                continue;
            }
            if (PlayerTechList[index].itemtype == ITEM_CLASSHEADER)
            {
                uicListAddItem(rmTechListWindowHandle, (ubyte *)&PlayerTechList[index], 0, UICLW_AddToTail);
            }
            else if ((PlayerTechList[index].itemtype == CAT_ITEM_TECH2) || (PlayerTechList[index].itemtype == CAT_ITEM_TECH3))
            {
                uicListAddItem(rmTechListWindowHandle, (ubyte *)&PlayerTechList[index], UICLI_CanSelect, UICLW_AddToTail);
            }
        }
        return;
    }
    if ((atom != NULL) && FELASTCALL(atom))
    {
        rmTechListWindowHandle = NULL;
        return;
    }
    if (rmTechListWindowHandle->message == CM_NewItemSelected)
    {
        if ((tutorial == TUTORIAL_ONLY) && !catRmTutEnabled2(0x200))
        {
            return;
        }
        row = (TechPrintList *)rmTechListWindowHandle->CurLineSelected->data;
        techinfo = row->itemID;
        if (rmTechImageRegion != NULL)
        {
            rmTechImageRegion->status |= RSF_DrawThisFrame;
        }
        rmCurrentLab = row->catLab;
        if (rmBaseRegion != NULL)
        {
            rmBaseRegion->status |= RSF_ReallyDirty;
        }
        if (tutorial == TUTORIAL_ONLY)
        {
            sprintf(message, "RM_TechSelect_%s", (techinfo < NumTechnologies) ? TechTypeToString(techinfo) :
                                                                                cat_ModuleTypeToString(techinfo - NumTechnologies));
            tutGameMessage(message);
        }
    }
    else if (rmTechListWindowHandle->message == CM_DoubleClick)
    {
        if (tutorial == TUTORIAL_ONLY)
        {
            tutGameMessage("RM_DoubleClick");
        }
        rmResearchItem(NULL, NULL);
    }
}

/*-----------------------------------------------------------------------------
    Name        : rmUpdateTechList
    Description : the list's states from the player's research
    CAT (M2-R6 managers): the exe's body (0x004f6d30): a technology row (kind 2 / 3) is held (STAT_ALREADYHAVE),
        researching, or, in a tutorial, researchable when the tutorial allows it (tutBuildRestrict's research twin
        0x00ba4680); in multiplayer researchable when its lab exists and the held technologies cover its requirements
        (cat_rmTechSetAnd / cat_rmTechSetEqual), the crystal harvest (technology 34) only with the game option 0x20;
        in single player researchable when the mission allows it (the same tutorial table) and its lab exists; a
        kind-1 row is never listed. A header is printable when any of its rows is shown. The list is rebuilt when the
        count of shown rows changed (the shown technology stays selected), and the screen redrawn
----------------------------------------------------------------------------*/
void rmUpdateTechList(void)
{
    Player *player = universe.curPlayerPtr;
    PlayerResearchInfo *research = &player->researchinfo;
    CatTechSet have, test;
    sdword index, count = 0;
    sword headerstat = STAT_CANTPRINT;
    TechnologyType tech;
    sdword *need;

    memcpy(have, research->HasTechnology, sizeof(CatTechSet));
    for (index = 0; PlayerTechList[index].itemtype != ITEM_ENDLIST; index++)
    {
        if ((PlayerTechList[index].itemtype == CAT_ITEM_TECH2) || (PlayerTechList[index].itemtype == CAT_ITEM_TECH3))
        {
            tech = PlayerTechList[index].itemID;
            if (cat_rmTechSetHas(have, tech))
            {
                PlayerTechList[index].itemstat = STAT_ALREADYHAVE;
            }
            else if (Researching(player, tech) != NULL)
            {
                PlayerTechList[index].itemstat = STAT_RESEARCHING;
            }
            else if (tutorial == TUTORIAL_ONLY)
            {
                PlayerTechList[index].itemstat = (catRmResearchAllowed(tech)) ? STAT_CANRESEARCH : STAT_CANTRESEARCH;
            }
            else if (!singlePlayerGame)
            {
                need = research->techstat->TechNeededToResearch[tech];
                cat_rmTechSetAnd(test, have, need);
                if ((research->researchlabs[PlayerTechList[index].catLab].labstatus == LS_NORESEARCHSHIP) ||
                    !cat_rmTechSetEqual(test, need))
                {
                    PlayerTechList[index].itemstat = STAT_CANTRESEARCH;
                }
                else if ((tech == TECH_CRYSTAL_HARVEST) && !catRmGameOptionCrystals())   // [audit: was TECH_INFECT (32); the exe: 0x22]
                {
                    PlayerTechList[index].itemstat = STAT_CANTRESEARCH;
                }
                else
                {
                    PlayerTechList[index].itemstat = STAT_CANRESEARCH;
                }
            }
            else
            {
                PlayerTechList[index].itemstat =
                    (!catRmResearchAllowed(tech) || (research->researchlabs[PlayerTechList[index].catLab].labstatus == LS_NORESEARCHSHIP)) ?
                    STAT_CANTRESEARCH : STAT_CANRESEARCH;
            }
        }
        else if (PlayerTechList[index].itemtype == ITEM_TECHNOLOGY)
        {
            PlayerTechList[index].itemstat = STAT_CANTRESEARCH;
        }
    }
    // from the end row down (the exe counts the end row too, hence the - 1 below)
    for (; index >= 0; index--)
    {
        if (((PlayerTechList[index].itemtype == CAT_ITEM_TECH2) || (PlayerTechList[index].itemtype == CAT_ITEM_TECH3)) &&
            (PlayerTechList[index].itemstat != STAT_CANTRESEARCH))
        {
            headerstat = STAT_CANPRINT;
        }
        else if (PlayerTechList[index].itemtype == ITEM_CLASSHEADER)
        {
            PlayerTechList[index].itemstat = headerstat;
            headerstat = STAT_CANTPRINT;
        }
        if ((PlayerTechList[index].itemstat != STAT_CANTPRINT) && (PlayerTechList[index].itemstat != STAT_CANTRESEARCH))
        {
            count++;
        }
    }

    if (rmTechListWindowHandle != NULL)
    {
        if (count - 1 != rmTechListWindowHandle->ListTotal)
        {
            uicListCleanUp(rmTechListWindowHandle);
            for (index = 0; PlayerTechList[index].itemtype != ITEM_ENDLIST; index++)
            {
                if ((PlayerTechList[index].itemstat == STAT_CANTPRINT) || (PlayerTechList[index].itemstat == STAT_CANTRESEARCH))
                {
                    continue;
                }
                if (PlayerTechList[index].itemtype == ITEM_CLASSHEADER)
                {
                    uicListAddItem(rmTechListWindowHandle, (ubyte *)&PlayerTechList[index], 0, UICLW_AddToTail);
                }
                else if ((PlayerTechList[index].itemtype == CAT_ITEM_TECH2) || (PlayerTechList[index].itemtype == CAT_ITEM_TECH3))
                {
                    uicListAddItem(rmTechListWindowHandle, (ubyte *)&PlayerTechList[index], UICLI_CanSelect, UICLW_AddToTail);
                }
            }
            for (index = 0; PlayerTechList[index].itemtype != ITEM_ENDLIST; index++)
            {
                if ((PlayerTechList[index].itemstat != STAT_CANTPRINT) && (PlayerTechList[index].itemstat != STAT_CANTRESEARCH) &&
                    ((PlayerTechList[index].itemtype == CAT_ITEM_TECH2) || (PlayerTechList[index].itemtype == CAT_ITEM_TECH3)) &&
                    (PlayerTechList[index].itemID == techinfo))
                {
                    uicListWindowSetCurItem(rmTechListWindowHandle,
                                            uicListFindItemByData(rmTechListWindowHandle, (ubyte *)&PlayerTechList[index]));
                }
            }
            catRmDirtyRegions();
        }
        rmTechListWindowHandle->reg.status |= RSF_DrawThisFrame;
    }
    catRmDirtyRegions();
}

/*-----------------------------------------------------------------------------
    Name        : rmResearchGUIBegin
    Description : open the research manager
    CAT (M2-R6 managers): the exe's body (0x004f7140): not during a playback or the console / game-stats pause, only with
        a mothership-class mothership and, in single player, for player 0; an open sensors manager is closed for good and
        reopened on close; the tutorial gate bit 0x10; the race's screen file and screen (the Beast's
        Beast_Research_Manager); the race's list; the five lab buttons on the player's labs; the sounds 0x12f / 0x106;
        the default picture (Default_Research_Image.lif) for the shown technology unless it is cached
----------------------------------------------------------------------------*/
sdword rmResearchGUIBegin(regionhandle region, smemsize ID, udword event, udword data)
{
    sdword index;
    char filename[128];

    if (playPackets || universePause || (universe.curPlayerPtr == NULL) ||
        (universe.curPlayerPtr->PlayerMothership == NULL) ||
        (universe.curPlayerPtr->PlayerMothership->staticinfo->shipclass != CLASS_Mothership) ||
        (singlePlayerGame && (universe.curPlayerPtr->playerIndex != 0)))
    {
        return 0;
    }
    catRmSensorsWereOpen = smSensorsActive;
    if (smSensorsActive)
    {
        smSensorsCloseForGood();
    }
    if ((tutorial == TUTORIAL_ONLY) && !catRmTutEnabled1(0x10))
    {
        return 0;
    }

    mrRenderMainScreen = FALSE;
    rmRenderEverythingCounter = (tutorial == TUTORIAL_ONLY) ? 4 : 0;
    rndClear();
    spLockout(SPLOCKOUT_MR);

    if (rmScreensHandle == NULL)
    {
        feCallbackAddMultiple(rmCallback);                  //add in the callbacks
        feDrawCallbackAddMultiple(rmDrawCallback);
    }
    else if (universe.curPlayerPtr->race != catRmScreensRace)
    {
        feScreensDelete(rmScreensHandle);
        rmScreensHandle = NULL;
    }
    if (rmScreensHandle == NULL)
    {
        catRmScreensRace = (universe.curPlayerPtr->race == RACE_Sect) ? RACE_Sect : RACE_Beast;
        rmScreensHandle = feScreensLoad((catRmScreensRace == RACE_Sect) ? RM_FIBFile : CAT_RM_BeastFIBFile);
    }

    cat_rmSelectTechPrintList();

    for (index=0;index<NUM_RESEARCHLABS;index++)
    {
        labbuttons[index].lab = &universe.curPlayerPtr->researchinfo.researchlabs[index];
        labbuttons[index].labid = (sword)(index + 1);
        labbuttons[index].selected = FALSE;
        labbuttons[index].pulsepos = 0;
    }

    soundEventStopSFX(0.5f);
    soundEventPlay(NULL, 0x12f, NULL);
    soundEventPlay(NULL, 0x106, NULL);

    rmUpdateTechList();

    rmBaseRegion = feScreenStart(region, (universe.curPlayerPtr->race == RACE_Sect) ? RM_ResearchScreen : CAT_RM_BeastResearchScreen);
    strcpy(filename, TechImagePaths[(universe.curPlayerPtr->race == RACE_Sect) ? 0 : 1]);
    strcat(filename, CAT_RM_DefaultImage);
    catRmPictureLoad(filename);

    rmGUIActive = TRUE;
    tutGameMessage("Start_ResearchManager");
    mouseCursorShow();
    mouseCursorShow();
    // CAT-TODO(stage 6 tutorial): cat_tutButtonsHideForScreen 0x005341f0 (the tutorial's buttons hide while the screen is open)
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : rmSetPrintList
    Description : Sets the print lists for single and multiplayer after the dependancies are loaded;
    Inputs      : none
    Outputs     : none
    Return      : void
    CAT (M2-R6 managers): the exe's body (0x004f75f0): list 1 (single-player Sect) and 3 (multiplayer Sect) are copies of
        the exe's Sect list, list 4 (Beast) of its Beast list, whole (no filter by the ships that need a technology);
        techstat is not read
----------------------------------------------------------------------------*/
static void catRmCopyPrintList(TechPrintList *dest, TechPrintList *source)
{
    sdword index;

    for (index = 0; source[index].itemtype != ITEM_ENDLIST; index++)
    {
        dest[index] = source[index];
    }
    dest[index].itemtype = ITEM_ENDLIST;
}

void rmSetPrintList(udword whichlist, TechStatics *techstat)
{
    (void)techstat;
    switch (whichlist)
    {
        case RM_SPRS:
            catRmCopyPrintList(SinglePlayerR1TechList, catRmSectTechListStatic);
            break;
        case RM_MPRS:
            catRmCopyPrintList(MultiPlayerR1TechList, catRmSectTechListStatic);
            break;
        case RM_MPRB:
            catRmCopyPrintList(MultiPlayerR2TechList, catRmBeastTechListStatic);
            break;
        default:
            break;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_rmSelectTechPrintList
    Description : CAT: new (0x004f7a40): the current player's research list: single player the Sect list 1 (a Beast
                  player the unfilled one), multiplayer list 3 (Sect) or 4 (Beast)
----------------------------------------------------------------------------*/
void cat_rmSelectTechPrintList(void)
{
    if (singlePlayerGame)
    {
        PlayerTechList = (universe.curPlayerPtr->race != RACE_Sect) ? SinglePlayerR2TechList : SinglePlayerR1TechList;
        return;
    }
    PlayerTechList = (universe.curPlayerPtr->race == RACE_Sect) ? MultiPlayerR1TechList : MultiPlayerR2TechList;
}

/*-----------------------------------------------------------------------------
    Name        : cat_rmBeastPrintIndexToShipType
    Description : CAT: new (0x004f4790): the ship type of a Beast list header (print index 10..29; -1 for any other,
                  9 included)
----------------------------------------------------------------------------*/
ShipType cat_rmBeastPrintIndexToShipType(sdword index)
{
    static const ShipType types[20] =
    {
        bRecon, bWorker, bProcessor, bAcolyte, bACV, bMultiBeamFrigate, bHiveFrigate, bSentinel, bInterceptor,
        bCloakedFighter, bAttackBomber, bCruiseMissile, bMissileCorvette, bMultiGunCorvette, bHeavyCorvette,
        bDFGFrigate, bIonArrayFrigate, bHeavyCruiser, bCarrier, bRammingFrigate
    };

    if ((index < 10) || (index > 29))
    {
        return (ShipType)-1;
    }
    return types[index - 10];
}

/*-----------------------------------------------------------------------------
    Name        : cat_rmAcquireShipTechs
    Description : CAT: new (0x004f4870): a retired (Dock.c) or converted (cat_univConvertShip 0x00543950) Beast ship
                  brings the technologies of its upgrades to its player (only when the player's first lab word is
                  set): the ship's upgrade technologies (cat_shipGetUpgradeTechs), or every technology when the ship's
                  or the player's row of its type says all, are ORed into the player's upgrade row of the type, and
                  the type becomes buildable (catShipTypeEnabled). For the local human player the research list is
                  marked first: under every header whose ship is this type or an enabled one, the rows of the type's
                  build technologies become CAT_ITEM_TECH2 and the rows of the new upgrade technologies
                  CAT_ITEM_TECH3 (the technologies 7, 16, 28, 32, 33 and 34 never; 24 not for a bAcolyte / bACV); the
                  open research screen is refreshed
----------------------------------------------------------------------------*/
static bool32 catRmAcquireSkipsTech(sdword tech)
{
    return (tech == TECH_REPAIRBOTS) || (tech == TECH_SALVAGE) || (tech == TECH_CRYSTAL_HARVEST) ||
           (tech == TECH_SUPPORT_SYSTEMS) || (tech == TECH_INFECT) || (tech == TECH_ADVANCED_SENSORS);
}

static void catRmAcquireMarkRows(Ship *ship, sdword tech, sword kind)
{
    TechPrintList *row;
    ShipType headertype;
    bool32 underHeader = TRUE;

    for (row = PlayerTechList; row->itemtype != ITEM_ENDLIST; row++)
    {
        if (row->itemtype == ITEM_CLASSHEADER)
        {
            headertype = cat_rmBeastPrintIndexToShipType(row->itemID);
            // CAT-FIX: the exe reads catShipTypeEnabled[-1] for a header that maps to no type (print index 9)
            underHeader = (headertype == ship->shiptype) ||
                          ((headertype != (ShipType)-1) && ship->playerowner->researchinfo.catShipTypeEnabled[headertype]);
        }
        else if (underHeader && (row->itemtype == ITEM_TECHNOLOGY) && (row->itemID == tech))
        {
            row->itemtype = kind;
        }
    }
}

void cat_rmAcquireShipTechs(Ship *ship)
{
    Player *player = ship->playerowner;
    CatTechSet techs;
    sdword tech;
    sdword *buildtechs;

    if (player->researchinfo.researchlabs[0].labstatus == 0)
    {
        return;
    }
    cat_shipGetUpgradeTechs(ship, techs);
    if ((techs[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL) ||
        (player->researchinfo.catShipUpgradeTechs[ship->shiptype][CAT_TECHSET_FLAG] == CAT_TECHSET_ALL))
    {
        cat_rmTechSetClear(techs);
        techs[CAT_TECHSET_FLAG] = CAT_TECHSET_ALL;
    }
    else
    {
        for (tech = 0; tech < NumTechnologies; tech++)
        {
            techs[tech] |= player->researchinfo.catShipUpgradeTechs[ship->shiptype][tech];
        }
        techs[CAT_TECHSET_FLAG] = 0;
    }
    if ((player == universe.curPlayerPtr) && (universe.curPlayerPtr->aiPlayer == NULL))
    {
        cat_rmSelectTechPrintList();
        buildtechs = player->researchinfo.techstat->TechNeededToBuildShip[ship->shiptype];
        for (tech = 0; tech < NumTechnologies; tech++)
        {
            if (catRmAcquireSkipsTech(tech) || ((ship->shiptype == bAcolyte) && (tech == TECH_LINK)))
            {
                continue;
            }
            if ((buildtechs[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL) || (buildtechs[tech] == 1))
            {
                catRmAcquireMarkRows(ship, tech, CAT_ITEM_TECH2);
            }
        }
        for (tech = 0; tech < NumTechnologies; tech++)
        {
            if (catRmAcquireSkipsTech(tech) || ((ship->shiptype == bACV) && (tech == TECH_LINK)))
            {
                continue;
            }
            if ((techs[CAT_TECHSET_FLAG] == CAT_TECHSET_ALL) || (techs[tech] == 1))
            {
                catRmAcquireMarkRows(ship, tech, CAT_ITEM_TECH3);
            }
        }
        if (rmBaseRegion != NULL)
        {
            rmUpdateTechList();
        }
    }
    memcpy(player->researchinfo.catShipUpgradeTechs[ship->shiptype], techs, sizeof(CatTechSet));
    player->researchinfo.catShipTypeEnabled[ship->shiptype] = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : rmGUIStartup
    Description : Initializes the Research Manager GUI
    CAT (M2-R6 managers): rmGUIStartup 0x004f76f0: the LRU slots (no image, no texture, technology and race -2), the
        Somtaaw lab buttons and the Beast animation frames (rmPaletted follows the last image), the three fonts
----------------------------------------------------------------------------*/
void rmGUIStartup(void)
{
    sdword index;
    char filename[128];

    rmRenderEverythingCounter = 0;

    for (index=0;index<RM_TOTALPICS;index++)
    {
        pictures[index].techImage   = NULL;
        pictures[index].techTexture = TR_InvalidInternalHandle;
        pictures[index].tech        = -2;
        pictures[index].race        = -2;
        pictures[index].timestamp   = 0.0;
    }
    for (index = 0; index < CAT_RM_NUM_LABICONS; index++)
    {
        strcpy(filename, TechImagePaths[0]);
        strcat(filename, "Research_icon_");
        strcat(filename, catRmLabIcons[index]);
        catRmLabImage[index] = trLIFFileLoad(filename, NonVolatile);
        rmPaletted = bitTest(catRmLabImage[index]->flags, TRF_Paletted) ? TRUE : FALSE;
        if (rmPaletted)
        {
            catRmLabTexture[index] = trPalettedTextureCreate(catRmLabImage[index]->data, catRmLabImage[index]->palette,
                                                             catRmLabImage[index]->width, catRmLabImage[index]->height);
        }
        else
        {
            catRmLabTexture[index] = trRGBTextureCreate((color*)catRmLabImage[index]->data,
                                                        catRmLabImage[index]->width, catRmLabImage[index]->height, TRUE);
        }
    }
    for (index = 0; index < CAT_RM_NUM_BEASTFRAMES; index++)
    {
        strcpy(filename, TechImagePaths[1]);
        strcat(filename, "bresearch_ani_");
        strcat(filename, catRmBeastFrames[index]);
        catRmBeastImage[index] = trLIFFileLoad(filename, NonVolatile);
        rmPaletted = bitTest(catRmBeastImage[index]->flags, TRF_Paletted) ? TRUE : FALSE;
        if (rmPaletted)
        {
            catRmBeastTexture[index] = trPalettedTextureCreate(catRmBeastImage[index]->data, catRmBeastImage[index]->palette,
                                                               catRmBeastImage[index]->width, catRmBeastImage[index]->height);
        }
        else
        {
            catRmBeastTexture[index] = trRGBTextureCreate((color*)catRmBeastImage[index]->data,
                                                          catRmBeastImage[index]->width, catRmBeastImage[index]->height, TRUE);
        }
    }
    rmCurTexture   = TR_InvalidInternalHandle;
    rmCurTechImage = NULL;
    rmCurTechTexture = -1;

    rmTechListFont  = frFontRegister(rmTechListFontName);
    rmTechInfoFont  = frFontRegister(rmTechInfoFontName);
    catRmHeadingFont = frFontRegister("EuroseWideHeavy_i8.hff");  // CAT: rmGUIStartup 0x004f76f0, third font
}

/*-----------------------------------------------------------------------------
    Name        : rmGUIShutdown
    Description : shuts down the research managers GUI
    CAT: rmGUIShutdown 0x004f7970: the screens, the LRU slots, the lab buttons and the Beast frames
----------------------------------------------------------------------------*/
void rmGUIShutdown(void)
{
    sdword index;

    if (rmScreensHandle)
    {
        feScreensDelete(rmScreensHandle);
        rmScreensHandle = NULL;
    }

    for (index=0;index<RM_TOTALPICS;index++)
    {
        if (pictures[index].techImage != NULL)
        {
            memFree(pictures[index].techImage);
            pictures[index].techImage = NULL;
        }

        if (pictures[index].techTexture != TR_InvalidInternalHandle)
        {
            trRGBTextureDelete(pictures[index].techTexture);
            pictures[index].techTexture = TR_InvalidInternalHandle;
        }
    }
    for (index = 0; index < CAT_RM_NUM_LABICONS; index++)
    {
        if (catRmLabImage[index] != NULL)
        {
            memFree(catRmLabImage[index]);
            catRmLabImage[index] = NULL;
        }
        if (catRmLabTexture[index] != TR_InvalidInternalHandle)
        {
            trRGBTextureDelete(catRmLabTexture[index]);
            catRmLabTexture[index] = TR_InvalidInternalHandle;
        }
    }
    for (index = 0; index < CAT_RM_NUM_BEASTFRAMES; index++)
    {
        if (catRmBeastImage[index] != NULL)
        {
            memFree(catRmBeastImage[index]);
            catRmBeastImage[index] = NULL;
        }
        if (catRmBeastTexture[index] != TR_InvalidInternalHandle)
        {
            trRGBTextureDelete(catRmBeastTexture[index]);
            catRmBeastTexture[index] = TR_InvalidInternalHandle;
        }
    }
}
