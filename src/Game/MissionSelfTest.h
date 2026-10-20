/*=============================================================================
    Name    : MissionSelfTest.h
    Purpose : HWI-TEST (M2-R8 kas-runtime-mission01): the mission self-test,
              /catMissionTest <ticks> (harness mode missiontest): Mission01 from
              its NIS-first start through its KAS objectives, headless, with
              scripted events in place of the player's input. Not in the exe.

    Created 2026-10-19 for Homeworld-Infection (M2-R8 kas-runtime-mission01).
    This tree is REULA-licensed, see src/COPYING.REULA.
=============================================================================*/

#ifndef ___MISSIONSELFTEST_H
#define ___MISSIONSELFTEST_H

#include "Types.h"

void catMissionSelfTestSet(char *string);           // /catMissionTest <ticks>
void catMissionSelfTestTutorialSet(char *string);   // /catMissionTestTutorial <ticks>: Tutorial1's first screen
bool32 catMissionSelfTestRequested(void);
bool32 catMissionSelfTestTutorialRequested(void);
void catMissionSelfTestTick(void);
void catMissionSelfTestPlay(void);                  // the scripted player, inside univUpdate (after the AI players)
void catMissionSelfTestEvent(const char *what);     // a flow event (NIS start / stop, level load, KAS start)

#endif
