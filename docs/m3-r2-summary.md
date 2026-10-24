<!-- done: exit-test, fences, ports, deltamap, left, m3r3-plan, user-decisions, playtest-brief, decisions -->
# M3-R2 summary: the second play-test fix round (the settings line + the overlay + `/frameLog` and the quit after a resolution change, the cutscene's one-frame flicker, the hyperspace arrival over the whole assembly, FULL DETAIL's mesh batching) and the plan for M3-R3

Status: **COMPLETE 2026-10-24; the M3-R2 exit test MET** on every product condition of "Exit test" below, with three statements beside
it: **one self-test check fails** (the toggle self-test under `/letterboxBehind`, `m3r2i-e-toggle-lb` `fails=1 result=FAIL`: an M3-R2
check's expectation that does not hold under the switch, not a product fault; the other six toggle runs PASS; owner M3-R3, one line),
fence 4's p99 is quoted under this machine's CPU load, and one done-when is not met by nature (item 2, the flicker: not reproduced in
450,644 frames, no engine path makes it; the instrument is in the user's hands); **presentation only**: no simulation field of any
stream of the round moved. This doc and the cycle/STATE.md update are the integrate step's checkpoint commit (the commit after
`47922ff`). Branch `m2`, worktree `.claude/worktrees/m2`, workflow `cycle/workflows/m3-r2.js` (944565c; `ROUND_BASE 06dae5c`, after the
three "M3 Windows build" commits; the M3-R1 baseline `1ae66610c3f02410` / release `234bcccfb948ab98`), run id `wf_3590992d-400`, run
from the user's terminal session 38027cce (Fable), not from the orchestrator; **cut off by the weekly limit on 2026-10-22 after port 1
and resumed on 2026-10-24** with the same run id (the nis-flicker producer continued from its WIP snapshot `641a9be`). 9 agents: four
producers in sequence, one per item (instrumentation incl. the quit fault, nis-flicker, hyperspace-assembly incl. the late stern
modules, mesh-batching), four auditors (each checkpointed its port: `bbdc613` 0.89, `4b799cb` 0.97, `83c4b26` 0.89, `47922ff` 1.00, all
PASS-WITH-FIXES) and this integrate step. The user's answers: cycle/answers/m3-r2-user.md (the mandate, the four reports verbatim, the
GPU free, the overlay key one of F1-F12 (queued as M3-R3 item 0), the overlay fine at 200 %, the camera jitter marked fixed; M3-R1's
decisions unanswered: the defaults stand), read first and again before this report; the user's third play-test (cycle/answers/
m3-r3-user.md, seven items) is M3-R3's queue and not this round's scope. User decisions: cycle/STATE.md "## M2" (the faithfulness
defaults and decisions 17-121 applied; decision 70's table `fsqrt` under every run; decision 44's baseline never re-baselined; decision
46's guard rails; decision 48 still open: no fourth recording, `ships_rot` stays excluded; mode B still the user's step).

## Exit test (this step, from a clean build of `47922ff`)

**Builds.** `tools/build.sh --debug --clean` and `--release --clean` on the tree at `47922ff` (14:2x; gcc 13.3.0): both rc 0,
`[274/274] Linking target hwi-engine` (M3-R1: `[273/273]`; the new source `src/Game/Instrument.c`), **0 `warning:` and 0 `error`
lines** under `-Werror`. The binaries were copied aside to `original/scratch-m2/m3r2-integrate/{debug,release}/hwi-engine` so that
nothing rebuilt under the runs; the build logs are `original/scratch-m2/m3r2-integrate/build-{debug,release}.log`. After the one
source edit of this step (the BTG.c comment, condition (f)) both builds were made again from clean: rc 0, `[274/274]`, 0 warnings
(debug `48bbed572cb108f4`, 90,533,856 B; release `a9a8d1d302797b3d`, 5,739,400 B; `rebuild-{debug,release}.log`).

| build | size | sha256 |
|---|---:|---|
| debug (ASan + UBSan) | 90,533,840 B | `9cd73b890cfd9fbc…` |
| release (`-O2`, no sanitizers) | 5,739,400 B | `489f5937ba6597b1…` |

`tools/m2_layout32.sh` OK (`all ILP32 layout asserts hold`, 45 ship-specifics files); the generators current: `python3 -I
tools/m2_save32_gen.py --check` (`SaveCat32Layouts.c is current`), `tools/m2_kas2c_gen.py --check` (`CatMissions.h` / `meson.build`
current), `tools/m2_kasf_table_gen.py --dispatch --check` (`KASFuncTable.c` / `KASFuncDispatch.c` current), `tools/m2_strings_gen.py
--exe <main checkout>/original/game/cataclysm.exe --check` (`check: up to date`, exe rows 1705; the worktree has no `original/game`, so
the exe is the main checkout's by absolute path).

**Runs.** 162 harness runs `m3r2i-e-*` on the plain `tools/m2_harness.sh` (silent, offscreen; `runs.txt` lines 4131-4292;
the prefix `m3r2i-e-` because producer 1 had already used `m3r2i-*` and the harness replaces an existing label's directory: its runs
are kept intact) in ten chains, then the two-run attribution chain X (fence 2's `rng0` question): H first and alone (the GPU batch on the release binary: the 4K frame-time runs batched and immediate,
the 4K `/frameLog` pair, the 4K captures, the three real-clock cutscene scans; nvidia-smi 5 % before, 16 % / 11 % between the groups, 0 %
after, no game running; the chain waited for 120 s of <= 40 % before each group; `claude-gpu-done` after it at 14:49:48), then A
(the rung's replays, the hwi-engine playbacks, missions 14-17), B (the Mission01 gates with every setting flipped, fe, the scripted
self-tests), C1 / C2 (missions 02-05 / 06-09), D1 (missions 10-13, Mission01's self-test, the tutorial, the lesson load, the failed-jump
pair, the warp start, loadgame), D2 (ai + the soak, the skirmishes, the motion / nis dumps and their opt-outs and `/meshImmediate`
twins, the toggles, uiscale x9, lodtest, the team colours), E (`sp` 1,200 s and fe at 4K), F (`sp` 1,200 s at 4K) and G (the four
fences' captures and scans) in parallel, beside the user's load (load average 24-37 throughout: VS Code's ripgrep at 600-1,000 % CPU,
Steam's start; no run lost its summary; nobody edited the harness during the step). **Every quitting run rc 0** except the timeouts by
design: `m3r2i-e-sp` and `m3r2i-e-sp-4k` (rc 124 at 1,200 s, `sp` in Mission12 after 12 levels (129 golden-shot points logged, 23,289 file loads, 12 animatics skipped headless), `sp-4k` in Mission11 (119 points, 22,285 loads)), `m3r2i-e-fe` and `m3r2i-e-fe-4k` (rc 124 at 120 s at
Main_game_screen); **ASan 0 in every run**; LeakSanitizer clean in every run. The `.hwo` hashes differ from M3-R1's and the producers'
for the same content: the stream header carries the build identity; IDENTICAL below is `hwo_compare.py`'s verdict over every tick and
packet record and all 21 fields.

**The runs** (every `m3r2i-e-*` run in `runs.txt` order; rc / wall from `runs.txt`, the rest from each `summary.txt`; the `.hwo`
sha256 is this step's own hash, comparable only within this step; the last column is the mode's own summary line(s), long ones cut
in the middle):

| run | mode | rc / wall | milestone | first stop | ASan; LeakSanitizer | `run.hwo` sha256 (size) | the mode's line |
|---|---|---|---|---|---|---|---|
| `m3r2i-e-perf4k-imm1` | ai | 0 / 12s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2d615f67ed8009bb (123368 B) | aitest: tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... 177 checks=16/16 result=PASS; FAIL lines: 0 / lodstats: frames=301 median=13.666 p99=41.047 max=48.011 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r2i-e-perf4k-bat1` | ai | 0 / 8s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2d615f67ed8009bb (123368 B) | aitest: tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... =177 checks=16/16 result=PASS; FAIL lines: 0 / lodstats: frames=301 median=5.038 p99=12.818 max=16.368 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r2i-e-perf4k-imm2` | ai | 0 / 12s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2d615f67ed8009bb (123368 B) | aitest: tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... 177 checks=16/16 result=PASS; FAIL lines: 0 / lodstats: frames=301 median=13.689 p99=34.043 max=49.664 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r2i-e-perf4k-bat2` | ai | 0 / 7s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2d615f67ed8009bb (123368 B) | aitest: tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... =177 checks=16/16 result=PASS; FAIL lines: 0 / lodstats: frames=301 median=4.581 p99=16.054 max=50.538 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r2i-e-flog4k-bat` | ai | 0 / 8s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2d615f67ed8009bb (123368 B) | aitest: tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... >-1 drift(thirds)=PASS seconds_over_budget=4 / lodstats: frames=301 median=4.889 p99=11.787 max=15.589 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r2i-e-flog4k-imm` | ai | 0 / 11s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2d615f67ed8009bb (123368 B) | aitest: tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... -1 drift(thirds)=PASS seconds_over_budget=7 / lodstats: frames=301 median=12.770 p99=30.011 max=34.242 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r2i-e-shot4k-bat` | ai | 0 / 7s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 5066f78afa0c9d41 (103688 B) | aitest: tick=418 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... ox=none letterboxBehind=0 ..); FAIL lines: 0 / lodstats: frames=261 median=4.801 p99=36.243 max=50.822 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r2i-e-shot4k-imm` | ai | 0 / 9s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 5066f78afa0c9d41 (103688 B) | aitest: tick=418 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... x=none letterboxBehind=0 ..); FAIL lines: 0 / lodstats: frames=261 median=12.433 p99=46.651 max=85.989 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r2i-e-rc-n01` | nis | 0 / 111s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=1788 frames=15211 sampled=15194 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-rc-n01/n ... e5045f66133f / framesig: frames=15277 readback=0 segments=1 outliers=0 (image 0, bars 0, objects 0, camera 0) first=none; short runs=0 (bars 0, objects 0) first=none |
| `m3r2i-e-rc-nislet01` | nis | 0 / 107s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=1728 frames=14628 sampled=14614 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-rc-nisle ... d95ec105a313 / framesig: frames=14689 readback=0 segments=1 outliers=0 (image 0, bars 0, objects 0, camera 0) first=none; short runs=0 (bars 0, objects 0) first=none |
| `m3r2i-e-rc-n06` | nis | 0 / 66s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=1056 frames=8975 sampled=8942 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-rc-n06/nis ... 4f89b781210ba / framesig: frames=9049 readback=0 segments=1 outliers=0 (image 0, bars 0, objects 0, camera 0) first=none; short runs=0 (bars 0, objects 0) first=none |
| `m3r2i-e-ai-a` | ai | 0 / 21s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d982c0dcae02869b (98768 B) | aitest: tick=398 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconcommand=49 build=17 harvest=33 skirmishplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS; FAIL lines: 0 |
| `m3r2i-e-ov-ai-on` | ai | 0 / 28s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e43b7db0b1a89218 (59408 B) | aitest: tick=238 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... box=none letterboxBehind=0 ..); FAIL lines: 0 / lodstats: frames=81 median=29.255 p99=39.378 max=39.378 verts_median=25169 verts_max=25169 fullDetail=1 size=1024x768 |
| `m3r2i-e-mission-a` | mission | 0 / 28s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-ai-b` | ai | 0 / 19s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d982c0dcae02869b (98768 B) | aitest: tick=398 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconcommand=49 build=17 harvest=33 skirmishplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS; FAIL lines: 0 |
| `m3r2i-e-r17-a` | replay | 0 / 52s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c3b7d873ae8fb692 (2630926 B) | oracle: ticks=9601 packets=4800 ships=80 randSyncErr=0 univSyncErr=2911 blobSyncErr=4746 pktSyncErr=0 records=14405 file=/home/evanmedinger/Documents/GameDev/Homewor ...  converted, 47 ships, 0 size problems;replay started oracle01-r6 players=2 ships=47 tick=1 multiplayer=0; last tick line: replay tick 9471 packets=4736 records=14209 |
| `m3r2i-e-ov-ai-off` | ai | 0 / 25s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e43b7db0b1a89218 (59408 B) | aitest: tick=238 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... box=none letterboxBehind=0 ..); FAIL lines: 0 / lodstats: frames=81 median=33.512 p99=43.018 max=43.018 verts_median=25169 verts_max=25169 fullDetail=1 size=1024x768 |
| `m3r2i-e-mission-b` | mission | 0 / 26s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-ai-soak` | ai | 0 / 41s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 5b53f300ce915205 (984370 B) | aitest: tick=3998 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconcommand=49 build=17 harvest=33 skirmishplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS; FAIL lines: 0 |
| `m3r2i-e-ov-ai4k-on` | ai | 0 / 32s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e43b7db0b1a89218 (59408 B) | aitest: tick=238 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... ox=none letterboxBehind=0 ..); FAIL lines: 0 / lodstats: frames=81 median=32.365 p99=72.773 max=72.773 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r2i-e-mission-perturb` | mission | 0 / 35s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 6956441278e48b98 (1476419 B) | mission: last tick line: mission tick 5888 records=5892; ships loaded 33 |
| `m3r2i-e-mission-rel-a` | mission | 0 / 7s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ce30c53259f034c8 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-m2-a` | missiontest | 0 / 100s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7497c9e5900b59b0 (6381116 B) | missiontest: mission=2 ticks=25936 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r2i-e-mission-rel-b` | mission | 0 / 7s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ce30c53259f034c8 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-skirmish-lfe2-a` | skirmish | 0 / 27s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a14774ffd6287143 (147967 B) | skirmishtest: ticks=600 ships=91 result=PASS; FAIL lines: 0 |
| `m3r2i-e-mission-rel-perturb` | mission | 0 / 6s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a0f27a70b7116c4f (1476419 B) | mission: last tick line: mission tick 5888 records=5892; ships loaded 33 |
| `m3r2i-e-r17-b` | replay | 0 / 60s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c3b7d873ae8fb692 (2630926 B) | oracle: ticks=9601 packets=4800 ships=80 randSyncErr=0 univSyncErr=2911 blobSyncErr=4746 pktSyncErr=0 records=14405 file=/home/evanmedinger/Documents/GameDev/Homewor ...  converted, 47 ships, 0 size problems;replay started oracle01-r6 players=2 ships=47 tick=1 multiplayer=0; last tick line: replay tick 9471 packets=4736 records=14209 |
| `m3r2i-e-ov-ai4k-off` | ai | 0 / 32s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e43b7db0b1a89218 (59408 B) | aitest: tick=238 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... ox=none letterboxBehind=0 ..); FAIL lines: 0 / lodstats: frames=81 median=33.260 p99=71.316 max=71.316 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r2i-e-skirmish-lfe2-b` | skirmish | 0 / 20s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a14774ffd6287143 (147967 B) | skirmishtest: ticks=600 ships=91 result=PASS; FAIL lines: 0 |
| `m3r2i-e-mission-noi` | mission | 0 / 27s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-r18-a` | replay | 0 / 28s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | bd2c316848aff30c (966649 B) | oracle: ticks=3527 packets=1763 ships=55 randSyncErr=0 univSyncErr=4 blobSyncErr=1619 pktSyncErr=0 records=5294 file=/home/evanmedinger/Documents/GameDev/Homeworld-I ...  converted, 47 ships, 0 size problems;replay started oracle01-r18 players=2 ships=47 tick=1 multiplayer=0; last tick line: replay tick 3327 packets=1664 records=4993 |
| `m3r2i-e-skirmish-armed8` | skirmish | 0 / 20s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c996a4b4a7314be7 (147967 B) | skirmishtest: ticks=600 ships=54 result=PASS; FAIL lines: 0 |
| `m3r2i-e-m10-a` | missiontest | 0 / 150s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a72352eb513d8239 (5397854 B) | missiontest: mission=10 ticks=21939 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r2i-e-mission-nonis` | mission | 0 / 26s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-m6-a` | missiontest | 0 / 168s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cc2d450412d30d07 (7629566 B) | missiontest: mission=6 ticks=31011 objectives=7/7 result=PASS; FAIL lines: 0 |
| `m3r2i-e-r18-b` | replay | 0 / 28s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | bd2c316848aff30c (966649 B) | oracle: ticks=3527 packets=1763 ships=55 randSyncErr=0 univSyncErr=4 blobSyncErr=1619 pktSyncErr=0 records=5294 file=/home/evanmedinger/Documents/GameDev/Homeworld-I ...  converted, 47 ships, 0 size problems;replay started oracle01-r18 players=2 ships=47 tick=1 multiplayer=0; last tick line: replay tick 3327 packets=1664 records=4993 |
| `m3r2i-e-m2-b` | missiontest | 0 / 88s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7497c9e5900b59b0 (6381116 B) | missiontest: mission=2 ticks=25936 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r2i-e-motion-noi` | motion | 0 / 41s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1431 sampled=1431 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-motion-noi/motion.dump result=PASS; dump motion.dump 1431 frames, sha256 c503c869f0dc9cdb |
| `m3r2i-e-mission-libm` | mission | 0 / 27s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-mission-lb` | mission | 0 / 27s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-r17-libm` | replay | 0 / 52s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 3823b695ea8daefd (2630929 B) | oracle: ticks=9601 packets=4800 ships=80 randSyncErr=3347 univSyncErr=4693 blobSyncErr=4746 pktSyncErr=0 records=14405 file=/home/evanmedinger/Documents/GameDev/Home ...  converted, 47 ships, 0 size problems;replay started oracle01-r6 players=2 ships=47 tick=1 multiplayer=0; last tick line: replay tick 9471 packets=4736 records=14209 |
| `m3r2i-e-motion-on` | motion | 0 / 43s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1431 sampled=1431 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-motion-on/motion.dump result=PASS; dump motion.dump 1431 frames, sha256 c0e411662fa2f527 |
| `m3r2i-e-mission-lbsw` | mission | 0 / 27s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-r18-libm` | replay | 0 / 29s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b63b7226f8f7c6a3 (966654 B) | oracle: ticks=3527 packets=1763 ships=55 randSyncErr=781 univSyncErr=1658 blobSyncErr=1619 pktSyncErr=0 records=5294 file=/home/evanmedinger/Documents/GameDev/Homewo ...  converted, 47 ships, 0 size problems;replay started oracle01-r18 players=2 ships=47 tick=1 multiplayer=0; last tick line: replay tick 3327 packets=1664 records=4993 |
| `m3r2i-e-r17-rel` | replay | 0 / 10s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 567026572a780e2e (2630926 B) | oracle: ticks=9601 packets=4800 ships=80 randSyncErr=0 univSyncErr=2911 blobSyncErr=4746 pktSyncErr=0 records=14405 file=/home/evanmedinger/Documents/GameDev/Homewor ...  converted, 47 ships, 0 size problems;replay started oracle01-r6 players=2 ships=47 tick=1 multiplayer=0; last tick line: replay tick 9471 packets=4736 records=14209 |
| `m3r2i-e-m3-a` | missiontest | 0 / 78s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d737643a58c0f58b (4385810 B) | missiontest: mission=3 ticks=17825 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r2i-e-mission-lodbands` | mission | 0 / 27s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-motion-optout` | motion | 0 / 41s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1431 sampled=1431 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-motion-optout/motion.dump result=PASS; dump motion.dump 1431 frames, sha256 183681235a3fdc7b |
| `m3r2i-e-play01-a` | replay | 0 / 28s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | dd4d8cf27ebae42d (1315714 B) | oracle: ticks=4801 packets=2400 ships=57 randSyncErr=0 univSyncErr=0 blobSyncErr=0 pktSyncErr=0 records=7205 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infe ... unks converted, 54 ships, 0 size problems;replay started hwirec01 players=2 ships=54 tick=1 multiplayer=0; last tick line: replay tick 4607 packets=2304 records=6913 |
| `m3r2i-e-m10-b` | missiontest | 0 / 143s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a72352eb513d8239 (5397854 B) | missiontest: mission=10 ticks=21939 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r2i-e-mission-nolod` | mission | 0 / 27s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-play01-b` | replay | 0 / 29s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | dd4d8cf27ebae42d (1315714 B) | oracle: ticks=4801 packets=2400 ships=57 randSyncErr=0 univSyncErr=0 blobSyncErr=0 pktSyncErr=0 records=7205 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infe ... unks converted, 54 ships, 0 size problems;replay started hwirec01 players=2 ships=54 tick=1 multiplayer=0; last tick line: replay tick 4607 packets=2304 records=6913 |
| `m3r2i-e-motion-on-imm` | motion | 0 / 46s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1431 sampled=1431 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-motion-on-imm/motion.dump result=PASS; dump motion.dump 1431 frames, sha256 c0e411662fa2f527 |
| `m3r2i-e-mission-uisw` | mission | 0 / 28s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-m6-b` | missiontest | 0 / 161s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cc2d450412d30d07 (7629566 B) | missiontest: mission=6 ticks=31011 objectives=7/7 result=PASS; FAIL lines: 0 |
| `m3r2i-e-play02-a` | replay | 0 / 27s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7242658d8c88dc30 (1315721 B) | oracle: ticks=4801 packets=2400 ships=71 randSyncErr=1909 univSyncErr=1984 blobSyncErr=11 pktSyncErr=0 records=7205 file=/home/evanmedinger/Documents/GameDev/Homewor ... nks converted, 103 ships, 0 size problems;replay started hwirec02 players=2 ships=91 tick=1 multiplayer=0; last tick line: replay tick 4607 packets=2304 records=6913 |
| `m3r2i-e-m3-b` | missiontest | 0 / 80s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d737643a58c0f58b (4385810 B) | missiontest: mission=3 ticks=17825 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r2i-e-mission-uicfg` | mission | 0 / 28s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-nis-noi` | nis | 0 / 50s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-nis-noi/nis.dump result=PASS; dump nis.dump 1440 frames, sha256 86cb155acffc180e |
| `m3r2i-e-play02-b` | replay | 0 / 28s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7242658d8c88dc30 (1315721 B) | oracle: ticks=4801 packets=2400 ships=71 randSyncErr=1909 univSyncErr=1984 blobSyncErr=11 pktSyncErr=0 records=7205 file=/home/evanmedinger/Documents/GameDev/Homewor ... nks converted, 103 ships, 0 size problems;replay started hwirec02 players=2 ships=91 tick=1 multiplayer=0; last tick line: replay tick 4607 packets=2304 records=6913 |
| `m3r2i-e-m11-a` | missiontest | 0 / 83s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cc083b2e7ecdc350 (2036509 B) | missiontest: mission=11 ticks=8275 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r2i-e-mission-optout` | mission | 0 / 28s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-mission-ov` | mission | 0 / 28s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-nis-on` | nis | 0 / 51s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-nis-on/nis.dump result=PASS; dump nis.dump 1440 frames, sha256 0307f3be2044c29c |
| `m3r2i-e-m7-a` | missiontest | 0 / 97s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 3de5395abd8de228 (4082492 B) | missiontest: mission=7 ticks=16592 objectives=4/4 result=PASS; FAIL lines: 0 |
| `m3r2i-e-m4-a` | missiontest | 0 / 83s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 9f141e8fb1927d7f (5122089 B) | missiontest: mission=4 ticks=20818 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r2i-e-mission-fl` | mission | 0 / 28s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 / framelog: seconds=10 frames=6000 refresh=144 ft p50=1.6 p95=2.1 p99=2.7 max=1203.634 over=3 (0.1%) ticks=6000 tick_p50=0.500 tick_max=1201.689 heap 41813504->41813504 (+0 B/min) pools=1 dupe=0->0 drift(thirds)=PASS seconds_over_budget=0 |
| `m3r2i-e-m11-b` | missiontest | 0 / 83s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cc083b2e7ecdc350 (2036509 B) | missiontest: mission=11 ticks=8275 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r2i-e-mission-imm` | mission | 0 / 28s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-nis-optout` | nis | 0 / 49s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-nis-optout/nis.dump result=PASS; dump nis.dump 1440 frames, sha256 13ca70eea998e5de |
| `m3r2i-e-mission-nohsa` | mission | 0 / 27s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2131271d771f2056 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r2i-e-m4-b` | missiontest | 0 / 83s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 9f141e8fb1927d7f (5122089 B) | missiontest: mission=4 ticks=20818 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r2i-e-m7-b` | missiontest | 0 / 96s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 3de5395abd8de228 (4082492 B) | missiontest: mission=7 ticks=16592 objectives=4/4 result=PASS; FAIL lines: 0 |
| `m3r2i-e-nis-on-imm` | nis | 0 / 55s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-nis-on-imm/nis.dump result=PASS; dump nis.dump 1440 frames, sha256 0307f3be2044c29c |
| `m3r2i-e-toggle` | toggle | 0 / 13s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=1/1 fails=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-m14-a` | missiontest | 0 / 173s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2ecb7d44c32a850d (5496008 B) | missiontest: mission=14 ticks=22338 objectives=4/4 result=PASS; FAIL lines: 0 |
| `m3r2i-e-toggle-noi` | toggle | 0 / 12s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=0/0 fails=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-m12-a` | missiontest | 0 / 96s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 642575577c73c2de (2705630 B) | missiontest: mission=12 ticks=10995 objectives=3/3 result=PASS; FAIL lines: 0 |
| `m3r2i-e-toggle-nonis` | toggle | 0 / 13s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=1/0 fails=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-toggle-lb` | toggle | 0 / 13s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=1/1 fails=1 result=FAIL; FAIL lines: 1 |
| `m3r2i-e-m5-a` | missiontest | 0 / 72s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 9bf2e49b7712a641 (4031324 B) | missiontest: mission=5 ticks=16384 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r2i-e-toggle-lodbands` | toggle | 0 / 12s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=1/1 fails=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-toggle-ov` | toggle | 0 / 12s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=1/1 fails=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-sig-n01` | nis | 0 / 487s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=1787 frames=15930 sampled=15913 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-sig-n01/ ... 7cfe5568 / framesig: frames=16084 readback=16084 segments=1 outliers=0 (image 0, bars 0, objects 0, camera 0) first=none; short runs=0 (bars 0, objects 0) first=none |
| `m3r2i-e-toggle-imm` | toggle | 0 / 13s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=1/1 fails=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-fe` | fe | 124 / 120s | Main_game_screen | none: still running at the 120s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r2i-e-us-4k-a` | uiscale | 0 / 15s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=3840x2160 logical=1920x1080 scale=200 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-statics` | statics | 0 / 15s | ship statics self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  |  |
| `m3r2i-e-statics-init` | statics | 0 / 15s | ship statics self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  |  |
| `m3r2i-e-us-4k-b` | uiscale | 0 / 16s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=3840x2160 logical=1920x1080 scale=200 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-m12-b` | missiontest | 0 / 97s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 642575577c73c2de (2705630 B) | missiontest: mission=12 ticks=10995 objectives=3/3 result=PASS; FAIL lines: 0 |
| `m3r2i-e-m5-b` | missiontest | 0 / 71s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 9bf2e49b7712a641 (4031324 B) | missiontest: mission=5 ticks=16384 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r2i-e-meshes` | meshes | 0 / 14s | mesh self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | meshes: 1758 listed, 1729 .geo/.peo loaded, 29 .mad loaded, 0 with bad=; self-test done, 1758 files, 0 with bad objects |
| `m3r2i-e-us-1440` | uiscale | 0 / 16s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=2560x1440 logical=1707x960 scale=150 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-us-1080` | uiscale | 0 / 15s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=1920x1080 logical=1920x1080 scale=100 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-textures` | textures | 0 / 20s | texture self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | textures: listing lookups 12403/12403 found, 0 size/flag mismatches, 0 bad sharedFrom;self-test done, 12403 listed, 12403 found in listing, 1742 without a file to load, 10661 registered in 3 batches, 10661 loaded, 0 not loaded, 9248 scaled down, 0 without bScalable, sizeof(texreg) 96 |
| `m3r2i-e-oracle` | oracle | 0 / 12s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 3456d0f9a434bc0b (140723 B) | oracle: ticks=512 packets=256 randSyncErr=0 univSyncErr=0 blobSyncErr=0 records=772 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-oracle/run.hwo; run.hwo 140723 bytes, sha256 3456d0f9a434bc0b |
| `m3r2i-e-us-1024` | uiscale | 0 / 15s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=1024x768 logical=1024x768 scale=100 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-oracle-rel-a` | oracle | 0 / 2s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ba704bca2b4d576b (140723 B) | oracle: ticks=512 packets=256 randSyncErr=0 univSyncErr=0 blobSyncErr=0 records=772 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-oracle-rel-a/run.hwo; run.hwo 140723 bytes, sha256 ba704bca2b4d576b |
| `m3r2i-e-oracle-rel-b` | oracle | 0 / 2s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ba704bca2b4d576b (140723 B) | oracle: ticks=512 packets=256 randSyncErr=0 univSyncErr=0 blobSyncErr=0 records=772 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-oracle-rel-b/run.hwo; run.hwo 140723 bytes, sha256 ba704bca2b4d576b |
| `m3r2i-e-oracle-rel-perturb` | oracle | 0 / 2s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 73ba204d53b2d276 (140773 B) | oracle: ticks=512 packets=256 randSyncErr=0 univSyncErr=0 blobSyncErr=0 records=773 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-oracle-rel-perturb/run.hwo; run.hwo 140773 bytes, sha256 73ba204d53b2d276 |
| `m3r2i-e-us-fit1960` | uiscale | 0 / 15s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=1024x768 logical=1024x768 scale=100 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-etg` | etg | 0 / 14s | etg effect self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | etg: 264 listed; etgStartup done, 8 scripts, 197 effects loaded, 0 referenced but not loaded;self-test done, 9 scripts parsed, 255 .ebg listed, 58 unused (no script  ... but not loaded;created 618 effects;effect self-test done, 618 created, 3197 deleted while running, 1093 left at the end, peak 2515 effects, peak 3012 particle blocks |
| `m3r2i-e-m14-b` | missiontest | 0 / 173s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2ecb7d44c32a850d (5496008 B) | missiontest: mission=14 ticks=22338 objectives=4/4 result=PASS; FAIL lines: 0 |
| `m3r2i-e-us-fit2160` | uiscale | 0 / 15s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=1024x768 logical=1024x768 scale=100 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-dock` | dock | 0 / 26s | dock self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | docktest: fighters=6 outside=6 corvettes=2 carrierleg=2 processorleg=1 RUs=1850 jackPausedTick=1024 result=PASS; FAIL lines: 0 |
| `m3r2i-e-us-4k-cfg100` | uiscale | 0 / 15s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=3840x2160 logical=3840x2160 scale=100 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-us-4k-sw150` | uiscale | 0 / 16s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=3840x2160 logical=2560x1440 scale=150 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-command` | command | 0 / 23s | command self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | commandtest: tick=2998 link=252 mcv=1 unlink=285 research=1/1 mimic=1 point=1/1 result=PASS; FAIL lines: 0 |
| `m3r2i-e-render` | render | 0 / 20s | render self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | rendertest: tick=599 frames=583 objects=582 lensflares=583 foglayers=1166 result=PASS |
| `m3r2i-e-lod-ai-on` | ai | 0 / 32s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0aa609f3d5fd1591 (98768 B) | aitest: tick=398 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS; FAIL lines: 0 / lodtest: tick=300 fullDetail=1 objects=164 drawnAbove0=0 bandAbove0=164 result=PASS |
| `m3r2i-e-sound` | sound | 0 / 20s | sound self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | soundtest: start: audio driver dummy, SFX 1, speech 1, mixer ticks 48;Music4Cat.wxd: 62 streams, track 0 (offset 8, bitrate 512, 2 channels) decoded 43 blocks;SectSp ... done: music 43, sect 124, beast 281 blocks decoded; 403 audible output blocks; mixer ticks 730; result=PASS; UBSan reports in fquant.c/fqcodec.c: 0; PATCH reports: 0 |
| `m3r2i-e-lod-ai-bands` | ai | 0 / 24s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0aa609f3d5fd1591 (98768 B) | aitest: tick=398 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... irmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS; FAIL lines: 0 / lodtest: tick=300 fullDetail=0 objects=164 drawnAbove0=164 bandAbove0=164 result=PASS |
| `m3r2i-e-hud` | hud | 0 / 21s | hud self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | hudtest: tick=599 boxed=19 selected=154 frames=537 statusbars=6840 infopanels=537 pie=537 result=PASS |
| `m3r2i-e-m8-a` | missiontest | 0 / 304s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4b059b09fffc5dd7 (7223667 B) | missiontest: mission=8 ticks=29361 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r2i-e-ui` | ui | 0 / 18s | ui self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uitest: selected=154 rows=10 popupframes=16 commands=26 targetrows=1 statsframes=8 result=PASS;game stats ended active=0 paused=0 |
| `m3r2i-e-fescreens` | fescreens | 0 / 12s | fescreens self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | fescreentest: files=22 loaded=22 screens=195 pushed=195 popped=195 wrong=0 skipped=0 listchanged=0 userregionstandins=84 result=PASS; FAIL lines: 0 / optionstest: options=35 startdefaults=35 readback=35 result=PASS |
| `m3r2i-e-m13-a` | missiontest | 0 / 204s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f7c771f5a3f34d8c (4744478 B) | missiontest: mission=13 ticks=19283 objectives=3/3 result=PASS; FAIL lines: 0 |
| `m3r2i-e-strings` | strings | 0 / 12s | strings self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | stringstest: ids=1705 mapback=1705 resolved=1681 nostring=24 wrongtext=0 wrongnostring=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-keys` | keys | 0 / 12s | keys self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | keystest: rows=197 known=149 unknown=48 commands=147 bindings=149 startsave=1 saved=1 reload=1 reset=1 newkey=1 conflicts=1 shares=1 failures=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-tc-user` | missiontest | 0 / 78s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0ee8c83b4d7bbc4d (3910048 B) | missiontest: mission=1 ticks=2600 objectives=6/6 result=PASS; FAIL lines: 0 / catshot: 8 files (tick=900 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=full l ... after,Mission01_NIS01.mission:level,Mission01.mission:after,Mission01.mission:level,Mission02.mission:after,Mission02.mission:level,Mission02.mission:end result=PASS |
| `m3r2i-e-console` | console | 0 / 17s | console self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | consoletest: selected=154 frames=4 shown=36 set=29 commands=8 keys=32 kas=6 closed=1 failures=0 result=PASS; FAIL lines: 0 |
| `m3r2i-e-build-a` | build | 0 / 30s | build self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8e5128c9a09386a8 (1722370 B) | buildtest: tick=4205 driver=1 SU=40/100 RUs=25925 result=PASS; FAIL lines: 0 |
| `m3r2i-e-m15-a` | missiontest | 0 / 218s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | aa4553c662d87218 (3795164 B) | missiontest: mission=15 ticks=15424 objectives=3/3 result=PASS; FAIL lines: 0 |
| `m3r2i-e-build-b` | build | 0 / 29s | build self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8e5128c9a09386a8 (1722370 B) | buildtest: tick=4205 driver=1 SU=40/100 RUs=25925 result=PASS; FAIL lines: 0 |
| `m3r2i-e-nisparse` | nisparse | 0 / 12s | nisparse self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | nisparsetest: nis=14 nisScripts=14 animatics=18 objects=593 events=1488 cataclysmEvents=31 unknownKeywordLines=47 failed=0 result=PASS; FAIL lines: 0; ignored keywords: motionCueScale x12,stretch x1,closeslipgate x1,minimumLODSet x32,widescreenOut x1 |
| `m3r2i-e-tc-red` | missiontest | 0 / 79s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0ee8c83b4d7bbc4d (3910048 B) | missiontest: mission=1 ticks=2600 objectives=6/6 result=PASS; FAIL lines: 0 / catshot: 8 files (tick=900 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=full l ... after,Mission01_NIS01.mission:level,Mission01.mission:after,Mission01.mission:level,Mission02.mission:after,Mission02.mission:level,Mission02.mission:end result=PASS |
| `m3r2i-e-modequit` | modequit | 0 / 11s | modequit self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | modequit: switched=1 result=PASS (the quit follows: the harness judges rc 0 and the quit line once); rc=0; "Quit game" lines: 1; quit: PASS |
| `m3r2i-e-tc-m3` | missiontest | 0 / 82s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | df68de4e18348963 (4693802 B) | missiontest: mission=3 ticks=17825 objectives=5/5 result=PASS; FAIL lines: 0 / teamcolour: checks=6 failed=0 afterLoads=3 levels=Mission03.mission:after,Mission03.mission:level,Mission04_NIS02.mission:after,Mission04_NIS02.mission:level,Mission04.mission:after,Mission04.mission:end result=PASS |
| `m3r2i-e-m13-b` | missiontest | 0 / 202s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f7c771f5a3f34d8c (4744478 B) | missiontest: mission=13 ticks=19283 objectives=3/3 result=PASS; FAIL lines: 0 |
| `m3r2i-e-sig-nislet01` | nis | 0 / 454s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=1728 frames=15399 sampled=15382 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-sig-nisl ... 9f1cf36e / framesig: frames=15553 readback=15553 segments=1 outliers=0 (image 0, bars 0, objects 0, camera 0) first=none; short runs=0 (bars 0, objects 0) first=none |
| `m3r2i-e-m8-b` | missiontest | 0 / 295s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4b059b09fffc5dd7 (7223667 B) | missiontest: mission=8 ticks=29361 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r2i-e-missiontest-a` | missiontest | 0 / 65s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c14c549ceac7b34f (3246588 B) | missiontest: mission=1 ticks=11424 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r2i-e-m15-b` | missiontest | 0 / 212s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | aa4553c662d87218 (3795164 B) | missiontest: mission=15 ticks=15424 objectives=3/3 result=PASS; FAIL lines: 0 |
| `m3r2i-e-missiontest-b` | missiontest | 0 / 65s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c14c549ceac7b34f (3246588 B) | missiontest: mission=1 ticks=11424 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r2i-e-sp` | sp | 124 / 1201s | universe running (univUpdateCounter > 0) | none: still running at the 1200s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r2i-e-sp-4k` | sp | 124 / 1201s | universe running (univUpdateCounter > 0) | none: still running at the 1200s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r2i-e-tutorial-a` | missiontest | 0 / 43s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 00fbc1aa76d477a2 (2569841 B) | missiontest: ticks=10444 tutorial kas=1 lesson1=2 objectives=2 buttons=1 prompts=7/102/158 pointer=1 next=1108 lesson2=1110 lessons=12/12 complete=12/12 inOrder=1 nextClicks=11 result=PASS; FAIL lines: 0 |
| `m3r2i-e-tutorial-b` | missiontest | 0 / 43s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 00fbc1aa76d477a2 (2569841 B) | missiontest: ticks=10444 tutorial kas=1 lesson1=2 objectives=2 buttons=1 prompts=7/102/158 pointer=1 next=1108 lesson2=1110 lessons=12/12 complete=12/12 inOrder=1 nextClicks=11 result=PASS; FAIL lines: 0 |
| `m3r2i-e-m9-a` | missiontest | 0 / 172s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0f035daa1fa80a6b (5072642 B) | missiontest: mission=9 ticks=20617 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r2i-e-tload-a` | loadgame | 0 / 22s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ed33724271bd859a (886516 B) | loadgame: ticks=3600 from=8361 to=11961 mission=1 tutorial=1 ships=30 objectives=1/1 kasTeams=10 result=PASS |
| `m3r2i-e-sig-n06` | nis | 0 / 257s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=1056 frames=9351 sampled=9316 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r2i-e-sig-n06/ni ... bf37415348 / framesig: frames=9505 readback=9505 segments=1 outliers=0 (image 0, bars 0, objects 0, camera 0) first=none; short runs=0 (bars 0, objects 0) first=none |
| `m3r2i-e-tload-b` | loadgame | 0 / 23s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ed33724271bd859a (886516 B) | loadgame: ticks=3600 from=8361 to=11961 mission=1 tutorial=1 ships=30 objectives=1/1 kasTeams=10 result=PASS |
| `m3r2i-e-fe-4k` | fe | 124 / 120s | Main_game_screen | none: still running at the 120s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r2i-e-m16-a` | missiontest | 0 / 183s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1580bd4cf9152bc8 (5295518 B) | missiontest: mission=16 ticks=21523 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r2i-e-fj3-a` | missiontest | 0 / 68s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a3044330e4a63b5b (4346699 B) | missiontest: mission=3 ticks=17667 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r2i-e-m9-b` | missiontest | 0 / 162s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0f035daa1fa80a6b (5072642 B) | missiontest: mission=9 ticks=20617 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r2i-e-fj3-b` | missiontest | 0 / 69s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a3044330e4a63b5b (4346699 B) | missiontest: mission=3 ticks=17667 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r2i-e-warp10-a` | warpstart | 0 / 17s | warp-start self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 10d1fe5e169c3d0d (24968 B) | ;LevelToWarpTo 10 / ) / warpstarttest: level=10 ticks=99 applied=1 level=1 kas=1 arrived=35 fleet=1 failed=0 result=PASS |
| `m3r2i-e-warp10-b` | warpstart | 0 / 17s | warp-start self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 10d1fe5e169c3d0d (24968 B) | ;LevelToWarpTo 10 / ) / warpstarttest: level=10 ticks=99 applied=1 level=1 kas=1 arrived=35 fleet=1 failed=0 result=PASS |
| `m3r2i-e-mt144` | missiontest | 0 / 176s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f68f68b04e38994d (1641190 B) | missiontest: mission=1 ticks=4898 objectives=1/6 result=FAIL; FAIL lines: 0 / framesig: frames=60031 readback=60031 segments=2 outliers=0 (image 0, bars 0, objects 0, camera 0) first=none; short runs=0 (bars 0, objects 0) first=none |
| `m3r2i-e-m16-b` | missiontest | 0 / 177s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1580bd4cf9152bc8 (5295518 B) | missiontest: mission=16 ticks=21523 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r2i-e-loadgame-a` | loadgame | 0 / 24s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7aba43a5ae46030e (886495 B) | loadgame: ticks=3600 from=1985 to=5585 mission=1 tutorial=2 ships=141 objectives=0/1 kasTeams=36 result=PASS |
| `m3r2i-e-hsa-shot01` | missiontest | 0 / 31s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 6c03c31c4ce9a158 (1173790 B) | missiontest: mission=1 ticks=2998 objectives=0/6 result=FAIL; FAIL lines: 0 / catshot: 130 files (tick=1745 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=full letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r2i-e-loadgame-b` | loadgame | 0 / 24s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7aba43a5ae46030e (886495 B) | loadgame: ticks=3600 from=1985 to=5585 mission=1 tutorial=2 ships=141 objectives=0/1 kasTeams=36 result=PASS |
| `m3r2i-e-hsa-shot01-noasm` | missiontest | 0 / 29s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 6c03c31c4ce9a158 (1173790 B) | missiontest: mission=1 ticks=2998 objectives=0/6 result=FAIL; FAIL lines: 0 / catshot: 130 files (tick=1745 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=full letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r2i-e-hsa-shot02` | missiontest | 0 / 58s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 910644d1d7f05540 (3344247 B) | missiontest: mission=1 ticks=300 objectives=6/6 result=PASS; FAIL lines: 0 / catshot: 324 files (tick=20 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=full l ... after,Mission01_NIS01.mission:level,Mission01.mission:after,Mission01.mission:level,Mission02.mission:after,Mission02.mission:level,Mission02.mission:end result=PASS |
| `m3r2i-e-hsa-shot03` | missiontest | 0 / 66s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d737643a58c0f58b (4385810 B) | missiontest: mission=3 ticks=17825 objectives=5/5 result=PASS; FAIL lines: 0 / catshot: 142 files (tick=20 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=full letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r2i-e-m17-a` | missiontest | 0 / 183s | mission self-test: the game's end (credits) | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a7dfb97bab990c2f (3834034 B) | missiontest: mission=17 ticks=15584 objectives=4/4 result=PASS; FAIL lines: 0 |
| `m3r2i-e-m17-b` | missiontest | 0 / 174s | mission self-test: the game's end (credits) | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a7dfb97bab990c2f (3834034 B) | missiontest: mission=17 ticks=15584 objectives=4/4 result=PASS; FAIL lines: 0 |
| `m3r2i-e-x-mt144-nosig` | missiontest | 0 / 173s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f00b98f9db1c849d (1641190 B) | missiontest: mission=1 ticks=4898 objectives=1/6 result=FAIL; FAIL lines: 0 |
| `m3r2i-e-x-mtdef-sig` | missiontest | 0 / 95s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7be2fb966df4c2fa (1641190 B) | missiontest: mission=1 ticks=4898 objectives=1/6 result=FAIL; FAIL lines: 0 / framesig: frames=6671 readback=6671 segments=2 outliers=1 (image 0, bars 0, objects 1, camera 0) first=3483@1712/s1; short runs=1 (bars 0, objects 1) first=3489@1718/s1 |

## The conditions, met or not

| | condition (the task's wording, abridged) | this step's result | met |
|---|---|---|---|
| (a) | the M3-R1 gates hold: the Mission01 6,000-tick pairs (debug + release) byte-identical and `tools/mission_test.sh --ticks 6000 --perturb 3000 --copy-tick 4000` PASS; the stream IDENTICAL in all 21 fields incl. `rng_all` to `m3r1i-mission-a` (`1ae66610c3f02410`, release `234bcccfb948ab98`), also with `/noInterp`, `/noNISInterp`, `/oracleLibmSqrt`, `letterboxBehind 1` (row and switch), `/lodBands`, `noLOD 0`, `/uiScale 200`, `uiScale 150`, the three opt-outs together, and this round's `/overlay`, `--frame-log`, `/meshImmediate`, `/noHsAssembly`; fe; all self-tests; layout32; the generators `--check`; the dumps vs M3-R1's hashes whole and crc-stripped; every guard-rail stream vs M3-R1's over 21 fields; the UBSan kinds vs M3-R1's nine | the debug pair `2131271d771f2056` (1,476,370 B) a = b, the release pair `ce30c53259f034c8` a = b, the release IDENTICAL to the debug; `mission_test.sh` **MISSION TEST PASS** in both builds (`original/scratch-m2/m3r2-integrate/logs/mission_test_{debug,release}.out`; the perturbed copy first divergent 3000 `rng10`); the 14 flipped-setting runs all byte-identical to A and **IDENTICAL in every field over ticks 0-6000 to `m3r1i-mission-a`** (the table below); fe Main_game_screen at 120 s (1024x768 and 4K); the self-tests PASS **but one check** (the toggle self-test under `/letterboxBehind`: the statement below); layout32 OK; the four generators current; the four dumps byte-equal to M3-R1's whole and crc-stripped; every guard rail IDENTICAL to M3-R1's (the table below); the same nine UBSan kinds, no new kind | **yes**, with the one failing self-test check stated (a check's expectation, not a product fault; owner M3-R3) |
| (b) | missions 01-17 pairs (`--mission-test-ticks 90000 --timeout 3600`) vs M3-R1's; Mission17's credits | 17 / 17 PASS with every objective in order, all 17 pairs byte-identical, ASan 0 in the 34 runs, **every stream IDENTICAL in every field to `m3r1i-mN-a`**; Mission17's milestone `mission self-test: the game's end (credits)` | **yes** |
| (c) | `sp --timeout 1200` default + fe and sp at 3840x2160, ASan 0 | `sp` to Mission12 (12 levels, 129 golden-shot points logged, 23,289 file loads, 12 animatics skipped headless), `sp-4k` to Mission11 (119 points, 22,285 loads; `uiscale: physical 3840x2160 ... scale 200% ... logical 1920x1080`), both rc 124 at 1,200 s by design, ASan 0, no unsuppressed leak, no `DEADLYSIGNAL`; fe and fe-4k Main_game_screen at 120 s, 300 file loads | **yes** |
| (d) | the rung unchanged: the replays 10-17 / 10-18 vs the GOG streams 2823 / 2871, libm 177 / 118, the release replay equal on all but `rng_all`; `ls original/oracle` for `hwirec*-play-*` and newer `oracle01-rec-*` | 10-17 `r17-a` / `r17-b` / `r17-rel` first divergent **2823 `ships_vel`**, `r17-libm` 177 `ships_vel`; 10-18 `r18-a` / `r18-b` **2871 `ships_vel`**, `r18-libm` 118 `ships_pos`; the exclusions `rng6` 161 (both), `real` 2 (10-18); `r17-rel` vs `r17-a` `rng_all` at 6377 only, the 20 other fields IDENTICAL; every replay IDENTICAL in every field to `m3r1i-r17-a` / `m3r1i-r18-a`, the hwirec01 / hwirec02 playbacks to M3-R1's; no `original/oracle/hwirec*-play-*`; the newest `oracle01-rec-*` still `20261019-143530` | **yes** |
| (e) | the four fences rerun with numbers | the next section: fence 1 (the settings line in every run, the overlay in its box, `/frameLog`, modequit PASS); fence 2 (the flicker scans: 0 outliers / 0 short runs in 140,188 frames; the `rng0` question of the 144-fps scan attributed to the virtual frame rate, no simulation field moved) with **the done-when not met by nature** (the flicker is not reproduced; the instrument is proven and in the user's hands, as the port and its audit state); fence 3 (the exe's gate, the stern modules present at 1918-1934, 130 / 130 frames byte-equal to producer 3's final capture); fence 4 (medians 4.6-5.0 vs 13.7 ms, 25,169 vertices, every capture and dump byte-equal between the paths) with **the p99 quoted under a load average of ~30** (12.8-16.1 vs 34-41 ms: the load's number, not the port's) | **yes** (the two statements beside it) |
| (f) | `/bin/grep -rn -E 'CAT-(TODO\|PROVISIONAL)\([^)]*M3-R2' src/` empty (BTG.c:1151 re-targeted to an M3 plan owner, comment-only, then rebuilt); the counts by owner vs M3-R1's; `HWI-FEATURE` / `HWI-FIX` / `HWI-TEST` / `CAT-FIX` counts; `tools/build.sh --debug` and `--release` from clean, 0 warnings | the grep empty after the re-target of `src/Game/BTG.c:1151` to `CAT-TODO(M3-R3)`; 37 `CAT-TODO` lines (M3-R1: 37), 28 owner tags: stage 5 23 (plain 8, multiplayer options 8, LAN 6, multiplayer 1), M3 oracle physics 3, M3 soak 1, **M3-R3 1** (was M3-R2 1); 4 `CAT-PROVISIONAL` (4); `HWI-FEATURE` motion-interp 486 / letterbox-setting 37 / full-detail 179 / ui-scale 300 / **instrumentation 102 / hyperspace-assembly 55 / mesh-batching 160** (new); `HWI-FIX` 62, `HWI-TEST` 286, `CAT-FIX` 195; both builds from clean rc 0, `[274/274]`, 0 warnings, before and after the comment edit | **yes** |

**M3-R2 exit test: MET** on every product condition (**presentation only** held: no simulation field of any stream of the round moved;
the Mission01 stream, the 17 missions, the guard rails and the replays IDENTICAL to M3-R1's, which are R10's), with three statements
that belong beside the verdict:

1. **One self-test check fails.** The toggle self-test under `/letterboxBehind` (`m3r2i-e-toggle-lb`: `toggletest: done start=1/1 fails=1
   result=FAIL`; the other six toggle runs `fails=0 result=PASS`; M3-R1's `m3r1i-toggle-lb` PASS, before the check existed). The failing
   check is producer 1's "settings line: a console row's change prints it with source console" (`src/Game/MotionTest.c:2128-2131`): it
   flips `letterboxBehind` through the console and expects a line with `letterboxBehind=1 (console)`; the run prints `hwi-settings:
   objInterpolate=1 (console) nisInterpolate=1 (console) letterboxBehind=1 (switch) ... switches=/letterboxBehind [console
   letterboxBehind]`. The line follows its rule: `Instrument.c:200-203` labels a row by its standing switch before any announced source
   ("`switch`: a run-only switch stands", docs/m3-r2-instrumentation.md), and `rndLetterboxSwitchStands()` stays TRUE through a console
   change (`render.c:5138`; the console row writes `letterboxBehind` directly, `Console.c:149`), exactly as `/noInterp` + a console row
   gives `objInterpolate=1 (switch)` in `m3r2i-e-toggle-noi`, which producer 1 documented as faithful reporting
   (docs/m3-r2-instrumentation.md:415). Neither producer 1 nor its audit ran the toggle self-test with `/letterboxBehind` (their runs:
   plain, `/noInterp`, `/noNISInterp`, `/overlay`, `/lodBands`; the audit's final `m3r2a-ins-ftoggle`), so the check's wrong
   expectation under the switch went unseen until this exit test ran the gate's "letterboxBehind 1 (row and switch)" against it. The
   product's behaviour is the documented one; the fix is one line either way (the check accepts `(switch)` while
   `rndLetterboxSwitchStands()`, or a console row ends the switch's standing for the label as `/overlay`'s does, `Instrument.c:106`),
   **owner M3-R3**; not fixed here (this step edits no code but the BTG comment, and a fix would need its own audit). Nothing else in
   the 162 runs reports a failure: every other `result=FAIL` in a summary is a tick budget by design (`mt144` and the two attribution
   runs `objectives=1/6` at 4,898 ticks, `hsa-shot01` / `-noasm` `0/6` at 2,998, the motion-test scans `the NIS ended before the window
   did`), as in the producers' and audits' runs of the same shape.
2. **Fence 2's done-when is not met by nature**: the flicker is not reproduced (0 outliers and 0 short runs in this step's 140,188
   frames, producer 2's 450,644, the audit's reruns); the instrument (`/catFrameSig`, `tools/dump_outliers.py`) is proven on the audit's
   forced frame and is in the user's hands (the brief); no engine change was made for it.
3. **Fence 4's p99 is quoted under this machine's load** (load average 24-37 through the step): the medians agree with the producer's
   idle numbers, the p99 does not; the number is the load's, the soak's attended hour on a quiet machine is the measurement (decision 132).

### The Mission01 stream, every setting flipped (condition (a))

| run (debug unless stated) | sha256 (size) | vs `m3r1i-mission-a` (`1ae66610c3f02410`), 21 fields |
|---|---|---|
| Mission01 mission-a | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-b | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-noi | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-nonis | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-libm | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-lb | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-lbsw | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-lodbands | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-nolod | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-uisw | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-uicfg | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-optout | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-ov | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-fl | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-imm | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-nohsa | `2131271d771f2056` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-rel-a | `ce30c53259f034c8` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 mission-rel-b | `ce30c53259f034c8` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 release vs this step's debug | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 release vs m3r1i-mission-rel-a | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 perturb vs a | FIRST DIVERGENT TICK 3000 (last equal tick 2999); 2 field(s) differ, likely origin: rng10 |

The `settings:` line of each run names its source (`mission-optout`: `objInterpolate=0 (switch) nisInterpolate=0 (switch) letterboxBehind=1
(cfg) noLOD=0 (switch) ... switches=/noInterp,/lodBands`); the release pair is IDENTICAL to the debug pair and to `m3r1i-mission-rel-a`
(`234bcccfb948ab98`); the perturbed copy diverges at 3000 in `rng10` and `rng_all` (the instrument works).

### The rung (condition (d))

| replay | `hwo_compare.py` vs the GOG stream (decision 47's exclusions, decision 60's `real`) |
|---|---|
| 10-17 r17-a | FIRST DIVERGENT TICK 2823 (last equal tick 2822); 1 field(s) differ, likely origin: ships_vel |
| 10-17 r17-b | FIRST DIVERGENT TICK 2823 (last equal tick 2822); 1 field(s) differ, likely origin: ships_vel |
| 10-17 r17-rel | FIRST DIVERGENT TICK 2823 (last equal tick 2822); 1 field(s) differ, likely origin: ships_vel |
| 10-17 r17-libm | FIRST DIVERGENT TICK 177 (last equal tick 176); 1 field(s) differ, likely origin: ships_vel |
| 10-18 r18-a | FIRST DIVERGENT TICK 2871 (last equal tick 2870); 1 field(s) differ, likely origin: ships_vel |
| 10-18 r18-b | FIRST DIVERGENT TICK 2871 (last equal tick 2870); 1 field(s) differ, likely origin: ships_vel |
| 10-18 r18-libm | FIRST DIVERGENT TICK 118 (last equal tick 117); 2 field(s) differ, likely origin: ships_pos |
| 10-17 exclusion rng6 | FIRST DIVERGENT TICK 161 (last equal tick 160); 1 field(s) differ, likely origin: rng6 |
| 10-18 exclusion rng6 | FIRST DIVERGENT TICK 161 (last equal tick 160); 1 field(s) differ, likely origin: rng6 |
| 10-18 exclusion real | FIRST DIVERGENT TICK 2 (last equal tick 1); 1 field(s) differ, likely origin: real |
| 10-17 release vs debug (all fields) | FIRST DIVERGENT TICK 6377 (last equal tick 6376); 1 field(s) differ, likely origin: rng_all |
| 10-17 release vs debug, rng_all exempt | IDENTICAL: 9601 ticks and 4800 packets compared (ticks 1-9601), no difference |

The replays' hashes this step: 10-17 `c3b7d873ae8fb692` (a = b), `/oracleLibmSqrt` `3823b695ea8daefd`, release `567026572a780e2e`; 10-18
`bd2c316848aff30c` (a = b), `/oracleLibmSqrt` `b63b7226f8f7c6a3`; hwirec01 `dd4d8cf27ebae42d` (a = b), hwirec02 `7242658d8c88dc30` (a = b);
every one IDENTICAL in every field to M3-R1's (the guard-rail table). `ls original/oracle`: no `hwirec*-play-*` (mode B not run), the
newest recording `oracle01-rec-20261019-143530` (decision 48 open).

### The guard rails against M3-R1's (conditions (a), (b), (d))

| guard rail (ticks) | M3-R1's `m3r1i-*` hash | this step's hash | `hwo_compare.py`, 21 fields | the mode's line / note |
|---|---|---|---|---|
| ai, 400 | `e4a40ecfcc025c23` | `d982c0dcae02869b` (a = b) | IDENTICAL: 401 ticks and 0 packets compared (ticks 0-400), no difference | tick=398 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attac |
| ai soak, 4,000 (20 fields, rng_all exempt: decision 84) | `7d8acdc92a183d71` | `5b53f300ce915205` | IDENTICAL: 4001 ticks and 0 packets compared (ticks 0-4000), no difference | tick=3998 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 atta |
| ai soak, rng_all | `7d8acdc92a183d71` | `5b53f300ce915205` | IDENTICAL: 4001 ticks and 0 packets compared (ticks 0-4000), no difference | tick=3998 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 atta |
| skirmish LFE2, 600 | `19191afed0025546` | `a14774ffd6287143` (a = b) | IDENTICAL: 601 ticks and 0 packets compared (ticks 0-600), no difference |  |
| skirmish Armed8, 600 | `86da91da0639f535` | `c996a4b4a7314be7` | IDENTICAL: 601 ticks and 0 packets compared (ticks 0-600), no difference |  |
| build, 7,000 | `dd269f1a6fdfa4ba` | `8e5128c9a09386a8` (a = b) | IDENTICAL: 7001 ticks and 0 packets compared (ticks 0-7000), no difference |  |
| missiontest (Mission01), NIS-first | `96c903b14adfa8b6` | `c14c549ceac7b34f` (a = b) | IDENTICAL: 11426 ticks and 0 packets compared (ticks 0-11425), no difference | mission=1 ticks=11424 objectives=6/6 result=PASS; FAIL lines: 0 |
| loadgame (01 - Hiigara), 3,600 | `ea5a09243505bae6` | `7aba43a5ae46030e` (a = b) | IDENTICAL: 3603 ticks and 0 packets compared (ticks 1984-5586), no difference | ticks=3600 from=1985 to=5585 mission=1 tutorial=2 ships=141 objectives=0/1 kasTeams=36 result=PASS |
| warp start (level 10) | `0e0f349f1b10bef7` | `10d1fe5e169c3d0d` (a = b) | IDENTICAL: 101 ticks and 0 packets compared (ticks 0-100), no difference |  |
| Tutorial1 leg | `2f361c2f043a48b4` | `00fbc1aa76d477a2` (a = b) | IDENTICAL: 10446 ticks and 0 packets compared (ticks 0-10445), no difference |  |
| lesson load (LESSON 11), 3,600 | `f04fe397ba093242` | `ed33724271bd859a` (a = b) | IDENTICAL: 3603 ticks and 0 packets compared (ticks 8360-11962), no difference | ticks=3600 from=8361 to=11961 mission=1 tutorial=1 ships=30 objectives=1/1 kasTeams=10 result=PASS |
| failed jump (Mission03) | `7a2bc8c3021eb23b` | `a3044330e4a63b5b` (a = b) | IDENTICAL: 17669 ticks and 0 packets compared (ticks 0-17668), no difference | mission=3 ticks=17667 objectives=5/5 result=PASS; FAIL lines: 0 |
| replay 10-17 | `53edd651fd28d8c7` | `c3b7d873ae8fb692` (a = b) | IDENTICAL: 9601 ticks and 4800 packets compared (ticks 1-9601), no difference |  |
| replay 10-17 /oracleLibmSqrt | `77f190c6425302db` | `3823b695ea8daefd` | IDENTICAL: 9601 ticks and 4800 packets compared (ticks 1-9601), no difference |  |
| replay 10-17 release | `82bc6c9a8bc4ac50` | `567026572a780e2e` | IDENTICAL: 9601 ticks and 4800 packets compared (ticks 1-9601), no difference |  |
| replay 10-18 | `cb9bec70d3fbe31c` | `bd2c316848aff30c` (a = b) | IDENTICAL: 3527 ticks and 1763 packets compared (ticks 1-3527), no difference |  |
| replay 10-18 /oracleLibmSqrt | `b580ecc5c58914d5` | `b63b7226f8f7c6a3` | IDENTICAL: 3527 ticks and 1763 packets compared (ticks 1-3527), no difference |  |
| hwirec01 playback (Armed8) | `1a29c207ff9efcb8` | `dd4d8cf27ebae42d` (a = b) | IDENTICAL: 4801 ticks and 2400 packets compared (ticks 1-4801), no difference |  |
| hwirec02 playback (LFE2) | `02bc810a29e13605` | `7242658d8c88dc30` (a = b) | IDENTICAL: 4801 ticks and 2400 packets compared (ticks 1-4801), no difference |  |
| oracle self-test (debug) | `a2eadbb91bf73b6a` | `3456d0f9a434bc0b` | IDENTICAL: 513 ticks and 256 packets compared (ticks 0-512), no difference |  |
| oracle self-test (release) | `4171cb5f07090dc0` | `ba704bca2b4d576b` (a = b) | IDENTICAL: 513 ticks and 256 packets compared (ticks 0-512), no difference |  |
| team colours, the user's scheme (missiontest into Mission02) | `6c809b4b19a7282a` | `0ee8c83b4d7bbc4d` | IDENTICAL: 11521 ticks and 0 packets compared (ticks 0-11520), no difference | checks=7 failed=0 afterLoads=3 levels=Mission01_NIS01.mission:after,Mission01_NIS01.mission:level,Mission01.mission:afte |
| team colours, red (missiontest into Mission02) | `6c809b4b19a7282a` | `0ee8c83b4d7bbc4d` | IDENTICAL: 11521 ticks and 0 packets compared (ticks 0-11520), no difference | checks=7 failed=0 afterLoads=3 levels=Mission01_NIS01.mission:after,Mission01_NIS01.mission:level,Mission01.mission:afte |
| team colours Mission03 | `e42c134acb5dfb96` | `df68de4e18348963` | IDENTICAL: 17794 ticks and 0 packets compared (ticks 0-17793), no difference | checks=6 failed=0 afterLoads=3 levels=Mission03.mission:after,Mission03.mission:level,Mission04_NIS02.mission:after,Miss |
| lodtest ai on | `f79c5d3e80f479d7` | `0aa609f3d5fd1591` | IDENTICAL: 401 ticks and 0 packets compared (ticks 0-400), no difference | tick=300 fullDetail=1 objects=164 drawnAbove0=0 bandAbove0=164 result=PASS |
| lodtest ai /lodBands | `f79c5d3e80f479d7` | `0aa609f3d5fd1591` | IDENTICAL: 401 ticks and 0 packets compared (ticks 0-400), no difference | tick=300 fullDetail=0 objects=164 drawnAbove0=164 bandAbove0=164 result=PASS |
| perf4k batched (release ai 500) | `3835134bb5b82760` | `2d615f67ed8009bb` | IDENTICAL: 501 ticks and 0 packets compared (ticks 0-500), no difference |  |
| perf4k /meshImmediate | `3835134bb5b82760` | `2d615f67ed8009bb` | IDENTICAL: 501 ticks and 0 packets compared (ticks 0-500), no difference |  |
| overlay ai capture: on vs off (240 ticks, --lodstats) | `e43b7db0b1a89218` | `e43b7db0b1a89218` | IDENTICAL: 241 ticks and 0 packets compared (ticks 0-240), no difference |  |
| overlay ai capture vs producer 1's m3r2-ov-ai-on | `dec1f7be9c1a2e71` | `e43b7db0b1a89218` | IDENTICAL: 241 ticks and 0 packets compared (ticks 0-240), no difference |  |
| overlay 4K capture: on vs off | `e43b7db0b1a89218` | `e43b7db0b1a89218` | IDENTICAL: 241 ticks and 0 packets compared (ticks 0-240), no difference |  |
| a `--lodstats` ai run (this step's `perf4k-bat1`, 500 ticks) vs the plain ai run `m3r1i-ai-a` (400 ticks) | `e4a40ecfcc025c23` | `2d615f67ed8009bb` | FIRST DIVERGENT TICK 18 (last equal tick 17); 1 field(s) differ, likely origin: rng_all | the lodstats camera walk's frames draw from the shared stream 0 (`rng_all`); not the round's: the next two rows |
| M3-R1's own `--lodstats` run vs its plain ai run (`m3r1i-perf4k-on` vs `m3r1i-ai-a`) | `e4a40ecfcc025c23` | `3835134bb5b82760` | FIRST DIVERGENT TICK 18 (last equal tick 17); 1 field(s) differ, likely origin: rng_all | the same divergence in M3-R1's own runs |
| this step's `perf4k-bat1` vs M3-R1's `m3r1i-perf4k-on` | `3835134bb5b82760` | `2d615f67ed8009bb` | IDENTICAL: 501 ticks and 0 packets compared (ticks 0-500), no difference | the `--lodstats` runs of the two rounds agree in every field |
| hyperspace capture Mission01 (`hsa-shot01`, 3,000 ticks) | `96c903b14adfa8b6` (`m3r1i-missiontest-a`) | `6c03c31c4ce9a158` | IDENTICAL: 3000 ticks and 0 packets compared (ticks 0-2999), no difference (`--to-tick 2999`: the capture quits at 3000 and the reference runs to 11425; without the bound the tool reports `event records differ` (the quit events at different ticks) and no verdict) | hsgate tick 1749; `done mission=1 ticks=2998 objectives=0/6 result=FAIL` is the 3,000-tick budget (6 / 6 needs 11,424), as the producer's and the audit's captures |
| hyperspace capture `/noHsAssembly` (`hsa-shot01-noasm`) | `96c903b14adfa8b6` | `6c03c31c4ce9a158` | IDENTICAL: 3000 ticks and 0 packets compared (ticks 0-2999), no difference (`--to-tick 2999`); IDENTICAL to `hsa-shot01` over every tick | the same hsgate with `assembly parts=0` |
| hyperspace capture Mission02 arrival (tc) | `6c809b4b19a7282a` | `910644d1d7f05540` | IDENTICAL: 11521 ticks and 0 packets compared (ticks 0-11520), no difference |  |
| hyperspace capture Mission03 departure | `0e52719366f43770` | `d737643a58c0f58b` | IDENTICAL: 17794 ticks and 0 packets compared (ticks 0-17793), no difference |  |
| flicker scan: the Mission01 self-test at 144 virtual fps (`mt144`, release, 4,900 ticks) | `96c903b14adfa8b6` | `f68f68b04e38994d` | FIRST DIVERGENT TICK 5 (last equal tick 4); 1 field(s) differ, likely origin: rng_all; **the 19 simulation fields** (`--to-tick 4899 --fields` every field but `rng0` / `rng_all`) **IDENTICAL: 4900 ticks and 0 packets compared (ticks 0-4899), no difference**; `rng0` alone first differs at 4759 (`CAND is 300 draw(s) ahead of REF`: `RANDOM_STREAM_0`, the shared particle stream); attributed to `--virtual-fps 144` alone, not to `--frame-sig` (fence 2 below); IDENTICAL in all 21 fields to producer 2's `m3r2-nf-fsig-mt144` | `done mission=1 ticks=4898 objectives=1/6 result=FAIL` is the 4,900-tick budget, as producer 2's run |

### Missions 01-17 (condition (b))

| mission | the self-test line | hash (a) | pair (b) | ASan a / b | against M3-R1 (`m3r1i-mN-a`) | milestone |
|---|---|---|---|---|---|---|
| 1 | mission=1 ticks=11424 objectives=6/6 result=PASS; FAIL lines: 0 | `c14c549ceac7b34f` (3246588 B) | byte-identical (c14c549ceac7b34f) | ASan errors: 0 / 0 | vs `m3r1i-missiontest-a`: IDENTICAL: 11426 ticks and 0 packets compared (ticks 0-11425), no difference | mission self-test done |
| 2 | mission=2 ticks=25936 objectives=6/6 result=PASS; FAIL lines: 0 | `7497c9e5900b59b0` (6381116 B) | byte-identical (7497c9e5900b59b0) | ASan errors: 0 / 0 | vs `m3r1i-m2-a`: IDENTICAL: 25905 ticks and 0 packets compared (ticks 0-25904), no difference | mission self-test done |
| 3 | mission=3 ticks=17825 objectives=5/5 result=PASS; FAIL lines: 0 | `d737643a58c0f58b` (4385810 B) | byte-identical (d737643a58c0f58b) | ASan errors: 0 / 0 | vs `m3r1i-m3-a`: IDENTICAL: 17794 ticks and 0 packets compared (ticks 0-17793), no difference | mission self-test done |
| 4 | mission=4 ticks=20818 objectives=5/5 result=PASS; FAIL lines: 0 | `9f141e8fb1927d7f` (5122089 B) | byte-identical (9f141e8fb1927d7f) | ASan errors: 0 / 0 | vs `m3r1i-m4-a`: IDENTICAL: 20787 ticks and 0 packets compared (ticks 0-20786), no difference | mission self-test done |
| 5 | mission=5 ticks=16384 objectives=6/6 result=PASS; FAIL lines: 0 | `9bf2e49b7712a641` (4031324 B) | byte-identical (9bf2e49b7712a641) | ASan errors: 0 / 0 | vs `m3r1i-m5-a`: IDENTICAL: 16353 ticks and 0 packets compared (ticks 0-16352), no difference | mission self-test done |
| 6 | mission=6 ticks=31011 objectives=7/7 result=PASS; FAIL lines: 0 | `cc2d450412d30d07` (7629566 B) | byte-identical (cc2d450412d30d07) | ASan errors: 0 / 0 | vs `m3r1i-m6-a`: IDENTICAL: 30980 ticks and 0 packets compared (ticks 0-30979), no difference | mission self-test done |
| 7 | mission=7 ticks=16592 objectives=4/4 result=PASS; FAIL lines: 0 | `3de5395abd8de228` (4082492 B) | byte-identical (3de5395abd8de228) | ASan errors: 0 / 0 | vs `m3r1i-m7-a`: IDENTICAL: 16561 ticks and 0 packets compared (ticks 0-16560), no difference | mission self-test done |
| 8 | mission=8 ticks=29361 objectives=6/6 result=PASS; FAIL lines: 0 | `4b059b09fffc5dd7` (7223667 B) | byte-identical (4b059b09fffc5dd7) | ASan errors: 0 / 0 | vs `m3r1i-m8-a`: IDENTICAL: 29330 ticks and 0 packets compared (ticks 0-29329), no difference | mission self-test done |
| 9 | mission=9 ticks=20617 objectives=6/6 result=PASS; FAIL lines: 0 | `0f035daa1fa80a6b` (5072642 B) | byte-identical (0f035daa1fa80a6b) | ASan errors: 0 / 0 | vs `m3r1i-m9-a`: IDENTICAL: 20586 ticks and 0 packets compared (ticks 0-20585), no difference | mission self-test done |
| 10 | mission=10 ticks=21939 objectives=5/5 result=PASS; FAIL lines: 0 | `a72352eb513d8239` (5397854 B) | byte-identical (a72352eb513d8239) | ASan errors: 0 / 0 | vs `m3r1i-m10-a`: IDENTICAL: 21908 ticks and 0 packets compared (ticks 0-21907), no difference | mission self-test done |
| 11 | mission=11 ticks=8275 objectives=5/5 result=PASS; FAIL lines: 0 | `cc083b2e7ecdc350` (2036509 B) | byte-identical (cc083b2e7ecdc350) | ASan errors: 0 / 0 | vs `m3r1i-m11-a`: IDENTICAL: 8244 ticks and 0 packets compared (ticks 0-8243), no difference | mission self-test done |
| 12 | mission=12 ticks=10995 objectives=3/3 result=PASS; FAIL lines: 0 | `642575577c73c2de` (2705630 B) | byte-identical (642575577c73c2de) | ASan errors: 0 / 0 | vs `m3r1i-m12-a`: IDENTICAL: 10964 ticks and 0 packets compared (ticks 0-10963), no difference | mission self-test done |
| 13 | mission=13 ticks=19283 objectives=3/3 result=PASS; FAIL lines: 0 | `f7c771f5a3f34d8c` (4744478 B) | byte-identical (f7c771f5a3f34d8c) | ASan errors: 0 / 0 | vs `m3r1i-m13-a`: IDENTICAL: 19252 ticks and 0 packets compared (ticks 0-19251), no difference | mission self-test done |
| 14 | mission=14 ticks=22338 objectives=4/4 result=PASS; FAIL lines: 0 | `2ecb7d44c32a850d` (5496008 B) | byte-identical (2ecb7d44c32a850d) | ASan errors: 0 / 0 | vs `m3r1i-m14-a`: IDENTICAL: 22307 ticks and 0 packets compared (ticks 0-22306), no difference | mission self-test done |
| 15 | mission=15 ticks=15424 objectives=3/3 result=PASS; FAIL lines: 0 | `aa4553c662d87218` (3795164 B) | byte-identical (aa4553c662d87218) | ASan errors: 0 / 0 | vs `m3r1i-m15-a`: IDENTICAL: 15393 ticks and 0 packets compared (ticks 0-15392), no difference | mission self-test done |
| 16 | mission=16 ticks=21523 objectives=6/6 result=PASS; FAIL lines: 0 | `1580bd4cf9152bc8` (5295518 B) | byte-identical (1580bd4cf9152bc8) | ASan errors: 0 / 0 | vs `m3r1i-m16-a`: IDENTICAL: 21492 ticks and 0 packets compared (ticks 0-21491), no difference | mission self-test done |
| 17 | mission=17 ticks=15584 objectives=4/4 result=PASS; FAIL lines: 0 | `a7dfb97bab990c2f` (3834034 B) | byte-identical (a7dfb97bab990c2f) | ASan errors: 0 / 0 | vs `m3r1i-m17-a`: IDENTICAL: 15585 ticks and 0 packets compared (ticks 0-15584), no difference | mission self-test: the game's end (credits) |

### The dumps (condition (a); decisions 46 / 118)

| dump (`m3r2i-e-*`) | M3-R1's `m3r1i-*` whole / crc-stripped | this step's whole / crc-stripped | verdict | lines |
|---|---|---|---|---|
| motion-noi | c503c869f0dc9cdb / 3016280e9e0dd3c8 | c503c869f0dc9cdb / 3016280e9e0dd3c8 | byte-equal to m3r1i | 1436 lines |
| motion-on | c0e411662fa2f527 / 428ef41479eb5749 | c0e411662fa2f527 / 428ef41479eb5749 | byte-equal to m3r1i | 1436 lines |
| motion-optout | 183681235a3fdc7b / 3016280e9e0dd3c8 | 183681235a3fdc7b / 3016280e9e0dd3c8 | byte-equal to m3r1i | 1436 lines |
| motion-on-imm | (no m3r1i run) | c0e411662fa2f527 / 428ef41479eb5749 |  | 1436 lines |
| nis-noi | 86cb155acffc180e / 196bab32d244f614 | 86cb155acffc180e / 196bab32d244f614 | byte-equal to m3r1i | 1489 lines |
| nis-on | 0307f3be2044c29c / a5de13a44b8d264f | 0307f3be2044c29c / a5de13a44b8d264f | byte-equal to m3r1i | 1489 lines |
| nis-optout | 13ca70eea998e5de / 196bab32d244f614 | 13ca70eea998e5de / 196bab32d244f614 | byte-equal to m3r1i | 1489 lines |
| nis-on-imm | (no m3r1i run) | 0307f3be2044c29c / a5de13a44b8d264f |  | 1489 lines |

The opt-outs together (`letterboxBehind 1` + `/lodBands` + `/noInterp`) against R10's and M3-R1's `/noInterp` dumps, and the `/meshImmediate` twins:
- `motion-optout` byte-equal to r10i-motion-noi
- `motion-optout` DIFFERS from m3r1i-motion-noi: 0 crc-stripped lines differ, 603 whole
- `motion-on` byte-equal to motion-on-imm (/meshImmediate)
- `nis-optout` byte-equal to r10i-nis-noi
- `nis-optout` DIFFERS from m3r1i-nis-noi: 0 crc-stripped lines differ, 1260 whole
- `nis-on` byte-equal to nis-on-imm (/meshImmediate)

Producer 4's final references (`m3r2-mbf-*`, the last dumps of the round):
- `m3r2-mbf-motion-noi` c503c869f0dc9cdb / 3016280e9e0dd3c8 EQUAL
- `m3r2-mbf-motion-on` c0e411662fa2f527 / 428ef41479eb5749 EQUAL
- `m3r2-mbf-nis-noi` 86cb155acffc180e / 196bab32d244f614 EQUAL
- `m3r2-mbf-nis-on` 0307f3be2044c29c / a5de13a44b8d264f EQUAL

Every dump is byte-equal to M3-R1's reference (decision 118) whole and crc-stripped: no producer moved a dump, nothing is re-baselined;
the opt-outs together are still byte-equal to R10's `/noInterp` dumps (603 / 1,260 crc-only lines from this step's defaults, as in
M3-R1); the batched and the immediate mesh path draw the same bytes.

### The self-tests (condition (a))

Every self-test PASS but the one toggle check above: statics x2 (`ship statics self-test done`), meshes (`1758 files, 0 with bad
objects`), textures (`12403 listed, 12403 found in listing, ... 10661 registered in 3 batches, 10661 loaded, 0 not loaded`), etg (`9
scripts parsed, 255 .ebg listed, ... 197 effects loaded, 0 referenced but not loaded`), oracle (`ticks=512 packets=256 randSyncErr=0
univSyncErr=0 blobSyncErr=0 records=772`; the release SELFTEST pair a = b), dock (`fighters=6 outside=6 corvettes=2 carrierleg=2
processorleg=1 RUs=1850 ... result=PASS`), command (`link=252 mcv=1 unlink=285 research=1/1 mimic=1 point=1/1 result=PASS`), render
(`tick=599 frames=583 objects=582 lensflares=583 foglayers=1166 result=PASS`), sound (the dummy driver, `Music4Cat.wxd: 62 streams`),
hud (`tick=599 boxed=19 selected=154 frames=537 statusbars=6840 infopanels=537 pie=537 result=PASS`), ui (`selected=154 rows=10
popupframes=16 commands=26 targetrows=1 statsframes=8 result=PASS`), fescreens (`options=35 startdefaults=35 readback=35 result=PASS`,
fescreen UBSan 0), strings (`ids=1705 mapback=1705 resolved=1681 nostring=24 wrongtext=0 wrongnostring=0 result=PASS`), keys (`rows=197
known=149 unknown=48 commands=147 bindings=149 ... failures=0 result=PASS`), console (`shown=36 set=29 commands=8 keys=32 kas=6 closed=1
failures=0 result=PASS`, `tablerows=486`), build pair (`tick=4205 driver=1 SU=40/100 RUs=25925 result=PASS`), nisparse (`nis=14
nisScripts=14 animatics=18 objects=593 events=1488 cataclysmEvents=31 unknownKeywordLines=47 failed=0 result=PASS`), modequit (fence 1),
toggle 6 of 7, uiscale x9 (`checks=45 fails=0` each), the missiontest / loadgame / warp / tload pairs, Tutorial1 (`lessons=12/12
complete=12/12 inOrder=1 ... result=PASS`), ai pair + the soak, skirmish LFE2 pair (`ticks=600 ships=91`) + Armed8 (`ships=54`), the
team colours (7 / 7 and 6 / 6 checks), lodtest (on + `/lodBands`); every quitting run rc 0; layout32 OK; the generators `--check` current.

### UBSan / ASan (condition (a))

ASan 0 and no unsuppressed leak in all 162 runs; fescreen UBSan 0 in all. The UBSan kinds (`UBSan report kinds (count, site)` blocks of
the 162 summaries, `an_ubsan.py`): **nine kinds, the same nine as M3-R1's and R10's, no new kind**; 48 runs report nothing (the release
runs, fe, the self-tests that load no universe); the first report of every universe run is `BTG.c:389:35` (a misaligned `short` load in
the background's star data), as in M3-R1.

| reports | runs | kind (all non-fatal, the same nine as M3-R1's and R10's) |
|---:|---:|---|
| 9383 | 61 | `Game/Particle.c: member access within misaligned address X for type 'struct particle', which requires 8 byte alignment` |
| 2626 | 114 | `Game/ETG.c: member access within misaligned address X for type 'struct etgdecision', which requires 8 byte alignment` |
| 1536 | 114 | `Game/ETG.c: member access within misaligned address X for type 'struct etgfunctioncall', which requires 8 byte alignment` |
| 1438 | 114 | `Game/ETG.c: member access within misaligned address X for type 'struct etgconditional', which requires 8 byte alignment` |
| 1024 | 114 | `Game/ETG.c: member access within misaligned address X for type 'struct etgbranch', which requires 8 byte alignment` |
| 970 | 60 | `Game/ETG.c: member access within misaligned address X for type 'struct particle', which requires 8 byte alignment` |
| 226 | 113 | `Game/BTG.c: load of misaligned address X for type 'short unsigned int', which requires 2 byte alignment` |
| 194 | 59 | `Game/ETG.c: member access within misaligned address X for type 'struct etgvariablecopy', which requires 8 byte alignment` |
| 56 | 56 | `Game/BTG.c: load of misaligned address X for type 'udword', which requires 4 byte alignment` |

## The four fences, rerun by this step

Every fence on the plain harness from the copied-aside binaries (chains G and H; H alone first: the release binary's GPU batch on the
idle GPU, nvidia-smi 5 % before, 16 % / 11 % between its groups, 0 % after, no game running, 120 s of <= 40 % waited before each
group, `claude-gpu-done` at 14:49:48; the CPU load average 24-37 throughout the step: the user's VS Code ripgrep at 600-1,000 %). The
runs' own lines are in the runs table; the comparisons below are this step's (`tools/ppm_region.py diff`, `cmp`, `hwo_compare.py`,
`tools/dump_outliers.py --oneline`, `tools/frame_log_report.py --oneline`).

### Fence 1: instrumentation (producer 1, audit 0.89): the settings line, the overlay, `/frameLog`, the quit after a resolution change

- **The settings line** is in every run's log and in every `settings:` summary line. The default line (`m3r2i-e-mission-a` and every run
  without a switch or cfg row): `hwi-settings: objInterpolate=1 (default) nisInterpolate=1 (default) letterboxBehind=0 (default) noLOD=1
  (default) uiScale=0->100% (default, auto) overlay=0 (default) meshImmediate=0 (default) fit=window (as requested (no desktop bounds))
  1024x768 switches=none [start]`. The sources label the switches and the rows: `mission-optout` (`/noInterp /lodBands` and
  `letterboxBehind 1` in the options file) `objInterpolate=0 (switch) nisInterpolate=0 (switch) letterboxBehind=1 (cfg) noLOD=0 (switch)
  ... switches=/noInterp,/lodBands`; `mission-uisw` `uiScale=200->160% (switch)` (the fit clamps the row at 1024x768); `mission-ov`
  `overlay=1 (switch)`; `mission-imm` `meshImmediate=1 (switch)`; the 4K uiscale run 5 lines, the toggle self-test 15 (`[console ..]`,
  `[key overlay]`). In the toggle runs a console row's change prints `(console)` and the Menu key `(key)`; under a standing switch the
  row keeps `(switch)` (`toggle-noi`: `objInterpolate=1 (switch)` after the console set it; `toggle-lb`: `letterboxBehind=1 (switch) ...
  [console letterboxBehind]`), which is the rule producer 1 documented (docs/m3-r2-instrumentation.md:415) and the one check the
  toggle self-test gets wrong under `/letterboxBehind` (the exit-test statement above).
- **The overlay** (the ai scene, 240 ticks, `--lodstats`, the on / off pairs at 1024x768 and at 3840x2160): the difference between
  the on and the off frame is confined to the top-left box, `differ=16064 bbox=2,2,253,66 maxdiff=185` at 1024x768 and
  `differ=64256 bbox=4,4,506,132` at 4K (the text's colour 255,255,160 within tol 40: 1,637 / 6,556 pixels); against producer 1's
  captures the on frames differ in 357 / 1,248 pixels inside the digits only (`bbox=45,7,249,26` / `98,14,498,52`: the frame times
  shown), the off frames in 0; the streams of the on and the off run IDENTICAL to each other and to producer 1's `m3r2-ov-ai-on`.
- **The frame log** (`--frame-log`, `tools/frame_log_report.py --oneline`): `flog4k-bat` (release, 3840x2160, the ai scene batched)
  `seconds=4 frames=500 refresh=144 ft p50=5.3 p95=12.1 p99=16.3 max=1213.603 over=94 (18.8%) ticks=500 tick_p50=0.248 tick_max=1202.510
  heap 48828416->48974912 (+2306385 B/min) pools=1 dupe=-1->-1 drift(thirds)=PASS seconds_over_budget=4`; `flog4k-imm` (`/meshImmediate`)
  `seconds=7 frames=500 refresh=144 ft p50=13.2 p95=24.1 p99=31.7 max=1225.135 over=485 (97.0%) ... heap 48824320->48974912 (+1194634
  B/min) ... drift(thirds)=PASS seconds_over_budget=7`; `mission-fl` (debug, the Mission01 6,000-tick gate with `--frame-log`)
  `seconds=10 frames=6000 refresh=144 ft p50=1.6 p95=2.1 p99=2.7 max=1203.634 over=3 (0.1%) ticks=6000 tick_p50=0.500 tick_max=1201.689
  heap 41813504->41813504 (+0 B/min) pools=1 dupe=0->0 drift(thirds)=PASS seconds_over_budget=0` (the `max` in every file is the first
  frame after the level load; the 4K pair's `over` and p99 are under the load average of ~30, the medians are the port's: the mesh
  batching cuts the 4K frame by 2.5x). `mission-fl`'s stream is byte-identical to `mission-a`'s.
- **The quit after a resolution change** (`m3r2i-e-modequit`, mode `modequit`, `--mode-to 1280x720`): `modequit: start request=1024x768
  physical=1024x768 logical=1024x768 to=1280x720;switched ok=1 request=1280x720 physical=1280x720 logical=1280x720; done switched=1
  result=PASS (the quit follows: the harness judges rc 0 and the quit line once); rc=0; "Quit game" lines: 1; quit: PASS`.
- **The console / keys / fescreens self-tests** with producer 1's and producer 4's rows: `consoletest: done selected=154 frames=4
  shown=36 set=29 commands=8 keys=32 kas=6 closed=1 failures=0 result=PASS` (`tablerows=486`), `keystest: done rows=197 known=149
  unknown=48 commands=147 bindings=149 ... result=PASS`, `optionstest: done options=35 startdefaults=35 readback=35 result=PASS`;
  the uiscale self-test x9 all `checks=45 fails=0 result=PASS` (4K `physical=3840x2160 logical=1920x1080 scale=200` twice and with
  `uiScale 100` in the cfg `logical=3840x2160 scale=100`, `/uiScale 150` `logical=2560x1440 scale=150`; 2560x1440 `logical=1707x960
  scale=150`; 1920x1080 and 1024x768 `scale=100`; the two `--fit` runs at 1024x768).

### Fence 2: nis-flicker (producer 2, audit 0.97): the frame-signature scans

| scan (`--frame-sig`) | binary, clock | frames | read-back | outliers (image, bars, objects, camera) | short runs |
|---|---|---:|---:|---|---|
| `sig-n01` (n01 whole) | debug, 144 virtual fps | 16,084 | 16,084 | 0 (0, 0, 0, 0) | 0 |
| `sig-nislet01` | debug, 144 virtual | 15,553 | 15,553 | 0 | 0 |
| `sig-n06` | debug, 144 virtual | 9,505 | 9,505 | 0 | 0 |
| `mt144` (Mission01 NIS-first self-test, 4,900 ticks, 2 segments) | release, 144 virtual | 60,031 | 60,031 | 0 | 0 |
| `rc-n01` | release, the real clock at 144 | 15,277 | 0 (none on the real clock) | 0 | 0 |
| `rc-nislet01` | release, real clock | 14,689 | 0 | 0 | 0 |
| `rc-n06` | release, real clock | 9,049 | 0 | 0 | 0 |
| **total** | | **140,188** | | **0** | **0** |
| `x-mtdef-sig` (the attribution run: the self-test on the default clock) | debug, default | 6,671 | 6,671 | 1 (0, 0, 1, 0) `first=3483@1712/s1` | 1 `first=3489@1718/s1` |

The seven scans are clean, as producer 2's 450,644 frames and the audit's reruns. The default-clock run's two hits are the two
scripted ones the port and its audit describe (frame 3483 = tick 1712, nislet01's last fade frame at fd=1.000; the run 3489-3499 =
ticks 1718-1728, the arrival's white flash with the bars drawn), identical to producer 2's `m3r2-nf-sig-mt` and the audit's
`m3r2-nfa-sig-mt` (`first=3483@1712/s1`, `3489@1718/s1`). The flicker is not reproduced by any scan: the instrument is proven
(the audit's forced frame) and the attended `frame.sig` of the play-test settles the presentation hypothesis (the brief).

**The `rng_all` question of the 144-fps scan, attributed.** `mt144`'s stream against `m3r1i-missiontest-a` is `FIRST DIVERGENT TICK 5
... rng_all`; with `--to-tick 4899 --fields` the 19 simulation fields (every field but `rng0` and `rng_all`) it is `IDENTICAL: 4900
ticks and 0 packets compared (ticks 0-4899), no difference`, and `rng0` alone first differs at 4759 (`CAND is 300 draw(s) ahead of
REF`): `rng0` is `RANDOM_STREAM_0`, the shared, wall-clock-seeded stream HW1's particles draw from (`src/Game/Randy.h:36`), and
`rng_all` folds it in. Two attribution runs on the debug binary (chain X, after the ten chains): `x-mt144-nosig` (`--virtual-fps 144`
without `--frame-sig`) diverges exactly as `mt144` (`rng_all` at 5, `rng0` at 4759, the 19 fields IDENTICAL to 4899; IDENTICAL in all 21
fields to `mt144` itself), `x-mtdef-sig` (`--frame-sig` on the default clock) is `IDENTICAL: 4900 ticks and 0 packets compared (ticks
0-4899), no difference` in all 21 fields to `m3r1i-missiontest-a`. So the virtual frame rate alone moves the particle stream's draw
count (9 frames a tick against 1); the frame signature's read-back moves nothing; no simulation field moved. `mt144` is IDENTICAL in
all 21 fields to producer 2's `m3r2-nf-fsig-mt144` and the audit's `m3r2-nfa-mt144`.

### Fence 3: hyperspace-assembly (producer 3, audit 0.89): the Kuun-Lan's arrival gate and the stern modules

- **Mission01 arrival** (`hsa-shot01`: the NIS-first self-test, `--mission-test-ticks 3000 --shot 1745-1800,1802-1990/4`, 130 frames;
  `hsa-shot01-noasm` the same with `/noHsAssembly`): `hsgate: tick=1749 ship=sMothership id=21 outof part=0 hsShouldDisplayEffect=1
  core up=339.2 right=293.7 fwd=2214.5 boxScale=6.00 gate tree up=339.2 right=293.7 exe up=2035.1 right=1762.1 now up=2035.1
  right=1762.1 c=(0.0,0.0) sweep 2214.5..-2214.5 (exe 2214.5..-2214.5) assembly parts=6 x -868.1..566.3 y -518.4..518.4 z
  -1998.1..1990.0` (the exe's 6.0x gate over the whole ship; `/noHsAssembly` prints the same gate with `assembly parts=0`);
  `hsassembly:` 20 lines, the first `tick=1750 pending parts drawn=6 under host=sMothership id=21 hsState=2 clipt=0.9375`; `hsarrive:`
  14; `hsfinish:` 7 (`sMothership` 1917, `OreCanArmour` 1927, `Command` 1930, `Engineering` 1933, `EnginesLower` / `EnginesUpper` 1935,
  `LowerSpine` 1938). The stream IDENTICAL to `m3r1i-missiontest-a` over ticks 0-2999 and to the `/noHsAssembly` run over every tick.
  **Frames**: 130 / 130 byte-equal to producer 3's final capture `m3r2-hsag-shot01-new`, 130 / 130 byte-equal to `hsa-shot01-noasm`
  (the assembly layer has no visible effect in the shipped data, as the audit found: the user's items are fixed by the `CAT` port of the
  exe's gate and planes alone), 57 / 130 differ from the audit's reverted build `m3r2-hsaa-shot01-rev` (the GoK gate and planes). **The
  stern modules** (`ppm_region.py diff --tol 40 --region 545,325,638,512`):

  | tick | this step vs `/noHsAssembly` | this step vs the reverted build (the audit's counts: 225 / 8,659 / 8,647 / 8,625 / 6,773 / 6,756 / 0 / 0) |
  |---|---|---|
  | 1914 | 0 | 225 (`bbox=625,428,638,459`) |
  | 1918 | 0 | **8,659** (the whole region) |
  | 1922 | 0 | 8,647 |
  | 1926 | 0 | 8,625 |
  | 1930 | 0 | 6,773 (`bbox=569,358,638,512`) |
  | 1934 | 0 | 6,756 |
  | 1938 | 0 | 0 |
  | 1942 | 0 | 0 |

  The modules the reverted build misses from 1918 to 1934 (the hull's exit ended at 1917, GoK clipped every module still in its own exit
  by the host's default plane) are present in this build at every tick.
- **Mission02 arrival** (`hsa-shot02`: `--teamcolour --shot 20-60,62-260/3 -- /catTeamColourTicks 300`, 324 frames, segment s2 108):
  `hsgate: tick=22 ship=sMothership id=21 outof part=0 hsShouldDisplayEffect=1 ... now up=2035.1 right=1762.1 ... assembly parts=6`;
  the team-colour checks `7 checks, 0 FAIL`; the stream IDENTICAL to `m3r1i-tc-user`. Frames: 324 / 324 byte-equal to the audit's fixed
  build's capture `m3r2-hsaa-shot02-new` and to its `/noHsAssembly` run `m3r2-hsaa-shot02-noasm`; 69 / 324 differ from producer 3's
  first capture `m3r2-hsa-shot02-new` (made before the producer's later fixes and the audit's); against the reverted build at tick 29
  `differ=58806 bbox=614,107,1024,336` (tol 40, the audit's figure; tol 0 113,169: the sliver of a gate against the rectangle spanning
  the ship), at 221 7,576 (tol 0), at 227 0: the audit's "from 29 through 221, 0 from 227".
- **Mission03 departure** (`hsa-shot03`: Mission03's self-test with `--shot 17570-17720/2`, 142 frames): `hsgate: tick=17579
  ship=sMothership id=0 into part=0 hsShouldDisplayEffect=1 ... boxScale=6.00 ... assembly parts=5` (the hull), `hsgate: tick=17591
  ship=sMothershipEnginesLower id=1 into part=1 hsShouldDisplayEffect=0 core up=469.2 right=721.3 fwd=665.5 boxScale=1.00 ...
  assembly parts=0` (a module: its own gate, no effect); `hsfinish:` the hull 190 / the modules 205-211 (the arrival at the start) and
  17767.. (the departure); the stream IDENTICAL to `m3r1i-m3-a`. Frames against the reverted build (tol 0): 17580 1,295 px
  (`bbox=499,341,689,373`, the gate), 17592 17,208, 17594 19,018, 17596 18,142 (`bbox=366..689 x 241..514`: the EnginesLower module the
  reverted build cuts at 17592 / 17594 while the hull is still popping up, and the 6.0x gate in every frame); 130 / 142 frames differ from
  the reverted build, 124 / 142 from producer 3's first capture `m3r2-hsa-shot03-new`.

### Fence 4: mesh-batching (producer 4, audit 1.00): FULL DETAIL at 4K, batched against `/meshImmediate`

The ai scene at 3840x2160 (`--lodstats`; 164 objects, `verts=25169 polys=46255 fullDetail=1` in every frame of every run), the release
binary, the idle GPU, **the CPU load average ~24-37 (the user's ripgrep)**:

| run | path | frames | median ms | p99 ms | max ms |
|---|---|---:|---:|---:|---:|
| `perf4k-bat1` | batched | 301 | **5.038** | 12.818 | 16.368 |
| `perf4k-bat2` | batched | 301 | **4.581** | 16.054 | 50.538 |
| `perf4k-imm1` | `/meshImmediate` | 301 | 13.666 | 41.047 | 48.011 |
| `perf4k-imm2` | `/meshImmediate` | 301 | 13.689 | 34.043 | 49.664 |
| `flog4k-bat` (with `--frame-log`) | batched | 301 | 4.889 | 11.787 | 15.589 |
| `flog4k-imm` | `/meshImmediate` | 301 | 12.770 | 30.011 | 34.242 |
| `shot4k-bat` (with the three 4K captures) | batched | 261 | 4.801 | 36.243 | 50.822 |
| `shot4k-imm` | `/meshImmediate` | 261 | 12.433 | 46.651 | 85.989 |

The medians agree with producer 4's and the audit's idle numbers (batched 3.7-4.6 / the audit 3.1-3.4 against immediate 9.2-12.5 /
7.4-8.9: the batching cuts the frame by 2.5-3x); **the p99 does not**: 12.8-16.1 ms batched here against the producer's 5.2-6.2 on a
quiet machine (immediate 34-41 against 13.4-15.3). The frame is CPU-bound vertex work, so a load average of ~30 measures the load, not
the port; the user's ripgrep ran through the whole step, a quiet-CPU rerun was not possible, and the number is **stated as the load's**
(the quiet-CPU rule, decision 132; the soak's attended hour is the measurement that counts). The look: the three 4K captures batched
against immediate `differ=0` at ticks 200 / 300 / 400 (whole frames, tol 0) and `differ=0` against producer 4's final captures
`m3r2-mbf-shot-ai`; the dumps `motion-on` = `motion-on-imm` and `nis-on` = `nis-on-imm` byte-equal; the lodtest `drawnAbove0=0
bandAbove0=164 maxBand=4 verts=25169` with FULL DETAIL and `drawnAbove0=164 ... verts=5330 polys=7030` with `/lodBands` (both PASS); the
streams of every batched / immediate pair IDENTICAL.

## The four ports

| # | item | what landed | DONE WHEN | audit | commit |
|---|---|---|---|---|---|
| 1 | the instrumentation + the quit after a resolution change ([report](m3-r2-instrumentation.md), [audit](audits/m3-r2-instrumentation.md)) | **The `hwi-settings:` line** (`src/Game/Instrument.c`, new): printed at start-up and at every change, one row per setting (`objInterpolate`, `nisInterpolate`, `letterboxBehind`, `noLOD`, `uiScale`, `overlay`, and producer 4's `meshImmediate`) with its source (default / cfg / switch / console / options / key) and a `[why]` tag naming the trigger; producers add a setting by appending a row to `hwiSettings[]`. **The overlay**: off by default, toggled by the console row `overlay` (`CAT_CON_HWI_ROW_ID + 5`), the run-only switch `/overlay` and the keyboard's Menu key (`SDL_SCANCODE_APPLICATION`: no VK code, so `keys.cfg` cannot bind it and the exe never uses it); drawn top-left (logical 4,4) at the UI scale with `default.hff`: `FRAME <ms> MED <p50> P99 <p99>` (the period between two ends of the render task, after the flip; no `glFinish`), `TICK`, `ALPHA`, the settings row, `VERTS`. **`/frameLog <file>`** (HWI-TEST, run-only; harness `--frame-log`): per second the frame-period percentiles, the universe ticks, the heap's used / growth counters, a histogram; `tools/frame_log_report.py` reads it (p50 / p95 / p99 / max, the frames over one refresh, the drift test over the first and last 10 minutes, the heap growth a minute, the `dupe trcolorinfo list` blocks, the seconds over budget). **The quit fix** (`HWI-FIX`, `main.c mainDestroyWindow`): GoK's `mainActuallyQuit = FALSE` clear after a resolution change swallowed the next quit's first `SDL_QUIT`; the exe's `WindowProc` consumes its WM_DESTROY at once, so the port sets `mainActuallyQuit = TRUE`; the new self-test `/catModeQuitTest WxH` (harness mode `modequit`) switches the mode and judges rc 0 with one `Quit game, baby!`. A soak finding on the way: the heap grows ~4 MB a minute in a 600 s `sp` run (`dupe trcolorinfo list` blocks 912 by Mission06; owner M3 soak, the counter in place). | met: the settings line with every source; the overlay confined to its box at 1024x768 (`differ=16064 bbox=2,2,253,66`) and at 4K (`64256`, `4,4,506,132`); the quit shown failing without the fix and passing with it; the four dumps EQUAL, Mission01 IDENTICAL also with `/overlay` and `/frameLog` | PASS-WITH-FIXES, precision 32 / 36 = 0.89 (the frame log's build field, `play.sh` lacking `/frameLog`, the Menu key's exe claim, an overbroad key claim fixed) | `bbdc613` |
| 2 | the cutscene's one-frame flicker ([report](m3-r2-nis-flicker.md), [audit](audits/m3-r2-nis-flicker.md)) | **Not reproduced; no engine fix.** 450,644 scanned frames over 22 runs (n01, nislet01 and n06 whole; Mission01's NIS-first start with both cutscenes and the KAS close-up; virtual rates 60 / 120 / 144 / 165 / 240; the real clock) find no frame that stands out from both neighbours; in the exe and in the port every NIS / KAS-letterbox frame after an NIS's first tick draws the bars late, so a frame with neither objects nor bars cannot leave the engine's frame loop: the one frame holding exactly that picture is a frame presented before it finished drawing (BTG first, objects next, bars last), which no headless run can see (XWayland / NVIDIA / cosmic-comp; the engine does set `SDL_GL_SetSwapInterval(1)`, `render.c:1062`: the audit's fix of the report's claim). The user's exact frame forced in a scratch tree is caught by the new instrument with and without read-back; the Escape skip forced and ruled out. **Landed**: `/catFrameSig <file>` (HWI-TEST, any mode: one cheap line per frame with the moved / visible object counts, the bars' state, the NIS time, the alpha and the frame time; the read-back columns on the virtual clock only) and `/catVirtualFps <fps>`; harness `--frame-sig`, `--virtual-fps N`; `tools/dump_outliers.py` (the image / bars / objects / camera / short-runs tests, `--oneline`). The KAS Out->In bar snap is the exe's and stays. | **not met by nature** (the flicker is not reproduced; the hypothesis is presentation): the attended play-test's `frame.sig` settles it (the brief); the done-when's scans are clean, the instrument proven on the forced frame | PASS-WITH-FIXES, precision 33 / 34 = 0.97 (the swap-interval claim fixed; two claims qualified) | `4b799cb` |
| 3 | the hyperspace arrival over the whole assembly ([report](m3-r2-hyperspace-assembly.md), [audit](audits/m3-r2-hyperspace-assembly.md)) | **Items 2 and 3 were GoK deviations; the exe's gate ported as the base (`CAT`)**: `hsStart` scales the gate rectangle and the effect by `hyperspaceBoxScale` (6.0 for the Kuun-Lan: `now up=2035.1 right=1762.1` where the tree drew the core spine's 339.2 x 293.7; the sweep 2214.5 .. -2214.5 unchanged), `hsGetEquation`'s POPUP_INTO plane, `hsContinue`'s host-inactive `glDisable` (the modules unclipped once the host's exit is over). **`HWI-FEATURE(hyperspace-assembly)`** (default ON, run-only `/noHsAssembly`): a part whose own exit has not started is drawn pending under the hull's clip plane (6 parts from tick 1750 while the hull is in POPUP_OUTOF, then 5, 4, 3, 1 as each part's exit takes over at 1759 .. 1770), the departure one collapse over the assembly (the modules clip by the hull's plane and finish together); the pending parts' render-list walk never runs the LOD pass (the audit's fix: sim-read fields were written). HWI-TEST lines `hsgate:` (core box, the tree's / exe's / now gate, the sweep, the assembly union), `hsarrive:`, `hsassembly:`, `hsfinish:`. 7 `CAT` lines, 55 `HWI-FEATURE(hyperspace-assembly)` lines, 5 `IDENTICAL -> MODIFIED` rows + the `hsUpdate` review row. | met: proven non-vacuous by a reverted build (the core-sized gate, the stern modules missing at 1918-1934: 8,659 / 8,647 / 8,625 / 6,773 / 6,756 px in the region 545,325,638,512; `EnginesLower` cut at the departure); every stream, replay (2823 / 2871) and dump IDENTICAL; 17 / 17 missions IDENTICAL | PASS-WITH-FIXES, precision 25 / 28 = 0.89 (the hidden parts in the LOD pass fixed, 5 untagged lines tagged, a review row added, the report's pixel counts made reproducible) | `83c4b26` |
| 4 | FULL DETAIL under the 144 Hz budget: the mesh batching ([report](m3-r2-mesh-batching.md), [audit](audits/m3-r2-mesh-batching.md)) | **`HWI-FEATURE(mesh-batching)`** (default ON): the CPU-lit mesh renderers (`meshObjectRenderLit` 0x004b0970, the ships; `meshSpecObjectRender` 0x004b1d40, the engine glows / particle meshes) record their corners with the same lighting calls in the same order and draw each material run with one `glDrawArrays` from one scratch array; the exact reuses in `Shader.c` (`shColourLit0`, `shColourReuseLit`, `shGlowCache` / `shGlowStamp`, `shColourCapture`; `catShFastToInt` = `_mm_cvtsd_si64` on x86-64 Linux, `lrint` elsewhere). Run-only `/meshImmediate` and the console row `meshImmediate` (HWI row id 0x1006, `CAT_CON_HWI_ROWS` 7) restore the immediate path; the settings line's `meshImmediate=` row; no options-file row, no control, no key. Unbatched: `meshObjectRender` (lighting off / wireframe / single texture), the warp, morphs, the edge lightning. Tools: `tools/hwiprof/hwiprof.c` (an LD_PRELOAD wall-clock sampler; the frame by function and by GL call class) and `tools/hwiprof/hwiprof_report.py`; the profiling build `build/prof`. 160 `HWI-FEATURE(mesh-batching)` lines. | met on the producer's and the audit's idle-GPU runs: the 4K ai scene (164 objects, 25,169 vertices / 46,255 polygons a frame) batched p50 3.72-4.55 / p99 5.21-6.19 ms (the audit 3.08-3.37 / 4.49-5.12) against `/meshImmediate` 9.24-12.47 / 13.40-15.30 (the audit 7.40-8.86 / 10.84-12.62): the 6.94 ms refresh interval met; every dump and capture byte-equal between the paths and to the baselines; **this step's rerun under a load average of ~30 (the user's ripgrep): the medians hold (4.58 / 5.04 against 13.67 / 13.69 ms), the p99 does not (12.8 / 16.1 against 34.0 / 41.0 ms): fence 4 below** | PASS-WITH-FIXES, precision 34 / 34 = 1.00 (one tool-doc fix) | `47922ff` |

**Round size** (`git diff --stat 06dae5c..47922ff`, the round base to the last audit's checkpoint; 12 commits): 49 tracked files
changed, +5,884 / −70 lines; `src/` alone: 27 files, +2,459 / −37, with 20 added lines carrying `CAT` (10 `CAT:`, 0 `CAT-FIX`); new
`src/` files: `src/Game/Instrument.c` / `.h` (the builds link `[274/274]`, M3-R1's `[273/273]`); new tools `tools/dump_outliers.py`,
`tools/frame_log_report.py`, `tools/hwiprof/hwiprof.c` + `hwiprof_report.py`. The tree now carries 4,475 `CAT:` lines (M3-R1: 4,465),
**37 lines carrying `CAT-TODO`** (37; **28 owner tags: stage 5 23** (plain 8, multiplayer options 8, LAN 6, multiplayer 1), **M3 oracle
physics 3**, **M3 soak 1** (the task clock, utility.c), **M3-R3 1** (the BTG star size, re-targeted by this step: below); 9 other lines
mention closed tags in prose), **4 `CAT-PROVISIONAL`** (4), 13 `CAT-DORMANT` (13), **195 `CAT-FIX`** (195), 49 `HWI-ORACLE` (49), **286
`HWI-TEST`** (222), **62 `HWI-FIX`** (M3-R1's exit 53, then 60 at the round base `06dae5c` with the six `HWI-FIX(win64)` of the Windows
build commits; this round +2: `mainDestroyWindow`'s `mainActuallyQuit = TRUE` and its explanatory line in `main.c`), 6 `HWI-EXPERIMENT` (6);
the feature tags (`/bin/grep`): `HWI-FEATURE(motion-interp)` **486** (484: the two settings rows in `Instrument.c`, one Console.c line
rewritten), `HWI-FEATURE(letterbox-setting)` **37** (36), `HWI-FEATURE(full-detail)` **179** (178: the `Instrument.c` row; two `render.c`
lines rewritten by producer 3), `HWI-FEATURE(ui-scale)` **300** (299), **new `HWI-FEATURE(instrumentation)` 102,
`HWI-FEATURE(hyperspace-assembly)` 55, `HWI-FEATURE(mesh-batching)` 160** (no `HWI-FEATURE(nis-flicker)`: producer 2 added test
instruments only); 1,314 `HWI-FEATURE(` lines in all. Tools: `tools/m2_harness.sh` (31 modes: `modequit` new; the flags `--mode-to WxH`,
`--frame-log`, `--frame-sig`, `--virtual-fps N`; the `settings:` summary line in every run), `tools/play.sh`'s header documents
`/overlay`, `/frameLog`, `/catFrameSig`, `/noHsAssembly`, `/meshImmediate`.

## Delta map after the round

The ports recorded their comparisons as reviews in `ghidra/delta_map_review.json` and merged them with `tools/delta_map.py merge`
(decision 55). Notes tagged `[review: M3-R2 <port>]`: hyperspace-assembly 7 (+ 1 `[review: M3-R2 hyperspace-assembly audit]`),
mesh-batching 4: 12 in all; producers 1 and 2 touched no mapped body (test instruments and a new file). This step added no review of its
own and ran no merge; `python3 -I tools/m2_stage_counts.py --md` on the tree's map gives the table.

| stage | HW1 files (main ones) | IDENTICAL fn / B | MODIFIED fn / B | REWRITTEN fn / B | cat-new fn / B (est.) |
|---|---|---:|---:|---:|---:|
| 1 file formats + data load | Mesh, MeshAnim, texreg, LOD, StatScript, SaveGame, BigFile, File, ObjTypes, StringSupport, Tweak, LZSS/BitIO/CRC32 | 210 / 41,085 | 105 / 54,159 | 0 / 0 | 76 / 27,383 |
| 2 universe and ships | Universe, UnivUpdate, CommandLayer, Collision, Physics, Gun, Dock, Formation, ETG, Particle, Select/ShipSelect, Stats, ResearchAPI, Ships/* | 719 / 130,879 | 653 / 270,426 | 30 / 4,444 | 808 / 180,868 |
| 3 UI and presentation | FEFlow/FEReg, UIControls, ConsMgr, ResearchGUI, Options, NIS, sound/speech, render/GL, main/mainrgn, mouse | 845 / 182,031 | 339 / 202,253 | 14 / 8,558 | 450 / 139,196 |
| 4 AI | AI*.c | 291 / 43,874 | 186 / 79,829 | 2 / 169 | 23 / 14,404 |
| 5 LAN networking | CommandNetwork, MultiplayerGame, MultiplayerLANGame, TitanNet, NetCheck, ChannelFSM, chat | 404 / 69,517 | 116 / 76,194 | 12 / 1,179 | 67 / 17,483 |
| 6 missions | KAS, KASFunc, Timer, Objectives, LevelLoad, SinglePlayer, Tutor + kas2c mission code | 288 / 19,469 | 181 / 53,276 | 25 / 1,505 | 2,365 / 656,918 (2,085 / 612,687 B are kas2c mission functions) |
| **total** | | **2,757 / 486,855** | **1,580 / 736,137** | **83 / 15,855** | **3,789 / 1,036,252** |

IDENTICAL 2,757 / MODIFIED 1,580 / REWRITTEN 83 (M3-R1: 2,762 / 1,575 / 83: producer 3's five edited stage-2 `HS.c` bodies IDENTICAL ->
MODIFIED (`hsOrientEffect`, `hsStart`, `hsGetEquation`, `hsContinue`, `hsEnd`; 2,724 B) plus its `hsUpdate`
review row), 3,579 reviewed rows (M3-R1: 3,573); **210 MODIFIED / REWRITTEN rows still without a `reviewed` flag (118 outside
`TitanInterface.cpp`**: ETG.c 41, Particle.c 38, ObjTypes.c 16, MultiplayerLANGame.c 7, MultiplayerGame.c 5, GameChat.c 3, StatScript.c
3, one each in Captaincy.c, CommandNetwork.c, File.c and TitanNet.c; unchanged from M3-R1). Bodies compared with the exe this round and
found as the tree (no row or a note): the hyperspace audit's 12 (the five edited bodies, `hsRectangle` 0x0048f750 IDENTICAL, `cat_spGetHyperspaceModuleTypes`,
`cat_univCreateMothershipModules` and four more: its section "Bodies compared"), the mesh-batching port's 4 (`meshObjectRenderLit`, `meshSpecObjectRender`, `meshSpecColour`,
`shColourSet0`: per-corner immediate submission in the exe, the `CAT` bodies stand under `/meshImmediate`) and the hierarchy walkers
relied on unchanged (`meshObjectRenderR`, `meshObjectRenderHierarchy`, `meshRender`, `meshRenderShipHierarchy`: MODIFIED rows already).

## What is left

Nothing blocks the next play-test; each item has an owner (the play-test, M3-R3, the M3 soak, M3 oracle physics, the Windows 11 test,
the user, stage 5, or after M3):

1. **From this round's ports** (the four reports' "What is left"):
   - **instrumentation**: the attended check (the overlay's look and legibility at 200 %, the Menu key, the quit at 4K on the user's
     desktop: the brief); **the overlay's F-key toggle** (the user's binding decision of 2026-10-24: one of F1-F12, candidate F12, the
     only F key absent from `src/SDL/mainrgn.c`'s hardwired table and used only inside the construction manager's build hotkeys; to be
     verified against `cataclysm.exe`'s key tables and the keys self-test 197 / 149; **owner M3-R3 item 0**, not applied here); the
     heap's growth over the `sp` loop (~4 MB a minute in a 600 s run, the second growth heap, the `dupe trcolorinfo list` blocks 912
     by Mission06: the frame log's counters are in place; owner M3 soak, O4); the `options` source labels any change no hook announced; **the toggle self-test under `/letterboxBehind`** fails one M3-R2 check
     (`m3r2i-e-toggle-lb`, this step's exit test: "settings line: a console row's change prints it with source console",
     `src/Game/MotionTest.c:2128-2131`): under the switch the line prints `letterboxBehind=1 (switch) ... [console letterboxBehind]` by
     `Instrument.c`'s rule (a standing switch labels the row before any announced source; `/noInterp` + a console row shows
     `objInterpolate=1 (switch)` the same way, documented as faithful at docs/m3-r2-instrumentation.md:415), so the check's expectation
     is wrong under the switch; the one-line fix (accept `(switch)` while `rndLetterboxSwitchStands()`, or end the switch's standing on
     the console row as `/overlay`'s does) is **M3-R3's**; nobody had run the toggle self-test with `/letterboxBehind` in this round.
   - **nis-flicker**: settle the presentation hypothesis from the play-test's `frame.sig` (owner: the brief, then M3-R3); if it is
     confirmed downstream, `--driver wayland` as `tools/play.sh`'s default, or a swap-interval / finish setting measured against producer
     4's budget (owner M3-R3); the KAS Out -> In bar snap is the exe's and stays.
   - **hyperspace-assembly**: the play-test check (the gate over the whole Kuun-Lan, the stern modules never late); the exe's `hsStart`
     tests `glCapFastFeature(GL_BLEND)` where the tree tests `etgEffectsEnabled` (a simulation-side creation call; owner M3 oracle physics
     if ever a stream needs it); the multiplayer arrival of a modular mothership is not exercised (owner stage 5, with the LAN work).
   - **mesh-batching**: the faster frame for bigger fleets (static per-object buffers for positions / UVs, the material switches' state
     cache, the lighting; owner **M3 soak**, the 4K@144 row: this scene is under budget on a quiet machine); the unbatched renderers
     (the planet / particle meshes, the warp, morphs, the edge lightning: ~3 % here; M3 soak if the attended hour shows them); **the p99
     on a loaded machine** (this step's fence 4: the medians hold, the p99 does not under a load average of ~30; the frame is CPU-bound
     vertex work, so the soak's attended hour on a quiet machine is the measurement; owner M3 soak); the attended 4K hour at 144 Hz
     (the user's measurement; the overlay and `/frameLog` are its instruments).
2. **M3-R1's items owned by M3-R2**: **done** this round: the quit after a resolution change (producer 1, `HWI-FIX`) and FULL DETAIL's
   mesh submission (producer 4). **Not taken, and why**: the **BTG background-star size** (`btgConvertStar`: the exe's star a fixed
   share of the screen, GoK's a fixed pixel size) is a look decision the user has not answered (M3-R1's question 5): its
   `CAT-TODO(M3-R2)` at `src/Game/BTG.c:1151` is re-targeted by this step to **`CAT-TODO(M3-R3)`** (a comment-only edit, rebuilt with 0
   warnings); the **letter height** (GoK's 2.39:1, 277 rows at 4K, against the exe's 42) and the **exe's additive state after full bars**
   are the user's calls (M3-R1's questions 2 and 3, still unanswered: M3-R3 one-liners if the user picks the exe's); the **effects cap /
   effect level 0** under FULL DETAIL (Relic's per-effect limits; needs the ETG callback audit and a stream proof) and **trails at the
   LOD-0 style** (would move `rng_all`: a stated re-baseline) stay the user's calls (question 4); the **sensors map's lines at 200 %**
   and the **tutorial's `TUT_PointerTypeShips` pointer** wait for the user to see them (M3-R3 if reported); the **window fit on the
   user's real desktop** is confirmed by the third play-test (it ran at 3840x2160 fullscreen-desktop, UI scale 200 %,
   `cycle/answers/m3-r3-user.md` "The session", no window complaint); the **task clock** (`CAT-TODO(M3 soak)`), **frame pacing at 4K**
   and the **`DEADLYSIGNAL` in a timeout-kill quit** stay the M3 soak's (this step's two 1,200 s `sp` runs: the exit test); the
   **colour-info leak O4** is the M3 soak's with producer 1's counter.
3. **The oracle's rung** (M3 oracle physics; unchanged this round): 10-17 tick 2823 `ships_vel` (ship 48) and 10-18 tick 2871
   `ships_vel` (ship 49), both a bProcessor following the bWorker 44 by `protectShip`'s follow-behind point (decision 100: the Worker's
   coordsys, invisible to the stream without decision 48's recording); the three `CAT-TODO(M3 oracle physics)` (`RangeToTarget` /
   `RangeToTargetGivenDist`'s unrounded returns, `cat_gunCanFire`'s second mode, `cloudUpdateSystem`), the other inlined `frandom` sites
   (LevelLoad.c's first), Mission17's self-targeting KAS teams; now also the `hsStart` `GL_BLEND` test above.
4. **The mode-B rung** (the user's session, decision 60): the GOG playback of `hwirec01` / `hwirec02` with the DLL; still no
   `original/oracle/hwirec*-play-*` directory (this step's `ls`).
5. **Decision 48** (the user's): no recording with the fixed DLL (the newest `original/oracle/oracle01-rec-*` is still
   `20261019-143530`); `ships_rot` stays excluded in every session.
6. **The user's third play-test** (`cycle/answers/m3-r3-user.md`, seven items + item 0; **owner M3-R3**, nothing of it in this round):
   Mission02's dust belts on the tactical map (fog of war), Mission02's Bushan-Re hint dialog's trigger, a nearest-texture-filtering
   launch option (a feature: the exe's linear filtering stays the base), Mission03's white mines (a failed-texture signature), the pause
   jitter (the other half of Smooth motion: the render lerp keeps blending while the universe is paused; the likely fix freezes the
   render pose and the camera while paused, fenced by a paused dump with every crc identical), the mine AI's 3-second seek-and-stop, the
   missile corvette's one trail for four nozzles. Items 1, 2, 4 and 6 are "is the recreation 1-to-1 with the exe here?" questions first.
7. **The Windows 11 test** (docs/m3-windows-build.md): the win64 zip `ce95fec5…` is eb5f7e3's source (nothing of M3-R2) and untested;
   the user's test list is the doc's "What the user must test on Windows 11 before sharing"; a rebuild from this round's tree needs
   `tools/release_win64.sh --engine <commit>` (the six `HWI-FIX(win64)` are in the tree since the round base).
8. **Stage 6's play-path items for M3** (unchanged from R10 / M3-R1): the attended animatic check (the brief), the mission self-test's
   combat (M3 test), Mission17's self-targeting KAS Taiidan teams (M3 missions / oracle physics), the chained campaign's heap (M3 soak:
   now measured by the frame log), `trTextureRegister`'s name case (M3 render, only if a texture is ever registered twice), decision 94's
   nebula `CAT-FIX` (the user's).
9. **Stage 5** (after M3, decision 9): the 23 `CAT-TODO(stage 5 ...)` sites, unchanged (plain 8, LAN 6, multiplayer options 8,
   multiplayer 1); now also the modular mothership's multiplayer arrival.
10. **Stage 4 / 3 leftovers that stay**: `gunResetGimbleGuns` uncalled (58), the five creators' default tactics (59), the folded
    handlers' save indices (63), HW1's cursor text (65), `rndLoadTarga`'s `GL_CLAMP_TO_EDGE` and the Transformer (28); **closed this
    round**: the quit after a resolution change, FULL DETAIL's 4K frame on an idle machine, the hyperspace gate's size and the late stern
    modules (GoK deviations, now the exe's); **not closed**: the one-frame flicker (not reproduced; the instrument is in the user's hands); **found by this exit test**: the toggle
    self-test's one failing check under `/letterboxBehind` (item 1; M3-R3).
11. **Motion interpolation** (decision 46): the `/noInterp` dumps' content unchanged in every non-crc column since R9 and in every column
    since M3-R1 (the dumps table); the clamp blend measured in M3-R1; the droid blend still unmeasured; **new**: the pause jitter (item 6
    above, M3-R3).
12. **The delta map's review flags**: 210 MODIFIED / REWRITTEN rows without a `reviewed` flag (118 outside `TitanInterface.cpp`; unchanged).
13. **Still open, the user's**: the two saves of the M0 play-test (8), the play-test sign-off (the brief below), the fourth recording (5),
    the mode-B playback (4), decision 94's `CAT-FIX`, M3-R1's questions 2-5 and 7-10, and this round's questions (below).

## Plan for M3-R3 and the rest of M3 (replaces M3-R1's "Plan for M3-R2" table)

The order the user set still holds ("we need to fix bugs before we get further from the mark"): this round fixed the second play-test's
items (the flicker instrumented, not reproduced); the user's third play-test has already reported seven more (M3-R3's queue, row 1), and
the user decided the overlay's F key; the platform rows (the Windows 11 test, the soak) and the oracle's rung are unchanged in scope but
the soak is now instrumented (`/frameLog`, the overlay, hwiprof, the heap counters). The play-test can start now with
`tools/play.sh --rebuild` (the brief below); it is short (four checks and a `frame.sig`), so it can run while M3-R3's exe-reading items
(rows 1's items 1, 2, 4, 6) start.

| # | step | scope | counts | done when |
|---|---|---|---|---|
| 0 | **the user's next play-test** (now; the brief below) | the four items re-checked (the settings line and the overlay with the Menu key, the quit at the first try after a resolution change, the Kuun-Lan's arrival gate and stern modules, FULL DETAIL's FRAME p99 at 4K and the ships' look batched vs `/meshImmediate`), the cutscenes with `/catFrameSig` (the flicker's verdict), then M3-R1's open items (the bars' height, the colours after Mission03, the far zoom, the UI at 4K, the sensors map's lines) and R10's (the animatics, the campaign, the tutorial, a skirmish, a recording) | 1-3 hours | a report per item, the `frame.sig` and the two frame logs handed over; M3-R3 sized from it |
| 1 | **M3-R3: the third fix round** (one producer + auditor per item, as this round; the user's items in the user's order) | **item 0** the overlay's F-key toggle (F12 candidate: verify against the exe's key tables and the keys self-test; the Menu key may stay as a second toggle); **the seven items of `m3-r3-user.md`** (the Mission02 dust belts on the tactical map and their fog-of-war exemption; the Mission02 Bushan-Re hint dialog's trigger conditions; a nearest-texture-filtering launch option (`HWI-FEATURE`, default the exe's linear); Mission03's white mines (the mine types' texture wiring and the load log); the pause jitter (freeze the render pose and the camera while paused; fence: a paused dump with every crc identical); the mine AI's seek / arm / re-acquire timers; the missile corvette's four trails if the exe draws four); **plus** the flicker's verdict from the `frame.sig` (a presentation setting if confirmed), the BTG star size / the letter height / the additive state if the user picks the exe's; **the toggle self-test's `/letterboxBehind` check** (one line, item 1) | 8 items + 1 test line: four are exe reads first (1, 2, 4, 6), two features (3, item 0), two fixes (5, 7); 4-8 producers | every item either shown 1-to-1 with the exe (with the exe's address and the mission script's line) or fixed with a fence; every stream IDENTICAL (the pause fix never touches a simulation field); the F key bound and in the keys self-test; the play-test's items re-played |
| 2 | **the Windows 11 test** (the user's day + a producer for the findings; M3-R1's row 1, now with an untested zip) | the win64 zip `ce95fec5…` (eb5f7e3's source) tested per docs/m3-windows-build.md "What the user must test" (launch, the front end, Mission01, a save, a recording, the 4K window), then a rebuild from this round's tree (`tools/release_win64.sh --engine <commit>`) with the overlay / frame log / hyperspace / batching in it; the harness modes the gate needs under Git Bash / MSYS2; the libm dependence measured first | 1 day + 1 producer | release + debug on Windows 11 with 0 warnings; the self-tests PASS there once; the Mission01 stream, both replays and a mission stream IDENTICAL to Linux (or every difference localised to a libm call); a save and a recording cross-load |
| 3 | **the one-hour 4K@144 soak, no gamescope** (producer + the user's hour; M3-R1's row 2, now instrumented) | the headless `sp` hour with `/frameLog` (the drift test, the heap growth a minute: ~4 MB/min in a 600 s `sp` run, the `dupe trcolorinfo list` blocks 912 by Mission06, O4's counts as the baseline), the task clock (`CAT-TODO(M3 soak)`), the `DEADLYSIGNAL` in a timeout-kill quit, FULL DETAIL's p99 re-measured on a quiet machine and the static per-object buffers if the attended hour needs them, then the attended hour at 3840x2160@144 with the overlay and `/frameLog` | 1 producer + the user's hour | no frame-rate drift over the attended hour (`frame_log_report.py`'s drift test PASS); no growth heap in the headless hour or every growth attributed and fixed; the task clock at the exe's rate or its difference stated |
| 4 | **the oracle's rung** (M3 oracle physics; M3-R1's row 3, unchanged) | 10-17 2823 / 10-18 2871 (the Worker's dock / harvest heading paths against the exe), the three `CAT-TODO(M3 oracle physics)`, the other `frandom` sites, Mission17's self-targeting KAS teams, `hsStart`'s `GL_BLEND` test; mode B and the fourth recording if the user made them | 1 producer round | the replays IDENTICAL beyond 2823 / 2871 or the next divergences localised and owned |
| then | **stage 5** (decision 9) | the 23 `CAT-TODO(stage 5 ...)` sites: the LAN game's senders and keep-alive, the options word, the `/packetRecord` header, the Titan / WON stubs; the modular mothership's multiplayer arrival | 23 sites + 1 | LAN interop with stock 1.01 nice-to-have (decision 9); the wire format not broken |

**Then**: the stage-5 remainder per decision 9 after M3, or an oracle-physics round only if the user wants the rung before the next
play-test. **Rules carried into M3-R3** (M3-R1's, decision 121, plus this round's): every port on the plain harness with a clean build;
every stream judged with `hwo_compare.py` in all 21 fields (never by sha256 across builds); the dumps compared whole and crc-stripped (a
crc-only move is a stated render change; a non-crc move is a render-timing change and must be stated with its frame count); GPU
measurements on an idle GPU only, with the CPU load stated, **and a frame-time p99 is quoted only from a quiet CPU** (this step's p99
under a load average of ~30 is 2-3x the producer's idle numbers while the medians agree: a loaded machine measures the load, not the
port); a "not reproduced" item's done-when is its instrument proven on a forced frame plus the attended run's file; each step's labels
get their own prefix (the harness replaces an existing label's directory); the user's answers in `cycle/answers/<round>-user.md`, read
again before the report; the "USER:" questions of a report quoted verbatim in the summary.

## What the user must decide

Every default is in the tree; say if you want otherwise. The four reports' "THE USER'S CHECK" sections are quoted verbatim (the
reports carry no "USER:" lines; these sections are their questions), producer 2's hints first.

1. **Producer 2's open hints (nis-flicker)**, from `cycle/answers/m3-r2-user.md` "Open hints the user may add later", still unanswered:
   *"Flicker: on a hard cut between shots, or mid-pan? (not yet answered)"* and *"Which cutscene exactly: Mission01's opening NIS or the
   arrival NIS? (not yet answered; 'the close up of the Kuun-Lan')"*. Its report's check, verbatim:
   > Run `tools/play.sh -- /catFrameSig ~/hwi-sig-$(date +%H%M).txt`. That is the release build at 4K with the settings as
   > they are; Smooth motion ON, as in the 08:15 / 08:40 sessions.
   > - **The moment:** start a new campaign and watch the opening in-engine cutscene (after Animatic01): the Kuun-Lan
   >   close-up (n01 23-33 s, the narrow-angle shot). Then watch Mission01's arrival cutscene and the following letterboxed
   >   close-up of the Kuun-Lan.
   > - **Where to look:** the whole frame. Note whether a flash shows the backdrop with no ships and no black bars, and when
   >   (which cutscene, roughly how many seconds in, on a cut between shots or mid-pan).
   > - **What to report:** whether it happened, the moment, and the file. Then run `python3 -I tools/dump_outliers.py
   >   <file>` (or hand the file over).
   >   - `short runs=0` with a flicker seen means the engine drew everything and the cause is the presentation.
   >   - A run gives the frame and what was missing.
   > - **Optional A/B:** `tools/play.sh --driver wayland -- /catFrameSig ...` (SDL's native Wayland instead of XWayland).

   Default if unanswered: the hypothesis stands (presentation of an unfinished frame); no engine change; M3-R3 acts on the `frame.sig`.
2. **Producer 1's check (instrumentation)**, verbatim:
   > 1. **Start.** Run `tools/play.sh --rebuild` (or `tools/play.sh --rebuild -- /frameLog
   >    ~/hwi-frame-$(date +%H%M).log` for the soak's numbers). Before anything else, read the top of the newest
   >    `original/play-home/logs/play-*.log`. The `hwi-settings:` line should say `objInterpolate=1 (cfg)
   >    nisInterpolate=1 (cfg) ... noLOD=1 (cfg) uiScale=0->200% (cfg, auto) ... fit=fullscreen-desktop (display size ->
   >    fullscreen desktop) 3840x2160`. Report the line if it says anything else.
   > 2. **The overlay.** In the front end, and again in Mission01 after the opening cutscene, press the keyboard's **Menu
   >    key** (the "application" key, usually right of the right Windows / Alt key). A dark panel should appear in the
   >    top-left corner. Press it again and the panel should go. Without a Menu key, start with `-- /overlay`. Report:
   >    - whether the key toggles it;
   >    - whether the five lines are readable at 200 %;
   >    - whether anything in the HUD is hidden under the panel;
   >    - the `FRAME ... MED ... P99` numbers during the Kuun-Lan close-up, and while the whole fleet is in view in
   >      Mission01 (FULL DETAIL on: above 6.94 ms means it misses 144 Hz, which is producer 4's work);
   >    - whether `ALPHA` moves between 0 and 1 in game (Smooth motion on), or shows -1.
   > 3. **The quit after a resolution change.** In the front end, go to Options -> Video, pick another resolution, press
   >    Accept, and answer the "keep these settings" box with Yes. Back at the main menu, press Quit **once**. The game
   >    should close at the first try. Report whether it needed a second Quit. Afterwards, set the resolution back to
   >    3840x2160 the same way (or edit `screenWidth` / `screenHeight` in `Cataclysm.cfg`).
   > 4. **Changing a setting in the session.** Untick and tick SMOOTH MOTION on Options -> Video and press Accept. The log
   >    should get a new `hwi-settings: ... [options accept]` line.

   Answered already (`m3-r2-user.md`, 2026-10-24): the overlay is readable at 200 % and covers nothing needed (*"At 200% ui scale,
   everythings seems to be normally viewable at this size"*); **the key: USER DECISION (binding)** *"let's dedicate a key from the
   F1-F12 suite for this overlay"*: one of F1-F12 (candidate F12), not the Menu key (which may stay as a second toggle); not applied
   inside M3-R2, queued as M3-R3 item 0. Until then the Menu key or `-- /overlay` starts it. Open: items 1, 3 and 4.
3. **Producer 3's check (hyperspace-assembly)**, verbatim:
   > `tools/play.sh` (the release build, 3840x2160, as on 10-21). New game, watch Mission01's opening: after the nislet,
   > the Kuun-Lan arrives in the white flash with the camera close on it.
   > 1. The gate: a large blue-white rectangle that spans the whole Kuun-Lan (not a small frigate-sized one) and slides from
   >    its bow to its stern as the ship comes out. Report whether it matches the vessel's size.
   > 2. The back modules (engines, lower spine, ore-can armour, at the stern): they must be there as the gate passes them
   >    and must NOT vanish and pop back about a second after the gate is gone. Report any part that appears late or
   >    flickers.
   > 3. Optional, Mission02's start (the same arrival from further out) and any jump out (Mission03's end): one gate over
   >    the whole ship, the modules never cut in half while the ship enters the gate.
   > 4. Optional A/B: `tools/play.sh -- /noHsAssembly` shows the exe's look (identical in these missions; it differs only
   >    if a module's wait outlasted the gate's opening).

   Default: the exe's gate and the assembly layer ON (nothing to decide unless a part still looks wrong).
4. **Producer 4's check (mesh-batching)**, verbatim:
   > `tools/play.sh` (release, the user's own Cataclysm.cfg: FULL DETAIL on, Smooth motion on), at 3840x2160 with the overlay
   > on: `tools/play.sh -- /overlay /frameLog ~/hwi-frame-batched-$(date +%H%M).log`. New Game: the Mission01 opening
   > cutscene (the Kuun-Lan close-up) and the first minutes of Mission01 with the whole fleet in view (zoom out to see every
   > ship), then a skirmish against one computer with a large battle on screen. Where to look: the overlay's FRAME line (the
   > p99 should stay at or under 6.9 ms where the M3-R1 build showed ~10-15 ms) and the ships themselves: the hulls, the team
   > colours (base and stripe), the Kuun-Lan's green-lit bays, the engine glows and the damage glow on hit
   > ships must look exactly as before. A/B: the same moments with `tools/play.sh -- /overlay /meshImmediate /frameLog
   > ~/hwi-frame-immediate-$(date +%H%M).log` (the old path, this run only). What to report: any difference in how a ship
   > looks between the two runs (a missing face, a wrong colour or texture, a glow that does not fade, a part drawn through
   > another), the FRAME figures of both runs in the big battle, and both frame logs (`python3 -I
   > tools/frame_log_report.py <file>` gives the p50 / p99 and the seconds over budget).

   Default: batching ON (`/meshImmediate` for an A/B run). Note from this step: with a heavy background load (this step's load average
   ~30 from VS Code's indexer) the p99 rises well above 6.9 ms while the median stays ~5 ms: compare on a quiet desktop.
5. **What `cycle/answers/m3-r2-user.md` already answered** (applied as defaults): the GPU free for measurements (2026-10-22 11:22 and
   10-24; the idle check still applies); the overlay key (above, binding, M3-R3 item 0); the overlay at 200 % fine; the camera jitter
   *"I would mark the camera issue as fixed."* (M3-R1's item 4 closed with Smooth motion ON); M3-R1's decisions **not answered**: FULL
   DETAIL on by default, the 2.39:1 letterbox height, the borderless fullscreen-desktop 4K window, the exe's level-following trails,
   decisions 111-121 as recorded: the defaults stand.
6. **M3-R1's questions still open** (docs/m3-r1-summary.md "What the user must decide"; question 1's Smooth motion part is answered by
   item 4 above): **2** the letter height at 16:9 and wider (GoK's 2.39:1, 277 bar rows at 4K, default; say "the exe's" for its 42 rows:
   M3-R3); **3** the exe's additive blending after full bars (default the port's normal blending; say "the exe's"); **4** FULL DETAIL
   (its 4K cost is now producer 4's: met on an idle machine; trails at the LOD-0 style or the exe's level-following trails (default);
   the effects cap lifted or left (default); the sensors manager's blips stay blips); **5** the UI scale and the window (200 % / 150 %
   auto, the borderless 4K window: confirmed in use by the third play-test; the BTG star size, now `CAT-TODO(M3-R3)`; the sensors map's
   thin lines at 200 %); **6** nothing (team colours); **10** the defaults recorded as decisions 111-121 and now 122-132.
7. **Decision 48** (the fourth recording with the DLL's `CAT_SHIP_coordsys` 0x98 edit): still none (the newest
   `original/oracle/oracle01-rec-*` is 10-19's); both open divergences point at the bWorker 44's coordsys, which only this recording
   makes comparable. Recommendation unchanged: before M3's rung producer runs (row 4).
8. **The mode-B playback** of `hwirec01` / `hwirec02` (the user's session; the commands in docs/m2-r10-summary.md "What the user must
   decide" 2 still apply: the right-hand sides are now this step's `m3r2i-e-play01-a` / `m3r2i-e-play02-a`, IDENTICAL to M3-R1's and
   R10's in every field).
9. **The two saves of the M0 play-test** (8: one mission, one skirmish): still open.
10. **The defaults recorded as decisions 122-132** in cycle/STATE.md (say if you want otherwise), and the order: the play-test first
    (short) while M3-R3's exe-reading items (1, 2, 4, 6 of `m3-r3-user.md`) start, the Windows 11 zip whenever you have the machine, the
    soak after M3-R3 (recommendation), or M3-R3 straight away without the play-test.

## The play-test brief, refreshed (replaces M3-R1's; the four items first, then M3-R1's open items and R10's; the M3-R3 queue last)

An attended run is the only one that may be audible or on screen (docs/CONSTRAINTS.md 5). Launch with `tools/play.sh` (read its
header: the release build, `--rebuild` once after this checkpoint so `build/release/hwi-engine` carries the four ports; `--debug` for the
ASan build when a crash needs a report; the harness data dir; the private home `original/play-home`, whose `.homeworld/Cataclysm.cfg`
keeps your colours, `noLOD 1`, `screenWidth 3840` / `screenHeight 2160` / `fullScreen 0` and, since 10-21, `objInterpolate 1` /
`nisInterpolate 1`; engine switches after `--`; the log under `original/play-home/logs/play-<timestamp>.log`; no gamescope):

```
cd ~/Documents/GameDev/Homeworld-Infection/.claude/worktrees/m2
tools/play.sh --rebuild                # 3840x2160 borderless fullscreen-desktop by the fit rule; switches after --
```

Report per item:

1. **The settings line and the overlay (item 1).** The newest `original/play-home/logs/play-*.log` starts with an `hwi-settings:` line:
   it should read `objInterpolate=1 (cfg) nisInterpolate=1 (cfg) letterboxBehind=0 (default) noLOD=1 (cfg) uiScale=0->200% (cfg, auto)
   overlay=0 (default) meshImmediate=0 (default) fit=fullscreen-desktop (display size -> fullscreen desktop) 3840x2160 switches=none
   [start]`; report it if it says anything else. Press the keyboard's **Menu key** (the "application" key; **an F key is coming in
   M3-R3** per your decision; without a Menu key start with `-- /overlay`): a dark panel top-left with `FRAME <ms> MED <p50> P99 <p99>`,
   `TICK`, `ALPHA`, the settings row and `VERTS`; press it again and it goes. Report whether the key toggles it, the `FRAME ... MED ...
   P99` numbers during the Kuun-Lan close-up and with the whole fleet in view in Mission01 (above 6.94 ms misses 144 Hz), whether
   `ALPHA` moves between 0 and 1 in a game (Smooth motion on) or shows -1, and whether anything in the HUD sits under the panel. Then
   Options -> Video: untick and tick SMOOTH MOTION, Accept: the log gets a new `hwi-settings: ... [options accept]` line.
2. **The quit after a resolution change (item 1).** Options -> Video, pick another resolution, Accept, answer "keep these settings"
   with Yes; back at the main menu press Quit **once**: the game must close at the first try (it needed two before). Report whether it
   needed a second Quit; then set the resolution back to 3840x2160 the same way (or edit `screenWidth` / `screenHeight` in the cfg).
3. **The cutscene flicker (item 2): the instrument, not a fix.** Run `tools/play.sh -- /catFrameSig ~/hwi-sig-$(date +%H%M).txt`.
   New campaign: watch the opening in-engine cutscene after Animatic01 (the Kuun-Lan close-up, n01 23-33 s, the narrow-angle shot), then
   Mission01's arrival cutscene and the letterboxed close-up that follows. Watch the whole frame for a flash that shows the backdrop with
   no ships and no black bars; note which cutscene, roughly how many seconds in, and whether on a cut between shots or mid-pan (producer
   2's two open hints). Report whether it happened, the moment, and hand over the file (or run `python3 -I tools/dump_outliers.py
   <file>`: `short runs=0` with a flicker seen means the engine drew everything and the cause is the presentation; a run names the frame
   and what was missing). Optional A/B: `tools/play.sh --driver wayland -- /catFrameSig ...` (SDL's native Wayland instead of XWayland).
4. **The hyperspace arrival (items 3 and 4 of 10-21).** New game, after the nislet the Kuun-Lan arrives in the white flash with the
   camera close on it: the gate must be a large blue-white rectangle spanning the whole Kuun-Lan (not frigate-sized) that slides from the
   bow to the stern as the ship comes out; the back modules (engines, lower spine, ore-can armour, at the stern) must be there as the
   gate passes them and must NOT vanish and pop back about a second after the gate is gone. Optional: Mission02's start (the same arrival
   from further out) and Mission03's jump out at its end: one gate over the whole ship, the modules never cut in half as the ship enters
   the gate. A/B: `tools/play.sh -- /noHsAssembly` (the exe's look; identical in these missions unless a module's wait outlasts the
   gate's opening). Report whether the gate matches the vessel's size and any part that appears late or flickers.
5. **FULL DETAIL under the 144 Hz budget (M3-R1's question 4).** `tools/play.sh -- /overlay /frameLog ~/hwi-frame-batched-$(date
   +%H%M).log`: the Mission01 opening cutscene, the first minutes of Mission01 with the whole fleet in view (zoom out to see every ship),
   then a skirmish against one computer with a large battle on screen. Watch the overlay's FRAME line (the p99 should stay at or under
   6.9 ms where the 10-21 build showed ~10-15 ms) and the ships: hulls, team colours (base and stripe), the Kuun-Lan's green-lit bays, the
   engine glows and the damage glow on hit ships must look exactly as before. A/B: the same moments with `tools/play.sh -- /overlay
   /meshImmediate /frameLog ~/hwi-frame-immediate-$(date +%H%M).log` (the old path, this run only). Report any difference in a ship's
   look between the two runs (a missing face, a wrong colour or texture, a glow that does not fade, a part drawn through another), the
   FRAME figures of both runs in the big battle, and hand over both frame logs (`python3 -I tools/frame_log_report.py <file>` prints the
   p50 / p99 and the seconds over budget). If the session runs an hour at 4K, keep the log: the drift test is the soak's first number.
6. **M3-R1's open items, unchanged**: the letterbox bars' height at 3840x2160 (277 rows each; the exe would draw 42: question 2) and
   whether the exe's brighter subtitles / cursor after full bars are wanted (question 3); the team colours after Mission03 (the P1 ion
   array frigates' and fighters') and on every jump; FULL DETAIL at the farthest zoom (no simpler model popping; `tools/play.sh --
   /lodBands` for the A/B) and whether the sensors manager's blips and the trails look right (question 4: trails at the LOD-0 style or
   not); the UI at 4K at 200 % (anything 2D that stayed small or sits wrong, any click that misses), the background stars' size (question
   5: the exe's fixed share of the screen or the fixed pixel size you have now) and whether the sensors map's lines are too thin; the
   window and the mouse at the edges (the cursor reaches the last row / column, the task bar pops up at the bottom edge; switching to the
   other monitor); the tutorial's pointer if a lesson's arrow seems a step behind.
7. **R10's open items, unchanged**: **the two animatics attended** (Animatic01 full screen before Mission01's opening NIS, at its own
   pace, with its sound and music, the cursor hidden; Animatic02 at Mission01's hyperspace white-out; the log's `animatic: play a=0 b=1
   file=Movies/Animatic01.bik` / `a=1 b=2`, **no `animatic: skipped` line**; whether Escape skips one); **the campaign from New Game as
   far as it goes** (you are past Mission04: on to Mission06's nebula tendrils, Mission09's Emergency Hyperspace, Mission17's Taiidan
   fighters at the Nomad Moon and the credits; per mission what differed from the GOG game, how long the fights take, where it stopped
   with the log's last lines); **the tutorial's 12 lessons** (Next through each, a Training save loading back); **a skirmish** against
   one computer for 10 minutes on a listed map, once with CPU Attacks "Humans only" / "Humans and allies", the managers' screens, **a
   recording** through the Record shortcut and its playback through Load Recorded Game.
8. **For every item**: any crash or hang with the terminal's last lines (`hwi-engine: fatal signal N`, the ASan report of a `--debug`
   run), the frame rate's drift over the session (the one-hour 4K@144 soak is M3's own item: the overlay's FRAME at the start and the
   end, or the frame log), and a screenshot of anything that looks wrong (the engine's own screenshots land in `original/play-home`'s
   ScreenShots).

**Queued for M3-R3, not to re-report** (your 2026-10-24 reports, `cycle/answers/m3-r3-user.md`, verbatim there): item 0 the overlay's
F key; 1 Mission02's dust belts on the tactical map; 2 Mission02's Bushan-Re hint dialog; 3 a nearest-texture launch option; 4
Mission03's white mines; 5 the pause jitter (ships and the camera hold their travel vector while paused); 6 the mine AI's
three-second seek and stop; 7 the missile corvette's single jet for four engines. Anything new you see in Missions 05 and beyond goes on
that list.

The known gaps to skip, not report: combat balance is untested by the self-tests (item 7 measures it); a failed hyperspace jump happens
only from the developer console; the GOG game's ships can differ from ours in the last bits of their velocities after about three
minutes of a skirmish (the rung's 2823 / 2871: invisible in play); the stage-5 LAN game and the Titan / WON screens are not ported; a
lesson or campaign save written by this build is in the tree's native format (only a recording's save is in the exe's format, decision
95), so it does not load in the GOG game; the window at 4K is borderless by design; the overlay's Menu key is the interim toggle (the
F key is M3-R3's); a flicker seen with `short runs=0` in the `frame.sig` is the presentation's, not the engine's (then M3-R3 tries
`--driver wayland` / a swap-interval setting).

## Decisions

**Applied this round:**
- The faithfulness defaults and decisions 17-121 of cycle/STATE.md; decision 70 (the table square root) under every run; decision 9 as
  "recorder" (stage 5's 23 sites untouched); decision 46's guard rails (the dumps compared whole and crc-stripped: **no dump moved this
  round**, every one byte-equal to M3-R1's / decision 118's references, the opt-outs together still byte-equal to R10's); decision 47's
  exclusions and decision 60's `real` in every replay comparison; decision 48 open (no fourth recording; `ships_rot` excluded); decision
  55 followed by producers 3 and 4 (12 review notes); decision 57 stands (the headless Mission01 stream did not move); decision 75
  untouched (the generators `--check` current); decisions 76 / 77 / 86 / 109 stand for every mission (17 / 17 IDENTICAL to M3-R1's and
  so to R10's); decision 84 (the ai soak's `rng_all` exempt); decision 92 / 117 extended (the harness: 127 below); decision 121's rules
  kept by every producer, audit and this step; the user's rules from the play-tests (Smooth motion ON, the overlay key one of F1-F12
  (queued), no gamescope, never fake input, the GPU on the user's word only).
- **Decision 44 unchanged** (128 below): the headless Mission01 stream of this step's build (`2131271d771f2056`, release
  `ce30c53259f034c8`) is IDENTICAL in every field over 6,001 ticks to M3-R1's `1ae66610c3f02410` / `234bcccfb948ab98` and so to R10's
  `5bd51234405ea0d8` (only the header's build identity differs), as every producer's and audit's Mission01 run of the round
  (`8d32ad5a04b0ff43`, `8285a210f60e4110`, …: the same content); **the rung unchanged**: 2823 / 2871 (177 / 118 with `/oracleLibmSqrt`).
- **Decision 46 / 118 unchanged** (129 below): the four guard-rail dumps byte-equal to M3-R1's references; producers 1, 2 and 4 moved
  no dump, producer 3 none (no hyperspace in the dump scenes).

**Still open (the user's; none blocks the play-test):**
- **8.** Two saves from the M0 play-test (one mission, one skirmish).
- **The play-test sign-off** (docs/PLAN.md); the refreshed brief above.
- **Decision 48**: a fourth recording with the fixed oracle DLL.
- **The mode-B playback** of `hwirec01` / `hwirec02` ("What the user must decide" 8).
- **M3-R1's questions 2-5, 7-10** and **this round's** ("What the user must decide" 1-4: the flicker's two hints and the `frame.sig`,
  the overlay's attended look and the quit at 4K, the arrival's look, the batched frame's look and figures); **the overlay's F key**
  (decided: one of F1-F12; which one, if not F12, say).

**Surfaced by M3-R2.** Each has a default in the tree; say if you want otherwise (numbered 122-132 in cycle/STATE.md):
122 the instrumentation (the `hwi-settings:` line with its sources and `[why]` tags; the overlay off by default, toggled by the console
row `overlay`, `/overlay` and the Menu key, drawn top-left at the UI scale; `/frameLog` + `frame_log_report.py`; producers add settings
as rows of `hwiSettings[]`); 123 the quit after a resolution change (`HWI-FIX` in `mainDestroyWindow`, the `modequit` self-test); 124
the flicker not reproduced (450,644 frames; the presentation hypothesis; `/catFrameSig`, `/catVirtualFps`, `dump_outliers.py`; no engine
change; the swap interval is set); 125 the hyperspace arrival (the exe's `hyperspaceBoxScale` gate, POPUP_INTO plane and host-inactive
unclip as the base; `HWI-FEATURE(hyperspace-assembly)` default ON, `/noHsAssembly`); 126 the mesh batching (`HWI-FEATURE(mesh-batching)`
default ON, `/meshImmediate` + the console row; the exact reuses; hwiprof; the 144 Hz budget met on an idle machine); 127 the harness
(31 modes: `modequit`; `--mode-to`, `--frame-log`, `--frame-sig`, `--virtual-fps`; the `settings:` line); 128 decision 44 and the rung
unchanged, presentation only held; 129 the dumps unchanged (decision 46 / 118's references stand); 130 the tags and the delta map (the
BTG star size re-targeted to `CAT-TODO(M3-R3)`); 131 the user's overlay-key decision (binding, M3-R3 item 0) and the M3-R3 queue
(`m3-r3-user.md`'s seven items); 132 the exit-test rules carried into M3-R3 (121 plus the quiet-CPU rule for a p99, the unique label
prefix per step, a "not reproduced" item's done-when, and a gate self-test under every switch the gate names: the `/letterboxBehind`
toggle check this exit test found).
