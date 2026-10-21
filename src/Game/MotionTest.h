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

// HWI-TEST (M3-R1 letterbox-front): the any-mode frame capture: /catShot <spec> saves the listed frames (the physical
//      framebuffer read back before the flip, top row first) as <prefix>.<tick>.<frame>.ppm; <spec> is a comma list of
//      <tick>[:<frame>] or <from>-<to>[/<step>][:<frame>] (universe ticks; <frame> = the n-th frame drawn in that tick,
//      default 0); /catShotFile <prefix> (default "shot" in the settings path); /catShotLetterbox: only the frames with
//      the NIS / KAS letterbox up. One "catshot:" log line per file
void catShotSet(char *string);              // /catShot <spec>
void catShotFileSet(char *string);          // /catShotFile <prefix>
void catShotLetterboxSet(void);             // /catShotLetterbox
void catShotFrameEnd(void);                 // end of the frame, before the flip (render.c rndRenderTask)
void catNisScissorOutSet(char *string);     // /catNisScissorOut <tick>:<seconds>: the nis mode's letterbox fades out there

extern bool32 catToggleSelfTest;            // /catToggleTest: the Smooth motion checkbox, the options file, the console
void catToggleRunSelfTest(void);

bool32 catMotionTestRequested(void);        // either mode
udword catMotionTestFramesPerTick(void);    // render frames per universe tick of the virtual clock (0: not a motion run)

// hooks
void catMotionTestTick(void);                       // after each universe tick (OracleDump.c)
void catMotionTestFrameSample(struct Camera *camera);   // right after the main view is drawn (mainrgn.c mrRegionDraw)
void catMotionTestFrameEnd(void);                   // end of the frame, before the flip (render.c rndRenderTask)

#endif // ___MOTIONTEST_H
