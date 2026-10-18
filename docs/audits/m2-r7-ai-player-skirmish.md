# Audit: M2-R7 ai-player-skirmish (producer 3)

Auditor: M2-R7 adversarial auditor 3. Base `1410c22` (producer 2's audit checkpoint); the port is the working tree on top of
it (no WIP commits). Producer report: `docs/m2-r7-ai-player-skirmish.md`. Every engine run below went through
`tools/m2_harness.sh` (or `tools/oracle/mission_test.sh`, which calls it) on the plain harness, silent and headless, with the
audit's own binaries. No Ghidra, no GOG game, no input injection.

## 1. Builds

Clean build in `original/scratch-m2/build-audit-r7-ai-player-skirmish/` with tools/build.sh's arguments
(`meson setup --buildtype=debug -Db_sanitize=address,undefined` and `-Doptimization=2 -Ddebug=false -Db_sanitize=none`),
`werror=true` from meson.build: both rc 0, 0 compiler warnings (the only "WARNING" line is meson's sanitizer notice).
Debug 87,714,384 B, release 5,419,312 B; after the audit's fixes 87,709,528 B / 5,419,312 B, still 0 warnings.
`tools/build.sh --debug` and `--release` in the worktree after the fixes: rc 0, 0 warnings. `tools/m2_save32_gen.py --check`:
current. `tools/m2_layout32.sh`: OK (see F4).

## 2. Bodies compared with the exe (export cataclysm_gog_r6: decompile, disassembly where it was unclear, data)

The AIPlayer fields were mapped through Ghidra's HW1 layout, which is 4 bytes short from +0x950 and 8 from +0xBC8 (so its
`ScriptCreator` is the exe's +0x950, its `NumRUsSpentOnAttman` the exe's resource-manager spend, its `firstTurn+2` +0xB8E).

| function | exe | result |
|---|---|---|
| `aiplayerStartup` / `aiplayerShutdown` | 0x00410fa0 / 0x00411000 | match (`cat_kasVarStoreInit` after `aivarStartup`; `cat_kasVarDestroyAll`) |
| `aiplayerUpdateAll` (+ inlined play) | 0x00411240 | match (ships-arriving runs; `cat_aisPlay` + frame count in multiplayer; SP fleet command only without an NIS; aiCurrentAIPlayer cleared in MP only) |
| `aiplayerPlayerDied` | 0x00411410 | match (the ship-died loop skips the SP human, 0x0041180a) |
| `aiplayerSave` | 0x00412380 | match |
| `BuildShip` | 0x00404470 | match (support-unit gate, the four counters by type 13/26/57, 14/49/88, 16/51, 60/74; external -> `clWrapCreateShip`) |
| `aifBuildRequestedShips` | 0x004045b0 | match (overflows: att / res stored float, def kept on the x87 0x00404a19..47; -1 / 1e21 constants; unordered-as-<= branches; the +0xB8E clears) |
| `aifProcessShipBuildRequests` | 0x004052f0 | match (research-delay table 02 01 00; first usable request per queue; Sect mothership/carrier upgrade, Beast TECH_INFECT 0x20; being-built bytes at +0x9D4/+0xA65/+0xAF6) |
| `airShipDied`, `airAddNewShip` | 0x00412590, 0x00412950 | match (jump table decoded: feature bits 1/4/8, support types 2/1/0, +0x944/+0x948 decrements only above 0, modules +0xBC8++, +0x950--, +0xB8E cleared; sensors +0x1CC0) |
| `aitMsgSend` / `aitMsgReceived` | 0x00417e10 / 0x00417e60 | match (0x81-dword clear, senders at +0x104, replace-by-last) |
| `aitTeamIsInMothershipRange` | 0x00417470 | match (900 / 1600 at 0x72a4cc / 0x72a4e4; the speed product unrounded, range stored as float; the inlined homogeneity test equals the tree's `aitTeamHomogenous`, HW1's always-set flag included) |
| `cat_aisPlay`, `cat_aisReset` | 0x00433d50, 0x00433d30 | match: intervals 20 / 10 / 5 (0x72a4d0 / 0x72a4f0 / 0x72a624; default 0x72a460 = 0), chances 20 / 50 / 100, DFG 300 (0x72a644) / 2000 (0x72a5ac), the five calls' teams 2 / 23 / 26 / 4 / 8 and parameters, s/bMothership 0x12 / 0x35 |
| `cat_aisTeamAttack` | 0x00432df0 | match (10000 0x72a488, offsets 500 / 500 / 100; `cost + 10000` compared on the x87; formation `% 5`, index 9 / 0x12 -> 5; kamikaze at 5) |
| `cat_aisScoutsUpdate` | 0x00432760 | match (1/3/5, 20000 0x72a638, 16000 0x72a634, protect, 0/3/6, Crate = derelicttype +0x290 == 0) |
| `cat_aisWorkersUpdate` | 0x004324d0 | match (stack decoded: harvest selection at esp+0x10, repair at +0xFB8, the special target's `timeLastStatus` uninitialised in the exe) |
| `cat_aisResearchUpdate` | 0x00431290 | match (module types 0x13 / 0x15 / 0x16 / 0x17 / 0x18 to the five tables; Beast table of 18; > 0.7 unrounded; difficulty > 0) |
| `cat_aisMothershipUpdate` | 0x00433310 | match: team 15 total at 0x0097ce5c (the producer's fix), 90000 0x72a640, 8000 0x72a63c, the y upper bound from smUniverseSizeX at 0x00433595 |
| `cat_aisSetupAlliances` | 0x004310a0 | match |
| `cat_aisUpdateTeams` | 0x0042fd50 | match in the sampled parts: the bounty leader loop and the enemy filter (0x0042fe8f..0x0042fed4: `idiv`, `fild`, > 5.0), SOF_Dead 0x800, the type -> team mapping of every case, the least-built test (0x0043013f: unrounded `< 0`, stored diff `<= -0.4`), carriers 1 / 2, carrier support hosts, modules, big gun team 15, DFG 3500 (0x72a620), crystals 0x7e / 0x7f, the build-queue tail |
| `cat_aisTeamAddShip`, `cat_aisSentinelsUpdate`, `cat_aisLeechesUpdate`, `cat_aisProcessorUpdate`, `cat_aisShipIsBusy` | 0x0042fa80, 0x00433760, 0x004338e0, 0x00432630, 0x004324a0 | match (5000 0x72a618; sentinel test inverted as the exe 0x0043386d, formationtype +0x230; leeches x2 / x10 / rest; 4000 0x72a630) |

Data: the 8 play timers at 0x008dfc44 are 8 zero dwords (cleared by `cat_aisReset` as the tree), `aisFirstPlay` 0x007525e4
= 1, the five target tables 0x00752578..0x007525e0 and the six research tables 0x007524bc..0x00752574 equal the tree's
arrays value for value (compiled probe of the tree's enums). Also checked: Dock.c / Salvage.c stats (0x00463294 / 0x004632aa
-> playerStats +0xC / +0x8, 0x004f99a5 -> +0x28), UnivUpdate.c's converted-ship dock (0x00543d10..0x00543d42: `% 100 + 1 <
10` -> purpose 7, else 0, aiplayerProcessing around it), `cat_rmAIResearchTech` returns 1 / 0 (0x004f3365 / 0x004f336e),
tested at 0x004131f2. `grep -rn "CAT-TODO(stage 4" src/` gives 0 (base: 11); the two new tags are `CAT-TODO(stage 6 KAS):`
one-liners. 44 `[review: M2-R7 ai-player-skirmish]` entries in `ghidra/delta_map_review.json`; the exe has the 17
`cat_ais*` functions plus `aisRequestShip` / `aisFleetUpdate` between 0x0042fa80 and 0x00433d50.

Observation (not this port's code): `hyperspaceCost` 0x00443c30 keeps the remaining distance and the cost sum on the x87
across the ships and returns the unrounded sum; the tree's `CommandLayer.c` version rounds each step to float. It feeds
`cat_aisTeamAttack`'s RU test; a candidate for the x87 list when the skirmish AI is compared with a recording.

## 3. Runs (plain harness, audit debug binary)

| run | result |
|---|---|
| `aud7c-ai-a`, `-b` (`--mode ai --timeout 300`) | `aitest: done tick=398 ... checks=16/16 result=PASS`, ASan 0, no unsuppressed leak; `cmp` equal, sha256 `96f479fe44f729cb...`; first ticks build 17, harvest 33, switch 65, skirmishplay / skirmishbuild / skirmishresearch 81, skirmishharvest 177: as the report states |
| `aud7c-mission-a`, `-b` (`mission_test.sh --ticks 6000 --no-perturb-run`) | MISSION TEST PASS, ASan 0, byte-identical `0ea19f070fede4c0`; vs producer 2's audit stream `aud7b-final-mission-a` (`7c224543`): IDENTICAL, 6,001 ticks (header build identity only, a3cc7f4b vs 1410c223); vs the baseline `r6-integrate-mission-a`: IDENTICAL; equal to the producer's base / wip1 runs |
| `aud7c-replay-a` (`--mode replay --timeout 600`) | `replay done ticks=3527 packets=1763 ... pktSyncErr=0`, rc 0, ASan 0, sha256 `8dc7808919e8834b`; vs the producer's base-binary `r7c-base-replay-a`: IDENTICAL (3,527 ticks, 1,763 packets) |
| `aud7c-dock` | `docktest: done ... result=PASS`, ASan 0 |
| `aud7c-build` | `buildtest: done tick=4205 ... result=PASS`, ASan 0; stream IDENTICAL to `aud7b-build` (7,001 ticks) |
| `aud7c-fe` (`--timeout 90`) | `Main_game_screen`, ASan 0, 0 UBSan reports |
| `aud7c-sp` (`--timeout 340`) | universe running (univUpdateCounter 3,264 at the last log line), ASan 0, no unsuppressed leak |
| `aud7c-final-mission-a/-b`, `aud7c-final-ai-a/-b`, `aud7c-final-replay-a` (binary rebuilt after the fixes) | mission PASS `0ea19f070fede4c0` both; ai PASS 16/16 `96f479fe` both; replay 3,527 ticks `8dc78089`; ASan 0 everywhere |

Replay against the GOG stream (`docs/m2-r6-oracle-mode-a.md` arguments):

```
python3 -I tools/oracle/hwo_compare.py original/oracle/oracle01-rec-20261018-102253/oracle01-rec.hwo \
  <runs>/aud7c-replay-a/run.hwo --from-tick 1 --to-tick 9600 \
  --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real \
  --json <runs>/aud7c-replay-a/compare.json
  -> FIRST DIVERGENT TICK 2 (real 615605.125 vs 615605.188), FIRST DIVERGENT PACKET 68 (tick 137, univcheck)
```

as the report states. The 9601-tick DONE WHEN item cannot be checked this round: `original/game/SavedGames/RecordedGames/
oracle01` was replaced at 10:13 today (sha256 `28d37f75...`, 1,763 packets; producer 2's audit replay ran at 08:37 on
`f9756e29...`), and the harness copies only from `original/game`. Confirmed: the producer's claim about the re-recording,
and that `oracle01-play-a.hwo` in the new rec directory is a copy of the 10-17 playback (`cmp` equal). The old recording
is still on disk as `original/oracle/oracle01-rec-20261017-140428/oracle01` + `.pkts` (sha256 `f9756e29...`, the one producer
2 replayed), so it can be restored under another name if the orchestrator wants the 9601-tick comparison back.

**Mission stream, before / after:** the producer's statement holds: before (base binary) and after, `0ea19f070fede4c0`,
IDENTICAL in every field to producer 2's `7c224543` and to `49240abcf9f0ed97`; no field moved. The computer player of
Mission01 is single player: the skirmish AI runs only in multiplayer, and the managers whose bodies changed only act with
fleet control on (producer 2's audit), so stream 6 is not drawn differently.

## 4. Findings

**F1 (fixed): `aiaClose` edited while classed IDENTICAL with no review.** The port dropped `aiumemFree(aiplayer->
aiaArmada.targets)` from `aiaClose` 0x00402780 (correct: the exe frees only +0x1CA8), but the delta map still had it
IDENTICAL, unreviewed. Fix: a MODIFIED entry (sample `audit-R7-ai-player-skirmish`) appended to `ghidra/delta_map_review.json`
(format kept: indent 1, ASCII), `python3 -I tools/delta_map.py merge` (1 record changed, checked against a copy taken
before), `tools/delta_map.py report` (which first reproduced the producer's `docs/m2-r1-function-deltas.md` byte for byte).
Totals now IDENTICAL 2,855, MODIFIED 1,481, REWRITTEN 84, reviewed 3,029. Report marked `[audit: ...]`.

**F2 (fixed): `cat_kasVarFind` / `cat_kasVarFindOrCreate` returned the value word.** The exe returns the entry (base + i*3
dwords, 0x00491b20 / 0x004918d0) and its callers read the value at +8 (gunShootGunsAtTarget 0x0048c32e `cmp [eax+0x8]`;
the tree's own stage-6 note in Dock.c writes `...->value = 1`). KASVar.h said "the value word". Both now return
`CatKasVar *`; no caller exists yet, so nothing else changes. Report marked.

**F3 (fixed): the report's naming statement.** "Names chosen here (no CET name): the team names ..., `aisRequestShip`,
`aisFleetUpdate`" is wrong: CET's `Cataclysm.h` declares both functions at 0x00431510 / 0x00431750 and its `AISkirmish.h`
names the 34 teams. Marked `[audit: ...]`.

**F4 (fixed): `tools/m2_layout32.sh` did not check the new ILP32 asserts.** AIPlayer.h (0x1D18 and 30 offsets) and
AISkirmish.h (0xFB4 / 0x9D10) were not reached by its translation unit. A direct `clang -m32 -fsyntax-only` of both headers
shows them holding (AIPlayer 7,448 B, AISTeamEntry 40,208 B, AITeam.msgQueue at 0x50); both headers were added to the script
(header note marked `[audit: ...]`), which reports OK.

**F5 (fixed): stale name in AITeam.h.** The layout comment named the team loader `aiplayerCat32LoadTeam`; it is
`aitCat32LoadTeam` (AITeam.c). Comment corrected.

No change outside the producer's scope was found: AIAttackMan.c / AIMoves.c / AIUtilities.c follow from closing producer 2's
`CAT-TODO(M2-R7 AI player)` (embedded armada), and the Dock / Salvage / UnivUpdate / utility.c edits close the stage-4 tags
there. Every other edited function the map lists is MODIFIED / REWRITTEN with a review, or Cataclysm-only (not in the map).
Every Cataclysm change sampled carries a `CAT:` comment (the two `CAT-FIX` deviations, the zero-bounty guard and the
leaked KAS array, are where the exe would fault or leak, decisions 19 / 25).

## 5. Precision

Sampled checks of the producer's claims: 59 (two builds; 27 bodies of section 2; the KAS store bodies; the timers and
cat_aisPlay constants; the target and research tables; the Dock / Salvage / UnivUpdate / ResearchAPI ports; the stage-4
count and the base's 11; the ai self-test PASS, its byte identity and its first ticks; the replay identity, its divergence
statement, the re-recording and play-copy statements; the mission before / after; fe, sp, dock, build; the 44 reviews and
the totals as stated; save32 current; the 17 + 2 functions; the i386 asserts; the naming statement; the review coverage
of edited functions; the AITeam.h comment). Wrong: 4 (F1, F2, F3, F5). **Precision = 55 / 59 = 0.932.** Fixes made in
place: 5 (F1 review + merge + report regeneration, F2 code, F4 tool, F5 comment, the report's `[audit: ...]` marks).

## 6. Reference for the next port

Mission01 stream of the final pair (`aud7c-final-mission-a` = `-b`, audit debug build after the fixes, 6,000 ticks):
**sha256 `0ea19f070fede4c0ae569b14eaae67daf24a599ee5cc1c21c6ce15e2bdb73101`**, IDENTICAL to `7c224543` and to the
baseline `49240abcf9f0ed97` in every field (header build identity only). ai self-test stream (400 ticks):
`96f479fe44f729cbc9da2aa9568f87dcfe72998c46c20dbc135dc59912a65d85`. oracle01 replay (new recording, 3,527 ticks):
`8dc7808919e8834ba5fb40174e0055bc08636fa8297e04a754d891657737997b`.

Verdict: PASS-WITH-FIXES - skirmish AI, player, fleet / resource managers and teams at the exe's in all 27 sampled bodies with the 8 timers and tables, ai self-test PASS 16/16 byte-identical, mission stream unchanged as stated; fixed a missing aiaClose review, the KAS lookup return, a wrong naming claim, the layout32 coverage and a stale comment (the 9601-tick replay is not checkable: the recording was replaced)
