/*=============================================================================
    Name    : options.c
    Purpose : Logic for the Options screen

    Created 11/05/1998 by yo
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "Options.h"

#include <math.h>
#include <stdio.h>

#include "AIPlayer.h"
#include "AutoLOD.h"
#include "Battle.h"
#include "Debug.h"
#include "FEColour.h"
#include "FEFlow.h"
#include "FontReg.h"
#include "glinc.h"
#include "Globals.h"
#include "InfoOverlay.h"
#include "LOD.h"
#include "mainrgn.h"
#include "PopupMenu.h"
#include "ShipSelect.h"
#include "SpeechEvent.h"
#include "Key.h"
#include "KeyBindings.h"
#include "Memory.h"
#include "mouse.h"
#include "MultiplayerGame.h"
#include "rinit.h"
#include "Sensors.h"
#include "Shader.h"
#include "SoundEvent.h"
#include "soundlow.h"
#include "StringsOnly.h"
#include "texreg.h"
#include "UIControls.h"
#include "utility.h"

#ifdef _MSC_VER
    #define strcasecmp _stricmp
#endif

#ifdef _WIN32
    #include "debugwnd.h"
#endif

/*=============================================================================
    Definitions:
=============================================================================*/

bool32   opTimerActive = FALSE;
real32 opTimerStart;
real32 opTimerLength = 22.0f;

bool32 opReloading = FALSE;

static char lastDev[64] = "";

sdword speechEventCleanup(void);


#define CONTROL_FEEDBACK 0.75f

#define OP_DefaultFont          "default.hff"
#define OP_KeyVertSpacing       17
#define OP_NumKeys              25
#define OP_KeysPerColumn        10
#define OP_KBMarginTop          10
#define OP_KBMarginLeft         10
#define OP_KBPrimaryKeyTab      130
#define OP_KBSecondaryKeyTab    172
#define OP_KBColumnTab          214
#define OP_KBBoxwidth           211
#define OP_BoxSpacingX          4
#define OP_BoxSpacingY          0


#define OP_KeyNameColor         colRGB(100,200,50)
#define OP_KeyPrimaryColor      colRGB(120,140,170)
#define OP_KeySecondaryColor    colRGB(80,100,150)
#define OP_KeySelectedColor     colRGB(200,200,50)
#define OP_KeyRedefiningColor   colRGB(250,80,80)
#define OP_KeyHighlightColor    colRGB(190,223,255)

#define OP_KeyHighlightTime     1
#define OP_KeyHighlightOnTime   0.8f
#define OP_HighlightStack       32

#define OP_SMOOTHIE_RATE        0.01f

#define NUM_EQ_BANDS SOUND_EQ_SIZE

#if(0)

bool32 smoothiesactive = FALSE;

real32 smoothmusicvol;
real32 smoothsfxvol;
real32 smoothspeechvol;
Smoothie volumesmoothie;
Smoothie speechsmoothie;
Smoothie sfxsmoothie;

real32 smootheqvalue[NUM_EQ_BANDS];
Smoothie smootheqsmoothie[NUM_EQ_BANDS];



region *volumeregion;
region *sfxregion;
region *speechregion;
region *nchannelsregion;


Smoothie *SmoothieArray[NUM_SMOOTHIES];


bool32 opSmoothiesBabyFunction(udword num, void *data, struct BabyCallBack *baby);
void AddSmoothie(Smoothie *smoo);
void RemoveSmoothie(Smoothie *smoo);
void DoSmoothies(void);

#endif


void opEQHelper(void);
void opEffectsHelper(void);
static void catOpTutorialGameplayDisable(void);
void opUpdateAudioSettings(void);

void opOptionsEndHelper(char* linkName);
void opOptionsEnd(void);

// The following functions are used to save and or restore the options in each screen

// the equalizer screen
void opOptionsSaveEqualizerSettings(void);
void opRestoreSavedEqualizerSettings(void);

// the advanced audio options screen
void opOptionsSaveAdvancedSettings(void);
void opRestoreAdvancedSettings(void);

// The three index screens altogether
void opOptionsSaveSettings(void);
void opRestoreSavedCustomEffectsSettings(void);


void opResListSet(rdevice* dev);

void opRenderListLoad(void);


/*=============================================================================
    Data:
=============================================================================*/

bool32 opCustomEffectsToggled;
bool32 opEqualizerToggled;

sword opKeySelected=-1;
sword opKeyBeingDefined=-1;
uword opKeyDetour = 0;
regionhandle keyboardregion = NULL;
regionhandle drawnumchannels = NULL;
//taskhandle opHighlightTaskHandle;
BabyCallBack    *opHighlightBaby=NULL;
BabyCallBack    *opSmoothiesBaby=NULL;

typedef struct
{
    keyindex key1;
    keyindex key2;
    sword highlight;
    char *funcname;
}
tempkeytrans;

tempkeytrans TempKeyTranslations[OP_NumKeys] =
{
   {TABKEY,      0, 0 ,"NEXT FORMATION" },
   {BKEY,        0, 0 ,"BUILD MANAGER" },
   {CKEY,        0, 0 ,"PREVIOUS FOCUS" },
   {VKEY,        0, 0 ,"NEXT FOCUS" },
   {DKEY,        0, 0 ,"DOCK" },
   {EKEY,        0, 0 ,"SELECT ALL VISIBLE" },
   {FKEY,        0, 0 ,"FOCUS" },
   {RKEY,        0, 0 ,"RESEARCH MANAGER" },
   {HKEY2,        0, 0 ,"HARVEST" },
   {MKEY,        0, 0 ,"MOVE" },
   {RBRACK,      0, 0 ,"NEXT TACTIC" },
   {LBRACK,      0, 0 ,"PREVIOUS TACTIC" },
   {SKEY,        0, 0 ,"SCUTTLE" },
   {ZKEY,        0, 0 ,"SHIP SPECIAL" },
   {CAPSLOCKKEY, 0, 0 ,"TACTICAL OVERLAY" },
   {SPACEKEY,    0, 0 ,"SENSORS MANAGER" },
   {HOMEKEY,     0, 0 ,"MOTHERSHIP" },
   {KKEY,        0, 0 ,"KAMIKAZE" },
   {TILDEKEY,    0, 0 ,"CANCEL ORDERS" },
   {0,0,0,""}
};

typedef struct
{
    keyindex sendkey;
    keyindex key1;
    keyindex key2;
}
keytrans;


keytrans KeyTranslations[OP_NumKeys] =
{
   {TABKEY,         TABKEY,      0 },   //NEXT FORMATION
   {BKEY,           BKEY,        0 },   //BUILD MANAGER
   {CKEY,           CKEY,        0 },   //PREVIOUS FOCUS
   {VKEY,           VKEY,        0 },   //NEXT FOCUS
   {DKEY,           DKEY,        0 },   //DOCK
   {EKEY,           EKEY,        0 },   //SELECT ALL VISIBLE
   {FKEY,           FKEY,        0 },   //FOCUS
   {RKEY,           RKEY,        0 },   //RESEARCH MANAGER
   {HKEY2,           HKEY2,        0 },   //HARVEST
   {MKEY,           MKEY,        0 },   //MOVE
   {RBRACK,         RBRACK,      0 },   //NEXT TACTIC
   {LBRACK,         LBRACK,      0 },   //PREVIOUS TACTIC
   {SKEY,           SKEY,        0 },   //SCUTTLE
   {ZKEY,           ZKEY,        0 },   //SHIP SPECIAL
   {CAPSLOCKKEY,    CAPSLOCKKEY, 0 },   //TACTICAL OVERLAY
   {SPACEKEY,       SPACEKEY,    0 },   //SENSORS MANAGER
   {HOMEKEY,        HOMEKEY,     0 },   //MOTHERSHIP
   {KKEY,           KKEY,        0 },   //KAMIKAZE
   {TILDEKEY,       TILDEKEY,    0 },   //CANCEL ORDERS
   {0,0,0 }
};



keyindex DefaultKeyTranslations[OP_NumKeys] =
{
   TABKEY,              //NEXT FORMATION
   BKEY,                //BUILD MANAGER
   CKEY,                //PREVIOUS FOCUS
   VKEY,                //NEXT FOCUS
   DKEY,                //DOCK
   EKEY,                //SELECT ALL VISIBLE
   FKEY,                //FOCUS
   RKEY,                //RESEARCH MANAGER
   HKEY2,                //HARVEST
   MKEY,                //MOVE
   RBRACK,              //NEXT TACTIC
   LBRACK,              //PREVIOUS TACTIC
   SKEY,                //SCUTTLE
   ZKEY,                //SHIP SPECIAL
   CAPSLOCKKEY,         //TACTICAL OVERLAY
   SPACEKEY,            //SENSORS MANAGER
   HOMEKEY,             //MOTHERSHIP
   KKEY,                //KAMIKAZE
   TILDEKEY             //CANCEL ORDERS
};



typedef struct
{
    keyindex key;
    char *name;
}
KeyNameStruct;

KeyNameStruct KeyNames[] =
{
   {AKEY,           "A    "},
   {BKEY,           "B    "},
   {CKEY,           "C    "},
   {DKEY,           "D    "},
   {EKEY,           "E    "},
   {FKEY,           "F    "},
   {GKEY,           "G    "},
   {HKEY2,           "H    "},
   {IKEY,           "I    "},
   {JKEY,           "J    "},
   {KKEY,           "K    "},
   {LKEY,           "L    "},
   {MKEY,           "M    "},
   {NKEY,           "N    "},
   {OKEY,           "O    "},
   {PKEY,           "P    "},
   {QKEY,           "Q    "},
   {RKEY,           "R    "},
   {SKEY,           "S    "},
   {TKEY,           "T    "},
   {UKEY,           "U    "},
   {VKEY,           "V    "},
   {WKEY,           "W    "},
   {XKEY,           "X    "},
   {YKEY,           "Y    "},
   {ZKEY,           "Z    "},

   {BACKSPACEKEY,   "BKSPC"},
   {TABKEY,         "TAB  "},
   {ARRLEFT,        "LEFT "},
   {ARRRIGHT,       "RIGHT"},
   {ARRUP,          "UP   "},
   {ARRDOWN,        "DOWN "},
   {LBRACK,         "[    "},
   {RBRACK,         "]    "},
   {CAPSLOCKKEY,    "CAPS "},
   {SPACEKEY,       "SPACE"},
   {ENTERKEY,       "ENTER"},
   {HOMEKEY,        "HOME "},
   {PAGEDOWNKEY,    "PGDN "},
   {PAGEUPKEY,      "PGUP "},
   {BACKSLASHKEY,   "\\    "},
   {PAUSEKEY,       "PAUSE"},
   {SCROLLKEY,      "SCRLL"},
   {PRINTKEY,       "PRINT"},
   {DELETEKEY,      "DELET"},
   {TILDEKEY,       "TILDE"},
   {NUMPAD0,        "NUM0"},
   {NUMPAD1,        "NUM1"},
   {NUMPAD2,        "NUM2"},
   {NUMPAD3,        "NUM3"},
   {NUMPAD4,        "NUM4"},
   {NUMPAD5,        "NUM5"},
   {NUMPAD6,        "NUM6"},
   {NUMPAD7,        "NUM7"},
   {NUMPAD8,        "NUM8"},
   {NUMPAD9,        "NUM9"},
   {NUMMINUSKEY,    "NUM-"},
   {NUMPLUSKEY,     "NUM+"},
   {NUMSTARKEY,     "NUM*"},
   {NUMSLASHKEY,    "NUM/"},
   {NUMDOTKEY,      "NUM."},
   {MINUSKEY,       "-"},
   {PLUSKEY,        "+"},
   {F1KEY,          "F1"},
   {F2KEY,          "F2"},
   {F3KEY,          "F3"},
   {F4KEY,          "F4"},
   {F5KEY,          "F5"},
   {F6KEY,          "F6"},
   {F7KEY,          "F7"},
   {F8KEY,          "F8"},
   {F9KEY,          "F9"},
   {F10KEY,          "F10"},
   {F11KEY,          "F11"},
   {F12KEY,          "F12"},

   {0,""}
};


char opKeymap[OP_KEYMAPSIZE] = "Z";


sdword opSpeakerSettings[3][NUM_EQ_BANDS] =
{
    {100,66,33,0,0,0,0,0},   //multimedia
    {0,0,0,0,0,0,0,0},       //stereo
    {0,17,33,50,50,33,17,0}, //headphones
};


fonthandle opKeyboardFont;


//use #define to modify a variable directly without modifying function

extern void mgDrawArrow(regionhandle region, bool32 leftArrow, bool32 human);

sdword opNoPalMaxMB = 128;
sdword opNoPalMinMB = 16;
sdword opSaveNoPalMB;
sdword opNoPalMB = 64;

static regionhandle opNoPalDrawRegion = NULL;

static sdword opOldDeviceIndex;

udword opDeviceCRC = 0;
sdword opDeviceIndex = -1;

sdword opMusicVol=60;                   // CAT: the exe's default 60 (.data 0x0088be4c; HW1 75)
sdword opSaveMusicVol;
sdword opSFXVol=75;
sdword opSaveSFXVol;
sdword opSpeechVol=75;                  // CAT: the exe's default 75 (.data 0x0088be54; HW1 60)
sdword opSaveSpeechVol;
sdword opNumChannels=SOUND_DEF_VOICES-SOUND_MIN_VOICES;
sdword opSaveNumChannels;
sdword opAutoChannel=SOUND_MODE_NORM;
sdword opSaveAutoChannel;
sdword opSoundQuality=SOUND_MODE_NORM;
sdword opSaveSoundQuality;
sdword opSpeakerSetting=1;
sdword opSaveSpeakerSetting;
sdword opEqualizerSettings[NUM_EQ_BANDS];
sdword opSaveEqualizerSettings[NUM_EQ_BANDS];
real32 opEQReal[NUM_EQ_BANDS];
sdword opVoiceSetting=2;
sdword opVoice0On = 1, opVoice1On = 1, opVoice2On = 1;
sdword opSaveVoice0On, opSaveVoice1On, opSaveVoice2On;
sdword opSaveVoiceSetting;
sdword opVoiceComm=1;
sdword opSaveVoiceComm;
sdword opVoiceStat=1;
sdword opSaveVoiceStat;
sdword opVoiceChat=1;
sdword opSaveVoiceChat;


#define OP_DETAIL_DIVISOR 22

sdword opSaveDetailThresholdVal;
udword opDetailThresholdVal = 10;       // CAT: a frame rate now (cat_lodSetTimeThreshold), the exe's default 10 (.data 0x0088be6c; HW1 100 polys/22)
sdword opSaveBrightnessVal;
sdword opBrightnessVal = 30;
sdword opNoLODVal = 0;
sdword opSaveNoLODVal;

sdword opSaveMAIN_WindowWidth;
sdword opSaveMAIN_WindowHeight;
sdword opSaveMAIN_WindowDepth;
sdword opSaveRenderDevice;


sdword opLightingVal=1;
sdword opSaveLightingVal;
sdword opEffectsVal=0;
sdword opSaveEffectsVal;
sdword savetexLinearFiltering;
sdword saveshowBackgrounds;
sdword savesmInstantTransition;
sdword saveenableSmoothing;
sdword saveenableStipple;
sdword saveenableTrails;
sdword savesmFuzzyBlobs;
sdword saveHitEffects;
sdword saveDamagEffects;
sdword saveMuzzleEffects;
sdword saveBulletEffects;

sdword opAutodockFuelCheck = 0;
sdword opSaveAutodockFuelCheck;
sdword opAutodockHealthCheck = 1;
sdword opSaveAutodockHealthCheck;

sdword opAutodockHealthVal = 20;
sdword opSaveAutodockHealthVal;
sdword opAutodockFuelVal = 5;
sdword opSaveAutodockFuelVal;


sdword opMouseSens = 50;
sdword opBattleChatter = 50;
sdword opInfoOverlayVar = 1;
sdword opSaveMouseSens;
sdword opSaveBattleChatter;
sdword opSaveInfoOverlay = 1;

sdword opNumEffects;
sdword opSaveNumEffects;

// CAT (M2-R6 frontend): Cataclysm's own options (the options-file rows TextureDetail, GameFramerateOption,
//      HardwareTransformLighting, SetTextureRequirements). The toggles of the Gameplay and sensors pages live with their
//      users (InfoOverlay.c catMo*, mainrgn.c catMr*, PopupMenu.c catPmLargeFont, Sensors.c catSmView*, ShipSelect.c
//      catOpExperienceMessages, LOD.c catLodDetail, main.c enableCPV / enableLensFlare / enableRepairbots)
sdword catOpTextureDetail = 50;                     // 0x0088be74: texture detail 0..100 (cat_trTextureMemorySet)
sdword catOpGameFramerate = 1;                      // 0x0088be98: the OP_CataclysmFramerate radio (0 = the low-end set)
sdword catOpHardwareTL = 0;                         // 0x00a4cc18: OP_HardwareTransformLighting's word (its callback is a
                                                    //   nullsub; the accept helper compares it, the save / restore keep it)
sdword catOpTextureRequirementsSet = 0;             // 0x00a76260: the start-up's memory test leaves the texture detail
                                                    //   alone when the options file sets it (utyGameSystemsInit 0x005abe70)
static regionhandle catOpTextureDetailRegion = NULL;    // 0x00a4cc1c: the texture-detail slider
static regionhandle catOpEffectsRegion = NULL;      // 0x00a4cc60: the OP_Effects radio (opEffects 0x004d9cc0)

// the words opOptionsSaveSettings 0x004d8d00 saves and opRestoreSavedSettings 0x004d7f80 puts back, beyond HW1's
// (the dynamic CPV, lens flare, repair droid and LOD detail words go with the custom effects' set)
static sdword *catOpSavedWords[] =
{
    &catOpHardwareTL, &catOpGameFramerate, (sdword *)&catPmLargeFont,
    (sdword *)&catMoHWTaskbar, (sdword *)&catMoTaskbarButtons, (sdword *)&catMoTaskbarBackdrop,
    (sdword *)&catMrHomeworldCSA, (sdword *)&catMrRightClickCSA, (sdword *)&catMrWaypointCentre, &catOpExperienceMessages,
    (sdword *)&catSmViewAreasVisited, (sdword *)&catSmViewAreasOccupied, (sdword *)&catSmViewCompass,
    (sdword *)&catSmViewPieplateLines, (sdword *)&catSmViewShipMeshes, (sdword *)&catSmViewResources,
    (sdword *)&catSmViewMeteors, (sdword *)&catSmViewFOWRanges, (sdword *)&catSmViewDecorativeAsteroids,
    (sdword *)&catSmViewBackground, (sdword *)&catSmViewIcons, (sdword *)&catSmViewObjectivePointers,
    (sdword *)&catSmViewSlipgatePaths,
    NULL
};
#define CAT_OP_NumSavedWords    23
static sdword catOpSavedValues[CAT_OP_NumSavedWords];
_Static_assert(sizeof(catOpSavedWords) / sizeof(catOpSavedWords[0]) == CAT_OP_NumSavedWords + 1, "the saved-word table and its count agree");
static sdword catOpSaveTextureDetail;               // 0x00c30a38
static sdword catOpSaveCPV, catOpSaveLensFlare, catOpSaveRepairbots;   // 0x00c30a84 / 0x00c30a34 / 0x00c30b5c
static udword catOpSaveLodDetail;                   // 0x00c30b44

// Game Options - Default Values
udword opShipRecoil = 0;
udword opPauseOrders = 0;

textentryhandle opAutodockFuelEntryBox     = NULL;
textentryhandle opAutodockHealthEntryBox   = NULL;

#define MAX_NUM_LENGTH 12


//video listboxes

fonthandle opResListFont = 0;
listwindowhandle opResListWindow = NULL;
sdword opResNumber;
sdword opResCurrentSelected;

typedef struct opres
{
    sdword width, height, depth;
} opres;

opres* opRes = NULL;

fonthandle opRenderListFont = 0;
listwindowhandle opRenderListWindow = NULL;
sdword opRenderNumber = -1;
sdword opRenderCurrentSelected = -1;

sliderhandle EffectsSlider = NULL;

typedef struct oprender
{
    int type;
    char data[64];
    char name[64];
    rdevice* dev;
} oprender;

oprender* opRnd = NULL;

static oprender* opRndSelected;
static oprender* opSaveRndSelected;

regionhandle opResListRegion = NULL;

//end video


void opCancelKeyDetour(void);


/*=============================================================================
    Private functions:
=============================================================================*/

/*=============================================================================
    Functions:
=============================================================================*/

void opDirtyResListWindow(void)
{
    if (opResListRegion != NULL)
    {
#ifdef DEBUG_STOMP
        regVerify(((regionhandle)opResListRegion));
#endif
        opResListRegion->status |= RSF_DrawThisFrame;
    }
}

/*-----------------------------------------------------------------------------
    Name        : opOptionsInit
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/

void opOptionsInit(void)
{
    opKeyboardFont = frFontRegister("hw_eurosecond_11.hff");
    opNumEffects = etgHistoryScalar - etgHistoryScalarMin;
    // CAT: opOptionsInit 0x004d71a0 ends here: kbInitKeyBindings runs at the end of utyGameSystemsInit 0x005abe70 (after
    //      cat_kbLoadKeyBindings), see utility.c
}

// CAT: no opSetDetailThreshold: the detail threshold is a frame rate for cat_lodSetTimeThreshold 0x004a50e0 (LOD.c),
//      not HW1's auto-LOD target of 22 polygons per step

/*-----------------------------------------------------------------------------
    Name        : opUpdateSettings
    Description : update game system settings after reading configuration file
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void opUpdateSettings(void)
{
    // CAT: opUpdateSettings 0x004d71d0: the effects, the frame-rate threshold of the LODs (HW1: the auto-LOD target
    //      polygons), the gamma and the texture detail (HW1: trNoPalResizePool), then the save
    opEffectsHelper();
    cat_lodSetTimeThreshold(opDetailThresholdVal);
    shGammaSet(opBrightnessVal);
    cat_trTextureMemorySet(catOpTextureDetail);
    opOptionsSaveSettings();
}

/*============================================================================

    Keyboard options

============================================================================*/


udword opSelectKey(regionhandle region, sdword ID, udword event, udword data);

/*-----------------------------------------------------------------------------
    Name        : opKeyboardDraw
    Description : Callback for keyboard redefinitions
    Inputs      : standard region callback
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void opKeyboardDraw(featom *atom, regionhandle region)
{
    sdword x, y, i;
    rectangle rect = region->rect, select;
    color c;
    fonthandle currentFont;
    bool32 hl = FALSE;

    keyboardregion = region;

    currentFont = fontMakeCurrent(opKeyboardFont);

    if (region->flags == 0 || region->flags == RPE_DrawFunctionAdded)
    {                                                       //if region not processed yet
        region->flags = RPE_PressLeft | RPE_PressRight |
                        RPE_WheelUp | RPE_WheelDown;        //receive mouse presses from now on
        regFunctionSet(region, (regionfunction) opSelectKey);          //set new region handler function
        regFilterSet(region, region->flags | RPE_DoubleLeft | RPE_DoubleRight);

        //opKeyboardStart();
    }

    feStaticRectangleDraw(region);                          //draw standard rectangle

    c = colRGB( 30, 160, 190);

    //fontPrint(100,100, c, "hello 1234");

    //rect.x1 -= CM_ProgressMarginRight;

    primRectSolid2(&rect, colRGB(0, 0, 0));

    y = region->rect.y0 + OP_KBMarginTop;
    x = region->rect.x0 + OP_KBMarginLeft;

    for (i = 0; KeyTranslations[i].sendkey != 0; i++)
    {
        if (i == OP_KeysPerColumn)
        {
            y = region->rect.y0 + OP_KBMarginTop;
            x += OP_KBColumnTab;
        }

        c = FEC_ListItemStandard;//OP_KeyNameColor;
        if (i == opKeySelected)
        {
            c = FEC_ListItemSelected;//OP_KeySelectedColor;

            select.x0 = x - OP_BoxSpacingX;
            select.y0 = y - OP_BoxSpacingY;
            select.x1 = x + OP_KBBoxwidth;
            select.y1 = y - OP_BoxSpacingY + OP_KeyVertSpacing;

            primRectTranslucent2(&select, FEC_ListItemTranslucent);
            primRectOutline2(&select, 1, FEC_ListItemTranslucentBorder);
        }

        if (i == opKeyBeingDefined)
        {
            c = OP_KeyRedefiningColor;
        }

        if (TempKeyTranslations[i].highlight)
        {
            c = OP_KeyHighlightColor;
            TempKeyTranslations[i].highlight = 0;
            hl = TRUE;
        }
        else
        {
            hl = FALSE;
        }

        fontPrint(x, y, c, TempKeyTranslations[i].funcname);

        c = (hl) ? OP_KeyHighlightColor : OP_KeyPrimaryColor;
        fontPrint(x + OP_KBPrimaryKeyTab, y, c,
                  opKeyToNiceString(TempKeyTranslations[i].key1));

        c = (hl) ? OP_KeyHighlightColor : OP_KeySecondaryColor;
        fontPrint(x + OP_KBSecondaryKeyTab, y, c,
                  opKeyToNiceString(TempKeyTranslations[i].key2));

        y += OP_KeyVertSpacing;
    }

    fontMakeCurrent(currentFont);
}

/*-----------------------------------------------------------------------------
    Name        : opSelectKey
    Description : Callback for selecting keystrokes
    Inputs      : standard region callback
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
udword opSelectKey(regionhandle region, sdword ID, udword event, udword data)
{
    //sdword index = cmSelectShipType(region, mouseCursorY());
    uword column=0,row = 0;

    bitClear(data, RF_ShiftBit);

    if ((mouseCursorX() > region->rect.x0+OP_KBMarginLeft) &&
        (mouseCursorX() < region->rect.x0+OP_KBMarginLeft+OP_KBColumnTab))
    {
        column = 1;
    }
    else if ((mouseCursorX() > region->rect.x0+OP_KBMarginLeft+OP_KBColumnTab ) &&
             (mouseCursorX() < region->rect.x0+OP_KBMarginLeft+2*OP_KBColumnTab))
    {
        column = 2;
    }

#ifdef DEBUG_STOMP
    regVerify(region);
#endif

    if (column)
    {
        if ((mouseCursorY() > region->rect.y0+OP_KBMarginTop) &&
          (mouseCursorY() < region->rect.y0+OP_KBMarginTop
           + (OP_KeyVertSpacing * OP_KeysPerColumn) ))

        {
            row = (uword)((mouseCursorY() - region->rect.y0+OP_KBMarginTop)/OP_KeyVertSpacing) - 1;
            if ( event == RPE_PressLeft   )
            {                                                       //left press (select/add job)
                opKeyBeingDefined = -1;
                opKeySelected = (column - 1)*OP_KeysPerColumn + row;
                bitSet(region->status, RSF_DrawThisFrame);
            }
            else if ( event == RPE_DoubleLeft   )
            {                                                       //left press (select/add job)
                opKeyBeingDefined = (column - 1)*OP_KeysPerColumn + row;

                feScreenStart(feStack[feStackIndex].baseRegion, "Press_Key"); //popup
                opKeyDetour = 1;         //send next keypress to opDefineThisKey
                bitSet(region->status, RSF_DrawThisFrame);
                bitSet(keyboardregion->status, RSF_KeyCapture);
                keyBufferClear();
            }
        }
    }

    return 0;
}


/*-----------------------------------------------------------------------------
    Name        : opHighlightTaskFunction
    Description : Task function for higlighting
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 opHighlightBabyFunction(udword num, void* data, struct BabyCallBack* baby)
{
    sdword i;

    for (i = 0; KeyTranslations[i].sendkey > 0; i++)
    {
        TempKeyTranslations[i].highlight = 0;
    }

#ifdef DEBUG_STOMP
    regVerify(keyboardregion);
#endif

    bitSet(keyboardregion->status, RSF_DrawThisFrame);

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : opDefineThisKey
    Description : keystroke from mrKeyPress is sent here
    Inputs      : standard region callback
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void opDefineThisKey(keyindex key)
{
    if (key == ESCKEY)
    {
        opCancelKeyDetour();
        return;
    }

    if (opValidKey(key))
    {
        opKeyAssign(opKeyBeingDefined, key);
    }

    opCancelKeyDetour();

    //highlight
    opHighlightBaby = taskCallBackRegister(opHighlightBabyFunction, 0, NULL, (real32)OP_KeyHighlightOnTime);

#ifdef DEBUG_STOMP
    regVerify(keyboardregion);
#endif

    bitSet(keyboardregion->status, RSF_DrawThisFrame);
//    bitClear(keyboardregion->status, RSF_KeyCapture);
}


/*-----------------------------------------------------------------------------
    Name        : opKeyTranslate
    Description : translate keypresses
    Inputs      : key - keyindex of key being pressed
    Outputs     :
    Return      : translated key to be fed to mrKeyPressed
-------------------------------------------------------------------------------*/
keyindex opKeyTranslate(keyindex key)
{
    udword i;

    for (i=0; KeyTranslations[i].sendkey > 0; ++i)
    {
        if (key == KeyTranslations[i].key1)
        {
            return KeyTranslations[i].sendkey;
        }

        if (key == KeyTranslations[i].key2)
        {
            return KeyTranslations[i].sendkey;
        }
    }
    //dbgMessagef("Untranslatable key: 0x%x", key);
    return key;

}

/*----------------------------------------------------------------------------
    Name        : opKeyAssign
    Description : assign new keystroke to a function
    Inputs      : func: function to assign, key: keyindex of key
    Outputs     :
    Return      :
-------------------------------------------------------------------------------*/
void opKeyAssign(uword func, keyindex key)
{
    uword i;
    sword oldfunc = -1;
    bool32 oldfuncsecondkey = FALSE;


    //find function key was previously assigned to
    for (i=0; KeyTranslations[i].sendkey > 0; ++i)
    {
        if (key == TempKeyTranslations[i].key1)
        {
            //already a primary key
            oldfunc = i;
            break;
        }

        if (key == TempKeyTranslations[i].key2)
        {
            //already a secondary key
            oldfunc = i;
            oldfuncsecondkey = TRUE;
            break;
        }
    }

    //if it's already used:

    if (oldfunc >= 0)
    {
        if (oldfuncsecondkey)
        {
            //clear it
            TempKeyTranslations[oldfunc].key2 = 0;
            TempKeyTranslations[oldfunc].highlight = OP_KeyHighlightTime;

            //new func:
            //primary becomes secondary, new becomes primary
            TempKeyTranslations[func].key2 = TempKeyTranslations[func].key1;
            TempKeyTranslations[func].key1 = key;
            TempKeyTranslations[func].highlight = OP_KeyHighlightTime;
        }
        else
        {
            //old function was primary key

            if (TempKeyTranslations[oldfunc].key2)
            {
                //if there's second key, use it
                TempKeyTranslations[oldfunc].key1 = TempKeyTranslations[oldfunc].key2 ;
                TempKeyTranslations[oldfunc].key2 = 0;
                TempKeyTranslations[oldfunc].highlight = OP_KeyHighlightTime;

                TempKeyTranslations[func].key2 = TempKeyTranslations[func].key1;
                TempKeyTranslations[func].key1 = key;
                TempKeyTranslations[func].highlight = OP_KeyHighlightTime;
                TempKeyTranslations[func].highlight = OP_KeyHighlightTime;
            }
            else
            {
                //there is no secondary key for old function
                //swap with the current key for the function being redefined
                if (TempKeyTranslations[func].key2)
                {
                    //old func gets secondary key
                    TempKeyTranslations[oldfunc].key1 = TempKeyTranslations[func].key2;
                    TempKeyTranslations[oldfunc].highlight = OP_KeyHighlightTime;
                    //new func:
                    //primary becomes secondary, new becomes primary
                    TempKeyTranslations[func].key2 = TempKeyTranslations[func].key1;
                    TempKeyTranslations[func].key1 = key;
                    TempKeyTranslations[func].highlight = OP_KeyHighlightTime;
                }
                else
                {
                    //old func gets primary key
                    TempKeyTranslations[oldfunc].key1 = TempKeyTranslations[func].key1;
                    TempKeyTranslations[oldfunc].highlight = OP_KeyHighlightTime;
                    //new func:
                    //gets no secondary, new becomes primary
                    TempKeyTranslations[func].key2 = 0;
                    TempKeyTranslations[func].key1 = key;
                    TempKeyTranslations[func].highlight = OP_KeyHighlightTime;
                }
            }
        }
    }
    else
    {
        //the key was not used already
        TempKeyTranslations[func].key2 = TempKeyTranslations[func].key1;
        TempKeyTranslations[func].key1 = key;
        TempKeyTranslations[func].highlight = OP_KeyHighlightTime;
    }
}

/*----------------------------------------------------------------------------
    Name        : opKeyToNiceString
    Description :
    Inputs      :
    Outputs     :
    Return      :
-------------------------------------------------------------------------------*/
char* opKeyToNiceString(keyindex key)
{
    sdword i;

    if (key == 0)
    {
        return "---";
    }

    for (i = 0; KeyNames[i].key > 0; i++)
    {
        if (KeyNames[i].key == key)
        {
            return KeyNames[i].name;
        }
    }

    return "[ERR]";
}

/*----------------------------------------------------------------------------
    Name        : opValidKey
    Description :
    Inputs      :
    Outputs     :
    Return      :
-------------------------------------------------------------------------------*/
bool32 opValidKey(keyindex key)
{
    sdword i;

    for (i = 0; KeyNames[i].key > 0; i++)
    {
        if (KeyNames[i].key == key)
        {
            return TRUE;
        }
    }

    return FALSE;
}

void opKeyboardStart(void)
{
    sdword i;

    for (i = 0; KeyTranslations[i].sendkey > 0; i++)
    {
        TempKeyTranslations[i].key1 = KeyTranslations[i].key1;
        TempKeyTranslations[i].key2 = KeyTranslations[i].key2;
    }
}

char UpperHex2Char(uword num)
{
    uword x;

    x = num >> 4;
    if (x>=10)
        return ('A' + x-10);
    else
        return ('0' + x);

}

char LowerHex2Char(uword num)
{
    uword x;

    x = num & 15;
    if (x>=10)
        return ('A' + x-10);
    else
        return ('0' + x);

}

uword Char2Hex(char ch)
{
    if (ch >= 'A')
        return (ch - 'A' + 10);
    else
        return (ch - '0');

}

char ConvertSliderToAIPlayerDifficulty(sdword sliderval)
{
    if (sliderval <= 33)
    {
        return AI_BEGINNER;
    }

    if (sliderval <= 66)
    {
        return AI_INTERMEDIATE;
    }

    return AI_ADVANCED;
}

bool32 opResHackSupported(void)
{
    rmode* mode;
    int width, height, depth;

    width  = opSaveMAIN_WindowWidth;
    height = opSaveMAIN_WindowHeight;
    depth  = opSaveMAIN_WindowDepth;

    mode = opSaveRndSelected->dev->modes;
    while (mode != NULL)
    {
        if (mode->width == width &&
            mode->height == height &&
            mode->depth == depth)
        {
            return TRUE;
        }
        mode = mode->next;
    }

    return FALSE;
}

bool32 opResResSupported(opres* res)
{
    rdevice* dev;
    rmode* mode;
    int width, height, depth;

    dev = opRndSelected->dev;

    width  = res->width;
    height = res->height;
    depth  = res->depth;

    mode = dev->modes;
    while (mode != NULL)
    {
        if (mode->width == width &&
            mode->height == height &&
            mode->depth == depth)
        {
            return TRUE;
        }
        mode = mode->next;
    }

    return FALSE;
}

bool32 opResChanged(void)
{
    if (opSaveMAIN_WindowWidth  != MAIN_WindowWidth ||
        opSaveMAIN_WindowHeight != MAIN_WindowHeight ||
        opSaveMAIN_WindowDepth  != MAIN_WindowDepth)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static void opGLCStop(void)
{
    // only glcompat function here, should be remove
}

static void opGLCStart(void)
{
    utyForceTopmost(fullScreen);
}

void opModeswitchFailed(void)
{
    GeneralMessageBox(strGetString(strOPFailed0),
                      strGetString(strOPFailed1));
    opGLCStart();
}

void opCountdownYes(char* name, featom* atom)
{
    opTimerActive = FALSE;
    feScreenDisappear(NULL, NULL);
    opGLCStart();
}

void opCountdownNo(char* name, featom* atom)
{
    opTimerActive = FALSE;
    if (name != NULL || atom != NULL)
    {
        feScreenDisappear(NULL, NULL);
    }
    soundEventShutdown();
    opGLCStop();
    mainShutdownRenderer();
    mainRestoreRender();
    opGLCStart();
    soundEventRestart();
    opDeviceIndex = opOldDeviceIndex;
}

void opTimerExpired(void)
{
    feAllScreensDelete();
    feScreenStart(ghMainRegion, "Main_game_screen");
    opCountdownNo(NULL, NULL);
}

void opCountdownBoxDraw(featom* atom, regionhandle region)
{
    fonthandle oldfont;
    char* str;
    sdword x, y, width, height;
    color c;
    extern fonthandle ghDefaultFont;

    oldfont = fontMakeCurrent(ghDefaultFont);

    x = region->rect.x0;
    y = region->rect.y0;
    width  = region->rect.x1 - region->rect.x0;
    height = 0;
    c = colRGB(255,200,0);

    str = strGetString(strOPCountdown0);
    fontPrintf(x + (width - fontWidth(str))/2, y + height, c, str);
    height += fontHeight(str);

    str = strGetString(strOPCountdown1);
    fontPrintf(x + (width - fontWidth(str))/2, y + height, c, str);
    height += fontHeight(str);

    str = strGetString(strOPCountdown2);
    fontPrintf(x + (width - fontWidth(str))/2, y + height, c, str, (sdword)opTimerLength - 2);

    fontMakeCurrent(oldfont);
}

void opCountdownBoxStart(void)
{
    opTimerStart  = taskTimeElapsed;
    opTimerActive = TRUE;
    feScreenStart(ghMainRegion, "Countdown_Message_Box");
}

void opOptionsAcceptHelper(char* name, featom* atom, char* linkName)
{
    udword i, temp;
    oprender* rnd;

    //copy temporary changed data to permanent storage

    //copy keyboard options

    for (i=0; KeyTranslations[i].sendkey > 0; ++i)
    {
        KeyTranslations[i].key1 = TempKeyTranslations[i].key1;
        KeyTranslations[i].key2 = TempKeyTranslations[i].key2;

        //also save for next time
        opKeymap[i*4]   = UpperHex2Char(KeyTranslations[i].key1);
        opKeymap[i*4+1] = LowerHex2Char(KeyTranslations[i].key1);
        opKeymap[i*4+2] = UpperHex2Char(KeyTranslations[i].key2);
        opKeymap[i*4+3] = LowerHex2Char(KeyTranslations[i].key2);
    }


    //audio - don't copy since we're doing live updates


    //video - copy selected stuff
    if (opResListWindow != NULL &&
        opResListWindow->CurLineSelected != NULL)
    {
        for (i = 0; i < opResNumber; i++)
        {
            if (opResListWindow->CurLineSelected->data == (ubyte*)&opRes[i])
            {
                opResCurrentSelected = i;
                mainWindowWidth  = opRes[i].width;
                mainWindowHeight = opRes[i].height;
                mainWindowDepth  = opRes[i].depth;
                break;
            }
        }
    }

    if (opRenderListWindow != NULL &&
        opRenderListWindow->CurLineSelected != NULL)
    {
        for (i = 0; i < opRenderNumber; i++)
        {
            if (opRenderListWindow->CurLineSelected->data == (ubyte*)&opRnd[i])
            {
                opRenderCurrentSelected = i;
                break;
            }
        }
    }

    opOptionsEndHelper(linkName);

    if (gameIsRunning)
    {
        return;     // don't allow render switches in game!
    }

    if (opRnd == NULL)
    {
        return;
    }

    //rswitch

    rnd = opSaveRndSelected;

    mainWindowWidth  = opSaveMAIN_WindowWidth;
    mainWindowHeight = opSaveMAIN_WindowHeight;
    mainWindowDepth  = opSaveMAIN_WindowDepth;

    opReloading = TRUE;
    opOldDeviceIndex = opDeviceIndex;

#ifndef __EMSCRIPTEN__
    if (opResChanged() || opDeviceIndex != opRenderCurrentSelected)
    {
        if (opResHackSupported())
        {
            soundEventShutdown();
            mainSaveRender();
            opGLCStop();

            MAIN_WindowWidth  = opSaveMAIN_WindowWidth;
            MAIN_WindowHeight = opSaveMAIN_WindowHeight;
            MAIN_WindowDepth  = opSaveMAIN_WindowDepth;

            if (mainLoadGL(rnd->data))
            {
                opDeviceIndex = opRenderCurrentSelected;
                opCountdownBoxStart();
            }
            else
            {
                mainRestoreRender();
                opModeswitchFailed();
            }

            soundEventRestart();
            SDL_Delay(20);
            opGLCStart();
        }
    }
#endif

    opReloading = FALSE;

    // CAT: no auto-LOD read-back (opOptionsAcceptHelper 0x004d76d0: the detail threshold is the frame-rate value the
    //      slider set). The exe's device switch also reloads when OP_HardwareTransformLighting changed (catOpHardwareTL
    //      against its saved copy); GoK's SDL device code above has no such device (decision 28)
    (void)temp;

    //update saved settings
    opOptionsSaveSettings();
}

void opOptionsAccept(char* name, featom* atom)
{
    opOptionsAcceptHelper(name, atom, "Main_game_screen");
}

void opInGameOptionsAccept(char* name, featom* atom)
{
    opOptionsAcceptHelper(name, atom, "Game_options");
    catOpTutorialGameplayDisable();
}


void opKeyboardLoad(void)
{
    udword i;


    if (opKeymap[0] != 'Z' )
    {
        dbgMessage("Keymap not found");
        for (i=0; KeyTranslations[i].sendkey > 0; ++i)
        {
            KeyTranslations[i].key1 = Char2Hex(opKeymap[i*4])*16 + Char2Hex(opKeymap[i*4+1]);
            KeyTranslations[i].key2 = Char2Hex(opKeymap[i*4+2])*16 + Char2Hex(opKeymap[i*4+3]);
        }

    }
    // else fine - they're already initialized

}

real32 normalizeSmoothie(sdword val)
{
    real32 fval;

    fval = ((real32)val * 1.025f) / 100.0f;
    if (fval < 0.0f) fval = 0.0f;
    if (fval > 1.0f) fval = 1.0f;

    return fval;
}

void opUpdateAudioSettings(void)
{
    sdword min, max;

    opEQHelper();

    soundEventVocalSettings(opVoiceComm, opVoiceStat, opVoiceChat);

    soundEventMasterEQ((real32*)opEQReal);

    soundEventMusicMasterVol(normalizeSmoothie(opMusicVol));
    soundEventSFXMasterVol(normalizeSmoothie(opSFXVol));
    soundEventSpeechMasterVol(normalizeSmoothie(opSpeechVol));

    soundGetVoiceLimits(&min, &max);
    soundSetNumVoices(opNumChannels + min, opAutoChannel);

    soundMixerSetMode(opSoundQuality);
}

void opRestoreSavedEqualizerSettings(void)
{
    sdword i;

    opSpeakerSetting = opSaveSpeakerSetting;

    for (i = 0; i < NUM_EQ_BANDS; i++)
    {
        opEqualizerSettings[i] = opSaveEqualizerSettings[i];
        opEQReal[i] = 1.0f - ((real32)opEqualizerSettings[i] / 100.0f);
    }

    soundEventMasterEQ((real32*)opEQReal);
}

void opRestoreSavedCustomEffectsSettings(void)
{
    enableCPV = catOpSaveCPV;                               // CAT: see opOptionsSaveCustomEffectsSettings
    enableLensFlare = catOpSaveLensFlare;
    enableRepairbots = catOpSaveRepairbots;
    catLodDetail = catOpSaveLodDetail;
    opEffectsVal = opSaveEffectsVal;
    texLinearFiltering = savetexLinearFiltering;
    showBackgrounds = saveshowBackgrounds;
    smInstantTransition = savesmInstantTransition;
    enableSmoothing = saveenableSmoothing;
    enableStipple = saveenableStipple;
    enableTrails = saveenableTrails;
    smFuzzyBlobs = savesmFuzzyBlobs;

    etgHitEffectsEnabled = saveHitEffects;
    etgDamageEffectsEnabled = saveDamagEffects;
    etgFireEffectsEnabled = saveMuzzleEffects;
    etgBulletEffectsEnabled = saveBulletEffects;
}

void opRestoreSavedSettings(void)
{
    //copy audio back

    sdword i;

    // CAT: opRestoreSavedSettings 0x004d7f80: the sound quality is not restored here (the advanced page's cancel does
    //      it), the speaker setting's equalizer is applied (opEQHelper inlined) as in opUpdateAudioSettings
    opMusicVol =     opSaveMusicVol;
    opSFXVol =       opSaveSFXVol;
    opSpeechVol =    opSaveSpeechVol;
    opNumChannels =  opSaveNumChannels;
    opAutoChannel =  opSaveAutoChannel;
//    opSpeakerSetting =  opSaveSpeakerSetting;

    opUpdateAudioSettings();

    //video
    // CAT: the hardware T&L word, the frame-rate threshold (cat_lodSetTimeThreshold) and the texture detail
    //      (cat_trTextureMemorySet) instead of HW1's no-palette pool and auto-LOD polygons
    opDetailThresholdVal = opSaveDetailThresholdVal;
    cat_lodSetTimeThreshold(opDetailThresholdVal);
    catOpTextureDetail = catOpSaveTextureDetail;
    cat_trTextureMemorySet(catOpTextureDetail);
    opBrightnessVal = opSaveBrightnessVal;
    shGammaSet(opBrightnessVal);
    opNoLODVal = opSaveNoLODVal;
    opNumEffects = opSaveNumEffects;
    etgHistoryScalar = opNumEffects + etgHistoryScalarMin;
    for (i = 0; catOpSavedWords[i] != NULL; i++)
    {                                                       //CAT: Cataclysm's options
        *catOpSavedWords[i] = catOpSavedValues[i];
    }

    opRestoreSavedCustomEffectsSettings();

    //gameplay
    // CAT: no ioEnable / ioDisable here (the exe only restores the word; nothing else in the exe reads it)
    opMouseSens = opSaveMouseSens;
    opInfoOverlayVar = opSaveInfoOverlay;

    cameraSensitivitySet(opMouseSens);

    opAutodockHealthCheck = opSaveAutodockHealthCheck;
    opAutodockFuelCheck = opSaveAutodockFuelCheck;
    opAutodockHealthVal = opSaveAutodockHealthVal;
    opAutodockFuelVal = opSaveAutodockFuelVal;
    cat_kbReloadKeyBindings(FALSE);         // CAT: opRestoreSavedSettings 0x004d7f80 ends with the bindings from keys.cfg again
}

void opOptionsCancelHelper(char* name, featom* atom, char* linkName)
{
    opRestoreSavedSettings();
    opOptionsEndHelper(linkName);
    cat_kbReloadKeyBindings(FALSE);         // CAT: opOptionsCancel 0x004d8370 / opInGameOptionsCancel 0x004d83a0 (HW1 kbRestoreSavedSettings)
}

void opOptionsCancel(char* name, featom* atom)
{
    opOptionsCancelHelper(name, atom, "Main_game_screen");
}

void opInGameOptionsCancel(char* name, featom* atom)
{
    opOptionsCancelHelper(name, atom, "Game_options");
    catOpTutorialGameplayDisable();
}

void opEqualizerToAudioAccept(char* name, featom* atom)
{
    if (opEqualizerToggled)
    {
        opSpeakerSetting = 3;
    }
    opOptionsSaveEqualizerSettings();
    feScreenDisappear(NULL, NULL);
    feScreenStart(ghMainRegion, "Audio_Options");
}

void opEqualizerToAudioCancel(char* name, featom* atom)
{
    opRestoreSavedEqualizerSettings();
    feScreenDisappear(NULL, NULL);
    feScreenStart(ghMainRegion, "Audio_Options");
}

void opEqualizerToInGameAudioAccept(char* name, featom* atom)
{
    if (opEqualizerToggled)
    {
        opSpeakerSetting = 3;
    }
    opOptionsSaveEqualizerSettings();
    feScreenDisappear(NULL, NULL);
    feScreenStart(ghMainRegion, "Audio");
}

void opEqualizerToInGameAudioCancel(char* name, featom* atom)
{
    opRestoreSavedEqualizerSettings();
    feScreenDisappear(NULL, NULL);
    feScreenStart(ghMainRegion, "Audio");
}

void opAdvancedToAudioAccept(char* name, featom* atom)
{
    speechEventCleanup();   // make sure we shut off and fade out and speech playing from the hear buttons
    opOptionsSaveAdvancedSettings();
    feScreenDisappear(NULL, NULL);
    feScreenStart(ghMainRegion, "Audio_Options");
}

void opAdvancedToAudioCancel(char* name, featom* atom)
{
    speechEventCleanup();   // make sure we shut off and fade out and speech playing from the hear buttons
    opRestoreAdvancedSettings();
    feScreenDisappear(NULL, NULL);
    feScreenStart(ghMainRegion, "Audio_Options");
}

void opAdvancedToInGameAudioAccept(char* name, featom* atom)
{
    speechEventCleanup();   // make sure we shut off and fade out and speech playing from the hear buttons
    opOptionsSaveAdvancedSettings();
    feScreenDisappear(NULL, NULL);
    feScreenStart(ghMainRegion, "Audio");
}

void opAdvancedToInGameAudioCancel(char* name, featom* atom)
{
    speechEventCleanup();   // make sure we shut off and fade out and speech playing from the hear buttons
    opRestoreAdvancedSettings();
    feScreenDisappear(NULL, NULL);
    feScreenStart(ghMainRegion, "Audio");
}

void opEffectsToVideoAccept(char* name, featom* atom)
{
    if (opCustomEffectsToggled)
    {
        opEffectsVal = 2;
    }
    opOptionsSaveCustomEffectsSettings();
    feScreenDisappear(NULL, NULL);
    feScreenStart(ghMainRegion, "Video_Options");
}

void opEffectsToVideoCancel(char* name, featom* atom)
{
    opRestoreSavedCustomEffectsSettings();
    feScreenDisappear(NULL, NULL);
    feScreenStart(ghMainRegion, "Video_Options");
}

void opEffectsToInGameVideoAccept(char* name, featom* atom)
{
    if (opCustomEffectsToggled)
    {
        opEffectsVal = 2;
    }
    opOptionsSaveCustomEffectsSettings();
    feScreenDisappear(NULL, NULL);
    feScreenStart(ghMainRegion, "Video");
}

void opEffectsToInGameVideoCancel(char* name, featom* atom)
{
    opRestoreSavedCustomEffectsSettings();
    feScreenDisappear(NULL, NULL);
    feScreenStart(ghMainRegion, "Video");
}

#if(0)
void VolumeSmoothieCallback(real32 data, Smoothie* smoo)
{
    sdword val = ((sliderhandle)volumeregion)->value = (sdword)volumesmoothie.buffer;
    bitSet(volumeregion->status, RSF_DrawThisFrame);

    soundEventMusicVol(normalizeSmoothie(val));
    opMusicVol = val;
}

void SFXSmoothieCallback(real32 data, Smoothie* smoo)
{
    sdword val = ((sliderhandle)sfxregion)->value = (sdword)sfxsmoothie.buffer;
    bitSet(sfxregion->status, RSF_DrawThisFrame);

    soundEventSFXVol(normalizeSmoothie(val));
    opSFXVol = val;
}

void SpeechSmoothieCallback(real32 data, Smoothie* smoo)
{
    sdword val = ((sliderhandle)speechregion)->value = (sdword)speechsmoothie.buffer;
    bitSet(speechregion->status, RSF_DrawThisFrame);

    soundEventSpeechVol(normalizeSmoothie(val));
    opSpeechVol = val;
}

void EqualizerSmoothieCallback(real32 data, Smoothie* smoo)
{
    sdword i;

    ((sliderhandle)(smoo->user))->value = (sdword)data;
    bitSet(((region*)(smoo->user))->status, RSF_DrawThisFrame);

    for (i = 0; i < NUM_EQ_BANDS; i++)
    {
        opEQReal[i] = 1.0f - ((real32)opEqualizerSettings[i] / 100.0f);
    }
}
#endif

/*
void soundEventSetActor(sdword actornum, bool32 bOn);
    actornum is 1, 2 or 3
    bOn is TRUE if checked FALSE if not
void soundEventVocalSettings(bool32 bCommandsOn, bool32 bStatusOn, bool32 bChatterOn);
 */

void opOptionsSaveEqualizerSettings(void)
{
    sdword i;

    opSaveSpeakerSetting = opSpeakerSetting;

    for (i = 0; i < NUM_EQ_BANDS; i++)
    {
        opSaveEqualizerSettings[i] = opEqualizerSettings[i];
        opEQReal[i] = 1.0f - ((real32)opEqualizerSettings[i] / 100.0f);
    }

    soundEventMasterEQ((real32*)opEQReal);
}

// CAT: the advanced audio page keeps the voice switches, the battle chatter, the sound quality and the speaker
//      setting with its equalizer (opAdvancedToAudioAccept 0x004d85e0 inline; no HW1 voice actors: no voice buttons)
void opOptionsSaveAdvancedSettings(void)
{
    opSaveVoiceComm = opVoiceComm;
    opSaveVoiceStat = opVoiceStat;
    opSaveVoiceChat = opVoiceChat;
    opSaveBattleChatter = opBattleChatter;
    opSaveSoundQuality = opSoundQuality;
    opOptionsSaveEqualizerSettings();                       //the speaker setting and the equalizer
    battleChatterFrequencySet(opBattleChatter);
}

// CAT: opAdvancedToAudioCancel 0x004d8690 inline: the voice switches, the sound quality, the speaker setting with its
//      equalizer, then the battle chatter
void opRestoreAdvancedSettings(void)
{
    opVoiceComm = opSaveVoiceComm;
    opVoiceStat = opSaveVoiceStat;
    opSoundQuality = opSaveSoundQuality;
    opVoiceChat = opSaveVoiceChat;
    opRestoreSavedEqualizerSettings();                      //the speaker setting and the equalizer
    opBattleChatter = opSaveBattleChatter;
    battleChatterFrequencySet(opBattleChatter);
}

// CAT: the custom effects' set also keeps the dynamic CPV, the lens flares, the repair droids and the LOD detail
//      (opOptionsSaveCustomEffectsSettings 0x004d8c40, opEffectsToVideoCancel 0x004d8990)
void opOptionsSaveCustomEffectsSettings(void)
{
    catOpSaveCPV = enableCPV;
    catOpSaveLensFlare = enableLensFlare;
    catOpSaveRepairbots = enableRepairbots;
    catOpSaveLodDetail = catLodDetail;
    opSaveEffectsVal = opEffectsVal;
    savetexLinearFiltering = texLinearFiltering;
    saveshowBackgrounds = showBackgrounds;
    savesmInstantTransition = smInstantTransition;
    saveenableSmoothing = enableSmoothing;
    saveenableStipple = enableStipple;
    saveenableTrails = enableTrails;
    savesmFuzzyBlobs = smFuzzyBlobs;

    saveHitEffects = (sdword)etgHitEffectsEnabled;
    saveDamagEffects = (sdword)etgDamageEffectsEnabled;
    saveMuzzleEffects = (sdword)etgFireEffectsEnabled;
    saveBulletEffects = (sdword)etgBulletEffectsEnabled;
}

void opOptionsSaveSettings(void)
{
    udword i;

    //copy keyboard options
    for (i = 0; KeyTranslations[i].sendkey > 0; i++)
    {
        TempKeyTranslations[i].key1 = KeyTranslations[i].key1;
        TempKeyTranslations[i].key2 = KeyTranslations[i].key2;
    }

    //copy audio options
    opSaveMusicVol = opMusicVol;
    opSaveSFXVol = opSFXVol;
    opSaveSpeechVol = opSpeechVol;
    opSaveNumChannels = opNumChannels;
    opSaveAutoChannel = opAutoChannel;
    opSaveSoundQuality = opSoundQuality;
//    opSaveSpeakerSetting = opSpeakerSetting;

    opOptionsSaveAdvancedSettings();

    opOptionsSaveEqualizerSettings();

    opUpdateAudioSettings();

    //video
    // CAT: opOptionsSaveSettings 0x004d8d00: the hardware T&L word, the frame-rate threshold as the slider set it (no
    //      auto-LOD read-back), the texture detail, the LOD detail and Cataclysm's toggles
    opSaveDetailThresholdVal = opDetailThresholdVal;
    catOpSaveTextureDetail = catOpTextureDetail;
    for (i = 0; catOpSavedWords[i] != NULL; i++)
    {
        catOpSavedValues[i] = *catOpSavedWords[i];
    }
    opBrightnessVal = shGammaGet();
    opSaveBrightnessVal = opBrightnessVal;
    opSaveNoLODVal = opNoLODVal;
    opSaveNumEffects = opNumEffects;

    opSaveMAIN_WindowWidth  = MAIN_WindowWidth;
    opSaveMAIN_WindowHeight = MAIN_WindowHeight;
    opSaveMAIN_WindowDepth  = MAIN_WindowDepth;

    opRenderListLoad();

    opSaveRndSelected = opRndSelected;

    opSaveMouseSens = opMouseSens;
    opSaveInfoOverlay = opInfoOverlayVar;

    opOptionsSaveCustomEffectsSettings();

    //gameplay

    opSaveAutodockHealthCheck = opAutodockHealthCheck;
    opSaveAutodockFuelCheck = opAutodockFuelCheck;
    opSaveAutodockHealthVal = opAutodockHealthVal;
    opSaveAutodockFuelVal = opAutodockFuelVal;

    cat_kbSaveKeyBindings();                                // CAT: keys.cfg written (opOptionsSaveSettings 0x004d8d00; HW1 kbSaveSettings)

    // save settings to .cfg file
    utyOptionsFileWrite();
    if (DebugWindow)
    {
        if (utyTest(SSA_DebugWindow))
        {
            /* dbw*() functions in other parts of code (main.c, utility.c, and
               Debug.c) will need to be uncommented if the debug window is
               reenabled. */
            /*dbwWriteOptions();*/
        }
    }
}

void opOptionsStartHelper(char* name, featom* atom, char* linkName)
{
    opOptionsSaveSettings();

/*    smoothmusicvol = (real32)opMusicVol;
    smoothsfxvol = (real32)opSFXVol;
    smoothspeechvol = (real32)opSpeechVol;

    InitSmoothie(&volumesmoothie,CONTROL_FEEDBACK,&smoothmusicvol,NULL,
                 &opMusicVol,&VolumeSmoothieCallback,1.1f);
    InitSmoothie(&sfxsmoothie,CONTROL_FEEDBACK, &smoothsfxvol,NULL,
        &opSFXVol, &SFXSmoothieCallback, 1.1f);
    InitSmoothie(&speechsmoothie, CONTROL_FEEDBACK, &smoothspeechvol, NULL,
        &opSpeechVol, &SpeechSmoothieCallback, 1.1f);

    smoothiesactive = TRUE;
    //in mousedraw
    //opSmoothiesBaby = taskCallBackRegister(opSmoothiesBabyFunction, 0, NULL, (real32)OP_SMOOTHIE_RATE);*/

    feScreenDisappear(NULL,NULL);

    feScreenStart(ghMainRegion, linkName);
}

void opOptionsStart(char* name, featom* atom)
{
    opOptionsStartHelper(name, atom, "Audio_Options");
}

// CAT: in the tutorial the in-game options keep their Gameplay link disabled (opInGameOptionsStart 0x004d9280,
//      opInGameOptionsAccept 0x004d7e50, opInGameOptionsCancel 0x004d83a0: RSF_RegionDisabled on the region of the
//      Game_options screen's "Game_options-TO-Gameplay" atom when tutorial == 1)
static void catOpTutorialGameplayDisable(void)
{
    fescreen *screen;
    featom *gameplay;

    if (tutorial == 1)
    {
        screen = feScreenFind("Game_options");
        if (screen != NULL)
        {
            gameplay = feAtomFindInScreen(screen, "Game_options-TO-Gameplay");
            if ((gameplay != NULL) && (gameplay->region != NULL))
            {
                bitSet(((regionhandle)gameplay->region)->status, RSF_RegionDisabled);
            }
        }
    }
}

void opInGameOptionsStart(char* name, featom* atom)
{
    opOptionsStartHelper(name, atom, "Game_options");
    catOpTutorialGameplayDisable();
}

void opOptionsEndHelper(char* linkName)

{
#if(0)
    RemoveSmoothie(&volumesmoothie);
    RemoveSmoothie(&sfxsmoothie);
    RemoveSmoothie(&speechsmoothie);
#endif

//    smoothiesactive = FALSE;

    feScreenDisappear(NULL,NULL);

    feScreenStart(ghMainRegion, linkName);
}

void opOptionsEnd(void)
{
    opOptionsEndHelper("Main_game_screen");
}

void opCancelKeyDetour(void)
{
    opKeyBeingDefined = -1;
    opKeyDetour = 0;
    feScreenDisappear(NULL, NULL);
}


void opKeyCancel(char* name, featom* atom)
{
    opCancelKeyDetour();
}


void opMusicVolume(char* name, featom* atom)
{
    udword f;
    sliderhandle shandle;

    if (FEFIRSTCALL(atom))
    {
       //feToggleButtonSet(name, texLinearFiltering);
       dbgMessage("opMusicVolume() first call");
//       volumeregion = atom->region;
#ifdef DEBUG_STOMP
       regVerify(((regionhandle)atom->region));
#endif
       shandle = (sliderhandle)atom->region;
       shandle->maxvalue = 100;
       shandle->value = opMusicVol;
//       smoothmusicvol = (real32)opMusicVol;

       f = regFilterSet(atom->region, 0);
       regFilterSet(atom->region, f | RPE_HoldLeft);

       shandle->processFunction  = (uicfunction) opMusicVolumeProcess;
       opMusicVolumeProcess(atom->region, 0, 0, 0);

//       shandle->processFunction  = opMusicVolumeProcess;

//       AddSmoothie(&volumesmoothie);
    }
    else // if (FELASTCALL(atom))
    {
        ;//RemoveSmoothie(&volumesmoothie);
    }
}

void opSFXVolume(char* name, featom* atom)
{
    udword f;
    sliderhandle shandle;

    if (FEFIRSTCALL(atom))
    {
        //feToggleButtonSet(name, texLinearFiltering);
        dbgMessage("opSFXVolume() first call");
//        sfxregion = atom->region;
#ifdef DEBUG_STOMP
        regVerify(((regionhandle)atom->region));
#endif
        shandle = (sliderhandle)atom->region;
        shandle->maxvalue = 100;
        shandle->value = opSFXVol;
//        smoothsfxvol = (real32)opSFXVol;

        f = regFilterSet(atom->region, 0);
        regFilterSet(atom->region, f | RPE_HoldLeft);

        shandle->processFunction  = (uicfunction) opSFXVolumeProcess;
        opSFXVolumeProcess(atom->region, 0, 0, 0);

//        AddSmoothie(&sfxsmoothie);
    }
    else //if (FELASTCALL(atom))
    {
        ;//RemoveSmoothie(&sfxsmoothie);
    }
}

void opSpeechVolume(char* name, featom* atom)
{
    udword f;
    sliderhandle shandle;

    if (FEFIRSTCALL(atom))
    {
        //feToggleButtonSet(name, texLinearFiltering);
        dbgMessage("opSpeechVolume() first call");
//        speechregion = atom->region;
#ifdef DEBUG_STOMP
        regVerify(((regionhandle)atom->region));
#endif
        shandle = (sliderhandle)atom->region;
        shandle->maxvalue = 100;
        shandle->value = opSpeechVol;
//        smoothspeechvol = (real32)opSpeechVol;

        f = regFilterSet(atom->region, 0);
        regFilterSet(atom->region, f | RPE_HoldLeft);

        shandle->processFunction  = (uicfunction) opSpeechVolumeProcess;

        opSpeechVolumeProcess(atom->region, 0, 0, 0);

//        AddSmoothie(&speechsmoothie);
    }
    else //if (FELASTCALL(atom))
    {
        ;//RemoveSmoothie(&speechsmoothie);
    }
}

/*void opBattleChatterCB(char* name, featom* atom)
{
    udword f;
    sliderhandle shandle;
    if (FEFIRSTCALL(atom))
    {
       //feToggleButtonSet(name, texLinearFiltering);
       //dbgMessage("opMusicVolume() first call");
       shandle = (sliderhandle)atom->region;
       shandle->maxvalue = 100;
       shandle->value = opBattleChatter;

       f = regFilterSet(atom->region, 0);
       regFilterSet(atom->region, f | RPE_HoldLeft);

       shandle->processFunction  = opBattleChatterProcess;
       opBattleChatterProcess(atom->region, 0, 0, 0);
    }
    else
    {
        ;
    }

}*/


void opNumberChannels(char* name, featom* atom)
{
    udword f;
    sdword min, max;
    sliderhandle shandle;

    if (FEFIRSTCALL(atom))
    {
        soundGetVoiceLimits(&min, &max);

        shandle = (sliderhandle)atom->region;
        shandle->maxvalue = max - min;
        shandle->value = opNumChannels;

        f = regFilterSet(atom->region, 0);
        regFilterSet(atom->region, f | RPE_HoldLeft);

        shandle->processFunction  = (uicfunction) opNumChannelsProcess;
        opNumChannelsProcess(atom->region, 0, 0, 0);
    }
    else
    {
        ;
    }
}

void opDrawNumChannels(featom *atom, regionhandle region)
{
    fonthandle currentFont;
    sdword min, max;

    if (FELASTCALL(atom))
    {
        drawnumchannels = NULL;
        return;
    }

    drawnumchannels = region;

    currentFont = fontMakeCurrent(opKeyboardFont);

    // CAT: opDrawNumChannels 0x004d9630: a slate background (0xff7a635a) and white digits (HW1: black, orange)
    primRectSolid2(&region->rect, colRGB(0x5a, 0x63, 0x7a));

    soundGetVoiceLimits(&min, &max);

    fontPrintf (region->rect.x0, region->rect.y0, colWhite, "%u", opNumChannels + min);

    fontMakeCurrent(currentFont);
}

void opAutoChannels(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, opAutoChannel);
    }
    else
    {
        opAutoChannel = FECHECKED(atom) ? SOUND_MODE_AUTO : SOUND_MODE_NORM;
    }
}

udword opMusicVolumeProcess(regionhandle reg, sdword ID, udword event, udword data)
{
    featom *atom = reg->atom;
    sliderhandle shandle = (sliderhandle)atom->region;

    opMusicVol = shandle->value;
    soundEventMusicMasterVolNOW(normalizeSmoothie(opMusicVol));

    return (0);
}

udword opSFXVolumeProcess(regionhandle reg, sdword ID, udword event, udword data)
{
    featom *atom = reg->atom;
    sliderhandle shandle = (sliderhandle)atom->region;

    opSFXVol = shandle->value;
    soundEventSFXMasterVol(normalizeSmoothie(opSFXVol));

    return (0);
}

udword opSpeechVolumeProcess(regionhandle reg, sdword ID, udword event, udword data)
{
    featom *atom = reg->atom;
    sliderhandle shandle = (sliderhandle)atom->region;

    opSpeechVol = shandle->value;
    soundEventSpeechMasterVol(normalizeSmoothie(opSpeechVol));

    return (0);
}

udword opNumChannelsProcess(regionhandle reg, sdword ID, udword event, udword data)
{
    featom *atom = reg->atom;
    sliderhandle shandle = (sliderhandle)atom->region;
    sdword min, max;

    opNumChannels = shandle->value;

  //  shandle->value = (sdword)nchannelssmoothie.buffer;

    if (drawnumchannels != NULL)
    {
#ifdef DEBUG_STOMP
        regVerify(((regionhandle)drawnumchannels));
#endif

        bitSet(drawnumchannels->status, RSF_DrawThisFrame);
    }

    soundGetVoiceLimits(&min, &max);
    soundSetNumVoices(opNumChannels + min, opAutoChannel);

    return (0);
}

udword opMouseSensitivityProcess(regionhandle reg, sdword ID, udword event, udword data)
{
    //dbgMessage(">");
    featom *atom = reg->atom;
    sliderhandle shandle = (sliderhandle)atom->region;

    opMouseSens = shandle->value;
    cameraSensitivitySet(opMouseSens);
    return (0);
}

udword opBattleChatterProcess(regionhandle reg, sdword ID, udword event, udword data)
{
    //dbgMessage(">");
    featom *atom = reg->atom;
    sliderhandle shandle = (sliderhandle)atom->region;

    opBattleChatter = shandle->value;
    battleChatterFrequencySet(opBattleChatter);
    return (0);
}

// CAT: opNumEffectsProcess 0x004d9900: moving the effects slider off the high (all) or low (none) preset switches
//      the OP_Effects radio to custom (2)
static void catOpNumEffectsCustomCheck(void)
{
    if (((opEffectsVal == 0) && (opNumEffects != 256 - etgHistoryScalarMin)) ||
        ((opEffectsVal == 1) && (opNumEffects != 0)))
    {
        opEffectsVal = 2;
        if (catOpEffectsRegion != NULL)
        {
            feRadioButtonSet("OP_Effects", 2);
            bitSet(catOpEffectsRegion->status, RSF_DrawThisFrame);
        }
    }
}

udword opNumEffectsProcess(regionhandle reg, sdword ID, udword event, udword data)
{
    //dbgMessage(">");
    featom *atom = reg->atom;
    sliderhandle shandle = (sliderhandle)atom->region;

    opNumEffects = shandle->value;
    catOpNumEffectsCustomCheck();
    etgHistoryScalar = opNumEffects + etgHistoryScalarMin;
    return (0);
}

// CAT: no slider processes for the CPU difficulty and attacks: they are radio buttons (opCPUDifficulty 0x004db400,
//      opCPUAttacks 0x004db440)

udword opEqualizerProcess(regionhandle reg, sdword ID, udword event, udword data)
{
    sdword val, i;
    featom* atom = reg->atom;
    real32 EQ[NUM_EQ_BANDS];
    sliderhandle shandle = (sliderhandle)atom->region;

    val = (sdword)((real32)shandle->value * 1.025f);

    opEqualizerSettings[shandle->ID] = val;
//    smootheqvalue[shandle->ID] = (real32)val;
//    shandle->value = (sdword)smootheqsmoothie[shandle->ID].buffer;

    opEqualizerToggled = TRUE;

    for (i = 0; i < NUM_EQ_BANDS; i++)
    {
        EQ[i] = 1.0f - ((real32)opEqualizerSettings[i] / 100.0f);
    }

    soundEventMasterEQ((real32*)EQ);

    return (0);
}

void opEQHelper(void)
{
    sdword i;

    if (opSpeakerSetting >= 0 &&
        opSpeakerSetting < 3)
    {
        for (i = 0; i < NUM_EQ_BANDS; i++)
        {
            opEqualizerSettings[i] = opSpeakerSettings[opSpeakerSetting][i];
        }
    }
    else
    {
        opSpeakerSetting = 3;
    }

    for (i = 0; i < NUM_EQ_BANDS; i++)
    {
        opSaveEqualizerSettings[i] = opEqualizerSettings[i];
        opEQReal[i] = 1.0f - ((real32)opEqualizerSettings[i] / 100.0f);
    }

    soundEventMasterEQ((real32*)opEQReal);
}

void opSpeaker(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feRadioButtonSet(name, opSpeakerSetting);
    }
    else
    {
        opSpeakerSetting = (sdword)atom->pData;
    }

    opEQHelper();
}

void opSoundQualitySet(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feRadioButtonSet(name, opSoundQuality);
    }
    else
    {
        opSoundQuality = (sdword)atom->pData;
    }

    soundMixerSetMode(opSoundQuality);
}

void opAdvancedButton(char* name, featom* atom)
{
}

void opEqualizerButton(char* name, featom* atom)
{
    feScreenStart(feStack[feStackIndex].baseRegion, "Equalizer"); //popup
}


// CAT: opDetailThresholdProcess 0x004d9b20: the slider runs 0..21 against the frame rate 30..9 (HW1: polygons 50..101)
#define CAT_OP_DetailFramerateTop   30
#define CAT_OP_DetailSliderMax      21
udword opDetailThresholdProcess(regionhandle reg, sdword ID, udword event, udword data)
{
    featom *atom = reg->atom;
    sliderhandle shandle = (sliderhandle)atom->region;
    opDetailThresholdVal = CAT_OP_DetailFramerateTop - shandle->value;
    cat_lodSetTimeThreshold(opDetailThresholdVal);
    return (0);
}


void opLighting(char* name, featom* atom)
{
    region *reg;
    if (FEFIRSTCALL(atom))
    {
        feRadioButtonSet(name, opLightingVal);
        reg = atom->region;
#ifdef DEBUG_STOMP
        regVerify(((regionhandle)reg));
#endif
        bitSet(reg->status, RSF_DrawThisFrame);
    }
    else
    {
        opLightingVal = (sdword)atom->pData;
    }
}

void opEffectsHelper(void)
{
    switch (opEffectsVal)
    {
    case 0: //hi
        // CAT: opEffectsHelper 0x004d9b90: the high preset also turns on the dynamic CPV, the lens flares, the repair
        //      droids and all four effect kinds; the low one turns the first three off with the hit and fire effects
        enableCPV = TRUE;
        enableLensFlare = TRUE;
        enableRepairbots = TRUE;
        etgHitEffectsEnabled = TRUE;
        etgDamageEffectsEnabled = TRUE;
        etgFireEffectsEnabled = TRUE;
        etgBulletEffectsEnabled = TRUE;
        texLinearFiltering = TRUE;
        showBackgrounds = TRUE;
        enableSmoothing = TRUE;
        enableStipple = FALSE;
        smFuzzyBlobs = TRUE;
        enableTrails = TRUE;
        smInstantTransition = FALSE;
        opNumEffects = 256 - etgHistoryScalarMin;
        if (EffectsSlider != NULL)
        {
#ifdef DEBUG_STOMP
            regVerify(((regionhandle)EffectsSlider));
#endif
            EffectsSlider->value = opNumEffects;
            bitSet(((regionhandle)EffectsSlider)->status, RSF_DrawThisFrame);
        }

        break;

    case 1: //lo
        enableCPV = FALSE;                                  // CAT: as above
        enableLensFlare = FALSE;
        enableRepairbots = FALSE;
        etgHitEffectsEnabled = FALSE;
        etgFireEffectsEnabled = FALSE;
        texLinearFiltering = FALSE;
        showBackgrounds = FALSE;
        enableSmoothing = TRUE;
        enableStipple = TRUE;
        smFuzzyBlobs = FALSE;
        enableTrails = TRUE;
        smInstantTransition = TRUE;
        opNumEffects = 0;
        if (EffectsSlider != NULL)
        {
#ifdef DEBUG_STOMP
            regVerify(((regionhandle)EffectsSlider));
#endif
            EffectsSlider->value = opNumEffects;
            bitSet(((regionhandle)EffectsSlider)->status, RSF_DrawThisFrame);
        }
        break;

    case 2: //custom
        break;

    default:
        opEffectsVal = 2;   //in case someone edits their config file
    }

    trFilterEnable(texLinearFiltering);
}

// CAT: no opUpdateVideoSettings: opUpdateSettings 0x004d71d0 inlines the effects, the LOD frame rate, the gamma and
//      the texture detail

void opEffects(char* name, featom* atom)
{
    region* reg;

    if (FEFIRSTCALL(atom))
    {
        catOpEffectsRegion = atom->region;                  // CAT: opEffects 0x004d9cc0 keeps the radio's region
        feRadioButtonSet(name, opEffectsVal);
        reg = atom->region;
#ifdef DEBUG_STOMP
        regVerify(((regionhandle)reg));
#endif
        bitSet(reg->status, RSF_DrawThisFrame);
    }
    else
    {
        if (FELASTCALL(atom))
        {
            catOpEffectsRegion = NULL;
        }
        opEffectsVal = (ubyte)(memsize)atom->pData;         // CAT: the exe reads pData's low byte
    }

    opEffectsHelper();
}

static sliderhandle detailSliderRegion = NULL;

void opDetailDisable(void)
{
    if (detailSliderRegion == NULL)
    {
        return;
    }

    if (opNoLODVal)
    {
        bitSet(detailSliderRegion->reg.status, RSF_RegionDisabled);
    }
    else
    {
        bitClear(detailSliderRegion->reg.status, RSF_RegionDisabled);
    }

#ifdef DEBUG_STOMP
    regVerify((regionhandle)&detailSliderRegion->reg);
#endif

    detailSliderRegion->reg.status |= RSF_DrawThisFrame;
}

void opNoLOD(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, opNoLODVal);
        opDetailDisable();
    }
    else
    {
        opNoLODVal = FECHECKED(atom) ? 1 : 0;
        opDetailDisable();
    }
}

void opDetailThreshold(char* name, featom* atom)
{
    udword f;
    sliderhandle shandle;
    if (FEFIRSTCALL(atom))
    {
        detailSliderRegion = (sliderhandle)atom->region;
        shandle = (sliderhandle)atom->region;
        shandle->maxvalue = CAT_OP_DetailSliderMax;         // CAT: opDetailThreshold 0x004da5e0 (HW1: 51, value - 50)
        shandle->value = CAT_OP_DetailFramerateTop - opDetailThresholdVal;

        f = regFilterSet(atom->region, 0);
        regFilterSet(atom->region, f | RPE_HoldLeft);

        shandle->processFunction  = (uicfunction) opDetailThresholdProcess;

        opDetailDisable();
    }
    else if (FELASTCALL(atom))
    {
        detailSliderRegion = NULL;
    }
}


udword opBrightnessProcess(regionhandle reg, sdword ID, udword event, udword data)
{
    featom *atom = reg->atom;
    sliderhandle shandle = (sliderhandle)atom->region;
    opBrightnessVal = shandle->value;
    shGammaSet(opBrightnessVal);
    return (0);
}

void opBrightness(char* name, featom* atom)
{
    udword f;
    sliderhandle shandle;
    if (FEFIRSTCALL(atom))
    {
        //feToggleButtonSet(name, texLinearFiltering);
        dbgMessage("opBrightness() first call");
        shandle = (sliderhandle)atom->region;
        shandle->maxvalue = 70;
        shandle->value = opBrightnessVal;

        f = regFilterSet(atom->region, 0);
        regFilterSet(atom->region, f | RPE_HoldLeft);

        shandle->processFunction = (uicfunction) opBrightnessProcess;
    }
    else
    {
        //dbgMessage("opSpeechVolume()");
    }
}

/*
void opEffects(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feRadioButtonSet(name, opEffectsVal);
    }
    else
    {
        opEffectsVal = (ubyte)atom->pData;
    }
}
*/

void opCustomEffects(char* name, featom* atom)
{
    feScreenStart(feStack[feStackIndex].baseRegion, "Effects"); //popup
}

/*-----------------------------------------------------------------------------
    Name        : opResItemDraw
    Description : draws a single Resolution listwindow item
    Inputs      : rect - region boundary
                  data - listitem userdata
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void opResItemDraw(rectangle* rect, listitemhandle data)
{
    char temp[64];
    sdword x, y;
    color c;
    fonthandle oldfont;
    opres* res = (opres*)data->data;

    oldfont = fontMakeCurrent(opResListFont);

    if (bitTest(data->flags, UICLI_Selected))
    {
        opSaveMAIN_WindowWidth  = res->width;
        opSaveMAIN_WindowHeight = res->height;
        opSaveMAIN_WindowDepth  = res->depth;

        c = FEC_ListItemSelected;
    }
    else
    {
        c = FEC_ListItemStandard;
    }

    if (!opResResSupported(res))
    {
        if (bitTest(data->flags, UICLI_Selected))
        {
            c = colRGB(0,255,0);
        }
        else
        {
            c = colRGB(127,127,127);
        }
    }

    x = rect->x0 + ((rect->x1 - rect->x0) / 2) + 9;
    y = rect->y0;

    sprintf(temp, "%dx%d", res->width, res->height);
    fontPrint(x - fontWidth(temp), y, c, temp);

    sprintf(temp, "%d bit", res->depth);
    fontPrint(x + fontWidth("I"), y, c, temp);

    fontMakeCurrent(oldfont);
}

/*-----------------------------------------------------------------------------
    Name        : opResListSet
    Description : sets the available resolutions based on selected rendering system
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void opResListSet(rdevice* dev)
{
    rmode* mode;
    opres* res;
    sdword nModes, index;
    fonthandle oldfont;

    mode = dev->modes;
	
    nModes = 0;
    while (mode != NULL)
    {
        nModes++;
        mode = mode->next;
    }
	
    opResNumber = nModes;
    opResCurrentSelected = 0;

    if (opRes != NULL)
    {
        memFree(opRes);
    }
    opRes = memAlloc(nModes * sizeof(opres), "res list", NonVolatile);

    mode = dev->modes;
    res = opRes;
    while (mode != NULL)
    {
        res->width  = mode->width;
        res->height = mode->height;
        res->depth  = mode->depth;
        res++;
        mode = mode->next;
    }

    for (index = 0; index < opResNumber; index++)
    {
        if (opSaveMAIN_WindowWidth  == opRes[index].width &&
            opSaveMAIN_WindowHeight == opRes[index].height &&
            opSaveMAIN_WindowDepth  == opRes[index].depth)
        {
            opResCurrentSelected = index;
            break;
        }
    }

    oldfont = fontMakeCurrent(opResListFont);

    uicListRemoveAllItems(opResListWindow);

    for (index = 0; index < opResNumber; index++)
    {
        udword flags = UICLI_CanSelect;

        if (index == opResCurrentSelected)
        {
            bitSet(flags, UICLI_Selected);
        }

        uicListAddItem(opResListWindow,
                       (ubyte*)&opRes[index],
                       flags, UICLW_AddToTail);
    }

    fontMakeCurrent(oldfont);

    opDirtyResListWindow();
}

/*-----------------------------------------------------------------------------
    Name        : opResListLoad
    Description : sets up opRes[], the list of possible resolutions
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void opResListLoad(void)
{
    //rswitch
    opResNumber = 0;
    opResCurrentSelected = 0;

    if (opRes != NULL)
    {
        memFree(opRes);
        opRes = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : opResolution
    Description : Resolution listwindow callback
    Inputs      : name, atom
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void opResolution(char* name, featom* atom)
{
    fonthandle oldfont;

    if (opResListFont == 0)
    {
        opResListFont = frFontRegister(OP_ResolutionFont);
    }

    if (FEFIRSTCALL(atom))
    {
        opResListLoad();

        oldfont = fontMakeCurrent(opResListFont);

        opResListRegion = (regionhandle)atom->region;
        opResListWindow = (listwindowhandle)atom->pData;

        uicListWindowInit(opResListWindow,
                          NULL,
                          NULL,
                          0,
                          opResItemDraw,
                          fontHeight(" ") + (fontHeight(" ") >> 1),
                          UICLW_CanSelect);

        fontMakeCurrent(oldfont);

        return;
    }
    else if (FELASTCALL(atom))
    {
        opResListWindow = NULL;
        opResListRegion = NULL;

        return;
    }
}

void opSqueezeString(char* buf, rectangle* rect, sdword offset)
{
    sdword width;

    width = rect->x1 - rect->x0;
    width -= offset;

    while (fontWidth(buf) > width)
    {
        if (strlen(buf) < 2)
        {
            return;
        }
        buf[strlen(buf) - 1] = '\0';
    }
}

/*-----------------------------------------------------------------------------
    Name        : opRenderItemDraw
    Description : draws a single Render listwindow item
    Inputs      : rect - region boundary
                  data - listitem userdata
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void opRenderItemDraw(rectangle* rect, listitemhandle data)
{
    char temp[128];
    sdword x, y;
    color c0, c1;
    fonthandle oldfont;
    oprender* rnd = (oprender*)data->data;

    oldfont = fontMakeCurrent(opRenderListFont);

    c0 = c1 = FEC_ListItemSelected;
    if (opRndSelected != rnd || opRes == NULL)
    {
        opRndSelected = rnd;
        opResListSet(rnd->dev);
        opSaveRndSelected = rnd;
    }
    else
    {
        c0 = FEC_ListItemStandard;
        c1 = FEC_ListItemStandard;
    }

    x = rect->x0;
    y = rect->y0;

    sprintf(temp, "%s", rnd->name);
    opSqueezeString(temp, rect, x - rect->x0);
    fontPrint(x + fontWidth(" "), y, c1, temp);

    fontMakeCurrent(oldfont);
}

/*-----------------------------------------------------------------------------
    Name        : opRenderListLoad
    Description : sets up opRnd[], the list of possible renderers
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void opRenderListLoad(void)
{
    //rswitch
    sdword nDevices;
    rdevice* dev;

    opRenderNumber = nDevices = 0;
    dev = rinGetDeviceList();
    while (dev != NULL)
    {
        nDevices++;
        dev = dev->next;
    }

    if (opRnd != NULL)
    {
        memFree(opRnd);
    }
    opRnd = (oprender*)memAlloc(nDevices * sizeof(oprender), "opRnd device list", NonVolatile);

    dev = rinGetDeviceList();
    while (dev != NULL)
    {
        opRnd[opRenderNumber].dev = dev;
        opRnd[opRenderNumber].type = dev->type;
        memStrncpy(opRnd[opRenderNumber].data, dev->data, 63);
        memStrncpy(opRnd[opRenderNumber].name, dev->name, 63);

        opRenderCurrentSelected = opRenderNumber;
        opRndSelected = &opRnd[opRenderNumber];

        //increment device count
        opRenderNumber++;

        //next device
        dev = dev->next;
    }

    if (opDeviceIndex != -1)
    {
        opRenderCurrentSelected = opDeviceIndex;
        opRndSelected = &opRnd[opDeviceIndex];
    }
}

/*-----------------------------------------------------------------------------
    Name        : opRender
    Description : Render listwindow callback
    Inputs      : name, atom
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void opRender(char* name, featom* atom)
{
    fescreen* screen;
    featom* npatom;
    fonthandle oldfont;
    sdword index;

    if (opRenderListFont == 0)
    {
        opRenderListFont = frFontRegister(OP_RenderFont);
    }

    if (FEFIRSTCALL(atom))
    {
        opRenderListLoad();

        oldfont = fontMakeCurrent(opRenderListFont);

        opRenderListWindow = (listwindowhandle)atom->pData;

        uicListWindowInit(opRenderListWindow,
                          NULL,
                          NULL,
                          0,
                          opRenderItemDraw,
                          fontHeight(" ") + (fontHeight(" ") >> 1),
                          UICLW_CanSelect);

        for (index = 0; index < opRenderNumber; index++)
        {
            udword flags = UICLI_CanSelect;

            if (opSaveRndSelected == &opRnd[index])
            {
                bitSet(flags, UICLI_Selected);
            }

            uicListAddItem(opRenderListWindow,
                           (ubyte*)&opRnd[index],
                           flags, UICLW_AddToTail);
        }

        // CAT: no no-palette pool display (opRender 0x004dada0: Cataclysm's video options have the texture detail)
        (void)screen; (void)npatom;

        fontMakeCurrent(oldfont);
    }
    else if (FELASTCALL(atom))
    {
        opRenderListWindow = NULL;
    }
}

void EqualizerCommon(char* name, featom* atom, sdword ID)
{
    udword f;
    sliderhandle shandle;

    if (FEFIRSTCALL(atom))
    {
        shandle = (sliderhandle)atom->region;
        shandle->maxvalue = 100;
        shandle->ID = ID;
        shandle->value = opEqualizerSettings[shandle->ID];

        f = regFilterSet(atom->region, 0);
        regFilterSet(atom->region, f | RPE_HoldLeft);

        shandle->processFunction = (uicfunction) opEqualizerProcess;

/*        //smoothies
        smootheqvalue[ID] = (real32)opEqualizerSettings[shandle->ID];

        smootheqsmoothie[ID].feedback = CONTROL_FEEDBACK;
        smootheqsmoothie[ID].source = &smootheqvalue[ID];
        smootheqsmoothie[ID].dest = NULL;
        smootheqsmoothie[ID].destint = &opEqualizerSettings[shandle->ID];
        smootheqsmoothie[ID].callback = &EqualizerSmoothieCallback;
        smootheqsmoothie[ID].threshold = 1.0f;
        smootheqsmoothie[ID].user = (sdword*)shandle;

        AddSmoothie(&smootheqsmoothie[ID]);*/
    }
}

void opEqualizer1(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        opEqualizerToggled = FALSE;
        EqualizerCommon(name, atom, 0);
    }
    else // if (FELASTCALL(atom))
    {
        ; //RemoveSmoothie(&smootheqsmoothie[0]);
    }
}

void opEqualizer2(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        EqualizerCommon(name, atom,1);
    }
    else // if (FELASTCALL(atom))
    {
        ; //RemoveSmoothie(&smootheqsmoothie[1]);
    }
}

void opEqualizer3(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        EqualizerCommon(name, atom,2);
    }
    else // if (FELASTCALL(atom))
    {
        ; //RemoveSmoothie(&smootheqsmoothie[2]);
    }
}

void opEqualizer4(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        EqualizerCommon(name, atom,3);
    }
    else // if (FELASTCALL(atom))
    {
        ; //RemoveSmoothie(&smootheqsmoothie[3]);
    }
}

void opEqualizer5(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        EqualizerCommon(name, atom,4);
    }
    else // if (FELASTCALL(atom))
    {
        ; //RemoveSmoothie(&smootheqsmoothie[4]);
    }
}

void opEqualizer6(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        EqualizerCommon(name, atom,5);
    }
    else // if (FELASTCALL(atom))
    {
        ; //RemoveSmoothie(&smootheqsmoothie[5]);
    }
}

void opEqualizer7(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        EqualizerCommon(name, atom,6);
    }
    else // if (FELASTCALL(atom))
    {
        ; //RemoveSmoothie(&smootheqsmoothie[6]);
    }
}

void opEqualizer8(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        EqualizerCommon(name, atom,7);
    }
    else // if (FELASTCALL(atom))
    {
        ; //RemoveSmoothie(&smootheqsmoothie[7]);
    }
}

// CAT: no opVoice / opVoice0..2 / opHearVoice1..3: Cataclysm's audio options have no voice actors and no voice
//      preview (the exe's callback table has no OP_Voice* / OP_Hear_Voice_* rows, and no such functions)

void opVoiceCommands(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, opVoiceComm);
        soundEventVocalSettings(opVoiceComm, opVoiceStat, opVoiceChat);
    }
    else
    {
        opVoiceComm = FECHECKED(atom) ? 1 : 0;
        soundEventVocalSettings(opVoiceComm, opVoiceStat, opVoiceChat);
    }
}

void opVoiceStatus(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, opVoiceStat);
        soundEventVocalSettings(opVoiceComm, opVoiceStat, opVoiceChat);
    }
    else
    {
        opVoiceStat = FECHECKED(atom) ? 1 : 0;
        soundEventVocalSettings(opVoiceComm, opVoiceStat, opVoiceChat);
    }
}

void opVoiceChatter(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, opVoiceChat);
        soundEventVocalSettings(opVoiceComm, opVoiceStat, opVoiceChat);
    }
    else
    {
        opVoiceChat = FECHECKED(atom) ? 1 :0;
        soundEventVocalSettings(opVoiceComm, opVoiceStat, opVoiceChat);
    }
}

void opMouseSensitivity(char* name, featom* atom)
{
    udword f;
    sliderhandle shandle;
    if (FEFIRSTCALL(atom))
    {
       //feToggleButtonSet(name, texLinearFiltering);
       //dbgMessage("opMusicVolume() first call");
       shandle = (sliderhandle)atom->region;
       shandle->maxvalue = 100;
       shandle->value = opMouseSens;

       f = regFilterSet(atom->region, 0);
       regFilterSet(atom->region, f | RPE_HoldLeft);

       shandle->processFunction  = (uicfunction) opMouseSensitivityProcess;

    }
    else
    {
        ;
    }

}

void opBattleChatterCB(char* name, featom* atom)
{
    udword f;
    sliderhandle shandle;
    if (FEFIRSTCALL(atom))
    {
       //feToggleButtonSet(name, texLinearFiltering);
       //dbgMessage("opMusicVolume() first call");
       shandle = (sliderhandle)atom->region;
       shandle->maxvalue = 100;
       shandle->value = opBattleChatter;

       f = regFilterSet(atom->region, 0);
       regFilterSet(atom->region, f | RPE_HoldLeft);

       shandle->processFunction  = (uicfunction) opBattleChatterProcess;
       opBattleChatterProcess(atom->region, 0, 0, 0);
    }
    else
    {
        ;
    }
}

void opNumberEffects(char* name, featom* atom)
{
    udword f;
    sliderhandle shandle;
    if (FEFIRSTCALL(atom))
    {
       //feToggleButtonSet(name, texLinearFiltering);
       //dbgMessage("opMusicVolume() first call");
       EffectsSlider = shandle = (sliderhandle)atom->region;
       shandle->maxvalue = 256 - etgHistoryScalarMin;
       shandle->value = opNumEffects;

       f = regFilterSet(atom->region, 0);
       regFilterSet(atom->region, f | RPE_HoldLeft);

       shandle->processFunction  = (uicfunction) opNumEffectsProcess;
       opNumEffectsProcess(atom->region, 0, 0, 0);          // CAT: opNumberEffects 0x004db300 inlines it (with the custom switch)
    }
    else if (FELASTCALL(atom))
    {
       EffectsSlider = NULL;
    }
}

// CAT: opCPUDifficulty 0x004db400 (REWRITTEN): a radio button set on tpGameCreated.aiplayerDifficultyLevel
//      (HW1: a 0..100 slider)
void opCPUDifficulty(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feRadioButtonSet(name, tpGameCreated.aiplayerDifficultyLevel);
        bitSet(((regionhandle)atom->region)->status, RSF_DrawThisFrame);
        return;
    }
    tpGameCreated.aiplayerDifficultyLevel = (ubyte)(memsize)atom->pData;
}

// CAT: opCPUAttacks 0x004db440 (REWRITTEN): a radio button set on tpGameCreated.aiplayerBigotry (HW1: a slider)
void opCPUAttacks(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feRadioButtonSet(name, tpGameCreated.aiplayerBigotry);
        bitSet(((regionhandle)atom->region)->status, RSF_DrawThisFrame);
        return;
    }
    tpGameCreated.aiplayerBigotry = (ubyte)(memsize)atom->pData;
}

/*-----------------------------------------------------------------------------
    Name        : opInfoOverlay
    Description : Processes toggling of Info overlay
    Inputs      : void
    Outputs     :
    Return      : Nothing
----------------------------------------------------------------------------*/
void opInfoOverlay(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        if (opInfoOverlayVar)
        {
            bitSet(atom->status, FAS_Checked);
        }
        else
        {
            bitClear(atom->status, FAS_Checked);
        }
        return;
    }

    if (bitTest(atom->status, FAS_Checked))
    {
        opInfoOverlayVar = 1;
    }
    else
    {
        opInfoOverlayVar = 0;
    }
    // CAT: opInfoOverlay 0x004db480 only sets the word (no ioEnable / ioDisable; nothing in the exe reads it beyond the
    //      options' save, restore and file)
}

void opTaskbarUp(char* name, featom* atom)
{

}

void opIncreaseChatting(char* name, featom* atom)
{

}

void opDecreaseChatting(char* name, featom* atom)
{

}

void opNumberChatlines(char* name, featom* atom)
{

}

void opAutodockHealth(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, opAutodockHealthCheck);
    }
    else
    {
        opAutodockHealthCheck = FECHECKED(atom);
    }

}

void opAutodockFuel(char* name, featom* atom)
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, opAutodockFuelCheck);
    }
    else
    {
        opAutodockFuelCheck = FECHECKED(atom);
    }

}


void opAutodockHealthValue(char* name, featom* atom)
{

    char    temp[20];

    if (FEFIRSTCALL(atom))
    {
        // initialize button here
        opAutodockHealthEntryBox = (textentryhandle)atom->pData;
        sprintf(temp,"%d",opAutodockHealthVal);
        uicTextEntrySet(opAutodockHealthEntryBox,temp,strlen(temp)+1);
        uicTextBufferResize(opAutodockHealthEntryBox,MAX_NUM_LENGTH-2);
        uicTextEntryInit(opAutodockHealthEntryBox,UICTE_NumberEntry);
        return;
    }

    switch (uicTextEntryMessage(atom))
    {
        case CM_LoseFocus :
        case CM_AcceptText :
            sscanf(opAutodockHealthEntryBox->textBuffer,"%d",&opAutodockHealthVal);
            if (uicTextEntryMessage(atom)==CM_AcceptText) feToggleButtonSet("OP_Autodock_Health", TRUE);
        break;
        case CM_GainFocus :
        break;
    }
}

void opAutodockFuelValue(char* name, featom* atom)

{

    char    temp[20];

    if (FEFIRSTCALL(atom))
    {
        // initialize button here
        opAutodockFuelEntryBox = (textentryhandle)atom->pData;
        sprintf(temp,"%d",opAutodockFuelVal);
        uicTextEntrySet(opAutodockFuelEntryBox,temp,strlen(temp)+1);
        uicTextBufferResize(opAutodockFuelEntryBox,MAX_NUM_LENGTH-2);
        uicTextEntryInit(opAutodockFuelEntryBox,UICTE_NumberEntry);
        return;
    }

    switch (uicTextEntryMessage(atom))
    {
        case CM_LoseFocus :
        case CM_AcceptText :
            sscanf(opAutodockFuelEntryBox->textBuffer,"%d",&opAutodockFuelVal);
            if (uicTextEntryMessage(atom)==CM_AcceptText) feToggleButtonSet("OP_Autodock_Fuel", TRUE);
        break;
        case CM_GainFocus :
        break;
    }

}




// Smoothies sucked some serious ass so bye bye!!

#if(0)
void InitSmoothie(Smoothie *smoothie,real32 feedback, real32 *source,
    real32 *dest, sdword *destint, smoothieFunc callback, real32 threshold)
{
    smoothie->feedback = feedback;
    smoothie->source = source;
    smoothie->dest = dest;
    smoothie->destint = destint;
    smoothie->callback = callback;
    smoothie->threshold = threshold;
}




void AddSmoothie(Smoothie *smoo)
{
    udword i;
    for (i=0; i<NUM_SMOOTHIES; i++)
    {
        if (!SmoothieArray[i])
        {
            SmoothieArray[i] = smoo;
            SmoothieArray[i]->buffer = *(SmoothieArray[i]->source);
            return;
        }
    }
    dbgFatal(DBG_Loc, "Out of smoothies!!!");
}

void RemoveSmoothie(Smoothie *smoo)
{
    udword i;
    for (i=0; i<NUM_SMOOTHIES; i++)
    {
        if (SmoothieArray[i] == smoo)
        {
            SmoothieArray[i] = NULL;
            return;
        }
    }
    dbgFatal(DBG_Loc, "Smoothie couldn't be removed!!!");
}


void DoSmoothies(void)
{
    Smoothie* smoo;
    sdword i;

    for (i = 0; i < NUM_SMOOTHIES; i++)
    {
        if (SmoothieArray[i])
        {
            smoo = SmoothieArray[i];

            if (ABS(*(smoo->source)) == smoo->buffer)
            {
                ; //nothing
            }
            else if (ABS(*(smoo->source) - smoo->buffer) < smoo->threshold)
            {
                //make same and do last update
                *(smoo->source) = smoo->buffer;
                if (smoo->dest)
                {
                    *(smoo->dest) = smoo->buffer;
                }
                if (smoo->destint)
                {
                    *(smoo->destint) = (sdword)smoo->buffer;
                }
                if (smoo->callback)
                {
                    (smoo->callback)(smoo->buffer, smoo);
                }
            }
            else
            {
                //smooth and update
                smoo->buffer = smoo->feedback * smoo->buffer +
                               (1.0f - smoo->feedback) * (*(smoo->source));

                if (smoo->dest)
                {
                    *(smoo->dest) = smoo->buffer;
                }
                if (smoo->destint)
                {
                    *(smoo->destint) = (sdword)smoo->buffer;
                }
                if (smoo->callback)
                {
                    (smoo->callback)(smoo->buffer, smoo);
                }
            }
        }
    }
}


/*-----------------------------------------------------------------------------
    Name        : opHighlightTaskFunction
    Description : Task function for higlighting
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 opSmoothiesBabyFunction(udword num, void* data, struct BabyCallBack* baby)
{
    DoSmoothies();
    return !smoothiesactive;
}

#endif

void opNoPalDraw(featom* atom, regionhandle region);

void opNoPalDirty(void)
{
    if (opNoPalDrawRegion != NULL)
    {
#ifdef DEBUG_STOMP
        regVerify(opNoPalDrawRegion);
#endif
        opNoPalDrawRegion->status |= RSF_DrawThisFrame;
        opNoPalDraw(NULL, opNoPalDrawRegion);
    }
}

udword opNoPalIncreaseProcess(regionhandle region, sdword ID, udword event, udword data)
{
#ifdef DEBUG_STOMP
    regVerify(region);
#endif

    if (event == RPE_ReleaseLeft)
    {
        opNoPalMB += 4;
        if (opNoPalMB > opNoPalMaxMB)
        {
            opNoPalMB = opNoPalMaxMB;
        }
        trNoPalResizePool(opNoPalMB);
        region->status |= RSF_DrawThisFrame;
        opNoPalDirty();
        return 0;
    }
    region->status |= RSF_DrawThisFrame;
    return 0;
}

udword opNoPalDecreaseProcess(regionhandle region, sdword ID, udword event, udword data)
{
#ifdef DEBUG_STOMP
    regVerify(region);
#endif

    if (event == RPE_ReleaseLeft)
    {
        opNoPalMB -= 4;
        if (opNoPalMB < opNoPalMinMB)
        {
            opNoPalMB = opNoPalMinMB;
        }
        trNoPalResizePool(opNoPalMB);
        region->status |= RSF_DrawThisFrame;
        opNoPalDirty();
        return 0;
    }
    region->status |= RSF_DrawThisFrame;
    return 0;
}

void opNoPalDecrease(featom* atom, regionhandle region)
{
    if (region->flags == 0 || region->flags == RPE_DrawFunctionAdded)
    {
        region->flags = RPE_PressLeft | RPE_ReleaseLeft | RPE_Enter | RPE_Exit;
        if (!bitTest(atom->flags, FAF_Disabled))
        {
            regFunctionSet(region, (regionfunction) opNoPalDecreaseProcess);
        }
    }
    mgDrawArrow(region, TRUE, FALSE);
}

void opNoPalIncrease(featom* atom, regionhandle region)
{
    if (region->flags == 0 || region->flags == RPE_DrawFunctionAdded)
    {
        region->flags = RPE_PressLeft | RPE_ReleaseLeft | RPE_Enter | RPE_Exit;
        if (!bitTest(atom->flags, FAF_Disabled))
        {
            regFunctionSet(region, (regionfunction) opNoPalIncreaseProcess);
        }
    }
    mgDrawArrow(region, FALSE, FALSE);
}

void opBlackRect(sdword x, sdword y, char* str)
{
    rectangle rect;

    rect.x0 = x;
    rect.y0 = y;
    rect.x1 = x + fontWidth(str);
    rect.y1 = y + fontHeight(str);
    primRectSolid2(&rect, colBlack);
}

void opNoPalDraw(featom* atom, regionhandle region)
{
    fonthandle oldfont;
    char buf[16];
    color c;
    extern fonthandle ghDefaultFont;

    primRectSolid2(&region->rect, colBlack);
    oldfont = fontMakeCurrent(ghDefaultFont);

    sprintf(buf, "%dMB", opNoPalMB);
    c = colRGB(255,200,0);
    opBlackRect(region->rect.x0 - 5,
                region->rect.y0, buf);
    fontPrint(region->rect.x0 - 5, region->rect.y0, c, buf);
    fontMakeCurrent(oldfont);
}

void opTroubleShoot(char* name, featom* atom)
{
    // CAT: opTroubleShoot 0x004db6c0 opens the language script's strTroubleShootPage (MessageStrings[1700], the
    //      install's http://homeworld.sierra.com/cataclysm/support/), not HW1's built-in address
    utyBrowserExec(strGetString(strTroubleShootPage));
}

/*=============================================================================
    CAT (M2-R6 frontend): Cataclysm's options callbacks (Options.c 0x004d9d20 .. 0x004dbb30), registered by
    utility.c's utyCallbacks with the exe's names (its table 0x008c1360 .. 0x008c1438). A toggle's first call shows the
    word, a click stores FECHECKED; a radio set's first call checks the stored value, a click stores the button's
    pData byte.
=============================================================================*/
#define CAT_OP_TOGGLE(fn, addr, word)                                   \
void fn(char *name, featom *atom)                   /* addr */          \
{                                                                       \
    if (FEFIRSTCALL(atom))                                              \
    {                                                                   \
        feToggleButtonSet(name, (word));                                \
        return;                                                         \
    }                                                                   \
    (word) = FECHECKED(atom) ? 1 : 0;                                   \
}

void cat_opLODDetail(char *name, featom *atom)      // 0x004d9d20: the first LOD level drawn (OP_LODDetail radio)
{
    if (FEFIRSTCALL(atom))
    {
        feRadioButtonSet(name, catLodDetail);
        bitSet(((regionhandle)atom->region)->status, RSF_DrawThisFrame);
        return;
    }
    catLodDetail = (ubyte)(memsize)atom->pData;
}

void cat_opSmallFonts(char *name, featom *atom)     // 0x004d9df0: OP_SmallFonts shows the popup menu's large fonts off
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, catPmLargeFont == FALSE);
        return;
    }
    catPmLargeFont = FECHECKED(atom) ? FALSE : TRUE;
}

CAT_OP_TOGGLE(cat_opHWTaskbar,          0x004d9e30, catMoHWTaskbar)
CAT_OP_TOGGLE(cat_opTaskbarButtons,     0x004d9e70, catMoTaskbarButtons)
CAT_OP_TOGGLE(cat_opTaskbarBackdrop,    0x004d9eb0, catMoTaskbarBackdrop)
CAT_OP_TOGGLE(cat_opExperienceMessages, 0x004d9ef0, catOpExperienceMessages)

void cat_opHomeworldCSA(char *name, featom *atom)   // 0x004d9f30: the HW1 context-sensitive actions, exclusive with
{                                                   //             the right-click ones
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, catMrHomeworldCSA);
        return;
    }
    catMrHomeworldCSA = FECHECKED(atom) ? 1 : 0;
    catMrRightClickCSA = (catMrHomeworldCSA == 0);
    feToggleButtonSet("OP_RightClickCSA", catMrRightClickCSA);
}

void cat_opRightClickCSA(char *name, featom *atom)  // 0x004d9f80
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, catMrRightClickCSA);
        return;
    }
    catMrRightClickCSA = FECHECKED(atom) ? 1 : 0;
    catMrHomeworldCSA = (catMrRightClickCSA == 0);
    feToggleButtonSet("OP_HomeworldCSA", catMrHomeworldCSA);
}

CAT_OP_TOGGLE(cat_opAutoFocusOnWaypoints, 0x004d9fd0, catMrWaypointCentre)
CAT_OP_TOGGLE(cat_opAreaExplored,       0x004da010, catSmViewAreasVisited)
CAT_OP_TOGGLE(cat_opSensorsSphere,      0x004da050, catSmViewFOWRanges)
CAT_OP_TOGGLE(cat_opCompass,            0x004da090, catSmViewCompass)
CAT_OP_TOGGLE(cat_opSensorsWireframe,   0x004da0d0, catSmViewAreasOccupied)

void cat_opResources(char *name, featom *atom)      // 0x004da110: the resources and the meteors together
{
    if (FEFIRSTCALL(atom))
    {
        feToggleButtonSet(name, (catSmViewResources && catSmViewMeteors) ? 1 : 0);
        return;
    }
    catSmViewResources = FECHECKED(atom) ? 1 : 0;
    catSmViewMeteors = FECHECKED(atom) ? 1 : 0;
}

CAT_OP_TOGGLE(cat_opDust,               0x004da180, catSmViewDecorativeAsteroids)
CAT_OP_TOGGLE(cat_opBackground,         0x004da1c0, catSmViewBackground)
CAT_OP_TOGGLE(cat_opObjectivePointers,  0x004da200, catSmViewObjectivePointers)
CAT_OP_TOGGLE(cat_opSlipgatePaths,      0x004da240, catSmViewSlipgatePaths)
CAT_OP_TOGGLE(cat_opCapitalShipOverlay, 0x004da280, catSmViewIcons)

// the sensors page's buttons from the words (the tail of cat_opHomeworldClassic, cat_opCataclysmDefault and
// cat_opSetAllDefaults)
static void catOpSensorsButtonsSet(void)
{
    feToggleButtonSet("OP_AreaExplored", catSmViewAreasVisited);
    feToggleButtonSet("OP_SensorsSphere", catSmViewFOWRanges);
    feToggleButtonSet("OP_SensorsWireframe", catSmViewAreasOccupied);
    feToggleButtonSet("OP_Resources", (catSmViewResources && catSmViewMeteors) ? 1 : 0);
    feToggleButtonSet("OP_Dust", catSmViewDecorativeAsteroids);
    feToggleButtonSet("OP_Background", catSmViewBackground);
    feToggleButtonSet("OP_Compass", catSmViewCompass);
    feToggleButtonSet("OP_ObjectivePointers", catSmViewObjectivePointers);
    feToggleButtonSet("OP_SlipgatePaths", catSmViewSlipgatePaths);
    feToggleButtonSet("OP_CapitalShipOverlay", catSmViewIcons);
}

void cat_opHomeworldClassic(char *name, featom *atom)   // 0x004da2c0: the HW1-like interface preset
{
    catMoHWTaskbar = TRUE;
    catMoTaskbarButtons = TRUE;
    catMoTaskbarBackdrop = TRUE;
    catOpExperienceMessages = 0;
    catMrHomeworldCSA = TRUE;
    catMrRightClickCSA = FALSE;
    feToggleButtonSet("OP_HWTaskbar", 1);
    feToggleButtonSet("OP_TaskbarButtons", catMoTaskbarButtons);
    feToggleButtonSet("OP_TaskbarBackdrop", catMoTaskbarBackdrop);
    feToggleButtonSet("OP_ExperienceMessages", catOpExperienceMessages);
    feToggleButtonSet("OP_HomeworldCSA", catMrHomeworldCSA);
    feToggleButtonSet("OP_RightClickCSA", catMrRightClickCSA);
    catSmViewDecorativeAsteroids = TRUE;
    catSmViewResources = TRUE;
    catSmViewFOWRanges = TRUE;
    catSmViewBackground = TRUE;
    catSmViewAreasVisited = FALSE;
    catSmViewAreasOccupied = FALSE;
    catSmViewIcons = TRUE;
    catSmViewShipMeshes = TRUE;
    catSmViewPieplateLines = TRUE;
    catSmViewMeteors = TRUE;
    catSmViewCompass = TRUE;
    catSmViewObjectivePointers = TRUE;
    catSmViewSlipgatePaths = TRUE;
    catOpSensorsButtonsSet();                       // the exe passes 0 for OP_AreaExplored: the word it just cleared
}

void cat_opCataclysmDefault(char *name, featom *atom)   // 0x004da460: Cataclysm's interface preset
{
    feToggleButtonSet("OP_HWTaskbar", 0);
    feToggleButtonSet("OP_TaskbarButtons", 0);
    feToggleButtonSet("OP_TaskbarBackdrop", 0);
    feToggleButtonSet("OP_ExperienceMessages", 0);
    feToggleButtonSet("OP_HomeworldCSA", 0);
    feToggleButtonSet("OP_RightClickCSA", 1);
    catMoHWTaskbar = FALSE;
    catMoTaskbarButtons = FALSE;
    catMoTaskbarBackdrop = FALSE;
    catOpExperienceMessages = 0;
    catMrHomeworldCSA = FALSE;
    catMrRightClickCSA = TRUE;
    catSmViewDecorativeAsteroids = TRUE;
    catSmViewResources = TRUE;
    catSmViewFOWRanges = TRUE;
    catSmViewBackground = TRUE;
    catSmViewAreasVisited = TRUE;
    catSmViewAreasOccupied = FALSE;
    catSmViewIcons = TRUE;
    catSmViewShipMeshes = TRUE;
    catSmViewPieplateLines = TRUE;
    catSmViewMeteors = TRUE;
    catSmViewCompass = FALSE;
    catSmViewObjectivePointers = TRUE;
    catSmViewSlipgatePaths = TRUE;
    catOpSensorsButtonsSet();                       // the exe passes 1 for OP_AreaExplored: the word it just set
}

// 0x004db6d0: the texture-detail slider's process
udword cat_opTextureDetailProcess(regionhandle reg, sdword ID, udword event, udword data)
{
    catOpTextureDetail = ((sliderhandle)((featom *)reg->atom)->region)->value;
    cat_trTextureMemorySet(catOpTextureDetail);
    return 0;
}

void cat_opTextureDetail(char *name, featom *atom)  // 0x004db6f0: the OP_Texture_Detail slider, 0..100
{
    sliderhandle shandle;
    udword f;

    if (FEFIRSTCALL(atom))
    {
        catOpTextureDetailRegion = atom->region;
        shandle = (sliderhandle)atom->region;
        shandle->maxvalue = 100;
        shandle->value = catOpTextureDetail;
        f = regFilterSet(atom->region, 0);
        regFilterSet(atom->region, f | RPE_HoldLeft);
        shandle->processFunction = (uicfunction)cat_opTextureDetailProcess;
        return;
    }
    if (FELASTCALL(atom))
    {
        catOpTextureDetailRegion = NULL;
    }
}

void cat_opSpeechTest(char *name, featom *atom)     // 0x004db760: a test line (speech event 0x2001024)
{
    speechEventQueue(NULL, 0x2001024, -1, -1, 0, -1, -1, 5.0f, -1);
}

void cat_opSoundFXTest(char *name, featom *atom)    // 0x004db790: a test effect (UI event 0x1c8, played from the ship bank)
{
    soundEventPlay(NULL, 0x1c8, NULL);
}

/*-----------------------------------------------------------------------------
    Name        : cat_opSetAllDefaults (Cataclysm 0x004db7b0)
    Description : The OP_CataclysmFramerate presets: the full set (lowEnd FALSE) or the low-end set (TRUE)
    Inputs      : lowEnd - cat_opCataclysmFramerate passes (catOpGameFramerate == 0)
    Outputs     : the sound channels, voices, chatter and quality, the LOD frame rate and detail, the texture detail,
                    the video toggles, the effects and five sensors words; then the experience messages, the compass
                    and the sensors sphere off, and the sensors page's buttons
    Return      :
    Note        : the exe also switches GL_POLYGON_STIPPLE on the rgl.dll device (0x00b3a6d8); GoK's GL backend has
                    no such device (decision 28)
----------------------------------------------------------------------------*/
void cat_opSetAllDefaults(sdword lowEnd)
{
    sdword minVoices, maxVoices;

    if (lowEnd == 0)
    {
        opNumChannels = 8;
        opAutoChannel = 0;
        soundGetVoiceLimits(&minVoices, &maxVoices);
        soundSetNumVoices(opNumChannels + minVoices, opAutoChannel);
        opVoiceComm = 1;
        opVoiceStat = 1;
        opVoiceChat = 1;
        soundEventVocalSettings(1, 1, 1);
        opBattleChatter = 50;
        battleChatterFrequencySet(50);
        opSoundQuality = 0;
        soundMixerSetMode(0);
        opNoLODVal = 0;
        opDetailThresholdVal = 10;
        cat_lodSetTimeThreshold(10);
        catOpTextureDetail = 50;
        cat_trTextureMemorySet(50);
        catLodDetail = 0;
        texLinearFiltering = 1;
        trFilterEnable(1);
        showBackgrounds = 1;
        enableSmoothing = 1;
        enableCPV = 1;
        enableLensFlare = 1;
        enableRepairbots = 1;
        enableStipple = 0;
        smFuzzyBlobs = 1;
        enableTrails = 1;
        smInstantTransition = 0;
        opNumEffects = 256 - etgHistoryScalarMin;
        opEffectsVal = 0;
        etgHitEffectsEnabled = 1;
        etgDamageEffectsEnabled = 1;
        etgFireEffectsEnabled = 1;
        etgBulletEffectsEnabled = 1;
        catSmViewAreasVisited = 1;
        catSmViewAreasOccupied = 1;
        catSmViewMeteors = 1;
        catSmViewDecorativeAsteroids = 1;
        catSmViewBackground = 1;
    }
    else
    {
        opNumChannels = 0;
        opAutoChannel = 0;
        soundGetVoiceLimits(&minVoices, &maxVoices);
        soundSetNumVoices(minVoices, opAutoChannel);
        opVoiceComm = 0;
        opVoiceStat = 0;
        opVoiceChat = 0;
        soundEventVocalSettings(0, 0, 0);
        opBattleChatter = 0;
        battleChatterFrequencySet(0);
        opSoundQuality = 2;
        soundMixerSetMode(2);
        opNoLODVal = 0;
        opDetailThresholdVal = 30;
        cat_lodSetTimeThreshold(30);
        catOpTextureDetail = 0;
        cat_trTextureMemorySet(0);
        catLodDetail = 2;
        texLinearFiltering = 0;
        trFilterEnable(0);
        showBackgrounds = 0;
        enableSmoothing = 0;
        enableCPV = 0;
        enableLensFlare = 0;
        enableRepairbots = 0;
        enableStipple = 1;
        smFuzzyBlobs = 0;
        enableTrails = 0;
        smInstantTransition = 1;
        opNumEffects = 0;
        opEffectsVal = 2;
        etgHitEffectsEnabled = 0;
        etgDamageEffectsEnabled = 0;
        etgFireEffectsEnabled = 0;
        etgBulletEffectsEnabled = 0;
        catSmViewAreasVisited = 0;
        catSmViewAreasOccupied = 0;
        catSmViewMeteors = 0;
        catSmViewDecorativeAsteroids = 0;
        catSmViewBackground = 0;
    }
    catOpExperienceMessages = 0;
    catSmViewCompass = 0;
    catSmViewFOWRanges = 0;
    feToggleButtonSet("OP_ExperienceMessages", 0);
    catOpSensorsButtonsSet();
}

void cat_opCataclysmFramerate(char *name, featom *atom) // 0x004dbb30: the frame-rate radio picks a preset
{
    if (FEFIRSTCALL(atom))
    {
        feRadioButtonSet(name, catOpGameFramerate);
        return;
    }
    catOpGameFramerate = (ubyte)(memsize)atom->pData;
    cat_opSetAllDefaults(catOpGameFramerate == 0);
}


/*=============================================================================
    CAT: port diagnostic, not in the exe: the options round trip of the front-end screens self-test
    (/catFeScreensTest, harness mode fescreens; M2-R6 frontend). The Cataclysm option set (Cataclysm's own rows of the
    options file and the HW1 rows whose default Cataclysm changed) is checked at the exe's defaults (its .data / .bss
    initial values, the three effects words as the start-up's high preset sets them) as the start-up left it, set to those defaults, written through utyOptionsFileWrite, scrambled,
    read back through utyOptionsList (catUtyOptionsFileReadBack) and compared. "optionstest:" log lines,
    "optionstest: done ... result=PASS|FAIL".
=============================================================================*/
typedef struct
{
    char *row;                                      // the options-file row
    sdword *word;
    sdword exeDefault;                              // the exe's initial value
    udword exeAddress;
}
catoptiontest;

void catOptionsRunSelfTest(void)
{
    static catoptiontest set[] =
    {
        {"LargePopupMenuFonts",         (sdword *)&catPmLargeFont,              0, 0x00a5749c},
        {"Homeworld_CSA",               (sdword *)&catMrHomeworldCSA,           0, 0x00a7526c},
        {"RightClick_CSA",              (sdword *)&catMrRightClickCSA,          1, 0x008b9ec8},
        {"AutoFocusOnWaypoints",        (sdword *)&catMrWaypointCentre,         0, 0x00a75270},
        {"ShowTaskbarBackdrop",         (sdword *)&catMoTaskbarBackdrop,        0, 0x00a4b880},
        {"ShowTaskbarButtons",          (sdword *)&catMoTaskbarButtons,         0, 0x00a4b87c},
        {"ExperienceMessages",          &catOpExperienceMessages,               0, 0x00a5e248},
        {"HomeworldTaskbar",            (sdword *)&catMoHWTaskbar,              0, 0x00a4b878},
        {"ShowAsteriod0",               (sdword *)&catSmViewDecorativeAsteroids, 1, 0x00892b48},
        {"ShowResources",               (sdword *)&catSmViewResources,          1, 0x00892b4c},
        {"ShowFOWBlobs",                (sdword *)&catSmViewFOWRanges,          1, 0x00892b50},
        {"ShowBackground",              (sdword *)&catSmViewBackground,         1, 0x00892b54},
        {"ShowAreaVisited",             (sdword *)&catSmViewAreasVisited,       1, 0x00892b58},
        {"ShowAreaOccupied",            (sdword *)&catSmViewAreasOccupied,      0, 0x00a5b19c},
        {"ShowIcons",                   (sdword *)&catSmViewIcons,              1, 0x00892b5c},
        {"ShowEnemyShips",              (sdword *)&catSmViewShipMeshes,         1, 0x00892b60},
        {"ShowFriendlyShips",           (sdword *)&catSmViewPieplateLines,      1, 0x00892b64},
        {"ShowUniverseExtents",         (sdword *)&catSmViewUniverseExtents,    0, 0x00a5b1a0},
        {"ShowMeteors",                 (sdword *)&catSmViewMeteors,            1, 0x00892b68},
        {"ShowCompass",                 (sdword *)&catSmViewCompass,            0, 0x00a5b1a4},
        {"ShowObjectivesPointer",       (sdword *)&catSmViewObjectivePointers,  1, 0x00892b6c},
        {"ShowSlipgatePaths",           (sdword *)&catSmViewSlipgatePaths,      1, 0x00892b70},
        {"HardwareTransformLighting",   &catOpHardwareTL,                       0, 0x00a4cc18},
        {"TextureDetail",               &catOpTextureDetail,                    50, 0x0088be74},
        {"GameFramerateOption",         &catOpGameFramerate,                    1, 0x0088be98},
        {"DetailQuality",               (sdword *)&catLodDetail,                0, 0x00a4cc38},
        {"SetTextureRequirements",      &catOpTextureRequirementsSet,           0, 0x00a76260},
        // .bss 0, but the start-up's opUpdateSettings applies effectsLevel 0 (the high preset), which sets them
        {"enableCPV",                   &enableCPV,                             1, 0x00c52378},
        {"enableLensFlare",             &enableLensFlare,                       1, 0x00c52380},
        {"enableRepairbots",            &enableRepairbots,                      1, 0x00c52374},
        {"MusicVolume",                 &opMusicVol,                            60, 0x0088be4c},
        {"SFXVolume",                   &opSFXVol,                              75, 0x0088be50},
        {"SpeechVolume",                &opSpeechVol,                           75, 0x0088be54},
        {"detailThreshold",             (sdword *)&opDetailThresholdVal,        10, 0x0088be6c},
        {NULL, NULL, 0, 0}
    };
    sdword i, n = 0, atDefault = 0, readBack = 0;
    char *path;

    for (i = 0; set[i].row != NULL; i++)
    {
        n++;
        if (*set[i].word == set[i].exeDefault)
        {
            atDefault++;
        }
        else
        {
            dbgMessagef("optionstest: %s is %d after the start-up, the exe's default (0x%08x) is %d", set[i].row,
                        *set[i].word, set[i].exeAddress, set[i].exeDefault);
        }
        *set[i].word = set[i].exeDefault;
    }
    utyOptionsFileWrite();
    for (i = 0; set[i].row != NULL; i++)
    {
        *set[i].word = set[i].exeDefault + 7;       //scrambled: a value the file must overwrite
    }
    path = catUtyOptionsFileReadBack();
    for (i = 0; set[i].row != NULL; i++)
    {
        if (*set[i].word == set[i].exeDefault)
        {
            readBack++;
        }
        else
        {
            dbgMessagef("optionstest: FAIL %s read back as %d, written %d", set[i].row, *set[i].word, set[i].exeDefault);
            *set[i].word = set[i].exeDefault;
        }
    }
    dbgMessagef("optionstest: %d options written to and read back from %s", n, path);
    dbgMessagef("optionstest: done options=%d startdefaults=%d readback=%d result=%s", n, atDefault, readBack,
                (readBack == n) ? "PASS" : "FAIL");
}
