<!-- done: layout, loader, colour, meshanim, selftest, runs -->
# M2-R2 port: Cataclysm mesh loader (producer 1)

Status: COMPLETE (2026-10-10), not committed. Branch `m2`, worktree `.claude/worktrees/m2`. Scope:
docs/m2-r1-summary.md "M2-R2 plan", producer 1. Spec: docs/formats/mesh.md (now with an "M2-R2 findings" section);
probe: tools/formats/mesh_probe.py. Subsystems, in done-marker order: `layout`, `loader`, `colour`, `meshanim`,
`selftest`, `runs`.

**Result.**
- `src/` loads Cataclysm's meshes as the exe does:
  - 0x74-byte polygon objects, with the new per-vertex info list at +0x1C;
  - no version check (the exe never reads the stamp);
  - `.lod` files that name `.peo` load unchanged;
  - shared lists stay shared in GoK's 64-bit conversion, which is rewritten.
- The per-vertex info is wired into the colour path: `shColourSet0` with Cataclysm's three arguments,
  `cat_shColourAddGlow`, the `enableCPV` option and its `Homeworld.cfg` key and `/noCPV` switch, and the per-vertex
  calls in `meshObjectRender`/`meshObjectRenderLit`. The per-ship runtime glow state is stage 2 and NULL here.
- MeshAnim needed no change. The 29 `.mad` files and `madFileLoad` are HW1's.
- **Self-test.** `/catMeshLoadTest <list>` (off the default start-up path) loads every `.geo`/`.peo`/`.mad` of the
  install: **1,758 of 1,758 load, 0 bad, 0 ASan errors**, and all 1,758 match the files count for count
  (`tools/formats/mesh_engine_check.py`).
- **Start-up.** On an isolation tree whose meshes are the unconverted originals, the engine loads the 21 engine-glow
  meshes and reaches `Main_game_screen` with 0 ASan errors. The HEAD engine stops at `Mesh.c (917): meshLoad:
  Invalid version #` on the same tree.
- Side effect for ship statics: the `/catShipStaticsTest` self-test now runs on the original meshes, with no variant
  tree and no `/catShipStaticsSkip`. 143/143 ship types parse as the probe does, `p1Outpost` included, and 27 other
  `.shp` load (was 24): the 3 derelicts load with 3 provisional derelict types.
- `tools/build.sh --debug --clean` and `--release --clean`: rc 0, `[292/292] Linking target hwi-engine`, 0 warnings.
  `tools/m2_layout32.sh`: OK, ILP32 `polygonobject` 116 bytes.
- **Plain harness (the gate):** the first stop is still `Unable to open file: HW_Music.wxd` (data-deltas rank 9,
  producer 4's item), which comes before any mesh is loaded. The mesh stop can therefore be shown passed only on
  isolation trees so far (see `runs`).

**DONE WHEN.**

| criterion | state | evidence |
|---|---|---|
| all 1,729 `.geo`/`.peo` load in the engine with no ASan report (loader test mode, not on the default start-up path) | **met**: 1,729/1,729, plus the 29 `.mad` files; 0 objects with a bad index; 0 ASan reports; no UBSan report from mesh or shader code | run `mesh-r10-meshes-iso`; `mesh_engine_check.py`: `summary: 1758 files on disk, 1758 match, 0 mismatch, 0 not loaded, 0 extra` |
| the 21 engine-glow meshes load at start-up | **met** on the isolation tree: `FileLoads.log` lists the 21 `misc\EngineGlows\*.geo` loads (`energybolt`, `engine_glow00-03`, `engine_glow_frigate00-03`, `r1moship0-3`, `r2moship0-3`, `p1moship0-3`), all from the unconverted files, then `feScreenPush: pushed screen 1` | runs `mesh-r1-fe-iso`, `mesh-r11-fe-iso` |
| the harness gets past the mesh stop without the down-conversion | **met on the isolation harness, not yet visible on the plain gate**. The `/ignoreBigfiles` tree has stage 15 (the down-conversion) undone and all 1,729 meshes linked to the originals; it gets past the mesh stop to `Main_game_screen`. The plain gate stops earlier, at `HW_Music.wxd` (producer 4). An archive-based tree with only the stage 0-2 renames stops at the front-end art rename (rank 10, also producer 4), which comes before the engine-trail module | runs `mesh-r0-head-fe-iso` (before: mesh stop), `mesh-r1-fe-iso`/`mesh-r11-fe-iso` (after), `mesh-gate-{fe,sp,meshes}`, `mesh-r8-fe-bigiso` |

## Subsystem `layout`

`src/Game/Mesh.h`

| change | exe evidence |
|---|---|
| new `vertexinfo` (16 B: `colourOffset`, `flags`, `reserved[2]`; names ours) | `meshObjectRender` 0x004b0170 passes `+0x1C[iV]` (stride 0x10) to `shColourSet0` 0x0050dfe0 (disassembly 0x4b045e-0x4b0470); `cat_shColourAddGlow` 0x0050d7c0 reads dword 0 as packed signed RGB |
| `polygonobject` gets `vertexinfo *pVertexInfo` between `pVertexList` and `pNormalList`; `polygonobject_disk` gets `udword pVertexInfo` at +0x1C, so the record is 0x74 and `pNormalList`/`pPolygonList`/links/`localMatrix` move by 4 | `meshLoad` 0x004aec90 loop stride `ppnVar16 += 0x1d` dwords, four unconditional fix-ups at +0x18..+0x24, three conditional at +0x28..+0x30 |
| static asserts: `GeoFileHeader_disk` 0x44, `polygonobject_disk` 0x74 (offsets +0x18, +0x1C, +0x20, +0x24, +0x28, +0x2C, +0x30, +0x34), `materialentry_disk` 0x20, `vertexentry`/`vertexinfo`/`normalentry` 0x10, `polyentry` 0x28; ILP32 only (`__i386__`): in-memory `polygonobject` 0x74, `meshdata.object` at 0x20, `materialentry` 0x20 with `texture` at 0x14 | `meshLoad` reads 0x44 bytes, allocates `fileSize - 0x24` (= file - 0x44 header + 0x20 `meshdata` head), material stride 0x20 |

`tools/m2_layout32.sh` now also includes `Mesh.h` and prints the ILP32 `polygonobject` size:
`ILP32 sizes (ShipStaticInfo GunStatic CustShipHeader polygonobject): char (*)[1112] char (*)[284] char (*)[140] char (*)[116]`,
`m2_layout32: OK`.

The field meanings were re-checked on all 1,729 files with a scratch script
(`original/scratch-m2/m2r2-mesh-tools/meshcheck.py`, outside git): with +0x18 = vertices, +0x1C = vertex info,
+0x20 = normals, +0x24 = polygons, every polygon's vertex/face-normal/material index and every vertex's normal index
is in range in all 4,563 objects, every normal is unit length, every object has a name, and dwords 2-3 of every
vertex-info entry are 0. Correction to docs/m2-r1-data-deltas.md 2.2: the "extra udword at record offset 0x24"
that points "right after the material table" is the polygon list (polygon lists follow the material table, as
docs/formats/mesh.md says); the new field is the vertex-info pointer at +0x1C.

## Subsystem `loader`

`src/Game/Mesh.c`

| function | change | exe evidence |
|---|---|---|
| `meshLoadAndConvertTo64Bit` (GoK's 64-bit helper, not in the exe) | rewritten. Reads objects at the 0x74 stride and translates the new +0x1C pointer. Everything behind the material table (vertex, vertex-info, normal and polygon lists, names) is copied as one verbatim block and every offset into it is translated, so duplicate objects keep sharing their lists as in the exe (400 of the 1,729 files share at least one list). The image size is computed (GoK: `3 * file length`, copying each object's lists separately, which can overflow when many objects share a list; this is the likely cause of the `p1Outpost` pool corruption seen in docs/m2-r2-shipstatics.md). Material names are kept (GoK dropped them; the exe fixes them up). Bad data (offsets outside the data area, wrong object stride, lists past EOF, links off an object boundary) is a fatal error naming the file, where the exe would misparse silently. GoK's stale-`loopsize` bug for materials without a texture is gone with the rewrite | `meshLoad` 0x004aec90: `publicMaterial = object + oPublicMaterial - 0x44`, `localMaterial = object + oLocalMaterial - 0x44`, material names fixed up when non-zero |
| `meshLoad` | `pVertexInfo` is fixed up unconditionally, like the other three lists (`FIX_ENDIAN` path: its pointer is swapped too). The HW1 version check (`MESH_ERROR_CHECKING`, i.e. our debug build: `Expected 0x402, found 0x500`) is removed; the ident check stays (debug only, all files pass) | `meshLoad` never reads the version dword; 1,727 files say 0x500 and 2 say 0x402 with the 0x74 layout |
| `.peo` paging (`meshPagedName`, `meshPagedVersionExists`) | unchanged. A `.lod` that names `.peo` directly (`race_sect/scarrier.lod`: `pMeshFile0 sCarrier\Rl0\LOD0\sCarrier.peo`) maps to itself (the `'p'` store replaces the first extension letter) | `meshLoad` inlines the same logic (`pbVar7[~uVar13 - 4] = 0x70`); `mainAllowPacking` 0x008b8614 = 1 in `.data`, no other reference to it in the code |
| `meshObjectFixupUV`, `meshFixupUV`, `meshFixupPacoUV` | unchanged: the exe reads polygons at +0x24, which is what `pPolygonList` now is | `meshObjectFixupUV` 0x004af220 (0.51 / `scaledWidth`, handle != -1), `meshObjectFixupPacoUV` 0x004aeb10 (IDENTICAL; named `meshFixupPacoUV` before the M2-R3 name fix, the meshdata-level loop is inlined in its callers) |

Two open points of docs/formats/mesh.md are settled from the exe:
- `mainOnlyPacking` (0x00a74d90) has one reference, the read in `meshLoad`, and `.data` holds 0. The exe has no
  `/onlyPacking`, `/disablePacking` or `/allowPacking` string, so paging is always on and "only packing" always off
  in Cataclysm. GoK's three switches stay (debug options, not in the exe).
- The version stamp carries no meaning for the loader (above).

`src/Game/ObjTypes.h`/`.c` (made necessary by the loader test): `meshNameToStaticInfo` 0x004ae7b0 (IDENTICAL) looks
a `Derelicts\<type>\...` mesh up in the derelict type table. HW1's table has no `Galaxy`, `LavaBall` or `MattWorld`,
so loading their 11 mesh files asserted (`Mesh.c (257): Assertion of (type != 0xffffffff)`, run `mesh-r2-meshes-iso`)
and then read `derelictStaticInfos[-1]`. The three names are appended after `HyperspaceGate`, marked
`CAT-PROVISIONAL`: the exe's table 0x00889b68 has 8 types (`Crate` 0, `Homeworld` 1, `PlanetOfOrigin` 2,
`PlanetOfOrigin_scarred` 3, `Galaxy` 4, `LavaBall` 5, `MattWorld` 6, `HyperspaceGate` 7), and renumbering HW1's
55-entry enum to it stays the open derelict-table item of docs/m2-r2-shipstatics.md. The tables sized by
`NUM_DERELICTTYPES` (`derelictStaticInfos`, the two ETG tables, `smDerelictTypeMesh`) get zero entries for the three.

## Subsystem `selftest`

A loader test mode, off the default start-up path:
- `src/SDL/main.c`: `/catMeshLoadTest <listfile>` (port diagnostic, `CatMeshLoadTestSet`).
- `src/SDL/utility.c`: after `SS2_SystemStarted` (so the normal start-up, including the 21 engine-glow meshes, has
  run), `catMeshRunSelfTest()` then quit, like `/catShipStaticsTest`.
- `src/Game/Mesh.c` `catMeshRunSelfTest`: loads every path of the list through `meshLoad` with paging off (so each
  named `.geo` is itself loaded, not its `.peo`), checks every polygon's vertex, face-normal and material index, every
  vertex's normal index, the hierarchy links, `iObject` and each `m44` (`catMeshSelfTestCheck`), logs
  `meshtest: loaded <path> ident= objects= materials= verts= polys= norms= glowverts= textures= bad=` and frees the
  mesh. `glowverts` counts vertex-info entries with a non-zero colour offset; `textures` counts materials with
  registered textures (ship meshes register none here, because no ship static, and so no team-colour list, is
  loaded in this mode).
- `tools/m2_harness.sh --mode meshes`: writes `<run>/meshes.list` from the `cataclysm.big` TOC
  (`tools/bigfile.py list`, 1,729 entries; `update.big` has no mesh) or, for an `/ignoreBigfiles` `--data` tree, from
  the tree's files, and passes `/catMeshLoadTest`; the summary prints listed/loaded/bad counts and the number of
  `misc\EngineGlows` loads in `FileLoads.log`. New milestone `mesh self-test done`.
- `tools/m2_harness_iso.sh`: stage `15` can be undone (every `.geo`/`.peo` of the isolation tree points at the
  original 0x74 file again).
- `tools/formats/mesh_engine_check.py --data DIR [--update DIR] RUN.log`: compares each logged line with the file
  (ident, objects, materials, vertices, polygons, normals, glow vertices; `bad=0`).

Result (run `mesh-r3-meshes-iso`, isolation tree with stages 3-8, 13, 14 and 15 undone): **1,729 listed, 1,729
loaded, 0 with bad objects, 0 ASan errors**; `mesh_engine_check.py`: `summary: 1729 files on disk, 1729 match,
0 mismatch, 0 not loaded, 0 extra`. Totals over the 1,729 files: 3,930 glow vertices in 324 files; 918 materials
with registered textures.

## Subsystem `colour`

The per-vertex info list is wired into the colour path the way the exe uses it.

| file / function | change | exe evidence |
|---|---|---|
| `src/Game/Shader.c` `shColourSet0` (MODIFIED in the delta map) | new signature `(vector *norm, real32 *vertexState, udword *vertexInfo)`. The lit sum is converted to bytes with FAST_TO_INT and clamped to 0..255 at both ends (HW1 clamped only the top), the vertex's own colour is added when an info entry is given, and with the `lighting` toggle clear the vertex is drawn 0x80 grey plus that colour at alpha 0xFF. Then `glColor4ub`. The sums stay in GoK's [0, 1] scale and are multiplied by 255 before the conversion, so the bytes equal the exe's 0..255 sums (render-only, not part of any checksum) | `shColourSet0` 0x0050dfe0: the `+ 1.5*2^52` double at 0x0072a458 read back as an int (FAST_TO_INT), the `< 0` / `< 0x100` tests, the two `cat_shColourAddGlow` calls, `DAT_0087816d == 0` branch with 0x80 and alpha 0xFF; alpha byte `DAT_00a5b618` = FAST_TO_INT in `shUpdateLighting` 0x0050dc60 |
| `src/Game/Shader.c` `cat_shColourAddGlow` (new, cat-new) | adds the info entry's packed signed R, G, B (bytes 0..2 as `signed char`) when its low 24 bits are non-zero, clamped to 0..255; only when the `cbv` toggle is set. A non-NULL vertex state is a fatal error: the runtime glow (`cat_shGlowUpdate`) needs the per-ship mesh state, which is stage 2, and no caller passes one | `cat_shColourAddGlow` 0x0050d7c0 |
| `src/Game/Shader.c` `catShColourByVertex`, `catShLighting` (new) | the two debug toggles, both TRUE | console command table rows `cbv` (0x00878560 → byte 0x0087816c) and `lighting` (0x00878574 → 0x0087816d); both bytes are 1 in `.data`, and the only code references are the reads at 0x0050d7c0 and 0x0050e21c |
| `src/Game/Shader.h` | prototypes and externs | |
| `src/SDL/main.c`, `src/SDL/utility.h` | `sdword enableCPV = FALSE` (the exe's `.bss` default) and the switch `/noCPV` (" - do not use vertex CPV.") | `enableCPV` 0x00c52378 (the exe's name is the `Homeworld.cfg` key); command-line table row 0x008b8a80 (`/noCPV`, value 0) |
| `src/SDL/utility.c` | `{"enableCPV", scriptSetUdwordCB, &enableCPV}` after `enableSmoothing` in the saved-options table | options table 0x008c14c0 (`noLOD`, `enableSmoothing`, `enableCPV`, `enableLensFlare`); the user's `original/game/Cataclysm.cfg` has `enableCPV    1` |
| `src/Game/Mesh.c` `meshObjectRender` (MODIFIED) | with `enableCPV` set, each vertex gets `shColourSet0(normal, state, &pVertexInfo[iV])` before its `glVertex3fv`, in all four poly modes | 0x004b0170, disassembly 0x4b0440-0x4b0475 (`test [0xc52378]`, then `shColourSet0(normal, state ? state + iV*0x28 : 0, vinfo + iV*0x10)`) |
| `src/Game/Mesh.c` `meshObjectRenderLit` (MODIFIED) | flat and textured: with `enableCPV` the first vertex gets its own colour, and so do the second and third (without it, one HW1 call per polygon with neither, as before); smooth modes: every vertex is lit with its info entry and state when `enableCPV` is set, with neither when not (`meshColourLit`) | 0x004b0970, disassembly 0x4b0af2-0x4b0bf5 (flat: `push 0; push 0` when the option is off, extra calls for vertices 1 and 2 only when on), 0x4b0d06-0x4b0d57 (smooth) |
| `src/Game/Mesh.c` macros | `meshColour` passes `NULL, NULL`; `meshColourCPV`, `meshColourLit`; `MESH_VERTEX_STATE(iV)` is NULL | the exe's state pointer is the new first argument of both render functions (`[esp+0x30]` / `[esp+0x24]` after the prologue), created per ship by `cat_univShipMeshStateCreate` 0x00541cd0 |

The colour path runs in the loader self-test: `catMeshSelfTestCheck` calls `shColourSet0(vertex normal, NULL, &info)`
for every vertex of every mesh (run `mesh-r4-meshes-iso`: 1,729 files, 0 ASan errors, no UBSan report from
`Mesh.c` or `Shader.c`). The renderer's use of it shows only once ships render (stage 2/3).

Not ported here (render, stage 3): the rest of the MODIFIED render functions (their new first parameter,
`cat_meshCurrentMaterialAlt`, `meshSpecObjectRender`, `meshMorphedObjectRender`, `meshObjectRenderR`,
`meshObjectRenderHierarchy`), and `shSpecularColour` 0x0050e280, which also calls `cat_shColourAddGlow` (7 → 9
parameters). The video-options toggles `cat_utyDynamicCPVToggle` 0x005a8cb0 / `cat_utyDynamicCPVBitmapToggle`
0x005a8d40 (`VO_DynamicCPV`), the low-memory override in `utyGameSystemsInit` (below 128 MB: `enableCPV = 0`) and
`cat_opSetAllDefaults` 0x004db7b0 (1 on high detail, 0 on low) are front-end items (stage 3).

## Subsystem `meshanim`

No source change was needed to load Cataclysm's mesh animations; the self-test now proves it.

- **Format.** All 29 `.mad` files are `Mellonn` version 0.91, at most 7 animations (`MAD_MaxAnimations` 32), and
  every object path stores its keyframe times, TCB parameters and the six motion arrays contiguously in the order
  GoK's `madFileLoadAndConvertTo64Bit` assumes (scratch survey of all 29; `update.big` has none).
- **`madFileLoad` 0x004b65c0** reads the 0x28-byte header, allocates `fileSize - stringBlockLength`, copies the
  header, reads the rest, reads the string block into its own allocation and fixes up the animation names, the
  object paths (name + CRC16, times, parameters, 6 paths, stride 0x30): statement for statement HW1's
  `meshanim.c:55` (ORIG, `src/reference/hw1-ad1bb82/Game/meshanim.c`), minus the `MAD_ERROR_CHECKING` block that a
  release build drops. The delta map's MODIFIED (high) for it rests on a dropped `"StatusDump.txt"` string, which
  belongs to `KAS.c` (`src/Game/KAS.c:797`), and on field-name noise; it should read IDENTICAL.
- The debug build keeps HW1's identifier/version/animation-count checks; every shipped file passes them.
- The runtime MeshAnim changes are stage 2 (ship animation): `madAnimationStart` (2 → 3 parameters),
  `madAnimationUpdate` (sound events, `madAnimationPause`), `madAnimationStop`, `madAnimIndexFindByName` (no fatal on a
  missing name, returns -1), `madBindingListCallback` (binding stride 0x58), `madAnimBindingsDupe`
  (`cat_univMeshBindingsLinkParents`), plus `meshBindingListRecurse`/`meshBindingListCreate`/`meshBindingListDelete`
  (parent binding index). They change `mhlocalbinding` (0x54 → 0x58) and are not ported here.

Self-test: `/catMeshLoadTest` loads a `.mad` path through `madFileLoad` (`catMeshSelfTestMad`: names, CRCs, at least
one keyframe, times in order, animation names; one `meshtest: mad loaded <path> objects= animations= keyframes= bad=`
line) and frees it with `madHeaderDelete`. Run `mesh-r5-meshes-iso`: 29/29 loaded, 0 bad, 0 ASan errors;
`mesh_engine_check.py` compares objects, animations and summed keyframes with the files.

## Subsystem `runs`

Harness: `tools/m2_harness.sh` (this item adds `--mode meshes` and the `mesh self-test done` milestone),
`tools/m2_harness_iso.sh` (adds stage 15). Isolation tree `m2r2-harness/iso-mesh` =
`m2_harness_iso.sh iso-mesh 3,4,5,6,7,8,13,14,15`. It is the data-deltas `/ignoreBigfiles` tree with the
identifier/statics stages undone (as in the earlier producers' runs) and **every mesh pointing at the original
Cataclysm file**. Its remaining workarounds are producer 4's: stages 0-2 renames, 9-12. Runs use
`/ignoreBigfiles /noSpeech`. `m2r2-harness/iso-mesh-big` = the plain harness data plus the stage 0-2 renames as
symlinks (archives, no `/ignoreBigfiles`).

The exit "first stop" `utility.c (3463/3464): Assertion of (result != -1)` in the isolation runs comes at quit,
after the timeout or the self-test: GoK's `utyGrowthHeapFree` calls `munmap(heap, 0)`, a known GoK bug
(docs/m2-r2-shipstatics.md "What is left" 9).

| run | binary | data / mode | milestone | first stop | notes |
|---|---|---|---|---|---|
| `mesh-r0-head-fe-iso` | HEAD (`build/tmp-mesh/head`, debug) | iso-mesh, fe | engine trail module started | `Mesh.c (917): meshLoad: Invalid version # misc\EngineGlows\energybolt.geo. Expected 0x402, found 0x500` | baseline: data-deltas run16's mesh stop, reproduced on the original meshes (a first attempt with a relative `--bin` path did not start, rc 127) |
| `mesh-r1-fe-iso` | port | iso-mesh, fe, 40 s | `Main_game_screen` | quit assertion (GoK) | 21 engine-glow `.geo` loads, 0 ASan |
| `mesh-r2-meshes-iso` | port | iso-mesh, meshes | `Main_game_screen` | `Mesh.c (257): Assertion of (type != 0xffffffff)` | `derelicts\galaxy\...`: no `Galaxy` in HW1's derelict table, so the 3 provisional types were added |
| `mesh-r3-meshes-iso` | port | iso-mesh, meshes | mesh self-test done | quit assertion | 1,729/1,729 meshes, 0 bad, 0 ASan |
| `mesh-r4-meshes-iso` | port + colour path | iso-mesh, meshes | mesh self-test done | quit assertion | same, with `shColourSet0` run per vertex |
| `mesh-r5-meshes-iso` | port + `.mad` | iso-mesh, meshes | mesh self-test done | quit assertion | 1,758 listed: 1,729 meshes + 29 `.mad`, 0 bad, 0 ASan |
| `mesh-r6-statics-iso` | port | iso-mesh, statics | ship statics self-test done | quit assertion | 143/143 ship types as the probe (`shipstatic_engine_check.py`), 27 other `.shp`; 776 mesh loads through `.lod` (750 `.peo`, 26 `.geo`), `p1Outpost` included |
| `mesh-gate-fe`, `mesh-gate-sp`, `mesh-gate-meshes` | port | **plain harness** | process started | `Unable to open file: HW_Music.wxd` | the gate; producer 4's rank 9 comes before any mesh. `meshes.list` from the `cataclysm.big` TOC: 1,758 entries |
| `mesh-r7-fe-bigiso` | port | iso-mesh-big (stage 0+1), fe | process started | `Unable to open file: HW_comp.vce` | rank 2 (producer 4), even with `/noSpeech` |
| `mesh-r8-fe-bigiso` | port | iso-mesh-big (stage 0-2), fe | front end module started | `fileSizeGet: can't find file .../FEMan/Construction_Manager/Build_race1_icon1.lif` | rank 10 (producer 4), before the engine-trail module |
| `mesh-r9-sp-iso` | port | iso-mesh, sp | level load (mission file read) | `GenericInterceptor.c (205)` (stage 2, as in the statics item's run) | 31 mesh loads before the stop |
| `mesh-r10-meshes-iso` | final clean debug build | iso-mesh, meshes | mesh self-test done | quit assertion | **1,758/1,758, 0 bad, 0 ASan; `mesh_engine_check.py` 1,758 match** |
| `mesh-r11-fe-iso` | final clean debug build | iso-mesh, fe, 40 s | `Main_game_screen` | quit assertion | **21 engine-glow meshes**, then `feScreenPush: pushed screen 1`, idles to the timeout, clean module shutdown |

Run directories: `original/scratch-m2/m2r2-harness/runs/<label>/` (run.log, FileLoads.log, summary.txt, meshes.list,
ubsan.*), one line each in `runs/runs.txt`. No ASan report file was written by any run of this item.
UBSan, in all port runs: only the known misaligned `fescreen`/`PATCH` accesses and one null `MUSICHEADER`
member access in `SpeechEvent.c:2666`. That last one is the music item (producer 4) and appears with `/noSpeech`
on the isolation trees. Nothing comes from `Mesh.c`, `Shader.c` or `MeshAnim.c`.

## What is left for later stages

1. **Plain gate.** The mesh stop shows as passed on the gate once producer 4's file items (ranks 9, 2, 10, 11, 12:
   music, speech, FE art, battle chatter) are in. Re-run `tools/m2_harness.sh <label> --mode fe` and
   `--mode meshes` on the plain harness then: both modes should reach the engine-trail module and the self-test
   unchanged.
2. **Derelict type table (identifiers/statics).** `Galaxy`, `LavaBall`, `MattWorld` are appended to HW1's 55-type
   enum (`CAT-PROVISIONAL`). The exe's table 0x00889b68 has 8 types with other values; renumbering to it, and
   retiring HW1's derelicts across `Sensors.c`, `ETG.c`, `LevelLoad.c`, `ShipSelect.c` and the HW1 missions, is still
   open (docs/m2-r2-shipstatics.md "What is left" 3).
3. **Per-vertex runtime state (stage 2).** `cat_univShipMeshStateCreate` 0x00541cd0 (0x28 bytes per vertex and LOD),
   `cat_shGlowUpdate` 0x0050d560 (decay by `universe.totaltimeelapsed`, team colour per trail bit 0x10<<i),
   `cat_meshApplyBulletHit` 0x004b52d0 and `cat_meshObjectForVerticesInRadius` 0x004b5190 (bullet-hit glow), and the
   state argument of `meshObjectRender`/`meshObjectRenderLit` (`MESH_VERTEX_STATE` is NULL; `cat_shColourAddGlow`
   stops with a fatal error if a state is passed before then). The meaning of `vertexinfo.flags` bits 0x01/0x02
   (hit glow) and 0x10<<i (trail i team colour) comes from `cat_shGlowUpdate`; it is not verified at runtime.
4. **Render (stage 3).** The rest of the MODIFIED mesh render functions: the new first parameter;
   `cat_meshCurrentMaterialAlt` 0x004afcb0 with its `0x00c32c08..0a` mode bytes; `meshSpecObjectRender`
   0x004b1d40; `meshMorphedObjectRender` 0x004b34f0; `meshObjectRenderR` 0x004b4110 with
   `cat_meshObjectRenderLitImmediate`/`LitTransformed`; `meshObjectRenderHierarchy` 0x004b4370 (5th parameter);
   `meshSetFade` 0x004ae700 (per slot); `meshSpecColour`/`shSpecularColour` 0x0050e280 (9 parameters, glow);
   `cat_meshObjectRenderEdgeEffect` 0x004b4a60. Also the `VO_DynamicCPV` toggles, the low-memory `enableCPV`
   override in `utyGameSystemsInit` and `cat_opSetAllDefaults`.
5. **Texture registration.** `meshTextureRegisterAllPlayers` 0x004ae910 passes the new 4th `trTextureRegister`
   argument (1). This is producer 4's texreg item.
6. **MeshAnim runtime and bindings (stage 2).** See `meshanim`: `mhlocalbinding` 0x58, the parent binding index, the
   animation start/stop/pause changes.
7. **MEX.** `mexLoad` and the `mexGet*` functions are MODIFIED (salvage dock chunks, collision info). They were not
   part of this item (docs/m2-r2-shipstatics.md "What is left" 2).
8. **Delta map.** `madFileLoad` 0x004b65c0 should be IDENTICAL (see `meshanim`). `meshLoad` stays IDENTICAL in
   source terms: only the struct changed. [audit: not so. HW1's meshLoad (`src/reference/hw1-ad1bb82/Game/mesh.c`
   :688-690) fixes up three lists unconditionally; the exe's 0x004aec90 fixes up four (+0x18..+0x24), so the port adds
   a statement (`pVertexInfo`) besides the struct change. The delta map's IDENTICAL (medium) for `meshLoad` should
   read MODIFIED (one added fix-up; the version check is debug-only in HW1 and absent from the exe either way).
   docs/audits/m2-r2-mesh.md]
9. **GoK quit bug.** `utyGrowthHeapFree` `munmap(heap, 0)` (every isolation run ends with it).

## Files changed

| file | what |
|---|---|
| `src/Game/Mesh.h` | `vertexinfo`; `pVertexInfo` in `polygonobject` and `polygonobject_disk` (0x74); layout asserts; self-test declarations |
| `src/Game/Mesh.c` | `meshLoadAndConvertTo64Bit` rewritten (+ `meshConvertOffset64`); `meshLoad` (`pVertexInfo` fix-up, no version check); per-vertex colour in `meshObjectRender`/`meshObjectRenderLit` (`meshColour`, `meshColourCPV`, `meshColourLit`, `MESH_VERTEX_STATE`); `catMeshRunSelfTest`, `catMeshSelfTestCheck`, `catMeshSelfTestMad` |
| `src/Game/Shader.c`, `Shader.h` | `shColourSet0` (3 arguments, byte colour, glow, lighting toggle); `cat_shColourAddGlow`; `catShColourByVertex`, `catShLighting` |
| `src/Game/ObjTypes.h`, `ObjTypes.c` | `Galaxy`, `LavaBall`, `MattWorld` derelict types (`CAT-PROVISIONAL`) |
| `src/SDL/main.c` | `enableCPV`, `/noCPV`, `/catMeshLoadTest` (`CatMeshLoadTestSet`), `#include "Mesh.h"` |
| `src/SDL/utility.c`, `utility.h` | `enableCPV` in the saved-options table; the self-test call after `SS2_SystemStarted`; extern |
| `tools/m2_harness.sh` | `--mode meshes`, mesh milestone and summary lines |
| `tools/m2_harness_iso.sh` | stage 15 undo |
| `tools/m2_layout32.sh` | includes `Mesh.h`, prints the `polygonobject` size |
| `tools/formats/mesh_engine_check.py` (new) | engine log against the files, per file |
| `docs/formats/mesh.md` | "M2-R2 findings" section |
| `docs/m2-r2-mesh.md` (new) | this report |

Scratch helpers (outside git): `original/scratch-m2/m2r2-mesh-tools/` (`pe.py` copied from the identifiers item,
`dc.py` decompile lookup, `meshcheck.py` semantic check, `splice.py`, `exe.dis` objdump of `.text`). The HEAD
baseline build is in `build/tmp-mesh/head/` (gitignored).

## Counts

- Diff: 12 tracked files, +597 / -212 lines before this report and the spec addendum; 74 added lines carry `CAT`.
- Static asserts in `Mesh.h`: 7 sizes and 9 offsets always, plus 2 sizes and 5 offsets on ILP32 (`__i386__`).
- Self-test coverage: 1,729/1,729 `.geo`/`.peo` (967 `.geo`, 762 `.peo`) and 29/29 `.mad`; 4,563 polygon objects;
  3,930 vertices with a colour offset in 324 files.
- Start-up: 21/21 engine-glow meshes. Ship statics on original meshes: 143/143 types, 776 LOD mesh loads.
- Harness runs: 16 (3 plain gate, 13 isolation including the HEAD baseline and one rc-127 false start).
