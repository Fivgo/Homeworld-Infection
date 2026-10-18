# Ship asset export for Blender (`tools/assets/export_ships.py`)

Written 2026-10-17. The tool turns every ship mesh and ship texture of Homeworld: Cataclysm into files Blender opens
directly: Wavefront OBJ + MTL with PNG textures. The user upscales the PNGs and hands them back for a later tool that
writes them into `.lif` files for the GOG game and loads them natively in the recreation. The export itself is game
data and lives in `original/assets-export/` (gitignored); this page holds no game data.

## Running it

```
python3 tools/assets/export_ships.py                 # export everything, then validate (about 15 s)
python3 tools/assets/export_ships.py --only race_sect/scarrier race_beast
python3 tools/assets/validate_export.py              # validate an existing export again
```

Python 3 standard library only, including a small zlib PNG encoder/decoder. The three files are:

| file | role |
|---|---|
| `tools/assets/hwformats.py` | readers: `.geo`/`.peo` meshes, `.lif` textures, `.lod` scripts, PNG, and a case-insensitive view of the extracted archives with `update.big` first |
| `tools/assets/export_ships.py` | the exporter; it also writes `README.md` (for the user), `index.md` and `manifest.json` into the output |
| `tools/assets/validate_export.py` | the checks listed under "Validation"; writes `validation.json` |

Output layout (`original/assets-export/`):

```
README.md  index.md  manifest.json  validation.json
ships/<race>/<ship>/<ship>_lod<N>.obj|.mtl             source mesh (.geo), individual textures
ships/<race>/<ship>/ingame/<ship>_lod<N>.obj|.mtl      paged mesh (.peo) the game loads, texture pages
textures/<race>/<ship>/lod<N>/<name>.png               base texture, RGBA
textures/<race>/<ship>/lod<N>/<name>_teambase.png      TeamColor0 weight, greyscale
textures/<race>/<ship>/lod<N>/<name>_teamstripe.png    TeamColor1 weight, greyscale
textures/<race>/<ship>/lod<N>/pages/page<K>.png        texture pages (+ masks)
```

The `lod<N>` folder level is necessary: 2,319 exported texture names occur in more than one LOD folder of the same
ship with different pixels, up to five versions of one name.

## What the exporter does, and where each rule comes from

* **Ships and LODs.** Every folder under the seven `race_*` directories is a ship. Its `<ship>.lod` script gives the
  LOD levels (`typeN` / `pMeshFileN`, HW1 `Game/LOD.c:38-73`); the mesh path is the race folder plus the field
  (`lodMeshFileLoad`). Five folders have no `.lod` (`smothershipgenetics`, `bmissiledestroyer`,
  `trresearchstationbridge`, `trresearchstationtower`, `dplanetoforigin`); their LODs come from the `rl0/lodN`
  folders. When a `.lod` names one mesh for several levels (34 levels in 13 derelict objects), the mesh is exported once
  and the other levels point at it (`same_mesh_as_level`).
* **Two meshes per LOD.** Every LOD has a `.geo` and a `.peo`. `meshLoad` loads the `.peo` whenever it exists
  (`mainAllowPacking` = 1, docs/formats/mesh.md), so the `.peo` is what the game draws. The `.peo` was written by
  Relic's packer "Paco" (`reference/gardens-of-kadesh/zarchive/original-release-snapshot/tools/win32/Paco`), which
  copied small textures into `pageN.lif` atlases and rewrote the UVs; the polygon order also differs from the
  `.geo` in 682 of the 731 pairs (Paco has a resort-by-material option). The main OBJ
  uses the `.geo`, whose materials name the individual source textures: those are the files to upscale. The `ingame/`
  OBJ uses the `.peo` and the pages.
* **Mesh layout.** `docs/formats/mesh.md`: 0x44 header, 0x74-byte Cataclysm polygon objects (the reader also accepts
  HW1's 0x70), 0x20 materials, 16-byte vertices, normals and vertex-info entries, 40-byte polygons. Each object's
  vertices are moved by its `localMatrix` chain, mother first, as `meshObjectRenderR` does with `glMultMatrixf`
  (HW1 `Game/Mesh.c`). Every object of every exported mesh is reachable from object 0 and every local matrix is rigid
  (orthonormal to 5e-7).
* **Axes.** The game's ship frame is x = up, y = right, z = forward: HW1 reads up from column 1 and heading from column 3
  of `rotinfo.coordsys` (`Game/Camera.c:645-646`), and the meshes agree (z is the long axis with the engine-named
  textures at -z, y is the mirror-symmetric axis). OBJ (x, y, z) = game (-y, x, z), a proper rotation, so Blender's
  default import shows ships level with the nose toward its front view. In 71 of the 76 LOD 0 meshes that have
  engine-named textures, those faces lie in the rear half (-z); the five exceptions are mothership modules, a buoy,
  the Sect carrier and the Taiidan resource collector. Checked in headless Blender 5.2.2 on seven OBJs: models
  symmetric in X, engine faces at +Y on the four checked ships that have them at the rear.
* **Winding and normals.** All 103,577 non-degenerate polygons of the 731 `.geo` files in the race folders wind
  counter-clockwise relative to
  their stored face normal, as OBJ expects and as the game culls (`glCullFace(GL_BACK)`, default `GL_CCW`).
  Materials with `MDF_Smoothing` (0x2) use per-vertex normals, the others the face normal, as `meshObjectRender`
  picks `MPM_Smooth*` vs `MPM_Flat/Texture`.
* **UVs.** The game uploads `.lif` row 0 as texture row 0 (`glTexImage2D`) and samples it at t = 0. The PNGs keep the
  file's row order (row 0 at the top), which shows the art upright: embossed logos such as `smo_mic_logo` are lit from
  above. So the OBJ writes `vt s (1 - t)`. UVs outside 0..1 are real tiling (`GL_REPEAT`, `trRGBTextureCreate`):
  13.9 % of all UV corners, in 566 of 706 meshes. The render-time PACO seam nudge (`meshObjectFixupPacoUV`, 0.6 texel)
  and the 0.51-texel edge nudge (`meshObjectFixupUV`) are not applied; the OBJ has the stored UVs.
* **Textures.** `docs/formats/textures.md`: "Willy 7" v0x104. Paletted textures are expanded through their RGBA palette
  (bytes R, G, B, A per HW1 `Color.h`); every palette entry used by a ship texture has alpha 255, so they are opaque.
  The 88 RGBA ship textures all carry real alpha, which is kept. The texture path is the mesh folder plus the material's
  texture name (`meshTextureNameToPath`).
* **Team colour.** A `.lif` with `TRF_TeamColor0` (0x10) or `TRF_TeamColor1` (0x20) gets `<name>_teambase.png` and/or
  `<name>_teamstripe.png`: the per-pixel weight that `trBufferColorRGB` uses to blend toward the player's base and
  stripe colours (`documentation/File Formats/LiF Specification.txt`: "TeamColor0 is the base color, TeamColor1 is the
  stripe color"). For paletted textures the 256-entry `teamEffect` tables are looked up per pixel index, as
  `trMeshSortListLoad` does (`teamEffect0[data[index]]`).
* **Page placements.** For every page the manifest lists where each source texture sits: `x, y, w, h` in page pixels and
  `transposed` (Paco's `pacRotate` mirrors a texture across its diagonal). They are solved from the UVs with Paco's
  `meshFixupSingleUV` formula, page UV = (UV * size + packed corner) / page size, after matching each `.peo` polygon to
  its `.geo` polygon by object and vertex triple. Every placement is then compared with the pixels.

## Counts (export of 2026-10-17)

| item | count |
|---|---|
| ship folders (Sect 35, Beast 30, Kushan 16, Taiidan 20, P1 7, Traders 11, Derelict 39) | 158 |
| distinct LOD meshes exported, each as source OBJ + in-game OBJ | 706 |
| OBJ files / MTL files | 1,412 / 1,412 |
| polygons over all exported LOD meshes (LOD 0 alone) | 102,853 (39,183) |
| source textures as PNG (LOD 0 alone) | 9,090 (2,515) |
| texture pages as PNG | 667 |
| team-colour mask PNGs | 9,030 |
| PNG files in total | 18,787 |
| source textures with team colour: base / stripe / either | 4,647 / 3,081 / 5,039 |
| source textures with alpha (all RGBA) | 88 |
| page placements (exact pixel copies) | 2,779 (2,778) |
| output size (apparent / on disk, many small files) | 62 MB / 132 MB |

Texture sides run from 2 to 256 pixels, mostly 16 to 128. One texture is not a power of two
(`race_beast/bcruisemissile/lod4/4bcr5`, 32x90).

## Validation (all pass)

| check | result |
|---|---|
| OBJ parsed back: v / vt / vn / f equal the counts from the mesh reader; every face has 3 in-range corners; every `usemtl` is defined | 1,412 / 1,412 |
| MTL `map_*` paths exist | 20,698 / 20,698 |
| PNG decoded (own reader, all filters) and pixel-identical to a fresh `.lif` decode, masks included | 18,787 / 18,787 |
| independent mesh reader `tools/formats/mesh_probe.py`: vertex and polygon totals equal the OBJ's | 1,412 / 1,412 |
| independent texture reader `tools/formats/textures_probe.py` on every exported `.lif` | 9,757 OK |
| page placements re-checked on the written PNGs | 2,778 exact, 1 known difference |
| extra, by hand: Pillow opens all 18,787 PNGs; 800 sampled are pixel-identical to the tool's reader | pass |
| extra, by hand: headless Blender 5.2.2 imports 7 OBJs (incl. one `ingame/`); every image loads; alpha textures get Image Alpha linked to BSDF Alpha | pass |

Spot check (LOD 0, `validation.json` → `spot_check`): face counts equal `mesh_probe.py` in all seven ships. UVs are not
all inside 0..1, by design (tiling):

| ship | faces | vt inside 0..1 | u range | v range |
|---|---|---|---|---|
| race_sect/scarrier | 633 | 94.2 % | 0.001 .. 2.20 | -1.00 .. 1.00 |
| race_beast/bacolyte | 270 | 93.5 % | 0.000 .. 1.51 | -1.00 .. 1.00 |
| race_kushan/kcarrier | 373 | 77.7 % | 0 .. 20.4 | -1 .. 1 |
| race_taidan/tcarrier | 341 | 70.7 % | 0 .. 5.92 | -12.1 .. 1 |
| race_p1/p1mothership | 366 | 94.8 % | 0 .. 2.70 | 0 .. 1 |
| race_traders/trfloatingcity | 436 | 79.2 % | 0 .. 17.4 | -0.79 .. 1 |
| race_derelict/dslipgatenode | 128 | 97.8 % | 0 .. 1 | -0.01 .. 1 |

Blender drops a triangle that repeats the three corners of another (253 such pairs over all LOD meshes, 58 in LOD 0;
230 of them share texture and UVs), so Blender can show a few faces fewer than the OBJ, for example 371 of 373 for
`kcarrier`.

## Not exported, and other findings

* **50 stray mesh files**, not named by their ship's `.lod`: extra `lod4` (and `dcrate` `lod1-4`) folders the scripts
  never load, a `tmissiledestroyer` mesh inside `tproximitysensor/`, and the one 0x402-stamped
  `nislensflaresource1/rl0/lod0/nislightsource1.geo`/`.peo`. Listed in `manifest.json` → `not_exported`.
* **`race_sect/srepairdroid.lod` names LOD 1-4 meshes that do not exist** (only `lod0` ships). HW1's `lodMeshFileLoad`
  falls back to `defaultmesh` for a missing file; whether Cataclysm does the same is not checked.
* **One page copy differs from its source:** `race_derelict/dslipgatenode/rl0/lod2/page1.lif` holds an older
  `sideboxtopbtm` (8x8). Its placement comes from the UVs and is marked `exact_copy: false`.
* **1,104 `.lif` files in ship folders are used by no exported mesh** (stale pages, unused variants). Listed per ship
  under `unreferenced_lifs`, not converted.
* `update.big` overrides no mesh, texture or `.lod` file (its 39 files are `.shp` stats, two scripts, one `.fib` and
  multiplayer descriptions), so nothing came from it.
* All 706 source meshes are `RMF97ba` `.geo` files and their 706 partners `RMF99ba` `.peo`, all version 0x500 with the
  Cataclysm 0x74 layout. No mesh or texture failed to decode.

## Return trip (plan for the next tool)

The user puts revised PNGs at `original/assets-return/<race>/<ship>/lod<N>/<name>.png` (the path under
`textures/`), with masks beside them, and returns only what they changed: anything missing keeps the original.
`manifest.json` maps each PNG to its `.lif` (`lif`, `return_png`), records the original size and flags, and gives a
pixel checksum (`pixels_sha1`) to find identical textures.

1. **Back to `.lif`.** Write RGBA `.lif` files (flag `TRF_Alpha` 0x8 plus the original team flags; data `w*h*4`, then a
   `w*h` team map per flag, `docs/formats/textures.md`). This avoids re-quantizing to 256 colours. If only a base PNG
   comes back, scale the original masks by the same factor. Whether Cataclysm's ship-texture path (`trMeshSortListLoad`,
   listed as not compared line by line in textures.md) colourizes RGBA textures exactly as HW1 does is UNVERIFIED and
   should be tested on one ship first.
2. **Rebuild the pages.** For every page, paste each scaled source texture at `(x, y) * factor`, transposed when
   `transposed` is true, then write the page `.lif`. The `.peo` UVs are fractions and stay valid. The one inexact
   placement rebuilds from the current source.
3. **Update `textures.ll`.** The game measures textures from the `textures.ll` listing first
   (`trImageMeasureFromListing`, binary search) and reads the `.lif` header only for unlisted names, so a loose
   `textures.ll` with the new sizes and flags must ship alongside, kept sorted by `_stricmp`.
4. **PACO seam data.** `.peo` polygons store log2 of the page size in `reserved[1]`, which sizes the 0.6-texel seam
   nudge. After upscaling, either patch it in loose `.peo` files or accept a nudge of 0.6 old texels.
5. **Texture memory.** Cataclysm halves textures until the total fits a budget set by the texture-detail option
   (`trCramRAMComputeAndScale`, `cat_trTextureMemorySet`, 1-12 MB cram limit; 30 % in mission 17). Upscaled textures will
   be shrunk back unless that limit is raised, for example by the CET hook `LoadMission_AdjustTextureMemoryLimit`.
6. **Loose files for the GOG game.** The GOG install already ships three loose front-end `.LiF` files that shadow
   archive entries (docs/big-archive-contents.md 1.7), but the retail override rules (timestamp compare, `fileOpen`
   preference) are UNVERIFIED there; the return tool should confirm with one texture first.
7. **The recreation** can load the PNGs directly. If it keeps `meshLoad`'s `.peo` preference it needs the rebuilt pages;
   loading the `.geo` with the individual textures avoids pages entirely.

## Modular ships: assembled models (`tools/assets/assemble_modular.py`)

Added 2026-10-17. Three hulls are built from parts that are ships of their own: `race_sect/smothership` (the Kuun-Lan),
`race_beast/bmothership` and `race_sect/scarrier`. The tool reads the parts' exported OBJs and writes, per hull,
`<hull>_assembled_lod<N>.obj|.mtl` (one OBJ object per part, materials renamed `<label>__<name>` and pointing at the
parts' own PNGs) and `<hull>_assembly.json` (category, dock point, transform and checks per part). It also writes a
user-facing section into the export's `README.md` between marker comments. A full export deletes all of this, so run
the tool again after every export:

```
python3 tools/assets/assemble_modular.py              # LOD 0 of all three hulls (under a second)
python3 tools/assets/assemble_modular.py --lods all --blender   # every LOD, plus a headless Blender import check
```

How each part is placed (code in `.claude/worktrees/m2/src`, read-only):

* **Made with the ship:** `cat_univCreateMothershipModules` (`Game/UnivUpdate.c:2219-2267`) puts each module at the
  host's position and clamps it (`Game/Clamp.c:28-52`). Host and module both start with `defaultshipmatrix`
  (`Game/UnivUpdate.c:351`), so the clamp offset is zero and the relative rotation is the identity. The module meshes
  are authored in the mothership's own frame. Independent check: every vertex of Relic's merged lower-section mesh
  `race_beast/bsectmoship` coincides with a vertex of the identity-placed lower spine, docking bay or lower engines.
* **Built in play:** the external-build launch (`Game/Dock.c:2700-2716`) puts the part at host + dock point position
  (the raw MEX `Dok` chunk, `Game/MEX.c:354`, from the `.mex` named by the hull's `pMexData`) and orients it with
  columns (up, right, heading) from the `.shp` heading/up codes (`dockPutShipOutside`, `Game/Dock.c:2969-3003`). The
  seven `DPT_Module_EB` points of the Kuun-Lan all sit at the origin with heading forward and up up, so this gives the
  identity too, the same as the code path above.
* **Supports** (`DPT_Support_EB`, `Game/Dock.c:1500-1503`) go to their own points and are turned by the parity of the
  point's index within its type (`Game/Dock.c:3013-3050`, exe `0x00463350`): Sect and carrier odd points
  `* RotX(pi)`; Beast bit 2 `* RotY(pi) * RotZ(+-7 deg)`, then bit 1 `* RotX(pi)`. The constants were read from the
  exe (`0x0072a7d8` pi, `0x0072a7cc` +7 degrees, `0x0072a7d0` -7 degrees). The support positions are the same in all
  five LOD `.mex` files.
* **Axes:** a part transform (M, t) in game space is applied to the exported OBJ as v' = A (M A^T v + t) with
  A = `to_obj_axes`.

Which parts start with the ship comes from `cat_univMothershipModuleTypesGet` and
`cat_univBeastMothershipModuleTypesGet` (`Game/Universe.c:5350-5424`). `smothershipgenetics` is cut content: the
Kuun-Lan `.shp` lists a `Genetics` dock point and the mesh exists, but there is no ship type, `.shp` or unit cap.
`bcarrier` has no module or support dock points and is not assembled. `bsectmoship` is never attached to a hull by
code.

Validation (LOD 0; every check passes; the tool exits 1 on any failure):

| check | smothership | bmothership | scarrier |
|---|---|---|---|
| objects (parts, supports numbered) | 26 | 15 | 7 |
| v / f written = sum over parts = manifest counts | 1,111 / 1,574 | 694 / 848 | 526 / 753 |
| every `usemtl` defined, every `map_*` and mask path exists | yes | yes | yes |
| every part's box touches another part's box | yes | yes | yes |
| support pairs mirrored left/right (worst error, units) | 6 pairs (0.000) | 6 pairs (0.434) | 3 pairs (0.000) |
| headless Blender 5.2.2 import: objects, faces, images loaded | 26, 1,574, 81/81 | 15, 848, 19/19 | 7, 753, 41/41 |

The Beast's upper and lower struts overlap by about 10 units where they meet, and their outer edges line up with
the hull's half-width. That fits the 7-degree turn of the lower struts.
