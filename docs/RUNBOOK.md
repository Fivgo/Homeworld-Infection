# RUNBOOK: GOG Homeworld: Emergence (Cataclysm) on this Linux box

From "installer downloaded" to "game window on screen", plus the same for Windows 11.
Machine: Pop!_OS 24.04, COSMIC on Wayland (Xwayland `:1`), RTX 5090 (driver 580-open),
GE-Proton10-34, gamescope 3.14.24 (`/usr/local/bin`). Written 2026-10-08 09:58, updated 14:05 after the first real game run, verified + corrected 14:20, re-verified 20:00-20:15 (second pass; see both Verification sections at the end).

## Status (2026-10-08 14:05)

| Step | State | Evidence |
|---|---|---|
| Offline installer | **present**: `original/setup_homeworld_emergence_1.01_hotfix_(12948).exe` | 360,195,200 B, sha256 `9e39851eff77e522df388e46eb4a3b744ff497e1784d44d97d0c14544ba335ee`, Inno Setup data 5.5.7 (unicode), GOG id 1766413869, single file (no `.bin` parts), 6 languages (english german french russian brazilian chinese) with **identical** file lists |
| `original/game/` | **extracted**, then CET installed and the game run once | 66 pristine files, 404,999,701 B (`MANIFEST.sha256`, written 09:30; `extract_installer.sh` re-extraction into a scratch dir gave a byte-identical `MANIFEST.sha256` (5,875 B both, `logs/extract_installer-20261008-094019.log:90`); the 09:30 `MANIFEST.sizes` is **not** identical: it carries a stale self-entry `0  ./MANIFEST.sizes` (67 lines vs 66; 2,052 B vs the script's 2,032 B) - harmless, `--verify` skips `./MANIFEST.*`). `--verify` at 13:56 (all expected): `rgl.dll`/`rgld3d.dll` FAILED (CET builds), EXTRA `cataclysm_loader.exe CataclysmEngineTweaks.{dll,ini,log} update102.big` (CET) and `Cataclysm.cfg keys.cfg AutoLock.txt` (written by the game at 13:54:57) |
| Wine prefix `prefix/` | **created + verified** | GE-Proton10-34 via umu-run 1.4.4; `setup_prefix.sh --check` (14:03) -> `Microsoft Windows 10.0.19045` in 3 s; a fresh prefix from nothing (runtime already cached) took 4.4 s |
| CET | **installed** v0.18.03 into `original/game/` at 13:54:34 | done with `install_cet.sh` by a parallel agent of this session (copies of these scripts in `scratchpad/snap/tools/`); stock DLLs in `original/game/.cet-backup/20261008-135434/`; all 6 files hash-checked against the zip. Re-running the installer now reports "already installed ... nothing to do" |
| Launch | **game reached the main menu** under `run_game.sh` defaults (that agent, 13:54:50-13:59:36) | run log + window list were written to the session scratchpad (`snap/logs/`, scripts run from a copy) and copied by the verifier to `logs/run_game-20261008-135450.log`, `logs/install_cet-20261008-135434.log`, `logs/screenshot-20261008-firstrun.log`; frames kept in `docs/screenshots/2026-10-08-first-run-main-menu-{gamescope1080,inner640}.png`. `CataclysmEngineTweaks.log`: `RGL patches applied`, `Update rate patches applied`, `Patches applied`, `DSound init OK`, `Universe reset called`. gamescope screenshot = 1920x1080 frame with the 640x480 front end centred (HW1 behaviour: `FEFlow.c:1546-1561`). Prefix registry afterwards: `screenWidth=1920 screenHeight=1080 screenDepth=32 glToSelect=opengl32.dll` (CET first-run OpenGL switch, `PatchRGL.cpp:40-54`). Process tree while running: `cataclysm_loader.exe` gone, `umu.exe` still waiting on `cataclysm.exe` -> the gamescope session stays up after the loader exits |
| Screenshots | gamescope (real game + notepad), X11, desktop verified | `grim` does **not** work on COSMIC |

**Commands for a run (CET is already installed):**

```bash
cd ~/Documents/GameDev/Homeworld-Infection
tools/run_game.sh            # 1920x1080 gamescope WINDOW -> cataclysm_loader.exe /noswddraw
tools/screenshot.sh          # -> logs/screenshots/auto-<ts>.png (path on the last line)
tools/run_game.sh --kill     # from another terminal: wineserver -k, gamescope exits
```

From a fresh extraction run `tools/install_cet.sh --no-download` first. Other window sizes: `tools/run_game.sh --size 1280x960 --sync-res` (section 4).

The five runbook scripts (`extract_installer.sh`, `setup_prefix.sh`, `install_cet.sh`, `run_game.sh`, `screenshot.sh`): bash, `set -euo pipefail`, `PROJECT_ROOT` = parent of `tools/`, `--help`, `--dry-run`
(except screenshot.sh, which has no `--dry-run`: it always writes a PNG + log), log to `logs/<script>-<timestamp>.log` (screenshot.sh: one log per day).
`original/`, `original/game/` and the installer file itself may be symlinks (e.g. to another disk): since the second verification pass the installer search uses `find -L` (`extract_installer.sh:155,158,188`), every `find` on the game dir uses `find -H`, and `extract_installer.sh` resolves the game dir with `readlink -f` (`:17`) so `--force` moves the real directory aside instead of only the link. Before, a symlinked `original/` or installer made `extract_installer.sh` report "no setup_homeworld_emergence*.exe" and a symlinked `original/game` made `run_game.sh`/`install_cet.sh` report no game directory (reproduced in a scratch tree, then re-tested OK).
Other files in `tools/` are **not** covered here: `ghidra_headless.sh` (Ghidra, `docs/ghidra-pipeline.md`), `bigfile.py`, and the rate-limit cycle `checkpoint.sh` / `qwen_worker.py` / `systemd/hwi-qwen-worker.{service,timer}` (`cycle/STATE.md`).
`.gitignore` (committed in `5334201`) covers `original/`, `reference/`, `prefix/`, `build/`, `*.log`, `logs/` (`.gitignore:14`, so `logs/screenshots/*.png` are ignored) and `docs/screenshots/` (`.gitignore:19`, verified with `git check-ignore -v`): the first-run main-menu frames (copyrighted game art) stay on this machine only
(captures can contain the user's desktop or copyrighted game frames).

---

## 0. Getting the installer (done once)

1. https://www.gog.com -> log in -> avatar menu -> **Games** (library) -> **Homeworld: Emergence**.
2. In the game card: **"Download offline backup game installers"** -> Windows -> English.
   Download `setup_homeworld_emergence_*.exe` and any `setup_homeworld_emergence_*-N.bin` listed with it
   (this build has none: one 344 MiB `.exe`).
3. Put it in `original/` under its original name.

Do **not** use `GOG_Galaxy_Homeworld_Emergence.exe` (the "Download with GOG Galaxy" button): 504,904 B,
UPX-packed, VERSIONINFO `InternalName "GOG Galaxy - Game Installer.exe"`, `ProductID 1766413869`,
`FileVersion 2.0.0.2`, PE timestamp 2020-04-09. It only drives Galaxy and contains no game data.
`extract_installer.sh` refuses it (exit 3) by file name and by that VERSIONINFO string.

## 1. `tools/extract_installer.sh` - unpack + manifest + exe report

```
tools/extract_installer.sh [--installer PATH] [--language english] [--force] [--list] [--info]
                           [--verify] [--report] [--innoextract PATH] [--build-innoextract]
                           [--wine-install [--silent]] [--dry-run]
```

| Step | Detail |
|---|---|
| find | `original/setup_homeworld_emergence*.exe` (newest if several) + `<stem>-N.bin` parts; refuses `GOG_Galaxy_*` / any file whose UTF-16 strings contain `GOG Galaxy - Game Installer` |
| pre-flight | `innoextract --info`; aborts (unless `--force`) on `Unexpected setup data version` / loader-revision / checksum warnings |
| extract | `innoextract --gog --exclude-temp --language english --output-dir original/game/`. `--language` only pins language-tagged entries: build 12948 has none (`innoextract --list --language X` gives the same 68 `app/` entries for all six languages = 66 unique files; `goggame-1766413869.ico` and `webcache.zip` are stored twice, innoextract reports "overwritten") |
| flatten | innoextract maps Inno `{app}` to `app/`; the script moves `app/*` up so `cataclysm.exe` is at `original/game/cataclysm.exe` (layout the rest of the project uses) |
| manifest | `MANIFEST.sha256` (`sha256sum` format, `./path`, `sort -z` order) + `MANIFEST.sizes` (`<bytes>  ./path`); same format as the existing files; excludes `MANIFEST.*`, `EXTRACT_INFO.txt`, `.cet-backup/` (the 09:30 `MANIFEST.sizes` predates that exclusion and lists `0  ./MANIFEST.sizes`) |
| info | `EXTRACT_INFO.txt`: installer name/size/sha256, innoextract version, GOG id, full `--info` |
| GOG post-install | copies `__support/app/*` into the game root if absent (here: `Cataclysm.cfg` = `language=0`). INFERRED from GOG's `__support/app` convention, not from the Inno script; not in the manifest. **Not handled:** `__support/add/` = nGlide **1.05** (`glide2x.dll` 1,372,160 B, `nglide_readme.txt` "Version: 1.05") vs nGlide **1.04** in the root (`glide2x.dll` 1,363,968 B, same FileVersion string 2.61.00.0658); whether GOG's installer copies `add/` over the root is UNVERIFIED, so the root keeps 1.04. GOG's install action (`goggame-1766413869.script`: `"action": "Execute"`, `galaxy_homeworld_emergence_2.0.0.11.exe`, `runAsAdmin: true`) is the likely writer of any install-time registry keys / `__support` copies (UNVERIFIED, not run) |
| report | every `.exe`: `file`, size, sha256, PE32 format, linker, **PE timestamp**, image base, entry RVA, SizeOfImage, Characteristics/DllCharacteristics, sections, PDB path if any, and for `cataclysm.exe` a CET check (base must be 0x400000, CET `dllmain.cpp:11-14`) |

Exe report for this build (from `--report`):

| exe | size | PE timestamp (UTC) | base / flags |
|---|---|---|---|
| `cataclysm.exe` | 5,369,856 | 2000-12-21 20:18:50 (`0x3a4265aa`), linker 6.0 | 0x400000, `RELOCS_STRIPPED`, no `DYNAMIC_BASE`, SizeOfImage 0x894000, entry RVA 0x27bbe0, sections `.text .rdata .data .cms_t .cms_d .idata .rsrc` -> OK for CET |
| `nglide_config.exe` | 344,064 | 2015-01-07 00:58:41 | 0x400000, `RELOCS_STRIPPED` (nGlide 1.04 configurator) |
| `__support/galaxy_homeworld_emergence_2.0.0.11.exe` | 1,737,200 | 2014-12-11 10:42:56, linker 2.25 | GOG Galaxy helper; the "Install the game" action of `goggame-1766413869.script` (not run by these tools) |

Verify any time: `tools/extract_installer.sh --verify` (runs `sha256sum -c`, checks sizes, lists files not in the manifest - after CET you expect `./rgl.dll: FAILED`, `./rgld3d.dll: FAILED`, `SIZE CHANGED: ./rgl.dll`, `SIZE CHANGED: ./rgld3d.dll` + `EXTRA` CET files, exit 1).

innoextract support (tested 2026-10-08 against jrsoftware/issrc release installers):

| innoextract | Inno Setup installers parsed cleanly | notes |
|---|---|---|
| 1.9 (apt, `/usr/bin`) | 6.1.2, 6.2.2 (data 6.1.0); **this GOG installer (5.5.7)** | 6.3.3+ -> `Unexpected setup data version` |
| git master `6e9e34e` (2025-02-06, "1.10-dev") | + 6.3.3 | 6.4.3, 6.5.4, 7.1.0 still warn; includes 2025 fixes for GOG Galaxy installer layouts |

(The git-master row comes from a scratch build that was not kept: `build/` does not exist now and only `scratchpad/ie-build.log`/`ie-cmake.log` remain, so commit `6e9e34e` and the 6.3.3-6.5.4/7.1.0 results are UNVERIFIED here; the real installer needed only the apt 1.9, `logs/extract_installer-20261008-135601.log:3`.) `--build-innoextract` clones + builds git master into `build/innoextract-git/` (needs `cmake g++ liblzma-dev libboost-{iostreams,filesystem,date-time,system,program-options}-dev`; the Boost dev packages were apt-installed for this test) and is then used automatically. Build takes ~5 s on 24 cores.

`--wine-install` (fallback only, not needed for this installer): runs GOG's real installer GUI in a throwaway prefix `build/installer-prefix` under **Proton Experimental** with `/DIR=Z:\...\original\game` preset; you click through it. Findings behind that design:
- GE-Proton10-34 **crashes** every Inno Setup stub tried (fixture 6.1.2 and this GOG installer, exit 5): they have `Characteristics 0x818f` (`RELOCS_STRIPPED`) *and* `DllCharacteristics 0x8140` (`DYNAMIC_BASE`); GE's Wine maps them away from 0x400000 anyway (Proton log: `Loaded ... at 7BF20000`) and they fault on the first absolute address. Proton Experimental (`version` file `experimental-11.0-20261001`; `11.0-100` is its `CURRENT_PREFIX_VERSION`, `proton:45`) installs the 6.1.2 fixture fine. (The `7BF20000` / `0x7BF90000` load addresses come from scratch Proton logs that no longer exist: UNVERIFIED now; the flag values `0x818f`/`0x8140` of this installer were re-read from its PE header.)
- With `/VERYSILENT` GOG's custom wizard aborts: `Current Page: Welcome / Failed to proceed to next wizard page; aborting.` (`inno_install.log`). Hence GUI by default; `--silent` only for experiments.

## 2. `tools/setup_prefix.sh` - Wine prefix with GE-Proton10-34, no Steam client

```
tools/setup_prefix.sh [--proton NAME|DIR] [--prefix DIR] [--direct-proton] [--reinstall-umu]
                      [--check] [--winetricks VERBS...] [--dry-run]
```

How to run Proton without Steam - options evaluated on this box:

| Method | Result | Why |
|---|---|---|
| `pip install --user umu-launcher` | **fails** | Pop!_OS Python 3.12 is PEP 668 "externally-managed-environment"; umu-launcher is also not on PyPI (`pip index versions` -> no distribution) |
| **umu-launcher 1.4.4 zipapp** (GitHub release `umu-launcher-1.4.4-zipapp.tar`, sha256 `eb590691...c48cc851`) | **chosen, works** | self-contained Python zipapp; installed to `~/.local/share/umu-launcher/1.4.4/umu-run`, symlink `~/.local/bin/umu-run` (on PATH). Downloads Steam Linux Runtime 3.0 "sniper" itself to `~/.local/share/umu/steamrt3` (803 MB, depot 3.0.20260928.262393, pressure-vessel 0.20260925.0) - GE-Proton10-34's `toolmanifest.vdf` requires it (`"require_tool_appid" "1628350"`). With `UMU_ID` set, GE's `proton` never uses `steam.exe` ("Don't use steam if it's not a steam game", `proton:2335-2350`): a unix-path exe (what run_game.sh passes) goes through `c:\windows\system32\umu.exe` (`:2345-2347`, log line `Proton: Executable a unix path, launching with /unix option.`), a prefix-internal program (`cmd`, `notepad`) is started by `wine64` directly (`:2348-2350`, `Executable is inside wine prefix, launching normally.`); `steam.exe` only with `UMU_USE_STEAM=1` (`:2342-2344`). No Steam client involved |
| `proton waitforexitandrun` directly (`--direct-proton`) with `STEAM_COMPAT_CLIENT_INSTALL_PATH=~/.local/share/Steam STEAM_COMPAT_DATA_PATH=prefix` | works, fallback | runs on host libraries (no sniper container) and goes through `steam.exe`; console output of the child is **not** relayed (verified by a registry write instead). Shares the same prefix: the script keeps umu's `pfx -> .` layout |

Environment used everywhere (setup_prefix.sh, run_game.sh):

```bash
WINEPREFIX=$PROJECT_ROOT/prefix GAMEID=umu-1766413869 STORE=gog \
PROTONPATH=~/.local/share/Steam/compatibilitytools.d/GE-Proton10-34 PROTON_VERB=waitforexitandrun umu-run <exe> [args]
```

`GAMEID=umu-1766413869` (GOG product id): umu derives `SteamAppId=SteamGameId=1766413869`, so every Wine
window gets X11 `WM_CLASS steam_app_1766413869` (verified: `notepad` and `nglide_config.exe` windows) - that is
what `screenshot.sh` matches. ProtonFixes finds no fix for it (none exists for Homeworld in GE's
`gamefixes-gog`/umu-database; only "Sentinel 3: Homeworld" is listed), which is fine.

Actual run (2026-10-08, `logs/setup_prefix-20261008-094150.log`, `--reinstall-umu` to exercise the download + hash pin):

```
[prefix] + gh release download 1.4.4 -R Open-Wine-Components/umu-launcher -p umu-launcher-1.4.4-zipapp.tar ...
[prefix] umu tarball sha256: eb590691841f7fad3fc3ad8fd5db4ccb87849fe7948e62b28ece7a4ee48cc851
[prefix] installed umu-run -> ~/.local/share/umu-launcher/1.4.4/umu-run (symlink ~/.local/bin/umu-run)
[prefix] probe: cmd /c ver inside the prefix
    INFO: Running 'GE-Proton10-34' using runtime 'sniper'
    Proton: Executable is inside wine prefix, launching normally.
    Microsoft Windows 10.0.19045
[prefix] OK: Microsoft Windows 10.0.19045
    ddraw.dll         Wine builtin
    d3d8.dll          Wine builtin
    d3d9.dll          DXVK (Vulkan)
    opengl32.dll      Wine builtin
    dsound.dll        Wine builtin
    msvcp140.dll      Wine builtin
    vcruntime140.dll  Wine builtin
```

First creation from nothing: ~40 s (runtime download), then 3.9 s per fresh prefix. Wine is `wine-10.0 (Staging)`.
Prefix size ~700 MB (`du -sh prefix` = 729 MB): DLLs are copied, not symlinked (624 of 627 `syswow64/*.dll` are regular files). Cause: GE's `proton` `pfx_copy()` copies every default-prefix file that is not itself a symlink (`proton:910-926`). The earlier explanation "umu sets `PROTON_DLL_COPY=*`" is UNVERIFIED: the umu 1.4.4 zipapp contains no `DLL_COPY` string.
Inside the prefix Proton sets `WINEDLLOVERRIDES=...d3d11=n;d3d10core=n;d3d9=n;dxgi=n;...`, `WINE_LARGE_ADDRESS_AWARE=1`, `WINEDEBUG=-all`, `WINEFSYNC=1` (GE-Proton10-34 `proton:1221-1238,1254` DXVK `d3d9/d3d10core/d3d11(+dxgi)=n`, wined3d list when `PROTON_USE_WINED3D`; `:2037`, `:1874`, `:1940`).

### DXVK decision

Which graphics API each in-game renderer really uses (imports via `objdump -p`, renderer loading via
`rgl` + device name in `reference/hw_rgl/kgl.c:2810-2816`):

| In-game renderer | DLL chain | Wine path | DXVK |
|---|---|---|---|
| Software | `rglsw.dll` (DDRAW, GDI32) | ddraw blit / GDI (`/noswddraw` = GDI, HW1 `main.c:685`; GOG passes it) | n/a |
| Direct3D, **stock** | `rgld3d.dll` 118,784 B -> DDRAW (DirectDraw/D3D7) | Wine ddraw -> wined3d | never (Proton has no D3D7 DXVK) |
| Direct3D, **CET** (hw_rgl) | `rgld3d.dll` 380,928 B -> `d3d9.dll` (`Direct3DCreate9`, `hw_rgl/D3D9/d3driver.cpp:2293`) | d3d9 | **yes (default)** |
| 3dfx Glide | `rglfx3.dll` -> `glide2x.dll` (GOG-bundled nGlide 1.04) -> `d3d9.dll` | d3d9 | **yes (default)** |
| OpenGL | system `opengl32.dll` (CET selects it on first run, `PatchRGL.cpp:13,53`) | Wine opengl32 -> NVIDIA GL | n/a |
| 3dfx MiniGL | `3Dfx/WinNT/opengl32.dll` -> `glide2x.dll` -> d3d9 | d3d9 | yes (untested) |

Default: **DXVK on** (Proton default; it only touches the D3D9 paths: CET's renderer and nGlide).
Flip per run: `tools/run_game.sh --no-dxvk` = `PROTON_USE_WINED3D=1` (proton `check_environment("PROTON_USE_WINED3D","wined3d")`, ~l.1882) -> d3d9/10/11/dxgi via wined3d. d3d8 stays wined3d unless `PROTON_DXVK_D3D8=1` (the game does not use d3d8). wined3d's own backend is GL; `WINE_D3D_CONFIG=renderer=vulkan` (UNVERIFIED for this game) can be exported before `run_game.sh` because umu passes the environment through.

## 3. `tools/install_cet.sh` - Cataclysm Engine Tweaks

```
tools/install_cet.sh [--download-only] [--tag vX] [--zip PATH] [--no-download] [--game-dir DIR]
                     [--keep-ini] [--restore [DIR]] [--dry-run]
```

1. `gh release download [TAG] -R ncblakely/CataclysmEngineTweaks -p 'cet_*.zip' -D original/cet --clobber`
   (curl + GitHub API fallback), writes `RELEASE-<tag>.json` and `<zip>.sha256`.
2. Backs up every game file the zip overwrites (case-insensitive match, Wine is case-insensitive) and always
   `rgl.dll` + `rgld3d.dll` (CET README "Installation" step 2) to `<game>/.cet-backup/<ts>/` with
   `BACKUP_MANIFEST.sha256`, `CET_ADDED.txt`, `CET_ZIP.txt`.
3. `unzip -o` into the game dir, then re-hashes every entry against the zip.
4. `--restore` puts back the **oldest** backup (= stock files) and deletes `CET_ADDED.txt` files; `--keep-ini` preserves an edited `CataclysmEngineTweaks.ini` on upgrade.
5. Idempotent: if every zip entry is already in the game dir with the same sha256, it logs `already installed ... nothing to do (no new backup taken)` and exits 0 (tested on the real `original/game/` and on a scratch game dir: install -> re-install no-op -> `--restore` -> stock hashes `sha256sum -c` OK). An **edited `CataclysmEngineTweaks.ini` still counts as installed** (verifier fix 14:14: before it, a re-run - checklist step 3 - overwrote the user's ini edits with the defaults); the run then logs `note: CataclysmEngineTweaks.ini differs from the zip (your edits are kept)`. If any other file differs, the full reinstall resets the ini unless `--keep-ini` (the edited copy lands in the new `.cet-backup/<ts>/`).
6. `--restore` removes only `CET_ADDED.txt` entries; `CataclysmEngineTweaks.log` and the game-written `Cataclysm.cfg keys.cfg AutoLock.txt` stay, so `--verify` afterwards still lists them as `EXTRA`.

Download run (`logs/install_cet-20261008-094330.log`): `cet_0.18.03.zip`, 2,122,976 B, sha256 `3c13c9621f447546f0268c0e8fbec703e25f6a0414cfb690ba279908576dc30f`. Contents:

| file | bytes | zip date | sha256 (installed) | role |
|---|---|---|---|---|
| `cataclysm_loader.exe` | 216,064 | 2024-08-22 | `3b458ae2...d123da393f` | console exe; starts `cataclysm.exe` suspended, injects the DLL, resumes |
| `CataclysmEngineTweaks.dll` | 5,322,752 | 2024-08-22 | `51320d2b...ad1aeb4b2512f7` | the hooks (MinHook); imports only DSOUND/KERNEL32/USER32 |
| `CataclysmEngineTweaks.ini` | 2,608 | 2024-07-24 | `cbd54ac8...c1cbb1aab13f900d85` | settings |
| `rgl.dll` | 228,352 | 2024-07-25 | `93947184...e3bd73beff739703` | hw_rgl front end (replaces stock 176,128 B) |
| `rgld3d.dll` | 380,928 | 2024-07-25 | `44117b58...6ca6fa045aa105412beb2e` | hw_rgl **Direct3D 9** device (replaces stock D3D7 118,784 B) |
| `update102.big` | 272,710 | 2024-07-25 | `542e39c4...62928e14d68` | CET's own update bigfile: CET patches the exe's three `push "<update bigfile>"` sites (0x005ABB21/0x005ABB3E/0x005ABB5C, `Cataclysm.h:119-121`) to open `Update102.big` **instead of** stock `update.big` (`GameHooks.cpp:12,108-116,129`; shipped by `Compress-Release.ps1:4-11`). So `--original` (no DLL) reads stock `update.big` again. ("v0.18.01 release notes" provenance: UNVERIFIED, only the v0.18.03 notes are on disk in `original/cet/RELEASE-v0.18.03.json`) |

Stock files that get backed up: `rgl.dll` `06082ec8...` and `rgld3d.dll` `b67fad3a...` (see `MANIFEST.sha256`).
Scratch test: install -> `extract_installer.sh --verify` showed exactly `rgl.dll`/`rgld3d.dll` changed + 4 CET extras -> `--restore` -> verify clean.

What CET does when launched (source in `reference/CataclysmEngineTweaks`):

| Fact | Where |
|---|---|
| loader requires `CataclysmEngineTweaks.dll` and `cataclysm.exe` **in the current directory** (run_game.sh `cd`s there; verified the Windows cwd = game dir with `cmd /c cd`) | `loader/Main.cpp:9-26` |
| kills running `cataclysm.exe`, builds `cataclysm.exe <args>` (args pass through), `CreateProcess(..., CREATE_SUSPENDED)`, `CreateRemoteThread(LoadLibraryA)`, `ResumeThread` | `loader/Main.cpp:28-38, 91, 96-118, 66` |
| DLL aborts unless the exe is mapped at 0x400000 | `dllmain.cpp:11-14` |
| logs to `CataclysmEngineTweaks.log` in the cwd; the 5 lines of a good start, in order: "RGL patches applied", "Update rate patches applied" (only when `UpdateRateShift != 0`, so its presence = non-stock universe rate), "Patches applied", "DSound init OK", "Universe reset called" | `dllmain.cpp:7`, `PatchRGL.cpp:110`, `PatchUniverse.cpp:428-431,457`, `Main.cpp:42-48`, `game/Win32/soundlow.cpp:87`, `PatchUniverse.cpp:45` |
| hw_rgl D3D9 logs to `rgld3d9.log` | `hw_rgl/D3D9/d3driver.cpp:89` |
| `BorderlessWindow=true` -> `fullScreen=FALSE, showBorder=FALSE` | `CataclysmEngineTweaks.ini:30`, `Main.cpp:21-25` |
| `NewRendererSelection=true`: if the saved device CRC differs, switch to **OpenGL at the "primary monitor" size** read from `GetClientRect(GetDesktopWindow())` | `PatchRGL.cpp:18-27, 40-54`, ini:27 |
| `UpdateRateShift=2` -> 64 Hz universe (stock 16 Hz = 0; `UpdateRate = 16 << shift`, clamped 0..3) | ini:11, `Config.cpp:5,15-16` |
| `DisableIntroMovies=true`, `HighDetailMode=true` | ini:33, ini:22 |
| loader needs the MSVC 2015-2022 x86 runtime (imports `MSVCP140`, `VCRUNTIME140`, `api-ms-win-crt-*`); the prefix has Wine builtins for these | `objdump -p cataclysm_loader.exe` |

## 4. `tools/run_game.sh` - launch

```
tools/run_game.sh [--cet|--original|--exe PATH] [--size WxH] [--window WxH] [--scaler S] [--filter F]
                  [--fps N] [--grab-cursor] [--no-gamescope] [--virtual-desktop WxH] [--no-dxvk]
                  [--wine-log] [--proton NAME] [--prefix DIR] [--direct-proton] [--game-dir DIR]
                  [--no-default-args] [--sync-res|--sync-res-only] [--dry-run] [--kill] [-- game args]
```

Default command (from `--dry-run`):

```bash
cd original/game && env SDL_VIDEODRIVER=x11 HWC_STATE=logs/run_game.state \
  gamescope --prefer-vk-device 10de:2b85 --backend sdl -w 1920 -h 1080 -W 1920 -H 1080 -- \
  bash -c '<write GAMESCOPE_WAYLAND_DISPLAY/DISPLAY to state>; "$@"; <write INNER_EXIT>' hwc-inner \
  env WINEPREFIX=... GAMEID=umu-1766413869 STORE=gog PROTONPATH=.../GE-Proton10-34 PROTON_VERB=waitforexitandrun \
  umu-run .../original/game/cataclysm_loader.exe /noswddraw
```

| Choice | Reason |
|---|---|
| nested gamescope, **windowed** (no `-f`), `-w/-h = -W/-H` = 1920x1080 | one isolated X screen: the game cannot see or resize to the 7680x2160 Xwayland root (`xwininfo -root`) spanning both monitors; CET's first-run auto mode reads the desktop size (`PatchRGL.cpp:18-27`) and therefore picks exactly 1920x1080 |
| `SDL_VIDEODRIVER=x11` + `--backend sdl` | COSMIC 150 % fractional scale crashes gamescope's Wayland path (user's standing note); the SDL/X11 path also restores refresh on refocus |
| `--prefer-vk-device 10de:2b85` | otherwise gamescope can pick the AMD iGPU (user's standing note); log shows `selecting physical device 'NVIDIA GeForce RTX 5090'` |
| `/noswddraw` | GOG's own play task: `goggame-1766413869.info` -> `"path": "cataclysm.exe", "arguments": "/noswddraw"` |
| `/window` added with `--no-gamescope` (not with `--virtual-desktop`) | stock HWC defaults to exclusive fullscreen (`HomeworldSDL/src/SDL/main.c:690-691`); inside gamescope that stays inside the window. Under CET it is redundant (`BorderlessWindow=true` already sets `fullScreen=FALSE`, `Main.cpp:21-25`); it matters for `--original` |
| **user ruling: gamescope is a test harness only** | `docs/CONSTRAINTS.md` rule 1: the user saw frame-rate drift after ~1 h under gamescope, so **long play-tests of the GOG build use `--no-gamescope --virtual-desktop WxH`**; the gamescope default is for short runs + screenshots. With a virtual desktop, CET's first-run size comes from `GetClientRect(GetDesktopWindow())` (`PatchRGL.cpp:18-27`), presumably the virtual-desktop size (UNVERIFIED), and COSMIC may resize the desktop window (test table below) |
| `cd` into the game dir | CET loader checks the cwd (`loader/Main.cpp:13-23`) |
| `Cataclysm.cfg` seeded from `__support/app/` if absent | what GOG's installer leaves in the root (`language=0`); observed in the 13:54 run log (`seeded .../Cataclysm.cfg`, `logs/run_game-20261008-135450.log:2`) |
| report the **saved video mode** before every game launch | read as text from `prefix/system.reg`, no Wine started; WARNING when it differs from `--size` |
| `--sync-res` / `--sync-res-only` | write `--size` into the game's saved mode first (below) |

Outputs: `logs/run_game-<ts>.log` (every line timestamped as it arrives; script header shows the exact command),
symlink `logs/run_game-latest.log`, at exit the tails of `CataclysmEngineTweaks.log` / `rgld3d9.log` if
written during the run. `logs/run_game.state` while running: `RUN_PID, STARTED, MODE, USE_GAMESCOPE, GAME_DIR, EXE, LOG, SIZE, PREFIX, PROTON_DIR, GAMESCOPE_PID, CHILD_PID, GAMESCOPE_WAYLAND_DISPLAY
(gamescope-0), GAMESCOPE_DISPLAY (:2), INNER_PID, INNER_EXIT` (`run_game.sh:333-363`, inner shell `:310`). Exit status = the game side's status; gamescope
3.14.24 itself always aborts on teardown (`terminate called without an active exception`, status 134) - logged, harmless.
Ctrl-C (or SIGTERM) -> `wineserver -k` then gamescope TERM. `--kill` from another terminal does the same; since the verifier fix (14:14) the state file also records `PREFIX=` and `PROTON_DIR=`, and `--kill` uses them unless `--prefix` is given again (before, a run started with `--prefix X` needed `--kill --prefix X`, else `wineserver -k` hit the default prefix and only the gamescope TERM stopped the game). Tested with a fake state file pointing at a scratch prefix: `wineserver -k in <scratch>`, stale state removed.

Saved video mode. The GOG game (Relic's Win32 code; HomeworldSDL moved these settings into its `.cfg`,
`HomeworldSDL/src/SDL/utility.c:644-660`) stores them in the registry key `HKLM\SOFTWARE\Sierra On-Line\Cataclysm`
(32-bit view, i.e. `[Software\\Wow6432Node\\Sierra On-Line\\Cataclysm]` in `prefix/system.reg`). Values seen after
the first run: `CmdLine="/noswddraw "`, `d3dToSelect=""`, `deviceToSelect=""`, `glToSelect="opengl32.dll"`,
`deviceCRC=0x329d5947`, `deviceIndex=0xffffffff`, `deviceCaps=deviceCaps2=0x00f9ff30`, `screenWidth=0x780`,
`screenHeight=0x438`, `screenDepth=0x20`, `HW_Language=""`, `"D657E436967616D4"=dword:0` (`prefix/system.reg:91053-91067`). All **13** value names (the list above, `deviceCaps` and `deviceCaps2` being two values; second pass corrected "12") and the key path `SOFTWARE\Sierra On-Line\Cataclysm` are plain strings in `cataclysm.exe`'s `.data` (file offset = RVA there; `.data` raw 0x34a000 = VA 0x34a000): `CmdLine` 0x008B9BD0 directly followed by the key path 0x008B9BD8; the other 12 names in one block 0x008C2D94 (`D657E436967616D4`) ... 0x008C2E28 (`deviceCRC`), e.g. `screenWidth` 0x008C2DD0, `glToSelect` 0x008C2DE8 (`strings -t x` + section table, second pass). `D657E436967616D4` is HW1's `REG_MAGIC_STR` "used for CD-checking code" (`HomeworldSDL/src/SDL/utility.c:146`, commented out there; read backwards it is hex for `MagicNum`); `CmdLine` is HW1's `RegisterCommandLine()` (`HomeworldSDL/src/SDL/main.c:291`, called at `:2155`, `#if 0` in the SDL port), so Cataclysm still runs that Win32 registry code.
`--sync-res` writes `screenWidth/screenHeight/screenDepth=32` with `reg import` of a temporary
`C:\hwc-video-mode.reg` (merge: other values stay; ~3 s; then polls `system.reg` until the values show,
else exits with an error). Tested on a scratch prefix: 1280x960 -> 1600x900 -> 1920x1080 with a
pre-existing `glToSelect` preserved. Caveats: CET's `NewRendererSelection` still overrides the size on a
start where `deviceCRC` mismatches (`PatchRGL.cpp:40-54`), and a mode picked in Options is overwritten on
the next `--sync-res` launch.

Loader lifetime. `cataclysm_loader.exe` returns right after `ResumeThread` (`loader/Main.cpp:118-120`).
Proton's `umu.exe` keeps waiting until `cataclysm.exe` exits (observed in the live process tree:
`gamescope -> bash hwc-inner -> umu-run -> proton waitforexitandrun -> umu.exe`, with `cataclysm.exe /noswddraw` running and no loader).
So gamescope is not torn down when the loader exits.

Exit status: the `INNER_EXIT` written by the inner shell; if gamescope dies first (killed / crashed), no
`INNER_EXIT` is recorded and gamescope's 134 is reported with an explicit log line. The 13:54 run ended this
way (`terminate called without an active exception`, `gamescopereaper ... Killing children`, a harmless
umu-run `ProcessLookupError` traceback; `logs/run_game-20261008-135450.log:164-195`, final `exit status: 134`). `--kill` instead ends the game side first: `Primary child shut
down!`, then gamescope's teardown abort, `exit status: 0` (tested 14:02 with notepad).

Live tests:

| Test | Result |
|---|---|
| `--exe notepad --size 800x600` (gamescope) | window inside gamescope `:2`, `WM_CLASS steam_app_1766413869`; host window class `gamescope`, title = inner window title; `--kill` cleaned up, nothing left running |
| `--exe nglide_config.exe --size 1024x768` (32-bit, unix path -> `Proton: Executable a unix path, launching with /unix option`) | dialog visible and captured (gamescope upscales a small window to fill), `logs/run_game-20261008-094931.log` |
| `--no-gamescope --exe notepad` | Xwayland window `steam_app_1766413869`, but COSMIC resized it to 2045x1329 (`logs/screenshot-20261008.log:46-51`); a Wine virtual desktop requested at 640x360 came up 2559x1329 (no log kept: UNVERIFIED) -> on this desktop, window sizes are not under the game's control without gamescope |
| `--no-gamescope --exe cmd -- /c cd` | prints `Z:\...\game-test` = Windows cwd is the game dir |
| SIGINT to run_game.sh | `caught SIGINT`, wineserver killed, gamescope gone, `exit status: 0` |
| **the game** (`run_game.sh --size 1920x1080`, CET, by a parallel agent 13:54:50) | CET log complete 7 s after start (13:54:57), main menu captured 13:55:46; see Status |
| `--prefix <scratch> --exe notepad --size 800x600`, then `--kill` (14:01-14:02) | window after 4 s; host window `class=gamescope title='Untitled - Notepad'`; `--kill`: host `wineserver -k` reaches the wineserver inside the sniper container, nothing left running, state file removed |
| `--prefix <scratch> --size 1600x900 --sync-res-only` | `video mode stored: 1600x900x32` in 2.7 s |

Window size rules: the game's saved video mode must equal `--size` (else gamescope letter-/pillarboxes or
upscales; the user's SPAZ note says a mismatch also caused stutter there). Use `--sync-res`, change the
mode in-game, or change `--size` to what the game saved. For a bigger window at low game
res: `--size 1280x960 --window 2560x1920 --scaler integer`. Without gamescope the mismatch WARNING is not printed
(`run_game.sh:265` checks it only with gamescope) and `--sync-res` writes `--size`, not the `--virtual-desktop` size: pass both
with the same value (e.g. `--no-gamescope --virtual-desktop 1600x900 --size 1600x900 --sync-res`). gamescope makes the window 1 px wider for some
sizes (observed 641x360 for 640x360 - evidence gone, UNVERIFIED; 1025x768 for 1024x768 - `logs/screenshot-20261008.log:63`, `logs/run_game-20261008-094931.log:146`).

## 5. `tools/screenshot.sh` - capture without input injection

```
tools/screenshot.sh [--auto|--gamescope|--x11|--desktop] [--out FILE] [--match REGEX]
                    [--window-id ID] [--display :N] [--socket gamescope-N] [--list]
```

| Method | How | Tested result |
|---|---|---|
| **gamescope** (auto default while `run_game.sh` runs) | `GAMESCOPE_WAYLAND_DISPLAY=gamescope-N gamescopectl screenshot <png>` (gamescope 3.14.24 ConCommand `screenshot`, source tree `~/build/gamescope` (tag 3.14.24) `src/steamcompmgr.cpp:971-990`; default type `GAMESCOPE_CONTROL_SCREENSHOT_TYPE_BASE_PLANE_ONLY`; PNG written by a worker thread, script polls until size is stable) | vkcube 641x360, notepad 800x600 and **the game's main menu (1920x1080)** captured, exact composited frame. Only fires when gamescope composites a frame |
| **x11** | python3-xlib `GetImage` + PIL on a viewable window, clipped to the root; default `--match` is `steam_app_1766413869\|^emergence( \|$)` on `<title> <class>` (the game's window title is exactly `Emergence`: `logs/screenshot-20261008-firstrun.log:4,6`), or `--window-id`. Verifier fix 14:14: the old default also had bare `cataclysm\|homeworld`, which match any X11 editor/terminal titled `...Homeworld-Infection...` on `:1` (largest match wins) and would have captured the user's desktop; `--display :2` targets gamescope's inner Xwayland; auto also falls back to the host `gamescope` window | NVIDIA Vulkan (vkcube 320x240) and Wine windows on `:1` and `:2` captured with real content. **Not for the game in gamescope**: its X11 window on `:2` reported 640x480 while it rendered 1920x1080, so the `:2` capture came back 640x480 and squeezed; use the gamescope method (auto does) |
| **desktop** | `cosmic-screenshot --interactive=false --notify=false --save-dir <tmp>` | 5120x1440 PNG = both outputs in **logical** pixels (4K @150 % scaled down); re-tested 14:02 (then deleted: it showed the user's private desktop). The interactive portal is broken at 150 % scale (user's note); non-interactive works |
| grim 1.4.0 (apt-installed for this test) | - | **fails**: `compositor doesn't support wlr-screencopy-unstable-v1` |
| gamescope Super+S hotkey | would need a key press | not used (no input injection) |

Output: `logs/screenshots/<mode>-<YYYYmmdd-HHMMSS-mmm>.png` (milliseconds, so two captures in one second do not overwrite each other), a stats line (`WxH mean= stddev=`, WARNING if near-black), path on the last line:
`png=$(tools/screenshot.sh | tail -n1)`. `--list` shows windows on `:1` and on the gamescope display.

## 6. Verification checklist (any run; steps 1-6 passed on 2026-10-08)

1. `tools/extract_installer.sh --verify` -> before CET: "all manifest files present and unchanged"; after CET and a
   run: exactly `rgl.dll`/`rgld3d.dll` FAILED + EXTRA `cataclysm_loader.exe CataclysmEngineTweaks.{dll,ini,log}
   update102.big Cataclysm.cfg keys.cfg AutoLock.txt` (exit 1 is then expected).
2. `tools/setup_prefix.sh --check` -> `OK: Microsoft Windows 10.0.19045`, `d3d9.dll DXVK (Vulkan)`. (Blocks while the
   game runs in the same prefix: `waitforexitandrun` waits for its wineserver.)
3. `tools/install_cet.sh --no-download` -> six `ok` lines, or `already installed ... nothing to do`.
4. `tools/run_game.sh --dry-run` -> no `WARNING missing`; `saved mode: 1920x1080, glToSelect=opengl32.dll` and no
   mismatch WARNING.
5. `tools/run_game.sh` -> host window `class=gamescope title='Emergence'` (check with `tools/screenshot.sh --list`); log
   contains `Proton: Executable a unix path`; `CataclysmEngineTweaks.log` gains `Patches applied` / `RGL patches
   applied` / `DSound init OK`; intro movies are skipped (CET `DisableIntroMovies=true`).
6. `tools/screenshot.sh` -> non-black 1920x1080 PNG of the front end (menu centred at 640x480 size).
7. Still to do: select the D3D9 renderer in Options -> Video (DXVK path), play a mission, check audio/speech, then
   `--no-dxvk` for comparison; a long play-test the way the user ruled (`docs/CONSTRAINTS.md` rule 1):
   `tools/run_game.sh --no-gamescope --virtual-desktop 1920x1080` (never tried with the game yet; stop the Qwen timer first, section 7).

## 7. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `cataclysm_loader.exe`: "CataclysmEngineTweaks.dll not found in the current directory" | not started from the game dir; use run_game.sh (it `cd`s) - `loader/Main.cpp:13-23` |
| `CataclysmEngineTweaks.log`: "unexpected image base" | exe not mapped at 0x400000 (`dllmain.cpp:11-14`). Stock `cataclysm.exe` has no `DYNAMIC_BASE`, so Wine maps it at 0x400000. **Never set `DYNAMIC_BASE` when patching the exe**: GE-Proton10-34 then maps relocs-stripped images elsewhere (probe: a copy of `nglide_config.exe` with the bit set loaded at 0x7BF90000 and faulted) |
| black / garbled screen after picking a video mode | wait or press Space (game reverts); Ctrl-Shift-F12 = default renderer (software 640x480 16 bit) (`video.txt`); or launch with `-- /sw` (reset renderer, HW1 `main.c:687`); or try `--no-dxvk` |
| OpenGL artefacts (backgrounds, missing stars) | `-- /safeGL` (`video.txt`, HW1 `main.c:682`) or CET `NoCompiledVertexArrays=true` |
| game uses a huge/odd resolution with `--no-gamescope` | CET reads the Xwayland desktop (7680x2160 root here); use gamescope (default) or `--virtual-desktop WxH` (COSMIC may still resize it) |
| cropped / letterboxed image, or `WARNING: saved mode AxB != --size` | game's saved mode != `--size`: add `--sync-res` (section 4) |
| `setup_prefix.sh --check` / `--sync-res` / a launch just waits | another program (usually the game) runs in the same prefix and `waitforexitandrun` waits for its wineserver; stop it (`tools/run_game.sh --kill`) |
| `no INNER_EXIT recorded` + `exit status: 134` | gamescope went down before the game side finished (killed or crashed); check the log above it for the cause |
| tiny menu in the middle of a big window | normal: HW1-lineage front-end screens are 640x480 layouts centred in higher modes (`HomeworldSDL/src/Game/FEFlow.c:1546-1561`); in-game view uses the full resolution |
| x11 capture of the game is 640x480 / squeezed | use the gamescope method (`tools/screenshot.sh` auto does); see section 5 |
| gamescope aborts at start (`terminate called without an active exception`) | check `SDL_VIDEODRIVER=x11` and `--prefer-vk-device` are in the logged command; check `journalctl -k \| grep Xid` (user's NVIDIA notes) |
| `gamescope exit status: 134` in the log | gamescope 3.14.24 teardown abort; harmless; the real game status is the final `exit status:` line |
| mouse leaves the window while edge-scrolling | `--grab-cursor` |
| no / distorted sound | CET `ForceWaveOut=true` or `DisableEqualizer=true` in `CataclysmEngineTweaks.ini` |
| need Wine debug output | `--wine-log` -> `logs/steam-1766413869.log` (+ DXVK logs); umu's own `UMU_LOG=1` dumps the whole environment into the log - do not share such logs |
| `WARNING: Executable not found: cmd/notepad` from umu | benign for Wine builtins |
| innoextract format warnings | `tools/extract_installer.sh --build-innoextract`, then rerun |
| first umu run hangs ~40 s | downloading the sniper runtime (803 MB) |
| game opens on the wrong monitor (no gamescope) | Xwayland primary is the HP; user's fix `cosmic-randr xwayland --primary DP-4` |
| GPU/VRAM busy while the game runs (stutter) | the local Qwen worker (`hwi-qwen-worker.timer`, `cycle/STATE.md`) runs Ollama `qwen3.6:27b` (~28 GB VRAM incl. desktop). It yields when `pgrep` sees a `.exe`/`gamescope` process (`tools/qwen_worker.py:115-127`, checked before a batch and every 10 functions, `:240-243,255-262`), but each request sets `keep_alive: "5m"` (`:215`), so the model can stay resident up to 5 min after it yields. Before a long play-test: `systemctl --user stop hwi-qwen-worker.timer` (and `ollama stop qwen3.6:27b` to free VRAM at once) |
| "no setup_homeworld_emergence*.exe" / "no game directory" although the files exist | `original/` or `original/game` is a symlink and an old copy of the scripts is in use (plain `find` does not enter a symlinked start dir); current scripts use `find -H` |
| multiplayer | WON/Titan lobby code is dead (`docs/binary-fingerprint.md`); out of scope |

## 8. Windows 11 side

1. Install from the same offline installer (`setup_homeworld_emergence_1.01_hotfix_(12948).exe`) or Galaxy. Default GOG folder is typically `C:\GOG Games\Homeworld Emergence` (UNVERIFIED for this build).
2. Install **Microsoft Visual C++ 2015-2022 Redistributable (x86)** if missing - `cataclysm_loader.exe` imports `MSVCP140.dll`/`VCRUNTIME140.dll`.
3. In the game folder back up `rgl.dll` and `rgld3d.dll`, then extract `cet_0.18.03.zip` over it (CET README "Installation"):

   ```powershell
   $g = 'C:\GOG Games\Homeworld Emergence'
   New-Item -ItemType Directory "$g\.cet-backup" -Force | Out-Null
   Copy-Item "$g\rgl.dll","$g\rgld3d.dll" "$g\.cet-backup\"
   Expand-Archive -Force "$env:USERPROFILE\Downloads\cet_0.18.03.zip" $g
   ```
4. Start `cataclysm_loader.exe` (not `cataclysm.exe`) with **Start in = the game folder**; to mirror GOG add `/noswddraw` (the loader forwards arguments, `loader/Main.cpp:28-38`). A desktop shortcut: Target `"<game>\cataclysm_loader.exe" /noswddraw`, Start in `"<game>"`.
5. Settings live in `CataclysmEngineTweaks.ini` (table in section 3). Choose the D3D9 renderer in Options -> Video if OpenGL misbehaves. Logs: `CataclysmEngineTweaks.log`, `rgld3d9.log` in the game folder.
6. Undo: copy the two DLLs back from `.cet-backup\` and delete `cataclysm_loader.exe`, `CataclysmEngineTweaks.*`, `update102.big`.
7. The loader injects a DLL with `CreateRemoteThread`; some antivirus products may flag it (UNVERIFIED).

## 9. Facts for the decompilation / port

- GOG build 12948 "1.01 hotfix": `cataclysm.exe` sha256 `f03d9a3e...6eef3ed4`, MSVC 6, 2000-12-21, base 0x400000, relocations stripped, no ASLR flag - CET's hard-coded addresses (e.g. `PatchRGL.cpp:29` `0x00563645`, `Cataclysm.h:202-204` globals at `0x008B86xx`) assume this exact image.
- Renderer plug-ins are runtime-loaded `rgl<device>.dll` (`hw_rgl/kgl.c:2810-2816`); CET's `rgl.dll`/`rgld3d.dll` are rebuilt from HW1 source (`hw_rgl`, D3D7 and D3D9 projects both target `rgld3d`: `D3D7/rgld3d7.vcxproj:111`, `D3D9/rgld3d9.vcxproj:108`).
- GOG ships nGlide 1.04 (`glide*.dll`, `3DfxSpl*.dll`, `nglide_config.exe`) so the 3dfx renderer runs on D3D9; a newer nGlide 1.05 `glide2x.dll` sits unused in `__support/add/` (section 1).
- Stock renderer DLLs carry **Intel C/C++ Compiler 4.5 (00015)** build stamps with Relic's source paths `C:\Cataclysm\src\rgl\<file>` and `-Qvc6 -G6 -O2 -MTd -W2 -Qipo -D HW_Release` (`strings` on the files; `cataclysm.exe` itself has none). Per-DLL source sets, i.e. what each one decompiles back to:

  | DLL (stock) | source files in the stamps | public source |
  |---|---|---|
  | `rgl.dll` 176,128 B (now in `.cet-backup/`) | `asm.c clip.c hash.c invert.c kgl.c kvb.c maths.c rglext.c wgl.c` | same 9 names at the `reference/hw_rgl/` root |
  | `rgld3d.dll` 118,784 B (now in `.cet-backup/`) | `d3dblt.cpp d3denum.cpp d3dinit.cpp d3driver.cpp d3dtex.cpp` | `reference/hw_rgl/D3D7/` (same 5 names) |
  | `rglsw.dll` 798,720 B | `asm.c draw.cpp lines.c newblendtextri.c newpersptri.c newrgbapersptri.c newrgbatextri.c newtextri.c newtriangle.c scan.c span.c swdriver.c` | **none** in any reference clone (`find -iname swdriver.c` empty) |
  | `rglfx3.dll` 94,208 B | `fxclose.c fxdriver.c fxtmg.c` | **none** in any reference clone |

- The game window class under Proton is `steam_app_1766413869`, title `Emergence`; the host gamescope window takes the game's title with class `gamescope`.
- Video/renderer settings live in `HKLM\SOFTWARE\Sierra On-Line\Cataclysm` (values in section 4), not in the game's `Cataclysm.cfg` (second pass: the game-written file has no `screen*`/`device*` keys). HomeworldSDL instead moved them into its own `Homeworld.cfg` (`UTY_CONFIG_FILENAME`, `utility.c:150`; option table `utility.c:644-660`); HW1 used `HKLM\SOFTWARE\Sierra On-Line\Homeworld` (`HomeworldSDL/src/SDL/regkey.h:29`). CET's `glToSelect` global is at `0x008B8644` (`Cataclysm.h:227`). The game writes `Cataclysm.cfg` (94 lines, options: `effectsLevel` ... `DifficultySetting`, HW1-style `name    value` lines), `keys.cfg` (byte-identical copy of the shipped `default_keys.cfg`, 6,150 B) and an empty `AutoLock.txt` into the game folder on first start (mtimes 13:54:57.36-.84).
- Video-mode / renderer-selection globals CET touches (`reference/CataclysmEngineTweaks/Cataclysm.h`): `MAIN_WindowWidth/Height/Depth` 0x008B85E8/0x008B85EC/0x008B85F0 (:228-230) = registry `screenWidth/screenHeight/screenDepth`; `opDeviceCRC` 0x00A4CC20 (:222) = `deviceCRC`; `glToSelect` 0x008B8644 (:227); `rinDeviceCRC()` 0x0057DCF0 (:155); `rinSortModes` 0x0057DE00, `rinEnumDisplayModes_cb` 0x0057E5C0, `rinEnumPrimaryDisplayModes_cb` 0x0057E710 (:156-158); `mainRescaleMainWindow()` 0x0055FC70 (:146); hook site `InitWindow_DeviceCrcCheck` 0x00563563 (:96) resuming at 0x00563645 (`PatchRGL.cpp:29`); `ActivateMe_ReInitRenderer` 0x00562084 (:98); `glCapNT` 0x00559AC0 / `glCapStartup` 0x00559BE0 (:143-144); `rndAspectRatio` 0x00B377A4 (:231). Command-line globals CET overwrites at startup: `showBorder` 0x008B8628, `fullScreen` 0x008B8638, `enableAVI` 0x008B8610 (:202-204; `Main.cpp:21-30`). The stock render-task frame limiter is at 0x0057B78B / 0x0057B7A1 (CET `DisableFramerateLimit`, default false, NOPs 8 bytes at each, `PatchRGL.cpp:96-100`) - keep it for a 1:1 port. CET hooks `soundStartDSound` 0x005808E0 (:170) unless `ForceWaveOut=true` (`PatchSound.cpp:14-16`).
- Stock `cataclysm.exe` import table (`objdump -p`, second pass): `WINMM WSOCK32 KERNEL32 USER32 GDI32 comdlg32 ADVAPI32 SHELL32 binkw32 DDRAW ole32`. **No `DSOUND.dll` import and no `dsound` DLL-name string; no OpenGL import.** `DDRAW` supplies only `DirectDrawEnumerateA` + `DirectDrawCreate` (display-mode enumeration, cf. CET's `rinEnumDisplayModes_cb`), `ole32` only `CoCreateGuid`; renderers are loaded at run time by name (strings `rgl.dll`, `opengl32.dll`, `glide2x.dll`). The help text strings `/dsound`, `/dsoundCoop` exist. So the log line `DSound init OK` comes from CET's replacement `soundStartDSound` (`game/Win32/soundlow.cpp:24` `DirectSoundCreate` via CET's own DSOUND import, `:87`); how (or whether) the stock code reaches DirectSound is UNVERIFIED (needs disassembly of 0x005808E0).
- CET's `update102.big` redirect is confirmed in the real exe (byte check, second pass): 0x005ABB21, 0x005ABB3E and 0x005ABB5C each hold `68 38 0F 8C 00` = `push 0x008C0F38`, and 0x008C0F38 is the string `Update.big` (the shipped file is `update.big`, 244,405 B; Wine/Windows open it case-insensitively). Those are the `utyGameSystemsPreInit_UpdateBigFileName1-3` sites of `Cataclysm.h:119-121`.
- Every run so far used CET defaults, i.e. a **64 Hz** universe (`UpdateRateShift=2`, log line "Update rate patches applied"), CET's rebuilt `rgl.dll`, and `Update102.big` data. A behavioural reference for the 1:1 port (stock 16 Hz = HW1 `UNIVERSE_UPDATE_RATE_ORIG`, `HomeworldSDL/src/Game/Universe.h:44`; note HomeworldSDL itself defaults to shift 2 = 64 Hz, `Universe.h:45-48` + `Game/Universe.c:75`, and its **`/vanilla`** switch restores 16 Hz (`SDL/main.c:848`, applied at `:2166-2169`; the `/legacy` named in the `Universe.h:46` comment does not exist in either HW1 port - second pass corrected); stock data) needs `UpdateRateShift=0` in the ini, or better `tools/install_cet.sh --restore` + `tools/run_game.sh --original` (note `--original` is stock exclusive fullscreen inside gamescope, no CET borderless/renderer fixes).

## 10. Open items / UNVERIFIED

- Proven on 2026-10-08 (parallel agent's run): CET injection under Wine (`CreateRemoteThread` + `LoadLibraryA`), CET's first-run OpenGL switch at the gamescope size, the OpenGL client inside nested gamescope, DirectSound init (`DSound init OK`), front end drawing. Not yet proven: actual audio output, the D3D9 (`rgld3d.dll` -> DXVK) renderer, `--no-dxvk`, Bink movies (skipped by CET by default), in-mission performance, save/load.
- The game's X11 window on gamescope's `:2` was listed as 640x480 while the composited frame was 1920x1080 (cause not investigated; matters only for x11 captures).
- `__support/app/Cataclysm.cfg` copy is inferred from GOG's layout. Whether GOG's installer writes registry keys is unknown (innoextract cannot show Inno `[Registry]`/Pascal actions); the game started without any, and the `Sierra On-Line\Cataclysm` key was created by the game/CET on that first start. Unexamined: `__support/add/` (nGlide 1.05 `glide2x.dll`) and the `goggame-1766413869.script` install action that runs `__support/galaxy_homeworld_emergence_2.0.0.11.exe` as admin; a real GOG install on the Windows 11 side would show what they change (diff the install folder + `HKLM\SOFTWARE\WOW6432Node\Sierra On-Line`).
- Evidence for the 13:54 game run was copied from the session scratchpad into `logs/` (git-ignored, so it lives only on this machine).
- `--sync-res` real write tested on a scratch prefix only (not followed by a game start), and only via umu (not `--direct-proton`).
- `--wine-install` GUI mode not exercised (needs clicks); silent mode proven to abort.
- `--virtual-desktop`: Wine honoured the desktop but COSMIC resized the window (observed 2559x1329 for a 640x360 request).

## Verification (round 1)

Skeptical pass 2026-10-08 14:08-14:20. Method: re-read every cited source line (CET, hw_rgl, HomeworldSDL, GE-Proton10-34 `proton`, umu 1.4.4 zipapp, gamescope `~/build/gamescope` 3.14.24), re-hashed/`objdump -p`/`strings` the files in `original/`, read `prefix/system.reg`, the logs, and the screenshots; ran the scripts' safe paths (`bash -n`, `--help`, `run_game.sh --dry-run` incl. size mismatch / `--sync-res` / `--exe --sync-res-only`, `install_cet.sh --dry-run --no-download`, `extract_installer.sh --verify`) from a scratch copy of `tools/` with `original/` + `prefix/` symlinked, so `logs/` was not polluted. Nothing was launched; no game window.

**42 claims checked, 9 wrong or stale (fixed above), 3 script defects (fixed), 3 marked UNVERIFIED.**

Confirmed (sample): installer size/sha256/Inno 5.5.7/GOG id/6 languages/68 `app/` entries with `goggame-*.ico` + `webcache.zip` doubled; stub 504,904 B UPX, `GOG Galaxy - Game Installer.exe`, 2020-04-09; installer flags `0x818f`/`0x8140`; `cataclysm.exe` sha256/PE timestamp `0x3a4265aa`/base/entry/SizeOfImage/sections; 66 files / 404,999,701 B; CET zip + all 6 entry hashes; import lists (CET DLL, loader, stock vs CET `rgld3d.dll`, `rglfx3.dll`, `glide2x.dll`); every CET line ref in sections 3 and 9 (`loader/Main.cpp`, `dllmain.cpp`, `Main.cpp`, `PatchRGL.cpp`, `Config.cpp`, ini, `Cataclysm.h:202-204,227`); HW1 refs (`main.c:682-691`, `utility.c:644-660`, `regkey.h:29`, `FEFlow.c:1546-1561`); hw_rgl refs (`kgl.c:2810-2816`, `d3driver.cpp:89,2293`, both vcxproj `TargetName rgld3d`); proton `:1882`, `:2335-2347`, toolmanifest `1628350`; umu `GAMEID` -> `SteamAppId` (`umu_run.py:278-281`); no ProtonFixes entry; sniper 803 MB; registry key values; title `Emergence`; 1920x1080 main-menu frame; CET log lines; `video.txt:14,18`; GOG play task `/noswddraw`; `--dry-run` default command; mismatch WARNING text.

| # | Was | Now |
|---|---|---|
| 1 | re-extraction gave a "byte-identical manifest" | only `MANIFEST.sha256` is identical; the 09:30 `MANIFEST.sizes` has a stale `0  ./MANIFEST.sizes` self-entry |
| 2 | `--verify` after CET prints `CHANGED rgl.dll rgld3d.dll` | real output `./rgl.dll: FAILED` + `SIZE CHANGED: ./rgl.dll` (same for `rgld3d.dll`), exit 1 |
| 3 | `update102.big` = "1.02 data overlay (v0.18.01 release notes)" | CET redirects the exe's update bigfile to it (`GameHooks.cpp:12,108-116,129`, sites `Cataclysm.h:119-121`); provenance note UNVERIFIED |
| 4 | `D657E436967616D4` "unexplained"; `HW_Language` missing | HW1 `REG_MAGIC_STR` CD-check value (`utility.c:146`); `CmdLine` = HW1 `RegisterCommandLine` (`main.c:291`); all names are strings in `cataclysm.exe` |
| 5 | "umu sets `PROTON_DLL_COPY=*`" | not in umu; Proton `pfx_copy()` copies non-symlink prefix files (`proton:910-926`) |
| 6 | Proton Experimental "(11.0-100)" | `experimental-11.0-20261001`; 11.0-100 is `CURRENT_PREFIX_VERSION` |
| 7 | `src/steamcompmgr.cpp:968-990` (no tree named) | `~/build/gamescope/src/steamcompmgr.cpp:971-990` |
| 8 | `.gitignore` does not cover `logs/screenshots/*.png` | `logs/` is ignored (`.gitignore:14`, uncommitted); `docs/screenshots/*.png` are not |
| 9 | screenshot.sh "only reads" | it always writes a PNG + log; it just has no `--dry-run` |

Script fixes (all re-tested; `bash -n` + `--help` pass):

- `tools/install_cet.sh`: an edited `CataclysmEngineTweaks.ini` made the "already installed" check fail, so checklist step 3 (re-run) silently reset the user's ini to defaults. It now counts as installed and logs a note (scratch game dir: install -> edit `ForceWaveOut=true` -> re-run keeps it, no new backup).
- `tools/run_game.sh`: `--kill` always used the default prefix; a run started with `--prefix X` was only stopped through the gamescope TERM. The state file now records `PREFIX`/`PROTON_DIR` and `--kill` uses them (tested with a fake state file).
- `tools/screenshot.sh`: default x11 `--match` included bare `cataclysm|homeworld`, which also matches editor/terminal windows titled `...Homeworld-Infection...` (largest match wins: a privacy leak). It is now `steam_app_1766413869|^emergence( |$)`.

Added (grounded): `__support/add/` nGlide 1.05 vs root 1.04 and the `goggame-*.script` admin install action; CET log-line sources; Proton override lines; full state-file field list; section 9 renderer-DLL build stamps (Intel C/C++ 4.5, `C:\Cataclysm\src\rgl\*`; `rglsw.dll`/`rglfx3.dll` have **no** public source), the video-mode/renderer address map from `Cataclysm.h`, `keys.cfg` = `default_keys.cfg`, and the 64 Hz-vs-stock-16 Hz caution for reference captures. The 13:54 game-run logs were copied from the scratchpad into `logs/` so the evidence cited here survives the session.

Remaining issues:

- UNVERIFIED (evidence gone): innoextract git `6e9e34e` results, the `7BF20000`/`0x7BF90000` relocation addresses, "641x360"/"1025x768" gamescope widths (second pass: 1025x768 found in `logs/screenshot-20261008.log:63`), the loader-gone/`umu.exe`-waiting process tree, `--direct-proton` not relaying output.
- `--sync-res` is still unproven against a real game start (it writes the key; whether the game honours it before CET's CRC check is untested).
- `docs/screenshots/*.png` hold copyrighted game frames and are not git-ignored: the committer must decide. (Resolved before the second pass: `.gitignore:19` `docs/screenshots/`, committed in `5334201`.)
- `__support/add/` (nGlide 1.05) and the Galaxy install action are not applied by `extract_installer.sh`; impact on the 3dfx path is unknown.
- `tools/ghidra_headless.sh` is not covered by this runbook.


## Verification (round 2)

Method: re-read every cited CET / hw_rgl / HomeworldSDL / gardens-of-kadesh / GE-Proton `proton` / Proton Experimental / umu 1.4.4 zipapp / gamescope 3.14.24 line; `sha256sum`, `objdump -p`, `strings -t x` + section table, and 5-byte reads at the three CET push sites of `cataclysm.exe` (no disassembly); `innoextract --info/--list` for three languages; a **fresh re-extraction** of the installer into a scratch dir with `extract_installer.sh` (15.7 s) compared against `original/game/MANIFEST.*`; `prefix/system.reg`, `git show HEAD:.gitignore`, `git check-ignore -v`, all logs cited above, the 1920x1080 main-menu frame. Scripts run from a scratch copy of `tools/` with `original/` + `prefix/` symlinked (project `logs/` untouched): `bash -n` + `--help` (all 5), `run_game.sh --dry-run` (default, `--size 1280x960` mismatch WARNING, `--sync-res`, `--sync-res-only`, `--exe notepad --sync-res` refused, `--no-gamescope` adds `/window`, `--virtual-desktop`), `run_game.sh --kill` against a fake state file (prefix taken from the state), `install_cet.sh --dry-run --no-download` ("already installed") and `--dry-run --restore`, `extract_installer.sh --verify` (2 FAILED, 2 SIZE CHANGED, 8 EXTRA, exit 1), `--report`, `--info`, stub refusal by name and by VERSIONINFO (renamed copy; both exit 3). Nothing was launched in Wine; no game window.

**52 claims checked; 7 wrong or stale + 1 script defect, all fixed in place; 3 newly marked UNVERIFIED.** Confirmed (sample): installer 360,195,200 B / sha256 `9e39851e...ba335ee` / Inno 5.5.7 / 68 `app/` entries in english, german, chinese / installer PE flags `0x818f`+`0x8140`; stub VERSIONINFO + 2020-04-09; `cataclysm.exe` sha256 `f03d9a3e...6eef3ed4`, `0x3a4265aa`, base/entry/SizeOfImage/7 sections, no `DYNAMIC_BASE`; `MANIFEST.sha256` **byte-identical** on re-extraction (66 files, 404,999,701 B), `MANIFEST.sizes` differing only by the stale `0  ./MANIFEST.sizes` line; CET zip + 6 entry hashes + imports; every CET ref (`loader/Main.cpp:9-26,28-38,66,91,96-120`, `dllmain.cpp:7,11-14`, `PatchRGL.cpp:13,18-29,40-54,110`, `Config.cpp:5,15-16`, ini `:11,22,27,30,33`, `Main.cpp:21-25,42-48`, `PatchUniverse.cpp:45,428-431,457`, `soundlow.cpp:87`, `GameHooks.cpp:12,108-116,129`, `Compress-Release.ps1:4-11`, all `Cataclysm.h` addresses in section 9); hw_rgl `kgl.c:2810-2816`, `d3driver.cpp:89,2293`, both `TargetName rgld3d`; Intel C/C++ 4.5 stamps + per-DLL source sets, no public `swdriver.c`/`fxdriver.c`; HW1 `main.c:682-692,291,2155`, `utility.c:146,644-660`, `regkey.h:29`, `FEFlow.c:1546-1561`, `Universe.h:44`; `D657E436967616D4` reversed = hex `MagicNum`; proton `:45` (Experimental), `:910-926,1221-1238,1254,1874,1882,1940,2037,2335-2350`; umu `umu_run.py:278-281`, no `DLL_COPY`; sniper 803 MB / depot 3.0.20260928.262393; gamescope `steamcompmgr.cpp:971-990`; registry key lines `91053-91067`; CET log lines; `keys.cfg` = `default_keys.cfg`; `Cataclysm.cfg` 94 lines; prefix 729 MB, 624/627 copied DLLs, `pfx -> .`; title `Emergence` (`logs/screenshot-20261008-firstrun.log:4,6`).

| # | Was | Now |
|---|---|---|
| 1 | "All 12 value names" of the Cataclysm registry key | 13 (`deviceCaps` + `deviceCaps2`); string VAs added |
| 2 | HomeworldSDL "`/legacy` restores 16 Hz" | the switch is `/vanilla` (`SDL/main.c:848`, `:2166-2169`, both HW1 ports); `/legacy` exists only in the `Universe.h:46` comment |
| 3 | `.gitignore:14` `logs/` is an "uncommitted edit"; "**Not ignored: `docs/screenshots/*.png`**" | committed in `5334201`; `docs/screenshots/` ignored by `.gitignore:19` |
| 4 | settings "not in `Cataclysm.cfg` as in HomeworldSDL" | HomeworldSDL's file is `Homeworld.cfg` (`utility.c:150`); the game's `Cataclysm.cfg` has no `screen*`/`device*` keys |
| 5 | "With `UMU_ID` set, proton launches via `umu.exe`" | only for unix-path exes (`proton:2345-2347`); prefix programs go straight to `wine64` (`:2348-2350`) |
| 6 | "All scripts: ... `--help`, `--dry-run`" | scoped to the five runbook scripts (`checkpoint.sh` etc. have neither) |
| 7 | "1025x768" gamescope width listed as UNVERIFIED (evidence gone) | in `logs/screenshot-20261008.log:63` + `logs/run_game-20261008-094931.log:146`; 2045x1329 COSMIC resize in `:46-51` |

Script fix: `extract_installer.sh`, `run_game.sh`, `install_cet.sh` used plain `find` on `original/`/`original/game`, which does not enter a symlinked start directory or match a symlinked installer: a symlinked `original/game` made `run_game.sh` fall back to "no game directory" (reproduced with the pre-fix copy). Now `find -L` for the installer search, `find -H` for game-dir searches, and `readlink -f` on the game dir so `--force` moves the real directory. Re-tested: `bash -n`, `--help`, dry-runs, `--verify`, `--report`, `install_cet.sh` "already installed" on a symlinked tree.

Added (grounded): user ruling `docs/CONSTRAINTS.md` rule 1 (long play-tests via `--no-gamescope --virtual-desktop`, section 4 + checklist 7); `--virtual-desktop` needs `--size` + `--sync-res` with the same value; Qwen worker VRAM coexistence (troubleshooting); stock `cataclysm.exe` import table (no DSOUND, no OpenGL); `Update.big` push sites byte-checked in the real exe (`push 0x008C0F38`); registry value-name string addresses; frame limiter 0x0057B78B/0x0057B7A1, `soundStartDSound` 0x005808E0, startup globals, `rndAspectRatio`; tools not covered by the runbook.

Remaining issues:
- UNVERIFIED: how the stock exe reaches DirectSound without a DSOUND import (needs disassembly of 0x005808E0); the virtual-desktop size CET picks on first start; "2559x1329" virtual desktop and "641x360" (no logs); Windows default install folder (innoextract 1.9 shows no `DefaultDirName`; `strings` on the compressed setup data finds nothing).
- `--no-gamescope --virtual-desktop` (the user's long-play path) has never been run with the game.
- `original/game/` has no `EXTRACT_INFO.txt` (the 09:30 extraction predates the script); harmless.
- `docs/screenshots/*.png` are now git-ignored, so the frames cited in Status exist only on this machine.
