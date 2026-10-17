# Homeworld-Infection: project plan

Status date: 2026-10-08. Goal: a 1-to-1 recreation of Homeworld: Cataclysm (GOG "Homeworld: Emergence", build 12948 = retail 1.01) that builds and runs natively on Linux and Windows 11 with no wrapper, then the user's additional features on top. Rules in [CONSTRAINTS.md](CONSTRAINTS.md); work cycle and resume point in [../cycle/STATE.md](../cycle/STATE.md).

## What we are working from

| Source | What it gives | Where |
|---|---|---|
| The GOG executable (5.37 MB, MSVC 6, linked 2000-12-21) | 11,194 functions, 2,828 named at import (CET anchors + library signatures), 8,310 unnamed; plain code, no live protection | [binary-fingerprint.md](binary-fingerprint.md), `ghidra/exports/cataclysm_gog/` |
| Homeworld 1 source (Relic, 2003) | The engine Cataclysm forked: use the unmodified tree (Gardens of Kadesh root commit `ad1bb82`) as the 1:1 template, the SDL ports as the platform-layer donor | [hw1-engine-core.md](hw1-engine-core.md), [-gameplay](hw1-engine-gameplay.md), [-presentation](hw1-engine-presentation.md), [hw1-port-lessons.md](hw1-port-lessons.md) |
| Cataclysm Engine Tweaks | 184 verified addresses in this exact exe, reconstructed structs, the rGL renderer ABI | [cet-knowledge.md](cet-knowledge.md), `ghidra/cet_symbols.json` |
| Game data | 16,567 files, HW1 `.big` format unchanged, 145 ship types, 17 missions + 11 cinematic levels + tutorial, meshes moved to version 0x500 with a new `.peo` type | [big-archive-contents.md](big-archive-contents.md), `tools/bigfile.py` |
| The KAS function table embedded in `.data` at 0x755048 | 487 entries of {script name, C function name, typed parameters}: a built-in symbol table for the mission runtime | `ghidra/proposals/missions.json`, [mission-code-map.md](mission-code-map.md) |
| A native Homeworld 1 build on this machine | `reference/build-hwsdl/homeworld` (ELF x86-64, 75 MB) proves the toolchain and gives a live reference engine | [hw1-port-lessons.md](hw1-port-lessons.md) |

## Milestones

### M0: stable run of the GOG build on this machine — reached 2026-10-08, sign-off pending
- Entry: extracted game, Wine prefix, CET installed. Done ([RUNBOOK.md](RUNBOOK.md)).
- Exit: the user plays a full mission and a skirmish, at least an hour, on the no-gamescope path, with no crash: `tools/run_game.sh --no-gamescope --virtual-desktop 1920x1080`. Evidence so far is the main menu and a clean exit ([m0-first-run.md](m0-first-run.md)).
- Proof tool: `game-crash-watch.py last`, the CET log, `tools/screenshot.sh`.

### M1: binary map (in progress)
Name and classify every function, so the decompilation knows what each piece is.
- Entry (done): Ghidra project with CET's 184 anchors applied; headless pipeline exports functions, strings, call graph, decompiled C.
- Work: library carving (CRT, WON/Titan SDK, Bink: roughly a third of the code, to be excluded), HW1 string matching, the KAS table (487 names for free), ship-type function tables (145 types), mission functions, core and simulation subsystems. Bulk naming by local Qwen during rate-limit windows; Opus audits samples; one serialized Ghidra apply per round.
- Exit: at least 80% of game-code bytes (libraries excluded) carry a name with evidence; every HW1 source file has at least one matched function or a documented reason; the 17 missions' entry points are located; `docs/binary-map.md` reports coverage and the ranked unnamed remainder.
- Proof tool: `tools/ghidra_headless.sh` re-export plus the coverage tables in `binary-map.md`.

### M2: reimplementation (decision needed, see below)
- Entry: M1 exit, and the user's approval of the approach.
- Exit: a single C source tree that compiles on Linux and Windows 11 and plays mission 1 and a skirmish indistinguishably from the GOG build, with the determinism check below passing.
- Proof tool: the engine's own lockstep checksum. Homeworld's multiplayer sync compares a per-tick universe checksum between peers; a recreated build that joins a LAN game against the stock GOG build and stays in sync for a whole match is, by the engine's own definition, behaving identically. Until the network layer works, a CET-style hook dumps the same checksum stream from the GOG build for offline comparison.

### M3: native Linux and Windows 11 builds
- Entry: M2 exit on one platform.
- Exit: both platforms build from one tree in CI, own their window (SDL2, no gamescope, no Wine), load the same GOG data files, and run a one-hour soak at 3840x2160 and 144 Hz with stable frame pacing (docs/features/high-refresh-4k.md).
- Proof tool: CI matrix; a frame-time log over the soak.

### Later: additional features
Reserved for the user's list once the recreation is stable. **First approved feature (2026-10-17): render-time motion interpolation (gameplay and cutscenes) with an in-game toggle, see docs/features/motion-interpolation.md; implemented in M2-R6 after the NIS port.** **Second target (2026-10-17): 4K at up to 144 fps on a modern system, docs/features/high-refresh-4k.md.** Other candidates raised so far: UI scaling above 640x480 (the front end is fixed-size art), frame pacing investigation, modern resolutions by default.

## M2 approach: recommendation

**Option A, recommended: Homeworld 1 source first, decompile only the delta.**
1. Start the tree from the unmodified Relic source (`ad1bb82`), with the Gardens of Kadesh SDL platform layer as the donor for windowing, input, timing and file paths (their port lessons list exactly what had to change for 64-bit and Linux).
2. For every Homeworld 1 function that the binary map matched, compare the decompiled Cataclysm version with the Homeworld 1 source and port only the differences. Most of the engine should survive this step largely intact; the map tells us how much.
3. Decompile and clean the Cataclysm-only functions by subsystem, in this order: file formats (the version-0x500 mesh and `.peo` loader are new and block everything visual), universe and ships (145 types; CET's headers give struct layouts), UI, AI, LAN networking (the WON lobby stays dead), missions last.
4. Missions: Homeworld 1 generated mission C from KAS scripts with `kas2c`, which is in the source release. With the embedded 487-entry runtime table naming every callable, each mission's decompiled code can be mapped back to KAS-style state machines or kept as cleaned C. Seventeen missions, one at a time, each verified by playing it against the GOG build.
5. Verify continuously with the checksum stream, not by eye.

**Option B, not recommended now: byte-matching decompilation.** Rebuilding the exact MSVC 6 binary is the gold standard of fidelity but needs the period toolchain, is far slower, and targets a 32-bit Windows binary rather than modern platforms. It can be a later purity goal once A works.

## Risks

| Risk | Effect | Mitigation |
|---|---|---|
| Lost Cataclysm source | Everything Cataclysm-specific is decompiled, not read | HW1 template for the shared engine; KAS table; CET anchors; Qwen bulk pass with Opus audit |
| Licence: Homeworld 1 source is under Relic's REULA (non-commercial, modifications assigned to Relic); the recreation is a derivative of Gearbox-owned IP | No commercial use, no redistribution of game data, a GOG copy required to play, same stance as HomeworldSDL and CET | State it in README; keep `original/` out of git; distribute code and patches only |
| Floating-point determinism | Lockstep sync and the checksum test depend on identical float results across compilers and platforms | Build with strict IEEE settings, no fast-math, fixed evaluation order; the checksum test catches drift early |
| Mission code volume | 17 missions plus tutorial and cinematics as compiled state machines | KAS table + kas2c patterns; one mission per cycle; GOG build as oracle |
| New mesh format (0x500, `.peo`) | Nothing renders until it is understood | Early M2 target; the HW1 0x402 loader is the starting point |
| Rate limits | Work stalls | The cycle in `cycle/STATE.md`: checkpoints, cached agents, Qwen during waits |
| Disc release (SafeDisc/SecuROM) | Addresses differ, protection in the way | Never target it; GOG build only |
| gamescope dependence creeping in | Violates the user's rule | Test-only; release recipe forbids it (CONSTRAINTS.md) |

## Decisions the user owns
1. Approve Option A for M2 (or ask for B).
2. M0 sign-off after the play-test.
3. Whether LAN interoperability with stock 1.01 clients is a requirement (it constrains the version check and checksum code) or a nice-to-have.
4. The additional-features list, when ready.
