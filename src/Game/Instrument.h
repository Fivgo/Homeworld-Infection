// =============================================================================
//  Instrument.h
//  - HWI-FEATURE(instrumentation) (M3-R2 producer 1): the settings line ("hwi-settings:" in the log at start-up and
//    at every change of an HWI setting), the on-screen overlay (frame time, universe tick cost, interpolation alpha,
//    the flags, the vertices drawn, FULL DETAIL and the UI scale), and HWI-TEST /frameLog <file> (one line a second:
//    frame-time percentiles, frames over the refresh interval, universe ticks and their cost, the main heap, the
//    vertices drawn). Not in cataclysm.exe (docs/m3-r2-instrumentation.md). Nothing here draws from a random stream
//    or writes a simulation field; the overlay is off by default.
// =============================================================================

#ifndef ___INSTRUMENT_H
#define ___INSTRUMENT_H

#include "Types.h"

// the overlay's key: the keyboard's Menu (Application) key, SDL_SCANCODE_APPLICATION. No default binding of
//      cataclysm.exe uses it (default_keys.cfg binds nothing to VK_APPS 0x5d), and this tree's Key.c catKeyVkTable has
//      no virtual-key code for it, so this engine's keys.cfg cannot bind it and its key-binding screen refuses it
//      [audit: said "not a key of cataclysm.exe's tables": the exe's WindowProc 0x00561f10 passes VK_APPS to keyPressDown
//      like any key, and its cat_kbKeyToString 0x0049b590 names it "]" (the '%c' fallback), so the exe's binding screen
//      would take it], docs/m3-r2-instrumentation.md "Why the Menu key"
#define HWI_OVERLAY_KEY         SDL_SCANCODE_APPLICATION        // HWI-FEATURE(instrumentation)
// M3-R3 settings-and-keys (the user's decision of 2026-10-24: "let's dedicate a key from the F1-F12 suite"): F1, the one
//      F key cataclysm.exe leaves unbound in the main game view: default_keys.cfg (v 84) binds plain F2..F12 (Tactics_*,
//      Formation_*; F12 = Formation_Parade) and F1 only with Shift / Ctrl / Shift+Ctrl; the exe's WindowProc 0x00561f10
//      special-cases only F11 / F12 key-ups; mrStartup 0x00563b50 gives every key to the bindings; no .fib atom has an F
//      key as its hot key; the construction manager's detour (cmBuildHotKey 0x00458d40) takes F1 but its table builds
//      nothing on it. F1 toggles the overlay only while it is free (KeyBindings.c hwiKbOverlayFKeyFree: no modifier
//      held, no keys.cfg binding of plain F1, the key-binding screen not waiting for a key), so it never takes a binding
//      the user gives F1; the key still goes on to the game as before (docs/m3-r3-settings-and-keys.md)
#define HWI_OVERLAY_FKEY        SDL_SCANCODE_F1                 // HWI-FEATURE(instrumentation)
#define HWI_OVERLAY_FKEY_VK     0x70                            // HWI-FEATURE(instrumentation): the exe's code for F1 (VK_F1)

extern sdword hwiOverlay;               // HWI-FEATURE(instrumentation): 0 | 1, the console row "overlay" (default 0)

// the settings line
void hwiSettingsRowsRead(const char *directory);   // HWI-FEATURE(instrumentation): utyOptionsFileRead: which rows the file has
void hwiSettingsWindow(void);                      // HWI-FEATURE(instrumentation): a window exists (uiscale.c): the line
void hwiSettingsConsole(const char *row);          // HWI-FEATURE(instrumentation): a console row was set (Console.c)
void hwiSettingsAccepted(void);                    // HWI-FEATURE(instrumentation): the Options screen's Accept (Options.c)
void hwiSettingsChanged(const char *why);          // HWI-FEATURE(instrumentation): the line again if anything changed
void hwiSettingsPreset(const char *row, const char *why);  // HWI-FEATURE(sharp-textures): a preset changed the row (source "preset")
const char *hwiSettingsLast(void);                 // HWI-FEATURE(instrumentation): the last line printed (the self-tests)
sdword hwiSettingsLines(void);                     // HWI-FEATURE(instrumentation): lines printed so far

// the overlay's surfaces
void hwiOverlaySwitch(void);                       // HWI-FEATURE(instrumentation): /overlay (this run only)
void hwiOverlayKey(void);                          // HWI-FEATURE(instrumentation): the key (main.c HandleEvent)
void hwiOverlayDraw(void);                         // HWI-FEATURE(instrumentation): rndRenderTask, after the cursor

// the frame's clocks (rndRenderTask, Task.c) and counts (render.c)
void hwiFrameBegin(void);                          // HWI-FEATURE(instrumentation): the render task's top
void hwiFrameEnd(void);                            // HWI-FEATURE(instrumentation): after the flip
void hwiUniverseCallBegin(void);                   // HWI-FEATURE(instrumentation): a universe task call (one tick)
void hwiUniverseCallEnd(void);                     // HWI-FEATURE(instrumentation)
bool32 hwiWantVerts(void);                         // HWI-FEATURE(instrumentation): the overlay or /frameLog needs the count
void hwiVertsDrawn(udword objects, udword verts);  // HWI-FEATURE(instrumentation): the main view's levels drawn

// HWI-TEST: /frameLog <file>
void hwiFrameLogSet(char *file);                   // HWI-TEST
void hwiFrameLogClose(void);                       // HWI-TEST: the quit
const char *hwiBuildVersion(void);                 // HWI-TEST: main.c: the build version (the frame log's header) [audit]

// HWI-TEST (M3-R3 settings-and-keys): scripted inputs through the engine's own paths (no OS input), for the captures
void hwiOverlayKeyAtSet(char *spec);               // HWI-TEST: /catOverlayKeyAt <tick>[,<tick>..]: F1 as an SDL_KEYDOWN / UP to HandleEvent
void hwiConsoleAtSet(char *spec);                  // HWI-TEST: /catConsoleAt <tick>:<row>=<value>[,..]: a console row (cat_consoleRunCommand)
void hwiFeScreenSet(char *spec);                   // HWI-TEST: /catFeScreen <screen>[@<tick>]: a front-end screen pushed (feScreenStart)
void hwiFeClickSet(char *spec);                    // HWI-TEST: /catFeClick <atom>[@<tick>]: a click on the top screen's atom (its callback)

#endif
