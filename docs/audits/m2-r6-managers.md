# Audit: M2-R6 managers (adversarial)

Port under audit: producer 4 of M2-R6, report `docs/m2-r6-managers.md`, work = `git diff c3e7c1f` (no commit after
`c3e7c1f`; the whole port is the working tree: 43 tracked files plus the untracked report). Every engine run went
through `tools/m2_harness.sh` (silent, headless, plain harness, `--bin` the audit binaries below); no Ghidra, no GOG
game, no input.

## 1. Clean build

`/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/build-audit-r6-managers`, removed first,
set up and compiled with tools/build.sh's arguments (debug: `--buildtype=debug -Db_sanitize=address,undefined`;
release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`), `werror=true` from meson.build:

| config | rc | compiler warnings | hwi-engine size (port as delivered) | after the audit fixes |
|---|---|---|---|---|
| debug | 0 | 0 | 84,594,032 B | 84,594,040 B |
| release | 0 | 0 | 5,211,968 B | 5,211,968 B |

`tools/m2_layout32.sh`: `m2_layout32: OK` (shipinprogress 48 = 0x30, Ship 1,596 = 0x63C). After the fixes
`tools/build.sh --debug` and `--release` are green too (the debug log's only "WARNING" is meson's sanitizer note).

## 2. Plain-harness runs

| label | mode | result | ASan |
|---|---|---|---|
| aud-r6mg-build-a / -b | build (7,000) | `buildtest: done tick=4205 driver=1 SU=40/100 RUs=25925 result=PASS` both; run.hwo 1,722,370 B sha256 `237157c9e5f7730e` both (`cmp` equal, hwo_compare IDENTICAL, 7,001 ticks) = the producer's `r6mg-final-build-a/-b`. Docking bay appears tick 17 at 0.0/18000, finishes tick 1457 (17,999.9); sDestroyer appears tick 1457 at 0.0/40000 clamped, finishes tick 4177; sAcolyte appears tick 208 at 220/220; screens phase: construction drawn, research rows 2 (last 2/8/4 = kind 2, TECH_DRIVE_FIGHTER, held), launch rows 2 = expected, Launch One: the acolyte left after 1 tick | 0 / 0 |
| aud-r6mg-dock | dock (5,000) | `docktest: done fighters=6 outside=6 corvettes=2 carrierleg=2 processorleg=1 RUs=1250 result=PASS`, checkpoint A tick 1250 | 0 |
| aud-r6mg-hud | hud (600) | sensors open tick 480; `scenes=119 rangecircles=14364 shippoints=8791 meshes=236 icons=236 hotkeys=5969 resources=0`; `result=PASS` | 0 |
| aud-r6mg-fe | fe, 90 s | Main_game_screen, still running at the timeout, 300 loads (= `r6mg-final-fe`), 0 fescreen UBSan | 0 |
| aud-r6mg-sp | sp, 340 s | universe running, still running at the timeout, 3,784 loads (= `r6mg-final-sp`), 0 fescreen UBSan | 0 |
| aud-r6mg-mission-a / -b | mission 6,000 | 1,476,370 B, sha256 `b0129e78bc6b9a50` both, `cmp` equal, hwo_compare IDENTICAL | 0 / 0 |
| aud-r6mg-fix-mission-a / -b, -fix-build-a / -b, -fix-dock, -fix-hud | after the code fixes | mission `b0129e78bc6b9a50` both (IDENTICAL to aud-r6mg-mission-a), build `237157c9e5f7730e` both PASS, dock PASS, hud PASS with the same sensors counts | 0 |

**Mission01 before / after (confirmed).** The producer's base run `r6mg-base-mission` (base binary c3e7c1f) is
IDENTICAL to R5's `r5-integrate-mission-a` (645f94c69d68319e; header build identity only). Against R5, the port's
stream: `FIRST DIVERGENT TICK 1; 2 field(s) differ, likely origin: rng6`, CAND 10 draws ahead; with every field but
`rng6` / `rng_all` selected (`--fields rng10,rng0,ships_n,ships_misc,ships_pos,ships_vel,ships_rot,ships_health,
missiles,bullets,derelicts,resources,statcheck,univcheck,players,dt,total,real,fpcw`): IDENTICAL over 6,001 ticks. The
draw distance grows by 5 per tick (15 at tick 2, 20 at 3, 35 at 6, 199 at 40): one stream-6 draw per player-1 ship
whose number matches the tick mod 32 (154 ships / 32), which is `cat_shipUpdateExternalBuildAndAI` 0x00543f10's
single-player special-ability chance (the attack draws return in single player). Player 1 has an aiPlayer in single
player (utility.c gameStart: `ComputerPlayerEnabled[1]`), so the exe draws there too; the call site (0x00544ada:
`[esi+0x14] & 0x800` live test, `0x00b62cf4` = aiplayerProcessing set around the call) matches. The stream therefore
changes before anything is built, as the report states, and only in the RNG fields.

## 3. Bodies compared with the exe (25)

Each compared line by line with `tools/m2_fdec.py` (and `m2_fdis.py` where Ghidra's types were in the way: the 0x24-byte
HW1 `shipinprogress` Ghidra uses makes `progress[1].nJobs / nJobsTotal / timeStart` the +0x24 / +0x28 / +0x2C words):

| function | address | verdict |
|---|---|---|
| cat_shipUpdateExternalBuildAndAI | 0x00543f10 | same (timer start buildTime/16 with the sign fix-up, 1e-6, cost; the 0xff / 0x1f masks; chance table and the ship-pointer default = 201; draw order; the special-ability switch incl. the 0x24/0x2c fall-through; attack merge against the old list only) |
| cmDeterministicBuildProcess | 0x004592a0 | same (slot + docking-bay gate, shiplagtotals, +0x40/+0x3C, cost with the Beast factor, the end through cat_cmBuildJobFinished for the local player, mode-2 progress) |
| cmDeterministicBuildCancel | 0x00459040 | same |
| cmDeterministicBuildStart | 0x00458e70 | same (the two CAT-FIXes are the documented leak / NULL entry) |
| cat_cmExternalBuildShipDied | 0x00455960 | same (recorder terms re-targeted) |
| cat_cmExternalBuildAttach | 0x00455af0 | same |
| cmCanBuildShipType | 0x00457e50 | same |
| cmUpdateFactory | 0x00458180 | same (an unknown race keeps the old list in the exe, NULL here: unreachable) |
| cat_suShipProvidesSU / cat_suShipAdded / cat_suShipRemoved | 0x00459a60 / 0x00459a90 / 0x00459ae0 | same (3) |
| cat_cmHasSupportUnits | 0x00459b40 | same |
| cmBuildTaskFunction | 0x00454940 | same |
| cat_cmBuildJobFinished | 0x00453c20 | same |
| cat_cmFactoryHasFreeSlot | 0x00454920 | same |
| rmUpdateTechList | 0x004f6d30 | **wrong** (finding 1) |
| rmSetPrintList | 0x004f75f0 | same (the exe's end write also zeroes the end row's itemstat; the lists are fixed-length, no effect) |
| cat_rmAcquireShipTechs | 0x004f4870 | same (skip list 7/16/28/32/33/34, bAcolyte 0x22 / bACV 0x26 with TECH_LINK 0x18) |
| lmLaunchBegin | 0x0049e1e0 | same |
| lmUpdateShipsInside | 0x0049e070 | same (spacers 0x92 / 0x93, classes 5/7 and 6/0xb) |
| lmLaunchCapableShipDied | 0x0049e690 | same (control flow traced through the gotos) |
| cat_smShipBlipType | 0x0050a280 | same (enum values printed from the build: sCarrier 0xc .. sDestroyer 0x19) |
| cat_smMinesDraw / cat_smResourcesDraw / cat_smSensorRangeBlobsDraw | 0x0050b380 / 0x0050ae00 / 0x00509be0 | same (3) |

Data checked against the exe: the Sect (38 rows with the end) and Beast (190) research lists at 0x008913c0 /
0x00891588: 0 differences; `smShipTypeRenderFlags` against .data 0x00892ba8 (145 bytes): 0 differences. Also
checked: `clWrapCreateShip` 0x00452f30 returns `clCreateShip`'s eax (0x00452f6c), `collCheckShipShipColl`'s docking
exemption at 0x0043ae38 / 0x0043ae9d (`SOF_CatDocking` 0x80000000 unless `+0x1cc & 8`), `cat_univConvertShip`
0x00543950's tail (finding 2).

`CAT_EXTERNAL_BUILD_DRIVER` is 1 (`CommandLayer.h:236`), its only readers UnivUpdate.c:2150 and the build test's log;
no `CAT-PROVISIONAL` names the driver, ConsMgr, research or sensors (11 left in the tree, all other subjects; the diff
removes 10 and adds none). The report's 107 review entries: 107 `[review: M2-R6 managers]` notes in delta_map.json.

## 4. Diff review

Scope: every changed file serves one of the six subsystems (Collision.c: the docking exemption the dock leg needed;
Region.h: the exe's 0x400 render events the sensors viewport needs; CameraCommand.c / LOD.c: the scene's helpers;
the univCreateShip 6th argument's callers). No out-of-scope change found. Changed functions were listed per file by
parsing both versions (`git show c3e7c1f:<file>` against the tree) and matched with the delta map: every edited
function the base map calls IDENTICAL has an M2-R6 managers review entry but one (finding 4). New HW1 aliases: none;
aliases left in ported files: finding 5.

## Findings

1. **rmUpdateTechList gated the wrong technology** (code + report). The exe's multiplayer branch makes a row
   unresearchable when `tech == 0x22 && !(0x008860ec & 0x20)` (decompile of 0x004f6d30); 0x22 = 34 =
   `TECH_CRYSTAL_HARVEST` (ResearchAPI.h's enum; bit 0x20 is the crystals option, Salvage.c / docs/m2-r4-combat.md).
   The port tested `TECH_INFECT` (32) through `catRmGameOptionInfect`, and the report called technology 34 "the Beast
   infection". Effect in multiplayer: infection never researchable, crystal harvest always. **Fixed**: the test is
   `TECH_CRYSTAL_HARVEST` / `catRmGameOptionCrystals` (still the stage-5 CAT-TODO for the option word), comments, the
   report (4 places) and the delta-map / review notes of 0x004f6d30 corrected.
2. **cat_univConvertShip read the new owner's rows** (code). The "new technology" test (0x00543d7f..0x00543de3) ANDs
   the new owner's held set with `[esp+0x18]+0x350` (techstat) and `+0x388` (catShipUpgradeTechs) at `[esp+0x14]*0xA8`,
   the owner and type saved from the OLD ship at 0x00543962 / 0x00543968; the port used `newOwner`'s techstat and
   upgrade row. **Fixed** (`oldOwner`, with an audit comment); the report's line annotated.
3. **Two multi-line CAT-TODOs**: KASFunc.c `CAT-TODO(stage 6 KAS)` (three lines, mid-comment) and UnivUpdate.c
   `CAT-TODO(stage 4 AI)` (three lines). **Fixed**: each on one line, owner first.
4. **Edited IDENTICAL without a review entry**: `clWrapCreateShip` 0x00452f30 (CommandWrap.c) now returns the ship;
   the map kept IDENTICAL with only R5 notes. **Fixed**: reclassified MODIFIED with an `[audit: M2-R6 managers]` note in
   delta_map.json and delta_map_review.json (sample `audit-R6-managers`); counts IDENTICAL 2,965, MODIFIED 1,371; the
   report's counts annotated.
5. **HW1 aliases left in ported files**: TradeMgr.c `R1` (tmTechInfoDraw, which this port compared) and Sensors.c
   `GravWellGenerator`, `Carrier`, `Mothership` (in HW1's blob render, now uncalled). **Fixed** with the aliases'
   values (`RACE_Sect`, `kGravWellGenerator`, `kCarrier`, `sMothership`): no behaviour change.
6. **Wrong owner tag in a prose reference**: Sensors.c's section comment pointed to "CAT-TODO(stage 3 FOW) in
   Select.c"; Select.c's tag is `CAT-TODO(M2-R6 FOW)`. **Fixed**.

Not findings (recorded): HW1's blob render (`smBlobsDraw` and helpers) stays uncalled in Sensors.c, as the report says;
`collCheckShipShipColl`'s other exe-only tests (the clamp-host comparison) were not ported, as the report says. The
x87 vs SSE precision of the per-tick cost truncation in `cmDeterministicBuildProcess` is HW1's expression, unchanged.

## Precision and fixes

Sampled checks: 25 bodies; data tables 2 (research lists, render flags); gate claims 12 (build PASS, build stream
hash and determinism, dock PASS, hud sensors frame, mission hash and determinism, before = R5, after = rng6/rng_all
only, the cause, sp, fe, driver switch and provisional tags, 107 review entries); build / layout 2; diff-review checks
10 (scope, CAT comments on the sampled hunks, CAT-TODO format, edited-IDENTICAL reviews, aliases in ConsMgr.c /
LaunchMgr.c, aliases in the other ported files, the convert-ship tail, the Collision.c exemption, clWrapCreateShip's
return, the Sensors.c owner reference) = 51 checks; wrong: 7 (finding 1, finding 2, the two CAT-TODOs of finding 3,
finding 4, finding 5, finding 6). Precision = 44 / 51 = 0.86.

Fixes in place: 8 (ResearchGUI.c tech 0x22; UnivUpdate.c old-owner rows; KASFunc.c and UnivUpdate.c one-line
CAT-TODOs; Sensors.c owner reference; the clWrapCreateShip reclassification in both map files; the four aliases;
docs/m2-r6-managers.md "[audit: ...]" edits). After them the build is green (0 warnings, both configurations), layout32
OK, and the Mission01, build, dock and hud runs give the same streams and results as before the fixes (the fixed
paths are multiplayer-only).

Verdict: PASS-WITH-FIXES - every DONE WHEN item met on the plain harness (build test PASS and byte-identical twice with ASan 0, both dock host legs and the ACVs PASS, the hud sensors frame through cat_sm*, the research lists equal to the exe's data, Mission01 changed only in rng6/rng_all by the exe's AI timer, sp/fe unchanged); two multiplayer logic errors (technology 0x22, the old owner's rows), two split CAT-TODOs, a missing review entry, leftover aliases and a wrong tag reference fixed
