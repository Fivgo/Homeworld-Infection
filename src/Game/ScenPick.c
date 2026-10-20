/*=============================================================================
    Name    : ScenPick.c
    Purpose : Functionality to choose scenarios

    Created 10/21/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifdef _WIN32
    #include <io.h>
#else
    #include <sys/stat.h>
    #include <dirent.h>
#endif

#include <stdlib.h>
#include <string.h>

#if !defined _MSC_VER
#include <strings.h>
#endif

#include <ctype.h>
#include "Debug.h"
#include "File.h"
#include "Memory.h"
#include "utility.h"
#include "font.h"
#include "FontReg.h"
#include "prim2d.h"
#include "mouse.h"
#include "Scroller.h"
#include "ScenPick.h"
#include "texreg.h"
#include "render.h"
#include "BMP.h"
#include "MaxMultiplayer.h"
#include "MultiplayerGame.h"
#include "FEColour.h"
#include "StatScript.h"
#include "Subtitle.h"
#include "StringSupport.h"

#include "CatSort.h"         // CAT (M2-R9 oracle-rung-physics): the exe's qsort
#ifdef _MSC_VER
    #define strcasecmp _stricmp
#endif

/*=============================================================================
    Data:
=============================================================================*/

char DefaultScenario[100] = "Default";

void spScenarioWindowInit(char *name, featom *atom);
void spScenarioNameDraw(featom *atom, regionhandle region);
void spScenarioBitmap(char *name, featom *atom);

// CAT: CS_ScenarioBitmap is a list window of the description's lines, set up by a function callback (the exe's tables
//      0x00892750 / 0x00892768: spCallbacks has it, spDrawCallbacks only CS_ScenarioName); HW1 drew the preview
//      bitmap and the text in a user region
fecallback spCallbacks[] =
{
    {spScenarioWindowInit, "CS_ScenarioWindowInit"},
    {spScenarioBitmap, "CS_ScenarioBitmap"},
    {NULL, NULL}
};

fedrawcallback spDrawCallbacks[] =
{
    {spScenarioNameDraw,   "CS_ScenarioName"},
    {NULL, NULL}
};

static listwindowhandle spDescriptionListWindow = NULL;  // CAT: 0x00a57930, the CS_ScenarioBitmap list window
#define SCP_DescriptionInset    6                   // CAT: spNewItem 0x00504a20 insets the text by 6 (HW1: 3)
#define SCP_NameLength          32                  // CAT: memStrncpy(.., 0x1f): the list and the name show at most 30 characters

listwindowhandle spScenarioListWindow = NULL;
//regionhandle     spScenarioBitmapWindow = NULL;

#ifdef _WIN32
char *scenarioRootPath = "MultiPlayer\\";
#else
char *scenarioRootPath = "MultiPlayer/";
#endif

lifheader *scenarioImage;
udword scenarioTexture = TR_InvalidInternalHandle;

sdword spTextureWidth, spTextureHeight;
ubyte* spTextureData = NULL;

bool32 spPickerStarted = FALSE;

#define SCP_TEXTURE_INSET 3


spscenario *spScenarios;                        //list of available scenarios
sdword spCurrentSelected = 0;                   //current scenario index, if OK is pressed
sdword spNumberScenarios = 0;                   //number of available scenarios
sdword spScenarioListLength = SCP_ScenarioListLength;//length of scenario list

fonthandle spListFont, spNameFont;              //fonts used for font printing

color spSelectedColor = SCP_SelectedColor;       //color to draw selected one at

uword spTopIndex = 0;                          //index of what is at the top of the list
uword spDisplayLength = SCP_DisplayListLength;  //number of scenarios displayed at one time

//text description stuff
sdword spNDescriptionLines = 0;                 //number of text description lines
char **spDescriptionLines = NULL;               //the actual lines
char *spDescription = NULL;                     //the actual description (may be chopped)
fonthandle spDescriptionFont = FONT_InvalidFontHandle;//font to print in
bool32 spDescriptionShadow = FALSE;                      //print with a dropshadow?
color spDescriptionColor = colWhite;            //print in what color?
GameType *gameTypeFromDescription = NULL;
static void spMissionDescriptionSet(char *directory,char *field,void *dataToFillIn);
static void spDescriptionFontSet(char *directory,char *field,void *dataToFillIn);
extern scriptStructEntry StaticMGInfoScriptTable[];
scriptEntry spDescriptionTweaks[] =
{
    {"Description", spMissionDescriptionSet, NULL},
    {"Font", spDescriptionFontSet, NULL},
    {"Color", scriptSetRGBCB, &spDescriptionColor},
    {"DropShadow", scriptSetBool8, &spDescriptionShadow},
    
    END_SCRIPT_ENTRY
};

scriptEntry spDescriptionTweaksFrench[] =
{
    {"DescriptionFrench", spMissionDescriptionSet, NULL},
    {"Font", spDescriptionFontSet, NULL},
    {"Color", scriptSetRGBCB, &spDescriptionColor},
    {"DropShadow", scriptSetBool8, &spDescriptionShadow},
    
    END_SCRIPT_ENTRY
};

scriptEntry spDescriptionTweaksGerman[] =
{
    {"DescriptionGerman", spMissionDescriptionSet, NULL},
    {"Font", spDescriptionFontSet, NULL},
    {"Color", scriptSetRGBCB, &spDescriptionColor},
    {"DropShadow", scriptSetBool8, &spDescriptionShadow},
    
    END_SCRIPT_ENTRY
};

scriptEntry spDescriptionTweaksSpanish[] =
{
    {"DescriptionSpanish", spMissionDescriptionSet, NULL},
    {"Font", spDescriptionFontSet, NULL},
    {"Color", scriptSetRGBCB, &spDescriptionColor},
    {"DropShadow", scriptSetBool8, &spDescriptionShadow},
    
    END_SCRIPT_ENTRY
};

scriptEntry spDescriptionTweaksItalian[] =
{
    {"DescriptionItalian", spMissionDescriptionSet, NULL},
    {"Font", spDescriptionFontSet, NULL},
    {"Color", scriptSetRGBCB, &spDescriptionColor},
    {"DropShadow", scriptSetBool8, &spDescriptionShadow},
    
    END_SCRIPT_ENTRY
};

/*=============================================================================
    Private functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : spScenarioItemDraw
    Description : Draw the list of available scenarios
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void spScenarioItemDraw(rectangle *rect, listitemhandle data)
{
    char            temp[SCP_NameLength];
    sdword          x, y;
    color           c;
    fonthandle      oldfont;
    spscenario     *spScenario = (spscenario *)data->data;

    oldfont = fontMakeCurrent(spListFont);

    if (data->flags&UICLI_Selected)
        c = FEC_ListItemSelected;
    else
        c = FEC_ListItemStandard;

    x = rect->x0;//+MG_HorzSpacing;
    y = rect->y0;//+MG_VertSpacing/2;

    memStrncpy(temp, spScenario->title, SCP_NameLength - 1);  // CAT: spScenarioItemDraw 0x00503da0 (HW1: the whole title)
    fontPrint(x,y,c,temp);

    fontMakeCurrent(oldfont);
}

/*-----------------------------------------------------------------------------
    Name        : spScenarioNameDraw
    Description : Draw the currently selected scenario name in screens other
                    than the scenario picker.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void spScenarioNameDraw(featom *atom, regionhandle region)
{
    fonthandle fhSave;
    char scenarioName[128];
    bool32 useDisplayName = FALSE;
    unsigned int i;

    rectangle *r = &region->rect;

    fhSave = fontMakeCurrent(spNameFont); //select the appropriate font

    if (multiPlayerGame && (!GameCreator))
    {
        if (LANGame)
        {
            if (currentScreen == MGS_Basic_Options_View)
            {
                useDisplayName = TRUE;
            }
        }
        else
        {
            if (WaitingForGame)
            {
                useDisplayName = TRUE;
            }
        }
    }

    // CAT: spScenarioNameDraw 0x00503e10 keeps 30 characters (memStrncpy 0x1f; HW1: 126)
    if (useDisplayName)
        memStrncpy(scenarioName, tpGameCreated.DisplayMapName, SCP_NameLength - 1);
    else
        memStrncpy(scenarioName, spScenarios[spCurrentSelected].title, SCP_NameLength - 1);
    for (i = 0; (scenarioName[i] = toupper(scenarioName[i])); i++) { }
    fontPrintf(r->x0, r->y0, FEC_ListItemSelected, scenarioName);
    fontMakeCurrent(fhSave);
}

/*-----------------------------------------------------------------------------
    Name        : spTitleFind
    Description : Finds the title of a mission file (on first line, in brackets)
    Inputs      : directory, fileName - combine to form path to the file.
    Outputs     : Allocated memory for and duplicates a string.
    Return      : Pointer to title string.
----------------------------------------------------------------------------*/
char *spTitleFind(char *directory, char *fileName)
{
    char string[256], fullName[PATH_MAX];
    char *ptr;
    filehandle handle;
    sdword status;

    // <directory>/<filename>/<filename>.level
    memStrncpy(fullName, directory, PATH_MAX - 1);
    
#ifdef _WIN32
    strcat(fullName,"\\");
#else
    strcat(fullName,"/");
#endif

    strcat(fullName, fileName);

#ifdef _WIN32
    strcat(fullName,"\\");
#else
    strcat(fullName,"/");
#endif

    strcat(fullName, fileName);
    strcat(fullName, ".level");

    handle = fileOpen(fullName, FF_TextMode|FF_ReturnNULLOnFail|FF_IgnorePrepend);
    if (!handle)
    {
        return NULL;
    }

    // go through the .level file and find the title which is expected to look
    // like ^[<title text>]$ but some editors leave trailing whitespace at the
    // end which we need to take account of 
    for (;;)
    {
        status = fileLineRead(handle,string,256);

        if (status == FR_EndOfFile)
        {
            break;
        }

        // blank lines, comments (;) etc
        if ((string[0] == '\n') || (string[0] == '/') || (string[0] == ';') || (string[0] == ' '))
        {
            continue;
        }

        if (string[0] == '[')
        {
            // could use strchr here to find the closing square bracket but there is
            // always the possibility someone has created a map with square brackets
            // in the name so for the sake of backwards compatibility...
            
            // look for ']' starting at end of string
            ptr = string + strlen(string) - 1; // last character
            while (*ptr != ']' && ptr > string)
            {
                ptr--;
            }
            
            // title must be at least one character so string must be at least three
            // with enclosing square brackets: "t" -> "[t]"
            if (strlen(string) >= 3 && *ptr == ']')
            {
                fileClose(handle);
                *ptr = '\0';
                return(memStringDupe(&string[1]));
            }
        }
    }

    fileClose(handle);

    return memStringDupe(fullName);
}

/*-----------------------------------------------------------------------------
    Name        : compareScenariosCB
    Description : compare's the scenarios alphabetically
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
int compareScenariosCB(const void *arg1,const void *arg2)
{
    int result;

    spscenario *scenario1 = (spscenario *)arg1;
    spscenario *scenario2 = (spscenario *)arg2;

    result = strcasecmp(scenario1->title,scenario2->title);
    return result;
}

/*-----------------------------------------------------------------------------
    Name        : spScenarioFind
    Description : Find a scenario by name
    Inputs      : scenarioName - name of scenario to find
    Outputs     :
    Return      : Index of specified scenario or 0 if not found
----------------------------------------------------------------------------*/
sdword spScenarioFind(char *scenarioName)
{
    sdword index;

    for (index = 0; index < spNumberScenarios; index++)
    {
        if (strstr(spScenarios[index].title, scenarioName) == spScenarios[index].title)
        {                                               //if this is the default level
            return(index);
            spCurrentSelected = index;                  //set default scenario
            break;
        }
    }
    return(0);
}

/*-----------------------------------------------------------------------------
    Name        : spTitleListLoad
    Description : Scans the missions directory and loads in the titles of all
                    the mission files available.
    Inputs      : void
    Outputs     : Fills in spScenarios and spNumberScenarios
    Return      : void
    Note        : CAT: spTitleListLoad 0x005040c0: every map name (a line of multiPlayerMissions.script or a directory
                    of MultiPlayer) is its own scenario: fileSpec and bitmapfileSpec are the whole name (HW1: the name
                    up to its number plus "%d", the sizes of one map merged into one entry with min / max players),
                    maxplayers is the name's number and minplayers 2 (gameStart's sprintf of the level name then
                    copies the name as it is)
----------------------------------------------------------------------------*/
void spTitleListLoad(void)
{
#if !(defined (HW_GAME_DEMO) || defined(HW_GAME_RAIDER_RETREAT))
#ifdef _WIN32
    struct _finddata_t find;
    intptr_t handle, startHandle;
#else
    DIR* dp;
    struct dirent* dir_entry;
#endif
#endif
    sdword index, numplayers;
    char fileName[PATH_MAX], nameBuffer[PATH_MAX];
    char bitmapfileName[PATH_MAX];
    char *pString;
    char *title;
    filehandle scriptFile;

#ifdef HW_GAME_DEMO
    scriptFile = fileOpen("DemoMissions.script", FF_TextMode);
#else
    // HW Raider Retreat has all missions!
    scriptFile = fileOpen("multiPlayerMissions.script", FF_TextMode);
#endif
    dbgAssertOrIgnore(scriptFile != 0);

    while (fileLineRead(scriptFile, nameBuffer, PATH_MAX) != FR_EndOfFile)
    {
        if (nameBuffer[0] == ';' || (nameBuffer[0] == '/' && nameBuffer[1] == '/'))
        {
            continue;
        }
        numplayers = 0;
        for (pString = nameBuffer; *pString != 0; pString++)
        {                                                   //search for a numeral character
            if (strchr("0123456789", *pString) != NULL)
            {                                               //if this is a numeral
				sscanf( pString, "%d", &numplayers );
                memset(fileName, 0, PATH_MAX);
                // CAT: the whole name is the scenario (spTitleListLoad 0x005040c0 copies the line, no "%d" pattern)
                memStrncpy(fileName, nameBuffer, PATH_MAX);
                memStrncpy(bitmapfileName, nameBuffer, PATH_MAX);
            }
        }

        if (numplayers == 0)
        {
            goto alreadyLoaded;
        }

#ifdef _WIN32
        title = spTitleFind("MultiPlayer\\", nameBuffer);
#else
        title = spTitleFind("MultiPlayer/", nameBuffer);
#endif
        if (title == NULL)
        {
            goto alreadyLoaded;                             //break-continue
        }

        // CAT: no merge of the sizes of one map: every line is its own scenario
        if (spNumberScenarios >= spScenarioListLength)
        {
            spScenarioListLength += SP_ScenarioListGrowth;
            spScenarios = memRealloc(spScenarios, spScenarioListLength * sizeof(spscenario), "spScenarios", NonVolatile);
        }
		
        dbgAssertOrIgnore(spNumberScenarios < spScenarioListLength);
        spScenarios[spNumberScenarios].fileSpec = memStringDupe(fileName);
        spScenarios[spNumberScenarios].bitmapfileSpec = memStringDupe(bitmapfileName);
        spScenarios[spNumberScenarios].title = title;
        spScenarios[spNumberScenarios].maxplayers = numplayers;
        spScenarios[spNumberScenarios].minplayers = 2;      //CAT: every scenario plays from two players
        spNumberScenarios++;

alreadyLoaded:;
    }
    fileClose(scriptFile);

#if !(defined (HW_GAME_DEMO) || defined(HW_GAME_RAIDER_RETREAT))
#ifdef _WIN32
    startHandle = handle = _findfirst(filePathPrepend("MultiPlayer\\*.", FF_HomeworldDataPath), &find);

    while (handle != -1)
    {
        if (find.name[0] == '.')
        {
            goto alreadyLoadedFromFileSystem;
        }
        fileName[0] = 0;
        numplayers = 0;
        for (pString = find.name; *pString != 0; pString++)
        {                                                   //search for a numeral character
            if (strchr("0123456789", *pString) != NULL)
            {                                               //if this is a numeral
				sscanf( pString, "%d", &numplayers );
                memset(fileName, 0, PATH_MAX);
                memStrncpy(fileName, find.name, PATH_MAX);  //CAT: the whole name
                memStrncpy(bitmapfileName, find.name, PATH_MAX);
            }
        }
        if (numplayers == 0)
        {
            goto alreadyLoadedFromFileSystem;
        }

        if (fileName[0] == 0)
        {
            goto alreadyLoadedFromFileSystem;
        }

        title = spTitleFind(filePathPrepend("MultiPlayer", FF_HomeworldDataPath), find.name);
        if (title == NULL)
        {
            goto alreadyLoadedFromFileSystem;
        }

        if (spNumberScenarios >= spScenarioListLength)
        {
            spScenarioListLength += SP_ScenarioListGrowth;
            spScenarios = memRealloc(spScenarios, spScenarioListLength * sizeof(spscenario), "spScenarios", NonVolatile);
        }
        dbgAssertOrIgnore(spNumberScenarios < spScenarioListLength);
        spScenarios[spNumberScenarios].fileSpec = memStringDupe(fileName);
        spScenarios[spNumberScenarios].bitmapfileSpec = memStringDupe(bitmapfileName);
        spScenarios[spNumberScenarios].title = title;
        spScenarios[spNumberScenarios].maxplayers = numplayers;
        spScenarios[spNumberScenarios].minplayers = 2;      //CAT
        spNumberScenarios++;

alreadyLoadedFromFileSystem:;
        handle = _findnext(startHandle, &find);
    }
#else   /* File search, not _WIN32... */

    dp = opendir(filePathPrepend("MultiPlayer", FF_HomeworldDataPath));

    if (dp)
    {
        while ((dir_entry = readdir(dp)))
        {
            if (dir_entry->d_name[0] == '.')
                continue;

            fileName[0] = 0;
            numplayers = 0;
			
            for (pString = dir_entry->d_name; *pString != 0; pString++)
            {                                                   //search for a numeral character
                if (strchr("0123456789", *pString) != NULL)
                {                                               //if this is a numeral
                    sscanf( pString, "%d", &numplayers );
                    memset(fileName, 0, PATH_MAX);
                    memStrncpy(fileName, dir_entry->d_name, PATH_MAX);          //CAT: the whole name
                    memStrncpy(bitmapfileName, dir_entry->d_name, PATH_MAX);
                }
            }

            if (numplayers == 0)
                continue;

            if (fileName[0] == '\0')
                continue;

            title = spTitleFind(filePathPrepend("MultiPlayer", FF_HomeworldDataPath), dir_entry->d_name);
            if (title == NULL)
            {
                continue;
            }
            
            if (spNumberScenarios >= spScenarioListLength)
            {
                spScenarioListLength += SP_ScenarioListGrowth;
                spScenarios = memRealloc(spScenarios, spScenarioListLength * sizeof(spscenario), "spScenarios", NonVolatile);
            }
			
            dbgAssertOrIgnore(spNumberScenarios < spScenarioListLength);
            spScenarios[spNumberScenarios].fileSpec = memStringDupe(fileName);
            spScenarios[spNumberScenarios].bitmapfileSpec = memStringDupe(bitmapfileName);
            spScenarios[spNumberScenarios].title = title;
            spScenarios[spNumberScenarios].maxplayers = numplayers;
            spScenarios[spNumberScenarios].minplayers = 2;  //CAT
            spNumberScenarios++;
        }

        closedir(dp);
    }
#endif // _WIN32
#endif // defined(HW_GAME_DEMO)...

    dbgAssertOrIgnore(spNumberScenarios > 0);
    if (spNumberScenarios > 1)
    {
        //alphabetically sort the scenario list
        // CAT (M2-R9 oracle-rung-physics): the exe's qsort 0x00671060 (CatSort.c; spTitleListLoad 0x005040c0 at 0x00504462): ties sort as in the exe
        catQsort(&spScenarios[0],spNumberScenarios,sizeof(spscenario),compareScenariosCB);
    }

    //find the default scenario's index
    if (spCurrentSelected == 0)
    {                                                       //if it's not already selected
        spCurrentSelected = spScenarioFind(DefaultScenario);
    }
}

/*=============================================================================
    Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : spStartup
    Description :
    Inputs      :
    Outputs     : Registers some callbacks for buttons and user regions.
    Return      :
----------------------------------------------------------------------------*/
void spStartup(void)
{
    feDrawCallbackAddMultiple(spDrawCallbacks);
    spListFont = frFontRegister(SCP_ListFont);
    spNameFont = frFontRegister(SCP_NameFont);
    spScenarios = memAlloc(spScenarioListLength * sizeof(spscenario), "spScenarios", NonVolatile);
    spTitleListLoad();
    if (spCurrentSelected >= spNumberScenarios)
    {
        spCurrentSelected = spScenarioFind(DefaultScenario);
    }

}

/*-----------------------------------------------------------------------------
    Name        : spShutdown
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void spShutdown(void)
{
    sdword index;

    for (index = 0; index < spNumberScenarios; index++)
    {
        memFree(spScenarios[index].bitmapfileSpec);
        memFree(spScenarios[index].fileSpec);
        memFree(spScenarios[index].title);
    }
    memFree(spScenarios);
}


void spNewItem(void);

/*-----------------------------------------------------------------------------
    Name        : spScenarioWindowInit
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void spScenarioWindowInit(char *name, featom *atom)
{
    fonthandle  oldfont;
    sdword      index;

    if (FEFIRSTCALL(atom))
    {
        oldfont = fontMakeCurrent(spListFont);

        spScenarioListWindow = (listwindowhandle)atom->pData;

        uicListWindowInit(spScenarioListWindow,
                          NULL,                             // title draw, no title
                          NULL,                             // title click process, no title
                          0/*fontHeight(" ")*2*/,                // title height, no title
                          spScenarioItemDraw,               // item draw function
                          fontHeight(" ")+SCP_VertSpacing,   // item height
                          UICLW_CanSelect|UICLW_CanHaveFocus);


        //spScenarioListWindow->reg.flags |=

        for (index = 0; index < spNumberScenarios; index++)
        {
            if (index==spCurrentSelected)
                uicListAddItem(spScenarioListWindow, (ubyte *)&spScenarios[index], UICLI_CanSelect|UICLI_Selected, UICLW_AddToTail);
            else
                uicListAddItem(spScenarioListWindow, (ubyte *)&spScenarios[index], UICLI_CanSelect, UICLW_AddToTail);
        }

        fontMakeCurrent(oldfont);
        return;
    }
    else
    {
        switch (spScenarioListWindow->message)
        {
            case CM_AcceptText:
                spDonePicking(NULL,NULL);
                break;
            case CM_DoubleClick:

                spDonePicking(NULL,NULL);
                break;
            case CM_NewItemSelected:
                spNewItem();

        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : spScenarioPick
    Description : Starts the scenario picker
    Inputs      : dest - where to store file name of destiation file.
                    Will be "" if nothing (yet) chosen.
    Outputs     : Starts the scenario picker screen.  When chosen, the name
                    will be stored in dest.
    Return      : void
----------------------------------------------------------------------------*/
void spScenarioPick(char *dest)
{
    feCallbackAddMultiple(spCallbacks);

    feScreenStart(ghMainRegion, SCP_ScreenName);

    spNewItem();

    spPickerStarted = TRUE;
}

bool32 spSelectionValid(sdword candidate)
{
    if (tpGameCreated.numPlayers + tpGameCreated.numComputers > spScenarios[candidate].maxplayers)
    {
        if (tpGameCreated.numPlayers > spScenarios[candidate].maxplayers)
        {
            return FALSE; // too many people for this map
        }
        else
        {
            // let's bump computers out for humans to join
            tpGameCreated.numComputers = spScenarios[candidate].maxplayers - tpGameCreated.numPlayers;
            // this line caused a warning in release build, numComputers is always >= 0 it is unsigned!
            //dbgAssertOrIgnore(tpGameCreated.numComputers >= 0);
            dbgAssertOrIgnore(tpGameCreated.numPlayers + tpGameCreated.numComputers <= spScenarios[candidate].maxplayers);
        }
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : spDescriptionDefaultsAndFreeText
    Description : Set text description defaults.
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void spDescriptionDefaultsAndFreeText(void)
{
    if (spDescription != NULL)
    {                                                       //if there was a description
        memFree(spDescription);                             //kill it
        spDescription = NULL;
        memFree(spDescriptionLines);
        spNDescriptionLines = 0;
    }
    spDescriptionFont = frFontRegister("default.hff");      //CAT: set printing defaults (the exe registers the font; HW1: invalid)
    spDescriptionShadow = FALSE;
    spDescriptionColor = colWhite;
    if (gameTypeFromDescription != NULL)
    {
        memFree(gameTypeFromDescription);
        gameTypeFromDescription = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : spDonePicking/spBackPicking
    // if the region is the scenario picker
    Description : Close the color picker and optionally specify the file path
                    of the chosen file.
    Inputs      : standard FE callback
    Outputs     : DonePicking stores chosen file name in parameter to spScenarioPick
    Return      : void
----------------------------------------------------------------------------*/
void spDonePicking(char *name, featom *atom)
{
    sdword index;
    sdword spCurrentSelectedCandidate = spCurrentSelected;

    if (spScenarioListWindow->CurLineSelected!=NULL)
    {
        for (index = 0; index < spNumberScenarios; index++)
        {
            if (spScenarioListWindow->CurLineSelected->data == (ubyte *)&spScenarios[index])
            {
                spCurrentSelectedCandidate = index;
                break;
            }
        }
    }

    if (gameTypeFromDescription != NULL)
    {                                                       //if there is a game type from description file
        mgSetGameTypeByStruct(gameTypeFromDescription);     //set that game type
        feAllCallOnCreate(feStack[feStackIndex - 1].screen);//update buttons to reflect changes
        mgGameTypesOtherButtonPressed();
    }
    spDescriptionDefaultsAndFreeText();

    if (scenarioTexture != TR_InvalidInternalHandle)
    {                                                       //delete the preview image, if there was one
        trRGBTextureDelete(scenarioTexture);
        scenarioTexture = TR_InvalidInternalHandle;
        if (spTextureData != NULL)
        {
            memFree(spTextureData);
            spTextureData = NULL;
        }
    }

    if (spSelectionValid(spCurrentSelectedCandidate))
    {
        spCurrentSelected = spCurrentSelectedCandidate;
    }

    feScreenDisappear(NULL, NULL);
    spPickerStarted = FALSE;
}

void spBackPicking(char *name, featom *atom)
{
    spDescriptionDefaultsAndFreeText();

    if (scenarioTexture != TR_InvalidInternalHandle)
    {                                                       //delete the preview image, if there was one
        trRGBTextureDelete(scenarioTexture);
        scenarioTexture = TR_InvalidInternalHandle;
        if (spTextureData != NULL)
        {
            memFree(spTextureData);
            spTextureData = NULL;
        }
    }

    feScreenDisappear(NULL, NULL);
    spPickerStarted = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : spFindMap
    Description : Sets the current selected scenario to the mapname specified
    Inputs      :
    Outputs     : DonePicking stores chosen file name in parameter to spScenarioPick
    Return      : void
----------------------------------------------------------------------------*/
void spFindMap(char *MapName)
{
    sdword index;

    for (index = 0; index < spNumberScenarios; index++)
    {
        if (strcasecmp(spScenarios[index].title,MapName)==0)
        {
            spCurrentSelected = index;
            break;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : catSpScenarioPickForTest
    Description : HWI-TEST (skirmish-start), port diagnostic, not in the exe: the scenario picker's choice of a map with
                  no input, for /catSkirmishTest (LevelLoad.c). The map's scenario is found by its directory name; a
                  map that multiPlayerMissions.script lists commented out (LiveFireExercise2) is added first the way
                  spTitleListLoad adds a line (its .level title, maxplayers the name's number, minplayers 2). Then
                  what a click on its line and OK do: spNewItem reads the description's game type
                  (MultiPlayer\<name>\description.txt, StaticMGInfoScriptTable) and spDonePicking sets it
                  (mgSetGameTypeByStruct, the screen's buttons updated) and selects the map if spSelectionValid lets it
    Inputs      : name - the map's directory name (case does not matter)
    Outputs     : spCurrentSelected, tpGameCreated's game type
    Return      : the scenario index, or -1 if the map has no .level title
----------------------------------------------------------------------------*/
sdword catSpScenarioPickForTest(char *name)
{
    char descriptionFile[PATH_MAX];
    char *pString;
    char *title;
    sdword index, numplayers = 0;
    GameType *gameType = NULL;

    for (index = 0; index < spNumberScenarios; index++)
    {
        if (strcasecmp(spScenarios[index].fileSpec, name) == 0)
        {
            break;
        }
    }
    if (index >= spNumberScenarios)
    {                                                       // not listed: add it as spTitleListLoad adds a line
        for (pString = name; *pString != 0; pString++)
        {
            if (strchr("0123456789", *pString) != NULL)
            {
                sscanf(pString, "%d", &numplayers);
                break;
            }
        }
#ifdef _WIN32
        title = spTitleFind("MultiPlayer\\", name);
#else
        title = spTitleFind("MultiPlayer/", name);
#endif
        if ((title == NULL) || (numplayers == 0))
        {
            return -1;
        }
        if (spNumberScenarios >= spScenarioListLength)
        {
            spScenarioListLength += SP_ScenarioListGrowth;
            spScenarios = memRealloc(spScenarios, spScenarioListLength * sizeof(spscenario), "spScenarios", NonVolatile);
        }
        index = spNumberScenarios;
        spScenarios[index].fileSpec = memStringDupe(name);
        spScenarios[index].bitmapfileSpec = memStringDupe(name);
        spScenarios[index].title = title;
        spScenarios[index].maxplayers = numplayers;
        spScenarios[index].minplayers = 2;
        spNumberScenarios++;
        dbgMessagef("skirmishtest: '%s' is not in the scenario list (multiPlayerMissions.script): added as spTitleListLoad adds a line", name);
    }

    // spNewItem: the description's game type
    sprintf(descriptionFile, "%s%s\\description.txt", "MultiPlayer\\", spScenarios[index].bitmapfileSpec);
    if (fileExists(descriptionFile, 0))
    {
        gameType = memAlloc(sizeof(GameType), "DescGameType", 0);
        memset(gameType, 0xff, sizeof(GameType));
        gameType->flag = gameType->flagNeeded = 0;
        scriptSetStruct(NULL, descriptionFile, StaticMGInfoScriptTable, (ubyte *)gameType);
    }
    // spDonePicking: the game type set, the buttons of the screen below the picker (the skirmish screen) updated
    if (gameType != NULL)
    {
        mgSetGameTypeByStruct(gameType);
        feAllCallOnCreate(feStack[feStackIndex].screen);
        mgGameTypesOtherButtonPressed();
        memFree(gameType);
    }
    if (spSelectionValid(index))
    {
        spCurrentSelected = index;
    }
    return index;
}




/*-----------------------------------------------------------------------------
    Name        : spGetScenarioDetails
    Description : retrieve the preview image and text description for a given scenario
    Inputs      : scenarioFile - name of scenario
    Outputs     : bitmapFile - path of preview.bmp file if found
                  textFile - path of description.txt file if found
    Return      : void
----------------------------------------------------------------------------*/
void spGetScenarioDetails(char* bitmapFile, char *textFile, char *scenarioFile)
{
    sdword i;
    bool32 foundBitmap = FALSE, foundText = FALSE;

    for (i = 2; i<=MAX_MULTIPLAYER_PLAYERS; i++)
    {
        if (!foundText)
        {
#ifdef _WIN32
            sprintf(textFile, "%s%s%c\\description.txt", scenarioRootPath, scenarioFile, '0' + i);
#else
            sprintf(textFile, "%s%s%c/description.txt", scenarioRootPath, scenarioFile, '0' + i);
#endif
            if (fileExists(textFile, 0))
            {                                               //see if there is a description text file in this directory
                foundText = TRUE;
            }
        }
    }
    if (!foundBitmap)
    {
        *bitmapFile = 0;
    }
    if (!foundText)
    {
        *textFile = 0;
    }
}

/*-----------------------------------------------------------------------------
    Name        : spMissionDescriptionSet
    Description : Script-parse callback for setting description text
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
static void spMissionDescriptionSet(char *directory,char *field,void *dataToFillIn)
{
    if (spDescription == NULL)
    {
        spDescription = memStringDupe(field);
    }
    else
    {
        spDescription = memRealloc(spDescription, strlen(spDescription) + strlen(field) + 3, "DescriptionString", 0);
        strcat(spDescription, "\\n");                       //auto-newline
        strcat(spDescription, field);
    }
}

/*-----------------------------------------------------------------------------
    Name        : spDescriptionFontSet
    Description : Script-parsing callback to set the font for the description text.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
static void spDescriptionFontSet(char *directory,char *field,void *dataToFillIn)
{
    spDescriptionFont = frFontRegister(field);
}

/*-----------------------------------------------------------------------------
    Name        : spNewItem
    Description : Called when a new item in the color picker list is selected.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: spNewItem 0x00504a20: Cataclysm shows no preview bitmap (the exe never fills its bitmap path: the area is filled
//      with FEC_Background) and puts the description's chopped lines into the CS_ScenarioBitmap list window
//      (spScenarioBitmap); the description is MultiPlayer\<name>\description.txt (HW1: searched over the map's sizes)
void spNewItem(void)
{
    char descriptionFile[PATH_MAX];
    char *chopBuffer;
    sdword scen, index;
    rectangle textureRect;
    featom *atom;

    scen = spScenarioListWindow->CurLineSelected->position;
    atom = feAtomFindInScreen(feStack[feStackIndex].screen, "CS_ScenarioBitmap");
    dbgAssertOrIgnore(atom != NULL);

    if (scenarioTexture != TR_InvalidInternalHandle)
    {                                                       //delete the old texture before creating a new one
        trRGBTextureDelete(scenarioTexture);
        scenarioTexture = TR_InvalidInternalHandle;
        if (spTextureData != NULL)
        {
            memFree(spTextureData);
            spTextureData = NULL;
        }
    }

    //free the previous text and set defaults
    spDescriptionDefaultsAndFreeText();

    sprintf(descriptionFile, "%s%s\\description.txt", "MultiPlayer\\", spScenarios[scen].bitmapfileSpec);
    if (!fileExists(descriptionFile, 0))
    {
        descriptionFile[0] = 0;
    }

    //no preview bitmap: fill its area
    textureRect.x0 = atom->x + SCP_DescriptionInset;
    textureRect.y0 = atom->y + SCP_DescriptionInset;
    textureRect.x1 = atom->x + atom->width - SCP_DescriptionInset;
    textureRect.y1 = atom->y + atom->height - SCP_DescriptionInset;
    primRectSolid2(&textureRect, FEC_Background);

    if (strlen(descriptionFile) > 0)
    {                                                       //if there is a description file
        // load in the proper description based on the language (the others fall back to English)
        if (strCurLanguage==languageFrench)
        {
            scriptSet(NULL, descriptionFile, spDescriptionTweaksFrench);
        }
        else if (strCurLanguage==languageGerman)
        {
            scriptSet(NULL, descriptionFile, spDescriptionTweaksGerman);
        }
        else if (strCurLanguage==languageSpanish)
        {
            scriptSet(NULL, descriptionFile, spDescriptionTweaksSpanish);
        }
        else if (strCurLanguage==languageItalian)
        {
            scriptSet(NULL, descriptionFile, spDescriptionTweaksItalian);
        }
        if (spDescription == NULL)
        {
            scriptSet(NULL, descriptionFile, spDescriptionTweaks);
        }

        if (spDescription != NULL)
        {                                                   //if there was real text in that file
            if (spDescriptionFont == FONT_InvalidFontHandle)
            {                                               //make sure we have a font
                spDescriptionFont = frFontRegister("default.hff");
            }
            spDescriptionLines = memAlloc(sizeof(char *) * (textureRect.y1 - textureRect.y0), "DescLines", NonVolatile);//allocate the chopping pointers
            chopBuffer = memAlloc(strlen(spDescription) + textureRect.y1 - textureRect.y0, "DescChopped", NonVolatile);//allocate chop buffer
            spNDescriptionLines = subStringsChop(&textureRect, spDescriptionFont, strlen(spDescription), spDescription, chopBuffer, spDescriptionLines);
            memFree(spDescription);                         //free unchopped description
            spDescription = chopBuffer;                     //keep pointer to chopped description
        }
        gameTypeFromDescription = memAlloc(sizeof(GameType), "DescGameType", 0);
        memset(gameTypeFromDescription, 0xff, sizeof(GameType));       //flag all members as unused
        gameTypeFromDescription->flag = gameTypeFromDescription->flagNeeded = 0;    //flag none of the flags needed

        scriptSetStruct(NULL, descriptionFile, StaticMGInfoScriptTable, (ubyte *)gameTypeFromDescription);
    }

    if (spDescriptionListWindow != NULL)
    {
        uicListCleanUp(spDescriptionListWindow);
    }
    if (strlen(descriptionFile) == 0)
    {
        return;
    }
    if ((spDescriptionListWindow != NULL) && (spDescription != NULL))
    {
        for (index = 0; index < spNDescriptionLines; index++)
        {
            uicListAddItem(spDescriptionListWindow, (ubyte *)spDescriptionLines[index], 0, UICLW_AddToTail);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : spScenarioBitmap
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: a description line of the CS_ScenarioBitmap list (0x00504f70): 6 pixels in, the description's font, shadow and
//      colour
static void spDescriptionItemDraw(rectangle *rect, listitemhandle data)
{
    fonthandle oldFont = fontMakeCurrent(spDescriptionFont);

    if (spDescriptionShadow)
    {
        fontShadowSet(FS_SE, colBlack);
    }
    fontPrintf(rect->x0 + SCP_DescriptionInset, rect->y0, spDescriptionColor, "%s", (char *)data->data);
    fontShadowSet(FS_NONE, colBlack);
    fontMakeCurrent(oldFont);
}

// CAT: spScenarioBitmap 0x00504fe0: the CS_ScenarioBitmap atom is a list window of the description's lines (HW1: a
//      user region drawing the preview bitmap and the text)
void spScenarioBitmap(char *name, featom *atom)
{
    sdword index;

    if (FEFIRSTCALL(atom))
    {
        spDescriptionListWindow = (listwindowhandle)atom->pData;
        uicListWindowInit(spDescriptionListWindow, NULL, NULL, 0, spDescriptionItemDraw,
                          fontHeight(" ") / 3 + fontHeight(" "), UICLW_CanSelect);
        if (spDescription != NULL)
        {
            for (index = 0; index < spNDescriptionLines; index++)
            {
                uicListAddItem(spDescriptionListWindow, (ubyte *)spDescriptionLines[index], 0, UICLW_AddToTail);
            }
        }
        return;
    }
    if (FELASTCALL(atom))
    {
        spDescriptionListWindow = NULL;
    }
}
