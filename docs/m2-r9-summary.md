<!-- done: exit-test, ports, deltamap, oracle-a, campaign, left, r10-plan, user-decisions, playtest-brief, decisions -->
# M2-R9 summary: the oracle's next rung, stage 6 part B (missions 02-17, the game's end, the warp start, the Tutorial1 lessons) and the recorder's record side

Status: **COMPLETE, 2026-10-20: the R9 exit test (the stage-6 exit) MET**; this doc and the cycle/STATE.md update are the
integrate step's checkpoint commit. Branch `m2`,
worktree `.claude/worktrees/m2`, workflow `cycle/workflows/m2-r9.js` (run id `wf_c0cd6893-248`, `ROUND_BASE 54dcbb2`,
`FSQRT_DEFAULT 'table'`, `STAGE5 'recorder'`, `FOURTH_RECORDING null`), run from the user's terminal session 38027cce
(Fable), not from the orchestrator. 9 agents: four producers in sequence (the four rows of docs/m2-r8-summary.md "Plan
for M2-R9": the rung, missions 02-09, missions 10-17, the recorder's record side + the test / tooling leftovers), four
auditors (each checkpointed its port: `64f49a9`, `307c3a5`, `9c1801f`, `9574c9b`) and this integrate step. No producer
was cut off this round. User decisions: cycle/STATE.md "## M2" (the faithfulness defaults and decisions 17-80 applied;
**decision 70 answered 2026-10-19: the exe's table `fsqrt` is the engine default**, the switch inverted to
`/oracleLibmSqrt`; stage 5 reduced to the recorder's record side; no fourth recording, so decision 48 stays open and
`ships_rot` stays excluded).

## Exit test (this step, from a clean build of `9574c9b`)

**Builds.** `tools/build.sh --debug --clean` and `--release --clean` on the tree at `9574c9b` (08:02-08:03; about 10 s
each on the 24-core box, gcc 13.3.0): both rc 0, `[270/270] Linking target hwi-engine` (R8: 253; the sixteen generated
missions and `CatSort.c` came in), **0 `warning:` and 0 `error` lines** under `-Werror` (the one WARNING line is meson's
sanitizer notice). The binaries were copied aside to `original/scratch-m2/m2r9-integrate/{debug,release}/hwi-engine` so
that nothing rebuilt under the runs.

| build | size | sha256 |
|---|---:|---|
| debug (ASan + UBSan) | 88,553,608 B | `48c6e125803c3198…` |
| release (`-O2`, no sanitizers) | 5,610,600 B | `9112f03e0458f0f2…` |

**Runs.** 102 harness runs `r9i-*` on the plain `tools/m2_harness.sh` (silent, offscreen; `runs.txt` lines
2296-2400) in five parallel chains (A the rung's replays and the recorder, B the R8 gates and the scripted
self-tests, C missions 02-09, D missions 10-17, E Mission01's self-test, the tutorial, the warp start, the lesson load, sp,
fe) beside the other agents' load (load average 14-19 throughout; no run lost its summary), plus the three bisect runs
`r9i-bis-*` of the ai soak (below). Every quitting run rc 0 except `r9i-sp` (rc 1: the Mission04 assertion) and `r9i-fe`
(rc 124: the 120 s timeout at Main_game_screen); **ASan 0 in every run**; LeakSanitizer clean in every run except
`r9i-sp`'s 40 B in 10 allocations at the assertion's abort (as in every sp run since producer 2's: the abort path frees
nothing). The `.hwo` hashes differ from the producers' and audits' for the same content: the stream header carries the
build identity (`hwo_compare.py` prints it); IDENTICAL below is the tool's verdict over every tick and packet record.

| run | mode | rc / wall | milestone | first stop | sanitizers | run.hwo sha256 (size) | result |
|---|---|---|---|---|---|---|---|
| `r9i-mission-a` | mission | 0 / 24s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 553845b65e87f3b5 (1476370 B) |  |
| `r9i-mission-b` | mission | 0 / 23s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 553845b65e87f3b5 (1476370 B) |  |
| `r9i-r17-a` | replay | 0 / 50s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 3fb2f34e7e5baf9f (2630929 B) | ticks=9601 packets=4800 ships=80 randSyncErr=1918 univSyncErr=3767 blobSyncErr=4746 pktSyncErr=0  |
| `r9i-missiontest-a` | missiontest | 0 / 63s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cc7f22e7129b9471 (3246588 B) | objectives=6/6 result=PASS  |
| `r9i-mission-perturb` | mission | 0 / 24s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 86c3b6ff93a005fe (1476419 B) |  |
| `r9i-mission-rel-a` | mission | 0 / 5s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ba8ee6e78597c175 (1476370 B) |  |
| `r9i-m2-a` | missiontest | 0 / 83s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d71408aacb2a6a5b (6184316 B) | objectives=6/6 result=PASS  |
| `r9i-mission-rel-b` | mission | 0 / 5s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ba8ee6e78597c175 (1476370 B) |  |
| `r9i-mission-rel-perturb` | mission | 0 / 5s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f4e628c5c2a4bd9f (1476419 B) |  |
| `r9i-r17-b` | replay | 0 / 48s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 3fb2f34e7e5baf9f (2630929 B) | ticks=9601 packets=4800 ships=80 randSyncErr=1918 univSyncErr=3767 blobSyncErr=4746 pktSyncErr=0  |
| `r9i-mission-noi` | mission | 0 / 23s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 553845b65e87f3b5 (1476370 B) |  |
| `r9i-m10-a` | missiontest | 0 / 119s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2fcca6671e2fcec1 (5397608 B) | objectives=5/5 result=PASS  |
| `r9i-missiontest-b` | missiontest | 0 / 59s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cc7f22e7129b9471 (3246588 B) | objectives=6/6 result=PASS  |
| `r9i-mission-nonis` | mission | 0 / 23s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 553845b65e87f3b5 (1476370 B) |  |
| `r9i-r17-libm` | replay | 0 / 47s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f9149ff29ef6c862 (2630929 B) | ticks=9601 packets=4800 ships=80 randSyncErr=3347 univSyncErr=4693 blobSyncErr=4746 pktSyncErr=0  |
| `r9i-mission-libm` | mission | 0 / 24s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 553845b65e87f3b5 (1476370 B) |  |
| `r9i-m2-b` | missiontest | 0 / 81s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d71408aacb2a6a5b (6184316 B) | objectives=6/6 result=PASS  |
| `r9i-tutorial-a` | missiontest | 0 / 43s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 416f819eaa29e3e0 (2569841 B) | lessons=12/12 objectives=1/1 objectives=1/2 objectives=2/2 objectives=3/3 objectives=4/4 objectives=5/5 result=PASS  |
| `r9i-r18-a` | replay | 0 / 26s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | abaa0ad279c50bec (966653 B) | ticks=3527 packets=1763 ships=55 randSyncErr=880 univSyncErr=875 blobSyncErr=1619 pktSyncErr=0  |
| `r9i-statics` | statics | 0 / 13s | ship statics self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | 143 of 145 ship types loaded  |
| `r9i-statics-init` | statics | 0 / 13s | ship statics self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | 143 of 145 ship types loaded  |
| `r9i-r18-b` | replay | 0 / 26s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | abaa0ad279c50bec (966653 B) | ticks=3527 packets=1763 ships=55 randSyncErr=880 univSyncErr=875 blobSyncErr=1619 pktSyncErr=0  |
| `r9i-meshes` | meshes | 0 / 12s | mesh self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | 1758 files, 0 with bad objects  |
| `r9i-tutorial-b` | missiontest | 0 / 43s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 416f819eaa29e3e0 (2569841 B) | lessons=12/12 objectives=1/1 objectives=1/2 objectives=2/2 objectives=3/3 objectives=4/4 objectives=5/5 result=PASS  |
| `r9i-textures` | textures | 0 / 19s | texture self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | 12403 listed, 12403 found  |
| `r9i-warp10-a` | warpstart | 0 / 16s | warp-start self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | fa0dc28b3d712299 (24968 B) | result=PASS  |
| `r9i-r18-libm` | replay | 0 / 28s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8edb29922c0969cc (966654 B) | ticks=3527 packets=1763 ships=55 randSyncErr=983 univSyncErr=1658 blobSyncErr=1619 pktSyncErr=0  |
| `r9i-oracle` | oracle | 0 / 11s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e50ee033d9c9e0e7 (140723 B) |  |
| `r9i-oracle-rel-a` | oracle | 0 / 1s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b91550e3bd75a052 (140723 B) |  |
| `r9i-oracle-rel-b` | oracle | 0 / 1s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b91550e3bd75a052 (140723 B) |  |
| `r9i-oracle-rel-perturb` | oracle | 0 / 1s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d52a2eb12bf6ef91 (140773 B) |  |
| `r9i-m10-b` | missiontest | 0 / 122s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2fcca6671e2fcec1 (5397608 B) | objectives=5/5 result=PASS  |
| `r9i-warp10-b` | warpstart | 0 / 16s | warp-start self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | fa0dc28b3d712299 (24968 B) | result=PASS  |
| `r9i-m3-a` | missiontest | 0 / 80s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f55080e8277964cf (4385810 B) | objectives=5/5 result=PASS  |
| `r9i-etg` | etg | 0 / 12s | etg effect self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  |  |
| `r9i-play01-a` | replay | 0 / 27s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 85fddfcf8b41facb (1315714 B) | ticks=4801 packets=2400 ships=57 randSyncErr=0 univSyncErr=0 blobSyncErr=0 pktSyncErr=0  |
| `r9i-tload-a` | loadgame | 0 / 22s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a6f4173d3188ef18 (886516 B) | objectives=1/1 result=PASS  |
| `r9i-dock` | dock | 0 / 23s | dock self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS  |
| `r9i-play01-b` | replay | 0 / 27s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 85fddfcf8b41facb (1315714 B) | ticks=4801 packets=2400 ships=57 randSyncErr=0 univSyncErr=0 blobSyncErr=0 pktSyncErr=0  |
| `r9i-tload-b` | loadgame | 0 / 21s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a6f4173d3188ef18 (886516 B) | objectives=1/1 result=PASS  |
| `r9i-command` | command | 0 / 19s | command self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS  |
| `r9i-m11-a` | missiontest | 0 / 54s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b2c33be9927d8190 (2036509 B) | objectives=5/5 result=PASS  |
| `r9i-play02-a` | replay | 0 / 24s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c8f6c7dc60e47434 (1315714 B) | ticks=4801 packets=2400 ships=70 randSyncErr=0 univSyncErr=0 blobSyncErr=0 pktSyncErr=0  |
| `r9i-render` | render | 0 / 16s | render self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS  |
| `r9i-m3-b` | missiontest | 0 / 75s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f55080e8277964cf (4385810 B) | objectives=5/5 result=PASS  |
| `r9i-play02-b` | replay | 0 / 21s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c8f6c7dc60e47434 (1315714 B) | ticks=4801 packets=2400 ships=70 randSyncErr=0 univSyncErr=0 blobSyncErr=0 pktSyncErr=0  |
| `r9i-sound` | sound | 0 / 17s | sound self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS  |
| `r9i-m11-b` | missiontest | 0 / 45s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b2c33be9927d8190 (2036509 B) | objectives=5/5 result=PASS  |
| `r9i-hud` | hud | 0 / 16s | hud self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS  |
| `r9i-rec02` | skirmish | 0 / 25s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | fdff10b842a4e14b (1370436 B) | result=PASS  |
| `r9i-ui` | ui | 0 / 14s | ui self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS  |
| `r9i-fescreens` | fescreens | 0 / 10s | fescreens self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | files=22 loaded=22 screens=195 result=PASS  |
| `r9i-play02i-a` | replay | 0 / 20s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0ddd8d7b439cb902 (1315716 B) | ticks=4801 packets=2400 ships=70 randSyncErr=0 univSyncErr=0 blobSyncErr=0 pktSyncErr=0  |
| `r9i-m4-a` | missiontest | 0 / 59s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ff70ef49d8bb8e0e (5121597 B) | objectives=5/5 result=PASS  |
| `r9i-strings` | strings | 0 / 10s | strings self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | ids=1705 mapback=1705 result=PASS  |
| `r9i-keys` | keys | 0 / 10s | keys self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS  |
| `r9i-play02i-b` | replay | 0 / 20s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0ddd8d7b439cb902 (1315716 B) | ticks=4801 packets=2400 ships=70 randSyncErr=0 univSyncErr=0 blobSyncErr=0 pktSyncErr=0  |
| `r9i-r17-rel` | replay | 0 / 8s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 96d1fd9f7e8d1c2a (2630929 B) | ticks=9601 packets=4800 ships=80 randSyncErr=1918 univSyncErr=3767 blobSyncErr=4746 pktSyncErr=0  |
| `r9i-console` | console | 0 / 14s | console self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS tablerows=486  |
| `r9i-m12-a` | missiontest | 0 / 71s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7ebb356e59cbd2ca (2705630 B) | objectives=3/3 result=PASS  |
| `r9i-build-a` | build | 0 / 23s | build self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8622e0787ba94553 (1722370 B) | result=PASS  |
| `r9i-m4-b` | missiontest | 0 / 60s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ff70ef49d8bb8e0e (5121597 B) | objectives=5/5 result=PASS  |
| `r9i-build-b` | build | 0 / 24s | build self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8622e0787ba94553 (1722370 B) | result=PASS  |
| `r9i-nisparse` | nisparse | 0 / 10s | nisparse self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS  |
| `r9i-toggle` | toggle | 0 / 10s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS  |
| `r9i-toggle-noi` | toggle | 0 / 10s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS  |
| `r9i-m12-b` | missiontest | 0 / 78s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7ebb356e59cbd2ca (2705630 B) | objectives=3/3 result=PASS  |
| `r9i-toggle-non` | toggle | 0 / 10s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS  |
| `r9i-m5-a` | missiontest | 0 / 62s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cc16b744d83abf09 (4031324 B) | objectives=6/6 result=PASS  |
| `r9i-ai-a` | ai | 0 / 16s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cefa7bd984ff3f57 (98768 B) | result=PASS  |
| `r9i-ai-b` | ai | 0 / 16s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cefa7bd984ff3f57 (98768 B) | result=PASS  |
| `r9i-ai-soak` | ai | 0 / 25s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4cd65bfa42cfa6c6 (984370 B) | result=PASS  |
| `r9i-m5-b` | missiontest | 0 / 57s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cc16b744d83abf09 (4031324 B) | objectives=6/6 result=PASS  |
| `r9i-skirmish-lfe2-a` | skirmish | 0 / 17s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 17fd0322a6e1a571 (147967 B) | result=PASS  |
| `r9i-skirmish-lfe2-b` | skirmish | 0 / 18s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 17fd0322a6e1a571 (147967 B) | result=PASS  |
| `r9i-skirmish-armed8` | skirmish | 0 / 18s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f9fb398a3ff83ef4 (147967 B) | result=PASS  |
| `r9i-sp` | sp | 1 / 339s | universe running (univUpdateCounter > 0) | src/SDL/texreg.c (569):Assertion of (reg->nPalettes < 8) failed. | ASan 0; 40 byte(s) leaked in 10 allocation(s). |  |  |
| `r9i-loadgame-a` | loadgame | 0 / 23s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7a893c872f5e910b (886495 B) | objectives=0/1 result=PASS  |
| `r9i-loadgame-b` | loadgame | 0 / 24s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7a893c872f5e910b (886495 B) | objectives=0/1 result=PASS  |
| `r9i-m13-a` | missiontest | 0 / 192s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 9c893019bcf2a3c2 (5350376 B) | objectives=3/3 result=PASS  |
| `r9i-motion-noi` | motion | 0 / 41s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS  |
| `r9i-m6-a` | missiontest | 0 / 145s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ad5fb335d947ff9a (7609886 B) | objectives=7/7 result=PASS  |
| `r9i-nis-noi` | nis | 0 / 43s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS  |
| `r9i-fe` | fe | 124 / 120s | Main_game_screen | none: still running at the 120s timeout | ASan 0; no unsuppressed leak |  |  |
| `r9i-motion-on` | motion | 0 / 40s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS  |
| `r9i-nis-on` | nis | 0 / 47s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS  |
| `r9i-m6-b` | missiontest | 0 / 143s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ff69a95372eee204 (7609886 B) | objectives=7/7 result=PASS  |
| `r9i-m13-b` | missiontest | 0 / 195s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 9c893019bcf2a3c2 (5350376 B) | objectives=3/3 result=PASS  |
| `r9i-m7-a` | missiontest | 0 / 86s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 547c5d2b8eb2dc12 (4082492 B) | objectives=4/4 result=PASS  |
| `r9i-m14-a` | missiontest | 0 / 113s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 95947e05c6cacd08 (5496254 B) | objectives=4/4 result=PASS  |
| `r9i-m7-b` | missiontest | 0 / 81s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 547c5d2b8eb2dc12 (4082492 B) | objectives=4/4 result=PASS  |
| `r9i-m14-b` | missiontest | 0 / 119s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 95947e05c6cacd08 (5496254 B) | objectives=4/4 result=PASS  |
| `r9i-m8-a` | missiontest | 0 / 126s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 935ec3a3dee89438 (7223667 B) | objectives=6/6 result=PASS  |
| `r9i-m8-b` | missiontest | 0 / 126s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 935ec3a3dee89438 (7223667 B) | objectives=6/6 result=PASS  |
| `r9i-m15-a` | missiontest | 0 / 187s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 44c784e32bb946a8 (3795164 B) | objectives=3/3 result=PASS  |
| `r9i-bis-307c3a5-ai-soak` | ai | 0 / 24s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ade5bfd286b85033 (984370 B) | result=PASS  |
| `r9i-m9-a` | missiontest | 0 / 130s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c102e7d44ef1c0b2 (5063294 B) | objectives=6/6 result=PASS  |
| `r9i-bis-9c1801f-ai-soak` | ai | 0 / 26s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8a2d9c31742d515f (984370 B) | result=PASS  |
| `r9i-m15-b` | missiontest | 0 / 204s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 44c784e32bb946a8 (3795164 B) | objectives=3/3 result=PASS  |
| `r9i-m9-b` | missiontest | 0 / 132s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c102e7d44ef1c0b2 (5063294 B) | objectives=6/6 result=PASS  |
| `r9i-bis-noblast-ai-soak` | ai | 0 / 26s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7107ec2997108889 (984370 B) | result=PASS  |
| `r9i-m16-a` | missiontest | 0 / 180s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 04fc7978e3c4ddef (5295272 B) | objectives=6/6 result=PASS  |
| `r9i-m16-b` | missiontest | 0 / 179s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 04fc7978e3c4ddef (5295272 B) | objectives=6/6 result=PASS  |
| `r9i-m17-a` | missiontest | 0 / 162s | mission self-test: the game's end (credits) | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 5b57af440ec15bff (3834034 B) | game end tick=15584 objectives=4/4 result=PASS  |
| `r9i-m17-b` | missiontest | 0 / 158s | mission self-test: the game's end (credits) | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 5b57af440ec15bff (3834034 B) | game end tick=15584 objectives=4/4 result=PASS  |

**Conditions.**

| # | condition | result | met |
|---|---|---|---|
| (a) | the R8 gates hold; the stream re-baselined ONCE against `0d2b76d565105a26`'s content with producer 1's statement | **Mission01 headless**: debug pair `r9i-mission-a/b` byte-identical, sha256 **`553845b65e87f3b5`** (1,476,370 B, 6,001 `T` records, 154 ships), ASan 0; `mission_test.sh` **MISSION TEST PASS** (the perturbed run caught at tick 3000 / `rng10`, the flipped `ships_health` at 4000); the release pair **`ba8ee6e78597c175`** byte-identical and IDENTICAL to the debug stream, its own MISSION TEST PASS; `/noInterp`, `/noNISInterp` and **`/oracleLibmSqrt`** runs all byte-identical to A (the table does not move this stream: no ship flies, decision 57). **Against R8's `0d2b76d565105a26`: `ships_pos` and `univcheck` from tick 0, `ships_rot` from tick 1, every other field IDENTICAL over 6,001 ticks** (per field: rng10, rng6, rng0, rng_all, ships_n, ships_misc, ships_vel, ships_health, missiles, bullets, derelicts, resources, players, dt, total, real, fpcw IDENTICAL) = producer 1's statement (the exe's `qsort` at `formationArrangeOptimum`'s ties; `1028375d20f15d75`'s content). IDENTICAL to every run of the round: `a9r-mission-a`, `a9m3-mission-a`, `a9m2-mission-a`, `r9a2-mission-a`, `r9r-mission-a` (and the release to `r9a2-mission-rel-a`). **sp** `--timeout 340`: the NIS-first start runs Mission01_NIS01 -> Mission01 (`univUpdateCounter` past 1,584 before the jump; the gate 1,248) -> Mission02 -> Mission03 -> Mission04_NIS02 -> Mission04, 39 screenshots, then **`texreg.c (569)` `nPalettes < 8`** at 339 s (rc 1, ASan 0: the known M2-R10 render stop, decision 90); the loop goes past Mission01 (R8: 4,496 ticks of Mission01 at the timeout). **fe**: Main_game_screen (300 file loads), rc 124 at the timeout, ASan 0. **Self-tests**: statics 143 of 145 types (the 2 disabled) twice, meshes 1,758 files 0 bad, textures 12,403 listed / 10,661 registered, oracle 512 ticks 0 sync errors, **release `selftest.sh` SELFTEST PASS** (perturb at 300 `rng10`, flip at 400 `ships_pos`), etg 197 effects, dock PASS, command (sACV#178 done at 285), render PASS, sound PASS, hud PASS, ui (154 selected, 26 commands), fescreens 22 / 22 files, 195 screens PASS, strings **1,705 / 1,705** (1,681 resolved + 24 nostring, 0 wrong), keys 197 rows, console **tablerows=486**, build PASS (pair `8622e0787ba94553`), nisparse 14 NIS PASS, toggle x3 PASS, ai pair `cefa7bd984ff3f57` PASS + the 4,000-tick soak PASS (`4cd65bfa42cfa6c6`, game over as before), skirmish LFE2 pair `17fd0322a6e1a571` (91 ships) + Armed8 `f9fb398a3ff83ef4` PASS, missiontest pair `cc7f22e7129b9471` 6/6, loadgame pair `7a893c872f5e910b` PASS (the KAS save "01 - Hiigara", 682 chunks); motion / nis `/noInterp` **dumps `183681235a3fdc7b` / `13ca70eea998e5de`** = producer 1's and the rung audit's (`r9a2-*`), re-baselined from R8's `0838a011db819920` / `d1e6f6e477906b52` with producer 1's stated physics (the table square root under every frame's flight, the exe's qsort in the start's formations and the NIS's sorted paths; decision 46's guard-rails untouched); flags-on `428fff6c765ab81b` / `6a4c1850799187ac` (R8 `7e4f12d35a662197` / `2f18ba93741ebdc6`). **The three generators `--check` current** (kas2c: every mission file, `KasShared.c`, `CatMissions.h`, `meson.build`; `m2_kasf_table_gen` `KASFuncTable.c`; `m2_save32_gen` `SaveCat32Layouts.c`); `tools/formats/layout32_check.py` OK. **Guard-rail hashes vs R8's**: the table below; every move matches producer 1's section 8 (and the LFE2 start, producer 2's reader); one move inside the round nobody stated (the ai soak's tick 694), bisected and attributed here. UBSan kinds: below | **met** |
| (b) | every mission 01-17 reaches its objectives headlessly (pairs, ASan 0), the game's end, the Tutorial1 leg through Lesson12 as a pair, the warp-start test, the tutorial save's load | the mission table below: **17 / 17 PASS with every objective** (Mission01 6/6 at 11,424 through the chained jump; 02 .. 17 through the warp start), 16 of 17 pairs byte-identical, Mission06's pair equal in every simulation field and different in `rng_all` only from tick 2081 (decision 94's flake), ASan 0 in all 34 runs; every stream IDENTICAL to producer 4's `r9r-mN-a` and the missions-10-17 audit's `a9m3-*` runs (Mission05 and 08 differ from the missions-02-09 audit's at 8535 `ships_health` / 18792 `rng10`: the blast fix and the scuttle, as that audit stated). **The game's end**: Mission17's run reaches `MissionCompleted` -> `singlePlayerMissionCompleteCB` -> the credits (`game end tick=15584`), rc 0. **Tutorial1**: pair `416f819eaa29e3e0` (10,444 ticks, `lessons=12/12 complete=12/12 inOrder=1 nextClicks=11`), IDENTICAL to `a9r-tut-a` and `r9r-tut-a`; the 12 lesson saves written. **Warp start** level 10: pair `fa0dc28b3d712299` PASS (`resources=8000/8000 types=18/18 ships=76/76 (all 97) ether=79 outside=0 tech=20/20`, then `ether=0 outside=79`), IDENTICAL to `a9m3-warp10-a` / `r9r-warp10-a`. **The tutorial save's load**: "LESSON 11 - Researching and Upgrading" from the tutorial run's own `SavedGames/Training` loads (`tutorial=1 mission=1 tick=8360 ships=30`) and runs 3,600 ticks, pair `a6f4173d3188ef18` PASS (the audit's reference loaded the wall-clock leg's save at tick 13,496, so no content comparison) | **met** |
| (c) | the oracle's rung from the clean build: 10-17 IDENTICAL beyond 247, 10-18 beyond 155, or the next divergence localised; the physics doc and its audit | pairs `r9i-r17-a/b` **`3fb2f34e7e5baf9f`** (2,630,929 B, 9,601 ticks, 4,800 packets) and `r9i-r18-a/b` **`abaa0ad279c50bec`** (3,527 ticks, 1,763 packets) byte-identical, ASan 0; against the GOG streams with the rung's field list (`rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real`): **`rng6` at 161 in both** (the playback skips the AI: excluded, decision 47) and **`real` at tick 2 in 10-18** (decision 60); without them **10-17 IDENTICAL through 1921, first divergent tick 1922 `ships_vel`; 10-18 IDENTICAL through 1720, first divergent tick 1721 `bullets`** (`bullets_n=2` in GOG, 0 here); with `/oracleLibmSqrt` 177 `ships_vel` / 118 `ships_pos` + `ships_vel` (the square root itself); every per-field tick equals docs/m2-r9-oracle-rung-physics.md section 6 / 8 and its audit's table ("Oracle mode A" below). The release replay `r9i-r17-rel` (9 s) IDENTICAL to the debug one on the simulation list plus `ships_rot` and `players` (`rng_all` differs from 6377, R8's from 7577: the frame-bound non-game draw). Both replays IDENTICAL in every field to the rung audit's (`r9a2-r17-a` / `-r18-a`, the libm ones too), producer 4's (`r9r-r17` / `-r18`) and producer 1's audited runs (`r9a-*`) | **met** (the rung beyond 247 / 155; the next divergences localised and owned by M2-R10 physics) |
| (d) | the recorder: the 4 `CAT-TODO(stage 5 recorder)` closed; the hwi-engine recordings' replay pairs byte-identical and IDENTICAL to their recording runs; the mode-B rung reported | `grep -rn 'stage 5 recorder' src/` 0. `hwirec01` (Armed8) replays as the pair `r9i-play01-a/b` **`85fddfcf8b41facb`** (4,801 ticks, 2,400 packets, 0 sync errors) and `hwirec02` (LFE2) as `r9i-play02-a/b` **`c8f6c7dc60e47434`**, each **IDENTICAL to its record run** (`r9r-rec01` / `r9r-rec02`, ticks 1-4800, the recorder's list `rng10,ships_n,ships_misc,ships_pos,ships_vel,ships_health,ships_rot,missiles,bullets,derelicts,resources,univcheck,players,dt,total,real`; all fields: `rng0` and `rng_all` from tick 2 only, decision 95) and to producer 4's and the audit's playbacks (`r9r-play0N-a`, `a9r-play0N-a`). **This build records too**: `r9i-rec02` (the skirmish self-test on LFE2, `--record hwirec02i --record-stop 4800`) wrote `hwirec02i` (841,152 B, `5607671c8d975a33`) and `hwirec02i.pkts` (88,798 B, `8bff562e591312fd`); its stream IDENTICAL to `r9r-rec02` and `a9r-rec02` (5,001 ticks, every field); its playback pair `r9i-play02i-a/b` **`0ddd8d7b439cb902`** IDENTICAL to its record run and to `r9i-play02-a` (the producer's recording's playback). `recording_compare.py` against producer 4's `hwirec02`: SAME FORMAT (1,892 chunks, 2,400 packets, the 29 pre-game chunks, version 0x10ad0027), `--chunks` 1,473 of 1,892 byte-identical and every differing dword in a pointer slot (stale pointers) or the 3 unmapped BASIC dwords; the `.pkts`: the same 2,400 record checksums, frames and 41 commands, and 1,200 packets differ in the 2-byte tail after `numberOfCommands` (+0x1a: the struct's padding, copied uninitialised; the probe's "header tail set in 1306" in both) — a note for M2-R10 (zero it, or state the exe's). **Mode B** (the GOG playback): NOT run by an agent (never the GOG game); no `original/oracle/hwirec*-play-*` directory exists, so the user has not run it yet: the user's step with producer 4's exact commands ("What the user must decide" 4) | **met** as far as an agent can take it; the mode-B rung is the user's |
| (e) | `grep -rn -E 'CAT-(TODO\|PROVISIONAL)\([^)]*(M2-R[89]\|stage 6)' src/` empty; the rest counted by owner | empty (0 lines). `/bin/grep -rn CAT-TODO src/`: **34 lines, 25 owner tags**: stage 5 23 (plain 8: CommandWrap.c's point / mimic / upgrade / special-target senders and the external-build packet, NetCheck.c's `recPackRecordSaveHeader`, UnivUpdate.c's dock words; multiplayer options 8: PopupMenu.c x2, ResearchGUI.c, Salvage.c, ShipSelect.c, UnivUpdate.c, mainrgn.c, MultiplayerGame.c's `mgUnitCapsEnabled`; LAN 6: CommandNetwork.c, HorseRace.c, LevelLoad.c, NetCheck.c, UnivUpdate.c, utility.c; multiplayer 1: SaveGame.c's 0x2B0 layout), **M2-R10 physics 2** (`aiuFindDistanceSquared`, `tacticsGetShipsMaxVelocity`); 9 other lines mention closed tags in prose. 5 `CAT-PROVISIONAL` (unchanged). (The shell's `grep` is a ugrep wrapper with `-I`, which skips `MultiplayerGame.c` as binary (ISO-8859 text) and under-counts by one: count with `/bin/grep`) | **met** |

**Guard-rail streams against R8's** (`hwo_compare.py`, every field; the first divergent tick per field; then the same
stream against the last audits' runs of this round):

| stream | R8 (`r8i-*`) | this step | moved against R8 (first tick per field) | producer statement | against the last audits' |
|---|---|---|---|---|---|
| ai, 400 | `df95f21bdb82abbf` | `cefa7bd984ff3f57` (a = b) | ships_pos 0, univcheck 0, ships_rot 1, ships_vel 17; nothing else | producer 1 section 8: the start's formations (qsort) 0 / 1, the table 17 | IDENTICAL to `a9r-ai`, `r9a2-ai-a` |
| ai soak, 4,000 | `a2b04b8998bc6818` | `4cd65bfa42cfa6c6` | ships_pos 0, ships_rot 1, ships_vel 17, bullets 413, rng10 / rng_all / ships_health 471, missiles 477, ships_n / ships_misc 694, rng0 698, rng6 706, players 849, resources 2013 | producer 1: ships_pos 0, ships_rot 1, ships_vel 17, bullets 413, rng10 471, ... (the table + the blob attacking flag); **and one more move inside the round**: against the rung audit's `r9a2-ai-soak` this stream first differs at **694 `ships_health`** (then rng_all 1122 (the flake), bullets 1281, rng10 / pos / vel / rot 1290, ...; derelicts, resources, dt, total, real, fpcw IDENTICAL). Bisected by this step: the soak of the tree at `307c3a5` (producer 2's audit) is IDENTICAL to the rung audit's in `ships_health`, the tree at `9c1801f` (producer 3's) is IDENTICAL to this step's, and `9c1801f` with the one-line `SOF_CatShipPart` test of `MakeExplosionRockBumpables` removed is IDENTICAL to the rung audit's again: **the move is producer 3's ship-part blast fix** (the first ship death at tick 694, `ships_n` 694; the same fix moved Mission05 at 8535), unstated for this stream because no producer or audit after the rung re-ran the 4,000-tick soak (the 400-tick ai pair sees no death) | the rung audit's `r9a2-ai-soak` (-2, -3, -prodbin agree with each other except `rng_all` from 1115): 694 `ships_health` as above |
| skirmish LFE2, 600 | `1b648d098a7659c5` | `17fd0322a6e1a571` (a = b) | ships_n (89 -> 91), ships_misc, ships_pos, ships_vel, ships_health, ships_rot, univcheck 0; rng_all 1; rng6 10 | producer 2 (the two dSlipgateLarge of the Slipgate reader: tick 0) over producer 1's table move at 321 | IDENTICAL to `a9r-lfe2`, `a9m3-lfe2` |
| skirmish Armed8, 600 | `898f68a5fc352ba2` | `f9fb398a3ff83ef4` | ships_vel / ships_rot 322, ships_pos 323, univcheck 357; nothing else | producer 1: the table only | IDENTICAL to `a9m3-armed8`, `r9a2-skirmish-armed8` |
| build, 7,000 | `b7851d51d1c21bfb` | `8622e0787ba94553` (a = b) | ships_pos 0, univcheck 0, ships_rot 1, ships_vel 4206, rng_all 4319 | producer 1: qsort 0 / 1, the table 4206, rng_all 4319 | IDENTICAL to `a9m3-build`, `r9a2-build-a` |
| missiontest (Mission01), 11,425 | `becb49b579aba435` | `cc7f22e7129b9471` (a = b, 6/6) | ships_misc / ships_pos / ships_rot / ships_health / resources 0, ships_vel 2, rng6 / rng_all 5, univcheck 39, bullets 1783, rng10 1785, ships_n 5580, players 7517, rng0 8134 | producer 1: the opening NIS's sorted paths and events and the start's formations (qsort) at 0, the table from 2 | IDENTICAL to `a9r-mt1-a`, `a9m3-mt1`, `r9a2-missiontest-a` |
| loadgame ("01 - Hiigara"), 3,600 | `68b0075ad1e20bb7` | `7a893c872f5e910b` (a = b) | ships_pos / ships_vel / ships_rot / bullets 1985 (the first tick after the load), univcheck 1987, rng_all 2036, rng10 2206, ships_misc 3480, ships_health 4753, ships_n / players 4962, rng6 4966 | producer 1: the table from 1985 | IDENTICAL to `a9m3-loadgame`, `r9a2-loadgame-a` |
| replay 10-17 | `r8i-r17-off-a` | `3fb2f34e7e5baf9f` | ships_rot 171 (excluded), then the rung's fields from 247 on | the rung (producer 1 section 6) | IDENTICAL to `r9a2-r17-a`, `r9r-r17`, `r9a-r17-a` (every field) |
| replay 10-18 | `r8i-r18-off-a` | `abaa0ad279c50bec` | ships_rot 105 (excluded), then from 155 on | the rung | IDENTICAL to `r9a2-r18-a`, `r9r-r18`, `r9a-r18-a` |
| motion / nis `/noInterp` dumps | `0838a011db819920` / `d1e6f6e477906b52` | `183681235a3fdc7b` / `13ca70eea998e5de` | the whole file (the camera pose and the NIS objects' screen positions) | producer 1: the table square root under every frame's flight, the exe's qsort in the formations and the NIS's sorted paths / events; decision 46 untouched | equal to `r9a2-motion-noi` / `r9a2-nis-noi` |

**The missions** (the per-mission self-test, debug build, `--mode missiontest --mission N --mission-test-ticks 90000
--timeout 3600`; Mission01 without `--mission`; every run ends 32 ticks after the next level loads, Mission17 at the
credits):

| mission | objectives | done tick | hash (a) | pair | against producer 4 / the audits | entry; the jump |
|---|---|---|---|---|---|---|
| 01 | 6 / 6 PASS | 11,424 | `cc7f22e7129b9471` | byte-identical | IDENTICAL `a9r-mt1-a`, `a9m3-mt1`, `r9a2-missiontest-a` | the NIS-first start (n01, the nislet); the chained jump to Mission02 at 11,392 |
| 02 | 6 / 6 PASS | 25,136 | `d71408aacb2a6a5b` | byte-identical | IDENTICAL `r9r-m2-a`, `a9m3-m2`, `a9m2-m2-a` | warp; letterbox jump at 24,976 -> Mission03 |
| 03 | 5 / 5 PASS | 17,825 | `f55080e8277964cf` | byte-identical | IDENTICAL `r9r-m3-a`, `a9m3-m3`, `a9m2-m3-a` | warp; roll call, Go Now at 17,472 -> Mission04_NIS02 |
| 04 | 5 / 5 PASS | 20,816 | `ff70ef49d8bb8e0e` | byte-identical | IDENTICAL `r9r-m4-a`, `a9m3-m4`, `a9m2-m4-a` | warp (at once); roll call, Go Now at 20,464 -> Mission05 |
| 05 | 6 / 6 PASS | 16,384 | `cc16b744d83abf09` | byte-identical | IDENTICAL `r9r-m5-a`, `a9m3-m5`, `a9r-m5-a`; `a9m2-m5-a` at 8535 `ships_health` (the blast fix, as stated) | warp (at once); letterbox jump at 16,224 -> Mission06_NIS04 |
| 06 | 7 / 7 PASS | 30,931 | `ad5fb335d947ff9a` (b `ff69a95372eee204`) | `rng_all` only from 2081; every other field IDENTICAL | IDENTICAL `r9r-m6-a`, `a9m3-m6`, `a9m2-m6-a` | warp (at once); roll call, Go Now at 30,576 -> Mission07_NIS05 |
| 07 | 4 / 4 PASS | 16,592 | `547c5d2b8eb2dc12` | byte-identical | IDENTICAL `r9r-m7-a`, `a9m3-m7`, `a9m2-m7-a` | warp (at once); letterbox jump at 16,432 -> Mission08 |
| 08 | 6 / 6 PASS | 29,361 | `935ec3a3dee89438` | byte-identical | IDENTICAL `r9r-m8-a`, `a9m3-m8`; `a9m2-m8-a` at 18792 `rng10` (the scuttle, as stated) | warp (at once); roll call, Go Now at 29,008 -> Mission09 |
| 09 | 6 / 6 PASS | 20,579 | `c102e7d44ef1c0b2` | byte-identical | IDENTICAL `r9r-m9-a`, `a9m3-m9`, `a9m2-m9-a` | warp; roll call under fire, **Emergency Hyperspace** at 20,224 -> Mission10_NIS06 |
| 10 | 5 / 5 PASS | 21,938 | `2fcca6671e2fcec1` | byte-identical | IDENTICAL `r9r-m10-a`, `a9m3-m10-a` | warp; roll call, Go Now at 21,584 -> Mission11 |
| 11 | 5 / 5 PASS | 8,275 | `b2c33be9927d8190` | byte-identical | IDENTICAL `r9r-m11-a`, `a9m3-m11-a` | warp; roll call, Go Now at 7,920 -> Mission12_NIS07 |
| 12 | 3 / 3 PASS | 10,995 | `7ebb356e59cbd2ca` | byte-identical | IDENTICAL `r9r-m12-a`, `a9m3-m12-a` | warp; roll call, Go Now at 10,640 -> Mission13 |
| 13 | 3 / 3 PASS | 21,746 | `9c893019bcf2a3c2` | byte-identical | IDENTICAL `r9r-m13-a`, `a9m3-m13-a`, `a9r-m13-a` | warp; roll call, Go Now at 21,392 -> Mission14_NIS08 |
| 14 | 4 / 4 PASS | 22,339 | `95947e05c6cacd08` | byte-identical | IDENTICAL `r9r-m14-a`, `a9m3-m14-a` | warp; roll call, Go Now at 21,984 -> Mission15_NIS09 |
| 15 | 3 / 3 PASS | 15,424 | `44c784e32bb946a8` | byte-identical | IDENTICAL `r9r-m15-a`, `a9m3-m15-a` | warp; letterbox jump at 15,264 -> Mission16_NIS10 |
| 16 | 6 / 6 PASS | 21,522 | `04fc7978e3c4ddef` | byte-identical | IDENTICAL `r9r-m16-a`, `a9m3-m16-a` | warp; roll call, Go Now at 21,168 -> Mission17_NIS11 |
| 17 | 4 / 4 PASS | 15,584 | `5b57af440ec15bff` | byte-identical | IDENTICAL `r9r-m17-a`, `a9m3-m17-a` | warp; `MissionCompleted` at 15,584 -> `singlePlayerMissionCompleteCB` -> the quit to the credits (`game end tick=15584 credits=Credits-GameEnd.cred`); no next level |

**UBSan kinds left** (the union over the 102 runs; all non-fatal; none is `fescreen`, `PATCH`, `fquant.c`,
`AIShip.c` or `Tutor.c`):
- the ETG opcode kinds of GoK's packed effect bytecode (`etgdecision`, `etgconditional`, `etgfunctioncall`, `etgbranch`,
  `etgvariablecopy`: `ETG.c` 3368-6121) and `struct particle` (`Particle.c` 521-3505, `ETG.c` 7134-7161) in 84 runs
  (every Mission01, skirmish, mission, replay, etg, render, hud and sp run), as in R8;
- `BTG.c:389` / `:392` (misaligned `short` loads) and `BTG.c:865` (a misaligned `udword` load) in 83 runs (R8's 388 /
  391 / 864: the file moved by one line);
- `Select.c:1153` / `:1154` signed overflow `N - -2147483648` (`selCircleCompute` from `rndShipVisibleUsingCoordSys`:
  a screen coordinate from an out-of-range float) in 11 runs: sp and the Mission06 / 12 / 14 / 15 / 16 pairs (the rung
  audit's finding; M2-R10);
- **`FOW.c:296-437` signed overflow `-2147483648 - 2`** (18 reports: `fowActivateCube` / `fowDeactivateCube` / `fowCubeHasUnvisited`'s cell boxes (lines 296 / 349 / 435, from the W wrappers' world-to-cell conversion) `x0 = x - r` with a
  cell coordinate of `INT_MIN`, the x86 "integer indefinite" of an out-of-range float position, then clamped) in the
  two Mission17 runs only, not noted by anyone before (producer 4's `r9r-m17-a` and the audit's `a9m3-m17-a/b` carry it
  too); non-fatal; M2-R10 (which object's position; the exe's `_ftol` gives the same `0x80000000`);
- the recorder's packet writes: `CommandNetwork.c` 2064-2229 (`struct HWPacketHeader`, 6) and 1070-1076
  (`struct NetMiscCommand`, 4), `Queue.c` 105-196 (6 misaligned `udword` loads / stores) in the one record run
  (`r9i-rec02`: the exe's packed packet layouts, new this round; M2-R10 may align the copies);
- `stb_image_write.h:1263` (1, sp only: the screenshot writer), as in R8.
**Gone since R8**: `Subtitle.c:1144` (producer 4's fix: 0 runs report it). **New since R8**: `Select.c:1153` / `:1154`
(the rung audit's finding: sp and the Mission06 / 12 / 14 / 15 self-tests; M2-R10 render), the recorder's packet writes
in `CommandNetwork.c` and `Queue.c` (the record run only: the exe's packed packet layouts; M2-R10 may align the copies).

**Harness facts for the next round**: the chains ran 5 wide under a load of 14-19 with no lost summary (the audits' losses
came from editing `tools/m2_harness.sh` while a run was in flight; nobody edited it during this step); the `missiontest`
runs take 55-190 s each in the debug build (Mission13 and 15 the longest), the Tutorial1 leg 44 s, a replay 50 / 27 s
(release 9 s), the `.hwo` of a mission run 2-7.6 MB; `hwo_compare.py` wants `--from-tick 1` when one side has a tick-0
record (a record run) and the other has not (a playback), else it prints no verdict.

## The four ports

| # | item | what landed | DONE WHEN | audit | commit |
|---|---|---|---|---|---|
| 1 | the oracle's next rung, physics ([report](m2-r9-oracle-rung-physics.md), [audit](audits/m2-r9-oracle-rung-physics.md)) | **The table square root as the engine default** (`FastMath.c` `catFsqrtTable` TRUE, the HWI-EXPERIMENT switch inverted to `/oracleLibmSqrt`, `/oracleTableFsqrt` gone; every stream's move by the table alone measured once: the headless Mission01 stream and the replays do not move, the ai / skirmish / build / missiontest / loadgame streams and the two dumps do). **The rung 247 -> 1922 (10-17) and 155 -> 1721 (10-18)**, each step the exe's machine code at the tick the dump and `hwo_invert.py` pointed at: the avoidance takes the AVOIDED object's collision size first and adds the ship's unrounded (`aishipFlyToPointAvoidingObjsFunc` 0x00414610; the too-close compare on the unrounded root); **the exe's CRT `qsort` 0x00671060** (`catQsort`, `CatSort.c`: the median to `lo`, the two-pointer partition, `shortsort` 0x006712a0, the byte `swap`) at the exe's 14 call sites (Blobs.c, `formationArrangeOptimum` 0x00481200, `nisLoad`, `animLoadNISScript`, `btgZip`, `selSelectionCompare`, the title lists, `cat_smSensorRangeBlobsDraw`, `trRegistryRefresh`), so sort ties come out as in the exe; ships in the collision blobs by their **collision position** (`bobFindNearestBlobToObject` 0x00425cb0, `bobUpdateObjsInBlobCollInfo` 0x00426050, `bobUpdateExtraCollBobInfo`, `bobAddObjToSpecificBlob`; `catBlobObjPosition` / `catBlobDistSqD`) and the blob attacking flag (`shipisattacking` AND `catAttackStarted`); `ObjectsCollided` 0x005456d0's x87 shape (the audit: the UNROUNDED `1/t` for x); `collCheckBigShipSmallShipColl` 0x0043b460's clamped-host test and distance shape; `cat_dockFlyOutOfCone` 0x004650d0; `cat_gunFixedGunAimedAtTarget` 0x0048cd40; **`hyperspaceCost` 0x00443c30 returns the double** to its five `_ftol` callers (the R8 tag closed); `protectShip` 0x004411c0 / `protectShipsAvg` 0x004413f0; `catFmathSqrtDouble`'s NaN case (the audit); the permanent dump's 'O' / 'V fly' / 'B' lines. The next divergences localised: 10-17 tick 1922 `ships_vel` ship 41 (a bRecon attacking: v.z one ulp, the attack point or the coordsys of the turning ship), 10-18 tick 1721 `bullets` (GOG fires 19 ticks before us: a firing decision). `aiuFindDistanceSquared`'s double return and `tacticsGetShipsMaxVelocity`'s unrounded product tagged `CAT-TODO(M2-R10 physics)`. **The headless Mission01 stream moved once, by the exe's qsort** (`1028375d20f15d75`: `ships_pos` / `univcheck` from tick 0, `ships_rot` from tick 1, the equal-size ships' formation slots; proved with a C-library-qsort experiment build). 11 IDENTICAL found modified, 34 reviews | met: both replay pairs byte-identical, ASan 0; the 10-17 replay IDENTICAL beyond 247 (through 1921: ship 40's velocity at 247 is GOG's `(420767c8, 4300eec6, 429fe606)`) and the 10-18 beyond 155 (through 1720: ship 28's v.z `4234afe1`), the next divergences localised and owned (M2-R10 physics); with `/oracleLibmSqrt` 177 / 118 (the square root itself); every stream's move stated before and after; the 4,000-tick soak ASan 0, no fatal | PASS-WITH-FIXES, **0.91** (50/55). Fixes: `ObjectsCollided`'s unrounded `1/t` (the same words at 1/16 s: no stream moved), `fsqrt`'s NaN argument returns 0.0 as the exe's `test ah,0x40`, 3 delta-map notes (`hyperspaceCost`'s contradictory text, the stale "not ported" notes of `fsqrt` / `fmathInit`), 3 report statements (the ulp sign, the sp UBSan kind `Select.c:1153` / `1154`, the stored `1/t`); +1 tag (`tacticsGetShipsMaxVelocity`, Physics.c); the ai soak's `rng_all` under load noted | `64f49a9` |
| 2 | missions 02-09 ([report](m2-r9-missions-02-09.md), [audit](audits/m2-r9-missions-02-09.md)) | **`tools/m2_kas2c_gen.py` generates Mission02 .. Mission09** (`DEFAULT_SCRIPTS`; **891 bodies verified** against the machine code, 10,026 all-constant calls' arguments equal; the new reading rules: the `aisTeams` garble resolved to the absolute address and its hash / string, MSVC's division magic, raw Ghidra literals resolved by their bytes, switch data and in-function data excluded, the localised table form `LSTRING_x[strCurLanguage]`; 4 OVERRIDES, each HAND-EDIT and verified: two Mission05 message hashes Ghidra took for an address, Mission08's dead-code `kasJump`, Mission09's folded `kasfAssert`); Mission01.c byte-unchanged, Tutorial1.c's Lesson12 line now the exe's localised table (the earlier output dereferenced exe address 0x008cd960); `src/Missions/meson.build` written by the generator. **The `catKasScripts` rows 02 .. 09**; **the "Mine" 0x004a3f20 and "Slipgate" `cat_scriptSetSlipgateCB` 0x004a1020 mission-sphere readers** (the mine of `mineStaticInfos[RACE_Derelict]`, the slipgate pairing through `catSlipgatePending` 0x00c362dc) in the table 0x00883a48, `levelPreInit`'s reset. 96 kasf the new scripts call compared with the exe: 3 fixed (`kasfSpecialOn`'s return, `kasfVectorAdd` / `kasfVectorCopy` only overwrite an existing point). **Six bugs the runs found, fixed**: the tweak table's keys (`twRepairRate[CLASS_*]` / `twSalvageDamageLimit[CLASS_*]` stringified as `[3]` since M2-R4, so every repair rate was 0: HWI-FIX), `AllowPlayerToResearch`'s -1 (CAT-FIX), `bobUpdateExtraCollBobInfo`'s GoK-only "no missile in a blob" assertion, `cat_beastGetAttackStaticInfo`'s 64-bit overlay (HWI-FIX), `dbgFatal`'s ASan stack trace. **`/catMissionTestMission N` + harness `--mission N`**: each mission starts through the exe's warp start (`cat_spWarpToLevel` 0x0051b9c0 with Warp.script's level N record), its Objectives FSM's table (`catMtMissions[]`) and a scripted player per mission (the command layer, the managers, `ShipsDamage`); the judge (created in order, each completed, the jump, the roll call's enabled button, the next level loaded); the held objectives (Mission04 Destroy, Mission09 Protect) complete through `ObjectivesDestroyAll`. The flows exercised: the at-once arrival (04 .. 08), the roll call with Go Now (03, 04, 06, 08) and Emergency Hyperspace under fire (09), the letterbox jumps (02 -> 03, 05 -> 06_NIS04, 07 -> 08), the NIS staging levels (n02, n04, n05, n06); the screenshot gate opened to every generated mission. 50 reviews. LiveFireExercise2's stream moved (two dSlipgateLarge: 89 -> 91 ships) | met: missions 02-09 each reach every objective in order headlessly, 8 byte-identical pairs, ASan 0 (02 6/6, 03 5/5, 04 5/5, 05 6/6, 06 7/7, 07 4/4, 08 6/6, 09 6/6); the generator `--check` current and reproduced from scratch byte for byte; the Mission01 stream IDENTICAL (`be3e5030cebbea0b` = `1028375d20f15d75`'s content) | PASS-WITH-FIXES, **0.94** (64/68). Fixes: 3 report statements (a fix count, a section reference, the roll-call counts), the harness header's missiontest line. Open: the sp run now enters Mission02 .. 04 and stops on HW1's `texreg.c (569)` `nPalettes < 8` assertion in Mission04 (the exe's `trTextureRegister` 0x00585760 appends a ninth colour scheme without a bound into a 64-byte block; ASan 0): M2-R10 render | `307c3a5` |
| 3 | missions 10-17 ([report](m2-r9-missions-10-17.md), [audit](audits/m2-r9-missions-10-17.md)) | **The generator generates all seventeen missions and Tutorial1**: **2,085 bodies verified** (every kas2c mission function `tools/m2_stage_counts.py` counts; 25,279 all-constant calls' arguments equal; 4 more reading rules: `BYTE_` operands, hex divisors, `s_<name>_<addr> + N` hashes, a localised table split over two lines; 11 more OVERRIDES addresses = 16 patches: the folded assert stub `kasfAssert` on 0x0049ae30 in the Sentinels FSMs of M11 / 14 / 16 / 17 and M13's SupportWave2, 0x0062dfe0's `void *` label and this-argument; 15 addresses / 20 patches in all, every output reproduced from scratch byte for byte); Mission01 .. 09 and Tutorial1 byte-unchanged. **The `catKasScripts` rows 10 .. 17** (the last `CAT-TODO(M2-R9 part B)` closed). **The end of the game**: Mission17's script calls `MissionCompleted` 0x00491c70 -> `singlePlayerMissionCompleteCB` 0x0051be40 -> `utyGameQuitToMain` 0x005ab6b0's credits (`psScreenStart("Credits-GameEnd.cred")`), reached headlessly. **Harness mode `warpstart`** (`--warp-level N`: a test copy of the install's warp.script with `WarpEnabled 1` in the run's own data dir, the level's record checked against the fleet in the ether and after the arrival), **loadgame `--training` / `--save-src`** (a tutorial lesson's save as the front end's tutorial load does), `/catMissionTestTrace`. **The Tutorial1 leg through Lesson12** (every scripted event through the game's own callbacks: the Next button, `cat_kbCommandExecute` keys, `tutGameMessage`, `rmResearchItem` / `rmExitMenu`, `mrRetire` / `mrScuttle`, the command layer; the engine's own run writes the 12 lesson saves; "LESSON 11" loads). **Seven port fixes at the exe's bodies**: `scriptStringToBool` 0x00525c40 (`'1'..'9'` TRUE: a MODIFIED row never ported), `tutGameSentMessage` 0x00533e80 (only reads the queue), `catKbFocus` / chase 0x0054f2c0 (a `MaxSelection` local: an ASan stack overflow), `cat_bigGunProcessFireCommand` 0x005bbe80 (the charge effect held across `etgEffectDelete`), `MakeExplosionRockBumpables` 0x00545f00 (no blast damage to a `SOF_CatShipPart`: an endless recursion), `spHyperspaceSelectionOut` 0x00518940 (the clamp host's NULL test, `midlevelHyperspaceOut` cleared), the generator's split table. 42 kasf compared (all as ported), 20 reviews. The later missions' screens and flows exercised: the big gun (M10, M16), the Bentusi (M11's repair, M15's slipgate block), the pirates (M10), the Beast (M11, 13, 14, 16, 17: the Naggarok, the nomad moon's emitters, the EMP), the mimic's stealth (M14), the capture (M14), the roll call (M10-14, 16), the letterbox jump (M15); the level objects: M10 4 mines, M12 26, M14 32 mines + 6 slipgates, M15 2 slipgates | met: missions 10-17 each reach every objective in order, 8 byte-identical pairs, ASan 0 (10 5/5, 11 5/5, 12 3/3, 13 3/3, 14 4/4, 15 3/3, 16 6/6, 17 4/4 + the game's end at tick 15,584); the warp start PASS (level 10, a pair); the Tutorial1 leg 12/12 PASS; the lesson save loads (a pair); the Mission01 stream IDENTICAL (`dc2fb8ce71c0d26b`); missions 01-09 rerun: 01 / 02 / 03 / 04 / 07 / 09 IDENTICAL to producer 2's, 05 moved at 8535 `ships_health` (the ship-part blast fix) and 08 at 18792 `rng10` (the test player's scuttle), both verified at their cause | PASS-WITH-FIXES, **0.97** (73/75). Fixes: a report statement (Mission12's unreachable WorkerReturn: its SecurityTeam stays empty), a stale source comment. Findings, not fixed: `animBinkPlay` is GoK's stub (no animatic plays between missions; `animAviPlay` is 0x00421860's port), Mission06's rare `rng_all`-only divergence (2 of 4 runs; every game field equal), the tutorial stream not a pair on the wall clock (closed by producer 4) | `9c1801f` |
| 4 | the recorder's record side (stage 5 reduced) and the test / tooling leftovers ([report](m2-r9-recorder-leftovers.md), [audit](audits/m2-r9-recorder-leftovers.md)) | **The 4 `CAT-TODO(stage 5 recorder)` sites closed at the exe's bodies**: `univFindNearestResource` 0x00549c50's `recordPackets` test, `cat_saveSavePlayerFOW` 0x004ff6b0's skip under `recordingStartSave` 0x00a578f8, `SaveUniverse` 0x005034c0's four delete passes, `clSendMove` 0x0044eb20 / `clSendMpHyperspace` 0x0044ed00 / `clSendFormation` 0x0044f690 in the exe's layouts (+ `clWrapMove` / `clWrapFormation`'s path and level: two `CAT-TODO(stage 5)` closed); `recPackInGameStartCBSafeToStart` 0x004cbbb0 in the exe's order; the recorder's files in the user settings path (HWI-FIX `recPackDeleteFile`). **hwi-engine records in cataclysm.exe's own format**: the recording's save at version 0x10ad0027 through the new **SaveCat32 writer** (`saveCat32ConvertBack` inverting every generated op, `saveCat32SaveStruct` / `Array` / `Ship` with the madanim block's spline offsets, `aitCat32SaveTeam`'s 0x280-byte team, every struct-chunk saver routed through it) and the `.pkts` file; `/oracleRecord <name>` / `/oracleRecordStop <tick>` (harness `--record` / `--record-stop`), `/oracleResave`; **`tools/formats/pkts_probe.py`, `tools/formats/recording_compare.py`**. Two recordings made by the skirmish self-test (`hwirec01` Armed8, `hwirec02` LiveFireExercise2; 4,800 ticks / 2,400 packets) replay byte-identical and **IDENTICAL to their record runs** in every simulation field including `ships_rot`, `players` and `real` (`rng0` / `rng6` / `rng_all` excluded: the computer player does not run in a playback, the DLL splits stream 0 too); the format matches the three GOG sessions field by field (`SAME FORMAT`); the writer's byte check on GOG's own saves (`/oracleResave`: 3,377 chunks, the same structure and byte count, 1,723 chunks byte-identical, every other differing dword classified as a stale pointer, a capacity, a function pointer or state a load resets); the user's GOG playback steps (the mode-B rung, section 3.6). **`Subtitle.c:1144` closed** (CAT-FIX: an actor past the 16-theme table, the location cards' 0x70, starts from `STT_LocationCard`; the exe forms the address and never reads it). **The Tutorial1 leg is a byte-identical pair**: the virtual audio clock (`/catVirtualAudio`, set by `/catMissionTestTutorial`: no device, no stream thread, `soundVirtualAdvance` mixes 22,050 samples per second of task time in 256-sample blocks; the narration takes its game-time length, the leg ends at 10,444 ticks). The mission self-test's combat deferred with a measured reason (37 armed ships attacking 14 bombers for 1,600 ticks killed none: M2-R10 test). **Reviews**: the 123 stage-4 IDENTICAL bodies compared (all the same; the ICF folds noted), 31 of the unflagged rows sampled (2 found modified and ported: `RegisterAllSpaceObjs` 0x004fe470 / `RegisterAllSpaceObjsInsideMe` 0x004fe400; 3 tagged for stage 5: `recPackRecordSaveHeader`, `KeepAliveDropPlayerCB`, `mgUnitCapsEnabled`; 15 found IDENTICAL), 42 notes on edited rows: 165 new entries | met: `grep` 0 for the recorder tag; the recordings replay as pairs IDENTICAL to their record runs over 4,800 ticks; the GOG playback is the user's step with the exact commands; the tutorial leg a pair; no `Subtitle.c` report in any run; no stream moved (the Mission01 pair `fd82fafbcd9011fc` IDENTICAL to `dc2fb8ce71c0d26b`; every mission, the warp start, loadgame, ai, build, both skirmish streams and both rung replays IDENTICAL to the previous audits') | PASS-WITH-FIXES, **0.91** (62/68). Fixes: a review row (`recPackRecordInit`), 3 comments (HS.c's wrong address, OracleDump.c and the harness's "cwd"), 3 report notes (the sync checksum's lag: GOG's packetnum = frame - 8, ours frame - 1, the same ring mechanism; the tutorial move's first tick 4058 `rng0`; the review count) | `9574c9b` |

**Round size** (`git diff --stat 54dcbb2..9574c9b`, the round base to the last audit's checkpoint): 112 tracked files
changed, +99,807 / −1,097 lines (the sixteen generated missions are most of it); `src/` alone: 94 files, +94,573 / −515,
with **384 added lines carrying `CAT`**; 18 new `src/` files (`Game/CatSort.c/.h`, `Missions/SinglePlayer/Mission02.c`
.. `Mission17.c`), 0 deleted. The tree now carries 4,422 `CAT:` lines (R8: 4,394), 34 lines carrying `CAT-TODO` (38; 25
owner tags: stage 5 8, stage 5 multiplayer options 8, stage 5 LAN 6, stage 5 multiplayer 1, M2-R10 physics 2), 5
`CAT-PROVISIONAL` (5), 13 `CAT-DORMANT` (13), **166 `CAT-FIX`** (163), 49 `HWI-ORACLE` (41), 441
`HWI-FEATURE(motion-interp)`, 132 `HWI-TEST` (94), 27 `HWI-FIX` (21), 6 `HWI-EXPERIMENT` (4). Tools: `tools/m2_kas2c_gen.py`
(the part-B rules, 15 OVERRIDES addresses), `tools/m2_harness.sh` (mode `warpstart`, `--mission`, `--warp-level`,
`--training`, `--save-src`, `--record`, `--record-stop`: 29 modes), new `tools/formats/pkts_probe.py` and
`tools/formats/recording_compare.py`, `tools/play.sh` (the user's attended launcher, `e249cc0`). `runs.txt` logs the
producers' and auditors' 547 harness runs this round (lines 1749-2295) before this step's.

## Delta map after the round

The ports recorded their comparisons as reviews in `ghidra/delta_map_review.json` and merged them with `tools/delta_map.py
merge` (decision 55; the audits appended their own rows: 2, 0, 0 and 1). Notes tagged `[review: M2-R9 <port>]` in the
map: oracle-rung-physics 34 (+2 audit), missions-02-09 50, missions-10-17 20, recorder-leftovers 193 (the 165 new entries
and the notes on existing rows). This step added no review of its own and ran no merge.

| class | M2-R8 end | M2-R9 end | M2-R9 bytes |
|---|---:|---:|---:|
| IDENTICAL | 2,773 | **2,773** | 495,987 |
| MODIFIED | 1,564 | **1,564** | 727,005 |
| REWRITTEN | 83 | **83** | 15,855 |
| hand-reviewed (`reviewed` rows) | 3,340 | **3,512** | |

The totals come back to R8's by coincidence: the rung found 11 IDENTICAL rows modified (2,762 / 1,575), missions-10-17 one
more (`tutGameSentMessage`: 2,761 / 1,576), and the recorder's review sample moved 15 unflagged MODIFIED / REWRITTEN
rows to IDENTICAL (`etgAdd` / `etgSub` / `etgMult`, `partSetTumble` / `partSetDeltaR`, `ShipClassToStr` / `ShipTypeToStr`,
`trLIFFileLoad` / `trColorsEqual`, `SaveBlob`, five one-per-file rows) while 2 of its sample went the other way
(`RegisterAllSpaceObjs` / `RegisterAllSpaceObjsInsideMe`: ported). 172 more rows carry the `reviewed` flag (the 123 stage-4
bodies, the sample, the edited rows).

Per stage (`python3 -I tools/m2_stage_counts.py --md` on the map after the round): HW1-matched IDENTICAL / MODIFIED /
REWRITTEN functions (MODIFIED + REWRITTEN bytes), then cat-new by the tool's rules.

| stage | M2-R8 end | M2-R9 end | cat-new |
|---|---|---|---|
| 1 | 201 / 114 / 0 (57,114 B) | **207 / 108 / 0 (55,860 B)** (the recorder's review sample: `ShipClassToStr` / `ShipTypeToStr`, `trLIFFileLoad` / `trColorsEqual`, `SaveBlob`, `bmpFileOpen` found IDENTICAL) | 76 / 27,383 B |
| 2 | 736 / 636 / 30 (258,578 + 4,444 B) | **734 / 638 / 30 (261,490 + 4,444 B)** (the rung's blob / collision / formation bodies found modified; the ETG / Particle rows found IDENTICAL) | 808 / 180,868 B |
| 3 | 847 / 337 / 14 (199,754 + 8,558 B) | **846 / 338 / 14 (200,979 + 8,558 B)** (`animLoadNISScript`, `btgZip`, `selSelectionCompare`, `gpTitleListLoad`: the exe's qsort; `hwDeleteWindow` IDENTICAL) | 450 / 139,196 B |
| 4 | 295 / 182 / 2 (79,159 + 169 B) | **294 / 183 / 2 (79,206 + 169 B)** (`aiuFindDistanceSquared`'s double return) | 23 / 14,404 B |
| 5 | 405 / 115 / 12 (75,094 + 1,179 B) | **404 / 116 / 12 (76,194 + 1,179 B)** (`clSendMove` / `clSendMpHyperspace` / `clSendFormation` found modified and ported; `lgShutdownMultiPlayerGameScreens`, `CheckNetworkVersionCompatibility` IDENTICAL) | 67 / 17,483 B |
| **6** | 289 / 180 / 25 (52,980 + 1,505 B) | **288 / 181 / 25 (53,276 + 1,505 B)** (`tutGameSentMessage`) | **2,365 / 656,918 B: the 2,085 kas2c mission functions (612,687 B) ALL generated and defined** (`nm --defined-only` on this step's debug `hwi-engine`: 2,085 `Init_` / `Watch_` symbols; 2,085 `// cataclysm.exe 0x..` bodies in `src/Missions`); of the 280 others 277 are defined (R8: 276; `cat_scriptSetSlipgateCB` landed; the three `cat_trans*` belong to the exe's Transformer, decision 28) |

What moved: **12 functions the map called IDENTICAL were found modified** by the ports (the rung 11: `bobUpdateObjsInBlobCollInfo`,
`bobUpdateExtraCollBobInfo`, `bobAddObjToSpecificBlob`, `bobFindNearestBlobToObject`, `ObjectsCollided` (R4 had compared it
"identical": the x87 shape differs), `animLoadNISScript`, `btgZip`, `formationArrangeOptimum`, `gpTitleListLoad`,
`selSelectionCompare`, `aiuFindDistanceSquared`; missions-10-17 1: `tutGameSentMessage`) and 3 more were found modified
and ported by the recorder's sample (`clSendMove` / `clSendMpHyperspace` / `clSendFormation`; `RegisterAllSpaceObjs` /
`RegisterAllSpaceObjsInsideMe` were MODIFIED rows whose Cataclysm change had not been ported). **Stage 6 is complete on
the map**: every kas2c mission function is generated from the exe and verified against its machine code (decision 75),
every HW1-matched stage-6 row carries a review. Across the whole map **240 MODIFIED / REWRITTEN rows still carry no
`reviewed` flag** (R8: 271): `TitanInterface.cpp` 92 (stubbed, not applicable), `ETG.c` 45 and `Particle.c` 41 (M2-R3's
scope), `ObjTypes.c` 20, `MultiplayerLANGame.c` 8, `texreg.c` 7, `StatScript.c` 6 (`scriptSetTweakableGlobals` 0x00527820
among them: only its table's keys were compared), `MultiplayerGame.c` 5, `GameChat.c` 3, 2 each in `Clouds.c` /
`CommandNetwork.c` / `File.c` / `Select.c`, 1 each in 7 files ("What is left" 9); 668 IDENTICAL rows were never
hand-reviewed (the R2 re-read covered the low-confidence ones).

## Oracle mode A

The rung after the round, from this step's clean build (the table square root, the exe's qsort, the round's physics;
the GOG streams `original/oracle/oracle01-rec-20261017-140428/oracle01-rec.hwo` (9,601 ticks) and
`oracle01-rec-20261018-102253/oracle01-rec.hwo` (3,527 ticks); decision 47's exclusions `ships_rot`, `rng_all`, `fpcw`,
`players` (93 / 272), `rng6` (161), decision 60's `real` for 10-18):

| recording | first divergent tick, the default | with `/oracleLibmSqrt` | R8 (table switch on / `sqrtf`) | R7 |
|---|---|---|---|---|
| 10-17 (`r9i-r17-a`, 9,600 ticks) | **1922 `ships_vel`** (IDENTICAL through 1921; the fields after it: ships_pos 1954, bullets 2005, univcheck 2020, rng10 2043, ships_health 2043, ships_misc 5885, resources 5885, ships_n 5890; missiles, derelicts, dt, total, real never) | 177 `ships_vel` | 247 / 177 | 177 |
| 10-18 (`r9i-r18-a`, 3,526 ticks) | **1721 `bullets`** (IDENTICAL through 1720; then rng10, ships_pos, ships_vel, ships_health, univcheck 1749, ships_n / ships_misc 1753; missiles, derelicts, resources, dt, total never) | 118 `ships_pos` + `ships_vel` | 155 / 118 | 118 |

Every tick equals docs/m2-r9-oracle-rung-physics.md (sections 6 and 8) and docs/audits/m2-r9-oracle-rung-physics.md
(its table at line 70); the replays are IDENTICAL in every field to the rung audit's, producer 4's and producer 1's
audited runs, so nothing after the rung (missions 02-17, the recorder, the leftovers) touched the replayed physics. The
release replay matches the debug one on the simulation list plus `ships_rot` and `players`.

**What the two divergences are** (producer 1's localisation with `/oracleShipDump` and `hwo_invert.py`, the audit's
reading):
- **10-17 tick 1922 `ships_vel`, ship 41** (a bRecon of player 1 under an attack order; `v.z` one ulp off). The flight
  path through the avoidance, the blobs, the collision and the formation code is now the exe's up to this tick (1,675
  ticks past R8's 247); what is left is the attack flight itself: the attack point / target lead the interceptor code
  computes for a moving target (`GenericInterceptor`'s flight, `aishipFlyToShipAvoidingObjs`'s destination) or the
  coordsys of the turning ship as the DLL words it (decision 48 would show the coordsys directly). The offline replica
  found 11 single-element candidates and no two-element one; the proposed fix is the bodies at 0x005b1xxx
  (`GenericInterceptorAttack` / `flightmanAttack`'s lead) in x87 shape, measured one at a time against this tick.
- **10-18 tick 1721 `bullets`**: GOG fires two bullets at 1721, this engine's first at 1740 (then `rng10`, the
  positions and the health follow at 1749). A firing decision 19 ticks early in the exe: `GenericInterceptorCanFire`
  0x005b2120, `cat_gunCanFire` / the fixed-gun aim already in x87 shape (`cat_gunFixedGunAimedAtTarget`), or the
  attack AI's trigger timing (`aishipAttack`'s range test, the tactics' `tacticsGetShipsMaxVelocity` product tagged
  `CAT-TODO(M2-R10 physics)` and `aiuFindDistanceSquared`'s unrounded return at its 70 callers); the shooter is
  named by inverting the GOG bullet hash against the dump's 'B' lines.
Both are M2-R10 physics' first item; the mode-B rung (the GOG playback of `hwirec01` / `hwirec02`) and the fourth
recording (decision 48) are the user's and would give the next evidence: a divergence in mode B names the first
packet the two engines read differently, and the fixed DLL makes `ships_rot` comparable.

**The stream moves of the round, summed** (every guard-rail against R8's, the table above): the headless Mission01 stream
moved once, by the exe's qsort (`ships_pos` / `univcheck` from tick 0, `ships_rot` from tick 1), not by the table (the
`/oracleLibmSqrt` run is byte-identical to the default's: no ship flies there) and not by the 16 missions, the recorder or
the leftovers (IDENTICAL to every run of the round): **RE-BASELINED to `553845b65e87f3b5`** (release `ba8ee6e78597c175`,
the same content; decision 44). The ai / build / missiontest / loadgame / Armed8 streams moved by the table and the qsort
exactly as producer 1 stated, LFE2 by the Slipgate reader at tick 0 (producer 2), and the ai soak once more by producer
3's ship-part blast fix at tick 694 (bisected here). The dumps moved to producer 1's hashes and are re-baselined.

## The campaign from the NIS-first start

What the player gets now, mission by mission, in the order the exe gives it (every step from the four reports, exercised
headlessly by the per-mission `missiontest` runs of the exit test below; the entry into each mission N >= 2 in those runs
is the exe's warp start with Warp.script's level-N record, decision 86, while the chained path Mission01 -> 02 -> 03 -> 04
is what `sp`'s screenshot loop walks; each self-test ends 32 ticks after the NEXT level has loaded, so every transition's
level load is exercised too):

1. **The start and Mission01** ("Taidan Contras Mount Attack On Higaran Space"), as R8 left it: New Game, the shuffled
   start positions, the opening NIS `n01` in its staging level, `kasMissionStart("mission01")`, the nislet, the fleet's
   arrival, the six objectives in order (DestroyBombers, ExploreRegion, DestroyResourcers, DestroyProxAttackers,
   DestroyDestroyer, Hyperspace: created / completed at 4768 / 4800, 6864 / 8096, 8208 / 8752, 9760 / 10240, 10816 / 11296,
   11328, the jump at 11392, `done ticks=11424 objectives=6/6`), the KAS saves "01 - Hiigara" / "01 - ~ End of Mission",
   the letterbox jump (Mission01 is a `catSpMissionNextLevel` mission). **New this round**: the animatic `(1, 2)` is
   called but `animBinkPlay` is GoK's stub (nothing plays: M2-R10), and **Mission02's level loads WITH its script**: the
   ether's fleet hyperspaces in, the player sees Mission02. The `sp` screenshot loop now walks Mission01 -> 02 -> 03 ->
   Mission04_NIS02 (`n02`) -> Mission04.
2. **Mission02** ("Rescue Kith Destroyer And Find Pod"; a letterbox-jump mission): the fleet rolls in (G_ShipsHaveArrived
   at tick 213 of the test), the hangar-module objective is destroyed by the script at once; **FindDestroyer** (a recon
   to the Kith destroyer, 2352), **Build** (the sMothershipMicro, 2352), **RepairDestroyer** (TECH_REPAIR / TECH_SALVAGE,
   the workers' upgrade and the repair special on the destroyer: 13152; possible since this round's tweak-table fix:
   every repair rate had been 0), **DestroyPirates** (the raiders, 21808), **SalvagePod** (the salvage special on the
   pod, 24448), Hyperspace: the letterbox jump at 24976 -> Mission03 (`done ticks=25136 objectives=6/6`). Mission02's
   meteor storm (R8) and its two slipgate lines (the Slipgate reader) are in the level.
3. **Mission03** ("Rescue Ecclesiam From Pirate Blockade"): **BuildHangarModule** (1744), **Find** (7168), **Mimic**
   (the mimic's disguise through `cat_mrMimicTarget`, 8624), **Lure** (the mines and the ships around the Cleesan,
   17184), Hyperspace: **the roll-call screen** comes up and **Go Now** is clicked (22 ships still out) at 17472 ->
   the staging level Mission04_NIS02 with `n02.nis` (`done 17825, 5/5`).
4. **Mission04** ("(Empty)"; an at-once arrival mission, `catSpMissionArrivesAtOnce` 4 .. 8: G_ShipsHaveArrived at tick
   35): **Replace** (the docking bay, 1712), **Escort** (the ACVs / acolytes under guard, 7424), **Research**
   (TECH_RESEARCH_CLEESAN_ATTACK, 14592), **Destroy** (a held objective: the script never ends it; it completes with the
   Hyperspace message's `ObjectivesDestroyAll` at 20400), Hyperspace: Go Now (5 ships out) at 20464 -> Mission05 (`20816,
   5/5`). **The render's known stop**: the debug `sp` run asserts here on `texreg.c (569)` `nPalettes < 8` when
   `RACE_P1/P1IONARRAYFRIGATE/RL0/LOD0/PAGE0` gets a ninth colour scheme (M2-R10 render); the headless test does not
   render and passes.
5. **Mission05** ("Beast Attacks Bentusi, Station Scuttled"; at once; letterbox jump): **InterceptBentusi** (a recon,
   1536), **SaveBentusi** (11968), **DestroyBeast** (12480), **ProtectCaalShto** (12480), **BuildEngineering** (the
   sMothershipSpecial, 15520), Hyperspace: the letterbox jump at 16224 -> Mission06_NIS04 with `n04.nis` (`16384, 6/6`).
6. **Mission06** ("Escourt Callisto Into Nebulae"; at once): **Explore** (the station, 2016), **Scout** (Nav1 .. Nav4,
   3520), **Mines** (4656), **HvCruiser** (the heavy cruiser group found and brought to 45% hull, 27648), **Leech** (the
   Leech scene, 28176), **Escort** (the Caal-Shto guarded, 30496), Hyperspace: Go Now (39 out) at 30576 -> Mission07_NIS05
   with `n05.nis` (`30931, 7/7`). (The "Kill" objective of its script is unreachable: its message is never sent.)
7. **Mission07** ("Rescue Convoy"; at once; letterbox jump): **Mayday** (a recon to the convoy, 2304), **Protect** and
   **Destroy** (the infected ships, 16368), Hyperspace: the letterbox jump at 16432 -> Mission08 (`16592, 4/4`).
8. **Mission08** ("Rendezvous With Spy"; at once): **Resource** (two workers harvesting, 4032), **Protect** (the spy's
   pursuers, 4992), **CaptureFrigate** (a salvage-upgraded worker pair on a frigate damaged to 40%, 18800),
   **CaptureResourcers** (three collectors, 28720), **NoSurvivors** (28736), Hyperspace: Go Now (55 out) at 29008 ->
   Mission09 (`29361, 6/6`).
9. **Mission09** ("Retrieve Info From Taidan Research Planet"): **MoveTransports** (2928), **DestroySensor** (3952),
   **Wait** (9728), **Engage** (16320), **Protect** (held: 20160), Hyperspace **under fire**: the roll call offers
   **Emergency Hyperspace**, which is clicked at 20224 -> Mission10_NIS06 with `n06.nis` (`20579, 6/6`).
10. **Mission10** ("(Empty)"): **SALVAGE_BIGGUN** (7152), **RESEARCH_BIGGUN** (10048), **BUILD_BIGGUN** (14064),
    **FIRE_BIG_GUN** (the Siege Cannon fired, 20496), HYPERSPACE: the roll call's Go Now (41 out) at 21584 -> Mission11
    (`21938, 5/5`); the pirates' escort waves; 4 mines in the level.
11. **Mission11** ("The Factorem Attacks"): **DESTROY_ATTACKERS** (1888), **REPAIR_FACTORUM** (the Bentusi repaired,
    4512), **DESTROY_BEAST** and **PROTECT_FACTORUM** (held, 5744), Hyperspace: Go Now at 7920 -> Mission12 (`8275,
    5/5`).
12. **Mission12** ("Negotiations With Pirates And Help From Contras"): **DefendMothership** and **AttackOutpost** (7600),
    Hyperspace: Go Now at 10640 -> Mission13 (`10995, 3/3`); 26 mines. (WorkerDock / WorkerReturn are unreachable: the
    SecurityTeam that would send their messages stays empty.)
13. **Mission13** ("Knocked From Hyperspace"): **DESTROY_ENEMY** (19488), **PROTECT_FAALCORUM** (19488), Hyperspace: Go
    Now at 21392 -> Mission14 (`21746, 3/3`).
14. **Mission14** ("(blank)"): **GET_SAMPLE** (the mimic's phases: the undisguised escort ring, the mimic out of view,
    the detectors near the disguised mimic destroyed; through `cat_shipStealthDetectionUpdate`'s detectors: 10080),
    **PROTECT_MOTHERSHIP** (held, 21920), **CAPTURE_TAIIDAN** (the frigate captured by salvage-upgraded workers; held,
    21920), Hyperspace: Go Now at 21984 -> Mission15 (`22339, 4/4`); 32 mines and 6 slipgates; the fleet arrives at
    378 / 372 support units (small craft scuttled to free them).
15. **Mission15** ("Convince Bentusi To Help"; letterbox jump): **DestroyGateNode** (6192), **Block** (the Bentusi's
    slipgate block, 6192), Hyperspace: the letterbox jump at 15264 -> Mission16 (`15424, 3/3`); 2 slipgates.
    (DefendMothership is unreachable: its selection is never filled.)
16. **Mission16** ("(Empty)"): **LOCATE_CS** (1872), **CRIPPLE_CS** (5360), **DOCKWITH_CS** (12816), **HARVEST**
    (20736), **DESTROY_BEAST** (21088), HYPERSPACE: Go Now at 21168 -> Mission17 (`21522, 6/6`); the big gun, the
    mimic docking, the Beast.
17. **Mission17** ("(Empty)"): **DestroyNaggarok** (the Naggarok: 15152), **DestroyNomadMoon** (5248; reopened at 5760,
    13200), **DestroyEmitters** (5744), **Recon** (12736); then the script's `MissionCompleted` ends the game: Fleet
    Command's completion, `quittime` and `utyCreditsSequence`, **the credits `Credits-GameEnd.cred`** (`game end
    tick=15584`). ("Research" is unreachable: its message is never sent.)
18. **The tutorial**: the front end's Tutorial starts the generated Tutorial1; **all 12 lessons** run through their
    FSMs with the game's own callbacks (LESSON 01 Start of Tutorial / ClickHere, 02 Camera, 03 Selection, 04 Focusing, 05
    Sensors Manager, 06 Moving and Waypoints, 07 Basic Attacking, 08 Building, 09 Harvesting, Repairing, and Salvaging, 10
    Advanced Attacking, Formations, and Tactics, 11 Researching and Upgrading, 12 Other Features of Interest), the
    narration on the virtual audio clock in the test (its game-time length: Lesson1 complete at 1108, Lesson12 at
    10348, `lessons=12/12 complete=12/12 inOrder=1 nextClicks=11`), the 12 lesson saves written by the engine and "LESSON
    11 - Researching and Upgrading" loading back through the tutorial load (`loadgame --training`).
19. **The warp start** (`warp.script` with `WarpEnabled 1`, Relic's test entry): `cat_spWarpToLevel` 0x0051b9c0 puts
    the level's recorded fleet, resources and technologies into the ether and the level starts with its script
    (`warpstart` mode: level 10 `applied ... resources=8000/8000 types=18/18 ships=76/76 (all 97) ether=79 outside=0
    tech=20/20`, then `ether=0 outside=79` after the arrival; level 5 passed in development).
20. **Recording** (the recorder's record side): a skirmish / multiplayer game recorded through the game's own in-game
    recording (the DLL's record mode on this engine: `/oracleRecord`) writes the exe-format save and the `.pkts` file;
    the GOG game should play it back (the user's mode-B step). The front end's Record shortcut path is the same code.

Known gaps on this path: the animatics between missions do not play (`animBinkPlay` stub); Mission04 asserts in the
debug render (texreg's colour block; the release build has no assertion and takes the exe's unbounded append); the
player's own combat is not simulated in the tests (`ShipsDamage` ends the destroy objectives, decision 77); the
failed-jump return (`HYPERSPACE_CATFAILEDRETURN`) is ported but no PASS run exercises it; Mission06's `rng_all` can
differ run to run (a non-game RNG stream on the wall clock; every game field equal).

## What is left of stages 3-6

Nothing blocks the stage-6 exit test; each item has an owner:

1. **The oracle's next rung** (M2-R10 physics): 10-17 tick 1922 `ships_vel` ship 41 (a bRecon of player 1 with an attack
   order, v.z one ulp: the attack flight's destination / target lead in `GenericInterceptor` / `aishipFlyToShipAvoidingObjs`,
   or the coordsys of the turning ship: 11 single-element solutions of the offline replica) and 10-18 tick 1721 `bullets`
   (GOG's first two bullets 19 ticks before ours: `GenericInterceptorCanFire` 0x005b2120, `cat_gunCanFire`, the fixed-gun
   aim already in x87 shape, or the attack AI's timing; the shooter is named by inverting the GOG bullet hash against the
   'B' lines); `aiuFindDistanceSquared` 0x0041a2c0's unrounded double to about 70 callers and `tacticsGetShipsMaxVelocity`
   0x00529450's unrounded product (the two `CAT-TODO(M2-R10 physics)`); the rest of Blobs.c (`bobListCreate`,
   `bobListUpdate`, `bobBlobCombine`, `bobObjectListMedian`, `FillInCollisionBlobSpecificLists`,
   `AddObjToObjectListsOfBlob`, `bobObjectDied`: IDENTICAL on the map and unreviewed after the four ported ones showed a
   Cataclysm change the R1 map missed). Decision 48's recording (the DLL's `CAT_SHIP_coordsys` 0x98) would show the
   coordsys of the turning ships that 1922 points at.
2. **The mode-B rung** (the user's session): the GOG playback of `hwirec01` / `hwirec02` with the DLL (the steps in
   docs/m2-r9-recorder-leftovers.md 3.6 and "What the user must decide" below); its first divergent tick, if any, is
   M2-R10 physics' next item. Open with it: the sync checksum's lag (ours 1 packet, GOG's real-time sessions 8: the same
   `netcheckShow` ring, so a playback should still find its entries) and the skirmish test's bigotry (GOG's sessions sent
   21 computer-alliance packets at frame 88; ours sends none).
3. **Decision 48** (the user's): no recording with the fixed DLL exists; `ships_rot` stays excluded in every session, and
   the DLL's words at Ship+0x80 cannot be inverted either.
4. **Stage 6's remaining play-path items** (M2-R10): **the animatics** (`singlePlayerGameUpdate`'s two calls and
   `singlePlayerPostInit` reach GoK's `animBinkPlay` stub; `animAviPlay` is the port of 0x00421860: route them, with the
   headless tests skipping the playback on the virtual clock); **the Mission04 render stop** (M2-R10 render: HW1's
   `trColorsIndexAlloc` asserts `nPalettes < 8` when `RACE_P1/P1IONARRAYFRIGATE/RL0/LOD0/PAGE0`, loaded and not shared with
   8 palettes, gets a ninth colour scheme; the exe's `trTextureRegister` 0x00585760 appends without a bound into a 64-byte
   colour block, its 8 colour infos read back as non-colours, so the block's provenance (palette sharing by CRC) is the
   question); **`Select.c:1153` / `1154`** (a signed overflow `384 - INT_MIN` in `selCircleCompute` from
   `rndShipVisibleUsingCoordSys` in the NIS-first `sp` run, not with `/oracleLibmSqrt` either: a screen coordinate from an
   out-of-range float; non-fatal); **`FOW.c:296-437`** (a signed overflow `INT_MIN - r` in `fowActivateCube` / `fowDeactivateCube` / `fowCubeHasUnvisited`'s cell boxes (lines 296 / 349 / 435, from the W wrappers' world-to-cell conversion) in
   the Mission17 runs only: an out-of-range float position converted to a cell; non-fatal; found by this step's UBSan
   union, present in producer 4's and the audit's Mission17 runs too); **the mission self-test's combat** (decision 77: the attack order on the bombers made
   no kills in 1,600 ticks; M2-R10 test), **the failed-jump return** with no PASS-path exercise (a deliberate loss test),
   **Mission06's `rng_all` flake** (the mixer's `RANDOM_SOUND` on the wall clock suspected, unverified: the virtual audio
   clock in the mission runs is the likely fix and moves every mission's `rng_all` once, or the oracle excludes the sound
   streams); the particle `is_final_tex` warnings and the `memAllocFunctionA` failures seen in Mission17's log (heap
   growth; not investigated); `scriptSetTweakableGlobals` 0x00527820's unreviewed row.
5. **Stage 5** (deferred to after M3, decision 9; the record side done): the 23 `CAT-TODO(stage 5 ...)` sites: plain 8
   (the `/packetRecord` HEAD header `recPackRecordSaveHeader` 0x004cbdb0, the point / mimic / upgrade / special-target
   senders of CommandWrap.c and the external-build packet `cat_clSendExternalBuildUpdatePacket` 0x004523a0, so a recorded
   game carries none of those orders (the record run and the replay agree), the dock words), LAN 6 (`KeepAliveDropPlayerCB`,
   `DeclarePlayerWin`, the colour checksum, the network game's start-position table, `sendChatMessage`), multiplayer
   options 8 (the game options word 0x008860ec's bits, `mgUnitCapsEnabled`), multiplayer 1 (`CaptainGameInfo`'s bytes after
   +0xC8); the Titan / WON interface stubbed.
6. **Stage 4 leftovers that stay**: `gunResetGimbleGuns` uncalled (decision 58), the five creators' default tactics (59),
   the folded handlers' save indices (63). Closed this round: the 123 IDENTICAL bodies compared without a review row (all
   the same as the tree).
7. **Stage 3 leftovers that stay**: HW1's cursor text (decision 65), `rndLoadTarga`'s `GL_CLAMP_TO_EDGE` and the
   Transformer (28), the camera-derived render values with interpolation on (the high-refresh-4k feature); plus item 4's
   render stop and the animatics.
8. **Motion interpolation**: the `/noInterp` and flags-on dumps re-baselined (the table square root and the exe's qsort
   under every frame's flight; decision 46's guard-rails untouched); the clamp / droid blend still unmeasured by the two
   dump scenes.
9. **The delta map's review flags**: 240 MODIFIED / REWRITTEN rows without a `reviewed` flag (the list in "Delta map");
   the 148 outside `TitanInterface.cpp` are a later audit's sample (31 were sampled this round: 2 ported, 15 IDENTICAL).
10. **Still open, the user's**: the two saves of the M0 play-test (8), the M0 play-test sign-off (M3's brief below), the
    fourth recording (3), the mode-B playback (2).

## Plan for M2-R10: the play path's leftovers (a short round), then M3

**Why a round and not M3 at once**: stage 5 is done as this round's setting left it (the recorder's record side) and
stage 6 is complete on the map (every mission generated, every objective reached, the game's end, the tutorial, the
warp start), but three things stand between the tree and an attended campaign play-test past Mission04: the render stop
in Mission04 (the debug build cannot pass it; the release build takes the exe's unbounded write), the animatics (none
plays), and the rung's two open divergences, which are the oracle's only evidence that the flight path is the exe's.
They are small, so M2-R10 is sized at three producers + three auditors + integrate (7 agents), with M3's Linux play-test
of Mission01 .. 03, the tutorial and a skirmish starting now in parallel (the brief below).

**Sizing** (`grep -rn 'CAT-TODO(' src/` after this round; the reports' "What is left"): 2 `CAT-TODO(M2-R10 physics)`
tags + the two localised divergences + 7 unreviewed Blobs.c bodies; the render stop (one texture registry path,
`trTextureRegister` 0x00585760 / `trColorsIndexAlloc`), the animatics (2 call sites -> `animAviPlay`), one UBSan site;
the test items (the combat, a loss test, the mission runs on the virtual audio clock, the bigotry setting), 1 unreviewed
row + the 148 unflagged rows as a sample.

| # | producer | scope | counts | done when |
|---|---|---|---|---|
| 1 | **the oracle's next rung**, physics | 10-17 tick 1922 (ship 41's attack flight: the destination / target lead and the coordsys of a turning ship), 10-18 tick 1721 (the first bullets: `GenericInterceptorCanFire`, `cat_gunCanFire`, the attack AI's timing), `aiuFindDistanceSquared`'s and `tacticsGetShipsMaxVelocity`'s unrounded returns at their callers, the rest of Blobs.c; the mode-B result and the fourth recording if the user made them | about 10 bodies + 7 reviews; the Mission01 stream does not move (no ship flies) | the 10-17 replay IDENTICAL beyond 1922 and the 10-18 beyond 1721, or the next divergences localised and owned; the mode-B playback's first divergence, if the user ran it, localised |
| 2 | **the render stop, the animatics and the sp gate** (render / presentation) | the texture registry's colour block (`trTextureRegister` 0x00585760's append, `trColorsIndexAlloc`, the palette sharing by CRC) ported or `CAT-FIX`ed with the exe's behaviour stated; `animBinkPlay` -> `animAviPlay` 0x00421860 at `singlePlayerGameUpdate`'s two calls and `singlePlayerPostInit`, skipped by the headless tests on the virtual clock; `Select.c:1153` / `1154` and `FOW.c:296-437`'s `INT_MIN` cell (Mission17); the `memAllocFunctionA` failures and `is_final_tex` warnings of Mission17's log | 4 sites + 2 diagnostics | the `sp` screenshot loop runs through Mission04 and on to its timeout with ASan 0 and no fatal (the screenshots per mission kept); an animatic plays between missions in an attended run; no `Select.c` UBSan report in `sp` |
| 3 | **the tests and the oracle's leftovers** | the mission self-test's combat (why the attack order makes no kills; the player's fleet flown where cheap), a deliberate-loss test for the failed-jump return, the mission runs on the virtual audio clock (Mission06's `rng_all`; every mission's `rng_all` moves once, stated), the skirmish test's bigotry option for a mode-B recording with alliances, the `.pkts` header's uninitialised tail word (two recordings of the same game differ in it: zero it or state the exe's), `scriptSetTweakableGlobals` and the 148 unflagged rows sampled | 5 test items + 1 review + a sample | Mission06 pairs byte-identical in every field over 4 runs; the failed-jump flow reached under a test; the 17 mission streams re-baselined once with the stated fields |

**M2-R10 integrate**: the R9 gates hold from a clean build (the mission streams moving only by producer 3's stated
`rng_all`), the `sp` run through Mission04 and beyond with ASan 0, the rung beyond 1922 / 1721 or owned, no `CAT-TODO`
naming M2-R9 or M2-R10, the generators `--check` current. **Then M3**: the user's play-test sign-off (the campaign from
New Game as far as it goes, the tutorial, a skirmish, with tools/play.sh), the one-hour 4K@144 soak (frame-rate drift
over the hour, docs/CONSTRAINTS.md 1: no gamescope), the Windows 11 build (SDL2 + GL on MSVC or clang, the harness's
self-tests run there once, the saves and recordings interchangeable with the Linux build), and the stage-5 remainder per
decision 9 after M3. If the user prefers M3 first, M2-R10's producers 1 and 2 can run beside it: nothing in them touches
the front end or the mission scripts.

## What the user must decide

1. **The `fsqrt` default**: settled (decision 70 answered 2026-10-19; the table is the default since `64f49a9`,
   `/oracleLibmSqrt` is the A/B switch). Nothing to decide.
2. **Stage 5's fate**: the recorder's record side is done; the remaining 23 `CAT-TODO(stage 5 ...)` sites (the LAN send
   sides and keep-alive, the options word, the `/packetRecord` header, the Titan / WON stubs) stay deferred to after M3
   per decision 9. Recommendation: keep it so; nothing in the oracle or the campaign needs them.
3. **The fourth recording with the DLL's `CAT_SHIP_coordsys` 0x98 edit** (decision 48): still none. The one-line edit in
   `tools/oracle/dll/hook_sites.h`, a DLL rebuild (clang-18 + lld-link, decision 7) and a recording session make
   `ships_rot` comparable and show the coordsys of the turning ships that both open divergences point at.
   Recommendation: before M2-R10's rung producer runs; the 10-17 recording stays the continuity reference.
4. **The GOG playback of the hwi-engine recordings (the mode-B rung)**: the user's step, producer 4's exact commands
   (from the main checkout; nothing in `original/` is written by an agent):
   ```
   cd ~/Documents/GameDev/Homeworld-Infection
   R=original/scratch-m2/m2r2-harness/runs
   cp $R/r9r-rec01/settings/SavedGames/RecordedGames/hwirec01 $R/r9r-rec01/settings/SavedGames/RecordedGames/hwirec01.pkts original/game/SavedGames/RecordedGames/
   cp $R/r9r-rec02/settings/SavedGames/RecordedGames/hwirec02 $R/r9r-rec02/settings/SavedGames/RecordedGames/hwirec02.pkts original/game/SavedGames/RecordedGames/
   ORACLE_NAME=hwirec01 tools/oracle/record_reference.sh verify     # two GOG playbacks on the virtual clock, rng0 split
   ORACLE_NAME=hwirec02 tools/oracle/record_reference.sh verify
   python3 -I tools/oracle/hwo_compare.py original/oracle/hwirec01-play-<timestamp>/hwirec01-play-a.hwo \
     original/scratch-m2/m2r2-harness/runs/r9r-play01-a/run.hwo --from-tick 1 --to-tick 4800 \
     --fields rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,players,dt,total \
     --json original/oracle/hwirec01-play-<timestamp>/compare-modeB.json    # the same for hwirec02 against r9r-play02-a
   ```
   (the expected file hashes: `hwirec01` `4640546c0708c284` / `.pkts` `e852ee9f3a457e3b`, `hwirec02` `13a17b9f3a2d8664`
   / `d24604387e083cf5`; the exclusions: `ships_rot` (decision 48), `rng0` (the DLL splits stream 0), `rng_all`, `fpcw`,
   `real` (decision 60); `rng6` and `players` stay in since both sides are playbacks). IDENTICAL over 4,800 ticks passes
   the rung; a divergence is M2-R10 physics' next item. This step's own replays of the same recordings
   (`r9i-play01-a` / `r9i-play02-a`, below) are IDENTICAL to `r9r-play01-a` / `r9r-play02-a`, so either run is the
   comparison's right-hand side.
5. **Whether M3's play-test starts now**, in parallel with M2-R10 (recommended: yes, the brief below), and the defaults
   recorded as decisions 81-97 in cycle/STATE.md (say if you want otherwise). Among them the ones producers asked about:
   the animatics routed to `animAviPlay` in M2-R10 (default: yes, with the tests skipping them); Tutorial1.c's one-line
   change to the exe's localised table (default: accepted, it is the exe's strings where the old line dereferenced an
   exe address); the held objectives counted complete at `ObjectivesDestroyAll` (default: accepted, the exe's own end for
   them); Mission06's `rng_all` (default: the mission runs adopt the virtual audio clock in M2-R10, which moves every
   mission's `rng_all` once); the skirmish test's bigotry (default: the front end's; a second recording pair with
   alliances only if mode B wants them); the exe-format writer for the tree's other saves (default: only a recording's
   save; the rest when LAN interop or a GOG-side load is wanted); the mission baseline moved by the exe's qsort (default:
   accepted and re-baselined, the C library's order at `formationArrangeOptimum` was HWI's).

## M3 play-test brief (refreshed; replaces R8's)

An attended run is the only one that may be audible or on screen (docs/CONSTRAINTS.md 5). Launch with the user's
launcher `tools/play.sh` (since `e249cc0`): the release build (`build/release/hwi-engine`, built by `tools/build.sh
--release` if missing; `--debug` for the ASan build when a crash needs a report), the harness data dir
(`original/scratch-m2/m2r2-harness/data`: the install's own files with writable SavedGames / ScreenShots next to them,
so nothing is written in `original/game`), a private home `original/play-home` (its `.homeworld/Cataclysm.cfg`, keys,
saves, recordings and screenshots live there), X11 by default; engine switches after `--`; the log under
`original/play-home/logs/play-<timestamp>.log`:

```
cd ~/Documents/GameDev/Homeworld-Infection/.claude/worktrees/m2
tools/play.sh -- /window              # or /fullscreen; /1600 for 1600x1200; --debug for the ASan build; --rebuild after a pull
```

Play, and report back per item:
1. **The campaign from New Game, as far as it goes.** Mission01 as before (the opening NIS, the arrival and the nislet,
   the six objectives in order, the hyperspace button's jump; the KAS saves "01 - Hiigara" / "01 - ~ End of Mission"
   loading back through Load Game). Then **Mission02** (the Kith destroyer found, the micro module, the repair and
   salvage research, the destroyer repaired, the raiders, the pod; the letterbox jump), **Mission03** (the hangar module,
   the Ecclesiam found, the mimic's disguise, the lure; **the roll-call screen** with Go Now / Abort), **Mission04** (the
   at-once arrival, the docking bay, the escort, the research, the Cleesan; the roll call again) and on as far as it
   goes: Mission05's Bentusi and the Beast, Mission06's nebula escort, Mission07's convoy, Mission08's spy and the
   captures, Mission09's transports and **Emergency Hyperspace under fire**, Mission10's Siege Cannon, Mission11's
   Faal-Corum, Mission12's pirates, Mission13, Mission14's mimic sample and the capture, Mission15's slipgates,
   Mission16's command ship, Mission17's Naggarok and **the credits**. For each mission: what differed from the GOG game
   in the same place (the order of events, a missing screen, text, sound or speech, a ship that does not do what the
   script says, the roll call's buttons, the difficulty's effect), and where it stopped if it stopped.
2. **The tutorial** from the front end: all 12 lessons (Next through each; the narration, the pointers, the objective
   list, the lesson saves under Training and one loading back).
3. **A skirmish** from Multiplayer > Skirmish against one computer opponent on a listed map (Armed for Burial (8);
   LiveFireExercise2 is hidden in the exe too) for 10 minutes: the start positions, the computer player's builds and
   attacks, the sensors manager, the managers' screens; then **record one** through the Record shortcut and play it back
   through Load Recorded Game (the recorder's record side is new this round).
4. For every item: any crash or hang with the terminal's last lines (`hwi-engine: fatal signal N`, the ASan report of a
   `--debug` run), the frame rate's drift over the session (the one-hour 4K@144 soak is M3's own item), and a screenshot
   of anything that looks wrong.

The known gaps to skip, not report: **no animatic plays between missions** (`animBinkPlay` is a stub until M2-R10);
**Mission04 may fail in the release build where the debug build asserts** (`texreg.c (569)`: the ninth colour scheme of
`P1IONARRAYFRIGATE`'s texture; report what happens, it is M2-R10 render's first item); the player's own fleet is never
flown by the tests, so combat balance is untested; the failed hyperspace jump's return is untested; Mission06 can draw a
non-game random differently run to run (invisible in play); the stage-5 LAN game and the Titan / WON screens are not
ported (do not start a LAN game).

## Decisions

**Applied this round:**
- The faithfulness defaults and decisions 17-80 of cycle/STATE.md; **decision 70 answered and applied** (the exe's table
  `fsqrt` is the engine default, `/oracleLibmSqrt` the switch: decisions 23 / 56 closed); **decision 9 applied as
  "recorder"** (stage 5 reduced to the recorder's record side, done; the rest after M3); decision 46's guard-rails
  untouched (the dumps re-baselined with the stated physics); decision 47's exclusions and decision 60's `real` applied to
  every comparison; decision 48 open (no fourth recording; `ships_rot` excluded); decision 55 followed by every producer
  and audit; decision 57 stands (the headless Mission01 stream does not see the AI: the rung's physics did not move it,
  the exe's qsort did); decision 75 followed (every generated body from the generator, 15 verified OVERRIDES addresses,
  no hand edit); decisions 76 / 77 extended to every mission (below).
- **Decision 44 updated**: the mission-stream baseline moved once this round, by the exe's qsort in the level's
  formations (`formationArrangeOptimum`'s ties): `1028375d20f15d75` (producer 1's build; `ships_pos` / `univcheck` from
  tick 0, `ships_rot` from tick 1 against `0d2b76d565105a26`, every other field IDENTICAL over 6,001 ticks); the same
  content through `be3e5030cebbea0b` (producer 2), `dc2fb8ce71c0d26b` (producer 3), `fd82fafbcd9011fc` (producer 4) and
  this step's build (the exit test). **Decision 49 updated**: the oracle's first divergences are tick 1922 (10-17) and
  1721 (10-18) on the default; 177 / 118 with `/oracleLibmSqrt`.

**Still open (the user's; none blocks M2-R10):**
- **8.** Two saves from the M0 play-test (one mission, one skirmish).
- **The M0 play-test sign-off** (docs/PLAN.md); M3's brief above.
- **Decision 48**: a fourth recording with the fixed oracle DLL.
- **The mode-B playback** of `hwirec01` / `hwirec02` (item 4 above).

**Surfaced by M2-R9.** Each has a default in the tree; say if you want otherwise (numbered 81-97 in cycle/STATE.md).
81. **The exe's `qsort` for every sort the exe makes** (`catQsort`, `CatSort.c`; the 14 call sites); ties come out as in
    the exe; `Memory.c`'s debug sort and the uncalled `selShipsInSelection` keep the C library's. The mission baseline
    moved by it (decision 44). Chosen names: `catQsort`, `catQsortShort`, `catQsortSwap`, `catBlobObjPosition`,
    `catBlobDistSqD`, `oracleShipDumpAvoid`, `oracleShipDumpFly`, the dump's 'O', 'V fly' and 'B' lines.
82. **The rung's reference numbers**: 1922 (10-17, ship 41's v.z) and 1721 (10-18, the first bullets) on the table; 177 /
    118 with `/oracleLibmSqrt`; both owned by M2-R10 physics with `aiuFindDistanceSquared`'s and
    `tacticsGetShipsMaxVelocity`'s unrounded returns (tagged) and the rest of Blobs.c.
83. **The flight path's further x87 shape**: the collision blobs by collision position, the blob attacking flag
    (`shipisattacking` AND `catAttackStarted`), `ObjectsCollided`'s shape with the unrounded `1/t`,
    `collCheckBigShipSmallShipColl`'s clamped-host test, `cat_dockFlyOutOfCone`, `cat_gunFixedGunAimedAtTarget`,
    `hyperspaceCost`'s double return at its five callers, `protectShip` / `protectShipsAvg`; `catFmathSqrtDouble` returns 0.0
    for a NaN as the exe's `fcomp` / `test ah,0x40` (CAT).
84. **The ai soak's `rng_all` is not reproducible under load** (the sound stream drawn on the mixer thread's wall clock;
    the `ai` mode does not silence it as the replay mode does); the 400-tick ai pair is unaffected; the 4,000-tick soak is
    judged on `result=PASS`, ASan 0 and no fatal.
85. **Two new non-fatal UBSan kinds**: `Select.c:1153` / `1154` signed overflow `N - -2147483648` in `selCircleCompute`
    (from `rndShipVisibleUsingCoordSys`) in the NIS-first `sp` start and the Mission06 / 12 / 14 / 15 / 16 self-tests, with
    either square root; `FOW.c:296-437` `-2147483648 - 2` in `fowActivateCube` / `fowDeactivateCube` / `fowCubeHasUnvisited`'s cell boxes (lines 296 / 349 / 435, from the W wrappers' world-to-cell conversion) in the Mission17 runs (an
    out-of-range float position's cell); both M2-R10. The recorder's packet writes add misaligned-access reports in
    `CommandNetwork.c` / `Queue.c` in a record run (the exe's packed layouts).
86. **The mission self-test per mission**: `/catMissionTestMission N` (harness `--mode missiontest --mission N`, 2 .. 17;
    without it Mission01's run is unchanged) enters mission N through the exe's warp start (`cat_spWarpToLevel` with
    Warp.script's level-N record, as a copy with `WarpEnabled 1` / `LevelToWarpTo N` would; chosen over chaining so each
    test depends on its own mission only); the tables `catMtMissions[]` and a scripted player per mission in
    `MissionSelfTest.c` (decision 77's events: the command layer, the managers, `ShipsDamage`; the roll call's enabled
    button clicked through its callback; a lost mission ends the run); PASS = level loaded, KAS started, fleet arrived,
    the objectives created in the table's order and each completed, the jump begun, the next level loaded (Mission17: the
    game's end), no loss; the run ends 32 ticks after the next level loads. The held objectives (Mission04 Destroy,
    Mission09 Protect, Mission11 DESTROY_BEAST / PROTECT_FACTORUM, Mission14 PROTECT_MOTHERSHIP / CAPTURE_TAIIDAN) complete
    through the Hyperspace path's `ObjectivesDestroyAll` (their scripts have no other end); the unreachable ones are left
    out of the tables (Mission06 Kill, Mission12 WorkerDock / WorkerReturn, Mission15 DefendMothership, Mission17
    Research). Lines: `missiontest: start mission=N ..`, `<objective> tick=N created|completed`, `next level tick=0
    <level>`, `end mission=N .. jump=T rollcall=<button> ..`, `game end tick=N credits=..`, `done mission=N ticks=N
    objectives=N/M result=PASS|FAIL`; `/catMissionTestTrace` for a line per KAS message. The combat experiment (the
    attack order instead of `ShipsDamage`) is deferred with its measurement (M2-R10 test).
87. **The generator covers the whole campaign**: `DEFAULT_SCRIPTS` = Mission01 .. 17 + Tutorial1; 2,085 bodies verified,
    25,279 all-constant calls' arguments equal; 15 OVERRIDES addresses (20 patches), each HAND-EDIT in the output and
    verified against the disassembly (the Mission05 hashes, Mission08's dead `kasJump`, the folded `kasfAssert` stub
    0x0049ae30 in 13 Sentinels / SupportWave2 sites, 0x0062dfe0's label and this-argument); the reading rules of docs
    m2-r9-missions-02-09.md 2.1 and m2-r9-missions-10-17.md 2; `src/Missions/meson.build` is generated. Tutorial1.c's
    Lesson12 `L12Tactical` line is now `LSTRING_008cd960[strCurLanguage]` (the exe's localised table; the earlier output
    dereferenced exe address 0x008cd960: a latent fault, so a justified deviation from "Tutorial1 unchanged").
88. **The "Mine" / "Slipgate" mission-sphere readers**: `cat_scriptSetMineCB` 0x004a3f20 (a mine of
    `mineStaticInfos[RACE_Derelict]` owned by players[1] in single player / players[7] in multiplayer, damage 5000, placed
    at the scanned point) and `cat_scriptSetSlipgateCB` 0x004a1020 (a `dSlipgate*` ship of `RACE_Derelict`, paired through
    `catSlipgatePending` 0x00c362dc, `SOF_NotBumpable`, disable reason (1, 6)); LiveFireExercise2 has 91 ships now.
89. **Port fixes found by the mission runs** (each at the exe's body): `kasfSpecialOn`'s return, `kasfVectorAdd` /
    `kasfVectorCopy` only overwrite an existing point, `AllowPlayerToResearch`'s -1 skipped (CAT-FIX: the exe writes below
    its table), `scriptStringToBool`'s digits, `tutGameSentMessage` never clears the queue, `catKbFocus` / chase's
    `MaxSelection` local, `cat_bigGunProcessFireCommand`'s charge effect held across `etgEffectDelete`,
    `MakeExplosionRockBumpables` spares `SOF_CatShipPart`, `spHyperspaceSelectionOut`'s clamp host and `midlevelHyperspaceOut`;
    HWI fixes: the tweak table's `CLASS_*` keys (every repair rate and salvage limit had been 0 since M2-R4),
    `bobUpdateExtraCollBobInfo`'s GoK-only assertion removed, `cat_beastGetAttackStaticInfo`'s 64-bit overlay, `dbgFatal`'s
    ASan stack trace.
90. **The `sp` gate opened to every generated mission** (`cat_spKasScriptGenerated` reads the complete `catKasScripts`)
    and now stops in Mission04 on HW1's `texreg.c (569)` assertion (`trColorsIndexAlloc`: `nPalettes < 8`); the exe's
    `trTextureRegister` 0x00585760 appends without a bound into a 64-byte colour block. Owner M2-R10 render; the release
    build has no assertion. Until then `sp` is judged on the running universe (4,496 ticks at 340 s in Mission01), ASan 0
    and the levels it walked (01 -> 02 -> 03 -> 04_NIS02 -> 04).
91. **The end of the game**: Mission17's `MissionCompleted` 0x00491c70 -> `singlePlayerMissionCompleteCB` 0x0051be40 ->
    `utyGameQuitToMain` 0x005ab6b0's credits (`psModeBegin("Credits\\", 0xd)`, `Credits-GameEnd.cred`); reached headlessly
    (`missiontest: game end`). `animBinkPlay` is GoK's `#if 0` stub, so no animatic plays between missions or at the start;
    `animAviPlay` is the port of 0x00421860 (M2-R10; default: route the calls, the headless tests skip the playback).
92. **The harness** (decision 80 extended): modes `warpstart` (`--warp-level N`: a test copy of warp.script in the run's
    own data dir), `missiontest --mission N`, `loadgame --training --save-src DIR`; `--record NAME` / `--record-stop N`
    (skirmish mode); the summary's `record:` line; 29 modes. Switches: `/catMissionTestMission`, `/catMissionTestTrace`,
    `/catWarpStartTest`, `/catLoadGameTraining`, `/oracleRecord`, `/oracleRecordStop`, `/oracleResave`, `/catVirtualAudio`,
    `/oracleLibmSqrt`. The audit's O1 of R8 closed: the Tutorial1 leg is a pair. Harness runs in parallel: do not edit
    `tools/m2_harness.sh` while a run is in flight (bash reads it as it runs; the recorder audit lost one `sp` run so).
93. **The Tutorial1 leg through Lesson12 on the virtual audio clock** (HWI-TEST `/catVirtualAudio`, set by
    `/catMissionTestTutorial`: no audio device, no stream thread; `soundVirtualAdvance` from `utyTasksDispatch`'s virtual
    branch mixes 22,050 samples per second of task time in 256-sample blocks, one `isoundstreamstep` pass before each
    block; `soundpause`'s fade-out mixes block by block); the narration takes its game-time length (prompt 3 at tick
    158), the leg ends at 10,444 ticks, byte-identical run to run; the 12 lesson saves are written and "LESSON 11" loads
    through the tutorial load. Chosen names: `soundVirtualClock`, `soundVirtualAdvance`, `isoundstreamstep`. A normal run
    is unchanged.
94. **Mission06's rare `rng_all`-only divergence** (2 of 4 producer runs from tick 2081 / 22235; every game field equal
    over 30.9k ticks; the mixer thread's `RANDOM_SOUND` on the wall clock suspected, unverified). Default: the mission
    runs keep the wall-clock mixer this round and are judged on the simulation fields when `rng_all` alone differs; M2-R10
    adopts the virtual audio clock for them (every mission's `rng_all` moves once) or the oracle excludes the sound streams.
95. **The recorder's record side** (stage 5 reduced; decision 9 applied as "recorder"): the 4 sites closed at the exe's
    bodies (81's list in the ports table); hwi-engine records skirmish / multiplayer games in cataclysm.exe's own format
    (the save at version 0x10ad0027 through the SaveCat32 writer with the exe's ILP32 layouts, the `.pkts` file);
    `/oracleRecord <name>` / `/oracleRecordStop <tick>` follow the DLL's record mode (a single-player game refused as the
    DLL refuses it); the replay of a recording is IDENTICAL to its record run except `rng0` / `rng6` / `rng_all` (the
    computer player does not run in a playback; the DLL splits stream 0 for the same reason); the sync checksum's lag is
    1 packet (GOG's real-time sessions: 8; the same `netcheckShow` ring); `/oracleResave` for the writer's byte check.
    Chosen names: `recordingStartSave` 0x00a578f8, `recPackDeleteFile` (HWI-FIX), `saveCat32Saving`, `saveCat32ConvertBack`,
    `saveCat32SaveStruct` / `Array` / `Ship` / `Begin` / `End`, `madCat32ObjectFileOffset`, `aitCat32SaveTeam`. Defaults: only
    a recording's save is written in the exe format (the tree's other saves keep the native format); the recorder's files
    live in the user settings path; a truncated stale pointer whose low half is 0 is written as 4 (non-NULL). New tools:
    `tools/formats/pkts_probe.py`, `tools/formats/recording_compare.py` (`--chunks`).
96. **Stage 5 deferred** (decision 9): a recorded game sends nothing for the external builds and the point / mimic /
    upgrade / special-target orders (their senders are not ported: the `CAT-TODO(stage 5)` lines of CommandWrap.c), so
    those do nothing in a recorded game, in the record run and the replay alike; `recPackRecordSaveHeader`'s HEAD header
    (`/packetRecord` only), `KeepAliveDropPlayerCB`'s `realtimeelapsed` (stage 5 LAN) and `mgUnitCapsEnabled`'s disabled
    button (stage 5 multiplayer options) tagged from the review sample.
97. **`Subtitle.c:1144` closed** (CAT-FIX at `subTitlesUpdate` 0x005842f0: an actor past the 16-theme table, the location
    cards' 0x70 from `kasfLocationCard` / `cat_nisSubtitle`, starts from `STT_LocationCard`; the exe forms the address and
    never reads it, since `#r3#tf` replaces the theme first); the reviews of the 123 stage-4 IDENTICAL bodies (all the
    same; the ICF folds and the inlined NoAdd bodies noted), the 31-row sample (2 ported, 3 tagged, 15 IDENTICAL) and the
    42 notes on edited rows are in the map (`m2r9:recorder-leftovers`).
