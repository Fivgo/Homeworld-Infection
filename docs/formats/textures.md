<!-- done: textures -->
# Textures: `.lif` images, `textures.ll` listing, texture registry (`texreg`)

M2-R1 item 2. Sources: Cataclysm `trLIFFileLoad` 0x00586aa0, `trListFileLoad` 0x005878f0,
`trImageMeasureFromListing` 0x005879a0, `trRegistryRefresh` 0x00587a50, `trStartup` 0x00584d30,
`trTextureRegister` 0x00585760, `trStructureGet` 0x00588b40, `trCramRAMComputeAndScale` 0x00589a50,
`cat_trTextureMemorySet` 0x00589cd0 (decompiled.c, `ghidra/exports/cataclysm_gog_r5`; `.data` read with `objdump -s`);
HW1 `ORIG@ad1bb82:src/Win32/texreg.h:110-116,170-300`, `texreg.c:254-321,789-928,1669-1707,2566-2731,2731-2910`;
`ORIG@ad1bb82:tools/win32/LiFList/liflist.c:35-67` (the tool that wrote the files). Checked against all 12,096 `.lif`
files and `textures.ll` with `tools/formats/textures_probe.py`.

## Summary

* **The file formats did not change.** `.lif` is still `"Willy 7"` v0x104 with a 0x30-byte header; `textures.ll` is
  still `"Event13"` v0x104 with a 0x1C-byte header and 32-byte elements. Cataclysm's loaders are byte-for-byte HW1's
  release logic: `trLIFFileLoad` fixes the same four offsets (+0x20..+0x2C), `trListFileLoad` reads 0x1C, strides 32,
  fixes names and `sharedTo`, with no ident/version/count checks (HW1 compiled those out: `TR_ERROR_CHECKING` is 0 in
  `HW_Release`, texreg.h:36 [audit: was :29; the release define is at line 36 of ad1bb82 texreg.h]). The only ident check that survives is the inlined `trLiFMeasure` in `trRegistryRefresh`
  (`strcmp(hdr, "Willy 7")` and `version == 0x104`), as in HW1.
* **`textures.ll` lists 12,403 textures, more than HW1's `LL_MaxElements` 12000** (texreg.h:114 [audit: was :112]). HW1's release build
  would load it (the check is debug-only); a debug build of the HW1 tree would `dbgFatalf`. Keep the check off or raise
  it in the recreation.
* **The registry entry (`texreg`) grew from 0x2C to 0x34 bytes**: two new `uword` down-scale shifts at +0x2C/+0x2E and a
  new `udword` at +0x30 that `trTextureRegister` takes as a **fourth argument** (HW1 has three). It is 1 for every caller
  except the subtitle theme pictures (`subThemePictureSet` 0x005833e0, `subTexturesReset` 0x00583990 pass 0), and
  `trCramRAMComputeAndScale` only shrinks entries where it is non-zero. Registry size is unchanged (6000, `.data`
  0x008bb688 = 0x1770).
* **Texture RAM fitting was rewritten.** HW1 searches a global 16.16 scale factor over RAM pools (`trCramRAMScaleCompute`,
  `trCramAttempt`, `trCramScaleTableCompute`, `trCramIntoRAM`, texreg.c:2373-2731). Cataclysm's single
  `trCramRAMComputeAndScale` sums the bytes of all listed textures, then repeatedly halves the longer side of each scalable
  texture larger than 8 texels (incrementing the per-entry shift) until the total fits the limit, then applies
  `diskWidth >> shiftX`, `diskHeight >> shiftY`. The limit comes from the new texture-detail option
  (`cat_trTextureMemorySet(percent)` maps 0-100 through two local tables: 1..12 MB for the cram limit at 0x008bb698,
  and 10..90 MB for 0x008bb6ac, probably the no-palette pool, UNVERIFIED) and is cut to 30 % in mission 17
  (`singlePlayerGameInfo.currentMission == 0x11`; the CET hook `LoadMission_AdjustTextureMemoryLimit` patches this).

## On-disk layout

```c
typedef struct lifheader {          /* 0x30 bytes, unchanged; "offsets" are relative to the file start */
/*00*/ char   ident[8];             /* "Willy 7\0" */
/*08*/ sdword version;              /* 0x104 */
/*0C*/ sdword flags;                /* TRF_Paletted 0x2, TRF_Alpha 0x8, TRF_TeamColor0 0x10, TRF_TeamColor1 0x20,
                                       TRF_IgnoreDimensions 0x100 (LiFList tool flag, liflist.c:67) */
/*10*/ sdword width, height;
/*18*/ crc32  paletteCRC;
/*1C*/ crc32  imageCRC;
/*20*/ udword oData;                /* always 0x30 */
/*24*/ udword oPalette;             /* paletted: oData + w*h, 256 x color (1024 B) */
/*28*/ udword oTeamEffect0;         /* paletted: 256 B after the palette; RGBA: w*h B map if TeamColor0 */
/*2C*/ udword oTeamEffect1;         /* paletted: 256 B after TE0;        RGBA: w*h B map if TeamColor1 */
} lifheader;
/* Paletted (flag 0x2 without 0x8): [hdr][w*h indices][palette 1024][te0 256][te1 256], file = 0x30 + w*h + 1536.
   Alpha (flag 0x8, even when 0x2 is also set, 72 front-end files with 0x10a): [hdr][w*h*4 RGBA][te0 w*h]?[te1 w*h]?;
   offsets of absent blocks hold left-over values. */

typedef struct llfileheader {       /* 0x1C bytes, unchanged */
/*00*/ char ident[8];               /* "Event13\0" (the string is not even in cataclysm.exe: never checked) */
/*08*/ sdword version;              /* 0x104 */
/*0C*/ sdword nElements;            /* 12,403 in Cataclysm's textures.ll */
/*10*/ sdword stringLength;         /* 547,729 */
/*14*/ sdword sharingLength;        /* 15,464 */
/*18*/ sdword totalLength;          /* == nElements*32 + stringLength + sharingLength == file - 0x1C */
} llfileheader;

typedef struct llelement {          /* 32 bytes, unchanged; sorted by _stricmp(name) for the binary search */
/*00*/ udword oTextureName;         /* offset into the string block (after the element array) */
/*04*/ sdword width, height;
/*0C*/ udword flags;
/*10*/ crc32  imageCRC;
/*14*/ sdword nShared;
/*18*/ udword oSharedTo;            /* offset into the sharing block (sdword indices) if nShared */
/*1C*/ sdword sharedFrom;           /* index or -1 */
} llelement;
```

## Registry entry (in memory, matters for every caller that indexes `trTextureRegistry`)

```c
typedef struct texreg {             /* Cataclysm 0x34 bytes (trStartup: memAlloc(trRegistrySize * 0x34)) */
/*00*/ sword  diskWidth, diskHeight;
/*04*/ sword  scaledWidth, scaledHeight;
/*08*/ sdword nUsageCount;
/*0C*/ ubyte  flags; sbyte nPalettes; sbyte currentPalette; ubyte nSharedTo;
/*10*/ crc32  paletteCRC;           /* 0xdeadb00b at register */
/*14*/ ubyte *palettes;             /* trcolorinfo[8], 0x40 B */
/*18*/ void  *meshReference;
/*1C*/ char  *fileName;
/*20*/ udword handle;
/*24*/ uword  baseScalar, stripeScalar;
/*28*/ udword sharedFrom;           /* TR_NotShared 0xfdfbfaf5 (trStructureGet) */
/*2C*/ uword  scaleShiftX;          /* NEW: diskWidth  >> shift = scaled width  (names mine) */
/*2E*/ uword  scaleShiftY;          /* NEW: diskHeight >> shift */
/*30*/ udword bScalable;            /* NEW: 4th arg of trTextureRegister; 0 = never shrink */
} texreg;                           /* HW1 release (TR_DEBUG_TEXTURES 0): 0x2C bytes, ends after sharedFrom */
```

## Loader control flow

* `trLIFFileLoad(name, flags)`: `fileLoadAlloc` the whole file, add the base to +0x20/+0x24/+0x28/+0x2C, return.
  Used by the front end and other direct users (`ferTextureRegisterType`, colour picker, etc.).
* `trRegistryRefresh`: `trListFileLoad("textures.ll")`; for each pending entry, find it in the listing
  (`trImageMeasureFromListing`, binary search, `_stricmp`) or measure the `.lif` header directly
  (`fileExists` + read 0x30 + `"Willy 7"`/0x104 check); then `trCramRAMComputeAndScale()`; then per mesh sort list
  `trMeshSortListLoad` (reads each `.lif`, scales with `trImageScaleIndexed`, colourises team palettes).

## Diff vs HW1

| Field / key / branch | HW1 (`ad1bb82`) | Cataclysm | Evidence |
|---|---|---|---|
| `.lif` header, ident, version, offsets | 0x30, "Willy 7", 0x104 | same | `trLIFFileLoad` 0x00586aa0; probe 12,096/12,096 OK |
| `textures.ll` header / element | 0x1C / 32 B | same | `trListFileLoad` 0x005878f0 (`fileBlockRead(...,0x1c)`, stride 8 dwords) |
| `LL_MaxElements` | 12000 (debug check only) | no check; file has 12,403 | texreg.h:114 [audit: was :112]; probe of textures.ll |
| ll ident/version check | debug only | none ("Event13" absent from the exe) | `grep -a` of cataclysm.exe |
| LiF measure check | "Willy 7" + 0x104 | same, inlined | `trRegistryRefresh` 0x00587a50 (`fileBlockRead(...,0x30)`, `== 0x104`) |
| `sizeof(texreg)` | 0x2C (release) | **0x34** | `trStartup` 0x00584d30 (`DAT_008bb688 * 0x34`), `trStructureGet` 0x00588b40 |
| `texreg` +0x2C/+0x2E | n/a | per-texture shift X/Y | `trCramRAMComputeAndScale` 0x00589a50 |
| `texreg` +0x30 | n/a | scalable flag | `trTextureRegister` 0x00585760 (`+0x30 = param_4`) |
| `trTextureRegister` args | `(fileName, info, meshReference)` | `(fileName, info, meshReference, bScalable)` | 0x00585760; callers pass 1, subtitles pass 0 |
| Registry size | `trRegistrySize` 6000 | 6000 | `.data` 0x008bb688 = 0x00001770 |
| Texture RAM fit | global 16.16 scale factor over RAM pools (`trCramRAMScaleCompute`/`trCramAttempt`/`trCramIntoRAM`) | per-texture halving loop, min side 8, limit from options; mission 17 at 30 % | 0x00589a50 vs texreg.c:2373-2731 |
| Texture memory option | none | `cat_trTextureMemorySet(percent)` from options/defaults/`utyGameSystemsInit` | 0x00589cd0 |
| Ship texture precache | none | `cat_trPrecacheShipTextures` | 0x00589910 (not decompiled here) |

## Probe

`tools/formats/textures_probe.py FILE...` takes `.lif` files and/or `textures.ll`.

```
OK   ll n=12403 strings=547729 sharing=15464 sharers=2223 shared_from=3866 first=cursorbitmaps\activate  textures.ll
OK   pal8 128x64 flags=0x2  derelicts/crate/rl0/lod0/fronts.lif
OK   rgba 64x64 flags=0x38  speechicons/r1/fleetcommand.lif
OK   rgba 16x16 flags=0x8  feman/loadscreen/ring.lif
all 12,096 .lif under game_extracted/cataclysm: 12,096 OK, 0 fail
```

Flag census (all `.lif`): 0x2 4,530; 0x32 3,890; 0x12 2,137; 0x108 528; 0x8 416; 0x22 415; 0x102 86; 0x10a 72; 0x18 10;
0x38 8; 0x132 2; 0x28 2.

## Open points

* `cat_trPrecacheShipTextures` and the full `trMeshSortListLoad` were not compared line by line (M2-R1 item 3 covers
  function deltas).
* Which loader path handles the 72 files flagged both Paletted and Alpha (0x10a) is not traced; their data is RGBA.
