# Round-2 symbol reconcile report

Output: `ghidra/symbols_round2.json` (cet_symbols.json schema). It holds **962 symbols** (911 function, 51 data): **899 new names** and **63 renames**. Generated 2026-10-09. Inputs:
- `ghidra/symbols_round1.json` (8,312 names, already applied).
- `ghidra/symbols_round1_data.json` (925 data names, applied by A2; A2's export is `.claude/worktrees/r2-a2/ghidra/exports/cataclysm_gog_r2b`).
- `ghidra/create_functions_r2.json` (3,258 A1 entries, 2,312 of them named).
- The six audited sets `ghidra/proposals/r2/*.verified.json`, plus the two auditor `suggested_corrections`.

The current name at each address was read from the r2b export, which reflects round 1 + A1 + A2. Function entries were validated against `ghidra/exports/cataclysm_gog_r2a/functions.csv`; r2b has the same 14,384 entries.

## Totals

| item | count |
|---|---|
| input `missions-r2.verified.json` | 2085 |
| input `ships-r2.verified.json` | 378 |
| input `gaps-render-fe.verified.json` | 479 |
| input `gaps-ai-flight.verified.json` | 328 |
| input `gaps-input-save-sp.verified.json` | 205 |
| input `libs-unknown.verified.json` | 503 |
| auditor suggested_corrections (gaps-input-save-sp, gaps-render-fe) | 2 |
| distinct proposal addresses | 3954 |
| already carry the proposed name (no-op; this covers all 2,085 mission names and 342 ship names applied by A1) | 2482 |
| class-only proposals on functions that already have a name (lib_magick_/lib_ijg_/lib_cms_/CRT); no name change | 404 |
| **deferred**: function address not an entry in r2a functions.csv (all 105 are free code, not inside another function) | **105** |
| **dropped** (other reasons) | **1** |
| cross-set conflicts at one address, resolved | 8 |
| name collisions needing a suffix | 0 |
| CET anchors overridden (libs-unknown compromise) | 2 |
| **symbols written** | **962** |

By confidence: high 673, low 70, medium 219. By class (hw1 = all `hw1:<File.c>` together): hw1 769, cat-new 139, crt 21, lib:libjpeg 18, lib:imagemagick 12, won 3.

By set (a `+` means both sets proposed the same name): gaps-ai-flight 256, gaps-ai-flight+gaps-input-save-sp 4, gaps-ai-flight+ships-r2 8, gaps-input-save-sp 181, gaps-input-save-sp(audit) 1, gaps-input-save-sp+gaps-render-fe 6, gaps-render-fe 421, gaps-render-fe(audit) 1, libs-unknown 56, ships-r2 28.

## Rules as applied

- **Scope.** Only new names and renames are emitted. A proposal whose name Ghidra already carries is a confirmation and is not re-emitted. This applies to all 2,085 missions-r2 entries (A1 applied the mission-code-map names) and to 342 of the ships-r2 entries.
- **Validity.** A function address must be an entry in r2a `functions.csv`. A data address must lie in `.rdata` 0x72a000-0x749fff or `.data` 0x74a000-0xc54197 (r2a program.json memory blocks); all 51 data names pass. Names must be C identifiers.
- **Renames.** A round-1 or A1 name is replaced only when a round-2 set gives stronger evidence for that address. That means one of: a real identification replacing a round-1 class-only placeholder (`lib_<class>_<addr>`); the verbatim HW1 spelling replacing a mechanical A1 slot label; or an explicit correction that the auditing set made (the libs-unknown `compromises` and `suffixes` subsystems). Every rename is listed below.
- **CET protection.** No CET anchor name is replaced except where the libs-unknown compromise resolution says so (0x006725D0, 0x00673080). Both are recorded under "CET differs". `cet_symbols.json` itself is unchanged.
- **Uniqueness.** Every output name was checked against: CET (patch sites included), round 1, round-1 data, A1 names, every non-default Ghidra function/IMPORTED/USER_DEFINED name in r2b, and the other round-2 names. A name that a rename in this file releases counts as free. No collision remained, so no `_N` suffix was added.
- **Apply order.** Renames that release a name come first in the file: 0x00673080 memFree->_free before 0x004AE130 memFree_2->memFree, and 0x00C60751 _malloc->_malloc_cms before 0x006725D0 memAlloc->_malloc.
- **Notes.** `notes` starts `class=<class>; confidence=<c>; sets=<sets>`. Optional fields follow: `resolved=cross-set`, `renamed-from=<old>`, `cet-differs=...`, `carved=...`, `class-fix=...`, `evidence=<=200 chars`. `type` holds the set's signature when it gave one. For a rename without a signature, it holds the round-1 prototype with the name replaced. `source` names the verified file; auditor corrections are marked `#audit.suggested_corrections`.
- **Auditor corrections included** (both medium):
  - 0x005025E0 `Load_CommandToDo`, which replaces the dropped `Load_CommandLayer`.
  - 0x004A8F00 `toLegendDraw` (hw1:Tactical.c), which replaces the dropped `cat_moShipOutlinesDraw`.
- **Auditor class fix:** 0x00B3A858 `MissileIDToPtr` has class `hw1:UnivUpdate.c`.

## Renames (63)

| address | old name | new name | old name came from | r2 set, confidence | reason |
|---|---|---|---|---|---|
| 0x004AE130 | `memFree_2` | `memFree` | round 1 (hw1-match.verified.json, high) | libs-unknown, high | libs-unknown compromise: the real HW1 Memory.c memFree (406 callers: pool lookup, SBH list moves, memFreeNV coalesce). Round 1 had to suffix it because CET's label memFree sat on CRT free 0x673080; that label is now _free (CET differs), so the bare name moves here |
| 0x005AB5B0 | `utyGameQuitToMain_2` | `cat_utyGameQuitToMainOrStats` | round 1 (sim-presentation.verified.json, high) | libs-unknown, high | libs-unknown compromise: FE table 0x8c1028 {0x5ab5b0,'QuitToMain'}; the body adds a multiplayer branch (delete screens, stop music, 'TerryStats' stats screen) before the HW1 quit-to-main path, so it is Cataclysm-specific and no longer needs the _2 suffix |
| 0x005AEF70 | `AdvanceSupportFrigateSpecialTarget` | `AdvacedSupportFrigateSpecialTarget` | A1 create_functions_r2.json (ship-slot) | ships-r2, medium | the A1 label was a mechanical slot name; ships-r2 (audit precision 1.0) uses the verbatim HomeworldSDL/src/Ships spelling (checked by grep: the HW1 typo/casing is kept) |
| 0x005B4DE0 | `GravWellGeneratorAttack` | `GravWellAttack` | A1 create_functions_r2.json (ship-slot) | ships-r2, high | the A1 label was a mechanical slot name; ships-r2 (audit precision 1.0) uses the verbatim HomeworldSDL/src/Ships spelling (checked by grep: the HW1 typo/casing is kept) |
| 0x005BB130 | `MinelayerCorvetteDied` | `MineLayerCorvetteDied` | A1 create_functions_r2.json (ship-slot) | ships-r2, high | the A1 label was a mechanical slot name; ships-r2 (audit precision 1.0) uses the verbatim HomeworldSDL/src/Ships spelling (checked by grep: the HW1 typo/casing is kept) |
| 0x005BB150 | `MinelayerCorvetteHouseKeep` | `MinelayerCorvetteHousekeep` | A1 create_functions_r2.json (ship-slot) | ships-r2, high | the A1 label was a mechanical slot name; ships-r2 (audit precision 1.0) uses the verbatim HomeworldSDL/src/Ships spelling (checked by grep: the HW1 typo/casing is kept) |
| 0x005BB2C0 | `MinelayerCorvette_PreFix` | `MineLayerCorvette_PreFix` | A1 create_functions_r2.json (ship-slot) | ships-r2, high | the A1 label was a mechanical slot name; ships-r2 (audit precision 1.0) uses the verbatim HomeworldSDL/src/Ships spelling (checked by grep: the HW1 typo/casing is kept) |
| 0x005BB2F0 | `MinelayerCorvette_Fix` | `MineLayerCorvette_Fix` | A1 create_functions_r2.json (ship-slot) | ships-r2, high | the A1 label was a mechanical slot name; ships-r2 (audit precision 1.0) uses the verbatim HomeworldSDL/src/Ships spelling (checked by grep: the HW1 typo/casing is kept) |
| 0x005BB420 | `MissileDestroyerHouseKeep` | `MissileDestroyerHousekeep` | A1 create_functions_r2.json (ship-slot) | ships-r2, high | the A1 label was a mechanical slot name; ships-r2 (audit precision 1.0) uses the verbatim HomeworldSDL/src/Ships spelling (checked by grep: the HW1 typo/casing is kept) |
| 0x005BE3C0 | `P1MissileCorvetteHouseKeep` | `P1MissileCorvetteHousekeep` | A1 create_functions_r2.json (ship-slot) | ships-r2, high | the A1 label was a mechanical slot name; ships-r2 (audit precision 1.0) uses the verbatim HomeworldSDL/src/Ships spelling (checked by grep: the HW1 typo/casing is kept) |
| 0x005C5AA0 | `SalCapCorvetteRemoveShipReferences` | `SalCapRemoveShipReferences` | A1 create_functions_r2.json (ship-slot) | ships-r2, medium | the A1 label was a mechanical slot name; ships-r2 (audit precision 1.0) uses the verbatim HomeworldSDL/src/Ships spelling (checked by grep: the HW1 typo/casing is kept) |
| 0x005C5AC0 | `SalCapCorvetteDied` | `SalCapDied` | A1 create_functions_r2.json (ship-slot) | ships-r2, medium | the A1 label was a mechanical slot name; ships-r2 (audit precision 1.0) uses the verbatim HomeworldSDL/src/Ships spelling (checked by grep: the HW1 typo/casing is kept) |
| 0x005CA170 | `lib_ijg_005ca170` | `jpeg_suppress_tables` | round 1 (lib-graph.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: IJG jcapimin.c public API: called by jpeg_start_compress(cinfo,0) when write_all_tables |
| 0x005CA1C0 | `lib_ijg_005ca1c0` | `jpeg_finish_compress` | round 1 (lib-graph.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: IJG jcapimin.c: state 0x65/0x66/0x67, finishes passes, marker->write_file_trailer, dest->term_destination, jpeg_abort; JpegWrite |
| 0x005CA2C0 | `lib_ijg_005ca2c0` | `jpeg_start_compress` | round 1 (lib-graph.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: IJG jcapistd.c: state 100 check, suppress_tables(FALSE) if write_all, err->reset, dest->init, jinit_compress_master, state 0x65/0x66; JpegWrite |
| 0x005CA340 | `lib_ijg_005ca340` | `jpeg_write_scanlines` | round 1 (lib-graph.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: IJG jcapistd.c: state 0x65 check, next_scanline +0xd8 vs image_height, progress hook, main->process_data; JpegWrite row loop |
| 0x005CE270 | `lib_ijg_005ce270` | `jpeg_add_quant_table` | round 1 (lib-graph.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: IJG jcparam.c public API: called twice by jpeg_set_quality and jpeg_set_defaults (tbl 0/1, std tables 0x72c0fc/0x72c1fc, scale, force_baseline) |
| 0x005CE330 | `lib_ijg_005ce330` | `jpeg_set_quality` | round 1 (lib-graph.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: IJG jcparam.c: quality clamp 1..100, 5000/q or 200-2q scaling, two jpeg_add_quant_table calls (std lum/chrom tables); called by JpegWrite |
| 0x005CE3A0 | `lib_ijg_005ce3a0` | `jpeg_set_defaults` | round 1 (lib-graph.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: IJG jcparam.c: state 100 check, data_precision=8, add_quant_table x2 scale 50, std Huffman tables, clears arith/optimize/CCIR/smoothing; JpegWrite |
| 0x005CE680 | `lib_ijg_005ce680` | `jpeg_default_colorspace` | round 1 (lib-graph.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: IJG jcparam.c: switch in_color_space 0..5 -> jpeg_set_colorspace 0x5ce700 (RGB/YCbCr->YCbCr), else ERREXIT 7; called by JpegWrite |
| 0x005CE700 | `lib_ijg_005ce700` | `jpeg_set_colorspace` | round 1 (lib-graph.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: IJG jcparam.c public API: called only by jpeg_default_colorspace with JCS ids 0/1/3/4/5 |
| 0x005CF340 | `lib_ijg_005cf340` | `jpeg_read_header` | round 1 (lib-graph.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: IJG jdapi.c: checks global_state 200/201, calls jpeg_consume_input 0x5cf3b0, maps JPEG_REACHED_SOS/EOI; called (cinfo,TRUE) by JpegInfo/JpegRead |
| 0x005CF640 | `lib_ijg_005cf640` | `jpeg_finish_decompress` | round 1 (lib-graph.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: IJG jdapi.c: state 0xcd/0xce/0xcf/0xd2 checks, drains inputctl until EOI, then jpeg_abort; called by JpegRead |
| 0x005CF700 | `lib_ijg_005cf700` | `jpeg_start_decompress` | round 1 (lib-graph.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: IJG jdapi.c: global_state 0xca/0xcb/0xcc, loops inputctl, sets state 0xcd/0xce (buffered_image +0x40); called by JpegInfo/JpegRead |
| 0x005CF800 | `lib_ijg_005cf800` | `jpeg_read_scanlines` | round 1 (lib-graph.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: IJG jdapi.c: state 0xcd check, output_scanline +0x88 vs output_height +0x70, progress hook, main->process_data; JpegRead row loop |
| 0x005CF8A0 | `lib_ijg_005cf8a0` | `jpeg_stdio_dest` | round 1 (lib-graph.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: IJG jdatadst.c: allocs dest mgr 0x1c, sets init/empty/term methods, outfile at +0x14; called by JpegWrite with FILE* |
| 0x005CF9D0 | `lib_ijg_005cf9d0` | `jpeg_stdio_src` | round 1 (lib-graph.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: IJG jdatasrc.c: allocs source mgr 0x28 + 4096-byte (INPUT_BUF_SIZE) buffer via mem->alloc_small, sets 5 methods, infile; called by JpegInfo/JpegRead w |
| 0x006725D0 | `memAlloc` | `_malloc` | round 1 (CataclysmEngineTweaks (cet_symbols.json), high) | libs-unknown, high | CET differs (see below): CET anchor renamed on high-confidence evidence: body is _nh_malloc_dbg(size,_newmode,1,0,0) = MSVC debug-CRT malloc. CET Cataclysm.h:147 labels it memAlloc (kee |
| 0x00673080 | `memFree` | `_free` | round 1 (CataclysmEngineTweaks (cet_symbols.json), high) | libs-unknown, high | CET differs (see below): CET anchor renamed on high-confidence evidence: body is _free_dbg(p,_NORMAL_BLOCK=1) = MSVC debug-CRT free. CET Cataclysm.h:148 labels it memFree (keep as CET a |
| 0x006C2210 | `std___Construct_7` | `std___Construct_Integer` | round 1 (lib-strings.verified.json, high) | libs-unknown, medium | suffix review: the round-1 name _Construct<EGPublicKey> (std___Construct_7) was a shape match with relocations masked and duplicates 0x6b3410; the body placement-copies via the CryptoPP Integer copy ctor 0x6c7260 |
| 0x006E41C0 | `lib_magick_006e41c0` | `Magick__Image__Image_spec` | round 1 (lib-strings.verified.json, high) | libs-unknown, medium | round 1 had only a class-only placeholder; round 2 identifies the library entry point: Magick++ Image(const std::string&): default-init then read() 0x6e4510 (Options::fileName + ReadImage 0x6e7b20 + replaceImage); AdApi 0x6dbce0 loads ad |
| 0x006E4270 | `lib_magick_006e4270` | `Magick__Image__Image` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | round 1 had only a class-only placeholder; round 2 identifies the library entry point: Magick++ Image() default ctor: new ImageRef(0x10), init lock at +8, vtable 0x731f30; WON AdApi lib_won_006db460 calls it |
| 0x006E42E0 | `lib_magick_006e42e0` | `Magick__Image__dtor` | round 1 (lib-strings.verified.json, medium) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: Magick++ ~Image: restores vtable, locks ImageRef, refcount-1, frees at 0; called by 6 WON Unwind funclets and AdApi locals |
| 0x006E4370 | `lib_magick_006e4370` | `Magick__Image__composite` | round 1 (lib-strings.verified.json, high) | libs-unknown, medium | round 1 had only a class-only placeholder; round 2 identifies the library entry point: Magick++ Image::composite(img,x,y,op): modifyImage, columns/rows, Geometry, GetGeometry 0x6f6cf0, CompositeImage 0x6e56a0, throwImageException |
| 0x006E45E0 | `lib_magick_006e45e0` | `Magick__Image__isValid` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | round 1 had only a class-only placeholder; round 2 identifies the library entry point: Magick++ Image::isValid(): returns rows()!=0 && columns()!=0 (0x6e4600 reads +0xd30, 0x6e4540 reads +0xd2c); AdApi 0x6db9a0 tests before DispatchImage |
| 0x006E4610 | `lib_magick_006e4610` | `Magick__Image__size` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | round 1 had only a class-only placeholder; round 2 identifies the library entry point: Magick++ Image::size(): returns Geometry(columns +0xd2c, rows +0xd30, 0,0,0,0) via Geometry ctor 0x6eaab0; called by AdApi 0x6db9a0 |
| 0x006E46D0 | `lib_magick_006e46d0` | `Magick__Image__operator_assign` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | round 1 had only a class-only placeholder; round 2 identifies the library entry point: Magick++ Image operator=(const Image&): ref+1 on source, ref-1/free own ImageRef, share ref, returns copy via copy ctor 0x6e4650 |
| 0x006E4760 | `lib_magick_006e4760` | `Magick__Image__Image_ref` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | round 1 had only a class-only placeholder; round 2 identifies the library entry point: Magick++ Image(MagickLib::Image*,Options*): new ImageRef(image,options) 0x6e4970; called by AdApi 0x6dbe40 to wrap next frame |
| 0x006E51F0 | `lib_magick_006e51f0` | `CloneImage` | round 1 (lib-strings.verified.json, high) | libs-unknown, medium | round 1 had only a class-only placeholder; round 2 identifies the library entry point: ImageMagick C CloneImage(image,columns,rows,orphan): allocs 0x1568-byte Image, copies, resets blob/pixel cache; 4 args, no ExceptionInfo (pre-5.0 API) |
| 0x006E6D70 | `lib_magick_006e6d70` | `GetNextImage` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | round 1 had only a class-only placeholder; round 2 identifies the library entry point: ImageMagick C GetNextImage(image): copies blob info (+0x152c, 6 dwords, +0x68c) into image->next (+0x1560) and returns it; AdApi animated-ad caller |
| 0x006E6DB0 | `lib_magick_006e6db0` | `GetNumberScenes` | round 1 (lib-strings.verified.json, high) | libs-unknown, medium | round 1 had only a class-only placeholder; round 2 identifies the library entry point: ImageMagick C GetNumberScenes(image): walks ->previous (+0x1558) to head, counts ->next (+0x1560); AdApi 0x6dbce0 stores frame count |
| 0x006E6E00 | `lib_magick_006e6e00` | `DispatchImage` | round 1 (lib-strings.verified.json, high) | libs-unknown, high | round 1 had only a class-only placeholder; round 2 identifies the library entry point: ImageMagick C DispatchImage(image,x,y,cols,rows,map,type,pixels): 'Invalid pixel map'/'Invalid pixel type', CMYK map -> RGBTransformImage; AdApi extra |
| 0x006FE9B0 | `lib_magick_006fe9b0` | `lib_won_006fe9b0` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | reclassed from ImageMagick to WON: not ImageMagick: finds default .html handler (HKCR shell\open\command via 0x6feae0) and CreateProcessA's it; sole caller WON AdApi 0x6ecd90 (ad click  |
| 0x006FEAE0 | `lib_magick_006feae0` | `lib_won_006feae0` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | reclassed from ImageMagick to WON: not ImageMagick: reads registry '.html' -> '\shell\open\command' for 0x6fe9b0 browser launch; only caller 0x6fe9b0 (WON AdApi path) |
| 0x00C60690 | `_memset_2` | `_memset_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C60751 | `_malloc` | `_malloc_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | frees _malloc for the game CRT malloc 0x6725d0; this is the second static CRT copy inside the .cms_t stub (calls __nh_malloc 0xc60763) |
| 0x00C60763 | `__nh_malloc_2` | `__nh_malloc_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C60960 | `_strcmp_2` | `_strcmp_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C60E20 | `_strncmp_2` | `_strncmp_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C60F90 | `_strlen_2` | `_strlen_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C61049 | `__exit_2` | `__exit_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C611EC | `__amsg_exit_2` | `__amsg_exit_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C6276F | `__fassign_2` | `__fassign_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C62A74 | `__cfltcvt_2` | `__cfltcvt_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C63C34 | `__global_unwind2_2` | `__global_unwind2_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C63C76 | `__local_unwind2_2` | `__local_unwind2_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C641C0 | `_strchr_2` | `_strchr_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C64280 | `_strstr_2` | `_strstr_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C65240 | `__aulldiv_2` | `__aulldiv_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C652B0 | `__aullrem_2` | `__aullrem_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C65CF0 | `_strncpy_2` | `_strncpy_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C66273 | `___add_12_2` | `___add_12_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |
| 0x00C67314 | `__mbsnbicoll_2` | `__mbsnbicoll_cms` | round 1 (lib-strings.verified.json, medium) | libs-unknown, medium | suffix review: duplicate copy in the .cms_t protection stub's own static CRT; the base name stays on the .text copy, and _cms says why it is a second copy |

## CET differs (2)

The libs-unknown `compromises` subsystem resolved these. Its audit sampled 0x00673080 and found precision 1.0. CET's own labels in `cet_symbols.json` / CET `Cataclysm.h:147-148` are **not** changed, and they stay valid as CET aliases. Anyone who later re-applies `cet_symbols.json` on its own will put the CET labels back, so apply it before this file, never after.

| address | CET name | Ghidra name now | evidence |
|---|---|---|---|
| 0x006725D0 | `memAlloc` | `_malloc` | CET anchor renamed on high-confidence evidence: body is _nh_malloc_dbg(size,_newmode,1,0,0) = MSVC debug-CRT malloc. CET Cataclysm.h:147 labels it memAlloc (keep alias) |
| 0x00673080 | `memFree` | `_free` | CET anchor renamed on high-confidence evidence: body is _free_dbg(p,_NORMAL_BLOCK=1) = MSVC debug-CRT free. CET Cataclysm.h:148 labels it memFree (keep as CET alias) |

The consequence: the HW1 pool allocator's `memFree` (0x004AE130) gets its bare name, and the round-1 "flag for a later ruling" is closed. `memAlloc` (the HW1 Memory.c allocator) is not named in this round. CET's `memAlloc` label sat on CRT malloc and has been renamed `_malloc`.

## Cross-set conflicts resolved (8)

| address | candidates (set: name, confidence) | chosen | reason |
|---|---|---|---|
| 0x004FD740 | gaps-ai-flight: `SpaceObjRegistryGetShip` (high); gaps-input-save-sp: `SpaceObjRegistryGetObj` (medium) | `SpaceObjRegistryGetObj` | ICF-folded registry[id] getter (23 bytes) sits directly after SpaceObjRegistryGetID 0x4fd710 and before BlobRegistryBlobPresent 0x4fd760; HW1 SaveGame.c:162 defines GetObj first and GetShip/GetResource/GetBullet are folded into it, so the first-in-source, most general name wins (gaps-ai-flight's GetShip high is an alias; auditor: 'either name fits; HW1 defines GetObj first') |
| 0x005ADC40 | gaps-ai-flight: `cat_acvClose` (medium); ships-r2: `ACVDied` (medium) | `ACVDied` | ships-r2 header-slot name (audited 1.0) preferred, as the gaps-ai-flight auditor recommends; the cat_* alternative names the same slot body |
| 0x005B7B70 | gaps-ai-flight: `cat_leechClose` (medium); ships-r2: `LeechClose` (high) | `LeechClose` | ships-r2 header-slot name (audited 1.0) preferred, as the gaps-ai-flight auditor recommends; the cat_* alternative names the same slot body |
| 0x005B8210 | gaps-ai-flight: `cat_mcvAttackPassive` (medium); ships-r2: `MCVAttackPassive` (medium) | `MCVAttackPassive` | ships-r2 header-slot name (audited 1.0) preferred, as the gaps-ai-flight auditor recommends; the cat_* alternative names the same slot body |
| 0x005B82E0 | gaps-ai-flight: `cat_mcvHaltCommand` (medium); ships-r2: `MCVHaltCommand` (medium) | `MCVHaltCommand` | ships-r2 header-slot name (audited 1.0) preferred, as the gaps-ai-flight auditor recommends; the cat_* alternative names the same slot body |
| 0x005C0A60 | gaps-ai-flight: `cat_rammingFrigateClose` (medium); ships-r2: `RammingFrigateClose` (high) | `RammingFrigateClose` | ships-r2 header-slot name (audited 1.0) preferred, as the gaps-ai-flight auditor recommends; the cat_* alternative names the same slot body |
| 0x005C59A0 | gaps-ai-flight: `cat_rkoAttackPassive` (low); ships-r2: `RKOAttackPassive` (medium) | `RKOAttackPassive` | ships-r2 header-slot name (audited 1.0) preferred, as the gaps-ai-flight auditor recommends; the cat_* alternative names the same slot body |
| 0x005C7620 | gaps-ai-flight: `cat_slipgateClose` (medium); ships-r2: `SlipgateDied` (medium) | `SlipgateDied` | ships-r2 header-slot name (audited 1.0) preferred, as the gaps-ai-flight auditor recommends; the cat_* alternative names the same slot body |

At the seven ship addresses, the ships-r2 header-slot name is emitted and gaps-ai-flight's cat_* alternative is not applied. Eighteen more emitted addresses were proposed by two sets with the same name: 6 BTG functions (input-save-sp + render-fe), 4 SaveGame registry/selection helpers (ai-flight + input-save-sp) and 8 ship-slot names (ai-flight + ships-r2). They are marked `sets=a+b`.

## Deferred: proposals at addresses that are not function entries (105)

The rule requires a function address to exist in r2a functions.csv, so these names are not in `symbols_round2.json`. None of them lies inside an existing function: they are free code (LAB_ targets of pointer tables) that A1's prologue sweep did not create. They are saved in `ghidra/proposals/r2/reconcile_deferred_functions.json` in the `create_functions_r2.json` format (address, name, reason, plus confidence and evidence). A later Ghidra stage can create and name them in one CreateFunctions pass. Per set: gaps-ai-flight 51, gaps-render-fe 52, gaps-input-save-sp 1, libs-unknown 1.

| address | name | set | confidence |
|---|---|---|---|
| 0x00406e90 | `aimProcessGuardShips` | gaps-ai-flight | high |
| 0x00406f00 | `aimShipDiedGuardShips` | gaps-ai-flight | medium |
| 0x00406fe0 | `aimProcessGetShips` | gaps-ai-flight | medium |
| 0x004072e0 | `aimProcessVarWait` | gaps-ai-flight | high |
| 0x004073f0 | `aimProcessVarDec` | gaps-ai-flight | high |
| 0x00407500 | `aimProcessVarInc` | gaps-ai-flight | medium |
| 0x00407570 | `aimProcessVarSet` | gaps-ai-flight | high |
| 0x00407670 | `aimProcessVarDestroy` | gaps-ai-flight | high |
| 0x00407ac0 | `aimResourceVolumeResourceDied` | gaps-ai-flight | high |
| 0x00407b80 | `aimResourceVolumeShipDied` | gaps-ai-flight | high |
| 0x00407c00 | `aimCaptureShipDied` | gaps-ai-flight | medium |
| 0x00407c20 | `aimProcessMove` | gaps-ai-flight | high |
| 0x00407cc0 | `aimProcessMoveSplit` | gaps-ai-flight | high |
| 0x00407dd0 | `aimProcessHyperspace` | gaps-ai-flight | medium |
| 0x00407fe0 | `aimProcessIntercept` | gaps-ai-flight | high |
| 0x004082a0 | `aimProcessMoveTo` | gaps-ai-flight | medium |
| 0x004083f0 | `aimProcessFlankAttack` | gaps-ai-flight | high |
| 0x00408840 | `aimProcessMoveAttack` | gaps-ai-flight | high |
| 0x00408ca0 | `aimProcessAdvancedAttack` | gaps-ai-flight | high |
| 0x00408d30 | `aimProcessHarassAttack` | gaps-ai-flight | high |
| 0x00409350 | `aimProcessDock` | gaps-ai-flight | high |
| 0x00409760 | `aimProcessPatrolMove` | gaps-ai-flight | high |
| 0x00409af0 | `aimProcessActivePatrol` | gaps-ai-flight | high |
| 0x00409c50 | `aimProcessTempGuard` | gaps-ai-flight | high |
| 0x00409ff0 | `aimProcessReinforce` | gaps-ai-flight | high |
| 0x0040a1c0 | `aimProcessSupport` | gaps-ai-flight | high |
| 0x0040a440 | `aimProcessActiveRecon` | gaps-ai-flight | high |
| 0x0040a790 | `aimProcessShipRecon` | gaps-ai-flight | high |
| 0x0040a910 | `aimProcessArmada` | gaps-ai-flight | high |
| 0x0040abb0 | `aimProcessControlResources` | gaps-ai-flight | high |
| 0x0040ad00 | `aimProcessResourceVolume` | gaps-ai-flight | high |
| 0x0040aee0 | `aimProcessActiveResource` | gaps-ai-flight | high |
| 0x0040b000 | `aimProcessMothershipMove` | gaps-ai-flight | high |
| 0x0040b3f0 | `aimProcessActiveCapture` | gaps-ai-flight | high |
| 0x0040b4b0 | `aimProcessActiveMine` | gaps-ai-flight | high |
| 0x0040b5e0 | `aimProcessMineVolume` | gaps-ai-flight | medium |
| 0x0040d0e0 | `aimProcessGuardCooperatingTeam` | gaps-ai-flight | high |
| 0x0040d420 | `aimProcessFormation` | gaps-ai-flight | high |
| 0x0040d5b0 | `aimProcessAttack` | gaps-ai-flight | high |
| 0x0040d780 | `aimProcessSpecial` | gaps-ai-flight | high |
| 0x0040d8c0 | `aimProcessKamikaze` | gaps-ai-flight | high |
| 0x0040da50 | `aimProcessFancyGetShips` | gaps-ai-flight | high |
| 0x00416b80 | `GenericCooperatingTeamDiedCB` | gaps-ai-flight | medium |
| 0x00468fb0 | `etgEffectLoadVoid` | gaps-render-fe | high |
| 0x0046c3d0 | `etgBlockOpen` | gaps-render-fe | high |
| 0x0046c420 | `etgBlockClose` | gaps-render-fe | high |
| 0x0046c490 | `etgParseModeRestore` | gaps-render-fe | high |
| 0x0046c4a0 | `etgCodeBlockEnd` | gaps-render-fe | high |
| 0x0046c4e0 | `etgVariableBlockOpen` | gaps-render-fe | high |
| 0x0046c530 | `etgStartupBlockOpen` | gaps-render-fe | high |
| 0x0046c580 | `etgEachFrameBlockOpen` | gaps-render-fe | high |
| 0x0046c5e0 | `etgTimeIndexBlockOpen` | gaps-render-fe | high |
| 0x0046c650 | `etgNewInteger` | gaps-render-fe | high |
| 0x0046caf0 | `etgSubTextureDWORD` | gaps-render-fe | high |
| 0x0046cc60 | `etgSetTextureParse` | gaps-render-fe | high |
| 0x0046ceb0 | `etgNewFloat` | gaps-render-fe | high |
| 0x0046d070 | `etgConditionalComplete` | gaps-render-fe | high |
| 0x0046d350 | `etgElseComplete` | gaps-render-fe | high |
| 0x0046d440 | `etgLabelGoto` | gaps-render-fe | high |
| 0x0046d4d0 | `etgLabelCall` | gaps-render-fe | high |
| 0x0046d7e0 | `etgAlternateComplete` | gaps-render-fe | high |
| 0x0046d9e0 | `etgTimeBlockComplete` | gaps-render-fe | high |
| 0x0046d9f0 | `etgTimeBlockOpen` | gaps-render-fe | high |
| 0x0046dce0 | `etgAtComplete` | gaps-render-fe | high |
| 0x0046dd00 | `etgBetweenComplete` | gaps-render-fe | high |
| 0x0046e150 | `etgSpawnParse` | gaps-render-fe | high |
| 0x0046e2e0 | `etgCreationCallback` | gaps-render-fe | high |
| 0x0046e390 | `etgCreateCallbackSetup` | gaps-render-fe | high |
| 0x0046e3b0 | `etgCallbackOpen` | gaps-render-fe | high |
| 0x0046e450 | `etgCallbackClose` | gaps-render-fe | high |
| 0x0046e490 | `etgCreationResolve` | gaps-render-fe | high |
| 0x0046e760 | `etgNOP` | gaps-render-fe | high |
| 0x0046e870 | `etgEnd` | gaps-render-fe | high |
| 0x0046e880 | `etgDelete` | gaps-render-fe | high |
| 0x0046ed30 | `etgCallTo` | gaps-render-fe | high |
| 0x0046ed80 | `etgReturnTo` | gaps-render-fe | high |
| 0x0046edb0 | `etgAlternate` | gaps-render-fe | high |
| 0x0046ee90 | `etgAt` | gaps-render-fe | high |
| 0x004704e0 | `etgCreateEffects` | gaps-render-fe | high |
| 0x00470c50 | `etgSpawnNewEffect` | gaps-render-fe | high |
| 0x00471170 | `etgThisOffsetXYZ` | gaps-render-fe | high |
| 0x0047c000 | `flightmanSlalomExecute` | gaps-ai-flight | high |
| 0x0047c180 | `flightmanSandwichExecute` | gaps-ai-flight | high |
| 0x0047c720 | `flightmanCelebFlipInit` | gaps-ai-flight | medium |
| 0x0047c7d0 | `flightmanWhipStrafeInit` | gaps-ai-flight | high |
| 0x0047d4c0 | `flightmanLoYoYoInit` | gaps-ai-flight | medium |
| 0x0047d8e0 | `flightmanSideStepExecute` | gaps-ai-flight | high |
| 0x004b0020 | `meshAlmostCurrentMaterial` | gaps-render-fe | medium |
| 0x004b68d0 | `madBindingsPost` | gaps-render-fe | medium |
| 0x004b68f0 | `madBindingListCallback` | gaps-render-fe | medium |
| 0x004e2690 | `partModifyExponent` | gaps-render-fe | high |
| 0x004e2b40 | `cat_partModifyScaleX` | gaps-render-fe | high |
| 0x004e3050 | `partModifyDeltaVelR` | gaps-render-fe | high |
| 0x004e33d0 | `partModifyMorph` | gaps-render-fe | high |
| 0x004e3670 | `partModifyPseudoBillboard` | gaps-render-fe | high |
| 0x004e3710 | `partModifyColorScheme` | gaps-render-fe | high |
| 0x004e39b0 | `partSetIsWorldspace` | gaps-render-fe | high |
| 0x004e3be0 | `partSetXYZScale` | gaps-render-fe | high |
| 0x004e4000 | `partSetColorADist` | gaps-render-fe | high |
| 0x004e40d0 | `partSetDeltaColorA` | gaps-render-fe | high |
| 0x004e4120 | `partSetDeltaColorADist` | gaps-render-fe | high |
| 0x00503410 | `SaveResourceVolumeCB` | gaps-input-save-sp | low |
| 0x005b0490 | `DefenseFighterHouseKeep` | gaps-ai-flight | medium |
| 0x005b0840 | `DefenseFighter_Load` | gaps-ai-flight | high |
| 0x005c9da0 | `JpegErrorExit` | libs-unknown | low |

## Dropped (1)

| address | name | set | reason |
|---|---|---|---|
| 0x0043453b | `` | gaps-ai-flight | empty name / spurious function entry inside recievedChatPacketCB; producer asks for removal (not a symbol, left for a Ghidra stage) |

The auditors already removed the sampled-wrong entries from the verified files: 0x00464660 dockFindNearestDockPoint (ai-flight), 0x005025e0 Load_CommandLayer and 0x004a8f00 cat_moShipOutlinesDraw. The last two return as the auditor corrections above.

## Not changed on purpose

- **Class-only libs-unknown entries (404).** The round-1 `lib_magick_*`, `lib_ijg_*` and `lib_cms_*` placeholders stay. libs-unknown now classes them `lib:imagemagick`, `lib:libjpeg` and `lib:securom` (SecuROM is low confidence), but renaming roughly 400 placeholders would only change their prefix. The exceptions are 0x006fe9b0 and 0x006feae0: they are not ImageMagick (they launch the browser for a WON ad click), so they become `lib_won_*`.
- `std__locale__locale_2` (0x006b8010): libs-unknown proposes a class fix only (crt). The name is unchanged.
- **Mission bodies.** missions-r2 confirmed all 2,085 names, so nothing is emitted for them. Its finding that **984 mission function bodies are truncated** in the Ghidra project (they end at a kasGrowSelectionPtr/kasfShipsUpgrade/kasfShipsCount call; about 292 KB of code is missing and the call graph lacks 7,717 edges) is a Ghidra-stage repair, not a naming task. The list is `ghidra/proposals/r2/missions-r2.body-fix.csv`. The r2 apply will not fix it.
