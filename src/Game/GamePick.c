/*=============================================================================
    Name    : GamePick.c
    Purpose : Functionality to choose a game

    Created 10/21/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "GamePick.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "CommandDefs.h"
#include "CommandWrap.h"
#include "Debug.h"
#include "FEColour.h"
#include "File.h"
#include "font.h"
#include "FontReg.h"
#include "Globals.h"
#include "mainrgn.h"
#include "mainswitches.h"
#include "Memory.h"
#include "MultiplayerGame.h"
#include "NetCheck.h"
#include "prim2d.h"
#include "SaveGame.h"
#include "Scroller.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "StringsOnly.h"
#include "Universe.h"
#include <stdio.h>
#include "StringSupport.h"
#include "GameStats.h"
#include "Key.h"
#include "glinc.h"
#include "render.h"
#include "Teams.h"
#include "SpeechEvent.h"
#include "Sensors.h"
#include "ResearchAPI.h"
#include "PopupMenu.h"
#include "HorseRace.h"
#include "Alliance.h"
#include "mouse.h"
#include "OverlayIcons.h"
#include "Timer.h"
#include "utility.h"

#include "CatSort.h"         // CAT (M2-R9 oracle-rung-physics): the exe's qsort
#ifdef _WIN32
    #include <windows.h>
#endif

#ifdef _MSC_VER
    #include <io.h>
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
#else
    #include <dirent.h>
    #include <strings.h>
#endif

/*=============================================================================
    Data:
=============================================================================*/

bool32 gpQuickSave(void);
bool32 gpQuickLoad(void);

void gpGameWindowInit(char *name, featom *atom);
void gpTextEntryWindowInit(char *name, featom *atom);
void gpGameNameDraw(featom *atom, regionhandle region);
void gpSaveGivenGame(char* gamename);
void gpLoadGivenGame(char* gamename);
void gpSaveGame(char *name, featom *atom);
void gpLoadGame(char *name, featom *atom);
void gpLoadTheRecordedGame(char *name, featom *atom);
void gpSaveTheRecordedGame(char *name, featom *atom);
void gpDeleteGame(char *name, featom *atom);
void gpBackToInGameEscapeMenu(char *name, featom *atom);
void gpStopPlayback(char *name, featom *atom);
void gpStopRecording(char *name, featom *atom);

void gpOverwriteYes(char *name, featom *atom);
void gpOverwriteNo(char *name, featom *atom);

bool32 gpLoadSinglePlayerGame = FALSE;
bool32 gpLoadTutorial = FALSE;

#ifdef _WIN32
//SinglePlayerSavedGamesPath is non-static because it is used in KASFunc.c
char SinglePlayerSavedGamesPath[] = "SavedGames\\SinglePlayer\\";
static char MultiPlayerSavedGamesPath[] = "SavedGames\\MultiPlayer\\";
static char RecordedGamesPath[] = "SavedGames\\RecordedGames\\";
//TutorialSavedGamesPath is non-static because it is used in Tutor.c
char TutorialSavedGamesPath[] = "SavedGames\\Training\\";
#else

//SinglePlayerSavedGamesPath is non-static because it is used in KASFunc.c
#if defined(HW_GAME_RAIDER_RETREAT)
char SinglePlayerSavedGamesPath[] = "SavedGames/Raider\ Retreat/";
#else
char SinglePlayerSavedGamesPath[] = "SavedGames/SinglePlayer/";
#endif

static char MultiPlayerSavedGamesPath[] = "SavedGames/MultiPlayer/";
static char RecordedGamesPath[] = "SavedGames/RecordedGames/";
//TutorialSavedGamesPath is non-static because it is used in Tutor.c
char TutorialSavedGamesPath[] = "SavedGames/Training/";
#endif
static char QuickSaveName[] = "Quick Save";

char *SavedGamesPath = NULL;

fecallback gpCallbacks[] =
{
    {gpGameWindowInit,         "FE_RecordedGameWindowInit"},
    {gpGameWindowInit,         "FE_GameWindowInit"},
    {gpGameWindowInit,         "FE_TutorialGameWindowInit"},
    {gpTextEntryWindowInit,    "IG_TextEntryWindowInit"},
    {gpStopPlayback,           "IG_StopPlayback"},
    {gpStopRecording,          "IG_StopRecording"},
    {gpLoadGame,               "IG_LoadGame"},
    {gpLoadTheRecordedGame,    "LoadRecordedGame"},
    {gpSaveTheRecordedGame,    "IG_SaveRecordedGame"},
    {gpSaveGame,               "IG_SaveGame"},
    {gpDeleteGame,             "IG_DeleteGame"},
    {gpBackToInGameEscapeMenu, "IG_StartEscapeMenu"},
    {gpOverwriteYes,           "OverwriteGameYes" },
    {gpOverwriteNo,            "OverwriteGameNo" },
    {NULL, NULL}
};

fedrawcallback gpDrawCallbacks[] =
{
    {gpGameNameDraw, "FE_GameName"},
    {NULL, NULL}
};

listwindowhandle gpGameListWindow = NULL;
textentryhandle  gpNameEntryBox   = NULL;

enum {
    ESCAPE_DEFAULT = 0,
    ESCAPE_SINGLE,
    ESCAPE_RECORDING,
    ESCAPE_PLAYING,
    ESCAPE_TUTORIAL,
    ESCAPE_DEFAULT_NOSAVE,
    ESCAPE_RECORDING_NOSAVE
};

/*=============================================================================*/

gpgame *gpGames;                                //list of available games
sdword gpCurrentGame = 0;                       //current game index, 0 is always default
sdword gpCurrentSelected = 0;                   //current game index, if OK is pressed
sdword gpNumberGames = 0;                       //number of available games
char   *gpTextEntryName;

fonthandle gpListFont, gpNameFont;              //fonts used for font printing

color gpSelectedColor = GP_SelectedColor;       //color to draw selected one at

uword gpTopIndex = 0;                          //index of what is at the top of the list
uword gpDisplayLength = GP_DisplayListLength;  //number of games displayed at one time

sdword escapemenu;                              //remember what scren to return to

/*=============================================================================
    Private functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : gpGameItemDraw
    Description : Draw the list of available games
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void gpGameItemDraw(rectangle *rect, listitemhandle data)
{
    char            temp[64];
    sdword          x, y;
    color           c;
    fonthandle      oldfont;
    gpgame         *gpGame = (gpgame *)data->data;

    oldfont = fontMakeCurrent(gpListFont);

    if (data->flags&UICLI_Selected)
        c = FEC_ListItemSelected;
    else
        c = FEC_ListItemStandard;

    x = rect->x0;
    y = rect->y0;

    sprintf(temp,"%s",gpGame->title);
    fontPrint(x,y,c,temp);

    fontMakeCurrent(oldfont);
}

/*-----------------------------------------------------------------------------
    Name        : gpGameNameDraw
    Description : Draw the currently selected game name in screens other
                    than the game picker.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void gpGameNameDraw(featom *atom, regionhandle region)
{
    fonthandle fhSave;
    char gameName[128];
    char curr_ch;
    unsigned int i;

    rectangle *r = &region->rect;

    fhSave = fontMakeCurrent(gpNameFont); //select the appropriate font
    for (i = 0; i < 127 && (curr_ch = gpGames[gpCurrentSelected].title[i]); i++)
        gameName[i] = toupper(curr_ch);
    gameName[i] = '\0';
    fontPrintf(r->x0, r->y0, GP_SelectedColor, gameName);
    fontMakeCurrent(fhSave);
}

/*-----------------------------------------------------------------------------
    Name        : compareGamesCB
    Description : compare's the games alphabetically
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
int compareGamesCB(const void *arg1,const void *arg2)
{
    int result;

    gpgame *game1 = (gpgame *)arg1;
    gpgame *game2 = (gpgame *)arg2;

    if (!strcasecmp(game1->title, QuickSaveName))
    {
        return -1;
    }
    else if (!strcasecmp(game2->title, QuickSaveName))
    {
        return 1;
    }

    result = strcasecmp(game1->title,game2->title);
    return result;
}

/*-----------------------------------------------------------------------------
    Name        : gpCountTrainingSavegames
    Description : count the number of training savegames
    Inputs      :
    Outputs     :
    Return      : >= 0
----------------------------------------------------------------------------*/
sdword gpCountTrainingSavegames(void)
{
#ifdef _WIN32
    struct _finddata_t find;
    intptr_t handle, startHandle;
#else
    DIR* dp;
    struct dirent* dir_entry;
#endif
    char fileSearch[128];
    sdword hits;

    hits = 0;

    strcpy(fileSearch, TutorialSavedGamesPath);
#ifdef _WIN32
    strcat(fileSearch, "*.*");

    startHandle = handle = _findfirst(filePathPrepend(fileSearch, FF_UserSettingsPath), &find);

    while (handle != -1)
    {
        if (find.name[0] != '.')
        {
            hits++;
        }

        handle = _findnext(startHandle, &find);
    }
#else
    dp = opendir(filePathPrepend(fileSearch, FF_UserSettingsPath));
    if (!dp)
        return 0;

    while ((dir_entry = readdir(dp)))
    {
        if (dir_entry->d_name[0] == '.')
            continue;
        hits++;
    }

    closedir(dp);
#endif

    return hits;
}

/*-----------------------------------------------------------------------------
    Name        : gpTitleListLoad
    Description : Scans the games directory and loads in the titles of all
                    the game files available.
    Inputs      : void
    Outputs     : Fills in gpGames and gpNumberGames
    Return      : void
----------------------------------------------------------------------------*/
void gpTitleListLoad(void)
{
#ifdef _WIN32
    struct _finddata_t find;
    intptr_t handle, startHandle;
#else
    DIR* dp;
    struct dirent* dir_entry;
#endif
    sdword index;
    char fileName[PATH_MAX]   = "";
    char fileSearch[PATH_MAX] = "";
//    char *pString;
//    char *title;

    for (index = 0; index < gpNumberGames; index++)
    {
        memFree(gpGames[index].fileSpec);
        memFree(gpGames[index].title);
    }
    if (gpNumberGames > 0)
    {
        memFree(gpGames);
        gpGames = NULL;
        gpNumberGames = 0;
    }

    strcpy(fileSearch,SavedGamesPath);
#ifdef _WIN32
    strcat(fileSearch,"*.*");

    startHandle = handle = _findfirst(filePathPrepend(fileSearch, FF_UserSettingsPath), &find);

    while (handle != -1)
    {
        if (find.name[0] == '.')
        {
            goto alreadyLoaded;
        }
        fileName[0] = 0;

        strcpy(fileName, find.name);

        if (fileName[0] == 0)
        {
            goto alreadyLoaded;
        }

        if (strstr(fileName,PKTS_EXTENSION))
        {
            goto alreadyLoaded;
        }

        for (index = 0; index < gpNumberGames; index++)
        {
            if (!strcasecmp(gpGames[index].fileSpec, fileName))
            {                                               //if matching file specs,
                goto alreadyLoaded;                         //break-continue
            }
        }

        gpGames = memRealloc(gpGames, (gpNumberGames+1) * sizeof(gpgame), "gpGames", NonVolatile);

        gpGames[gpNumberGames].fileSpec = memStringDupe(fileName);
        //gpGames[gpNumberGames].title = title;
        gpGames[gpNumberGames].title = memStringDupe(fileName);

        gpNumberGames++;
alreadyLoaded:;
        handle = _findnext(startHandle, &find);
    }
#else
    dp = opendir(filePathPrepend(fileSearch, FF_UserSettingsPath));

    if (dp)
    {
        while ((dir_entry = readdir(dp)))
        {
            if (dir_entry->d_name[0] == '.')
                continue;
            fileName[0] = 0;

            strcpy(fileName, dir_entry->d_name);

            if (fileName[0] == 0)
                continue;

            if (strstr(fileName,PKTS_EXTENSION))
                continue;

            for (index = 0; index < gpNumberGames; index++)
            {
                if (!strcasecmp(gpGames[index].fileSpec, fileName))
                {                                               /*if matching file specs,*/
                    break;                                      /*break-continue*/
                }
            }
            if (index < gpNumberGames)
                continue;

            gpGames = memRealloc(gpGames, (gpNumberGames+1) * sizeof(gpgame), "gpGames", NonVolatile);

            gpGames[gpNumberGames].fileSpec = memStringDupe(fileName);
            //gpGames[gpNumberGames].title = title;
            gpGames[gpNumberGames].title = memStringDupe(fileName);

            gpNumberGames++;
        }

        closedir(dp);
    }
#endif

    if (gpNumberGames > 1)
    {
        //alphabetically sort the game list
        // CAT (M2-R9 oracle-rung-physics): the exe's qsort 0x00671060 (CatSort.c; gpTitleListLoad 0x00487440 at 0x00487644): ties sort as in the exe
        catQsort(&gpGames[0],gpNumberGames,sizeof(gpgame),compareGamesCB);
    }
    gpCurrentSelected = gpCurrentGame = 0;      //set default game
}
/*-----------------------------------------------------------------------------
    Name        : gpTextEntryWindowInit
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void gpTextEntryWindowInit(char *name, featom *atom)
{
    if (FEFIRSTCALL(atom))
    {
        // initialize button here
        gpNameEntryBox = (textentryhandle)atom->pData;
//        uicTextEntrySet(gpNameEntryBox,gpTextEntryName,strlen(gpTextEntryName)+1);
        uicTextBufferResize(gpNameEntryBox,GP_GAMENAME_LENGTH-2);
        bitSet(gpNameEntryBox->textflags, UICTE_FileName);
        return;
    }
    else if ((FELASTCALL(atom)))
    {
        gpNameEntryBox = NULL;
        return;
    }

    switch (uicTextEntryMessage(atom))
    {
        case CM_LoseFocus :
        case CM_AcceptText :
            strcpy(gpTextEntryName,gpNameEntryBox->textBuffer);
        break;
        case CM_GainFocus :
        break;
    }
}

/*=============================================================================
    Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : gpStartup
    Description :
    Inputs      :
    Outputs     : Registers some callbacks for buttons and user regions.
    Return      :
----------------------------------------------------------------------------*/
void gpStartup(void)
{
    feDrawCallbackAddMultiple(gpDrawCallbacks);
    feCallbackAddMultiple(gpCallbacks);

    gpListFont = frFontRegister(GP_ListFont);
    gpNameFont = frFontRegister(GP_NameFont);
    gpTextEntryName = memAlloc(GP_GAMENAME_LENGTH, "gpTextEntryName", NonVolatile);
    gpTextEntryName[0] = 0;
    gpGames = NULL;
}

/*-----------------------------------------------------------------------------
    Name        : gpReset
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void gpReset(void)
{
    SavedGamesPath = NULL;
}

/*-----------------------------------------------------------------------------
    Name        : gpShutdown
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void gpShutdown(void)
{
    sdword index;

    for (index = 0; index < gpNumberGames; index++)
    {
        memFree(gpGames[index].fileSpec);
        memFree(gpGames[index].title);
    }
    if (gpGames!=NULL)
        memFree(gpGames);
    if (gpTextEntryName!=NULL)
        memFree(gpTextEntryName);
//    frFontUnregister(gpListFont);
//    frFontUnregister(gpNameFont);
}

char* gpQuickSetup(void)
{
    if (gameIsRunning)
    {
        if (singlePlayerGame)
        {
            gpLoadSinglePlayerGame = TRUE;
        }
        else
        {
            gpLoadSinglePlayerGame = FALSE;
        }

        gpLoadTutorial = (tutorial==TUTORIAL_ONLY) ? TRUE : FALSE;
    }
    else
    {
        if (mgRunning)
        {
            gpLoadSinglePlayerGame = FALSE;
        }
        else
        {
            gpLoadSinglePlayerGame = TRUE;
        }

        gpLoadTutorial = (tutorial==TUTORIAL_ONLY) ? TRUE : FALSE;
    }

    if (gpLoadSinglePlayerGame)
    {
        return(gpLoadTutorial ? TutorialSavedGamesPath : SinglePlayerSavedGamesPath);
    }
    else
    {
        return(MultiPlayerSavedGamesPath);
    }
}

bool32 gpQuickSave(void)
{
    char* path;
    char  filename[128];

    path = gpQuickSetup();
    if (path == NULL)
    {
        dbgMessage("gpQuickSave couldn't find a dir");
        return FALSE;
    }

    strcpy(filename, path);
    strcat(filename, QuickSaveName);
    gpSaveGivenGame(filename);

    return TRUE;
}

bool32 gpQuickLoad(void)
{
    char* path;
    char  filename[128];

    path = gpQuickSetup();
    if (path == NULL)
    {
        dbgMessage("gpQuickLoad couldn't find a dir");
        return FALSE;
    }

    strcpy(filename, path);
    strcat(filename, QuickSaveName);
    gpLoadGivenGame(filename);

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : gpGameWindowInit
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void gpGameWindowInit(char *name, featom *atom)
{
    fonthandle  oldfont;
    sdword      index;

    if (FEFIRSTCALL(atom))
    {
        if (strcmp(name,"FE_RecordedGameWindowInit") == 0)
        {
            SavedGamesPath = RecordedGamesPath;
        }
        else if (strcmp(name,"FE_TutorialGameWindowInit") == 0)
        {
            SavedGamesPath = TutorialSavedGamesPath;
            tutorial = TUTORIAL_ONLY;
            gpLoadTutorial = TRUE;
            gpLoadSinglePlayerGame = TRUE;
        }
        else
        {
            if (gameIsRunning)
            {
                if (singlePlayerGame)
                {
                    gpLoadSinglePlayerGame = TRUE;
                }
                else
                {
                    gpLoadSinglePlayerGame = FALSE;
                }

                gpLoadTutorial = (tutorial==TUTORIAL_ONLY) ? TRUE : FALSE;
            }
            else
            {
                if (mgRunning)
                {
                    gpLoadSinglePlayerGame = FALSE;
                }
                else
                {
                    gpLoadSinglePlayerGame = TRUE;
                }

                gpLoadTutorial = (tutorial==TUTORIAL_ONLY) ? TRUE : FALSE;
            }

            if (gpLoadSinglePlayerGame)
            {
                SavedGamesPath = gpLoadTutorial ? TutorialSavedGamesPath : SinglePlayerSavedGamesPath;
            }
            else
            {
                SavedGamesPath = MultiPlayerSavedGamesPath;
            }
        }

        gpTitleListLoad();

        oldfont = fontMakeCurrent(gpListFont);

        gpGameListWindow = (listwindowhandle)atom->pData;

        uicListWindowInit(gpGameListWindow,
                          NULL,                             // title draw, no title
                          NULL,                             // title click process, no title
                          0,                                // title height, no title
                          gpGameItemDraw,                   // item draw function
                          fontHeight(" ")+GP_VertSpacing,   // item height
                          UICLW_CanSelect|UICLW_CanHaveFocus);

        for (index = 0; index < gpNumberGames; index++)
        {
            if (index == 0)
            {
                uicListAddItem(gpGameListWindow, (ubyte *)&gpGames[index], UICLI_CanSelect|UICLI_Selected, UICLW_AddToTail);
                gpCurrentSelected = 0;
            }
            else
            {
                uicListAddItem(gpGameListWindow, (ubyte *)&gpGames[index], UICLI_CanSelect, UICLW_AddToTail);
            }
        }

        fontMakeCurrent(oldfont);
        return;
    } else if (FELASTCALL(atom))
    {
        gpGameListWindow = NULL;
        return;
    }
    else if (gpGameListWindow->message == CM_NewItemSelected)
    {
        if (gpNameEntryBox != NULL)
        {
            uicTextEntrySet(gpNameEntryBox, ((gpgame *)gpGameListWindow->CurLineSelected->data)->title,strlen(((gpgame *)gpGameListWindow->CurLineSelected->data)->title));
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : gpGamePick
    Description : Starts the game picker
    Inputs      : dest - where to store file name of destiation file.
                    Will be "" if nothing (yet) chosen.
    Outputs     : Starts the game picker screen.  When chosen, the name
                    will be stored in dest.
    Return      : void
----------------------------------------------------------------------------*/
void gpGamePick(char *dest)
{
    if (dest[0] == 'S')
    {
        gpLoadSinglePlayerGame = TRUE;
    }
    else
    {
        gpLoadSinglePlayerGame = FALSE;
    }

    SavedGamesPath = (gpLoadSinglePlayerGame) ? SinglePlayerSavedGamesPath : MultiPlayerSavedGamesPath;

    feScreenStart(ghMainRegion, GP_ScreenName);
}

/*-----------------------------------------------------------------------------
    Name        : gpDonePicking/gpBackPicking
    // if the region is the game picker
    Description : Close the color picker and optionally specify the file path
                    of the chosen file.
    Inputs      : standard FE callback
    Outputs     : DonePicking stores chosen file name in parameter to gpGamePick
    Return      : void
----------------------------------------------------------------------------*/
void gpDonePicking(char *name, featom *atom)
{
    sdword index;

    gpCurrentSelected = gpCurrentGame;

    if (gpGameListWindow->CurLineSelected!=NULL)
    {
        for (index = 0; index < gpNumberGames; index++)
        {
            if (gpGameListWindow->CurLineSelected->data == (ubyte *)&gpGames[index])
            {
                gpCurrentSelected = index;
                break;
            }
        }
    }
}

static char overwritefilename[PATH_MAX] = "";
static bool32 overwriteRecGame = FALSE;

void gpOverwriteYes(char *name, featom *atom)
{
    dbgAssertOrIgnore(overwritefilename[0] != 0);

    if (overwriteRecGame)
    {
        overwriteRecGame = FALSE;

        recPackInGameStartCB(overwritefilename);
        feAllScreensDelete();
        if (!multiPlayerGame)
        {
            universePause = FALSE;      // unpause game
        }
    }
    else
    {
        if (SaveGame(overwritefilename))
            clCommandMessage(strGetString(strSavedGame), 0xffffffff);
        else
            clCommandMessage(strGetString(strPatchUnableWriteFile), 0xffffffff);
        feAllScreensDelete();
        if (!multiPlayerGame)
        {
            universePause = FALSE;      // unpause game
        }
    }
}

void gpOverwriteNo(char *name, featom *atom)
{
    feScreenDisappear(NULL, NULL);
}

void gpSaveGame(char *name, featom *atom)
{
    char filename[300];

    if ((gpTextEntryName != NULL) && (strlen(gpTextEntryName) > 0))
    {
        if (strlen(gpTextEntryName) < 2)
        {
            GeneralMessageBox(strGetString(strErrorInSaveGameName),strGetString(strAtLeast2Chars));
            return;
        }

        strcpy(filename,SavedGamesPath);
        strcat(filename,gpTextEntryName);

        if (fileExists(filename,0))
        {
            overwriteRecGame = FALSE;
            strcpy(overwritefilename,filename);
            feScreenStart(ghMainRegion, "OverwriteGamePopup");
            return;
        }

        if (!SaveGame(filename))
        {
            GeneralMessageBox(strGetString(strPatchUnableWriteFile),NULL);
            return;
        }

        clCommandMessage(strGetString(strSavedGame), 0xffffffff);
        feScreenDisappear(NULL, NULL);
        if (!multiPlayerGame)
        {
            universePause = FALSE;      // unpause game
        }
        return;
    }

    if (gpNumberGames > 0)
    {
        gpDonePicking(name, atom);
        dbgAssertOrIgnore(gpCurrentSelected < gpNumberGames);
        strcpy(filename,SavedGamesPath);
        strcat(filename,gpGames[gpCurrentSelected].title);

        if (fileExists(filename,0))
        {
            overwriteRecGame = FALSE;
            strcpy(overwritefilename,filename);
            feScreenStart(ghMainRegion, "OverwriteGamePopup");
            return;
        }

        if (!SaveGame(filename))
        {
            GeneralMessageBox(strGetString(strPatchUnableWriteFile),NULL);
            return;
        }

        clCommandMessage(strGetString(strSavedGame), 0xffffffff);
        feScreenDisappear(NULL, NULL);
        if (!multiPlayerGame)
        {
            universePause = FALSE;      // unpause game
        }
        return;
    }

    GeneralMessageBox(strGetString(strErrorInSaveGameName),strGetString(strAtLeast2Chars));
}

/*-----------------------------------------------------------------------------
    Name        : gpLoadRecordedGameGivenName
    Description : CAT (M2-R6 oracle-mode-a): the body of the front end's "LoadRecordedGame" callback
                  (LoadRecordedGame, code at 0x00487cf0, not a Ghidra function: read from the disassembly) once the
                  names are set (recordPacketSaveFileName = the saved game, recordPacketFileName = it + ".pkts"):
                  VerifySaveFile 0x004fdf40 (a message box on failure), feScreenDisappear 0x00475200,
                  recPackPlayInGameInit 0x004cc010, utyLoadMultiPlayerGameGivenFilename 0x005aaa50, then the
                  recorded-game words: 0x00a7519c and 0x00a751a0 set, 0x00a75190 cleared, or set when one of players
                  1 .. numPlayers-1 is not named "<computer name> <n>" (format 0x008812d8 "%s %i" against the
                  0x12-byte names from 0x00c39652). Also used by the oracle's replay mode (OracleDump.c).
    Inputs      :
    Outputs     :
    Return      : TRUE if the game was loaded
----------------------------------------------------------------------------*/
bool32 gpLoadRecordedGameGivenName(void)
{
    sdword verifysavename;
    sdword i;
    char computerName[MAX_PERSONAL_NAME_LEN + 32];

    if ((verifysavename = VerifySaveFile(recordPacketSaveFileName)) != VERIFYSAVEFILE_OK)
    {
        if (verifysavename == VERIFYSAVEFILE_BADVERSION)
            GeneralMessageBox(strGetString(strErrorInvalidSaveGameFileVersion),NULL);
        else
            GeneralMessageBox(strGetString(strErrorInvalidSaveGameFile),NULL);
        return FALSE;
    }

    feScreenDisappear(NULL, NULL);

    recPackPlayInGameInit();                    // CAT: recPackPlayInGameInit 0x004cc010 (NetCheck.c)

    utyLoadMultiPlayerGameGivenFilename(recordPacketSaveFileName);  // CAT: 0x005aaa50

    // CAT: the recorded-game words (LoadRecordedGame 0x00487e4d .. 0x00487ede)
    recordedGameActive = TRUE;
    recordedGamePlayback = TRUE;
    recordedGameMultiplayer = FALSE;
    for (i = 1; i < universe.numPlayers; i++)
    {
        snprintf(computerName, sizeof(computerName), "%s %i", strGetString(strComputerName), (int)i);
        if (strcmp(playerNames[i], computerName) != 0)
        {
            recordedGameMultiplayer = TRUE;
        }
    }
    return TRUE;
}

void gpLoadTheRecordedGame(char *name, featom *atom)
{
    if (!gpGetGameName(name,atom,recordPacketSaveFileName))
    {
        return;
    }

    strcpy(recordPacketFileName,recordPacketSaveFileName);
    strcat(recordPacketFileName,PKTS_EXTENSION);

    gpLoadRecordedGameGivenName();
}

void gpSaveTheRecordedGame(char *name, featom *atom)
{
    char filename[300];

    if ((gpTextEntryName != NULL) && (strlen(gpTextEntryName) > 0))
    {
        if (strlen(gpTextEntryName) < 2)
        {
            GeneralMessageBox(strGetString(strErrorInSaveGameName),strGetString(strAtLeast2Chars));
            return;
        }

        strcpy(filename,SavedGamesPath);
        strcat(filename,gpTextEntryName);

        if (fileExists(filename,0))
        {
            overwriteRecGame = TRUE;
            strcpy(overwritefilename,filename);
            feScreenStart(ghMainRegion, "OverwriteGamePopup");
            return;
        }

        recPackInGameStartCB(filename);
        feScreenDisappear(NULL, NULL);
        if (!multiPlayerGame)
        {
            universePause = FALSE;      // unpause game
        }
        return;
    }

    if (gpNumberGames > 0)
    {
        gpDonePicking(name, atom);
        dbgAssertOrIgnore(gpCurrentSelected < gpNumberGames);
        strcpy(filename,SavedGamesPath);
        strcat(filename,gpGames[gpCurrentSelected].title);

        if (fileExists(filename,0))
        {
            overwriteRecGame = TRUE;
            strcpy(overwritefilename,filename);
            feScreenStart(ghMainRegion, "OverwriteGamePopup");
            return;
        }

        recPackInGameStartCB(filename);
        feScreenDisappear(NULL, NULL);
        if (!multiPlayerGame)
        {
            universePause = FALSE;      // unpause game
        }
        return;
    }

    GeneralMessageBox(strGetString(strErrorInSaveGameName),strGetString(strAtLeast2Chars));
}

void gpStopPlayback(char *name, featom *atom)
{
    dbgMessage("Stop playback.");
}

void gpStopRecording(char *name, featom *atom)
{
    recPackInGameStopCB();
    feScreenDisappear(NULL, NULL);
    if (!multiPlayerGame)
    {
        universePause = FALSE;      // unpause game
    }
}

void gpSaveGivenGame(char* gamename)
{
    if (SaveGame(gamename))
        clCommandMessage(strGetString(strQuickSave), 0xffffffff);
    else
        clCommandMessage(strGetString(strPatchUnableWriteFile), 0xffffffff);

    if (!multiPlayerGame)
    {
        universePause = FALSE;
    }
}

void gpLoadGivenGame(char* gamename)
{
    sdword verifysavename;

    if (!fileExists(gamename, FF_UserSettingsPath))
    {
        return;
    }

    if ((verifysavename = VerifySaveFile(gamename)) != VERIFYSAVEFILE_OK)
    {
        if (verifysavename == VERIFYSAVEFILE_BADVERSION)
            GeneralMessageBox(strGetString(strErrorInvalidSaveGameFileVersion),NULL);
        else
            GeneralMessageBox(strGetString(strErrorInvalidSaveGameFile),NULL);
        return;
    }

    gameEnd();

    if (gpLoadSinglePlayerGame)
    {
        if (gpLoadTutorial)
        {
            tutorial = 1;
        }
        else
        {
            tutorial = 2;
        }
        utyLoadSinglePlayerGameGivenFilename(gamename);
    }
    else
    {
        utyLoadMultiPlayerGameGivenFilename(gamename);
    }
    if (!multiPlayerGame)
    {
        universePause = FALSE;
    }
}

void spRestartLevel(char *name, featom *atom)
{
    if (strlen(CurrentLevelName) > 0)
    {
        gpLoadSinglePlayerGame = TRUE;
        gpLoadTutorial = FALSE;
        gpLoadGivenGame(CurrentLevelName);
    }
}

void gpLoadGame(char *name, featom *atom)
{
    char filename[PATH_MAX] = "";
    sdword verifysavename;

    if (gpNumberGames > 0)
    {
        gpDonePicking(name, atom);

        dbgAssertOrIgnore(gpCurrentSelected < gpNumberGames);
        if (gpLoadTutorial)
        {
            strcpy(filename,TutorialSavedGamesPath);
        }
        else
        {
            strcpy(filename,SavedGamesPath);
        }
        strcat(filename,gpGames[gpCurrentSelected].title);

        if ((verifysavename = VerifySaveFile(filename)) != VERIFYSAVEFILE_OK)
        {
            if (verifysavename == VERIFYSAVEFILE_BADVERSION)
                GeneralMessageBox(strGetString(strErrorInvalidSaveGameFileVersion),NULL);
            else
                GeneralMessageBox(strGetString(strErrorInvalidSaveGameFile),NULL);
            return;
        }

        feScreenDisappear(NULL, NULL);

        soundEventStopTrack(SOUND_EVENT_DEFAULT, 1.0f);
        soundEventPause(TRUE);
        gameEnd();
        if (gpLoadSinglePlayerGame)
        {
            if (gpLoadTutorial)
            {
                tutorial = 1;
            }
            else
            {
                //for single player games, tutorial == 2 to use certain tutorial functions
                tutorial = 2;
            }
            utyLoadSinglePlayerGameGivenFilename(filename);
        }
        else
        {
            utyLoadMultiPlayerGameGivenFilename(filename);
        }
        if (!multiPlayerGame)
        {
            universePause = FALSE;      // unpause game
        }
        soundEventPause(FALSE);
    }
}


void gpDeleteGame(char *name, featom *atom)
{
    char filename[PATH_MAX] = "";
    sdword i;
    sdword index;
    fonthandle  oldfont;

    if (!gpGetGameName(name,atom,filename))
    {
        return;
    }

    // gpCurrentSelected will be set by gpGetGameName

    feScreenDisappear(NULL, NULL);

    char *tmpFilePath = filePathPrepend(filename, FF_UserSettingsPath);
    strcpy(filename, tmpFilePath);
    fileDelete(filename);

    if (SavedGamesPath == RecordedGamesPath)
    {
        char tmpfile[PATH_MAX] = "";
        strcpy(tmpfile,filename);
        strcat(tmpfile,PKTS_EXTENSION);
        fileDelete(tmpfile);
    }

    for (i=gpCurrentSelected;i<gpNumberGames-1;i++)
    {
        gpGames[i] = gpGames[i+1];
    }
    gpNumberGames--;
    if (gpNumberGames == 0)
    {
        gpCurrentSelected = 0;
    }
    else if (gpCurrentSelected >= gpNumberGames)
    {
        gpCurrentSelected--;
        dbgAssertOrIgnore(gpCurrentSelected >= 0);
        dbgAssertOrIgnore(gpCurrentSelected < gpNumberGames);
    }

    dbgAssertOrIgnore(gpNumberGames >= 0);

    oldfont = fontMakeCurrent(gpListFont);      // fontHeight called later on in this function

    uicListCleanUp(gpGameListWindow);

    uicListWindowInit(gpGameListWindow,
                      NULL,                             // title draw, no title
                      NULL,                             // title click process, no title
                      0,                                // title height, no title
                      gpGameItemDraw,                   // item draw function
                      fontHeight(" ")+GP_VertSpacing,   // item height
                      UICLW_CanSelect);

    for (index = 0; index < gpNumberGames; index++)
    {
        if (index==gpCurrentSelected)
            uicListAddItem(gpGameListWindow, (ubyte *)&gpGames[index], UICLI_CanSelect|UICLI_Selected, UICLW_AddToTail);
        else
            uicListAddItem(gpGameListWindow, (ubyte *)&gpGames[index], UICLI_CanSelect, UICLW_AddToTail);
    }

    fontMakeCurrent(oldfont);
}

bool32 gpGetGameName(char *name, featom *atom, char *filename)
{
    if (gpNumberGames > 0)
    {
        gpDonePicking(name, atom);
        dbgAssertOrIgnore(gpCurrentSelected < gpNumberGames);
        strcpy(filename,SavedGamesPath);
        strcat(filename,gpGames[gpCurrentSelected].title);
        return TRUE;
    }
    return FALSE;
}

void gpBackPicking(char *name, featom *atom)
{
    feScreenDisappear(NULL, NULL);
}

// CAT (M2-R5 hud): the escape menu screen of the current kind, shared by gpBackToInGameEscapeMenu and
//      gpStartInGameEscapeMenu (which inlines it in the exe, 0x004887c0)
static void catGpEscapeMenuScreenStart(void)
{
    switch (escapemenu)
    {
        case ESCAPE_DEFAULT:
            feScreenStart(ghMainRegion, "In_game_esc_menu");
            break;

        case ESCAPE_SINGLE:
            feScreenStart(ghMainRegion, "In_game_esc_menu2");
            break;

        case ESCAPE_RECORDING:
            feScreenStart(ghMainRegion, "In_game_esc_menu_recording");
            break;

        case ESCAPE_PLAYING:
            feScreenStart(ghMainRegion, "In_game_esc_menu_playing");
            break;

        case ESCAPE_TUTORIAL:
            feScreenStart(ghMainRegion, "In_game_esc_menu_tutorial");
            break;

        case ESCAPE_DEFAULT_NOSAVE:
            feScreenStart(ghMainRegion, "In_game_esc_menu_nosave");
            break;

        case ESCAPE_RECORDING_NOSAVE:
            feScreenStart(ghMainRegion, "In_game_esc_menu_nosave_recording");
            break;
    }
}

void gpBackToInGameEscapeMenu(char *name, featom *atom)
{
    feScreenDisappear(NULL, NULL);

#if defined(HW_GAME_DEMO)
    if ((singlePlayerGame) && (!(utyCreditsSequence|utyPlugScreens)) && (universe.quittime > 0.0f) && (universe.totaltimeelapsed >= universe.quittime))
#else
    if ((singlePlayerGame) && (!(utyCreditsSequence)) && (universe.quittime > 0.0f) && (universe.totaltimeelapsed >= universe.quittime))
#endif
    {
        feScreenStart(ghMainRegion, "SP_Game_Over");
        return;
    }

    catGpEscapeMenuScreenStart();
}

void gpStartInGameEscapeMenu(void)
{
    if (tutorial==TUTORIAL_ONLY)
    {
        escapemenu = ESCAPE_TUTORIAL;
    }
    else if (singlePlayerGame)
    {
        escapemenu = ESCAPE_SINGLE;
    }
    else if (playPackets)
    {
        escapemenu = ESCAPE_PLAYING;
    }
    else if (recordPackets)
    {
        if (debugPacketRecord)
        {
            if (multiPlayerGame)
                escapemenu = ESCAPE_DEFAULT_NOSAVE;
            else
                escapemenu = ESCAPE_DEFAULT;
        }
        else
        {
            if (multiPlayerGame)
                escapemenu = ESCAPE_RECORDING_NOSAVE;
            else
                escapemenu = ESCAPE_RECORDING;
        }
    }
    else
    {
        if (multiPlayerGame)
            escapemenu = ESCAPE_DEFAULT_NOSAVE;
        else
            escapemenu = ESCAPE_DEFAULT;
    }

    mrReset();
    // CAT: gpStartInGameEscapeMenu 0x004887c0 inlines gpBackToInGameEscapeMenu but tests the quit time against the
    //      real time (universe.realtimeelapsed), not the game time
    feScreenDisappear(NULL, NULL);
#if defined(HW_GAME_DEMO)
    if ((singlePlayerGame) && (!(utyCreditsSequence|utyPlugScreens)) && (universe.quittime > 0.0f) && (universe.realtimeelapsed >= universe.quittime))
#else
    if ((singlePlayerGame) && (!(utyCreditsSequence)) && (universe.quittime > 0.0f) && (universe.realtimeelapsed >= universe.quittime))
#endif
    {
        feScreenStart(ghMainRegion, "SP_Game_Over");
        return;
    }
    catGpEscapeMenuScreenStart();
}

/*=============================================================================
    CAT (M2-R5 hud): the game-stats screen
=============================================================================*/
static fonthandle catGameStatsFont;                 // 0x00a43c84 default.hff (the table)
static fonthandle catGameStatsButtonFont;           // 0x00a43c88 default.hff (the buttons)
static fonthandle catGameStatsTitleFont;            // 0x00a43c8c Arial_b17.hff
static bool32 catGameStatsActive = FALSE;           // 0x00a43c78
static CatTimer catGameStatsBarTimer;               // 0x00a43b78: the bars grow over 2 s after a column is picked

/*-----------------------------------------------------------------------------
    Name        : cat_gameStatsButtonDraw                           0x00489470
    Description : A three-part button (left cap, middle, right cap icons) with its text centred; with wholeHover the
                  whole button shows its mouse-over icons, else only the cap under the mouse. With a hot-key index the
                  text's letter there is white and the rest grey.
----------------------------------------------------------------------------*/
void cat_gameStatsButtonDraw(rectangle *rect, char *text, sdword hotkeyIndex, fonthandle font, sdword capWidth,
                             sdword iconLeft, sdword iconLeftMouse, sdword iconMid, sdword iconMidMouse,
                             sdword iconRight, sdword iconRightMouse, bool32 wholeHover)
{
    sdword left = iconLeft, mid = iconMid, right = iconRight;
    sdword width, height, x, y;
    fonthandle oldFont;
    char head[20], letter[2];

    (void)font;                                     // the exe prints with its own button font
    if (wholeHover)
    {
        if (rect->x0 < mouseCursorXPosition && rect->y0 < mouseCursorYPosition &&
            mouseCursorXPosition < rect->x1 && mouseCursorYPosition < rect->y1)
        {
            left = iconLeftMouse;
            mid = iconMidMouse;
            right = iconRightMouse;
        }
    }
    else if (rect->x0 < mouseCursorXPosition && rect->y0 < mouseCursorYPosition &&
             mouseCursorXPosition < rect->x0 + capWidth && mouseCursorYPosition < rect->y1)
    {
        left = iconLeftMouse;
    }
    else if (rect->x1 - capWidth < mouseCursorXPosition && rect->y0 < mouseCursorYPosition &&
             mouseCursorXPosition < rect->x1 && mouseCursorYPosition < rect->y1)
    {
        right = iconRightMouse;
    }
    cat_overlayIconDraw(left, rect->y0, rect->x0, capWidth, rect->y1 - rect->y0, 0xffffffff);
    cat_overlayIconDraw(right, rect->y0, rect->x1 - capWidth, capWidth, rect->y1 - rect->y0, 0xffffffff);
    cat_overlayIconDraw(mid, rect->y0, rect->x0 + capWidth, (rect->x1 - capWidth * 2) - rect->x0, rect->y1 - rect->y0,
                        0xffffffff);
    oldFont = fontMakeCurrent(catGameStatsButtonFont);
    width = ((rect->x1 - rect->x0) - capWidth * 2) - fontWidth(text);
    height = (rect->y1 - rect->y0) - fontHeight(text);
    x = width / 2 + rect->x0 + capWidth;
    y = height / 2 + rect->y0;
    if (hotkeyIndex == -1)
    {
        fontPrintf(x, y, 0xffffffff, "%s", text);
    }
    else
    {
        strncpy(head, text, sizeof(head) - 1);      // CAT-FIX: the exe copies the whole text into its 20 bytes
        head[sizeof(head) - 1] = '\0';
        letter[0] = text[hotkeyIndex];
        letter[1] = '\0';
        if (hotkeyIndex < (sdword)sizeof(head))
        {
            head[hotkeyIndex] = '\0';
        }
        fontPrintf(x, y, 0xffc8c8c8, "%s", head);
        x += fontWidth(head);
        fontPrintf(x, y, 0xffffffff, "%s", letter);
        fontPrintf(x + fontWidth(letter), y, 0xffc8c8c8, "%s", text + hotkeyIndex + 1);
    }
    fontMakeCurrent(oldFont);
}

/*-----------------------------------------------------------------------------
    Name        : cat_gameStatsStartup                              0x00489e50
----------------------------------------------------------------------------*/
void cat_gameStatsStartup(void)
{
    catGameStatsFont = frFontRegister("default.hff");
    catGameStatsButtonFont = frFontRegister("default.hff");
    catGameStatsTitleFont = frFontRegister("Arial_b17.hff");
    cat_timerStartRealTime(&catGameStatsBarTimer, 2.0f);
    (void)catGameStatsTitleFont;                    // the exe registers it; no reader was found
}

/*-----------------------------------------------------------------------------
    CAT (M2-R6 mainrgn-popup): the rest of the game-stats screen (cat_gameStatsValueGet 0x00488970 ..
    cat_gameStatsBegin 0x00489e90, hud's deferral). One row of 17 values per player; the table shows them by
    column, the bar graph the selected column; Up / Down or a click on a column's picker select it; Exit (a click,
    Escape or X) quits to the main menu. Layout data: the exe's 0x00881400 .. 0x00881438 and its 17 titles
    (0x008813b8).
-----------------------------------------------------------------------------*/
#define CAT_GS_COLUMNS          17
#define CAT_GS_ROW_WORDS        17          // 0x44 bytes per player row
#define CAT_GS_EXIT_DY          40          // 0x00881400: the exit button 40 px above the screen bottom
#define CAT_GS_EXIT_WIDTH       200         // 0x00881404
#define CAT_GS_EXIT_HEIGHT      30          // 0x00881408
#define CAT_GS_EXIT_CAP         30          // 0x0088140c
#define CAT_GS_TITLE_X          10          // 0x00881410
#define CAT_GS_PLAYER_X         28          // 0x00881414
#define CAT_GS_GAP_X            10          // 0x00881418
#define CAT_GS_PICKER_GAP       10          // 0x0088141c
#define CAT_GS_TOP              20          // 0x00881420
#define CAT_GS_PANEL_WIDTH      640         // 0x00881424
#define CAT_GS_PANEL_HEIGHT     480         // 0x00881428
#define CAT_GS_HEADER_GAP       30          // 0x0088142c
#define CAT_GS_ICON             35          // 0x00881430
#define CAT_GS_LINE_GAP         1           // 0x00881434
#define CAT_GS_BACKDROP         0xd2323232  // 0x00881438

// the column titles (0x008813b8)
static const sdword catGameStatsTitles[CAT_GS_COLUMNS] =
{
    str_GSD_SurvivalTime, str_GSD_PlayersKilled, str_GSD_ResourcesHarvested, str_GSD_CrystalsHarvested,
    str_GSD_TechResearched, str_GSD_FightersBuilt, str_GSD_FightersLost, str_GSD_FightersKilled,
    str_GSD_FrigatesBuilt, str_GSD_FrigatesLost, str_GSD_FrigatesKilled, str_GSD_DestroyersBuilt,
    str_GSD_DestroyersLost, str_GSD_DestroyersKilled, str_GSD_NonCombatBuilt, str_GSD_NonCombatLost,
    str_GSD_NonCombatKilled
};
// a column's word of a player's row (cat_gameStatsValueGet / cat_gameStatsValuePrint)
static const sdword catGameStatsWordOfColumn[CAT_GS_COLUMNS] = { 0, 1, 2, 3, 4, 13, 9, 5, 14, 10, 6, 15, 11, 7, 16, 12, 8 };

static sdword *catGameStatsRows = NULL;             // 0x00a43c7c: numPlayers rows of 17 words
static bool32 catGameStatsShown = FALSE;            // 0x00a43c80
static sdword catGameStatsOrder[MAX_MULTIPLAYER_PLAYERS];   // 0x00a43818: the players by survival time
static sdword catGameStatsColumn = 0;               // 0x00a43b88: the selected column
static rectangle catGameStatsPanel;                 // 0x00a43838
static rectangle catGameStatsPickerRight[CAT_GS_COLUMNS];   // 0x00a43848: the column picker's right cap
static rectangle catGameStatsPlayerIcon[MAX_MULTIPLAYER_PLAYERS];   // 0x00a43958
static rectangle catGameStatsPlayerColumn[MAX_MULTIPLAYER_PLAYERS]; // 0x00a439d8: a player's column of values
static rectangle catGameStatsPickerLeft[CAT_GS_COLUMNS];    // 0x00a43a58: the column picker's left cap
static rectangle catGameStatsGraph;                 // 0x00a43b68: the bar graph
static sdword catGameStatsPlayerY[MAX_MULTIPLAYER_PLAYERS]; // 0x00a43b8c
static sdword catGameStatsPlayerX[MAX_MULTIPLAYER_PLAYERS]; // 0x00a43bac
static rectangle catGameStatsHeader;                // 0x00a43bd0
static sdword catGameStatsTitleX[CAT_GS_COLUMNS];   // 0x00a43be0
static sdword catGameStatsTitleY[CAT_GS_COLUMNS];   // 0x00a43c24
static rectangle catGameStatsExit;                  // 0x00a43c68

real32 cat_gameStatsValueGet(sdword row, sdword column)                                     // 0x00488970
{
    if ((column < 0) || (column >= CAT_GS_COLUMNS))
    {
        return 0.0f;
    }
    return (real32)catGameStatsRows[row * CAT_GS_ROW_WORDS + catGameStatsWordOfColumn[column]];
}

void cat_gameStatsValuePrint(sdword row, sdword column, char *out)                          // 0x00488a50
{
    sdword value;

    if ((column < 0) || (column >= CAT_GS_COLUMNS))
    {
        return;
    }
    value = catGameStatsRows[row * CAT_GS_ROW_WORDS + catGameStatsWordOfColumn[column]];
    if (column == 0)
    {                                                       // the survival time h:mm:ss
        sprintf(out, "%d:%02d:%02d", value / 3600, (value / 60) % 60, value % 60);
        return;
    }
    sprintf(out, "%d", value);
}

/*-----------------------------------------------------------------------------
    Name        : cat_gameStatsPlayerRowFill                        0x00488c80
    Description : A player's row: survival time (the time of death, else the game time), players killed, RUs
                  harvested, crystals, technologies researched, and per group (strike craft = fighters + corvettes +
                  micro, frigates, capital = heavy cruisers + destroyers, non-combat = carriers + resource +
                  non-combat + components) the ships built, killed and lost.
----------------------------------------------------------------------------*/
void cat_gameStatsPlayerRowFill(sdword playerIndex, sdword *row)
{
    PlayerStats *stats = &universe.gameStats.playerStats[playerIndex];
    sdword tech;

    row[0] = (sdword)stats->timeOfDeath;
    if (row[0] == 0)
    {
        row[0] = (sdword)universe.totaltimeelapsed;
    }
    row[1] = stats->catStat7D0;
    row[2] = stats->totalResourceUnitsCollected;
    row[3] = stats->catStat28;
    row[4] = 0;
    for (tech = 0; tech < 0x29; tech++)
    {
        if (cat_rmTechSetHas((sdword *)&universe.players[playerIndex].researchinfo.HasTechnology, tech))
        {
            row[4]++;
        }
    }
    row[13] = stats->catBuiltForEachClass[CLASS_Fighter] + stats->catBuiltForEachClass[CLASS_Corvette] +
              stats->catBuiltForEachClass[CLASS_Micro];
    row[5] = stats->totalKillsForEachClass[CLASS_Fighter] + stats->totalKillsForEachClass[CLASS_Corvette] +
             stats->totalKillsForEachClass[CLASS_Micro];
    row[9] = stats->totalLossesForEachClass[CLASS_Fighter] + stats->totalLossesForEachClass[CLASS_Corvette] +
             stats->totalLossesForEachClass[CLASS_Micro];
    row[14] = stats->catBuiltForEachClass[CLASS_Frigate];
    row[6] = stats->totalKillsForEachClass[CLASS_Frigate];
    row[10] = stats->totalLossesForEachClass[CLASS_Frigate];
    row[15] = stats->catBuiltForEachClass[CLASS_HeavyCruiser] + stats->catBuiltForEachClass[CLASS_Destroyer];
    row[7] = stats->totalKillsForEachClass[CLASS_HeavyCruiser] + stats->totalKillsForEachClass[CLASS_Destroyer];
    row[11] = stats->totalLossesForEachClass[CLASS_HeavyCruiser] + stats->totalLossesForEachClass[CLASS_Destroyer];
    row[16] = stats->catBuiltForEachClass[CLASS_Carrier] + stats->catBuiltForEachClass[CLASS_Resource] +
              stats->catBuiltForEachClass[CLASS_NonCombat] + stats->catBuiltForEachClass[CLASS_Component];
    row[8] = stats->totalKillsForEachClass[CLASS_Carrier] + stats->totalKillsForEachClass[CLASS_Resource] +
             stats->totalKillsForEachClass[CLASS_NonCombat] + stats->totalKillsForEachClass[CLASS_Component];
    row[12] = stats->totalLossesForEachClass[CLASS_Carrier] + stats->totalLossesForEachClass[CLASS_Resource] +
              stats->totalLossesForEachClass[CLASS_NonCombat] + stats->totalLossesForEachClass[CLASS_Component];
}

/*-----------------------------------------------------------------------------
    Name        : cat_gameStatsLayoutCompute                        0x00488e70
    Description : The panel (640 x 480 centred), the player names down the left, the column titles right-aligned
                  under them with their pickers, a column of values per player, the exit button at the bottom.
----------------------------------------------------------------------------*/
void cat_gameStatsLayoutCompute(void)
{
    sdword h, headerH, maxW = 0, colW, x, y, index, width, numPlayers = universe.numPlayers;

    fontMakeCurrent(catGameStatsFont);
    h = fontHeight("");
    headerH = CAT_GS_LINE_GAP * 7 + h * 8;
    for (index = 0; index < CAT_GS_COLUMNS; index++)
    {
        width = fontWidth(strGetString(catGameStatsTitles[index]));
        if (maxW < width)
        {
            maxW = width;
        }
    }
    catGameStatsPanel.x0 = (MAIN_WindowWidth - CAT_GS_PANEL_WIDTH) / 2;
    catGameStatsPanel.x1 = catGameStatsPanel.x0 + CAT_GS_PANEL_WIDTH;
    catGameStatsPanel.y0 = (MAIN_WindowHeight - CAT_GS_PANEL_HEIGHT) / 2;
    catGameStatsPanel.y1 = CAT_GS_PANEL_HEIGHT + catGameStatsPanel.y0;
    colW = (catGameStatsPanel.x1 - (CAT_GS_PICKER_GAP + h) * 2 - CAT_GS_GAP_X - CAT_GS_TITLE_X - catGameStatsPanel.x0 -
            maxW) / 8;
    catGameStatsExit.x0 = (MAIN_WindowWidth - CAT_GS_EXIT_WIDTH) / 2;
    catGameStatsExit.y0 = MAIN_WindowHeight - CAT_GS_EXIT_DY;
    catGameStatsExit.x1 = catGameStatsExit.x0 + CAT_GS_EXIT_WIDTH;
    catGameStatsExit.y1 = CAT_GS_EXIT_HEIGHT + catGameStatsExit.y0;
    x = CAT_GS_PLAYER_X + catGameStatsPanel.x0;
    y = (8 - numPlayers) * h + CAT_GS_TOP + catGameStatsPanel.y0;
    for (index = 0; index < numPlayers; index++)
    {
        catGameStatsPlayerX[index] = x;
        catGameStatsPlayerY[index] = y;
        y += CAT_GS_LINE_GAP + h;
    }
    for (index = 0; index < CAT_GS_COLUMNS; index++)
    {
        catGameStatsTitleX[index] = (CAT_GS_TITLE_X + catGameStatsPanel.x0 + maxW) -
                                    fontWidth(strGetString(catGameStatsTitles[index]));
        if (index < 1)
        {
            catGameStatsTitleY[index] = CAT_GS_ICON + CAT_GS_HEADER_GAP + CAT_GS_TOP + catGameStatsPanel.y0 + headerH + h;
        }
        else
        {
            catGameStatsTitleY[index] = catGameStatsTitleY[index - 1] + h;
            if ((index == 5) || (index == 8) || (index == 11) || (index == 14))
            {                                               // half a line between the groups
                catGameStatsTitleY[index] = catGameStatsTitleY[index - 1] + h + h / 2;
            }
        }
        catGameStatsPickerLeft[index].x0 = fontWidth(strGetString(catGameStatsTitles[index])) + CAT_GS_PICKER_GAP +
                                           catGameStatsTitleX[index];
        catGameStatsPickerLeft[index].x1 = catGameStatsPickerLeft[index].x0 + h;
        catGameStatsPickerLeft[index].y0 = catGameStatsTitleY[index];
        catGameStatsPickerLeft[index].y1 = catGameStatsTitleY[index] + h;
        catGameStatsPickerRight[index].x0 = CAT_GS_PICKER_GAP + numPlayers * colW + catGameStatsPickerLeft[index].x1;
        catGameStatsPickerRight[index].x1 = catGameStatsPickerRight[index].x0 + h;
        catGameStatsPickerRight[index].y0 = catGameStatsPickerLeft[index].y0;
        catGameStatsPickerRight[index].y1 = catGameStatsPickerLeft[index].y1;
    }
    catGameStatsHeader.x0 = catGameStatsPanel.x0;
    catGameStatsHeader.y0 = catGameStatsTitleY[0] - h;
    catGameStatsHeader.x1 = catGameStatsPickerLeft[0].x0;
    catGameStatsHeader.y1 = catGameStatsTitleY[CAT_GS_COLUMNS - 1] + h * 2;
    x = catGameStatsPickerLeft[0].x1;
    for (index = 0; index < MAX_MULTIPLAYER_PLAYERS; index++)
    {
        catGameStatsPlayerColumn[index].x0 = x;
        catGameStatsPlayerColumn[index].y0 = CAT_GS_ICON + CAT_GS_HEADER_GAP + CAT_GS_TOP + catGameStatsPanel.y0 + headerH + h;
        catGameStatsPlayerColumn[index].x1 = x + colW;
        catGameStatsPlayerColumn[index].y1 = catGameStatsTitleY[CAT_GS_COLUMNS - 1] + h;
        x += colW;
    }
    for (index = 0; index < MAX_MULTIPLAYER_PLAYERS; index++)
    {                                                       // the player numbers' icons over their columns
        catGameStatsPlayerIcon[index].x0 = catGameStatsPlayerColumn[index].x0 + (colW - CAT_GS_ICON) / 2;
        catGameStatsPlayerIcon[index].x1 = catGameStatsPlayerIcon[index].x0 + CAT_GS_ICON;
        catGameStatsPlayerIcon[index].y0 = (catGameStatsPlayerColumn[index].y0 - CAT_GS_ICON) - h;
        catGameStatsPlayerIcon[index].y1 = catGameStatsPlayerIcon[index].y0 + CAT_GS_ICON;
    }
    catGameStatsPanel.x0 -= 10;
    catGameStatsPanel.x1 += 10;
    catGameStatsPanel.y0 = catGameStatsPlayerY[0] - 10;
    catGameStatsGraph.y0 = catGameStatsPlayerY[0];
    catGameStatsPanel.y1 = catGameStatsHeader.y1 + 10;
    catGameStatsGraph.x0 = catGameStatsPlayerX[0];
    catGameStatsGraph.y1 = fontHeight("") + catGameStatsPlayerY[(numPlayers > 0) ? numPlayers - 1 : 0];
    catGameStatsGraph.x1 = catGameStatsPlayerColumn[MAX_MULTIPLAYER_PLAYERS - 1].x1;
}

// the players by survival time, longest first (a bubble sort that restarts after each swap, as the exe's)
static void catGameStatsSort(void)
{
    sdword index, swap;

    for (index = 0; index < universe.numPlayers; index++)
    {
        catGameStatsOrder[index] = index;
    }
    index = 0;
    while (index < universe.numPlayers - 1)
    {
        if (cat_gameStatsValueGet(catGameStatsOrder[index + 1], 0) > cat_gameStatsValueGet(catGameStatsOrder[index], 0))
        {
            swap = catGameStatsOrder[index];
            catGameStatsOrder[index] = catGameStatsOrder[index + 1];
            catGameStatsOrder[index + 1] = swap;
            index = 0;
        }
        else
        {
            index++;
        }
    }
    cat_timerStartRealTime(&catGameStatsBarTimer, 2.0f);
}

void cat_gameStatsEnd(void)                                                                 // 0x00489290
{
    universePause = FALSE;
    mrRenderMainScreen = TRUE;
    memFree(catGameStatsRows);
    catGameStatsRows = NULL;
    mrEnable();
    catGameStatsActive = FALSE;
    hrShutdownBackground();
    utyGameQuitToMain(NULL, NULL);
}

/*-----------------------------------------------------------------------------
    Name        : cat_gameStatsClick                                0x004892e0
    Description : A click: the exit button ends the screen; a column's picker selects that column (the bars grow
                  again over 2 s).
----------------------------------------------------------------------------*/
void cat_gameStatsClick(void)
{
    sdword column;

    if (!catGameStatsActive)
    {
        return;
    }
    if ((catGameStatsExit.x0 <= mouseCursorXPosition) && (catGameStatsExit.y0 <= mouseCursorYPosition) &&
        (mouseCursorXPosition < catGameStatsExit.x1) && (mouseCursorYPosition < catGameStatsExit.y1))
    {
        cat_gameStatsEnd();
        return;
    }
    for (column = 0; column < CAT_GS_COLUMNS; column++)
    {
        if ((catGameStatsPickerLeft[column].x0 <= mouseCursorXPosition) &&
            (catGameStatsPickerLeft[column].y0 <= mouseCursorYPosition) &&
            (mouseCursorXPosition < catGameStatsPickerRight[column].x1) &&
            (mouseCursorYPosition < catGameStatsPickerLeft[column].y1))
        {
            catGameStatsColumn = column;
            catGameStatsSort();
            if (!catGameStatsShown)
            {
                catGameStatsShown = TRUE;
            }
            soundEventPlay(NULL, 0x109, NULL);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_gameStatsDraw                                 0x004896b0
    Description : The screen: the translucent panel in the popup menu's frame, the exit button, the 17 titles with
                  every player's values (the selected column green, the hovered or selected one with its picker), the
                  bar graph of the selected column (each player's name, race icon and team-colour bar growing over
                  2 s) and the players' number icons.
----------------------------------------------------------------------------*/
void cat_gameStatsDraw(void)
{
    fonthandle oldFont;
    rectangle box, inner, bar;
    sdword column, slot, player, left, right, mid;
    real32 maxValue, fraction, grow;
    color c;
    char text[136];

    oldFont = fontMakeCurrent(catGameStatsFont);
    // CAT: the exe reads the arrow keys' states directly (keyScanCode VK_DOWN / VK_UP), every frame they are down
    if (keyIsHit(ARRDOWN) || keyIsHit(ARRUP))
    {
        catGameStatsColumn += keyIsHit(ARRDOWN) ? 1 : -1;
        if (catGameStatsColumn < 0)
        {
            catGameStatsColumn = CAT_GS_COLUMNS - 1;
        }
        else if (catGameStatsColumn >= CAT_GS_COLUMNS)
        {
            catGameStatsColumn = 0;
        }
        catGameStatsSort();
        if (!catGameStatsShown)
        {
            catGameStatsShown = TRUE;
        }
    }
    rndTextureEnable(FALSE);
    rndLightingEnable(FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    if (!primModeEnabled)
    {
        primModeSet2();
    }
    inner.x0 = catGameStatsPanel.x0;
    inner.y0 = catGameStatsPlayerIcon[0].y0 - 6;
    inner.x1 = catGameStatsPanel.x1;
    inner.y1 = catGameStatsPickerLeft[CAT_GS_COLUMNS - 1].y1;
    box.x0 = inner.x0 - 16;
    box.y0 = inner.y0 - 16;
    box.x1 = inner.x1 + 16;
    box.y1 = inner.y1 + 16;
    primRectTranslucent2(&box, CAT_GS_BACKDROP);
    cat_popupMenuDrawFrame(&inner, FALSE, 0x14);
    cat_gameStatsButtonDraw(&catGameStatsExit, strGetString(str_GSD_Exit), 1, catGameStatsButtonFont, CAT_GS_EXIT_CAP,
                            CAT_OI_Stats_Exit_Button_Left, CAT_OI_Stats_Exit_Button_Left_Mouse,
                            CAT_OI_Stats_Exit_Button_Mid, CAT_OI_Stats_Exit_Button_Mid_Mouse,
                            CAT_OI_Stats_Exit_Button_Right, CAT_OI_Stats_Exit_Button_Right_Mouse, TRUE);
    for (column = 0; column < CAT_GS_COLUMNS; column++)
    {
        c = (column & 1) ? 0xffb4b4b4 : 0xffffffff;
        if (catGameStatsColumn == column)
        {
            c = 0xff00ff00;
        }
        fontPrint(catGameStatsTitleX[column], catGameStatsTitleY[column], c,
                  strGetString(catGameStatsTitles[column]));
        left = -1;
        if ((catGameStatsPickerLeft[column].x0 <= mouseCursorXPosition) &&
            (catGameStatsPickerLeft[column].y0 <= mouseCursorYPosition) &&
            (mouseCursorXPosition < catGameStatsPickerRight[column].x1) &&
            (mouseCursorYPosition < catGameStatsPickerLeft[column].y1))
        {
            left = CAT_OI_Stats_Picker_1_Mouse;
            right = CAT_OI_Stats_Picker_2_Mouse;
            mid = CAT_OI_Stats_Picker_Mid_Mouse;
        }
        else if (column == catGameStatsColumn)
        {
            left = CAT_OI_Stats_Picker_1;
            right = CAT_OI_Stats_Picker_2;
            mid = CAT_OI_Stats_Picker_Mid;
        }
        if (left != -1)
        {                                                   // the picker: the left cap, the right cap, the middle
            cat_overlayIconDraw(left, catGameStatsPickerLeft[column].y0, catGameStatsPickerLeft[column].x0,
                                catGameStatsPickerLeft[column].x1 - catGameStatsPickerLeft[column].x0,
                                catGameStatsPickerLeft[column].y1 - catGameStatsPickerLeft[column].y0, 0xffffffff);
            cat_overlayIconDraw(right, catGameStatsPickerRight[column].y0, catGameStatsPickerRight[column].x0,
                                catGameStatsPickerRight[column].x1 - catGameStatsPickerRight[column].x0,
                                catGameStatsPickerRight[column].y1 - catGameStatsPickerRight[column].y0, 0xffffffff);
            cat_overlayIconDraw(mid, catGameStatsPickerLeft[column].y0, catGameStatsPickerLeft[column].x1,
                                catGameStatsPickerRight[column].x0 - catGameStatsPickerLeft[column].x1,
                                catGameStatsPickerRight[column].y1 - catGameStatsPickerRight[column].y0, 0xffffffff);
        }
        for (slot = 0; slot < universe.numPlayers; slot++)
        {                                                   // the values, right-aligned in the players' columns
            cat_gameStatsValuePrint(catGameStatsOrder[slot], column, text);
            fontPrint(catGameStatsPlayerColumn[slot].x1 - fontWidth(text), catGameStatsTitleY[column], c, text);
        }
    }
    maxValue = 0.0f;
    for (slot = 0; slot < universe.numPlayers; slot++)
    {
        if (maxValue < cat_gameStatsValueGet(catGameStatsOrder[slot], catGameStatsColumn))
        {
            maxValue = cat_gameStatsValueGet(catGameStatsOrder[slot], catGameStatsColumn);
        }
    }
    box.x0 = catGameStatsGraph.x0 - 16;
    box.y0 = catGameStatsGraph.y0 - 16;
    box.x1 = catGameStatsGraph.x1 + 16;
    box.y1 = catGameStatsGraph.y1 + 16;
    primRectSolid2(&box, 0xff000000);
    cat_popupMenuDrawFrame(&catGameStatsGraph, FALSE, 0x14);
    for (slot = 0; slot < universe.numPlayers; slot++)
    {
        player = catGameStatsOrder[slot];
        grow = cat_timerFractionElapsed(&catGameStatsBarTimer);
        fraction = 0.0f;
        if (maxValue > 0.0f)
        {
            fraction = cat_gameStatsValueGet(player, catGameStatsColumn) / maxValue;
        }
        sprintf(text, "%d. %s", slot + 1, playerNames[player]);
        if (player == universe.curPlayerIndex)
        {
            c = 0xff00ff00;
        }
        else
        {                                                   // allies blue, enemies red
            c = allianceArePlayersAllied(&universe.players[player], universe.curPlayerPtr) ? 0xffff0000 : 0xff0000ff;
        }
        if ((catGameStatsPlayerColumn[slot].x0 <= mouseCursorXPosition) &&
            (catGameStatsPlayerColumn[slot].y0 <= mouseCursorYPosition) &&
            (mouseCursorXPosition < catGameStatsPlayerColumn[slot].x1) &&
            (mouseCursorYPosition < catGameStatsPlayerColumn[slot].y1))
        {
            c = 0xffffffff;
        }
        fontPrint(fontHeight("") + catGameStatsPlayerX[slot] + 5, catGameStatsPlayerY[slot], c, text);
        cat_overlayIconDraw((universe.players[player].race != 0) ? CAT_OI_BeastMothership : CAT_OI_SectMothership,
                            catGameStatsPlayerY[slot], catGameStatsPlayerX[slot], fontHeight(""), fontHeight(""),
                            0xffffffff);
        if (fraction > grow)
        {                                                   // the bar grows to its length over 2 s
            fraction = grow;
        }
        bar.x0 = catGameStatsPlayerColumn[0].x0;
        bar.x1 = (sdword)((real32)(catGameStatsPlayerColumn[MAX_MULTIPLAYER_PLAYERS - 1].x1 - bar.x0) * fraction) + bar.x0;
        bar.y0 = catGameStatsPlayerY[slot];
        bar.y1 = fontHeight("") + bar.y0 - 1;
        if (bar.x0 < bar.x1)
        {
            cat_primRectShadedHorizontal2(&bar, teColorSchemes[player].textureColor.base, 0xffffffff);
        }
        primLine2(catGameStatsPlayerColumn[slot].x1, catGameStatsPlayerColumn[slot].y0, catGameStatsPlayerColumn[slot].x1,
                  catGameStatsPlayerColumn[slot].y1, 0xffffffff);
        cat_overlayIconDraw(CAT_OI_Stats_First + slot, catGameStatsPlayerIcon[slot].y0, catGameStatsPlayerIcon[slot].x0,
                            catGameStatsPlayerIcon[slot].x1 - catGameStatsPlayerIcon[slot].x0,
                            catGameStatsPlayerIcon[slot].y1 - catGameStatsPlayerIcon[slot].y0, 0xffffffff);
    }
    fontMakeCurrent(oldFont);
}

/*-----------------------------------------------------------------------------
    Name        : cat_gameStatsBegin                                0x00489e90
    Description : The end of a game: the universe pauses, the main region stops taking orders, the rows are taken
                  from the game statistics, the loading screen's background starts, and the stats music only when
                  the player is called "TerryStats" (any case). [audit: was "unless"; the exe's _strnicmp result 0
                  plays it]
----------------------------------------------------------------------------*/
void cat_gameStatsBegin(void)
{
    sdword player;

    if (smSensorsActive)
    {
        smSensorsCloseForGood();
    }
    mrDisable();
    universePause = TRUE;
    mrRenderMainScreen = FALSE;                     // 0x008b9ecc (globalsInit, cmClose and cmConstructionBegin write it too)
    catGameStatsShown = FALSE;
    if (!catGameStatsActive)
    {
        catGameStatsRows = memAlloc(universe.numPlayers * CAT_GS_ROW_WORDS * sizeof(sdword), "catGameStatsRows", 0);
        for (player = 0; player < universe.numPlayers; player++)
        {
            cat_gameStatsPlayerRowFill(player, &catGameStatsRows[player * CAT_GS_ROW_WORDS]);
        }
        catGameStatsActive = TRUE;
        hrInitBackground();
        cat_gameStatsLayoutCompute();
        catGameStatsColumn = 0;
        catGameStatsSort();
        if (!catGameStatsShown)
        {
            catGameStatsShown = TRUE;
        }
        mouseCursorShow();
        catMrRightMode = 0;
        cat_utyMultiplayerQuitNotify();
    }
    soundEventStopSFX(0.5f);
    soundEventPause(FALSE);
    mrReset();
    // CAT: the exe plays track 0x28 only when _strnicmp (0x006929d0) says the player IS "TerryStats" (0x0048a026
    //      jne skips it otherwise) [audit: was the inverted, case-sensitive test]
    if (strncasecmp(playerNames[universe.curPlayerIndex], "TerryStats", MAX_PERSONAL_NAME_LEN) == 0)
    {
        soundEventPlayMusic(0x28);                  // the game-statistics track (SpeechEvent.c CAT_MUSIC_GAME_STATS)
        cat_musicRadioReset();
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_gameStatsIsActive                             0x0048a050
----------------------------------------------------------------------------*/
bool32 cat_gameStatsIsActive(void)
{
    return catGameStatsActive;
}
