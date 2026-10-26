<!-- done: a-code, b-sizes, c-data, d-surfaces, e-fence, f-captures, g-user-check, h-left, review-followup (engineTrails toggle row, toggle2 / console2 runs, decisions recorded) -->
# M3-R4 sensors-points: bigger Dust points and resource markers in the sensors manager (HWI-FEATURE(sensors-points))

Base `ee69987` (branch m2 = main). This report is kept current part by part (the marker above).

## The user's decision (verbatim, 2026-10-26)

> "it looks like the dust particles are rendering to the screen you are right. They are just incredibly tiny so lets do
> as you suggested and double the dust particle size and increase the resource markers by 50% to ensure a difference in
> size."

Background: docs/m3-r3-mission02.md (a3) and its audit: Mission02's "dust belts" are the 486 decorative Asteroid0 of the
`decorations` sphere, the game's "Dust" (`OP_Dust`, cfg `ShowAsteriod0`), drawn by `cat_smViewportRenderScene`
0x0050b4b0 as one physical pixel each in 0xff788ea2 (about 275 pixels of 8.3 million at 3840x2160). docs/m3-r3-summary.md
"What the user must decide" item 3 (the round's question 4) asked whether to draw the sensors manager's points (the Dust,
the resource and the ship dots) at the UI scale's size or keep the exe's 1 pixel. The decision above answers it with fixed
factors for the Dust (x2) and the resource markers (x1.5). It names no ship dots, so the ship blips keep the exe's 2.

## Summary

**ON by default:** the Dust points are 2 physical pixels (the exe's 1, x2). The resource, meteor, mine and crate markers
are 3 (the exe's 2, x1.5). The one marker size the exe gives as 1 pixel (a static at or under the collision radius 45)
is also 3, so no marker is ever the Dust's size. The ship blips keep the exe's 2. A mimic posing as an asteroid is the
one ship blip that grows: the exe draws it in the asteroid colour at a big asteroid's size, so the feature keeps the
disguise at the markers' 3. The sizes are fixed physical pixels: the UI scale does not change them (the user's factors).
`glPointSize` takes a float, but the driver rounds a non-antialiased point's size to an integer, so the sizes are set as
the integers 2.0 and 3.0 (1.5 x 1 pixel would otherwise be whatever the driver rounds 1.5 to).
`/exePoints` (run-only) and the console row `sensorsPoints 0` give the exe's sizes and GL calls exactly.

## (a) What changed, file by file

The exe's base (already ported, unchanged): `cat_smViewportRenderScene` 0x0050b4b0 draws every object of
`universe.MinorSpaceObjList` with `primPoint3` 0x00572a50 (glColor3ub, glBegin(GL_POINTS), the vertex, glEnd; **no point
size**: 1 pixel) when 0x00892b48 ("Dust") is set; `cat_smResourcesDraw` 0x0050ae00 (meteors, asteroids, dust clouds),
`cat_smMinesDraw` 0x0050b380 and the scene's crate loop draw each through the inlined size rule the tree calls
`catSmBigPoint`: `glPointSize(collspheresize > 45.0 (0x0072b0e4) ? 2.0 : 1.0)`, the point, `glPointSize(1.0)`;
`cat_smShipsDraw` 0x0050a310 draws a ship's blip at `glPointSize(2.0)`.

- `src/Game/Sensors.c` (the feature, 29 `HWI-FEATURE(sensors-points)` lines):
  - the block above `catSmBigPoint`: `hwiSensorsPoints` (1 = on, the console row), `hwiSensorsPointsSwitch`
    (`/exePoints` given), `HWI_SM_DUST_POINT` 2.0, `HWI_SM_MARKER_POINT` 3.0, `hwiSensorsPointsExeSwitch()` (the
    switch), `hwiSmMarkerSize(exeSize)` (3 when on, the exe's size when off).
  - `catSmBigPoint`: the exe's size expression is kept as is and passed through `hwiSmMarkerSize`; the exe's
    `glPointSize(1.0)` after the point is unchanged.
  - `cat_smShipsDraw`: `hwiAsResource` is set in the exe's "mimic posing as a resource" branch (the asteroid colour);
    the exe's `glPointSize(2.0f)` becomes `hwiAsResource ? hwiSmMarkerSize(2.0f) : 2.0f`. Every other blip stays 2.0.
  - `cat_smViewportRenderScene`'s Dust loop: when on, `glPointSize(2.0)` before the loop and `glPointSize(1.0)` after
    (the size the rest of the scene expects, as the exe's state there). Off: no added GL call at all.
  - HWI-TEST: `hwiSmCountMarker` (the markers drawn per size into `catSmDrawCounts.markerSizes[]`), `dustSize`, and
    `hwiSmPointsStaticLog` (the statics mode's "sensorspoints: static" lines, (c)).
- `src/Game/Sensors.h`: the externs and prototypes; `CatSmDrawCounts.markerSizes[4]`, `.dustSize` (HWI-TEST; an HWI
  struct, not a pinned layout).
- `src/Game/Console.c`: the row `{ 5, CAT_CON_HWI_ROW_ID + 9, &hwiSensorsPoints, "sensorsPoints", NULL }`;
  `CAT_CON_HWI_ROWS` 9 -> 10 (the console self-test now expects shown = 29 + 10 = 39, set = 22 + 10 = 32).
- `src/Game/Instrument.c`: `#include "Sensors.h"`; `hwiSettings[]` row 9 `sensorsPoints` (value, switch state,
  switch name `/exePoints`, no options-file row); `hwiSettingsConsole`: the console's choice ends `/exePoints`' state
  (as rows 6 and 8 do).
- `src/SDL/main.c`: `ExePointsSet` and the `entryFn("/exePoints", ...)` row after `/exeTrails`.
- `src/Game/MotionTest.c` (the toggle self-test, HWI-TEST): `mtToggleSwitchLabels` enumerates the switch-backed console
  rows; `sensorsPoints` / `/exePoints` added as a no-row switch (as `/meshImmediate`); the table's struct gets a
  `restore` function (the run-only switch's state back) instead of the hard-coded `meshImmediateSwitch()`. After the
  review: the row M3-R3 missed, `{ "engineTrails", &hwiEngineTrails, mtEngTrailsStands, FALSE,
  hwiEngineTrailsExeSwitch }` (`/exeTrails`; `mtEngTrailsStands` returns `hwiEngineTrailsSwitch`; `#include
  "Trails.h"`), tagged `HWI-TEST (M3-R4 sensors-points): the M3-R3 switch the table missed`.
- `src/Game/Universe.c` (HWI-TEST): `catShipStaticsRunSelfTest` logs one "sensorspoints: static" line per mine,
  asteroid, dust-cloud and crate static of the install.
- `src/Game/InfoOverlay.c` / `src/Game/MissionSelfTest.c` (HWI-TEST): the lines "hudtest: sensorspoints markers
  1px=.. 2px=.. 3px=.. dust=.. dustsize=.. sensorsPoints=.." and "missiontest: sensorspoints markers ..", after the
  existing sensors counts (the self-tests' PASS rules are unchanged).
- `tools/play.sh`: the header block "Sensors points" (the switch, the console row, the settings line, the sizes).

Tagged lines (`/bin/grep -r`): 54 `HWI-FEATURE(sensors-points)` (Sensors.c 29, Instrument.c 9, main.c 7, Sensors.h 4,
Console.c 2, MotionTest.c 2, play.sh 1) and 40 `HWI-TEST (M3-R4 sensors-points)`; `git diff --stat`: 10 files changed,
135 insertions(+), 12 deletions(-).

Not changed: no options-file row, no Options control, no key (not asked); `tools/m2_harness.sh` (never edited).

## (b) The sizes per object class (physical pixels)

| Class | Colour | The exe (base, `/exePoints`) | Default (the feature) |
|---|---|---|---|
| Dust (the decorative Asteroid0, minor list) | 0xff788ea2 (162,142,120) | 1 | **2** |
| Big resource: asteroid (Asteroid1-4), dust cloud | 0xff5a78b4 (180,120,90), 0xff6e8cc8 (200,140,110) | 2 | **3** |
| Meteor (killer-collision asteroid) | 0xff9696c8 (200,150,150) | 2 | **3** |
| Small resource (radius <= 45) | | 1 (none exists: (c)) | 3 |
| Mine, level (race Derelict, Beast, Kushan) | 0xff2070ff (255,112,32) | 2 | **3** |
| Mine, small (race Sect, Taiidan: the minelayer's laid mines) | 0xff2070ff | 1 | **3** |
| Crate | 0xff00ffff (255,255,0) | 2 | **3** |
| Mimic posing as an asteroid (a ship blip) | 0xff5a78b4 | 2 | **3** |
| Every other ship blip | | 2 | 2 (unchanged) |

Mines and crates scale with the resources: they share the exe's marker rule (`catSmBigPoint`) and are markers of the
same kind.

## (c) The resource statics' radii from the data (the small-resource list)

Run `m3r4-sp-statics` (`--mode statics`: every static of the install loaded once), the "sensorspoints: static" lines
(collision radius, the smallest a dust cloud shrinks to while harvested, the exe's size, the feature's):

| Static | Radius | Smallest | The exe | Feature |
|---|---:|---:|---:|---:|
| Resources/Asteroids/Asteroid0.shp (the Dust: minor list only, never a marker) | 12.597 | | (1, Dust) | (2, Dust) |
| Asteroid1.shp | 79.315 | | 2 | 3 |
| Asteroid2.shp | 264.718 | | 2 | 3 |
| Asteroid3.shp | 547.813 | | 2 | 3 |
| Asteroid4.shp | 922.352 | | 2 | 3 |
| Resources/DustClouds/DustCloud0.shp | 350.000 | 70.000 | 2 | 3 |
| DustCloud1.shp | 500.000 | 100.000 | 2 | 3 |
| DustCloud2.shp | 800.000 | 160.000 | 2 | 3 |
| DustCloud3.shp | 1300.000 | 260.000 | 2 | 3 |
| RACE_SECT/Mine.shp | 19.053 | | **1** | 3 |
| RACE_BEAST/Mine.shp | 85.737 | | 2 | 3 |
| RACE_KUSHAN/Mine.shp | 85.737 | | 2 | 3 |
| RACE_TAIDAN/Mine.shp | 11.517 | | **1** | 3 |
| RACE_DERELICT/Mine.shp | 85.737 | | 2 | 3 |
| Derelicts/Crate.shp | 85.430 | | 2 | 3 |

**No resource static is at or under 45**: every harvestable asteroid and dust cloud is a 2-pixel marker in the exe, even
a dust cloud harvested to its smallest (`dustcloudMinShrink`; Cataclysm's asteroids never shrink, ResCollect.c). So no
resource class would have ended up at the Dust's 2 pixels; the 1-pixel rule matters only for the Sect and Taiidan mines
(Mission03's laid mines are the Taiidan minelayer's, docs/m3-r3-mines.md). Those are 3 under the feature too (ceil(1.5)
would be 2, the Dust's size).

## (d) The surfaces

| Surface | Name | Default |
|---|---|---|
| run-only switch | `/exePoints` (src/SDL/main.c `ExePointsSet`; no options-file row, as `/exeTrails`, `/meshImmediate`) | not given |
| console row | `sensorsPoints 0|1` (src/Game/Console.c, id `CAT_CON_HWI_ROW_ID + 9`; `CAT_CON_HWI_ROWS` 9 -> 10) | 1 |
| settings line | `sensorsPoints=1 (default)`; `0 (switch)` with `/exePoints`; `(console)` after the row (src/Game/Instrument.c `hwiSettings[]` row 9; the console's choice ends the switch's state) | 1 |
| tools/play.sh header | "Sensors points (M3-R4, HWI-FEATURE(sensors-points))": the switch, the row, the settings line, the sizes | |
| toggle self-test | `mtToggleSwitchLabels` rows `sensorsPoints` (`/exePoints`) and, after the review, `engineTrails` (`/exeTrails`, M3-R3's): each flipped by its console row, the line checked for `(console)`, the run-only switch's state put back by its own switch function | |
| options-file row / Options control / key | none (not asked) | |

## (e) The fence

Every engine run went through `tools/m2_harness.sh` (offscreen, silent, a fresh home per run), labels `m3r4-sp-*` in
`original/scratch-m2/m2r2-harness/runs/`, on the final tree's debug build (ASan + UBSan). The fence's runs below are the
final binary's. A first pass of the same runs on the binary before the last two HWI-TEST log lines gave the same results
(its counts are kept in `original/scratch-m2/m3r4-sensors-points/prelim/`); those run directories were then replaced.

**(a) Builds.** `tools/build.sh --debug --clean` and `--release --clean` on the final tree: both rc 0,
`[274/274] Linking target hwi-engine`, **0 `warning:` lines** under `-Werror` (logs
`original/scratch-m2/m3r4-sensors-points/build-{debug,release}.log`).

| build | size | sha256 (first 16) |
|---|---:|---|
| debug (ASan + UBSan) | 91,194,856 B | `9f0c73ec37e5e1c1` |
| release (`-O2`) | 5,787,952 B | `dca288f5464d6b3a` |

After the review's follow-up (the `engineTrails` row in MotionTest.c, HWI-TEST only) both were built again from clean:
rc 0, `[274/274]`, 0 warnings (logs `build-{debug,release}-2.log`): debug 91,195,080 B `66f176d0fe0b8305`, release
5,788,056 B `538bcffddc97742c`; `tools/m2_layout32.sh` OK. The toggle2 / console2 runs below are that debug binary's;
the other runs of this section are the first binary's (the follow-up changes only the toggle self-test's code).

`tools/m2_layout32.sh`: `m2_layout32: OK (all ILP32 layout asserts hold)` (45 ship-specifics files). No pinned struct
was touched (`CatSmDrawCounts` is an HWI struct). No `_WIN32` code was touched; the Windows cross build was not run.

**(b) The captures** (the mission02 audit's recipe: `--mode missiontest --mission 2 --mission-test-ticks 6400 --shot
5832-6272/40:0 -- /catMissionTestHoldScout 6400`, `--res 3840x2160` for 4K; the hint's sensors view, opened by
Mission02's own script at tick 5824; 12 captures a run). `-exe` adds `/exePoints`.

| run | size | settings line | ASan | missiontest |
|---|---|---|---|---|
| `m3r4-sp-shot-exe` | 1024x768 | `sensorsPoints=0 (switch) ... switches=/exePoints` | 0 | `sensorspoints markers 1px=0 2px=8700 3px=0 dustsize=1` |
| `m3r4-sp-shot-on` | 1024x768 | `sensorsPoints=1 (default) ... switches=none` | 0 | `sensorspoints markers 1px=0 2px=0 3px=8700 dustsize=2` |
| `m3r4-sp-shot4k-exe` | 3840x2160 | `sensorsPoints=0 (switch)`, `uiScale=0->200%` | 0 | `markers 1px=0 2px=8700 3px=0 dustsize=1` |
| `m3r4-sp-shot4k-on` | 3840x2160 | `sensorsPoints=1 (default)`, `uiScale=0->200%` | 0 | `markers 1px=0 2px=0 3px=8700 dustsize=2` |

All four: `sensors scenes=435 dustpoints=211410 resourcepoints=8700 minor=486` and the hint `PASS` (speech at 5824),
as M3-R3's runs.

**The pixel counts** (exact colour, tolerance 0). Each capture was counted twice: by `tools/ppm_region.py stats FILE
--colour r,g,b` and by an independent P6 reader of a few lines (`sp_count.py`, which also sizes each connected blob of
the colour). The two agree on every one of the 192 counts (48 captures x 4 colours).

| tick | 1024x768 Dust exe / on (x) | asteroid exe / on (x) | dust cloud | meteor exe / on (x) |
|---|---|---|---|---|
| 5832 | 247 / 979 (3.96) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 5872 | 261 / 1031 (3.95) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 5912 | 261 / 1031 (3.95) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 5952 | 259 / 1027 (3.97) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 5992 | 261 / 1029 (3.94) | 44 / 99 (2.25) | 0 / 0 | 36 / 77 (2.14) |
| 6032 | 261 / 1029 (3.94) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 6072 | 261 / 1031 (3.95) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 6112 | 261 / 1031 (3.95) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 6152 | 261 / 1031 (3.95) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 6192 | 259 / 1027 (3.97) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 6232 | 261 / 1029 (3.94) | 44 / 99 (2.25) | 0 / 0 | 36 / 77 (2.14) |
| 6272 (sensors closed) | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 |

| tick | 3840x2160 Dust exe / on (x) | asteroid exe / on (x) | dust cloud | meteor exe / on (x) |
|---|---|---|---|---|
| 5832 | 269 / 1071 (3.98) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 5872 | 275 / 1100 (4.00) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 5912 | 275 / 1100 (4.00) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 5952 | 275 / 1097 (3.99) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 5992 | 275 / 1100 (4.00) | 44 / 99 (2.25) | 0 / 0 | 34 / 79 (2.32) |
| 6032 | 275 / 1100 (4.00) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 6072 | 275 / 1100 (4.00) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 6112 | 275 / 1100 (4.00) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 6152 | 275 / 1100 (4.00) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 6192 | 275 / 1097 (3.99) | 44 / 99 (2.25) | 0 / 0 | 36 / 81 (2.25) |
| 6232 | 275 / 1100 (4.00) | 44 / 99 (2.25) | 0 / 0 | 34 / 79 (2.32) |
| 6272 (sensors closed) | 1 / 1 | 0 / 0 | 0 / 0 | 0 / 0 |

Reading them:
- **The Dust: 4x** (1x1 to 2x2). At 4K the 275 single pixels become 270 separate 2x2 blobs plus 2 of 6 and 1 of 8
  pixels (neighbours that touch): 1,100 pixels, exactly 4.00x. At 1024x768 the points lie closer together, so more of
  them overlap (3.94-3.97x).
- **The resource markers: 2.25x** (2x2 to 3x3). The asteroid colour is 11 blobs of 4 pixels under `/exePoints` and 11
  blobs of 9 by default at 4K (at 1024x768 two of them touch: 9 of 9 and 1 of 18). The meteors: 9 blobs of 4 to 9 of 9.
  Where a ship or a line lies on top of one marker (5992 / 6232) the count is a few pixels off the ratio.
- **The dust-cloud colour 0xff6e8cc8 is 0 everywhere**: Mission02 has no dust cloud (docs/m3-r3-mission02.md (a1)); its
  markers go through the same `catSmBigPoint` as the asteroids.
- The one 4K pixel of the Dust colour at 6272 (sensors closed) is in the main view, the same in both runs.

**Nothing else moved.** Every pixel that differs between a default capture and its `/exePoints` twin is a point colour
in the default (`sp_diff.py`: 4K 6072: 925 pixels differ = Dust 825 + asteroid 55 + meteor 45, **non-point 0**; every
capture of both sizes: non-point 0; 6272: 0 pixels differ). The yellow hint pings (255,255,0) count the same in both.

**`/exePoints` is the exe's look, byte for byte:** every `m3r4-sp-shot-exe` capture is byte-equal to M3-R3's exit-test
capture `m3r3i-e-m02-shot-on` (the 6ad69ec binary, before this feature): 12 / 12; every `m3r4-sp-shot4k-exe` capture to
`m3r3i-e-m02-shot4k-on`: 12 / 12. The default captures differ from those in 11 of 12 (equal at 6272, sensors closed).
The four capture runs' Mission02 streams are IDENTICAL to M3-R3's (`hwo_compare.py`, 6,400 ticks, rc 0; 4 bytes
differ, the header's build identity).

**(c) The self-tests** (final binary; ASan errors 0 and no unsuppressed leak in every run, rc 0):

| run | result |
|---|---|
| `m3r4-sp-hud` (`--mode hud`) | `hudtest: done ... result=PASS`; `hudtest: sensorspoints markers 1px=0 2px=0 3px=0 dust=0 dustsize=2 sensorsPoints=1` (Mission01's sensors frames have no visited resource and no Dust) |
| `m3r4-sp-hud-exe` (`-- /exePoints`) | `result=PASS`; `dustsize=1 sensorsPoints=0`; the settings line `sensorsPoints=0 (switch)` |
| `m3r4-sp-console` | `consoletest: done ... shown=39 set=32 ... failures=0 result=PASS` (was 38 / 31: `CAT_CON_HWI_ROWS` 10); `table rows=39`; kas `tablerows=486` (unchanged) |
| `m3r4-sp-toggle` | `toggletest: done start=1/1 fails=0 result=PASS`; `switch label sensorsPoints switch=0 -> expect 'sensorsPoints=0 (console)' line=yes` |
| `m3r4-sp-toggle-exepts` (`-- /exePoints`) | `result=PASS`; `switch label sensorsPoints switch=1 -> expect 'sensorsPoints=1 (console)' line=yes`; start line `sensorsPoints=0 (switch) ... switches=/exePoints` |
| `m3r4-sp-fescreens` | `fescreentest: done files=22 ... screens=195 ... wrong=0 ... result=PASS`; `optionstest: done options=35 ... result=PASS` (no Options control added) |
| `m3r4-sp-statics` | `shipstatic: self-test done, 143 of 145 ship types loaded`; 15 "sensorspoints: static" lines ((c) above) |

After the review's follow-up (the `engineTrails` row; the second binary; ASan errors 0, no unsuppressed leak, rc 0 in
all four):

| run | switch | switch-label lines | result |
|---|---|---|---|
| `m3r4-sp-toggle2-default` | none | `sensorsPoints switch=0 -> expect 'sensorsPoints=0 (console)' line=yes`; `engineTrails switch=0 -> expect 'engineTrails=0 (console)' line=yes` | `toggletest: done start=1/1 fails=0 result=PASS` |
| `m3r4-sp-toggle2-exetrails` | `/exeTrails` | `sensorsPoints switch=0 -> ... line=yes`; `engineTrails switch=1 -> expect 'engineTrails=1 (console)' line=yes`; the last line `engineTrails=0 (switch) ... switches=/exeTrails` (the switch put back) | `result=PASS` |
| `m3r4-sp-toggle2-exepoints` | `/exePoints` | `sensorsPoints switch=1 -> expect 'sensorsPoints=1 (console)' line=yes`; `engineTrails switch=0 -> ... line=yes`; the last line `sensorsPoints=0 (switch) ... switches=/exePoints` | `result=PASS` |
| `m3r4-sp-console2` | none | `consoletest: table rows=39 shown=39 set=32 commands=8 failures=0` | `consoletest: done ... shown=39 set=32 ... failures=0 result=PASS` |

**(d) The Mission01 stream** (`--mode mission --mission-ticks 6000`, the default settings, `sensorsPoints=1 (default)`):

| run | sha256 (first 16) / size | vs `m3r3i-e-mission-a/run.hwo` (M3-R3's exit test, `c20ccda7fdae0f66`) |
|---|---|---|
| `m3r4-sp-mission-a` | `78570a643d0312cd` / 1,476,370 B | **IDENTICAL**: 6001 ticks and 0 packets compared (ticks 0-6000), no difference; rc 0 |
| `m3r4-sp-mission-b` | `78570a643d0312cd` / 1,476,370 B | **IDENTICAL**, the same; a = b byte for byte |

The tool compares every field both files fill (21, `rng_all` included). Beyond it: of the 1,476,370 bytes only 4 differ
from the baseline, offsets 17-20, the header's build identity (`6ad69ec5` against `ee69987b`, the tool's "HEADER: same
producer, different build identity"). The simulation did not move, as a render-only change must not.

**(e) fe**: `m3r4-sp-fe`: milestone `Main_game_screen`, still running at the 120 s timeout (rc 124 by design, as M3-R3's
`m3r3i-e-fe`), ASan errors 0, no unsuppressed leak.

## (f) The captures' paths

- The PPMs: `original/scratch-m2/m2r2-harness/runs/m3r4-sp-{shot-exe,shot-on,shot4k-exe,shot4k-on}/shot.<tick>.0.ppm`
  (ticks 5832 .. 6272 every 40).
- The PNGs (`original/scratch-m2/m3r4-sensors-points/png/`): `4k-6072-resources-exe-vs-on-x3.png` (the asteroid and
  meteor markers and the hint's look-at, `/exePoints` left, default right, 3x), `4k-6072-dust-exe-vs-on-x4.png` (a
  patch of Dust, 4x), `4k-6072-full-exe.png` / `4k-6072-full-on.png` (the whole 4K frames), `1024-6072-exe-vs-on.png`.
- The counts: `original/scratch-m2/m3r4-sensors-points/counts-ppm_region.txt`, `counts-independent.txt`,
  `diff-classes.txt`, `counts-table.txt` (the join: `keys=192 disagree=0`); the stream compares
  `compare-mission-{a,b}.txt`; the build logs `build-{debug,release}.log`; the counting scripts
  (`sp_count.py`, `sp_diff.py`, `sp_table.py`, `sp_png.py`) in `scripts/`, run with `python3 -I`.

## (g) THE USER'S CHECK (for the next play-test)

Launch `tools/play.sh` as usual (3840x2160) and reach Mission02. Open the sensors manager (Space) before exploring.
The Dust arcs should now read as dotted lines of small tan-grey 2x2 dots across the whole map. The orange-tan resource dots
(asteroids, where your ships have been) and the pinkish meteor dots should be visibly bigger (3x3) than the Dust. The ship blips
should look as before. For the A/B, quit and start once with `tools/play.sh -- /exePoints` (the original 1 / 2 pixel
points; the log's `hwi-settings:` line says `sensorsPoints=0 (switch)`). **Report**: whether the Dust is now visible
enough at 4K, whether the resource markers read as clearly bigger, and whether you would want either larger.

## (h) What is left

Decided at the review (the reviewer's decisions, 2026-10-26), no longer open:
- **The small mines stay at 3 pixels** (Sect and Taiidan, radius 19.1 / 11.5: the Taiidan minelayer's laid mines in
  Mission03). Visibility at 4K is the point of the item; the exe's 1-against-2-pixel difference between laid and level
  mines is not something a player reads.
- **The mimic posing as an asteroid stays at the markers' 3 pixels**: a disguise drawn smaller than the asteroids it
  imitates would give it away, which the exe never does. (Reasoned from the code, `cat_smShipsDraw`'s branch and
  `cat_mimicAppearsAsResource` 0x005b9860; no capture shows one: none in Mission02's sensors view.)
- **Fixed pixel sizes, not scaled by the UI scale**, as the user's words.

Closed:
- **The toggle self-test's missing `engineTrails` / `/exeTrails` row** (M3-R3 engine-trails added the switch but not
  the row): added after the review, PASS default, under `/exeTrails` and under `/exePoints` ((e) above).

Still open:
- **Dust clouds and crates** are not in Mission02's view; their markers take the same path as the asteroids (the
  statics list (c), the marker counter: every one of the 8,700 markers drawn at 3).
- The Windows cross build was not run (no `_WIN32` code touched).
- No CAT-TODO names this item; none added, none closed. No delta-map row: the four sensors functions touched
  (0x0050a310, 0x0050ae00, 0x0050b380, 0x0050b4b0) are Cataclysm-new and have no row; `primPoint3` 0x00572a50
  (IDENTICAL) is not edited.
