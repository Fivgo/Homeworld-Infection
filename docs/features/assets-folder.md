# The Assets folder: textures you repaint, used by the game

HWI-FEATURE(assets-folder). Code: `src/Game/AssetsFolder.c` (the index and the lookup), `src/SDL/assettex.c` (PNGs,
team masks and atlas pages turned into `.lif` images), hooks in `src/Game/File.c` (`fileLoadAlloc`, `fileLoad`,
`fileSizeGet`, `fileOpen`), `src/SDL/texreg.c` (`trLIFFileLoad`, `trMeshSortListLoad`, `trRegistryRefresh`, the
texture-memory fit), `src/SDL/utility.c` (start-up and shutdown), `src/SDL/main.c` (`/assetsPath`, `/noAssets`). The tool:
`tools/assets/export_assets.py` (it reuses `hwformats.py` and `export_ships.py`, docs/assets-export.md).

## The user's words (verbatim)
"I want to add a dedicated folder for the textures and assets that are used in the game so that they can be modified in a
blender environment. Then if those assets are ever updated like a texture map is repainted, then the game will use those
updated assets."

## The folder

`Documents/Homeworld Infection/Assets` on Windows, Linux and macOS: the visible user-files folder
(HWI-FEATURE(user-files), `fileUserFilesPath`; the user, 2026-10-31: "keep everything that can be intentionally edited by
the user, in viewable space. I want to refrain from causing confusion"). Documents is the Windows Documents known folder
(it may be in OneDrive), Linux's `XDG_DOCUMENTS_DIR` (else `~/Documents`), macOS's `~/Documents` (macOS asks the player
once to allow access to Documents). It was first `<settings folder>/Assets`, which is hidden on every system
(`~/.homeworld`, `%APPDATA%`, `~/Library`). `/userFilesPath <dir>` moves the user-files folder (Assets and Screenshots);
the user's development runs (`tools/play.sh`) use the project's top folder, so `Homeworld-Infection/Assets` (gitignored).
`/assetsPath <dir>` points a run at another Assets folder (the tests use it); `/noAssets` ignores the folder (and the
screen pictures in it).

What the export puts there (`tools/assets/export_assets.py`, below):

| path | what |
|---|---|
| `race_sect/sacolyte/rl0/lod0/0aco1.png` ... | every `.lif` texture of the archives as a PNG, at the path the game uses, lower case |
| `<name>_teambase.png`, `<name>_teamstripe.png` | the team base / stripe colour weight per pixel (greyscale), for a `.lif` with that flag |
| `credits/english/01.jpg`, `btg/bitmaps/*.tga` ... | the archives' other pictures, copied as they are (they are replaced as native files) |
| `Models/<race>/<ship>/<ship>_lodN.obj` / `.mtl` | each ship's source mesh for Blender, textured with the PNGs above; edited, the game draws it (HWI-FEATURE(assets-models), [assets-models.md](assets-models.md)) |
| `Models/<race>/<ship>/ingame/<ship>_lodN.obj` / `.mtl` | the paged mesh the game draws, textured with its atlas pages; edited, it replaces the paged mesh |
| `assets-models.txt` | which OBJ is which game mesh, per ship and level (since 2026-11-02; the engine reads edited OBJs with it) |
| `assets-manifest.txt` | `<size> <mtime> <sha256> <path>` of every exported file |
| `assets-pages.txt` | where each source texture sits in its atlas page |
| `README.txt` | for the user: what to edit, Blender / GIMP, what the game picks up, how to undo |
| `feman/backgrounds/<Screen>.png` | NOT part of the export: the pictures behind front-end screens, with their own loader (HWI-FEATURE(screen-background), [docs/features/screen-background.md](screen-background.md)). The index skips this folder and the exporter never writes, lists or flags it |

## The rules

**1. Only what changed is used.** At start-up (`assetsStartup`, right after the archives are opened) the folder is
indexed once: every file's size and time (stat, or `FindFirstFile` on Windows), nothing is read. A file is ACTIVE when
`assets-manifest.txt` does not list it, or lists another size or time (whole seconds). So an untouched export changes
nothing, and a repainted texture is used at the next start. Without a manifest every file that names a game file is used.
A copy of the folder that loses the file times makes every file active (harmless: the pictures are the game's own, but
the start does more work); `export_assets.py --refresh` re-stamps the files whose content is still as exported. Skipped
by the index: `Models/`, `feman/backgrounds/`, the three files `README.txt`, `assets-manifest.txt`,
`assets-pages.txt` at the top, and names starting with a dot.

**2. Native files: the same path replaces the game's file.** An active file whose path (case ignored, `/` or `\`)
names a file of the archives or of the game folder replaces it, whatever its type: `.lif`, `.geo` / `.peo`, `.fib`,
scripts, `.jpg`... The Assets folder comes before `update.big`, `cataclysm.big` and the loose game folder. It applies to
the game's reads (`fileLoadAlloc`, `fileLoad`, `fileSizeGet`, `fileOpen` for reading); never to settings files,
CD paths, absolute paths or writes. A file that names nothing is ignored (logged as `assets: ignored`), so OBJ, MTL,
`.blend` files and notes can live anywhere in the folder; the OBJs `assets-models.txt` names are the models (rule in
[assets-models.md](assets-models.md)). Changing gameplay files (`.shp`, scripts) changes the game
itself, and every player of a multiplayer game would need the same files.

**3. PNG textures.** For a texture `<name>.lif`, an active `<name>.png` replaces its pixels, and `<name>_teambase.png`
/ `<name>_teamstripe.png` replace its team weights (TeamColor0 / TeamColor1), each on its own: a mask left alone keeps
the game's weights for the repainted picture, a mask can also give team colour to a texture that had none. A native
`<name>.lif` in the folder wins over the PNGs. Decoded with stb_image (any PNG it reads: 8 or 16 bit, grey, RGB, RGBA,
paletted, interlaced; at most 4096 pixels a side).
- **Textures of the texture registry** (ships, derelicts, asteroids, effects: everything `trRegistryRefresh` loads) are
  built as RGBA `.lif` images in memory (`assetsLifLoad`) at the PNG's size. A paletted texture becomes RGB: its team
  colour is applied per pixel with the same weights and the same list team flags the paletted path applies to its
  palette, so an unchanged picture draws exactly as before (proven below). **Other sizes:** any size loads; non-powers of
  two too (the game already has 182 of them, GL draws them, the fit handles them). **Texture memory:** a replaced texture
  is counted in the fit (`trCramRAMComputeAndScale`) at its ORIGINAL size, so every other texture is scaled as without
  the replacement, and the replacement gets the original's halvings: a 2x picture always shows twice the original's
  detail; with TEXTURE MEMORY at the top (`TextureDetail 100`) nothing is halved.
- **Pictures drawn by other code** (`trLIFFileLoad` called directly: front-end and HUD pictures, cursors, the research
  and build screens' pictures) keep the game's size and format, because that code expects them: the PNG is scaled to
  the original size (nearest pixel), and a paletted picture (the 86 research-screen pictures, the colour picker's two)
  is mapped onto its own 256-colour palette: a pixel left as it was keeps its index, a changed one takes the nearest
  palette colour (so colours outside the palette are approximated).
- **Shared textures** (`textures.ll`): 3,866 textures are listed as a copy of another (the same picture, quantized
  separately); the registry draws the other one's file for them. The export therefore writes the PNG of such a texture
  from that file (3,156 PNGs; a page source keeps its own, below), and the engine builds a replaced one on that file too,
  so the PNG is what the game draws. A replaced texture is never shared, and a texture listed as a copy of a replaced one
  loads that one's ORIGINAL file (`TRF_SharedFileName`, the replacement suspended): a picture changes only where its
  own name is drawn. The 654 listed textures without a file of their own have no PNG and follow their parent's
  original.

**4. Atlas pages (the Paco finding).** The game draws a ship's `.peo` whenever it exists (`meshLoad`,
`mainAllowPacking`), and Relic's packer "Paco" copied small textures into `pageN.lif` atlases and rewrote the `.peo`
UVs: 99 of the 158 ships' LOD 0 meshes draw from pages; the archives hold 702 pages with 2,874 placements. So repainting
a source texture alone would change nothing on screen. The engine therefore REBUILDS a page at start-up when one of its
source textures changed (PNG, mask or native `.lif`): the page (its own PNG if that changed, else the game's page), every
changed source pasted at its place, mirrored across its diagonal where Paco rotated it, and the whole page scaled by
the largest whole factor a source's upscale needs (at most 4096 a side; the `.peo` UVs are fractions, so they stay
right; Paco's 0.6-texel seam nudge becomes 0.6 original texels). The places come from `assets-pages.txt`, which the
export writes (solved from the UVs and checked against the pixels, `export_ships.Exporter.placements`; one of the 2,874
is not an exact copy: `race_derelict/dslipgatenode` LOD 2 `page1` holds an older `sideboxtopbtm`). The workflow:
paint the source textures with the main OBJ (`Models/<race>/<ship>/<ship>_lod0.obj`), or paint the pages with the
`ingame/` OBJ; where a changed source sits it wins over a painted page.

## The export (`tools/assets/export_assets.py`)

```
python3 tools/assets/export_assets.py                     # into the project's Assets/ (about 11 s)
python3 tools/assets/export_assets.py --out DIR           # another folder (e.g. ~/Documents/Homeworld Infection/Assets)
python3 tools/assets/export_assets.py --status            # what changed since the export, and what each file replaces
python3 tools/assets/export_assets.py --refresh           # re-stamp files whose content is as exported
python3 tools/assets/export_assets.py --game DIR          # read cataclysm.big / update.big of a game folder (unpacked
                                                          # into a temporary folder) instead of original/game_extracted
```

- Source: the extracted archives `original/game_extracted/{cataclysm,update}`, `update.big` first, as the game reads
  them (a worktree uses the main checkout's `original/`).
- It never overwrites a file that differs from the previous export's manifest (the user's work stays, and stays in use),
  nor a file the previous export did not write; `--force` overwrites those. It never deletes anything. It refuses to write
  into or above `original/gog`, `original/game`, `original/game_extracted`, the harness data folder, the source folders
  or any folder holding `cataclysm.big`. It never writes into `feman/backgrounds/`.
- A re-export writes only files whose content changed (the others keep their time, so they stay inactive).
- The export of 2026-10-31 (made into `original/play-home/.homeworld/Assets`, moved with its file times to the
  project's `Assets/` the same day) (11 s): 25,413 files, 81.2 MB (161 MB on disk):
  12,096 texture PNGs (11,060 of them paletted in the game, 1,036 RGBA, 182 not a power of two; 3,156 from their
  `textures.ll` parent) and 10,364 team masks, 127 `.jpg` / `.tga`, 1,412 OBJ + 1,412 MTL for 158 ships, 702 pages with
  2,874 placements in `assets-pages.txt`; the 7 files already in `feman/backgrounds/` were left as they were.
- Models: `export_ships.Exporter` with its OBJ / MTL writer; the MTL `map_Kd` paths point at the PNGs above (relative),
  masks are named in MTL comments; axes and UVs as in docs/assets-export.md. The assembled modular hulls
  (`assemble_modular.py`) and the atlas export (`atlas_ships.py`) are not part of this folder.

## The log (`assets:` lines, stdout)

```
assets: folder <dir>: 22587 files in 1150 folders scanned in 67.5 ms; manifest 25413, unchanged 22587, active 0, ignored 0
assets: active race_sect/smothershipdockingbay/rl0/lod0/smo_dock_back.png (texture race_sect/.../smo_dock_back.lif, 128x256)
assets: active <path> (team base mask of <texture>.lif) | (replaces the game's texture, WxH) | (replaces the game's file)
assets: ignored <path> (no game file of that name)                      (the first 20, then a count)
assets: page race_sect/sacolyte/rl0/lod0/page0.lif is rebuilt (3 changed sources: 0aco1 0aco12 0aco17)
assets: texture <name>.lif 256x512 from png teambase changed-sources (game's 128x256 paletted) page scaled up   (at load)
assets: using <file>                                                    (a native file, at its first read)
assets: folder <dir>: none (nothing replaced)      /      assets: off (/noAssets)
```

## Tests (2026-10-31, on 77c77c9 + this branch; debug ASan build unless noted, offscreen driver, no input)

The ship scene is the Kuun-Lan at its closest zoom (skirmish LiveFireExercise2, `--scene mothership:2300 --shot 100`).

| test | result |
|---|---|
| builds | `tools/build.sh` (ASan/UBSan) and `--release`: 0 warnings; `tools/release_win64.sh`: links, the imports check passes (SDL2.dll and Windows' own), 0 warnings |
| the untouched full export (25,413 files) | `scanned 22587 files ... active 0`; the frame is byte-identical to a run without the folder (`differ=0`), also at `TextureDetail 100` |
| start-up time (release build, `--mode statics`, warm cache) | the index takes 65 to 79 ms for 22,587 files; the whole start 1.68 to 1.73 s without the folder, 1.76 to 1.81 s with it (about +70 ms). Debug ASan: 100 to 140 ms |
| every Kuun-Lan LOD 0 PNG and mask touched (252 files, time only): all of them load through the PNG path, 4 pages are rebuilt | byte-identical frame. Before the share-parent rule (rule 3) it differed by up to 8/255 on 6,312 pixels: `smo_dock_back` is drawn from its `textures.ll` parent's file, quantized with another palette |
| a repainted texture: `smo_dock_back.png` (the docking bay's back, the big face in view) tinted red | `active` / `texture ... from png`; 8,302 pixels change, all on that face (bbox 377,389-567,530); the PNG deleted: `active 0`, `differ=0` |
| team masks: `_teambase.png` all 0 / `_teamstripe.png` doubled / the base mask deleted | 1,984 pixels (the blue base-colour patches lose the team colour) / 772 pixels / `differ=0` |
| a native file: `smo_dock_sides_1.lif` (128x128) copied as `smo_dock_back.lif` (64x128); `notes.txt`, `my_paint.png`, `myship.blend` beside it | `active ... (replaces the game's texture, 128x128)`, `assets: using <file>`, 8,266 pixels change; the three others `ignored` |
| a 2x PNG (128x256, yellow one-pixel columns every 4 pixels) | drawn with twice the columns of the same 1x picture (8,083 vs 4,107 pixels changed); at the default texture memory it keeps the original's two halvings in height, at `TextureDetail 100` it loads at 128x256 |
| an atlas page: three of the acolyte `page0`'s six sources changed (two tinted, one 2x; two of the six are transposed) | the page is rebuilt at 256x512; the image the engine builds (a temporary dump) is byte-identical, RGBA and both team maps, to an independent Python composition |
| pictures drawn by other code: the 34 cursors tinted; the 107 research-screen pictures touched / tinted (`--mode build`, the research screen at tick 4194) | the pointer is drawn green (89 pixels); touched: byte-identical, the paletted 256x256 research picture mapped back onto its palette; tinted: 35,045 pixels change on the picture; `buildtest ... result=PASS` |
| a folder without a manifest and with a symbolic-link loop | the scan stops at 24 levels (71 folders, 2.5 ms), the PNG is used; the frame equals the same tint in the full export |
| a PNG in `feman/backgrounds/` | not indexed (same file count, `active 0`, `differ=0`) |
| the 6000-tick mission (`--mode mission --mission-ticks 6000`): plain, with the untouched export, with a repainted texture and a 2x one | all three IDENTICAL to `m3r3i-e-mission-a` (6001 ticks): textures never reach the simulation |
| ASan / LSan | `ASan errors: 0; LeakSanitizer at exit: no unsuppressed leak` in every run |
| `export_assets.py --status` after every test | 0 changed, 0 new, 0 touched, 0 missing (the tests put every file back with its time) |

## Edited meshes: done (2026-11-02)

This section listed what reading an edited OBJ would take (the format, the two files per level, every level of
detail, validation). It is built: HWI-FEATURE(assets-models), [assets-models.md](assets-models.md). An edited OBJ is
turned into the game's `.geo` image with the game's mesh as the template, replaces that level's `.geo` and `.peo` (an
`ingame/` OBJ: the `.peo`), and stands in for the ship's levels above it that have no edit of their own. The `.shp`
and `.mex` data stays as it is; the hit tests, the ramming frigate and the repair droids read the edited shape.
