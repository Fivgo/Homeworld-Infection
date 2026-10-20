# Audit: M2-R10 tests-leftovers (producer 3)

Auditor: adversarial audit of `git diff 3ea7b12` in the m2 worktree (no commits after 3ea7b12; the whole port is the
working tree: 17 tracked files + the report `docs/m2-r10-tests-leftovers.md`). `<R>` =
`original/scratch-m2/m2r2-harness/runs`. Every engine run went through `tools/m2_harness.sh` (plain harness, silent,
offscreen) or `tools/oracle/mission_test.sh` (which calls it); runs are labelled `r10a3-*` / `r10a3f-*`.

## 1. Clean build

`original/scratch-m2/build-audit-r10-tests-leftovers/{debug,release}` from an empty dir with tools/build.sh's options
(`--buildtype=debug -Db_sanitize=address,undefined`; `-Doptimization=2 -Ddebug=false -Db_sanitize=none`):
rc 0 / rc 0, **0 compiler warnings** under `-Werror`; `hwi-engine` debug 88,662,880 B, release 5,623,728 B (the same sizes
after the audit's comment / help-string fixes). After the fixes: incremental rebuild of both (0 warnings),
`tools/build.sh --debug` and `--release` in the worktree rc 0 (the only "WARNING" line is meson's sanitizer notice),
`tools/m2_layout32.sh`: OK.

## 2. Bodies compared with the exe (cataclysm_gog_r6, `tools/m2_fdec.py` / `m2_fdis.py`)

| # | body | result |
|---|---|---|
| 1 | `scriptSetTweakableGlobals` 0x00527820 | clear loop 0x00ba1fd0 step 0x14 to 0x00ba2b24 = 145 entries; AIShip / AITrack / Camera / CameraCommand / CommandLayer inlined (string copy into 0x00c52da0 on a found entry), then Formation .. FEColour (19, no Dock.script), `crateInit`, `paradeSetTweakables`: the tree's order (StatScript.c:1858). Review row correct |
| 2 | `cat_aisSetupAlliances` 0x004310a0 (+ tree AISkirmish.c:694) | bigotry 1: 3 computers 2 alliances, 4/5: 4, 6/7: 6, else none; bigotry 2: every pair i < j from the first computer to 7 = C(7,2) = **21** with one human. Report correct |
| 3 | `opCPUAttacks` 0x004db440 | not first call: the byte after `aiplayerDifficultyLevel` (aiplayerBigotry) = low byte of pData; the test's atom (status 0) takes that branch. Correct |
| 4 | `kasfDisablePlayerHyperspace` 0x004935e0 | `hyperspaceFails = 1`; the decompile's only `= 1` write of it (3 writes in all, the other two clear it) |
| 5 | `cat_kasCallFunctionByName` 0x0045b9a0 | hash 0x009c2c05 -> kasfDisablePlayerHyperspace; `cat_kasHashString("DisablePlayerHyperspace")` computed = 0x9c2c05; call_graph: its only caller is `cat_consoleKasCommand` 0x0045f270, and 0x004935e0's only caller is the dispatcher. Report correct |
| 6 | `singlePlayerGameUpdate` 0x005197d0 case 10 | NO_HYPERSPACE, ether cleanup, spUnlockout, launches, clAutoLaunch, "Game_HyperspaceFailed", hyperspaceFails = 0; `playerCanHyperspace` untouched: the tree's case `HYPERSPACE_CATFAILEDRETURN` matches; "no second jump" holds |
| 7 | `captainServerTask` 0x00451580 | three branches; only the uword at +0x18 written; +0x1A..+0x1B never; no clear. The CAT-FIX memset (size 0x1C - 0x18 - 2 = 2, static assert 0x1C) is placed after the branches, before `netcheckFillInChecksum`: correct |
| 8 | the 14 exe senders in the map (0x0044ee90 .. 0x00450780) | each `memAllocFunction(size, 0)`, no memset / zeroing; `clSendMisc` allocates 0x28 + 2n (2 padding bytes the exe leaves): the CAT-FIX claim holds |
| 9 | `nebInitTendril` 0x004c7d50 | sets flags, counter, a/b, prev/next, direction, 2 LODs, colour, maxAlpha, fadeFactor; never radius / midpoint |
| 10 | `nebRenderNebula` 0x004cadb0 | the inlined clip test reads `tendril->midpoint` for every non-flag-1 tendril: the stale-heap read is real |
| 11 | `parseLine` 0x00526120 | `strstr(line, "http:")` (0x00897e04) tested before the `//` cut, after the `;` cut: the port matches |
| 12 | `cloudZapLoop` 0x00438c30 | healthFactor(+0x14) * radius(+4) * scalar, squared on the FPU, stored float; (dz*dz + dy*dy) + dx*dx vs it, hit on <=; `fild maxhealth / fidiv hits * CLOUD_ZAP_SCALAR`: the port matches (cloudSystem offsets checked) |
| 13 | `selCentrePointComputeGeneral` 0x00508be0 | missile: `[esi+0x254]` if nonzero else 1.0 (0x00508d94..0x00508daf), `fmul; fadd [avg]; fstp` at 0x00508db5: the port matches (`catMissileScale` asserted at 0x254) |
| 14 | `collCheckShipDerelictColl` 0x0043bed0 | clamp skips at 0x0043bfcc..0x0043bff3 after the isMoving test; Hide; (dz^2+dy^2)+dx^2 `fst` then `< colldist^2` (0x0043c114..0x0043c145); rect bump; <1 -> 10 / 100; NotBumpable; `fsqrt` of the double (0x0043c197): the port matches, except the unmarked NULL guard (finding F6) |
| 15 | `trMeshSortAdd` 0x00586190 | trColorInfosPointer inlined, CRC over n*8, memcmp, growth 16 / 10: HW1's body; IDENTICAL is right |
| 16 | `cloudUpdateSystem` 0x00439130 | `fmul 0x72a4d0 (20); fadd 0x72a458` magic round; `healthFactor * radius * 0x72a5b4`; thresh only gates new bursts: the CAT-TODO's text is right |

The sample: **23 rows** merged (`delta_map.json` against 3ea7b12's: 23 changed rows, all `reviewed`); unflagged
MODIFIED/REWRITTEN 235 -> 212, outside TitanInterface.cpp 143 -> 120; IDENTICAL 2,762 -> 2,763, reviewed 3,531 -> 3,554: the
report's numbers are right. Five rows checked against the exe: parseLine, cloudZapLoop, selCentrePointComputeGeneral,
collCheckShipDerelictColl, trMeshSortAdd (rows 11-15 above), plus scriptSetTweakableGlobals and cloudUpdateSystem.
Generated files: `delta_map.py merge` on a copy of the map changes 0 records; `report` regenerates
`docs/m2-r1-function-deltas.md` byte for byte.

## 3. The diff

All 17 files are in the producer's scope (tests, the recorder's leftovers, the review sample's ports). CAT-TODO: one
added, one line, owner first (`CAT-TODO(M3 oracle physics)`, Clouds.c); none names M2-R9 / M2-R10
(`/bin/grep` count 0); stage-5 tags 23 untouched; the CAT-PROVISIONAL in Select.c retired by the port. GOG `.pkts`
tails (`tools/formats/pkts_probe.py`): 10-17 2,535 of 4,800 set, 10-18 1,159 of 1,763: the report's numbers are right.
The producer's unstated-move finding is right: `r10p1g-m3-a` = `r9i-m3-a` IDENTICAL, `r10p3b-m03` (3ea7b12) vs
`r10p1g-m3-a` first differs at 6708 in rng_all only.

### Findings

| # | finding | evidence | fixed |
|---|---|---|---|
| F1 | `MissionSelfTest.h`'s description of `/catMissionTestFailJump` said "then the test jumps again for real and the mission ends as usual" | the code ends the run 32 ticks after the return (`catMtFailJumpEndTick`, the end condition in `catMissionSelfTestTick`); fj3 run ends at 17667 with `next=-1` | yes, comment rewritten `[audit: ...]` |
| F2 | the `/catMissionTestFailJump` option help (main.c commandOptions) said "its return is judged, then the real jump." | as F1 | yes, "the run ends 32 ticks later (no second jump)" |
| F3 | 15 bodies the map calls IDENTICAL were edited (CAT-FIX) without a review entry: `nebInitTendril`, `captainServerTask`, `clSendAttack`, `clSendDock`, `clSendLaunchMultipleShips`, `clSendMisc`, `clSendRuTransfer`, `clSendAutoLaunch`, `clSendSetAlliance`, `clSendResearch`, `clSendCreateShip`, `clSendDeterministicBuild`, `clSendBuildShip`, `clSendCollectResource`, `clSendProtect` (`clSendSpecial` had an old entry without the edit) | delta_map.json rows: class IDENTICAL, reviewed false, not in delta_map_review.json; precedent: CAT-FIX edits of IDENTICAL bodies carry entries (e.g. 0x0047c590) | yes: 15 entries `[audit: M2-R10 tests-leftovers]` (sample `audit-R10-tests-leftovers`, class IDENTICAL kept, each body compared: row 7 / 8 / 9 above) and a note on 0x0044f090; merged (records changed 16; reviewed 3,554 -> 3,569; classes unchanged); `m2-r1-function-deltas.md` regenerated; the report annotated in section 5 |
| F4 | `clPktCommandAlloc`'s description listed 15 senders (no "god") while 16 got the memset | 16 `+    memset(packet, 0, sizeofPacket);` lines in the diff, `clSendGodSync` among them | yes, list completed |
| F5 | `captainServerTask`'s CAT-FIX comment gave "10-17 / 10-18 .pkts: 2,535 of 4,800" (the 10-17 count only) | pkts_probe as above | yes, both counts |
| F6 | `collCheckShipDerelictColl`'s `clampInfo != NULL` tests are an HWI guard the exe has not (0x0043bfd8 / 0x0043bfeb read `[clampInfo]` unguarded), unmarked | disassembly above | yes, marked CAT-FIX in the comment |

All fixes are comments, the help string, review entries and the generated map / doc: no stream can move by them (the
final mission pair below was run on the fixed source's build and is unchanged).

## 4. Runs (plain harness, the audit's debug build unless noted)

| run(s) | result | stream | against |
|---|---|---|---|
| `r10a3-m06-{a,b,c,d}` (`--mission 6 --mission-test-ticks 90000 --timeout 3600`) | 7/7 PASS, done 31011, ASan 0 | **all four `d0ac2eeb9a668f8b`** | `r10a2-m6-b`: rng_all only from 22235 (every other field IDENTICAL through 30979); = producer's |
| `r10a3-m01-{a,b}` | 6/6 PASS, 11424, ASan 0 | `15c7ac5528a7f81c` pair | IDENTICAL to `r10a2-missiontest-a`; = producer's |
| `r10a3-m09-{a,b}` | 6/6 PASS, 20617 | `814390fe68591cdb` pair | = producer's `r10p3g-m09`; vs `r9i-m9-a` first 3242 ships_rot (producer 1's move, as stated) |
| `r10a3-m14-{a,b}` | 4/4 PASS, 22338 | `5bafe922231f0438` pair | IDENTICAL to `r10a2-m14-a` |
| `r10a3-m17-{a,b}` | 4/4 PASS, game end tick=15584 | `fa1b4c657f887752` pair | IDENTICAL to `r10a2-m17-a` |
| `r10a3-fj3-{a,b}` (`--mission 3 --fail-jump --mission-test-ticks 90000 --timeout 3600`) | armed / begun 17472, return 17635 ok, judged PASS, done 5/5 PASS, ASan 0 | `4c6b1299c1b6b9ba` pair (= producer's) | vs base `r10p3b-m03`: first 17634 (ships_pos, ships_vel, univcheck), as stated |
| `r10a3-recb-{a,b}` (`--skirmish-ticks 5000 --skirmish-bigotry 2 --record hwibig04 --record-stop 4800 -- /catVirtualAudio`) | PASS 63 ships, `bigotry=2`, `record stopped ... randSyncErr=0 univSyncErr=0 blobSyncErr=0 pktSyncErr=0`, ASan 0 | `2d5fab41f3b25838` pair; **`.pkts` a = b = `135fc5d57c48c8be`** (= producer's), tail set in 0 | stream IDENTICAL to `r10p3g-recb-a`; `recording_compare.py` vs GOG 10-17: SAME FORMAT, alliance 21 {8: 21} both |
| `r10a3-playbig-{a,b}` (`--mode replay --recording hwibig04 --recording-src <R>/r10a3-recb-a/settings/SavedGames/RecordedGames --recording-as hwibig04a3 --timeout 900`) | all sync errors 0, ASan 0 | `3a5e426cde695b0d`, a = b (`cmp`) | vs the record run: `hwo_compare.py <R>/r10a3-recb-a/run.hwo <R>/r10a3-playbig-a/run.hwo --from-tick 1 --to-tick 4800 --fields rng10,ships_n,ships_misc,ships_pos,ships_vel,ships_health,ships_rot,missiles,bullets,derelicts,resources,univcheck,players,dt,total,real` IDENTICAL (and `fpcw`); IDENTICAL to `r10p3g-playbig-a` |
| `r10a3-skirmish-lfe2`, `-armed8` (defaults) | PASS 91 / 54 | `ca7a75badc8f2842`, `9492e24f4971db3f` | IDENTICAL to `r10a2-skirmish-lfe2` / `r10a1-skirmish-armed8`: unchanged |
| `r10a3-play01-a`, `-play02-a` (hwirec01 / 02) | ASan 0 (02's sync errors 1909/1984/11 as producer 1's) | `d635826dfea7a548`, `21a5fbce968decac` | IDENTICAL to `r10p1g-play0{1,2}-a` and `r10p3g-play0{1,2}-a`: unchanged |
| `r10a3-tutorial-{a,b}` | 12/12 PASS, 10444 | `77f00ad417ceffe6` pair | IDENTICAL to `r10a2-tutorial` |
| `r10a3-fe` (120 s) | Main_game_screen, ASan 0, no leak | | |
| `r10a3-sp` (`--timeout 340`) | universe running, ASan 0, no leak; levels Mission01_NIS01, 01, 02, 03, Mission04_NIS02 by 340 s; Animatic01..04 skipped (headless); UBSan only Particle / ETG / BTG misaligned kinds | | (producer's 400 s run reached Mission04) |
| `r10a3-mission-{a,b}` + perturb (`mission_test.sh --ticks 6000 --perturb 3000 --copy-tick 4000`) | MISSION TEST PASS, ASan 0 | `65456c876ff727cc` (1,476,370 B) pair | IDENTICAL to producer 2's audited `r10a2-mission-a` and to R9's `r9i-mission-a` |
| `r10a3-mission-rel-{a,b}` (release, `--no-perturb-run`) | PASS | `4a699037f527783e` pair | IDENTICAL to `r10a2-mission-rel-a` |
| `r10a3f-mission-{a,b}` (**after the audit's fixes**, `--no-perturb-run`) | PASS, ASan 0 | **`65456c876ff727cc`** pair | IDENTICAL to `r10a2-mission-a` |
| `r10a3-r17`, `r10a3-r18` (replays 10-17 / 10-18) | ASan 0 | `5f1acd4e3daf051b`, `819c1a0abc28644e` | IDENTICAL to `r10a2-r17` / `r10a1-r18-a` |
| `r10a3-ai`, `r10a3-ai-soak` (400 / 4,000) | PASS, ASan 0 | `20f2257aac535352`, `15c3f19947070a97` | IDENTICAL to `r10a2-ai` / `r10a1-ai-soak` |
| `r10a3-warp10`, `r10a3-build`, `r10a3-loadgame` | PASS | `e5b4c7a105101506`, `cc7f42e943762505`, `30a208dcdefbd8b6` | IDENTICAL to `r10a2-warp10` / `r10a2-build` / `r10a2-loadgame` |

The producer's 17 mission pairs (`r10p3g-mNN-{a,b}`) read from their files: all 17 byte-identical pairs, PASS with every
objective, `inOrder=1`, ASan 0, the hashes of the report's table.

## 5. DONE WHEN, re-checked

- Mission06 byte-identical over 4 runs in every field: **yes** (`d0ac2eeb9a668f8b` x4).
- Every mission 01-17 PASS as a byte-identical pair on the virtual audio clock, moves stated: **yes** (01, 06, 09, 14, 17 rerun;
  all 17 read); the only move of this step is Mission06's rng_all from 22235 (the nebula CAT-FIX); the virtual clock is a
  no-op with sound off (the mixer never starts), as the report says.
- Failed-jump return under a test (PASS, pair, ASan 0): **yes**.
- Combat: deferred to an M3 owner with measured reasons (runs `r10p3-xf*-m1` exist): **yes**.
- Bigotry flag documented with what it carries; recording with alliances replaying as a pair IDENTICAL to its record run:
  **yes**.
- `.pkts` tail zeroed, two record runs' `.pkts` byte-identical, playbacks unchanged: **yes**.
- `scriptSetTweakableGlobals` reviewed, the sample stated (23 rows, 143 -> 120), merged: **yes** (plus the audit's 15
  entries, F3).
- Headless Mission01 stream IDENTICAL to producer 2's audited stream: **yes**. Tutorial1 leg a pair: **yes**. fe / sp ASan
  0: **yes**.

## 6. Precision

44 sampled checks: the 16 body comparisons, 5 data / document checks (sample counts, generated files, GOG tail counts,
the Mission03 attribution, the 17 producer pairs), 17 run checks (section 4's rows as judged against the report's
claims), and the 6 findings. Correct 38, wrong 6 (F1-F6). **Precision = 38 / 44 = 0.864.** Fixes in place: 6.

## 7. Reference streams for the next port

Headless Mission01 (debug) `65456c876ff727cc` (1,476,370 B), release `4a699037f527783e`; missiontest Mission01
`15c7ac5528a7f81c`, 06 `d0ac2eeb9a668f8b`, 09 `814390fe68591cdb`, 14 `5bafe922231f0438`, 17 `fa1b4c657f887752` (the others:
the report's section 6 table); failed jump `4c6b1299c1b6b9ba`; Tutorial1 `77f00ad417ceffe6`; skirmish LFE2
`ca7a75badc8f2842`, Armed8 `9492e24f4971db3f`; bigotry record `2d5fab41f3b25838` (`.pkts` `135fc5d57c48c8be`) and its
replay `3a5e426cde695b0d`; playbacks `d635826dfea7a548` / `21a5fbce968decac` (build identity in the header differs per
build; the content equals `r10p1g-play0{1,2}-a`); replays 10-17 `5f1acd4e3daf051b`, 10-18 `819c1a0abc28644e`; ai
`20f2257aac535352`, soak `15c3f19947070a97`; warp10 `e5b4c7a105101506`; build `cc7f42e943762505`; loadgame
`30a208dcdefbd8b6`.

Verdict: PASS-WITH-FIXES - every DONE WHEN item reproduced on the plain harness (Mission06 x4 identical, the failed-jump and bigotry pairs, .pkts identical, no unstated stream move); 6 fixes: two wrong failed-jump descriptions, 15 missing review entries for CAT-FIX edits of IDENTICAL bodies, an unmarked NULL guard, two comment counts
