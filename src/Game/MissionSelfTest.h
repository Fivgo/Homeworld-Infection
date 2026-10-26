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
// HWI-TEST (M2-R9 missions-02-09): /catMissionTestMission <N>: the mission (default 1); 2 .. 17 start through the warp
void catMissionSelfTestMissionSet(char *string);
sdword catMissionSelfTestWarpLevel(void);           // the warp start's level for the test (0: none)
// HWI-TEST (M2-R9 missions-02-09): the KAS objective calls (KASFunc.c): "destroy" <label>, "set" <label> <status>,
//      "destroyall"; a mission after the first counts an objective completed when its script destroys it by name or sets
//      it complete (ObjectivesDestroyAll, the Hyperspace message's clean-up, completes none)
void catMissionSelfTestObjective(const char *what, const char *label, sdword status);
bool32 catMissionSelfTestRequested(void);
bool32 catMissionSelfTestTutorialRequested(void);
void catMissionSelfTestTick(void);
void catMissionSelfTestPlay(void);                  // the scripted player, inside univUpdate (after the AI players)
void catMissionSelfTestEvent(const char *what);     // a flow event (NIS start / stop, level load, KAS start)
// HWI-TEST (M2-R9 missions-10-17): /catMissionTestTrace: every new KAS message (aitMsgSend) logged as a "missiontest:
//      msg" line (a team's re-queue of a message it did not handle is not repeated); a diagnostic, off in the gate runs
void catMissionSelfTestTraceSet(void);
// HWI-TEST (M2-R10 tests-leftovers): /catMissionTestFailJump with /catMissionTest <ticks> /catMissionTestMission N (harness
//      missiontest --fail-jump): the deliberate-loss test of the failed jump. At the test's first hyperspace command
//      DisablePlayerHyperspace runs through the developer console's KAS call (cat_kasCallFunctionByName 0x0045b9a0, the
//      exe's only route to hyperspaceFails); the jump fizzles and returns through singlePlayerGameUpdate's case
//      HYPERSPACE_CATFAILEDRETURN (10); the return is judged ("missiontest: failed jump ..." lines) and the run ends 32
//      ticks later (no second jump: case 10 does not give playerCanHyperspace back) [audit: M2-R10 tests-leftovers: was
//      "then the test jumps again for real and the mission ends as usual", which the code does not do]
void catMissionSelfTestFailJumpSet(void);
// HWI-TEST (M3-R3 mission02): /catMissionTestHoldScout <tick> with /catMissionTest <ticks> /catMissionTestMission 2: the
//      FindDestroyer recon waits until <tick>, so Mission02's BUSHAN-RE hint timer can run out (MissionSelfTest.c
//      catMtHintJudge); catMissionSelfTestSpeech: kasfSpeechEvent's call, a "speech: event=.. tick=.." line per event
void catMissionSelfTestHoldScoutSet(char *string);
void catMissionSelfTestSpeech(sdword event);
// HWI-TEST (M2-R9 missions-10-17): /catWarpStartTest with /catMissionTest <ticks> (harness mode warpstart): the warp
//      start from a test copy of warp.script (WarpEnabled 1, LevelToWarpTo N) in the run's data dir: the level's record
//      against the fleet in the ether (cat_spWarpStartSelfTestCheck), then the level, its KAS script and the fleet's
//      arrival; "warpstarttest: ..." lines, "warpstarttest: done ... result=PASS|FAIL"
void catWarpStartSelfTestSet(void);
bool32 catWarpStartSelfTestRequested(void);
void catWarpStartSelfTestScript(bool32 enabled, sdword level);  // singlePlayerInit, after Warp.script is read
void catWarpStartSelfTestApplied(bool32 pass);                  // cat_spWarpToLevel, before the level loads
struct AITeam;
void catMissionSelfTestMessage(struct AITeam *from, struct AITeam *to, udword msg);
// HWI-TEST (M2-R8 singleplayer-tutorial): /catLoadGameTest <ticks> /catLoadGameName <name> (harness mode loadgame): a
//      single-player save of the install (SavedGames/SinglePlayer/<name>) loaded as the front end's Load Game does, then
//      run <ticks> universe ticks on the mission run's virtual clock
void catLoadGameSelfTestSet(char *string);
void catLoadGameSelfTestNameSet(char *string);
void catLoadGameSelfTestTrainingSet(void);           // M2-R9 missions-10-17: a tutorial lesson save (SavedGames/Training)
bool32 catLoadGameSelfTestRequested(void);
void catLoadGameSelfTestStart(void);
void catLoadGameSelfTestTick(void);

#endif
