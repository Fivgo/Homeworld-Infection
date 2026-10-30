# Finding the game files (HWI-FEATURE(data-setup), release 1.0.0)

The user (2026-10-29): "The intention is that all the user needs is a gog installation or the 300MB offline setup to have
the mod work. [...] please refrain from having a user have to run console commands for the deployment process."
(docs/release-1.0.md, contract 2.)

The engine finds the game's data folder by itself. A new player who has neither a GOG installation nor the installer in
a known place gets one plain question. No environment variable, no working directory, no terminal.

Code: `src/SDL/datasetup.c` (`dsGameDataFind`), called from `utyGameSystemsPreInit` (`src/SDL/utility.c`) when `HW_Data`
is not set. Every step logs a `datasetup:` line on stdout.

## The order

1. **`HW_Data`** set: used as it is. `tools/play.sh` and `tools/m2_harness.sh` set it. Nothing below runs, and no dialog
   is shown.
2. **The working directory**, if it holds `cataclysm.big`. This is the old Windows zip layout: `hwi-engine.exe` put in the
   GOG folder. This folder is not saved.
3. **The saved path**: `gamedata-path.txt` in the settings folder (one line), if that folder still holds the game files.
4. **The options file's `HomeworldDataPath` row** (GoK's own saved path; every run writes it into `Cataclysm.cfg`), if
   that folder still holds the game files. Before 1.0.0 this row was used without any check.
5. **An installed GOG copy** (auto-detect, below). The first one that holds the game files is used and saved, without a
   question.
6. **The GOG offline installer** (`setup_homeworld_emergence*.exe`, at least 1 MB, the newest one) in Downloads, on the
   Desktop, in the home folder, beside the AppImage or in the working directory. The player is asked: [Unpack]
   [Choose something else] [Quit].
7. **Ask**: "Homeworld Infection needs the game files of Homeworld: Emergence, from your copy of the game on GOG.com"
   with [Choose the GOG game folder] [Choose the offline installer] [Quit]. A folder that does not hold the files gets
   an error that names what is missing, then the question again. A chosen folder whose `Homeworld Emergence` or `app`
   subfolder holds the files is accepted too. Cancel in a picker returns to the question.

Quit (or closing the box, or Escape) ends the program with exit code 0 and no error line.

### What counts as the game files

`cataclysm.big`, `update.big`, `Music4Cat.wxd`, `SectSpeech.vce`, `BeastSpeech.vce`, `default_keys.cfg`, `devstats.dat`
and the folders `Movies`, `feman` and `credits`. The log gives `cataclysm.big`'s size. GOG 1.01 (installer build 12948)
is 76864402 bytes; another size gets a warning and is still allowed.

### Auto-detect

| platform | where |
|---|---|
| all | the per-user GameData folder (an earlier unpack) |
| Windows | the registry value `path` under `HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\1766413869` (the GOG offline installer and GOG Galaxy both write it), the same key through the 32-bit and the 64-bit views; `%SystemDrive%\GOG Games\Homeworld Emergence`, `D:\GOG Games\Homeworld Emergence`, `%ProgramFiles(x86)%` and `%ProgramFiles%\GOG Galaxy\Games\Homeworld Emergence` |
| Linux, macOS | `~/GOG Games/Homeworld Emergence` (Wine's and minigalaxy's default); `~/Games/*/Homeworld Emergence` (Heroic); `~/Games/*/drive_c/GOG Games/...` and `~/Games/*/drive_c/Program Files (x86)/GOG Galaxy/Games/...` (Lutris prefixes); `~/.wine/drive_c/` (both); Bottles (`~/.local/share/bottles/bottles/*` and the flatpak's); `~/.local/share/Steam/steamapps/compatdata/*/pfx/drive_c/GOG Games/...` (the installer added to Steam) |
| macOS | also CrossOver bottles (`~/Library/Application Support/CrossOver/Bottles/*/drive_c/GOG Games/...`) |

GOG has no Linux or macOS build of this game, so on those systems it is the Windows installer under Wine, a launcher or
the offline installer.

## The folders

| | Windows | Linux | macOS |
|---|---|---|---|
| settings (`gamedata-path.txt`, options, keys, saves) | `%APPDATA%\Homeworld-Infection` | `~/.homeworld` | `$HOME/` + `CONFIGDIR` (`regkey.h`; the macOS package sets it to `Library/Application Support/Homeworld-Infection`) |
| unpacked installer | `%LOCALAPPDATA%\Homeworld-Infection\GameData` | `${XDG_DATA_HOME:-~/.local/share}/homeworld-infection/GameData` | `~/Library/Application Support/Homeworld-Infection/GameData` |

`/settingspath <dir>` moves the settings folder, and `gamedata-path.txt` moves with it. To choose another folder later,
delete `gamedata-path.txt`. Because auto-detect checks GameData first, also delete or move GameData if it exists. The
unpacked copy (66 files, 404999701 bytes) is never deleted by the engine.

## A GOG folder is only read

A GOG installation holds the original game's own `Cataclysm.cfg` (GOG's post-install copy, then the original game's
options), `keys.cfg`, `AutoLock.txt` and `SavedGames`. GoK's `fileOpen` (`src/Game/File.c`) looked for a file in the
data folder first, even for a settings file opened for writing. With a GOG folder as the data folder, the recreation
therefore wrote its options into the original game's `Cataclysm.cfg`, recreated `AutoLock.txt` there and read the
original game's `keys.cfg`. That also happened with the old Windows zip layout (the exe in the GOG folder).

The fix (HWI-FEATURE(data-setup) in `fileOpen`): a file opened with `FF_UserSettingsPath` never comes from the game
folder, for reading or writing. `fileExists`, `fileSizeGet` and `fileSave` already used the settings folder only.

This was found while testing data-setup on 2026-10-29: two test runs that used the user's untouched GOG copy
(`original/gog/game`, the one `tools/play_original.sh` plays) overwrote the 2138-byte `Cataclysm.cfg` that cataclysm.exe
had written at 17:07. That file is lost. It was reset to the GOG install's own `language=0`. The 66 installer files,
`keys.cfg` and the user's save were not touched. Tests now use a scratch folder of links to the 66 installer files only.

Regression test: see "Tests". The data folder holds its own `Cataclysm.cfg`, `keys.cfg`, `AutoLock.txt`, `netlog.txt`, a
`SavedGames` save and a `ScreenShots` file (real files; the big files are links) and is made read-only
(`chmod -R a-w`). After a full run (a mission with a recording and a screenshot, the options saved at the quit), every
file there is byte-identical with the same mtime, and everything the engine wrote is in the settings folder.

## Unpacking the offline installer

1. The installer is checked first: it must be a file and must not be GOG Galaxy's download helper
   (`GOG_Galaxy_*.exe`, about 0.5 MB, which holds no game). There must be about 450 MB free next to GameData.
   `innoextract --gog-game-id` must read the installer and report GOG game 1766413869; another id is refused.
2. innoextract runs into a temp folder beside GameData (`.../homeworld-infection/unpack.tmp`, `...\Homeworld-Infection\unpack.tmp`):

   `innoextract --gog --exclude-temp --language english --progress=1 --color=0 --output-dir <temp> -- <installer>`

   Its `-N.bin` parts, if any, must sit beside the `.exe`, where innoextract finds them itself.
3. `<temp>/app` (Inno's `{app}`) is checked for the game files and renamed to GameData, then the temp folder is removed.
   A GameData that is in the way and not empty is renamed `GameData.old-<date>-<time>`, never deleted. The path is saved.

innoextract is looked up beside the engine through `SDL_GetBasePath()`: `innoextract.exe` beside `hwi-engine.exe`, and
`usr/bin/innoextract` beside `usr/bin/hwi-engine` in the AppImage. On macOS the lookup goes to the executable's folder
first (`Contents/MacOS/innoextract`), because SDL's base path is `Contents/Resources`, then the base path, then
`../MacOS`, then Homebrew's folders. Development builds fall back to `innoextract` on PATH. The system innoextract 1.9
handles the GOG installer (Inno Setup data version 5.5.7). On Windows the child runs without a console window
(`CreateProcessW`, `CREATE_NO_WINDOW`).

While innoextract runs, a small window shows a bar and the percentage in its title. It uses SDL's software renderer on
the window's own framebuffer (`SDL_HINT_FRAMEBUFFER_ACCELERATION` 0) and never creates a GL context, because this
machine's NVIDIA + Xwayland has trouble re-creating GL windows. The window is destroyed afterwards. The SDL video
subsystem stays up, and the event queue is flushed so the game's loop gets no stray quit. Closing the window stops
innoextract and removes the temp folder. A failure shows a box with the reason and innoextract's last message, then the
question again.

## Dialogs and pickers

- **Questions and errors**: `SDL_ShowMessageBox`. It works before `SDL_Init`. On Linux, unless `SDL_VIDEODRIVER`
  names x11, SDL may itself use the host's zenity for the box, then its own X11 box.
- **Pickers**: on Windows, `IFileOpenDialog` (`FOS_PICKFOLDERS` for the folder; the installer picker filters on
  `setup_homeworld_emergence*.exe` and starts in Downloads). The engine's paths are `char` in the ANSI code page; a path
  that does not fit gets its short name (`GetShortPathNameW`), else an error that asks for a plain folder name. On
  macOS, `osascript` `choose folder` / `choose file`. On Linux, zenity, then kdialog. Without either, a box names the
  GameData folder and offers [Open that folder] (xdg-open) [Retry] [Quit], so the player can copy the GOG folder's
  files there with the file manager. There is no xdg-desktop-portal picker.

Every child process (innoextract, zenity, kdialog, osascript, xdg-open, open) is started from an argv array with
`posix_spawn` (or `CreateProcessW`), never through a shell command line built from a path.

## Never blocking a run without a player

No dialog or picker is ever shown, and the run fails at once with a `datasetup:` line, the reason on stderr and exit
code `ERR_ErrorStart` (0xfed5, which is 213 as a POSIX exit status), when:

- `HWI_DATASETUP_NONINTERACTIVE` is set (not `0`). CI under xvfb-run sets it.
- `SDL_VIDEODRIVER` (or the SDL hint) is `offscreen` or `dummy`.
- on Linux, neither `DISPLAY` nor `WAYLAND_DISPLAY` is set.

Steps 1 to 5 still run, because they need no dialog. An installer found in step 6 is not unpacked without a question.

## Test switch (HWI-TEST)

`/catDataSetupScript <answers>` replaces every dialog and picker with answers from a `;`-separated list. What would have
been shown is logged (`datasetup: [test] ...`). It also lifts the non-interactive rule, so it works under the offscreen
driver.

| answer | at |
|---|---|
| `folder:<path>` / `installer:<path>` | the question: that button, and that path as the picker's result |
| `folder` / `installer` | the question: that button, then the real picker (tests put stub zenity / kdialog first on PATH) |
| `unpack` / `other` / `quit` | the found-installer question |
| `open` / `retry` / `quit` | the no-picker box (`open` is only logged) |
| `show` | anywhere: the unpack's progress window is not hidden (it is hidden in a test otherwise; for a headless-gamescope capture) |

When the list runs out, the answer is Quit. A box with only OK takes no answer. `/catHiddenWindow` also hides the
progress window.

## Tests (2026-10-29)

Throwaway HOME, XDG_DATA_HOME, XDG_CONFIG_HOME and cwd under the session scratchpad. No HW_Data, no DISPLAY, offscreen
video, dummy audio, stub zenity and kdialog first on PATH. Debug ASan build. The data is a scratch folder of links to
the 66 installer files (never `original/gog/game` itself). While the user played FFXIV, the runs used Mesa's software GL
(llvmpipe through SDL's offscreen EGL: `__EGL_VENDOR_LIBRARY_FILENAMES=.../50_mesa.json SDL_HINT_EGL_DEVICE=2
LIBGL_ALWAYS_SOFTWARE=1`, "OpenGL Renderer: llvmpipe" in every log, the NVIDIA GPU sampled every 0.5 s: hwi-engine
never on it) at `nice -n 19 ionice -c3`. LeakSanitizer then reports 2 to 4 allocations (112 bytes and up) made inside
the unloaded Mesa driver under `glTexImage2D`, like `tools/lsan.supp`'s libnvidia-glcore line, so those runs exit 1.
Every other leak report would be counted, and there was none.

| test | switches / setup | result |
|---|---|---|
| (a) a GOG folder chosen | `folder:<mirror>` then a second start without a script | saved in `.homeworld/gamedata-path.txt`; the second start: "using the path saved in ..." with no question; both reach the main menu (`/catStringsTest` PASS) |
| (b) the offline installer chosen | `installer:<the real setup_homeworld_emergence_1.01_hotfix_(12948).exe>` | innoextract 1.9 (system): 66 files, 404999701 bytes, 17.3 s at idle I/O priority; `sha256sum -c` of the GOG MANIFEST: all 66 match; the temp folder removed; the same process goes on to the main menu (PASS); a second start uses the saved GameData and draws 120 frames of the main menu (`/catModeQuitTest 1280x720:120`, PASS) |
| (c) auto-detect | `~/GOG Games/Homeworld Emergence` = links to the 66 files | found, saved, main menu PASS, no question |
| (d) installer in Downloads | `XDG_DOWNLOAD_DIR="$HOME/Téléchargements"`, the installer linked there, `unpack` | found, the question logged, unpacked in 14.3 s, all 66 match, main menu PASS |
| (e) Quit | `quit` | exit 0, no error line, no LSan report |
| (f) cannot ask | offscreen; `HWI_DATASETUP_NONINTERACTIVE=1`; no driver and no DISPLAY; offscreen with an installer in ~/Downloads | exit 213 in 0.1 s each, the reason on stdout and stderr; the installer is not unpacked |
| (g) broken installers | a 150 MB head, a 2 MB head, 3 MB of random bytes, GOG Galaxy's stub, the whole installer with 4 MB zeroed at 200 MB | "Could not determine setup data version!" (both heads), "Not a supported Inno Setup installer!", the Galaxy-helper text, and for the zeroed one innoextract stops at 60%: "Stream error while extracting files! / error reason: lzma decrompression error" in the box; the temp folder removed each time; back to the question; Quit exits 0 |
| pickers | stub zenity printing the folder; stub zenity cancelling; two wrong folders; no zenity or kdialog on PATH | the path taken and saved; cancel = the question again; "That folder does not hold the game files ... Missing: ..." each time; "no zenity / no kdialog" then the copy-by-hand box (open logged, retry names what is missing, quit); the next start finds the copied GameData |
| options row | `HomeworldDataPath /nonexistent/game/` in Cataclysm.cfg; then a valid one | the stale row is ignored and the question asked; the valid row is used and saved |
| HW_Data | `HW_Data` = the mirror; `HW_Data` = an empty folder | no `datasetup:` line at all; the menu PASS; the empty folder: "Unable to open file: Music4Cat.wxd", exit 213, as before |
| read-only data folder (the regression test above) | the decoy folder, `chmod -R a-w`; `/oracleMission 600 /autosavedebug /catShot 300` | the mission ran, three autosaves, the shot and the options at the quit: all in `.homeworld/` (AutoLock.txt, Cataclysm.cfg, keys.cfg, SavedGames/AutoSaveDebug0..2, shot.300.0.ppm, gamedata-path.txt, mem.*, Randylog.txt, texturesUsage.list); the data folder unchanged (mtime, mode, size, sha256). The same test on the engine without the fix: "Fatal error - fileOpen: cannot open file .../ro-decoy/AutoLock.txt", exit 213 |

Not tested here: the Windows code (the registry, IFileOpenDialog, CreateProcessW, a Windows innoextract.exe; the cross
build compiles and links under -Werror, and its imports are within the release whitelist), the macOS code
(osascript, the bundle's innoextract lookup), kdialog itself, and the visible progress window on a real desktop.
