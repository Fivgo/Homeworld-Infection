Homeworld Infection @VERSION@ for Linux (64-bit PC)
==================================================

Built @DATE@ from source commit @COMMIT@.

WHAT THIS IS
Homeworld Infection recreates the engine of Homeworld: Cataclysm. The game's own files (ships, maps, music,
voices) are NOT included. They come from your own copy of Homeworld: Emergence, Cataclysm's name on GOG.

WHAT YOU NEED
- A 64-bit PC running Linux from 2022 or later (for example Ubuntu 22.04 or newer, Linux Mint 21 or newer,
  Pop!_OS, Fedora, Debian 12, Arch, or the desktop mode of a Steam Deck), with working 3D graphics drivers.
- Your GOG copy of Homeworld: Emergence, in one of two forms:
    - installed, for example through the Heroic Games Launcher or Lutris, or
    - GOG's offline installer setup_homeworld_emergence_*.exe (about 300 MB). Download it from your GOG account:
      Games, Homeworld: Emergence, "Download offline backup game installers".

INSTALL AND PLAY
1. Right-click Homeworld-Infection-@VERSION@-Linux-x86_64.tar.gz and choose "Extract Here".
2. Open the new folder Homeworld-Infection-@VERSION@ and double-click Homeworld-Infection-@VERSION@-x86_64.AppImage.
   The AppImage is the whole program. You can first move it anywhere you like, for example into a Games folder.
   If double-clicking does nothing, or opens an archive viewer: right-click the AppImage, Properties, Permissions,
   tick "Allow executing file as program" (or "Is executable"), then double-click it again.
3. The first start looks for the game files:
   - An installed copy in the usual places: ~/GOG Games/Homeworld Emergence (minigalaxy, Wine), the Heroic Games
     Launcher's ~/Games/Heroic/Homeworld Emergence, Lutris, Wine and Bottles prefixes, and a Steam Proton prefix.
   - The offline installer setup_homeworld_emergence_*.exe in your Downloads folder, on the Desktop, in your home
     folder or next to the AppImage. It asks before unpacking it (any setup_homeworld_emergence_*-1.bin parts
     must sit next to the .exe).
   - If it finds neither, it asks you to pick the installed game's folder (the one that holds cataclysm.big) or the
     offline installer.
   An unpacked installer goes into ~/.local/share/homeworld-infection/GameData (about 400 MB, a minute or two).
   Your choice is remembered, so later starts go straight to the game. A GOG installation is only read, never
   changed.

THE MENU ENTRY
Each start adds "Homeworld Infection" to your applications menu, or updates it, pointing at the AppImage where it
is now. If you move the AppImage, start it once from its new place and the menu entry follows. If you delete the
AppImage, the entry disappears from the menu. To remove its file as well, delete
    ~/.local/share/applications/homeworld-infection-appimage.desktop

WHERE THE GAME PUTS ITS FILES
1. Your own files, to open and change: your Documents folder, in Homeworld Infection
       ~/Documents/Homeworld Infection/Assets        every texture as a PNG and every ship as a model for Blender
       ~/Documents/Homeworld Infection/Screenshots   the pictures you take in the game
   (Documents is your desktop's Documents folder: ~/Documents on most systems.)
2. Settings, saved games and the log: the hidden folder ~/.homeworld (in the file manager, Ctrl+H shows hidden
   folders). It holds the settings files Cataclysm.cfg and keys.cfg, your saved games (SavedGames),
   gamedata-path.txt (the remembered game-files folder), and hwi-engine.log, the messages of the last run
   (hwi-engine.log.old has the run before). The GOG game's own saves are not touched.
3. The game files unpacked from GOG's offline installer (about 400 MB, only if you chose the installer):
       ~/.local/share/homeworld-infection/GameData
   These are the game's packed archives as GOG ships them: nothing in them is meant to be edited. The ships and
   textures to change are the copies in Assets. A GOG installation the game found is only read, never changed.
4. The program is the AppImage itself, wherever you put it, and the menu entry is
       ~/.local/share/applications/homeworld-infection-appimage.desktop
Defaults: Smooth motion on, FULL DETAIL on, UI scale automatic (200% on a 4K screen, 150% at 1440p, 100% below).

CHANGING TEXTURES AND SHIPS (THE ASSETS FOLDER)
At its first start the game fills ~/Documents/Homeworld Infection/Assets by itself, in the background while the
intro plays (about 25,000 files: 90 MB, up to 160 MB of disk space). Inside:
    Models/<race>/<ship>/<ship>_lod0.obj     each ship for Blender, with its textures (lod0 = the most detail)
    race_kushan/, race_taidan/, feman/ ...   every texture as a PNG, under the game's own folder and name
    feman/backgrounds/                       your own pictures behind the menu screens
    README.txt                               what you can change, and how
Repaint a PNG, or open a ship's .obj in Blender (File -> Import -> Wavefront), change it and export it over the
same file (File -> Export -> Wavefront, default settings): the game uses your version from its next start. A
changed ship only changes how it looks; the game still plays with the original shape, so multiplayer and replays
are not affected. The game never overwrites a file you changed. To undo a change, delete that file; delete the
whole Assets folder to get a fresh copy at the next start.

USING OTHER GAME FILES, AND UNINSTALLING
To make the game look for its files again, delete ~/.homeworld/gamedata-path.txt. If the offline installer was
unpacked, also delete the folder ~/.local/share/homeworld-infection/GameData, because it is checked first.
To uninstall, delete the AppImage. The menu entry then disappears. The game leaves these behind, for you to delete
if you want to:
- ~/Documents/Homeworld Infection: your Assets folder and screenshots;
- ~/.homeworld: settings and saved games;
- ~/.local/share/homeworld-infection: the unpacked game files (about 400 MB);
- ~/.local/share/applications/homeworld-infection-appimage.desktop and the homeworld-infection-appimage.png
  icons under ~/.local/share/icons/hicolor.

RESOLUTION AND FULLSCREEN
Options -> Video: pick the resolution and the UI scale there, then Accept. A resolution as large as the screen
opens as a borderless window covering it.

KNOWN ISSUES
- Needs a Linux from 2022 or newer: the program is built on Ubuntu 22.04 and does not start on older systems
  (for example Ubuntu 20.04 or Debian 11).
- Tested on few distributions so far. Please report the distribution, the desktop (GNOME, KDE, ...), whether it
  runs on X11 or Wayland, and the graphics card along with any problem.
- The game window uses X11 (through XWayland on a Wayland desktop).
- The hyperspace gate can look the wrong size for the Kith ship, and parts of ships can pop in just after a
  hyperspace exit.
- A rare one-frame flicker in a cutscene.
- FULL DETAIL costs frame rate in big battles: untick it in Options -> Video if a battle stutters.
- No LAN or internet play.
- Quit may need a second try after a resolution change.
- Saved games from this build do not load in the GOG game.

IF SOMETHING GOES WRONG
If the game stops with an error, a message says so and names the log ~/.homeworld/hwi-engine.log. Please send
that file with your report.

LICENCES
The engine is based on Relic Entertainment's Homeworld source code, released under the Relic End User License
Agreement: for non-commercial use only. The AppImage also carries:
- SDL2, the Simple DirectMedia Layer library (zlib licence);
- a minimal build of FFmpeg 6.1 for the cutscene movies (LGPL 2.1 or later; the source URL is in
  SOURCE-FFmpeg.txt);
- innoextract, which unpacks the GOG offline installer (zlib licence; it is built with Boost, bzip2, liblzma,
  libstdc++ and zlib, whose licences are included too).
All the licence texts are in the licenses folder next to this file. Homeworld and Homeworld: Cataclysm are
trademarks of their owners; this is a fan project.
