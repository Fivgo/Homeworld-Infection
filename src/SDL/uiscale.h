// =============================================================================
//  uiscale.h
//  - HWI-FEATURE(ui-scale) (M3-R1 producer 5, the user's play-test item 5): one
//    scale factor for everything drawn in 2D, and the window fit of windowed
//    mode (HWI-FIX, the user's item 6). Not in cataclysm.exe (docs/m3-r1-ui-scale.md).
// =============================================================================

#ifndef ___UISCALE_H
#define ___UISCALE_H

#include "Types.h"

/*=============================================================================
    The three sizes (no new size globals: GoK's split, src/SDL/main.c):
      the window REQUEST   the options file's screenWidth / screenHeight rows, the /640 .. /1600 switches and the
                           Video screen's resolution list: MAIN_WindowWidth / Height until the window exists
                           (uiScaleWindowRequestGet afterwards);
      PHYSICAL             MAIN_WindowWidthActual / HeightActual, the drawable (SDL_GL_GetDrawableSize): the GL
                           viewport, every glScissor box, every glReadPixels and the 3D projection's aspect;
      LOGICAL              MAIN_WindowWidth / Height once the window exists = physical / scale, rounded: every 2D
                           layout and draw (the 2D projection maps 0 .. MAIN_Window* onto the whole viewport), the
                           mouse, picking and the 3D-to-2D projections.
=============================================================================*/
#define UI_SCALE_AUTO           0       // HWI-FEATURE(ui-scale): uiScale 0 = by the window height
#define UI_SCALE_MIN            100     // HWI-FEATURE(ui-scale): percent
#define UI_SCALE_MAX            400     // HWI-FEATURE(ui-scale)
#define UI_SCALE_AUTO_4K_HEIGHT 2160    // HWI-FEATURE(ui-scale): the user's rule: 200% at a window height of 2160 or more
#define UI_SCALE_AUTO_2K_HEIGHT 1440    // HWI-FEATURE(ui-scale): 150% at 1440 or more, 100% below

extern sdword uiScale;                  // HWI-FEATURE(ui-scale): Cataclysm.cfg row and console row "uiScale" (0 auto, 100 .. 400)
extern sdword uiScalePercent;           // HWI-FEATURE(ui-scale): the scale in use (percent; 100 before the window exists)

// the setting's surfaces
void uiScaleSwitchSet(sdword percent);                  // HWI-FEATURE(ui-scale): /uiScale N, this run only
void uiScaleSwitchEnd(void);                            // HWI-FEATURE(ui-scale): a choice on the Video screen ends the switch
bool32 uiScaleSwitchStands(void);                       // HWI-FEATURE(ui-scale)
void uiScaleStateSave(void);                            // HWI-FEATURE(ui-scale): the Video screens' Cancel copy
void uiScaleStateRestore(void);                         // HWI-FEATURE(ui-scale)
sdword uiScaleFileValue(void);                          // HWI-FEATURE(ui-scale): the value the options file keeps
void uiScaleFileValues(bool32 begin);                   // HWI-FEATURE(ui-scale): around utyOptionsFileWrite
void uiScaleWindowRows(bool32 begin);                   // HWI-FEATURE(ui-scale): screenWidth / Height = the request (a read)
sdword uiScaleWanted(void);                             // HWI-FEATURE(ui-scale): the percent the setting asks for this window
const char *uiScaleFitRule(void);                       // HWI-FEATURE(instrumentation): the window fit's choice (the settings line)
const char *uiScaleSource(void);                        // HWI-FEATURE(ui-scale): "auto" | "cfg" | "switch"

// the window (render.c setupPixelFormat)
void uiScaleWindowFit(sdword reqW, sdword reqH, bool32 full, bool32 border, udword *flags, sdword *w, sdword *h,
                      sdword *x, sdword *y);                          // HWI-FIX: the size, flags and display position made
sdword uiScaleFitDisplay(sdword n, const sdword *dw, const sdword *dh, sdword reqW, sdword reqH);   // HWI-FIX: which display
void uiScaleWindowPlace(void *window);                               // HWI-FIX: a shrunk window less its borders
void uiScaleWindowCreated(void *window, sdword reqW, sdword reqH);   // HWI-FEATURE(ui-scale): physical, scale, logical
bool32 uiScaleWindowCurrent(sdword *reqW, sdword *reqH);             // HWI-FEATURE(ui-scale): MAIN_Window* = this window's logical size
void uiScaleWindowRequest(sdword w, sdword h);                       // HWI-FEATURE(ui-scale): a new request into MAIN_Window*
void uiScaleWindowRequestGet(sdword *w, sdword *h);                  // HWI-FEATURE(ui-scale): the current window's request
void uiScaleUpdate(void);                                            // HWI-FEATURE(ui-scale): a front-end Accept: a changed setting applied [audit: was "per frame"]
bool32 uiScaleApply(sdword percent);                                 // HWI-FEATURE(ui-scale): the logical size for a scale, relaid out

// the conversions
void uiScaleMouseToLogical(sdword *x, sdword *y);       // HWI-FEATURE(ui-scale): window coordinates (SDL's) -> logical
void uiScaleLogicalToMouse(sdword *x, sdword *y);       // HWI-FEATURE(ui-scale): logical -> window coordinates (a warp)
void uiScaleRelativeToLogical(sdword *dx, sdword *dy);  // HWI-FEATURE(ui-scale): a relative motion -> logical units (rounded)
sdword uiScaleToPhysicalX(sdword x);                    // HWI-FEATURE(ui-scale): a logical x / width -> physical pixels
sdword uiScaleToPhysicalY(sdword y);                    // HWI-FEATURE(ui-scale)
real32 uiScaleLineWidth(real32 width);                  // HWI-FEATURE(ui-scale): a 2D line width in physical pixels
void uiScaleScissor(sdword x, sdword y, sdword width, sdword height);   // HWI-FEATURE(ui-scale): glScissor of a logical box (GL's y up)

// HWI-TEST: the fit's check (/testUsableBounds WxH[@DWxDH]) and the self-test's mouse position (UiScaleTest.c)
void uiScaleTestBoundsSet(char *string);                // HWI-TEST
void uiScaleTestMouseSet(bool32 on, sdword x, sdword y);// HWI-TEST: mousePoll reads this window position instead of SDL's
bool32 uiScaleTestMouseGet(sdword *x, sdword *y);       // HWI-TEST
const char *uiScaleWindowFitCheck(sdword reqW, sdword reqH, bool32 full, bool32 border, sdword dispW, sdword dispH,
                                  sdword usableW, sdword usableH, udword *flags, sdword *w, sdword *h);   // HWI-TEST: the rule, logged

#endif
