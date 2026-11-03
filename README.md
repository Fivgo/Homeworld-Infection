# Homeworld Infection

A native recreation of **Homeworld: Cataclysm** (2000) for Windows, Linux and macOS. GOG sells the game as *Homeworld: Emergence*. No game files are included: you need your own copy from GOG.

## Download and play (version 1.1.1)

**You need** your GOG copy of Homeworld: Emergence, in either form:
- installed on Windows with GOG Galaxy or GOG's installer, or
- just GOG's **offline installer**, `setup_homeworld_emergence_<version>.exe`, about 360 MB. On gog.com, open your games library, pick Homeworld: Emergence and choose "Download offline backup game installers". GOG Galaxy's small download stub is not enough.

Download the package for your system from the [Releases page](https://github.com/Fivgo/Homeworld-Infection/releases). No step needs a terminal.

### Windows 10 or 11 (64-bit)
1. Download `Homeworld-Infection-1.1.1-Windows-Setup.exe` and double-click it. If Windows says it protected your PC, click **More info**, then **Run anyway** (the installer is not signed).
2. Follow the installer. It needs no administrator rights, and it adds Homeworld Infection to the Start menu and the Desktop.
3. Start Homeworld Infection. A GOG installation of Homeworld: Emergence is found automatically. Otherwise the game asks for the GOG game folder or the offline installer, and unpacks the installer by itself in about a minute.

### Linux (64-bit PC)
1. Download `Homeworld-Infection-1.1.1-Linux-x86_64.tar.gz` and extract it in your file manager ("Extract Here").
2. Double-click `Homeworld-Infection-1.1.1-x86_64.AppImage`. The first start adds Homeworld Infection to your applications menu.
3. The game looks for an installed copy (for example through Heroic or Lutris) and for the offline installer in your Downloads folder. If it finds neither, it asks for one of them.

### macOS 11 or newer
1. Download `Homeworld-Infection-1.1.1-macOS.dmg`, open it and drag **Homeworld Infection** into Applications.
2. The first time, macOS blocks it because it is not from an identified developer. Open **System Settings**, then **Privacy & Security**, scroll down and click **Open Anyway**, then confirm.
3. The game asks for the offline installer (or a GOG game folder copied from a Windows PC) and unpacks it.

**After the first start** the game remembers where its files are. Pick the resolution, window or fullscreen, and the UI scale under Options, then Video. The GOG files are never changed.

The release notes on the Releases page list the known issues.

### Where the game puts its files

| What | Windows | Linux | macOS |
|---|---|---|---|
| **Your own files**, to open and change: `Assets` (every texture and ship model) and `Screenshots` | `Documents\Homeworld Infection` | `~/Documents/Homeworld Infection` | `~/Documents/Homeworld Infection` |
| Settings and saved games (and on Linux the log, `hwi-engine.log`) | `%APPDATA%\Homeworld-Infection` | `~/.homeworld` | `~/Library/Application Support/Homeworld-Infection` |
| The game files unpacked from GOG's offline installer (about 400 MB) | `%LOCALAPPDATA%\Homeworld-Infection\GameData` | `~/.local/share/homeworld-infection/GameData` | `~/Library/Application Support/Homeworld-Infection/GameData` |
| The program | `%LOCALAPPDATA%\Programs\Homeworld Infection` | the AppImage, wherever you put it | `/Applications/Homeworld Infection.app` |

Only the first row is meant for you to open. `GameData` holds the game's packed archives as GOG ships them, and exists only if the game unpacked the offline installer; a GOG installation it found is only read, never changed. The ships and textures to edit are the copies in `Assets`. On Windows, `Documents` may be in OneDrive; on Linux it is your desktop's Documents folder. On a Mac the game asks once for access to Documents. Uninstalling never removes `Documents/Homeworld Infection`.

### Changing textures and ships (the Assets folder)

At its first start the game fills `Assets` by itself, in the background while the intro plays (about 25,000 files: 90 MB, up to 160 MB of disk space):

```
Documents/Homeworld Infection/Assets/
├── Models/<race>/<ship>/<ship>_lod0.obj    each ship for Blender, with its textures (lod0 = most detail,
│                                           lod1-4 = the far-away versions, ingame/ = as drawn in battle)
├── race_kushan/, race_taidan/, feman/ ...  every texture as a PNG, under the game's own folder and name
├── feman/backgrounds/                      your own pictures behind the menu screens
└── README.txt                              what you can change, and how
```

Repaint a PNG, or open a ship's `.obj` in Blender (File, Import, Wavefront), change it and export it over the same file (File, Export, Wavefront, default settings): the game uses your version from its next start. A changed model only changes how the ship looks; the game still plays with the original shape, so multiplayer and replays are not affected. The game never overwrites a file you changed: delete a file to undo your change, or the whole `Assets` folder for a fresh copy at the next start. The `README.txt` in `Assets` explains the details (see also `docs/features/assets-folder.md` and `docs/features/assets-models.md`).

## Build and play from source (Linux, for developers)

**You need** a 64-bit Linux PC with working OpenGL graphics (tested on Pop!_OS 24.04, the same as Ubuntu 24.04), and the game's **offline installer** from GOG. On gog.com, open your games, pick Homeworld: Emergence and choose "Download offline backup game installers". The file is named `setup_homeworld_emergence_<version>.exe`. GOG Galaxy's small download stub does not work.

1. Get this repository: the green **Code** button, then *Download ZIP*, and unzip it. Or use `git clone`.
2. Put the GOG installer in your `Downloads` folder, or in the repository's folder.
3. Open a terminal in the repository's folder and run:
   ```
   bash setup.sh
   ```
   It installs the build tools (it asks for your password once), unpacks the game from the installer, builds the engine and adds a **Homeworld Infection** shortcut to your applications menu and your Desktop. Running it again is safe: it skips what is done. If the game is already installed somewhere, for example on a Windows drive, run `bash setup.sh /path/to/the/game/folder` instead.
4. Click **Homeworld Infection**.

The Sierra and Barking Dog logos play first. Any key skips the one playing. Pick the resolution, a resizable window and the UI scale under Options, then Video. Your settings, saved games and logs are kept in `original/play-home/`, and the GOG files are never changed. Screenshots go in `Screenshots/` at the top of the repository's folder. To repaint the game's textures: the game fills `Assets/` there with every texture as a PNG and the ships as Blender models at a start that finds no `Assets/assets-manifest.txt` (or run `python3 tools/assets/export_assets.py`), and it uses any file you change from its next start (`docs/features/assets-folder.md`, `docs/features/assets-autoexport.md`). Pictures behind the menu screens go in `Assets/feman/backgrounds/` (`docs/features/screen-background.md`). Both folders stay out of git.

If the game cannot start or stops with an error, the shortcut shows a message with the log's location. To watch the messages yourself, run `tools/play.sh` in a terminal. `bash setup.sh --remove-shortcut` removes the shortcut.

The release packages are built by `.github/workflows/release.yml` (`docs/release-1.0.md`).

## About the project

A reverse-engineering and recreation project for **Homeworld: Cataclysm** (2000, Barking Dog Studios; re-released by GOG in 2017 as *Homeworld: Emergence*). Its source code is lost. It is a fork of the Homeworld 1 engine, whose 2003 source release and community ports give us the template, and the Cataclysm Engine Tweaks project gives us verified anchors in the GOG executable. The target is a native build for Windows, Linux and macOS that plays identically to the GOG build, with no compatibility wrapper, then the user's additional features.

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
