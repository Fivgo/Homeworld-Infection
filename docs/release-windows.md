# Release 1.0.0: the Windows package

The Windows part of [release-1.0.md](release-1.0.md): `Homeworld-Infection-1.0.0-Windows-Setup.exe`, an NSIS
installer built on a GitHub Ubuntu runner and smoke-tested on a GitHub Windows runner. It contains no game data. On
its first start the engine finds the GOG game or unpacks the offline installer itself (contract 2, `src/SDL/datasetup.c`,
written separately).

## What the player gets

| | |
|---|---|
| Install | double-click the Setup, SmartScreen "More info, Run anyway" (unsigned), Next / I Agree / Install / Finish. **No admin prompt** (`RequestExecutionLevel user`). |
| Pages | Welcome (what the game needs), Licence (the REULA), Folder (default `%LOCALAPPDATA%\Programs\Homeworld Infection`), progress, Finish with "Start Homeworld Infection" (ticked) and "Show the README" (unticked). |
| Shortcuts | "Homeworld Infection" in the user's Start menu (`%APPDATA%\Microsoft\Windows\Start Menu\Programs`) and on the Desktop: target `hwi-engine.exe`, start in the install folder, the exe's icon. |
| Apps & features | `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\Homeworld Infection`: DisplayName, DisplayVersion `1.0.0`, DisplayIcon `...\hwi-engine.exe,0`, Publisher "Homeworld-Infection project", EstimatedSize (KB, measured at install), InstallLocation, UninstallString, QuietUninstallString, NoModify, NoRepair. |
| Uninstall | Removes exactly the installed files (a generated list, never a recursive delete of the install folder), the shortcuts and the registry entry. A components page offers two **unticked** boxes: "Settings and saved games" (`%APPDATA%\Homeworld-Infection`) and "Unpacked game data" (`%LOCALAPPDATA%\Homeworld-Infection`); a box is hidden when its folder does not exist, and the page is skipped when neither does. Silent: `uninstall.exe /S` keeps both, `uninstall.exe /S /PURGE` removes both. |

Installed files (the install folder):

| File | From |
|---|---|
| `hwi-engine.exe` | this repository, llvm-mingw cross build, movies on, with the icon and VERSIONINFO 1.0.0 |
| `SDL2.dll` | SDL2 2.30.12, `SDL2-devel-2.30.12-mingw.tar.gz` |
| `avcodec-60.dll`, `avformat-60.dll`, `avutil-58.dll`, `swscale-7.dll` | FFmpeg 6.1.2 built by `tools/release/ffmpeg-min.sh --target mingw` (shared, LGPL, only the Bink demuxer and decoders, the file protocol and swscale) |
| `innoextract.exe` | innoextract 1.9, the official Windows release (contract 3: beside the exe) |
| `README.txt` | `tools/release/README-Windows.txt` with the version, date and commit filled in |
| `licenses\REULA.txt` | `src/COPYING.REULA` |
| `licenses\SDL2.txt` | the SDL2 tarball's `LICENSE.txt` (zlib) |
| `licenses\FFmpeg\` | `COPYING.LGPLv2.1.txt`, `LICENSE.md.txt`, `SOURCE.txt` (source URL, plus the configure line read from avutil) |
| `licenses\innoextract\` | from the innoextract zip: `README.txt` and `license/`: `innoextract.LICENSE.txt` (zlib), `Boost.LICENSE_1_0.txt`, `liblzma.COPYING.txt`, `libstdc++.COPYING3.txt` + `libstdc++.COPYING.RUNTIME.txt` (the libraries linked into the exe) |
| `uninstall.exe` | written by the installer |

Text files are CRLF.

## How it is built

`tools/release/build-windows.sh [--deps DIR] [--out DIR] [--llvm-mingw DIR] [--sdl2 DIR] [--ffmpeg DIR]
[--innoextract DIR] [--no-installer]`:

1. **Third-party parts.** Each one is downloaded into `build/win-deps/dl` and checked against its sha256 pin, unless
   a folder is given. llvm-mingw and SDL2 are unpacked; FFmpeg is built with `ffmpeg-min.sh --target mingw
   --cross-prefix x86_64-w64-mingw32- --cc x86_64-w64-mingw32-clang` into `build/win-deps/ffmpeg-mingw`. A stamp
   (the sha256 of ffmpeg-min.sh and the llvm-mingw name) rebuilds that prefix when either changes.
2. **The engine.** The cross build of `tools/release_win64.sh` (the same cross file `tools/cross/x86_64-w64-mingw32.ini`
   and the release flags `-Doptimization=2 -Ddebug=false -Db_sanitize=none`) with `-Dmovies=true
   -Dbuild_version=1.0.0` in `build/win64-release`. A third machine file, `build/win64-release-ffmpeg.ini`, sets
   `pkg_config_libdir` to SDL2's and FFmpeg's `lib/pkgconfig`. Later machine files override earlier ones, so the
   shared cross file is unchanged. The script checks that the exe has a GROUP_ICON and a VERSIONINFO resource.
3. **Stage** in `build/win64-release/stage`: the files above.
4. **Import check.** Every exe and DLL in the stage may import only DLLs of the package and Windows' own (kernel32,
   user32, gdi32, shell32, advapi32, ole32, oleaut32, comdlg32, winmm, opengl32, bcrypt, imm32, version, setupapi,
   shlwapi, ws2_32, msvcrt, the UCRT `api-ms-win-crt-*`). The log lists each file's imports.
5. **Installer.** A python snippet writes the File list and the matching Delete/RMDir list from the stage. `makensis`
   then compiles `tools/release/windows/installer.nsi` into `<out>/Homeworld-Infection-1.0.0-Windows-Setup.exe` and
   prints its sha256.

The version comes from `meson.build`'s project version. The icon and VERSIONINFO come from
`tools/release/windows/hwi-engine.rc.in`: `meson.build` configures it for any Windows host and compiles it with
`import('windows').compile_resources`, so `tools/release_win64.sh`'s zip build gets them too. VERSIONINFO has
FILEVERSION/PRODUCTVERSION 1,0,0,0, "FileVersion"/"ProductVersion" `1.0.0`, ProductName "Homeworld Infection", and
Comments `build <build_version>`. The `.ico` is the original art of `tools/release/icon/`.

On the development machine: `tools/release/build-windows.sh --llvm-mingw ~/Applications/llvm-mingw/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64 --sdl2 ~/Applications/SDL2-mingw/SDL2-2.30.12/x86_64-w64-mingw32`
still downloads and builds FFmpeg and downloads innoextract unless `--ffmpeg` and `--innoextract` are given, and it
needs `makensis` (apt `nsis`). So far it has only run on the CI runners (third-party code is fetched and run only
there).

## Pins

| What | Version | sha256 | Where pinned |
|---|---|---|---|
| llvm-mingw | 20261006, `llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64.tar.xz` (clang 23.1.3, UCRT) | `5f9c6ed95b2d4bdb2869a488c5fd5857fbdabcf288a0aa3eb1da43f6a08d8ab4` (equals GitHub's asset digest) | `tools/release/windows/pins.env` |
| SDL2 | 2.30.12, `SDL2-devel-2.30.12-mingw.tar.gz` | `dddafbf0705a8cbe1f97c69680886ce147ed4d38f1d04e03523681298e081b0f` (the tarball the 2026-10-21 build used; libsdl publishes no checksum) | `pins.env` |
| innoextract | 1.9, `innoextract-1.9-windows.zip` (GitHub release asset; i386 exe, statically linked; imports only KERNEL32, msvcrt, SHELL32) | `6989342c9b026a00a72a38f23b62a8e6a22cc5de69805cf47d68ac2fec993065` (from the first CI download, run 40975585872; GitHub publishes no digest for it). Also checked: sha512 `eea751bc…5f902488` (`INNOEXTRACT_SHA512`), the author's SHA512SUMS value for constexpr.org's copy, as Scoop's manifest carries it. It matched, so the GitHub and constexpr.org zips are the same file. | `pins.env` |
| FFmpeg | 6.1.2, `ffmpeg-6.1.2.tar.xz` from ffmpeg.org | `3b624649725ecdc565c903ca6643d41f33bd49239922e45c9b1442c63dca4e38` (Homebrew's ffmpeg@6 formula; the CI log printed the same) | `tools/release/ffmpeg-min.sh` (shared with Linux and macOS) |
| NSIS | Ubuntu 24.04's `nsis` package (apt) | apt's signatures | `.github/workflows/build-windows.yml` |

innoextract 1.9 is enough for this game. The user's GOG installer `setup_homeworld_emergence_1.01_hotfix_(12948).exe`
is Inno Setup data 5.5.7, and apt's innoextract 1.9 unpacked it (docs/RUNBOOK.md "innoextract support"). 1.9 parses
Inno Setup up to 6.2. An installer GOG might rebuild with Inno Setup 6.3 or later would need a newer innoextract.

## CI

`.github/workflows/build-windows.yml` runs on a push to `ci/windows`, on `workflow_dispatch`, and through
`workflow_call` from the release workflow.

- **build** (`ubuntu-24.04`): apt `meson ninja-build pkg-config nsis make unzip xz-utils binutils`, then
  `tools/release/build-windows.sh --out dist`. The installer is uploaded as the artifact **`package-windows`**.
  `actions/cache` keeps `build/win-deps/dl` (the downloads) and `build/win-deps/ffmpeg-mingw` (the FFmpeg build). Its
  key is the hash of `pins.env` and `ffmpeg-min.sh`. The unpacked toolchains are not cached; unpacking them takes
  seconds.
- **smoke** (`windows-latest`, after build): `tools/release/windows/smoke-test.ps1 -Setup <the artifact>`. It sets
  the error mode SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX (inherited by the children) and Windows Error
  Reporting's DontShowUI, so a missing DLL or a crash ends with a status code instead of a dialog. Then:
  1. `Setup.exe /S` exits 0. Every installed file is present: four FFmpeg DLLs, licence files, and the exe's
     ProductVersion and FileVersion `1.0.0`. Both shortcuts exist, with target, working folder and icon. The
     Apps & features values are set.
  2. `hwi-engine.exe` with `SDL_VIDEODRIVER=dummy` and `SDL_AUDIODRIVER=dummy`, no game data, a 60 s timeout. It
     must exit by itself, with an exit code that is not a loader failure (0xC0000135 DLL not found, 0xC0000139 entry
     point not found, 0xC000007B bad image, 0xC0000142 DLL init failed). Its stdout and stderr go to the log. With
     data setup merged, a headless run without data fails fast with exit code 65237 (ERR_ErrorStart) and never shows
     a dialog.
  3. `innoextract.exe --version` exits 0 and says 1.9.
  4. Stand-in files are put in `%APPDATA%\Homeworld-Infection` and `%LOCALAPPDATA%\Homeworld-Infection\GameData`.
     `uninstall.exe /S` must remove the install folder, both shortcuts and the registry entry, and keep both stand-ins.
  5. `Setup.exe /S` again, then `uninstall.exe /S /PURGE`: everything goes, both folders included.

CI never has game data, so nothing past the engine's start is tested there.

## Results (2026-10-29)

| Run | Commit | Result |
|---|---|---|
| [40975585872](https://github.com/Fivgo/Homeworld-Infection/actions/runs/40975585872) | 54c2f85 | build and smoke green. Found two problems: the innoextract zip keeps its licences in `license/`, which the stage missed; and the actions ran on Node 20 (forced to 24). |
| [40976227065](https://github.com/Fivgo/Homeworld-Infection/actions/runs/40976227065) | d1af679 | build 2m4s (every third-party part from the cache), smoke 30 s: **55 checks PASS, 0 FAIL** |

The installer is 3,711,100 bytes (11.1 MB unpacked; EstimatedSize 10,825 KB). makensis 3.09: 5 install pages, 3
uninstall sections (1 required). Imports, from the build log: `hwi-engine.exe`: SDL2, OPENGL32, KERNEL32, USER32,
SHELL32, the UCRT api-ms-win-crt-*, and avformat-60, avcodec-60, avutil-58, swscale-7. FFmpeg's DLLs import each other,
the UCRT, KERNEL32, and bcrypt (avutil). SDL2.dll imports only Windows DLLs (msvcrt among them). Smoke details:
- The engine on the Windows runner, no data, dummy video: it loaded every DLL, printed `Unable to open file:
  Music4Cat.wxd` and **exited with 65237 (0xFED5)** in well under the timeout. That is the code before data setup
  is merged, with the working folder as the data path.
- `innoextract.exe --version`: `innoextract 1.9`, "Extracts installers created by Inno Setup 1.2.10 to 6.0.5", exit 0.
- Uninstall `/S`: install folder, both shortcuts and the registry entry gone; the stand-in settings and game data kept.
  `/S /PURGE` after a reinstall: both folders gone too.

Local checks (development machine, no third-party downloads): the llvm-mingw cross build with `tools/release_win64.sh`
compiles the resource (`llvm-readobj`: ICON 1-7, GROUP_ICON 1, VERSIONINFO 1; FileVersion/ProductVersion 1.0.0) with 0
warnings. `src/SDL/avi.c` with HW_ENABLE_MOVIES compiles for mingw with clang 23 and -Werror against the host's
FFmpeg 6.1 headers. `tools/build.sh --release` and the debug build have 0 compiler warnings. A default
`tools/m2_harness.sh` run matches another branch's run of the same day: rc 124 at the 60 s timeout, 1745 file loads,
the same DEADLYSIGNAL at the kill.

Side fix: `-Dmovies=false` builds (`tools/release_win64.sh`) failed at bdf5a0c, because `aviShotTake()` in avi.c,
outside `HW_ENABLE_MOVIES`, read `aviRectLast`, which was declared inside it. The declaration moved above the `#ifdef`
(HWI-FIX(release-1.0)).

## Not done / not verified

- **Not run by a person on Windows.** Only the silent paths ran in CI. Nobody has seen the installer's pages (licence,
  folder, Finish with "Start Homeworld Infection"), the uninstaller's components page, SmartScreen's warning, or the
  icons. No game data in CI: the main menu, missions, the Bink movies through FFmpeg and the unpacking of a GOG
  installer by innoextract are not tested on Windows.
- **The data setup is not merged yet.** CI ran main's engine, which exits with 65237 on the first missing data file.
  After the merge, run the workflow again; the datasetup agent says a headless start fails fast with the same code
  and opens no dialog.
- Unsigned: SmartScreen shows "Windows protected your PC" (More info, Run anyway).
- `tools/release_win64.sh`'s developer zip now carries the installer-oriented README; it still builds with movies off.
- The FFmpeg 6.1 branch has later point releases (6.1.6 per Homebrew) with security fixes; all three packages pin
  6.1.2.
- The build script has run only on the GitHub runner. On the development machine it would download FFmpeg and
  innoextract (unless `--ffmpeg` / `--innoextract` are given) and needs `nsis`.
