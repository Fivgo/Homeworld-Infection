Homeworld-Infection for Windows (64-bit)
========================================

Build @DATE@, engine @ENGINE@ (the build play-tested on 2026-10-21), source commit @COMMIT@.

WHAT THIS IS
This is a recreation of the engine of Homeworld: Cataclysm. The game's own files (ships, maps, music, voices) are
NOT included. They come from your own GOG installation of Homeworld: Emergence (Cataclysm's GOG name).

INSTALL
1. Extract every file in this zip into the GOG game folder, next to cataclysm.exe and cataclysm.big.
   (In GOG Galaxy: Homeworld: Emergence -> Manage installation -> Show folder.)
2. Nothing of the GOG game is changed or replaced. To uninstall, delete hwi-engine.exe, SDL2.dll and the .txt files
   from this zip.

RUN
Double-click hwi-engine.exe. Windows may warn that the program is from an unknown publisher (it is not signed):
click "More info", then "Run anyway".
The first run creates the settings folder
    %APPDATA%\Homeworld-Infection
(type that into the Explorer address bar to open it). It holds the settings file Cataclysm.cfg, keys.cfg, your saved
games and screenshots. The GOG game's own saves are not touched.
Defaults: Smooth motion on, FULL DETAIL on, UI scale automatic (200% on a 4K screen, 150% at 1440p, 100% below).

RESOLUTION AND FULLSCREEN
Options -> Video: pick the resolution and the UI scale there, then Accept. A resolution as large as the screen opens
as a borderless window covering it. You can also edit Cataclysm.cfg in the settings folder (with the game closed):
    screenWidth 1920
    screenHeight 1080
    fullScreen 0          (1 = fullscreen)
For one run only, a shortcut's target can add switches after the exe's name:
    hwi-engine.exe /fullscreen      hwi-engine.exe /window      hwi-engine.exe /1600   (1600x1200)

KNOWN ISSUES
- Not yet run on Windows: this build was only compiled and checked on Linux; there has been no Wine test either.
  Please report whether the main menu, a New Game into Mission01, Options -> Video and Quit work.
- The animatics (the cutscene movies before Mission01 and at its hyperspace jump) are not played in this Windows
  build; the in-engine cutscenes play.
- The hyperspace gate can look the wrong size for the Kith ship, and parts of ships can pop in just after a
  hyperspace exit.
- A rare one-frame flicker in a cutscene.
- FULL DETAIL costs frame rate in big battles: untick it in Options -> Video if a battle stutters.
- No LAN or internet play.
- Quit may need a second try after a resolution change.
- Saved games from this build do not load in the GOG game.
To send a log of a problem: open a Command Prompt in the game folder and run
    hwi-engine.exe > hwi-log.txt 2>&1
then send hwi-log.txt.

LICENCES
The engine is based on Relic Entertainment's Homeworld source code, released under the Relic End User License
Agreement (LICENSE-REULA.txt): for non-commercial use only. SDL2.dll is the Simple DirectMedia Layer library, zlib
licence (LICENSE-SDL2.txt). Homeworld and Homeworld: Cataclysm are trademarks of their owners; this is a fan project.
