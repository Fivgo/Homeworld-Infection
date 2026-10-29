# Release 1.0.0: Windows, Linux and macOS packages (plan and shared contracts)

The user (2026-10-29): "okay lets make a private repo for the stable release of the mod. I will need a release for
windows, linux, and mac os to be prepared. Have their release versions start at 1.0.0 since this is a full release.
The intention is that all the user needs is a gog installation or the 300MB offline setup to have the mod work. Again,
we want to have a user friendly setup so please refrain from having a user have to run console commands for the
deployment process."

Repository: https://github.com/Fivgo/Homeworld-Infection (private; `main` pushed 2026-10-29).

## Deliverables (a GitHub Release `v1.0.0`)

| Platform | Asset | What the player does |
|---|---|---|
| Windows x64 | `Homeworld-Infection-1.0.0-Windows-Setup.exe` (NSIS, per-user install, no admin prompt; Start menu + Desktop shortcuts; uninstaller in Apps & features) | double-click, Next, Finish; SmartScreen "More info, Run anyway" (unsigned) |
| Linux x86-64 | `Homeworld-Infection-1.0.0-Linux-x86_64.tar.gz` holding `Homeworld-Infection-1.0.0-x86_64.AppImage` (tar keeps the executable bit; static type-2 runtime, so no libfuse2) and a short README | "Extract here", double-click the AppImage |
| macOS | `Homeworld-Infection-1.0.0-macOS.dmg` holding `Homeworld Infection.app` (universal arm64 + x86_64 if feasible; ad-hoc signed; macOS 11+) | open the dmg, drag to Applications; first start: System Settings, Privacy & Security, "Open Anyway" (unsigned) |

No package contains game data. Each carries the engine, SDL2, a minimal LGPL FFmpeg (movies on), innoextract, the
licences (REULA, SDL2 zlib, FFmpeg LGPL-2.1 with its source URL, innoextract zlib and its libraries' licences) and a
player README. No step needs a terminal.

## Shared contracts

1. **Version.** `meson.build` project version `1.0.0`; release builds configure `-Dbuild_version=1.0.0` (the main menu
   and the pause screen show it). Windows VERSIONINFO, the app's Info.plist and the Linux desktop/AppStream data say
   1.0.0. The tag is `v1.0.0`.
2. **The engine finds the game data itself** (HWI-FEATURE(data-setup), `src/SDL/datasetup.c`): no environment
   variable, no working directory. Order: `HW_Data` (play.sh, the harness: unchanged) -> the working directory if it
   holds `cataclysm.big` (the old Windows zip layout) -> the saved path -> an auto-detected GOG install -> an
   auto-found offline installer (offer to unpack) -> ask (a GOG folder or the offline installer). The chosen folder is
   saved as `gamedata-path.txt` in the settings directory. A GOG folder is only read, never written.
3. **innoextract sits next to the engine binary** (found through `SDL_GetBasePath()`): Windows `innoextract.exe`
   beside the exe; Linux `usr/bin/innoextract` beside `usr/bin/hwi-engine` inside the AppImage; macOS
   `Contents/MacOS/innoextract` beside the engine.
4. **Per-user folders.** Settings (options, keys, saves, screenshots): Windows `%APPDATA%\Homeworld-Infection`
   (exists); Linux `~/.homeworld` (unchanged: play.sh and the harness set HOME); macOS
   `~/Library/Application Support/Homeworld-Infection`. Game data unpacked from the offline installer: Windows
   `%LOCALAPPDATA%\Homeworld-Infection\GameData`; Linux `${XDG_DATA_HOME:-~/.local/share}/homeworld-infection/GameData`;
   macOS `~/Library/Application Support/Homeworld-Infection/GameData`.
5. **Minimal FFmpeg** (`tools/release/ffmpeg-min.sh`): FFmpeg 6.1.x, LGPL, shared libraries, only what the movie
   player uses (the bink demuxer, the bink and binkaudio decoders, the file protocol, swscale).
6. **Icon**: original art in `tools/release/icon/` (never the GOG, Sierra, Relic or Homeworld artwork).
7. **CI**: `.github/workflows/release.yml` builds the three packages on GitHub-hosted runners through
   `tools/release/build-{linux,windows,macos}.sh`; a `v*` tag makes a draft Release with the assets. The Linux and
   Windows scripts also run on the development machine.
8. **Player docs**: README.md opens with "Download and play" per platform, no console steps.

## Standing rules that apply

- Never commit `original/`, `reference/`, `prefix/`, `build/`, `ghidra/exports`, `ghidra/projects` or any game-derived
  data; CI never receives game data.
- Never fake mouse or keyboard input. Test runs are silent and never on screen: the harness's offscreen driver,
  `/catHiddenWindow`, or a headless gamescope (a developer screenshot harness only; never part of a package).
- GPU measurements only on an idle GPU.
