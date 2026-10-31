// =============================================================================
//  VideoLayoutTest.h
//  - HWI-TEST (video-layout): /catVideoLayoutTest <screen>: the screen's layout,
//    its sliders' knobs and clicks checked once it is shown (VideoLayoutTest.c).
//    Not in cataclysm.exe.
// =============================================================================

#ifndef ___VIDEOLAYOUTTEST_H
#define ___VIDEOLAYOUTTEST_H

#include "Types.h"

void catVideoLayoutTestSet(char *screenName);   // HWI-TEST: /catVideoLayoutTest <screen>
void catVideoLayoutFrameEnd(void);              // HWI-TEST: render.c, each frame before its swap

#endif
