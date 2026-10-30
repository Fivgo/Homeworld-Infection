# Release 1.0.0: the macOS package

The macOS part of [release-1.0.md](release-1.0.md): `Homeworld-Infection-1.0.0-macOS.dmg`, holding
`Homeworld Infection.app` (universal arm64 + x86_64, macOS 11+, ad-hoc signed), an Applications link, the player
README (`tools/release/README-macOS.txt`) and the licences. Built only on GitHub's macOS runners: nobody on the
project has a Mac, so **nothing here has run on a real Mac with the game data** (see "Unverified").

## Result

| | |
|---|---|
| CI run | https://github.com/Fivgo/Homeworld-Infection/actions/runs/40977988388 (commit a302d38, branch `ci/macos`): every step green |
| artifact | `package-macos` (id 12942219976): `Homeworld-Infection-1.0.0-macOS.dmg`, 9,687,330 bytes, sha256 `38c5c52b6ffbc18616e2148b9665bd75c239984507f3c20636fc625daf534d70` (every build differs: timestamps). Also `logs-macos` (every step's log) |
| runner | `macos-15` arm64: macOS 15.7.9, Xcode 16.4, Apple clang 17.0.0 (clang-1700.0.13.5), cmake 4.4.3, meson 1.6.1, Rosetta present |
| engine | arm64 and x86_64 built with `-Werror`: **0 compiler warnings** on each |
| universal | yes: one dmg; every Mach-O in the bundle is `x86_64 arm64` |
| time | about 2 min with the third-party cache; a cold third-party build adds about 4 min |

How it got there (each run's fixes in the next commit):
1. [40976222887](https://github.com/Fivgo/Homeworld-Infection/actions/runs/40976222887): SDL2, both FFmpeg slices,
   xz and Boost built; innoextract's configure failed (`find_package(ZLIB REQUIRED)` inside its "static first" loop;
   the SDK has no `libz.a`): zlib, bzip2 and iconv are now taken dynamically from the SDK.
2. [40976773729](https://github.com/Fivgo/Homeworld-Infection/actions/runs/40976773729): all third-party parts built
   and cached; the arm64 engine compiled (one warning: `SIG_BLOCK` redefined) and failed to link on `_fsqrt`.
3. [40977544320](https://github.com/Fivgo/Homeworld-Infection/actions/runs/40977544320): app, dmg and smoke ran;
   0 warnings; two script bugs: the dylib ids carried the temporary slice names (`@rpath/arm64-libavcodec.60.dylib`)
   and the smoke read the engine's normal exit status 213 as a signal.
4. [40977988388](https://github.com/Fivgo/Homeworld-Infection/actions/runs/40977988388): green, `-Werror` on.

`lipo -info` (run 40977988388):

```
Architectures in the fat file: Homeworld Infection.app/Contents/Frameworks/libavcodec.60.dylib are: x86_64 arm64
Architectures in the fat file: Homeworld Infection.app/Contents/Frameworks/libavformat.60.dylib are: x86_64 arm64
Architectures in the fat file: Homeworld Infection.app/Contents/Frameworks/libavutil.58.dylib are: x86_64 arm64
Architectures in the fat file: Homeworld Infection.app/Contents/Frameworks/libSDL2-2.0.0.dylib are: x86_64 arm64
Architectures in the fat file: Homeworld Infection.app/Contents/Frameworks/libswscale.7.dylib are: x86_64 arm64
Architectures in the fat file: Homeworld Infection.app/Contents/MacOS/hwi-engine are: x86_64 arm64
Architectures in the fat file: Homeworld Infection.app/Contents/MacOS/innoextract are: x86_64 arm64
```

`otool -L` (the same for both slices; `/System/Library/Frameworks` lists shortened):

```
MacOS/hwi-engine:      /System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL, @rpath/libSDL2-2.0.0.dylib,
                       /usr/lib/libSystem.B.dylib, @rpath/libavutil.58.dylib, @rpath/libswscale.7.dylib,
                       @rpath/libavcodec.60.dylib, @rpath/libavformat.60.dylib; LC_RPATH @executable_path/../Frameworks
MacOS/innoextract:     /usr/lib/libz.1.dylib, /usr/lib/libbz2.1.0.dylib, /usr/lib/libiconv.2.dylib, /usr/lib/libc++.1.dylib,
                       /usr/lib/libSystem.B.dylib  (Boost and liblzma static)
libavcodec.60.dylib:   id @rpath/libavcodec.60.dylib; @rpath/libavutil.58.dylib, libSystem, CoreFoundation, CoreVideo, CoreMedia
libavformat.60.dylib:  id @rpath/libavformat.60.dylib; @rpath/libavcodec.60.dylib, @rpath/libavutil.58.dylib, libSystem,
                       CoreFoundation, CoreVideo, CoreMedia
libavutil.58.dylib:    id @rpath/libavutil.58.dylib; libSystem, CoreFoundation, CoreVideo, CoreMedia
libswscale.7.dylib:    id @rpath/libswscale.7.dylib; @rpath/libavutil.58.dylib, libSystem
libSDL2-2.0.0.dylib:   id @rpath/libSDL2-2.0.0.dylib; libSystem, libobjc (weak), Cocoa, AppKit, Foundation, CoreFoundation,
                       CoreGraphics, CoreServices, CoreVideo, IOKit, ForceFeedback, Carbon, CoreAudio, AudioToolbox,
                       AVFoundation, GameController/Metal/QuartzCore/CoreHaptics (weak)
```

No `/opt/homebrew`, `/usr/local` or build-directory path anywhere; the only `LC_RPATH` is the engine's
`@executable_path/../Frameworks` (meson's build rpaths are stripped); `minos 11.0` on all 14 slices.

Smoke results (run 40977988388):
- codesign: `valid on disk`, `satisfies its Designated Requirement`; `Identifier=io.github.fivgo.homeworld-infection`,
  `Format=app bundle with Mach-O universal (x86_64 arm64)`, `Signature=adhoc`, `TeamIdentifier=not set`. `spctl
  --assess`: `rejected` (expected for an ad-hoc app; printed only).
- `innoextract --version`, arm64 and x86_64 (Rosetta): `innoextract 1.10-dev`, "Extracts installers created by Inno
  Setup 1.2.10 to 6.3.3".
- The engine without data, both slices (x86_64 through Rosetta), dummy video and audio: it loads every bundled dylib,
  prints `Unable to open file: Music4Cat.wxd` and exits with status 213 (`ERR_ErrorStart`) at once; nothing is
  written to `HOME`. Once datasetup is merged it exits the same way after its "datasetup:" line (it shows no dialog
  under the dummy driver, its author says).
- The dmg mounts and lists `Applications -> /Applications`, `Homeworld Infection.app`, `Licenses`, `README.txt`; the app's
  signature is valid on it.

## Port changes (engine)

All tagged `HWI-FIX(macos)`; the Linux build is unchanged (gcc `tools/build.sh` and `--release`: 0 warnings; a clang
release build on Linux: 0 warnings; the default `tools/m2_harness.sh` run gives the same result as the unmodified
HEAD, see "Checks on Linux").

| Where | Change | Why |
|---|---|---|
| `meson.build` | a `darwin` branch: `-D_LINUX_FIX_ME -D_GNU_SOURCE -DGL_SILENCE_DEPRECATION`; no `x11` dependency; `dependency('gl')` is meson's OpenGL framework | the macOS build follows the tested Linux code paths. GoK's `_MACOSX` / `_MACOSX_FIX_*` (HomeworldSDL's Xcode prefix header) are **not** defined: they switch off movies, sound streaming and LAN, size the heap from a fixed page count and look for an `Override.big` folder. `__APPLE__` (the compiler's) marks what differs |
| `meson.build` | comment only: `-ffp-contract=off` (already in `determinism_args` for every build) also overrides clang's arm64 default, which fuses `a*b+c` | the x86-64 results assume no fusion |
| `src/SDL/utility.c` | the X11 includes and the X11 lock-key code in `utyToggleKeyStatesRestore` are skipped on `__APPLE__` | no X11 on macOS (that function only calls `SDL_SetModState` there) |
| `src/SDL/regkey.h` | `CONFIGDIR` on `__APPLE__` is `Library/Application Support/Homeworld-Infection` (GoK: `""`, i.e. `$HOME` itself) | contract 4. `utyGameSystemsPreInit`'s non-Windows branch (`$HOME/` CONFIGDIR) and `utyOptionsFileRead` both use it, so `utility.c`'s settings block needed no edit; datasetup's `gamedata-path.txt` and `GameData` use the same `$HOME/` CONFIGDIR. The folder is made on the first write (`fileMakeDestinationDirectory`) |
| `src/SDL/render.c` `setupPixelFormat` | on `__APPLE__`: `SDL_GL_CONTEXT_MAJOR_VERSION 2`, `MINOR 1`, `PROFILE_MASK 0` (the legacy 2.1 context; SDL's default on macOS, now explicit) and `SDL_WINDOW_ALLOW_HIGHDPI` | the renderer is fixed-function (a core profile has no `glBegin`); Retina: the drawable is the physical size. `uiscale.c` already keeps the window size (points: the mouse, `SDL_WarpMouseInWindow`) apart from the drawable (`SDL_GL_GetDrawableSize`: `MAIN_WindowWidthActual`, the viewport, the screenshots), so nothing else changed |
| `src/SDL/render.c` | `glGenerateMipmap` falls back to `glGenerateMipmapEXT` when the plain name is not found (all platforms) | a 2.1 context's `EXT_framebuffer_object` name; the texture code calls it unconditionally |
| `src/SDL/main.c` `ProcessCommandLine` | on `__APPLE__`, a `-psn_...` argument is skipped | older Finders pass the process serial number; an unknown switch prints the help and quits |
| `src/Game/FastMath.c`, `src/Game/Types.h` | the Windows build's `float fsqrt(double)` (C23, correctly rounded) is also defined and declared on `__APPLE__` | glibc has it (the Linux build reaches it through `<math.h>`), Apple's libm does not: the arm64 link failed on `_fsqrt` |
| `src/Game/MotionTest.c` | the frame-signature thumbnail block macro `SIG_BLOCK` is `MT_SIG_BLOCK` | macOS's `<sys/signal.h>` defines `SIG_BLOCK` (the one Apple clang warning) |

Checked and left alone:
- `utyGetFirstCDPath`: the non-Windows branch is `stat()` of `$HW_CDROM` or `/mnt/cdrom` (POSIX); it finds nothing on a
  Mac, as on most Linux machines.
- The heap size (`sysconf(_SC_PHYS_PAGES)`), `mmap(MAP_ANON)`, `fnmatch`, `dirent`: POSIX, as on Linux.
- `FILE_CASE_INSENSITIVE_SEARCH` stays 1 (it is 0 only under `_MACOSX`): a Mac volume can be case-sensitive.
- x86-only code is guarded: `Shader.c` (`_mm_cvtsd_si64` under `__x86_64__ && __SSE2__`), `OracleDump.c` (MXCSR under
  `__SSE__ || __x86_64__`; 0 elsewhere), `Matrix-mult.c` inline asm (`__i386__` only), stb (its own detection). No
  `-msse` outside the 32-bit x86 branch. arm64 gets meson's aarch64 branch (`-DARM -DHW_PTR_64 -DHW_PTR_64_FIX_ME`;
  `-DARM` is not tested anywhere in `src/`) and `-DGENERIC_ETGCALLFUNCTION` (no longer tested either).
- Apple arm64 passes variadic arguments on the stack, unlike x86-64: the engine's variadic functions are the printf
  family, `regKeyChildAlloc`/`regKeysSet` and `etgEffectCreate`, all called with prototypes; `etgEffectCreate` reads
  each parameter as a `memsize` and the callers pass `SCALECAST`/`udword` values (a 32-bit value is widened in its
  8-byte slot as on x86-64).
- A clang build with the legacy suppressions off lists 21 implicit function declarations (the suppression list
  hides them). They behave the same on arm64 as on x86-64, with one existing bug on every platform:
  `CameraCommand.c:1975` `cat_ccZoomScripted` calls `cameraMinimumZoom` (returns `real32`, declared nowhere) as an
  `int` function, so the scripted zoom reads an integer register instead of the float one (x86-64 `eax`, arm64 `w0`):
  its minimum zoom is garbage on Linux and Windows too. Not changed here (it would change Linux behaviour); a
  prototype in `Camera.h` fixes it.
- Floating point: `-ffp-contract=off` keeps arm64's basic arithmetic bit-identical to x86-64's SSE2, but Apple's libm
  (`sinf`, `cosf`, `atan2f`, `powf`, ...) can differ from glibc's in the last bit. The checksum oracle's reference
  streams are Linux x86-64 only; a Mac run is not an oracle baseline.

## The build: `tools/release/build-macos.sh`

Runs on a macOS machine with Xcode's command line tools, cmake, meson, ninja and pkg-config; the CI job installs
meson 1.6.1 and ninja 1.11.1.1 in a venv. One arm64 runner builds both architectures (`-arch x86_64` cross builds):

1. **Third-party** (`build/macos-deps`, each part kept while its recipe line, versions + sha256 + `RECIPE`, is the
   same; `--deps-key` prints the cache key over all of them):
   - SDL2: one CMake build with `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`, shared, no tests, no X11, no rpath.
   - FFmpeg: `tools/release/ffmpeg-min.sh --target macos-arm64` and `macos-x86_64` (contract 5).
   - liblzma: XZ Utils' configure, static, no tools, per architecture.
   - Boost: `iostreams filesystem date_time system program_options`, static, b2 `toolset=clang` with
     `architecture=`/`address-model=64` and `-arch`, per architecture; iostreams on the SDK's zlib and bzip2.
   - innoextract: CMake per architecture with `USE_STATIC_LIBS=ON` (Boost and liblzma static; the SDK's libz,
     libbz2, libiconv and libc++ are system libraries), then `lipo`.
   Every CMake call ignores `/opt/homebrew`, `/usr/local` and `/opt/local`; `PKG_CONFIG_LIBDIR` points only at the
   deps, so nothing from Homebrew can be picked up.
2. **Engine**: one meson cross build per architecture (`build/macos-release/macos-<arch>.ini`; a cross build also
   leaves out kas2c, which needs bison/flex), with `tools/build.sh --release`'s options (`-Doptimization=2
   -Ddebug=false -Db_sanitize=none`) and `-Dbuild_version=<version>`; `-Dwerror` from `MACOS_WERROR` (default true).
3. **App**: every Mach-O is thinned, fixed and `lipo`'d again: dylib ids `@rpath/<name>`, references to anything
   outside `/usr/lib` and `/System` rewritten to `@rpath/<name>`, absolute `LC_RPATH`s deleted; the engine gets
   `@executable_path/../Frameworks`. Info.plist, `PkgInfo`, the icon, the licences; then the ad-hoc signature inside
   out (the dylibs, innoextract, then the bundle) and `codesign --verify --deep --strict`.
4. **dmg**: `hdiutil create -fs HFS+ -format UDZO` (retried: the runners' hdiutil is sometimes "busy") of the app,
   an `Applications` symlink, `README.txt` and `Licenses/`.
5. **Smoke** (also `--smoke-only`): see "CI".

Bundle layout:

```
Homeworld Infection.app/Contents/
  Info.plist            CFBundleName/DisplayName "Homeworld Infection", CFBundleIdentifier io.github.fivgo.homeworld-infection,
                        CFBundleShortVersionString/CFBundleVersion 1.0.0, CFBundleExecutable hwi-engine,
                        LSMinimumSystemVersion 11.0, NSHighResolutionCapable, NSSupportsAutomaticGraphicsSwitching,
                        LSApplicationCategoryType public.app-category.strategy-games, CFBundleIconFile homeworld-infection
  PkgInfo
  MacOS/hwi-engine      the engine (universal)
  MacOS/innoextract     contract 3 (universal; datasetup finds it beside the executable via _NSGetExecutablePath)
  Frameworks/           libSDL2-2.0.0.dylib, libavcodec.60.dylib, libavformat.60.dylib, libavutil.58.dylib, libswscale.7.dylib
  Resources/            homeworld-infection.icns (tools/release/icon, original art), Licenses/
  _CodeSignature/
```

Note: `SDL_GetBasePath()` in a bundle is `Contents/Resources/`, not `Contents/MacOS/`; datasetup looks beside the
executable first, then `<base>innoextract`, then `<base>../MacOS/innoextract`.

## Pins

| Part | Version | Source | sha256 | Where the sum comes from |
|---|---|---|---|---|
| SDL2 | 2.30.12 | github.com/libsdl-org/SDL release `SDL2-2.30.12.tar.gz` | `ac356ea55e8b9dd0b2d1fa27da40ef7e238267ccf9324704850d5d47375b48ea` | Homebrew's sdl2 2.30.12 formula; the Linux CI download printed the same |
| FFmpeg | 6.1.2 | ffmpeg.org `ffmpeg-6.1.2.tar.xz` (ffmpeg-min.sh) | `3b624649725ecdc565c903ca6643d41f33bd49239922e45c9b1442c63dca4e38` | Homebrew's ffmpeg@6 6.1.2 formula; the Linux CI printed the same |
| XZ Utils (liblzma) | 5.8.4 | github.com/tukaani-project/xz release `xz-5.8.4.tar.gz` | `0014c7886930454fe8bd4228665b51af55eeae560ea135c9c4cd33f55b2591d9` | GitHub's release asset digest |
| Boost | 1.86.0 | github.com/boostorg/boost release `boost-1.86.0-b2-nodocs.tar.xz` | `a4d99d032ab74c9c5e76eddcecc4489134282245fffa7e079c5804b92b45f51d` | Boost's own `.tar.xz.txt` |
| innoextract | master `6e9e34ed0876014fdb46e684103ef8c3605e382e` (2025-02-06; reports itself as 1.10-dev) | github.com/dscharrer/innoextract archive | `4d885b27e452dcb137a9d97736b19478447f172bcfed1aa98d80138e9a4c08ae` | the first CI download (run 40976222887) |

Why these: Boost 1.86 because innoextract asks for the `system` component, whose stub library Boost 1.89 removed
(Homebrew carries an unofficial patch for it). innoextract from master rather than release 1.9: 1.9 does not build
with Boost >= 1.85 or CMake 4 without two backports, and master adds Inno Setup 6.3/6.4 and the 2025 GOG Galaxy
installer fixes. The user's installer (`setup_homeworld_emergence_1.01_hotfix_(12948).exe`, Inno Setup 5.5.7) needs
only 1.9, which the Windows package ships; master is a superset for it. FFmpeg 6.1.2 is contract 5's pin (the 6.1
branch is at 6.1.6).

## CI: `.github/workflows/build-macos.yml`

Triggers: push to `ci/macos`, `workflow_dispatch` and `workflow_call` (input `version`, default 1.0.0). Runner
`macos-15` (arm64). Steps: the build tools; the deps key; `actions/cache/restore`; `--deps-only`;
`actions/cache/save` as soon as the third-party builds succeed (so a later failure never throws them away); the
engine, app, dmg and smoke; upload `package-macos` (the dmg) and `logs-macos` (every step's log).

Smoke checks (CI never has game data):
- `lipo -info` of every Mach-O in the bundle: each must be `x86_64 arm64`.
- `otool -L` of every Mach-O, per architecture: only `@rpath/`, `@executable_path/`, `@loader_path/`, `/usr/lib/`,
  `/System/Library/`; no `/opt/homebrew` or `/usr/local`; the `LC_RPATH`s and each slice's `minos` are printed.
- `codesign --verify --deep --strict`, the signature details, `spctl --assess` (rejects an ad-hoc app: printed only).
- `innoextract --version` from the bundle, both slices (x86_64 through Rosetta when the runner has it).
- The bundled engine per architecture with `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`, an empty `HOME`, an empty
  working directory, no `HW_Data`, under a 60 s alarm: it must exit by itself (a hang or a signal fails).
- The dmg is mounted read-only and the app's signature checked on it.

## Checks on Linux

- `tools/build.sh` (debug, ASan/UBSan) and `tools/build.sh --release` with gcc: 0 warnings, after every port change.
- A clang 18 release build on Linux: 0 warnings.
- The release engine headless without data: `Unable to open file: Music4Cat.wxd`, status 213 (the same as the Mac
  smoke).
- `tools/m2_harness.sh` default run (sp, offscreen, 60 s): `universe running`, rc 124 at the time-out, the log ending
  with `AddressSanitizer:DEADLYSIGNAL` during the shutdown the time-out starts. The unmodified HEAD bdf5a0c, built
  separately, gives the identical result (run macos-baseline-175138), so it predates this port.

## Unverified (no Mac, no game data in CI)

Factually: the engine has been compiled, linked and started on macOS (both slices) only as far as the no-data exit.
Not seen working:
- **Rendering**: Apple's legacy OpenGL 2.1 (on Apple Silicon a layer over Metal) with this fixed-function renderer;
  whether `glGenerateMipmap` resolves in a 2.1 context; Retina (`SDL_WINDOW_ALLOW_HIGHDPI`): drawable 2x the window,
  the UI scale's auto rule then picks from the physical height (150% on a typical MacBook, 200% at 2160 px or more);
  fullscreen (`SDL_WINDOW_FULLSCREEN_DESKTOP` is a macOS Space), window resizing, the Video screen's mode list
  (SDL2 lists display modes in points on macOS).
- **Input**: keyboard (no Insert/Scroll Lock/Print Screen on Mac keyboards; GoK's `__APPLE__` screenshot alternatives,
  F13 and keypad +/-, are compiled in), the mouse and trackpad (right-click, relative mouse mode in the camera),
  Cmd-Q (SDL sends `SDL_QUIT`).
- **Audio** through CoreAudio, and the movies through the bundled FFmpeg.
- **Data setup** (datasetup.c, merged separately): the osascript pickers, unpacking the installer with the bundled
  innoextract, the paths with spaces (`Application Support`, `Homeworld Infection.app`).
- **Gatekeeper**: the "Open Anyway" flow for an ad-hoc signed, quarantined app on macOS 15 (macOS 11-14: Control-click
  Open); whether Gatekeeper lets the nested ad-hoc signed `innoextract` run when the app spawns it (it is sealed in
  the app's signature, so it should); App Translocation when the app is started from the dmg or Downloads.
- **Determinism**: arm64 arithmetic matches x86-64 SSE2 with `-ffp-contract=off`, but Apple's libm may differ from
  glibc in the last bit; a Mac run is not an oracle baseline. Single player only, so no lockstep across platforms.
- **macOS 11-14**: built with `minos 11.0` on every slice but only started on macOS 15.7.9.

## Merge notes

- `tools/release/ffmpeg-min.sh`: only the `FFMPEG_SHA256=` line changes, byte-identical to the Windows branch's.
- `src/SDL/utility.c`: three guards near the top (includes) and in `utyToggleKeyStatesRestore`; nothing near the
  data-path / settings-path blocks of `utyGameSystemsPreInit`.
- With m2's removal of `-Wno-implicit-function-declaration`: the macOS changes add no undeclared call (`fsqrt` is
  declared in `Types.h` for `__APPLE__`, as for `_WIN32`).
- After the merge, run the workflow again (`workflow_dispatch` or `ci/macos`): the smoke then covers datasetup's
  no-data path on macOS.
