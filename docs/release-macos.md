# Release 1.0.0: the macOS package

The macOS part of [release-1.0.md](release-1.0.md): `Homeworld-Infection-1.0.0-macOS.dmg`, holding
`Homeworld Infection.app` (universal arm64 + x86_64, macOS 11+, ad-hoc signed), an Applications link, the player
README (`tools/release/README-macOS.txt`) and the licences. Built only on GitHub's macOS runners: nobody on the
project has a Mac, so **nothing here has run on a real Mac with the game data** (see "Unverified").

RESULTS_PLACEHOLDER

## Port changes (engine)

All tagged `HWI-FIX(macos)`; the Linux build is unchanged (gcc `tools/build.sh` and `--release`: 0 warnings; a clang
release build on Linux: 0 warnings; the default `tools/m2_harness.sh` run gives the same result as the unmodified
HEAD, see "Checks on Linux").

| Where | Change | Why |
|---|---|---|
| `meson.build` | a `darwin` branch: `-D_LINUX_FIX_ME -D_GNU_SOURCE -DGL_SILENCE_DEPRECATION`; no `x11` dependency; `dependency('gl')` is meson's OpenGL framework | the macOS build follows the tested Linux code paths. GoK's `_MACOSX` / `_MACOSX_FIX_*` (HomeworldSDL's Xcode prefix header) are **not** defined: they switch off movies, sound streaming and LAN, use a fixed 128 MB-ish heap guess and an `Override.big` folder. `__APPLE__` (the compiler's) marks what differs |
| `meson.build` | comment only: `-ffp-contract=off` (already in `determinism_args` for every build) also overrides clang's arm64 default, which fuses `a*b+c` | the x86-64 results assume no fusion |
| `src/SDL/utility.c` | the X11 includes and the X11 lock-key code in `utyToggleKeyStatesRestore` are skipped on `__APPLE__` | no X11 on macOS (that function only calls `SDL_SetModState` there) |
| `src/SDL/regkey.h` | `CONFIGDIR` on `__APPLE__` is `Library/Application Support/Homeworld-Infection` (GoK: `""`, i.e. `$HOME` itself) | contract 4. `utyGameSystemsPreInit`'s non-Windows branch (`$HOME/` CONFIGDIR) and `utyOptionsFileRead` both use it, so `utility.c`'s settings block needed no edit; datasetup's `gamedata-path.txt` and `GameData` use the same `$HOME/` CONFIGDIR. The folder is made on the first write (`fileMakeDestinationDirectory`) |
| `src/SDL/render.c` `setupPixelFormat` | on `__APPLE__`: `SDL_GL_CONTEXT_MAJOR_VERSION 2`, `MINOR 1`, `PROFILE_MASK 0` (the legacy 2.1 context; SDL's default on macOS, now explicit) and `SDL_WINDOW_ALLOW_HIGHDPI` | the renderer is fixed-function (a core profile has no `glBegin`); Retina: the drawable is the physical size. `uiscale.c` already keeps the window size (points: the mouse, `SDL_WarpMouseInWindow`) apart from the drawable (`SDL_GL_GetDrawableSize`: `MAIN_WindowWidthActual`, the viewport, the screenshots), so nothing else changed |
| `src/SDL/render.c` | `glGenerateMipmap` falls back to `glGenerateMipmapEXT` when the plain name is not found (all platforms) | a 2.1 context's `EXT_framebuffer_object` name; the texture code calls it unconditionally |
| `src/SDL/main.c` `ProcessCommandLine` | on `__APPLE__`, a `-psn_...` argument is skipped | older Finders pass the process serial number; an unknown switch prints the help and quits |

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
| innoextract | master `6e9e34ed0876014fdb46e684103ef8c3605e382e` (2025-02-06) | github.com/dscharrer/innoextract archive | INNOEXTRACT_PIN | the first CI download |

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

UNVERIFIED_PLACEHOLDER
