<!-- done: exit-test, fences, ports, deltamap, left, m3r4-plan, user-decisions, playtest-brief, decisions -->
# M3-R3 summary: the third play-test fix round (the overlay's F key + SHARP TEXTURES, the pause freeze, Mission02's dust belts + the Bushan-Re hint, Mission03's mines, the missile corvette's four trails) and the plan for M3-R4

Status: **COMPLETE 2026-10-26; the M3-R3 exit test MET** on every product condition of "Exit test" below, with two statements beside it (below): the two `universePause` dumps differ from producer 2's in a build-identity text alone, and the 4K cost is quoted from the idle GPU after the user's game ended;
**presentation only**: no simulation field of any stream of the round moved (the Mission01 stream with every setting of the round
flipped, the 17 missions, the guard rails and the replays IDENTICAL to M3-R2's, which are M3-R1's and R10's). This doc and the
cycle/STATE.md update are the integrate step's checkpoint commit (the commit after `6ad69ec`). Branch `m2`, worktree
`.claude/worktrees/m2`, workflow `cycle/workflows/m3-r3.js` (`dae51e1`; `ROUND_BASE 7aa3111`, the M3-R2 integrate's checkpoint and the
last commit touching `src/` before the round; the M3-R1 baseline `1ae66610c3f02410` / release `234bcccfb948ab98`), run id
`wf_8a0247ed-904`, run from the user's terminal session 38027cce (Fable), not from the orchestrator; **cut off by the session's end on
2026-10-25 ~20:30 during the mines audit and resumed on 2026-10-26** with the same run id (the mines audit re-run from scratch, then
engine-trails and its audit; this integrate step itself continued across a context restore). 11 agents: five producers in sequence, one
per item or item pair (settings-and-keys: the overlay's F key, the toggle check and SHARP TEXTURES; pause-freeze; mission02: the dust
belts and the hint; mines: the textures and the AI; engine-trails), five auditors (each checkpointed its port: `c8f97bb` 0.94,
`292a6da` 0.78, `96b0137` 0.94, `4ae7d91` 0.95, `6ad69ec` 0.944, all PASS-WITH-FIXES) and this integrate step. The user's answers:
cycle/answers/m3-r3-user.md (the mandate "yeah lets get r3 underway. also I want to implement a new option to support sharp texture
meshes or the original texture look", the third play-test's seven items verbatim with the orchestrator's corrections (`c698dd5`,
`06976ff`), the overlay's key "from the F1-F12 suite", the GPU free on 2026-10-26 (`9c07c9b`)), read first and again before this report;
M3-R2's four checks and M3-R1's questions unanswered: the defaults stand. User decisions: cycle/STATE.md "## M2" (the faithfulness
defaults and decisions 17-132 applied; decision 70's table `fsqrt` under every run; decision 44's baseline never re-baselined; decision
46's guard rails; decision 48 still open: no fourth recording, `ships_rot` stays excluded; mode B still the user's step).

## Exit test (this step, from a clean build of `6ad69ec`)

**Builds.** `tools/build.sh --debug --clean` and `--release --clean` on the tree at `6ad69ec` (11:00-11:01; gcc 13.3.0): both rc 0,
`[274/274] Linking target hwi-engine` (as M3-R2: no new source file this round), **0 `warning:` and 0 `error` lines** under `-Werror`.
The binaries were copied aside to `original/scratch-m2/m3r3-integrate/{debug,release}/hwi-engine` so that nothing rebuilt under the
runs; the build logs are `original/scratch-m2/m3r3-integrate/build-{debug,release}.log`. After the one source edit of this step (the
BTG.c comment, condition (f)) both builds were made again from clean: rc 0, `[274/274]`, 0 warnings (debug `30f405f513695eb9`,
91,184,872 B; release `a61a3a7ce4ff917d`, 5,787,216 B; `rebuild-{debug,release}.log`; the 16 extra bytes of the debug binary are
the `+` that meson's `git describe --dirty=+` puts into `HOMEWORLD_BUILD_VERSION` on a dirty tree). **No run used those**: every run
of the exit test ran the copied-aside clean binaries below (`HOMEWORLD_BUILD_VERSION` `6ad69ec`).

| build | size | sha256 |
|---|---:|---|
| debug (ASan + UBSan) | 91,184,856 B | `e4e1137e86218ae1…` |
| release (`-O2`, no sanitizers) | 5,787,216 B | `77c3861e6f74ea3d…` |

`tools/m2_layout32.sh` OK (`all ILP32 layout asserts hold`, 45 ship-specifics files); the generators current: `python3 -I
tools/m2_save32_gen.py --check` (`SaveCat32Layouts.c is current`), `tools/m2_kas2c_gen.py --check` (`CatMissions.h` / `meson.build`
current), `tools/m2_kasf_table_gen.py --dispatch --check` (`KASFuncTable.c` / `KASFuncDispatch.c` current), `tools/m2_strings_gen.py
--exe <main checkout>/original/game/cataclysm.exe --check` (`check: up to date`, exe rows 1705; the worktree has no `original/game`, so
the exe is the main checkout's by absolute path). **The Windows cross build** `tools/release_win64.sh --engine 6ad69ec` (llvm-mingw;
an incremental Meson / ninja build in the worktree's `build/win64`, `[32/32] Linking target hwi-engine.exe`; `hwi-engine.exe` imports `SDL2.dll`, `OPENGL32.dll` and Windows' own DLLs):
`dist/Homeworld-Infection-win64-20261026-6ad69ec.zip`, 2,912,703 B, sha256
`3d41aaefccb560b2fe56d639a81bb9f834d17520515fe18838db6619b512c195` (untested on Windows: the plan's row 2; the log
`original/scratch-m2/m3r3-integrate/logs/win64.log`).

**Runs.** 225 harness runs `m3r3i-e-*` on the plain `tools/m2_harness.sh` (silent, offscreen; `runs.txt` lines 4822-5046;
the prefix `m3r3i-e-` so that no producer's label is replaced) in ten correctness chains launched together at 11:07 under `nice 10`
(A: the rung's replays, the hwirec playbacks, missions 14-17; B: the Mission01 gates with every setting flipped incl. this round's, fe,
the scripted self-tests with the release SELFTEST pair; C1: missions 02-05 and the mines fence; C2: missions 06-09; D1: missions
10-13, Mission01's self-test, the tutorial, the lesson load, the failed-jump pair, the warp start, loadgame; D2: ai + the soak, the
skirmishes, the motion / nis dumps with their opt-outs, `/meshImmediate`, sharp and `/exeTrails` twins, the toggle self-test under
nine switch sets, uiscale x9, lodtest, the team colours; EF: `sp` 1,200 s, fe and `sp` at 3840x2160; G: the settings-and-keys, pause
and trails fences at 1024x768; G2: the Mission02 fence; K: the fences' 4K captures), **then H alone after all ten** (the GPU batch on
the release binary: the 4K frame-time runs default, `/exeTrails`, `/meshImmediate` and `/sharpTextures`, twice; run at 14:55-14:59 after `gpuwait` found the GPU at or under 40 % for 120 s (nvidia-smi 0 % / 3142 MiB and load average 7.12, 14.37, 17.67 at its start, 0 % / 8.82, 12.85, 16.45 between the two batches, 1 % / 8.64, 12.49, 16.24 after; it had waited since 11:49 for the user's game; `claude-ping` fired 2 time(s): 11:49:21, 14:57:12, since the chain library's `gpuwait` pings once per CALL and chain H calls it before each batch, so the second wait, two minutes of a 50 % sample between the batches, pinged again: a deviation from the user's once-per-gate rule to fix in `chains/lib.sh` before M3-R4);
`claude-gpu-done` after it at 14:59), beside the user's load (21-38 through the step: the user's game, a UE `GameThread` process at ~600 %
CPU with the GPU at 49-86 %, ran through the correctness chains; no run lost its summary; nobody edited the harness during the step).
**Every quitting run rc 0** except the timeouts by design: `m3r3i-e-sp` and `m3r3i-e-sp-4k` (rc 124 at 1,200 s, `sp` in Mission12 after 12 levels (126 golden-shot points logged, 23,331 file loads, 12 animatics skipped headless), `sp-4k` in Mission11 (119 points, 22,327 loads)),
`m3r3i-e-fe` and `m3r3i-e-fe-4k` (rc 124 at 120 s at Main_game_screen), and the five front-end capture runs of fence 1 (`sk-scr-fe` 61 s,
`sk-scr-fe4k` 90 s, `sk-base-cfg0` / `-cfg0c` / `-nofilter` 25 s: fe mode never quits by itself, the capture taken before the kill): nine
rc 124 in all, the other 216 rc 0; **ASan 0 in every run**; LeakSanitizer clean in every run.
The `.hwo` hashes differ from M3-R2's and the producers' for the same content: the stream header carries the build identity; IDENTICAL
below is `hwo_compare.py`'s verdict over every tick and packet record and all 21 fields.

**The runs** (every `m3r3i-e-*` run in `runs.txt` order; rc / wall from `runs.txt`, the rest from each `summary.txt`; the `.hwo`
sha256 is this step's own hash, comparable only within this step; the last column is the mode's own summary line(s), long ones cut
in the middle):

| run | mode | rc / wall | milestone | first stop | ASan; LeakSanitizer | `run.hwo` sha256 (size) | the mode's line |
|---|---|---|---|---|---|---|---|
| `m3r3i-e-sk-fkey` | ai | 0 / 26s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d0500d175717c1aa (64328 B) | aitest: tick=258 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ...  checks=16/16 result=PASS; FAIL lines: 0 / catshot: 3 files (tick=140 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-ai-a` | ai | 0 / 28s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0aeceba584a6212a (98768 B) | aitest: tick=398 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconcommand=49 build=17 harvest=33 skirmishplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS; FAIL lines: 0 |
| `m3r3i-e-sk-cap4k-lin` | ai | 0 / 34s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7fe217472dab4ddc (64328 B) | aitest: tick=258 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... ox=none letterboxBehind=0 ..); FAIL lines: 0 / lodstats: frames=81 median=35.553 p99=77.418 max=77.418 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r3i-e-mission-a` | mission | 0 / 34s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-sk-fkey-off` | ai | 0 / 23s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d0500d175717c1aa (64328 B) | aitest: tick=258 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ...  checks=16/16 result=PASS; FAIL lines: 0 / catshot: 3 files (tick=140 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-m02-shot-on` | missiontest | 0 / 51s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0d1fb43232ce1565 (1574523 B) | missiontest: mission=2 ticks=6398 objectives=1/6 result=FAIL; FAIL lines: 0 / catshot: 12 files (tick=5832 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=full letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-ai-b` | ai | 0 / 24s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0aeceba584a6212a (98768 B) | aitest: tick=398 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconcommand=49 build=17 harvest=33 skirmishplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS; FAIL lines: 0 |
| `m3r3i-e-r17-a` | replay | 0 / 65s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a0d8acbfb7828cd8 (2630926 B) | oracle: ticks=9601 packets=4800 ships=80 randSyncErr=0 univSyncErr=2911 blobSyncErr=4746 pktSyncErr=0 records=14405 file=/home/evanmedinger/Documents/GameDev/Homewor ...  converted, 47 ships, 0 size problems;replay started oracle01-r6 players=2 ships=47 tick=1 multiplayer=0; last tick line: replay tick 9471 packets=4736 records=14209 |
| `m3r3i-e-sk-cap4k-sw` | ai | 0 / 31s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7fe217472dab4ddc (64328 B) | aitest: tick=258 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... ox=none letterboxBehind=0 ..); FAIL lines: 0 / lodstats: frames=81 median=38.508 p99=73.055 max=73.055 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r3i-e-mission-b` | mission | 0 / 31s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-sk-cap-lin` | ai | 0 / 31s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7fe217472dab4ddc (64328 B) | aitest: tick=258 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... box=none letterboxBehind=0 ..); FAIL lines: 0 / lodstats: frames=81 median=37.761 p99=53.627 max=53.627 verts_median=25169 verts_max=25169 fullDetail=1 size=1024x768 |
| `m3r3i-e-ai-soak` | ai | 0 / 40s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | de7b525ffd94302e (984370 B) | aitest: tick=3998 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconcommand=49 build=17 harvest=33 skirmishplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS; FAIL lines: 0 |
| `m3r3i-e-mission-perturb` | mission | 0 / 32s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ca22a72601a3f31a (1476419 B) | mission: last tick line: mission tick 5888 records=5892; ships loaded 33 |
| `m3r3i-e-m02-shot-off` | missiontest | 0 / 48s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0d1fb43232ce1565 (1574523 B) | missiontest: mission=2 ticks=6398 objectives=1/6 result=FAIL; FAIL lines: 0 / catshot: 12 files (tick=5832 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=full letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-mission-rel-a` | mission | 0 / 8s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 316025e5c47989e1 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-m2-a` | missiontest | 0 / 106s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b6393960ef8abba0 (6381116 B) | missiontest: mission=2 ticks=25936 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r3i-e-sk-cap-sw` | ai | 0 / 30s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7fe217472dab4ddc (64328 B) | aitest: tick=258 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... box=none letterboxBehind=0 ..); FAIL lines: 0 / lodstats: frames=81 median=36.255 p99=44.789 max=44.789 verts_median=25169 verts_max=25169 fullDetail=1 size=1024x768 |
| `m3r3i-e-mission-rel-b` | mission | 0 / 8s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 316025e5c47989e1 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-skirmish-lfe2-a` | skirmish | 0 / 23s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | bbcb4623ae8caff1 (147967 B) | skirmishtest: ticks=600 ships=91 result=PASS; FAIL lines: 0 |
| `m3r3i-e-mission-rel-perturb` | mission | 0 / 8s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 069e4a09b5addd63 (1476419 B) | mission: last tick line: mission tick 5888 records=5892; ships loaded 33 |
| `m3r3i-e-r17-b` | replay | 0 / 60s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a0d8acbfb7828cd8 (2630926 B) | oracle: ticks=9601 packets=4800 ships=80 randSyncErr=0 univSyncErr=2911 blobSyncErr=4746 pktSyncErr=0 records=14405 file=/home/evanmedinger/Documents/GameDev/Homewor ...  converted, 47 ships, 0 size problems;replay started oracle01-r6 players=2 ships=47 tick=1 multiplayer=0; last tick line: replay tick 9471 packets=4736 records=14209 |
| `m3r3i-e-sk-cap-cfg` | ai | 0 / 28s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7fe217472dab4ddc (64328 B) | aitest: tick=258 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... box=none letterboxBehind=0 ..); FAIL lines: 0 / lodstats: frames=81 median=25.313 p99=33.508 max=33.508 verts_median=25169 verts_max=25169 fullDetail=1 size=1024x768 |
| `m3r3i-e-skirmish-lfe2-b` | skirmish | 0 / 24s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | bbcb4623ae8caff1 (147967 B) | skirmishtest: ticks=600 ships=91 result=PASS; FAIL lines: 0 |
| `m3r3i-e-r18-a` | replay | 0 / 39s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | eae711646d1b9e7d (966649 B) | oracle: ticks=3527 packets=1763 ships=55 randSyncErr=0 univSyncErr=4 blobSyncErr=1619 pktSyncErr=0 records=5294 file=/home/evanmedinger/Documents/GameDev/Homeworld-I ...  converted, 47 ships, 0 size problems;replay started oracle01-r18 players=2 ships=47 tick=1 multiplayer=0; last tick line: replay tick 3327 packets=1664 records=4993 |
| `m3r3i-e-mission-noi` | mission | 0 / 48s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-skirmish-armed8` | skirmish | 0 / 33s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4c3297929eddd609 (147967 B) | skirmishtest: ticks=600 ships=54 result=PASS; FAIL lines: 0 |
| `m3r3i-e-sk-cap-cfgh` | ai | 0 / 39s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7fe217472dab4ddc (64328 B) | aitest: tick=258 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... box=none letterboxBehind=0 ..); FAIL lines: 0 / lodstats: frames=81 median=40.469 p99=60.402 max=60.402 verts_median=25169 verts_max=25169 fullDetail=1 size=1024x768 |
| `m3r3i-e-m10-a` | missiontest | 0 / 188s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8887b807b263dae2 (5397854 B) | missiontest: mission=10 ticks=21939 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r3i-e-sk-ku4k-lin` | skirmish | 0 / 139s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 54a7a36160d91654 (29887 B) | skirmishtest: ticks=120 ships=91 result=PASS; FAIL lines: 0 / catshot: 1 files (tick=100 frame=0 segment=0 size=3840x2160 ui=1920x1080 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 / scene:  |
| `m3r3i-e-m6-a` | missiontest | 0 / 205s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8c17835f5f3895e0 (7629566 B) | missiontest: mission=6 ticks=31011 objectives=7/7 result=PASS; FAIL lines: 0 |
| `m3r3i-e-r18-b` | replay | 0 / 46s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | eae711646d1b9e7d (966649 B) | oracle: ticks=3527 packets=1763 ships=55 randSyncErr=0 univSyncErr=4 blobSyncErr=1619 pktSyncErr=0 records=5294 file=/home/evanmedinger/Documents/GameDev/Homeworld-I ...  converted, 47 ships, 0 size problems;replay started oracle01-r18 players=2 ships=47 tick=1 multiplayer=0; last tick line: replay tick 3327 packets=1664 records=4993 |
| `m3r3i-e-mission-nonis` | mission | 0 / 39s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-sk-cap-live` | ai | 0 / 32s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7fe217472dab4ddc (64328 B) | aitest: tick=258 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... box=none letterboxBehind=0 ..); FAIL lines: 0 / lodstats: frames=81 median=39.686 p99=77.114 max=77.114 verts_median=25169 verts_max=25169 fullDetail=1 size=1024x768 |
| `m3r3i-e-motion-noi` | motion | 0 / 50s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1431 sampled=1431 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-motion-noi/motion.dump result=PASS; dump motion.dump 1431 frames, sha256 c503c869f0dc9cdb |
| `m3r3i-e-m2-b` | missiontest | 0 / 121s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b6393960ef8abba0 (6381116 B) | missiontest: mission=2 ticks=25936 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r3i-e-mission-libm` | mission | 0 / 32s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-m02-hint-n` | missiontest | 0 / 145s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1ff0b886c95b78c5 (7266716 B) | missiontest: mission=2 ticks=29536 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r3i-e-sk-ku-lin` | skirmish | 0 / 36s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 54a7a36160d91654 (29887 B) | skirmishtest: ticks=120 ships=91 result=PASS; FAIL lines: 0 / catshot: 1 files (tick=100 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 / scene:  |
| `m3r3i-e-r17-libm` | replay | 0 / 60s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 002abc7fa5caf08d (2630929 B) | oracle: ticks=9601 packets=4800 ships=80 randSyncErr=3347 univSyncErr=4693 blobSyncErr=4746 pktSyncErr=0 records=14405 file=/home/evanmedinger/Documents/GameDev/Home ...  converted, 47 ships, 0 size problems;replay started oracle01-r6 players=2 ships=47 tick=1 multiplayer=0; last tick line: replay tick 9471 packets=4736 records=14209 |
| `m3r3i-e-motion-on` | motion | 0 / 47s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1431 sampled=1431 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-motion-on/motion.dump result=PASS; dump motion.dump 1431 frames, sha256 c0e411662fa2f527 |
| `m3r3i-e-mission-lb` | mission | 0 / 31s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-sk-ku-sw` | skirmish | 0 / 35s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 54a7a36160d91654 (29887 B) | skirmishtest: ticks=120 ships=91 result=PASS; FAIL lines: 0 / catshot: 1 files (tick=100 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 / scene:  |
| `m3r3i-e-r18-libm` | replay | 0 / 34s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8644ae5a69eefa23 (966654 B) | oracle: ticks=3527 packets=1763 ships=55 randSyncErr=781 univSyncErr=1658 blobSyncErr=1619 pktSyncErr=0 records=5294 file=/home/evanmedinger/Documents/GameDev/Homewo ...  converted, 47 ships, 0 size problems;replay started oracle01-r18 players=2 ships=47 tick=1 multiplayer=0; last tick line: replay tick 3327 packets=1664 records=4993 |
| `m3r3i-e-mission-lbsw` | mission | 0 / 31s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-motion-optout` | motion | 0 / 47s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1431 sampled=1431 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-motion-optout/motion.dump result=PASS; dump motion.dump 1431 frames, sha256 183681235a3fdc7b |
| `m3r3i-e-r17-rel` | replay | 0 / 12s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | af8301aa516141f8 (2630926 B) | oracle: ticks=9601 packets=4800 ships=80 randSyncErr=0 univSyncErr=2911 blobSyncErr=4746 pktSyncErr=0 records=14405 file=/home/evanmedinger/Documents/GameDev/Homewor ...  converted, 47 ships, 0 size problems;replay started oracle01-r6 players=2 ships=47 tick=1 multiplayer=0; last tick line: replay tick 9471 packets=4736 records=14209 |
| `m3r3i-e-m3-a` | missiontest | 0 / 90s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 177f18624d530736 (4385810 B) | missiontest: mission=3 ticks=17825 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r3i-e-sk-ku4k-sw` | skirmish | 0 / 125s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 54a7a36160d91654 (29887 B) | skirmishtest: ticks=120 ships=91 result=PASS; FAIL lines: 0 / catshot: 1 files (tick=100 frame=0 segment=0 size=3840x2160 ui=1920x1080 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 / scene:  |
| `m3r3i-e-mission-lodbands` | mission | 0 / 31s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-m02-hint-e` | missiontest | 0 / 96s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 788a0779fa27131e (6180380 B) | missiontest: mission=2 ticks=25120 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r3i-e-sk-scr-fe` | fe | 124 / 61s | Main_game_screen | none: still running at the 60s timeout | ASan 0; no unsuppressed leak |  | catshot: 1 files (tick=0 frame=400 segment=0 size=1024x768 ui=1024x768 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-play01-a` | replay | 0 / 34s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 900128813f41a06b (1315714 B) | oracle: ticks=4801 packets=2400 ships=57 randSyncErr=0 univSyncErr=0 blobSyncErr=0 pktSyncErr=0 records=7205 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infe ... unks converted, 54 ships, 0 size problems;replay started hwirec01 players=2 ships=54 tick=1 multiplayer=0; last tick line: replay tick 4607 packets=2304 records=6913 |
| `m3r3i-e-sk-m14k-lin` | mission | 0 / 23s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e1168b094eddfef5 (79088 B) | mission: last tick line: mission tick 256 records=259; ships loaded 33 / catshot: 2 files (tick=150 frame=0 segment=0 size=3840x2160 ui=1920x1080 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 / lodtest: tick=100 fullDetail=1 objects=27 drawnAbove0=0 bandAbove0=27 result=PASS |
| `m3r3i-e-m10-b` | missiontest | 0 / 166s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8887b807b263dae2 (5397854 B) | missiontest: mission=10 ticks=21939 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r3i-e-sk-scr-ig` | mission | 0 / 20s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | da8363760ee140fe (34808 B) | mission: last tick line: ; ships loaded 33 / catshot: 1 files (tick=120 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-mission-nolod` | mission | 0 / 31s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-motion-on-imm` | motion | 0 / 52s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1431 sampled=1431 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-motion-on-imm/motion.dump result=PASS; dump motion.dump 1431 frames, sha256 c0e411662fa2f527 |
| `m3r3i-e-sk-m14k-sw` | mission | 0 / 23s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e1168b094eddfef5 (79088 B) | mission: last tick line: mission tick 256 records=259; ships loaded 33 / catshot: 2 files (tick=150 frame=0 segment=0 size=3840x2160 ui=1920x1080 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 / lodtest: tick=100 fullDetail=1 objects=27 drawnAbove0=0 bandAbove0=27 result=PASS |
| `m3r3i-e-play01-b` | replay | 0 / 34s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 900128813f41a06b (1315714 B) | oracle: ticks=4801 packets=2400 ships=57 randSyncErr=0 univSyncErr=0 blobSyncErr=0 pktSyncErr=0 records=7205 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infe ... unks converted, 54 ships, 0 size problems;replay started hwirec01 players=2 ships=54 tick=1 multiplayer=0; last tick line: replay tick 4607 packets=2304 records=6913 |
| `m3r3i-e-sk-base-cfg0` | fe | 124 / 25s | Main_game_screen | none: still running at the 25s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r3i-e-m6-b` | missiontest | 0 / 188s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8c17835f5f3895e0 (7629566 B) | missiontest: mission=6 ticks=31011 objectives=7/7 result=PASS; FAIL lines: 0 |
| `m3r3i-e-mission-uisw` | mission | 0 / 32s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-m3-b` | missiontest | 0 / 93s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 177f18624d530736 (4385810 B) | missiontest: mission=3 ticks=17825 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r3i-e-sk-base-cfg0c` | fe | 124 / 25s | Main_game_screen | none: still running at the 25s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r3i-e-play02-a` | replay | 0 / 31s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 246c77e55a7c685c (1315721 B) | oracle: ticks=4801 packets=2400 ships=71 randSyncErr=1909 univSyncErr=1984 blobSyncErr=11 pktSyncErr=0 records=7205 file=/home/evanmedinger/Documents/GameDev/Homewor ... nks converted, 103 ships, 0 size problems;replay started hwirec02 players=2 ships=91 tick=1 multiplayer=0; last tick line: replay tick 4607 packets=2304 records=6913 |
| `m3r3i-e-motion-sharp` | motion | 0 / 48s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1431 sampled=1431 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-motion-sharp/motion.dump result=PASS; dump motion.dump 1431 frames, sha256 1a2227d966ec9712 |
| `m3r3i-e-mission-uicfg` | mission | 0 / 32s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-sk-base-nofilter` | fe | 124 / 25s | Main_game_screen | none: still running at the 25s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r3i-e-play02-b` | replay | 0 / 32s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 246c77e55a7c685c (1315721 B) | oracle: ticks=4801 packets=2400 ships=71 randSyncErr=1909 univSyncErr=1984 blobSyncErr=11 pktSyncErr=0 records=7205 file=/home/evanmedinger/Documents/GameDev/Homewor ... nks converted, 103 ships, 0 size problems;replay started hwirec02 players=2 ships=91 tick=1 multiplayer=0; last tick line: replay tick 4607 packets=2304 records=6913 |
| `m3r3i-e-m11-a` | missiontest | 0 / 95s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d19814ded54871e0 (2036509 B) | missiontest: mission=11 ticks=8275 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r3i-e-mission-optout` | mission | 0 / 32s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-sk-scr-fe4k` | fe | 124 / 90s | Main_game_screen | none: still running at the 90s timeout | ASan 0; no unsuppressed leak |  | catshot: 1 files (tick=0 frame=400 segment=0 size=3840x2160 ui=1920x1080 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-motion-exe` | motion | 0 / 50s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1431 sampled=1431 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-motion-exe/motion.dump result=PASS; dump motion.dump 1431 frames, sha256 c0e411662fa2f527 |
| `m3r3i-e-et-c4k-on` | ai | 0 / 28s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 689a035b49991fff (103688 B) | aitest: tick=418 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... hecks=16/16 result=PASS; FAIL lines: 0 / catshot: 2 files (tick=300 frame=0 segment=0 size=3840x2160 ui=1920x1080 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-mission-ov` | mission | 0 / 33s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-m7-a` | missiontest | 0 / 110s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | dbd168ab702b8c91 (4082492 B) | missiontest: mission=7 ticks=16592 objectives=4/4 result=PASS; FAIL lines: 0 |
| `m3r3i-e-m4-a` | missiontest | 0 / 95s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b2df80cb273139f4 (5122089 B) | missiontest: mission=4 ticks=20818 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r3i-e-pz-key-on` | motion | 0 / 70s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1503 sampled=1503 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-pz-key-on/motion.dump result=PASS; dump motion.dump 1503 frames, sha256 4cc39108eb1a0193 |
| `m3r3i-e-et-c4k-exe` | ai | 0 / 27s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | fbb4f3269d0579d6 (103688 B) | aitest: tick=418 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... hecks=16/16 result=PASS; FAIL lines: 0 / catshot: 2 files (tick=300 frame=0 segment=0 size=3840x2160 ui=1920x1080 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-nis-noi` | nis | 0 / 55s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-nis-noi/nis.dump result=PASS; dump nis.dump 1440 frames, sha256 86cb155acffc180e |
| `m3r3i-e-mission-fl` | mission | 0 / 33s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 / framelog: seconds=12 frames=6000 refresh=144 ft p50=1.8 p95=3.5 p99=5.5 max=1205.131 over=17 (0.3%) ticks=6000 tick_p50=0.565 tick_max=1202.880 heap 41889600->41889600 (+0 B/min) pools=1 dupe=0->0 drift(thirds)=FAIL seconds_over_budget=1 |
| `m3r3i-e-et-p1mv4k-on` | missiontest | 0 / 16s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | bcbcdfdd4864a8b2 (49321 B) | missiontest: mission=3 ticks=198 objectives=0/5 result=FAIL; FAIL lines: 0 / catshot: 1 files (tick=160 frame=0 segment=0 size=3840x2160 ui=1920x1080 letterbox=full letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-m11-b` | missiontest | 0 / 94s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d19814ded54871e0 (2036509 B) | missiontest: mission=11 ticks=8275 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r3i-e-et-p1rest4k` | missiontest | 0 / 17s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 53f4cffbb62b5d9a (64081 B) | missiontest: mission=3 ticks=258 objectives=0/5 result=FAIL; FAIL lines: 0 / catshot: 1 files (tick=60 frame=0 segment=0 size=3840x2160 ui=1920x1080 letterbox=full letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-mission-imm` | mission | 0 / 31s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-nis-on` | nis | 0 / 56s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-nis-on/nis.dump result=PASS; dump nis.dump 1440 frames, sha256 0307f3be2044c29c |
| `m3r3i-e-pz-key-noi` | motion | 0 / 69s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1503 sampled=1503 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-pz-key-noi/motion.dump result=PASS; dump motion.dump 1503 frames, sha256 84b84e7378bf1838 |
| `m3r3i-e-mission-nohsa` | mission | 0 / 32s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-m4-b` | missiontest | 0 / 95s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b2df80cb273139f4 (5122089 B) | missiontest: mission=4 ticks=20818 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r3i-e-m02-shot4k-on` | missiontest | 0 / 55s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7dd7c389d7a38248 (1574523 B) | missiontest: mission=2 ticks=6398 objectives=1/6 result=FAIL; FAIL lines: 0 / catshot: 12 files (tick=5832 frame=0 segment=0 size=3840x2160 ui=1920x1080 letterbox=full letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-m7-b` | missiontest | 0 / 109s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | dbd168ab702b8c91 (4082492 B) | missiontest: mission=7 ticks=16592 objectives=4/4 result=PASS; FAIL lines: 0 |
| `m3r3i-e-mission-cfg0` | mission | 0 / 31s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-nis-optout` | nis | 0 / 53s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-nis-optout/nis.dump result=PASS; dump nis.dump 1440 frames, sha256 13ca70eea998e5de |
| `m3r3i-e-m14-a` | missiontest | 0 / 193s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c05153b6a0dc960f (5496008 B) | missiontest: mission=14 ticks=22338 objectives=4/4 result=PASS; FAIL lines: 0 |
| `m3r3i-e-pz-key-on-b` | motion | 0 / 68s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1467 sampled=1467 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-pz-key-on-b/motion.dump result=PASS; dump motion.dump 1467 frames, sha256 b5843cdfa5f91c57 |
| `m3r3i-e-m12-a` | missiontest | 0 / 108s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b719d46f084c859a (2705630 B) | missiontest: mission=12 ticks=10995 objectives=3/3 result=PASS; FAIL lines: 0 |
| `m3r3i-e-mission-sharpcfg` | mission | 0 / 28s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-m02-hint-h` | missiontest | 0 / 315s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 33d63823957df538 (22140125 B) | missiontest: mission=2 ticks=89998 objectives=4/6 result=FAIL; FAIL lines: 0 |
| `m3r3i-e-m5-a` | missiontest | 0 / 78s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 23626d660659b53f (4031324 B) | missiontest: mission=5 ticks=16384 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r3i-e-mission-sharpsw` | mission | 0 / 31s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-nis-on-imm` | nis | 0 / 59s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-nis-on-imm/nis.dump result=PASS; dump nis.dump 1440 frames, sha256 0307f3be2044c29c |
| `m3r3i-e-m02-shot4k-off` | missiontest | 0 / 53s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7dd7c389d7a38248 (1574523 B) | missiontest: mission=2 ticks=6398 objectives=1/6 result=FAIL; FAIL lines: 0 / catshot: 12 files (tick=5832 frame=0 segment=0 size=3840x2160 ui=1920x1080 letterbox=full letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-pz-univ-on` | motion | 0 / 67s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1503 sampled=1503 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-pz-univ-on/motion.dump result=PASS; dump motion.dump 1503 frames, sha256 781f8430d5bcac19 |
| `m3r3i-e-mission-nofilter` | mission | 0 / 31s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-nis-sharp` | nis | 0 / 54s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-nis-sharp/nis.dump result=PASS; dump nis.dump 1440 frames, sha256 871eac333fd603d0 |
| `m3r3i-e-mission-exetrails` | mission | 0 / 30s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-m12-b` | missiontest | 0 / 104s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b719d46f084c859a (2705630 B) | missiontest: mission=12 ticks=10995 objectives=3/3 result=PASS; FAIL lines: 0 |
| `m3r3i-e-m5-b` | missiontest | 0 / 77s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 23626d660659b53f (4031324 B) | missiontest: mission=5 ticks=16384 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r3i-e-mission-trailrow` | mission | 0 / 30s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-pz-univ-noi` | motion | 0 / 66s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1503 sampled=1503 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-pz-univ-noi/motion.dump result=PASS; dump motion.dump 1503 frames, sha256 6f5f15893daa5563 |
| `m3r3i-e-nis-exe` | nis | 0 / 53s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-nis-exe/nis.dump result=PASS; dump nis.dump 1440 frames, sha256 0307f3be2044c29c |
| `m3r3i-e-mission-fkey` | mission | 0 / 29s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c20ccda7fdae0f66 (1476370 B) | mission: last tick line: mission tick 5888 records=5891; ships loaded 33 |
| `m3r3i-e-toggle` | toggle | 0 / 13s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=1/1 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-toggle-noi` | toggle | 0 / 13s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=0/0 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-m14-b` | missiontest | 0 / 184s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c05153b6a0dc960f (5496008 B) | missiontest: mission=14 ticks=22338 objectives=4/4 result=PASS; FAIL lines: 0 |
| `m3r3i-e-toggle-nonis` | toggle | 0 / 13s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=1/0 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-mn-shot-lvl` | missiontest | 0 / 82s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4b14ef8162b0ec20 (4385810 B) | missiontest: mission=3 ticks=17825 objectives=5/5 result=PASS; FAIL lines: 0 / catshot: 4 files (tick=320 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-toggle-lb` | toggle | 0 / 13s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=1/1 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-pz-nis-on` | nis | 0 / 73s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=600 nisTime=36.000 frames=1512 sampled=1512 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-pz-nis-on/nis.dump result=PASS; dump nis.dump 1512 frames, sha256 055ff7ff35748319 |
| `m3r3i-e-toggle-lodbands` | toggle | 0 / 13s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=1/1 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-toggle-ov` | toggle | 0 / 13s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=1/1 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-toggle-imm` | toggle | 0 / 13s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=1/1 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-toggle-nofilter` | toggle | 0 / 13s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=1/1 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-mn-shot-drop` | missiontest | 0 / 72s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a480c56c308c26cd (4385810 B) | missiontest: mission=3 ticks=17825 objectives=5/5 result=PASS; FAIL lines: 0 / catshot: 16 files (tick=2000 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-toggle-sharp` | toggle | 0 / 13s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=1/1 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-pz-nis-noi` | nis | 0 / 71s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=600 nisTime=36.000 frames=1512 sampled=1512 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-pz-nis-noi/nis.dump result=PASS; dump nis.dump 1512 frames, sha256 06c1c33fd84457fb |
| `m3r3i-e-fe` | fe | 124 / 120s | Main_game_screen | none: still running at the 120s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r3i-e-toggle-exe` | toggle | 0 / 13s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | toggletest: start=1/1 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-m8-a` | missiontest | 0 / 324s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | efee0966b00cc71e (7223667 B) | missiontest: mission=8 ticks=29361 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r3i-e-statics` | statics | 0 / 15s | ship statics self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  |  |
| `m3r3i-e-us-4k-a` | uiscale | 0 / 16s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=3840x2160 logical=1920x1080 scale=200 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-statics-init` | statics | 0 / 15s | ship statics self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  |  |
| `m3r3i-e-us-4k-b` | uiscale | 0 / 16s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=3840x2160 logical=1920x1080 scale=200 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-meshes` | meshes | 0 / 14s | mesh self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | meshes: 1758 listed, 1729 .geo/.peo loaded, 29 .mad loaded, 0 with bad=; self-test done, 1758 files, 0 with bad objects |
| `m3r3i-e-m13-a` | missiontest | 0 / 219s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0faaf6710a8b833d (4744478 B) | missiontest: mission=13 ticks=19283 objectives=3/3 result=PASS; FAIL lines: 0 |
| `m3r3i-e-us-1440` | uiscale | 0 / 16s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=2560x1440 logical=1707x960 scale=150 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-us-1080` | uiscale | 0 / 16s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=1920x1080 logical=1920x1080 scale=100 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-textures` | textures | 0 / 24s | texture self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | textures: listing lookups 12403/12403 found, 0 size/flag mismatches, 0 bad sharedFrom;self-test done, 12403 listed, 12403 found in listing, 1742 without a file to load, 10661 registered in 3 batches, 10661 loaded, 0 not loaded, 9248 scaled down, 0 without bScalable, sizeof(texreg) 96 |
| `m3r3i-e-pz-nis-shot` | nis | 0 / 73s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=600 nisTime=36.000 frames=1512 sampled=1512 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-pz-nis-shot/nis.dump result=PASS; dump nis.dump 1512 frames, sha256 055ff7ff35748319 |
| `m3r3i-e-us-1024` | uiscale | 0 / 16s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=1024x768 logical=1024x768 scale=100 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-oracle` | oracle | 0 / 13s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | bf25e677ccbaa887 (140723 B) | oracle: ticks=512 packets=256 randSyncErr=0 univSyncErr=0 blobSyncErr=0 records=772 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-oracle/run.hwo; run.hwo 140723 bytes, sha256 bf25e677ccbaa887 |
| `m3r3i-e-oracle-rel-a` | oracle | 0 / 2s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e86cf80daddf15c2 (140723 B) | oracle: ticks=512 packets=256 randSyncErr=0 univSyncErr=0 blobSyncErr=0 records=772 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-oracle-rel-a/run.hwo; run.hwo 140723 bytes, sha256 e86cf80daddf15c2 |
| `m3r3i-e-oracle-rel-b` | oracle | 0 / 2s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e86cf80daddf15c2 (140723 B) | oracle: ticks=512 packets=256 randSyncErr=0 univSyncErr=0 blobSyncErr=0 records=772 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-oracle-rel-b/run.hwo; run.hwo 140723 bytes, sha256 e86cf80daddf15c2 |
| `m3r3i-e-oracle-rel-perturb` | oracle | 0 / 2s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e907c2605742ffd5 (140773 B) | oracle: ticks=512 packets=256 randSyncErr=0 univSyncErr=0 blobSyncErr=0 records=773 file=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-oracle-rel-perturb/run.hwo; run.hwo 140773 bytes, sha256 e907c2605742ffd5 |
| `m3r3i-e-us-fit1960` | uiscale | 0 / 16s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=1024x768 logical=1024x768 scale=100 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-mn-shot-lvl-imm` | missiontest | 0 / 116s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4b14ef8162b0ec20 (4385810 B) | missiontest: mission=3 ticks=17825 objectives=5/5 result=PASS; FAIL lines: 0 / catshot: 4 files (tick=320 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-etg` | etg | 0 / 15s | etg effect self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | etg: 264 listed; etgStartup done, 8 scripts, 197 effects loaded, 0 referenced but not loaded;self-test done, 9 scripts parsed, 255 .ebg listed, 58 unused (no script  ... but not loaded;created 618 effects;effect self-test done, 618 created, 3197 deleted while running, 1093 left at the end, peak 2515 effects, peak 3012 particle blocks |
| `m3r3i-e-us-fit2160` | uiscale | 0 / 16s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=1024x768 logical=1024x768 scale=100 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-us-4k-cfg100` | uiscale | 0 / 15s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=3840x2160 logical=3840x2160 scale=100 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-dock` | dock | 0 / 29s | dock self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | docktest: fighters=6 outside=6 corvettes=2 carrierleg=2 processorleg=1 RUs=1850 jackPausedTick=1024 result=PASS; FAIL lines: 0 |
| `m3r3i-e-m15-a` | missiontest | 0 / 239s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2d754826d33ab4c5 (3795164 B) | missiontest: mission=15 ticks=15424 objectives=3/3 result=PASS; FAIL lines: 0 |
| `m3r3i-e-us-4k-sw150` | uiscale | 0 / 16s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uiscaletest: physical=3840x2160 logical=2560x1440 scale=150 checks=45 fails=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-pz-scene-key` | skirmish | 0 / 78s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 23c3c9d8b1b71a2e (64327 B) | skirmishtest: ticks=260 ships=91 result=PASS; FAIL lines: 0 / scene: tick=248 frames=1944 sampled=1944 Bmoved=451 realFrames=0 realLate=0 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-pz-scene-key/scene.dump result=PASS |
| `m3r3i-e-command` | command | 0 / 25s | command self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | commandtest: tick=2998 link=252 mcv=1 unlink=285 research=1/1 mimic=1 point=1/1 result=PASS; FAIL lines: 0 |
| `m3r3i-e-et-c-on` | ai | 0 / 25s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 657295031352f9cf (103688 B) | aitest: tick=418 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ...  checks=16/16 result=PASS; FAIL lines: 0 / catshot: 2 files (tick=300 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-render` | render | 0 / 21s | render self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | rendertest: tick=599 frames=583 objects=582 lensflares=583 foglayers=1166 result=PASS |
| `m3r3i-e-lod-ai-on` | ai | 0 / 35s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b093aed2c0dc08cd (98768 B) | aitest: tick=398 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS; FAIL lines: 0 / lodtest: tick=300 fullDetail=1 objects=164 drawnAbove0=0 bandAbove0=164 result=PASS |
| `m3r3i-e-mn-shot-drop-imm` | missiontest | 0 / 98s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a480c56c308c26cd (4385810 B) | missiontest: mission=3 ticks=17825 objectives=5/5 result=PASS; FAIL lines: 0 / catshot: 16 files (tick=2000 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-sound` | sound | 0 / 21s | sound self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | soundtest: start: audio driver dummy, SFX 1, speech 1, mixer ticks 58;Music4Cat.wxd: 62 streams, track 0 (offset 8, bitrate 512, 2 channels) decoded 43 blocks;SectSp ... done: music 43, sect 124, beast 281 blocks decoded; 403 audible output blocks; mixer ticks 744; result=PASS; UBSan reports in fquant.c/fqcodec.c: 0; PATCH reports: 0 |
| `m3r3i-e-et-c-exe` | ai | 0 / 26s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 223beb10ad173f61 (103688 B) | aitest: tick=418 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ...  checks=16/16 result=PASS; FAIL lines: 0 / catshot: 2 files (tick=300 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-lod-ai-bands` | ai | 0 / 26s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b093aed2c0dc08cd (98768 B) | aitest: tick=398 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... irmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS; FAIL lines: 0 / lodtest: tick=300 fullDetail=0 objects=164 drawnAbove0=164 bandAbove0=164 result=PASS |
| `m3r3i-e-hud` | hud | 0 / 24s | hud self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | hudtest: tick=599 boxed=19 selected=154 frames=537 statusbars=6840 infopanels=537 pie=537 result=PASS |
| `m3r3i-e-et-b-on` | ai | 0 / 26s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f80c1d67de7828e6 (103688 B) | aitest: tick=418 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ...  checks=16/16 result=PASS; FAIL lines: 0 / catshot: 2 files (tick=300 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-ui` | ui | 0 / 19s | ui self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | uitest: selected=154 rows=10 popupframes=16 commands=26 targetrows=1 statsframes=8 result=PASS;game stats ended active=0 paused=0 |
| `m3r3i-e-et-b-exe` | ai | 0 / 25s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 877062f687468e51 (103688 B) | aitest: tick=418 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ...  checks=16/16 result=PASS; FAIL lines: 0 / catshot: 2 files (tick=300 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=none letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-fescreens` | fescreens | 0 / 13s | fescreens self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | fescreentest: files=22 loaded=22 screens=195 pushed=195 popped=195 wrong=0 skipped=0 listchanged=0 userregionstandins=84 result=PASS; FAIL lines: 0 / optionstest: options=35 startdefaults=35 readback=35 result=PASS |
| `m3r3i-e-m13-b` | missiontest | 0 / 222s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0faaf6710a8b833d (4744478 B) | missiontest: mission=13 ticks=19283 objectives=3/3 result=PASS; FAIL lines: 0 |
| `m3r3i-e-strings` | strings | 0 / 12s | strings self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | stringstest: ids=1705 mapback=1705 resolved=1681 nostring=24 wrongtext=0 wrongnostring=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-sp` | sp | 124 / 1201s | universe running (univUpdateCounter > 0) | none: still running at the 1200s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r3i-e-et-rs-on` | ai | 0 / 26s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2275e0ccaf49f5f2 (103688 B) | aitest: tick=418 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconcommand=49 build=17 harvest=33 skirmishplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS; FAIL lines: 0 |
| `m3r3i-e-keys` | keys | 0 / 13s | keys self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | keystest: rows=197 known=149 unknown=48 commands=147 bindings=149 startsave=1 saved=1 reload=1 reset=1 newkey=1 conflicts=1 shares=1 failures=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-mn-ailog` | missiontest | 0 / 87s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 177f18624d530736 (4385810 B) | missiontest: mission=3 ticks=17825 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r3i-e-tc-user` | missiontest | 0 / 85s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cc25f0b882e7945c (3910048 B) | missiontest: mission=1 ticks=2600 objectives=6/6 result=PASS; FAIL lines: 0 / catshot: 8 files (tick=900 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=full l ... after,Mission01_NIS01.mission:level,Mission01.mission:after,Mission01.mission:level,Mission02.mission:after,Mission02.mission:level,Mission02.mission:end result=PASS |
| `m3r3i-e-console` | console | 0 / 20s | console self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | consoletest: selected=154 frames=4 shown=38 set=31 commands=8 keys=32 kas=6 closed=1 failures=0 result=PASS; FAIL lines: 0 |
| `m3r3i-e-et-rs-exe` | ai | 0 / 26s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 827805c5727cb15a (103688 B) | aitest: tick=418 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconcommand=49 build=17 harvest=33 skirmishplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS; FAIL lines: 0 |
| `m3r3i-e-mn-chase-recon` | missiontest | 0 / 21s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 50c3afe133e5a399 (344523 B) | missiontest: mission=3 ticks=1398 objectives=0/5 result=FAIL; FAIL lines: 0 |
| `m3r3i-e-et-p1mv-on` | missiontest | 0 / 16s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d2903fdb11dd70b2 (64081 B) | missiontest: mission=3 ticks=258 objectives=0/5 result=FAIL; FAIL lines: 0 / catshot: 2 files (tick=160 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=full letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-m8-b` | missiontest | 0 / 322s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | efee0966b00cc71e (7223667 B) | missiontest: mission=8 ticks=29361 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r3i-e-mn-chase-acolyte` | missiontest | 0 / 25s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1da6957e444e78c4 (639723 B) | missiontest: mission=3 ticks=2598 objectives=1/5 result=FAIL; FAIL lines: 0 |
| `m3r3i-e-build-a` | build | 0 / 33s | build self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2c707048b2614a35 (1722370 B) | buildtest: tick=4205 driver=1 SU=40/100 RUs=25925 result=PASS; FAIL lines: 0 |
| `m3r3i-e-et-p1mv-exe` | missiontest | 0 / 16s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d2903fdb11dd70b2 (64081 B) | missiontest: mission=3 ticks=258 objectives=0/5 result=FAIL; FAIL lines: 0 / catshot: 2 files (tick=160 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=full letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-missiontest-a` | missiontest | 0 / 74s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a59672e794c76827 (3246588 B) | missiontest: mission=1 ticks=11424 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r3i-e-et-p1rest` | missiontest | 0 / 16s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 53f4cffbb62b5d9a (64081 B) | missiontest: mission=3 ticks=258 objectives=0/5 result=FAIL; FAIL lines: 0 / catshot: 1 files (tick=60 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=full letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-build-b` | build | 0 / 33s | build self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2c707048b2614a35 (1722370 B) | buildtest: tick=4205 driver=1 SU=40/100 RUs=25925 result=PASS; FAIL lines: 0 |
| `m3r3i-e-m15-b` | missiontest | 0 / 239s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2d754826d33ab4c5 (3795164 B) | missiontest: mission=15 ticks=15424 objectives=3/3 result=PASS; FAIL lines: 0 |
| `m3r3i-e-tc-red` | missiontest | 0 / 85s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cc25f0b882e7945c (3910048 B) | missiontest: mission=1 ticks=2600 objectives=6/6 result=PASS; FAIL lines: 0 / catshot: 8 files (tick=900 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=full l ... after,Mission01_NIS01.mission:level,Mission01.mission:after,Mission01.mission:level,Mission02.mission:after,Mission02.mission:level,Mission02.mission:end result=PASS |
| `m3r3i-e-nisparse` | nisparse | 0 / 13s | nisparse self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | nisparsetest: nis=14 nisScripts=14 animatics=18 objects=593 events=1488 cataclysmEvents=31 unknownKeywordLines=47 failed=0 result=PASS; FAIL lines: 0; ignored keywords: motionCueScale x12,stretch x1,closeslipgate x1,minimumLODSet x32,widescreenOut x1 |
| `m3r3i-e-fe-4k` | fe | 124 / 120s | Main_game_screen | none: still running at the 120s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r3i-e-modequit` | modequit | 0 / 12s | modequit self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | modequit: switched=1 result=PASS (the quit follows: the harness judges rc 0 and the quit line once); rc=0; "Quit game" lines: 1; quit: PASS |
| `m3r3i-e-mn-missile` | missiontest | 0 / 85s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 177f18624d530736 (4385810 B) | missiontest: mission=3 ticks=17825 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r3i-e-missiontest-b` | missiontest | 0 / 73s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a59672e794c76827 (3246588 B) | missiontest: mission=1 ticks=11424 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r3i-e-tutorial-a` | missiontest | 0 / 48s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 983932d28cbd6594 (2569841 B) | missiontest: ticks=10444 tutorial kas=1 lesson1=2 objectives=2 buttons=1 prompts=7/102/158 pointer=1 next=1108 lesson2=1110 lessons=12/12 complete=12/12 inOrder=1 nextClicks=11 result=PASS; FAIL lines: 0 |
| `m3r3i-e-tc-m3` | missiontest | 0 / 88s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 18b9047d39eda0d4 (4693802 B) | missiontest: mission=3 ticks=17825 objectives=5/5 result=PASS; FAIL lines: 0 / teamcolour: checks=6 failed=0 afterLoads=3 levels=Mission03.mission:after,Mission03.mission:level,Mission04_NIS02.mission:after,Mission04_NIS02.mission:level,Mission04.mission:after,Mission04.mission:end result=PASS |
| `m3r3i-e-tutorial-b` | missiontest | 0 / 47s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 983932d28cbd6594 (2569841 B) | missiontest: ticks=10444 tutorial kas=1 lesson1=2 objectives=2 buttons=1 prompts=7/102/158 pointer=1 next=1108 lesson2=1110 lessons=12/12 complete=12/12 inOrder=1 nextClicks=11 result=PASS; FAIL lines: 0 |
| `m3r3i-e-m9-a` | missiontest | 0 / 191s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4e2b839f9ce6cf73 (5072642 B) | missiontest: mission=9 ticks=20617 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r3i-e-tload-a` | loadgame | 0 / 25s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f36d6abebb31db26 (886516 B) | loadgame: ticks=3600 from=8361 to=11961 mission=1 tutorial=1 ships=30 objectives=1/1 kasTeams=10 result=PASS |
| `m3r3i-e-tload-b` | loadgame | 0 / 25s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f36d6abebb31db26 (886516 B) | loadgame: ticks=3600 from=8361 to=11961 mission=1 tutorial=1 ships=30 objectives=1/1 kasTeams=10 result=PASS |
| `m3r3i-e-m16-a` | missiontest | 0 / 207s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b6549e9b7d380558 (5295518 B) | missiontest: mission=16 ticks=21523 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r3i-e-fj3-a` | missiontest | 0 / 81s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 5c5ece5ace844989 (4346699 B) | missiontest: mission=3 ticks=17667 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r3i-e-m9-b` | missiontest | 0 / 189s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4e2b839f9ce6cf73 (5072642 B) | missiontest: mission=9 ticks=20617 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r3i-e-fj3-b` | missiontest | 0 / 81s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 5c5ece5ace844989 (4346699 B) | missiontest: mission=3 ticks=17667 objectives=5/5 result=PASS; FAIL lines: 0 |
| `m3r3i-e-warp10-a` | warpstart | 0 / 20s | warp-start self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c444a75e9c3e796e (24968 B) | ;LevelToWarpTo 10 / ) / warpstarttest: level=10 ticks=99 applied=1 level=1 kas=1 arrived=35 fleet=1 failed=0 result=PASS |
| `m3r3i-e-warp10-b` | warpstart | 0 / 21s | warp-start self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c444a75e9c3e796e (24968 B) | ;LevelToWarpTo 10 / ) / warpstarttest: level=10 ticks=99 applied=1 level=1 kas=1 arrived=35 fleet=1 failed=0 result=PASS |
| `m3r3i-e-m16-b` | missiontest | 0 / 204s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b6549e9b7d380558 (5295518 B) | missiontest: mission=16 ticks=21523 objectives=6/6 result=PASS; FAIL lines: 0 |
| `m3r3i-e-loadgame-a` | loadgame | 0 / 29s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 833b459a1e8d59d9 (886495 B) | loadgame: ticks=3600 from=1985 to=5585 mission=1 tutorial=2 ships=141 objectives=0/1 kasTeams=36 result=PASS |
| `m3r3i-e-loadgame-b` | loadgame | 0 / 29s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 833b459a1e8d59d9 (886495 B) | loadgame: ticks=3600 from=1985 to=5585 mission=1 tutorial=2 ships=141 objectives=0/1 kasTeams=36 result=PASS |
| `m3r3i-e-m17-a` | missiontest | 0 / 231s | mission self-test: the game's end (credits) | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 6eec1524e012e870 (3834034 B) | missiontest: mission=17 ticks=15584 objectives=4/4 result=PASS; FAIL lines: 0 |
| `m3r3i-e-m17-b` | missiontest | 0 / 225s | mission self-test: the game's end (credits) | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 6eec1524e012e870 (3834034 B) | missiontest: mission=17 ticks=15584 objectives=4/4 result=PASS; FAIL lines: 0 |
| `m3r3i-e-pz-univ-on2` | motion | 0 / 61s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1503 sampled=1503 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-pz-univ-on2/motion.dump result=PASS; dump motion.dump 1503 frames, sha256 781f8430d5bcac19 |
| `m3r3i-e-pz-univ-exe` | motion | 0 / 61s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1503 sampled=1503 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-pz-univ-exe/motion.dump result=PASS; dump motion.dump 1503 frames, sha256 781f8430d5bcac19 |
| `m3r3i-e-sk-cap-exe` | ai | 0 / 27s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7fe217472dab4ddc (64328 B) | aitest: tick=258 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... box=none letterboxBehind=0 ..); FAIL lines: 0 / lodstats: frames=81 median=33.420 p99=44.093 max=44.093 verts_median=25169 verts_max=25169 fullDetail=1 size=1024x768 |
| `m3r3i-e-m02-shot-exe` | missiontest | 0 / 42s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0d1fb43232ce1565 (1574523 B) | missiontest: mission=2 ticks=6398 objectives=1/6 result=FAIL; FAIL lines: 0 / catshot: 12 files (tick=5832 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=full letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-sp-4k` | sp | 124 / 1201s | universe running (univUpdateCounter > 0) | none: still running at the 1200s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r3i-e-x-univ-4ae7d91` | motion | 0 / 59s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1503 sampled=1503 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-x-univ-4ae7d91/motion.dump result=PASS; dump motion.dump 1503 frames, sha256 9f4eab010c2200c1 |
| `m3r3i-e-x-univ-shot` | motion | 0 / 61s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1503 sampled=1503 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-x-univ-shot/motion.dump result=PASS; dump motion.dump 1503 frames, sha256 781f8430d5bcac19 |
| `m3r3i-e-x-univ-shot-4ae7d91` | motion | 0 / 60s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | motiontest: tick=223 frames=1503 sampled=1503 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r3i-e-x-univ-shot-4ae7d91/motion.dump result=PASS; dump motion.dump 1503 frames, sha256 9f4eab010c2200c1 |
| `m3r3i-e-x-m02-4ae7d91` | missiontest | 0 / 43s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e691dfbb798098c3 (1574523 B) | missiontest: mission=2 ticks=6398 objectives=1/6 result=FAIL; FAIL lines: 0 / catshot: 12 files (tick=5832 frame=0 segment=0 size=1024x768 ui=1024x768 letterbox=full letterboxBehind=0 ..); FAIL lines: 0 |
| `m3r3i-e-perf-on1` | ai | 0 / 6s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0f768355db11c1df (123368 B) | aitest: tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... st=177 checks=16/16 result=PASS; FAIL lines: 0 / lodstats: frames=301 median=3.743 p99=5.643 max=6.486 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r3i-e-perf-exe1` | ai | 0 / 5s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0f768355db11c1df (123368 B) | aitest: tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... st=177 checks=16/16 result=PASS; FAIL lines: 0 / lodstats: frames=301 median=3.483 p99=5.043 max=7.144 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r3i-e-perf-imm1` | ai | 0 / 8s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0f768355db11c1df (123368 B) | aitest: tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... =177 checks=16/16 result=PASS; FAIL lines: 0 / lodstats: frames=301 median=8.096 p99=11.699 max=12.577 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r3i-e-perf-sw1` | ai | 0 / 5s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0f768355db11c1df (123368 B) | aitest: tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... st=177 checks=16/16 result=PASS; FAIL lines: 0 / lodstats: frames=301 median=3.472 p99=5.054 max=5.863 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r3i-e-perf-on2` | ai | 0 / 6s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0f768355db11c1df (123368 B) | aitest: tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... st=177 checks=16/16 result=PASS; FAIL lines: 0 / lodstats: frames=301 median=3.422 p99=4.886 max=6.685 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r3i-e-perf-exe2` | ai | 0 / 5s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0f768355db11c1df (123368 B) | aitest: tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... st=177 checks=16/16 result=PASS; FAIL lines: 0 / lodstats: frames=301 median=3.240 p99=5.061 max=6.088 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r3i-e-perf-imm2` | ai | 0 / 8s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0f768355db11c1df (123368 B) | aitest: tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... =177 checks=16/16 result=PASS; FAIL lines: 0 / lodstats: frames=301 median=8.003 p99=13.076 max=13.445 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r3i-e-perf-sw2` | ai | 0 / 5s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0f768355db11c1df (123368 B) | aitest: tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencecommand=65 reconco ... st=177 checks=16/16 result=PASS; FAIL lines: 0 / lodstats: frames=301 median=3.184 p99=5.036 max=5.412 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |

## The conditions, met or not

| | condition (the task's wording, abridged) | this step's result | met |
|---|---|---|---|
| (a) | the M3-R2 gates hold: the Mission01 6,000-tick pairs (debug + release) byte-identical and `tools/oracle/mission_test.sh --ticks 6000 --perturb 3000 --copy-tick 4000` PASS; the stream IDENTICAL in all 21 fields incl. `rng_all` to `m3r1i-mission-a` (`1ae66610c3f024108a08e6ad61829aa6c3c52f59e0a53ce74be19d866a7f4758`), also with every setting flipped incl. this round's `texLinearFiltering 0` as row and switch, `/exeTrails` and every other producer-added setting; fe; all self-tests (fescreens `options=35` plus the round's control, strings 1705 / 1705, keys 197 / 149 + the F-key line, console `tablerows=486` with the round's rows, the toggle self-test under every switch incl. this round's); producer 2's paused dumps (every paused frame IDENTICAL); uiscale x9; the team colours; lodtest; the ai pair + the 4,000 soak; the skirmishes; missiontest, loadgame, warpstart, the lesson load, Tutorial1, the failed jump, the hwirec01 / 02 playback pairs; every quitting run rc 0; layout32; the generators `--check`; the dumps whole and crc-stripped vs M3-R2's with the crc moves matched to producer statements and the opt-outs EQUAL to R10's; every guard-rail stream vs M3-R2's over 21 fields with a table; the UBSan kinds vs M3-R2's nine | Mission01: `mission-a` = `mission-b` byte-identical (`c20ccda7fdae0f66`), the release pair `316025e5c47989e1`, MISSION TEST PASS in both (perturb at 3000 `rng10`, the flipped copy at 4000 `ships_health`); **29 variants all IDENTICAL in all 21 fields to `m3r1i-mission-a` and to `m3r2i-e-mission-a`** (the Mission01 table below): M3-R2's list and this round's `texLinearFiltering 0` alone (undone by the preset as in the exe) and under `effectsLevel 2`, `/sharpTextures`, `/noFilter`, `/exeTrails`, the console rows `engineTrails` and `texLinearFiltering` flipped live, F1 pressed twice; fe at Main_game_screen; **every self-test PASS** (fescreens `options=35` with the SHARP TEXTURES checkbox, strings 1705 / 1705, keys 197 / 149 + `overlay fkey F1 ... free=1`, console `tablerows=486 shown=38 set=31`, **toggle PASS under all ten switch sets**); the nine paused dumps hold every column (fence 2); uiscale x9 45 / 45; the team colours 7 / 7 and 6 / 6; lodtest on + `/lodBands` PASS; ai pair IDENTICAL + the soak IDENTICAL in 20 fields; the skirmishes, missiontest, loadgame, warp, tload, Tutorial1 12 / 12, fj3, play01 / 02 pairs all byte-identical and IDENTICAL to M3-R2's (the guard-rail table); every quitting run rc 0; layout32 OK; the generators current; **the four default dumps byte-equal to M3-R2's whole and crc-stripped**, the opt-outs byte-equal to R10's, the crc-only moves the two options (`motion-sharp` / `nis-sharp` `1a2227d966ec9712` / `871eac333fd603d0` whole, crc-stripped equal to `motion-on` / `nis-on`: 1,431 / 1,439 lines differ whole, 0 crc-stripped, byte-equal to producer 1's `m3r3-sk-motion-sharp` / `-nis-sharp`; `motion-exe` = `motion-on` and `nis-exe` = `nis-on` byte for byte (no missile corvette in either scene), `motion-exe` byte-equal to producer 5's `m3r3-et-motion-exe`) and the two `universePause` dumps' paused frames differ from producer 2's in the commit id of the build text drawn while paused alone (beside the verdict, 1); the UBSan kinds the same nine | **yes** |
| (b) | all 17 missions pairs (`--mission-test-ticks 90000 --timeout 3600`) vs M3-R2's hashes; Mission02's hint check and Mission03's mine checks in their lines; Mission17's credits | 17 / 17 PASS with every objective in order, all 17 pairs byte-identical, ASan 0 in the 34 runs, **every stream IDENTICAL in every field to `m3r2i-e-mN-a`** (the missions table); Mission02's line `hint skill=2 timer=360 kasstart=1 due=5762 found=1152 ... expected=no fired=no ... result=PASS`, Mission03's `level objects tick=1 mines=199 ... objectives=5/5 result=PASS`; Mission17's milestone `mission self-test: the game's end (credits)` | **yes** |
| (c) | `sp --timeout 1200` + fe and sp at 3840x2160, ASan 0 | `sp` reaches Mission12 at 1024x768 (12 levels, 126 golden-shot points, 23,331 file loads, 12 animatics skipped headless); `sp-4k` reaches Mission11 at 3840x2160 (11 levels, 119 golden-shot points, 22,327 file loads; `uiscale: physical 3840x2160 ... scale 200% (auto, uiScale 0) logical 1920x1080`); both rc 124 at 1,200 s by design, ASan 0, no unsuppressed leak, no `DEADLYSIGNAL`; `fe` and `fe-4k` Main_game_screen at 120 s, 300 file loads, the 4K one `uiscale: ... scale 200% ... logical 1920x1080` | **yes** |
| (d) | the rung unchanged: the replays 10-17 / 10-18 vs the GOG streams 2823 / 2871 (RUNG fields), libm 177 / 118, the release replay equal on all but `rng_all`; `ls original/oracle` for `hwirec*-play-*` and newer `oracle01-rec-*` | 10-17 `r17-a` / `r17-b` / `r17-rel` first divergent **2823 `ships_vel`**, `r17-libm` 177 `ships_vel`; 10-18 `r18-a` / `r18-b` **2871 `ships_vel`**, `r18-libm` 118 `ships_pos`; the exclusions `rng6` 161 (both), `real` 2 (10-18); `r17-rel` against `r17-a` `rng_all` at 6377 only, the 20 other fields IDENTICAL; every replay IDENTICAL in every field to `m3r2i-e-r17-a` / `-r18-a` (= M3-R1's), the hwirec01 / 02 playbacks to M3-R2's; no `original/oracle/hwirec*-play-*`; the newest `oracle01-rec-*` still `20261019-143530` (decision 48 open) | **yes** |
| (e) | the five fences rerun with numbers (settings / keys; the pause freeze; Mission02; the mines; the engine trails incl. the 4K cost on an idle GPU) | the next section: (1) F1 `differ=14400 bbox=2,2,227,66`, the toggle self-test PASS x10, the sharp captures 2,685 / 113,372 / 890,954 px, the cfg row alone 0 px, the live flip; (2) every paused pair holds every world column in the nine runs (the scene run's camera converges by one ulp: screen moves <= 0.0019 px), 7 of 9 dumps byte-equal to producer 2's, the two `universePause` dumps crc-only (the two `universePause` dumps' paused frames differ from producer 2's in the commit id of the build text drawn while paused alone (beside the verdict, 1)); (3) 486 Dust points in every sensors frame, the on / off captures 247-261 / 269-275 px, the hint at 5824 on NORMAL with the scout held and never on HARD; (4) the level mine `textures=11 bound=11 white=0`, the laid `4 / 4 / 0`, batched = immediate, every frame byte-equal to producer 4's, the Recon and Acolyte chases and the Mission03 log the producer's ticks; (5) the four ribbons byte-equal to producer 5's frames, the trail streams 13-17 equal at every frame, the 4K cost on the idle GPU after the correctness chains (the default (four ribbons) medians 3.743 / 3.422 ms (p99 5.643 / 4.886), `/exeTrails` 3.483 / 3.240 (p99 5.043 / 5.061): **the trails' 4K cost +0.221 ms on the median against a run-to-run spread of 0.321 ms**; `/meshImmediate` 8.096 / 8.003 (p99 11.699 / 13.076: the batching's gain stands), `/sharpTextures` 3.472 / 3.184 (p99 5.054 / 5.036: the same as the default within the spread)) | **yes** |
| (f) | `/bin/grep -rn -E 'CAT-(TODO\|PROVISIONAL)\([^)]*M3-R3' src/` empty (BTG.c:1151 re-targeted, comment-only, then rebuilt); the counts by owner vs M3-R2's 37 / 28 / 4; `HWI-FEATURE` per name; `HWI-FIX` vs 61; `HWI-TEST` vs 283; `CAT-FIX` vs 195; `tools/build.sh --debug` / `--release` from clean with 0 warnings; the Windows cross build with the zip's sha256 | the grep empty after the re-target of `src/Game/BTG.c:1151` to `CAT-TODO(M3-R4)`; 37 `CAT-TODO` lines (M3-R2: 37), 28 owner tags: stage 5 23 (plain 8, multiplayer options 8, LAN 6, multiplayer 1), M3 oracle physics 3, M3 soak 1, **M3-R4 1** (was M3-R3 1); 4 `CAT-PROVISIONAL` (4); `HWI-FEATURE` motion-interp 486 / letterbox-setting 37 / full-detail 179 / ui-scale 300 / **instrumentation 141** (102) / hyperspace-assembly 55 / mesh-batching 160 / **sharp-textures 196 / pause-freeze 90 / engine-trails 113** (new); **`HWI-FIX` 76** (the task's 61 and M3-R2's exit count 62: +14 `HWI-FIX (M3-R3 mines)` lines of `meshStaticTeamColor`); **`HWI-TEST` 461** (the task's 283 / M3-R2's 286); **`CAT-FIX` 196** (195: the live textures' re-filter); both builds from clean rc 0, `[274/274]`, 0 warnings, before and after the edit; the win64 zip `dist/Homeworld-Infection-win64-20261026-6ad69ec.zip` sha256 `3d41aaefccb560b2fe56d639a81bb9f834d17520515fe18838db6619b512c195` | **yes** |

**M3-R3 exit test: MET** on every condition (**presentation only** held: no simulation field of any stream of the round moved; the
Mission01 stream, the 17 missions, the guard rails and the replays IDENTICAL to M3-R2's, which are M3-R1's and R10's), with the
statements that belong beside the verdict:

1. **The two `universePause` dumps are not byte-equal to producer 2's final dumps** (`pz-univ-on`, `pz-univ-noi`): the 72 paused lines differ in `crc` and `crcv` alone, every object, eye, look and up column equal, the 1,431 un-paused lines byte-equal. Chain X attributed it: a rerun is byte-equal to this step's run (deterministic), the `/exeTrails` twin gives the same `crcv`, and the captured paused frame (`/catMotionShot 501`) of this step's binary against the mines-port build `4ae7d91`'s differs in **71 px at `bbox=980,756,1020,763`: the seven characters of the commit id in the build text `rndRenderTask` draws at the bottom right while `universePause || !gameIsRunning`** (src/SDL/render.c:5461-5469: `networkVersion` + `HOMEWORLD_BUILD_VERSION`, meson's `git describe`: `Emergence 1 6ad69ec` against `Emergence 1 4ae7d91` in the enlarged crops; the frame before the pause byte-equal across the builds, the paused frames equal to each other within each build). The three `crcv` values are the three commits (`292a6da` `f5b531de`: the producer's and the audit's; `4ae7d91` `2a49932e`; `6ad69ec` `411a110d`); the key pause draws no such text (its dumps byte-equal to the producer's, `crc` included). A build-identity text, not a render change (the same text is the front end's 283 px against producer 1's `scr-fe` / `scr-fe4k` captures at `c8f97bb`); the gate (every paused pair equal in every world column, the camera's own chase stated) holds in all nine runs.
2. **The 4K cost (fence 5) was measured after the correctness chains, on the idle GPU** (the GPU batch waited from 11:49 to 14:55 for the user's game to release the GPU (`gpuwait`: nvidia-smi every 10 s until 120 s at or under 40 %), then ran its eight runs on the idle GPU in two batches of four, the second after a two-minute wait on a 50 % sample; `claude-ping` fired 2 times (11:49:21, 14:57:12): the library pings once per `gpuwait` call, not once per gate, a deviation from the user's rule to fix in `chains/lib.sh` before M3-R4): the medians and the p99 are quoted with the load average of the batch; the user's game (a UE `GameThread` process at ~600 % CPU, the GPU at 49-86 %) held both through the correctness chains (load average 21-38), which is why no `--lodstats` number of the capture runs is a measurement.
3. **One self-test is FAIL by design**: `m02-hint-h` (Mission02 on HARD with the scout held, a fence run) ends `objectives=4/6 result=FAIL` at 90,000 ticks because the scripted player does not finish Mission02 on HARD (the SalvagePod), as producer 3 found and the self-tests play NORMAL; its hint check is `expected=no fired=no result=PASS` (no timer exists on HARD). Every other `result=FAIL` in the 225 runs is a tick budget by design (`m02-shot-on` / `-off` / `-exe` / `-4k-*` `objectives=1/6` at 6,400, the mine chases `0/5` / `1/5` at 1,400 / 2,600), as in the producers' runs of the same shape.

### The Mission01 stream, every setting flipped (condition (a))

| run (debug unless stated) | sha256 (size) | vs `m3r1i-mission-a` (`1ae66610c3f02410`), 21 fields | vs `m3r2i-e-mission-a` | the settings line's rows |
|---|---|---|---|---|
| Mission01 mission-a | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=none |
| Mission01 mission-b | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=none |
| Mission01 mission-noi | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=/noInterp |
| Mission01 mission-nonis | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=/noNISInterp |
| Mission01 mission-libm | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=none |
| Mission01 mission-lb | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=none |
| Mission01 mission-lbsw | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=/letterboxBehind |
| Mission01 mission-lodbands | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=/lodBands |
| Mission01 mission-nolod | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=none |
| Mission01 mission-uisw | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=/uiScale |
| Mission01 mission-uicfg | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=none |
| Mission01 mission-optout | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=/noInterp,/lodBands |
| Mission01 mission-ov | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=/overlay |
| Mission01 mission-fl | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=none |
| Mission01 mission-imm | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=/meshImmediate |
| Mission01 mission-nohsa | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=none |
| Mission01 mission-cfg0 | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=0 (cfg) engineTrails=1 (default) switches=none |
| Mission01 mission-sharpcfg | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=0 (cfg) engineTrails=1 (default) switches=none |
| Mission01 mission-sharpsw | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=0 (switch) engineTrails=1 (default) switches=/noFilter |
| Mission01 mission-nofilter | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=0 (switch) engineTrails=1 (default) switches=/noFilter |
| Mission01 mission-exetrails | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=0 (switch) switches=/exeTrails |
| Mission01 mission-trailrow | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=none |
| Mission01 mission-fkey | `c20ccda7fdae0f66` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=none |
| Mission01 mission-rel-a | `316025e5c47989e1` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=none |
| Mission01 mission-rel-b | `316025e5c47989e1` (1476370 B) | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference | texLinearFiltering=1 (default) engineTrails=1 (default) switches=none |
| Mission01 release vs this step's debug | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 release vs m3r2i-e-mission-rel-a | IDENTICAL: 6001 ticks and 0 packets compared (ticks 0-6000), no difference |
| Mission01 perturb vs a | FIRST DIVERGENT TICK 3000 (last equal tick 2999); 2 field(s) differ, likely origin: rng10 |
| Mission01 the settings lines of the flipped runs: |

    mission-cfg0: 2 lines; first: objInterpolate=1 (default) nisInterpolate=1 (default) letterboxBehind=0 (default) noLOD=1 (default) uiScale=0->100% (default, auto) overlay=0 (default) meshImmediate=0 (default) texLinearFiltering=0 (cfg) engineTrails=1 (default) fit=window (as requested (no desktop bounds)) 1024x768 switches=none [start]
    mission-sharpcfg: 1 lines; first: objInterpolate=1 (default) nisInterpolate=1 (default) letterboxBehind=0 (default) noLOD=1 (default) uiScale=0->100% (default, auto) overlay=0 (default) meshImmediate=0 (default) texLinearFiltering=0 (cfg) engineTrails=1 (default) fit=window (as requested (no desktop bounds)) 1024x768 switches=none [start]
    mission-sharpsw: 1 lines; first: objInterpolate=1 (default) nisInterpolate=1 (default) letterboxBehind=0 (default) noLOD=1 (default) uiScale=0->100% (default, auto) overlay=0 (default) meshImmediate=0 (default) texLinearFiltering=0 (switch) engineTrails=1 (default) fit=window (as requested (no desktop bounds)) 1024x768 switches=/noFilter [start]
    mission-nofilter: 1 lines; first: objInterpolate=1 (default) nisInterpolate=1 (default) letterboxBehind=0 (default) noLOD=1 (default) uiScale=0->100% (default, auto) overlay=0 (default) meshImmediate=0 (default) texLinearFiltering=0 (switch) engineTrails=1 (default) fit=window (as requested (no desktop bounds)) 1024x768 switches=/noFilter [start]
    mission-exetrails: 1 lines; first: objInterpolate=1 (default) nisInterpolate=1 (default) letterboxBehind=0 (default) noLOD=1 (default) uiScale=0->100% (default, auto) overlay=0 (default) meshImmediate=0 (default) texLinearFiltering=1 (default) engineTrails=0 (switch) fit=window (as requested (no desktop bounds)) 1024x768 switches=/exeTrails [start]
    mission-trailrow: 3 lines; first: objInterpolate=1 (default) nisInterpolate=1 (default) letterboxBehind=0 (default) noLOD=1 (default) uiScale=0->100% (default, auto) overlay=0 (default) meshImmediate=0 (default) texLinearFiltering=1 (default) engineTrails=1 (default) fit=window (as requested (no desktop bounds)) 1024x768 switches=none [start]
    mission-fkey: 5 lines; first: objInterpolate=1 (default) nisInterpolate=1 (default) letterboxBehind=0 (default) noLOD=1 (default) uiScale=0->100% (default, auto) overlay=0 (default) meshImmediate=0 (default) texLinearFiltering=1 (default) engineTrails=1 (default) fit=window (as requested (no desktop bounds)) 1024x768 switches=none [start]
    mission-optout: 1 lines; first: objInterpolate=0 (switch) nisInterpolate=0 (switch) letterboxBehind=1 (cfg) noLOD=0 (switch) uiScale=0->100% (default, auto) overlay=0 (default) meshImmediate=0 (default) texLinearFiltering=1 (default) engineTrails=1 (default) fit=window (as requested (no desktop bounds)) 1024x768 switches=/noInterp,/lodBands [start]

The settings lines of the flipped runs (the sources labelled as the switch-label rule says): `mission-cfg0` two lines,
`texLinearFiltering=0 (cfg) ... [start]` then `=1 (preset)` (the exe's HIGH preset undoes the row alone); `mission-sharpcfg` one line
`texLinearFiltering=0 (cfg)` (under `effectsLevel 2`); `mission-sharpsw` and `mission-nofilter` `texLinearFiltering=0 (switch) ...
switches=/noFilter` (the alias reported as the exe's switch); `mission-exetrails` `engineTrails=0 (switch) ... switches=/exeTrails`;
`mission-trailrow` three lines (`engineTrails=0 (console)` at 300, `=1 (console)` at 400); `mission-fkey` five lines (`overlay=1 (key)`
/ `=0 (key)` at 100 / 200, `texLinearFiltering=0 (console)` / `=1 (console)` at 300 / 400); `mission-optout` `objInterpolate=0 (switch)
nisInterpolate=0 (switch) letterboxBehind=1 (cfg) noLOD=0 (switch) ... switches=/noInterp,/lodBands`.

### The rung (condition (d))

| replay | this step | result |
|---|---|---|
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

| replay hashes: 10-17 a0d8acbfb7828cd8 (a), a0d8acbfb7828cd8 (b), libm 002abc7fa5caf08d, rel af8301aa516141f8; 10-18 eae711646d1b9e7d (a), eae711646d1b9e7d (b), libm 8644ae5a69eefa23; hwirec01 900128813f41a06b / 900128813f41a06b; hwirec02 246c77e55a7c685c / 246c77e55a7c685c |

### The guard-rail streams (conditions (a) and (d))

| guard rail (ticks) | reference hash | this step's hash | `hwo_compare.py`, 21 fields unless stated | the mode's line / note |
|---|---|---|---|---|
| ai, 400 (`ai-a` vs `m3r2i-e-ai-a`) | `d982c0dcae02869b` | `0aeceba584a6212a` (a = b) | IDENTICAL: 401 ticks and 0 packets compared (ticks 0-400), no difference | tick=398 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defencec |
| ai soak, 4,000 (20 fields, rng_all exempt: decision 84) (`ai-soak` vs `m3r2i-e-ai-soak`) | `5b53f300ce915205` | `de7b525ffd94302e` | IDENTICAL: 4001 ticks and 0 packets compared (ticks 0-4000), no difference | tick=3998 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defence |
| ai soak, rng_all (`ai-soak` vs `m3r2i-e-ai-soak`) | `5b53f300ce915205` | `de7b525ffd94302e` | FIRST DIVERGENT TICK 1316 (last equal tick 1315); 1 field(s) differ, likely origin: rng_all | tick=3998 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attackcommand=33 defence |
| skirmish LFE2, 600 (`skirmish-lfe2-a` vs `m3r2i-e-skirmish-lfe2-a`) | `a14774ffd6287143` | `bbcb4623ae8caff1` (a = b) | IDENTICAL: 601 ticks and 0 packets compared (ticks 0-600), no difference |  |
| skirmish Armed8, 600 (`skirmish-armed8` vs `m3r2i-e-skirmish-armed8`) | `c996a4b4a7314be7` | `4c3297929eddd609` | IDENTICAL: 601 ticks and 0 packets compared (ticks 0-600), no difference |  |
| build, 7,000 (`build-a` vs `m3r2i-e-build-a`) | `8e5128c9a09386a8` | `2c707048b2614a35` (a = b) | IDENTICAL: 7001 ticks and 0 packets compared (ticks 0-7000), no difference |  |
| missiontest (Mission01), NIS-first (`missiontest-a` vs `m3r2i-e-missiontest-a`) | `c14c549ceac7b34f` | `a59672e794c76827` (a = b) | IDENTICAL: 11426 ticks and 0 packets compared (ticks 0-11425), no difference | mission=1 ticks=11424 objectives=6/6 result=PASS; FAIL lines: 0 |
| loadgame (01 - Hiigara), 3,600 (`loadgame-a` vs `m3r2i-e-loadgame-a`) | `7aba43a5ae46030e` | `833b459a1e8d59d9` (a = b) | IDENTICAL: 3603 ticks and 0 packets compared (ticks 1984-5586), no difference | ticks=3600 from=1985 to=5585 mission=1 tutorial=2 ships=141 objectives=0/1 kasTeams=36 result=PASS |
| warp start (level 10) (`warp10-a` vs `m3r2i-e-warp10-a`) | `10d1fe5e169c3d0d` | `c444a75e9c3e796e` (a = b) | IDENTICAL: 101 ticks and 0 packets compared (ticks 0-100), no difference |  |
| Tutorial1 leg (`tutorial-a` vs `m3r2i-e-tutorial-a`) | `00fbc1aa76d477a2` | `983932d28cbd6594` (a = b) | IDENTICAL: 10446 ticks and 0 packets compared (ticks 0-10445), no difference | ticks=10444 tutorial kas=1 lesson1=2 objectives=2 buttons=1 prompts=7/102/158 pointer=1 next=1108 lesson2=1110 lessons=12/12 complete=12/12  |
| lesson load (LESSON 11), 3,600 (`tload-a` vs `m3r2i-e-tload-a`) | `ed33724271bd859a` | `f36d6abebb31db26` (a = b) | IDENTICAL: 3603 ticks and 0 packets compared (ticks 8360-11962), no difference | ticks=3600 from=8361 to=11961 mission=1 tutorial=1 ships=30 objectives=1/1 kasTeams=10 result=PASS |
| failed jump (Mission03) (`fj3-a` vs `m3r2i-e-fj3-a`) | `a3044330e4a63b5b` | `5c5ece5ace844989` (a = b) | IDENTICAL: 17669 ticks and 0 packets compared (ticks 0-17668), no difference | mission=3 ticks=17667 objectives=5/5 result=PASS; FAIL lines: 0 |
| replay 10-17 (`r17-a` vs `m3r2i-e-r17-a`) | `c3b7d873ae8fb692` | `a0d8acbfb7828cd8` (a = b) | IDENTICAL: 9601 ticks and 4800 packets compared (ticks 1-9601), no difference |  |
| replay 10-17 /oracleLibmSqrt (`r17-libm` vs `m3r2i-e-r17-libm`) | `3823b695ea8daefd` | `002abc7fa5caf08d` | IDENTICAL: 9601 ticks and 4800 packets compared (ticks 1-9601), no difference |  |
| replay 10-17 release (`r17-rel` vs `m3r2i-e-r17-rel`) | `567026572a780e2e` | `af8301aa516141f8` | IDENTICAL: 9601 ticks and 4800 packets compared (ticks 1-9601), no difference |  |
| replay 10-18 (`r18-a` vs `m3r2i-e-r18-a`) | `bd2c316848aff30c` | `eae711646d1b9e7d` (a = b) | IDENTICAL: 3527 ticks and 1763 packets compared (ticks 1-3527), no difference |  |
| replay 10-18 /oracleLibmSqrt (`r18-libm` vs `m3r2i-e-r18-libm`) | `b63b7226f8f7c6a3` | `8644ae5a69eefa23` | IDENTICAL: 3527 ticks and 1763 packets compared (ticks 1-3527), no difference |  |
| hwirec01 playback (Armed8) (`play01-a` vs `m3r2i-e-play01-a`) | `dd4d8cf27ebae42d` | `900128813f41a06b` (a = b) | IDENTICAL: 4801 ticks and 2400 packets compared (ticks 1-4801), no difference |  |
| hwirec02 playback (LFE2) (`play02-a` vs `m3r2i-e-play02-a`) | `7242658d8c88dc30` | `246c77e55a7c685c` (a = b) | IDENTICAL: 4801 ticks and 2400 packets compared (ticks 1-4801), no difference |  |
| oracle self-test (debug) (`oracle` vs `m3r2i-e-oracle`) | `3456d0f9a434bc0b` | `bf25e677ccbaa887` | IDENTICAL: 513 ticks and 256 packets compared (ticks 0-512), no difference |  |
| oracle self-test (release) (`oracle-rel-a` vs `m3r2i-e-oracle-rel-a`) | `ba704bca2b4d576b` | `e86cf80daddf15c2` (a = b) | IDENTICAL: 513 ticks and 256 packets compared (ticks 0-512), no difference |  |
| team colours, the user's scheme (missiontest into Mission02) (`tc-user` vs `m3r2i-e-tc-user`) | `0ee8c83b4d7bbc4d` | `cc25f0b882e7945c` | IDENTICAL: 11521 ticks and 0 packets compared (ticks 0-11520), no difference | checks=7 failed=0 afterLoads=3 levels=Mission01_NIS01.mission:after,Mission01_NIS01.mission:level,Mission01.mission:after,Mission01.mission: |
| team colours, red (missiontest into Mission02) (`tc-red` vs `m3r2i-e-tc-red`) | `0ee8c83b4d7bbc4d` | `cc25f0b882e7945c` | IDENTICAL: 11521 ticks and 0 packets compared (ticks 0-11520), no difference | checks=7 failed=0 afterLoads=3 levels=Mission01_NIS01.mission:after,Mission01_NIS01.mission:level,Mission01.mission:after,Mission01.mission: |
| team colours Mission03 into Mission04 (`tc-m3` vs `m3r2i-e-tc-m3`) | `df68de4e18348963` | `18b9047d39eda0d4` | IDENTICAL: 17794 ticks and 0 packets compared (ticks 0-17793), no difference | checks=6 failed=0 afterLoads=3 levels=Mission03.mission:after,Mission03.mission:level,Mission04_NIS02.mission:after,Mission04_NIS02.mission: |
| lodtest ai on (`lod-ai-on` vs `m3r2i-e-lod-ai-on`) | `0aa609f3d5fd1591` | `b093aed2c0dc08cd` | IDENTICAL: 401 ticks and 0 packets compared (ticks 0-400), no difference | tick=300 fullDetail=1 objects=164 drawnAbove0=0 bandAbove0=164 result=PASS |
| lodtest ai /lodBands (`lod-ai-bands` vs `m3r2i-e-lod-ai-bands`) | `0aa609f3d5fd1591` | `b093aed2c0dc08cd` | IDENTICAL: 401 ticks and 0 packets compared (ticks 0-400), no difference | tick=300 fullDetail=0 objects=164 drawnAbove0=164 bandAbove0=164 result=PASS |
| perf 4K batched (release ai 500 --lodstats; = M3-R2's perf4k-bat1) (`perf-on1` vs `m3r2i-e-perf4k-bat1`) | `2d615f67ed8009bb` | `0f768355db11c1df` | IDENTICAL: 501 ticks and 0 packets compared (ticks 0-500), no difference |  |
| perf 4K /exeTrails vs this step's default (`perf-exe1` vs `m3r3i-e-perf-on1`) | `0f768355db11c1df` | `0f768355db11c1df` | IDENTICAL: 501 ticks and 0 packets compared (ticks 0-500), no difference |  |
| perf 4K /meshImmediate vs the default (`perf-imm1` vs `m3r3i-e-perf-on1`) | `0f768355db11c1df` | `0f768355db11c1df` | IDENTICAL: 501 ticks and 0 packets compared (ticks 0-500), no difference |  |
| perf 4K /sharpTextures vs the default (`perf-sw1` vs `m3r3i-e-perf-on1`) | `0f768355db11c1df` | `0f768355db11c1df` | IDENTICAL: 501 ticks and 0 packets compared (ticks 0-500), no difference |  |
| perf 4K second default run vs the first (`perf-on2` vs `m3r3i-e-perf-on1`) | `0f768355db11c1df` | `0f768355db11c1df` | IDENTICAL: 501 ticks and 0 packets compared (ticks 0-500), no difference |  |
| F1 key pressed at 150 / 200 (ai 260) vs the same run without the key (`sk-fkey` vs `m3r3i-e-sk-fkey-off`) | `d0500d175717c1aa` | `d0500d175717c1aa` | IDENTICAL: 261 ticks and 0 packets compared (ticks 0-260), no difference |  |
| F1 key run vs the plain ai run (260 vs 400 ticks) (`sk-fkey` vs `m3r3i-e-ai-a`) | `0aeceba584a6212a` | `d0500d175717c1aa` | IDENTICAL: 260 ticks and 0 packets compared (ticks 0-259), no difference |  |
| sharp capture, linear (ai 260 --lodstats 150-230) vs M3-R2's overlay-off capture (240) (`sk-cap-lin` vs `m3r2i-e-ov-ai-off`) | `e43b7db0b1a89218` | `7fe217472dab4ddc` | IDENTICAL: 240 ticks and 0 packets compared (ticks 0-239), no difference |  |
| sharp: /sharpTextures vs linear (`sk-cap-sw` vs `m3r3i-e-sk-cap-lin`) | `7fe217472dab4ddc` | `7fe217472dab4ddc` | IDENTICAL: 261 ticks and 0 packets compared (ticks 0-260), no difference |  |
| sharp: the cfg row under CUSTOM (effectsLevel 2) vs linear (`sk-cap-cfg` vs `m3r3i-e-sk-cap-lin`) | `7fe217472dab4ddc` | `7fe217472dab4ddc` | IDENTICAL: 261 ticks and 0 packets compared (ticks 0-260), no difference |  |
| sharp: the cfg row alone (the exe's preset undoes it) vs linear (`sk-cap-cfgh` vs `m3r3i-e-sk-cap-lin`) | `7fe217472dab4ddc` | `7fe217472dab4ddc` | IDENTICAL: 261 ticks and 0 packets compared (ticks 0-260), no difference |  |
| sharp: the live console flip at 180 / 220 vs linear (`sk-cap-live` vs `m3r3i-e-sk-cap-lin`) | `7fe217472dab4ddc` | `7fe217472dab4ddc` | IDENTICAL: 261 ticks and 0 packets compared (ticks 0-260), no difference |  |
| sharp: 4K ai capture sharp vs linear (`sk-cap4k-sw` vs `m3r3i-e-sk-cap4k-lin`) | `7fe217472dab4ddc` | `7fe217472dab4ddc` | IDENTICAL: 261 ticks and 0 packets compared (ticks 0-260), no difference |  |
| sharp: 4K ai capture linear vs the 1024x768 one (`sk-cap4k-lin` vs `m3r3i-e-sk-cap-lin`) | `7fe217472dab4ddc` | `7fe217472dab4ddc` | IDENTICAL: 261 ticks and 0 packets compared (ticks 0-260), no difference |  |
| Kuun-Lan zoom (skirmish 120, --scene mothership:2300): sharp vs linear (`sk-ku-sw` vs `m3r3i-e-sk-ku-lin`) | `54a7a36160d91654` | `54a7a36160d91654` | IDENTICAL: 121 ticks and 0 packets compared (ticks 0-120), no difference |  |
| Kuun-Lan zoom vs the plain skirmish (120 of 600 ticks) (`sk-ku-lin` vs `m3r3i-e-skirmish-lfe2-a`) | `bbcb4623ae8caff1` | `54a7a36160d91654` | FIRST DIVERGENT TICK 2 (last equal tick 1); 2 field(s) differ, likely origin: rng0 |  |
| Kuun-Lan zoom vs the plain skirmish, 19 fields (rng0 + rng_all exempt): the scene gives its orders (M3-R1's move / formation / dock / harvest), a different game from the first order on, by design (`sk-ku-lin` vs `m3r3i-e-skirmish-lfe2-a`) | `bbcb4623ae8caff1` | `54a7a36160d91654` | FIRST DIVERGENT TICK 48 (last equal tick 47); 1 field(s) differ, likely origin: ships_misc |  |
| Kuun-Lan zoom 4K: sharp vs linear (`sk-ku4k-sw` vs `m3r3i-e-sk-ku4k-lin`) | `54a7a36160d91654` | `54a7a36160d91654` | IDENTICAL: 121 ticks and 0 packets compared (ticks 0-120), no difference |  |
| Kuun-Lan zoom 4K vs 1024x768 (`sk-ku4k-lin` vs `m3r3i-e-sk-ku-lin`) | `54a7a36160d91654` | `54a7a36160d91654` | IDENTICAL: 121 ticks and 0 packets compared (ticks 0-120), no difference |  |
| Mission01 4K --lodtest 100:fleet (320): sharp vs linear (`sk-m14k-sw` vs `m3r3i-e-sk-m14k-lin`) | `e1168b094eddfef5` | `e1168b094eddfef5` | IDENTICAL: 321 ticks and 0 packets compared (ticks 0-320), no difference |  |
| Mission01 4K lodtest run vs the Mission01 stream (320 of 6,000) (`sk-m14k-lin` vs `m3r3i-e-mission-a`) | `c20ccda7fdae0f66` | `e1168b094eddfef5` | IDENTICAL: 320 ticks and 0 packets compared (ticks 0-319), no difference |  |
| Video options screen in game (mission 140) vs the Mission01 stream (`sk-scr-ig` vs `m3r3i-e-mission-a`) | `c20ccda7fdae0f66` | `da8363760ee140fe` | IDENTICAL: 140 ticks and 0 packets compared (ticks 0-139), no difference |  |
| pause: the user's recipe (skirmish 260, --scene follow:1500:500:200, P after F700) vs the plain skirmish (`pz-scene-key` vs `m3r3i-e-skirmish-lfe2-a`) | `bbcb4623ae8caff1` | `23c3c9d8b1b71a2e` | FIRST DIVERGENT TICK 2 (last equal tick 1); 2 field(s) differ, likely origin: rng0 |  |
| pause: the scene run vs the plain skirmish, 19 fields (rng0 + rng_all exempt): the scene's orders make it a different game from the first order on, by design (`pz-scene-key` vs `m3r3i-e-skirmish-lfe2-a`) | `bbcb4623ae8caff1` | `23c3c9d8b1b71a2e` | FIRST DIVERGENT TICK 48 (last equal tick 47); 1 field(s) differ, likely origin: ships_misc |  |
| pause: the scene run vs producer 2's final (m3r3-pf-fix2-scene-key) (`pz-scene-key` vs `m3r3-pf-fix2-scene-key`) | `c519fc1d0d5f812d` | `23c3c9d8b1b71a2e` | IDENTICAL: 261 ticks and 0 packets compared (ticks 0-260), no difference |  |
| Mission02 captures: dust on vs off (`ShowAsteriod0 0`), 6,400 ticks, the scout held (`m02-shot-on` vs `m3r3i-e-m02-shot-off`) | `0d1fb43232ce1565` | `0d1fb43232ce1565` | IDENTICAL: 6400 ticks and 0 packets compared (ticks 0-6399), no difference |  |
| Mission02 capture: 4K on vs 1024x768 on (`m02-shot4k-on` vs `m3r3i-e-m02-shot-on`) | `0d1fb43232ce1565` | `7dd7c389d7a38248` | FIRST DIVERGENT TICK 2271 (last equal tick 2270); 1 field(s) differ, likely origin: rng_all |  |
| Mission02 capture: 4K on vs 1024x768 on, 20 fields (rng_all exempt) (`m02-shot4k-on` vs `m3r3i-e-m02-shot-on`) | `0d1fb43232ce1565` | `7dd7c389d7a38248` | IDENTICAL: 6400 ticks and 0 packets compared (ticks 0-6399), no difference |  |
| Mission02 capture: 4K off vs 4K on (`m02-shot4k-off` vs `m3r3i-e-m02-shot4k-on`) | `7dd7c389d7a38248` | `7dd7c389d7a38248` | IDENTICAL: 6400 ticks and 0 packets compared (ticks 0-6399), no difference |  |
| Mission02 capture vs producer 3's m3r3-m02-shot-on (`m02-shot-on` vs `m3r3-m02-shot-on`) | `03beac7d5b9d22fb` | `0d1fb43232ce1565` | IDENTICAL: 6400 ticks and 0 packets compared (ticks 0-6399), no difference |  |
| Mission02 capture (held scout) vs the gate run m2-a: the hold is a different game from the scout's first order (`m02-shot-on` vs `m3r3i-e-m2-a`) | `b6393960ef8abba0` | `0d1fb43232ce1565` | FIRST DIVERGENT TICK 328 (last equal tick 327); 5 field(s) differ, likely origin: ships_misc |  |
| Mission02 hint NORMAL, scout held to 6400 vs producer 3's (`m02-hint-n` vs `m3r3-m02-hint-n`) | `521c972bd8cc6191` | `1ff0b886c95b78c5` | IDENTICAL: 29505 ticks and 0 packets compared (ticks 0-29504), no difference | mission=2 ticks=29536 objectives=6/6 result=PASS; FAIL lines: 0 |
| Mission02 hint VERY EASY vs producer 3's (`m02-hint-e` vs `m3r3-m02-hint-e`) | `0f63de20ad98fbc5` | `788a0779fa27131e` | IDENTICAL: 25089 ticks and 0 packets compared (ticks 0-25088), no difference | mission=2 ticks=25120 objectives=6/6 result=PASS; FAIL lines: 0 |
| Mission02 hint HARD, scout held vs producer 3's (`m02-hint-h` vs `m3r3-m02-hint-h`) | `cbb8fc70c974b19b` | `33d63823957df538` | IDENTICAL: 90000 ticks and 0 packets compared (ticks 0-89999), no difference | mission=2 ticks=89998 objectives=4/6 result=FAIL; FAIL lines: 0 |
| Mission02 hint VERY EASY vs the gate run m2-a (NORMAL) (`m02-hint-e` vs `m3r3i-e-m2-a`) | `b6393960ef8abba0` | `788a0779fa27131e` | FIRST DIVERGENT TICK 1 (last equal tick 0); 3 field(s) differ, likely origin: rng10 |  |
| mines: the level-mine capture run (--shot, /catMineView: camera only) vs Mission03 m3-a (`mn-shot-lvl` vs `m3r3i-e-m3-a`) | `177f18624d530736` | `4b14ef8162b0ec20` | FIRST DIVERGENT TICK 2569 (last equal tick 2568); 1 field(s) differ, likely origin: rng_all |  |
| mines: the level-mine capture run vs m3-a, 20 fields (rng_all exempt) (`mn-shot-lvl` vs `m3r3i-e-m3-a`) | `177f18624d530736` | `4b14ef8162b0ec20` | IDENTICAL: 17794 ticks and 0 packets compared (ticks 0-17793), no difference |  |
| mines: the laid-mine capture run vs m3-a (`mn-shot-drop` vs `m3r3i-e-m3-a`) | `177f18624d530736` | `a480c56c308c26cd` | FIRST DIVERGENT TICK 4833 (last equal tick 4832); 1 field(s) differ, likely origin: rng_all |  |
| mines: the laid-mine capture run vs m3-a, 20 fields (rng_all exempt) (`mn-shot-drop` vs `m3r3i-e-m3-a`) | `177f18624d530736` | `a480c56c308c26cd` | IDENTICAL: 17794 ticks and 0 packets compared (ticks 0-17793), no difference |  |
| mines: /meshImmediate twin (level) vs the batched (`mn-shot-lvl-imm` vs `m3r3i-e-mn-shot-lvl`) | `4b14ef8162b0ec20` | `4b14ef8162b0ec20` | IDENTICAL: 17794 ticks and 0 packets compared (ticks 0-17793), no difference |  |
| mines: /meshImmediate twin (laid) vs the batched (`mn-shot-drop-imm` vs `m3r3i-e-mn-shot-drop`) | `a480c56c308c26cd` | `a480c56c308c26cd` | IDENTICAL: 17794 ticks and 0 packets compared (ticks 0-17793), no difference |  |
| mines: the per-tick log run (/catMineLog 480) vs m3-a (`mn-ailog` vs `m3r3i-e-m3-a`) | `177f18624d530736` | `177f18624d530736` | IDENTICAL: 17794 ticks and 0 packets compared (ticks 0-17793), no difference |  |
| mines: the missile texture-log run vs m3-a (`mn-missile` vs `m3r3i-e-m3-a`) | `177f18624d530736` | `177f18624d530736` | IDENTICAL: 17794 ticks and 0 packets compared (ticks 0-17793), no difference |  |
| mines: the Recon chase (a test scene) vs producer 4's (`mn-chase-recon` vs `m3r3-mines-chase-recon`) | `cd563d07ebb8be75` | `50c3afe133e5a399` | IDENTICAL: 1400 ticks and 0 packets compared (ticks 0-1399), no difference |  |
| mines: the Acolyte chase vs producer 4's (`mn-chase-acolyte` vs `m3r3-mines-chase-acolyte`) | `3d31cce1a71d0911` | `1da6957e444e78c4` | IDENTICAL: 2600 ticks and 0 packets compared (ticks 0-2599), no difference |  |
| mines: the level capture vs producer 4's final (m3r3-mines-shot-lvl-after) (`mn-shot-lvl` vs `m3r3-mines-shot-lvl-after`) | `04e8784f3cef24fa` | `4b14ef8162b0ec20` | IDENTICAL: 17794 ticks and 0 packets compared (ticks 0-17793), no difference |  |
| trails: the ai capture (420, /catTrailView) default vs /exeTrails (`et-c-on` vs `m3r3i-e-et-c-exe`) | `223beb10ad173f61` | `657295031352f9cf` | FIRST DIVERGENT TICK 246 (last equal tick 245); 1 field(s) differ, likely origin: rng_all |  |
| trails: the ai capture default vs /exeTrails, 20 fields (rng_all exempt) (`et-c-on` vs `m3r3i-e-et-c-exe`) | `223beb10ad173f61` | `657295031352f9cf` | IDENTICAL: 421 ticks and 0 packets compared (ticks 0-420), no difference |  |
| trails: the ai capture vs the plain ai run (400 of 420 ticks), 21 fields (`et-c-on` vs `m3r3i-e-ai-a`) | `0aeceba584a6212a` | `657295031352f9cf` | FIRST DIVERGENT TICK 101 (last equal tick 100); 2 field(s) differ, likely origin: rng0 |  |
| trails: the same, 20 fields (rng_all exempt) (`et-c-on` vs `m3r3i-e-ai-a`) | `0aeceba584a6212a` | `657295031352f9cf` | FIRST DIVERGENT TICK 101 (last equal tick 100); 1 field(s) differ, likely origin: rng0 |  |
| trails: the same, 19 fields (rng0 + rng_all exempt: the camera moves at tick 100) (`et-c-on` vs `m3r3i-e-ai-a`) | `0aeceba584a6212a` | `657295031352f9cf` | IDENTICAL: 400 ticks and 0 packets compared (ticks 0-399), no difference |  |
| trails: the random-stream log runs, default vs /exeTrails (`et-rs-on` vs `m3r3i-e-et-rs-exe`) | `827805c5727cb15a` | `2275e0ccaf49f5f2` | FIRST DIVERGENT TICK 246 (last equal tick 245); 1 field(s) differ, likely origin: rng_all |  |
| trails: the random-stream log runs, default vs /exeTrails, 20 fields (rng_all exempt) (`et-rs-on` vs `m3r3i-e-et-rs-exe`) | `827805c5727cb15a` | `2275e0ccaf49f5f2` | IDENTICAL: 421 ticks and 0 packets compared (ticks 0-420), no difference |  |
| trails: rs-on vs producer 5's m3r3-et-rs-on (`et-rs-on` vs `m3r3-et-rs-on`) | `acd49d2f6f3ffb63` | `2275e0ccaf49f5f2` | FIRST DIVERGENT TICK 255 (last equal tick 254); 1 field(s) differ, likely origin: rng_all |  |
| trails: rs-on vs producer 5's m3r3-et-rs-on, 20 fields (rng_all exempt) (`et-rs-on` vs `m3r3-et-rs-on`) | `acd49d2f6f3ffb63` | `2275e0ccaf49f5f2` | IDENTICAL: 421 ticks and 0 packets compared (ticks 0-420), no difference |  |
| trails: the close capture (d250) default vs /exeTrails (`et-b-on` vs `m3r3i-e-et-b-exe`) | `877062f687468e51` | `f80c1d67de7828e6` | FIRST DIVERGENT TICK 216 (last equal tick 215); 1 field(s) differ, likely origin: rng_all |  |
| trails: the close capture (d250) default vs /exeTrails, 20 fields (rng_all exempt) (`et-b-on` vs `m3r3i-e-et-b-exe`) | `877062f687468e51` | `f80c1d67de7828e6` | IDENTICAL: 421 ticks and 0 packets compared (ticks 0-420), no difference |  |
| trails: the 4K ai capture default vs /exeTrails (`et-c4k-on` vs `m3r3i-e-et-c4k-exe`) | `fbb4f3269d0579d6` | `689a035b49991fff` | FIRST DIVERGENT TICK 187 (last equal tick 186); 1 field(s) differ, likely origin: rng_all |  |
| trails: the 4K ai capture default vs /exeTrails, 20 fields (rng_all exempt) (`et-c4k-on` vs `m3r3i-e-et-c4k-exe`) | `fbb4f3269d0579d6` | `689a035b49991fff` | IDENTICAL: 421 ticks and 0 packets compared (ticks 0-420), no difference |  |
| trails: P1 corvette under way (Mission03 260) default vs /exeTrails (`et-p1mv-on` vs `m3r3i-e-et-p1mv-exe`) | `d2903fdb11dd70b2` | `d2903fdb11dd70b2` | IDENTICAL: 260 ticks and 0 packets compared (ticks 0-259), no difference |  |
| trails: P1 under way vs producer 5's (`et-p1mv-on` vs `m3r3-et-p1mv-on`) | `60dc25beb3d943a8` | `d2903fdb11dd70b2` | IDENTICAL: 260 ticks and 0 packets compared (ticks 0-259), no difference |  |
| trails: P1 at rest vs Mission03 m3-a (260 ticks) (`et-p1rest` vs `m3r3i-e-m3-a`) | `177f18624d530736` | `53f4cffbb62b5d9a` | IDENTICAL: 260 ticks and 0 packets compared (ticks 0-259), no difference |  |
| trails: P1 at rest 4K vs 1024x768 (`et-p1rest4k` vs `m3r3i-e-et-p1rest`) | `53f4cffbb62b5d9a` | `53f4cffbb62b5d9a` | IDENTICAL: 260 ticks and 0 packets compared (ticks 0-259), no difference |  |
| trails: P1 under way 4K (200) vs 1024x768 (260) (`et-p1mv4k-on` vs `m3r3i-e-et-p1mv-on`) | `d2903fdb11dd70b2` | `bcbcdfdd4864a8b2` | IDENTICAL: 200 ticks and 0 packets compared (ticks 0-199), no difference |  |

### Missions 01-17 (condition (b))

| mission | the self-test line | hash (a) | pair (b) | ASan a / b | against M3-R2 (`m3r2i-e-mN-a`) | milestone |
|---|---|---|---|---|---|---|
| 1 | mission=1 ticks=11424 objectives=6/6 result=PASS; FAIL lines: 0 | `a59672e794c76827` (3246588 B) | byte-identical (a59672e794c76827) | ASan errors: 0 / 0 | vs `m3r2i-e-missiontest-a`: IDENTICAL: 11426 ticks and 0 packets compared (ticks 0-11425), no difference | mission self-test done |
| 2 | mission=2 ticks=25936 objectives=6/6 result=PASS; FAIL lines: 0 | `b6393960ef8abba0` (6381116 B) | byte-identical (b6393960ef8abba0) | ASan errors: 0 / 0 | vs `m3r2i-e-m2-a`: IDENTICAL: 25905 ticks and 0 packets compared (ticks 0-25904), no difference | mission self-test done |
| 3 | mission=3 ticks=17825 objectives=5/5 result=PASS; FAIL lines: 0 | `177f18624d530736` (4385810 B) | byte-identical (177f18624d530736) | ASan errors: 0 / 0 | vs `m3r2i-e-m3-a`: IDENTICAL: 17794 ticks and 0 packets compared (ticks 0-17793), no difference | mission self-test done |
| 4 | mission=4 ticks=20818 objectives=5/5 result=PASS; FAIL lines: 0 | `b2df80cb273139f4` (5122089 B) | byte-identical (b2df80cb273139f4) | ASan errors: 0 / 0 | vs `m3r2i-e-m4-a`: IDENTICAL: 20787 ticks and 0 packets compared (ticks 0-20786), no difference | mission self-test done |
| 5 | mission=5 ticks=16384 objectives=6/6 result=PASS; FAIL lines: 0 | `23626d660659b53f` (4031324 B) | byte-identical (23626d660659b53f) | ASan errors: 0 / 0 | vs `m3r2i-e-m5-a`: IDENTICAL: 16353 ticks and 0 packets compared (ticks 0-16352), no difference | mission self-test done |
| 6 | mission=6 ticks=31011 objectives=7/7 result=PASS; FAIL lines: 0 | `8c17835f5f3895e0` (7629566 B) | byte-identical (8c17835f5f3895e0) | ASan errors: 0 / 0 | vs `m3r2i-e-m6-a`: IDENTICAL: 30980 ticks and 0 packets compared (ticks 0-30979), no difference | mission self-test done |
| 7 | mission=7 ticks=16592 objectives=4/4 result=PASS; FAIL lines: 0 | `dbd168ab702b8c91` (4082492 B) | byte-identical (dbd168ab702b8c91) | ASan errors: 0 / 0 | vs `m3r2i-e-m7-a`: IDENTICAL: 16561 ticks and 0 packets compared (ticks 0-16560), no difference | mission self-test done |
| 8 | mission=8 ticks=29361 objectives=6/6 result=PASS; FAIL lines: 0 | `efee0966b00cc71e` (7223667 B) | byte-identical (efee0966b00cc71e) | ASan errors: 0 / 0 | vs `m3r2i-e-m8-a`: IDENTICAL: 29330 ticks and 0 packets compared (ticks 0-29329), no difference | mission self-test done |
| 9 | mission=9 ticks=20617 objectives=6/6 result=PASS; FAIL lines: 0 | `4e2b839f9ce6cf73` (5072642 B) | byte-identical (4e2b839f9ce6cf73) | ASan errors: 0 / 0 | vs `m3r2i-e-m9-a`: IDENTICAL: 20586 ticks and 0 packets compared (ticks 0-20585), no difference | mission self-test done |
| 10 | mission=10 ticks=21939 objectives=5/5 result=PASS; FAIL lines: 0 | `8887b807b263dae2` (5397854 B) | byte-identical (8887b807b263dae2) | ASan errors: 0 / 0 | vs `m3r2i-e-m10-a`: IDENTICAL: 21908 ticks and 0 packets compared (ticks 0-21907), no difference | mission self-test done |
| 11 | mission=11 ticks=8275 objectives=5/5 result=PASS; FAIL lines: 0 | `d19814ded54871e0` (2036509 B) | byte-identical (d19814ded54871e0) | ASan errors: 0 / 0 | vs `m3r2i-e-m11-a`: IDENTICAL: 8244 ticks and 0 packets compared (ticks 0-8243), no difference | mission self-test done |
| 12 | mission=12 ticks=10995 objectives=3/3 result=PASS; FAIL lines: 0 | `b719d46f084c859a` (2705630 B) | byte-identical (b719d46f084c859a) | ASan errors: 0 / 0 | vs `m3r2i-e-m12-a`: IDENTICAL: 10964 ticks and 0 packets compared (ticks 0-10963), no difference | mission self-test done |
| 13 | mission=13 ticks=19283 objectives=3/3 result=PASS; FAIL lines: 0 | `0faaf6710a8b833d` (4744478 B) | byte-identical (0faaf6710a8b833d) | ASan errors: 0 / 0 | vs `m3r2i-e-m13-a`: IDENTICAL: 19252 ticks and 0 packets compared (ticks 0-19251), no difference | mission self-test done |
| 14 | mission=14 ticks=22338 objectives=4/4 result=PASS; FAIL lines: 0 | `c05153b6a0dc960f` (5496008 B) | byte-identical (c05153b6a0dc960f) | ASan errors: 0 / 0 | vs `m3r2i-e-m14-a`: IDENTICAL: 22307 ticks and 0 packets compared (ticks 0-22306), no difference | mission self-test done |
| 15 | mission=15 ticks=15424 objectives=3/3 result=PASS; FAIL lines: 0 | `2d754826d33ab4c5` (3795164 B) | byte-identical (2d754826d33ab4c5) | ASan errors: 0 / 0 | vs `m3r2i-e-m15-a`: IDENTICAL: 15393 ticks and 0 packets compared (ticks 0-15392), no difference | mission self-test done |
| 16 | mission=16 ticks=21523 objectives=6/6 result=PASS; FAIL lines: 0 | `b6549e9b7d380558` (5295518 B) | byte-identical (b6549e9b7d380558) | ASan errors: 0 / 0 | vs `m3r2i-e-m16-a`: IDENTICAL: 21492 ticks and 0 packets compared (ticks 0-21491), no difference | mission self-test done |
| 17 | mission=17 ticks=15584 objectives=4/4 result=PASS; FAIL lines: 0 | `6eec1524e012e870` (3834034 B) | byte-identical (6eec1524e012e870) | ASan errors: 0 / 0 | vs `m3r2i-e-m17-a`: IDENTICAL: 15585 ticks and 0 packets compared (ticks 0-15584), no difference | mission self-test: the game's end (credits) |

### The dumps (condition (a); decisions 46 / 118 / 129)

| dump (m3r3i-e-*) | M3-R2's m3r2i-e-* whole / crc-stripped | this step's whole / crc-stripped | verdict | lines |
| motion-noi | c503c869f0dc9cdb / 3016280e9e0dd3c8 | c503c869f0dc9cdb / 3016280e9e0dd3c8 | byte-equal to m3r2i-e | 1436 lines |
| motion-on | c0e411662fa2f527 / 428ef41479eb5749 | c0e411662fa2f527 / 428ef41479eb5749 | byte-equal to m3r2i-e | 1436 lines |
| motion-optout | 183681235a3fdc7b / 3016280e9e0dd3c8 | 183681235a3fdc7b / 3016280e9e0dd3c8 | byte-equal to m3r2i-e | 1436 lines |
| motion-on-imm | c0e411662fa2f527 / 428ef41479eb5749 | c0e411662fa2f527 / 428ef41479eb5749 | byte-equal to m3r2i-e | 1436 lines |
| nis-noi | 86cb155acffc180e / 196bab32d244f614 | 86cb155acffc180e / 196bab32d244f614 | byte-equal to m3r2i-e | 1489 lines |
| nis-on | 0307f3be2044c29c / a5de13a44b8d264f | 0307f3be2044c29c / a5de13a44b8d264f | byte-equal to m3r2i-e | 1489 lines |
| nis-optout | 13ca70eea998e5de / 196bab32d244f614 | 13ca70eea998e5de / 196bab32d244f614 | byte-equal to m3r2i-e | 1489 lines |
| nis-on-imm | 0307f3be2044c29c / a5de13a44b8d264f | 0307f3be2044c29c / a5de13a44b8d264f | byte-equal to m3r2i-e | 1489 lines |

this round's variants against the step's own defaults (crc-only moves expected: producer 1's sharp run, producer 5's trail frames):
| motion-sharp vs motion-on | 1a2227d966ec9712 / 428ef41479eb5749 | whole: 1431 lines differ; crc-stripped: 0 lines differ | 1436 lines |
| motion-exe vs motion-on | c0e411662fa2f527 / 428ef41479eb5749 | whole: 0 lines differ; crc-stripped: 0 lines differ | 1436 lines |
| nis-sharp vs nis-on | 871eac333fd603d0 / a5de13a44b8d264f | whole: 1439 lines differ; crc-stripped: 0 lines differ | 1489 lines |
| nis-exe vs nis-on | 0307f3be2044c29c / a5de13a44b8d264f | whole: 0 lines differ; crc-stripped: 0 lines differ | 1489 lines |

opt-outs vs R10's / M3-R1's / M3-R2's /noInterp, and the /meshImmediate twins:
motion-optout byte-equal to r10i-motion-noi
motion-optout DIFFERS from m3r1i-motion-noi: 0 crc-stripped lines differ, 603 whole
motion-optout DIFFERS from m3r2i-e-motion-noi: 0 crc-stripped lines differ, 603 whole
motion-on byte-equal to motion-on-imm (/meshImmediate)
nis-optout byte-equal to r10i-nis-noi
nis-optout DIFFERS from m3r1i-nis-noi: 0 crc-stripped lines differ, 1260 whole
nis-optout DIFFERS from m3r2i-e-nis-noi: 0 crc-stripped lines differ, 1260 whole
nis-on byte-equal to nis-on-imm (/meshImmediate)

the producers' last references (m3r3-et-*, the engine-trails port's final dumps; m3r3-sk-* producer 1's; m3r3-pf-f-* producer 2's):
m3r3-et-motion-noi c503c869f0dc9cdb / 3016280e9e0dd3c8 EQUAL
m3r3-et-motion-on c0e411662fa2f527 / 428ef41479eb5749 EQUAL
m3r3-et-nis-noi 86cb155acffc180e / 196bab32d244f614 EQUAL
m3r3-et-nis-on 0307f3be2044c29c / a5de13a44b8d264f EQUAL
m3r3-sk-motion-noi c503c869f0dc9cdb / 3016280e9e0dd3c8 EQUAL
m3r3-sk-motion-on c0e411662fa2f527 / 428ef41479eb5749 EQUAL
m3r3-sk-nis-noi 86cb155acffc180e / 196bab32d244f614 EQUAL
m3r3-sk-nis-on 0307f3be2044c29c / a5de13a44b8d264f EQUAL
m3r3-pf-f-motion-noi c503c869f0dc9cdb / 3016280e9e0dd3c8 EQUAL
m3r3-pf-f-motion-on c0e411662fa2f527 / 428ef41479eb5749 EQUAL
m3r3-pf-f-nis-noi 86cb155acffc180e / 196bab32d244f614 EQUAL
m3r3-pf-f-nis-on 0307f3be2044c29c / a5de13a44b8d264f EQUAL
m3r3-mines-motion-noi c503c869f0dc9cdb / 3016280e9e0dd3c8 EQUAL
m3r3-mines-motion-on c0e411662fa2f527 / 428ef41479eb5749 EQUAL
m3r3-mines-nis-noi 86cb155acffc180e / 196bab32d244f614 EQUAL
m3r3-mines-nis-on 0307f3be2044c29c / a5de13a44b8d264f EQUAL
m3r3-m02-motion-noi c503c869f0dc9cdb / 3016280e9e0dd3c8 EQUAL
m3r3-m02-motion-on c0e411662fa2f527 / 428ef41479eb5749 EQUAL
m3r3-m02-nis-noi 86cb155acffc180e / 196bab32d244f614 EQUAL
m3r3-m02-nis-on 0307f3be2044c29c / a5de13a44b8d264f EQUAL
m3r3-et-motion-exe vs m3r3i-e-motion-exe: EQUAL
m3r3-sk-motion-sharp vs m3r3i-e-motion-sharp: EQUAL
m3r3-sk-nis-sharp vs m3r3i-e-nis-sharp: EQUAL

**No default dump moved this round**: the four references whole / crc-stripped (motion `/noInterp` `c503c869f0dc9cdb` / `3016280e9e0dd3c8`,
flags-on `c0e411662fa2f527` / `428ef41479eb5749`, nis `/noInterp` `86cb155acffc180e` / `196bab32d244f614`, flags-on `0307f3be2044c29c` /
`a5de13a44b8d264f`) are byte-equal to M3-R2's and M3-R1's files and to every producer's final references of the round (producers 1-5:
`m3r3-sk-*`, `m3r3-pf-f-*`, `m3r3-m02-*`, `m3r3-mines-*`, `m3r3-et-*`); the opt-outs together byte-equal to R10's `r10i-motion-noi` /
`r10i-nis-noi` (`183681235a3fdc7b` / `13ca70eea998e5de`; against M3-R1's / M3-R2's `/noInterp` runs 0 crc-stripped lines differ, the
`crc` column alone as in M3-R2); the `/meshImmediate` twins byte-equal to the batched path's. **The two stated crc-only moves are the
options themselves**: `/sharpTextures` (`motion-sharp` / `nis-sharp` `1a2227d966ec9712` / `871eac333fd603d0` whole, crc-stripped equal to `motion-on` / `nis-on`: 1,431 / 1,439 lines differ whole, 0 crc-stripped, byte-equal to producer 1's `m3r3-sk-motion-sharp` / `-nis-sharp`: producer 1's statement, every mesh texture re-filtered) and `/exeTrails`
(`motion-exe` = `motion-on` and `nis-exe` = `nis-on` byte for byte (no missile corvette in either scene), `motion-exe` byte-equal to producer 5's `m3r3-et-motion-exe`: no missile corvette in either scene, as producer 5 states); the nine paused dumps are fence 2 (the two `universePause` dumps' paused frames differ from producer 2's in the commit id of the build text drawn while paused alone (beside the verdict, 1)).

### The self-tests (condition (a))

Every self-test of the gate PASS: `statics` / `statics-init` (`ship statics: 36 loaded, 0 not found (disabled)`, the static-init variant the same), `meshes` (`1758 listed, 1729 .geo/.peo loaded, 29 .mad loaded, 0 with bad objects`), `textures` (`listing lookups 12403/12403 found, 0 size/flag mismatches ... 10661 registered in 3 batches, 10661 loaded, 0 not loaded, 9248 scaled`), `etg` (`8 scripts, 197 effects loaded, 0 referenced but not loaded`), `dock`, `command` (`link=252 mcv=1 unlink=285 research=1/1 mimic=1 point=1/1 result=PASS`), `render` (583 frames, `effects=1404 trails=582`), `sound` (the dummy driver, `Music4Cat.wxd` 62 streams, `SectSpeech.vce`), `hud` (`sensors scenes=119 rangecircles=14388 shippoints=8791`), `ui`, `fescreens` (`files=22 loaded=22 screens=195 ... wrong=0 result=PASS` + `optionstest: options=35 startdefaults=35 readback=35`), `strings` (`ids=1705 mapback=1705 resolved=1681 nostring=24 wrongtext=0`), `keys` (`rows=197 known=149 ... failures=0` + the F-key line), `console` (`shown=38 set=31 ... tablerows=486 ... failures=0`), the `build` pair (7,000 ticks, byte-identical), `nisparse` (`46 files listed; nis=14 nisScripts=14 animatics=18 objects=593 events=1488 cataclysmEvents=31 ... failed=0`), `modequit` (1024x768 -> 1280x720, `switched=1 result=PASS`, rc 0, one quit line), the toggle self-test under all ten switch sets, the uiscale self-test x9 (`checks=45 fails=0`), the oracle self-test (`ticks=512 packets=256 randSyncErr=0 univSyncErr=0 blobSyncErr=0 records=772`) with `tools/oracle/selftest.sh` on the release binary (`SELFTEST PASS`: the pair byte-identical, the perturbed copy reported at tick 400 `ships_pos`), the `missiontest` pair (Mission01 NIS-first, `objectives=6/6` at 11,424), the `loadgame` pair (`01 - Hiigara`: `682 chunks converted, 167 ships, 0 size problems`, 3,600 ticks), the `warp10` pair, the `tload` pair (`LESSON 11 - Researching and Upgrading`, `ticks=3600 from=8361 to=11961 ... result=PASS`), the Tutorial1 pair (`lessons=12/12 complete=12/12`), the `fj3` pair (the failed jump in Mission03), the ai pair + the 4,000-tick soak, the skirmish pair (LiveFireExercise2) + Armed8, the team colours (`tc-user` / `tc-red` `checks=7 failed=0`, `tc-m3` `checks=6 failed=0`), lodtest (`drawnAbove0=0 bandAbove0=164 maxBand=4 verts=25169` with FULL DETAIL; `drawnAbove0=164 ... verts=5330` with `/lodBands`); every quitting run rc 0.

### UBSan kinds (condition (a))

ASan 0 and no unsuppressed leak in all 225 runs; fescreen UBSan 0 in all. The UBSan kinds (`UBSan report kinds (count, site)` blocks of the 225 summaries, `an_ubsan.py`): **9 kinds, the same nine as M3-R2's, M3-R1's and R10's, no new kind** (every one a misaligned access in the particle / ETG / BTG data, non-fatal); 52 runs report nothing (the release runs, fe, the self-tests that load no universe); the first report of every universe run is `BTG.c:389:35` (the misaligned `short` load in the background's star data), as in M3-R2.

| reports | runs | kind (all non-fatal, the same nine as M3-R2's) |
|---:|---:|---|
| 11256 | 78 | `Game/Particle.c: member access within misaligned address X for type 'struct particle', which requires 8 byte alignment` |
| 3897 | 173 | `Game/ETG.c: member access within misaligned address X for type 'struct etgdecision', which requires 8 byte alignment` |
| 2258 | 173 | `Game/ETG.c: member access within misaligned address X for type 'struct etgfunctioncall', which requires 8 byte alignment` |
| 1958 | 173 | `Game/ETG.c: member access within misaligned address X for type 'struct etgconditional', which requires 8 byte alignment` |
| 1520 | 173 | `Game/ETG.c: member access within misaligned address X for type 'struct etgbranch', which requires 8 byte alignment` |
| 1132 | 69 | `Game/ETG.c: member access within misaligned address X for type 'struct particle', which requires 8 byte alignment` |
| 344 | 172 | `Game/BTG.c: load of misaligned address X for type 'short unsigned int', which requires 2 byte alignment` |
| 230 | 75 | `Game/ETG.c: member access within misaligned address X for type 'struct etgvariablecopy', which requires 8 byte alignment` |
| 107 | 107 | `Game/BTG.c: load of misaligned address X for type 'udword', which requires 4 byte alignment` |

(M3-R2's 162 runs: 9,383 / 2,626 / 1,536 / 1,438 / 1,024 / 970 / 226 / 194 reports of the first eight kinds; more runs here, the same sites.)

## The five fences, rerun by this step

Every fence on the plain harness from the copied-aside binaries (chains G, G2 and K: the 1024x768 and 4K captures and the dumps on
the debug binary, beside the user's load; chain H alone after the ten correctness chains: the release binary's GPU batch, run at 14:55-14:59 after `gpuwait` found the GPU at or under 40 % for 120 s (nvidia-smi 0 % / 3142 MiB and load average 7.12, 14.37, 17.67 at its start, 0 % / 8.82, 12.85, 16.45 between the two batches, 1 % / 8.64, 12.49, 16.24 after; it had waited since 11:49 for the user's game; `claude-ping` fired 2 time(s): 11:49:21, 14:57:12, since the chain library's `gpuwait` pings once per CALL and chain H calls it before each batch, so the second wait, two minutes of a 50 % sample between the batches, pinged again: a deviation from the user's once-per-gate rule to fix in `chains/lib.sh` before M3-R4);
chain X after the first analysis: four attribution runs, below). The runs' own lines are in the runs table; the comparisons below are
this step's (`tools/ppm_region.py diff` / `stats`, `cmp`, `hwo_compare.py`, the dump column tests of `an_pause.py`, the scanline and
ring tests of `an_trails.py`, all under `original/scratch-m2/m3r3-integrate/an/`).

### Fence 1: settings-and-keys (producer 1, audit 0.94): the overlay's F key, the switch-label rule, SHARP TEXTURES

- **The keys self-test's F-key line** (`m3r3i-e-keys`): `overlay fkey F1 scancode=58 vk=0x70 plain bindings F1=0 F12=1 free=1
  bound-free=0 reset-free=1`, the test `rows=197 known=149 unknown=48 commands=147 bindings=149 startsave=1 saved=1 reload=1 reset=1
  newkey=1 conflicts=1 shares=1 failures=0 result=PASS` (M3-R2: the same counts without the F-key line).
- **F1 toggles the overlay** (`sk-fkey`: the ai scene, `/catOverlayKeyAt 150,200`, shots at 140 / 160 / 210, against `sk-fkey-off`):
  t140 `differ=0`, **t160 `differ=14400 bbox=2,2,227,66 maxdiff=204`** (the overlay's box alone: 225 x 64 px, 70 colours), t210
  `differ=0`; the settings lines `overlay=1 (key) ... [key overlay]` at 150 and `overlay=0 (key)` at 200; the streams of the two runs
  IDENTICAL to each other and to `ai-a` over the 260 ticks. Against producer 1's captures 5 of the 6 frames are byte-equal; the t160
  frame differs in 119 px inside the digits (`bbox=45,7,74,26`: the frame times shown), as M3-R2's overlay check. The Mission01 gate
  `mission-fkey` (F1 at 100 and 200, the console rows `texLinearFiltering=0` at 300 and `=1` at 400): 5 settings lines, the stream
  IDENTICAL to `mission-a`.
- **The toggle self-test under all ten switch sets** (the switch-label rule; M3-R2's one failing check): plain `toggle: done start=1/1
  fails=0 result=PASS`, `/noInterp` `start=0/0 fails=0 PASS`, `/noNISInterp` `start=1/0 PASS`, **`/letterboxBehind` `start=1/1 fails=0
  result=PASS`** (M3-R2's `fails=1` closed), `/lodBands`, `/overlay`, `/meshImmediate`, `/noFilter`, `/sharpTextures`, `/exeTrails` all
  `fails=0 result=PASS`; the settings lines under a standing switch keep `(switch)` after a console row (`toggle-exe`: `engineTrails=0
  (switch) switches=/exeTrails`), the console row of a row with no file row prints `(console)` (`mission-trailrow`: `engineTrails=0
  (console) ... [console engineTrails]` at 300, `=1 (console)` at 400).
- **SHARP TEXTURES, the captures** (tol 0; the ai scene with `--lodstats 150-230:fleet`, 164 objects, 25,169 vertices): linear against
  `/sharpTextures` **t200 `differ=2685 bbox=810,366,906,410 maxdiff=35`, t240 `2674 (811,369,908,414)`** (producer 1's 2,685 / 2,674
  exactly); the cfg row under `effectsLevel 2` (`sk-cap-cfg`) against the switch 438 px (`bbox=160,352,901,438 maxdiff=146`: the CUSTOM
  level's other effects rows at the file's values, as the producer's 445) and against linear 3,065; **the cfg row alone** (`sk-cap-cfgh`,
  `texLinearFiltering 0` without `effectsLevel 2`) against linear **`differ=0`**: the exe's HIGH preset undoes it at the start (the
  settings line twice, `texLinearFiltering=0 (cfg) [start]` then `=1 (preset)`; `texfilter: filter=linear`; the cfg saved back at exit
  as `effectsLevel 0 texLinearFiltering 1`, as the exe writes it); **the live console flip** (`sk-cap-live`: `=0` at 180, `=1` at 220):
  t200 = the switch's frame (`differ=0` against `sk-cap-sw`), t240 = the linear frame (`differ=0` against `sk-cap-lin`), its lines
  `texfilter: filter=sharp textures=2361 globjects=2361 wrong=0 scaled=472 pass=2` then `filter=linear ... pass=3`. At 4K: the ai scene
  `differ=20864 bbox=2758,1028,3027,1153 maxdiff=84`; Mission01 with `--lodtest 100:fleet` t150 `17781 (878,545,2424,1500)`, t300 `17867`;
  **the Kuun-Lan's closest zoom** (`--scene mothership:2300`, the skirmish at tick 100) **`differ=113372 bbox=279,0,766,768 maxdiff=123`
  at 1024x768 and `890954 (1265,0,2634,2160) maxdiff=125` at 4K** (a 320 x 200 hull patch: 52,366 px). Against producer 1's captures:
  the Kuun-Lan pair, the 4K Kuun-Lan pair, the four Mission01 4K frames and the in-game Video screen **byte-equal**; the ten ai-scene
  frames differ in 64-88 px at the frame's centre (`bbox=384,367,459,385` / `388,371,454,388`), the Beast missile corvettes of the ai fleet (`bMissileCorvette#302` among them): chain X's `/exeTrails` twin of the capture (`sk-cap-exe`) is **byte-equal to producer 1's linear capture** at both ticks (`differ=0`; made at `c8f97bb`, before the engine-trails port) and differs from this step's linear capture in the same 64 / 88 px: the four ribbons of producer 5's feature, its stream IDENTICAL.
- **The Video options screens**: `sk-scr-ig` (`/catFeScreen Video@100` in Mission01, shot 120) byte-equal to producer 1's; `sk-scr-fe` /
  `sk-scr-fe4k` (`Video_Options` on the front end, shot 0:400) differ from producer 1's in 283 px at the bottom-right corner
  (`bbox=904,756,1021,765`, two colours: the build-identity text, `c8f97bb` against `6ad69ec`).
- **The saved rows at exit**: `sk-base-cfg0` (fe, the row alone) `effectsLevel 0 texLinearFiltering 1`; `sk-base-cfg0c` (`effectsLevel 2`
  + the row) `effectsLevel 2 texLinearFiltering 0`; `sk-base-nofilter` (`/noFilter`) `effectsLevel 0 texLinearFiltering 1` (the switch
  outlives the start and is not saved); Mission01's `mission-cfg0` / `mission-sharpcfg` the same; `mission-sharpsw` / `-nofilter`
  `texLinearFiltering=0 (switch) ... switches=/noFilter` (the alias reported as the exe's switch).
- **The console, fescreens and strings self-tests** with producer 1's and producer 5's rows: `consoletest: done selected=154 frames=4
  shown=38 set=31 commands=8 keys=32 kas=6 closed=1 failures=0 result=PASS` (`tablerows=486`; M3-R2 36 / 29), `fescreens: done files=22
  loaded=22 screens=195 pushed=195 popped=195 wrong=0 ... result=PASS` with `optionstest: done options=35 startdefaults=35 readback=35
  result=PASS` (the SHARP TEXTURES control is a checkbox over the existing row, so the option count holds at 35), `stringstest ...
  ids=1705 mapback=1705 resolved=1681 nostring=24 wrongtext=0 wrongnostring=0 result=PASS`, modequit PASS, the uiscale self-test x9
  `checks=45 fails=0 result=PASS` (4K `logical=1920x1080 scale=200` twice and `uiScale 100` in the cfg `scale=100`, `/uiScale 150`
  `logical=2560x1440 scale=150`; 2560x1440 `scale=150`; 1920x1080, 1024x768 and the two `--fit` runs `scale=100`).
- The `--lodstats` numbers of the capture runs (1024x768 medians 36-38 ms, 4K 36-39) are **not measurements** (the user's game on the
  CPU and GPU): the 4K cost is chain H's, fence 5.

### Fence 2: pause-freeze (producer 2, audit 0.78): the paused dumps

The nine pause runs of producer 2's gate (`/catMotionPause`), judged by `an_pause.py` on every column of every paused frame (every token
but `crc=`, `crcv=`, `pause=`, the tick and the scene dump's `raw=`, the task clock's fraction (`taskPeriodFraction`, which
cycles through a pause by design; `alpha=`, the held render alpha, is a column): eye, look, up, each object's x, y, depth, on, the
scene's world positions):

| run | pause (summary) | paused frames | paused pairs with a column change | crcv changes | crc changes | entry (first paused == previous, every column) | after (first 11 frames after the unpause vs the un-paused twin: columns changed) | vs the producer's final dump |
|---|---|---|---|---|---|---|---|---|
| `pz-key-on` | on key F500 t119; off key F572 t119 | 72 (F501..F572) | **0 of 71** | 0 | 67 | F501 == F500 | 11 of 11 frames differ (per frame 6,6,6,5,6,6,6,6,6,6,5) | byte-equal (`m3r3-pf-f-pz-key-on`) |
| `pz-key-noi` | on key F500 t119; off key F572 t119 | 72 (F501..F572) | **0 of 71** | 0 | 67 | F501 == F500 | 0 of 11 frames differ (per frame 0,0,0,0,0,0,0,0,0,0,0) | byte-equal (`m3r3-pf-f-pz-key-noi`) |
| `pz-key-on-b` | on key F503 t119; off key F544 t120 | 41 (F504..F544) | **0 of 40** | 0 | 37 | F504 == F503 | 11 of 11 frames differ (per frame 6,5,5,6,5,6,6,2,6,6,5) | byte-equal (`m3r3-pf-f-pz-key-on-b`) |
| `pz-univ-on` | on univ F500 t119; off univ F572 t119 | 72 (F501..F572) | **0 of 71** | 0 | 67 | F501 == F500 | 11 of 11 frames differ (per frame 6,6,6,6,6,6,6,6,6,6,6) | DIFFERS (72 / 1503 lines) (`m3r3-pf-f-pz-univ-on`) |
| `pz-univ-noi` | on univ F500 t119; off univ F572 t119 | 72 (F501..F572) | **0 of 71** | 0 | 67 | F501 == F500 | 11 of 11 frames differ (per frame 4,4,4,4,4,4,4,4,4,4,4) | DIFFERS (72 / 1503 lines) (`m3r3-pf-f-pz-univ-noi`) |
| `pz-nis-on` | on nis F600 t498; off nis F672 t506 | 72 (F601..F672) | **0 of 71** | 1 | 1 | F601 == F600 | 0 of 11 frames differ (per frame 0,0,0,0,0,0,0,0,0,0,0) | byte-equal (`m3r3-pf-f-pz-nis-on`) |
| `pz-nis-noi` | on nis F600 t498; off nis F672 t506 | 72 (F601..F672) | **0 of 71** | 1 | 1 | F601 == F600 | 0 of 11 frames differ (per frame 0,0,0,0,0,0,0,0,0,0,0) | byte-equal (`m3r3-pf-f-pz-nis-noi`) |
| `pz-nis-shot` | on nis F600 t498; off nis F672 t506 | 72 (F601..F672) | **0 of 71** | 1 | 1 | F601 == F600 | 0 of 11 frames differ (per frame 0,0,0,0,0,0,0,0,0,0,0) | byte-equal (`m3r3-pf-fix-nis-on-shot`) |
| `pz-scene-key` | (no pause line) | 72 (F701..F772) | **24 of 71** | 24 | 69 | F701 == F700 | n/a | byte-equal (`m3r3-pf-fix2-scene-key`) |

- **Every paused pair holds every world column** in the nine runs (and every screen column in eight; the scene run's camera converges
  by one ulp, below): the key pause 0 of 71 (the `crc` moves in 67 pairs are the HUD's SU blink,
  `crcv` 0), the mid-tick key pause 0 of 40 (`crc` 37; its entry differs from the previous frame in the tick token alone: the pause
  took effect after a tick, as the producer's table), `universePause` 0 of 71 (`crc` 67, `crcv` 0), the NIS pause 0 of 71 with its one
  `crc` / `crcv` change at F601 -> F602 (the simulation's own: the NIS's first tick in the pause writes each NIS ship's velocity as
  its position, the trails stop; the universe keeps ticking under a NIS-only pause: 498 -> 506 over the 72 frames), the user's recipe
  (72 paused frames (F701..F772): no world column (`Aw`..`Mw`, the objects' world positions) moves in any of the 71 pairs; the camera's eye alternates by one float32 ulp (`5830.0688` / `5830.0693`) in 24 of 71 pairs, with `crcv` moving in those 24 and the objects' screen columns following the camera by at most 0.0019 px (`CameraChase`'s easing at a converged camera, the exe's; the audit measured the same)). The entry frame equals the frame before in every column in all nine. After the unpause the flags-on runs' first 11
  frames differ from the un-paused twin's in the camera and the four objects (the hold until the next tick, F573-F575, then the carry
  tick F576-F583: the producer's "hold and carry"), `/noInterp` 0 of 11, the NIS pause 0 of 11 (its time aligned by the 72 frames).
- **Byte-equality with producer 2's final dumps**: 7 of 9 byte-equal (`pz-key-on`, `-noi`, `-on-b`, `pz-nis-on`, `-noi`, `-shot`,
  `pz-scene-key`); **`pz-univ-on` and `pz-univ-noi` differ in the 72 paused lines' `crc` and `crcv` only** (every object, eye, look and
  up column equal; this step's `crcv` constant `411a110d` through the pause, the producer's final and the audit's `f5b531de`, the audit's
  reverted build `99420165`; the 1,431 un-paused lines byte-equal, `crc` included). Chain X: a rerun (`pz-univ-on2`) is byte-equal to
  `pz-univ-on` (deterministic), the `/exeTrails` twin (`pz-univ-exe`) gives the same `411a110d`; `rndRenderTask` (src/SDL/render.c:5461-5469, the exe's 0x0057ab78 render task) prints `networkVersion` (`Emergence 1`) and `HOMEWORLD_BUILD_VERSION` (meson's `git describe --always --tags --dirty=+`: `6ad69ec` for this step's clean builds) in white at the bottom right of every frame drawn while `universePause || !gameIsRunning`: the captured paused frame (`/catMotionShot 499,501,502,560`, `x-univ-shot`) of this step's binary against the mines-port build `4ae7d91`'s (a scratch worktree, removed after) differs in **71 px at `bbox=980,756,1020,763` (`maxdiff=224`): the seven characters of the commit id** (the enlarged crops read `Emergence 1 6ad69ec` and `Emergence 1 4ae7d91` beside the "Paused" caption's timer); F499 before the pause `differ=0` across the builds, F501 -> F502 -> F560 `differ=0` within the pause on both; `4ae7d91`'s paused `crcv` is a third value, `2a49932e`. So the three `crcv` values are the three commit ids (`292a6da` `f5b531de`, `4ae7d91` `2a49932e`, `6ad69ec` `411a110d`): a build-identity text, not a render change; the key pause (`keyPause`, not `universePause`) draws no such text (its dumps byte-equal to the producer's, `crc` included), and the same text is the front end's 283 px at `bbox=904,756,1021,765` (`scr-fe`, `scr-fe4k` vs producer 1's captures at `c8f97bb`: `!gameIsRunning`).
- **The NIS pause's frames** (`pz-nis-shot`, `/catMotionShot 600,601,602,603`): F600 -> F601 2,533 px (`bbox=386,488,604,550 maxdiff=9`:
  the last live frame's trail shimmer against the first paused), F601 -> F602 16,179 px (`bbox=383,488,603,661 maxdiff=210`: the
  simulation's trail stop, the one `crc` change inside the pause), **F602 -> F603 `differ=0`**; all four byte-equal to producer 2's.
- **The user's recipe** (`pz-scene-key`: the skirmish with `--scene follow:1500:500:200`, P after F700 for 72 frames, shots 702 / 703 /
  704 / 710): F702 -> F703 `differ=0`, F703 -> F704 19 px (`bbox=508,238,752,441 maxdiff=8`: the camera's one-ulp alternation), F704 ->
  F710 `differ=0`; byte-equal to producer 2's `m3r3-pf-fix2-scene-key` frames; the stream IDENTICAL to the producer's and, in the 19
  simulation fields, to the plain skirmish's first 260 ticks (the scene's own orders (a move, a formation, a dock, a harvest: M3-R1's scene) make it a different game from the plain skirmish from tick 48 on, by design; against producer 2's `m3r3-pf-fix2-scene-key` IDENTICAL in all 21 fields).

### Fence 3: mission02 (producer 3, audit 0.94): the Dust points and the BUSHAN-RE hint

- **The hint check** in every Mission02 run (`hint ... expected=.. fired=.. result=..`): the gate pair `m2-a` / `m2-b` (NORMAL, the
  scripted player's scout finds her at tick 1152, the timer's 360 s would run out at 5762) `expected=no fired=no result=PASS`, 6 / 6 at
  25,936; **`m02-hint-n`** (NORMAL, the scout held to 6400): `hint watch tick=5762 the hint timer's 360 s have run out (skill 2)`, **`hint
  speech tick=5824 (FleetIntel 0x004ce352: sensors open on the BUSHAN-RE, PingHint1-3)`**, found at 7232, `expected=yes fired=yes
  result=PASS`, 6 / 6 at 29,536; `m02-hint-e` (VERY EASY, `DifficultySetting 0`: found at 1152 before the 120 s due at 1922)
  `expected=no fired=no PASS`, 6 / 6 at 25,120; `m02-hint-h` (HARD, `DifficultySetting 3`, the scout held) `skill=3 timer=-1 due=-1
  found=7488 ... expected=no fired=no result=PASS` (no timer exists on HARD), the self-test itself `objectives=4/6 result=FAIL` at
  90,000 (the scripted player does not finish on HARD: the SalvagePod, as the producer found); the capture runs `m02-shot-on` / `-off`
  the speech at 5824 too. Every one IDENTICAL in every field to producer 3's run of the same spec.
- **The Dust points**: `resources tick=1 asteroids=0/10/5/0/0 meteors=420 minor asteroid0=486 other=0 dustclouds=0/0/0/0` in every
  Mission02 run (no dust cloud in the level), `sensors scenes=1794 dustpoints=871884 resourcepoints=55667` in `m2-a` (**486 points in
  every one of the 1,794 sensors frames**), 2,229 / 1,083,294 in `m02-hint-n`, 435 / 211,410 in the capture runs with the view option on
  and **`dustpoints=0`** with `ShowAsteriod0 0`.
- **The captures** (the sensors manager open at 5832-6272 every 40 ticks, the scout held; on against off, tol 0): 1024x768 **247, 261,
  261, 259, 261, 261, 261, 261, 261, 259, 261 px** (`bbox=84,120,994,660` then `145,221,1006,661`, `maxdiff=162`), 4K **269, then 275 x 10**
  (`bbox=716,339,3718,1883` / `890,623,3665,1883`): the points are 1 physical pixel at both sizes (the user's question 4); t6272 `differ=0`
  (the 3D view); the on frame's pixels of the dust colour (162,142,120 within 8) at t6072: 261. Against producer 3's captures 11 of 12
  frames byte-equal in each of the four runs; the t6272 frame (the 3D view) differs in 33 px (`bbox=989,543,1024,549 maxdiff=7`; 4K 314
  px `bbox=3261,1526,3462,1543 maxdiff=31`) at the right edge: the mines fix: chain X's `/exeTrails` twin (`m02-shot-exe`) is byte-equal to this step's default run in all 12 frames (so not the trails), and the same capture on the mines-port build `4ae7d91` (a scratch worktree, since removed) is byte-equal to this step's 12 frames and differs from producer 3's `96b0137` frame in the same 33 px: the pixels moved at `4ae7d91`, i.e. `meshStaticTeamColor` (a Beast missile in flight at the frame's right edge reads its own colour row now), producer 4's stated change; the streams of all four runs IDENTICAL.
- The streams: `m02-shot-on` = `m02-shot-off` IDENTICAL (the view option moves nothing), the 4K pair IDENTICAL, 4K against 1024x768
  IDENTICAL in the 20 fields (`rng_all` at 2271: decision 84's stream 11), against producer 3's IDENTICAL; the held scout against the
  gate run `m2-a` diverges at 328 in five fields (the hold is a different game from the scout's first order, by design).

### Fence 4: mines (producer 4, audit 0.95): the textures and the mine AI

- **The textures** (`/catMeshTexLog /MINE/RL` in Mission03's self-test; `meshtex: draw`, first per mesh): the level mine `tick=300
  type=8 race=6 scheme=0 mesh RACE_DERELICT/mine/Rl0/LOD0/mine.peo textures=11 bound=11 white=0` (`sidereardisks` 64x64 x3, `botmid`
  32x64, ...), the laid mine `tick=14210 type=8 race=3 scheme=5 mesh RACE_TAIDAN/mine/rl0/lod0/mine.peo textures=4 bound=4 white=0`
  (`page0` 64x64 x4; the drop run at 3504 / 4832); the register lines `RACE_TAIDAN/MINE/RL0/LOD0/PAGE0 static=found teamColor=00000100
  handles=...,485,...`; **FileLoads 3,174** (3,170 before the fix: the Taiidan mine's `PAGE0` LOD0-3). The missiles' log (`mn-missile`,
  `/catMeshTexLog MISSILE/RL`): 84 `register load` lines (`RACE_BEAST/MISSILE/RL0/LOD0-3`, `teamColor=01000000`), byte-identical to
  producer 4's `m3r3-mines-missile-after`, no draw line (no missile in the self-test's view; the brief asks).
- **The captures** (`/catMineView 300:150` at 320 / 400 / 480 / 640; `300:dropped:150` at 2000-17000 / 1000): the level mine's region
  `432,304,592,464` **t400 10,657 colours** (mean 85,63,41), t640 10,582; the laid mine's `480,350,544,418` **t5000 970**, t6000 977 (the
  producer's BEFORE captures: 19 and 6 flat colours; against them 25,389 / 25,401 and 1,341 / 1,332 px differ); **batched against
  `/meshImmediate` `differ=0`** on all nine frames; **all 40 frames byte-equal to producer 4's final captures** (`-after`, `-imm`).
- **The Recon chase** (`mn-chase-recon`, `/catMineChase 400`, `/catMineLog 480`): mine 63 `2->3 why=acquired target=sRecon#6` at 1295
  (`dist=9818`), 1,000 u/s by 1310, `dist=329` at 1374, **`3->2 why=faster-away` at 1376** (`speed=975.3`), braking 487.6 / 61.0 / 0.0 by
  1387, **re-acquired at 1391 (`dist=1092`), `faster-away` at 1392**; mine 62 acquired 1294, `faster-away` 1397 (`speed=1000.0`): the
  producer's ticks exactly. **The Acolyte chase** (`400:sAcolyte`): mine 63 acquires `sAcolyte#11` at 1535, 62 `sAcolyte#9` at 1550, 66
  `#11` at 1730, **`cleared` at 1753** (the target's pointer gone), 62 `faster-away` 1755 then `sAcolyte#8` 1758, **66 re-acquires
  `sAcolyte#10` at 1762 and its log ends at 1833** (`dist=5666`: the audit's 5 s lost-target lifetime). Both streams IDENTICAL to producer
  4's.
- **The Mission03 log** (`mn-ailog`, 2,528 lines): 199 level mines `0->3 why=start` at tick 1 and `3->2 why=cleared` at 2; 30 laid mines
  `1->4`; **six laid mines `4->3 why=acquired target=sMimic#19` at 14138, 14141, 14156, 14158, 14169, 14171**; **four `3->2 why=cleared`
  at 14391** (ids 217, 219, 220, 222: the Mimic gone; 218 and 221 reached it), their re-acquire of `sMothership#0` at 14393 / 14395 /
  14396 / 14398 (`speed=150.0 / 37.5 / 18.8 / 4.7`: the brake), **their per-tick lines end at 14470 / 14471** (the 5 s lifetime); two
  level mines acquire Recons at 16871 / 16875; `why=faster-away` 0 in the self-test's own play (nothing flies away from a mine);
  `mines=199 ... objectives=5/5 result=PASS` at 17,825 in every Mission03 run; the stream IDENTICAL to `m3-a` (the log is passive).
- The meshes self-test's mine rows: the Beast mine `textures=0` at every LOD (no texture in the data, as the exe); every other mine row
  `bad=0`.

### Fence 5: engine-trails (producer 5, audit 0.944): the missile corvette's four ribbons

- **The ribbons** (`/catTrailView`; the camera behind the focused corvette; `an_trails.py` counts the orange / yellow runs on three
  scanlines 30 / 50 / 80 px below the nozzles' mean centre (x3.75 at 4K), within the corvette's nozzle span +-40 px, a run at least 2
  px (8 at 4K); the frames are byte-equal to producer 5's, so the counts differ from the producer's only by the method):

| run pair | size | tick | default | /exeTrails | vs producer 5's frames (cmp) |
|---|---|---|---|---|---|
| `et-c-on` / `et-c-exe` | 1024x768 | 300 | 3 / 3 / 2 | 2 / 2 / 1 | m3r3-et-c-on2: EQUAL ; m3r3-et-c-exe2: EQUAL |
| `et-c-on` / `et-c-exe` | 1024x768 | 400 | 4 / 3 / 2 | 2 / 2 / 1 | m3r3-et-c-on2: EQUAL ; m3r3-et-c-exe2: EQUAL |
| `et-c4k-on` / `et-c4k-exe` | 3840x2160 | 300 | 4 / 3 / 1 | 2 / 2 / 1 | m3r3-et-c4k-on: EQUAL ; m3r3-et-c4k-exe: EQUAL |
| `et-c4k-on` / `et-c4k-exe` | 3840x2160 | 400 | 4 / 3 / 1 | 2 / 2 / 1 | m3r3-et-c4k-on: EQUAL ; m3r3-et-c4k-exe: EQUAL |
| `et-p1mv-on` / `et-p1mv-exe` | 1024x768 | 160 | 4 / 3 / 1 | 2 / 1 / 1 | m3r3-et-p1mv-on: EQUAL ; m3r3-et-p1mv-exe: EQUAL |
| `et-p1mv-on` / `et-p1mv-exe` | 1024x768 | 240 | 2 / 2 / 2 | 1 / 2 / 0 | m3r3-et-p1mv-on: EQUAL ; m3r3-et-p1mv-exe: EQUAL |
| `et-b-on` / `et-b-exe` | 1024x768 | 300 | 2 / 3 / 2 | 2 / 3 / 2 | m3r3-et-b-on: EQUAL ; m3r3-et-b-exe: EQUAL |
| `et-b-on` / `et-b-exe` | 1024x768 | 400 | 2 / 3 / 2 | 2 / 4 / 2 | m3r3-et-b-on: EQUAL ; m3r3-et-b-exe: EQUAL |

  On the first scanline below the nozzles **the default draws 3-4 runs where `/exeTrails` draws 2** (1024x768 and 4K, both corvette
  types); the lower scanlines merge the fanning jets. Every frame (`et-c-on` / `-exe` 300 / 400, the 4K pair, `et-b-on` / `-exe`,
  `et-p1mv-on` / `-exe` 160 / 240, `et-p1mv4k-on` 160, `et-p1rest` / `-4k` 60) **byte-equal to producer 5's final captures**; the
  `/exeTrails` frames byte-equal to the audit's reverted build's (`m3r3-et-c-rev`, `-c4k-rev`: the exe's two ribbons are the reverted
  tree's); default against `/exeTrails`: 1024x768 t300 29,643 px (`bbox=0,204,1024,768 maxdiff=161`), t400 26,815; 4K 304,246 / 206,106;
  the close capture 39,002 / 55,558; the P1 corvette under way 15,556 (t160, `bbox=228,387,514,661`) / 2,951 (t240). The sha256 prefixes
  (producer 5's exactly): `et-c-on` t300 `bd153523a3ce9ea0` / t400 `9d7cda8505f753a2`, `et-c-exe` `0d11d085f3f612e7` / `34208164fc395f64`,
  `et-c4k-exe` `1d5c7d46904470da` / `f980bf9cea5d0bbf`, `et-p1mv-on` t160 `46f0fb91877fda03`. The focus lines `trailview: focus tick=100
  ship=bMissileCorvette#302 trails=1,1,1,1 engineTrails=1` (`=0` with the switch), `tick=40 ship=p1MissileCorvette#31`.
- **The nozzle rings at rest** (`et-p1rest`, Mission03 tick 60, the P1 corvette from straight behind; the whitish ring pixels nearest each
  projected circle centre `c<i>`, the box centre against `c` and the ribbon origin `n`):

| run | circle | ring box | c - box centre | n - box centre |
|---|---|---|---|---|
| `et-p1rest` (EQUAL to m3r3-et-p1rest) | 0 | 362..472 x 291..374 | (+1.4, +0.4) | (+1.6, +0.5) |
| `et-p1rest` () | 1 | 559..669 x 291..374 | (-0.1, +0.4) | (-0.3, +0.5) |
| `et-p1rest` () | 2 | 362..472 x 416..500 | (+1.3, +0.6) | (+1.5, +0.4) |
| `et-p1rest` () | 3 | 559..669 x 416..500 | (+0.0, +0.6) | (-0.2, +0.4) |
| `et-p1rest4k` (EQUAL to m3r3-et-p1rest4k) | 0 | 1497..1810 x 818..1054 | (+3.2, +0.2) | (+3.8, +0.6) |
| `et-p1rest4k` () | 1 | 2051..2364 x 818..1053 | (-0.8, +0.7) | (-1.5, +1.0) |
| `et-p1rest4k` () | 2 | 1497..1810 x 1171..1407 | (+3.0, +0.8) | (+3.6, +0.3) |
| `et-p1rest4k` () | 3 | 2051..2364 x 1171..1407 | (-0.6, +0.9) | (-1.3, +0.4) |

  The ribbon origins sit within 1.6 px of the rings' centres at 1024x768 and within 3.8 px (x) / 1.0 px (y) at 4K (rings 313 px
  across; the producer's figures 0.8 / 3.3 px by its own box method).
- **The random streams** (`rs=` per frame in the `trailview:` lines; streams 13-17 = `RANDOM_TRAILS_0..4`, 11 =
  `RANDOM_SOUND_GAME_THREAD`): `et-rs-on` against `et-rs-exe` 321 common frames (ticks 100-420), **streams 13-17 differ in 0 frames**,
  stream 11 differs from tick 245 (151 frames); the same against the capture runs (0 / 0; stream 11 from 302 / 304); the trail streams
  advance between 320 of 320 consecutive frames in both; this step's `et-rs-on` against producer 5's `m3r3-et-rs-on`: streams 13-17
  equal in 321 of 321 frames. Hence the stream verdicts: the 20 fields IDENTICAL between every default / `/exeTrails` pair (`et-c-on` / `-exe`, `et-rs-on` / `-exe`, `et-b-on` / `-exe` and `et-c4k-on` / `-exe` IDENTICAL over 421 ticks in the 20 fields, `et-p1mv-on` / `-exe` IDENTICAL in all 21 over 260),
  `rng_all` alone from 187-246 (stream 11, decision 84 sharpened), the Mission01 gate `mission-exetrails` IDENTICAL in all 21.
- **The settings line**: `engineTrails=1 (default)` in every default run (`fe` included), `engineTrails=0 (switch) ...
  switches=/exeTrails` under the switch, `(console)` under the console row (`mission-trailrow`), the toggle self-test under `/exeTrails`
  PASS.
- **The 4K cost** (chain H, the release binary, the ai scene at 3840x2160 with `--lodstats 150-450:fleet`, 164 objects, `verts=25169
  polys=46255 fullDetail=1`; run at 14:55-14:59 after `gpuwait` found the GPU at or under 40 % for 120 s (nvidia-smi 0 % / 3142 MiB and load average 7.12, 14.37, 17.67 at its start, 0 % / 8.82, 12.85, 16.45 between the two batches, 1 % / 8.64, 12.49, 16.24 after; it had waited since 11:49 for the user's game; `claude-ping` fired 2 time(s): 11:49:21, 14:57:12, since the chain library's `gpuwait` pings once per CALL and chain H calls it before each batch, so the second wait, two minutes of a 50 % sample between the batches, pinged again: a deviation from the user's once-per-gate rule to fix in `chains/lib.sh` before M3-R4); the CPU load average 7.12, 14.37, 17.67 before the first four runs, 8.82, 12.85, 16.45 before the second four, 8.64, 12.49, 16.24 after (the three `gpu` lines of chain H's log; nvidia-smi 0 % / 0 % / 1 %, 3142 / 2897 / 2905 MiB)):

| run | path | frames | median ms | p99 ms | max ms | verts |
|---|---|---:|---:|---:|---:|---:|
| `perf-on1` | default (four ribbons) | 301 | 3.743 | 5.643 | 6.486 | 25169 |
| `perf-exe1` | `/exeTrails` | 301 | 3.483 | 5.043 | 7.144 | 25169 |
| `perf-imm1` | `/meshImmediate` | 301 | 8.096 | 11.699 | 12.577 | 25169 |
| `perf-sw1` | `/sharpTextures` | 301 | 3.472 | 5.054 | 5.863 | 25169 |
| `perf-on2` | default (four ribbons) | 301 | 3.422 | 4.886 | 6.685 | 25169 |
| `perf-exe2` | `/exeTrails` | 301 | 3.240 | 5.061 | 6.088 | 25169 |
| `perf-imm2` | `/meshImmediate` | 301 | 8.003 | 13.076 | 13.445 | 25169 |
| `perf-sw2` | `/sharpTextures` | 301 | 3.184 | 5.036 | 5.412 | 25169 |

  the default (four ribbons) medians 3.743 / 3.422 ms (p99 5.643 / 4.886), `/exeTrails` 3.483 / 3.240 (p99 5.043 / 5.061): **the trails' 4K cost +0.221 ms on the median against a run-to-run spread of 0.321 ms**; `/meshImmediate` 8.096 / 8.003 (p99 11.699 / 13.076: the batching's gain stands), `/sharpTextures` 3.472 / 3.184 (p99 5.054 / 5.036: the same as the default within the spread). Producer 5's numbers on its idle GPU: `perf-on1` median 3.524 / p99 5.611 / max 5.822 ms, `perf-exe1` 3.572 / 5.313
  / 6.309, `perf-on2` 3.073 / 5.151 / 5.603, `perf-exe2` 3.345 / 5.001 / 5.808; M3-R2's exit test under the user's ripgrep: batched 5.038
  / 12.818 and 4.581 / 16.054, `/meshImmediate` 13.666 / 41.047 and 13.689 / 34.043 (the medians agree with the producers', the p99 is the
  load's: decision 132). The 6.94 ms budget (144 Hz) holds on every batched run's median (the largest 3.743 ms) and on every batched run's p99 (the largest batched p99 5.643 ms; `/meshImmediate` 11.699 / 13.076 is the un-batched baseline, over it as in M3-R2).

## The five ports

| # | item | what landed | DONE WHEN | audit | commit |
|---|---|---|---|---|---|
| 1 | the overlay's F key + the toggle check + SHARP TEXTURES ([report](m3-r3-settings-and-keys.md), [audit](audits/m3-r3-settings-and-keys.md)) | **The overlay's key is F1** (not F12: the exe's `default_keys.cfg` binds plain F12 to Formation_Parade and F2..F11 to tactics / formations; F1 is bound only with modifiers): `main.c HandleEvent` toggles the overlay while the key is free (`KeyBindings.c hwiKbOverlayFKeyFree`: no modifier held, no binding of plain F1, the binding screen not waiting) and the key still goes on to the game; the Menu key stays as a second toggle; the keys self-test 197 / 149 plus the F-key line (`overlay fkey F1 ... plain bindings F1=0 F12=1 free=1 bound-free=0 reset-free=1`); `HWI-FEATURE(instrumentation)` 102 -> 141 lines. **The switch-label rule** (M3-R2's failing toggle check): a console row under a run-only switch over an options-file row (`/noInterp`, `/noNISInterp`, `/letterboxBehind`, `/lodBands`, `/uiScale`, `/noFilter`) keeps the switch standing and the label `switch` (the saves keep the file's row); a switch with no options-file row (`/overlay`, `/meshImmediate`) ends on its console row (`console`); the toggle self-test checks it for every such row. **SHARP TEXTURES** (`HWI-FEATURE(sharp-textures)`, 196 lines): the exe's base is `GL_LINEAR` (the original look) or `GL_NEAREST` min and mag with no mip maps (`trRGBTextureCreate` 0x005866a0, `trFilterEnable` 0x005888e0), its own SURFACE FILTERING box on CUSTOM EFFECTS and its effects presets (`opEffectsHelper` 0x004d9b90, re-applied at every start: the HIGH preset resets `texLinearFiltering` to 1, so the cfg row alone is undone as in the exe); on top, the Options -> Video checkbox SHARP TEXTURES (`OP_SharpTextures`, `Video_Options` and the in-game Video screen; a click makes the effects level CUSTOM; Accept re-filters every live texture), the store the existing row `texLinearFiltering` (1 = the original look = **default**, 0 = sharp; by hand it needs `effectsLevel 2`), the run-only `/noFilter` and its alias `/sharpTextures` (they outlive the start's preset), the console row `texLinearFiltering` (HWI rows 8), the settings-line row with the new source `preset`, the `texfilter:` log line (count, read-back, `wrong=0`); the sharp look is `GL_NEAREST` magnification over GoK's `GL_NEAREST_MIPMAP_LINEAR` minification (the mip maps and 8x anisotropy kept for both looks, decision 28: question 2 below). A `CAT` fix (`trFilterEnable`'s third branch sets the mag filter) and a `CAT-FIX` (the live textures follow the flag at the next frame; the exe's Cancel left them on the cancelled filter). **Why the Kuun-Lan looked blurry at 4K** (the user's 10-25 report): its LOD-0 hull textures are 32x16 .. 256x128 (a texel ~4-9 world units: ~4 px at 4K at the closest zoom, 1.4 at 1024x768) and the TEXTURE MEMORY slider's middle (`TextureDetail 50`, the exe's 6 MB fitting) halves 462 of Mission01's 2,343 live textures, 35 of the Kuun-Lan's 58 LOD-0 ones; HI (100) shrinks none (question 1). HWI-TEST: `/catOverlayKeyAt`, `/catConsoleAt`, `/catFeScreen`, `/catTexList`, the `--scene` kind `mothership`. | met: F1 toggles the overlay (tick 160 `differ=14400 bbox=2,2,227,66`; 140 / 210 `differ=0`), the toggle self-test PASS under all nine switch sets, the capture pairs (ai 2,685 px, the Kuun-Lan's closest zoom 113,372 / 890,954 at 4K), the cfg row alone undone (`differ=0`), the live flip, every stream IDENTICAL, the default dumps EQUAL, the sharp dumps crc-only | PASS-WITH-FIXES, precision 32 / 34 = 0.94 (the claim that the option sharpens the effect sprites fixed in the report, `play.sh` and the help string; one citation; 5 edits for 2 findings) | `c8f97bb` |
| 2 | the pause freeze ([report](m3-r3-pause-freeze.md), [audit](audits/m3-r3-pause-freeze.md)) | **The user's pause is the P key's flag 0x00a6ed74** (`Pause 80`), the exe's frame-step flag, not `universePause` (the Escape menu's and the console's); the jitter reproduced headlessly (a reverted build: 71 / 71 paused pairs moved, the lerp replaying the last tick with a moving alpha) and its cause stated. **`HWI-FEATURE(pause-freeze)`** (90 lines, `MotionInterp.c`; always on, no setting): while the universe is paused (`miUniversePaused()` = `universePause` 0x00a6c5b4, or the P key's flag with no frame step left) every object is drawn at the pose the last frame drew (the lerp's alpha held), the camera float's clocks hold (the exe stops them for `universePause` only), the engine glows keep their size and the trail segments their colour (the random streams drawn as the exe draws them), the NIS's own pause (`nisPaused`, NIS_TIME_CONTROLS builds) holds the NIS render time; the first capture after a pause starts the next tick's blend from the held pose (no step at the pause or at the unpause); the HUD's real-time animations (the SU blink, the upgrade pulse, the ship view, a hit flash's fade) run on as in the exe; the exe's `universePause` float restart (one frame of float time) is kept. HWI-TEST: `/catMotionPause <frame>:<frames>:key\|univ\|nis` (motion, nis and `--scene` runs), `/catMotionShot <frames>`, the ` pause=` and ` crcv=` dump tokens (`crcv` = the frame's crc with the HUD's over-cap SU blink blanked, `catHudSupportBlinkRect`). | met: the paused dumps hold every column (0 of 71 paired changes in every run, `crcv` 0; the 67 `crc` changes are the SU blink; the NIS pause's one `crc` change the simulation's own trail stop), the user's recipe (`--scene follow:1500:500:200`, P after F700) holds every object column (the camera's eye alternates by one float32 ulp in 24 of 71 pairs: `CameraChase`'s easing, 0.005 px), the default dumps EQUAL, the Mission01 and missiontest pairs IDENTICAL, fe / sp ASan 0 | PASS-WITH-FIXES, precision 29 / 37 = 0.78 (10 report figures fixed, among them a depth quoted as pixels; 4 untagged `Trails.c` lines tagged) | `292a6da` |
| 3 | Mission02's dust belts + the BUSHAN-RE hint ([report](m3-r3-mission02.md), [audit](audits/m3-r3-mission02.md)) | **Nothing ported: both are the exe's.** The dust belts are the decorative Asteroid0 (486 from the `decorations` sphere's 97 cylinders; the game's "Dust" view option, `OP_Dust` / cfg `ShowAsteriod0`, on by default): the exe's sensors scene (`cat_smViewportRenderScene` 0x0050b4b0) draws every minor object as a 1-pixel `primPoint3` in 0xff788ea2 with **no fog-of-war test**, where the harvestable asteroids, the dust clouds and the crates (`cat_smResourcesDraw` 0x0050ae00) appear only where the player's grid is visited; the tree does the same (Sensors.c 1843-1938, 1698-1742); no dust cloud exists in Mission02 (Missions 04 / 06 / 08 have 2 / 63 / 5, as their levels say). The hint is the exe's difficulty timer: `Init_Mission02` 0x005e05a0 (table 0x005e0b9c) 120 / 180 / 360 s of game time on VERY EASY / EASY / NORMAL, **none on HARD / VERY HARD**, cancelled for good when she is found (global 0x849928e4, `Watch_Mission02_Destroyer_Find` 0x005df130); the user plays HARD (`DifficultySetting 3`), so it cannot play; the tree's generated code matches line by line. HWI-TEST: `/catMissionTestHoldScout <tick>` (the scripted player's recon waits), the Mission02 hint checks (`catMtHintWatch` / `catMtHintJudge`, the speech log `speech: event=.. tick=..`), the sensors draw counters (`CatSmDrawCounts.dustPoints`), the resources / dust-cloud logs. | met: 486 Dust points in every sensors frame (871,884 over the gate run's 1,794), the captures on / off 247-261 px at 1024x768 and 269-275 at 4K; the hint fires at tick 5824 only with the scout held on NORMAL (`hint ... expected=yes fired=yes result=PASS`) and never on VERY EASY found in time or on HARD; proven non-vacuous by two mutations; every stream, both replays, the dumps IDENTICAL | PASS-WITH-FIXES, precision 30 / 32 = 0.94 (2 report figures fixed: 485 -> 486, 0.034 % -> 0.031-0.033 %; the backdrop question added to the user's check) | `96b0137` |
| 4 | Mission03's white mines + the mine AI ([report](m3-r3-mines.md), [audit](audits/m3-r3-mines.md)) | **`HWI-FIX meshStaticTeamColor`** (`Mesh.c`; +14 `HWI-FIX` lines): the white mines and missiles were an x86-64 layout fault, not the exe's: the four team-colour reads cast every static through `ShipStaticInfo`, whose colour-scheme row sits at another offset in the 64-bit layout, so a mine's / missile's static read a wrong row (the exe reads +0xF0 of every static kind); now a missile / mine static gives its own row. Level mine `textures=11 bound=11 white=0` (was `bound=0 white=11`), laid mine `textures=4 bound=4 white=0` (was `unregistered`); FileLoads 3,170 -> 3,174 (the Taiidan mine's `PAGE0` LOD0-3; the Derelict mine's materials are the registry's shared derelict textures); the captures 19 / 6 flat colours -> 10,657 / 970, batched = `/meshImmediate` 0 px. **The mine AI is the exe's** (`aishipGuideMine` 0x00416410, `aishipFlyMineToTarget` 0x00415fa0; nothing ported): the user's "3 s then stop" is the **faster-away rule** (a mine gives up a target flying away faster than itself and brakes x0.5 a tick) plus the once-a-second search; the laid mines (600 u/s) give up on every Somtaaw strike craft flying away, the level mines (1,000 u/s) catch Acolytes (825) but not Recons (1,050); a mine whose target is destroyed or leaves lives 5 s more (the audit's lost-target lifetime, 0x008b5c68). HWI-TEST: `/catMeshTexLog <substring>` (`meshtex: register / draw`), `/catMineView <tick>[:dropped][:<distance>]`, `/catMineLog <ticks>` (`minelog: change ... why=acquired\|start\|faster-away\|range\|cloaked\|disabled\|cleared`), `/catMineChase <tick>[:<shiptype>]`. | met: proven non-vacuous by a reverted build (flat 19 / 6 colours -> 10,657 / 970 textured in the regions), batched = immediate; the Recon chase (mine 63: acquired 1295, chased at 1,000 u/s, `faster-away` 1376, stopped by 1387, re-acquired 1391, `faster-away` 1392) and the Mission03 log as the exe's machine; every stream, Missions 03 / 06 / 10 / 12 / 14, the failed jump, the team colours, both skirmishes, both replays (2823 / 2871) and the dumps IDENTICAL / EQUAL | PASS-WITH-FIXES, precision 38 / 40 = 0.95 (3 report errors fixed: six -> four mines losing the Mimic, the missing 5 s lost-target lifetime, the FileLoads attribution; 5 doc edits) | `4ae7d91` |
| 5 | the missile corvette's four trails ([report](m3-r3-engine-trails.md), [audit](audits/m3-r3-engine-trails.md)) | **The exe draws one ribbon per mex Eng chunk** (`InitializeEngineTrails` 0x00541f40; the trail pass 0x005772dd draws all four slots): two for each missile corvette (`bMissileCorvette` / `p1MissileCorvette`: 2 Eng chunks, nozzles (0, +-17.854, -45.75)), each centred between an upper and a lower painted circle; no type has more Eng chunks than the exe draws (max 3 of 4); the tree matched the exe. **`HWI-FEATURE(engine-trails)`** (113 lines; default ON): the missile corvettes draw four ribbons, one from the centre of each painted engine circle (up +-11.5, right +-17.891, the Eng chunk's z), each half the exe's 42-unit width across the pair, the exe's other parameters and glow; `/exeTrails` (run-only), the console row `engineTrails` (HWI rows 9: console shown 38 / set 31), the settings line's `engineTrails`; no cfg row, no Options control, no key; the added ribbons draw their render jitter from their own generator, never the shared random stream (the trail streams 13-17 equal at every frame with the feature on / off / reverted); **decision 84 sharpened**: the run-to-run `rng_all` noise is stream 11 (`RANDOM_SOUND_GAME_THREAD`) alone. HWI-TEST: `/catTrailView <tick>:<ShipType>[:d][:e][:s][:r][:m]` (the camera behind a ship type; the trail heads, nozzles, circles and the random streams per frame). | met: four ribbons from the four circles at 1024x768 and 4K for both corvette types (the exe's two with `/exeTrails`), the nozzles within 0.8 px of their rings' centres (3.3 px at 4K, rings 314 px across), `/exeTrails` byte for byte the reverted build and `4ae7d91`, every stream (ai, soak, skirmishes, replays 2823 / 2871, recordings, Missions 02-05 / 10-12 / 14 / 16 / 17) IDENTICAL, the dumps EQUAL (`motion-exe` = `motion-on`), the 4K cost nothing above the run-to-run spread on the idle GPU (medians 3.07-3.57 ms, p99 5.0-5.6, the 6.94 ms budget met) | PASS-WITH-FIXES, precision 34 / 36 = 0.944 (10 untagged feature lines tagged, a moved M3-R1 tag restored; tags only, the release binary byte-identical) | `6ad69ec` |

**Round size** (`git diff --stat 7aa3111..6ad69ec`, the round base to the last audit's checkpoint; 11 commits: the five ports with their
audits and six orchestrator / answers commits, `c698dd5` among them: `tools/play.sh` rebuilds a stale binary itself): 53 tracked files
changed, +5,784 / -120 lines; `src/` alone: 33 files, +2,455 / -52 (AIShip, Console, FEFlow, InfoOverlay, Instrument, KASFunc, KeyBindings,
Mesh, MissionSelfTest, MotionInterp, MotionTest, Options, OracleDump, Sensors, Trails, UnivUpdate, main, render, texreg, utility and their
headers), with 1 added line carrying `CAT:` and 1 `CAT-FIX`; no new `src/` file (the builds still link `[274/274]`); tools: `tools/m2_harness.sh`
(+36 header lines: this round's switches) and `tools/play.sh` (+69). The tree now carries 4,476 `CAT:` lines (M3-R2: 4,475), **37 lines
carrying `CAT-TODO`** (37; **28 owner tags: stage 5 23** (plain 8, multiplayer options 8, LAN 6, multiplayer 1), **M3 oracle physics 3**,
**M3 soak 1**, **M3-R4 1** (the BTG star size, re-targeted by this step: below)), **4 `CAT-PROVISIONAL`** (4), 13 `CAT-DORMANT` (13),
**196 `CAT-FIX`** (195: the live textures' re-filter), 49 `HWI-ORACLE` (49), **461 `HWI-TEST`** (286: the five ports' instruments), **76
`HWI-FIX`** (62: the 14 lines of `meshStaticTeamColor` and its four callers), 6 `HWI-EXPERIMENT` (6); the feature tags (`/bin/grep`):
`HWI-FEATURE(motion-interp)` 486 (486), `letterbox-setting` 37 (37), `full-detail` 179 (179), `ui-scale` 300 (300), **`instrumentation`
141** (102: the F key), `hyperspace-assembly` 55 (55), `mesh-batching` 160 (160), **new `sharp-textures` 196, `pause-freeze` 90,
`engine-trails` 113**: 1,748 `HWI-FEATURE(` lines in all (1,314). Tools: `tools/m2_harness.sh` (31 modes; this round's switches through
`--`: `/catMotionPause`, `/catMotionShot`, `/catOverlayKeyAt`, `/catConsoleAt`, `/catFeScreen`, `/catTexList`, `/catMissionTestHoldScout`,
`/catMeshTexLog`, `/catMineView`, `/catMineLog`, `/catMineChase`, `/catTrailView`, `/exeTrails`, `/sharpTextures` / `/noFilter`; the
`--scene` kind `mothership:<distance>`), `tools/play.sh` (rebuilds when the binary is older than the newest `src/` commit; documents
`/sharpTextures`, `/exeTrails`, `/catMineLog`, `/catFrameSig`).

## Delta map after the round

The ports recorded their comparisons as reviews in `ghidra/delta_map_review.json` and merged them with `tools/delta_map.py merge`
(decision 55; `ghidra/delta_map.json` committed by `c8f97bb`, `4ae7d91` and `6ad69ec`). Notes tagged `[review: M3-R3 <port>]`:
settings-and-keys 15 (`trRGBTextureCreate`, `trFilterEnable`, the registry walk, the key handler, the options screens), mines 6 (the
static tables, `aishipGuideMine`'s machine), engine-trails 7 (`InitializeEngineTrails`, the trail pass, `Trails.c`'s draws): **28 in all**
(M3-R2: 12); producers 2 and 3 touched no mapped body (the pause's side table is new code; Mission02 ported nothing). This step added no
review of its own and ran no merge; `python3 -I tools/m2_stage_counts.py --md` on the tree's map gives the table.

| stage | HW1 files (main ones) | IDENTICAL fn / B | MODIFIED fn / B | REWRITTEN fn / B | cat-new fn / B (est.) |
|---|---|---:|---:|---:|---:|
| 1 file formats + data load | Mesh, MeshAnim, texreg, LOD, StatScript, SaveGame, BigFile, File, ObjTypes, StringSupport, Tweak, LZSS/BitIO/CRC32 | 210 / 41,085 | 105 / 54,159 | 0 / 0 | 76 / 27,383 |
| 2 universe and ships | Universe, UnivUpdate, CommandLayer, Collision, Physics, Gun, Dock, Formation, ETG, Particle, Select/ShipSelect, Stats, ResearchAPI, Ships/* | 719 / 130,879 | 653 / 270,426 | 30 / 4,444 | 808 / 180,868 |
| 3 UI and presentation | FEFlow/FEReg, UIControls, ConsMgr, ResearchGUI, Options, NIS, sound/speech, render/GL, main/mainrgn, mouse | 845 / 182,031 | 339 / 202,253 | 14 / 8,558 | 450 / 139,196 |
| 4 AI | AI*.c | 291 / 43,874 | 186 / 79,829 | 2 / 169 | 23 / 14,404 |
| 5 LAN networking | CommandNetwork, MultiplayerGame, MultiplayerLANGame, TitanNet, NetCheck, ChannelFSM, chat | 404 / 69,517 | 116 / 76,194 | 12 / 1,179 | 67 / 17,483 |
| 6 missions | KAS, KASFunc, Timer, Objectives, LevelLoad, SinglePlayer, Tutor + kas2c mission code | 288 / 19,469 | 181 / 53,276 | 25 / 1,505 | 2,365 / 656,918 (2,085 / 612,687 B are kas2c mission functions) |
| **total** | | **2,757 / 486,855** | **1,580 / 736,137** | **83 / 15,855** | **3,789 / 1,036,252** |

IDENTICAL 2,757 / MODIFIED 1,580 / REWRITTEN 83 (M3-R2: 2,757 / 1,580 / 83: **no body changed class this round**; the mines fix edits
the recreation's own `Mesh.c` reads, the trails feature its own `Trails.c` wrapper, both noted on their rows), 3,582 reviewed rows
(M3-R2: 3,579); **210 MODIFIED / REWRITTEN rows still without a `reviewed` flag (118 outside `TitanInterface.cpp`**: ETG.c 41, Particle.c
38, ObjTypes.c 16, MultiplayerLANGame.c 7, MultiplayerGame.c 5, GameChat.c 3, StatScript.c 3, one each in Captaincy.c, CommandNetwork.c,
File.c, TitanNet.c and ddraw.cpp; unchanged from M3-R2). Bodies compared with the exe this round and found as the tree (a note, no class
change): the settings port's texture registry (`trRGBTextureCreate` 0x005866a0, `trFilterEnable` 0x005888e0 against HW1 `texreg.c:3370`,
`opEffectsHelper` 0x004d9b90, the key handler 0x00563b50), the Mission02 port's sensors scene (`cat_smViewportRenderScene` 0x0050b4b0,
`cat_smResourcesDraw` 0x0050ae00, `Init_Mission02` 0x005e05a0, `Watch_Mission02_Destroyer_Find` 0x005df130: the audit's `cloudRenderSystem`
0x00438640 observation for the owner of the cloud look), the mines port's AI (`aishipGuideMine` 0x00416410, `aishipFlyMineToTarget`
0x00415fa0, `univRemoveAllTargetReferencesFromMissiles` 0x00547570) and the trails port's `InitializeEngineTrails` 0x00541f40 / the trail
pass 0x005772dd / `trailUpdate` 0x005a4e10.

## What is left

Nothing blocks the next play-test; each item has an owner (the play-test, M3-R4 if the play-test reports anything, the M3 soak, M3 oracle
physics, the Windows 11 test, the user, stage 5, or after M3):

1. **From this round's ports** (the five reports' "What is left"):
   - **settings-and-keys**: the attended check (F1 in every screen, the sharp look at the Kuun-Lan's closest zoom, TEXTURE MEMORY HI at
     4K: the brief); **TEXTURE MEMORY's default** (`TextureDetail 50`: the exe's 6 MB fitting halves 462 of Mission01's 2,343 live
     textures, 35 of the Kuun-Lan's 58 LOD-0 ones; 100 shrinks none; a default change moves every capture and dump with a shrunk texture in
     view): the user's (question 1), then M3-R4; **the minification** (GoK's mip maps and 8x anisotropy against the exe's unfiltered
     minification, in both looks): the user's (question 2), no port without a "the exe's"; the `CAT-TODO` at `BTG.c:1151` re-targeted by
     this step (below).
   - **pause-freeze**: the camera chase's 1-ulp alternation at a converged zenith camera (the audit's 0.005 px, this step's 0.0019 px; the exe's easing; no owner); a
     wall-clock (`--real-clock 144`) capture of a pause was not run (the logic does not depend on the clock): the next fix round only if
     the user still sees motion while paused; **the exe's float and glow shimmer during P** (held still now): the user's (question 3), a
     setting if the user wants the exe's look back.
   - **mission02**: **the sensors manager's points at 4K** (the Dust, the resource and ship dots: 1 physical pixel as the exe, or the UI
     scale's 2x2): the user's (question 4; the same kind as the BTG star size); the open question whether the dust clouds were missing
     in the 3D view too or only on the tactical map (Mission02 has none in either) and whether the remembered belts are the sensors
     backdrop band (`ShowBackground 1`, drawn); the Mission02 self-test's scripted player does not finish on HARD (`m02-hint-h`, SalvagePod;
     the self-tests play NORMAL): M3 tests only if a HARD run is ever wanted; the audit's `cloudRenderSystem` 0x00438640 note for the
     owner of the cloud look (Missions 04 / 06 / 08) if it is ever compared.
   - **mines**: the Beast mine has no texture in the game data (plain in the exe too; no mission places one: none); the missiles'
     textures registered for the right schemes (the draw log) but not captured in flight (the trails port's captures show the corvettes,
     not a missile: the brief asks); the other x86-64 casts of a non-ship static through `ShipStaticInfo` beyond the five texture reads
     (M3 oracle physics, layout audits, if wanted).
   - **engine-trails**: the user's look at the four streams (the brief); other ships with painted multi-engine panels not surveyed (a
     row of `hwiTrailSplits[]` each, measured as in the report's section 1; M3 polish only if the user names a ship).
2. **M3-R2's items owned by M3-R3**: **done** this round: the overlay's F key (item 0: F1), the toggle self-test's `/letterboxBehind` check
   (the switch-label rule; the toggle self-test PASS under every switch of the gate, this step's 10 runs), and the seven items of
   `cycle/answers/m3-r3-user.md` (1 and 2 by producer 3, 3 by producer 1 as SHARP TEXTURES, 4 and 6 by producer 4, 5 by producer 2, 7 by
   producer 5). **Not taken, and why**: the **flicker's verdict** waits for the play-test's `frame.sig` (M3-R2's item 2: not reproduced; no
   file handed over yet); the **BTG background-star size** (`btgConvertStar`: the exe's star a fixed share of the screen, GoK's a fixed pixel
   size) is a look decision the user has not answered (M3-R1's question 5, through three rounds): its `CAT-TODO(M3-R3)` at
   `src/Game/BTG.c:1151` is re-targeted by this step to **`CAT-TODO(M3-R4)`** (a comment-only edit, rebuilt with 0 warnings); the **letter
   height** (GoK's 2.39:1, 277 rows at 4K, against the exe's 42) and the **exe's additive state after full bars** are the user's calls
   (M3-R1's questions 2 and 3: M3-R4 one-liners if the user picks the exe's); the **effects cap / effect level 0** under FULL DETAIL and
   **trails at the LOD-0 style** stay the user's calls (question 4); the **sensors map's lines at 200 %** and the **tutorial's
   `TUT_PointerTypeShips` pointer** wait for the user to see them; the **task clock** (`CAT-TODO(M3 soak)`), **frame pacing at 4K**, the
   heap's growth over the `sp` loop and the **`DEADLYSIGNAL` in a timeout-kill quit** stay the M3 soak's (this step's two 1,200 s `sp` runs:
   no `DEADLYSIGNAL` line in either `sp` run's timeout-kill quit, nor in any of the 225 runs); the **colour-info leak O4** is the M3 soak's with producer 1's counter; **the p99 on a loaded machine** (M3-R2's fence 4)
   is the soak's attended hour: this step's 4K numbers come from the idle GPU after the correctness chains (fence 5).
3. **The oracle's rung** (M3 oracle physics; unchanged this round): 10-17 tick 2823 `ships_vel` (ship 48) and 10-18 tick 2871 `ships_vel`
   (ship 49), both a bProcessor following the bWorker 44 by `protectShip`'s follow-behind point (decision 100); the three `CAT-TODO(M3
   oracle physics)`, the other inlined `frandom` sites, Mission17's self-targeting KAS teams, `hsStart`'s `GL_BLEND` test; now also the
   layout audit of the non-ship statics (item 1, mines).
4. **The mode-B rung** (the user's session, decision 60): the GOG playback of `hwirec01` / `hwirec02` with the DLL; still no
   `original/oracle/hwirec*-play-*` directory (this step's `ls`).
5. **Decision 48** (the user's): no recording with the fixed DLL (the newest `original/oracle/oracle01-rec-*` is still
   `20261019-143530`); `ships_rot` stays excluded in every session.
6. **The user's play-tests**: the third play-test's seven items are closed by this round (each either the exe's, shown from its machine
   code, or fixed with a fence); the attended re-check of the five items is the brief; **M3-R2's four checks were never reported** (the
   overlay's numbers at 4K, the quit after a resolution change, the cutscene flicker's `frame.sig`, the hyperspace arrival, FULL DETAIL
   batched against `/meshImmediate`): the brief carries them again.
7. **The Windows 11 test** (docs/m3-windows-build.md): **a fresh zip from this round's tree**, built by this step:
   `dist/Homeworld-Infection-win64-20261026-6ad69ec.zip` (2,912,703 B, sha256 `3d41aaefccb560b2fe56d639a81bb9f834d17520515fe18838db6619b512c195`;
   `hwi-engine.exe` imports only `SDL2.dll`, `OPENGL32.dll` and Windows' own; the earlier `20261021-eb5f7e3` zip is M3-R1's source);
   untested; the user's test list is the doc's "What the user must test on Windows 11 before sharing".
8. **Stage 6's play-path items for M3** (unchanged from R10 / M3-R1 / M3-R2): the attended animatic check (the brief), the mission
   self-test's combat (M3 test), Mission17's self-targeting KAS Taiidan teams (M3 missions / oracle physics), the chained campaign's heap
   (M3 soak: measured by the frame log), `trTextureRegister`'s name case (M3 render), decision 94's nebula `CAT-FIX` (the user's).
9. **Stage 5** (after M3, decision 9): the 23 `CAT-TODO(stage 5 ...)` sites, unchanged; the modular mothership's multiplayer arrival.
10. **Stage 4 / 3 leftovers that stay**: `gunResetGimbleGuns` uncalled (58), the five creators' default tactics (59), the folded handlers'
    save indices (63), HW1's cursor text (65), `rndLoadTarga`'s `GL_CLAMP_TO_EDGE` and the Transformer (28); **closed this round**: the
    overlay's key, the toggle check, the pause jitter (a recreation fault of M3-R1's Smooth motion, now frozen), the white mines (an x86-64
    layout fault), the one-ribbon corvettes (the exe's; four behind a default-on feature); **shown to be the exe's, nothing to close**:
    Mission02's dust points and the hint's timer, the mine AI; **not closed**: the one-frame flicker (M3-R2; not reproduced; the instrument
    is in the user's hands).
11. **Motion interpolation** (decision 46): the `/noInterp` dumps' content unchanged in every column since M3-R1 (the dumps table); the
    pause now frozen (item 5 closed); the clamp blend measured in M3-R1; the droid blend still unmeasured.
12. **The delta map's review flags**: 210 MODIFIED / REWRITTEN rows without a `reviewed` flag (118 outside `TitanInterface.cpp`; unchanged).
13. **Still open, the user's**: the two saves of the M0 play-test (8), the play-test sign-off (the brief below), the fourth recording (5),
    the mode-B playback (4), decision 94's `CAT-FIX`, M3-R1's questions 2-5 and 7-10, M3-R2's four checks, and this round's questions
    (below).

## Plan for M3-R4 and the rest of M3 (replaces M3-R2's "Plan for M3-R3" table)

The order the user set still holds ("we need to fix bugs before we get further from the mark"): this round closed the third play-test's
seven items and the two carried from M3-R2 (the F key, the toggle check); **there is no queue for a fourth fix round yet**: M3-R4 exists
when the next play-test reports something, or when the user picks "the exe's" on one of the open look questions (the BTG star size, the
letter height, the additive state, the sensors points' size, TEXTURE MEMORY's default, the minification, the pause's float). The platform
rows (the Windows 11 test, now with a fresh zip; the soak) and the oracle's rung are unchanged in scope. The play-test can start now with
`tools/play.sh` (it rebuilds the release binary itself since `c698dd5`; the brief below): the five items are short attended checks.

| # | step | scope | counts | done when |
|---|---|---|---|---|
| 0 | **the user's next play-test** (now; the brief below) | the five items re-checked (F1 in the game, the sensors manager and the build screen; SHARP TEXTURES at the Kuun-Lan's closest zoom and TEXTURE MEMORY HI at 4K; the pause with P and with the Escape menu over moving ships; Mission02's Dust points at 4K and the hint on NORMAL with the BUSHAN-RE left alone 6 minutes; Mission03's mines textured, the mines' faster-away rule, the missile corvettes' four streams), then M3-R2's four unreported checks (the overlay's numbers and the quit at 4K, the cutscene's `frame.sig`, the hyperspace arrival, FULL DETAIL batched against `/meshImmediate`), then M3-R1's open items (the bars' height, the colours after Mission03, the far zoom, the UI at 4K, the sensors map's lines, the stars' size) and R10's (the animatics, the campaign, the tutorial, a skirmish, a recording) | 1-3 hours | a report per item and the `frame.sig` handed over; M3-R4 sized from it (or not needed) |
| 1 | **M3-R4: the fourth fix round** (one producer + auditor per item, as this round; only if row 0 reports items or the user picks "the exe's") | the play-test's new items in the user's order; the look decisions if answered the exe's way: the BTG star size (`CAT-TODO(M3-R4)`, `BTG.c:1151`), the letter height, the additive state after full bars, the sensors points at the UI scale, TEXTURE MEMORY's default HI, the exe's unfiltered minification, the pause's float and glow shimmer behind a setting; the flicker's verdict from the `frame.sig` (`--driver wayland` as `play.sh`'s default or a swap-interval setting if the presentation is confirmed); a wall-clock pause capture if the user still sees motion while paused | 0-6 items; 0-4 producers | every item either shown 1-to-1 with the exe (with the exe's address and the script's line) or fixed with a fence; every stream IDENTICAL; the play-test's items re-played |
| 2 | **the Windows 11 test** (the user's day + a producer for the findings; M3-R1's row 1, M3-R2's row 2) | the fresh win64 zip `20261026-6ad69ec` (sha256 `3d41aaef…`, this round's tree: the F key, SHARP TEXTURES, the pause freeze, the mines fix, the four trails in it) tested per docs/m3-windows-build.md "What the user must test" (launch, the front end, Mission01, a save, a recording, the 4K window); the harness modes the gate needs under Git Bash / MSYS2; the libm dependence measured first | 1 day + 1 producer | release + debug on Windows 11 with 0 warnings; the self-tests PASS there once; the Mission01 stream, both replays and a mission stream IDENTICAL to Linux (or every difference localised to a libm call); a save and a recording cross-load |
| 3 | **the one-hour 4K@144 soak, no gamescope** (producer + the user's hour; M3-R1's row 2, M3-R2's row 3) | the headless `sp` hour with `/frameLog` (the drift test, the heap growth a minute: ~4 MB/min in M3-R2's 600 s `sp`, the `dupe trcolorinfo list` blocks, O4), the task clock (`CAT-TODO(M3 soak)`), the `DEADLYSIGNAL` in a timeout-kill quit, FULL DETAIL's p99 on a quiet machine (this step's idle-GPU figures are the baseline: fence 5), the static per-object buffers if the attended hour needs them, then the attended hour at 3840x2160@144 with the overlay (F1) and `/frameLog` | 1 producer + the user's hour | no frame-rate drift over the attended hour (`frame_log_report.py`'s drift test PASS); no growth heap in the headless hour or every growth attributed and fixed; the task clock at the exe's rate or its difference stated |
| 4 | **the oracle's rung** (M3 oracle physics; M3-R1's row 3, unchanged) | 10-17 2823 / 10-18 2871 (the Worker's dock / harvest heading paths against the exe), the three `CAT-TODO(M3 oracle physics)`, the other `frandom` sites, Mission17's self-targeting KAS teams, `hsStart`'s `GL_BLEND` test, the non-ship statics' layout audit; mode B and the fourth recording if the user made them | 1 producer round | the replays IDENTICAL beyond 2823 / 2871 or the next divergences localised and owned |
| then | **stage 5** (decision 9) | the 23 `CAT-TODO(stage 5 ...)` sites: the LAN game's senders and keep-alive, the options word, the `/packetRecord` header, the Titan / WON stubs; the modular mothership's multiplayer arrival | 23 sites + 1 | LAN interop with stock 1.01 nice-to-have (decision 9); the wire format not broken |

**Then**: the stage-5 remainder per decision 9 after M3, or an oracle-physics round only if the user wants the rung before the next
play-test. **Rules carried into M3-R4** (decisions 121 and 132, plus this round's): every port on the plain harness with a clean build;
every stream judged with `hwo_compare.py` in all 21 fields (never by sha256 across builds); the dumps compared whole and crc-stripped (a
crc-only move is a stated render change: this round's sharp and `/exeTrails` variants; a non-crc move is a render-timing change and must
be stated with its frame count); GPU measurements on an idle GPU only, with the CPU load stated, and a frame-time p99 only from a quiet
CPU (this step ran its GPU batch after the correctness chains, the GPU batch waited from 11:49 to 14:55 for the user's game to release the GPU (`gpuwait`: nvidia-smi every 10 s until 120 s at or under 40 %), then ran its eight runs on the idle GPU in two batches of four, the second after a two-minute wait on a 50 % sample; `claude-ping` fired 2 times (11:49:21, 14:57:12): the library pings once per `gpuwait` call, not once per gate, a deviation from the user's rule to fix in `chains/lib.sh` before M3-R4); a "not reproduced" item's done-when is its instrument proven
on a forced frame plus the attended run's file; each step's labels get their own prefix (the harness replaces an existing label's
directory); a gate self-test runs under every switch the gate names before a round closes (10 toggle runs here); **every
producer-added setting is flipped on the Mission01 stream by the integrate** (this round: the cfg row alone and with `effectsLevel 2`,
`/sharpTextures`, `/noFilter`, `/exeTrails`, the console rows `engineTrails` and `texLinearFiltering` live, the F key pressed); **a
paused dump is a gate** (`/catMotionPause` in the motion, nis and scene runs: every paused pair equal in every world column, the camera's own chase stated); **a port's
captures are compared byte for byte with the integrate's** (every PPM of the five fences); the user's answers in
`cycle/answers/<round>-user.md`, read again before the report; the "USER:" questions of a report quoted verbatim in the summary.

## What the user must decide

Every default is in the tree; say if you want otherwise. The five reports' "THE USER'S CHECK" sections and their "USER:" questions are
quoted verbatim (the audits' `[audit: ...]` insertions kept), in the ports' order; then what `cycle/answers/m3-r3-user.md` already
answered, M3-R2's and M3-R1's open questions, and the standing items.

1. **Producer 1 (settings-and-keys): the questions** (questions 1 and 2 of the round: TEXTURE MEMORY's default, the minification), verbatim:
   > 1. USER: the TEXTURE MEMORY slider (Options -> Video, front end) is in the middle by default (`TextureDetail 50`, your file
   >    too); the original game then loads 462 of Mission01's 2,343 textures at half size, 35 of the Kuun-Lan's 58 hull
   >    textures among them (a 1999 video-memory budget), which is most of the "more blurry" mothership at 4K. Should the
   >    recreation default it to HI (100: nothing shrunk), or keep the original game's middle and leave HI to you?
   > 2. USER: at a distance, this engine smooths textures with mip maps and 8x anisotropic filtering (from the Gardens of Kadesh
   >    port); the original game drew distant textures without mip maps (crisper but shimmering when the camera moves). Keep the
   >    smoother distance look for both the original and the sharp setting, or should "the original texture look" be the
   >    original game's exact distance look too?

   Its check, verbatim:
   > 1. **Start**: `tools/play.sh --rebuild`. The log's first `hwi-settings:` line now ends with `... meshImmediate=0 (default)
   >    texLinearFiltering=1 (cfg) fit=fullscreen-desktop ... 3840x2160 switches=none [start]`. Report it if it says anything
   >    else.
   > 2. **The overlay's key, F1**: in the front end and in Mission02 or later, press **F1**: the dark panel appears top-left;
   >    F1 again hides it. The Menu key still works too. Check that Shift+F1 (Leech_Spy), Ctrl+F1 (the sensors' areas visited)
   >    and F2..F12 (tactics, formations; F12 = parade formation) still do what they did. Report: does F1 toggle it in the game,
   >    in the sensors manager, in the build screen; does it ever do anything else.
   > 3. **SHARP TEXTURES, the original look first**: in a mission, zoom all the way in on the Kuun-Lan (its closest zoom) and look
   >    at the hull plates. This is the original look (smooth). Then press Escape -> Options -> Video and tick **SHARP
   >    TEXTURES** (top middle of that screen): the hull should change behind the menu (or at once when you close it with
   >    ACCEPT): each texel a square block (about 5-8 pixels at 4K). The EFFECTS row of the same screen switches to CUSTOM
   >    (that is how the original game keeps its own SURFACE FILTERING box: it is the same setting). Press ACCEPT. Report:
   >    which look you prefer; whether anything else on screen changed (it should not: only the ship / derelict / asteroid mesh
   >    textures [audit: said "ship / asteroid / effect": the effect sprites and the trails' billboard sprites stay smooth, as in the exe]; the HUD, fonts, menus and the starfield keep their own look); whether the log has `texfilter: filter=sharp
   >    textures=N globjects=N wrong=0` (wrong must be 0).
   > 4. **The blur at 4K, the TEXTURE MEMORY slider**: still on Options -> Video (the front end's screen: the TEXTURE MEMORY slider
   >    bottom left, in the middle by default), slide it to **HI** and ACCEPT; restart the game (the textures are sized when a
   >    level loads). Look at the Kuun-Lan again with the original look: the hull should be noticeably less soft (the middle
   >    setting loads 35 of its 58 hull textures at half size: a 1999 memory budget). Report whether HI fixes the "more blurry"
   >    look for you, and whether you want HI as the recreation's default.
   > 5. **A run-only check without the menu**: `tools/play.sh -- /sharpTextures` starts sharp for that run only (your
   >    Cataclysm.cfg keeps its rows); the log's line says `texLinearFiltering=0 (switch) ... switches=/noFilter`.

   Defaults if unanswered: TEXTURE MEMORY stays the exe's middle (`TextureDetail 50`); the minification stays GoK's mip maps and 8x
   anisotropy in both looks (decision 28); the original look stays the default; F1 stays the key (the Menu key a second toggle).
2. **Producer 2 (pause-freeze): the question** (question 3: the pause's float and shimmer), verbatim:
   > USER question: the exe keeps the camera's gentle float and the engine glows' shimmer going while P pauses the game;
   > the recreation now holds them still (the Escape menu already stopped the float in the exe). Keep the still pause (the
   > default now), or bring the exe's look back behind a setting?

   Its check, verbatim:
   > `tools/play.sh` (it rebuilds the release binary when it is older than the newest src/ commit; Smooth motion on, the
   > default). Any mission with moving ships, e.g. **Mission02 or Mission03 with the fleet moving** (or a skirmish): focus the
   > camera on a moving ship (or a moving group), zoom in so the ships fill the view, then press **P** (the game's Pause key;
   > "PAUSED" appears). Look at the moving ships, their engine glows and trails, and the background while paused for a few
   > seconds; then press **P** again. Also open the **Escape** menu (universePause) over moving ships and close it.
   > Report: (1) while paused, does anything in the 3D view still move or shimmer (ships, the camera, engine glows, trails)?
   > (the HUD's blinking red SU number when over the unit cap, the selected ship's turning picture and the upgrade pulse
   > keep animating, as in the original); (2) on the unpause, do the ships jump, step back, or just resume (a brief catch-up
   > of under a tenth of a second is expected); (3) the same in the Escape menu. Compare, if wanted, with `tools/play.sh --
   > /noInterp` (Smooth motion off): the ships stand still there too, the camera float and the glows likewise.

   Default if unanswered: the still pause (always on, no setting).
3. **Producer 3 (mission02): the question** (question 4: the sensors points' size and the dust-belt question; its "What is left"), verbatim:
   > **USER question (look decision, no owner until answered):** draw the sensors manager's points (the Dust, the
   > resource and ship dots) at the UI scale's size (2x2 physical pixels at 200%) so they read at 4K as they did at
   > 1024x768, or keep the exe's 1 physical pixel. Same kind of decision as the background star's size (BTG.c:1151);
   > default stays the exe's until the user picks.

   and its open question: *"were the dust clouds missing in the 3D view too, or only on the tactical map? (Mission02 has none in
   either; if the user meant something in the 3D view, a capture of what was expected would locate it.)"* Its check, verbatim:
   > 1. **The dust belts** (Mission02, any difficulty): `tools/play.sh`, load or reach Mission02, open the sensors manager
   >    (Space) at once, before exploring. Look for the dotted arcs of single faint tan-grey dots (the "Dust": the decorative
   >    asteroid belts, 486 of them), drawn over the whole map from the first moment, unlike the brighter resource dots that
   >    only appear where your ships have been. Right-click in the sensors manager, "Sensors", "Dust" toggles them (on by
   >    default; your options file has `ShowAsteriod0 1`). **Report**: whether you can see them at 3840x2160 at all, and
   >    whether you want them bigger (the question below). If you want a side-by-side, `tools/play.sh -- /uiScale 100`
   >    changes only the 2D, not these points; a 1920x1080 window (`screenWidth 1920` / `screenHeight 1080`) shows them at
   >    twice the relative size.
   >    [audit: also tell us whether what you remember as "dust belts" is the broad grey-blue band across the sensors
   >    manager's backdrop (the Mission02 background, "Background" in the same "Sensors" menu, cfg `ShowBackground 1`): it
   >    is drawn in the tree's captures (`original/scratch-m2/m3r3a-mission02/on6072.png`), so it is not missing either.]
   > 2. **The BUSHAN-RE hint**: it **cannot** play on HARD (your options file: `DifficultySetting 3`) or VERY HARD, in the
   >    original game either. The difficulty in effect when Mission02 BEGINS decides it: the hint's timer is made (or not)
   >    by Mission02's script at its start and is kept in every save made during Mission02, so a Mission02 save begun on
   >    HARD has no hint whatever the difficulty is now (loading a save keeps the current difficulty setting, kasLoad
   >    0x00491450, but not a timer that was never made). To hear it: pick NORMAL (or EASY / VERY EASY) on the front end's
   >    GAME DIFFICULTY buttons, then start a new campaign or load a save from Mission01 (e.g. "01 - Hiigara") and jump into
   >    Mission02; there keep every ship away from the BUSHAN-RE for 6 minutes of game time (EASY: 3, VERY EASY: 2; pausing
   >    stops the clock). Fleet Intelligence then speaks, the game pauses, the sensors manager opens centred on her with three
   >    ping circles around her. **Report**: whether it played and where the pings were.

   Default if unanswered: the exe's 1-pixel points.
4. **Producer 4 (mines): the check**, verbatim (no "USER:" question):
   > `tools/play.sh` (it rebuilds when the binary is older than the newest src/ commit), play into **Mission03**:
   >
   > 1. **The mines' textures (item 4).** Once the fleet has arrived, look at a minefield up close (the level's mines sit in
   >    clusters on the way to the CLEE-SAN; select one or zoom the camera to it) and later at the mines the minelayers
   >    drop. Expected: the level mines are grey-brown metal with vents and panels; the laid mines are square with
   >    yellow team-coloured corners. Report any mine still flat white, and whether missiles in flight (the player's Acolytes',
   >    the P1 missile corvettes') now show textures too (they had the same fault).
   > 2. **The mines' behaviour (item 6), unchanged, the exe's.** Fly a group past a minefield: the mines chase while you come
   >    towards them and **stop and wait when a faster ship flies away from them**, then pick it up again if it turns back.
   >    The laid mines (600 u/s) give up on every Somtaaw strike craft flying away; the level mines (1,000 u/s) catch Acolytes
   >    but not Recons. Report a mine that stops while its target is slower and still coming towards it (that would be a
   >    fault). [audit: a mine whose target is destroyed (or leaves) disappears about 5 s later even while it chases a new
   >    target: the exe's lost-target lifetime, not a fault.] Optional evidence: `tools/play.sh -- /catMineLog 480` writes the `minelog:` lines into the play log.

   Nothing to decide: the textured look is the exe's and the default; the AI is the exe's.
5. **Producer 5 (engine-trails): the check** (question 5: the four streams' look), verbatim (no "USER:" question; the default is a look choice):
   > `tools/play.sh` (the release build rebuilds itself when older than src/), then Mission03 (or any later mission with
   > the Turanic raiders' or the Beast's missile corvettes; skirmish against the Beast works too). When a missile corvette
   > flies (Mission03: when the raiders attack), bring the camera close to it and look at it from behind and a
   > little above: **four jet streams, one from the centre of each of the four glowing engine circles on its rear**, each
   > about as wide as its circle; the engine glow as before. For the A/B, start once with `tools/play.sh -- /exeTrails`:
   > the two broad ribbons of the original game, each centred between an upper and a lower circle. Report: whether the four
   > read as four from the angles you play at, whether any looks offset from its circle, and whether you prefer them (the
   > default stays ON unless you say otherwise). The log's settings line says `engineTrails=1 (default)` (`0 (switch)` with
   > /exeTrails).

   Default if unanswered: the four ribbons ON (`/exeTrails` for the exe's two).
6. **What `cycle/answers/m3-r3-user.md` already answered** (applied as defaults): the mandate *"yeah lets get r3 underway. also I want
   to implement a new option to support sharp texture meshes or the original texture look"* (SHARP TEXTURES is the option, the original
   look the default: producer 1); the overlay's key *"let's dedicate a key from the F1-F12 suite for this overlay"* (F1, verified against
   the exe's key tables; the F12 candidate was wrong: plain F12 is Formation_Parade); the GPU free (*"okay you can use the gpu now if its
   needed"*, 2026-10-26: this step's GPU batch waited for the idle check all the same); the seven items of the third play-test, each now
   either the exe's or fixed (the ports table). Not answered: M3-R2's four checks and M3-R1's questions: the defaults stand.
7. **M3-R2's four checks, still unreported** (docs/m3-r2-summary.md "What the user must decide" 1-4; the brief carries them again):
   the flicker's two hints and the `frame.sig`; the overlay's attended numbers and the quit after a resolution change at 4K; the
   hyperspace arrival's look; the batched frame's look and figures. Defaults unchanged (the presentation hypothesis; batching ON; the
   exe's gate and the assembly layer ON).
8. **M3-R1's questions still open** (docs/m3-r1-summary.md "What the user must decide"): **2** the letter height at 16:9 and wider
   (GoK's 2.39:1, 277 bar rows at 4K, default; say "the exe's" for its 42 rows: M3-R4); **3** the exe's additive blending after full bars
   (default the port's normal blending; say "the exe's"); **4** FULL DETAIL (met on an idle machine; trails at the LOD-0 style or the
   exe's level-following trails (default); the effects cap lifted or left (default); the sensors manager's blips stay blips); **5** the
   UI scale and the window (confirmed in use); the BTG star size (now `CAT-TODO(M3-R4)`: the exe's fixed share of the screen or the
   fixed pixel size you have now); the sensors map's thin lines at 200 % (and now its points: question 3 above); **6** nothing (team
   colours); **10** the defaults recorded as decisions 111-121, 122-132 and now 133-144.
9. **Decision 48** (the fourth recording with the DLL's `CAT_SHIP_coordsys` 0x98 edit): still none (the newest `original/oracle/
   oracle01-rec-*` is 10-19's); both open divergences point at the bWorker 44's coordsys, which only this recording makes comparable.
   Recommendation unchanged: before M3's rung producer runs (row 4).
10. **The mode-B playback** of `hwirec01` / `hwirec02` (the user's session; the commands in docs/m2-r10-summary.md "What the user must
    decide" 2 still apply: the right-hand sides are now this step's `m3r3i-e-play01-a` / `m3r3i-e-play02-a`, IDENTICAL to M3-R2's,
    M3-R1's and R10's in every field).
11. **The two saves of the M0 play-test** (8: one mission, one skirmish): still open.
12. **The defaults recorded as decisions 133-144** in cycle/STATE.md (say if you want otherwise), and the order: the play-test first
    (short: five attended checks), M3-R4 only from what it reports (or from a "the exe's" on a look question), the Windows 11 zip
    whenever you have the machine, the soak after (recommendation).

## The play-test brief, refreshed (replaces M3-R2's; this round's five items first, then M3-R2's four unreported checks, then M3-R1's and R10's open items)

An attended run is the only one that may be audible or on screen (docs/CONSTRAINTS.md 5). Launch with `tools/play.sh` (read its header:
the release build at 3840x2160 borderless fullscreen-desktop by the fit rule, **rebuilt by the script itself when the binary is older
than the newest `src/` commit** (since `c698dd5`; `--rebuild` forces it, `--debug` for the ASan build when a crash needs a report); the
harness data dir; the private home `original/play-home`, whose `.homeworld/Cataclysm.cfg` keeps your colours, `noLOD 1`, `screenWidth
3840` / `screenHeight 2160` / `fullScreen 0`, `objInterpolate 1` / `nisInterpolate 1`, `texLinearFiltering 1` and `DifficultySetting 3`;
engine switches after `--`; the log under `original/play-home/logs/play-<timestamp>.log`; **no gamescope**):

```
cd ~/Documents/GameDev/Homeworld-Infection          # the main checkout (main = m2 at 48c5a23)
tools/play.sh                          # 3840x2160 borderless fullscreen-desktop; rebuilds itself; switches after --
```

[2026-10-26, after the round: the brief first said the m2 worktree; a worktree's `original/` has no game data and no play
home, so a launch from there rebuilt the engine and stopped at "no Cataclysm data". `tools/play.sh` now falls back to the
main checkout's `original/` from a worktree, so either directory works; the main checkout is the plain way.]

Report per item:

1. **The overlay's F key (item 0).** The newest `original/play-home/logs/play-*.log` starts with an `hwi-settings:` line ending `...
   meshImmediate=0 (default) texLinearFiltering=1 (cfg) engineTrails=1 (default) fit=fullscreen-desktop (display size -> fullscreen
   desktop) 3840x2160 switches=none [start]`; report it if it says anything else. In the front end and again in Mission02 or later press
   **F1**: the dark panel appears top-left (`FRAME <ms> MED <p50> P99 <p99>`, `TICK`, `ALPHA`, the settings row, `VERTS`); F1 again hides
   it; the Menu key still toggles it too. Check that Shift+F1 (Leech_Spy), Ctrl+F1 (the sensors' areas visited) and F2..F12 (tactics,
   formations; F12 = parade formation) still do what they did. Report whether F1 toggles it in the game, in the sensors manager and in
   the build screen, and whether it ever does anything else; and, since M3-R2's check was never reported, the `FRAME ... MED ... P99`
   numbers during the Kuun-Lan close-up and with the whole fleet in view in Mission01 (above 6.94 ms misses 144 Hz) and whether `ALPHA`
   moves between 0 and 1 (Smooth motion on).
2. **SHARP TEXTURES and the 4K blur (item 3, your mandate).** In a mission, zoom all the way in on the Kuun-Lan (its closest zoom) and
   look at the hull plates: this is the original look (smooth; the default). Escape -> Options -> Video, tick **SHARP TEXTURES** (top
   middle of that screen; the EFFECTS row switches to CUSTOM: the original game keeps its own SURFACE FILTERING box there, it is the same
   setting), ACCEPT: each texel a square block (about 5-8 pixels at 4K). Report which look you prefer; whether anything else changed
   (it should not: only the ship / derelict / asteroid mesh textures; the effect sprites, the trails' sprites, the HUD, fonts, menus and
   the starfield keep their own look); whether the log has `texfilter: filter=sharp textures=N globjects=N wrong=0`. Then, for the blur
   you reported on 10-25: on the front end's Options -> Video slide **TEXTURE MEMORY** (bottom left, in the middle by default) to **HI**,
   ACCEPT, restart the game and look at the Kuun-Lan again with the original look: the hull should be noticeably less soft (the middle
   setting loads 35 of its 58 hull textures at half size, a 1999 memory budget). Report whether HI fixes it for you and whether you want
   HI as the recreation's default (question 1). A run-only sharp start without the menu: `tools/play.sh -- /sharpTextures` (the log's
   line says `texLinearFiltering=0 (switch) ... switches=/noFilter`).
3. **The pause (item 5).** Any mission with moving ships (Mission02 or Mission03 with the fleet under way, or a skirmish): focus the
   camera on a moving ship or group, zoom in so the ships fill the view, press **P** ("PAUSED" appears). Look at the ships, their engine
   glows and trails, and the background for a few seconds, then press P again. Also open the **Escape** menu over moving ships and
   close it. Report: (1) while paused, does anything in the 3D view still move or shimmer (ships, the camera, engine glows, trails)?
   (the HUD's blinking red SU number when over the unit cap, the selected ship's turning picture and the upgrade pulse keep animating,
   as in the original); (2) on the unpause, do the ships jump, step back, or just resume (a brief catch-up under a tenth of a second is
   expected); (3) the same in the Escape menu; (4) whether you want the original's camera float and glow shimmer back during P
   (question 3). `tools/play.sh -- /noInterp` (Smooth motion off) for a comparison: the ships stand still there too.
4. **Mission02: the Dust points and the BUSHAN-RE hint (items 1 and 2).** Reach Mission02 (a new campaign or the "01 - Hiigara" save and
   the jump), open the sensors manager (Space) at once, before exploring: the dotted arcs of single faint tan-grey dots are the "Dust"
   (the decorative asteroid belts, 486 of them), drawn over the whole map from the first moment, unlike the brighter resource dots that
   only appear where your ships have been; right-click, "Sensors", "Dust" toggles them (on by default; your options file has
   `ShowAsteriod0 1`). Report whether you can see them at 3840x2160 at all and whether you want them bigger (question 4: the UI scale's
   2x2 pixels or the exe's 1); whether what you remembered as "dust belts" is instead the broad grey-blue band across the sensors
   manager's backdrop (the Mission02 background, "Background" in the same menu, `ShowBackground 1`; drawn), and whether the dust clouds
   you missed were in the 3D view (Mission02 has none in either view; Missions 04 / 06 / 08 have them). The hint **cannot play on HARD**
   (your `DifficultySetting 3`) or VERY HARD, in the original game either; the difficulty in effect when Mission02 **begins** decides it
   (a Mission02 save begun on HARD has no hint whatever the setting is now). To hear it: pick NORMAL (or EASY / VERY EASY) on the front
   end's GAME DIFFICULTY buttons, start a new campaign or load "01 - Hiigara" and jump into Mission02, then keep every ship away from the
   BUSHAN-RE for 6 minutes of game time (EASY 3, VERY EASY 2; pausing stops the clock): Fleet Intelligence speaks, the game pauses, the
   sensors manager opens centred on her with three ping circles. Report whether it played and where the pings were.
5. **Mission03: the mines and the missile corvettes' four streams (items 4, 6 and 7).** Once the fleet has arrived, look at a
   minefield up close (the level's mines sit in clusters on the way to the CLEE-SAN) and later at the mines the minelayers drop:
   expected, the level mines grey-brown metal with vents and panels, the laid mines square with yellow team-coloured corners; report
   any mine still flat white, and whether missiles in flight (the Acolytes', the P1 missile corvettes') show textures too (they had the
   same fault; not captured in flight yet). Fly a group past a minefield: the mines chase while you come towards them and **stop and wait
   when a faster ship flies away from them**, then pick it up again if it turns back (the laid mines, 600 u/s, give up on every Somtaaw
   strike craft flying away; the level mines, 1,000 u/s, catch Acolytes but not Recons; a mine whose target died disappears about 5 s
   later even while it chases a new one: the exe's); report a mine that stops while its target is slower and still coming towards it
   (that would be a fault); `tools/play.sh -- /catMineLog 480` writes the `minelog:` lines into the play log as evidence. When the
   raiders' missile corvettes fly, bring the camera close behind and a little above one: **four jet streams, one from the centre of
   each of the four glowing engine circles**, each about as wide as its circle, the engine glow as before; the A/B `tools/play.sh --
   /exeTrails` shows the original's two broad ribbons, each centred between an upper and a lower circle. Report whether the four read
   as four from the angles you play at, whether any looks offset from its circle, and whether you prefer them (the default stays ON).
6. **M3-R2's four checks, never reported** (docs/m3-r2-summary.md's brief, items 2-5): **the quit after a resolution change** (Options ->
   Video, another resolution, Accept, "keep these settings" Yes; at the main menu press Quit **once**: it must close at the first try;
   then set 3840x2160 back); **the cutscene flicker's instrument** (`tools/play.sh -- /catFrameSig ~/hwi-sig-$(date +%H%M).txt`: new
   campaign, the opening in-engine cutscene after Animatic01 (the Kuun-Lan close-up), then Mission01's arrival cutscene and the
   letterboxed close-up; watch for a flash showing the backdrop with no ships and no bars; report the cutscene, the seconds in, cut or
   mid-pan, and hand over the file: `python3 -I tools/dump_outliers.py <file>`, `short runs=0` with a flicker seen means the
   presentation; optional `--driver wayland`); **the hyperspace arrival** (after the nislet the gate must span the whole Kuun-Lan and
   slide bow to stern; the stern modules present as the gate passes, never popping back a second later; optional Mission02's start and
   Mission03's jump out; A/B `-- /noHsAssembly`); **FULL DETAIL under the budget** (`tools/play.sh -- /overlay /frameLog
   ~/hwi-frame-batched-$(date +%H%M).log`: the opening cutscene, Mission01 with the whole fleet in view, a skirmish battle; the FRAME
   line's p99 at or under 6.9 ms; the ships' look exactly as before; A/B `-- /overlay /meshImmediate /frameLog
   ~/hwi-frame-immediate-$(date +%H%M).log`; hand over both logs: `python3 -I tools/frame_log_report.py <file>`).
7. **M3-R1's open items, unchanged**: the letterbox bars' height at 3840x2160 (277 rows each; the exe would draw 42: question 2) and
   whether the exe's brighter subtitles / cursor after full bars are wanted (question 3); the team colours after Mission03 and on every
   jump; FULL DETAIL at the farthest zoom (`tools/play.sh -- /lodBands` for the A/B) and whether the sensors manager's blips and the
   level-following trails look right (question 4); the UI at 4K at 200 % (anything 2D that stayed small or sits wrong, any click that
   misses); the background stars' size (question 5: the exe's fixed share of the screen or the fixed pixel size you have now; now
   `CAT-TODO(M3-R4)`); the sensors map's thin lines at 200 %; the window and the mouse at the edges; the tutorial's pointer if a
   lesson's arrow seems a step behind.
8. **R10's open items, unchanged**: **the two animatics attended** (Animatic01 full screen before Mission01's opening NIS, at its own pace,
   with its sound and music, the cursor hidden; Animatic02 at Mission01's hyperspace white-out; the log's `animatic: play a=0 b=1
   file=Movies/Animatic01.bik` / `a=1 b=2`, **no `animatic: skipped` line**; whether Escape skips one); **the campaign from New Game as far
   as it goes** (you are past Mission04: on to Mission06's nebula tendrils, Mission09's Emergency Hyperspace, Mission17's Taiidan fighters
   at the Nomad Moon and the credits; per mission what differed from the GOG game, how long the fights take, where it stopped with the
   log's last lines); **the tutorial's 12 lessons** (Next through each, a Training save loading back); **a skirmish** against one computer
   for 10 minutes on a listed map, once with CPU Attacks "Humans only" / "Humans and allies", the managers' screens, **a recording**
   through the Record shortcut and its playback through Load Recorded Game.
9. **For every item**: any crash or hang with the terminal's last lines (`hwi-engine: fatal signal N`, the ASan report of a `--debug`
   run), the frame rate's drift over the session (the one-hour 4K@144 soak is M3's own item: the overlay's FRAME at the start and the
   end, or the frame log), and a screenshot of anything that looks wrong (the engine's own screenshots land in `original/play-home`'s
   ScreenShots).

**The known gaps to skip, not report**: combat balance is untested by the self-tests (item 8's campaign measures it); a failed hyperspace
jump happens only from the developer console; the GOG game's ships can differ from ours in the last bits of their velocities after about
three minutes of a skirmish (the rung's 2823 / 2871: invisible in play); the stage-5 LAN game and the Titan / WON screens are not ported;
a lesson or campaign save written by this build is in the tree's native format (only a recording's save is in the exe's format, decision
95), so it does not load in the GOG game; the window at 4K is borderless by design; the Mission02 self-test's scripted player does not
finish on HARD (the self-tests play NORMAL; in play HARD is fine); the Beast mine has no texture in the game data (plain in the exe too;
no mission places one); the HUD's SU blink, the ship view and the upgrade pulse keep animating while paused (the exe's); a flicker seen
with `short runs=0` in the `frame.sig` is the presentation's, not the engine's (then M3-R4 tries `--driver wayland` / a swap-interval
setting); the distant textures are smoother than the original's in both looks (GoK's mip maps: question 2).

## Decisions

**Applied this round:**
- The faithfulness defaults and decisions 17-132 of cycle/STATE.md; decision 70 (the table square root) under every run; decision 9 as
  "recorder" (stage 5's 23 sites untouched); decision 46's guard rails (the dumps compared whole and crc-stripped: **no default dump moved
  this round**, every one byte-equal to M3-R2's / M3-R1's / decision 118's references, the opt-outs together still byte-equal to R10's; the
  two stated crc-only moves are the options themselves: `/sharpTextures` on every frame, `/exeTrails` on none of the default scenes);
  decision 47's exclusions and decision 60's `real` in every replay comparison; decision 48 open (no fourth recording; `ships_rot`
  excluded); decision 55 followed by producers 1, 4 and 5 (28 review notes, merged); decision 57 stands (the headless Mission01 stream did
  not move); decision 75 untouched (the generators `--check` current); decisions 76 / 77 / 86 / 109 stand for every mission (17 / 17
  IDENTICAL to M3-R2's and so to M3-R1's and R10's); decision 84 (the ai soak's `rng_all` exempt; now sharpened to stream 11 by producer
  5); decisions 92 / 117 / 127 extended (the harness: 139 below); decisions 121 and 132's rules kept by every producer, audit and this
  step; the user's rules from the play-tests (Smooth motion ON, the overlay key from F1-F12, SHARP TEXTURES as an option with the original
  look the default, no gamescope, never fake input, the GPU on the user's word only and idle).
- **Decision 44 unchanged** (140 below): the headless Mission01 stream of this step's build (`c20ccda7fdae0f66`, release `316025e5c47989e1`) is
  IDENTICAL in every field over 6,001 ticks to M3-R1's `1ae66610c3f02410` / `234bcccfb948ab98` and so to R10's `5bd51234405ea0d8` (only
  the header's build identity differs), with every setting of this round flipped, as every producer's and audit's Mission01 run of the
  round (`7dd5f0181bb56ce1`, `f5064299e536486c`, `e7b718c577a7458d`, `4616da73824c375b`, `a549f68d3a06bad5`: the same content); **the rung
  unchanged**: 2823 / 2871 (177 / 118 with `/oracleLibmSqrt`).
- **Decisions 46 / 118 / 129 unchanged** (141 below): the four guard-rail dumps byte-equal to M3-R2's; producers 2, 3 and 4 moved no
  dump; producer 1's `/sharpTextures` and producer 5's `/exeTrails` move only the `crc=` column, as their reports state.

**Still open (the user's; none blocks the play-test):**
- **8.** Two saves from the M0 play-test (one mission, one skirmish).
- **The play-test sign-off** (docs/PLAN.md); the refreshed brief above.
- **Decision 48**: a fourth recording with the fixed oracle DLL.
- **The mode-B playback** of `hwirec01` / `hwirec02` ("What the user must decide" 10).
- **M3-R1's questions 2-5, 7-10**, **M3-R2's four checks** and **this round's five** ("What the user must decide" 1-5: TEXTURE MEMORY's
  default, the minification look, the pause's float and shimmer, the sensors points' size and the dust-belt question, the four streams'
  look).

**Surfaced by M3-R3.** Each has a default in the tree; say if you want otherwise (numbered 133-144 in cycle/STATE.md):
133 the overlay's key F1 and the switch-label rule (the toggle check fixed; `HWI-FEATURE(instrumentation)`); 134 SHARP TEXTURES
(`HWI-FEATURE(sharp-textures)`: the Options -> Video checkbox over the exe's `texLinearFiltering` row and effects presets, `/noFilter` /
`/sharpTextures`, the console row, the `preset` source, the `texfilter:` line; the original look the default; the mip maps kept); 135 the
pause freeze (`HWI-FEATURE(pause-freeze)`, always on: the held pose, the camera float's clocks, the glows and trails, the NIS's own pause;
the HUD's animations run on; the exe's `universePause` float restart kept); 136 Mission02's dust belts and the BUSHAN-RE hint are the exe's
(the Dust points with no fog test; the difficulty timer that never runs on HARD; nothing ported; the hold-scout switch and the hint checks);
137 the mines' textures (`HWI-FIX meshStaticTeamColor`: an x86-64 layout fault) and the mine AI as the exe's (the faster-away rule, the
once-a-second search, the 5 s lost-target lifetime); 138 the engine trails (`HWI-FEATURE(engine-trails)` default ON, `/exeTrails`, the
console row; the exe's one ribbon per Eng chunk; decision 84 sharpened to stream 11); 139 the harness (31 modes; this round's switches and
the `mothership` scene kind); 140 decision 44 and the rung unchanged, presentation only held; 141 the dumps unchanged (the two stated
crc-only options); 142 the tags and the delta map (the BTG star size re-targeted to `CAT-TODO(M3-R4)`; 28 review notes merged); 143 the
user's decisions of M3-R3 (the mandate, F1, the GPU) and the five open questions; 144 the exit-test rules carried into M3-R4 (132 plus:
every producer-added setting flipped on the Mission01 stream by the integrate, a paused dump as a gate, the ports' captures compared byte
for byte, the GPU batch after the correctness chains).
