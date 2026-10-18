/*=============================================================================
    Name    : MotionTest.h
    Purpose : HWI-FEATURE(motion-interp) measurement: the motion and nis
              harness modes (/catMotionTest, /catNisMotionTest), which run a
              headless Mission01 at a fixed virtual frame rate and dump, per
              rendered frame, the camera pose and the screen-space position of
              the tracked objects (docs/features/motion-interpolation.md item 4,
              docs/m2-r6-motion-interp.md).

    Created 2026-10-18 for Homeworld-Infection (M2-R6 motion-interp). Not Relic
    or Cataclysm code: the whole file is HWI-FEATURE(motion-interp). This tree
    is REULA-licensed, see src/COPYING.REULA.
=============================================================================*/

#ifndef ___MOTIONTEST_H
#define ___MOTIONTEST_H

#include "Types.h"

struct Camera;

// command line (SDL/main.c)                                    HWI-FEATURE(motion-interp)
void catMotionTestSet(char *string);        // /catMotionTest <ticks>: the gameplay measurement
void catNisMotionTestSet(char *string);     // /catNisMotionTest <nis>: the NIS measurement (e.g. n06)
void catMotionTestDumpSet(char *string);    // /catMotionDump <file>: the per-frame dump
void catMotionTestFpsSet(char *string);     // /catMotionFps <fps>: the virtual frame rate (default 144)
void catMotionTestShotSet(char *string);    // /catMotionShot <frame>: that frame of the window saved as <dump>.<frame>.ppm

extern bool32 catToggleSelfTest;            // /catToggleTest: the Smooth motion checkbox, the options file, the console
void catToggleRunSelfTest(void);

bool32 catMotionTestRequested(void);        // either mode
udword catMotionTestFramesPerTick(void);    // render frames per universe tick of the virtual clock (0: not a motion run)

// hooks
void catMotionTestTick(void);                       // after each universe tick (OracleDump.c)
void catMotionTestFrameSample(struct Camera *camera);   // right after the main view is drawn (mainrgn.c mrRegionDraw)
void catMotionTestFrameEnd(void);                   // end of the frame, before the flip (render.c rndRenderTask)

#endif // ___MOTIONTEST_H
