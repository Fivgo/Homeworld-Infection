# HW1 source port: lessons for the Cataclysm recreation

Scope: how the community ported Relic's 2003 Homeworld 1 source to SDL/Linux (HomeworldSDL, now Gardens of Kadesh), a native build of it on this machine as a toolchain test, and which parts of that work a Cataclysm decompilation will have to repeat or can reuse.

**Path prefixes used below**

| Prefix | Meaning |
|---|---|
| `GoK:` | `reference/gardens-of-kadesh/` (shallow clone, HEAD `3af31fb`, author date 2026-04-28; committer date 2026-09-25 because the whole history was rebased, see A1) |
| `HWSDL:` | `reference/HomeworldSDL/` (GitHub mirror, shallow, HEAD `1bed493`, 2024-10-13; the README says "PROJECT MOVED ... This repo is only a mirror") |
| `ORIG@ad1bb82:` | Relic's 2003 source exactly as published. It is the **root commit `ad1bb82`** of the GoK history ("Original Relic code, as published in 2003", author aheadley, author date 2012-07-19, i.e. when Alex Headley put the snapshot on GitHub; the 2003 port commits sit on top of it). The reference clones are depth-1 (`git rev-parse --is-shallow-repository` returns `true`), so this commit is **not** in `reference/`. I read it from a full clone in scratch space: `git clone https://gitlab.com/gardens-of-kadesh/gardens-of-kadesh.git` (1091 commits, 62 MB). |
| `CET:` | `reference/CataclysmEngineTweaks/` |

Bottom line: **the build worked on the first real try.** After three missing packages were installed, `meson setup` plus `meson compile` produced `reference/build-hwsdl/homeworld` (ELF x86-64, 74.9 MB ASan/UBSan debug build) in 7.6 s wall time with zero warnings under `-Werror`. Release, clang and HomeworldSDL-mirror builds also succeed (Part B). The game was not run.

---

## 0. Key findings at a glance

1. **The GoK history keeps Relic's untouched original as the root commit (`ad1bb82`).** `git diff ad1bb82 HEAD` shows every port change. The first Linux port commit is `43ded70` (2003-10-12, aaron): 680 files changed, +162571/-151331.
2. **A symbolized Relic build of HW1 ships in the repo:** `GoK:zarchive/original-release-snapshot/exe/Homeworld.exe`.
   - PE32, 11,462,804 B (11.46 MB), link timestamp `0x3F4563EF` = 2003-08-22 00:29:35 UTC (2003-08-21 in North American time zones), optional-header linker 5.10, string "C/C++ Optimizing Compiler Version 11.00" present (that is VC++ 5.0). ImageBase 0x400000, entry RVA 0x29DA40, sections `.text .rdata .data .data1 .rsrc`.
   - It has a **Rich header** (file offset 0xB0). Decoded `(prodid, build, count)`: `(4, 8168, 13) (11, 8447, 47) (0, 0, 851) (6, 1668, 1)`. In the commonly published comp.id table these are Linker600, Utc12_CPP (VC6 C++), unmarked objects, and Cvtres500; that reading is UNVERIFIED. It is consistent with the game objects coming from a VC5-era compiler that emits no comp.id (851 unmarked) and some libraries from VC6.
   - It carries **embedded CodeView NB11 debug info** at file offset 0x4A9C00. Its directory has 1753 subsections: 912 sstModule, 559 sstAlignSym, 275 sstSrcModule (line numbers), plus sstGlobalTypes, sstGlobalSym and sstGlobalPub.
   - Symbol records use the 32-bit-type-index CodeView kinds: 11,204 distinct `S_GPROC32/S_LPROC32` names (kinds 0x100B/0x100A, 378 static) and 24,648 `S_PUB32` (0x1009, including C++/CryptoPP library publics). **6809 of the 6940** function names in the original source (`hw1_src_funcs.txt` in the build scratch dir) have a proc record. The first draft said 6439; that came from a looser string scan and was corrected by the verifier's record-level parse. Module paths are `c:\ReleaseHomeworldSource\obj\Debug\*.obj` (568 occurrences).
   - It is a **Debug (`-Od`) build**, so it gives types, struct layouts and names. It does not give byte-matching code for an optimized Cataclysm binary.
3. **Relic's real release builds mixed compilers.** Per `ORIG@ad1bb82:src/Win32/MAKEFILE:62-78,144-149` and `Release.depend`:
   - Most C files (244) were built with **Intel C (`icl`) `/Ogitb1 /GsF -G6 -Zp4`**.
   - Exactly 12 C files were built with **MSVC `cl -Od` (`CCMD0`)**, unoptimized even in release: `captaincy.c demo.c etg.c feflow.c key.c particle.c physics.c render.c speechevent.c tactics.c task.c univupdate.c`.
   - `trails.c` used `CCMD1`, which is the same `icl` command as the other 244.
   - The 12 C++ files (`avi clientcdkey dct ddraw dxdraw fqcodec fqeffect fquant md52 rinit titaninterface titanpacketmsg .cpp`) were built with `cl -Od /GX /GR` (`CCMD0CPP`).
   - The 18 kas2c-generated mission files (`..\generated\mission01.c` ... `mission16.c`, `mission05_oem.c`, `tutorial1.c`) used plain `$(CCMD)`. That is `cl` with **no optimisation switch**, which is unoptimised under MSVC's documented default (`/Od`; the default itself is not verified here). So mission code in a Relic release exe is unoptimised MSVC output.
   - `shader.c` and `knitransform.cpp` used `$(CCMDI) -O2` (Intel, `-O2` instead of `/Ogitb1`).
   - The Release level still links with `/DEBUG /DEBUGTYPE:CV` in both branches (`MAKEFILE:177-180`). `HW_PDB=HW_YESPDB` selects `LinkFlags2` (separate PDB); otherwise it uses `LinkFlags0` (`/PDB:NONE`, CodeView embedded in the exe). The no-debug `LinkFlags1/3` are commented out. A shipped release exe may therefore still carry a CodeView trailer (at least publics). Check Cataclysm's exe for one (C3).
   - **`-Zp4` struct packing everywhere.** Whether Cataclysm used the same toolchain is UNVERIFIED (blocked on the binary).
4. **Missions are compiled C, not data.** `.kas` scripts are run through `cc -E`, converted by `kas2c` (flex/bison), and linked into the exe as `Init_/Watch_` FSM functions (`GoK:src/Missions/SinglePlayer/meson.build`, `GoK:tools/kas2c/README.txt`). The `MissionNN_FunctionPointers[]` table that the port's save games use (`GoK:src/Game/SinglePlayer.c:2971ff`) is a **HomeworldSDL addition**. Per `kas2c/README.txt:1-6`, portable saves were "never done in the original version of Homeworld". Relic's kas2c is `ORIG@ad1bb82:tools/win32/KAS/KAS2C.{c,h,l,y}`. Cataclysm's missions will therefore be **decompiled C code inside the exe**, with no function-pointer table to anchor them.
5. **The original task scheduler is x86 stack-switching asm** (`ORIG@ad1bb82:src/Game/task.c:255-625`, `taskFunctionContinue`). The port replaced it with switch-on-`__LINE__` coroutines in 2007 (`ec6a8042`, `GoK:src/Game/Task.h:150-180`). CET's `CET:game/Game/task.h:131,167` still has the asm `taskYield`. But that header is a **verbatim copy** of HW1's original `task.h`: the only diff is a `#if 0` around the callbacks block. Nothing in CET references `taskFunctionContinue`/`taskStart`, so it is not independent evidence. Cataclysm (2000) predates the 2007 C rewrite, so it almost certainly uses the asm scheduler. That is UNVERIFIED until the binary is available.
6. **The 64-bit port keeps original assets.** On-disk structs with 32-bit offset "pointers" are mirrored as `*_disk` types and converted at load time (`a1ba7d94`, 2024-01-17). This covers `.geo .mad .nis .fib .ll/.lif .hff` and sound banks. Pointer-sized fields use `memsize` (`GoK:src/Game/Types.h:55-63`). `memsize` itself appears on 211 lines in 22 files, `smemsize` on 151 lines in 33 files, and `memrealsize` on 2. Together that is 364 lines in 49 files under `src/`, and 0 in the original.
7. **rGL is gone.** Relic's `src/rgl` (software rasterizer, D3D and Glide drivers behind an `rgl.dll` GL-1.1-subset API) was deprecated in 2007 (`e8e7e634`) and removed by 2011. The port draws with **legacy immediate-mode OpenGL through SDL2**. In 2024 (`25478715`) the CPU-side rGL vertex shading was restored because GL fixed-function lighting looked wrong. Cataclysm ships `rgl.dll`/`rgld3d.dll` (`CET:README.md` install step 2).
8. **Determinism was not preserved.**
   - The port compiles x86-64 SSE math (the original used x87).
   - The port runs the universe at **64 Hz instead of 16 Hz** by default (`UNIVERSE_UPDATE_RATE_SHIFT=2`, `GoK:src/Game/Universe.c:75`, commit `e3aa4e25`). `/vanilla` restores 16 Hz.
   - It replaced `FAST_TO_INT` round-to-nearest with truncating casts.
   - Multiplayer compatibility is a build-version string match (`GoK:meson_options.txt` `build_version`).
   - Original save games (`0x10ad0027`, raw memory dumps) are **not loadable** (`98726fa4`; `GoK:src/Game/SaveGame.h:16-22`).
9. **Licence:** the REULA is non-commercial only, and modifications are assigned to Relic. GoK asks new contributors to also license under MIT (all commits after `0e40bdd0`). In the full history that covers only 4 commits, all by Thibault Lemaire, and `COPYING.MIT` names only him (2019-2026). Rafael Brune's 2021-2024 work (64-bit assets, sound, rGL shading, wasm) carries **no MIT grant**. See A12.
10. **Relic's `udword`/`sdword` are `unsigned/signed long`** (`ORIG@ad1bb82:src/Game/TYPES.H:33-34`), which is 8 bytes on LP64 Linux; the port rebased them on SDL `Uint32`/`Sint32` in its first commit. CET's `TYPES.h` keeps `long`. The original has **no endianness handling**; GoK's `FIX_ENDIAN*` macros (511 uses in 21 files of `src/` and `tools/`) are no-ops on little-endian hosts. The 15 files that swap right after a load double as an inventory of the binary-format readers; `Types.h` defines the macros; the other 5 are packed-colour or flag-bit fixes (A5b).

---

## Part A: How the port was done

### A1. Lineage and repository layout

| When | Event | Evidence |
|---|---|---|
| 2003 | Relic releases the HW1 source via RDN under the REULA | `GoK:README.md` History; `HWSDL:LICENSE.txt` |
| 2003-10-12 | Aaron Plattner's "Initial import of the Homeworld 1 Linux port" (SVN r1) | commit `43ded70` |
| 2003-2016 | HomeworldSDL (SVN at homeworldsdl.org, later GitHub). 64-bit (aunxx, 2008), Mac, ARM (2009), GLES (2011) | `GoK:zarchive/CHANGELOG`, git log |
| 2019- | Revived as Gardens of Kadesh (Thibault Lemaire, Rafael Brune, et al.) on GitLab. Meson, sanitizers, runtime 64-bit asset conversion, wasm, Remaster assets | `GoK:README.md`, git log |
| 2026-08/09 | History rebased so that `ad1bb82` = Relic's untouched snapshot. Unused original material moved to `zarchive/original-release-snapshot` | `GoK:zarchive/README.md` (states the rebase, gives no date); `7adffa20` "Archive everything not used by HWSDL" (author date 2026-08-17); rebased commits carry committer date 2026-09-10/25 |

**Tree at HEAD (GoK)**

| Path | Contents |
|---|---|
| `src/Game` | Engine and gameplay (304 files) |
| `src/Ships` | Per-ship behaviour (84 files) |
| `src/SDL` | Platform layer: the old `src/Win32`, renamed |
| `src/Missions/{SinglePlayer,Tutorials}` | `.kas` mission scripts |
| `src/ThirdParty` | `CRC/CRC32.c`; `LZSS/{LZSS,BitIO}.c`; `stb/stb_image{,_write}.h`; `Titan/ServerStatus.h` |
| `tools/` | Ported Relic tools |
| `Linux/`, `Mac/`, `Windows/`, `mingw/`, `wasm/` | Per-platform build files |
| `zarchive/` | Obsolete docs and the original-release snapshot |

The original tree (`ORIG@ad1bb82`) was `src/{Game(302 files),Win32(108),rgl(116),Titan(183),JPG(61),Ships(81),SinglePlayer(37),Tutorials,Sigs}`, `Lib/{d3d,3dFX,Titan,Smacker}`, `tools/{win32(580),bin(214),batch(44)}`, `documents/technical/*.doc`, `obj/`, `exe/`.

The original source filenames were **lowercase** (`universe.c`, `univupdate.c`, `bigfile.c`) plus `TYPES.H`. The port renamed them to CamelCase (`Universe.c`, ...) and fixed case-mismatched `#include`s (`739cb8d8`, "Fix case sensitive includes", 2007).

### A2. Build system

#### Meson (current, `GoK:meson.build`, Meson 1.3.2 is enough)

| Item | Value | Line |
|---|---|---|
| Default options | `warning_level=3`, `c_std=gnu11`, **`b_sanitize=address,undefined`** (debug + ASan/UBSan by default) | 6 |
| Hard dependencies | `sdl2` (38), `gl` (44), `m`; `x11` on Linux and macOS (131/134) | 38-47, 131 |
| `-Werror` | Always on, followed by 37 `-Wno-*` suppressions for legacy code (lines 59-99; the same 37 appear on the logged compile line in `06-tool-polycount.log`). Several are GCC-only, so a clang build needs extra flags (Part B). 8 more clang-style ones are added only for emscripten (102-113) | 55 |
| CPU != x86 | `-DGENERIC_ETGCALLFUNCTION` (portable ETG call path, no asm) | 117 |
| x86_64 / aarch64 | `-DHW_PTR_64 -DHW_PTR_64_FIX_ME` | 121, 127 |
| x86 (32-bit) | `-malign-double -D_X86 -msse` | 123 |
| Linux | `-D_LINUX_FIX_ME -D_GNU_SOURCE` | 132 |
| Windows (mingw) | `-D_LINUX_FIX_ME -D_GNU_SOURCE -D_WIN32 -DWIN32 -DWINDOWS` | 143 |
| `-Ddemo` | `HW_GAME_DEMO` vs `HW_GAME_HOMEWORLD` | 150/152 |
| `debug` + `-Ddebug_asserts` | `HW_BUILD_FOR_DEBUGGING`, else `HW_BUILD_FOR_DISTRIBUTION` | 156/158 |
| `-Dmovies` (default true) | `HW_ENABLE_MOVIES`; adds `libavutil libswscale libavcodec libavformat` | 162-172 |
| `-Dbuild_version` | Shown in the menu; **used for the multiplayer compatibility check**; defaults to `git describe` | `meson_options.txt` |
| Subdirs | `tools` (kas2c, polycount), `src`, `tools/biggie`, `tools/monochrome-btg`, `tools/x86_64` (x86_64 build hosts only). Tools other than kas2c are `build_by_default: false` | 203-209 |

Generated sources:
- `src/Game/meson.build:139-142` runs `cc -E functions.h` to produce `wrapped_functions.h`, the ETG call wrappers (see A6).
- `src/Missions/*/meson.build` runs `cc -E -x c MissionNN.kas` to produce `.kp`, then `kas2c` to produce `MissionNN.c`, `.h` and `.func.c`.
- `tools/kas2c/meson.build:3-4` requires **flex and bison**.

`GoK:Linux/BUILD.md`:
- Fedora package list: `meson ninja libGL SDL ffmpeg libswscale-free libasan libubsan`.
- Ubuntu/other distros: "Coming Soon™" (run meson, install what's missing, repeat).
- Quick start: `meson setup build && meson compile`.
- Fast build: `meson setup --buildtype=release -Db_sanitize=none build.fast`.
- Deprecated autotools path: `cd Linux; ./bootstrap; ../configure; make`.
- 32-bit: `CFLAGS='-m32' ../configure --disable-x86_64`.
- wasm: cross file `wasm/wasm32-emscripten.meson-cross-build-definition.txt`.

`GoK:Linux/flake.nix`:
- Dev shell adds `SDL2_image clang-tools_16 meson pkg-config python3 ninja gdb ffmpeg emscripten`.
- Sets `LD_LIBRARY_PATH` to pipewire + `/run/opengl-driver/lib`, because with sanitizers "dlopen() fails to find libs" (`flake.nix:32-33`).
- `hardeningDisable = ["all"]`, because `_FORTIFY_SOURCE` conflicts with `-O0` (`:34-37`).
- The package is built through an external flake (`the-guidestone`) using **autotools** with `--enable-hwdebug --enable-sanitizers` (`:50-53`).
- There is also a CI docker image for emscripten.

Legacy autotools options (`GoK:Linux/stuff/configure.ac:50-227`): `--enable-raiders --enable-demo --enable-hwdebug --enable-sanitizers --disable-x86_64 --disable-generic-etg --disable-linuxfixme --enable-jpegmemmgr --enable-asm --disable-win32fixme --enable-movies --enable-network (HW_ENABLE_NETWORK) --enable-oldsound`, plus GLES (`HW_ENABLE_GLES`, :235-236). **Meson never defines `HW_ENABLE_NETWORK` or `HW_ENABLE_GLES`**, so LAN play and GLES are compiled out of a default meson build.

Windows paths:
- `GoK:mingw/Makefile`: mingw64 x86_64, `14cd46cd`, 2023.
- Meson on Windows: `21266470`, 2024-04-22.
- Old VS2003/2005 solutions under `GoK:Windows/` (SDL 1.2-era, stale, `Windows/building.txt`).
- A GoK 1.2.0 Windows x86_64 zip is linked from `GoK:documentation/community/installation-manual.md:104`.

#### Relic's original build (`ORIG@ad1bb82:src/Win32/MAKEFILE`)

- Watcom `wmake` driving MSVC `cl` and Intel `icl`.
- C flags: `-c -MT -Zp4 -nologo -W2 -DWIN32 -Zl ... -D__MSC__=yes`, `Processor=-G6` (Pentium Pro).
- Levels, from the makefile comment:
  - `HW_Debug`: VC, full debug, `-Od`.
  - `HW_Interim`: Intel, debug, no opt.
  - Otherwise: Intel, `/Ogitb1 /GsF`, no debug info.
- Link: `/MAP /SUBSYSTEM:WINDOWS,4.0 /DEBUG /DEBUGTYPE:CV /PDB:NONE`. This embedded CodeView explains the NB11 in `exe/Homeworld.exe`.
- Libs: `comdlg32 user32 gdi32 advapi32 winmm kernel32 ole32 libcp vfw32 dsound libcmt msacm32 shell32`, Titan `common/msg/EasySocket/crypt/auth_w95.lib`, `binkw32.lib`, `wsock32`, `ddraw`, `dxguid`.
- Language defines: `HW_English/HW_French/HW_German`.
- Product variants: `CGW`, `Downloadable` (demo), `DLPublicBeta`, `OEM` (Raider Retreat).

### A3. The SDL layer: Win32 surface replaced

Original Win32 API use (counted with `git grep` over `ORIG@ad1bb82:src/{Game,Win32,Ships}`, call-name occurrences): `Sleep` 31, `MessageBox` 13, `_findfirst` 13, `QueryPerformanceCounter` 11, `GetProcAddress` 11, `CreateMutex` 11, `WaitForSingleObject` 10, `PeekMessage` 10, `Reg*` ~20, `ChangeDisplaySettings` 6, `waveOut*` ~15, `DirectDrawCreate` 4, `CreateWindow(Ex)` 5, `_beginthread` 4, `ClipCursor` 2, `SetPixelFormat/ChoosePixelFormat/SwapBuffers` 6, `GetDriveType` 3, `GetVolumeInformation` 1, `AVIFileInit` 1, `DirectSoundCreate` 1.

| Subsystem | Original (Win32) | Port (SDL2) | Where (GoK) |
|---|---|---|---|
| Entry / message pump | `WinMain`, `WindowProc`, `PeekMessage`/`DispatchMessage` | `main()` → `SDL_PollEvent` loop → `HandleEvent` → `utyTasksDispatch()` each iteration, `SDL_Delay(0)` | `src/SDL/main.c:2135` (main), `:1685` (HandleEvent), loop `:2284-2306`; emscripten variant `main_loop` `:2034` |
| Single-instance check | `CreateFileMapping("LSMap")` + `FindWindow` | Kept only under `#ifdef _WIN32` | `main.c` (top of `main`) |
| Window + GL context | `CreateWindow`, `ChoosePixelFormat`/`wglCreateContext` through `gldll.c`, `ChangeDisplaySettings` | `SDL_CreateWindow(SDL_WINDOW_OPENGL [\|FULLSCREEN_DESKTOP])`, `SDL_GL_CreateContext`, MSAA fallback loop halving 8→4→2→1, then no MSAA (`render.c:976-1001`; after the no-MSAA retry succeeds the `while` condition calls `SDL_CreateWindow` once more, which looks like a window leak, UNVERIFIED by test), `SDL_GL_SetSwapInterval(1)` (vsync, `:1012`); default is fullscreen (`/fullscreen` default TRUE, `/window` to override, `main.c:690-691`), `SDL_GL_SwapWindow` | `src/SDL/render.c:893-1015`, `:4655` |
| Display modes | `EnumDisplaySettings` | `SDL_GetNumDisplayModes`/`SDL_GetDisplayMode` | `src/SDL/rinit.c:420-440` |
| Keyboard | `WM_KEYDOWN` virtual keys, `ToAscii` | SDL **scancodes** fed into the original `keyPressDown/Up/Repeat`; per-language remap `keyLanguageTranslate` | `main.c:1623,1725-1780` |
| Mouse | `WM_*BUTTON*`, `ClipCursor`, `SetCursorPos` | SDL button events with a hand-rolled 500 ms double-click; `SDL_SetRelativeMouseMode`, `SDL_WarpMouseInWindow` (scaled for logical vs actual resolution) | `main.c:1782-1870`; `src/SDL/mouse.c:501,522-524` |
| Wheel / joystick | n/a | `SDL_MOUSEWHEEL` → `FLYWHEEL_UP/DOWN`; `SDL_JOYAXISMOTION` → camera (SpaceNavigator) | `main.c:1836-1921` |
| App focus | `WM_ACTIVATEAPP` | `SDL_APP_DIDENTERFOREGROUND/WILLENTERBACKGROUND` → `ActivateMe/DeactivateMe` | `main.c:1700-1724`, `:1108,1154` |
| Lock-key restore | Win32 keyboard state (`SetKeyboardState`/`keybd_event`) | `utyToggleKeyStatesRestore` contains **raw Xlib `XSendEvent` of fake Caps/Scroll/Num Lock presses to its own window via `SDL_GetWindowWMInfo().info.x11`**. It is compiled in, so it is the only reason meson links `x11`. **It is dead code in a Linux meson build.** The two `main.c` call sites (Activate/Deactivate) are inside `/* ... */` comments. The only other caller, `utility.c:4653-4657` (`utyGameSystemsShutdown`), is wrapped in `#ifndef _LINUX_FIX_ME`, and meson defines `-D_LINUX_FIX_ME` on Linux (meson.build:132). Only `utyToggleKeyStatesSave` (pure SDL) runs. Correction by verifier: the first draft said it was called from `main.c` | `src/SDL/utility.c:117-118` (includes), `:4912-5010` (function), `:3840` (Save call); commented-out calls `main.c:1139-1143,1183-1187` |
| Timing | `QueryPerformanceCounter` | `SDL_GetTicks()` (ms) → `utyTasksDispatch` converts to task ticks; `taskStartup(1000/utyTimerDivisor)`; `UTY_TimerResolutionMax = 15*UNIVERSE_UPDATE_RATE`. Because of integer millisecond division (`utyTimerDivisor = 1000 / UTY_TimerResolutionMax`), the task tick rate is **250 Hz** at 16 Hz (1000/240 = 4 ms) and **1000 Hz** at 64 Hz (1000/960 = 1 ms). The original uses `UTY_TimerResloutionMax 240` (sic, `ORIG:src/Win32/utility.h:39`) with `LONGLONG utyTimerDivisor = QPC frequency / 240`, which gives a ~240 Hz task tick (`ORIG:src/Win32/utility.c:4095-4109`). CET exposes Cataclysm's `utyTimerDivisor` (LONGLONG, 0x00AB7520), `TimerResolutionMax` (float, 0x0072A614) and `taskFrequency` (0x00BB3260) (`CET:Cataclysm.h`) | `src/SDL/utility.c:4671-4697,3906-3909`; `src/SDL/utility.h:39` |
| Threads / locks | `_beginthread`, `CreateThread`, `CreateMutex`, `WaitForSingleObject` | `SDL_CreateThread` (sound stream `src/SDL/sstream.c:88`; network), `SDL_CreateMutex/mutexP/V` (`Game/TitanNet.c:295`, `SDL/Queue.c`), `SDL_CreateSemaphore` (`Game/Subtitle.c`) | as listed |
| Audio out | DirectSound / `waveOut*` | `SDL_OpenAudio` callback + original software mixer (`smixer.c`) and streamer (`sstream.c`) | `src/SDL/soundlow.c:230` |
| Audio codec | Relic `fqcodec/fqeffect/fquant/dct` shipped **only as `.obj`** (`ORIG:obj/*/fq*.obj`) | 2004: Windows COFF objects converted and linked with glue (`src/SDL/win32glue.c`: `_ftol`, `_chkstk` from Wine). **2008: reverse-engineered to C** (`7fe67f29`). 2022 fixes (`7f0c0af5`, "Fix issues with reverse engineered sound system") | `src/SDL/{fqcodec,fqeffect,fquant,dct,mixfft}.c` |
| Movies | Bink SDK (`binkw32.dll`), `vfw32` AVI | FFmpeg `avformat_open_input` on the original `Movies/*.bik` (FFmpeg decodes Bink 1) | `src/SDL/avi.c:420`; `src/Game/Animatic.c:721-724` |
| CD audio / CD check | MCI, `GetDriveType`, `GetVolumeInformation`, CD key | SDL1 `SDL_CD*` calls left in `Game/SoundEvent.c` under guards; CD validation compiled out (`CD_VALIDATION_ENABLED 0`) | `src/Game/SoundEvent.c`; `utility.c:140,3501,3636` |
| File search | `_findfirst/_findnext` | `opendir/readdir` + `strcasecmp` (A8) | `src/Game/File.c:342-622` |
| Registry | `HKLM\SOFTWARE\Sierra On-Line\Homeworld` read/write table (`ORIG:src/Win32/utility.c:66-196`, `main.c:20-36`) | 2003-2008: a "reg" file emulation. Since `e50060d3` (2008): **`Homeworld.cfg`** (`UTY_CONFIG_FILENAME`, `utility.c:142`, `utyOptionsFileRead :866`, `utyOptionsFileWrite :899`) in the user settings dir. `regkey.h` still defines `BASEKEYNAME` and `CONFIGDIR` (`.homeworld`, `.homeworldDownloadable`, `.homeworldOEM`) | `src/SDL/regkey.h`, `src/SDL/utility.c` |
| Paths | exe dir, registry CD path | `HW_Data` env, else cwd → data path; override dir = data path; settings `~/.homeworld`; `/bigoverride`, `/settingspath` flags | `utility.c:3534-3600`; `main.c:633-635` |
| Debug window | Win32 `debugwnd.c` | Windows only (`#ifdef _WIN32`) | `src/SDL/debugwnd.c` |
| Networking | Titan/WON (C++, `Lib/Titan/*.lib` closed) | `TitanInterfaceC.stub.c` ("Dummy functions to simulate a WONnet connection that never works") plus an SDL_net LAN stack (`NetworkInterface.c`, UDP broadcast + TCP) **only with `HW_ENABLE_NETWORK`**, not defined by meson | `src/SDL/TitanInterfaceC.c`, `NetworkInterface.c:5-83` |

`src/SDL` files compiled by meson (`GoK:src/SDL/meson.build`): `NetworkInterface Queue TimeoutTimer Titan TitanInterfaceC avi font main mainrgn mixfft mouse prim2d prim3d render rglu rinit screenshot smixer soundlow sstream texreg utility dct fqcodec fqeffect fquant`.

Not compiled: `utility_mac.c`, `debugwnd.c`, `wave.c`, `win32glue.c`, `ClientCDKey.cpp`, `TitanInterface.cpp`, `TitanPacketMsg.cpp`, `TitanInterfaceC.stub.c`.

### A4. Renderer: rGL to OpenGL

**Original design.** The game called a GL-1.1 subset through function pointers loaded at runtime (`ORIG:src/Win32/gldll.c`, "alternative to linking against OpenGL32.dll"; `LoadLibrary` + `GetProcAddress`).
- Targets: `opengl32.dll`, `3dfxvgl.dll`, or Relic's **`rgl.dll`**.
- `rgl.dll` front-ends three devices: `sw` (Relic's own software rasterizer: `kgl.c`, `swdriver.c`, span/scan code), `d3d` (`d3d*.cpp`, DirectX), and `fx` (3dfx Glide, `fxdriver.c`).
- `ORIG:src/rgl/rgl.exp` is a **binary COFF export object** (`.edata`, 544 symbols, created 2003-08-22 00:29:22 UTC, 13 s before `Homeworld.exe` was linked). Siblings: `rgld3d.exp` and `rglsw.exp`. Its strings show the GL subset (`glBegin` ... `glViewport`, `glColorTable(EXT)`, `glLitColorTable`, `glSuperClear`, `glGetFrameCountEXT`, `glPushAttrib`/`glPopAttrib`; 112 distinct undecorated `gl*` names), `wglCreateContext wglDeleteContext wglGetProcAddress wglMakeCurrent`, and these **42 Relic `rgl*` extensions** (verifier's full list; the first draft listed 20): `rglAnotherPoly rglBackground rglCreate0 rglCreate1 rglCreate2 rglCreateWindow rglCulledPolys rglD3DGetDevice rglD3DSetDevice rglDeleteWindow rglDisable rglDrawLightVectors rglDrawPitchedPixels rglEnable rglFeature rglGetFramebuffer rglGetFullscreen rglGetMaxTexobj rglGetSkip rglGetSlow rglGetTexobj rglGetTexobjs rglGetTruecolor rglIsClipped rglIsFast rglLightingAdjust rglList rglListSpec rglMeshRender rglNormal rglNumPolys rglRestoreCursorUnder rglSaveCursorUnder rglSelectDevice rglSetAllocs rglSetExtensionString rglSetRendererString rglSmoothTexturedTriangle rglSmoothTriangle rglSpecExp rglTexturedTriangle rglTriangle`.
- `reference/hw_rgl` defines every one of those 42 names (set difference against a grep of its sources is empty). Its sources also mention `rglDDrawActivate rglGetProcAddress rglPixelAt rglPutPixelAt rglPut24BitPixelAt`; some are internal, e.g. `static rglPixelAt` at `kgl.c:4464`. Exports: the `gl*` set is listed in `hw_rgl/draw.def` (`LIBRARY rgl`, "RelicGL rendering library"). The `rgl*` set is exported with `__declspec(dllexport)` through the `DLL` macro (`kgl_macros.h:9`, e.g. `rglext.h:18-22`). Its HEAD commit `41ff3c3` (2026-06-04) is "Handle Cataclysm attrib stack calls", so Cataclysm exercises `glPushAttrib`/`glPopAttrib` (`draw.def:77,79`).
- **This is the interface CET's `hw_rgl` reimplements for Cataclysm.** Cataclysm ships `rgl.dll`/`rgld3d.dll` (`CET:README.md`).

**Port timeline**

| Date | Change | Commit |
|---|---|---|
| 2003 | Glide 2 headers imported | `43ded70` |
| 2006 | 3dfx code removed | `2e850824` |
| 2007 | `src/rgl` deprecated | `e8e7e634` |
| 2008 | DirectDraw (`ddraw.cpp`, `dxdraw.cpp`) removed | `e50060d3` |
| 2011 | "RGL cleanup part 1-3" (`gldll.c`, `glcaps.c` deleted) | `cc1c88cd` |
| 2011 | GLES/EGL path | `910ffa5b`, `687b8889` |
| 2017 | SDL 1.2 → SDL2 | `657f6b64` |
| 2024 | "Restore original rgl shading code to fix gas/nebulae rendering and lighting" | `25478715` |

`25478715` restored CPU vertex colouring and removed a VBO path "for the sake of correctness". The commit message: *"Instead of relying on opengl material and lighting code to generate vertex colors the internal CPU code path is used ... This restores the original brightness of the shadowed side of meshes and the specular highlight on the nebulae."*

Today:
- `src/SDL/render.c` creates a default (compatibility) GL context and draws immediate-mode GL.
- About 184 `glBegin/glVertex3f/glEnd()` occurrences in `render.c` + `src/Game/*.c`.
- GLES emulates immediate mode in `src/SDL/glinc.h:4-216` (`#ifdef HW_ENABLE_GLES` block, `gles_*` arrays; the desktop path is `:216-237`).
- `rglu.c` keeps a few rGL helper routines.

### A5. 64-bit fixes (`HW_PTR_64`)

| Technique | Detail | Where |
|---|---|---|
| Pointer-width integer type | `memsize`/`smemsize`/`memrealsize` = `uqword`/`sqword`/`real64` on 64-bit, `udword`/`sdword`/`real32` on 32-bit. **211 uses / 22 files in GoK, 0 in the original.** Heaviest: `ETG.c` (61), `wrap.h` (27), `NIS.c` (25), `MeshAnim.c` (14), `texreg.c` (12), `Memory.c` (12) | `src/Game/Types.h:55-63` |
| Effect bytecode parameter scaling | `SCALECAST(x)` = `(memsize)(udword)x`; `SCALESHIFT(x)` doubles parameter offsets "from 4 to 8 bytes" (30 uses, mostly `etgEffectCreate` varargs in `Gun.c`, `HS.c`, `ETG.h:258`) | `Types.h:117-123` |
| **On-disk structs with offset "pointers"** | Every asset struct that stores `udword` file offsets later fixed up into pointers (`base + offset`) gets a `*_disk` twin. The loader reads the 32-bit layout and expands it. Types (typedef closing lines verified): `GeoFileHeader_disk` (Mesh.h:259-273), `polygonobject_disk` (:275-293), `meshdata_disk` (:309), `materialentry_disk` (:311-324); `madobjpath/madanimation/madheader_disk` (MeshAnim.h:158/167/182); `nisheader/spaceobjpath/camerapath_disk` (NIS.h:399/413/423), `lightpath_disk` = `spaceobjpath_disk` (:425); `fescreen/felink/featom_disk` (FEFlow.h:214-223/226-232/235-258); `charheader/charfileheader/fontheader/fontfileheader_disk` (SDL/font.h:130/138/156/166); `llelement/lifheader_disk` (SDL/texreg.h:257/291). Sound banks have no `*_disk` struct: `loadbankAndConvertTo64Bit()` (SoundEvent.c:2580ff, called at :2825/2838/2851 for Gun/Ship/SpecialEffect banks) re-lays out the bank in a 2x buffer | commit `a1ba7d94` (2024-01-17), "Support original assets in 64b architectures by converting them on-the-fly at load time". Before 2024 a separately converted `HomeworldSDL.big` was required (`GoK:Linux/README.X86_64`) |
| Enum fields holding pointers or -1 | `ShipType` became `smemsize` on 64-bit (on 32-bit Linux it had to be `udword`, else "the assault on Kharak NIS in Mission 3 segfaults") | `src/Ships/ShipDefs.h:79-91`; `Mesh.c:268` (`0xffffffffffffffff` sentinel) |
| ETG function calls | The original pushed N dwords with inline asm and called. The port generates one typed wrapper per callable (`wrap_<fn>`) from `funcEntryN(name, ret, fn, types...)` via `wrap.h` + `cc -E` | `src/Game/ETG.h:401-418,450-466`; `src/Game/{functions.h,wrap.h,wrap_types.h}`; `ETG.c:413,4061...` |
| Memory block header | Name lengths shrink so the cookie stays the same size on 64-bit (`MEM_NameLength` 48/52) | `src/Game/Memory.h:86-95` |
| `time_t` in files | `bigTOCFileEntry.timeStamp` changed from `time_t` to `udword` | `src/Game/BigFile.h:100` |
| Numeric workarounds | B-spline returns 0 on NaN ("Fixes the launching of ships through the big bad door") | `src/Game/B-Spline.c:252-258` |
| | `HW_PTR_64_BROKEN` mixer path disabled | `src/SDL/smixer.c:839` |
| | `HW_PTR_64_FIX_ME` in particles | `Particle.c:2064` |

Obsolete offline converters `tools/x86_64/{bank,fib,font,geo,lif,ll,mad,nis}.c` (2008, aunxx) remain in the tree. Several no longer compile because their `*_disk` typedefs now clash with the in-engine ones (Part B, B4).

### A5b. Type layer, endianness and the `HW_*` switches

Added in a continuation pass (sources: GoK `3af31fb`, full-history clone `gok-hist`, `CET:game/Game/TYPES.h`).

**Base types**

| Item | Original (`ORIG@ad1bb82:src/Game/TYPES.H`) | Port (`GoK:src/Game/Types.h`) |
|---|---|---|
| Compiler guard | `#if defined(_WIN32) && defined(_M_IX86)` else `#error Use of types.h for compiler other than Microsoft C` (:11-13) | SDL types (`SDL_stdinc.h`, `SDL_endian.h`, :15-16); MSVC warning pragmas only under `_MSC_VER` (:28-35) |
| `udword` / `sdword` | `unsigned long int` / `signed long int` (:33-34). 4 bytes on Win32, **8 bytes on LP64 Linux/macOS** (4 on LLP64 Win64) | `Uint32` / `Sint32` (`udword` :49, `sdword` :48; the first draft said :49-50, fixed by the verifier). Done in the very first port commit (`43ded70:src/Game/Types.h:41-42`) |
| `sqword` / `uqword` | `__int64` (:35-36) | `Sint64` / `Uint64` |
| `bool` | `typedef sdword bool` (:40), **signed** | 2003: `typedef udword bool` (unsigned, `43ded70:...Types.h:48`); 2024 `f47c87f6` (atsb, "bool to bool32 cleanup"): renamed to `bool32` = `udword` (:65), so C99/C23 `bool` is free |
| Pointer-width integers | none (pointers stored in `udword` freely) | `memsize`/`smemsize`/`memrealsize` (:55-63), see A5 |
| Type punning | `TreatAsUdword`/`TreatAsReal32` | Kept as pointer casts (:113, :115), a strict-aliasing hazard under `-O2` (no `meson.build` passes `-fno-strict-aliasing`; 8 use sites besides the two definitions: `Universe.c` 4, `DefenseFighter.c` 3, `KASFunc.c` 1); new `Real32ToUdword` etc. use `memcpy` (`Types.c:21ff`) |

CET's reconstructed `CET:game/Game/TYPES.h:11-24` is the Relic original (`udword` = `unsigned long int`), but with `bool32` = **`sdword`** (signed, `:24`), unlike GoK's unsigned `bool32`. CET's signedness matches Relic's original `bool` (`sdword`, `ORIG TYPES.H:40`); it is GoK that changed it (to `udword` in 2003). **Lesson:** a native Cataclysm rebuild must take its base types from GoK's `Types.h` (fixed-width), never from CET or Ghidra output (`ulong`/`DWORD`), or every on-disk struct doubles in size on LP64. Signedness of `bool32` is a behaviour question (comparisons, `>> 31`) to settle against the binary per use site.

**Endianness**

- The original game code has **no byte-order handling at all**: `src/Game` and `src/Ships` have no `endian` hit, and `git grep -iE 'byteswap|swap16|swap32|bswap|ntohl|htonl' ad1bb82 -- src/Game 'src/Win32/*.c'` is empty. `git grep -i endian ad1bb82` hits only: `REG_DWORD_LITTLE_ENDIAN` in the registry code (`ORIG:src/Win32/utility.c:114,157`); the MD5 code `src/Win32/Md52.cpp:10,24,29` (`#define LITTLE_ENDIAN`, CD-key/WON auth); a comment in `src/Win32/TitanInterface.cpp:4027`; the 3dfx Glide headers and `.lib`s under `src/rgl` and `Lib/3dFX`; and two editor tools (`tools/win32/LiFList/psd.h:104` big-endian PSD helpers, `tools/win32/Mission/Lib/lwob.cpp:112` LightWave I/O). (Verifier: the first draft said the registry code and Glide headers were the only hits.)
- GoK: `HW_ENDIAN = SDL_LIL_ENDIAN` (`Types.h:82`, comment :78-81: all on-disk data is little-endian). `#if SDL_BYTEORDER == HW_ENDIAN` (:89) makes `FIX_ENDIAN` 0 and `FIX_ENDIAN_INT_16/INT_32/FLOAT_32(x)` identity; otherwise `SDL_Swap16`/`SDL_Swap32`/`SwapFloat32` (:96-101; `SwapFloat32` = union + `SDL_Swap32`, `Types.c:7-19`).
- There are **no `LITTLE_ENDIAN`/`BIG_ENDIAN` tokens** left in GoK `src/` or `tools/` (grep); `Types.h:89` is the only byte-order test.
- History: Mac OS X PowerPC port merged 2004-05-24 (`9ad00155`, lmop, code by Michael Trepka and "Tigital"); The Mac merge handled speech by **rearranging the constants instead of the data**: `9ad00155:src/Game/SpeechEvent.h:1-36` has an `#if defined(__LITTLE_ENDIAN__)` block (:1-31) and a PPC block (from :33) with rearranged values (`ACTOR_PILOT_FLAG 0x04000000`). The PPC values are the LE values with their two 16-bit halves exchanged (LE `0x00000400` → PPC `0x04000000`, `SPEECH_GROUP_FLAG 0x00800000` → `0x00000080`, `SPEECH_EVENT_MASK 0x3FF` → `0x03FF0000`), i.e. a rotate by 16, **not** a 32-bit byte swap (which would give `0x00040000`); the first draft called them "pre-swapped". Why a halfword exchange was needed is UNVERIFIED (not traced to the speech loader). and its little-endian block mistyped Relic's `1024` as `1023`. `a21dc1cb` (2004-05-31) switched the test to `ENDIAN_BIG` and restored `1024` (Relic's value: `ORIG@ad1bb82:src/Game/speechevent.h:2`). Lesson: diff every constant table against the original after a platform merge. `1df2d4c5` (2006-12-07) moved to SDL's swap routines; `97213da5` (2007-03-02) fixed colour constants stored as `udword`.
- **`FIX_ENDIAN*` inventory: 511 occurrences in 21 files** (`grep -rho 'FIX_ENDIAN[A-Z_0-9]*' src tools`; the only other hit in the repo is a comment in `Mac/Homeworld_Prefix.h:11`). The first 13 rows below (through `Light.c`) plus `render.c` and `sstream.c` swap fields right after a file load or `memcpy` of an on-disk struct, so together they are the most complete inventory of binary-format readers in the engine. The rest are **not** file readers (verifier correction; the first draft said every swap follows a file read): `Particle.c`, `ETG.c`, `Color.h` and `Trails.h` fix packed-colour `udword` constants (the `97213da5` issue: "color c is actually a numerically calculated RGBA udword", `ETG.c:7909-7912`), and `Tutor.c:1948-1952` mirrors the bit order of the `tutEnable` flag words (`1 << (31 - Index & 31)`):

| File | `FIX_ENDIAN*` uses | `#if FIX_ENDIAN` blocks | Format read |
|---|---|---|---|
| `src/Game/SoundEvent.c` | 66 | 12 | sound banks, event tables |
| `src/Game/Mesh.c` | 54 | 4 | `.geo` |
| `src/Game/NIS.c` | 53 | 6 | `.nis` |
| `src/Game/BTG.c` | 51 | 7 | `.btg` backgrounds |
| `src/SDL/texreg.c` | 39 | 5 | `.lif`/`.ll` texture lists |
| `src/Game/FEFlow.c` | 36 | 4 | `.fib` front-end screens |
| `src/Game/BigFile.c` | 36 | 8 | `.big` header/TOC |
| `src/Game/MEX.c` | 29 | 2 | `.mex` mesh extensions |
| `src/Game/MeshAnim.c` | 29 | 5 | `.mad` animations |
| `src/SDL/font.c` | 25 | 5 | `.hff` fonts |
| `src/Game/SpeechEvent.c` | 24 | 4 | speech `SentenceLUT`/`PhraseLUT` (`speechsentence_comp.lut`, `speechphrase_comp.lut`; `DLsentence.lut` etc. in the demo) and the music header (`musicHeaderName`); `fileLoadAlloc` at `:446,473,563`, swaps at `:448-465,475-482,565-568` |
| `tools/monochrome-btg/monochrome-btg.c` | 22 | 1 (`:83`) | `.btg` header (tool) |
| `src/Game/Light.c` | 14 | 2 | `.hsf` lighting (`hsf\default.hsf`, `:218-220,318`) |
| `src/Game/Types.h` | 8 | n/a | definitions |
| `src/SDL/render.c` | 7 | 1 | TGA header (`:3276-3290`) |
| `src/Game/Particle.c` | 6 | 3 | packed colour dwords (`:3110,3137,3271`) |
| `src/SDL/sstream.c` | 4 | 2 | streamed audio headers |
| `src/Game/ETG.c` | 4 | 2 | not a reader: packed-colour arguments of `etgSetColor`/`etgSetColorA` (`:7909,7925`) |
| `src/Game/Color.h` | 2 | 1 | colour macros |
| `src/Game/Tutor.c`, `src/Game/Trails.h` | 1 each | 1 each | not readers: tutorial flag bit order (`Tutor.c:1948`); `TP_KeyColor` constant (`Trails.h:37`) |

(Format column: from the loader code at the cited lines where given, otherwise from the file's role in the engine. Particle.c and Color.h are likewise colour-constant fixes, not readers.)

**`HW_*` and other port switches**

| Define | Set by | Used in | Effect |
|---|---|---|---|
| `HW_PTR_64` | `meson.build:121` (x86_64), `:127` (aarch64) | 17 files (Types.h, Memory, ETG, NIS, Mesh, MeshAnim, FEFlow, SoundEvent, B-Spline, Particle.h, ShipDefs.h, texreg, font) | `memsize` = 64-bit, `*_disk` conversion, `SCALECAST`/`SCALESHIFT` (A5) |
| `HW_PTR_64_FIX_ME` | meson as above; `mingw/Makefile:45` | `Particle.c:2064` only | one 64-bit workaround |
| `HW_PTR_64_BROKEN` | never defined | `smixer.c:839` | disabled alternative mixer path ("this skips and makes noise; the original code in #else works ok on X86_64") |
| `_LINUX_FIX_ME` | `meson.build:132` (Linux), `:143` (Windows mingw), emscripten branch | `Sensors.c:3427` (sensors-manager mouse clip), `ShipDefs.h:83` (`ShipType` = `udword` on 32-bit), `utility.c`, `utility_mac.c` | marks "fixed for non-Win32" code; note the mingw Windows build also defines it |
| `GENERIC_ETGCALLFUNCTION` | `meson.build:117` (every CPU except 32-bit x86) | `ETG.h`, `ETG.c` | portable ETG call path via generated wrappers (A6) |
| `HW_ENABLE_MOVIES` | `-Dmovies` (default true), `meson.build:162-172` | `Animatic.c`, `avi.c` | FFmpeg Bink playback |
| `HW_ENABLE_MOVIES_WIN32` | never defined by any build file | `avi.c:32,79,515,626` | dead vfw32 AVI path ("Disable AVI code outside of Windows") |
| `HW_ENABLE_NETWORK` | autotools `--enable-network` only | `TitanInterfaceC.c`, `NetworkInterface.{c,h}` | SDL_net LAN (off in meson, A3) |
| `HW_ENABLE_GLES` | autotools only (`configure.ac:235-236`) | `HS.c`, `Particle.c`, `font.c`, `glinc.h`, `render.c`, `rglu.c` | GLES immediate-mode emulation (A4) |
| `HW_GAME_HOMEWORLD` / `HW_GAME_DEMO` | `meson.build:150/152` (`-Ddemo`) | `HW_GAME_DEMO` in 21 files | full game vs Downloadable demo |
| `HW_GAME_RAIDER_RETREAT` | autotools `--enable-raiders` only (`configure.ac:50-55`) | 13 files | OEM "Raider Retreat" variant; **meson cannot build it** |
| `HW_BUILD_FOR_DEBUGGING` / `HW_BUILD_FOR_DISTRIBUTION` | `meson.build:156/158` (debug + `-Ddebug_asserts`; `debug_asserts` defaults to **true**, so the default debug build, including `reference/build-hwsdl/homeworld`, is `HW_BUILD_FOR_DEBUGGING`) | 97 files in `src/` use `HW_BUILD_FOR_DEBUGGING`, 11 use `HW_BUILD_FOR_DISTRIBUTION` | Relic's debug features (asserts, cheat keys, stats). `meson_options.txt` warns the flag "causes slight behaviour changes", so fidelity comparisons must use a `--buildtype=release` build |
| `_X86`, `-malign-double`, `-msse` | `meson.build:123` (32-bit x86) | no C use of `_X86` in `src/` | `-malign-double` aligns `double`/`long long` struct members to 8, which **breaks `-Zp4` layout parity** (verifier probe below); the only on-disk structs with `real64` (`btgVertex`, `btgStar`) are read field by field, so BTG loading is unaffected |
| `ARM` | `meson.build:125,127` (arm, aarch64) | no use in `src/` outside comments in `ThirdParty/stb/stb_image.h` | none |
| `__APPLE___FIX_ANIM/_LAN/_MISC`, `__APPLE___64/_FIX_64`, `__APPLE___86/_FIX_86` (triple underscore) | `meson.build:135-140` (darwin), since `22488e6a` (2022) | **no use anywhere**: the source tests `_MACOSX_FIX_ANIM/_GL/_LAN/_MISC/_SOUND` (49 uses), which only the Xcode prefix header defines (`Mac/Homeworld_Prefix.h:19-24`, under `#ifdef __APPLE__`: ANIM/LAN/GL/MISC, not SOUND); no `meson.build` references that header, and `_MACOSX` itself is not defined by meson either | a meson macOS build appears to leave the `_MACOSX_FIX_*` paths in their non-Mac state (inferred from the sources, not built) |

**Relic's own build defines and their port names.** Relic's makefile passed `-D$(%HW_Level) -D$(Language) -D$(%USERNAME)` on every compile, plus `-DCGW`, `-DDownloadable`, `-DDLPublicBeta` or `-DOEM` for those product variants (the plain full-game branch at `:77-78` adds none) (`ORIG@ad1bb82:src/Win32/MAKEFILE:62-78`). There is also an upper-case `HW_DEBUG` (6 uses, 4 files). Counts are over `ORIG:src/{Game,Win32,Ships}`:

| Original define | Uses / files | Port name | Rename commit |
|---|---|---|---|
| `HW_Debug` | 128 / 61 | `HW_BUILD_FOR_DEBUGGING` | `6c9ecfb4` (lmop, 2006-12-09) |
| `HW_Release` | 176 / 96 | `HW_BUILD_FOR_DISTRIBUTION` | `441f2d2e` (lmop, 2006-12-08) |
| `HW_Interim` | 12 / 5 | gone (no hit in GoK `src/`) | |
| `Downloadable` / `OEM` / `CGW` / `DLPublicBeta` | 22 / 17 / 13 / 12 files | `HW_DEMO` / `HW_RAIDER_RETREAT` / `HW_COMPUTER_GAMING_WORLD_DEMO` / `HW_PUBLIC_BETA` (`f67df587`, 2006-06-10), then `HW_GAME_DEMO` (`bfcd63d3`), `HW_GAME_RAIDER_RETREAT` (`090a413a`), `HW_GAME_HOMEWORLD` (`101521d5`), all 2006-12-08 | |
| `%USERNAME%` of whoever ran the build | `#ifdef gshaw` 32, `bpasechn`/`bpasechnik` 9, `ddunlop` 6, `khentschel`/`khent` 6, `salfreds` 3, `lmoloney` 2, `#ifdef fpoiker` 2, **`#ifndef fpoiker` 3** (`AIUtilities.c:3734`, `mainrgn.c:4538,4601`) | a few survive verbatim in GoK (`#ifdef bpasechn` in `Tactics.c:1341`, `CommandLayer.c:1637`; `#ifdef khent` in `Damage.c:229`) | |

For a decompilation this means: a shipped Relic binary contains the `HW_Release` (or `HW_Interim`/`HW_Debug`) branch of every such `#if`, plus whatever the build account's username enabled. Code under `#ifndef <developer>` is normally **present**. Which account built Cataclysm's release exe is UNVERIFIED.

**Pointer-size assumptions that needed fixing** are listed in A5 (`memsize`, `*_disk`, `ShipType`, ETG parameter offsets, memory cookie, `time_t`). The common root pattern in Relic's code is a `udword` field that holds either a file offset (fixed up to a pointer after load) or a pointer cast to an integer. A decompiled Cataclysm will show the same pattern as `int` fields being dereferenced; each one needs a `memsize` or a `*_disk` split for a 64-bit build.

### A6. Inline assembly removed

Original asm sites (`git grep _asm` on `ORIG@ad1bb82`):
- `src/Game`: `task.c` (scheduler, ~13 blocks), `etg.c` (function call, register save), `matrix.c` (3x3 multiply), `memory.c`, `file.c` (`mov spp,esp`), `transformer.c` (asm + **Katmai/SSE intrinsics** selected by `useKatmai`), `univupdate.c`, `formation.c`, `region.c`, `netcheck.c`, `knitransform.cpp`, `AIFleetMan.c`, `CommandLayer.c`, `CommandNetwork.c`, `Debug.c`, `FeFlow.c`, `HorseRace.c`.
- `src/Win32`: `glcaps.c`, `glcompat.c`, `gldll.c`, `main.c`, `render.c`, `texreg.c`, `wassert.c`, `rad.h`.
- `src/rgl`: 30 asm lines.

Port steps:
1. GCC inline-asm transliteration (2003-2008; `1e45b0c0` local labels; `28547f7b` Mac stack alignment in `etgFunctionCall`).
2. C task manager (`ec6a8042`, 2007).
3. Generic ETG call (`GENERIC_ETGCALLFUNCTION`; `f6648dd6`, `096f993c`).
4. "Removing all leftover assembly instructions" (`e633a03b`, 2024-01-29: `Matrix.c` -249 lines, `ETG.c` -150, `NetCheck.c`, `Debug.c`, `texreg.c`).

What remains (`GoK:src/Game/Matrix-mult.c`, `src/SDL/win32glue.c`) is dead code: neither file is listed in any `meson.build`, `Makefile*` or `*.am`, and nothing `#include`s `Matrix-mult.c` (checked with `grep -rn 'Matrix-mult'`).

**Task system detail.** The original `taskYield(n)` is `((void (*)(void))taskFunctionContinue)()`: it jumps into the scheduler, which saved ESP/EIP in asm (`ORIG:src/Game/task.h:165`, `task.c:255-625`). The port's `taskBegin/taskYield/taskEnd` (`GoK:src/Game/Task.h:150-180`) store `__LINE__` in a per-task context and `switch` back to it. As a result, **local variables do not survive a yield** in the port: task functions must keep their state in the context struct. A decompiled Cataclysm task function will show calls through `taskFunctionContinue` that never return normally.

### A7. Floating point and determinism

| Concern | Original | Port | Evidence |
|---|---|---|---|
| FPU mode | No `_controlfp`/`fldcw` anywhere in Game/Win32 (`git grep` on `ORIG`; only `src/rgl/fxdriver.c:118,123` and `scan.c:98,106` touch the control word). Relies on x87 defaults plus one identical binary on all clients | x86_64 SSE scalar math (GCC default); x86 builds use `-msse` | meson.build:121-123 |
| x87 precision vs D3D | Direct3D lowers x87 precision to 24-bit unless told not to (documented D3D device-creation behaviour, not tested here). **`hw_rgl/D3D9/d3dinit.cpp:141` passes `D3DCREATE_FPU_PRESERVE` with the comment "Audio and various other things will break in Cataclysm if D3D changes the FPU precision"** | n/a (no D3D) | So Cataclysm depends on the CRT-default 53-bit x87 precision. Inference, UNVERIFIED: the `FAST_TO_INT` 1.5·2^52 trick needs at least 53-bit adds and breaks at 24-bit. A native port must pin `double` semantics (SSE2 doubles, or x87 PC=53 on 32-bit builds) |
| Task tick | ~240 Hz from QPC (`UTY_TimerResloutionMax 240`) | 250 Hz at 16 Hz sim, 1000 Hz at 64 Hz, from integer ms division | see A3 Timing row |
| Sim-critical files | `physics.c univupdate.c etg.c task.c particle.c tactics.c captaincy.c demo.c feflow.c key.c speechevent.c render.c` compiled **unoptimized MSVC `-Od`** in release (`CCMD0`); all other C used Intel `icl /Ogitb1 /GsF` | Optimized like everything else | `ORIG:src/Win32/Release.depend` |
| Sim rate | 16 Hz (`UNIVERSE_UPDATE_RATE 16`, `ORIG:src/Game/universe.h:57`) | 64 Hz default: `UNIVERSE_UPDATE_RATE_ORIG*2^SHIFT`, `UNIVERSE_WOODPECKER` adapts `% N` schedules; `/vanilla` → shift 0 (the comment at `Universe.h:45` still says `/legacy`; the real flag is `/vanilla`, `main.c:848`) | `GoK:src/Game/Universe.h:44-53`, `Universe.c:75`, `main.c:848,2195-2197`; commit `e3aa4e25` (2024-03-03). CET does the same for Cataclysm (`CET:Config.h:7-33`) |
| Float→int | `FAST_TO_INT` = add 2^52·1.5 double and read low dword (**round-to-nearest**) | Plain casts (**truncate**); colour only | `ORIG:src/Game/shader.c:38`; `GoK:src/Game/Shader.c:498`; CHANGELOG 0.3 |
| `sqrt` | `fmathSqrtDouble` | `sqrtf` | `ORIG:src/Game/fastmath.h:18`; `GoK:src/Game/FastMath.h` |
| RNG | `ranRandom` streams; one stream advanced by wall-clock ticks | `SDL_GetTicks()` seeding kept | `GoK:src/Game/Randy.c:209-215` |
| Multiplayer sync | Lockstep; `COMMANDTYPE_DETERMINISTICBUILD`; sync dumps | Same code; compatibility = same `build_version` string; `/debugSync`, `syncDumpInit` debug aids | `GoK:src/Game/CommandNetwork.c:1307-1726`, `main.c:492,578` |
| Save games | `SAVE_VERSION_NUMBER_ORIGINAL 0x10ad0027` = memory dumps | Only `HWSDL_1 0x10ad0028` / `HWSDL_2 0x10ad0029` loadable | `GoK:src/Game/SaveGame.h:16-22`, commit `98726fa4` |

GoK makes no bit-exact claim versus the 1999 executable. Replays, demos, packet recordings and MP sync with the original binary are not expected to work (UNVERIFIED by test; inferred from the above).

### A8. Asset paths and case sensitivity

- **Case-insensitive lookup** is on everywhere except `_WIN32` and `_MACOSX` (`GoK:src/Game/File.h:27-31`).
  - `fileNameCorrectCase` (`File.c:342-622`) reduces `..`/`.`, walks each path component with `opendir/readdir` + `strcasecmp`, and rewrites the string to the on-disk case.
  - Slashes are normalized by `fileNameReplaceSlashesInPlace` (`File.c:116`) and `filenameSlashMassage` (`BigFile.c:104`).
- **Inside `.big`**: names are stored as two CRC32s of the **lowercased**, backslash-normalized name, plus a length (`BigFile.h:88-102`, version notes 1.10/1.14/1.22: "Encrypted filenames"). Archive lookups are therefore case-insensitive by design.
  - Format tag: `"RBF"` + `"1.23"` (`BigFile.h:67,76`).
  - TOC entries are `fread` as raw structs (`BigFile.c:418`, `sizeof(bigTOCFileEntry)`). Layout (natural alignment, identical under `-Zp4`; checked with gcc): `nameCRC1 @0, nameCRC2 @4, nameLength (u16) @8, pad @10, storedLength @12, realLength @16, offset @20, timeStamp @24, compressionType (char) @28, pad`. That is **32 bytes** per entry, and **36** for `bigTOCFileEntryRemaster`. Names are lowercased before the CRC (`BigFile.c:570`, `tolower`). The name strings stored before each file's data are XOR-"encrypted" (`bigFilenameEncrypt/Decrypt`, `BigFile.c:668,684`).
  - The Remaster's `homeworld.big` adds an extra `udword` per TOC entry with the same version number (`bigTOCFileEntryRemaster`, `BigFile.h:104-121`; commit `f1fd6778`, Rafael Brune, 2021-08-31).
  - **How the variant is detected (added by the verifier):** not from the header but from a magic file count. `bigTOCRead` reads 36-byte Remaster entries only when `toc->numFiles == 13887`, and 32-byte entries otherwise (`BigFile.c:400-440`, test at `:416`). The header check is an exact string compare against `"RBF" "1.23"` (`bigHeaderVerify`, `BigFile.c:499-512`), used by the game (`:2542`) and by every biggie path (`bigView` prints `ERROR: Incompatible file` at `:2161-2166`). Consequence for Cataclysm: an archive with any other version string is rejected outright, and one with an unknown TOC entry size is misparsed silently unless its file count happens to be 13887.
  - The archive *file itself* must be named `Homeworld.big` with a capital H (`installation-manual.md:51`).
- **Precedence**: loose files in the override path, then `Update.big` (optional), then `Homeworld.big` (required) (`BigFile.c:72-95`). `/ignoreBigfiles` and `LogFileLoads` exist for debugging. The flags `FF_IgnoreBIG FF_IgnoreDisk FF_CDROM FF_UserSettingsPath FF_HomeworldDataPath` route each open (`File.h:48-60`, `filePathPrepend File.c:1889`).
- **Locations**:
  - Data: `$HW_Data`, else cwd (`utility.c:3534-3555`).
  - Settings, saves, screenshots, logs: `~/.homeworld` (`regkey.h:34`; `utility.c:3577-3605`, `snprintf ... CONFIGDIR` at `:3599`). On Windows the data dir is used, falling back to `C:\HomeworldData` (a "HACKHACK", `:3589`).
- **Required HW1 assets** (`installation-manual.md:26-57`, with sha256s): `Homeworld.big`, `Update.big` (1.05 patch), `HW_Comp.vce` (speech), `HW_Music.wxd` (music), optional `Movies/*.bik`.
- The 1.06 "Classic" (Remaster) assets differ from 1.05. GoK supports both but deliberately does not ship the 1.06 changes (`design-and-architecture.md:103-126`).

### A9. Enhancements versus Relic's original

From `GoK:zarchive/documentation/HWSDL_enhancements.txt`, `GoK:zarchive/CHANGELOG`, `GoK:documentation/contributors/design-and-architecture.md` and git log:

- **Display**: resolutions above 1600x1200; widescreen and corrected mouse/move-disc alignment; desktop-fullscreen; resizable window (wasm); MSAA up to 8x; vsync (`73e50e04`).
- **Input**: 3D/CAD joystick (SpaceNavigator) camera.
- **`Homeworld.cfg` `[New HWSDL Options]`**:
  - `CollectResourcesAtEndOfMission` (auto-harvest on hyperspace).
  - `ShipsAlwaysUseOwnerColors` (repaint captured ships).
  - `TimeCompressionFactor` (default 8, "the same as in Homeworld Cataclysm", toggled with `\`).
- **Simulation**: 64 Hz universe update (`/vanilla` for 16 Hz).
- **UI**: Build Manager ShipView auto-rotation (CHANGELOG 0.3); `Warp.script` mission-skip debug helper (`SinglePlayer.c`).
- **Assets**: Remaster `homeworld.big` support; original 32-bit assets on 64-bit; stb_image replacing the JPG library (`19d56f91`, `7d004b25`); FFmpeg Bink playback (`4931ce49`).
- **Platforms**: Linux x86/x86_64/aarch64/ARM, macOS (PPC, then Intel + ARM64 universal, `09567894`), Windows mingw64, wasm32/emscripten (`fc15dda4`).
- **Saves**: portable save format (`SAVE_VERSION_NUMBER_HWSDL_*`), saves in `~/.homeworld`.
- **Tooling**: ASan/UBSan default, `-Werror`, clangd/clang-format config.

### A10. What was removed or stubbed

| Removed | Replacement |
|---|---|
| WON/Titan internet play, CD-key auth (`ClientCDKey`, `Md52`, `Lib/Titan/*.lib`), `TitanInterface.cpp` | Stub, plus an optional SDL_net LAN stack (off in meson) |
| rGL: software rasterizer, D3D driver, 3dfx Glide (`src/rgl`, `sstglide.c`, `include/glide2`) | Direct OpenGL / GLES |
| DirectDraw (`ddraw.cpp`, `dxdraw.cpp`) and the `glcompat.c` "fast frontend" | n/a |
| Bink/Smacker SDK (`binkw32`) | FFmpeg |
| Relic JPG library (`src/JPG`) | SDL2_image, then stb_image |
| Registry | `Homeworld.cfg` |
| CD checks / CD validation | Compiled out |
| Win32 debug window | Windows only |
| Sigs (`src/Sigs`, 4 files) | Still in `zarchive` |
| Original `.obj` audio codec | RE'd C |
| `SAVE_VERSION_NUMBER_ORIGINAL` loading | n/a |

### A11. Original Relic tools and material in the repo

Ported or buildable (`GoK:tools/`):

| Tool | What | Builds here? |
|---|---|---|
| `kas2c` (flex/bison) | KAS mission script → C (Relic "KAS2C Version 2.06noleak", modified for GCC `-E` output and portable saves) | Yes, by default |
| `biggie` | `.big` (RBF1.23) view/extract/create/patch, compressed with `ThirdParty/LZSS` | Yes (`meson compile -C build-hwsdl biggie`); usage text verified |
| `polycount` | Mesh polygon counter | **No** (`mex.c:35` implicit `malloc`, `-Werror`) |
| `monochrome-btg` | BTG background tool | **No** (`PATH_MAX` undeclared) |
| `x86_64/{bank,lif,nis}` | Obsolete 2008 64-bit asset converters | Yes |
| `x86_64/{fib,font,ll,mad}` | Same | **No** (typedef clash with in-engine `*_disk`) |
| `btg/` | Relic BTG background editor (MFC, VC project) | Not on Linux |
| `liflist`, `convertors`, `editors`, `miscellaneous` | Assorted (autotools era) | Not in meson |

Archived, unported (`GoK:zarchive/original-release-snapshot/`):
- `tools/bin/*.exe` prebuilt Win32 tools: `Biggie BigTree BigViewer BTG builder CDKeyer Fontastic FXFont FXLate FXTract HWDetect HWVideoTweaker IniScript LWExport MissionMan MMHash NetLogView Paco Patcher PktView Plumber polycount PushHWBuild PushPatch Roofer Scaly SEDemo SetLang SFXTool SPTool SPVal stackdump statviewer teapot VolTool WAVExport wmake`, plus `bison.exe` and `flex.exe`. The verifier's `ls` also shows `AddValidVersion`, `controls`, `cw3230mt` and `perl`.
- `tools/win32/*` sources of the above, triaged Y/N/? in `GoK:tools/README.txt`.
- `documents/technical/*.doc`: `AI Scripting System Specs`, `BattleChatter`, `BuildingHomeworld`, `CompilingHomeworld`, `Computer_Player_Commands`, `ETGUserGuide`, `FEManBinary`, `FEManLocalize`, `FontSpec`, `Homeworld_debug`, `KAS Reference v2`, `MeshAnim`, `MeshExtensions`, `NISPipeline`, `TextureMapping`.
- `src/Titan` (WON C++), `src/SinglePlayer` (original KAS/mission data), `src/Sigs`, `src/Win32` leftovers (`bink.c`, `gldll.h`, `MAKEFILE`, `*.depend`, `vc50.pdb`).
- `Lib/Titan/*_W95.lib`, `Lib/Smacker/binkw32.lib`.
- `exe/Homeworld.exe` + `binkw32.dll`: **the symbolized debug build, see section 0, item 2.**
- `EB Levels/` (Gas Giant / Belly of the Beast levels).
- `sound/`: Relic's sound-design **Access databases** (9 `.mdb`): `speech/Speech.mdb`, plus `SFX/{guns,ships,UI,Special FX}/*_{SFX,Bank}.mdb`. These are the authoring sources behind the sound banks and speech tables that `SoundEvent.c`/`SpeechEvent.c` load. They are useful when checking whether Cataclysm's bank and event-table formats changed. Added by the verifier.
- `BuildingHomeworld.doc` (also at the snapshot root).
- `obj/{Debug,Interim,Release}/{dct,fqcodec,fqeffect,fquant}.obj`.

Community format specs: `GoK:zarchive/documentation/File Formats/{BIG,GEO,LiF,MAD,MEX} Specification.txt`, plus `Map Editing/*.txt`.

### A12. Licence

`GoK:COPYING.REULA` is the same text as `HWSDL:LICENSE.txt` minus the HWSDL preamble. `GoK:COPYING` adds a preamble. `GoK:documentation/contributors/license-agreement.md` explains it.

REULA terms:
- §1.1 grants use/reproduce/modify **"solely for non-commercial purposes"**. Distribution is allowed only "to registered members of RDN ... who have agreed to abide by" it.
- "Non-commercial" means **free of any charge and with no direct or indirect financial consideration to anyone**.
- §2.1: all modifications ("MODIFIED SOFTWARE") are **assigned to RELIC**; moral rights are waived.
- §3.1: five-year term from download. §3.2: terminable at will, and automatically on breach; then destroy all copies.
- §5.2: no circumvention of copy protection.
- §7.5: British Columbia law.
- No warranty.

GoK's position:
- It is not OSI open source.
- It disregards the defunct-RDN distribution clause and hosts publicly.
- Each contributor implicitly agrees to the REULA, and new code is **also MIT** (`COPYING.MIT`; "All commits after 0e40bdd0 are licensed under MIT", `license-agreement.md:34`). Historic contributors opt in by adding their names to `COPYING.MIT`. Today it lists only `Copyright (c) 2019-2026 Thibault Lemaire`, and `git rev-list --count 0e40bdd0..HEAD` = 4 commits (`36c23071 4f4a81b1 c6cafc8d 3af31fbc`, all Lemaire). `0e40bdd0` itself is author-dated 2026-08-20.
- Code from the first commit stays REULA. GoK asks contributors not to reformat untouched code, so that `git blame` still shows which lines are Relic's.

**Implication for Homeworld-Infection:**
- Reusing GoK code means REULA (non-commercial) for the Relic-derived parts. Only the MIT-tagged contributions are freely reusable: Lemaire's commits and anything after `0e40bdd0`. Everything else in `src/SDL` and the `*_disk` loaders (aunxx, lauriebradshaw, Rafael Brune, ...) is community code with no stated licence beyond the REULA chain.
- A Cataclysm decompilation is a separate, unlicensed derivative of Sierra/Relic IP (Gearbox now). CET calls its own status "a legal gray area" (`CET:README.md` Legal).
- Keep the project free and non-commercial. Never redistribute game assets.

---

## Part B: Native Linux build (toolchain proof)

### B1. Environment

Pop!_OS 24.04 base (Ubuntu noble packages), gcc 13.3.0, clang 18.1.3, meson 1.3.2, ninja 1.11.1, pkg-config 1.8.1.

Already installed: `libsdl2-dev 2.30.0`, `libgl-dev`, `libx11-dev`, `libavcodec-dev`, `libavutil-dev`, `libswscale-dev 6.1.1`, `libasan8`, `libubsan1`.

Missing: `bison`, `flex`, `libavformat-dev`.

Source: GoK `3af31fb`, so the build version string is `3af31fb` (a shallow clone has no tags for `git describe`).

### B2. Commands and outputs (verbatim)

All commands were run from `/home/evanmedinger/Documents/GameDev/Homeworld-Infection/reference`. The clone was not modified. The build directory is out of tree.

**Step 1.** `meson setup build-hwsdl gardens-of-kadesh` at 2026-10-08T09:21:59-06:00. It failed:

```
Run-time dependency libavutil found: YES 58.29.100
Run-time dependency libswscale found: YES 7.5.100
Run-time dependency libavcodec found: YES 60.31.102
Found CMake: /usr/bin/cmake (3.28.3)
Run-time dependency libavformat found: NO (tried pkgconfig and cmake)

gardens-of-kadesh/meson.build:170:2: ERROR: Dependency "libavformat" not found, tried pkgconfig and cmake
```

Flex and bison were also missing (`tools/kas2c/meson.build:3-4` declares both `required: true`). They would have been the next failure.

**Step 2.** `sudo -n apt-get install -y bison flex libavformat-dev`. It succeeded:
- NEW: `bison flex libaacs0 libavformat-dev libavformat60 libbdplus0 libbluray2 libcjson1 libfl-dev libfl2 libmbedcrypto7t64 libnorm1t64 libpgm-5.3-0t64 librabbitmq4 librist4 libsodium23 libssh-gcrypt-4 libudfread0 libzmq5`
- Upgraded: `libssh-4`
- Summary line: "1 upgraded, 19 newly installed, 0 to remove". bison now provides `/usr/bin/yacc`.

**Step 3.** `meson setup --wipe build-hwsdl gardens-of-kadesh`. It succeeded:

```
Message: Build version: 3af31fb
gardens-of-kadesh/meson.build:33: WARNING: Building with memory sanitizers on. If linking fails, try disabling them with `meson configure -Db_sanitize=none`.
Run-time dependency sdl2 found: YES 2.30.0
Run-time dependency gl found: YES 1.2
Library m found: YES
Run-time dependency x11 found: YES 1.8.7
gardens-of-kadesh/meson.build:162: WARNING: Building with video cutscenes support. ...
Run-time dependency libavformat found: YES 60.16.100
Program flex found: YES (/usr/bin/flex)
Program bison found: YES (/usr/bin/bison)
Build targets in project: 34
```

**Step 4.** `meson compile -C build-hwsdl` at 09:22:16 → 09:22:24. It succeeded: `[292/292] Linking target homeworld`, `real 0m7.633s user 1m44.031s`, exit 0, **0 warnings** (the build uses `-Werror`).

### B3. Result: binary produced

| Artifact | Path | Size | `file` |
|---|---|---|---|
| **Game (debug + ASan/UBSan, default)** | `reference/build-hwsdl/homeworld` | 74,878,048 B | `ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=321587a20f938df07532a71b59750d4150987844, for GNU/Linux 3.2.0, with debug_info, not stripped` |
| kas2c | `reference/build-hwsdl/tools/kas2c/kas2c` | 1,448,312 B | ELF x86-64 |

- sha256 of `homeworld`: `fd49b2d01dedc26a0febb5c66920cd2cb773eccd95078c3e467db148058af516`.
- `ldd homeworld` (abridged): `libasan.so.8 libubsan.so.1 libGL.so.1 libSDL2-2.0.so.0 libX11.so.6 libavutil.so.58 libswscale.so.7 libavcodec.so.60 libavformat.so.60 libm libc`. SDL pulls in X11 and Wayland client libraries (`libwayland-client`, `libdecor-0`, `libxkbcommon`).
- Generated mission sources are in `build-hwsdl/homeworld.p/MissionNN.{c,h,func.c}`, for example the header "autogenerated from Mission01.kp by KAS2C Version 2.06noleak", with `Mission01_FunctionPointers[]`.
- **Not run**: no assets, per the task rules.
- For a later smoke test: `HW_Data=<assets> ./homeworld /window`. **`/window` is mandatory**: the default is fullscreen (`main.c:690-691`, `SDL_WINDOW_FULLSCREEN_DESKTOP`, `render.c:913-914`). `SDL_VIDEODRIVER=x11` is not needed for the lock-key code, which is dead on Linux (A3). SDL 2.30.0 on this session already picks `x11` (Xwayland) by default. The verifier checked this with a no-window probe (`SDL_Init(SDL_INIT_VIDEO)` → `SDL_GetCurrentVideoDriver()` = `x11`; driver order `x11 wayland KMSDRM offscreen dummy evdev`).

### B4. Extra builds (scratch only, not in the repo tree)

| Build | Command | Result |
|---|---|---|
| GoK release | `meson setup --buildtype=release -Db_sanitize=none <scratch>/gok-release gardens-of-kadesh && meson compile -C <scratch>/gok-release` | OK. 5,157,976 B ELF x86-64, not stripped |
| HomeworldSDL mirror (debug+ASan) | `meson setup <scratch>/hwsdl-mirror HomeworldSDL && meson compile ...` | OK. 75,020,584 B |
| GoK with clang 18, try 1 | `CC=clang meson setup -Db_sanitize=none --buildtype=release ...` | **FAIL**, 42 errors, e.g. `error: unknown warning option '-Wno-stringop-truncation'; did you mean '-Wno-format-truncation'? [-Werror,-Wunknown-warning-option]` (also `-Wno-stringop-overread`, `-Wno-restrict`, `-Wno-multistatement-macros`, `-Wno-maybe-uninitialized`, `-Wno-discarded-qualifiers`) |
| Clang try 2 | Add `-Dc_args=-Wno-unknown-warning-option` | **FAIL**, 12 errors, e.g. `tools/kas2c/parser.y:35:5: error: a function declaration without a prototype is deprecated ... [-Werror,-Wdeprecated-non-prototype]`, `src/SDL/sstream.c:1174:26: error: assigning to 'sbyte *' ... from 'ubyte *' ... [-Werror,-Wpointer-sign]`, `src/SDL/smixer.c:461:29: error: use of logical '&&' with constant operand [-Werror,-Wconstant-logical-operand]` |
| Clang try 3 | `-Dc_args="-Wno-unknown-warning-option -Wno-deprecated-non-prototype -Wno-pointer-sign -Wno-constant-logical-operand -Wno-invalid-source-encoding -Wno-uninitialized -Wno-sometimes-uninitialized -Wno-non-literal-null-conversion"` (the same set meson.build already uses for emscripten) | **OK**. 4,770,080 B |

Optional tools: `meson compile -C build-hwsdl polycount biggie monochrome-btg bank fib font lif ll mad nis`, then each target individually with `ninja -k 0`.

| Target | Result | First error (verbatim) |
|---|---|---|
| biggie | OK (589,816 B; prints `Biggie - version 3.00 [RBF1.23]` usage) | |
| bank, lif, nis | OK | |
| polycount | FAIL | `tools/polycount/mex.c:35:12: error: incompatible implicit declaration of built-in function 'malloc' [-Werror=builtin-declaration-mismatch]` |
| monochrome-btg | FAIL | `tools/monochrome-btg/monochrome-btg.c:9:29: error: 'PATH_MAX' undeclared (first use in this function)` |
| fib | FAIL | `tools/x86_64/fib.h:6:16: error: redefinition of 'struct tagfescreen_disk'` (originally defined at `src/Game/FEFlow.h:214:16`) |
| font | FAIL | `tools/x86_64/font_l.h:11:1: error: conflicting types for 'charheader_disk'` (vs `src/SDL/font.h:130:1`) |
| ll | FAIL | `tools/x86_64/ll.h:15:1: error: conflicting types for 'llelement_disk'` (vs `src/SDL/texreg.h:257:1`) |
| mad | FAIL | `tools/x86_64/mad.h:14:1: error: conflicting types for 'madobjpath_disk'` (vs `src/Game/MeshAnim.h:158:1`) |

Not attempted:
- 32-bit (`-m32`) build. It would need `gcc-multilib` and `libsdl2-dev:i386`; the i386 architecture is already enabled in dpkg.
- mingw cross-build for Windows (no mingw-w64 installed).
- emscripten.

### B5. Time spent

About 2 minutes of build time, including package installation. The build path is healthy on this machine.

---

## Part C: Lessons for the Cataclysm recreation

### C1. Port tasks HW1 needed that a Cataclysm decompilation will repeat

Ordered roughly by when they block a first stable native build.

1. **Recover a compilable source tree from the binary.** HW1 had source. Cataclysm needs decompilation into the HW1 module layout (`Universe.c`, `UnivUpdate.c`, `ETG.c`, `Mesh.c`, `KAS*.c`, `Ships/*.c`, ...).
   - Seed types and names from the HW1 tree and from the **symbolized `exe/Homeworld.exe`**: NB11 global types, -Zp4 layouts, ~6.8k source function names with proc records.
   - Build a Ghidra data-type archive from the HW1 headers compiled with `-m32 -fpack-struct=4`-equivalent packing (MSVC `-Zp4`). Whether Ghidra imports NB11 directly is UNVERIFIED.
   - Use CET's reconstructed headers (`CET:game/{Game,rgl,Win32}`) and the **151 distinct addresses** in `CET:Cataclysm.h` as anchors: 44 `DEFINE_ADDRESS` patch sites, 55 `DEFINE_FUNCTION` function addresses, and 52 typed global pointers such as `universe` 0x00B62A00. The first draft said 45. Note that many CET game headers are verbatim HW1 copies (e.g. `task.h`), so a CET struct layout is not proof of Cataclysm's layout.
2. **Win32 platform layer → SDL2.** Repeat A3 for Cataclysm's WinMain, message pump, registry, `QueryPerformanceCounter`, threads, DirectSound and CD checks. Most of `GoK:src/SDL` drops in (C2). Expect Cataclysm-specific additions: its registry key (likely `Sierra On-Line\Homeworld Cataclysm`, UNVERIFIED), CET-fixed DirectSound path (`CET:PatchSound.cpp`), and resolution handling.
3. **rGL → OpenGL.** Cataclysm calls the `rgl.dll` GL subset plus `rgl*` extensions (A4).
   - Route those calls to GL via SDL (GoK `render.c`, `rinit.c`, `rglu.c`, `prim2d.c`, `prim3d.c`, `texreg.c`).
   - Keep the **CPU vertex shading path** (lesson of `25478715`).
   - Reference: `reference/hw_rgl` (D3D9) for the exact rGL semantics Cataclysm expects.
   - Any `rgl*` export Cataclysm calls that HW1 did not must be found in the binary (blocked).
4. **Replace inline asm.** Expect the same hot spots as HW1:
   - The task scheduler: convert to GoK's `__LINE__` coroutines, and move yield-surviving locals into the task context.
   - `etgFunctionCall`: GoK `wrap.h` wrappers, with a regenerated `functions.h` for Cataclysm's ETG function table.
   - Matrix/transform asm and Katmai intrinsics: use the portable C in `GoK:src/Game/Matrix.c`, `Transformer.c`.
   - `memory.c`, `file.c` stack tricks.
   - Decompiled asm-calling-convention code (custom pushes before indirect calls) will look broken until these are recognized.
5. **KAS missions.**
   - Reconstruct each Cataclysm mission as `.kas`, or keep it as decompiled C following kas2c's output shape: `Init_<Mission>_<FSM>_<State>` / `Watch_...` pairs and `LSTRING_*` multilingual string arrays.
   - Rebuild `MissionNN_FunctionPointers[]` for portable saves.
   - kas2c output is highly regular, so it can serve as a pattern-matching template.
   - In HW1's release build the generated mission C was compiled with plain `cl` and no optimisation switch (section 0, item 3). If Cataclysm kept that makefile, its mission functions are unoptimised MSVC code, the easiest code in the exe to match and lift.
6. **64-bit asset loading** (if targeting x86_64, which Windows 11 and modern Linux want).
   - Add `*_disk` structs and load-time conversion for every Cataclysm binary format that stores offsets: `.geo` (Cataclysm may use newer GEO/MEX versions, UNVERIFIED), `.mad`, `.nis`, `.fib`, `.lif/.ll`, `.hff` fonts, sound banks.
   - Use `memsize` wherever the code stuffs pointers into `udword` (ETG variables, `ShipType`-like fields, KAS variables, effect params via `SCALECAST`).
   - Alternative: ship a 32-bit (`-m32`) build first. Data structs then overlay directly, **provided** the build drops GoK's `-malign-double` (meson.build:123). Plain i386 SysV already aligns `double`/`long long` struct members to 4, matching `-Zp4`; with `-malign-double` any struct holding one shifts (verifier probe, Open questions). A global `-fpack-struct=4` is not needed for that and would also repack system-header structs. That is lower risk for "stable version first", but 32-bit Linux userland is shrinking.
7. **Endianness.** Not needed for x86/ARM-LE targets. Still add the `FIX_ENDIAN_*` hooks where GoK has them (A5b inventory. Readers: SoundEvent, Mesh, NIS, BTG, texreg, FEFlow, BigFile, MEX, MeshAnim, font, SpeechEvent, Light, render (TGA), sstream. Packed-colour/flag fixes, not readers: Particle, ETG, Color.h, Trails.h, Tutor). Take base types from GoK `Types.h` (fixed-width `udword`), not from CET/Ghidra `unsigned long`. The reader list is an **inventory of the binary readers that decode multi-byte fields**; formats parsed as text, or read byte by byte, cannot appear in it by construction (verifier: the first draft called it complete and listed Particle/ETG as readers).
8. **Case-insensitive files and paths.**
   - Reuse `File.c` (`fileNameCorrectCase`), `BigFile.c` and the `HW_Data`/`~/.<config>` scheme.
   - Cataclysm's archive and file names (e.g. `Cataclysm.big`, `Update.big`; CET redirects to `Update102.big`, `CET:GameHooks.cpp:12-129`) need their own `bigFilePrecedence[]`.
   - Verify the big format version and TOC entry layout on the real files (UNVERIFIED that Cataclysm uses RBF1.23 unchanged).
9. **Audio.** HW1's codec existed only as `.obj` and was reverse-engineered (A3); the result is in GoK.
   - If Cataclysm's speech and music formats are the same (`.vce`, `.wxd`, sound banks; UNVERIFIED), reuse `soundlow/smixer/sstream/fq*/dct/mixfft` directly.
   - If not, decompile from Cataclysm's binary.
10. **Movies.** Replace Bink SDK calls with FFmpeg (`avi.c`). Check whether Cataclysm's `.bik` files are Bink 1, which FFmpeg decodes (UNVERIFIED).
11. **Networking.** Stub WON/Titan as GoK did. LAN can come later (SDL_net path exists but is off).
12. **Saves.** Cataclysm saves are almost certainly raw-memory-pointer dumps like HW1's `0x10ad0027`. Define a new portable save version from day one and do not promise compatibility with original saves.
13. **Determinism and fidelity decisions.**
    - The 16 Hz universe rate is the "1:1" baseline. 64 Hz is what CET and GoK offer as an enhancement, with known timer side effects (`CET:README.md` Known Issues). Keep `/vanilla`-style switching.
    - Watch x87 vs SSE differences, especially in the files Relic compiled `-Od` (A7) and in `FAST_TO_INT`-style rounding idioms in decompiled code. Cataclysm is known to break if x87 precision drops to 24-bit (`hw_rgl/D3D9/d3dinit.cpp:141`, A7). Keep all `double` math at real double precision.
    - Task tick rate: the original runs tasks at ~240 Hz from QPC. GoK's millisecond integer division gives 250 Hz (16 Hz sim) or 1000 Hz (64 Hz sim) (A3 Timing). For a 1:1 baseline, derive ticks from a high-resolution counter (`SDL_GetPerformanceCounter`) the way the original did. Translate the `fadd 6755399441055744.0; mov eax,[tmp]` pattern to `lrint`-style round-to-nearest, not `(int)` truncation.
14. **Build hygiene.** Copy GoK's meson setup:
    - Sanitizers on by default (this is how the community found the `memcpy`/`memmove` explosion crash, CHANGELOG 0.2).
    - `-Werror` with an explicit suppression list.
    - Generated KAS/ETG sources.
    - mingw64 and meson-on-Windows for the Windows 11 target.
    - Make the suppression list compiler-aware (`-Wno-unknown-warning-option` for clang), unlike GoK (Part B).
15. **Wayland and NVIDIA on this machine.** The GoK window/GL path is SDL2 + legacy GL compatibility context, and SDL 2.30 defaults to the `x11` backend here (Xwayland; B3). The Xlib `XSendEvent` lock-key code (`utility.c:4912-5010`) is never called on Linux (A3). Do not copy it; if lock-key restore is wanted, gate it on `SDL_GetCurrentVideoDriver()=="x11"` so `SDL_VIDEODRIVER=wayland` stays safe.

### C2. HomeworldSDL / GoK files reusable as the Cataclysm platform layer

Legend:
- **W** = reuse wholesale.
- **A** = adapt (Cataclysm-specific constants or tables).
- **R** = reference only.

All of these are REULA-derived unless their `git blame` shows MIT-era commits. Per A12 those are Lemaire's commits or ones after `0e40bdd0`, which in practice is almost none of the files below.

| File(s) (GoK) | Role | Reuse |
|---|---|---|
| `src/SDL/main.c` | SDL entry, event pump, command-line table, activate/deactivate, renderer reload | **A** (window title, Cataclysm command-line flags, remove the `HW_GAME_*` variants) |
| `src/SDL/mouse.c` | Cursor, relative mode, scaling | **W** |
| `src/SDL/render.c`, `rinit.c`, `rglu.c`, `prim2d.c`, `prim3d.c`, `font.c`, `texreg.c`, `screenshot.c`, `glinc.h` | GL replacement for rGL; device enumeration; 2D primitives; texture registry; fonts; screenshots via stb | **A** (add the rGL entry points Cataclysm uses that HW1 did not; texreg/font `*_disk` for Cataclysm formats) |
| `src/SDL/soundlow.c`, `smixer.c`, `sstream.c`, `fqcodec.c`, `fqeffect.c`, `fquant.c`, `dct.c`, `mixfft.c` | SDL audio output, mixer, streaming, RE'd Relic codec | **W** if formats match (verify on the real `.vce`/`.wxd`), else **A** |
| `src/SDL/avi.c` | FFmpeg Bink playback | **W** (movie list differs) |
| `src/SDL/utility.c` | Game-system init/shutdown, `Homeworld.cfg` I/O, task dispatch timer, paths | **A** (largely game-specific init order; keep the timer, path and cfg parts) |
| `src/SDL/Queue.c`, `TimeoutTimer.c`, `Titan.c`, `TitanInterfaceC.c`, `NetworkInterface.c` | Net plumbing / stub / LAN | **W** (stub), **A** (LAN later) |
| `src/SDL/regkey.h` | Config dir / registry names | **A** (Cataclysm names) |
| `src/Game/Types.h`, `Types.c` | `udword` etc. on SDL types, endian macros, `memsize`, `SCALECAST` | **W** |
| `src/Game/File.c`, `File.h` | Case-insensitive VFS, path prepend, log files | **W** |
| `src/Game/BigFile.c/.h`, `src/ThirdParty/LZSS`, `src/ThirdParty/CRC` | `.big` reader with precedence and compression | **A** (precedence table, any TOC variant) |
| `src/Game/Task.c/.h` | Portable coroutine task manager | **W** (decompiled task bodies must be rewritten to the macro style) |
| `src/Game/Memory.c/.h` | Relic allocator, 64-bit cookie | **W** |
| `src/Game/LinkedList.c`, `Hash.c`, `Debug.c`, `Key.c`, `KeyBindings.c`, `StringSupport.c`, `Matrix.c`, `Vector.c`, `FastMath.h`, `Randy.c` | Engine utilities (asm-free) | **W** (verify Cataclysm's RNG constants and key tables match) |
| `src/Game/ETG.c/.h`, `wrap.h`, `wrap_types.h`, `functions.h` | Effect bytecode VM and portable call wrappers | **A** (regenerate `functions.h` from Cataclysm's ETG function table) |
| `src/Game/KAS.c`, `KASFunc.c`, `tools/kas2c/*` | Mission script runtime and compiler | **A** (Cataclysm KAS function set differs) |
| `src/Game/Mesh.c`, `MeshAnim.c`, `NIS.c`, `FEFlow.c`, `SoundEvent.c`, `BTG.c`, `MEX.c`, `Light.c` | Asset loaders with `*_disk` 64-bit conversion and endian fixes | **R/A** (templates; formats may have changed) |
| `src/Game/Universe*.c`, `Physics.c`, `AI*.c`, `Ships/*.c`, `SaveGame.c`, UI modules | Gameplay | **R** (Cataclysm diverged; decompile and diff against these) |
| `tools/biggie` | Inspect/extract `.big` | **W** (first tool to point at Cataclysm's archives once they exist) |
| `meson.build`, `meson_options.txt`, `Linux/BUILD.md`, `mingw/Makefile` | Build system | **A** |

### C3. Recommended first steps when the real installer arrives

1. Extract the installer with `innoextract`. First dump the first 16 bytes of each `.big` (`xxd -l 16`) to read the `RBF` + version string and `numFiles`/`flags`. Then run `reference/build-hwsdl/tools/biggie/biggie -v` on each to confirm the RBF1.23 / 32-byte TOC layout and the precedence order. biggie refuses any header other than exactly `RBF1.23` and only switches to the 36-byte Remaster TOC when `numFiles == 13887` (A8), so if the listing looks wrong, suspect a changed TOC entry size before suspecting the data.
2. Check the toolchain of `cataclysm.exe`: linker version, Rich header presence, CRT, and whether it was compiled with Intel `icl` like HW1. Compare idioms against `exe/Homeworld.exe`.
   - **First look for an embedded CodeView trailer.** The last 8 bytes of `Homeworld.exe` are `"NB11"` + a little-endian dword `n`, and the CV base is at `filesize - n` (0x4A9C00 there). Relic's Release level also linked with `/DEBUG /DEBUGTYPE:CV` (section 0, item 3), so a retail Cataclysm exe might still carry publics. Also check the PE debug directory (Homeworld.exe: RVA 0x356450, size 0x54).
   - Decode the Rich header and compare with Homeworld.exe's `(4,8168,13) (11,8447,47) (0,0,851) (6,1668,1)` (section 0, item 2).
3. Load `exe/Homeworld.exe` (symbolized) and `cataclysm.exe` into Ghidra. Transfer names and types by function similarity (BSim / Version Tracking). Start from the asm-free utility modules, the MSVC `-Od` modules (`task`, `etg`, `physics`, `univupdate`) and the unoptimised kas2c mission code, which are the easiest to match.
4. Wire a skeleton out of GoK's `src/SDL` plus the C2 **W** files, with decompiled modules replacing HW1 ones one at a time. Use the same meson and sanitizer setup.

---

## Open questions and UNVERIFIED items

- Whether Ghidra (or any available tool) imports CodeView **NB11** embedded in `exe/Homeworld.exe` directly. The parse above was a hand-written directory walk (`sstModule` = 0x120 etc.).
- RESOLVED by the verifier: SDL 2.30.0's default backend on this COSMIC session is `x11` (no-window probe, B3). The Xlib lock-key path is never called on Linux (A3). Still open: whether the game runs correctly with `SDL_VIDEODRIVER=wayland` (legacy compat GL context via EGL on NVIDIA 580). UNVERIFIED.
- PARTLY RESOLVED by the verifier: how `-malign-double` (meson x86 builds) interacts with Relic's `-Zp4` layouts. Probe (`gcc -m32 -S`, no headers needed) on `struct { char c; double d; long long q; }`: plain `-m32` gives `d`@4, `q`@12, size 20, identical to `#pragma pack(4)` (= MSVC `-Zp4`); with `-malign-double` it gives `d`@8, `q`@16, size 24. So `-malign-double` makes 32-bit GoK layouts **differ** from Relic's for any struct with a `double`/`real64`/`sqword`/`uqword` member. The on-disk structs that have them (`btgVertex`, `btgStar`: `udword flags; real64 x, y; ...`, `BTG.h:35-50`) are copied field by field, with the comment "Actual size from file. (No Alignment)" = `4 + 2*8 + 5*4` = 40 bytes per vertex (`BTG.c:666-668`, vertex copies `:677-702`, star copies `:753-774`; since `5d5cd3eb`, aunxx, 2007-03-22). Still open: any in-memory struct shared with original data via raw `fread`/`memcpy` (save games, network packets) that contains an 8-byte member. For a Cataclysm decompilation: assume 4-byte alignment for `double` inside structs.
- Every Cataclysm-side claim: `.big` version and names, KAS function set, sound and movie formats, compiler, registry key, extra rGL exports. All need the GOG offline installer.

---

## Verification

Independent check on 2026-10-08 against `reference/` (GoK `3af31fb`, HWSDL `1bed493`, CET `19a08c3`, hw_rgl `41ff3c3`), the full-history clone `gok-hist` in the build scratch dir (1091 commits, root `ad1bb82c`), the build logs `01`–`09c`, and the binaries in `reference/build-hwsdl/`. Nothing was run except: `biggie` with no arguments (usage text), a no-window SDL probe, a gcc `offsetof` probe, and Python parses of `Homeworld.exe`. No file in `reference/` was modified.

**Claims checked: 57 claim groups (about 140 individual facts).** Confirmed as written:
- clone HEADs, shallowness, root commit, `43ded70` stats.
- 29 commit hashes with their dates and subjects.
- NB11 offset and subsection counts (912/559/275 + globals).
- MAKEFILE flags and `-Zp4`; `Release.depend` split (244 `CCMD2` .c, 12 `CCMD0`, `trails.c` `CCMD1`, 12 `CCMD0CPP`).
- `task.c` asm (13 `_asm` blocks, 255-625) and `task.h:165`.
- GoK `Task.h:150-180`; `Types.h:55-63`, `117-123` (30 `SCALE*` uses).
- `Universe.c:75`, `Universe.h:44-53`, `/vanilla` at `main.c:848,2195-2197`; `SaveGame.h:16-22`.
- every `meson.build` line in the A2 table; `HW_ENABLE_NETWORK`/`GLES` absent from all `meson.build`.
- the `src/SDL` compiled and not-compiled lists; `main.c` 2135/1685/2034/2284-2306.
- `utility.h:39`, `soundlow.c:230`, `avi.c:420`, `Animatic.c:721-724`.
- `File.c:116/342/1889`, `File.h` flags, `BigFile.h` TOC and precedence.
- `Memory.h:86-95`, `ShipDefs.h:79-91`, `B-Spline.c:252-258`, `smixer.c:839`, `Particle.c:2064`.
- 184 `glBegin|glVertex3f|glEnd()`; the 16 Win32 API counts; the original and GoK tree file counts; the inline-asm site list.
- `CET:Config.h`, `GameHooks.cpp:12,108-129`, CET README install step and Known Issues.
- all Part B sizes, sha256, `file`/`ldd` output, timings, apt summary, clang error counts (42/12/0), tool errors.
- toolchain versions; REULA clause numbers; installation-manual, `flake.nix`, `BUILD.md` and `configure.ac` lines; `TimeCompressionFactor` default 8 (`Universe.c:102`).

**Claims wrong or overclaimed: 11, all fixed in place.**

| # | Was | Now | Evidence |
|---|---|---|---|
| 1 | 6439 of 6940 source function names in the NB11 info | 6809 of 6940 have an `S_GPROC32/S_LPROC32` record (11,204 procs, 24,648 publics) | Record-level parse of kinds 0x100A/0x100B/0x1009 in the sstAlignSym and sstGlobal* subsections |
| 2 | CET has "45 known addresses" | 151 distinct addresses (44 `DEFINE_ADDRESS`, 55 `DEFINE_FUNCTION`, 52 typed globals) | `grep` of `CET:Cataclysm.h` |
| 3 | "~35" `-Wno-*` | 37, plus 8 emscripten-only | `meson.build:59-99,102-113`; logged compile line |
| 4 | MSAA fallback 8→2→0 | 8→4→2→1, then off | `render.c:976-1001` |
| 5 | Xlib lock-key code is "called from `main.c:1140-1186`"; Wayland risk | Dead on Linux: the `main.c` calls are commented out, and `utility.c:4655` sits under `#ifndef _LINUX_FIX_ME`. B3 and C1.15 adjusted | `main.c:1139-1143,1183-1187`; `utility.c:4652-4657`; `meson.build:132` |
| 6 | `glinc.h:4-80` | `:4-216` | `#ifdef HW_ENABLE_GLES` ... `#else` at 216 |
| 7 | rgl.exp lists 20 `rgl*` extensions | 42 (`rgl.exp` is a binary COFF export object); `hw_rgl` coverage added | `strings` on `ORIG:src/rgl/rgl.exp`; set difference against hw_rgl |
| 8 | `*_disk` line ranges; "bank headers" | Exact typedef lines; banks are converted by `loadbankAndConvertTo64Bit`, with no `*_disk` type | `grep` of the headers; `SoundEvent.c:2580-2857` |
| 9 | CET `task.h` shows Cataclysm keeps the asm scheduler | CET's `task.h` is a verbatim HW1 copy, so the conclusion is likely but UNVERIFIED | `diff` against `ORIG:src/Game/task.h` |
| 10 | `MissionNN_FunctionPointers[]` described as part of how missions are linked into the exe | It is a HomeworldSDL addition; the original has no such table | `tools/kas2c/README.txt:1-6`; no match in `ad1bb82` |
| 11 | A1 dates the rebase to 2026-08 citing `zarchive/README.md` | README has no date; dated via `7adffa20` and committer dates | `git log` in `gok-hist` |

Clarified (not wrong): the link timestamp is 2003-08-22 00:29:35 UTC; the `memsize` count of 211/22 is for `memsize` alone; `filenameSlashMassage` is at `BigFile.c:104`.

**Added (missing from the first draft, all sourced):**
- Release-build details: 18 mission `.c` files compiled with plain `cl`; `shader.c`/`knitransform.cpp` with `icl -O2`; Release still links `/DEBUG /DEBUGTYPE:CV`.
- `Homeworld.exe` PE layout, Rich header and symbol-record counts.
- Task tick rate (original ~240 Hz vs GoK 250/1000 Hz).
- hw_rgl's `D3DCREATE_FPU_PRESERVE` note that Cataclysm breaks at reduced x87 precision.
- `.big` TOC entry layout (32/36 bytes) and name lowercasing/encryption.
- Scope of the MIT grant (4 post-`0e40bdd0` commits plus Lemaire; Brune's work is not MIT).
- SDL 2.30 default backend is `x11` on this session (probe); `/window` is mandatory because fullscreen is the default.
- The archived Relic sound `.mdb` databases.
- C3 check for a CodeView trailer in Cataclysm's exe.
- Continuation pass (2026-10-08): new section A5b (base-type table original vs port vs CET, endianness history and `FIX_ENDIAN*` per-file inventory, `HW_*`/port-define table with set-by and used-in), key finding 10, and the C1.7 cross-reference fix (it pointed at A6, which has no endian list). Sources: GoK `Types.h`/`Types.c`/`meson.build`, `gok-hist` commits `43ded70 9ad00155 a21dc1cb 1df2d4c5 97213da5 f47c87f6`, `ORIG@ad1bb82:src/Game/{TYPES.H,speechevent.h}`, `CET:game/Game/TYPES.h`.

**Remaining issues / still UNVERIFIED:**
- Whether Ghidra imports embedded NB11 directly (no Ghidra here).
- The comp.id names for the Rich header values.
- That `cl` without `/O` means `/Od`.
- The suspected window leak in the MSAA loop.
- How `hw1_src_funcs.txt` (the 6940-name list) was produced; the verifier reused it.
- Behaviour under `SDL_VIDEODRIVER=wayland`; the game was never run (no assets).
- Every Cataclysm-side statement (blocked on the GOG offline installer).

### Second pass (A5b continuation), 2026-10-08

Scope: the continuation-pass content (A5b, key finding 10, C1.7) plus a re-check of high-impact older claims. Sources: GoK `3af31fb`, `gok-hist` (root `ad1bb82`), CET `19a08c3`, hw_rgl `41ff3c3`, `reference/build-hwsdl/`. Nothing was run except `biggie` with no arguments, `sha256sum`/`file`/`stat` on the build outputs, and one header-free `gcc -m32 -S` alignment probe in scratch. No file in `reference/` was modified.

**Claims checked: 36 groups.** Confirmed as written: ORIG `TYPES.H:11-13,33-36,40`; `43ded70:Types.h:41-42,48`; `f47c87f6` (atsb, 2024-04-05) and `Types.h:65`; GoK `Types.h:15-16,28-35,55-63,78-82,89,96-101,113,115` and `Types.c:7-19,21ff`; `CET:game/Game/TYPES.h:11-24`; no `LITTLE_ENDIAN`/`BIG_ENDIAN` token in GoK `src/`/`tools/`; commits `9ad00155 a21dc1cb 1df2d4c5 97213da5` (authors, dates, subjects); the 1023/1024 typo and `ORIG speechevent.h:2`; all 21 per-file `FIX_ENDIAN*` counts and 511 total; every `#if FIX_ENDIAN` block count except one; `Light.c`, `render.c`, `Particle.c`, `SpeechEvent.c` line refs; every `meson.build` line in the `HW_*` table; `mingw/Makefile:45`; file counts 17/21/13/97; `avi.c:32,79,515,626`; `Sensors.c:3427`; `ShipDefs.h:83`; `smixer.c:839`; `Particle.c:2064`; `configure.ac:50-55,214-219,235-236`; no raiders option in `meson_options.txt`. Older claims re-checked: CET `Cataclysm.h` 44/55/52 = 151 addresses and the four cited addresses; `CET:Config.h:7-33`; `Universe.c:75`, `Universe.h:44-53`, ORIG `universe.h:57`, `shader.c:38`, `utility.h:39`; `main.c:690-691,848,2195-2197`; `SaveGame.h:16-22`; `BigFile.h:67,76,86-121` and `BigFile.c:72-95`; `hw_rgl/D3D9/d3dinit.cpp:141`; `CET:GameHooks.cpp:12`; GoK `Task.h:150-180`, ORIG `task.h:165`, `CET task.h:131,167`; the 12 `CCMD0` files; binary size, sha256, `file` line, kas2c/biggie sizes, biggie usage (`-v` exists); 37+8 `-Wno-*`; `kas2c/meson.build:3-4`; `installation-manual.md:51`.

**Claims wrong or overclaimed: 6, all fixed in place.**

| # | Was | Now | Evidence |
|---|---|---|---|
| 1 | GoK `Uint32`/`Sint32` at `Types.h:49-50` | `sdword` :48, `udword` :49 | `cat -n Types.h` |
| 2 | `git grep -i endian ad1bb82` hits only the registry code and Glide headers | Also `Md52.cpp:10,24,29`, a `TitanInterface.cpp:4027` comment, two editor tools; the claim now scoped to game code (`src/Game`, `src/Ships`: no hit) | `git grep` in `gok-hist` |
| 3 | Mac merge "pre-swapped" / "byte-swapping" speech constants | Values have their 16-bit halves exchanged (`0x400` → `0x04000000`), not a 32-bit byte swap | `9ad00155:src/Game/SpeechEvent.h:1-60` |
| 4 | "every swap sits right after an `fread`/`memcpy`"; ETG row "ETG bytecode"; C1.7 "complete inventory" listing Particle/ETG as readers; key finding 10 "every reader" | 15 reader files; `Particle.c`, `ETG.c`, `Color.h`, `Trails.h`, `Tutor.c` are packed-colour/flag-bit fixes; inventory limited to binary readers of multi-byte fields | `ETG.c:7909-7928`, `Tutor.c:1948-1952`, `Trails.h:37`, `Color.h:24-46` |
| 5 | `monochrome-btg.c` `#if FIX_ENDIAN` blocks "n/a" | 1 (`:83`) | `grep` |
| 6 | `-malign-double` impact left fully open; C1.6 says a `-m32` build overlays data structs directly | Probe shows `-malign-double` breaks `-Zp4` parity for 8-byte members; plain `-m32` matches; BTG's `real64` structs are read field by field; C1.6 caveat added | probe in scratch; `BTG.h:35-50`, `BTG.c:666-668` |

**Added (missing, all sourced):**
- `.big` variant detection: Remaster TOC chosen only when `numFiles == 13887` (`BigFile.c:416`); exact `RBF1.23` header compare (`BigFile.c:499-512`) in game and biggie; C3 step 1 now starts with a raw header dump.
- Relic's own build defines (`-D$(%HW_Level) -D$(%USERNAME)`, product defines) with use counts and the 2006 rename commits, including the developer-username `#ifdef`s (`gshaw` ×32, `#ifndef fpoiker` ×3) that decide what a shipped binary contains.
- `debug_asserts` defaults to true, so the default build (and `reference/build-hwsdl/homeworld`) is `HW_BUILD_FOR_DEBUGGING`, which `meson_options.txt` says changes behaviour slightly.
- Dead meson defines: `ARM` (unused) and the macOS `__APPLE___FIX_*` names (no source tests them; the code wants `_MACOSX_FIX_*`).
- Strict-aliasing exposure of `TreatAs*` (8 use sites, no `-fno-strict-aliasing`); CET's signed `bool32` matches Relic's signed `bool`; speech `.lut` filenames; stale `/legacy` comment at `Universe.h:45`.

**Remaining issues / still UNVERIFIED (this pass):**
- Why the PPC speech constants needed a halfword exchange rather than a byte swap (speech loader not traced).
- Whether any raw-`fread`/`memcpy` struct other than BTG has an 8-byte member that `-malign-double` would shift (not exhaustively searched; only the loader headers `Mesh.h MeshAnim.h NIS.h FEFlow.h font.h texreg.h BTG.h MEX.h Light.h SoundEventPrivate.h SoundStructs.h BigFile.h SpeechEvent.h ETG.h LOD.h HS.h` were grepped for `real64|double|sqword|uqword`).
- Which build account produced Relic's or Cataclysm's release exe, and hence which username-gated blocks it contains.
- The macOS meson behaviour is inferred from the sources; no macOS build was attempted.
