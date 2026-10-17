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

typedef enum
{
    kbNEXT_FORMATION=0,
    kbBUILD_MANAGER,
    kbPREVIOUS_FOCUS,
    kbNEXT_FOCUS,
    kbDOCK,
    kbSELECT_ALL_VISIBLE,
    kbFOCUS,
    kbRESEARCH_MANAGER,
    kbHARVEST,
    kbMOVE,
    kbNEXT_TACTIC,
    kbPREVIOUS_TACTIC,
    kbSCUTTLE,
    kbSHIP_SPECIAL,
    kbTACTICAL_OVERLAY,
//    kbSENSORS_MANAGER,
    kbMOTHERSHIP,
    kbKAMIKAZE,
    kbCANCEL_ORDERS,
    kbLAUNCH_MANAGER,
    kbTOTAL_COMMANDS
} CommandsToBind;

#define KB_NormalBound      0
#define KB_NotBound         1
#define KB_GetKeyPress      2

typedef struct kbBoundKeys
{
    udword      command;
    udword      defaultkey;
    udword      primarykey;
    udword      resettokey;
    udword      status;
} kbBoundKeys;


/*=============================================================================
    External Variables :
=============================================================================*/

extern fonthandle kbKeyListFont;

extern udword kbKeySavedKeys[20];

/*=============================================================================
    Function Prototypes :
=============================================================================*/

void kbListWindowCB(char *string, featom *atom);
void kbKeyResetToDefault(char *string, featom *atom);

void kbPoolListWindowCB(char *string, featom *atom);

sdword kbCheckBindings(sdword keypressed);

void kbRestoreSavedSettings(void);
void kbSaveSettings(void);

udword kbKeyBoundToCommand(udword command);
bool32 kbCommandKeyIsHit(udword command);

void kbInitKeyBindings(void);

// CAT (M2-R6 mainrgn-popup): Cataclysm's key commands (KeyBindings.c's end)
void cat_kbCommandExecute(udword state, sdword command);                        // 0x0054eb10
void cat_kbCommandKeyString(sdword command, char *out);                         // 0x0054e960
bool32 cat_kbCommandKeyMatch(udword state, sdword command, udword key, bool32 execute);   // 0x0054ebc0
void cat_kbLeechCB(udword state);                                               // 0x0054eff0
void cat_kbMimicShipsCB(udword state);                                          // 0x0054f0d0
void cat_kbMimicPlayersCB(udword state);                                        // 0x0054f130

#endif
