/*=============================================================================
    Name    : AISelfTest.h
    Purpose : HWI-TEST (M2-R7 ai-orders-moves): the computer player's self-test,
              /catAiTest <ticks> (harness mode ai). Not in the exe.

    Created 2026-10-18 for Homeworld-Infection (M2-R7 ai-orders-moves).
    This tree is REULA-licensed, see src/COPYING.REULA.
=============================================================================*/

#ifndef ___AISELFTEST_H
#define ___AISELFTEST_H

#include "Types.h"

void catAiSelfTestSet(char *string);
bool32 catAiSelfTestRequested(void);
void catAiSelfTestTick(void);

#endif
