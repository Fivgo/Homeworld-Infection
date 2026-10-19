<!-- done: exit-test, ports, deltamap, oracle-a, left, r8-plan, decisions -->
# M2-R7 summary: stage 4 (the AI) with the oracle's first divergence first

Status: **COMPLETE, 2026-10-18.** Branch `m2`, worktree `.claude/worktrees/m2`, workflow `cycle/workflows/m2-r7.js`
(run id `wf_b238b029-e72`), run from the user's terminal session 38027cce (Fable), not from the orchestrator. 9 agents:
four producers in sequence (the four rows of docs/m2-r6-summary.md "Plan for M2-R7": the oracle's first divergence + the
AI ship and utility code, the AI orders / moves / handlers / events with the new `ai` self-test, the AI player / fleet /
resource managers / teams with the Cataclysm skirmish AI, the R6 render / UI leftovers), four auditors (each checkpointed
its port: `a3cc7f4`, `1410c22`, `9b34dda`, `3e33fee`) and this integrate step. No producer was cut off (no WIP snapshot);
producer 3 handed off with its final gates still running, and its auditor reran them. User decisions: cycle/STATE.md
"## M2" (the faithfulness defaults and decisions 17-55 applied; decision 49, the x87 rounding, made this round; decision
23, `fsqrt`, exercised once as the one experiment the round allows and left at the default).

**R7 exit test (the stage-4 exit): met.** From a clean build on the plain harness (silent, `tools/m2_harness.sh` only;
46 runs, all through the harness): (a) the R6 gates hold: two 6,000-tick headless Mission01 runs of the debug
`hwi-engine` (ASan + UBSan) are byte-identical with ASan 0 and so are two release runs, both pairs with the perturbed run
and the flipped copy caught (`MISSION TEST PASS` twice), on the stream **`39a512d0465d73a6`** (this step's build), which is
**IDENTICAL in every field over all 6,001 ticks to R6's `49240abcf9f0ed97`** (only the header's build identity differs,
9e20b5bd vs 3e33feef): **no field moved, from any tick.** The AI ports did not move the Mission01 stream after all,
because Mission01's computer player never gets fleet control under `CAT_HW1_MISSION_SCRIPTS 0` (the KAS call that sets
`giveComputerFleetControl` does not run), so the managers, orders and moves never execute there; `rng6` keeps the exe's
per-ship timer draws of R6. The AI is exercised instead by the new `ai` self-test, whose stream is the AI guard-rail
("Exit test" below). sp reaches the running universe with the Cataclysm HUD and renderers drawing (**3,264 ticks** at 340
s on a loaded machine, ASan 0); fe `Main_game_screen` with 0 UBSan reports; statics, meshes, textures, oracle and etg give
R6's numbers; the release oracle self-test says `SELFTEST PASS`; every quitting run returns rc 0; `tools/m2_layout32.sh`
OK; the twelve stage-3 self-tests and this round's `ai` complete with `result=PASS` and ASan 0 (ai 16/16, two runs
byte-identical); the `/noInterp` dumps of the motion and nis modes still equal the pre-feature hashes `b2af0d6decdf6474` /
`3f04c837e605ba23`. (b) **The oracle's next rung:** the replay of `oracle01` (the 10-17 recording, replayed from its
archived copy: the install's file was re-recorded during the round, "Oracle mode A" below) is **IDENTICAL to the GOG
stream `oracle01-rec.hwo` in every simulation field of the list through tick 176**, the tick-136 health divergence gone;
the first divergence is **tick 177, `ships_vel`, ship 35**, localised by producer 1 to the turn of a Worker from tick 171
(`aitrackHeadingFunc` 0x00418de0 / `vecNormalize` 0x00550770 with the exe's table `fsqrt` 0x00472860 on a 53-bit
argument), with the fix proposed (decision 23 plus the x87 shape of the vector helpers; confirmation target the inverted
GOG velocity `(c20420c2, c110b4d6, 41d3f9da)`); every per-field tick equals docs/m2-r7-oracle-rung-aiship.md and its
audit. (c) No `CAT-TODO` / `CAT-PROVISIONAL` in `src/` names an M2-R6 or M2-R7 owner and no "stage 4" tag is left: 71
owner tags on 80 lines, all stage 5 (25) or stage 6 (46). Stage 4 is closed except for the items under "What is left"
(every one with an owner); the oracle's rung and stage 6 (missions) are next.

## Exit test (this step, from a clean build at 3e33fee)

**Builds.** `tools/build.sh --debug --clean` and `--release --clean` on the tree at `3e33fee` (the r6-leftovers audit
checkpoint): both rc 0, `[319/319] Linking target hwi-engine` (R6: 315; the four new `.c` files of the round), **0
`warning:` and 0 `error` lines** under `-Werror` (the only WARNING line is meson's sanitizer notice), 12 s and 10 s of
wall time on the loaded machine.

| build | size | sha256 |
|---|---:|---|
| debug (ASan + UBSan) | 87,477,488 B | `38cb4926a7c69b48…` |
| release (`-O2`, no sanitizers) | 5,415,480 B | `520c68238aa56abd…` |

`tools/m2_layout32.sh`: OK (`Ship` 1596 = 0x63C, `Player` 25904 = 0x6530, `Universe` 252348, `CommandToDo` 816, the 45
ship specifics; since this round also `AITeamMove` 0x170, `AIEvents` 0x88, `AIPlayer` 0x1D18 and `AISTeamEntry` 0x9D10).
`python3 -I tools/m2_save32_gen.py --check`: current (108 layouts).

**Harness.** `tools/m2_harness.sh` on `original/scratch-m2/m2r2-harness/data` (the GOG install's own files, no CET, no
workarounds), offscreen video on the NVIDIA GL driver, dummy audio (docs/CONSTRAINTS.md 5), no input. The two binaries
were copied to `original/scratch-m2/m2r7-integrate/{debug,release}/` first so that no rebuild could touch a running test.
Logs are in `original/scratch-m2/m2r2-harness/runs/r7-integrate-*/` (46 runs, `runs.txt` lines 1288-1333). The runs
were made from four parallel chains on a loaded machine (load average 12-15: the user's game, the editor and the other
agents of the session; the producers' and auditors' 214 runs of the round ran beside the same load), so the wall times
are 1.3-1.5x R6's; the milestones and the sanitizers are the gate, not the wall clock.

**This step's one tool edit** (`[integrate: M2-R7]`): the replay mode of `tools/m2_harness.sh` takes `--recording-src DIR`
(the directory the pair is copied from; default the install's `SavedGames/RecordedGames`) and `--recording-as NAME2` (the
name it is copied to in the harness data dir and replayed under), so an archived recording can be replayed next to the
install's current one without touching either (the engine names the packets `<name>.pkts`, `OracleDump.c`
`oracleStartReplay`). The default path is unchanged. No `src/` file changed in this step.

| run | mode | rc / wall | milestone | first stop | sanitizers and checks |
|---|---|---|---|---|---|
| `r7-integrate-mission-a`, `-b` (`tools/oracle/mission_test.sh --ticks 6000 --perturb 3000 --copy-tick 4000`) | mission, 6,000 ticks, debug | **0** / 25.6 s and 24.7 s | **oracle mission run done** (`ticks=6000 records=6003 ships=154`) | none (rc 0) | **ASan 0** in both, no unsuppressed leak; 3,783 file loads each; `cmp` equal, **1,476,370 B, sha256 `39a512d0465d73a6`**; `hwo_compare.py`: `IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000)`. **Against R6's `r6-integrate-mission-a` (`49240abcf9f0ed97`): `IDENTICAL: 6001 ticks`, `HEADER: same producer, different build identity: 9e20b5bd/1fbfa32f vs 3e33feef/1fbfa32f`** (every field, the 27 RNG streams included); against the r6-leftovers audit's `aud7d-final-mission-a` (`5bab894e28d6dc7b`): IDENTICAL |
| `r7-integrate-mission-perturb` | mission, 6,000, `--perturb 3000` | 0 / 24.5 s | oracle mission run done | none | ASan 0; `FIRST DIVERGENT TICK 3000`, origin `rng10` (`CAND is 1 draw(s) ahead`) |
| copy of A with bit 0 of `ships_health` flipped at tick 4000 | | | | | `FIRST DIVERGENT TICK 4000`, field `ships_health`. **MISSION TEST PASS** |
| `r7-integrate-mission-rel-a`, `-b`, `-rel-perturb` (`mission_test.sh` with the release binary) | mission, 6,000 ticks, **release** | 0 / 6.0 s each | oracle mission run done | none | `cmp` equal, sha256 `5855c57c68cdaaa3`; vs run A: `IDENTICAL: 6001 ticks` (header build flags 1fbfa32f vs 1afeb7ac): the -O2 build gives the same stream as the ASan build; the release perturbed run caught at 3000 / `rng10`, the flipped copy at 4000 / `ships_health`: **MISSION TEST PASS** |
| `r7-integrate-mission-noi` (`-- /noInterp`), `-nonis` (`-- /noNISInterp`) | mission, 6,000, debug | 0 / 24.7 s, 23.6 s | oracle mission run done | none | ASan 0; both `cmp` equal to run A (`39a512d0465d73a6`): the motion-interpolation guard-rail |
| `r7-integrate-replay-a`, `-b` (`--mode replay --recording oracle01 --recording-src original/oracle/oracle01-rec-20261017-140428 --recording-as oracle01-r6`) | **oracle mode A**, the 10-17 recording, debug | **0** / 49.2 s, 48.7 s | **oracle replay done** (`ticks=9601 packets=4800 ships=82 randSyncErr=3857 univSyncErr=4702 blobSyncErr=4746 pktSyncErr=0 records=14405`; `savecat32: loaded ... 1843 chunks converted, 47 ships, 0 size problems`; `replay started oracle01-r6 players=2 ships=47 tick=1 multiplayer=0`) | none (the replay quits at the end of the packets) | ASan 0, no leak; 5,015 file loads; **byte-identical**, 2,630,929 B, sha256 **`fc48b3123e8168f2`** (6 B more than the producers' 2,630,923 B: the longer name in the two event records); against producer 1's audit run `aud7a-replay-a`: `IDENTICAL: 9601 ticks and 4800 packets compared (ticks 1-9601), no difference`. The comparison with the GOG stream is in "Oracle mode A" |
| `r7-integrate-replay-rel` (the same recording, release) | replay, release | 0 / 10.8 s | oracle replay done (the same counters) | none | sha256 `6fc7344741d43057`; against `r7-integrate-replay-a` with every simulation field plus `ships_rot` and `players`: `IDENTICAL: 9601 ticks and 4800 packets` (only the excluded `rng_all` can differ, decision 50) |
| `r7-integrate-replay-new-a`, `-b` (`--mode replay`, the install's current `oracle01` of 2026-10-18) | oracle mode A, the 10-18 recording, debug | 0 / 26.0 s, 25.6 s | oracle replay done (`ticks=3527 packets=1763 ships=57 randSyncErr=987 univSyncErr=1670 blobSyncErr=1619 pktSyncErr=0 records=5294`) | none | ASan 0; byte-identical, 966,646 B, sha256 `f02e0257d569ea0b`; against the r6-leftovers audit's `aud7d-final-replay`: `IDENTICAL: 3527 ticks and 1763 packets`. "Oracle mode A" |
| `r7-integrate-sp` | sp, `--timeout 340` | 0 / 340 s (timeout) | **universe running**: last `debugScreenshots univUpdateCounter` line **3,264** (R6: 3,376 on an idle machine; the gate is 1,248) | none: still running at the timeout. 3,784 file loads (R6's count) | **ASan 0**; no unsuppressed leak; **0 `fescreen` UBSan reports**; UBSan kinds: the ETG opcode kinds (`etgdecision` 21, `etgfunctioncall` 10, `etgconditional` 8, `etgbranch` 8), `BTG.c:388` (2), `BTG.c:864` (1), `stb_image_write.h:1263` (1): R6's list exactly |
| `r7-integrate-fe` | fe, 120 s | 0 / 120 s (timeout) | **`Main_game_screen`** | none: still running at the timeout | ASan 0; no unsuppressed leak; 300 file loads; **0 UBSan reports of any kind** |
| `r7-integrate-statics` | statics | **0** / 13.5 s | ship statics self-test done | none | ASan 0, no leak. **143 of 145** ship types loaded, 2 not found (the disabled cloak-generator `.shp`), 27 other `.shp`; 3,011 file loads |
| `r7-integrate-statics-init` | statics + `/catShipStaticsStaticInit` | **0** / 13.1 s | ship statics self-test done | none | ASan 0, no leak. 143 of 145; **108 StaticInits ran** (`staticinit=next` x108, `none` x35; R4-R6: 108) |
| `r7-integrate-meshes` | meshes | **0** / 12.6 s | mesh self-test done | none | ASan 0, no leak. 1,758 listed, 1,729 `.geo/.peo` + 29 `.mad` loaded, 0 with bad objects |
| `r7-integrate-textures` | textures | **0** / 19.4 s | texture self-test done | none | ASan 0, no leak. 12,403 / 12,403 listing lookups, 10,661 registered in 3 batches and loaded, 0 not loaded, 1,742 rows with no file, 9,248 scaled down (R6's numbers) |
| `r7-integrate-oracle` | oracle (debug), 512 ticks | **0** / 11.6 s | oracle self-test done | none | ASan 0, no leak. `randSyncErr=0 univSyncErr=0 blobSyncErr=0`, 772 records |
| `tools/oracle/selftest.sh --label r7-integrate-oracle-rel` | oracle (release), 512 ticks | 0 / 1.6 s each | oracle self-test done | none | runs a and b `cmp` equal (140,723 B, sha256 `e6042eb7c87a32d1`) and `IDENTICAL: 513 ticks and 256 packets`; the perturbed run reported at tick 300 / `rng10` / 1 draw ahead; the flipped copy at tick 400 / `ships_pos`; `rngs=27`. **`SELFTEST PASS`** |
| `r7-integrate-etg` | etg, 128 ticks | **0** / 13.0 s | etg effect self-test done | none | ASan 0, no leak. 9 of 9 scripts parsed, 197 effects loaded, 0 referenced but not loaded; 618 created, 3,197 deleted while running, 1,093 left, peak 2,515 effects / 3,012 particle blocks (R3-R6's numbers) |
| `r7-integrate-dock` | dock, 5,000 ticks | **0** / 24.7 s | **dock self-test done** | none | ASan 0, no leak. `done fighters=6 outside=6 corvettes=2 carrierleg=2 processorleg=1 RUs=1850 jackPausedTick=1024 result=PASS` (R6's line); **no `AIShip.c` UBSan report any more** (R6: 1 at AIShip.c:1467; fixed by producer 1) |
| `r7-integrate-command` | command, 3,000 ticks | **0** / 21.3 s | **command self-test done** | none | ASan 0, no leak. `done tick=2998 link=252 mcv=1 unlink=285 research=1/1 mimic=1 point=1/1 result=PASS`, 0 FAIL lines |
| `r7-integrate-render` | render, 600 frames | **0** / 18.0 s | **render self-test done** | none | ASan 0, no leak. `frames=583 listed=1823 sorted=1241 ships=582 effects=1241 trails=582; lensflares=583 lensflareElements=9328 foglayers=1166; droids=2500 droidUpdates=583; result=PASS` (R6's numbers) |
| `r7-integrate-sound` | sound | **0** / 18.9 s | **sound self-test done** | none | ASan 0, no leak. `audio driver dummy`; music 43, sect 124, beast 281 blocks decoded; 88 chatter events, 84 with an id, 64 Somtaaw / 57 Beast sentences; 403 audible output blocks (into the dummy device); `result=PASS`; **UBSan in `fquant.c` / `fqcodec.c`: 0; `PATCH`: 0** |
| `r7-integrate-hud` | hud, 600 frames | **0** / 18.9 s | **hud self-test done** | none | ASan 0, no leak. `selected tick=64 band-boxed=19 player ships=154 selected=154`; **the waypoint leg (M2-R7 r6-leftovers): `waypoints start tick=420 type=4 waypoints=3 cursor=waypoint`, `draws=181 randoms=606 (expected 606)`** = 63 + 3 x 181 stream-0 draws; `sensors open tick=480 active=1 hotkeygroup=154`; `done tick=599 boxed=19 selected=154 frames=537 statusbars=6840 infopanels=537 pie=537 result=PASS` |
| `r7-integrate-ui` | ui | **0** / 16.3 s | **ui self-test done** | none | ASan 0, no leak. `selected tick=16 ships=154; popup open tick=24 rows=10; popup drawn frames=16; commands tick=40 run=26 target=resource; game stats begin tick=48 paused=1; done selected=154 rows=10 popupframes=16 commands=26 targetrows=1 statsframes=8 result=PASS` (`plug.tga` loaded while paused: the plug of r6-leftovers) |
| `r7-integrate-fescreens` | fescreens | **0** / 11.3 s | **fescreens self-test done** | none | ASan 0, no leak. `done files=22 loaded=22 screens=195 pushed=195 popped=195 wrong=0 skipped=0 listchanged=0 userregionstandins=84 result=PASS`; `optionstest: done options=34 startdefaults=34 readback=34 result=PASS`; **0 `fescreen` UBSan reports** |
| `r7-integrate-strings` | strings | **0** / 11.3 s | **strings self-test done** | none | ASan 0, no leak. `done ids=1705 mapback=1705 resolved=1681 nostring=24 wrongtext=0 wrongnostring=0 result=PASS`; ship names 145 of 145 |
| `r7-integrate-keys` | keys | **0** / 11.3 s | **keys self-test done** | none | ASan 0, no leak. `done rows=197 known=149 unknown=48 commands=147 bindings=149 startsave=1 saved=1 reload=1 reset=1 newkey=1 conflicts=1 shares=1 failures=0 result=PASS` |
| `r7-integrate-console` | console | **0** / 16.8 s | **console self-test done** | none | ASan 0, no leak. `done selected=154 frames=4 shown=31 set=24 commands=8 keys=32 closed=1 failures=0 result=PASS` (`plug.tga` loaded with the console open) |
| `r7-integrate-build-a`, `-b` | build, 7,000 ticks (`CAT_EXTERNAL_BUILD_DRIVER 1`) | **0** / 25.7 s, 26.3 s | **build self-test done** | none | ASan 0, no leak. `done tick=4205 driver=1 SU=40/100 RUs=25925 result=PASS`; `run.hwo` 1,722,370 B, sha256 `1cc8547ebf790a70` both runs, `cmp` equal; against the ai-player-skirmish audit's `aud7c-build`: `IDENTICAL: 7001 ticks` (it differs from R6's at tick 29 `ships_health` by the x87 rounding fix, as producer 2 recorded) |
| `r7-integrate-nisparse` | nisparse | **0** / 11.5 s | **nisparse self-test done** | none | ASan 0, no leak. `done nis=14 nisScripts=14 animatics=18 objects=593 events=1488 cataclysmEvents=31 unknownKeywordLines=47 failed=0 result=PASS` |
| `r7-integrate-toggle`, `-toggle-noi` (`-- /noInterp`), `-toggle-non` (`-- /noNISInterp`) | toggle | **0** / 11.5 s each | toggle self-test done | none | ASan 0, no leak. `done start=1/1 fails=0 result=PASS`, `start=0/0 ... PASS`, `start=1/0 ... PASS`; with a switch the run-only rows hold (`run-only save rows=2`: the r6-leftovers override) |
| `r7-integrate-ai-a`, `-b` | **ai**, 400 ticks (this round's self-test) | **0** / 19.0 s, 18.9 s | **ai self-test done** | none | ASan 0, no leak; 4,294 file loads. `teams tick=17 teamsUsed=50 attack=7 guard=2 recon=1; attackorder / defenceorder / reconorder 17; build tick=17 (bMultiGunCorvette); attackmove / attackcommand 33 (team 2, 11 ships, bMultiGunCorvette#286 order 1); harvest tick=33 (bWorker#308); reconmove / reconcommand 49 (bCloakedFighter#240); defencemove / defencecommand 65 (team 0, 9 ships, bHeavyCorvette#251); skirmish tick=65 (RUs=22495 SU=1119/1319 buildsqueued=13); skirmishplay / skirmishbuild (bWorker) / skirmishresearch (TECH_DRIVE_FIGHTER) 81; skirmishharvest 177; done tick=398 fleet=156 ... checks=16/16 result=PASS`; `run.hwo` 98,768 B, **sha256 `9d42fd414007fbe2` both runs, `cmp` equal**; against the ai-player-skirmish audit's `aud7c-final-ai-a` (`96f479fe44f729cb`): `IDENTICAL: 401 ticks` (header build identity only) |
| `r7-integrate-motion-noi`, `-nis-noi` | motion 224 / nis n06:26:36, `-- /noInterp` | **0** / 41.2 s and 44.1 s | motion self-test done (`result=PASS`) | none | ASan 0, no leak. Dumps: `motion.dump` 1,431 frames sampled, **sha256 `b2af0d6decdf6474`**; `nis.dump` 1,440 frames, **sha256 `3f04c837e605ba23`**: byte-identical to the pre-feature dumps (R6: `b2af0d6decdf647431d6c2adf701798cd8e98d4a614d7b69390e6df02452d1dd`, `3f04c837e605ba23d6c74c639bba4ff10a95c139f33d462bca79427d4a985029`) |
| `r7-integrate-motion-on`, `-nis-on` | motion 224 / nis n06:26:36, flags on (not required by the exit test) | **0** / 40.1 s and 46.9 s | motion self-test done | none | ASan 0, no leak. Dumps `d9a8661fcce6f4b3` (1,431 frames) and `074a70f17e316cd2` (1,440 frames): **R6's flags-on hashes exactly**, so the r6-leftovers follow-ups (the run-only switches, the clamped-object and droid blend) changed nothing in these two scenes (neither has a clamped object or a launched droid in view) |

So every row of the R7 exit test holds:

| condition (docs/m2-r6-summary.md "M2-R7 integrate (stage-4 exit)") | result |
|---|---|
| (a) two 6,000-tick Mission01 streams byte-identical with ASan 0, the perturbed run and the flipped copy caught, also in release; the re-baselined stream's hash and the moved fields stated against R6's `49240abcf9f0ed97`, the moves the AI ports' and nothing else | **met**: debug `39a512d0465d73a6` (A = B), release `5855c57c68cdaaa3` (A = B, IDENTICAL to debug), both `MISSION TEST PASS` (perturb 3000 / `rng10`, flip 4000 / `ships_health`); against R6: **IDENTICAL in every field over 6,001 ticks, header build identity only** (9e20b5bd vs 3e33feef): nothing moved, so there is nothing to re-baseline but the build identity. The reason is stated above (no fleet control without KAS); the AI's own guard-rail is the `ai` stream `9d42fd414007fbe2` |
| (a) sp universe running (`--timeout 340`, `univUpdateCounter` at least 1,248, ASan 0) with the Cataclysm HUD and renderers drawing | **met**: 3,264 ticks at the 340 s timeout on the loaded machine, ASan 0, no leak; the HUD and the Cataclysm object-list renderer are what the sp frames draw (the hud and render self-tests exercise the same paths with their counts, the hud test now with the waypoint cursor) |
| (a) fe `Main_game_screen` | **met**, ASan 0, 0 UBSan reports of any kind |
| (a) statics, meshes, textures, oracle, etg unchanged or better | **met**: 143/145 (108 StaticInits), 1,758/1,758, 10,661 / 0 not loaded, 0 sync errors, 618 effects: R6's numbers |
| (a) dock, command, render, sound, hud, ui, fescreens (0 `fescreen` UBSan), strings (1,705 / 1,705), keys, console, build, nisparse, toggle `result=PASS` with ASan 0; the new `ai` PASS with two runs byte-identical | **met**: all thirteen `result=PASS`, rc 0, ASan 0; `fescreen UBSan reports: 0` in every summary of this step; the ai pair byte-identical (`9d42fd414007fbe2`, 16/16); build byte-identical (`1cc8547ebf790a70`) |
| (a) release oracle self-test `SELFTEST PASS` | **met**, 27 RNG streams |
| (a) every quitting run returns rc 0 | **met**: 44 of 44 quitting runs (the 7 debug and 3 release mission runs, the 6 replays, statics, statics-init, meshes, textures, oracle, etg, the 3 release self-test runs, dock, command, render, sound, hud, ui, fescreens, strings, keys, console, build x2, nisparse, toggle x3, ai x2, the 4 motion / nis runs); sp and fe stop at their timeouts by design (rc 124) |
| (a) `tools/m2_layout32.sh` OK; clean builds 0 warnings | **met** (319/319 debug and release, 0 warnings; Ship 0x63C) |
| (a) the motion and nis dumps under `/noInterp` equal the pre-feature hashes | **met**: `b2af0d6decdf6474` / `3f04c837e605ba23`; the six 6,000-tick debug streams with the flags on, `/noInterp` and `/noNISInterp` are all `39a512d0465d73a6` |
| (b) the replay of `oracle01` twice from the clean build byte-identical and IDENTICAL to `oracle01-rec.hwo` in every field of the list through tick 176 at least, the next divergence localised and its fix proposed, matching docs/m2-r7-oracle-rung-aiship.md and its audit | **met**: `fc48b3123e8168f2` (A = B), IDENTICAL to the audit's `aud7a-replay-a`; against the GOG stream: IDENTICAL through tick 176 in every listed field (`rng6`'s stock skip at 161 excluded), first divergence tick 177 `ships_vel`, every per-field first tick equal to the report's and the audit's ("Oracle mode A") |
| (c) no `CAT-TODO` / `CAT-PROVISIONAL` in `src/` names an M2-R6 or M2-R7 owner; no "stage 4" tag left | **met**: `grep -rn -E 'CAT-(TODO\|PROVISIONAL)\([^)]*(M2-R[67]\|stage 4)' src/` is empty; the tags left are 71 `CAT-TODO(<owner>)` tags on 80 lines (the other 9 lines mention closed tags in prose): **stage 6: 46** (tutorial 12, KAS 11, plain 10, single player 9, level load 2, "level load / KAS" 1, "stage 6, KAS" 1), **stage 5: 25** (plain 9, multiplayer options 7, recorder 4, LAN 4, multiplayer 1); 8 `CAT-PROVISIONAL` (the stage-6 switch `CAT_HW1_MISSION_SCRIPTS`, the kas2c / NIS-first notes, the M2-R2 appends, four medium-confidence readings: R6's eight) |

**UBSan kinds left** (the union over the 46 runs; all non-fatal; none is `fescreen`, `PATCH`, `fquant.c` or `AIShip.c`
any more): the ETG opcode kinds of GoK's packed effect bytecode in every Mission01 and replay run (`etgdecision` 21-25,
`etgfunctioncall` 10-16, `etgconditional` 8-32, `etgbranch` 8-10 in 25 runs; `etgvariablecopy` 4 in dock, etg and the
replays) and `struct particle` (`Particle.c` 56-166 in etg, render and the replays; `ETG.c` 16-18 in etg and the 10-17
replays); `BTG.c:388` (2 misaligned `short` loads) and `BTG.c:864` (1 misaligned `udword` load) in every Mission01 run
(22); `stb_image_write.h:1263` (1, sp only); `Select.c:1166` (2, the 10-17 replay only; `384 - INT_MIN` for a far-off
ship's screen y, render-only); `Subtitle.c:1144` `index 112 out of bounds for subtheme [16]` (1, the nis mode only: n06's
subtitle theme index). Gone since R6: **`AIShip.c:1467`** (the dock mode's null member access in
`aishipFlyToPointAvoidingObjsFunc`: the exe compares the pointers, so does the tree; producer 1).

Harness facts for the next rounds, in addition to R3-R6's:
- **The `ai` mode** (`--mode ai [--ai-ticks N]`, default 400): the headless Mission01 run where the computer player gets
  a scripted factory, 30,000 RUs, a 156-ship fleet and fleet control at tick 16, then the skirmish leg from tick 65; its
  stream (`/oracleDump`) is byte-identical run to run and is the AI's guard-rail (`9d42fd414007fbe2` in this build, the
  content of the audit's `96f479fe44f729cb`); about 19 s under ASan. A longer soak hits `ETG.c:7295` (`nParams <= 8`,
  fatal in debug) near tick 1,300, with the base binary too ("What is left").
- **The Mission01 stream does not see the AI**: without KAS (`CAT_HW1_MISSION_SCRIPTS 0`) the computer player has no fleet
  control, so stage-4 ports leave `49240abcf9f0ed97`'s content alone; expect stage 6 (the KAS call, the start-position
  shuffle) to move it.
- **The replay mode** can replay an archived recording (`--recording-src DIR --recording-as NAME2`); the install's
  `oracle01` is now the 10-18 recording (1,763 packets, 3,527 ticks, about 26 s), the 10-17 one (4,800 packets, 9,601
  ticks, about 50 s on a loaded machine) lives in `original/oracle/oracle01-rec-20261017-140428/`. Judge each against
  its own session's `oracle01-rec.hwo` with the field list of "Oracle mode A"; for the 10-18 session exclude `real` too.
- **The motion and nis flags-on dumps** are still `d9a8661fcce6f4b3` / `074a70f17e316cd2`: a change of the clamp or droid
  blend is invisible in those two scenes; a scene with a clamped child or a launched droid is needed to measure it.

## The four ports

| # | item | what landed | DONE WHEN | audit | commit |
|---|---|---|---|---|---|
| 1 | the oracle's first divergence, then the AI ship and utility code ([report](m2-r7-oracle-rung-aiship.md), [audit](audits/m2-r7-oracle-rung-aiship.md)) | **The x87 single rounding of `cat_clExternalBuildUpdate` 0x0044dfc0** in `clExternalBuildAdvance` (CommandLayer.c): the exe's `fld / fsub / fmul / fadd / fst` on float operands at the 53-bit control word (0x027f in all 12,282 GOG records) is the increment evaluated in double and rounded once at the store, the clamp on the double sum; the tick-136 health divergence is gone (`ships_health` now first differs at 1934). **The next divergence localised**: tick 177 `ships_vel`, ship 35 (an sWorker of player 0 starting to harvest), GOG's velocity recovered by inverting the FNV-1a hash (one solution in 1.2e9 candidates: `(c20420c2, c110b4d6, 41d3f9da)` against ours `(c20420cf, c110b44d, 41d3f9d1)`, -13 / +137 / +9 ulps), the thrusters saturated so the velocity is a pure function of the coordsys the ship built turning in place from tick 171 (`aitrackHeadingFunc` 0x00418de0 / `vecNormalize` 0x00550770: `fstp qword` of the 53-bit magnitude into the TABLE `fsqrt` 0x00472860, a double reciprocal, one rounding per component; the tree takes `sqrtf` of a float); **the one fsqrt experiment** of the round (`/oracleTableFsqrt`, `FastMath.c`, HWI-EXPERIMENT, default off and verified so): -1 / +4 / -2 ulps from GOG's, the first divergent tick still 177. **AIShip.c** (20 functions): `aishipFlyToPointAvoidingObjsFunc` 0x00414610 rewritten in the exe's order (the small-target pass skipped below `originalCollSphereSize` 500, the Ship+0x608 / `CLASS_Component` / mission-type exceptions, `cat_shipIsCloakedFully` + `cat_shipDetectedByPlayer`, pass-on-right from 30 m, no potshots, the `AIShip.c:1467` null member access fixed as the exe's pointer compare), the missile / mine bodies (`aishipmineaquiretarget` with `CAT_SHIPFLAG_KasMines`, `aishipGuideMine`'s `MINE_SEARCH_AND_STOP`), the six massive types; **AIUtilities.c** (85 + `cat_aiuWrapCapture` 0x0041f3d0): the `aiuShipIsntSelectable` / `aiuShipIsntTargetable` macros on `cat_shipIsTargetableBy`, `aiuFindCaptureTarget`, `aiuFindArmadaTarget` (the mothership alone), `aiuAttack` without HW1's `aiuFindPriorityTarget` (deleted; it drew from stream 6), `aiuWrapAttack` dropping untargetable targets and sending Leeches, `aiuRateShip` / `aiuRateBlobs` rounded to nearest (`cat_aiuRoundToUdword`), `aiuUnitCapCanBuildShip` by race range, 22 IDENTICAL found modified, 43 compared identical; **AITrack.c** (GoK's gun-gimbal reset removed from the three `aitrackSteadyShip*`: the exe has none; `aitrackHeadingFunc`'s zero-heading return) and **AIVar.c** compared; `aimProcessDock`'s rest and `aimProcessAdvancedAttack`'s list ownership; 139 reviews (105 new) | met: the replay IDENTICAL to `oracle01-rec.hwo` in every simulation field through tick 176, the next divergence owned with evidence (the hash inversion verified by the audit); the bodies at the exe's (29 compared by the audit); the mission stream unchanged at every step (`7c074b0b122f2fb5`, IDENTICAL to `49240abcf9f0ed97` but for the build identity) | PASS-WITH-FIXES, **0.93** (53/57). Fixes: two wrong exe addresses in comments (`aishipGuideMine` 0x00416783, `aiuFillInAIBlobArrays` 0x0041fc3f), the fsqrt ulps' sign, the `aiuFindPriorityTarget` statement | `a3cc7f4` |
| 2 | AI orders, moves, handlers, events, the attack and defense managers, the `ai` self-test ([report](m2-r7-ai-orders-moves.md), [audit](audits/m2-r7-ai-orders-moves.md)) | **AIEvents.c / AIHandler.c**: `aieExecute` 0x00403a70 in the exe's order (no shipDied, no fuel), 12 handler slots (`AIEvents` 0x88, the fix functions converting 12), the exe's 20-entry handler table (two bodies ICF-folded), the numbers-low handlers with Cataclysm's types (`aihHarassNumbersLowHandler` 16 sAcolyte / bHeavyCorvette ...), the distress handlers' rescue type by race (`cat_aihRescueSupportType`), the single-player script team's tactics; **AIOrders.c**: all 22 exe orders rewritten (0x0040e160..0x00410920) with the per-race alternative tables read from the exe's stack stores and the Beast rows gated by `catShipTypeEnabled`, `aioCreateFancyTakeoutTarget`'s secondary team from **`cat_statsRandomAttackShipStatic` 0x00524d70** (Stats.c, the five per-race tables 0x0089700c..); the orders with no exe counterpart removed; **AIMoves.c**: the P2 swarm moves gone (the exe's move numbers: fancy get-ships 0x16, dock 0x17, kamikaze 0x2B, delete-team 0x2D), `AITeamMove` 0x170 asserted, Aggressive as the creators' default tactics (26 creators; control / active resource keep Neutral as the exe's 1), flank / harass / defend-mothership / temp-guard / active-recon (no random tactics draw) / active-capture (`cat_aiuWrapCapture` with the dontIssue flag) ported, 7 IDENTICAL found modified (hyperspace, active patrol, resource volume, mothership move, special defense, armada, ...); **AIAttackMan.c / AIDefenseMan.c**: `aiaArmada`, `aiaGenerateAttackType` (huge by class, generators by race), `aiaProcessAttackTeams` (`r % (singlePlayerGame ? 15 : 11)`, enemies within 30,000), `aiaDivideNewShips`' Cataclysm pairs, `aiaInit`'s kamikaze draw order, `aidMothershipDefense` (the lone-fighter draw made and dropped), `aidCheckSphereOfInfluence` / `aidCloakDefense` rounded seconds; the 14 HW1-type stand-ins of M2-R4 ships-b in these files gone; 92 reviews. **`/catAiTest` + mode `ai`** (`AISelfTest.c`): the computer player of a headless Mission01 run gets a scripted factory, RUs, a 156-ship fleet and fleet control; one table of checks over the AI's own structures (teams 17, orders 17, attack move 33, recon 49, defence 65) | met: `grep -c "CAT-TODO(stage 4" AIOrders.c AIHandler.c` 0 and 0; ai PASS 10/10, ASan 0, two runs byte-identical (`2f86c13e83803037`); the mission stream (`7c224543c9ad751a`) and the replay unchanged, with the reason (no fleet control) | PASS-WITH-FIXES, **0.98** (49/50). Fixes: two missing delta-map reviews (`aiePreFixAIEvents` / `aieFixAIEvents`, edited while classed IDENTICAL), a stale `aimProcessActiveCapture` comment | `1410c22` |
| 3 | AI player, fleet and resource managers, teams, the Cataclysm skirmish AI ([report](m2-r7-ai-player-skirmish.md), [audit](audits/m2-r7-ai-player-skirmish.md)) | **New `Game/AISkirmish.c/.h`**: the 17 `cat_ais*` functions plus `aisRequestShip` 0x00431510 / `aisFleetUpdate` 0x00431750 (CET's names) at the exe's: the 34 teams of 0x9D10 bytes (static-asserted), the 8 per-player play timers (0x008dfc44), the five attack-target and six research tables (value for value against the exe's data), `cat_aisPlay` 0x00433d50's 20 / 10 / 5 s intervals and chances, `cat_aisUpdateTeams` 0x0042fd50 (4,691 B: the bounty-leader filter, the type-to-team mapping, carriers, modules, the charged big gun), `aisFleetUpdate`'s module / capital / Worker / scout / leech / mimic / cruise-missile logic, `cat_aisTeamAttack`, `cat_aisMothershipUpdate` (the own big-gun team 15: a bug the self-test found while being written), `cat_aisReset` from `gameStart`; `aiplayerUpdateAll` runs it for every multiplayer computer player, single player keeps HW1's fleet command. **AIPlayer.c / .h** at the exe's 0x1D18 bytes (30 offsets asserted; the embedded armada `MaxSelection`, `catNumSupportModulesBeingBuilt` / `catSupportModuleRequested` / `catAirNumSupportModules`), `aiplayerStartup` with the exe's arguments, `aiplayerPlayerDied`, `aiplayerGameStart` (ships under construction left out), the saves; **new `KASVar.c`** (the KAS variable store, `CatKasVar *` lookups); **the AI save chunks** of the mode-A loader (AIPlayer, RequestShips, TeamWaitingForTheseShips, AITeamMove through `SaveCat32` layouts, AITeam by the exe's offsets; `aiplayerCat32Load` gone; `oracle01` loads 1,843 chunks); **AIFleetMan.c** (`BuildShip` 0x00404470's support-unit gate and external construction, `aifBuildRequestedShips`' x87 overflow compares, `aifProcessShipBuildRequests`' research delays, `cat_aifRequestSupportModule`), **AIResourceMan.c** (`airAddNewShip`'s jump table, `airAddedResourceCollector`'s retire without harvesting, the HW1 per-type helpers removed), the AI counters of Dock.c / Salvage.c, UnivUpdate.c's converted-ship dock (1 in 10 retire, stream 6), `cat_rmAIResearchTech`'s return, `selNumShipsInSelection`; **AITeam.c** (`aitMsgSend` / `aitMsgReceived` rewritten to the exe's pointer queue with `cat_aitMsgQueueRemoveAt` / `Flush`, `aitTeamIsInMothershipRange` 0x00417470, the coop functions by race, `aitCat32LoadTeam`); the ai test's build / harvest rows and the skirmish leg (16 checks); 17 IDENTICAL found modified; 45 reviews | met (the final gates were still running at the producer's hand-off; the audit reran them): ai PASS 16/16, ASan 0, byte-identical (`96f479fe44f729cb`); the `cat_ais*` code at the exe's with the 8 timers (27 bodies sampled); `grep -rn "CAT-TODO(stage 4" src/` 0 (the 11 left after producer 2 closed; two new `CAT-TODO(stage 6 KAS)` carry the KAS message / team identity); the mission stream unchanged (`0ea19f070fede4c0`, IDENTICAL to `49240abcf9f0ed97` but for the build identity) | PASS-WITH-FIXES, **0.932** (55/59). Fixes: a missing `aiaClose` review, `cat_kasVarFind` / `FindOrCreate` returning the entry (as the exe) not the value word, a wrong naming claim, `tools/m2_layout32.sh` extended to AIPlayer.h / AISkirmish.h, a stale AITeam.h comment | `9b34dda` |
| 4 | the R6 render / UI leftovers ([report](m2-r7-r6-leftovers.md), [audit](audits/m2-r7-r6-leftovers.md)) | `rndLoadShamelessPlug` 0x00579fa0 / **`rndShamelessPlug`** 0x00579ff0 from the disassembly (the ortho quad of `plug.tga` at the right edge, drawn after `mouseDraw` while `universePause` or `catUnivFrameStep`; freed by `mainCloseRender`); **the waypoint cursor 0x004e6e70** (`cat_pieWaypointCursorDraw`, not in the export: the white path, three circles of radius 200 about each waypoint's axis turning by `(10 + r * 10 * 2^-32) * dt` degrees with one `ranRandomFn(0)` per waypoint per frame and 63 jitter draws on the first frame, the move-line colour by path type) and **the pie plate's line callback 0x004e6530** (the ship list with the Cataclysm filters, `cat_fowCanPlayerSeeShip`, the pings through **`cat_pingListGet`** 0x004e8840, the crates; chime 0x118), `piePointModeOnOff`'s UI id 0x116, HW1's per-frame reset of the pie callbacks removed from `mrRegionDraw`; **`selShipUnderMouse`** 0x00506550 (one parameter, the sensors-manager branch with the FOW test, the nearest distance before the type checks, a clamped component names its host, derelicts only gates / salvageable; one HWI line kept: the cursor-text expiry that `mouseCursorTextDraw` still reads); **the `mrHoldRight` / `mrHoldLeft` stand-ins retired** (every tester reads `catMrRightMode`, decision 36); **the ETG review**: all 47 `etgEffectCreate` sites of the exe compared with the tree's 49 calls (the tree already passes the same 32-bit words; `kasfSpawnEffect`'s integer fixed to the exe's real32; the audit found `startRepairEffect` 0x005c22c0's 150.0 width cap and ported it); **the motion-interpolation follow-ups** (every line `HWI-FEATURE(motion-interp)`): `/noInterp` / `/noNISInterp` are run-only (`miSwitchOff`, the saves write the options file's values, Cancel restores the override, a click ends it; the toggle test checks it), objects clamped to a blended host drawn at their clamp offset in the host's blended frame (`miGameplayClamped`), the repair droids' owner-space walk captured and blended, the camera-derived values measured (no simulation field moves; `rng_all` differs from the mixer thread, not the blend); `cat_cpValueGradientCreateSized` not ported (glcompat only, decision 28); **HW1's blob render and the sensor weirdness removed from Sensors.c** (1,730 lines, decision 42); the `Memory.c` tag settled (the exe's `memStartup` seeds the fill patterns from the user name's crc32 or `ranRandomFn(26)`: the tree's `MEM_VOLATILE_CLEARING`); the hud self-test's waypoint leg; 13 reviews | met: `grep -rn "CAT-TODO(M2-R7" src/` 0 (was 4, plus `CAT-TODO(Memory.c)` closed); the `/noInterp` dumps `b2af0d6decdf6474` / `3f04c837e605ba23`; the toggle test PASS with and without the switches; the mission stream unchanged (`5bab894e28d6dc7b`, IDENTICAL to producer 3's) and the replay unchanged; the hud test's `draws=181 randoms=606` | PASS-WITH-FIXES, **0.9375** (30/32). Fixes: `startRepairEffect`'s 150.0 cap (code + review), the `mrHoldRight` equivalence sentence, `cat_taskRealTimeElapsed`'s address 0x00a6b20c in two comments | `3e33fee` |

**Round size** (`git diff 4c31f6e`, the atlas-tools merge after R6's integrate, to the tree at `3e33fee`): 87 tracked
files changed, +10,886 / −6,426 lines (plus this step's four files: the harness, the two map files' done marker, this
summary and cycle/STATE.md); `src/` alone: 71 files, +6,974 / −5,966, with **500 added lines carrying `CAT`**; 7 new
`src/` files (`Game/AISelfTest.c/.h`, `Game/AISkirmish.c/.h`, `Game/FastMath.c`, `Game/KASVar.c/.h`), 0 deleted
(`AIOrders.c`, `AIMoves.c`, `AITeam.c`, `AIFleetMan.c`, `AIResourceMan.c` and `Sensors.c` lost their HW1 / P2 bodies in
place). The tree now carries 3,808 `CAT:` lines (R6: 3,533), 80 lines carrying `CAT-TODO` (105; 71 owner tags, all stage
5 or 6), 8 `CAT-PROVISIONAL` (8), 15 `CAT-DORMANT`, **134 `CAT-FIX`** (121), 22 `HWI-ORACLE`, 441 `HWI-FEATURE(motion-interp)`
(433), 54 `HWI-TEST`, 4 `HWI-EXPERIMENT` (the fsqrt switch). Tools: `tools/m2_harness.sh` (mode `ai`, the hud waypoint
leg, the toggle run-only lines, this step's `--recording-src` / `--recording-as`), `tools/m2_layout32.sh` (the AI
layouts), `tools/m2_save32_gen.py` (the AI chunk roots). `runs.txt` logs 260 harness runs this round (lines 1074-1333),
46 of them this step's.

## Delta map after the round

The ports recorded their comparisons as reviews in `ghidra/delta_map_review.json` and merged them with `tools/delta_map.py
merge` (decision 55; no producer wrote into the map directly this round, and the audits appended their own rows: 2, 1
and 1). Notes tagged `[review: M2-R7 <port>]`: oracle-rung-aiship 139 (105 new), ai-orders-moves 90 (+2 audit),
ai-player-skirmish 44 (+1 audit), r6-leftovers 12 (+1 audit); this step added no review (its only `merge --done
m2r7:integrate` changed 0 records and appended the marker; `docs/m2-r1-function-deltas.md` regenerated for it).

| class | M2-R6 end | M2-R7 end | M2-R7 bytes |
|---|---:|---:|---:|
| IDENTICAL | 2,942 | **2,854** | 517,717 |
| MODIFIED | 1,394 | **1,482** | 705,215 |
| REWRITTEN | 84 | **84** | 15,915 |
| hand-reviewed | 2,816 | **3,032** | |

Per stage (`python3 -I tools/m2_stage_counts.py --md` on the map after this step): HW1-matched IDENTICAL / MODIFIED /
REWRITTEN functions (MODIFIED + REWRITTEN bytes), then cat-new by the tool's rules.

| stage | M2-R6 end | M2-R7 end | cat-new (defined in `src/`) |
|---|---|---|---|
| 1 | 201 / 114 / 0 (57,114 B) | 201 / 114 / 0 (57,114 B) | 76 / 27,383 B (70) |
| 2 | 758 / 614 / 30 (255,297 B) | 757 / 615 / 30 (255,536 + 4,444 B) | 808 / 180,868 B (760) |
| 3 | 847 / 337 / 14 (199,754 + 8,558 B) | 847 / 337 / 14 (199,754 + 8,558 B) | 450 / 139,196 B (428) |
| **4** | **386 / 91 / 2 (50,741 + 169 B)** | **299 / 178 / 2 (77,515 + 169 B)** | **23 / 14,404 B (23 of 23)** |
| 5 | 406 / 114 / 12 (74,848 B) | 406 / 114 / 12 (74,848 + 1,179 B) | 67 / 17,483 B (18) |
| 6 | 344 / 124 / 26 (40,448 B) | 344 / 124 / 26 (40,448 + 1,565 B) | 2,365 / 656,918 B (2,085 kas2c; 45 of the 280 others) |

What moved: **88 functions the map called IDENTICAL were found modified** by the ports and their audits (87 in stage 4:
oracle-rung-aiship 22, ai-orders-moves 48, ai-player-skirmish 17 + the audit's `aiaClose`; 1 in stage 2: the r6-leftovers
audit's `startRepairEffect`), 2 MODIFIED were read as identical after all (inlining only: `aiuFindBestUnarmedEnemyShip`,
`aiuFindBestFighterVulnerableEnemyShip`) and `aiplayerClose` went IDENTICAL (the exe's nullsub is the empty `aifClose`).
**Stage 4 reads 0 unread**: every MODIFIED / REWRITTEN function of the 14 AI files has a review (R6: 60 unread, 35,494 B),
and of its 299 IDENTICAL bodies the producers compared every one in their scope (123 of them without a review row, as the
R6 ports did for bodies read and found identical). The 23 stage-4 cat-new functions are all defined (R6: 2): the 17
`cat_ais*` + `aisRequestShip` / `aisFleetUpdate` in `AISkirmish.c`, `cat_aiuWrapCapture`, `cat_aifRequestSupportModule`,
`cat_aitMsgQueueRemoveAt` / `Flush`; `cat_shipGetAimPoint` and `cat_MoveReachedDestinationRadius` were already in the
tree. Not in the map (no HW1 counterpart): `cat_clExternalBuildUpdate`, `cat_statsRandomAttackShipStatic`, the waypoint
cursor 0x004e6e70 and the pie line callback 0x004e6530 (unlisted in the export; recorded in the r6-leftovers report),
`cat_pingListGet`, `fsqrt` / `fmathInit`.

## Oracle mode A

**The comparison result (the rung).** The replay of the user's reference recording `oracle01` of 2026-10-17 (2 players,
the user as Somtaaw against "Computer 1", 47 ships at the save, 4,800 packets = 9,600 universe ticks) in hwi-engine from
the clean build (`r7-integrate-replay-a` = `-b`, `fc48b3123e8168f2`, ASan 0) compared with the GOG game's checksum stream
of that session (`original/oracle/oracle01-rec-20261017-140428/oracle01-rec.hwo`: GOG DLL in record mode, 12,282 `T`
records, `record-start` at tick 1, `record-stop` at 9600), with the docs/m2-r6-oracle-mode-a.md arguments:

```
python3 -I tools/oracle/hwo_compare.py original/oracle/oracle01-rec-20261017-140428/oracle01-rec.hwo \
    <runs>/r7-integrate-replay-a/run.hwo --from-tick 1 --to-tick 9600 \
    --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real \
    --json <runs>/r7-integrate-replay-a/compare.json
  FIRST DIVERGENT TICK 161 (last equal tick 160); 1 field(s) differ, likely origin: rng6   (CAND is 1 draw(s) behind REF: excluded, decision 47)
  FIRST DIVERGENT PACKET 46 (REF tick 93, CAND tick 93): blobcheck 1785610478 vs 1785610479   (the stock record / playback difference, decision 47)
... the same without rng6 (--json <runs>/r7-integrate-replay-a/compare-sim.json)
  FIRST DIVERGENT TICK 177 (last equal tick 176); 1 field(s) differ, likely origin: ships_vel
    ships_vel     REF  ships_vel=740322b034279499   CAND ships_vel=445c0bd4842e549c
```

- **IDENTICAL in every simulation field of the list through tick 176.** The tick-136 `ships_health` divergence of R6 is
  gone (decision 49 made: the x87 single rounding of `cat_clExternalBuildUpdate`; `ships_health` now first differs at
  1934, behind everything else). Per field (`--fields <one>`): `ships_vel` **177**, `ships_pos` 180, `univcheck` 180,
  `resources` 243, `ships_misc` 871, `rng10` 1408, `bullets` 1917, `ships_health` 1934, `ships_n` 2048; `missiles`,
  `derelicts`, `dt`, `total`, `real`: no divergent tick in 1..9600; `rng6` 161 (the stock playback skips the AI,
  excluded); `players` 93 (stock, excluded); `ships_rot` 1 (the DLL offset defect, excluded). The engine's own sync
  counters: `randSyncErr 3857` (first frame 1425), `univSyncErr 4702` (197), `blobSyncErr 4746` (109, equal to GOG's own
  playback), `pktSyncErr 0`. **Every number equals docs/m2-r7-oracle-rung-aiship.md's final table and its audit's
  section 2.1** (the audit's `aud7a-replay-a` is IDENTICAL to this step's run over all 9,601 ticks and 4,800 packets).
- **Owning function and the proposed fix** (producer 1, verified by its audit): ship 35 (type 14 `sWorker`, player 0,
  `COMMAND_COLLECT_RESOURCES` from the `collectresource` commands of frame 84) is the first ship of the recording that
  moves at all; at tick 177 `aitrackVelocityVectorGuidance` 0x00419460 saturates all three thrusters, so its velocity from
  rest is `120,000 x (col1 - col2 + col3)` of the coordsys it built turning in place from tick 171 (`STATE_POINT_IN_DIRECTION`
  of `aishipFlyToPointAvoidingObjsFunc`, `vecNormalize` 0x00550770 and `aitrackHeadingFunc` 0x00418de0, the rotation of
  `physUpdateObjPosVelShip` 0x004e4950). The exe's `vecNormalize` keeps `x*x + y*y + z*z` on the FPU and `fstp qword`s the
  DOUBLE into the table `fsqrt` 0x00472860 (the 16,384-entry table of `fmathInit` 0x004728c0), then `fdivr 1.0` and one
  rounding per component; the tree takes `sqrtf` of a float magnitude (decision 23). The experiment with the table
  (`/oracleTableFsqrt`) brings the velocity to -1 / +4 / -2 ulps of GOG's `(c20420c2, c110b4d6, 41d3f9da)` (sqrtf: -13 /
  +137 / +9) and the rotation speed at 171 already differs between the two (`3f7b79a6` sqrtf, `3f7b7ad1` table), which
  pins the turn. **Proposed for M2-R8, first item**: (a) the user's ruling on decision 23 (the table `fsqrt` as the
  default); with it (b) the exe's x87 shape of the vector helpers on the flight path (`vecNormalize` 0x00550770,
  `vecCapVector` 0x00550950 and the other `vecCap*` / `getVectDist*` that `fstp qword` the magnitude: the double magnitude
  into the table, the double reciprocal, one rounding per component), then `aitrackHeadingFunc`'s own intermediates and
  `physUpdateObjPosVelShip`'s rotation (`fsin` / `fcos`, `fptan` in `aitrackHeadingWithBankPitchFunc`). What confirms it:
  the tick-177 velocity comes out as `(c20420c2, c110b4d6, 41d3f9da)` exactly and `ships_vel` is IDENTICAL at 177; a DLL
  with `CAT_SHIP_coordsys` 0x98 (decision 48) would show the rotation's divergence directly (tick 171 expected). A
  candidate for the same list from the ai-player-skirmish audit: `hyperspaceCost` 0x00443c30 keeps its distance and cost
  sums on the x87 across the ships (the tree rounds each step).
- **Excluded, with the reason** (decision 47): `rng6` at 161 and `players` at 93 (the stock game's own record / playback
  differences, shown by GOG's own playback stream of the same recording), `ships_rot` (the DLL hashes `Ship+0x80` where the
  coordsys is at +0x98), `rng_all` (GOG splits stream 0; the sound streams), `fpcw`.

**The recording was replaced during the round.** The install's `SavedGames/RecordedGames/oracle01` was re-recorded by the
user on 2026-10-18 at 10:13 (sha256 `28d37f75…`, `.pkts` `ebe13cda…`: 1,763 packets = 3,526 ticks; its GOG session is
`original/oracle/oracle01-rec-20261018-102253/`: 8,381 `T` records, `record-start` at tick 1, no `record-stop` event and
no "recording stopped at the stop tick" line in the DLL's log, so the recording ended by the user's hand while the T
records go on to tick 8380; the `oracle01-play-a/-b.hwo` there are copies of the 10-17 playbacks), after producer 2's
audit and before producer 3's runs. The 10-17 recording (sha256 `f9756e29…`, `.pkts` `7121f1ce…`) is kept in
`original/oracle/oracle01-rec-20261017-140428/` next to its stream, and this step replays it from there (the harness's
new `--recording-src` / `--recording-as`, copied under the name `oracle01-r6`; nothing in `original/` was written). The
oracle DLL is still the 10-17 13:39 build (`tools/oracle/dll/hook_sites.h` `CAT_SHIP_coordsys 0x80`), so `ships_rot` is
not comparable in the new session either (decision 48 stays open). **The 10-18 recording's own comparison**
(`r7-integrate-replay-new-a` = `-b`, `f02e0257d569ea0b`, IDENTICAL to the r6-leftovers audit's replay; `ticks=3527
packets=1763`): with the same field list the first divergence is **tick 2, `real`** (`universe.realtimeelapsed`, REF
615605.125 vs ours 615605.188: one 1/16-s step; the DLL's `vqpc` steps the game's clock per tick and this session's
clock is one step off from tick 2 on, a session artefact of the recording start, not simulation: tick 1 is equal in every
field); with `real` excluded the first simulation divergence is **tick 118, `ships_pos` and `ships_vel` together** (last
equal 117; `univcheck` 137, packet 68 at tick 137 `univcheck 1234771699 vs 1234771701`, `resources` 243, `rng10` 944,
`ships_misc` 1049, `bullets` 1721, `ships_health` 1746, `ships_n` 1753; `rng6` 161 and `players` 272 are this session's
stock differences; `missiles`, `derelicts`, `dt`, `total` identical over 1..3527). The user's ships move earlier in this
session, so the same class of divergence (ships in motion: the turn / flight-path vector helpers) shows at 118 instead of
177; it was not localised to a ship this step (the mode-A per-ship instrumentation is the producer's temporary patch,
"What is left" 9). Both recordings are the M2-R8 rung's inputs; the 10-17 one is the continuity reference.

## What is left of stages 3 and 4

Nothing blocks the stage-4 exit test (met above); each item has an owner:

1. **The oracle's next rung** (tick 177 of the 10-17 recording, tick 118 of the 10-18 one): M2-R8, first item (above:
   decision 23, the x87 vector helpers, `aitrackHeadingFunc`'s intermediates, `physUpdateObjPosVelShip`'s rotation,
   `hyperspaceCost`).
2. **`gunResetGimbleGuns`** (Gun.c) is uncalled since GoK's gimbal reset left the three `aitrackSteadyShip*` (the exe has
   none; "Cataclysm behaviour wins"): decision 58 (keep uncalled, or delete with the next Gun.c reader).
3. **The default move tactics** of the five creators the exe does not contain (var inc, move team index / split, move
   to, capture, resource volume) stay HW1's Neutral: no evidence either way (decision 59).
4. **AI saves**: the folded handler indices (the tree writes 16 / 19 where the exe's table writes 15 / 18 for the same
   bodies; decision 63), the `AITeamMove` `params` union the SaveCat32 generator reports (bytes 0x28-0x2b written twice;
   `oracle01`'s teams have no moves, so unexercised), the two `CAT-TODO(stage 6 KAS)` of AITeam.c (a queued message's
   kas2c id, a cataclysm.exe team's KAS identity): stage 6.
5. **Two faults outside stage 4 seen in a 4,000-tick `ai` soak** (producer 3; the base binary hits them too): the
   `ETG.c:7295` assertion `nParams >= 0 && nParams <= 8` (fatal in debug, near tick 1,300) and a UBSan null member access in
   `univRemoveAllTargetReferencesFromBullets` (`&bullet->owner->targetable` with a NULL owner, HW1 code): M2-R8, the
   combat / effects owner (decision 67). The exit test's `ai` run is 400 ticks and does not reach them.
6. **Stage-4 bodies compared without a review row**: 123 IDENTICAL bodies (each producer: "the other N compared:
   identical"), and producer 3 did not re-read the small save / fix callbacks of AIPlayer.c / AITeam.c; a later audit
   may sample them.
7. **Stage 3 leftovers that stay**: HW1's cursor text (`mouseCursorTextDraw` in `mrRegionDraw`, no Cataclysm counterpart;
   removing it changes 144 frame CRCs of the motion guard-rail: decision 65, the user's); the **30 other `soundEvent(NULL,
   UI_*)` sites** in src/Game and src/SDL on HW1's numbering (the two read this round are Cataclysm's HW1 id + 13: 0x109 ->
   0x116, 0x10b -> 0x118; each site's exe push must be read): M2-R8 with the sound owner; the uncalled HW1 sensors helpers
   `smClickedOnPlayer`, `smPlayerNamesDraw`, `smEyeFactorGet` / `smLookFactorGet` (the next Sensors.c reader);
   `collCheckShipShipColl`'s other exe-only tests (the physics owner); `rndLoadTarga`'s `GL_CLAMP_TO_EDGE` (decision 28);
   `DeclarePlayerWin` (stage 5).
8. **Motion interpolation**: the camera-derived render values with the flags on (`univUpdateRenderList`'s distances follow
   the blended focus; measured: no simulation field moves): the high-refresh-4k feature's owner or the user's ruling
   (decision 65); the clamp / droid blend is unmeasured by the two dump scenes ("Harness facts").
9. **Oracle tooling**: the DLL's `CAT_SHIP_coordsys` 0x98 before the user's next recording (decision 48, still 0x80 in
   `hook_sites.h`); a per-ship / per-packet dump switch in the engine (both rounds' instrumentation was temporary, outside
   git); a GOG playback stream of the 10-18 recording (only its record stream exists); `real` excluded for the 10-18
   session (decision 60).
10. **Stage 5 and the saves**: unchanged from R6's list 8 (the send bodies of move / hyperspace / formation, the
    recorder's record side, the colour checksum, `catExtra`, the `CaptainGameInfo` bytes, the multiplayer options word,
    the Titan / WON interface; the user's two saves for `savegame_probe.py`).
11. **Stage 6**: R6's list 9 plus the two AITeam.c tags and the KAS variable store's callers (`cat_kasVarFind` has no
    caller yet; the exe's `gunShootGunsAtTarget` reads an entry's value at +8).
12. **Readings kept as they are**: decisions 29, 36 (closed: the stand-ins are gone), 37, 39, 43; the ai test's fleet
    (a 156-ship reserve and a scripted enemy mothership, since the fleet manager's builds take longer than the test).

## Plan for M2-R8: the oracle's rung, then stage 6 (missions); stage 5 (LAN) is the user's call

**Sizing** (`python3 -I tools/m2_stage_counts.py` on the map after this step, the unread counts from the map's review
flags, the tag counts from `grep -rn 'CAT-TODO(stage [56]' src/`):

- **Stage 6, missions**: 344 / 124 / 26 HW1-matched functions (40,448 + 1,565 MODIFIED / REWRITTEN bytes; **106 bodies
  unread, 29,334 B**: KASFunc.c 35, SinglePlayer.c 26, KAS.c 15, Tutor.c 10, Timer.c 8, LevelLoad.c 7, Objectives.c 5;
  344 IDENTICAL to compare, 196 of them never reviewed) + **2,365 cat-new** (656,918 B): the **2,085 kas2c mission
  functions** (612,687 B, 17 missions + Tutorial1, 322 FSMs / 1,256 states, every KAS name a 28-bit hash;
  docs/mission-code-map.md) and 280 others (44,231 B; 45 defined: the 184 Cataclysm-only kasf, the KAS runtime helpers,
  `cat_spGameDifficulty`, the mission-number mapping, ...) + **46 `CAT-TODO(stage 6 ...)` sites** (tutorial 12, KAS 13,
  single player 9, level load 3, plain 10; 4 `CAT-PROVISIONAL`). The items: the kas2c code (the runtime, the Cataclysm
  dialect, the generated mission code regenerated from the exe's FSM / state maps: Mission01 and Tutorial1 first), the
  NIS-first start (`CAT_HW1_MISSION_SCRIPTS` retired), the start-position shuffle of `gameStart` (40 stream-0 draws per
  new game: moves the mission stream's `rng0` once), the task bar's removal (`tbStartup` / `tbShutdown` and their callers
  in Objectives.c, SinglePlayer.c, KASFunc.c), the letterbox callers, the tutorial buttons around the managers (12 sites),
  the KAS-hashed selection names, the console's KAS command (`cat_consoleKasCommand` 0x0045f270 on the exe's own kas2c
  table 0x00755048), `cat_spGameDifficulty` / `DifficultySetting`'s readers, `cat_trPrecacheShipTextures`' caller,
  `nebLoad_Nebula`'s chunks, the mission-number mapping (the 11 `MissionNN_NISmm` staging levels), the KAS variable
  store's callers, the two AITeam.c KAS tags. About 70 KB of HW1-matched exe code to read plus the generated mission
  code: **M2-R8 takes stage 6 part A** (the runtime, the kasf library, Mission01 + Tutorial1, the start and level load,
  the exit test) and **M2-R9 part B** (missions 02-17's generated code, the remaining single-player screens).
- **Stage 5, LAN** (nice-to-have, decision 9): 406 / 114 / 12 (74,848 + 1,179 B; **116 bodies unread, 72,102 B, of which
  92 are `TitanInterface.cpp`**, the WON interface: not applicable to a LAN-only build, stubbed; the 24 others:
  MultiplayerLANGame.c 9, MultiplayerGame.c 6, CommandNetwork.c 3, GameChat.c 3, TitanNet.c 2, NetCheck.c 1) + 67 cat-new
  (17,483 B; 18 defined) + **25 `CAT-TODO(stage 5 ...)` sites** (the recorder's record side 4, LAN 4, the multiplayer
  options word 7, multiplayer 1, plain 9: the packets' send sides of move / hyperspace / formation, the colour checksum
  0x00ab7658, `catExtra`, the `CaptainGameInfo` bytes, `DeclarePlayerWin`'s contest result). **The oracle does not need
  stage 5**: mode A replays the GOG game's packets through the receive side, which is ported; the send sides would matter
  only for a mode B (hwi-engine recording a game for the GOG build to play back), which no rung asks for. The recorder's
  record side (4 sites) is the one stage-5 piece with oracle value (recording the ai self-test for a GOG playback).

**The order recommended** (the user decides, below): the oracle's divergence fix first wherever the round sits, then
stage 6 part A, with stage 5 deferred behind stage 6 (or reduced to the recorder's record side when a mode-B oracle is
wanted). Four producers + four auditors + integrate (9 agents, as R7), in sequence:

| # | producer | scope | counts | done when |
|---|---|---|---|---|
| 1 | **the oracle's divergence** (tick 177 / tick 118), physics | decision 23 (the table `fsqrt` as the default, if the user rules so; else the experiment switch stays), the exe's x87 shape of `vecNormalize` 0x00550770, `vecCapVector` 0x00550950, the other `vecCap*` / `getVectDist*` (double magnitude into the square root, double reciprocal, one rounding per component), `aitrackHeadingFunc` 0x00418de0's intermediates, `physUpdateObjPosVelShip` 0x004e4950's rotation (`fsin` / `fcos` / `fptan`), `hyperspaceCost` 0x00443c30; a permanent per-ship / per-packet dump switch (`/oracleShipDump`); both recordings replayed | the flight-path vector helpers (about 10 functions) and the two physics bodies; the mission stream will move (every distance in the simulation) and is re-baselined once | the 10-17 replay IDENTICAL to `oracle01-rec.hwo` beyond tick 177 (`ships_vel` at 177 equal to the inverted `(c20420c2, c110b4d6, 41d3f9da)`), the next divergence owned with evidence; the 10-18 replay IDENTICAL beyond tick 118 with `real` excluded; the mission stream hash before / after with the fields stated |
| 2 | the KAS runtime, the kas2c dialect, Mission01 + Tutorial1, the NIS-first start, level load | `KAS.c` (15 unread) and the 10 Cataclysm runtime helpers (`cat_kasHashString` 0x00491860, `cat_kasCallFunctionByName` 0x0045b9a0, the hashed names), `Timer.c` (8), `LevelLoad.c` (7: the start-position shuffle with `scriptSetMissionSphereCB` / `scriptPreMissionSphereCB`, `nebLoad_Nebula`'s chunks, the mission-number mapping), the kas2c output of Mission01 and Tutorial1 regenerated from the exe's FSM / state maps (docs/mission-code-map.md 8), `singlePlayerGameUpdate` 0x005197d0's `kasMissionStart`, the NIS-first start of SinglePlayer.c (`CAT_HW1_MISSION_SCRIPTS` retired), `cat_consoleKasCommand`; 193 IDENTICAL to compare | 30 unread (about 9 KB) + the two missions' generated code (Mission01: its FSMs and states from the map) | Mission01 starts NIS-first and runs its KAS script headlessly: the mission's objectives advance through their FSM states with no input (a scripted `/catMissionTest` + mode `mission-kas`), the mission stream re-baselined once by the start-position shuffle (`rng0` from tick 0) with the fields stated |
| 3 | the kasf library | `KASFunc.c` (35 unread, the 184 Cataclysm-only kasf of docs/mission-code-map.md 5.1 with their exe addresses, the 9 ICF-folded stubs, the task bar's removal from the kasf callers, the letterbox callers, the tutorial buttons, `kasfSpawnEffect`-style parameter conversions), `Objectives.c` (5), the KAS variable store's callers | 40 unread + 184 cat-new (most small) | every kasf Mission01 / Tutorial1 calls exists at the exe's; the console's KAS command lists the exe's 486-entry table |
| 4 | single player and the tutorial | `SinglePlayer.c` (26 unread: the mission flow, `cat_spGameDifficulty` / `DifficultySetting`'s readers, the hyperspace / ether flow, the stats screen after `univKillPlayer`, the KAS end-of-mission), `Tutor.c` (10: the tutorial buttons, `tutInitTutorial` 0x0052f690), the KAS-hashed selection names, `cat_trPrecacheShipTextures`' caller, the AITeam.c KAS tags | 36 unread | no `CAT-TODO(stage 6 single player / tutorial)` left; sp from the NIS-first start reaches the running universe with the mission running (univUpdateCounter 1,248+, ASan 0) |

**M2-R8 integrate (stage-6 part A exit):** from a clean build on the plain harness: the R7 gates hold (the stream
re-baselined by the rung and by the shuffle, each move stated against `39a512d0465d73a6`'s content; sp, fe, the 19
self-tests PASS incl. `ai`, release `SELFTEST PASS`, every quit rc 0, layout32 OK, the `/noInterp` dumps unchanged);
**the stage-6 exit test: Mission01 playable from the NIS-first start through its KAS objectives in the headless harness**
(the new mission self-test: the NIS plays, the script's FSMs reach their objective states in order with scripted events
and no input, the mission ends in the KAS end-of-mission flow), the mission stream re-baselined once by the
start-position shuffle; **the oracle's rung**: the 10-17 replay IDENTICAL beyond tick 177 (or the next divergence owned);
no `CAT-TODO` naming M2-R7 or M2-R8; the stage-6 tags of part A gone or re-targeted to part B with a reason;
`CAT_HW1_MISSION_SCRIPTS` retired. Then M2-R9 (stage 6 part B: missions 02-17; stage 5 if wanted) and M3.

**What the user must decide** (none of it blocks the first item):
1. **Decision 23, the table `fsqrt`**: without it the 10-17 replay cannot match GOG at tick 177 (the experiment came
   within -1 / +4 / -2 ulps with the table alone). Recommendation: make the exe's table square root the default, since
   every distance in the simulation is computed through it in the exe; the mission baseline moves once.
2. **Stage 5 before stage 6?** Recommendation: **stage 6 first**; stage 5 is nice-to-have (decision 9), the oracle does
   not need its send sides, and only the recorder's record side (4 sites) has oracle value (a mode-B playback in the GOG
   game). Decide whether stage 5 is done at all, reduced to the recorder, or left for after M3.
3. **Does M3 (the user's play-test, the one-hour 4K@144 soak, the Windows build) start after stage 6?** Recommendation:
   after stage 6 part A plus the rung, M3's Linux play-test of Mission01 and a skirmish can start in parallel with
   M2-R9 (the other missions), since the engine, the AI and Mission01 are then playable; the Windows build and the soak
   follow the play-test's sign-off.
4. **The recordings**: whether the 10-17 recording stays the rung's reference (this step's default: yes, from its
   archive) and whether the next recording waits for the DLL's `CAT_SHIP_coordsys` 0x98 edit (decision 48; recommended,
   so that the rotation becomes comparable and the turn at tick 171 is seen directly).
5. The defaults recorded as decisions 56-67 in cycle/STATE.md (say if you want otherwise).

## Decisions

**Applied this round:**
- The faithfulness defaults and decisions 17-55 of cycle/STATE.md; **decision 49 made** (the x87 single rounding of
  `cat_clExternalBuildUpdate`: the tick-136 divergence gone); **decision 23 exercised once** (the fsqrt experiment behind
  `/oracleTableFsqrt`, default off, the default replay byte-identical to the one before the switch existed); decision 35
  (the waypoint draw ported with its stream-0 draws and the effect stated: a played game's stream 0 moves by a
  frame-rate-dependent number of draws, as the exe); decision 36 closed (the stand-ins retired); decision 42 (the blob
  render removed); decision 46's follow-up made (the run-only override); decision 55 followed by every producer.
- **Decision 44 updated**: the mission-stream baseline's content is unchanged by the round; this build's
  `39a512d0465d73a6` equals `49240abcf9f0ed97` in every field (header build identity only), so no re-baseline but the
  identity. **Decision 49 updated**: the rounding fix is made; the oracle's first divergence is now tick 177 `ships_vel`
  (the 10-17 recording) and tick 118 `ships_pos` / `ships_vel` (the 10-18 recording).

**Still open (the user's; none blocks M2-R8):**
- **8.** Two saves from the M0 play-test (one mission, one skirmish).
- **The M0 play-test sign-off** (docs/PLAN.md).
- **A second recording with the fixed oracle DLL** (decision 48): the 10-18 recording was made with the unfixed DLL, so
  `ships_rot` is still not comparable.

**Surfaced by M2-R7.** Each has a default in the tree; say if you want otherwise.
56. **The table `fsqrt`** (decision 23 restated as M2-R8's first question): the default stays `sqrtf`; the experiment
    switch `/oracleTableFsqrt` (`FastMath.c`, HWI-EXPERIMENT) stays compiled in, off by default. The replay cannot match
    GOG at tick 177 without the table and the x87 vector helpers.
57. **The Mission01 stream does not see the AI**: Mission01's computer player gets no fleet control without the KAS call
    (`CAT_HW1_MISSION_SCRIPTS 0`), so the four ports left `49240abcf9f0ed97`'s content unchanged; the `ai` self-test
    stream (`9d42fd414007fbe2` in this build; the audit's `96f479fe44f729cb`) is the AI's guard-rail. Default: no
    re-baseline with fleet control forced on; stage 6's KAS call moves the mission stream when it lands.
58. **`gunResetGimbleGuns` is uncalled**: GoK's gun-gimbal reset left the three `aitrackSteadyShip*` (the exe returns TRUE
    once steady; `/vanilla` still sets `vanillaGunsMode`). Default: the function stays in Gun.c until a cleanup.
59. **Default move tactics**: Aggressive (2) for the 26 creators the exe stores it for, Neutral (1) for control resources
    and active resource (the exe's) and for the five creators the exe does not contain (no evidence).
60. **The recordings**: the install's `oracle01` is the 10-18 recording (1,763 packets); the 10-17 one is archived in
    `original/oracle/oracle01-rec-20261017-140428/` and stays the rung's reference, replayed through `--recording-src` /
    `--recording-as`; for the 10-18 session `real` is excluded (a one-step `vqpc` clock alignment from tick 2) and its
    first simulation divergence is tick 118 `ships_pos` / `ships_vel`; `ships_rot` stays excluded for both (the DLL is
    unchanged, decision 48).
61. **Chosen names** where the exe's are unknown: `CAT_SHIPFLAG_KasMines` (catFlags18 bit 5), the AISkirmish statics and
    globals (`aisShipBeingBuilt` 0x00a2d980, `aisEnemyBigGunActiveShip` / `ChargedShip`, `aisCrystalTarget`,
    `aisOwnBigGunShip`, `aisEnemyDFGNearMothership`, `aisLeastBuiltShip`, `aisFirstPlay`, `aiBuildingShip`,
    `aiSupportUnitsPending`, `aiCarrier1Ship` / `2Ship`, `aiHasExternalConstruction`, the module-queued flags), the five
    `AIS_TEAM_*` ids where CET's names differ (10, 15, 16, 24, 25), the AIPlayer fields `catNumSupportModulesBeingBuilt`
    (+0x950) / `catSupportModuleRequested` (+0xB8E) / `catAirNumSupportModules` (+0xBC8), `catMsgQueue` / `catUnknown08`,
    `cat_aihRescueSupportType`, `cat_aiuRoundToUdword`, `cat_aiuRemoveUntargetableTargets`, `aioCatEnabledOr` /
    `aioCatAttackTactics` / `aioCatTeamTactics`, `cat_pieWaypointCursorDraw`, the KASVar store names, `miSwitchOff` and
    the other motion-interp helpers.
62. **HWI deviations where the exe would fault or leak** (13 new `CAT-FIX`, 134 in all): `aiuFindClosestEnemyShipWorthCapturing`
    returns NULL where the exe returns an uninitialised local; `aioCreateGuardShips`' Kushan / Taiidan alternatives (the exe's
    uninitialised stack), `aioCreateActiveSupport`'s empty team, `aioCreateTakeoutMothershipBig` and the three numbers-low
    handlers for a race the exe has no table for; `aimCreateGetShipsNoAdd`'s dropped allocation and `aimProcessActiveCapture`'s
    target selection (leaks); `cat_aisUpdateTeams`' zero bounty (an integer division); `cat_kasVarLoad`'s still-allocated
    array; the saved `ResourceManRequestShips.creator` pointer cleared on load; `startRepairEffect`'s cap is the exe's (not a
    fix).
63. **AI save bytes**: the ICF-folded handlers (recon-ship / remove) are written as 16 / 19 where the exe writes 15 / 18
    (both load the same body). Default: keep (a cataclysm.exe save loads; a tree save differs in those bytes).
64. **`aiplayerUpdateAll`'s branches**: multiplayer computer players run the skirmish AI (`cat_aisPlay`), single player
    runs HW1's fleet command with fleet control from KAS, the playback skip kept: the exe's.
65. **Render leftovers**: HW1's cursor text stays (removing it re-baselines 144 frame CRCs of the motion guard-rail: the
    user's call); the 30 remaining `UI_*` sound ids stay HW1's until each site's exe push is read (M2-R8, sound); the
    camera-derived render values with interpolation on belong to the high-refresh-4k feature.
66. **The harness replays archived recordings** (`--recording-src DIR --recording-as NAME2`, this step); the install's
    directory stays the default and is never written.
67. **Two faults in long AI soaks** (`ETG.c:7295` `nParams <= 8`, `univRemoveAllTargetReferencesFromBullets`' NULL owner; HW1
    code, present in the base binary): listed for M2-R8's combat / effects owner; the `ai` mode's default stays 400 ticks.
