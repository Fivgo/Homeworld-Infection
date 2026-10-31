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
4. **Per-user folders.** Settings (options, keys, saves): Windows `%APPDATA%\Homeworld-Infection`
   (exists); Linux `~/.homeworld` (unchanged: play.sh and the harness set HOME); macOS
   `~/Library/Application Support/Homeworld-Infection`. The player's own files, in a visible folder
   (HWI-FEATURE(user-files), 2026-10-31): `Documents/Homeworld Infection/Assets` (replacement textures, menu pictures)
   and `Documents/Homeworld Infection/Screenshots` on all three systems (`/userFilesPath` moves it). Game data unpacked from the offline installer: Windows
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

## Verification of the v1.0.0 packages (2026-10-30, the draft release's own files)

The three files were downloaded from the draft release and checked against its SHA256SUMS.txt. Every run was hidden
and silent, with a throwaway home or Wine prefix in the session scratchpad and the user's offline installer
(`original/setup_homeworld_emergence_1.01_hotfix_(12948).exe`) or links to `original/gog/game`'s 66 files.

| Package | Run | Result |
|---|---|---|
| Linux tar.gz | extracted (the AppImage keeps mode 755), started through FUSE with the installer linked into `~/Downloads`, `/catDataSetupScript unpack /catHiddenWindow /catModeQuitTest 1280x720:120`, SDL dummy audio, stub dialog programs | found and unpacked by the bundled innoextract (66 files, 404,999,701 bytes, all match MANIFEST.sha256), the path saved, both intros through the bundled FFmpeg (240/240 and 807/807 frames, 4:3), the switch PASS, 120 frames, rc 0, the menu entry `homeworld-infection-appimage.desktop` written, no dialog |
| Linux, second start | `/catDataSetupScript quit /disableAVI ...` | the saved path used without a question, PASS, 3 s |
| Windows Setup.exe | `/S` under GE-Proton 10-34 | the NSIS stub crashes in Wine (page fault in its 32-bit code at 0x00439474: the same GE relocation fault as GOG's Inno stubs, docs/RUNBOOK.md); not a Windows fault (CI's windows-latest install passes) |
| Windows Setup.exe | `/S` under Proton Experimental 11.0-20261014, in a headless gamescope | rc 0, every file installed |
| Windows game, first start | `/catDataSetupScript "show;installer:Z:\...exe"` in a headless gamescope, Wine audio drivers off | the bundled innoextract.exe unpacked the installer (66 files match), the progress window shown (captured), the Barking Dog intro captured at 4:3 in the 1440-wide centre; gamescope's headless backend then segfaulted at the window's resize (a harness fault) |
| Windows game, second start | headless gamescope, `/disableAVI` | the saved path used, the main menu captured, "Emergence 1 1.0.0" |
| Windows game, mode switch + quit | Wine on the real display with `/catHiddenWindow`, output redirected by a .bat | saved path, `/catModeQuitTest 1280x720:120` PASS, 120 frames, rc 0 |
| Windows registry detection | GOG's key `HKLM\Software\Wow6432Node\GOG.com\Games\1766413869` `path` = a folder of links to the 66 files; the saved path and GameData moved aside | found in both registry views, used without a question, saved, PASS, rc 0; nothing written into the game folder |
| Engine on the NVIDIA display | `--mode modequit --res 2560x1600 --mode-to 3840x2160:120 --hidden-x11`; `--mode resize --res 2560x1600 --hidden-x11` (debug ASan build of the tagged tree) | PASS, 120 frames, 0 X errors, quit PASS, rc 0; resize PASS; ASan 0, no leak |

Not verified: a real Windows PC, any Mac, the uninstaller's components page by hand, SmartScreen and Gatekeeper.
