**Homeworld Infection 1.0.0** is the first full release: a native recreation of the engine of *Homeworld: Cataclysm* (2000) for Windows, Linux and macOS. No game files are included. You need your own GOG copy of *Homeworld: Emergence* (Cataclysm's GOG name): either an installed copy, or just GOG's offline installer `setup_homeworld_emergence_<version>.exe` (about 360 MB; on gog.com: your library, Homeworld: Emergence, "Download offline backup game installers").

## Downloads

| System | File |
|---|---|
| Windows 10 or 11, 64-bit | `Homeworld-Infection-1.0.0-Windows-Setup.exe`: an installer (no administrator rights needed; Start menu and Desktop shortcuts; uninstall from Settings, Apps) |
| Linux, 64-bit PC (2022 or newer, e.g. Ubuntu 22.04+) | `Homeworld-Infection-1.0.0-Linux-x86_64.tar.gz`: extract it, double-click the AppImage inside; the first start adds a menu entry |
| macOS 11 or newer (Apple Silicon and Intel) | `Homeworld-Infection-1.0.0-macOS.dmg`: drag the app to Applications |

`SHA256SUMS.txt` lists the files' checksums. No step needs a terminal.

## First start

The game looks for your copy of Homeworld: Emergence by itself: a GOG installation (on Windows, also one made by GOG Galaxy), a copy installed through Heroic, Lutris or Wine on Linux, or the offline installer in your Downloads or Desktop folder, which it unpacks for you in about a minute. If it finds nothing it asks you for the game folder or the offline installer. It remembers the answer, and it never changes the GOG files.

The packages are not signed. Windows may show "Windows protected your PC": click **More info**, then **Run anyway**. macOS blocks the first start: open **System Settings**, **Privacy & Security**, and click **Open Anyway**.

## What is in 1.0.0

- The single-player campaign (Missions 1 to 17) and skirmish games against the computer.
- Any resolution, in a window, a borderless window or fullscreen, with an automatic UI scale for large screens (Options, Video).
- Smooth motion, full detail at every distance, and the original intro logos and cutscene movies in their own shape.
- Your settings and saved games are kept apart from the GOG game: `%APPDATA%\Homeworld-Infection` (Windows), `~/.homeworld` (Linux), `~/Library/Application Support/Homeworld-Infection` (macOS).

## Known issues

- **Windows:** tested under Wine with the game files (unpacking the offline installer, finding a GOG installation through its registry entry, the intro movies, the main menu, a resolution change and Quit), and installed, started and uninstalled on a Windows test machine; not yet played on a Windows PC.
- **macOS:** built and checked automatically on Apple Silicon (the Intel half too); not yet played on a Mac.
- **Linux:** the AppImage is tested on Pop!_OS 24.04 with the game files (the offline installer found in Downloads and unpacked, the intro movies, the main menu, a resolution change, the menu entry); please report other distributions and desktops. The window uses X11 (XWayland on Wayland desktops).
- The hyperspace gate can look the wrong size for the Kith ship, and parts of ships can pop in just after a hyperspace exit.
- A rare one-frame flicker in a cutscene.
- FULL DETAIL costs frame rate in big battles: untick it in Options, Video if a battle stutters.
- Quit may need a second try after a resolution change.
- No LAN or internet play.
- Saved games do not load in the GOG game, and the GOG game's saves do not load here.

## Licences

The engine is based on Relic Entertainment's Homeworld source code, released under the Relic End User License Agreement: non-commercial use only. Each package carries the licences of what it bundles: SDL2 (zlib), FFmpeg (LGPL 2.1, with its source location) and innoextract (zlib, with its libraries). Homeworld is a trademark of its owner; this is a fan project, not affiliated with or endorsed by Relic, Barking Dog, Sierra, Gearbox or GOG.
