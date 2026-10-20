# Audit: M2-R8 singleplayer-tutorial

Auditor: adversarial audit of the M2-R8 producer 4 port (report `docs/m2-r8-singleplayer-tutorial.md`), base `3903f02`.
The port is `git diff 3903f02`. There is no commit after the base, so the whole port is the working tree: 37 tracked
files changed and the new report. Every engine run below went through `tools/m2_harness.sh` (the plain harness: silent,
offscreen, no GOG game, no DLL, no input), with the auditor's own clean build passed with `--bin`. No Ghidra run: the
exe was read through the export `cataclysm_gog_r6` with `tools/m2_fdec.py`, `tools/m2_fdis.py` and `tools/m2_exedata.py`.

## 1. Clean builds

`rm -rf original/scratch-m2/build-audit-r8-singleplayer-tutorial`, then `meson setup` with `tools/build.sh`'s arguments
(debug: `--buildtype=debug -Db_sanitize=address,undefined`; release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`;
the intro options show werror true and warning_level 3), then `meson compile`:

| config | result | compiler warnings | size before / after the audit's fixes | sha256 after the fixes |
|---|---|---|---|---|
| debug | `[253/253] Linking target hwi-engine`, rc 0 | 0 | 81,660,152 B / 81,660,216 B | `98d0ba6502d87788` |
| release | `[253/253] Linking target hwi-engine`, rc 0 | 0 | 4,523,768 B / 4,523,768 B | `f8a2d48c0bf8bd52` |

After the fixes, both audit configurations and the worktree's `tools/build.sh --debug` / `--release` were rebuilt: rc 0
and 0 `warning:` lines. The debug setup's one "WARNING" is meson's sanitizer notice. `tools/m2_layout32.sh` printed
"m2_layout32: OK (all ILP32 layout asserts hold)", including the new `tutGameEnableFlags` 8 and `tutpointer` 0x68 pins.
`python3 -I tools/m2_save32_gen.py --check` printed "src/Game/SaveCat32Layouts.c is current". `python3 -I
tools/m2_kas2c_gen.py --check` reported all four files current.

## 2. Runs (auditor's clean build, plain harness)

Labels `r8p4a-*` ran on the build before the audit's fixes and `r8p4a-fix-*` on the build after them. All ran in
`original/scratch-m2/m2r2-harness/runs/`.

| run | mode | result |
|---|---|---|
| `r8p4a-m6k-{a,b}`, `r8p4a-fix-m6k-{a,b}` (`tools/oracle/mission_test.sh --bin <audit debug> --label ... --ticks 6000 --no-perturb-run`) | mission, 6,000 ticks | MISSION TEST PASS both times (a = b byte-identical, 1,476,370 B, sha256 **`00ae16e7f73689d9`** in all four runs; the flipped copy was reported at tick 2000 / ships_health); ASan 0, no leak |
| `r8p4a-mt-{a,b}`, `r8p4a-fix-mt-{a,b}` | missiontest (`--mission-test-ticks 20000`) | all four `done ticks=11424 objectives=6/6 result=PASS`, ASan 0; run.hwo a = b, sha256 `35d612581c658119` in all four (= the producer's `r8p4-f2-mt-a/b`) |
| `r8p4a-tut`, `r8p4a-fix-tut1..3` | missiontest `--tutorial` | PASS every time, ASan 0. Buttons up at tick 1, Lesson1 at tick 2, prompts at ticks 7 / 102 / **1175, 1171, 1178, 1160**, the pointer at tick 156, Next clicked, Lesson2 one or two ticks later. The streams differ run to run (finding F4) |
| `r8p4a-load-{a,b}`, `r8p4a-fix-load` | loadgame `--load-ticks 3600` ("01 - Hiigara") | `loaded 'SavedGames/SinglePlayer/01 - Hiigara': 682 chunks converted, 167 ships, 0 size problems`; `done ticks=3600 from=1985 to=5585 mission=1 ships=140 objectives=0/1 kasTeams=36 result=PASS` all three times; sha256 `b9ba66b8a1638bc2` all three times; ASan 0 |
| `r8p4a-ai` | ai, 400 | done, ASan 0; sha256 `a88770b54df64d4e` |
| `r8p4a-sk` / `r8p4a-sk8` | skirmish LiveFireExercise2 / Armed8, 600 | PASS `ships=89` / `ships=54`, ASan 0; sha256 `7e135b2daee5d99e` / `1d6d27adc0a79ca5` |
| `r8p4a-fe`, `r8p4a-fix-fe` | fe, 120 s | Main_game_screen, still running at the timeout; ASan 0, no leak |
| `r8p4a-sp` | sp, `--timeout 340` (NIS-first start) | universe running, still running at the timeout; `debugScreenshots univUpdateCounter: 4496` (at least 1,248); FileLoads lists `SinglePlayer/Mission01_NIS01/Mission01_NIS01.mission` and then `SinglePlayer/Mission01/Mission01.mission`; ASan 0, no leak |
| `tools/oracle/selftest.sh --label r8p4a-oracle-rel --bin <audit release>` | oracle | `SELFTEST PASS` |

**The Mission01 stream** against producer 3's audited stream (`r8k2a-final6k-a`, sha256 `19560d16033efd53`):
`python3 -I tools/oracle/hwo_compare.py <runs>/r8k2a-final6k-a/run.hwo <runs>/r8p4a-m6k-a/run.hwo --from-tick 1
--to-tick 6000 --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real
--json <runs>/r8p4a-m6k-a/compare-audit.json` printed "HEADER: same producer, different build identity: 7d225de4/1fbfa32f
vs 3903f026/1fbfa32f" and "IDENTICAL: 6000 ticks and 0 packets compared (ticks 1-6000), no difference". With every field
over ticks 0-6000 the result was also IDENTICAL. The audit's fixed pair (`r8p4a-fix-m6k-a/b`) has the same sha256
(`cmp`-identical). **The stream did not move**, as the producer states.

**The other streams** against the previous audit's runs (all fields from tick 0, `hwo_compare.py`):
- `r8k2a-ai` vs `r8p4a-ai`, `r8k2a-fix-lfe2` vs `r8p4a-sk` and `r8k2a-sk-armed8` vs `r8p4a-sk8`: IDENTICAL. Only the
  header's build identity differs, so "= audited" holds in content.
- The mission self-test, `r8k2a-fix-mt-a` vs `r8p4a-mt-a`: "FIRST DIVERGENT TICK 1772 ... likely origin: rng0 ... CAND
  is 300 draw(s) ahead of REF". This matches the report's statement (UpdateArrivingShip's motion-cue reset), except for
  which random stream it names (F5).

## 3. Bodies compared with the exe

The auditor read each body below against its decompile, and the disassembly where noted. Tables and strings were read
with `m2_exedata.py`.

- **SinglePlayer.c**:
  - The difficulty readers:
    - `cat_spGetDifficultyModifiers` 0x00516f90: 6 / 3 / 1 / 0.85 / 0.7. The integer divisors 4 / 2 / 1 / 1 / 1 are the
      float-typed denormals in Ghidra's output. An out-of-range difficulty leaves the outputs alone. As ported.
    - `cat_spGameDifficulty` 0x0051c140: first call, radio set, `| 8` redraw, the click's pData. As ported.
    - `kasLoad` 0x00491450: local, then current, then `0x008941a4 = 0x00894194 / local`. As ported, with the CAT-FIX
      default 1.0.
    - `kasfMissionSkillSet` / `Get` 0x00493b80 / 0x00493b90 (IDENTICAL): both use the word 0x008941a0. As ported.
  - The end of a mission:
    - `singlePlayerMissionCompleteCB` 0x0051be40 (0x008b9ecc = `mrRenderMainScreen`, quittime in real time,
      0x00a76258): as ported.
    - `singlePlayerMissionFailedCB` 0x0051be70: speech 0x2001028, `univKillPlayer(0, 2, -1)`, `CheckPlayerWin`. As
      ported.
    - `SetOnMissionCompleteInfo` 0x0051ab50 ("%d%s", `_stricmp` 0x00676c90 against "NOHYPERSPACE"): as ported.
    - `singlePlayerSetMissionAttributes` 0x00519440 (race 0 / 1 before `scriptSet`, then `nebReset`): as ported.
  - `CalculateHyperspaceTimeForShip` 0x005178a0 (disassembly 0x005179ab ..): `push 0xa`, `fild qword` of the draw,
    RANDOM * 2^-32 (0x0072a4e0 = 0x2f800000), `fadd` BASE, one `fstp`. The tree's real64 expression is the same.
  - `CalculateRollCall` 0x005176e0 together with `cat_spShipCanHyperspace` 0x00517650: the inside count, the
    non-capital non-module "remaining unless 0x40", the capital non-mothership non-module "docked if 0x80". As ported.
  - `spHyperspaceButtonPushed` 0x00518f50: the frame-step gate 0x00a6ed74, the under-construction refusal (string
    0x00bb4e6c), the next-level table, Fleet Command 0x200101f, the inlined lockout 2, the roll call. The Go Now /
    Emergency choice (+0x35a / +0x35b of a player-0 ship) sets the disabled bit 4. As ported.
  - `spMainScreen` / `spLockout` / `spMainScreenAndLockout` / `cat_spMainScreenAndLockoutKeepSensors` / `spUnlockout`
    0x00518d80 .. 0x00518f00: as ported.
  - `singlePlayerInit` 0x00518c50: 19 warp records of 0x2F0 bytes (0x00a67498 .. 0x00a6ac68), "Warp.script", mission 1
    or the warp's, `oldAutoLaunch` 0xF. As ported.
  - `cat_spEmergencyHyperspaceCB` 0x005174b0: reason 0x2f with `+0x18 | 0x40`, then reason 0x23, a steady ship and
    `+0x1c8 | 0x8080` (disassembly 0x0051756f), then `feScreenDeleteFlags(.., 0)`. As ported.
  - `cat_spMissionFlagValue` 0x0051c120 with the table 0x00894424 (0,1,1,0,0,1,0,1,0,0,0,0,0,0,0,1,0,1,0) and the
    strings 0x00bb4e04 / 0x00bb4e08 = `strSinglePlayerNextLevel` / `strSinglePlayerHyperspace` (table base 0x00bb3620,
    index 1529): as ported.
- **Tutor.c**:
  - `tutInitTutorial` 0x0052f690: as ported.
  - `tutGameMessage` 0x00533e20 (only tutorial 1, 16 slots, no duplicate test): as ported.
  - `tutSaveLesson` 0x0052f6d0 (commas, "%s%s" with "SavedGames\Training\"): as ported, with a CAT-FIX copy.
  - `tutShowNextButton` 0x00532010: position, size and atom are as ported. Its region's extra size is **finding F2**.
  - `tutDrawTextPointers` 0x005312b0's colour (R pulse/4, G pulse, B 2·pulse/3): as ported.
  - `tutSaveTutorialGame` / `tutLoadTutorialGame` 0x0052f750 / 0x0052f8b0 (the order text, Next, previous, quit, back,
    next text, speech flag): as ported.
  - `cat_tutPointerListReset` 0x0052f890: 16 pointers of 0x68, +0x3c type and +0x60 selection, matching the tree's
    layout. It is called last in `universeReset` 0x0053df80. As ported.
  - **The flag name table 0x008ab74c (54 names): finding F1.**
- **The selection-name hashing**: `kasGetGrowSelectionPtrIfExists` 0x00490b70 and `kasGrowSelectionPtr` 0x00490bb0
  compare the stored hash word, allocating in steps of 0x10. The tree's `udword label` versions are the same. The
  report's statement that an earlier producer closed this holds.
- **The sensors helpers**: `smClickedOnPlayer` and `smPlayerNamesDraw` occur 0 times in functions.csv, symbols.csv and
  decompiled.c. No caller is left in the built tree; only the unbuilt `src/Win32/mainrgn.c` still names one.
  `toLegendDraw`'s map entry already records "no smPlayerNamesDraw".
- **IDENTICALs compared (5)**: `tutNextButtonClicked` 0x00532a80, `kasfMissionSkillSet` 0x00493b80,
  `kasfMissionSkillGet` 0x00493b90, `hsStaticGateRender` 0x00490240 (flag 0x00a6ad78, then `cat_hsGateRectangle` with
  90) and `pingLoad` 0x004e9790 (the 0x58 head + `+0x50` data size = the new `ping` layout 0x58 / 0x80): same as the
  tree.

## 4. The diff (`git diff 3903f02`)

- **Scope**: every file belongs to the single-player, tutorial, save-tail, self-test or harness scope, or follows from
  it. The other files only carry call-site changes: `spMainScreen(TRUE)` in KASFunc.c, Objectives.c and UnivUpdate.c,
  the tutorial button hooks in the managers, and the comment-only texreg.c. No change is outside the producer's scope.
- **Edited functions vs the delta map**: an audit script compared every function body of the changed .c files at the
  base and now, with comments and whitespace stripped. Of 176 changed, new or deleted functions, every one in the map
  carries a `[review: M2-R8 singleplayer-tutorial]` entry or an earlier R8 one. The exceptions are call-site-only edits
  of MODIFIED rows (`kasfTutCameraFocusDerelictType`, `univUpdate`: `spMainScreen(TRUE)`). The rest are not in the map:
  HW1 helpers the exe inlines, CAT-only bodies and HWI test hooks. No IDENTICAL row was edited without a review.
- **The delta map**: `python3 -I tools/delta_map.py merge --out <copy>` printed "records changed 0", and the copy is
  byte-identical. `report` reproduced `docs/m2-r1-function-deltas.md` byte for byte. The counts are IDENTICAL 2,785 ->
  2,773, MODIFIED 1,552 -> 1,564 and reviewed 3,274 -> 3,340, with 98 tagged entries, as reported.
  `m2_stage_counts.py` gives stage 6 as 289 / 180 / 25, as reported.
- **CAT comments**: every sampled Cataclysm change carries one. Universe.c's new comment had a continuation line at
  column 0 (**F6**, cosmetic).
- **CAT-TODOs**: `grep -arn "CAT-TODO(stage 6" src/` gives 0 lines. The base had 25 such lines (`git grep` at
  `3903f02`), and all 25 are removed and replaced by ports. No CAT-TODO was added, so the port adds no tag that breaks the one-line,
  owner-first rule. The report's count is **F3**. (Note: this host's `grep` is ugrep, which silently skips files it
  thinks binary, such as `UIControls.c`. The greps above use `-a`.)
- **Stream moves**: the Mission01 stream did not move. The mission self-test stream's move is stated (F5 is its
  wording).
- **Generated files**: `m2_save32_gen.py --check` and `m2_kas2c_gen.py --check` both report current.
- **The harness**: the loadgame mode copies the save into the run's settings dir only. The data dir's
  `SavedGames/SinglePlayer` is empty, and the install's save is unchanged (sha256 `73f52d0766d309fc`).
  `savegame_probe.py` on it printed `OK sp=1 mission=1 players=2; chunks=2354 ... ship_chunks=167 min_ship=0x63c`.

## 5. DONE WHEN, re-checked

| item | result |
|---|---|
| `grep -rn "CAT-TODO(stage 6" src/` lists only M2-R9 part B re-targets | met: 0 lines (nothing re-targeted; producer 2's `CAT-TODO(M2-R9 part B)` tags remain) |
| sp from the NIS-first start reaches the running universe with Mission01, univUpdateCounter 1,248+ at 340 s, ASan 0 | met (4,496) |
| the mission self-test PASS twice, byte-identical | met (`35d612581c658119`, four runs) |
| the Tutorial1 leg | PASS in four runs. Its stream is not byte-identical run to run (F4, observation O1) |
| "01 - Hiigara" loads through the mode-A loader, ASan 0 | met (three runs, `b9ba66b8a1638bc2`) |
| the Mission01 stream IDENTICAL to producer 3's audited stream | met |
| fe ASan 0 | met |

## 6. Findings

- **F1 (code, wrong)**: `tutGameEnableString` lost `"Attack"`. The port inserted `"Waypoint"` / `"WaypointIssue"` by
  replacing the `"Attack"` line, so the table had 53 names, while `tutGameEnableFlags` and the exe's table 0x008ab74c
  both have 54, with `Attack` at index 13 (the auditor dumped all 54 pointers and compared them by name).
  `tutSetEnableFlags` maps a name to its table index, and that index is the bit. As a result:
  - every flag from `Harvest` to `ManagersLocked` set the bit below its own: `"MothershipFocus"` set bit 47
    (FocusCancel's), while `cat_mrFocusOnMothership` tests bit 48;
  - `"Attack"` was not found, and index -1 set bit 31 (ContextFormX).

  This contradicts the report's "The bitfield and the name table now match". **Fixed**: `"Attack"` is restored after
  `"WaypointIssue"`, and a `_Static_assert` pins the table at 54 + 1 entries. Mission01 does not use the flags: its
  stream and self-test are unchanged by the fix. The tutorial leg still PASSes.
- **F2 (code, latent memory overrun)**: `catTutButtonCreate` (new, used by all five tutorial buttons) asked
  `regChildAlloc` for `sizeof(buttonhandle) - sizeof(regionhandle)` extra bytes. Both are pointer types, so this is 0.
  It then wrote `button->processFunction` at offset `sizeof(region)` (152 on LP64, measured with the project's flags),
  past the requested block. Only the memory pool's rounding (64-byte cookies) kept it inside. The exe passes 0x1c.
  **Fixed**: `sizeof(uicbutton) - sizeof(region)`.
- **F3 (report)**: the report says "Closed (24 ..., single player 10)". The base had 25 `CAT-TODO(stage 6` lines and
  the report's own list names 11 single-player ones. **Fixed** with `[audit: ...]`.
- **F4 (report)**: "the tutorial leg's prompt ticks moved with them [the last two fixes]" is wrong. The third prompt's
  tick varies run to run with one binary: 1152 and 1238 in the producer's two runs, and 1175, 1171, 1178 and 1160 in
  the audit's. The tutorial leg keeps speech on, on SDL's dummy driver, whose audio thread runs on the wall clock.
  **Fixed** with `[audit: ...]`.
- **F5 (report)**: the mission self-test's move is "the game RNG 300 draws ahead". The stream that moves is stream 0
  (rng0: `cat_mcResetRequest` draws `ranRandom(RANDOM_STREAM_0)`), not the game stream 10. **Fixed** with
  `[audit: ...]`.
- **F6 (comment)**: Universe.c `universeReset`'s new comment had its last line at column 0. **Fixed**.
- **O1 (observation, for M2-R9; not counted)**: the Tutorial1 leg cannot be a byte-identical pair while it keeps
  speech on. A later owner could drive the subtitle and speech timing from the virtual clock, or turn speech off as the
  loadgame test does, if the tutorial leg is to give a reproducible stream.

The report got `[audit: ...]` notes for F1 to F5. Fixes: **6** (F1 to F6).

## 7. Precision

Sampled checks:

| sample | checks | correct | wrong |
|---|---|---|---|
| exe bodies, tables and data (section 3) | 31 | 29 | 2 (F1, F2) |
| report claims and gates (builds, layout32, both generators, map merge and report, counts, stage counts, review count, the two mission pairs, the stream compare, the self-test move, tutorial PASS, loadgame, save probe, install untouched, ai, two skirmish maps, fe, sp, release selftest, the stage-6 grep, the harness data dir, CAT comments) | 26 | 26 | 0 |
| report statements and conventions | 4 | 0 | 4 (F3, F4, F5, F6) |

Correct 55, wrong 6: **precision 55 / 61 = 0.90**.

Mission-stream hash of the final pair (the next port's reference): **`00ae16e7f73689d9`** (6,000 ticks, 1,476,370 B, build
identity `3903f026`). Its content equals producer 3's audited `19560d16033efd53` in every field. The checkpoint's commit
changes only the header's build identity.

Verdict: PASS-WITH-FIXES - single player, hyperspace, difficulty, the save's mission tail and the tutorial buttons match the exe in 29 of 31 sampled bodies; every gate passes (mission pair 00ae16e7 IDENTICAL to the audited stream, self-test 6/6 byte-identical, Tutorial1 leg, "01 - Hiigara" load, sp 4,496, fe, ai, skirmish, ASan 0); fixed the tutorial flag-name table (lost "Attack": flags off by one bit), the tutorial buttons' region size, two report counts/claims, an RNG stream name and a comment
