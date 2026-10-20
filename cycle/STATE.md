# Work cycle and resume point

Last checkpoint: 2026-10-20 08:49

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
    gpu gate       : free (GPU at 0%)
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

### M2-R6: COMPLETE 2026-10-18 (stage 3 part B: main region + popup menu + game stats, front end, start-up + keys + console, managers + external build, NIS + FOW + camera + droids; plus motion interpolation and oracle mode A)

Summary, exit test and the M2-R7 plan: [docs/m2-r6-summary.md](../docs/m2-r6-summary.md). Workflow
`cycle/workflows/m2-r6.js`, run from the user's terminal session 38027cce (Fable), not from the orchestrator; 15 agents
(seven ports in sequence, seven audits, integrate); the startup-keys port was cut off twice by the usage limit (WIP
snapshots `57ae3b4`, `6645763`) and resumed from its report. Each port was checkpointed by its auditor. The R6 plan as
launched (5 producers + 2 additions, scopes, done-whens) is in docs/m2-r5-summary.md "Plan for M2-R6"; it replaced the
table that stood here.

**R6 exit test (the stage-3 exit): MET** (the integrate step, clean builds at 9e20b5b plus the step's integrate edits,
plain harness, silent). Debug `hwi-engine` (ASan + UBSan):
- **Mission01 headless:** two 6,000-tick runs (`r6-integrate-mission-a/b`, 19 s each) byte-identical (1,476,370 B,
  6,001 `T` records, 154 ships), **ASan 0**, sha256 **`49240abcf9f0ed97`**; the perturbed run caught at tick 3000 /
  `rng10` and the flipped `ships_health` bit at 4000; the release pair `cdc60256f37f6961` IDENTICAL to debug with its
  own perturb / flip caught (`MISSION TEST PASS` twice). **Against R5's `645f94c69d68319e`: `rng6` (10 draws ahead at
  tick 1) and `rng_all` only** (the managers port's exe-faithful computer-player timer), every other field IDENTICAL:
  the new baseline (decision 44).
- **sp:** universe running, **3,376 ticks** at 340 s (the animatics now play at their 30 fps), ASan 0, no leak. **fe:**
  `Main_game_screen`, ASan 0, 0 UBSan reports (the `fescreen` kind is gone from every run).
- **Self-tests:** rc 0, ASan 0: statics 143/145 (108 StaticInits), meshes 1,758/1,758, textures 10,661 / 0 not loaded,
  oracle 0 sync errors, etg 618 effects (R5's numbers); release oracle `SELFTEST PASS`. The twelve stage-3 self-tests
  all `result=PASS`: dock (both host legs, the ACVs judged, `jackPausedTick=1024`), command (`stealthUpdates=387`),
  render (`droids=2500`), sound (0 `fquant` / `PATCH`), hud (the sensors frames), ui, fescreens (195 screens, **0
  `fescreen` UBSan**, options 34/34), strings (1,705 / 1,705), keys (the written `keys.cfg` equals the GOG game's),
  console, build (`CAT_EXTERNAL_BUILD_DRIVER 1`, two runs byte-identical `85969556400f4a83`), nisparse (14 NIS, 18
  animatics); motion, nis and toggle PASS.
- **Motion-interpolation guard-rails:** the six debug streams with the flags on, `/noInterp` and `/noNISInterp` are
  byte-identical; the `/noInterp` dumps equal the pre-feature dumps (`b2af0d6decdf6474`, `3f04c837e605ba23`); with
  the flags on the crossing object has 0 / 724 and the tracked NIS object 0 / 1389 repeated frames (off: 640 / 719 and
  1231 / 1384, 8 of 9 frames a tick).
- **Oracle mode A:** the replay of `oracle01` runs to tick 9601 / packet 4800 (81 ships, ASan 0, 34 s, byte-identical
  twice); against the GOG stream `oracle01-rec.hwo` the **first divergence is tick 136, `ships_health`** (ship 47,
  `cat_clExternalBuildUpdate` 0x0044dfc0's single x87 rounding, the hash reproduced exactly), IDENTICAL in every
  simulation field through tick 135; `players` at 93 and `rng6` at 161 are the stock game's own record / playback
  differences (GOG's playback shows them too), `ships_rot` is the DLL's offset defect, `rng_all` / `fpcw` excluded.
- **Builds:** `tools/build.sh --debug` / `--release --clean`: `[315/315]`, 0 warnings (debug 87,136,752 B, release
  5,362,248 B); `tools/m2_layout32.sh` OK with the UI, key, `.fib`, NIS, FOW and camera pins (`CameraCommand` 0x94).
- **No `CAT-TODO` / `CAT-PROVISIONAL` in `src/` names an M2-R5 or M2-R6 owner**; the integrate step settled the last
  six "stage 3" tags (three small ports grounded in the decompile: `univKillPlayer`'s kill credit, manager closes and
  exe speech id, `univKillOtherPlayersIfDead`'s ether look and kill credit, the time-compression string; two
  re-targets with reasons: `DeclarePlayerWin` to stage 5 LAN, the harvest effect's real32 parameters to M2-R7 ETG; one
  closed by decision 28). It also brought `ghidra/delta_map_review.json` back in step with the map (16 reverted and
  107 missing rows: decision 55).
- UBSan kinds left (all non-fatal): the ETG / particle opcode kinds, `BTG.c:388` / `BTG.c:864`,
  `stb_image_write.h:1263` (sp), `Select.c:1166` (the replay), `Subtitle.c:1144` (the nis mode), `AIShip.c:1467` (dock;
  a stage-4 file).

| item | report | result | audit | commit |
|---|---|---|---|---|
| mainrgn-popup | [docs/m2-r6-mainrgn-popup.md](../docs/m2-r6-mainrgn-popup.md) | `PopupMenu.c` (32 exe functions, the item tables), the 11 unread `mainrgn.c` bodies, the band box, 26 `cat_ui*` commands, `cat_mouseGetContextAction` with both jump tables, the 31 cursors, the game-stats screen, `cat_selComputeTargetActions`, the mimic menu, the big gun. `/catUiTest` + mode `ui` | 0.92, PASS-WITH-FIXES (the inverted repair-target filter and `TerryStats` test fixed) | 60bfd21 |
| frontend | [docs/m2-r6-frontend.md](../docs/m2-r6-frontend.md) | the `fescreen` UBSan kind fixed faithfully (`FIB_ScreensOffset`), the 26 `cat_op*` option callbacks and 24 HW1 option bodies, strings (`strLoadLanguage`, `fontPrintN`'s `bAdditive`), subtitles, ScenPick at the exe's; 230 IDENTICAL compared (15 modified). `/catFeScreensTest` + `fescreens`, `/catStringsTest` + `strings` | 0.912, PASS-WITH-FIXES | 142a8e2 |
| startup-keys | [docs/m2-r6-startup-keys.md](../docs/m2-r6-startup-keys.md) | `Cataclysm.cfg` (the exe's 94-row table), the exe's key bindings (`default_keys.cfg` / `keys.cfg`, the command table's callbacks, HW1's key remap gone), the developer console (`Console.c`), `main` / `gameStart` / `gameEnd` / `utyGameSystemsPreInit` at the exe's, the debug-key overlay; 60 IDENTICAL compared (3 modified). `/catKeysTest` + `keys`, `/catConsoleTest` + `console` | 0.90, PASS-WITH-FIXES | c3e7c1f |
| managers | [docs/m2-r6-managers.md](../docs/m2-r6-managers.md) | **`CAT_EXTERNAL_BUILD_DRIVER 1`**: `cmBuildTaskFunction`, the deterministic build list, `cat_shipUpdateExternalBuildAndAI`, the support units; `ConsMgr.c` at the exe's (nine lists from its data), `ResearchGUI.c` and `LaunchMgr.c` rewritten, `Sensors.c`'s Cataclysm scene (`cat_sm*`), the `SOF_CatDocking` collision exemption, the dock test's carrier / processor legs. `/catBuildTest` + `build` | 0.86, PASS-WITH-FIXES (the tech-0x22 gate, the convert-ship rows fixed) | a6e72c9 |
| nis-fow-camera | [docs/m2-r6-nis-fow-camera.md](../docs/m2-r6-nis-fow-camera.md) | the Cataclysm NIS opcodes and 0x28-byte events, all 16 unread `NIS.c` bodies and the 27 `cat_nis*` callbacks, the animatics at 30 fps, `Camera` 0x5C / `CameraCommand` 0x94, the `cat_cc*` calls, the motion cues, **`FOW.c`** (local-player grid, cubes, boundary, stealth), the beam's sixth argument, the exe's madanim with `madAnimationStart`'s mode at 25 callers, the repair droids; 23 IDENTICAL found modified. `/catNisParseTest` + `nisparse` | 0.89, PASS-WITH-FIXES | 3aa4cce |
| motion-interp | [docs/m2-r6-motion-interp.md](../docs/m2-r6-motion-interp.md) | render-time interpolation of gameplay motion (side table + `alpha`, the draw bracket) and of the NIS curves (`bsCurveEvalAt`), the "Smooth motion" checkbox, `Cataclysm.cfg` rows, `/noInterp` / `/noNISInterp`, console rows; `HWI-FEATURE(motion-interp)` on every line, no `CAT:`. Modes `motion`, `nis`, `toggle`, `tools/nis_smoothness.py` | 0.884, PASS-WITH-FIXES (a stale-capture blend after re-enabling fixed) | af50963 |
| oracle-mode-a | [docs/m2-r6-oracle-mode-a.md](../docs/m2-r6-oracle-mode-a.md) | the Cataclysm save converted field by field (`SaveCat32.c`, 120 generated layout tables), the exe's load / save / fix order, the 27 packet types at the exe's offsets, `LoadRecordedGame`, `/oracleReplay` + mode `replay`; first divergence tick 136 `ships_health` owned with evidence | 0.93, PASS-WITH-FIXES | 9e20b5b |

Round size (`git diff a57394b`): 212 tracked files, +51,207 / −20,333 (plus the integrate edits); `src/` 173 files,
+32,786 / −13,306 with 2,833 added lines carrying `CAT`; 13 new `src/` files, 0 deleted. Delta map: IDENTICAL /
MODIFIED / REWRITTEN went from 3,009 / 1,334 / 77 (2,162 reviewed) to **2,942 / 1,394 / 84 (2,816 reviewed)**; 68
IDENTICAL bodies were found modified, 7 MODIFIED became REWRITTEN. Stage 3 is now **847 / 337 / 14 (199,754 + 8,558
B) + 450 cat-new (428 defined)**; stage 2 758 / 614 / 30 + 808 cat-new (760 defined). The tree carries 121 `CAT-FIX`
(R5: 40), 105 lines with `CAT-TODO` (96 owner tags: stage 4 22, stage 5 25, stage 6 44, M2-R7 4), 8 `CAT-PROVISIONAL`,
433 `HWI-FEATURE(motion-interp)`. 411 harness runs this round (runs.txt lines 663-1073), 79 of them the integrate step's.

### M2-R7: COMPLETE 2026-10-19 (stage 4: the oracle's x87 rounding + AI ship / utilities, AI orders / moves / handlers / events + the ai self-test, AI player / fleet / resource managers / teams + the Cataclysm skirmish AI, the R6 leftovers; plus the skirmish-start merge)

Summary, exit test and the M2-R8 plan: [docs/m2-r7-summary.md](../docs/m2-r7-summary.md). Workflow
`cycle/workflows/m2-r7.js` (run id `wf_b238b029-e72`), run from the user's terminal session 38027cce (Fable), not from
the orchestrator; 9 agents (four ports in sequence, four audits, integrate); no producer was cut off (producer 3 handed
off with its final gates running; its auditor reran them). The integrate step was cut off by the weekly usage limit on
2026-10-18 13:20 after its first pass (WIP snapshot `d0a4216`) and resumed on 2026-10-19, after the user's session had
merged the branch `skirmish-start` into `m2` (`49823fa`, docs/skirmish-start-fix.md: the level loader's MissionSphere
readers from the exe, the front-end skirmish crash fixed, harness mode `skirmish`, visible fatal errors in release; the
last row of the table below); the whole exit test was then re-run from the merged tree. Each port was checkpointed by its
auditor. The R7 plan as launched (4 producers, scopes, done-whens) is in docs/m2-r6-summary.md "Plan for M2-R7"; it
replaced the table that stood here.

**R7 exit test (the stage-4 exit): MET** (the integrate step, clean builds of the merged tree `cd5866d`, plain harness,
silent, 49 runs `r7i2-*` in four parallel chains; the first pass's 46 runs `r7-integrate-*` at `3e33fee` gave the same
content for every stream). Debug `hwi-engine` (ASan + UBSan):
- **Mission01 headless:** two 6,000-tick runs (`r7i2-mission-a/b`, 22 s each) byte-identical (1,476,370 B, 6,001 `T`
  records, 154 ships), **ASan 0**, sha256 **`9f7416bb64fec98f`** (the first pass: `39a512d0465d73a6`, the same content);
  the perturbed run caught at tick 3000 / `rng10` and the flipped `ships_health` bit at 4000; the release pair
  `ec49dd4649ce882e` IDENTICAL to debug with its own perturb / flip caught (`MISSION TEST PASS` twice). **Against R6's
  `49240abcf9f0ed97`: IDENTICAL in every field over 6,001 ticks, the header's build identity only** (9e20b5bd vs
  cd5866de): the AI ports do not reach Mission01's computer player (no fleet control without the KAS call under
  `CAT_HW1_MISSION_SCRIPTS 0`) and the merge's start-position table draws nothing, so nothing moved (decisions 44 / 57 /
  68). The AI's own guard-rail is the `ai` self-test stream `c6ff74db1a364512` (two runs byte-identical, 16/16 checks).
- **sp:** universe running, **3,376 ticks** at 340 s (the gate 1,248; four chains beside it), ASan 0, no leak. **fe:**
  `Main_game_screen`, ASan 0, 0 UBSan reports.
- **Self-tests:** rc 0, ASan 0: statics 143/145 (108 StaticInits), meshes 1,758/1,758, textures 10,661 / 0 not loaded,
  oracle 0 sync errors, etg 618 effects (R6's numbers); release oracle `SELFTEST PASS`. The thirteen scripted self-tests
  all `result=PASS`: dock (the R6 line; the `AIShip.c:1467` UBSan gone), command (`stealthUpdates` as R6), render
  (`droids=2500`), sound (0 `fquant` / `PATCH`), hud (now with the waypoint leg: `draws=181 randoms=606`), ui, fescreens
  (195 screens, 0 `fescreen` UBSan), strings (1,705 / 1,705), keys, console, build (two runs byte-identical
  `31fbd86ebc8c2b1a`), nisparse (14 NIS, 18 animatics), toggle x3 (the run-only switches); **ai** (16/16, `c6ff74db1a364512`
  twice); **skirmish** (the merge's: LiveFireExercise2 twice byte-identical `979f7d23606207f5`, 89 ships; Armed8
  `af269ea4bc7dca8f`, 54 ships, the spheres of the absent players 2..7 skipped; PASS, ASan 0).
- **Motion-interpolation guard-rails:** the three 6,000-tick debug streams with the flags on, `/noInterp` and
  `/noNISInterp` are byte-identical (`9f7416bb64fec98f`); the `/noInterp` dumps equal the pre-feature dumps
  (`b2af0d6decdf6474`, `3f04c837e605ba23`); the flags-on dumps are still R6's (`d9a8661fcce6f4b3`, `074a70f17e316cd2`).
- **Oracle mode A, the rung:** the replay of the 10-17 `oracle01` (replayed from its archive
  `original/oracle/oracle01-rec-20261017-140428/`, since the install's file was re-recorded on 10-18 10:13 during the
  round; the harness got `--recording-src` / `--recording-as` for it) runs to tick 9601 / packet 4800 (ASan 0, 44-48 s,
  byte-identical twice, `cb865735cb8e84bb`, IDENTICAL to producer 1's audit run and to the first pass's
  `fc48b3123e8168f2`); against the GOG stream `oracle01-rec.hwo`
  it is **IDENTICAL in every simulation field through tick 176** (the tick-136 health divergence gone: decision 49 made);
  **first divergence tick 177, `ships_vel`, ship 35** (an sWorker's turn from tick 171 through `aitrackHeadingFunc`
  0x00418de0 / `vecNormalize` 0x00550770 with the exe's table `fsqrt` 0x00472860 on a 53-bit argument; the GOG velocity
  recovered by inverting the hash, the one fsqrt experiment within -1 / +4 / -2 ulps); per field ships_pos 180, univcheck
  180, resources 243, ships_misc 871, rng10 1408, bullets 1917, ships_health 1934, ships_n 2048; `rng6` 161 / `players`
  93 stock, `ships_rot` the DLL defect, excluded. The 10-18 recording (1,763 packets, 3,527 ticks; the unfixed DLL;
  `09c447a5f1bcd18f` twice): `real` off by one 1/16 step from tick 2 (a session clock artefact, excluded), then **tick 118
  `ships_pos` / `ships_vel`** (decision 60).
- **Builds:** `tools/build.sh --debug --clean` / `--release --clean` of `cd5866d`: `[319/319]`, 0 warnings (debug
  87,553,256 B, release 5,424,960 B); `tools/m2_layout32.sh` OK with the AI pins (`AITeamMove` 0x170, `AIEvents` 0x88,
  `AIPlayer` 0x1D18, `AISTeamEntry` 0x9D10; `Ship` 0x63C); `m2_save32_gen.py --check` current (108 layouts).
- **No `CAT-TODO` / `CAT-PROVISIONAL` in `src/` names an M2-R6 or M2-R7 owner, no "stage 4" tag left**: 73 owner tags on
  82 lines, stage 6 47 (tutorial 12, KAS 12, level load / KAS 1, single player 9, level load 3, plain 10), stage 5 26
  (recorder 4, LAN 5, multiplayer options 7, multiplayer 1, plain 9); 9 `CAT-PROVISIONAL` (the ninth: the merge's identity
  start-position table, decision 68).
- UBSan kinds left (all non-fatal): the ETG / particle opcode kinds, `BTG.c:388` / `BTG.c:864`, `stb_image_write.h:1263`
  (sp), `Select.c:1166` (the 10-17 replay), `Subtitle.c:1144` (the nis mode); the skirmish runs show only the Mission01
  kinds. Gone: `AIShip.c:1467`.

| item | report | result | audit | commit |
|---|---|---|---|---|
| oracle-rung-aiship | [docs/m2-r7-oracle-rung-aiship.md](../docs/m2-r7-oracle-rung-aiship.md) | the x87 single rounding of `cat_clExternalBuildUpdate` 0x0044dfc0 (decision 49 made), the replay IDENTICAL through 176, tick 177 `ships_vel` ship 35 localised with the GOG velocity inverted from the hash, the one fsqrt experiment (`/oracleTableFsqrt`, default off); `AIShip.c` (`aishipFlyToPointAvoidingObjsFunc` in the exe's order, the missile / mine bodies, the `AIShip.c:1467` UBSan fixed), `AIUtilities.c` (85 + `cat_aiuWrapCapture`; the `aiuWrap*` on `cat_shipIsTargetableBy`, `aiuAttack` without `aiuFindPriorityTarget`), `AITrack.c` (GoK's gimbal reset removed), `AIVar.c`, `aimProcessDock`; 22 IDENTICAL found modified, 139 reviews | 0.93, PASS-WITH-FIXES (two cited addresses, the ulps sign, one statement) | a3cc7f4 |
| ai-orders-moves | [docs/m2-r7-ai-orders-moves.md](../docs/m2-r7-ai-orders-moves.md) | the 22 orders rewritten with the exe's per-race tables (`cat_statsRandomAttackShipStatic` 0x00524d70), the 14 stand-ins gone, the events (12 slots, no fuel), the handlers (the exe's 20-entry table), the moves (no P2 swarm, the exe's numbers, Aggressive defaults), the attack / defense managers; `AITeamMove` 0x170; **`/catAiTest` + mode `ai`**; 48 IDENTICAL found modified, 92 reviews | 0.98, PASS-WITH-FIXES (two missing reviews, a stale comment) | 1410c22 |
| ai-player-skirmish | [docs/m2-r7-ai-player-skirmish.md](../docs/m2-r7-ai-player-skirmish.md) | **new `AISkirmish.c`** (the 17 `cat_ais*` + `aisRequestShip` / `aisFleetUpdate`, 34 teams, 8 timers, the tables), `AIPlayer` 0x1D18 with the embedded armada, **new `KASVar.c`**, the AI save chunks through SaveCat32 (108 layouts), `AIFleetMan.c` / `AIResourceMan.c` at the exe's, the Dock / Salvage / UnivUpdate AI counters, `AITeam.c` (the pointer message queue); the ai test's build / harvest rows and skirmish leg (16 checks); the 11 stage-4 tags closed; 18 IDENTICAL found modified, 45 reviews | 0.932, PASS-WITH-FIXES (an `aiaClose` review, the KAS lookup return, a naming claim, layout32 coverage, a comment) | 9b34dda |
| r6-leftovers | [docs/m2-r7-r6-leftovers.md](../docs/m2-r7-r6-leftovers.md) | `rndShamelessPlug` / `rndLoadShamelessPlug`, the waypoint cursor 0x004e6e70 with its stream-0 draws (the hud test's leg), the pie line callback 0x004e6530 + `cat_pingListGet`, `selShipUnderMouse`'s FOW test, the `mrHold*` stand-ins retired, the ETG review of all 47 `etgEffectCreate` sites (`kasfSpawnEffect`; the audit's `startRepairEffect` cap), the motion-interp follow-ups (run-only switches, clamped objects / droids against a blended host), HW1's blob render removed, the Memory.c tag settled; 13 reviews | 0.9375, PASS-WITH-FIXES (the 150.0 cap, a sentence, an address) | 3e33fee |
| skirmish-start (the user's session's branch, merged after the round's audits; not a workflow row) | [docs/skirmish-start-fix.md](../docs/skirmish-start-fix.md) | `scriptSetMissionSphereCB` 0x004a2420 / `scriptPreMissionSphereCB` 0x004a2820 from the exe (the start-position table, absent players' spheres skipped, the ownerless sphere to player 8, the Hive Frigate gift), `scriptSetShipsToBeNeeded`'s -1 lookup, `catStartPositions` 0..7 with no shuffle (`CAT-PROVISIONAL`, decision 68), `mgSkirmish`'s one computer opponent, `dbgFatal` to stderr + the fatal-signal line in release (`HWI-FIX`), the skirmish self-test (`/catSkirmishTest`, harness mode `skirmish`, decision 69); 4 reviews + 2 notes, merged into the map by the integrate step | no audit (the branch's own gates: skirmish PASS on both maps in debug and release, the mission stream IDENTICAL to the base's; re-run by the integrate step) | b64a768, merge 49823fa |

Round size (`git diff 4c31f6e..cd5866d`, the merge included): 98 tracked files, +12,198 / −6,496 (plus this integrate
pass's edits); `src/` 80 files, +7,479 / −6,006 with 532 added lines carrying `CAT`; 7 new `src/` files, 0 deleted. Delta
map: IDENTICAL / MODIFIED / REWRITTEN went from 2,942 / 1,394 / 84 (2,816 reviewed) to **2,853 / 1,483 / 84 (3,036
reviewed)**; 89 IDENTICAL bodies were found modified (87 stage 4, 1 stage 2, 1 stage 6: the merge's
`scriptPreMissionSphereCB`). Stage 4 is now **299 / 178 / 2 (77,515 + 169 B), 0 unread, + 23 cat-new all defined** (R6:
386 / 91 / 2, 60 unread, 2 defined); stage 6 343 / 125 / 26 (104 unread, 27,781 B). The tree carries 137 `CAT-FIX` (R6:
121), 82 lines with `CAT-TODO` (73 owner tags: stage 5 26, stage 6 47), 9 `CAT-PROVISIONAL`, 441
`HWI-FEATURE(motion-interp)`, 67 `HWI-TEST`, 21 `HWI-FIX`, 4 `HWI-EXPERIMENT`. The harness has 26 modes (sp, fe, statics,
meshes, textures, oracle, etg, mission, dock, command, render, sound, hud, ui, fescreens, strings, keys, console, build,
nisparse, motion, nis, toggle, replay, ai, skirmish). 316 harness runs this round (runs.txt lines 1074-1389): the
producers' and auditors' 214, the integrate step's first pass 46, the merge's checks 7, the integrate step's second pass
49.

### M2-R8: COMPLETE 2026-10-19 (the oracle's rung: the x87 vector / heading / rotation helpers and the soak faults; stage 6 part A: the kasf library, the KAS runtime and kas2c dialect, Mission01 + Tutorial1 from the NIS-first start, the start-position shuffle, single player and the tutorial)

Summary, exit test and the M2-R9 plan: [docs/m2-r8-summary.md](../docs/m2-r8-summary.md). Workflow
`cycle/workflows/m2-r8.js` (run id `wf_74bcf42e-330`, `ROUND_BASE 145352a`, launched with `FSQRT_DEFAULT 'sqrtf'`), run
from the user's terminal session 38027cce (Fable), not from the orchestrator; 9 agents (four ports in sequence: the rung,
the kasf library, the KAS runtime + Mission01 / Tutorial1, single player + the tutorial; four audits, each checkpointing
its port; integrate). One producer was cut by the five-hour limit (the kasf library, WIP snapshot `e3f05f4`) and continued
from its tree. The R8 plan as launched is in docs/m2-r7-summary.md "Plan for M2-R8"; it replaced the table that stood here.

**R8 exit test (the stage-6 part A exit): MET** (the integrate step, clean builds of `3e55019`, plain harness, silent, 62
runs `r8i-*` in five parallel chains; `runs.txt` lines 1687-1748). Debug `hwi-engine` (ASan + UBSan):
- **Mission01 headless:** two 6,000-tick runs (`r8i-mission-a/b`, 28-31 s) byte-identical (1,476,370 B, 6,001 `T`
  records, 154 ships), **ASan 0**, sha256 **`0d2b76d565105a26`**; the perturbed run caught at tick 3000 / `rng10` and the
  flipped `ships_health` bit at 4000; the release pair `3dc687dbb95feab6` IDENTICAL to debug with its own perturb / flip
  caught (`MISSION TEST PASS` twice). **Against R7's `9f7416bb64fec98f` (R6's `49240abcf9f0ed97` in content) exactly two
  things moved: `rng0` 40 draws ahead from tick 0 (the start-position shuffle, decisions 40 / 68) and `ships_rot` from tick
  1 (the rung's x87 helpers); every other field IDENTICAL over 6,001 ticks** (decision 44). The chain: `9f7416bb` -> the
  rung `245b3d88387ed076` (`ships_rot`) -> the kasf library `b97180de748702e5` (same content) -> the KAS runtime + shuffle
  `19560d16033efd53` (`rng0` +40) -> single player `00ae16e7f73689d9` (same content) -> `0d2b76d565105a26` (same content).
  The table square root (`-- /oracleTableFsqrt`) leaves this stream byte-identical.
- **Mission01 from the NIS-first start:** the new `missiontest` self-test (`/catMissionTest`, harness mode `missiontest`)
  plays the opening NIS (ticks 0-1771), loads Mission01, starts the generated kas2c script, plays the nislet, hyperspaces
  the fleet in and reaches the **six objectives in order** (DestroyBombers 4768 / 4800, ExploreRegion 6864 / 8096,
  DestroyResourcers 8208 / 8752, DestroyProxAttackers 9760 / 10240, DestroyDestroyer 10816 / 11296, Hyperspace 11328) and
  the jump: `done ticks=11424 objectives=6/6 result=PASS`, two runs byte-identical (`becb49b579aba435`, IDENTICAL to the
  last audit's), ASan 0; the Tutorial1 leg PASSes (three prompts, the objective pointer, Next -> Lesson2); the new
  `loadgame` self-test loads the user's "01 - Hiigara" (682 chunks, 167 ships, 36 KAS teams) twice byte-identical
  (`68b0075ad1e20bb7`). **sp** (the NIS-first start): universe running, **4,496 ticks** at 340 s (the gate 1,248), the NIS
  then `Mission01/Mission01.mission` in FileLoads, ASan 0, no leak. **fe:** `Main_game_screen`, ASan 0, 0 UBSan reports.
- **Self-tests:** rc 0, ASan 0: statics 143/145 (108 StaticInits), meshes 1,758/1,758, textures 10,661 / 0 not loaded,
  oracle 0 sync errors, etg 618 effects (R6's numbers); release oracle `SELFTEST PASS`. The twenty scripted self-tests all
  `result=PASS`: dock, command, render (`droids=2500`), sound, hud (`draws=181 randoms=606`), ui, fescreens (195 screens,
  0 `fescreen` UBSan), strings (1,705 / 1,705), keys, console (**`kas=6 tablerows=486`**: the exe's KAS table through the
  generated dispatcher), build (`b7851d51d1c21bfb` twice), nisparse (14 NIS, 18 animatics), toggle x3, **ai** (16/16,
  `df95f21bdb82abbf` twice; the 4,000-tick soak PASSes with ASan 0 and no fatal: decision 67 closed), **skirmish**
  (LiveFireExercise2 `1b648d098a7659c5` twice, Armed8 `898f68a5fc352ba2` with 324 resources: the meteor storms), missiontest
  and loadgame (above).
- **Motion-interpolation guard-rails:** the three 6,000-tick debug streams with the flags on, `/noInterp` and
  `/noNISInterp` are byte-identical (`0d2b76d565105a26`); **the `/noInterp` dumps moved with the rung's shared vector /
  matrix helpers** (every frame goes through them): `0838a011db819920` / `d1e6f6e477906b52` (the rung audit's exactly;
  against the pre-feature `b2af0d6decdf6474` / `3f04c837e605ba23` only the frame CRC of 34 of 1,431 motion frames and the
  NIS objects' screen positions differ), **re-baselined**; the flags-on dumps are now `7e4f12d35a662197` /
  `2f18ba93741ebdc6` (R6 / R7: `d9a8661fcce6f4b3` / `074a70f17e316cd2`). Decision 46's guard-rails themselves untouched.
- **Oracle mode A, the rung:** both recordings replayed twice from the clean build (byte-identical pairs, ASan 0, IDENTICAL
  to the rung audit's runs). **With `/oracleTableFsqrt` the 10-17 replay is IDENTICAL in every simulation field through
  tick 246; first divergence tick 247 `ships_vel`, ship 40** (a bRecon guarding the bMothership: the coordsys after its
  turn at 245-246 or the computed UP thrust; GOG `(420767c8, 4300eec6, 429fe606)`, ours (-2, +1, -2) ulps); per field
  ships_pos 259, univcheck 315, ships_misc 1593, resources 1944, bullets 2005, rng10 2043, ships_health 2043, ships_n 5559.
  **The 10-18 replay through tick 154; first divergence 155 `ships_vel`, ship 28** (an sWorker's v.z -1 ulp after a hard
  turn); per field ships_pos 179, univcheck 216. With the round's `sqrtf` default the first divergences stay at **177** (ship
  35: the square root itself) and **118**. Every number equals docs/m2-r8-oracle-rung-physics.md and its audit. The install's
  `oracle01` was re-recorded a third time (10-19, 266 ticks, the unfixed DLL; replayed for information: 221 with the table,
  179 without); `ships_rot` stays excluded in every session (decision 48: `hook_sites.h` still 0x80).
- **Builds:** `tools/build.sh --debug --clean` / `--release --clean` of `3e55019`: `[253/253]` (R7: 319; HW1's compiled
  missions left the build, 9 new files in), 0 warnings (debug 81,645,224 B, release 4,523,768 B); `tools/m2_layout32.sh` OK
  (`Ship` 0x63C; the round's pins `AITeam` 0x280, `ResourceVolume` 0xB0, `nebChunk` 0x58, `nebTendril` 0xFC, `tutpointer`
  0x68, `CatSpWarpLevel` 0x2F0); `m2_save32_gen.py --check` (119 layouts), `m2_kas2c_gen.py --check` (149 bodies verified)
  and `m2_kasf_table_gen.py --dispatch --check` current.
- **No `CAT-TODO` / `CAT-PROVISIONAL` in `src/` names an M2-R7 or M2-R8 owner; no "stage 4" or "stage 6" tag left;
  `CAT_HW1_MISSION_SCRIPTS` gone (decision 14 retired)**: 38 lines carry `CAT-TODO` (R7: 82), 29 owner tags: stage 5 26
  (plain 9, multiplayer options 7, LAN 5, recorder 4, multiplayer 1), M2-R9 part B 2 (missions 02-17's generated code; the
  "Mine" / "Slipgate" sphere readers), M2-R9 physics 1 (`hyperspaceCost`'s double return); 5 `CAT-PROVISIONAL` (R7: 9).
- UBSan kinds left (all non-fatal): the ETG opcode / `struct particle` kinds, `BTG.c:388` / `:391` / `:864`,
  `stb_image_write.h:1263` (sp), `Subtitle.c:1144` (the nis mode, and now sp and the mission test: n01 / nislet01's subtitle
  theme index). Gone: `Select.c:1166`, `Tutor.c:1958`.

| item | report | result | audit | commit |
|---|---|---|---|---|
| oracle-rung-physics | [docs/m2-r8-oracle-rung-physics.md](../docs/m2-r8-oracle-rung-physics.md) | the permanent `/oracleShipDump` + `tools/oracle/hwo_invert.py`; the exe's x87 shape of the flight path (Vector.c, Matrix.c, AITrack.c, Physics.c, AIShip.c, GenericInterceptor.c, Formation.c, CommandLayer.c, Collision.c, UnivUpdate.c: 53-bit intermediates, one rounding per store), `collCheckShipShipColl`'s clamped-host test, the killer-asteroid wrap; the rung 177 -> 247 (10-17) and 118 -> 155 (10-18) with the table switch, 179 / 180 / 183 / 243 fixed on the way, the next divergences localised (ship 40 / ship 28: the coordsys after a hard turn); the soak faults fixed (ten ETG parameters, the owner compare, the AISkirmish table read); the mission stream `245b3d88387ed076` (`ships_rot` only); 25 IDENTICAL found modified, 50 reviews | 0.970, PASS-WITH-FIXES (a report overstatement; `hyperspaceCost`'s float return re-targeted to M2-R9 physics) | 15df4d6 |
| kasf-library (ran second) | [docs/m2-r8-kasf-library.md](../docs/m2-r8-kasf-library.md) | the exe's 486-row kas2c table 0x00755048 and dispatcher 0x0045b9a0 **generated** by `tools/m2_kasf_table_gen.py` (KASFuncTable.c/.h, KASFuncDispatch.c), the kasf with the exe's hash signatures, the 184 Cataclysm-only kasf at the exe's, 30 KASFunc.c IDENTICAL found modified, KAS names as hashes everywhere (KAS.c, Timer.c, AITeam.c pinned 0x280, LevelLoad.c, NIS.c), the KAS variable store's callers, `cat_consoleKasCommand` (the console self-test's `kas=6 tablerows=486`), Objectives.c on the info overlay, the task bar removed (decision 41), the letterbox, the 32 UI sound ids (decision 65), HW1's compiled missions out of the build; 40 IDENTICAL found modified, 229 reviews | 0.92, PASS-WITH-FIXES (`kasfShipsSort`'s x87 shape, the letterbox 1/30, 3 reviews, a tag, a comment, 2 statements) | 7d225de |
| kas-runtime-mission01 (ran third) | [docs/m2-r8-kas-runtime-mission01.md](../docs/m2-r8-kas-runtime-mission01.md) | KAS.c's runtime bodies (`cat_kasShipRemovedFromSelections`, kasSave / kasLoad, the function converters by exe address), **`tools/m2_kas2c_gen.py`** (the generated Mission01 84 + Tutorial1 59 + 6 shared functions, every body verified against the machine code, 0 hand edits), LevelLoad.c (`scriptSetShipsToBeNeeded`, `levelPreInit`, the meteor storm with `ResourceVolume` 0xB0 and the wrap box's writer, the nebula save records, the mission-number mapping, `MISSION_17`), **the NIS-first start** (`singlePlayerStartGame` / `StartNis` / `NisStoppedCB` / `LoadNewLevel`, `HYPERSPACE_LOADDONE` -> `kasMissionStart`, states 9 / 10), **the start-position shuffle** (40 stream-0 draws), `CAT_HW1_MISSION_SCRIPTS` retired, **`/catMissionTest` + mode `missiontest`**, `/catMissionTestTutorial`; the mission stream `19560d16033efd53` (`rng0` +40 from tick 0, nothing else); 3 found modified, 46 reviews | 0.88, PASS-WITH-FIXES (the save converters for Tutorial1's functions, the meteor storm's texts, 2 reviews, the "Mine" / "Slipgate" tag, a count, a comment) | 3903f02 |
| singleplayer-tutorial | [docs/m2-r8-singleplayer-tutorial.md](../docs/m2-r8-singleplayer-tutorial.md) | the hyperspace flow at the exe's (the roll call, `cat_spEmergencyHyperspaceCB`, `UpdateArrivingShip`'s x87 arrival and motion-cue reset, the ether), the mission flow (no mission sequence, `Warp.script` and `cat_spWarpToLevel`, `spHyperspaceButtonPushed`, the rest of `singlePlayerGameUpdate`: the letterbox jump, the white-out and animatic, the at-once arrival, case 10; the fades in `mrRegionDraw`), the difficulty (`cat_spGetDifficultyModifiers`, `cat_spGameDifficulty`, the KAS skill level = the difficulty word), a cataclysm.exe save's mission tail through 8 new SaveCat32 roots + **the `loadgame` self-test**, Tutor.c (the 54 flags in the exe's layout, the pointers, Cataclysm's five buttons, the managers' hide / restore, save / load) + the Tutorial1 leg; 12 found modified, 98 reviews, 25 stage-6 tags closed | 0.90, PASS-WITH-FIXES (the lost `"Attack"` flag name, the buttons' region size, 2 report counts, a stream name, a comment) | 3e55019 |

Round size (`git diff --stat 145352a..3e55019`): 113 tracked files, +25,810 / -5,417; `src/` 94 files, +19,221 / -4,658 with
937 added lines carrying `CAT`; 9 new `src/` files (the generated KAS table / dispatcher, the generated missions, the
mission self-test), 0 deleted. Delta map: IDENTICAL / MODIFIED / REWRITTEN went from 2,853 / 1,483 / 84 (3,036 reviewed) to
**2,773 / 1,564 / 83 (3,340 reviewed)**; 80 IDENTICAL bodies were found modified (stage 2 21, stage 4 4, stage 5 1, stage 6
54). Stage 6 is now **289 / 180 / 25 (52,980 + 1,505 B), 0 unread in KAS.c / KASFunc.c / Timer.c / LevelLoad.c /
Objectives.c / SinglePlayer.c / Tutor.c** (R7: 104 unread) + 2,365 cat-new (2,085 kas2c mission functions, 149 generated;
276 of the 280 others defined, R7: 45). The tree carries 163 `CAT-FIX` (R7: 137), 38 lines with `CAT-TODO` (82), 5
`CAT-PROVISIONAL` (9), 441 `HWI-FEATURE(motion-interp)`, 94 `HWI-TEST` (67), 41 `HWI-ORACLE` (22), 21 `HWI-FIX`, 4
`HWI-EXPERIMENT`. The harness has 28 modes (26 + `missiontest`, `loadgame`). 359 harness runs this round (runs.txt lines
1390-1748): the producers' and auditors' 297, the integrate step's 62.

### M2-R9: COMPLETE 2026-10-20 (the oracle's next rung: the exe's table `fsqrt` as the default, its CRT `qsort`, the collision blobs' and the flight's remaining x87 shape; stage 6 part B: missions 02-17 generated with per-mission self-tests, the sphere readers, the later missions' flows, the game's end, the warp start, the Tutorial1 lessons; the recorder's record side (stage 5 reduced); the test / tooling leftovers)

Summary, exit test and the M2-R10 plan: [docs/m2-r9-summary.md](../docs/m2-r9-summary.md). Workflow
`cycle/workflows/m2-r9.js` (run id `wf_c0cd6893-248`, `ROUND_BASE 54dcbb2`, `FSQRT_DEFAULT 'table'`, `STAGE5 'recorder'`,
`FOURTH_RECORDING null`), run from the user's terminal session 38027cce (Fable), not from the orchestrator; 9 agents (four
ports in sequence: the rung, missions 02-09, missions 10-17, the recorder's record side + the test / tooling leftovers; four
audits, each checkpointing its port: `64f49a9` 0.91, `307c3a5` 0.94, `9c1801f` 0.97, `9574c9b` 0.91, all PASS-WITH-FIXES;
integrate). No producer was cut off. The R9 plan as launched is in docs/m2-r8-summary.md "Plan for M2-R9"; it replaced the
table that stood here.

**R9 exit test (the stage-6 exit): MET** (the integrate step, clean builds of `9574c9b` (`[270/270]`, 0 warnings), plain
harness, silent, 102 runs `r9i-*` in five parallel chains + 3 bisect runs; `runs.txt` lines 2296-2400; ASan 0 in all 105):
- **Mission01 headless**: pairs byte-identical, debug **`553845b65e87f3b5`** / release `ba8ee6e78597c175` (IDENTICAL to
  each other), MISSION TEST PASS in both (perturb at 3000 `rng10`, flip at 4000); `/noInterp`, `/noNISInterp` and
  `/oracleLibmSqrt` byte-identical to A; IDENTICAL to every producer's and audit's Mission01 run of the round; **against
  R8's `0d2b76d565105a26`: `ships_pos` / `univcheck` from tick 0, `ships_rot` from tick 1, nothing else** (producer 1's
  statement: the exe's `qsort` at `formationArrangeOptimum`'s ties) — **RE-BASELINED** (decision 44).
- **The rung**: 10-17 IDENTICAL through 1921, first divergent **1922 `ships_vel`** (ship 41); 10-18 through 1720, **1721
  `bullets`**; with `/oracleLibmSqrt` 177 / 118; the release replay equal to the debug on the simulation list; both
  replays IDENTICAL in every field to the rung audit's and producer 4's (decision 49 updated).
- **Missions 02-17**: 16 / 16 PASS with every objective (and Mission01 6/6), 15 pairs byte-identical, Mission06's pair
  equal in every game field and different in `rng_all` only (decision 94); every stream IDENTICAL to producer 4's and the
  audits'; **Mission17 ends the game** (`game end tick=15584`, `Credits-GameEnd.cred`); the Tutorial1 leg 12 / 12 as a
  pair (`416f819eaa29e3e0`, 10,444 ticks); the warp start (level 10) a pair; "LESSON 11" loads (a pair).
- **The recorder**: `grep` 0 for the recorder tag; `hwirec01` / `hwirec02` replay as pairs IDENTICAL to their record runs
  and to the audit's playbacks; this build's own LFE2 recording replays IDENTICAL to its record run and to the producer's
  (the save differs in stale pointer slots only, the `.pkts` in a header padding word in 1,200 packets: decision 95);
  **mode B (the GOG playback) not run** (no `original/oracle/hwirec*-play-*`): the user's step with the commands in
  docs/m2-r9-summary.md "What the user must decide" 4.
- **Self-tests** all PASS (statics x2, meshes, textures, oracle, release SELFTEST PASS, etg, dock, command, render,
  sound, hud, ui, fescreens 22/22, strings 1,705/1,705, keys, console tablerows=486, build pair, nisparse, toggle x3,
  ai pair + 4,000-tick soak, skirmish LFE2 pair (91 ships) + Armed8, missiontest pair, loadgame pair, motion / nis dumps);
  **sp** `--timeout 340` walks Mission01_NIS01 -> 01 -> 02 -> 03 -> 04_NIS02 -> 04 (39 screenshots) and stops on
  `texreg.c (569)` `nPalettes < 8` (ASan 0; M2-R10 render, decision 90); **fe** Main_game_screen.
- **Guard rails vs R8**: ai / soak / build / missiontest / loadgame / Armed8 moved exactly as producer 1 stated (the
  table + the qsort), LFE2 at tick 0 by the Slipgate reader (producer 2); the **ai soak moved once more at tick 694
  `ships_health`** by producer 3's ship-part blast fix (`MakeExplosionRockBumpables`), unstated by anyone and bisected by
  this step (three extra builds + soaks); the `/noInterp` dumps re-baselined to `183681235a3fdc7b` / `13ca70eea998e5de`
  (decision 46); the generators `--check` current; layout32 OK.
- **Tags**: `grep -rn -E 'CAT-(TODO|PROVISIONAL)\([^)]*(M2-R[89]|stage 6)' src/` empty; 34 `CAT-TODO` lines, 25 owner
  tags (stage 5 23: plain 8, multiplayer options 8, LAN 6, multiplayer 1; M2-R10 physics 2), 5 `CAT-PROVISIONAL`
  (count with `/bin/grep`: the shell's ugrep wrapper skips `MultiplayerGame.c` as binary).
- **UBSan kinds** (all non-fatal): the ETG / particle alignment kinds and `BTG.c` as in R8; new: `Select.c:1153` /
  `1154` (sp + five mission tests), `FOW.c:296-437` (Mission17 only), the recorder's packet writes in `CommandNetwork.c` /
  `Queue.c` (the record run); gone: `Subtitle.c:1144`.

Delta map after the round: IDENTICAL 2,773 / MODIFIED 1,564 / REWRITTEN 83 (the same totals as R8 by coincidence: 12
IDENTICAL rows found modified, 15 unflagged rows found IDENTICAL), 3,512 reviewed rows; stage 6 complete on the map:
**the 2,085 kas2c mission functions all generated and defined** (decision 87). 34 lines carry `CAT-TODO` (R8: 38).

**M2-R10 plan (a short round: the play path's leftovers), then M3** (docs/m2-r9-summary.md "Plan for M2-R10"; M3's Linux
play-test of Mission01 .. 03, the tutorial and a skirmish can start now with `tools/play.sh`, the brief in the summary):

NOT YET LAUNCHED; `cycle/workflows/m2-r10.js` is WRITTEN (74816ff, an Opus draft reviewed by the user's session 38027cce: 3 producers + 3 auditors + integrate, `ROUND_BASE 29f7c0d`, the R9 baseline 553845b65e87f3b5, constants `FOURTH_RECORDING null` and `MODEB_RESULT null`, each set to the user's archive path if the fourth recording / the GOG playback of hwirec01-02 exist before the launch). Launch = Workflow({scriptPath: cycle/workflows/m2-r10.js}) from the user's terminal session on the user's word, never the orchestrator; relaunch after a cut-off with resumeFromRunId from that session.

| # | producer | scope | done when |
|---|---|---|---|
| 1 | the oracle's next rung, physics | 10-17 tick 1922 (ship 41's attack flight: the target lead / the turning ship's coordsys), 10-18 tick 1721 (the first bullets: `GenericInterceptorCanFire`, `cat_gunCanFire`, the attack AI's timing), `aiuFindDistanceSquared`'s and `tacticsGetShipsMaxVelocity`'s unrounded returns, the rest of Blobs.c; the mode-B result and the fourth recording if the user made them | the 10-17 replay IDENTICAL beyond 1922 and the 10-18 beyond 1721, or the next divergences localised and owned; a mode-B divergence localised |
| 2 | the render stop, the animatics and the sp gate | `trTextureRegister` 0x00585760's colour block (`trColorsIndexAlloc`'s `nPalettes < 8`), `animBinkPlay` -> `animAviPlay` 0x00421860 at its 3 call sites (skipped by the headless tests), `Select.c:1153` / `1154`, `FOW.c:296-437`, Mission17's `memAllocFunctionA` / `is_final_tex` notes | the `sp` loop runs through Mission04 and on to its timeout with ASan 0; an animatic plays in an attended run; no `Select.c` / `FOW.c` report |
| 3 | the tests and the oracle's leftovers | the mission self-test's combat, a deliberate-loss test for the failed-jump return, the mission runs on the virtual audio clock (Mission06's `rng_all`), the skirmish test's bigotry option, the `.pkts` header tail word, `scriptSetTweakableGlobals` and the 148 unflagged rows sampled | Mission06 pairs byte-identical in every field over 4 runs; the failed-jump flow reached; the 17 mission streams re-baselined once with the stated fields |

**R10 integrate**: the R9 gates hold from a clean build (the mission streams moving only by producer 3's stated `rng_all`),
the `sp` run through Mission04 and beyond with ASan 0, the rung beyond 1922 / 1721 or owned, no `CAT-TODO` naming M2-R9 or
M2-R10, the generators `--check` current. **Then M3**: the play-test sign-off (the campaign from New Game as far as it goes,
the tutorial, a skirmish, with `tools/play.sh`), the one-hour 4K@144 soak (no gamescope), the Windows 11 build, and the
stage-5 remainder per decision 9 after M3.

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
   wire format is not broken gratuitously. **M2-R9: applied as "recorder"** (stage 5 reduced to the recorder's record
   side, done: hwi-engine records in cataclysm.exe's own format, decision 95; the remaining 23 `CAT-TODO(stage 5 ...)`
   sites stay deferred to after M3).
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

6. ANSWERED 2026-10-17: **the first reference recording is `oracle01`** (a 10-minute skirmish, the user as Somtaaw
   against "Computer 1", recorded through the Record shortcut; docs/m2-r2-oracle.md "2026-10-17 14:44: the reference
   recording is VERIFIED"). M2-R6 oracle-mode-a replays it in hwi-engine: first divergence tick 136 `ships_health`
   (`cat_clExternalBuildUpdate` 0x0044dfc0), the M2-R7 rung.
10. ANSWERED 2026-10-17: **the first live oracle session was run** (two live sessions and the replay verification):
    the DLL loads under Proton, the H6 trigger works, `vqpc` gives 1/16 s per tick; the GOG record and playback streams
    are in `original/oracle/`. One DLL defect found by mode A (`CAT_SHIP_coordsys` 0x80 -> 0x98, decision 48) waits for
    the user's next recording session.

**Still open (the user's; none blocks M2-R8):**
8. **Two saves** from the M0 play-test (one mission, one skirmish), for `tools/formats/savegame_probe.py` and
   `saveCatLoadHook`. Since M2-R6 the loader reads a real multiplayer save (`oracle01`) and the probe reads the user's
   Mission 1 save; what stays code-derived is a single-player save's mission tail (stage 6).
- The M0 play-test sign-off (docs/PLAN.md).
- A second recording with the fixed oracle DLL (decision 48), so that `ships_rot` becomes comparable; not needed for the
  M2-R7 rung.

**New from M2-R3** (each has a default in the tree; say if you want otherwise):
13. **LeakSanitizer suppressions.** `tools/lsan.supp` suppresses 11 start-up allocations: libdbus, the GL driver,
    GoK's SDL mutexes and one `rinit` mode record. Every other leak still fails a run. The entries are dropped when
    stages 3 and 5 port those paths.
14. **`CAT_HW1_MISSION_SCRIPTS 0` (CAT-PROVISIONAL).** No HW1 KAS mission script and no HW1 mission-start NIS until
    stage 6 ports Cataclysm's kas2c missions and its NIS-first start. Without the switch, sp stops at `KAS.c (265)`. **RETIRED in M2-R8**: the switch is gone; Cataclysm's generated kas2c
    missions (Mission01, Tutorial1 so far) and the exe's NIS-first start replaced HW1's, whose compiled missions are out of
    the build.
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
    oracle baseline, so it is yours to call; the reference recording will show it first. **M2-R8**: still `sqrtf`; with the table the 10-17
    replay matches GOG through tick 246 (through 176 without it): decisions 56 / 70. **M2-R9: the table is the default**
    (decision 70 answered and applied: `catFsqrtTable` TRUE, `/oracleLibmSqrt` the A/B switch); the rung reads 1922 / 1721
    with it, 177 / 118 without.
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

**New from M2-R6** (each has a default in the tree; say if you want otherwise; details in docs/m2-r6-summary.md
"Decisions"):
35. **The waypoint draw 0x004e6e70** draws its jitter from `ranRandomFn(0)` at draw time (stream-0 randoms consumed
    while the waypoint UI is active): not ported, M2-R7 producer 4. Default: port it as the exe does and state the
    stream effect.
36. **`mrHoldRight` / `mrHoldLeft`** stay as stand-ins next to Cataclysm's state words while other modules test them.
37. **The `fescreens` self-test** runs the in-game screens with no-op stand-in callbacks (their own need a running game
    and fault out of context, as in the exe); the front-end screens run the game's callbacks.
38. **The options file is `Cataclysm.cfg`** (the exe's name and 94-row table), read from `$HOME/.homeworld/` (GoK's
    config dir) and written to the user settings path; the harness's home template holds it (`TutorialNeeded 0`,
    `PlayIntros 0`).
39. **Effects preset 0 at start-up** (the exe's `opUpdateSettings`) turns the dynamic CPV, lens flares and repair droids
    on by default (`.bss` 0 in the exe). The Mission01 stream is unchanged by it.
40. **The start-position shuffle** of `gameStart` (40 stream-0 draws per new game, Mission01 included) is re-targeted to
    stage 6 level load with its readers; it will move the mission stream's `rng0` when it lands. **LANDED in M2-R8** (the KAS
    runtime port): the 40 draws move `rng0` from tick 0 (decision 44); the identity table of decision 68 is gone.
41. **Cataclysm has no task bar**; HW1's stays until stage 6 removes its callers.
42. **HW1's blob render** stays uncalled in Sensors.c (the exe has none); cleanup later.
43. **The FOW grid belongs to the local player only** (`cat_fowCreatePlayerGrids` compares with `curPlayer`); computer
    players see every ship not fully cloaked and undetected; unvisited cells hide the sensors scene's resources / mines.
44. **The mission-stream baseline moved in managers**: `rng6` (10 draws ahead at tick 1) and `rng_all` only, the exe's
    computer-player per-ship timer (`cat_shipUpdateExternalBuildAndAI` 0x00543f10). Default: the new baseline
    `49240abcf9f0ed97` (the integrate step's build) is accepted; the droid / motion-cue draws did not appear in the
    headless run (no damaged Sect ship in play), so no second move this round. **M2-R7: the content is unchanged** (the
    AI ports do not reach Mission01's computer player without KAS fleet control, decision 57; the skirmish-start merge's
    start-position table draws nothing, decision 68); the R7 build's hash is `9f7416bb64fec98f` (the first pass's
    `39a512d0465d73a6`), header build identity only. **M2-R8: the baseline is `0d2b76d565105a26`** (the integrate build;
    release `3dc687dbb95feab6`, the same content), which differs from `9f7416bb64fec98f` in exactly `rng0` (40 draws ahead
    from tick 0, the shuffle) and `ships_rot` (from tick 1, the rung's helpers); nothing else moved. The chain of the round's
    hashes is in docs/m2-r8-summary.md. **M2-R9: the baseline is `553845b65e87f3b5`** (the integrate build; release
    `ba8ee6e78597c175`, the same content), which differs from `0d2b76d565105a26` in exactly `ships_pos` / `univcheck` from
    tick 0 and `ships_rot` from tick 1 (the exe's `qsort` at `formationArrangeOptimum`'s ties, producer 1); not by the table
    (the `/oracleLibmSqrt` run is byte-identical: no ship flies), not by the 16 missions, the recorder or the leftovers; the
    chain of the round's hashes (`1028375d20f15d75`, `be3e5030cebbea0b`, `dc2fb8ce71c0d26b`, `fd82fafbcd9011fc`: the same
    content, the header's build identity only) is in docs/m2-r9-summary.md.
45. **Cataclysm's 18 animatics are 30 fps**; GoK's player assumed 15 (every animatic played at half speed until R6).
46. **Motion interpolation**: `objInterpolate` / `nisInterpolate` both 1 (`Cataclysm.cfg` rows, `/noInterp`,
    `/noNISInterp`, console rows, the Smooth motion checkbox added in code to `Video_Options` and the in-game `Video`
    screen); a paused universe draws the simulation state (no blend); a camera cut is never blended across (the frames
    before it continue the pre-cut key); NIS objects are evaluated ahead of their last tick, non-curve objects between
    their last two; the render transforms are written into the simulation's fields for the draw and restored bit for
    bit (the spec asked for separate render fields); a switch's 0 persists into `Cataclysm.cfg` through the next
    `opOptionsSaveSettings` (a run-only override is M2-R7's follow-up). `tools/nis_smoothness.py` thresholds 0.05 /
    0.01 px. **M2-R9: untouched** (no motion-interpolation code changed); the `/noInterp` guard-rail dumps re-baselined
    once to `183681235a3fdc7b` / `13ca70eea998e5de` (the table square root under every frame's flight and the exe's qsort
    in the formations and the NIS's sorted paths: producer 1's stated physics); flags-on `428fff6c765ab81b` /
    `6a4c1850799187ac`.
47. **Oracle mode A's exclusions**: `ships_rot` (the DLL offset defect), `rng_all`, `fpcw`, `players` (the stock
    record / playback difference at tick 93; `rng6`'s at 161: the playback skips the AI). The comparison is the
    simulation field list over ticks 1..9600. **M2-R9**: unchanged (`rng6` at 161, `players` at 93 / 272 in the 10-18
    one); the rung's list is `rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,
    resources,univcheck,dt,total,real` minus the exclusions (`real` too for 10-18).
48. **The oracle DLL's `CAT_SHIP_coordsys` should be 0x98, not 0x80** (tools/oracle/dll/hook_sites.h); the DLL was not
    run or rebuilt this round. A one-line edit before the user's next recording. **M2-R9: still no fixed-DLL recording**
    (`FOURTH_RECORDING null`); `ships_rot` stays excluded in every session (it diverges at tick 1); both open divergences
    (1922: a turning ship's coordsys, 1721) would be helped by it: recommended before M2-R10's rung producer.
49. **The x87 single rounding of `cat_clExternalBuildUpdate`**: MADE in M2-R7 (`clExternalBuildAdvance`: the increment
    in double, rounded once at the store, the clamp on the double sum); the tick-136 divergence is gone and the oracle's
    first divergence is now **tick 177 `ships_vel`** (ship 35's turn from tick 171: the table `fsqrt` / x87 vector helpers,
    M2-R8's first item, decision 56) in the 10-17 recording and tick 118 `ships_pos` / `ships_vel` in the 10-18 one.
    **M2-R8**: 247 / 155 with the table switch. **M2-R9**: on the table default the first divergences are **1922 `ships_vel`**
    (10-17, ship 41's attack flight) and **1721 `bullets`** (10-18, GOG fires 19 ticks earlier); 177 / 118 with
    `/oracleLibmSqrt`; M2-R10 physics' first item (docs/m2-r9-summary.md "Oracle mode A").
50. **Release vs debug replay** differ in stream 8 (battle chatter) from tick 1952 (a debug-only speech path);
    simulation fields identical. The oracle compares simulation fields only.
51. **Latent 64-bit union aliasing** in `MCVSpec` and `P1StandardCorvetteSpec` (listed by `m2_save32_gen.py`): no owner
    yet.
52. **New non-fatal UBSan kinds**: `BTG.c:864`, `stb_image_write.h:1263` (sp), `Select.c:1166` (the replay),
    `Subtitle.c:1144` (the nis mode), `AIShip.c:1467` (dock, a stage-4 file). Listed, not required.
53. **The integrate step's three small ports** (`univKillPlayer`'s kill credit / manager closes / speech id 0x200102d,
    `univKillOtherPlayersIfDead`, the time-compression string) are grounded in the decompile and the disassembly and
    unaudited; the M2-R7 audits may re-check them.
54. **Chosen names** where the exe's are unknown: `catKillerIndex`, `catSnapToFocus` / `catScriptedDistance` (Camera
    +0x55 / +0x58), `CAT_SHIPCMN_ANIM_OPEN` / `CLOSE`, `CatFOWGrid` / `CatFOWCell`, `catPreviousAnim` /
    `catBindingSwapPending`, the droid entry fields, `catExternalShipAttached`, `cmDetermProgress_t`'s `catAttached` /
    `catShipStarted` / `catShip`, `SS2_CatConsole`, `catKeyNameStrings`, the appended `NEO_*` opcode names.
55. **Reviews go into `ghidra/delta_map_review.json`**: a note written into `delta_map.json` directly is reverted by the
    next `delta_map.py merge` (the integrate step restored 16 and added 107 such rows).

**New from M2-R7** (each has a default in the tree; say if you want otherwise; details in docs/m2-r7-summary.md
"Decisions" and "What the user must decide"):
56. **The table `fsqrt`** (decision 23 restated as M2-R8's first question): the default stays `sqrtf`; the experiment switch
    `/oracleTableFsqrt` (`FastMath.c`, HWI-EXPERIMENT) is compiled in, off by default (the default replay byte-identical to
    before it existed). Without the table and the x87 vector helpers the replay cannot match GOG at tick 177 (the one
    experiment came within -1 / +4 / -2 ulps). Recommendation: make it the default in M2-R8 (the mission baseline moves once). **M2-R8 result**: with the switch on
    the 10-17 replay is IDENTICAL through 246 (247 `ships_vel`, ship 40) and the 10-18 through 154 (155 `ships_vel`, ship 28);
    off: 177 / 118. The headless Mission01 baseline does not move with the table (measured). Still the user's: decision 70.
    **M2-R9**: superseded by decision 70's answer: the table is the default, `/oracleTableFsqrt` is gone.
57. **The Mission01 stream does not see the AI**: no fleet control without the KAS call (`CAT_HW1_MISSION_SCRIPTS 0`), so
    the four ports left the baseline's content unchanged; the `ai` self-test stream (`c6ff74db1a364512` in the R7 build; the
    first pass's `9d42fd414007fbe2` is the same content) is the AI's guard-rail. Default: no re-baseline with fleet control forced on; stage 6 moves the stream when it lands. **M2-R8**: stands;
    Mission01's generated script never calls `BuildControl` (the kasf that sets `giveComputerFleetControl`), its enemies are
    KAS teams, and the `/oracleMission` run keeps the direct start (decision 76); the stream moved by the shuffle only.
    **M2-R9**: stands; the stream moved by the exe's qsort only (decision 44), not by the table or the rung's physics; the
    ai streams (`cefa7bd984ff3f57` pair, the soak `4cd65bfa42cfa6c6`) are the AI's guard-rails.
58. **`gunResetGimbleGuns` is uncalled** (GoK's gimbal reset left `aitrackSteadyShip*`; the exe has none). Default: keep.
59. **Default move tactics**: Aggressive for the 26 creators the exe stores 2 for; Neutral for control / active resource
    (the exe's 1) and for the five creators the exe does not contain (no evidence).
60. **The recordings**: the install's `oracle01` is the 10-18 recording (1,763 packets, the unfixed DLL); the 10-17 one is
    archived in `original/oracle/oracle01-rec-20261017-140428/` and stays the rung's reference (replayed through the
    harness's `--recording-src` / `--recording-as`); for the 10-18 session `real` is excluded (a one-step `vqpc` clock
    alignment from tick 2) and its first simulation divergence is tick 118 `ships_pos` / `ships_vel`. **M2-R8**: the install's `oracle01` is now the
    10-19 recording (133 packets, 266 ticks, the unfixed DLL; not a reference); both older recordings replay from their
    archives (`--recording-as oracle01-r6` / `oracle01-r18`); the 10-19 session excludes `real` too (decision 80).
    **M2-R9**: unchanged; the two archived recordings stay the rung's references (10-17: 1922; 10-18: 1721).
61. **Chosen names** where the exe's are unknown: `CAT_SHIPFLAG_KasMines`, the AISkirmish statics / globals
    (`aisShipBeingBuilt`, `aisEnemyBigGun*`, `aisCrystalTarget`, `aisOwnBigGunShip`, `aisEnemyDFGNearMothership`,
    `aisLeastBuiltShip`, `aisFirstPlay`, `aiBuildingShip`, `aiSupportUnitsPending`, `aiCarrier1Ship` / `2Ship`,
    `aiHasExternalConstruction`), the five `AIS_TEAM_*` ids where CET's names differ, the AIPlayer fields
    `catNumSupportModulesBeingBuilt` / `catSupportModuleRequested` / `catAirNumSupportModules`, `catMsgQueue`,
    `cat_aihRescueSupportType`, `cat_aiuRoundToUdword`, `cat_aiuRemoveUntargetableTargets`, `aioCatEnabledOr` /
    `aioCatAttackTactics`, `cat_pieWaypointCursorDraw`, the KASVar store names, the motion-interp helpers.
62. **HWI deviations where the exe would fault or leak** (13 new `CAT-FIX`, 134 in all): `aiuFindClosestEnemyShipWorthCapturing`'s
    NULL, `aioCreateGuardShips`' k/t alternatives, `aioCreateActiveSupport`'s empty team, `aioCreateTakeoutMothershipBig` and
    the three numbers-low handlers for other races, `aimCreateGetShipsNoAdd` / `aimProcessActiveCapture` leaks,
    `cat_aisUpdateTeams`' zero bounty, `cat_kasVarLoad`'s array, the saved creator pointer cleared on load.
63. **AI save bytes**: the ICF-folded handlers are written as 16 / 19 where the exe writes 15 / 18 (same bodies). Keep.
64. **`aiplayerUpdateAll`**: multiplayer computer players run the skirmish AI (`cat_aisPlay`), single player HW1's fleet
    command with fleet control from KAS, the playback skip kept: the exe's.
65. **Render leftovers**: HW1's cursor text stays (removing it re-baselines 144 frame CRCs of the motion guard-rail: your
    call); the 30 remaining `UI_*` sound-id sites stay HW1's until each exe push is read (M2-R8, sound); the camera-derived
    render values with interpolation on belong to the high-refresh-4k feature.
66. **The harness replays archived recordings** (`--recording-src DIR --recording-as NAME2`); the install's directory is
    the default and is never written.
67. **Two faults in long AI soaks** (`ETG.c:7295` `nParams <= 8`, `univRemoveAllTargetReferencesFromBullets`' NULL owner; HW1
    code, in the base binary too): M2-R8's combat / effects owner; the `ai` mode's default stays 400 ticks. **M2-R8: FIXED** (the
    rung producer: the ten ETG parameters, the owner compare, the AISkirmish table read; the 4,000-tick soak PASSes with ASan 0
    and no fatal; the ai test keeps the universe running after the game over, HWI-TEST; decision 73).
68. **The identity start-position table** (the skirmish-start merge, docs/skirmish-start-fix.md): `catStartPositions[8]`
    (the exe's 0x00ab77e0, name chosen) is 0..7, not shuffled (`CAT-PROVISIONAL`, `src/SDL/utility.c`): player i starts at
    the map's position i until stage 6 ports `gameStart`'s 40 stream-0 draws (decision 40); the MissionSphere readers
    `scriptSetMissionSphereCB` 0x004a2420 / `scriptPreMissionSphereCB` 0x004a2820 are the exe's (absent players' spheres
    skipped, the ownerless sphere to the misc player 8, the capture gift the race's Hive Frigate; `CAT-FIX` where the exe
    reads past its table or takes an unset local); a network game's table (`tpGameCreated` +0xC8) is the identity too
    (`CAT-TODO(stage 5 LAN)`). The Mission01 stream is unchanged. Default: keep until stage 6. **M2-R8: the shuffle landed**
    (decision 40); the identity table and its `CAT-PROVISIONAL` are gone; a network game's table stays stage 5 LAN's.
69. **The skirmish self-test and the visible fatals** (the merge): harness mode `skirmish` (`/catSkirmishMap`,
    `/catSkirmishTest`; LiveFireExercise2 by default, added to the scenario list as `spTitleListLoad` adds a line since
    `multiPlayerMissions.script` comments it out, in the exe too; Armed8 the listed 8-player map) is in the exit test from
    now on; `mgSkirmish` sets one computer opponent on every entry (HW1's and the exe's; GoK kept the previous count, 0 in
    a fresh options file); `dbgFatal` writes its string to stderr and a build without ASan prints `hwi-engine: fatal
    signal N` before the default action (`HWI-FIX`, not in the exe: the release build died silently). Default: keep.

**New from M2-R8** (each has a default in the tree; say if you want otherwise; details in docs/m2-r8-summary.md
"Decisions" and "What the user must decide"):
70. **The table `fsqrt`** (decision 23 / 56 restated as M2-R9's first question): the default stays `sqrtf`, the switch
    `/oracleTableFsqrt` (`FastMath.c`, HWI-EXPERIMENT) compiled in and off. With the table the rung's reference reads 246
    (10-17) / 154 (10-18), without it 176 / 117, and the divergence at 177 is the square root itself. Recommendation: make
    the table the default in M2-R9 (`FSQRT_DEFAULT 'table'`, the switch inverted to `/oracleLibmSqrt`); the headless
    Mission01 baseline does not move with it, the replay, ai, skirmish and mission-test streams do. **ANSWERED 2026-10-19: the table `fsqrt` is the engine default** ("keep the game relying on fsqrt, don't ditch core fundamentals"); M2-R9 producer 1 makes it the default and inverts the switch to `/oracleLibmSqrt` (`FSQRT_DEFAULT 'table'` in m2-r9.js). **M2-R9: APPLIED** (`64f49a9`): `catFsqrtTable` TRUE by default, `/oracleLibmSqrt` the A/B switch (HWI-EXPERIMENT), `/oracleTableFsqrt` gone; the rung 1922 / 1721 on the table, 177 / 118 without; every stream's move by the table alone measured once by producer 1 (the headless Mission01 stream and the replays do not move with the table; the ai / skirmish / build / missiontest / loadgame streams and the dumps do); the Mission01 baseline moved by the exe's qsort instead (decision 44).
71. **The exe's x87 shape on the flight path** (the vector, matrix, heading, guidance, rotation, avoidance, formation,
    parade, collision and move-point helpers; `catFsqrtD`): the 53-bit intermediates and single roundings of the
    disassembly; libm `sin` / `cos` for `fsin` / `fcos`, `tanl` + `fma` for `fptan`. The motion / nis guard-rail dumps are
    re-baselined to `0838a011db819920` / `d1e6f6e477906b52` (`/noInterp`) and `7e4f12d35a662197` / `2f18ba93741ebdc6`
    (flags on); decision 46's guard-rails untouched.
72. **The killer asteroid's wrap box** (`Asteroid.catWrapExtent` +0x1A8): written by `AddResourceToVolumeAtPosition`
    0x0049fcf0 for a new game, copied from saves and recordings; a sphere storm's box and a meteor storm's unread terms
    are zero (`CAT-FIX`).
73. **The soak faults** (decision 67 closed): `CAT_ETG_CREATE_PARAMETERS` 10 with the bounded parameter write (`CAT-FIX`),
    the bullet / missile owner compared as a pointer, `catAisTargetTeam`'s one-past read on the exe's contiguous tables
    (`CAT-FIX`); the ai self-test clears the quit and win times after the game over (HWI-TEST). The `ai` mode's default
    stays 400 ticks.
74. **The kasf library takes the exe's signatures** (KAS names, timers, variables, messages and labels as hashes through
    `cat_kasHashString` 0x00491860); the 486-row table and its dispatcher are generated by `tools/m2_kasf_table_gen.py`
    (never hand-typed); HW1's compiled missions are out of the build and stay in the tree unbuilt; `cat_consoleKasCommand`
    does not free the strtok name (`CAT-FIX`). Chosen names: `kasfScopeHash`, the 26 `kasfPred*` predicates,
    `kasfDistanceSquaredBits`, `kasfShipsCreateSome`, `kasfVectorClampToUniverse`, `kasfWideScreenInBody`,
    `kasfWideScreenIsActive` 0x00a43ff0, `kasfWideScreenCloseSensors` 0x00882104, `timTimerFind`, `timTimerLeft`,
    `CAT_SHIPFLAG_Invincible` (was `Orphaned`), the 12 `CAT_UI_*` ids (0x103, 0x123, 0x124, 0x128, 0x12b, 0x12c, 0x12f,
    0x130, 0x131-0x134), `catDamageFactor` (Ship +0x3E0).
75. **The generated kas2c code** (`src/Missions/CatMissions.h`, `KasShared.c`, `SinglePlayer/Mission01.c`,
    `Tutorials/Tutorial1.c`) is written by `tools/m2_kas2c_gen.py` from the export's decompile and verified against the
    machine code (callees, constants, all-constant argument tuples; 149 bodies, 0 hand edits); the functions keep the
    export's kas2c names; a saved KAS function is its exe address minus the current mission's Watch's, searched across
    every generated script's list. Chosen names: `cat_kasShipRemovedFromSelections`, `HYPERSPACE_CATSHIPSHYPERSPACINGIN`
    (9) / `HYPERSPACE_CATFAILEDRETURN` (10), `MISSION_17`, `catSpMissionStartsInNis` 0x008944bc, `catSpMissionNis`
    0x00894508, `catSpMissionArrivesAtOnce` 0x00894470, `catSpNisNames` 0x00894550, `catSpNisletName` 0x008945a0,
    `catSpLevelFade` 0x00a6ac70, `catSpWhiteFlash` 0x00a75280, `catSpLevelFromNisStop` 0x00a6ac74, `catSpFleetArriving`
    0x00a6ad78, `catSpArriveAtOnce` / `catSpArriveAtOnceTime`, `catSpStartInLoadingScreen` 0x00a6ac68,
    `catHrMissionNisVariant` 0x00a6adac, `CatKasFunction`, `cat_spKasScriptFunctions`, `ResourceVolume.catDriftSpeed` /
    `catDriftAddY` / `catDriftAddX` / `catWrapExtent`.
76. **The mission oracle run keeps the direct Mission01 start** (`/oracleMission` and the self-tests on it; `HWI-ORACLE`),
    no NIS and no KAS, so the baseline moved only by the shuffle and does not see the AI; `/catMissionTest` takes the
    exe's NIS-first start and its stream (`becb49b579aba435`) is the mission's guard-rail. Default: keep both. **M2-R9**:
    extended to every mission (`/catMissionTestMission N`, decision 86); the Mission01 self-test stream is
    `cc7f22e7129b9471` (the integrate build, the same content as the producers'), moved against `becb49b579aba435` by the
    table and the qsort as producer 1's section 8 states (the tick-0 fields, `ships_vel` 2, `univcheck` 39, ...).
77. **The mission self-test's events**: the destroy objectives are met with the mission language's `ShipsDamage`, the recon
    with the launch and move orders, the jump with the hyperspace button's function; the player's combat is not simulated;
    no sound and no speech. The Tutorial1 leg keeps speech on, so its stream is not a pair. **M2-R9**: stands for every
    mission (a scripted player per mission in `MissionSelfTest.c`; the combat experiment deferred with its measurement:
    M2-R10 test); the held objectives complete through `ObjectivesDestroyAll` (decision 86); the Tutorial1 leg IS a pair now
    (the virtual audio clock, decision 93).
78. **Single player**: no mission sequence (`currentMission` 1 .. 17); the KAS skill level is the difficulty word
    0x008941a0 (HW1's `CurrentMissionSkillLevel`, which doubled single-player damage, deleted); the warp start follows
    the install's `warp.script` (off); the exe's roll-call, hyperspace and arrival bodies with their `CAT-FIX`es; a
    cataclysm.exe single-player save's mission tail converts through eight new SaveCat32 roots (119 layouts). Chosen names:
    `catSpCapitalOrComponent`, `ShipCanMakeHyperspace`, `CAT_SHIPFLAG_LeftBehind` (catFlags18 0x40), `catSpEtherCleanup`,
    `catSpFleetAnyInHsState` / `catSpEtherAnyInHsState`, `catSpFailedJumpSoundHandle` 0x0089469c, `catSpMissionNextLevel`
    0x00894424, `catSpWideScreenJumpTime` 0x00a6adb4, `catSpFleetCommandTime` 0x00a67488, `CatSpWarpLevel` /
    `catSpWarpLevels` 0x00a67498 and the warp words, `CAT_SP_DEG_TO_RAD`; the exe's `cat_spEmergencyHyperspaceCB`,
    `cat_spPrepareShipHyperspaceOut`, `cat_spLaunchDockedShipsOnArrival`, `cat_spForgetDamageEffects`,
    `cat_spGetDifficultyModifiers`, `cat_spGameDifficulty`, `cat_spWarpToLevel` and its four setters.
79. **The tutorial**: Cataclysm's 54 flags in the exe's layout and name table (pinned 54 + 1), the pointers' objective flag,
    the five buttons as regions under the tutorial's root, hidden behind the three managers; the test clicks Next through
    its region's own callback. Chosen names: `catTutNextTextVisible` 0x00a6c4f0, `catTutQuitVisible` 0x00a6c508,
    `catTutObjPointerX/Y` 0x00a6c51c / 0x00a6c520, `CAT_TUT_POINTER_COLOUR`, the `TUT_NEXTLESSON_*` / `TUT_QUIT_*` /
    `TUT_REST_*` / `TUT_NEXT_*` image indices, `bWaypoint` / `bWaypointIssue` / `bManagersLocked` (the exe's names),
    `tutpointer.objective`; the exe's `cat_tutPointerListReset`, `cat_tutObjPointerXYSet`, `cat_tutButtonsHideForScreen` /
    `RestoreAfterScreen` and the button functions.
80. **The harness** (decisions 60 / 66 updated): the install's `oracle01` is the 10-19 recording (not a reference); the
    10-17 and 10-18 recordings replay from their archives; modes `missiontest` (`--tutorial`, `--mission-test-ticks`) and
    `loadgame` (`--save`, `--load-ticks`; the save copied into the run's settings dir only) are in the exit test from this
    round on; `/oracleShipDump` and `tools/oracle/hwo_invert.py` are the permanent per-ship instrumentation. Chosen
    names: `catFsqrtD`, `catVecMagSqD` / `catVecScaleD`, `catAitrackTurn` / `catAitrackThrust`, `catGetCollSizeD`,
    `catResourceWrap` / `catResourceWrapAxis`, `Asteroid.catWrapExtent`, `CAT_ETG_CREATE_PARAMETERS`, `aisTargetTables` /
    `catAisTargetTeam`, `oracleShipDump*`. **M2-R9**: decision 92 (modes `warpstart`, `missiontest --mission N`, `loadgame
    --training --save-src`, `--record` / `--record-stop`; 29 modes; never edit the harness while a run is in flight).

81. **The exe's `qsort` for every sort the exe makes** (`catQsort` / `catQsortShort` / `catQsortSwap`, `CatSort.c`: the
    exe's CRT quicksort 0x00671060 at its 14 call sites); ties come out as in the exe; `Memory.c`'s debug sort and the
    uncalled `selShipsInSelection` keep the C library's. The mission baseline moved by it (decision 44). Chosen names
    also: `catBlobObjPosition`, `catBlobDistSqD`, `oracleShipDumpAvoid`, `oracleShipDumpFly`, the dump's 'O' / 'V fly' /
    'B' lines.
82. **The rung's reference numbers**: 1922 (10-17, ship 41's `v.z`) and 1721 (10-18, the first bullets) on the table; 177 /
    118 with `/oracleLibmSqrt`; both M2-R10 physics' with `aiuFindDistanceSquared`'s and `tacticsGetShipsMaxVelocity`'s
    unrounded returns (`CAT-TODO(M2-R10 physics)`) and the rest of Blobs.c.
83. **The flight path's further x87 shape** (producer 1): the collision blobs by collision position, the blob attacking flag
    (`shipisattacking` AND `catAttackStarted`), `ObjectsCollided` with the unrounded `1/t`, `collCheckBigShipSmallShipColl`'s
    clamped-host test, `cat_dockFlyOutOfCone`, `cat_gunFixedGunAimedAtTarget`, `hyperspaceCost`'s double return at its five
    callers, `protectShip` / `protectShipsAvg`; `catFmathSqrtDouble` returns 0.0 for a NaN as the exe's `test ah,0x40`.
84. **The ai soak's `rng_all` is not reproducible under load** (the sound stream on the mixer thread's wall clock; the `ai`
    mode does not silence it); the 400-tick ai pair is unaffected; the soak is judged on `result=PASS`, ASan 0, no fatal.
    **The soak moved once more inside R9** at tick 694 `ships_health` (producer 3's ship-part blast fix: the first death),
    bisected by the integrate step; no producer after the rung re-ran it: the next rounds' producers re-run the soak.
85. **New non-fatal UBSan kinds**: `Select.c:1153` / `1154` signed overflow in `selCircleCompute` (sp and the Mission06 /
    12 / 14 / 15 / 16 self-tests), `FOW.c:296-437` `-2147483648 - 2` in `fowActivateCube` / `fowDeactivateCube` / `fowCubeHasUnvisited`'s cell boxes (lines 296 / 349 / 435, from the W wrappers' world-to-cell conversion) (Mission17 only);
    the recorder's packet writes in `CommandNetwork.c` / `Queue.c` (a record run); all M2-R10.
86. **The mission self-test per mission**: `/catMissionTestMission N` (harness `--mode missiontest --mission N`, 2 .. 17)
    enters mission N through the exe's warp start (`cat_spWarpToLevel` 0x0051b9c0 with Warp.script's level-N record; chosen
    over chaining so each test depends on its own mission only); the tables `catMtMissions[]` and a scripted player per
    mission (`MissionSelfTest.c`: the command layer, the managers, `ShipsDamage`; the roll call's enabled button through its
    callback); PASS = level loaded, KAS started, fleet arrived, every objective created in the table's order and completed,
    the jump begun, the next level loaded (Mission17: the game's end), no loss; the run ends 32 ticks after the next level
    loads. The held objectives (Mission04 Destroy, 09 Protect, 11 DESTROY_BEAST / PROTECT_FACTORUM, 14 PROTECT_MOTHERSHIP /
    CAPTURE_TAIIDAN) complete through the Hyperspace path's `ObjectivesDestroyAll` (their scripts have no other end); the
    unreachable ones are left out (Mission06 Kill, 12 WorkerDock / WorkerReturn, 15 DefendMothership, 17 Research).
    `/catMissionTestTrace` for a line per KAS message.
87. **The generator covers the whole campaign**: `tools/m2_kas2c_gen.py` `DEFAULT_SCRIPTS` = Mission01 .. 17 + Tutorial1;
    **2,085 bodies verified** against the machine code, 25,279 all-constant calls' arguments equal; 15 OVERRIDES addresses
    (20 patches), each HAND-EDIT in the output and verified (Mission05's hashes, Mission08's dead `kasJump`, the folded
    `kasfAssert` stub 0x0049ae30 at 13 sites, 0x0062dfe0's label); `src/Missions/meson.build` generated; Tutorial1.c's
    Lesson12 line is the exe's localised table `LSTRING_008cd960[strCurLanguage]` (the earlier output dereferenced an exe
    address: a justified deviation).
88. **The "Mine" / "Slipgate" mission-sphere readers**: `cat_scriptSetMineCB` 0x004a3f20 and `cat_scriptSetSlipgateCB`
    0x004a1020 (paired through `catSlipgatePending` 0x00c362dc); LiveFireExercise2 has 91 ships now (its stream moved once).
89. **Port fixes found by the mission runs** (each at the exe's body): `kasfSpecialOn`'s return, `kasfVectorAdd` /
    `kasfVectorCopy`, `AllowPlayerToResearch`'s -1 (CAT-FIX), `scriptStringToBool`'s digit characters, `tutGameSentMessage`,
    `catKbFocus` / chase's local, `cat_bigGunProcessFireCommand`'s held effect, `MakeExplosionRockBumpables` spares
    `SOF_CatShipPart`, `spHyperspaceSelectionOut`'s clamp host; HWI fixes: the tweak table's `CLASS_*` keys (every repair
    rate had been 0 since M2-R4), `bobUpdateExtraCollBobInfo`'s GoK-only assertion, `cat_beastGetAttackStaticInfo`'s 64-bit
    overlay, `dbgFatal`'s ASan trace.
90. **The `sp` gate opened to every generated mission** and now stops in Mission04 on HW1's `texreg.c (569)` `nPalettes < 8`
    (the exe's `trTextureRegister` 0x00585760 appends a ninth colour scheme without a bound): M2-R10 render; until then `sp`
    is judged on the running universe, ASan 0 and the levels walked (01 -> 02 -> 03 -> 04_NIS02 -> 04).
91. **The end of the game**: Mission17's `MissionCompleted` 0x00491c70 -> `singlePlayerMissionCompleteCB` 0x0051be40 ->
    `utyGameQuitToMain` 0x005ab6b0's credits (`Credits-GameEnd.cred`), reached headlessly. `animBinkPlay` is GoK's stub (no
    animatic plays); `animAviPlay` is 0x00421860's port: M2-R10 routes the calls, the headless tests skip them (default).
92. **The harness** (decision 80 extended): modes `warpstart` (`--warp-level N`), `missiontest --mission N`, `loadgame
    --training --save-src DIR`; `--record NAME` / `--record-stop N` (skirmish); 29 modes; switches `/catMissionTestMission`,
    `/catMissionTestTrace`, `/catWarpStartTest`, `/catLoadGameTraining`, `/oracleRecord`, `/oracleRecordStop`,
    `/oracleResave`, `/catVirtualAudio`, `/oracleLibmSqrt`. Never edit `tools/m2_harness.sh` while a run is in flight.
    `hwo_compare.py` wants `--from-tick 1` between a record run and a playback. Count tags with `/bin/grep`.
93. **The Tutorial1 leg on the virtual audio clock** (`/catVirtualAudio`, set by `/catMissionTestTutorial`: no device, no
    stream thread; `soundVirtualAdvance` mixes 22,050 samples per second of task time in 256-sample blocks); the narration
    takes its game-time length, the leg ends at 10,444 ticks, byte-identical run to run; the 12 lesson saves written and
    "LESSON 11" loads. Names: `soundVirtualClock`, `soundVirtualAdvance`, `isoundstreamstep`. A normal run is unchanged.
94. **Mission06's rare `rng_all`-only divergence** (the mixer thread's `RANDOM_SOUND` on the wall clock suspected): the
    mission runs keep the wall-clock mixer this round and are judged on the simulation fields when `rng_all` alone differs
    (the integrate's pair: 2081); M2-R10 adopts the virtual audio clock for them (every mission's `rng_all` moves once) or
    the oracle excludes the sound streams.
95. **The recorder's record side** (stage 5 reduced): the 4 sites closed at the exe's bodies; hwi-engine records skirmish /
    multiplayer games in cataclysm.exe's format (the save at 0x10ad0027 through the SaveCat32 writer, the `.pkts` file);
    `/oracleRecord <name>` / `/oracleRecordStop <tick>` follow the DLL's record mode (single player refused); a recording's
    replay is IDENTICAL to its record run except `rng0` / `rng6` / `rng_all` (the computer player does not run in a
    playback); the sync checksum's lag is 1 packet (GOG's real-time sessions: 8); `/oracleResave` for the writer's byte
    check; two recordings of the same game differ in the save's stale pointer slots and the `.pkts` header's uninitialised
    tail word (M2-R10). Names: `recordingStartSave`, `recPackDeleteFile` (HWI-FIX), `saveCat32*`, `madCat32ObjectFileOffset`,
    `aitCat32SaveTeam`; tools `tools/formats/pkts_probe.py`, `tools/formats/recording_compare.py`. Only a recording's save
    is written in the exe format; the recorder's files live in the user settings path.
96. **Stage 5 deferred** (decision 9): a recorded game sends nothing for the external builds and the point / mimic / upgrade /
    special-target orders (CommandWrap.c's `CAT-TODO(stage 5)`), in the record run and the replay alike;
    `recPackRecordSaveHeader`'s HEAD header, `KeepAliveDropPlayerCB` (LAN) and `mgUnitCapsEnabled` (options) tagged.
97. **`Subtitle.c:1144` closed** (CAT-FIX at `subTitlesUpdate` 0x005842f0: the location cards' actor 0x70 starts from
    `STT_LocationCard`); the reviews of the 123 stage-4 IDENTICAL bodies, the 31-row sample (2 ported, 3 tagged, 15
    IDENTICAL) and 42 notes on edited rows are in the map (`m2r9:recorder-leftovers`).

## M2-R6: COMPLETE 2026-10-18 (run from the user's terminal session 38027cce, not from the orchestrator)

cycle/workflows/m2-r6.js finished in session 38027cce (Fable, Remote Control): the "## M2" subsection "M2-R6: COMPLETE" above has the exit test and the seven ports. **M2-R7 also finished in that session on 2026-10-19** (cycle/workflows/m2-r7.js, run id wf_b238b029-e72; its integrate step was cut off by the weekly limit on 2026-10-18 and resumed on 2026-10-19 after the skirmish-start merge; "M2-R7: COMPLETE" above, docs/m2-r7-summary.md). **M2-R8 also finished in that session on 2026-10-19** (cycle/workflows/m2-r8.js, run id wf_74bcf42e-330; "M2-R8: COMPLETE" above, docs/m2-r8-summary.md). **M2-R9 also finished in that session on 2026-10-20** (cycle/workflows/m2-r9.js, run id wf_c0cd6893-248; "M2-R9: COMPLETE" above, docs/m2-r9-summary.md). The orchestrator must relaunch none of them; the M2-R10 script is not yet written (docs/m2-r9-summary.md "Plan for M2-R10" and "What the user must decide"). Merges to main are handled by the user.

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
