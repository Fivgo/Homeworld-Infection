/*=============================================================================
    Name    : launchMgr.h
    Purpose : Definitons for the launch manager

    Created 5/18/1998 by ddunlop
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/


#ifndef ___LAUNCHMGR_H
#define ___LAUNCHMGR_H

#include "Region.h"
#include "SpaceObj.h"

/*=============================================================================
    Definitions:
=============================================================================*/

#ifdef _WIN32
#define LM_FIBFile              "FEMAN\\Launch_Manager.fib"
#else
#define LM_FIBFile              "FEMAN/Launch_Manager.fib"
#endif
#define LM_LaunchScreen         "Launch_Manager"

#define LM_ShipListTextColor        colRGB( 30, 160, 190)
#define LM_ShipSelectedTextColor    colRGB(255, 255, 105)
#define LM_Ship2ListTextColor        colRGB(  0, 100, 140)
#define LM_Ship2SelectedTextColor    colRGB(255, 220, 105)
#define LM_UsedColor                colRGB(200,  50,  50);
#define LM_AvailableColor           colRGB(50,  200,  50);

#define LM_DefaultFont          "default.hff"
#define LM_ShipListFont         "HW_EuroseCond_11.hff"
#define LM_FontNameLength       64


//ships available for launch
typedef struct
{
    sword  nShips;
    bool16 bSelected;
    Ship   *ship;
}
LaunchShipsAvailable;

extern bool32 lmActive;

// CAT: the launch manager's seven module slots (the 0x00a4478c array): the sMothership's attached modules, one per
//      slot, filled by cat_lmFillInModuleSlots 0x0049e9f0 when the screen opens and drawn by cat_lmModuleSlot1..7Draw
#define CAT_LM_NUM_MODULE_SLOTS 7
extern Ship *cat_lmModuleSlots[CAT_LM_NUM_MODULE_SLOTS];
void cat_lmModuleSlotsRemoveType(ShipType type);    // CAT: 0x0049e970


/*=============================================================================
    Function Prototypes:
=============================================================================*/

//start the launch manager.  It will kill itself when you hit the launch button.
sdword lmLaunchBegin(regionhandle region, smemsize ID, udword event, udword data);
void   lmUpdateShipsInside(void);
void   lmStartup(void);
void   lmShutdown(void);
void   lmCloseIfOpen(void);
void   lmFreeTextures(void);
void   lmLoadTextures(void);
void   lmLaunchCapableShipDied (Ship *ship);

#endif
