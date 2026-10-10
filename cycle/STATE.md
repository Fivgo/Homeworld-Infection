# Work cycle and resume point

Last checkpoint: 2026-10-10 14:51

**Read this first when resuming.** It is the single source of truth for where the work stands.

## The cycle (user design, 2026-10-08)

1. **Tokens available:** Claude resumes from this file, runs Opus agents/workflows, and calls `tools/checkpoint.sh "<msg>"` after every completed stage.
2. **Limit reached:** Opus work stops at the five-hour rate limit. Nothing is lost: finished workflow agents are cached (resume with `resumeFromRunId`), and finished stages are committed.
3. **During the wait:** the systemd user timer `hwi-qwen-worker.timer` (every ~2 min) sees the genuine rate-limit event and runs `tools/qwen_worker.py run --auto`. Local Qwen 3.6 27B names unnamed functions into `ghidra/proposals/qwen/naming.jsonl` (append-only, fsynced per function, resumable). It yields to any running game and stops on its own at the reset time.
4. **Reset:** Claude is re-prompted (one-shot CronCreate a few minutes past `resetsAt`, the user's standing rule) and goes back to step 1, folding Qwen's output in through an Opus audit.

### Why there is no "stop at 90%" trigger
Claude Code records no "approaching limit" signal: across a week of transcripts every `quotaLimits` record is `status: rejected` (a hard limit), carrying `rateLimitType: five_hour` and an exact `resetsAt` epoch. So the cycle makes hitting 100% harmless (cached agents, per-stage commits, resumable Qwen queue) instead of guessing at 90%. If a usage-percentage signal ever appears, `claude_limit_state()` in `tools/qwen_worker.py` is the one place to extend.

## Local worker

| | |
|---|---|
| model | `qwen3.6:27b` via Ollama 0.40.1 (system service, flash attention + q8 KV), ctx 8192, ~2 s/function warm, ~28 GB VRAM incl. desktop while loaded, unloads after 5 min idle |
| queue | 8,310 unnamed, decompiled functions of `cataclysm.exe`, most-nameable first |
| output | `ghidra/proposals/qwen/naming.jsonl` → `tools/qwen_worker.py export` → `ghidra/proposals/qwen-naming.json` (binary-map contract) |
| trust | **Unaudited.** Calibration rules: echoing a weak HW1 match caps at medium; a name given to two addresses is marked low with a `[dup]` note. Opus samples it before anything reaches Ghidra |
| control | `tools/qwen_worker.py status` · `run --force --max-items N` (manual) · `systemctl --user stop hwi-qwen-worker.timer` (pause) |
| log | `ghidra/proposals/qwen/worker.log`, `journalctl --user -u hwi-qwen-worker` |

<!-- qwen-status -->
```
    claude limited : False (reset 13:20, seen 12:49)
    gpu gate       : BLOCKED (a game is running)
    model          : qwen3.6:27b ctx=8192
    progress       : 12/0 functions proposed -> /home/evanmedinger/Documents/GameDev/Homeworld-Infection/.claude/worktrees/m2/ghidra/proposals/qwen/naming.jsonl
```
<!-- /qwen-status -->


## Orchestrator: ACTIVE since 2026-10-09 12:29 (user chose option B)

A background CLI session `homeworld-orchestrator` (claude --bg, Opus, auto permissions, brief in cycle/ORCHESTRATOR.md) now owns relaunching and chaining the workflows. The IDE session must NOT launch workflows while it is active. Watch: `claude agents`, `claude logs homeworld-orchestrator`; stop: `claude stop homeworld-orchestrator`.

### Where the work lives now: branch `m2` (2026-10-10)

Rounds 2-5 are on main (the user fast-forwarded to eae37c9). M2 works on branch `m2` in the worktree `.claude/worktrees/m2`, made from main at eae37c9, because the orchestrator's subagents cannot write to the main checkout and a background session does not merge. The Ghidra exports, `original/` and `reference/` stay in the main checkout; scratch data derived from the game goes to `original/scratch-m2/` (gitignored). Land M2 on main with `git merge --ff-only m2` when you like. The old `r2-apply-coverage` worktree is merged and can be removed: `git worktree remove .claude/worktrees/r2-apply`.

### Decisions the user owns
1. M2-R1 runs 11 agents (5 producers, one per M2_BRIEF R1 item, 5 auditors, 1 integrate), close to the brief's "about 10". The brief's "one producer per format" became one formats producer that writes each format doc as it goes, to keep the round near 10 agents. Say if you want the formats split.
2. The bootstrap producer is told to import the ad1bb82 tree plus only the GoK portability fixes it needs; if that grows into hundreds of hand-picked hunks it falls back to importing GoK's whole `src/` and records it here as your decision. **Outcome (M2-R1):** the fallback was taken; see "## M2", decision 1.
3. Still open from docs/PLAN.md: the M0 play-test sign-off; whether LAN interop with stock 1.01 clients is required.
4. If `/tmp/claude-r3a` still exists (a pre-round-3 copy of the Ghidra project, with the game's program bytes): `rm -rf /tmp/claude-r3a`.

### (history) PAUSED 2026-10-09 12:05

Nothing is running. The in-session round-2 workflow was stopped on purpose (to hand off to a background orchestrator), the auto-mode classifier then refused to start that background session AND refused to relaunch the in-session workflow ("Create Unsafe Agents"). The IDE session also tears down background work after 15-35 idle minutes, which is what kept killing runs. Two ways forward (user's choice):

A. Allow the Workflow tool in Claude Code settings (permission rule for Workflow) and keep using in-session workflows plus a 10-minute relaunch cron.
B. Run the orchestrator yourself, once, from a terminal (lives in the CLI, survives IDE restarts):
   cd ~/Documents/GameDev/Homeworld-Infection && claude --bg --name homeworld-orchestrator --model opus --permission-mode auto --effort high --add-dir "$PWD" "$(cat cycle/ORCHESTRATOR.md)"
   Watch: claude agents   |   claude logs homeworld-orchestrator   |   stop: claude stop homeworld-orchestrator
   If B is chosen, no in-session crons must launch anything (set "orchestrator: active" here).

Round-2 state on disk: A1 complete (r2a export, 13,440 functions); A2 (data names + struct types), producers, audits, reconcile, apply not started. Relaunch = Workflow resumeFromRunId wf_2d9d668a-e53.

## M2

### M2-R1: COMPLETE 2026-10-10 (the delta inventory, measurement only)

Summary and M2-R2 plan: [docs/m2-r1-summary.md](../docs/m2-r1-summary.md). All five items exist and are audited. Every verdict is PASS-WITH-FIXES, and each fix is marked inline with `[audit: ...]`. `src/` builds.

| item | output | key numbers | audit |
|---|---|---|---|
| 1 data deltas | [docs/m2-r1-data-deltas.md](../docs/m2-r1-data-deltas.md) | 34 headless/closed runs. The unmodified HW1 engine stops 17 times before its first frame; 15 were cleared in scratch, 1 with `/noSpeech`, and 1 is a hard blocker: mesh v0x500 (116-byte polygon objects in all 1,727 v0x500 files). With the meshes down-converted it reaches `Main_game_screen`. ASan found 2 out-of-bounds writes from ship names. 145 vs 59 ship types, no id kept. 226 of 590 HW1 string ids missing | 0.936, PASS-WITH-FIXES |
| 2 formats | [docs/formats/](../docs/formats/) (mesh, textures, script, shipstatic, savegame, fib) + `tools/formats/*_probe.py` | polygonobject 0x70→0x74 (1,729 files probe OK); texreg entry 0x2C→0x34; 1,994 new script keys (652 functional); ShipStaticInfo 0x370→0x458, CustShipHeader 34 slots; save layout from code only (no saves exist yet); FIB unchanged | 0.889, PASS-WITH-FIXES |
| 3 function deltas | [docs/m2-r1-function-deltas.md](../docs/m2-r1-function-deltas.md), `ghidra/delta_map.json` | 4,420 of 4,426 classified: IDENTICAL 3,357 (53.1% of bytes, about 58% after review correction), MODIFIED 983 (46.0%), REWRITTEN 80 (0.8%) | random sample 31/40 = 0.775, PASS-WITH-FIXES (treat classes as estimates) |
| 4 bootstrap | [docs/m2-r1-bootstrap.md](../docs/m2-r1-bootstrap.md), `src/`, `meson.build`, `tools/build.sh` | GoK `3af31fb` whole `src/` (603 files) + Relic `Win32/` as reference; `hwi-engine` release 5,157,976 B, debug 74,870,832 B, 0 warnings under `-Werror`. Integrate re-ran `tools/build.sh --release --clean`: rc 0, same sha256 `0024b0f6…53b297` | 0.935, PASS-WITH-FIXES |
| 5 checksum oracle | [docs/m2-checksum-oracle.md](../docs/m2-checksum-oracle.md) | design only. 23 Cataclysm functions located; checksum covers ships only; 16-byte sync entry (HW1 12); 27 RNG streams; 7 hook sites; driven by the game's own recorded-game playback, no input | 0.901, PASS-WITH-FIXES |

### M2-R2 order, with delta counts

HW1-matched counts come from `delta_map.json`, by HW1 file. cat-new counts are estimates: the kas2c mission region is assigned exactly, the rest by name prefix or by nearest neighbour in address order (method in the summary).

| stage | IDENTICAL fn / B | MODIFIED fn / B | REWRITTEN fn / B | cat-new fn / B (est.) |
|---|---:|---:|---:|---:|
| 1 file formats (mesh 0x500/.peo first) + data load, now including the 145-type/7-race/12-class enums | 236 / 50,910 | 79 / 44,334 | 0 / 0 | 76 / 27,383 |
| 2 universe and ships (incl. ETG/particle) | 971 / 195,981 | 401 / 205,267 | 30 / 4,501 | 829 / 187,242 |
| 3 UI and presentation (FE, render, sound, NIS) | 958 / 222,114 | 233 / 168,373 | 7 / 2,355 | 429 / 132,822 |
| 4 AI | 404 / 80,807 | 72 / 42,717 | 3 / 348 | 23 / 14,404 |
| 5 LAN networking | 411 / 72,656 | 108 / 72,557 | 13 / 1,677 | 67 / 17,483 |
| 6 missions (2,085 kas2c functions, 612,687 B, among the cat-new) | 377 / 35,921 | 90 / 36,705 | 27 / 1,624 | 2,365 / 656,918 |

How R1 changed the order:
- The identifier enums move into stage 1, because they block the first frame and cause memory-corrupting writes.
- Systemic changes are ported once, in headers, at the start of the stage that owns them: ETG argument arrays, the AI move macro, the KAS hash keys.
- The oracle's recreation side is built alongside stage 1.
- The save layout waits for real saves.

Stage-1 gate: debug `hwi-engine` reaches `Main_game_screen` on unmodified Cataclysm data with no scratch workarounds, and gets past the `RACE_*` level-load stop.

(The M2-R2 order above was R1's; the table is kept as history. Current per-stage counts after the re-read: docs/m2-r1-summary.md "Delta counts per M2-R2 stage" and docs/m2-r2-summary.md.)

### M2-R2: COMPLETE 2026-10-10 (stage 1: formats + data load)

Summary, exit test and the M2-R3/R4 plan: [docs/m2-r2-summary.md](../docs/m2-r2-summary.md). Workflow `cycle/workflows/m2-r2.js`, run `wf_3583f3e2-1ab`, 13 agents. Each port was checkpointed by its auditor.

**Stage-1 exit test: MET** (integrate, clean build at 5687473). Debug `hwi-engine` (ASan) on the plain harness (`original/scratch-m2/m2r2-harness/data`: the GOG install's own files, no workarounds, no mesh down-conversion, no `/noSpeech`): fe reaches `Main_game_screen` (run `integrate-exit-fe`, ASan 0); sp gets past the run27 `RACE_*` stop (Mission01 parsed, `RACE_SECT/sRecon.shp` loaded) and stops at `Ships/GenericInterceptor.c (205)`, the stage-2 slot body (run `integrate-exit-sp`, ASan 0). Self-tests on the plain harness: ship statics 143/143 types as the probe (170/173 `.shp` load), meshes 1,758/1,758, textures 12,403 listed / 10,661 loaded / 0 failed, oracle `SELFTEST PASS`. `tools/build.sh --debug`/`--release` `--clean`: 0 warnings under `-Werror`. Every quit still ends with GoK's `munmap(heap, 0)` assert (`utility.c (3484)`), a stage-1 close-out item.

| item | report | result | audit | commit |
|---|---|---|---|---|
| identifiers + tweak tables | [docs/m2-r2-identifiers.md](../docs/m2-r2-identifiers.md) | 145 types / 7 races / 12 classes with the exe's ids; 16 tables re-keyed from the exe, 7 provisional; ranks 2, 3, 5, 7 and both ASan writes gone | 0.905, PASS-WITH-FIXES | b4e0f23 |
| ship statics | [docs/m2-r2-shipstatics.md](../docs/m2-r2-shipstatics.md) | `ShipStaticInfo` 0x458, `GunStatic` 0x11C, `CustShipHeader` 0x8C/34 slots (layout), 178 + 39 keys, 233 static asserts; 143/143 types | 0.953, PASS-WITH-FIXES | da09c2f |
| mesh 0x500 | [docs/m2-r2-mesh.md](../docs/m2-r2-mesh.md) | 0x74 polygon objects, per-vertex colour path; 1,758/1,758 files, 0 ASan; 21 engine glows | 0.956, PASS-WITH-FIXES | fe8645d |
| start-up names, audio, FE art, textures | [docs/m2-r2-startup.md](../docs/m2-r2-startup.md) | ranks 4, 6, 8, 9, 10 cleared without `/noSpeech`; texreg 0x34, `textures.ll` 12,403; GoK archive age-array defect fixed | 0.898, PASS-WITH-FIXES | 7de1b18 |
| oracle, recreation side | [docs/m2-r2-oracle.md](../docs/m2-r2-oracle.md) | `.hwo` writer, compare tool, scripted-start self-test byte-identical; 16 Hz default, FP flags pinned, release `-O2`; GOG DLL builds with clang-18 + lld-link (never loaded) | 0.96, PASS-WITH-FIXES | 5687473 |
| delta-map re-read | [docs/m2-r2-reread.md](../docs/m2-r2-reread.md) | 1,013 read: 870 / 140 / 3; 143 changed class; totals now IDENTICAL 3,175 / MODIFIED 1,168 / REWRITTEN 77 (51.1 / 48.1 / 0.8% of bytes) | 1.00, PASS | ddc8a18 |

Stage-1 leftovers (carried into M2-R3 producer 1, none blocks the exit test): the 8-type derelict table, GoK's quit assert, the save container and chunk sizes, 14 delta-map reclassifications found by the ports, the 1,705-entry string-id enum.

### M2-R3 plan (stage-1 close-out + stage 2 part A; M2-R4 = stage 2 part B)

5 producers in sequence + 5 auditors + integrate (11 agents). Counts: HW1-matched IDENTICAL / MODIFIED / REWRITTEN functions (MODIFIED bytes) and cat-new by address neighbour, from `ghidra/delta_map.json` with `tools/m2_stage_counts.py`'s rules. Stage 2 in all: 860 / 512 / 30 (216,998 B MODIFIED) + 829 cat-new (187,242 B). Details and the M2-R4 table: docs/m2-r2-summary.md.

| # | producer | scope | I / M / R (M bytes) | cat-new | done when |
|---|---|---|---|---|---|
| 1 | close-out + systemic headers | stage-1 leftovers; `SOF_*` renumbering, `MAX_MILITARY_SLOTS` 63, `AlternativeShips` 0xA8, `ClampInfo` 0x38, `StaticCollInfo`, `CustShipFire` +0x11C; `Randy.c` 27 streams + `realtimeelapsed` | 8 / 0 / 0 | 15 / 8,705 B | quit rc 0; size asserts; oracle self-test PASS with 27 streams |
| 2 | ETG + particle argument arrays | one-pointer callables, `etg\race_*.script`, ETG.c, Particle.c | 171 / 164 / 2 (21,936 B) | 24 / 6,685 B | all `etg\*.script` parse, ASan 0 |
| 3 | selection | `ioUpdateShipTotals` in 30 filters, Select.c, ShipSelect.c | 33 / 44 / 4 (14,536 B) | 59 / 11,278 B | no HW1 alias left in both files |
| 4 | ship code A (HW1 headers) | `CustShipHeader` slot wiring; StaticInit/slot bodies of the reused HW1 headers, `GenericInterceptorStaticInit` 0x005b1e70 first | Ships/* 80 / 45 / 12 (15,070 B) | part of 422 / 78,155 B | sp passes `GenericInterceptor.c (205)` |
| 5 | universe statics + MEX | Universe.c (mine statics x7 races, team colours, derelict loader), MEX.c, `DockStaticInfo` 0x50, Globals.c | 38 / 31 / 0 (15,684 B) | 20 / 5,549 B | sp reaches `universe running` on Mission01 |

M2-R4 (stage 2 part B): UnivUpdate.c + glow runtime; CommandLayer/CommandWrap/Formation; combat + docking + salvage; ship code B (the 22 Cataclysm-only headers, rest of Ships/* cat-new); the rest + oracle stage 2. Stage-2 exit: Mission01 runs headless with ASan 0 and two runs give byte-identical `.hwo` streams.

### Decisions the user owns

**Answered by the user 2026-10-10** (R1 list numbering; the user's own numbering in brackets where it differs), all applied in M2-R2:
1. ANSWERED: accept GoK 3af31fb as `src/`.
2. ANSWERED: Cataclysm behaviour wins, decided per subsystem as it is ported; 16 Hz universe is the default (GoK's 64 Hz only behind `/gok64Hz`); `-O2 -fno-strict-aliasing`, no fast-math, pinned before any checksum comparison (done in M2-R2 oracle).
3. ANSWERED: Relic ad1bb82 `Game/` and `Ships/` in-tree as reference (`src/reference/hw1-ad1bb82/`, commit 86c3ee6).
4. ANSWERED: re-read the 1,013 low-confidence IDENTICALs and apply the audit's reclassifications (done: docs/m2-r2-reread.md).
5. ANSWERED: the oracle may call the game's own record/playback functions; no OS input is synthesised (built into the DLL).
7. ANSWERED [their 6]: clang-18 + lld-link first, mingw fallback. **Outcome: the DLL builds with clang-18 + lld-link; no mingw install is needed.**
9. ANSWERED [their 7]: LAN interop with stock 1.01 is nice-to-have, not required: no effort spent, wire format not broken gratuitously.

**Still open:**
6. **First reference recording.** Which skirmish map and player setup you record through the menus as the first oracle reference.
8. **Two saves** from the M0 play-test (one mission, one skirmish) for `tools/formats/savegame_probe.py`; without them the save layout stays code-derived.
- The M0 play-test sign-off (docs/PLAN.md).

**New from M2-R2:**
10. **The first live oracle session is yours** (goes with 6): copy `build/oracle-dll/hwi_oracle.dll` and `hwi_oracle_loader.exe` next to `cataclysm.exe`, record one skirmish, then two playback runs; procedure in docs/m2-r2-oracle.md "What is left" 1. Until then it is unverified that the DLL loads under Proton, that the H6 trigger works and that `vqpc` gives 1/16 s per tick.
11. **Faithfulness defaults already in the tree** (say if you want otherwise): the 22 dropped HW1 types stay dormant (ids 200-221) until stages 2/4 delete their code; `kCloakGenerator`/`tCloakGenerator` use `DefaultShipHeader` where the exe has NULL; `crateInit` resets 46 entries as the exe does; the mission-17 texture cut follows the stock exe, not CET; the fq audio functions will follow the binary, not GoK's reconstruction.
12. **One small Ghidra pass** to apply the 3 name fixes in `ghidra/proposals/m2-r2-name-fixes.json` (M2-R2 ran no Ghidra).

## Opus workflows in flight

**Observed 2026-10-09:** the session restarts roughly hourly (00:53, 03:53 restarts logged), which kills any agent that has not written its file yet. The sim-subsystems producer ran 36 min and was lost twice. Rule for every long producer: write its output file incrementally per subsystem (the binary-map script now does this and splits sim into three producers).

Scripts are copied in `cycle/workflows/` so a new session can relaunch them. Resume a run (same session) with `Workflow({scriptPath, resumeFromRunId})`; in a fresh session, relaunch the script and the finished stages' files on disk are reused.

| workflow | run id | state at last checkpoint | next |
|---|---|---|---|
| dissect-public | `wf_05709792-47f` | COMPLETE 2026-10-08 20:12: 7 docs, all 7 verified (two docs carry two verification rounds). The planner agent stopped on a denied file read, so PLAN.md and README.md were written by the lead instead. | nothing; do not resume |
| binary-map-r1 | `wf_0c18f70b-e23` | COMPLETE 2026-10-09 08:40 (commit 139d7ac): 8,312 symbols applied, 0 failures; real names on 3,867 functions = 46.4% of function bytes; game region 55.3% named (was 2.8%); docs/binary-map.md | round 2 (binary-map-r2.js): create the 2,085 mission functions + ship slot targets, re-admit 925 data names, import struct headers, fill the ranked gaps |
| binary-map-r2 | `wf_b34f7e52-1a1` (orchestrator session; earlier `wf_2d9d668a-e53`) | **COMPLETE 2026-10-09 14:03** (apply rc=0, 962/962 symbols, 0 failures; export `ghidra/exports/cataclysm_gog_r2`). 14,384 functions (A1 created 3,190); real names on 7,055 = 59.7% of function bytes (round 1: 3,867 / 46.4%); game region 65.7% real-named (55.3%); all 2,085 mission functions named, but their Ghidra bodies cover only 50.9% of 0x5d6fe0-0x670a0f; 925 + 51 data labels; 1,735 struct types, 817 globals typed (A2); HW1 matches 3,053 in 180 files. docs/binary-map.md "Round 2"; coverage script `tools/binary_map_coverage.py` | round 3 (docs/binary-map.md R2.6): fix the 984 truncated mission bodies (`missions-r2.body-fix.csv`), name the 548 A1-created `FUN_*` (pointer-table targets; 111 in WON/Magick need a class), create + apply the 105 deferred names, the 2,081 game-region `FUN_*` clusters (skirmish AI, clouds/nebulae, main-view renderer, sensors manager, Leech/RepairDroid ship code), a prototype pass using the A2 types, audit qwen-naming.json |
| binary-map-r3 | `wf_3abebc51-f81` (orchestrator session) | **COMPLETE 2026-10-09 15:17** (apply rc=0, wall 63 s; 3,087/3,087 symbols, 0 name failures; prototypes 2,319 applied + 31 degraded, 77 `__thiscall` TitanInterface prototypes unparsed; export `ghidra/exports/cataclysm_gog_r3`). B1 (export `cataclysm_gog_r3a`): 984/984 mission bodies re-grown, 105/105 deferred names created; mission bodies cover 97.4% of 0x5d6fe0-0x670a0f (was 50.9%). 14,490 functions; real names on 8,235 = 73.9% of function bytes (round 2: 7,055 / 59.7%); game region 83.5% real-named (65.7%), 1,064 `FUN_*` left there (261,804 bytes); 1,075 new function names + 5 data, 0 renames, 2,007 prototype-only entries; 485 of the 548 A1-created `FUN_*` named; HW1 matches 3,946 in 190 files. docs/binary-map.md "Round 3"; coverage `tools/binary_map_coverage.py` (now with `--history/--label` options) | round 4 (docs/binary-map.md R3.6): create + apply the 186 deferred names (NIS.c 59, MultiplayerGame.c 38, Options.c 24), the 1,064 game-region `FUN_*` clusters (popup menu 0x004exxxx, TitanInterface C++ 0x0058-0x005a, renderer/video options, AI build/defense, mouse UI, ship code), fix the 77 `__thiscall` prototypes and give the 2,085 mission functions a prototype, class-only names for the 54 WON/Magick + 183 class-tagged `FUN_*`, low-confidence re-sample, audit qwen-naming.json |
| binary-map-r4 | `wf_f7f48caa-888` (orchestrator session) | **COMPLETE 2026-10-09 18:39** (apply rc=0, wall 58 s; 3,526/3,526 symbols, 0 name failures; prototypes 3,295 applied, 0 degraded, 0 unparsed (the 77 TitanInterface `__thiscall` fixes applied as `__fastcall`); 1 rename (0x00419E80 `aiuGenerateRandomVector` -> `cat_MoveReachedDestinationRadius`, stale plate comment removed by the new post-script `DropStaleComments.py`), 0 removals; export `ghidra/exports/cataclysm_gog_r4`). C1 (export `cataclysm_gog_r4a`): 186/186 deferred round-3 functions created and named, 46 mission switch tables typed, 44 off-cut holes repaired, 4/4 CALL refs restored. 14,679 functions; real names on 9,049 = 79.5% of function bytes (round 3: 8,235 / 73.9%); not-`FUN_*` 96.7%; game region 93.1% real-named (83.5%), 458 `FUN_*` left there (99,908 bytes); 663 new names + 2,862 prototype-only entries; all 2,085 mission functions have a prototype; HW1 matches 4,245 in 192 files; 54 names deferred to round 5. docs/binary-map.md "Round 4"; coverage `tools/binary_map_coverage.py` (class-only tier now includes the `hw1_<File>_<addr>`/`cat_new_<addr>` placeholders) | round 5 (docs/binary-map.md R4.6): the 458 game-region `FUN_*` (untouched bands 0x0050xxxx and 0x0053xxxx; selection/special-ship UI 0x00507-0x00508, fog of war 0x0047f-0x00480, region screens/game stats/key bindings, rendering, colour, combat/ship code, `Matrox` 0x004c3490), create + apply the 54 deferred names (Options.c 10, UIControls.c 9, AIHandler.c 5, AIPlayer.c 5), prototypes for the 1,821 real names with none (+107 audited signatures for C1's functions), re-sample the 180 round-4 low names, audit qwen-naming.json |
| binary-map-r5 | `wf_e06796c8-094` (orchestrator session) | **COMPLETE 2026-10-09 19:48** (apply rc=0, wall 52 s; 1,106/1,106 symbols, 0 apply failures; prototypes 831 applied, 0 degraded, 0 unparsed; 25 renames (8 real names shown wrong + 17 placeholders) with stale plate lines dropped; 2/2 off-cuts merged into their parents by the new post-script `MergeOffcuts.py`; the 15 reconcile-deferred names created in the same pass; export `ghidra/exports/cataclysm_gog_r5`). D1 (export `cataclysm_gog_r5a`): 54/54 deferred round-4 functions created and named, 42/42 `.data` artefacts removed. 14,705 functions; real names on 9,590 = 83.0% of function bytes (round 4: 9,049 / 79.5%); **0 `FUN_*` left anywhere**; game region 99.1% real-named by function bytes (93.1%), 91.1% of all region bytes; 457 new names (high 140 / medium 124 / low 193) + 597 prototype-only entries; prototypes 7,309 full / 15 degraded / 0 unparsed; HW1 matches 4,426 in 196 files, 28 zero-match files. docs/binary-map.md "Round 5" (R5.6 gaps, R5.7 verdict); new `tools/uncovered_code_candidates.py` -> `ghidra/proposals/r5/uncovered_code_candidates.json` | **naming saturated** (R5.7): no binary-map naming round 6. Optional M1 close-out, small: one guarded Ghidra pass creating functions at the filtered 488 pointer-only code candidates (~70 KB, R5.5) + the 15 deferred prototypes, at most one producer/auditor to name them (~1M tokens est.), a reasons table for the 28 zero-match HW1 files. Then M2 per docs/PLAN.md (user decisions first) |
| m2-r1 | `wf_cb36c8f7-ad7` (orchestrator session) | **COMPLETE 2026-10-10** (commit 13723dd on branch `m2`; 11 agents, ~3.2M subagent tokens): all five items written and audited (PASS-WITH-FIXES; precision 0.936 / 0.889 / 0.775 / 0.935 / 0.901); `hwi-engine` builds (release 5,157,976 B, 0 warnings); hw1-matched functions IDENTICAL 3,357 / MODIFIED 983 / REWRITTEN 80. Details in the "## M2" section and docs/m2-r1-summary.md | M2-R2 launched 2026-10-10 after the user's answers (was: on hold for the user's decision 1) in the "## M2" decisions (the `src/` base is GoK's whole tree, not ad1bb82 as Option A said; every R2 round edits `src/`, so the base must be settled first). Decision 2 (GoK's 64 Hz / SSE behaviour changes) should be answered with it. Once answered, the orchestrator writes cycle/workflows/m2-r2.js (5 producers + 5 auditors + integrate, plan in docs/m2-r1-summary.md) and launches it |
| m2-r2 | `wf_3583f3e2-1ab` (orchestrator session) | **COMPLETE 2026-10-10** (integrate commit 53d4fcf; stage-1 exit test met: debug ASan engine reaches Main_game_screen on unmodified data, sp passes the RACE_* stop and stops at the stage-2 GenericInterceptor.c (205) assert; ASan 0; 13 agents, ~4.7M subagent tokens; next = M2-R3 per the "## M2" section, script not yet written). Was: launched 2026-10-10 from cycle/workflows/m2-r2.js on branch `m2`: stage 1 (formats + data load). Five src/ ports IN SEQUENCE (they share src/ and the build): identifiers -> shipstatics -> mesh -> startup -> oracle, each followed by its auditor, which checkpoints the port; in parallel the delta-map re-read of the 1,013 low-confidence IDENTICAL functions + its auditor; then integrate (stage-1 exit test: debug hwi-engine reaches Main_game_screen on unmodified data and passes the RACE_* stop). 13 agents | if cut off: resumeFromRunId wf_3583f3e2-1ab from the orchestrator session, or relaunch the script (each port resumes from its report doc + the committed earlier ports) |
| m2-r3 | `wf_914794bc-f5e` (orchestrator session) | launched 2026-10-10 from cycle/workflows/m2-r3.js on branch `m2`: stage 2 part A (docs/m2-r2-summary.md). Five src/ ports IN SEQUENCE: closeout-headers -> etg-particles -> selection -> ships-a -> universe-mex, each followed by its auditor, which checkpoints the port; in parallel one Ghidra stage applies the 3 audited name fixes (export `cataclysm_gog_r6`); then integrate (R3 exit test: sp reaches universe running on Mission01 with ASan 0). 12 agents. User 2026-10-10: faithfulness defaults accepted; the oracle recording and two saves come later and do not block | if cut off: resumeFromRunId wf_914794bc-f5e from the orchestrator session, or relaunch the script (ports resume from their report docs + committed earlier ports; the Ghidra stage from its export) |

## Next steps after the workflows land
1. Opus audit of `ghidra/proposals/qwen-naming.json` (30-sample precision) → `qwen-naming.verified.json`, so the next reconcile includes it.
2. Binary-map rounds 1-5 are COMPLETE (rows above). Naming is saturated (docs/binary-map.md R5.7), and the remaining gaps are in R5.6. Qwen's output is still only 12 proposals. With no `FUN_*` left, its queue is empty against `cataclysm_gog_r5`, so step 1 reduces to comparing those 12 with the applied names. Round-2 A2 work (ImportHeaders.py, prep_ghidra_headers.py, `--post-script` in ghidra_headless.sh, docs/ghidra-round2-prep.md) was merged from the A2 worktree in the round-2 checkpoint.
3. Keep the play-test track separate: `tools/run_game.sh --no-gamescope --virtual-desktop 1920x1080` (gamescope is test-only, docs/CONSTRAINTS.md).
