# M2 kickoff brief (reimplementation) — approved approach: Option A in docs/PLAN.md

Written 2026-10-10 for the orchestrator. M0 reached (GOG build runs; the user's play-test sign-off is still pending and can happen alongside). M1 complete (docs/binary-map.md rounds 1-5: 14,705 functions, no FUN_* left, 83% of function bytes real-named, 99.1% in the game region, 4,126 prototypes). Everything below keeps docs/CONSTRAINTS.md: no gamescope in any release path, no game data in git, no input injection, gamescope/Proton only as a test harness.

## Option A in one paragraph
Start the recreation from the unmodified Relic Homeworld 1 source (Gardens of Kadesh root commit `ad1bb82`), with the GoK SDL platform layer as the donor for window, input, timing and file paths. For every function the binary map matched to Homeworld 1 (class hw1:*), compare the decompiled Cataclysm body with the HW1 source and port only the differences. Decompile and clean the Cataclysm-only functions (class cat-new) by subsystem. Missions last, using the KAS runtime table and kas2c conventions. Verify by behaviour against the running GOG build, ultimately with the engine's own lockstep checksum.

## M2 rounds (each a workflow in cycle/workflows/, same conventions as the binary-map rounds: incremental saves, resume guards, one adversarial auditor per producer, checkpoint commit)

### M2-R1: the delta inventory (do this first; it is measurement, not porting)
1. **HW1 engine vs Cataclysm data.** The native Homeworld 1 build already exists at `reference/build-hwsdl/homeworld` (see docs/hw1-port-lessons.md). Point it at Cataclysm's data (`original/game_extracted/cataclysm/` and `original/game/cataclysm.big`; HW1 expects `Homeworld.big`, so symlink or copy under the expected names in a scratch assets dir, never inside git). Run it HEADLESS or windowed-and-closed (no play, no input injection) and catalogue every failure and divergence: mesh version 0x500 and `.peo`, new `.script` keys, 145 ship types vs HW1's enum, string tables, `.fib` front-end changes, missing files. Output: `docs/m2-r1-data-deltas.md`.
2. **Loader-format deltas from the binary.** From the named decompiled code (`ghidra/exports/cataclysm_gog_r5` or newest): document the Cataclysm versions of the loaders that changed vs HW1: mesh 0x500 / `.peo`, textures, `.script` parsing (new keys), ship static-info parser (new fields), save-game layout (version 0x10ad0027), `.fib`. One producer per format, each writing `docs/formats/<format>.md` with struct layouts and the HW1 diff. Audited.
3. **Function-level delta map.** For each hw1:* function, classify from the decompiled C vs the HW1 source: IDENTICAL / MODIFIED (describe the change in one line) / REWRITTEN. Script-assisted (string xrefs, call graph, prototypes), audited by sampling. Output: `ghidra/delta_map.json` + a per-HW1-file table in `docs/m2-r1-function-deltas.md` with counts. This tells us how much of the engine is free.
4. **Source-tree bootstrap.** Create `src/` in the repo: the REULA notice and provenance file, an import of the HW1 tree at `ad1bb82` (Game/, Win32/ as reference, Ships/) plus the GoK SDL layer files, a meson build that compiles the UNMODIFIED tree on Linux as `hwi-engine` (reuse what docs/hw1-port-lessons.md learned), and a `tools/build.sh`. No Cataclysm changes yet. Must build; record the exact commands and the binary's size in `docs/m2-r1-bootstrap.md`.
5. **Checksum oracle design.** Read how HW1 computes the multiplayer sync checksum (docs/hw1-engine-gameplay.md, Universe.c) and where Cataclysm's lives (named now). Design, do not yet build, a CET-style hook DLL that dumps the per-tick checksum stream from the GOG build to a file during a scripted skirmish (no input injection: the game's own AI-vs-AI or a recorded game), and the matching dump in the recreation. Output: `docs/m2-checksum-oracle.md`.
Exit criteria for R1: the five documents exist and are audited; `src/` builds the unmodified HW1 engine; STATE.md lists the M2-R2 order with the delta counts.

### M2-R2 onward (the orchestrator plans these from R1's numbers; order from docs/PLAN.md)
file formats (mesh 0x500/.peo first) → universe and ships (145 types) → UI → AI → LAN networking → missions. Each round: port IDENTICAL functions verbatim, apply MODIFIED deltas, decompile-and-clean REWRITTEN and cat-new ones, with the oracle comparison as the gate once it exists.

## Rules for the orchestrator in M2
- Never commit `original/`, `reference/`, build outputs or any game data; `src/` holds source only.
- Keep the HW1 licence notice (REULA, non-commercial) at the top of `src/` and in README.
- Round size: aim for about 10 agents; split producers rather than growing them.
- Record every decision the user must make in STATE.md "Decisions the user owns". The user reads STATE.md and this chat, not the terminal.
