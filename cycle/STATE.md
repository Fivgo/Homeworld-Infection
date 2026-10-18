# Work cycle and resume point

Last checkpoint: 2026-10-17 22:56

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
    claude limited : False (reset 19:10, seen 19:02)
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

### M2-R3: COMPLETE 2026-10-10 (stage 2 part A: stage-1 close-out, systemic headers, effects, selection, ship code A, universe statics)

Summary, exit test and the M2-R4 plan: [docs/m2-r3-summary.md](../docs/m2-r3-summary.md). Workflow
`cycle/workflows/m2-r3.js`, run `wf_914794bc-f5e`, 12 agents. Each port was checkpointed by its auditor. The Ghidra
stage applied the 3 name fixes. The R3 plan as launched (5 producers, scopes, done-whens) is in docs/m2-r2-summary.md
"Plan for M2-R3 and M2-R4"; it replaced the "M2-R3 plan" table that stood here.

**R3 exit test: MET** (this integrate step, clean builds at e068e39). Debug `hwi-engine` (ASan) on the plain harness:
- **sp:** passes the Mission01 level load and reaches **`universe running` (`univUpdateCounter > 0`)**, 1,248 ticks
  in 340 s, ASan 0, no unsuppressed leak (run `r3-integrate-sp`). The intro animatic plays in real time first, about
  246 s, so sp needs `--timeout 340`. This holds with the CAT-PROVISIONAL `CAT_HW1_MISSION_SCRIPTS 0` (no HW1 KAS
  script and no HW1 mission-start NIS until stage 6).
- **fe:** `Main_game_screen`, ASan 0.
- **Self-tests:** all exit **rc 0** (R2: rc 1 from GoK's quit assert). Statics 143/143 types as the probe (170/173
  `.shp`); meshes 1,758/1,758; textures 10,661 loaded, 0 failed; oracle 512 ticks with 0 sync errors; new ETG/effect
  self-test 9/9 scripts, 618 effects, ASan 0.
- **Release oracle self-test:** `SELFTEST PASS` with 27 RNG streams.
- **Builds:** `tools/build.sh --debug`/`--release` `--clean` give 0 warnings under `-Werror`; `tools/m2_layout32.sh`
  OK.

| item | report | result | audit | commit |
|---|---|---|---|---|
| close-out + systemic headers | [docs/m2-r3-closeout-headers.md](../docs/m2-r3-closeout-headers.md) | **quit rc 0:** `munmap` lengths, SDL audio stopped before the heap unmap, `tools/lsan.supp`. **Randy:** 27 streams. **Headers:** `SOF_*` renumbered (24 / 3 retired / 3 new), 63 parade slots, `AlternativeShips` 0xA8, `ClampInfo` 0x38. **Tables:** 8 derelict types, the 1,705-id string enum. **Saves:** version 0x10ad0027, chunk sizes, `saveCatLoadHook`. 27 static asserts | 0.92, PASS-WITH-FIXES | 2a5f51c |
| ETG + particle argument arrays | [docs/m2-r3-etg-particles.md](../docs/m2-r3-etg-particles.md) | one-pointer callables; the exe's 167-row table; GoK wrappers deleted; 0x100-byte particle; `/catEtgParseTest`, `/catEtgEffectTest` (9/9 scripts, 618 effects, ASan 0) | 0.875, PASS-WITH-FIXES | 2ee01a0 |
| selection | [docs/m2-r3-selection.md](../docs/m2-r3-selection.md) | `ShipSelect.c` filters at the exe bodies, `ioUpdateShipTotals` 45/45, skip-disabled argument split as the exe; 33 cat-new ported; no HW1 alias left | 0.94, PASS-WITH-FIXES | 91602f4 |
| ship code A | [docs/m2-r3-ships-a.md](../docs/m2-r3-ships-a.md) | 29 reused headers slot for slot; 14-type `GenericInterceptorStaticInit`; new `Cloak.c`, `Salvage.c`, `BeastWeapon.c`; 97 cat-new ported or hooked; sp passes `GenericInterceptor.c (205)` | 0.935, PASS-WITH-FIXES | ce5726d |
| universe statics + MEX | [docs/m2-r3-universe-mex.md](../docs/m2-r3-universe-mex.md) | **Level load:** the 60-entry Sect/Beast swap table. **`MEX.c`** and the `DockStaticInfo` 0x50 header. **`Universe.c`:** 7-race mine statics, team colours, 8-type derelict loader. **`Globals.c`.** Universe running | 0.925, PASS-WITH-FIXES | e068e39 |
| Ghidra name fixes | docs/binary-map.md "M2-R3 name fixes" | 3/3 applied (`aieCheckHealthLow/High`, `meshObjectFixupPacoUV`); export `cataclysm_gog_r6` | (audited in M2-R2) | d2c5494 |

Round size (`git diff 7ac664f HEAD`): 134 tracked files changed, +14,900 / -4,976 lines. In `src/`: 109 files, 1,295
added lines carrying `CAT`. Delta map: IDENTICAL / MODIFIED / REWRITTEN went from 3,175 / 1,168 / 77 to
**3,110 / 1,234 / 76**, with 1,374 functions hand-reviewed. Stage 2 is now **804 / 569 / 29 (240,423 MODIFIED B) +
829 cat-new**. The ports found IDENTICAL bodies modified at a high rate (ships-a 19 of 80, universe-mex 9 of 38), so
the R4 producers compare every IDENTICAL body in their scope.

### M2-R4: COMPLETE 2026-10-11 (stage 2 part B: universe update, command layer, combat/docking/salvage, the Cataclysm-only ships, the rest + Player layout + oracle stage 2)

Summary, exit test and the M2-R5/R6 plan: [docs/m2-r4-summary.md](../docs/m2-r4-summary.md). Workflow
`cycle/workflows/m2-r4.js` in four runs (`wf_985641f3-673`, `wf_20c186e0-be8`, `wf_6ec89407-0bf`, `wf_c1815ea8-981`;
three producers were cut off by the usage limit, committed as unaudited WIP snapshots `b39dea7` / `2f1a847` / `619ba34`
and resumed), 11 agents per run. Each port was checkpointed by its auditor. The R4 plan as launched (5 producers,
scopes, done-whens) is in docs/m2-r3-summary.md "Plan for M2-R4"; it replaced the table that stood here.

**Stage-2 exit test: MET** (this integrate step, clean builds at 127b5b9, plain harness, silent). Debug `hwi-engine`
(ASan + UBSan):
- **Mission01 headless (oracle mode `mission`):** two 6,000-tick runs (`r4-integrate-mission-a/b`, 22 s each) are
  byte-identical (1,476,370 B, 6,001 `T` records, 154 ships), **ASan 0**; a run perturbed by one `gamerand()` is caught
  at tick 3000 / `rng10` / 1 draw ahead and a flipped `ships_health` bit at tick 4000; the release build gives the same
  6,001 `T` records (`r4-integrate-mission-rel`, 4.4 s); `tools/oracle/mission_test.sh`: **MISSION TEST PASS**.
- **sp:** universe running, 1,248 ticks at the 340 s timeout (as R3), ASan 0, no leak (`r4-integrate-sp`).
  **fe:** `Main_game_screen`, ASan 0.
- **Self-tests:** rc 0, ASan 0: statics 143/145 types (170/173 `.shp`) with **108 StaticInits** under ASan (R3: 56);
  meshes 1,758/1,758; textures 10,661 loaded / 0 not loaded; oracle 512 ticks, 0 sync errors; etg 9/9 scripts, 618
  effects. **Release oracle self-test: `SELFTEST PASS`**, 27 RNG streams. Every quitting run rc 0 (9 debug, 4 release).
- **Builds:** `tools/build.sh --debug`/`--release` `--clean`: `[305/305]`, 0 warnings under `-Werror` (debug
  80,149,976 B, release 4,934,904 B); `tools/m2_layout32.sh` OK, now with `Player` 0x6530, `Universe` 0x3D9BC,
  `PlayerStats` 0x7F8, `TechStatics` 0x7AB4, `shipinprogress` 0x30 and 45 ship files' specifics.

| item | report | result | audit | commit |
|---|---|---|---|---|
| universe update + mesh state | [docs/m2-r4-univupdate.md](../docs/m2-r4-univupdate.md) | `SpaceObj` +0x80/+0x84 mesh state, per-vertex glow runtime and bullet-hit glow, binding parents, `univDeleteDeadShip` in the exe's order, killer collisions, `univUpdate` frame stepping and real-time quit/win, the deferred per-ship update (partial), ship counters; 52 IDENTICAL compared, 10 modified; 98 reviews | 0.95, PASS-WITH-FIXES | 3160fc0 |
| command layer + formations | [docs/m2-r4-command.md](../docs/m2-r4-command.md) | **specialFlags/specialFlags2 at the exe's bits** (61 sites, static asserts, 14 HW1 flags retired, 19 names chosen), orders 11-17 and formations 9/10, 16 CommandLayer/CommandWrap bodies, `RemoveShipFromBeingTargeted` rewritten, the external-build timer/update behind `CAT_EXTERNAL_BUILD_DRIVER 0`; declared partial (about 35 bodies, the orders' functions, the force-field formations left: R5) | 0.938, PASS-WITH-FIXES | d47a23f |
| combat, docking, salvage | [docs/m2-r4-combat.md](../docs/m2-r4-combat.md) | the salvage runtime (`Salvage.c`, HW1's 3,206 lines and every reader deleted, `SalCapCorvetteHeader` 0x30), the gun weapon word (`Gun` 0x88) and damage vs armour from the exe's tables, `gunShoot`/`cat_gunCreateBullet`, the 7-argument `ApplyDamageToTarget`, the Beast weapon, kamikaze, `Blast.c`, DFG/Sentinel bullet checks, asteroid pieces and the harvest loop, the sCarrier door, module slots; the Cataclysm dock purposes declared not done (R5) | 0.891, PASS-WITH-FIXES | d407410 |
| ship code B | [docs/m2-r4-ships-b.md](../docs/m2-r4-ships-b.md) | 22 new ship files on the exe's headers slot for slot + `ShipDisable.c`, `Dissolve.c`; the big gun, modules, Worker/Processor repair, RKO, Sentinel fields, mimics; the 22 dormant HW1 ids, 14 HW1-only ship files, `DefenseFighter.c`, the `RepairCorvetteHeader` and every dropped-type reader gone; statics 143/145 with 108 StaticInits | 0.921, PASS-WITH-FIXES | 3078c10 |
| the rest, Player layout, oracle stage 2, tools | [docs/m2-r4-rest-oracle.md](../docs/m2-r4-rest-oracle.md) | `Player` 0x6530 / `Universe` 0x3D9BC with 56 ILP32 asserts, `ResearchAPI.c` rewritten from the exe (tech sets), `Ship.health` +0x170 and the Player fields in the GOG DLL's checksum, `/oracleMission` + harness mode `mission` + `mission_test.sh`, `scriptSetShipsCB`/`unitCapCanCreateShip`, Bounties/Alliance/Crates/Task (time compression)/Tactics/Stats, the tools (`report --calib-samples`, `classify` on r6, 21 UI cat-new to stage 3) | 0.954, PASS-WITH-FIXES | 127b5b9 |

Round size (`git diff 9c2543d HEAD`): 257 tracked files, +32,262 / -16,821; `src/` 229 files, +26,350 / -16,231 with
1,772 added lines carrying `CAT`; 49 new `src/` files, 28 deleted. Delta map: IDENTICAL / MODIFIED / REWRITTEN went
from 3,110 / 1,234 / 76 (1,374 reviewed) to **3,065 / 1,278 / 77 (1,615 reviewed)**; 58 IDENTICAL bodies were found
modified. Stage 2 is now **783 / 589 / 30 (245,967 MODIFIED B) + 808 cat-new**, with 573 of the 808 cat-new names in
the tree; stage 3 is 928 / 263 / 7 (175,469 B) + 450 cat-new (338 not in the tree). 125 harness runs this round.

### M2-R5: COMPLETE 2026-10-17 (stage-2 close-out + stage 3 part A: layout + dock purposes, command layer + formations + tactics, render/GL, sound and speech, HUD)

Summary, exit test and the M2-R6 plan: [docs/m2-r5-summary.md](../docs/m2-r5-summary.md). Workflow
`cycle/workflows/m2-r5.js` in two runs (`wf_2afecf39-75d`, cut off by the weekly limit on 2026-10-12 with layout-dock
produced but unaudited, WIP snapshot `f59e2ef`; `wf_9f1eaba2-4de`, relaunched 2026-10-16 after the weekly reset with
the ports, audits and probe on opus (high) and integrate on fable (max), the layout-dock audit first), 12 agents. Each
port was checkpointed by its auditor. The R5 plan as launched (5 producers, scopes, done-whens) is in
docs/m2-r4-summary.md "Plan for M2-R5 and M2-R6"; it replaced the table that stood here.

**R5 exit test: MET** (this integrate step, clean builds at 02e8e4d plus the step's nine marker edits, plain harness,
silent). Debug `hwi-engine` (ASan + UBSan):
- **Mission01 headless (oracle mode `mission`):** two 6,000-tick runs (`r5-integrate-mission-a/b`, 24 s each) are
  byte-identical (1,476,370 B, 6,001 `T` records, 154 ships), **ASan 0**; the perturbed run is caught at tick 3000 /
  `rng10` / 1 draw ahead and the flipped `ships_health` bit at tick 4000; the release build gives the same 6,001 `T`
  records (`r5-integrate-mission-rel`, 4.8 s); `tools/oracle/mission_test.sh`: **MISSION TEST PASS**. The stream is
  IDENTICAL to the hud, sound and command-tactics audits' streams; the baseline moved once this round
  (command-tactics, from tick 0 in `ships_pos` / `univcheck`: decision 30).
- **sp:** universe running, 1,248 ticks at the 340 s timeout (a shared machine), ASan 0, no leak (`r5-integrate-sp`).
  **fe:** `Main_game_screen`, ASan 0 (284 file loads: R4's 232 plus the HUD's start-up data).
- **Self-tests:** rc 0, ASan 0: statics 143/145 (108 StaticInits), meshes 1,758/1,758, textures 10,661 / 0 not
  loaded, oracle 512 ticks 0 sync errors, etg 9/9 scripts, 618 effects (R4's numbers). **The five new ones:** dock,
  command, render, sound (**0 `fquant.c` / `fqcodec.c` UBSan reports, 0 `struct PATCH` reports**) and hud, all
  `result=PASS`. **Release oracle self-test: `SELFTEST PASS`**, 27 RNG streams. Every quitting run rc 0 (16 of 16).
- **Builds:** `tools/build.sh --debug`/`--release` `--clean`: `[308/308]`, 0 warnings under `-Werror` (debug
  82,303,928 B, release 5,023,224 B); `tools/m2_layout32.sh` OK, now with **`Ship` 0x63C (`health` 0x170)**,
  `SpaceObj` 0x98 (the attribute dword at +0x38), `Missile` 0x258, `Bullet` 0x164, `Derelict` 0x294, `Asteroid` 0x1B4,
  `CommandToDo` 0x330, `TacticsInfo` 0x9C4, `PATCH` 0x38 and the HUD structs.
- **No `CAT-TODO` / `CAT-PROVISIONAL` in `src/` names an M2-R4 or M2-R5 owner**: this step settled the last nine (the
  `catQueuedAttackTarget` clear and the gun-word copy of a converted ship ported from the disassembly, three dead
  extra arguments closed, four re-targets with reasons; all tagged `[integrate: M2-R5]`).
- UBSan kinds left: `fescreen` (M2-R6 producer 2), the ETG / particle opcode kinds and `BTG.c:388` (stage-1/2 LP64
  artefacts, listed, not blocking).

| item | report | result | audit | commit |
|---|---|---|---|---|
| layout-dock | [docs/m2-r5-layout-dock.md](../docs/m2-r5-layout-dock.md) | **Layout:** `Ship` 0x63C / `health` 0x170 / the +0x38 attribute dword, `SpaceObj` 0x98, `ShipOrDerelict` 0x238, `Missile` / `Bullet` / `Derelict` / `Asteroid`, the slave trade deleted. **Dock:** `Dock.c` rewritten on the purposes 0..10 (`cat_dockGetDockClass`, the 17-state cone machine, `clDock`, `cat_dockFinishedDocking`, `cat_clHarvestNearestResource`; 95 HW1 functions and 60 tweakables gone; every stand-in replaced). **Experience:** `cat_shipUpdateExperienceLevel` + 7 upgrade helpers. **97 IDENTICAL compared** (2 modified; `fsqrt` is HW1's table in the exe: decision 23). `/catDockTest` + mode `dock` | 0.895, PASS-WITH-FIXES (the self-test's carrier / processor coverage open: M2-R6 #4) | 6f64a21 |
| command-tactics | [docs/m2-r5-command-tactics.md](../docs/m2-r5-command-tactics.md) | `CommandToDo` 0x330 with the waypoint `MoveCommand`; orders 11-16 and their `cat_process*ToDo`, the link flight, 10 wraps (packets: stage 5); the remaining `CommandLayer.c` bodies; `clFormation`'s 4th argument, the force-field wall and sphere with the Sentinel links, the leader's collision centre; the exe's `TacticsInfo` 0x9C4 + 198-row table, 12 retaliation functions, `univCheckShipState`; 55 IDENTICAL compared (14 modified). `/catCommandTest` + mode `command` | 0.943, PASS-WITH-FIXES | 84a9d05 |
| render | [docs/m2-r5-render.md](../docs/m2-r5-render.md) | `cat_rndRenderObjectList` with the render state block and the depth-sorted pass, the Cataclysm main view, `meshSetFade`'s 8 channels, the render-time LOD; lens flares, fog layers, BTG's flashes; the ship renderers (Sentinel fields at 79 segments, Dreadnought charge, Nomad beam, blasts, droid proxies), `cat_meshLineIntersect`; every unread GL function compared (22 ported). `/catRenderTest` + mode `render` | 0.944, PASS-WITH-FIXES | af90827 |
| sound | [docs/m2-r5-sound.md](../docs/m2-r5-sound.md) | the fq codec at the exe (both `fquant.c` UBSan kinds gone), the mixer with the aligned bank patch table (every `PATCH` report gone), Cataclysm's sound events (92-row LUT, `GunSpecEvents.lut`), speech (3 queues + music, the exe's ids with HW1 ids muted, the race voice, alerts, letterbox, the 62-track music machine), battle chatter (88 rows verified); 13 R3/R4 CAT-TODOs now calls. `/catSoundTest` + mode `sound` | 0.85, PASS-WITH-FIXES (fixed the clean-build break: kas2c and `\|`) | a47a8a6 |
| hud | [docs/m2-r5-hud.md](../docs/m2-r5-hud.md) | status bars + picking at the exe bodies, the ship info panel (`ShipPrint.c`), ship names (`ShipNames.c`), 53 overlay icons (`OverlayIcons.c`), `InfoOverlay.c` rewritten (main overlay, message scroller, mission timer), pie plate + compass, `ShipView.c` from the disassembly, escape menu, loading screen; 40 IDENTICAL compared (5 modified). `/catHudTest` + mode `hud`. Deferred: the game-stats screen (M2-R6 #1) | 0.913, PASS-WITH-FIXES | 02e8e4d |

Round size (`git diff 1434b90`): 188 tracked files, +34,852 / -21,031; `src/` 170 files, +24,521 / -16,852 with 2,476
added lines carrying `CAT`; 6 new `src/` files, 0 deleted. Delta map: IDENTICAL / MODIFIED / REWRITTEN went from
3,065 / 1,278 / 77 (1,615 reviewed) to **3,009 / 1,334 / 77 (2,162 reviewed)**; 56 IDENTICAL bodies were found
modified. Stage 2 is now **765 / 607 / 30 (253,494 MODIFIED B) + 808 cat-new** (650 defined in the tree); stage 3 is
896 / 295 / 7 (187,650 B) + 450 cat-new (223 defined). The tree carries 40 `CAT-FIX` (R4: 7), 188 `CAT-TODO`, 22
`CAT-PROVISIONAL`. 181 harness runs this round.

### M2-R6 plan (stage 3 part B; details, the counting method and the stage-3 exit test in docs/m2-r5-summary.md "Plan for M2-R6")

5 producers in sequence + 5 auditors + integrate (11 agents). Counts: HW1-matched I / M / R functions by HW1 file
(MODIFIED + REWRITTEN bytes; then the MODIFIED / REWRITTEN bodies still unread), and the cat-new functions not yet
defined in `src/` (by name prefix, else by the nearest HW1-matched function), from `ghidra/delta_map.json` at the end
of R5. Every producer compares each IDENTICAL body in its scope, runs the engine only through `tools/m2_harness.sh`,
writes its `CAT-TODO`s on one line with the owner first, and (producers 4 and 5) records the mission-stream hash
before and after.

| # | producer | scope | I / M / R (M+R bytes); unread | cat-new left | done when |
|---|---|---|---|---|---|
| 1 | main region, mouse, popup menu, game-stats screen | `mainrgn.c`, `mouse.c`, `PlugScreen.c`: the band box, `cat_mouseGetContextAction`, the right-click popup menu (`cat_popupMenu*`, 31 functions, 19 KB), the `cat_ui*` special commands (link, unlink, mimic, leech, EMP, swarm, ram, siege cannon, cloak, halt, the protect formation), the mimic menu, `cat_bigGunFireCommand`, `cat_selComputeTargetActions`, the tactics / formation keys, the music keys; the game-stats screen (hud's deferral: `cat_gameStats*` x8, `cat_popupMenuDrawFrame`, `cat_primRectShadedHorizontal2`) | 36 / 23 / 1 (19,329 B); 15 | 90 / 38,322 B | a scripted `/catUiTest` (mode `ui`) opens the popup menu for a Mission01 selection, runs each `cat_ui*` command once through the wraps, closes it and shows the game-stats screen once; the exe's context-action table reproduced; ASan 0; sp, fe and the mission stream unchanged |
| 2 | front-end flow, screens, controls, fonts, strings, options, subtitles | `FEFlow.c`, `FEReg.c`, `UIControls.c`, `font.c`, `FontReg.c`, `ScenPick.c`, `ColPick.c`, `Options.c` (19 unread; the Cataclysm toggle set `cat_op*`, `cat_opSetAllDefaults`, Homeworld-classic, `OP_ExperienceMessages`, texture detail with the precache), `Demo.c`, `Titan.c`, `bink.c`, `StringSupport.c` (the 1,705-string readers), `Subtitle.c` (the rest of `subTitlesDraw`); the `fescreen` UBSan kind; the Bink term and the voice preview | 282 / 48 / 2 (17,627 B); 41 | 33 / 4,757 B | `/catFeScreensTest` pushes and pops every screen of the install's 22 `.fib` files once: ASan 0 and **0 `fescreen` UBSan reports**; `/catStringsTest` resolves all 1,705 ids; the option set at the exe's defaults round-trips; fe `Main_game_screen` |
| 3 | start-up, main, utility, key bindings, console | `main.c` (15 unread: the exe's start-up and command line, the GL start-up as far as SDL allows), `utility.c` (11 unread: `utyGameSystemsPreInit` / `Shutdown`, `gameStart` / `gameEnd`, `utyGameQuit`; `cat_utyGameQuitToMainOrStats`, the toggles), `KeyBindings.c` + `cat_kb*` (`default_keys.cfg` load / save / reload, key-to-string, conflicts, the 10 hot-key groups, `cat_keyNameStringsInit`), `Key.c`, the developer console (`cat_console*`, 17 functions, 9.5 KB: `experienceships`, `upgradeships`, `alltech`, the config file, the 0x00a751c0 toggle), the debug-key overlay, `screenshot.c`'s message (`cat_dd*` is the Win32 debug window: not applicable) | 89 / 39 / 0 (29,297 B); 29 | 89 / 21,025 B | the install's `default_keys.cfg` round-trips through the Cataclysm key-binding code (`/catKeysTest`); a scripted `/catConsoleTest` runs the command table headless, ASan 0; start-up as the exe's `main` (the R2 workaround list stays empty); fe and sp unchanged |
| 4 | construction, research, trade, launch managers; sensors; the external-build driver | `ConsMgr.c` (19 unread: **`cmDeterministicBuildProcess` 0x004592a0 / `Cancel` 0x00459040**, `cmCanBuildShipType`'s 3rd argument, `cmUpdateFactory`; `cat_shipUpdateExternalBuildAndAI`, `cat_cmExternalBuild*`, `cat_cmBuildJobFinished`, the SU bar and `cat_su*`, `cat_mothershipModuleSlotSet`; **`CAT_EXTERNAL_BUILD_DRIVER` to 1**), `ResearchGUI.c` (11 unread; `cat_rmAcquireShipTechs`, the print lists), `TradeMgr.c`, `LaunchMgr.c` (7 unread; the module-slot draws), `Sensors.c` (`cat_sm*`: ships, blobs, resources, mines, bounds, the viewport scene), `cat_rsMPRBShipDependSetCB`; the dock self-test's carrier / processor leg and the ACV judgement | 73 / 56 / 1 (30,789 B); 41 | 41 / 17,274 B | a scripted `/catBuildTest` (mode `build`) builds an externally built type and an internal one in a headless Mission01 run with the driver on: the ship appears at health 0 and finishes, ASan 0, two runs give identical streams; the dock self-test gets an sCarrier and an sProcessor host leg and judges the two ACVs; the sensors frame of `/catHudTest`; the research screen's tech sets at the exe's |
| 5 | NIS, animatic, mesh animation + repair droids, fog of war, camera | `NIS.c` (114 IDENTICAL to compare, 16 unread; the 27 `cat_nis*` script callbacks), `Animatic.c`; `MeshAnim.c` / `MadLinkIn.c` (`madAnimationStart`'s third argument, the exe's madanim layout, `cat_madAnimClear`, the animation sounds); the repair droids (`droidCreateArray` / `UpdateArray` / `cat_droidFreeArray` and helpers, `Ship+0x4C0`); the FOW (`cat_fow*` / `fow*`: `cat_fowCreatePlayerGrids` at `levelInit`, the cubes, the boundary, `cat_univShipFOW*`, the stealth update, `cat_fowCanPlayerSeeShip` at its waiting sites, the picking helpers' grid, the FOW saves); the camera UI (`CameraCommand.c` 7 unread, `cat_mc*` motion cues, `cat_cc*`); the beam's sixth argument | 184 / 39 / 0 (20,083 B); 29 | 74 / 21,546 B | `/catNisParseTest` parses every NIS and NIS script of the install, ASan 0; `Player.catFOWGrid` non-NULL for every player in a Mission01 run and the stealth update runs; the camera bodies at the exe's; the dock self-test's Worker reaches its paused jack animation; the droids update and render; **the mission stream re-baselined**: the pre-droid pair IDENTICAL to R5's `645f94c69d68319e` records, the post-droid pair byte-identical with the change confined to the stream-0 fields, both in the report |

**R6 integrate (stage-3 exit):** from a clean build on the plain harness: fe `Main_game_screen`, `/catFeScreensTest`
with 0 `fescreen` reports, `/catStringsTest` 1,705 / 1,705; the dock, command, render, sound, hud, ui, keys, console,
build and nis self-tests ASan 0; sp 1,248+ ticks at 340 s with the Cataclysm HUD and renderers drawing; two
6,000-tick Mission01 streams identical in debug and release on the new baseline (its hash and the moved fields
stated against R5's); `/catBuildTest` with `CAT_EXTERNAL_BUILD_DRIVER 1`; the old self-tests unchanged or better;
every quit rc 0; layout32 OK; no `CAT-TODO` / `CAT-PROVISIONAL` naming an M2-R5 or M2-R6 owner. Then stage 4 (AI:
386 / 91 / 2, 23 cat-new; the `CAT-TODO(stage 4 AI)` stand-ins, the `aishipFlyToPointAvoidingObjsFunc` and
corvette-cone items first), stage 5 (LAN, nice-to-have) and stage 6 (missions: the kas2c code, the NIS-first start,
`levelPreInit`'s Beast marking, the asteroid box, the letterbox callers; `CAT_HW1_MISSION_SCRIPTS` retired).

### Decisions the user owns

**Answered by the user 2026-10-10** (R1 list numbering; the user's own numbering in brackets where it differs). Items
1-9 were applied in M2-R2; items 11 and 12 in M2-R3.
1. ANSWERED: accept GoK 3af31fb as `src/`.
2. ANSWERED: Cataclysm behaviour wins, decided per subsystem as it is ported. The 16 Hz universe is the default; GoK's
   64 Hz is only behind `/gok64Hz`. `-O2 -fno-strict-aliasing`, no fast-math, pinned before any checksum comparison
   (done in the M2-R2 oracle item).
3. ANSWERED: Relic ad1bb82 `Game/` and `Ships/` in-tree as reference (`src/reference/hw1-ad1bb82/`, commit 86c3ee6).
4. ANSWERED: re-read the 1,013 low-confidence IDENTICALs and apply the audit's reclassifications (done:
   docs/m2-r2-reread.md).
5. ANSWERED: the oracle may call the game's own record/playback functions; no OS input is synthesised (built into the
   DLL).
7. ANSWERED [their 6]: clang-18 + lld-link first, mingw fallback. **Outcome: the DLL builds with clang-18 + lld-link;
   no mingw install is needed.**
9. ANSWERED [their 7]: LAN interop with stock 1.01 is nice-to-have, not required. No effort is spent on it, and the
   wire format is not broken gratuitously.
11. ANSWERED 2026-10-10: **the faithfulness defaults already in the tree are accepted.**
    - The 22 dropped HW1 types stay dormant at ids 200-221 until M2-R4 #4 deletes them.
    - `kCloakGenerator`/`tCloakGenerator` use `DefaultShipHeader` where the exe has NULL.
    - `crateInit` resets 46 entries.
    - The mission-17 texture cut follows the stock exe, not CET.
    - The fq audio code follows the binary.
12. ANSWERED 2026-10-10: **run the Ghidra name fixes.** Done in M2-R3: commit d2c5494, export `cataclysm_gog_r6`.
- ANSWERED 2026-10-10: **the oracle reference recording (6) and the two saves (8) come later and do not block.** The
  rounds build what does not need them. The hooks are ready: `saveCatLoadHook` (`src/Game/SaveGame.c`) for the
  saves; `tools/oracle/` and docs/m2-r2-oracle.md "What is left" 1 for the recording.

**Still open (the user's; none blocks M2-R6):**
6. **First reference recording.** Which skirmish map and player setup you record through the menus as the first
   oracle reference.
8. **Two saves** from the M0 play-test (one mission, one skirmish), for `tools/formats/savegame_probe.py` and
   `saveCatLoadHook`. Without them the save layout stays code-derived.
10. **The first live oracle session is yours** (goes with 6). Copy `build/oracle-dll/hwi_oracle.dll` and
    `hwi_oracle_loader.exe` next to `cataclysm.exe`, record one skirmish, then make two playback runs; the procedure
    is in docs/m2-r2-oracle.md "What is left" 1, and since M2-R5 one command
    (`tools/oracle/record_reference.sh record` / `verify`, commit 1ad47c6). Until then three things are unverified: that the DLL loads under
    Proton, that the H6 trigger works, and that `vqpc` gives 1/16 s per tick.
- The M0 play-test sign-off (docs/PLAN.md).

**New from M2-R3** (each has a default in the tree; say if you want otherwise):
13. **LeakSanitizer suppressions.** `tools/lsan.supp` suppresses 11 start-up allocations: libdbus, the GL driver,
    GoK's SDL mutexes and one `rinit` mode record. Every other leak still fails a run. The entries are dropped when
    stages 3 and 5 port those paths.
14. **`CAT_HW1_MISSION_SCRIPTS 0` (CAT-PROVISIONAL).** No HW1 KAS mission script and no HW1 mission-start NIS until
    stage 6 ports Cataclysm's kas2c missions and its NIS-first start. Without the switch, sp stops at `KAS.c (265)`.
15. **Chosen names.** The 3 new `SOF_*` flags have names chosen here, since the exe's are unknown:
    `SOF_CatCloakDevice` 0x200, `SOF_CatShipPart` 0x40000000, `SOF_CatDocking` 0x80000000. The 3 HW1 flags with no
    exe bit are defined 0 (`SOF_CloakGenField`, `SOF_Cloaking`/`DeCloaking`, `SOF_Slaveable`).
16. **The LP64 save blob changes with each appended `Ship` field.** Nothing is pinned until the saves (8) arrive.

**New from M2-R4** (each has a default in the tree; say if you want otherwise):
17. **`CAT_EXTERNAL_BUILD_DRIVER 0` (CAT-PROVISIONAL, `src/Game/CommandLayer.h`).** The 30 `.shp` types with
    `externalBuild TRUE` (destroyers, dreadnoughts, carriers, processors, mothership modules) are built inside the
    factory and created at the end, as HW1 does, until ConsMgr's external-build driver (`cmDeterministicBuildProcess`
    0x004592a0) lands in M2-R6; with the branch on now they would be created at health 0 and never finish.
18. **Chosen names** for exe bits, orders and fields whose names are unknown, taken from their setter functions:
    `SPECIAL_CatKasSpecialRepairModifier` 0x2000 and 18 `SPECIAL_2_Cat*` flags (`CatBeingBuilt` 0x8, `CatNoAvoidance`
    0x40, `CatParalyzed` 0x4000, `CatBeastified` 0x10000, `CatSalvaged` 0x20000, `CatExempt20` 0x100000, ...);
    `COMMAND_CAT_13` / `COMMAND_CAT_17` (no setter found); the Player fields `catFOWGrid`, `catArmourLevelCap`,
    `catWeaponAvailable`, `catUpgradeBits`, `catUnknown08`; `catSensorsHoldRenderList`, `catSelection1A0`,
    `catStat28`, `catBuiltForEachClass`, `catStat7D0`, `catUnknown24`, `catExternalShipStarted`, `catExternalShip`.
19. **HWI deviations only where the exe would fault or leak**, each marked `CAT-FIX` or commented: the bullet-hit
    glow skips LOD levels without slots; a vanished salvage depot is "no change" (the exe reads NULL+0x58);
    `cat_clampUnclampAllFromShip` stops when the list is gone; `cat_moduleShipEject` uses direction 0 for an unlisted
    module type; `cat_mimicBecomeTarget` returns on an asteroid type outside 1-3; `cat_univDeleteMimicResource` frees
    the asteroid; the big-gun shell is NULL-tested before it is tracked; Beast-weapon hit entries are freed.
20. **Two medium-confidence readings** (noted in the reports): `rmGiveTechToPlayerByType`'s dependency index and
    `getPlayerBountyRender`'s array read.
21. **The 22 dormant HW1 types and 14 HW1-only ship files are deleted**; where the stage-4 AI asked for one of them the
    request is dropped or a Cataclysm type stands in (`CAT-TODO(stage 4 AI)`, 14 sites).
22. **The `Ship` prefix is not yet the exe's** (`health` at ILP32 0x15C, exe 0x170; `Ship` not pinned at 0x63C). The
    oracle hashes `health` by name, so the streams are unaffected; the pin is M2-R5 producer 1's first item.

**New from M2-R5** (each has a default in the tree; say if you want otherwise; details in docs/m2-r5-summary.md
"Decisions"):
23. **`fsqrt`.** The exe keeps HW1's table square root (`fsqrt` 0x00472860, `fmathInit` 0x004728c0); this tree maps
    `fsqrt` to `sqrtf` (GoK). Default: `sqrtf` stays. Switching changes every distance in the simulation and every
    oracle baseline, so it is yours to call; the reference recording will show it first.
24. **Chosen names** where the exe's are unknown: `catRetreating` 0x1D0, `catObjReserved88[4]`, `catShipReserved5B0`,
    `catMissileReserved248[3]`, `catNavLightStatic` 0x634, `catDamageFactor`, `catMaxVelocity`;
    `CAT_SHIPFLAG_Launching` / `Bit7`; `CAT_ATTRIBUTES_DockBaysClosed` 0x400, `DockWithEnemies` 0x40000, `BlastHit`
    0x20000, `BlastImmune` 0x10000; the dock purposes `CAT_DOCK_*` 0..10 and states `CAT_DOCKSTATE_*` 0..17;
    `CAT_MOVEPATH_*`, `CAT_UPGRADE_Link`; `CAT_SPEECH_ACTOR_FLEET_INTEL` (actor 0x2000), the game-stats music track
    0x28, `catBatLastStatus*`; the HUD structs. `COMMAND_CAT_13` / `COMMAND_CAT_17` stay defined but unused (no setter).
25. **HWI deviations where the exe would fault or leak** (40 `CAT-FIX` markers, 7 in R4): the dock helpers' -1-class
    and point-index guards, `FillInCarrierMothershipInfo`'s four-slot array, `flightmanEndoverInit`'s allocation
    size, the 8-byte alignment of a ship's extra blocks, the freed `getShipAndItsFormation` copy, the lens-flare
    element bound, `musicEventUpdateVolume`'s restart level, `cat_soundEventBlast` / `AsteroidBreak` testing
    `enableSFX`, `catIoMessageInsert`'s 0x1ff bound.
26. **HW1-named speech ids are muted** (`HW1_SPEECH_ID` 0x400): about 90 HW1-named speech call sites stay silent until
    each port passes the exe's id. Default: keep (the alternative is a bulk conversion now).
27. **Four battle-chatter events have id 0 in the exe** (entries 76 / 77 / 81 / 87) and never speak; the tree too.
28. **GoK's GL backend stays** (`rndLoadTarga`'s `GL_CLAMP_TO_EDGE` where the exe uses `GL_REPEAT`; glcompat, the
    RGL / Glide / D3D device lists, the Transformer, the WGL set-up), as do SDL's audio layer in place of the exe's
    DirectSound / waveOut thread and no DirectDraw debug window.
29. **The 0.325 pixel-centre offset** of the exe's screen-to-GL conversion was dropped tree-wide by GoK, so the band
    box and the icon quads sit 0.325 px off the exe's. Default: GoK's; a renderer decision (M2-R6 producer 1 or you).
30. **The mission-stream baseline moved in command-tactics** (from tick 0 in `ships_pos` / `univcheck`; the cause
    was not isolated: the formation centre at the leader's collision centre and the ported command-layer bodies,
    both the exe's). Default: the new baseline (`645f94c69d68319e` in this step's build) is accepted; the reference
    recording is the real check; M2-R6's droid port will move it again (the plan's re-baseline rule).
31. **Medium-confidence readings kept:** `catRetreating`'s meaning; `cat_clUnlink` may create a command with no
    ships (as the exe; not a CAT-FIX); the dock state names; the two reserved `Ship` words.
32. **The integrate step's two small ports** (the `catQueuedAttackTarget` clear in
    `univRemoveAllShipReferencesFromSpecialShips`, the gun-word copy in `cat_univConvertShip`) are grounded in the
    disassembly and unaudited; the M2-R6 audits may re-check them.
33. **The dock self-test's coverage gap** (no sCarrier / sProcessor host leg, the ACVs and the Worker logged, not
    judged) is carried to M2-R6 producer 4 instead of blocking R5.

## M2-R6: IN FLIGHT from the user's terminal session 38027cce (2026-10-17 14:2x), NOT from the orchestrator

The orchestrator must NOT launch cycle/workflows/m2-r6.js: it is already running in session 38027cce (Fable, Remote Control). If prompted, reply that R6 is in flight elsewhere and do nothing. Merges to main are handled by that session.

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
| m2-r3 | `wf_914794bc-f5e` (orchestrator session) | **COMPLETE** (integrate commit 1b96aab; resumed once after a usage limit, universe-mex continued from its uncommitted edits): R3 exit test met: sp on Mission01 reaches the running universe (1,248 ticks, ASan 0) with the provisional CAT_HW1_MISSION_SCRIPTS 0, fe Main_game_screen, self-tests unchanged or better, release oracle SELFTEST PASS with 27 streams, every quit rc 0; all five audits PASS-WITH-FIXES (0.875-0.94); details in docs/m2-r3-summary.md. Was: launched 2026-10-10 from cycle/workflows/m2-r3.js on branch `m2`: stage 2 part A (docs/m2-r2-summary.md). Five src/ ports IN SEQUENCE: closeout-headers -> etg-particles -> selection -> ships-a -> universe-mex, each followed by its auditor, which checkpoints the port; in parallel one Ghidra stage applies the 3 audited name fixes (export `cataclysm_gog_r6`); then integrate (R3 exit test: sp reaches universe running on Mission01 with ASan 0). 12 agents. User 2026-10-10: faithfulness defaults accepted; the oracle recording and two saves come later and do not block | if cut off: resumeFromRunId wf_914794bc-f5e from the orchestrator session, or relaunch the script (ports resume from their report docs + committed earlier ports; the Ghidra stage from its export) |
| m2-r4 | `wf_c1815ea8-981` (session 13e7880d, run 4 of the round; earlier `wf_6ec89407-0bf`, `wf_20c186e0-be8`, `wf_985641f3-673`) | **COMPLETE 2026-10-11** (integrate commit e3e0111): stage-2 exit test met from clean builds on the plain, silent harness: two 6,000-tick headless Mission01 oracle runs byte-identical, ASan 0 (perturbed run caught at tick 3000, flipped copy at 4000; release run identical); sp 1,248 ticks ASan 0; fe Main_game_screen; self-tests unchanged or better (108 StaticInits); release SELFTEST PASS; every quit rc 0; layout32 OK (Player 0x6530, Universe 0x3D9BC); debug + release 305/305, 0 warnings. Five audits PASS-WITH-FIXES: univupdate 3160fc0 (0.95), command d47a23f (0.938), combat d407410 (0.891), ships-b 3078c10 (0.921), rest-oracle 127b5b9 (0.954). Details in docs/m2-r4-summary.md; decisions 17-22 added in "## M2" | M2-R5 (stage-2 close-out + stage 3 part A; plan table in "## M2" and docs/m2-r4-summary.md): script cycle/workflows/m2-r5.js not yet written |
| m2-r5 | `wf_9f1eaba2-4de` (session 13e7880d, run 2; earlier `wf_2afecf39-75d`, cut off by the weekly limit 2026-10-12) | **COMPLETE 2026-10-17** (integrate commit a57394b): R5 exit test met from clean builds (308/308, 0 warnings) on the plain, silent harness: two 6,000-tick Mission01 streams byte-identical with ASan 0 (perturb caught at 3000, flip at 4000; release identical), sp 1,248 ticks ASan 0, fe Main_game_screen, the old self-tests unchanged, release SELFTEST PASS, layout32 OK with Ship 0x63C; the five new self-tests dock, command, render, sound (0 fquant/fqcodec/PATCH UBSan) and hud PASS with ASan 0; no CAT-TODO names M2-R4 or M2-R5. Audits PASS-WITH-FIXES: layout-dock 6f64a21 (0.895), command-tactics 84a9d05 (0.943), render af90827 (0.944), sound a47a8a6 (0.85), hud 02e8e4d (0.913). Mission-stream baseline moved in command-tactics, new baseline 645f94c69d68319e accepted by default (decision 30). Details in docs/m2-r5-summary.md; decisions 23-34 added in "## M2" | M2-R6 (stage 3 part B; plan table in "## M2" and docs/m2-r5-summary.md "Plan for M2-R6"): script cycle/workflows/m2-r6.js not yet written |

## Next steps after the workflows land
1. Opus audit of `ghidra/proposals/qwen-naming.json` (30-sample precision) → `qwen-naming.verified.json`, so the next reconcile includes it.
2. Binary-map rounds 1-5 are COMPLETE (rows above). Naming is saturated (docs/binary-map.md R5.7), and the remaining gaps are in R5.6. Qwen's output is still only 12 proposals. With no `FUN_*` left, its queue is empty against `cataclysm_gog_r5`, so step 1 reduces to comparing those 12 with the applied names. Round-2 A2 work (ImportHeaders.py, prep_ghidra_headers.py, `--post-script` in ghidra_headless.sh, docs/ghidra-round2-prep.md) was merged from the A2 worktree in the round-2 checkpoint.
3. Keep the play-test track separate: `tools/run_game.sh --no-gamescope --virtual-desktop 1920x1080` (gamescope is test-only, docs/CONSTRAINTS.md).
