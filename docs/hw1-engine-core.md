# Homeworld 1 engine: core / platform layer

Map of the non-gameplay layer of the Homeworld 1 (HW1) engine that Cataclysm forked: start-up, main loop and 16 Hz timing, task scheduler, memory manager, file system and `.big` archives, `.script` parser, string tables, save games, demos, logging, key/option tables and the module graph. It ends with the evidence on what Cataclysm changed and a list of literal strings and constants for matching the Cataclysm binary.

Written 2026-10-08 for the engineer doing the decompilation and the port. Every claim cites a file. "UNVERIFIED" marks inference.

---

## 0. Sources, anchors, build facts

### 0.1 Anchor prefixes used below

| prefix | what | where |
|---|---|---|
| `GoK:` | Gardens of Kadesh, the active HW1 SDL port, commit 3af31fb (2026-09-25) | `reference/gardens-of-kadesh/src/...` |
| `HWSDL:` | HomeworldSDL, commit 1bed493 (2024-10-13) | `reference/HomeworldSDL/src/...` |
| `ORIG:` | Relic's 2003 source release, unmodified. This is the GoK **root commit** `ad1bb82` ("Original Relic code, as published in 2003"). Paths are `src/Win32/...` and `src/Game/...` | not in the depth-1 reference clone; see 0.2 |
| `SNAP:` | parts of the 2003 release that GoK has not integrated (Win32 makefiles, the Relic-built `Homeworld.exe`, tools) | `reference/gardens-of-kadesh/zarchive/original-release-snapshot/...` |
| `CET:` | Cataclysm Engine Tweaks, a hook DLL for the GOG `cataclysm.exe` | `reference/CataclysmEngineTweaks/...` |
| `GAME:` | GOG build 12948 extracted by another agent. Never commit it | `original/game/...` |

GoK and HWSDL differ very little in the core files: `diff -rq` shows 0 changed lines in Task.c/.h, BigFile.c/.h, Tweak.c, Key.c, Debug.c, debugwnd.c, StringSupport.c, Timer.c and KeyBindings.c, and 6 to 41 changed lines in main.c (41), Memory.c (9), File.c (29), Demo.c (6) and StatScript.c (6) (`diff | grep -c '^[<>]'`, re-run during verification). The 236 changed lines in SaveGame.c are almost all GoK's `SpaceObj` refactor (`ship->objtype` → `ship->header.objtype`, `ShipOrDerelict`; GoK HEAD 3af31fb), not save-format changes: `SaveGame.h` (version table) is byte-identical in both. **ORIG is the better template for a 1:1 Cataclysm rebuild**: both ports rewrote the task system, the timer and the `.big` bookkeeping (sections 2, 3 and 5).

### 0.2 Getting ORIG locally (the reference clone is depth 1)

```sh
git clone --filter=blob:none --no-checkout https://gitlab.com/gardens-of-kadesh/gardens-of-kadesh.git /tmp/gok
mkdir -p /tmp/hw1orig && git -C /tmp/gok --work-tree=/tmp/hw1orig checkout ad1bb82 -- src/Win32 src/Game
```
This round used a scratch copy at `/tmp/claude-1000/hw1orig/orig/src` made with exactly these commands. The ORIG files are ISO-8859-1 text, so use `grep -a`.

### 0.3 Core file names: original vs port

| role | ORIG | GoK |
|---|---|---|
| entry, window proc, command line | `src/Win32/main.c` (WinMain) | `SDL/main.c` (`main`) |
| system start-up/shutdown, timer, options file, registry | `src/Win32/utility.c` | `SDL/utility.c` |
| task scheduler | `src/Game/task.c/.h` (x86 asm) | `Game/Task.c/.h` (C coroutines) |
| memory manager | `src/Game/memory.c/.h` | `Game/Memory.c/.h` |
| file layer | `src/Game/file.c/.h` | `Game/File.c/.h` |
| `.big` archive | `src/Game/bigfile.c/.h`, `BitIO.c`, `LZSS.C`, `crc32.c` | `Game/BigFile.c/.h`, `ThirdParty/LZSS/*`, `ThirdParty/CRC/*` |
| script parser | `src/Game/statscript.c/.h`, `tweak.c/.h` | `Game/StatScript.c/.h`, `Game/Tweak.c/.h` |
| strings | `src/Game/Strings.c/.h`, `StringsOnly.h` | `Game/StringSupport.c/.h`, `Game/StringsOnly.h` |
| save game | `src/Game/SaveGame.c/.h` | `Game/SaveGame.c/.h` |
| demo | `src/Game/demo.c/.h` | `Game/Demo.c/.h` |
| debug / logging | `src/Game/Debug.c/.h`, `src/Win32/debugwnd.c/.h` | `Game/Debug.c/.h`, `SDL/debugwnd.c/.h` (window disabled) |
| keys | `src/Game/key.c`, `KEY.H`, `KeyBindings.c/.h` | `Game/Key.c/.h`, `Game/KeyBindings.c/.h` |
| options UI and key names | `src/Game/options.c/.h` | `Game/Options.c/.h` |
| universe task | `src/Game/universe.c/.h`, `univupdate.c` | `Game/Universe.c/.h`, `Game/UnivUpdate.c` |

### 0.4 Build configurations and toolchain

* ORIG compiles with exactly one of `HW_Debug`, `HW_Interim` or `HW_Release` (`SNAP: src/Win32/MAKEFILE` passes `-D$(%HW_Level)`; ORIG `Game/Debug.h:11-13`). Most switches test `#ifndef HW_Release`. GoK uses `HW_BUILD_FOR_DEBUGGING` or `HW_BUILD_FOR_DISTRIBUTION` instead (`reference/gardens-of-kadesh/meson.build:156-158`).
* What a **release** build strips (this decides which strings can exist in a retail binary):

| switch (release value) | effect | ORIG | GoK |
|---|---|---|---|
| `DBG_FILE_LINE 0`, `DBG_ASSERT 0` | `DBG_Loc` becomes `NULL, 0`. Asserts compile out, but the `dbgFatalf` **format strings stay** | `Game/Debug.h:24-34,73-75` | `Game/Debug.h:15-30,37-43` |
| `MEM_USE_NAMES 0` | `memAlloc(l,n,f)` becomes `memAllocFunction(l,f)`, so allocation **name strings are dropped** | `Game/memory.h:20-58,289-297` | `Game/Memory.h:40-60,301-315` |
| `TASK_ERROR_CHECKING 0` | no "stillborn task" or "task pointers in use" strings | `Game/task.h:30-37` | `Game/Task.h:28-35` |
| `DEM_CHECKSUM 0` etc. | smaller demo packets (section 9) | | `Game/Demo.h` switches |
| `UNIVERSE_TURBORECORD_ONLY 1` | turbo works only during packet playback | `Game/universe.h:37-50` | n/a (GoK made turbo a user feature) |

* Toolchain (relevant to binary matching and struct layout): `SNAP: src/Win32/MAKEFILE` builds with `cl`/`icl -c -MT -Zp4 -G6`. Debug uses `cl -Od`. Release uses the **Intel compiler** `icl /Ogitb1 /GsF`. It links with `link /MAP`, and Watcom `wmake` syntax drives the build. `-Zp4` means structs are packed to 4 bytes: reproduce it (`#pragma pack(4)` or equivalent) for anything read raw from disk or from saves.
* HW1 reference binary: `SNAP: exe/Homeworld.exe` (11.4 MB, PE timestamp 2003-08-22, linker 5.10, image base 0x400000, sections `.text .rdata .data .data1 .rsrc`). It contains `..\Win32\main.c` and `c:\ReleaseHomeworldSource\obj\Debug\*.obj` strings, so it is a **non-release (debug) build of exactly the released source**. Use it as a Rosetta stone for function signatures/FLIRT against the source. `SNAP: src/Win32/vc50.pdb` is a compile-time type PDB, not a link PDB.
* Cataclysm: linked with MSVC 6 (linker 6.0), PE timestamp 2000-12-21 (`docs/binary-fingerprint.md`, written by another agent this round). So the compiler differs from HW1 retail (Intel). Expect different codegen. Do not expect byte-identical HW1 functions.
* Cataclysm links the MSVC 6 **debug** C runtime (LIBCD/LIBCMTD), not the release one: `cataclysm.exe` contains `Microsoft Visual C++ Debug Library`, `dbgheap.c`, `dbgrpt.c`, `_CrtDbgReport`, `_CrtIsValidHeapPointer`, `_CrtCheckMemory` and `DAMAGE: after %hs block` (GAME strings, verification pass). The bytes at CET's `memAlloc` 0x006725D0 (`55 8B EC 51 6A 00 6A 00 6A 01 A1 14 8F A7 00 50 8B 4D 08 51 E8 ..`) fit debug-CRT `malloc(n)` = `_nh_malloc_dbg(n, _newmode, _NORMAL_BLOCK, NULL, 0)`, and CET's `memFree` 0x00673080 (`55 8B EC 6A 01 8B 45 08 50 E8 ..`) fits `free(p)` = `_free_dbg(p, _NORMAL_BLOCK)` (byte pattern, not disassembled). Use Ghidra's debug-CRT (LIBCD) signatures, not the release ones, to strip the CRT. The game code itself is not necessarily `/Od` (UNVERIFIED).

### 0.5 Byte-level checks against GAME made for this doc

No disassembly or execution. These read-only byte reads/searches of `original/game/cataclysm.exe`, `cataclysm.big` and `update.big` confirm several statements below:
* a string-presence test for about 250 literals (results in section 14),
* dword reads at CET-documented addresses (section 13),
* the `.big` headers (section 5.2).

---

## 1. Program entry and start-up

### 1.1 Start-up sequence

| # | step | ORIG anchor | GoK anchor | notes |
|---|---|---|---|---|
| 1 | single-instance guard: `CreateFileMapping(..., "LSMap")`. If it already exists, `FindWindow(windowTitle)`, restore that window and exit | `Win32/main.c:3293-3317` | `SDL/main.c:2147-2171` (`_WIN32` only) | `OutputDebugString("\r\nProgram already running.\r\n")` |
| 2 | `RegisterWindowMessage("CloseHomeworld")` | `Win32/main.c:3331` | removed | Cataclysm: `CloseCataclysm` (GAME string) |
| 3 | `utyOptionsFileRead()`: options file, then registry | `Win32/main.c:3338` → `Win32/utility.c:1064-1095` | `SDL/main.c:2178` → `SDL/utility.c:866-890` | see 1.3 |
| 4 | `opKeyboardLoad()`: hex keymap string into `KeyTranslations[]` | `Win32/main.c:3341` | `SDL/main.c:2181` → `Game/Options.c:1320-1336` | |
| 5 | `RegisterCommandLine(cmdline)`: writes the raw command line to `HKLM\<BASEKEYNAME>\CmdLine` | `Win32/main.c:16-46,3343` | `#if 0` (`SDL/main.c:290-322`) | |
| 6 | `ProcessCommandLine()`; an unknown option shows the help dialog and exits | `Win32/main.c:1464-1516,3347` | `SDL/main.c:930-983,2189` | see 1.2 |
| 7 | `glCapNT()/glCap95()`; MAIN_Password check | `Win32/main.c:3352-3368` | removed | CET hooks `glCapNT` (`CET: Cataclysm.h:143`) |
| 8 | `utyGameSystemsPreInit()` | `Win32/utility.c:3782-4065` | `SDL/utility.c:3499-3825` | key, font registry, data path, CD path, CD checks, music/voice presence, **heap**, **bigfiles**, CRCs, version string, `scriptSetTweakableGlobals()` |
| 9 | `InitWindow()` (register class, create window, `rinEnumerateDevices`, device-CRC check: if it changed, force the software renderer) | `Win32/main.c` InitWindow, `mainRegisterClass` at `:2153` | `SDL/main.c:1948-2002` | CET hooks the Cataclysm equivalent at `0x00563563` (`CET: PatchRGL.cpp:29-61`) |
| 10 | AVI/intro set-up (640x480 mode for Bink) | `Win32/main.c:3396-3420` | `SDL/main.c:2240-2266` | |
| 11 | `utyGameSystemsInit()` | `Win32/utility.c:4075-...` | `SDL/utility.c:3835-4247` | strings, **timer + task manager**, lock file, regions, sound, intros, renderer, mouse, render task, texture registry, baby callbacks, **universe + universe task**, network queues, captain task, then about 40 module `*Startup()` calls, front end start, demo start |
| 12 | main loop | `Win32/main.c:3434-3464` | `SDL/main.c:2284-2306` | section 2 |
| 13 | `utyGameSystemsShutdown()` writes options and registry | `Win32/utility.c` | `SDL/utility.c:4308-4661` | |

Start-up bits: each module that starts sets a bit in `utyStartedBits[SSA_NumberBits]` through `utySet/utySet2` (enum at `SDL/utility.h:43-100`). Shutdown tears down only the modules whose bits are set. On a failed init, main calls `utyGameSystemsPreShutdown()` (`SDL/utility.c:4257`) and shows `MessageBox(errorString)` (ORIG `Win32/main.c:3472`).

Data-file checks in pre-init: `fileExists(utyMusicFilename)` and `fileExists(utyVoiceFilename)` on the hard drive, then on the CD (`Win32/utility.c:3936-3954`; GoK `SDL/utility.c:3716-3735`). HW1 names: `HW_Music.wxd` and `HW_comp.vce`, plus `Homeworld.big`, `Update.big`, `Extra.big` (CD pad file) and `AutoLock.txt` (`Win32/utility.c:380-414`). Cataclysm has `Music4Cat.wxd`, `BeastSpeech.vce`, `SectSpeech.vce`, `Cataclysm.big`, `Update.big`, `Extra.big` and `AutoLock.txt` (GAME strings, file list). Two voice files means Cataclysm checks or opens per-race speech, which is a change (UNVERIFIED where).

### 1.2 Command line

* ORIG takes the single `LPSTR` from WinMain and tokenizes it with `strtok(cmd, TS_Delimiters)`, where `TS_Delimiters = " \t,="` (`GoK SDL/main.h:53`). A leading `-` is rewritten to `/`, and matching uses `_stricmp` (`Win32/main.c:1464-1516`). GoK walks `argv` instead (`SDL/main.c:930-983`).
* Table entry: `commandoption {flags, parameter, function, variableToModify, valueToSet, helpString}`. Flags: `COF_Visible 1` and `COF_NextToken 2` (`GoK SDL/main.h:57-64`). Macros `entryFn/entryVr/entryFV/entryFnParam/...Hidden/entryComment` are at `Win32/main.c:1109-1119` and `SDL/main.c:597-607`. A matching entry first writes `*(udword*)var = value`, then calls `function(token)`. `COF_NextToken` consumes the next token as the argument.
* Unknown option: `DebugHelpDefault()` builds `"Invalid or unrecognised command line option: '%s'\r\n"` plus all visible entries and shows dialog `IDD_CommandLine` (ORIG `Win32/main.c:1403-1451`). The string is present in Cataclysm.
* Table: ORIG `Win32/main.c:1120-1395`, GoK `SDL/main.c:608-851`. In the third column, "Y" means the literal is present in `cataclysm.exe` and "N" means absent (ASCII search, section 0.5).

| option | effect (variable / function) | release visibility (ORIG) | Cata exe |
|---|---|---|---|
| `/debug` | `DebugWindow=TRUE` | hidden | Y |
| `/nodebugInt` | `dbgInt3Enabled=FALSE` (no `int 3` on fatal) | hidden | Y |
| `/debugToFile` | `debugToFile=TRUE` → `debugMessages.txt` | hidden | Y |
| `/heap <n>` | `MemoryHeapSize` | visible | Y |
| `/prepath <path>` | `PrependPathSet` → `filePrependPathSet` (GoK renamed it `/bigoverride`) | visible | Y |
| `/CDpath <path>` | `CDROMPathSet` | visible | Y |
| `/freemouse` | `startupClipMouse=FALSE` | hidden | Y |
| `/ignoreBigfiles`, `/logFileLoads` | bigfile bypass / `FileLoads.log` | **not compiled in release** | N / N |
| `/enableSSE /forceSSE /enable3DNow` | CPU paths | visible | Y |
| `/noSound /noSpeech` | `SE_DEBUG` only | n/a | N / N |
| `/dsound /dsoundCoop /waveout /reverseStereo` | audio back end | visible | Y |
| `/rasterSkip /noBG /noFilter /noSmooth /stipple /noShowDamage` | detail | visible | Y |
| `/nilTexture /noEffects /NoFETextures` | debug switches | conditional | `/nilTexture` Y, `/noEffects` N, `/NoFETextures` N |
| `/safeGL /triple /nodrawpixels /noswddraw /noglddraw /sw /noFastFE /fullscreen /window /noBorder` | video | visible | Y |
| `/noSavedMode` | `mainAutoRenderer=FALSE` | hidden | Y |
| `/d3dDeviceCRC` | `mainOutputCRC` → `d3dDeviceCRC.txt` | hidden | Y |
| `/minny /640 /800 /1024 /1280 /1600` | resolution presets | visible | Y |
| `/device <dev>` | rGL device (`sw`, `fx`, `d3d`) | visible | Y |
| `/nohint /noPause /noMinimize` | GL hint, alt-tab behaviour | visible / hidden | Y |
| `/cheapShips /sensorLevel /noCompPlayer /notactics /noretreat` | cheats | not in release | N (all five) |
| `/disableAVI` | `enableAVI=FALSE` | hidden | Y |
| `/NoBind` | `bkDisableKeyRemap` | hidden in release | **N** |
| `/aiplayerLog /determCompPlayer /gatherStats /showStatsFight /showStatsFancyFight` | AI/stats | not in release | `/aiplayerLog` Y, other four N |
| `/logOff /logOn /logOnVerbose /logFilePath` | network log | hidden | Y |
| `/debugSync /noWon /forceLAN /noAuth /shortWon /BryceAndDrewAreGods` | network debug | mixed | N (all six) |
| `/testNIS /testNISScript /nisCounter /nisNoLockout` | NIS debug | conditional | N (all four) |
| `/packetRecord /packetPlay` | MP packet record/playback (section 9.3) | hidden | Y |
| `/disableFakeRenders` | demo playback | conditional | N |
| `/nopal` | `mainNoPalettes` | visible | Y |
| `/smCentreCamera /closeCaptioned` | misc | hidden | Y |
| `/noPlug /pilotView` | ORIG HW1 (`/noPlug` under `RND_PLUG_DISABLEABLE`; `/pilotView` unconditional, `Win32/main.c:1359,1363`) | | N / N |
| **Cataclysm-only** `/noCPV`, `/LoadMPGame`, `/SaveMPGames` (" - periodically save multiplayer games") | new | | Y (GAME strings) |
| GoK-only `/settingspath /msaa /debugScreenshots /vanilla` | port additions; `/vanilla` sets `UNIVERSE_UPDATE_RATE_SHIFT=0` (`SDL/main.c:848,2195-2198`) | | n/a |

`/demoRecord <file>` and `/demoPlay <file>` are **already commented out in ORIG**: the "RECORDED DEMOS" entries sit inside a `/* ... */` block (`Win32/main.c:1321-1325`), and GoK keeps the same comment (`SDL/main.c:809-813`; the handlers `EnableDemoRecord/EnableDemoPlayback` remain at ORIG `Win32/main.c:822-841`, GoK `:450-469`). So HW1 retail had no command-line demo trigger either; their absence from `cataclysm.exe` is inherited, not a Cataclysm change. ORIG also has `/password` (under `MAIN_Password`) and `/noPlug` (Relic logo on pause, under `RND_PLUG_DISABLEABLE`) (`Win32/main.c:1355-1360`); `/disablePacking`, `/allowPacking` and `/onlyPacking` are inside another `/* ... */` block (`:1345-1351`). None of these are in `cataclysm.exe`.

Complete list of NUL-bounded `/xxx` literals in `cataclysm.exe` (57; regex `(?<=\0)/[A-Za-z0-9]{2,30}(?=\0)` over the whole file, verification pass): `/1024 /1280 /1600 /640 /800 /aiplayerLog /CDpath /closeCaptioned /d3dDeviceCRC /debug /debugToFile /device /disableAVI /dsound /dsoundCoop /enable3DNow /enableSSE /forceSSE /freemouse /fullscreen /heap /LoadMPGame /logFilePath /logOff /logOn /logOnVerbose /minny /nilTexture /noBG /noBorder /noCPV /nodebugInt /nodrawpixels /noFastFE /noFilter /noglddraw /nohint /noMinimize /nopal /noPause /noSavedMode /noShowDamage /noSmooth /noswddraw /packetPlay /packetRecord /prepath /rasterSkip /reverseStereo /safeGL /SaveMPGames /smCentreCamera /stipple /sw /triple /waveout /window`. This is the retail command-line table to expect.

GOG starts the game as `cataclysm.exe /noswddraw` (`original/game/goggame-1766413869.info` playTasks).

### 1.3 Persistent configuration

**Registry (ORIG and Cataclysm; GoK dropped it).** `BASEKEYNAME`: ORIG `src/Win32/regkey.h` = `"SOFTWARE\\Sierra On-Line\\Homeworld"` (with Downloadable and OEM variants). The key is under `HKEY_LOCAL_MACHINE`. Cataclysm uses `SOFTWARE\Sierra On-Line\Cataclysm` (GAME string). `utyRegistryOptionsRead/Write` (`Win32/utility.c:86-171`) iterate `regOptionsList` (`Win32/utility.c:44-59`):

| value | type | variable |
|---|---|---|
| `deviceCRC` | DWORD | `opDeviceCRC` |
| `deviceCaps`, `deviceCaps2` | DWORD | `loadedDevcaps(2)` |
| `deviceIndex` | DWORD | `opDeviceIndex` |
| `deviceToSelect`, `glToSelect`, `d3dToSelect` | SZ | `mainDeviceToSelect`, `mainGLToSelect`, `mainD3DToSelect` |
| `screenWidth`, `screenHeight`, `screenDepth` | DWORD | `MAIN_WindowWidth/Height/Depth` |
| `HW_Language` | SZ | `regLanguageVersion` |
| `REG_MAGIC_STR` = `D657E436967616D4` | DWORD | `regMagicNum` (UK oversize-CD support) |
| `CmdLine` | SZ | written by `RegisterCommandLine` |

All of these names, including `D657E436967616D4`, are present in `cataclysm.exe` in the same block (they appear in reverse order in `.data`, `deviceCRC` at 0x008C2E28 down to `D657E436967616D4` at 0x008C2D94; MSVC 6 put these string literals in `.data`, not `.rdata`). So Cataclysm kept `regOptionsList` unchanged. The CET README says of its resolution unlock "No need to edit the registry anymore!" (`CET: README.md:22`). That points to the same `screenWidth/Height` values.

**Options file.** `DIS_FileName = "Homeworld.cfg"` (ORIG `Win32/debugwnd.h:55`), read from the **current directory** with `scriptSetFileSystem("", DIS_FileName, utyOptionsList)` (`Win32/utility.c:1088`). It is written back as `"%s    %d\n"` / `"%s    %s\n"` lines (`Win32/utility.c:1104-1145`). Cataclysm's file is **`Cataclysm.cfg`**: the string (0x008B6F80) sits next to `HW_DebugWindow` (0x008B6F68), `Cataclysm Debug Window` (0x008B6F50) and `debugMessages.txt` (0x008B6FA4) in `.data`, the same neighbourhood as debugwnd.c. The HW1 key list is `Win32/utility.c:879-970`: graphics, sound, colours, `PlayerName`, `PlayerPwd`, 19 key slots `NEXT_FORMATION...LAUNCH_MANAGER`, MP settings (`MPGameFlags`, `ResInjAmmount` with the original misspelling). Cataclysm's list (GAME strings) adds `enableRepairbots`, `enableLensFlare` and `enableCPV`, and drops `noPalMB`. GoK moved the file to `~/.homeworld/Homeworld.cfg` with `[section]` comment lines (`SDL/utility.c:142,636-786`). The debug window also stores `Location`, `Size`, `Font` and `Pane%d` in the same file (`SDL/debugwnd.c:566-620`).

**Separate key file (Cataclysm only).** `original/game/default_keys.cfg` begins `v 84` and `; Generated Keys Configuration File - DO NOT EDIT!`, followed by 197 lines of `key  <Command_Name>  <VK code>` or `SHIFT+<VK>` (two spaces between fields, CRLF line ends, 77 lines use `SHIFT+`). HW1 has no equivalent (section 11).

**Environment.** ORIG: `HW_Data` sets the prepend path and the bigfile compare root (`Win32/utility.c:3814`); `HW_Root` is used in main.c:1873 (GL DLL path). GoK: `HW_Data`, `HW_CDROM` and `HOME`/`APPDATA` (`SDL/utility.c:3528-3607,3367`). Memory: `USERNAME`/`USERPROFILE` seed the fill patterns (`Game/Memory.c:262-281`). `HW_Data` and `HW_Root` are present in `cataclysm.exe`.

---

## 2. Main loop and frame timing

### 2.1 Main loop

ORIG `Win32/main.c:3434-3464`:
```c
while (TRUE) {
    Sleep(0);
    if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) { if (!GetMessage(...)) break; Translate/Dispatch; }
    else utyTasksDispatch();                       // tasks run only when the message queue is empty
    if (opTimerActive && taskTimeElapsed > opTimerStart + opTimerLength) opTimerExpired();  // video-mode confirm countdown (22 s, Game/Options.c:50-52)
    if (patchComplete) { RunPatcher(); patchComplete = 0; }
}
```
GoK `SDL/main.c:2284-2306` drains **all** SDL events first, then calls `utyTasksDispatch()` once. The game loop has no frame cap and no vsync wait: render is a once-per-frame task (2.2). Cataclysm added a render-task frame limiter (section 13).

### 2.2 Timer to ticks to tasks (the accumulator)

`utyTasksDispatch` (ORIG `Win32/utility.c:4846-4857`, GoK `SDL/utility.c:4671-4682`):
```c
QueryPerformanceCounter(&timer);
difference      = timer - utyTimerLast;
utyNFrameTicks  = difference / utyTimerDivisor;     // whole task ticks this frame
remainder       = difference % utyTimerDivisor;
taskExecuteAllPending(utyNFrameTicks);
utyTimerLast    = timer - remainder;                // carry the sub-tick remainder: no drift
```
Set-up (ORIG `Win32/utility.c:4095-4110`): `QueryPerformanceFrequency(F)`. If `F < UTY_TimerResloutionMax` it fails with "System performance timer runs at %d Hz.  We need at least %d Hz.". Then `utyTimerDivisor = F / 240` (a `LONGLONG`, `:364`) and `taskStartup((udword)(F / utyTimerDivisor))`, which gives `taskFrequency ≈ 240` (stored as `real32`). `UTY_TimerResloutionMax 240` is spelled that way in ORIG `Win32/utility.h:39`. GoK defines it as `(15 * UNIVERSE_UPDATE_RATE)` (`SDL/utility.h:39`).

Per task (`taskExecuteAllPending`, ORIG `Game/task.c:380-...`, GoK `Game/Task.c:280-381`):
* `taskTimeDelta = ticks / taskFrequency; taskTimeElapsed += taskTimeDelta` (the global real-time clock in seconds).
* `TF_OncePerFrame` task: called exactly once per dispatch, **even when `ticks == 0`**.
* Periodic task: `local = ticks + task->ticks; task->ticks = local % ticksPerCall; calls = min(local / ticksPerCall, TSK_MaxTicks=8)`. Ticks beyond the 8-call cap are **discarded**: the game slows down instead of spiralling.
* `ticksPerCall = (udword)(period * taskFrequency)` (`Game/task.c:252`).

Effective universe rate. The universe task period is `UNIVERSE_UPDATE_PERIOD = 1/16 s`:

| build | clock | TimerResMax | divisor | taskFrequency | universe ticksPerCall | universe Hz | max universe updates per frame |
|---|---|---|---|---|---|---|---|
| ORIG HW1 / Cataclysm | QPC | 240 | F/240 | 240 (truncated) | 15 | **16.0** | 8 (0.5 s of game time) |
| GoK default (`UNIVERSE_UPDATE_RATE_SHIFT=2`, `Game/Universe.c:75`) | `SDL_GetTicks` ms | 960 | 1000/960 = 1 | 1000 | (udword)15.625 = 15 | 66.67 (4.2 % fast) | 8 (0.125 s) |
| GoK `/vanilla` | ms | 240 | 1000/240 = 4 | 250 | (udword)15.625 = 15 | 16.67 (4.2 % fast) | 8 |
| CET `UpdateRateShift=2` | QPC | 960 (`CET: Config.h:31-34`) | F/960 | 960 | 15 | 64.0 | 8 |

Port rule: use a high-resolution clock and keep `taskFrequency = 240·2^shift` exact. Do not use millisecond integer division.

Rendering has no interpolation. The renderer draws the last universe state, which causes the "choppy 16 FPS" look the CET README describes.

### 2.3 `universeUpdateTask`

ORIG `Game/universe.c:3765`, GoK `Game/Universe.c:3786-4063`. It is a periodic task (`taskStart(universeUpdateTask, UNIVERSE_UPDATE_PERIOD, 0)`, ORIG `Win32/utility.c:4228`, GoK `SDL/utility.c:4040`). Each call:
1. Game-start state machine: `CHECK_AUTODOWNLOAD_MAP` → `AUTODOWNLOAD_MAP` → `PROCESS_TEXTURES` (calls `gameStart`) → `GAME_STARTED` (`Game/Universe.c:3806-3897`).
2. While the game runs, `universeUpdateCounter++` (a task-local counter, separate from `universe.univUpdateCounter`). Render lists rebuild when `(counter & REFRESH_RENDERLIST_RATE) == ..._FRAME` (defaults 7/1 and minor 7/3, `Game/Tweak.c:393-397`).
3. **Multiplayer and packet playback** (`:3902-3993`): `clWaitSyncPacket()`. Each received sync packet causes **two** `univUpdate(UNIVERSE_UPDATE_PERIOD)` calls with a `taskYield` between them (`:3973,3986`; ORIG `:3957,3972`). The captain sends one packet per `captainServerTask` call with `CAPTAINSERVER_PERIOD = 1/8 s` (`Game/CommandNetwork.h:126`, task at `Game/CommandNetwork.c:2024`). So MP lockstep is 8 packets/s × 2 = 16 updates/s. `TOO_MANY_PACKETS` skips the yield so the client catches up. Lag display uses `(counter & INTERNET_LAG_UPDATERATE=31)` (`Game/LagPrint.h:23`).
4. **Single player** (`:3995-4043`): pause, tutorial-only KAS stepping, optional turbo (GoK: `turboTimeCompressionFactor-1` extra updates, default 8, `Game/Universe.c:102`; ORIG: `UNIVERSE_TURBO_REPEAT 19` in debug only, `Game/universe.c:95-96`), then one `univUpdate(UNIVERSE_UPDATE_PERIOD)`.

### 2.4 `univUpdate(real32 phystimeelapsed)`

ORIG `Game/univupdate.c:7422`, GoK `Game/UnivUpdate.c:7271`. `universe.univUpdateCounter++` (`:7323`), `universe.phystimeelapsed = phystimeelapsed` (`:7327`) and `universe.totaltimeelapsed += phystimeelapsed` (`:7328`), then all simulation. `univupdateReset()` (ORIG `:6322`, GoK `:6214`) sets `phystimeelapsed = UNIVERSE_UPDATE_PERIOD` and `totaltimeelapsed = 0` and zeroes the counter. In Cataclysm, `universe` is at `0x00B62A00`, `phystimeelapsed` at +0xBC, `totaltimeelapsed` at +0xC0, a new `realtimeelapsed` at +0xC4 and `univUpdateCounter` at +0x1AC (`CET: game/Game/universe.h` static_asserts; `Cataclysm.h:189,210`).

### 2.5 Every place that assumes 16 Hz

GoK replaced the hard-coded assumptions with `UNIVERSE_UPDATE_RATE_ORIG 16`, `UNIVERSE_UPDATE_RATE_SHIFT` (runtime ubyte, default 2), `UNIVERSE_UPDATE_RATE_FACTOR`, `UNIVERSE_UPDATE_RATE`, `UNIVERSE_UPDATE_PERIOD` and the `UNIVERSE_WOODPECKER(r,f)` macro (`Game/Universe.h:44-53`). The macro runs masked work only on every FACTOR-th update and tests `(counter >> SHIFT) & r == f`. So the GoK grep (61 sites) is a ready-made inventory. ORIG has a flat `#define UNIVERSE_UPDATE_RATE 16` / `UNIVERSE_UPDATE_PERIOD (1.0f/16)` (`Game/universe.h:57-58`).

**A. Rate/period constants** (GoK anchors; ORIG uses the same expressions with the constant 16):

| site | use |
|---|---|
| `SDL/utility.h:39` | timer resolution 15×rate |
| `SDL/utility.c:4040`, `Game/NIS.c:580` | task periods (universe, NIS) |
| `Game/Task.h:90` `BABY_CallBackPeriod 1.0f/16.0f` | **literal 16** (not derived from the rate); also ORIG `task.h:113` |
| `Game/UnivUpdate.c:6216` | reset of `phystimeelapsed` |
| `Game/Universe.c:3973,3986,4028,4037`, `Game/Stats.c:434` (ORIG `universe.c:3957,3972,4016,4027`, `stats.c:523`) | live `univUpdate()` call sites. ORIG `universe.c:4016` (turbo loop) is compiled out of release builds (`#if (!UNIVERSE_TURBORECORD_ONLY)`, `universe.h:37-50`), so an ORIG release build has **4**. The NIS teaser call (GoK `NIS.c:543`, ORIG `nis.c:537`) is inside a `/* ... */` block in both trees (dead code). Cataclysm has exactly **5** direct `E8` calls to `univUpdate` 0x0054C3F0, each preceded by `push 0x3D800000`: 0x00524846, 0x005256BC, 0x0053E8AA, 0x0053E8E0, 0x0053E9BA (the 5 CET push sites + 5). Three lie inside `universeUpdateTask` (0x0053E460, before `GetShipStaticInfo` 0x0053EA50), matching ORIG's MP×2 + SP×1; the other two (0x00524841, 0x005256B7) are in code between `speechEventQueue` 0x00522560 and `scriptSetReal32CB` 0x005259D0, owner UNVERIFIED (HW1 `stats.c` `HaveShipsFight` is the only other live HW1 caller) |
| `Game/Universe.c:1408-1412` `scriptSetFramesCB` | script field in **seconds × rate** → frames |
| `Game/SpaceObj.h:1109` `RECENT_ATTACK_DURATION ((ubyte)(5*RATE))` | stored in `ubyte recentlyAttacked` (`:1306`). **At 64 Hz, 320 truncates to 64 (1 s instead of 5 s)**, a latent GoK bug |
| `Game/Battle.h:78` | `RECENT_ATTACK_DURATION - RATE/2` |
| `Game/ETG.h:246` `ETG_UpdateRoundOff`, `Game/ETG.c:1546,6580-6581` | effect timing |
| `Game/Gun.c:1657,1665` | DPS estimate ("gunShipFirePower") |
| `Game/Alliance.c:57` | alliance proposal timeout `RATE*90` |
| `Game/ResCollect.c:848` | `RESOURCE_NOT_ACCESSIBLE_SECONDS` |
| `Ships/Mothership.c:79` | `DOCKPOINT_BUSY_TOO_LONG RATE*60` |
| `Game/Crates.c:704` | crate expiry |
| `Game/ConsMgr.c:87` | `CM_TaskServicePeriod` |
| `Game/NIS.c:438-546,1612,4299` | NIS fades, seeks, camera step |
| `SDL/render.c:2355,2616` | beam bullets drawn only on their first update |
| `Game/CommandNetwork.h:126` + `Game/Universe.c:3973/3986` | **MP: 1/8 s packets × 2 updates**, not scaled by GoK |
| `Game/LagPrint.h:23` | `INTERNET_LAG_UPDATERATE 31` (2 s at 16 Hz) |

**B. Frame-mask schedules `(universe.univUpdateCounter & RATE) == FRAME`**. ORIG sites; GoK converts each to `UNIVERSE_WOODPECKER`:

| file | ORIG lines | GoK lines | masks |
|---|---|---|---|
| univupdate.c | 5672, 6046, 6175, 6246, 7424, 7437, 7527, 7560, 7566, 7584, 7712, 7730 | 5582, 5966, 6095, 6166, 7279, 7292, 7384, 7417, 7423, 7441, 7567, 7585 (+ factor gates 5553, 5653, 7433, 7490, 7496) | prox sensor 31/27, out-of-world, passive attack 15/31, autosave 31 and 255, RU regen `regenerateRUrate`, coll blobs 7/6,4, research 15/13, regrow 255/86 (CET), bounties 127/101 |
| AIPlayer.c | 850, 865, 878 | 846, 861, 874 | `aiplayerUpdateRate`, tutorial KAS |
| physics.c | 575, 880, 1048, 1193, 1337 | 453, 621, 766, 913 (+775 factor) | `UPDATE_COLLRECT_RATE/FRAME` |
| rescollect.c | 600, 1124 | 596, 1093 | `shipstatic->harvestRate` (CET reads it at static+0x25C) |
| crates.c | 691 | 686 | `CRATES_ArePlayersNearUpdateMask` |
| formation.c | 982 | 987 | 7/2 |
| AIShip.c | 1954, 2300 | 1905, 2251 | missile retarget, mine search |
| tactics.c | 1712, 1923, 2030 | 1674, 1885, 1992 | 15/14, 31/26, 255/254 |
| netcheck.c | 463, 473 | n/a | sync checksum ring |
| CommandLayer.c | 984, 1145, 1237, 1306, 1837, 1998, 2246, 2261, 8391, 8447, 8759 | 957, 1118, 1210, 1279, 1809, 1970, 2218, 2233, 8314, 8370, 8682 | retarget, bogey check, evasive, sphere, attack-move 31/22 |
| universe.c (task-local counter) | render lists, lag | `Game/Universe.c:3906,3911,3935` | 7/1, 7/3, 31 |
| Ships/ | not checked out | CloakGenerator.c:335,406; DefenseFighter.c:621,650; GravWellGenerator.c:508; ProximitySensor.c:331,380 | |
| Collision.c | | 1840 (factor gate) | |

**C. Data-driven frame counts** (the assumption lives in the **data**, not the code):
* `Tweak.script` RATE/FRAME masks overwrite the `Game/Tweak.c:362-410` defaults.
* `AUTOSTABILIZE_FRAME_KICK_IN` (ubyte, `Game/Tweak.c:203`). The shipped Cataclysm script comment reads "number of univupdate frames ... 16 == 1 second" (`CET: assets/tweak.script`).
* Ship statics `harvestRate`, AI `aiplayerUpdateRate`, `regenerateRUrate` (Cataclysm `RegenerateRURate` = 255 at `0x0088F580`, "once every 16s", `CET: PatchUniverse.cpp:287`).
* `HYPERSPACE_SLICE_RATE`: engine default 0.075 at `0x0089410C` (byte-read), script value 0.025 (`CET: PatchUniverse.cpp:20-24`).
* Any field read with `scriptSetFramesCB`.
* Trail segment counts and ETG `nParticleBlocks` (CET scales both, `PatchUniverse.cpp:107-117`, `PatchETG.cpp:49-59,82`).
* AI external build timers (`buildTime / rate`, `CET: PatchUniverse.cpp:179-191`).

**D. Per-update quantities that are not time-scaled.** These apply a fixed amount per `univUpdate`. CET had to gate them to every FACTOR-th frame: missile trail update, Beast mothership self-damage, beam bullet collisions (`CET: PatchUniverse.cpp:98-141,221-253`), plus DefenseFighter `DamageRate` (GoK `Ships/DefenseFighter.c:621`).

**E. Type-width hazards.** Any frame count stored in a `ubyte` or `uword` (`recentlyAttacked`, `recentlyFiredUpon`, `autostabilizewait`) overflows at higher rates.

---

## 3. Task scheduler

### 3.1 Structures

ORIG (`Game/task.h:44-93`): `TSK_NumberTasks 32` and flags `TF_Allocated 1`, `TF_Paused 2`, `TF_OncePerFrame 4` and `TF_PauseSave 16`.
```c
typedef struct {            // ORIG, 48 bytes with TASK_STACK_SAVE
    udword flags;  taskfunction function;  udword ticks;  udword ticksPerCall;
    udword ebx, ecx, edi, esi, esp, ebp, eip;   // saved context at TOF_Context = 16
    udword nBytesStack;     // extra stack bytes copied after the struct (memRealloc'd "task+Stack")
} taskdata;
typedef void (*taskfunction)(void);
#define taskYield(n) ((void (*)(void))taskFunctionContinue)()
#define taskExit()   ((void (*)(void))taskFunctionExit)()
```
`taskStart(function, period, flags)` (ORIG `Game/task.c:219-342`) allocates the struct (`memAlloc(.., "taskData", NonVolatile)`), saves esi/edi/esp/ebp/ebx into statics, points `taskFunctionContinue` at the label `taskContinued` and `jmp`s into the task. When the task calls `taskYield`, the code pops the return EIP, stores the registers at `TOF_Context+0..24`, copies up to 255 bytes of the task's live stack frame (`TASK_STACK_SAVE`) into the block, restores the caller's registers and returns the handle. `taskExecuteAllPending` (`:380-...`) uses the same trick in reverse to resume each task at its saved EIP.

GoK/HWSDL (`Game/Task.h:60-69,148-183`) replace this with switch-based protothreads: `taskdata {flags, ticks, ticksPerCall, function, context, name}`, and `taskYield` stores `__LINE__` in a malloc'd context (`documentation/tasks.txt` in HWSDL). Task locals must be `static` or live in the context struct.

**Decompiler notes for Cataclysm.** If Cataclysm kept the ORIG asm scheduler (CET's `game/Game/task.h` is the ORIG header with the callback block `#if 0`'d out at `:115`; it still declares the ORIG 3-argument `taskStart` at `:203`, which CET never calls, so it is **not** evidence about Cataclysm's signature; UNVERIFIED for the binary):
* every task body is a `void f(void)` with `static` locals;
* it contains `call [taskFunctionContinue]` (yield) and `call [taskFunctionExit]` sites;
* control "returns" into the middle of the function on the next frame.
Ghidra will mis-analyse these call sites as calls that never return. Mark `taskFunctionContinue` and `taskFunctionExit` as non-returning, and treat the code after each yield as a separate entry. Port strategy: re-express each task as a protothread (GoK style) or as an explicit state machine. Cataclysm names its tasks (GAME strings `UnivUpdateTask`, `RenderTask`, `CaptServTask`, `RegionProcessTask`, `NisUpdateTask`, `PingTask`, `PlugRenderTask`, `MultiplayerGameTask`, `MultiplayerLanTask`, `BabyTasks`). So **Cataclysm's `taskStart` takes a name argument** that HW1 lacks. The bytes at `0x005AC173` read `push ebp` (flags) / `push 0x3D800000` (1/16 s) / `push 0x0053E460` (universeUpdateTask) / `push "UnivUpdateTask"` / `call`. Under cdecl that gives the signature **`taskStart(char *name, taskfunction fn, real32 period, udword flags)`** (inferred from the push order, not disassembled). The call is `E8` to **0x0052A5F0** (`taskStart`), and a byte scan finds exactly 10 `E8` calls to 0x0052A5F0, one per name string, all with the same push shape (section 3.3). GoK has a similar but differently ordered `taskStartName(fn, name, period, flags)` behind a `taskStart(f,p,fl)` macro that stringifies the function name (`Game/Task.h:195-198`); Cataclysm's names are hand-written (`UnivUpdateTask`, not `universeUpdateTask`). The next instructions are two calls followed by `push 50000; push 0x00C39320; call`, which matches HW1's `autodownloadmapStartup(); KeepAliveStartup(); InitQueue(&ProcessSyncPktQ,50000)` (ORIG `Win32/utility.c:4230-4232`).

### 3.2 Control API

`taskStartup(freq)`, `taskShutdown`, `taskStart`, `taskStop`, `taskPause/Resume`, `taskFreezeAll/ResumeAll` (save the pause bit in `TF_PauseSave`), `taskSavePauseStatus`, `taskPeriodSet` (GoK `Game/Task.c:80-526`). Alt-tab: single player calls `taskFreezeAll`; MP or `/noPause` pauses only the render and region tasks (`SDL/main.c:1108-1145`). `utyTaskTimerClear` (drop pending ticks) runs in `DeactivateMe` (GoK `:1118`, ORIG `Win32/main.c:1741`), not on activate; `ActivateMe` only calls `taskResumeAll` (GoK `:1170`, ORIG `:1809`). Paused tasks are skipped before their tick counter is updated (`Game/Task.c:314-316`), so frozen tasks do not catch up after alt-tab, while `taskTimeElapsed` keeps advancing as long as the loop dispatches (read from the code, not run).

### 3.3 Task table (retail registration order = execution order within a frame)

Handles are allocated first-free (`taskPointerAlloc`, GoK `Game/Task.c:160-178`) and executed in ascending handle order. So the per-frame order is the registration order below. That order is derived from `utyGameSystemsInit`; the handle numbers are UNVERIFIED at runtime.

| expected handle | task | period / flags | registered at (ORIG / GoK) | Cata name string |
|---|---|---|---|---|
| (debug only) | `memStatsTaskFunction` | 1.0 s | `Win32/utility.c:4115` / `SDL/utility.c:3915` | n/a |
| 0 | `regProcessTask`: input, regions, demo state save/load | `REG_TaskFrequencyOPF`=1, `TF_OncePerFrame` | `Game/region.c:994` / `Game/Region.c:984` (in `regStartup`) | `RegionProcessTask` |
| (debug only) | `rndFrameRateTaskFunction`, `rndPolyStatsTaskFunction` | 2.0 s | `Win32/render.c:1043,1050` / `SDL/render.c:1186,1193` (`RND_FRAME_RATE`) | n/a |
| 1 | `rndRenderTask` | 1.0, `TF_OncePerFrame` | `Win32/utility.c:4218` / `SDL/utility.c:4027` | `RenderTask` |
| 2 | `taskCallBackProcess` (baby callbacks) | `BABY_CallBackPeriod` = 1/16 s | `Game/task.c:808` / `Game/Task.c:539` via `taskCallBackInit` (`SDL/utility.c:4031`) | `BabyTasks` |
| 3 | `universeUpdateTask` | 1/16 s | `Win32/utility.c:4228` / `SDL/utility.c:4040` | `UnivUpdateTask` |
| 4 | `captainServerTask` | 1/8 s | `Win32/utility.c:4238` / `SDL/utility.c:4050` | `CaptServTask` |
| 5 | `pingUpdateTask` | 0.5 s | `Game/ping.c:196` / `Game/Ping.c:199` | `PingTask` |
| 6 | `nisUpdateTask` | 1/16 s | `Game/nis.c:575` / `Game/NIS.c:580` | `NisUpdateTask` |
| dynamic | `mgProcessCallBacksTask` / `lgProcessCallBacksTask` | 0.25 s | `Game/MultiplayerGame.c:6538`, `MultiplayerLANGame.c:2829` / `MultiplayerLANGame.c:2886` | `MultiplayerGameTask`, `MultiplayerLanTask` |
| dynamic | `psRenderTaskFunction` (plug screens replace the render task) | 1.0, OPF | `Game/PlugScreen.c:718` / `:700` | `PlugRenderTask` |

So within one frame the order is: input and region processing, then render, then baby callbacks, then 0 to 8 universe updates, then the captain server. The render task runs **before** the universe advances in the same frame.

Cataclysm registrations recovered from the bytes before the 10 `call 0x0052A5F0` sites (verification pass; push values read directly, `push ebp`/`push edi` flags are register values, assumed 0; not disassembled):

| call site | name string | task function | period | flags | HW1 equivalent |
|---|---|---|---|---|---|
| 0x004C046E | `MultiplayerGameTask` (0x00887B08) | 0x004BF7F0 | 0x3E800000 = 0.25 | `edi` | `mgProcessCallBacksTask`, `MG_TaskServicePeriod` |
| 0x004C7403 | `MultiplayerLanTask` (0x008883F4) | 0x004C6DF0 | 0.25 | 0 | `lgProcessCallBacksTask` |
| 0x004CC901 | `NisUpdateTask` (0x0088927C) | 0x004CC600 | 1/16 | 0 | `nisUpdateTask` (CET `nisStartup_UNIVERSE_UPDATE_PERIOD` 0x004CC8F2) |
| 0x004E866A | `PingTask` (0x0088DCA4) | 0x004E8540 | 0x3F000000 = 0.5 | 0 (pushes at 0x004E862D, interleaved with x87 code before the call) | `pingUpdateTask`, `PNG_TaskPeriod` |
| 0x004EA7AF | `PlugRenderTask` (0x0088DDD0) | 0x004EA4A0 | 1.0 | 4 (`TF_OncePerFrame`) | `psRenderTaskFunction` |
| 0x004F0A5A | `RegionProcessTask` (0x0088F4F4) | 0x004F09A0 | 1.0 | 4 | `regProcessTask` |
| 0x0052AF21 | `BabyTasks` (0x008AA720) | 0x0052B020 | 1/16 | 0 | `taskCallBackProcess` (CET `BabyTasksStartup_...` 0x0052AF12) |
| 0x005AC14E | `RenderTask` (0x008C3140) | 0x0057A910 | 1.0 | 4 | `rndRenderTask` |
| 0x005AC189 | `UnivUpdateTask` (0x008C3130) | 0x0053E460 | 1/16 | `ebp` | `universeUpdateTask` |
| 0x005AC1F2 | `CaptServTask` (0x008C3120) | 0x00451580 | 0x3E000000 = 1/8 | `ebp` | `captainServerTask`, `CAPTAINSERVER_PERIOD` |

Between `UnivUpdateTask` and `CaptServTask` the bytes are 2 calls, then `InitQueue` pushes 50000, 50000, 20000, 10000 and one more call, which is exactly ORIG `Win32/utility.c:4228-4238` (`autodownloadmapStartup; KeepAliveStartup; InitQueue×4; InitLastSyncPktsQ`). No debug tasks (`memStats`, frame-rate, poly-stats) are registered. The frame-limiter NOPs CET writes (0x0057B78B, 0x0057B7A1) lie 0xE7B bytes after `RenderTask`'s function start, which is consistent with the CET ini's "built-in framerate limiter in the rendering task" (`CataclysmEngineTweaks.ini:13`), but the function's end is not established (UNVERIFIED).

### 3.4 Baby callbacks

`taskCallBackRegister(fn, num, data, delaySeconds)` allocates a `BabyCallBack` (`memAlloc(..,"BabyCallBack",Volatile)`), timed on `taskTimeElapsed`. The callback fires when `taskTimeElapsed - timeStarted >= timeToExecute`; it returns TRUE to delete itself, otherwise it re-arms (GoK `Game/Task.c:536-621`).

---

## 4. Memory manager

There are no handles. Every block is a **raw pointer** preceded by a 32-byte cookie (`sizeof(memcookie)` must equal 32; `memStartup` fails with "Bad cookie size: %d, %d" otherwise, GoK `Game/Memory.c:217-224`).

* **Heap acquisition.** `MemoryHeapSize` defaults to `MEM_HeapSizeDefault`. When the user gives no `/heap`, it becomes `min(0.40 × physical RAM, MEM_HeapDefaultMax)` if that is larger. ORIG sizes: **18 MB default, 64 MB max** (`Game/memory.h:103-105`). GoK/HWSDL: 64 MB and 128 MB (`Game/Memory.h:115-117`). The heap is one `VirtualAlloc(MemoryHeapSize + 4 cookies)` (`Win32/utility.c:3957-3981`; GoK `SDL/utility.c:3737-3780`, mmap on POSIX).
* **Cookie** (`Game/Memory.h:170-185`): `udword flags` (low 6 bits are flags; high bits must equal `MBF_VerifyValue = 0xfa7babe5 & 0xffffffc0`), `name[MEM_NameLength]` (20 bytes in release, 16 with `MEM_DETECT_VOLATILE`), `sdword blocksNext` and `blocksPrevious` (doubly linked by block counts). Granularity: 1 block = 32 bytes.
* **Flags** (`:66-80`): `MBF_AllocatedNext 1`, `MBF_AllocatedPrev 2`, `MBF_SmallBlockHeap 4` (= `Pyrophoric` = `Flammable`), `MBF_NonVolatile 8` (= `NonVolatile`), `MBF_String 16`, `MBF_ExtendedPoolOnly 32` (= `ExtendedPool`). `Volatile` = 0.
* **Pools.**
  * Main pool `memMainPool`. Small-block heaps sit at the bottom of the pool, sized from `memSmallHeapInfo` = {64×3072, 256×768, 384×512, 512×128, 1024×32, 2048×16}, scaled by `heapSize/MEM_HeapSizeDefault` (ORIG `Game/memory.c:48-57`, GoK `Game/Memory.c:55-64,297-387`).
  * Volatile allocations walk up from `firstFree`, with per-size first-free hints `firstSize[]`.
  * Non-volatile allocations (`memAllocFunctionANV`) are carved from the **top** (`lastFree`, `lastSize[]`) to limit fragmentation.
  * `Pyrophoric` (small-block) allocations go to a fixed-size SBH list first.
* **Growth.** When the main pool fails, the allocator tries up to `MGH_NumberGrowthHeaps 16` extra pools of at least 8 MB from the `memgrowcallback` (`VirtualAlloc`) (`Game/Memory.c:1141-1163`). Fatal messages: "Exceeded %d growth heaps, each of length at least %d.", "Could not allocate growth heap %d with a size of %d", and in release "memAlloc: failed to allocate %d bytes." (`:1307-1313`).
* **API.** `memAlloc` (fatal on failure), `memAllocAttempt` (returns NULL), `memRealloc`, `memFree`, `memStringDupe(NV)`, `memClearDword`, `memStrncpy`, `memRename`, plus analysis/statistics in debug builds (`Game/Memory.h:366-409`).
* **Fill patterns.** `MEM_ClearSetting 0x4e7a110c` and `MEM_FreeSetting 0xf4eeda7a` are re-seeded from `crc32(getenv("USERNAME"/"USERPROFILE"))` (`Game/Memory.c:262-283`). Neither constant is in `cataclysm.exe` (dword search). `MBF_VerifyValue 0xfa7babc0` appears 3 times, so Cataclysm still uses cookies.
* **Port notes.** The heap is a single preallocated arena. Code can depend on allocation order (for example, NV-from-top). A 1:1 port can keep this allocator (it is about 2,200 lines of portable C in GoK) or map it to malloc. If you map it, keep `memAlloc`'s never-returns-NULL contract. GoK runs 64-bit with `HW_PTR_64` 64-byte cookies (`Game/Memory.h:86-96`). CET calls a Cataclysm function it names `memAlloc` at `0x006725D0` as a one-argument function (`CET: Cataclysm.h:147`), with `memFree` at 0x00673080. **This is not the HW1 cookie allocator.** ORIG `Win32/rinit.cpp:73-87` defines a file-local C++ `void *memAlloc(int size)` (`malloc` + `memset`) and `void memFree(void *)` (`free`) for the renderer device list, and CET's `game/Game/rinit.cpp:27,100` re-implements that file. The bytes at 0x006725D0/0x00673080 match debug-CRT `malloc`/`free` (section 0.4), with 39 and 30 `E8` callers. So CET's `memAlloc` is the CRT `malloc` (the rinit helper was probably inlined; UNVERIFIED). The HW1 allocator is elsewhere: the three `MBF_VerifyValue` (0xFA7BABC0) immediates are at 0x004AD7EE, 0x004AD833 and 0x004ADD27, so Cataclysm's `memory.c` code is around 0x004AD000 (UNVERIFIED which functions).

---

## 5. File I/O and `.big` archives

### 5.1 File layer (`Game/File.h`, `Game/File.c`)

* Handles: `filehandle` is an index into `filesOpen[MAX_FILES_OPEN+1 = 33]`. Slot 0 is unused so that 0 can mean failure (GoK `Game/File.c:61-66`).
* `fileOpenInfo` (`Game/File.h:81-99`) holds `inUse, path, usingBigfile, fileP, bigFP, bigTOC, textMode, offsetStart, decompBuf, offsetVirtual, length`.
* A compressed bigfile member opened as a stream is fully decompressed into `decompWorkspaceP`. That buffer is reused and grows in 64 KB steps (`decompWorkspaceIncrement`). If it is already in use, a temporary `memAlloc(len,"decompBuf")` is used (`Game/File.c:1222-1286`). Uncompressed members are read through `bigFP` with `offsetStart` and `offsetVirtual`.
* Flags (`Game/File.h:48-60`): `FF_TextMode 0x1`, `FF_IgnoreDisk 0x2`, `FF_WriteMode 0x10`, `FF_AppendMode 0x20`, `FF_ReturnNULLOnFail 0x40`, `FF_CDROM 0x80`, `FF_IgnoreBIG 0x100`, `FF_IgnorePrepend 0x200`; GoK adds `FF_UserSettingsPath 0x400` and `FF_HomeworldDataPath 0x800`.
* Paths:
  * ORIG keeps `filePrependPath`, set by `/prepath` or `HW_Data` and forced to end in `\`, plus `fileCDROMPath`. `filePathPrepend(name, flags)` returns `prepend + name`, or the CD path with `FF_CDROM`, or `name` unchanged with `FF_IgnorePrepend` (ORIG `Game/file.c:57-59,1168-1236`).
  * GoK has four roots (`fileHomeworldDataPath`, `fileOverrideBigPath`, `fileUserSettingsPath`, `fileCDROMPath`; `Game/File.c:69-84,1889-1927`).
  * Saves, screenshots, `AutoLock.txt` and the cfg go to the user settings root in GoK. ORIG writes them to the prepend path, which is the current directory in a retail install.
* Text mode: on Windows, `"rt"`. On other platforms the port strips CRs itself, case-corrects names (`FILE_CASE_INSENSITIVE_SEARCH`) and maps `\` to `/` (`Game/File.c:116-140,342-...`).
* API: `fileLoadAlloc`, `fileLoad`, `fileSave`, `fileOpen/Close/Seek/BlockRead/LineRead/CharRead`, `fileExists`, `fileSizeGet`, `fileStream`, `fileUsingBigfile`, `logfileClear/Log/Logf` (`Game/File.h:112-153`).

### 5.2 `.big` on-disk format (RBF1.23)

Sources: `HWSDL: documentation/File Formats/BIG Specification.txt` (community spec plus Relic's "BIGaddendum"), `Game/BigFile.h:66-127` and `Game/BigFile.c:327-511`. Little-endian. Structs follow `-Zp4`:

| offset | size | field |
|---|---|---|
| 0 | 7 | ASCII `"RBF1.23"`, built with `sprintf("%s%s", BF_FILE_HEADER "RBF", BF_VERSION "1.23")` and checked with `fgets`+`strcmp` (`bigHeaderVerify`) |
| 7 | 4 | `int numFiles` |
| 11 | 4 | `int flags` (bit 0 `BF_FLAG_TOC_SORTED`: TOC sorted ascending by (CRC1, CRC2), so lookups binary-search) |
| 15 | 32 × numFiles | `bigTOCFileEntry[]` (below) |
| ... | | per file at `offset`: `nameLength+1` bytes of encrypted name, NUL included, then `storedLength` bytes of data |

`bigTOCFileEntry` (32 bytes):

| off | type | field |
|---|---|---|
| 0 | u32 | `nameCRC1` = crc32(first half of the lowercased, `\`-normalised name) |
| 4 | u32 | `nameCRC2` = crc32(second half) |
| 8 | u16 | `nameLength` (excludes NUL) |
| 10 | 2 | padding (00 00 in shipped files) |
| 12 | u32 | `storedLength` |
| 16 | u32 | `realLength` |
| 20 | u32 | `offset` (from the file start to the name blob) |
| 24 | u32 | `timeStamp` (time_t, used by the override compare) |
| 28 | u8 | `compressionType` (0/1) |
| 29 | 3 | padding, always `C9 CA CB` |

Verified on GAME: `cataclysm.big` starts `RBF1.23`, `numFiles=16528`, `flags=1`; its first entries have `nameLength` 30/31, padding `0000`, compression 1 and trailing bytes `c9cacb`. `update.big`: `RBF1.23`, 39 files, flags 1. HWRM "classic" archives use a 36-byte entry with an extra u32 before `compressionType`; GoK detects them by the magic count `numFiles == 13887` (`Game/BigFile.c:416-441`). Cataclysm does not need this. Verification pass, whole TOCs: both archives are sorted ascending by unsigned (CRC1, CRC2) with no duplicate keys, every entry has `0000` and `C9CACB` padding, `cataclysm.big` uses compression types 0 and 1 (391 of the first 400 entries are compressed), `update.big` only 1.

### 5.3 Name hashing, encryption and lookup

* Lookup (`bigTOCFileExists`, ORIG `Game/bigfile.c:387-414`, GoK `Game/BigFile.c:551-603`): lowercase the name, run `filenameSlashMassage` (all slashes become `\`, repeats collapse), then compute `CRC1 = crc32(name, len/2)` and `CRC2 = crc32(name+len/2, len/2)`. **Bug kept for compatibility:** for odd lengths the last character is never hashed. GoK retries with the corrected `len - len/2`. ORIG has only the buggy form. Search order: binary search when sorted (CRC64 compare macros `Game/BigFile.h:81-83`), else a linear scan that resumes from the previous hit (`:609-654`). The unsorted path also compares `nameLength`.
* CRC32: the standard reflected table (`0x77073096`...), init `0xFFFFFFFF`, final `~` (`ThirdParty/CRC/CRC32.c:91-104`; table in the BIG spec).
* Name encryption (`Game/BigFile.c:668-692`) is a chained XOR seeded with `0xD5` (213): `cipher[i] = plain[i] ^ plain[i-1]` with `plain[-1] = 0xD5`. Decrypt with `maskch=213; c ^= maskch; maskch = c`. The NUL is not encrypted. Names are used only by tools; the game resolves by CRC.
* Checked on GAME (verification pass): decrypting with this chain and re-hashing the lowercased name reproduces the stored CRCs (`update.big` entry 0 = `RACE_SECT\sHiveFrigate.shp`), and the byte after each name is a raw 0x00. For the 31-character `multiplayer\coldwar2\type1.dist` (`cataclysm.big` entry 1) CRC2 equals crc32 of the 15 characters after the midpoint, not 16. **The odd-length hashing bug is baked into the shipped archives**, so a port that looks names up must hash exactly like ORIG.

### 5.4 Compression

Nelson/Gailly LZSS (`ThirdParty/LZSS/LZSS.c:43-52,430-511`):
* `INDEX_BIT_COUNT 12` (4096-byte window), `LENGTH_BIT_COUNT 4`, `BREAK_EVEN 1`, so match length = field+2 (2..17).
* Bit stream is MSB-first (`BitIO.c` mask 0x80). A 1 flag bit means an 8-bit literal; a 0 means a 12-bit position plus a 4-bit length. Position 0 = end of stream.
* The window write cursor starts at 1. The `window[]` array is a static global and is **not cleared between files**. Encoder references are always to already-written bytes, so this is safe (UNVERIFIED for every file). Checked on GAME (verification pass): a decoder that starts every member with a zeroed window and cursor 1 expands all 39 `update.big` members and all 391 compressed members among `cataclysm.big` TOC entries 0-399 to exactly `realLength` bytes, and the text members come out clean. Matching lengths do not prove that no member reads stale window bytes, so the whole-archive claim stays UNVERIFIED.

Members are read with `bitioFileInputStart(bigFP)` and `lzssExpandFileToBuffer` directly from the archive (`Game/BigFile.c:2580-2673`). The tool falls back to storing uncompressed when ratio > 0.95 (`MINIMUM_COMPRESSION_RATIO`, `:44,1581`).

### 5.5 Archive precedence and loose-file overrides

**ORIG (and Cataclysm, see section 13).**
* `bigOpen(utyBigFilename, utyUpdateBigFilename)`: main archive mandatory, `Update.big` optional. Both headers must verify; both TOCs are read (`Game/bigfile.c:1943-1974`, called at `Win32/utility.c:3986`).
* On failure, `bigCheck()` chooses between "Incompatible file: %s" and "Unable to open file: %s" (5 languages each, `Win32/utility.c:3757-3771,3988-4002`).
* Then `mainNewerAvailable[]` and `updateNewerAvailable[]` are allocated, and **`CompareBigfiles = TRUE` in every ORIG build** (`Game/bigfile.c:45`). `bigFilesystemCompare(".", ...)`, or `HW_Data` if set, recursively walks the directory (`_findfirst`). For every loose file whose relative path matches a TOC entry it sets 2 if the disk `time_write` is newer than `timeStamp`, else 1 (`Game/bigfile.c:2093-2180`).
* `fileOpen`/`fileLoad` order (ORIG `Game/file.c:519-...`):
  1. `Update.big`, unless `CompareBigfiles && updateNewerAvailable[i] > 1`.
  2. Else main archive, unless its entry's `NewerAvailable > 1`.
  3. Else the disk via `filePathPrepend`.
* So **a loose file overrides an archived file only if its modification time is newer than the TOC timestamp**. `FF_IgnoreBIG` and `FF_CDROM` skip the archives; writes always go to disk.

**GoK.**
* `bigFilePrecedence[] = { "Update.big" (optional), "Homeworld.big" (required) }` (`Game/BigFile.c:72-91`), opened by `bigOpenAllBigFiles` (`:2505-2573`).
* `CompareBigfiles` is TRUE only in debug builds (`:54-60`).
* `bigFindFile` skips entries marked `LOCAL_FILE_IS_NEWER` (`:2869-2891`).
* GoK's `fileOpen` also uses any existing loose file under the override path regardless of age (`Game/File.c:1182-1197`). That is a behaviour change from ORIG.

### 5.6 CRCs fed to multiplayer version checks

* `bigTOCCRC` = crc32 over the in-memory `bigTOCFileEntry` array, padding bytes included (`Game/BigFile.c:467-470`). A port must keep the exact 32-byte layout read from disk to reproduce it. ORIG reads the TOC with one raw `fread` (`Game/bigfile.c:291-296`). For GOG build 12948, crc32 over the on-disk TOC bytes gives **0x617E5240** for `cataclysm.big` (16528 × 32 bytes) and **0xE86EEFA6** for `update.big` (39 × 32) (verification pass). ORIG would put these in `HomeworldCRC[1]` and `[2]`; that Cataclysm keeps this layout is UNVERIFIED. CET's `Update102.big` redirect changes the second value.
* ORIG `HomeworldCRC[0] = utyCodeCRCCompute()` = crc32 of the code between `linkStartCode` and `linkEndCode` (`Win32/utility.c:4033,5020-5023`; `Win32/LinkStart.c`, `LinkEnd.c`). `[1]` is the main TOC CRC, `[2]` the update TOC CRC (`bigCRC`, `Game/bigfile.c:2201-2205`), `[3]` is 0.
* A recompiled exe can never reproduce `[0]`. For network compatibility with the original Cataclysm, hard-code the GOG exe's value (UNVERIFIED that Cataclysm still sends it).
* GoK zeroes `[0]` and fills `[0..1]` from the TOCs (`SDL/utility.c:3794-3802`); its log labels are off by one (`:3854-3863`).

### 5.7 Tools

`tools/biggie` in GoK is the Relic `bigAdd`, `bigFastCreate`, `bigView`, `bigExtract` and `bigPatch` code compiled as a CLI (`tools/biggie/main.c:26-60`, "biggie 3.00"). Relic's binaries are in `SNAP: tools/bin/Biggie.exe`, `BigViewer.exe` and `BigTree.exe`. `tools/bigfile.py` in this repository was written by another agent this round (not reviewed here).

---

## 6. `.script` parser and tweak binding

* Line grammar (`parseLine`, ORIG `Game/statscript.c:678`, GoK `Game/StatScript.c:692-763`):
  * lines starting with `[`, `]` or empty are skipped;
  * trailing blanks and tabs are trimmed;
  * `;` and `//` start comments;
  * the name is the first token, ended by space, tab or `=`;
  * the value is the rest with leading blanks skipped and trailing spaces/CR/LF stripped.
  * Lines are at most `MAX_LINE_CHARS 650` (`:77`).
* Tables:
  * `scriptEntry {char *name; setVarCback setVarCB; void *dataPtr;}` binds to globals.
  * `scriptStructEntry {name, setVarCB, offset1, offset2}` binds to struct fields. `offset1 = &template.field` and `offset2 = &template` are both **absolute addresses** of a static template, and the field address is `structureToFillIn + (offset1 - offset2)` (`Game/StatScript.h:15-30`, `Game/StatScript.c:783-825`). In the binary these tables are pairs of pointers into one static struct.
  * Callback signature: `void cb(char *directory, char *field, void *dataToFillIn)`.
* `makeEntry(var, cb)` is `{ #var, cb, &var }` (GoK `Game/StatScript.h` + `Types.h:262 strdollar`; ORIG `Game/statscript.h:45` `{ str$(var),callback,&##var }`). **Every tweakable global's C identifier is a literal string beside its address in `.rdata`/`.data`.** That makes the tables the best way to name globals in the Cataclysm binary. Array elements keep the bracket text, for example `twRepairRate[CLASS_Mothership]` (present in `cataclysm.exe`).
* Table layout checked in `cataclysm.exe` (verification pass; a pointer search, not disassembly). `scriptEntry` rows are 12-byte `{char *name, cb, void *data}` triples in `.data`:

| entry (row address) | name string | cb | data | HW1 source |
|---|---|---|---|---|
| `HYPERSPACE_SLICE_RATE` (0x00894220) | 0x00894B9C | 0x005259D0 (`scriptSetReal32CB`, CET) | 0x0089410C (CET's address for the variable) | GoK `Game/SinglePlayer.c:265` (`SinglePlayerTweaks`) |
| `AUTOSTABILIZE_FRAME_KICK_IN` (0x008AC718) | 0x008B08CC | 0x00525BC0 | 0x008AC500 | `Game/Tweak.c:489`, `scriptSetUbyteCB` |
| `REFRESH_RENDERLIST_RATE` (0x008AD924) | 0x008AE23C | 0x00525C20 (CET calls it `scriptSetUdwordCB`; HW1 uses `scriptSetSdwordCB` here, `Game/Tweak.c:976`, so one of the two names is off, UNVERIFIED) | 0x008AC5A8 | `Game/Tweak.c:976` |


CET's `ShipStaticScriptTable` (0x008B2428, `Cataclysm.h:224`) is a 16-byte `scriptStructEntry` table. Its first rows are `LODFile`, `pMexData`, `mass` and `momentOfInertiaX/Y/Z`, all with `offset2 = 0x00B3B420` (the static template) and `offset1 - offset2` = 0, 4, 8, 0xC, 0x10, 0x14. So scanning `.data` for {pointer to string, pointer into `.text`, pointer into `.data`} triples recovers the name and address of every tweakable global.

* Drivers:
  * `scriptSet(dir, file, table)` opens through `fileOpen(FF_TextMode|FF_IgnorePrepend)`, so archives apply (`:844-886`).
  * `scriptSetStruct(...)` (`:783`).
  * `scriptSetFileSystem` uses `fopen("rt")` and never the archive; it is used for the cfg (`:896-935`).
  * Unknown names are ignored silently, and `globalScriptFileName` is set before each callback.
* Generic callbacks (`Game/StatScript.c:220-626`): RGB/RGBA, real32 (also `Sqr`, `ARRAY`), s/u byte/word/dword, `Bool` and `Bool8` (`TRUE`/`YES`/nonzero), `BitUdword`/`BitUword` (`BITn`), string, string pointer, cos/sin/tan/angle, gun type, bullet type, ship type/race/class, vector, LW→HW vector, formation, tactics, sphere declination.
* Global tweak load, `scriptSetTweakableGlobals` (ORIG `Game/statscript.c:1839`, GoK `:1856-1893`), runs in pre-init after the archives open. HW1 order: `AIShip, AITrack, Camera, CameraCommand, CommandLayer, Formation, Galaxy, Tweak, Flightman, Dock, Soundevent, Trail, Resource, Cloud, SinglePlayer, Nebulae, AIPlayer, NetTweak, AIResourceMan, UnitLimitCaps, Damage, ShipView, AutoLOD, GatherStatsFor, FEColour` `.script`, then `crateInit`, `paradeSetTweakables`. **Cataclysm has the same list without `Dock.script`**: the strings appear in reverse call order in `.data` (`AIShip.script` at 0x0089808C down to `FEColour.script` at 0x00897EE8), with `gametypes.script` just above them at 0x008980B0. CET hooks the Cataclysm function at `0x00527820` (`CET: Cataclysm.h:165`, `GameHooks.cpp:95-106`).
* `scriptEntry` tables in GoK: 52 file-scope tables (`grep -c '^scriptEntry [A-Za-z]*\[\]' Game/*.c`). They include Camera, CameraCommand, AIResourceMan, AIPlayer, AITrack, AutoLOD, ResearchAPI, Battle, Damage, CommandLayer, FlightMan, Nebulae, Crates, Teams, Formation, ShipView, AIShip, Tactical, Tweak (`Game/Tweak.c:467`, 402 `makeEntry` lines), Clouds, ETG, Ping, NIS, SinglePlayer (×5), LevelLoad (×2), Stats (×3), ResCollect, ScenPick (×5 languages) and PlugScreen. `gametypes.script` is parsed by hand in `mgGameTypeScriptInit` (`Game/StatScript.c:1920-1937`).

---

## 7. String tables and localization

* `MessageStrings[NumStrings]` (`char*`) is indexed by `enum strGamesMessages` (`Game/StringsOnly.h`, `NumStrings` at `:657`; the enum has **590** entries, `NumStrings` = 590, identical in ORIG and GoK; counted in the verification pass. The line number is not the count). `strGetString(id)` is a macro over that array.
* `LanguageStrings[]` is a `scriptEntry` table built with `strEntry(var) {#var, strSetStringCB, &MessageStrings[var]}` (ORIG `Game/Strings.c:28`, GoK `Game/StringSupport.c:18-670`). `strNameToEnum` returns the table index, so **table order must equal enum order** (`:869-881`).
* `strLoadLanguage(lang)` (ORIG `Game/Strings.c:720`, GoK `:721-795`):
  * fills every slot with `crapMessage = "*NO_STRING*"`;
  * reads the keyboard layout via `GetKeyboardLayout(0)` into `strCurKeyboardLanguage`;
  * loads `English.script`, `French.script`, `German.script`, `Spanish.script` or `Italian.script` with `scriptSet("", ...)` (so from the archive).
  * `strSetStringCB` duplicates each value with `memStringDupe` and expands `\n` (`:825-860`).
* Language enum: English 0, French 1, German 2, Spanish 3, Italian 4 (`Game/StringSupport.h:19-26`). It is saved as `language` in the cfg and as `HW_Language` in the registry. `PrepareVersionStringStuff` builds `networkVersion\tlanguage\tminorBuild` (`SDL/utility.c:3407-3425`).
* Other language-dependent paths:
  * fonts `fonts\<Lang>\` (`Game/FontReg.h:35-45`, `FontReg.c:152-170`);
  * NIS/animatic subpaths `French\`, `German\` (`Game/NIS.c:282-...`);
  * plug screens;
  * FE `.fib` atoms carry 5 NUL-separated strings and per-language hotkeys (`Game/FEFlow.c:695,2860`);
  * per-language arrays in code (CD messages, `spDescriptionTweaks*`);
  * keyboard translation `keyFrench/German/Spanish/ItalianToEnglish` (`Game/Key.c:364-406`).
* Cataclysm: `CET: assets/english.script` (the shipped Cataclysm table repacked by CET) has **1693 keys (lines starting `str`), 1329 of which are not in HW1's 590-entry enum, while 226 HW1 names are absent** (verification recount; the original count said 1694/1330), using new prefixes such as `strsRecon` (Somtaaw) and `strbRecon` (Beast). The CET tweak.script also adds `g_bNeedMoreSpaceForOtherLanguage` and `g_bNeedReverseWordsForOtherLanguage` ("the code doesn't know what language is being used any more"). `strDefaultPlayerName`, `strVersion`, `strsRecon` and `strbRecon` are present as literals in `cataclysm.exe`.

---

## 8. Save / load

* File: ORIG opens `fileOpen(name, FF_WriteMode|FF_ReturnNULLOnFail)` relative to the prepend path (`Game/SaveGame.c:598-602`). GoK opens the same with `FF_UserSettingsPath` (`:675-684`). Directories: `SavedGames\SinglePlayer\`, `SavedGames\MultiPlayer\`, `SavedGames\RecordedGames\` and `SavedGames\Training\` (ORIG `Game/gamepick.c:64-68`; all four are present in `cataclysm.exe` and as folders in `original/game/SavedGames`).
* Format: first a raw `sdword` version, then a stream of chunks `SaveChunk {sdword type; sdword contentsSize;}` followed by the contents (`Game/SaveGame.h:65-81`). The type is a structure kind (`BASIC_STRUCTURE 0x80000000`, `VARIABLE_STRUCTURE 0x40000000`, `INFO_CHUNK 0x20000000`, `LINKEDLISTSPACEOBJS`, `AIPLAYER`) ORed with a `SAVE_*` subtype and, for space objects, `OBJ_*Type` in the low nibble (`:24-63`). `INFO_CHUNK` holds a single `sdword` (`SaveInfoNumber`/`LoadInfoNumber`, `Game/SaveGame.c:402-443`).
* Version: HW1 `0x10ad0027` (ORIG `Game/SaveGame.h:13`). GoK writes `0x10ad0029` and reads 0x28/0x29 (`:16-22`, `Game/SaveGame.c:62-65`). The dword `0x10ad0027` occurs 3 times in `cataclysm.exe` (save, load and verify use it), so **Cataclysm probably kept the HW1 version number** (UNVERIFIED: the uses are not disassembled).
* Top-level order (identical in ORIG `Game/SaveGame.c:598-680` and GoK `:675-758`):
  1. `SaveVersionInfo`
  2. `SavePreGameInfo`: SP flag, music track, `numPlayers`, `playerNames`, per player race and colours, mission number or `tpGameCreated`, `universeSaveEverythingNeeded`
  3. `SpaceObjRegistryInit` + `BlobRegistryInit`, then `RegisterAllSpaceObjs/Blobs`
  4. `SaveAllSpaceObjs`
  5. `SaveAllBlobs`
  6. `SaveUniverse`
  7. 4× `SaveIDToPtrTable` (Ship, Resource, Derelict, Missile)
  8. `nebSave_Nebula`
  9. `SaveConsMgr`
  10. `aiplayerSave`
  11. `SaveSinglePlayerGame`
  12. if SP: `kasSave`, `objectiveSave`, `tmSave`, `pingSave`, `CurrentLevelName`
  13. tutorial game
  14. `selSelected` + hot-key groups
  15. background
  16. lighting
  17. `smSave`
  18. `ranSave`
  19. `actorFlagsEnabled`, `smGhostMode`
  20. `SaveConsMgrDetermOptional` ("added for V1.04 patch")
  21. On write error the file is deleted.
  Load mirrors this and then runs `FixAllSpaceObjs/FixAllBlobs` (`Game/SaveGame.c:813-873`). `PreLoadGame` reads the version and pre-game info before level load (`:792-804`).
* **Pointer fix-up.** Objects are saved as **raw struct images** (`CreateChunk(..., ship)` copies `sizeof(Ship)` plus ship-specific data plus mad bindings, `Game/SaveGame.c:1704-1731`). Each pointer field in the copy is overwritten with an ID:
  * `SpaceObjRegistryGetID(obj)` = index in a `GrowSelection` registry (`:101-228`);
  * `BlobRegistryGetID`;
  * player pointer → `playerIndex` (`SavePlayerToPlayerIndex`);
  * list members → `ConvertPointerInListToNum` (position in a `LinkedList`);
  * `staticinfo` → NULL, restored from type and race;
  * `-1` means NULL.
  After all objects load, `Fix*()` converts the IDs back (`FixShip` `:2034-2116`). Strings use `Save_String/Load_String`, and linked lists use `SaveLinkedListOfStuff` + callbacks.
* **Consequences for a port.**
  * The format is defined by the 32-bit MSVC struct layout of `Ship`, `Universe` and the other saved structs.
  * To load original Cataclysm saves, the port needs either 32-bit, `-Zp4`, layout-identical structs or an explicit converter. GoK does not load ORIG saves (`supportedVersionNumbers` lacks 0x27).
  * CET found a Cataclysm bug: after `LoadPreGameInfo` the game zeroes the `SongNumber` through an offset table at `0x00892670` (`CET: PatchSaveGame.cpp`).

---

## 9. Demo record/playback and packet recording

### 9.1 Demos (`Game/Demo.c/.h`)

* File: `filePathPrepend(name, FF_UserSettingsPath)` in GoK. The name gets `"%ux%u.dem"` (resolution) appended (`SDL/utility.c:4207,4216`). ORIG uses `"%dx%d.dem"` (`Win32/utility.c:4397,4406`, live code in `utyGameSystemsInit`), and Cataclysm has the same `%dx%d.dem` string (GAME). It is inherited, not a Cataclysm variant.
* Header `demoheader {char ident[6] = "BaNng"; uword version = 0x007; real32 startTime; sdword initialStateSize; ranstream randyState[RANDOM_NUMBER_STREAMS];}` (`Game/Demo.h:50-78`), followed by `initialStateSize` bytes from `utyPreDemoStateSaveCB` (`SDL/utility.c:2241-2355`).
* Stream (all written with `demBlockWrite`):
  * per `taskExecuteAllPending` call: an `sdword nTicks` (`demNumberTicksSave`, `Game/Task.c:297-304`);
  * per region-task frame: a `demostate {time, [checksums], sword mouseX/Y, udword frameTicks, ubyte mouseButtons, ubyte taskNumber, pad, ubyte nKeys}` followed by `nKeys × demokey {ubyte index; keyScanType key;}` (`Game/Demo.h:81-115`, `Game/Demo.c:205-288`).
  * Hooks: `Game/Region.c:924-932` freezes `keyScanCode` and calls `demStateSave` or `demStateLoad`.
* Playback replays the recorded tick counts, so the whole task schedule is deterministic.
  * "Fake renders" skip real rendering when playback runs slower than the recording (`demNumberTicksLoad` `:579-629`, `DEM_MaxFakeRenders 6`).
  * Recording forces `determCompPlayer = TRUE`.
  * `keyScanType` index is a `ubyte`, so HW1 VK codes fit but GoK's SDL scancodes (up to 520) do not.
* The demo checksum fields (universe checksum, random streams, camera CRC, selection counts) exist only in debug builds, so **demo files from debug and release builds are incompatible**.
* HW1 retail could not start demos at all: the `/demoRecord` and `/demoPlay` table entries are commented out in ORIG (`Win32/main.c:1321-1325`) and the auto-demo (`DEM_AUTO_DEMO`) is 0 (`Game/demo.h:20`). Only `EnableDemoRecord/EnableDemoPlayback` (`Win32/main.c:822-841`, now unreferenced) set `demDemoRecording/Playing`. The start code in `utyGameSystemsInit` (`Win32/utility.c:4395-4417`) is still compiled. Cataclysm is in the same state: no demo command-line entries, but `BaNng`, `C:\Cataclysm\src\game\demo.c` (0x00879274, right after `BaNng`) and `%dx%d.dem` are present, so the code is linked in. Whether anything can reach it is UNVERIFIED.

### 9.2 Packet recording (multiplayer replays)

`/packetRecord` and `/packetPlay <file>` plus the in-game "save recorded game" path: `recPack*` in `Game/NetCheck.c:527-710`, extension `.pkts` (`Game/NetCheck.h:16`), folder `SavedGames\RecordedGames\`. During playback `universeUpdateTask` takes the MP branch (`playPackets|recordFakeSendPackets`) and turbo is allowed (section 2.3). Cataclysm also has `/LoadMPGame` and `/SaveMPGames` (section 1.2).

---

## 10. Debug output, DebugWindow and logging

* ORIG `dbgMessage(s)` calls `dbwPrint(0, s)` (debug window pane 0). `dbwPrint` appends to `debugMessages.txt` when `debugToFile` (`/debugToFile`) is set (ORIG `Win32/debugwnd.c:743-796`, `debugwnd.h:13-14`). The window is a separate Win32 window, class `HW_DebugWindow`, title `Homeworld Debug Window`, 4 panes, with its geometry saved in the cfg (`debugwnd.h:30-56`). It is started only with `/debug` (`Win32/main.c:3380-3393`). GoK prints to stdout and disables the window (`Game/Debug.c:105-114`).
* Formats:

| call | ORIG format | GoK format |
|---|---|---|
| `dbgWarning` | `"\n%s (%d): Warning- %s"` | `"\n%s (%d): Warning - %s"` |
| `dbgFatal` | `"\n%s (%d): Fatal error - %s"` (`Game/Debug.c:122`) | same (`Game/Debug.c:195`) |
| `dbgFatalf` | `"\n%s (%d): Fatal Error - %s"` (capital E, `Game/Debug.c:158`) | delegates to `dbgFatal`, so there is no separate string |
| `dbgNonFatal` | | `"...Non-fatal error - %s"` |

  `DBG_Loc` is `NULL, 0` in release builds.
* `dbgFatal` (ORIG `Game/Debug.c:122-138`): formats into `dbgFatalErrorString`, executes `int 3` if `dbgInt3Enabled` (`/nodebugInt` clears it), then `utyFatalErrorWaitLoop(0xfed5)`. That function does `OutputDebugString`, sends `WM_COMMAND CID_ExitError`, which makes the window proc show `MessageBox(..., "Fatal Error")`, and calls `exit(0xfed5)` (`Win32/utility.c:1655-1676`). Non-fatal errors show `"Non-Fatal Error (cancel to debug)"`. A debug build also writes `stack-<asctime>.dump` (`DBG_STACK_CONTEXT`).
* Cataclysm:
  * `dbgInt3Enabled` at `0x0087922C` = 1 in the file, so release asserts trap. CET clears it unless a debugger is attached (`CET: GameHooks.cpp:151-156`).
  * `cataclysm.exe` contains both `"%s (%d): Fatal error - %s"` (ORIG `dbgFatal`) and `"%s (%d): Fatal Error - %s"` (ORIG `dbgFatalf`, `Game/Debug.c:158`; the capital-E variant is **inherited from HW1, not new**, and only GoK merged the two), plus the window title `Cataclysm Debug Window`, `HW_DebugWindow` and `debugMessages.txt`.
  * Fourteen `C:\Cataclysm\src\game\*.c` / `C:\Cataclysm\src\win32\*` `__FILE__` strings survive in `.data`: `btg.c`, `demo.c`, `eval.c`, `FeFlow.c`, `mex.c`, `particle.c`, `soundevent.c`, `speechevent.c`, `ClientCDKey.cpp`, `font.c`, `render.c`, `texreg.c`, `TitanInterface.cpp`, `TitanPacketMsg.cpp`. The source tree had the same `src\game` and `src\win32` layout as HW1. Their code cross-references give file anchors (see `docs/binary-fingerprint.md`).
* Other logs: `FileLoads.log` (`/logFileLoads`, `Game/File.h:66`), `open.log`, `loadfree.log` (`UNIV_LOAD_FREE_FILENAME`), `demolog.txt`, network logs via `/logFilePath`, `d3dDeviceCRC.txt`, `mem.nv` (debug).

---

## 11. Key and option tables

* Key state (`Game/Key.c/.h`):
  * `volatile keyScanType keyScanCode[KEY_TOTAL_KEYS]`, where `keyScanType` is the bitfield `{ubyte keypressed:1; ubyte keystick:2; sbyte keynumpressed:5}`;
  * `keySaveScan[]` for demos;
  * a 16-entry typed-key ring buffer (`KEY_BufferLength`).
  * ORIG `KEY_TOTAL_KEYS 256` with **Windows VK codes** as indices (`ESCKEY 0x1b`, `AKEY 'A'`) and mouse pseudo-keys `LMOUSE_BUTTON 1`, `RMOUSE_BUTTON 2`, `FLYWHEEL_UP 3`, `MMOUSE_BUTTON 4`, `FLYWHEEL_DOWN 5`, `LMOUSE_DOUBLE 6`, `RMOUSE_DOUBLE 7`, `MMOUSE_DOUBLE 0xf8` (ORIG `Game/KEY.H:26-147`; pseudo-keys at `:127-136`).
  * GoK uses SDL scancodes plus 8 pseudo-keys (`Game/Key.h:29,144-166`).
  * `keyPressDown/Up/Repeat` are called from the window procedure.
  * Non-English layouts are translated through `keyXxxToEnglish` (GoK `SDL/main.c:1623-1678`).
* HW1 key bindings (`Game/KeyBindings.c/.h`):
  * `enum CommandsToBind` has 19 commands: `kbNEXT_FORMATION ... kbLAUNCH_MANAGER` (`KeyBindings.h:25-50`).
  * `kbBoundKeys {command, defaultkey, primarykey, resettokey, status}` with five default tables, `kbKeyTableEnglish/French/German/Spanish/Italian` (`KeyBindings.c:54-174`).
  * Overrides are saved as `kbKeySavedKeys[20]` through cfg names `NEXT_FORMATION`... (`SDL/utility.c:708-727`), so **cfg values are platform key codes**.
  * Key names for the UI come from `KeyNames[] {keyindex, "A    "}` (5-char padded, `Game/Options.c:268-560`).
  * `opKeymap`/`KeyTranslations` form a hex-encoded remap string (`Options.c:1320-1336`).
* Options module (`Game/Options.c`): `op*` FE callbacks listed in `utyCallbacks[]` (`SDL/utility.c:431-608`) with draw callbacks (`:610-620`). `opTimerActive/opTimerLength=22 s` is the video-mode confirm countdown polled by the main loop (`Options.c:50-52,1147-1187`). `opUpdateSettings` applies loaded values (`:573`).
* Cataclysm: a new **data-driven binding system**.
  * `original/game/default_keys.cfg` (`v 84`, 197 `key <Name> <VK>` lines with `SHIFT+` modifiers) covers gameplay (`Move`, `Dock`, `Retire`, `Waypoints`, `Upgrade_Ships`, `Enter_Slipgate`, `Leech_*`, `Select_Mimic_*`, `Force_Field_*`, `Cloak`, `Launch_Swarmers`, `Big_Gun_Attack`, `EMP`, `Ram`, `Repulse`, `Fire_Missiles`, tactics, formations including `Formation_Delta3D`, `Formation_Parade`), camera, groups 1-9, time (`Accelerate_Time`, `Decelerate_Time`, `Toggle_Game_Speed`), plus about 50 developer bindings (`Open_Console`, `Real_Time_Tweak`, `Kas_Dump`, `Toggle_Draw_KAS_*`, `Profiler_Next`, `SingleStep`, `Play_NIS*`).
  * Values are VK codes, for example `Cancel_Orders 192` = VK_OEM_3.
  * The HW1 19-slot `kbKeySavedKeys` cfg names are still present in the exe (`NEXT_FORMATION` was found). The relationship between the two systems is UNVERIFIED.
  * New FE callback names in `cataclysm.exe` include `OP_CataclysmFramerate`, `OP_LODDetail`, `OP_Texture_Detail`, `OP_HardwareTransformLighting`, `OP_CataclysmDefault`, `OP_HomeworldClassic`, `OP_SmallFonts`, `OP_AutoFocusOnWaypoints`, `OP_SpecialActionBeforeAttack` and `SP_GameDifficulty`.

---

## 12. Module dependency graph (directory level)

Computed from `#include "..."` in GoK `src/` (each (file, header) pair counted once; headers resolved to their directory):

```
            Game (140 .c)  --1639-->  Game
            Game           --276--->  SDL        (render.h, main.h, utility.h, mouse.h, texreg.h, ...)
            Game           --55---->  Ships
            Game           --6/3/3/2> ThirdParty/LZSS, CRC, Titan, stb
            SDL (31 .c + 3 .cpp) --362--->  Game
            SDL            --135--->  SDL
            SDL            --7----->  Ships
            Ships (40 .c)  --202--->  Game
            Ships          --80---->  Ships
            Ships          --5----->  SDL
```
Game and the platform directory depend on each other in both directions: the platform layer is not a clean bottom layer. `utility.c` includes about 90 game headers (`SDL/utility.c:7-104`), and game code calls `utySet`, `mainWindow*` and `rnd*`. ORIG has the same shape (`SNAP: src/Win32/Release.depend` lists `..\Game\*.h` and `..\Win32\*.h` for every object, for example `advancesupportfrigate.obj` depends on `..\Win32\main.h`, `titaninterfacec.h`, `trails.h`).

Core module fan-in (number of GoK `.c` files including the header; a verification recount over `Game/ SDL/ Ships/*.c` lands within ±1-2 of these, depending on whether `SDL/utility_mac.c` is counted): `Debug.h` 97, `Memory.h` 97, `Universe.h` 89, `StatScript.h` 54, `File.h` 50, `Tweak.h` 40, `utility.h` 40, `StringSupport.h` 37, `SaveGame.h` 34, `main.h` 30, `Task.h` 18, `Types.h` 17, `Key.h` 14, `Demo.h` 11, `KeyBindings.h` 6, `BigFile.h` 3.

Core module includes (GoK):

| file | includes |
|---|---|
| `Game/Task.c` | Debug, Memory, Demo, TitanInterfaceC, Task |
| `Game/Memory.c` | Types, Debug, Key, File, Memory, CRC32, Randy |
| `Game/File.c` | BitIO, Debug, File, LZSS, Memory |
| `Game/BigFile.c` | BigFile, BitIO, LZSS, standard_library, Memory, Debug, File |
| `Game/StatScript.c` | StatScript, Color, Crates, Debug, FastMath, File, Formation, Memory, MEX, Mothership, MultiplayerGame, SpaceObj, Tactics, texreg, Tweak |
| `Game/Tweak.c` | Demo, Tweak, StatScript, Blobs, MultiplayerGame, HorseRace |
| `Game/StringSupport.c` | StringSupport, Memory, StatScript |
| `Game/SaveGame.c` | about 33 game headers (all subsystems with save functions) |
| `Game/Demo.c` | File, Memory, Debug, Region, mouse, UnivUpdate, Camera, mainrgn, Select, render, NetCheck, Demo |
| `Game/Debug.c` | Debug, File, utility |
| `Game/Key.c` | Debug, Demo, Task, Key, Options, ConsMgr |
| `SDL/debugwnd.c` | Types, main, Task, File, debugwnd |

Suggested port layering (not how the original is built): `platform` (window, input, timer, file roots, threads) → `core` (Memory, File, BigFile, LZSS, CRC32, Task, Debug, StatScript, Strings) → `game`. The core files above only touch `utility`/`main` through `utyFatalErrorWaitLoop`, `taskTimeElapsed` and a few globals, so they can be separated with a thin shim.

---

## 13. What Cataclysm most likely changed here

Evidence levels: **CET** (address-level reverse engineering in `reference/CataclysmEngineTweaks`), **GAME** (byte/string read of `original/game`, no disassembly) and **DOC** (shipped text files).

| area | HW1 | Cataclysm | evidence |
|---|---|---|---|
| universe rate | 16 Hz, `UNIVERSE_UPDATE_PERIOD` | **still 16 Hz.** `push 0x3D800000` (0.0625f) at 0x0053E8A5 and 0x00524841 (univUpdate calls), 0x005AC174 (`taskStart(universeUpdateTask, ...)` in utyGameSystemsInit), 0x004CC8F2 (nisStartup) and 0x0052AF12 (baby task start); `mov [0x00B62ABC], 0x3D800000` at 0x0054ACDB (univupdateReset). CET patches 5 univUpdate push sites, and a byte scan finds exactly 5 `E8` calls to `univUpdate` 0x0054C3F0 (3 inside `universeUpdateTask`), against 4 live call sites in an ORIG release build (section 2.5 A) | CET `Cataclysm.h:40-66`; GAME bytes read at those VAs |
| timer | `UTY_TimerResloutionMax 240` int | `240.0f` float in `.rdata` 0x0072A614 plus `push 0xF0` at 0x005ABF68. 0x0072A614 is read by 8 `fmul dword [0x0072A614]` (`D8 0D`) sites (0x0042EDA9, 0x0042F22F, 0x0042F485, 0x0042F6C2, 0x0042F8C0, 0x0052EE43, 0x0052EE4F, 0x0058A019). ORIG has no float use of `UTY_TimerResloutionMax`, and MSVC pools identical float literals, so some of these may be unrelated 240.0f constants that CET's overwrite (`PatchUniverse.cpp:448`) also changes (UNVERIFIED); `utyTimerDivisor` is a `LONGLONG` at 0x00AB7520; `taskFrequency` float at 0x00BB3260 | CET `Cataclysm.h:71,190,212,218`; GAME |
| real-time clock | none in `Universe` | new `universe.realtimeelapsed` (+0xC4) driven by `QueryPerformanceCounter`, with a remainder `LARGE_INTEGER` at 0x00A6D9A0. It stops advancing after long PC uptimes (float precision); CET rebases QPC to fix it | CET `PatchUniverse.cpp:291-325`, `game/Game/universe.h` |
| per-update constants | `ETG_UpdateRoundOff = PERIOD/2` float | `.rdata` 0x0072A82C = -16.0f (ETG effector CI rate). At 0x0072A818 the bytes `00 00 00 00 00 00 A0 3F` are the **double** 0.03125. CET writes that address as a float, which only overwrites the low dword. Its only code reference is `DD 05 18 A8 72 00` (`fld qword [0x0072A818]`) at 0x0046E037, so the game reads it as a double. The CET write therefore barely changes the value: a likely CET bug (byte pattern, not disassembled). 0x0072A810 = 0.015625f | CET `Cataclysm.h:192-194`; GAME |
| hyperspace | `HYPERSPACE_SLICE_RATE` | engine default 0.075 at 0x0089410C, script value 0.025 | CET `PatchUniverse.cpp:20-24`; GAME |
| tasks | unnamed `taskStart(fn, period, flags)`; asm context switch | tasks are **named** (`UnivUpdateTask`, `RenderTask`, `CaptServTask`, `RegionProcessTask`, `NisUpdateTask`, `PingTask`, `PlugRenderTask`, `MultiplayerGameTask`, `MultiplayerLanTask`, `BabyTasks`). Push order at 0x005AC173 implies `taskStart(name, fn, period, flags)` with `taskStart` = 0x0052A5F0. It has exactly 10 `E8` callers, one per name, with HW1's periods and flags (table in section 3.3). The utyGameSystemsInit call order around it matches HW1. The scheduler internals are UNVERIFIED | GAME strings and bytes |
| render task | no frame cap | **built-in frame-rate limiter** in the render task (CET NOPs 8 bytes at 0x0057B78B and 0x0057B7A1) plus a new FE callback `OP_CataclysmFramerate` | CET `PatchRGL.cpp:96-100`, `CataclysmEngineTweaks.ini` `DisableFrameRateLimit`; GAME |
| time compression | debug or replay-only turbo | user-facing 8x time compression (`Accelerate_Time`, `Decelerate_Time`, `Toggle_Game_Speed`) | DOC `default_keys.cfg`; `HWSDL: documentation/HWSDL_enhancements.txt` ("8 times faster", same as Cataclysm) |
| data files | `Homeworld.big` + `Update.big`, `HW_Music.wxd`, `HW_comp.vce` | `Cataclysm.big` + `Update.big` (opened at 0x005ABB21 as `push "Update.big"; push "Cataclysm.big"; call` = **ORIG two-argument `bigOpen(main, update)`**, followed on failure by `push "Update.big"; call; cmp eax,-1` and a `[strCurLanguage*4 + table]` message lookup. That instruction shape matches ORIG `bigCheck` / `utyIncompatibleBigMessages` at `Win32/utility.c:3986-4002`), `Music4Cat.wxd`, `BeastSpeech.vce`, `SectSpeech.vce`. CET redirects to `Update102.big` | GAME bytes and strings; CET `GameHooks.cpp:12,108-116`, `Cataclysm.h:118-121` |
| `.big` format | RBF1.23, 32-byte TOC entries, sorted, LZSS, XOR names | **identical** (headers and first entries verified, padding `C9CACB`) | GAME |
| config | `Homeworld.cfg` in CWD + HKLM `...\Homeworld` | `Cataclysm.cfg` + `SOFTWARE\Sierra On-Line\Cataclysm`. Same `regOptionsList` incl. `D657E436967616D4`; cfg adds `enableRepairbots`, `enableLensFlare` and `enableCPV`, drops `noPalMB` | GAME strings |
| key bindings | 19 fixed commands, cfg slots | 197-command `default_keys.cfg` (`v 84`) including developer bindings | DOC |
| command line | ORIG table | same table (incl. `/prepath`, `/d3dDeviceCRC`, `/smCentreCamera`); new `/noCPV`, `/LoadMPGame` and `/SaveMPGames`; release table lacks `/ignoreBigfiles`, `/logFileLoads`, `/NoBind`, `/noWon`, `/forceLAN`, `/noAuth`, `/shortWon`, `/pilotView`, `/noPlug` | GAME strings |
| version strings | `networkVersion[16] "HomeworldV1C"`, `minorBuildVersion "06.1"` | at 0x008B8850: `"Emergence 1"`, at 0x008B8860: `"01"` (16 bytes apart, matching `MAX_NETWORKVERSION_STRING_LEN 16`). CET overwrites them with `v1.02`/`0` | CET `Cataclysm.h:243-244`, `GameHooks.cpp:9-10,135-136`; GAME |
| main.c globals | `MAIN_WindowWidth/Height/Depth = 640/480/16`, ... | same defaults at 0x008B85E8/EC/F0; `enableAVI`, `showBorder`, `fullScreen`, `glToSelect="rgl.dll"` and `networkVersion` sit in the same relative order as in ORIG `Win32/main.c:149-285` | CET `Cataclysm.h:202-230`; GAME dword reads |
| tweak scripts | HW1 `Tweaks[]` | same mechanism; `Dock.script` dropped from the global list; new tweak names `g_bNeedMoreSpaceForOtherLanguage`, `twRepairRate[CLASS_*]`, `twSalvageDamageLimit[CLASS_*]`, `twSalTransMaxTime/Speed`, `twRetireSalvageDistanceBelowDepot(Tolerance)`, `twnLipsBaseOffset`, `twCrystalProcessingTime`, `twProcessorAutoRepair*`, `twNotifySalvagersNeededRate`, `twNotifySalvageTargetTooStrongRate`; new classes `CLASS_Composite/Component/Micro/MonsterShip` | `CET: assets/tweak.script` compared with GoK `Tweak.c`; GAME |
| strings | 590 enum entries (`NumStrings`) | 1693 keys (1329 not in HW1's enum) | `CET: assets/english.script`; verification recount |
| save game | version 0x10ad0027, raw structs | 0x10ad0027 still present (3 uses); `Player` and `Universe` larger (`supportUnitsMax/Used`, extra fields, `unk3[0x1F8]`); song-number zeroing bug | GAME dword search; CET `game/Game/universe.h`, `PatchSaveGame.cpp` |
| memory | cookie allocator, fill patterns | `MBF_VerifyValue` present; fill constants absent (the volatile-clearing variables are probably gone, UNVERIFIED) | GAME dword search |
| debug | `dbgInt3Enabled=TRUE`, "Fatal error" (`dbgFatal`) and "Fatal Error" (`dbgFatalf`, ORIG `Game/Debug.c:158`) | same, both formats kept (inherited, not new); window title `Cataclysm Debug Window`; 14 `C:\Cataclysm\src\...` `__FILE__` strings | CET `Cataclysm.h:246`; GAME |
| renderer start | device-CRC check forces software | same logic at 0x00563563 (CET replaces it to default to OpenGL); `ActivateMe` reinitialises the renderer on alt-tab (0x00562084) | CET `PatchRGL.cpp:29-94`, `Cataclysm.h:96-98` |
| intro movies | `Movies\sierra.bik`, `Movies\relicintro.bik` | `Movies\sierra.bik`, `Movies\bdogintro.bik` | GAME |
| toolchain | Intel C (release), VC5 link | MSVC 6 link, **debug CRT** linked (`dbgheap.c`, `dbgrpt.c`, `Microsoft Visual C++ Debug Library`); CET's `memAlloc/memFree` 0x006725D0/0x00673080 are CRT `malloc/free` (section 4) | `docs/binary-fingerprint.md`; GAME strings and bytes |

Implications for the rebuild:
1. Reproduce the 16 Hz universe exactly: 240-tick timer, 15 ticks per universe update, `TSK_MaxTicks` 8, MP 1/8 s packets × 2.
2. Treat a higher update rate as an opt-in feature implemented the GoK/CET way (factor-gated masks plus rescaled data). Audit sections 2.5 A-E.
3. Port the ORIG two-archive `bigOpen`/`NewerAvailable` logic, not GoK's precedence array.
4. Keep raw-struct save compatibility behind a converter.

---

## 14. Strings and constants to search for in the Cataclysm binary

All entries come from the ORIG/GoK files named. "Cata" is the result of an exact ASCII search (NUL-terminated) in `original/game/cataclysm.exe`: **Y** present, **N** absent, **var** present in a Cataclysm-specific variant. N entries are still useful: they mark debug-only code the retail build compiled out.

### 14.1 File names and paths
| string | source | Cata |
|---|---|---|
| `Update.big` | ORIG `Win32/utility.c:412` | Y |
| `Homeworld.big` → `Cataclysm.big` | `:386` | var |
| `Extra.big` | `:413` | Y |
| `AutoLock.txt` | `:414` | Y |
| `Homeworld.cfg` → `Cataclysm.cfg` | ORIG `Win32/debugwnd.h:55` | var |
| `HW_Music.wxd` → `Music4Cat.wxd` | `Win32/utility.c:399` | var |
| `HW_comp.vce` → `BeastSpeech.vce`, `SectSpeech.vce` | `:408` | var |
| `FileLoads.log` | GoK `Game/File.h:66` | Y |
| `debugMessages.txt` | ORIG `Win32/debugwnd.h:14` | Y |
| `d3dDeviceCRC.txt` | ORIG `Win32/main.c:1203` (help) | Y |
| `SavedGames\SinglePlayer\` (also MultiPlayer, RecordedGames, Training) | ORIG `Game/gamepick.c:64-68` | Y |
| `FEMan\Front_end.fib` (and the other `utyFrontEndFiles`) | GoK `SDL/utility.c:226-238` | Y |
| `Main_game_screen` | `SDL/utility.c:4191` | Y |
| `fonts\`, `English\`, `French\` | GoK `Game/FontReg.h:35-38` | Y |
| `Movies\sierra.bik` (`Movies\relicintro.bik` → `Movies\bdogintro.bik`) | ORIG `Win32/utility.c:4167,4171` | Y (var) |
| `%dx%d.dem` (GoK changed it to `%ux%u.dem`) | ORIG `Win32/utility.c:4397`; GoK `SDL/utility.c:4207` | Y (same as ORIG) |
| `rgl.dll`, `rgld3d.dll`, `rglsw.dll`, `opengl32.dll`, `3dfxvgl.dll` | ORIG `Win32/main.c:218,1919-1923` | Y |

### 14.2 Format magic and registry
| string | source | Cata |
|---|---|---|
| `RBF`, `1.23` (separate literals, joined with `%s%s`) | `Game/BigFile.h:67,76` | Y |
| `BaNng` (demo ident) | `Game/Demo.h:50` | Y |
| `SOFTWARE\Sierra On-Line\Homeworld` → `...\Cataclysm` | ORIG `Win32/regkey.h` | var |
| `CmdLine` | ORIG `Win32/main.c:28,36` | Y |
| `deviceCRC`, `deviceCaps`, `deviceCaps2`, `deviceIndex`, `deviceToSelect`, `glToSelect`, `d3dToSelect`, `screenWidth`, `screenHeight`, `screenDepth`, `HW_Language`, `D657E436967616D4` | ORIG `Win32/utility.c:44-58` | Y (all) |
| `HW_Data`, `HW_Root` | ORIG `Win32/utility.c:3814`, `main.c:1873` | Y |
| `LSMap` | ORIG `Win32/main.c:3298` | Y |
| `CloseHomeworld` → `CloseCataclysm` | `:3331` | var |
| `Program already running.` (wrapped in `\r\n`) | `:3310` | Y |
| `HW_DebugWindow`; `Homeworld Debug Window` → `Cataclysm Debug Window` | ORIG `Win32/debugwnd.h:42-43` | Y / var |
| `SOFTWARE\Classes\http\shell\open\command` | ORIG `Win32/utility.c:190` | Y |

### 14.3 Error and log messages
| string | source | Cata |
|---|---|---|
| `Invalid or unrecognised command line option: '%s'\r\n` | ORIG `Win32/main.c:1430` | Y |
| `Command-Line Usage`, `Cannot allocate memory for help.` | `:1425` | Y |
| `%s (%d): Fatal error - %s` (with leading `\n`) | ORIG `Game/Debug.c:122` | Y |
| `%s (%d): Fatal Error - %s` (with leading `\n`, `dbgFatalf`) | ORIG `Game/Debug.c:158` | Y |
| `Fatal Error` (MessageBox caption) | ORIG `Game/Debug.h:47` | Y |
| `Error creating window` | ORIG `Win32/main.c` `ersWindowInit` | Y |
| `fileOpen: too many files open - cannot open file %s` | GoK `Game/File.c:1177` | Y |
| `fileOpen: cannot open file %s` | `:1364` | Y |
| `fileLoadAlloc: couldn't allocate %d bytes for %s` | `:887` | Y |
| `fileLoadAlloc: coundn't open file %s` (typo in source) | `:892` | Y |
| `bigFileLoadAlloc: couldn't allocate %d bytes for %s` | `Game/BigFile.c:2612` | Y |
| `memAlloc: failed to allocate %d bytes.` (release variant) | `Game/Memory.c:1313` | Y |
| `Exceeded %d growth heaps, each of length at least %d.` | `:1148` | Y |
| `Could not allocate growth heap %d with a size of %d` | `:1158` | Y |
| `Unable to query system performance timer.` | ORIG `Win32/utility.c:4097` | Y |
| `System performance timer runs at %d Hz.  We need at least %d Hz.` | `:4102` | Y |
| `Error allocating heap of size %d` | `:3972` | Y |
| `Error starting memory manager with heap size %d at 0x%x` | `:3978` | Y |
| `Unable to start the region processor.`, `Unable to start mouse.`, `Unable to start front end.` | `:4125,4203,4258` | Y |
| `Big files must be on for this feature`, `WON stuff saved`, `WON stuff save error` | `:4051-4058` | Y |
| `Incompatible file: %s`, `Unable to open file: %s` | `:3759,3767` | Y |
| `Homeworld CD not detected.` → `Cataclysm CD not detected.` | ORIG `Win32/utility.c:1719` (gameStart), `:3853` (pre-init, `MAIN_CDCheck`) | var |
| `\nScanning for newer files in %s` | ORIG `Game/bigfile.c:2107` | Y |
| `\nCompared %d filesystem files to main & update bigfiles.` | ORIG `Game/bigfile.c` | Y |
| `Error %d loading game %s` | GoK `Game/SaveGame.c:800` | Y |
| `Saving Gas clouds not supported yet` | `:2600` | Y |
| `*NO_STRING*` | ORIG `Game/Strings.c:26` | Y |
| debug-only: `Corrupt cookie: 0x%x`, `Bad cookie size: %d, %d`, `All %d task pointers in use.`, `taskStart: stillborn task %s`, `Called before task module started or after closed.`, `memAlloc: failed to allocate %d bytes of %s.`, `fileLoadAlloc: expected %d bytes, read %d bytes`, `Non-Fatal Error (cancel to debug)`, `\n%s (%d): Warning- %s`, `\nHomeworld CRC = 0x%x 0x%x 0x%x 0x%x` | various | N (compiled out or changed) |

### 14.4 Script names and tweak/option identifiers
| string | source | Cata |
|---|---|---|
| `English.script` `French.script` `German.script` `Spanish.script` `Italian.script` | ORIG `Game/Strings.c:763-783` | Y |
| `Tweak.script` `AIShip.script` `AITrack.script` `Camera.script` `CameraCommand.script` `CommandLayer.script` `Formation.script` `Galaxy.script` `Flightman.script` `Soundevent.script` `Trail.script` `Resource.script` `Cloud.script` `SinglePlayer.script` `Nebulae.script` `AIPlayer.script` `NetTweak.script` `AIResourceMan.script` `UnitLimitCaps.script` `Damage.script` `ShipView.script` `AutoLOD.script` `GatherStatsFor.script` `FEColour.script` `gametypes.script` | GoK `Game/StatScript.c:1865-1890,1937` | Y |
| `Dock.script` | `:1875` | N |
| `WONstuff.txt` | GoK `SDL/utility.c:3788` | Y |
| `RETALIATE_ZONE` `BLAST_CONSTANT` `cmFloatRefundRatio` `shipsRetaliate` `DefaultScenario` `useMission` `AUTOSTABILIZE_FRAME_KICK_IN` `REFRESH_RENDERLIST_RATE` `REFRESH_RENDERLIST_FRAME` `CHECK_BOUNTIES_RATE` `REGROW_RESOURCES_CHECK_RATE` `REFRESH_RESEARCH_RATE` `HYPERSPACE_SLICE_RATE` | GoK `Game/Tweak.c:467-...` (`makeEntry`), except `REGROW_RESOURCES_CHECK_RATE` (`Game/ResCollect.c`) and `HYPERSPACE_SLICE_RATE` (`Game/SinglePlayer.c:265`) | Y |
| `g_bNeedMoreSpaceForOtherLanguage` `twRepairRate[CLASS_Mothership]` `twCrystalProcessingTime` `twnLipsBaseOffset` | Cataclysm-only (`CET: assets/tweak.script`) | Y |
| cfg keys `PlayerBaseColor` `PrevColor0.base` `PrevColor0.detail` `NEXT_FORMATION` `MusicVolume` `Equalizer0` `TutorialNeeded` `ResInjAmmount` `MPGameFlags` `PlayerName` `PlayerPwd` `FirewallDetect` `spCurrentSelected` `effectsLevel` | ORIG `Win32/utility.c:879-968` | Y |
| `noPalMB` | `:881` | N |
| `strDefaultPlayerName` `strVersion` (enum names via `strEntry`) | GoK `Game/StringSupport.c:18` | Y |
| command-line literals, see the section 1.2 table | | |

### 14.5 Numeric constants (little-endian dwords or immediates)
| value | meaning | source | Cata |
|---|---|---|---|
| `0x10ad0027` | save version (HW1) | ORIG `Game/SaveGame.h:13` | Y (3) |
| `0xfa7babc0` | `MBF_VerifyValue` (memory cookie) | GoK `Game/Memory.h:83` | Y (3) |
| `0x4e7a110c`, `0xf4eeda7a` | memory fill patterns | `:126-127` | N |
| `0x0000fed5` | `DBG_ExitCode` | GoK `Game/Debug.c:25` | Y (3) |
| `0x77073096` | CRC32 table entry 1 | `ThirdParty/CRC/CRC32.c` | Y (1) |
| `0x3D800000` | 0.0625f = 1/16 s | universe/NIS/baby periods | Y (20) |
| `0x43700000` | 240.0f timer resolution | CET `Cataclysm.h:190` | Y (2) |
| `0x41800000` / `0xC1800000` | ±16.0f | ETG/effector | Y (6 / 4) |
| `0xD5` | `.big` name XOR seed | `Game/BigFile.c:670` | not searched (byte) |
| `0x0FFF` / 12-bit / 4-bit fields | LZSS window mask | `ThirdParty/LZSS/LZSS.c:43-52` | not searched |
| `240` (push 0xF0), `8` (`TSK_MaxTicks`), `32` (`TSK_NumberTasks`), `33` file slots | scheduler/file limits | | push 0xF0 Y at 0x005ABF68 |

---

## 15. Open items for the decompilation

* Confirm whether Cataclysm's scheduler is still the ORIG asm context switch (look for `call [taskFunctionContinue]`-style indirect calls and register saves into a `taskdata`). Confirm the inferred `taskStart(name, fn, period, flags)` and `universeUpdateTask = 0x0053E460`.
* Find where `universe.realtimeelapsed` is advanced and what consumes it. CET says SP fade in/out depends on it.
* Identify the two `univUpdate` callers outside `universeUpdateTask` (0x00524846 and 0x005256BC). ORIG release has only one live caller outside the task (`stats.c:523` `HaveShipsFight`); the NIS teaser call is commented out in ORIG and GoK. Also find how Cataclysm's user-facing time compression drives extra updates, given only 3 call sites in the task.
* Check whether the 19-slot `kbKeySavedKeys` path still runs alongside `default_keys.cfg`, and how `v 84` is checked.
* Check whether the save version 0x10ad0027 is really used by `SaveVersionInfo`/`LoadVersionInfo`/`VerifySaveFile`, and work out the full Cataclysm `Universe`/`Player` layout (CET has partial offsets).
* Find whether `HomeworldCRC[0]` (code CRC) is still computed and sent in Cataclysm's network handshake.
* Find Cataclysm's real HW1 `memAllocFunction(length, flags)` (release, no name argument) near the `MBF_VerifyValue` immediates at 0x004AD7EE/0x004AD833/0x004ADD27. CET's `memAlloc` (0x006725D0) is the debug-CRT `malloc`, not this allocator (section 4).

---

## Verification

A skeptical second pass on 2026-10-08 checked this document against the sources: ORIG at `/tmp/claude-1000/hw1orig/orig/src` (GoK root commit ad1bb82, confirmed with `git log`), GoK 3af31fb, HWSDL 1bed493, CET 19a08c3, and read-only byte reads of `original/game` (PE section mapping, string and dword searches, `E8 rel32` call-target scans, LZSS decoding of `.big` members). Nothing was executed or disassembled, and no reference clone was modified.

**Claims checked: 57.** These were file:line anchors, constants, struct layouts, CET addresses, GAME byte and string claims, and counts. Confirmed without change:
* the ORIG and GoK start-up, main-loop, timer, task and universe anchors;
* `taskdata` (48 bytes, `TOF_Context` 16, at most 255 stack bytes);
* the 16 Hz rate table and the 16 GoK "16 Hz site" anchors spot-checked;
* the 22 frame-mask line numbers spot-checked;
* every CET `Cataclysm.h` line and address cited, and all 12 CET instruction sites (bytes read);
* the memory constants and cookie layout, the `.big` header, TOC, XOR, CRC and LZSS facts, and the save-version constants;
* about 200 string and command-line Y/N results (115 literals plus 96 options re-searched);
* `default_keys.cfg` (`v 84`, 197 `key` lines), the GOG `/noswddraw` play task, the MAKEFILE flags and the `Homeworld.exe` PE facts.

**Claims wrong: 18, all fixed in place:**

| # | claim | truth (evidence) | fixed in |
|---|---|---|---|
| 1 | `/demoRecord`, `/demoPlay` are visible, unconditional ORIG entries; Cataclysm removed them | commented out in ORIG `Win32/main.c:1321-1325`, so the absence is inherited | 1.2, 9.1 |
| 2 | ORIG has `/disablePacking` | inside `/* */` at `Win32/main.c:1345-1351` | 1.2 |
| 3 | `%dx%d.dem` is a Cataclysm variant of GoK's `%ux%u.dem` | ORIG uses `%dx%d.dem` (`Win32/utility.c:4397,4406`) | 9.1, 14.1 |
| 4 | `"Fatal Error - %s"` is new in Cataclysm | ORIG `dbgFatalf` format, `Game/Debug.c:158` | 10, 13, 14.3 |
| 5 | HW1 has 6 `univUpdate` call sites incl. the NIS teaser | the NIS call is inside `/* */` in ORIG and GoK, and the turbo call is compiled out of release, so ORIG release has 4; Cataclysm has 5 `E8` calls (3 in `universeUpdateTask`) | 2.5 A, 13, 15 |
| 6 | CET `memAlloc` 0x006725D0 may be the HW1 `memAllocFunction` | its bytes match debug-CRT `malloc`/`free`; ORIG `rinit.cpp:73-87` explains the name; HW1 cookie code is near 0x004AD7EE | 4, 13, 15 |
| 7 | HW1 string enum has about 656 entries | 590 (`NumStrings`); 657 is a line number | 7, 13 |
| 8 | CET `english.script` has 1694 keys, 1330 new | 1693 keys, 1329 not in HW1 enum (226 HW1 names absent) | 7, 13 |
| 9 | most SaveGame.c GoK/HWSDL diff lines are the version table | they are the `->header.` SpaceObj refactor; `SaveGame.h` identical | 0.1 |
| 10 | GoK/HWSDL diff "2 to 41" lines | 6 to 41 (main 41, Memory 9, File 29, Demo 6, StatScript 6) | 0.1 |
| 11-13 | registry names, `Cataclysm.cfg` and the tweak-script names sit in `.rdata` | all in `.data` (addresses now given) | 1.3, 6 |
| 14 | `PatchETG.cpp:159-176` | file has 95 lines; the code is at `:49-59,82` | 2.5 C |
| 15 | `SDL (34 .c)` | 31 `.c` + 3 `.cpp` | 12 |
| 16 | `utyTaskTimerClear` runs on activate | it runs in `DeactivateMe` (GoK `SDL/main.c:1118`, ORIG `:1741`) | 3.2 |
| 17 | all listed tweak names come from `Tweak.c` | `HYPERSPACE_SLICE_RATE` is `SinglePlayer.c:265`; `REGROW_RESOURCES_CHECK_RATE` is `ResCollect.c` | 14.4 |
| 18 | CET README says "no longer" | the exact text is "No need to edit the registry anymore!" (`README.md:22`) | 1.3 |

**Added (grounded in sources or GAME bytes):**
* debug-CRT evidence (0.4);
* the full list of 57 command-line literals in the exe (1.2);
* the Cataclysm task table with `taskStart` = 0x0052A5F0 and its 10 callers, names, functions, periods and flags (3.3);
* `.big` whole-TOC sort and padding checks, the odd-length CRC bug confirmed in the shipped archives, and LZSS decode checks (5.2-5.4);
* TOC CRCs 0x617E5240 and 0xE86EEFA6 (5.6);
* in-binary `scriptEntry`/`scriptStructEntry` layout examples (6);
* the pooled-constant caveat for 0x0072A614 and the `fld qword` evidence for the CET ETG write (13);
* `RMOUSE_DOUBLE 7` (11), `__FILE__` paths (10), and filled "not tested" cells in 1.2.

**Remaining issues (UNVERIFIED):**
* the internals of Cataclysm's scheduler (ORIG asm context switch or something else);
* which functions own the two `univUpdate` calls at 0x00524846/0x005256BC, and how 8x time compression drives extra updates;
* which of the eight `fmul [0x0072A614]` sites are timer-related;
* where the frame limiter sits inside `RenderTask` 0x0057A910;
* whether Cataclysm still sends `HomeworldCRC[0..3]` in ORIG layout;
* the `scriptSetUdwordCB`/`scriptSetSdwordCB` naming conflict at 0x00525C20;
* how `default_keys.cfg` relates to the 19-slot `kbKeySavedKeys`;
* LZSS stale-window safety for all 16528 members (430 checked);
* fan-in counts in section 12 (±1-2 recount drift).

All of these need Ghidra (`docs/ghidra-pipeline.md`).
