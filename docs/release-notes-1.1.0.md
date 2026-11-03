**Homeworld Infection 1.1.0** adds ship modding: the game now writes every texture and every ship model into a folder you can edit, and uses your changed textures and Blender models from its next start. Homeworld Infection is a native recreation of the engine of *Homeworld: Cataclysm* (2000) for Windows, Linux and macOS. No game files are included. You need your own GOG copy of *Homeworld: Emergence* (Cataclysm's GOG name): either an installed copy, or just GOG's offline installer `setup_homeworld_emergence_<version>.exe` (about 360 MB; on gog.com: your library, Homeworld: Emergence, "Download offline backup game installers").

## Downloads

| System | File |
|---|---|
| Windows 10 or 11, 64-bit | `Homeworld-Infection-1.1.0-Windows-Setup.exe`: an installer (no administrator rights needed; Start menu and Desktop shortcuts; uninstall from Settings, Apps) |
| Linux, 64-bit PC (2022 or newer, e.g. Ubuntu 22.04+) | `Homeworld-Infection-1.1.0-Linux-x86_64.tar.gz`: extract it, double-click the AppImage inside; the first start adds a menu entry |
| macOS 11 or newer (Apple Silicon and Intel) | `Homeworld-Infection-1.1.0-macOS.dmg`: drag the app to Applications |

`SHA256SUMS.txt` lists the files' checksums. No step needs a terminal.

## First start

The game looks for your copy of Homeworld: Emergence by itself: a GOG installation (on Windows, also one made by GOG Galaxy), a copy installed through Heroic, Lutris or Wine on Linux, or the offline installer in your Downloads or Desktop folder, which it unpacks for you in about a minute. If it finds nothing it asks you for the game folder or the offline installer. It remembers the answer, and it never changes the GOG files.

The packages are not signed. Windows may show "Windows protected your PC": click **More info**, then **Run anyway**. macOS blocks the first start: open **System Settings**, **Privacy & Security**, and click **Open Anyway**.

## What is new in 1.1.0

- **The Assets folder fills itself.** At its first start the game writes `Documents/Homeworld Infection/Assets` in the background (about 6 seconds while the intro plays, about 90 MB): every texture as a PNG at the game's own path, every ship as a model for Blender (`Models/<race>/<ship>/<ship>_lod0.obj`), and a `README.txt` that explains what you can change. Nothing to install and no tool to run, on Windows, Linux and macOS.
- **Repaint textures.** Change a PNG and the game uses it from the next start, at any size. Small textures that the game combines into pages are rebuilt into their page by themselves.
- **Reshape ships in Blender.** Import a ship's `.obj`, change it (keep the part names: guns, engines, docking and animations find parts by name), and export it over the same file with Blender's default settings. The game turns it back into its own model format at the next start. A changed full-detail model (lod0) is drawn at every distance unless you change the lower levels too, and a material can point at a new PNG of your own.
- **Looks only.** An edited model never changes play: the game keeps the original shape for where shots hit and where repair droids walk, so multiplayer games and replays never depend on anyone's Assets folder.
- **Safe to experiment.** Only files you change are used. The game never overwrites them, even when a newer version refreshes the folder. Delete a file to undo it, or delete the whole folder to get a fresh copy at the next start.
- **Fixed:** small specks and stray lines on the menu frames at a 150% UI scale (for example 2560x1440).

## Known issues

- **Windows:** tested under Wine with the game files (unpacking the offline installer, finding a GOG installation through its registry entry, the intro movies, the main menu, a resolution change and Quit), and installed, started and uninstalled on a Windows test machine; not yet played on a Windows PC.
- **macOS:** built and checked automatically on Apple Silicon (the Intel half too); not yet played on a Mac.
- **Linux:** the AppImage is tested on Pop!_OS 24.04 with the game files (the offline installer found in Downloads and unpacked, the intro movies, the main menu, a resolution change, the menu entry); please report other distributions and desktops. The window uses X11 (XWayland on Wayland desktops).
- The hyperspace gate can look the wrong size for the Kith ship, and parts of ships can pop in just after a hyperspace exit.
- A rare one-frame flicker in a cutscene.
- FULL DETAIL costs frame rate in big battles: untick it in Options, Video if a battle stutters.
- Quit may need a second try after a resolution change.
- No LAN or internet play.
- An edited ship model only changes how the ship looks: shots still hit, and repair droids still sit, where the original hull is, so a much smaller or larger model can look hit in empty space or inside its hull. Its collision size, gun and engine positions, engine trails and docking points stay the original's.
- Blender drops a face that exactly duplicates another face, so a ship drawn with two back-to-back sheets (the Worker's) becomes one-sided after a round trip.
- On a Mac the first start asks for access to Documents, where the game writes the Assets folder.
- Saved games do not load in the GOG game, and the GOG game's saves do not load here.

## Licences

The engine is based on Relic Entertainment's Homeworld source code, released under the Relic End User License Agreement: non-commercial use only. Each package carries the licences of what it bundles: SDL2 (zlib), FFmpeg (LGPL 2.1, with its source location) and innoextract (zlib, with its libraries). Homeworld is a trademark of its owner; this is a fan project, not affiliated with or endorsed by Relic, Barking Dog, Sierra, Gearbox or GOG.
