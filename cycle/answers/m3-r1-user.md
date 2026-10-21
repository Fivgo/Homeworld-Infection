# The user's answers for M3-R1 (relayed by the launching session, 2026-10-21)

## Smooth motion during the 2026-10-20 play-test
The user did not know the checkbox existed and never knowingly touched it: "I don't know where I turn that on so unless
it was on by default, it was probably off." The code default is ON, so the `objInterpolate 0` / `nisInterpolate 0` rows
in original/play-home/.homeworld/Cataclysm.cfg (written 2026-10-20 20:09:00) were NOT set on purpose by the user.
Producer 4: treat the play-test as run with interpolation OFF, and treat the origin of those zeros as a first-class
question: find the path that wrote them (an earlier session's options screen, the Defaults button, opRestoreSavedSettings,
the Accept / Cancel flow, the copied OP_NoLOD atom's initial state, the options-file read of a missing row). If any path
writes 0 without the user ticking the box off, that is the first bug of the item.

## Jitter example (the user, 2026-10-21; "I was able to easily recreate it")
A standard multiplayer (skirmish) match. Focus the camera on the nearest Recon to the mothership and put the camera at
the ZENITH of that Recon (looking straight down on it). Select an ADJACENT Recon and order it to move directly forward
about 500 m "to get a good travel distance". The moving, unfocused Recon jitters visibly along its whole path.
Producer 4: reproduce this exactly as a measured scene (the skirmish self-test's map, LiveFireExercise2 or Armed8:
the camera focused on Recon A at its zenith, Recon B given a move order 500 units ahead, 144 Hz virtual, the flags on
and /noInterp), report Recon B's per-frame screen-space displacement and repeated frames, and treat it as the primary
case of the item; the six object kinds of the task come after it. The monitor's refresh rate is not confirmed by the
user (the machine's 4K monitor is the 144 Hz target of docs/features/high-refresh-4k.md). Check this file again before
writing the report: the user may add more cases.

## After M3-R1 (the user's second play-test, 2026-10-21 morning, release build eb5f7e3 at 3840x2160)
- **The camera jitter is FIXED** with Smooth motion on: "okay that worked a lot better this time. I would mark the camera
  issue as fixed." (The 07:43 session had run with the flag off again: no options visit, the cfg rows still 0; the
  launching session set objInterpolate 1 / nisInterpolate 1 in original/play-home/.homeworld/Cataclysm.cfg by hand,
  backup Cataclysm.cfg.bak-20261021. Lesson for M3-R2: log the flags at start-up and show them on screen.)
- **NEW: a one-frame flicker in an in-game cutscene** (the NIS with the close-up of the Kuun-Lan): "an odd 1-frame
  flicker where all objects were missing including the black bars. I think I can only see the backdrop during that
  frame. It's infrequent." Detection idea: the nis dumps carry a crc per frame; scan the Mission01 NISes at 144 Hz for a
  single-frame outlier (a frame whose pixel diff to both neighbours is huge) and capture it; candidates: the NIS camera-cut
  handling in the render-time path (bsCurveEvalAtBeforeJump, alpha_nis at a cut), the letterbox / scissor state at an NIS
  event tick, an early return in the main-region draw (mrDisabled / nisCameraCutTime at mainrgn.c:1913), a frame drawn
  before the regions (the backdrop only).
- **NEW: the hyperspace gate is not to scale** for the Kith ship warping in during the in-game cutscene (Mission02's):
  "It was more frigate sized then mothership sized." Lead: HS.c sizes the gate from the ship's staticCollInfo (the core
  hull of a modular ship, not the assembly).
- **NEW: parts of the arriving ship pop in after the hyperspace exit**: "some of the back half parts of the ship
  flickered into existence after the hyperspace was complete ... I would ensure that all the parts necessary for the
  scene are loaded and then let the exit hyperspace animation play out." USER RULE: every part present before the
  animation starts, then one clip reveal over the whole assembly. Lead: the modules are clamped ships; hsContinue has a
  clamped-ship path; check when the exe creates / shows the modules for a ship arriving by hyperspace.
