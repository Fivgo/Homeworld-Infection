# Work cycle and resume point

Last checkpoint: 2026-10-30 09:09

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
    claude limited : False
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
the round ran and finished the same day; its record is the subsection below (the plan table it carried is in
docs/m2-r9-summary.md "Plan for M2-R10").

### M2-R10: COMPLETE 2026-10-20 (the play path's leftovers: the oracle's rung past 1922 / 1721, the Mission04 render stop + the animatics + the sp gate, the tests and the oracle's leftovers: the virtual audio clock in the mission runs, the failed-jump test, the review sample; the M3 plan)

Summary, exit test and the M3 plan: [docs/m2-r10-summary.md](../docs/m2-r10-summary.md). Workflow `cycle/workflows/m2-r10.js`
(74816ff; run id `wf_dd85ad7e-70e`, `ROUND_BASE 29f7c0d`, `FOURTH_RECORDING null`, `MODEB_RESULT null`), launched 09:56 on
the user's word from the user's terminal session 38027cce (Fable), not from the orchestrator; 7 agents (three ports in
sequence: the rung, physics; the render stop + the animatics + the sp gate; the tests and the oracle's leftovers; three
audits, each checkpointing its port: `36289a8` 0.97, `3ea7b12` 0.9375, `f9590bc` 0.864, all PASS-WITH-FIXES; integrate).
No producer was cut off.

**R10 exit test: MET** (the integrate step, clean builds of `f9590bc` (`[270/270]`, 0 warnings), plain harness, silent,
101 runs `r10i-*` in five parallel chains; `runs.txt` lines 2794-2894; ASan 0 in all):
- **Mission01 headless**: pairs byte-identical, debug `5bd51234405ea0d8` / release `a44d585d71233c4a` (IDENTICAL to each
  other), MISSION TEST PASS in both (perturb at 3000 `rng10`, flip at 4000); `/noInterp`, `/noNISInterp` and
  `/oracleLibmSqrt` byte-identical to A; **IDENTICAL in every field to R9's `553845b65e87f3b5`** (decision 44 unchanged)
  and to every producer's and audit's Mission01 run of the round.
- **The rung**: 10-17 IDENTICAL through 2822, first divergent **2823 `ships_vel`** (ship 48; R9: 1922); 10-18 through
  2870, **2871 `ships_vel`** (ship 49; R9: 1721 `bullets`); with `/oracleLibmSqrt` 177 / 118; the release replay equal to
  the debug on every field but `rng_all` (6377, as R9); both replays IDENTICAL in every field to producer 1's, the rung
  audit's and producer 3's; the next divergences localised (a bProcessor following the turning bWorker 44: the Worker's
  coordsys, decision 100; owner M3 oracle physics). **Mode B not run** (no `original/oracle/hwirec*-play-*`), no fourth
  recording (decision 48 open); the hwirec01 / hwirec02 playbacks are pairs, hwirec01's IDENTICAL to R9's, hwirec02's
  moved from 706 by the ports as stated.
- **Missions 01-17**: 17 / 17 PASS with every objective in order, **all 17 pairs byte-identical (Mission06 too)**, ASan 0
  in all 34 runs; every stream IDENTICAL to producer 3's final `r10p3g-mNN-a`; against R9's, producer 1's stated flight /
  combat moves plus four render-side `rng_all` moves (02 / 10 / 14 / 03 by producer 2's texture-registry change, 06 by
  the nebula fix); **Mission17 ends the game** (`game end tick=15584`, `Credits-GameEnd.cred`); the Tutorial1 leg 12 / 12
  as a pair; the warp start (level 10) a pair; "LESSON 11" loads (a pair); **the failed-jump pair** (Mission03
  `--fail-jump`: armed 17472, the fleet back at 17635) IDENTICAL to producer 3's.
- **The sp gate**: `sp --timeout 1200` walks Mission01_NIS01 -> 01 -> 02 -> 03 -> 04_NIS02 -> 04 -> 05 -> 06_NIS04 -> 06
  -> 07_NIS05 -> 07 -> 08 -> 09 -> 10_NIS06 -> 10 -> 11 -> 12_NIS07 -> 12 (127 screenshots, running in Mission12 at the
  timeout), **ASan 0, no leak, no fatal, no assertion** (the Mission04 `texreg.c` stop gone, decision 101), **0 `Select.c`
  / `FOW.c` reports** (decision 103), `animatics: 12 skipped (headless): Animatic01 .. Animatic12` (one line per
  transition, decision 102); **fe** Main_game_screen.
- **Self-tests** all PASS (statics x2, meshes, textures, oracle, release SELFTEST PASS, etg, dock, command, render, sound,
  hud, ui, fescreens 22/22 (0 fescreen UBSan), strings 1,705/1,705, keys, console tablerows=486, build pair, nisparse,
  toggle x3, ai pair + 4,000-tick soak, skirmish LFE2 pair + Armed8, missiontest pair, loadgame pair, motion / nis dumps
  unchanged `183681235a3fdc7b` / `13ca70eea998e5de`); every quitting run rc 0 (sp and fe at their timeouts by design);
  layout32 OK; the generators `--check` current.
- **Guard rails vs R9**: ai 400, both skirmishes, build, the dumps, hwirec01's playback **unchanged**; the soak (431),
  missiontest (1772), loadgame (1985), tutorial (4753), lesson load (8360), warp start (`ships_rot` 1), hwirec02's playback
  (706) moved exactly as producer 1 stated; no unstated move found (the four render-side `rng_all` moves were stated by
  producers 2 and 3).
- **Tags**: `grep -rn -E 'CAT-(TODO|PROVISIONAL)\([^)]*(M2-R(9|10)|stage 6)' src/` empty; 35 `CAT-TODO` lines, 26 owner
  tags (stage 5 23: plain 8, multiplayer options 8, LAN 6, multiplayer 1; M3 oracle physics 3), 4 `CAT-PROVISIONAL`
  (count with `/bin/grep`).
- **UBSan kinds** (all non-fatal): the ETG / particle alignment kinds and `BTG.c` as in R9; **gone**: `Select.c:1153` /
  `1154`, `FOW.c:296-437`, `stb_image_write.h:1263`; no new kind.

Delta map after the round: IDENTICAL 2,763 / MODIFIED 1,574 / REWRITTEN 83 (14 IDENTICAL rows found modified by the
ports, 4 MODIFIED rows found IDENTICAL), 3,569 reviewed rows (R9: 3,512); 212 MODIFIED / REWRITTEN rows still unflagged
(120 outside `TitanInterface.cpp`). 35 lines carry `CAT-TODO` (R9: 34: the two `M2-R10 physics` closed, three
`M3 oracle physics` opened).

**M3 plan** (**superseded 2026-10-21 by the M3-R1 table below**: M2-R10's rows 1-3 carried, row 0 done once, row 4 sized by this round; docs/m2-r10-summary.md "Plan for M3"; the play-test can start now with `tools/play.sh`, the brief in the
summary; first steps: the user plays Mission01 .. 04 with the two animatics attended, a Windows 11 producer is launched
from the main checkout, the soak's `/frameLog` switch is written before the user's hour):

| # | step | scope | counts | done when |
|---|---|---|---|---|
| 0 | **the user's play-test** (now, `tools/play.sh`; the brief below) | the campaign from New Game as far as it goes (Mission01 .. 04 and on to the credits if it goes), **Animatic01 / 02 attended** (producer 2's check), the tutorial's 12 lessons, a skirmish against one computer, a recording and its playback | 3-6 hours of play | a report per item (what differed from the GOG game, every stop with the log's last lines, the frame rate's drift); M3-R2 sized from it |
| 1 | **the Windows 11 build** (producer) | meson + SDL2 + GL with clang (clang-cl / lld-link, decision 7's toolchain) or MSVC; what the Linux tree assumes (the offscreen video driver, ASan / LSan, `long` is 32-bit on Win64: the LP64 save blob changes shape, so the native save format is pinned to fixed-width types or saves stay per platform; the path layer; `tools/m2_harness.sh` under Git Bash / MSYS2 or a port of the modes the gate needs: statics .. missiontest, mission, replay, skirmish); the x87 helpers' libm dependence measured first (the Mission01 stream, both replays and one mission run against the Linux streams; a last-ulp difference in `sin` / `cos` names its site) | 2 platform layers, 29 modes, 1 format pin | the release and debug builds on Windows 11 with 0 warnings; the self-tests PASS there once; the headless Mission01 stream, the two replays and a mission stream IDENTICAL to the Linux ones (or every difference localised to a libm call and closed); a save and a recording made on one OS load and replay on the other |
| 2 | **the one-hour 4K@144 soak, no gamescope** (producer + the user's hour) | a `/frameLog` switch (HWI-TEST) writing per-second frame-time percentiles, the universe tick rate and the heap's used / growth counters to the log; the headless proxy: the chained `sp` loop walked past Mission09 for an hour on the virtual clock with the heap counters (the growth heaps 1 .. 3 by Mission08: the "dupe trcolorinfo list" overwrite in `trRegisterRemoval` / `trTextureColorsUpdate`, the 2,438 live blocks at Mission09, the main heap's size for the 64-bit layout against the exe's 24 MB); then the attended hour at 3840x2160@144 (`tools/play.sh -- /fullscreen`, the campaign or a skirmish, no gamescope: docs/CONSTRAINTS.md 1) | 1 switch, 2 leak candidates, 1 attended hour | no frame-rate drift over the attended hour (the percentiles of the last ten minutes within the first ten's); the headless hour allocates no growth heap, or every growth is attributed and fixed; ASan 0 |
| 3 | **the oracle's rung** (M3 oracle physics; the fourth recording and mode B if the user made them) | 10-17 2823 / 10-18 2871 (the Worker's dock / harvest heading paths against the exe), `RangeToTarget` / `RangeToTargetGivenDist`'s unrounded returns at their callers, `cat_gunCanFire`'s second mode, `cloudUpdateSystem`, the other `frandom` sites (LevelLoad.c's first: layouts), Mission17's self-targeting KAS teams (the exe's selection and attack path) | 3 tags + ~10 sites + 2 divergences + 1 self-target | the 10-17 replay IDENTICAL beyond 2823 and the 10-18 beyond 2871, or the next divergences localised and owned; mode B's first divergence localised if run; no NaN flight in Mission17 or its cause stated as the exe's |
| 4 | **M3-R2: the fix round** | sized from the play-test report (every stop and difference an item with an owner) | | the play-test's stops fixed and re-played; the sign-off |
| then | **stage 5** (decision 9) | the 23 `CAT-TODO(stage 5 ...)` sites: the LAN game's senders and keep-alive, the options word, the `/packetRecord` header, the Titan / WON stubs | 23 sites | LAN interop with stock 1.01 nice-to-have (decision 9); the wire format not broken |

**Then**: the stage-5 remainder per decision 9 after M3 (the 23 `CAT-TODO(stage 5 ...)` sites), or an M2-R11 physics round
only if the user wants the rung before the play-test.

### M3-R1: COMPLETE 2026-10-21 (the play-test fix round: the letterbox bars in front + the `letterboxBehind` setting, the team colours across the warp, FULL DETAIL = LOD 0 by default, the unfocused objects' jitter, the UI scale 200% at 4K / 150% at 1440p + the 4K window; presentation only, no simulation field moved)

Summary, exit test, the five fences and the refreshed play-test brief: [docs/m3-r1-summary.md](../docs/m3-r1-summary.md).
Workflow `cycle/workflows/m3-r1.js` (18281e2; run id `wf_42a468c8-942`, `ROUND_BASE ce4fab4`), launched 2026-10-20 21:19
on the user's word from the user's terminal session 38027cce (Fable), not from the orchestrator, relaunched twice on
10-21 (item 6 and the jitter recipe folded in; the main checkout's stale script copy); 11 agents (five ports in sequence
with their audits, each checkpointing its port: letterbox-front `94599c8` 0.919, team-colours-warp `2b454b6` 0.91,
full-detail `8410122` 0.913, motion-jitter `fdf35ee` 0.853, ui-scale `58df92d` 0.857, all PASS-WITH-FIXES; integrate).
No producer was cut off. The user's answers: `cycle/answers/m3-r1-user.md` (Smooth motion unknown to the user; the
skirmish Recon recipe).

**M3-R1 exit test: MET** (the integrate step, clean builds of `58df92d` (`[273/273]`, 0 warnings: debug
`dc2e8e09ba80f7cc`, release `194161675b97762a`, copied aside), plain harness, silent, 161 runs `m3r1i-*` in eight
chains beside the user's load (load average 12-20: VS Code's ripgrep); `runs.txt` lines 3544-3704; ASan 0 in all):
- **Mission01 headless**: pairs byte-identical, debug `1ae66610c3f02410` / release `234bcccfb948ab98` (IDENTICAL to each
  other), MISSION TEST PASS in both (perturb at 3000 `rng10`, flip at 4000); `/noInterp`, `/noNISInterp`,
  `/oracleLibmSqrt`, `letterboxBehind 1` (row and switch), `/lodBands`, `noLOD 0`, `/uiScale 200`, `uiScale 150` and
  the three opt-outs together all byte-identical to A; **IDENTICAL in every one of the 21 fields to R10's
  `5bd51234405ea0d8`** (decision 44 unchanged) and to every producer's and audit's Mission01 run of the round.
- **The rung unchanged**: 10-17 IDENTICAL through 2822, first divergent **2823 `ships_vel`**; 10-18 through 2870, **2871
  `ships_vel`**; with `/oracleLibmSqrt` 177 / 118; the release replay equal to the debug on every field but `rng_all`
  (6377, as R10); both replays IDENTICAL in every field to `r10i-r17-a` / `r10i-r18-a`; the hwirec01 / hwirec02
  playbacks IDENTICAL to R10's. **Mode B not run** (no `original/oracle/hwirec*-play-*`), no fourth recording (the
  newest `oracle01-rec-*` is 20261019-143530; decision 48 open).
- **Missions 01-17**: 17 / 17 PASS with every objective in order, all 17 pairs byte-identical, ASan 0 in all 34 runs;
  **every stream IDENTICAL in every field to R10's `r10i-mN-a`** (the full-detail audit's `renderedLODs` guard and the
  Beast-stun lightning kept on the exe's level: no `rng0` / `rng_all` move anywhere); the Tutorial1 leg, the lesson
  load, the warp start, the failed-jump pair, loadgame, the skirmishes, build, ai and the 4,000-tick soak (20 fields;
  `rng_all` exempt, decision 84) all IDENTICAL to R10's.
- **The sp gate**: `sp --timeout 1200` at 1024x768 walks Mission01_NIS01 .. Mission12 (129 screenshots, running in Mission12 at the timeout; ASan 0, no leak, no fatal, 0 `Select.c` / `FOW.c` lines, 12 animatics skipped); `sp --timeout 1200` at **3840x2160** walks the same chain to Mission11 (121 screenshots; ASan 0, no leak); **fe**
  Main_game_screen at 1024x768 and at 3840x2160 (`uiscale: ... scale 200% (auto, uiScale 0) logical 1920x1080`); no
  `DEADLYSIGNAL` in either 4K run.
- **Self-tests** all PASS (statics x2, meshes, textures, oracle + the release SELFTEST pair, etg, dock, command, render,
  sound, hud, ui, fescreens 22 / 22 `options=35` (0 fescreen UBSan), strings 1,705 / 1,705, keys, console `tablerows=486
  shown=34 set=27`, build pair, nisparse, **toggle x5** (`1/1`, `0/0`, `1/0`, `/letterboxBehind`, `/lodBands`: the new
  `screen untouched` / `screen cancel`, `letterbox` and `fulldetail` checks PASS), **uiscale x9** (45 checks at 4K /
  1440p / 1080p / 1024x768, the forced bounds, the cfg / switch overrides), missiontest pair, loadgame pair, warp pair,
  tload pair, Tutorial1 pair 12 / 12, ai pair + soak, skirmish pair + Armed8; every quitting run rc 0 (fe / sp / the fe
  captures at their timeouts by design); layout32 OK; the generators `--check` current.
- **The dumps** (decision 46 updated, re-baselined with the producers' reasons): motion `/noInterp` `c503c869f0dc9cdb` / `3016280e9e0dd3c8`, flags-on `c0e411662fa2f527` / `428ef41479eb5749`, nis `/noInterp`
  `86cb155acffc180e` / `196bab32d244f614`, flags-on `0307f3be2044c29c` / `a5de13a44b8d264f` (whole / crc-stripped; 1,431 / 1,440 frames;
  all four EQUAL to producer 5's audited references); the opt-outs together (`letterboxBehind 1` + `/lodBands` + `/noInterp`) byte-equal
  (`cmp`) to R10's `r10i-motion-noi` / `r10i-nis-noi` (`183681235a3fdc7b` / `13ca70eea998e5de`): against the default only the `crc=`
  column differs (603 of 1,431 motion frames, 1,260 of 1,440 nis frames), no position column (decision 118).
- **The five fences** rerun with numbers (the summary's tables): the letterbox bars (NIS n06 frame 1374: 109,568 /
  109,568 bar pixels black; the old look 29,254 non-black, **bit for bit ce4fab4's frame** with `letterboxBehind 1` +
  `/lodBands`; the fade frame 584 all 32,32,32 over the ship; Mission01's cutscenes (`m3r1i-lb-m1seg` / `-behind`, missiontest NIS-first, `--shot 747,789,1128 --shot-letterbox`: 6 frames each, n01 and the arrival nislet01 (`s1`); 3 differ between the settings: n01 tick 1128 242 pixels in the top bar, s1 tick 747 51,099 in the bottom bar (the hull), s1 tick 789 65,115 in the top bar, the default's bar rows 0 non-black in all three; the other 3 byte-identical: the report's numbers); Mission02's KAS letterbox
  (`m3r1i-lb-m2-shot` / `-behind`, `--mission 2 --shot 25770-25840/2 --shot-letterbox`: 32 frames each, 0 differ between the settings; tick 25800 the top 51 rows 66,66,66 (52,134 of 52,224, the cursor's 90); tick 25840 bands 107 / 107; the streams IDENTICAL to R10's Mission02)); the team colours (7 / 7 checks PASS into Mission02 with the user's scheme and a red one; the Kuun-Lan's 24x24 region green-dominant (53 / 71 / 35, 93 / 118 / 32, 114 / 118 / 58) with the user's, red-dominant with the other; the streams IDENTICAL to R10's missiontest); FULL DETAIL (`lodtest` 164 ships at the ai scene all drawn 0 under the default, drawn = band with `/lodBands`, 25,169 vs 5,330 vertices; the 4K cost on an idle GPU, release, 164 ships: on
  9.578 / 9.083 ms median, off 2.602 / 3.574 ms, 25,169 vs 4,726 vertices); the motion residual on the real clock at
  144 Hz (the user's Recon B: `/noInterp` repeated 832 / 941 frames, jerk p95 1.488 px; flags on **0 / 941**, 0.008 px;
  the NIS camera jerk p95 492 -> 5.8); the UI scale (the 4K front end box-downscaled 2x equals the 1080p frame to the bit at frames 400 and 800, the 4K HUD 0.9996+, the 1440p frame centre-sampled equals the 1707x960 frame to the bit, the 4K click hit / miss, the bottom row 2159 -> logical 1079, the forced-bounds fit `display size -> fullscreen desktop`, 45 / 45 checks in nine runs).
- **Tags**: `CAT-(TODO|PROVISIONAL)\([^)]*M3-R1` empty; 37 `CAT-TODO` lines, 28 owner tags (stage 5 23, M3 oracle
  physics 3, M3 soak 1, M3-R2 1), 4 `CAT-PROVISIONAL`; `HWI-FEATURE` motion-interp 484 / letterbox-setting 36 /
  full-detail 178 / ui-scale 299 (decision 120).
- **UBSan kinds** (all non-fatal): the same nine kinds as R10's, no new kind (the ETG opcode structs and `struct particle`, `BTG.c`'s
  `short` / `udword` loads; 47 of 161 runs report nothing; fescreen 0 in all 161); ASan 0 and no unsuppressed leak in all 161.

Delta map after the round: IDENTICAL 2,762 / MODIFIED 1,575 / REWRITTEN 83 (`rndDrawScissorBars` IDENTICAL -> MODIFIED;
producer 5's four reclassifications reverted by its audit), 3,573 reviewed rows (R10: 3,569); 210 MODIFIED / REWRITTEN
rows still unflagged (118 outside `TitanInterface.cpp`).

**The plan for M3-R2 and the rest of M3** (**superseded 2026-10-24 by M3-R2's plan table in the M3-R2 subsection below**; docs/m3-r1-summary.md "Plan for M3-R2"; supersedes M2-R10's M3 plan table
above; the play-test can start now with `tools/play.sh --rebuild`, the brief in the summary; Smooth motion must be ticked
once: the play-home cfg still says `objInterpolate 0` / `nisInterpolate 0`):

| # | step | scope | done when |
|---|---|---|---|
| 0 | **the user's next play-test** (now; the refreshed brief) | the six items re-checked (bars, colours across every jump, FULL DETAIL at the far zoom, the moving Recon with Smooth motion ON, the 4K UI at 200% and the window, the mouse at the edges), then M2-R10's items (the two animatics, the campaign, the tutorial, a skirmish and a recording); the questions of "What the user must decide" answered | a report per item; M3-R2 sized from it |
| 1 | **the Windows 11 build** (producer; M2-R10's row 1 + uiscale.c's display bounds / fullscreen-desktop fit on Windows) | the two platform layers, the 30 harness modes, the LP64 save blob's format pin, the libm dependence measured first | release + debug on Windows 11 with 0 warnings; the self-tests PASS there once; the Mission01 stream, both replays and a mission stream IDENTICAL to Linux (or every difference localised to a libm call); a save and a recording cross-load |
| 2 | **the one-hour 4K@144 soak, no gamescope** (producer + the user's hour; M2-R10's row 2 + this round's inputs) | `/frameLog` (HWI-TEST), the headless `sp` hour with the heap counters (O4's 1,587 / 1,898 blocks as the baseline), the task clock (`CAT-TODO(M3 soak)`), frame pacing at 4K (FULL DETAIL's ~10 ms frame), the `DEADLYSIGNAL` in a timeout-kill quit, then the attended hour at 3840x2160@144 (borderless by the fit rule) | no frame-rate drift over the attended hour; no growth heap in the headless hour or every growth attributed and fixed; the task clock at the exe's rate or its difference stated; ASan 0 |
| 3 | **the oracle's rung** (M3 oracle physics; M2-R10's row 3, unchanged) | 10-17 2823 / 10-18 2871, the three `CAT-TODO(M3 oracle physics)`, the other `frandom` sites, Mission17's self-targeting KAS teams; mode B and the fourth recording if the user made them | the replays IDENTICAL beyond 2823 / 2871 or the next divergences localised and owned |
| 4 | **M3-R2: the fix round** (sized from row 0's report) | already owned: the letter height if the user picks the exe's, the additive state after full bars if preferred, the BTG star size, the quit after a resolution change, FULL DETAIL's mesh submission and the effects cap if wanted, trails at LOD 0 if wanted, the sensors map's lines at 200% if wanted, the tutorial pointer if seen; plus every stop and difference the play-test reports | the play-test's stops fixed and re-played; every port IDENTICAL on the streams; the sign-off |
| then | **stage 5** (decision 9) | the 23 `CAT-TODO(stage 5 ...)` sites | LAN interop nice-to-have; the wire format not broken |

### M3-R2: COMPLETE 2026-10-24 (the second play-test fix round: the `hwi-settings:` line + the overlay + `/frameLog` and the quit after a resolution change, the cutscene's one-frame flicker (not reproduced; instrumented), the hyperspace arrival over the whole assembly, FULL DETAIL's mesh batching; presentation only, no simulation field moved)

Summary, exit test, the four fences and the refreshed play-test brief: [docs/m3-r2-summary.md](../docs/m3-r2-summary.md).
Workflow `cycle/workflows/m3-r2.js` (944565c; run id `wf_3590992d-400`, `ROUND_BASE 06dae5c`), launched 2026-10-22 11:00 on the
user's word from the user's terminal session 38027cce (Fable), not from the orchestrator; cut off by the weekly limit after port 1
and resumed on 2026-10-24 with the same run id; 9 agents (four ports in sequence with their audits, each checkpointing its port:
instrumentation `bbdc613` 0.89, nis-flicker `4b799cb` 0.97, hyperspace-assembly `83c4b26` 0.89, mesh-batching `47922ff` 1.00, all
PASS-WITH-FIXES; integrate). The user's answers: `cycle/answers/m3-r2-user.md` (the GPU free; the overlay key one of F1-F12, queued as
M3-R3 item 0; the overlay fine at 200 %; the camera jitter marked fixed); the third play-test's seven items: `cycle/answers/m3-r3-user.md`
(M3-R3's queue).

**M3-R2 exit test: MET** (the integrate step, clean builds of `47922ff` (`[274/274]`, 0 warnings: debug `9cd73b890cfd9fbc`, release
`489f5937ba6597b1`, copied aside), plain harness, silent, 162 runs `m3r2i-e-*` in ten chains + a two-run attribution chain beside the user's load (load average
24-37: VS Code's ripgrep); `runs.txt` lines 4131-4292; ASan 0 in all); one done-when not met by nature (item 2: the flicker not
reproduced; the instrument proven on a forced frame; the attended `frame.sig` settles it); fence 4's p99 stated under the load; **one self-test check fails**: the toggle self-test under `/letterboxBehind`
(`m3r2i-e-toggle-lb` `fails=1 result=FAIL`: the M3-R2 check "a console row's change prints it with source console" expects `(console)`
where the line prints `letterboxBehind=1 (switch)` by its documented rule while the switch stands, `src/Game/MotionTest.c:2128-2131`;
a check's expectation, not a product fault; nobody had run the toggle self-test with `/letterboxBehind` in the round; owner M3-R3, one
line; the other six toggle runs PASS):
- **Mission01 headless**: pairs byte-identical, debug `2131271d771f2056` / release `ce30c53259f034c8` (IDENTICAL to each other),
  MISSION TEST PASS in both (perturb at 3000 `rng10`, flip at 4000); `/noInterp`, `/noNISInterp`, `/oracleLibmSqrt`, `letterboxBehind
  1` (row and switch), `/lodBands`, `noLOD 0`, `/uiScale 200`, `uiScale 150`, the three opt-outs together, `/overlay`, `--frame-log`,
  `/meshImmediate` and `/noHsAssembly` all byte-identical to A; **IDENTICAL in every one of the 21 fields to M3-R1's
  `1ae66610c3f02410`** and so to R10's `5bd51234405ea0d8` (decision 44 unchanged) and to every producer's and audit's Mission01 run.
- **The rung unchanged**: 10-17 IDENTICAL through 2822, first divergent **2823 `ships_vel`**; 10-18 through 2870, **2871
  `ships_vel`**; with `/oracleLibmSqrt` 177 / 118; the exclusions `rng6` at 161 in both, `real` at 2 in 10-18; the release replay equal
  to the debug on every field but `rng_all` (6377, as M3-R1); both replays IDENTICAL in every field to `m3r1i-r17-a` / `m3r1i-r18-a`;
  the hwirec01 / hwirec02 playbacks IDENTICAL to M3-R1's. **Mode B not run** (no `original/oracle/hwirec*-play-*`), no fourth
  recording (the newest `oracle01-rec-*` is 20261019-143530; decision 48 open).
- **Missions 01-17**: 17 / 17 PASS with every objective in order, all 17 pairs byte-identical, ASan 0 in all 34 runs; **every stream
  IDENTICAL in every field to M3-R1's `m3r1i-mN-a`** (Mission17's milestone `the game's end (credits)`); the Tutorial1 leg, the lesson
  load, the warp start, the failed-jump pair, loadgame, the skirmishes, build, ai and the 4,000-tick soak (20 fields; `rng_all` exempt,
  decision 84, and `rng_all` alone) all IDENTICAL to M3-R1's.
- **The sp gate**: the 1,200 s `sp` reaches Mission12 at 1024x768 (12 levels, 129 golden-shot points, 23,289 file loads, ASan 0, no unsuppressed leak) and Mission11 at 3840x2160 (119 points, 22,285 loads), both rc 124 at the timeout by design; **fe** Main_game_screen at 1024x768 and at 3840x2160 (`uiscale: ... scale 200% (auto, uiScale 0)
  logical 1920x1080`); no `DEADLYSIGNAL` line in either `sp` run's timeout-kill quit, nor in any of the 162 runs.
- **Self-tests** all PASS but the one check above (statics x2, meshes, textures, oracle + the release SELFTEST pair, etg, dock, command, render, sound, hud, ui,
  fescreens `options=35` (0 fescreen UBSan), strings 1,705 / 1,705, keys 197 / 149, console `tablerows=486 shown=36 set=29`, build pair,
  nisparse, **modequit** (the switch to 1280x720, rc 0, one quit line: PASS), **toggle 6 of 7** (plain, `/noInterp`, `/noNISInterp`, `/lodBands`,
  `/overlay`, `/meshImmediate` PASS; `/letterboxBehind` `fails=1`, the one check), **uiscale x9** (45 / 45 checks each), missiontest pair, loadgame pair,
  warp pair, tload pair, Tutorial1 pair 12 / 12, ai pair + soak, skirmish pair + Armed8, the team colours (the user's scheme and a red one
  into Mission02, 7 / 7 checks; Mission03 into Mission04, 6 / 6), lodtest (on + `/lodBands`); every quitting run rc 0 (fe / sp at their
  timeouts by design); layout32 OK; the generators `--check` current.
- **The dumps** (decisions 46 / 118 stand: **no dump moved this round**): motion `/noInterp` `c503c869f0dc9cdb` / `3016280e9e0dd3c8`,
  flags-on `c0e411662fa2f527` / `428ef41479eb5749`, nis `/noInterp` `86cb155acffc180e` / `196bab32d244f614`, flags-on
  `0307f3be2044c29c` / `a5de13a44b8d264f` (whole / crc-stripped; 1,431 / 1,440 frames), byte-equal to M3-R1's files and to producer
  4's final references; the opt-outs together byte-equal to R10's `r10i-motion-noi` / `r10i-nis-noi`; the `/meshImmediate` twins byte-equal to the batched path's (`motion-on` = `motion-on-imm`, `nis-on` = `nis-on-imm`).
- **The four fences** rerun with numbers (the summary's tables): the settings line in every run (`objInterpolate=1 (default) ...
  overlay=0 (default) meshImmediate=0 (default) fit=window (...) 1024x768 switches=none [start]`; the switches and cfg rows labelled by
  source), the overlay confined to its box (1024x768 `differ=16064 bbox=2,2,253,66`, 4K `differ=64256 bbox=4,4,506,132`, byte-equal to
  producer 1's captures), the modequit PASS, the 4K `/frameLog` pair (batched `p50=5.3 p95=12.1 p99=16.3 max=1213.603 over=94 (18.8%)` against immediate `p50=13.2 p95=24.1 p99=31.7 over=485 (97.0%)` ms over 500 frames each under the load, the heap +2.3 / +1.2 MB a minute in the 4K ai scene, `drift(thirds)=PASS`; Mission01's 6,000 frames `p50=1.6 p95=2.1 p99=2.7 over=3 (0.1%)`, the heap constant, `dupe=0->0`); the flicker scans clean (n01 16,084 / nislet01 15,553 / n06 9,505 frames at 144 virtual fps (debug), the Mission01 NIS-first self-test 60,031 (release, 2 segments), the three real-clock scans 15,277 / 14,689 / 9,049 (release): 140,188 frames,
  0 outliers, 0 short runs; the three real-clock scans on the release binary clean); the hyperspace arrival (`hsgate:` tick 1749 `now
  up=2035.1 right=1762.1`, the assembly union z -1998.1..1990.0, `hsassembly:` 20 lines, the first `tick=1750 pending parts drawn=6 under host=sMothership id=21 hsState=2 clipt=0.9375`; `hsarrive:` 14, `hsfinish:` 7: the hull at 1917, the modules 1927-1938; the stern modules present at 1918-1934 where the
  audit's reverted build misses 8,659 / 8,647 / 8,625 / 6,773 / 6,756 px: this step 0 px against `/noHsAssembly` at every one of 1914-1942 and 225 / 8,659 / 8,647 / 8,625 / 6,773 / 6,756 / 0 / 0 against the reverted build (the audit's counts exactly); 130 / 130 frames byte-equal to producer 3's final capture; `/noHsAssembly` byte-equal in these
  missions; Mission02's gate at tick 22 and Mission03's departure as the producer's captures); FULL DETAIL batched vs immediate at 4K on
  the idle GPU under a load average of ~30: medians 4.6-5.0 ms batched against 13.7 immediate (`--lodstats`, 301 frames; the capture runs 4.8 against 12.4), p99 12.8-16.1 against 34.0-41.0 ms (the capture runs 36.2 against 46.7), 25,169 vertices both, the three 4K captures byte-equal
  between the paths, the dumps too.
- **Tags**: `CAT-(TODO|PROVISIONAL)\([^)]*M3-R2` empty (BTG.c:1151 re-targeted to `CAT-TODO(M3-R3)`, comment-only, rebuilt clean with 0
  warnings); 37 `CAT-TODO` lines, 28 owner tags (stage 5 23, M3 oracle physics 3, M3 soak 1, M3-R3 1), 4 `CAT-PROVISIONAL`; `HWI-FEATURE`
  motion-interp 486 / letterbox-setting 37 / full-detail 179 / ui-scale 300 / instrumentation 102 / hyperspace-assembly 55 /
  mesh-batching 160; `HWI-FIX` 62 (60 at the round base), `HWI-TEST` 286 (222), `CAT-FIX` 195 (decision 130).
- **UBSan kinds** (all non-fatal): the same nine kinds as M3-R1's and R10's, no new kind (the ETG opcode structs and `struct particle`,
  `BTG.c`'s `short` / `udword` loads; 48 of 162 runs report nothing; fescreen 0 in all); ASan 0 and no unsuppressed
  leak in all 162.

Delta map after the round: IDENTICAL 2,757 / MODIFIED 1,580 / REWRITTEN 83 (producer 3's five edited `HS.c` bodies IDENTICAL ->
MODIFIED), 3,579 reviewed rows (M3-R1: 3,573); 210 MODIFIED / REWRITTEN rows still unflagged (118 outside `TitanInterface.cpp`).

**The plan for M3-R3 and the rest of M3** (**superseded 2026-10-26 by M3-R3's plan table in the M3-R3 subsection below**; docs/m3-r2-summary.md "Plan for M3-R3"; supersedes M3-R1's M3 plan table above; the
play-test can start now with `tools/play.sh --rebuild`, the brief in the summary; the overlay's F key is M3-R3's first item):

| # | step | scope | done when |
|---|---|---|---|
| 0 | **the user's next play-test** (now; the refreshed brief) | the four items re-checked (the settings line and the overlay by the Menu key, the quit at the first try after a resolution change, the Kuun-Lan's arrival gate and stern modules, FULL DETAIL's FRAME p99 at 4K batched vs `/meshImmediate`), the cutscenes with `/catFrameSig`, then M3-R1's and R10's open items | a report per item, the `frame.sig` and the frame logs handed over; M3-R3 sized |
| 1 | **M3-R3: the third fix round** (one producer + auditor per item) | item 0 the overlay's F-key toggle (F12 candidate, verified against the exe's key tables); the seven items of `cycle/answers/m3-r3-user.md` (Mission02's dust belts on the tactical map, the Bushan-Re hint dialog's trigger, a nearest-texture launch option, Mission03's white mines, the pause jitter, the mine AI, the missile corvette's four trails); the flicker's verdict from the `frame.sig`; the BTG star size / letter height / additive state if the user picks the exe's | every item shown 1-to-1 with the exe or fixed with a fence; every stream IDENTICAL; the F key in the keys self-test |
| 2 | **the Windows 11 test** (the user's day + a producer; M3-R1's row 1) | the untested win64 zip `ce95fec5…` (eb5f7e3's source) per docs/m3-windows-build.md, then a rebuild from this round's tree (`tools/release_win64.sh --engine <commit>`); the harness modes the gate needs; the libm dependence measured first | release + debug on Windows 11 with 0 warnings; the self-tests PASS there once; the Mission01 stream, both replays and a mission stream IDENTICAL to Linux (or every difference localised to a libm call); a save and a recording cross-load |
| 3 | **the one-hour 4K@144 soak, no gamescope** (producer + the user's hour; M3-R1's row 2, now instrumented) | the headless `sp` hour with `/frameLog` (the drift test, the heap growth ~4 MB/min, the `dupe` blocks 912 by Mission06), the task clock, the `DEADLYSIGNAL`, FULL DETAIL's p99 re-measured on a quiet machine, then the attended hour with the overlay | no drift over the attended hour; no growth heap in the headless hour or every growth attributed and fixed; the task clock at the exe's rate or its difference stated |
| 4 | **the oracle's rung** (M3 oracle physics; unchanged) | 10-17 2823 / 10-18 2871, the three `CAT-TODO(M3 oracle physics)`, the `frandom` sites, Mission17's KAS teams, `hsStart`'s `GL_BLEND` test; mode B and the fourth recording if made | IDENTICAL beyond 2823 / 2871 or the next divergences localised and owned |
| then | **stage 5** (decision 9) | the 23 `CAT-TODO(stage 5 ...)` sites + the modular mothership's multiplayer arrival | LAN interop nice-to-have; the wire format not broken |

### M3-R3: COMPLETE 2026-10-26 (the third play-test fix round: the overlay's F key F1 + the toggle check + SHARP TEXTURES, the pause freeze, Mission02's dust belts + the Bushan-Re hint (both the exe's), Mission03's white mines (an x86-64 layout fault) + the mine AI (the exe's), the missile corvette's four trails; presentation only, no simulation field moved)

Summary, exit test, the five fences and the refreshed play-test brief: [docs/m3-r3-summary.md](../docs/m3-r3-summary.md).
Workflow `cycle/workflows/m3-r3.js` (dae51e1; run id `wf_8a0247ed-904`, `ROUND_BASE 7aa3111`), launched 2026-10-25 15:24 on the
user's word from the user's terminal session 38027cce (Fable), not from the orchestrator; cut off by the session's end on 2026-10-25
~20:30 during the mines audit and resumed on 2026-10-26 with the same run id; 11 agents (five ports in sequence with their audits, each
checkpointing its port: settings-and-keys `c8f97bb` 0.94, pause-freeze `292a6da` 0.78, mission02 `96b0137` 0.94, mines `4ae7d91` 0.95,
engine-trails `6ad69ec` 0.944, all PASS-WITH-FIXES; integrate). The user's answers: `cycle/answers/m3-r3-user.md` (the mandate: SHARP
TEXTURES as an option with the original look the default; the overlay's key from the F1-F12 suite: F1; the GPU free 2026-10-26; the
third play-test's seven items, each now either the exe's or fixed).

**M3-R3 exit test: MET** (the integrate step, clean builds of `6ad69ec` (`[274/274]`, 0 warnings: debug `e4e1137e86218ae1`, release
`77c3861e6f74ea3d`, copied aside), plain harness, silent, 225 runs `m3r3i-e-*` in ten correctness chains + the GPU chain H after them
(the user's game on the CPU and GPU through the correctness chains, load average 21-38 through the step); `runs.txt` lines 4822-5046; ASan 0 in
all); two statements beside the verdict: the two `universePause` dumps differ from producer 2's final dumps in the paused frames' `crc` / `crcv` alone, the commit id of the build text `rndRenderTask` draws while paused (render.c:5461; chain X's captures: 71 px at the bottom right, `Emergence 1 6ad69ec` vs `Emergence 1 4ae7d91`, the object columns equal), and the 4K cost was measured after the user's game released the GPU (the GPU batch waited from 11:49 to 14:55 for the user's game to release the GPU (`gpuwait`: nvidia-smi every 10 s until 120 s at or under 40 %), then ran its eight runs on the idle GPU in two batches of four, the second after a two-minute wait on a 50 % sample; `claude-ping` fired 2 times (11:49:21, 14:57:12): the library pings once per `gpuwait` call, not once per gate, a deviation from the user's rule to fix in `chains/lib.sh` before M3-R4):
- **Mission01 headless**: pairs byte-identical, debug `c20ccda7fdae0f66` / release `316025e5c47989e1` (IDENTICAL to each other), MISSION TEST
  PASS in both (perturb at 3000 `rng10`, flip at 4000); M3-R2's list (`/noInterp`, `/noNISInterp`, `/oracleLibmSqrt`, `letterboxBehind
  1` row and switch, `/lodBands`, `noLOD 0`, `/uiScale 200`, `uiScale 150`, the three opt-outs together, `/overlay`, `--frame-log`,
  `/meshImmediate`, `/noHsAssembly`) and this round's (`texLinearFiltering 0` alone and under `effectsLevel 2`, `/sharpTextures`,
  `/noFilter`, `/exeTrails`, the console rows `engineTrails` and `texLinearFiltering` flipped live, the F key pressed at 100 / 200) all
  byte-identical to A (29 variants); **IDENTICAL in every one of the 21 fields to M3-R1's `1ae66610c3f02410`** and so to R10's
  `5bd51234405ea0d8` (decision 44 unchanged) and to every producer's and audit's Mission01 run.
- **The rung unchanged**: 10-17 IDENTICAL through 2822, first divergent **2823 `ships_vel`**; 10-18 through 2870, **2871
  `ships_vel`**; with `/oracleLibmSqrt` 177 / 118; the exclusions `rng6` at 161 in both, `real` at 2 in 10-18; the release replay equal
  to the debug on every field but `rng_all` (6377, as M3-R1 / M3-R2); both replays IDENTICAL in every field to `m3r2i-e-r17-a` /
  `m3r2i-e-r18-a` (= M3-R1's); the hwirec01 / hwirec02 playbacks IDENTICAL to M3-R2's. **Mode B not run** (no
  `original/oracle/hwirec*-play-*`), no fourth recording (the newest `oracle01-rec-*` is 20261019-143530; decision 48 open).
- **Missions 01-17**: 17 / 17 PASS with every objective in order, all 17 pairs byte-identical, ASan 0 in all 34 runs; **every stream
  IDENTICAL in every field to M3-R2's `m3r2i-e-mN-a`** (Mission17's milestone `the game's end (credits)`); Mission02's hint check
  (`hint skill=2 timer=360 ... found=1152 ... expected=no fired=no result=PASS`) and Mission03's mine checks (`level objects tick=1 mines=199 ... objectives=5/5 result=PASS`) in their lines; the Tutorial1 leg, the lesson load, the warp start, the
  failed-jump pair, loadgame, the skirmishes, build, ai and the 4,000-tick soak (20 fields; `rng_all` exempt, decision 84, and
  `rng_all` alone) all IDENTICAL to M3-R2's.
- **The sp gate**: the 1,200 s `sp` reaches Mission12 at 1024x768 (12 levels, 126 golden-shot points, 23,331 file loads, ASan 0, no unsuppressed leak) and Mission11 at 3840x2160 (119 points, 22,327 loads), both rc 124 at the timeout by design; **fe** Main_game_screen at 1024x768 and at 3840x2160 (`uiscale: ... scale 200% (auto, uiScale 0)
  logical 1920x1080`); no `DEADLYSIGNAL` line in either `sp` run's timeout-kill quit, nor in any of the 225 runs.
- **Self-tests** all PASS (statics x2, meshes, textures, oracle + the release SELFTEST pair, etg, dock, command, render, sound, hud, ui,
  fescreens `options=35` with the SHARP TEXTURES control (`optionstest: done options=35 startdefaults=35 readback=35 result=PASS`, `screens=195 ... wrong=0`), strings 1,705 / 1,705, keys 197 / 149 + the F-key line
  (`overlay fkey F1 scancode=58 vk=0x70 plain bindings F1=0 F12=1 free=1 bound-free=0 reset-free=1`), console `tablerows=486 shown=38 set=31` (producer 1's and 5's rows), build pair, nisparse, modequit (PASS), **toggle
  10 of 10** (plain, `/noInterp`, `/noNISInterp`, `/letterboxBehind`, `/lodBands`, `/overlay`, `/meshImmediate`,
  `/noFilter`, `/sharpTextures`, `/exeTrails`: the switch-label rule PASS under every one, M3-R2's one failing check closed), **uiscale
  x9** (45 / 45 checks each), missiontest pair, loadgame pair, warp pair, tload pair, Tutorial1 pair 12 / 12, ai pair + soak, skirmish
  pair + Armed8, the team colours (the user's scheme and a red one into Mission02, 7 / 7 checks; Mission03 into Mission04, 6 / 6),
  lodtest (on + `/lodBands`); every quitting run rc 0 (the nine fe-mode / sp runs at their timeouts by design); layout32 OK; the generators `--check`
  current; the win64 zip `20261026-6ad69ec` built (sha256 `3d41aaef…`).
- **The dumps** (decisions 46 / 118 / 129 stand: **no default dump moved this round**): motion `/noInterp` `c503c869f0dc9cdb` /
  `3016280e9e0dd3c8`, flags-on `c0e411662fa2f527` / `428ef41479eb5749`, nis `/noInterp` `86cb155acffc180e` / `196bab32d244f614`, flags-on
  `0307f3be2044c29c` / `a5de13a44b8d264f` (whole / crc-stripped; 1,431 / 1,440 frames), byte-equal to M3-R2's and M3-R1's files; the
  opt-outs together byte-equal to R10's `r10i-motion-noi` / `r10i-nis-noi`; the `/meshImmediate` twins equal; the two stated crc-only
  moves are the options themselves: `/sharpTextures` (`motion-sharp` / `nis-sharp` `1a2227d966ec9712` / `871eac333fd603d0` whole, crc-stripped equal to `motion-on` / `nis-on`: 1,431 / 1,439 lines differ whole, 0 crc-stripped, byte-equal to producer 1's `m3r3-sk-motion-sharp` / `-nis-sharp`), `/exeTrails` (`motion-exe` = `motion-on` and `nis-exe` = `nis-on` byte for byte (no missile corvette in either scene), `motion-exe` byte-equal to producer 5's `m3r3-et-motion-exe`); **the paused dumps** hold every
  column (0 of 71 paired changes in the key and `universePause` runs, 0 of 40 in the mid-tick one, 0 of 71 in the NIS pause (its one `crc` / `crcv` change the trail stop), `crcv` 0 and `crc` 67 (the SU blink) in the key / univ runs; the scene run's 24 of 71 are the camera's one-ulp chase (`CameraChase`, the exe's: the world columns 0 of 71, the screen columns <= 0.0019 px)).
- **The five fences** rerun with numbers (the summary's tables): (1) the keys self-test's F-key line and F1 toggling the overlay in the
  ai scene (t160 `differ=14400 bbox=2,2,227,66`, t140 / t210 0), the toggle self-test under all 10 switch sets, the sharp captures (the ai scene sharp vs linear 2,685 / 2,674 px (t200 / t240), 20,864 at 4K; the
  Kuun-Lan's closest zoom 113,372 px at 1024x768, 890,954 at 4K; the cfg row alone 0 px (the exe's preset undoes it); the live flip t200 = the switch's frame, t240 = the linear frame, 0 px each), every capture byte-equal to producer 1's
  (the Kuun-Lan, Mission01 4K and in-game Video screen frames byte-equal; the ai-scene frames differ in the corvettes' four ribbons only (chain X), the front-end Video screen in the build-identity text); (2) the paused dumps (0 of 71 paired changes in the key and `universePause` runs, 0 of 40 in the mid-tick one, 0 of 71 in the NIS pause (its one `crc` / `crcv` change the trail stop), `crcv` 0 and `crc` 67 (the SU blink) in the key / univ runs; the scene run's 24 of 71 are the camera's one-ulp chase (`CameraChase`, the exe's: the world columns 0 of 71, the screen columns <= 0.0019 px); the scene recipe 72 paused frames (F701..F772): no world column (`Aw`..`Mw`, the objects' world positions) moves in any of the 71 pairs; the camera's eye alternates by one float32 ulp (`5830.0688` / `5830.0693`) in 24 of 71 pairs, with `crcv` moving in those 24 and the objects' screen columns following the camera by at most 0.0019 px (`CameraChase`'s easing at a converged camera, the exe's; the audit measured the same); the NIS shots F602 -> F603 `differ=0` (F601 -> F602 16,179 px: the one `crc` change, the simulation's trail stop)), byte-equal to
  producer 2's dumps (7 of 9 byte-equal; the two `universePause` dumps the version string's commit id alone); (3) Mission02's 486 Dust points in every sensors frame (`sensors scenes=1794 dustpoints=871884` in `m2-a`, 0 with `ShowAsteriod0 0`), the on / off captures
  247-261 px at 1024x768, 269-275 at 4K, 0 in the 3D view, the hint at tick 5824 on NORMAL with the scout held, VERY EASY found at 1152 before the 120 s due at 1922, `expected=no fired=no PASS`, HARD `timer=-1 due=-1 ... expected=no fired=no PASS` (no timer on HARD; the self-test's player does not finish: 4 / 6), the captures byte-equal
  to producer 3's (11 of 12 frames per run; the t6272 3D-view frame 33 px at the right edge: the mines fix's missile colour); (4) the mines' textures (level `textures=11 bound=11 white=0`, laid `textures=4 bound=4 white=0`; the regions 10,657 / 10,582 and 970 / 977 colours (the BEFORE captures 19 / 6);
  batched = immediate 0 px on nine frames; vs the producer's BEFORE captures 25,389 / 25,401 / 1,341 / 1,332 px), the Recon chase (mine 63 acquired 1295, `faster-away` 1376, re-acquired 1391, `faster-away` 1392; mine 62 1294 / 1397), the Acolyte
  chase (mine 63 acquires `sAcolyte#11` at 1535, mine 66 `cleared` 1753, re-acquires 1762, its log ends 1833 (the 5 s lifetime)), the Mission03 log (199 level mines start, 30 laid, six `4->3` on the Mimic 14138-14171, four `cleared` 14391, their logs end 14470 / 14471, two level mines acquire Recons 16871 / 16875; 5 / 5 at 17,825); (5) the four ribbons (3-4 runs on the first scanline where `/exeTrails` draws 2, at 1024x768 and 4K, both corvette types; every frame byte-equal to producer 5's; the nozzles within 1.6 px of the rings' centres at 1024x768, 3.8 / 1.0 px at 4K), the random streams
  13-17 equal at every frame (0 of 321 frames differ in streams 13-17, stream 11 alone from tick 245), `/exeTrails` frames byte-equal to producer 5's (every capture byte-equal; the `/exeTrails` frames byte-equal to the audit's reverted build's), the 4K cost on the idle GPU
  after the correctness chains (the default (four ribbons) medians 3.743 / 3.422 ms (p99 5.643 / 4.886), `/exeTrails` 3.483 / 3.240 (p99 5.043 / 5.061): **the trails' 4K cost +0.221 ms on the median against a run-to-run spread of 0.321 ms**; `/meshImmediate` 8.096 / 8.003 (p99 11.699 / 13.076: the batching's gain stands), `/sharpTextures` 3.472 / 3.184 (p99 5.054 / 5.036: the same as the default within the spread)).
- **Tags**: `CAT-(TODO|PROVISIONAL)\([^)]*M3-R3` empty (BTG.c:1151 re-targeted to `CAT-TODO(M3-R4)`, comment-only, rebuilt clean with 0
  warnings); 37 `CAT-TODO` lines, 28 owner tags (stage 5 23, M3 oracle physics 3, M3 soak 1, M3-R4 1), 4 `CAT-PROVISIONAL`; `HWI-FEATURE`
  motion-interp 486 / letterbox-setting 37 / full-detail 179 / ui-scale 300 / instrumentation 141 / hyperspace-assembly 55 /
  mesh-batching 160 / **sharp-textures 196 / pause-freeze 90 / engine-trails 113**; `HWI-FIX` 76 (62 at the round base), `HWI-TEST` 461
  (286), `CAT-FIX` 196 (195).
- **UBSan kinds** (all non-fatal): 9 kinds, the same nine as M3-R2's / M3-R1's / R10's, no new kind (the misaligned particle / ETG / BTG accesses; the first report of every universe run `BTG.c:389:35`); 52 runs report nothing; ASan 0 and no unsuppressed leak in all 225.

Delta map after the round: IDENTICAL 2,757 / MODIFIED 1,580 / REWRITTEN 83 (no class change), 3,582 reviewed rows (M3-R2: 3,579; 28
`[review: M3-R3 ...]` notes merged); 210 MODIFIED / REWRITTEN rows still unflagged (118 outside `TitanInterface.cpp`).

**The plan for M3-R4 and the rest of M3** (docs/m3-r3-summary.md "Plan for M3-R4"; supersedes M3-R2's M3 plan table above; the
play-test can start now with `tools/play.sh` (it rebuilds itself), the brief in the summary; **no queue for M3-R4 yet**: it exists when
the play-test reports something or the user picks "the exe's" on an open look question):

| # | step | scope | done when |
|---|---|---|---|
| 0 | **the user's next play-test** (now; the refreshed brief) | the five items re-checked (F1 in the game, the sensors manager and the build screen; SHARP TEXTURES at the Kuun-Lan's closest zoom and TEXTURE MEMORY HI at 4K; the pause with P and the Escape menu over moving ships; Mission02's Dust points at 4K and the hint on NORMAL; Mission03's textured mines, the faster-away rule, the four streams), then M3-R2's four unreported checks (the overlay's numbers and the quit at 4K, the cutscene's `frame.sig`, the hyperspace arrival, FULL DETAIL batched vs `/meshImmediate`), then M3-R1's and R10's open items | a report per item and the `frame.sig` handed over; M3-R4 sized from it (or not needed) |
| 1 | **M3-R4: the fourth fix round** (one producer + auditor per item; only if row 0 reports items or the user picks "the exe's") | the play-test's new items; the look decisions if answered the exe's way: the BTG star size (`CAT-TODO(M3-R4)`), the letter height, the additive state, the sensors points at the UI scale, TEXTURE MEMORY HI, the unfiltered minification, the pause's float behind a setting; the flicker's verdict from the `frame.sig`; a wall-clock pause capture if motion is still seen while paused | every item shown 1-to-1 with the exe or fixed with a fence; every stream IDENTICAL; the play-test's items re-played |
| 2 | **the Windows 11 test** (the user's day + a producer; M3-R1's row 1, M3-R2's row 2) | the fresh win64 zip `20261026-6ad69ec` (this round's tree) per docs/m3-windows-build.md "What the user must test"; the harness modes the gate needs; the libm dependence measured first | release + debug on Windows 11 with 0 warnings; the self-tests PASS there once; the Mission01 stream, both replays and a mission stream IDENTICAL to Linux (or every difference localised to a libm call); a save and a recording cross-load |
| 3 | **the one-hour 4K@144 soak, no gamescope** (producer + the user's hour; M3-R1's row 2, M3-R2's row 3) | the headless `sp` hour with `/frameLog` (the drift test, the heap growth ~4 MB/min, the `dupe` blocks, O4), the task clock, the `DEADLYSIGNAL`, FULL DETAIL's p99 on a quiet machine (this step's idle-GPU figures the baseline), then the attended hour with the overlay (F1) | no drift over the attended hour; no growth heap in the headless hour or every growth attributed and fixed; the task clock at the exe's rate or its difference stated |
| 4 | **the oracle's rung** (M3 oracle physics; unchanged) | 10-17 2823 / 10-18 2871, the three `CAT-TODO(M3 oracle physics)`, the `frandom` sites, Mission17's KAS teams, `hsStart`'s `GL_BLEND` test, the non-ship statics' layout audit; mode B and the fourth recording if made | IDENTICAL beyond 2823 / 2871 or the next divergences localised and owned |
| then | **stage 5** (decision 9) | the 23 `CAT-TODO(stage 5 ...)` sites + the modular mothership's multiplayer arrival | LAN interop nice-to-have; the wire format not broken |

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
    content, the header's build identity only) is in docs/m2-r9-summary.md. **M2-R10: the content is unchanged** (the integrate's `5bd51234405ea0d8`, release `a44d585d71233c4a`, IDENTICAL in every field to `553845b65e87f3b5`: no ship flies in the headless Mission01 run, the flight / combat ports do not reach it); every other guard rail re-baselined in docs/m2-r10-summary.md. **M3-R1: the content is unchanged** (the integrate's `1ae66610c3f02410`, release `234bcccfb948ab98`, IDENTICAL in every field to `5bd51234405ea0d8`; presentation only: no simulation field of any stream of the round moved; decision 119).
    **M3-R2 (2026-10-24): the content is unchanged** (decision 128): the integrate's `2131271d771f2056` (release `ce30c53259f034c8`) is IDENTICAL in all 21 fields over ticks 0-6000 to M3-R1's `1ae66610c3f02410` and so to R10's `5bd51234405ea0d8` (the header's build identity only), with every setting of the round flipped; the rung 2823 / 2871 unchanged.
    **M3-R3 (2026-10-26): the content is unchanged** (decision 140): the integrate's `c20ccda7fdae0f66` (release `316025e5c47989e1`) is IDENTICAL in all 21 fields over ticks 0-6000 to M3-R1's `1ae66610c3f02410` and so to R10's `5bd51234405ea0d8` (the header's build identity only), with every setting of the round flipped (the sharp row, the switches, the console rows, the F key); the rung 2823 / 2871 unchanged.
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
    `6a4c1850799187ac`. **M3-R1: re-baselined** (decision 118): the default dumps are now motion `/noInterp` `c503c869f0dc9cdb` / flags-on `c0e411662fa2f527`, nis `/noInterp` `86cb155acffc180e` / flags-on `0307f3be2044c29c` (crc-stripped `3016280e9e0dd3c8` / `428ef41479eb5749` / `196bab32d244f614` / `a5de13a44b8d264f`): the `crc=` column moved for the bars in front (producer 1, the nis pair: the sRecon's two passes through the bottom bar) and for the LOD-0 meshes (producer 3, all four), the nis flags-on dump's positions for the NIS task's alpha fix (producer 4, fix 1); the opt-outs `letterboxBehind 1` + `/lodBands` + `/noInterp` reproduce `183681235a3fdc7b` / `13ca70eea998e5de` byte for byte (`cmp` against `r10i-motion-noi` / `r10i-nis-noi`); no position column of a `/noInterp` dump moved in the round (the crc-stripped `3016280e9e0dd3c8` / `196bab32d244f614` are R10's).
    **M3-R2 (2026-10-24): unchanged** (decision 129): the four dumps byte-equal to M3-R1's references (decision 118) and the opt-outs together to R10's; the `/meshImmediate` twins byte-equal to the batched path's; no producer moved a dump.
    **M3-R3 (2026-10-26): unchanged** (decision 141): the four dumps byte-equal to M3-R2's references (decision 129) and the opt-outs together to R10's; the two stated crc-only moves are the round's options (`/sharpTextures`, `/exeTrails`); the paused dumps of the pause freeze a gate.
47. **Oracle mode A's exclusions**: `ships_rot` (the DLL offset defect), `rng_all`, `fpcw`, `players` (the stock
    record / playback difference at tick 93; `rng6`'s at 161: the playback skips the AI). The comparison is the
    simulation field list over ticks 1..9600. **M2-R9**: unchanged (`rng6` at 161, `players` at 93 / 272 in the 10-18
    one); the rung's list is `rng10,rng6,ships_n,ships_misc,ships_pos,ships_vel,ships_health,missiles,bullets,derelicts,
    resources,univcheck,dt,total,real` minus the exclusions (`real` too for 10-18).
48. **The oracle DLL's `CAT_SHIP_coordsys` should be 0x98, not 0x80** (tools/oracle/dll/hook_sites.h); the DLL was not
    run or rebuilt this round. A one-line edit before the user's next recording. **M2-R9: still no fixed-DLL recording**
    (`FOURTH_RECORDING null`); `ships_rot` stays excluded in every session (it diverges at tick 1); both open divergences
    (1922: a turning ship's coordsys, 1721) would be helped by it: recommended before M2-R10's rung producer. **M2-R10: still no fixed-DLL recording** (`FOURTH_RECORDING null`; the newest `original/oracle/oracle01-rec-*` is 10-19's); both next divergences (2823 / 2871: a bProcessor following the turning bWorker 44) are the Worker's coordsys, invisible without it: recommended before M3's rung producer (decision 100).
49. **The x87 single rounding of `cat_clExternalBuildUpdate`**: MADE in M2-R7 (`clExternalBuildAdvance`: the increment
    in double, rounded once at the store, the clamp on the double sum); the tick-136 divergence is gone and the oracle's
    first divergence is now **tick 177 `ships_vel`** (ship 35's turn from tick 171: the table `fsqrt` / x87 vector helpers,
    M2-R8's first item, decision 56) in the 10-17 recording and tick 118 `ships_pos` / `ships_vel` in the 10-18 one.
    **M2-R8**: 247 / 155 with the table switch. **M2-R9**: on the table default the first divergences are **1922 `ships_vel`**
    (10-17, ship 41's attack flight) and **1721 `bullets`** (10-18, GOG fires 19 ticks earlier); 177 / 118 with
    `/oracleLibmSqrt`; M2-R10 physics' first item (docs/m2-r9-summary.md "Oracle mode A"). **M2-R10**: on the table default the first divergences are **2823 `ships_vel`** (10-17, ship 48) and **2871 `ships_vel`** (10-18, ship 49); 177 / 118 with `/oracleLibmSqrt` (unchanged); decision 100.
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
    (the virtual audio clock, decision 93). **M2-R10**: stands; the combat measured and deferred to the attended play-test (decision 109); the mission runs on the virtual audio clock (a no-op with sound off).
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
    unrounded returns (`CAT-TODO(M2-R10 physics)`) and the rest of Blobs.c. **M2-R10**: 2823 (10-17, ship 48 `ships_vel`) and 2871 (10-18, ship 49 `ships_vel`) on the table; 177 / 118 with `/oracleLibmSqrt`; the two `CAT-TODO(M2-R10 physics)` closed (double returns); the three `CAT-TODO(M3 oracle physics)` (decision 99) and the next divergences (decision 100) are M3's.
83. **The flight path's further x87 shape** (producer 1): the collision blobs by collision position, the blob attacking flag
    (`shipisattacking` AND `catAttackStarted`), `ObjectsCollided` with the unrounded `1/t`, `collCheckBigShipSmallShipColl`'s
    clamped-host test, `cat_dockFlyOutOfCone`, `cat_gunFixedGunAimedAtTarget`, `hyperspaceCost`'s double return at its five
    callers, `protectShip` / `protectShipsAvg`; `catFmathSqrtDouble` returns 0.0 for a NaN as the exe's `test ah,0x40`.
84. **The ai soak's `rng_all` is not reproducible under load** (the sound stream on the mixer thread's wall clock; the `ai`
    mode does not silence it); the 400-tick ai pair is unaffected; the soak is judged on `result=PASS`, ASan 0, no fatal.
    **The soak moved once more inside R9** at tick 694 `ships_health` (producer 3's ship-part blast fix: the first death),
    bisected by the integrate step; no producer after the rung re-ran it: the next rounds' producers re-run the soak. **M2-R10**: the soak re-run by producers 1 and 3 and the integrate: IDENTICAL to each other (`bf8b4579fb36b512`), moved against R9 from 431 by the combat ports only; the load flake did not recur (not investigated: the `HWI_EXP_RNGLOG` experiment binary names the stream if it does).
85. **New non-fatal UBSan kinds**: `Select.c:1153` / `1154` signed overflow in `selCircleCompute` (sp and the Mission06 /
    12 / 14 / 15 / 16 self-tests), `FOW.c:296-437` `-2147483648 - 2` in `fowActivateCube` / `fowDeactivateCube` / `fowCubeHasUnvisited`'s cell boxes (lines 296 / 349 / 435, from the W wrappers' world-to-cell conversion) (Mission17 only);
    the recorder's packet writes in `CommandNetwork.c` / `Queue.c` (a record run); all M2-R10. **M2-R10: closed** (`Select.c` by `selCircleCompute`'s port, `FOW.c` by the cell boxes' `CAT-FIX`, decision 103); the recorder's packet-write kinds remain in a record run (none this integrate ran).
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
    is judged on the running universe, ASan 0 and the levels walked (01 -> 02 -> 03 -> 04_NIS02 -> 04). **M2-R10: the stop is gone** (decision 101): `sp` walks Mission01 .. 12 in 1,200 s (01 .. 09 in 900 s) with ASan 0, 127 screenshots, no assertion; judged on the running universe, ASan 0, the levels walked and the `animatics:` line.
91. **The end of the game**: Mission17's `MissionCompleted` 0x00491c70 -> `singlePlayerMissionCompleteCB` 0x0051be40 ->
    `utyGameQuitToMain` 0x005ab6b0's credits (`Credits-GameEnd.cred`), reached headlessly. `animBinkPlay` is GoK's stub (no
    animatic plays); `animAviPlay` is 0x00421860's port: M2-R10 routes the calls, the headless tests skip them (default). **M2-R10: routed** (decision 102): every caller reaches `animAviPlay`; the headless runs skip with a diagnostic line (`animatics: 12 skipped (headless)` in the sp run); the attended check is the user's.
92. **The harness** (decision 80 extended): modes `warpstart` (`--warp-level N`), `missiontest --mission N`, `loadgame
    --training --save-src DIR`; `--record NAME` / `--record-stop N` (skirmish); 29 modes; switches `/catMissionTestMission`,
    `/catMissionTestTrace`, `/catWarpStartTest`, `/catLoadGameTraining`, `/oracleRecord`, `/oracleRecordStop`,
    `/oracleResave`, `/catVirtualAudio`, `/oracleLibmSqrt`. Never edit `tools/m2_harness.sh` while a run is in flight.
    `hwo_compare.py` wants `--from-tick 1` between a record run and a playback. Count tags with `/bin/grep`. **M2-R10**: `--fail-jump` (missiontest, with `--mission N`), `--skirmish-bigotry N`, the `animatics:` summary line; switches `/catMissionTestFailJump`, `/catSkirmishBigotry`; still 29 modes (decision 110). **M3-R1**: mode `uiscale` (30 modes), the flags `--shot`, `--shot-letterbox`, `--res`, `--cfg`, `--player-colours`, `--teamcolour`, `--lodtest`, `--lodstats`, `--scene`, `--real-clock`, the capture / colour / LOD / scene / UI-scale switches and tools (decision 117).
    **M3-R2 (2026-10-24): extended again** (decision 127): 31 modes with `modequit` (`--mode-to WxH`); `--frame-log`, `--frame-sig`, `--virtual-fps N`; the `settings:` summary line; each step prefixes its labels (the harness replaces an existing label's directory).
    **M3-R3 (2026-10-26)**: decision 139 (this round's switches through `--`, the `mothership` scene kind, the `preset` source).
93. **The Tutorial1 leg on the virtual audio clock** (`/catVirtualAudio`, set by `/catMissionTestTutorial`: no device, no
    stream thread; `soundVirtualAdvance` mixes 22,050 samples per second of task time in 256-sample blocks); the narration
    takes its game-time length, the leg ends at 10,444 ticks, byte-identical run to run; the 12 lesson saves written and
    "LESSON 11" loads. Names: `soundVirtualClock`, `soundVirtualAdvance`, `isoundstreamstep`. A normal run is unchanged.
94. **Mission06's rare `rng_all`-only divergence** (the mixer thread's `RANDOM_SOUND` on the wall clock suspected): the
    mission runs keep the wall-clock mixer this round and are judged on the simulation fields when `rng_all` alone differs
    (the integrate's pair: 2081); M2-R10 adopts the virtual audio clock for them (every mission's `rng_all` moves once) or
    the oracle excludes the sound streams. **M2-R10: answered by a fix** (decision 105): the cause was the nebula tendrils' uninitialised bounding sphere (`nebInitTendril`, a `CAT-FIX`), not the mixer; the mission runs set the virtual audio clock (a no-op with sound off); Mission06 is byte-identical over six runs.
95. **The recorder's record side** (stage 5 reduced): the 4 sites closed at the exe's bodies; hwi-engine records skirmish /
    multiplayer games in cataclysm.exe's format (the save at 0x10ad0027 through the SaveCat32 writer, the `.pkts` file);
    `/oracleRecord <name>` / `/oracleRecordStop <tick>` follow the DLL's record mode (single player refused); a recording's
    replay is IDENTICAL to its record run except `rng0` / `rng6` / `rng_all` (the computer player does not run in a
    playback); the sync checksum's lag is 1 packet (GOG's real-time sessions: 8); `/oracleResave` for the writer's byte
    check; two recordings of the same game differ in the save's stale pointer slots and the `.pkts` header's uninitialised
    tail word (M2-R10). Names: `recordingStartSave`, `recPackDeleteFile` (HWI-FIX), `saveCat32*`, `madCat32ObjectFileOffset`,
    `aitCat32SaveTeam`; tools `tools/formats/pkts_probe.py`, `tools/formats/recording_compare.py`. Only a recording's save
    is written in the exe format; the recorder's files live in the user settings path. **M2-R10**: the `.pkts` tail word and the command padding zeroed (`CAT-FIX`, decision 108): two record runs' `.pkts` byte-identical; mode B still the user's step (no `original/oracle/hwirec*-play-*`): the commands in docs/m2-r10-summary.md "What the user must decide" 2 with this step's `r10i-play01-a` / `r10i-play02-a` as the right-hand sides.
96. **Stage 5 deferred** (decision 9): a recorded game sends nothing for the external builds and the point / mimic / upgrade /
    special-target orders (CommandWrap.c's `CAT-TODO(stage 5)`), in the record run and the replay alike;
    `recPackRecordSaveHeader`'s HEAD header, `KeepAliveDropPlayerCB` (LAN) and `mgUnitCapsEnabled` (options) tagged.
97. **`Subtitle.c:1144` closed** (CAT-FIX at `subTitlesUpdate` 0x005842f0: the location cards' actor 0x70 starts from
    `STT_LocationCard`); the reviews of the 123 stage-4 IDENTICAL bodies, the 31-row sample (2 ported, 3 tagged, 15
    IDENTICAL) and 42 notes on edited rows are in the map (`m2r9:recorder-leftovers`).
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
    `trTextureRegister`'s name case (the exe uppercases, GoK does not) stays until a texture is found registered twice. **M3-R1**: `trRegistryRefresh` 0x00587a50 lists only pending entries as the exe (decision 112): a loaded texture is never shrunk or reloaded at a level change (the team colours across the warp; 473 fewer file loads over Mission01 + Mission02); the colour-info leak O4 now 1,587 / 1,898 live blocks (the soak's baseline).
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

111. **The letterbox bars in front** (M3-R1 producer 1, item 1): the cause was the M2-R6 port of `cat_popupMenuDraw`
    0x004ee120, which restored the depth test to TRUE where the exe passes the never-written word 0x00a57628 (0: the
    depth test stays off for the rest of the frame, so `rndDrawScissorBars` 0x0057a410's late bars cover the scene); one
    line (`CAT`). **The exe never clips the 3D view** (`rndScissorEnabled` is the GL DLL's `glIsEnabled`, FALSE every
    frame there too). `rndDrawScissorBars`: the fading bars blend normally (`CAT`) and the additive state is restored
    (`CAT-FIX`: the exe's full-bars branch restores `ebx`, the top bar's GL y, and leaves additive blending ON for the
    subtitles / cursor drawn after the bars: the user's call whether to keep the exe's look, the audit's O1). **Setting
    `letterboxBehind`** (`HWI-FEATURE(letterbox-setting)`): `Cataclysm.cfg` row `letterboxBehind 0|1`, default 0 (the
    bars in front); the run-only switch `/letterboxBehind`; the console row (HWI row id 0x1002); no Options control
    (the task asked for none). With it the M2 look is back bit for bit (this step: the frame at NIS n06 dump frame 1374
    EQUAL to ce4fab4's with `letterboxBehind 1` and `/lodBands`, differ=0). The letter height stays GoK's
    `NIS_LetterHeight` (277 rows at 3840x2160; the exe's `(H - W*20/37)/2` gives 42): the user's question; a one-line
    change if the exe's is chosen (owner M3-R2).
112. **The team colours across the warp** (producer 2, item 2): the "warp" was the jump from the opening cutscene's
    staging level into Mission01; the registry and the scheme were right on both sides, but the tree's
    `trRegistryRefresh` kept HW1's loop listing the already-LOADED textures, so `trCramRAMComputeAndScale` 0x00589a50
    halved them to the 6 MB budget (TextureDetail 50) and `trPreLoadedTextureScale` 0x00586bd0 reloaded them with a stale
    listing-flags word (no team bit): 240 textures of the Kuun-Lan, its modules, the Worker and the Recon came back
    untinted. **The exe lists only PENDING entries** (`trRegistryRefresh` 0x00587a50: `test al,4; je; test al,1; je
    0x00588246` at 0x00587d1c / 0x00587d24): a loaded texture is never shrunk or reloaded at a level change; ported as
    `CAT` (the audit replaced the producer's `CAT-FIX`); loaded textures keep their size and palettes into the next level
    (473 fewer file loads over Mission01 + Mission02). No setting. HWI-TEST `/catTeamColourTest`, `/catTeamColourTicks N`
    (harness `--teamcolour`, `--player-colours BASE/STRIPE`; `teamcolour:` lines; texreg.c `catTrPaletteTeamFlags[]`):
    34 / 34 checks PASS over the whole `sp` chain with the user's scheme (30 FAIL without the fix). Heap input for the
    M3 soak: the "dupe trcolorinfo list" leak (O4, one 64-byte block per texture reload, the exe alike) is 1,587 live
    blocks at Mission09 and 1,898 at Mission12_NIS07 with the gate (2,437 / 3,308 before it). No stream or dump moved.
113. **FULL DETAIL = LOD 0 always, default ON** (producer 3, item 3; the user's rule). The exe's FULL DETAIL
    (`opNoLODVal` 0x00a4cc34) has one reader outside the options code, `rndMainViewRenderFunction` 0x005764e0 at
    0x00576c03: it ends the frame-time detail passes after pass 0; `lodLevelGet` 0x004a4d70 never reads it and still drops
    small / far objects to levels 1..5 by the size-on-screen bands from the DetailQuality radio (the user's "noLOD 1,
    models still change" is the exe's behaviour); the detail slider is a frame-rate floor, greyed while FULL DETAIL is
    on. `HWI-FEATURE(full-detail)`: `cat_lodDrawLevel` / `cat_lodDrawLevelGet` (LOD.c) give the level DRAWN (0 under
    FULL DETAIL when the table has a mesh at 0) while `currentLOD` keeps the exe's level for every non-draw reader
    (`renderedLODs`, guarded by the audit to the exe's mesh path: KAS `RenderedShips` / `RenderedDerelicts`,
    `aitCheckAmIBeingWatched`; the effect levels; the droid runtime; the trail style; the ACV link bolts; the ship
    lightning width; the selection and status bars; the Beast-stun edge lightning, stream 0). **Default ON**: `noLOD 1`
    (the exe's own row and FULL DETAIL checkbox; the high preset 1, the low 0); the run-only switch `/lodBands`; the
    console row `noLOD` (HWI row id 0x1003). HWI-TEST `/catLodTest <tick>[:far][:fleet]` (harness `--lodtest`),
    `/catLodStats <from>-<to>[:fleet]` (`--lodstats`). **The effects cap is not lifted** (Relic's per-effect limits
    applied in the universe update; the high preset already 256): M3-R2 if the user wants it. **The 4K cost** (release,
    offscreen, idle GPU, 164 ships): ~10 ms median / ~15 ms p99 on (25,169 vertices) against ~3 ms off (4,726), vertex
    submission (the same at 1024x768): owner M3-R2 (high-refresh-4k). Trails at the LOD-0 style would move `rng_all`:
    the user's call. The default motion / nis dumps move in `crc=` only (the LOD-0 meshes), the streams not at all.
114. **The unfocused objects' jitter** (producer 4, item 4): the user's 2026-10-20 play-test ran with Smooth motion OFF
    (`original/play-home/.homeworld/Cataclysm.cfg` rows 107-108 `objInterpolate 0` / `nisInterpolate 0`, written
    2026-10-20 20:09; the user did not know the checkbox existed: cycle/answers/m3-r1-user.md). Two write paths: the
    M2-R6 builds of 2026-10-18 persisted `/noInterp` (the user's four 10-18 sessions), and a live bug: the Smooth motion
    checkbox, a copy of OP_NoLOD whose `.fib` flags carry FAF_CallOnDelete, ran `opSmoothMotion` at the screen's deletion
    too (Cancel did not cancel; an untouched Video screen closed with Accept under `/noInterp` wrote 0): fixed, only a
    click sets the flags (the toggle test's new `screen untouched` / `screen cancel` checks). **Three real-clock residuals
    fixed** (`HWI-FEATURE(motion-interp)`, no setting): (1) the render task runs before the universe task, so one frame a
    tick had alpha above 1 and was drawn at the tick: `MI_ALPHA_MAX` 2, the frame drawn at its own time (exactly 1
    unchanged; the NIS curves at the render time); (2) GoK's 4 ms task clock with a millisecond remainder gave alpha
    steps 0.100 / 0.117 / 0.133: `taskClockSample` / `taskSubTickRender` read SDL's performance counter (the exe's QPC /
    240 task clock itself stays `CAT-TODO(M3 soak)`); (3) the dock latch: a ship clamped during a tick jumped to its
    clamp point and stood eight frames: `MiEntry.clamped`, the newly clamped child keeps its own blend for that tick.
    The user's Recon B on the real clock at 144 Hz: repeated frames 41 / 933 -> 0, jerk p95 0.143 -> 0.010 px (`/noInterp`
    825 / 933, 1.49 px); the NIS camera jerk p95 39.99 -> 5.58 (this step's rerun: B 0 / 941, 0.008; NIS O9 0.322,
    camera 5.811). Ruled out with numbers: alpha drops, objects the capture misses, teleport false positives, the camera,
    draws outside the bracket (one tutorial-pointer case owned by M3-R2), frame pacing (the M3 soak's). The measurement:
    `/catMotionScene` (harness `--scene recon|follow:H:D:TICKS`: the user's recipe on LiveFireExercise2 plus a crossing
    ship, a formation, a docking Worker, a harvesting Worker, the mothership's clamped module, an asteroid, a derelict),
    `/catMotionRealClock` (`--real-clock FPS`), `tools/nis_smoothness.py` `--span` / `--offscreen` / `--world` /
    `--objects` / `--jerk` and its clock line. Defaults unchanged (both flags on; the switches run-only). No stream moved;
    the `/noInterp` dumps and the flags-on motion dump EQUAL; the flags-on nis dump moved by fix 1 (160 of 1,440
    frames, one a tick from tick 432: the NIS task's phase).
115. **The UI scale** (producer 5, item 5; the user's rule): the exe has one size (MAIN_WindowWidth / Height are the
    client size and the render size; its 2D path normalises by them: at 3840x2160 the 640x480 front end is a sixth of
    the screen); the front end centres on `(MAIN - 640) / 2` (`feAllScreensReposition` 0x00474ae0), the HUD anchors on
    the edges. `HWI-FEATURE(ui-scale)` (src/SDL/uiscale.c / .h): three sizes: the window REQUEST (the cfg's
    `screenWidth` / `screenHeight`, kept in uiscale.c), the PHYSICAL size (the drawable: `MAIN_WindowWidthActual` /
    `HeightActual`: the GL viewport, every scissor box and read-back, the 3D projection's aspect, the stars and point
    sizes) and the LOGICAL size (physical / scale, rounded: `MAIN_WindowWidth` / `Height` once the window exists: every
    2D layout and draw, the mouse, picking). **`uiScale` 0 = auto** (200% at a physical height >= 2160, 150% at >= 1440,
    100% below), 100 .. 400 fixed, capped at a 640x480 logical screen (1024x768 at 200 -> 160%); logical = round(physical
    / scale) with the exact per-axis factor in the projection and the mouse (2560x1440 -> 1707x960); a window pixel maps
    to the logical pixel its centre falls in; nearest filtering at every scale (at 2.0 an exact pixel-doubling); the 2D
    line widths and the pie plate's lines / stipple follow the scale; the 3D view is the physical size's. Surfaces: the
    cfg row `uiScale`, the run-only switch `/uiScale N`, the console row (HWI row id 0x1004, `CAT_CON_HWI_ROWS` 5), the
    Video-options control "UI SCALE" AUTO / 100% / 150% / 200% (`OP_UiScale` radio buttons copied from OP_Effects on
    Video_Options and the in-game Video screen; applied at once at a front-end Accept, at the next start from a game;
    the audit placed its atoms at their 640x480 coordinates after the screen is centred and relaid the mode switch out at
    the request). HWI-TEST `/catUiScaleTest` (harness `--mode uiscale`, 45 checks: the scale and logical size, the fit
    over nine desktops and the display choice, the mouse at the bottom row / corner / outside, a scripted click at the
    OPTIONS button's centre and one logical pixel right of it, the control's buttons, a real ACCEPT applying 200%, the
    row / switch / console); `tools/ui_scale_compare.py` (`--preset fe|hud`, `--region`, `--tol`, `--sample area|centre`).
    Fence: the 4K front end box-downscaled 2x equals the 1080p frame to the bit; the 4K HUD matches 0.9996+; the 1440p
    frame centre-sampled equals the 1707x960 frame to the bit; 1024x768 unchanged. `CAT-TODO(M3-R2)`: `btgConvertStar`'s
    background-star size (the exe's a fixed share of the screen, GoK's a fixed pixel size). No stream or dump moved.
116. **The window at 4K** (producer 5, item 6; `HWI-FIX`): the exe's windowed mode makes the client size plus the frame
    at the screen's origin (`WinMain` 0x005631f0), so a desktop-sized request is partly off-screen there too; the fit
    (`uiScaleWindowFit`): a windowed request of the display's size or larger -> SDL fullscreen-desktop on the display
    chosen by the request's size (borderless, above the panel); larger than the usable bounds but smaller than the
    display -> shrunk to the usable bounds keeping the aspect; else as requested; `/noBorder` = the exe's WS_POPUP
    (`CAT`); `mousePositionSet` 0x0056e8d0's clamp ported (`CAT`); the poll clamps to the logical screen, so the physical
    bottom row lands on the logical bottom row (the user's item 6: the mouse could not reach the bottom ~200 rows under
    COSMIC's title bar + panel). `/testUsableBounds WxH[@DWxDH]` forces the bounds in a harness run (`window:` summary
    line). Open: GoK's `mainDestroyWindow` clears `mainActuallyQuit`, so the next quit's first SDL_QUIT after a
    resolution change is ignored (owner M3-R2); an intermittent `AddressSanitizer:DEADLYSIGNAL` in the quit path after
    a timeout kill of a 4K `sp` run (once in four in the ui-scale round; none in this step's two; owner M3 soak).
117. **The harness** (decision 92 extended; `tools/m2_harness.sh`, 30 modes): mode `uiscale`; flags `--shot SPEC`
    (`/catShot`: `<prefix>.<tick>.<frame>.ppm`, `.s<N>.` after a level load), `--shot-letterbox` (`/catShotLetterbox`),
    `--res WxH`, `--cfg 'KEY VALUE'`, `--player-colours BASE/STRIPE`, `--teamcolour`, `--lodtest SPEC`, `--lodstats SPEC`,
    `--scene SPEC`, `--real-clock FPS`; switches `/catShotFile`, `/catNisScissorOut <tick>:<s>`, `/catTeamColourTest`,
    `/catTeamColourTicks`, `/catLodTest`, `/catLodStats`, `/catMotionScene`, `/catMotionRealClock`, `/catUiScaleTest`,
    `/testUsableBounds`, `/letterboxBehind`, `/lodBands`, `/uiScale`; the summary lines `window:`, `uiscale:`,
    `teamcolour:`, `lodtest:`, `lodstats:`, `scene:`; tools `ppm_region.py` (info, stats, probe, diff, down2, bands),
    `ui_scale_compare.py`, `nis_smoothness.py`'s scene options. Never edit the harness while a run is in flight; count
    tags with `/bin/grep`; GPU measurements on an idle GPU only (nvidia-smi before and after; the CPU load stated:
    this step 12-20 from the user's ripgrep); `tools/play.sh`'s header documents every setting of this round.
    **M3-R2 (2026-10-24): extended** (decision 127): mode `modequit` (`/catModeQuitTest`, `--mode-to`); flags `--frame-log` (`/frameLog`), `--frame-sig` (`/catFrameSig`), `--virtual-fps N` (`/catVirtualFps`); `/overlay`, `/noHsAssembly`, `/meshImmediate` through `--`; the `settings:` line in every summary.
    **M3-R3 (2026-10-26)**: decision 139.
118. **The guard-rail dumps re-baselined** (decision 46 updated; the integrate's `m3r1i-motion-*` / `-nis-*`): the references are
    now motion `/noInterp` `c503c869f0dc9cdb` (crc-stripped `3016280e9e0dd3c8`), motion flags-on `c0e411662fa2f527` (`428ef41479eb5749`),
    nis `/noInterp` `86cb155acffc180e` (`196bab32d244f614`), nis flags-on `0307f3be2044c29c` (`a5de13a44b8d264f`), every one EQUAL to
    producer 5's audited hashes, with the producers' stated reasons: producer 1 moved the two nis dumps' `crc=` column only (the
    bars drawn in front: the sRecon's two passes through the bottom bar, 279 / 277 of 1,440 frames), producer 3 moved every dump's
    `crc=` column only (the ships drawn with their LOD-0 meshes: motion 603 / 605 of 1,431 frames, nis 1,260 / 1,267 of 1,440),
    producer 4 moved the nis flags-on dump's positions and crc (fix 1: the NIS task's alpha past 1 in the frame whose pass runs the
    next NIS tick; 160 of 1,440 frame lines); producers 2 and 5 moved nothing. The three opt-outs together (`--cfg 'letterboxBehind 1'
    -- /lodBands /noInterp`) reproduce R10's `/noInterp` dumps byte for byte (`183681235a3fdc7b` / `13ca70eea998e5de`: `cmp` equal to
    `r10i-motion-noi` / `r10i-nis-noi`; against the default only the `crc=` column differs, 603 of 1,431 and 1,260 of 1,440 frames):
    the old picture is still there under the settings. No position column of any `/noInterp` dump moved in the round (the
    crc-stripped hashes are R10's). The rule stands: a dump moves only with a stated reason, and the next round's integrate checks
    these four and the opt-out pair.
    **M3-R2 (2026-10-24): the references stand** (decision 129): the integrate's four dumps byte-equal to these hashes, whole and crc-stripped (1,431 / 1,440 frames); the `/meshImmediate` twins too; nothing re-baselined.
    **M3-R3 (2026-10-26): the references still stand** (decision 141): the integrate's `m3r3i-e-motion-*` / `-nis-*` byte-equal to these files.
119. **Decision 44 unchanged, the rung unchanged** (the integrate step, from clean builds of `58df92d`): the headless
    Mission01 stream `1ae66610c3f02410` (release `234bcccfb948ab98`) is IDENTICAL in all 21 fields over ticks 0-6000 to
    R10's `5bd51234405ea0d8` / `a44d585d71233c4a` (and to R9's `553845b65e87f3b5`), with `/noInterp`, `/noNISInterp`,
    `/oracleLibmSqrt`, `letterboxBehind 1` (row and switch), `/lodBands`, `noLOD 0`, `/uiScale 200`, `uiScale 150` and the
    three opt-outs together all byte-identical to A; `mission_test.sh` PASS in both builds. The oracle's rung: 10-17
    first divergent **2823** `ships_vel` and 10-18 **2871** `ships_vel` (177 / 118 with `/oracleLibmSqrt`; the release
    replay equal to the debug on every field but `rng_all` at 6377), both replays IDENTICAL in every field to
    `r10i-r17-a` / `r10i-r18-a`; mode B still not run (no `original/oracle/hwirec*-play-*`); no fourth recording (the
    newest `oracle01-rec-*` is 20261019-143530; decision 48 open). **Presentation only** held: no simulation field of any
    oracle stream of the round moved (the 17 missions, the guard rails and the replays IDENTICAL to R10's).
120. **Tags and the delta map after M3-R1**: no `CAT-TODO` / `CAT-PROVISIONAL` names M3-R1; 37 lines carry `CAT-TODO`
    (R10: 35), 28 owner tags (26): stage 5 23 (plain 8, multiplayer options 8, LAN 6, multiplayer 1), M3 oracle physics
    3, **M3 soak 1** (utility.c's task clock, producer 4), **M3-R2 1** (BTG.c's star size, producer 5); 4
    `CAT-PROVISIONAL`; `HWI-FEATURE(motion-interp)` 484 (441), `HWI-FEATURE(letterbox-setting)` 36,
    `HWI-FEATURE(full-detail)` 178, `HWI-FEATURE(ui-scale)` 299; `CAT-FIX` 195 (194), `HWI-TEST` 222 (143), `HWI-FIX` 53
    (27). The delta map: IDENTICAL 2,762 / MODIFIED 1,575 / REWRITTEN 83 (`rndDrawScissorBars` found modified), 3,573
    reviewed rows; 210 MODIFIED / REWRITTEN rows unflagged (118 outside `TitanInterface.cpp`); review notes `[review:
    M3-R1 <port>]` 40 (letterbox-front 6, team-colours-warp 11, full-detail 5, motion-jitter 1, ui-scale 17) + 9
    `[audit: M3-R1 <port>]` notes. Round size: 66 files, +7,891 / −217 (`src/` 44 files, +4,080 / −95; new
    `src/SDL/uiscale.c`, `src/Game/TeamColourTest.c`, `src/Game/UiScaleTest.c`; `[273/273]`).
121. **The exit-test rules carried into M3-R2** (this step): every port and audit on the plain harness from a clean build;
    every stream judged with `hwo_compare.py` over the 21 fields (never by sha256 across builds: the header carries the
    build identity); the dumps compared whole and `crc=`-stripped, a crc-only move stated as a render change with its
    frame count and reason, a non-crc move stated as a render-timing change (fix 1's 160 frames); the five fences'
    numbers quoted from the harness's lines; GPU measurements only on an idle GPU with nvidia-smi before and after and
    the CPU load stated (`claude-ping` once if the user's game holds the GPU; `claude-gpu-done` after the last one); the
    user's answers in `cycle/answers/<round>-user.md`, read again before each report; the "USER:" questions of a report
    quoted verbatim in the summary.
    **M3-R2 (2026-10-24): carried and extended** (decision 132): plus a frame-time p99 only from a quiet CPU (the load average stated), each step's own label prefix, a "not reproduced" item's done-when (its instrument proven on a forced frame plus the attended run's file), and a gate self-test run under every switch the gate names before the round closes (this step's `/letterboxBehind` toggle run failed one M3-R2 check nobody had run: the check's expectation, `src/Game/MotionTest.c:2128-2131`; owner M3-R3).
122. **The instrumentation** (M3-R2 producer 1, `HWI-FEATURE(instrumentation)`, `src/Game/Instrument.c`): the `hwi-settings:` line
    is printed at start-up (once the window exists) and at every change, one row per setting with its source (default / cfg / switch /
    console / options / key) and a `[why]` tag naming the trigger (`[start]`, `[options accept]`, …); the overlay is off by default,
    toggled by the console row `overlay` (`CAT_CON_HWI_ROW_ID + 5`), the run-only `/overlay` and the Menu key
    (`SDL_SCANCODE_APPLICATION`: no VK code, so `keys.cfg` cannot bind it and the exe never uses it), drawn top-left (logical 4,4) at
    the UI scale with `default.hff`, no options-file row; `/frameLog <file>` (HWI-TEST, run-only; harness `--frame-log`;
    `tools/frame_log_report.py`: p50 / p95 / p99 / max, the frames over one refresh, the drift test, the heap growth a minute, the
    seconds over budget); FRAME is the period between two ends of the render task after the flip, RND the render task alone, neither
    with `glFinish` (the lodstats ms is RND + `glFinish`); producers add a setting by appending a row to `hwiSettings[]`. The console's
    counts shown 35 / set 28 after producer 1, 36 / 29 with producer 4's row (tablerows 486); the keys 197 / 149. A soak finding on the
    way: the heap grows ~4 MB a minute in a 600 s `sp` run, `dupe trcolorinfo list` blocks 912 by Mission06 (owner M3 soak).
123. **The quit after a resolution change** (producer 1, `HWI-FIX` in `main.c mainDestroyWindow`): GoK's clear of `mainActuallyQuit`
    after a window re-creation made the next quit's first `SDL_QUIT` be ignored; the exe's window consumes its WM_DESTROY at once
    (`mainDestroyWindow 0x00560350`, `WindowProc 0x00561f10` case 2), so the port sets `mainActuallyQuit = TRUE`; the self-test
    `/catModeQuitTest WxH` (harness mode `modequit`, `--mode-to`) switches the mode and judges rc 0 with one `Quit game, baby!`; shown
    failing without the fix.
124. **The cutscene's one-frame flicker: not reproduced, no engine change** (producer 2): 450,644 scanned frames over 22 runs (n01,
    nislet01 and n06 whole; Mission01's NIS-first start with both cutscenes and the KAS close-up; virtual 60 / 120 / 144 / 165 / 240 fps;
    the real clock) with 0 outliers and 0 short runs; in the exe and in the port every NIS / KAS-letterbox frame after an NIS's first
    tick draws the bars late, so a frame with neither objects nor bars cannot leave the frame loop: the suspect is the presentation of
    an unfinished frame (XWayland / NVIDIA / cosmic-comp), open until the attended play-test's `frame.sig`; the engine does set
    `SDL_GL_SetSwapInterval(1)` (`render.c:1062`). Names: `/catFrameSig <file>` (HWI-TEST, any mode; the read-back columns on the
    virtual clock only), `/catVirtualFps <fps>`; harness `--frame-sig`, `--virtual-fps N`; `tools/dump_outliers.py` (the image / bars /
    objects / camera / short-runs tests, `--max-run`, `--oneline`). No default changed, no setting added; the KAS Out -> In bar snap is
    the exe's and stays. If the play-test confirms the presentation, M3-R3 measures `--driver wayland` or a swap-interval / finish
    setting against the frame budget.
125. **The hyperspace arrival of a modular ship** (producer 3): items 2 and 3 of the 10-21 report were GoK deviations; the exe's gate
    is the base (`CAT`): `hsStart` scales the gate rectangle and the effect by `hyperspaceBoxScale` (6.0 for the Kuun-Lan: `up=2035.1
    right=1762.1` against the core spine's 339.2 x 293.7; the sweep 2214.5 .. -2214.5), `hsGetEquation`'s POPUP_INTO plane,
    `hsContinue`'s host-inactive `glDisable`; `HWI-FEATURE(hyperspace-assembly)` default ON (run-only `/noHsAssembly`; no cfg row,
    console row, key or control): a part whose own exit has not started is drawn pending under the hull's clip plane and never through
    the LOD pass (the audit's fix); names `hsGateShapeGet`, `hsAssemblyBoxGet`, `hsAssemblyPartPending`, `hsAssemblyEnabled`; HWI-TEST
    lines `hsgate:`, `hsarrive:`, `hsassembly:`, `hsfinish:`. No stream, replay or dump moved; nothing re-baselined. Left: the exe's
    `hsStart` tests `glCapFastFeature(GL_BLEND)` where the tree tests `etgEffectsEnabled` (M3 oracle physics if a stream needs it);
    the modular mothership's multiplayer arrival (stage 5).
126. **The mesh batching** (producer 4, `HWI-FEATURE(mesh-batching)`, default ON): `meshObjectRenderLit` 0x004b0970 and
    `meshSpecObjectRender` 0x004b1d40 record their corners with the same lighting calls in the same order and draw each material run
    with one `glDrawArrays` from one scratch array; the exact reuses in `Shader.c` (`shColourLit0`, `shColourReuseLit`, `shGlowCache` /
    `shGlowStamp`, `shColourCapture`; `catShFastToInt` = `_mm_cvtsd_si64` on x86-64 Linux, `lrint` elsewhere); the run-only
    `/meshImmediate` and the console row `meshImmediate` (HWI row id 0x1006, `CAT_CON_HWI_ROWS` 7) restore the immediate path (no
    options-file row, no control, no key); unbatched: `meshObjectRender`, the warp, morphs, the edge lightning; `tools/hwiprof` (an
    LD_PRELOAD wall-clock sampler and its report), the profiling build `build/prof`. The 4K ai scene (164 objects, 25,169 vertices) on
    an idle GPU and a quiet CPU: batched p50 3.7-4.6 / p99 5.2-6.2 ms (the audit 3.1-3.4 / 4.5-5.1) against `/meshImmediate` 9.2-12.5 /
    13.4-15.3 (7.4-8.9 / 10.8-12.6): the 6.94 ms refresh interval met; every dump and capture byte-equal between the paths. This step's
    rerun under a load average of ~30: the medians 4.6-5.0 against 13.7, the p99 12.8-16.1 against 34-41 ms (the load's, not the port's:
    the frame is CPU-bound vertex work; the soak re-measures on a quiet machine).
127. **The harness** (decisions 92 / 117 extended; `tools/m2_harness.sh`, 31 modes): mode `modequit` (`--mode-to WxH`); flags
    `--frame-log` (`/frameLog`), `--frame-sig` (`/catFrameSig`), `--virtual-fps N` (`/catVirtualFps`); the `settings:` summary line (the
    run's `hwi-settings:` lines: their count and the first) in every run; the switches `/overlay`, `/noHsAssembly`, `/meshImmediate`
    pass through `--`. The harness replaces an existing label's directory, so each step prefixes its labels uniquely (this integrate
    `m3r2i-e-*` beside producer 1's `m3r2i-*`). Never edit `tools/m2_harness.sh` while a run is in flight.
    **M3-R3 (2026-10-26)**: extended again by decision 139 (31 modes; the round's switches: `/catMotionPause`, `/catMotionShot`, `/catOverlayKeyAt`, `/catConsoleAt`, `/catFeScreen`, `/catTexList`, `/catMissionTestHoldScout`, `/catMeshTexLog`, `/catMineView`, `/catMineLog`, `/catMineChase`, `/catTrailView`, `/exeTrails`, `/sharpTextures`; `tools/play.sh` rebuilds a stale binary itself).
128. **Decision 44 unchanged, the rung unchanged** (the integrate step, from clean builds of `47922ff`): the headless Mission01 stream
    `2131271d771f2056` (release `ce30c53259f034c8`) is IDENTICAL in all 21 fields over ticks 0-6000 to M3-R1's `1ae66610c3f02410` /
    `234bcccfb948ab98` and so to R10's `5bd51234405ea0d8`, with `/noInterp`, `/noNISInterp`, `/oracleLibmSqrt`, `letterboxBehind 1`
    (row and switch), `/lodBands`, `noLOD 0`, `/uiScale 200`, `uiScale 150`, the three opt-outs together, `/overlay`, `--frame-log`,
    `/meshImmediate` and `/noHsAssembly` all byte-identical to A; `mission_test.sh` PASS in both builds. The oracle's rung: 10-17 first
    divergent **2823** `ships_vel` and 10-18 **2871** `ships_vel` (177 / 118 with `/oracleLibmSqrt`; the exclusions `rng6` at 161 in
    both and `real` at 2 in 10-18; the release replay equal to the debug on every field but `rng_all` at 6377), both replays IDENTICAL
    in every field to `m3r1i-r17-a` / `m3r1i-r18-a`; the hwirec01 / hwirec02 playbacks IDENTICAL to M3-R1's; mode B still not run (no
    `original/oracle/hwirec*-play-*`); no fourth recording (the newest `oracle01-rec-*` is 20261019-143530; decision 48 open).
    **Presentation only** held: no simulation field of any oracle stream of the round moved (the 17 missions, the guard rails and the
    replays IDENTICAL to M3-R1's, which are R10's). The `--lodstats` runs' `rng_all` parts from the plain ai run's at tick 18 in M3-R1's
    own runs (`m3r1i-perf4k-on` vs `m3r1i-ai-a`) exactly as in this step's: a property of the lodstats camera walk, not of the round;
    the overlay on / off pairs and the batched / immediate pairs are IDENTICAL.
129. **The guard-rail dumps unchanged** (decisions 46 / 118 stand): this step's motion `/noInterp` `c503c869f0dc9cdb` /
    `3016280e9e0dd3c8`, flags-on `c0e411662fa2f527` / `428ef41479eb5749`, nis `/noInterp` `86cb155acffc180e` / `196bab32d244f614`,
    flags-on `0307f3be2044c29c` / `a5de13a44b8d264f` (whole / crc-stripped; 1,431 / 1,440 frames) are byte-equal to M3-R1's `m3r1i-*`
    files and to producer 4's final references (`m3r2-mbf-*`); the opt-outs together (`letterboxBehind 1` + `/lodBands` + `/noInterp`)
    byte-equal to R10's `r10i-motion-noi` / `r10i-nis-noi`; the batched and the immediate path's dumps byte-equal (`motion-on` =
    `motion-on-imm`, `nis-on` = `nis-on-imm`). No producer moved a dump: nothing re-baselined.
    **M3-R3 (2026-10-26): unchanged again** (decision 141): the same four hashes whole and crc-stripped; the `/meshImmediate` twins equal; `/sharpTextures` and `/exeTrails` the two stated crc-only variants.
130. **Tags and the delta map after M3-R2**: no `CAT-TODO` / `CAT-PROVISIONAL` names M3-R2 (`src/Game/BTG.c:1151`'s star size, M3-R1's
    look question the user has not answered, re-targeted by this step to `CAT-TODO(M3-R3)`: a comment-only edit, rebuilt from clean with
    0 warnings); 37 `CAT-TODO` lines (37), 28 owner tags: stage 5 23 (plain 8, multiplayer options 8, LAN 6, multiplayer 1), M3 oracle
    physics 3, M3 soak 1, **M3-R3 1**; 4 `CAT-PROVISIONAL`; `HWI-FEATURE(motion-interp)` 486 (484), `letterbox-setting` 37 (36),
    `full-detail` 179 (178), `ui-scale` 300 (299), **new `instrumentation` 102, `hyperspace-assembly` 55, `mesh-batching` 160**;
    `CAT-FIX` 195 (195), `HWI-TEST` 286 (222), `HWI-FIX` 62 (M3-R1's exit 53, 60 at the round base with the six `HWI-FIX(win64)` of the
    Windows build commits, +2 this round: the quit fix and its explanatory line). The delta map: IDENTICAL 2,757 / MODIFIED 1,580 /
    REWRITTEN 83 (producer 3's five edited `HS.c` bodies IDENTICAL -> MODIFIED), 3,579 reviewed rows; 210 MODIFIED / REWRITTEN rows
    unflagged (118 outside `TitanInterface.cpp`); review notes `[review: M3-R2 <port>]` 12 (hyperspace-assembly 7 + its audit's 1,
    mesh-batching 4). Round size: 49 files, +5,884 / −70 (`src/` 27 files, +2,459 / −37; new `src/Game/Instrument.c` / `.h`; the builds
    `[274/274]`).
131. **The user's decisions of M3-R2 and the M3-R3 queue** (`cycle/answers/m3-r2-user.md`, `m3-r3-user.md`): the overlay's toggle is
    **one of F1-F12** (binding; candidate F12, the only F key absent from `mainrgn.c`'s hardwired table, to be verified against the
    exe's key tables and the keys self-test before binding; the Menu key may stay as a second toggle), not applied in M3-R2, queued as
    M3-R3 item 0; the overlay at 200 % readable and covers nothing; the camera jitter marked fixed by the user (Smooth motion ON, M3-R1
    item 4 closed); the GPU free on the user's word (the idle check applies); M3-R1's decisions unanswered, the defaults stand (FULL
    DETAIL on, the 2.39:1 letterbox, the borderless 4K window, the exe's trails, 111-121). The user's third play-test (seven items,
    Missions 02-04) is M3-R3's queue; nothing of it is in M3-R2.
132. **The exit-test rules carried into M3-R3** (this step): decision 121's rules; a frame-time p99 is quoted only from a quiet CPU
    (state the load average; under ~30 this step's p99 was 2-3x the idle numbers while the medians agreed); a "not reproduced" item's
    done-when is its instrument proven on a forced frame plus the attended run's file; each step's harness labels get their own prefix;
    GPU measurements on an idle GPU with nvidia-smi before and after (this step 5 % / 0 %, no game running), `claude-ping` once if the
    user's game holds it, `claude-gpu-done` after the last one. A gate self-test is run under every switch combination the gate names
    before a round closes: this step's `/letterboxBehind` toggle run failed one M3-R2 check nobody had run (the check's expectation,
    `src/Game/MotionTest.c:2128-2131`, against the settings line's documented switch rule; owner M3-R3, one line).
    **M3-R3 (2026-10-26): kept by every producer, audit and the integrate** (decision 144 carries the rules into M3-R4 with four more: every producer-added setting flipped on the Mission01 stream, a paused dump as a gate, the ports' captures compared byte for byte, the GPU batch after the correctness chains); the toggle self-test ran under all ten switch sets this time (PASS).
133. **The overlay's key F1 and the switch-label rule** (M3-R3 producer 1, `HWI-FEATURE(instrumentation)` 102 -> 141 lines; `src/SDL/main.c`
    `HandleEvent`, `src/Game/KeyBindings.c hwiKbOverlayFKeyFree`): the overlay toggles on plain **F1** while the key is free (no modifier
    held, no binding of plain F1 in `keys.cfg`, the binding screen not waiting) and the key still goes on to the game; the Menu key stays a
    second toggle; F12 (M3-R2's candidate) was wrong: the exe's `default_keys.cfg` binds plain F12 to Formation_Parade and F2..F11 to the
    tactics / formations, F1 only with modifiers (Shift+F1 Leech_Spy, Ctrl+F1 the sensors' areas visited); the keys self-test's F-key line
    `overlay fkey F1 ... plain bindings F1=0 F12=1 free=1 bound-free=0 reset-free=1`. **The switch-label rule** (M3-R2's failing toggle
    check closed): a console row under a standing run-only switch that overrides an options-file row (`/noInterp`, `/noNISInterp`,
    `/letterboxBehind`, `/lodBands`, `/uiScale`, `/noFilter`) keeps the switch standing and the label `switch` (the saves keep the file's
    row); a switch with no options-file row (`/overlay`, `/meshImmediate`) ends on its console row (`console`); the toggle self-test
    checks it for every such row and is PASS under every switch of the gate (10 toggle runs this step). HWI-TEST:
    `/catOverlayKeyAt <ticks>`, `/catConsoleAt <tick>:<row>=<value>,...`, `/catFeScreen <screen>[@tick]`, `/catTexList`.
134. **SHARP TEXTURES** (producer 1, `HWI-FEATURE(sharp-textures)`, 196 lines; `Options.c`, `FEFlow.c`, `texreg.c`, `render.c`, `Console.c`,
    `Instrument.c`): the exe's base texture filter is `GL_LINEAR` (the original look) or `GL_NEAREST` min and mag with no mip maps
    (`trRGBTextureCreate` 0x005866a0, `trFilterEnable` 0x005888e0; the exe's own SURFACE FILTERING box on CUSTOM EFFECTS; `opEffectsHelper`
    0x004d9b90 re-applies the effects preset at every start, so HIGH resets `texLinearFiltering` to 1 and the cfg row alone is undone, as
    in the exe). On top: the Options -> Video checkbox SHARP TEXTURES (`OP_SharpTextures`, on `Video_Options` and the in-game Video
    screen; a click makes the effects level CUSTOM; Accept re-filters every live texture), the store is the existing row
    `texLinearFiltering` (1 = the original look = **the default**; 0 = sharp; by hand it needs `effectsLevel 2`), the run-only `/noFilter`
    and its alias `/sharpTextures` (they outlive the start's preset), the console row `texLinearFiltering` (HWI rows 8), the settings
    line's row with the new source `preset`, the `texfilter:` log line (`filter=sharp textures=N globjects=N wrong=0`); the sharp look is
    `GL_NEAREST` magnification over GoK's `GL_NEAREST_MIPMAP_LINEAR` minification (the mip maps and 8x anisotropy kept in both looks,
    decision 28: the user's question 2). A `CAT` fix (`trFilterEnable`'s third branch sets the mag filter) and a `CAT-FIX` (the live
    textures follow the flag at the next frame; the exe's Cancel left them on the cancelled filter). The 4K blur the user reported: the
    Kuun-Lan's LOD-0 hull textures are 32x16 .. 256x128 and TEXTURE MEMORY's middle (`TextureDetail 50`, the exe's 6 MB fitting) halves
    462 of Mission01's 2,343 live textures, 35 of the Kuun-Lan's 58 LOD-0 ones (HI shrinks none): the user's question 1; the default stays
    the exe's middle. Gate: the ai scene sharp against linear 2,685 px at 1024x768 / 20,864 at 4K, the Kuun-Lan's closest zoom 113,372 /
    890,954, the cfg row alone 0 px, the live console flip, the sharp dumps crc-only moves, every stream IDENTICAL.
135. **The pause freeze** (producer 2, `HWI-FEATURE(pause-freeze)`, 90 lines, `src/Game/MotionInterp.c`; always on, no setting): the
    user's pause is the P key's flag 0x00a6ed74 (`Pause 80`: the exe's frame-step flag), not `universePause` 0x00a6c5b4 (the Escape
    menu's and the console's); the jitter was M3-R1's Smooth motion replaying the last tick's blend with a moving alpha (a reverted build:
    71 / 71 paired paused frames moved). While the universe is paused (`miUniversePaused()`: `universePause`, or the P flag with no frame
    step left) every object draws at the pose the last frame drew (the alpha held), the camera float's clocks hold (the exe stops them for
    `universePause` only), the engine glows keep their size and the trail segments their colour (the random streams drawn as the exe
    draws them), the NIS's own pause holds the NIS render time; the first capture after a pause blends from the held pose (no step at the
    pause or at the unpause); the HUD's real-time animations (the SU blink, the upgrade pulse, the ship view, a hit flash's fade) run on
    as in the exe; the exe's `universePause` float restart (one frame of float time) is kept. HWI-TEST: `/catMotionPause
    <frame>:<frames>:key|univ|nis` (motion, nis and `--scene` runs), `/catMotionShot <frames>`, the dump tokens ` pause=` and ` crcv=`
    (the crc with the HUD's SU blink rect blanked). Gate: every paused pair equal in every world column, the camera's own chase stated (0 of 71 and 0 of 40 paired changes in
    the motion runs, the NIS pause's one `crc` change the simulation's own trail stop, the scene recipe's camera alternating by one
    float32 ulp in 24 of 71 pairs: `CameraChase`'s easing, 0.005 px); the default dumps EQUAL. The exe's camera float and glow shimmer
    during P are held still: the user's question 3 (a setting if the exe's look is wanted back).
136. **Mission02's dust belts and the BUSHAN-RE hint are the exe's** (producer 3; nothing ported): the belts are the decorative Asteroid0
    (486 from the `decorations` sphere's 97 cylinders; the "Dust" view option `OP_Dust` / cfg `ShowAsteriod0`, on by default), drawn by
    the exe's sensors scene (`cat_smViewportRenderScene` 0x0050b4b0) as 1-pixel `primPoint3` in 0xff788ea2 with **no fog-of-war test**,
    where the harvestable asteroids, the dust clouds and the crates (`cat_smResourcesDraw` 0x0050ae00) appear only in visited grid
    squares; the tree does the same (`Sensors.c` 1843-1938, 1698-1742); Mission02 has no dust cloud in either view (Missions 04 / 06 / 08
    have 2 / 63 / 5). The hint is `Init_Mission02` 0x005e05a0's difficulty timer (table 0x005e0b9c: 120 / 180 / 360 s of game time on
    VERY EASY / EASY / NORMAL, **none on HARD / VERY HARD**), cancelled for good when she is found (global 0x849928e4,
    `Watch_Mission02_Destroyer_Find` 0x005df130); the user plays HARD (`DifficultySetting 3`), so it cannot play; the tree's generated
    code matches line by line. HWI-TEST: `/catMissionTestHoldScout <tick>`, the Mission02 hint checks (`catMtHintWatch` /
    `catMtHintJudge`, the `speech:` log), the sensors draw counters, the resources / dust-cloud logs. Gate: 486 Dust points in every
    sensors frame, the captures on / off 247-261 px at 1024x768 (269-275 at 4K), the hint at tick 5824 on NORMAL with the scout held and
    never on HARD (VERY EASY: found first), proven non-vacuous by two mutations; every stream IDENTICAL. The sensors points' size at 4K
    (1 physical pixel as the exe, or the UI scale's 2x2) is the user's question 4; the Mission02 self-test's scripted player does not
    finish on HARD (SalvagePod; the self-tests play NORMAL).
137. **The mines' textures (an x86-64 layout fault) and the mine AI as the exe's** (producer 4, `HWI-FIX meshStaticTeamColor`, +14
    `HWI-FIX` lines in `src/Game/Mesh.c` and its callers): the white mines and missiles came from the four team-colour reads casting every
    static through `ShipStaticInfo`, whose colour-scheme row sits at another offset in the 64-bit layout (the exe reads +0xF0 of every
    static kind), so a mine's / missile's static read a wrong row; now a missile / mine static gives its own row: the level mine
    `textures=11 bound=11 white=0` (was `bound=0 white=11`), the laid mine `textures=4 bound=4 white=0` (was `unregistered`); FileLoads
    3,170 -> 3,174 (the Taiidan mine's `PAGE0` LOD0-3); the captures 19 / 6 flat colours -> 10,657 / 970; batched = `/meshImmediate` 0 px.
    The mine AI is the exe's (`aishipGuideMine` 0x00416410, `aishipFlyMineToTarget` 0x00415fa0; nothing ported): the user's "3 s then
    stop" is the **faster-away rule** (a mine gives up a target flying away faster than itself and brakes x0.5 a tick) plus the
    once-a-second search; the laid mines (600 u/s) give up on every Somtaaw strike craft flying away, the level mines (1,000 u/s) catch
    Acolytes (825) but not Recons (1,050); a mine whose target is destroyed or leaves lives 5 s more (0x008b5c68). HWI-TEST:
    `/catMeshTexLog <substring>`, `/catMineView <tick>[:dropped][:<distance>]`, `/catMineLog <ticks>`, `/catMineChase <tick>[:<shiptype>]`.
    The Beast mine has no texture in the game data (plain in the exe too); the other x86-64 casts of a non-ship static through
    `ShipStaticInfo` are M3 oracle physics' layout audit.
138. **The missile corvettes' four trails** (producer 5, `HWI-FEATURE(engine-trails)`, 113 lines, `src/Game/Trails.c`; **default ON**): the
    exe draws one ribbon per mex Eng chunk (`InitializeEngineTrails` 0x00541f40; the trail pass 0x005772dd draws all four slots): two for
    each missile corvette (`bMissileCorvette` / `p1MissileCorvette`: 2 Eng chunks, nozzles (0, +-17.854, -45.75)), each centred between
    an upper and a lower painted circle; no type has more Eng chunks than the exe draws; the tree matched the exe. The feature draws four
    ribbons, one from the centre of each painted engine circle (up +-11.5, right +-17.891, the Eng chunk's z), each half the exe's
    42-unit width across the pair, the exe's other parameters and glow; `/exeTrails` (run-only), the console row `engineTrails` (HWI rows
    9: console shown 38 / set 31), the settings line's `engineTrails`; no cfg row, no Options control, no key; the added ribbons draw
    their render jitter from their own generator, never the shared random stream (the trail streams 13-17 equal at every frame on /
    off / reverted). **Decision 84 sharpened**: the run-to-run `rng_all` noise is stream 11 (`RANDOM_SOUND_GAME_THREAD`) alone. HWI-TEST:
    `/catTrailView <tick>:<ShipType>[:d][:e][:s][:r][:m]`. Gate: four ribbons from the four circles at 1024x768 and 4K for both types
    (the exe's two with `/exeTrails`), the nozzles within 0.8 px (3.3 at 4K) of their rings' centres, `/exeTrails` byte for byte the
    reverted build, every stream IDENTICAL, the dumps EQUAL (`motion-exe` = `motion-on`), the 4K cost nothing above the run-to-run
    spread on the idle GPU (medians default 3.743 / 3.422 ms against `/exeTrails` 3.483 / 3.240 at 3840x2160, cost 0.221 ms against a spread of 0.321; p99 5.643 at most). The user's look at the four streams: the brief.
139. **The harness** (decisions 92 / 117 / 127 extended; `tools/m2_harness.sh`, 31 modes, +36 header lines): this round's engine switches
    through `--` (`/catOverlayKeyAt`, `/catConsoleAt`, `/catFeScreen`, `/catTexList`, `/sharpTextures` / `/noFilter`, `/catMotionPause`,
    `/catMotionShot`, `/catMissionTestHoldScout`, `/catMeshTexLog`, `/catMineView`, `/catMineLog`, `/catMineChase`, `/catTrailView`,
    `/exeTrails`), the `--scene` kind `mothership:<distance>`, `--cfg` rows with the settings line's `(cfg)` and `(preset)` sources;
    `tools/play.sh` rebuilds a stale release binary itself (`c698dd5`: when the binary is older than the newest `src/` commit;
    `--rebuild`, `--debug`) and documents `/sharpTextures`, `/exeTrails`, `/catMineLog`, `/catFrameSig`.
140. **The Mission01 stream and the rung unchanged** (decision 44 stands, as decisions 128 / 140): this step's `c20ccda7fdae0f66` (release
    `316025e5c47989e1`) is IDENTICAL in all 21 fields over ticks 0-6000 to M3-R1's `1ae66610c3f02410` and so to R10's `5bd51234405ea0d8` (the
    header's build identity only), with every setting of the round flipped (29 variants: the cfg row alone and under `effectsLevel
    2`, `/sharpTextures`, `/noFilter`, `/exeTrails`, the console rows `engineTrails` and `texLinearFiltering` flipped live, the F key
    pressed twice; and M3-R2's list), and so is every producer's and audit's Mission01 run of the round; the rung 2823 / 2871 (177 / 118
    with `/oracleLibmSqrt`), the release replay equal on all but `rng_all` (6377); the hwirec01 / hwirec02 playbacks, 17 / 17 missions,
    the guard rails IDENTICAL to M3-R2's, which are M3-R1's and R10's. **Presentation only** held for the whole round: no simulation
    field of any stream moved.
141. **The guard-rail dumps unchanged** (decisions 46 / 118 / 129 stand): the four default dumps byte-equal to M3-R2's references (motion
    `/noInterp` `c503c869f0dc9cdb` / `3016280e9e0dd3c8`, flags-on `c0e411662fa2f527` / `428ef41479eb5749`, nis `/noInterp`
    `86cb155acffc180e` / `196bab32d244f614`, flags-on `0307f3be2044c29c` / `a5de13a44b8d264f`: whole / crc-stripped), the opt-outs
    together byte-equal to R10's `183681235a3fdc7b` / `13ca70eea998e5de`, the `/meshImmediate` twins equal; the two **stated crc-only
    moves** are the options themselves: `/sharpTextures` (`motion-sharp` / `nis-sharp` `1a2227d966ec9712` / `871eac333fd603d0` whole, crc-stripped equal to `motion-on` / `nis-on`: 1,431 / 1,439 lines differ whole, 0 crc-stripped, byte-equal to producer 1's `m3r3-sk-motion-sharp` / `-nis-sharp`) and `/exeTrails` (`motion-exe` = `motion-on` and `nis-exe` = `nis-on` byte for byte (no missile corvette in either scene), `motion-exe` byte-equal to producer 5's `m3r3-et-motion-exe`); the paused dumps a gate (every
    paused pair equal in every column). Producers 2, 3 and 4 moved no dump.
142. **Tags and the delta map after M3-R3**: `CAT-(TODO|PROVISIONAL)\([^)]*M3-R3` empty (`src/Game/BTG.c:1151`'s star size, M3-R1's
    question 5 unanswered through three rounds, re-targeted by this step to `CAT-TODO(M3-R4)`: comment-only, rebuilt from clean with 0
    warnings); 37 `CAT-TODO` lines (37), 28 owner tags: stage 5 23 (plain 8, multiplayer options 8, LAN 6, multiplayer 1), M3 oracle
    physics 3, M3 soak 1, **M3-R4 1**; 4 `CAT-PROVISIONAL`; `HWI-FEATURE(motion-interp)` 486 (486), `letterbox-setting` 37, `full-detail`
    179, `ui-scale` 300, **`instrumentation` 141 (102)**, `hyperspace-assembly` 55, `mesh-batching` 160, **new `sharp-textures` 196,
    `pause-freeze` 90, `engine-trails` 113** (1,748 `HWI-FEATURE(` lines; 1,314 at the round base); `CAT-FIX` 196 (195), `HWI-TEST` 461
    (286), `HWI-FIX` 76 (62: the 14 lines of `meshStaticTeamColor`), `CAT:` 4,476 (4,475). The delta map: IDENTICAL 2,757 / MODIFIED 1,580
    / REWRITTEN 83 (no class change), 3,582 reviewed rows (3,579); 210 MODIFIED / REWRITTEN rows unflagged (118 outside
    `TitanInterface.cpp`); review notes `[review: M3-R3 <port>]` 28 (settings-and-keys 15, mines 6, engine-trails 7), merged. Round size:
    53 files, +5,784 / -120 (`src/` 33 files, +2,455 / -52; no new `src/` file; the builds `[274/274]`).
143. **The user's decisions of M3-R3 and the open questions** (`cycle/answers/m3-r3-user.md`): the mandate ("a new option to support
    sharp texture meshes or the original texture look": SHARP TEXTURES, the original look the default), the overlay's key "from the F1-F12
    suite" (F1, verified against the exe's key tables; M3-R2's F12 candidate was wrong), the GPU free on the user's word (the idle check
    applied all the same); the third play-test's seven items each either the exe's, shown from its machine code (the dust points, the
    hint's timer, the mine AI), or fixed with a fence (the F key, SHARP TEXTURES, the pause, the mines' textures, the four trails);
    M3-R2's four checks and M3-R1's questions 2-5 / 7-10 unanswered: the defaults stand; **this round's five open questions** (TEXTURE
    MEMORY's default, the minification look, the pause's float and shimmer, the sensors points' size with the dust-belt / backdrop
    question, the four streams' look): the defaults in the tree until answered; **M3-R4 exists only when the next play-test reports
    something or the user picks "the exe's" on a look question** (the BTG star size, the letter height, the additive state, the sensors
    points, TEXTURE MEMORY HI, the unfiltered minification, the pause's float).
144. **The exit-test rules carried into M3-R4** (this step): decisions 121 and 132's rules, and: every producer-added setting is flipped on
    the Mission01 stream by the integrate (the cfg row, the switch, the console row live, the key); a paused dump is a gate
    (`/catMotionPause` in the motion, nis and scene runs: every paused pair equal in every world column, the camera's own chase stated); a port's captures are compared byte
    for byte with the integrate's (every PPM of the five fences); the GPU batch runs after the correctness chains so the CPU is quiet
    (the GPU batch waited from 11:49 to 14:55 for the user's game to release the GPU (`gpuwait`: nvidia-smi every 10 s until 120 s at or under 40 %), then ran its eight runs on the idle GPU in two batches of four, the second after a two-minute wait on a 50 % sample; `claude-ping` fired 2 times (11:49:21, 14:57:12): the library pings once per `gpuwait` call, not once per gate, a deviation from the user's rule to fix in `chains/lib.sh` before M3-R4); a frame-time p99 is quoted with the load average; a "USER:" question of a report is quoted verbatim in the summary;
    the user's answers file is read first and again before the report.

## M3-R4, ad hoc items landed between rounds (2026-10-26, the user's terminal session 38027cce)

- **sensors-points** (HWI-FEATURE(sensors-points), docs/m3-r4-sensors-points.md): the user's decision "double the dust particle size and increase the resource markers by 50%": the sensors manager's Dust points 2 px (the exe's 1), the resource / meteor / mine / crate markers 3 px (the exe's 2, or 1 for the smallest), a mimic posing as an asteroid 3; `/exePoints` run-only, console row `sensorsPoints`, the settings line; the toggle self-test gained the `engineTrails` row M3-R3 missed. One Opus producer, reviewed by the session (Fable) instead of an auditor: Mission01 stream IDENTICAL in every field, Mission02 sensors captures 275 -> 1,100 Dust pixels at 4K (4.00x), markers 2.25x, every /exePoints capture byte-equal to M3-R3's, hud / console / toggle x3 / fescreens / statics PASS, ASan 0. Decisions (the reviewer's): the small mines at 3 px, the mimic at the markers' size, fixed pixels not scaled by the UI scale.
- **speech_probe** (tools/formats/speech_probe.py, 5c5e4ea): speech event id -> phrase offsets in the race's .vce + subtitle text; used to answer the user's Mission02 dialog question (the BUSHAN-RE hint 0x10002169 is a difficulty timer: 120 / 180 / 360 s on VERY EASY / EASY / NORMAL, never on HARD / VERY HARD; the only other difficulty-gated lines in the campaign: Mission01's "They are going after our Workers" and Mission13's "Incoming capital ships", both EASY and below).
- **mines-recolor** (CAT-FIX (M3-R4 mines-recolor), docs/m3-r4-mines-recolor.md, the commit after ec98d3e): the user's Mission06 report "still seeing the same untextured mines": the 29 level (Derelict) mines were white after Mission05, whose colour table has no Derelict entry, because `universeStaticInit` never recoloured a mine or missile static kept loaded across a level change (the exe's structure too: a decompile reading); the mine and missile loops now recolour as the ship and derelict loops do; fence: Mission05 played into Mission06 before (bound=0 white=11, ~640 colours) / after (bound=11, ~8,600), the Mission01 / 03 / 06 streams IDENTICAL to M3-R3's, the M3-R3 mine capture byte-equal, ASan 0. The user's workaround meanwhile: save and load in Mission06.
- **infect-convert** (CAT-FIX (M3-R4 infect-convert), docs/m3-r4-infect-convert.md, the commit after 0c0d634): the user's Mission07 SIGSEGV of 19:47: a Beast cruise missile infected the Taiidan heavy cruiser salvaged in Mission06, and the conversion created its Beast form bHeavyCruiser from a static Mission07 never loads (its IncludeShips are closed under the Beast forms, the fleet the player brings in is not; the exe's structure too: `SetInfoNeededForShipAndRelatedStaticInfo` 0x004a29f0 flags a type, its missile and its mine, `univCreateShip` 0x00542320 reads the static unchecked). Fix: `universeStaticInit` runs a second load pass for the `beastAttackConvertTo` of every loaded infectable type when the level loads any Beast type (level entry and save load alike, so the user's existing save is covered). HWI-TEST `/catInfectAt <tick>:<ShipType>` reproduces it on the user's save under ASan (before: the UnivUpdate.c:1940 guard; after: `infecttest: converted ... new=bHeavyCruiser ... beast=1`). Fence: Mission01 / 06 / 07 streams IDENTICAL to M3-R3's, Mission04 and the user's save before/after IDENTICAL (unfixed binary vs fixed; +bMothershipEnginesLower / +bHeavyCruiser in the file lists), ASan 0. Follow-up noted: since 0c0d634 the Mission03 / 06 / 07 file lists load six derelict junk .LiF textures a second time (the mine recolour's registration), harmless.
- **crystal-tooltip** (HWI-FIX (M3-R4 crystal-tooltip), docs/m3-r4-crystal-tooltip.md, the commit after 452991f): the user's "the resource value given is always 0" on every crystal's tooltip (Mission11): the port printed `cat_mimicGetApparentObject` (Ship+0x72c behind the mimic flag, 0 for a crystal) where the exe prints the crystal's own `CrystalSpec.resourceValue` (Ship+0x638; 0x004a7dfd, read from the disassembly: Ghidra drops sprintf's varargs, so the decompile hid it). Fix: the crystal branch prints its resource units; the tooltip's text builder is split out as `cat_moTooltipText` for HWI-TEST `/catTooltipAt <tick>:<ShipType>` (tooltiptest: lines through `selShipUnderMouse`, the cursor state put back). Fence: Mission11 before "CRYSTAL SMALL (0)" / after "(6000)"; Mission11 and Mission08 to their ends "CRYSTAL LARGE (10000)" / "CRYSTAL SMALL (6000)" with streams IDENTICAL to M3-R3's, Mission01 IDENTICAL, ASan 0, both builds 0 warnings; Mission11's file list +26 .LiF lines are 0c0d634's (the control run with the pre-item binary matches). Tags HWI-FIX 76 -> 77, HWI-TEST 511 -> 519.
- **nan-positions** (CAT-FIX (M3-R4 nan-positions) + HWI-FIX focus safety net + HWI-TEST (M3-R4 speech-log), docs/m3-r4-nan-positions.md, the commit after 3b99088): the user's Mission17 "screen went full black" on focusing group 1 (HUD intact): ships whose physics state was NaN (ten `univCheckShipState` kills in the play log, one in Mission16) made `NewSetFocusPoint`'s look-at point NaN. Chain (reproduced headlessly with HWI-TEST `/catNaNProbe`, nanprobe: lines per univUpdate phase, and on the user's own save): Mission17's Taiidan "k" teams get KAS attack lists holding their own ships (the script's `kasfShipsSelectFriendly` before `kasfTeamSwitchPlayerOwner`; no engine filter, in the exe either), `AttackClosestOpponent` picks the ship itself at distance 0, the interceptor's REAPPROACH flies to its own position and `vecCopyAndNormalize` of the zero vector is a NaN heading in both; the exe's `aitrackHeadingFunc` 0x00418eb9 (`fcom -1.0; fnstsw; test ah,0x41`) takes the "exactly opposite" branch on the UNORDERED compare (a finite spin about X, no NaN enters the ship), the port's IEEE `<= -1.0` fell through into torque, coordinate system, position (and every ship targeting a NaN ship caught it). Fix: `(dot <= -1.0) || isnan(dot)` at the three x87 sites (aitrackHeadingFunc, aitrackHeadingAndUp x2, AITrack.c); HWI-FIX: `NewSetFocusPoint` leaves NaN collision centres out of the focus (refused when none is finite). Mission15 "didn't hear dialog 13" (the build manager opened then): no path by which the manager alone silences a line was found (only `smSensorsSkip`: Backspace, Escape in the fleet-intel view, the Skip button; and `kasfSubtitleReset`), so the log now tells: `speechlog:` lines for every mission line (queued / dropped+reason / play / ended / expired / cleared, the skips' triggers, the manager's open and close) in the next play log. Fence: the user's save 4000 ticks 23 -> 0 NaN ships; Mission17 self-test 4 -> 0 NaN kills (its stream differs from M3-R3's as it must); Mission08 / 15 / 16 and the mission mode streams IDENTICAL to M3-R3's; ASan 0; 0 warnings. Lesson (memory x87_unordered_compare_nan): Ghidra renders `fcom` + `test ah,0x41` as a plain `<=`; the disassembly shows the unordered branch.
- **mesh-anim-interp** (HWI-FEATURE(motion-interp) mesh animations, docs/m3-r4-mesh-anim-interp.md, spec item 8 in docs/features/motion-interpolation.md, the commit after 616f11f): after the full playthrough the user asked for "a few interpolation steps" in the Workers' animations and the canister scene, "16 hz animations" at 60 fps. Cause: a ship's `.mad` animation (six B-spline curves per object) is advanced by `madAnimationUpdate` once per universe tick and `madAnimBindingUpdate` evaluated the curves at that time when the ship was drawn (`madBindingsPost` zeroes the elapsed time after the first frame), so every frame of a tick drew the tick's pose. Fix: `madAnimBindingUpdate` evaluates the drawn pose on a COPY of the object's curves stepped on by `alpha * phystimeelapsed` (`madRenderStep`: `miRenderAlpha` only between `miRenderBegin` / `miRenderEnd`, new `miRenderDrawing`; a paused animation holds; the animation's own loop / end rule on the copy); the simulation's curves, `madanim.time`, the gun / dock evaluation and the save are untouched, so the oracle streams are bit-identical and `/noInterp` gives the exe's pose. HWI-TEST: `/catMadProbe` (madprobe: lines, the tick's pose and the frame's pose), scene kinds `worker` (the camera on H, the depositing Worker) and `harvest` (on W) in MotionTest.c. Fence: rc60 recon probe (the Mothership's GravRotate, 27 frames over 8 ticks, the drawn pose within 0.0001 rad of the line between tick poses), `/noInterp` 0 lines, worker (DockOpen, 3.5 frames per tick, the frame at alpha ~1 meets the next tick's pose) and harvest (JackOut) probes; mission and Mission04 streams IDENTICAL to M3-R3's, Mission01 ran (no baseline); ASan 0; 0 warnings both builds. The Mission04 jettison cutscene's own motion is NIS-curve-driven and was already interpolated (item 1); the user is to eyeball the Workers at 60 fps.
- **sound-burst-log** (HWI-TEST (M3-R4 sound-burst-log), docs/m3-r4-sound-burst-log.md, the commit after 654b900): the user's "large audio spike of white noise" during standard play near the end of the full playthrough. The logs could not place it: neither the exe's sound system nor the port's logged anything (no play log since 2026-10-20 has a mixer, stream or patch line); the tail of play-20261028-102144.log shows only speech lines, build manager opens, pauses and two more `univCheckShipState` NaN kills of Beast Ramming Frigates (#1295 / #1296; a second NaN path besides nan-positions, in a ramming frigate's manoeuvre, OPEN) which cannot reach the mixer (integer pan / volume, a NaN pitch silences). The mixer, streamer, channel code and fq decoder were read for a cause (none proven): white noise out of this mixer is bytes decoded off a block boundary (an entry end stepped past by the exact `==` end tests, stale buffer bytes), or the fq band exponents carried into a wrong block; the panic mode (a mixer block with no rendered frame for ~128 ms; the user's AutoChannel 1 lets the voices fall to 8) and a starved stream are silence, not hiss. Now: `soundlog:` lines in every play log, posted by the mixer and stream threads into a lock-free 16-slot ring and printed by the render task with the tick: `burst start` (3 blocks in a row: left peak >= 12000, sign changes >= 30 %, spectral flatness >= 0.30; white noise measures ~50 % / ~0.77) with up to three `burst src` lines (the loudest decoded spectra: an SFX channel's bank:patch, id, block position and loop points, pitch, volume; a stream's entry, speech event, file offset, size, bytes read, buffer state) and `burst end`; `panic on / off`; `stream N starved`; `stream N end missed` (an entry size not a block multiple); `stream N recover`; HWI-TEST `/catSoundNoiseAt <block>` injects 12 blocks of white noise into the first playing source. Fence: the sound self-test, a headless skirmish and an 87 s real-clock scene (6556 blocks) give 0 bursts / starved / missed ends (one headless `panic on` at each run's end, expected); the injection: `burst start block=41 peak=21955 zcr=44% flat=0.76`, `burst src stream=3` (the music stream, its offset / size / bitrate), `burst end blocks=11`; ASan 0; 0 warnings both builds. Next: the user plays; a repeat gives the source line, and its absence puts the noise outside the mixer (the audio device or the OS).
- **module-spin-interp** (HWI-FEATURE(motion-interp) module spin, docs/m3-r4-module-spin-interp.md, spec item 9 in docs/features/motion-interpolation.md, the commit after b5e82c3): the user: "for mission 2, constructing the engineering module triggers the ingame cutscene that jettisons the ore canister. I still see its animation at the low refresh rate". The cutscene is neither an NIS nor a mesh animation: the module built under the ore-can armour is created inside the Kuun-Lan and launched (`clCreateShip` -> `processLaunchShipToDo` 0x00466b00), which calls `cat_moduleShipEject` (0x005af940) on `sMothershipOreCanArmour`: unclamped, pushed (physics: the gameplay lerp blended the drift) and spun by `spinAngle += 0.05` once per tick in `ModuleShipHouseKeep`, which the renderer drew raw in every frame of the tick (2.9 degrees a step at 16 Hz); the KAS then focuses the camera on it. Fix: `cat_moduleShipDrawnSpinAngle` gives `render.c`'s one `glRotatef` the tick's angle plus `alpha * 0.05` (`miRenderAlpha` while `miRenderDrawing`) while the next housekeep would step it (ejecting, `totaltimeelapsed + phystimeelapsed <= ejectEndTime`); the simulation's angle is never written, so the streams are bit-identical and `/noInterp` draws the exe's. HWI-TEST: `/catMadProbe` also logs `spinprobe:` lines; scene kind `eject` (O = the clamped ore-can armour, the camera on it, no Recons needed, M stays put, O jettisoned at the order tick as a launch does). Fence: Mission02 self-test `--scene eject:0:500:200:240 --real-clock 60`: 600 lines over ticks 272-439, 3.57 frames a tick, every step = alpha * 0.05, continuity 0.019 rad modulo the 2 pi wrap; `/noInterp` 0 lines, the eject the same; the natural path (`--scene eject:..:1200` over the self-test's own Micro build at tick 1192): 600 lines from tick 1193, 8 frames a tick, continuity 0.0056 rad; Mission02 and mission streams IDENTICAL to M3-R3's; ASan 0; 0 warnings both builds. Lesson: the harness's `--mode mission` has no mission selector (the campaign start, Mission01); a Mission N scene runs in `--mode missiontest --mission N` with `--scene`.
- **window-modes** (HWI-FEATURE(resolution-presets) + HWI-FEATURE(window-resize), docs/m3-r4-window-modes.md, spec docs/features/window-modes.md, the commit after 93e853f): the user: "please add typical supported resolutions for 16:9, 16:10, and 4:3 aspect ratios as well as support for dynamic window sizing incase anyone wants a windowed format that the user can drag and expand the window to the size they want". Before: the SDL port's `rinEnumeratePrimary` listed the desktop mode and its integer divisions only (at 4K: 3840x2160, 1920x1080, 1280x720, 960x540), an unlisted request left the first entry selected (an untouched Accept switched to it), and the window was not resizable (the exe's fixed client size). Now: rinit.c lists every preset of the three aspects (and the exe's 5:4 1280x1024) that fits a connected display, each display's own mode and the start's request, ascending and unique (`rinAddModeIfMissing`); Options.c draws the aspect name (`rinAspectName`: 4:3 / 16:10 / 16:9 / 5:4 within 1%, else W:H) instead of the depth, adds and selects the current request when the Video screen opens, and matches without the depth. A windowed mode is `SDL_WINDOW_RESIZABLE` (minimum 640x480); `uiScaleWindowResized` on `SDL_WINDOWEVENT_SIZE_CHANGED`: physical = the drawable, the scale chosen again for the height, the logical size, `rndAspectRatio`, the viewport, the mouse, the request (so Cataclysm.cfg's screenWidth / screenHeight hold the dragged size and the next start opens at it) and the exe's `utyChangeResolution` reflow, at once in the front end and owed while an in-game screen is open (`uiScaleResizeFlush` from the main loop: `feAllScreensReposition` would pop a manager past its close path). The fit rule is unchanged: the user's 3840x2160 `fullScreen 0` file still opens fullscreen-desktop (no frame); a smaller size on the Video screen gives the draggable window. HWI-TEST `/catResizeTest` (harness mode resize, 68 checks: the list, six scripted sizes 1600x900 / 800x600 / 1280x1024 / 2560x1440 / 3840x2160 / 1234x777 with the scale, logical size, main region, menu centring, aspect, request and mouse corner, the Video screen listing and selecting the dragged size, the file rows), `/catResizeAt <tick>:<W>x<H>` (harness `--resize-at`). Fence: resize PASS at 1024x768, `--res 3840x2160` and `--res 1234x777`; skirmish `--res 3840x2160 --resize-at 300:1280x720 --shot 280,340`: the 4K buffer's bottom-left 1280x720 shows the HUD and the view laid out for 1280x720 (a scripted change UP is clipped to the offscreen driver's real buffer: the harness limit); uiscale / uiscale-4k 45 checks PASS, modequit PASS, toggle PASS, mission IDENTICAL to m3r3i-e-mission-a; ASan 0; 0 warnings both builds. The real drag (SDL's event under the user's compositor) is the user's check. Lesson: `src/SDL/rinit.h` is the header in the build, not `src/Win32/rinit.h` (a declaration added to the Win32 copy left the function implicit, `-Wno-implicit-function-declaration`, and its returned pointer truncated to 32 bits: an ASan SEGV in the first run).
- **mode-switch-fix** (HWI-FIX, docs/m3-r4-mode-switch-fix.md, the commit after fbf80ba): the user: "I tried switching between 2560x1600 to 3840x2160 and now I just see a black screen for the game" (and asked for the hung hwi-engine to be killed: SIGTERM ignored, SIGKILL). The play logs: 2026-10-20 1024x768 -> 3840x2160 and 21:57 3840x2160 -> 2560x1600 died on an X error BadPixmap from Present's PresentPixmap (opcode 145 minor 1 on the user's Xwayland); 21:57:53 2560x1600 -> 3840x2160 hung (main thread asleep in poll). Cause: GoK's `rndClose` quit SDL's whole video subsystem (the X connection) and never deleted the GL context; under the NVIDIA driver 580 + Xwayland the first swap on the new connection's window fails or never returns. Proved by `tools/glswitch.c` (hidden windows on the user's display: GoK's order BadPixmap / hang, the fix's 4 windows clean) and by the engine with the new harness flag `--hidden-x11` (HWI-TEST `/catHiddenWindow`: windows created hidden, no pointer warp / relative mode; the Linux build sends no synthetic keys) and `--mode-to WxH:N` (`/catModeQuitTest` draws N frames after the switch): before, every switch died on frame 1's swap and LSan found 4.3 MB in libnvidia-glcore; after, 120 frames on 2560x1600 -> 3840x2160, 3840x2160 -> 2560x1600, 1024x768 -> 3840x2160 and 1280x720 -> 1920x1080. Fix: render.c `rndWindowDestroy` (make-current NULL, delete the context, destroy the window, keep the subsystem; main's SDL_Quit at exit) from rndClose and the palette-failure path; setupPixelFormat's "nothing changed" return needs a window. `tools/lsan.supp` gains `leak:libnvidia-glcore.so` (the driver's own 22 KB). The x11 resize self-test lists all 20 presets on the user's 3840x2160 displays (85 checks PASS). Offscreen modequit / resize / uiscale (both sizes) / toggle PASS, mission IDENTICAL; ASan 0; 0 warnings both builds. The user's own visible switch is the last check. Lessons: an engine hung in a swap ignores SIGTERM (SDL's handler only queues SDL_QUIT): time out with SIGKILL; the user's Xwayland makes a 2560x1600 window's drawable 2561x1601 (fractional scale), so x11-driver size checks allow a pixel.
- **intro-movies** (HWI-FEATURE(intro-movies), docs/m3-r4-intro-movies.md, docs/features/intro-movies.md): the user: "renable the starting cutscenes for sierra and barking dog and maintain their original aspect ratio instead of stretching them out". The exe (utyGameSystemsInit 0x005abe70, when enableAVI): `sierra.bik` with music track 0x3c, `Sleep(2000)`, `bdogintro.bik` with 0x3d (binkPlay 0x00551840 plays the track from the first frame, stops it at the end; Music4Cat has 62 tracks); it blits 640x480 1:1 or switches a fullscreen display to 640x480 (stretched by a widescreen monitor). GoK's `aviIntroPlay` played HW1's `relicintro.bik`, no music, only with its own row `PlayIntros` (default 0, also seeded by play.sh and the harness template). Now: `aviPlayIntros` default 1 (`PlayIntros 0` / `/disableAVI` / headless / no-movie build = off), the exe's order, tracks and 2 s hold; every movie's picture fitted whole and centred (`aviPictureRect`): the logos' whole 4:3 frame, the animatics' 5:3 band (all 18 measured: rows 48..431; GoK's 1.85 assumption cut a quarter of a logo and ~6% of an animatic at 16:9). Also fixed in the GoK player: one texture name kept across GL contexts (a movie after a resolution switch), the filter set before the bind, GL_REPLACE / GL_TEXTURE_2D left behind the state cache (now glPushAttrib), an AVFrame leaked per movie, one event read a frame (now drained: a key ends the movie, SDL_QUIT ends it and is put back for the main loop, window events go to HandleEvent with the relayout deferred while `aviIsPlaying`). HWI-TEST `/catMovieShot`, `/catMovieTest <level>[:<frames>]`, `/catMovieEventAt <frame>:quit|WxH`. Fence: 4K / 5:4 / animatic / quit / off / hidden-x11 resize runs: frames 240/240 in 16000 ms + hold 2003 ms + 807/807 in 26901 ms; every shot matches its source frame scaled into the logged rect (mean diff 0.02-0.10 of 255, bars 0); the quit run rc 0; ASan 0, LSan clean. Regression: modequit, modequit-f, resize, uiscale, uiscale-4k, toggle PASS; mission 6000 IDENTICAL; 0 warnings. Not verified: the music by ear (dummy audio), an animatic's subtitle card (the dummy driver's speech expires unplayed), the user's own start. Lesson: a headless mission run's default is 3000 ticks; the baseline m3r3i-e-mission-a is 6000 (`--mission-ticks 6000`).
- **launcher** (README.md "Play it", `setup.sh`, `tools/launch.sh`): the user: "make a shortcut that I would click to trigger the game. We need to make the game easy to launch for a lay person. Include a short readme to any new users of the github repo, and ensure that any setup work is done already for the user". `setup.sh` (repo root; idempotent): the apt packages (sudo), the game files from the GOG offline installer (auto-found in ., original/, ~/Downloads; tools/extract_installer.sh) or copied from an installed folder (+ MANIFEST.sizes), `tools/m2_harness.sh --setup`, `tools/build.sh --release` (skipped while the game runs), the shortcut `homeworld-infection.desktop` in ~/.local/share/applications and on the Desktop (Exec tools/launch.sh, the GOG .ico's largest PNG as the icon, StartupWMClass homeworld-infection), `--shortcut`, `--remove-shortcut`; refuses a linked worktree. `tools/launch.sh`: one game at a time (flock), SDL_VIDEO_X11_WMCLASS=homeworld-infection, tools/play.sh's defaults (a notification before a build: HWI_LAUNCHER), a dialog (zenity / kdialog / notify-send) for not set up, a failed build, a non-zero exit with the session's log. play.sh no longer seeds `PlayIntros 0`; the user's `original/play-home` Cataclysm.cfg row was set to 1. Fence: two throwaway non-git copies with their own HOME: the installer from ~/Downloads (387 MB unpacked) and the folder copy, each built and installed a valid desktop file (desktop-file-validate) and the icon; a second run takes 0.8 s; launcher cases with stub dialogs: normal (rc 0, no dialog), error exit (the session's log), already running, not set up, a failed build (the notification, then the build dialog: fixed to read only this launch's log lines). The user's machine: setup.sh run in the main checkout. Not verified: apt on a machine missing packages, other distributions (dnf / pacman print the names), a click by the user.
- **original-viewer** (`tools/play_original.sh`, README "Comparing with the original game", docs/RUNBOOK.md section 4b): the user: "Is there still a way I could view the game in its original format from gog? ... I want to compare what the game looks like now after our changes to the original version". `original/game/` carries CET's `rgl.dll`/`rgld3d.dll`/`Update102.big` and the oracle DLL, so a second extraction `original/gog/game/` (manifest identical, every file verifies) with its own prefix `original/gog/prefix/` (OpenGL preset, `deviceCRC` copied from `prefix/` so the stock exe does not fall back to software) and the shortcut "Homeworld Cataclysm (original GOG)" (menu + Desktop). Start: a Wine virtual desktop the size of the saved mode (1920x1080; `--size WxH`), no gamescope (CONSTRAINTS rule 1); a BadWindow death within 20 s retries as a plain window. Fence: hidden smoke runs (headless gamescope, no Wine audio driver, no audio stream seen) reached the main menu ("Version Emergence 1.01", OpenGL through `opengl32.dll`); the registry writer stores a mode and the preset; launcher cases with a stub run_game.sh and stub dialogs: normal, error (dialog with the run's log), BadWindow then plain window, already running, not set up. Found: the exe has no `/noSound` (strings; it shows "Command-line usage" and waits) while the recreation accepts it (HW1 row): a fidelity difference, not changed. Not verified: a start on the user's desktop.
- **release-1.0** (docs/release-1.0.md, docs/release-notes-1.0.0.md, docs/release-{windows,linux,macos}.md, docs/features/data-setup.md; README "Download and play"): the user: "make a private repo for the stable release of the mod ... a release for windows, linux, and mac os ... versions start at 1.0.0 ... all the user needs is a gog installation or the 300MB offline setup ... refrain from having a user have to run console commands". Private repo https://github.com/Fivgo/Homeworld-Infection (main pushed). meson version 1.0.0. Four parallel jobs merged into m2: **data-setup** (src/SDL/datasetup.c: HW_Data, the cwd, gamedata-path.txt, the options row, auto-detect (registry, GOG/Heroic/Lutris/Wine/Bottles/Proton/CrossOver folders, an earlier unpack), the offline installer in Downloads/Desktop/home (innoextract beside the binary, software-rendered progress window), else a dialog; no dialog without a display: exit 0xfed5; HWI-TEST /catDataSetupScript; File.c fileOpen: settings files never come from the game folder (it overwrote original/gog/game/Cataclysm.cfg during the first tests: the user's original-game options of 17:07 were lost, the file reset to GOG's language=0)), **Windows** (NSIS per-user installer from tools/release/build-windows.sh, movies on, minimal FFmpeg 6.1.2, innoextract 1.9, icon + VERSIONINFO; CI smoke on windows-latest), **Linux** (AppImage in a tar.gz on ubuntu-22.04, static type-2 runtime, SDL2 2.30.12, AppRun menu entry), **macOS** (port: darwin meson branch, legacy GL 2.1 + HiDPI, CONFIGDIR Library/Application Support/Homeworld-Infection, fsqrt, -psn; universal dmg, ad-hoc signed, from macos-15). Integration fixes: every function declared (cameraMinimumZoom's float was read as an int in cat_ccZoomScripted; isnan was an undeclared external call on mingw in B-Spline.c), -Wno-implicit-function-declaration removed, _WIN32_WINNT 0x0601. The original icon tools/release/icon. .github/workflows/release.yml: a v tag makes a DRAFT release with the three packages + SHA256SUMS.txt. Fence on the merged tree: Linux debug/release and win64 0 warnings; mission 6000 IDENTICAL to m3r3i-e-mission-a (software GL while the user played FFXIV); modequit, modequit-f, resize, toggle, uiscale, uiscale-4k PASS (rc 1 only from Mesa llvmpipe's own allocations at exit). v1.0.0 tagged at d49a990; the CI made a DRAFT release (Windows-Setup.exe 3.7 MB, Linux tar.gz 4.9 MB, macOS dmg 9.6 MB, SHA256SUMS.txt). 2026-10-30, with the user's permission: the draft's own Linux and Windows packages run hidden with the game files (docs/release-1.0.md "Verification": unpack, saved path, registry detection, intros, menu, mode switch, quit all PASS; the NSIS stub crashes under GE-Proton 10-34 but installs under Proton Experimental), and the hidden-x11 pair PASS. Not verified: a real Windows PC, any Mac. The user asked whether the name is legally safe (the "Homeworld" trademark): answered not legal advice; a rename, if wanted, belongs before the draft is published.
- **video-options + screen-background + assets-folder** (2026-10-31; docs/features/screen-background.md, docs/features/assets-folder.md): the user: "add a dedicated folder for the textures and assets ... so that they can be modified in a blender environment ... if those assets are ever updated ... the game will use those updated assets", "the UI scale and smooth motion settings are put to the side where they are in the complete black. Please provide the background image so I can edit it in gimp", "the video settings menu in game is out of alignment ... the smooth motion toggle could be moved inline", "the sharp textures option does the same work as the original \"surface filtering\" option ... remove that section". Two parallel jobs merged into m2 (77c77c9, ce74435). **video-options**: SHARP TEXTURES checkbox and `/sharpTextures` removed (SURFACE FILTERING, `/noFilter`, trFilterSync, the texLinearFiltering row kept); SMOOTH MOTION one row (box + label) on both Video screens; the added controls on the screens' grid (front end under the tabs at x 502; in game UI SCALE on the VIDEO SETTINGS row); FULL DETAIL's label on its box border and the NUMBER OF EFFECTS knob are the exe's own (.fib / ferDrawHorizSlider), left. **screen-background** (src/Game/ScreenBackground.c): `<settings>/Assets/feman/backgrounds/<Screen>.png` drawn over the 640x480 area under the base atom and inside the FAF_Background fills; looked at on open and once a second; HWI-TEST `/catScreenBgExport`, `/catScreenBgDir`, `/catFeClick`, harness `--settings-file`. The user's GIMP files (Video_Options.ora, Video.ora, reference/controls PNGs, README.txt) in original/play-home/.homeworld/Assets/feman/backgrounds/. **assets-folder** (src/Game/AssetsFolder.c, src/SDL/assettex.c; File.c / texreg.c hooks): a file in `<settings>/Assets` with a game path replaces it (reads only; not settings, writes, absolute paths); PNG for .lif with `_teambase`/`_teamstripe` masks; `assets-manifest.txt` (size, mtime, sha256) so only changed files count; atlas pages (`pageN.lif`, 702 pages) rebuilt at start-up from changed sources via `assets-pages.txt`; `/assetsPath`, `/noAssets`; tools/assets/export_assets.py (`--out`, `--status`, `--refresh`); the user's export in original/play-home/.homeworld/Assets (25,413 files, 161 MB). Not done: edited OBJ meshes are not read (native .geo/.peo are); the repaint shows at the next start. Fence on 90afca4: Linux debug/release and win64 0 warnings; mission 6000 IDENTICAL to m3r3i-e-mission-a; toggle (drives the exe's SURFACE FILTERING box), uiscale, uiscale-4k, modequit, resize, fescreens 195/195 + options 35/35 PASS; ASan 0. The v1.0.0 draft (d49a990) predates this: rebuild before publishing. **user-files** (7871e55, the same day): the user: "let's keep everything that can be intentionally edited by the user, in viewable space. I want to refrain from causing confusion" (the first Assets folder sat in the hidden settings folder). The player's own files now live in `Documents/Homeworld Infection` (`fileUserFilesPath`, datasetup.c dsDocumentsDir: Windows FOLDERID_Documents, Linux XDG_DOCUMENTS_DIR else ~/Documents, macOS ~/Documents; `/userFilesPath`): `Assets/` (the screen pictures follow it: assetsFolderPath) and `Screenshots/` (screenshot.c; HorseRace's dev ShotList stays in the settings folder). Settings, keys and saves stay in the settings folder (game-managed). tools/play.sh passes the main checkout's top folder (`Homeworld-Infection/Assets`, `/Screenshots`, both gitignored); the harness passes `<run>/user` on every run and has `--user-file SRC:REL`; export_assets.py defaults to the project's Assets/. The user's export and GIMP files were moved there with their file times. Fence: the picture from the new place draws byte-identical to the earlier capture (only the version text differs), the old place is ignored, sp's debug screenshots land in `<run>/user/Screenshots`; mission 6000 IDENTICAL, toggle, uiscale, modequit, fescreens PASS, ASan 0; Linux release and win64 0 warnings. Not run on Windows or macOS (macOS asks once for Documents access).

## M3-R3: COMPLETE 2026-10-26 (the third play-test fix round; run from the user's terminal session 38027cce, never the orchestrator)

cycle/workflows/m3-r3.js (dae51e1; run id `wf_8a0247ed-904`, `ROUND_BASE 7aa3111`) finished in session 38027cce (Fable): the "## M2" subsection "M3-R3: COMPLETE" above has the exit test, the five ports and the plan for M3-R4; docs/m3-r3-summary.md the full report and the refreshed play-test brief. The run was cut off by the session's end on 2026-10-25 ~20:30 during the mines audit and resumed on 2026-10-26 with the same run id (the mines audit re-run from scratch, then engine-trails and its audit, then the integrate, itself continued across a context restore); the five audits checkpointed `c8f97bb` 0.94, `292a6da` 0.78, `96b0137` 0.94, `4ae7d91` 0.95, `6ad69ec` 0.944 (all PASS-WITH-FIXES); the integrate's checkpoint is the commit after `6ad69ec`. Exit test MET from clean builds of `6ad69ec` (225 runs `m3r3i-e-*`, ASan 0 in all; every self-test PASS, the toggle self-test under every switch; presentation only: the Mission01 stream IDENTICAL to M3-R1's `1ae66610c3f02410` with every setting of the round flipped, the rung 2823 / 2871, 17 / 17 missions IDENTICAL, the default dumps byte-equal; the five fences rerun with numbers, the 4K cost on the idle GPU after the correctness chains). The user's decisions recorded as 133-144; the five open look questions and M3-R2's four unreported checks are in the brief; no queue for M3-R4 until the play-test reports or a look question is answered the exe's way. The launching session updates the workflow table row.

**As launched (2026-10-25 15:24):** 
Run id wf_8a0247ed-904 (task wr55jil3n), script `cycle/workflows/m3-r3.js` at dae51e1, ROUND_BASE 7aa3111 (the M3-R2 integrate:
the last commit touching src/). Five ports in sequence, each audited: settings-and-keys (item 0 the overlay's F key, the
/letterboxBehind toggle-check fix, SHARP TEXTURES: the user's 2026-10-25 mandate, default = the original look), pause-freeze
(item 5), mission02 (items 1 + 2: the dust belts, the Bushan-Re hint), mines (items 4 + 6), engine-trails (item 7); then the
Fable integrate (the M3-R2 gates + the five fences, docs/m3-r3-summary.md, this file). The user's words are verbatim in
`cycle/answers/m3-r3-user.md`. RESUME after a cut-off: `git -C <main> merge --ff-only m2`, then
`Workflow({scriptPath: "<main>/cycle/workflows/m3-r3.js", resumeFromRunId: "wf_8a0247ed-904"})`; on a usage-limit
failure, a one-shot "please continue your work" cron a few minutes past the reset (the user's standing rule).

**RESUMED 2026-10-26 (session 38027cce restored after its end on 2026-10-25 ~20:30).** Ports 1-3 (settings-and-keys c8f97bb, pause-freeze 292a6da, mission02 96b0137) are audited and on main. The mines producer finished (journal: result cached; its report `docs/m3-r3-mines.md` and its src/ changes sit uncommitted in the m2 worktree); its auditor started 20:14 and was cut off by the session's end (its reverted-build scratch worktree `original/scratch-m2/wt-audit-m3r3-mines` removed on resume, diff kept in the session scratchpad). Relaunched with `resumeFromRunId wf_8a0247ed-904`: the cached chain replays, the mines audit re-runs from scratch, then engine-trails + audit, then the Fable integrate. The usage limit was not active at the relaunch.

## M2-R6: COMPLETE 2026-10-18 (run from the user's terminal session 38027cce, not from the orchestrator)

cycle/workflows/m2-r6.js finished in session 38027cce (Fable, Remote Control): the "## M2" subsection "M2-R6: COMPLETE" above has the exit test and the seven ports. **M2-R7 also finished in that session on 2026-10-19** (cycle/workflows/m2-r7.js, run id wf_b238b029-e72; its integrate step was cut off by the weekly limit on 2026-10-18 and resumed on 2026-10-19 after the skirmish-start merge; "M2-R7: COMPLETE" above, docs/m2-r7-summary.md). **M2-R8 also finished in that session on 2026-10-19** (cycle/workflows/m2-r8.js, run id wf_74bcf42e-330; "M2-R8: COMPLETE" above, docs/m2-r8-summary.md). **M2-R9 also finished in that session on 2026-10-20** (cycle/workflows/m2-r9.js, run id wf_c0cd6893-248; "M2-R9: COMPLETE" above, docs/m2-r9-summary.md). **M2-R10 also finished in that session on 2026-10-20** (cycle/workflows/m2-r10.js, run id wf_dd85ad7e-70e; "M2-R10: COMPLETE" above, docs/m2-r10-summary.md). The orchestrator must relaunch none of them. Merges to main are handled by the user.

## M3-R1: COMPLETE 2026-10-21 (the play-test fix round; run from the user's terminal session 38027cce, never the orchestrator)

cycle/workflows/m3-r1.js (18281e2; run id `wf_42a468c8-942`, `ROUND_BASE ce4fab4`) finished in session 38027cce (Fable): the "## M2" subsection "M3-R1: COMPLETE" above has the exit test, the five ports and the plan for M3-R2; docs/m3-r1-summary.md the full report and the refreshed play-test brief. Relaunched twice on 2026-10-21 within its first half hour (item 6 and the jitter recipe folded in; the main checkout's stale script copy: LESSON kept: the Workflow scriptPath is the main checkout's file; fast-forward main to m2 BEFORE every launch or relaunch). The orchestrator must relaunch nothing. Merges to main are handled by the user. Next: the user's play-test with docs/m3-r1-summary.md's brief (Smooth motion ticked once), then M3-R2 (the Windows 11 build + the one-hour 4K@144 soak + the oracle's rung + the fix items), a new script `cycle/workflows/m3-r2.js` written from the play-test report.

## M3-R2: COMPLETE 2026-10-24 (the second play-test fix round; run from the user's terminal session 38027cce, never the orchestrator)

cycle/workflows/m3-r2.js (944565c; run id `wf_3590992d-400`, `ROUND_BASE 06dae5c`) finished in session 38027cce (Fable): the "## M2" subsection "M3-R2: COMPLETE" above has the exit test, the four ports and the plan for M3-R3; docs/m3-r2-summary.md the full report and the refreshed play-test brief. The run was cut off by the weekly limit on 2026-10-22 after port 1 (bbdc613) and resumed on 2026-10-24 with the same run id (the nis-flicker producer from its WIP snapshot 641a9be); the four audits checkpointed `bbdc613` 0.89, `4b799cb` 0.97, `83c4b26` 0.89, `47922ff` 1.00 (all PASS-WITH-FIXES); the integrate's checkpoint is the commit after `47922ff`. Exit test MET from clean builds of `47922ff` (162 runs `m3r2i-e-*`, ASan 0 in all; one self-test check fails by its expectation (the toggle self-test under `/letterboxBehind`, owner M3-R3); presentation only: the Mission01 stream IDENTICAL to M3-R1's `1ae66610c3f02410`, the rung 2823 / 2871, 17 / 17 missions IDENTICAL, the dumps byte-equal); item 2 (the flicker) not reproduced and instrumented (`/catFrameSig`, the attended play-test settles it). The user's decisions recorded as 122-132; the overlay's F key and the third play-test's seven items are M3-R3's queue (`cycle/answers/m3-r3-user.md`). The launching session updates the workflow table row.

**As launched (2026-10-22 11:00):** 
cycle/workflows/m3-r2.js (944565c; `ROUND_BASE 06dae5c`, after the three "M3 Windows build" commits of 2026-10-21 made by the user's other session, which zipped an untested win64 build: docs/m3-windows-build.md), run id `wf_3590992d-400`, launched on the user's word ("try and fix up those issues in r2 with the remaining tokens we have. be prepared for the interruption"). Four Opus ports in sequence, each with its auditor, then the Fable integrate: instrumentation (the settings line, the overlay, /frameLog + tools/frame_log_report.py, the quit-after-resolution-change fault), nis-flicker (the one-frame flicker in a Mission01 cutscene, found by a dump outlier scan), hyperspace-assembly (the gate over the whole assembly, every part under one clip reveal; the exe's UpdateArrivingShip waits are the lead), mesh-batching (FULL DETAIL under 6.94 ms p99 at 4K behind /meshImmediate). The user's answers live in `cycle/answers/m3-r2-user.md` (f47d384). THE STREAM RULE: the Mission01 baseline IDENTICAL, the rung 2823 / 2871, only producer 3's stated arrival moves. If cut off by a limit: `Workflow({scriptPath: cycle/workflows/m3-r2.js, resumeFromRunId: 'wf_3590992d-400'})` from that session after `git -C <main> merge --ff-only m2`; in a fresh session relaunch the script (audited ports are skipped by the probe). The user's standing rule: a one-shot cron "please continue your work" a few minutes past the reset.

**Cut-off and resume (2026-10-22 → 2026-10-24).** The weekly limit (reset 2026-10-23 23:00 Denver) cut the run after
port 1: instrumentation PASS-WITH-FIXES at bbdc613 (stream 8d32ad5a04b0ff43 IDENTICAL; the quit-after-resolution-change
fix proven; a soak finding: the heap grows about 4 MB a minute in a 600 s sp run, "dupe trcolorinfo list" blocks 912 by
Mission06, owner M3 soak). The nis-flicker producer was cut mid-subsystem (a); its partial work is the WIP snapshot
641a9be (per-frame signal column /catFrameSig, tools/dump_outliers.py), which the resumed producer continues per the
script's RESUME RULE. Resumed 2026-10-24 on the user's word with the same run id (resumeFromRunId wf_3590992d-400; main
fast-forwarded to 1e7cffa first). The user's third play-test (missions 2 to 4, seven reports) is recorded verbatim in
cycle/answers/m3-r3-user.md for the NEXT round, not this one.

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
