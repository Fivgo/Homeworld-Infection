# Round-1 symbol reconcile report

Output: `ghidra/symbols_round1.json` (cet_symbols.json schema, 8312 symbols: 8278 function, 34 data). Generated 2026-10-09 from the 9 audited sets `ghidra/proposals/*.verified.json`. `qwen-naming.json` and the un-suffixed producer files were not used (unaudited / superseded by their verified copies).

## Totals

| item | count |
|---|---|
| input `core.verified.json` | 1227 |
| input `hw1-match.verified.json` | 189 |
| input `lib-graph.verified.json` | 5596 |
| input `lib-strings.verified.json` | 5232 |
| input `missions.verified.json` | 537 |
| input `ships.verified.json` | 158 |
| input `sim-cataclysm.verified.json` | 200 |
| input `sim-presentation.verified.json` | 1003 |
| input `sim-universe.verified.json` | 474 |
| distinct valid addresses after rule 5 | 8386 |
| **dropped by rule 5** (address not in functions.csv / symbols.csv+strings.csv) | **925** |
| class-only entries left out because Ghidra already has a non-identifier FID/demangled name | 74 |
| **symbols written** | **8312** |
| name conflicts between sets resolved | 17 |
| class-only conflicts between sets resolved | 71 |
| CET names overridden | 0 |
| name collisions made unique with _2/_3 suffixes | 91 names |
| C++ qualified names flattened (`A::B` -> `A__B`, original kept in `qualified=`) | 623 |

By class (hw1 = all `hw1:<File.c>` together): won 3171, hw1 2249, crt 1912, cat-new 493, unknown 476, bink 8, rgl 3.
By confidence: medium 1195, high 7096, low 21.
How the names arise: 3658 carry a producer name; 2672 class-only library functions got `lib_<class>_<addr>`; 1907 EH funclets got `lib_<class>_Unwind_<addr>` / `lib_<class>_Catch_<addr>` (Ghidra name kept inside); 75 class-only entries keep the valid identifier Ghidra/FID already gave them (re-emitted only so the class is tabulated).
Unknown-class library carvings: lib=ijg 76, lib=magick 360, lib=cms 12 (named `lib_magick_*`, `lib_ijg_*`, `lib_cms_*`; `class=unknown` is kept because the class enum has no slot for them).

## Rules as applied

- **Inclusion (rule 1).** An address is written if any set names it, or if its class is a library class: crt, won, bink, rgl, or `unknown` whose evidence starts `lib:` and names one library (ImageMagick/Magick++, IJG libjpeg, the .cms_t copy-protection stub). Every class-only entry had a library class, so none were left out for lacking one.
- **Class-only entries on already-named functions.** If Ghidra already gives the function a valid C identifier (FID names such as `_strncmp`), that name is re-emitted unchanged. Ghidra `Unwind@xxxxxxxx` / `Catch@xxxxxxxx` funclet names become `lib_<class>_Unwind_xxxxxxxx`. Other non-identifier FID/demangler names (`FID_conflict:...`, `std::...`, templates, `@N` decorations) are **not** emitted (74 entries, listed below), so a good Ghidra name is never swapped for a worse synthetic one.
- **Name precedence (rule 2).** CET name, then an audited-high hw1 name with hw1_equivalent, then higher confidence, then the number of sets that agree, then the more specific (longer, non-generic) name. Two evidence-based exceptions, both checked against the decompile and the HW1 source, are listed under conflicts.
- **CET protection (rule 3).** A CET name (cet_symbols.json function/data) is replaced only if two different sets agree on one other name, both at high confidence. That never happened: **0 overrides**. Only 4 CET addresses appear in the output: at two of them hw1-match proposes the same name as CET (singlePlayerGameUpdate, scriptSetTweakableGlobals), and at the other two lib-strings gives only a class (crt), to CET's `memAlloc`/`memFree`.
- **MSVC decoration.** Where two sets named the same CRT function and the names differ only by the cdecl leading underscore (`sprintf` vs `_sprintf`, `_mtinit` vs `__mtinit`), the sets were treated as agreeing. The decorated linker spelling was kept, which is the convention of Ghidra's existing FID names (`_strncmp`, `__ftol`, `__malloc_dbg`) and of the 1,755 lib-strings CRT names.
- **Class conflicts.** The game's own hw1 wrapper wins over a library class (0x559500 glCapGetRGLAddresses, `hw1:glcaps.c` over `rgl`). For crt vs won, crt is chosen: these are C++ std-library/CRT COMDATs instantiated inside WON objects, so the byte match to the Titan `.lib` shows where they were linked, not who wrote them. Both producers already class that library's own block (0x705ca0-0x71cbf0) crt. For crt vs unknown, the std-lib COMDAT call evidence is more specific than the ImageMagick block range. For unknown vs won, the higher confidence wins (lib-strings medium vs lib-graph low).
- **Class spelling.** `hw1:<file>` is normalised to the case of the HomeworldSDL file (8 entries): 0x00462110 hw1:crc32.c->hw1:CRC32.c, 0x00462150 hw1:crc32.c->hw1:CRC32.c, 0x0055eb60 hw1:light.c->hw1:Light.c, 0x0052ef80 hw1:transformer.c->hw1:Transformer.c, 0x0052f110 hw1:transformer.c->hw1:Transformer.c, 0x005a49d0 hw1:trails.c->hw1:Trails.c, 0x005a4ac0 hw1:trails.c->hw1:Trails.c, 0x004b7130 hw1:Mex.c->hw1:MEX.c.
- **Uniqueness (rule 4).** Names are unique across this file, all CET names (patch sites included), and every non-default function/IMPORTED/USER_DEFINED symbol name Ghidra keeps at addresses this file does not touch. Who keeps the bare name: an evidence pick first (only 0x5ab6b0), then CET, then higher confidence, then hw1 class, then the lower address.
- **Validity (rule 5).** A function address must be an entry in functions.csv. A data address must appear in symbols.csv or strings.csv.
- `notes` always starts `class=<class>; confidence=<c>; sets=<a+b>`. Optional `lib=`, `qualified=`, `unique-suffix-of=`, `carved=`, `resolved=` and `evidence=` follow. `type` holds the winning set's signature. `target` carries image_base and the sha256 of build 12948, so ImportSymbolsJson will refuse a different exe.

## Name conflicts resolved (17)

| address | candidates (sets: name, confidence) | chosen | reason |
|---|---|---|---|
| 0x00421860 | core: `animBinkPlay` (medium); hw1-match: `animAviPlay` (high) | `animBinkPlay` | evidence exception: body builds 'Movies\%s.bik' and calls the Bink wrapper 0x551230 (class bink, binkInit): that is ORIG animatic.c animBinkPlay (Relic source, src/Game/animatic.c:524-532). HomeworldSDL keeps animBinkPlay (Animatic.c:688, #if 0) and adds animAviPlay (Animatic.c:769) as the SDL port's AVI replacement, a different function, so the hw1-match name names the port's substitute, not this compiled body |
| 0x00518f50 | core: `HyperspaceRollCallBegin` (medium); hw1-match: `spHyperspaceButtonPushed` (high) | `spHyperspaceButtonPushed` | audited-high hw1 name with hw1_equivalent beats the alternative |
| 0x0052b800 | core: `teStartup` (high); hw1-match: `teReset` (high) | `teReset` | evidence exception: both high hw1; the tie-break 'more specific' would pick teStartup, but the body is teReset's (scriptSet TeamColors.script, race defaults, R2 swap; Teams.c:157) and 3 of its 4 callers are the HW1 teReset call sites cpAcceptColors 0x440c00 (ColPick.c:719) and levelPreInit 0x4a3230 (LevelLoad.c:2206), plus gameEnd 0x5a9f40; teStartup is a one-line teReset() wrapper (Teams.c), so whether utyGameSystemsInit's call reaches it through a folded teStartup or Cataclysm calls teReset directly, the compiled body is teReset's |
| 0x005731b0 | core: `Enqueue` (medium); hw1-match: `HWEnqueue` (high) | `HWEnqueue` | audited-high hw1 name with hw1_equivalent beats the alternative |
| 0x00573250 | core: `Dequeue` (medium); hw1-match: `HWDequeue` (high) | `HWDequeue` | audited-high hw1 name with hw1_equivalent beats the alternative |
| 0x00670a50 | lib-graph: `_fpmath` (medium); lib-strings: `__fpmath` (medium) | `__fpmath` | same CRT function, spellings differ only by MSVC cdecl decoration; kept the decorated linker name, the convention of Ghidra's FID names (_strncmp, __ftol) and of the 1,755 lib-strings CRT entries |
| 0x00670a90 | lib-graph: `_cfltcvt_init` (medium); lib-strings: `__cfltcvt_init` (high) | `__cfltcvt_init` | same CRT function, spellings differ only by MSVC cdecl decoration; kept the decorated linker name, the convention of Ghidra's FID names (_strncmp, __ftol) and of the 1,755 lib-strings CRT entries |
| 0x00670b30 | hw1-match: `sprintf` (high); lib-strings: `_sprintf` (medium) | `_sprintf` | same CRT function, spellings differ only by MSVC cdecl decoration; kept the decorated linker name, the convention of Ghidra's FID names (_strncmp, __ftol) and of the 1,755 lib-strings CRT entries |
| 0x00670d30 | hw1-match: `sscanf` (high); lib-strings: `_sscanf` (high) | `_sscanf` | same CRT function, spellings differ only by MSVC cdecl decoration; kept the decorated linker name, the convention of Ghidra's FID names (_strncmp, __ftol) and of the 1,755 lib-strings CRT entries |
| 0x00672430 | hw1-match: `fseek` (high); lib-strings: `_fseek` (medium) | `_fseek` | same CRT function, spellings differ only by MSVC cdecl decoration; kept the decorated linker name, the convention of Ghidra's FID names (_strncmp, __ftol) and of the 1,755 lib-strings CRT entries |
| 0x00674eb0 | hw1-match: `strftime` (high); lib-strings: `_strftime` (medium) | `_strftime` | same CRT function, spellings differ only by MSVC cdecl decoration; kept the decorated linker name, the convention of Ghidra's FID names (_strncmp, __ftol) and of the 1,755 lib-strings CRT entries |
| 0x0067bbe0 | lib-graph: `WinMainCRTStartup` (high); lib-strings: `_WinMainCRTStartup` (high) | `_WinMainCRTStartup` | same CRT function, spellings differ only by MSVC cdecl decoration; kept the decorated linker name, the convention of Ghidra's FID names (_strncmp, __ftol) and of the 1,755 lib-strings CRT entries |
| 0x0067fc50 | lib-graph: `_mtinit` (high); lib-strings: `__mtinit` (high) | `__mtinit` | same CRT function, spellings differ only by MSVC cdecl decoration; kept the decorated linker name, the convention of Ghidra's FID names (_strncmp, __ftol) and of the 1,755 lib-strings CRT entries |
| 0x00686100 | lib-graph: `_ioinit` (high); lib-strings: `__ioinit` (high) | `__ioinit` | same CRT function, spellings differ only by MSVC cdecl decoration; kept the decorated linker name, the convention of Ghidra's FID names (_strncmp, __ftol) and of the 1,755 lib-strings CRT entries |
| 0x0068c260 | lib-graph: `_setenvp` (high); lib-strings: `__setenvp` (high) | `__setenvp` | same CRT function, spellings differ only by MSVC cdecl decoration; kept the decorated linker name, the convention of Ghidra's FID names (_strncmp, __ftol) and of the 1,755 lib-strings CRT entries |
| 0x0068c3b0 | lib-graph: `_setargv` (high); lib-strings: `__setargv` (high) | `__setargv` | same CRT function, spellings differ only by MSVC cdecl decoration; kept the decorated linker name, the convention of Ghidra's FID names (_strncmp, __ftol) and of the 1,755 lib-strings CRT entries |
| 0x0068c8c0 | lib-graph: `__crtGetEnvironmentStringsA` (high); lib-strings: `___crtGetEnvironmentStringsA` (high) | `___crtGetEnvironmentStringsA` | same CRT function, spellings differ only by MSVC cdecl decoration; kept the decorated linker name, the convention of Ghidra's FID names (_strncmp, __ftol) and of the 1,755 lib-strings CRT entries |

Notes: for 0x005731b0/0x00573250, the HomeworldSDL names `HWEnqueue`/`HWDequeue` win by confidence. The original Relic names are `Enqueue`/`Dequeue` (orig src/Win32/Queue.c:93,136). 0x00421860 and 0x0052b800 are the two evidence exceptions to the mechanical order.

## CET overrides (0)

None. No two independent high-confidence sets proposed a different name at any CET address, and no set proposed one at all.

**Flag for a later ruling:** CET names CRT `free` (0x00673080) `memFree`, though its own note says it is `rinMemFree`/CRT free. core and hw1-match agree at high confidence that HW1 Memory.c `memFree` is **0x004ae130** (406 callers; the pool/growth-heap free). Rule 3 keeps CET's name, so the real memFree is written as `memFree_2`. Renaming CET's entry (to `rinMemFree`, its hw1_equivalent) in cet_symbols.json would free the name.

## Existing names this file replaces

Names that are not CET symbols but are not plain `FUN_*` either:

| address | current Ghidra name | new name | sets |
|---|---|---|---|
| 0x0049ae30 | `seekdir` (PE export, ICF-folded stub) | `nullsub` | ships |
| 0x006fe9a0 | `telldir` (PE export, ICF-folded stub) | `nullsubReturnFalse` | ships |
| 0x005631f0 | `InitWindow` (ANALYSIS, CET_container) | `WinMain` | core,hw1-match,lib-graph |
| 0x00673b00 | `FID_conflict:AtlIsValidAddress` (ANALYSIS) | `__CrtIsValidPointer` | lib-strings |
| 0x00678d60 | `FID_conflict:___CxxFrameHandler3` (ANALYSIS) | `___CxxFrameHandler` | lib-strings |
| 0x00679280 | `exception::exception` (ANALYSIS) | `exception__exception` | lib-strings |
| 0x006792f0 | `exception::exception` (ANALYSIS) | `exception__exception` | lib-strings |
| 0x00679460 | `__CxxThrowException@8` (ANALYSIS) | `__CxxThrowException` | lib-strings |
| 0x0067a0a0 | `lpStartAddress_0067a0a0` (USER_DEFINED) | `_threadstart` | lib-strings |
| 0x0067bbe0 | `entry` (IMPORTED) | `_WinMainCRTStartup` | lib-graph,lib-strings |
| 0x0071cdc0 | `lpStartAddress_0071cdc0` (USER_DEFINED) | `_threadstartex` | lib-strings |

Both PE exports (seekdir 0x49ae30, telldir 0x6fe9a0) are ImageMagick export labels that ICF folded onto a shared RET / return-0 stub. The ships audit kept the generic names, and `Ordinal_16`/`Ordinal_17` remain as labels. `InitWindow` was a CET_container name taken from patch sites; three sets agree the function is WinMain.

## Class conflicts resolved (71)

- crt vs won -> **crt**: 62
- crt vs unknown -> **crt**: 4
- unknown vs won -> **unknown**: 4
- hw1:glcaps.c vs rgl -> **hw1:glcaps.c**: 1

| address | candidates (set:class:confidence) | chosen |
|---|---|---|
| 0x00559500 | lib-strings:rgl:high, sim-presentation:hw1:glcaps.c:high | hw1:glcaps.c |
| 0x00695b00 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00695b30 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00695b70 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00695b90 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00695e10 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x00695eb2 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x00695ede | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x006960c0 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x00696140 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x006962fa | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x00696323 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x006963d0 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00696730 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00696ca0 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x00697590 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006975b0 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006975d0 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00697630 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00697650 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00697a50 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00697ad0 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006985c0 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006986f0 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006ad940 | lib-graph:crt:medium, lib-strings:won:medium | crt |
| 0x006ae260 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x006ae3a9 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x006ae3d2 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x006ae920 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006ae980 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x006af320 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006af470 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006af580 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x006af7e0 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006b1fa0 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006b4690 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006b63a0 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006b63d0 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006b6420 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006b64e0 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006b7210 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x006b7b30 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006b7c80 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006b7ca0 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006b7d80 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006b81b0 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006b82a0 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x006e4550 | lib-graph:crt:high, lib-strings:unknown:medium | crt |
| 0x006e4570 | lib-graph:crt:medium, lib-strings:unknown:medium | crt |
| 0x006e45c0 | lib-graph:crt:high, lib-strings:unknown:medium | crt |
| 0x006ead40 | lib-graph:crt:medium, lib-strings:unknown:medium | crt |
| 0x006ead70 | lib-graph:won:low, lib-strings:unknown:medium | unknown |
| 0x006ead80 | lib-graph:won:low, lib-strings:unknown:medium | unknown |
| 0x006eadb0 | lib-graph:won:low, lib-strings:unknown:medium | unknown |
| 0x006eadc0 | lib-graph:won:low, lib-strings:unknown:medium | unknown |
| 0x00721330 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x00721350 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x00721370 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00721430 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00721438 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00721450 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00721470 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00722fe0 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x00722fe8 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x00722ff0 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x00722ff8 | lib-graph:crt:medium, lib-strings:won:high | crt |
| 0x00723680 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x007237e0 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x007237e8 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00723870 | lib-graph:crt:high, lib-strings:won:high | crt |
| 0x00723890 | lib-graph:crt:high, lib-strings:won:high | crt |

## Uniqueness collisions (91 names)

Two are cross-set and were checked by hand:

- `utyGameQuitToMain`: **0x005ab6b0** (sim-universe) is HW1 utility.c:3059 line for line (gameEnd, credits / "Main_game_screen", network branch), called by univUpdate and universeUpdateTask, so it keeps the name. 0x005ab5b0 (sim-presentation) holds the FE "QuitToMain" table slot. It is a Cataclysm wrapper: an early-out when not single-player, then an inlined copy of the same body. It becomes `utyGameQuitToMain_2`, and a later pass should give it a cat_ name.
- `memFree`: see CET overrides above.

The rest are C++ overloads and multiply-instantiated library functions within the library sets (constructors, `Init`, `do_compare`, ...), which is expected.

| name | entries (address -> written name, sets, confidence) |
|---|---|
| `ArrayUnwindFilter` | 0x0067a680 -> `ArrayUnwindFilter` (lib-graph,lib-strings, high); 0x006898c0 -> `ArrayUnwindFilter_2` (lib-graph,lib-strings, high) |
| `CryptoPP__BufferedTransformation__TransferTo` | 0x006be4e0 -> `CryptoPP__BufferedTransformation__TransferTo` (lib-strings, high); 0x006be580 -> `CryptoPP__BufferedTransformation__TransferTo_2` (lib-strings, high) |
| `CryptoPP__ByteQueueNode__Get` | 0x006be940 -> `CryptoPP__ByteQueueNode__Get` (lib-strings, high); 0x006be970 -> `CryptoPP__ByteQueueNode__Get_2` (lib-strings, high) |
| `CryptoPP__ByteQueueNode__Put` | 0x006be8c0 -> `CryptoPP__ByteQueueNode__Put` (lib-strings, high); 0x006be8f0 -> `CryptoPP__ByteQueueNode__Put_2` (lib-strings, high) |
| `CryptoPP__CBCPaddedDecryptor__Put` | 0x006be220 -> `CryptoPP__CBCPaddedDecryptor__Put` (lib-strings, high); 0x006be250 -> `CryptoPP__CBCPaddedDecryptor__Put_2` (lib-strings, high) |
| `CryptoPP__CBCPaddedEncryptor__Put` | 0x006be050 -> `CryptoPP__CBCPaddedEncryptor__Put` (lib-strings, high); 0x006be080 -> `CryptoPP__CBCPaddedEncryptor__Put_2` (lib-strings, high) |
| `CryptoPP__DERLengthEncode` | 0x006c3c90 -> `CryptoPP__DERLengthEncode` (lib-strings, high); 0x006c3d00 -> `CryptoPP__DERLengthEncode_2` (lib-strings, high) |
| `CryptoPP__Integer__Integer` | 0x006c7230 -> `CryptoPP__Integer__Integer` (lib-strings, high); 0x006c7260 -> `CryptoPP__Integer__Integer_2` (lib-strings, high); 0x006c72f0 -> `CryptoPP__Integer__Integer_3` (lib-strings, high); 0x006c7350 -> `CryptoPP__Integer__Integer_4` (lib-strings, high); 0x006c73c0 -> `CryptoPP__Integer__Integer_5` (lib-strings, high); 0x006c7420 -> `CryptoPP__Integer__Integer_6` (lib-strings, high); 0x006c7bb0 -> `CryptoPP__Integer__Integer_7` (lib-strings, high) |
| `CryptoPP__Integer__InverseMod` | 0x006ca8a0 -> `CryptoPP__Integer__InverseMod` (lib-strings, high); 0x006cac00 -> `CryptoPP__Integer__InverseMod_2` (lib-strings, high) |
| `CryptoPP__Integer__Randomize` | 0x006c82c0 -> `CryptoPP__Integer__Randomize` (lib-strings, high); 0x006c8370 -> `CryptoPP__Integer__Randomize_2` (lib-strings, high); 0x006c8410 -> `CryptoPP__Integer__Randomize_3` (lib-strings, high) |
| `CryptoPP__ModExpPrecomputation__ModExpPrecomputation` | 0x006c0050 -> `CryptoPP__ModExpPrecomputation__ModExpPrecomputation` (lib-strings, high); 0x006c00b0 -> `CryptoPP__ModExpPrecomputation__ModExpPrecomputation_2` (lib-strings, high); 0x006bff00 -> `CryptoPP__ModExpPrecomputation__ModExpPrecomputation_3` (lib-strings, medium) |
| `CryptoPP__NRDigestSigner__NRDigestSigner` | 0x006bc520 -> `CryptoPP__NRDigestSigner__NRDigestSigner` (lib-strings, high); 0x006bf3f0 -> `CryptoPP__NRDigestSigner__NRDigestSigner_2` (lib-strings, high); 0x006bf800 -> `CryptoPP__NRDigestSigner__NRDigestSigner_3` (lib-strings, high) |
| `Init` | 0x0070cf50 -> `Init` (lib-graph,lib-strings, high); 0x0070cfc0 -> `Init_2` (lib-graph,lib-strings, high); 0x0070e710 -> `Init_3` (lib-graph,lib-strings, high); 0x007191d0 -> `Init_4` (lib-graph,lib-strings, high) |
| `WONCommon__DataObject__DataObject` | 0x006b58e0 -> `WONCommon__DataObject__DataObject` (lib-strings, high); 0x006b5960 -> `WONCommon__DataObject__DataObject_2` (lib-strings, high); 0x006b5a00 -> `WONCommon__DataObject__DataObject_3` (lib-strings, high); 0x006b5b60 -> `WONCommon__DataObject__DataObject_4` (lib-strings, high) |
| `WONCommon__EventLog__Log` | 0x006b69d0 -> `WONCommon__EventLog__Log` (lib-strings, high); 0x006b6a10 -> `WONCommon__EventLog__Log_2` (lib-strings, medium) |
| `WONCommon__MemoryException__MemoryException` | 0x006b4c20 -> `WONCommon__MemoryException__MemoryException` (lib-strings, high); 0x006b4c70 -> `WONCommon__MemoryException__MemoryException_2` (lib-strings, high) |
| `WONCommon__RegKey__GetValue` | 0x006e35f0 -> `WONCommon__RegKey__GetValue` (lib-strings, high); 0x006e3750 -> `WONCommon__RegKey__GetValue_2` (lib-strings, high) |
| `WONCommon__RegKey__SetValue` | 0x006e37b0 -> `WONCommon__RegKey__SetValue` (lib-strings, high); 0x006e3800 -> `WONCommon__RegKey__SetValue_2` (lib-strings, high) |
| `WONCommon__StringToWString` | 0x006b4ef0 -> `WONCommon__StringToWString` (lib-strings, high); 0x006b5020 -> `WONCommon__StringToWString_2` (lib-strings, high); 0x006b5170 -> `WONCommon__StringToWString_3` (lib-strings, high) |
| `WONCrypt__BFSymmetricKey__BFSymmetricKey` | 0x006b85b0 -> `WONCrypt__BFSymmetricKey__BFSymmetricKey` (lib-strings, high); 0x006b8570 -> `WONCrypt__BFSymmetricKey__BFSymmetricKey_2` (lib-strings, medium) |
| `WONCrypt__BFSymmetricKey__Create` | 0x006b86c0 -> `WONCrypt__BFSymmetricKey__Create` (lib-strings, medium); 0x006b8780 -> `WONCrypt__BFSymmetricKey__Create_2` (lib-strings, medium) |
| `WONCrypt__CryptKeyBase__CryptKeyBase` | 0x006bd450 -> `WONCrypt__CryptKeyBase__CryptKeyBase` (lib-strings, high); 0x006bd4b0 -> `WONCrypt__CryptKeyBase__CryptKeyBase_2` (lib-strings, high) |
| `WONCrypt__EGPublicKey__EGPublicKey` | 0x006b90f0 -> `WONCrypt__EGPublicKey__EGPublicKey` (lib-strings, high); 0x006b9150 -> `WONCrypt__EGPublicKey__EGPublicKey_2` (lib-strings, high) |
| `WONMisc__EasySocket__connect` | 0x006d2e00 -> `WONMisc__EasySocket__connect` (lib-strings, high); 0x006d2e50 -> `WONMisc__EasySocket__connect_2` (lib-strings, high); 0x006d2d60 -> `WONMisc__EasySocket__connect_3` (lib-strings, medium) |
| `WONMisc__SocketPipe__RemoveCompletedCmd` | 0x006d06e0 -> `WONMisc__SocketPipe__RemoveCompletedCmd` (lib-strings, high); 0x006d0810 -> `WONMisc__SocketPipe__RemoveCompletedCmd_2` (lib-strings, medium) |
| `WONMsg__AddresseeList__AddresseeList` | 0x006a3960 -> `WONMsg__AddresseeList__AddresseeList` (lib-strings, high); 0x006a3a20 -> `WONMsg__AddresseeList__AddresseeList_2` (lib-strings, high) |
| `WONMsg__BaseMessage__BaseMessage` | 0x00694cb0 -> `WONMsg__BaseMessage__BaseMessage` (lib-strings, high); 0x00694ce0 -> `WONMsg__BaseMessage__BaseMessage_2` (lib-strings, high) |
| `WONMsg__MMsgCommPingReply__MMsgCommPingReply` | 0x00699a40 -> `WONMsg__MMsgCommPingReply__MMsgCommPingReply` (lib-strings, high); 0x00699ba0 -> `WONMsg__MMsgCommPingReply__MMsgCommPingReply_2` (lib-strings, high); 0x00699c70 -> `WONMsg__MMsgCommPingReply__MMsgCommPingReply_3` (lib-strings, high) |
| `WONMsg__MMsgCommPing__MMsgCommPing` | 0x006997e0 -> `WONMsg__MMsgCommPing__MMsgCommPing` (lib-strings, high); 0x006998d0 -> `WONMsg__MMsgCommPing__MMsgCommPing_2` (lib-strings, high); 0x00699930 -> `WONMsg__MMsgCommPing__MMsgCommPing_3` (lib-strings, high) |
| `WONMsg__MMsgRoutingBaseDataObject__MMsgRoutingBaseDataObject` | 0x0069b7a0 -> `WONMsg__MMsgRoutingBaseDataObject__MMsgRoutingBaseDataObject` (lib-strings, high); 0x0069ba10 -> `WONMsg__MMsgRoutingBaseDataObject__MMsgRoutingBaseDataObject_2` (lib-strings, high); 0x0069b8a0 -> `WONMsg__MMsgRoutingBaseDataObject__MMsgRoutingBaseDataObject_3` (lib-strings, medium) |
| `WONMsg__MMsgRoutingCreateDataObject__MMsgRoutingCreateDataObject` | 0x0069c8d0 -> `WONMsg__MMsgRoutingCreateDataObject__MMsgRoutingCreateDataObject` (lib-strings, high); 0x0069ca20 -> `WONMsg__MMsgRoutingCreateDataObject__MMsgRoutingCreateDataObject_2` (lib-strings, high) |
| `WONMsg__MMsgRoutingPeerChat__MMsgRoutingPeerChat` | 0x0069ee40 -> `WONMsg__MMsgRoutingPeerChat__MMsgRoutingPeerChat` (lib-strings, high); 0x0069eeb0 -> `WONMsg__MMsgRoutingPeerChat__MMsgRoutingPeerChat_2` (lib-strings, high) |
| `WONMsg__MMsgRoutingPeerData__MMsgRoutingPeerData` | 0x0069f400 -> `WONMsg__MMsgRoutingPeerData__MMsgRoutingPeerData` (lib-strings, high); 0x0069f290 -> `WONMsg__MMsgRoutingPeerData__MMsgRoutingPeerData_2` (lib-strings, medium) |
| `WONMsg__MMsgRoutingReadDataObjectReply__MMsgRoutingReadDataObjectReply` | 0x0069ff10 -> `WONMsg__MMsgRoutingReadDataObjectReply__MMsgRoutingReadDataObjectReply` (lib-strings, high); 0x006a0090 -> `WONMsg__MMsgRoutingReadDataObjectReply__MMsgRoutingReadDataObjectReply_2` (lib-strings, high) |
| `WONMsg__MMsgRoutingRegisterClientReply__MMsgRoutingRegisterClientReply` | 0x006a0fc0 -> `WONMsg__MMsgRoutingRegisterClientReply__MMsgRoutingRegisterClientReply` (lib-strings, high); 0x006a1100 -> `WONMsg__MMsgRoutingRegisterClientReply__MMsgRoutingRegisterClientReply_2` (lib-strings, high) |
| `WONMsg__MMsgRoutingRegisterClient__MMsgRoutingRegisterClient` | 0x006a0870 -> `WONMsg__MMsgRoutingRegisterClient__MMsgRoutingRegisterClient` (lib-strings, high); 0x006a0a00 -> `WONMsg__MMsgRoutingRegisterClient__MMsgRoutingRegisterClient_2` (lib-strings, high) |
| `WONMsg__MMsgRoutingReplaceDataObject__MMsgRoutingReplaceDataObject` | 0x006a1540 -> `WONMsg__MMsgRoutingReplaceDataObject__MMsgRoutingReplaceDataObject` (lib-strings, high); 0x006a1680 -> `WONMsg__MMsgRoutingReplaceDataObject__MMsgRoutingReplaceDataObject_2` (lib-strings, high); 0x006a17e0 -> `WONMsg__MMsgRoutingReplaceDataObject__MMsgRoutingReplaceDataObject_3` (lib-strings, high) |
| `WONMsg__MMsgRoutingSendChat__MMsgRoutingSendChat` | 0x006a1ae0 -> `WONMsg__MMsgRoutingSendChat__MMsgRoutingSendChat` (lib-strings, high); 0x006a1be0 -> `WONMsg__MMsgRoutingSendChat__MMsgRoutingSendChat_2` (lib-strings, high) |
| `WONMsg__MMsgRoutingSendDataBroadcast__MMsgRoutingSendDataBroadcast` | 0x006a24e0 -> `WONMsg__MMsgRoutingSendDataBroadcast__MMsgRoutingSendDataBroadcast` (lib-strings, high); 0x006a2630 -> `WONMsg__MMsgRoutingSendDataBroadcast__MMsgRoutingSendDataBroadcast_2` (lib-strings, high) |
| `WONMsg__MMsgRoutingSendData__MMsgRoutingSendData` | 0x006a1eb0 -> `WONMsg__MMsgRoutingSendData__MMsgRoutingSendData` (lib-strings, high); 0x006a2050 -> `WONMsg__MMsgRoutingSendData__MMsgRoutingSendData_2` (lib-strings, high) |
| `WONMsg__MMsgRoutingStatusReply__MMsgRoutingStatusReply` | 0x006a2ae0 -> `WONMsg__MMsgRoutingStatusReply__MMsgRoutingStatusReply` (lib-strings, medium); 0x006a2b10 -> `WONMsg__MMsgRoutingStatusReply__MMsgRoutingStatusReply_2` (lib-strings, medium) |
| `WONMsg__MiniMessage__MiniMessage` | 0x00694ef0 -> `WONMsg__MiniMessage__MiniMessage` (lib-strings, high); 0x00694fc0 -> `WONMsg__MiniMessage__MiniMessage_2` (lib-strings, high); 0x00694fa0 -> `WONMsg__MiniMessage__MiniMessage_3` (lib-strings, medium) |
| `WONMsg__RoutingServerMessage__RoutingServerMessage` | 0x006a33d0 -> `WONMsg__RoutingServerMessage__RoutingServerMessage` (lib-strings, medium); 0x006a3420 -> `WONMsg__RoutingServerMessage__RoutingServerMessage_2` (lib-strings, medium) |
| `WONMsg__SMsgCommRegisterRequest__SMsgCommRegisterRequest` | 0x0069a580 -> `WONMsg__SMsgCommRegisterRequest__SMsgCommRegisterRequest` (lib-strings, high); 0x0069aaf0 -> `WONMsg__SMsgCommRegisterRequest__SMsgCommRegisterRequest_2` (lib-strings, high) |
| `WONMsg__SMsgDirG2EntityBase__SMsgDirG2EntityBase` | 0x006afc20 -> `WONMsg__SMsgDirG2EntityBase__SMsgDirG2EntityBase` (lib-strings, high); 0x006afcd0 -> `WONMsg__SMsgDirG2EntityBase__SMsgDirG2EntityBase_2` (lib-strings, high) |
| `WONMsg__SMsgDirG2EntityListBase__SMsgDirG2EntityListBase` | 0x006affc0 -> `WONMsg__SMsgDirG2EntityListBase__SMsgDirG2EntityListBase` (lib-strings, medium); 0x006b0030 -> `WONMsg__SMsgDirG2EntityListBase__SMsgDirG2EntityListBase_2` (lib-strings, medium) |
| `WONMsg__SMsgDirG2KeyedBase__SMsgDirG2KeyedBase` | 0x006b0710 -> `WONMsg__SMsgDirG2KeyedBase__SMsgDirG2KeyedBase` (lib-strings, high); 0x006b0780 -> `WONMsg__SMsgDirG2KeyedBase__SMsgDirG2KeyedBase_2` (lib-strings, high) |
| `WONMsg__SMsgDirG2MultiEntityReply__SMsgDirG2MultiEntityReply` | 0x006b0c60 -> `WONMsg__SMsgDirG2MultiEntityReply__SMsgDirG2MultiEntityReply` (lib-strings, high); 0x006b0cc0 -> `WONMsg__SMsgDirG2MultiEntityReply__SMsgDirG2MultiEntityReply_2` (lib-strings, high) |
| `WONMsg__SMsgDirG2QueryExtendBase__SMsgDirG2QueryExtendBase` | 0x006b0e00 -> `WONMsg__SMsgDirG2QueryExtendBase__SMsgDirG2QueryExtendBase` (lib-strings, high); 0x006b0ef0 -> `WONMsg__SMsgDirG2QueryExtendBase__SMsgDirG2QueryExtendBase_2` (lib-strings, high) |
| `WONMsg__SMsgDirG2QueryGetBase__SMsgDirG2QueryGetBase` | 0x006b1400 -> `WONMsg__SMsgDirG2QueryGetBase__SMsgDirG2QueryGetBase` (lib-strings, high); 0x006b1450 -> `WONMsg__SMsgDirG2QueryGetBase__SMsgDirG2QueryGetBase_2` (lib-strings, high) |
| `WONMsg__SMsgEventRecordEvent__SMsgEventRecordEvent` | 0x006acbd0 -> `WONMsg__SMsgEventRecordEvent__SMsgEventRecordEvent` (lib-strings, high); 0x006acf00 -> `WONMsg__SMsgEventRecordEvent__SMsgEventRecordEvent_2` (lib-strings, high) |
| `WONMsg__SMsgEventTaggedRecordEvent__SMsgEventTaggedRecordEvent` | 0x006adfe0 -> `WONMsg__SMsgEventTaggedRecordEvent__SMsgEventTaggedRecordEvent` (lib-strings, high); 0x006ae0e0 -> `WONMsg__SMsgEventTaggedRecordEvent__SMsgEventTaggedRecordEvent_2` (lib-strings, high) |
| `WONMsg__SMsgFactStartProcessBase__SMsgFactStartProcessBase` | 0x006a5f50 -> `WONMsg__SMsgFactStartProcessBase__SMsgFactStartProcessBase` (lib-strings, high); 0x006a5c70 -> `WONMsg__SMsgFactStartProcessBase__SMsgFactStartProcessBase_2` (lib-strings, medium) |
| `WONMsg__SmallMessage__SmallMessage` | 0x00695270 -> `WONMsg__SmallMessage__SmallMessage` (lib-strings, high); 0x00695320 -> `WONMsg__SmallMessage__SmallMessage_2` (lib-strings, medium) |
| `WONMsg__TMessage__TMessage` | 0x006955a0 -> `WONMsg__TMessage__TMessage` (lib-strings, high); 0x00695640 -> `WONMsg__TMessage__TMessage_2` (lib-strings, medium) |
| `WONMsg__TMsgAuth1Challenge1__TMsgAuth1Challenge1` | 0x006a9550 -> `WONMsg__TMsgAuth1Challenge1__TMsgAuth1Challenge1` (lib-strings, high); 0x006a95c0 -> `WONMsg__TMsgAuth1Challenge1__TMsgAuth1Challenge1_2` (lib-strings, high) |
| `WONMsg__TMsgAuth1Complete__TMsgAuth1Complete` | 0x006a9ac0 -> `WONMsg__TMsgAuth1Complete__TMsgAuth1Complete` (lib-strings, high); 0x006a9b40 -> `WONMsg__TMsgAuth1Complete__TMsgAuth1Complete_2` (lib-strings, high) |
| `WONMsg__TMsgAuth1LoginBase2__TMsgAuth1LoginBase2` | 0x006aa320 -> `WONMsg__TMsgAuth1LoginBase2__TMsgAuth1LoginBase2` (lib-strings, high); 0x006aa370 -> `WONMsg__TMsgAuth1LoginBase2__TMsgAuth1LoginBase2_2` (lib-strings, high) |
| `WONMsg__TMsgAuth1LoginReply__TMsgAuth1LoginReply` | 0x006a81b0 -> `WONMsg__TMsgAuth1LoginReply__TMsgAuth1LoginReply` (lib-strings, high); 0x006a82f0 -> `WONMsg__TMsgAuth1LoginReply__TMsgAuth1LoginReply_2` (lib-strings, high) |
| `WONMsg__TMsgAuth1LoginRequestHW__TMsgAuth1LoginRequestHW` | 0x006aa840 -> `WONMsg__TMsgAuth1LoginRequestHW__TMsgAuth1LoginRequestHW` (lib-strings, high); 0x006aa9d0 -> `WONMsg__TMsgAuth1LoginRequestHW__TMsgAuth1LoginRequestHW_2` (lib-strings, high) |
| `WONMsg__TMsgAuth1Request__TMsgAuth1Request` | 0x006a9290 -> `WONMsg__TMsgAuth1Request__TMsgAuth1Request` (lib-strings, high); 0x006a9380 -> `WONMsg__TMsgAuth1Request__TMsgAuth1Request_2` (lib-strings, high) |
| `WONMsg__TMsgAuthRawBufferBase__TMsgAuthRawBufferBase` | 0x006a9f00 -> `WONMsg__TMsgAuthRawBufferBase__TMsgAuthRawBufferBase` (lib-strings, medium); 0x006a9fb0 -> `WONMsg__TMsgAuthRawBufferBase__TMsgAuthRawBufferBase_2` (lib-strings, medium); 0x006aa020 -> `WONMsg__TMsgAuthRawBufferBase__TMsgAuthRawBufferBase_3` (lib-strings, medium) |
| `WONMsg__TRawData__TRawData` | 0x00694130 -> `WONMsg__TRawData__TRawData` (lib-strings, high); 0x00694170 -> `WONMsg__TRawData__TRawData_2` (lib-strings, high); 0x00694220 -> `WONMsg__TRawData__TRawData_3` (lib-strings, high) |
| `WONMsg__TRawMsg__TRawMsg` | 0x00694ae0 -> `WONMsg__TRawMsg__TRawMsg` (lib-strings, high); 0x00694b40 -> `WONMsg__TRawMsg__TRawMsg_2` (lib-strings, high); 0x00694bb0 -> `WONMsg__TRawMsg__TRawMsg_3` (lib-strings, high) |
| `___add_12` | 0x00691ce0 -> `___add_12` (lib-strings, medium); 0x00c66273 -> `___add_12_2` (lib-strings, medium) |
| `__amsg_exit` | 0x0067bd50 -> `__amsg_exit` (lib-strings, medium); 0x00c611ec -> `__amsg_exit_2` (lib-strings, medium) |
| `__aulldiv` | 0x0067ec50 -> `__aulldiv` (lib-strings, high); 0x00c65240 -> `__aulldiv_2` (lib-strings, medium) |
| `__aullrem` | 0x0067ecc0 -> `__aullrem` (lib-strings, high); 0x00c652b0 -> `__aullrem_2` (lib-strings, medium) |
| `__cfltcvt` | 0x0067c520 -> `__cfltcvt` (lib-strings, medium); 0x00c62a74 -> `__cfltcvt_2` (lib-strings, medium) |
| `__exit` | 0x00678960 -> `__exit` (lib-strings, medium); 0x00c61049 -> `__exit_2` (lib-strings, medium) |
| `__fassign` | 0x0067c040 -> `__fassign` (lib-strings, medium); 0x00c6276f -> `__fassign_2` (lib-strings, medium) |
| `__global_unwind2` | 0x00679040 -> `__global_unwind2` (lib-strings, medium); 0x00c63c34 -> `__global_unwind2_2` (lib-strings, medium) |
| `__local_unwind2` | 0x00679082 -> `__local_unwind2` (lib-strings, medium); 0x00c63c76 -> `__local_unwind2_2` (lib-strings, medium) |
| `__mbsnbicoll` | 0x00689150 -> `__mbsnbicoll` (lib-strings, medium); 0x00c67314 -> `__mbsnbicoll_2` (lib-strings, medium) |
| `__nh_malloc` | 0x00672630 -> `__nh_malloc` (lib-strings, medium); 0x00c60763 -> `__nh_malloc_2` (lib-strings, medium) |
| `_memset` | 0x00681a10 -> `_memset` (lib-strings, medium); 0x00c60690 -> `_memset_2` (lib-strings, medium) |
| `_strchr` | 0x00670e40 -> `_strchr` (lib-strings, medium); 0x00c641c0 -> `_strchr_2` (lib-strings, medium) |
| `_strcmp` | 0x0068a020 -> `_strcmp` (lib-strings, medium); 0x00c60960 -> `_strcmp_2` (lib-strings, medium) |
| `_strlen` | 0x0067ebd0 -> `_strlen` (lib-strings, medium); 0x00c60f90 -> `_strlen_2` (lib-strings, medium) |
| `_strncmp` | 0x00670df0 -> `_strncmp` (lib-strings, medium); 0x00c60e20 -> `_strncmp_2` (lib-strings, medium) |
| `_strncpy` | 0x00676d60 -> `_strncpy` (lib-strings, medium); 0x00c65cf0 -> `_strncpy_2` (lib-strings, medium) |
| `_strstr` | 0x00671690 -> `_strstr` (lib-strings, medium); 0x00c64280 -> `_strstr_2` (lib-strings, medium) |
| `copy_environ` | 0x0068a4c0 -> `copy_environ` (lib-graph,lib-strings, high); 0x0068a910 -> `copy_environ_2` (lib-graph,lib-strings, high) |
| `do_compare` | 0x007103b0 -> `do_compare` (lib-graph,lib-strings, high); 0x00717da0 -> `do_compare_2` (lib-graph,lib-strings, high) |
| `do_get_monthname` | 0x00711150 -> `do_get_monthname` (lib-graph,lib-strings, high); 0x00718b10 -> `do_get_monthname_2` (lib-graph,lib-strings, high) |
| `do_get_weekday` | 0x007110f0 -> `do_get_weekday` (lib-graph,lib-strings, high); 0x00718ab0 -> `do_get_weekday_2` (lib-graph,lib-strings, high) |
| `exception__exception` | 0x00679220 -> `exception__exception` (lib-strings, high); 0x00679280 -> `exception__exception_2` (lib-strings, high); 0x006792f0 -> `exception__exception_3` (lib-strings, medium) |
| `memFree` | 0x00673080 -> `memFree` (cet, high); 0x004ae130 -> `memFree_2` (core,hw1-match, high) |
| `std___Construct` | 0x006a51d0 -> `std___Construct` (lib-strings, high); 0x006a5340 -> `std___Construct_2` (lib-strings, high); 0x006aca20 -> `std___Construct_3` (lib-strings, high); 0x006aca40 -> `std___Construct_4` (lib-strings, high); 0x006afa20 -> `std___Construct_5` (lib-strings, high); 0x006b3410 -> `std___Construct_6` (lib-strings, high); 0x006c2210 -> `std___Construct_7` (lib-strings, high); 0x006c2db0 -> `std___Construct_8` (lib-strings, high); 0x006c3860 -> `std___Construct_9` (lib-strings, high); 0x006db2c0 -> `std___Construct_10` (lib-strings, high); 0x00698fa0 -> `std___Construct_11` (lib-strings, medium); 0x006a7740 -> `std___Construct_12` (lib-strings, medium); 0x006dd150 -> `std___Construct_13` (lib-strings, medium) |
| `std__locale__locale` | 0x006b7d80 -> `std__locale__locale` (lib-strings, high); 0x006b8010 -> `std__locale__locale_2` (lib-strings, medium) |
| `utyGameQuitToMain` | 0x005ab6b0 -> `utyGameQuitToMain` (sim-universe, high); 0x005ab5b0 -> `utyGameQuitToMain_2` (sim-presentation, high) |

## Dropped by rule 5 (925)

All drops are **kind=data**: core 844, ships 57, sim-cataclysm 24. No function proposal was dropped. Every dropped address is inside a mapped block (916 in .data, 9 in .rdata). The cause is that symbols.csv lists only non-DEFAULT symbols, so an unnamed `DAT_` global never appears in it. These are audited names (taskData, the ships' CustShipHeader tables, sim-cataclysm's script tables, ...). The ships audit verified its headers as targets of the shipHeaders pointer table. **Recommendation:** relax rule 5 for data to "inside a mapped non-executable block", or have the export stage dump DEFAULT data labels too, then re-run this reconcile. The full list is below so it can be re-applied.

| set | address | name | class |
|---|---|---|---|
| core | 0x0072bd14 | `cat_timeCompressionFactorTable` | cat-new |
| core | 0x0074a874 | `ATTMAN_BUILDPRIORITY_RATIO` | hw1:AIPlayer.c |
| core | 0x0074a878 | `DEFMAN_BUILDPRIORITY_RATIO` | hw1:AIPlayer.c |
| core | 0x0074a87c | `RESMAN_BUILDPRIORITY_RATIO` | hw1:AIPlayer.c |
| core | 0x0074a880 | `AIPLAYER_OVERKILL_FACTOR` | hw1:AIPlayer.c |
| core | 0x0074a9f4 | `AIA_ENEMYBLOBS_PER_RECON` | hw1:AIPlayer.c |
| core | 0x0074a9f8 | `AID_MOTHERSHIP_DEFENSE_FLEET_STRENGTH` | hw1:AIPlayer.c |
| core | 0x0074a9fc | `AID_RESCON_ASF_DEFENDABLE_MULTIPLIER` | hw1:AIPlayer.c |
| core | 0x0074aa00 | `AID_SPHERE_OF_INFLUENCE_INTERVAL` | hw1:AIPlayer.c |
| core | 0x0074aa04 | `AID_ROVING_DEFENSE_LOW_COUNT_CUTOFF` | hw1:AIPlayer.c |
| core | 0x0074aa08 | `AID_ROVING_DEFENSE_LOW_COUNT_DIVISOR` | hw1:AIPlayer.c |
| core | 0x0074aa0c | `AID_ROVING_DEFENSE_HIGH_COUNT_DIVISOR` | hw1:AIPlayer.c |
| core | 0x0074aa14 | `AID_MOTHERSHIP_ATTACKER_FLEET_RADIUS` | hw1:AIPlayer.c |
| core | 0x0074aa30 | `AIT_DEFTEAM_NEEDS_HELP_STRENGTH` | hw1:AIPlayer.c |
| core | 0x0074aa34 | `AIO_GUARD_SHIPS_NUM_CAPITAL` | hw1:AIPlayer.c |
| core | 0x0074aa38 | `AIO_GUARD_SHIPS_NUM_NONCAPITAL` | hw1:AIPlayer.c |
| core | 0x0074aa44 | `AIO_HARASS_NUMBERS_LOW` | hw1:AIPlayer.c |
| core | 0x0074aa58 | `AIO_FASTROVING_NUMBERS_LOW` | hw1:AIPlayer.c |
| core | 0x0074aa60 | `AIO_SLOWROVING_NUMBERS_LOW` | hw1:AIPlayer.c |
| core | 0x0074aa64 | `AIO_TOUT_MSHIP_FAST_TGRD_FORMATION` | hw1:AIPlayer.c |
| core | 0x0074aa68 | `AIO_TOUT_MSHIP_FAST_TGRD_TACTICS` | hw1:AIPlayer.c |
| core | 0x0074aa6c | `AIO_TOUT_MSHIP_BIG_TGRD_FORMATION` | hw1:AIPlayer.c |
| core | 0x0074aa70 | `AIO_TOUT_MSHIP_BIG_TGRD_TACTICS` | hw1:AIPlayer.c |
| core | 0x0074aa74 | `AIO_TOUT_MSHIP_GUARD_TGRD_FORMATION` | hw1:AIPlayer.c |
| core | 0x0074aa78 | `AIO_TOUT_MSHIP_GUARD_TGRD_TACTICS` | hw1:AIPlayer.c |
| core | 0x0074aa7c | `AIO_TOUT_TARG_WCUR_FORMATION` | hw1:AIPlayer.c |
| core | 0x0074aa84 | `AIO_TOUT_TARG_TGUARD_FORMATION` | hw1:AIPlayer.c |
| core | 0x0074aa88 | `AIO_TOUT_TARG_TGUARD_TACTICS` | hw1:AIPlayer.c |
| core | 0x0074aa8c | `AIO_TOUT_TARG_FANCY_TGRD_FORMATION` | hw1:AIPlayer.c |
| core | 0x0074aa90 | `AIO_TOUT_TARG_FANCY_TGRD_TACTICS` | hw1:AIPlayer.c |
| core | 0x0074aa98 | `AIO_CORVETTE_KAMIKAZE_HEALTH` | hw1:AIPlayer.c |
| core | 0x0074aa9c | `AIH_GENERIC_GETTINGROCKED_STRENGTH` | hw1:AIPlayer.c |
| core | 0x0074aaa0 | `AIH_GENERIC_GETTINGROCKED_FORMATION` | hw1:AIPlayer.c |
| core | 0x0074aaec | `AIM_DOCK_DEFAULT_FORMATION` | hw1:AIPlayer.c |
| core | 0x0074aaf0 | `AIM_DEF_MOTHERSHIP_ATTACK_FORMATION` | hw1:AIPlayer.c |
| core | 0x0074ab48 | `AIU_STANDOFF_NEG_ERROR_ANGLE` | hw1:AIPlayer.c |
| core | 0x0074ab4c | `AIU_STANDOFF_POS_ERROR_ANGLE` | hw1:AIPlayer.c |
| core | 0x0074ab50 | `AIU_STANDOFF_DIST_ERROR_LOW` | hw1:AIPlayer.c |
| core | 0x0074ab54 | `AIU_STANDOFF_DIST_ERROR_HIGH` | hw1:AIPlayer.c |
| core | 0x0074ab58 | `AIU_RESCUE_MULTIPLE_MINSHIPS` | hw1:AIPlayer.c |
| core | 0x0074ab5c | `AIU_RESCUE_ENEMYMULTIPLE_MINSHIPS` | hw1:AIPlayer.c |
| core | 0x0074ab60 | `AIU_RESCUE_NOMULTIPLE_FORMATION` | hw1:AIPlayer.c |
| core | 0x0074ab64 | `AIU_RESCUE_TEAMMULTIPLE_FORMATION` | hw1:AIPlayer.c |
| core | 0x0074ab68 | `AIU_RESCUE_BOTHMULTIPLE_FORMATION` | hw1:AIPlayer.c |
| core | 0x0074ab84 | `AIU_SAFEST_STANDOFF_DIST_MULT` | hw1:AIPlayer.c |
| core | 0x0074ab88 | `AIU_FINDUNARMED_PROTECTION_STRENGTH` | hw1:AIPlayer.c |
| core | 0x0074ab8c | `AIU_FINDATTACKING_ENEMYFLEET_RADIUS` | hw1:AIPlayer.c |
| core | 0x0074ab90 | `AIU_HARASS_PROTECTION_RADIUS` | hw1:AIPlayer.c |
| core | 0x0074ab94 | `AIU_UNARMED_UNDEFENDED_MODIFIER` | hw1:AIPlayer.c |
| core | 0x0074ab98 | `AIU_UNARMED_MODIFIER` | hw1:AIPlayer.c |
| core | 0x0074ab9c | `AIU_FIGHTER_VULNERABLE_MODIFIER` | hw1:AIPlayer.c |
| core | 0x0074d310 | `UPDATE_RU_COUNT_RATE` | hw1:AIResourceMan.c |
| core | 0x0074d314 | `BUILD_RC_CASH_TABLE_BASE` | hw1:AIResourceMan.c |
| core | 0x0074d318 | `BUILD_RC_CASH_TABLE_INC` | hw1:AIResourceMan.c |
| core | 0x0074d31c | `MAX_RCOLLECTORS_PER_DOCK_POINT` | hw1:AIResourceMan.c |
| core | 0x0074d320 | `MAX_RCOLLECTORS_TO_BUILD` | hw1:AIResourceMan.c |
| core | 0x0074d324 | `MIN_WORTHWHILE_RUs` | hw1:AIResourceMan.c |
| core | 0x0074d328 | `RUs_PER_RCOLLECTOR` | hw1:AIResourceMan.c |
| core | 0x0074d32c | `MIN_WORTHWHILE_RUs_FOR_CONTROLLER` | hw1:AIResourceMan.c |
| core | 0x0074d330 | `MIN_RCOLLECTORS_FOR_CONTROLLER` | hw1:AIResourceMan.c |
| core | 0x0074d334 | `BUILD_RCONTROLLER_CASH_BASE` | hw1:AIResourceMan.c |
| core | 0x0074d33c | `BUILD_CARRIER_CASH_BASE` | cat-new |
| core | 0x0074d340 | `BUILD_CARRIER_SU_POINT` | cat-new |
| core | 0x0074d344 | `ASF_POSITION_TO_ENEMY_MOTHERSHIP` | hw1:AIResourceMan.c |
| core | 0x0074d348 | `RCONTROLLER_POS_FACTOR_FRIENDLIES_PRESENT` | hw1:AIResourceMan.c |
| core | 0x0074d34c | `RCONTROLLER_POS_FACTOR_ENEMIESPRESENT_BUT_OUTNUMBERED` | hw1:AIResourceMan.c |
| core | 0x0074d350 | `RCONTROLLER_POS_FACTOR_ENEMIESPRESENT` | hw1:AIResourceMan.c |
| core | 0x0074d354 | `RCONTROLLER_POS_FACTOR_INBETWEEN` | hw1:AIResourceMan.c |
| core | 0x0074d358 | `RCONTROLLER_POS_FACTOR_NOTINBETWEEN` | hw1:AIResourceMan.c |
| core | 0x0074d6a0 | `VELOCITY_SCALE_FACTOR` | hw1:AIShip.c |
| core | 0x0074d6b0 | `MISSILE_USE_VELOCITYPRED_DISTANCE` | hw1:AIShip.c |
| core | 0x0074d6b4 | `AVOID_OBJ_SCALE_FACTOR` | hw1:AIShip.c |
| core | 0x0074d6b8 | `COMBAT_AVOID_OBJ_SCALE_FACTOR` | hw1:AIShip.c |
| core | 0x0074d6c4 | `AVOID_OBJ_PADDING_SCALE` | hw1:AIShip.c |
| core | 0x0074d6c8 | `AVOID_OBJ_PADDING_SCALE_BIG` | hw1:AIShip.c |
| core | 0x0074d6cc | `AVOID_OBJ_ROW_PADDING_SCALE` | hw1:AIShip.c |
| core | 0x0074d6d0 | `AVOID_OBJ_FORMATION_PADDING_SCALE` | hw1:AIShip.c |
| core | 0x0074d6d4 | `AVOID_OBJ_MSHIP_MILPARADE_PADDING_SCALE` | hw1:AIShip.c |
| core | 0x0074d6dc | `AVOID_OBJ_OBSCUREDPOINT_SCALE` | hw1:AIShip.c |
| core | 0x0074d6e0 | `AVOID_SPREAD_OUT_DIST` | hw1:AIShip.c |
| core | 0x0074d6e4 | `CAR_TURN_POWER` | hw1:AIShip.c |
| core | 0x0074d6e8 | `FLYSHIP_HEADINGACCURACY` | hw1:AIShip.c |
| core | 0x0074d6ec | `FLYSHIP_ATTACKACCURACY` | hw1:AIShip.c |
| core | 0x0074d6f4 | `CORVETTE_BANK` | hw1:AIShip.c |
| core | 0x0074d6f8 | `FRIGATE_BANK` | hw1:AIShip.c |
| core | 0x0074d6fc | `DESTROYER_BANK` | hw1:AIShip.c |
| core | 0x0074d700 | `MASSIVESHIP_BANK` | hw1:AIShip.c |
| core | 0x0074d708 | `FRIGATE_TURNPITCH` | hw1:AIShip.c |
| core | 0x0074d70c | `DESTROYER_TURNPITCH` | hw1:AIShip.c |
| core | 0x0074d710 | `MASSIVESHIP_TURNPITCH` | hw1:AIShip.c |
| core | 0x0074d718 | `FRIGATE_DESCENDPITCH` | hw1:AIShip.c |
| core | 0x0074d71c | `DESTROYER_DESCENDPITCH` | hw1:AIShip.c |
| core | 0x0074d720 | `MASSIVESHIP_DESCENDPITCH` | hw1:AIShip.c |
| core | 0x0074d728 | `INTERCEPTORBREAK_TOLERANCE` | hw1:AIShip.c |
| core | 0x0074d72c | `INTERCEPTORBREAK_MINVELOCITY` | hw1:AIShip.c |
| core | 0x0074d734 | `DESCEND_PITCH4_DIST` | hw1:AIShip.c |
| core | 0x0074d738 | `DESCEND_PITCH3_DIST` | hw1:AIShip.c |
| core | 0x0074d73c | `DESCEND_PITCH2_DIST` | hw1:AIShip.c |
| core | 0x0074d740 | `MIN_DIST_FOR_FANCY_DESCEND` | hw1:AIShip.c |
| core | 0x0074d744 | `MIN_ANGLE_FOR_FANCY_DESCEND` | hw1:AIShip.c |
| core | 0x0074d748 | `MIN_DIST_TO_TRACK_HEADING` | hw1:AIShip.c |
| core | 0x0074d74c | `MAX_ANGLE_FOR_TRACK_HEADING` | hw1:AIShip.c |
| core | 0x0074d750 | `ROW_GOING_MOSTLY_UPDOWN_ANGLE` | hw1:AIShip.c |
| core | 0x0074d754 | `AVOID_ROW_SHIP_BY_SCALE` | hw1:AIShip.c |
| core | 0x0074d758 | `AVOID_ROW_SHIP_BY_ADD` | hw1:AIShip.c |
| core | 0x0074d760 | `ROW_GO_DOWN_INSTEAD_OF_UP_RATIO` | hw1:AIShip.c |
| core | 0x0074d764 | `PASS_RIGHT_FACTOR` | hw1:AIShip.c |
| core | 0x0074d768 | `USE_PASS_RIGHT_ANGLE` | hw1:AIShip.c |
| core | 0x0074d770 | `AVOID_MIN_VEL` | hw1:AIShip.c |
| core | 0x0074d774 | `DONTUSEVELOCITYPRED_IFBELOW` | hw1:AIShip.c |
| core | 0x0074e380 | `ERROR_VELOCITY_SQUARED` | hw1:AITrack.c |
| core | 0x0074e384 | `STILL_VELOCITY_LO` | hw1:AITrack.c |
| core | 0x0074e388 | `STILL_VELOCITY_HI` | hw1:AITrack.c |
| core | 0x0074e38c | `STILL_ROT_LO` | hw1:AITrack.c |
| core | 0x0074e390 | `STILL_ROT_HI` | hw1:AITrack.c |
| core | 0x0074e394 | `STABILIZE_Z_LO` | hw1:AITrack.c |
| core | 0x0074e398 | `STABILIZE_Z_HI` | hw1:AITrack.c |
| core | 0x0074e778 | `alodFastMinScale` | hw1:AutoLOD.c |
| core | 0x0074e77c | `alodFastMaxScale` | hw1:AutoLOD.c |
| core | 0x0074e784 | `alodFastTargetDelta` | hw1:AutoLOD.c |
| core | 0x0074e788 | `alodSlowMinScale` | hw1:AutoLOD.c |
| core | 0x0074e78c | `alodSlowMaxScale` | hw1:AutoLOD.c |
| core | 0x0074e794 | `alodSlowTargetDelta` | hw1:AutoLOD.c |
| core | 0x0075071c | `batMouthpieceTries` | hw1:Battle.c |
| core | 0x00750720 | `batStandingFriendlyPower` | hw1:Battle.c |
| core | 0x00750724 | `batStandingFriendlyValue` | hw1:Battle.c |
| core | 0x00750728 | `batStandingEnemyPower` | hw1:Battle.c |
| core | 0x0075072c | `batStandingEnemyValue` | hw1:Battle.c |
| core | 0x00750734 | `batBSNKickingButt` | hw1:Battle.c |
| core | 0x00750738 | `batBSNWinning` | hw1:Battle.c |
| core | 0x0075073c | `batBSNNeutral` | hw1:Battle.c |
| core | 0x00750740 | `batBSNLosing` | hw1:Battle.c |
| core | 0x00750744 | `batBogeyAspect` | hw1:Battle.c |
| core | 0x00750748 | `batRetreatModifier` | hw1:Battle.c |
| core | 0x00751498 | `CompareBigfiles` | hw1:BigFile.c |
| core | 0x007518bc | `CAMERA_ANGLE_ROT` | hw1:Camera.c |
| core | 0x007518c0 | `CAMERA_DECLINATION_ROT` | hw1:Camera.c |
| core | 0x007518c4 | `CAMERA_MAX_DECLINATION` | hw1:Camera.c |
| core | 0x007518c8 | `CAMERA_MIN_DECLINATION` | hw1:Camera.c |
| core | 0x007518cc | `CAMERA_ZOOM_IN` | hw1:Camera.c |
| core | 0x007518d0 | `CAMERA_ZOOM_OUT` | hw1:Camera.c |
| core | 0x007518d4 | `CAMERA_KEY_ZOOM_IN` | hw1:Camera.c |
| core | 0x007518d8 | `CAMERA_KEY_ZOOM_OUT` | hw1:Camera.c |
| core | 0x007518e4 | `CAMERA_CLIP_NEAR` | hw1:Camera.c |
| core | 0x007518e8 | `CAMERA_CLIP_FAR` | hw1:Camera.c |
| core | 0x007518ec | `CAMERA_CLIP_FAR_PLANET` | hw1:Camera.c |
| core | 0x007518f0 | `CAMERA_FIELD_OF_VIEW` | hw1:Camera.c |
| core | 0x007518f4 | `CAMERA_WHEEL_ZOOM_IN` | hw1:Camera.c |
| core | 0x007518f8 | `CAMERA_WHEEL_ZOOM_OUT` | hw1:Camera.c |
| core | 0x007518fc | `CAMERA_MAX_ZOOMOUT_DISTANCE` | hw1:Camera.c |
| core | 0x00751900 | `CAMERA_MIN_ZOOMOUT_DISTANCE` | hw1:Camera.c |
| core | 0x00751904 | `CAMERA_FLOAT_SCALAR0` | hw1:Camera.c |
| core | 0x00751908 | `CAMERA_FLOAT_SCALAR1` | hw1:Camera.c |
| core | 0x0075190c | `CAMERA_FLOAT_DIST_SCALAR` | hw1:Camera.c |
| core | 0x00751910 | `CAMERA_FLOAT_OFFSET_SCALAR0` | hw1:Camera.c |
| core | 0x00751914 | `CAMERA_FLOAT_OFFSET_SCALAR1` | hw1:Camera.c |
| core | 0x00751d38 | `MAX_CAMERA_STACKS` | hw1:CameraCommand.c |
| core | 0x00751d40 | `CAMERA_FAR_DISTANCE` | hw1:CameraCommand.c |
| core | 0x00751d44 | `CAMERA_FOCUSFAR_DISTANCE` | hw1:CameraCommand.c |
| core | 0x00751d48 | `CAMERA_MOTHERSHIPFOCUS_DISTANCE` | hw1:CameraCommand.c |
| core | 0x00751d4c | `CAMERA_MOTHERSHIPFOCUS_ANGLE` | hw1:CameraCommand.c |
| core | 0x00751d50 | `CAMERA_MOTHERSHIPFOCUS_DECLINATION` | hw1:CameraCommand.c |
| core | 0x00751d54 | `GO_CAMERA_DELAY_TIME` | hw1:CameraCommand.c |
| core | 0x00751d7c | `CAM_CUBIC_EVAL_TWEAK` | hw1:CameraCommand.c |
| core | 0x00751d80 | `CAM_CUBIC_EVAL_ANGLE_TWEAK` | hw1:CameraCommand.c |
| core | 0x00751da0 | `camTrackPosSpeed` | hw1:CameraCommand.c |
| core | 0x00751da4 | `camTrackPosThreshold` | hw1:CameraCommand.c |
| core | 0x00751da8 | `camTrackPosBase` | hw1:CameraCommand.c |
| core | 0x00751db4 | `CollapseTimeDelay` | hw1:CameraCommand.c |
| core | 0x007521fc | `TimedOutWaitingForPauseAcksGiveUpAfterNumTimes` | hw1:Captaincy.c |
| core | 0x00752600 | `DUSTCLOUD_RADIAL_SCALE` | hw1:Clouds.c |
| core | 0x00752614 | `DUSTCLOUD_BG_SCALAR` | hw1:Clouds.c |
| core | 0x0075261c | `DUSTCLOUD_DENSITY_THRESHOLD` | hw1:Clouds.c |
| core | 0x00752638 | `DUSTCLOUD_FOG_RED` | hw1:Clouds.c |
| core | 0x0075263c | `DUSTCLOUD_FOG_GREEN` | hw1:Clouds.c |
| core | 0x00752640 | `DUSTCLOUD_FOG_BLUE` | hw1:Clouds.c |
| core | 0x00752644 | `DUSTCLOUD_RAN_MULTIPLIER` | hw1:Clouds.c |
| core | 0x00752648 | `CLOUD_LDT0` | hw1:Clouds.c |
| core | 0x0075264c | `CLOUD_LDT1` | hw1:Clouds.c |
| core | 0x00752650 | `CLOUD_LIGHTNING_MAXDIST` | hw1:Clouds.c |
| core | 0x0075265c | `CLOUD_RADIAL_DAMAGE_SCALAR` | hw1:Clouds.c |
| core | 0x00752660 | `CLOUD_ZAP_SCALAR` | hw1:Clouds.c |
| core | 0x00752664 | `CLOUD_LIGHTNING_WIDTH_UPCLOSE` | hw1:Clouds.c |
| core | 0x00752668 | `CLOUD_LIGHTNING_WIDTH_FARAWAY` | hw1:Clouds.c |
| core | 0x00752678 | `CLOUD_LIGHTNING_FRINGE_SCALAR` | hw1:Clouds.c |
| core | 0x0075267c | `CLOUD_LIGHTNING_MIDPOINT_SCALAR` | hw1:Clouds.c |
| core | 0x00752688 | `CLOUD_LIGHTNING_MAIN_RED` | hw1:Clouds.c |
| core | 0x0075268c | `CLOUD_LIGHTNING_MAIN_GREEN` | hw1:Clouds.c |
| core | 0x00752690 | `CLOUD_LIGHTNING_MAIN_BLUE` | hw1:Clouds.c |
| core | 0x00752694 | `CLOUD_LIGHTNING_MAIN_ALPHA` | hw1:Clouds.c |
| core | 0x00753150 | `DISTMAXRATIO` | hw1:CommandLayer.c |
| core | 0x00753154 | `SHRINKMIN` | hw1:CommandLayer.c |
| core | 0x00753158 | `SHRINKMAX` | hw1:CommandLayer.c |
| core | 0x00753160 | `PROTECT_GET_THIS_CLOSE_CAP` | hw1:CommandLayer.c |
| core | 0x00753164 | `PROTECT_GET_THIS_CLOSE_STRIKE` | hw1:CommandLayer.c |
| core | 0x00753168 | `PROTECT_STILL_VELOCITY` | hw1:CommandLayer.c |
| core | 0x0075316c | `WINGMAN_JOIN_MAX_DIST` | hw1:CommandLayer.c |
| core | 0x0075317c | `FIGHTER_PAIRUP_PROB` | hw1:CommandLayer.c |
| core | 0x00753198 | `MULTIPLETARGETS_RETARGET_RATE` | hw1:CommandLayer.c |
| core | 0x0075319c | `GLOBAL_SHIP_HEALTH_MODIFIER` | hw1:CommandLayer.c |
| core | 0x007531a0 | `RESCONTROLLER_FOLLOW_BEHIND` | hw1:CommandLayer.c |
| core | 0x007549d0 | `cmStandardTextColor` | hw1:ConsMgr.c |
| core | 0x008787b0 | `CRATES_DoWeNeedToAddCratesCheckTime` | hw1:Crates.c |
| core | 0x008787b4 | `CRATES_ArePlayersNearUpdateMask` | hw1:Crates.c |
| core | 0x008787b8 | `CRATES_ArePlayersNearUpdateFrame` | hw1:Crates.c |
| core | 0x008787bc | `CRATES_PlayerNearRadius` | hw1:Crates.c |
| core | 0x008787c0 | `CRATES_MaximumCratesInTheWorldPerPlayer` | hw1:Crates.c |
| core | 0x008787c4 | `CRATES_GetResearch` | hw1:Crates.c |
| core | 0x008787c8 | `CRATES_RandomLowDistance` | hw1:Crates.c |
| core | 0x008787cc | `CRATES_RandomHighDistance` | hw1:Crates.c |
| core | 0x008787d0 | `CRATES_AddACrateProb` | hw1:Crates.c |
| core | 0x008787d4 | `CRATES_MAX_RUS_GIVEN` | hw1:Crates.c |
| core | 0x008787d8 | `CRATES_MIN_RUS_GIVEN` | hw1:Crates.c |
| core | 0x008787e0 | `CRATES_GetAShip` | hw1:Crates.c |
| core | 0x00878800 | `CRATE_EXPIRY_TIME` | hw1:Crates.c |
| core | 0x008790dc | `DMG_StrikeTrailLimit` | hw1:Damage.c |
| core | 0x008790e0 | `DMG_StrikeLightLimit` | hw1:Damage.c |
| core | 0x008790e4 | `DMG_TrailLimit` | hw1:Damage.c |
| core | 0x008790e8 | `DMG_LightLimit` | hw1:Damage.c |
| core | 0x008790ec | `DMG_HeavyLimit` | hw1:Damage.c |
| core | 0x008790f0 | `DMG_DyingLimit` | hw1:Damage.c |
| core | 0x0087ac7c | `etgSoftwareScalarHit` | hw1:ETG.c |
| core | 0x0087ac84 | `etgHistoryScalar` | hw1:ETG.c |
| core | 0x0087ac88 | `etgHistoryScalarMin` | hw1:ETG.c |
| core | 0x0087ac8c | `etgDamageEffectsEnabled` | hw1:ETG.c |
| core | 0x0087ac90 | `etgHitEffectsEnabled` | hw1:ETG.c |
| core | 0x0087ac94 | `etgFireEffectsEnabled` | hw1:ETG.c |
| core | 0x0087ac98 | `etgBulletEffectsEnabled` | hw1:ETG.c |
| core | 0x0087e398 | `FLIPTURN_MAXROTSPEEDFLIP` | hw1:FlightMan.c |
| core | 0x0087e39c | `FLIPTURN_PAUSEAFTERFLIPTIME` | hw1:FlightMan.c |
| core | 0x0087e3a0 | `FLIPTURN_FLIPACCELMODIFIER` | hw1:FlightMan.c |
| core | 0x0087e3a4 | `FLIPTURN_AFTERFLIPTHRUSTMOD` | hw1:FlightMan.c |
| core | 0x0087e3a8 | `FLIPTURN_ROLLTHRUSTMOD` | hw1:FlightMan.c |
| core | 0x0087e3ac | `FLIPTURN_ROLLACCELMODIFIER` | hw1:FlightMan.c |
| core | 0x0087e3b0 | `FLIPTURN_MAXROLLSPEED` | hw1:FlightMan.c |
| core | 0x0087e3b4 | `IMMELMAN_THRUSTMOD` | hw1:FlightMan.c |
| core | 0x0087e3b8 | `IMMELMAN_ACCELMODIFIERFLIP` | hw1:FlightMan.c |
| core | 0x0087e3bc | `IMMELMAN_MAXROTSPEEDFLIP` | hw1:FlightMan.c |
| core | 0x0087e3c0 | `IMMELMAN_ROLLACCELMODIFIER` | hw1:FlightMan.c |
| core | 0x0087e3c4 | `IMMELMAN_MAXROLLSPEED` | hw1:FlightMan.c |
| core | 0x0087e3c8 | `IMMELMAN_ROLLTHRUSTMOD` | hw1:FlightMan.c |
| core | 0x0087e3cc | `SPLIT_S_THRUSTMOD` | hw1:FlightMan.c |
| core | 0x0087e3d0 | `SPLIT_S_ACCELMODIFIERFLIP` | hw1:FlightMan.c |
| core | 0x0087e3d4 | `SPLIT_S_MAXROTSPEEDPITCH` | hw1:FlightMan.c |
| core | 0x0087e3d8 | `SPLIT_S_ACCELMODIFIERROLL` | hw1:FlightMan.c |
| core | 0x0087e3dc | `SPLIT_S_MAXROTSPEEDROLL` | hw1:FlightMan.c |
| core | 0x0087e3e8 | `HARDBANK_THRUSTMOD` | hw1:FlightMan.c |
| core | 0x0087e3ec | `HARDBANK_ACCELMODIFIERFLIP` | hw1:FlightMan.c |
| core | 0x0087e3f0 | `HARDBANK_MAXROTSPEEDPITCH` | hw1:FlightMan.c |
| core | 0x0087e3f4 | `HARDBANK_ACCELMODIFIERROLL` | hw1:FlightMan.c |
| core | 0x0087e3f8 | `HARDBANK_MAXROTSPEEDROLL` | hw1:FlightMan.c |
| core | 0x0087e3fc | `SOFTBANK_THRUSTMOD` | hw1:FlightMan.c |
| core | 0x0087e400 | `SOFTBANK_ACCELMODIFIERFLIP` | hw1:FlightMan.c |
| core | 0x0087e404 | `SOFTBANK_MAXROTSPEEDPITCH` | hw1:FlightMan.c |
| core | 0x0087e408 | `SOFTBANK_ACCELMODIFIERROLL` | hw1:FlightMan.c |
| core | 0x0087e40c | `SOFTBANK_MAXROTSPEEDROLL` | hw1:FlightMan.c |
| core | 0x0087e448 | `WHIPSTRAFE_BULLETRANGEOPTIMUM` | hw1:FlightMan.c |
| core | 0x0087e44c | `WHIPSTRAFE_MAXANGCOMPAREFACTOR` | hw1:FlightMan.c |
| core | 0x0087e450 | `WHIPSTRAFE_FLYBYOVERSHOOT` | hw1:FlightMan.c |
| core | 0x0087e454 | `WHIPSTRAFE_MINVELOCITY` | hw1:FlightMan.c |
| core | 0x0087e458 | `WHIPSTRAFE_REACHDESTINATIONRANGE` | hw1:FlightMan.c |
| core | 0x0087e478 | `SPLITSEVASIVE_ACCELMODIFIERPITCH` | hw1:FlightMan.c |
| core | 0x0087e47c | `SPLITSEVASIVE_MAXROTSPEEDPITCH` | hw1:FlightMan.c |
| core | 0x0087e480 | `SPLITSEVASIVE_THRUSTMOD` | hw1:FlightMan.c |
| core | 0x0087e484 | `SPLITSEVASIVE_ACCELMODIFIERROLL` | hw1:FlightMan.c |
| core | 0x0087e488 | `SPLITSEVASIVE_MAXROTSPEEDROLL` | hw1:FlightMan.c |
| core | 0x0087e48c | `LOYOYO_THRUSTMOD0` | hw1:FlightMan.c |
| core | 0x0087e490 | `LOYOYO_THRUSTMOD1` | hw1:FlightMan.c |
| core | 0x0087e494 | `LOYOYO_THRUSTMOD2` | hw1:FlightMan.c |
| core | 0x0087e498 | `LOYOYO_THRUSTMOD3` | hw1:FlightMan.c |
| core | 0x0087e49c | `LOYOYO_THRUSTMOD4` | hw1:FlightMan.c |
| core | 0x0087e4a0 | `LOYOYO_ACCELMODIFIERPITCH` | hw1:FlightMan.c |
| core | 0x0087e4a4 | `LOYOYO_MAXROTSPEEDPITCH` | hw1:FlightMan.c |
| core | 0x0087e4a8 | `LOYOYO_GODOWN_TIME` | hw1:FlightMan.c |
| core | 0x0087e4ac | `LOYOYO_GOUP_TIME` | hw1:FlightMan.c |
| core | 0x0087e4b0 | `HIYOYO_THRUSTMOD0` | hw1:FlightMan.c |
| core | 0x0087e4b4 | `HIYOYO_THRUSTMOD1` | hw1:FlightMan.c |
| core | 0x0087e4b8 | `HIYOYO_THRUSTMOD2` | hw1:FlightMan.c |
| core | 0x0087e4bc | `HIYOYO_THRUSTMOD3` | hw1:FlightMan.c |
| core | 0x0087e4c0 | `HIYOYO_THRUSTMOD4` | hw1:FlightMan.c |
| core | 0x0087e4c4 | `HIYOYO_ACCELMODIFIERPITCH` | hw1:FlightMan.c |
| core | 0x0087e4c8 | `HIYOYO_MAXROTSPEEDPITCH` | hw1:FlightMan.c |
| core | 0x0087e4cc | `HIYOYO_GOUP_TIME` | hw1:FlightMan.c |
| core | 0x0087e4d0 | `HIYOYO_GODOWN_TIME` | hw1:FlightMan.c |
| core | 0x0087e530 | `FLIGHTMAN_TOKEN_EVASIVEPURE_CHECKRATE` | hw1:FlightMan.c |
| core | 0x0087e534 | `FLIGHTMAN_TOKEN_EVASIVEPURE_CHECKFRAME` | hw1:FlightMan.c |
| core | 0x0087e538 | `FLIGHTMAN_TOKEN_EVASIVEPURE_PROB` | hw1:FlightMan.c |
| core | 0x0087f948 | `DELTA_FORMATION_PADDING_SCALE` | hw1:Formation.c |
| core | 0x0087f94c | `BROAD_FORMATION_PADDING_SCALE` | hw1:Formation.c |
| core | 0x0087f950 | `CLAW_FORMATION_PADDING_SCALE` | hw1:Formation.c |
| core | 0x0087f954 | `DELTA3D_FORMATION_PADDING_SCALE` | hw1:Formation.c |
| core | 0x0087f958 | `WALL_FORMATION_PADDING_SCALE` | hw1:Formation.c |
| core | 0x0087f95c | `WALL_FORMATION_PADDING` | hw1:Formation.c |
| core | 0x0087f964 | `SPHERE_FORMATION_PADDING_SCALE` | hw1:Formation.c |
| core | 0x0087f968 | `CLAW_FORMATION_CURVATURE` | hw1:Formation.c |
| core | 0x0087f96c | `SPHERE_MINIMUM_RADIUS` | hw1:Formation.c |
| core | 0x0087f970 | `SPHERE_MAXIMUM_RADIUS` | hw1:Formation.c |
| core | 0x0087f974 | `SPHERESIZE_IGNORE_NUMDECLINATIONS` | hw1:Formation.c |
| core | 0x0087f978 | `STRIKECRAFT_PADDING_MODIFIER` | hw1:Formation.c |
| core | 0x0087f97c | `FORMATION_TRAVELVEL_MAXSCALE` | hw1:Formation.c |
| core | 0x0087f980 | `FORMATION_TRAVELVEL_MINSCALE` | hw1:Formation.c |
| core | 0x0087f984 | `FORMATION_ERROR_BIG` | hw1:Formation.c |
| core | 0x0087f988 | `FORMATION_ERROR_NEGLIGIBLE` | hw1:Formation.c |
| core | 0x0087f990 | `FORMATION_TRAVEL_SLOWEST_SHIP` | hw1:Formation.c |
| core | 0x008832e0 | `SLOW_INTERNETPRINTTIME` | hw1:LagPrint.c |
| core | 0x008832e4 | `lagSlowCompX` | hw1:LagPrint.c |
| core | 0x008832e8 | `lagSlowCompY` | hw1:LagPrint.c |
| core | 0x008832ec | `lagSlowIntX` | hw1:LagPrint.c |
| core | 0x008832f0 | `lagSlowIntY` | hw1:LagPrint.c |
| core | 0x00883960 | `SongNumber` | hw1:LevelLoad.c |
| core | 0x00885c40 | `memSmallHeapInfo` | hw1:Memory.c |
| core | 0x00886014 | `firewallButton` | hw1:MultiplayerGame.c |
| core | 0x00886618 | `mgGameWhisperedColor` | hw1:MultiplayerGame.c |
| core | 0x00886620 | `mgGameNormalChatColor` | hw1:MultiplayerGame.c |
| core | 0x00886624 | `mgGameMessageChatColor` | hw1:MultiplayerGame.c |
| core | 0x0088662c | `mgPrivateChatColor` | hw1:MultiplayerGame.c |
| core | 0x00886630 | `mgNormalChatColor` | hw1:MultiplayerGame.c |
| core | 0x00886634 | `mgMessageChatColor` | hw1:MultiplayerGame.c |
| core | 0x008884d0 | `NEB_RADIUS` | hw1:Nebulae.c |
| core | 0x008884d4 | `NEB_FOG_RED` | hw1:Nebulae.c |
| core | 0x008884d8 | `NEB_FOG_GREEN` | hw1:Nebulae.c |
| core | 0x008884dc | `NEB_FOG_BLUE` | hw1:Nebulae.c |
| core | 0x008884e4 | `NEB_TENDRIL_RED_DEV` | hw1:Nebulae.c |
| core | 0x008884e8 | `NEB_TENDRIL_GREEN_DEV` | hw1:Nebulae.c |
| core | 0x008884ec | `NEB_TENDRIL_BLUE_DEV` | hw1:Nebulae.c |
| core | 0x008884f4 | `NEB_MAX_TENDRIL_ANGULAR_DEVIATION` | hw1:Nebulae.c |
| core | 0x008884f8 | `NEB_MAX_TENDRIL_DISTANCE` | hw1:Nebulae.c |
| core | 0x008884fc | `NEB_PICK_DISTANCE_THRESHOLD` | hw1:Nebulae.c |
| core | 0x00888500 | `NEB_PICK_DISTANCE_MINTHRESH` | hw1:Nebulae.c |
| core | 0x00888504 | `NEB_SHUFFLE_DISTANCE` | hw1:Nebulae.c |
| core | 0x00888508 | `NEB_VEL_RAN_MULTIPLIER` | hw1:Nebulae.c |
| core | 0x00888510 | `NEB_VEL_BASE` | hw1:Nebulae.c |
| core | 0x00888514 | `NEB_TENDRIL_RADIUS_BASE` | hw1:Nebulae.c |
| core | 0x00888518 | `NEB_TENDRIL_RADIUS_RANGE` | hw1:Nebulae.c |
| core | 0x0088ba00 | `opTimerLength` | hw1:Options.c |
| core | 0x0088be48 | `opDeviceIndex` | hw1:Options.c |
| core | 0x0088be4c | `opMusicVol` | hw1:Options.c |
| core | 0x0088be54 | `opSpeechVol` | hw1:Options.c |
| core | 0x0088be58 | `opNumChannels` | hw1:Options.c |
| core | 0x0088be5c | `opSpeakerSetting` | hw1:Options.c |
| core | 0x0088be60 | `opVoiceComm` | hw1:Options.c |
| core | 0x0088be64 | `opVoiceStat` | hw1:Options.c |
| core | 0x0088be68 | `opVoiceChat` | hw1:Options.c |
| core | 0x0088be6c | `opDetailThresholdVal` | hw1:Options.c |
| core | 0x0088be70 | `opBrightnessVal` | hw1:Options.c |
| core | 0x0088be88 | `opMouseSens` | hw1:Options.c |
| core | 0x0088be8c | `opBattleChatter` | hw1:Options.c |
| core | 0x0088be90 | `opInfoOverlayVar` | hw1:Options.c |
| core | 0x0088d758 | `pieMaxMoveHorizontal` | hw1:PiePlate.c |
| core | 0x0088d75c | `pieMaxMoveVertical` | hw1:PiePlate.c |
| core | 0x0088d848 | `pingAnomalyColor` | hw1:Ping.c |
| core | 0x0088d84c | `pingAnomalySize` | hw1:Ping.c |
| core | 0x0088d850 | `pingAnomalyMinSize` | hw1:Ping.c |
| core | 0x0088d854 | `pingAnomalyPingDuration` | hw1:Ping.c |
| core | 0x0088d858 | `pingAnomalyInterPingPause` | hw1:Ping.c |
| core | 0x0088d85c | `pingAnomalyEvaluatePeriod` | hw1:Ping.c |
| core | 0x0088d860 | `pingBattleColor` | hw1:Ping.c |
| core | 0x0088d864 | `pingBattleMinSize` | hw1:Ping.c |
| core | 0x0088d868 | `pingBattlePingDuration` | hw1:Ping.c |
| core | 0x0088d86c | `pingBattleInterPingPause` | hw1:Ping.c |
| core | 0x0088d870 | `pingBattleEvaluatePeriod` | hw1:Ping.c |
| core | 0x0088d874 | `pingNewShipColor` | hw1:Ping.c |
| core | 0x0088d878 | `pingNewShipSize` | hw1:Ping.c |
| core | 0x0088d87c | `pingNewShipMinSize` | hw1:Ping.c |
| core | 0x0088d880 | `pingNewShipPingDuration` | hw1:Ping.c |
| core | 0x0088d884 | `pingNewShipInterPingPause` | hw1:Ping.c |
| core | 0x0088d888 | `pingNewShipEvaluatePeriod` | hw1:Ping.c |
| core | 0x0088d894 | `pingTOLingerTime` | hw1:Ping.c |
| core | 0x0088f50c | `MAX_RESOURCE_VELOCITY_TO_CHASE` | hw1:ResCollect.c |
| core | 0x0088f510 | `ASTEROID_HARVEST_RANGE` | hw1:ResCollect.c |
| core | 0x0088f54c | `ASTEROID_ROTATE_PROB` | hw1:ResCollect.c |
| core | 0x0088f554 | `COLLECTRESOURCE_ZONE` | hw1:ResCollect.c |
| core | 0x0088f558 | `REGROW_RESOURCES_CHECK_RATE` | hw1:ResCollect.c |
| core | 0x0088f55c | `REGROW_RESOURCES_CHECK_FRAME` | hw1:ResCollect.c |
| core | 0x0088f584 | `regenerateRUsIfUnder` | hw1:ResCollect.c |
| core | 0x0088f588 | `regenerateRUs` | hw1:ResCollect.c |
| core | 0x0088f58c | `CHECK_HARV_OUTOFWORLD_RESOURCE_RATE` | hw1:ResCollect.c |
| core | 0x0088f590 | `CHECK_SHIP_OUTOFWORLD_RATE` | hw1:ResCollect.c |
| core | 0x00891f2c | `rmProgressDoneColor0` | hw1:ResearchGUI.c |
| core | 0x00891f30 | `rmProgressDoneColor1` | hw1:ResearchGUI.c |
| core | 0x00891f3c | `rmNoResearchItemColor` | hw1:ResearchGUI.c |
| core | 0x00892790 | `spDescriptionColor` | hw1:ScenPick.c |
| core | 0x00892a38 | `selSelectingColor` | hw1:Select.c |
| core | 0x00892a3c | `selShipResourceColor` | hw1:Select.c |
| core | 0x00892a40 | `selShipDarkResourceColor` | hw1:Select.c |
| core | 0x00892a44 | `selShipHealthGreen` | hw1:Select.c |
| core | 0x00892a48 | `selShipHealthYellow` | hw1:Select.c |
| core | 0x00892a4c | `selShipHealthRed` | hw1:Select.c |
| core | 0x00892a50 | `selShipHealthDarkGreen` | hw1:Select.c |
| core | 0x00892a54 | `selShipHealthDarkYellow` | hw1:Select.c |
| core | 0x00892a58 | `selShipHealthDarkRed` | hw1:Select.c |
| core | 0x00892a7c | `selDepthSelectMultiplier` | hw1:Select.c |
| core | 0x00892b98 | `smBlobColor` | hw1:Sensors.c |
| core | 0x00892b9c | `smBlobColorHigh` | hw1:Sensors.c |
| core | 0x00892ba0 | `smBlobColorLow` | hw1:Sensors.c |
| core | 0x00892ba4 | `smFuzzyBlobs` | hw1:Sensors.c |
| core | 0x00892c48 | `smSelectedFlashSpeed` | hw1:Sensors.c |
| core | 0x00892c50 | `smFocusRadius` | hw1:Sensors.c |
| core | 0x00892c5c | `smZoomLength` | hw1:Sensors.c |
| core | 0x00892c60 | `smMainViewZoomLength` | hw1:Sensors.c |
| core | 0x00892c84 | `smWorldPlaneColor` | hw1:Sensors.c |
| core | 0x00892c98 | `smWorldPlaneDistanceFactor` | hw1:Sensors.c |
| core | 0x00892ce0 | `smProjectionScale` | hw1:Sensors.c |
| core | 0x00892ce4 | `smPanSpeedMultiplier` | hw1:Sensors.c |
| core | 0x00892ce8 | `smPanTrack` | hw1:Sensors.c |
| core | 0x00892cec | `smPanEvalThreshold` | hw1:Sensors.c |
| core | 0x00892cf4 | `smCursorPanX` | hw1:Sensors.c |
| core | 0x00892cf8 | `smCursorPanY` | hw1:Sensors.c |
| core | 0x00892cfc | `smMaxFrameTicks` | hw1:Sensors.c |
| core | 0x00892d00 | `smDepthCueRadius` | hw1:Sensors.c |
| core | 0x00892d04 | `smDepthCueStartRadius` | hw1:Sensors.c |
| core | 0x00892d08 | `smCircleBorder` | hw1:Sensors.c |
| core | 0x00892d0c | `smZoomMax` | hw1:Sensors.c |
| core | 0x00892d10 | `smZoomMin` | hw1:Sensors.c |
| core | 0x00892d14 | `smZoomMinFactor` | hw1:Sensors.c |
| core | 0x00892d18 | `smZoomMaxFactor` | hw1:Sensors.c |
| core | 0x00892d1c | `smInitialDistance` | hw1:Sensors.c |
| core | 0x00892d20 | `smUniverseSizeX` | hw1:Sensors.c |
| core | 0x00892d24 | `smUniverseSizeY` | hw1:Sensors.c |
| core | 0x00892d28 | `smUniverseSizeZ` | hw1:Sensors.c |
| core | 0x00892d2c | `smSkipFadeoutTime` | hw1:Sensors.c |
| core | 0x00893cd4 | `svAngle` | hw1:ShipView.c |
| core | 0x00893cd8 | `svDeclination` | hw1:ShipView.c |
| core | 0x00893cdc | `svZoomOutScalar` | hw1:ShipView.c |
| core | 0x00893ce0 | `svZoomInScalar` | hw1:ShipView.c |
| core | 0x00894110 | `HYPERSPACE_MOTHERSHIP_ALWAYS_ARRIVES_FIRST` | hw1:SinglePlayer.c |
| core | 0x00894118 | `HYPERSPACE_BASE_WAITTIME` | hw1:SinglePlayer.c |
| core | 0x0089411c | `HYPERSPACE_BASE_ARRIVETIME` | hw1:SinglePlayer.c |
| core | 0x00894120 | `HYPERSPACE_BASE_MOTHERSHIPARRIVETIME` | hw1:SinglePlayer.c |
| core | 0x00894124 | `HYPERSPACE_RANDOM_WAITTIME` | hw1:SinglePlayer.c |
| core | 0x00894134 | `HYPERSPACE_ACCELERATE_TIME` | hw1:SinglePlayer.c |
| core | 0x00894140 | `HYPERSPACE_HEADINGACCURACY` | hw1:SinglePlayer.c |
| core | 0x00894144 | `HYPERSPACE_FARCLIPPLANE` | hw1:SinglePlayer.c |
| core | 0x00894148 | `HYPERSPACE_WHITEOUTTIME` | hw1:SinglePlayer.c |
| core | 0x00894154 | `HYPERSPACE_DECELERATE_TIME` | hw1:SinglePlayer.c |
| core | 0x00894158 | `HYPERSPACE_DECELERATION` | hw1:SinglePlayer.c |
| core | 0x00894160 | `SINGLEPLAYER_STARTINGRUS` | hw1:SinglePlayer.c |
| core | 0x00894164 | `HS_CLIPT_NEG_THRESH` | hw1:SinglePlayer.c |
| core | 0x00894168 | `HS_CLIPT_NEG_SCALAR` | hw1:SinglePlayer.c |
| core | 0x0089416c | `HS_CLIPT_NEG_FINISH` | hw1:SinglePlayer.c |
| core | 0x00894170 | `HS_CLIPT_POS_THRESH` | hw1:SinglePlayer.c |
| core | 0x00894174 | `HS_CLIPT_POS_SCALAR` | hw1:SinglePlayer.c |
| core | 0x00894178 | `HS_CLIPT_POS_START` | hw1:SinglePlayer.c |
| core | 0x0089417c | `HYPERSPACEGATE_HEALTH` | hw1:SinglePlayer.c |
| core | 0x00894180 | `HYPERSPACEGATE_WIDTH` | hw1:SinglePlayer.c |
| core | 0x00894184 | `HYPERSPACEGATE_HEIGHT` | hw1:SinglePlayer.c |
| core | 0x00898148 | `LanguageStrings` | hw1:StringSupport.c |
| core | 0x008a8078 | `ATTACKING_FROM_ABOVE_MIN_DIST` | hw1:Tactics.c |
| core | 0x008a807c | `ATTACKING_FROM_ABOVE_MIN_RATIO` | hw1:Tactics.c |
| core | 0x008a8098 | `GLOBAL_SHIP_SPEED_MODIFIER` | hw1:Tactics.c |
| core | 0x008aa6f0 | `cat_timeCompressionDefaultIndex` | cat-new |
| core | 0x008aa6f4 | `taskCurrentTask` | hw1:Task.c |
| core | 0x008aa6fc | `babyTaskHandle` | hw1:Task.c |
| core | 0x008aa7b0 | `teSpecularAdjust` | hw1:Teams.c |
| core | 0x008aa7b4 | `teDiffuseAdjust` | hw1:Teams.c |
| core | 0x008aa7b8 | `teAmbientAdjust` | hw1:Teams.c |
| core | 0x008aa988 | `CONNECT_TIMEOUT` | hw1:TitanNet.c |
| core | 0x008aa98c | `TITAN_PICKER_REFRESH_TIME` | hw1:TitanNet.c |
| core | 0x008aa990 | `TITAN_GAME_EXPIRE_TIME` | hw1:TitanNet.c |
| core | 0x008aa998 | `DIRSERVER_NUM` | hw1:TitanNet.c |
| core | 0x008aa9a0 | `GAME_PORT` | hw1:TitanNet.c |
| core | 0x008aa9a4 | `AD_PORT` | hw1:TitanNet.c |
| core | 0x008aaa20 | `PRINTLAG_IFGREATERTHAN` | hw1:TitanNet.c |
| core | 0x008aaa24 | `PRINTLAG_MINFRAMES` | hw1:TitanNet.c |
| core | 0x008aaa28 | `ROOM_MIN_THRESHOLD` | hw1:TitanNet.c |
| core | 0x008aaa2c | `ROOM_MAX_THRESHOLD` | hw1:TitanNet.c |
| core | 0x008ac3b8 | `singleMachineNumPlayers` | hw1:Tweak.c |
| core | 0x008ac3c0 | `RETALIATE_ZONE` | hw1:Tweak.c |
| core | 0x008ac3c4 | `BLAST_CONSTANT` | hw1:Tweak.c |
| core | 0x008ac3c8 | `BLAST_RADIUS_MULTIPLE_DEFAULT` | hw1:Tweak.c |
| core | 0x008ac3cc | `BLAST_DAMAGE_MAX_PERCENT` | hw1:Tweak.c |
| core | 0x008ac3d0 | `BLAST_CONSTANT_SCUTTLE` | hw1:Tweak.c |
| core | 0x008ac3d4 | `BLAST_RADIUS_SCUTTLE_BONUS` | hw1:Tweak.c |
| core | 0x008ac3d8 | `BLAST_DAMAGE_MAX_PERCENT_SCUTTLE` | hw1:Tweak.c |
| core | 0x008ac3dc | `YAWEXPLOSIONROCKLO` | hw1:Tweak.c |
| core | 0x008ac3e0 | `YAWEXPLOSIONROCKHI` | hw1:Tweak.c |
| core | 0x008ac3e4 | `PITCHEXPLOSIONROCKLO` | hw1:Tweak.c |
| core | 0x008ac3e8 | `PITCHEXPLOSIONROCKHI` | hw1:Tweak.c |
| core | 0x008ac3ec | `ROLLEXPLOSIONROCKLO` | hw1:Tweak.c |
| core | 0x008ac3f0 | `ROLLEXPLOSIONROCKHI` | hw1:Tweak.c |
| core | 0x008ac3f4 | `YAWROCKLO` | hw1:Tweak.c |
| core | 0x008ac3f8 | `YAWROCKHI` | hw1:Tweak.c |
| core | 0x008ac3fc | `PITCHROCKLO` | hw1:Tweak.c |
| core | 0x008ac400 | `PITCHROCKHI` | hw1:Tweak.c |
| core | 0x008ac404 | `ROLLROCKLO` | hw1:Tweak.c |
| core | 0x008ac408 | `ROLLROCKHI` | hw1:Tweak.c |
| core | 0x008ac40c | `YAWCAREENLO` | hw1:Tweak.c |
| core | 0x008ac410 | `YAWCAREENHI` | hw1:Tweak.c |
| core | 0x008ac414 | `PITCHCAREENLO` | hw1:Tweak.c |
| core | 0x008ac418 | `PITCHCAREENHI` | hw1:Tweak.c |
| core | 0x008ac41c | `ROLLCAREENLO` | hw1:Tweak.c |
| core | 0x008ac420 | `ROLLCAREENHI` | hw1:Tweak.c |
| core | 0x008ac424 | `YAWCRAZYLO` | hw1:Tweak.c |
| core | 0x008ac428 | `YAWCRAZYHI` | hw1:Tweak.c |
| core | 0x008ac42c | `PITCHCRAZYLO` | hw1:Tweak.c |
| core | 0x008ac430 | `PITCHCRAZYHI` | hw1:Tweak.c |
| core | 0x008ac434 | `ROLLCRAZYLO` | hw1:Tweak.c |
| core | 0x008ac438 | `ROLLCRAZYHI` | hw1:Tweak.c |
| core | 0x008ac43c | `RENDER_VIEWABLE_DISTANCE_SQR` | hw1:Tweak.c |
| core | 0x008ac440 | `RENDER_MAXVIEWABLE_DISTANCE_SQR` | hw1:Tweak.c |
| core | 0x008ac444 | `RENDER_VIEWABLE_FADE` | hw1:Tweak.c |
| core | 0x008ac448 | `RENDER_MAXVIEWABLE_FADE` | hw1:Tweak.c |
| core | 0x008ac44c | `BG_RADIUS` | hw1:Tweak.c |
| core | 0x008ac450 | `RENDER_LIMIT_MOTHERSHIP` | hw1:Tweak.c |
| core | 0x008ac454 | `RENDER_FADE_MOTHERSHIP` | hw1:Tweak.c |
| core | 0x008ac458 | `RENDER_LIMIT_HEAVYCRUISER` | hw1:Tweak.c |
| core | 0x008ac45c | `RENDER_FADE_HEAVYCRUISER` | hw1:Tweak.c |
| core | 0x008ac460 | `RENDER_LIMIT_CARRIER` | hw1:Tweak.c |
| core | 0x008ac464 | `RENDER_FADE_CARRIER` | hw1:Tweak.c |
| core | 0x008ac468 | `RENDER_LIMIT_DESTROYER` | hw1:Tweak.c |
| core | 0x008ac46c | `RENDER_FADE_DESTROYER` | hw1:Tweak.c |
| core | 0x008ac470 | `RENDER_LIMIT_FRIGATE` | hw1:Tweak.c |
| core | 0x008ac474 | `RENDER_FADE_FRIGATE` | hw1:Tweak.c |
| core | 0x008ac478 | `RENDER_LIMIT_CORVETTE` | hw1:Tweak.c |
| core | 0x008ac47c | `RENDER_FADE_CORVETTE` | hw1:Tweak.c |
| core | 0x008ac480 | `RENDER_LIMIT_FIGHTER` | hw1:Tweak.c |
| core | 0x008ac484 | `RENDER_FADE_FIGHTER` | hw1:Tweak.c |
| core | 0x008ac488 | `RENDER_LIMIT_RESOURCE` | hw1:Tweak.c |
| core | 0x008ac48c | `RENDER_FADE_RESOURCE` | hw1:Tweak.c |
| core | 0x008ac490 | `RENDER_LIMIT_NONCOMBAT` | hw1:Tweak.c |
| core | 0x008ac494 | `RENDER_FADE_NONCOMBAT` | hw1:Tweak.c |
| core | 0x008ac498 | `VERYBIGSHIP_SIZE` | hw1:Tweak.c |
| core | 0x008ac4a0 | `SHIP_CRAZY_FRICTION` | hw1:Tweak.c |
| core | 0x008ac4a4 | `DONT_DRIFT_FRICTION` | hw1:Tweak.c |
| core | 0x008ac4a8 | `CONSIDERED_STILL_LO` | hw1:Tweak.c |
| core | 0x008ac4ac | `CONSIDERED_STILL_HI` | hw1:Tweak.c |
| core | 0x008ac4b8 | `TW_MOVETO_CIRCLE_RADIUS` | hw1:Tweak.c |
| core | 0x008ac4bc | `TW_MOVETO_PULSE_SPEED_SCALE` | hw1:Tweak.c |
| core | 0x008ac4c0 | `TW_MOVETO_ENDCIRCLE_RADIUS` | hw1:Tweak.c |
| core | 0x008ac4c4 | `FLASH_TIMER` | hw1:Tweak.c |
| core | 0x008ac4cc | `COLLDAMAGE_DEFAULTMIN_FRACTION` | hw1:Tweak.c |
| core | 0x008ac4d0 | `COLLDAMAGE_DEFAULTMAX_FRACTION` | hw1:Tweak.c |
| core | 0x008ac4d4 | `PLAYERLOSE_SHIPDEATHTIMEMIN` | hw1:Tweak.c |
| core | 0x008ac4d8 | `PLAYERLOSE_SHIPDEATHTIMEMAX` | hw1:Tweak.c |
| core | 0x008ac4e0 | `WAIT_TIME_TO_QUIT` | hw1:Tweak.c |
| core | 0x008ac4e4 | `WAIT_TIME_TO_WIN` | hw1:Tweak.c |
| core | 0x008ac4ec | `FACEOFF_DISTANCE` | hw1:Tweak.c |
| core | 0x008ac4f0 | `STATFIGHT_TIMEOUT_TIME` | hw1:Tweak.c |
| core | 0x008ac4f8 | `MAX_MOVE_DISTANCE` | hw1:Tweak.c |
| core | 0x008ac500 | `AUTOSTABILIZE_FRAME_KICK_IN` | hw1:Tweak.c |
| core | 0x008ac504 | `resourceStartSmall` | hw1:Tweak.c |
| core | 0x008ac508 | `resourceStartMedium` | hw1:Tweak.c |
| core | 0x008ac50c | `resourceStartLarge` | hw1:Tweak.c |
| core | 0x008ac514 | `attrVELOCITY_TOWARDS` | hw1:Tweak.c |
| core | 0x008ac528 | `attrRING_WIDTH_SCALE` | hw1:Tweak.c |
| core | 0x008ac544 | `friendlyFireModifier` | hw1:Tweak.c |
| core | 0x008ac548 | `TW_ScuttleReconfirmTime` | hw1:Tweak.c |
| core | 0x008ac550 | `MINING_BASE_VERTICAL_DIST_HACK` | hw1:Tweak.c |
| core | 0x008ac554 | `COLLISION_CLEAR_TIME` | hw1:Tweak.c |
| core | 0x008ac558 | `GLANCE_COLLISION_VEL_RATIO` | hw1:Tweak.c |
| core | 0x008ac55c | `COLLISION_REPULSE_FACTOR` | hw1:Tweak.c |
| core | 0x008ac560 | `TUTORIAL_KAS_UPDATE_MASK` | hw1:Tweak.c |
| core | 0x008ac564 | `MINE_DO_SEARCH_MASK` | hw1:Tweak.c |
| core | 0x008ac568 | `CHECKBOGEY_FRAME` | hw1:Tweak.c |
| core | 0x008ac56c | `SPHERE_CHECK_TARGET_MASK` | hw1:Tweak.c |
| core | 0x008ac570 | `SPHERE_CHECK_TARGET_FRAME` | hw1:Tweak.c |
| core | 0x008ac574 | `CHECK_ATTACKMOVE_TO_MOVE_RATE` | hw1:Tweak.c |
| core | 0x008ac578 | `CHECK_ATTACKMOVE_TO_MOVE_FRAME` | hw1:Tweak.c |
| core | 0x008ac57c | `FORMATION_ERROR_CALCULATE_RATE` | hw1:Tweak.c |
| core | 0x008ac580 | `FORMATION_ERROR_CALCULATE_FRAME` | hw1:Tweak.c |
| core | 0x008ac5a0 | `CHECK_PASSIVE_ATTACK_RATE` | hw1:Tweak.c |
| core | 0x008ac5a4 | `CHECK_PASSIVE_ATTACK_WHILEMOVING_RATE` | hw1:Tweak.c |
| core | 0x008ac5a8 | `REFRESH_RENDERLIST_RATE` | hw1:Tweak.c |
| core | 0x008ac5ac | `REFRESH_RENDERLIST_FRAME` | hw1:Tweak.c |
| core | 0x008ac5b0 | `REFRESH_MINORRENDERLIST_RATE` | hw1:Tweak.c |
| core | 0x008ac5b4 | `REFRESH_MINORRENDERLIST_FRAME` | hw1:Tweak.c |
| core | 0x008ac5b8 | `REFRESH_COLLBLOB_RATE` | hw1:Tweak.c |
| core | 0x008ac5bc | `REFRESH_COLLBLOB_FRAME` | hw1:Tweak.c |
| core | 0x008ac5c0 | `REFRESH_COLLBLOB_BATTLEPING_FRAME` | hw1:Tweak.c |
| core | 0x008ac5c4 | `REFRESH_RESEARCH_RATE` | hw1:Tweak.c |
| core | 0x008ac5c8 | `REFRESH_RESEARCH_FRAME` | hw1:Tweak.c |
| core | 0x008ac5cc | `CHECK_BOUNTIES_RATE` | hw1:Tweak.c |
| core | 0x008ac5d0 | `CHECK_BOUNTIES_FRAME` | hw1:Tweak.c |
| core | 0x008ac5d8 | `TW_GLOBAL_PASSIVE_ATTACK_CANCEL_MODIFIER` | hw1:Tweak.c |
| core | 0x008ac5dc | `twSalTransSpeed` | cat-new |
| core | 0x008ac5e0 | `twSalTransMaxTime` | cat-new |
| core | 0x008ac5e4 | `armourPiercingModifier` | hw1:Tweak.c |
| core | 0x008ac5e8 | `twnLipsBaseOffset` | cat-new |
| core | 0x008ac5ec | `twCrystalProcessingTime` | cat-new |
| core | 0x008ac5f0 | `twProcessorAutoRepairDistance` | cat-new |
| core | 0x008ac5f4 | `twProcessorAutoRepairConeAngle` | cat-new |
| core | 0x008ac5f8 | `twProcessorAutoRepairUpdateRate` | cat-new |
| core | 0x008ac5fc | `twProcessorAutoRepairUpdateDelay` | cat-new |
| core | 0x008ac604 | `twNotifySalvagersNeededRate` | cat-new |
| core | 0x008ac608 | `twNotifySalvageTargetTooStrongRate` | cat-new |
| core | 0x008b5b94 | `FLY_INTO_WORLD_PERCENT_DIST` | hw1:UnivUpdate.c |
| core | 0x008b5b98 | `NUM_STARS` | hw1:UnivUpdate.c |
| core | 0x008b5b9c | `NUM_BIG_STARS` | hw1:UnivUpdate.c |
| core | 0x008b861c | `MemoryHeapSize` | hw1:main.c |
| core | 0x008b8624 | `showBackgrounds` | hw1:main.c |
| core | 0x008b8630 | `enableSmoothing` | hw1:main.c |
| core | 0x008b8634 | `enableTrails` | hw1:main.c |
| core | 0x008ba38c | `mrFastMoveShipClickTime` | hw1:mainrgn.c |
| core | 0x008ba390 | `mrFastMoveShipClickX` | hw1:mainrgn.c |
| core | 0x008ba394 | `mrFastMoveShipClickY` | hw1:mainrgn.c |
| core | 0x008bb418 | `subScrollDwellStart` | hw1:Subtitle.c |
| core | 0x008bb41c | `subScrollDwellEnd` | hw1:Subtitle.c |
| core | 0x008bb420 | `subScrollShortest` | hw1:Subtitle.c |
| core | 0x008bb424 | `subTitleShortest` | hw1:Subtitle.c |
| core | 0x008bb69c | `texLinearFiltering` | hw1:texreg.c |
| core | 0x008c07dc | `RAN_JITTER` | hw1:Trails.c |
| core | 0x008c07e4 | `VELRATIO_ACCELTHRESH` | hw1:Trails.c |
| core | 0x008c07ec | `ACCELTHRESH` | hw1:Trails.c |
| core | 0x008c0800 | `XADJ` | hw1:Trails.c |
| core | 0x008c0804 | `HALFWIDTH` | hw1:Trails.c |
| core | 0x008c0808 | `HALFWIDTH_FALLOFF` | hw1:Trails.c |
| core | 0x008c080c | `HALFWIDTH_MIN` | hw1:Trails.c |
| core | 0x008c0820 | `BURN_SIZESCALE` | hw1:Trails.c |
| core | 0x008c0828 | `BURN_COLORADJUST` | hw1:Trails.c |
| core | 0x008c082c | `COLORADJUST_FREQ` | hw1:Trails.c |
| core | 0x008c0830 | `COLOR_ADJUST` | hw1:Trails.c |
| core | 0x008c0834 | `TRAIL_EXPONENT_BASE` | hw1:Trails.c |
| core | 0x008c0838 | `TRAIL_EXPONENT_RANGE` | hw1:Trails.c |
| core | 0x008c0854 | `TRAIL_EXPANSION_TICKS` | hw1:Trails.c |
| core | 0x008c0e50 | `loadedDevcaps` | hw1:utility.c |
| core | 0x008c0e54 | `loadedDevcaps2` | hw1:utility.c |
| core | 0x008c0e58 | `regOptionsList` | hw1:utility.c |
| core | 0x008c0ef4 | `needtutorial` | hw1:utility_mac.c |
| core | 0x008c0f60 | `utyFrontEndFiles` | hw1:utility.c |
| core | 0x008c0f80 | `utyCallbacks` | hw1:utility.c |
| core | 0x008c1448 | `utyDrawCallbacks` | hw1:utility.c |
| core | 0x008c1478 | `utyOptionsList` | hw1:utility.c |
| core | 0x008c18f0 | `utyDemoStateTable` | hw1:utility.c |
| core | 0x008c4368 | `FIGHTER_BREAK_ANGLE_MIN` | hw1:CommandLayer.c |
| core | 0x008c436c | `FIGHTER_BREAK_ANGLE_MAX` | hw1:CommandLayer.c |
| core | 0x008c4370 | `FIGHTER_BREAK_VERTICAL_ANGLE_MIN` | hw1:CommandLayer.c |
| core | 0x008c4374 | `FIGHTER_BREAK_VERTICAL_ANGLE_MAX` | hw1:CommandLayer.c |
| core | 0x008df640 | `AIO_HARASS_INITIAL_MOVE_FORMATION` | hw1:AIPlayer.c |
| core | 0x008df9fc | `IgnoreBigfiles` | hw1:BigFile.c |
| core | 0x008dfa04 | `mainFP` | hw1:BigFile.c |
| core | 0x008dfa08 | `updateFP` | hw1:BigFile.c |
| core | 0x008dfa0c | `mainNewerAvailable` | hw1:BigFile.c |
| core | 0x008dfa10 | `updateNewerAvailable` | hw1:BigFile.c |
| core | 0x00a2da2c | `CLOUD_LIGHTNING_FRINGE_RED` | hw1:Clouds.c |
| core | 0x00a2da30 | `CLOUD_LIGHTNING_FRINGE_GREEN` | hw1:Clouds.c |
| core | 0x00a2da84 | `cpColorsPicked` | hw1:ColPick.c |
| core | 0x00a2df1c | `demDemoRecording` | hw1:Demo.c |
| core | 0x00a2df20 | `demDemoPlaying` | hw1:Demo.c |
| core | 0x00a2df28 | `demFileSaveName` | hw1:Demo.c |
| core | 0x00a2dfa8 | `demLoadFile` | hw1:Demo.c |
| core | 0x00a2dfac | `demFinishFunction` | hw1:Demo.c |
| core | 0x00a3e180 | `decompWorkspaceP` | hw1:File.c |
| core | 0x00a3e188 | `decompWorkspaceInUse` | hw1:File.c |
| core | 0x00a43c90 | `gameIsRunning` | hw1:Universe.c |
| core | 0x00a4443c | `keyNumberBufferKeys` | hw1:Key.c |
| core | 0x00a4b998 | `memNumberGrowthPools` | hw1:Memory.c |
| core | 0x00a4b99c | `memGrowthAllocate` | hw1:Memory.c |
| core | 0x00a4b9a0 | `memModuleInit` | hw1:Memory.c |
| core | 0x00a4b9a8 | `memClearSetting` | hw1:Memory.c |
| core | 0x00a4b9ac | `memFreeSetting` | hw1:Memory.c |
| core | 0x00a4c7d4 | `patchComplete` | hw1:MultiplayerGame.c |
| core | 0x00a4cbbc | `opTimerActive` | hw1:Options.c |
| core | 0x00a4cc2c | `opAutoChannel` | hw1:Options.c |
| core | 0x00a4cc30 | `opSoundQuality` | hw1:Options.c |
| core | 0x00a4cc34 | `opNoLODVal` | hw1:Options.c |
| core | 0x00a4cc3c | `opEffectsVal` | hw1:Options.c |
| core | 0x00a50d00 | `pingAnomalyMinScreenSize` | hw1:Ping.c |
| core | 0x00a50d04 | `pingBattleMinScreenSize` | hw1:Ping.c |
| core | 0x00a50d08 | `pingNewShipMinScreenSize` | hw1:Ping.c |
| core | 0x00a578f0 | `savefile` | hw1:SaveGame.c |
| core | 0x00a57910 | `spCurrentSelected` | hw1:ScenPick.c |
| core | 0x00a57928 | `spDescriptionShadow` | hw1:ScenPick.c |
| core | 0x00a57938 | `selFontName0` | hw1:Select.c |
| core | 0x00a5793c | `selFontName1` | hw1:Select.c |
| core | 0x00a57940 | `selFontName2` | hw1:Select.c |
| core | 0x00a57944 | `selFontName3` | hw1:Select.c |
| core | 0x00a5b1e8 | `smInstantTransition` | hw1:Sensors.c |
| core | 0x00a5b244 | `smHotKeyOffsetX` | hw1:Sensors.c |
| core | 0x00a5b248 | `smHotKeyOffsetY` | hw1:Sensors.c |
| core | 0x00a6b00c | `strCurLanguage` | hw1:StringSupport.c |
| core | 0x00a6b010 | `strCurKeyboardLanguage` | hw1:StringSupport.c |
| core | 0x00a6b1e8 | `taskModuleInit` | hw1:Task.c |
| core | 0x00a6b1ec | `taskFunctionReturn` | hw1:Task.c |
| core | 0x00a6b1f0 | `taskFunctionExit` | hw1:Task.c |
| core | 0x00a6b1f4 | `taskFunctionContinue` | hw1:Task.c |
| core | 0x00a6b200 | `taskTimeDelta` | hw1:Task.c |
| core | 0x00a6b204 | `taskTimeElapsed` | hw1:Task.c |
| core | 0x00a6b208 | `cat_taskRealTimeDelta` | cat-new |
| core | 0x00a6b20c | `cat_taskRealTimeElapsed` | cat-new |
| core | 0x00a6b210 | `taskProcessIndex` | hw1:Task.c |
| core | 0x00a6b29c | `TitanActive` | hw1:TitanNet.c |
| core | 0x00a6c57c | `TUTORIAL_KAS_UPDATE_FRAME` | hw1:Tweak.c |
| core | 0x00a6c580 | `SP_KAS_UPDATE_FRAME` | hw1:Tweak.c |
| core | 0x00a6c5a4 | `repeattimes` | hw1:Universe.c |
| core | 0x00a6c5c4 | `universeUpdateCounter` | hw1:Universe.c |
| core | 0x00a74d94 | `DebugWindow` | hw1:main.c |
| core | 0x00a74d98 | `FilePathPrepended` | hw1:main.c |
| core | 0x00a74d9c | `CDROMPathPrepended` | hw1:main.c |
| core | 0x00a74da4 | `enableStipple` | hw1:main.c |
| core | 0x00a74dd0 | `mainDeviceToSelect` | hw1:main.c |
| core | 0x00a74e50 | `mainGLToSelect` | hw1:main.c |
| core | 0x00a75050 | `mainD3DToSelect` | hw1:main.c |
| core | 0x00a75174 | `SecretWON` | hw1:main.c |
| core | 0x00a7528c | `ghMainRegion` | hw1:mainrgn.c |
| core | 0x00a7620c | `dataEnvironment` | hw1:utility.c |
| core | 0x00a76214 | `utyNFrameTicks` | hw1:utility.c |
| core | 0x00a7621c | `utyName` | hw1:utility_mac.c |
| core | 0x00a7625c | `whichRaceSelected` | hw1:utility_mac.c |
| core | 0x00a762d8 | `BombersUseBombingRun` | hw1:CommandLayer.c |
| core | 0x00ab7620 | `utyStartedBits` | hw1:utility.c |
| core | 0x00ab765c | `utyBaseColor` | hw1:utility_mac.c |
| core | 0x00ab76b8 | `utyTimerLast` | hw1:utility.c |
| core | 0x00ab76c0 | `utyLockFilehandle` | hw1:utility.c |
| core | 0x00ab76c4 | `utyStripeColor` | hw1:utility_mac.c |
| core | 0x00ab76c8 | `utyRenderTask` | hw1:utility.c |
| core | 0x00ab76e0 | `errorString` | hw1:utility.c |
| core | 0x00ab7800 | `utyMemoryHeap` | hw1:utility.c |
| core | 0x00b39ccc | `ghMainWindow` | hw1:main.c |
| core | 0x00b39ef0 | `ghInstance` | hw1:main.c |
| core | 0x00ba1774 | `cdMaxShipsAllowed` | hw1:Universe.c |
| core | 0x00ba1e7c | `FEC_Background` | hw1:UIControls.c |
| core | 0x00ba1e80 | `FEC_ListItemStandard` | hw1:UIControls.c |
| core | 0x00ba1ea0 | `TW_MOVETO_PULSE_COLOR` | hw1:Tweak.c |
| core | 0x00ba1eac | `TW_HOLDING_PATTERN_NUM_TURNS` | hw1:Tweak.c |
| core | 0x00ba1ebc | `TW_SCUTTLE_CONFIRM_TIME` | hw1:Tweak.c |
| core | 0x00ba1ec8 | `TW_SECOND_SEE_PERCENTAGE` | hw1:Tweak.c |
| core | 0x00ba1ed8 | `TW_RU_READOUT_COLOR_BAD` | hw1:Tweak.c |
| core | 0x00ba1ee8 | `tweakNumTimesCheck` | hw1:Tweak.c |
| core | 0x00ba1efc | `TW_FIRST_SEE_PERCENTAGE` | hw1:Tweak.c |
| core | 0x00ba1f08 | `TW_HW_PING_RATE_IN` | hw1:Tweak.c |
| core | 0x00ba1f10 | `TW_DISTANCE_READOUT_COLOR` | hw1:Tweak.c |
| core | 0x00ba1f14 | `TW_FIRST_SEE_TIME` | hw1:Tweak.c |
| core | 0x00ba1f18 | `R1BulletColor` | hw1:Tweak.c |
| core | 0x00ba1f1c | `TW_MULTIPLAYER_HYPERSPACE_WAIT_IN_ETHER_TIME` | hw1:Tweak.c |
| core | 0x00ba1f20 | `TW_NOONE_SEE_TIME` | hw1:Tweak.c |
| core | 0x00ba1f24 | `TW_HOLDING_PATTERN_HEADING_COEFFICIENT` | hw1:Tweak.c |
| core | 0x00ba1f40 | `horseTotalNumBars` | hw1:Tweak.c |
| core | 0x00ba1f94 | `TW_HW_PING_MIN_SIZE_OUT` | hw1:Tweak.c |
| core | 0x00ba1f9c | `TW_RU_READOUT_COLOR_GOOD` | hw1:Tweak.c |
| core | 0x00ba1fa4 | `TW_ATTMOVETO_LINE_COLOR` | hw1:Tweak.c |
| core | 0x00ba1fac | `TW_HW_PING_RATE_OUT` | hw1:Tweak.c |
| core | 0x00ba1fb4 | `TW_HOLDING_PATTERN_PATH_MAX_SPEED` | hw1:Tweak.c |
| core | 0x00ba1fb8 | `TW_HW_PING_MAX_SIZE_IN` | hw1:Tweak.c |
| core | 0x00ba1fc0 | `TW_HyperSpaceCostStruct` | hw1:Tweak.c |
| core | 0x00ba2b14 | `tweakCheckMissileTargetLossRate` | hw1:Tweak.c |
| core | 0x00ba2b6c | `TW_HOLDING_PATTERN_PATH_DISTANCE` | hw1:Tweak.c |
| core | 0x00ba2b70 | `TW_HW_PING_COLOUR_IN` | hw1:Tweak.c |
| core | 0x00ba2bb4 | `TW_HOLDING_PATTERN_PATHTIME` | hw1:Tweak.c |
| core | 0x00ba2c30 | `tweakBoostIfFlightMan` | hw1:Tweak.c |
| core | 0x00ba2c40 | `TW_ATTMOVETO_PULSE_COLOR` | hw1:Tweak.c |
| core | 0x00ba2c44 | `TW_HW_PING_COLOUR_OUT` | hw1:Tweak.c |
| core | 0x00ba2c48 | `selShipHealthGreenFactor` | hw1:Tweak.c |
| core | 0x00ba2c4c | `TW_MOVETO_LINE_COLOR` | hw1:Tweak.c |
| core | 0x00ba2c50 | `tweakBoostIfDodging` | hw1:Tweak.c |
| core | 0x00ba2c54 | `TW_SECOND_SEE_TIME` | hw1:Tweak.c |
| core | 0x00ba2c5c | `TW_BURST_ATTACK_FORCE_MODIFIER` | hw1:Tweak.c |
| core | 0x00ba2c70 | `R2BulletColor` | hw1:Tweak.c |
| core | 0x00ba2c74 | `TW_HOLDING_PATTERN_RIGHT_COEFFICIENT` | hw1:Tweak.c |
| core | 0x00ba2c80 | `TW_CURSORTEXT_FONT` | hw1:Tweak.c |
| core | 0x00ba2d00 | `TM_Font` | hw1:Tweak.c |
| core | 0x00ba2d60 | `TW_DISTANCE_READOUT_FONT` | hw1:Tweak.c |
| core | 0x00ba2dbc | `TW_HYPERSPACE_TELEGRAPH_WAIT_TIME` | hw1:Tweak.c |
| core | 0x00ba2dc0 | `selShipHealthYellowFactor` | hw1:Tweak.c |
| core | 0x00ba2dc8 | `TW_HW_PING_MIN_SIZE_IN` | hw1:Tweak.c |
| core | 0x00ba2dd4 | `tweakMinDistBeforeTargetLoss` | hw1:Tweak.c |
| core | 0x00ba2dd8 | `TW_HW_PING_MAX_SIZE_OUT` | hw1:Tweak.c |
| core | 0x00ba2e00 | `TM_TechListFont` | hw1:Tweak.c |
| core | 0x00ba2e44 | `TW_PROXIMITY_RING_COLOUR` | hw1:Tweak.c |
| core | 0x00ba2e4c | `horseRaceRenderTime` | hw1:Tweak.c |
| core | 0x00bab7a0 | `PATCHSERVER_PORTS` | hw1:TitanInterfaceC.c |
| core | 0x00bab7e0 | `DIRSERVER_IPSTRINGS` | hw1:TitanInterfaceC.c |
| core | 0x00bb2da0 | `PATCHSERVER_IPSTRINGS` | hw1:TitanInterfaceC.c |
| core | 0x00bb2fa0 | `DIRSERVER_PORTS` | hw1:TitanInterfaceC.c |
| core | 0x00bb31a0 | `teNeutralColor` | hw1:Teams.c |
| core | 0x00bb31a4 | `teFriendlyColor` | hw1:Teams.c |
| core | 0x00bb31b0 | `teAlliedColor` | hw1:Teams.c |
| core | 0x00bb31b8 | `teHostileColor` | hw1:Teams.c |
| core | 0x00bb31e0 | `taskData` | hw1:Task.c |
| core | 0x00bb3268 | `cat_timeCompressionFactor` | cat-new |
| core | 0x00bb326c | `cat_timeCompressionIndex` | cat-new |
| core | 0x00bb3270 | `taskNumberCalls` | hw1:Task.c |
| core | 0x00bb3274 | `taskMaxTask` | hw1:Task.c |
| core | 0x00bb32a0 | `burstSpeed` | hw1:Tactics.c |
| core | 0x00bb3504 | `burstCoolDownTime` | hw1:Tactics.c |
| core | 0x00bb3508 | `kamikazeSpeedBurst` | hw1:Tactics.c |
| core | 0x00bb3510 | `burstRadius` | hw1:Tactics.c |
| core | 0x00bb3620 | `MessageStrings` | hw1:StringSupport.c |
| core | 0x00c097a0 | `starRedVariance` | hw1:Star3d.c |
| core | 0x00c097a4 | `starMinColor` | hw1:Star3d.c |
| core | 0x00c097a8 | `starBlueVariance` | hw1:Star3d.c |
| core | 0x00c097ac | `starMaxColor` | hw1:Star3d.c |
| core | 0x00c097b0 | `starGreenVariance` | hw1:Star3d.c |
| core | 0x00c0a180 | `MUSIC_DISTANCE_MAX` | hw1:SoundEvent.c |
| core | 0x00c0a188 | `SFX_CARDIOD_FACTOR` | hw1:SoundEvent.c |
| core | 0x00c0a18c | `SFX_AMBIENT_VOLUME` | hw1:SoundEvent.c |
| core | 0x00c0a190 | `CORVETTE_DOPPLER_HIGH` | hw1:SoundEvent.c |
| core | 0x00c0a198 | `SFX_CARDIOD_MIN` | hw1:SoundEvent.c |
| core | 0x00c0a19c | `MUSIC_MININACTIVE_VOL` | hw1:SoundEvent.c |
| core | 0x00c0a1a0 | `SPEECH_SINGLEPLAYER_RATIO` | hw1:SoundEvent.c |
| core | 0x00c0a1a4 | `MUSIC_TUTORIAL_VOL` | hw1:SoundEvent.c |
| core | 0x00c0a1a8 | `FIGHTER_DOPPLER_USEVELOCITY` | hw1:SoundEvent.c |
| core | 0x00c0a1b4 | `SFX_FLOAT_VELOCITY` | hw1:SoundEvent.c |
| core | 0x00c0a1b8 | `CORVETTE_DOPPLER_LOW` | hw1:SoundEvent.c |
| core | 0x00c0a1bc | `MUSIC_MAXGAME_VOL` | hw1:SoundEvent.c |
| core | 0x00c0a4b0 | `RANDOM_AMBIENCE_MINFREQ` | hw1:SoundEvent.c |
| core | 0x00c0a4b4 | `RANDOM_AMBIENCE_ADDRANDOM` | hw1:SoundEvent.c |
| core | 0x00c0a4d0 | `SPEECH_AMBIENT_LEVEL` | hw1:SoundEvent.c |
| core | 0x00c0a4d4 | `FIGHTER_VELOCITY_HIGHPITCH` | hw1:SoundEvent.c |
| core | 0x00c0a4e4 | `FIGHTER_DOPPLER_LOW` | hw1:SoundEvent.c |
| core | 0x00c0a500 | `MUSIC_DISTANCE_SILENT` | hw1:SoundEvent.c |
| core | 0x00c0a504 | `SFX_DAMAGERATIO_LIGHT` | hw1:SoundEvent.c |
| core | 0x00c0a508 | `MUSIC_MINBATTLE_VOL` | hw1:SoundEvent.c |
| core | 0x00c0a50c | `FIGHTER_DOPPLER_HIGH` | hw1:SoundEvent.c |
| core | 0x00c0a518 | `SFX_NIS_FLOAT_VELOCITY` | hw1:SoundEvent.c |
| core | 0x00c0a520 | `MUSIC_MINACTIVE_VOL` | hw1:SoundEvent.c |
| core | 0x00c0a530 | `CORVETTE_VELOCITY_HIGHPITCH` | hw1:SoundEvent.c |
| core | 0x00c0a534 | `SFX_DAMAGERATIO_MEDIUM` | hw1:SoundEvent.c |
| core | 0x00c0a538 | `SPEECH_MIN_PERCEPTABLE_VOL` | hw1:SoundEvent.c |
| core | 0x00c0a53c | `SFX_MIN_PERCEPTABLE_VOL` | hw1:SoundEvent.c |
| core | 0x00c0a544 | `FIGHTER_DOPPLER_SCALE` | hw1:SoundEvent.c |
| core | 0x00c0a550 | `CORVETTE_DOPPLER_SCALE` | hw1:SoundEvent.c |
| core | 0x00c0a558 | `SPEECH_CHATTER_RATIO` | hw1:SoundEvent.c |
| core | 0x00c0a588 | `MUSIC_MAXBATTLE_VOL` | hw1:SoundEvent.c |
| core | 0x00c0a58c | `FIGHTER_VELOCITY_LOWPITCH` | hw1:SoundEvent.c |
| core | 0x00c0a590 | `SFX_DAMAGERATIO_HEAVY` | hw1:SoundEvent.c |
| core | 0x00c0a5a0 | `CORVETTE_VELOCITY_LOWPITCH` | hw1:SoundEvent.c |
| core | 0x00c0a5a8 | `CORVETTE_DOPPLER_USEVELOCITY` | hw1:SoundEvent.c |
| core | 0x00c0caa8 | `selMinSelectionRadius` | hw1:Select.c |
| core | 0x00c2fd6c | `regTaskHandle` | hw1:Region.c |
| core | 0x00c30150 | `pieCircleSizeMax` | hw1:PiePlate.c |
| core | 0x00c30158 | `pieCircleSizeMin` | hw1:PiePlate.c |
| core | 0x00c30a30 | `opTimerStart` | hw1:Options.c |
| core | 0x00c32cc0 | `memGrowthPool` | hw1:Memory.c |
| core | 0x00c34f40 | `memMainPool` | hw1:Memory.c |
| core | 0x00c36a60 | `keySaveScan` | hw1:Key.c |
| core | 0x00c36b60 | `keyBuffer` | hw1:Key.c |
| core | 0x00c36b80 | `keyScanCode` | hw1:Key.c |
| core | 0x00c39320 | `ProcessSyncPktQ` | hw1:Globals.c |
| core | 0x00c39580 | `ProcessRequestedSyncPktQ` | hw1:Globals.c |
| core | 0x00c395c0 | `ProcessCaptaincyPktQ` | hw1:Globals.c |
| core | 0x00c39600 | `ProcessCmdPktQ` | hw1:Globals.c |
| core | 0x00c3c9a0 | `filePrependPath` | hw1:File.c |
| core | 0x00c3cac0 | `filePrependedPath` | hw1:File.c |
| core | 0x00c3cbe0 | `fileCDROMPath` | hw1:File.c |
| core | 0x00c3cd00 | `filesOpen` | hw1:File.c |
| core | 0x00c42b60 | `demDemoFilename` | hw1:Demo.c |
| core | 0x00c43400 | `SHIP_PROBS` | hw1:Crates.c |
| core | 0x00c435c0 | `NUM_IN_GROUPS` | hw1:Crates.c |
| core | 0x00c46b20 | `collBlobProperties` | hw1:Collision.c |
| core | 0x00c52160 | `updateTOC` | hw1:BigFile.c |
| core | 0x00c52170 | `mainTOC` | hw1:BigFile.c |
| core | 0x00c52370 | `SPEECH_MOSHIP_WARNING_TIME` | hw1:SoundEvent.c |
| core | 0x00c5237c | `SPEECH_WARNING_TIME` | hw1:SoundEvent.c |
| core | 0x00c52d80 | `callbacks` | hw1:Task.c |
| ships | 0x008894e8 | `FirstShipTypeOfRace` | hw1:ObjTypes.c |
| ships | 0x00889504 | `LastShipTypeOfRace` | hw1:ObjTypes.c |
| ships | 0x00889530 | `shiptypeinfo` | hw1:ObjTypes.c |
| ships | 0x008935a0 | `ShipNamesScriptTable` | cat-new |
| ships | 0x008b1938 | `shipHeaders` | hw1:Universe.c |
| ships | 0x008c3260 | `AcolyteHeader` | cat-new |
| ships | 0x008c3328 | `ACVHeader` | cat-new |
| ships | 0x008c3408 | `AdvanceSupportFrigateHeader` | hw1:AdvanceSupportFrigate.c |
| ships | 0x008c34d0 | `CarrierHeader` | hw1:Carrier.c |
| ships | 0x008c3720 | `ModuleShipHeader` | cat-new |
| ships | 0x008c37b0 | `CruiseMissileHeader` | cat-new |
| ships | 0x008c3860 | `CrystalHeader` | cat-new |
| ships | 0x008c3920 | `dBeastMothershipHeader` | cat-new |
| ships | 0x008c39b0 | `DefaultShipHeader` | hw1:DefaultShip.c |
| ships | 0x008c3a40 | `DoNothingShipHeader` | hw1:DefaultShip.c |
| ships | 0x008c3ad0 | `MiningBaseHeader` | hw1:DefaultShip.c |
| ships | 0x008c3b60 | `StationaryGunHeader` | hw1:DefaultShip.c |
| ships | 0x008c3ff8 | `DFGFrigateHeader` | hw1:DFGFrigate.c |
| ships | 0x008c4130 | `DreadnoughtHeader` | cat-new |
| ships | 0x008c4200 | `FloatingCityHeader` | hw1:FloatingCity.c |
| ships | 0x008c42d8 | `GenericDefenderHeader` | hw1:GenericDefender.c |
| ships | 0x008c4448 | `GenericInterceptorHeader` | hw1:GenericInterceptor.c |
| ships | 0x008c44d8 | `CloakedFighterHeader` | hw1:GenericInterceptor.c |
| ships | 0x008c4568 | `AttackBomberHeader` | hw1:GenericInterceptor.c |
| ships | 0x008c4868 | `GravWellGeneratorHeader` | hw1:GravWellGenerator.c |
| ships | 0x008c4998 | `HeavyCorvetteHeader` | hw1:HeavyCorvette.c |
| ships | 0x008c4ab0 | `HeavyCruiserHeader` | hw1:HeavyCruiser.c |
| ships | 0x008c4b40 | `HiveFrigateHeader` | cat-new |
| ships | 0x008c4bd0 | `IonCannonFrigateHeader` | hw1:IonCannonFrigate.c |
| ships | 0x008c4c60 | `LeechHeader` | cat-new |
| ships | 0x008c4d10 | `MCVHeader` | cat-new |
| ships | 0x008c4df0 | `MimicHeader` | cat-new |
| ships | 0x008c5040 | `MinelayerCorvetteHeader` | hw1:MinelayerCorvette.c |
| ships | 0x008c5320 | `MissileDestroyerHeader` | hw1:MissileDestroyer.c |
| ships | 0x008c53e8 | `MothershipHeader` | hw1:Mothership.c |
| ships | 0x008c55a8 | `MultiBeamFrigateHeader` | cat-new |
| ships | 0x008c5648 | `MultiGunCorvetteHeader` | hw1:MultiGunCorvette.c |
| ships | 0x008c56e8 | `NomadMoonHeader` | cat-new |
| ships | 0x008c5778 | `NomadFieldEmitterHeader` | cat-new |
| ships | 0x008c5808 | `P1IonArrayFrigateHeader` | hw1:P1IonArrayFrigate.c |
| ships | 0x008c58d8 | `P1MissileCorvetteHeader` | hw1:P1MissileCorvette.c |
| ships | 0x008c5968 | `P1MothershipHeader` | hw1:P1Mothership.c |
| ships | 0x008c5a88 | `P1StandardCorvetteHeader` | hw1:P1StandardCorvette.c |
| ships | 0x008c5b60 | `ProcessorHeader` | cat-new |
| ships | 0x008c5c88 | `ProximitySensorHeader` | hw1:ProximitySensor.c |
| ships | 0x008c5e38 | `RammingFrigateHeader` | cat-new |
| ships | 0x008c6170 | `RepairDroidHeader` | cat-new |
| ships | 0x008c6378 | `ResourceCollectorHeader` | hw1:ResourceCollector.c |
| ships | 0x008c6458 | `RKOHeader` | cat-new |
| ships | 0x008c6698 | `SalCapCorvetteHeader` | hw1:SalCapCorvette.c |
| ships | 0x008c6830 | `SentinelHeader` | cat-new |
| ships | 0x008c68c0 | `SlipgateHeader` | cat-new |
| ships | 0x008c6990 | `StandardDestroyerHeader` | hw1:StandardDestroyer.c |
| ships | 0x008c6a20 | `StandardFrigateHeader` | hw1:StandardFrigate.c |
| ships | 0x008c6ab0 | `SwarmerHeader` | cat-new |
| ships | 0x008c6b50 | `WorkerHeader` | cat-new |
| ships | 0x00b3b420 | `ShipStaticInfos` | cat-new |
| sim-cataclysm | 0x0072b228 | `cat_damageVsArmourTable` | cat-new |
| sim-cataclysm | 0x0072b318 | `cat_shipTypeArmourLevels` | cat-new |
| sim-cataclysm | 0x0072b7a0 | `cat_shipTypeUpgradeMasks` | cat-new |
| sim-cataclysm | 0x0072b9e8 | `cat_upgradeFlagTechTable` | cat-new |
| sim-cataclysm | 0x0072bb18 | `cat_weaponUpgradeTechTable` | cat-new |
| sim-cataclysm | 0x0072bb60 | `cat_armourUpgradeTechTable` | cat-new |
| sim-cataclysm | 0x0072bb78 | `cat_experienceDamageScale` | cat-new |
| sim-cataclysm | 0x0072bb8c | `cat_experienceSpeedScale` | cat-new |
| sim-cataclysm | 0x00751698 | `cat_beastWeaponScriptTable` | cat-new |
| sim-cataclysm | 0x0087f720 | `fowFunctionNames` | cat-new |
| sim-cataclysm | 0x008860e8 | `tpGameCreated_aiplayerDifficultyLevel` | hw1:TitanInterfaceC.c |
| sim-cataclysm | 0x008860e9 | `tpGameCreated_aiplayerBigotry` | hw1:TitanInterfaceC.c |
| sim-cataclysm | 0x00889ea8 | `cat_weapontypeinfo` | cat-new |
| sim-cataclysm | 0x008901a8 | `techtypeinfo` | hw1:ResearchAPI.c |
| sim-cataclysm | 0x008902f8 | `cat_moduletypeinfo` | cat-new |
| sim-cataclysm | 0x008c53c0 | `cat_bigGunAimTime` | cat-new |
| sim-cataclysm | 0x008c53d0 | `cat_bigGunAimDotThreshold` | cat-new |
| sim-cataclysm | 0x008c53d4 | `cat_bigGunMaxRangeSP` | cat-new |
| sim-cataclysm | 0x008c53dc | `cat_bigGunMaxRangeMP` | cat-new |
| sim-cataclysm | 0x00a3e2f0 | `fowCurrentGrid` | cat-new |
| sim-cataclysm | 0x00a6c534 | `g_bNeedMoreSpaceForOtherLanguage` | cat-new |
| sim-cataclysm | 0x00a6c538 | `g_bNeedReverseWordsForOtherLanguage` | cat-new |
| sim-cataclysm | 0x00a752b4 | `cat_waypointMode` | cat-new |
| sim-cataclysm | 0x00c52140 | `cat_beastWeaponTweaks` | cat-new |

## Class-only entries not emitted (74)

These are library functions that Ghidra already names with a FID/demangled name that is not a C identifier. They were left untouched. The class is listed here only.

| address | Ghidra name | class | sets |
|---|---|---|---|
| 0x00672aa0 | `FID_conflict:__expand` | crt | lib-graph,lib-strings |
| 0x00672ad0 | `FID_conflict:__realloc_dbg` | crt | lib-graph,lib-strings |
| 0x00675680 | `FID_conflict:_store_str` | crt | lib-graph,lib-strings |
| 0x00675780 | `FID_conflict:_store_number` | crt | lib-graph,lib-strings |
| 0x00678500 | `FID_conflict:__getenv_lk` | crt | lib-graph,lib-strings |
| 0x00679380 | `exception::~exception` | crt | lib-graph,lib-strings |
| 0x0067a6b0 | ``eh_vector_constructor_iterator'` | crt | lib-graph,lib-strings |
| 0x006859a0 | `FID_conflict:__open` | crt | lib-graph,lib-strings |
| 0x006878c0 | `FID_conflict:__set_errno_from_matherr` | crt | lib-graph,lib-strings |
| 0x00689e60 | `FID_conflict:___AdjustPointer` | crt | lib-graph,lib-strings |
| 0x00689eb0 | `__CallSettingFrame@12` | crt | lib-graph,lib-strings |
| 0x0068bf50 | `__rt_probe_read4@4` | crt | lib-graph,lib-strings |
| 0x00705e40 | `FID_conflict:~logic_error` | crt | lib-graph,lib-strings |
| 0x007060b0 | `std::ios_base::_Callfns` | crt | lib-graph,lib-strings |
| 0x00706230 | `std::ios_base::_Tidy` | crt | lib-graph,lib-strings |
| 0x00706300 | `std::basic_string<char,std::char_traits<char>,std::allocator<char>_>::assign` | crt | lib-graph,lib-strings |
| 0x007069d0 | `std::basic_string<char,std::char_traits<char>,std::allocator<char>_>::operator=` | crt | lib-graph,lib-strings |
| 0x00706fa0 | `FID_conflict:codecvt<char,char,int>` | crt | lib-graph,lib-strings |
| 0x007070e0 | `std::_Locinfo::_Getctype` | crt | lib-graph,lib-strings |
| 0x00707260 | `~ctype<>` | crt | lib-graph,lib-strings |
| 0x00707700 | `codecvt<>` | crt | lib-graph,lib-strings |
| 0x007077c0 | `FID_conflict:codecvt<char,char,int>` | crt | lib-graph,lib-strings |
| 0x007083a0 | `FID_conflict:codecvt<char,char,int>` | crt | lib-graph,lib-strings |
| 0x00708420 | `std::ios_base::precision` | crt | lib-graph,lib-strings |
| 0x007085f0 | `FID_conflict:_Getcoll` | crt | lib-graph,lib-strings |
| 0x00709220 | `std::basic_streambuf<char,std::char_traits<char>_>::sbumpc` | crt | lib-graph,lib-strings |
| 0x007092b0 | `std::basic_streambuf<char,std::char_traits<char>_>::egptr` | crt | lib-graph,lib-strings |
| 0x0070ab90 | `std::ios_base::fail` | crt | lib-graph,lib-strings |
| 0x0070ae40 | `std::fpos<int>::fpos<int>` | crt | lib-graph,lib-strings |
| 0x0070af50 | `std::basic_streambuf<char,std::char_traits<char>_>::gbump` | crt | lib-graph,lib-strings |
| 0x0070b2d0 | `FID_conflict:`vbase_destructor'` | crt | lib-graph,lib-strings |
| 0x0070b360 | `FID_conflict:`vbase_destructor'` | crt | lib-graph,lib-strings |
| 0x0070b8e0 | `~basic_ostream<>` | crt | lib-graph,lib-strings |
| 0x0070cdb0 | `FID_conflict:gbump` | crt | lib-graph,lib-strings |
| 0x0070d1d0 | `std::strstreambuf::strstreambuf` | crt | lib-graph,lib-strings |
| 0x0070d2b0 | `FID_conflict:~istrstream` | crt | lib-graph,lib-strings |
| 0x0070d320 | `std::strstreambuf::~strstreambuf` | crt | lib-graph,lib-strings |
| 0x0070e6b0 | `codecvt<>` | crt | lib-graph,lib-strings |
| 0x0070ea70 | `std::ctype<char>::ctype<char>` | crt | lib-graph,lib-strings |
| 0x0070eed0 | `std::_Timevec::~_Timevec` | crt | lib-graph,lib-strings |
| 0x0070f0a0 | `FID_conflict:codecvt<char,char,int>` | crt | lib-graph,lib-strings |
| 0x0070fc60 | `FID_conflict:codecvt<char,char,int>` | crt | lib-graph,lib-strings |
| 0x00710350 | `FID_conflict:money_put<wchar_t,std::ostreambuf_iterator<wchar_t,std::char_traits<wchar_t>_>_>` | crt | lib-graph,lib-strings |
| 0x00710530 | `FID_conflict:codecvt<char,char,int>` | crt | lib-graph,lib-strings |
| 0x00710660 | `FID_conflict:codecvt<char,char,int>` | crt | lib-graph,lib-strings |
| 0x00710900 | `FID_conflict:codecvt<char,char,int>` | crt | lib-graph,lib-strings |
| 0x00710c60 | `moneypunct<>` | crt | lib-graph,lib-strings |
| 0x00710d90 | `moneypunct<>` | crt | lib-graph,lib-strings |
| 0x00710dc0 | `FID_conflict:codecvt<char,char,int>` | crt | lib-graph,lib-strings |
| 0x00711280 | `~ctype<>` | crt | lib-graph,lib-strings |
| 0x007112f0 | `~time_put<>` | crt | lib-graph,lib-strings |
| 0x007117f0 | `std::ctype<char>::_Init` | crt | lib-graph,lib-strings |
| 0x007118a0 | `std::_Timevec::operator=` | crt | lib-graph,lib-strings |
| 0x007119e0 | `FID_conflict:operator*` | crt | lib-graph,lib-strings |
| 0x00712ea0 | `FID_conflict:_Getcoll` | crt | lib-graph,lib-strings |
| 0x00714650 | `FID_conflict:_Getmonths` | crt | lib-graph,lib-strings |
| 0x00716310 | `std::_LStrxfrm<unsigned_short>` | crt | lib-graph,lib-strings |
| 0x00716980 | `moneypunct<>` | crt | lib-graph,lib-strings |
| 0x00717d30 | `FID_conflict:money_put<wchar_t,std::ostreambuf_iterator<wchar_t,std::char_traits<wchar_t>_>_>` | crt | lib-graph,lib-strings |
| 0x00717ed0 | `std::collate<char>::do_hash` | crt | lib-graph,lib-strings |
| 0x00717f20 | `FID_conflict:codecvt<char,char,int>` | crt | lib-graph,lib-strings |
| 0x00718020 | `FID_conflict:codecvt<char,char,int>` | crt | lib-graph,lib-strings |
| 0x007182c0 | `FID_conflict:codecvt<char,char,int>` | crt | lib-graph,lib-strings |
| 0x00718600 | `moneypunct<>` | crt | lib-graph,lib-strings |
| 0x00718770 | `moneypunct<>` | crt | lib-graph,lib-strings |
| 0x007187a0 | `FID_conflict:codecvt<char,char,int>` | crt | lib-graph,lib-strings |
| 0x00719010 | `~time_put<>` | crt | lib-graph,lib-strings |
| 0x0071aa20 | `std::num_put<char,std::ostreambuf_iterator<char,std::char_traits<char>_>_>::_Put` | crt | lib-graph,lib-strings |
| 0x0071aa70 | `std::money_put<char,std::ostreambuf_iterator<char,std::char_traits<char>_>_>::_Rep` | crt | lib-graph,lib-strings |
| 0x0071b900 | `moneypunct<>` | crt | lib-graph,lib-strings |
| 0x0071d2b0 | `FID_conflict:__non_rtti_object` | crt | lib-graph,lib-strings |
| 0x0071d2e0 | `FID_conflict:bad_typeid` | crt | lib-graph,lib-strings |
| 0x0071d3b0 | `FID_conflict:evaluation_error` | crt | lib-graph,lib-strings |
| 0x0071dac0 | `FID_conflict:__ctime64` | crt | lib-graph,lib-strings |

## Open items for the next round

1. Re-admit the 925 dropped data names once rule 5 accepts mapped `.data`/`.rdata` addresses.
2. Rename CET's 0x00673080 `memFree` to `rinMemFree` in cet_symbols.json, so HW1 memFree (0x004ae130) can drop its `_2` suffix.
3. Give 0x005ab5b0 (`utyGameQuitToMain_2`) a `cat_` name.
4. Add enum classes for ImageMagick, IJG and the copy-protection stub (now `class=unknown; lib=magick|ijg|cms`), and decide whether IJG should be `hw1:<jfile.c>`, since HW1 ships src/JPG.
5. Mission Init/Watch code (0x5d6fe0-0x670a10) still has no function entries, so none of it could be named here.
