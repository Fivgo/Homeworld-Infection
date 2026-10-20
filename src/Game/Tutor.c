/*
    Tutor.c - Functions for the Tutorial System
*/

#include "Tutor.h"
#include "SinglePlayer.h"
#include "CatMissions.h"      // CAT (M2-R8 kas-runtime-mission01): the generated Tutorial1 (Init_Tutorial1 / Watch_Tutorial1)

#include <math.h>

#include "AIPlayer.h"
#include "CameraCommand.h"
#include "Collision.h"
#include "ConsMgr.h"
#include "FastMath.h"
#include "File.h"
#include "FontReg.h"
#include "GamePick.h"
#include "glinc.h"
#include "KAS.h"
#include "NIS.h"            // CAT: nisFullyScissored / nisScissorFade (the pointers under the letterbox)
#include "Sensors.h"        // CAT: smZoomingIn / smZoomingOut / catSmViewObjectivePointers
#include "LevelLoad.h"
#include "main.h"
#include "mainrgn.h"
#include "Memory.h"
#include "mouse.h"
#include "PiePlate.h"
#include "Region.h"
#include "render.h"
#include "SaveGame.h"
#include "Select.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "SpeechEvent.h"
#include "StringSupport.h"
#include "Subtitle.h"
#include "TaskBar.h"
#include "texreg.h"
#include "UIControls.h"
#include "utility.h"

#ifdef _WIN32
    #include <windows.h>
#endif

#ifdef _MSC_VER
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif


extern KASWatchFunction CurrentMissionWatchFunction;       // KAS.c (0x00a43ee4)

//hack to get the speech to work when game is paused
bool32 FalkosFuckedUpTutorialFlag = FALSE;

// Function declarations
void utySinglePlayerGameStart(char *name, featom *atom);
char *tutGetNextTextLine(char *pDest, char *pString, long Width, int pDestSize);
udword tutProcessNextButton(struct tagRegion *reg, smemsize ID, udword event, udword data);
udword tutProcessBackButton(struct tagRegion *reg, smemsize ID, udword event, udword data);
udword uicButtonProcess(regionhandle region, smemsize ID, udword event, udword data);

// flags for controlling game elements from the tutorial
tutGameEnableFlags tutEnable;
char tutBuildRestrict[TOTAL_STD_SHIPS];
char tutGameMessageList[16][256];
long tutGameMessageIndex = 0;
ShipType tutFEContextMenuShipType;

static char tutLastLessonName[256] = "";
static char tutCurrLessonName[256] = "";

/*
sdword  tutTextPointerType = 0;
sdword  tutPointerX, tutPointerY;
rectangle tutPointerRegion;
hvector tutPointerAIPoint;
*/
ShipPtr tutPointerShip;
rectangle *tutPointerShipHealthRect;
rectangle *tutPointerShipGroupRect;
//list of named tutorial pointers
static tutpointer tutPointer[TUT_NumberPointers];
bool32 tutPointersDrawnThisFrame = FALSE;


static sdword  tutTextPosX, tutTextPosY;
static sdword  tutTextSizeX, tutTextSizeY;

static rectangle   tutTextRect;
static fonthandle tutTextFont = 0;

static regionhandle    tutRootRegion;
static sdword tutRootRegionCount = 0;

// visibility flag & region handle for the displayed text
sdword  tutTextVisible = FALSE;
regionhandle tutTextRegion = NULL;
void tutDrawTextFunction(regionhandle reg);

// visibility flag & region handle for the displayed images
sdword  tutImageVisible = FALSE;
regionhandle tutImageRegion = NULL;
void tutDrawImageFunction(regionhandle reg);

// visibility flag & region handle for the "next" button
sdword tutNextVisible = FALSE;
buttonhandle tutNextRegion = NULL;
sdword  tutNextButtonState = 0;
void tutDrawNextButtonFunction(regionhandle reg);

// visibility flag & region handle for the "back" button
sdword tutBackVisible = FALSE;
sdword tutPrevVisible = FALSE;
buttonhandle tutBackRegion = NULL;
void tutDrawBackButtonFunction(regionhandle reg);

// CAT (M2-R8 singleplayer-tutorial): Cataclysm's other tutorial buttons, each its own region under the root
//      (0x00532190 ..): the next-text button (0x00a6c4f0 visible, 0x00a6c4f4 region, 0x00a6c4f8 clicked), the
//      previous-lesson button (tutPrevVisible 0x00a6c4fc, 0x00a6c500, 0x00a6c504; HW1 showed it as the back button)
//      and the quit button (0x00a6c508, 0x00a6c50c, 0x00a6c510); the Next button (0x00a6c4e4 ..) is the next lesson's
sdword catTutNextTextVisible = FALSE;
static buttonhandle catTutNextTextRegion = NULL;
static sdword catTutNextTextState = 0;
static buttonhandle catTutPrevRegion = NULL;
static sdword catTutPrevState = 0;
sdword catTutQuitVisible = FALSE;
static buttonhandle catTutQuitRegion = NULL;
static sdword catTutQuitState = 0;
// CAT: the buttons shown before a manager screen opened (cat_tutButtonsHideForScreen 0x005341f0: 0x00a6c524 ..)
static sdword catTutSavedNextVisible = FALSE, catTutSavedPrevVisible = FALSE, catTutSavedQuitVisible = FALSE;
// CAT: where the objective list's pointer points (cat_tutObjPointerXYSet 0x00531290: 0x00a6c51c / 0x00a6c520)
static sdword catTutObjPointerX = 0, catTutObjPointerY = 0;
static udword cat_tutProcessNextTextButton(struct tagRegion *reg, smemsize ID, udword event, udword data);
static udword cat_tutProcessPrevButton(struct tagRegion *reg, smemsize ID, udword event, udword data);
static udword cat_tutProcessQuitButton(struct tagRegion *reg, smemsize ID, udword event, udword data);
static void cat_tutDrawNextTextButtonFunction(regionhandle reg);
static void cat_tutDrawPrevButtonFunction(regionhandle reg);
static void cat_tutDrawQuitButtonFunction(regionhandle reg);

// List of image indices for the displayed images
static char szImageIndexList[16];


// Construction manager exports
extern shipavailable *cmShipsAvailable;
extern LinkedList listofShipsInProgress;

featom tutRootAtom =
{
    "TutorialWrapper",      //  char  *name;                                //optional name of control
    FAF_Function,           //  udword flags;                               //flags to control behavior
    0,                      //  udword status;                              //status flags for this atom, checked etc.
    FA_UserRegion,          //  ubyte  type;                                //type of control (button, scroll bar, etc.)
    0,                      //  ubyte  borderWidth;                         //width, in pixels, of the border
    0,                      //  uword  tabstop;                             //denotes the tab ordering of UI controls
    150,                    //  color  borderColor;                         //optional color of border
    4,                      //  color  contentColor;                        //optional color of content
    0,0,                      //  sdword x;                                   //-+
    0,0,                      //  sdword y;                                   // |>rectangle of region
    640,0,                    //  sdword width;                               // |
    480,0,                    //  sdword height;                              //-+
    NULL,                   //  ubyte *pData;                               //pointer to type-specific data
    NULL,                   //  ubyte *attribs;                             //sound(button atom) or font(static text atom) reference
    0,                      //  char   hotKeyModifiers;
    {0},                    //  char   hotKey[FE_NumberLanguages];
    {0},                    //  char   pad2[2]
    {0},                    //  udword drawstyle[2];
    0,                      //  void*  region;
    {0}                     //  udword pad[2];
};


featom tutTextAtom =
{
    "TutorialText",         //  char  *name;                                //optional name of control
    FAF_Function,           //  udword flags;                               //flags to control behavior
    0,                      //  udword status;                              //status flags for this atom, checked etc.
    FA_UserRegion,          //  ubyte  type;                                //type of control (button, scroll bar, etc.)
    0,                      //  ubyte  borderWidth;                         //width, in pixels, of the border
    0,                      //  uword  tabstop;                             //denotes the tab ordering of UI controls
    150,                    //  color  borderColor;                         //optional color of border
    4,                      //  color  contentColor;                        //optional color of content
    0,0,                      //  sdword x;                                   //-+
    0,0,                      //  sdword y;                                   // |>rectangle of region
    0,0,                      //  sdword width;                               // |
    0,0,                      //  sdword height;                              //-+
    NULL,                   //  ubyte *pData;                               //pointer to type-specific data
    NULL,                   //  ubyte *attribs;                             //sound(button atom) or font(static text atom) reference
    0,                      //  char   hotKeyModifiers;
    {0},                    //  char   hotKey[FE_NumberLanguages];
    {0},                    //  char   pad2[2]
    {0},                    //  udword drawstyle[2];
    0,                      //  void*  region;
    {0}                     //  udword pad[2];
};

featom tutImageAtom =
{
    "TutorialImage",        //  char  *name;                                //optional name of control
    FAF_Function,           //  udword flags;                               //flags to control behavior
    0,                      //  udword status;                              //status flags for this atom, checked etc.
    FA_UserRegion,          //  ubyte  type;                                //type of control (button, scroll bar, etc.)
    0,                      //  ubyte  borderWidth;                         //width, in pixels, of the border
    0,                      //  uword  tabstop;                             //denotes the tab ordering of UI controls
    150,                    //  color  borderColor;                         //optional color of border
    4,                      //  color  contentColor;                        //optional color of content
    0,0,                      //  sdword x;                                   //-+
    0,0,                      //  sdword y;                                   // |>rectangle of region
    0,0,                      //  sdword width;                               // |
    0,0,                      //  sdword height;                              //-+
    NULL,                   //  ubyte *pData;                               //pointer to type-specific data
    NULL,                   //  ubyte *attribs;                             //sound(button atom) or font(static text atom) reference
    0,                      //  char   hotKeyModifiers;
    {0},                    //  char   hotKey[FE_NumberLanguages];
    {0},                    //  char   pad2[2]
    {0},                    //  udword drawstyle[2];
    0,                      //  void*  region;
    {0}                     //  udword pad[2];
};

featom tutNextAtom =
{
    "TutorialNext",         //  char  *name;                                //optional name of control
    FAF_Function,           //  udword flags;                               //flags to control behavior
    0,                      //  udword status;                              //status flags for this atom, checked etc.
    FA_Button,              //  ubyte  type;                                //type of control (button, scroll bar, etc.)
    0,                      //  ubyte  borderWidth;                         //width, in pixels, of the border
    0,                      //  uword  tabstop;                             //denotes the tab ordering of UI controls
    150,                    //  color  borderColor;                         //optional color of border
    4,                      //  color  contentColor;                        //optional color of content
    0,0,                      //  sdword x;                                   //-+
    0,0,                      //  sdword y;                                   // |>rectangle of region
    0,0,                      //  sdword width;                               // |
    0,0,                      //  sdword height;                              //-+
    NULL,                   //  ubyte *pData;                               //pointer to type-specific data
    NULL,                   //  ubyte *attribs;                             //sound(button atom) or font(static text atom) reference
    0,                      //  char   hotKeyModifiers;
    {0},                    //  char   hotKey[FE_NumberLanguages];
    {0},                    //  char   pad2[2]
    {0},                    //  udword drawstyle[2];
    0,                      //  void*  region;
    {0}                     //  udword pad[2];
};

// CAT (M2-R8 singleplayer-tutorial): the atoms of Cataclysm's three other buttons (0x008ab340, 0x008ab390, 0x008ab3e0)
#define CAT_TUT_BUTTON_ATOM(atomname)                                                                               \
{                                                                                                                   \
    atomname, FAF_Function, 0, FA_Button, 0, 0, 150, 4, 0,0, 0,0, 0,0, 0,0, NULL, NULL, 0, {0}, {0}, {0}, 0, {0}      \
}
featom catTutNextTextAtom = CAT_TUT_BUTTON_ATOM("TutorialNextText");
featom catTutPrevAtom = CAT_TUT_BUTTON_ATOM("TutorialPrev");
featom catTutQuitAtom = CAT_TUT_BUTTON_ATOM("TutorialQuit");

featom tutBackAtom =
{
    "TutorialBack",         //  char  *name;                                //optional name of control
    FAF_Function,           //  udword flags;                               //flags to control behavior
    0,                      //  udword status;                              //status flags for this atom, checked etc.
    FA_Button,              //  ubyte  type;                                //type of control (button, scroll bar, etc.)
    0,                      //  ubyte  borderWidth;                         //width, in pixels, of the border
    0,                      //  uword  tabstop;                             //denotes the tab ordering of UI controls
    150,                    //  color  borderColor;                         //optional color of border
    4,                      //  color  contentColor;                        //optional color of content
    0,0,                      //  sdword x;                                   //-+
    0,0,                      //  sdword y;                                   // |>rectangle of region
    0,0,                      //  sdword width;                               // |
    0,0,                      //  sdword height;                              //-+
    NULL,                   //  ubyte *pData;                               //pointer to type-specific data
    NULL,                   //  ubyte *attribs;                             //sound(button atom) or font(static text atom) reference
    0,                      //  char   hotKeyModifiers;
    {0},                    //  char   hotKey[FE_NumberLanguages];
    {0},                    //  char   pad2[2]
    {0},                    //  udword drawstyle[2];
    0,                      //  void*  region;
    {0}                     //  udword pad[2];
};


// CAT (M2-R8 singleplayer-tutorial): Cataclysm's 35 tutorial images (tutInitialize 0x00533190 loads
//      feman\texdecorative\<name>.LiF from the list of the current language; HW1 had 32): the next-lesson, quit and
//      restart buttons, the keys and mouse pictures, then the next-text button
#define TUT_NUM_IMAGES 36

#define TUT_NEXTLESSON_ON       0       // the Next (lesson) button: tutDrawNextButtonFunction 0x00532820
#define TUT_NEXTLESSON_OFF      1
#define TUT_NEXTLESSON_MOUSE    2
#define TUT_QUIT_ON             3       // the quit button: cat_tutDrawQuitButtonFunction 0x005329e0
#define TUT_QUIT_OFF            4
#define TUT_QUIT_MOUSE          5
#define TUT_REST_ON             6       // the previous / back buttons draw the restart art (0x00532940, 0x00532d80)
#define TUT_REST_OFF            7
#define TUT_REST_MOUSE          8
#define TUT_NEXT_ON             32      // the next-text button: cat_tutDrawNextTextButtonFunction 0x005328c0
#define TUT_NEXT_OFF            33
#define TUT_NEXT_MOUSE          34

// English (0x008ab47c, also the names tutShowImages looks up)
char *tutImageList[TUT_NUM_IMAGES] =
{
    "Tut_NextLesson_On",
    "Tut_NextLesson_Off",
    "Tut_NextLesson_Mouse",
    "Tut_quit_on",
    "Tut_quit_off",
    "Tut_quit_mouse",
    "Tut_restart_on",
    "Tut_restart_off",
    "Tut_restart_mouse",
    "Tut_Rightclick",
    "Tut_Key_F",
    "Tut_Key_M",
    "Tut_Key_Shift",
    "Tut_Key_Ctrl",
    "Tut_Key_Alt",
    "Tut_Key_Space",
    "Tut_Key_Tab",
    "Tut_Mouse_ButtonR",
    "Tut_Mouse_ButtonR_MoveLRUD",
    "Tut_Mouse_ButtonR_MoveLR",
    "Tut_Mouse_ButtonR_MoveU",
    "Tut_Mouse_ButtonR_MoveD",
    "Tut_Mouse_ButtonLR_MoveUD",
    "Tut_Mouse_ButtonLR_MoveU",
    "Tut_Mouse_ButtonLR_MoveD",
    "Tut_Mouse_ButtonL",
    "Tut_Mouse_ButtonL_MoveLRUD",
    "Tut_Mouse_ButtonM",
    "Tut_Mouse_Middle_RollD",
    "Tut_Mouse_Middle_RollU",
    "Tut_Mouse_Middle_RollUD",
    "Tut_Mouse_Lbandbox",
    "Tut_Next_On",
    "Tut_Next_Off",
    "Tut_Next_Mouse",
    ""
};

// French (0x008ab50c)
char *tutFrenchImageList[TUT_NUM_IMAGES] =
{
    "Tut_fr_NextLesson_On",
    "Tut_fr_NextLesson_Off",
    "Tut_fr_NextLesson_Mouse",
    "Tut_fr_quit_on",
    "Tut_fr_quit_off",
    "Tut_fr_quit_mouse",
    "Tut_fr_restart_on",
    "Tut_fr_restart_off",
    "Tut_fr_restart_mouse",
    "Tut_Rightclick",
    "Tut_Key_F",
    "Tut_Key_D",
    "Tut_fr_Key_Shift",
    "Tut_fr_Key_Ctrl",
    "Tut_Key_Alt",
    "Tut_fr_Key_Space",
    "Tut_Key_Tab",
    "Tut_Mouse_ButtonR",
    "Tut_Mouse_ButtonR_MoveLRUD",
    "Tut_Mouse_ButtonR_MoveLR",
    "Tut_Mouse_ButtonR_MoveU",
    "Tut_Mouse_ButtonR_MoveD",
    "Tut_Mouse_ButtonLR_MoveUD",
    "Tut_Mouse_ButtonLR_MoveU",
    "Tut_Mouse_ButtonLR_MoveD",
    "Tut_Mouse_ButtonL",
    "Tut_Mouse_ButtonL_MoveLRUD",
    "Tut_Mouse_ButtonM",
    "Tut_Mouse_Middle_RollD",
    "Tut_Mouse_Middle_RollU",
    "Tut_Mouse_Middle_RollUD",
    "Tut_Mouse_Lbandbox",
    "Tut_fr_Next_On",
    "Tut_fr_Next_Off",
    "Tut_fr_Next_Mouse",
    ""
};

// German (0x008ab59c)
char *tutGermanImageList[TUT_NUM_IMAGES] =
{
    "Tut_ger_NextLesson_On",
    "Tut_ger_NextLesson_Off",
    "Tut_ger_NextLesson_Mouse",
    "Tut_ger_quit_on",
    "Tut_ger_quit_off",
    "Tut_ger_quit_mouse",
    "Tut_ger_restart_on",
    "Tut_ger_restart_off",
    "Tut_ger_restart_mouse",
    "Tut_Rightclick",
    "Tut_Key_F",
    "Tut_Key_W",
    "Tut_ger_Key_Shift",
    "Tut_ger_Key_Ctrl",
    "Tut_Key_Alt",
    "Tut_ger_Key_Space",
    "Tut_Key_Tab",
    "Tut_Mouse_ButtonR",
    "Tut_Mouse_ButtonR_MoveLRUD",
    "Tut_Mouse_ButtonR_MoveLR",
    "Tut_Mouse_ButtonR_MoveU",
    "Tut_Mouse_ButtonR_MoveD",
    "Tut_Mouse_ButtonLR_MoveUD",
    "Tut_Mouse_ButtonLR_MoveU",
    "Tut_Mouse_ButtonLR_MoveD",
    "Tut_Mouse_ButtonL",
    "Tut_Mouse_ButtonL_MoveLRUD",
    "Tut_Mouse_ButtonM",
    "Tut_Mouse_Middle_RollD",
    "Tut_Mouse_Middle_RollU",
    "Tut_Mouse_Middle_RollUD",
    "Tut_Mouse_Lbandbox",
    "Tut_ger_Next_On",
    "Tut_ger_Next_Off",
    "Tut_ger_Next_Mouse",
    ""
};

// Spanish (0x008ab62c)
char *tutSpanishImageList[TUT_NUM_IMAGES] =
{
    "Tut_sp_NextLesson_On",
    "Tut_sp_NextLesson_Off",
    "Tut_sp_NextLesson_Mouse",
    "Tut_sp_quit_on",
    "Tut_sp_quit_off",
    "Tut_sp_quit_mouse",
    "Tut_sp_restart_on",
    "Tut_sp_restart_off",
    "Tut_sp_restart_mouse",
    "Tut_Rightclick",
    "Tut_Key_F",
    "Tut_Key_D",
    "Tut_sp_Key_Shift",
    "Tut_Key_Ctrl",
    "Tut_Key_Alt",
    "Tut_sp_Key_Space",
    "Tut_Key_Tab",
    "Tut_Mouse_ButtonR",
    "Tut_Mouse_ButtonR_MoveLRUD",
    "Tut_Mouse_ButtonR_MoveLR",
    "Tut_Mouse_ButtonR_MoveU",
    "Tut_Mouse_ButtonR_MoveD",
    "Tut_Mouse_ButtonLR_MoveUD",
    "Tut_Mouse_ButtonLR_MoveU",
    "Tut_Mouse_ButtonLR_MoveD",
    "Tut_Mouse_ButtonL",
    "Tut_Mouse_ButtonL_MoveLRUD",
    "Tut_Mouse_ButtonM",
    "Tut_Mouse_Middle_RollD",
    "Tut_Mouse_Middle_RollU",
    "Tut_Mouse_Middle_RollUD",
    "Tut_Mouse_Lbandbox",
    "Tut_sp_Next_On",
    "Tut_sp_Next_Off",
    "Tut_sp_Next_Mouse",
    ""
};

// Italian (0x008ab6bc)
char *tutItalianImageList[TUT_NUM_IMAGES] =
{
    "Tut_it_NextLesson_On",
    "Tut_it_NextLesson_Off",
    "Tut_it_NextLesson_Mouse",
    "Tut_it_quit_on",
    "Tut_it_quit_off",
    "Tut_it_quit_mouse",
    "Tut_it_restart_on",
    "Tut_it_restart_off",
    "Tut_it_restart_mouse",
    "Tut_Rightclick",
    "Tut_Key_F",
    "Tut_Key_M",
    "Tut_Key_Shift",
    "Tut_Key_Ctrl",
    "Tut_Key_Alt",
    "Tut_it_Key_Space",
    "Tut_Key_Tab",
    "Tut_Mouse_ButtonR",
    "Tut_Mouse_ButtonR_MoveLRUD",
    "Tut_Mouse_ButtonR_MoveLR",
    "Tut_Mouse_ButtonR_MoveU",
    "Tut_Mouse_ButtonR_MoveD",
    "Tut_Mouse_ButtonLR_MoveUD",
    "Tut_Mouse_ButtonLR_MoveU",
    "Tut_Mouse_ButtonLR_MoveD",
    "Tut_Mouse_ButtonL",
    "Tut_Mouse_ButtonL_MoveLRUD",
    "Tut_Mouse_ButtonM",
    "Tut_Mouse_Middle_RollD",
    "Tut_Mouse_Middle_RollU",
    "Tut_Mouse_Middle_RollUD",
    "Tut_Mouse_Lbandbox",
    "Tut_it_Next_On",
    "Tut_it_Next_Off",
    "Tut_it_Next_Mouse",
    ""
};

// This string structure must match the tutGameEnableFlags struct
char *tutGameEnableString[] =
{
    "KASFrame",     // FOR INTERAL USE ONLY
    "GameRunning",


    "BuildManager",
    "SensorsManager",
    "ResearchManager",
    "PauseGame",
    "Dock",
    "Formation",
    "Launch",
    "Move",
    "MoveIssue",
    "Waypoint",         // CAT: new (the exe's flag table 0x008ab74c)
    "WaypointIssue",    // CAT: new
    "Attack",           // [audit: M2-R8 singleplayer-tutorial] restored: index 13 in the exe's table, bAttack's bit
    "Harvest",
    "CancelCommand",
    "Scuttle",
    "Retire",
    "ClickSelect",
    "BandSelect",
    "CancelSelect",
    "Special",
    "BuildBuildShips",
    "BuildPauseJobs",
    "BuildCancelJobs",
    "BuildClose",
    "BuildArrows",
    "SensorsClose",
    "ContextMenus",
    "ContextFormDelta",
    "ContextFormBroad",
    "ContextFormX",
    "ContextFormClaw",
    "ContextFormWall",
    "ContextFormSphere",
    "ContextFormCustom",
    "Evasive",
    "Neutral",
    "Agressive",
    "TaskbarOpen",
    "TaskbarClose",
    "ResearchSelectTech",
    "ResearchSelectLab",
    "ResearchResearch",
    "ResearchClearLab",
    "ResearchClose",
    "Focus",
    "FocusCancel",
    "MothershipFocus",
    "LaunchSelectShips",
    "LaunchLaunch",
    "LaunchLaunchAll",
    "LaunchClose",
    "ManagersLocked",   // CAT: new
    ""
};
// [audit: M2-R8 singleplayer-tutorial] the exe's 54 names (0x008ab74c) and the terminator: a flag's index is its bit
_Static_assert(sizeof(tutGameEnableString) / sizeof(tutGameEnableString[0]) == 54 + 1,
               "tutGameEnableString must list the exe's 54 tutorial flags in tutGameEnableFlags' order");

color       tutTexture[TUT_NUM_IMAGES];
lifheader   *tutImage[TUT_NUM_IMAGES];


static char *tutTutorialNames[4] = {"", "Tutorial1"};

void tutPreInitTutorial(char *dirfile, char *levelfile)
{
#ifdef _WIN32
    sprintf(dirfile, "Tutorials\\%s\\", tutTutorialNames[tutorial]);
#else
    sprintf(dirfile, "Tutorials/%s/", tutTutorialNames[tutorial]);
#endif
    sprintf(levelfile, "%s.mission", tutTutorialNames[tutorial]);

    tutEnableEverything();

    //perform a level pass to see what ships we need to load
    levelPreInit(dirfile, levelfile);
}


void tutInitTutorial(char *dirfile, char *levelfile)
{
    levelInit(dirfile, levelfile);
    tutLastLessonName[0] = '\0';
    tutCurrLessonName[0] = '\0';

    // CAT (M2-R8 kas-runtime-mission01): Cataclysm's kas2c Tutorial1, generated from the exe (tutInitTutorial 0x0052f690:
    //      kasMissionStart("tutorial1", Init_Tutorial1 0x006704e0, Watch_Tutorial1 0x006708a0) when tutorial == 1)
    if (tutorial == 1)
    {
        kasMissionStart("tutorial1", Init_Tutorial1, Watch_Tutorial1);
    }
}

void tutSaveLesson(sdword Num, char *pName)
{
    char name[256];
    char *comma;

    collUpdateCollBlobs();
    collUpdateObjsInCollBlobs();

    strcpy(tutLastLessonName, tutCurrLessonName);

    // CAT: a comma of the lesson's name becomes a space (tutSaveLesson 0x0052f6d0). CAT-FIX: the exe edits the
    //      caller's string (a KAS literal); a copy is edited here
    memStrncpy(name, pName, sizeof(name) - 1);
    for (comma = strchr(name, ','); comma != NULL; comma = strchr(comma + 1, ','))
    {
        *comma = ' ';
    }
    sprintf(tutCurrLessonName, "%s%s", TutorialSavedGamesPath, name);
    SaveGame(tutCurrLessonName);
}

static void SaveLong(sdword Val)
{
    SaveStructureOfSize( &Val, 4 );
}

static sdword LoadLong(void)
{
sdword Val;

    LoadStructureOfSizeToAddress( &Val, 4 );
    return Val;
}

/*-----------------------------------------------------------------------------
    Name        : tutSaveTutorialGame / tutLoadTutorialGame         0x0052f750 / 0x0052f8b0
    Description : CAT (M2-R8 singleplayer-tutorial): Cataclysm's tutorial state: the lesson names, the flags (8 bytes),
                  the build restrictions, the text box, then the visible flags of the text, Next, previous, quit, back
                  and next-text buttons and the speech flag. A load shows the saved buttons again (the exe inlines
                  each Show), clears the clicks, frees the pointers (cat_tutPointerListReset) and the pointer ship
----------------------------------------------------------------------------*/
void tutSaveTutorialGame(void)
{
    Save_String(tutCurrLessonName);
    Save_String(tutLastLessonName);

    SaveStructureOfSize( &tutEnable, sizeof(tutGameEnableFlags) );
    SaveStructureOfSize( tutBuildRestrict, sizeof(tutBuildRestrict) );

    SaveLong(tutTextPosX);
    SaveLong(tutTextPosY);
    SaveLong(tutTextSizeX);
    SaveLong(tutTextSizeY);

    SaveStructureOfSize( &tutTextRect, sizeof(rectangle) );

    SaveLong(tutTextVisible);
    SaveLong(tutNextVisible);
    SaveLong(tutPrevVisible);
    SaveLong(catTutQuitVisible);
    SaveLong(tutBackVisible);
    SaveLong(catTutNextTextVisible);
    SaveLong(FalkosFuckedUpTutorialFlag);
}

void tutLoadTutorialGame(void)
{
    Load_StringToAddress(tutCurrLessonName);
    Load_StringToAddress(tutLastLessonName);

    LoadStructureOfSizeToAddress( &tutEnable, sizeof(tutGameEnableFlags) );
    LoadStructureOfSizeToAddress( tutBuildRestrict, sizeof(tutBuildRestrict) );

    tutTextPosX = LoadLong();
    tutTextPosY = LoadLong();
    tutTextSizeX = LoadLong();
    tutTextSizeY = LoadLong();

    LoadStructureOfSizeToAddress( &tutTextRect, sizeof(rectangle) );

    tutTextVisible = LoadLong();
    if (LoadLong())
    {
        tutShowNextButton();
    }
    if (LoadLong())
    {
        tutShowPrevButton();
    }
    if (LoadLong())
    {
        cat_tutShowQuitButton();
    }
    if (LoadLong())
    {
        tutShowBackButton();
    }
    if (LoadLong())
    {
        cat_tutShowNextTextButton();
    }
    FalkosFuckedUpTutorialFlag = LoadLong();

    tutResetGameMessageQueue();                             // CAT: 0x00a6c2c4

    tutNextButtonState = 0;
    catTutPrevState = 0;
    catTutQuitState = 0;
    catTutNextTextState = 0;

    //clear out all the pointers
    cat_tutPointerListReset();
    tutPointerShip = NULL;
    tutPointerShipGroupRect = tutPointerShipHealthRect = NULL;
    tutFEContextMenuShipType = 0;
}

void tutTutorial1(char *name, featom *atom)
{
    static bool32 beginning;

    if (FEFIRSTCALL(atom))
    {
        beginning = FALSE;
    }
    else if (FELASTCALL(atom))
    {
        if (!beginning)
        {
            tutorial = 0;
        }
    }
    else
    {
        beginning = TRUE;
        tutorial = 1;
        dbgAssertOrIgnore(startingGame == FALSE);

#ifdef HW_BUILD_FOR_DEBUGGING
        dbgMessage("Tutorial1 started");
#endif

        utySinglePlayerGameStart(name, atom);
    }
}

/*-----------------------------------------------------------------------------
    Name        : tutPointerAllocate
    Description : Allocate a tutorial pointer from the pointer list.
    Inputs      : name - name of pointer.  Will overwrite pointers of same name.
                  type - type of pointer to allocate.
    Outputs     :
    Return      : pointer to pointer.  Quaint isn't it?
----------------------------------------------------------------------------*/
tutpointer *tutPointerAllocate(char *name, sdword type)
{
    sdword index, freeIndex = -1;

    for (index = 0; index < TUT_NumberPointers; index++)
    {                                                       //make sure this name is unique
        if (!strcasecmp(name, tutPointer[index].name))      // CAT: _stricmp (tutSetPointerTargetXY 0x0052ff70 ..)
        {                                                   //if this one is the same name
            if (tutPointer[index].ship == tutPointerShip)
            {
                tutPointerShip = NULL;
                tutPointerShipGroupRect = tutPointerShipHealthRect = NULL;
            }
            freeIndex = index;
            break;
        }
        if (tutPointer[index].pointerType == TUT_PointerTypeNone)
        {                                                   //if this one is free
            freeIndex = index;
        }
    }
    
#ifdef HW_BUILD_FOR_DEBUGGING
    if (freeIndex < 0)
    {
        dbgFatalf(DBG_Loc, "Cannot allocate tutorial pointer '%s'.", name);
    }
#endif

    memStrncpy(tutPointer[freeIndex].name, name, TUT_PointerNameMax);
    tutPointer[freeIndex].pointerType = type;
    tutPointer[freeIndex].objective = FALSE;                // CAT: +0x64, set by the objective setters
    return(&tutPointer[freeIndex]);
}

// CAT: a screen point given on the 640 x 480 layout is scaled to the window (tutSetPointerTargetXY 0x0052ff70: x * width
//      / 640, y * height / 480, integer); the Right / BottomRight / Taskbar / FE setters place their point unscaled
static void catTutSetPointerXYRaw(char *name, sdword x, sdword y)
{
    tutpointer *pointer = tutPointerAllocate(name, TUT_PointerTypeXY);
    pointer->x = x;
    pointer->y = y;
}

void tutSetPointerTargetXY(char *name, sdword x, sdword y)
{
    catTutSetPointerXYRaw(name, (MAIN_WindowWidth * x) / 640, (MAIN_WindowHeight * y) / 480);
}

//sets the tutorial pointer relative to the right side of the screen
void tutSetPointerTargetXYRight(char *name, sdword x, sdword y)
{
    catTutSetPointerXYRaw(name, MAIN_WindowWidth+x-640, y);     // CAT: unscaled (0x00530120)
}

//sets the tutorial pointer relative to the bottom right corner of the screen
void tutSetPointerTargetXYBottomRight(char *name, sdword x, sdword y)
{
    catTutSetPointerXYRaw(name, MAIN_WindowWidth+x-640, MAIN_WindowHeight+y-480);   // CAT: unscaled (0x005301e0)
}

void tutSetPointerTargetXYTaskbar(char *name, sdword x, sdword y)
{
    catTutSetPointerXYRaw(name, feResRepositionCentredX(x), MAIN_WindowHeight+y-480);    // CAT: unscaled (0x005302b0)
}

void tutSetPointerTargetXYFE(char *name, sdword x, sdword y)
{
    catTutSetPointerXYRaw(name, feResRepositionCentredX(x), feResRepositionCentredY(y));   // CAT: unscaled (0x00530380)
}

// CAT (M2-R8 kasf library): Cataclysm's pointer setters take a third argument, TRUE for a pointer of the objective list
//      (kasfTutSetObjPointerTargetShip 0x004963c0 and its siblings pass it to tutSetPointerTargetShip and the others);
//      M2-R8 singleplayer-tutorial: kept in the pointer (+0x64, tutSetPointerTargetShip 0x00530450), so that
//      tutRemoveAllPointers leaves it
void tutSetPointerTargetShip(char *name, ShipPtr ship, bool32 objective)
{
    tutpointer *pointer = tutPointerAllocate(name, TUT_PointerTypeShip);
    pointer->ship = ship;
    pointer->objective = objective;
}

void tutSetPointerTargetShipSelection(char *name, SelectCommand *ships, bool32 objective)
{
    tutpointer *pointer = tutPointerAllocate(name, TUT_PointerTypeShips);
    pointer->selection = ships;
    pointer->objective = objective;                         // CAT: 0x00530500
}

void tutSetPointerTargetShipHealth(char *name, ShipPtr ship)
{
    tutpointer *pointer = tutPointerAllocate(name, TUT_PointerTypeShipHealth);
    pointer->ship = ship;
    tutPointerShip = ship;
    tutPointerShipHealthRect = &pointer->rect;
}

void tutSetPointerTargetShipGroup(char *name, ShipPtr ship)
{
    tutpointer *pointer = tutPointerAllocate(name, TUT_PointerTypeShipGroup);
    pointer->ship = ship;
    tutPointerShip = ship;
    tutPointerShipGroupRect = &pointer->rect;
}

void tutPlayerShipDied(ShipPtr ship)
{
    sdword index;

    for (index = 0; index < TUT_NumberPointers; index++)
    {
        if (tutPointer[index].ship == ship)
        {
            tutPointer[index].pointerType = TUT_PointerTypeNone;
            tutPointer[index].ship = NULL;
        }
    }
    if(ship == tutPointerShip)
    {
        //tutTextPointerType = TUT_PointerTypeNone;
        tutPointerShip = NULL;
        tutPointerShipGroupRect = tutPointerShipHealthRect = NULL;
    }
}

void tutSetPointerTargetFERegion(char *name, char *pAtomName)
{
    regionhandle    reg;

    reg = regFindChildByAtomName(&regRootRegion, pAtomName);

    if(reg)
    {
        tutpointer *pointer = tutPointerAllocate(name, TUT_PointerTypeRegion);
        pointer->rect = reg->rect;
        pointer->rect.x0 -= 1;
        pointer->rect.y0 -= 1;
        pointer->rect.y1 += 2;
    }
}

void tutSetPointerTargetRect(char *name, sdword x0, sdword y0, sdword x1, sdword y1)
{
    tutpointer *pointer = tutPointerAllocate(name, TUT_PointerTypeRegion);
    pointer->rect.x0 = x0;
    pointer->rect.y0 = y0;
    pointer->rect.x1 = x1;
    pointer->rect.y1 = y1;
}

void tutSetPointerTargetAIVolume(char *name, Volume *volume, bool32 objective)
{
    if (volume != NULL)
    {
        tutpointer *pointer = tutPointerAllocate(name, TUT_PointerTypeAIVolume);
        pointer->volume = volume;
        pointer->objective = objective;                     // CAT: 0x005308f0
    }
}

/*-----------------------------------------------------------------------------
    Name        : tutRemovePointerByName
    Description : Turn off a named pointer.
    Inputs      : name - name of pointer to remove
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void tutRemovePointerByName(char *name)
{
    sdword index;

    for (index = 0; index < TUT_NumberPointers; index++)
    {
        if (!strcasecmp(tutPointer[index].name, name))      // CAT: _stricmp (tutRemovePointerByName 0x005309a0)
        {
            tutPointer[index].pointerType = TUT_PointerTypeNone;
            if (tutPointer[index].ship == tutPointerShip)
            {
                tutPointerShip = NULL;
                tutPointerShipGroupRect = tutPointerShipHealthRect = NULL;
            }
            return;
        }
    }

#ifdef HW_BUILD_FOR_DEBUGGING
    dbgMessagef("tutRemovePointerByName: '%s' not found.", name);
#endif
}

/*-----------------------------------------------------------------------------
    Name        : tutRemoveAllPointers
    Description : Remove all pointers.  Gone.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void tutRemoveAllPointers(void)
{
    sdword index;

    for (index = 0; index < TUT_NumberPointers; index++)
    {
        // CAT: the objective list's pointers stay (tutRemoveAllPointers 0x00530a20 tests +0x64)
        if ((tutPointer[index].pointerType != TUT_PointerTypeNone) && !tutPointer[index].objective)
        {
            tutPointer[index].pointerType = TUT_PointerTypeNone;
            if (tutPointer[index].ship == tutPointerShip)
            {
                tutPointerShip = NULL;
                tutPointerShipGroupRect = tutPointerShipHealthRect = NULL;
            }
        }
    }
}

// HWI-TEST (M2-R8 singleplayer-tutorial): a named pointer is set (the mission self-test's tutorial leg; not in the exe)
bool32 tutPointerIsSet(char *name)
{
    sdword index;

    for (index = 0; index < TUT_NumberPointers; index++)
    {
        if ((tutPointer[index].pointerType != TUT_PointerTypeNone) && !strcasecmp(tutPointer[index].name, name))
        {
            return TRUE;
        }
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_tutPointerListReset                           0x0052f890
    Description : CAT (M2-R8 singleplayer-tutorial): every pointer is free and has no selection (universeReset
                  0x0053df80, tutStartup 0x00533140, tutLoadTutorialGame 0x0052f8b0)
----------------------------------------------------------------------------*/
void cat_tutPointerListReset(void)
{
    sdword index;

    for (index = 0; index < TUT_NumberPointers; index++)
    {
        tutPointer[index].pointerType = TUT_PointerTypeNone;
        tutPointer[index].selection = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_tutObjPointerXYSet                            0x00531290
    Description : CAT (M2-R8 singleplayer-tutorial): where the objective list is on screen (cat_moObjectivesDraw
                  0x004a76b0 gives its right edge and top every frame), for cat_tutSetPointerTargetObj
----------------------------------------------------------------------------*/
void cat_tutObjPointerXYSet(sdword x, sdword y)
{
    catTutObjPointerX = x;
    catTutObjPointerY = y;
}

// This function sets the current text display position and size
void tutSetTextDisplayBox(sdword x, sdword y, sdword width, sdword height, bool32 bScale)
{
    if(bScale)
    {
        x = (long)( ((real32)x / 640.0f) * (real32)MAIN_WindowWidth );
        y = (long)( ((real32)y / 480.0f) * (real32)MAIN_WindowHeight );
    }
    else
    {
        x += ((MAIN_WindowWidth - 640) / 2);
        y += ((MAIN_WindowHeight - 480) / 2);
    }

    tutTextPosX = x+5;
    tutTextPosY = y+5;
    tutTextSizeX = width-10;
    tutTextSizeY = height-10;

    tutTextRect.x0 = x;
    tutTextRect.y0 = y;
    tutTextRect.x1 = x + width;
    tutTextRect.y1 = y + height;
}

/*-----------------------------------------------------------------------------
    Name        : tutSetTextDisplayBoxToSubtitleRegion
    Description : Causes pointers to be clipped to the subtitle region
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void tutSetTextDisplayBoxToSubtitleRegion(void)
{
    tutTextRect = subRegion[STR_LetterboxBar].rect;
}

void tutRootDrawFunction(regionhandle reg)
{
    sdword index;
    regionhandle escapeRegion = NULL;

    if (reg->previous)
    {
        for (index = 0; index <= feStackIndex; index++)
        {
            if (feStack[index].screen != NULL)
            {
                // CAT: _stricmp (tutRootDrawFunction 0x00530b40)
                if (strcasecmp(feStack[index].screen->name, "Construction_manager") &&
                    strcasecmp(feStack[index].screen->name, "Launch_Manager") &&
                    strcasecmp(feStack[index].screen->name, "Research_Manager") &&
                    strcasecmp(feStack[index].screen->name, "Sensors_manager"))
                {                                           //if it's not one of the screens the turorial teaches
                    escapeRegion = feStack[index].baseRegion;
                    break;
                }
            }
        }
        //we want the tutorial stuff to be on top of all menu screens EXCEPT the in-game escape menu and it's minions
        if (escapeRegion == NULL)
        {                                                   //don't put it on top of the build manager or any other base-level manager.
            regSiblingMoveToFront(reg);
        }
    }
}

void tutAllocateRootRegion(void)
{
    if(tutRootRegionCount == 0)
    {
        tutRootRegion = regChildAlloc(ghMainRegion, (smemsize)&tutRootAtom,
            tutRootAtom.x, tutRootAtom.y, tutRootAtom.width, tutRootAtom.height, 0, RPE_DrawEveryFrame);

        tutRootAtom.region = (void*)tutRootRegion;
        tutRootRegion->atom = &tutRootAtom;
        tutRootAtom.pData = NULL;

        regDrawFunctionSet(tutRootRegion, tutRootDrawFunction);
    }

    tutRootRegionCount++;
}

void tutDeallocateRootRegion(void)
{
    tutRootRegionCount--;

    if(tutRootRegionCount == 0)
    {
        if(tutRootRegion)
            regRegionDelete(tutRootRegion);

        tutRootRegion = NULL;
    }
}

// This function will accept a string to be displayed in the current text box.
// The current text box is set up with the tutSetTextDisplayBox() function.
void tutShowText(char *szText)
{
fonthandle  currFont;
sdword      Height;
char        Line[256], *pString;

    if(tutTextVisible)
        tutHideText();

    if (MAIN_WindowWidth >= 1024)
    {
        tutTextFont = frFontRegister("ScrollingText.hff");
    }
    else
    {
        tutTextFont = frFontRegister("SimplixSSK_13.hff");
    }

    Height = 0;
    pString = szText;
    currFont = fontMakeCurrent(tutTextFont);

    do {
        pString = tutGetNextTextLine(Line, pString, tutTextSizeX, 256); // 256 defined above as size of Line
        Height += fontHeight(" ");
    } while(pString && pString[0]);

    fontMakeCurrent(currFont);

    tutAllocateRootRegion();

    tutTextAtom.x = tutTextPosX-5;
    tutTextAtom.y = tutTextPosY-5;
    tutTextAtom.width = tutTextSizeX + 10;
    tutTextAtom.height = max(Height, tutTextSizeY)+10;

    tutTextRect.x0 = tutTextPosX-5;
    tutTextRect.y0 = tutTextPosY-5;
    tutTextRect.x1 = tutTextRect.x0 + tutTextSizeX + 10;
    tutTextRect.y1 = tutTextRect.y0 + max(Height, tutTextSizeY) + 10;

    tutTextRegion = regChildAlloc(tutRootRegion, (smemsize)&tutTextAtom,
        tutTextAtom.x, tutTextAtom.y, tutTextAtom.width, tutTextAtom.height, 0, 0);

    tutTextAtom.region = (void*)tutTextRegion;
    tutTextRegion->atom = &tutTextAtom;
    tutTextAtom.pData = (ubyte *)szText;

    tutTextVisible = TRUE;

    regDrawFunctionSet(tutTextRegion, tutDrawTextFunction);
}

void tutHideText(void)
{
//    sdword index;

    if(tutTextRegion)
    {
        regRegionDelete(tutTextRegion);
        tutDeallocateRootRegion();
    }

    tutTextRegion = NULL;
    tutTextVisible = FALSE;

    tutRemoveAllPointers();

    //kills the speech associated with the text
    speechEventActorStop(CAT_SPEECH_ACTOR_FLEET_COMMAND, 0.1f);   // CAT: Fleet Command's bit (tutHideText 0x00530ef0)
}


/*-----------------------------------------------------------------------------
    Name        : tutIsspace
    Description : Returns TRUE if the character is a "space" character
    Inputs      : c - the character to test
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 tutIsspace(char c)
{
    if (c==' '||c=='\n')
    {
        return TRUE;
    }
    return FALSE;
}

// This function gets a line of text that is up to Width pixels wide, and returns a
// pointer to the start of the next line.  Assumes the current font is set.
char *tutGetNextTextLine(char *pDest, char *pString, long Width, int pDestSize)
{
long    WordLen, StringLen;
char    *pstr;
long    Done = FALSE;
char    temp;

    StringLen = 0;
    WordLen = 0;
    *pDest = 0;

    if(pString[0] == 0)
        return NULL;

    if (strlen(pString) < pDestSize) {
	memStrncpy(pDest, pString, strlen(pString) +1);
    }
    else {
	memStrncpy(pDest, pString, pDestSize -1);
	memStrncpy(pDest + pDestSize -1, '\0', 1);
    }

    do {
        // Skip leading whitespace
        pstr = &pDest[StringLen];
        while( *pstr && *pstr != '\n' && (*pstr == '-' || tutIsspace(*pstr)) )
        {
            WordLen++;
            pstr++;
        }

        if(*pstr && *pstr != '\n')
        {
            while( *pstr && *pstr != '\n' && *pstr != '-' && !tutIsspace(*pstr) )
            {
                WordLen++;
                pstr++;
            }

            temp = *pstr;
            *pstr = 0;
            if(fontWidth(pDest) > Width)
                Done = TRUE;
            else
            {
                StringLen += WordLen;
                WordLen = 0;
            }
            *pstr = temp;
        }
        else
        {
            Done = TRUE;
            StringLen += WordLen;
        }
    } while(!Done);

    if(StringLen)
    {
        // memStrncpy(pDest, pString, StringLen+1);
        pDest[StringLen] = 0;

        while( pString[StringLen] && tutIsspace(pString[StringLen]) )
            StringLen++;
    }
    else
        return NULL;

    return pString + StringLen;
}

static long PulseVal(long Val)
{
static long pulseDir = TUT_PointerPulseInc;

    Val += pulseDir;
    if(Val >= 255 || Val <= TUT_PointerPulseMin)
    {
        pulseDir = -pulseDir;
        Val += pulseDir;
    }
    return Val;
}

// CAT: the pointers' colour (tutDrawTextPointers 0x005312b0: red a quarter, green the pulse, blue two thirds; HW1 had
//      half, full, half; the XY and region pointers were grey)
#define CAT_TUT_POINTER_COLOUR(pulse)   colRGB((pulse) / 4, (pulse), ((pulse) * 2) / 3)

// long vector type for the clipper
typedef struct {
    sdword x,y;
} lvector;


// Clips segment to a horizontal line at y.
// Puts the intersection point in the return vector at *pDest
// returns true if clipped, false otherwise
sdword tutClipSegToHorizontal(lvector *pSeg, lvector *pDest, sdword y)
{
sdword dy0, dy1;
real32 t;

    dy0 = pSeg[1].y - pSeg[0].y;
    dy1 = y - pSeg[0].y;

    t = (real32)dy1 / dy0;

    if(t > 0.0f && t < 1.0f)
    {
        pDest->y = y;
        pDest->x = pSeg[0].x + (sdword)((real32)(pSeg[1].x - pSeg[0].x) * t + 0.5f);

        return 1;
    }
    else
        return 0;
}

// Clips segment to a vertical line at x.
// Puts the intersection point in the return vector at *pDest
// returns true if clipped, false otherwise
sdword tutClipSegToVertical(lvector *pSeg, lvector *pDest, sdword x)
{
sdword dx0, dx1;
real32 t;

    dx0 = pSeg[1].x - pSeg[0].x;
    dx1 = x - pSeg[0].x;

    t = (real32)dx1 / dx0;

    if(t > 0.0f && t < 1.0f)
    {
        pDest->x = x;
        pDest->y = pSeg[0].y + (sdword)((real32)(pSeg[1].y - pSeg[0].y) * t + 0.5f);

        return 1;
    }
    else
        return 0;
}


void tutClipSegToTextBox(sdword *x0, sdword *y0, sdword *x1, sdword *y1)
{
lvector ClipSeg[2];
lvector Clipped;

    ClipSeg[0].x = *x0; ClipSeg[0].y = *y0;
    ClipSeg[1].x = *x1; ClipSeg[1].y = *y1;

    if(tutClipSegToHorizontal(ClipSeg, &Clipped, tutTextRect.y0))
    {
        if(Clipped.x >= tutTextRect.x0 && Clipped.x <= tutTextRect.x1)
            ClipSeg[0] = Clipped;
    }

    if(tutClipSegToHorizontal(ClipSeg, &Clipped, tutTextRect.y1))
    {
        if(Clipped.x >= tutTextRect.x0 && Clipped.x <= tutTextRect.x1)
            ClipSeg[0] = Clipped;
    }

    if(tutClipSegToVertical(ClipSeg, &Clipped, tutTextRect.x0))
    {
        if(Clipped.y >= tutTextRect.y0 && Clipped.y <= tutTextRect.y1)
            ClipSeg[0] = Clipped;
    }

    if(tutClipSegToVertical(ClipSeg, &Clipped, tutTextRect.x1))
    {
        if(Clipped.y >= tutTextRect.y0 && Clipped.y <= tutTextRect.y1)
            ClipSeg[0] = Clipped;
    }

    *x0 = ClipSeg[0].x; *y0 = ClipSeg[0].y;
    *x1 = ClipSeg[1].x; *y1 = ClipSeg[1].y;
}



/*-----------------------------------------------------------------------------
    Name        : tutDrawTextPointers
    Description : Draw all active tutorial pointers
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void tutDrawTextPointers(rectangle *pRect)
{
    sdword  x0, y0, x1, y1, index;
    real32  x, y, rad, angle, dx, dy, magnitude;
    static long tutPulse = TUT_PointerPulseMin;
    //hmatrix modelview, projection;
    real32  sx, sy;
    tutpointer *pointer;
    color c;
    vector temp;
    rectangle box;

    // CAT: under the NIS letterbox the box's top is the bar's bottom + 5 (tutDrawTextPointers 0x005312b0)
    box = *pRect;
    if (nisFullyScissored)
    {
        box.y0 = (MAIN_WindowHeight - (MAIN_WindowWidth * 20) / 37) / 2 + 5;
    }
    for (pointer = tutPointer, index = 0; index < TUT_NumberPointers; index++, pointer++)
    {                                                       //for each pointer
        // CAT: an objective list's pointer is drawn only in the sensors manager, not while it zooms, with the
        //      objective pointers option on and no NIS letterbox
        if (pointer->objective && (smZoomingIn || smZoomingOut || !smSensorsActive || !catSmViewObjectivePointers ||
                                   (nisScissorFade > 0.0f) || nisFullyScissored))
        {
            continue;
        }
        switch(pointer->pointerType)
        {
            case TUT_PointerTypeNone:
                break;                                      //not active; ignore

            case TUT_PointerTypeXY:
                x0 = (box.x0 + box.x1) / 2;
                y0 = (box.y0 + box.y1) / 2;
                x1 = pointer->x;//tutPointerX;
                y1 = pointer->y;//tutPointerY;

                tutClipSegToTextBox(&x0, &y0, &x1, &y1);
                c = CAT_TUT_POINTER_COLOUR(tutPulse);
                primLine2(x0, y0, x1, y1, c);
                dx = (real32)(x0 - x1);                     //vector from arrowhead to source
                dy = (real32)(y0 - y1);
                if (dx != 0.0f || dy != 0.0f)
                {                                           //if not zero-length vector
                    magnitude = fmathSqrt(dx * dx + dy * dy);   //magnitude of vector
                    dx /= magnitude;
                    dy /= magnitude;                        //normalize the vector
                    if (dx == 0.0f)
                    {                                       //singularity: vertical axis
                        if (dy > 0.0f)
                        {
                            angle = PI / 2.0f;              //90
                        }
                        else
                        {
                            angle = PI * 3.0f / 2.0f;       //270
                        }
                    }
                    else if (dy > 0.0f)
                    {                                       //0..180
                        if (dx > 0.0f)
                        {                                   //0..90
                            angle = (real32)atan((double)(dy / dx));
                        }
                        else
                        {                                   //90..180
                            angle = PI - (real32)atan((double)(dy / -dx));
                        }
                    }
                    else
                    {                                       //180..360
                        if (dx < 0.0f)
                        {                                   //180..270
                            angle = (real32)atan((double)(dy / dx)) + PI;
                        }
                        else
                        {                                   //270..360
                            angle = 2.0 * PI - (real32)atan((double)(-dy / dx));
                        }
                    }
                    //now we have angle of main vector; offset that to get arrowhead vectors
                    x0 = x1 + (sdword)(cos((double)(angle - TUT_ArrowheadAngle)) * TUT_ArrowheadLength);
                    y0 = y1 + (sdword)(sin((double)(angle - TUT_ArrowheadAngle)) * TUT_ArrowheadLength);
                    primLine2(x0, y0, x1, y1, c);
                    x0 = x1 + (sdword)(cos((double)(angle + TUT_ArrowheadAngle)) * TUT_ArrowheadLength);
                    y0 = y1 + (sdword)(sin((double)(angle + TUT_ArrowheadAngle)) * TUT_ArrowheadLength);
                    primLine2(x0, y0, x1, y1, c);
                }
                break;

            case TUT_PointerTypeShip:
                rad = pointer->ship->impactable.collInfo.selCircleRadius;
                x = pointer->ship->impactable.collInfo.selCircleX;
                y = pointer->ship->impactable.collInfo.selCircleY;
                goto shipsCase;                             //rest of code is common

            case TUT_PointerTypeShips:
                selSelectionDimensions(&rndCameraMatrix, &rndProjectionMatrix, pointer->selection, &x, &y, &rad);
shipsCase:
                if(rad > 0.0)
                {                                           //if ship is on-screen
                    real32 sx, sy, deltx, delty, len;

                    rad = max(rad, TUT_ShipCircleSizeMin);
                    //x = pointer->ship->impactable.collInfo.selCircleX;
                    //y = pointer->ship->impactable.collInfo.selCircleY;

                    primGLCircleOutline2(x, y, rad, pieCircleSegmentsCompute(rad), CAT_TUT_POINTER_COLOUR(tutPulse));

                    // CAT: an objective list's pointer starts at the list (catTutObjPointerX / Y), the others at the box
                    if (pointer->objective)
                    {
                        sx = primScreenToGLX(catTutObjPointerX);
                        sy = primScreenToGLY(catTutObjPointerY);
                    }
                    else
                    {
                        sx = primScreenToGLX((box.x0 + box.x1) / 2);
                        sy = primScreenToGLY((box.y0 + box.y1) / 2);
                    }

                    deltx = x-sx;
                    delty = y-sy;

                    len = (real32)fsqrt(deltx*deltx + delty*delty);
                    len = (len - rad) / len;

                    if (len > 0.0f)
                    {
                        x0 = primGLToScreenX(sx);
                        y0 = primGLToScreenY(sy);

                        x1 = primGLToScreenX(sx + deltx * len);
                        y1 = primGLToScreenY(sy + delty * len);

                        tutClipSegToTextBox(&x0, &y0, &x1, &y1);
                        primLine2(x0, y0, x1, y1, CAT_TUT_POINTER_COLOUR(tutPulse));
                    }

                }
                break;

            case TUT_PointerTypeShipHealth:
                rad = pointer->ship->impactable.collInfo.selCircleRadius;
                if(rad > 0.0)
                {                                           //if ship on-screen
                    x0 = (box.x0 + box.x1) / 2;
                    y0 = (box.y0 + box.y1) / 2;

                    x1 = (pointer->rect.x0 + pointer->rect.x1) / 2;
                    y1 = pointer->rect.y0;

                    tutClipSegToTextBox(&x0, &x1, &y0, &y1);
                    primLine2(x0, y0, x1, y1, CAT_TUT_POINTER_COLOUR(tutPulse));
                    primRectOutline2(&pointer->rect, 1, CAT_TUT_POINTER_COLOUR(tutPulse));
                }
                break;

            case TUT_PointerTypeShipGroup:
                rad = pointer->ship->impactable.collInfo.selCircleRadius;
                if(rad > 0.0)
                {                                           //if ship on-screen
                    x0 = (box.x0 + box.x1) / 2;
                    y0 = (box.y0 + box.y1) / 2;

                    x1 = (pointer->rect.x0 + pointer->rect.x1) / 2;
                    y1 = pointer->rect.y0;

                    tutClipSegToTextBox(&x0, &y0, &x1, &y1);
                    primLine2(x0, y0, x1, y1, CAT_TUT_POINTER_COLOUR(tutPulse));
                    primRectOutline2(&pointer->rect, 1, CAT_TUT_POINTER_COLOUR(tutPulse));
                }
                break;

            case TUT_PointerTypeRegion:
                primRectOutline2(&pointer->rect, 3, CAT_TUT_POINTER_COLOUR(tutPulse));
                break;

            case TUT_PointerTypeAIVolume:
                temp = volFindCenter(pointer->volume);

                selCircleComputeGeneral(&rndCameraMatrix, &rndProjectionMatrix,
                    (vector *)&temp, volFindRadius(pointer->volume),
                    &sx, &sy, &rad);

                if(rad > 0.0)
                {
                    real32 x, y, deltx, delty, len;

                    rad = max(rad, TUT_ShipCircleSizeMin);
                    x = sx;
                    y = sy;
                    primGLCircleOutline2(x, y, rad, pieCircleSegmentsCompute(rad), CAT_TUT_POINTER_COLOUR(tutPulse));

                    // CAT: an objective list's pointer starts at the list (catTutObjPointerX / Y), the others at the box
                    if (pointer->objective)
                    {
                        sx = primScreenToGLX(catTutObjPointerX);
                        sy = primScreenToGLY(catTutObjPointerY);
                    }
                    else
                    {
                        sx = primScreenToGLX((box.x0 + box.x1) / 2);
                        sy = primScreenToGLY((box.y0 + box.y1) / 2);
                    }

                    deltx = x-sx;
                    delty = y-sy;

                    len = (real32)fsqrt(deltx*deltx + delty*delty);
                    len = (len - rad) / len;

                    x0 = primGLToScreenX(sx);
                    y0 = primGLToScreenY(sy);

                    x1 = primGLToScreenX(sx + deltx * len);
                    y1 = primGLToScreenY(sy + delty * len);

                    tutClipSegToTextBox(&x0, &y0, &x1, &y1);
                    primLine2(x0, y0, x1, y1, CAT_TUT_POINTER_COLOUR(tutPulse));
                }
                break;
        }
    }
    tutPulse = PulseVal(tutPulse);
    tutPointersDrawnThisFrame = TRUE;
}

// This function actually handles drawing the text in the region added by tutShowText
void tutDrawTextFunction(regionhandle reg)
{
char    *pString;
char    Line[256];
long    x, y, Width;
fonthandle  currFont;
featom  *pAtom = (featom *)reg->atom;
rectangle   rect;

    rect = reg->rect;
//    if(tutTextPointerType != TUT_PointerTypeNone)
    if (!tutPointersDrawnThisFrame)
    {
        tutDrawTextPointers(&rect);
    }

    // Draw the translucent blue box behind the text
    primRectTranslucent2(&rect, colRGBA(0, 0, 64, 192));

    currFont = fontMakeCurrent(tutTextFont);

    x = rect.x0+5;
    y = rect.y0+5;
    Width = (rect.x1 - rect.x0) - 10;

    pString = (char *)pAtom->pData;

    do {
        pString = tutGetNextTextLine(Line, pString, Width, 256); //256 defined above as size of Line
        if(Line[0])
            fontPrintf(x, y, colRGB(255, 255, 128), "%s", Line);
        y += fontHeight(" ");
    } while(pString && pString[0]);

    fontMakeCurrent(currFont);
}

/*-----------------------------------------------------------------------------
    CAT (M2-R8 singleplayer-tutorial): Cataclysm's tutorial buttons, in the row at the text box's bottom
    (tutTextRect.y1, 0x00a6bc14): Next lesson (128 x 32, 192 pixels left of the right edge), previous lesson (128 x 32,
    320 from it), quit (64 x 32 at the edge), next text (64 x 32 at the text box's left), back (128 x 32 at the text box's
    left, HW1's). Each is a button region under the tutorial's root (catTutButtonCreate: the exe inlines it per button)
----------------------------------------------------------------------------*/
static buttonhandle catTutButtonCreate(featom *atom, sdword x, sdword y, sdword width, sdword height,
                                       uicfunction process, regiondrawfunction draw)
{
    buttonhandle button;

    tutAllocateRootRegion();
    atom->x = x;
    atom->y = y;
    atom->width = width;
    atom->height = height;
    // [audit: M2-R8 singleplayer-tutorial] the button's tail past the region (the exe passes 0x1c; HW1's
    //      sizeof(buttonhandle) - sizeof(regionhandle) is two pointer sizes, 0, and processFunction lay past the block)
    button = (buttonhandle)regChildAlloc(tutRootRegion, (smemsize)atom, x, y, width, height,
                                         (sdword)(sizeof(uicbutton) - sizeof(region)), 0);
    atom->region = (void *)button;
    button->reg.atom = atom;
    button->reg.flags |= RPM_MouseEvents;
    atom->pData = NULL;
    button->reg.processFunction = uicButtonProcess;
    button->processFunction = process;
    regDrawFunctionSet((regionhandle)button, draw);
    return button;
}

static void catTutButtonDelete(buttonhandle *button)
{
    if (*button)
    {
        regRegionDelete((regionhandle)*button);
        tutDeallocateRootRegion();
    }
    *button = NULL;
}

// the textured button of the three states: pressed, under the mouse, off; drawn in the button row (0x00532820 ..)
static void catTutButtonDraw(regionhandle reg, sdword texOn, sdword texOff, sdword texMouse)
{
    sdword height = reg->rect.y1 - reg->rect.y0;
    udword texture;

    reg->rect.y0 = tutTextRect.y1;
    reg->rect.y1 = tutTextRect.y1 + height;
    texture = tutTexture[texOff];
    if ((reg->rect.x0 <= mouseCursorX()) && (reg->rect.y0 <= mouseCursorY()) && (mouseCursorX() < reg->rect.x1) &&
        (mouseCursorY() < reg->rect.y1))
    {
        texture = mouseLeftButton() ? tutTexture[texOn] : tutTexture[texMouse];
    }
    trRGBTextureMakeCurrent(texture);
    // CAT: rndPerspectiveCorrection(FALSE) is not issued (render.c: the hint has no effect on a current GL)
    primRectSolidTextured2(&reg->rect);
}

void tutShowNextButton(void)                                // CAT: 0x00532010
{
    if(tutNextVisible)
        tutHideNextButton();

    tutNextRegion = catTutButtonCreate(&tutNextAtom, MAIN_WindowWidth - 192, tutTextRect.y1, 128, 32,
                                       tutProcessNextButton, tutDrawNextButtonFunction);
    tutNextVisible = TRUE;
}

void tutHideNextButton(void)
{
    catTutButtonDelete(&tutNextRegion);
    tutNextVisible = FALSE;
}

udword tutProcessNextButton(struct tagRegion *reg, smemsize ID, udword event, udword data)
{
    // CAT: a click counts only while the main view or the sensors manager shows (tutProcessNextButton 0x00532750)
    if ((event == CM_ButtonClick) && (mrRenderMainScreen || smSensorsActive))
    {
        tutNextButtonState = 1;
        return 1;
    }

    return 0;
}

void tutDrawNextButtonFunction(regionhandle reg)            // CAT: 0x00532820: the next-lesson art
{
    if ((mrRenderMainScreen || smSensorsActive) && (tutTextRect.y1 != 0))
    {
        catTutButtonDraw(reg, TUT_NEXTLESSON_ON, TUT_NEXTLESSON_OFF, TUT_NEXTLESSON_MOUSE);
    }
}

sdword tutNextButtonClicked(void)
{
sdword  RetVal;

    RetVal = tutNextButtonState;
    tutNextButtonState = 0;

    return RetVal;
}


void tutShowBackButton(void)                                // CAT: 0x00532ac0
{
    if(tutBackVisible)
    {
        tutPrevVisible = FALSE;
        tutHideBackButton();
    }

    tutBackRegion = catTutButtonCreate(&tutBackAtom, tutTextRect.x0, tutTextRect.y1, 128, 32,
                                       tutProcessBackButton, tutDrawBackButtonFunction);
    tutBackVisible = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : tutShowPrevButton                                 0x00532300
    Description : CAT: the previous-lesson button is its own region ("TutorialPrev", 320 pixels left of the right edge,
                  the restart art; its click sets the flag cat_tutPrevButtonClicked reads); HW1 showed the back button
----------------------------------------------------------------------------*/
void tutShowPrevButton(void)
{
    if (tutPrevVisible)
    {
        catTutButtonDelete(&catTutPrevRegion);
        tutPrevVisible = FALSE;
    }
    catTutPrevRegion = catTutButtonCreate(&catTutPrevAtom, MAIN_WindowWidth - 320, tutTextRect.y1, 128, 32,
                                          cat_tutProcessPrevButton, cat_tutDrawPrevButtonFunction);
    tutPrevVisible = TRUE;
}

/*-----------------------------------------------------------------------------
    CAT (M2-R8 singleplayer-tutorial): the next-text (0x00532190 ..), previous (0x005326b0 ..) and quit (0x005324a0 ..)
    buttons
-----------------------------------------------------------------------------*/
void cat_tutShowNextTextButton(void)                        // 0x00532190
{
    if (catTutNextTextVisible)
    {
        cat_tutHideNextTextButton();
    }
    catTutNextTextRegion = catTutButtonCreate(&catTutNextTextAtom, tutTextRect.x0, tutTextRect.y1, 64, 32,
                                              cat_tutProcessNextTextButton, cat_tutDrawNextTextButtonFunction);
    catTutNextTextVisible = TRUE;
}

void cat_tutHideNextTextButton(void)                        // 0x00532660
{
    catTutButtonDelete(&catTutNextTextRegion);
    catTutNextTextVisible = FALSE;
}

void cat_tutHidePrevButton(void)                            // 0x005326b0
{
    catTutButtonDelete(&catTutPrevRegion);
    tutPrevVisible = FALSE;
}

void cat_tutShowQuitButton(void)                            // 0x005324a0
{
    if (catTutQuitVisible)
    {
        cat_tutHideQuitButton();
    }
    catTutQuitRegion = catTutButtonCreate(&catTutQuitAtom, MAIN_WindowWidth - 64, tutTextRect.y1, 64, 32,
                                          cat_tutProcessQuitButton, cat_tutDrawQuitButtonFunction);
    catTutQuitVisible = TRUE;
}

void cat_tutHideQuitButton(void)                            // 0x00532700
{
    catTutButtonDelete(&catTutQuitRegion);
    catTutQuitVisible = FALSE;
}

// 0x00532780: the next-text click also runs key command 0x6b (the subtitles' next line)
static udword cat_tutProcessNextTextButton(struct tagRegion *reg, smemsize ID, udword event, udword data)
{
    if (event == CM_ButtonClick)
    {
        catTutNextTextState = 1;
        cat_kbCommandExecute(0, 0x6b);
        return 1;
    }
    return 0;
}

// 0x005327b0
static udword cat_tutProcessPrevButton(struct tagRegion *reg, smemsize ID, udword event, udword data)
{
    if ((event == CM_ButtonClick) && (mrRenderMainScreen || smSensorsActive))
    {
        catTutPrevState = 1;
        return 1;
    }
    return 0;
}

// 0x005327e0: the quit click leaves the game for the main menu at once
static udword cat_tutProcessQuitButton(struct tagRegion *reg, smemsize ID, udword event, udword data)
{
    if ((event == CM_ButtonClick) && (mrRenderMainScreen || smSensorsActive))
    {
        catTutQuitState = 1;
        utyGameQuitToMain(NULL, NULL);
        return 1;
    }
    return 0;
}

static void cat_tutDrawNextTextButtonFunction(regionhandle reg)     // 0x005328c0 (drawn over the managers too)
{
    if (tutTextRect.y1 != 0)
    {
        catTutButtonDraw(reg, TUT_NEXT_ON, TUT_NEXT_OFF, TUT_NEXT_MOUSE);
    }
}

static void cat_tutDrawPrevButtonFunction(regionhandle reg)         // 0x00532940
{
    if ((mrRenderMainScreen || smSensorsActive) && (tutTextRect.y1 != 0))
    {
        catTutButtonDraw(reg, TUT_REST_ON, TUT_REST_OFF, TUT_REST_MOUSE);
    }
}

static void cat_tutDrawQuitButtonFunction(regionhandle reg)         // 0x005329e0
{
    if ((mrRenderMainScreen || smSensorsActive) && (tutTextRect.y1 != 0))
    {
        catTutButtonDraw(reg, TUT_QUIT_ON, TUT_QUIT_OFF, TUT_QUIT_MOUSE);
    }
}

sdword cat_tutNextTextButtonClicked(void)                   // 0x00532a90
{
    sdword clicked = catTutNextTextState;

    catTutNextTextState = 0;
    return clicked;
}

sdword cat_tutPrevButtonClicked(void)                       // 0x00532aa0
{
    sdword clicked = catTutPrevState;

    catTutPrevState = 0;
    return clicked;
}

sdword cat_tutQuitButtonClicked(void)                       // 0x00532ab0
{
    sdword clicked = catTutQuitState;

    catTutQuitState = 0;
    return clicked;
}

/*-----------------------------------------------------------------------------
    Name        : cat_tutButtonsHideForScreen / cat_tutButtonsRestoreAfterScreen     0x005341f0 / 0x00534300
    Description : CAT (M2-R8 singleplayer-tutorial): a manager screen hides the Next, previous and quit buttons while
                  it is open (the construction, launch and research managers' begin functions) and shows the ones that
                  were up when it closes
----------------------------------------------------------------------------*/
void cat_tutButtonsHideForScreen(void)
{
    catTutSavedNextVisible = tutNextVisible;
    catTutSavedPrevVisible = tutPrevVisible;
    catTutSavedQuitVisible = catTutQuitVisible;
    if (tutNextVisible)
    {
        tutHideNextButton();
    }
    if (tutPrevVisible)
    {
        cat_tutHidePrevButton();
    }
    if (catTutQuitVisible)
    {
        cat_tutHideQuitButton();
    }
}

void cat_tutButtonsRestoreAfterScreen(void)
{
    if (catTutSavedNextVisible)
    {
        tutShowNextButton();
    }
    if (catTutSavedPrevVisible)
    {
        tutShowPrevButton();
    }
    if (catTutSavedQuitVisible)
    {
        cat_tutShowQuitButton();
    }
}

void tutHideBackButton(void)                                // CAT: 0x00532c40 (the previous button's flag cleared too)
{
    catTutButtonDelete(&tutBackRegion);
    tutBackVisible = FALSE;
    tutPrevVisible = FALSE;
}

udword tutProcessBackButton(struct tagRegion *reg, smemsize ID, udword event, udword data)
{
    if(event == CM_ButtonClick)
    {
        if((tutPrevVisible) && (tutLastLessonName[0]))
            strcpy(tutCurrLessonName, tutLastLessonName);

        if (fileExists(tutCurrLessonName, 0) && (VerifySaveFile(tutCurrLessonName) == VERIFYSAVEFILE_OK))
        {
            tutEnableEverything();
            spMainScreen(TRUE);
            tbForceTaskbar(FALSE);                  // CAT: kept, the exe calls it (0x00532d48 -> tbForceTaskbar 0x0052b3b0)
            gameEnd();

            tutorial = 1;

            utyLoadSinglePlayerGameGivenFilename(tutCurrLessonName);
            return 1;
        }
    }

    return 0;
}

void tutDrawBackButtonFunction(regionhandle reg)            // CAT: 0x00532d80: always the restart art, where it was put
{
    udword texture = tutTexture[TUT_REST_OFF];

    if ((reg->rect.x0 <= mouseCursorX()) && (reg->rect.y0 <= mouseCursorY()) && (mouseCursorX() < reg->rect.x1) &&
        (mouseCursorY() < reg->rect.y1))
    {
        texture = mouseLeftButton() ? tutTexture[TUT_REST_ON] : tutTexture[TUT_REST_MOUSE];
    }
    trRGBTextureMakeCurrent(texture);
    // CAT: rndPerspectiveCorrection(FALSE) is not issued (render.c: the hint has no effect on a current GL)
    primRectSolidTextured2(&reg->rect);
}


// Returns the next "StartFrom" value, and copies the token into pDest
long GetNextCommaDelimitedToken(char *pString, char *pDest, long StartFrom)
{
static char *pSrc = NULL;
long Index = 0, Len;

    if(pString)
    {
        pSrc = pString;
        Index = 0;
    }

    if(pSrc)
    {
        Index = StartFrom;
        Len = 0;

        while(pSrc[Index] && pSrc[Index] != ',' && pSrc[Index] != 0x0a && pSrc[Index] != 0x0d)
        {
            pDest[Len] = pSrc[Index];
            Len++;
            Index++;
        }
        pDest[Len] = 0;

        if(pSrc[Index] == ',')
            Index++;
        else
            pSrc = NULL;
    }
    else
        return 0;

    return Index;
}

long tutFindTokenIndex(char *pTokenList[], char *pToken)
{
long    i;

    i = 0;
    while(pTokenList[i][0])
    {
        if(strcasecmp(pTokenList[i], pToken) == 0)
            return i;
        i++;
    }

    return -1;
}

long tutParseImagesIntoIndices(char *szImages)
{
char    szToken[256];
long    StrIndex, Count, TokenIndex;

    Count = 0;
    StrIndex = GetNextCommaDelimitedToken(szImages, szToken, 0);
    while(StrIndex)
    {
        TokenIndex = tutFindTokenIndex(tutImageList, szToken);

        if(TokenIndex != -1)
            szImageIndexList[Count] = TokenIndex;
        else
            szImageIndexList[Count] = 0;

        Count++;
        StrIndex = GetNextCommaDelimitedToken(NULL, szToken, StrIndex);
    }

    szImageIndexList[Count] = 0;
    return Count;
}


/*
This function will accept a comma delimited list of images to be displayed, in
order of appearance in this list, from left to right on the screen.  The image
list is included in the header.
*/
void tutShowImages(char *szImages)
{
long    ImageCount;

    if(tutImageVisible)
        tutHideImages();

    tutAllocateRootRegion();

    ImageCount = tutParseImagesIntoIndices(szImages);

    tutImageAtom.x = MAIN_WindowWidth - ImageCount * 64;
    tutImageAtom.y = MAIN_WindowHeight - 64;
    tutImageAtom.width = ImageCount * 64;
    tutImageAtom.height = 64;

    tutImageRegion = regChildAlloc(tutRootRegion, (smemsize)&tutImageAtom,
        tutImageAtom.x, tutImageAtom.y, tutImageAtom.width, tutImageAtom.height, 0, 0);

    tutImageAtom.region = (void*)tutImageRegion;
    tutImageRegion->atom = &tutImageAtom;
    tutImageAtom.pData = (ubyte *) szImages;

    tutImageVisible = TRUE;

    regDrawFunctionSet(tutImageRegion, tutDrawImageFunction);
}

void tutDrawImageFunction(regionhandle reg)
{
rectangle   rect;
long    i, Index;
long    x, y;

    x = reg->rect.x0;
    y = reg->rect.y0;

//  glEnable(GL_BLEND);
    i = 0;

    while(szImageIndexList[i])
    {
        Index = szImageIndexList[i];

        trRGBTextureMakeCurrent(tutTexture[Index]);
        rect.x0 = x + (64 * i);
        rect.y0 = y;
        rect.x1 = x + (64 * i) + 64;
        rect.y1 = y + 64;
        primRectSolidTextured2(&rect);

        i++;
    }
//  glDisable(GL_BLEND);
}


void tutHideImages(void)
{
    if(tutImageRegion)
    {
        regRegionDelete(tutImageRegion);
        tutDeallocateRootRegion();
    }

    tutImageRegion = NULL;
    tutImageVisible = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : tutStartup
    Description : tutorial init function called at game startup
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void tutStartup(void)                                       // CAT: 0x00533140
{
    sdword index;

    for (index = 0; tutImageList[index][0]; index++)
    {
        tutTexture[index] = TR_InvalidInternalHandle;
        tutImage[index] = NULL;
    }
    cat_tutPointerListReset();                              // CAT: and the pointers (inlined)
}

/*-----------------------------------------------------------------------------
    Name        : tutShutdown
    Description : Shutdown function called at game shutdown time
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void tutShutdown(void)
{
    ;
}

/*-----------------------------------------------------------------------------
    Name        : tutInitialize
    Description : Tutorial init function called once for every tutorial load
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void tutInitialize(void)
{
    long    i;
    char    Filename[256];

    i = 0;

    while( tutImageList[i][0] )
    {
        if (tutTexture[i] == TR_InvalidInternalHandle)
        {
            dbgAssertOrIgnore(tutImage[i] == NULL);
#ifdef _WIN32
            strcpy(Filename, "feman\\texdecorative\\");
#else
            strcpy(Filename, "feman/texdecorative/");
#endif

            //load the correct texture depending on language
            if (strCurLanguage == languageEnglish)
            {
                strcat(Filename, tutImageList[i]);
            }
            else if (strCurLanguage == languageFrench)
            {
                strcat(Filename, tutFrenchImageList[i]);
            }
            else if (strCurLanguage == languageGerman)
            {
                strcat(Filename, tutGermanImageList[i]);
            }
            else if (strCurLanguage == languageSpanish)
            {
                strcat(Filename, tutSpanishImageList[i]);
            }
            else if (strCurLanguage == languageItalian)
            {
                strcat(Filename, tutItalianImageList[i]);
            }
            else
            {
                //defaults to english
                strcat(Filename, tutImageList[i]);
            }

            strcat(Filename, ".LiF");
            tutImage[i] = trLIFFileLoad(Filename, NonVolatile);

            dbgAssertOrIgnore(tutImage[i] != NULL);
            tutTexture[i] = trRGBTextureCreate((color *)tutImage[i]->data, tutImage[i]->width, tutImage[i]->height, TRUE);
            i++;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : tutUnInitialize
    Description : Tutorial shutdown function called at the end of a tutorial game
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    Name        : tutUnInitialize                                   0x005332c0
    Description : CAT: the text, images and every button go (Cataclysm's next-text, previous and quit too), the images
                  are freed, everything enabled, the trade manager's technologies cleared and the KAS watch function
                  dropped (CurrentMissionWatchFunction 0x00a43ee4)
----------------------------------------------------------------------------*/
void tutUnInitialize(void)
{
long    i;

    tutHideText();
    tutHideImages();
    tutHideNextButton();
    cat_tutHidePrevButton();
    cat_tutHideQuitButton();
    tutHideBackButton();
    cat_tutHideNextTextButton();

    i = 0;
    while( tutImageList[i][0] )
    {
        if(tutImage[i] != NULL)
        {
            memFree(tutImage[i]);
            tutImage[i] = NULL;
        }

        if (tutTexture[i] != TR_InvalidInternalHandle)
        {
            trRGBTextureDelete(tutTexture[i]);
            tutTexture[i] = TR_InvalidInternalHandle;
        }
        i++;
    }
    tutEnableEverything();
    tmClearTechs();
    CurrentMissionWatchFunction = NULL;
}

void tutSetFlagIndex(sdword Index, sdword Val)
{
sdword *pFlagMem;
sdword FlagBit;

    //hack code to get speech to work when game paused
    if (Index == 1)
    {
        FalkosFuckedUpTutorialFlag = !Val;
    }

    pFlagMem = (sdword *)&tutEnable;
	pFlagMem += Index/32;

    // CAT: an unsigned shift (the exe's shl of 1; flag 31 is ContextFormX since the exe's layout, M2-R8)
#if FIX_ENDIAN
	FlagBit = (sdword)(1u << (31 - Index & 31));
#else
	FlagBit = (sdword)(1u << (Index & 31));
#endif

    if(Val)
        *pFlagMem |= FlagBit;
    else
        *pFlagMem &= ~FlagBit;
}

void tutEnableEverything(void)
{
long    i;

    for(i=0; i<sizeof(tutGameEnableFlags)*8; i++)
        tutSetFlagIndex(i, 1);
}

void tutDisableEverything(void)
{
long    i;

    for(i=0; i<sizeof(tutGameEnableFlags)*8; i++)
        tutSetFlagIndex(i, 0);
}

void tutSetEnableFlags(char *pFlagString, long Val)
{
char    szToken[256];
long    StrIndex, TokenIndex;

    StrIndex = GetNextCommaDelimitedToken(pFlagString, szToken, 0);
    while(StrIndex)
    {
        TokenIndex = tutFindTokenIndex(tutGameEnableString, szToken);
        tutSetFlagIndex(TokenIndex, Val);
        StrIndex = GetNextCommaDelimitedToken(NULL, szToken, StrIndex);
    }
}

void tutBuilderSetRestrictions(char *pShipTypes, bool32 bRestricted)
{
ShipType    st;
char    szToken[256];
long    StrIndex;

    StrIndex = GetNextCommaDelimitedToken(pShipTypes, szToken, 0);
    while(StrIndex)
    {
        st = StrToShipType(szToken);
        tutBuildRestrict[st] = bRestricted;

        StrIndex = GetNextCommaDelimitedToken(NULL, szToken, StrIndex);
    }

    // usually BuildArrows are on when the player can build things
    // but the arrows can be disabled seperately if needed
    tutSetEnableFlags("BuildArrows", !bRestricted);
}

void tutBuilderRestrictAll(void)
{
int i;

    for(i=0; i<TOTAL_STD_SHIPS; i++)
        tutBuildRestrict[i] = 1;

    tutSetEnableFlags("BuildArrows", 0);
}

void tutBuilderRestrictNone(void)
{
int i;

    for(i=0; i<TOTAL_STD_SHIPS; i++)
        tutBuildRestrict[i] = 0;

    tutSetEnableFlags("BuildArrows", 1);
}

sdword tutIsBuildShipRestricted(sdword shipType)
{
    return tutBuildRestrict[shipType];
}

sdword tutSelectedContainsShipType(ShipType st)
{
long i, j;

    for(i=0, j=0; i<selSelected.numShips; i++)
    {
        if(selSelected.ShipPtr[i]->shiptype == st)
            j++;
    }
    return j;
}

sdword tutSelectedContainsShipTypes(char *pShipTypes)
{
ShipType    st;
char    szToken[256];
long    StrIndex, Count;

    Count = 0;
    StrIndex = GetNextCommaDelimitedToken(pShipTypes, szToken, 0);
    while(StrIndex)
    {
        st = StrToShipType(szToken);

        Count += tutSelectedContainsShipType(st);

        StrIndex = GetNextCommaDelimitedToken(NULL, szToken, StrIndex);
    }

    return Count;
}

void tutGameMessage(char *eventName)
{
    // CAT: only in the tutorial itself (tutorial 1), and every message is queued while there is room
    //      (tutGameMessage 0x00533e20; GoK queued in any single-player game and skipped a message already queued)
    if (tutorial != 1)
    {
        return;
    }

#ifdef HW_BUILD_FOR_DEBUGGING
    dbgMessagef("%s: '%s'", __func__, eventName);
#endif

    if (tutGameMessageIndex < 16)
    {
        strcpy(tutGameMessageList[ tutGameMessageIndex ], eventName);
        tutGameMessageIndex++;
    }
}

bool32 tutGameSentMessage(char *eventNames)
{
    // CAT (M2-R9 missions-10-17): the queue is only read (tutGameSentMessage 0x00533e80 reads the count 0x00a6c2c4 and
    //      never writes it): Tutorial1's lessons test several messages in one watch and clear the queue themselves
    //      (TutResetGameMessageQueue at the end, Watch_Tutorial1_Tutorial_Lesson3 0x0066dca0 ..); HW1 / GoK cleared it
    //      after every test, so only a lesson's first test could see a message
    return tutGameMessageInQueue(eventNames);
}

bool32 tutGameMessageInQueue(char *eventNames)
{
    udword  i;
    char    szToken[256];
    long    StrIndex;

    StrIndex = GetNextCommaDelimitedToken(eventNames, szToken, 0);
    while (StrIndex)
    {
        for (i=0; i<tutGameMessageIndex; i++)
        {
            if (strcasecmp(szToken, tutGameMessageList[i]) == 0)
            {
                return TRUE;
            }
        }
        StrIndex = GetNextCommaDelimitedToken(NULL, szToken, StrIndex);
    }

    return FALSE;
}

void tutResetGameMessageQueue(void)
{
    tutGameMessageIndex = 0;
}

sdword tutContextMenuDisplayedForShipType(char *pShipType)
{
    ShipType st;

    st = StrToShipType(pShipType);
    return (tutFEContextMenuShipType == st);
}

void tutResetContextMenuShipTypeTest(void)
{
    tutFEContextMenuShipType = (ShipType)-1;
}


sdword tutBuildManagerShipTypeInBatchQueue(char *pShipType)
{
    long        i;
    ShipType    st;

    st = StrToShipType(pShipType);
    for (i=0; cmShipsAvailable[i].nJobs != -1; i++)
    {
        if(cmShipsAvailable[i].type == st)
            return cmShipsAvailable[i].nJobs;
    }

    return 0;
}

sdword tutBuildManagerShipTypeInBuildQueue(char *pShipType)
{
    long        index;
    ShipType    st;
    Node        *node;
    Ship        *factoryship;
    shipsinprogress *sinprogress;
    shipinprogress  *progress;

    st = StrToShipType(pShipType);

    node = listofShipsInProgress.head;
    while (node != NULL)
    {
        sinprogress = (shipsinprogress *)listGetStructOfNode(node);
        factoryship = sinprogress->ship;

        if (factoryship->playerowner == universe.curPlayerPtr)
        {
            for (index = 0, progress = &sinprogress->progress[0]; index < TOTAL_STD_SHIPS; index++, progress++)
            {
                if(progress->info && progress->info->shiptype == st)
                    return progress->nJobs;
            }
        }
        node = node->next;
    }

    return 0;
}

//external function definition
shipinprogress *cmSIP(udword index);

/*-----------------------------------------------------------------------------
    Name        : tutBuildManagerShipTypeSelected
    Description : Returns TRUE if the shiptype is selected in the build manager
    Inputs      : pShipType - string of shiptypes
    Outputs     :
    Return      : TRUE or FALSE
----------------------------------------------------------------------------*/

sdword tutBuildManagerShipTypeSelected(char *pShipType)
{
    ShipType shiptype;
    shipinprogress *shipprog;

    shiptype = StrToShipType(pShipType);
    shipprog = cmSIP(shiptype);

    if (shipprog->selected)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}



long tutCameraFocusShipCount(ShipType st)
{
long Count = 0;
CameraStackEntry *entry;
long    i;

    entry = currentCameraStackEntry(&universe.mainCameraCommand);

    for(i=0; i<entry->focus.numShips; i++)
    {
        if(entry->focus.ShipPtr[i]->shiptype == st)
            Count++;
    }
    return Count;
}


sdword tutCameraFocusedOnShipType(char *pShipTypes)
{
char    szToken[256];
long    StrIndex, Count;
ShipType st;

    if (smSensorsActive)
    {
        //camera can't be focused on a ship in the Sensors Manager
        return 0;
    }
    Count = 0;
    StrIndex = GetNextCommaDelimitedToken(pShipTypes, szToken, 0);
    while(StrIndex)
    {
        st = StrToShipType(szToken);
        Count += tutCameraFocusShipCount(st);
        StrIndex = GetNextCommaDelimitedToken(NULL, szToken, StrIndex);
    }
    return Count;
}

/*-----------------------------------------------------------------------------
    Name        : cat_tutButtonRowSet (Cataclysm 0x00532480)
    Description : In the tutorial, place the tutorial's button row under a subtitle picture
    Inputs      : y - the picture's bottom (subTitlesDraw 0x00584620)
    Outputs     : tutTextRect.y1 (0x00a6bc14): the row's top, where the tutorial's buttons are placed and drawn
    Return      : the bottom of the row (y + 32) in the tutorial, else y
----------------------------------------------------------------------------*/
sdword cat_tutButtonRowSet(sdword y)
{
    if (tutorial == 1)
    {
        tutTextRect.y1 = y;
        y += 32;
    }
    return y;
}

/*-----------------------------------------------------------------------------
    Name        : cat_tutSetPointerTargetObj                        0x00530060
    Description : CAT (M2-R8 singleplayer-tutorial): a named pointer at the objective list (the point
                  cat_tutObjPointerXYSet keeps: its right edge and top), not an objective pointer itself (KAS
                  TutSetPointerTargetObj 0x004964d0)
----------------------------------------------------------------------------*/
void cat_tutSetPointerTargetObj(char *name)
{
    tutpointer *pointer = tutPointerAllocate(name, TUT_PointerTypeXY);

    pointer->x = catTutObjPointerX;
    pointer->y = catTutObjPointerY;
}

/*-----------------------------------------------------------------------------
    Name        : cat_tutSelectShips                                0x00533ca0
    Description : CAT (M2-R8 kasf library): the ships become the current selection (kasfTutSelectShips 0x0049a350)
----------------------------------------------------------------------------*/
void cat_tutSelectShips(MaxSelection *ships)
{
    sdword i;

    while (selSelected.numShips != 0)
    {
        selSelectionRemoveSingleShip(&selSelected, selSelected.ShipPtr[0]);
    }
    selSelected.numShips = 0;
    for (i = 0; i < ships->numShips; i++)
    {
        selSelectionAddSingleShip(&selSelected, ships->ShipPtr[i]);
    }
}
