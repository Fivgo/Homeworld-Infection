# The user's answers for M3-R1 (relayed by the launching session, 2026-10-21)

## Smooth motion during the 2026-10-20 play-test
The user did not know the checkbox existed and never knowingly touched it: "I don't know where I turn that on so unless
it was on by default, it was probably off." The code default is ON, so the `objInterpolate 0` / `nisInterpolate 0` rows
in original/play-home/.homeworld/Cataclysm.cfg (written 2026-10-20 20:09:00) were NOT set on purpose by the user.
Producer 4: treat the play-test as run with interpolation OFF, and treat the origin of those zeros as a first-class
question: find the path that wrote them (an earlier session's options screen, the Defaults button, opRestoreSavedSettings,
the Accept / Cancel flow, the copied OP_NoLOD atom's initial state, the options-file read of a missing row). If any path
writes 0 without the user ticking the box off, that is the first bug of the item.

## Jitter examples
Not yet. The user will supply consistent reproductions later (mission, moment, camera focus, objects, refresh rate);
until then measure the six object kinds of the task. Check this file again before writing the report.
