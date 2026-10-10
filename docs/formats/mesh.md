<!-- done: mesh -->
# Mesh format: `.geo` / `.peo` (Cataclysm "version 0x500")

M2-R1 item 2 (loader-format deltas). Sources: Cataclysm `meshLoad` 0x004aec90, `meshPagedVersionExists` 0x004ae9c0,
`meshObjectFixupUV` 0x004af220, `meshObjectRender` 0x004b0170, `shColourSet0` 0x0050dfe0, `cat_shColourAddGlow` 0x0050d7c0,
`cat_meshObjectForVerticesInRadius` 0x004b5190, `cat_meshObjectRenderEdgeEffect` 0x004b4a60 (decompiled.c of
`ghidra/exports/cataclysm_gog_r5`, plus `objdump -d` of `cataclysm.exe` 0x4b0217-0x4b0483 where Ghidra lost track of the
stack); HW1 `ORIG@ad1bb82:src/Game/mesh.h:50-205`, `mesh.c:347-760` (`meshTextureNameToPath` :347, `meshPagedName` :381,
`meshPagedVersionExists` :414, `meshObjectFixupPacoUV` :430, `meshLoad` :598). Checked against all 1,729 mesh files in `original/game_extracted/cataclysm/` with
`tools/formats/mesh_probe.py`.

## Summary

* **The version number is not what changed.** Cataclysm's `meshLoad` never reads the version dword (HW1 only checked it
  under `MESH_ERROR_CHECKING`, which is 0 in `HW_Release`, mesh.h:40). The one file still stamped 0x402
  (`race_derelict/nislensflaresource1/rl0/lod0/nislightsource1.geo`, and its byte-identical `.peo`) already has the new
  layout.
* **The real change: `polygonobject` grew from 0x70 to 0x74 bytes.** A new pointer at +0x1C (a per-vertex 16-byte array,
  one entry per vertex) is inserted between `pVertexList` and the normal list, so `pNormalList` moves to +0x20,
  `pPolygonList` to +0x24, the three hierarchy links to +0x28/+0x2C/+0x30 and `localMatrix` to +0x34. Evidence: stride
  `ppnVar16 += 0x1d` dwords and four unconditional fixups at +0x18..+0x24 in `meshLoad`; `meshObjectFixupUV` and
  `cat_meshObjectRenderEdgeEffect` read polygons at +0x24; `meshObjectRender` reads vertices +0x18, normals +0x20, polygons
  +0x24 and passes `+0x1C[iV]` to `shColourSet0` (disassembly 0x4b0224-0x4b022d, 0x4b0424-0x4b0470).
* **The new per-vertex entry is a colour offset plus flags.** Dword 0 is `0xFFrrggbb`-style packed signed RGB that
  `cat_shColourAddGlow` adds (as `signed char` per channel, clamped 0..255) to the lit vertex colour when the low 24 bits are
  non-zero; dword 1 holds small flags (values 0, 1, 3, 0x10, 0x13 seen); dwords 2-3 are always 0 in the shipped data. The
  bullet-hit code (`cat_meshApplyBulletHit` 0x004b52d0 → `cat_meshObjectForVerticesInRadius`) hands these entries, with a
  per-ship runtime array of 0x28 bytes per vertex, to a callback. Field names below are mine.
* **`.peo` is not new.** HW1 already had it: `meshPagedName` replaces the first extension letter with `p` (mesh.c:381-404,
  "paged version (.peo)" in the comment at :409), and `RMF99ba` marks the "new format" whose `polyentry.reserved[]` carries
  the PACO UV-seam fixup data (`meshObjectFixupPacoUV`, mesh.c:430ff). Cataclysm keeps the logic verbatim (inlined into
  `meshLoad`; the `'p'` store is `pbVar7[~uVar13 - 4] = 0x70`). With `mainAllowPacking` = 1 (`.data` 0x008b8614 holds 1 in
  the exe) every ship LOD loads the `.peo` when it exists; the `.lod` files name `.peo` directly anyway
  (`race_sect/scarrier.lod`: `pMeshFile0 sCarrier\Rl0\LOD0\sCarrier.peo`). 761 of 762 `.peo` files are `RMF99ba`, all 967
  `.geo` are `RMF97ba`.

## On-disk layout (Cataclysm; all little-endian, 32-bit)

```c
/* File = GeoFileHeader, polygonobject[nPolygonObjects], materialentry[nPublic+nLocal], then the arrays.
   Every "offset" field is a FILE offset (the loader adds (mesh->object - 0x44)). */
typedef struct GeoFileHeader {        /* 0x44 bytes, unchanged from HW1 */
/*00*/ char   identifier[8];          /* "RMF97ba\0" (.geo) or "RMF99ba\0" (.peo, PACO UV data present) */
/*08*/ udword version;                /* 0x500 in 1,727 files, 0x402 in 2; NOT read by the loader */
/*0C*/ udword oName;                  /* 0 in shipped files (mesh->name then points at the header area) */
/*10*/ udword fileSize;               /* == file length - 0x44 (loader uses fileSizeGet instead) */
/*14*/ udword localSize;              /* ignored; loader sets mesh->localSize = 0 (RMF99ba) / 0xFFFFFFFF (RMF97ba) */
/*18*/ udword nPublicMaterials;
/*1C*/ udword nLocalMaterials;        /* 0 in all files probed */
/*20*/ udword oPublicMaterial;        /* == 0x44 + nPolygonObjects*0x74 */
/*24*/ udword oLocalMaterial;         /* == oPublicMaterial when nLocalMaterials == 0 */
/*28*/ udword nPolygonObjects;
/*2C*/ ubyte  reserved[24];           /* 0xAB fill */
} GeoFileHeader;

typedef struct polygonobject {        /* 0x74 bytes in Cataclysm (HW1: 0x70) */
/*00*/ udword oName;                  /* fixed up if != 0; nameCRC = crc16(name) computed at load */
/*04*/ ubyte  flags;                  /* POF_* (HW1 values) */
/*05*/ ubyte  iObject;                /* set to the index at load */
/*06*/ uword  nameCRC;                /* computed at load */
/*08*/ sdword nVertices;
/*0C*/ sdword nFaceNormals;
/*10*/ sdword nVertexNormals;
/*14*/ sdword nPolygons;
/*18*/ udword oVertexList;            /* vertexentry[nVertices], 16 B */
/*1C*/ udword oVertexInfo;            /* NEW: vertexinfo[nVertices], 16 B */
/*20*/ udword oNormalList;            /* normalentry[nFaceNormals+nVertexNormals], 16 B   (HW1: +0x1C) */
/*24*/ udword oPolygonList;           /* polyentry[nPolygons], 40 B                       (HW1: +0x20) */
/*28*/ udword oMother;                /* 0 = none, else file offset of another polygonobject (HW1: +0x24) */
/*2C*/ udword oDaughter;              /*                                                   (HW1: +0x28) */
/*30*/ udword oSister;                /*                                                   (HW1: +0x2C) */
/*34*/ hmatrix localMatrix;           /* 64 B, column-major, m44 = 1.0                     (HW1: +0x30) */
} polygonobject;

typedef struct vertexinfo {           /* NEW, 16 B; names are guesses */
/*00*/ udword colourOffset;           /* bytes 0..2 = signed R,G,B offsets; byte 3 = 0xFF in data */
/*04*/ udword flags;                  /* 0,1,3,0x10,0x13 observed; consumed by cat_shGlowUpdate (UNVERIFIED meaning) */
/*08*/ udword reserved[2];            /* always 0 in shipped data */
} vertexinfo;

typedef struct materialentry {        /* 0x20 B, unchanged */
/*00*/ udword oName; /*04*/ color ambient; /*08*/ color diffuse; /*0C*/ color specular;
/*10*/ real32 kAlpha; /*14*/ udword oTexture /* file offset of texture name, or 0 */;
/*18*/ uword flags; /*1A*/ ubyte nFullAmbient; /*1B*/ bool8 bTexturesRegistered; /*1C*/ char *textureNameSave;
} materialentry;

typedef struct vertexentry { real32 x, y, z; sdword iVertexNormal; } vertexentry;          /* 16 B, unchanged */
typedef struct normalentry { real32 x, y, z; ubyte pad[4]; } normalentry;                  /* 16 B, unchanged */
typedef struct polyentry {                                                                 /* 40 B, unchanged */
/*00*/ sdword iFaceNormal; /*04*/ uword iV0, iV1, iV2; /*0A*/ uword iMaterial;
/*0C*/ real32 s0, t0, s1, t1, s2, t2; /*24*/ uword flags /* PAC_* */;
/*26*/ ubyte reserved[2];  /* RMF99ba: [0] = has-adjust flag, [1] = log2(w)<<4 | log2(h) (HW1 meaning) */
} polyentry;
```

Array order in the files (from the probe): vertex lists for all objects, then vertex-info lists, then normal lists, then
names, with the polygon lists right after the material table. Duplicate objects share arrays (400 of 1,729 files have at
least one shared span); the loader does not care.

## Loader control flow (`meshLoad` 0x004aec90)

1. If `mainAllowPacking` (0x008b8614, initial value 1) and the paged name exists (`.geo` → `.peo`, built inline with
   `strtok("\\/")`), load that instead.
2. `fileSizeGet`, `fileOpen`, read 0x44-byte header. `strncmp(ident, "RMF99ba", 8)` only decides `localSize = 0` vs
   `0xFFFFFFFF` (PACO data present or not). No ident or version validation.
3. `memAlloc(fileSize - 0x24)` (= file length - 0x44 header + 0x20 `meshdata` head), copy counts, set
   `publicMaterial/localMaterial = object + o - 0x44`, `fileName = memStringDupe(name)`, read the rest of the file into
   `mesh->object[0]`, close.
4. For each object (stride 0x74): add the base to +0x18, +0x1C, +0x20, +0x24 unconditionally and to +0x28, +0x2C, +0x30 and
   the name when non-zero; set `iObject`, `nameCRC = crc16Compute(name)`.
5. If `mainOnlyPacking` (0x00a74d90; no direct write found in decompiled.c) and the paged file was NOT used: fix material
   names, clear texture handles, return. Otherwise, per material (stride 0x20): fix the name, clear
   `bTexturesRegistered`, and if `texture != 0` fix it to the name, save it in `textureNameSave`, build
   `<mesh dir>\<texture>` uppercased, `meshTextureRegisterAllPlayers`, store the handle and set registered.

Identical to HW1 `mesh.c:598-760` apart from step 4's object stride and extra pointer.

## Diff vs HW1

| Field / key / branch | HW1 (`ad1bb82`) | Cataclysm | Evidence |
|---|---|---|---|
| `MESH_Version` check | 0x402, fatal mismatch only with `MESH_ERROR_CHECKING` (0 in release) | never read; files say 0x500 | `meshLoad` 0x004aec90 (no compare after `fileBlockRead(...,0x44)`); mesh.h:40,56 |
| Ident check | fatal unless RMF97ba/RMF99ba (debug only) | none; only `strncmp("RMF99ba")` for the PACO flag | `meshLoad` 0x004aec90 |
| `sizeof(polygonobject)` | 0x70 | **0x74** | `meshLoad` loop `ppnVar16 += 0x1d`; probe: `oPublicMaterial == 0x44 + n*0x74` in 1,729/1,729 files |
| +0x1C | `pNormalList` | **`pVertexInfo` (new per-vertex colour offset + flags)** | `meshObjectRender` 0x004b0170 → `shColourSet0(normal, state, vinfo)`; `cat_shColourAddGlow` 0x0050d7c0 |
| `pNormalList` | +0x1C | +0x20 | `meshObjectRender` asm 0x4b0227, 0x4b0424 (`[esp+0x14]`) |
| `pPolygonList` | +0x20 | +0x24 | `meshObjectFixupUV` 0x004af220, `cat_meshObjectRenderEdgeEffect` 0x004b4a60, `meshObjectRender` 0x4b022d |
| `pMother/pDaughter/pSister` | +0x24/+0x28/+0x2C | +0x28/+0x2C/+0x30 | `meshLoad` conditional fixups; `cat_meshApplyBulletHit` walks +0x2C/+0x30 |
| `localMatrix` | +0x30 | +0x34 | `cat_meshApplyBulletHit` pushes `obj + 0x34` as GL matrix; probe m44 at +0x70 |
| `shColourSet0` signature | `(vector *norm)` | `(float *norm, float *vertexState, udword *vinfo)` | shader.c:701 vs 0x0050dfe0 |
| `.peo` paging | `meshPagedName` + `meshPagedVersionExists` | same, inlined into `meshLoad` | mesh.c:381-418; 0x004ae9c0, 0x004aec90 |
| `mainAllowPacking` default | TRUE (GoK `src/SDL/main.c:155`; not defined in `ORIG` main.c) | 1 (`.data` 0x008b8614) | objdump of `.data` |
| Header, materials, vertex, normal, polygon entries | 0x44 / 0x20 / 0x10 / 0x10 / 0x28 | unchanged | `meshLoad` reads 0x44, material stride 0x20; `meshObjectFixupUV` polygon stride 10 floats |
| Per-ship mesh runtime state | none | 0x28 bytes per vertex, per LOD (`cat_univShipMeshStateCreate` 0x00541cd0) | `meshObjectRender` (`state + iV*0x28`), `cat_meshObjectForVerticesInRadius` |

## Probe

`tools/formats/mesh_probe.py [-v] FILE...` checks the header, the 0x74 object stride (oPublicMaterial must equal
0x44 + n*0x74), that every array is inside the file, that arrays do not partially overlap, that hierarchy links land on
object boundaries and that each `localMatrix.m44` at +0x70 is 1.0. Results:

```
OK   ident=RMF99ba ver=0x500 nObj=17 nMat=109 verts=454 polys=633 norms=641 shared_arrays=40  race_sect/scarrier/rl0/lod0/scarrier.peo   [audit: re-run gives shared_arrays=40, not 8; the probe is deterministic]
OK   ident=RMF97ba ver=0x500 nObj=1 nMat=1 verts=8 polys=12 norms=6 shared_arrays=0  defaultship/defaultship.geo
OK   ident=RMF97ba ver=0x402 nObj=1 nMat=1 verts=3 polys=1 norms=1 shared_arrays=0  race_derelict/nislensflaresource1/rl0/lod0/nislightsource1.geo
all .geo + .peo under game_extracted/cataclysm: summary: 1729 files, 1729 ok, 0 fail
```

Vertex-info value census over 200 `.peo` files of race_sect/race_beast/derelicts (17,759 entries): dword 0 has 28
distinct values, `0xff000000` in 17,156 (no offset); dword 1 is 3 (9,335), 0 (7,869), 0x10 (471), 0x13 (76), 1 (8);
dwords 2 and 3 are always 0.

## Open points

* The meaning of `vertexinfo.flags` bits (0x1, 0x2, 0x10) is not traced; `cat_shGlowUpdate` consumes the entry with the
  per-ship state. Next step: decompile `cat_shGlowUpdate` and `cat_univShipMeshStateCreate` in M2-R2.
* Whether anything sets `mainOnlyPacking` (0x00a74d90) is not established (no direct write in decompiled.c; HW1 sets it
  from a command-line switch in neither `ORIG` nor GoK's main.c as far as grep shows).
* `.mex` / `.mad` (mesh extensions, animation) were not re-checked here.
