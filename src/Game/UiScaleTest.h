// =============================================================================
//  UiScaleTest.h
//  - HWI-TEST (M3-R1 ui-scale): the UI scale's self-test (/catUiScaleTest,
//    harness mode uiscale). Not in cataclysm.exe.
// =============================================================================

#ifndef ___UISCALETEST_H
#define ___UISCALETEST_H

#include "Types.h"

extern bool32 catUiScaleSelfTest;          // /catUiScaleTest
void catUiScaleRunSelfTest(void);           // utility.c, once the front end is up; then the engine quits

// HWI-TEST (M3-R2 instrumentation): /catModeQuitTest <W>x<H>: a resolution change through the Video screen's render
//      switch, then one quit (harness mode modequit; the quit after a resolution change, main.c mainDestroyWindow)
extern bool32 catModeQuitSelfTest;          // HWI-TEST
void catModeQuitTestSet(char *string);      // HWI-TEST
void catModeQuitRunSelfTest(void);          // HWI-TEST: utility.c, once the front end is up; then the engine quits once

#endif
