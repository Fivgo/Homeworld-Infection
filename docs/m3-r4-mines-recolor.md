# M3-R4 ad hoc: the level mines white after a level change (CAT-FIX mines-recolor), 2026-10-26

Landed from the user's terminal session 38027cce between rounds, after the user's play-test of M3-R3's mines fix.

## The user's words (verbatim)
- "In mission 6 im still seeing the same untextured mines. Should this update account for those aswell?"
- "I can see that one type of mine is textured and they're the ones laid by the minelayer corvettes. there are still
  another type of mine the game uses that still needs textures"

## What Mission06 has
Two kinds, as Mission03: 29 level mines (`Mine` lines in `singleplayer/mission06/resources`: race Derelict,
`mineStaticInfos[RACE_Derelict]`, colour scheme 0) and the Taiidan minelayers' laid mines (the layer team is set to
scheme 2 by the script, `Init_Mission06_Minelayer`; the Taiidan row in Mission06 is `10100000`, so they register). The
level `Mine` lines of the campaign: Mission03 210, 06 29, 10 4, 12 26, 14 32.

## The finding
- A fresh Mission06 start is textured (`m3r4-m6mines-lvl`: `meshtex: draw ... RACE_DERELICT/mine ... textures=11 bound=11
  white=0`, the centre crop 8,653 / 8,603 / 8,572 colours at t320 / 400 / 480).
- The user's path was Mission05 played into Mission06. **Reproduced** (`m3r4-m6mines-m5to6-before`, `--mode missiontest
  --mission 5` with `/catTeamColourTest /catTeamColourTicks 800` so the run goes on into the Mission06 level, `/catMeshTexLog
  /MINE/RL /catMineView 300:150`, `--shot 320-640/80:0`): in Mission05 the Derelict mine static loads with the row
  `00000000` (`universeDefaultTeamColors`: missions 5, 7, 11 and 15 have no Derelict entry) so `meshTextureRegisterAllPlayers`
  registers nothing (`register load ... handles=none (nothing registered)`); the Derelict mine static is loaded whether needed
  or not and never closed; `universeStaticInit`'s mine loop (and its missile loop) has no recolour branch for a static that is
  already loaded, unlike the ship and derelict loops; so in Mission06 (row `11000000`) the mesh keeps its unregistered
  materials: `draw tick=304 ... textures=11 bound=0 white=11`, every material `unregistered`; the Mission06 captures
  (`shot.s2.*`) 618 / 645 / 645 / 927 colours in the centre crop.
- A save made in Mission06 loads textured (`m3r4-m6mines-load06b`, the user's "06 - Kadiir Nebula" of 18:24: `bound=11
  white=0`, 9,509 colours): the load reloads the static under Mission06's row. Loading the user's "05 - ~ End of Mission"
  save shows the Mission05 state (`register load ... teamColor=00000000 handles=none`).

## The exe
The same structure, read in `ghidra/exports/cataclysm_gog_r3a/decompiled.c`: `universeStaticInit` 0x0053d2a0's mine section
(the `&DAT_00b3ac10` loop) has the load branch and the close branch only, skips the close for `mineStaticInfos[6]`
(`&DAT_00b3b318`, the Derelict mine) and never calls the recolour (`FUN_004a4c80` = `lodAllMeshesRecolorize`, called in the
ship and derelict sections only); `meshTextureRegisterAllPlayers` 0x004ae910 returns -1 when no scheme of the row is set;
`meshLoad` 0x4aec90 leaves the material unregistered on -1. So cataclysm.exe shows the white Derelict mines on this path too
(a decompile reading; not run on the exe). Hence a **CAT-FIX** (a fault the exe shares), not an HWI-FIX.

## The fix (CAT-FIX (M3-R4 mines-recolor))
`src/Game/Universe.c` `universeStaticInit`: the mine loop and the missile loop get the `else if (LOD != NULL)
lodAllMeshesRecolorize(LOD)` branch the ship and derelict loops have, so a static kept loaded across a level change is
recoloured for the new level's row; `meshRecolorize`'s never-registered branch (the exe's own code) then registers the
textures from the saved texture name. HWI-TEST: `meshtex: register recolor ...` lines from that branch (`/catMeshTexLog`).
Render only: no simulation field.

## Fence (debug builds of the worktree, ASan 0 in every run, both builds 0 warnings)
| run | result |
|---|---|
| `m3r4-m6mines-m5to6-after` (the before run's shape) | at the Mission06 load `register recolor ... RACE_DERELICT/MINE ... teamColor=11000000 handles=a5f,10a5f,...` (48 lines); `draw tick=304 ... textures=11 bound=11 white=0`; the Mission06 captures 8,627 / 8,637 / 8,587 / 8,884 colours (t640 403 in both runs: the frame after the camera leaves); the stream IDENTICAL to the before run (16,353 ticks) |
| `m3r4-mr-mission-a` | the Mission01 stream IDENTICAL to `m3r3i-e-mission-a` (6,001 ticks) |
| `m3r4-mr-m3-a` | Mission03 self-test `objectives=5/5 result=PASS` at 17,825; IDENTICAL to `m3r3i-e-m3-a` |
| `m3r4-mr-m6-a` | Mission06 self-test `objectives=7/7 result=PASS` at 31,011; IDENTICAL to `m3r3i-e-m6-a` |
| `m3r4-mr-mn-shot-lvl` (M3-R3's level-mine capture) | 4 / 4 frames byte-equal to `m3r3i-e-mn-shot-lvl`, the stream IDENTICAL, 0 recolour registrations (the fresh path is untouched) |

Tags: `CAT-FIX` 196 -> 198, `HWI-TEST` +4 (505 with sensors-points' lines). Pre-existing, not this change's: the UBSan
misaligned `ushort` load in `btgGetTexture` (src/Game/BTG.c:389) in every run that loads a save or changes level, on the
unpatched binary too.

## For the user
Play from a Mission05 save through the jump into Mission06 (or load any Mission06 save): the level mines are the grey-brown
textured mines of Mission03. The same path exists for Mission11 -> Mission12 (26 level mines). Missiles kept across a level
change take the same branch; not captured in flight.
