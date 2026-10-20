<!-- done: exit-test, ports, deltamap, oracle-a, mission01, left, r9-plan, decisions -->
# M2-R8 summary: the oracle's rung, then stage 6 part A (the kasf library, the KAS runtime, Mission01 + Tutorial1 from the NIS-first start, single player and the tutorial)

Status: **COMPLETE, 2026-10-19.** Branch `m2`, worktree `.claude/worktrees/m2`, workflow `cycle/workflows/m2-r8.js`
(run id `wf_74bcf42e-330`, `ROUND_BASE 145352a`, `FSQRT_DEFAULT 'sqrtf'`), run from the user's terminal session 38027cce
(Fable), not from the orchestrator. 9 agents: four producers in sequence (the four rows of docs/m2-r7-summary.md "Plan
for M2-R8", run in the order the rung, the kasf library, the KAS runtime + Mission01 / Tutorial1, single player + the
tutorial: the library before the generated code that calls it), four auditors (each checkpointed its port: `15df4d6`,
`7d225de`, `3903f02`, `3e55019`) and this integrate step. One producer was cut off: the kasf library hit the five-hour
limit at about 15:21 (WIP snapshot `e3f05f4` at 15:35) and the workflow restarted it from that tree at 15:30; it
continued and finished. User decisions: cycle/STATE.md "## M2" (the faithfulness defaults and decisions 17-69 applied;
decision 23 / 56, the table `fsqrt`, left at `sqrtf` for the round, so the rung was judged with the HWI-EXPERIMENT switch
`/oracleTableFsqrt` on and off; decision 14's `CAT_HW1_MISSION_SCRIPTS` retired; decision 40 / 68's start-position shuffle
landed).

**R8 exit test (the stage-6 part A exit): met.** From a clean build of `3e55019` on the plain harness (silent,
`tools/m2_harness.sh` only; 62 runs, labels `r8i-*`, every one through the harness): (a) the R7 gates hold: two 6,000-tick
headless Mission01 runs of the debug `hwi-engine` (ASan + UBSan) are byte-identical with ASan 0 and so are two release
runs, both pairs with the perturbed run and the flipped copy caught (`MISSION TEST PASS` twice), on the re-baselined
stream **`0d2b76d565105a26`** (release `3dc687dbb95feab6`, IDENTICAL content); **against R7's `9f7416bb64fec98f` (R6's
`49240abcf9f0ed97` in content) exactly two things moved: `rng0` is 40 draws ahead from tick 0 (the start-position
shuffle of `gameStart`, decision 40 / 68) and `ships_rot` differs from tick 1 (the rung's x87 vector / matrix / rotation
helpers under the ships Mission01 turns); every other field (`rng10`, `rng6`, the ship, missile, bullet, derelict and
resource hashes, `statcheck`, `univcheck`, `players`, the clocks, `fpcw`) is IDENTICAL over all 6,001 ticks** (`rng_all`
follows `rng0`). sp from the NIS-first start reaches the running universe with Mission01 running (**4,496 ticks** at
340 s, the opening NIS then `Mission01/Mission01.mission` in FileLoads, ASan 0); fe `Main_game_screen` with 0 UBSan
reports; statics, meshes, textures, oracle and etg give R6's numbers; the release oracle self-test says `SELFTEST PASS`;
every quitting run returns rc 0 (60 of 60; sp and fe stop at their timeouts); `tools/m2_layout32.sh` OK; the scripted
self-tests dock, command, render, sound, hud, ui, fescreens (0 `fescreen` UBSan), strings (1,705 / 1,705), keys, console
(now with the 486-row KAS table), build, nisparse, motion, nis, toggle x3, ai (16/16 twice byte-identical, plus the 4,000-tick soak with
no fatal), skirmish (LiveFireExercise2 twice byte-identical, Armed8 once) and this round's **missiontest** (two runs
byte-identical, every Mission01 objective reached in order, the Tutorial1 leg) and **loadgame** ("01 - Hiigara" twice
byte-identical) complete with `result=PASS` and ASan 0. **One guard-rail could not hold by construction**: the motion
and nis dumps under `/noInterp` no longer equal the pre-feature hashes `b2af0d6decdf6474` / `3f04c837e605ba23`, because
the rung's shared vector and matrix helpers sit under every frame (producer 1 and its audit said so); they equal the
rung's audited dumps `0838a011db819920` / `d1e6f6e477906b52`, differ from the pre-feature ones only in the frame CRC (34 of
1,431 motion frames) and in the NIS objects' screen positions, and are **re-baselined** here (decision 46's rule: never
touch the guard-rails, so nothing else was done). (b) **The oracle's rung**, both recordings replayed twice from the clean
build, byte-identical pairs, compared with the GOG streams: **with `/oracleTableFsqrt` the 10-17 replay is IDENTICAL in
every simulation field through tick 246 and first diverges at 247 `ships_vel` (ship 40), the 10-18 replay through 154
and first diverges at 155 `ships_vel` (ship 28): the rung moved past 177 / 118, the next divergences are localised and
owned (M2-R9 physics)**; with the round's default `sqrtf` they still first diverge at 177 (the 10-17 one, ship 35: the
square root itself) and 118 (the 10-18 one): the fix at 177 is decision 23 / 56, the user's. Every per-field tick equals
docs/m2-r8-oracle-rung-physics.md and its audit. (c) No `CAT-TODO` / `CAT-PROVISIONAL` in `src/` names an M2-R7 or M2-R8
owner, `CAT_HW1_MISSION_SCRIPTS` is gone (`grep` 0), and no "stage 6" tag is left at all: the 29 owner tags are stage 5's 26
and M2-R9's 3 (part B 2: missions 02-17's generated code and the "Mine" / "Slipgate" sphere readers; physics 1:
`hyperspaceCost`'s double return). Clean `tools/build.sh --debug` and `--release`: `[253/253]`, 0 warnings.

## Exit test (this step, from a clean build of `3e55019`)

**Builds.** `tools/build.sh --debug --clean` and `--release --clean` on the tree at `3e55019`: both rc 0, `[253/253]
Linking target hwi-engine` (R7: 319; HW1's compiled missions and their kas2c steps left the build with the kasf library,
the round's 9 new `src/` files came in), **0 `warning:` and 0 `error` lines** under `-Werror` (the only WARNING line is
meson's sanitizer notice).

| build | size | sha256 |
|---|---:|---|
| debug (ASan + UBSan) | 81,645,224 B | `d1668cb564712163…` |
| release (`-O2`, no sanitizers) | 4,523,768 B | `a2b9a84352c94fc3…` |

`tools/m2_layout32.sh`: OK (`Ship` 1596 = 0x63C, `Player` 25904 = 0x6530, `Universe` 252348, `CommandToDo` 816, the 45
ship specifics; the round's pins `AITeam` 0x280, `ResourceVolume` 0xB0, `nebChunk` 0x58, `nebTendril` 0xFC, `tutpointer`
0x68, `tutGameEnableFlags` 8, `CatSpWarpLevel` 0x2F0). The generators are current: `python3 -I tools/m2_save32_gen.py
--check` (119 layouts), `python3 -I tools/m2_kas2c_gen.py --check` ("verified 149 bodies: callees and constants equal to
the machine code's, 1790 all-constant calls' arguments equal"; the four generated files current), `python3 -I
tools/m2_kasf_table_gen.py --dispatch --check` (KASFuncTable.c and KASFuncDispatch.c current).

**Harness.** `tools/m2_harness.sh` on `original/scratch-m2/m2r2-harness/data` (the GOG install's own files, no CET, no
workarounds), offscreen video on the NVIDIA GL driver, dummy audio (docs/CONSTRAINTS.md 5), no input. The two binaries
were copied to `original/scratch-m2/m2r8-integrate/{debug,release}/hwi-engine` first and every run passed them with
`--bin`, so no rebuild could touch a running test. Logs are in `original/scratch-m2/m2r2-harness/runs/r8i-*/` (62 runs,
`runs.txt` lines 1687-1748); the round's producers and auditors made 297 runs before them (lines 1390-1686). The runs were
made from five parallel chains (load average 24-30 at the start: another agent's search and the user's game beside them);
the milestones and the sanitizers are the gate, not the wall clock. **No tool or `src/` file changed in this step.**

| run | mode | rc / wall | milestone | first stop | sanitizers and checks |
|---|---|---|---|---|---|
| `r8i-mission-a`, `-b` (`tools/oracle/mission_test.sh --bin <debug> --label r8i-mission --ticks 6000 --timeout 900 --perturb 3000 --copy-tick 4000`) | mission, 6,000 ticks, debug | **0** / 28.3 s and 31.4 s | **oracle mission run done** (`ticks=6000 records=6003 ships=154`) | none (rc 0) | **ASan 0** in both, no unsuppressed leak; 3,781 file loads each; `cmp` equal, **1,476,370 B, sha256 `0d2b76d565105a26`**; `hwo_compare.py`: IDENTICAL. **Against R7's `r7i2-mission-a` (`9f7416bb64fec98f`), all fields: `FIRST DIVERGENT TICK 0`, `rng0` (`CAND is 40 draw(s) ahead of REF`) and `rng_all`; `ships_rot` alone: first tick 1; every field but `rng0` / `rng_all` / `ships_rot` (`rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,statcheck,univcheck,players,dt,total,real,fpcw`): `IDENTICAL: 6001 ticks`; the simulation list of "Oracle mode A": IDENTICAL over ticks 1-6000.** Against the rung's audited `r8au-mission-a` (`245b3d88387ed076`): `rng0` 40 ahead (and `rng_all`) only; against the KAS runtime's audited `r8k2a-final6k-a` (`19560d16033efd53`) and the last audit's `r8p4a-fix-m6k-a` (`00ae16e7f73689d9`): IDENTICAL in every field, `HEADER: same producer, different build identity` only |
| `r8i-mission-perturb` | mission, 6,000, `--perturb 3000` | 0 / 28 s | oracle mission run done | none | ASan 0; `FIRST DIVERGENT TICK 3000`, origin `rng10` (`CAND is 1 draw(s) ahead`) |
| copy of A with bit 0 of `ships_health` flipped at tick 4000 | | | | | `FIRST DIVERGENT TICK 4000`, field `ships_health`. **MISSION TEST PASS** |
| `r8i-mission-rel-a`, `-b`, `-rel-perturb` (`mission_test.sh` with the release binary) | mission, 6,000 ticks, **release** | 0 / 6.4 s, 6.3 s, 6 s | oracle mission run done | none | `cmp` equal, sha256 `3dc687dbb95feab6`; vs run A: `IDENTICAL: 6001 ticks` (header build flags 1fbfa32f vs 1afeb7ac): the -O2 build gives the same stream as the ASan build; the release perturbed run caught at 3000 / `rng10`, the flipped copy at 4000 / `ships_health`: **MISSION TEST PASS** |
| `r8i-mission-noi` (`-- /noInterp`), `-nonis` (`-- /noNISInterp`) | mission, 6,000, debug | 0 / 28 s, 27 s | oracle mission run done | none | ASan 0; both `cmp` equal to run A (`0d2b76d565105a26`): the motion-interpolation guard-rail on the stream |
| `r8i-mission-table` (`-- /oracleTableFsqrt`) | mission, 6,000, debug | 0 / 28 s | oracle mission run done | none | ASan 0; `cmp` equal to run A: the table square root does not move the headless Mission01 stream (no ship flies; producer 1's measurement reproduced) |
| `r8i-r17-off-a`, `-b` (`--mode replay --recording oracle01 --recording-src original/oracle/oracle01-rec-20261017-140428 --recording-as oracle01-r6 --timeout 900`) | **oracle mode A**, the 10-17 recording, the default `sqrtf` | **0** / 55-56 s | **oracle replay done** (`ticks=9601 packets=4800 ships=80 randSyncErr=3587 univSyncErr=4693 blobSyncErr=4746 pktSyncErr=0 records=14405`; `savecat32: loaded ... 1843 chunks converted, 47 ships, 0 size problems`) | none (the replay quits at the end of the packets) | ASan 0, no leak; 5,014 file loads; **byte-identical**, 2,630,929 B, sha256 **`6b3e3c15a42de5af`**; against the rung audit's `r8au-r17-off-a`: `IDENTICAL: 9601 ticks and 4800 packets`; against R7's `r7i2-replay-a` (the simulation list): first difference tick 177 `ships_vel` (the helpers moved ship 35's velocity). The GOG comparison is in "Oracle mode A" |
| `r8i-r17-on-a`, `-b` (the same, `-- /oracleTableFsqrt`) | oracle mode A, the 10-17 recording, **the table `fsqrt`** | **0** / 55 s, 30 s | oracle replay done (`randSyncErr=3406 univSyncErr=4611 blobSyncErr=4746 pktSyncErr=0`) | none | ASan 0, no leak; **byte-identical**, sha256 **`d88fd3c4a6e2de54`**; against `r8au-r17-on-a`: IDENTICAL over 9,601 ticks and 4,800 packets; on vs off (the simulation list): first difference tick 177 `ships_vel` (the switch's effect begins at the rung). "Oracle mode A" |
| `r8i-r17-rel` (the 10-17 recording, release, default) | replay, release | 0 / 11.4 s | oracle replay done (the same counters as `-off`) | none | sha256 `66e68c9c4cc608f1`; against `r8i-r17-off-a` with the simulation list plus `ships_rot` and `players`: `IDENTICAL: 9600 ticks and 4800 packets` |
| `r8i-r18-off-a`, `-b` (`--recording-src original/oracle/oracle01-rec-20261018-102253 --recording-as oracle01-r18`) | oracle mode A, the 10-18 recording (from its archive: the install's `oracle01` was re-recorded again on 2026-10-19), default | 0 / 31 s each | oracle replay done (`ticks=3527 packets=1763 ships=57 randSyncErr=984 univSyncErr=1664 blobSyncErr=1619 pktSyncErr=0 records=5294`) | none | ASan 0; byte-identical, 966,654 B, sha256 `9c51469ece54c88f`; against `r8au-r18-off-a`: `IDENTICAL: 3527 ticks and 1763 packets`. "Oracle mode A" |
| `r8i-r18-on-a`, `-b` (the same, `-- /oracleTableFsqrt`) | oracle mode A, the 10-18 recording, the table | 0 / 28 s each | oracle replay done (`randSyncErr=888 univSyncErr=1621 blobSyncErr=1619`) | none | ASan 0; byte-identical, sha256 `98985af637e37bf3`; against `r8au-r18-on-a`: IDENTICAL. "Oracle mode A" |
| `r8i-r19-off`, `-on` (`--recording-src original/oracle/oracle01-rec-20261019-143530 --recording-as oracle01-r19`) | oracle mode A, **the install's current `oracle01` of 2026-10-19 14:35** (133 packets = 266 ticks, 47 ships; not in the exit test) | 0 / 16 s each | oracle replay done (`ticks=267 packets=133 ships=47 randSyncErr=0 univSyncErr=21 blobSyncErr=0 pktSyncErr=0`) | none | ASan 0; sha256 `0ba6440eb923f733` / `2fdbe8493a26f164`; the GOG stream (529 `T` records, events hooks / game-new / offset-check / record-start / game-new, no record-stop): `real` off by one step from tick 2 (the 10-18 session's `vqpc` artefact, excluded); with the default the first simulation divergence is tick 179 `ships_vel` (`ships_pos` 182, `univcheck` 196), with the table **221 `ships_vel`** (`ships_pos` 223, `univcheck` 223); `rng6` 161 stock; `ships_rot` 1 (the unfixed DLL, `hook_sites.h` still 0x80); `rng10`, `ships_n`, `ships_misc`, `ships_health`, the lists, `players`, `dt`, `total` identical over 1..266. "Oracle mode A" |
| `r8i-sp` | sp, `--timeout 340` (the NIS-first start) | 124 / 340 s (timeout) | **universe running**: last `debugScreenshots univUpdateCounter` **4,496** (the gate 1,248; the audit 4,496, producer 4 4,832 at 360 s; R7's direct start 3,376), 40 screenshots to `golden-01-04496.jpg` | none: still running at the timeout. 4,368 file loads: `SinglePlayer/Mission01_NIS01/Mission01_NIS01.mission` (line 286), `nis/n01.nis` (1716), then `SinglePlayer/Mission01/Mission01.mission` (1721): the opening NIS, then the mission; "KAS has PAUSED / UNPAUSED the universe" | **ASan 0**; no unsuppressed leak; **0 `fescreen` UBSan reports**; UBSan kinds: the ETG opcode / `struct particle` kinds, `BTG.c:388` / `:391` / `:864`, `stb_image_write.h:1263`, and now `Subtitle.c:1144` (n01's subtitle theme index, the nis mode's kind) |
| `r8i-fe` | fe, 120 s | 124 / 120 s (timeout) | **`Main_game_screen`** | none: still running at the timeout | ASan 0; no unsuppressed leak; 300 file loads; **0 UBSan reports of any kind** |
| `r8i-statics` | statics | **0** / 14.7 s | ship statics self-test done | none | ASan 0, no leak. **143 of 145** ship types loaded, 2 not found (the disabled cloak-generator `.shp`); 3,011 file loads |
| `r8i-statics-init` | statics + `/catShipStaticsStaticInit` | **0** / 14.7 s | ship statics self-test done | none | ASan 0, no leak. 143 of 145; **108 StaticInits ran** (`staticinit=next` x108, `none` x35; R4-R7: 108) |
| `r8i-meshes` | meshes | **0** / 14.2 s | mesh self-test done | none | ASan 0, no leak. 1,758 listed, 1,729 `.geo/.peo` + 29 `.mad` loaded, 0 with bad objects |
| `r8i-textures` | textures | **0** / 20.7 s | texture self-test done | none | ASan 0, no leak. 12,403 / 12,403 listing lookups, 10,661 registered in 3 batches and loaded, 0 not loaded, 1,742 rows with no file, 9,248 scaled down (R6's numbers) |
| `r8i-oracle` | oracle (debug), 512 ticks | **0** / 12.6 s | oracle self-test done | none | ASan 0, no leak. `randSyncErr=0 univSyncErr=0 blobSyncErr=0`, 772 records, 140,723 B, sha256 `73286ba3ce5b0cc4` |
| `tools/oracle/selftest.sh --label r8i-oracle-rel --bin <release>` | oracle (release), 512 ticks | 0 / 1.6 s each | oracle self-test done | none | runs a and b `cmp` equal (140,723 B, sha256 `0a4849bad44a1eed`) and IDENTICAL; the perturbed run reported at tick 300 / `rng10` / 1 draw ahead; the flipped copy at 400 / `ships_pos`. **`SELFTEST PASS`** |
| `r8i-etg` | etg, 128 ticks | **0** / 13.8 s | etg effect self-test done | none | ASan 0, no leak. 9 of 9 scripts parsed, 197 effects loaded, 0 referenced but not loaded; 618 created, 3,197 deleted while running, 1,093 left, peak 2,515 effects / 3,012 particle blocks (R3-R7's numbers) |
| `r8i-dock` | dock, 5,000 ticks | **0** / 27.0 s | **dock self-test done** | none | ASan 0, no leak. `done fighters=6 outside=6 corvettes=2 carrierleg=2 processorleg=1 RUs=1850 jackPausedTick=1024 result=PASS` (R6's line) |
| `r8i-command` | command, 3,000 ticks | **0** / 23.0 s | **command self-test done** | none | ASan 0, no leak. `done tick=2998 link=252 mcv=1 unlink=285 research=1/1 mimic=1 point=1/1 result=PASS`, 0 FAIL lines |
| `r8i-render` | render, 600 frames | **0** / 19.6 s | **render self-test done** | none | ASan 0, no leak. `frames=583 listed=1986 sorted=1404 ships=582 effects=1404 trails=582; lensflares=583 lensflareElements=9328 foglayers=1166; droids=2500 droidUpdates=583; result=PASS` (`listed` / `sorted` / `effects` 1986 / 1404 / 1404; R7: 1823 / 1241 / 1241, so 163 more listed effects in this tree's Mission01 start, not traced to a port; `droids=2500` as R6) |
| `r8i-sound` | sound | **0** / 20.1 s | **sound self-test done** | none | ASan 0, no leak. `audio driver dummy`; music 43, sect 124, beast 281 blocks decoded; 88 chatter events; 403 audible output blocks (into the dummy device); `result=PASS`; **UBSan in `fquant.c` / `fqcodec.c`: 0; `PATCH`: 0** |
| `r8i-hud` | hud, 600 frames | **0** / 21.3 s | **hud self-test done** | none | ASan 0, no leak. `selected tick=64 band-boxed=19 player ships=154 selected=154`; the waypoint leg `waypoints start tick=420 type=4 waypoints=3 cursor=waypoint`, `draws=181 randoms=606 (expected 606)`; `sensors open tick=480 active=1 hotkeygroup=154`; `done tick=599 boxed=19 selected=154 frames=537 statusbars=6840 infopanels=537 pie=537 result=PASS` |
| `r8i-ui` | ui | **0** / 18.6 s | **ui self-test done** | none | ASan 0, no leak. `selected tick=16 ships=154; popup open tick=24 rows=10; popup drawn frames=16; commands tick=40 run=26 target=resource; game stats begin tick=48 paused=1; done selected=154 rows=10 popupframes=16 commands=26 targetrows=1 statsframes=8 result=PASS` |
| `r8i-fescreens` | fescreens | **0** / 12.3 s | **fescreens self-test done** | none | ASan 0, no leak. `done files=22 loaded=22 screens=195 pushed=195 popped=195 wrong=0 skipped=0 listchanged=0 userregionstandins=84 result=PASS`; `optionstest: done options=34 startdefaults=34 readback=34 result=PASS`; **0 `fescreen` UBSan reports** |
| `r8i-strings` | strings | **0** / 12.3 s | **strings self-test done** | none | ASan 0, no leak. `done ids=1705 mapback=1705 resolved=1681 nostring=24 wrongtext=0 wrongnostring=0 result=PASS` |
| `r8i-keys` | keys | **0** / 11.9 s | **keys self-test done** | none | ASan 0, no leak. `done rows=197 known=149 unknown=48 commands=147 bindings=149 startsave=1 saved=1 reload=1 reset=1 newkey=1 conflicts=1 shares=1 failures=0 result=PASS` |
| `r8i-console` | console | **0** / 17.8 s | **console self-test done** | none | ASan 0, no leak. `done selected=154 frames=4 shown=31 set=24 commands=8 keys=32 kas=6 closed=1 failures=0 result=PASS`; **`kas lines=6 ok=6 tablerows=486 last='KAS Returned 0'`** (the kasf library's extension: the exe's 486-row kas2c table through `cat_consoleKasCommand` and the generated dispatcher) |
| `r8i-build-a`, `-b` | build, 7,000 ticks (`CAT_EXTERNAL_BUILD_DRIVER 1`) | **0** / 28.0 s each | **build self-test done** | none | ASan 0, no leak. `done tick=4205 driver=1 SU=40/100 RUs=25925 result=PASS`; `run.hwo` 1,722,370 B, sha256 **`b7851d51d1c21bfb`** both runs, `cmp` equal; against R7's `r7i2-build-a` (`31fbd86ebc8c2b1a`): `rng0` 40 ahead from tick 0 (the shuffle), `ships_rot` from tick 1 and, every other field, `ships_vel` from tick 4207 (the built destroyer's flight through the x87 helpers after the jobs end at 4205) |
| `r8i-nisparse` | nisparse | **0** / 12.2 s | **nisparse self-test done** | none | ASan 0, no leak. `done nis=14 nisScripts=14 animatics=18 objects=593 events=1488 cataclysmEvents=31 unknownKeywordLines=47 failed=0 result=PASS` |
| `r8i-toggle`, `-toggle-noi` (`-- /noInterp`), `-toggle-non` (`-- /noNISInterp`) | toggle | **0** / 12.0 s, 11.9 s, 12.2 s | toggle self-test done | none | ASan 0, no leak. `done start=1/1 fails=0 result=PASS`, `start=0/0 ... PASS`, `start=1/0 ... PASS`; with a switch the run-only rows hold (`run-only save rows=2 objInterpolate=0 nisInterpolate=0`) |
| `r8i-ai-a`, `-b` | **ai**, 400 ticks | **0** / 20.6 s each | **ai self-test done** | none | ASan 0, no leak; 4,292 file loads. `teams tick=17 ... attackmove 33 ... harvest 33 ... skirmish tick=65 (RUs=22495 SU=1119/1319 buildsqueued=13) ... skirmishharvest 177; done tick=398 fleet=156 ... checks=16/16 result=PASS` (R7's ticks); `run.hwo` 98,768 B, **sha256 `df95f21bdb82abbf` both runs, `cmp` equal**; against the last audit's `r8p4a-ai` (`a88770b54df64d4e`): `IDENTICAL: 401 ticks` (header build identity only); against R7's `c6ff74db1a364512`: `rng0` 40 ahead from tick 0 and the rung's fields (producer 1 and 2's statements) |
| `r8i-ai-soak` (`--ai-ticks 4000`) | ai, **4,000 ticks** (decision 67's soak) | **0** / 57.4 s | ai self-test done | none | **ASan 0, no leak, no fatal**; `aitest: game over tick=1466 player0=dead player1=alive (the test keeps the universe running)` (producer 1: 1101; the shuffle moved the battle), `done tick=3998 ... checks=16/16 result=PASS`; `run.hwo` 984,370 B, sha256 `a2b04b8998bc6818`; against the rung audit's `r8au-ai-soak` (`fd1480fef8611673`): `rng0` 40 ahead from tick 0 (the shuffle). No `ETG.c` assertion, no `AISkirmish.c` overflow, no null member access in `univRemoveAllTargetReferencesFromBullets` (the R7 soak's three faults, decision 67) |
| `r8i-skirmish-lfe2-a`, `-b` | **skirmish**, LiveFireExercise2, 600 ticks | **0** / 20.8 s, 21.3 s | **skirmish self-test done** | none | ASan 0, no leak; 5,236 file loads. `started numPlayers=2 ... missionman=1 ships=89 resources=66; ... resources asteroids=20 dust=30 gas=0 nebulae=16; no-owner ships=10 (player 8); absent players' ships=0; done ticks=600 ships=89 result=PASS`; `run.hwo` 147,967 B, **sha256 `1b648d098a7659c5` both runs, `cmp` equal**; against the last audit's `r8p4a-sk` (`7e135b2daee5d99e`): `IDENTICAL: 601 ticks`; against R7's `979f7d23606207f5`: 10 fields from tick 0, origin `rng0` (the shuffle places the players, the helpers place the ships) |
| `r8i-skirmish-armed8` (`--skirmish-map Armed8`) | skirmish, Armed8 (an 8-player map, 2 players), 600 ticks | **0** / 21.2 s | **skirmish self-test done** | none | ASan 0, no leak; 5,170 file loads. `started ... ships=54 resources=324` (R7: 242; **the meteor storms of the KAS runtime port**), `resources asteroids=324`, `no-owner ships=24 (player 8)`, `done ticks=600 ships=54 result=PASS`; `run.hwo` sha256 `898f68a5fc352ba2`; against `r8p4a-sk8` (`1d6d27adc0a79ca5`): `IDENTICAL: 601 ticks`; against R7's `af269ea4bc7dca8f`: 9 fields from tick 0, origin `rng10` (the storms' 602 draws, then the shuffle and the helpers) |
| `r8i-missiontest-a`, `-b` | **missiontest** (this round's self-test: Mission01 from the NIS-first start through its KAS objectives) | **0** / 72.5 s each | **mission self-test done** | none | ASan 0, no leak; 4,345 file loads. `flow tick=0 level loaded SinglePlayer/Mission01_NIS01/...; nis start nis/n01.nis; tick=1771 nis stopped; level loaded SinglePlayer/Mission01/...; tick=1 kas start mission01; tick=2 nis start nis/nislet01.nis; tick=1713 nis stopped`; the objectives in order: **DestroyBombers 4768 / 4800, ExploreRegion 6864 / 8096, DestroyResourcers 8208 / 8752, DestroyProxAttackers 9760 / 10240, DestroyDestroyer 10816 / 11296, Hyperspace 11328** (created / completed); `done ticks=11424 objectives=6/6 result=PASS`; `run.hwo` 3,246,588 B (11,425 ticks), **sha256 `becb49b579aba435` both runs, `cmp` equal**; against the last audit's `r8p4a-fix-mt-a` (`35d612581c658119`): `IDENTICAL: 11426 ticks` (header build identity only); against the KAS runtime's audited `b03fdee2b85d36a0`: tick 1772 `rng0` 300 draws ahead (producer 4's `UpdateArrivingShip` motion-cue reset, as its audit states) |
| `r8i-tutorial` (`--mode missiontest --tutorial`) | missiontest, the Tutorial1 leg | **0** / 18.6 s | mission self-test done | none | ASan 0, no leak. `tutorial buttons tick=1 next prev quit; Lesson1 tick=2; objectives tick=2 L1Dummy1 L1Dummy2; prompt 1 tick=7 'LESSON 01 - Start of Tutorial'; prompt 2 tick=102 'Welcome to the Homeworld: Cataclysm tutorial.'; objective pointer tick=156; prompt 3 tick=1150 'This tutorial is divided into 12 lessons...'; Lesson2 tick=1152 (after the Next button)`; `done ticks=1152 tutorial kas=1 lesson1=2 objectives=2 buttons=1 prompts=7/102/1150 pointer=1 next=1150 lesson2=1152 result=PASS` (the third prompt's tick follows the audio thread's wall clock: 1150 here, 1152 / 1238 / 1160-1178 in the producer's and the audit's runs; its stream `51a51e0d7018185b` is not a pair) |
| `r8i-loadgame-a`, `-b` (`--mode loadgame --load-ticks 3600`, the user's "01 - Hiigara") | **loadgame** (this round's second self-test) | **0** / 27.1 s each | **save-load self-test done** | none | ASan 0, no leak. `loaded singlePlayer=1 mission=1 tick=1984 ships=139 player0ships=80 objectives=0 kas='mission01' watch=1 level='SavedGames\SinglePlayer\01 - Hiigara' hyperspaceState=0 difficulty=2`; `done ticks=3600 from=1985 to=5585 mission=1 ships=140 objectives=0/1 kasTeams=36 result=PASS`; `run.hwo` 886,495 B, **sha256 `68b0075ad1e20bb7` both runs, `cmp` equal**; against the last audit's `r8p4a-fix-load` (`b9ba66b8a1638bc2`): `IDENTICAL: 3603 ticks (ticks 1984-5586)`. `original/game` untouched (the save is copied into the run's settings dir only) |
| `r8i-motion-noi`, `-nis-noi` | motion 224 / nis n06:26:36, `-- /noInterp` | **0** / 44.3 s and 47.0 s | motion self-test done (`result=PASS`) | none | ASan 0, no leak. Dumps: `motion.dump` 1,431 frames sampled, **sha256 `0838a011db819920`**; `nis.dump` 1,440 frames, **sha256 `d1e6f6e477906b52`**: byte-identical to the rung audit's `r8au-motion-noi` / `r8au-nis-noi`; against the pre-feature dumps (R6 / R7's `b2af0d6decdf6474` / `3f04c837e605ba23`): 34 of the 1,431 motion frame lines differ, **in the frame CRC field only** (the camera pose and every ship's screen position, depth and flag equal); 1,170 of the 1,440 nis lines differ in the NIS objects' screen positions (O5 .. O21). **Re-baselined** (the conditions table) |
| `r8i-motion-on`, `-nis-on` | motion 224 / nis n06:26:36, flags on (not required by the exit test) | **0** / 42.8 s and 49.0 s | motion self-test done | none | ASan 0, no leak. Dumps `7e4f12d35a662197` (1,431 frames) and `2f18ba93741ebdc6` (1,440 frames): R6 / R7's flags-on `d9a8661fcce6f4b3` / `074a70f17e316cd2` moved too (1,260 motion lines: the blend goes through the same helpers); the new flags-on reference |

So every row of the R8 exit test holds, with one guard-rail re-baselined and stated:

| condition (docs/m2-r7-summary.md "M2-R8 integrate (stage-6 part A exit)") | result |
|---|---|
| (a) two 6,000-tick Mission01 streams byte-identical with ASan 0, the perturbed run and the flipped copy caught, also in release; the re-baselined stream's hash and the moved fields stated against R7's `9f7416bb64fec98f`, the moves exactly the rung's physics change and the start-position shuffle and nothing else | **met**: debug `0d2b76d565105a26` (A = B), release `3dc687dbb95feab6` (A = B, IDENTICAL to debug), both `MISSION TEST PASS` (perturb 3000 / `rng10`, flip 4000 / `ships_health`); against R7 (= R6's `49240abcf9f0ed97` in content): **`rng0` 40 draws ahead from tick 0 (the shuffle; `rng_all` with it) and `ships_rot` from tick 1 (the rung); every other field IDENTICAL over 6,001 ticks**. The chain through the round: `9f7416bb64fec98f` -> the rung `245b3d88387ed076` (`ships_rot` only) -> the kasf library `b97180de748702e5` (IDENTICAL content) -> the KAS runtime + the shuffle `19560d16033efd53` (`rng0` +40) -> single player `00ae16e7f73689d9` (IDENTICAL content) -> this build `0d2b76d565105a26` (IDENTICAL content): the stream moved twice, as planned, and the headers' build identities make the other hashes |
| (a) sp reaches the running universe with Mission01 running from the NIS-first start (`--timeout 340`, `univUpdateCounter` at least 1,248, ASan 0) | **met**: 4,496 ticks at the 340 s timeout, the opening NIS then `Mission01/Mission01.mission` in FileLoads, ASan 0, no leak |
| (a) fe `Main_game_screen` | **met**, ASan 0, 0 UBSan reports of any kind |
| (a) statics, meshes, textures, oracle, etg unchanged or better | **met**: 143/145 (108 StaticInits), 1,758/1,758, 10,661 / 0 not loaded, 0 sync errors, 618 effects: R6's numbers |
| (a) dock, command, render, sound, hud, ui, fescreens (0 `fescreen` UBSan), strings (1,705 / 1,705), keys, console, build, nisparse, toggle, ai (two runs byte-identical; plus one 4,000-tick run with no fatal), skirmish (LiveFireExercise2 twice byte-identical, Armed8 once) and this round's missiontest (two runs byte-identical, every Mission01 objective reached in order, the Tutorial1 leg) `result=PASS` with ASan 0 | **met**: all nineteen scripted self-tests `result=PASS` (motion and nis included), rc 0, ASan 0; `fescreen UBSan reports: 0` in all 62 summaries; the ai pair `df95f21bdb82abbf` (16/16) and the soak with no fatal (`game over tick=1466`, `checks=16/16`); build `b7851d51d1c21bfb` twice; skirmish `1b648d098a7659c5` twice and `898f68a5fc352ba2`; missiontest `becb49b579aba435` twice with the six objectives in order and the Tutorial1 leg (prompts, the pointer, Next, Lesson2); also loadgame `68b0075ad1e20bb7` twice |
| (a) the release oracle self-test `SELFTEST PASS` | **met** |
| (a) every quitting run returns rc 0 | **met**: 60 of 60 quitting runs (the 9 debug and release mission runs, the 11 replays, statics x2, meshes, textures, oracle x4, etg, dock, command, render, sound, hud, ui, fescreens, strings, keys, console, build x2, nisparse, toggle x3, ai x3, skirmish x3, missiontest x3, loadgame x2, the 4 motion / nis runs); sp and fe stop at their timeouts by design (rc 124) |
| (a) `tools/m2_layout32.sh` OK; clean builds 0 warnings | **met** (253/253 debug and release, 0 warnings; Ship 0x63C and the round's pins) |
| (a) the motion and nis dumps under `/noInterp` still equal the pre-feature hashes `b2af0d6d…` / `3f04c837…` | **not met as written, re-baselined**: the dumps are `0838a011db819920` / `d1e6f6e477906b52`, the rung audit's exactly; the pre-feature dumps differ only in the frame CRC of 34 of 1,431 motion frames (every camera pose and screen position equal) and in the NIS objects' screen positions, because the rung put the exe's x87 shape into `matMultiplyMatByMat` / `matMultiplyMatByVec` / `vecNormalize` and the other helpers every frame goes through (docs/m2-r8-oracle-rung-physics.md 6, its audit). The guard-rail's meaning (flags off = the feature changes nothing) is kept by the stream check (the three 6,000-tick streams with the flags on, `/noInterp` and `/noNISInterp` are all `0d2b76d565105a26`) and by this build's dumps from now on; the flags-on references move to `7e4f12d35a662197` / `2f18ba93741ebdc6`. Decision 46's guard-rails themselves were not touched |
| (b) both replay pairs byte-identical from the clean build; the 10-17 replay IDENTICAL beyond tick 177 (or the next divergence localised to its owning function with its fix proposed), the 10-18 one beyond 118; matching docs/m2-r8-oracle-rung-physics.md and its audit | **met with the table switch, owned without it**: the four pairs byte-identical (`6b3e3c15a42de5af`, `d88fd3c4a6e2de54`, `9c51469ece54c88f`, `98985af637e37bf3`), each IDENTICAL to the audit's run over every tick and packet; with `/oracleTableFsqrt` the 10-17 replay is IDENTICAL through 246 (first divergence 247 `ships_vel`, ship 40) and the 10-18 one through 154 (155 `ships_vel`, ship 28), both localised by producer 1 (the coordsys after a hard turn / roll on the guard and avoidance paths) and owned by M2-R9 physics; with the round's default `sqrtf` the first divergences stay at 177 (ship 35: the table square root itself, decision 23 / 56) and 118; every per-field tick equals the report's tables and the audit's ("Oracle mode A") |
| (c) no `CAT-TODO` / `CAT-PROVISIONAL` in `src/` names an M2-R7 or M2-R8 owner; `CAT_HW1_MISSION_SCRIPTS` gone; every "stage 6" tag left is part B's with a reason | **met**: `grep -rn -E 'CAT-(TODO\|PROVISIONAL)\([^)]*(M2-R[78]\|stage 4\|stage 6)' src/` is empty (**no "stage 6" tag at all**); `grep -rn CAT_HW1_MISSION_SCRIPTS src/` 0; 38 lines carry `CAT-TODO` (R7: 82), 29 of them owner tags: **stage 5 26** (plain 9, multiplayer options 7, LAN 5, recorder 4, multiplayer 1: R7's list unchanged), **M2-R9 part B 2** (SinglePlayer.c: the generated code of Mission02 .. 17, "until then those missions start no KAS script"; LevelLoad.c: the "Mine" 0x004a3f20 / "Slipgate" `cat_scriptSetSlipgateCB` 0x004a1020 mission-sphere readers, with the maps that need them), **M2-R9 physics 1** (CommandLayer.c: `hyperspaceCost`'s double return); 5 `CAT-PROVISIONAL` (R7: 9; the M2-R2 appends and four medium-confidence readings: the stage-6 switch, the kas2c / NIS-first notes and the identity start-position table are gone) |

**UBSan kinds left** (the union over the 62 runs; all non-fatal; none is `fescreen`, `PATCH`, `fquant.c`, `AIShip.c`
or `Tutor.c`): the ETG opcode kinds of GoK's packed effect bytecode in every Mission01, skirmish, mission-test and replay
run (41 runs: `etgdecision`, `etgfunctioncall`, `etgbranch`, `etgconditional`, `etgvariablecopy`) and `struct particle`
(`Particle.c` 521-3505, `ETG.c` 7134-7161; etg, render, sp, the mission tests and the replays); `BTG.c:388` / `:391` (2
misaligned `short` loads) and `BTG.c:864` (1 misaligned `udword` load) in every Mission01 and skirmish run (40);
`stb_image_write.h:1263` (1, sp only); `Subtitle.c:1144` `index out of bounds for subtheme [16]` (5 runs: the nis mode's
n06 as before, and now sp and the two mission-test runs, whose NIS-first start plays n01 and its nislet: the same subtitle
theme index kind; a small stage-3 subtitle item, "What is left" 10). **Gone since R7**: `Select.c:1166` (the 10-17
replay's far-off ship: not reported by any replay of this step) and the kas-runtime audit's `Tutor.c:1958` shift (fixed by
producer 4: an unsigned shift as the exe's `shl`).

Harness facts for the next rounds, in addition to R3-R7's:
- **The `missiontest` mode** (`--mode missiontest [--mission-test-ticks N] [--tutorial]`, default limit 20,000): a new
  single-player game as sp starts it, on the virtual clock, with the exe's NIS-first start and Mission01's generated kas2c
  script, no sound and no speech; the test plays the player's part with scripted events (the destroy objectives through
  the mission language's `ShipsDamage`, the recon through the launch and move orders, the hyperspace button); PASS at tick
  11,424 with 6/6, about 65-73 s under ASan; its stream (`becb49b579aba435` in this build) is byte-identical run to run and
  is the mission's guard-rail from now on. `--tutorial` runs the Tutorial1 leg (about 18 s); its stream is **not** a pair
  (speech stays on, on SDL's dummy driver, whose audio thread runs on the wall clock: the third prompt's tick varies).
- **The `loadgame` mode** (`--mode loadgame [--save NAME] [--load-ticks N]`, default "01 - Hiigara" / 400): the user's
  single-player save is copied from `original/game/SavedGames/SinglePlayer` into the run's settings dir only (never into
  the data dir, never written in `original/game`), loaded as the front end's Load Game does through the mode-A converter,
  and run N ticks; byte-identical run to run (`68b0075ad1e20bb7` at 3,600 ticks). The install holds one such save.
- **The `/oracleShipDump` switch** (`-- /oracleShipDump <ships>[@<from>-<to>]`, any mode that writes `run.hwo`) and
  `tools/oracle/hwo_invert.py` are the permanent per-ship instrumentation; a run with the dump is byte-identical to one
  without it.
- **The recordings** (decision 60 updated): the install's `SavedGames/RecordedGames/oracle01` is now the 10-19 recording
  (133 packets, 266 ticks, 47 ships, the unfixed DLL: `original/oracle/oracle01-rec-20261019-143530/`); the 10-17 one
  (the rung's reference) and the 10-18 one are replayed from their archives with `--recording-src DIR --recording-as
  oracle01-r6` / `oracle01-r18`. Judge each against its own session's `oracle01-rec.hwo`; for the 10-18 and 10-19 sessions
  exclude `real` too.
- **The console self-test** checks the exe's 486-row KAS table (`kas=6 tablerows=486`); the skirmish Armed8 map now has 324
  resources (the meteor storms); the ai soak reaches the game over at tick 1,466 and keeps running (HWI-TEST).
- **The dumps' new references**: `/noInterp` `0838a011db819920` / `d1e6f6e477906b52`, flags on `7e4f12d35a662197` /
  `2f18ba93741ebdc6` (this build). A clamp / droid blend change is still invisible in those two scenes.

## The four ports

| # | item | what landed | DONE WHEN | audit | commit |
|---|---|---|---|---|---|
| 1 | the oracle's next rung, then the physics / combat leftovers ([report](m2-r8-oracle-rung-physics.md), [audit](audits/m2-r8-oracle-rung-physics.md)) | **The permanent dump** `/oracleShipDump <ships>[@from-to]` (`S` / `A` / `F` / `V` / `R` / `C` lines next to `run.hwo`, read-only, in the harness header) and **`tools/oracle/hwo_invert.py`** (recovers a GOG tick hash's ship or resource words by FNV-1a's byte-wise inverse). **The exe's x87 shape** (53-bit intermediates, one rounding at each `fstp dword`, the compiler's order of the sums, read in the disassembly) of the whole flight path: `Vector.c` (`vecNormalize` 0x00550770, `vecCopyAndNormalize`, `vecNormalizeToLength`, `vecCapVector` 0x00550950, `vecCapMinVector`, `vecCapMinMaxVector`, `vecHomogenize`, the four plane intersections, `vecVectorsBlend`, `cat_vecDistance`, `cat_vecPlaneNormalFrom3Points`: the double magnitude into the square root, the double reciprocal), `Matrix.c` (`matMultiplyMatByMat` 0x004ad1a0, `matMultiplyMatByVec`, `matMultiplyVecByMat`, `matCreateCoordSysFromHeading`), `AITrack.c` (`aitrackHeadingFunc` 0x00418de0 + `catAitrackTurn`, `aitrackHeadingWithBankPitchFunc` 0x00419200 with `fptan` as `tanl` + `fma`, `aitrackVelocityVectorGuidance` 0x00419460 + `catAitrackThrust`), `Physics.c` (`physApplyForceToObj` 0x004e43f0, `physUpdateObjPosVelShip` 0x004e4950's rotation), `AIShip.c` (`GetCollSizeInDirection`, `aishipGetTrajectoryWithVelPrediction`, the avoidance of `aishipFlyToPointAvoidingObjsFunc`), `GenericInterceptorPassiveAttack`, `Formation.c` (`formationWingmanTrackLeader`, `catParadeSlotPosition`), `CommandLayer.c` (`CalculateMoveToPoints`, `hyperspaceCost`), `Collision.c` (`RangeToTarget`; `collCheckShipShipColl` 0x0043ade0's clamped-host test and the squared-distance test before the root), `UnivUpdate.c` (`univFindNearestResource`; **the killer-asteroid wrap** `catResourceWrap` from the R5 tag, `Asteroid.catWrapExtent` +0x1A8; `univDeleteDeadShip`'s tenth ETG parameter; the bullet / missile owner compares); `catFsqrtD` (the table on the double with the switch, `sqrtf` by default). **The rung**: tick 177 fixed (ship 35's velocity exactly GOG's `(c20420c2, c110b4d6, 41d3f9da)` with the table), 179 / 180 / 183 / 243 fixed on the way (the formation, the parade slots, the collision shape, the asteroid wrap), the next divergences localised with the inverter: 10-17 **247 `ships_vel` ship 40** (a bRecon guarding the bMothership: the coordsys after its turn at 245-246 or the computed UP thrust), 10-18 **155 `ships_vel` ship 28** (an sWorker's v.z -1 ulp after a hard turn). **The soak faults** (decision 67): `univDeleteDeadShip`'s big-death explosion passes the exe's ten parameters (`CAT_ETG_CREATE_PARAMETERS` 10, the bounded parameter write `CAT-FIX`), the bullet / missile owner compared as a pointer, `cat_aisTeamAttack`'s one-past read on the contiguous target tables (`catAisTargetTeam`, `CAT-FIX`), the ai self-test keeps the universe running after the game over (HWI-TEST); the 4,000-tick soak PASSes with ASan 0. The Mission01 stream moved in `ships_rot` only (`245b3d88387ed076`); the ai, skirmish and motion / nis dumps moved with the helpers. 25 IDENTICAL found modified, 50 reviews | met with the switch, owned without it: both pairs byte-identical, ASan 0; with `/oracleTableFsqrt` the 10-17 replay IDENTICAL beyond 177 (through 246) and the 10-18 beyond 118 (through 154), the next divergences localised to the rotation of a hard turn and owned; with `sqrtf` (the round's setting) 177 / 118 stay, the difference being the square root itself (decision 23 / 56); the stream hash before (`a97da06b142b590a` = `9f7416bb64fec98f`'s content) and after stated; the soak ASan 0 with no fatal | PASS-WITH-FIXES, **0.970** (64/66). Fixes: a report overstatement ("every later field moved out"), `hyperspaceCost`'s float return re-targeted (`CAT-TODO(M2-R9 physics)`: the exe returns the unrounded double to `_ftol` callers) | `15df4d6` |
| 3 (ran second) | the kasf library ([report](m2-r8-kasf-library.md), [audit](audits/m2-r8-kasf-library.md)) | **The exe's own kas2c table** `cat_kasFunctionTable` 0x00755048 (486 rows + the `"*"` sentinel) and its dispatcher `cat_kasCallFunctionByName` 0x0045b9a0, **generated by `tools/m2_kasf_table_gen.py`** (the PE's `.data` rows, the exe's hash `cat_kasHashString` 0x00491860 of every KAS name, the dispatcher's compare tree walked for every hash: the exe address of every kasf with no guessing) into `KASFuncTable.c/.h` and `KASFuncDispatch.c` (a `switch` on the hash, 486 cases, the console's argument conversions); the kasf take the exe's signatures (names, timers, variables, messages and labels are `udword` hashes); **the 184 Cataclysm-only kasf** at the exe's bodies (the 26 `kasfShipsSelect*` on `cat_kasShipsSelectByPredicate` 0x00494f90, `kasfShipsSetInvincible`, `kasfShipsSort` (x87), the RKO / Naggarok / beast / swarm / slipgate kasf, the camera, NIS, speech, subtitle and effect calls, `kasfStartLevelByHyperspacingShips`); the 9 ICF-folded stubs; **30 KASFunc.c IDENTICAL bodies found modified** (the timers' / variables' scope prologue `kasfScopeHash` with `G_` globals, `kasfTimerStop`, `kasfMissionCompleted`'s animatic, `SelectionSwitchPlayerOwner`'s `keepSU`, `kasfRenderedShips`' LOD mask, ...); every KASFunc.c / Objectives.c row reviewed. **KAS names are hashes everywhere**: KAS.c's labels (7 found modified), Timer.c's (scope, name) timers (`timTimerLeft` rounds to nearest), AITeam.c's message queue of hashes and the team's KAS identity (+0x58 / +0x5C / +0x60 / +0x64 / +0x68; `AITeam` 0x280 pinned; the two R7 tags closed), LevelLoad.c / NIS.c / SinglePlayer.c lookups by hash; **the KAS variable store's callers** (`G_StopShipsFiringOnMission15` in `gunShootGunsAtTarget`, `G_ResourcersRetired`, `G_CrystalsHarvested`, `G_NaggarokEMP`, Mission 7's prison ship, `RKOHouseKeep`); `catDamageFactor` Ship +0x3E0; **`cat_consoleKasCommand`** 0x0045f270 (the console self-test runs six KAS lines through the table and the dispatcher: `kas=6 tablerows=486`); Objectives.c to the info overlay (`cat_ioObjective*`, `cat_objectiveCountActive`); **the task bar removed** (`tbStartup` / `tbShutdown` and their callers, decision 41); the letterbox (`kasfWideScreenIn` / `Out`, `cat_kasWideScreenActive` / `InFinish`, `nisScissorFadeIn` 0x00a4cb38 for the time keys); the kasf side of the tutorial buttons (stand-ins handed to producer 4); **the 32 UI sound-id sites** read in the exe (12 new `CAT_UI_*` ids; decision 65's sound item closed); HW1's compiled missions (`src/Missions`'s `.kas` output) out of the build; `CAT_SHIPFLAG_Invincible` (was `Orphaned`); 40 IDENTICAL found modified in all, 229 reviews (+3 audit), 21 tags closed | met: every kasf Mission01 / Tutorial1 calls exists at the exe's (the dispatcher links all 486); the console's KAS command lists the exe's 486-entry table; the Mission01 stream unchanged (`b97180de748702e5` = `245b3d88387ed076`'s content) | PASS-WITH-FIXES, **0.92** (93/101). Fixes: `kasfShipsSort`'s x87 shape (five differences on the stack, `(z*z + y*y) + x*x`), the letterbox constant 1/30 (`0x3d088889`), 3 missing reviews, an untagged lockout hand-off, a misplaced comment, 2 report statements | `7d225de` |
| 2 (ran third) | the KAS runtime, the kas2c dialect, Mission01 + Tutorial1, the NIS-first start, level load ([report](m2-r8-kas-runtime-mission01.md), [audit](audits/m2-r8-kas-runtime-mission01.md)) | **KAS.c**: `cat_kasShipRemovedFromSelections` 0x004911e0 at its five exe call sites (the three UnivUpdate.c tags closed), `kasLabelledEntitiesDestroy`'s variable store, `kasSave` / `kasLoad` (the difficulty slot, the six `rmCanBuildShip` words, `G_RecentlyLoaded`), the function converters by exe address minus the Watch's (the audit: across every generated script's list, so a Tutorial1 save works), `WatchFunctionToMissionEnum` / `MissionEnumToWatchFunction`; HS.c's static-gate tag answered. **`tools/m2_kas2c_gen.py`**: the generator from the export's decompile, the exe and `objdump` (the Cataclysm dialect of docs/mission-code-map.md 4: the scope prologue, every kasf call rebuilt from the table's parameter list, hashes for every name, the exe's strings and localised tables, the ICF-shared bodies to `KasShared.c`), **every body verified against the machine code** (149 bodies: the multiset of callees, the 32-bit constants, 1,790 all-constant calls' argument tuples; 0 hand edits), writing `Mission01.c` (84 functions + 14 `LSTRING` tables), `Tutorial1.c` (59), `KasShared.c` (6), `CatMissions.h` (the per-script lists with each function's exe address). **LevelLoad.c**: `scriptSetShipsToBeNeeded` 0x004a2f30's deltas (no race swap, the need words), `levelPreInit` 0x004a3230, `llAvailableColorScheme`'s seven carriers, `scriptSetResourcesCB`'s x87 count, **the meteor storm** `cat_scriptSetMeteorStormCB` 0x004a0d10 (`ResourceVolume` at the exe's 0xB0 with the drift terms, `AddResourceToVolumeAtPosition` 0x0049fcf0: the drift, the wrap box of a new game's killer asteroid, which closes the rung's open question), `nebLoad_Nebula`'s save chunks through SaveCat32, **the mission-number mapping** (`GetMissionsDirAndFile` 0x00517200's staging levels `MissionNN_NISmm`, the exe's five tables, `MISSION_17`). **The NIS-first start** (`singlePlayerStartGame` 0x0051bdb0, `singlePlayerStartNis` 0x0051b670, `singlePlayerNisStoppedCB` 0x0051b430, `singlePlayerLoadNewLevel` 0x0051ac80, `singlePlayerGameUpdate`'s nislets and **`HYPERSPACE_LOADDONE` -> `kasMissionStart("missionNN")`**, `HYPERSPACE_CATSHIPSHYPERSPACINGIN` 9 / `CATFAILEDRETURN` 10), `tutInitTutorial` starts the generated Tutorial1, `gameStart`'s staging preload and **the start-position shuffle** (the 40 stream-0 draws; decisions 40 / 68 closed), **`CAT_HW1_MISSION_SCRIPTS` retired** (the switch, its branches, HW1's NIS tables); the HWI-ORACLE direct start kept for `/oracleMission` and the self-tests on it; `nisSeek`'s `currentPos[6]` read (`CAT-FIX`). **`/catMissionTest` + mode `missiontest`** and `/catMissionTestTutorial`. 3 IDENTICAL found modified, 46 reviews (+2 audit), 10 tags closed, 5 `CAT-PROVISIONAL` removed | met: Mission01 starts NIS-first and its KAS objectives advance headlessly (6/6 at tick 12,880 in that build, byte-identical `b03fdee2b85d36a0`); the mission stream re-baselined once by the shuffle (`rng0` 40 draws ahead from tick 0, nothing else: `19560d16033efd53`) | PASS-WITH-FIXES, **0.88** (53/60). Fixes: the save converters for Tutorial1's functions, the meteor storm's fatal texts, 2 missing reviews (`nisSeek`, `tutInitTutorial`), the untagged "Mine" / "Slipgate" sphere readers (`CAT-TODO(M2-R9 part B)`), a report count, a comment | `3903f02` |
| 4 | single player and the tutorial ([report](m2-r8-singleplayer-tutorial.md), [audit](audits/m2-r8-singleplayer-tutorial.md)) | **The hyperspace flow** at the exe's: `spAbortHyperspaceCB` 0x00517320, `spGoNowHyperspaceCB` 0x005173a0, **`cat_spEmergencyHyperspaceCB`** 0x005174b0 (the third roll-call button; `CAT_SHIPFLAG_LeftBehind`), `CalculateRollCall` 0x005176e0, `TellHyperspacingShipsToReady` 0x00517770, `CalculateHyperspaceTimeForShip` 0x005178a0 (x87), `AddShipToHyperspace` 0x00517a10, `UpdateHyperspacingShip` 0x00517af0 (no HW1 hyperspace sounds), `OrientHyperspacingShips` 0x00517f30, `UpdateArrivingShip` 0x00518170 (the arrival angle in x87, the ship hidden until it decelerates in, the motion-cue reset `cat_mcResetRequest`), `cat_spPrepareShipHyperspaceOut` 0x00518880, `UpdateMidLevelHyperspacingShips` 0x00519700, `cat_spLaunchDockedShipsOnArrival` 0x005191a0, `cat_spForgetDamageEffects` 0x00519490; 16 HW1 helpers the exe inlines deleted. **The mission flow**: no mission sequence (`currentMission` 1 .. 17), `singlePlayerInit` 0x00518c50 reads `Warp.script` through the exe's table and **`cat_spWarpToLevel`** 0x0051b9c0, `spMainScreen` 0x00518d80's `closeSensors`, the lockouts, **`spHyperspaceButtonPushed`** 0x00518f50 (the letterbox jump of the `catSpMissionNextLevel` missions, Go Now / Emergency Hyperspace disabled under fire), **the rest of `singlePlayerGameUpdate`** 0x005197d0 (the letterbox jump, the white-out and the animatic `(cur, cur + 1)`, mission 17's credits, the ether's ships forget their damage effects, **the at-once arrival** of the 0x00894470 missions, the failed jump's case 10), `singlePlayerGameCleanup` / `PostInit` / `Close` / `SetMissionAttributes` / `SetOnMissionCompleteInfo` / `MissionFailedCB`, **the fades in `mrRegionDraw`** 0x00569260 (the black level fade, Mission01's white flash), `hsStaticGateRender`'s word, the hyperspace objective's row. **The difficulty**: `cat_spGetDifficultyModifiers` 0x00516f90 (health 6 / 3 / 1 / 0.85 / 0.7, the build-time and cost divisors), `cat_spGameDifficulty` 0x0051c140, `kasLoad`'s rescale, **the KAS skill level is the difficulty word** 0x008941a0 (HW1's separate `CurrentMissionSkillLevel`, which doubled every single-player ship's damage, deleted). **A cataclysm.exe single-player save's mission tail** through SaveCat32 (8 new roots: `KasSelection`, the labelled path / vector / volume, `Objective`, `FleetIntelligence`, `hsStaticGate`, `ping`; 119 layouts) and **the `loadgame` self-test** ("01 - Hiigara" loads: 682 chunks, 167 ships, the KAS watch restored, 36 KAS teams running). **Tutor.c**: the 54 flags in the exe's layout (the audit: `"Attack"` restored, 54 + 1 pinned), the pointers (`cat_tutPointerListReset` 0x0052f890, `cat_tutObjPointerXYSet` 0x00531290, `cat_tutSetPointerTargetObj` 0x00530060, `tutDrawTextPointers` 0x005312b0's colours and objective pointer), **Cataclysm's buttons** (next lesson, previous, quit, next text, back: 0x00532010 .. 0x00532d80, `catTutButtonCreate` with the exe's 0x1c extra bytes after the audit), `cat_tutButtonsHideForScreen` 0x005341f0 / `RestoreAfterScreen` 0x00534300 in the three managers, save / load with the new buttons' flags, `tutInitialize`'s 35 images, `tutGameMessage` (the tutorial only); **the Tutorial1 leg** of the mission self-test (three prompts, the objective pointer, the Next button clicked through its region's callback, Lesson2); the uncalled HW1 sensors helpers deleted; 12 IDENTICAL found modified, 98 reviews, 25 stage-6 tags closed, 0 added | met: `grep -rn "CAT-TODO(stage 6" src/` 0; sp from the NIS-first start reaches the running universe with Mission01 running (4,832 at 360 s; the audit 4,496 at 340 s), ASan 0; the Mission01 stream unchanged (`00ae16e7f73689d9` = `19560d16033efd53`'s content); the mission self-test's stream moved as stated (tick 1772, `rng0` +300: the exe's motion-cue reset per arriving ship) and still PASSes 6/6 byte-identical | PASS-WITH-FIXES, **0.90** (55/61). Fixes: the tutorial flag-name table (the lost `"Attack"`: every flag from `Harvest` on set the bit below its own), the tutorial buttons' region size (a latent overrun), 2 report counts / claims, the RNG stream's name (`rng0`, not the game stream), a comment | `3e55019` |

**Round size** (`git diff --stat 145352a..3e55019`, the round base to the last audit's checkpoint): 113 tracked files
changed, +25,810 / −5,417 lines (the four ports, the two generators and their output, the new self-tests, the four
reports and audits, the workflow script, the map files); `src/` alone: 94 files, +19,221 / −4,658, with **937 added lines
carrying `CAT`**; 9 new `src/` files (`Game/KASFuncTable.c/.h`, `Game/KASFuncDispatch.c`, `Game/MissionSelfTest.c/.h`,
`Missions/CatMissions.h`, `Missions/KasShared.c`, `Missions/SinglePlayer/Mission01.c`, `Missions/Tutorials/Tutorial1.c`:
10,129 generated or test lines in all with the two generators), 0 deleted (HW1's `.kas` scripts and their kas2c meson
files stay in the tree, unbuilt). The tree now carries 4,394 `CAT:` lines (R7: 3,824), 38 lines carrying `CAT-TODO` (82;
29 owner tags: stage 5 26, M2-R9 part B 2, M2-R9 physics 1), 5 `CAT-PROVISIONAL` (9), 13 `CAT-DORMANT` (15), **163
`CAT-FIX`** (137), 41 `HWI-ORACLE` (22: the dump and the direct start), 441 `HWI-FEATURE(motion-interp)`, 94 `HWI-TEST`
(67: the mission, tutorial and load-game tests), 21 `HWI-FIX`, 4 `HWI-EXPERIMENT` (the fsqrt switch). Tools: new
`tools/m2_kasf_table_gen.py`, `tools/m2_kas2c_gen.py`, `tools/oracle/hwo_invert.py`; `tools/m2_harness.sh` (modes
`missiontest` and `loadgame`, `--tutorial`, `--save`, `--load-ticks`, `--mission-test-ticks`, the dump in the replay
mode's header: 28 modes), `tools/m2_layout32.sh` (AITeam, Tutor.h, the volumes and nebula records), `tools/m2_save32_gen.py`
(the eight roots, the nebula records). `runs.txt` logs 359 harness runs this round (lines 1390-1748): the producers' and
auditors' 297 and this step's 62.

## Delta map after the round

The ports recorded their comparisons as reviews in `ghidra/delta_map_review.json` and merged them with `tools/delta_map.py
merge` (decision 55; the audits appended their own rows: 0, 3, 2 and 0). Notes tagged `[review: M2-R8 <port>]`:
oracle-rung-physics 50 (2 new), kasf-library 229 (+3 `kasf-library audit`), kas-runtime-mission01 46 (36 new; +2
`kas-runtime-mission01 audit`), singleplayer-tutorial 98 (66 new). This step added no review of its own and ran no merge.

| class | M2-R7 end | M2-R8 end | M2-R8 bytes |
|---|---:|---:|---:|
| IDENTICAL | 2,853 | **2,773** | 500,313 |
| MODIFIED | 1,483 | **1,564** | 722,679 |
| REWRITTEN | 84 | **83** | 15,855 |
| hand-reviewed | 3,036 | **3,340** | |

Per stage (`python3 -I tools/m2_stage_counts.py --md` on the map after the round): HW1-matched IDENTICAL / MODIFIED /
REWRITTEN functions (MODIFIED + REWRITTEN bytes), then cat-new by the tool's rules, with the number of them defined in the
linked `hwi-engine` (`nm --defined-only`, this step).

| stage | M2-R7 end | M2-R8 end | cat-new (defined in `src/`) |
|---|---|---|---|
| 1 | 201 / 114 / 0 (57,114 B) | 201 / 114 / 0 (57,114 B) | 76 / 27,383 B (70) |
| 2 | 757 / 615 / 30 (255,536 + 4,444 B) | **736 / 636 / 30 (258,578 + 4,444 B)** (the rung's vector / matrix / formation / command / collision helpers) | 808 / 180,868 B (761) |
| 3 | 847 / 337 / 14 (199,754 + 8,558 B) | 847 / 337 / 14 (199,754 + 8,558 B) | 450 / 139,196 B (429) |
| 4 | 299 / 178 / 2 (77,515 + 169 B) | **295 / 182 / 2 (79,159 + 169 B)** (the rung's AITrack / AIShip bodies) | 23 / 14,404 B (23) |
| 5 | 406 / 114 / 12 (74,848 + 1,179 B) | **405 / 115 / 12 (75,094 + 1,179 B)** (`gcProcessGameChatPacket`'s sound id) | 67 / 17,483 B (18) |
| **6** | **343 / 125 / 26 (40,897 + 1,565 B)** | **289 / 180 / 25 (52,980 + 1,505 B)** | **2,365 / 656,918 B (2,085 kas2c mission functions, 149 of them generated: Mission01 84 + Tutorial1 59 + 6 shared; 276 of the 280 others defined)** |

What moved: **80 functions the map called IDENTICAL were found modified** by the ports and their audits (the rung 25:
21 in stage 2 and 4 in stage 4; the kasf library 40: KASFunc.c 30, KAS.c 7, Timer.c 1, Objectives.c 1, GameChat.c 1; the
KAS runtime 3: LevelLoad.c; single player 12: SinglePlayer.c 3, Tutor.c 9) and `kasfSaveLevel` went REWRITTEN ->
MODIFIED. **Stage 6 part A reads 0 unread**: every MODIFIED / REWRITTEN row of KAS.c (20 + 3), KASFunc.c (66 + 20),
Timer.c (10), LevelLoad.c (17), Objectives.c (5 + 1), SinglePlayer.c (35 + 1) and Tutor.c (27) carries a review, and so
does every IDENTICAL row of those seven files (R7: 104 unread, 27,781 B; 195 IDENTICAL never reviewed). Of the 280
Cataclysm-only non-mission functions the tool assigns to stage 6, 276 are defined (R7: 45): the 184 kasf, the KAS runtime
helpers, the single-player and tutorial bodies; the four not defined are `cat_scriptSetSlipgateCB` 0x004a1020 (the
`CAT-TODO(M2-R9 part B)` sphere reader) and `cat_transGeneralPerspectiveTransform` / `_C` /
`cat_transIsSimplePerspective`, which the address-neighbour rule puts in stage 6 but belong to the exe's Transformer
(decision 28: GoK's GL backend stays). Across the whole map 271 MODIFIED / REWRITTEN rows still carry no review flag, none
of them in stage 6 part A: `TitanInterface.cpp` 92 (stubbed, not applicable), `ETG.c` 48 and `Particle.c` 43 (M2-R3's etg-particles
scope, rows the map never flagged reviewed), `ObjTypes.c` 22, the stage-5 files 24, and a few others ("What is left" 9).

## Oracle mode A

**The comparison results (the rung).** Both reference recordings replayed in hwi-engine from the clean build (each pair
byte-identical, ASan 0, IDENTICAL to the rung audit's runs over every tick and packet) and compared with the GOG game's
checksum stream of the same session, with the docs/m2-r6-oracle-mode-a.md arguments (`<O>` = `original/oracle`, `<R>` =
`original/scratch-m2/m2r2-harness/runs`; the field list `rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,
missiles,bullets,derelicts,resources,univcheck,dt,total,real`; the exclusions: `players` at tick 93 / 272 and `rng6` at
161, the stock game's own record / playback differences, decision 47; `ships_rot`, the DLL offset defect, decision 48;
`rng_all` and `fpcw`; for the 10-18 and 10-19 sessions also `real`, the one-step `vqpc` clock alignment from tick 2,
decision 60):

```
python3 -I tools/oracle/hwo_compare.py <O>/oracle01-rec-20261017-140428/oracle01-rec.hwo <R>/r8i-r17-on-a/run.hwo \
    --from-tick 1 --to-tick 9600 \
    --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real \
    --json <R>/r8i-r17-on-a/compare.json
  FIRST DIVERGENT TICK 161 (last equal tick 160); 1 field(s) differ, likely origin: rng6   (CAND is 1 draw(s) behind REF: excluded)
  FIRST DIVERGENT PACKET 46 (REF tick 93, CAND tick 93): blobcheck 1785610478 vs 1785610479   (stock, excluded)
... the same without rng6 (--json <R>/r8i-r17-on-a/compare-sim.json):
  FIRST DIVERGENT TICK 247 (last equal tick 246); 1 field(s) differ, likely origin: ships_vel
    ships_vel     REF  ships_vel=83380d15912e50a0   CAND ships_vel=f959f314d5ba4d97
python3 -I tools/oracle/hwo_compare.py <O>/oracle01-rec-20261018-102253/oracle01-rec.hwo <R>/r8i-r18-on-a/run.hwo \
    --from-tick 1 --to-tick 3526 --fields <the same list> --json <R>/r8i-r18-on-a/compare.json
  FIRST DIVERGENT TICK 2 (last equal tick 1); 1 field(s) differ, likely origin: real   (615605.125 vs 615605.188: excluded)
... without rng6 and real (--json <R>/r8i-r18-on-a/compare-sim.json):
  FIRST DIVERGENT TICK 155 (last equal tick 154); 1 field(s) differ, likely origin: ships_vel
    ships_vel     REF  ships_vel=be9dd23a2cdceb2a   CAND ships_vel=cfa0591c536ab701
  FIRST DIVERGENT PACKET 108 (REF tick 217, CAND tick 217): univcheck 1234686647 vs 1234686645
... and each run again with every field alone (--fields <one>) for the table below; the r8i-r17-off-a / r8i-r18-off-a runs the same way
```

| recording, square root | run | IDENTICAL through | first divergence | per field (first divergent tick; none = equal over the range) |
|---|---|---|---|---|
| **10-17** (the rung's reference, 4,800 packets), **the table `fsqrt`** (`/oracleTableFsqrt`) | `r8i-r17-on-a` = `-b`, `d88fd3c4a6e2de54` | **tick 246** | **247 `ships_vel`, ship 40** | `ships_vel` 247, `ships_pos` 259, `univcheck` 315, `ships_misc` 1593, `resources` 1944, `bullets` 2005, `rng10` 2043, `ships_health` 2043, `ships_n` 5559; `missiles`, `derelicts`, `dt`, `total`, `real` none in 1..9600; (`rng6` 161, `players` 93, `ships_rot` 1: excluded) |
| 10-17, `sqrtf` (the round's default) | `r8i-r17-off-a` = `-b`, `6b3e3c15a42de5af` | tick 176 | 177 `ships_vel`, ship 35 (R7's tick) | `ships_vel` 177, `ships_pos` 180, `univcheck` 190, `rng10` 1408, `resources` 1408, `ships_misc` 1593, `bullets` 1917, `ships_health` 2043, `ships_n` 5559 (R7: 177, 180, 180, 1408, 243, 871, 1917, 1934, 2048) |
| **10-18** (1,763 packets), **the table** | `r8i-r18-on-a` = `-b`, `98985af637e37bf3` | **tick 154** (`real` excluded) | **155 `ships_vel`, ship 28** | `ships_vel` 155, `ships_pos` 179, `univcheck` 216, `bullets` 1721, `rng10` 1735, `ships_health` 1741, `resources` 1752, `ships_misc` 1753, `ships_n` 1753; `missiles`, `derelicts`, `dt`, `total` none in 1..3526; (`real` 2, `rng6` 161, `players` 272, `ships_rot` 1: excluded); first divergent packet 108 (tick 217) |
| 10-18, `sqrtf` | `r8i-r18-off-a` = `-b`, `9c51469ece54c88f` | tick 117 | 118 `ships_pos` + `ships_vel` (R7's tick) | `ships_pos` 118, `ships_vel` 118, `univcheck` 137, `rng10` 944, `resources` 944, `bullets` 1721, `ships_health` 1741, `ships_misc` 1753, `ships_n` 1753; first divergent packet 68 (tick 137) |
| 10-19 (the install's current `oracle01`: 133 packets, 266 ticks, 47 ships; informational), the table / `sqrtf` | `r8i-r19-on` `2fdbe8493a26f164` / `r8i-r19-off` `0ba6440eb923f733` | tick 220 / tick 178 | 221 `ships_vel` / 179 `ships_vel` | the table: `ships_vel` 221, `ships_pos` 223, `univcheck` 223; `sqrtf`: 179, 182, 196; the other simulation fields none in 1..266; (`real` 2, `rng6` 161, `ships_rot` 1: excluded); the GOG stream has 529 `T` records and two `game-new` events (a second `game-new` event follows the record start; no `record-stop`) |

**Every number equals docs/m2-r8-oracle-rung-physics.md section 3 and its audit's section 3** (the audit's runs
`r8au-r17-on-a` / `-off-a` / `r8au-r18-on-a` / `-off-a` are IDENTICAL to this step's over all ticks and packets). The
engine's own sync counters with the table: 10-17 `randSyncErr 3406 univSyncErr 4611 blobSyncErr 4746 pktSyncErr 0` (the
first failing frames rand 2059 / univ 331 / blob 109, the last equal to GOG's own playback); 10-18 `888 / 1621 / 1619 / 0`.

- **The rung moved from 177 to 247 (10-17) and from 118 to 155 (10-18), with the table square root.** The tick-177
  velocity of ship 35 is now exactly GOG's `(c20420c2, c110b4d6, 41d3f9da)` (R7: -13 / +137 / +9 ulps; the one R7
  experiment -1 / +4 / -2), and the divergences of 179 (a wingman's formation slot), 180 (the collision distance shape and
  the avoidance pads), 183 (the parade slots) and 243 (the killer asteroid's wrap at z = -40,000 -> +40,000, found by
  inverting GOG's resource hash) went with it. **Without the table** (the round's `sqrtf` default) the first divergence
  stays at 177 / 118 and the difference there is the square root itself (`vecNormalize`'s double magnitude through the
  table versus `sqrtf` of a float): the x87 helpers alone moved most later fields out (`ships_n` 2048 -> 5559,
  `ships_misc` 871 -> 1593, `ships_health` 1934 -> 2043, `resources` 243 -> 1408; `rng10` 1408 and `bullets` 1917 stayed).
  **The table square root does not move the headless Mission01 stream** (`r8i-mission-table` byte-identical to the default
  run, as producer 1 measured): the decision moves only streams in which ships fly (the replays, the ai and skirmish
  streams, the mission self-test).
- **The next divergences, localised by producer 1 with the dump and the inverter, verified by its audit.** 10-17 tick 247:
  ship 40, a bRecon of player 1 with `COMMAND_MASK_PROTECTING` (guarding the bMothership 36), GOG `(420767c8, 4300eec6,
  429fe606)` versus ours `(420767ca, 4300eec5, 429fe608)` = (-2, +1, -2) ulps, every other ship equal; at 247 its RIGHT and
  FORWARD thrusts are saturated and UP is computed, its desired velocity is the protect target plus two avoidance terms,
  it rolls and turns from 245: the coordsys after that turn (not comparable: decision 48) or the UP thrust. No command
  touches it (the `C` lines of frames 115-123 are the external-build progress of player 0). 10-18 tick 155: ship 28, an
  sWorker of player 0 collecting resources, GOG v.z `4234afe1` versus ours `4234afe2` (v.y the capped -425); it avoids
  another object in every dumped tick, passes on the right at 152 and turns hard at 152-155 (torques to -9,479): the
  coordsys after a hard turn. **Owner: M2-R9 physics** (the rotation of a hard turn / roll, `protectShip` 0x004411c0's
  desired range and protect point, the avoidance's remaining compares; `hyperspaceCost`'s double return; the x87 shape of
  the ~200 other `fsqrt` call sites as they show). **What confirms it**: a DLL with `CAT_SHIP_coordsys` 0x98 (decision 48:
  `tools/oracle/dll/hook_sites.h` still says 0x80, so none of the three recordings compares the coordsys) shows the
  rotation directly; then the port that makes ship 40's velocity `(420767c8, 4300eec6, 429fe606)` at 247 and ship 28's
  v.z `4234afe1` at 155.
- **The proposed fix at 177 is decision 23 / 56**: make the exe's table square root (`fsqrt` 0x00472860, `fmathInit`
  0x004728c0, the 16,384-entry table on the 53-bit argument) the default and invert the switch (`/oracleLibmSqrt`); then
  the rung's reference reads 246 / 154 and the mission baseline does not move (measured), the replay and the ai /
  skirmish / mission-test streams do.
- **The recordings.** The install's `SavedGames/RecordedGames/oracle01` was re-recorded a third time, on 2026-10-19 at
  14:35 (sha256 `a7f20de5…`, `.pkts` `81bc090e…`: 133 packets = 266 ticks; its GOG session is
  `original/oracle/oracle01-rec-20261019-143530/`), after producer 3's audit; the 10-17 (`f9756e29…` / `7121f1ce…`,
  `original/oracle/oracle01-rec-20261017-140428/`) and 10-18 (`28d37f75…` / `ebe13cda…`,
  `original/oracle/oracle01-rec-20261018-102253/`) recordings are replayed from their archives through `--recording-src`
  / `--recording-as oracle01-r6` / `oracle01-r18`; nothing in `original/` was written. The 10-19 session's stream shows
  the same `real` one-step offset as the 10-18 one (excluded), its first divergence with the table is tick 221
  (`ships_vel`), and it is 16 seconds of game: not a reference, replayed for information. All three were made with the
  unfixed DLL (`CAT_SHIP_coordsys` 0x80), so `ships_rot` stays excluded in every session (decision 48 open).
- **Release equals debug on the replay**: `r8i-r17-rel` (`66e68c9c4cc608f1`) is IDENTICAL to `r8i-r17-off-a` over 9,600
  ticks and 4,800 packets in the simulation list plus `ships_rot` and `players` (only the excluded `rng_all` can differ,
  decision 50).

## Mission01 from the NIS-first start

What the player gets now, in the order the exe gives it (every step from docs/m2-r8-kas-runtime-mission01.md 4-5 and
docs/m2-r8-singleplayer-tutorial.md 1-2, exercised headlessly by the `missiontest` run above and by `sp`):

1. **The start.** New Game from the front end (the difficulty radio `cat_spGameDifficulty` sets the health factor and the
   build-time / cost divisors) runs `gameStart` 0x005a9290: the start-position table is shuffled (20 swaps of two
   `ranRandomFn(0) % maxplayers` picks: the 40 stream-0 draws that moved the Mission01 stream), the staging level
   `SinglePlayer\Mission01_NIS01\Mission01_NIS01.mission` is preloaded in the loading screen, and `singlePlayerStartGame`
   0x0051bdb0 plays the opening NIS `nis\n01.nis` / `n01.script` at the origin in that level (no KAS script runs there:
   the 11 `MissionNN_NISmm` staging levels have none). The install's `warp.script` says `WarpEnabled 0`, so the warp start
   (`cat_spWarpToLevel`) is not taken.
2. **After the NIS** (`singlePlayerNisStoppedCB` 0x0051b430, tick 1,771 of the test): the mothership is the player's ship
   of class Mothership and its recons take the custom formation; the mission's own level
   `SinglePlayer\Mission01\Mission01.mission` loads with `HYPERSPACE_LOADDONE`, the screens close and lock out, and
   **`kasMissionStart("mission01")`** starts the generated script (`Init_Mission01` 0x005db0f0 / `Watch_Mission01`
   0x005db150: the 20 FSMs of docs/mission-code-map.md 8). The level waits in `HYPERSPACE_SHIPSARRIVING` while the
   script has the nislet `nislet01.nis` played (through the KAS global `G_PlayNislet`; the textures of Mission01's ships
   precached first; ticks 2-1713 of the test; the white flash at its end) and then calls `StartLevelByHyperspacingShips`
   (`HYPERSPACE_CATSHIPSHYPERSPACINGIN`): the fleet hyperspaces in (`UpdateArrivingShip` 0x00518170: each ship hidden
   until it decelerates in, the mothership and its modules after 2 x BASE, the others after 3 x BASE plus the x87 random
   wait; the camera's motion cues reset with the mothership; arrived at tick 1,939 in the audit's run), and
   `G_ShipsHaveArrived` = 1 lets the FleetIntel FSM go. Mission01's script never calls `BuildControl` (the kasf that sets
   `giveComputerFleetControl`): its Taiidan enemies are KAS teams, not the fleet AI (decision 57 stands).
3. **The mission.** FleetIntel runs the intel cycle (`NISLet1` -> `WaitMessage` -> `WaitIntel` <-> `WaitTimer`) and the
   **six objectives in order: DestroyBombers, ExploreRegion, DestroyResourcers, DestroyProxAttackers, DestroyDestroyer,
   Hyperspace** (the Objectives FSM's counter 0 -> 6; the info overlay lists them, with the hyperspace objective's "next
   level" row), while the Kushan and Taiidan teams play their FSMs (`KushanFlankFrigates` UnderAttack / AttackCarrier,
   `KushanProxDefend` Patrolling / Attacking, `TaiidanBombers` AttackFlank, `TaiidanDestroyer` PreFlee / Flee,
   `TaiidanProxAttack` AttackProx, `TaiidanBattle` Docked / Create / Launch / Attacking / GuardingCruiser, `Pester`,
   `TaiidanReinforcement` SitThere, ...). The script's failure condition is the loss of the 33 proximity sensors (a run
   without the test's scripted events lost at tick 17,536 to the Taiidan raiders); a mission failed goes
   `kasfMissionFailed` -> `singlePlayerMissionFailedCB` (Fleet Command's line) -> `univKillPlayer` -> `CheckPlayerWin` ->
   the `SP_Game_Over` screen. The script writes the two KAS saves "01 - Hiigara" and "01 - ~ End of Mission"
   (`kasSave` with the difficulty slot and the six research words; the user's own "01 - Hiigara" of the GOG game loads
   through the mode-A converter: the `loadgame` test). The KAS variable store, the hashed timers, the task-bar-less
   objectives, the letterbox, the tutorial pointers, the tutorial buttons hidden behind the managers: all the kasf the
   script calls exist at the exe's bodies.
4. **The end of the mission.** The Hyperspace objective enables the hyperspace button; `spHyperspaceButtonPushed`
   0x00518f50 refuses while a ship is under construction, and Mission01 is one of the `catSpMissionNextLevel` missions
   (1, 2, 5, 7, 15, 17): the jump begins at once in the letterbox (no roll call), the fleet leaves 5 s of real time later
   (the small ships dock at once, every jumping ship is flagged in range and the fleet goes into the ether, Fleet
   Command's `0x2001020`), the white-out runs, the animatic `(1, 2)` plays (`animBinkPlay`; mission 17's would end the game
   with the credits), the ether's ships forget their damage effects and the `DeleteAfterHSOut` ships go, and the next
   level loads: **Mission02 has no generated script yet** (`catKasScripts[1].init == NULL`, `CAT-TODO(M2-R9 part B)`), so
   its level loads and waits in `HYPERSPACE_SHIPSARRIVING` for a `StartLevelByHyperspacingShips` nobody calls (Mission02
   is neither an at-once mission of `catSpMissionArrivesAtOnce` nor entered from its own NIS): the fleet stays in the
   ether and the player sees an empty level; `sp`'s screenshot loop stops at Mission01 for the same reason
   (`cat_spKasScriptGenerated`). The roll-call screen with Go Now / Emergency Hyperspace, the at-once arrival and
   the failed jump's return belong to other missions and are ported but exercised only by the tests' flow.
5. **The tutorial.** The front end's Tutorial Alert (or the Tutorial button) starts the generated Tutorial1 (`tutInitTutorial`
   0x0052f690 -> `kasMissionStart("tutorial1")`): the SpeechEvents FSM puts up the next-lesson, previous and quit buttons,
   the Tutorial FSM reaches Lesson1 with its two objectives, the prompts show in the text box with the narration, the
   objective list's pointer points, and the Next button takes Lesson2 (the test's leg); the 12 lessons' FSMs are
   generated and verified against the machine code but only Lesson1 -> Lesson2 has been run.

The exact ticks of the test: NIS 0-1771, Mission01 loaded and the script started at tick 1, nislet 2-1713, the objectives
created / completed at 4768 / 4800, 6864 / 8096, 8208 / 8752, 9760 / 10240, 10816 / 11296, 11328 (Hyperspace), the jump at
11392, `done ticks=11424 objectives=6/6`. (Producer 2's build had them at 4768 / 4800, 7792 / 9088, 9200 / 10192, 11216 /
11696, 12272 / 12752, 12784 and the jump at 12848; producer 4's exe motion-cue reset moved `rng0` by 300 draws at the
fleet's arrival and the battles with it.)

## What is left of stages 3-6

Nothing blocks the stage-6 part A exit test (met above); each item has an owner:

1. **The oracle's next rung** (M2-R9 physics): 10-17 tick 247 (ship 40) and 10-18 tick 155 (ship 28), both the coordsys
   after a hard turn / roll on the guard and avoidance paths ("Oracle mode A"); `hyperspaceCost` 0x00443c30's unrounded
   double return to its `_ftol` callers (`CAT-TODO(M2-R9 physics)`, CommandLayer.c); the x87 shape of the other `fsqrt`
   call sites and float expressions outside the flight path, each as the rung reaches it. Needs the user's ruling on
   decision 23 / 56 first (the rung cannot pass 177 without the table) and the DLL's `CAT_SHIP_coordsys` 0x98 (decision
   48) to see the coordsys.
2. **Decision 23 / 56** (the user's): the table square root as the default.
3. **Decision 48** (the user's): `tools/oracle/dll/hook_sites.h` still says `CAT_SHIP_coordsys 0x80`; all three recordings
   were made with that DLL. The one-line edit, a DLL rebuild and a fourth recording make `ships_rot` comparable and show
   the turns at 171 / 245 / 152 directly.
4. **Stage 6 part B** (M2-R9): missions 02-17's generated code (`tools/m2_kas2c_gen.py --scripts Mission02,...`, the
   `catKasScripts` rows of SinglePlayer.c, `render.c`'s screenshot gate), the "Mine" 0x004a3f20 and "Slipgate"
   `cat_scriptSetSlipgateCB` 0x004a1020 mission-sphere readers (LiveFireExercise2 and other multiplayer maps, Missions 03
   / 10 / 12 / 14), the screens and flows only the later missions use (the roll-call screen under fire with the Emergency
   Hyperspace button, the at-once arrival of the 0x00894470 missions, the failed jump's case 10, the warp start with
   `WarpEnabled 1`, the animatics between missions, mission 17's credits, a tutorial save's load: ported, unexercised),
   the 11 remaining Tutorial1 lessons' leg, the mission self-test's combat (the destroy objectives are met with
   `ShipsDamage`; the player's battle is not played) and a reproducible Tutorial1 stream (speech on the wall clock: the
   audit's O1), `Subtitle.c:1144`'s theme index for n01 / nislet01 / n06 (a small subtitle item, now in sp and the mission
   test too).
5. **Stage 5** (the user's call, "Plan for M2-R9"): the 26 `CAT-TODO(stage 5 ...)` sites of R7's list, unchanged (the
   packets' send sides of move / hyperspace / formation, the recorder's record side 4, the colour checksum, `catExtra`,
   the `CaptainGameInfo` bytes and the network game's start-position table at `tpGameCreated` +0xC8, `DeclarePlayerWin`,
   the multiplayer options word, the Titan / WON interface stubbed); the oracle needs none of it but the recorder.
6. **Stage 4 leftovers that stay**: `gunResetGimbleGuns` uncalled (decision 58), the five creators' default tactics (59),
   the folded handlers' save indices (63), the 123 IDENTICAL bodies compared without a review row; the ai self-test keeps
   the universe running after the game over (HWI-TEST, this round).
7. **Stage 3 leftovers that stay**: HW1's cursor text (decision 65; removing it re-baselines the motion dump, which moved
   this round anyway), `rndLoadTarga`'s `GL_CLAMP_TO_EDGE` and the Transformer (decision 28: the three `cat_trans*`
   functions the stage counter files under stage 6), the camera-derived render values with interpolation on (the
   high-refresh-4k feature). Closed this round: the 30 `UI_*` sound ids (all 32 sites read), the uncalled sensors helpers.
8. **Motion interpolation**: the `/noInterp` and flags-on dumps re-baselined (above); the clamp / droid blend still
   unmeasured by the two dump scenes.
9. **The delta map's review flags**: 271 MODIFIED / REWRITTEN rows outside stage 6 part A carry no review flag
   (`TitanInterface.cpp` 92: stubbed; `ETG.c` 48 and `Particle.c` 43; `ObjTypes.c` 22; `MultiplayerLANGame.c` 9,
   `MultiplayerGame.c` 6, `CommandNetwork.c` 3, `GameChat.c` 3, `TitanNet.c` 2, `NetCheck.c` 1; `texreg.c` 9, `SaveGame.c`
   6, `StatScript.c` 6, `Select.c` 4, `File.c` 3, `DefenseFighter.c` 3, 2 x `Clouds.c` / `ddraw.cpp`, 1 each in 7 files):
   the stage-5 ones are stage 5's; the rest were in earlier rounds' scopes and a later audit may sample them.
10. **Still open, the user's**: the two saves of the M0 play-test (8; a single-player one now loads), the M0 play-test
    sign-off, the fourth recording (3).

## Plan for M2-R9: stage 6 part B (missions 02-17), the rung's next item, stage 5 reduced to the recorder, and M3's play-test

**Sizing** (`python3 -I tools/m2_stage_counts.py` after this round; the tag counts from `grep -rn 'CAT-TODO(' src/`):

- **Stage 6 part B**: the **1,936 kas2c mission functions not yet generated** (2,085 - Mission01's 84 - Tutorial1's 59 -
  the 6 shared: about 566 KB of the 612,687 B; 16 missions, 298 of the map's 322 FSMs, docs/mission-code-map.md 8), all
  from the generator, which verifies every body against the machine code (its `--scripts` list is the only input; the
  `OVERRIDES` table is empty so far); the **2 `CAT-TODO(M2-R9 part B)`** sites (the `catKasScripts` rows, the "Mine" /
  "Slipgate" readers; `cat_scriptSetSlipgateCB` 0x004a1020 is the one stage-6 cat-new function the map lists as
  undefined); the HW1-matched stage-6 files are all reviewed (289 / 180 / 25, 0 unread), so part B's exe reading is the
  per-mission screens and flows (the roll call under fire, the at-once arrival, the animatics and credits, the warp
  start, the trade manager's missions, the Bentusi / pirate / Beast screens of the later missions as the mission maps
  name them) and whatever the generated code calls that no test has run (each mission's kasf set is in the map's section
  8). A per-mission self-test like Mission01's (the objectives of each mission's Objectives FSM met with scripted
  events) is the exit test's shape.
- **The rung**: one item (M2-R9 physics, "What is left" 1), after the user's two rulings.
- **Stage 5**: reduced to **the recorder's record side** (4 sites) unless the user rules otherwise: it lets hwi-engine
  record a game the GOG build plays back (a mode-B oracle), the only stage-5 piece with oracle value; the LAN send sides,
  the options word and the Titan / WON stubs stay deferred (decision 9).
- **M3's Linux play-test of Mission01 and a skirmish can start now** (below).

Four producers + four auditors + integrate (9 agents, as R7 and R8), in sequence:

| # | producer | scope | counts | done when |
|---|---|---|---|---|
| 1 | **the oracle's next rung**, physics | the coordsys after a hard turn / roll (`physUpdateObjPosVelShip`'s rotation path for a rolling ship, `aitrackHeadingWithBankPitchFunc`'s remaining intermediates), `protectShip` 0x004411c0, the avoidance's remaining compares, `hyperspaceCost`'s double return, the next `fsqrt` sites the dump points at; both archived recordings and the fourth one if it exists | about 10 bodies; the mission stream does not move (no ship flies) unless the user's fsqrt ruling lands (then once) | the 10-17 replay IDENTICAL beyond 247 (`ships_vel` at 247 = `(420767c8, 4300eec6, 429fe606)`), the 10-18 one beyond 155 (v.z `4234afe1`), the next divergences owned; with the fourth recording `ships_rot` compared |
| 2 | **missions 02-09** | the generator run for Mission02 .. Mission09 (`--scripts`), their `catKasScripts` rows, the level-load entries they need (the "Mine" / "Slipgate" readers, the meteor storm of Mission02 already ported), the per-mission screens and flows they call (the at-once arrival, the roll call under fire, the animatics between missions), a per-mission self-test (`/catMissionTest N`: each mission's objectives with scripted events) | about 900 generated functions + the "Mine" / "Slipgate" readers + the flows | missions 02-09 start from Mission01's end (or from `/catMissionTest N`'s direct start) and each reaches its objectives in order headlessly, byte-identical pairs, ASan 0; the generator `--check` current |
| 3 | **missions 10-17** | the generator run for Mission10 .. Mission17, the later missions' screens (the Bentusi / pirate / Beast flows, mission 17's end and the credits), the warp start under a test (`WarpEnabled 1` in a test copy of warp.script), a tutorial save's load, the remaining Tutorial1 lessons' leg | about 1,000 generated functions + the end-game flow | missions 10-17 as row 2; the game's end (mission 17's credits) reached headlessly; the Tutorial1 leg through Lesson12 |
| 4 | **the recorder's record side** (stage 5, reduced) and the test / tooling leftovers | the 4 `CAT-TODO(stage 5 recorder)` sites (hwi-engine records a game the GOG build can play back: a mode-B oracle of the ai self-test), the mission self-test's combat (the player's fleet flown instead of `ShipsDamage`, if cheap), a reproducible Tutorial1 stream (speech timing from the virtual clock), `Subtitle.c:1144`, the 123 stage-4 IDENTICAL bodies and the 271 unflagged rows sampled | 4 + the test items | a recording hwi-engine wrote plays back in the GOG game with the DLL's stream IDENTICAL to ours over its length (the mode-B rung); the tutorial leg a pair |

**M2-R9 integrate (the stage-6 exit)**: the R8 gates hold from a clean build (the stream re-baselined once if the fsqrt
ruling lands, stated against `0d2b76d565105a26`'s content); every mission 01-17 reaches its objectives headlessly
(byte-identical pairs); the rung beyond 247 / 155 or owned; no `CAT-TODO` naming M2-R8 or M2-R9; the generator and the
table generator `--check` current. Then M3 (the user's play-test sign-off, the one-hour 4K@144 soak, the Windows build).

**M3's Linux play-test, which can start now.** The engine, the AI, Mission01 and the skirmish are playable; what the
user should play and report (an attended run is the only one that may be audible or on screen, docs/CONSTRAINTS.md 5):

```
cd ~/Documents/GameDev/Homeworld-Infection/.claude/worktrees/m2 && tools/build.sh --release
mkdir -p ~/Documents/GameDev/Homeworld-Infection/original/scratch-m2/playtest && cd "$_"
HW_Data=~/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/data \
  ~/Documents/GameDev/Homeworld-Infection/.claude/worktrees/m2/build/release/hwi-engine /window /settingspath "$PWD"
```

(the harness's data dir is the install's own files with writable SavedGames / ScreenShots next to them, so nothing is
written in `original/game`; `/fullscreen` instead of `/window` for the real thing; `/1024` for 1024x768; the options file
`Cataclysm.cfg` is read from `~/.homeworld` and written to the settings path; a crash reproduces under ASan with
`build/debug/hwi-engine` the same way, slower). Play: (1) **Mission01 from New Game**: the opening NIS, the fleet's arrival
and the nislet, the six objectives in order (bombers, explore, resourcers, the proximity attackers, the destroyer,
hyperspace), the hyperspace button's jump and the animatic; (2) **a skirmish** from Multiplayer > Skirmish against one
computer opponent (LiveFireExercise2 is not in the list, the exe hides it too; Armed for Burial (8) is) for 10 minutes:
the start positions (shuffled now), the computer player's builds and attacks, the sensors manager, the managers'
screens; (3) **the Tutorial** from the front end: Lesson1 -> Lesson2 and as far as it goes. Report back, per item: what
differed from the GOG game in the same place (the order of events, a missing screen, text, sound or speech, a ship that
does not do what the script says), any crash or hang with the terminal's last lines (`hwi-engine: fatal signal N` or
the ASan report), the frame rate drift over the session, and the mission's KAS saves ("01 - Hiigara", "01 - ~ End of
Mission") loading back through Load Game. The known gaps to skip: Mission02 onwards has no script yet; the tutorial past
Lesson2 is untested; the roll-call screen (Go Now / Emergency Hyperspace) shows only in missions other than 01.

**What the user must decide** (none of it blocks M2-R9's missions):
1. **Decision 23 / 56, the table `fsqrt`** (restated: the round ran with `sqrtf`): with the exe's table the 10-17 replay
   matches GOG through tick 246 and the 10-18 one through 154; without it through 176 / 117, and the rung cannot pass
   177 at all (the difference there is the square root itself). Measured this round: the headless Mission01 baseline does
   not move with it; the replay, ai, skirmish and mission-test streams do. Recommendation: make the table the default
   (`FSQRT_DEFAULT 'table'` in the next workflow: the switch inverted to `/oracleLibmSqrt`).
2. **Stage 5's fate**: reduced to the recorder's record side (row 4 above), done in full, or left for after M3.
   Recommendation: the recorder only (a mode-B oracle), the rest after M3.
3. **The next recording with the DLL's `CAT_SHIP_coordsys` 0x98 edit** (decision 48): the one-line edit in
   `tools/oracle/dll/hook_sites.h`, a DLL rebuild (clang-18 + lld-link, decision 7) and a recording session make the
   coordsys comparable, which both open divergences point at. Recommendation: before M2-R9's rung producer runs; the
   10-17 recording stays the continuity reference, the 10-19 one (16 s) is not a reference.
4. Whether M3's play-test starts now, in parallel with M2-R9 (recommended: yes, the brief above), and the defaults
   recorded as decisions 70-80 in cycle/STATE.md (say if you want otherwise).

## Decisions

**Applied this round:**
- The faithfulness defaults and decisions 17-69 of cycle/STATE.md; **decision 14 retired** (`CAT_HW1_MISSION_SCRIPTS`
  gone: the Cataclysm kas2c missions replace HW1's, HW1's compiled missions are out of the build); **decision 23 / 56 kept
  at `sqrtf`** for the round (the rung judged with the switch on and off; the ruling is the user's, above); **decisions 40
  / 68 closed** (the start-position shuffle's 40 stream-0 draws landed with the KAS runtime port: the identity table and
  its `CAT-PROVISIONAL` are gone, the Mission01 stream's `rng0` moved once); **decision 41 made** (the task bar removed);
  decision 46's guard-rails untouched (the dumps re-baselined, not the feature); decision 47's exclusions and decision
  60's `real` applied to every comparison; decision 55 followed by every producer and audit; decision 57 stands
  (Mission01's generated script never calls `BuildControl`, the kasf that sets `giveComputerFleetControl`; its enemies
  are KAS teams, and the headless `/oracleMission` run keeps the direct start, so its stream does not see the AI); decision 65's sound item done (the 32 `UI_*` sites); decision 67's
  faults fixed (the soak PASSes).
- **Decision 44 updated**: the mission-stream baseline is **`0d2b76d565105a26`** (this build; debug and release IDENTICAL
  in content), which differs from `9f7416bb64fec98f` (= R6's `49240abcf9f0ed97`) in exactly `rng0` (40 draws ahead from
  tick 0, the shuffle) and `ships_rot` (from tick 1, the rung's helpers); the chain of this round's hashes is in the
  conditions table. **Decision 49 updated**: with the table square root the oracle's first divergences are tick 247
  (10-17) and 155 (10-18); with `sqrtf` 177 and 118.

**Still open (the user's; none blocks M2-R9):**
- **8.** Two saves from the M0 play-test (one mission, one skirmish): a single-player save of the GOG game loads now.
- **The M0 play-test sign-off** (docs/PLAN.md); M3's Linux play-test above.
- **Decision 48**: a fourth recording with the fixed oracle DLL, so that `ships_rot` becomes comparable.

**Surfaced by M2-R8.** Each has a default in the tree; say if you want otherwise.
70. **The table `fsqrt`** (decision 23 / 56 restated as M2-R9's first question): the default stays `sqrtf` and the switch
    `/oracleTableFsqrt` stays compiled in, off by default; the rung's reference numbers with the table are 246 / 154.
71. **The exe's x87 shape on the flight path** (the vector, matrix, heading, guidance, rotation, avoidance, formation,
    parade, collision and move-point helpers): the 53-bit intermediates and single roundings of the disassembly are the
    tree's now; libm's `sin` / `cos` stand for `fsin` / `fcos` (an experiment with the x87 instructions gave the same
    stream), `tanl` + `fma` for `fptan`'s extended product. The motion / nis guard-rail dumps are re-baselined to this
    build's (`0838a011db819920` / `d1e6f6e477906b52`; flags on `7e4f12d35a662197` / `2f18ba93741ebdc6`).
72. **The killer asteroid's wrap box** (`Asteroid.catWrapExtent` +0x1A8): written by `AddResourceToVolumeAtPosition`
    0x0049fcf0 for a new game (the larger of the universe's half size and the volume's extent per axis), copied from
    saves and recordings; a sphere storm's box and a meteor storm's unread `?` terms are zero (`CAT-FIX`).
73. **The soak faults** (decision 67 closed): `univDeleteDeadShip`'s ten ETG parameters (`CAT_ETG_CREATE_PARAMETERS` 10,
    the bounded parameter write `CAT-FIX`), the bullet / missile owner compared as a pointer, `cat_aisTeamAttack`'s
    one-past read reproduced on the exe's contiguous tables (`catAisTargetTeam`, `CAT-FIX`); the ai self-test clears
    the quit and win times after the game over so a long soak keeps running (HWI-TEST; the 400-tick exit-test run never
    reaches it). The `ai` mode's default stays 400 ticks.
74. **The kasf library takes the exe's signatures** (KAS names, timers, variables, messages and labels as `udword`
    hashes through `cat_kasHashString` 0x00491860), the 486-row table and its dispatcher are generated from the exe
    (`tools/m2_kasf_table_gen.py`, never hand-typed), HW1's compiled missions (`src/Missions`'s kas2c output) are out of
    the build and stay in the tree unbuilt; `cat_consoleKasCommand` does not free the strtok name (`CAT-FIX`); the Tutor.c
    stand-ins of the tutorial buttons are the exe's bodies now (producer 4). Chosen names: `kasfScopeHash`, the 26
    `kasfPred*` predicates of `cat_kasShipsSelectByPredicate` 0x00494f90, `kasfDistanceSquaredBits`,
    `kasfShipsCreateSome`, `kasfVectorClampToUniverse`, `kasfWideScreenInBody`, `kasfWideScreenIsActive` 0x00a43ff0,
    `kasfWideScreenCloseSensors` 0x00882104, `timTimerFind`, `timTimerLeft`, `CAT_SHIPFLAG_Invincible` (was
    `CAT_SHIPFLAG_Orphaned`), the 12 `CAT_UI_*` ids (`CAT_UI_TradingAmbient` 0x103, `Letterbox` 0x123, `Unletterbox` 0x124,
    `CrateFound` 0x128, `ChatMessage` 0x12b, `SensorsPing` 0x12c, `ManagerIntro` 0x12f, `ManagerExit` 0x130, the four pings
    0x131-0x134), `catDamageFactor` (Ship +0x3E0).
75. **The generated kas2c code** (`src/Missions/CatMissions.h`, `KasShared.c`, `SinglePlayer/Mission01.c`,
    `Tutorials/Tutorial1.c`) is written by `tools/m2_kas2c_gen.py` from the export's decompile and verified against the
    machine code (callees, constants, all-constant argument tuples), with no hand edit (an empty `OVERRIDES` table);
    the functions keep the export's kas2c names (unresolved hashes as `FSM_x…` / `S_x…`); a saved KAS function is its exe
    address minus the current mission's Watch's, searched across every generated script's list. Chosen names:
    `cat_kasShipRemovedFromSelections` (the map's), `HYPERSPACE_CATSHIPSHYPERSPACINGIN` (9) / `HYPERSPACE_CATFAILEDRETURN`
    (10), `MISSION_17`, `catSpMissionStartsInNis` 0x008944bc, `catSpMissionNis` 0x00894508, `catSpMissionArrivesAtOnce`
    0x00894470, `catSpNisNames` 0x00894550, `catSpNisletName` 0x008945a0, `catSpLevelFade` 0x00a6ac70, `catSpWhiteFlash`
    0x00a75280, `catSpLevelFromNisStop` 0x00a6ac74, `catSpFleetArriving` 0x00a6ad78, `catSpArriveAtOnce` /
    `catSpArriveAtOnceTime` 0x00a6adbc / 0x00a6adb8, `catSpStartInLoadingScreen` 0x00a6ac68, `catHrMissionNisVariant`
    0x00a6adac, `CatKasFunction`, `cat_spKasScriptFunctions`, `ResourceVolume.catDriftSpeed` / `catDriftAddY` /
    `catDriftAddX` / `catWrapExtent`.
76. **The mission oracle run keeps the direct Mission01 start** (`/oracleMission` and the self-tests on it: dock,
    command, render, hud, ui, console, build, ai, motion, nis; `HWI-ORACLE`), no NIS and no KAS, so the mission baseline
    moved only by the shuffle this round and does not see the AI; `/catMissionTest` takes the exe's NIS-first start and
    its stream (`becb49b579aba435`) is the mission's guard-rail. Default: keep both (a full NIS-first re-baseline of the
    oracle run is a later choice).
77. **The mission self-test's events**: the destroy objectives are met with the mission language's `ShipsDamage`, the
    recon with the launch and move orders, the jump with the hyperspace button's function; the player's combat is not
    simulated; no sound and no speech. The Tutorial1 leg keeps speech on, so its stream is not a pair (the audit's O1).
78. **Single player**: no mission sequence (Cataclysm's `currentMission` 1 .. 17); the KAS skill level is the difficulty
    word 0x008941a0 (HW1's `CurrentMissionSkillLevel` deleted: it doubled single-player damage); the warp start follows
    the install's `warp.script` (off); the exe's roll-call, hyperspace and arrival bodies with their `CAT-FIX`es (the
    warp's ship walk, the at-once arrival's node, the clamp release walk, the lesson name copy, `kasLoad`'s bad saved
    difficulty); a cataclysm.exe single-player save's mission tail converts through the eight new SaveCat32 roots. Chosen
    names: `catSpCapitalOrComponent`, `ShipCanMakeHyperspace`, `CAT_SHIPFLAG_LeftBehind` (catFlags18 0x40),
    `catSpEtherCleanup`, `catSpFleetAnyInHsState` / `catSpEtherAnyInHsState`, `catSpFailedJumpSoundHandle` 0x0089469c,
    `catSpMissionNextLevel` 0x00894424, `catSpWideScreenJumpTime` 0x00a6adb4, `catSpFleetCommandTime` 0x00a67488,
    `CatSpWarpLevel` / `catSpWarpLevels` 0x00a67498 and the warp words, `CAT_SP_DEG_TO_RAD`; the exe's names
    `cat_spEmergencyHyperspaceCB`, `cat_spPrepareShipHyperspaceOut`, `cat_spLaunchDockedShipsOnArrival`,
    `cat_spForgetDamageEffects`, `cat_spGetDifficultyModifiers`, `cat_spGameDifficulty`, `cat_spWarpToLevel` and its
    four setters.
79. **The tutorial**: Cataclysm's 54 flags in the exe's layout and name table (pinned 54 + 1), the pointers' objective
    flag, Cataclysm's five buttons as regions under the tutorial's root, hidden behind the three managers; the Next button
    is clicked through its region's own callback in the test (no input). Chosen names: `catTutNextTextVisible`
    0x00a6c4f0, `catTutQuitVisible` 0x00a6c508, `catTutObjPointerX/Y` 0x00a6c51c / 0x00a6c520, `CAT_TUT_POINTER_COLOUR`,
    the image indices `TUT_NEXTLESSON_*` / `TUT_QUIT_*` / `TUT_REST_*` / `TUT_NEXT_*`, the flags `bWaypoint` /
    `bWaypointIssue` / `bManagersLocked` (the exe's names), `tutpointer.objective`; the exe's `cat_tutPointerListReset`,
    `cat_tutObjPointerXYSet`, `cat_tutButtonsHideForScreen` / `RestoreAfterScreen` and the button functions.
80. **The harness** (decisions 60 / 66 updated): the install's `oracle01` is the 10-19 recording (266 ticks; not a
    reference); the 10-17 and 10-18 recordings are replayed from their archives (`--recording-as oracle01-r6` /
    `oracle01-r18`); modes `missiontest` (`--tutorial`, `--mission-test-ticks`) and `loadgame` (`--save`, `--load-ticks`;
    the save copied into the run's settings dir only) are in the exit test from this round on; `/oracleShipDump` and
    `tools/oracle/hwo_invert.py` are the permanent per-ship instrumentation. Chosen names: `catFsqrtD`, `catVecMagSqD` /
    `catVecScaleD`, `catAitrackTurn` / `catAitrackThrust`, `catGetCollSizeD`, `catResourceWrap` / `catResourceWrapAxis`,
    `Asteroid.catWrapExtent`, `CAT_ETG_CREATE_PARAMETERS`, `aisTargetTables` / `catAisTargetTeam`, `oracleShipDump*`.
