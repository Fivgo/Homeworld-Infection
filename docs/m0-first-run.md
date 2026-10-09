# M0 result: first stable run on the Linux box (2026-10-08 13:55 MDT)

**Outcome: the GOG build reaches its main menu under GE-Proton10-34 with Cataclysm Engine Tweaks 0.18.03 injected, inside a windowed nested gamescope session.** No crash, no Wine exception in the log, process still alive after the 30 s settle. Verified by screenshot (no input was injected; the menu was not navigated).

![main menu inside the 1920x1080 gamescope window](screenshots/2026-10-08-first-run-main-menu-gamescope1080.png)

## Exact recipe that worked

| step | command | result |
|---|---|---|
| extract | `innoextract -m --gog --language english -d original/game setup_homeworld_emergence_1.01_hotfix_(12948).exe` | 66 files, `cataclysm.exe` sha256 `f03d9a3e…` (docs/binary-fingerprint.md) |
| prefix | `tools/setup_prefix.sh` (umu-run + GE-Proton10-34, prefix at `prefix/`) | created 09:41, `wineboot` ok |
| CET | `tools/install_cet.sh` (download) then `--no-download` (install) | v0.18.03: `cataclysm_loader.exe`, `CataclysmEngineTweaks.dll/.ini`, CET `rgl.dll` + `rgld3d.dll`, `update102.big`; stock DLLs in `original/game/.cet-backup/<ts>/` |
| launch | `tools/run_game.sh --size 1920x1080` | `gamescope --prefer-vk-device 10de:2b85 --backend sdl -w 1920 -h 1080 -W 1920 -H 1080 -- umu-run cataclysm_loader.exe /noswddraw` with `SDL_VIDEODRIVER=x11`, DXVK on for d3d9 |
| capture | `tools/screenshot.sh --auto` (gamescopectl) and `--x11 --display :2 --window-id <inner>` | both PNGs under docs/screenshots/ |

CET .ini defaults in effect: `UpdateRateShift=2` (64 Hz universe), `NewRendererSelection=true`, `BorderlessWindow=true`, `DisableIntroMovies=true`, `HighDetailMode=true`, `UnlimitedVideoMemory=true`.

## Observations

- Video mode: the prefix registry key `HKLM\\Software\\Wow6432Node\\Sierra On-Line\\Cataclysm` holds `screenWidth=0x780` (1920), `screenHeight=0x438` (1080), `screenDepth=32`, `glToSelect=opengl32.dll`, `deviceCRC=0x329d5947`, `CmdLine=/noswddraw`: CET's `NewRendererSelection` picked OpenGL at the gamescope size on first start, as its .ini comment says. The small menu in the screenshot is the engine's fixed 640x480 front-end art centred on the 1920x1080 surface (Homeworld 1 behaviour at any resolution above 640x480), not a wrong mode. The inner X11 window briefly reported 640x480 during start-up. UI scaling is therefore a candidate for the "additional features" list, not an M0 defect.
- CET log (`original/game/CataclysmEngineTweaks.log`): `RGL patches applied`, `Update rate patches applied`, `Patches applied`, `DSound init OK`, `Universe reset called`: the DLL injection and DirectSound path work under Wine.
- First run wrote `Cataclysm.cfg` (options, default player name), `keys.cfg`, `AutoLock.txt` next to the exe.
- gamescope logged two harmless `vkGetPhysicalDeviceFormatProperties2 returned zero modifiers` errors at start and the known SIGABRT on teardown (3.14.24).
- The title screen reads HOMEWORLD EMERGENCE, i.e. GOG's loose 2017 title `.LiF` files are being used over the archive's Cataclysm art, which answers the big-archive doc's open question in favour of "loose files override the archive" (UNVERIFIED whether by mtime rule or by GOG patching; see docs/big-archive-contents.md).

## Next for M0

1. Video mode needs no seeding (see above). If a different size is wanted, `screenWidth`/`screenHeight` in the registry key above are the knobs, or Options → Video in game.
2. The user play-tests: start a skirmish and mission 1, watch `game-crash-watch.py status` for abnormal exits, note any rendering fault; try the CET D3D9 renderer (DXVK) vs OpenGL in the video options.
3. Windows 11 side is untested here (CET README steps apply unchanged).
