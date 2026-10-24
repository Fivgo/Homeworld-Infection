# M3-R3: the user's third play-test reports (relayed 2026-10-24 by the launching session)

NOT M3-R2's scope. M3-R2 (the flicker, the hyperspace assembly, the mesh batching, the instrumentation) was cut off by the
weekly limit on 2026-10-22 after its first port and audit (bbdc613) and resumed on 2026-10-24; these reports are queued
for the round after it. The M3-R2 integrate's play-test brief may list them as queued, nothing more.

## The session
The user played "past mission 4" (missions 2, 3 and 4 at least) on the release build tools/play.sh made from main at
9f6e6a7 (src/ identical to 06dae5c: the M3-R1 exit eb5f7e3 plus the M3 Windows build source fixes; nothing of M3-R2),
3840x2160 fullscreen-desktop, UI scale 200%, objInterpolate 1 / nisInterpolate 1 (cfg rows set by hand on 2026-10-21),
FULL DETAIL on. Logs: original/play-home/logs/play-20261022-155157.log (read only; never write under original/play-home).

## The reports, verbatim (the user's message of 2026-10-24, in the user's order)
"Also I found these issues while playing past mission 4:"

1. (mission 2, the tactical map, fog of war) "in mission 2, I believe I was supposed to see dust belts rendered while on
   the tactical map. There may be key elements that all players get to see through the fog of war."

2. (mission 2, the Bushan-Re hint dialog) "In mission 2 there is a dialog trigger that hints at where the BUSHAN-RE is
   located. I did not experience this so I would like you to double check the conditions for what triggers that dialog.
   Perhaps it only triggers if the player starts searching in the opposite direction."

3. (texture filtering, a launch option) "Next I noticed that in game, the game prefers using linear shading. This my be
   the intended style choice from the devs but lets support a launch option that sets the shading to closest for texture
   interpolation."

4. (mission 3, the mines' textures) "In mission 3, there are 2 types of mines: ones that are story related and ones that
   are dropped from minelayers. Please double check what textures they are wired to because they are currently
   completely white which to me seems like a failed texture load."

5. (pause, the other half of Smooth motion) "In general, I've notice that while paused, it seems like all ships hold onto
   their travel vector as well as the camera. During a pause I notice that ships jitter in place and it seems to be the
   other half of the issue to get smooth interpolation to work."

6. (mission 3, the mines' AI) "In mission 3, I noticed that the mines tended to travel toward their target for about 3
   seconds then stop in place trying to find the target again. Please double check the ai behind mines to see what they
   are trying to do."

7. (missile corvette engine trails) "In mission 3, I noticed that missile corvettes only emit a single jet stream when
   they have 4 engines on the model. See if we can get 4 jet steams to emit from the direct centers of the circles"

## The orchestrator's notes (hypotheses, not findings; every item still starts from the exe)
- 1, 2, 4, 6 are "is the recreation 1-to-1 with cataclysm.exe here?" questions first: read the exe's and the mission
  scripts' behaviour (the M02 dust-belt tactical-map draw and its fog-of-war exemption; the M02 hint-dialog trigger
  conditions; the mine ship types' texture wiring and the texture-load log; the mine AI's seek/arm/re-acquire timers)
  and state what the exe does before touching src/. A white mesh is the classic failed-texture signature; the mine
  types may be the only ships whose textures live in a path the recreation does not search, or the lifs may be palette
  textures the loader rejects.
- 3 is a recreation feature (HWI-FEATURE): a run-only switch plus a cfg row for GL_NEAREST texture filtering, default
  unchanged (the exe's linear filtering is the base).
- 5 is M3-R1's Smooth motion (src/Game/MotionInterp.c): while the universe is paused the 16 Hz capture tick does not
  fire, so the render lerp keeps blending between a stale previous pose and the current one with an alpha that keeps
  moving (taskPeriodFraction) or wraps; the likely fix is to freeze the render pose (alpha held, or the last rendered
  pose kept) whenever the universe is paused, and the same for the camera. Fence: a paused motion dump with every
  per-frame crc identical.
- 7 is the engine-trail (nozzle) wiring: the missile corvette's mesh has four engine nozzles; check how many trail
  emitters the exe creates for that ship type (the ship static data's trail count and the nozzle positions from the
  mesh) against the recreation; an exe that also draws one trail makes this a recreation feature, not a fix.
