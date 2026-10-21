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
