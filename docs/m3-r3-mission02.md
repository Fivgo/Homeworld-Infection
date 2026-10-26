<!-- done: a-dust-belts, b-hint, c-fence, d-report -->
# M3-R3 producer 3: Mission02 (the dust belts, the BUSHAN-RE hint)

Base 292a6da (the pause-freeze audit's checkpoint). The user's items 1 and 2 (cycle/answers/m3-r3-user.md, read at the
start and again before this report: no later answer changes this producer's scope; the open question "were the dust
clouds missing everywhere (the 3D view too) or only on the tactical map?" is still unanswered).

Subsystems: **(a) dust-belts** (the level's resources, the 3D path, the sensors map and its fog-of-war rule), **(b) hint**
(the exe's trigger conditions, the generated KAS code, the Mission02 self-test's hint checks), **(c) fence** (streams,
replays, dumps, fe / sp, builds), **(d) report**.

## Summary

**Both items are 1-to-1 with cataclysm.exe already. Nothing was ported: no CAT line, no stream moved.** This round adds
only HWI-TEST instrumentation (log lines, one counter, one test switch) that proves it.

- **Item 1, the dust belts.** Mission02's level has **no dust cloud at all**: not in the exe either (its level files have
  no DustCloud / Nebula / GasCloud volume; `dustcloud.dist` is an unused type table every mission directory carries). The
  "dust belts" are the level's **486 decorative Asteroid0 rocks** in 97 cylinder volumes (`decorations`), which the game
  itself calls **"Dust"** (english.script `strMenuSV_DecorativeAsteroids Dust`: the sensors manager's View menu and the
  `OP_Dust` option, cfg `ShowAsteriod0`, default on). The exe's sensors scene (`cat_smViewportRenderScene` 0x0050b4b0)
  draws every one of them as a point **with no fog-of-war test** (the user was right: "key elements that all players get
  to see through the fog of war"), while the harvestable asteroids and dust clouds (`cat_smResourcesDraw` 0x0050ae00) show
  only where the player's grid has been. The tree does exactly this, and the headless runs prove it: every sensors frame
  of Mission02 draws all 486 (871,884 points over 1,794 frames), and a capture with the option off differs from one with
  it on by 247-261 pixels at 1024x768 and 269-275 at 3840x2160. **Why the user did not see them:** each is a single
  1x1 physical pixel (the exe's `primPoint3` sets no point size) in a dim tan-grey (0xff788ea2) over the brown Mission02
  background; at 3840x2160 that is ~275 isolated pixels in 8.3 million. Faithful, but nearly invisible at 4K: a
  question for the user (below), not a fix this round.
- **Item 2, the BUSHAN-RE hint.** It is a **timer**, not a direction or distance test: 2, 3 or 6 minutes of game time
  after Mission02's script starts on VERY EASY, EASY or NORMAL, if no ship of the player's has found the BUSHAN-RE by then;
  FleetIntel then pauses the game, opens the sensors manager centred on her with three ping circles (one on her, two at
  random points within 20,000 of her) and speaks event 0x10002169. **On HARD and VERY HARD the exe creates no timer, so
  the hint never plays.** The user's options file says `DifficultySetting 3` (HARD): the user could not have heard it, in
  the original game either. The tree's generated Mission02 code matches the exe's machine code on every path of it.

## (a) The dust belts (item 1)

### (a1) Are they created: the level against the exe's level load

The exe's path, as the tree's: the level's `MissionSphere` lines (mission02.mission: `ResourceSphere_1..3.missphere`
and `Decorations`) are read through `MissionSphereScriptTable` (exe 0x00883aa8); a `Resources` line is
`scriptSetResourcesCB` 0x004a07b0 (a Sphere / Cylinder / Rectangle volume filled through `AddResourceToSphere` 0x004a05f0
/ `AddResourceToCylinder` 0x004a03e0 / `AddResourceToRectangle` 0x004a0290 and `AddResourceToVolumeAtPosition`
0x0049fcf0; a Nebula volume through `nebGo`), a `MeteorStorm` line Cataclysm's `cat_scriptSetMeteorStormCB` 0x004a0d10.
`univAddAsteroid` 0x00540a30 puts an **Asteroid0 on the minor list only** (`universe.MinorSpaceObjList`: not on the
resource, object or impactable lists, no collision blob), every other type on the resource list. Compared with the tree
(LevelLoad.c 679-815, 396-500; UnivUpdate.c 747-800): the same; both already MODIFIED rows of the delta map
(M2-R8's ports), nothing found different.

Mission02's level files (extracted to `original/scratch-m2/m3r3-mission02/data/`):

| File | Lines | Objects the exe makes |
|---|---|---|
| `decorations` (MissionSphere "Decorations") | 100 `Resources Asteroid` cylinders: 97 `type0.dist` (2 to 10 each, 486 in all; most x 5) [audit: was "97 `type0.dist` x 5", which makes 485: the counts per cylinder are 2-10, sum 486], 2 `type1.dist` x 5, 1 `type2.dist` x 5 | **486 Asteroid0** (type0.dist = Asteroid0 only), 10 Asteroid1, 5 Asteroid2 |
| `resourcesphere_3.missphere` | 3 `MeteorStorm ... type4.dist,140` | 420 meteors (Asteroid4 with the killer-collision drift) |
| any file | `DustCloud`, `Nebula`, `GasCloud` | **none** |

Single-player levels with dust clouds at all: Mission04 (1 sphere x 2), Mission06 (21 spheres x 3, and 8 nebulae) and
Mission08 (1 cylinder x 5); every other mission directory carries a `dustcloud.dist` nobody references.

**Headless (HWI-TEST "missiontest: resources" line, at the level's first tick):**

| Run | Line |
|---|---|
| `m3r3-m02-m2-a` (Mission02) | `asteroids=0/10/5/0/0 meteors=420 minor asteroid0=486 other=0 dustclouds=0/0/0/0 gasclouds=0 nebulae=0` |
| `m3r3-m02-m4` (Mission04) | `asteroids=0/20/4/0/0 meteors=0 minor asteroid0=239 dustclouds=0/1/1/0`: two clouds, type 1 at (-49109, 4834, 7870) and type 2 at (-50419, 6944, 8865), inside the level's sphere at (-50782, 5975, 7468) r 2500 |
| `m3r3-m02-m6`, `m3r3-m02-m8` | see (c) |

Every Mission02 number equals the level files' count. (The M3-R2 run printed nothing about resources; its stream's
`resources` field covers the resource list, so it never counted the minor list.)

### (a2) Are they drawn in the 3D view

There is no dust cloud in Mission02 to draw, in the exe or the tree. The Asteroid0 decorations are drawn in the main
view as the small rocks of the minor render list (`univUpdateMinorRenderList`, within `RENDER_VIEWABLE_DISTANCE` of the
camera). For the levels that have dust clouds the tree's path is render.c:3912-3956 (`cloudRenderSystem`, the cloud
system of Clouds.c) with decision 94's nebula CAT-FIX; nothing there was touched or re-examined this round (no Mission02
reason to). The start-up warnings `Could not locate mex Resources/DustClouds/dustcloudN\Rl0\LOD0\dustcloudN.mex` are
benign (the orchestrator's corrected lead: the archive has no dust-cloud .mex; the exe takes the same default path).

### (a3) The sensors map and the fog of war

The exe's scene, `cat_smViewportRenderScene` 0x0050b4b0, in order: the background (`rndBackgroundRender`) and the
derelict worlds when 0x00892b54 ("Background", `ShowBackground`) is set; the sensor-range blobs; the universe bounds;
the player's fog-of-war boundary (`fowRender`); **then, when 0x00892b48 ("Dust", `OP_Dust`, cfg `ShowAsteriod0`) is set,
every object of `universe.MinorSpaceObjList` through `primPoint3` (0x00572a50: glColor3ub, glBegin(GL_POINTS), the
vertex, glEnd; no point size) in colour 0x0089306c = 0xff788ea2, with no fog-of-war test at all**; the mines
(`cat_smMinesDraw`); the crates (where the live usage count of the grid is above 0); `cat_smResourcesDraw` 0x0050ae00;
the ships; the compass; the pings. `cat_smResourcesDraw`: the meteors (killer-collision attribute) with 0x00892b68
("ShowMeteors"); otherwise, with 0x00892b4c ("ShowResources"), the asteroids no mimic poses as and the dust clouds;
each only where the player's grid has **visited** the cell (`fowSelect`, `fowIsValidW`, `fowGetVisitedW` > 0; outside
the grid hidden in mode 2, the exe's fixed 0x008b8848 = 2; no grid: hidden), drawn as a 1- or 2-pixel point by the
camera distance.

The tree: Sensors.c `cat_smViewportRenderScene` (1843-1938) and `cat_smResourcesDraw` (1698-1742) with
Select.c `cat_selPositionHiddenByFOW` 0x00509400 (visited = TRUE): the same steps, flags, colours and tests. **No
difference: nothing to port.** The fog-of-war rule, stated: the "Dust" points (decorative Asteroid0) ignore the fog of
war and are drawn in every sensors frame from the first one; the harvestable asteroids, the dust clouds and the crates
appear only where the player has been (visited) or is (live); the meteors likewise.

**Evidence** (debug build, 1024x768 unless said; the sensors manager opened by Mission02's own script: the hint's view
in the held-scout run, see (b)):

| Run | Sensors frames | Dust points drawn | Per frame |
|---|---|---|---|
| `m3r3-m02-m2-a` / `-b` (NORMAL, the gate run) | 1,794 | 871,884 | **486** (all) |
| `m3r3-m02-hint-n` (held scout) | 2,229 | 1,083,294 | **486** |
| `m3r3-m02-hint-e` (VERY EASY) / `-hint-h` (HARD) | 1,794 / 1,794 | 871,884 / 871,884 | **486** |

against the resource points (fogged): 55,667 over the gate run's 1,794 frames (31 a frame on average).

Captures of the hint's sensors view (`--shot 5832-6272/40:0`), `ShowAsteriod0 1` (the default) against `--cfg
'ShowAsteriod0 0'`, otherwise identical runs (`tools/ppm_region.py diff`, tol 0):

| Tick | 1024x768 (`m3r3-m02-shot-on` / `-off`) | 3840x2160 (`m3r3-m02-shot4k-on` / `-off`) |
|---|---|---|
| 5832 (the view opening) | 247 px differ, bbox 84,120-994,660, max 162 | 269 px, bbox 716,339-3718,1883 |
| 5952 / 6072 / 6192 (open, the three hint pings) | 259 / 261 / 259 px, max 162 | 275 / 275 px (6072, 6192) |
| 6272 (closed again) | 0 | not captured |

The dust is drawn, and at both sizes it is about the same few hundred single pixels: 0.031-0.033% of the 1024x768 frame [audit: was 0.034%; 247-261 of 786,432 pixels], 0.003%
of the 4K frame. PPMs / PNGs: `original/scratch-m2/m2r2-harness/runs/m3r3-m02-shot*/`,
`original/scratch-m2/m3r3-mission02/png/` (on6072.png: the view with the three yellow hint pings).

## (b) The BUSHAN-RE hint (item 2)

### (b1) The trigger, from the exe's machine code

The Mission02 script is kas2c code compiled into cataclysm.exe (0x005db2a0 .. 0x005e0bb0); the tree's
`src/Missions/SinglePlayer/Mission02.c` is generated from it by `tools/m2_kas2c_gen.py` and verified against the
machine code (the multiset of calls and every 32-bit constant). The hint is three steps:

1. **`Init_Mission02` 0x005e05a0**, at the script's start (tick 1): `kasfMissionSkillGet` 0x00493b90 (`mov eax,
   [0x008941a0]`: the single-player difficulty `catSpDifficultySetting`, 0 VERY EASY, 1 EASY, 2 NORMAL, 3 HARD, 4 VERY
   HARD: the front end's `SP_GameDifficulty` radio buttons; the order from `cat_spGetDifficultyModifiers` 0x00516f90's
   health factors 6, 3, 1, 0.85, 0.7), `cmp eax,3; ja 0x005e093c; jmp [eax*4+0x005e0b9c]`, the table
   0x005e084d / 0x005e08b1 / 0x005e08f5 / 0x005e0929:
   - skill 0 (0x005e084d): `TimerCreateSetStart(0x0004ce34, 120)` (0x78), and the Patrol1-5 fighters scuttled;
   - skill 1 (0x005e08b1): the timer at **180 s** (0xb4), Patrol3-5 scuttled;
   - skill 2 (0x005e08f5): the timer at **360 s** (0x168), Patrol4-5 scuttled;
   - skill 3 (0x005e0929): **no timer**, Patrol5 scuttled; skill 4: past the table (`ja`), **no timer**, nothing scuttled.
2. **`Watch_Mission02` 0x005e0bb0** (0x005e1102 .. 0x005e1155), every script tick: if the global 0x443ccb8 is 0 and the
   global 0x849928e4 is 0 and `TimerExpiredDestroy(0x0004ce34)` (0x004920a0 -> `timTimerExpiredDestroy` 0x0052c290: a timer
   that does not exist returns 0) is true: set 0x443ccb8, send message 0x0004ce34 to FleetIntel (team 0x0a8a880c). The
   global 0x849928e4 is set by **`Watch_Mission02_Destroyer_Find` 0x005df130** when `HasShipBeenDiscovered` of the
   BUSHAN-RE's team is 1 (with the destroyer's switch to the player and message 0x2e80a02): **once she is found the hint
   is off for good**.
3. **`Watch_Mission02_FleetIntel_WaitMessage` 0x005dbac0**: 0x0004ce34 (FleetIntel not busy): timer 3 s,
   `WideScreenInDontCloseSensors(90)`, busy, message 0x004ce352 to itself, then `Watch_Mission02_FleetIntel_Wait`
   0x005dba70 until the 3 s pass; 0x004ce352 (busy): `OtherKASPause`, `UniversePause`, open the sensors manager if it is
   closed, `VectorNear` two random points within 20,000 of the BUSHAN-RE, **PingHint1** on her, **PingHint2** and
   **PingHint3** on the two points, `SetSensorsLookAtPoint` on her, **`SpeechEvent(0x10002169)`**, on to
   `Watch_Mission02_FleetIntel_WaitIntel` 0x005db830, which on the speech's end (`kasfIntelEventEnded` 0x00497b00) closes
   the widescreen, unpauses the universe and the other scripts and closes the sensors manager. The pings stay until
   `kasfPingRemove("PingHint1..3")` (Mission02.c:931).

There is **no direction, distance or position test** anywhere in it: the user's guess ("only triggers if the player starts
searching in the opposite direction") is not the trigger; the only player-side condition is not having found her in time.

### (b2) The tree against it, line by line

| Exe | Tree (generated) | Same? |
|---|---|---|
| Init_Mission02's skill switch, table 0x005e0b9c, 120 / 180 / 360 / none / none | Mission02.c:3269-3299 (`switch (kasfMissionSkillGet())`, cases 0-3, no default) | yes (disassembly read for this report) |
| Watch_Mission02 0x005e1102-0x005e1155 | Mission02.c:3529-3540 | yes (disassembly read) |
| FleetIntel 0x0004ce34 / 0x004ce352 handlers | Mission02.c:402-409 / 593-630 | yes |
| Destroyer_Find 0x005df130 sets 0x849928e4 | Mission02.c:2433-2455 | yes |
| `kasfMissionSkillGet` 0x00493b90, `kasfTimerCreateSetStart` 0x00491f90, `kasfTimerExpiredDestroy` 0x004920a0, `timTimerExpiredDestroy` 0x0052c290, `kasfIntelEventEnded` 0x00497b00 (timeout: the tweakable 0x00894190 = 15.0 = `SUBMESSAGE_SAFETY_TIMEOUT`) | KASFunc.c / Timer.c | yes (decompiles read) |

**No difference: nothing ported, the generator not touched** (`python3 -I tools/m2_kas2c_gen.py --check`: every file
current).

### (b3) The self-test's hint checks (HWI-TEST)

`src/Game/MissionSelfTest.c` (Mission02's block, above `catMtMission02`): `catMtHintWatch` (every tick of Mission02's
level: the first tick the global 0x849928e4 is set, "missiontest: hint watch tick=N the BUSHAN-RE found", and the first
tick the skill's seconds of game time since the script's start have passed, "... the hint timer's N s have run out");
`catMissionSelfTestSpeech` (called by `kasfSpeechEvent`: "speech: event=0x........ tick=N (kasfSpeechEvent)" for every
speech event of every mission test, and "missiontest: hint speech tick=N" for 0x10002169 in Mission02); `catMtHintJudge`
at Mission02's end: "missiontest: hint skill=.. timer=.. kasstart=.. due=.. found=.. hold=.. speech=.. expected=yes|no
fired=yes|no speechevents=N result=PASS|FAIL", PASS when the speech fired exactly when the exe's rule says (the skill
has a timer and she was not found by its end) and not before that end; the result joins the mission's done line. The new
switch **`/catMissionTestHoldScout <tick>`** (src/SDL/main.c; with `/catMissionTest` and `/catMissionTestMission 2`): the
FindDestroyer recon of the scripted player waits until universe tick <tick>.

| Run | Difficulty | Recon | Found | Timer's end | Speech 0x10002169 | Expected | Hint | Mission |
|---|---|---|---|---|---|---|---|---|
| `m3r3-m02-m2-a` / `-b` (the gate pair) | NORMAL (2, the harness default) | at once | tick 1152 | 5762 (360 s) | none | no | **PASS** | 6/6 PASS, 25,936 ticks |
| `m3r3-m02-hint-n` | NORMAL | held to 6400 | 7232 | 5762 | **tick 5824** (the 3 s widescreen wait after the message) | yes | **PASS** | 6/6 PASS, 29,536 ticks |
| `m3r3-m02-hint-e` (`--cfg 'DifficultySetting 0'`) | VERY EASY | at once | 1152 | 1922 (120 s) | none | no | **PASS** | 6/6 PASS, 25,120 ticks |
| `m3r3-m02-hint-h` (`--cfg 'DifficultySetting 3'`) | HARD (the user's) | held to 6400 | 7488 | none (no timer) | none | no | **PASS** | 4/6 FAIL at the 90,000-tick limit: SalvagePod never completes at HARD (the scripted player is built for NORMAL; not the hint's) |

In `m3r3-m02-hint-n` the hint's sensors view is captured (a3): the three yellow ping circles and the look-at on her
(on6072.png). The speech-off mission test ends the speech at once, so the hint's universe pause lasts under a tick
there; in play it lasts the spoken line (or 15 s).

**Could the user have heard it?** No: the user's `Cataclysm.cfg` has `DifficultySetting 3` (HARD), and on HARD the
exe never starts the timer. On NORMAL the user would also have had to leave her undiscovered for 6 minutes of game time.

## (c) The fence

All runs: the plain harness (`tools/m2_harness.sh`: offscreen, silent, a fresh home per run), the debug build of this
tree (ASan / UBSan), labels `m3r3-m02-*` in `original/scratch-m2/m2r2-harness/runs/`. **Every run: ASan errors 0, no
unsuppressed leak.** Streams judged with `python3 -I tools/oracle/hwo_compare.py` (all 21 fields), against M3-R2's exit
runs (the previous audits ran none of these missions; their Mission01 / missiontest / dump runs equal M3-R2's).

**Builds** (final tree): `tools/build.sh --debug` and `--release` green, 0 warnings under -Werror; the Windows cross
build (llvm-mingw, tools/release_win64.sh's options, in `original/scratch-m2/m02-win64`) rc 0, 0 warnings, no `_WIN32`
code touched; `tools/m2_layout32.sh` OK (`CatSmDrawCounts` is an HWI struct, not pinned); `python3 -I
tools/m2_kas2c_gen.py --check` current.

| Stream | Run(s) | Against | Result |
|---|---|---|---|
| headless Mission01 (mission, 6,000 ticks) | `m3r3-m02-mission-a` / `-b` (sha256 e7b718c577a7458d, this build's header) | `m3r1i-mission-a/run.hwo` (1ae66610c3f02410) | **IDENTICAL** both, 6,001 ticks, every field (rng_all included) |
| missiontest Mission01 NIS-first | `m3r3-m02-missiontest-a` / `-b` | `m3r2i-e-missiontest-a` (c14c549ceac7b34f) | **IDENTICAL** both (11,426 ticks); 6/6, done 11,424 |
| Mission02 | `m3r3-m02-m2-a` / `-b` | `m3r2i-e-m2-a` (7497c9e5900b59b0) | **IDENTICAL** both (25,905 ticks compared); 6/6, 25,936 |
| Mission03 | `m3r3-m02-m3` | `m3r2i-e-m3-a` (d737643a58c0f58b) | **IDENTICAL**; 5/5, 17,825 |
| Mission04 (dust clouds) | `m3r3-m02-m4` | `m3r2i-e-m4-a` (9f141e8fb1927d7f) | **IDENTICAL**; 5/5, 20,818 |
| Mission06 (dust clouds, nebulae) | `m3r3-m02-m6` | `m3r2i-e-m6-a` (cc2d450412d30d07) | **IDENTICAL**; 7/7, 31,011 |
| Mission08 (dust clouds) | `m3r3-m02-m8` | `m3r2i-e-m8-a` (4b059b09fffc5dd7) | **IDENTICAL**; 6/6, 29,361 |
| team colours into Mission02 (`--teamcolour --player-colours ff1bca82/ffff5539 --shot 900,1000,2016 -- /catTeamColourTicks 2600`) | `m3r3-m02-tc-user` | `m3r2i-e-tc-user` (0ee8c83b4d7bbc4d) | **IDENTICAL**; teamcolour 7 checks PASS; all 8 captures byte-equal |
| replay 10-17 (oracle01-r6, 1..9601) | `m3r3-m02-r6` | `m3r2i-e-r17-a` (c3b7d873ae8fb692) | **IDENTICAL** (9,601 ticks, 4,800 packets): the rung stays 2823 |
| replay 10-18 (oracle01-r18, 1..3527) | `m3r3-m02-r18` | `m3r2i-e-r18-a` (bd2c316848aff30c) | **IDENTICAL** (3,527 ticks, 1,763 packets): the rung stays 2871 |

The test-only runs (not baselines): `m3r3-m02-hint-n` / `-hint-e` / `-hint-h` (b3), `m3r3-m02-shot-on` / `-off` /
`-shot4k-on` / `-shot4k-off` (a3; `--mission-test-ticks 6400`, so their done lines say FAIL by design: they stop before
the mission ends).

**Dumps** (whole / with the crc= column stripped, first 16 hex of sha256), against M3-R2's exit copies:

| Dump | This round | M3-R2 | |
|---|---|---|---|
| `m3r3-m02-motion-noi` | c503c869f0dc9cdb / 3016280e9e0dd3c8 | the same | **EQUAL** |
| `m3r3-m02-motion-on` | c0e411662fa2f527 / 428ef41479eb5749 | the same | **EQUAL** |
| `m3r3-m02-nis-noi` | 86cb155acffc180e / 196bab32d244f614 | the same | **EQUAL** |
| `m3r3-m02-nis-on` | 0307f3be2044c29c / a5de13a44b8d264f | the same | **EQUAL** |

**fe**: `m3r3-m02-fe`: Main_game_screen at the 120 s timeout, ASan 0. **sp** (`m3r3-m02-sp`, `--timeout 1200`):
the /debugScreenshots loop to Mission12 at the timeout, ASan 0, 23,289 file loads (as M3-R2's `m3r2i-e-sp`); the same
shots per level as M3-R2 (golden-01 .. golden-11: 10, 8, 14, 13, 8, 13, 8, 13, 13, 13, 13; golden-12: 1 against 3, the
wall-clock cut-off under a load average of 14-28); **Mission02's screenshots** are
`settings/ScreenShots/golden-02-00128.jpg` .. `golden-02-00912.jpg` (8, every 112 ticks, after BTG/Mission02.btg).

**No stream and no dump moved**: this round's code is log lines, one draw counter and a test switch, none of which
touches the simulation, the shared random stream or a pixel.

## (d) What changed, file by file

All HWI-TEST (measurement only; 15 `HWI-TEST` lines added); no CAT, CAT-FIX, HWI-FIX or HWI-FEATURE line, because nothing
of the exe's behaviour differed.

- `src/Game/MissionSelfTest.c`: `catMtKasStartTick` (the "kas start missionNN" flow event's tick); Mission02's hint block
  above `catMtMission02` (the exe's rule in its header comment with every address; `catMissionSelfTestHoldScoutSet`,
  `catMissionSelfTestSpeech`, `catMtHintSeconds`, `catMtHintWatch`, `catMtHintJudge`); `catMtPlayMission02` calls the
  watch every tick and holds the FindDestroyer scout until `/catMissionTestHoldScout`'s tick (default -1: no hold, so
  the gate run is unchanged); `catMtResourcesLog` ("missiontest: resources ..." and up to 32 "missiontest: dustcloud
  type=.. pos=(..)" lines once a mission level after the first is up, next to "level objects"); `catMtMissionEnd`:
  "missiontest: sensors scenes=.. dustpoints=.. resourcepoints=.. minor=.." for every mission after the first and, for
  Mission02, the hint's judgement in the result; `#include "Attributes.h"`.
- `src/Game/MissionSelfTest.h`: the two prototypes.
- `src/Game/KASFunc.c` `kasfSpeechEvent` 0x00496eb0: one call, `catMissionSelfTestSpeech(event)`, before the exe's body
  (it returns at once outside a mission test; it only logs).
- `src/Game/Sensors.h` / `Sensors.c`: `CatSmDrawCounts.dustPoints` (HWI struct of the HUD self-test), counted in
  `cat_smViewportRenderScene`'s decorative-asteroid loop.
- `src/SDL/main.c`: the switch `/catMissionTestHoldScout <tick>` (`CatMissionTestHoldScoutSet`, the help line).
- `tools/m2_harness.sh`: the header documents `-- /catMissionTestHoldScout <tick>` and the new missiontest lines (edited
  after every run of this producer had ended, decision 92). No new flag: the switch goes after `--`, as
  `/catMissionTestTrace` does.
- `docs/m3-r3-mission02.md`: this report.

**Self-tests**: the Mission02 self-test is extended (the hint's judgement is part of its result); the switch
`/catMissionTestHoldScout <tick>` is the scripted path that meets the hint's conditions. No new harness mode or flag, no
new setting, key, cfg row, console row, settings-line row or control (no HWI-FEATURE this round: the default is the exe's
behaviour, already in the tree).

**Delta map**: no new review row. Every IDENTICAL body relied on was compared with the exe and already has a review row
saying so: `kasfMissionSkillGet` 0x00493b90 / `kasfMissionSkillSet` 0x00493b80 (M2-R8: the difficulty word 0x008941a0),
`kasfIntelEventEnded` 0x00497b00 (M2-R8), `primPoint3` 0x00572a50 (R2); the MODIFIED ones relied on
(`scriptSetResourcesCB`, `AddResourceToVolumeAtPosition`, `univAddAsteroid`, `kasfTimerCreateSetStart`,
`kasfTimerExpiredDestroy`, `timTimerExpiredDestroy`, `kasfSpeechEvent`) read as their rows say.

**CAT-TODOs**: none closed, none added, none re-targeted. The one that names M3-R3 (BTG.c:1151, the background star's
size) is the integrate's to re-target; the user's question below is the same kind of look decision.

## THE USER'S CHECK (for the play-test brief)

1. **The dust belts** (Mission02, any difficulty): `tools/play.sh`, load or reach Mission02, open the sensors manager
   (Space) at once, before exploring. Look for the dotted arcs of single faint tan-grey dots (the "Dust": the decorative
   asteroid belts, 486 of them), drawn over the whole map from the first moment, unlike the brighter resource dots that
   only appear where your ships have been. Right-click in the sensors manager, "Sensors", "Dust" toggles them (on by
   default; your options file has `ShowAsteriod0 1`). **Report**: whether you can see them at 3840x2160 at all, and
   whether you want them bigger (the question below). If you want a side-by-side, `tools/play.sh -- /uiScale 100`
   changes only the 2D, not these points; a 1920x1080 window (`screenWidth 1920` / `screenHeight 1080`) shows them at
   twice the relative size.
   [audit: also tell us whether what you remember as "dust belts" is the broad grey-blue band across the sensors
   manager's backdrop (the Mission02 background, "Background" in the same "Sensors" menu, cfg `ShowBackground 1`): it
   is drawn in the tree's captures (`original/scratch-m2/m3r3a-mission02/on6072.png`), so it is not missing either.]
2. **The BUSHAN-RE hint**: it **cannot** play on HARD (your options file: `DifficultySetting 3`) or VERY HARD, in the
   original game either. The difficulty in effect when Mission02 BEGINS decides it: the hint's timer is made (or not)
   by Mission02's script at its start and is kept in every save made during Mission02, so a Mission02 save begun on
   HARD has no hint whatever the difficulty is now (loading a save keeps the current difficulty setting, kasLoad
   0x00491450, but not a timer that was never made). To hear it: pick NORMAL (or EASY / VERY EASY) on the front end's
   GAME DIFFICULTY buttons, then start a new campaign or load a save from Mission01 (e.g. "01 - Hiigara") and jump into
   Mission02; there keep every ship away from the BUSHAN-RE for 6 minutes of game time (EASY: 3, VERY EASY: 2; pausing
   stops the clock). Fleet Intelligence then speaks, the game pauses, the sensors manager opens centred on her with three
   ping circles around her. **Report**: whether it played and where the pings were.

## For STATE.md

- Decision (M3-R3 mission02): **Mission02's dust belts are the decorative Asteroid0** (486, the `decorations` sphere's
  97 cylinders; the game's "Dust" view option, `OP_Dust` / `ShowAsteriod0`); the exe draws them in the sensors manager
  as 1-pixel points with no fog-of-war test (`cat_smViewportRenderScene` 0x0050b4b0); the tree already does; no dust
  cloud exists in Mission02 (only Missions 04, 06, 08 have them: 2, 63, 5 created, as their levels say).
- Decision (M3-R3 mission02): **the BUSHAN-RE hint is the exe's timer**: `Init_Mission02` 0x005e05a0 (table
  0x005e0b9c) 120 / 180 / 360 s on VERY EASY / EASY / NORMAL, none on HARD / VERY HARD; cancelled for good when she is
  found (global 0x849928e4, `Watch_Mission02_Destroyer_Find` 0x005df130); the tree's generated code matches; nothing
  ported.
- Names chosen: `/catMissionTestHoldScout <tick>` (HWI-TEST switch), `catMissionSelfTestSpeech`, `catMtHintWatch`,
  `catMtHintJudge`, `catMtResourcesLog`, `CatSmDrawCounts.dustPoints`; log lines "missiontest: resources", "missiontest:
  dustcloud", "missiontest: sensors", "missiontest: hint watch / speech / hint skill=..", "speech: event=.. tick=..".
- Defaults: unchanged (the self-test's scout is held only with the switch).

## What is left, and its owner

- **USER question (look decision, no owner until answered):** draw the sensors manager's points (the Dust, the
  resource and ship dots) at the UI scale's size (2x2 physical pixels at 200%) so they read at 4K as they did at
  1024x768, or keep the exe's 1 physical pixel. Same kind of decision as the background star's size (BTG.c:1151);
  default stays the exe's until the user picks.
- The open question to the user stays: were the dust clouds missing in the 3D view too, or only on the tactical map?
  (Mission02 has none in either; if the user meant something in the 3D view, a capture of what was expected would
  locate it.)
- The Mission02 self-test's scripted player does not finish the mission on HARD (SalvagePod, `m3r3-m02-hint-h`); not a
  gate (the self-tests play NORMAL, the harness default); owner M3 tests if a HARD run is ever wanted.
