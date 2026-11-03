Homeworld Infection @VERSION@ for Windows (64-bit)
==============================================

Built @DATE@ from source commit @COMMIT@.

WHAT THIS IS
Homeworld Infection is a recreation of the engine of Homeworld: Cataclysm. The game's own files (ships, maps, music,
voices) are NOT included. They come from your own copy of Homeworld: Emergence (Cataclysm's name on GOG).

WHAT YOU NEED
Homeworld: Emergence from GOG, in either form:
- installed (with GOG Galaxy or with GOG's offline installer), or
- just GOG's offline installer, setup_homeworld_emergence_*.exe (about 345 MB; on gog.com: Games -> Homeworld:
  Emergence -> "Download offline backup game installers").

INSTALL
Double-click Homeworld-Infection-@VERSION@-Windows-Setup.exe and follow the steps. Windows may warn that the program is
from an unknown publisher (it is not signed): click "More info", then "Run anyway". No administrator rights are
needed. It installs for your Windows account only, into
    %LOCALAPPDATA%\Programs\Homeworld Infection
and adds "Homeworld Infection" to the Start menu and the Desktop.

FIRST START
Start "Homeworld Infection". The game looks for your GOG copy of Homeworld: Emergence by itself. If it does not find
one, it asks you to choose either
- the GOG game folder, the one that holds cataclysm.exe and cataclysm.big (in GOG Galaxy: Homeworld: Emergence ->
  Manage installation -> Show folder), or
- the offline installer setup_homeworld_emergence_*.exe. The game unpacks it once into
      %LOCALAPPDATA%\Homeworld-Infection\GameData
  The installer file itself is not changed.
Your choice is remembered (the file gamedata-path.txt in the settings folder; delete it to choose again). A GOG
installation is only read, never changed, and the GOG game keeps working as before.

WHERE THE GAME PUTS ITS FILES
To open one of the %...% folders below, type it into the Explorer address bar.
1. Your own files, to open and change: your Documents folder (in Explorer: This PC, Documents), in Homeworld Infection
       Documents\Homeworld Infection\Assets        every texture as a PNG and every ship as a model for Blender
       Documents\Homeworld Infection\Screenshots   the pictures you take in the game
   (If OneDrive backs up your Documents, the folder is in OneDrive\Documents.)
2. Settings and saved games:
       %APPDATA%\Homeworld-Infection
   It holds the settings files Cataclysm.cfg and keys.cfg, your saved games (SavedGames) and gamedata-path.txt (the
   remembered game-files folder). The GOG game's own saves are not touched.
3. The game files unpacked from GOG's offline installer (about 400 MB, only if you chose the installer):
       %LOCALAPPDATA%\Homeworld-Infection\GameData
   These are the game's packed archives as GOG ships them: nothing in them is meant to be edited. The ships and
   textures to change are the copies in Assets. A GOG installation the game found is only read, never changed.
4. The program:
       %LOCALAPPDATA%\Programs\Homeworld Infection
Defaults: Smooth motion on, FULL DETAIL on, UI scale automatic (200% on a 4K screen, 150% at 1440p, 100% below).

CHANGING TEXTURES AND SHIPS (THE ASSETS FOLDER)
At its first start the game fills Documents\Homeworld Infection\Assets by itself, in the background while the intro
plays (about 25,000 files: 90 MB, up to 160 MB of disk space). Inside:
    Models\<race>\<ship>\<ship>_lod0.obj     each ship for Blender, with its textures (lod0 = the most detail)
    race_kushan\, race_taidan\, feman\ ...   every texture as a PNG, under the game's own folder and name
    feman\backgrounds\                       your own pictures behind the menu screens
    README.txt                               what you can change, and how
Repaint a PNG, or open a ship's .obj in Blender (File -> Import -> Wavefront), change it and export it over the
same file (File -> Export -> Wavefront, default settings): the game uses your version from its next start. A
changed ship only changes how it looks; the game still plays with the original shape, so multiplayer and replays
are not affected. The game never overwrites a file you changed. To undo a change, delete that file; delete the
whole Assets folder to get a fresh copy at the next start.

RESOLUTION AND FULLSCREEN
Options -> Video: pick the resolution and the UI scale there, then Accept. A resolution as large as the screen opens
as a borderless window covering it. You can also edit Cataclysm.cfg in the settings folder (with the game closed):
    screenWidth 1920
    screenHeight 1080
    fullScreen 0          (1 = fullscreen)
For one shortcut only, add a switch at the end of its Target (right-click the shortcut -> Properties), after the
closing quote:  /fullscreen   /window   /1600 (1600x1200)

UNINSTALL
Windows Settings -> Apps -> Installed apps -> Homeworld Infection -> Uninstall. The program files are always removed.
The uninstaller asks whether to remove your settings and saved games and the unpacked game data too; unless you tick
them, they are kept for a later install. It never removes Documents\Homeworld Infection (your Assets folder and
screenshots): delete that folder yourself if you want it gone.

KNOWN ISSUES
- Not yet played on Windows: this build was made and checked on Linux, and on Windows it was only installed, started
  without game data and uninstalled by an automated test. Please report whether the main menu, a New Game into
  Mission01, Options -> Video and Quit work.
- The movies (the Sierra and Barking Dog logos at start-up and the animatics before Mission01 and at its hyperspace
  jump) are in this build but have not been watched on Windows yet: please report if one does not play.
- The hyperspace gate can look the wrong size for the Kith ship, and parts of ships can pop in just after a
  hyperspace exit.
- A rare one-frame flicker in a cutscene.
- FULL DETAIL costs frame rate in big battles: untick it in Options -> Video if a battle stutters.
- No LAN or internet play.
- Quit may need a second try after a resolution change.
- Saved games from this build do not load in the GOG game.
To send a log of a problem: press Windows+R, type cmd and press Enter, then paste this line and press Enter:
    "%LOCALAPPDATA%\Programs\Homeworld Infection\hwi-engine.exe" > "%USERPROFILE%\Desktop\hwi-log.txt" 2>&1
Play until the problem shows, quit, and send hwi-log.txt from your Desktop.

LICENCES (in the licenses folder next to the game)
- The engine is based on Relic Entertainment's Homeworld source code, released under the Relic End User License
  Agreement (REULA.txt): for non-commercial use only.
- SDL2.dll: Simple DirectMedia Layer, zlib licence (SDL2.txt).
- avcodec, avformat, avutil and swscale DLLs: FFmpeg, LGPL version 2.1 or later (FFmpeg folder). They are unmodified
  FFmpeg; FFmpeg\SOURCE.txt names the source download and how it was configured. You may replace these DLLs with
  your own build of the same FFmpeg version.
- innoextract.exe: innoextract by Daniel Scharrer, zlib licence, with the licences of the libraries built into it
  (innoextract folder). Source: https://constexpr.org/innoextract/
Homeworld and Homeworld: Cataclysm are trademarks of their owners; this is a fan project.
