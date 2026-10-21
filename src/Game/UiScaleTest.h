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

#endif
