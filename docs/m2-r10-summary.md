<!-- done: exit-test, ports, deltamap, oracle-a, campaign, left, m3-plan, user-decisions, playtest-brief, decisions -->
# M2-R10 summary: the play path's leftovers (the oracle's rung past 1922 / 1721, the Mission04 render stop + the animatics + the sp gate, the tests and the oracle's leftovers) and the M3 plan

Status: **COMPLETE, 2026-10-20: the R10 exit test MET**; this doc and the cycle/STATE.md update are the integrate step's
checkpoint commit. Branch `m2`, worktree `.claude/worktrees/m2`, workflow `cycle/workflows/m2-r10.js` (run id
`wf_dd85ad7e-70e`, `ROUND_BASE 29f7c0d`, `FOURTH_RECORDING null`, `MODEB_RESULT null`: neither existed at the launch nor
exists now), run from the user's terminal session 38027cce (Fable), not from the orchestrator. 7 agents: three producers
in sequence (the three rows of docs/m2-r9-summary.md "Plan for M2-R10": the rung, physics; the render stop + the animatics
+ the sp gate; the tests and the oracle's leftovers), three auditors (each checkpointed its port: `36289a8` 0.97, `3ea7b12`
0.9375, `f9590bc` 0.864, all PASS-WITH-FIXES) and this integrate step. No producer was cut off this round. User
decisions: cycle/STATE.md "## M2" (the faithfulness defaults and decisions 17-97 applied; decision 70's table `fsqrt` the
default under every run; decision 48 still open: no fourth recording, so `ships_rot` stays excluded; mode B still the
user's step).

## Exit test (this step, from a clean build of `f9590bc`)

**Builds.** `tools/build.sh --debug --clean` and `--release --clean` on the tree at `f9590bc` (15:35; about 10 s each on
the 24-core box, gcc 13.3.0, meson 1.3.2): both rc 0, `[270/270] Linking target hwi-engine` (as R9: no new source file
this round), **0 `warning:` and 0 `error` lines** under `-Werror` (the one WARNING line is meson's sanitizer notice in the
debug build). The binaries were copied aside to `original/scratch-m2/m2r10-integrate/{debug,release}/hwi-engine` so that
nothing rebuilt under the runs.

| build | size | sha256 |
|---|---:|---|
| debug (ASan + UBSan) | 88,647,440 B | `3f7a39823b41fb7e…` |
| release (`-O2`, no sanitizers) | 5,623,728 B | `c636255a307b8faf…` |

**Runs.** 101 harness runs `r10i-*` on the plain `tools/m2_harness.sh` (silent, offscreen; `runs.txt` lines
2794-2894) in five parallel chains (A the rung's replays, the hwi-engine playbacks and missions 14-17, B the R9 gates
and the scripted self-tests, C missions 02-09, D missions 10-13, Mission01's self-test, the tutorial, the lesson load and
the failed-jump pair, E the `sp` loop to its 1,200 s timeout) beside the user's load (load average 13-27 throughout: FFXIV,
VS Code and a 12-core search were running; no run lost its summary; nobody edited the harness during the step). **Every
quitting run rc 0** except the two timeouts by design: `r10i-sp` (rc 124 at the 1,200 s timeout, **running in Mission12**,
no assertion) and `r10i-fe` (rc 124 at the 120 s timeout at Main_game_screen); **ASan 0 in every run**; LeakSanitizer
clean in every run, `r10i-sp` included (R9's 40 B at the Mission04 abort are gone with the abort). The `.hwo` hashes
differ from the producers' and audits' for the same content: the stream header carries the build identity
(`hwo_compare.py` prints it; this build's ident `f9590bce`); IDENTICAL below is the tool's verdict over every tick and
packet record.

| run | mode | rc / wall | milestone | first stop | sanitizers | run.hwo sha256 (size) | result |
|---|---|---|---|---|---|---|---|
| `r10i-mission-a` | mission | 0 / 29s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 5bd51234405ea0d8 (1476370 B) |  |
| `r10i-mission-b` | mission | 0 / 28s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 5bd51234405ea0d8 (1476370 B) |  |
| `r10i-r17-a` | replay | 0 / 57s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 9af78e15f5234902 (2630926 B) |  |
| `r10i-mission-perturb` | mission | 0 / 27s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2a25032ea0cf8a1d (1476419 B) |  |
| `r10i-mission-rel-a` | mission | 0 / 6s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a44d585d71233c4a (1476370 B) |  |
| `r10i-m2-a` | missiontest | 0 / 93s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 674817852fe7243e (6381116 B) | mission=2 ticks=25936 objectives=6/6 result=PASS |
| `r10i-mission-rel-b` | mission | 0 / 7s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a44d585d71233c4a (1476370 B) |  |
| `r10i-mission-rel-perturb` | mission | 0 / 7s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b142c2dc79e1ae53 (1476419 B) |  |
| `r10i-r17-b` | replay | 0 / 56s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 9af78e15f5234902 (2630926 B) |  |
| `r10i-mission-noi` | mission | 0 / 27s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 5bd51234405ea0d8 (1476370 B) |  |
| `r10i-m10-a` | missiontest | 0 / 135s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b2b64a1d6a268171 (5397854 B) | mission=10 ticks=21939 objectives=5/5 result=PASS |
| `r10i-mission-nonis` | mission | 0 / 27s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 5bd51234405ea0d8 (1476370 B) |  |
| `r10i-r17-libm` | replay | 0 / 55s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f9bb9674dc18078e (2630929 B) |  |
| `r10i-m2-b` | missiontest | 0 / 96s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 674817852fe7243e (6381116 B) | mission=2 ticks=25936 objectives=6/6 result=PASS |
| `r10i-mission-libm` | mission | 0 / 28s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 5bd51234405ea0d8 (1476370 B) |  |
| `r10i-r18-a` | replay | 0 / 31s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8112853eacffc80a (966649 B) |  |
| `r10i-statics` | statics | 0 / 14s | ship statics self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | ship statics: 143 loaded, 2 not found (disabled), 27 other .shp; self-test done, 143 of 145 ship types loaded |
| `r10i-statics-init` | statics | 0 / 14s | ship statics self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | ship statics: 143 loaded, 2 not found (disabled), 27 other .shp; self-test done, 143 of 145 ship types loaded |
| `r10i-r18-b` | replay | 0 / 31s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8112853eacffc80a (966649 B) |  |
| `r10i-meshes` | meshes | 0 / 13s | mesh self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | meshes: 1758 listed, 1729 .geo/.peo loaded, 29 .mad loaded, 0 with bad=; self-test done, 1758 files, 0 with bad objects |
| `r10i-textures` | textures | 0 / 23s | texture self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | textures: listing lookups 12403/12403 found, 0 size/flag mismatches, 0 bad sharedFrom;self-test done, 12403 listed, 12403 found in listing, 1742 without a file  |
| `r10i-r18-libm` | replay | 0 / 32s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b7b9176c333702fb (966654 B) |  |
| `r10i-oracle` | oracle | 0 / 12s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 11966c5c4674c16f (140723 B) | oracle: self-test done ticks=512 packets=256 randSyncErr=0 univSyncErr=0 blobSyncErr=0 records=772 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infection |
| `r10i-m10-b` | missiontest | 0 / 136s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b2b64a1d6a268171 (5397854 B) | mission=10 ticks=21939 objectives=5/5 result=PASS |
| `r10i-oracle-rel-a` | oracle | 0 / 1s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8b97d3114e27ae8e (140723 B) | oracle: self-test done ticks=512 packets=256 randSyncErr=0 univSyncErr=0 blobSyncErr=0 records=772 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infection |
| `r10i-oracle-rel-b` | oracle | 0 / 1s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8b97d3114e27ae8e (140723 B) | oracle: self-test done ticks=512 packets=256 randSyncErr=0 univSyncErr=0 blobSyncErr=0 records=772 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infection |
| `r10i-oracle-rel-perturb` | oracle | 0 / 1s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c30f9e1b3dfea4cb (140773 B) | oracle: self-test done ticks=512 packets=256 randSyncErr=0 univSyncErr=0 blobSyncErr=0 records=773 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infection |
| `r10i-r17-rel` | replay | 0 / 12s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 60de70ccef41fedc (2630926 B) |  |
| `r10i-m3-a` | missiontest | 0 / 89s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4600af7eb64edc13 (4385810 B) | mission=3 ticks=17825 objectives=5/5 result=PASS |
| `r10i-etg` | etg | 0 / 14s | etg effect self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | etg: 264 listed; etgStartup done, 8 scripts, 197 effects loaded, 0 referenced but not loaded;self-test done, 9 scripts parsed, 255 .ebg listed, 58 unused (no sc |
| `r10i-play01-a` | replay | 0 / 31s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4cd0ef18b5c7bc8c (1315714 B) |  |
| `r10i-dock` | dock | 0 / 26s | dock self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | fighters=6 outside=6 corvettes=2 carrierleg=2 processorleg=1 RUs=1850 jackPausedTick=1024 result=PASS |
| `r10i-m11-a` | missiontest | 0 / 63s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a7639dc528811bb7 (2036509 B) | mission=11 ticks=8275 objectives=5/5 result=PASS |
| `r10i-play01-b` | replay | 0 / 30s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4cd0ef18b5c7bc8c (1315714 B) |  |
| `r10i-command` | command | 0 / 23s | command self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | link=252 mcv=1 unlink=285 research=1/1 mimic=1 point=1/1 result=PASS |
| `r10i-render` | render | 0 / 21s | render self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | frames=583 objects=582 lensflares=583 foglayers=1166 result=PASS |
| `r10i-play02-a` | replay | 0 / 29s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 35a2bf7816623808 (1315721 B) |  |
| `r10i-m3-b` | missiontest | 0 / 89s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4600af7eb64edc13 (4385810 B) | mission=3 ticks=17825 objectives=5/5 result=PASS |
| `r10i-sound` | sound | 0 / 20s | sound self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | soundtest: start: audio driver dummy, SFX 1, speech 1, mixer ticks 56;Music4Cat.wxd: 62 streams, track 0 (offset 8, bitrate 512, 2 channels) decoded 43 blocks;S |
| `r10i-play02-b` | replay | 0 / 29s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 35a2bf7816623808 (1315721 B) |  |
| `r10i-m11-b` | missiontest | 0 / 63s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a7639dc528811bb7 (2036509 B) | mission=11 ticks=8275 objectives=5/5 result=PASS |
| `r10i-hud` | hud | 0 / 21s | hud self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | boxed=19 selected=154 frames=537 statusbars=6840 infopanels=537 pie=537 result=PASS |
| `r10i-ui` | ui | 0 / 19s | ui self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | selected=154 rows=10 popupframes=16 commands=26 targetrows=1 statsframes=8 result=PASS |
| `r10i-fescreens` | fescreens | 0 / 12s | fescreens self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | files=22 loaded=22 screens=195 pushed=195 popped=195 wrong=0 skipped=0 listchanged=0 userregionstandins=84 result=PASS; options=34 startdefaults=34 readback=34 result=PASS |
| `r10i-strings` | strings | 0 / 12s | strings self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | ids=1705 mapback=1705 resolved=1681 nostring=24 wrongtext=0 wrongnostring=0 result=PASS |
| `r10i-m4-a` | missiontest | 0 / 86s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | bc1d5df38e9f5c25 (5122089 B) | mission=4 ticks=20818 objectives=5/5 result=PASS |
| `r10i-keys` | keys | 0 / 12s | keys self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | rows=197 known=149 unknown=48 commands=147 bindings=149 startsave=1 saved=1 reload=1 reset=1 newkey=1 conflicts=1 shares=1 failures=0 result=PASS |
| `r10i-console` | console | 0 / 18s | console self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | selected=154 frames=4 shown=31 set=24 commands=8 keys=32 kas=6 closed=1 failures=0 result=PASS |
| `r10i-m12-a` | missiontest | 0 / 102s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 5ff39c8ffab9379b (2705630 B) | mission=12 ticks=10995 objectives=3/3 result=PASS |
| `r10i-build-a` | build | 0 / 30s | build self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 3e0464cd827c947b (1722370 B) | driver=1 SU=40/100 RUs=25925 result=PASS |
| `r10i-m4-b` | missiontest | 0 / 86s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | bc1d5df38e9f5c25 (5122089 B) | mission=4 ticks=20818 objectives=5/5 result=PASS |
| `r10i-build-b` | build | 0 / 30s | build self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 3e0464cd827c947b (1722370 B) | driver=1 SU=40/100 RUs=25925 result=PASS |
| `r10i-m14-a` | missiontest | 0 / 151s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | de1be2f4149b7d82 (5496008 B) | mission=14 ticks=22338 objectives=4/4 result=PASS |
| `r10i-nisparse` | nisparse | 0 / 12s | nisparse self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | nis=14 nisScripts=14 animatics=18 objects=593 events=1488 cataclysmEvents=31 unknownKeywordLines=47 failed=0 result=PASS |
| `r10i-toggle` | toggle | 0 / 12s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | start=1/1 fails=0 result=PASS |
| `r10i-toggle-noi` | toggle | 0 / 12s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | start=0/0 fails=0 result=PASS |
| `r10i-toggle-non` | toggle | 0 / 12s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | start=1/0 fails=0 result=PASS |
| `r10i-m12-b` | missiontest | 0 / 103s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 5ff39c8ffab9379b (2705630 B) | mission=12 ticks=10995 objectives=3/3 result=PASS |
| `r10i-ai-a` | ai | 0 / 22s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 9f43cbfb5ef0b199 (98768 B) | fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconcommand=49 build=17 harvest=33 skirmishplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS |
| `r10i-m5-a` | missiontest | 0 / 76s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a0725ce96ea68d92 (4031324 B) | mission=5 ticks=16384 objectives=6/6 result=PASS |
| `r10i-ai-b` | ai | 0 / 23s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 9f43cbfb5ef0b199 (98768 B) | fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconcommand=49 build=17 harvest=33 skirmishplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS |
| `r10i-ai-soak` | ai | 0 / 33s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | bf8b4579fb36b512 (984370 B) | fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconcommand=49 build=17 harvest=33 skirmishplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS |
| `r10i-m5-b` | missiontest | 0 / 77s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a0725ce96ea68d92 (4031324 B) | mission=5 ticks=16384 objectives=6/6 result=PASS |
| `r10i-skirmish-lfe2-a` | skirmish | 0 / 24s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a8c47a7507628300 (147967 B) | ticks=600 ships=91 result=PASS |
| `r10i-m14-b` | missiontest | 0 / 155s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | de1be2f4149b7d82 (5496008 B) | mission=14 ticks=22338 objectives=4/4 result=PASS |
| `r10i-skirmish-lfe2-b` | skirmish | 0 / 22s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a8c47a7507628300 (147967 B) | ticks=600 ships=91 result=PASS |
| `r10i-skirmish-armed8` | skirmish | 0 / 23s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 739466459c63a79a (147967 B) | ticks=600 ships=54 result=PASS |
| `r10i-loadgame-a` | loadgame | 0 / 29s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ecdf98716cb11dc3 (886495 B) | ticks=3600 from=1985 to=5585 mission=1 tutorial=2 ships=141 objectives=0/1 kasTeams=36 result=PASS |
| `r10i-loadgame-b` | loadgame | 0 / 28s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ecdf98716cb11dc3 (886495 B) | ticks=3600 from=1985 to=5585 mission=1 tutorial=2 ships=141 objectives=0/1 kasTeams=36 result=PASS |
| `r10i-warp10-a` | warpstart | 0 / 20s | warp-start self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e1d321f0b217650f (24968 B) | level=10 ticks=99 applied=1 level=1 kas=1 arrived=35 fleet=1 failed=0 result=PASS |
| `r10i-m13-a` | missiontest | 0 / 222s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 08b09d8a9ec393bb (4744478 B) | mission=13 ticks=19283 objectives=3/3 result=PASS |
| `r10i-warp10-b` | warpstart | 0 / 20s | warp-start self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e1d321f0b217650f (24968 B) | level=10 ticks=99 applied=1 level=1 kas=1 arrived=35 fleet=1 failed=0 result=PASS |
| `r10i-m6-a` | missiontest | 0 / 167s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8a2062d83bda4342 (7629566 B) | mission=6 ticks=31011 objectives=7/7 result=PASS |
| `r10i-motion-noi` | motion | 0 / 46s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | frames=1431 sampled=1431 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/r10i-motion-noi/motion.dump result=PASS |
| `r10i-motion-on` | motion | 0 / 46s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | frames=1431 sampled=1431 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/r10i-motion-on/motion.dump result=PASS |
| `r10i-m15-a` | missiontest | 0 / 248s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | aa14f9db5b467ff2 (3795164 B) | mission=15 ticks=15424 objectives=3/3 result=PASS |
| `r10i-nis-noi` | nis | 0 / 52s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/r10i-nis-noi/nis.dump result=PASS |
| `r10i-m6-b` | missiontest | 0 / 173s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8a2062d83bda4342 (7629566 B) | mission=6 ticks=31011 objectives=7/7 result=PASS |
| `r10i-nis-on` | nis | 0 / 54s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/r10i-nis-on/nis.dump result=PASS |
| `r10i-m13-b` | missiontest | 0 / 224s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 08b09d8a9ec393bb (4744478 B) | mission=13 ticks=19283 objectives=3/3 result=PASS |
| `r10i-missiontest-a` | missiontest | 0 / 71s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 01759e3c6af634f5 (3246588 B) | mission=1 ticks=11424 objectives=6/6 result=PASS |
| `r10i-m7-a` | missiontest | 0 / 103s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 3e124280e333a7ff (4082492 B) | mission=7 ticks=16592 objectives=4/4 result=PASS |
| `r10i-fe` | fe | 124 / 120s | Main_game_screen | none: still running at the 120s timeout | ASan 0; no unsuppressed leak |  |  |
| `r10i-missiontest-b` | missiontest | 0 / 63s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 01759e3c6af634f5 (3246588 B) | mission=1 ticks=11424 objectives=6/6 result=PASS |
| `r10i-m15-b` | missiontest | 0 / 238s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | aa14f9db5b467ff2 (3795164 B) | mission=15 ticks=15424 objectives=3/3 result=PASS |
| `r10i-sp` | sp | 124 / 1200s | universe running (univUpdateCounter > 0) | none: still running at the 1200s timeout | ASan 0; no unsuppressed leak |  |  |
| `r10i-m7-b` | missiontest | 0 / 86s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 3e124280e333a7ff (4082492 B) | mission=7 ticks=16592 objectives=4/4 result=PASS |
| `r10i-tutorial-a` | missiontest | 0 / 42s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4c62ba9ef9c64ab8 (2569841 B) | ticks=10444 kas=1 lesson1=2 objectives=2 buttons=1 prompts=7/102/158 pointer=1 next=1108 lesson2=1110 lessons=12/12 complete=12/12 inOrder=1 nextClicks=11 result=PASS |
| `r10i-tutorial-b` | missiontest | 0 / 40s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4c62ba9ef9c64ab8 (2569841 B) | ticks=10444 kas=1 lesson1=2 objectives=2 buttons=1 prompts=7/102/158 pointer=1 next=1108 lesson2=1110 lessons=12/12 complete=12/12 inOrder=1 nextClicks=11 result=PASS |
| `r10i-tload-a` | loadgame | 0 / 22s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cc08e4e1684028a2 (886516 B) | ticks=3600 from=8361 to=11961 mission=1 tutorial=1 ships=30 objectives=1/1 kasTeams=10 result=PASS |
| `r10i-tload-b` | loadgame | 0 / 21s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cc08e4e1684028a2 (886516 B) | ticks=3600 from=8361 to=11961 mission=1 tutorial=1 ships=30 objectives=1/1 kasTeams=10 result=PASS |
| `r10i-m8-a` | missiontest | 0 / 121s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f2bcc6c68fbbaa14 (7223667 B) | mission=8 ticks=29361 objectives=6/6 result=PASS |
| `r10i-m16-a` | missiontest | 0 / 185s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2139819be3a5bd13 (5295518 B) | mission=16 ticks=21523 objectives=6/6 result=PASS |
| `r10i-fj3-a` | missiontest | 0 / 73s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7ca867e8435b2f41 (4346699 B) | mission=3 ticks=17667 objectives=5/5 result=PASS |
| `r10i-fj3-b` | missiontest | 0 / 73s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7ca867e8435b2f41 (4346699 B) | mission=3 ticks=17667 objectives=5/5 result=PASS |
| `r10i-m8-b` | missiontest | 0 / 128s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f2bcc6c68fbbaa14 (7223667 B) | mission=8 ticks=29361 objectives=6/6 result=PASS |
| `r10i-m16-b` | missiontest | 0 / 188s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2139819be3a5bd13 (5295518 B) | mission=16 ticks=21523 objectives=6/6 result=PASS |
| `r10i-m9-a` | missiontest | 0 / 133s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 985c37906f443d3c (5072642 B) | mission=9 ticks=20617 objectives=6/6 result=PASS |
| `r10i-m9-b` | missiontest | 0 / 124s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 985c37906f443d3c (5072642 B) | mission=9 ticks=20617 objectives=6/6 result=PASS |
| `r10i-m17-a` | missiontest | 0 / 173s | mission self-test: the game's end (credits) | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d3aabd18f67ae6cb (3834034 B) | game end tick=15584; mission=17 ticks=15584 objectives=4/4 result=PASS |
| `r10i-m17-b` | missiontest | 0 / 175s | mission self-test: the game's end (credits) | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d3aabd18f67ae6cb (3834034 B) | game end tick=15584; mission=17 ticks=15584 objectives=4/4 result=PASS |

**Conditions.**

| # | condition | result | met |
|---|---|---|---|
| (a) | the R9 gates hold; the stream UNCHANGED against `553845b65e87f3b5`'s content | **Mission01 headless**: debug pair `r10i-mission-a/b` byte-identical, sha256 **`5bd51234405ea0d8`** (1,476,370 B, 6,001 `T` records, 154 ships), ASan 0; `mission_test.sh` **MISSION TEST PASS** (the perturbed run caught at tick 3000 / `rng10`, the flipped `ships_health` at 4000); the release pair **`a44d585d71233c4a`** byte-identical, IDENTICAL to the debug stream and to R9's release `ba8ee6e78597c175`, its own MISSION TEST PASS; `/noInterp`, `/noNISInterp` and `/oracleLibmSqrt` runs all byte-identical to A; **against R9's `r9i-mission-a` (`553845b65e87f3b5`): IDENTICAL in every one of the 21 fields over ticks 0-6000** (no ship flies there: the flight / combat ports do not reach it, decision 57), and IDENTICAL to every producer's and audit's Mission01 run of the round (`r10a1f-mission-a`, `r10a2-mission-a`, `r10a3-mission-a`, `r10p3g-mission-a`). **fe** reaches Main_game_screen (rc 124 at its 120 s timeout, ASan 0, 0 fescreen reports). **Self-tests** all PASS with ASan 0: statics 143 of 145 (x2, the static-init variant too), meshes 1,758 files / 0 bad, textures 12,403 / 12,403, oracle (debug `11966c5c4674c16f`, 0 sync errors) and the release `selftest.sh` pair `8b97d3114e27ae8e` **SELFTEST PASS**, etg (197 effects, 0 missing), dock, command, render, sound, hud, ui, fescreens (22 / 22, 195 screens, `options=34`; **0 fescreen UBSan reports**), strings **1,705 / 1,705** (`urls=3 whole=3`), keys (197 rows, 0 failures), console **tablerows=486**, build pair, nisparse, toggle x3 (`1/1`, `0/0`, `1/0`), ai pair + the 4,000-tick soak, skirmish LFE2 pair (91 ships) + Armed8, missiontest pair 6 / 6, loadgame pair, warp start (level 10) pair, lesson load pair, Tutorial1 pair 12 / 12; the **`/noInterp` dumps `183681235a3fdc7b` / `13ca70eea998e5de`** unchanged (decision 46); `tools/m2_layout32.sh` **OK**; the generators `--check` current (`gen_kas_missions.py`, `gen_mission_tables.py`, `gen_tutorial.py`); every guard-rail stream compared per field against R9's with the producers' statements (the table below); the UBSan kinds listed against R9's (below) | **yes** |
| (b) | every mission 01-17 reaches every objective in order headlessly (pairs byte-identical, Mission06 too, ASan 0); the table against R9's hashes, moving only as stated; Mission17's credits; the failed-jump pair; the combat as producer 3 left it | the mission table below: **17 / 17 PASS with every objective in its order** (`inOrder=1` in every run; Mission01 6 / 6 at 11,424 through the chained jump, 02 .. 17 through the warp start), **all 17 pairs byte-identical, Mission06's too** (R9's `rng_all`-only pair difference is gone: the nebula fix), ASan 0 in all 34 runs; every stream IDENTICAL to producer 3's final `r10p3g-mNN-a` (and Mission01's to producer 1's `r10p1g-missiontest-a`); against R9's hashes every stream but Mission07's moved, each at producer 1's stated first ticks (section 9 of its report) or by a stated render-side `rng_all` move (02 / 03 / 10 / 14 by producer 2's texture-registry change, 06 by producer 3's nebula fix; the table lists every field); **Mission17 ends the game** (`game end tick=15584 credits=Credits-GameEnd.cred`, 4 / 4); **the failed-jump pair** `r10i-fj3-a/b` byte-identical, IDENTICAL to producer 3's, the fleet back at 17635; **the combat** scripted away as producer 3 left it (decision 109: measured, deferred to the attended play-test) | **yes** |
| (c) | the sp gate: `sp` through Mission04 and beyond to its timeout with ASan 0, no fatal, levels and screenshots per level, no `Select.c` / `FOW.c` UBSan; the animatic diagnostic line at every transition | `r10i-sp --timeout 1200` (`/debugScreenshots`, the debug build): **walks Mission01_NIS01 -> 01 -> 02 -> 03 -> 04_NIS02 -> 04 -> 05 -> 06_NIS04 -> 06 -> 07_NIS05 -> 07 -> 08 -> 09 -> 10_NIS06 -> 10 -> 11 -> 12_NIS07 (`n07.nis`) -> 12** and is running in Mission12 at the timeout (its univUpdateCounter 128 at the last screenshot; producer 2's 900 s run reached Mission09): **127 screenshots** (`settings/ScreenShots/golden-NN-*.jpg`: 01 10, 02 8, 03 14, 04 13, 05 8, 06 13, 07 8, 08 13, 09 13, 10 13, 11 13, 12 1; producer 2's counts per level), **ASan 0, no leak, no fatal, no assertion** (the Mission04 `texreg.c (569)` stop is gone), **0 `Select.c` / `FOW.c` reports, 0 `is_final_tex` lines**; the harness line **`animatics: 12 skipped (headless): Animatic01 .. Animatic12`** (one `animatic: skipped (headless) a=N b=N+1 file=AnimaticNN` line at each of the 12 transitions 0->1 .. 11->12); UBSan kinds in the run: `Particle.c` / `ETG.c` members and `BTG.c:389` / `:392` only (R9's) | **yes** |
| (d) | the oracle's rung: both archived replay pairs, hwo_compare against the GOG streams (10-17 to 9600, 10-18 to 3526, the task's field list and exclusions, default and `/oracleLibmSqrt`): IDENTICAL beyond 1922 / 1721 or the next divergence localised; against docs/m2-r10-oracle-rung-physics.md and its audit; mode B; the hwirec playback pairs | pairs `r10i-r17-a/b` **`9af78e15f5234902`** (2,630,926 B, 9,601 ticks, 4,800 packets) and `r10i-r18-a/b` **`8112853eacffc80a`** (966,649 B, 3,527 ticks, 1,763 packets) byte-identical, ASan 0; with the task's list from tick 1: **`rng6` at 161 in both** and **`real` at 2 in 10-18** (the exclusions); without them **10-17 IDENTICAL through 2822, first divergent `ships_vel` 2823** (R9: 1922; +901) and **10-18 IDENTICAL through 2870, first divergent `ships_vel` 2871** (R9: 1721 `bullets`; +1,150); every per-field tick and sync counter equals the report's and the audit's ("Oracle mode A" below); with `/oracleLibmSqrt` 177 / 118 (R9's); the release replay equal to the debug on every field but `rng_all` (6377, as R9); both replays IDENTICAL in every field to producer 1's, the rung audit's and producer 3's. **The next divergences are localised** (the same mechanism: a bProcessor following the turning bWorker 44; owner M3 oracle physics). **Mode B still not run**: no `original/oracle/hwirec*-play-*` (ls), no fourth recording (the newest `oracle01-rec-*` is 10-19's). The hwirec playbacks: `r10i-play01-a/b` `4cd0ef18b5c7bc8c` IDENTICAL to R9's and to its record run; `r10i-play02-a/b` `35a2bf7816623808` IDENTICAL to producer 1's / 3's, moved from R9's at 706 by the ports as stated | **yes** (the rung advanced and the next divergence is localised; the physics beyond it is M3's) |
| (e) | `grep -rn -E 'CAT-(TODO\|PROVISIONAL)\([^)]*(M2-R(9\|10)\|stage 6)' src/` empty; the rest counted by owner; `tools/build.sh --debug` / `--release` from clean with 0 warnings | empty (0 lines). `/bin/grep -rn CAT-TODO src/`: **35 lines, 26 owner tags**: stage 5 23 (plain 8: CommandWrap.c's point / mimic / upgrade / special-target senders and the external-build packet, NetCheck.c's `recPackRecordSaveHeader`, UnivUpdate.c's dock words; multiplayer options 8: PopupMenu.c x2, ResearchGUI.c, Salvage.c, ShipSelect.c, UnivUpdate.c, mainrgn.c, MultiplayerGame.c's `mgUnitCapsEnabled`; LAN 6: CommandNetwork.c, HorseRace.c, LevelLoad.c, NetCheck.c, UnivUpdate.c, utility.c; multiplayer 1: SaveGame.c's 0x2B0 layout), **M3 oracle physics 3** (`Collision.c:110` `RangeToTarget`, `:136` `RangeToTargetGivenDist`, `Clouds.c:1627` `cloudUpdateSystem`; R9's two `M2-R10 physics` tags closed by the ports); **4 `CAT-PROVISIONAL`** (R9: 5; `ObjTypes.c:340`, `ShipSelect.c:2749` / `:3465`, `Mothership.c:97`; `selCentrePointComputeGeneral`'s retired). Tag counts: `CAT` 4,459, `CAT-FIX` 194, `CAT-DORMANT` 13, `HWI-ORACLE` 49, `HWI-TEST` 143, `HWI-FIX` 27, `HWI-EXPERIMENT` 6, `HWI-FEATURE(motion-interp)` 441 (count with `/bin/grep`: the shell's ugrep wrapper skips `MultiplayerGame.c` as binary). The clean builds: 0 warnings (above) | **yes** |

**R10 exit test: MET** (every condition; the rung's "beyond 1922 / 1721" met by +901 / +1,150 ticks with the next
divergence localised and owned).

**Guard-rail streams against R9's** (`hwo_compare.py`, every one of the 21 fields; the first divergent tick per field,
`-` = IDENTICAL; then the same stream against the producers' and the audits' runs of this round):

| stream | R9 (`r9i-*`) | this step | moved against R9 (first tick per field) | producer statement | against the round's runs |
|---|---|---|---|---|---|
| ai, 400 | `cefa7bd984ff3f57` | `9f43cbfb5ef0b199` (a = b) | **IDENTICAL** (no ship fights in 400 ticks) | producer 1 section 9: unchanged | IDENTICAL to `r10p1g-ai-a`, `r10p3g-ai-a` |
| ai soak, 4,000 | `4cd65bfa42cfa6c6` | `bf8b4579fb36b512` | rng10 / rng_all / ships_health 431, ships_pos / ships_vel / univcheck 455, ships_rot 457, bullets 458, missiles 589, rng0 / ships_n / ships_misc 717, rng6 1072, players 1137, statcheck 1430, resources 1984 | producer 1: exactly these (the first bullet of the soak at 431: the line of sight and the interceptor flight); **no further move inside the round** (decision 84's "re-run the soak": done, PASS, 16 / 16 checks) | IDENTICAL to `r10p1g-ai-soak`, `r10p3g-ai-soak` |
| skirmish LFE2, 600 | `17fd0322a6e1a571` | `a8c47a7507628300` (a = b) | **IDENTICAL** | producer 1: unchanged (the bigotry default 0 leaves it: producer 3) | IDENTICAL to `r10p1g-skirmish-lfe2-a` |
| skirmish Armed8, 600 | `f9fb398a3ff83ef4` | `739466459c63a79a` | **IDENTICAL** | producer 1: unchanged | IDENTICAL to `r10p1g-skirmish-armed8`, `r10p3g-skirmish-armed8` |
| build, 7,000 | `8622e0787ba94553` | `3e0464cd827c947b` (a = b) | **IDENTICAL** | producer 1: unchanged | IDENTICAL to `r10p1g-build-a`, `r10p3g-build-a` |
| missiontest (Mission01), 11,425 | `cc7f22e7129b9471` | `01759e3c6af634f5` (a = b, 6 / 6) | rng10 / rng_all / ships_pos / ships_vel / ships_rot / univcheck 1772 (the fleet's arrival at 1939 is after it: the NIS's last frames), bullets 1782, ships_misc 3564, ships_health 4754, ships_n / players 5643, rng6 5673, rng0 6689 | producer 1: exactly these | IDENTICAL to `r10p1g-missiontest-a` |
| loadgame ("01 - Hiigara"), 3,600 | `7a893c872f5e910b` | `ecdf98716cb11dc3` (a = b) | ships_rot 1985, rng10 / rng_all / ships_vel / bullets 1986, ships_pos 1991, univcheck 2029, ships_health 4753, ships_n / ships_misc / players 5062, rng6 5065 | producer 1: exactly these (from the first tick after the load) | IDENTICAL to `r10p1g-loadgame-a`, `r10p3g-loadgame-a` |
| warp start (level 10), 1 tick | `r9i-warp10-a` | `e1d321f0b217650f` (a = b) | ships_rot 1 only | producer 1: `ships_rot` at 1 only | IDENTICAL to `r10p1g-warp10-a`, `r10p3g-warp10-a` |
| Tutorial1 leg, 10,444 | `416f819eaa29e3e0` | `4c62ba9ef9c64ab8` (a = b, 12 / 12) | ships_rot 4753, bullets 4764, ships_vel 4814, ships_pos 4932, univcheck 4936, rng10 / rng0 / rng_all / ships_health 4966 | producer 1: exactly these (Lesson 07's attack) | IDENTICAL to `r10p1g-tutorial-a`, `r10p3g-tutorial-a` |
| lesson load ("LESSON 11"), 3,600 | `r9i-tload-a` | `cc08e4e1684028a2` (a = b, 1 / 1) | rng0 / rng_all / ships_pos / ships_vel / ships_rot 8360 (the first tick after the load), univcheck 8364 | producer 1: from 8360 | IDENTICAL to `r10p1g-tload-a`, `r10p3g-tload-a` |
| replay 10-17 | `3fb2f34e7e5baf9f` | `9af78e15f5234902` (a = b) | ships_rot 1921, ships_vel 1922, ships_pos 1954, bullets 2005, univcheck 2020, rng10 / rng_all / ships_health 2043, ships_misc 5885, ships_n / players / statcheck 5890 (R9's divergence removed; nothing before it) | the rung (producer 1 sections 6-7) | IDENTICAL to `r10p1g-r17-a`, `r10a1-r17-a`, `r10p3g-r17` (every field) |
| replay 10-18 | `abaa0ad279c50bec` | `8112853eacffc80a` (a = b) | bullets / rng_all 1721, ships_rot 1743, rng0 / rng10 / ships_pos / ships_vel / ships_health / univcheck 1749, ships_n / ships_misc / players 1753, rng6 1769, statcheck 1805 | the rung | IDENTICAL to `r10p1g-r18-a`, `r10p3g-r18` |
| hwirec01 playback (Armed8) | `85fddfcf8b41facb` | `4cd0ef18b5c7bc8c` (a = b) | **IDENTICAL** (and to its record run `r9r-rec01` from tick 1 on every field but rng0 / rng_all 2, rng6 321: as R9) | producer 1: unchanged | IDENTICAL to `r10p1g-play01-a`, `r10p3g-play01-a` |
| hwirec02 playback (LFE2) | `c8f6c7dc60e47434` | `35a2bf7816623808` (a = b) | ships_rot 706, ships_vel 709, rng10 / rng_all / ships_pos / ships_health / bullets / univcheck 831, ships_n / ships_misc / statcheck / players 1214, rng6 1800, resources 2976, derelicts 3840 | producer 1: exactly these (the flight / combat ports under R9's recorded commands) | IDENTICAL to `r10p1g-play02-a`, `r10p3g-play02-a` |
| motion / nis `/noInterp` dumps | `183681235a3fdc7b` / `13ca70eea998e5de` | **the same** | unchanged (decision 46) | producer 1: unchanged | equal to the producers' |

**The missions** (the per-mission self-test, debug build, `--mode missiontest --mission N --mission-test-ticks 90000
--timeout 3600`; Mission01 without `--mission`; every run ends 32 ticks after the next level loads, Mission17 at the
credits; "moved against R9" lists the first divergent tick per field; `rng_all`-only moves are render-side):

| mission | objectives | done tick | hash (a) | pair | against producer 3 / the audits | moved against R9 (`r9i-mN-a`) | entry; the jump |
|---|---|---|---|---|---|---|---|
| 01 | 6 / 6 PASS | 11,424 | `01759e3c6af634f5` | byte-identical | IDENTICAL `r10p1g-missiontest-a` | from 1772 (the table above) | the NIS-first start (n01, the nislet); the chained jump to Mission02 at 11,392 |
| 02 | 6 / 6 PASS | 25,936 | `674817852fe7243e` | byte-identical | IDENTICAL `r10p3g-m02-a`; `r10p1g-m2-a` at 11035 `rng_all` only | ships_rot 2, ships_vel 5, rng_all 11035, ships_pos 11065, bullets 11090, univcheck 11113, rng10 11138, missiles 11218, rng0 / ships_health 11462, ships_n / ships_misc / players 11640, rng6 11643, statcheck 18733 | warp; letterbox jump at 25,776 -> Mission03 |
| 03 | 5 / 5 PASS | 17,825 | `4600af7eb64edc13` | byte-identical | IDENTICAL `r10p3g-m03-a`; `r10p1g-m3-a` at 6708 `rng_all` only | **`rng_all` only, from 6708** (producer 2's trail draw) | warp; roll call, Go Now at 17,472 -> Mission04_NIS02 |
| 04 | 5 / 5 PASS | 20,818 | `bc1d5df38e9f5c25` | byte-identical | IDENTICAL `r10p3g-m04-a`, `r10p1g-m4-a` | ships_rot 8467, ships_vel 8468, ships_pos 8473, univcheck 8492, rng10 / rng_all / bullets 8510, ships_health 11169, rng0 11194, ships_n / ships_misc / players 11210, statcheck 17104 | warp (at once); roll call, Go Now at 20,464 -> Mission05 |
| 05 | 6 / 6 PASS | 16,384 | `a0725ce96ea68d92` | byte-identical | IDENTICAL `r10p3g-m05-a`, `r10p1g-m5-a` | ships_rot 2518, ships_vel 3394, ships_pos 5033, rng10 / rng_all / ships_health / bullets 5034, univcheck 5043, rng0 5080, ships_n / ships_misc / players / statcheck 5107, rng6 5255, resources 6192 | warp (at once); letterbox jump at 16,224 -> Mission06_NIS04 |
| 06 | 7 / 7 PASS | 31,011 | `8a2062d83bda4342` | **byte-identical** (R9: `rng_all` only differed) | IDENTICAL `r10p3g-m06-a` (= its b / c / d); `r10p1g-m6-a` at 22235 `rng_all` only (the nebula fix) | ships_rot 3169, rng10 / rng_all / bullets 3234, ships_vel 3236, ships_pos 3242, univcheck 3402, ships_misc 15061, ships_health 15768, ships_n / players 16088, rng6 16104, rng0 / statcheck 27639 | warp (at once); roll call, Go Now at 30,656 -> Mission07_NIS05 |
| 07 | 4 / 4 PASS | 16,592 | `3e124280e333a7ff` | byte-identical | IDENTICAL `r10p3g-m07-a`, `r10p1g-m7-a` | **IDENTICAL** | warp (at once); letterbox jump at 16,432 -> Mission08 |
| 08 | 6 / 6 PASS | 29,361 | `f2bcc6c68fbbaa14` | byte-identical | IDENTICAL `r10p3g-m08-a`, `r10p1g-m8-a` | ships_rot 8809, ships_vel 8810, rng10 / rng_all / ships_pos / ships_health / bullets / univcheck 12558 | warp (at once); roll call, Go Now at 29,008 -> Mission09 |
| 09 | 6 / 6 PASS | 20,617 | `985c37906f443d3c` | byte-identical | IDENTICAL `r10p3g-m09-a`, `r10p1g-m9-a` | ships_rot 3242, rng10 / rng_all / ships_health / bullets 3262, rng0 3273, ships_vel 3276, ships_pos 3279, univcheck 3290, ships_n / ships_misc 3307, rng6 17691, missiles 18738, players 20330 | warp; roll call under fire, **Emergency Hyperspace** at 20,224 -> Mission10_NIS06 |
| 10 | 5 / 5 PASS | 21,939 | `b2b64a1d6a268171` | byte-identical | IDENTICAL `r10p3g-m10-a`; `r10p1g-m10-a` at 11118 `rng_all` only | ships_rot 1, ships_pos / ships_vel 358, univcheck 359, rng_all 6695, rng10 / bullets 9582, ships_health 9599, ships_n / ships_misc / players 9720, rng6 9733, rng0 9745, statcheck 12124, missiles 15699, resources 15728 | warp; roll call, Go Now at 21,584 -> Mission11 |
| 11 | 5 / 5 PASS | 8,275 | `a7639dc528811bb7` | byte-identical | IDENTICAL `r10p3g-m11-a`, `r10p1g-m11-a` | ships_rot 2, ships_vel 3, ships_pos 8, univcheck 20, bullets 45, rng10 / rng_all / ships_misc 104, ships_health 1841, ships_n 2355, rng0 2993, rng6 5200, players 5224 | warp; roll call, Go Now at 7,920 -> Mission12_NIS07 |
| 12 | 3 / 3 PASS | 10,995 | `5ff39c8ffab9379b` | byte-identical | IDENTICAL `r10p3g-m12-a`, `r10p1g-m12-a` | ships_rot 1, ships_vel 4, ships_pos 6365; nothing else | warp; roll call, Go Now at 10,640 -> Mission13 |
| 13 | 3 / 3 PASS | 19,283 | `08b09d8a9ec393bb` | byte-identical | IDENTICAL `r10p3g-m13-a`, `r10p1g-m13-a` | ships_rot 2083, rng10 / rng_all / ships_pos / ships_vel / bullets 2105, univcheck 2151, ships_health 2160, missiles 2287, ships_misc 2425, rng0 2992, ships_n 3003, rng6 3022, resources 4435, players / statcheck 8358 | warp; roll call, Go Now at 18,928 -> Mission14_NIS08 (R9: 21,392) |
| 14 | 4 / 4 PASS | 22,338 | `de1be2f4149b7d82` | byte-identical | IDENTICAL `r10p3g-m14-a`; `r10p1g-m14-a` at 10111 `rng_all` only | rng_all 10111, rng10 / bullets 10146, ships_rot 10168, ships_vel 10172, ships_pos / univcheck 10178, missiles 10215, ships_health 10513, ships_misc 12479, ships_n 13219, rng6 13221, players / statcheck 13417, resources 13421, rng0 18815 | warp; roll call, Go Now at 21,984 -> Mission15_NIS09 |
| 15 | 3 / 3 PASS | 15,424 | `aa14f9db5b467ff2` | byte-identical | IDENTICAL `r10p3g-m15-a`, `r10p1g-m15-a` | ships_rot 6634, ships_vel 6638, ships_pos 8938, bullets 9134, missiles 9170; nothing else | warp; letterbox jump at 15,264 -> Mission16_NIS10 |
| 16 | 6 / 6 PASS | 21,523 | `2139819be3a5bd13` | byte-identical | IDENTICAL `r10p3g-m16-a`, `r10p1g-m16-a` | ships_pos / ships_vel 1874, ships_rot / univcheck 1875, bullets 2158, rng10 / rng_all 2235, ships_health 2386, ships_misc 3503, rng6 6944, players 7105, resources 7904, ships_n 10112, missiles 12554, rng0 14968 | warp; roll call, Go Now at 21,168 -> Mission17_NIS11 |
| 17 | 4 / 4 PASS | 15,584 | `d3aabd18f67ae6cb` | byte-identical | IDENTICAL `r10p3g-m17-a`, `r10p1g-m17-a` | ships_rot 2, ships_vel 3, ships_pos 15, bullets 78, rng10 / rng_all 108, univcheck 170, ships_health 448, ships_misc 623, ships_n 981, players 1203, rng6 1208, rng0 1405, missiles 2488, resources 2616, statcheck 6585 | warp; `MissionCompleted` at 15,584 -> `singlePlayerMissionCompleteCB` -> the quit to the credits (`game end tick=15584 credits=Credits-GameEnd.cred`); no next level; **0 `FOW.c` reports** (R9: 18) |

Every move against R9 is producer 1's stated flight / combat move (docs/m2-r10-oracle-rung-physics.md section 9's
per-mission list: the first ticks match it field for field) plus the four render-side `rng_all` moves stated by
producers 2 and 3 (02 / 10 / 14 at 11035 / 11118 / 10111 and 03 at 6708 by the texture-registry change reaching
`trailDraw`'s `RANDOM_TRAILS_1` draw; 06 at 22235 by the nebula fix); every stream is IDENTICAL to producer 3's final
`r10p3g-mNN-a` (the content the audits judged), so the mission table is **re-baselined to these hashes** (the content of
producer 3's `d575eb871d4ef80b` .. `fa1b4c657f887752`). **The failed-jump pair** `r10i-fj3-a/b` `7ca867e8435b2f41`
(Mission03 with `--fail-jump`: armed 17472, the fleet back at 17635, the run ends 17667, 5 / 5, PASS) byte-identical and
IDENTICAL to producer 3's `r10p3g-fj3-a`; against `r10i-m3-a` it diverges only after the arming (ships_pos / ships_vel /
univcheck 17634, ships_rot 17636, rng6 / rng_all 17648). **The combat** is as producer 3 left it: scripted away in the
tests (decision 109).

**UBSan kinds left** (the union over the 101 runs; all non-fatal; none is `fescreen`, `PATCH`, `fquant.c`, `AIShip.c`,
`Tutor.c`, `Select.c`, `FOW.c`, `Subtitle.c`, `CommandNetwork.c`, `Queue.c` or `stb_image_write.h`):
- the ETG opcode kinds of GoK's packed effect bytecode (`etgdecision` 1,868 reports, `etgfunctioncall` 1,130,
  `etgconditional` 1,119, `etgbranch` 736, `etgvariablecopy` 166) and `struct particle` (`Particle.c` 7,648, `ETG.c` 794)
  in 80 runs (every Mission01, skirmish, mission, replay, etg, render, hud and sp run), as in R9;
- `BTG.c` misaligned `short` loads (158 reports in 79 runs) and the misaligned `udword` load (33 reports in 33 runs), as
  in R9;
- 21 runs report nothing (the scripted self-tests without a universe, the oracle self-tests, fe, the toggle runs).
**Gone since R9**: `Select.c:1153` / `:1154` (producer 2's `selCircleCompute` port: 0 runs report it, R9's 11 did),
`FOW.c:296-437` (the cell boxes' `CAT-FIX`: 0, R9's two Mission17 runs did), the recorder's packet writes in
`CommandNetwork.c` / `Queue.c` (no record run in this exit test; producer 3's record runs are in its report),
`stb_image_write.h:1263` (R9's one report in the sp run's screenshot writer: 0 in this step's 127 screenshots). **New
since R9**: none.

**Harness facts for the next round**: the chains ran 5 wide under a load of 13-27 with no lost summary (nobody edited
`tools/m2_harness.sh` during this step); the `missiontest` runs take 63-248 s each in the debug build (Mission11 the shortest, Mission15 the longest), the
Tutorial1 leg 42 s, a replay 57 / 29 s (release 9 s), the `sp` loop reaches Mission12 in 1,200 s (Mission09 in 900 s);
the `.hwo` of a mission run 2-7.6 MB; `hwo_compare.py` wants `--from-tick 1` when one side has a tick-0 record (a record
run) and the other has not (a playback), else it prints no verdict; a run's `summary.txt` carries the `animatics:` line
in every mode now, and the per-mission `missiontest:` line carries every objective's created / completed tick.

## The three ports

| # | item | what landed | DONE WHEN | audit | commit |
|---|---|---|---|---|---|
| 1 | the oracle's next rung, physics ([report](m2-r10-oracle-rung-physics.md), [audit](audits/m2-r10-oracle-rung-physics.md)) | **The rung 1922 -> 2823 (10-17) and 1721 -> 2871 (10-18)** on the table default, each step the exe's machine code at the tick the dump and `hwo_invert.py` pointed at. **The shooter of 10-18 tick 1721 named**: the sMothership 24's guns 3 and 6 at the bRecon 41 (a forced-fire experiment and the GOG bullet hash `1ff62819fb2ff28a`); the tree's line of sight saw the mothership's own modules in the way: `collCheckLineOfSight` 0x0043c2e0 skips every ship clamped to the source or the target in the source's blob (`cat_collLosSkipClamped`) with the x87 length and crossing tests, `collCheckRectLine` 0x00439ef0 (no rectangle = a miss, no early "pointing away" miss, the plane distances rounded once). Then, tick by tick: `GenericInterceptorAttack` 0x005b2620's x87 trajectory normalisation (`catInterceptorNormalizeTrajectory`), the KILL dot product, WHIPSTRAFE1's `bulletRange * 0.7f` and break angle, `GenericInterceptorCanFire` 0x005b2120's elapsed time, `aishipGetTrajectoryWithAngleCorrection` 0x00413f10; `collCheckBulletTargetColl` 0x0043cd90's sphere test **without a square root** (`v*v - dsqr < traveldist^2`); the exe's inlined x87 `frandombetween` (`catFrandomBetweenD`: the draw loaded whole, `draw * ((hi - lo) * 2^-32) + lo`, the caller rounding at its store) at the bullet / missile / explosion rocks and `ApplyCareenRotationDirectly` 0x00545a80, the hit's recoil; **the burst cut's double 0.1** in `cat_univShipDeferredUpdate` 0x005447b0 (a 0.9 s burst ends on its 16th tick, the tree's float sum ended it a tick late) and `gunCanShoot` 0x0048a2f0's x87 shape; `aiuFindDistanceSquared` 0x0041a2c0's and `tacticsGetShipsMaxVelocity` 0x00529450's **double returns** at their 35 / 16 exe call sites (the two `CAT-TODO(M2-R10 physics)` closed); **the seven Blobs.c bodies all found modified and ported** (bullets out of the target lists, the `collspheresize` maxima, `collMyBlob` cleared for a dead or satisfied object, the x87 volumes, sort distances and densities: `catBlobSortDistance`, `catBlobVolumeFactor`); the permanent dump's 'T' / 'G' lines. In 10-18 `rng10`, `bullets`, `ships_health`, `ships_n` and `ships_misc` agree with GOG over the whole recording; in 10-17 `rng10` / `ships_n` over all 9,600 ticks and `randSyncErr 0` (R9: 1,918). The next divergences localised: **both a bProcessor of player 1 (ship 48 / ship 49) following the bWorker 44 by `protectShip`'s follow-behind point, whose heading (coordsys) turns to dock (10-17, from 2809) or spins while harvesting at rest (10-18, from 2860) and is not in the comparable fields** (decision 48). 13 IDENTICAL rows found modified, 27 reviews | met: both replay pairs byte-identical, ASan 0; the 10-17 replay IDENTICAL beyond 1922 (through 2822) and the 10-18 beyond 1721 (through 2870); the next divergences localised and owned (M3 oracle physics); `/oracleLibmSqrt` 177 / 118 (the square root itself); every stream's move stated (the report's section 9); the 4,000-tick soak ASan 0, no fatal; the Mission01 stream unchanged; **no mode-B result** (the user's step, not run) | PASS-WITH-FIXES, **0.97** (65/67). Fixes: `RangeToTarget` 0x00439c30 / `RangeToTargetGivenDist` 0x00439c80 return the UNROUNDED difference (the report had called them "as the tree"): two one-line `CAT-TODO(M3 oracle physics)`, not ported (a stream move); `cat_collLosSkipClamped`'s NULL guard marked CAT-FIX; an open item, `cat_gunCanFire` 0x0048a1c0's second-mode threshold, unrounded in the exe, the same owner | `36289a8` |
| 2 | the Mission04 render stop, the animatics and the sp gate ([report](m2-r10-render-animatics-sp.md), [audit](audits/m2-r10-render-animatics-sp.md)) | **The Mission04 stop was GoK's, not the exe's**: GoK's `HW_PTR_64` guards in `meshFree` and `trTextureUnregister` compared the whole handle (a palette index in bits 16 and up) with `TR_RegistrySize`, so the second .. eighth colour schemes of every team-coloured texture were never unregistered, the texture stayed loaded into the next level, and the next level's `meshTextureRegisterAllPlayers` appended into a loaded texture's colour block (each append moved `trColorInfosPointer` one palette further, the lookups read palette bytes as colour infos) until `nPalettes` hit 8 (`RACE_P1/P1IONARRAYFRIGATE/RL0/LOD0/PAGE0`, loaded since Mission03). Fixed at the cause (HWI: every handle but -1 is unregistered, as the exe's `meshFree` 0x004af710); the exe's `trTextureUnregister` 0x005860d0 (the pending slot reset only at the last usage) and `trTextureDelete` 0x005854c0 (the colour-info list freed: HW1 leaked it, 3,488 blocks by Mission05) ported; `trColorsIndexAlloc`'s ninth scheme `CAT-FIX`ed (palette 0 and a warning; the exe and the old release build wrote past the 64-byte block). **The animatics**: `singlePlayerPostInit` 0x005193d0, `singlePlayerGameUpdate` 0x005197d0's white-out (ONE call, 0x0051a01a, not two) and `utyVideoPlay` 0x005a81a0 -> `animAviPlay` (0x00421860's port), as `kasfMissionCompleted` already did; `animHeadlessSkip` (the virtual clock or `/debugScreenshots`) returns after the exe's time-compression reset and tutorial test and logs `animatic: skipped (headless) a=N b=M file=AnimaticNN`; every transition 0 -> 1 .. 17 -> 18 logged headlessly; an attended run logs `animatic: play ...` (the user's check, the report's 2.1). **`Select.c:1153` / `1154`** was an object exactly in the camera plane (w 0, an infinite circle): `selCircleCompute` 0x00506720 ported (the depth test before the rectangle, the radius cases: a mimic's `cat_mimicGetSelectRadius`, a bullet 1.0, the slipgates +200 / +300 / +400, the pixel rounding `catSelScreenX/Y`). **`FOW.c:296-437`**: the INT_MIN cells are NaN positions of Mission17's KAS-team Taiidan heavy interceptors (player 0) attacking a selection that holds themselves (`Init_Mission17`'s 0x6da2da9; `clAttack` never removes the attackers from their targets; a self-target flies to its own position with limitvel 0, a NaN velocity from tick 328); the cube boxes `CAT-FIX`ed to the exe's two's-complement wrap (`fowCellSub` / `fowCellAdd`); the self-target owned by M3. **Mission17's log**: `is_final_tex` decides on the low 32 bits of the 4-byte `-1` terminator (98,718 warnings -> 0); the `memAllocFunctionA` lines are growth-heap notes from a 64 MB main heap 94% full after the load (named now; the exe's heap is 24 MB); GoK converted the sound banks into twice their size (16.4 MB for 8.2 MB): the exact size now (HWI). **The sp gate walks Mission01_NIS01 -> 01 -> 02 -> 03 -> 04_NIS02 -> 04 -> 05 -> 06_NIS04 -> 06 -> 07_NIS05 -> 07 -> 08 -> 09 in 900 s** (94 screenshots, ASan 0, no leak, no fatal). 1 IDENTICAL found modified, 3 MODIFIED found as HW1, 15 reviews (+2 by the audit). No simulation stream moved (every stream IDENTICAL to producer 1's; Mission03's `rng_all` from 6708 by one `RANDOM_TRAILS_1` draw in `trailDraw`, render only, found by producer 3) | met: `sp` through Mission04 and on to its timeout with ASan 0 and no fatal, the screenshots per mission kept; the animatic playing in an attended run is **the user's step** (the headless line at every transition is the agent's evidence); no Select.c / FOW.c report in `sp` or any mission run | PASS-WITH-FIXES, **0.9375** (45/48). Fixes: three wrong exe addresses in `selCircleCompute`'s CAT comments (0x005070b8, 0x00506fa0 / 0x00506fb7, 0x005068af); two review entries for `memAllocFunctionANV` / `memAllocFunctionA` (edited only under `MEM_VERBOSE_LEVEL`, class IDENTICAL kept); an unrelated re-encoding in the review file restored. Observations for M3: the headless skip no longer sets `animaticJustPlayed` (render only); `trRegisterRemoval` / `trTextureColorsUpdate` overwrite the "dupe trcolorinfo list" pointer after `trInternalTexturesDelete` (a likely source of the 2,438 live blocks the soak inherits) | `3ea7b12` |
| 3 | the tests and the oracle's leftovers ([report](m2-r10-tests-leftovers.md), [audit](audits/m2-r10-tests-leftovers.md)) | **Mission06's `rng_all` flake was not the mixer** (no mixer runs in the mission tests: sound and speech are off): the nebula tendrils' uninitialised bounding sphere (`nebInitTendril` 0x004c7d50 never sets `radius` / `midpoint`; `nebRenderNebula` 0x004cadb0's inlined clip test reads it; the exe the same), stream 5 `RANDOM_NEBULAE` on the main thread at tick 2081 when nebula 4's tendrils come into view; **`CAT-FIX`: zero**, what a fresh pool block holds (Mission06 byte-identical over four runs, `d0ac2eeb9a668f8b`; `rng_all` only from 22235 against R9's content). The mission self-tests set the virtual audio clock (`CatMissionTestSet`; a no-op while sound is off, decision 94's default). **The `.pkts` header tail** (+0x1A after `numberOfCommands`, never written in `captainServerTask` 0x00451580's three branches; GOG's files carry heap words there) **and every command's struct padding zeroed** (`CAT-FIX`: the 16 HW1-layout senders `memset` their blocks as the M2-R9 three did): two record runs' `.pkts` byte-identical (`135fc5d57c48c8be`); the playbacks unchanged. **The skirmish test's bigotry** `/catSkirmishBigotry N` (harness `--skirmish-bigotry N`): `opCPUAttacks` 0x004db440 pressed through its callback with a button atom of pData N; 2 sends `ALLIANCE_FORM_NEW_ALLIANCE` for every index pair from the first computer to 7 whether the player exists or not, **21 commands in a 2-player game = GOG's 21 at frame 88** (`cat_aisSetupAlliances` 0x004310a0: no extra computer players needed); the recording with alliances (`hwibig02`, SAME FORMAT) replays as a pair IDENTICAL to its record run; `-- /catVirtualAudio` for a record pair in every field (the skirmish test keeps sound on; stream 12's speech variation draws split `rng_all`). **The failed-jump test** `/catMissionTestFailJump` (harness `--fail-jump`): `hyperspaceFails` is written 1 only by `kasfDisablePlayerHyperspace` 0x004935e0, reachable only through the console's KAS call (`cat_kasCallFunctionByName` 0x0045b9a0; no mission script calls it), so the test makes that call at its first hyperspace command; the jump fizzles and `singlePlayerGameUpdate`'s case 10 (`HYPERSPACE_CATFAILEDRETURN`) returns the fleet, judged at the return (no hyperspace, the flag clear, the same level, the mothership outside, none of the player's ships in the ether, none lost); the run ends 32 ticks later (case 10 does not give `playerCanHyperspace` back: no second jump). Mission03 in the gates (pair `4c6b1299c1b6b9ba`, armed 17472, the return 17635; Mission11 / 04 are lost by their scripts after a failed jump, 12 keeps ten level ships in the ether). **The combat measured and deferred** (M3 test): the targets are fine; the player's interceptors sit 30,000-54,000 units from the raiding bombers; R9's experiment re-issued the order every 64 ticks (each re-pick among 14 targets); one held order kills 2 of 14 in 3,200 ticks and loses 3 ships; `ShipsDamage` stays (decision 77). **The review sample**: `scriptSetTweakableGlobals` 0x00527820 (the tree's) and 22 of the 143 unflagged rows: 4 ported (`parseLine` keeps `//` after `http:`: the three URLs had loaded as `http:`; `cloudZapLoop`'s x87 shape; `selCentrePointComputeGeneral`'s missile scale `catMissileScale` +0x254 and sum, the old CAT-PROVISIONAL retired; `collCheckShipDerelictColl`'s clamp skips and distance shape), `cloudUpdateSystem` 0x00439130 tagged `CAT-TODO(M3 oracle physics)`, `trMeshSortAdd` found IDENTICAL; 143 -> 120 unflagged rows outside `TitanInterface.cpp`; the strings test checks the three URLs. The 17 mission streams re-baselined once. 23 reviews (+15 by the audit) | met: Mission06 byte-identical in every field over 4 runs; the failed-jump flow reached under a test, a pair; the 17 mission streams re-baselined once with the stated fields (only Mission06's `rng_all` moved by this step; the virtual clock moves none, no mixer runs); the bigotry flag documented with what it carries; the `.pkts` tail zeroed; the Mission01 stream IDENTICAL | PASS-WITH-FIXES, **0.864** (38/44). Fixes: two wrong descriptions of the failed-jump test (no second jump), 15 review entries for CAT-FIX edits of IDENTICAL bodies (`nebInitTendril`, `captainServerTask`, 13 senders), `clPktCommandAlloc`'s sender list (16, not 15), `captainServerTask`'s comment counts, `collCheckShipDerelictColl`'s NULL guard marked CAT-FIX | `f9590bc` |

**Round size** (`git diff --stat 29f7c0d..f9590bc`, the round base to the last audit's checkpoint): 52 tracked files changed,
+3,368 / −445 lines; `src/` alone: 40 files, +939 / −242, with **108 added lines carrying `CAT`**; no new or deleted `src/`
file. The tree now carries 4,459 `CAT:` lines (R9: 4,422), 35 lines carrying `CAT-TODO` (34; **26 owner tags: stage 5 23**
(plain 8, multiplayer options 8, LAN 6, multiplayer 1) **and M3 oracle physics 3** (`RangeToTarget`, `RangeToTargetGivenDist`,
`cloudUpdateSystem`); 9 other lines mention closed tags in prose), **4 `CAT-PROVISIONAL`** (5: `selCentrePointComputeGeneral`'s
retired), 13 `CAT-DORMANT` (13), **194 `CAT-FIX`** (166), 49 `HWI-ORACLE` (49), 441 `HWI-FEATURE(motion-interp)`, **143
`HWI-TEST`** (132), 27 `HWI-FIX` (27), 6 `HWI-EXPERIMENT` (6). Tools: `tools/m2_harness.sh` (`--fail-jump`,
`--skirmish-bigotry N`, the `animatics:` summary line, the 'T' / 'G' dump lines in the header; still 29 modes); no new tool.
`runs.txt` logs the producers' and auditors' 393 harness runs this round (lines 2401-2793) before this step's.

## Delta map after the round

The ports recorded their comparisons as reviews in `ghidra/delta_map_review.json` and merged them with `tools/delta_map.py
merge` (decision 55; the audits appended their own rows: 0, 2 and 15, and the render audit regenerated the map after its
address fixes). Notes tagged `[review: M2-R10 <port>]` in the map: oracle-rung-physics 27, render-animatics-sp 15 (+2
audit), tests-leftovers 23 (+15 audit). This step added no review of its own and ran no merge; `python3 -I
tools/m2_stage_counts.py --md` on the tree's map gives the table.

| class | M2-R9 end | M2-R10 end | M2-R10 bytes |
|---|---:|---:|---:|
| IDENTICAL | 2,773 | **2,763** | 490,853 |
| MODIFIED | 1,564 | **1,574** | 732,139 |
| REWRITTEN | 83 | **83** | 15,855 |
| hand-reviewed (`reviewed` rows) | 3,512 | **3,569** | |

What moved: **14 functions the map called IDENTICAL were found modified** by the ports (the rung 13: `collCheckLineOfSight`,
`collCheckRectLine`, `aishipGetTrajectoryWithAngleCorrection`, `ApplyCareenRotationDirectly`, `aitTeamIsFinishedMoving`,
`aimProcessMoveTo`, `FillInCollisionBlobSpecificLists`, `bobListCreate`, `bobListUpdate`, `bobBlobCombine`,
`bobObjectListMedian`, `AddObjToObjectListsOfBlob`, `bobObjectDied`; the render port 1: `trTextureDelete`), and **4 MODIFIED
rows were found to be HW1's body** (`trRegisterAddition`, `trRegisterRemoval`, `trTextureColorsUpdate`: the auto-class came
from the inlined helpers; `trMeshSortAdd`): 2,773 − 14 + 4 = 2,763. Every edited exe function the map knows carries a
review row, including the CAT-FIX edits of IDENTICAL bodies (`nebInitTendril`, `captainServerTask`, the 13 command
senders, `memAllocFunctionA` / `ANV`: class IDENTICAL kept, the audits' entries). Across the whole map **212 MODIFIED /
REWRITTEN rows still carry no `reviewed` flag** (R9: 240), **120 outside `TitanInterface.cpp`** (148): `ETG.c` 41,
`Particle.c` 38 (mostly the argument-array convention: every one of the 8 sampled this round was the tree's already),
`ObjTypes.c` 16, `MultiplayerLANGame.c` 7, `MultiplayerGame.c` 5, `GameChat.c` 3, `StatScript.c` 3, `texreg.c` 2, 1 each in
`Captaincy.c`, `CommandNetwork.c`, `File.c`, `TitanNet.c` and `ddraw.cpp` ("What is left" 9).

Per stage (`python3 -I tools/m2_stage_counts.py --md`): HW1-matched IDENTICAL / MODIFIED / REWRITTEN functions (MODIFIED +
REWRITTEN bytes), then cat-new by the tool's rules.

| stage | M2-R9 end | M2-R10 end | cat-new |
|---|---|---|---|
| 1 | 207 / 108 / 0 (55,860 B) | **210 / 105 / 0 (54,159 B)** (`trRegisterAddition` / `trRegisterRemoval` / `trTextureColorsUpdate` found HW1's; `trMeshSortAdd` IDENTICAL; `trTextureDelete` found modified; `parseLine` ported) | 76 / 27,383 B |
| 2 | 734 / 638 / 30 (261,490 + 4,444 B) | **724 / 648 / 30 (267,702 + 4,444 B)** (the rung's collision / interceptor / blob bodies found modified; `selCircleCompute`, `cloudZapLoop`, `selCentrePointComputeGeneral`, `collCheckShipDerelictColl` ported) | 808 / 180,868 B |
| 3 | 846 / 338 / 14 (200,979 + 8,558 B) | **846 / 338 / 14 (200,979 + 8,558 B)** (unchanged) | 450 / 139,196 B |
| 4 | 294 / 183 / 2 (79,206 + 169 B) | **291 / 186 / 2 (79,829 + 169 B)** (`aishipGetTrajectoryWithAngleCorrection`, `aitTeamIsFinishedMoving`, `aimProcessMoveTo`'s unrounded uses) | 23 / 14,404 B |
| 5 | 404 / 116 / 12 (76,194 + 1,179 B) | **404 / 116 / 12 (76,194 + 1,179 B)** (unchanged; the senders' CAT-FIX keeps IDENTICAL) | 67 / 17,483 B |
| 6 | 288 / 181 / 25 (53,276 + 1,505 B) | **288 / 181 / 25 (53,276 + 1,505 B)** (unchanged; the 2,085 kas2c mission functions all generated and defined, decision 87) | 2,365 / 656,918 B |

## Oracle mode A

The rung after the round, from this step's clean build (the table square root, the exe's qsort, the round's flight / combat
ports; the GOG streams `original/oracle/oracle01-rec-20261017-140428/oracle01-rec.hwo` (9,601 ticks) and
`oracle01-rec-20261018-102253/oracle01-rec.hwo` (3,527 ticks); the task's field list
`rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real`
from tick 1; decision 47's exclusions `ships_rot` (1), `rng_all` / `rng0` (2 / 3: the DLL splits stream 0), `fpcw`,
`players` (93 / 272), `statcheck` (93 / 272), `rng6` (161 in both: the playback skips the AI), decision 60's `real` (tick 2)
for 10-18):

| recording | first divergent tick, the default | the fields after it | with `/oracleLibmSqrt` | R9 | R8 / R7 |
|---|---|---|---|---|---|
| 10-17 (`r10i-r17-a` = `-b` `9af78e15f5234902`, 2,630,926 B, 9,601 ticks, 4,800 packets, 80 ships) | **2823 `ships_vel`** (IDENTICAL through 2822; ship 48) | ships_pos 3251, univcheck 3335, bullets 5479, resources 5885, ships_misc 6281, ships_health 7593; **rng10, ships_n, missiles, derelicts, dt, total, real never in 1 .. 9600** | 177 `ships_vel` (then ships_pos 180, univcheck 190, resources / rng10 1408, bullets 1917, ships_health 2043, ships_misc 2321, ships_n 5559) | 1922 `ships_vel` | 247 / 177 |
| 10-18 (`r10i-r18-a` = `-b` `8112853eacffc80a`, 966,649 B, 3,527 ticks, 1,763 packets, 55 ships) | **2871 `ships_vel`** (IDENTICAL through 2870; ship 49) | ships_pos 2909, univcheck 3062; **rng10, ships_n, ships_misc, ships_health, missiles, bullets, derelicts, resources, dt, total never in 1 .. 3526** | 118 `ships_pos` + `ships_vel` (then univcheck 137, resources / rng10 944, bullets 1721, ships_health 1861, ships_misc / ships_n 1955) | 1721 `bullets` | 155 / 118 |

Every tick equals docs/m2-r10-oracle-rung-physics.md (sections 6 and 7, its table at line 226) and
docs/audits/m2-r10-oracle-rung-physics.md (its table at line 37); the sync counters are the producer's
(10-17 `randSyncErr=0 univSyncErr=2911 blobSyncErr=4746 pktSyncErr=0`, 10-18 `0 / 4 / 1619 / 0`; with libm 3347 / 4693 and
781 / 1658; R9's default: 1918 / 3767 and 880 / 875). Both replays are IDENTICAL in every field to the rung producer's
`r10p1g-r17-a` / `r10p1g-r18-a`, to the rung audit's `r10a1-r17-a` and to the leftovers producer's `r10p3g-r17` / `-r18`;
the libm replays to `r10p1g-r17-libm` / `-r18-libm`: nothing after the rung port (the render, the animatics, the tests,
the review ports, the nebula fix) touched the replayed physics. The release replay `r10i-r17-rel` (`60de70ccef41fedc`)
equals the debug one in every field but `rng_all` (from 6377, as R9's release replay against its debug one: a
render-side stream). Against R9's replays (`r9i-r17-a` / `r9i-r18-a`), this build's streams move at exactly the old divergences:
10-17 `ships_rot` 1921, `ships_vel` 1922, ships_pos 1954, bullets 2005, univcheck 2020, rng10 / rng_all / ships_health 2043,
ships_misc 5885, ships_n / players / statcheck 5890; 10-18 `bullets` / `rng_all` 1721, ships_rot 1743, rng0 / rng10 /
ships_pos / ships_vel / ships_health / univcheck 1749, ships_n / ships_misc / players 1753, rng6 1769, statcheck 1805:
the port removed R9's two divergences and nothing before them.

**What the two divergences are now** (producer 1's localisation with `hwo_invert.py` and the dump's 'T' / 'G' lines,
sections 7.1 / 7.2; the audit's reading): **the same mechanism in both**, a **bProcessor of player 1 under PROTECT**
(ship 48 at 10-17 tick 2823, ship 49 at 10-18 tick 2871; `ships_vel` 3-7 raw bits off, every other ship equal) flying
`protectShip` 0x004411c0's follow-behind point `P - heading * 1.5 * desiredrange` of the **bWorker 44**, whose
**heading** (coordsys column 3) is the input the stream cannot see (decision 48: `ships_rot` excluded): at 10-17 the
Worker turns to dock from 2809 (order DOCK, a heading torque of -4,404.7, then the 2.0 rad/s cap); at 10-18 it harvests
at rest and spins from 2860 (torques -9,600 / -3,949.8). The offline replica of the start-flying target velocity
reproduces ours exactly and GOG's with the destination one to five ulps off, so the candidate bodies are the dock
flight's heading tracking (`aitrackHeadingFunc` 0x00418de0, `cat_dockShipDocksAtShip` 0x00465860) and the harvest
turn's torques (the Cataclysm harvest path), read against the exe at 2809 .. 2822 / 2860 .. 2870; what confirms it at
once is **decision 48's fourth recording** (the DLL's `CAT_SHIP_coordsys` 0x98 edit makes `ships_rot` comparable and
shows the Worker's coordsys directly). Owner: **M3 oracle physics** (the M3 plan's row 3; an M2-R11 physics round only
if the user wants the rung before the play-test). Also owned there (`CAT-TODO(M3 oracle physics)`, 3 lines): the
unrounded returns of `RangeToTarget` 0x00439c30 / `RangeToTargetGivenDist` 0x00439c80 (the audit's finding F1: the exe
returns the difference unrounded on the FPU and several of their 25 / 16 call sites compare it unrounded, the port's
`real32` return rounds it; no replay stream shows it yet), `cat_gunCanFire`'s second-mode threshold,
`cloudUpdateSystem` 0x00439130's x87 shape (producer 3's review), and the inlined `frandom` sites not yet read
(decision 99).

**What the round's physics ports were, in the order they moved the rung** (producer 1's section 4, x1 .. x8): the
shooter of the old 10-18 1721 named by the GOG bullet hash (the sMothership 24's guns 3 and 6 at the bRecon 41: a
**line-of-sight** test that skipped every ship clamped to the source or the target in the source's blob,
`cat_collLosSkipClamped`, and `collCheckRectLine` without rectangle information as a miss) and `GenericInterceptorAttack`'s
x87 normalisation (`catInterceptorNormalizeTrajectory`) moved 10-17 to 1796; the bullet-target sphere test
`collCheckBulletTargetColl` 0x0043cd90 (no square root) to 2823; the inlined x87 `frandombetween` of the hit rocks /
careen (`catFrandomBetweenD`) moved 10-18 to 1854; the burst cut's double 0.1 and `gunCanShoot` 0x0048a2f0's shape to
2871; `tacticsGetShipsMaxVelocity` / `aiuFindDistanceSquared`'s double returns and the seven Blobs.c bodies left both
streams byte-identical (shape-only ports). Section 6's complete field table stands.

**The mode-B rung** (the GOG playback of `hwirec01` / `hwirec02` with the DLL) is **still the user's step and still not
run**: no `original/oracle/hwirec*-play-*` directory exists (`ls` this step), the install's `SavedGames/RecordedGames/`
holds only `oracle01` (the 10-19 recording, 1,181,732 B) and the INVALID campaign one; **no fourth recording** either
(the newest `original/oracle/oracle01-rec-*` is `20261019-143530`, the one the DLL made before the coordsys edit). The
recordings themselves are unchanged since R9 (`hwirec01` `4640546c0708c284` / `.pkts` `e852ee9f3a457e3b`, `hwirec02`
`13a17b9f3a2d8664` / `d24604387e083cf5`), and their hwi-engine playbacks from this build are pairs: `r10i-play01-a/b`
`4cd0ef18b5c7bc8c` (4,801 ticks, 2,400 packets, 57 ships, 0 sync errors of every kind) **IDENTICAL in every field to R9's
`r9i-play01-a`** and to its record run `r9r-rec01` (ticks 1 .. 4800 on every field but `rng0` / `rng_all` from 2 and `rng6`
from 321: the DLL-split stream and the computer player that does not run in a playback, as in R9); `r10i-play02-a/b`
`35a2bf7816623808` (71 ships, `randSyncErr=1909 univSyncErr=1984 blobSyncErr=11`) IDENTICAL to producer 1's
`r10p1g-play02-a` and producer 3's `r10p3g-play02-a` and **moved against R9's `r9i-play02-a` (and its record run
`r9r-rec02`) from tick 706 `ships_rot`, 709 `ships_vel`, 831 rng10 / rng_all / ships_pos / ships_health / bullets /
univcheck, 1214 ships_n / ships_misc / statcheck / players, 1800 rng6, 2976 resources, 3840 derelicts**: the flight /
combat ports, exactly as producer 1 stated (section 9), so the sync counters of that playback are expected (the
recording's checksums are R9's physics) and the right-hand side of the user's mode-B comparison for `hwirec02` is
**this step's `r10i-play02-a`**, not R9's run ("What the user must decide" 2).

**The stream moves of the round, summed** (every guard-rail against R9's, the table above): the headless Mission01 stream
did not move (IDENTICAL to `553845b65e87f3b5` in every field, debug and release, the three switches byte-identical: no
ship flies there, decision 57), nor ai 400, both skirmishes, build, the `/noInterp` dumps and `hwirec01`'s playback; every
stream where ships fly and fight moved by the flight / combat ports exactly as producer 1 stated (the soak from 431, the
missiontest from 1772, the loadgame from 1985, the tutorial leg from 4753, the lesson load from 8360, the warp start's
`ships_rot` at 1 only, `hwirec02`'s playback from 706, missions 02 / 04-06 / 08-17 at producer 1's ticks), plus two
render-only `rng_all` moves of the later ports (Mission06 from 22235 by producer 3's nebula fix; Mission03 from 6708 and
Mission02 / 10 / 14 from 11035 / 11118 / 10111 by producer 2's texture-registry change reaching the trail drawing
(`RANDOM_TRAILS_1`), stated by producers 2 and 3). **No re-baseline of decision 44** (the Mission01 baseline stands); the 16 moved mission streams and the
guard rails are re-baselined to this step's hashes in the tables above (decisions 49 / 82 updated).

## The campaign from the NIS-first start

What the player gets now, mission by mission, in the order the exe gives it (every step from the three reports, exercised
headlessly by the per-mission `missiontest` runs of the exit test above; the entry into each mission N >= 2 in those runs
is the exe's warp start with Warp.script's level-N record, decision 86, while the chained path Mission01 -> 02 -> ... ->
Mission12 is what `sp`'s screenshot loop walks in 1,200 s; each self-test ends 32 ticks after the NEXT level has loaded,
so every transition's level load is exercised too; the ticks that moved since R9 moved by the flight / combat ports,
the objectives and their order did not):

1. **The start and Mission01** ("Taidan Contras Mount Attack On Higaran Space"), unchanged since R8: New Game, the
   shuffled start positions, the opening NIS `n01` in its staging level (stopped at 1771), `kasMissionStart("mission01")`,
   the nislet (to 1713), the fleet's arrival (1939), the six objectives in order (DestroyBombers, ExploreRegion,
   DestroyResourcers, DestroyProxAttackers, DestroyDestroyer, Hyperspace: created / completed at 4768 / 4800, 6864 / 8096,
   8208 / 8752, 9760 / 10240, 10816 / 11296, 11328, the jump at 11392, `done ticks=11424 objectives=6/6`), the KAS saves
   "01 - Hiigara" / "01 - ~ End of Mission", the letterbox jump (a `catSpMissionNextLevel` mission). **New this round**:
   the animatic `(0, 1)` before Mission01 and `(1, 2)` after it are **routed to `animAviPlay`** (the exe's player at
   0x00421860; the headless runs log `animatic: skipped (headless) a=0 b=1 file=Animatic01` and skip the playback, an
   attended run plays the AVI: the brief's first check), and the headless Mission01 stream is the one of R9 (decision 44).
2. **Mission02** ("Rescue Kith Destroyer And Find Pod"; a letterbox-jump mission): the fleet rolls in (G_ShipsHaveArrived
   at tick 213 of the test), the hangar-module objective is destroyed by the script at once (224); **FindDestroyer** (a
   recon to the Kith destroyer, 304 / 2352), **Build** (the sMothershipMicro, 768 / 2352), **RepairDestroyer** (TECH_REPAIR
   / TECH_SALVAGE, the workers' upgrade and the repair special on the destroyer: 2352 / 13152), **SalvagePod** (the
   salvage special on the pod, 14592 / 25248), **DestroyPirates** (the raiders, 15728 / 19552: 2,256 ticks sooner than
   R9's 21808, the fight's new timing), Hyperspace (25712): the letterbox jump at 25776 -> Mission03 (`done ticks=25936
   objectives=6/6`; R9: 25136). Mission02's meteor storm (R8) and its two slipgate lines (the Slipgate reader) are in the
   level; the chained path plays Animatic03 after it.
3. **Mission03** ("Rescue Ecclesiam From Pirate Blockade"): **BuildHangarModule** (288 / 1744), **Find** (2144 / 7168),
   **Mimic** (the mimic's disguise through `cat_mrMimicTarget`, 7168 / 8624), **Lure** (the mines and the ships around the
   Cleesan, 8624 / 17184), Hyperspace (17408): **the roll-call screen** comes up at 17472 (22 ships still out, 2 docked)
   and **Go Now** is clicked at 17536 -> the staging level Mission04_NIS02 with `n02.nis` (`done 17825, 5/5`; the ticks
   unchanged since R9, the stream too in every simulation field). **The failed jump** (new this round, `--fail-jump`): the
   same mission with `DisablePlayerHyperspace` armed through the console's KAS call at the first hyperspace command
   (17472) fizzles the jump, **returns the fleet at 17635** (case 10 `HYPERSPACE_CATFAILEDRETURN`: the mothership outside,
   32 player ships, the level the same, `playerCanHyperspace` 0 after it) and the run ends at 17667 (`r10i-fj3-a/b`,
   a pair, IDENTICAL to producer 3's).
4. **Mission04** ("(Empty)"; an at-once arrival mission, `catSpMissionArrivesAtOnce` 4 .. 8: G_ShipsHaveArrived at tick
   35): **Replace** (the docking bay, 256 / 1712), **Escort** (the ACVs / acolytes under guard, 1712 / 7424), **Research**
   (TECH_RESEARCH_CLEESAN_ATTACK, 10736 / 14592), **Destroy** (a held objective: the script never ends it; it completes
   with the Hyperspace message's `ObjectivesDestroyAll` at 20400), Hyperspace: the roll call at 20464 (5 out), Go Now at
   20528 -> Mission05 (`20818, 5/5`). **The render's stop is gone**: the `sp` run no longer asserts on `texreg.c (569)`
   when `RACE_P1/P1IONARRAYFRIGATE/RL0/LOD0/PAGE0` gets its ninth colour scheme (GoK's 64-bit handle guards had refused
   every `trTextureUnregister` of a palette index above 0, so no scheme was ever freed; the exe's `trTextureUnregister` /
   `trTextureDelete` ported, `trColorsIndexAlloc`'s ninth scheme a `CAT-FIX` with a warning); 13 screenshots of it.
5. **Mission05** ("Beast Attacks Bentusi, Station Scuttled"; at once; letterbox jump): **InterceptBentusi** (a recon,
   608 / 1536), **SaveBentusi** (2496 / 11952), **ProtectCaalShto** (8464 / 12480), **DestroyBeast** (12416 / 12480),
   **BuildEngineering** (the sMothershipSpecial, 13344 / 15520), Hyperspace (16160): the letterbox jump at 16224 ->
   Mission06_NIS04 with `n04.nis` (`16384, 6/6`).
6. **Mission06** ("Escourt Callisto Into Nebulae"; at once): **Explore** (the station, 608 / 2016), **Scout** (Nav1 ..
   Nav4, 2672 / 3520), **Escort** (the Caal-Shto guarded, 3632 / 30576), **Mines** (4608 / 4656), **HvCruiser** (the heavy
   cruiser group found and brought to 45% hull, 26672 / 27648), **Leech** (the Leech scene, 27200 / 28176), Hyperspace
   (30592): the roll call at 30656 (40 out), Go Now at 30720 -> Mission07_NIS05 with `n05.nis` (`31011, 7/7`). (The "Kill"
   objective of its script is unreachable: its message is never sent.) **Its stream is a pair now** (byte-identical over
   this step's two runs and producer 3's four): the nebula tendrils' uninitialised bounding sphere was the `rng_all`
   flake (decision 105), not the mixer.
7. **Mission07** ("Rescue Convoy"; at once; letterbox jump): **Mayday** (a recon to the convoy, 1584 / 2304), **Protect**
   (4464 / 16368) and **Destroy** (the infected ships, 5696 / 16368), Hyperspace (16368): the letterbox jump at 16432 ->
   Mission08 (`16592, 4/4`; IDENTICAL to R9's stream in every field: nothing fights there that the ports changed).
8. **Mission08** ("Rendezvous With Spy"; at once): **Resource** (two workers harvesting, 208 / 4032), **Protect** (the
   spy's pursuers, 4032 / 4992), **CaptureFrigate** (a salvage-upgraded worker pair on a frigate damaged to 40%, 8752 /
   18800), **CaptureResourcers** (three collectors, 8752 / 28720), **NoSurvivors** (8752 / 28736), Hyperspace (28944):
   the roll call at 29008 (49 out, 16 docked), Go Now at 29072 -> Mission09 (`29361, 6/6`; the same ticks as R9 though
   the stream moved from 8809).
9. **Mission09** ("Retrieve Info From Taidan Research Planet"): **MoveTransports** (224 / 2928), **DestroySensor** (3456 /
   3952), **Wait** (3952 / 9728), **Engage** (9728 / 16320), **Protect** (held: 16320), Hyperspace (20160) **under fire**:
   the roll call at 20224 (64 out, 21 docked) offers **Emergency Hyperspace**, which is clicked at 20288 ->
   Mission10_NIS06 with `n06.nis` (`20617, 6/6`; R9: 20579).
10. **Mission10** ("(Empty)"): **SALVAGE_BIGGUN** (160 / 7152), **RESEARCH_BIGGUN** (7152 / 10048), **BUILD_BIGGUN** (10048
    / 14064), **FIRE_BIG_GUN** (the Siege Cannon fired, 18592 / 20496), HYPERSPACE (21520): the roll call at 21584 (37
    out, 26 docked), Go Now at 21648 -> Mission11 (`21939, 5/5`); the pirates' escort waves; 4 mines in the level.
11. **Mission11** ("The Factorem Attacks"): **DESTROY_ATTACKERS** (1600 / 1888), **REPAIR_FACTORUM** (the Bentusi
    repaired, 3008 / 4544), **DESTROY_BEAST** and **PROTECT_FACTORUM** (held, 5248), Hyperspace (7856): the roll call at
    7920 (61 out, 11 docked), Go Now at 7984 -> Mission12_NIS07 with `n07.nis` (`8275, 5/5`).
12. **Mission12** ("Negotiations With Pirates And Help From Contras"): **DefendMothership** and **AttackOutpost** (3216 /
    7600), Hyperspace (10576): the roll call at 10640 (74 out, 34 docked), Go Now at 10704 -> Mission13 (`10995, 3/3`); 26
    mines. (WorkerDock / WorkerReturn are unreachable: the SecurityTeam that would send their messages stays empty.) The
    `sp` loop reaches it in 1,200 s (one screenshot before the timeout).
13. **Mission13** ("Knocked From Hyperspace"): **DESTROY_ENEMY** (3008 / 17056: R9's 19488, the fight 2,432 ticks
    shorter), **PROTECT_FAALCORUM** (17008 / 17056), Hyperspace (18864): the roll call at 18928 (16 out, 22 docked), Go
    Now at 18992 -> Mission14_NIS08 with `n08.nis` (`19283, 3/3`; R9: 21746).
14. **Mission14** ("(blank)"): **GET_SAMPLE** (the mimic's phases: the undisguised escort ring, the mimic out of view, the
    detectors near the disguised mimic destroyed; through `cat_shipStealthDetectionUpdate`'s detectors: 4192 / 10080),
    **PROTECT_MOTHERSHIP** (held, 4192), **CAPTURE_TAIIDAN** (the frigate captured by salvage-upgraded workers; held,
    13264), Hyperspace (21920): the roll call at 21984 (14 out, 32 docked), Go Now at 22048 -> Mission15_NIS09 with
    `n09.nis` (`22338, 4/4`); 32 mines and 6 slipgates; the fleet arrives at 378 / 372 support units (small craft scuttled
    to free them).
15. **Mission15** ("Convince Bentusi To Help"; letterbox jump): **DestroyGateNode** (352 / 6192), **Block** (the Bentusi's
    slipgate block, 2096 / 6192), Hyperspace (15200): the letterbox jump at 15264 -> Mission16_NIS10 with `n10.nis`
    (`15424, 3/3`); 2 slipgates. (DefendMothership is unreachable: its selection is never filled.)
16. **Mission16** ("(Empty)"): **LOCATE_CS** (240 / 1872), **CRIPPLE_CS** (1936 / 5360), **DOCKWITH_CS** (5872 / 12816),
    **HARVEST** (12816 / 20736), **DESTROY_BEAST** (20736 / 21088), HYPERSPACE (21104): the roll call at 21168 (71 out,
    28 docked), Go Now at 21232 -> Mission17_NIS11 with `n11.nis` (`21523, 6/6`); the big gun, the mimic docking, the
    Beast.
17. **Mission17** ("(Empty)"): **DestroyNaggarok** (the Naggarok: 144 / 15152), **DestroyNomadMoon** (4208 / 13200),
    **DestroyEmitters** (5248 / 5744), **Recon** (8592 / 12736); then the script's `MissionCompleted` ends the game: Fleet
    Command's completion, `quittime` and `utyCreditsSequence`, **the credits `Credits-GameEnd.cred`** (`game end
    tick=15584`, 4 / 4; the stream moved from tick 2 by the ports, the objectives' ticks did not). ("Research" is
    unreachable: its message is never sent.) **Mission17's log** is quiet now: 0 `is_final_tex` lines (R9: 98,718), the
    `FOW.c` reports gone, the `memAllocFunctionA` growth-heap notes named by block (decision 104).
18. **The tutorial**: the front end's Tutorial starts the generated Tutorial1; **all 12 lessons** run through their
    FSMs with the game's own callbacks (LESSON 01 Start of Tutorial / ClickHere, 02 Camera, 03 Selection, 04 Focusing, 05
    Sensors Manager, 06 Moving and Waypoints, 07 Basic Attacking, 08 Building, 09 Harvesting, Repairing, and Salvaging, 10
    Advanced Attacking, Formations, and Tactics, 11 Researching and Upgrading, 12 Other Features of Interest), the
    narration on the virtual audio clock in the test (its game-time length: Lesson1 complete at 1108, Lesson12 at 10348,
    `lessons=12/12 complete=12/12 inOrder=1 nextClicks=11`, `done ticks=10444`: the same ticks as R9 though Lesson 07's
    attack moved the stream from 4753), the 12 lesson saves written by the engine and "LESSON 11 - Researching and
    Upgrading" loading back through the tutorial load (`loadgame --training`: `from=8361 to=11961 objectives=1/1`).

**The animatics** (decision 102): the twelve `AnimaticNN` of the chained campaign are reached at every transition the
`sp` loop walked (0->1 before Mission01, 1->2 .. 11->12 after each mission's jump; `animatics: 12 skipped (headless)`)
and `kasfMissionCompleted`'s game-end one is on the same path; the headless runs skip the playback after the exe's
time-compression reset and tutorial test, so what an attended run shows (the AVI's frames, its sound, the Escape skip)
is the user's check in the brief. **Known gaps** (unchanged in kind): the player's combat is scripted away in the
headless tests (decision 77 / 109: the attended play-test measures the real fight); Mission17's KAS Taiidan teams
attack a selection that holds themselves (NaN flights from tick 328; whether the exe does the same is M3's question,
decision 103); the chained campaign's growth heaps by Mission08 (the M3 soak's question, decision 104); the three
unreachable objectives (Mission06 "Kill", Mission12 WorkerDock / WorkerReturn, Mission15 DefendMothership, Mission17
"Research") are the scripts' own.

## What is left of stages 3-6

Nothing blocks the play-test; each item has an owner (M3, or after M3):

1. **The oracle's rung** (M3 oracle physics): 10-17 tick 2823 `ships_vel` ship 48 and 10-18 tick 2871 `ships_vel` ship 49,
   both a bProcessor following the bWorker 44 by `protectShip`'s follow-behind point `P - heading * 1.5 * desiredrange`:
   the Worker's heading (coordsys) as it turns to dock (10-17: order DOCK from 2809, a heading torque of −4404.7 then the
   2.0 rad/s cap) or spins while harvesting at rest (10-18: from 2860, torques −9,600 / −3,949.8); the paths to read are
   the dock flight's heading tracking for a turning Worker (`aitrackHeadingFunc` 0x00418de0, `cat_dockShipDocksAtShip`
   0x00465860) and the Cataclysm harvest path that applies those torques; decision 48's recording would show the Worker's
   coordsys directly. With it: `RangeToTarget` 0x00439c30 / `RangeToTargetGivenDist` 0x00439c80's unrounded returns at
   their callers (25 / the interceptor's two tests) and `cat_gunCanFire` 0x0048a1c0's second-mode threshold (the three
   `CAT-TODO(M3 oracle physics)` with `cloudUpdateSystem` 0x00439130's burst threshold and distances), the rest of the
   inlined `frandom` / `frandombetween` sites (Crates.c, LevelLoad.c's asteroid and derelict placement, which could move
   level layouts, `ApplyCareen`'s torques, the scuttle / death times, GravWellGenerator.c, Gun.c's forced-drop damage,
   AIShip.c; `catFrandomBetweenD` is the helper, each site read before porting), `partMakeSPos`'s x87 offset (render
   only), `GenericInterceptorAttack`'s bombing run (not Cataclysm's races).
2. **The mode-B rung** (the user's session): the GOG playback of `hwirec01` / `hwirec02` with the DLL (the commands in
   "What the user must decide" 2); its first divergent tick, if any, is M3 oracle physics' next item. The right-hand
   side for `hwirec02` is this round's playback (its stream moved from tick 706 by producer 1's flight / combat ports);
   `hwirec01`'s is unchanged since R9. Open with it: the sync checksum's lag (ours 1 packet, GOG's real-time sessions 8)
   and the alliances (the bigotry flag now gives GOG's 21 commands if a recording with alliances is wanted).
3. **Decision 48** (the user's): no recording with the fixed DLL exists; `ships_rot` stays excluded in every session.
4. **Stage 6's play-path items for M3**: **the attended animatic check** (Animatic01 before Mission01, Animatic02 at its
   white-out, with `tools/play.sh`: the brief); **the mission self-test's combat** (M3 test: a scripted player that
   launches, positions and holds interceptors near the raided frigates and keeps the script's timing, or the attended
   play-test's figures; `ShipsDamage` stays); **Mission17's self-targeting KAS Taiidan teams** (M3 missions / oracle
   physics: whether the exe's selection 0x6da2da9 holds player 0's ships at tick 1 and what its attack path does with a
   self-target; the NaN flights are in every Mission17 stream since R9); **the chained campaign's heap** (M3 soak:
   growth heaps 1 .. 3 by Mission08 in `sp`; the "dupe trcolorinfo list" blocks `trRegisterRemoval` /
   `trTextureColorsUpdate` overwrite after `trInternalTexturesDelete` stored them; the 64 MB main heap of the 64-bit
   layout against the exe's 24 MB); `trTextureRegister`'s name case (M3 render, only if a texture is ever registered
   twice under two cases); decision 94's nebula CAT-FIX (the user's: keep the zero, or the exe's undefined read in the
   release build).
5. **Stage 5** (deferred to after M3, decision 9; the record side done): the 23 `CAT-TODO(stage 5 ...)` sites: plain 8 (the
   `/packetRecord` HEAD header `recPackRecordSaveHeader` 0x004cbdb0, CommandWrap.c's point / mimic / upgrade /
   special-target senders and the external-build packet, UnivUpdate.c's dock words), LAN 6 (`KeepAliveDropPlayerCB`,
   `DeclarePlayerWin`, the colour checksum, the network game's start-position table, `sendChatMessage`), multiplayer
   options 8 (the game options word 0x008860ec's bits, `mgUnitCapsEnabled`), multiplayer 1 (`CaptainGameInfo`'s bytes
   after +0xC8); the Titan / WON interface stubbed; `lgStartGame` 0x004c5900 and `ReceivedPacketCB` 0x0044e370 reviewed
   this round, untagged (decision 9's list stays 23).
6. **Stage 4 leftovers that stay**: `gunResetGimbleGuns` uncalled (decision 58), the five creators' default tactics (59),
   the folded handlers' save indices (63).
7. **Stage 3 leftovers that stay**: HW1's cursor text (decision 65), `rndLoadTarga`'s `GL_CLAMP_TO_EDGE` and the
   Transformer (28), the camera-derived render values with interpolation on (the high-refresh-4k feature); closed this
   round: the Mission04 render stop, the animatics' routing, `Select.c:1153` / `1154`, the sound banks' double size.
8. **Motion interpolation**: the `/noInterp` and flags-on dumps unchanged this round (`183681235a3fdc7b` /
   `13ca70eea998e5de`; `428fff6c765ab81b` / `6a4c1850799187ac`; decision 46's guard-rails untouched); the clamp / droid
   blend still unmeasured by the two dump scenes.
9. **The delta map's review flags**: 212 MODIFIED / REWRITTEN rows without a `reviewed` flag (120 outside
   `TitanInterface.cpp`: the list in "Delta map"); a later audit's sample (23 sampled this round: 4 ported, 1 tagged, 1
   IDENTICAL, 17 the tree's or not applicable).
10. **Still open, the user's**: the two saves of the M0 play-test (8), the play-test sign-off (M3's brief below), the fourth
    recording (3), the mode-B playback (2), decision 94's CAT-FIX (4).

## Plan for M3: the recreation plays

**Why M3 now and not an M2-R11**: every R10 gate is met from a clean build (the exit test above): the campaign's
seventeen missions pass headlessly as byte-identical pairs, the `sp` loop walks past Mission04, the animatics are called
at every transition, the rung's two open divergences are localised to one mechanism (a Worker's rotation that only the
fourth recording can show), and no `CAT-TODO` names an M2 owner. What stands between the tree and a signed-off
recreation is not a port but **play**: an attended campaign, the one-hour soak that docs/CONSTRAINTS.md 1 asks for, and
the Windows 11 build. M3 is sized as the user's play-test in parallel with one producer round (three producers + three
auditors + integrate, 7 agents: the Windows build, the soak's instrumentation and leak, the oracle's rung), then a fix
round sized from the play-test report, then the stage-5 remainder (decision 9).

**Sizing** (the reports' "What is left"; `grep -rn 'CAT-TODO(' src/`): 3 `CAT-TODO(M3 oracle physics)` + 2 localised
divergences + about 10 unread `frandom` sites + 1 self-target; 2 leak candidates + 1 measurement switch; the Windows
build's two platform layers (`src/SDL/` is built, `src/Win32/` is not: the tree builds through SDL2 + GL on both OSes),
the harness's 29 modes, the LP64 save blob (decision 16: nothing pinned; the exe-format recordings are fixed-width
already) and the x87 helpers' libm calls (`sin` / `cos` / `tanl` for `fsin` / `fcos` / `fptan`, decision 71: glibc's
against the UCRT's is the first thing to measure).

| # | step | scope | counts | done when |
|---|---|---|---|---|
| 0 | **the user's play-test** (now, `tools/play.sh`; the brief below) | the campaign from New Game as far as it goes (Mission01 .. 04 and on to the credits if it goes), **Animatic01 / 02 attended** (producer 2's check), the tutorial's 12 lessons, a skirmish against one computer, a recording and its playback | 3-6 hours of play | a report per item (what differed from the GOG game, every stop with the log's last lines, the frame rate's drift); M3-R2 sized from it |
| 1 | **the Windows 11 build** (producer) | meson + SDL2 + GL with clang (clang-cl / lld-link, decision 7's toolchain) or MSVC; what the Linux tree assumes (the offscreen video driver, ASan / LSan, `long` is 32-bit on Win64: the LP64 save blob changes shape, so the native save format is pinned to fixed-width types or saves stay per platform; the path layer; `tools/m2_harness.sh` under Git Bash / MSYS2 or a port of the modes the gate needs: statics .. missiontest, mission, replay, skirmish); the x87 helpers' libm dependence measured first (the Mission01 stream, both replays and one mission run against the Linux streams; a last-ulp difference in `sin` / `cos` names its site) | 2 platform layers, 29 modes, 1 format pin | the release and debug builds on Windows 11 with 0 warnings; the self-tests PASS there once; the headless Mission01 stream, the two replays and a mission stream IDENTICAL to the Linux ones (or every difference localised to a libm call and closed); a save and a recording made on one OS load and replay on the other |
| 2 | **the one-hour 4K@144 soak, no gamescope** (producer + the user's hour) | a `/frameLog` switch (HWI-TEST) writing per-second frame-time percentiles, the universe tick rate and the heap's used / growth counters to the log; the headless proxy: the chained `sp` loop walked past Mission09 for an hour on the virtual clock with the heap counters (the growth heaps 1 .. 3 by Mission08: the "dupe trcolorinfo list" overwrite in `trRegisterRemoval` / `trTextureColorsUpdate`, the 2,438 live blocks at Mission09, the main heap's size for the 64-bit layout against the exe's 24 MB); then the attended hour at 3840x2160@144 (`tools/play.sh -- /fullscreen`, the campaign or a skirmish, no gamescope: docs/CONSTRAINTS.md 1) | 1 switch, 2 leak candidates, 1 attended hour | no frame-rate drift over the attended hour (the percentiles of the last ten minutes within the first ten's); the headless hour allocates no growth heap, or every growth is attributed and fixed; ASan 0 |
| 3 | **the oracle's rung** (M3 oracle physics; the fourth recording and mode B if the user made them) | 10-17 2823 / 10-18 2871 (the Worker's dock / harvest heading paths against the exe), `RangeToTarget` / `RangeToTargetGivenDist`'s unrounded returns at their callers, `cat_gunCanFire`'s second mode, `cloudUpdateSystem`, the other `frandom` sites (LevelLoad.c's first: layouts), Mission17's self-targeting KAS teams (the exe's selection and attack path) | 3 tags + ~10 sites + 2 divergences + 1 self-target | the 10-17 replay IDENTICAL beyond 2823 and the 10-18 beyond 2871, or the next divergences localised and owned; mode B's first divergence localised if run; no NaN flight in Mission17 or its cause stated as the exe's |
| 4 | **M3-R2: the fix round** | sized from the play-test report (every stop and difference an item with an owner) | | the play-test's stops fixed and re-played; the sign-off |
| then | **stage 5** (decision 9) | the 23 `CAT-TODO(stage 5 ...)` sites: the LAN game's senders and keep-alive, the options word, the `/packetRecord` header, the Titan / WON stubs | 23 sites | LAN interop with stock 1.01 nice-to-have (decision 9); the wire format not broken |

**M3 integrate**: the R10 gates hold from a clean build on both OSes (the mission streams unchanged or re-baselined with a
stated reason), the soak's figures, the rung beyond 2823 / 2871 or owned, no `CAT-TODO` naming M3, the generators `--check`
current on both OSes. If the play-test finds a stop in the campaign before the Windows build is green, M3-R2 goes first.

## What the user must decide

1. **The fourth recording with the DLL's `CAT_SHIP_coordsys` 0x98 edit** (decision 48): still none (the install's
   `oracle01` is the 10-19 recording, 1,181,732 B; no newer `original/oracle/oracle01-rec-*` directory). Both open
   divergences (2823 / 2871) point at the bWorker 44's coordsys, which only this recording makes comparable: the one-line
   edit in `tools/oracle/dll/hook_sites.h`, a DLL rebuild (clang-18 + lld-link, decision 7) and a recording session.
   Recommendation: before M3's rung producer runs; the 10-17 recording stays the continuity reference. Producer 1's
   question: without it, M3 oracle physics reads the Worker's dock and harvest heading paths against the exe (default:
   yes, it does; the recording makes the check direct instead of inferred).
2. **The GOG playback of the hwi-engine recordings (the mode-B rung)**: the user's step, not run (no
   `original/oracle/hwirec*-play-*` directory). Producer 1's current commands (from the main checkout; nothing in
   `original/` is written by an agent):
   ```
   cd ~/Documents/GameDev/Homeworld-Infection
   R=original/scratch-m2/m2r2-harness/runs
   cp $R/r9r-rec01/settings/SavedGames/RecordedGames/hwirec01 $R/r9r-rec01/settings/SavedGames/RecordedGames/hwirec01.pkts original/game/SavedGames/RecordedGames/
   cp $R/r9r-rec02/settings/SavedGames/RecordedGames/hwirec02 $R/r9r-rec02/settings/SavedGames/RecordedGames/hwirec02.pkts original/game/SavedGames/RecordedGames/
   ORACLE_NAME=hwirec01 tools/oracle/record_reference.sh verify     # two GOG playbacks on the virtual clock, rng0 split
   ORACLE_NAME=hwirec02 tools/oracle/record_reference.sh verify
   python3 -I tools/oracle/hwo_compare.py original/oracle/hwirec01-play-<timestamp>/hwirec01-play-a.hwo \
     original/scratch-m2/m2r2-harness/runs/r10i-play01-a/run.hwo --from-tick 1 --to-tick 4800 \
     --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,players,dt,total \
     --json original/oracle/hwirec01-play-<timestamp>/compare-modeB.json
   python3 -I tools/oracle/hwo_compare.py original/oracle/hwirec02-play-<timestamp>/hwirec02-play-a.hwo \
     original/scratch-m2/m2r2-harness/runs/r10i-play02-a/run.hwo --from-tick 1 --to-tick 4800 \
     --fields <the same list> --json original/oracle/hwirec02-play-<timestamp>/compare-modeB.json
   ```
   (the recordings unchanged: `hwirec01` `4640546c0708c284` / `.pkts` `e852ee9f3a457e3b`, `hwirec02` `13a17b9f3a2d8664` /
   `d24604387e083cf5`; the exclusions: `ships_rot` (decision 48), `rng0` (the DLL splits stream 0), `rng_all`, `fpcw`,
   `real` (decision 60); `rng6` and `players` stay in since both sides are playbacks). The right-hand sides are this
   step's playbacks (`r10i-play01-a` is IDENTICAL to R9's `r9i-play01-a`; `r10i-play02-a` is this round's content, moved
   from tick 706 by the flight / combat ports, so R9's `r9i-play02-a` is no longer a valid right-hand side). IDENTICAL over
   4,800 ticks passes the rung; a divergence is M3 oracle physics' next item. A recording with alliances
   (`--skirmish-bigotry 2`) can be made on request (producer 3's `hwibig02` / `hwibig03` are such recordings).
3. **Producer 1's deferral** of the other inlined `frandom` sites (Crates.c, LevelLoad.c's asteroid and derelict
   placement, the careen torques, the scuttle / death times, GravWellGenerator.c, Gun.c's forced drop, AIShip.c): default
   accepted, each read against its exe site in M3 oracle physics before porting, LevelLoad.c's first since its sites
   could move level layouts (no replay or mission stream shows them yet).
4. **Producer 2's questions**: Mission17's KAS-controlled Taiidan teams attack a selection holding themselves (the NaN
   flights from tick 328): owner M3 missions / oracle physics (default); the user can watch, in the GOG game's Mission17,
   whether the Taiidan fighters near the Nomad Moon fly at all. The chained campaign's heap growth: the M3 soak's
   question (default). The attended animatic check: the brief below.
5. **Producer 3's questions**: decision 94's cause was the nebula, not the mixer, fixed as a `CAT-FIX` (the bounding
   sphere starts at zero: what a fresh pool block holds; only `rng_all` ever saw the difference): default keep the fix in
   every build. The re-baseline: this step re-baselined all 17 mission streams (the table above), not only Mission06 and
   Mission03. The combat: the attended play-test decides (default), no scripted interceptor positioning attempted. The ai
   soak's `rng_all` under load (decision 84): not investigated; this round's soaks were IDENTICAL to each other; the
   `HWI_EXP_RNGLOG` experiment binary names the stream if it comes back.
6. **Whether M3 starts now** with the play-test in parallel with its producer round (recommended: yes, the brief below),
   and the defaults recorded as decisions 98-110 in cycle/STATE.md (say if you want otherwise).

## M3 play-test brief (refreshed; replaces R9's)

An attended run is the only one that may be audible or on screen (docs/CONSTRAINTS.md 5). Launch with the user's
launcher `tools/play.sh` (read its header): the release build (`build/release/hwi-engine`, built by `tools/build.sh
--release` if missing; `--debug` for the ASan build when a crash needs a report; `--rebuild` after a pull), the harness
data dir (`original/scratch-m2/m2r2-harness/data`: the install's own files with writable SavedGames / ScreenShots next to
them, so nothing is written in `original/game`), a private home `original/play-home` (its `.homeworld/Cataclysm.cfg`,
keys, saves, recordings and screenshots live there), X11 by default; engine switches after `--`; the log under
`original/play-home/logs/play-<timestamp>.log`:

```
cd ~/Documents/GameDev/Homeworld-Infection/.claude/worktrees/m2
tools/play.sh -- /window              # or /fullscreen; /1600 for 1600x1200; --debug for the ASan build; --rebuild after a pull
```

Play, and report back per item:
1. **The animatics** (new this round; producer 2's attended check, report 2.1): Single Player -> New Game: **Animatic01**
   must play full screen before Mission01's opening NIS at its own pace (30 fps, about 4 minutes, the subtitles from
   `movies\animatic01.script`, its sound and music, the cursor hidden); at its end the cursor returns and the opening NIS
   starts. At Mission01's hyperspace jump (the white-out) **Animatic02** must play the same way, then Mission02 loads. In
   the log: `animatic: play a=0 b=1 file=Movies/Animatic01.bik` and `a=1 b=2 ... Animatic02.bik`, and **no `animatic:
   skipped` line** in an attended run. Listen for the animatic's audio starting with the picture, the game's music
   stopped during it, the game sounds back (volumes restored) afterwards. Optional: Mission17's end plays Animatic18
   before the credits. (Escape skips an animatic in the exe: report whether it does here.)
2. **The campaign from New Game, as far as it goes.** Mission01 as before (the opening NIS, the arrival and the nislet,
   the six objectives in order, the hyperspace button's jump; the KAS saves "01 - Hiigara" / "01 - ~ End of Mission"
   loading back through Load Game). Then **Mission02** (the Kith destroyer found, the micro module, the repair and salvage
   research, the destroyer repaired, the raiders, the pod; the letterbox jump), **Mission03** (the hangar module, the
   Ecclesiam found, the mimic's disguise, the lure; **the roll-call screen** with Go Now / Abort), **Mission04** (the
   at-once arrival, the docking bay, the escort, the research, the Cleesan; the roll call again: **this mission no longer
   stops the debug build**, report what the release build shows of the P1 ion array frigates' and fighters' team colours
   after Mission03, since the colour-scheme registration was the bug), and on as far as it goes: Mission05's Bentusi and
   the Beast, Mission06's nebula escort (its tendrils drew from an uninitialised sphere before this round: report any
   nebula tendril that pops in or out), Mission07's convoy, Mission08's spy and the captures, Mission09's transports and
   **Emergency Hyperspace under fire**, Mission10's Siege Cannon, Mission11's Faal-Corum, Mission12's pirates, Mission13,
   Mission14's mimic sample and the capture, Mission15's slipgates, Mission16's command ship, Mission17's Naggarok (watch
   the Taiidan fighters of the Nomad Moon's side: do they fly, or sit still / vanish?) and **the credits**. For each
   mission: what differed from the GOG game in the same place (the order of events, a missing screen, text, sound or
   speech, a ship that does not do what the script says, the roll call's buttons, the difficulty's effect), **how long
   the fights take** (the self-tests never fly the player's fleet: the first measured combat is this one), and where it
   stopped if it stopped (the mission number matters: the chained campaign grows the heap by about one growth heap every
   two missions, so a stop late in the campaign is the soak's item).
3. **The tutorial** from the front end: all 12 lessons (Next through each; the narration, the pointers, the objective
   list, the lesson saves under Training and one loading back).
4. **A skirmish** from Multiplayer > Skirmish against one computer opponent on a listed map (Armed for Burial (8);
   LiveFireExercise2 is hidden in the exe too) for 10 minutes, once with CPU Attacks set to "Humans only" or "Humans and
   allies" (the bigotry radio button, whose alliance commands are the recorder's new item): the start positions, the
   computer player's builds and attacks, the sensors manager, the managers' screens; then **record one** through the
   Record shortcut and play it back through Load Recorded Game.
5. For every item: any crash or hang with the terminal's last lines (`hwi-engine: fatal signal N`, the ASan report of a
   `--debug` run), the frame rate's drift over the session (the one-hour 4K@144 soak is M3's own item: if a session runs
   an hour at 4K@144 without gamescope, note the frame rate at the start and the end), and a screenshot of anything that
   looks wrong.

The known gaps to skip, not report: the player's own fleet is never flown by the tests, so combat balance is untested
(item 2 measures it); a failed hyperspace jump happens only from the developer console (`DisablePlayerHyperspace`), never
in retail play; the GOG game's ships can differ from ours in the last bits of their velocities after about three minutes
of a skirmish (the rung's 2823 / 2871: invisible in play); the stage-5 LAN game and the Titan / WON screens are not ported
(do not start a LAN game); a lesson save or a campaign save written by this build is in the tree's native format (only a
recording's save is in the exe's format, decision 95), so it does not load in the GOG game.

## Decisions

**Applied this round:**
- The faithfulness defaults and decisions 17-97 of cycle/STATE.md; decision 70 (the table square root) under every run;
  decision 9 as "recorder" (stage 5's 23 sites untouched: no producer edited them; `lgStartGame` / `ReceivedPacketCB`
  reviewed, not tagged); decision 46's guard-rails untouched (the `/noInterp` dumps unchanged); decision 47's exclusions
  and decision 60's `real` applied to every comparison; decision 48 open (no fourth recording; `ships_rot` excluded);
  decision 55 followed by every producer and audit (the audits added 17 review entries of their own); decision 57 stands
  (the headless Mission01 stream did not move: no ship flies there; the flight / combat ports moved every stream where
  ships fly); decision 75 untouched (no generated mission file changed: the generator `--check` current); decisions 76 /
  77 / 86 stand for every mission (the combat deferred again, measured); decision 91 applied (the animatics routed, the
  headless tests skip them); decision 92 extended (the harness's two new flags); decision 93's virtual audio clock
  extended to the mission runs (decision 94's default), where it is a no-op.
- **Decision 44 unchanged**: the mission-stream baseline did not move this round: the headless Mission01 stream of this
  step's build (`5bd51234405ea0d8`, release `a44d585d71233c4a`) is IDENTICAL in every field over 6,001 ticks to R9's
  `553845b65e87f3b5` (only the header's build identity differs), as every producer's and audit's run of the round
  (`44f6c81a6bf3f58d`, `902884367d98025d`, `65456c876ff727cc`: the same content). **Decision 49 / 82 updated**: the oracle's
  first divergences are tick **2823** (10-17, ship 48 `ships_vel`) and **2871** (10-18, ship 49 `ships_vel`) on the
  default; 177 / 118 with `/oracleLibmSqrt`; the shooter of the old 1721 is the sMothership 24's guns 3 and 6.

**Still open (the user's; none blocks M3):**
- **8.** Two saves from the M0 play-test (one mission, one skirmish).
- **The play-test sign-off** (docs/PLAN.md); M3's brief above.
- **Decision 48**: a fourth recording with the fixed oracle DLL.
- **The mode-B playback** of `hwirec01` / `hwirec02` ("What the user must decide" 2).

**Surfaced by M2-R10.** Each has a default in the tree; say if you want otherwise (numbered 98-110 in cycle/STATE.md).
98. **The 10-18 shooter and the line of sight**: `collCheckLineOfSight` 0x0043c2e0 skips every ship clamped to the source
    or the target in the source's blob (the mothership's own modules no longer obscure its guns; `cat_collLosSkipClamped`,
    its NULL guard a `CAT-FIX`); `collCheckRectLine` 0x00439ef0 without rectangle information is a miss and has no early
    "pointing away" miss. The shooter of 10-18 tick 1721 is the sMothership 24's guns 3 and 6 at the bRecon 41.
99. **The flight and combat path's x87 shape, continued** (producer 1): `GenericInterceptorAttack`'s normalisation
    (`catInterceptorNormalizeTrajectory`), dot products and break angle; `GenericInterceptorCanFire`'s elapsed time;
    `aishipGetTrajectoryWithAngleCorrection`; `collCheckBulletTargetColl`'s sphere test without a square root; the exe's
    inlined x87 `frandombetween` (`catFrandomBetweenD`: the draw loaded whole, one rounding at the caller's store) **only
    where its exe site was read** (the bullet / missile / explosion rocks, `ApplyCareenRotationDirectly`, `gunCanShoot`'s
    passive penalty; the other `frandom` sites stay HW1's macro until read: M3 oracle physics); the hit's recoil; the burst
    cut's double 0.1 (`cat_univShipDeferredUpdate` 0x005447b0); `gunCanShoot` 0x0048a2f0's shape; `aiuFindDistanceSquared`
    0x0041a2c0 and `tacticsGetShipsMaxVelocity` 0x00529450 return doubles (their 35 / 16 exe call sites read); the seven
    Blobs.c bodies (`catBlobSortDistance`, `catBlobVolumeFactor`). The dump's 'T' / 'G' lines (`oracleShipDumpTarget`,
    `oracleShipDumpAttack`). **Not ported, owned by M3 oracle physics** (`CAT-TODO`): `RangeToTarget` 0x00439c30 /
    `RangeToTargetGivenDist` 0x00439c80's unrounded returns (the audit's finding), `cat_gunCanFire`'s second-mode
    threshold, `cloudUpdateSystem` 0x00439130's shape (producer 3's review).
100. **The rung's next divergences** (decision 49 / 82's numbers): 10-17 tick 2823 and 10-18 tick 2871, both a bProcessor of
    player 1 following the bWorker 44 by `protectShip`'s follow-behind point while the Worker turns to dock / spins while
    harvesting: the Worker's coordsys, invisible to the stream without decision 48's recording. Owner M3 oracle physics
    (the dock flight's heading tracking `aitrackHeadingFunc` 0x00418de0 / `cat_dockShipDocksAtShip` 0x00465860, the
    harvest torques).
101. **The Mission04 render stop closed** (decision 90): GoK's `HW_PTR_64` guards in `meshFree` / `trTextureUnregister`
    refused every handle with a palette index above 0 (HWI fix); the exe's `trTextureUnregister` 0x005860d0 (the pending
    slot reset at the last usage) and `trTextureDelete` 0x005854c0 (the colour-info list freed) ported;
    `trColorsIndexAlloc`'s ninth scheme is a `CAT-FIX` (palette 0 and a warning: the exe and the earlier release build
    wrote past the 64-byte block). The `sp` gate walks Mission01 .. 09 in 900 s (this step: Mission01 .. 12 in 1,200 s, 127 screenshots).
    `trTextureRegister`'s name case (the exe uppercases, GoK does not) stays until a texture is found registered twice.
102. **The animatics routed** (decision 91 applied): every caller of 0x00421860 (`singlePlayerPostInit`,
    `singlePlayerGameUpdate`'s ONE call at 0x0051a01a, `utyVideoPlay`, which no `.fib` binds, and `kasfMissionCompleted`)
    reaches `animAviPlay`; a headless run (the virtual clock or `/debugScreenshots`: `animHeadlessSkip`) skips the
    playback after the exe's time-compression reset and tutorial test and logs `animatic: skipped (headless) a=N b=M
    file=AnimaticNN`; an attended run logs `animatic: play ...`; the harness summary counts them (`animatics:`). The
    headless skip no longer sets `animaticJustPlayed` (GoK's stub did; render only). The attended check is the user's
    (the brief). Name: `animHeadlessSkip`.
103. **`Select.c:1153` / `1154` closed** (decision 85): `selCircleCompute` 0x00506720 ported (the depth test `depth <= 0`
    before the rectangle and before a precise hit, the pixel rounding `catSelScreenX/Y` by the exe's `1.5 * 2^52`
    trick, the radius cases: a mimic's `cat_mimicGetSelectRadius` 0x005b94e0, a bullet 1.0, the slipgates +200 / +300 /
    +400, no precise selection of a bullet); GoK's mouse pixel-centre (decision 29) kept. **`FOW.c:296-437` closed**: the
    cube boxes in 32-bit two's complement as the exe's `sub` / `add` (`fowCellSub` / `fowCellAdd`, `CAT-FIX`); the INT_MIN
    cells came from NaN positions of Mission17's KAS-team Taiidan heavy interceptors (player 0) attacking a selection
    that holds themselves (`Init_Mission17`'s 0x6da2da9 through `kasfAttack`; `clAttack` 0x00448950 never removes the
    attackers): whether the exe's selection holds them is M3 missions / oracle physics' question (the NaN ships are in
    every Mission17 stream since R9).
104. **Mission17's log**: `is_final_tex` (`partAdvanceTexAnim` 0x004df940's 64-bit helper) decides on the low 32 bits of the
    4-byte `-1` terminator (98,718 warnings -> 0); the `memAllocFunctionA` lines are growth-heap notes, not failures (the
    64 MB main heap of the 64-bit layout 94% full after Mission17's load; the exe's heap is 24 MB, `HeapSizeSet`
    0x0055f390), named by block now; GoK's `loadbankAndConvertTo64Bit` allocated twice each sound bank's length: the exact
    converted size now (HWI; 8.2 MB back in the main heap of every run with sound). The chained campaign still allocates
    growth heaps 1 .. 3 by Mission08: the M3 soak's question (the "dupe trcolorinfo list" overwrite in
    `trRegisterRemoval` / `trTextureColorsUpdate`, the audit's O4, is the first candidate).
105. **Mission06's `rng_all` flake** (decision 94 answered by a fix, not by the sound streams): the mission self-tests run
    with no mixer at all (sound and speech off), so the suspect could not hold; the cause was the nebula tendrils'
    uninitialised bounding sphere (`nebInitTendril` 0x004c7d50 never sets `radius` / `midpoint`, `nebRenderNebula`
    0x004cadb0 reads it; the exe the same; stream 5 `RANDOM_NEBULAE`, render only, main thread): `CAT-FIX` zero, what a
    fresh pool block holds. Mission06 is byte-identical over four runs. The mission self-tests set the virtual audio clock
    (`CatMissionTestSet`), a no-op while sound is off; a normal run is unchanged. Only Mission06's `rng_all` moved by the
    fix (from 22235) and Mission03's by producer 2's texture-registry change (one `RANDOM_TRAILS_1` draw in `trailDraw`,
    from 6708): the 17 mission streams re-baselined in the table above.
106. **The failed-jump test** (`/catMissionTestFailJump`, harness `--fail-jump` with `--mission N`): `hyperspaceFails` is
    written 1 only by `kasfDisablePlayerHyperspace` 0x004935e0, reachable only through the console's KAS call
    (`cat_kasCallFunctionByName` 0x0045b9a0: no mission script calls it; a retail jump fails only from the console); the
    test makes that call at its first hyperspace command, the jump fizzles, case 10 (`HYPERSPACE_CATFAILEDRETURN`)
    returns the fleet and is judged; the run ends 32 ticks later (case 10 does not give `playerCanHyperspace` back). A
    roll-call mission only (not 2, 5, 7, 15, 17); Mission03 in the gates. Names: `catMtFailJumpArm`, `catMtFailJumpWatch`,
    `catMtFailJumpCount`, `catMissionSelfTestFailJumpSet`, `CatMissionTestFailJumpSet`.
107. **The skirmish test's bigotry** (`/catSkirmishBigotry N`, harness `--skirmish-bigotry N`): `opCPUAttacks` 0x004db440
    pressed through its callback with a button atom of pData N (update.big's `Skirmish_Basic` radio button); 2 gives
    GOG's 21 `ALLIANCE_FORM_NEW_ALLIANCE` commands in a 2-player game (`cat_aisSetupAlliances` 0x004310a0: every index
    pair from the first computer to 7); the default stays 0 (every default skirmish stream unchanged); a record run that
    must pair in every field adds `-- /catVirtualAudio` (the skirmish test keeps sound on; stream 12's speech draws).
    Names: `catSkirmishSelfTestSetBigotry`, `CatSkirmishBigotrySet`.
108. **The `.pkts` bytes the exe leaves to the heap are zeroed** (`CAT-FIX`): the sync header's tail after
    `numberOfCommands` (+0x1A, never written in `captainServerTask` 0x00451580's three branches) and every command's struct
    padding (the 16 HW1-layout senders `memset` their blocks, noted at `clPktCommandAlloc`); nothing reads them (the sync
    checksum is the four checksum words); two record runs' `.pkts` are byte-identical; the playbacks unchanged. GOG's own
    files carry heap words there (10-17: 2,535 of 4,800 packets set; 10-18: 1,159 of 1,763).
109. **The mission self-test's combat** (decision 77 stands): measured and deferred to M3 test: the targets are fine; the
    player's interceptors are 30,000-54,000 units from the raiding bombers; R9's experiment re-issued the order every 64
    ticks (re-picking targets each time); one held order kills 2 of 14 bombers in 3,200 ticks and loses 3 ships, of the
    order of 15,000-20,000 ticks for all 14 while the script's next objective comes at 8096. The attended play-test
    measures the real fight.
110. **The review sample and the small ports** (producer 3): `parseLine` 0x00526120 keeps `//` after `http:` (`CAT`; the
    strings test checks the three URLs: `urls=3 whole=3`); `cloudZapLoop` 0x00438c30's x87 shape (`catCloudDistSqD`);
    `selCentrePointComputeGeneral` 0x00508be0's missile scale `catMissileScale` (+0x254) and sum (the old
    CAT-PROVISIONAL retired); `collCheckShipDerelictColl` 0x0043bed0's clamp skips and distance shape (its NULL guards a
    `CAT-FIX`); `scriptSetTweakableGlobals` 0x00527820 is the tree's; `trMeshSortAdd` 0x00586190 IDENTICAL; 120 unflagged
    rows outside `TitanInterface.cpp` remain. The harness (decision 92 extended): `--fail-jump`, `--skirmish-bigotry N`,
    the `animatics:` summary line; switches `/catMissionTestFailJump`, `/catSkirmishBigotry`; still 29 modes.
