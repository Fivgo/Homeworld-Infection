# M3-R3: the user's third play-test reports (relayed 2026-10-24 by the launching session)

NOT M3-R2's scope. M3-R2 (the flicker, the hyperspace assembly, the mesh batching, the instrumentation) was cut off by the
weekly limit on 2026-10-22 after its first port and audit (bbdc613) and resumed on 2026-10-24; these reports are queued
for the round after it. The M3-R2 integrate's play-test brief may list them as queued, nothing more.

## The session
The user played "past mission 4" (missions 2, 3 and 4 at least) on the release build tools/play.sh made from main at
9f6e6a7 (src/ identical to 06dae5c: the M3-R1 exit eb5f7e3 plus the M3 Windows build source fixes; nothing of M3-R2),
3840x2160 fullscreen-desktop, UI scale 200%, objInterpolate 1 / nisInterpolate 1 (cfg rows set by hand on 2026-10-21),
FULL DETAIL on. Logs: original/play-home/logs/play-20261022-155157.log (read only; never write under original/play-home).

## Item 0 (the user's decision of 2026-10-24, on the instrumentation port's question)
"let's dedicate a key from the F1-F12 suite for this overlay": bind the frame overlay's toggle to an F1-F12 key that
cataclysm.exe leaves unbound in the main game view (candidate F12: the only F key absent from the main region's hardwired
table in src/SDL/mainrgn.c, used only inside the construction manager's build hotkeys; verify against the exe's key
tables and the keys self-test, 197/149). The Menu key may stay as a second toggle. Also from the user: at 200% UI scale
the overlay is readable and covers nothing needed (no layout change).

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

## Findings by the launching session (2026-10-25), for the producers of items 1 and 3
- Item 3 (closest texture filtering): GoK already carries it. `texLinearFiltering` is a cfg row (the user's
  Cataclysm.cfg row 11, value 1) and `/noFilter` a run-only switch (src/SDL/main.c:1225); texreg.c:1241-1251 selects
  GL_LINEAR_MIPMAP_LINEAR / GL_LINEAR against GL_NEAREST_MIPMAP_LINEAR / GL_NEAREST, both with 8x anisotropy. The
  Options-screen toggle is commented out (Options.c:1923-2021). The item reduces to: confirm the cfg row and the switch
  work at 4K (a capture pair), put it on the hwi-settings line, and decide whether to revive the Options toggle; no new
  feature. The user has not yet tried it: tell them `tools/play.sh -- /noFilter` or `texLinearFiltering 0` in the cfg.
- Item 1 (Mission02's dust belts), CORRECTED 2026-10-25: the start-up warning `Could not locate mex
  Resources/DustClouds/dustcloud0\Rl0\LOD0\dustcloud0.mex` (dustcloud0-3, nebula0) is BENIGN: cataclysm.big holds only
  resources\dustclouds\dustcloudN.{lod,shp} and resources\nebulae\nebula0.{lod,shp}, no .mex at all, so the exe takes the
  same default-mex path (Universe.c loadMexDataCB; not a path-separator fault, the earlier note was wrong). The user
  confirmed on the M3-R2 build (log play-20261025-150145, hwi-settings present) that the dust clouds still do not show
  in Mission02. Starting points instead: (a) are they CREATED: mission02's resource volumes with DustCloud types
  (singleplayer\mission02\dustcloud.dist is only the type-frequency table: types 1-3; LevelLoad.c:471 univAddDustCloud);
  count the DustCloud objects in a headless mission-2 run (m3r2i-e-m2-a prints nothing about them); (b) are they DRAWN
  in the 3D view (Clouds.c, the cloud system; render.c); (c) the tactical map: cat_smResourcesDraw (exe 0x0050ae00,
  Sensors.c:1698) draws dust clouds "as points ... where the grid has seen them" with smResources TRUE; read the exe's
  routine for whether dust belts are drawn as regions and whether they are exempt from the fog-of-war grid (the user's
  "key elements that all players get to see through the fog of war"); a mission-2 capture pair against the exe's look.
- The session of 2026-10-25 14:52 ran the 2026-10-21 07:43 release binary (no hwi-settings line in the log: nothing of
  M3-R2 was in it); tools/play.sh now rebuilds when the binary is older than the newest src/ commit.

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

## The user's later words (2026-10-25), verbatim
- The blur report (on the OLD binary built 2026-10-21 07:43, session play-20261025-145250.log): "after going back into
  the game, the textures on the mother ship look more blurry. I forget, did I ask you to change the texture interpolation
  to closest or is it still on linear" (answer given: still linear; nothing in that path changed since R9; the engine
  already has `texLinearFiltering` / `/noFilter`).
- On the M3-R2 build (session play-20261025-150145.log, the first with the hwi-settings line): "yeah doesn't look like
  the dust clouds loaded in mission 2 again. Am I launching it correctly?" (yes: the log shows the new build and the
  settings line; the dust clouds were never touched before this round).
- THE MANDATE for this round: "yeah lets get r3 underway. also I want to implement a new option to support sharp
  texture meshes or the original texture look". USER DECISION: item 3 becomes a real option (Options -> Video control +
  the existing cfg row + the switch + the settings line), DEFAULT = the original look (the exe's linear filtering),
  SHARP = the option. Producer 1 (settings-and-keys) owns it.

## The round's shape (the launching session, 2026-10-25)
Five ports in sequence: (1) settings-and-keys = item 0 (the overlay's F key) + M3-R2's /letterboxBehind toggle-check fix +
item 3 (SHARP TEXTURES); (2) pause-freeze = item 5; (3) mission02 = items 1 + 2; (4) mines = items 4 + 6; (5) engine-trails
= item 7. Open question for the user, not blocking: in Mission02, were the dust clouds missing everywhere (the 3D view
too) or only on the tactical map?

## The user's words (2026-10-26 10:2x, during the engine-trails audit), verbatim
- "okay you can use the gpu now if its needed": the GPU is free for this round's measurements and captures (the 4K
  FULL DETAIL figures, the trail captures at 3840x2160, the integrate's soak-style checks). The idle check in the
  harness still applies; at the time of the note the GPU sat at 5% with only Steam's helpers running, no game.
