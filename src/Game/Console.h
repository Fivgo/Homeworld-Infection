// =============================================================================
//  Console.h
//  - Cataclysm's developer console: a text line that sets the variables of a
//    command table and runs its commands, its output lines, its history and
//    its binary config file (cataclysm.exe 0x0045a260-0x004601d0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___CONSOLE_H
#define ___CONSOLE_H

#include "FEFlow.h"
#include "Types.h"

// CAT: new file (M2-R6 startup-keys). The exe links the console right after ConsMgr.c (cat_cmIsOpen 0x0045a250), its
//      config file at 0x0045faf0 .. 0x004601d0 and three commands next to the key bindings (0x0054d080 .. 0x0054d1c0).

#define CAT_CON_LINE_LENGTH         0x4D        // every line buffer: 77 bytes (a line is entered at 0x4B characters)
#define CAT_CON_OUTPUT_LINES        28          // 0x70 bytes of line pointers
#define CAT_CON_INPUT_LINES         8           // the history: 0x20 bytes of line pointers

// the console's command table (the exe's cat_consoleCommandTable 0x008783c8, 0x14 bytes a row, ended by type -1)
typedef void (*CatConsoleCallback)(sdword type, void *data, void *value, char **reply);
typedef struct
{
    sdword type;                // +0x00: 0 bool (a byte), 1 / 2 byte, 3 / 4 word, 5 / 6 dword, 7 float, 8 string, 9 string -> word
    sdword id;                  // +0x04: the config file's key
    void *data;                 // +0x08: the variable
    char *name;                 // +0x0C
    CatConsoleCallback callback;    // +0x10: instead of the store when set
} CatConsoleCommand;

extern CatConsoleCommand catConsoleCommandTable[];      // 0x008783c8
extern bool8 catConsoleOpenFlag;                        // 0x00a2dcd4: the console is open
extern bool8 catMotionCues;                             // 0x008b8844: the camera's motion cues (console "cameracues")

// the console's variables that only it sets in the exe (no reader there either), and those other files read
extern real32 catConsoleMorphStartScale;                // 0x0087813c "morphStartScale": the mimic's radius scale (Mimic.c)
extern sdword catConsoleRammingTime;                    // 0x00878144 "rammingtime" (RammingFrigateInit)
extern real32 catConsoleRammingAngle;                   // 0x00878148 "rammingangle" (RammingFrigateHouseKeep)

#define cat_consoleIsOpen()         (catConsoleOpenFlag != 0)

void cat_consoleStartup(void);                          // 0x0045a260
void cat_consoleShutdown(void);                         // 0x0045a3d0
void cat_consoleOpen(void);                             // 0x0045a460
void cat_consoleClose(void);                            // 0x0045a4d0
void cat_consoleAddChar(ubyte c);                       // 0x0045a520
void cat_consoleOutputScroll(void);                     // 0x0045a560
void cat_consoleExecuteCommand(char *line);             // 0x0045a590
void cat_consoleKeyPress(udword vk);                    // 0x0045a700 (a virtual-key code)
void cat_conTextDraw(featom *atom, regionhandle region);    // 0x0045b1c0
void cat_consoleTabComplete(char *prefix, char **completion, sdword *index);  // 0x0045b420
sdword cat_consoleRunCommand(char *name, char *argument, char **reply, char *line);   // 0x0045b520
sdword cat_consoleShowVariable(char *name, char **reply, char *line);                // 0x0045f110
sdword cat_consoleKasCommand(char **reply, char *line);                             // 0x0045f270
void cat_consoleConfigFileLoad(char *name);             // 0x0045faf0
void cat_consoleConfigFileSave(char *name);             // 0x0045fec0
void cat_consoleConfigStartup(void);                    // 0x004601a0
void cat_consoleConfigShutdown(void);                   // 0x004601c0

// HWI-TEST (M2-R6 startup-keys), port diagnostic: /catConsoleTest <ticks> (harness mode console)
void catConsoleSelfTestSet(char *string);
bool32 catConsoleSelfTestRequested(void);
void catConsoleSelfTestTick(void);                      // after each universe tick (OracleDump.c oracleUnivUpdate)

#endif
