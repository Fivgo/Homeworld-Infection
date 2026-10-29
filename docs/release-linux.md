# Release 1.0.0: the Linux package

The Linux part of [release-1.0.md](release-1.0.md). The package is `Homeworld-Infection-<version>-Linux-x86_64.tar.gz`,
built by GitHub Actions (`.github/workflows/build-linux.yml`, artifact `package-linux`). It holds one folder,
`Homeworld-Infection-<version>/`, with:

- `Homeworld-Infection-<version>-x86_64.AppImage` (mode 755: the tar keeps the executable bit, so "Extract Here" and
  a double-click start it)
- `README-Linux.txt` (the player's guide: `tools/release/README-Linux.txt` with the version, date and commit filled in)
- `licenses/` (the same licence texts as inside the AppImage)

## Files

| File | What it is |
|---|---|
| `tools/release/build-linux.sh` | the whole package build: SDL2, FFmpeg, the engine, innoextract, the AppDir, appimagetool, the tar.gz |
| `tools/release/smoke-linux.sh` | the CI checks of a built package (below) |
| `tools/release/linux/AppRun` | the AppImage's entry point (`@VERSION@` filled in) |
| `tools/release/linux/homeworld-infection.desktop` | the AppImage's own desktop file (appimagetool reads it; Name, Icon, Categories) |
| `tools/release/linux/io.github.fivgo.HomeworldInfection.metainfo.xml` | AppStream metadata (id `io.github.fivgo.HomeworldInfection`, launchable `homeworld-infection.desktop`), release `@VERSION@` dated by the commit |
| `tools/release/linux/requirements-ci.txt` | meson for the runner, pinned by the wheel's sha256 |
| `tools/release/README-Linux.txt` | the player's README |
| `tools/release/ffmpeg-min.sh` | the shared minimal-FFmpeg build (contract 5) |
| `tools/release/icon/make_icon.py` | the original icon art; `--sizes` draws 16 to 512 px |

## How the package is built

`tools/release/build-linux.sh [--deps DIR] [--out DIR] [--work DIR] [--deps-only] [--cache-keys]`, on the
`ubuntu-22.04` runner (glibc 2.35, so the AppImage runs on distributions with glibc 2.35 or newer: Ubuntu 22.04+,
Debian 12+, Fedora 36+, current Arch, Mint 21+, SteamOS 3). The version comes from `meson.build`'s project version.

1. **SDL2** 2.30.12 from source (CMake) into `--deps/sdl2`. X11 and Wayland (with libdecor), PipeWire, PulseAudio,
   ALSA and libsamplerate are all `*_SHARED`, so SDL dlopens them on the player's machine. dbus and udev are
   always dlopened. `libSDL2-2.0.so.0` itself needs only glibc. The build stops if X11 or Wayland did not get
   configured. The log lists each library SDL dlopens (`SDL_*_DYNAMIC`). Ubuntu 22.04's SDL 2.0.20 would compile
   the engine (every SDL call it makes is from 2.0.6 or older). 2.30 is bundled for its Wayland, input and audio
   fixes, and it is the same version as the Windows package.
2. **FFmpeg** 6.1.2 through `tools/release/ffmpeg-min.sh --target linux` into `--deps/ffmpeg`: LGPL, shared, only
   the bink demuxer and decoders, the file protocol and swscale (movies on).
3. **The engine**: `meson setup -Doptimization=2 -Ddebug=false -Db_sanitize=none -Dbuild_version=<version>
   -Dmovies=true -Dwerror=false`. These are the release flags of `tools/build.sh --release`. `-Dwerror=false` is
   there so that a warning only the runner's GCC 11 gives cannot stop a release; the log counts the warnings.
   `PKG_CONFIG_PATH` points at the two builds above. The binary is not stripped, so a player's crash report keeps
   function names.
4. **innoextract** 1.9: the official Linux release tarball. Its `bin/amd64/innoextract` becomes
   `usr/bin/innoextract`. That binary is built for Linux 2.6.9 and needs only glibc's `libc.so.6` and `libm.so.6`.
   Boost, libbz2, liblzma, libstdc++ and zlib are built into it, and their licences come from the tarball's
   `license/` folder.
   Contract 3 puts it beside `usr/bin/hwi-engine`, where the engine looks first (`SDL_GetBasePath()`). The
   engine runs it as `innoextract --gog --exclude-temp --language english ... --output-dir <tmp> -- <installer>`.
   The GOG installer of Homeworld: Emergence is Inno Setup setup data 5.5.7, which 1.9 handles.
5. **The AppDir**:
   - `AppRun` and `homeworld-infection.desktop` at the top, `homeworld-infection.png` (256 px) with `.DirIcon`
     linked to it;
   - `usr/bin/{hwi-engine,innoextract}`;
   - `usr/lib/` holds every library that `ldd` resolves into the two third-party builds, copied under its soname:
     `libSDL2-2.0.so.0`, `libavformat.so.60`, `libavcodec.so.60`, `libavutil.so.58`, `libswscale.so.7`;
   - `usr/share/applications/`, `usr/share/icons/hicolor/<n>x<n>/apps/homeworld-infection.png` (n = 16, 32, 48, 64,
     128, 256, 512, from `make_icon.py --sizes`), `usr/share/metainfo/io.github.fivgo.HomeworldInfection.metainfo.xml`;
   - `usr/share/doc/homeworld-infection/`: `LICENSE-REULA.txt`, `LICENSE-SDL2.txt` + `SOURCE-SDL2.txt`,
     `LICENSE-FFmpeg.md` + `COPYING-FFmpeg-LGPLv2.1.txt` + `SOURCE-FFmpeg.txt` (the source URL),
     `LICENSE-innoextract.txt` (the binary's own `--license` output) and `LICENSE-innoextract-<file>.txt`. The
     latter are the tarball's `license/` files: `innoextract.LICENSE` (zlib), `Boost.LICENSE_1_0`,
     `libbz2.LICENSE`, `liblzma.COPYING`, `libstdc++.COPYING3` with `libstdc++.COPYING.RUNTIME`, and
     `zlib.copyright`. Last comes `README-Linux.txt`.

   `patchelf` sets the engine's RUNPATH to `$ORIGIN/../lib` and each bundled library's to `$ORIGIN`. AppRun sets
   no `LD_LIBRARY_PATH`, so the host programs the engine starts (zenity, kdialog, xdg-open) never load the bundled
   libraries. Not bundled, from the player's system: glibc, libGL/GLX (the graphics driver), libX11 (the engine
   links it for the lock-key handling; every desktop has it), and the X11, Wayland and audio client libraries
   that SDL dlopens.
6. **appimagetool** 1.9.1, extracted with `--appimage-extract` (no FUSE on the runner), with
   `--runtime-file runtime-x86_64` from type2-runtime 20251108. That is the static runtime: the player needs no
   libfuse2, which Ubuntu 24.04 does not install. The AppImage carries no update information. `--no-appstream` is
   given, and CI validates the metadata on its own. Then a reproducible tar (`--sort=name`, owner 0, the commit's
   time, `gzip -n`).

The third-party builds are cached by key (`--cache-keys`): `sdl2-<version>-r<recipe>-<distro>`, and for FFmpeg
`ffmpeg-<version>-<first 16 hex of the sha256 of ffmpeg-min.sh without its FFMPEG_SHA256 line>-<distro>`. A whole
run takes about 5 minutes, of which 2 are SDL2 and FFmpeg on a cache miss. Bump `SDL2_RECIPE` in `build-linux.sh` when
SDL's options change. A `.hwi-key` stamp in each build folder lets a restored cache skip the build.

## What is pinned

Every download is checked against its sha256 (`fetch()` in `build-linux.sh`). A mismatch stops the build. An empty
pin prints the sum and a warning; `HWI_REQUIRE_PINS=1`, which the workflow sets, turns that into an error. The
workflow's actions are pinned by commit. The SDL2 and FFmpeg sums agree with Homebrew's formulas. innoextract's sum
was trusted on first use: there is no published digest, only a GPG signature. It was taken from the runner,
never downloaded on the development machine.

| What | Version | sha256 | Where the pin comes from |
|---|---|---|---|
| SDL2 source `SDL2-2.30.12.tar.gz` | 2.30.12 | `ac356ea55e8b9dd0b2d1fa27da40ef7e238267ccf9324704850d5d47375b48ea` | first CI download, run 40975277840; the same as Homebrew's sdl2 formula |
| FFmpeg source `ffmpeg-6.1.2.tar.xz` (in `ffmpeg-min.sh`) | 6.1.2 | `3b624649725ecdc565c903ca6643d41f33bd49239922e45c9b1442c63dca4e38` | Homebrew's ffmpeg@6 formula; the Linux, Windows and macOS CI downloads match |
| innoextract `innoextract-1.9-linux.tar.xz` | 1.9 | `008efe5011476ccc4aae17c3e22038b5a1bc5c7aad2b9d4d869537bf3874d21f` | first CI download, run 40975277840 |
| appimagetool `appimagetool-x86_64.AppImage` | 1.9.1 | `ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0` | GitHub's release-asset digest |
| type2-runtime `runtime-x86_64` | 20251108 | `2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d` | GitHub's release-asset digest |
| meson wheel (PyPI) | 1.3.2 | `0ba4a71fbc060c44721c7b674807598c5af9ea51335073cae7a3e9a95b375c89` | PyPI's digest |
| actions/checkout | v7.0.1 | commit `3d3c42e5aac5ba805825da76410c181273ba90b1` | |
| actions/cache (restore, save) | v6.1.0 | commit `55cc8345863c7cc4c66a329aec7e433d2d1c52a9` | |
| actions/upload-artifact | v7.0.2 | commit `cf430e030ddbb5b0abf93d22962f4752f3646cd9` | |

The Ubuntu packages (compiler, CMake, patchelf, Pillow, the SDL headers, Xvfb, Mesa) come from the runner's signed
apt archive.

## AppRun

`tools/release/linux/AppRun` (POSIX sh). On each start it does three things.

1. **The menu entry.** It writes `${XDG_DATA_HOME:-~/.local/share}/applications/homeworld-infection-appimage.desktop`
   with `Exec="<the AppImage's path>"`, `TryExec=<the same>`, `Icon=homeworld-infection-appimage`,
   `StartupWMClass=homeworld-infection-appimage` and `Categories=Game;StrategyGame;`. The Exec value follows the
   desktop-entry quoting rules: `\ " ` $` get a backslash, the string rule doubles backslashes, and `%` becomes
   `%%`. It installs the seven icon sizes as `icons/hicolor/<n>x<n>/apps/homeworld-infection-appimage.png`.
   - A file is written only when its content differs, so the step is idempotent. A moved AppImage rewrites its
     entry on its next start. TryExec hides the entry once the AppImage is deleted.
   - After a change it runs `update-desktop-database`, and `gtk-update-icon-cache` when an icon cache exists.
   - The names differ from `setup.sh`'s `homeworld-infection.desktop` and `homeworld-infection.png`, so a development
     checkout's shortcut and the AppImage's never overwrite each other.
   - It skips the step when started from an extracted AppDir (no `$APPIMAGE`) or with `HWI_NO_MENU_ENTRY=1`.
2. **The working directory.** If the AppImage's own folder holds `cataclysm.big` and the working directory does
   not, it changes to the AppImage's folder. An AppImage dropped into the game folder then finds the data
   (contract 2's working-directory step).
3. **The engine.** It exports `SDL_VIDEO_X11_WMCLASS`, `SDL_VIDEO_WAYLAND_WMCLASS` and `SDL_APP_ID` =
   `homeworld-infection-appimage`, so docks match the window to the entry, and starts `usr/bin/hwi-engine "$@"`
   under `flock -n -o ~/.homeworld/.appimage.lock`. That allows one game at a time. A second start says "already
   running". `-o` keeps the lock out of programs the engine starts.
   - From a terminal, the output stays on the terminal.
   - Otherwise it goes to `~/.homeworld/hwi-engine.log`, and the previous run's log is kept as
     `hwi-engine.log.old`. A non-zero exit other than HUP, INT or TERM then shows a zenity, kdialog or
     notify-send message that names the log.
   - The engine's own data-setup exits 0 when the player cancels, and shows its own boxes for its errors.
   - `HWI_DATASETUP_NONINTERACTIVE=1` turns AppRun's messages off, as it does the engine's.

Tested locally with a stub engine in a throwaway HOME. The script is
`tools/release/linux/AppRun` driven through a scratch harness: it was run with dash, busybox sh and bash, with no
display and dialogs off. The harness checked:

- the first start writes a valid entry (`desktop-file-validate`) and the 7 icons, and the log file;
- a second start leaves the files untouched, and the old log is kept;
- a move to a path with a space, `$`, backquotes, double quotes and a backslash rewrites the entry, and GLib parses
  its Exec back to the exact path;
- for a path with `%`, the entry is written with `%%` and validates. GLib's own DesktopAppInfo still refuses that
  entry: it checks argv[0] before unescaping `%%`. That is a GLib quirk;
- the working-directory rule, the lock, exit-code pass-through and the error message;
- no entry without `$APPIMAGE`, and a relative `XDG_DATA_HOME` is ignored.

CI repeats the entry checks on the real AppImage (below).

## How CI tests it

`tools/release/smoke-linux.sh <tar.gz>` runs after the upload in the same job:

1. The tar: the AppImage is still executable after extraction, and README-Linux.txt and licenses/ are present.
2. `--appimage-extract`, the file list, then `ldd` on `usr/bin/hwi-engine` and every `usr/lib/*.so*`. Every library
   must resolve, either inside the AppImage (bundled) or to a system library on an allow-list: glibc's, libGL,
   libGLX, libGLdispatch, libX11 and its xcb, Xau, Xdmcp, bsd and md chain. The table goes into the job summary.
   The engine's RUNPATH must be `$ORIGIN/../lib`. libSDL2 must not link any window-system or audio library
   directly. The AppImage's desktop file must pass `desktop-file-validate`, and `appstreamcli validate` on the
   metainfo is reported but never fails the job.
3. `usr/bin/innoextract --version`.
4. The engine with no game data, under `xvfb-run` (Mesa's llvmpipe), with `SDL_AUDIODRIVER=dummy`,
   `HWI_DATASETUP_NONINTERACTIVE=1` (otherwise the data-setup prompt would wait on Xvfb) and a 120 s timeout. It
   must exit by itself, not hang (timeout's 124 or 137) and not die from a signal (129 to 192). Without data
   the engine prints `Unable to open file: Music4Cat.wxd` and exits with ERR_ErrorStart, 0xfed5, which shows as 213.
   Two runs:
   - the binary with `HW_Data` set to an empty folder;
   - the AppImage itself (`APPIMAGE_EXTRACT_AND_RUN=1`: runtime and AppRun) with no `HW_Data`, in a throwaway HOME.
5. AppRun's menu entry in that HOME:
   - it is written and valid, Exec is the AppImage's path, and the 7 icons and the log are in place;
   - a second start leaves the entry untouched and keeps `hwi-engine.log.old`;
   - a copy at `.../moved dir $x/Homeworld Infection.AppImage` rewrites Exec, correctly escaped and still valid.

CI never gets game data, so nothing past the data check runs there. The game itself is tested on a player's
machine.

## Running it elsewhere

The script also runs on a development machine with the packages the workflow installs. It downloads and runs the
pinned third-party code (the SDL2 and FFmpeg sources, the innoextract binary, appimagetool), so it is meant for
CI. `--deps`, `--work` and `--out` default to `build/linux-deps`, `build/linux-package` and `dist/`, which are all
gitignored.
