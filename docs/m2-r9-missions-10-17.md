<!-- done: generator, mission-table-flows, mission-selftest-10-17, warp-start-test, tutorial-leg-save, runs-streams-reviews -->
# M2-R9 producer 3: stage 6 part B, missions 10-17 and the end of the game

Base commit `307c3a5` (branch `m2`, worktree `.claude/worktrees/m2`). Nothing committed. Every engine run went through
`tools/m2_harness.sh` (silent, headless, offscreen driver). No Ghidra, no GOG game, no oracle DLL, no input injection,
no gamescope. Run directories are `original/scratch-m2/m2r2-harness/runs/<label>` (gitignored scratch).

## 1. Summary

- **Generator**: `tools/m2_kas2c_gen.py` generates all seventeen missions and Tutorial1 (`DEFAULT_SCRIPTS`).
  `--check` is current: **"verified 2085 bodies: callees and constants equal to the machine code's, 25279 all-constant
  calls' arguments equal"**. All 2,085 kas2c mission functions that `tools/m2_stage_counts.py` counts (`mission 2085`)
  are generated: there are 2,085 `// cataclysm.exe 0x..` bodies across `src/Missions`, none missing and none extra.
  This round added 4 reading rules and 11 OVERRIDES addresses (16 text patches). Producer 2's 4 addresses bring the
  total to 15 addresses and 20 patches. Mission01 .. 09, Tutorial1 and every earlier output are byte-unchanged.
- **Missions 10 .. 17**: their `catKasScripts` rows now have code. Each mission has a self-test table and a scripted
  player (MissionSelfTest.c). **All eight PASS: every objective in order, two runs byte-identical, ASan 0.**
- **The end of the game**: Mission17's own script calls MissionCompleted. univUpdate quits into the credits
  (`utyGameQuitToMain` 0x005ab6b0: `psScreenStart("Credits-GameEnd.cred")`). Reached headlessly:
  `missiontest: game end tick=15584 credits=Credits-GameEnd.cred completed=15584`.
- **Warp start under a test** (new harness mode `warpstart`): the run gets a test copy of the install's warp.script
  (WarpEnabled 1, LevelToWarpTo N) in its own data dir. The level's Warp.script record is checked against the fleet in
  the ether and again after the arrival. **PASS for level 10, pair byte-identical, ASan 0.**
- **Tutorial**: the `--tutorial` leg goes **through Lesson12, PASS** (12/12 lessons, every lesson's objectives, Next
  clicked 11 times, ASan 0). The engine's own Tutorial1 run wrote all 12 lesson saves. The "LESSON 11" save loads
  through the loadgame mode (`--training --save-src`): **PASS, pair byte-identical, ASan 0**.
- **Port defects the runs found, fixed at the exe's body**:
  - `scriptStringToBool` 0x00525c40: "1" read FALSE. This is a MODIFIED delta-map row that was never ported.
  - `tutGameSentMessage` 0x00533e80: cleared the queue, which the exe never does. Found modified.
  - `cat_kbFocus` 0x0054f2c0 and chase 0x0054f1c0: an ASan stack overflow in the one-ship focus local.
  - `cat_bigGunProcessFireCommand` 0x005bbe80: a NULL dereference after etgEffectDelete.
  - `MakeExplosionRockBumpables` 0x00545f00: blast damage to ship parts recursed without end (a stack overflow).
  - `spHyperspaceSelectionOut` 0x00518940: no NULL test on a clamp's host, and midlevelHyperspaceOut was never cleared.
  - Generator: a localised table split over two lines, so Mission10 did not compile.
- `grep -rn "CAT-TODO(M2-R9 part B" src/` = **0**. The single one, in SinglePlayer.c, is closed.
- **Headless Mission01**: `tools/oracle/mission_test.sh --ticks 6000` gives a byte-identical pair
  (`dc2fb8ce71c0d26b`). Its records are **IDENTICAL to producer 2's audited `a9m2-mission-a` (`be3e5030cebbea0b`)**:
  all 6,001 ticks, no difference. Only the header's build identity differs.

## 2. File-by-file changes

| file | change |
|---|---|
| `tools/m2_kas2c_gen.py` | `DEFAULT_SCRIPTS` = Mission01, Tutorial1, Mission02 .. Mission17. Reading rules: a `BYTE_xxxxxxxx` operand (sym_addr and the operand regex); a hex divisor (`[/%] 0x..`, MSVC's magic-number imul); a hash printed as `s_<name>_<addr> + N` becomes `GARBLE(addr+N)` (Mission12 / 17); a localised table index wrapped onto the next line is joined (`(&PTR_..)\n[strCurLanguage]`, Mission10's location card 0x008cb5f4). `_assert_stub()` and the OVERRIDES of section 7. |
| `src/Missions/SinglePlayer/Mission10.c` .. `Mission17.c` (new, generated) | 90 / 160 / 73 / 124 / 171 / 31 / 158 / 370 bodies (1,177) |
| `src/Missions/KasShared.c`, `CatMissions.h`, `meson.build` (generated) | shared bodies 29 -> 46, prototypes and `CAT_MISSIONnn_WATCH_EXE`, the eight new sources |
| `src/Game/SinglePlayer.c` | `catKasScripts` rows mission10 .. 17 (Init / Watch / function table / count / exe watch address); the CAT-TODO closed. `spHyperspaceSelectionOut` (0x00518940): host tested non-NULL (0x00518a05 .. 0x00518a13), and the StartInHS branch clears `midlevelHyperspaceOut` when the ship leaves (0x0051899e .. 0x005189d3). HWI_ORACLE: `catWarpStartSelfTestScript` after Warp.script is read (singlePlayerInit); the record check at the end of `cat_spWarpToLevel` (0x0051b9c0); new `cat_spWarpStartCount` and `cat_spWarpStartSelfTestCheck` |
| `src/Game/SinglePlayer.h` | prototype of `cat_spWarpStartSelfTestCheck` (HWI_ORACLE) |
| `src/Game/MissionSelfTest.c` / `.h` | tables and players for missions 10 .. 17 (section 5); the game-end judgement; the loss ends the run; objective reopen; the warp-start test; the KAS message trace (`/catMissionTestTrace`); the "level objects" line (mines, slipgates); the Tutorial1 leg through Lesson12 (section 6); the training-save load (`/catLoadGameTraining`); helpers (damage ring, big gun fire, volume centre, capture with a replacement worker, support units freed by scuttling small craft) |
| `src/Game/OracleDump.c` / `.h` | `oracleMissionStopNow()`: QUIT event, close, stop (the game end) |
| `src/Game/KASFunc.c` | HWI_ORACLE hook in `kasfMissionCompleted` (0x00491c70): "mission completed" |
| `src/SDL/utility.c` | HWI_ORACLE hook in `utyGameQuitToMain`'s credits branch (0x005ab6b0): "credits Credits-GameEnd.cred" |
| `src/Game/AITeam.c` | HWI_ORACLE hook in `aitMsgSend` (0x00417e10): the message trace (off unless `/catMissionTestTrace`) |
| `src/SDL/main.c` | switches `/catMissionTestTrace`, `/catWarpStartTest`, `/catLoadGameTraining` |
| `src/Game/StatScript.c` | `scriptStringToBool`: `'1'..'9'` are TRUE (cmp al,0x31 / 0x39 at 0x00525c6a) |
| `src/Game/Tutor.c` | `tutGameSentMessage`: only reads the queue (0x00533e80 never writes 0x00a6c2c4) |
| `src/Game/UnivUpdate.c` | `MakeExplosionRockBumpables`: no blast damage to a `SOF_CatShipPart` (0x00546157 / 0x005461a4) |
| `src/Game/KeyBindings.c` | `catKbFocus` / `catKbChaseMode`: the focus local is a `MaxSelection` (the exe copies 0x3EA dwords into 0xFA8 bytes, 0x0054f2f9) |
| `src/Ships/Mothership.c` | `cat_bigGunProcessFireCommand` (0x005bbe80): the charge effect held in a local (edi at 0x005bbf2f) across `etgEffectDelete` |
| `src/Game/ResearchGUI.h` | prototype of `rmExitMenu` (the research manager's Close button, used by the tutorial leg) |
| `tools/m2_harness.sh` | mode `warpstart` (`--warp-level N`), `--save-src DIR` and `--training` for loadgame; milestones "mission self-test: the game's end (credits)" and "warp-start self-test done"; warpstart summary line; header docs |
| `ghidra/delta_map_review.json`, `ghidra/delta_map.json` | 20 `[review: M2-R9 missions-10-17]` notes, merged (section 8) |

## 3. Cataclysm deltas (exe addresses)

- **The end of the game**: Mission17's script calls `MissionCompleted` (0x00491c70: `singlePlayerGameInfo`'s completed
  path). `singlePlayerGameUpdate` (0x005197d0) was compared again: mission 17's white-out sets `quittime` and
  `utyCreditsSequence` (0x0051a027 ..), but Mission17 ends through MissionCompleted, not this branch.
  `utyGameQuitToMain` (0x005ab6b0) plays the credits: `psModeBegin("Credits\\", 0xd)` and
  `psScreenStart("Credits-GameEnd.cred")` (0x005ab70d ..), the same as the tree.
- **Finding (not fixed, owner M2-R10)**: `singlePlayerGameUpdate`'s two animatic calls and `singlePlayerPostInit`
  reach the tree's `animBinkPlay`, which is GoK's `#if 0` stub, so no animatic plays between missions.
  `animAviPlay` is the port of 0x00421860.
- The seven port fixes in section 1, each at its exe address.
- **Later missions' screens and flows exercised** (final pairs):
  - Big gun: salvage, research, build, fire (M10, M16).
  - Bentusi: M11 repair (`REPAIR_FACTORUM`), M15 slipgate block.
  - Pirates: M10 escort.
  - Beast: M11, M13, M14, M16, M17 (Naggarok, nomad moon emitters, EMP).
  - Mimic stealth: M14 `GET_SAMPLE`, through `cat_shipStealthDetectionUpdate`'s detectors.
  - Capture: M14.
  - Roll call: `GoNowHyperspace` in M10, 11, 12, 13, 14, 16.
  - Letterbox jump: M15 (`catSpMissionNextLevel`).
  - The credits: M17.
- **Mine / Slipgate readers** (level-object counts at tick 1): M10 4 mines; M12 26 mines; M14 32 mines and 6
  slipgates; M15 2 slipgates.
- **kasf comparison**: 42 kasf that the new scripts call and that no audit had sampled were compared with the exe body
  (decompile and disassembly). All 42 are the same as ported:
  - 40 called only by missions 10-17: kasfAttackSpecial, kasfBuilderRestrictAll / None, kasfDecNaggarokMissiles,
    kasfDisableBigGunFiring, kasfEnableNaggarok, kasfGetNaggarokMissiles, kasfGetVectorX / Y / Z,
    kasfHaltAllPlayerShips, kasfIntelEventNotEnded, kasfIsBeastReady, kasfIsEMPReady, kasfMissionCompleted,
    kasfNomadMoonCanFire / EmitterCount / Fire, kasfRKOAvoidLocation / Beware / IsSafe, kasfSetShipPositionXYZ,
    kasfSetStayMimicked, kasfShipsAttributesBitClear / Set, kasfShipsDockSupportWith, kasfShipsFinishedLaunching,
    kasfShipsGetClamped, kasfShipsLaunch, kasfShipsParalyze, kasfShipsSelectDocked, kasfShipsSelectSalvaged,
    kasfShipsSetDerelict, kasfShipsStopForResearchDocking, kasfSlipgateExplode, kasfSoundEventShips,
    kasfTeamAttributesBitClear / BitTest, kasfTimesHitByEMP, kasfVectorCrossProduct.
  - 2 more: kasfIsSwarmReady and kasfShipsSelectNotIdle.

## 4. Builds

- `tools/build.sh --debug` and `--release`: rc 0 and 0 warnings under `-Werror` in every build of the round.
- Final binaries: debug `e956d61f30eb35d0` (every final run), release `9cefea9baccf5fc1`.
- `tools/m2_layout32.sh`: OK (all ILP32 layout asserts hold; 45 ship-specifics files).
- `python3 -I tools/m2_save32_gen.py --check`: current.
- `python3 -I tools/m2_kasf_table_gen.py --dispatch --check`: KASFuncTable.c and KASFuncDispatch.c current.
- `python3 -I tools/m2_kas2c_gen.py --check`: all 21 outputs current, with the "verified 2085 bodies" line above.
- `python3 -I tools/m2_stage_counts.py` after the merge: IDENTICAL 2761, MODIFIED 1576, REWRITTEN 83, catnew 3789,
  mission 2085.

## 5. Mission self-tests 10-17 (final pairs `r9f-mNN-a/b`, debug build, ASan)

Command: `tools/m2_harness.sh r9f-mNN-{a,b} --mode missiontest --mission NN --mission-test-ticks 90000`. Each mission
starts through the exe's warp start (Warp.script's level-NN record). The judge marks an objective complete when its
script destroys it or sets it. An objective the table marks *held* completes through the Hyperspace path's
`ObjectivesDestroyAll`.

| mission | objectives (table order; tick created -> completed) | N/M | done tick | stream (run.hwo sha256) | pair |
|---|---|---|---|---|---|
| 10 | SALVAGE_BIGGUN 160->7152, RESEARCH_BIGGUN 7152->10048, BUILD_BIGGUN 10048->14064, FIRE_BIG_GUN 18592->20496, HYPERSPACE 21520->jump 21584 (roll call) | 5/5 | 21938 | `c2ce3bd90fde7545` | identical, ASan 0/0 |
| 11 | DESTROY_ATTACKERS 1600->1888, REPAIR_FACTORUM 3008->4512, DESTROY_BEAST 5216->5744 (held), PROTECT_FACTORUM 5216->5744 (held), Hyperspace 7856->jump 7920 | 5/5 | 8275 | `6d68a150e477d31c` | identical, 0/0 |
| 12 | DefendMothership 3216->7600, AttackOutpost 3216->7600, Hyperspace 10576->jump 10640 | 3/3 | 10995 | `0f692f9bf5aa50b0` | identical, 0/0 |
| 13 | DESTROY_ENEMY 3008->19488, PROTECT_FAALCORUM 19472->19488, Hyperspace 21328->jump 21392 | 3/3 | 21746 | `101cf559320b56bd` | identical, 0/0 |
| 14 | PROTECT_MOTHERSHIP 4192->21920 (held), GET_SAMPLE 4192->10080, CAPTURE_TAIIDAN 13264->21920 (held; the frigate captured), Hyperspace 21920->jump 21984 | 4/4 | 22339 | `6771039a945af3c0` | identical, 0/0 |
| 15 | DestroyGateNode 352->6192, Block 2096->6192, Hyperspace 15200->jump 15264 (letterbox) | 3/3 | 15424 | `4e003c8c99f7680b` | identical, 0/0 |
| 16 | LOCATE_CS 240->1872, CRIPPLE_CS 1936->5360, DOCKWITH_CS 5872->12816, HARVEST 12816->20736, DESTROY_BEAST 20736->21088, HYPERSPACE 21104->jump 21168 | 6/6 | 21522 | `c4b0e21e6cc9a1e0` | identical, 0/0 |
| 17 | DestroyNaggarok 144->15152, DestroyNomadMoon 4208->5248, reopened 5760 ->13200, DestroyEmitters 5248->5744, Recon 8592->12736; **game end 15584** (MissionCompleted, credits) | 4/4 | 15584 | `244b3664ab20976a` | identical, 0/0 |

Every run: rc 0, milestone "mission self-test done" (M17: "mission self-test: the game's end (credits)"), first stop
none, LeakSanitizer no unsuppressed leak.

**Players**, in brief:
- **M14**: the mimic's phases (undisguised escort ring 3000..9000, mimic out of view, detectors near the disguised
  mimic destroyed). The capture uses workers with the salvage upgrade. Over the unit cap, small craft are scuttled to
  free support units: Mission14's fleet arrives at 378/372. When a worker of the pair dies, a free one replaces it.
  The command ship's defence (12000) spares the Naggarok and both capture teams.
- **M17**: the moon is held at 50% until Recon completes, and `TECH_INERTIALESS_DRIVE` is researched when it becomes
  researchable.

**Unreachable in the exe's own scripts** (left out of the tables; grep-verified against the generated bodies and the
exe's pushes):
- M12 WorkerDock / WorkerReturn: message 0x04f3cd02 is never sent. [audit: that holds for WorkerDock only; WorkerReturn waits on 0x0d00b093, which the SecurityTeam FSM (team 0x0be7033d, Watch_Mission12_SecurityTeam_Docking 0x006218c0) sends; that team stays empty in the run ("missiontest: fsm tick=1 team=be7033d ... ships=0"), as MissionSelfTest.c's Mission12 note says]
- M15 DefendMothership: selection 0x004a2a39 is never filled.
- M17 "Research": 0xa796318 is never sent.

**Missions 01-09 again** (`r9f-m1..9-a/b`): all PASS and ASan 0. Their streams compared with producer 2's audited
`a9m2-mN-a`:

| mission | done | stream | pair | vs producer 2 |
|---|---|---|---|---|
| 01 (`--mission 1`) | 11424, 6/6 | `5bcaf13b1cba4a45` | identical | (no `--mission 1` reference). `r9g-mt1-a/b` (no `--mission`, as `a9m2-mt1-a`): `5bcaf13b1cba4a45` pair identical, **IDENTICAL records** to `a9m2-mt1-a` (11,426 ticks) |
| 02 | 25136, 6/6 | `0917cc3258e19e51` | identical | IDENTICAL records (25,105 ticks) |
| 03 | 17825, 5/5 | `4803d7cc0e0c8911` | identical | IDENTICAL records (17,794 ticks) |
| 04 | 20816, 5/5 | `a95a64d1c8ed9d38` | identical | IDENTICAL records (20,785 ticks) |
| 05 | 16384, 6/6 | `fb4398be696cea4f` | identical | first divergent tick 8535, field `ships_health` (after the self-test's ShipsDamage of 42 ships around the command ship at 8344 / 8408: expected from the ship-part blast fix) |
| 06 | 30931, 7/7 | a `e95151c47b80026d` / b matches reference | **DIFF** | b IDENTICAL records (30,900 ticks); a differs only in `rng_all` from tick 2081 (see below) |
| 07 | 16592, 4/4 | `55571ec3b6fd59b6` | identical | IDENTICAL records (16,561 ticks) |
| 08 | 29361, 6/6 (was 29363) | `c89457cce7e20ad8` | identical | first divergent tick 18792, `rng10` + `rng_all`: the test player's new scuttle of 4 small craft for support units (the generalised `catMtFreeSupport`); expected |
| 09 | 20579, 6/6 | `93d35ed586303949` | identical | IDENTICAL records (20,548 ticks) |

The whole-file sha256 always differs from producer 2's because the header carries the build identity (307c3a50 vs
64f49a98).

**Mission06 `rng_all` flake (found, not fixed)**: in 4 Mission06 runs (`r9f-m6-a/b`, `r9g-m6r-a/b`), two match
producer 2's records exactly. The other two differ **only** in `rng_all`, the CRC over all 27 RNG streams, from tick
2081 in one run and 22235 in the other. Every game field stays equal for all 30.9k ticks: `rng10` (gamerand), `r6`,
`r0`, univ, stat, pos, hp, misc and the rest. So a non-game stream is drawn off the tick. The likely candidate is
`RANDOM_SOUND`, drawn by the mixer thread (`smixer.c:198` fqRand; HW1's own comment calls that stream
non-deterministic). This was not verified. Mission06's outcome and every objective tick are the same in all four runs.

## 6. Tutorial leg, tutorial save, warp start

**Tutorial1 leg** (`r9g-tut`, `--tutorial --mission-test-ticks 30000`): `missiontest: done ticks=15118 tutorial ...
lessons=12/12 complete=12/12 inOrder=1 nextClicks=11 result=PASS`. ASan 0, rc 0.

Lessons completed at ticks 2644 (L1 ClickHere), 2936, 3276, 3472, 9050, 9382, 9578, 11166, 12960, 13252, 14586 and
15022 (Lesson12: 5/5). Lines `missiontest: tutorial LessonN tick=N` and `missiontest: tutorial LessonN complete
tick=N objectives=a/b` are logged.

Every scripted event goes through the game's own callbacks:
- poPopupFleetIntelligence for L1ClickHere.
- The Next button's click callback.
- Camera kasf.
- `cat_kbCommandExecute` keys: focus, open menu, sensors, delta, aggressive, create group, harvest, research manager,
  link, tactical, chase, time.
- `tutGameMessage` for the UI messages.
- `rmResearchItem` on the TECH_LINK row.
- `rmExitMenu`.
- `mrRetire` and `mrScuttle`.
- Command-layer moves, attacks, special actions and builds.

The stream is not a pair while speech runs on the wall clock (`4215dcdb6bad33f5` here, `59baa32135fd39e8` in the same
build's earlier run).

Lesson11 and Lesson12 needed three test-side fixes, each grounded in the engine's own code:
- **The research manager must be closed before linking**: while it is open, `mrDisabled` blocks every order
  (`cat_uiLink` 0x00565570).
- **The pair of acolytes is moved 6000 out from the command ship before the link**: the link flight
  (`cat_processLinkToDo` 0x00467b80) flies with DontFlyToObscuredPoints | ReturnImmedIfPointObscured. About 2400 from
  the command ship (where the tutorial's fleet waits) the meeting point is obscured and the leader never moves. This
  was compared with the exe at 0x0046842e .. 0x00468545, and the obscure checks with 0x00415649 .. 0x0041567c: same
  as the tree.
- **The scuttle confirmation is pressed within TW_ScuttleReconfirmTime** (3 s of `realtimeelapsed`). The virtual
  clock gives 1/16 s a tick, so 48 ticks is exactly 3.0 s, which is not under the limit.

**Tutorial save load**: the engine's Tutorial1 run (`r9g-tut`) wrote all 12 lesson saves to
`settings/SavedGames/Training`. Then `tools/m2_harness.sh r9g-tload-{a,b} --mode loadgame --training --save-src
runs/r9g-tut/settings/SavedGames/Training --save "LESSON 11 - Researching and Upgrading" --load-ticks 3600` gives
`loadgametest: loaded singlePlayer=1 tutorial=1 ... kas='tutorial1' watch=1` and `done ticks=3600 from=13353 ...
objectives=1/1 kasTeams=10 result=PASS`. The pair is byte-identical (`80a84d9bd0c90031`), ASan 0/0.

**Warp start** (`r9g-warp10-{a,b}`, `--mode warpstart --warp-level 10 --mission-test-ticks 4000`):
- The test warp.script reads `WarpEnabled=1 LevelToWarpTo=10`.
- At `cat_spWarpToLevel`: `applied level=10 resources=8000/8000 types=18/18 ships=76/76 (all 97) ether=79 outside=0
  tech=20/20 result=PASS`.
- After the arrival: `ether=0 outside=79 ... result=PASS`.
- Overall: `done level=10 ticks=99 applied=1 level=1 kas=1 arrived=35 fleet=1 failed=0 result=PASS`.
- The pair is byte-identical (`46690e6a7b4cfeb1`), ASan 0/0. Level 5 also passed in development (`r9m3-warp5-i1`).

Harness flag: `--mode warpstart [--warp-level N]`. The run's data dir is symlinks to the install plus
`warp.script` from `bigfile.py cat cataclysm.big warp.script`, with sed applied. The install is untouched.

## 7. Generator: rerun and verification

Rerun commands:
- `python3 -I tools/m2_kas2c_gen.py` writes every default script.
- `python3 -I tools/m2_kas2c_gen.py --check` verifies them and checks that the outputs are current.
- Reproduced from scratch into `original/scratch-m2/r9m3-gen2`: all 21 outputs byte-identical to the tree.

Counts: 2,085 bodies verified (callee multisets and 32-bit constants equal to the machine code's); 25,279 all-constant
calls' arguments equal.

OVERRIDES added this round (each marked HAND-EDIT in the output):
- **Folded assert stub**: Ghidra's `nullsub()` is `kasfAssert(...)` on the folded stub 0x0049ae30.
  - The Sentinels FSMs of M11 / 14 / 16 / 17 assert `<= 12` at 0x006167b0 (2 sites), 0x00616130, 0x006161f0,
    0x006309e0 (2), 0x00630420, 0x0063e920 (2), 0x0063e360, 0x0064dd60 (2) and 0x0064d710.
  - M13 SupportWave2 asserts `TeamCount == 3` at 0x006295b0.
- **0x0062dfe0 Init_Mission14_Wave1_WaitCreate** (2 patches): the kasLabel load that Ghidra typed `void *`, and ECX
  passed as a this-argument of the one-parameter kasfGetBuildTime.

## 8. Delta-map review entries

`python3 -I tools/delta_map.py merge --done m2r9:missions-10-17`: "records changed 20". The counts after the merge
are IDENTICAL 2761, MODIFIED 1576, REWRITTEN 83, reviewed 3346. 0x00533e80 moved IDENTICAL -> MODIFIED.

The 20 notes:
- 0x00525c40 scriptStringToBool: ported now.
- 0x00533e80 tutGameSentMessage: found modified, ported.
- 0x00545f00 MakeExplosionRockBumpables: the ship-part exclusion, ported.
- 0x00546250: compared again.
- 0x00518940 spHyperspaceSelectionOut: host test and StartInHS clear, ported.
- 0x005197d0 singlePlayerGameUpdate: the end of the game compared, plus the animatic-stub finding.
- 0x005ab6b0 utyGameQuitToMain: the credits compared, plus the hook.
- 0x00491c70 kasfMissionCompleted: compared, plus the hook.
- 0x0051be40: compared.
- 0x00518c50 and 0x00417e10: hook notes.
- 9 kasf rows "compared again (Mission10..17 call it): same as ported": 0x004932a0, 0x004932d0, 0x00494610,
  0x00496130, 0x00496140, 0x00497b80, 0x00492280, 0x004934d0, 0x00496e80.

The other compared kasf are Cataclysm-only and have no row. KeyBindings' `catKbFocus` / chase and Mothership's big gun
are Cataclysm-new (catnew) and have no row.

## 9. CAT-TODOs

- Closed: `CAT-TODO(M2-R9 part B)` in SinglePlayer.c (the generated code of Mission10 .. Mission17).
- `grep -rn "CAT-TODO(M2-R9 part B" src/` = 0. No CAT-TODO was added.

## 10. Guard rails (final debug build `e956d61f30eb35d0`)

| run | rc | milestone | first stop | ASan | stream |
|---|---|---|---|---|---|
| `r9g-mission-a/b` (mission_test.sh 6000) | 0/0 | oracle mission run done | none | 0/0 | `dc2fb8ce71c0d26b` pair, records IDENTICAL to `a9m2-mission-a`; MISSION TEST PASS (perturbed copy found at tick 2000) |
| `r9g-lfe2-a/b` skirmish | 0/0 | skirmish self-test done | none | 0/0 | `875350e737d7a520` pair |
| `r9g-armed8` | 0 | skirmish self-test done | none | 0 | `34784de5c3d0d9f6` |
| `r9g-ai-a/b` | 0/0 | ai self-test done | none | 0/0 | `365ec02f314dd779` pair |
| `r9g-build-a/b` | 0/0 | build self-test done | none | 0/0 | `23d9e51680225092` pair |
| `r9g-loadgame-a/b` ("01 - Hiigara", 3600) | 0/0 | save-load self-test done | none | 0/0 | `f1a3fc673a2a6019` pair |
| `r9g-console` | 0 | console self-test done | none | 0 | - |
| `r9g-fe` (120 s) | 124 | Main_game_screen | still running at the timeout | **0** | - |
| `r9g-sp` (340 s) | 1 | universe running | `texreg.c (569): Assertion of (reg->nPalettes < 8)` (producer 2's known Mission04 stop, M2-R10) | **0** | 40 bytes leaked after the fatal exit, as producer 2's |

## For STATE.md

- M2-R9 part B, producer 3: Mission10 .. 17 generated (2,085 of 2,085 kas2c mission functions; `--check` verifies
  2085 bodies and 25,279 all-constant calls). `catKasScripts` is complete.
- Self-tests: missions 10-17 PASS, every pair byte-identical, ASan 0. The game's end is reached headlessly: Mission17
  -> MissionCompleted -> credits "Credits-GameEnd.cred".
- New harness modes and flags: `warpstart` (`--warp-level`), loadgame `--training --save-src`. New switches
  `/catMissionTestTrace`, `/catWarpStartTest`, `/catLoadGameTraining`.
- Tutorial1 leg PASS through Lesson12. A lesson save written by the engine loads (training mode), pair identical.
- Port fixes: scriptStringToBool digits, tutGameSentMessage no clear, kb focus / chase selection copy, big gun charge
  effect local, ship parts take no blast damage, spHyperspaceSelectionOut clamp host / StartInHS.
- Headless Mission01 records identical to the audited stream. Missions 02/03/04/07/09 and the Mission01 self-test have
  records identical to producer 2's; 05 and 08 moved for known reasons (section 5).

## What is left, and owners

- **Animatics**: the tree's `animBinkPlay` is GoK's stub, so no animatic plays between missions (0x005197d0 note).
  Owner: M2-R10.
- **The sp gate's texreg stop in Mission04**. Owner: M2-R10 render (producer 2's section 7.3).
- **Mission06's rare `rng_all`-only divergence** (a non-game RNG stream drawn off the tick, likely the mixer's
  `RANDOM_SOUND`). Owner: the oracle / M2-R10.
- **Tutorial pairs** need speech off the wall clock to be byte-identical. Owner: M2-R10.
- **Seen in the logs, not investigated**: particle `is_final_tex` warnings (GoK 64-bit) and `memAllocFunctionA`
  failures in Mission17 (heap growth). Owner: M2-R10.
