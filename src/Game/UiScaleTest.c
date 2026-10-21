/*=============================================================================
    Name    : UiScaleTest.c
    Purpose : HWI-TEST (M3-R1 ui-scale): the UI scale's self-test, /catUiScaleTest (harness mode uiscale; pass --res WxH
              for the size, --cfg 'uiScale N' or "-- /uiScale N" for an override). Runs once the front end is up at
              Main_game_screen, synchronously, with no OS input: the mouse is the engine's own path (mouse.c mousePoll,
              which takes a window position from uiscale.c's test hook instead of SDL_GetMouseState, as SDL would
              report it) and its buttons are SDL_MOUSEBUTTONDOWN / UP events handed to main.c HandleEvent, the function
              SDL's event loop calls; the regions are processed as regProcessTask does (mousePoll, regRegionProcess).
              "uiscaletest:" lines, "uiscaletest: done ... result=PASS|FAIL"; utility.c then quits.

              1. the scale: auto by the physical height (2160 -> 200, 1440 -> 150, else 100) or the setting, the
                 logical size physical / scale rounded;
              2. the window fit's table (uiscale.c uiScaleWindowFitCheck: the user's 3840x2160 request against a
                 3840x2160 display with 3840x1960 usable bounds, and others);
              3. the mouse at the physical bottom row, the bottom-right corner and below the window: the logical
                 bottom row, and the region there;
              4. a click at the physical centre of Main_game_screen's OPTIONS button (its callback opens the options)
                 and one just outside it (nothing);
              5. the Video screens' UI SCALE buttons (Video_Options pushed, a scripted click on its 150% button,
                 Cancel; a click on 200%, then Accept's order: applied at once in the front end, the main menu re-made
                 at the new logical size; the in-game Video screen's atoms);
              6. the options file's uiScale row written and read back (the window's logical size kept), the switch
                 run-only, the console row.
    Created 2026-10-21 for Homeworld-Infection.
=============================================================================*/

#include "UiScaleTest.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "Console.h"
#include "Debug.h"
#include "FEFlow.h"
#include "File.h"
#include "main.h"
#include "mouse.h"
#include "Options.h"
#include "Region.h"
#include "uiscale.h"
#include "utility.h"

bool32 catUiScaleSelfTest = FALSE;

extern sdword regProcessingRegions;
udword regRegionProcess(regionhandle reg, udword mask);
extern regionhandle ghMainRegion;

static sdword utChecks = 0;
static sdword utFails = 0;

static void utCheck(bool32 ok, const char *what)
{
    utChecks++;
    if (!ok)
    {
        utFails++;
        dbgMessagef("uiscaletest: FAIL %s", what);
    }
}

static const char *utTopScreen(void)
{
    return (feStackIndex >= 0 && feStack[feStackIndex].screen != NULL) ? feStack[feStackIndex].screen->name : "none";
}

// one pass of regProcessTask's body: the mouse polled, every region processed
static void utProcess(void)
{
    mousePoll();
    regProcessingRegions = TRUE;
    regRegionProcess(&regRootRegion, 0xffffffff);
    regProcessingRegions = FALSE;
}

// the pointer at window position (px, py), then a left press and release there, as SDL delivers them
static void utClick(sdword px, sdword py)
{
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    uiScaleTestMouseSet(TRUE, px, py);
    utProcess();                                    // the pointer arrives (RPE_Enter)
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.state = SDL_PRESSED;
    event.button.clicks = 1;
    event.button.x = px;
    event.button.y = py;
    HandleEvent(&event);
    utProcess();                                    // RPE_PressLeft
    event.type = SDL_MOUSEBUTTONUP;
    event.button.state = SDL_RELEASED;
    HandleEvent(&event);
    utProcess();                                    // RPE_ReleaseLeft: the button's callback
    SDL_Delay(600);                                 // past HandleEvent's 500 ms double-click window
}

// the window position of a logical pixel's centre
static sdword utPhysX(sdword x)
{
    return (sdword)(((real64)x + 0.5) * (real64)MAIN_WindowWidthActual / (real64)MAIN_WindowWidth);
}
static sdword utPhysY(sdword y)
{
    return (sdword)(((real64)y + 0.5) * (real64)MAIN_WindowHeightActual / (real64)MAIN_WindowHeight);
}

// the deepest region under (x, y) that takes mouse events, in processing order
static regionhandle utRegionAt(regionhandle reg, sdword x, sdword y)
{
    regionhandle child, found;

    for (child = reg->child; child != NULL; child = child->next)
    {
        if (x >= child->rect.x0 && x < child->rect.x1 && y >= child->rect.y0 && y < child->rect.y1)
        {
            found = utRegionAt(child, x, y);
            if (found != NULL)
            {
                return found;
            }
            if (bitTest(child->flags, RPM_MouseEvents))
            {
                return child;
            }
        }
    }
    return NULL;
}

static featom *utAtom(char *screenName, char *name, char *text, sdword pData)
{
    fescreen *screen = feScreenFind(screenName);
    sdword i;

    if (screen == NULL)
    {
        return NULL;
    }
    for (i = 0; i < screen->nAtoms; i++)
    {
        featom *atom = &screen->atoms[i];
        if (name != NULL && atom->name != NULL && strcmp(atom->name, name) == 0 &&
            (pData < 0 || (sdword)(smemsize)atom->pData == pData))
        {
            return atom;
        }
        if (text != NULL && atom->type == FA_StaticText && atom->pData != NULL && strcmp((char *)atom->pData, text) == 0)
        {
            return atom;
        }
    }
    return NULL;
}

// 1. the scale chosen for this window
static void utScale(void)
{
    sdword h = MAIN_WindowHeightActual, w = MAIN_WindowWidthActual, expect, most;
    char what[160];

    if (uiScale > 0)
    {
        expect = uiScale;
    }
    else
    {
        expect = (h >= 2160) ? 200 : ((h >= 1440) ? 150 : 100);
    }
    most = min(w * 100 / 640, h * 100 / 480);
    expect = (expect > most) ? most : expect;
    expect = (expect < 100) ? 100 : expect;
    dbgMessagef("uiscaletest: scale physical=%dx%d logical=%dx%d scale=%d source=%s uiScale=%d expect=%d", (int)w, (int)h,
                (int)MAIN_WindowWidth, (int)MAIN_WindowHeight, (int)uiScalePercent, uiScaleSource(), (int)uiScale,
                (int)expect);
    snprintf(what, sizeof(what), "the scale %d%% for %dx%d (%s)", (int)expect, (int)w, (int)h, uiScaleSource());
    utCheck(uiScalePercent == expect, what);
    utCheck(MAIN_WindowWidth == (w * 100 + expect / 2) / expect && MAIN_WindowHeight == (h * 100 + expect / 2) / expect,
            "the logical size = physical / scale, rounded");
}

// 2. the window fit's rule
static void utFit(void)
{
    static const struct { sdword rw, rh, full, border, dw, dh, uw, uh, ew, eh, eflags; const char *name; } cases[] =
    {
        { 3840, 2160, 0, 1, 3840, 2160, 3840, 1960, 3840, 2160, SDL_WINDOW_FULLSCREEN_DESKTOP, "the user's: 3840x2160 windowed, 3840x1960 usable" },
        { 3840, 2160, 0, 1, 3840, 2160, 3840, 2160, 3840, 2160, SDL_WINDOW_FULLSCREEN_DESKTOP, "3840x2160 windowed, 3840x2160 usable" },
        { 3840, 2160, 0, 0, 3840, 2160, 3840, 1960, 3840, 2160, SDL_WINDOW_FULLSCREEN_DESKTOP, "3840x2160 /noBorder, 3840x1960 usable" },
        { 3840, 2160, 1, 1, 3840, 2160, 3840, 1960, 3840, 2160, SDL_WINDOW_FULLSCREEN_DESKTOP, "3840x2160 /fullscreen" },
        { 2560, 1440, 0, 1, 3840, 2160, 3840, 1960, 2560, 1440, 0, "2560x1440 windowed on the 4K desktop: fits" },
        { 3840, 2160, 0, 1, 5120, 2880, 5120, 2800, 3840, 2160, 0, "3840x2160 windowed on a 5K desktop: fits" },
        { 3840, 2300, 0, 1, 3840, 2400, 3840, 2200, 3673, 2200, 0, "3840x2300 windowed, 3840x2200 usable: shrunk" },
        { 2400, 1600, 0, 1, 2560, 1440, 2560, 1400, 2100, 1400, 0, "2400x1600 on a 2560x1440 display: shrunk" },
        { 2560, 1600, 0, 1, 2560, 1440, 2560, 1400, 2560, 1440, SDL_WINDOW_FULLSCREEN_DESKTOP, "2560x1600 on a 2560x1440 display: the display" },
    };
    sdword i, w, h;
    udword flags;
    char what[200];

    // the display a request goes to (uiscale.c uiScaleFitDisplay): the user's desktop with a smaller display 0
    {
        static const sdword dw[2] = { 1920, 3840 }, dh[2] = { 1080, 2160 }, dw0[2] = { 3840, 1920 }, dh0[2] = { 2160, 1080 };
        sdword d4k = uiScaleFitDisplay(2, dw, dh, 3840, 2160), d1080 = uiScaleFitDisplay(2, dw, dh, 1920, 1080);
        sdword dbig = uiScaleFitDisplay(2, dw, dh, 5120, 2880), d0 = uiScaleFitDisplay(2, dw0, dh0, 3840, 2160);

        dbgMessagef("uiscaletest: fit display (1920x1080, 3840x2160): 3840x2160 -> %d, 1920x1080 -> %d, 5120x2880 -> %d; "
                    "(3840x2160, 1920x1080): 3840x2160 -> %d", (int)d4k, (int)d1080, (int)dbig, (int)d0);
        utCheck(d4k == 1 && d1080 == 0 && dbig == 1 && d0 == 0, "fit display: the request's display (the 4K one for 3840x2160)");
    }
    for (i = 0; i < (sdword)(sizeof(cases) / sizeof(cases[0])); i++)
    {
        flags = cases[i].full ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
        uiScaleWindowFitCheck(cases[i].rw, cases[i].rh, cases[i].full, cases[i].border, cases[i].dw, cases[i].dh,
                              cases[i].uw, cases[i].uh, &flags, &w, &h);
        snprintf(what, sizeof(what), "fit %s: %dx%d %s", cases[i].name, (int)cases[i].ew, (int)cases[i].eh,
                 cases[i].eflags ? "fullscreen-desktop" : "window");
        utCheck(w == cases[i].ew && h == cases[i].eh &&
                ((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP) ==
                (cases[i].eflags == SDL_WINDOW_FULLSCREEN_DESKTOP), what);
    }
}

// 3. the mouse at the physical edges: the logical edges, never cut off
static void utEdges(void)
{
    sdword pw = MAIN_WindowWidthActual, ph = MAIN_WindowHeightActual;
    struct { sdword px, py, ex, ey; const char *name; } points[] =
    {
        { pw / 2, ph - 1, -1, MAIN_WindowHeight - 1, "the physical bottom row" },
        { pw - 1, ph - 1, MAIN_WindowWidth - 1, MAIN_WindowHeight - 1, "the bottom-right corner" },
        { 0, 0, 0, 0, "the top-left corner" },
        { pw / 2, ph + 50, -1, MAIN_WindowHeight - 1, "50 rows below the window (a held button)" },
        { pw + 9, ph / 2, MAIN_WindowWidth - 1, -1, "right of the window" },
    };
    sdword i;
    regionhandle reg;
    char what[200];

    for (i = 0; i < (sdword)(sizeof(points) / sizeof(points[0])); i++)
    {
        uiScaleTestMouseSet(TRUE, points[i].px, points[i].py);
        utProcess();
        reg = utRegionAt(&regRootRegion, mouseCursorX(), mouseCursorY());
        dbgMessagef("uiscaletest: mouse %s window=%d,%d -> logical=%d,%d region=%d,%d-%d,%d atom=%s", points[i].name,
                    (int)points[i].px, (int)points[i].py, (int)mouseCursorX(), (int)mouseCursorY(),
                    reg ? (int)reg->rect.x0 : -1, reg ? (int)reg->rect.y0 : -1, reg ? (int)reg->rect.x1 : -1,
                    reg ? (int)reg->rect.y1 : -1,
                    (reg && reg->atom && ((featom *)reg->atom)->name) ? ((featom *)reg->atom)->name : "-");
        snprintf(what, sizeof(what), "mouse %s lands on the logical edge", points[i].name);
        utCheck((points[i].ex < 0 || mouseCursorX() == points[i].ex) && (points[i].ey < 0 || mouseCursorY() == points[i].ey) &&
                mouseCursorX() >= 0 && mouseCursorX() < MAIN_WindowWidth && mouseCursorY() >= 0 &&
                mouseCursorY() < MAIN_WindowHeight, what);
        if (points[i].ey == MAIN_WindowHeight - 1)
        {
            snprintf(what, sizeof(what), "mouse %s: a region reaching the bottom edge is under it", points[i].name);
            utCheck(reg != NULL && reg->rect.y1 >= MAIN_WindowHeight, what);
        }
    }
}

// 4. a click at the OPTIONS button's centre and one just outside it
static void utButtonClick(void)
{
    featom *atom = utAtom("Main_game_screen", "OP_Options_Start", NULL, -1);
    regionhandle reg = (atom != NULL) ? (regionhandle)atom->region : NULL;
    sdword cx, cy, px, py;
    char what[200];

    utCheck(reg != NULL && strcmp(utTopScreen(), "Main_game_screen") == 0, "click: Main_game_screen's OPTIONS button");
    if (reg == NULL)
    {
        return;
    }
    // just outside: one logical pixel right of the button, at its middle row
    px = utPhysX(reg->rect.x1);
    py = utPhysY((reg->rect.y0 + reg->rect.y1) / 2);
    utClick(px, py);
    dbgMessagef("uiscaletest: click miss window=%d,%d -> logical=%d,%d button=%d,%d-%d,%d screen=%s", (int)px, (int)py,
                (int)mouseCursorX(), (int)mouseCursorY(), (int)reg->rect.x0, (int)reg->rect.y0, (int)reg->rect.x1,
                (int)reg->rect.y1, utTopScreen());
    utCheck(mouseCursorX() == reg->rect.x1 && strcmp(utTopScreen(), "Main_game_screen") == 0,
            "click just outside the button: nothing opens");
    // the centre
    cx = (reg->rect.x0 + reg->rect.x1) / 2;
    cy = (reg->rect.y0 + reg->rect.y1) / 2;
    px = utPhysX(cx);
    py = utPhysY(cy);
    utClick(px, py);
    dbgMessagef("uiscaletest: click hit window=%d,%d -> logical=%d,%d button=%d,%d-%d,%d (640x480 %d,%d-%d,%d) screen=%s",
                (int)px, (int)py, (int)mouseCursorX(), (int)mouseCursorY(), (int)reg->rect.x0, (int)reg->rect.y0,
                (int)reg->rect.x1, (int)reg->rect.y1, (int)atom->loadedX, (int)atom->loadedY,
                (int)(atom->loadedX + atom->loadedWidth), (int)(atom->loadedY + atom->loadedHeight), utTopScreen());
    snprintf(what, sizeof(what), "click at the button's centre (%d,%d): its callback opens the options", (int)cx, (int)cy);
    utCheck(mouseCursorX() == cx && mouseCursorY() == cy && strcmp(utTopScreen(), "Main_game_screen") != 0, what);
    snprintf(what, sizeof(what), "the button at the exe's centring at the logical size: x0 = %d + (%d - 640) / 2",
             (int)atom->loadedX, (int)MAIN_WindowWidth);
    utCheck(reg->rect.x0 == atom->loadedX + (MAIN_WindowWidth - 640) / 2 &&
            reg->rect.y0 == atom->loadedY + (MAIN_WindowHeight - 480) / 2, what);
    feAllScreensDelete();
    feScreenStart(ghMainRegion, "Main_game_screen");
}

// the options file's uiScale rows: how many, and the last one's value
static sdword utFileRow(char *key, sdword *count)
{
    char path[PATH_MAX], line[256], format[64];
    FILE *f;
    sdword value = -1;
    int v;

    *count = 0;
    snprintf(format, sizeof(format), "%s %%d", key);
    snprintf(path, sizeof(path), "%s", filePathPrepend("Cataclysm.cfg", FF_UserSettingsPath));
    f = fopen(path, "r");
    if (f != NULL)
    {
        while (fgets(line, sizeof(line), f) != NULL)
        {
            if (sscanf(line, format, &v) == 1)
            {
                (*count)++;
                value = v;
            }
        }
        fclose(f);
    }
    return value;
}

// 5. the Video screens' UI SCALE buttons
static void utControl(sdword startScale, sdword startPercent)
{
    static sdword values[4] = { 0, 100, 150, 200 };
    static char *texts[4] = { "AUTO", "100%", "150%", "200%" };
    featom *radio, *atom;
    regionhandle reg;
    sdword i, found = 0, count, row, reqW, reqH, base;
    char what[200];

    opOptionsSaveSettings();                        // the options screens' Cancel copies, as opening them saves
    feScreenStart(ghMainRegion, "Video_Options");
    for (i = 0; i < 4; i++)
    {
        radio = utAtom("Video_Options", "OP_UiScale", NULL, values[i]);
        atom = utAtom("Video_Options", NULL, texts[i], -1);
        found += (radio != NULL && radio->region != NULL && atom != NULL && atom->region != NULL) ? 1 : 0;
        if (radio != NULL && values[i] == uiScale)
        {
            utCheck(FECHECKED(radio) ? TRUE : FALSE, "control: the button of the current setting is checked");
        }
    }
    atom = utAtom("Video_Options", NULL, "UI SCALE", -1);
    dbgMessagef("uiscaletest: control Video_Options buttons=%d title=%s uiScale=%d", (int)found,
                (atom != NULL) ? "yes" : "no", (int)uiScale);
    utCheck(found == 4 && atom != NULL, "control: Video_Options has the title and the four buttons with labels");
    // a scripted click on the 150% button, then Cancel
    radio = utAtom("Video_Options", "OP_UiScale", NULL, 150);
    reg = (radio != NULL) ? (regionhandle)radio->region : NULL;
    // [audit: M3-R1 ui-scale] the button sits in the screen's panel: the exe's centring of its 640x480 position, as
    //      the screen's own atoms (the producer's buttons sat at their 640x480 coordinates until a relayout)
    if (reg != NULL)
    {
        dbgMessagef("uiscaletest: control 150%% button=%d,%d-%d,%d (640x480 %d,%d)", (int)reg->rect.x0, (int)reg->rect.y0,
                    (int)reg->rect.x1, (int)reg->rect.y1, (int)radio->loadedX, (int)radio->loadedY);
    }
    utCheck(reg != NULL && reg->rect.x0 == radio->loadedX + (MAIN_WindowWidth - 640) / 2 &&
            reg->rect.y0 == radio->loadedY + (MAIN_WindowHeight - 480) / 2,
            "control: the UI SCALE buttons at the exe's centring of the screen");
    if (reg != NULL)
    {
        utClick(utPhysX((reg->rect.x0 + reg->rect.x1) / 2), utPhysY((reg->rect.y0 + reg->rect.y1) / 2));
        dbgMessagef("uiscaletest: control click 150%% at logical %d,%d -> uiScale=%d checked=%d", (int)mouseCursorX(),
                    (int)mouseCursorY(), (int)uiScale, FECHECKED(radio) ? 1 : 0);
        utCheck(uiScale == 150 && FECHECKED(radio), "control: a click on 150% sets uiScale 150");
    }
    opRestoreSavedSettings();                       // Cancel: the restore, then the screen's deletion
    feCurrentScreenDelete();
    dbgMessagef("uiscaletest: control cancel uiScale=%d scale=%d", (int)uiScale, (int)uiScalePercent);
    utCheck(uiScale == startScale && uiScalePercent == startPercent, "control: Cancel restores uiScale");
    // a click on 200%, then a click on the screen's ACCEPT button (its callback opOptionsAccept ->
    // opOptionsAcceptHelper: uiScaleUpdate, the screen gone, the main menu made, the save) [audit: M3-R1 ui-scale: the
    // real button's click, inside the region processing, replaces the producer's call-by-call copy of its order]
    opOptionsSaveSettings();
    base = feStackIndex;                            // Main_game_screen's place on the stack
    feScreenStart(ghMainRegion, "Video_Options");
    radio = utAtom("Video_Options", "OP_UiScale", NULL, 200);
    reg = (radio != NULL) ? (regionhandle)radio->region : NULL;
    if (reg != NULL)
    {
        utClick(utPhysX((reg->rect.x0 + reg->rect.x1) / 2), utPhysY((reg->rect.y0 + reg->rect.y1) / 2));
    }
    utCheck(uiScale == 200, "control: a click on 200% sets uiScale 200");
    atom = utAtom("Video_Options", "OP_Options_Accept", NULL, -1);
    reg = (atom != NULL) ? (regionhandle)atom->region : NULL;
    if (reg != NULL)
    {
        utClick(utPhysX((reg->rect.x0 + reg->rect.x1) / 2), utPhysY((reg->rect.y0 + reg->rect.y1) / 2));
        utProcess();                                // the frame after: the screens the callback left
    }
    for (i = 0, found = 0; i <= feStackIndex; i++)
    {
        found += (feStack[i].screen != NULL && strcmp(feStack[i].screen->name, "Video_Options") == 0) ? 1 : 0;
    }
    // (the test pushed Video_Options on Main_game_screen, so the callback's feScreenDisappear + feScreenStart leave the
    // main menu on the stack twice: stack = before + 1)
    dbgMessagef("uiscaletest: control accept click screen=%s stack=%d (before %d) video=%d", utTopScreen(),
                (int)feStackIndex, (int)base, (int)found);
    utCheck(reg != NULL && strcmp(utTopScreen(), "Main_game_screen") == 0 && found == 0,
            "control: the ACCEPT button's click closes Video_Options and ends in Main_game_screen");
    row = utFileRow("uiScale", &count);
    atom = utAtom("Main_game_screen", "OP_Options_Start", NULL, -1);
    reg = (atom != NULL) ? (regionhandle)atom->region : NULL;
    dbgMessagef("uiscaletest: control accept 200%% scale=%d logical=%dx%d row=%d rows=%d options button=%d,%d", (int)uiScalePercent,
                (int)MAIN_WindowWidth, (int)MAIN_WindowHeight, (int)row, (int)count, reg ? (int)reg->rect.x0 : -1,
                reg ? (int)reg->rect.y0 : -1);
    utCheck(count == 1 && row == 200, "control: Accept saves uiScale 200");
    utCheck(uiScalePercent == min(200, min(MAIN_WindowWidthActual * 100 / 640, MAIN_WindowHeightActual * 100 / 480)) &&
            MAIN_WindowWidth == (MAIN_WindowWidthActual * 100 + uiScalePercent / 2) / uiScalePercent,
            "control: Accept applies 200% in the front end (capped at 640x480 logical)");
    utCheck(reg != NULL && reg->rect.x0 == atom->loadedX + (MAIN_WindowWidth - 640) / 2 &&
            reg->rect.y0 == atom->loadedY + (MAIN_WindowHeight - 480) / 2,
            "control: the main menu re-made at the new logical size");
    uiScaleWindowRequestGet(&reqW, &reqH);
    row = utFileRow("screenWidth", &count);
    utCheck(row == reqW && utFileRow("screenHeight", &count) == reqH, "control: the saved screenWidth / Height are the request");
    // back to the start
    uiScale = startScale;
    (void)uiScaleApply(startScale);
    feAllScreensDelete();
    feScreenStart(ghMainRegion, "Main_game_screen");
    opOptionsSaveSettings();                        // the file's row as the start-up wrote it
    dbgMessagef("uiscaletest: control back scale=%d logical=%dx%d", (int)uiScalePercent, (int)MAIN_WindowWidth,
                (int)MAIN_WindowHeight);
    utCheck(uiScalePercent == startPercent, "control: the start scale back");
    // the in-game Video screen (not pushed: no game runs)
    for (i = 0, found = 0; i < 4; i++)
    {
        radio = utAtom("Video", "OP_UiScale", NULL, values[i]);
        atom = utAtom("Video", NULL, texts[i], -1);
        found += (radio != NULL && radio->type == FA_RadioButton && atom != NULL) ? 1 : 0;
    }
    atom = utAtom("Video", NULL, "UI SCALE", -1);
    dbgMessagef("uiscaletest: control in-game Video buttons=%d title=%s", (int)found, (atom != NULL) ? "yes" : "no");
    utCheck(found == 4 && atom != NULL, "control: the in-game Video screen has the title and the four buttons");
}

// 6. the options-file row, the switch, the console row
static void utSurfaces(sdword startScale)
{
    sdword values[2] = { 150, 0 }, i, count, row, logicalW = MAIN_WindowWidth, logicalH = MAIN_WindowHeight;
    bool32 switched = uiScaleSwitchStands();
    sdword file = uiScaleFileValue();
    char buffer[512] = "", *reply = buffer, line[64], what[200];

    row = utFileRow("uiScale", &count);
    dbgMessagef("uiscaletest: file start uiScale=%d switch=%d file=%d row=%d rows=%d", (int)uiScale, (int)switched,
                (int)file, (int)row, (int)count);
    utCheck(count == 1 && row == file, "file: the start-up's save wrote the options file's uiScale");
    uiScaleSwitchEnd();
    for (i = 0; i < 2; i++)
    {
        uiScale = values[i];
        opOptionsSaveSettings();
        row = utFileRow("uiScale", &count);
        uiScale = 77;                               // scrambled: the file must set it back
        catUtyOptionsFileReadBack();
        dbgMessagef("uiscaletest: file accept-%d row=%d rows=%d read back uiScale=%d logical=%dx%d", (int)values[i],
                    (int)row, (int)count, (int)uiScale, (int)MAIN_WindowWidth, (int)MAIN_WindowHeight);
        snprintf(what, sizeof(what), "file: Cataclysm.cfg has uiScale %d and reads it back, the logical size kept",
                 (int)values[i]);
        utCheck(count == 1 && row == values[i] && uiScale == values[i] && MAIN_WindowWidth == logicalW &&
                MAIN_WindowHeight == logicalH, what);
    }
    uiScaleSwitchSet(200);                          // run-only: the save keeps the file's 0
    opOptionsSaveSettings();
    row = utFileRow("uiScale", &count);
    dbgMessagef("uiscaletest: switch run-only uiScale=%d row=%d", (int)uiScale, (int)row);
    utCheck(uiScale == 200 && count == 1 && row == 0, "switch: /uiScale is run-only");
    uiScaleSwitchEnd();
    snprintf(line, sizeof(line), "uiScale 150");
    cat_consoleRunCommand("uiScale", "150", &reply, line);
    dbgMessagef("uiscaletest: console 'uiScale 150' -> uiScale=%d", (int)uiScale);
    utCheck(uiScale == 150, "console: uiScale 150");
    snprintf(line, sizeof(line), "uiScale 0");
    cat_consoleRunCommand("uiScale", "0", &reply, line);
    utCheck(uiScale == 0, "console: uiScale 0");
    uiScale = file;                                 // as the start-up left it
    if (switched)
    {
        uiScaleSwitchSet(startScale);
    }
    opOptionsSaveSettings();
    row = utFileRow("uiScale", &count);
    dbgMessagef("uiscaletest: file end uiScale=%d switch=%d row=%d", (int)uiScale, (int)uiScaleSwitchStands(), (int)row);
    utCheck(uiScale == startScale && uiScaleSwitchStands() == switched && row == file, "file: the start state and row back");
}

// 7. [audit: M3-R1 ui-scale] a mode switch at a UI scale (the Video screen's resolution list: opOptionsAcceptHelper /
//    mainRestoreRender -> mainLoadGL -> mainStartupGL): mainRescaleMainWindow lays the front end out at the REQUEST
//    before the new window exists, then setupPixelFormat's uiScaleWindowCreated sets the logical size and must lay it
//    out again there. The steps of that order on this window (the request kept, so the physical size stays; a real
//    mainLoadGL makes a second window and GoK's mainDestroyWindow then swallows the quit: the audit ran it once,
//    docs/audits/m3-r1-ui-scale.md); at 100% the request is the logical size and nothing moves
void mainRescaleMainWindow(void);
static void utModeSwitch(void)
{
    extern SDL_Window *sdlwindow;
    sdword reqW, reqH;
    featom *atom;
    regionhandle reg;
    sdword laidW;
    char what[200];

    uiScaleWindowRequestGet(&reqW, &reqH);
    uiScaleWindowRequest(reqW, reqH);               // the new request into MAIN_Window* (opOptionsAcceptHelper)
    mainRescaleMainWindow();                        // mainStartupGL: laid out at the request
    laidW = ghMainRegion->rect.x1;
    uiScaleWindowCreated(sdlwindow, reqW, reqH);    // setupPixelFormat: the window's logical size
    atom = utAtom("Main_game_screen", "OP_Options_Start", NULL, -1);
    reg = (atom != NULL) ? (regionhandle)atom->region : NULL;
    dbgMessagef("uiscaletest: modeswitch request=%dx%d laid out at %d, then logical=%dx%d main=%d,%d options button=%d,%d "
                "screen=%s", (int)reqW, (int)reqH, (int)laidW, (int)MAIN_WindowWidth, (int)MAIN_WindowHeight,
                (int)ghMainRegion->rect.x1, (int)ghMainRegion->rect.y1, reg ? (int)reg->rect.x0 : -1,
                reg ? (int)reg->rect.y0 : -1, utTopScreen());
    snprintf(what, sizeof(what), "mode switch: the main region and the main menu at the logical size %dx%d, not the "
             "request %dx%d", (int)MAIN_WindowWidth, (int)MAIN_WindowHeight, (int)reqW, (int)reqH);
    utCheck(ghMainRegion->rect.x1 == MAIN_WindowWidth && ghMainRegion->rect.y1 == MAIN_WindowHeight && reg != NULL &&
            reg->rect.x0 == atom->loadedX + (MAIN_WindowWidth - 640) / 2 &&
            reg->rect.y0 == atom->loadedY + (MAIN_WindowHeight - 480) / 2, what);
}

void catUiScaleRunSelfTest(void)
{
    sdword startScale = uiScale, startPercent = uiScalePercent;

    dbgMessagef("uiscaletest: start physical=%dx%d logical=%dx%d scale=%d source=%s uiScale=%d screen=%s",
                (int)MAIN_WindowWidthActual, (int)MAIN_WindowHeightActual, (int)MAIN_WindowWidth, (int)MAIN_WindowHeight,
                (int)uiScalePercent, uiScaleSource(), (int)uiScale, utTopScreen());
    utScale();
    utFit();
    utEdges();
    utButtonClick();
    utControl(startScale, startPercent);
    utSurfaces(startScale);
    uiScaleTestMouseSet(FALSE, 0, 0);
    utModeSwitch();                                 // [audit: M3-R1 ui-scale]
    dbgMessagef("uiscaletest: done physical=%dx%d logical=%dx%d scale=%d checks=%d fails=%d result=%s",
                (int)MAIN_WindowWidthActual, (int)MAIN_WindowHeightActual, (int)MAIN_WindowWidth, (int)MAIN_WindowHeight,
                (int)uiScalePercent, (int)utChecks, (int)utFails, (utFails == 0) ? "PASS" : "FAIL");
}
