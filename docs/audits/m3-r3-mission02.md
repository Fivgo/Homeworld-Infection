# Audit: M3-R3 mission02 (the dust belts, the BUSHAN-RE hint)

Auditor's labels: `m3r3a-mission02-*` in `original/scratch-m2/m2r2-harness/runs/`. Every engine run went through
`tools/m2_harness.sh` (offscreen, silent, a fresh home per run) with `--bin` on the auditor's clean debug build. The answers
file `cycle/answers/m3-r3-user.md` was read at the start and again before this report: no later answer changes items 1
and 2 (the open question "the 3D view too, or only the tactical map?" is still unanswered). Base 292a6da; the port is the
working tree on top of it (7 files of src/ and tools/, the report): no commit after 292a6da is port work.

## 1. Builds (from clean, `original/scratch-m2/build-audit-m3r3-mission02`)

| Build | Flags (as tools/build.sh / tools/release_win64.sh) | Result | Size |
|---|---|---|---|
| debug | `--buildtype=debug -Db_sanitize=address,undefined`, werror | rc 0, 0 warnings | 90,900,448 B |
| release | `-Doptimization=2 -Ddebug=false -Db_sanitize=none`, werror | rc 0, 0 warnings | 5,768,032 B |
| win64 (llvm-mingw cross, the script's cross files, `-Dmovies=false`) | | rc 0, 0 warnings | 5,376,000 B (hwi-engine.exe) |

No new source file, no GL call, no `_WIN32` code touched. `tools/m2_layout32.sh`: OK. `python3 -I tools/m2_kas2c_gen.py
--check`: every generated file current (the port did not touch the generator or its output).

## 2. The exe, read by the auditor

- **Level data** (extracted from cataclysm.big to `original/scratch-m2/m3r3a-mission02/`): mission02.mission's four
  MissionSphere files; no `DustCloud`, `Nebula` or `GasCloud` line in any of them. `decorations`: 100 `Resources Asteroid`
  cylinders, 97 type0.dist (counts 2-10 each: 79 of them 5; **486** in all), 2 type1.dist x 5, 1 type2.dist x 5;
  type0/1/2.dist are Asteroid0/1/2 only; resourcesphere_3: 3 MeteorStorm x 140 (420). Every single-player level file
  scanned: dust clouds only in mission04\general (1 sphere x 2), mission06\general (21 spheres x 3, plus 8 Nebula lines)
  and mission08\resourcesphere_1 (1 cylinder x 5). Producer's statement correct except the "x 5" (finding 1).
- **univAddAsteroid** 0x00540a30 (decompile): an Asteroid0 goes to `MinorSpaceObjList` only and returns; other types to
  the blob, object, resource and impactable lists. The tree (UnivUpdate.c) the same.
- **cat_smViewportRenderScene** 0x0050b4b0 (decompile): after `fowRender`, `if (DAT_00892b48 != 0)` every node of
  `MinorSpaceObjList` -> `primPoint3(pos, DAT_0089306c)` with no fog-of-war call; then mines, crates, resources, ships.
  `0x00892b48` = 1 (`ShowAsteriod0` / `OP_Dust`, menu string `strMenuSV_DecorativeAsteroids Dust`), `0x0089306c` =
  0xff788ea2. The tree's Sensors.c 1880-1892 the same (plus the producer's HWI-TEST counter).
- **cat_smResourcesDraw** 0x0050ae00: meteors with 0x00892b68; asteroids (no mimic) and dust clouds with 0x00892b4c;
  each only where `fowGetVisitedW > 0` (outside the grid hidden when 0x008b8848 = 2 > 1; no grid: hidden; 0x008b8848 = 0:
  always), point size 2 above collision radius 45.0 (0x0072b0e4 = 0x42340000) else 1. The tree (Sensors.c 1698-1742 with
  Select.c `cat_selPositionHiddenByFOW`, compared with 0x00509400's decompile) the same.
- **The hint** (`objdump` listing `m2r2-mesh-tools/exe.dis`, the decompile): `Init_Mission02` 0x005e0838 `call
  kasfMissionSkillGet` (0x00493b90 = `mov eax,[0x8941a0]; ret`), `cmp eax,3; ja 0x5e093c; jmp [eax*4+0x5e0b9c]`, table
  0x005e084d / 0x005e08b1 / 0x005e08f5 / 0x005e0929; `push 0x78 / 0xb4 / 0x168; push 0x4ce34; call 0x491f90`
  (kasfTimerCreateSetStart) on skills 0 / 1 / 2, none on 3 (only Patrol5's scuttle) or 4 (past the table). `0x4ce34` is
  pushed nowhere else in the exe but the FleetIntel compare 0x5dbb78 and Watch_Mission02's 0x5e111a / 0x5e113a.
  `Watch_Mission02` 0x5e10f8-0x5e1155: `VarValueGet(0x443ccb8)==0 && VarValueGet(0x849928e4)==0 &&
  TimerExpiredDestroy(0x4ce34)` -> `VarCreateSet(0x443ccb8,1)`, `MsgSend(FleetIntel 0xa8a880c, 0x4ce34)`.
  `Watch_Mission02_Destroyer_Find` 0x005df130: `HasShipBeenDiscovered == 1` -> 0x443ccae, **0x849928e4** set, owner
  switch, message 0x2e80a02. `Watch_Mission02_FleetIntel_WaitMessage` 0x005dbac0 (only while 0x5137b, "busy", is 0):
  0x4ce34 -> timer 3 s, `WideScreenInDontCloseSensors(0x5a)`, message 0x4ce352; 0x4ce352 -> OtherKASPause,
  UniversePause, sensors opened if closed, two `VectorNear(...,20000)` around team 0x893b502's vector, PingHint1 on her,
  PingHint2/3 on the points, `SetSensorsLookAtPoint`, `SpeechEvent(0x10002169)`. `timTimerExpiredDestroy` 0x0052c290
  returns 0 for a timer that is not found. The generated Mission02.c (3268-3299, 3529-3540, 402-409, 593-630, 2433-2455)
  matches line for line. Difficulty order from `cat_spGetDifficultyModifiers` 0x00516f90 (6, 3, 1, 0.85, 0.7) and the
  front end's SP_GameDifficulty buttons (VERY EASY .. VERY HARD, "GAME DIFFICULTY" in front_end.fib). `kasLoad`
  0x00491450 reads the saved skill only for a ratio and keeps `0x008941a0`. The user's
  `play-home/.homeworld/Cataclysm.cfg` row 94: `DifficultySetting 3` (HARD). **Producer's statement correct: a timer, no
  direction test; never on HARD / VERY HARD.**

**12 bodies compared with the export** (decompile / disassembly against the tree): cat_smViewportRenderScene, cat_smResourcesDraw,
cat_selPositionHiddenByFOW, univAddAsteroid, primPoint3 (IDENTICAL: same GL calls; glColor before glBegin in the exe,
after in GoK: same pixels), kasfMissionSkillGet (IDENTICAL), kasfIntelEventEnded (IDENTICAL; 0x00894190 = 15.0),
kasfPingAddPoint (IDENTICAL), kasfUniversePause (IDENTICAL: 0x00a4cb68 = nisUniversePause), kasfTimerExpiredDestroy +
timTimerExpiredDestroy, Init_Mission02's switch / Watch_Mission02's hint block / Destroyer_Find / the FleetIntel handlers
(generated), cloudRenderSystem (Clouds.c, IDENTICAL and unreviewed: see observation O1). No IDENTICAL body was edited:
the one edited exe body, `kasfSpeechEvent` 0x00496eb0, is MODIFIED with an M2-R8 review row, and its new line is HWI-TEST
(logging only). `cat_smViewportRenderScene` is a Cataclysm-new function (no delta-map row).

## 3. Non-vacuity (scratch worktree `original/scratch-m2/m3r3a-mission02-mut`, the producer's tree + two mutations)

There is no fix to revert (nothing was ported), so the auditor proved the producer's new instruments catch a deviation
from the exe: (A) Mission02.c Init's NORMAL timer 0x168 -> 0x20 (the generated code made wrong), (B) a fog-of-war test
(`cat_selPositionHiddenByFOW(.., TRUE)`) put on the "Dust" loop. Run `m3r3a-mission02-mut-m2` (Mission02, NORMAL, no
hold, 9,000 ticks): `hint skill=2 timer=360 ... due=5762 found=1152 speech=3968 expected=no fired=yes ... result=FAIL`
(the early timer fires the hint; it waits in FleetIntel's queue until it is not busy) and `sensors scenes=897
dustpoints=34086` (38 a frame, against 486 unmutated). Both instruments detect the deviation. The scratch worktree was
removed after the run.

## 4. The fence, rerun from the clean build

| Stream | Run(s) | sha256 (this build's header) | Against | Result |
|---|---|---|---|---|
| headless Mission01, 6,000 ticks | `-mission-a` / `-b` | e7b718c577a7458d (1,476,370 B) | `m3r1i-mission-a/run.hwo` (1ae66610c3f02410) | **IDENTICAL** both, 6,001 ticks, every field |
| missiontest Mission01 NIS-first | `-missiontest-a` / `-b` | 502d6cebacb6229f | `m3r2i-e-missiontest-a` (c14c549ceac7b34f) | **IDENTICAL** both (11,426 ticks); 6/6, done 11,424 |
| Mission02 | `-m2-a` / `-b` | dcd8841ceb929aaf | `m3r2i-e-m2-a` (7497c9e5900b59b0) | **IDENTICAL** both (25,905 ticks); 6/6, 25,936; hint PASS (expected no, fired no) |
| Mission03 | `-m3` | f116c682e99afc43 | `m3r2i-e-m3-a` (d737643a58c0f58b) | **IDENTICAL** (17,794); 5/5, 17,825 |
| Mission04 | `-m4` | 1350fb2fe48d6f40 | `m3r2i-e-m4-a` (9f141e8fb1927d7f) | **IDENTICAL** (20,787); 5/5, 20,818; dustclouds 0/1/1/0 at (-49109, 4834, 7870), (-50419, 6944, 8865) |
| team colours into Mission02 | `-tc-user` | 83ad2edf38a98a2f | `m3r2i-e-tc-user` (0ee8c83b4d7bbc4d) | **IDENTICAL** (11,521); 7 checks 0 FAIL; all 8 captures byte-equal to M3-R2's |
| replay 10-17 | `-r6` | e7f9329c304de46e | `m3r2i-e-r17-a` (c3b7d873ae8fb692) | **IDENTICAL** (9,601 ticks, 4,800 packets): the rung stays 2823 |
| replay 10-18 | `-r18` | d97242da7bf0b5d0 | `m3r2i-e-r18-a` (bd2c316848aff30c) | **IDENTICAL** (3,527 ticks, 1,763 packets): the rung stays 2871 |

Dumps (whole / crc= stripped, first 16 hex of sha256): `-motion-noi` c503c869f0dc9cdb / 3016280e9e0dd3c8, `-motion-on`
c0e411662fa2f527 / 428ef41479eb5749, `-nis-noi` 86cb155acffc180e / 196bab32d244f614, `-nis-on` 0307f3be2044c29c /
a5de13a44b8d264f: **all four EQUAL** to M3-R1 / M3-R2. **fe** (`-fe`): Main_game_screen, ASan 0. **sp** (`-sp`,
--timeout 1200): to Mission12, ASan 0, 23,289 file loads; shots per level 10, 8, 14, 13, 8, 13, 8, 13, 13, 13, 13, 2
(golden-12 a wall-clock cut-off, load average 13-25). **Every run ASan 0, no unsuppressed leak. No simulation field of any
stream moved; no stream or dump moved at all** (the port is HWI-TEST only, as stated).

The hint's other cases (test-only runs): `-hint-n` (NORMAL, scout held to 6400): due 5762, **speech 5824**, found 7232,
expected yes / fired yes, PASS, 6/6 in 29,536 ticks; `-hint-h` (`DifficultySetting 3`, held, 9,000 ticks): timer -1,
found 7488, no speech, PASS (the mission's own done line FAIL only because the run was cut at 9,000 ticks).

**Captures** (`--shot 5832-6272/40:0`, held scout, `ShowAsteriod0` 1 against 0; counted with `tools/ppm_region.py diff`
and an independent P6 parser that also checks each differing pixel's colour): 1024x768: 5832 247 px, 5952 259, 6072 261,
6192 259, 6272 0; 3840x2160: 5952 / 6072 / 6192 275 px each; **every differing pixel is exactly (162, 142, 120) =
0xff788ea2**, the exe's dust colour; every PPM byte-equal to the producer's. The view at 6072 shows the sensors manager
with the three yellow hint pings, the backdrop's grey-blue band and the dots (`original/scratch-m2/m3r3a-mission02/on6072.png`).

## 5. The diff

7 files: KASFunc.c (1 line), MissionSelfTest.c / .h, Sensors.c / .h, main.c (the switch), tools/m2_harness.sh (header
comment only). 15 added `HWI-TEST` lines; no CAT, CAT-FIX, HWI-FIX, HWI-FEATURE, CAT-TODO or CAT-PROVISIONAL line added;
none needed (no behaviour changed). Nothing outside the scope; no generated file touched; no `_WIN32` code; the harness
edit is a header comment (decision 92 respected by the producer's statement; no run of this audit overlapped an edit).
The switch defaults to -1 (no hold), so the gate run is unchanged (proven by the IDENTICAL Mission02 pair). Cosmetic
only: a blank line inside `CatSmDrawCounts` (Sensors.h 246), left as is.

## 6. Findings

1. **WRONG (doc)**: report (a1) "97 `type0.dist` x 5" (= 485): the 97 cylinders hold 2-10 each (79 of them 5), 486 in all.
   Fixed in place `[audit: ...]`.
2. **WRONG (doc, minor)**: report (a3) "0.034% of the 1024x768 frame": 247-261 / 786,432 = 0.031-0.033%. Fixed `[audit: ...]`.
3. **Added (doc)**: the user's check now also asks whether the remembered "dust belts" were the sensors manager's
   backdrop band (Background, `ShowBackground 1`), which the tree draws (capture above) `[audit: ...]`. Not an error of
   the producer's; a gap in the question to the user.

Observations (not this port's; no change):
- O1: `cloudRenderSystem` 0x00438640 is IDENTICAL in the delta map without a review. The exe picks the cloud alpha by
  `glCapFastFeature(GL_BLEND)` and `glIsEnabled(GL_POLYGON_STIPPLE)` (0x7f, or 0xc0 when blending is not fast and stipple
  is off) where GoK's body fixes 127; GoK has no glCapFastFeature (blending is always fast), so the two agree on this
  renderer. Mission02 has no clouds; for the owner of the cloud look (Missions 04/06/08) if it is ever compared.
- O2: the Mission02 self-test's scripted player does not finish at HARD (the producer's `hint-h`); the hint judgement
  is unaffected.

## 7. DONE WHEN, re-checked

Dust clouds: creation (none in Mission02, exe and tree; the level files), 3D draw (nothing to draw; the decorative rocks
on the minor render list) and the sensors-map draw (the "Dust" points with no fog-of-war test, cat_smViewportRenderScene
0x0050b4b0; resources / clouds only where visited, cat_smResourcesDraw 0x0050ae00) each stated from the exe and matched
in the tree; headless counts and captures confirm; no difference, so no CAT line is due. The hint's conditions stated from
the machine code, compared with the generated code (current), the Mission02 self-test extended and PASS with the event
under the conditions (NORMAL held) and none without (NORMAL found in time, VERY EASY found in time, HARD), proven
non-vacuous; the user told what makes it fire (NORMAL / EASY / VERY EASY at Mission02's start, 6 / 3 / 2 minutes
undiscovered). Streams, replays (2823 / 2871), dumps, fe / sp: as section 4. **Met.**

The user's check names `tools/play.sh` (it exists; switches after `--`), Mission02 from its start, Space
(`Toggle_SensorsManager 32` in default_keys.cfg), the sensors menu "Sensors" -> "Dust" (`strMenuSensorsViewDesc`,
`strMenuSV_DecorativeAsteroids`), the cfg rows `ShowAsteriod0`, `DifficultySetting`, `screenWidth` / `screenHeight`, the
switch `/uiScale`, the front end's GAME DIFFICULTY buttons and a save ("01 - Hiigara" exists in the user's play-home):
all exist.

## 8. Precision

32 sampled checks of the producer's statements (level counts x2, the 04/06/08 clouds, univAddAsteroid, the scene's
untested Dust loop, its colour, its flag / option / string, the resources' fog rule, the tree's match x2, the per-frame
count, the capture counts x2 (both sizes), the percentage, the jump table, the Watch conditions, Destroyer_Find, the
FleetIntel handlers, the missing-timer rule, the generator check, the difficulty order, the user's cfg, kasLoad, the
hint-n and hint-h results, the stream table, the dumps, fe / sp, the builds, the HWI-TEST count, the delta-map rows, the
user's-check names, the 15 s timeout): **30 correct, 2 wrong (findings 1, 2) -> precision 0.94**. Fixes: 3 (all docs).

## 9. References for the next port

Mission01 pair e7b718c577a7458d (this build's header; IDENTICAL to 1ae66610c3f02410); missiontest 502d6cebacb6229f;
Mission02 dcd8841ceb929aaf; Mission03 f116c682e99afc43; Mission04 1350fb2fe48d6f40; tc-user 83ad2edf38a98a2f; r6
e7f9329c304de46e; r18 d97242da7bf0b5d0; dumps as section 4 (unchanged since M3-R1). Each IDENTICAL in every field to
M3-R2's exit runs.

Verdict: PASS-WITH-FIXES - both items already match cataclysm.exe (Dust points drawn with no fog test, the hint an exe difficulty timer that never runs on HARD); nothing ported, every stream/replay/dump IDENTICAL, instruments proven by mutation; fixed 2 report figures and added the backdrop question to the user's check
