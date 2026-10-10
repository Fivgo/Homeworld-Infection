# src/ provenance

**Licence: everything in this directory is derived from Relic Entertainment's Homeworld 1 source and is bound by the Relic End-User License Agreement (REULA), non-commercial use only. Full text: [`COPYING.REULA`](COPYING.REULA); summary at the top of [`README.md`](README.md).**

Written by the M2-R1 bootstrap (2026-10-10). Regenerate the lists with the commands in `docs/m2-r1-bootstrap.md` if files are re-imported.

## Commits

| Role | Repository | Commit | What it is |
|---|---|---|---|
| **Base** (licence origin, comparison point) | Gardens of Kadesh, https://gitlab.com/gardens-of-kadesh/gardens-of-kadesh | `ad1bb82ca3c62dcc8be9d35f2e836d0bb7cf7caa` (root commit) | "Original Relic code, as published in 2003": Relic's Homeworld 1 source exactly as released, CRLF line endings, lowercase file names, Win32/MSVC only |
| **Donor** (what is actually built) | same | `3af31fbca2acfb4e791386a041087b340e77892f` (GoK `master` HEAD on 2026-10-10; identical to `reference/gardens-of-kadesh`) | the HomeworldSDL / Gardens of Kadesh port of that source to SDL2 + OpenGL, 64-bit and Linux |

How the files were obtained: `ad1bb82` from the GitLab archive endpoint (`.../-/archive/ad1bb82/gardens-of-kadesh-ad1bb82.tar.gz`, pax comment `ad1bb82ca3c6...`), extracted to `original/scratch-m2/gok-export/` (the reference clone is shallow and does not contain `ad1bb82`); `3af31fb` from the working tree of the read-only clone `reference/gardens-of-kadesh` (`.git/HEAD` -> `refs/heads/master` = `3af31fb`; a fresh full clone in `original/scratch-m2/gok-hist-bootstrap` shows `origin/master` at the same commit).

## Decision: whole GoK `src/` instead of ad1bb82 + hand-picked hunks

The brief asked for Relic's `ad1bb82` tree plus only the GoK portability fixes the build needs, falling back to GoK's whole `src/` if that meant hundreds of hand-picked hunks. Measured before importing: Relic's `src/Game` and `src/Ships` against GoK's (361 files matched case-insensitively) differ in 360 files, in **14,257 diff hunks** (+39,685 / -37,078 lines after normalising line endings; script `hunks.py`, see docs/m2-r1-bootstrap.md). [audit: these are zero-context difflib change blocks. GNU `diff -U3` on the same normalised files gives 7,039 hunks, and `-U0` gives 14,273.] Relic's headers do not even compile outside MSVC (`ad1bb82:src/Game/TYPES.H:11-13` is `#if defined(_WIN32) && defined(_M_IX86)` ... `#error`), `udword` is `unsigned long` (8 bytes on LP64), the task scheduler and ETG calls are x86 inline asm, and the GoK SDL layer depends on GoK's reworked headers (`memsize`, `bool32`, `*_disk` loaders). So the fallback was taken: **GoK `src/` at `3af31fb` is imported whole** (minus autotools files), and `ad1bb82` is kept as the comparison base (diffstat below) and, for `Win32/`, as an in-tree reference copy. This is a decision the user owns: it is listed under Decisions in `docs/m2-r1-bootstrap.md` for the round's integrate stage to carry into `cycle/STATE.md` "Decisions the user owns".

Consequence: "unmodified HW1" here means *no Cataclysm changes*. The tree carries every change HomeworldSDL/GoK made since 2003, not only portability fixes. Behavioural ones that matter for a 1:1 recreation (each verified in this tree):

| GoK change | Where | Effect vs Relic 1.05 |
|---|---|---|
| 64 Hz universe update by default | `Game/Universe.c:75` `UNIVERSE_UPDATE_RATE_SHIFT = 2`; `/vanilla` restores 16 Hz (`SDL/main.c:848,2195`) | simulation rate x4 unless `/vanilla` |
| Portable save format | `Game/SaveGame.h:16-22` (`SAVE_VERSION_NUMBER` = `HWSDL_2` 0x10ad0029) | Relic saves (0x10ad0027) not loadable |
| HWSDL options in `Homeworld.cfg` | `SDL/utility.c:778-783` (`CollectResourcesAtEndOfMission`, `ShipsAlwaysUseOwnerColors`, `TimeCompressionFactor`) | the two bools default to FALSE (`Game/SinglePlayer.c:151`, `SDL/utility.c:941`); `turboTimeCompressionFactor = 8` (`Game/Universe.c:102`, commented "same as Homeworld Cataclysm") |
| Float-to-int, x87 vs SSE, task tick from ms timer | see `docs/hw1-port-lessons.md` A3, A7 | numerics differ from the 1999/2003 binary |
| Remaster `homeworld.big` support, widescreen, MSAA, vsync, stb_image, FFmpeg movies, SDL_net LAN (compiled out) | `docs/hw1-port-lessons.md` A9 | presentation / platform |

## Local changes made by Homeworld-Infection (all build files; no C source edited)

| File | Change |
|---|---|
| `../meson.build`, `../meson_options.txt` | new, adapted from GoK `meson.build`/`meson_options.txt`: project and executable `hwi-engine`; `-Werror` via meson's `werror` option (default true); `-Wno-*` list through `get_supported_arguments()`, clang-only extras for clang; option `legacy_warnings`; emscripten/darwin branches, `-Ddemo` and GoK's optional tools dropped |
| `meson.build` (this dir) | same content as GoK `src/meson.build` [audit: plus one added first line, a comment naming the source; the build content is unchanged] |
| `tools/kas2c/meson.build` | GoK `tools/kas2c/meson.build` + `include_directories('..')`, because `parser.y:18` includes `"../../src/Game/Types.h"`, a path written for GoK's `tools/kas2c` location |
| `README.md`, `PROVENANCE.md` | new |

Not imported from GoK `3af31fb`: every `Makefile.am`, `tools/kas2c/kas2c-macOS-compile.sh`, `src/homeworld.c` (autotools placeholder), `src/obj/` (Relic's prebuilt codec `.obj`/`.o`, replaced by the reverse-engineered C in `SDL/`), and everything outside `src/` except `tools/kas2c` and `COPYING.REULA`. Not imported from `ad1bb82`: binary files in `src/Win32` (`Homeworld.ico`, `HomeworldLogo.ico`, `res.geo`, `vc50.pdb`, `vssver.scc`).

## Diffstat of src/ against ad1bb82

Line counts are after normalising line endings and trailing whitespace (Relic's files are CRLF; a raw diff would count every line). Path mapping: `ad1bb82:src/Game` -> `Game/`, `src/Ships` -> `Ships/`, `src/Win32` -> `SDL/` (GoK's rename) and -> `Win32/` (reference copy), `src/SinglePlayer` -> `Missions/SinglePlayer/`, `src/Tutorials` -> `Missions/Tutorials/`, `tools/win32/KAS/KAS2C.{c,h,l,y}` -> `tools/kas2c/{kas2c.c,kas2c.h,lexer.l,parser.y}`. Files match case-insensitively (GoK renamed `universe.c` to `Universe.c` etc.); "moved" = no same-name file in the mapped original directory but one elsewhere in `ad1bb82:src/`. Only code files are counted (`.c .h .cpp .kas .kash .l .y .def .rc`, plus new `meson.build`/text files). Deletions count the lines of files that exist only in `ad1bb82`.

| Directory | Files | Identical | Modified | Moved in | Added | Deleted (in ad1bb82 only) | Lines + | Lines - |
|---|---|---|---|---|---|---|---|---|
| `Game` | 313 | 1 | 280 | 8 | 12 | 12 | 41,412 | 42,273 |
| `Ships` | 83 | 0 | 80 | 2 | 1 | 0 | 2,855 | 4,321 |
| `SDL` | 70 | 1 | 54 | 0 | 15 | 0 | 14,109 | 7,924 |
| `Win32` | 93 | 93 | 0 | 0 | 0 | 0 | 0 | 0 |
| `Missions/SinglePlayer` | 19 | 1 | 18 | 0 | 0 | 0 | 250 | 803 |
| `Missions/Tutorials` | 1 | 0 | 1 | 0 | 0 | 0 | 11 | 46 |
| `tools/kas2c` | 4 | 0 | 4 | 0 | 0 | 0 | 193 | 38 |
| `tools` | 2 | 0 | 0 | 0 | 2 | 0 | 76 | 0 |
| `ThirdParty` | 14 | 0 | 0 | 7 | 7 | 0 | 9,934 | 277 |
| `Missions` | 3 | 0 | 0 | 0 | 3 | 0 | 211 | 0 |
| **total** | 602 | 96 | 437 | 17 | 40 | 12 | 69,051 | 55,682 |

(`Win32/` is byte-identical to `ad1bb82`. Relic `src/Win32` files that GoK deleted are not counted as deletions under `SDL/`, since `Win32/` keeps them.)

Most-changed files (normalised lines changed, + / -):

| File | + | - |
|---|---|---|
| `Game/AIMoves.c` | 5,552 | 22 |
| `Game/UIControls.c` | 1,630 | 1,684 |
| `SDL/main.c` | 824 | 1,977 |
| `SDL/render.c` | 977 | 1,288 |
| `Game/UnivUpdate.c` | 1,007 | 1,152 |
| `SDL/utility.c` | 882 | 1,003 |
| `Game/Dock.c` | 840 | 1,019 |
| `Game/SinglePlayer.c` | 991 | 761 |
| `Game/SpeechEvent.h` | 856 | 844 |
| `Game/File.c` | 1,207 | 465 |
| `Game/ETG.c` | 930 | 592 |
| `Game/Universe.c` | 761 | 746 |
| `Game/CommandLayer.c` | 679 | 785 |
| `SDL/texreg.c` | 624 | 667 |
| `Game/BigFile.c` | 995 | 265 |
| `Game/NIS.c` | 798 | 459 |
| `Game/Physics.c` | 405 | 831 |
| `Game/Mesh.c` | 636 | 599 |
| `Game/Formation.c` | 339 | 872 |
| `Game/Key.c` | 70 | 1,077 |
| `SDL/mainrgn.c` | 562 | 533 |
| `Game/HorseRace.c` | 467 | 552 |
| `Game/SaveGame.c` | 546 | 473 |
| `Ships/SalCapCorvette.c` | 432 | 525 |
| `Game/Collision.c` | 405 | 519 |

Moved into `Game/`, `Ships/`, `SDL/` or `ThirdParty/` from another original directory (not counted again as deletions):

- `Game/Color.c` <- `ad1bb82:src/Win32/color.c` (+9 / -10)
- `Game/Color.h` <- `ad1bb82:src/Win32/color.h` (+36 / -22)
- `Game/Light.c` <- `ad1bb82:src/Win32/light.c` (+62 / -43)
- `Game/Light.h` <- `ad1bb82:src/Win32/light.h` (+6 / -5)
- `Game/Subtitle.c` <- `ad1bb82:src/Win32/Subtitle.c` (+63 / -73)
- `Game/Subtitle.h` <- `ad1bb82:src/Win32/Subtitle.h` (+10 / -11)
- `Game/Trails.c` <- `ad1bb82:src/Win32/trails.c` (+164 / -217)
- `Game/Trails.h` <- `ad1bb82:src/Win32/trails.h` (+22 / -19)
- `Ships/ShipDefs.h` <- `ad1bb82:src/Game/ShipDefs.h` (+110 / -77)
- `Ships/Ships.h` <- `ad1bb82:src/Game/ships.h` (+37 / -39)
- `ThirdParty/CRC/CRC32.c` <- `ad1bb82:src/Game/crc32.c` (+86 / -90)
- `ThirdParty/CRC/CRC32.h` <- `ad1bb82:src/Game/crc32.h` (+16 / -21)
- `ThirdParty/LZSS/BitIO.c` <- `ad1bb82:src/Game/BitIO.c` (+38 / -108)
- `ThirdParty/LZSS/BitIO.h` <- `ad1bb82:src/Game/BitIO.h` (+38 / -40)
- `ThirdParty/LZSS/LZSS.c` <- `ad1bb82:src/Game/LZSS.C` (+11 / -13)
- `ThirdParty/LZSS/LZSS.h` <- `ad1bb82:src/Game/LZSS.h` (+8 / -5)
- `ThirdParty/Titan/ServerStatus.h` <- `ad1bb82:src/Titan/msg/ServerStatus.h` (+1 / -0)

New in GoK (no same-name file anywhere in `ad1bb82:src/`):

`Game/File_Mac.c`, `Game/Matrix-mult.c`, `Game/StringSupport.c`, `Game/StringSupport.h`, `Game/StubTransform.c`, `Game/Types.c`, `Game/functions.h`, `Game/meson.build`, `Game/win32_vc.h`, `Game/wrap.h`, `Game/wrap_types.h`, `Game/wrapped_unlisted_functions.h`, `Missions/SinglePlayer/meson.build`, `Missions/Tutorials/meson.build`, `Missions/meson.build`, `SDL/NetworkInterface.c`, `SDL/NetworkInterface.h`, `SDL/TitanInterfaceC.c`, `SDL/TitanInterfaceC.stub.c`, `SDL/avi.c`, `SDL/dct.c`, `SDL/fqcodec.c`, `SDL/fqeffect.c`, `SDL/fquant.c`, `SDL/meson.build`, `SDL/mixfft.h`, `SDL/rinit.c`, `SDL/standard_library.h`, `SDL/utility_mac.c`, `SDL/win32glue.c`, `Ships/meson.build`, `ThirdParty/CRC/meson.build`, `ThirdParty/LZSS/meson.build`, `ThirdParty/Titan/meson.build`, `ThirdParty/meson.build`, `ThirdParty/stb/meson.build`, `ThirdParty/stb/stb_image.h`, `ThirdParty/stb/stb_image_write.h`, `tools/kas2c/README.txt`, `tools/kas2c/meson.build`

In `ad1bb82` but gone from GoK `Game/`/`Ships/`/`Missions/` (merged, renamed or moved to `ThirdParty/`):

`src/Game/AIMoves1.c`, `src/Game/AIMoves2.c`, `src/Game/AIOrders2.c`, `src/Game/LetterDefs.h`, `src/Game/NumberDefs.h`, `src/Game/Strings.c`, `src/Game/Strings.h`, `src/Game/fastmath.c`, `src/Game/knitransform.cpp`, `src/Game/liloptions.h`, `src/Game/tutorial.c`, `src/Game/tutorial.h`

## Files taken from each commit

### From GoK `3af31fb` (built): 500 files

- `./` (1): COPYING.REULA
- `Game/` (303): AIAttackMan.c, AIAttackMan.h, AIDefenseMan.c, AIDefenseMan.h, AIEvents.c, AIEvents.h, AIFeatures.h, AIFleetMan.c, AIFleetMan.h, AIHandler.c, AIHandler.h, AIMoves.c, AIMoves.h, AIOrders.c, AIOrders.h, AIPlayer.c, AIPlayer.h, AIResourceMan.c, AIResourceMan.h, AIShip.c, AIShip.h, AITeam.c, AITeam.h, AITrack.c, AITrack.h, AIUtilities.c, AIUtilities.h, AIVar.c, AIVar.h, Alliance.c, Alliance.h, Animatic.c, Animatic.h, Attack.c, Attack.h, Attributes.h, AutoDownloadMap.c, AutoDownloadMap.h, AutoLOD.c, AutoLOD.h, B-Spline.c, B-Spline.h, Battle.c, Battle.h, BigFile.c, BigFile.h, Blobs.c, Blobs.h, BMP.c, BMP.h, Bounties.c, Bounties.h, BTG.c, BTG.h, Camera.c, Camera.h, CameraCommand.c, CameraCommand.h, Captaincy.c, Captaincy.h, ChannelFSM.c, ChannelFSM.h, Chatting.c, Chatting.h, Clamp.c, Clamp.h, ClassDefs.h, Clipper.c, Clipper.h, Clouds.c, Clouds.h, Collision.c, Collision.h, Color.c, Color.h, ColPick.c, ColPick.h, CommandDefs.h, CommandLayer.c, CommandLayer.h, CommandNetwork.c, CommandNetwork.h, CommandWrap.c, CommandWrap.h, ConsMgr.c, ConsMgr.h, Crates.c, Crates.h, Damage.c, Damage.h, Debug.c, Debug.h, Demo.c, Demo.h, desktop.ini, Dock.c, Dock.h, ETG.c, ETG.h, Eval.c, Eval.h, FastMath.h, FEColour.h, FEFlow.c, FEFlow.h, FEReg.c, FEReg.h, File.c, File.h, File_Mac.c, FlightMan.c, FlightMan.h, FlightManDefs.h, FontReg.c, FontReg.h, Formation.c, Formation.h, FormationDefs.h, functions.h, GameChat.c, GameChat.h, GamePick.c, GamePick.h, GameStats.h, Globals.c, Globals.h, Gun.c, Gun.h, Hash.c, Hash.h, HorseRace.c, HorseRace.h, HS.c, HS.h, InfoOverlay.c, InfoOverlay.h, KAS.c, KAS.h, KASFunc.c, KASFunc.h, Key.c, Key.h, KeyBindings.c, KeyBindings.h, LagPrint.c, LagPrint.h, LaunchMgr.c, LaunchMgr.h, LevelLoad.c, LevelLoad.h, Light.c, Light.h, LinkedList.c, LinkedList.h, LOD.c, LOD.h, MadLinkIn.c, MadLinkIn.h, MadLinkInDefs.h, Matrix-mult.c, Matrix.c, Matrix.h, Matrix.s, MaxMultiplayer.h, Memory.c, Memory.h, Mesh.c, Mesh.h, MeshAnim.c, MeshAnim.h, meson.build, MEX.c, MEX.h, MultiplayerGame.c, MultiplayerGame.h, MultiplayerLANGame.c, MultiplayerLANGame.h, NavLights.c, NavLights.h, Nebulae.c, Nebulae.h, NetCheck.c, NetCheck.h, NIS.c, NIS.h, Objectives.c, Objectives.h, ObjTypes.c, ObjTypes.h, Options.c, Options.h, Particle.c, Particle.h, Physics.c, Physics.h, PiePlate.c, PiePlate.h, Ping.c, Ping.h, PlugScreen.c, PlugScreen.h, ProfileTimers.c, ProfileTimers.h, RaceDefs.h, Randy.c, Randy.h, Region.c, Region.h, ResCollect.c, ResCollect.h, ResearchAPI.c, ResearchAPI.h, ResearchGUI.c, ResearchGUI.h, SaveGame.c, SaveGame.h, ScenPick.c, ScenPick.h, Scroller.c, Scroller.h, Select.c, Select.h, Sensors.c, Sensors.h, Shader.c, Shader.h, ShipSelect.c, ShipSelect.h, ShipView.c, ShipView.h, SinglePlayer.c, SinglePlayer.h, SoundEvent.c, SoundEvent.h, SoundEventDefs.h, SoundEventPlay.c, SoundEventPrivate.h, SoundEventStop.c, SoundMusic.h, SoundStructs.h, SpaceObj.h, SpeechEvent.c, SpeechEvent.h, Star3d.c, Star3d.h, Stats.c, Stats.h, StatScript.c, StatScript.h, StringsOnly.h, StringSupport.c, StringSupport.h, StubTransform.c, Subtitle.c, Subtitle.h, Switches.h, Tactical.c, Tactical.h, Tactics.c, Tactics.h, Task.c, Task.h, TaskBar.c, TaskBar.h, Teams.c, Teams.h, Timer.c, Timer.h, TitanNet.c, TitanNet.h, Tracking.c, Tracking.h, TradeMgr.c, TradeMgr.h, Trails.c, Trails.h, Transformer.c, Transformer.h, Tutor.c, Tutor.h, Tweak.c, Tweak.h, Twiddle.c, Twiddle.h, Types.c, Types.h, UIControls.c, UIControls.h, Undo.c, Undo.h, Universe.c, Universe.h, UnivUpdate.c, UnivUpdate.h, Vector.c, Vector.h, VolTweakDefs.h, Volume.c, Volume.h, win32_vc.h, wrap.h, wrap_types.h, wrapped_unlisted_functions.h
- `Missions/` (1): meson.build
- `Missions/SinglePlayer/` (20): meson.build, Mission01.kas, Mission02.kas, Mission03.kas, Mission04.kas, Mission05.kas, Mission05_OEM.kas, Mission06.kas, Mission07.kas, Mission08.kas, Mission09.kas, Mission10.kas, Mission11.kas, Mission12.kas, Mission13.kas, Mission14.kas, Mission15.kas, Mission16.kas, Swarmer.kash, Utilities.kash
- `Missions/Tutorials/` (2): meson.build, Tutorial1.kas
- `SDL/` (70): avi.c, avi.h, ClientCDKey.cpp, ClientCDKey.h, dct.c, dct.h, debugwnd.c, debugwnd.h, font.c, font.h, fqcodec.c, fqcodec.h, fqeffect.c, fqeffect.h, fquant.c, fquant.h, glinc.h, main.c, main.h, mainrgn.c, mainrgn.h, mainswitches.h, meson.build, mixfft.c, mixfft.h, mouse.c, mouse.h, NetworkInterface.c, NetworkInterface.h, prim2d.c, prim2d.h, prim3d.c, prim3d.h, Queue.c, Queue.h, regkey.h, render.c, render.h, resource.h, rglu.c, rglu.h, rinit.c, rinit.h, screenshot.c, screenshot.h, smixer.c, soundcmn.h, soundlow.c, soundlow.h, sstream.c, standard_library.h, texreg.c, texreg.h, TimeoutTimer.c, TimeoutTimer.h, Titan.c, Titan.h, TitanInterface.cpp, TitanInterface.h, TitanInterfaceC.c, TitanInterfaceC.h, TitanInterfaceC.stub.c, TitanPacketMsg.cpp, TitanPacketMsg.h, utility.c, utility.h, utility_mac.c, wave.c, wave.h, win32glue.c
- `Ships/` (83): AdvanceSupportFrigate.c, AdvanceSupportFrigate.h, Carrier.c, Carrier.h, CloakGenerator.c, CloakGenerator.h, DDDFrigate.c, DDDFrigate.h, DefaultShip.c, DefaultShip.h, DefenseFighter.c, DefenseFighter.h, DFGFrigate.c, DFGFrigate.h, Drone.c, Drone.h, FloatingCity.c, FloatingCity.h, GenericDefender.c, GenericDefender.h, GenericInterceptor.c, GenericInterceptor.h, GravWellGenerator.c, GravWellGenerator.h, HeavyCorvette.c, HeavyCorvette.h, HeavyCruiser.c, HeavyCruiser.h, IonCannonFrigate.c, IonCannonFrigate.h, LightCorvette.c, LightCorvette.h, meson.build, MinelayerCorvette.c, MinelayerCorvette.h, MissileDestroyer.c, MissileDestroyer.h, Mothership.c, Mothership.h, MultiGunCorvette.c, MultiGunCorvette.h, P1IonArrayFrigate.c, P1IonArrayFrigate.h, P1MissileCorvette.c, P1MissileCorvette.h, P1Mothership.c, P1Mothership.h, P1StandardCorvette.c, P1StandardCorvette.h, P2AdvanceSwarmer.c, P2AdvanceSwarmer.h, P2FuelPod.c, P2FuelPod.h, P2Mothership.c, P2Mothership.h, P2MultiBeamFrigate.c, P2MultiBeamFrigate.h, P2Swarmer.c, P2Swarmer.h, P3StandardShip.c, P3StandardShip.h, Probe.c, Probe.h, ProximitySensor.c, ProximitySensor.h, RepairCorvette.c, RepairCorvette.h, ResearchShip.c, ResearchShip.h, ResourceCollector.c, ResourceCollector.h, ResourceController.c, ResourceController.h, SalCapCorvette.c, SalCapCorvette.h, SensorArray.c, SensorArray.h, ShipDefs.h, Ships.h, StandardDestroyer.c, StandardDestroyer.h, StandardFrigate.c, StandardFrigate.h
- `ThirdParty/` (1): meson.build
- `ThirdParty/CRC/` (3): CRC32.c, CRC32.h, meson.build
- `ThirdParty/LZSS/` (5): BitIO.c, BitIO.h, LZSS.c, LZSS.h, meson.build
- `ThirdParty/Titan/` (2): meson.build, ServerStatus.h
- `ThirdParty/stb/` (3): meson.build, stb_image.h, stb_image_write.h
- `tools/kas2c/` (6): kas2c.c, kas2c.h, lexer.l, meson.build, parser.y, README.txt

### From Relic `ad1bb82` (reference only, not built): 103 files

- `Win32/` (103): avi.cpp, avi.h, bink.c, bink.h, ClientCDKey.cpp, ClientCDKey.h, color.c, color.h, dct.h, ddraw.cpp, Debug.depend, debugwnd.c, debugwnd.h, devstats.h, dxdraw.cpp, dxdraw.h, font.c, font.h, fqcodec.h, fqeffect.h, fquant.h, glcaps.c, glcaps.h, glcompat.c, glcompat.h, gldefines.h, gldll.c, gldll.h, glext.h, glinc.h, Homeworld.ex, Homeworld.mif, Homeworld.rc, hw.mak, Interim.depend, leakyfaucet.h, light.c, light.h, LinkEnd.c, LinkLimits.h, LinkStart.c, main.c, main.h, mainrgn.c, mainrgn.h, mainswitches.h, MAKEFILE, MAKEFILE.NoDebugInfo, Md52.cpp, Md52.h, mixfft.c, mouse.c, mouse.h, mssccprj.scc, prim2d.c, prim2d.h, prim3d.c, prim3d.h, Queue.c, Queue.h, rad.h, radbink.h, regkey.h, Release.depend, render.c, render.h, resource.h, rglu.c, rglu.h, rinit.cpp, rinit.h, screenshot.c, screenshot.h, smixer.c, soundcmn.h, soundlow.c, soundlow.h, sstglide.c, sstglide.h, sstream.c, Subtitle.c, Subtitle.h, sw.mak, texreg.c, texreg.h, TimeoutTimer.c, TimeoutTimer.h, Titan.c, Titan.h, TitanInterface.cpp, TitanInterface.h, TitanInterfaceC.h, TitanPacketMsg.cpp, TitanPacketMsg.h, trails.c, trails.h, utility.c, utility.h, wassert.c, wassert.h, wave.c, wave.h, zmouse.h

Source paths: GoK files keep their GoK path under `src/` (GoK `src/X` -> `X`), except GoK `tools/kas2c/*` -> `tools/kas2c/*` and GoK `COPYING.REULA` -> `COPYING.REULA`. Relic files: `ad1bb82:src/Win32/*` -> `Win32/*`, byte for byte (CRLF kept). The import manifest (per-file source path and sha256) and the diffstat data are kept outside git in `original/scratch-m2/bootstrap-logs/` (`manifest.json`, `diffstat.json`). Every imported file is byte-identical to its source except `tools/kas2c/meson.build` (see Local changes).
