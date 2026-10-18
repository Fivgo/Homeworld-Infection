/*=============================================================================
    Name    : KeyBindings.h
    Purpose : This file contains all of the defines and headers for the Key
              bindings system.

    Created 8/04/1999 by Drew Dunlop
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___KEYBINDINGS_H
#define ___KEYBINDINGS_H

#include "FEFlow.h"
#include "font.h"

/*=============================================================================
    Defines :
=============================================================================*/


/*=============================================================================
    Structure Prototypes :
=============================================================================*/

// CAT (M2-R6 startup-keys): Cataclysm's key bindings (KeyBindings.c's end). HW1's 19-command table (kbBoundKeys,
//      CommandsToBind, the per-language default tables, the key pool) is gone: the exe binds its 150 key commands from
//      default_keys.cfg / keys.cfg.
#define CAT_KB_NUMBER_COMMANDS      0x97        // commands 1 .. 0x96 (the exe's tables have a row 0)
#define CAT_KB_MAX_BINDINGS         3           // cat_kbKeySetCB 0x0054d460: "To many keys assigned"

// one binding: the exe's 16-byte record (cat_kbKeySetCB stores {key, shift, alt, ctrl})
typedef struct CatKbBinding
{
    udword key;                 // +0x0 the virtual-key code (its low byte; the exe leaves the other bytes unset)
    bool32 shift;               // +0x4
    bool32 alt;                 // +0x8
    bool32 ctrl;                // +0xC
} CatKbBinding;

// a command's bindings: the exe's 0x34-byte row of its table 0x00a6ee90 (cat_kbBindingGet copies 13 dwords)
typedef struct CatKbCommandBindings
{
    sdword count;               // +0x00
    CatKbBinding binding[CAT_KB_MAX_BINDINGS];  // +0x04
} CatKbCommandBindings;

// the records hold only 32-bit words, so the ILP32 pins (tools/m2_layout32.sh) and the LP64 build agree
_Static_assert(sizeof(CatKbBinding) == 0x10, "a key binding is 16 bytes in cataclysm.exe");
_Static_assert(sizeof(CatKbCommandBindings) == 0x34, "a command's key bindings are 0x34 bytes in cataclysm.exe");

// the key commands the other files name (the exe's rows of 0x008b5e60, in default_keys.cfg's order)
#define CAT_KB_Move                 0x01
#define CAT_KB_ShipSpecial          0x07
#define CAT_KB_Focus                0x3B
#define CAT_KB_PanSensorsLeft       0x41
#define CAT_KB_PanSensorsRight      0x42
#define CAT_KB_PanSensorsUp         0x43
#define CAT_KB_PanSensorsDown       0x44
#define CAT_KB_ZoomIn               0x45
#define CAT_KB_ZoomOut              0x46
#define CAT_KB_BandboxGuard         0x4A
#define CAT_KB_BandboxAttack        0x4B
#define CAT_KB_BandboxForceAttack   0x4C
#define CAT_KB_BandboxFocus         0x4D
#define CAT_KB_MultipleSelect       0x4E
#define CAT_KB_ToggleSensorsManager 0x4F
#define CAT_KB_Escape               0x5A
#define CAT_KB_NextMusicTrack       0x60
#define CAT_KB_PrevMusicTrack       0x61
#define CAT_KB_ToggleOverlayOptions 0x62
#define CAT_KB_ShowKeys1            0x64
#define CAT_KB_ShowKeys2            0x65
#define CAT_KB_ShowKeys3            0x66
#define CAT_KB_SkipSensors          0x6B

/*=============================================================================
    External Variables :
=============================================================================*/

extern fonthandle kbKeyListFont;
extern CatKbCommandBindings catKbBindings[CAT_KB_NUMBER_COMMANDS];     // 0x00a6ee90
extern bool32 catKbBindingsLoaded;                                      // 0x00a70d3c
extern sdword catKbDebugKeyGroups[CAT_KB_NUMBER_COMMANDS];              // 0x008b67c8

/*=============================================================================
    Function Prototypes :
=============================================================================*/

void kbListWindowCB(char *string, featom *atom);                        // 0x0049bca0
void kbKeyResetToDefault(char *string, featom *atom);                   // 0x0049be80
void kbInitKeyBindings(void);                                           // 0x0049bea0
bool32 kbCommandKeyIsHit(udword command);                               // 0x0054ec60

// CAT: Cataclysm's key bindings. HWI: a 'key' argument is this tree's key index (an SDL scancode, as the regions
//      and the popup menu pass it); a binding keeps the exe's virtual-key code (Key.c catKeyVkFromKey converts)
bool32 cat_kbKeyCharParse(char *string, char *key);                     // 0x0049b570
char *cat_kbKeyToString(udword vk);                                     // 0x0049b590 (a virtual-key code)
bool32 cat_kbCommandsCanShareKey(sdword command1, sdword command2, bool32 swapped);  // 0x0054d6a0
void cat_kbBindingToString(CatKbBinding *binding, char *out, bool32 useKeyName);    // 0x0054d750
sdword cat_kbCheckBindingConflict(CatKbBinding *binding, sdword command, sdword unused, char *message);  // 0x0054d880
void cat_kbLoadKeyBindings(void);                                       // 0x0054da10
void cat_kbReloadKeyBindings(bool32 resetToDefaults);                   // 0x0054de10
void cat_kbSaveKeyBindings(void);                                       // 0x0054e2d0
void cat_kbSetNewKey(sdword command, udword vk);                        // 0x0054e410 (a virtual-key code)
sdword cat_kbCommandsForKey(udword vk, sdword *commands);               // 0x0054e680 (a virtual-key code)
void cat_kbKeyPress(udword state, udword key);                          // 0x0054e7c0
void cat_kbKeyRelease(udword state, udword key);                        // 0x0054e8c0
void cat_kbBindingGet(sdword command, CatKbCommandBindings *bindings);  // 0x0054e910
char *cat_kbCommandNameGet(sdword command);                             // 0x0054e940
void cat_kbCommandExecute(udword state, sdword command);                // 0x0054eb10
void cat_kbCommandKeyString(sdword command, char *out);                 // 0x0054e960
bool32 cat_kbModifiersMatch(CatKbBinding *binding);                     // 0x0054eb40
bool32 cat_kbCommandKeyMatch(udword state, sdword command, udword key, bool32 execute);   // 0x0054ebc0
void cat_kbLeechCB(udword state);                                       // 0x0054eff0
void cat_kbMimicShipsCB(udword state);                                  // 0x0054f0d0
void cat_kbMimicPlayersCB(udword state);                                // 0x0054f130
void cat_kbSelectHotKeyGroupX(udword state, sdword group);              // 0x0054fe30
void cat_kbAddHotKeyGroupX(udword state, sdword group);                 // 0x00550060
sdword cat_moDebugKeyGet(sdword command);                               // 0x0054d3c0
char *cat_moDebugKeyValue(sdword group);                                // 0x0054d3d0

// HWI-TEST (M2-R6 startup-keys), port diagnostic: /catKeysTest (harness mode keys)
extern bool32 catKeysSelfTest;
void catKeysRunSelfTest(void);

#endif
