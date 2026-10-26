/*=============================================================================
    Name    : MotionInterp.h
    Purpose : HWI-FEATURE(motion-interp): render-time motion interpolation of
              every drawn object and the camera, in gameplay and in NIS
              cutscenes, behind the "Smooth motion" toggle
              (docs/features/motion-interpolation.md, docs/m2-r6-motion-interp.md).
              Not in cataclysm.exe; presentation only: the simulation and the
              oracle stream are unchanged with the flags on or off.

    Created 2026-10-18 for Homeworld-Infection (M2-R6 motion-interp). Not Relic
    or Cataclysm code: the whole file is HWI-FEATURE(motion-interp). This tree
    is REULA-licensed, see src/COPYING.REULA.
=============================================================================*/

#ifndef ___MOTIONINTERP_H
#define ___MOTIONINTERP_H

#include "Task.h"
#include "Types.h"

struct Camera;

/*=============================================================================
    Data:
=============================================================================*/

// the two flags (both default on): objInterpolate draws every object between its previous and current universe tick
// (and the camera's focus with it), nisInterpolate evaluates the NIS camera and NIS-driven objects' curves at the
// render time. The Smooth motion checkbox (Options -> Video) drives both; the options-file keys objInterpolate /
// nisInterpolate, the switches /noInterp and /noNISInterp and the console rows set them one by one.
extern bool32 objInterpolate;               // HWI-FEATURE(motion-interp)
extern bool32 nisInterpolate;               // HWI-FEATURE(motion-interp)
extern taskhandle universeTaskHandle;       // HWI-FEATURE(motion-interp): the universe task (utility.c)

/*=============================================================================
    Functions:
=============================================================================*/

void miCaptureTick(void);                           // start of every universe tick (univUpdate): prev transforms
void miForget(void *pointer);                       // an object's memory is freed (memFree)
void miRenderBegin(void);                           // before the regions draw (rndRenderTask): render transforms in
void miRenderEnd(void);                             // after: the simulation's transforms back, bit for bit
struct Camera *miRenderCamera(struct Camera *camera);   // the camera to draw with (the NIS camera at the render time)
real32 miRenderAlpha(void);                         // this frame's alpha (-1: not interpolating)
void miRenderStats(udword *blended, udword *fresh, udword *teleport);   // M3-R1 motion-jitter: this frame's counts
bool32 miRenderWritten(const void *obj);            // M3-R1 motion-jitter: the frame wrote this object's transform
bool32 miUniversePaused(void);                      // HWI-FEATURE(pause-freeze): universePause or the Pause key's flag

// the toggle (Options.c, Console.c, main.c)
bool32 miSmoothMotionGet(void);                     // TRUE when either flag is on
void miSmoothMotionSet(bool32 on);                  // both flags (a click ends a switch's run-only override)
void miSwitchOff(bool32 obj, bool32 nis);           // /noInterp, /noNISInterp: off for the run only (main.c)
void miOptionsFileValues(bool32 begin);             // utyOptionsFileWrite: the options file's values in / the run's back
void miSwitchStateSave(void);                       // opOptionsSaveSettings: the switch state for Cancel
void miSwitchStateRestore(void);                    // opRestoreSavedSettings
bool32 miSwitchStands(void);                        // a switch's override stands (the toggle self-test)
bool32 miSwitchObjStands(void);                     // HWI-FEATURE(instrumentation): objInterpolate's override (/noInterp)
bool32 miSwitchNisStands(void);                     // HWI-FEATURE(instrumentation): nisInterpolate's (/noInterp, /noNISInterp)

#endif // ___MOTIONINTERP_H
