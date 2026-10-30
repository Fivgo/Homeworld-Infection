/*=============================================================================
    Name    : GamePick.c
    Purpose : Code for choosing a Game

    Created 10/21/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___GAMEPICK_H
#define ___GAMEPICK_H

#include "font.h"
#include "prim2d.h"

#include "FEFlow.h"
#include "Region.h"

/*=============================================================================
    Switches:
=============================================================================*/

/*=============================================================================
    Definitions:
=============================================================================*/
#define GP_ScreenName           "Load_game"
#define GP_ListFont             "Arial_12.hff"
#define GP_NameFont             "Arial_12.hff"
#define GP_NonSelectedColor     colRGB(0, 75, 120)
#define GP_SelectedTabbedColor  colRGB(1, 170, 221)
#define GP_SelectedColor        GP_SelectedTabbedColor
#define GP_ListMarginInterLine  1
#define GP_ListMarginX          2
#define GP_ListMarginY          2
#define GP_DisplayListLength    11      //number of games displayed on the screen at one time
#define GP_VertSpacing          (fontHeight(" ") >> 1)
#define GP_GAMENAME_LENGTH      32

/*=============================================================================
    Type definitions:
=============================================================================*/
typedef struct
{
    char    *title;
    char    *fileSpec;
}
gpgame;

/*=============================================================================
    Data:
=============================================================================*/
extern gpgame *gpGames;             //list of available games
extern sdword gpCurrentSelected;    //current game index, if OK is pressed
extern char   *gpTextEntryName;     //text box entry name

extern bool32 gpLoadSinglePlayerGame;
extern bool32 gpLoadTutorial;

extern char TutorialSavedGamesPath[];

/*=============================================================================
    Functions:
=============================================================================*/

//startup/shutdown
void gpStartup(void);
void gpStartInGameEscapeMenu(void);
void gpShutdown(void);
void gpReset(void);

//run the game picker
void gpGamePick(char *dest);

//close the picker, either with 'back' or with 'OK'
void gpDonePicking(char *name, featom *atom);
void gpBackPicking(char *name, featom *atom);
void gpLoadGivenGame(char *gamename);     // HWI-FIX(release-1.0)

bool32 gpQuickSave(void);
bool32 gpQuickLoad(void);

bool32 gpGetGameName(char *name, featom *atom, char *filename);

sdword gpCountTrainingSavegames(void);

// CAT (M2-R5 hud): the game-stats screen (cat_gameStats*, 0x00488970-0x0048a056) and its button drawer, which the main
//      overlay's taskbar uses too
void cat_gameStatsButtonDraw(rectangle *rect, char *text, sdword hotkeyIndex, fonthandle font, sdword capWidth,
                             sdword iconLeft, sdword iconLeftMouse, sdword iconMid, sdword iconMidMouse,
                             sdword iconRight, sdword iconRightMouse, bool32 wholeHover);       // 0x00489470
void cat_gameStatsStartup(void);                                                            // 0x00489e50
real32 cat_gameStatsValueGet(sdword row, sdword column);                                    // 0x00488970
void cat_gameStatsValuePrint(sdword row, sdword column, char *out);                         // 0x00488a50
void cat_gameStatsPlayerRowFill(sdword playerIndex, sdword *row);                           // 0x00488c80
void cat_gameStatsLayoutCompute(void);                                                      // 0x00488e70
void cat_gameStatsEnd(void);                                                                // 0x00489290
void cat_gameStatsClick(void);                                                              // 0x004892e0
void cat_gameStatsDraw(void);                                                               // 0x004896b0
void cat_gameStatsBegin(void);                                                              // 0x00489e90
bool32 cat_gameStatsIsActive(void);                                                         // 0x0048a050

bool32 gpLoadRecordedGameGivenName(void);      // CAT: LoadRecordedGame 0x00487cf0 after the names (M2-R6 oracle-mode-a)

#endif
