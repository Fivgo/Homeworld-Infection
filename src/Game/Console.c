// =============================================================================
//  Console.c
//  - Cataclysm's developer console: a text line that sets the variables of a
//    command table and runs its commands, its output lines, its history and
//    its binary config file (cataclysm.exe 0x0045a260-0x004601d0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R6 startup-keys). Each body follows the r6 decompile of its exe function (address on each) and,
//      where the decompile lost the arguments, the disassembly. The retail exe starts the console in every gameStart
//      (0x005a9290) and shuts it down with the game systems, draws it through the "console" screen of
//      FEMan\console.fib (CON_TextDraw) and gives it every key down (WindowProc 0x00561f10, after keyPressDown), but
//      nothing in it opens the console: cat_consoleOpen's only caller is cat_consoleKeyPress's tilde key, which
//      returns at once while the console is closed. The port keeps that (the self-test opens it through the API).

#include "Console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "Debug.h"
#include "File.h"
#include "font.h"
#include "FontReg.h"
#include "Key.h"
#include "mainrgn.h"
#include "Memory.h"
#include "OracleDump.h"
#include "ResearchAPI.h"
#include "Select.h"
#include "Sensors.h"
#include "ShipSelect.h"
#include "SinglePlayer.h"
#include "Shader.h"
#include "render.h"
#include "Universe.h"
#include "utility.h"

#define CAT_CON_PROMPT              "]"                 // 0x0075501c
#define CAT_CON_CURSOR              "|"                 // 0x00755044
#define CAT_CON_HEIGHT              300                 // 0x00754f98
#define CAT_CON_SPEED               100                 // the scroll per frame (cat_consoleOpen, the tilde key)
#define CAT_CON_BLINK               10                  // 0x00754f9c: frames per cursor blink
#define CAT_CON_CONFIG_DEFAULT      "dog.cfg"           // 0x0072a730
#define CAT_CON_COLOUR_SHADOW       0xff000000
#define CAT_CON_COLOUR_TEXT         0xffffffff

/*=============================================================================
    Data
=============================================================================*/
bool8 catConsoleOpenFlag = FALSE;               // 0x00a2dcd4
static bool8 catConTabbing = FALSE;             // 0x00a2dcd5: TAB has completed the input line
static sdword catConScrollSpeed = 0;            // 0x00a2dcd8
static sdword catConScrollPos = 0;              // 0x00a2dcdc
static char **catConOutput = NULL;              // 0x00a2dce0: the output lines, the newest last
static char **catConInput = NULL;               // 0x00a2dce4: the history of input lines
static char *catConTabPrefix = NULL;            // 0x00a2dce8: the line TAB completes
static sdword catConCursor = 1;                 // 0x00a2dcec: the input position (after the prompt)
static sdword catConInputIndex = 0;             // 0x00a2dcf0
static sdword catConTabIndex = 0;               // 0x00a2dcf4: the next completion to give
static fibfileheader *catConScreens = NULL;     // 0x00a2dcf8: FEMan\console.fib
static regionhandle catConScreen = NULL;        // 0x00a2dcfc: the "console" screen while it is shown
static fonthandle catConFont = 0;               // 0x00a2dd00: Arial_12.hff
static char *catConConfigPath = NULL;           // 0x00a2de0c: the config file's directory
static sdword catConHeight = CAT_CON_HEIGHT;    // 0x00754f98
static sdword catConBlink = CAT_CON_BLINK;      // 0x00754f9c
static bool8 catConCursorOn = TRUE;             // 0x00754fa0

// the command table's variables (0x0087813c .. 0x00878171, 0x00a2de04 / 0x00a2de08, 0x00a75151 / 0x00a75152). The
//      exe's other code reads only morphStartScale (cat_mimicBecomeShipType / Target), rammingtime / rammingangle
//      (RammingFrigateInit / HouseKeep), constyle (the build-in fade, render.c), cbv / lighting (Shader.c), BTG
//      (rndBackgroundRender) and TRAILS (rndMainViewRenderFunction); the others are the console's alone.
real32 catConsoleMorphStartScale = 1.2f;        // 0x0087813c
static real32 catConMimicTime = 300.0f;         // 0x00878140
sdword catConsoleRammingTime = 300;             // 0x00878144
real32 catConsoleRammingAngle = 0.99f;          // 0x00878148
static real32 catConRammingMinDistance = 500.0f;    // 0x0087814c
static real32 catConRammingMaxDistance = 5000.0f;   // 0x00878150
static real32 catConRammingAbortAngle = 0.7f;   // 0x00878154
static real32 catConLeechRange = 100.0f;        // 0x00878158
static sdword catConLeechLeechRate = 1;         // 0x0087815c
static sdword catConLeechVentRate = 2;          // 0x00878160
static sdword catConLeechResourceRate = 10;     // 0x00878164
static bool8 catConDrawBoxes = FALSE;           // 0x00a75151 "boxes"
static bool8 catConDrawSpheres = FALSE;         // 0x00a75152 "spheres"
static bool8 catConPlanets = TRUE;              // 0x0087816f "PLANETS"
static bool8 catConFog = TRUE;                  // 0x00878170 "FOG"
static char *catConLoadConfigName = NULL;       // 0x00a2de04
static char *catConSaveConfigName = NULL;       // 0x00a2de08
bool8 catMotionCues = TRUE;                     // 0x008b8844 (kasfSetMotionCues, the camera's motion cues: CameraCommand)

static void catConTestFrame(void);                 // HWI-TEST: the self-test's frame hook (end of the file)
static void cat_consoleLoadConfig(sdword type, void *data, void *value, char **reply);
static void cat_consoleSaveConfig(sdword type, void *data, void *value, char **reply);
static void cat_spConsoleAddResources(sdword type, void *data, void *value, char **reply);
static void cat_spConsoleWarpMission(sdword type, void *data, void *value, char **reply);
static void cat_consoleAllTech(sdword type, void *data, void *value, char **reply);
static void cat_consoleUpgradeShips(sdword type, void *data, void *value, char **reply);
static void cat_consoleExperienceShips(sdword type, void *data, void *value, char **reply);

CatConsoleCommand catConsoleCommandTable[] =       // 0x008783c8
{
    { 8,  0, &catConLoadConfigName,         "loadConfig",           cat_consoleLoadConfig },
    { 8,  1, &catConSaveConfigName,         "saveConfig",           cat_consoleSaveConfig },
    { 0,  2, &mrDrawTactical,               "tactical",             NULL },
    { 7,  4, &catConsoleMorphStartScale,    "morphStartScale",      NULL },
    { 7,  8, &catConMimicTime,              "mimictime",            NULL },
    { 5,  9, &catConsoleRammingTime,        "rammingtime",          NULL },
    { 7, 10, &catConsoleRammingAngle,       "rammingangle",         NULL },
    { 7, 11, &catConRammingMinDistance,     "rammingmindistance",   NULL },
    { 7, 12, &catConRammingMaxDistance,     "rammingmaxdistance",   NULL },
    { 7, 13, &catConRammingAbortAngle,      "rammingabortangle",    NULL },
    { 7, 14, &catConLeechRange,             "leechrange",           NULL },
    { 5, 15, &catConLeechLeechRate,         "leechleechrate",       NULL },
    { 5, 16, &catConLeechVentRate,          "leechventrate",        NULL },
    { 5, 17, &catConLeechResourceRate,      "leechresourcerate",    NULL },
    { 5, 18, &catRndConsoleStyle,           "constyle",             NULL },
    { 5, 19, NULL,                          "addResources",         cat_spConsoleAddResources },
    { 5, 21, NULL,                          "warpTo",               cat_spConsoleWarpMission },
    { 0, 22, &catConDrawBoxes,              "boxes",                NULL },
    { 0, 23, &catConDrawSpheres,            "spheres",              NULL },
    { 0, 29, NULL,                          "alltech",              cat_consoleAllTech },
    { 0, 30, &catShColourByVertex,          "cbv",                  NULL },
    { 0, 31, &catShLighting,                "lighting",             NULL },
    { 0, 33, &catMotionCues,                "cameracues",           NULL },
    { 0, 36, &catRndDrawBTG,                "BTG",                  NULL },
    { 0, 37, &catConPlanets,                "PLANETS",              NULL },
    { 0, 38, &catConFog,                    "FOG",                  NULL },
    { 0, 39, &catRndDrawTrails,             "TRAILS",               NULL },
    { 5, 41, NULL,                          "upgradeships",         cat_consoleUpgradeShips },
    { 5, 45, NULL,                          "experienceships",      cat_consoleExperienceShips },
    { -1, -1, NULL, NULL, NULL }
};

static fedrawcallback catConDrawCallbacks[] =       // 0x00754fa8
{
    { cat_conTextDraw, "CON_TextDraw" },
    { NULL, NULL }
};

/*=============================================================================
    Start-up, shut-down, open, close
=============================================================================*/
static char *catConLineAlloc(void)
{
    char *line = memAlloc(CAT_CON_LINE_LENGTH, "conline", NonVolatile);

    memset(line, 0, CAT_CON_LINE_LENGTH);
    return line;
}

static void catConLinesFree(void)
{
    sdword index;

    if (catConOutput != NULL)
    {
        for (index = 0; index < CAT_CON_OUTPUT_LINES; index++)
        {
            memFree(catConOutput[index]);
        }
        memFree(catConOutput);
        catConOutput = NULL;
    }
    if (catConInput != NULL)
    {
        for (index = 0; index < CAT_CON_INPUT_LINES; index++)
        {
            memFree(catConInput[index]);
        }
        memFree(catConInput);
        catConInput = NULL;
    }
    if (catConTabPrefix != NULL)
    {
        memFree(catConTabPrefix);
        catConTabPrefix = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_consoleStartup                                0x0045a260
    Description : (gameStart) the line buffers, the font, the console screens and the config file's directory.
----------------------------------------------------------------------------*/
void cat_consoleStartup(void)
{
    sdword index;

    catConScrollPos = 0;
    catConScrollSpeed = 0;
    catConHeight = CAT_CON_HEIGHT;
    if (catConConfigPath != NULL)
    {                                                       // CAT-FIX: the exe allocates again in every gameStart and
        catConLinesFree();                                  //      frees only at the shut-down (it would leak each
        cat_consoleConfigShutdown();                        //      game's lines)
    }
    catConOutput = memAlloc(CAT_CON_OUTPUT_LINES * sizeof(char *), "conoutput", NonVolatile);
    catConInput = memAlloc(CAT_CON_INPUT_LINES * sizeof(char *), "coninput", NonVolatile);
    for (index = 0; index < CAT_CON_OUTPUT_LINES; index++)
    {
        catConOutput[index] = catConLineAlloc();
    }
    for (index = 0; index < CAT_CON_INPUT_LINES; index++)
    {
        catConInput[index] = catConLineAlloc();
        strcpy(catConInput[index], CAT_CON_PROMPT);
    }
    catConTabPrefix = catConLineAlloc();
    strcpy(catConTabPrefix, CAT_CON_PROMPT);
    catConCursor = 1;
    catConInputIndex = 0;
    catConFont = frFontRegister("Arial_12.hff");
    if (catConScreens == NULL)
    {
        feDrawCallbackAddMultiple(catConDrawCallbacks);
        catConScreens = feScreensLoad("FEMan\\console.fib");
    }
    catConsoleOpenFlag = FALSE;
    catConTabbing = FALSE;
    catConScrollSpeed = 0;
    catConScrollPos = 0;
    catConHeight = CAT_CON_HEIGHT;
    catConBlink = CAT_CON_BLINK;
    catConCursorOn = TRUE;
    catConInputIndex = 0;
    catConTabIndex = 0;
    cat_consoleConfigStartup();
}

/*-----------------------------------------------------------------------------
    Name        : cat_consoleShutdown                               0x0045a3d0
    Description : (utyGameSystemsShutdown) the buffers freed, the screen gone.
----------------------------------------------------------------------------*/
void cat_consoleShutdown(void)
{
    catConLinesFree();
    cat_consoleConfigShutdown();
    if (catConScreen != NULL)
    {
        feScreenDeleteFlags(catConScreen, 1);
        catConScreen = NULL;
        catConsoleOpenFlag = FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_consoleOpen                                   0x0045a460
    Description : The console screen above the top edge, scrolling down; the interface locked out (and a
                  single-player game paused).
----------------------------------------------------------------------------*/
void cat_consoleOpen(void)
{
    spLockout(SPLOCKOUT_MOUSE);
    if (singlePlayerGame)
    {
        universePause = TRUE;                               // 0x00a6c5b4
    }
    if (catConScreen == NULL)
    {
        catConScreen = feScreenStart(ghMainRegion, "console");
        regRegionScroll(catConScreen, 0, -catConHeight);
    }
    catConsoleOpenFlag = TRUE;
    catConScrollSpeed = CAT_CON_SPEED;
}

/*-----------------------------------------------------------------------------
    Name        : cat_consoleClose                                  0x0045a4d0
    Description : At once: the screen gone, the interface back (warpTo; the tilde key scrolls it out instead).
----------------------------------------------------------------------------*/
void cat_consoleClose(void)
{
    catConsoleOpenFlag = FALSE;
    catConScrollPos = 0;
    catConScrollSpeed = 0;
    if (catConScreen != NULL)
    {
        feScreenDeleteFlags(catConScreen, 1);
        catConScreen = NULL;
        spUnlockout();
        if (singlePlayerGame)
        {
            universePause = FALSE;
        }
    }
}

/*=============================================================================
    The input line and the output
=============================================================================*/
void cat_consoleAddChar(ubyte c)                    // 0x0045a520
{
    catConInput[catConInputIndex][catConCursor] = (char)c;
    catConCursor++;
    catConInput[catConInputIndex][catConCursor] = '\0';
}

void cat_consoleOutputScroll(void)                  // 0x0045a560: the oldest line becomes the newest
{
    char *first = catConOutput[0];

    memmove(&catConOutput[0], &catConOutput[1], (CAT_CON_OUTPUT_LINES - 1) * sizeof(char *));
    catConOutput[CAT_CON_OUTPUT_LINES - 1] = first;
}

/*-----------------------------------------------------------------------------
    Name        : cat_consoleExecuteCommand                         0x0045a590
    Description : An entered line (after the prompt): "PROF" only scrolls; a name alone shows the variable, a name and
                  an argument sets it or runs the command; the reply goes to a new output line ("Not a command" when
                  nothing matched; a command that writes no reply leaves that line's old text).
----------------------------------------------------------------------------*/
void cat_consoleExecuteCommand(char *line)
{
    char *copy = memStringDupe(line);
    char *string = copy + 1;
    char *full = memStringDupe(string);
    char *argument = NULL, *reply;
    sdword index, result;

    for (index = 0; string[index] != '\0'; index++)
    {
        if (string[index] == ' ')
        {
            string[index] = '\0';
            argument = string + index + 1;
            break;
        }
    }
    if (strcasecmp(string, "PROF") == 0)
    {
        cat_consoleOutputScroll();
    }
    else
    {
        cat_consoleOutputScroll();
        reply = catConOutput[CAT_CON_OUTPUT_LINES - 1];
        if (argument == NULL)
        {
            result = cat_consoleShowVariable(string, &reply, full);
        }
        else
        {
            result = cat_consoleRunCommand(string, argument, &reply, full);
        }
        if (result == 0)
        {
            strcpy(reply, "Not a command");
        }
    }
    memFree(full);
    memFree(copy);
}

/*-----------------------------------------------------------------------------
    Name        : cat_consoleKeyPress                               0x0045a700
    Description : A key down while the console is in: letters, digits (shifted: their symbols), ';' '-' '.' '\',
                  space, backspace, Escape (the line cleared), up / down (the history), TAB (the next command whose
                  name starts so), tilde (scroll out), Enter (or a full line: echo, run, next history line).
                  The exe gets every key down here (WindowProc 0x00561f10); vk is the virtual-key code.
----------------------------------------------------------------------------*/
void cat_consoleKeyPress(udword vk)
{
    static const char shiftedDigits[10] = { ')', '!', '@', '#', '$', '%', '^', '&', '*', '(' };
    bool32 shift;
    char *completion, *source, *line;
    bool32 store = FALSE;
    char c = 0;

    if (!catConsoleOpenFlag || (catConScrollPos < 1) || (catConScrollSpeed < 0))
    {
        return;
    }
    shift = catKeyVkIsHit(0x10);
    if (catConTabbing && (vk != 0x0d) && (vk != 0x20) && (vk != 0x08) && (vk != 0x09))
    {                                                       // any other key: back to the line TAB started from
        strcpy(catConInput[catConInputIndex], catConTabPrefix);
        catConCursor = (sdword)strlen(catConInput[catConInputIndex]);
        catConTabbing = FALSE;
        catConTabIndex = 0;
    }
    line = catConInput[catConInputIndex];
    if ((vk >= '0') && (vk <= '9'))
    {
        c = shift ? shiftedDigits[vk - '0'] : (char)vk;
        store = TRUE;
    }
    else if ((vk >= 'A') && (vk <= 'Z'))
    {
        c = shift ? (char)vk : (char)(vk + ('a' - 'A'));
        store = TRUE;
    }
    else
    {
        switch (vk)
        {
            case 0x08:                                      // backspace
                if (catConCursor > 1)
                {
                    catConCursor--;
                    line[catConCursor] = '\0';
                }
                catConTabbing = FALSE;
                catConTabIndex = 0;
                break;
            case 0x09:                                      // TAB
                if (strlen(line) > 1)
                {
                    completion = NULL;
                    source = (!catConTabbing && (catConTabIndex < 1)) ? line : catConTabPrefix;
                    cat_consoleTabComplete(source + 1, &completion, &catConTabIndex);
                    if (completion != NULL)
                    {
                        catConTabbing = TRUE;
                        strcpy(catConTabPrefix, line);
                        strcpy(line, CAT_CON_PROMPT);
                        strncat(line, completion, CAT_CON_LINE_LENGTH - 2);     // HWI: bounded (the exe's strcat)
                        catConCursor = (sdword)strlen(line);
                        memFree(completion);
                    }
                }
                break;
            case 0x1b:                                      // Escape: the line cleared
                strcpy(line, CAT_CON_PROMPT);
                catConCursor = 1;
                catConTabbing = FALSE;
                catConTabIndex = 0;
                break;
            case 0x20:
                cat_consoleAddChar(' ');
                catConTabbing = FALSE;
                catConTabIndex = 0;
                break;
            case 0x26:                                      // up: the older line
                catConInputIndex--;
                if (catConInputIndex < 0)
                {
                    catConInputIndex = CAT_CON_INPUT_LINES - 1;
                }
                catConCursor = (sdword)strlen(catConInput[catConInputIndex]);
                catConTabbing = FALSE;
                catConTabIndex = 0;
                if (catConCursor > 1)
                {
                    break;
                }
                // (an empty line: on to the next one, as the exe falls into the down key)
            case 0x28:                                      // down: the newer line, if it has text
                if (strlen((catConInputIndex < CAT_CON_INPUT_LINES - 1) ? catConInput[catConInputIndex + 1] : catConInput[0]) < 2)
                {
                    break;
                }
                catConInputIndex++;
                if (catConInputIndex > CAT_CON_INPUT_LINES - 1)
                {
                    catConInputIndex = 0;
                }
                catConCursor = (sdword)strlen(catConInput[catConInputIndex]);
                catConTabbing = FALSE;
                catConTabIndex = 0;
                break;
            case 0xba:
                c = shift ? ':' : ';';
                store = TRUE;
                break;
            case 0xbd:
                c = shift ? '_' : '-';
                store = TRUE;
                break;
            case 0xbe:
                c = shift ? '>' : '.';
                store = TRUE;
                break;
            case 0xc0:                                      // tilde: scroll out (the open branch is dead: see above)
                if (!catConsoleOpenFlag)
                {
                    cat_consoleOpen();
                }
                else
                {
                    catConScrollSpeed = -CAT_CON_SPEED;
                }
                catConTabbing = FALSE;
                catConTabIndex = 0;
                break;
            case 0xdc:
                c = '\\';
                store = TRUE;
                break;
            default:
                break;
        }
    }
    line = catConInput[catConInputIndex];
    if (store)
    {
        line[catConCursor] = c;
        catConCursor++;
        line[catConCursor] = '\0';
    }
    if ((vk == 0x0d) || (catConCursor > 0x4b))
    {                                                       // Enter or a full line: echoed, run, next history line
        cat_consoleOutputScroll();
        strcpy(catConOutput[CAT_CON_OUTPUT_LINES - 1], line);
        catConCursor = 1;
        catConTabbing = FALSE;
        catConTabIndex = 0;
        if (strlen(line) > 1)
        {
            cat_consoleExecuteCommand(line);
            catConInputIndex++;
            if (catConInputIndex > CAT_CON_INPUT_LINES - 1)
            {
                catConInputIndex = 0;
            }
            strcpy(catConInput[catConInputIndex], CAT_CON_PROMPT);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_conTextDraw                                   0x0045b1c0
    Description : CON_TextDraw: the scroll (gone when it is back above the edge), the output lines up from the input
                  line (black shadow, white text) and the input line with a blinking '|'.
----------------------------------------------------------------------------*/
void cat_conTextDraw(featom *atom, regionhandle region)
{
    sdword bottom = region->rect.y1;
    fonthandle font;
    sdword index, y, height;
    char line[0x50];
    char *text;

    (void)atom;
    catConTestFrame();                                      // HWI-TEST: the console self-test's frames (no effect otherwise)
    catConScrollPos += catConScrollSpeed;
    if (catConScrollPos < 0)
    {
        catConScrollPos = 0;
        catConScrollSpeed = 0;
        if (catConScreen != NULL)
        {
            feScreenDeleteFlags(catConScreen, 1);
            catConScreen = NULL;
            spUnlockout();
            if (singlePlayerGame)
            {
                universePause = FALSE;
            }
            catConsoleOpenFlag = FALSE;
        }
    }
    else if (catConScrollPos > catConHeight)
    {
        catConScrollPos = catConHeight;
        catConScrollSpeed = 0;
    }
    if ((catConScrollSpeed != 0) && (catConScreen != NULL))
    {
        regRegionScroll(catConScreen, 0, catConScrollSpeed);
    }
    font = fontMakeCurrent(catConFont);
    if (catConScrollPos > 0)
    {
        for (index = 0; index < CAT_CON_OUTPUT_LINES; index++)
        {                                                   // the newest output line just above the input line
            height = fontHeight(" ");
            y = bottom - (height + 2) * (index + 2) - 15;
            if (y >= 0)
            {
                fontPrintf(16, y + 1, CAT_CON_COLOUR_SHADOW, "%s", catConOutput[CAT_CON_OUTPUT_LINES - 1 - index]);
                fontPrintf(15, y, CAT_CON_COLOUR_TEXT, "%s", catConOutput[CAT_CON_OUTPUT_LINES - 1 - index]);
            }
        }
        if ((catConBlink < 1) || (--catConBlink < 1))
        {
            catConCursorOn = !catConCursorOn;
            catConBlink = CAT_CON_BLINK;
        }
        y = bottom - fontHeight(" ");
        if (!catConCursorOn)
        {
            text = catConInput[catConInputIndex];
        }
        else
        {
            snprintf(line, sizeof(line), "%s%s", catConInput[catConInputIndex], CAT_CON_CURSOR);
            text = line;
        }
        if (y - 17 >= 0)
        {
            fontPrintf(16, y - 16, CAT_CON_COLOUR_SHADOW, "%s", text);
            fontPrintf(15, y - 17, CAT_CON_COLOUR_TEXT, "%s", text);
        }
    }
    fontMakeCurrent(font);
}

/*-----------------------------------------------------------------------------
    Name        : cat_consoleTabComplete                            0x0045b420
    Description : The index-th command whose name starts with the prefix (case aside); past the last one it starts
                  again from the first.
    Outputs     : completion - a copy of the name (memFree it); index - the next one
----------------------------------------------------------------------------*/
void cat_consoleTabComplete(char *prefix, char **completion, sdword *index)
{
    sdword row, count;

    for (;;)
    {
        count = 0;
        for (row = 0; catConsoleCommandTable[row].type != -1; row++)
        {
            if ((catConsoleCommandTable[row].name[0] != '\0') &&
                (strncasecmp(catConsoleCommandTable[row].name, prefix, strlen(prefix)) == 0))
            {
                if (count == *index)
                {
                    *completion = memStringDupe(catConsoleCommandTable[row].name);
                    (*index)++;
                    return;
                }
                count++;
            }
        }
        if (*index == 0)
        {
            return;
        }
        *index = 0;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_consoleRunCommand                             0x0045b520
    Description : A name with an argument: the row's callback with the parsed value, or the value stored ("true" /
                  "on" / "false" / "off" or a number for a bool). A name the table lacks goes to the KAS functions.
    Return      : TRUE if a row matched
----------------------------------------------------------------------------*/
sdword cat_consoleRunCommand(char *name, char *argument, char **reply, char *line)
{
    CatConsoleCommand *row;
    sdword index, number;
    real32 real;
    ubyte byte;
    uword word;
    char *string;

    for (index = 0; catConsoleCommandTable[index].type != -1; index++)
    {
        if (strcasecmp(name, catConsoleCommandTable[index].name) == 0)
        {
            break;
        }
    }
    if (catConsoleCommandTable[index].type == -1)
    {
        return cat_consoleKasCommand(reply, line);
    }
    row = &catConsoleCommandTable[index];
    if (argument == NULL)
    {
        if ((row->type == 8) && (row->callback != NULL))
        {                                                   // a string command alone: on the default file
            string = memStringDupe(CAT_CON_CONFIG_DEFAULT);
            row->callback(row->type, row->data, &string, reply);
        }
        return TRUE;
    }
    switch (row->type)
    {
        case 0:
            if ((strcasecmp(argument, "true") == 0) || (strcasecmp(argument, "on") == 0))
            {
                byte = 1;
            }
            else if ((strcasecmp(argument, "false") == 0) || (strcasecmp(argument, "off") == 0))
            {
                byte = 0;
            }
            else
            {
                byte = (ubyte)atoi(argument);
            }
            if (row->callback != NULL)
            {
                number = byte;                              // (the exe passes the byte in a dword slot)
                row->callback(row->type, row->data, &number, reply);
                return TRUE;
            }
            *(ubyte *)row->data = byte;
            return TRUE;
        case 1:
        case 2:
            byte = (ubyte)atoi(argument);
            if (row->callback != NULL)
            {
                number = byte;
                row->callback(row->type, row->data, &number, reply);
                return TRUE;
            }
            *(ubyte *)row->data = byte;
            return TRUE;
        case 3:
        case 4:
            word = (uword)atoi(argument);
            if (row->callback != NULL)
            {
                number = word;
                row->callback(row->type, row->data, &number, reply);
                return TRUE;
            }
            *(uword *)row->data = word;
            return TRUE;
        case 5:
        case 6:
            number = atoi(argument);
            if (row->callback != NULL)
            {
                row->callback(row->type, row->data, &number, reply);
                return TRUE;
            }
            *(sdword *)row->data = number;
            return TRUE;
        case 7:
            real = (real32)atof(argument);
            if (row->callback != NULL)
            {
                row->callback(row->type, row->data, &real, reply);
                return TRUE;
            }
            *(real32 *)row->data = real;
            return TRUE;
        case 8:
            string = memStringDupe(argument);
            if (row->callback != NULL)
            {
                row->callback(row->type, row->data, &string, reply);
                return TRUE;
            }
            if (*(char **)row->data != NULL)
            {
                memFree(*(char **)row->data);
            }
            *(char **)row->data = string;
            return TRUE;
        case 9:
            string = memStringDupe(argument);
            if (row->callback != NULL)
            {
                row->callback(row->type, row->data, &string, reply);
            }
            else
            {
                *(uword *)row->data = (uword)atoi(argument);
            }
            memFree(string);
            return TRUE;
        default:
            return TRUE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_consoleShowVariable                           0x0045f110
    Description : A name alone: the row's variable ("%s true" / "%s false", "%s: %d" (a dword: the exe reads four
                  bytes for every integer type), "%s: %f", "%s: %s"; a command without a variable says so). A name the
                  table lacks goes to the KAS functions.
    Return      : TRUE if a row matched
----------------------------------------------------------------------------*/
sdword cat_consoleShowVariable(char *name, char **reply, char *line)
{
    CatConsoleCommand *row;
    sdword index;

    for (index = 0; catConsoleCommandTable[index].type != -1; index++)
    {
        if (strcasecmp(name, catConsoleCommandTable[index].name) == 0)
        {
            break;
        }
    }
    if (catConsoleCommandTable[index].type == -1)
    {
        return cat_consoleKasCommand(reply, line);
    }
    row = &catConsoleCommandTable[index];
    if (row->data == NULL)
    {
        sprintf(*reply, "This command has no variable state!");
        return TRUE;
    }
    switch (row->type)
    {
        case 0:
            sprintf(*reply, (*(ubyte *)row->data != 0) ? "%s true" : "%s false", name);
            break;
        case 1: case 2: case 3: case 4: case 5: case 6: case 9:
            sprintf(*reply, "%s: %d", name, *(sdword *)row->data);
            break;
        case 7:
            sprintf(*reply, "%s: %f", name, (double)*(real32 *)row->data);
            break;
        case 8:
            snprintf(*reply, CAT_CON_LINE_LENGTH, "%s: %s", name,
                     (*(char **)row->data != NULL) ? *(char **)row->data : "(null)");  // HWI: bounded, NULL shown
            break;
        default:
            break;
    }
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_consoleKasCommand                             0x0045f270
    Description : A line naming a KAS function (the exe's cat_kasFunctionTable: name, argument types) runs it through
                  cat_kasCallFunctionByName 0x0045b9a0 with the parsed arguments ("KAS Returned %d").
                  CAT-TODO(stage 6 KAS): the KAS function table and cat_kasCallFunctionByName come with the kas2c missions; until then no line names a KAS function and the console answers "Not a command", as the exe does for an unknown name
----------------------------------------------------------------------------*/
sdword cat_consoleKasCommand(char **reply, char *line)
{
    (void)reply;
    (void)line;
    return FALSE;
}

/*=============================================================================
    The config file: per row its type (2 bytes), its id (4 bytes) and its value
=============================================================================*/
// HWI: the exe's directory is the developer path "\cataclysm\exe" (0x0072a720, cat_consoleConfigStartup) on the current
//      drive; here the user settings path (as every file the game writes)
static void catConConfigFileName(char *path, char *name)
{
    snprintf(path, PATH_MAX, "%s%s", catConConfigPath, (name != NULL) ? name : CAT_CON_CONFIG_DEFAULT);
}

// the exe's readers: a 16-bit value as (sbyte)second | (sword)(first << 8), a 32-bit one as two of them (the high half
//      from the last two bytes); the writer writes the values little-endian, so its files read back only where the
//      bytes are alike (exe behaviour: the developers' files predate the writer's byte order)
static sdword catConRead16(FILE *f)
{
    sdword first = fgetc(f), second = fgetc(f);

    return (sdword)(sbyte)second | (sdword)(sword)(((ubyte)first) << 8);
}

static sdword catConRead32(FILE *f)
{
    sdword low = catConRead16(f);
    sdword high = catConRead16(f);

    return (sdword)((udword)high * 0x10000u + (udword)low);
}

/*-----------------------------------------------------------------------------
    Name        : cat_consoleConfigFileLoad                         0x0045faf0
    Description : The config file's rows read in table order; a row whose type and id are not the table's next
                  row's is looked up in the whole table (a row of no match skips nothing).
----------------------------------------------------------------------------*/
void cat_consoleConfigFileLoad(char *name)
{
    char path[PATH_MAX];
    FILE *f;
    CatConsoleCommand *row, *expect;
    sdword type, id, value, index, length;
    char *string;

    catConConfigFileName(path, name);
    f = fopen(path, "r");
    if (f == NULL)
    {
        return;
    }
    expect = catConsoleCommandTable;
    while (!feof(f) && (expect->type != -1))
    {
        type = catConRead16(f);
        id = catConRead32(f);
        row = NULL;
        if ((id == expect->id) && (type == expect->type))
        {
            row = expect;
        }
        else
        {
            for (index = 0; catConsoleCommandTable[index].type != -1; index++)
            {
                if ((id == catConsoleCommandTable[index].id) && (type == catConsoleCommandTable[index].type))
                {
                    row = &catConsoleCommandTable[index];
                    break;
                }
            }
        }
        if ((row != NULL) && (row->data != NULL))
        {                                                   // CAT-FIX: the exe stores through a callback row's NULL
            switch (type)
            {
                case 0: case 1: case 2:
                    *(sbyte *)row->data = (sbyte)fgetc(f);
                    break;
                case 3: case 4: case 9:
                    *(uword *)row->data = (uword)catConRead16(f);
                    break;
                case 5: case 6:
                    *(sdword *)row->data = catConRead32(f);
                    break;
                case 7:
                    value = catConRead32(f);
                    *(real32 *)row->data = (real32)(value >> 16) + (real32)(value & 0xffff) * (1.0f / 65536.0f);
                    break;
                case 8:
                    length = catConRead32(f);
                    if (length > 0)
                    {
                        if (*(char **)row->data != NULL)
                        {
                            memFree(*(char **)row->data);
                        }
                        string = memAlloc(length + 1, "constring", NonVolatile);   // HWI: + 1 for the terminator
                        memset(string, 0, length + 1);
                        if (fread(string, 1, length, f) != (size_t)length)
                        {
                            string[0] = '\0';
                        }
                        *(char **)row->data = string;
                    }
                    break;
                default:
                    break;
            }
        }
        expect++;
    }
    fclose(f);
}

/*-----------------------------------------------------------------------------
    Name        : cat_consoleConfigFileSave                         0x0045fec0
    Description : Every row: its type (2 bytes), its id (4 bytes), its value (a byte; a word; a dword; a float as
                  16.16 fixed point; a string as its length and its bytes, a NULL string as length 0).
----------------------------------------------------------------------------*/
void cat_consoleConfigFileSave(char *name)
{
    char path[PATH_MAX];
    FILE *f;
    CatConsoleCommand *row;
    uword word;
    udword dword;
    char *string;

    catConConfigFileName(path, name);
    f = fopen(path, "w+");
    if (f == NULL)
    {
        return;
    }
    for (row = catConsoleCommandTable; row->type != -1; row++)
    {
        word = (uword)row->type;
        fwrite(&word, 2, 1, f);
        dword = (udword)row->id;
        fwrite(&dword, 4, 1, f);
        if (row->data == NULL)
        {                                                   // CAT-FIX: the exe reads through a callback row's NULL
            continue;
        }
        switch (row->type)
        {
            case 0: case 1: case 2:
                fputc(*(sbyte *)row->data, f);
                break;
            case 3: case 4: case 9:
                word = *(uword *)row->data;
                fwrite(&word, 2, 1, f);
                break;
            case 5: case 6:
                dword = *(udword *)row->data;
                fwrite(&dword, 4, 1, f);
                break;
            case 7:                                         // FAST_TO_INT of value * 65536 (the exe's 1.5 * 2^52 trick)
                dword = (udword)(sdword)lrint((double)(*(real32 *)row->data * 65536.0f));
                fwrite(&dword, 4, 1, f);
                break;
            case 8:
                string = *(char **)row->data;
                dword = (string == NULL) ? 0 : (udword)strlen(string);
                fwrite(&dword, 4, 1, f);
                if (dword > 0)
                {
                    fwrite(string, 1, dword, f);
                }
                break;
            default:
                break;
        }
    }
    fclose(f);
}

static void cat_consoleLoadConfig(sdword type, void *data, void *value, char **reply)       // 0x00460160
{
    (void)type;
    (void)data;
    (void)reply;
    cat_consoleConfigFileLoad(*(char **)value);
    memFree(*(char **)value);
}

static void cat_consoleSaveConfig(sdword type, void *data, void *value, char **reply)       // 0x00460180
{
    (void)type;
    (void)data;
    (void)reply;
    cat_consoleConfigFileSave(*(char **)value);
    memFree(*(char **)value);
}

void cat_consoleConfigStartup(void)                 // 0x004601a0
{
    catConConfigPath = memStringDupe(filePathPrepend("", FF_UserSettingsPath));    // HWI: see catConConfigFileName
}

void cat_consoleConfigShutdown(void)                // 0x004601c0
{
    if (catConConfigPath != NULL)
    {
        memFree(catConConfigPath);
        catConConfigPath = NULL;
    }
}

/*=============================================================================
    The command rows' callbacks
=============================================================================*/
// cat_spConsoleAddResources 0x0054d080: up to 20,000 RUs to the current player (its message goes to an empty stub)
static void cat_spConsoleAddResources(sdword type, void *data, void *value, char **reply)
{
    udword amount = *(udword *)value;

    (void)type;
    (void)data;
    (void)reply;
    if (amount < 20001)
    {
        dbgMessagef("\nAdded Resources to current player: %i", (sdword)amount);
        universe.curPlayerPtr->resourceUnits += amount;
    }
}

// cat_spConsoleWarpMission 0x00518820: in a single-player game, to mission 1..19 through the hyperspace button
static void cat_spConsoleWarpMission(sdword type, void *data, void *value, char **reply)
{
    sdword mission = *(sdword *)value;

    (void)type;
    (void)data;
    (void)reply;
    if ((singlePlayerGame == TRUE) && (mission > 0) && (mission < 20))
    {
        cat_consoleClose();
        spHyperspaceButtonPushed();
        singlePlayerGameInfo.currentMission = mission - 1;
    }
}

// cat_consoleAllTech 0x004f4350: every technology for the current player, its ships' upgrades with it
static void cat_consoleAllTech(sdword type, void *data, void *value, char **reply)
{
    (void)type;
    (void)data;
    (void)value;
    (void)reply;
    cat_rmTechSetInit((sdword *)&universe.curPlayerPtr->researchinfo, 2);
    cat_playerUpdateUpgradesFromResearch(universe.curPlayerPtr);
}

// cat_consoleUpgradeShips 0x0054d0e0: the selected ships' upgrades applied
static void cat_consoleUpgradeShips(sdword type, void *data, void *value, char **reply)
{
    sdword index;

    (void)type;
    (void)data;
    (void)value;
    (void)reply;
    for (index = 0; index < selSelected.numShips; index++)
    {
        cat_shipApplyUpgrades(selSelected.ShipPtr[index]);
    }
}

// cat_consoleExperienceShips 0x0054d1c0: each selected ship one fighter kill more (Ship +0x5E0), its level updated
static void cat_consoleExperienceShips(sdword type, void *data, void *value, char **reply)
{
    sdword index;

    (void)type;
    (void)data;
    (void)value;
    (void)reply;
    for (index = 0; index < selSelected.numShips; index++)
    {
        selSelected.ShipPtr[index]->catKillsByClass[CLASS_Fighter]++;
        cat_shipUpdateExperienceLevel(selSelected.ShipPtr[index]);
    }
}

/*=============================================================================
    HWI-TEST (M2-R6 startup-keys), port diagnostic, not in the exe: /catConsoleTest <ticks> (harness mode console). The
    headless Mission01 run of /oracleMission (one universe tick a frame, no input). At tick 16 the test views the game
    as the owner of Mission01's fleet and selects it through the selection API (as /catUiTest); at tick 24 it opens the
    console (cat_consoleOpen: the retail game has no opener, see the top of the file; a single-player game pauses, so
    the rest runs from the console's own draw callback, frame by frame); once the console has scrolled in it
      - shows every row of the command table ("]<name>" through cat_consoleExecuteCommand: the reply line), sets every
        variable row to a test value and back ("]<name> <value>", checked in the variable), runs the command rows
        (addResources 100 and 30000, warpTo 0 and 20: refused, alltech, upgradeships, experienceships: every selected
        ship one fighter kill more), "PROF", an unknown name ("Not a command"), saveConfig / loadConfig of a test file
        in the settings path (the file's size checked; what loadConfig reads back is logged: the exe's reader decodes
        the writer's little-endian words big-endian);
      - types lines through the console's key handler (cat_consoleKeyPress with virtual-key codes, no OS input): a
        command and Enter (echo and effect checked), TAB completion, the history (up), backspace, Escape;
      - scrolls it out with the tilde key; when it is gone (the universe runs again) "consoletest: done ...
        result=PASS|FAIL" and the engine quits.
=============================================================================*/
static sdword catConTestTicks = 0;
static sdword catConTestStep = 0;
static sdword catConTestFrames = 0;
static sdword catConTestSelected = 0;
static sdword catConTestFailures = 0;
static sdword catConTestShown = 0, catConTestSet = 0, catConTestCommands = 0, catConTestKeys = 0;
static bool32 catConTestOpened = FALSE, catConTestClosed = FALSE;
#define CAT_CON_TEST_SELECT_TICK    16
#define CAT_CON_TEST_OPEN_TICK      24
#define CAT_CON_TEST_FILE           "consoletest.cfg"

#define CAT_CON_TEST_FAIL(...)  do { catConTestFailures++; dbgMessagef("consoletest: FAIL " __VA_ARGS__); } while (0)

void catConsoleSelfTestSet(char *string)
{
    catConTestTicks = (sdword)strtol(string, NULL, 10);
    if (catConTestTicks <= 0)
    {
        catConTestTicks = 400;
    }
    oracleSetMission(string);                       // the headless Mission01 run with the virtual clock
}

bool32 catConsoleSelfTestRequested(void)
{
    return (catConTestTicks > 0);
}

// a line through the console as if typed and entered: "]" + text
static char *catConTestRun(char *text)
{
    char line[CAT_CON_LINE_LENGTH];

    snprintf(line, sizeof(line), "%s%s", CAT_CON_PROMPT, text);
    cat_consoleExecuteCommand(line);
    return catConOutput[CAT_CON_OUTPUT_LINES - 1];
}

// keys through the console's key handler (letters, digits, space, '.', '-', then the given special key or 0)
static void catConTestType(char *text, udword last)
{
    udword vk;

    for (; *text != '\0'; text++)
    {
        if ((*text >= 'a') && (*text <= 'z'))
        {
            vk = (udword)(*text - 'a' + 'A');
        }
        else if (*text == ' ')
        {
            vk = 0x20;
        }
        else if (*text == '.')
        {
            vk = 0xbe;
        }
        else if (*text == '-')
        {
            vk = 0xbd;
        }
        else
        {
            vk = (udword)*text;                     // the digits: their own codes
        }
        cat_consoleKeyPress(vk);
        catConTestKeys++;
    }
    if (last != 0)
    {
        cat_consoleKeyPress(last);
        catConTestKeys++;
    }
}

static void catConTestTable(void)
{
    CatConsoleCommand *row;
    char *reply, line[0x80];
    ubyte savedByte;
    sdword savedDword, ru, kills = 0, killsAfter = 0, index, size;
    real32 savedReal;
    FILE *f;
    char path[PATH_MAX];

    for (row = catConsoleCommandTable; row->type != -1; row++)
    {
        reply = catConTestRun(row->name);
        if (row->data == NULL)
        {
            if (strcmp(reply, "This command has no variable state!") != 0)
            {
                CAT_CON_TEST_FAIL("show %s: '%s'", row->name, reply);
            }
        }
        else if (strncasecmp(reply, row->name, strlen(row->name)) != 0)
        {
            CAT_CON_TEST_FAIL("show %s: '%s'", row->name, reply);
        }
        catConTestShown++;
        if (row->data == NULL)
        {
            continue;
        }
        switch (row->type)
        {
            case 0:
                savedByte = *(ubyte *)row->data;
                snprintf(line, sizeof(line), "%s off", row->name);
                catConTestRun(line);
                if (*(ubyte *)row->data != 0)
                {
                    CAT_CON_TEST_FAIL("%s off", row->name);
                }
                snprintf(line, sizeof(line), "%s on", row->name);
                catConTestRun(line);
                if (*(ubyte *)row->data != 1)
                {
                    CAT_CON_TEST_FAIL("%s on", row->name);
                }
                snprintf(line, sizeof(line), "%s %d", row->name, (sdword)savedByte);
                catConTestRun(line);
                if (*(ubyte *)row->data != savedByte)
                {
                    CAT_CON_TEST_FAIL("%s back to %d", row->name, (sdword)savedByte);
                }
                catConTestSet++;
                break;
            case 5:
                savedDword = *(sdword *)row->data;
                snprintf(line, sizeof(line), "%s 7", row->name);
                catConTestRun(line);
                if (*(sdword *)row->data != 7)
                {
                    CAT_CON_TEST_FAIL("%s 7", row->name);
                }
                reply = catConTestRun(row->name);
                snprintf(line, sizeof(line), "%s: 7", row->name);
                if (strcmp(reply, line) != 0)
                {
                    CAT_CON_TEST_FAIL("show %s 7: '%s'", row->name, reply);
                }
                snprintf(line, sizeof(line), "%s %d", row->name, savedDword);
                catConTestRun(line);
                if (*(sdword *)row->data != savedDword)
                {
                    CAT_CON_TEST_FAIL("%s back", row->name);
                }
                catConTestSet++;
                break;
            case 7:
                savedReal = *(real32 *)row->data;
                snprintf(line, sizeof(line), "%s 2.5", row->name);
                catConTestRun(line);
                if (*(real32 *)row->data != 2.5f)
                {
                    CAT_CON_TEST_FAIL("%s 2.5", row->name);
                }
                *(real32 *)row->data = savedReal;           // (a decimal round trip could differ in the last bit)
                catConTestSet++;
                break;
            case 8:
                break;                                      // loadConfig / saveConfig: below
            default:
                break;
        }
    }

    // the command rows
    ru = universe.curPlayerPtr->resourceUnits;
    catConTestRun("addResources 100");
    catConTestRun("addResources 30000");
    if (universe.curPlayerPtr->resourceUnits != ru + 100)
    {
        CAT_CON_TEST_FAIL("addResources: %d -> %d", ru, universe.curPlayerPtr->resourceUnits);
    }
    universe.curPlayerPtr->resourceUnits = ru;
    catConTestRun("warpTo 0");
    catConTestRun("warpTo 20");
    if (!catConsoleOpenFlag)
    {
        CAT_CON_TEST_FAIL("warpTo closed the console");
    }
    catConTestRun("alltech 1");
    catConTestRun("upgradeships 1");
    for (index = 0; index < selSelected.numShips; index++)
    {
        kills += selSelected.ShipPtr[index]->catKillsByClass[CLASS_Fighter];
    }
    catConTestRun("experienceships 1");
    for (index = 0; index < selSelected.numShips; index++)
    {
        killsAfter += selSelected.ShipPtr[index]->catKillsByClass[CLASS_Fighter];
    }
    if (killsAfter != kills + selSelected.numShips)
    {
        CAT_CON_TEST_FAIL("experienceships: kills %d -> %d for %d ships", kills, killsAfter, selSelected.numShips);
    }
    catConTestCommands = 8;
    reply = catConTestRun("PROF");
    reply = catConTestRun("nosuchcommand 1");
    if (strcmp(reply, "Not a command") != 0)
    {
        CAT_CON_TEST_FAIL("unknown command: '%s'", reply);
    }

    // the config file
    catConTestRun("saveConfig " CAT_CON_TEST_FILE);
    catConConfigFileName(path, CAT_CON_TEST_FILE);
    size = -1;
    f = fopen(path, "rb");
    if (f != NULL)
    {
        fseek(f, 0, SEEK_END);
        size = (sdword)ftell(f);
        fclose(f);
    }
    if (size != 240)                                        // 29 rows * 6 + 10 bytes + 5 dwords + 7 floats + 2 empty strings
    {
        CAT_CON_TEST_FAIL("saveConfig wrote %d bytes, not 240 (%s)", size, path);
    }
    savedDword = catConsoleRammingTime;
    catConsoleRammingTime = 12345;
    catConTestRun("loadConfig " CAT_CON_TEST_FILE);
    dbgMessagef("consoletest: config file %s bytes=%d rammingtime after loadConfig=%d (saved %d: the exe's reader does "
                "not read its writer's byte order)", path, size, catConsoleRammingTime, savedDword);
    catConsoleRammingTime = savedDword;
    remove(path);
    dbgMessagef("consoletest: table rows=%d shown=%d set=%d commands=%d failures=%d", (sdword)(row - catConsoleCommandTable),
                catConTestShown, catConTestSet, catConTestCommands, catConTestFailures);
}

static void catConTestKeyLines(void)
{
    char *echo, *line;
    ubyte saved = *(ubyte *)&mrDrawTactical;

    catConTestType("tactical 1", 0x0d);                    // a typed command
    echo = catConOutput[CAT_CON_OUTPUT_LINES - 2];
    if ((strcmp(echo, "]tactical 1") != 0) || (*(ubyte *)&mrDrawTactical != 1))
    {
        CAT_CON_TEST_FAIL("typed 'tactical 1': echo '%s' tactical %d", echo, (sdword)*(ubyte *)&mrDrawTactical);
    }
    catConTestType("exp", 0x09);                           // TAB completion
    line = catConInput[catConInputIndex];
    if (strcmp(line, "]experienceships") != 0)
    {
        CAT_CON_TEST_FAIL("TAB: '%s'", line);
    }
    cat_consoleKeyPress(0x1b);                              // Escape: the line cleared
    if (strcmp(catConInput[catConInputIndex], "]") != 0)
    {
        CAT_CON_TEST_FAIL("Escape: '%s'", catConInput[catConInputIndex]);
    }
    catConTestType("tacticalx", 0x08);                     // backspace
    if (strcmp(catConInput[catConInputIndex], "]tactical") != 0)
    {
        CAT_CON_TEST_FAIL("backspace: '%s'", catConInput[catConInputIndex]);
    }
    cat_consoleKeyPress(0x1b);
    cat_consoleKeyPress(0x26);                              // up: the last entered line
    if (strcmp(catConInput[catConInputIndex], "]tactical 1") != 0)
    {
        CAT_CON_TEST_FAIL("history: '%s'", catConInput[catConInputIndex]);
    }
    cat_consoleKeyPress(0x28);
    catConTestType(" 0", 0x0d);                            // the recalled line edited and entered
    *(ubyte *)&mrDrawTactical = saved;
    catConTestKeys += 4;
    dbgMessagef("consoletest: keys typed=%d input='%s' history=%d failures=%d", catConTestKeys,
                catConInput[catConInputIndex], catConInputIndex, catConTestFailures);
}

// HWI-TEST: called by cat_conTextDraw every frame the console is drawn
static void catConTestFrame(void)
{
    if (catConTestStep != 2)
    {
        return;
    }
    catConTestFrames++;
    if (catConScrollPos < catConHeight)
    {
        return;
    }
    catConTestStep = 3;
    dbgMessagef("consoletest: scrolled in frame=%d pos=%d paused=%d", catConTestFrames, catConScrollPos,
                (sdword)universePause);
    catConTestTable();
    catConTestKeyLines();
    cat_consoleKeyPress(0xc0);                              // tilde: scroll out
    if (catConScrollSpeed != -CAT_CON_SPEED)
    {
        CAT_CON_TEST_FAIL("tilde: speed %d", catConScrollSpeed);
    }
}

void catConsoleSelfTestTick(void)
{
    Node *node;
    Ship *ship;
    bool32 pass;

    if ((catConTestStep == 0) && ((sdword)universe.univUpdateCounter >= CAT_CON_TEST_SELECT_TICK))
    {
        catConTestStep = 1;
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {                                           // view as the fleet's owner (the headless start's player 1)
            ship = (Ship *)listGetStructOfNode(node);
            if ((ship->playerowner != NULL) && (ship->playerowner != universe.curPlayerPtr))
            {
                universe.curPlayerIndex = ship->playerowner->playerIndex;
                universe.curPlayerPtr = ship->playerowner;
                break;
            }
        }
        if (smSensorsActive)
        {
            smSensorsCloseForGood();
        }
        selSelected.numShips = 0;
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            ship = (Ship *)listGetStructOfNode(node);
            if ((ship->playerowner == universe.curPlayerPtr) && !bitTest(ship->header.flags, SOF_Dead) &&
                (selSelected.numShips < COMMAND_MAX_SHIPS))
            {
                selSelectionAddSingleShip(&selSelected, ship);
            }
        }
        catConTestSelected = selSelected.numShips;
        dbgMessagef("consoletest: selected tick=%u ships=%d player=%d", (unsigned)universe.univUpdateCounter,
                    catConTestSelected, (sdword)universe.curPlayerIndex);
    }
    else if ((catConTestStep == 1) && ((sdword)universe.univUpdateCounter >= CAT_CON_TEST_OPEN_TICK))
    {
        catConTestStep = 2;
        cat_consoleOpen();
        catConTestOpened = catConsoleOpenFlag && (catConScreen != NULL);
        dbgMessagef("consoletest: open tick=%u open=%d screen=%d single=%d paused=%d", (unsigned)universe.univUpdateCounter,
                    (sdword)catConsoleOpenFlag, catConScreen != NULL, (sdword)singlePlayerGame, (sdword)universePause);
    }
    else if ((catConTestStep == 3) && !catConsoleOpenFlag)
    {                                               // scrolled out and gone: the universe runs again
        catConTestStep = 4;
        catConTestClosed = (catConScreen == NULL) && (catConScrollPos == 0) && !(singlePlayerGame && universePause);
        pass = (catConTestSelected > 0) && catConTestOpened && catConTestClosed && (catConTestFailures == 0) &&
               (catConTestShown == 29) && (catConTestSet == 22) && (catConTestFrames > 0);
        dbgMessagef("consoletest: done selected=%d frames=%d shown=%d set=%d commands=%d keys=%d closed=%d failures=%d "
                    "result=%s", catConTestSelected, catConTestFrames, catConTestShown, catConTestSet, catConTestCommands,
                    catConTestKeys, catConTestClosed, catConTestFailures, pass ? "PASS" : "FAIL");
        utyGameQuit(NULL, NULL);
    }
}
