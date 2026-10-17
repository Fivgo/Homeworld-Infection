/*=============================================================================
    Name    : InfoOverlay.h
    Purpose : this file has the definitions for the information overlay.

    Created 6/8/1998 by ddunlop
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___INFOOVERLAY_H
#define ___INFOOVERLAY_H

#include "Region.h"
#include "SpaceObj.h"

/*=============================================================================
    Defines:
=============================================================================*/

#define IO_ListTextColor        colRGB(0  , 100, 160)
#define IO_SelectedTextColor    colRGB(255, 255, 255)

/*=============================================================================
    Data Structures:
=============================================================================*/

typedef struct
{
    sword           nShips;
    bool16          bSelected;
    sdword          shipnum;
    ShipStaticInfo *statinfo;
} ShipListInfo;


typedef struct
{
    regionhandle  region;
    bool32          inlist;
    ShipListInfo *listinfo;
} ShipOverlayInfo;

extern bool32 ioRunning;


/*=============================================================================
    Function Prototypes:
=============================================================================*/

void ioSetSelection(bool32 shiftrelease);
void ioUpdateShipTotals(void);

void ioEnable(void);
bool32 ioDisable(void);

void ioStartup(void);
void ioShutdown(void);
void ioResolutionChange(void);

// CAT (M2-R5 hud): the main overlay (cat_mo*) and the info overlay's Cataclysm parts (cat_io*)
bool8 cat_moAltPrintOptionsActive(void);                                                    // 0x004aa080
void cat_ioReset(void);                                                                     // 0x004ab590
void cat_ioSubtitleNotify(sdword value);                                                    // 0x004ab620
void cat_ioRemoveShipReferences(Ship *ship);                                                // 0x004ab800
bool32 cat_ioClick(bool32 select);                                                          // 0x004ab840
void cat_ioUpdateHover(void);                                                               // 0x004ac050
void cat_ioObjectiveAdd(void *objective);                                                   // 0x004ac5e0
void cat_ioObjectiveAddFirst(void *objective);                                              // 0x004ac600
void cat_ioObjectiveRemove(void *objective);                                                // 0x004ac650
void cat_ioObjectivesClear(void);                                                           // 0x004ac6a0
void cat_ioMessageScroll(void);                                                             // 0x004ac6b0
void cat_ioMessageAdd(char *text, udword colour, udword type, void *data);                  // 0x004ac940
void cat_ioCommandMessage(char *text, udword colour);                                       // clCommandMessage 0x004ac820's body
void cat_ioCycleMode(bool32 forward);                                                       // 0x004acb40
void cat_missionTimerReset(void);                                                           // 0x004acb80
void cat_missionTimerSave(void);                                                            // 0x004acbb0
void cat_missionTimerLoad(void);                                                            // 0x004acbc0
void cat_moSelectedDraw(void);                                                              // 0x004a6bb0
void cat_moSelectingDraw(void);                                                             // 0x004a6ff0
void cat_moObjectivesDraw(void);                                                            // 0x004a76b0
sdword cat_moClickedFlagConsume(void);                                                      // 0x004a7980
sdword cat_moMouseOverObjective(void);                                                      // 0x004a7990
void cat_moTooltipDraw(void);                                                               // 0x004a79d0
void cat_moSupportUnitsDraw(void);                                                          // 0x004a7fe0
void cat_moObjectivesFrameDraw(void);                                                       // 0x004a83b0
void cat_moGameMessagesDraw(void);                                                          // 0x004a8650
sdword cat_moPlayerNamesDraw(sdword top);                                                   // 0x004a8b20
void cat_moConstructionDraw(sdword top, sdword bottom);                                     // 0x004a91c0
void cat_moResearchDraw(sdword top, sdword bottom);                                         // 0x004a9a00
void cat_moDebugKeysDraw(void);                                                             // 0x004aa2a0
void cat_moPrintOptionToggle(char key);                                                     // 0x004aa4f0
void cat_moPrintOptionsDraw(void);                                                          // 0x004a5e40
void cat_moButtonsDraw(void);                                                               // 0x004aac70
sdword cat_moButton1Clicked(void);                                                          // 0x004aaf70
sdword cat_moButton2Clicked(void);                                                          // 0x004aaf80
sdword cat_moButton3Clicked(void);                                                          // 0x004aaf90
sdword cat_moButton4Clicked(void);                                                          // 0x004aafa0
void cat_moRender(void);                                                                    // 0x004aafb0

// CAT: port diagnostic, not in the exe: the HUD self-test (/catHudTest <frames>, harness mode hud) and the HUD's
//      draw counters it reports
typedef struct CatHudCounts
{
    udword frames;              // cat_moRender calls that drew the elements
    udword statusBars;          // cat_selStatusBarsDraw calls
    udword bars;                // cat_selStatusBarDraw calls
    udword icons;               // cat_overlayIconDraw calls
    udword infoPanels;          // cat_shipPrintDraw printing passes
    udword infoLines;           // cat_shipPrintLine calls that printed
    udword selectedLines;       // overlay selection-list lines
    udword selectingLines;      // overlay band-box-list lines
    udword messageLines;        // overlay message lines drawn
    udword supportUnits;        // cat_moSupportUnitsDraw panels drawn
    udword pieDraws;            // piePointSpecDraw calls
    udword compassDraws;        // cat_pieCompassDraw calls
} CatHudCounts;
extern CatHudCounts catHudCounts;
void catHudSelfTestSet(char *string);
bool32 catHudSelfTestRequested(void);
void catHudSelfTestTick(void);

#endif
