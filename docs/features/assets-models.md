# The Assets folder's models: ships edited in Blender, used by the game

HWI-FEATURE(assets-models). Code: `src/Game/AssetsModels.c` (OBJ -> the game's mesh image, the round-trip comparison),
`src/Game/AssetsFolder.c` (which models are edited, the level stand-ins, new pictures, `/catAssetsModelTest`),
`src/Game/Mesh.c` `meshLoad` (uses the image), `src/SDL/texreg.c` + `src/SDL/assettex.c` (new pictures). The map:
`assets-models.txt`, written by `tools/assets/export_assets.py` (43b9734) and by the engine's own export.

The user (2026-11-02): "I wanted that packed as a feature so that players themselves can modify the ships if they
wanted. When they use the appimage to unpack the game, I expect an assets folder to be found inside so that any changes
to the folder will result in those updated models to be piped into the game." Before this, the game read the pictures
only (docs/features/assets-folder.md, "Not done: loading an edited mesh").

## What a player does

Open `Models/<race>/<ship>/<ship>_lod0.obj` from the Assets folder in Blender (File > Import > Wavefront), change the
shape, and export it over the same file (File > Export > Wavefront, default settings). At the next start the game draws
the new shape. Deleting the file (or exporting the folder again) brings the game's own back.

## Rules

1. **Which OBJ is which mesh.** `assets-models.txt` (tab-separated: ship, level, source mesh, OBJ, in-game mesh,
   in-game OBJ or `-`; game paths, lower case) is read at start-up while the manifest is loaded. An OBJ is **edited**
   when it or its `.mtl` has another size or time than `assets-manifest.txt` lists (or is not listed): the textures'
   rule. OBJs it does not name are ignored. Without `assets-models.txt` (an export from before 2026-11-02) models are
   not read, and the log says so once. A folder without `assets-manifest.txt` counts every OBJ it names as edited (as
   it uses every texture PNG). Cost: 1,412 OBJs and their MTLs checked in 6 to 18 ms (release).
2. **Main or in-game model.** An edited main OBJ (`Models/<race>/<ship>/<ship>_lodN.obj`, made from the `.geo`)
   replaces that level's mesh, `.geo` and `.peo` both: the paged mesh is not used, the source textures are registered
   as for a `.geo`. An edited `ingame/` OBJ (made from the `.peo`) replaces the paged mesh and draws from the atlas
   pages. Both edited: the main one is used (logged). A mesh two ships share is taken by the first edited line.
3. **Levels without an edited model.** A level of a ship without an edited OBJ uses the nearest lower edited level of
   the same ship, so an edited `lod0` shows at any distance (one log line per ship). FULL DETAIL (on by default) draws
   level 0 everywhere anyway; with it off, the far levels show the edit too.
4. **Built when the game loads the mesh**, once per run (`assetsModelFind`, cached): the game's own mesh (read from the
   archives, never a replacement) is the template, and the result is the on-disk `.geo` image (`RMF97ba`, 32-bit file
   offsets) that `meshLoad` reads as if from a file, with the mesh's file name switched to the template's (its folder
   holds the textures). A level that stands in keeps the edited level's texture folder.
5. **Objects** are found by the export's names (`export_ships.Exporter.write_obj`: whitespace as `_`, `object<N>` when
   empty, `<name>_<N>` for a repeated name); then without Blender's `.001` suffix; then without regard to case. `o`
   starts an object; a `g` counts only when it names one. Each object keeps its name, flags, mother / daughter / sister
   links and local matrix, so the `.mex` points, the `.mad` animations and the code still find their parts. Faces of an
   OBJ object the template does not have go to object 0 (logged); a template object the OBJ lacks stays, empty.
6. **Positions and normals:** game (x, y, z) = OBJ (y, -x, z), then each object's inverse world matrix (the local
   matrices of the mother chain, mother first, inverted as an affine matrix). Normals through the inverse linear part,
   normalized.
7. **Materials** are found by the export's MTL names (texture, or `untextured_<diffuse>`, + `_glow` / `_2sided` /
   `_smooth`, + `_vN`), then without `.001`, then without case; the template's material is used as it is (its colours,
   flags, texture). Its `map_Kd` in the OBJ's MTL decides the picture: the export's own PNG (or none) keeps the game's
   texture; a PNG of a game texture in the mesh's own folder takes that texture; **any other PNG inside the Assets
   folder is a new picture**: a texture `hwi_<file name>` in the mesh's folder (a number added if taken), loaded from that
   PNG with its `_teambase` / `_teamstripe` masks if they sit beside it (AF_New; transparent pixels make it an alpha
   texture; an opaque one loads as a repainted paletted texture does: RGB, coloured through its team masks). A PNG outside the Assets folder, or missing:
   the game's texture (logged). An unknown `usemtl` is a new material: its flags from the name's suffixes, smooth also
   when its faces have `s` on, `Kd` its colour (`Ka` or a quarter of `Kd` the ambient), `map_Kd` as above. Faces with no
   material: untextured grey.
8. **Faces:** triangles as they are; a polygon with more corners becomes a fan from its first corner (right for convex
   polygons; a concave one draws wrong). UV s = u, t = 1 - v. A flat material's face normal is the mean of its corners'
   OBJ normals (the export writes the face normal at all three), else from the triangle; a smooth material's vertices
   take the OBJ normal of the corner (a vertex whose corners have different normals is split); without OBJ normals,
   the area-weighted face normals around it. A `nan` normal (the game's own meshes have 796, on degenerate polygons,
   and the export writes them) counts as none.
9. **The per-vertex glow entries** (Cataclysm's `vertexinfo`): from the template vertex of the same number at the same
   place (the export's own order), else the one at the same place with the closest normal, else, for a moved vertex,
   the one of the same number when the OBJ object has as many vertices as the template object; otherwise zero.
10. **A bad OBJ never stops the game:** an unreadable or empty file, no faces, a number or index that is wrong, more
   than 65,535 vertices in an object or materials in the mesh, a face of more than 256 corners: `assets: model <OBJ>:
   <why>; the game's own mesh is used`, for that level and the levels it would have stood in for.

## What an edited model does not change, and what it does

Not changed (they are not in the OBJ): the collision box and sizes in the `.shp`, the `.mex` points (guns, engines,
docking, nav lights), the engine trails' positions, the animations' keyframes (the objects are found by name).

**Looks only (the user's ruling, 2026-11-02: "Looks only").** The simulation reads the mesh in two functions: hit
points (`cat_meshLineIntersect`: a bullet, beam or missile inside a ship's collision box hits only where its line meets
the mesh at level 3, `cat_collMeshHitDistance` / `Collision.c`, for every ship whose `.shp` leaves
`bUseRayTriangleIntersectionForHitLocation` on, 174 of 198; the ramming frigate's contact point at level 0,
`RammingFrigate.c`) and the repair droids' hull walk (object 0 of level 0, `RepairDroid.c`). Both now read
`meshSimulationMesh(mesh)` (`Mesh.c`): when `meshLoad` builds a mesh from an edited model it also loads the game's own
mesh of the same file beside it (the Assets folder suspended, no textures registered) and pairs the two; `meshFree`
frees the pair. So an edited model only changes what is drawn: the shots, the droids and every other piece of play
stay the original's, and multiplayer games and replays never depend on the Assets folder. On screen a much smaller
or larger model is still hit, and its droids still sit, where the original hull is. The per-ship vertex state for the
bullet-hit glow (`cat_univShipMeshStateCreate`) is drawing only and follows the drawn mesh. The other mesh reads of
the simulation side use part names only (the `.mex` nav lights and engine nozzles, the gun bindings), which an
edited model keeps.

| check (2026-11-02, after the ruling) | result |
|---|---|
| Mission01's scripted play-through (`--mode missiontest`, release, 11,424 ticks, with the Turanic raiders' attack) with every main OBJ shrunk to half size (706 edited) | before the change: **diverges at tick 1772** (rng10, the shots' draws); after: **IDENTICAL** to the unedited run (11,426 ticks); objectives 6/6 PASS in all three runs |
| the same under the debug ASan build | IDENTICAL to the unedited debug run; ASan 0, no leak (the pairs freed). (The debug and release streams of this play-through differ from tick 8739 with or without edits: a build difference, not this feature) |
| the 6000-tick Mission01 with every ship shrunk / with nothing edited (debug ASan) | both IDENTICAL to `m3r3i-e-mission-a`; ASan 0, no leak |
| the widened docking bay in the Kuun-Lan scene | the frame byte-identical to before the change (the drawn mesh is the edited one) |
| `/catAssetsModelTest` on the game's own export | 1,480 / 1,480 PASS, ASan 0 |

## Blender (5.2.3 LTS, headless, default import and export settings)

What Blender's round trip changes, measured on the Acolyte, the Carrier (main and in-game), the Kuun-Lan command
module and the Worker (an order-independent comparison of the image the engine builds with the game's mesh):
- object and material names: kept (no `.001` in a fresh scene; the importer strips them if they appear);
- faces: regrouped by material inside each object; every triangle found again; no winding flipped;
- positions: float32 in Blender, written with 6 decimals: within 1.5e-4 (Kuun-Lan coordinates of about 1,000 units);
  UVs within 2.4e-7; normals re-normalized, within 0.006; every glow entry found again;
- `s 1` on every face (ignored for known materials); `map_Kd` written as **absolute** paths (used as above; the
  pictures stay found as long as the folder is not moved); Blender's two default materials appear in the MTL, unused;
- **a face whose three corners another face of the object also has is dropped**: a two-sided sheet modelled as two
  faces back to back becomes one-sided (the Worker's hull has two: 577 -> 575 triangles). Give such a sheet the
  `_2sided` material (both sides drawn) or a thickness.

## Tests (2026-11-02, on 43b9734 + this branch; debug ASan build unless noted, offscreen driver, no input)

The ship scene is the texture feature's: the Kuun-Lan at its closest zoom (skirmish LiveFireExercise2,
`--scene mothership:2300 --shot 100`). Test folders are exports in the session scratchpad; edits made by headless
Blender (scale / widen) or by a script (bad files).

| test | result |
|---|---|
| builds | `tools/build.sh` (ASan/UBSan) and `--release`: 0 warnings; `tools/release_win64.sh`: links, 0 warnings |
| **HWI-TEST `/catAssetsModelTest`**: every line of `assets-models.txt` (740 levels x main and in-game = 1,480 builds of the 1,412 OBJs), the export's own files, compared polygon by polygon with the game's mesh | **1,480 / 1,480 PASS**: same objects (names, flags, links, matrices bit for bit), same 211,506 triangles in the same order, positions within 9.5e-7, UVs within 9.5e-7, normals within 8.5e-7 (796 NaN normals of the game's skipped), every material and every glow entry identical; 2.4 s under ASan; ASan 0, no leak |
| the same, Windows build under GE-Proton 10-34 (`umu-run`, hidden window) | 1,480 / 1,480 PASS; the untouched folder `active 0` (the Windows file-time check agrees with the manifest) |
| every model edited (the 1,412 OBJs touched) and every game mesh loaded (`--mode meshes`, 1,758 files through `meshLoad`) | 706 edited meshes built (1.4 ms each under ASan, 988 ms in all), `self-test done, 1758 files, 0 with bad objects`, ASan 0, no leak |
| the untouched export | frame byte-identical to `/noAssets` (Kuun-Lan scene and the Video_Options menu); `assets: models` line absent |
| Blender round trip (Acolyte, Carrier main + in-game, command module, Worker) | as above: all triangles matched except the Worker's two back-to-back duplicates |
| a visible edit: the docking bay module's OBJ widened 1.5x in Blender (`lod0` only) | `edited`, `also stands in for ... levels 1, 2, 3, 4`; 94,964 pixels change, all on the module (bbox 157,386-595,768) |
| the stand-in: the same edit, FULL DETAIL off, camera at 9,000 (`--cfg 'noLOD 0' --lodtest 100`: the bay drawn at level 2) | 8,479 pixels change, all on the module: the edited level 0 is drawn for level 2 |
| an `ingame/` OBJ edit (the same widening on the paged mesh, which draws from `page0`) | `replaces the paged mesh`; the frame is byte-identical to the main-model edit (the pages hold exact copies of the sources) |
| a new picture and a new material: `smo_dock_back`'s `map_Kd` -> `my_checker.png` beside the OBJ; a `usemtl hwi_red` (Kd 1 0 0) | `new picture ... (texture race_sect/smothershipdockingbay/rl0/lod0/hwi_my_checker)`, `assets: texture ... hwi_my_checker.lif 128x128 from png`; the back face shows the checker (8,422 pixels); same result on the Windows build |
| bad files: an index out of range, an empty file, 20 kB of random bytes, a `v nan nan nan`, renamed objects, CRLF + negative indices + a quad + a pentagon | `line 473: an index is out of range`, `no faces` (x2), `line 9: a number is expected`: each `the game's own mesh is used`; the renamed objects' faces go to the root (logged); the polygons: 579 faces -> 582 triangles; `--mode meshes` 0 bad objects, ASan 0 |
| the 6000-tick mission (`--mode mission --mission-ticks 6000`) with the untouched export, and with the docking bay edited (loaded for all five levels) | both IDENTICAL to `m3r3i-e-mission-a` (6001 ticks, 0 packets) |
| start-up (release, `--mode statics`, the untouched export, 3 runs) | the folder index 75 to 97 ms (69 to 80 ms before: + the model check); the whole start 1.81 to 1.83 s (1.83 to 1.85 s before) |

## The log (`assets:` lines, stdout)

```
assets: model Models/race_sect/smothershipdockingbay/smothershipdockingbay_lod0.obj edited: it replaces the mesh and its paged version (race_sect/smothershipdockingbay level 0)
assets: model ... also stands in for race_sect/smothershipdockingbay levels 1, 2, 3, 4 (no edited model of its own)
assets: models: 1 edited, 4 levels stand in
assets: model ...: new picture Models/.../my_checker.png (texture race_sect/smothershipdockingbay/rl0/lod0/hwi_my_checker)
assets: model ...: 1 objects with faces, 66 triangles from 66 faces, 109 vertices, 2 new materials, 1 new picture, glow from the game's mesh at 109 vertices, 1.5 ms
assets: model ... drawn for RACE_SECT/sMothershipDockingbay\Rl0\LOD0\sMothershipDockingbay.peo
assets: model ...: line 473: an index is out of range; the game's own mesh is used
```

## Not done

- No in-game reload: an edit shows at the next start (as the textures).
- The `.shp` / `.mex` data that follows the shape (collision sizes, gun and engine points) is not derived from the
  edited mesh.
- An `ingame/` edit is drawn without Paco's 0.6-texel seam nudge (the image is `RMF97ba`; the nudge data is per
  polygon of the original paged mesh).
- Multiplayer does not check that both players have the same models (see "What an edited model ... does").
