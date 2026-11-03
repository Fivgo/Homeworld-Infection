# The game fills its Assets folder itself

HWI-FEATURE(assets-autoexport). Code: `src/Game/AssetsExport.c` / `.h` (the export and its thread),
`src/Game/AssetsExportReadme.h` (README.txt, generated), hooks in `src/Game/AssetsFolder.c` (`afExportMaybe` at the
end of `assetsStartup`, `assetsExportStop` in `assetsShutdown`, the unfinished export's journal read as the manifest),
`src/Game/BigFile.c` (`bigOpenedFilePaths`), `src/SDL/main.c` (`/noAssetsExport`, `/catAssetsExportWait`). Tools:
`tools/assets/compare_exports.py`, `tools/assets/gen_readme_header.py`; the harness's `--assets-export` /
`--assets-wait`. Builds on HWI-FEATURE(assets-folder) ([assets-folder.md](assets-folder.md)).

## The user's words (verbatim, 2026-11-02)
"I wanted that packed as a feature so that players themselves can modify the ships if they wanted. When they use the
appimage to unpack the game, I expect an assets folder to be found inside so that any changes to the folder will result
in those updated models to be piped into the game"

Players have no Python (the Windows and macOS packages, and most Linux players), so `tools/assets/export_assets.py` was
no way for them to get the folder. The engine now writes the same folder itself. Reading edited OBJ models back is the
sibling feature (the OBJ importer, branch assets-objimport); this one writes everything it needs, including
`assets-models.txt`.

## When it runs

At start-up, right after the archives are opened and the Assets folder is indexed (`assetsStartup`), when the folder
(`Documents/Homeworld Infection/Assets`, or `/assetsPath`) has no `assets-manifest.txt`:
- the first start of a new install, whatever the data source (a GOG folder, the offline installer unpacked by the game,
  `HW_Data`): the archives are the ones the engine opened;
- the player deleted the manifest (to get missing files back) or the whole folder (a fresh copy);
- an earlier export was stopped (quit, crash, power): it resumes;
- the manifest's export version is older than this game's, or it has none: the folder is **refreshed** (below).

Not with `/noAssets`; `/noAssetsExport` turns it off. A folder that holds `cataclysm.big` (or is inside one that does)
is never written, like the Python tool's `check_out`. `tools/play.sh` runs use the project's `Assets/`, which has its
Python-made manifest, so they never export; every harness run passes `/noAssetsExport` unless `--assets-export` is
given.

## Design

- **A C port of `export_assets.py`** (with the parts of `export_ships.py` and `hwformats.py` it uses): the .big TOC and
  names, LZSS, `.lif` (paletted, RGBA, team maps), `textures.ll` (the share-parent rule), `.geo` / `.peo`, `.lod`
  scripts (and the `rl0/lodN` fallback), Paco's page placements (the UV solver with Python's round-half-to-even, then
  the pixel search), the OBJ / MTL writer with `fmt` (`%.6f` without trailing zeros, `-0` as `0`, NaN as `nan`: 108 OBJ
  files of the game have NaN normals), `assets-pages.txt`, `assets-models.txt`, `README.txt`. Same output: every file
  byte for byte except the PNGs, whose pixels and colour type are the same and whose deflate differs.
- **README.txt is one text**: `export_assets.README_TEXT`; `tools/assets/gen_readme_header.py` writes it into
  `src/Game/AssetsExportReadme.h` (`--check` tells when the header is out of date). The text now explains the export,
  how to change a ship's shape (Blender import, keep the part names, export over the same file; a changed lod0 is
  drawn at every distance; collision size, gun, engine and docking points stay) and how to undo.
- **Its own thread** (`SDL_CreateThread`, low priority): the game never waits; the intros, the menu or a mission run
  while it works. The thread opens the archives again by their paths (`bigOpenedFilePaths`, filled in
  `bigOpenAllBigFiles`) with its own `FILE` handles, never the engine's; it has its own LZSS (the engine's
  `ThirdParty/LZSS/LZSS.c` keeps a static window), its own `mkdir -p` (`File.c`'s `fileMakeDirectory` uses a global
  `stat` buffer) and malloc memory; it touches no game state. At quit `assetsShutdown` sets a stop flag, which the
  thread checks between files, and waits up to 10 s (with `/catAssetsExportWait` it lets the export finish instead).
- **PNGs**: filter type 0 on every row, as `hwformats.png_bytes`, deflated by stb_image_write's `stbi_zlib_compress`
  (already in the engine; its fixed Huffman codes make the folder 90.5 MB instead of Python's 80.2 MB). Letting stbiw
  choose filters gave 106.9 MB; its global settings are left alone (screenshots use them).
- **Never half a file, never the player's file.** Each file is written as `.<name>.hwi-export` (a dot name: both scans
  skip it) and renamed. Each written file is journalled in `assets-manifest.partial` (the manifest's line format,
  flushed per file); at the end the journal becomes `assets-manifest.txt` (written through a temporary and renamed)
  and is deleted. The engine's scan reads the journal as the manifest when `assets-manifest.txt` is missing, so the
  files of a stopped export are "as exported" (inactive) at the next start, which resumes: a journalled file whose size
  and time are unchanged is taken over without being built again. The Python Writer's rules otherwise:

| a file already in the folder | the export |
|---|---|
| journalled, unchanged | taken over (resume) |
| journalled, changed since (the player edited it before the resume) | kept; the manifest keeps the journal's record, so it stays in use; logged `kept your <file>` |
| not journalled, the same bytes as the export's | taken over |
| not journalled, a PNG with the same size, colour type and pixels (e.g. written by the Python tool) | taken over |
| not journalled, different (the player's own) | kept, not listed, so it stays in use; logged `kept <file> (not from an export)` |
| anything in `feman/backgrounds/` | never written, never listed (the screen pictures, HWI-FEATURE(screen-background)) |

- **Export versions and the refresh** (the coordinator, 2026-11-02: "add an export version so a newer game refreshes
  an existing folder"). The manifest's header carries `# export-version N` (`AE_ExportVersion` in
  `src/Game/AssetsExport.h`, `EXPORT_VERSION` in `export_assets.py`: raise both whenever the export's output changes;
  it is 1 now). Both readers skip it as a comment. At start-up a manifest without the line, or with an older number,
  starts the same background export as a refresh, under the Python Writer's rules with the manifest:

| a file in the folder, during a refresh | the export |
|---|---|
| listed, still as exported (its sha256 is the manifest's), the same content as now | taken over; a PNG whose size and time are the manifest's and whose pixels are the new ones is taken over without being encoded again, whatever its deflate (a folder made by the Python tool keeps its PNGs) |
| listed, still as exported, other content now (the export changed) | rewritten |
| listed, changed by the player | kept; the new manifest keeps its old record, so it stays in use (`kept your <file>`) |
| not listed (the player's own) | kept, not listed, in use (`kept <file> (not from an export)`) |
| listed but deleted | written again, as exported (`of them N deleted ones again`) |
| not there, not listed (a file the newer export adds, e.g. `assets-models.txt`) | written |

  Re-creating a deleted file is the Python tool's rule too, and it is safe: the file comes back exactly as exported,
  so the manifest lists it as unchanged and the game keeps drawing its own, as it did while the file was missing; only
  the reference copy is back. (Not re-creating would give the same game, but the two tools would then write different
  folders.)

  The old manifest stays in force until the new one replaces it (a rename at the end). The engine's scan reads the
  journal after the manifest, its records winning, so a file a stopped refresh already rewrote stays unused at the next
  start. A run with failed files writes its manifest without the version line, so the next start refreshes again and
  retries them. `export_assets.py --refresh` (re-stamping file times) keeps the folder's version, since it adds no file.
- **The log**: `assets: export: started|resuming into <folder>`, `refreshing <folder> (its export version N, this
  game's M; F files listed)`, `stopped at quit after N files`, and at the end
  `done in S s: N files in the manifest (written, already there, kept yours, kept not ours, failed), MB` plus the
  same counts the Python tool prints.

## Tests (2026-11-02; Linux release build unless noted, offscreen driver, hidden and silent, no input)

The reference is a Python export (`export_assets.py --out <scratch>`, 25,414 files, 80.2 MB, 11.0-11.3 s) of the same
archives; `compare_exports.py REF OTHER` checks the same file list, byte-identical non-PNG files, the same PNG size,
colour type and pixels, and that each manifest lists exactly its folder with true sizes, times and sha256s.

| test | result |
|---|---|
| first start, export allowed to finish (`--mode modequit --assets-wait`) | `done in 5.5-6.0 s: 25414 files ... written 25414, failed 0, 90.5 MB`; textures 12,096 (paletted 11,060, RGBA 1,036, not a power of two 182), team masks 10,364, pictures 127, pages 702 with 2,874 placements (1 not exact), 3,156 from their share parent, 158 ships, 1,412 OBJ: every count as the Python tool's. `compare_exports`: **EQUAL** (22,460 PNGs by pixels, 2,954 files byte for byte). `export_assets.py --status`: 0 changed, 0 new, 0 touched, 0 missing |
| the realistic first start: a skirmish (`--mode skirmish --skirmish-ticks 120 --scene mothership:2300 --shot 100 --assets-export`, no wait) | the export done in 5.9 s while the game ran; wall 8.54 s, the same as without the export (8.54 s); `skirmishtest ... result=PASS`; the frame byte-identical to the run without a folder; the folder EQUAL |
| stopped and resumed: quit at 0.5 s, again, then finished | `stopped at quit after 463 files`; next start `an unfinished export (assets-manifest.partial)`, `1575 files scanned ... unchanged 1575, active 0`, `resuming`, stopped after 1,575; then `done ... written 23839, already there 1575`; the journal gone; EQUAL; `--status` 0/0/0/0 |
| second start on the finished folder | no export; `22587 files in 1150 folders scanned in 72.4 ms; manifest 25414, unchanged 22587, active 0`; the Kuun-Lan frame byte-identical to a run without the folder |
| the player's files before the first export: a different `0aco1.png`, the Python tool's `0aco12.png` (other deflate, same pixels), `feman/backgrounds/Video_Options.png` | `kept race_sect/sacolyte/rl0/lod0/0aco1.png (not from an export)`, not in the manifest, the file unchanged; `0aco12.png` taken over (`already there 1`), unchanged; the background untouched and unlisted. Next start: `active 0aco1.png`, `page0.lif is rebuilt`, `active 1`, no export |
| the player edits a file between a stop and the resume (`colorpicker/race1/race1.png` inverted) | `kept your colorpicker/race1/race1.png`; the manifest holds the journal's record; next start `active colorpicker/race1/race1.png`, `active 1` |
| `/noAssetsExport` (the harness default), `--assets-export -- /noAssets` | `export: off (/noAssetsExport)` / `assets: off (/noAssets)`; the user folder stays empty |
| Mission01, 6,000 ticks, debug ASan build, the export running beside it (`--mode mission --mission-ticks 6000 --assets-wait`) | **IDENTICAL** to `m3r3i-e-mission-a` (`hwo_compare`: 6,001 ticks, no difference); the export done in 30.4 s (ASan, -O0) and EQUAL; `ASan errors: 0; LeakSanitizer at exit: no unsuppressed leak` |
| stopped at quit under ASan (`--mode modequit --assets-export`) | `stopped at quit after 3 files`; ASan 0, no leak; no temporary file left |
| Windows build (`tools/release_win64.sh`, llvm-mingw) under Proton Experimental's Wine, a fresh prefix, the window hidden, Wine audio off, `HW_Data` | the export into `C:\users\steamuser\Documents\Homeworld Infection\Assets` in 20.9 s, `modequittest ... PASS`; EQUAL to the Python export (the OBJ numbers through `__mingw_snprintf`); a second start: `22587 files ... active 0`, no export |
| refresh of a folder made by the tool before 43b9734 (`git archive 43b9734^ tools/assets`: no `assets-models.txt`, the old README, no version line) with `race_sect/sacolyte/rl0/lod0/0aco1.png` repainted (colours inverted), debug ASan build, `--assets-wait` | the scan first: `active 0aco1.png`, `page0.lif is rebuilt`, `active 1`; then `refreshing ... (its export version 0, this game's 1; 25413 files listed)`, `kept your race_sect/sacolyte/rl0/lod0/0aco1.png`, `done in 9.8 s: 25414 files ... (written 2, ...; already there 25411, kept yours 1, ...)`: `assets-models.txt` and the new README written, every other file taken over (no PNG re-encoded); the repainted PNG byte-unchanged, its manifest record the old one; `# export-version 1`; `compare_exports` against the current Python export: the only 2 differences are that PNG's pixels and its (kept) manifest record; `ASan errors: 0`, no leak. Second start (release): no export, `active 0aco1.png`, page0 rebuilt, `active 1` |
| refresh with a deleted file, a stale file and a stop: the same old-tool folder with `Models/race_sect/sacolyte/sacolyte_lod1.obj` deleted and `colorpicker/race1/race1.png` repainted with its manifest line patched to match (as if an older export had written it) | stopped after 4,803 files (1 written: the stale PNG); next start `an unfinished export`, `manifest 30216` (since then counted once: 25,414 + the journal's new keys), `unchanged 22587, active 0` (the rewritten PNG counts as exported through the journal); stopped again; then `done in 1.7 s ... written 3, of them 1 deleted ones again`: the OBJ back, the journal gone, **EQUAL** to the current Python export; the next start: no export, `active 0` |
| the current export versions | a fresh first start: `# export-version 1`, EQUAL, `--status` 0/0/0/0; a Python export now writes `# export-version 1` too, and the game starts on it without exporting (`active 0`); `export_assets.py --refresh` keeps the line |
| builds | `tools/build.sh --release` and `--debug` (ASan/UBSan): 0 warnings; `tools/release_win64.sh`: 0 warnings, the imports check passes |

## Not done, limits

- No progress shown in the game: the export is silent (the log only). It takes about 6 s on this machine (21 s under
  Wine), normally during the intros.
- 90.5 MB instead of the Python tool's 80.2 MB (stb_image_write's deflate). A better deflate would need new code or a
  library.
- A refresh needs `AE_ExportVersion` / `EXPORT_VERSION` raised by hand when the export's output changes; nothing
  checks that a change to the output came with a new version.
- Not run on a real Windows PC or on macOS (the macOS build compiles in CI; the export's paths there are the
  `Documents` folder the game already uses).
