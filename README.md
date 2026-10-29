# Homeworld-Infection

A native Linux recreation of **Homeworld: Cataclysm** (2000), sold by GOG as *Homeworld: Emergence*. No game files are included: you need your own copy from GOG.

## Play it (Linux)

**You need** a 64-bit Linux PC with working OpenGL graphics (tested on Pop!_OS 24.04, the same as Ubuntu 24.04), and the game's **offline installer** from GOG. On gog.com, open your games, pick Homeworld: Emergence and choose "Download offline backup game installers". The file is named `setup_homeworld_emergence_<version>.exe`. GOG Galaxy's small download stub does not work.

1. Get this repository: the green **Code** button, then *Download ZIP*, and unzip it. Or use `git clone`.
2. Put the GOG installer in your `Downloads` folder, or in the repository's folder.
3. Open a terminal in the repository's folder and run:
   ```
   bash setup.sh
   ```
   It installs the build tools (it asks for your password once), unpacks the game from the installer, builds the engine and adds a **Homeworld Infection** shortcut to your applications menu and your Desktop. Running it again is safe: it skips what is done. If the game is already installed somewhere, for example on a Windows drive, run `bash setup.sh /path/to/the/game/folder` instead.
4. Click **Homeworld Infection**.

The Sierra and Barking Dog logos play first. Any key skips the one playing. Pick the resolution, a resizable window and the UI scale under Options, then Video. Your settings, saved games and logs are kept in `original/play-home/`, and the GOG files are never changed.

If the game cannot start or stops with an error, the shortcut shows a message with the log's location. To watch the messages yourself, run `tools/play.sh` in a terminal. `bash setup.sh --remove-shortcut` removes the shortcut.

**Windows:** an experimental build, without the movies, is made with `tools/release_win64.sh`. See `tools/release/README-Windows.txt`.

## About the project

A reverse-engineering and recreation project for **Homeworld: Cataclysm** (2000, Barking Dog Studios; re-released by GOG in 2017 as *Homeworld: Emergence*). Its source code is lost. It is a fork of the Homeworld 1 engine, whose 2003 source release and community ports give us the template, and the Cataclysm Engine Tweaks project gives us verified anchors in the GOG executable. The target is a native build for Linux and Windows 11 that plays identically to the GOG build, with no compatibility wrapper, then the user's additional features.

**Legal stance.** This is non-commercial research on a game the author owns. The Homeworld 1 source is used under Relic's non-commercial licence; the recreation is a derivative of IP now owned by Gearbox. No game data or binaries are in this repository, and a GOG copy of the game is required to run anything.

## Licence

The engine source in `src/` comes from Relic Entertainment's Homeworld 1 source release (via the HomeworldSDL / Gardens of Kadesh port) and is bound by the **Relic End-User License Agreement (REULA)**: non-commercial use only, modifications assigned to Relic (now the Gearbox-held IP). The full text is in [`src/COPYING.REULA`](src/COPYING.REULA), with a summary and the Gardens of Kadesh notice at the top of [`src/README.md`](src/README.md); where each file came from is in [`src/PROVENANCE.md`](src/PROVENANCE.md). Homeworld is a registered trademark of Gearbox Enterprises, LLC; this project is not affiliated with or endorsed by Relic, Sierra, Gearbox, 2K or Take-Two. The project's own tools and docs carry no separate licence grant yet.

## Layout

| path | contents |
|---|---|
| `src/` | the engine source (REULA; Homeworld 1 engine at M2-R1), built by `meson.build` / `tools/build.sh` into `build/` (ignored) |
| `docs/` | the knowledge base and plan (index below) |
| `tools/` | extraction, Wine prefix, CET install, launch, screenshots, Ghidra pipeline, `.big` archives, the local Qwen worker, checkpointing |
| `ghidra/` | CET symbols, analysis scripts, proposal sets; projects and exports are generated and ignored |
| `cycle/` | `STATE.md` resume point and copies of the workflow scripts |
| `original/`, `reference/`, `prefix/` | game files, third-party clones, Wine prefix: ignored by git |

## Getting started (development)
To play, see "Play it" above. For development, read `docs/RUNBOOK.md` (installer to running game) and `cycle/STATE.md` (where the work stands).

### Comparing with the original game
`tools/play_original.sh --setup` (once) unpacks an untouched copy of the GOG game into `original/gog/` (checked against the installer's file list), gives it its own Wine prefix with the OpenGL renderer selected, and adds a "Homeworld Cataclysm (original GOG)" shortcut next to the recreation's. It needs GE-Proton10-34 and umu-run (`docs/RUNBOOK.md` sections 2 and 4b). The shortcut runs the original in a Wine window the size of its video mode: 1920x1080 at first; change it in the game's Options or with `tools/play_original.sh --size 2560x1440`.

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
