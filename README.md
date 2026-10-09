# Homeworld-Infection

A reverse-engineering and recreation project for **Homeworld: Cataclysm** (2000, Barking Dog Studios; re-released by GOG in 2017 as *Homeworld: Emergence*). Its source code is lost. It is a fork of the Homeworld 1 engine, whose 2003 source release and community ports give us the template, and the Cataclysm Engine Tweaks project gives us verified anchors in the GOG executable. The target is a native build for Linux and Windows 11 that plays identically to the GOG build, with no compatibility wrapper, then the user's additional features.

**Legal stance.** This is non-commercial research on a game the author owns. The Homeworld 1 source is used under Relic's non-commercial licence; the recreation is a derivative of IP now owned by Gearbox. No game data or binaries are in this repository, and a GOG copy of the game is required to run anything.

## Layout

| path | contents |
|---|---|
| `docs/` | the knowledge base and plan (index below) |
| `tools/` | extraction, Wine prefix, CET install, launch, screenshots, Ghidra pipeline, `.big` archives, the local Qwen worker, checkpointing |
| `ghidra/` | CET symbols, analysis scripts, proposal sets; projects and exports are generated and ignored |
| `cycle/` | `STATE.md` resume point and copies of the workflow scripts |
| `original/`, `reference/`, `prefix/` | game files, third-party clones, Wine prefix: ignored by git |

## Getting started
Read `docs/RUNBOOK.md` (installer to running game) and `cycle/STATE.md` (where the work stands).

## Docs
- `PLAN.md`: milestones, M2 approach, risks, open decisions
- `CONSTRAINTS.md`: the user's standing rules
- `m0-first-run.md`: the first stable run and its recipe
- `binary-fingerprint.md`: what the executable is
- `big-archive-contents.md`: what the data archives contain
- `hw1-engine-core.md`, `hw1-engine-gameplay.md`, `hw1-engine-presentation.md`: the Homeworld 1 engine mapped for the port
- `hw1-port-lessons.md`: how the community ported Homeworld 1, and a native build of it
- `cet-knowledge.md`: everything Cataclysm Engine Tweaks knows about the GOG exe
- `ghidra-pipeline.md`: the headless analysis pipeline and its exports
- `mission-code-map.md`: the KAS runtime table and mission code findings
- `RUNBOOK.md`: step by step from download to a game window
