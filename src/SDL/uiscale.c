/*=============================================================================
    Name    : uiscale.c
    Purpose : HWI-FEATURE(ui-scale) (M3-R1 producer 5; the user's play-test item 5, "game UI is very small when at 4k.
              ensure that the ui scale is increased by 200% on that mode and 150% on 2k resolution") and the window
              fit of windowed mode (HWI-FIX; the user's item 6, "the mouse can't reach the bottom of the screen when
              in 4k"). Neither is in cataclysm.exe: the exe draws its 640x480-authored front end centred at any
              resolution (feAllScreensReposition 0x00474ae0: x + (MAIN_WindowWidth - 640) / 2) and creates its
              windowed-mode window at the screen's origin with the requested client size and a border
              (WinMain 0x005631f0: CreateWindowExA(.., 0x06cf0008, 0, 0, W + 2 * SM_CXFRAME,
              H + SM_CYCAPTION + 2 * SM_CYFRAME, ..)), whatever the desktop's size.

              The design is GoK's own split (docs/m3-r1-ui-scale.md): the 2D projection maps 0 .. MAIN_WindowWidth /
              Height onto the whole GL viewport (prim2d.h primScreenToGL*, the font quads, the cursor), the viewport
              is the drawable (the physical size, MAIN_WindowWidthActual / HeightActual), and the mouse is scaled
              between the two (mouse.c). GoK used it for a fullscreen-desktop window at a lower UI resolution; the
              UI scale sets MAIN_WindowWidth / Height = physical / scale once the window exists, so every 2D layout
              site (the front end's centring, the HUD's anchors, the managers, the subtitles, the letterbox height,
              the 3D-to-2D projections of selection and picking) keeps the exe's own arithmetic at the logical size.

    Created 2026-10-21 for Homeworld-Infection (M3-R1 ui-scale).
=============================================================================*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"

#include "Debug.h"
#include "glinc.h"
#include "Globals.h"
#include "main.h"
#include "mouse.h"
#include "uiscale.h"
#include "utility.h"
#include "Instrument.h"                                         // HWI-FEATURE(instrumentation): the settings line
#include "FEFlow.h"                                             // HWI-FEATURE(window-resize): feStackIndex (an open screen)
#include "render.h"                                             // HWI-FEATURE(window-resize): rndAspectRatio

/*=============================================================================
    Data:
=============================================================================*/
sdword uiScale        = UI_SCALE_AUTO;      // HWI-FEATURE(ui-scale): the setting (Cataclysm.cfg row, console row, Video control)
sdword uiScalePercent = 100;                // HWI-FEATURE(ui-scale): the scale in use

static bool32 uiSwitched      = FALSE;      // HWI-FEATURE(ui-scale): /uiScale stands for this run
static sdword uiSwitchFile    = 0;          // HWI-FEATURE(ui-scale): the options file's value under the switch
static sdword uiRunValue      = 0;          // HWI-FEATURE(ui-scale): the run's value while the file is written
static sdword uiSavedLogicalW = 0;          // HWI-FEATURE(ui-scale): the logical size while the file's rows are read / written
static sdword uiSavedLogicalH = 0;          // HWI-FEATURE(ui-scale)

static bool32 uiWindow        = FALSE;      // HWI-FEATURE(ui-scale): a window exists and MAIN_Window* hold its logical size
static bool32 uiRequestNew    = FALSE;      // HWI-FEATURE(ui-scale): MAIN_Window* hold a new request (a mode switch)
static sdword uiRequestW      = 0;          // HWI-FEATURE(ui-scale): the current window's request
static sdword uiRequestH      = 0;          // HWI-FEATURE(ui-scale)
static sdword uiLogicalW      = 0;          // HWI-FEATURE(ui-scale): the logical size set for it
static sdword uiLogicalH      = 0;          // HWI-FEATURE(ui-scale)
static sdword uiPointW        = 0;          // HWI-FEATURE(ui-scale): the window's size in SDL's event coordinates
static sdword uiPointH        = 0;          // HWI-FEATURE(ui-scale)

static bool32 uiTestBounds    = FALSE;      // HWI-TEST: /testUsableBounds
static SDL_Rect uiTestUsable;               // HWI-TEST
static SDL_Rect uiTestDisplay;              // HWI-TEST
static bool32 uiTestMouse     = FALSE;      // HWI-TEST: the self-test's mouse position (UiScaleTest.c)
static sdword uiTestMouseX    = 0;          // HWI-TEST
static sdword uiTestMouseY    = 0;          // HWI-TEST

/*-----------------------------------------------------------------------------
    The setting's surfaces. /uiScale N is run-only, as /noInterp (MotionInterp.c miSwitchOff) and /letterboxBehind:
    the start-up's opOptionsSaveSettings and every Accept write the options file's own value.
----------------------------------------------------------------------------*/
static sdword uiScaleClamp(sdword percent)                      // HWI-FEATURE(ui-scale)
{
    if (percent <= 0)
    {
        return UI_SCALE_AUTO;
    }
    return (percent < UI_SCALE_MIN) ? UI_SCALE_MIN : ((percent > UI_SCALE_MAX) ? UI_SCALE_MAX : percent);
}

void uiScaleSwitchSet(sdword percent)                           // HWI-FEATURE(ui-scale)
{
    if (!uiSwitched)
    {
        uiSwitchFile = uiScale;
        uiSwitched = TRUE;
    }
    uiScale = uiScaleClamp(percent);
}

void uiScaleSwitchEnd(void)                                     // HWI-FEATURE(ui-scale): a choice on the Video screen
{
    if (uiSwitched)
    {
        uiScale = uiSwitchFile;
        uiSwitched = FALSE;
    }
}

// the Video screens' Cancel (Options.c opOptionsSaveSettings / opRestoreSavedSettings): the value and the switch's state
static sdword uiSavedScale    = UI_SCALE_AUTO;                  // HWI-FEATURE(ui-scale)
static bool32 uiSavedSwitched = FALSE;                          // HWI-FEATURE(ui-scale)
void uiScaleStateSave(void)                                     // HWI-FEATURE(ui-scale)
{
    uiSavedScale = uiScale;
    uiSavedSwitched = uiSwitched;
}
void uiScaleStateRestore(void)                                  // HWI-FEATURE(ui-scale)
{
    uiScale = uiSavedScale;
    uiSwitched = uiSavedSwitched;
}

bool32 uiScaleSwitchStands(void)                                // HWI-FEATURE(ui-scale)
{
    return uiSwitched;
}

sdword uiScaleFileValue(void)                                   // HWI-FEATURE(ui-scale)
{
    return uiSwitched ? uiSwitchFile : uiScale;
}

// the window's rows of the options file (screenWidth / screenHeight point at MAIN_Window*): while the window exists
// they hold its logical size, so the file is read and written with the request in them (the read's request is not
// acted on until the next start, as the exe's registry rows)
void uiScaleWindowRows(bool32 begin)                            // HWI-FEATURE(ui-scale)
{
    if (!uiWindow)
    {
        return;
    }
    if (begin)
    {
        uiSavedLogicalW = MAIN_WindowWidth;
        uiSavedLogicalH = MAIN_WindowHeight;
        MAIN_WindowWidth  = uiRequestW;
        MAIN_WindowHeight = uiRequestH;
    }
    else
    {
        MAIN_WindowWidth  = uiSavedLogicalW;
        MAIN_WindowHeight = uiSavedLogicalH;
    }
}

void uiScaleFileValues(bool32 begin)                            // HWI-FEATURE(ui-scale): around utyOptionsFileWrite
{
    if (begin)
    {
        uiRunValue = uiScale;
        if (uiSwitched)
        {
            uiScale = uiSwitchFile;
        }
    }
    else
    {
        uiScale = uiRunValue;
    }
    uiScaleWindowRows(begin);
}

/*-----------------------------------------------------------------------------
    The scale for a physical size: the user's rule (2.0 at a height of 2160 or more, 1.5 at 1440 or more, 1.0 below),
    or the setting's percent; never so large that the logical size drops under the front end's 640x480.
----------------------------------------------------------------------------*/
static sdword uiScaleChoose(sdword physW, sdword physH, sdword setting)     // HWI-FEATURE(ui-scale)
{
    sdword percent, most;

    if (setting > 0)
    {
        percent = uiScaleClamp(setting);
    }
    else if (physH >= UI_SCALE_AUTO_4K_HEIGHT)
    {
        percent = 200;
    }
    else if (physH >= UI_SCALE_AUTO_2K_HEIGHT)
    {
        percent = 150;
    }
    else
    {
        percent = 100;
    }
    most = min(physW * 100 / 640, physH * 100 / 480);
    if (percent > most)
    {
        percent = most;
    }
    return (percent < 100) ? 100 : percent;
}

// the logical size: physical / scale, rounded to the nearest pixel (2560x1440 at 150%: 1706.67 -> 1707 x 960). The
// 2D projection then carries the exact per-axis factor physical / logical (1.49971 x 1.5), and the mouse uses the same
// factor, so drawing and picking agree to the physical pixel
static sdword uiScaleLogical(sdword phys, sdword percent)       // HWI-FEATURE(ui-scale)
{
    return (phys * 100 + percent / 2) / percent;
}

sdword uiScaleWanted(void)                                      // HWI-FEATURE(ui-scale)
{
    return uiScaleChoose(MAIN_WindowWidthActual, MAIN_WindowHeightActual, uiScale);
}

const char *uiScaleSource(void)                                 // HWI-FEATURE(ui-scale)
{
    return uiSwitched ? "switch" : ((uiScale > 0) ? "cfg" : "auto");
}

/*-----------------------------------------------------------------------------
    The window fit (HWI-FIX, the user's item 6). The exe's windowed mode (/window, fullScreen 0) makes a bordered
    window of the requested client size at the screen's origin (WinMain 0x005631f0); GoK's makes it at
    SDL_WINDOWPOS_UNDEFINED (render.c setupPixelFormat). A request as large as the desktop cannot be seen whole: on the
    user's 3840x2160 desktop (COSMIC, 150%) the title bar and the panel hide its bottom ~200 rows, which the pointer
    never reaches. The rule:
      a request of the display's size or larger (both sides)   -> SDL_WINDOW_FULLSCREEN_DESKTOP: borderless, the
                                                                 display's size (the whole display was asked for);
      larger than the usable bounds (the desktop less panels)  -> a bordered window shrunk to them keeping the aspect,
                                                                 then (render.c) shrunk by its borders and placed in them;
      else                                                     -> the request, as before.
    /fullscreen keeps SDL_WINDOW_FULLSCREEN_DESKTOP; /noBorder (no decoration) is fitted the same way. The harness's
    offscreen and dummy drivers have no desktop: no fit, unless /testUsableBounds forces the bounds.
----------------------------------------------------------------------------*/
static const char *uiFitRule = "as requested";                  // HWI-FIX
static SDL_Rect uiFitUsable;                                    // HWI-FIX
static SDL_Rect uiFitDisplay;                                   // HWI-FIX
static bool32 uiFitHave = FALSE;                                // HWI-FIX

static const char *uiScaleFitDecide(sdword reqW, sdword reqH, bool32 full, SDL_Rect *display, SDL_Rect *usable,
                                    bool32 have, udword *flags, sdword *w, sdword *h)    // HWI-FIX
{
    *w = reqW;
    *h = reqH;
    if (full)
    {
        return "fullscreen";                                    // SDL_WINDOW_FULLSCREEN_DESKTOP, GoK's
    }
    if (!have)
    {
        return "as requested (no desktop bounds)";
    }
    if (reqW >= display->w && reqH >= display->h)
    {
        *flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        *w = display->w;
        *h = display->h;
        return "display size -> fullscreen desktop";
    }
    if (reqW > usable->w || reqH > usable->h)
    {
        if ((sqword)reqW * usable->h > (sqword)reqH * usable->w)
        {
            *w = usable->w;
            *h = (sdword)((sqword)reqH * usable->w / reqW);
        }
        else
        {
            *h = usable->h;
            *w = (sdword)((sqword)reqW * usable->h / reqH);
        }
        return "larger than the usable bounds -> shrunk, aspect kept";
    }
    return "fits the usable bounds";
}

static void uiFlagsName(udword flags, char *buf, size_t size)   // HWI-FIX
{
    snprintf(buf, size, "%s%s", (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN_DESKTOP ?
             "fullscreen-desktop" : ((flags & SDL_WINDOW_FULLSCREEN) ? "fullscreen" : "window"),
             (flags & SDL_WINDOW_BORDERLESS) ? " borderless" : "");
}

// the display a request goes to: display 0 when it is as large as the request (both sides), else the smallest display
// that is, else the largest (the user's desktop has a 4K and a smaller monitor, and XWayland's primary, display 0, may be
// the smaller one: a 3840x2160 request belongs on the 4K)
sdword uiScaleFitDisplay(sdword n, const sdword *dw, const sdword *dh, sdword reqW, sdword reqH)   // HWI-FIX
{
    sdword i, best = -1, largest = 0;

    if (n <= 0)
    {
        return 0;
    }
    if (dw[0] >= reqW && dh[0] >= reqH)
    {
        return 0;
    }
    for (i = 0; i < n; i++)
    {
        if (dw[i] >= reqW && dh[i] >= reqH && (best < 0 || (sqword)dw[i] * dh[i] < (sqword)dw[best] * dh[best]))
        {
            best = i;
        }
        if ((sqword)dw[i] * dh[i] > (sqword)dw[largest] * dh[largest])
        {
            largest = i;
        }
    }
    return (best >= 0) ? best : largest;
}

static sdword uiFitDisplayIndex = 0;                            // HWI-FIX

void uiScaleWindowFit(sdword reqW, sdword reqH, bool32 full, bool32 border, udword *flags, sdword *w, sdword *h,
                      sdword *x, sdword *y)                     // HWI-FIX
{
    const char *driver = SDL_GetCurrentVideoDriver();

    uiFitHave = FALSE;
    uiFitDisplayIndex = 0;
    *x = *y = (sdword)SDL_WINDOWPOS_UNDEFINED;
    if (uiTestBounds)
    {
        uiFitDisplay = uiTestDisplay;
        uiFitUsable = uiTestUsable;
        uiFitHave = TRUE;
    }
    else if (driver != NULL && strcmp(driver, "offscreen") != 0 && strcmp(driver, "dummy") != 0)
    {
        sdword n = SDL_GetNumVideoDisplays(), i, dw[16], dh[16];
        SDL_Rect r;

        n = (n > 16) ? 16 : n;
        for (i = 0; i < n; i++)
        {
            dw[i] = dh[i] = 0;
            if (SDL_GetDisplayBounds(i, &r) == 0)
            {
                dw[i] = r.w;
                dh[i] = r.h;
            }
        }
        uiFitDisplayIndex = uiScaleFitDisplay(n, dw, dh, reqW, reqH);
        if (SDL_GetDisplayBounds(uiFitDisplayIndex, &uiFitDisplay) == 0 &&
            SDL_GetDisplayUsableBounds(uiFitDisplayIndex, &uiFitUsable) == 0)
        {
            uiFitHave = TRUE;
            *x = *y = (sdword)SDL_WINDOWPOS_UNDEFINED_DISPLAY(uiFitDisplayIndex);
        }
    }
    if (!border && !full)
    {
        *flags |= SDL_WINDOW_BORDERLESS;                        // CAT: /noBorder: the exe's WS_POPUP window (WinMain 0x005631f0) [audit: tag]
    }
    uiFitRule = uiScaleFitDecide(reqW, reqH, full, &uiFitDisplay, &uiFitUsable, uiFitHave, flags, w, h);
}

const char *uiScaleFitRule(void)                                // HWI-FEATURE(instrumentation): the settings line
{                                                               // HWI-FEATURE(instrumentation)
    return uiFitRule;                                           // HWI-FEATURE(instrumentation)
}                                                               // HWI-FEATURE(instrumentation)

// HWI-TEST: the rule for given bounds (UiScaleTest.c's table), logged
const char *uiScaleWindowFitCheck(sdword reqW, sdword reqH, bool32 full, bool32 border, sdword dispW, sdword dispH,
                                  sdword usableW, sdword usableH, udword *flags, sdword *w, sdword *h)   // HWI-TEST
{
    SDL_Rect display = { 0, 0, dispW, dispH }, usable = { 0, 0, usableW, usableH };
    const char *rule;
    char names[48];

    if (!border && !full)
    {
        *flags |= SDL_WINDOW_BORDERLESS;
    }
    rule = uiScaleFitDecide(reqW, reqH, full, &display, &usable, TRUE, flags, w, h);
    uiFlagsName(*flags, names, sizeof(names));
    dbgMessagef("window: fit requested %dx%d%s, usable %dx%d (display %dx%d) -> %dx%d (%s): %s", (int)reqW, (int)reqH,
                full ? " fullscreen" : (border ? " windowed" : " windowed no border"), (int)usableW, (int)usableH,
                (int)dispW, (int)dispH, (int)*w, (int)*h, names, rule);
    return rule;
}

// the shrunk window (the usable-bounds rule) less its decoration, placed inside the usable bounds: SDL reports the
// borders once the window manager has framed it (X11 _NET_FRAME_EXTENTS, Windows' frame); none known, it stays as made
void uiScaleWindowPlace(void *window)                           // HWI-FIX
{
    SDL_Window *sdlWindow = (SDL_Window *)window;
    int top = 0, left = 0, bottom = 0, right = 0, w = 0, h = 0, fitW, fitH;

    if (!uiFitHave || strncmp(uiFitRule, "larger", 6) != 0 || uiTestBounds ||
        SDL_GetWindowBordersSize(sdlWindow, &top, &left, &bottom, &right) != 0)
    {
        return;
    }
    SDL_GetWindowSize(sdlWindow, &w, &h);
    if (w <= 0 || h <= 0)
    {
        return;
    }
    fitW = w;
    fitH = h;
    if (fitW + left + right > uiFitUsable.w || fitH + top + bottom > uiFitUsable.h)
    {
        int roomW = uiFitUsable.w - left - right, roomH = uiFitUsable.h - top - bottom;

        if ((sqword)w * roomH > (sqword)h * roomW)
        {
            fitW = roomW;
            fitH = (int)((sqword)h * roomW / w);
        }
        else
        {
            fitH = roomH;
            fitW = (int)((sqword)w * roomH / h);
        }
        SDL_SetWindowSize(sdlWindow, fitW, fitH);
    }
    SDL_SetWindowPosition(sdlWindow, uiFitUsable.x + left + (uiFitUsable.w - left - right - fitW) / 2, uiFitUsable.y + top);
    printf("window: borders %d,%d,%d,%d: %dx%d placed in the usable bounds\n", top, left, bottom, right, fitW, fitH);
}

/*-----------------------------------------------------------------------------
    The window exists: the physical size (the drawable), the scale, the logical size. Called by render.c
    setupPixelFormat after SDL_GL_GetDrawableSize; once per created window.
----------------------------------------------------------------------------*/
static void uiScaleSet(sdword percent)                          // HWI-FEATURE(ui-scale)
{
    uiScalePercent = percent;
    MAIN_WindowWidth  = uiLogicalW = uiScaleLogical(MAIN_WindowWidthActual, percent);
    MAIN_WindowHeight = uiLogicalH = uiScaleLogical(MAIN_WindowHeightActual, percent);
    // glDrawPixels (the font's bitmap fallback, font.c fontPrintN) draws physical pixels: zoomed with the UI
    glPixelZoom((GLfloat)MAIN_WindowWidthActual / (GLfloat)MAIN_WindowWidth,
                (GLfloat)MAIN_WindowHeightActual / (GLfloat)MAIN_WindowHeight);
}

void uiScaleWindowCreated(void *window, sdword reqW, sdword reqH)   // HWI-FEATURE(ui-scale)
{
    SDL_Window *sdlWindow = (SDL_Window *)window;
    int pw = 0, ph = 0;
    udword flags = SDL_GetWindowFlags(sdlWindow);
    char names[48];
    bool32 remade = uiWindow;                   // [audit: M3-R1 ui-scale] a window before this one: a mode switch

    SDL_GetWindowSize(sdlWindow, &pw, &ph);
    uiPointW = (pw > 0) ? pw : MAIN_WindowWidthActual;
    uiPointH = (ph > 0) ? ph : MAIN_WindowHeightActual;
    uiRequestW = reqW;
    uiRequestH = reqH;
    uiRequestNew = FALSE;
    uiWindow = TRUE;
    uiScaleSet(uiScaleWanted());
    uiFlagsName(flags, names, sizeof(names));
    if (uiFitHave)
    {
        printf("window: requested %dx%d, usable %dx%d (display %d: %dx%d), created %dx%d (%s): %s\n", (int)reqW, (int)reqH,
               uiFitUsable.w, uiFitUsable.h, (int)uiFitDisplayIndex, uiFitDisplay.w, uiFitDisplay.h, (int)MAIN_WindowWidthActual,
               (int)MAIN_WindowHeightActual, names, uiFitRule);
    }
    else
    {
        printf("window: requested %dx%d, usable none, created %dx%d (%s): %s\n", (int)reqW, (int)reqH,
               (int)MAIN_WindowWidthActual, (int)MAIN_WindowHeightActual, names, uiFitRule);
    }
    printf("uiscale: physical %dx%d window %dx%d scale %d%% (%s, uiScale %d) logical %dx%d factor %.5f x %.5f\n",
           (int)MAIN_WindowWidthActual, (int)MAIN_WindowHeightActual, (int)uiPointW, (int)uiPointH, (int)uiScalePercent,
           uiScaleSource(), (int)uiScale, (int)MAIN_WindowWidth, (int)MAIN_WindowHeight,
           (double)MAIN_WindowWidthActual / (double)MAIN_WindowWidth, (double)MAIN_WindowHeightActual / (double)MAIN_WindowHeight);
    fflush(stdout);
    // [audit: M3-R1 ui-scale] a mode switch (the Video screen's resolution list: opOptionsAcceptHelper / mainRestoreRender
    //      -> mainLoadGL -> mainStartupGL) runs mainRescaleMainWindow BEFORE the new window exists, so the main region and
    //      every front-end screen were laid out at the REQUEST (utyChangeResolution(MAIN_Window*)); at a UI scale other
    //      than 100% the logical size differs: lay them out again at it (the first window has nothing laid out yet)
    if (remade && ghMainRegion != NULL &&
        (ghMainRegion->rect.x1 != MAIN_WindowWidth || ghMainRegion->rect.y1 != MAIN_WindowHeight))
    {
        (void)utyChangeResolution(MAIN_WindowWidth, MAIN_WindowHeight, MAIN_WindowDepth);
        printf("uiscale: relaid out at logical %dx%d after the mode switch\n", (int)MAIN_WindowWidth, (int)MAIN_WindowHeight);
        fflush(stdout);
    }
    hwiSettingsWindow();                                        // HWI-FEATURE(instrumentation): the settings line (start-up / mode switch)
}

bool32 uiScaleWindowCurrent(sdword *reqW, sdword *reqH)         // HWI-FEATURE(ui-scale)
{
    if (uiWindow && !uiRequestNew && MAIN_WindowWidth == uiLogicalW && MAIN_WindowHeight == uiLogicalH)
    {
        *reqW = uiRequestW;
        *reqH = uiRequestH;
        return TRUE;
    }
    return FALSE;
}

void uiScaleWindowRequest(sdword w, sdword h)                   // HWI-FEATURE(ui-scale): Options' and main.c's mode switches
{
    MAIN_WindowWidth  = w;
    MAIN_WindowHeight = h;
    uiRequestNew = uiWindow;
}

void uiScaleWindowRequestGet(sdword *w, sdword *h)              // HWI-FEATURE(ui-scale)
{
    if (uiWindow && !uiRequestNew)
    {
        *w = uiRequestW;
        *h = uiRequestH;
    }
    else
    {
        *w = MAIN_WindowWidth;
        *h = MAIN_WindowHeight;
    }
}

/*-----------------------------------------------------------------------------
    A changed setting applied to the window (the front end's Video screen Accept, Options.c, and the self-test): the
    logical size for the scale, then the reflow the exe runs after a resolution change (utyChangeResolution: the main
    region, every loaded front-end screen re-centred by feAllScreensReposition, the subtitle regions, the info
    overlay). In game the exe allows no render switch (opOptionsAcceptHelper returns while gameIsRunning): there the
    setting is saved and takes effect at the next start.
----------------------------------------------------------------------------*/
bool32 uiScaleApply(sdword percent)                             // HWI-FEATURE(ui-scale)
{
    sdword oldW = MAIN_WindowWidth, oldH = MAIN_WindowHeight;

    if (!uiWindow || uiRequestNew)
    {
        return FALSE;
    }
    percent = uiScaleChoose(MAIN_WindowWidthActual, MAIN_WindowHeightActual, percent);
    if (percent == uiScalePercent)
    {
        return FALSE;
    }
    uiScaleSet(percent);
    mouseCursorXPosition = mouseCursorXPosition * MAIN_WindowWidth / oldW;
    mouseCursorYPosition = mouseCursorYPosition * MAIN_WindowHeight / oldH;
    (void)utyChangeResolution(MAIN_WindowWidth, MAIN_WindowHeight, MAIN_WindowDepth);
    dbgMessagef("uiscale: applied scale %d%% (%s, uiScale %d) logical %dx%d (was %dx%d)", (int)uiScalePercent,
                uiScaleSource(), (int)uiScale, (int)MAIN_WindowWidth, (int)MAIN_WindowHeight, (int)oldW, (int)oldH);
    return TRUE;
}

void uiScaleUpdate(void)                                        // HWI-FEATURE(ui-scale): the front end's Accept
{
    if (!gameIsRunning)
    {
        (void)uiScaleApply(uiScale);
    }
}

/*-----------------------------------------------------------------------------
    HWI-FEATURE(window-resize) (M3-R4 window-modes): the user asked for "dynamic window sizing in case anyone wants a
    windowed format that the user can drag and expand the window to the size they want". The window is made
    SDL_WINDOW_RESIZABLE (render.c setupPixelFormat, a windowed mode only) and every SDL_WINDOWEVENT_SIZE_CHANGED
    (main.c HandleEvent) comes here: the physical size is the new drawable, the scale is chosen for it again (the auto
    rule by the height), the logical size follows (uiScaleSet), the 3D projection's aspect and the GL viewport are the
    drawable's (rndSmallInit's), the mouse keeps its place on the logical screen, and the exe's reflow after a
    resolution change runs (utyChangeResolution: the main region, every front-end screen re-centred, the subtitle
    regions). In the front end that reflow pops the open screens and restarts the main menu, as the Video screen's mode
    switch does (feAllScreensReposition); in a game it would pop a manager or the Escape menu past their own close
    paths, so there it waits until no screen is open (uiScaleResizeFlush, every pass of the main loop). The dragged
    size becomes the window's request (uiRequestW / H): Cataclysm.cfg's screenWidth / screenHeight rows (written at an
    Accept and at exit) open the next start at it, and the Video screen lists and selects it (Options.c opResListSet).
    cataclysm.exe's window has a fixed client size (WinMain 0x005631f0: no WS_THICKFRAME).
-----------------------------------------------------------------------------*/
static bool32 uiResizePending = FALSE;                          // HWI-FEATURE(window-resize): a relayout owed

static void uiScaleResizeRelayout(void)                         // HWI-FEATURE(window-resize)
{
    uiResizePending = FALSE;
    (void)utyChangeResolution(MAIN_WindowWidth, MAIN_WindowHeight, MAIN_WindowDepth);
}

static bool32 uiScaleResizeApply(sdword pointW, sdword pointH, sdword physW, sdword physH, bool32 request)   // HWI-FEATURE(window-resize)
{
    sdword oldW = MAIN_WindowWidth, oldH = MAIN_WindowHeight;
    bool32 deferred;

    if (!uiWindow || physW <= 0 || physH <= 0)
    {
        return FALSE;
    }
    if (physW == MAIN_WindowWidthActual && physH == MAIN_WindowHeightActual &&
        (pointW <= 0 || pointW == uiPointW) && (pointH <= 0 || pointH == uiPointH))
    {
        return FALSE;                                           // the size it has (the creation's event, a move)
    }
    MAIN_WindowWidthActual  = physW;
    MAIN_WindowHeightActual = physH;
    uiPointW = (pointW > 0) ? pointW : physW;
    uiPointH = (pointH > 0) ? pointH : physH;
    if (request)
    {
        uiRequestW = uiPointW;                                  // the dragged size is the next start's
        uiRequestH = uiPointH;
    }
    uiRequestNew = FALSE;
    uiScaleSet(uiScaleWanted());                                // the scale for the new height, the logical size
    rndAspectRatio = (real32)physW / (real32)physH;             // the 3D projection's aspect (rndSmallInit's rule)
    glViewport(0, 0, physW, physH);
    if (oldW > 0 && oldH > 0)
    {
        mouseCursorXPosition = mouseCursorXPosition * MAIN_WindowWidth / oldW;
        mouseCursorYPosition = mouseCursorYPosition * MAIN_WindowHeight / oldH;
    }
    deferred = gameIsRunning && feStackIndex > 0;
    uiResizePending = TRUE;
    if (!deferred)
    {
        uiScaleResizeRelayout();
    }
    dbgMessagef("window: resized to %dx%d (window %dx%d) scale %d%% (%s, uiScale %d) logical %dx%d (was %dx%d) aspect %.4f%s",
                (int)physW, (int)physH, (int)uiPointW, (int)uiPointH, (int)uiScalePercent, uiScaleSource(), (int)uiScale,
                (int)MAIN_WindowWidth, (int)MAIN_WindowHeight, (int)oldW, (int)oldH, (double)rndAspectRatio,
                deferred ? " (the relayout waits for the open screen)" : "");
    hwiSettingsWindow();                                        // HWI-FEATURE(instrumentation): the settings line again
    return TRUE;
}

void uiScaleResizeFlush(void)                                   // HWI-FEATURE(window-resize): the main loop, every pass
{
    if (uiResizePending && !(gameIsRunning && feStackIndex > 0))
    {
        uiScaleResizeRelayout();
        dbgMessagef("window: the owed relayout at logical %dx%d", (int)MAIN_WindowWidth, (int)MAIN_WindowHeight);
    }
}

void uiScaleWindowResized(void *window)                         // HWI-FEATURE(window-resize)
{
    SDL_Window *sdlWindow = (SDL_Window *)window;
    int pw = 0, ph = 0, dw = 0, dh = 0;

    if (sdlWindow == NULL)
    {
        return;
    }
    SDL_GetWindowSize(sdlWindow, &pw, &ph);
    SDL_GL_GetDrawableSize(sdlWindow, &dw, &dh);
    (void)uiScaleResizeApply(pw, ph, dw, dh, (SDL_GetWindowFlags(sdlWindow) & SDL_WINDOW_FULLSCREEN) == 0);
}

bool32 uiScaleWindowResizedTo(sdword pointW, sdword pointH, sdword physW, sdword physH)   // HWI-TEST
{
    return uiScaleResizeApply(pointW, pointH, physW, physH, TRUE);
}

void uiScaleWindowPointGet(sdword *w, sdword *h)                 // HWI-TEST
{
    *w = (uiPointW > 0) ? uiPointW : MAIN_WindowWidthActual;
    *h = (uiPointH > 0) ? uiPointH : MAIN_WindowHeightActual;
}

/*-----------------------------------------------------------------------------
    The conversions. SDL reports the mouse in window coordinates (the window's size, SDL_GetWindowSize), which are the
    drawable's pixels on X11 and Windows and points on a high-DPI Wayland or macOS window. A window pixel belongs to the
    logical pixel its centre falls in, as the rasteriser draws it: logical = floor((window + 0.5) * logical / window
    size), so the window's last row and column land on the logical last ones and the edge of a button is where it is
    drawn (2560x1440 at 150%: window x 1435 -> 957.16 -> 957, the column a 1707-wide button edge at 957 covers). At 100%
    it is the identity.
----------------------------------------------------------------------------*/
void uiScaleMouseToLogical(sdword *x, sdword *y)                // HWI-FEATURE(ui-scale)
{
    sdword pw = (uiPointW > 0) ? uiPointW : MAIN_WindowWidthActual;
    sdword ph = (uiPointH > 0) ? uiPointH : MAIN_WindowHeightActual;

    *x = (sdword)floor(((real64)*x + 0.5) * (real64)MAIN_WindowWidth / (real64)pw);
    *y = (sdword)floor(((real64)*y + 0.5) * (real64)MAIN_WindowHeight / (real64)ph);
}

void uiScaleLogicalToMouse(sdword *x, sdword *y)                // HWI-FEATURE(ui-scale): the logical pixel's centre
{
    sdword pw = (uiPointW > 0) ? uiPointW : MAIN_WindowWidthActual;
    sdword ph = (uiPointH > 0) ? uiPointH : MAIN_WindowHeightActual;

    *x = (sdword)floor(((real64)*x + 0.5) * (real64)pw / (real64)MAIN_WindowWidth);
    *y = (sdword)floor(((real64)*y + 0.5) * (real64)ph / (real64)MAIN_WindowHeight);
}

void uiScaleRelativeToLogical(sdword *dx, sdword *dy)           // HWI-FEATURE(ui-scale): the fraction carried to the next
{
    static real32 restX = 0.0f, restY = 0.0f;
    sdword pw = (uiPointW > 0) ? uiPointW : MAIN_WindowWidthActual;
    sdword ph = (uiPointH > 0) ? uiPointH : MAIN_WindowHeightActual;
    real32 x, y;

    if (pw == MAIN_WindowWidth && ph == MAIN_WindowHeight)
    {
        return;                                                 // 100%: SDL's motion as it is
    }
    x = (real32)*dx * (real32)MAIN_WindowWidth / (real32)pw + restX;
    y = (real32)*dy * (real32)MAIN_WindowHeight / (real32)ph + restY;
    *dx = (sdword)x;
    *dy = (sdword)y;
    restX = x - (real32)*dx;
    restY = y - (real32)*dy;
}

sdword uiScaleToPhysicalX(sdword x)                             // HWI-FEATURE(ui-scale)
{
    if (MAIN_WindowWidthActual == MAIN_WindowWidth || MAIN_WindowWidth <= 0 || MAIN_WindowWidthActual <= 0)
    {
        return x;
    }
    return (sdword)lrint((double)x * (double)MAIN_WindowWidthActual / (double)MAIN_WindowWidth);
}

sdword uiScaleToPhysicalY(sdword y)                             // HWI-FEATURE(ui-scale)
{
    if (MAIN_WindowHeightActual == MAIN_WindowHeight || MAIN_WindowHeight <= 0 || MAIN_WindowHeightActual <= 0)
    {
        return y;
    }
    return (sdword)lrint((double)y * (double)MAIN_WindowHeightActual / (double)MAIN_WindowHeight);
}

real32 uiScaleLineWidth(real32 width)                           // HWI-FEATURE(ui-scale)
{
    return (uiScalePercent == 100) ? width : width * (real32)uiScalePercent / 100.0f;
}

void uiScaleScissor(sdword x, sdword y, sdword width, sdword height)    // HWI-FEATURE(ui-scale)
{
    sdword x0 = uiScaleToPhysicalX(x), y0 = uiScaleToPhysicalY(y);

    glScissor(x0, y0, uiScaleToPhysicalX(x + width) - x0, uiScaleToPhysicalY(y + height) - y0);
}

/*-----------------------------------------------------------------------------
    HWI-TEST
----------------------------------------------------------------------------*/
// /testUsableBounds WxH[@DWxDH]: the usable bounds (and the display's, default the usable's) the fit uses for this
// run's window, as a desktop would report them (SDL_GetDisplayUsableBounds / SDL_GetDisplayBounds); for the harness,
// whose offscreen driver has no desktop
void uiScaleTestBoundsSet(char *string)                         // HWI-TEST
{
    int uw = 0, uh = 0, dw = 0, dh = 0;
    char *at = strchr(string, '@');

    if (sscanf(string, "%dx%d", &uw, &uh) != 2 || uw <= 0 || uh <= 0)
    {
        return;
    }
    if (at == NULL || sscanf(at + 1, "%dx%d", &dw, &dh) != 2 || dw <= 0 || dh <= 0)
    {
        dw = uw;
        dh = uh;
    }
    uiTestUsable.x = uiTestUsable.y = uiTestDisplay.x = uiTestDisplay.y = 0;
    uiTestUsable.w = uw;
    uiTestUsable.h = uh;
    uiTestDisplay.w = dw;
    uiTestDisplay.h = dh;
    uiTestBounds = TRUE;
}

void uiScaleTestMouseSet(bool32 on, sdword x, sdword y)         // HWI-TEST
{
    uiTestMouse = on;
    uiTestMouseX = x;
    uiTestMouseY = y;
}

bool32 uiScaleTestMouseGet(sdword *x, sdword *y)                // HWI-TEST
{
    if (uiTestMouse)
    {
        *x = uiTestMouseX;
        *y = uiTestMouseY;
    }
    return uiTestMouse;
}
