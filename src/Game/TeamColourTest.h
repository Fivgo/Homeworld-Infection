/*=============================================================================
    Name    : TeamColourTest.h
    Purpose : HWI-TEST (M3-R1 team-colours-warp): the team-colour dump and self-test (/catTeamColourTest)
=============================================================================*/
#ifndef ___TEAMCOLOURTEST_H
#define ___TEAMCOLOURTEST_H

#include "Types.h"

extern bool32 catTeamColourTest;
extern bool32 catTeamColourLoadedGame;          // the colours came from a saved game (the load-game test)
extern sdword catTeamColourNextTicks;           // /catTeamColourTicks N (default 32): missiontest ticks in the next mission level

void catTeamColourDump(const char *tag);
bool32 catTeamColourCheck(const char *where);
void catTeamColourLevelChange(sdword stage);    // singlePlayerLoadNewLevel: 0 at its start, 1 before trRegistryRefresh, 2 after it
void catTeamColourFrameEnd(void);               // render.c rndRenderTask, next to catShotFrameEnd
void catTeamColourDone(void);                   // the run's end: a last check and the "teamcolour: done" line

#endif
