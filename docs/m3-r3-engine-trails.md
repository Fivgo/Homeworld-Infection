<!-- done: exe, view, feature, fence, perf -->
# M3-R3 engine-trails: the missile corvette's four jet streams (producer 5, the user's item 7)

Base 4ae7d91 (the mines audit). Subsystems: **exe** (the exe's trail count, the mex chunks, the circles),
**view** (the HWI-TEST capture hook /catTrailView + the harness flag), **feature** (HWI-FEATURE(engine-trails) and its
surfaces), **fence** (captures, dumps, streams, self-tests), **perf** (the 4K cost).

## 1. The exe (subsystem exe)

### The trail count per ship: InitializeEngineTrails 0x00541f40
`python3 -I tools/m2_fdec.py 0x00541f40`: the exe looks up the mex chunks "Eng0".."Eng3" (`mexGetChunk(mex, "Eng",
"EngN")`, N = 0..3) and keeps `numTrails = (highest N found) + 1`; it clears the ship's four trail slots (+0x42C..+0x438)
and creates `numTrails` trails: with a ribbon static (`static +0x2F8 != 0`) `trailNew(trailStatic[i], ship, i != 0, i)`,
else the capital-ship glow (`second` 2 for race 1 / 3, else 3). **The exe's limit is four trails a ship (one per Eng
chunk Eng0..Eng3); there is no per-type table.** The tree's `InitializeEngineTrails` (src/Game/UnivUpdate.c:1752) is the
same loop, so the tree creates exactly the exe's trails.

### The missile corvettes' mex (cataclysm.big, the mex each .shp's pMexData names)
| type | mex | Eng chunks | nozzle (model x = up, y = right, z = forward) | normal | object name |
|---|---|---|---|---|---|
| bMissileCorvette (59, Beast) | race_beast\bMissileCorvette\Rl0\LOD0\bMissileCorvette.mex (348 B, 6 chunks: GUN0, GUN1, Eng0, Eng1, Col0, Rct0) | **2** | Eng0 (0, -17.854, -45.750); Eng1 (0, +17.854, -45.750) | (0, 0, -1), cone 30, edge 5 | none |
| p1MissileCorvette (95, P1 / Turanic, Mission03's) | race_p1\P1MissileCorvette\Rl0\LOD0\P1MissileCorvette.mex (348 B, the same 6 chunks) | **2** | the same two | the same | none |

Both .shp files give the parameters of exactly two trails (trailWidth/Height/Angle/RibbonAdjust/SpriteRadius and the
"2" keys: width 42, height 18, angle 90, ribbon adjust 0.75, sprite radius 60). No Kushan / Taiidan / Somtaaw missile
corvette exists in the install. **So the exe draws TWO ribbon trails for each missile corvette, not one and not four.**

### Every ship type's Eng chunks
173 .shp files: 82 name a mex with no Eng chunk, 68 one, 20 two, 3 three (bRammingFrigate, sRammingFrigate,
tCloakGenerator); **none has more than four**, so **no type has more nozzles than the exe draws**: the exe and the tree draw
one trail per Eng chunk for every type. (The table: original/scratch-m2/m3r3-trails/engcount.txt, made by a few lines of
Python over the extracted mex chunk tables.)

### The four circles
The engine panel is one flat rear face of the mesh at z = -48.375 (lod0 .geo, 1 object, 16 engine_port + 2
engine_port_mid triangles, x -20.4..20.4, y -29..29). The circles are not geometry and not nozzles: they are painted in
engine_port.lif (128x32, paletted), two rings at texel centres (24.5, 16.5) and (103.5, 16.5) (the ring's bounding box,
luminance >= 180 / 200), mapped once on the upper half of the panel (x > 0) and mirrored on the lower half (x < 0). Mapped
through the panel's triangles (barycentric on the UVs) **the four circle centres are (x, y) = (+11.5, -17.89),
(-11.5, -17.89), (+11.5, +17.89), (-11.5, +17.89) at z = -48.375** (both corvettes: the same mesh). The exe's two nozzles
(0, +-17.854) sit exactly between the upper and lower circle of each side, and each ribbon (42 wide, turned 90 degrees by
trailAngle, so 42 tall) spans that pair of circles: Relic's two ribbons stand for four engines.

### The jet stream
The user's "jet stream" is the trail ribbon (trailDraw's segments, additive). Each trail also draws the engine glow
sprite (trailDrawBillboardedSquareThingz 0x005a5be0, radius trailSpriteRadius 60, at the nozzle); the "engine_glow"
effects of Trails.c are the capital ships' glows (style > 2), not used by a corvette.

### Why the user saw "a single jet stream"
The tree draws exactly the exe's two ribbons (proven below: /exeTrails = a reverted build, byte for byte). Each is 42
units across the pair of circles and 18 wide, 35.7 units apart: seen from the side or from far behind, the two overlap
into one broad stream (the user's Mission03 view); seen from behind and above they are two broad bands, each covering
two circles. Nothing in the tree drops a trail: no chunk count read wrong, no limit missed, no glow in place of a ribbon.

## 2. The capture hook (subsystem view, HWI-TEST)
`/catTrailView <tick>:<ShipType>[:d<distance>][:e<elevation>][:s<side>][:r<ratio>][:m<move>]` (src/SDL/render.c, after
`/catMineView`; switch in src/SDL/main.c): from universe tick `<tick>` the sensors manager is closed and the camera
stays on the first ship of that type moving at over `<ratio>` (default 0.4; 0 = any) of its top speed (the static's
maxvelocity), `<distance>` (default 500) behind it along its heading, raised `<elevation>` degrees (20) and turned
`<side>` degrees (30) about the world z axis, snapped every frame; `:m<move>` orders it `<move>` units straight ahead at
the focus (clWrapMove, the command layer, no input). Every frame after the flip one line
`trailview: tick=.. ship=..#id pos=.. speed=.. nlips=.. engineTrails=.. camera=.. h<slot>=x,y n<slot>=x,y c<slot>=x,y`:
the physical pixel of each trail's head as trailDraw puts it (h: the exe's nozzle), its split nozzle (n) and the circle's
centre on the rear panel (c, z -48.375), with that frame's matrices (Trails.c `hwiTrailHeadWorld`). With no qualifying
ship, every 32 ticks `trailview: none tick=.. type=..(n) count=.. bestSpeedRatio=..`. A capture run (the view changes
the render list, so it is no stream reference). Passed after `--` (no harness flag; the harness script is unchanged).

## 3. The feature (subsystem feature, HWI-FEATURE(engine-trails))
**Not a CAT: port**: the exe draws two (section 1), so the four jet streams are a recreation feature on top of the exe's
behaviour. Default **ON** (the user's wish).

What it does (src/Game/Trails.c, the block after `_afterburning`):
- `hwiTrailSplits[]`: the types whose engine circles are known: **bMissileCorvette** and **p1MissileCorvette** (the same
  mesh), circles at up +-11.5, right +-17.891.
- `hwiEngineTrailsSplitCreate` (called at the end of `InitializeEngineTrails`, UnivUpdate.c): for a listed type with the
  exe's two ribbons, slot 0 / 1 (the exe's) become the upper circle of their side (`hwiSplit = +1`) and slots 2 / 3 get an
  added ribbon each (`trailNew(trailStatic[2 + i], ship, 1, i)`: trailNum i, so the exe's parameters of that side) for
  the lower circle (`hwiSplit = -1`). Made whether or not the feature is on (the console row can switch it in a game).
- `trailGetNozzleOffset`: with the feature on, a split ribbon's nozzle is the circle's centre (x += +-11.5, |y| = 17.891;
  z stays the Eng chunk's -45.75, so the ribbon starts inside the hull as the exe's does). The exe's nozzle is
  `trailNozzleOffset(.., FALSE)`.
- The width across the pair is halved for a split ribbon (`hwiTrailWidth`, read by `trailHalfWidth` /
  `trailHeightScalar`): 21 instead of 42, so the two ribbons of a side cover the exe's one ribbon; at 42 each, the upper
  and lower ribbon (23 units apart) would overlap over 19 of their 42 units and, added, read as one again. Height (18),
  angle (90), ribbon adjust (0.75), segments (20 / 10), granularity (3) and the team colours stay the exe's.
- The engine glow stays the exe's: a split pair's sprite is drawn at the exe's nozzle (one per side, radius 60); the
  added ribbon draws none (`hwiTrailGlowHidden`, the afterburn test still runs).
- **The random stream**: an added ribbon draws its jitter (RANDOM_TRAILS_1 in the glow, RANDOM_TRAILS_2..4 per segment
  in trailSurpriseColorAdjust) from its own xorshift32 generator (`trailRandom`, `hwiTrailPrivateRandom`), never from the
  shared stream; the exe's two ribbons draw from the shared stream exactly as before (the same queue lengths, the same
  segment counts, the same order). So **no stream moves, rng_all included** (the task allowed an rng_all move; none was
  needed). The extra ribbons cost memory only in the heap (2 x (176 + n x 100) B per corvette).
- Off (`/exeTrails` or the console row `engineTrails 0`): the added ribbons are not drawn (their queues emptied) and the
  exe's two are the exe's in every respect.

Surfaces:
| surface | name | default |
|---|---|---|
| run-only switch | `/exeTrails` (src/SDL/main.c `ExeTrailsSet`; "this run only": no options-file row, as /meshImmediate) | not given |
| console row | `engineTrails 0|1` (src/Game/Console.c, id CAT_CON_HWI_ROW_ID + 8; CAT_CON_HWI_ROWS 8 -> 9) | 1 |
| settings line | `engineTrails=1 (default)`, `0 (switch)` with /exeTrails, `(console)` after the row (src/Game/Instrument.c hwiSettings[] row 8; the console's choice ends the switch's state) | 1 |
| cfg row / Options control / key | none (the task asks for none) | |
| tools/play.sh header | the switch, the row, the settings line | |

The list of types: by the task's rule (more Eng chunks than the exe draws) **no type** qualifies (section 1: at most 3
chunks, the exe draws up to 4). The split is a per-mesh fact (painted circles), applied to the two types that carry the
missile corvette's mesh; every other type keeps the exe's trails (no other mesh's engine texture was surveyed: a type the
user names later gets a row of `hwiTrailSplits[]` after its circles are measured the same way).

## 4. The fence (subsystem fence)

Every run through tools/m2_harness.sh (offscreen, silent), the debug binary pinned for the batch as
original/scratch-m2/m3r3-trails/hwi-engine-fence (`--bin`); run dirs original/scratch-m2/m2r2-harness/runs/m3r3-et-*.
The crops below are in original/scratch-m2/m3r3-trails/*.png (game-derived, not in git).

### 4a. The exe's count (section 1)
Two ribbons for each missile corvette (InitializeEngineTrails 0x00541f40, one per Eng chunk; the trail pass
0x005772dd draws all four slots); bMissileCorvette / p1MissileCorvette mex: 2 Eng chunks each, nozzles (0, +-17.854,
-45.75); no type has more Eng chunks than the exe draws (max 3 against the limit 4).

### 4b. The captures: four ribbons from the four circles (the camera behind a corvette under way)
The view: `-- /catTrailView 100:bMissileCorvette:d800:e25:s20` in `--mode ai --ai-ticks 420` (the ai scene; the AI's
**bMissileCorvette #302**, 490 u/s by tick 300), shots at ticks 300 and 400; for the other type
`-- /catTrailView 40:p1MissileCorvette:d800:e25:s20:r0:m4000` in `--mode missiontest --mission 3 --mission-test-ticks
260` (**p1MissileCorvette #31**, ordered 4,000 units ahead at tick 40: Mission03's corvettes never move in the
self-test), shot at tick 160. Each with and without `/exeTrails`. Ribbon pixels (orange: R >= 225, 105 <= G <= 205,
B <= 130; yellow for P1: R >= 200, G >= 120, B <= 140) counted as separate runs (>= 2 px, x scaled) on the scanlines
30 / 50 / 80 px (x3.75 at 4K) below the centre of the four projected nozzles:

| run | size | tick | default (four) | /exeTrails | run widths (default / exe) |
|---|---|---|---|---|---|
| m3r3-et-c-on / -c-exe | 1024x768 | 300 | 3 / 4 / 4 runs | 2 / 2 / 2 | 11-14 px / 18-24 px |
| m3r3-et-c-on / -c-exe | 1024x768 | 400 | 4 / 4 / 4 | 2 / 2 / 2 | 10-13 / 18-24 |
| m3r3-et-c4k-on / -c4k-exe | 3840x2160 | 300 | 4 / 4 / 4 | 2 / 2 / 2 | 31-41 / 52-73 |
| m3r3-et-c4k-on / -c4k-exe | 3840x2160 | 400 | 4 / 4 / 4 | 2 / 2 / 2 | 31-38 / 51-70 |
| m3r3-et-p1mv-on / -p1mv-exe | 1024x768 | 160 | 4 / 4 / 4 | 2 / 2 / 2 | 9-12 / 12-13 |
| m3r3-et-p1mv4k-on / -p1mv4k-exe | 3840x2160 | 160 | 4 / 4 / 4 | 2 / 2 / 2 | 26-33 / 33-40 |

(The one 3 at 1024x768 tick 300, +30 px: two of the four still touch that close to the hull.) Crops:
c4k-exe-300.png / c4k-on-300.png, p1mv-exe.png / p1mv-on.png (the P1 corvette's four glowing circles with two broad
ribbons between the pairs, and with four ribbons, one from each circle).

**/exeTrails is the tree before this change, byte for byte**: a reverted build (the same tree without the
`hwiEngineTrailsSplitCreate` call: `trails=1,1,0,0`) captured the same frames (m3r3-et-c-rev, -c4k-rev): every PPM equal
to the /exeTrails one (1024x768 tick 300 `0d11d085f3f612e7`, 400 `34208164fc395f64`; 3840x2160 300 `1d5c7d46904470da`,
400 `f980bf9cea5d0bbf`; sha256 prefixes), and the final binary's frames (m3r3-et-c-on2 / -c-exe2) equal the first
build's (on 300 `bd153523a3ce9ea0`, 400 `9d7cda8505f753a2`).

### 4c. The nozzles on the circles (measured)
The P1 corvette at rest (Mission03 tick 60, `-- /catTrailView 40:p1MissileCorvette:d150:e0:s0:r0`, the camera straight
behind: no ribbon covers the panel), m3r3-et-p1rest (1024x768) and -p1rest4k (3840x2160): the whitish ring pixels
(min(R,G,B) >= 170, R+G+B >= 600) in a window round each projected circle centre, their bounding-box centre against
the projection of the circle centre c (and of the nozzle n):

| circle | 1024x768: ring box (px) | c - box centre | n - box centre | 3840x2160: ring box | c - box centre | n - box centre |
|---|---|---|---|---|---|---|
| 0 (upper, -y) | 363..472 x 291..374 | (+0.4, -0.1) | (+0.6, +0.0) | 1497..1810 x 818..1054 | (+2.7, -0.3) | (+3.3, +0.1) |
| 1 (upper, +y) | 559..668 x 291..374 | (-0.1, -0.1) | (-0.3, -0.0) | 2051..2364 x 818..1053 | (-1.3, +0.2) | (-2.0, +0.5) |
| 2 (lower, -y) | 363..472 x 416..500 | (+0.3, +0.1) | (+0.5, -0.1) | 1497..1810 x 1171..1407 | (+2.5, +0.3) | (+3.1, -0.2) |
| 3 (lower, +y) | 559..669 x 416..500 | (-0.5, +0.1) | (-0.8, -0.1) | 2051..2364 x 1171..1407 | (-1.1, +0.4) | (-1.8, -0.1) |

Every nozzle within 0.8 px of its ring's centre at 1024x768 (rings 110 px across) and 3.3 px at 3840x2160 (rings 314 px
across: about 1% of the diameter).

### 4d. The streams: nothing moved, rng_all included
`python3 -I tools/oracle/hwo_compare.py <ref> <run>` (all 21 fields):

| run (m3r3-et-) | mode | reference | result |
|---|---|---|---|
| mission-a, mission-b | mission 6,000 (decision 44) | m3r1i-mission-a (and m3r3a-mines-mission-a) | **IDENTICAL**, 6,001 ticks, every field; mission-a run.hwo sha256 prefix in the return value |
| ai-a, ai-b | ai 400 | m3r2i-e-ai-a; each other | IDENTICAL, 401 ticks |
| ai-soak | ai 4,000 | m3r2i-e-ai-soak | the 20 fields other than rng_all IDENTICAL over 4,001 ticks; rng_all from 1316 (decision 84, below); PASS, ASan 0, no fatal |
| lfe2, armed8 | skirmish 600 | m3r3a-mines-lfe2 / -armed8 | IDENTICAL, 601 ticks |
| r17, r18 | replay oracle01-r6 / -r18 | m3r3a-mines-r17 / -r18 | IDENTICAL (9,601 ticks + 4,800 packets / 3,527 + 1,763); against GOG (R10's field list without rng6 / real): first divergent **2823** ships_vel / **2871** ships_vel |
| play01, play02 | replay hwirec01 / hwirec02 | m3r2i-e-play01-a / -play02-a | IDENTICAL, 4,801 ticks + 2,400 packets each |
| missiontest-a, -b | missiontest Mission01 NIS-first | m3r3a-mines-missiontest-a; each other | IDENTICAL, 11,426 ticks; 6/6, done 11,424 |
| m2-a .. m17-a | missiontest 2, 3, 4, 5, 10, 11, 12, 14, 16, 17 (every mission whose level loads a missile corvette) | m3r3a-mines-m{2,3,4,10,12,14}, m3r2i-e-m{5,11,16,17}-a | all IDENTICAL; m3-b = m3-a |

The fleets: bMissileCorvette is loaded in the ai scene (6 ships), both skirmishes, both oracle replays and both hwi
recordings, Missions 04, 05, 10, 11, 14, 16, 17; p1MissileCorvette in Missions 02, 03, 04, 10, 11, 12, 14; the headless
Mission01 stream has none (no Mission01 type gets an added ribbon).

**The random stream, measured directly.** The headless runs above may draw no corvette's trail at all (the sensors
manager is up), so the captures are the test: the trail view logs every random stream's state each frame (`rs=`). In
four runs of the ai capture (m3r3-et-rs-on, -rs-on2: the feature; -rs-exe: /exeTrails; -rs-rev: the reverted build),
with the four (or two) ribbons drawn on #302 every frame from tick 100 to 420, **the trail streams 13-17
(RANDOM_TRAILS_0..4) hold the same state in all four runs at every one of the 321 frames** (and do advance over them);
the only stream that ever differs is **11, RANDOM_SOUND_GAME_THREAD** (from tick 254 / 350 / 352 between any two runs,
even two runs of the same binary and switches). That is the rng_all noise of decision 84 (the sound side), and the
reason the rng_all of same-look capture runs, and of the soak, differs from run to run (c-exe vs c-exe2 at 181, c-on
vs c-on2 at 246; at 4K two reverted runs at 251). So: **no rng_all move is caused by this change**; none is stated.

### 4e. The dumps (1024x768, 144 fps virtual; whole / crc= stripped, sha256 prefix)
| run (m3r3-et-) | whole | stripped | frames | baseline |
|---|---|---|---|---|
| motion-noi | c503c869f0dc9cdb | 3016280e9e0dd3c8 | 1,431 | m3r1i-motion-noi: equal |
| motion-on | c0e411662fa2f527 | 428ef41479eb5749 | 1,431 | m3r1i-motion-on: equal |
| nis-noi | 86cb155acffc180e | 196bab32d244f614 | 1,440 | m3r1i-nis-noi: equal |
| nis-on | 0307f3be2044c29c | a5de13a44b8d264f | 1,440 | m3r1i-nis-on: equal |
| motion-optout (letterboxBehind 1, /lodBands /noInterp) | 183681235a3fdc7b | 3016280e9e0dd3c8 | | R10 /noInterp: equal |
| nis-optout | 13ca70eea998e5de | 196bab32d244f614 | | R10 /noInterp: equal |
| motion-exe (/exeTrails) | c0e411662fa2f527 | 428ef41479eb5749 | | = motion-on |

Position columns IDENTICAL and **0 crc= moves** (no missile corvette is in those scenes).

### 4f. Self-tests and plain-harness runs
| label (m3r3-et-) | mode | milestone | first stop | result |
|---|---|---|---|---|
| render | render 600 | render self-test done | none (rc 0) | PASS, ASan 0 |
| hud | hud 600 | hud self-test done | none | PASS, ASan 0 |
| meshes | meshes | mesh self-test done | none | 1,758 files, 0 bad, ASan 0 |
| toggle / toggle-lb (/letterboxBehind) / toggle-exe (/exeTrails) | toggle | toggle self-test done | none | PASS x3, ASan 0; the settings line `engineTrails=0 (switch) ... switches=/exeTrails` |
| console | console 400 | console self-test done | none | PASS: `table rows=38 shown=38 set=31 commands=8 failures=0` (M3-R3 before: 37 / 37 / 30) |
| fescreens | fescreens | fescreens self-test done | none | PASS x2 (options unchanged: no control added) |
| fe | fe 120 | Main_game_screen | still running at the timeout (as every fe run) | ASan 0; settings line `... engineTrails=1 (default) ...` |
| mission-a/-b, missiontest-a/-b, m2..m17, ai-a/-b, ai-soak, lfe2, armed8, r17, r18, play01, play02 | (4d) | each mode's done | none | all PASS, ASan 0 |
| sp | sp 1200 | universe running | still running at the 1200 s timeout (as M3-R2) | reaches Mission12 (12 animatics skipped), ASan 0, no leak |
| sp-4k | sp 1200, 3840x2160 | universe running | still running at the timeout | reaches Mission11 (11 animatics), ASan 0, no leak |
| final-mission-a / -b | mission 6,000 on the final binary (the trail view's rs= logging added after the batch) | oracle mission run done | none | IDENTICAL to m3r1i-mission-a in every field, run.hwo sha256 a549f68d3a06bad5, ASan 0 |
| c-on/-exe/-rev, c4k-on/-exe/-rev, p1rest, p1rest4k, p1mv-on/-exe, p1mv4k-on/-exe, rs-* | captures | ai / mission self-test done | none | ASan 0 |

## 5. The 4K cost (subsystem perf, a GPU measurement)
The release binary (build/release/hwi-engine, the final tree), `--mode ai --ai-ticks 500 --res 3840x2160 --lodstats
150-450:fleet` (the camera on every ship of the 156-ship fleet, 164 objects, 25,169 vertices / 46,255 polygons drawn a
frame, 301 frames, FULL DETAIL on, batched meshes), alternating the default and `-- /exeTrails`. The GPU was held by the
user's game (Helldivers 2, 56-73%) from 09:06 to 10:02: one claude-ping, then 10-minute watchers until it stayed <= 40%
for 120 s (10:04, the game still open, idle). nvidia-smi before 3%, between runs 3 / 9 / 8 / 0%, after 0%; CPU load
average 8.5 -> 8.1 (1 min) on 24 cores, the 15-minute average still 25-26 from the hour before.

| run (m3r3-et-) | look | median ms | p99 ms | max ms |
|---|---|---|---|---|
| perf-on1 | four streams (default) | 3.524 | 5.611 | 5.822 |
| perf-exe1 | /exeTrails | 3.572 | 5.313 | 6.309 |
| perf-on2 | four streams (default) | 3.073 | 5.151 | 5.603 |
| perf-exe2 | /exeTrails | 3.345 | 5.001 | 5.808 |

The four streams cost nothing above the run-to-run spread (median -0.05 / -0.27 ms, p99 +0.30 / +0.15 ms against the
neighbouring /exeTrails run; the two default runs differ by 0.45 ms of median between themselves): the six corvettes'
four added ribbons are at most 6 x 2 x 20 segments. Every run meets the 144 Hz budget (6.94 ms) at p99. (The p99 is from
a moderately loaded CPU, load 8 of 24; M3-R2's idle-machine p99 4.5-5.1.) claude-gpu-done run after the last one.

## 6. What changed, file by file
- **src/Game/Trails.c**: the HWI-FEATURE(engine-trails) block (hwiEngineTrails, hwiEngineTrailsSwitch,
  hwiTrailSplits[], hwiEngineTrailsExeSwitch, trailRandom with the added ribbons' xorshift32, hwiTrailSplitOn,
  hwiEngineTrailsSplitCreate); trailNew clears hwiSplit / hwiSplitType; trailGetNozzleOffset's body is now
  trailNozzleOffset(.., split) (split FALSE = the exe's, 0x005a4cc0) with the circle's centre for a split ribbon;
  hwiTrailWidth (half the exe's width across the pair) read by trailHeightScalar / trailHalfWidth;
  trailDrawBillboardedSquareThingz (0x005a5be0) and trailSurpriseColorAdjust (0x005a6c30) draw through trailRandom, the
  sprite skipped for an added ribbon; trailDraw (0x005a7310) is a wrapper over trailDrawBody (an added ribbon only with
  the feature on, with its own generator; a split pair's glow at the exe's nozzle); HWI-TEST hwiTrailHeadWorld.
- **src/Game/Trails.h**: shiptrail gains `sbyte hwiSplit; ubyte hwiSplitType;` before the segments (a heap struct, not
  pinned, not saved: SaveGame clears the trail pointers and InitializeEngineTrails remakes them on load); the feature's
  and the test's prototypes.
- **src/Game/UnivUpdate.c**: InitializeEngineTrails (0x00541f40) ends with `hwiEngineTrailsSplitCreate(newship,
  numTrails)` (one HWI-FEATURE line; the exe's loop untouched).
- **src/SDL/main.c**: `/exeTrails` (ExeTrailsSet, HWI-FEATURE) and `/catTrailView` (CatTrailViewSet, HWI-TEST), with
  their help strings; includes Trails.h.
- **src/Game/Console.c**: the row `engineTrails` (CAT_CON_HWI_ROW_ID + 8), CAT_CON_HWI_ROWS 9.
- **src/Game/Instrument.c**: the hwiSettings[] row `engineTrails` (value, /exeTrails' state, its name; no options-file
  row); a console change ends the switch's state.
- **src/SDL/render.c / render.h**: the HWI-TEST /catTrailView (camera, the per-frame trailview line with the heads,
  nozzles, circles and the 27 random streams' states); includes CommandWrap.h, Trails.h, Randy.h.
- **tools/m2_harness.sh**: header only (the /catTrailView paragraph under the missiontest mode's M3-R3 notes; edited with no run in flight).
- **tools/play.sh**: the header's "Engine trails" paragraph (the switch, the console row, the settings line).
- **ghidra/delta_map_review.json** (7 notes: InitializeEngineTrails, trailNew, trailGetNozzleOffset, trailUpdate,
  trailDrawBillboardedSquareThingz, trailSurpriseColorAdjust, trailDraw; classes unchanged: trailNew and trailUpdate
  compared again with the exe and still IDENTICAL, the other five MODIFIED as before), **ghidra/delta_map.json**
  (`python3 -I tools/delta_map.py merge --done m3r3:engine-trails`: records changed 7; IDENTICAL 2,757, MODIFIED 1,580,
  REWRITTEN 83, reviewed 3,582), **docs/m2-r1-function-deltas.md** (`tools/delta_map.py report`: the done marker).
- **docs/m3-r3-engine-trails.md**: this report.

Builds: `tools/build.sh --debug` and `--release` green with 0 warnings (-Werror) after every subsystem;
`tools/m2_layout32.sh`: OK (all ILP32 layout asserts hold); the win64 cross build (`ninja -C build/win64`) compiles
(hwi-engine.exe, no warning; no _WIN32 code touched). No CAT-TODO added or closed; the only CAT-TODO naming M3-R3 is
M3-R2's BTG.c:1151 (the integrate's).

## 7. THE USER'S CHECK (for the play-test brief)
`tools/play.sh` (the release build rebuilds itself when older than src/), then Mission03 (or any later mission with
the Turanic raiders' or the Beast's missile corvettes; skirmish against the Beast works too). When a missile corvette
flies (Mission03: when the raiders attack), bring the camera close to it and look at it from behind and a
little above: **four jet streams, one from the centre of each of the four glowing engine circles on its rear**, each
about as wide as its circle; the engine glow as before. For the A/B, start once with `tools/play.sh -- /exeTrails`:
the two broad ribbons of the original game, each centred between an upper and a lower circle. Report: whether the four
read as four from the angles you play at, whether any looks offset from its circle, and whether you prefer them (the
default stays ON unless you say otherwise). The log's settings line says `engineTrails=1 (default)` (`0 (switch)` with
/exeTrails).

## 8. For STATE.md
- HWI-FEATURE(engine-trails): the missile corvettes (bMissileCorvette, p1MissileCorvette) draw four ribbons, one per
  painted engine circle (up +-11.5, right +-17.891, the Eng chunk's z), each half the exe's 42-unit width across the
  pair, the exe's other parameters and glow; default ON; `/exeTrails` (run-only), the console row `engineTrails`, the
  settings line's `engineTrails`; no cfg row, no Options control, no key.
- The exe draws one ribbon per mex Eng chunk (Eng0..Eng3, InitializeEngineTrails 0x00541f40), two for each missile
  corvette; no type has more chunks than the exe draws (max 3): the tree matched the exe.
- The added ribbons draw their render jitter from their own generator, never the shared random stream: no stream
  moved, rng_all included (measured: the trail streams 13-17 equal at every frame with the feature on / off / reverted).
- decision 84 sharpened: the run-to-run rng_all noise is stream 11 (RANDOM_SOUND_GAME_THREAD) alone (the capture runs'
  rs= column).
- HWI-TEST `/catTrailView <tick>:<ShipType>[:d][:e][:s][:r][:m]` (render.c): the camera behind a ship type, the trail
  heads, nozzles, circles and the random streams per frame.

## 9. What is left (owner)
- The user's look at the four streams (the attended play-test; owner: the integrate's brief).
- Other ships with painted multi-engine panels: not surveyed (a row of hwiTrailSplits[] each, measured as in section
  1; owner: M3 polish, only if the user names a ship).
