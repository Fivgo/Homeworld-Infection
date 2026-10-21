# M3: the Windows x86-64 build (2026-10-21)

A shareable Windows 11 build of the engine as play-tested on 2026-10-21 (eb5f7e3), cross-compiled on Linux. **Not
yet run on Windows; no Wine test** (the user declined the Wine launch; the user tests it on the dual-boot machine).

## Package

| | |
|---|---|
| zip | `/home/evanmedinger/Documents/GameDev/Homeworld-Infection/dist/Homeworld-Infection-win64-20261021-eb5f7e3.zip` |
| sha256 | `ce95fec524d256f41f370cdb875da4704ca56fa90eec0920c60fbb47e56a0b6b` |
| contents | hwi-engine.exe (5,313,024 B, version string 2e76066), SDL2.dll 2.30.12, README-Windows.txt, LICENSE-REULA.txt (src/COPYING.REULA), LICENSE-SDL2.txt (zlib); CRLF text; no game data |
| rebuild | `tools/release_win64.sh --engine eb5f7e3` (build/win64, then zips into MAIN/dist; dist/ is gitignored) |

The exe imports only SDL2.dll (shipped) and Windows' own DLLs: OPENGL32, KERNEL32, USER32, SHELL32 and the UCRT
`api-ms-win-crt-{stdio,runtime,filesystem,string,convert,locale,time,math,utility,heap,environment,private}-l1-1-0`
(part of Windows 10/11). No libc++, libunwind or winpthread DLL. The script refuses any other import.

## Toolchain (in ~/Applications, nothing system-wide)

- llvm-mingw **20261006**, `llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64.tar.xz` (clang 23.1.3, UCRT), sha256
  `5f9c6ed9…a8d8ab4`, equal to GitHub's published digest. The newer 20261020 was published on the day and skipped.
- SDL2 **2.30.12** `SDL2-devel-2.30.12-mingw.tar.gz` (the last 2.30.x; Linux has 2.30.0), sha256 `dddafbf0…e081b0f`.
  libsdl publishes no checksum for the devel tarball.
- Cross file `tools/cross/x86_64-w64-mingw32.ini`: takes `llvm_mingw` / `sdl2_mingw` constants from a machine file
  passed before it (the script writes build/win64-paths.ini); `pkg-config --define-prefix`, because the tarball's
  sdl2.pc names /tmp/tardir as its prefix.
- Options: tools/build.sh --release's (`-Doptimization=2 -Ddebug=false -Db_sanitize=none`, the pinned
  `-fno-strict-aliasing -ffp-contract=off -fno-fast-math`, -Werror, 0 warnings) plus **`-Dmovies=false`**: there is no
  FFmpeg for mingw here, so the two animatics are skipped (README known issue).

## Source changes (each marked `HWI-FIX(win64)`; commit 2e76066)

1. **meson.build: kas2c skipped in a cross build.** It is a native tool linking the host's SDL and c_base_args, and
   nothing uses it since M2-R8.
2. **meson.build: clang `-Wno-incompatible-pointer-types-discards-qualifiers`** in the clang-only list. This is GCC's
   `-Wno-discarded-qualifiers` (237 sites in the generated missions). The Linux build is gcc 13 and does not see it.
3. **MissionSelfTest.c: `#undef near` under `_WIN32`.** windows.h, through SDL, defines `near` as empty, and the
   self-tests use it as a variable name.
4. **The LLP64 save table.** The only struct pinned by the save converter whose layout changes under LLP64 is
   **CaptainGameInfo**: its wchar_t is 2 bytes and its unsigned long 4, so it is 616 B on Windows against 896 B on
   Linux. All 118 other layouts are the same. `tools/m2_save32_gen.py --win64` writes
   `src/Game/SaveCat32Layouts_win64.c` from llvm-mingw clang's record layouts. Those fields become plain copies there,
   because the exe's ILP32 layout already has 2-byte wchar_t and 4-byte long. src/Game/meson.build picks the file by
   host system. The generator treats clang 23's `(anonymous union)::` scope prefix as equal to clang 18's name, and
   passes `-Wno-implicit-function-declaration`. `--check` of the Linux table: current.
5. **fsqrt (FastMath.c + a Types.h prototype, `_WIN32` only).** Four files call `fsqrt` without FastMath.h's
   table-sqrt macro: Mimic.c, BeastWeapon.c, Dissolve.c and MissionSelfTest.c. On Linux these calls reach glibc 2.35's
   C23 `float fsqrt(double)` (`nm`: `fsqrt@GLIBC_2.35`), which UCRT lacks. The Windows function gives the same correctly
   rounded result for any argument: `(float)sqrt(x)` with a midpoint fix through the sign of `fma(r, r, -x)`. Every
   caller passes a float, for which it equals sqrtf exactly. Linux is unchanged.
6. **Settings folder `%APPDATA%\Homeworld-Infection`** (utility.c and regkey.h `HWI_WIN_CONFIGDIR`). It holds the
   options file, keys.cfg, saves and screenshots, and is created on the first run. GoK used the game folder, which a
   64-bit exe cannot write under Program Files because UAC virtualisation applies to 32-bit programs only. GoK's
   APPDATA read was dead code, since it tested `_WINDOWS` but meson defines `_WIN32`. `/settingspath` still overrides
   the folder, and the data path is still the working directory, the game folder when the exe is double-clicked.

The LLP64 audit found no other dependence on 64-bit `long`. Grepping the compiled sources for `long` finds only
Relic's 32-bit-era UI and tutorial code, BitIO/LZSS (written for a 32-bit long) and lrint's UI rounding.
The base types are SDL's fixed-width types, and the oracle stream's format uses fixed-width fields.

## Guard rails (Linux)

- `tools/build.sh --release` and `--debug`: rc 0, **0 warnings**.
- The headless Mission01 pair `win64g-mission-a` / `win64g-mission-b` (`tools/m2_harness.sh --mode mission
  --mission-ticks 6000`, debug build as the baseline) is milestone "oracle mission run done", rc 0, with 0 ASan
  errors. Both streams have sha256 310e2d6b44c4dd55, byte-equal to each other.
- `hwo_compare.py` against the baseline m3r1i-mission-a: **IDENTICAL: 6001 ticks and 0 packets compared (ticks
  0-6000), no difference** for both runs. Only the header's build identity differs (58df92dd vs 0a6839b7).

## What the user must test on Windows 11 before sharing

1. Extract the zip into a copy of the GOG folder, next to cataclysm.exe, and double-click hwi-engine.exe. SmartScreen
   will warn, because the exe is unsigned. The main menu must appear at the expected UI scale.
2. `%APPDATA%\Homeworld-Infection` must exist with Cataclysm.cfg after the first Accept in Options.
3. New Game into Mission01: the opening cutscene, without the animatic, then the mission. Check the HUD and the letterbox.
4. Options -> Video: change the resolution and the UI scale, toggle fullscreen, then Accept.
5. Save, load, then Quit, including a Quit after a resolution change.
6. With a problem, run `hwi-engine.exe > hwi-log.txt 2>&1` from a Command Prompt. The exe is a GUI-subsystem
   program, so a double-click leaves no log.

## What is left

- A run on real Windows, by the user. Not run here at all, not even under Wine.
- The animatics on Windows need an FFmpeg build for mingw (`-Dmovies=true`).
- The headless self-tests and the Mission01 stream on Windows (M2-R10 plan row 1). These need an offscreen GL path or
  a native harness. The x87/libm sites are not measured there.
- Saves are per platform: wchar_t differs in CaptainGameInfo and in any native blob holding wchar_t. Recordings'
  exe-format saves go through the win64 table, which is untested on Windows.
- An optional code signature to avoid the SmartScreen warning.
