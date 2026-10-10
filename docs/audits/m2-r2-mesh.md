# Audit: M2-R2 port "mesh" (producer 1)

Audited: `docs/m2-r2-mesh.md`, the "M2-R2 findings" addendum in `docs/formats/mesh.md`, and the uncommitted diff of
branch `m2` against HEAD da09c2f: 13 tracked files, +613/-212 before the audit, plus the new
`tools/formats/mesh_engine_check.py`. Date 2026-10-10. Ghidra was not run.

Evidence came from these sources:
- Code: `ghidra/exports/cataclysm_gog_r5/decompiled.c` (`meshLoad` 0x004aec90, `meshObjectRender` 0x004b0170,
  `cat_shColourAddGlow` 0x0050d7c0, `shColourSet0` 0x0050dfe0, `shUpdateLighting` 0x0050dc60), `functions.csv` and
  `decompiled_index.csv`.
- Disassembly: the `objdump` of `.text` in `original/scratch-m2/m2r2-mesh-tools/exe.dis`. I read the instructions
  myself and did not use the producer's scripts.
- Exe data: `original/game/cataclysm.exe`, read with the audit's own section reader `/tmp/audit-mesh/exedata.py`.
- Mesh files: all of `original/game_extracted/`, read with the audit's own scripts in `/tmp/audit-mesh/`
  (`treecheck.py`, `counts.py`, `mats.py`, `vinfo.py`, `pick10.py`, `hunks.py`). None of them is committed.
- Harness runs: `original/scratch-m2/m2r2-harness/runs/audit-mesh-*`, on a fresh isolation tree `iso-audit-mesh`.

## 1. Build from clean

- I deleted `original/scratch-m2/build-audit-mesh` and ran `meson setup` there with the arguments of
  `tools/build.sh`. The debug build used `--buildtype=debug -Db_sanitize=address,undefined` and the release build
  `--buildtype=release -Db_sanitize=none`. Then I ran `meson compile` in each. meson.build sets `warning_level=3`
  and `werror=true`.
- Both builds finished with rc 0 and `[292/292] Linking target hwi-engine`. Neither log has a compiler `warning:`
  or `error:` line. The only WARNING is meson's own notice that sanitizers are on.
- Sizes:

  | build | file size | text | data | bss |
  |---|---:|---:|---:|---:|
  | debug | 75,428,504 B | 41,329,940 | 23,961,928 | 2,528,928 |
  | release | 5,194,952 B | 4,215,309 | 248,080 | 1,989,664 |

- After the audit's fixes, which change comments only, I rebuilt with `tools/build.sh --debug` and `--release` in
  the worktree. Both returned rc 0 with 0 compiler warnings. `tools/m2_layout32.sh` printed `OK`, with ILP32
  `polygonobject` at 116 bytes.

## 2. Layout and pointer fix-ups against the exe

### 2a. `meshLoad` 0x004aec90 (decompiled.c) against `src/Game/Mesh.h` and `src/Game/Mesh.c`

| # | claim | evidence | verdict |
|---:|---|---|---|
| 1 | the header is 0x44 bytes (`GeoFileHeader_disk`, static assert) | `fileBlockRead(handle, local_d44, 0x44)` | correct |
| 2 | header fields: `pName` +0x0C, `nPublicMaterials` +0x18, `nLocalMaterials` +0x1C, `oPublicMaterial` +0x20, `oLocalMaterial` +0x24, `nPolygonObjects` +0x28 | locals `d38`, `d2c`, `d28`, `d24`, `d20` and `d1c` relative to `d44`; the struct in Mesh.h has the same order | correct |
| 3 | the `meshdata` head is 0x20 bytes (ILP32 assert `offsetof(meshdata, object) == 0x20`) | `memAllocFunction(sVar9 + -0x24)` = file − 0x44 + 0x20; `m2_layout32.sh` OK | correct |
| 4 | polygon objects have a stride of 0x74 (`polygonobject_disk`; ILP32 `polygonobject` 116) | `ppnVar16 = ppnVar16 + 0x1d` dwords; `m2_layout32.sh` prints `char (*)[116]` | correct |
| 5 | +0x18 `pVertexList` is fixed up unconditionally | `ppnVar16[-1]` (relative to +0x1C) = base + value | correct |
| 6 | +0x1C `pVertexInfo` is fixed up unconditionally (new) | `*ppnVar16` = base + value (Ghidra's `[4].pad - 0x4c` cancels out) | correct |
| 7 | +0x20 `pNormalList` is fixed up unconditionally | `ppnVar16[1]` (`[1].reserved - 0x4e` cancels out) | correct |
| 8 | +0x24 `pPolygonList` is fixed up unconditionally | `ppnVar16[2]` | correct |
| 9 | +0x28, +0x2C and +0x30 (mother, daughter, sister) are fixed up only when non-zero | `ppnVar16[3]`, `[4]` and `+5` (shown as `hmatrix.m11 != 0.0`) | correct |
| 10 | `localMatrix` is at +0x34 (16 floats up to the 0x74 stride) | 0x74 − 0x34 = 0x40; the self-test checks `m44 == 1.0` on all 4,563 objects | correct |
| 11 | `pName` (+0) is fixed up only when non-zero | `if (pName != 0) pName += base` | correct |
| 12 | `iObject` (+5) gets the index and `nameCRC` (+6) gets crc16 of the name | `*(ubyte*)(ppnVar16 - 0x17) = i`, `*(crc16*)(ppnVar16 - 0x16) = crc16Compute(...)` | correct |
| 13 | `mesh->name` gets the header `pName` plus the base, unconditionally | `meshReference->name = object + local_d38 - 0x44` | correct |
| 14 | `publicMaterial`/`localMaterial` = `object + o - 0x44`; the 64-bit converter keeps both table offsets (GoK set them equal) | decompiled; in all 1,729 files `nLocalMaterials` = 0 and `oLocal == oPublic` (`mats.py`), so the two agree on the shipped data | correct |
| 15 | `localSize` = 0 for `RMF99ba`, else 0xFFFFFFFF | `_strncmp(local_d44, "RMF99ba", 8)` | correct |
| 16 | materials: stride 0x20; `pName` fixed up when non-zero; `bTexturesRegistered` = 0; `textureNameSave` = texture name | `iVar15 += 0x20` loop | correct |
| 17 | in the only-packing branch, texture = 0 and nothing is registered | the `else` branch of `(iVar17 == 0) \|\| (DAT_00a74d90 == 0) \|\| ...` | correct |
| 18 | the exe never reads the version dword (the HW1 version check is removed) | header bytes +8..+0xB appear in no expression in the decompile; in the files, 1,727 have 0x500 and 2 have 0x402, all with the 0x74 stride (`treecheck.py`) | correct |
| 19 | `.peo` paging is inlined (`'p'` store) and `mainAllowPacking` 0x008b8614 = 1 | `pbVar7[~uVar13 - 4] = 0x70`; `exedata.py`: dword = 1 | correct |
| 20 | `mainOnlyPacking` 0x00a74d90 is 0, and the exe has no `/onlyPacking`, `/disablePacking` or `/allowPacking` | address lies past `.data`'s raw size (0x196000), so it is zero-initialised; byte search finds 0 of each string | correct |

### 2b. Render and colour path (disassembly and decompile)

| # | claim | evidence | verdict |
|---:|---|---|---|
| 21 | `meshObjectRender` takes +0x1C as the info list (stride 0x10) and +0x20 as normals. The decompile shows +0x1C as normals, but it uses a wrong 2-parameter prototype | disassembly 0x4b0224/0x4b0227 store +0x1C and +0x20 into `[base+0x10]` and `[base+0x14]`. The face normal comes from `[esp+0x14]` (0x4b0424), the info entry from `[esp+0x10] + iV<<4` (0x4b045e-0x4b046b). The state is `ebp + iV*0x28` (0x4b0453/0x4b0456) | correct |
| 22 | `enableCPV` 0x00c52378 gates every per-vertex `shColourSet0`. Its default is the `.bss` 0 | `test [0xc52378]` at 0x4b0440; the address lies past `.data`'s raw size | correct |
| 23 | the `shColourSet0` arguments are (normal, state, info) | `push ecx(info); push eax(state); push edi(normal); call 0x50dfe0` | correct |
| 24 | in the flat/textured paths of `meshObjectRenderLit`, vertex 0 is called with (NULL, NULL) when the option is off, and vertices 1 and 2 get calls only when it is on | 0x4b0af4-0x4b0bf5: `push 0; push 0` at 0x4b0b63, and `je` past the 2nd and 3rd calls | correct |
| 25 | in the smooth paths of `meshObjectRenderLit`, every vertex gets the info entry and state when the option is on, and neither when it is off | 0x4b0d06-0x4b0d57 | correct |
| 26 | `cat_shColourAddGlow` is gated by the `cbv` byte. It adds signed bytes 0..2 when the low 24 bits are non-zero and clamps to 0..255. With a state, it takes the glow path | decompiled 0x0050d7c0. The port fatals on a non-NULL state (a stage-2 stub, documented), and no caller passes one | correct |
| 27 | `shColourSet0` converts with FAST_TO_INT and clamps at both ends. It adds the glow when info ≠ NULL. With the `lighting` byte clear it uses 0x80 grey plus the glow at alpha 0xFF, and otherwise the alpha byte comes from `shUpdateLighting` | decompiled 0x0050dfe0 and 0x0050dc60 (`_DAT_00a5b618 = FAST_TO_INT`). The constant at 0x0072a458 is 6755399441055744.0 = 1.5·2^52 | correct |
| 28 | console rows `cbv` 0x00878560 → 0x0087816c and `lighting` 0x00878574 → 0x0087816d, both bytes 1 | bytes at 0x878560/0x878574: var 0x87816c/0x87816d, names 0x8782f0 = `cbv` and 0x8782f4 = `lighting`. Both bytes = 1. The only direct code references are 0x50d7c0 and 0x50e21c | correct |
| 29 | the options table row 0x008c14c0 is `enableCPV` → 0x00c52378, after `enableSmoothing` | `exedata.py`: rows `noLOD`, `enableSmoothing`, `enableCPV`, `enableLensFlare`, all with callback 0x00525c20 | correct |
| 30 | command-line row 0x008b8a80 is `/noCPV` → 0x00c52378 with value 0 and help " - do not use vertex CPV." | the row's dwords are `1, 0x8b96b0 "/noCPV", 0, 0xc52378, 0, 0x8b9694 " - do not use vertex CPV."` | correct |

### 2c. Loads under ASan and counts against the probe

- **Isolation tree.** I built a fresh tree with `tools/m2_harness_iso.sh iso-audit-mesh 3,4,5,6,7,8,13,14,15`.
  `treecheck.py` confirmed all 1,729 `.geo`/`.peo` in it:
  - all of them resolve into `original/game_extracted`;
  - all have the 0x74 stride;
  - versions: 0x500 in 1,727 files, 0x402 in 2.
- **Run `audit-mesh-meshes-iso`** (audit clean debug build, ASan+UBSan, `--mode meshes`): milestone `mesh self-test
  done`.
  - Loads: 1,758 listed, 1,729 `.geo`/`.peo` + 29 `.mad` loaded, 0 with `bad=`.
  - ASan: **0 errors**, and no `asan.*` file was written.
  - UBSan: only the known `fescreen`/`PATCH` misalignments and the `MUSICHEADER` null access in `SpeechEvent.c`
    (music item). Nothing comes from `Mesh.c`, `Shader.c` or `MeshAnim.c`.
  - `mesh_engine_check.py`: `1758 files on disk, 1758 match, 0 mismatch, 0 not loaded, 0 extra`.
- **Run `audit-mesh-meshes-iso-postfix`** (the worktree's debug build after the audit's comment fixes): same
  result, and `mesh_engine_check.py` again reports 1,758 match.
- **10 meshes, probe against engine** (`pick10.py` plus `tools/formats/mesh_probe.py`). ident, objects, materials,
  vertices, polygons and normals are equal for all 10 (**10/10**):

  | mesh | why it was picked |
  |---|---|
  | `nislightsource1.geo` and `nislightsource1.peo` | the 0x402 stamps |
  | `bheavycruiser.geo` and `bheavycruiser.peo` | 44 shared arrays |
  | `trghostship.geo` | the largest file, 20 shared arrays |
  | `misc/engineglows/energybolt.geo` | an engine glow |
  | `derelicts/galaxy/.../galaxy.peo` | a new derelict type |
  | `p1mothership.peo` | a mothership |
  | `scarriersupport.peo` | |
  | `p1outpost/rl0/lod0/p1outpost.peo` | 24 shared arrays, the earlier pool-corruption case |

| # | claim | evidence | verdict |
|---:|---|---|---|
| 31 | all 1,729 `.geo`/`.peo` and 29 `.mad` load with 0 ASan reports, and every count matches | the run above | correct |
| 32 | 10 sampled meshes: the engine's counts equal the probe's | 10/10 | correct |
| 33 | totals: 967 `.geo` + 762 `.peo`; 4,563 objects; 3,930 glow vertices in 324 files; 400 files share a list | `counts.py` reproduces every number | correct |
| 34 | static asserts: 7 sizes + 9 offsets always, 2 sizes + 5 offsets on ILP32 | counted in `Mesh.h` | correct |
| 35 | `Mesh.h` `vertexinfo` comments: "byte 3 = 0xFF in the data" and flags "0, 1, 3, 0x10, 0x13 in the data" | `vinfo.py` over all 1,729 files (139,602 entries, shared lists counted once): flags also take **2 (86)** and **0x12 (48)**, and byte 3 is **0x80 in 28** entries. The producer's text comes from R1's 200-file census, which `docs/formats/mesh.md` labels as such; the comment states it for "the data" as a whole | **wrong (fixed)** |

## 3. The diff: scope, IDENTICAL functions, CAT comments

`hunks.py` lists every `src/` hunk with no `CAT` in its lines. Classes come from `ghidra/delta_map.json`.

| # | item | evidence | verdict |
|---:|---|---|---|
| 36 | **`meshLoad` (IDENTICAL, medium) was edited.** It gains the `pVertexInfo` fix-up (also on the `FIX_ENDIAN` path), and the debug-only version check is removed. Both edits are justified by the exe and carry `CAT:` comments. The report's "What is left" 8, "`meshLoad` stays IDENTICAL in source terms: only the struct changed", is wrong: HW1 `mesh.c:688-690` fixes up 3 lists and the exe 4 | `src/reference/hw1-ad1bb82/Game/mesh.c`; §2a rows 5-8 | **wrong (fixed: doc annotation; the delta map should read MODIFIED)** |
| 37 | 3 of 36 hunks have no `CAT` marker: the six `meshColourLit` call sites in the smooth modes of `meshObjectRenderLit` (2 hunks), and `#include <stddef.h>` in `Mesh.h`. The macro definition carried the comment, but the call sites did not | `hunks.py` | finding (fixed: `CAT:` comments added at all 7 lines) |
| 38 | `ObjTypes.h`/`.c` (the 3 derelict types `Galaxy`, `LavaBall`, `MattWorld`) are outside the mesh scope. The loader self-test needs them, because `meshNameToStaticInfo` (IDENTICAL, not edited) asserts on `derelicts\galaxy\...`. The types are appended, so existing values do not move. They are marked `CAT-PROVISIONAL`, a convention already in the tree (`Sensors.c`, `ConsMgr.c`), and the open derelict-table item is recorded. Start-up does not load them (run `audit-mesh-fe-iso`: 0 derelict loads). Tables sized by `NUM_DERELICTTYPES` get zero-filled entries | grep of `NUM_DERELICTTYPES`; `FileLoads.log` | finding (accepted, no change) |
| 39 | MODIFIED functions edited with `CAT:` comments: `meshObjectRender`, `meshObjectRenderLit`, `shColourSet0` (+ `cat_shColourAddGlow` cat-new), and `utyGameSystemsInit` (the self-test hook after `SS2_SystemStarted`, active only when `/catMeshLoadTest` is given). GoK-only code: `meshLoadAndConvertTo64Bit`. Data rows: the `commandOptions` and `utyOptionsList` rows | diff | correct |
| 40 | `meshObjectRender` has 3 whitespace-only line changes (trailing blanks removed in the flat case) | diff | finding (cosmetic, no change) |
| 41 | `madFileLoad` is unchanged in source. The report says the delta map's "drops `StatusDump.txt`" is a mismatch, because the string belongs to `KAS.c` | `grep`: `StatusDump` is only in `src/Game/KAS.c:797` and absent from HW1 `meshanim.c`. The rest of the reclassification ("field-name noise") I did not re-derive | correct (partly verified) |

Observation, not counted: the `Shader.c` comment says no code writes the `cbv`/`lighting` bytes. That is true of
direct references (§2b row 28). The console table holds their addresses, so a console command could write them; the
tree has no such console.

## 4. DONE WHEN

| criterion | audit evidence | verdict |
|---|---|---|
| all 1,729 `.geo`/`.peo` load with no ASan report; the test mode stays off the default start-up path | run `audit-mesh-meshes-iso` (§2c); the hook runs only when `catMeshSelfTestList[0] != '\0'` | **met** (row 42, correct) |
| the 21 engine-glow meshes load at start-up | run `audit-mesh-fe-iso` (audit build, `--mode fe`, 40 s): milestone `Main_game_screen`; `FileLoads.log` lists the 21 `misc\EngineGlows\*.geo` from the unconverted originals; 0 ASan errors | **met** on the isolation tree (row 43, correct) |
| the harness gets past the mesh stop without the down-conversion | `audit-mesh-fe-iso` gets past the engine-trail meshes to `Main_game_screen` on a tree with stage 15 undone. The HEAD baseline's stop is `Mesh.c (917): meshLoad: Invalid version # ...energybolt.geo` (`runs/mesh-r0-head-fe-iso/summary.txt`), and the removed check is the one at that line. Plain gate `audit-mesh-gate-meshes`: first stop `Unable to open file: HW_Music.wxd` with 0 file loads, so producer 4's item comes before any mesh | **met on the isolation harness**; the plain gate cannot show it until the music item lands, as the producer reported (row 44, correct) |

Also confirmed: the plain-harness `meshes.list` comes from the `cataclysm.big` TOC with 1,758 entries (row 45,
correct).

## Precision

Sampled checks: 45 (rows 1-45). Correct: 43. Wrong: 2 (row 35: the `vertexinfo` value comment; row 36: "`meshLoad`
stays IDENTICAL in source terms"). **Precision = 43 / 45 = 0.956.** Findings that are not wrong claims (rows 37,
38, 40) are not counted in precision.

## Fixes (4)

1. `src/Game/Mesh.h` `vertexinfo`: the comments now give the full value sets, marked `[audit: ...]`.
2. `docs/formats/mesh.md` "M2-R2 findings": `[audit: ...]` note with the census over all 1,729 files.
3. `docs/m2-r2-mesh.md` "What is left" 8: `[audit: ...]` note. `meshLoad` is not IDENTICAL in source terms, and
   the delta map should read MODIFIED.
4. `CAT:` comments added:
   - `src/Game/Mesh.c`: the six `meshColourLit` call sites (`meshObjectRenderLit` 0x004b0970);
   - `src/Game/Mesh.h`: `#include <stddef.h>`.

The fixes change comments only. Both builds stay green with 0 warnings, ILP32 layout is OK, and the post-fix
self-test run gives 1,758/1,758 with 0 ASan errors.

## What is left

The producer's report lists what is left, items 1-9. The audit adds one item: correct the delta map's class for
`meshLoad` 0x004aec90 to MODIFIED when the map is next regenerated.

Verdict: PASS-WITH-FIXES - layout, all 7 pointer fix-ups and the colour path match the exe (43/45 checks); 1,758/1,758 meshes load under ASan with 0 reports and match the files; 21 engine glows load from the originals; 2 factual errors and 3 CAT-less hunks fixed (comments/docs only); the plain gate still stops first at producer 4's HW_Music.wxd
