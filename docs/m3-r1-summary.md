<!-- done: exit-test, fences, ports, deltamap, left, m3r2-plan, user-decisions, playtest-brief, decisions -->
# M3-R1 summary: the play-test fix round (the letterbox bars in front + the letterbox setting, the team colours across the warp, FULL DETAIL = LOD 0 by default, the unfocused objects' jitter, the UI scale 200% at 4K / 150% at 1440p and the 4K window) and the plan for M3-R2

Status: **COMPLETE 2026-10-21; the M3-R1 exit test MET** (every condition of "Exit test" below; presentation only:
no simulation field of any stream of the round moved); this doc and the cycle/STATE.md update are the integrate step's
checkpoint commit (the commit after `58df92d`). Branch `m2`, worktree `.claude/worktrees/m2`, workflow `cycle/workflows/m3-r1.js` (18281e2; `ROUND_BASE ce4fab4`,
the R10 baseline `5bd51234405ea0d8` / release `a44d585d71233c4a`), run id `wf_42a468c8-942`, run from the user's terminal
session 38027cce (Fable), not from the orchestrator; relaunched twice on 2026-10-21 (item 6 and the jitter recipe folded
in; the main checkout's stale script copy). 11 agents: five producers in sequence, one per play-test item (letterbox-front,
team-colours-warp, full-detail, motion-jitter, ui-scale; item 6, the 4K mouse border, inside ui-scale), five auditors
(each checkpointed its port: `94599c8` 0.919, `2b454b6` 0.91, `8410122` 0.913, `fdf35ee` 0.853, `58df92d` 0.857, all
PASS-WITH-FIXES) and this integrate step. No producer was cut off. The user's answers: cycle/answers/m3-r1-user.md
(95e011d / 335af38: Smooth motion unknown to the user, the skirmish Recon recipe). User decisions: cycle/STATE.md "## M2"
(the faithfulness defaults and decisions 17-110 applied; decision 70's table `fsqrt` under every run; decision 44's
baseline never re-baselined; decision 46's guard rails; decision 48 still open: no fourth recording, `ships_rot` stays
excluded; mode B still the user's step). **Every item is presentation only**: no simulation field of any oracle stream
moved this round (the tables below).

## Exit test (this step, from a clean build of `58df92d`)

**Builds.** `tools/build.sh --debug --clean` and `--release --clean` on the tree at `58df92d` (05:53; gcc 13.3.0, meson
1.3.2): both rc 0, `[273/273] Linking target hwi-engine` (R10: `[270/270]`; the three new sources
`src/SDL/uiscale.c`, `src/Game/TeamColourTest.c`, `src/Game/UiScaleTest.c`), **0 `warning:` and 0 `error` lines** under
`-Werror` (the one WARNING line is meson's sanitizer notice in the debug build). The binaries were copied aside to
`original/scratch-m2/m3r1-integrate/{debug,release}/hwi-engine` so that nothing rebuilt under the runs; the build logs
are `original/scratch-m2/m3r1-integrate/build-{debug,release}.log`.

| build | size | sha256 |
|---|---:|---|
| debug (ASan + UBSan) | 89,572,808 B | `dc2e8e09ba80f7cc…` |
| release (`-O2`, no sanitizers) | 5,699,832 B | `194161675b97762a…` |

`tools/m2_layout32.sh` OK; the generators current: `python3 -I tools/m2_save32_gen.py --check`, `tools/m2_kas2c_gen.py
--check`, `tools/m2_kasf_table_gen.py --dispatch --check`, `tools/m2_strings_gen.py --exe original/game/cataclysm.exe
--check` (R10's summary named them by other names; these are the files in `tools/`).

**Runs.** 161 harness runs `m3r1i-*` on the plain `tools/m2_harness.sh` (silent, offscreen; `runs.txt` lines
3544-3704) in eight chains: H first and alone (the GPU batch: the 4K frame-time runs and the real-clock scenes on the
release binary; nvidia-smi 3 % before, 2 % after, no game running; `claude-gpu-done` after it), then A (the rung's
replays, the hwi-engine playbacks, missions 14-17), B (the Mission01 gates with every new setting flipped, fe, the
scripted self-tests), C (missions 02-09), D (missions 10-13, Mission01's self-test, the tutorial, the lesson load, the
failed-jump pair, the warp start, loadgame, ai + the soak, the skirmishes, the motion / nis dumps and their opt-outs),
E (`sp` 1,200 s and fe at 4K), F (`sp` 1,200 s at 4K), G (the five fences' captures and self-tests) and X (the old-look
cross-check) in parallel, beside the user's load (load average 12-20 throughout: VS Code's ripgrep at ~1,100 % CPU; no
run lost its summary; nobody edited the harness during the step). **Every quitting run rc 0** except the timeouts by
design: `m3r1i-sp` and `m3r1i-sp-4k` (rc 124 at 1,200 s, running in Mission12 / Mission11, no assertion), `m3r1i-fe` and `m3r1i-fe-4k` (rc 124 at 120 s at
Main_game_screen) and the four fe captures (`--mode fe --shot`, 120 s); **ASan 0 in every run**; LeakSanitizer clean
in every run. The `.hwo` hashes differ from the producers' and audits' for the same content: the stream header carries
the build identity; IDENTICAL below is `hwo_compare.py`'s verdict over every tick and packet record and all 21 fields.

**The runs** (every `m3r1i-*` run in `runs.txt` order; rc / wall from `runs.txt`, the rest from each `summary.txt`; the `.hwo`
sha256 is this step's own hash, comparable only within this step; the last column is the mode's own summary line, long ones
cut in the middle):

| run | mode | rc / wall | milestone | first stop | ASan; LeakSanitizer | `run.hwo` sha256 (size) | the mode's line |
|---|---|---|---|---|---|---|---|
| `m3r1i-perf4k-on` | ai | 0 / 9s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 3835134bb5b82760 (123368 B) | done tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=3 ... hplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS lodstats: 301 frames; done frames=301 median=9.578 p99=11.389 max=13.347 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r1i-perf4k-bands` | ai | 0 / 5s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 3835134bb5b82760 (123368 B) | done tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=3 ... hplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS lodstats: 301 frames; done frames=301 median=2.602 p99=3.878 max=4.114 verts_median=4726 verts_max=4726 fullDetail=0 size=3840x2160 |
| `m3r1i-perf4k-on2` | ai | 0 / 9s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 3835134bb5b82760 (123368 B) | done tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=3 ... hplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS lodstats: 301 frames; done frames=301 median=9.083 p99=13.689 max=14.260 verts_median=25169 verts_max=25169 fullDetail=1 size=3840x2160 |
| `m3r1i-perf4k-bands2` | ai | 0 / 6s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 3835134bb5b82760 (123368 B) | done tick=498 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=3 ... hplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS lodstats: 301 frames; done frames=301 median=3.574 p99=4.295 max=5.155 verts_median=4726 verts_max=4726 fullDetail=0 size=3840x2160 |
| `m3r1i-rc-recon-noi` | skirmish | 0 / 46s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d06249034f24d3a4 (172567 B) | scene done tick=688 frames=5597 sampled=5597 Bmoved=451 realFrames=5867 realLate=4 dump=/h ... -Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-rc-recon-noi/scene.dump result=PASS |
| `m3r1i-rc-recon-on` | skirmish | 0 / 46s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4268f9a3b19c9dbc (172567 B) | scene done tick=688 frames=5598 sampled=5598 Bmoved=451 realFrames=5867 realLate=3 dump=/h ... d-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-rc-recon-on/scene.dump result=PASS |
| `m3r1i-rc-follow-noi` | skirmish | 0 / 46s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0983df80a36d4b7b (172567 B) | scene done tick=688 frames=5598 sampled=5598 Bmoved=451 realFrames=5869 realLate=3 dump=/h ... Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-rc-follow-noi/scene.dump result=PASS |
| `m3r1i-rc-follow-on` | skirmish | 0 / 46s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ce884d24dd088fcf (172567 B) | scene done tick=688 frames=5598 sampled=5598 Bmoved=451 realFrames=5867 realLate=3 dump=/h ... -Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-rc-follow-on/scene.dump result=PASS |
| `m3r1i-rc-nis-noi` | nis | 0 / 39s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=592 nisTime=36.000 frames=1374 sampled=1374 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-rc-nis-noi/nis.dump result=PASS |
| `m3r1i-rc-nis-on` | nis | 0 / 39s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=592 nisTime=36.000 frames=1382 sampled=1382 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-rc-nis-on/nis.dump result=PASS |
| `m3r1i-mission-a` | mission | 0 / 27s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1ae66610c3f02410 (1476370 B) |  |
| `m3r1i-lb-nis-shot` | nis | 0 / 51s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-lb-nis-shot/nis.dump result=PASS |
| `m3r1i-mission-b` | mission | 0 / 26s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1ae66610c3f02410 (1476370 B) |  |
| `m3r1i-r17-a` | replay | 0 / 64s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 53edd651fd28d8c7 (2630926 B) |  |
| `m3r1i-mission-perturb` | mission | 0 / 25s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f32a7ed4da018931 (1476419 B) |  |
| `m3r1i-mission-rel-a` | mission | 0 / 6s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 234bcccfb948ab98 (1476370 B) |  |
| `m3r1i-mission-rel-b` | mission | 0 / 6s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 234bcccfb948ab98 (1476370 B) |  |
| `m3r1i-mission-rel-perturb` | mission | 0 / 6s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 386599490ec94dc6 (1476419 B) |  |
| `m3r1i-lb-nis-shot-behind` | nis | 0 / 49s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-lb-nis-shot-behind/nis.dump result=PASS |
| `m3r1i-m2-a` | missiontest | 0 / 101s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c117111d821ec170 (6381116 B) | done mission=2 ticks=25936 objectives=6/6 result=PASS |
| `m3r1i-mission-noi` | mission | 0 / 25s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1ae66610c3f02410 (1476370 B) |  |
| `m3r1i-r17-b` | replay | 0 / 61s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 53edd651fd28d8c7 (2630926 B) |  |
| `m3r1i-mission-nonis` | mission | 0 / 25s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1ae66610c3f02410 (1476370 B) |  |
| `m3r1i-lb-nisfade-a` | nis | 0 / 49s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-lb-nisfade-a/nis.dump result=PASS |
| `m3r1i-mission-libm` | mission | 0 / 26s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1ae66610c3f02410 (1476370 B) |  |
| `m3r1i-r17-libm` | replay | 0 / 61s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 77f190c6425302db (2630929 B) |  |
| `m3r1i-m10-a` | missiontest | 0 / 186s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1c264df4875f829c (5397854 B) | done mission=10 ticks=21939 objectives=5/5 result=PASS |
| `m3r1i-r17-rel` | replay | 0 / 14s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 82bc6c9a8bc4ac50 (2630926 B) |  |
| `m3r1i-lb-nisfade-behind` | nis | 0 / 49s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-lb-nisfade-behind/nis.dump result=PASS |
| `m3r1i-mission-lb` | mission | 0 / 25s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1ae66610c3f02410 (1476370 B) |  |
| `m3r1i-m2-b` | missiontest | 0 / 100s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c117111d821ec170 (6381116 B) | done mission=2 ticks=25936 objectives=6/6 result=PASS |
| `m3r1i-us-4k-a` | uiscale | 0 / 15s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done physical=3840x2160 logical=1920x1080 scale=200 checks=45 fails=0 result=PASS |
| `m3r1i-mission-lbsw` | mission | 0 / 26s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1ae66610c3f02410 (1476370 B) |  |
| `m3r1i-us-4k-b` | uiscale | 0 / 15s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done physical=3840x2160 logical=1920x1080 scale=200 checks=45 fails=0 result=PASS |
| `m3r1i-r18-a` | replay | 0 / 33s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cb9bec70d3fbe31c (966649 B) |  |
| `m3r1i-us-1440` | uiscale | 0 / 15s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done physical=2560x1440 logical=1707x960 scale=150 checks=45 fails=0 result=PASS |
| `m3r1i-mission-lodbands` | mission | 0 / 25s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1ae66610c3f02410 (1476370 B) |  |
| `m3r1i-us-1080` | uiscale | 0 / 14s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done physical=1920x1080 logical=1920x1080 scale=100 checks=45 fails=0 result=PASS |
| `m3r1i-r18-b` | replay | 0 / 33s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | cb9bec70d3fbe31c (966649 B) |  |
| `m3r1i-us-1024` | uiscale | 0 / 15s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done physical=1024x768 logical=1024x768 scale=100 checks=45 fails=0 result=PASS |
| `m3r1i-mission-nolod` | mission | 0 / 25s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1ae66610c3f02410 (1476370 B) |  |
| `m3r1i-us-fit1960` | uiscale | 0 / 15s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done physical=1024x768 logical=1024x768 scale=100 checks=45 fails=0 result=PASS |
| `m3r1i-r18-libm` | replay | 0 / 33s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | b580ecc5c58914d5 (966654 B) |  |
| `m3r1i-m3-a` | missiontest | 0 / 97s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0e52719366f43770 (4385810 B) | done mission=3 ticks=17825 objectives=5/5 result=PASS |
| `m3r1i-mission-uisw` | mission | 0 / 26s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1ae66610c3f02410 (1476370 B) |  |
| `m3r1i-us-fit2160` | uiscale | 0 / 15s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done physical=1024x768 logical=1024x768 scale=100 checks=45 fails=0 result=PASS |
| `m3r1i-us-4k-cfg100` | uiscale | 0 / 15s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done physical=3840x2160 logical=3840x2160 scale=100 checks=45 fails=0 result=PASS |
| `m3r1i-mission-uicfg` | mission | 0 / 26s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1ae66610c3f02410 (1476370 B) |  |
| `m3r1i-play01-a` | replay | 0 / 30s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1a29c207ff9efcb8 (1315714 B) |  |
| `m3r1i-us-4k-sw150` | uiscale | 0 / 15s | uiscale self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done physical=3840x2160 logical=2560x1440 scale=150 checks=45 fails=0 result=PASS |
| `m3r1i-mission-optout` | mission | 0 / 27s | oracle mission run done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1ae66610c3f02410 (1476370 B) |  |
| `m3r1i-play01-b` | replay | 0 / 31s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1a29c207ff9efcb8 (1315714 B) |  |
| `m3r1i-m10-b` | missiontest | 0 / 187s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1c264df4875f829c (5397854 B) | done mission=10 ticks=21939 objectives=5/5 result=PASS |
| `m3r1i-lb-nis-shot-behind-bands` | nis | 0 / 49s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/Ga ... /original/scratch-m2/m2r2-harness/runs/m3r1i-lb-nis-shot-behind-bands/nis.dump result=PASS |
| `m3r1i-play02-a` | replay | 0 / 28s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 02bc810a29e13605 (1315721 B) |  |
| `m3r1i-m3-b` | missiontest | 0 / 100s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0e52719366f43770 (4385810 B) | done mission=3 ticks=17825 objectives=5/5 result=PASS |
| `m3r1i-play02-b` | replay | 0 / 29s | oracle replay done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 02bc810a29e13605 (1315721 B) |  |
| `m3r1i-lb-nis-shot-behind-bands-noi` | nis | 0 / 50s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/Ga ... ginal/scratch-m2/m2r2-harness/runs/m3r1i-lb-nis-shot-behind-bands-noi/nis.dump result=PASS |
| `m3r1i-fe-4k-shot` | fe | 124 / 120s | Main_game_screen | none: still running at the 120s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r1i-fe` | fe | 124 / 120s | Main_game_screen | none: still running at the 120s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r1i-m11-a` | missiontest | 0 / 110s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 58c7d84a5b92f927 (2036509 B) | done mission=11 ticks=8275 objectives=5/5 result=PASS |
| `m3r1i-statics` | statics | 0 / 14s | ship statics self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  |  |
| `m3r1i-statics-init` | statics | 0 / 14s | ship statics self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  |  |
| `m3r1i-m4-a` | missiontest | 0 / 106s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | dcc48a178d674ee4 (5122089 B) | done mission=4 ticks=20818 objectives=5/5 result=PASS |
| `m3r1i-meshes` | meshes | 0 / 13s | mesh self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  |  |
| `m3r1i-textures` | textures | 0 / 19s | texture self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  |  |
| `m3r1i-oracle` | oracle | 0 / 12s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | a2eadbb91bf73b6a (140723 B) |  |
| `m3r1i-oracle-rel-a` | oracle | 0 / 2s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4171cb5f07090dc0 (140723 B) |  |
| `m3r1i-oracle-rel-b` | oracle | 0 / 2s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 4171cb5f07090dc0 (140723 B) |  |
| `m3r1i-oracle-rel-perturb` | oracle | 0 / 2s | oracle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2c09d068c4a3e953 (140773 B) |  |
| `m3r1i-etg` | etg | 0 / 13s | etg effect self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  |  |
| `m3r1i-fe-1080-shot` | fe | 124 / 120s | Main_game_screen | none: still running at the 120s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r1i-m11-b` | missiontest | 0 / 106s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 58c7d84a5b92f927 (2036509 B) | done mission=11 ticks=8275 objectives=5/5 result=PASS |
| `m3r1i-dock` | dock | 0 / 24s | dock self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done fighters=6 outside=6 corvettes=2 carrierleg=2 processorleg=1 RUs=1850 jackPausedTick=1024 result=PASS |
| `m3r1i-m4-b` | missiontest | 0 / 102s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | dcc48a178d674ee4 (5122089 B) | done mission=4 ticks=20818 objectives=5/5 result=PASS |
| `m3r1i-command` | command | 0 / 22s | command self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=2998 link=252 mcv=1 unlink=285 research=1/1 mimic=1 point=1/1 result=PASS |
| `m3r1i-render` | render | 0 / 19s | render self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=599 frames=583 objects=582 lensflares=583 foglayers=1166 result=PASS |
| `m3r1i-m14-a` | missiontest | 0 / 219s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8eb173eb9fefeb30 (5496008 B) | done mission=14 ticks=22338 objectives=4/4 result=PASS |
| `m3r1i-sound` | sound | 0 / 19s | sound self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | result=PASS |
| `m3r1i-hud` | hud | 0 / 22s | hud self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=599 boxed=19 selected=154 frames=537 statusbars=6840 infopanels=537 pie=537 result=PASS |
| `m3r1i-m5-a` | missiontest | 0 / 80s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7308990062e47d6e (4031324 B) | done mission=5 ticks=16384 objectives=6/6 result=PASS |
| `m3r1i-ui` | ui | 0 / 18s | ui self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done selected=154 rows=10 popupframes=16 commands=26 targetrows=1 statsframes=8 result=PASS |
| `m3r1i-fe-1440-shot` | fe | 124 / 120s | Main_game_screen | none: still running at the 120s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r1i-fescreens` | fescreens | 0 / 12s | fescreens self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done options=35 startdefaults=35 readback=35 result=PASS |
| `m3r1i-m12-a` | missiontest | 0 / 120s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d3f08b58b926d8ad (2705630 B) | done mission=12 ticks=10995 objectives=3/3 result=PASS |
| `m3r1i-strings` | strings | 0 / 12s | strings self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done ids=1705 mapback=1705 resolved=1681 nostring=24 wrongtext=0 wrongnostring=0 result=PASS |
| `m3r1i-keys` | keys | 0 / 12s | keys self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done rows=197 known=149 unknown=48 commands=147 bindings=149 startsave=1 saved=1 reload=1 reset=1 newkey=1 conflicts=1 shares=1 failures=0 result=PASS |
| `m3r1i-console` | console | 0 / 17s | console self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done selected=154 frames=4 shown=34 set=27 commands=8 keys=32 kas=6 closed=1 failures=0 result=PASS |
| `m3r1i-m5-b` | missiontest | 0 / 81s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7308990062e47d6e (4031324 B) | done mission=5 ticks=16384 objectives=6/6 result=PASS |
| `m3r1i-build-a` | build | 0 / 28s | build self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | dd269f1a6fdfa4ba (1722370 B) | done tick=4205 driver=1 SU=40/100 RUs=25925 result=PASS |
| `m3r1i-build-b` | build | 0 / 28s | build self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | dd269f1a6fdfa4ba (1722370 B) | done tick=4205 driver=1 SU=40/100 RUs=25925 result=PASS |
| `m3r1i-nisparse` | nisparse | 0 / 12s | nisparse self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done nis=14 nisScripts=14 animatics=18 objects=593 events=1488 cataclysmEvents=31 unknownKeywordLines=47 failed=0 result=PASS |
| `m3r1i-fe-960-shot` | fe | 124 / 120s | Main_game_screen | none: still running at the 120s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r1i-toggle` | toggle | 0 / 12s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done start=1/1 fails=0 result=PASS |
| `m3r1i-m12-b` | missiontest | 0 / 121s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | d3f08b58b926d8ad (2705630 B) | done mission=12 ticks=10995 objectives=3/3 result=PASS |
| `m3r1i-hud-4k` | hud | 0 / 22s | hud self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=599 boxed=19 selected=154 frames=537 statusbars=6840 infopanels=537 pie=537 result=PASS |
| `m3r1i-toggle-noi` | toggle | 0 / 13s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done start=0/0 fails=0 result=PASS |
| `m3r1i-toggle-non` | toggle | 0 / 12s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done start=1/0 fails=0 result=PASS |
| `m3r1i-m14-b` | missiontest | 0 / 222s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 8eb173eb9fefeb30 (5496008 B) | done mission=14 ticks=22338 objectives=4/4 result=PASS |
| `m3r1i-hud-1080` | hud | 0 / 22s | hud self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=599 boxed=19 selected=154 frames=537 statusbars=6840 infopanels=537 pie=537 result=PASS |
| `m3r1i-toggle-lb` | toggle | 0 / 12s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done start=1/1 fails=0 result=PASS |
| `m3r1i-toggle-lodbands` | toggle | 0 / 12s | toggle self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done start=1/1 fails=0 result=PASS |
| `m3r1i-hud-1440` | hud | 0 / 22s | hud self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=599 boxed=19 selected=154 frames=537 statusbars=6840 infopanels=537 pie=537 result=PASS |
| `m3r1i-hud-960` | hud | 0 / 21s | hud self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=599 boxed=19 selected=154 frames=537 statusbars=6840 infopanels=537 pie=537 result=PASS |
| `m3r1i-lod-ai-on` | ai | 0 / 39s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f79c5d3e80f479d7 (98768 B) | done tick=300 fullDetail=1 objects=164 drawnAbove0=0 bandAbove0=164 result=PASS lodtest: 164 objects; frame tick=300 fullDetail=1 switch=0 camera=16220 objects=164 drawnAbove0=0 bandAbove0=164 maxBand=4 verts=25169 polys=46255; done tick=300 fullDetail=1 objects=164 drawnAbove0=0 bandAbov |
| `m3r1i-m6-a` | missiontest | 0 / 185s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 9d1e1fb4124fa7a2 (7629566 B) | done mission=6 ticks=31011 objectives=7/7 result=PASS |
| `m3r1i-lod-ai-bands` | ai | 0 / 24s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f79c5d3e80f479d7 (98768 B) | done tick=300 fullDetail=0 objects=164 drawnAbove0=164 bandAbove0=164 result=PASS lodtest: 164 objects; frame tick=300 fullDetail=0 switch=1 camera=16220 objects=164 drawnAbove0=164 bandAbove0=164 maxBand=4 verts=5330 polys=7030; done tick=300 fullDetail=0 objects=164 drawnAbove0=164 bandAb |
| `m3r1i-lod-render-on` | render | 0 / 18s | render self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=300 fullDetail=1 objects=1 drawnAbove0=0 bandAbove0=1 result=PASS lodtest: 1 objects; frame tick=300 fullDetail=1 switch=0 camera=14500 objects=1 drawnAbove0=0 bandAbove0=1 maxBand=2 verts=458 polys=558; done tick=300 fullDetail=1 objects=1 drawnAbove0=0 bandAbove0=1 result= |
| `m3r1i-lod-render-bands` | render | 0 / 17s | render self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=300 fullDetail=0 objects=1 drawnAbove0=1 bandAbove0=1 result=PASS lodtest: 1 objects; frame tick=300 fullDetail=0 switch=1 camera=14500 objects=1 drawnAbove0=1 bandAbove0=1 maxBand=2 verts=162 polys=196; done tick=300 fullDetail=0 objects=1 drawnAbove0=1 bandAbove0=1 result= |
| `m3r1i-m13-a` | missiontest | 0 / 253s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e2adc6154159337f (4744478 B) | done mission=13 ticks=19283 objectives=3/3 result=PASS |
| `m3r1i-v-recon-on` | skirmish | 0 / 106s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 20e6d275a234fcce (172567 B) | scene done tick=688 frames=5832 sampled=5832 Bmoved=451 realFrames=0 realLate=0 dump=/home ... ld-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-v-recon-on/scene.dump result=PASS |
| `m3r1i-m6-b` | missiontest | 0 / 177s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 9d1e1fb4124fa7a2 (7629566 B) | done mission=6 ticks=31011 objectives=7/7 result=PASS |
| `m3r1i-m15-a` | missiontest | 0 / 287s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1a8cb8a1fb7b5c87 (3795164 B) | done mission=15 ticks=15424 objectives=3/3 result=PASS |
| `m3r1i-sp` | sp | 124 / 1200s | universe running (univUpdateCounter > 0) | none: still running at the 1200s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r1i-sp-4k` | sp | 124 / 1201s | universe running (univUpdateCounter > 0) | none: still running at the 1200s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r1i-v-recon-noi` | skirmish | 0 / 104s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 20e6d275a234fcce (172567 B) | scene done tick=688 frames=5832 sampled=5832 Bmoved=451 realFrames=0 realLate=0 dump=/home ... d-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-v-recon-noi/scene.dump result=PASS |
| `m3r1i-m7-a` | missiontest | 0 / 119s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 51db6ce95cf41da3 (4082492 B) | done mission=7 ticks=16592 objectives=4/4 result=PASS |
| `m3r1i-v-follow-on` | skirmish | 0 / 104s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 20e6d275a234fcce (172567 B) | scene done tick=688 frames=5832 sampled=5832 Bmoved=451 realFrames=0 realLate=0 dump=/home ... d-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-v-follow-on/scene.dump result=PASS |
| `m3r1i-fe-4k` | fe | 124 / 120s | Main_game_screen | none: still running at the 120s timeout | ASan 0; no unsuppressed leak |  |  |
| `m3r1i-m13-b` | missiontest | 0 / 249s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e2adc6154159337f (4744478 B) | done mission=13 ticks=19283 objectives=3/3 result=PASS |
| `m3r1i-m7-b` | missiontest | 0 / 117s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 51db6ce95cf41da3 (4082492 B) | done mission=7 ticks=16592 objectives=4/4 result=PASS |
| `m3r1i-tc-user` | missiontest | 0 / 85s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 6c809b4b19a7282a (3910048 B) | done mission=1 ticks=2600 objectives=6/6 result=PASS teamcolour: 7 checks, 0 FAIL, 0 fail lines; done checks=7 failed=0 afterLoads=3 levels=Mission01_NIS01.mission:after,Mission01_NIS01.mission:level,Mission01.mission:after,Mission01.mission:level,Mission02.mission |
| `m3r1i-missiontest-a` | missiontest | 0 / 72s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 96c903b14adfa8b6 (3246588 B) | done mission=1 ticks=11424 objectives=6/6 result=PASS |
| `m3r1i-m15-b` | missiontest | 0 / 281s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1a8cb8a1fb7b5c87 (3795164 B) | done mission=15 ticks=15424 objectives=3/3 result=PASS |
| `m3r1i-missiontest-b` | missiontest | 0 / 73s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 96c903b14adfa8b6 (3246588 B) | done mission=1 ticks=11424 objectives=6/6 result=PASS |
| `m3r1i-tc-red` | missiontest | 0 / 86s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 6c809b4b19a7282a (3910048 B) | done mission=1 ticks=2600 objectives=6/6 result=PASS teamcolour: 7 checks, 0 FAIL, 0 fail lines; done checks=7 failed=0 afterLoads=3 levels=Mission01_NIS01.mission:after,Mission01_NIS01.mission:level,Mission01.mission:after,Mission01.mission:level,Mission02.mission |
| `m3r1i-tutorial-a` | missiontest | 0 / 48s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2f361c2f043a48b4 (2569841 B) | done ticks=10444 tutorial kas=1 lesson1=2 objectives=2 buttons=1 prompts=7/102/158 pointer=1 next=1108 lesson2=1110 lessons=12/12 complete=12/12 inOrder=1 nextClicks=11 result=PASS |
| `m3r1i-tutorial-b` | missiontest | 0 / 47s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 2f361c2f043a48b4 (2569841 B) | done ticks=10444 tutorial kas=1 lesson1=2 objectives=2 buttons=1 prompts=7/102/158 pointer=1 next=1108 lesson2=1110 lessons=12/12 complete=12/12 inOrder=1 nextClicks=11 result=PASS |
| `m3r1i-tc-m3` | missiontest | 0 / 94s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e42c134acb5dfb96 (4693802 B) | done mission=3 ticks=17825 objectives=5/5 result=PASS teamcolour: 6 checks, 0 FAIL, 0 fail lines; done checks=6 failed=0 afterLoads=3 levels=Mission03.mission:after,Mission03.mission:level,Mission04_NIS02.mission:after,Mission04_NIS02.mission:level,Mission04.mission |
| `m3r1i-tload-a` | loadgame | 0 / 24s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f04fe397ba093242 (886516 B) | done ticks=3600 from=8361 to=11961 mission=1 tutorial=1 ships=30 objectives=1/1 kasTeams=10 result=PASS |
| `m3r1i-tload-b` | loadgame | 0 / 25s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f04fe397ba093242 (886516 B) | done ticks=3600 from=8361 to=11961 mission=1 tutorial=1 ships=30 objectives=1/1 kasTeams=10 result=PASS |
| `m3r1i-lb-m1seg` | missiontest | 0 / 72s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 96c903b14adfa8b6 (3246588 B) | done mission=1 ticks=11424 objectives=6/6 result=PASS |
| `m3r1i-m16-a` | missiontest | 0 / 248s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e4ac8fa11506541f (5295518 B) | done mission=16 ticks=21523 objectives=6/6 result=PASS |
| `m3r1i-fj3-a` | missiontest | 0 / 88s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7a2bc8c3021eb23b (4346699 B) | done mission=3 ticks=17667 objectives=5/5 result=PASS |
| `m3r1i-lb-m1seg-behind` | missiontest | 0 / 72s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 96c903b14adfa8b6 (3246588 B) | done mission=1 ticks=11424 objectives=6/6 result=PASS |
| `m3r1i-m8-a` | missiontest | 0 / 401s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1f4708f4bc82cab6 (7223667 B) | done mission=8 ticks=29361 objectives=6/6 result=PASS |
| `m3r1i-fj3-b` | missiontest | 0 / 88s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7a2bc8c3021eb23b (4346699 B) | done mission=3 ticks=17667 objectives=5/5 result=PASS |
| `m3r1i-warp10-a` | warpstart | 0 / 16s | warp-start self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0e0f349f1b10bef7 (24968 B) | done level=10 ticks=99 applied=1 level=1 kas=1 arrived=35 fleet=1 failed=0 result=PASS |
| `m3r1i-lb-m2-shot` | missiontest | 0 / 92s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c117111d821ec170 (6381116 B) | done mission=2 ticks=25936 objectives=6/6 result=PASS |
| `m3r1i-warp10-b` | warpstart | 0 / 17s | warp-start self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 0e0f349f1b10bef7 (24968 B) | done level=10 ticks=99 applied=1 level=1 kas=1 arrived=35 fleet=1 failed=0 result=PASS |
| `m3r1i-loadgame-a` | loadgame | 0 / 26s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ea5a09243505bae6 (886495 B) | done ticks=3600 from=1985 to=5585 mission=1 tutorial=2 ships=141 objectives=0/1 kasTeams=36 result=PASS |
| `m3r1i-loadgame-b` | loadgame | 0 / 27s | save-load self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | ea5a09243505bae6 (886495 B) | done ticks=3600 from=1985 to=5585 mission=1 tutorial=2 ships=141 objectives=0/1 kasTeams=36 result=PASS |
| `m3r1i-ai-a` | ai | 0 / 19s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e4a40ecfcc025c23 (98768 B) | done tick=398 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=3 ... hplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS |
| `m3r1i-lb-m2-shot-behind` | missiontest | 0 / 94s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | c117111d821ec170 (6381116 B) | done mission=2 ticks=25936 objectives=6/6 result=PASS |
| `m3r1i-m16-b` | missiontest | 0 / 248s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e4ac8fa11506541f (5295518 B) | done mission=16 ticks=21523 objectives=6/6 result=PASS |
| `m3r1i-ai-b` | ai | 0 / 18s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | e4a40ecfcc025c23 (98768 B) | done tick=398 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=3 ... hplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS |
| `m3r1i-ai-soak` | ai | 0 / 34s | ai self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 7d8acdc92a183d71 (984370 B) | done tick=3998 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove= ... hplay=81 skirmishbuild=81 skirmishresearch=81 skirmishharvest=177 checks=16/16 result=PASS |
| `m3r1i-skirmish-lfe2-a` | skirmish | 0 / 19s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 19191afed0025546 (147967 B) | done ticks=600 ships=91 result=PASS |
| `m3r1i-skirmish-lfe2-b` | skirmish | 0 / 18s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 19191afed0025546 (147967 B) | done ticks=600 ships=91 result=PASS |
| `m3r1i-skirmish-armed8` | skirmish | 0 / 20s | skirmish self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 86da91da0639f535 (147967 B) | done ticks=600 ships=54 result=PASS |
| `m3r1i-motion-noi` | motion | 0 / 40s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=223 frames=1431 sampled=1431 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-motion-noi/motion.dump result=PASS |
| `m3r1i-motion-on` | motion | 0 / 40s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=223 frames=1431 sampled=1431 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-motion-on/motion.dump result=PASS |
| `m3r1i-nis-noi` | nis | 0 / 46s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-nis-noi/nis.dump result=PASS |
| `m3r1i-m8-b` | missiontest | 0 / 402s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 1f4708f4bc82cab6 (7223667 B) | done mission=8 ticks=29361 objectives=6/6 result=PASS |
| `m3r1i-nis-on` | nis | 0 / 46s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-nis-on/nis.dump result=PASS |
| `m3r1i-m17-a` | missiontest | 0 / 269s | mission self-test: the game's end (credits) | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f023197b107946a7 (3834034 B) | done mission=17 ticks=15584 objectives=4/4 result=PASS |
| `m3r1i-motion-optout` | motion | 0 / 40s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=223 frames=1431 sampled=1431 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-motion-optout/motion.dump result=PASS |
| `m3r1i-nis-optout` | nis | 0 / 42s | motion self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak |  | done tick=592 nisTime=36.000 frames=1440 sampled=1440 dump=/home/evanmedinger/Documents/GameDev/Homeworld-Infection/original/scratch-m2/m2r2-harness/runs/m3r1i-nis-optout/nis.dump result=PASS |
| `m3r1i-m9-a` | missiontest | 0 / 210s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 95fb1dc620628804 (5072642 B) | done mission=9 ticks=20617 objectives=6/6 result=PASS |
| `m3r1i-m17-b` | missiontest | 0 / 233s | mission self-test: the game's end (credits) | none found in the log (rc=0) | ASan 0; no unsuppressed leak | f023197b107946a7 (3834034 B) | done mission=17 ticks=15584 objectives=4/4 result=PASS |
| `m3r1i-m9-b` | missiontest | 0 / 179s | mission self-test done | none found in the log (rc=0) | ASan 0; no unsuppressed leak | 95fb1dc620628804 (5072642 B) | done mission=9 ticks=20617 objectives=6/6 result=PASS |

**Conditions.**

| # | condition | result | met |
|---|---|---|---|
| (a) | the R10 gates hold; the stream IDENTICAL in all 21 fields to `5bd51234405ea0d8`, also with `/noInterp`, `/noNISInterp`, `/oracleLibmSqrt` and each new setting flipped; fe; the self-tests; the dumps per decision 46; the guard rails vs R10's; the UBSan kinds vs R10's | **Mission01 headless**: debug pair `m3r1i-mission-a/b` byte-identical, sha256 **`1ae66610c3f02410`** (1,476,370 B, 6,001 `T` records), ASan 0; `mission_test.sh` **MISSION TEST PASS** (the perturbed run caught at tick 3000 / `rng10`, the flipped `ships_health` at 4000); the release pair **`234bcccfb948ab98`** byte-identical, IDENTICAL to the debug stream, its own MISSION TEST PASS; **`/noInterp`, `/noNISInterp`, `/oracleLibmSqrt`, `letterboxBehind 1` (the cfg row), `/letterboxBehind`, `/lodBands`, `noLOD 0`, `/uiScale 200`, `uiScale 150` and the three opt-outs together (`letterboxBehind 1` + `/lodBands` + `/noInterp`) all byte-identical to A**; **against R10's `r10i-mission-a` (`5bd51234405ea0d8`) and `r10i-mission-rel-a`: IDENTICAL in every one of the 21 fields over ticks 0-6000** (decision 44 unchanged; also IDENTICAL to R9's `553845b65e87f3b5`). **fe** reaches Main_game_screen at 1024x768 and at 3840x2160 (rc 124 at 120 s, ASan 0, 0 fescreen reports). **Self-tests** all PASS with ASan 0: statics 143 of 145 (x2), meshes 1,758 / 0 bad, textures 12,403 / 12,403, oracle (debug `a2eadbb91bf73b6a`, 0 sync errors, IDENTICAL to R10's) and the release `selftest.sh` pair `4171cb5f07090dc0` SELFTEST PASS (IDENTICAL to R10's), etg, dock, command, render, sound, hud, ui, fescreens 22 / 22 (195 screens, **`options=35`**: the exe's 34 + `uiScale`; 0 fescreen UBSan), strings 1,705 / 1,705, keys (197 rows, 0 failures), console **`tablerows=486 shown=34 set=27`** (R10: 31 / 24: the three new HWI rows), build pair, nisparse, **toggle x5** (`start=1/1`, `0/0`, `1/0`, with `/letterboxBehind`, with `/lodBands`: the `screen untouched` / `screen cancel`, `letterbox` x5 and `fulldetail` x8 checks PASS), **uiscale x9** (45 / 45 checks each), ai pair + the 4,000-tick soak, skirmish LFE2 pair + Armed8, missiontest pair, loadgame pair, warp start pair, lesson load pair, Tutorial1 pair 12 / 12; **the dumps** re-baselined per decision 46 with the producers' stated crc reasons and the opt-outs EQUAL to R10's (the table below); every guard-rail stream against R10's over the 21 fields (the table below: all 32 rows IDENTICAL, no field shows a first divergent tick); the UBSan kinds against R10's (below: no new kind) | **yes** |
| (b) | every mission 01-17 reaches every objective in order headlessly (pairs byte-identical, ASan 0); the table against R10's hashes | **17 / 17 PASS**: every mission's pair byte-identical, ASan 0 / 0, every objective in order (`result=PASS`), Mission17's milestone `the game's end (credits)`; **every stream IDENTICAL in all 21 fields to R10's `r10i-mN-a` / `r10i-missiontest-a`** (the mission table below: no field shows a first divergent tick); the same 17 hashes-by-content as R10's, so the campaign's simulation did not move in the round (the fix round touched the draw, the UI and the clocks only) | **yes** |
| (c) | `sp --timeout 1200` at the default size, plus fe and sp at 3840x2160, ASan 0 | `m3r1i-sp --timeout 1200` (1024x768, `/debugScreenshots`, the debug build): **walks Mission01_NIS01 -> 01 -> 02 -> 03 -> 04_NIS02 -> 04 -> 05 -> 06_NIS04 -> 06 -> 07_NIS05 -> 07 -> 08 -> 09 -> 10_NIS06 -> 10 -> 11 -> 12_NIS07 -> 12** and is running in Mission12 at the timeout (rc 124, **129 screenshots**: `golden-01` 10, 02 8, 03 14, 04 13, 05 8, 06 13, 07 8, 08 13, 09 13, 10 13, 11 13, 12 3; R10: 127 with 1 in Mission12), **ASan 0, no leak, no fatal, no assertion, 0 `Select.c` / `FOW.c` / `is_final_tex` lines**, `animatics: 12 skipped (headless): Animatic01 .. Animatic12`, 23,289 file loads; the UBSan kinds in the run `Particle.c` / `ETG.c` members and `BTG.c:389` / `:392` only (R10's). **`m3r1i-sp-4k --timeout 1200 --res 3840x2160`**: `window: requested 3840x2160, usable none, created 3840x2160 (window)`, `uiscale: physical 3840x2160 ... scale 200% (auto, uiScale 0) logical 1920x1080`, **walks the same chain to Mission11** (running in Mission11 at the timeout, rc 124; **121 screenshots**, `golden-11` 8; `animatics: 11 skipped`: the 4K ASan frames are slower, so the loop is one mission behind), **ASan 0, no leak, no `DEADLYSIGNAL`** in the timeout-kill shutdown (the ui-scale round's intermittent one did not recur), 0 `Select.c` / `FOW.c` lines, the same UBSan kinds. **fe** at 1024x768 (`m3r1i-fe`) and at 3840x2160 (`m3r1i-fe-4k`, `scale 200% ... logical 1920x1080`): Main_game_screen at the 120 s timeout, ASan 0, 0 fescreen reports | **yes** |
| (d) | the rung unchanged: the replay pairs 10-17 / 10-18 against the GOG streams to 9600 / 3526 with the stated fields and exclusions, first divergences still 2823 / 2871 (177 / 118 with `/oracleLibmSqrt`), IDENTICAL to `r10i-r17-a` / `r10i-r18-a`; mode B's directory listing | pairs `m3r1i-r17-a/b` **`53edd651fd28d8c7`** (2,630,926 B, 9,601 ticks, 4,800 packets) and `m3r1i-r18-a/b` **`cb9bec70d3fbe31c`** (966,649 B, 3,527 ticks) byte-identical, ASan 0; against `original/oracle/oracle01-rec-20261017-140428/oracle01-rec.hwo` (`--from-tick 1 --to-tick 9600`, the task's list less `rng6`: `rng10,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,resources,univcheck,dt,total,real`) **10-17 IDENTICAL through 2822, first divergent `ships_vel` 2823** (a and b, and the release replay `m3r1i-r17-rel`); against `…20261018-102253/oracle01-rec.hwo` (`--to-tick 3526`, the list less `rng6` and `real`) **10-18 IDENTICAL through 2870, first divergent `ships_vel` 2871**; the exclusions as R10's (`rng6` at 161 in both, `real` at 2 in 10-18); with `/oracleLibmSqrt` **177** (`ships_vel`) / **118** (`ships_pos`); the release replay equal to the debug on every field but `rng_all` (6377, as R10); **both replays IDENTICAL in every field to `r10i-r17-a` / `r10i-r18-a`** (the guard-rail table), the hwirec01 / hwirec02 playbacks IDENTICAL to R10's. **Mode B not run**: `ls original/oracle` shows no `hwirec*-play-*` (0 entries) and the newest `oracle01-rec-*` is `oracle01-rec-20261019-143530` (no newer than 10-19; decision 48 open) | **yes** |
| (e) | the five fences rerun with numbers | the section "The five fences" below: the letterbox PPM bars, the team colours' self-test and Kuun-Lan region, FULL DETAIL's lodtest and the 4K frame time on an idle GPU, the motion residual table (flags on against `/noInterp`, the non-focus objects), the UI scale (4K downscaled vs 1080p, 1440p at 1.5 vs 1707x960, the scripted 4K click, the window fit and the mouse at the border) | **yes** |
| (f) | no `CAT-TODO` / `CAT-PROVISIONAL` names M3-R1; the counts by owner; the `HWI-FEATURE` counts; clean builds with 0 warnings | `grep -rn -E 'CAT-(TODO\|PROVISIONAL)\([^)]*M3-R1' src/` empty (0 lines). `/bin/grep -rn CAT-TODO src/`: **37 lines, 28 owner tags** (R10: 35 / 26): stage 5 23 (plain 8, multiplayer options 8, LAN 6, multiplayer 1), M3 oracle physics 3, **M3 soak 1** (`utility.c`: the task clock, producer 4), **M3-R2 1** (`BTG.c`: the star size, producer 5); **4 `CAT-PROVISIONAL`** (R10: 4). `HWI-FEATURE(motion-interp)` **484** (R10 441), `HWI-FEATURE(letterbox-setting)` **36**, `HWI-FEATURE(full-detail)` **178**, `HWI-FEATURE(ui-scale)` **299** (all new); `CAT:` 4,465, `CAT-FIX` 195, `CAT-DORMANT` 13, `HWI-ORACLE` 49, `HWI-TEST` 222, `HWI-FIX` 53, `HWI-EXPERIMENT` 6 (count with `/bin/grep`). The clean builds: 0 warnings (above) | **yes** |

**M3-R1 exit test: MET** (every condition; presentation only held: no simulation field of any stream of the round moved).

**Guard-rail streams against R10's** (`hwo_compare.py`, every one of the 21 fields over the whole common range; `a = b` = the pair
byte-identical; the ai soak compared over its 20 fields with `rng_all` exempt (decision 84) and then over `rng_all` alone; the
Mission01 rows are the settings-flipped runs of condition (a) against R10's `r10i-mission-a`; the last row the release stream
against this step's debug one):

| stream | R10 (`r10i-*`) | this step | against R10 | first divergent tick per field (`-` = none) | the self-test line |
|---|---|---|---|---|---|
| ai, 500 | `9f43cbfb5ef0b199` | `e4a40ecfcc025c23` (a = b) | IDENTICAL | - | tick=398 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 attac |
| ai soak, 4,000 (20 fields, rng_all exempt: decision 84) | `bf8b4579fb36b512` | `7d8acdc92a183d71` | IDENTICAL | - | tick=3998 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 atta |
| ai soak, rng_all | `bf8b4579fb36b512` | `7d8acdc92a183d71` | IDENTICAL | - | tick=3998 fleet=156 teams=17 attackorder=17 defenceorder=17 reconorder=17 attackmove=33 defencemove=65 reconmove=49 atta |
| skirmish LFE2, 600 | `a8c47a7507628300` | `19191afed0025546` (a = b) | IDENTICAL | - | ticks=600 ships=91 result=PASS |
| skirmish Armed8, 600 | `739466459c63a79a` | `86da91da0639f535` | IDENTICAL | - | ticks=600 ships=54 result=PASS |
| build, 7,000 | `3e0464cd827c947b` | `dd269f1a6fdfa4ba` (a = b) | IDENTICAL | - | tick=4205 driver=1 SU=40/100 RUs=25925 result=PASS |
| missiontest (Mission01), 11,425 | `01759e3c6af634f5` | `96c903b14adfa8b6` (a = b) | IDENTICAL | - | mission=1 ticks=11424 objectives=6/6 result=PASS |
| loadgame (01 - Hiigara), 3,600 | `ecdf98716cb11dc3` | `ea5a09243505bae6` (a = b) | IDENTICAL | - | ticks=3600 from=1985 to=5585 mission=1 tutorial=2 ships=141 objectives=0/1 kasTeams=36 result=PASS |
| warp start (level 10) | `e1d321f0b217650f` | `0e0f349f1b10bef7` (a = b) | IDENTICAL | - | level=10 ticks=99 applied=1 level=1 kas=1 arrived=35 fleet=1 failed=0 result=PASS |
| Tutorial1 leg | `4c62ba9ef9c64ab8` | `2f361c2f043a48b4` (a = b) | IDENTICAL | - | ticks=10444 tutorial kas=1 lesson1=2 objectives=2 buttons=1 prompts=7/102/158 pointer=1 next=1108 lesson2=1110 lessons=1 |
| lesson load (LESSON 11), 3,600 | `cc08e4e1684028a2` | `f04fe397ba093242` (a = b) | IDENTICAL | - | ticks=3600 from=8361 to=11961 mission=1 tutorial=1 ships=30 objectives=1/1 kasTeams=10 result=PASS |
| failed jump (Mission03) | `7ca867e8435b2f41` | `7a2bc8c3021eb23b` (a = b) | IDENTICAL | - | mission=3 ticks=17667 objectives=5/5 result=PASS |
| replay 10-17 | `9af78e15f5234902` | `53edd651fd28d8c7` (a = b) | IDENTICAL | - |  |
| replay 10-17 /oracleLibmSqrt | `f9bb9674dc18078e` | `77f190c6425302db` | IDENTICAL | - |  |
| replay 10-17 release | `60de70ccef41fedc` | `82bc6c9a8bc4ac50` | IDENTICAL | - |  |
| replay 10-18 | `8112853eacffc80a` | `cb9bec70d3fbe31c` (a = b) | IDENTICAL | - |  |
| replay 10-18 /oracleLibmSqrt | `b7b9176c333702fb` | `b580ecc5c58914d5` | IDENTICAL | - |  |
| hwirec01 playback (Armed8) | `4cd0ef18b5c7bc8c` | `1a29c207ff9efcb8` (a = b) | IDENTICAL | - |  |
| hwirec02 playback (LFE2) | `35a2bf7816623808` | `02bc810a29e13605` (a = b) | IDENTICAL | - |  |
| oracle self-test (debug) | `11966c5c4674c16f` | `a2eadbb91bf73b6a` | IDENTICAL | - |  |
| oracle self-test (release) | `8b97d3114e27ae8e` | `4171cb5f07090dc0` (a = b) | IDENTICAL | - |  |
| Mission01 noi | `5bd51234405ea0d8` | `1ae66610c3f02410` | IDENTICAL | - |  |
| Mission01 nonis | `5bd51234405ea0d8` | `1ae66610c3f02410` | IDENTICAL | - |  |
| Mission01 libm | `5bd51234405ea0d8` | `1ae66610c3f02410` | IDENTICAL | - |  |
| Mission01 lb | `5bd51234405ea0d8` | `1ae66610c3f02410` | IDENTICAL | - |  |
| Mission01 lbsw | `5bd51234405ea0d8` | `1ae66610c3f02410` | IDENTICAL | - |  |
| Mission01 lodbands | `5bd51234405ea0d8` | `1ae66610c3f02410` | IDENTICAL | - |  |
| Mission01 nolod | `5bd51234405ea0d8` | `1ae66610c3f02410` | IDENTICAL | - |  |
| Mission01 uisw | `5bd51234405ea0d8` | `1ae66610c3f02410` | IDENTICAL | - |  |
| Mission01 uicfg | `5bd51234405ea0d8` | `1ae66610c3f02410` | IDENTICAL | - |  |
| Mission01 optout | `5bd51234405ea0d8` | `1ae66610c3f02410` | IDENTICAL | - |  |
| Mission01 release vs debug | `1ae66610c3f02410` | `234bcccfb948ab98` | IDENTICAL | - |  |

**The dumps** (decision 46 / 118: `--mode motion` / `--mode nis` at 144 fps, whole sha256 / with the `crc=` column stripped, first 16 hex):

| dump | R10 (`r10i-*`) | the reference after the round (producer 5's audit) | this step | frames |
|---|---|---|---|---|
| motion `/noInterp` (`m3r1i-motion-noi`) | `183681235a3fdc7b` / `3016280e9e0dd3c8` | `c503c869f0dc9cdb` / `3016280e9e0dd3c8` | **`c503c869f0dc9cdb` / `3016280e9e0dd3c8` EQUAL** | 1,431 |
| motion flags on (`m3r1i-motion-on`) | `428fff6c765ab81b` / `428ef41479eb5749` | `c0e411662fa2f527` / `428ef41479eb5749` | **`c0e411662fa2f527` / `428ef41479eb5749` EQUAL** | 1,431 |
| nis `/noInterp` (`m3r1i-nis-noi`) | `13ca70eea998e5de` / `196bab32d244f614` | `86cb155acffc180e` / `196bab32d244f614` | **`86cb155acffc180e` / `196bab32d244f614` EQUAL** | 1,440 |
| nis flags on (`m3r1i-nis-on`) | `6a4c1850799187ac` / `7ce8472514254f4f` | `0307f3be2044c29c` / `a5de13a44b8d264f` | **`0307f3be2044c29c` / `a5de13a44b8d264f` EQUAL** | 1,440 |
| the three opt-outs together, `--cfg 'letterboxBehind 1' -- /lodBands /noInterp` (`m3r1i-motion-optout` / `-nis-optout`) | R10's `/noInterp` dumps, above | (R10's) | **`183681235a3fdc7b` / `3016280e9e0dd3c8` and `13ca70eea998e5de` / `196bab32d244f614`: byte-equal (`cmp`) to `r10i-motion-noi` / `r10i-nis-noi`**; against this step's defaults only the `crc=` column differs (603 of 1,431 motion frames, 1,260 of 1,440 nis frames: the LOD-0 meshes and the bars) | 1,431 / 1,440 |

The moves since R10, each with its producer's stated reason (the audits confirmed each): producer 1 moved the two nis dumps' `crc=`
column only (the bars in front: the sRecon's two passes through the bottom bar nearer than the bar depth, 279 / 277 of 1,440 frames;
`2a7fec4de8cfe716` / `46df1d391a86a404`); producer 2 moved nothing; producer 3 moved every dump's `crc=` column only (the ships drawn
with their LOD-0 meshes: motion 603 / 605 of 1,431 frames, nis 1,260 / 1,267 of 1,440; with `/lodBands` producer 2's hashes); producer 4
moved the nis flags-on dump's positions and crc (fix 1: the NIS task's alpha past 1 in the frame whose pass runs the next NIS tick,
160 of 1,440 frame lines; `da532dafedb89960` -> `0307f3be2044c29c`); producer 5 moved nothing (the dumps are physical-pixel at
1024x768, scale 100 %). No position column of a `/noInterp` dump moved in the round: the crc-stripped `3016280e9e0dd3c8` /
`196bab32d244f614` are R10's.

**Missions 01-17 against R10's** (`--mode missiontest --mission N`, pairs a / b on the debug binary; Mission01 is the NIS-first
`m3r1i-missiontest-a/b`; `hwo_compare.py` over every field of the common range):

| mission | the self-test line | hash (a) | pair (b) | ASan a / b | against R10 (`r10i-mN-a`) | first divergent tick per field (`-` = none) | milestone |
|---|---|---|---|---|---|---|---|
| 1 | mission=1 ticks=11424 objectives=6/6 result=PASS | `96c903b14adfa8b6` | byte-identical (96c903b14adfa8b6) | ASan errors: 0 / 0 | vs `r10i-missiontest-a`: IDENTICAL | - | mission self-test done |
| 2 | mission=2 ticks=25936 objectives=6/6 result=PASS | `c117111d821ec170` | byte-identical (c117111d821ec170) | ASan errors: 0 / 0 | vs `r10i-m2-a`: IDENTICAL | - | mission self-test done |
| 3 | mission=3 ticks=17825 objectives=5/5 result=PASS | `0e52719366f43770` | byte-identical (0e52719366f43770) | ASan errors: 0 / 0 | vs `r10i-m3-a`: IDENTICAL | - | mission self-test done |
| 4 | mission=4 ticks=20818 objectives=5/5 result=PASS | `dcc48a178d674ee4` | byte-identical (dcc48a178d674ee4) | ASan errors: 0 / 0 | vs `r10i-m4-a`: IDENTICAL | - | mission self-test done |
| 5 | mission=5 ticks=16384 objectives=6/6 result=PASS | `7308990062e47d6e` | byte-identical (7308990062e47d6e) | ASan errors: 0 / 0 | vs `r10i-m5-a`: IDENTICAL | - | mission self-test done |
| 6 | mission=6 ticks=31011 objectives=7/7 result=PASS | `9d1e1fb4124fa7a2` | byte-identical (9d1e1fb4124fa7a2) | ASan errors: 0 / 0 | vs `r10i-m6-a`: IDENTICAL | - | mission self-test done |
| 7 | mission=7 ticks=16592 objectives=4/4 result=PASS | `51db6ce95cf41da3` | byte-identical (51db6ce95cf41da3) | ASan errors: 0 / 0 | vs `r10i-m7-a`: IDENTICAL | - | mission self-test done |
| 8 | mission=8 ticks=29361 objectives=6/6 result=PASS | `1f4708f4bc82cab6` | byte-identical (1f4708f4bc82cab6) | ASan errors: 0 / 0 | vs `r10i-m8-a`: IDENTICAL | - | mission self-test done |
| 9 | mission=9 ticks=20617 objectives=6/6 result=PASS | `95fb1dc620628804` | byte-identical (95fb1dc620628804) | ASan errors: 0 / 0 | vs `r10i-m9-a`: IDENTICAL | - | mission self-test done |
| 10 | mission=10 ticks=21939 objectives=5/5 result=PASS | `1c264df4875f829c` | byte-identical (1c264df4875f829c) | ASan errors: 0 / 0 | vs `r10i-m10-a`: IDENTICAL | - | mission self-test done |
| 11 | mission=11 ticks=8275 objectives=5/5 result=PASS | `58c7d84a5b92f927` | byte-identical (58c7d84a5b92f927) | ASan errors: 0 / 0 | vs `r10i-m11-a`: IDENTICAL | - | mission self-test done |
| 12 | mission=12 ticks=10995 objectives=3/3 result=PASS | `d3f08b58b926d8ad` | byte-identical (d3f08b58b926d8ad) | ASan errors: 0 / 0 | vs `r10i-m12-a`: IDENTICAL | - | mission self-test done |
| 13 | mission=13 ticks=19283 objectives=3/3 result=PASS | `e2adc6154159337f` | byte-identical (e2adc6154159337f) | ASan errors: 0 / 0 | vs `r10i-m13-a`: IDENTICAL | - | mission self-test done |
| 14 | mission=14 ticks=22338 objectives=4/4 result=PASS | `8eb173eb9fefeb30` | byte-identical (8eb173eb9fefeb30) | ASan errors: 0 / 0 | vs `r10i-m14-a`: IDENTICAL | - | mission self-test done |
| 15 | mission=15 ticks=15424 objectives=3/3 result=PASS | `1a8cb8a1fb7b5c87` | byte-identical (1a8cb8a1fb7b5c87) | ASan errors: 0 / 0 | vs `r10i-m15-a`: IDENTICAL | - | mission self-test done |
| 16 | mission=16 ticks=21523 objectives=6/6 result=PASS | `e4ac8fa11506541f` | byte-identical (e4ac8fa11506541f) | ASan errors: 0 / 0 | vs `r10i-m16-a`: IDENTICAL | - | mission self-test done |
| 17 | mission=17 ticks=15584 objectives=4/4 result=PASS | `f023197b107946a7` | byte-identical (f023197b107946a7) | ASan errors: 0 / 0 | vs `r10i-m17-a`: IDENTICAL | - | mission self-test: the game's end (credits) |

**UBSan kinds** (the union over the 161 runs; all non-fatal; none is `fescreen`, `PATCH`, `fquant.c`, `AIShip.c`, `Tutor.c`,
`Select.c`, `FOW.c`, `Subtitle.c`, `CommandNetwork.c`, `Queue.c` or `stb_image_write.h`: the same nine kinds as R10's, **no new kind**):
- the ETG opcode kinds of GoK's packed effect bytecode (`etgdecision` 2,614 reports in 114 runs, `etgfunctioncall` 1,548, `etgconditional`
  1,484, `etgbranch` 1,024 (114 runs each), `etgvariablecopy` 206 in 59) and `struct particle` (`Particle.c` 9,252 in 60 runs, `ETG.c` 926
  in 57): every run with a universe, as in R10;
- `BTG.c` misaligned `short` loads (226 reports in 113 runs; `BTG.c:389` the first report of 113 runs) and the misaligned `udword` load
  (59 in 59), as in R10;
- 47 runs report nothing (the scripted self-tests without a universe, the oracle self-tests, the release runs, fe and its captures, the
  toggle and uiscale runs, the perf and real-clock scenes on the release binary).
**fescreen UBSan reports 0 in all 161 runs**; ASan errors 0 and `no unsuppressed leak` in all 161; the new code of the round
(`uiscale.c`, `TeamColourTest.c`, `UiScaleTest.c`, the letterbox and LOD paths) reports nothing.

## The five fences, rerun from this step's clean build

Every number below is this step's own run (`m3r1i-*`), quoted from the harness's summary lines or the tools' output;
the producers' and audits' numbers are named where they differ. Debug ASan binary unless stated; the GPU measurements
(fence 3's frame time, fence 4's real-clock scenes) on the release binary with the GPU idle (nvidia-smi 2-4 % before
and after, no game running; the one `claude-gpu-done` after the batch). No fence moved a simulation field: the streams of
every fence run that writes one are IDENTICAL to R10's (the guard-rail and mission tables above).

### Fence 1: the letterbox bars (PPM captures, `tools/ppm_region.py`; 1024x768: the bars are rows 0-106 and 661-767)

| capture | default (the bars in front) | `letterboxBehind 1` / `/letterboxBehind` |
|---|---|---|
| NIS n06 26-36 s, dump frame 1374 (tick 584: the sRecon O9 crossing the bottom bar), `m3r1i-lb-nis-shot` / `-behind` (`--shot 584:6 -- /catMotionShot 1374`) | `bands`: top_rows 0 (the cursor's 89 pixels at 0,0), **bottom_rows 107**; bottom rows 661-767 **match=109568 other=0** (all black); top rows 0-106 match=109479 other=89 (the cursor box, drawn after the bars as in the exe); `shot.584.6.ppm` vs `nis.dump.1374.ppm` differ=0 (the any-mode capture = the motion capture) | bottom rows **match=80314 other=29254** (the Recon in the bar, bbox x 322-710, rows 661-767); default vs behind differ=29254, bbox 322,661-711,768; **the old look back bit for bit**: the behind frame's two bar regions equal ce4fab4's own frame (`m3r1-lb-base-nis1374`, the launching session's ce4fab4 debug binary) differ=0 top and bottom, and the whole frame equals it (differ=0 over 1024x768) once `/lodBands` is added (`m3r1i-lb-nis-shot-behind-bands`): the 1,306 scene pixels that differ without it (bbox 474,107-807,599, in the picture, not the bars) are the LOD-0 meshes of item 3 |
| the fade (`--shot 576-598/2:6 -- /catNisScissorOut 580:1`, `m3r1i-lb-nisfade-a` / `-behind`, 8 frames each 576 .. 590) | tick 584 (fade 0.75, 80 rows): bottom rows 688-767 **match=81920 other=0 at 32,32,32**, top rows 0-79 81,830 at 32,32,32 + 90 cursor pixels (the ship under the fading bar is covered); | default vs behind differ 22,591 (576) / 26,850 (578) / 31,993 (580) / 28,140 (582) / **21,873 (584, bbox 396,688-711,768)** / 9,256 (586) / **0 (588, 590)**: the two settings agree once the bars are full and the ship is past |
| Mission01's cutscenes (missiontest NIS-first start, `--shot 747,789,1128 --shot-letterbox`, `m3r1i-lb-m1seg` / `-behind`: n01 (segment 0) and the arrival nislet01 (`shot.s1.*`), 6 frames each, both the Mission01 self-test PASS, the streams IDENTICAL to R10's missiontest) | `bands` top_rows 107 / bottom_rows 107 in all six; the bar rows **0 non-black, top and bottom, in every frame** | 3 of 6 differ: n01 tick 1128 **242** pixels (bbox 239,0-358,107, the top bar), s1 tick 747 **51,099** (bbox 280,661-791,768, the hull in the bottom bar), s1 tick 789 **65,115** (bbox 0,0-710,107, the hangar hull in the top bar); n01 747 / 789 and s1 1128 byte-identical (the report: 242 / 51,099 / 65,115) |
| Mission02's KAS letterbox (the jump at the end: `--mission 2 --shot 25770-25840/2 --shot-letterbox`, `m3r1i-lb-m2-shot` / `-behind`, 32 frames each from the fade-in at 25778, the Mission02 self-test 6 / 6 PASS) | tick 25800 (fade 0.479): the top 51 rows **52,134 of 52,224 at 66,66,66** (128 x 0.521; the cursor's 90); tick 25840: `bands` top_rows 107 / bottom_rows 107 | **all 32 byte-identical between the settings** (differ=0: no geometry nearer than the bar depth in the mission camera; the report: 64 of 64) |

### Fence 2: the team colours across the warp (the self-test and the Kuun-Lan region)

Mission01 from the NIS-first start with `--teamcolour --shot 900,1000,2016 -- /catTeamColourTicks 2600` (the run goes
2,600 ticks into Mission02; segment 0 = Mission01_NIS01, s1 = Mission01, s2 = Mission02; 8 captures each), the user's
scheme `--player-colours ff1bca82/ffff5539` (`m3r1i-tc-user`) and a contrasting one `ff2020e0/ff20e020` (`m3r1i-tc-red`):

| run | `teamcolour:` checks | the 24x24 region's mean R, G, B (`ppm_region.py stats --region`) |
|---|---|---|
| `m3r1i-tc-user` (base R130 G202 B27) | **7 / 7 PASS** (`after` + `level` at Mission01_NIS01 (248 textures), Mission01 (513), Mission02 (594), `end` at tick 2600; `failed=0` in each: mothership 10, acolyte 53, worker 38, recon 51 statics coloured) | Mission01_NIS01 tick 900 (444,450-468,474): **53.00, 71.16, 35.19**; Mission01 tick 2016 after the arrival (342,314-366,338): **93.02, 118.23, 32.17**; Mission02 tick 1000 (518,418-542,442): **113.85, 117.67, 57.52**: green over red over blue in all three (the report: 53.0 / 71.2 / 35.2, 95.6 / 122.6 / 32.0, 116.3 / 120.5 / 58.2; the M2 build gave 73.4 / 70.1 / 47.7 grey-brown at Mission01 2016) |
| `m3r1i-tc-red` (base R224 G32 B32) | 7 / 7 PASS | 72.15, 36.95, 32.11; 133.38, 48.88, 33.46; 149.53, 63.81, 59.16: red-dominant in all three (the report: 72.2 / 37.0 / 32.1, 138.5 / 48.7 / 33.4, 153.4 / 64.6 / 59.9) |
| user vs red, the same frames | | differ 68,994 (NIS01 900) / 60,321 (Mission01 2016) / 60,867 (Mission02 1000) pixels: the fleet's hulls follow the picked scheme on every level, the arrival level included |
| `m3r1i-tc-m3` (Mission03, the user's scheme) | **6 / 6 PASS** (`--mission 3 --teamcolour`: `after` + `level` at Mission03 (`afterLoads=3`), Mission04_NIS02 and Mission04 `after`, `end`; `failed=0` in each); the Mission03 self-test done in 17,825 ticks, 5 / 5 objectives in order (the rollcall jump at 17,472); the stream IDENTICAL in every field to `r10i-m3-a` and to `m3r1i-m3-a` over ticks 0-17793 (the whole self-test) | no capture (the self-test's per-level checks: the registry refreshed after every level load, the pending colours applied) |

The streams: `m3r1i-tc-user` and `-tc-red` IDENTICAL in every field to `r10i-missiontest-a` over ticks 0-11424 and to
each other over the whole run (the colours draw nothing from the streams); the small shifts in the region means against
the report (Mission01 2016: 93.0 / 118.2 / 32.2 against 95.6 / 122.6 / 32.0) are item 3's LOD-0 hull under the same
palette, the same frames otherwise.

### Fence 3: FULL DETAIL (the lodtest lines and the 4K frame time on an idle GPU)

The 4K cost (release binary, `--mode ai --ai-ticks 500 --res 3840x2160 --lodstats 150-450:fleet`: the camera on every
ship of the 156-ship fleet, 164 objects in view, 301 frames each; nvidia-smi **3 % before, 4 % after** the four runs
(no game running; the user's VS Code ripgrep held the CPU: load average 12.5-13.4); `claude-gpu-done` after the batch):

| run | FULL DETAIL | median ms | p99 ms | max ms | vertices / polygons a frame |
|---|---|---|---|---|---|
| `m3r1i-perf4k-on` | on (`noLOD 1`) | 9.578 | 11.389 | 13.347 | 25,169 / 46,255 |
| `m3r1i-perf4k-on2` | on | 9.083 | 13.689 | 14.260 | 25,169 / 46,255 |
| `m3r1i-perf4k-bands` | off (`/lodBands`) | 2.602 | 3.878 | 4.114 | 4,726 / ~5,700 |
| `m3r1i-perf4k-bands2` | off | 3.574 | 4.295 | 5.155 | 4,726 / ~5,700 |

The producer's 10.466 / 9.819 on and 2.713 / 3.308 off, the audit's 11.470 / 11.946 and 3.146 / 3.482 (under a heavier
load): the ratio on / off 3.0-3.6x in all three, the vertex counts exact; a FULL DETAIL frame of this scene is above a
144 Hz refresh's 6.94 ms on this machine (the user's question 4; owner M3-R2). The `uiscale:` line of every 4K run:
`physical 3840x2160 window 3840x2160 scale 200% (auto, uiScale 0) logical 1920x1080`.

The `lodtest` lines (debug binary; `--lodtest 300:fleet` at the ai scene, `--lodtest 300:far` at the render scene):

| run | FULL DETAIL | objects | drawn above level 0 | band above 0 | vertices / polygons | result |
|---|---|---|---|---|---|---|
| `m3r1i-lod-ai-on` | on | 164 (maxBand 4) | **0** | 164 | 25,169 / 46,255 | PASS |
| `m3r1i-lod-ai-bands` | off (`/lodBands`) | 164 | 164 (drawn = band) | 164 | 5,330 / 7,030 | PASS |
| `m3r1i-lod-render-on` | on | 1 (maxBand 2) | **0** | 1 | 458 / 558 | PASS |
| `m3r1i-lod-render-bands` | off | 1 | 1 | 1 | 162 / 196 | PASS |

Every object is drawn at level 0 under the default and at its band with `/lodBands` (the exe's bands still computed:
`bandAbove0` 164 in both). The toggle self-test's `fulldetail` checks (`m3r1i-toggle`, `-lodbands`: `switch run-only
noLOD=0 row=1`, `end noLOD=1 switch=0 row=1` / `noLOD=0 switch=1 row=1`) PASS: the switch never writes the row.

### Fence 4: the motion residual (flags on against `/noInterp`, the non-focus objects; this step's reruns)

Release binary, `--real-clock 144` (the wall clock paced to a 1/144 s grid, the offscreen driver), skirmish
LiveFireExercise2 with `--scene recon:1500:500:640` (the camera 1,500 units above Recon A, Recon B ordered 500 units
ahead: the user's recipe; plus C crossing, the formation G / g0 / g2, the docking Worker H, the harvesting Worker W, the
mothership M and its clamped module F, the asteroid D, the derelict E) and `--scene follow:1500:500:640` (the camera on
B); the NIS n06 on the real clock (`--mode nis --real-clock 144`); `tools/nis_smoothness.py <noi> <on> --span --jerk`
(`--world` for the objects' own drawn motion). Cells: per-frame displacement max / p95 / mean (px), the repeated frames
(moved under 0.05 px) / frame pairs in the object's own motion span, the jerk max / p95 (px). GPU 2-3 % before and after
(no game running), load average 12-13 (the user's ripgrep) during the batch; `claude-gpu-done` run after it.

| object (on screen) | `/noInterp` real 144 (`m3r1i-rc-recon-noi`) | flags on, real 144 (`m3r1i-rc-recon-on`) | the report's fix column |
|---|---|---|---|
| **B** (mover, the user's Recon) | 1.977 / 1.094 / 0.129, rep **832 / 941** (7.96 of 9 a tick), jerk 1.976 / **1.488** | 0.252 / 0.215 / 0.126, rep **0 / 941**, jerk 0.116 / **0.008** | rep 0 / 933, jerk p95 0.010 |
| C (crossing) | 2.301 / 1.860 / 0.197, rep 2580 / 2920, jerk 2.293 / 2.165 | 0.288 / 0.274 / 0.185, rep 3 / 2920, jerk 0.100 / 0.025 | rep 3 / 2912, 0.026 |
| G (formation wingman) | 1.319 / 0.874 / 0.113, rep 740 / 838, jerk 1.316 / 1.101 | 0.179 / 0.147 / 0.100, rep 0 / 838, jerk 0.067 / 0.023 | rep 2 / 856, 0.022 |
| g0 (formation lead) | 1.496 / 0.883 / 0.116, rep 778 / 881, jerk 1.501 / 1.152 | 0.195 / 0.165 / 0.103, rep 1 / 881, jerk 0.073 / 0.023 | rep 0 / 873, 0.023 |
| A (the focus) | at rest on screen | at rest | at rest |
| `follow`: A (at rest, carried by the camera) | 2.142 / 1.160 / 0.137, rep 916 / 1037, jerk 2.142 / 1.599 | 0.278 / 0.232 / 0.132, rep 0 / 1037, jerk 0.137 / 0.010 | rep 0 / 1020, 0.010 |
| `follow`: C (crossing) | 1.883 / 1.514 / 0.159, rep 2620 / 2964, jerk 1.880 / 1.728 | 0.237 / 0.216 / 0.149, rep 98 / 2964, jerk 0.046 / 0.020 | rep 99 / 2955, 0.019 |
| `follow`: G | 1.625 / 0.974 / 0.121, rep 939 / 1063, jerk 1.624 / 1.291 | 0.204 / 0.177 / 0.110, rep 58 / 1063, jerk 0.056 / 0.023 | rep 60 / 1072, 0.022 |
| `follow`: the camera (eye + look, world units) | max 12.313, rep poses 1619 / 5597, jerk 12.313 / 2.524 | max 1.602, rep 1441 / 5597, jerk 1.180 / 0.008 | - |
| NIS n06, O9 (sRecon, tracked; `m3r1i-rc-nis-noi` / `-on`) | 120.532 / 24.739 / 2.854, rep 1168 / 1320, jerk 120.532 / 42.306 | 40.948 / 6.352 / 2.935, rep 0 / 1333, jerk 2.734 / **0.322** | 0.231 (virtual) |
| NIS n06, the camera | jerk p95 492.256, rep poses 1224 / 1373 | jerk p95 **5.811**, rep 156 / 1381 | 5.58 |

The six kinds, each object's own drawn WORLD motion (`--world --span --jerk`, units; the camera left out):

| kind (object) | `/noInterp` real 144 | flags on, real 144 |
|---|---|---|
| crossing ship (C) | max 65.627, rep 3080 / 3483, jerk p95 35.808 | max 8.125, rep 1 / 3483, jerk 3.280 / 0.050 |
| mothership module (F, clamped; the mothership M moving: the same numbers) | max 8.750, rep 3490 / 3948, jerk 8.750 / 7.540 | max 1.258, rep 0 / 3948, jerk 0.555 / 0.009 |
| formation ship (G / g0 / g2) | rep 2339 / 2644, jerk p95 10.691; g0 11.039; g2 65.626 | rep 9 / 2644, jerk 1.199 / 0.015; g0 1.297 / 0.019; g2 4.417 / 0.052 |
| docking ship (H, sWorker#73 to the Processor, latched) | max 28.567, rep 4945 / 5502, jerk 28.567 / 26.562 | max 5.178, rep 691 / 5503 (the latched frames), jerk 1.909 / 0.024 |
| harvesting Worker (W) | max 26.565, jerk 26.565 / 26.565 | max 3.945, rep 15 / 5053, jerk 3.557 / 0.021 |
| the mover (B) | max 6.156, rep 1301 / 1470, jerk 6.156 / 3.965 | max 0.785, rep 9 / 1470, jerk 0.550 / 0.008 |
| asteroid (D), derelict (E) | at rest in the world | at rest |

The clock (the scene dumps' frame lines): flags on `alpha 0.1188..1.1144, alpha step min 0.0866 median 0.1158 max
0.1829, wraps 647, raw >= 1: 648 (max 1.1144), universe ticks a frame 0 x4951 / 1 x647 / more x0; frame time ms min
4.989 median 6.944 p99 6.944 max 10.984; task ticks a frame {1: 1477, 2: 4119, 3: 1}` (the report's fix row: 0.0638 /
0.1157 / 0.1770, 13 unblended; here 0 unblended frames); `/noInterp`: 5,597 unblended frames (alpha -1), raw >= 1 661
(max 1.1607). The repeated frames with the flags on are 0 for the user's Recon on the real clock at 144 Hz (the
`/noInterp` run repeats 7.96 of every 9 frames: the 16 Hz stepping the user saw); every number is within the report's
and the audit's (B rep 0 / 942, jerk p95 0.008 in the audit).

The virtual clock (debug binary, `m3r1i-v-recon-noi` / `-on`, 5,832 frames, ASan 0; the report's "on, virtual" column
reproduced to the digit): B 1.978 / 1.451 / 0.160, rep 446 / 501, jerk 1.978 / 1.693 -> 0.221 / 0.213 / 0.159, rep **0 /
501**, jerk 0.091 / **0.006**; C rep 2042 / 2297 -> 0, jerk p95 2.210 -> 0.022; G 139 / 156 -> 0, 1.300 -> 0.023; g0 189 /
212 -> 0, 1.414 -> 0.023; the world motion: C rep 3217 / 3618 -> 1, jerk 34.920 -> 0.014; the clamped module F 3482 / 3917
-> 0, 7.578 -> 0.009; the docking Worker H 5152 / 5705 -> 720 (its latched frames), jerk 26.562 -> **0.006** (the clamp
fix: max 0.585, the one-frame step over the latch tick); the formation G 2449 / 2754 -> 9, 10.691 -> 0.005; the harvesting
Worker W 4680 / 5263 -> 16, 26.565 -> 0.002; the clock line `alpha 0.1111..0.8889, step 0.1111 / 0.1111 / 0.1111, 648
unblended (raw exactly 1), universe ticks a frame 0 x5185 / 1 x647`.
The `follow` variant on the virtual clock (`m3r1i-v-follow-on`, 5,375 frames): A (at rest in the world, carried by the
camera) 0.241 / 0.230 / 0.166, rep **0 / 582**, jerk 0.097 / 0.007 (the report: 0 / 583, 0.007); C rep 0 / 2112, jerk p95
0.017; G 0 / 645, 0.023; g0 0 / 659, 0.024; the camera on B jerk p95 0.008.

### Fence 5: the UI scale and the 4K window (`tools/ui_scale_compare.py`, the uiscale self-test)

| comparison | runs | result |
|---|---|---|
| Main_game_screen, 3840x2160 auto 200% box-downscaled 2x vs 1920x1080 auto 100%, frames 400 and 800 | `m3r1i-fe-4k-shot` vs `m3r1i-fe-1080-shot` (`--mode fe --shot 0:400,0:800`) | **`--preset fe --tol 0`: every region match=1.0000, mean 0, max 0, at both frames** (fe-buttons 47,632 px, fe-title 77,355, fe-version-label 4,278, fe-panel 307,200, cursor 576): the 4K front end is the 1080p one pixel-doubled, to the bit |
| the in-game HUD (the hud self-test's scene), ticks 100 / 300 / 450, 4K vs 1080p, area 2x, tol 24 | `m3r1i-hud-4k` vs `m3r1i-hud-1080` (`--mode hud --shot 100,300,450`) | **PASS** at every tick: hud-info-panel 0.9996 (mean 0.04-0.09), hud-lists 0.9999, hud-support 1.0000, cursor 1.0000 (the residue is the 3D view behind the translucent panel and lists, drawn at 4K in one and 1080p in the other) |
| the pie plate's lines (3D, HUD), tick 300, 4K vs 1080p | the same, regions pie-lines 480,20,620,560 / pie-plane 0,530,1920,580 | match **0.9900 / 0.9981** (the report's numbers: the plate's width follows the scale) |
| Main_game_screen, 2560x1440 auto 150% vs 1707x960 auto 100% (the 150% frame's own logical size drawn at 100%), frame 800 | `m3r1i-fe-1440-shot` vs `m3r1i-fe-960-shot` | **centre-sampled, tol 0: every region 1.0000 / max 0** (nearest filtering: each logical pixel's centre is its colour); area-averaged (the 1.5 box), tol 24: buttons 0.9340 (mean 6.08), title 0.9558, version 0.9294, panel 0.9743, cursor 0.9288: the expected mixing of glyph edges at a non-integer factor, as the report states |
| the HUD, 1440p 150% vs 1707x960, centre-sampled, tol 24 | `m3r1i-hud-1440` vs `m3r1i-hud-960` | PASS: ticks 100 / 300 info panel 0.9998, lists 1.0000, support 1.0000, cursor 1.0000; tick 450 (the sensors manager) 0.9954 / 0.9961 / 1.0000 / 0.9844 |
| `uiscale:` lines | the captures | 4K `scale 200% (auto, uiScale 0) logical 1920x1080 factor 2.00000 x 2.00000`; 1440p `scale 150% ... logical 1707x960 factor 1.49971 x 1.50000`; 1080p and 1707x960 `scale 100%`; 1024x768 `scale 100%` |

**The scripted 4K click, the edges, the fit** (the uiscale self-test, `--mode uiscale`, debug binary; **45 / 45 checks PASS
in all nine runs**: `m3r1i-us-4k-a` / `-b` (3840x2160), `-us-1440`, `-us-1080`, `-us-1024`, `-us-fit1960`, `-us-fit2160`,
`-us-4k-cfg100`, `-us-4k-sw150`):
- 4K (auto 200%, logical 1920x1080): `click miss window=2129,1205 -> logical=1064,602 button=858,586-1064,618
  screen=Main_game_screen` (one logical pixel right of the OPTIONS button: a miss) and `click hit window=1923,1205 ->
  logical=961,602 ... (640x480 218,286-424,318) screen=Audio_Options` (the button's centre: the options open); the mouse:
  `the physical bottom row window=1920,2159 -> logical=960,1079 region=732,956-1226,1133` (a region reaching the bottom
  edge), `the bottom-right corner window=3839,2159 -> logical=1919,1079`, `50 rows below the window (a held button)
  window=1920,2210 -> logical=960,1079`, `right of the window window=3849,1080 -> logical=1919,540`: the clamp never cuts
  a row or column off (item 6); `fit display (1920x1080, 3840x2160): 3840x2160 -> 1, 1920x1080 -> 0, 5120x2880 -> 1;
  (3840x2160, 1920x1080): 3840x2160 -> 0` (the display chosen by the request's size); `switch run-only uiScale=200 row=0`;
  `console 'uiScale 150' -> uiScale=150`; `modeswitch request=3840x2160 laid out at 3840, then logical=1920x1080
  main=1920,1080 options button=858,586 screen=Main_game_screen` (the audit's relayout).
- 2560x1440 (150%, 1707x960): miss `1435,813 -> 957,542` (the edge column, outside) / hit `1281,813 -> 854,542 ->
  Audio_Options`; the bottom row `1280,1439 -> 853,959`, the corner `2559,1439 -> 1706,959`.
- 1024x768: the live Accept at 200% `uiscale: applied scale 160% (cfg, uiScale 200) logical 640x480 (was 1024x768)`
  (the cap) and back `applied scale 100% (auto, uiScale 0) logical 1024x768 (was 640x480)`.
- The forced bounds: `m3r1i-us-fit1960` (`-- /testUsableBounds 3840x1960@3840x2160`) logs `window: requested 3840x2160,
  usable 3840x1960 (display 0: 3840x2160), created 1024x768 (fullscreen-desktop): display size -> fullscreen desktop`,
  `-us-fit2160` the same with usable 3840x2160 (the offscreen driver gives its fullscreen-desktop window its own 1024x768
  mode: the decision and the flags are the check; on the user's desktop the created size is the display's); without the
  switch every run logs `usable none ... as requested (no desktop bounds)`.
- The overrides: `-us-4k-cfg100` (`--cfg 'uiScale 100'`) `scale 100% (cfg, uiScale 100) logical 3840x2160`; `-us-4k-sw150`
  (`-- /uiScale 150`) `scale 150% (switch, uiScale 150) logical 2560x1440`; the Mission01 stream under `/uiScale 200`
  (`scale 160% (switch, uiScale 200) logical 640x480` at 1024x768) and `uiScale 150` (`logical 683x512 factor 1.49927 x
  1.50000`) byte-identical to A (the guard-rail table).
- fe and sp at 3840x2160 with ASan 0: the exit-test table (`m3r1i-fe-4k`, `m3r1i-sp-4k`).

## The five ports

| # | item | what landed | DONE WHEN | audit | commit |
|---|---|---|---|---|---|
| 1 | the letterbox bars in front + the letterbox setting ([report](m3-r1-letterbox-front.md), [audit](audits/m3-r1-letterbox-front.md)) | **The cause was the M2-R6 port of `cat_popupMenuDraw` 0x004ee120, not GoK and not the scissor flag**: the exe's last depth call of the main region is `push [0x00a57628]; call cat_rndDepthTestEnable` (0x004ee5bd), a .bss word nothing writes (one reference in the whole exe), so the depth test stays OFF for the rest of the frame and `rndDrawScissorBars` 0x0057a410's late bars cover the scene; the tree's port passed TRUE, so the bars were depth-tested (GL_LEQUAL at z 0) and every ship nearer than about twice the near plane drew over them. **The exe never clips the 3D view** (`rndScissorEnabled` is the GL DLL's own `glIsEnabled`, FALSE every frame there too: the scissor box of `mrRegionDraw` 0x00569260 is dead in both). One line fixes the full, fading and KAS letterbox (`CAT`, under `HWI-FEATURE(letterbox-setting)`). `rndDrawScissorBars`: the fading bars blend normally (`CAT`: the exe turns additive blending off for the fade branch) and the additive state is restored (`CAT-FIX`: the exe's full-bars branch restores ebx, the top bar's GL y, so the exe leaves additive blending ON after full bars). **The setting `letterboxBehind`** (Cataclysm.cfg row, default 0; the run-only switch `/letterboxBehind`; the console row; no Options control: the task asked for none) brings the M2 look back bit for bit. **The shared capture**: `/catShot <spec>` (any mode; `<prefix>.<tick>.<frame>.ppm`, `.s<N>.` after a level load), `/catShotLetterbox`, `/catNisScissorOut <tick>:<s>`; harness `--shot`, `--shot-letterbox`, `--res WxH`, `--cfg 'KEY VALUE'`; `tools/ppm_region.py` (info, stats, probe, diff, down2, bands). Fence: the sRecon crossing the bottom bar at n06 dump frame 1374: 109,568 of 109,568 bar pixels black (the old look 29,254 non-black); Mission01's two NISes 1,390 frames, 31 differ between the settings, 0 non-bar pixels in the bars in each; the fade frame 32,32,32 over the ship; Mission02's KAS letterbox fades in from grey. Dumps: the motion dumps unchanged; the two default nis dumps moved in `crc=` only (279 / 277 frames: the two passes of the Recon through the bar). The letter height stays GoK's `NIS_LetterHeight` (277 rows at 3840x2160; the exe's `(H - W*20/37)/2` gives 42): the user's question. 1 IDENTICAL found modified (`rndDrawScissorBars`), 6 review rows | met: the bars bar-coloured over the ship in the full, fading and KAS letterbox with the default; the old look back with the setting; no stream moved in any field; the dumps moved in crc only, stated; the capture usable in any mode | PASS-WITH-FIXES, **0.919** (34/37). Fixes: the "uninitialised ebx" wording (it is the bar's GL y: the exe deterministically leaves additive blending on after full bars), two fade-frame pixel counts. Open: whether the exe's additive-on state after full bars (the subtitles / cursor blend additively there) should be kept instead of the port's restore (O1); the letter height (O4) | `94599c8` |
| 2 | the team colours across the warp ([report](m3-r1-team-colours-warp.md), [audit](audits/m3-r1-team-colours-warp.md)) | **The "warp" was the jump from the opening cutscene's staging level into Mission01** (the user's log never reached Mission02): the registry and the scheme were right before and after the switch (the user's ff1bca82 / ffff5539 at palette 0 of every Kuun-Lan texture), but Mission01 needs more texture memory than its NIS level, and the tree's `trRegistryRefresh` kept HW1's measurement loop, which listed the already-LOADED textures: `trCramRAMComputeAndScale` 0x00589a50 then halved them to the 6 MB budget (TextureDetail 50) and `trPreLoadedTextureScale` 0x00586bd0 reloaded them, and the reload built each palette with a stale listing-flags word (no team bit), so 240 textures of the Kuun-Lan, its modules, the Worker and the Recon came back untinted. **The exe lists only PENDING entries** (`trRegistryRefresh` 0x00587a50: `test al,4; je; test al,1; je 0x00588246` at 0x00587d1c / 0x00587d24, past the `trMeshSortAdd` call at 0x0058823e), so a loaded texture is never shrunk or reloaded at a level change: the audit replaced the producer's CAT-FIX (a loaded entry passes its own flags) with the exe's gate, a plain `CAT` port; loaded textures keep their size and palettes into the next level (473 fewer file loads in Mission01 + Mission02). **The self-test** `/catTeamColourTest` (harness `--teamcolour`, `--player-colours BASE/STRIPE`; src/Game/TeamColourTest.c): the registry dump at each level change and the checks of every Somtaaw texture's scheme-0 palette; 34 / 34 checks PASS over the whole `sp` chain (Mission01_NIS01 .. Mission12_NIS07) with the user's scheme, 30 FAIL without the fix; the Kuun-Lan's 24x24 region follows the base colour (green over red and blue with the user's, red with a contrasting scheme, grey-brown without the fix). **The heap input for the soak**: the "dupe trcolorinfo list" leak (one 64-byte block per texture reload: `trTextureColorsUpdate` 0x00585fa0 / `trRegisterRemoval` 0x00585e70 / the tree's `trRegisterAddition` overwrite the list `trInternalTexturesDelete` stored; R10's O4) is now 1,587 live blocks at Mission09 and 1,898 at Mission12_NIS07 (the producer's tree 2,437 / 3,308; the reloads gone). No stream moved; the dumps equal. 11 review rows (+3 audit notes) | met: the self-test PASS at Mission02 and at the level after an NIS level (Mission04 after Mission04_NIS02) with the user's scheme; the PPM region follows the base colour; no stream or dump moved | PASS-WITH-FIXES, **0.91** (20/22). Fixes: the cause attributed to the exe and the CAT-FIX built on it replaced by the exe's pending-only gate (`CAT`); the heap baseline; `tools/play.sh --rebuild` in the user's check; a hook placement | `2b454b6` |
| 3 | FULL DETAIL = LOD 0 always, default ON ([report](m3-r1-full-detail.md), [audit](audits/m3-r1-full-detail.md)) | **What the exe's FULL DETAIL does**: `opNoLODVal` 0x00a4cc34 has one reader outside the options code, `rndMainViewRenderFunction` 0x005764e0 at 0x00576c03: it ends the frame-time detail passes after pass 0 (`cat_lodLevelGetTimed` 0x004a5000 over the table 0x0072be84 while the estimated render time exceeds `catLodTimeThreshold`); `lodLevelGet` 0x004a4d70 never reads it and still drops small / far objects to levels 1..5 by the size-on-screen bands (0.4 / 0.12 / 0.045 / 0.017 / 0.0065 of the selection radius) from the DetailQuality radio, so the user's "noLOD 1, models still change" is the exe's behaviour; the detail slider is a frame-rate floor (30 - slider fps), greyed while FULL DETAIL is on. The tree already had all of that. **The feature** `HWI-FEATURE(full-detail)`: `cat_lodDrawLevel` / `cat_lodDrawLevelGet` (LOD.c) give the level DRAWN (0 under FULL DETAIL when the table has a mesh at 0) while `currentLOD` keeps the exe's level for every non-draw reader; the draw sites (ships and their conversion / mimic meshes, asteroids, derelicts, missiles, dust and gas clouds, nebula tendrils, the sensors manager's meshes, nav lights, repair droids, the trail nozzles) read it; kept on the exe's level because they draw a random stream or feed the simulation / UI: `renderedLODs` (KAS `RenderedShips` / `RenderedDerelicts`, `aitCheckAmIBeingWatched`; the audit guarded the bit to the exe's mesh path), the effect levels, the droid runtime, the trail style, the ACV link bolts, the ship lightning width, the selection and status bars, the Beast-stun edge lightning (stream 0; a LOD-0 lightning moved `rng0` in Mission04 / 10 / 14 and was caught and fixed). **Default ON** (`noLOD 1`, the exe's own row and FULL DETAIL checkbox; the high preset 1, the low 0; the run-only switch `/lodBands`; the console row `noLOD`). `/catLodTest` (`--lodtest`): 164 ships at the ai scene all drawn 0 (bands 4 x132 / 3 x10 / 2 x22), with `/lodBands` drawn = band; `/catLodStats` (`--lodstats`). **The 4K cost** (release, offscreen, idle GPU, 164 ships): on ~10 ms median / ~15 ms p99 (25,169 vertices), off ~3 ms (4,726): vertex submission (the same at 1024x768), under 144 Hz in that scene. **The effects cap** is not lifted (Relic's per-effect limits applied in the universe update, `etgFrequencyExceeded` 0x00470bd0 draws RANDOM_ETG; the high preset already sets 256): M3-R2 if wanted. 17 / 17 missions IDENTICAL; the default dumps move in crc only (motion 603 / 605 of 1,431 frames, nis 1,260 / 1,267 of 1,440: the LOD-0 meshes), with `/lodBands` equal to the previous audit's. 5 review rows (one the audit's, +1 audit note) | met: the exe's FULL DETAIL and slider stated from the machine code; LOD 0 at every draw site under the setting; default ON persisted by `noLOD`; the effects-cap verdict; no simulation reader of the drawn level (after the audit's `renderedLODs` guard); the 4K on / off cost on an idle GPU; the Mission01 pair IDENTICAL; the per-mission self-tests PASS and IDENTICAL; the rung 2823 | PASS-WITH-FIXES, **0.913** (42/46). Fixes: `renderedLODs` set only when the exe's level is a mesh (a latent simulation reader: the planet derelicts' SubPixel level; no shipped script reads them), 3 untagged lines, the `nebRenderNebula` review row, +0x20 -> +8, the stale function-deltas report | `8410122` |
| 4 | the unfocused objects' jitter ([report](m3-r1-motion-jitter.md), [audit](audits/m3-r1-motion-jitter.md)) | **The play-test ran with Smooth motion OFF**: the play-home options file says `objInterpolate 0` / `nisInterpolate 0` (rows 107-108; the code default is 1; the user did not know the checkbox existed). Two write paths: the M2-R6 builds of 2026-10-18 persisted `/noInterp` (the user's four 10-18 sessions fall in that window), and **a live bug**: the Smooth motion checkbox is a copy of OP_NoLOD whose .fib flags carry FAF_CallOnDelete, so `opSmoothMotion` also ran at the screen's deletion and set both flags from the box: Cancel did not cancel, and under `/noInterp` an untouched Video screen closed with Accept wrote the switch's 0 (both reproduced by the toggle test, FAIL before the fix). Fixed: only a click sets the flags (the exe's own OP_NoLOD keeps its deletion call). **The measurement**: `/catMotionScene` (harness `--scene`: the user's recipe on LiveFireExercise2: the camera at the zenith of Recon A, Recon B ordered 500 units ahead, plus a crossing ship, a formation, a docking Worker, a harvesting Worker, the moving mothership's clamped module, an asteroid and a derelict), `/catMotionRealClock` (`--real-clock FPS`: the wall clock paced to a 1/FPS grid), `tools/nis_smoothness.py` (`--span`, `--world`, `--jerk`, the clock line). **Three residuals found and fixed** (all `HWI-FEATURE(motion-interp)`, no setting): (1) the render task runs before the universe task, so one frame a tick has alpha above 1 on the wall clock (1.0 .. 1.10 at 144 Hz) and was drawn at the tick: `MI_ALPHA_MAX` 2, the frame drawn at its own time (the NIS curves at the render time; exactly 1 unchanged); (2) GoK's 4 ms task clock with a millisecond remainder gave alpha steps of 0.100 / 0.117 / 0.133 (a 14 % speed ripple): `taskClockSample` / `taskSubTickRender` read SDL's performance counter (the exe's QPC / 240 clock itself stays `CAT-TODO(M3 soak)`); (3) the dock latch: a ship clamped during a tick jumped 5.277 units to its clamp point and stood eight frames: `MiEntry.clamped`, the newly clamped child keeps its own blend for that tick. The user's Recon B on the real clock at 144 Hz: repeated frames 41 / 933 -> 0, jerk p95 0.143 -> 0.010 px (/noInterp: 825 / 933, 1.49 px); the NIS camera jerk p95 39.99 -> 5.58. Ruled out with numbers: alpha drops / several ticks a frame, objects the capture misses, teleport false positives, the camera, draws outside the bracket (one tutorial pointer case owned), frame pacing (the M3 soak's). No stream moved; the /noInterp dumps and the flags-on motion dump EQUAL; the flags-on nis dump moved by fix 1 (160 of 1,440 frames: the NIS task's phase). 1 review row | met: interpolation in the user's session shown OFF with both write paths and the live one fixed; the residual measured for the six kinds at 144 Hz virtual and on the real clock; every candidate kept or ruled out with a number; the before / after table; the dumps stated; the Mission01 pair IDENTICAL with the flags on, `/noInterp`, `/noNISInterp` | PASS-WITH-FIXES, **0.853** (29/34). Fixes: one figure, the CAT-TODO line naming M3-R1 reworded, the `--scene` header, two missing tags, a B-Spline comment | `fdf35ee` |
| 5 | the UI scale 200% at 4K / 150% at 1440p and the 4K window ([report](m3-r1-ui-scale.md), [audit](audits/m3-r1-ui-scale.md)) | **The exe has one size**: MAIN_WindowWidth / Height are the client size and the render size; its 2D path normalises by them, so at 3840x2160 the 640x480 front end is a sixth of the screen; nothing scales. The front end centres on `(MAIN - 640) / 2` (`feAllScreensReposition` 0x00474ae0), the HUD anchors on MAIN_Window*'s edges: the layout holds at the logical size, nothing to port. **The feature** `HWI-FEATURE(ui-scale)` (src/SDL/uiscale.c): three sizes: the window REQUEST (the options file's `screenWidth` / `screenHeight`, kept in uiscale.c once the window exists), the PHYSICAL size (the drawable: the GL viewport, every scissor box and read-back, the 3D projection's aspect and point sizes) and the LOGICAL size (physical / scale, rounded: MAIN_WindowWidth / Height once the window exists: every 2D layout and draw, the mouse, picking); `uiScale` 0 = auto by the physical height (200% at 2160 rows or more, 150% at 1440 or more, 100% below), 100 .. 400 fixed, capped at a 640x480 logical screen. 2560x1440 -> 1707x960 with the exact per-axis factor in the projection and the mouse; a window pixel maps to the logical pixel its centre falls in; nearest filtering at every scale (at 2.0 an exact pixel-doubling); the 2D line widths and the pie plate's lines / stipple follow the scale; the 3D view (the aspect, the stars, the points) is the physical size's. Surfaces: the cfg row `uiScale`, the run-only switch `/uiScale N`, the console row, the Video-options control "UI SCALE" AUTO / 100% / 150% / 200% (copies of OP_Effects' radio buttons on Video_Options and the in-game Video screen; applied at once at a front-end Accept, at the next start from a game). **The window at 4K (item 6, `HWI-FIX`)**: the exe's windowed mode makes the client size plus the frame at the screen's origin (`WinMain` 0x005631f0), so a desktop-sized request is partly off-screen there too; the fit (`uiScaleWindowFit`): a windowed request of the display's size or larger -> SDL fullscreen-desktop on the display chosen by the request's size (borderless, above the panel), larger than the usable bounds but smaller than the display -> shrunk to the usable bounds keeping the aspect, else as requested; `/noBorder` = the exe's WS_POPUP (`CAT`); `mousePositionSet` 0x0056e8d0's clamp ported (`CAT`); the poll clamps to the logical screen, so the physical bottom row lands on the logical bottom row. **The self-test** `/catUiScaleTest` (harness `--mode uiscale`, 45 checks): the scale and logical size, the fit over nine desktops and the display choice, the mouse at the bottom row / corner / outside the window, a scripted click at the OPTIONS button's centre (hit) and one logical pixel right of it (miss), the control's buttons at the exe's centring, a real ACCEPT click applying 200% at once, the row / switch / console; `/testUsableBounds WxH[@DWxDH]`; `tools/ui_scale_compare.py`. Fence: the 4K front end box-downscaled 2x equals the 1080p frame to the bit (tol 0 over every region), the 4K HUD matches 0.9996+, the 1440p frame centre-sampled equals the 1707x960 frame to the bit; 1024x768 unchanged from fdf35ee. 17 review rows (+4 audit notes; the producer's four IDENTICAL -> MODIFIED rows reverted: the exe equals HW1 there, the differences are GoK's). One `CAT-TODO(M3-R2)`: `btgConvertStar`'s background-star size (the exe's is a fixed share of the screen, GoK's a fixed pixel size) | met: the 4K frames downscaled equal the 1080p frames within the tolerance for Main_game_screen and the HUD; the 1440p frame at 1.5; the scripted 4K click hit and miss; the bottom-row events reach the logical edge; the forced-usable-bounds fit; the cfg row / switch / console / control; no stream or dump moved; fe and sp at 3840x2160 ASan 0 | PASS-WITH-FIXES, **0.857** (42/49). Fixes: the control's atoms placed at their 640x480 coordinates after the screen was centred (off the panel at any logical size above 640x480; proven fail -> pass), the mode switch's relayout at the request (the main menu off-centre after a resolution change; proven), 4 wrong delta-map rows, 4 missing review notes, a CAT tag, 2 text errors. Open: GoK's `mainDestroyWindow` makes the next quit's first SDL_QUIT be ignored after a resolution change (owner M3-R2); the intermittent `AddressSanitizer:DEADLYSIGNAL` in the quit path after a timeout kill of a 4K `sp` run (once in four; owner M3 soak) | `58df92d` |

**Round size** (`git diff --stat ce4fab4..58df92d`, the round base to the last audit's checkpoint): 66 tracked files changed,
+7,891 / −217 lines; `src/` alone: 44 files, +4,080 / −95, with 26 added lines carrying `CAT` (7 `CAT:`, 1 `CAT-FIX`);
new `src/` files: `src/SDL/uiscale.c` / `.h`, `src/Game/TeamColourTest.c` / `.h`, `src/Game/UiScaleTest.c` / `.h` (the
builds link `[273/273]`, R10's `[270/270]`). The tree now carries 4,465 `CAT:` lines (R10: 4,459), **37 lines carrying
`CAT-TODO`** (35; **28 owner tags: stage 5 23** (plain 8, multiplayer options 8, LAN 6, multiplayer 1), **M3 oracle
physics 3**, **M3 soak 1** (the task clock, utility.c), **M3-R2 1** (the BTG star size); 9 other lines mention closed tags
in prose), **4 `CAT-PROVISIONAL`** (4), 13 `CAT-DORMANT` (13), **195 `CAT-FIX`** (194), 49 `HWI-ORACLE` (49), **222
`HWI-TEST`** (143), **53 `HWI-FIX`** (27), 6 `HWI-EXPERIMENT` (6); the feature tags (`/bin/grep`): `HWI-FEATURE(motion-interp)`
**484** (441), `HWI-FEATURE(letterbox-setting)` **36**, `HWI-FEATURE(full-detail)` **178**, `HWI-FEATURE(ui-scale)` **299**
(new). Tools: `tools/m2_harness.sh` (30 modes: `uiscale` new; the flags `--shot`, `--shot-letterbox`, `--res`, `--cfg`,
`--player-colours`, `--teamcolour`, `--lodtest`, `--lodstats`, `--scene`, `--real-clock`), `tools/ppm_region.py` and
`tools/ui_scale_compare.py` new, `tools/nis_smoothness.py` extended, `tools/play.sh`'s header documents every setting.

## Delta map after the round

The ports recorded their comparisons as reviews in `ghidra/delta_map_review.json` and merged them with `tools/delta_map.py
merge` (decision 55; the audits appended their own notes). Notes tagged `[review: M3-R1 <port>]`: letterbox-front 6,
team-colours-warp 11, full-detail 5 (one written by its audit), motion-jitter 1, ui-scale 17: 40 in all, plus 9 `[audit: M3-R1
<port>]` notes (letterbox-front 1, team-colours-warp 3, full-detail 1, ui-scale 4). This step added no review of its own and
ran no merge; `python3 -I tools/m2_stage_counts.py --md` on the tree's map gives the table.

| stage | HW1 files (main ones) | IDENTICAL fn / B | MODIFIED fn / B | REWRITTEN fn / B | cat-new fn / B (est.) |
|---|---|---:|---:|---:|---:|
| 1 file formats + data load | Mesh, MeshAnim, texreg, LOD, StatScript, SaveGame, BigFile, File, ObjTypes, StringSupport, Tweak, LZSS/BitIO/CRC32 | 210 / 41,085 | 105 / 54,159 | 0 / 0 | 76 / 27,383 |
| 2 universe and ships | Universe, UnivUpdate, CommandLayer, Collision, Physics, Gun, Dock, Formation, ETG, Particle, Select/ShipSelect, Stats, ResearchAPI, Ships/* | 724 / 133,603 | 648 / 267,702 | 30 / 4,444 | 808 / 180,868 |
| 3 UI and presentation | FEFlow/FEReg, UIControls, ConsMgr, ResearchGUI, Options, NIS, sound/speech, render/GL, main/mainrgn, mouse | 845 / 182,031 | 339 / 202,253 | 14 / 8,558 | 450 / 139,196 |
| 4 AI | AI*.c | 291 / 43,874 | 186 / 79,829 | 2 / 169 | 23 / 14,404 |
| 5 LAN networking | CommandNetwork, MultiplayerGame, MultiplayerLANGame, TitanNet, NetCheck, ChannelFSM, chat | 404 / 69,517 | 116 / 76,194 | 12 / 1,179 | 67 / 17,483 |
| 6 missions | KAS, KASFunc, Timer, Objectives, LevelLoad, SinglePlayer, Tutor + kas2c mission code | 288 / 19,469 | 181 / 53,276 | 25 / 1,505 | 2,365 / 656,918 (2,085 / 612,687 B are kas2c mission functions) |
| **total** | | **2,762 / 489,579** | **1,575 / 733,413** | **83 / 15,855** | **3,789 / 1,036,252** |

IDENTICAL 2,762 / MODIFIED 1,575 / REWRITTEN 83 (R10: 2,763 / 1,574 / 83: `rndDrawScissorBars` 0x0057a410 IDENTICAL ->
MODIFIED by producer 1; producer 5's four reclassifications reverted by its audit), 3,573 reviewed rows (R10: 3,569);
**210 MODIFIED / REWRITTEN rows still without a `reviewed` flag (118 outside `TitanInterface.cpp`**: ETG.c 41, Particle.c 38,
ObjTypes.c 16, MultiplayerLANGame.c 7, MultiplayerGame.c 5, GameChat.c 3, StatScript.c 3, one each in Captaincy.c,
CommandNetwork.c, File.c and two more; R10: 212 / 120). Bodies compared with the exe this round and found as the tree (no
row or a note): 14 by producer 1's audit, 15 by producer 2's, 12 by producer 3's, 8 by producer 4's, 12 by producer 5's.

## What is left

Nothing blocks the next play-test; each item has an owner (M3-R2, the M3 soak, M3 oracle physics, the user, or after M3):

1. **From this round's ports** (the five reports' "What is left"):
   - the **letter height at wide aspects** (letterbox-front): GoK's 2.39:1 (`NIS_LetterHeight`, 277 rows at 3840x2160)
     against the exe's 1.85:1 (42 rows, `(H - W*20/37)/2` at 0x005693c7): a look decision of the user's (below); if the
     exe's is chosen, a one-line change (owner M3-R2); **the exe's additive-on state after full bars** (the audit's O1):
     the exe leaves additive blending on after `rndDrawScissorBars`' full-bars branch (its `ebx` is the top bar's GL y),
     so its subtitles and cursor drawn after the bars blend additively; the port restores the state (`CAT-FIX`): the
     user may want the exe's look instead (below; owner M3-R2 if so);
   - the **colour-info leak O4** (team-colours-warp; one 64-byte "dupe trcolorinfo list" per texture reload, the exe
     alike): 1,587 live blocks at Mission09 / 1,898 at Mission12_NIS07 with the pending-only gate (the producer's tree
     2,437 / 3,308), the M3 soak's baseline (owner M3 soak); the texture budget (TextureDetail 50 = 6 MB; only a level's
     NEW textures are shrunk now, loaded ones keep their size as in the exe) is a quality question for FULL DETAIL, not
     touched; GoK's `utyShipsAlwaysUseOwnerColors` condition (harmless while FALSE; owner M3-R2 only if the row is
     exposed);
   - **FULL DETAIL at 144 Hz** (full-detail): the mesh submission (~10 ms median / ~15 ms p99 at 164 ships at 4K, the
     same at 1024x768: CPU vertex submission; owner M3-R2 with docs/features/high-refresh-4k.md); **the effects cap /
     effect level 0** under FULL DETAIL (Relic's per-effect limits in the universe update, `etgFrequencyExceeded`
     0x00470bd0 draws RANDOM_ETG: needs the ETG callback audit and a stream proof; owner M3-R2 if the user wants it);
     **trails at the LOD-0 style** (their TRAILS-stream draws follow the level and would move `rng_all`: the user's call);
   - **the task clock** (motion-jitter; `CAT-TODO(M3 soak)` in utility.c: GoK's `SDL_GetTicks / 4` = 250 task ticks a
     second, a 60 ms universe tick, 4 % fast on the wall clock, against the exe's QPC / 240: a simulation-timing change
     measured with the soak's universe tick rate); **frame pacing at 4K** (a frame above the refresh interval presents 1
     or 2 refreshes late: the soak's `/frameLog` and the user's refresh-rate answer; owner M3 soak); the tutorial's
     `TUT_PointerTypeShips` pointer computed after `miRenderEnd` (owner M3-R2, only if the user sees a pointer step);
     the piecewise-linear velocity kink at each tick (B's world jerk max 0.281 units at its start / stop, p95 0.005: not
     a jitter, no owner unless the play-test asks);
   - **the BTG background-star size** (ui-scale; `CAT-TODO(M3-R2)` at `btgConvertStar`: the exe's star is a fixed share
     of the screen, GoK's a fixed pixel size: a look decision for the fix round); **the window fit on the user's real
     desktop** (COSMIC XWayland's display order, usable bounds and frame extents: only the forced bounds are checked
     here; the play-test confirms it, M3-R2 if it misbehaves); the sensors manager's 3D map lines and the hyperspace /
     trail lines stay physical-pixel thin at 4K (M3-R2 if the user finds them too thin at 200%); **the quit after a
     resolution change** (GoK's `mainDestroyWindow` clears `mainActuallyQuit`, so the next quit's first SDL_QUIT is
     ignored; owner M3-R2); **the intermittent `AddressSanitizer:DEADLYSIGNAL` in the quit path after a timeout kill**
     of a 4K `sp` run (once in four 1,200 s runs in the ui-scale round, no stack; this step: none in its two `sp` runs,
     the table above; owner M3 soak).
2. **The oracle's rung** (M3 oracle physics; unchanged this round): 10-17 tick 2823 `ships_vel` (ship 48) and 10-18
   tick 2871 `ships_vel` (ship 49), both a bProcessor following the bWorker 44 by `protectShip`'s follow-behind point
   while the Worker turns to dock / spins while harvesting (decision 100: the Worker's coordsys, invisible to the
   stream without decision 48's recording); the three `CAT-TODO(M3 oracle physics)` (`RangeToTarget` /
   `RangeToTargetGivenDist`'s unrounded returns, `cat_gunCanFire`'s second mode, `cloudUpdateSystem`), the other inlined
   `frandom` sites (LevelLoad.c's first), Mission17's self-targeting KAS teams (docs/m2-r10-summary.md "What is left" 1).
3. **The mode-B rung** (the user's session, decision 60 / "What the user must decide" below): the GOG playback of
   `hwirec01` / `hwirec02` with the DLL; still no `original/oracle/hwirec*-play-*` directory (this step's `ls`).
4. **Decision 48** (the user's): no recording with the fixed DLL (the newest `original/oracle/oracle01-rec-*` is still
   `20261019-143530`); `ships_rot` stays excluded in every session.
5. **Stage 6's play-path items for M3** (unchanged from R10): the attended animatic check (the brief), the mission
   self-test's combat (M3 test), Mission17's self-targeting KAS Taiidan teams (M3 missions / oracle physics), the chained
   campaign's heap (M3 soak: now with O4's counts above and the pending-only gate's 473 fewer file loads), `trTextureRegister`'s
   name case (M3 render, only if a texture is ever registered twice), decision 94's nebula `CAT-FIX` (the user's).
6. **Stage 5** (after M3, decision 9): the 23 `CAT-TODO(stage 5 ...)` sites, unchanged (plain 8, LAN 6, multiplayer
   options 8, multiplayer 1).
7. **Stage 4 / 3 leftovers that stay**: `gunResetGimbleGuns` uncalled (58), the five creators' default tactics (59), the
   folded handlers' save indices (63), HW1's cursor text (65), `rndLoadTarga`'s `GL_CLAMP_TO_EDGE` and the Transformer
   (28); **closed this round**: the letterbox depth state, the texture registry's reload at a level change, the Smooth
   motion checkbox's deletion-time write, the three real-clock interpolation residuals, the UI's one size.
8. **Motion interpolation** (decision 46): the `/noInterp` dumps' content unchanged in every non-crc column since R9;
   the flags-on nis dump moved once (fix 1); the clamp blend now measured (H, the docking Worker: 5.277 units -> 0.585 a
   frame); the droid blend still unmeasured.
9. **The delta map's review flags**: 210 MODIFIED / REWRITTEN rows without a `reviewed` flag (118 outside
   `TitanInterface.cpp`; R10: 212 / 120).
10. **Still open, the user's**: the two saves of the M0 play-test (8), the play-test sign-off (the brief below), the
    fourth recording (4), the mode-B playback (3), decision 94's `CAT-FIX`, and this round's questions (below).

## Plan for M3-R2 and the rest of M3 (replaces M2-R10's "Plan for M3" table)

The order the user set after the play-test ("we need to fix bugs before we get further from the mark"): this round
fixed the six reported items; the next round carries the two platform items M2-R10 planned (rows 1 and 2, untouched
since), the oracle's rung (row 3) and the fix items this round left, sized by the user's next play-test (row 0). The
play-test can start now with `tools/play.sh --rebuild` (the brief below).

| # | step | scope | counts | done when |
|---|---|---|---|---|
| 0 | **the user's next play-test** (now; the brief below) | the six items re-checked (the bars, the colours across every jump, FULL DETAIL at the far zoom, the moving Recon at the zenith with Smooth motion ON, the 4K UI at 200% and the window, the mouse at the edges), then M2-R10's items (the two animatics attended, the campaign as far as it goes, the tutorial's 12 lessons, a skirmish and a recording) | 3-6 hours | a report per item (what differed from the GOG game, every stop with the log's last lines, the frame rate's drift, the answers to the questions below); M3-R2 sized from it |
| 1 | **the Windows 11 build** (producer; M2-R10's row 1, unchanged) | meson + SDL2 + GL with clang (clang-cl / lld-link, decision 7) or MSVC; the offscreen video driver, ASan / LSan, `long` 32-bit on Win64 (the LP64 save blob: fixed-width types or per-platform saves), the path layer, the harness under Git Bash / MSYS2 (the modes the gate needs); the x87 helpers' libm dependence measured first (the Mission01 stream, both replays and one mission run against the Linux streams); **new this round**: `src/SDL/uiscale.c`'s SDL display bounds and the fullscreen-desktop fit on Windows, the nearest-filtered 2x UI | 2 platform layers, 30 modes, 1 format pin, 1 window layer | the release and debug builds on Windows 11 with 0 warnings; the self-tests PASS there once (the uiscale self-test included); the headless Mission01 stream, the two replays and a mission stream IDENTICAL to the Linux ones (or every difference localised to a libm call and closed); a save and a recording made on one OS load and replay on the other |
| 2 | **the one-hour 4K@144 soak, no gamescope** (producer + the user's hour; M2-R10's row 2 with this round's inputs) | the `/frameLog` switch (HWI-TEST: per-second frame-time percentiles, the universe tick rate, the heap's used / growth counters); the headless proxy (the chained `sp` loop past Mission09 for an hour on the virtual clock with the heap counters: O4's 1,587 / 1,898 blocks as the baseline; the 64 MB main heap against the exe's 24 MB); **this round's inputs**: the task clock (`CAT-TODO(M3 soak)`: 250 task ticks a second against the exe's QPC / 240), frame pacing at 4K (FULL DETAIL's ~10 ms frame against 6.9 ms at 144 Hz: a frame presents 1 or 2 refreshes late), the `DEADLYSIGNAL` in the timeout-kill quit path of a 4K `sp` run; then the attended hour at 3840x2160@144 (`tools/play.sh`, now borderless fullscreen-desktop by the fit rule; no gamescope: docs/CONSTRAINTS.md 1) | 1 switch, 1 leak (O4), 1 clock, 1 attended hour | no frame-rate drift over the attended hour (the last ten minutes' percentiles within the first ten's); the headless hour allocates no growth heap, or every growth is attributed and fixed; the task clock at the exe's rate or its difference stated; ASan 0 |
| 3 | **the oracle's rung** (M3 oracle physics; the fourth recording and mode B if the user made them; M2-R10's row 3, unchanged) | 10-17 2823 / 10-18 2871 (the Worker's dock / harvest heading paths against the exe), `RangeToTarget` / `RangeToTargetGivenDist`'s unrounded returns, `cat_gunCanFire`'s second mode, `cloudUpdateSystem`, the other `frandom` sites (LevelLoad.c's first), Mission17's self-targeting KAS teams | 3 tags + ~10 sites + 2 divergences + 1 self-target | the 10-17 replay IDENTICAL beyond 2823 and the 10-18 beyond 2871, or the next divergences localised and owned; mode B's first divergence localised if run; no NaN flight in Mission17 or its cause stated as the exe's |
| 4 | **M3-R2: the fix round** (sized from row 0's report; one producer + auditor per item as this round) | **already owned by M3-R2 from this round**: the letter height if the user picks the exe's 42 rows (`NIS_LetterHeight`), the exe's additive-on state after full bars if the user prefers it (O1), the BTG star size (`CAT-TODO(M3-R2)`), the quit after a resolution change (`mainDestroyWindow`), FULL DETAIL's mesh submission at 144 Hz and the effects cap if wanted, trails at LOD 0 if the user says so, the sensors map's thin lines at 200% if the user says so, the tutorial pointer if seen; **plus every stop and difference the play-test reports** (each an item with an owner) | 8 candidates + the report's | the play-test's stops fixed and re-played; every port IDENTICAL on the Mission01 stream, the 17 missions and the rung; the sign-off |
| then | **stage 5** (decision 9) | the 23 `CAT-TODO(stage 5 ...)` sites: the LAN game's senders and keep-alive, the options word, the `/packetRecord` header, the Titan / WON stubs | 23 sites | LAN interop with stock 1.01 nice-to-have (decision 9); the wire format not broken |

**Then**: the stage-5 remainder per decision 9 after M3, or an oracle-physics round only if the user wants the rung
before the next play-test. **Rules carried into M3-R2** (from this round): every port on the plain harness with a clean
build; every stream judged with `hwo_compare.py` in all 21 fields (never by sha256 across builds); the dumps compared
whole and crc-stripped (a crc-only move is a stated render change; a non-crc move is a render-timing change and must be
stated with its frame count); GPU measurements on an idle GPU only, with the CPU load stated (this step's load average
12-20: the user's ripgrep); the user's answers in `cycle/answers/<round>-user.md`, read again before the report.

## What the user must decide

Every default is in the tree; say if you want otherwise.

1. **Producer 4's questions (motion-jitter), verbatim from its report's "THE USER'S CHECK"** (its first question, "was
   Smooth motion on?", was put to you by the launching session and answered in `cycle/answers/m3-r1-user.md`, 95e011d:
   *"I don't know where I turn that on so unless it was on by default, it was probably off."* The report treats the
   play-test as run with interpolation OFF and found two paths that wrote the 0 (the M2-R6 builds of 2026-10-18
   persisted `/noInterp`; the Smooth motion checkbox's deletion-time call, a live bug, fixed); your jitter recipe from
   335af38 is the measured scene):
   - "**Turn Smooth motion back on** (the play-home options file still says off: rows 107-108 are 0). Run `tools/play.sh
     --rebuild`, open Options -> Video in the front end, tick **SMOOTH MOTION**, press Accept. (Or set `objInterpolate 1`
     and `nisInterpolate 1` in original/play-home/.homeworld/Cataclysm.cfg before starting.) Then open and close the
     Video screen once more: the box should still be ticked."
   - "**Your recipe**: Multiplayer -> Skirmish (one computer), the Sect. Focus the Recon nearest the mothership, put the
     camera straight above it, select the adjacent Recon and order it about 500 m straight ahead. Watch the moving Recon
     along its whole path: **report whether it still jitters**, and whether anything else on screen does (ships crossing,
     a formation, a Worker docking at a Processor: the moment it latches on)."
   - "**The same with the camera moving**: focus the moving Recon instead and watch the still ones and the asteroids slide
     by."
   - "**A cutscene**: New Game -> the Mission01 opening; watch the camera pans and ships in them."
   - "**Tell us the monitor's refresh rate and whether the game ran at it** (the 4K FULL DETAIL frame is ~10 ms on this
     machine, more than a 144 Hz refresh's 6.9 ms: if the motion is smooth but uneven in a regular beat, that is frame
     pacing, not interpolation, and goes to the soak round). For an A/B, `tools/play.sh -- /noInterp` turns it off for
     that run only (the options file keeps its rows)."
   Default if unanswered: the flags on (the code default; your file's rows 107-108 still say 0 until you tick the box or
   edit them), the monitor taken as the 144 Hz 4K target of docs/features/high-refresh-4k.md, frame pacing to the soak.
2. **The letter height at 16:9 and wider** (producer 1, the audit's O4): GoK's `NIS_LetterHeight` keeps a 2.39:1 picture
   (277 bar rows at 3840x2160, 107 at 1024x768); the exe's bars are `(H - W*20/37)/2` rows, a 1.85:1 picture (42 rows at
   3840x2160, 0 at 1024x768: the exe draws no bars at 4:3). Default: GoK's (the M2 look, the bars now in front). Say
   "the exe's" for the one-line change in M3-R2.
3. **The exe's additive blending after full bars** (the audit's O1): in the exe the full-bars branch leaves additive
   blending on, so the subtitles and the cursor drawn after the bars blend additively (brighter over the scene); the port
   restores the state it found (`CAT-FIX`). Default: the port's (normal blending). Say "the exe's" to keep its look.
4. **FULL DETAIL** (producer 3): the 4K cost (~10 ms median / ~15 ms p99 at 164 ships, on an idle GPU, the same at
   1024x768: vertex submission) is acceptable as the default, or the default goes back to the exe's 0 with the checkbox
   as the opt-in; **trails at the LOD-0 style** (would move `rng_all` in the mission streams: a stated re-baseline) or
   the exe's level-following trails (default); **the effects cap** (Relic's per-effect limits; the high preset already
   256) lifted in M3-R2 or left (default); the sensors manager's blips stay blips (default).
5. **The UI scale and the window** (producer 5): 200% at 4K / 150% at 1440p as the auto rule (default; AUTO / 100 / 150
   / 200 in Options -> Video, `/uiScale N` for a run, the `uiScale` row); the 4K window as borderless fullscreen-desktop
   over the panel (the fit rule; default) or a decorated window at a smaller request; whether it opens on the 4K monitor
   and what switching to the other monitor does; the BTG star size (the exe's fixed share of the screen or GoK's fixed
   pixel size: `CAT-TODO(M3-R2)`); the sensors map's thin lines at 200%.
6. **Team colours** (producer 2): nothing to decide; the texture budget (6 MB at TextureDetail 50) is a quality question
   if FULL DETAIL leaves the textures looking soft (the exe's UnlimitedVideoMemory-style lift would be a feature).
7. **Decision 48** (the fourth recording with the DLL's `CAT_SHIP_coordsys` 0x98 edit): still none (the newest
   `original/oracle/oracle01-rec-*` is 10-19's); both open divergences point at the bWorker 44's coordsys, which only this
   recording makes comparable. Recommendation unchanged: before M3's rung producer runs.
8. **The mode-B playback** of `hwirec01` / `hwirec02` (the user's session; the commands in docs/m2-r10-summary.md "What
   the user must decide" 2 still apply: the right-hand sides are now this step's `m3r1i-play01-a` / `m3r1i-play02-a`,
   IDENTICAL to R10's `r10i-play01-a` / `r10i-play02-a` in every field).
9. **The two saves of the M0 play-test** (8: one mission, one skirmish): still open.
10. **The defaults recorded as decisions 111-121** in cycle/STATE.md (say if you want otherwise), and whether M3-R2
    starts from the play-test report or the Windows 11 / soak producers go first (recommendation: the play-test first,
    the Windows build in parallel from the main checkout as M2-R10 planned).

## The play-test brief, refreshed (replaces M2-R10's; the six items first, then R10's open items)

An attended run is the only one that may be audible or on screen (docs/CONSTRAINTS.md 5). Launch with `tools/play.sh`
(read its header: the release build, `--rebuild` once after this checkpoint so `build/release/hwi-engine` carries the five
fixes; `--debug` for the ASan build when a crash needs a report; the harness data dir; the private home
`original/play-home`, whose `.homeworld/Cataclysm.cfg` keeps your colours, `noLOD 1`, `screenWidth 3840` /
`screenHeight 2160` / `fullScreen 0`, and still says `objInterpolate 0` / `nisInterpolate 0`; engine switches after
`--`; the log under `original/play-home/logs/play-<timestamp>.log`; no gamescope):

```
cd ~/Documents/GameDev/Homeworld-Infection/.claude/worktrees/m2
tools/play.sh --rebuild                # the 4K window is now borderless fullscreen-desktop by the fit rule; -- /window etc. after --
```

Before playing: **Options -> Video, tick SMOOTH MOTION, Accept** (or set the two rows to 1 in the cfg): your 10-20
session ran with it off. Then report per item:

1. **The letterbox bars (item 1).** New Game: the opening cutscene, then the arrival cutscene when the fleet comes out of
   hyperspace in Mission01: about 46-50 s into it fighters stream past and a large orange hull swings across the top of
   the frame; at about 1:12 and 1:40 ships pass the bottom edge. Nothing of a ship or the hull may cross into the black
   bars; every ship must disappear at the bar's edge (the harness's capture of tick 789 drew 65,115 pixels of the hull in
   the top bar before, none now). Any in-mission cutscene (Mission02's jump at its end) fades the bars in from grey to
   black, smoothly, in front. The old look on request: `letterboxBehind 1` in the cfg or `tools/play.sh --
   /letterboxBehind` for one run. Report any frame where a ship still shows in a bar with the default (the mission and
   roughly when), and **whether the bars' height at 3840x2160 suits you** (277 rows each; the exe would draw 42: question 2).
2. **The team colours across the jumps (item 2).** The opening cutscene (the Kuun-Lan in your green with blue chevrons,
   as before), then the jump into Mission01: after the arrival cutscene, when the fleet comes out of hyperspace (about two
   minutes in), the Kuun-Lan must keep the same green and blue (the M2 build showed it grey-brown there), the Recons and
   Workers that launch from it too. Play on into Mission02 and, if time allows, Mission03 -> the cutscene level before
   Mission04 -> Mission04: the colours must stay on every ship of your fleet after each jump and each cutscene level
   (the P1 ion array frigates' and fighters' colours after Mission03 were R10's question: report them too). Optional:
   `tools/play.sh -- /catTeamColourTest` writes `teamcolour: check ... result=PASS` lines to the log at each level.
   Report any level where your ships show other colours than the picked ones, and the play log's name.
3. **FULL DETAIL (item 3).** Nothing to change (`noLOD 1` is in your cfg, the box ticked, the detail slider greyed). The
   Mission01 opening and its first minutes: zoom out (mouse wheel) from the Kuun-Lan and the fleet to the farthest zoom
   and back; a skirmish against one computer with the fleet spread out. Ships far from the camera and at the screen's
   edges, and the asteroids, keep their full shape (no simpler model popping in or out with the zoom). A/B with
   `tools/play.sh -- /lodBands` (this run only). Report any model still changing detail with distance in the default
   run; any glitch on a ship (missing parts, a turret or engine in the wrong place, an engine trail starting off the
   hull); **the frame rate in a large battle at 4K against `/lodBands`** (FULL DETAIL costs ~3.5x the render time in the
   harness's 164-ship scene: question 4); whether the sensors manager's blips and the trails look right.
4. **The unfocused objects' jitter (item 4).** With SMOOTH MOTION on: your recipe, Multiplayer -> Skirmish (one
   computer), the Sect: focus the Recon nearest the mothership, put the camera straight above it, select the adjacent
   Recon and order it about 500 m straight ahead; watch it along its whole path: **does it still jitter**, and does
   anything else (ships crossing, a formation, a Worker docking at a Processor at the moment it latches on)? The same with
   the camera on the moving Recon (the still ones and the asteroids slide by). A cutscene (the Mission01 opening: the
   camera pans and the ships in them). **Tell us the monitor's refresh rate and whether the game ran at it** (a regular
   beat in otherwise smooth motion is frame pacing, the soak's item, not interpolation). A/B: `tools/play.sh -- /noInterp`
   (this run only).
5. **The UI at 4K (item 5).** The main menu twice its old size (1280x960 in the middle of the screen), the text sharp
   (pixel-doubled). New Game -> Mission01: the HUD text (the ship lists top right, the info panel, the objectives, the RU /
   support box bottom right), the subtitles and the letterbox, a right-click menu, the build / research / launch screens,
   the Escape menu, the movement disc and its lines: all twice their old size and placed as at 1080p; click small things
   (menu buttons, list entries, the movement disc's height) and see that clicks land under the pointer. The control:
   Options -> Video, UI SCALE AUTO / 100% / 150% / 200%: 150% redraws the menu at once at 1.5x, 100% is the old small UI,
   back to AUTO; in a game the change applies at the next start; `tools/play.sh -- /uiScale 150` for one run. Report
   **whether 200% at 4K (and 150% at 1440p, if you try it) is the size you want**, anything 2D that stayed small or sits
   in a wrong place (which screen), any click that misses, the background stars' size (question 5), and whether the
   sensors map's lines are too thin.
6. **The window and the mouse at the edges (item 6).** The window now covers the whole 4K monitor with no title bar,
   over the panel (borderless fullscreen-desktop, because the whole display was asked for); the log shows `window:
   requested 3840x2160, usable WxH (display N: 3840x2160), created 3840x2160 (fullscreen-desktop): display size ->
   fullscreen desktop` and `uiscale: physical 3840x2160 ... scale 200% (auto, uiScale 0) logical 1920x1080`. Check it
   opens on the 4K monitor. In the main menu move the pointer to the very bottom, the bottom corners and the right edge:
   the cursor must reach the last row / column; in a game push the pointer to the bottom edge: the task bar must pop up
   and its buttons must be clickable. Report whether switching to the other monitor (Alt-Tab, the mouse) minimises the
   window or behaves badly, whether you would rather have a decorated window (then pick a smaller resolution in
   Options -> Video or the cfg's screenWidth / screenHeight: a request that fits the desktop's usable area stays a
   decorated window), and **whether Quit needs a second try after a resolution change** (a known GoK fault, owner M3-R2).
7. **R10's open items, unchanged**: **the two animatics attended** (Animatic01 full screen before Mission01's opening
   NIS, at its own pace, with its sound and music, the cursor hidden; Animatic02 at Mission01's hyperspace white-out; the
   log's `animatic: play a=0 b=1 file=Movies/Animatic01.bik` / `a=1 b=2`, **no `animatic: skipped` line**; whether Escape
   skips one); **the campaign from New Game as far as it goes** (Mission01's six objectives and the KAS saves loading back,
   Mission02 .. 04 with the roll-call screens, and on: Mission06's nebula tendrils, Mission09's Emergency Hyperspace,
   Mission17's Taiidan fighters at the Nomad Moon and the credits; per mission what differed from the GOG game, how long
   the fights take, where it stopped with the log's last lines); **the tutorial's 12 lessons** (Next through each, a
   Training save loading back); **a skirmish** against one computer for 10 minutes on a listed map, once with CPU Attacks
   "Humans only" / "Humans and allies", the managers' screens, **a recording** through the Record shortcut and its
   playback through Load Recorded Game.
8. **For every item**: any crash or hang with the terminal's last lines (`hwi-engine: fatal signal N`, the ASan report of
   a `--debug` run), the frame rate's drift over the session (the one-hour 4K@144 soak is M3's own item: if a session runs
   an hour at 4K without gamescope, note the frame rate at the start and the end), and a screenshot of anything that
   looks wrong (the engine's own screenshots land in `original/play-home`'s ScreenShots).

The known gaps to skip, not report: combat balance is untested by the self-tests (item 7 measures it); a failed
hyperspace jump happens only from the developer console; the GOG game's ships can differ from ours in the last bits of
their velocities after about three minutes of a skirmish (the rung's 2823 / 2871: invisible in play); the stage-5 LAN game
and the Titan / WON screens are not ported; a lesson or campaign save written by this build is in the tree's native
format (only a recording's save is in the exe's format, decision 95), so it does not load in the GOG game; the window at
4K is borderless by design (question 5).

## Decisions

**Applied this round:**
- The faithfulness defaults and decisions 17-110 of cycle/STATE.md; decision 70 (the table square root) under every
  run; decision 9 as "recorder" (stage 5's 23 sites untouched); decision 46's guard rails (the dumps compared whole and
  crc-stripped, every move stated: 118); decision 47's exclusions and decision 60's `real` in every replay comparison;
  decision 48 open (no fourth recording; `ships_rot` excluded); decision 55 followed by every producer and audit (49
  review notes); decision 57 stands (the headless Mission01 stream did not move); decision 75 untouched (the generators
  `--check` current); decisions 76 / 77 / 86 / 109 stand for every mission (17 / 17 IDENTICAL to R10's); decision 84 (the
  ai soak's `rng_all` exempt); decision 92 extended (the harness: 117); decision 101 extended (the texture registry's
  pending-only refresh: 112); the user's rules from the play-test (the letterbox old look opt-in default off, FULL
  DETAIL ON by default, the UI scale 2.0 at >= 2160 / 1.5 at >= 1440 / 1.0 below with overrides, motion interpolation on
  by default with its toggle, no gamescope, never fake input).
- **Decision 44 unchanged** (119): the headless Mission01 stream of this step's build (`1ae66610c3f02410`, release
  `234bcccfb948ab98`) is IDENTICAL in every field over 6,001 ticks to R10's `5bd51234405ea0d8` (only the header's build
  identity differs), as every producer's and audit's Mission01 run of the round (`787e98865eb42c38`, `2a992dcf…`: the
  same content); **the rung unchanged**: 2823 / 2871 (177 / 118 with `/oracleLibmSqrt`).
- **Decision 46 updated** (118): the guard-rail dumps re-baselined with the stated reasons; the opt-outs together equal
  R10's.

**Still open (the user's; none blocks the play-test):**
- **8.** Two saves from the M0 play-test (one mission, one skirmish).
- **The play-test sign-off** (docs/PLAN.md); the refreshed brief above.
- **Decision 48**: a fourth recording with the fixed oracle DLL.
- **The mode-B playback** of `hwirec01` / `hwirec02` ("What the user must decide" 8).
- **This round's questions** ("What the user must decide" 1-6): Smooth motion's refresh-rate answer and the jitter
  verdict, the letter height, the exe's additive state after full bars, FULL DETAIL's cost / trails / effects cap, the UI
  size and the window, the BTG stars.

**Surfaced by M3-R1.** Each has a default in the tree; say if you want otherwise (numbered 111-121 in cycle/STATE.md):
111 the letterbox cause, fix and `letterboxBehind` setting (the letter height stays GoK's; the exe's additive state after
full bars is the user's call); 112 the team colours' cause and the exe's pending-only registry refresh (`CAT`, no
setting; O4's heap counts for the soak); 113 FULL DETAIL = the exe's checkbox with LOD 0 drawn, default ON, the non-draw
readers on the exe's level, the effects cap not lifted, the 4K cost owned by M3-R2; 114 the play-test ran with Smooth
motion OFF (two write paths, the live one fixed), the three real-clock residuals fixed, the measurement tools; 115 the
UI scale rule, its overrides, rounding and filtering, the three sizes; 116 the 4K window fit (`HWI-FIX`), the mouse
clamp, the quit fault and the `DEADLYSIGNAL` owned; 117 the harness (30 modes, the flags, switches and tools); 118 the
dumps re-baselined; 119 decision 44 and the rung unchanged, presentation only held; 120 the tags and the delta map; 121
the exit-test rules carried into M3-R2.
