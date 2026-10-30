Homeworld Infection @VERSION@ for macOS
====================================

Build @DATE@, source commit @COMMIT@. For macOS 11 (Big Sur) or newer, on Apple Silicon and Intel Macs.

WHAT THIS IS
A recreation of the engine of Homeworld: Cataclysm. The game's own files (ships, maps, music, voices) are NOT
included: they come from your copy of Homeworld: Emergence from GOG (Cataclysm's GOG name).

INSTALL
1. Drag "Homeworld Infection" onto the Applications folder in this window.
2. Eject the disk image.

FIRST START (once)
The app is not signed by an Apple developer account, so macOS stops it the first time:
1. Open Applications and double-click Homeworld Infection. macOS says it cannot verify the app: click "Done"
   (or "Cancel"; never "Move to Trash").
2. Open System Settings (Apple menu), Privacy & Security, scroll down to Security: next to "Homeworld Infection was
   blocked" click "Open Anyway", then confirm with your password or Touch ID and click "Open Anyway" again.
   On macOS 11 to 14 you can instead right-click (Control-click) the app in Applications, choose Open, then Open.
From then on it opens with a double-click.

THE GAME FILES (once)
On its first start the game looks for Homeworld: Emergence and, if it finds none, asks for one of these:
- The GOG offline installer, setup_homeworld_emergence_*.exe (about 300 MB; there may also be a .bin file beside
  it: keep the two together). Get it from the GOG website: sign in, Library (Games), Homeworld: Emergence, then
  "Download offline backup game installers". The game unpacks it by itself; you do not need Windows, Wine or
  GOG Galaxy. The unpacked files go to
      ~/Library/Application Support/Homeworld-Infection/GameData
  and the installer can be deleted afterwards.
- Or a Homeworld: Emergence game folder copied from a Windows PC (the folder holding cataclysm.big). It is only
  read, never changed.
The folder you chose is remembered.

SETTINGS AND SAVED GAMES
Everything the game writes is in
    ~/Library/Application Support/Homeworld-Infection
(in Finder: Go menu, hold the Option key, Library, Application Support). It holds the settings file
Cataclysm.cfg, keys.cfg, your saved games and screenshots. Delete that folder to start over.
Defaults: Smooth motion on, FULL DETAIL on, the UI scale automatic (by the window's height in pixels: 200% from
2160, 150% from 1440). Options, Video: resolution, fullscreen and UI scale.

UNINSTALL
Drag Homeworld Infection from Applications to the Trash, and the folder above if you want your settings and the
unpacked game files gone too.

KNOWN ISSUES
- Not yet tested on a real Mac: this build was made and checked on GitHub's macOS build machines, which have no
  game files. Please report whether the first start, the game-file setup, the main menu, a New Game into
  Mission01, Options, Video and Quit work, and on which Mac and macOS version.
- macOS runs this game through Apple's OpenGL, which Apple no longer develops; it still works on Apple Silicon.
- If the game cannot unpack the installer (a message about innoextract), unpack it on another computer, or copy
  the game folder from a Windows install, and choose that folder instead.
- The hyperspace gate can look the wrong size for the Kith ship, and parts of ships can pop in just after a
  hyperspace exit. A rare one-frame flicker in a cutscene.
- FULL DETAIL costs frame rate in big battles: untick it in Options, Video if a battle stutters.
- No LAN or internet play. Saved games from this build do not load in the GOG game.
To send a log of a problem: open Terminal, run
    "/Applications/Homeworld Infection.app/Contents/MacOS/hwi-engine" > ~/Desktop/hwi-log.txt 2>&1
then send hwi-log.txt from the Desktop.

LICENCES
The engine is based on Relic Entertainment's Homeworld source code, released under the Relic End User License
Agreement (Licenses/LICENSE-REULA.txt): for non-commercial use only. The app also carries SDL2 (zlib licence),
FFmpeg (LGPL 2.1; its source address is in Licenses/FFmpeg/SOURCE.txt) and innoextract (zlib licence) with Boost
and liblzma: see Licenses/README.txt (the same folder is inside the app, Contents/Resources/Licenses). Homeworld
and Homeworld: Cataclysm are trademarks of their owners; this is a fan project.
