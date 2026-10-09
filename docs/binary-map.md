# Binary map, rounds 1-2: cataclysm.exe (GOG build 12948)

> **Round 2 (2026-10-09)** is the "Round 2" section at the end. 14,384 functions; 7,055 have real names, covering 59.7% of function bytes. The game region is 65.7% real-named, and all 2,085 mission functions are named. The round-3 targets are in R2.6. Sections 1-9 below are round 1 as written then; where round 2 changed a fact (mission code, ship slots, data names, struct types, the memFree compromise), the Round 2 section is authoritative.

Generated 2026-10-09 from the Ghidra re-export `ghidra/exports/cataclysm_gog_r1/` after `ghidra/symbols_round1.json` (8,312 symbols from 9 audited proposal sets) was applied to project `ghidra/projects/cataclysm_gog`. Image base 0x400000; all addresses are VAs. Baseline numbers come from the pre-apply export `ghidra/exports/cataclysm_gog/`. Related: `docs/mission-code-map.md` (KAS / mission code), `ghidra/proposals/reconcile_report.md` (how the 9 sets were merged), `docs/cet-knowledge.md`, `docs/binary-fingerprint.md`.

**Naming tiers used in every table.** *Real name* = an identifier from CET, a producer set, FID or an import (for example `univUpdate`, `_sprintf`, `cat_kasFunctionTable`). *Class-only* = a library function whose library is known but whose name is not: synthetic `lib_<class>_<addr>` names, EH funclets `lib_<class>_Unwind_<addr>`, and Ghidra's own `Unwind@`/`Catch@`/`thunk_FUN_`/`switchD_` names. *Unnamed* = still `FUN_*`. "Named" in the narrow sense (`named_functions` below) means not `FUN_*`, so it includes class-only names.

## 1. Apply pass

| item | value |
|---|---|
| command | `tools/ghidra_headless.sh --symbols ghidra/symbols_round1.json --export-name cataclysm_gog_r1 original/game/cataclysm.exe cataclysm_gog` (reuse mode, `-noanalysis`, ImportSymbolsJson phase=all, then ExportAnalysis with decompile) |
| run | 2026-10-09 08:32, first attempt (no project-lock retry needed), rc=0, wall 40 s; no `ERROR`/Traceback lines in `logs/headless.out` |
| target check | sha256 f03d9a3e... matches; only the informational exe-name note was logged |
| symbols applied | **8,312 of 8,312**: 8,278 `function` + 34 `label` (data). 0 `bad-address-or-name`, 0 `not-in-memory`, 0 `error`, 0 `label-only`, 0 `inside:` (no address fell mid-function) |
| function names | 8,103 renamed, 173 already had the name, **2 `kept-user-name`**: `_threadstart` 0x0067a0a0 and `_threadstartex` 0x0071cdc0. Ghidra holds USER_DEFINED names `lpStartAddress_0067a0a0` / `lpStartAddress_0071cdc0` there (created by the thread-start analyzer), and ImportSymbolsJson never overwrites USER_DEFINED, so our names went on as secondary labels. Functions still print as `lpStartAddress_*` |
| prototypes | 862 applied as given, 1,006 applied degraded (unknown HW1 types -> `void *`/`undefined4`), 6,408 entries had no prototype, 2 unparsed: `univHideShipFromSpheres` 0x00547c20 and `univReallyDeleteThisDerelictRightNow` 0x00549700 |
| data labels | 34 applied; 33 had no type, 1 type not understood: `cat_kasFunctionTable` 0x00755048 (`FunctionCall[487]` is not a known Ghidra type, so the label is untyped) |
| CET patch-site containers | 0 renamed, 0 conflicts (all were named in earlier runs) |
| decompile | ok 11,105, thunk 86, failed 3 (`lib_crt_00671da2`, and .data artefacts `FUN_00892106`, `FUN_00892114`), error 0 |
| per-symbol report | `ghidra/exports/cataclysm_gog_r1/cet_apply_report.csv` |

**Apply failures: 0.** The two `kept-user-name` rows are applied (as labels) but do not show as function names.

## 2. Coverage

### 2.1 Overall (11,194 functions, 2,194,228 function bytes)

| tier | baseline functions | baseline bytes | round 1 functions | round 1 bytes | round 1 % of bytes |
|---|---:|---:|---:|---:|---:|
| real name | 379 | 68,010 | 3,867 | 1,019,110 | 46.4% |
| class-only | 2,502 | 28,898 | 5,125 | 521,493 | 23.8% |
| unnamed `FUN_*` | 8,313 | 2,097,320 | 2,202 | 653,625 | 29.8% |
| **not `FUN_*`** | 2,881 | 96,908 | **8,992** | **1,540,603** | **70.2%** |

Baseline "real name" is lower than the 2,828 "already named" quoted earlier because 2,343 of those were Ghidra `Unwind@`/`Catch@` funclet names, which this table counts as class-only. Function bytes are Ghidra function body sizes; .text is 3,313,664 bytes, and the 629,296-byte mission range 0x5d6fe0-0x670a0f has no functions at all (section 6).

### 2.2 Per section

| section | functions | bytes | real | class-only | `FUN_*` | `FUN_*` bytes |
|---|---:|---:|---:|---:|---:|---:|
| .text | 10,980 | 2,157,828 | 3,847 | 4,973 | 2,160 | 653,583 |
| .cms_t | 172 | 36,358 | 20 | 152 | 0 | 0 |
| .data | 42 | 42 | 0 | 0 | 42 | 42 |

The 42 `.data` functions are one-byte disassembly artefacts at 0x00891f6c-0x00892114 (lib-graph); they are not real code.

### 2.3 Per address region (round 1, with baseline `FUN_*` share)

| region | functions | real | class-only | `FUN_*` | real % bytes | `FUN_*` % bytes (baseline) |
|---|---:|---:|---:|---:|---:|---:|
| game C/C++ 0x401000-0x5ca0ff | 5,079 | 2,786 | 133 | 2,160 | 55.3% | 43.8% (97.0%) |
| IJG libjpeg 0x5ca100-0x5d6fdf | 102 | 26 | 76 | 0 | 10.7% | 0% (100%) |
| kas2c mission code 0x5d6fe0-0x670a0f | 0 | 0 | 0 | 0 | - | no functions |
| WSOCK32 thunks + static CRT 0x670a10-0x69412f | 595 | 256 | 339 | 0 | 41.1% | 0% (88.5%) |
| WON/Titan + AdApi + Magick 0x694130-0x705c9f | 2,105 | 627 | 1,478 | 0 | 31.2% | 0% (99.1%) |
| Dinkumware C++ std lib 0x705ca0-0x71cccf | 708 | 124 | 584 | 0 | 9.3% | 0% (93.2%) |
| late CRT 0x71ccd0-0x71f62f | 48 | 28 | 20 | 0 | 58.3% | 0% (86.8%) |
| text$x EH funclets 0x71f630-0x729fff | 2,343 | 0 | 2,343 | 0 | 0% | 0% (0%) |
| .cms_t protection stub + CRT 0xc55000-0xc68fff | 172 | 20 | 152 | 0 | 5.1% | 0% (94.9%) |

Every remaining `FUN_*` in code is in the game region 0x401000-0x5c9fff. Library carving is complete: no `FUN_*` is left at or above 0x5ca100.

### 2.4 Per class

Class comes from the `class=` note of the applied symbol. Rows `CET` are CET function names that no round-1 set re-proposed (they keep CET's name; `CET (hw1)` ones carry an HW1 `hw1_equivalent`). `other named` = Ghidra names outside round 1 (469 `Unwind@`, 72 `Catch@`, 36 `FID_conflict:`, 36 thunks, 23 C++/switch names, 26 plain identifiers including the 10 CET_container names).

| class | functions | bytes | real | class-only | high | medium | low | lowest - highest entry |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| hw1:* (all files) | 2,217 | 668,101 | 2,217 | 0 | 1,841 | 371 | 5 | 0x004018c0 - 0x005be2e0 |
| cat-new | 491 | 120,482 | 491 | 0 | 266 | 209 | 16 | 0x00417ec0 - 0x005c99f0 |
| crt | 1,912 | 276,949 | 334 | 1,578 | 1,582 | 330 | 0 | 0x00553420 - 0x00c67ca0 |
| won | 3,171 | 310,100 | 603 | 2,568 | 3,022 | 149 | 0 | 0x00694130 - 0x007276f1 |
| unknown, lib=magick (ImageMagick/Magick++) | 360 | 82,199 | 15 | 345 | 281 | 79 | 0 | 0x006e3de0 - 0x00727bd0 |
| unknown, lib=ijg (IJG libjpeg 6a) | 76 | 22,622 | 0 | 76 | 43 | 33 | 0 | 0x005ca170 - 0x005d6d10 |
| unknown, lib=cms (.cms_t stub) | 12 | 7,621 | 0 | 12 | 0 | 12 | 0 | 0x00c55000 - 0x00c60540 |
| unknown, named (IJG API + ICF stubs) | 28 | 2,722 | 28 | 0 | 22 | 6 | 0 | 0x0049ae30 - 0x006fe9a0 |
| bink | 8 | 2,503 | 8 | 0 | 6 | 2 | 0 | 0x00551220 - 0x00551d40 |
| rgl | 3 | 3,586 | 3 | 0 | 1 | 2 | 0 | 0x00559a80 - 0x0055dd30 |
| CET (hw1) | 42 | 14,724 | 42 | 0 | 0 | 0 | 0 | 0x00420880 - 0x005a7a70 |
| CET (other) | 10 | 6,862 | 10 | 0 | 0 | 0 | 0 | 0x00431510 - 0x005bc900 |
| other named | 662 | 22,132 | 116 | 546 | 0 | 0 | 0 | 0x00425ca0 - 0x00727750 |
| unnamed `FUN_*` | 2,202 | 653,625 | 0 | 0 | 0 | 0 | 0 | 0x00401000 - 0x00892114 |

Class counts here are function entries only; `reconcile_report.md` counts the 34 data symbols too (hw1 2,249, cat-new 493). The crt class reaches down to 0x553420 because CRT/std COMDATs instantiated in the game's own Titan glue (0x552000-0x5a4900) are classed crt (the reconcile rule for its 62 crt-vs-won conflicts), and up to 0xc67ca0 because .cms_t carries its own release CRT.

### 2.5 What each set contributed to the applied file

| set | entries in symbols_round1 | with a real name | only this set | audit (sampled / wrong / precision) |
|---|---:|---:|---:|---|
| sim-presentation | 1,003 | 1,003 | 978 | 30 / 0 / 1.00 |
| missions | 537 | 537 | 536 | 30 / 0 / 1.00 |
| sim-universe | 474 | 474 | 440 | 30 / 1 / 0.967 |
| core | 379 | 379 | 313 | 30 / 1 / 0.967 |
| hw1-match | 188 | 188 | 115 | 30 / 0 / 1.00 |
| sim-cataclysm | 176 | 176 | 164 | 30 / 0 / 1.00 |
| ships | 101 | 101 | 100 | 30 / 1 / 0.967 |
| lib-strings | 5,141 | 965 | 909 | 30 / 0 / 1.00 |
| lib-graph | 4,647 | 101 | 418 | 30 / 0 / 1.00 |
| cet (agreement only) | 4 | 4 | 2 | - |

core and ships lost most of their entries to rule 5 (data addresses absent from symbols.csv/strings.csv; 844 core and 57 ships data names dropped, section 9).

## 3. Method, per producer

**lib-strings (5,232 proposals; high 4,655 / medium 577).** Library carving by binary identity and link layout. Functions were byte-compared, relocations masked, against the HW1-era Titan/WON static libraries (`gardens-of-kadesh` `Lib/Titan/*_W95.lib`), keeping unique matches only. Names were then carried to callees through REL32 call-site relocations of verified matches. EH funclets took their parent's class through the `__CxxFrameHandler` FuncInfo tables, Ghidra FID names were kept, and the game's own wrappers in `bink.c`, `gldll.c` and `glcaps.c` were anchored by string. It also fixed the linker blocks in section 4. The producer left `method` as a placeholder; the auditor rebuilt it in `lib-strings.verified.json`. Audit: 30 sampled, 0 wrong.

**lib-graph (5,596; high 2,573 / medium 3,011 / low 12).** Call-graph and linker-layout carving, with no string matching. On top of `functions.csv`/`call_graph.csv` it parsed three structures from the exe: RTTI CompleteObjectLocators to vftables (311), EH ThrowInfo/CatchableType (37 thrown types), and the 908 MSVC6 `__ehhandler` stubs, whose FuncInfo maps tie 2,336 of 2,343 `Unwind@` and 105 `Catch@` funclets to their parents. Import-thunk runs mark library boundaries. WON and ImageMagick were split by call direction (WON calls ImageMagick, never the reverse), with no conflicts. It named WinMain 0x005631f0 (sole caller: the CRT entry 0x0067bbe0) and the crt0.c start-up chain. Audit: 30 / 0 wrong.

**hw1-match (189; high 187 / medium 2).** `MatchHW1Strings.py` scored functions by the HW1 source strings they reference (output: `ghidra/exports/cataclysm_gog_hw1match/hw1_string_matches.csv`, 355 rows). Candidates were then reviewed by hand against HomeworldSDL (80 `[review]`, 38 `[audit]`), and 71 `[callee]` entries were grown through the call graph around confirmed matches. Where kept, rows at score >= 0.8 had the matcher's name right about 89% of the time; below 0.8 the matcher was close to useless. Its main failure is MSVC6 inlining: it gives the outer function the name of the helper that owns the string (14 corrections, for example `SEloadscripts` -> `speechEventInit`). Audit: 30 / 0 wrong.

**core (1,227; high 1,062 / medium 159 / low 6).** Core and platform pass. It anchored on WinMain and the CET-named `utyGameSystemsPreInit`/`utyGameSystemsInit`, then walked their call order against Relic's 2003 source (GoK root ad1bb82, `Win32/utility.c`, `main.c`) and the `utyStartedBits` pairs in `utyGameSystemsShutdown`. Task, memory, file/bigfile/BitIO, statscript, Strings, key, demo, debug, ddraw, LinkedList, Queue, FEFlow and SaveGame code was found by error strings, the call graph and the original function order. Globals came from decompiled DAT_ uses and from a `.data` scan of 12-byte scriptEntry rows `{name, cb, &var}`. Audit: 30 / 1 wrong (dropped `SEloadscripts` 0x00521a40, which is the stream half of speechEventInit).

**sim-universe (474; high 327 / medium 145 / low 2).** Simulation core. It anchored on CET `univUpdate` 0x0054c3f0 and walked its decompile line by line against HomeworldSDL `UnivUpdate.c:7292`, naming each per-tick callee by its call slot. It then aligned each HW1 source file's function order with the binary address order (MSVC6 keeps per-file source order) and confirmed each pick from its decompile. The command layer was mapped from `clWrap*` (CET) to the SP `cl*`/MP `clSend*` pairs, and the `clProcess` switch cases to `process*ToDo`. Audit: 30 / 1 wrong (dropped `DeleteDeadResource` 0x00548f40).

**sim-cataclysm (200; high 87 / medium 104 / low 9).** Cataclysm-only mechanics (`cat_*`). These were found from string literals (the `beastweapon.script` scriptStructEntry table, `english.script` keys resolved through the language scriptEntry table, debug formats, `SALVAGING:*`, `fow*`) and from callers of named HW1/CET functions that have no HW1 counterpart. Each decompile was read and its struct offsets matched to CET's Ship/ShipStaticInfo/Player fields (for example beastAttackCascadeCost +0x3DC, fowRadius +0x240, supportUnits +0x410). Audit: 30 / 0 wrong.

**sim-presentation (1,003; high 852 / medium 150 / low 1).** Presentation and network. The exe keeps alphabetical per-directory link order (`Game/*.c`, then `Win32/*.c`), so each HW1 file maps to a contiguous address window between already-named neighbours. Inside each window, functions were aligned with the original Relic source in source order and confirmed by reading the decompile: strings, packet constants (0xcccc, 0xca00, 0xa1a1), struct sizes, shared globals and agreement with HW1 call sites. The WON glue was named from TitanInterface's own `TitanInterface::X` debug strings. Audit: 30 / 0 wrong.

**ships (159; high 85 / medium 71 / low 3).** Ship tables, read from exe bytes plus the exports. Section 7 has the details. Slot targets were found by locating every `call [staticinfo+0x10C..0x190]` site and matching it to its HW1 caller. Headers were attributed by the ship types that use them and by object-file order. Audit: 30 / 1 wrong (dropped `gunCanShoot` 0x0048a1c0, a Cataclysm front end; the real gunCanShoot is 0x0048a2f0).

**missions (538; high 523 / medium 15).** KAS. Hashing every name in the embedded kas2c table and matching the hashes to the dispatcher branches gave every kasf address exactly. The KAS.c runtime was matched by body, call order and callers. Details are in section 6 and `docs/mission-code-map.md`. Audit: 30 / 0 wrong; all 475 kasf addresses were cross-checked against the exe again.

**reconcile (symbols_round1.json, 8,312).** Merged the 9 `*.verified.json` sets (qwen-naming.json excluded as unaudited). Name precedence: CET, then audited-high hw1 with an hw1_equivalent, then confidence, then agreement, then specificity. There were 2 evidence exceptions (`animBinkPlay` 0x00421860, `teReset` 0x0052b800). CET names are never overridden without two independent high-confidence sets (0 overrides). 17 name conflicts and 71 class conflicts were resolved, 91 collisions were made unique with `_2`/`_3`, 623 C++ names were flattened `A::B` -> `A__B`, and class-only library entries got `lib_<class>_<addr>` names. Rule 5 dropped 925 data entries whose addresses are not in symbols.csv/strings.csv. Full log: `ghidra/proposals/reconcile_report.md`.

## 4. Library ranges

| range | content | class | evidence |
|---|---|---|---|
| 0x401000-0x5ca0ff | game objects: HW1 + Cataclysm C, with game C++ Titan glue at ~0x552000-0x5a4900 (carries std/WON COMDATs) | hw1:* / cat-new | lib-graph link layout; sim-* windows |
| 0x551220-0x551d40 | game wrappers around Bink (`bink.c`) | bink (8) | lib-strings string anchors |
| 0x559a80-0x55dd30 | rGL loader wrappers (`gldll.c`/`glcaps.c`) | rgl (3) | lib-strings; reconcile reclassed 0x559500 glCapGetRGLAddresses to hw1:glcaps.c |
| 0x5ca100-0x5d6fdf | IJG libjpeg 6a, closed cluster entered only from game glue 0x5c9cd0/0x5c9dc0/0x5c9f80 | unknown, lib=ijg | lib-graph; IJG create/error API named |
| 0x5d6fe0-0x670a0f | kas2c-generated mission Init/Watch code (2,085 functions, none created in Ghidra) | - | mission-code-map.md section 1 |
| 0x670a10-0x670a45 | WSOCK32 import thunks | - | lib-graph |
| 0x670a50-0x69411d | MSVC6 static debug CRT (LIBCMTD); entry `_WinMainCRTStartup` 0x0067bbe0 | crt | crt0.c call order; FID; lib-strings |
| 0x694130-0x6e3c0a, 0x6eae30-0x6f139d | WON/Titan SDK incl. CryptoPP; WON AdApi | won | masked-reloc match to Titan `*_W95.lib` |
| 0x6e3de0-0x6eae2f, 0x6f13e0-0x705c81 | Magick++ / ImageMagick C (PE exports `SetClientName`, `opendir`, `NTElapsedTime`, ... are ImageMagick `MagickExport`s) | unknown, lib=magick | call-direction split from WON |
| 0x705ca0-0x71cbf0 | Dinkumware C++ std lib (LIBCP: iostream/locale/strstream) | crt | lib-strings/lib-graph agree |
| 0x71ccd0-0x71f62d | late CRT members (threadex, asctime, RTTI dynamic_cast, wide stdio) | crt | lib-graph |
| 0x71f630-0x729a5f | text$x EH funclets + stubs; 2,336 of 2,343 Unwind@ tied to parents | parent's class | 908 `__ehhandler` FuncInfo maps |
| 0xc55000-0xc6068f | copy-protection stub in .cms_t (SecuROM inferred from the section name only) | unknown, lib=cms | lib-graph |
| 0xc60690-0xc67ca5 | the stub's own static release CRT | crt | lib-graph |

ICF-folded shared stubs: `nullsub` 0x0049ae30 (lone `ret`, 138 call sites, was the PE export `seekdir`) and `nullsubReturnFalse` 0x006fe9a0 (`xor eax,eax; ret`, was `telldir`). Both are used as several kasf names, as ship-header slots and by unrelated callers, so they keep generic names.

## 5. Per HW1 source file

`matched` = functions applied with class `hw1:<File>`, plus CET function names whose hw1_equivalent is in that file (`CET` column). `defs` = function definitions counted roughly in `reference/HomeworldSDL/src/**/<File>`: lines at column 0 that look like `type name(` with a `{` before the next `;`. #if/#else branches are counted twice and macro-generated functions are missed, so treat `defs` as approximate. `verbatim` = matched names, `_N` suffix stripped, that are also a definition name in that HomeworldSDL file; a gap means the name comes from Relic's 2003 source or the port renamed it. `bytes` = sum of matched function sizes.

Totals: 2,259 matched functions in 153 files (verbatim 2,089). These 153 files define about 6,303 functions. HomeworldSDL `src/` has 215 .c/.cpp files with about 7,113 definitions in all, which includes Mac/SDL-only and stub files that are not in the Win32 build.

| HW1 file | path under src/ | matched | of which CET | defs (approx.) | matched / defs | verbatim | bytes |
|---|---|---:|---:|---:|---:|---:|---:|
| KASFunc.c | Game/KASFunc.c | 293 | 0 | 313 | 93.6% | 293 | 16,460 |
| TitanInterface.cpp | SDL/TitanInterface.cpp | 112 | 0 | 123 | 91.1% | 28 | 56,453 |
| UnivUpdate.c | Game/UnivUpdate.c | 97 | 4 | 139 | 69.8% | 96 | 39,956 |
| MultiplayerGame.c | Game/MultiplayerGame.c | 78 | 3 | 233 | 33.5% | 77 | 20,984 |
| CommandLayer.c | Game/CommandLayer.c | 59 | 0 | 144 | 41.0% | 59 | 35,217 |
| CommandNetwork.c | Game/CommandNetwork.c | 53 | 0 | 68 | 77.9% | 52 | 15,236 |
| Tutor.c | Game/Tutor.c | 42 | 0 | 80 | 52.5% | 42 | 7,351 |
| ConsMgr.c | Game/ConsMgr.c | 40 | 0 | 87 | 46.0% | 40 | 13,006 |
| FEFlow.c | Game/FEFlow.c | 38 | 1 | 71 | 53.5% | 35 | 11,322 |
| Options.c | Game/Options.c | 37 | 1 | 154 | 24.0% | 37 | 8,648 |
| TitanNet.c | Game/TitanNet.c | 35 | 0 | 39 | 89.7% | 35 | 3,507 |
| SoundEvent.c | Game/SoundEvent.c | 32 | 0 | 51 | 62.7% | 32 | 17,324 |
| Region.c | Game/Region.c | 31 | 0 | 45 | 68.9% | 30 | 5,265 |
| UIControls.c | Game/UIControls.c | 31 | 3 | 85 | 36.5% | 31 | 8,276 |
| KAS.c | Game/KAS.c | 30 | 0 | 44 | 68.2% | 30 | 4,426 |
| MultiplayerLANGame.c | Game/MultiplayerLANGame.c | 28 | 0 | 77 | 36.4% | 27 | 7,697 |
| NIS.c | Game/NIS.c | 28 | 0 | 159 | 17.6% | 28 | 17,509 |
| glcompat.c | (not in HomeworldSDL) | 28 | 0 | - | - | 0 | 12,438 |
| render.c | SDL/render.c | 28 | 0 | 60 | 46.7% | 23 | 12,685 |
| FEReg.c | Game/FEReg.c | 26 | 0 | 35 | 74.3% | 26 | 12,304 |
| ETG.c | Game/ETG.c | 25 | 4 | 243 | 10.3% | 25 | 8,575 |
| utility.c | SDL/utility.c | 25 | 0 | 98 | 25.5% | 24 | 11,791 |
| Particle.c | Game/Particle.c | 24 | 0 | 175 | 13.7% | 24 | 7,779 |
| soundlow.c | SDL/soundlow.c | 24 | 0 | 31 | 77.4% | 24 | 4,628 |
| texreg.c | SDL/texreg.c | 24 | 1 | 76 | 31.6% | 22 | 12,191 |
| Collision.c | Game/Collision.c | 23 | 0 | 26 | 88.5% | 23 | 14,582 |
| CommandWrap.c | Game/CommandWrap.c | 23 | 5 | 24 | 95.8% | 23 | 3,522 |
| Universe.c | Game/Universe.c | 23 | 2 | 93 | 24.7% | 23 | 9,283 |
| TradeMgr.c | Game/TradeMgr.c | 22 | 0 | 41 | 53.7% | 22 | 1,919 |
| StatScript.c | Game/StatScript.c | 21 | 3 | 61 | 34.4% | 21 | 8,471 |
| font.c | SDL/font.c | 21 | 0 | 29 | 72.4% | 21 | 5,742 |
| AITrack.c | Game/AITrack.c | 19 | 0 | 24 | 79.2% | 19 | 6,816 |
| File.c | Game/File.c | 19 | 0 | 30 | 63.3% | 18 | 6,480 |
| Formation.c | Game/Formation.c | 19 | 0 | 49 | 38.8% | 19 | 11,948 |
| Sensors.c | Game/Sensors.c | 19 | 0 | 56 | 33.9% | 19 | 8,273 |
| SpeechEvent.c | Game/SpeechEvent.c | 19 | 1 | 33 | 57.6% | 19 | 7,677 |
| prim2d.c | SDL/prim2d.c | 19 | 0 | 36 | 52.8% | 18 | 4,741 |
| mainrgn.c | SDL/mainrgn.c | 18 | 0 | 93 | 19.4% | 18 | 2,440 |
| NetCheck.c | Game/NetCheck.c | 17 | 0 | 24 | 70.8% | 17 | 2,398 |
| main.c | SDL/main.c | 17 | 1 | 69 | 24.6% | 13 | 7,163 |
| AIMoves.c | Game/AIMoves.c | 16 | 0 | 270 | 5.9% | 16 | 2,593 |
| Blobs.c | Game/Blobs.c | 16 | 0 | 31 | 51.6% | 16 | 7,317 |
| ColPick.c | Game/ColPick.c | 16 | 0 | 46 | 34.8% | 16 | 3,273 |
| ResearchGUI.c | Game/ResearchGUI.c | 16 | 0 | 47 | 34.0% | 16 | 6,318 |
| Task.c | Game/Task.c | 15 | 0 | 16 | 93.8% | 13 | 2,708 |
| AITeam.c | Game/AITeam.c | 14 | 0 | 80 | 17.5% | 14 | 2,514 |
| Memory.c | Game/Memory.c | 14 | 0 | 35 | 40.0% | 14 | 4,243 |
| Subtitle.c | Game/Subtitle.c | 14 | 0 | 26 | 53.8% | 14 | 4,926 |
| HorseRace.c | Game/HorseRace.c | 13 | 0 | 29 | 44.8% | 13 | 6,463 |
| LinkedList.c | Game/LinkedList.c | 13 | 0 | 19 | 68.4% | 13 | 1,351 |
| Objectives.c | Game/Objectives.c | 13 | 0 | 24 | 54.2% | 13 | 2,400 |
| PiePlate.c | Game/PiePlate.c | 13 | 0 | 20 | 65.0% | 13 | 6,755 |
| SaveGame.c | Game/SaveGame.c | 13 | 1 | 180 | 7.2% | 13 | 6,809 |
| SinglePlayer.c | Game/SinglePlayer.c | 13 | 1 | 107 | 12.1% | 13 | 8,915 |
| sstream.c | SDL/sstream.c | 13 | 0 | 17 | 76.5% | 13 | 3,294 |
| AIPlayer.c | Game/AIPlayer.c | 12 | 0 | 36 | 33.3% | 12 | 3,398 |
| Key.c | Game/Key.c | 12 | 0 | 17 | 70.6% | 12 | 920 |
| LaunchMgr.c | Game/LaunchMgr.c | 12 | 0 | 38 | 31.6% | 12 | 4,074 |
| Vector.c | Game/Vector.c | 12 | 0 | 15 | 80.0% | 12 | 1,056 |
| AIUtilities.c | Game/AIUtilities.c | 11 | 0 | 115 | 9.6% | 11 | 2,082 |
| AutoDownloadMap.c | Game/AutoDownloadMap.c | 11 | 0 | 19 | 57.9% | 11 | 2,109 |
| Matrix.c | Game/Matrix.c | 11 | 0 | 25 | 44.0% | 11 | 1,397 |
| ResCollect.c | Game/ResCollect.c | 11 | 0 | 26 | 42.3% | 11 | 2,522 |
| Timer.c | Game/Timer.c | 11 | 0 | 17 | 64.7% | 11 | 2,010 |
| prim3d.c | SDL/prim3d.c | 11 | 0 | 18 | 61.1% | 11 | 3,328 |
| Captaincy.c | Game/Captaincy.c | 10 | 0 | 30 | 33.3% | 10 | 3,364 |
| GameChat.c | Game/GameChat.c | 10 | 0 | 18 | 55.6% | 10 | 1,782 |
| ResearchAPI.c | Game/ResearchAPI.c | 10 | 2 | 32 | 31.2% | 10 | 1,699 |
| AIShip.c | Game/AIShip.c | 9 | 0 | 29 | 31.0% | 9 | 8,082 |
| ScenPick.c | Game/ScenPick.c | 9 | 0 | 20 | 45.0% | 9 | 3,776 |
| Stats.c | Game/Stats.c | 9 | 0 | 61 | 14.8% | 9 | 2,334 |
| BigFile.c | Game/BigFile.c | 8 | 0 | 35 | 22.9% | 5 | 3,088 |
| Demo.c | Game/Demo.c | 8 | 0 | 10 | 80.0% | 8 | 1,625 |
| GamePick.c | Game/GamePick.c | 8 | 0 | 31 | 25.8% | 8 | 1,564 |
| GenericInterceptor.c | Ships/GenericInterceptor.c | 8 | 0 | 17 | 47.1% | 8 | 5,921 |
| Light.c | Game/Light.c | 8 | 0 | 15 | 53.3% | 8 | 1,792 |
| Queue.c | SDL/Queue.c | 8 | 0 | 8 | 100.0% | 8 | 446 |
| glcaps.c | (not in HomeworldSDL) | 8 | 0 | - | - | 0 | 1,472 |
| smixer.c | SDL/smixer.c | 8 | 0 | 9 | 88.9% | 5 | 3,779 |
| AIFleetMan.c | Game/AIFleetMan.c | 7 | 0 | 21 | 33.3% | 7 | 2,855 |
| AIResourceMan.c | Game/AIResourceMan.c | 7 | 0 | 22 | 31.8% | 7 | 2,970 |
| Physics.c | Game/Physics.c | 7 | 0 | 8 | 87.5% | 7 | 5,646 |
| Trails.c | Game/Trails.c | 7 | 2 | 46 | 15.2% | 7 | 1,933 |
| FontReg.c | Game/FontReg.c | 6 | 0 | 6 | 100.0% | 6 | 935 |
| Gun.c | Game/Gun.c | 6 | 0 | 20 | 30.0% | 6 | 6,306 |
| Scroller.c | Game/Scroller.c | 6 | 0 | 7 | 85.7% | 6 | 530 |
| Shader.c | Game/Shader.c | 6 | 0 | 32 | 18.8% | 6 | 936 |
| ShipSelect.c | Game/ShipSelect.c | 6 | 0 | 68 | 8.8% | 6 | 500 |
| debugwnd.c | SDL/debugwnd.c | 6 | 0 | 16 | 37.5% | 6 | 2,442 |
| fqeffect.c | SDL/fqeffect.c | 6 | 1 | 20 | 30.0% | 6 | 557 |
| Dock.c | Game/Dock.c | 5 | 0 | 113 | 4.4% | 5 | 2,329 |
| Ping.c | Game/Ping.c | 5 | 0 | 25 | 20.0% | 4 | 542 |
| Select.c | Game/Select.c | 5 | 0 | 39 | 12.8% | 5 | 434 |
| TaskBar.c | Game/TaskBar.c | 5 | 0 | 49 | 10.2% | 5 | 1,330 |
| ddraw.cpp | (not in HomeworldSDL) | 5 | 0 | - | - | 0 | 660 |
| rinit.cpp | (not in HomeworldSDL) | 5 | 0 | - | - | 0 | 2,058 |
| AIAttackMan.c | Game/AIAttackMan.c | 4 | 0 | 23 | 17.4% | 4 | 2,548 |
| AIDefenseMan.c | Game/AIDefenseMan.c | 4 | 0 | 23 | 17.4% | 4 | 913 |
| AIEvents.c | Game/AIEvents.c | 4 | 0 | 32 | 12.5% | 4 | 1,650 |
| BitIO.c | ThirdParty/LZSS/BitIO.c | 4 | 0 | 13 | 30.8% | 2 | 226 |
| HS.c | Game/HS.c | 4 | 0 | 28 | 14.3% | 4 | 1,212 |
| LagPrint.c | Game/LagPrint.c | 4 | 0 | 4 | 100.0% | 4 | 263 |
| Mesh.c | Game/Mesh.c | 4 | 0 | 47 | 8.5% | 4 | 1,283 |
| Mothership.c | Ships/Mothership.c | 4 | 0 | 18 | 22.2% | 4 | 542 |
| Nebulae.c | Game/Nebulae.c | 4 | 0 | 71 | 5.6% | 4 | 1,030 |
| ObjTypes.c | Game/ObjTypes.c | 4 | 1 | 46 | 8.7% | 4 | 203 |
| Randy.c | Game/Randy.c | 4 | 1 | 12 | 33.3% | 4 | 518 |
| ShipView.c | Game/ShipView.c | 4 | 0 | 15 | 26.7% | 4 | 2,421 |
| StringSupport.c | Game/StringSupport.c | 4 | 0 | 6 | 66.7% | 4 | 778 |
| Tactics.c | Game/Tactics.c | 4 | 0 | 37 | 10.8% | 4 | 368 |
| TimeoutTimer.c | SDL/TimeoutTimer.c | 4 | 0 | 8 | 50.0% | 4 | 233 |
| Titan.c | SDL/Titan.c | 4 | 0 | 3 | 133.3% | 2 | 327 |
| Animatic.c | Game/Animatic.c | 3 | 0 | 17 | 17.6% | 3 | 725 |
| Attack.c | Game/Attack.c | 3 | 0 | 7 | 42.9% | 3 | 1,154 |
| Carrier.c | Ships/Carrier.c | 3 | 0 | 10 | 30.0% | 3 | 192 |
| Clamp.c | Game/Clamp.c | 3 | 0 | 3 | 100.0% | 3 | 898 |
| Clouds.c | Game/Clouds.c | 3 | 0 | 43 | 7.0% | 3 | 1,023 |
| Crates.c | Game/Crates.c | 3 | 0 | 14 | 21.4% | 3 | 1,109 |
| LevelLoad.c | Game/LevelLoad.c | 3 | 0 | 47 | 6.4% | 3 | 3,202 |
| Tactical.c | Game/Tactical.c | 3 | 0 | 7 | 42.9% | 3 | 1,613 |
| fqcodec.c | SDL/fqcodec.c | 3 | 0 | 5 | 60.0% | 3 | 1,372 |
| fquant.c | SDL/fquant.c | 3 | 0 | 4 | 75.0% | 3 | 1,270 |
| mouse.c | SDL/mouse.c | 3 | 0 | 29 | 10.3% | 3 | 3,474 |
| rglu.c | SDL/rglu.c | 3 | 0 | 10 | 30.0% | 3 | 976 |
| AIHandler.c | Game/AIHandler.c | 2 | 0 | 25 | 8.0% | 2 | 83 |
| AutoLOD.c | Game/AutoLOD.c | 2 | 0 | 15 | 13.3% | 2 | 157 |
| CRC32.c | ThirdParty/CRC/CRC32.c | 2 | 0 | 2 | 100.0% | 2 | 104 |
| CameraCommand.c | Game/CameraCommand.c | 2 | 0 | 40 | 5.0% | 2 | 224 |
| Debug.c | Game/Debug.c | 2 | 0 | 9 | 22.2% | 2 | 183 |
| DefaultShip.c | Ships/DefaultShip.c | 2 | 0 | 6 | 33.3% | 2 | 287 |
| Globals.c | Game/Globals.c | 2 | 0 | 2 | 100.0% | 1 | 334 |
| KeyBindings.c | Game/KeyBindings.c | 2 | 0 | 14 | 14.3% | 2 | 463 |
| LOD.c | Game/LOD.c | 2 | 1 | 11 | 18.2% | 2 | 865 |
| MEX.c | Game/MEX.c | 2 | 0 | 17 | 11.8% | 2 | 496 |
| MadLinkIn.c | Game/MadLinkIn.c | 2 | 0 | 16 | 12.5% | 2 | 820 |
| MinelayerCorvette.c | Ships/MinelayerCorvette.c | 2 | 0 | 14 | 14.3% | 2 | 473 |
| SoundEventStop.c | Game/SoundEventStop.c | 2 | 0 | 2 | 100.0% | 2 | 136 |
| Transformer.c | Game/Transformer.c | 2 | 0 | 1 | 200.0% | 0 | 374 |
| Undo.c | Game/Undo.c | 2 | 0 | 4 | 50.0% | 2 | 62 |
| rinit.c | SDL/rinit.c | 2 | 2 | 14 | 14.3% | 2 | 653 |
| AIVar.c | Game/AIVar.c | 1 | 0 | 18 | 5.6% | 1 | 193 |
| Alliance.c | Game/Alliance.c | 1 | 1 | 10 | 10.0% | 1 | 160 |
| Battle.c | Game/Battle.c | 1 | 0 | 26 | 3.8% | 1 | 21 |
| Bounties.c | Game/Bounties.c | 1 | 0 | 6 | 16.7% | 1 | 190 |
| Damage.c | Game/Damage.c | 1 | 0 | 15 | 6.7% | 1 | 412 |
| GenericDefender.c | Ships/GenericDefender.c | 1 | 0 | 4 | 25.0% | 1 | 1,025 |
| GravWellGenerator.c | Ships/GravWellGenerator.c | 1 | 0 | 21 | 4.8% | 1 | 275 |
| InfoOverlay.c | Game/InfoOverlay.c | 1 | 0 | 10 | 10.0% | 1 | 220 |
| P1MissileCorvette.c | Ships/P1MissileCorvette.c | 1 | 0 | 6 | 16.7% | 1 | 121 |
| SoundEventPlay.c | Game/SoundEventPlay.c | 1 | 0 | 4 | 25.0% | 1 | 3,032 |
| Teams.c | Game/Teams.c | 1 | 0 | 8 | 12.5% | 1 | 142 |
| fastmath.c | (not in HomeworldSDL) | 1 | 0 | - | - | 0 | 142 |
| screenshot.c | SDL/screenshot.c | 1 | 0 | 3 | 33.3% | 0 | 710 |

`glcompat.c`, `glcaps.c`, `ddraw.cpp` and `fastmath.c` are Relic Win32 files that HomeworldSDL dropped; `rinit.cpp` exists only in CET's `game/Game/rinit.cpp` (HomeworldSDL has `SDL/rinit.c`). `Titan.c` (4 > 3) and `Transformer.c` (2 > 1) have more matches than SDL definitions because the matched names (`titanShutdown`, `titanPumpEngine`, `transStartup`, `transShutdown`) are from Relic's original, not from the SDL port.

**Largest HW1 gaps.** These files have many definitions and few or no matches. Files that are Mac-, SDL- or stub-only (`utility_mac.c`, `File_Mac.c`, `TitanInterfaceC.stub.c`, `avi.c`) are expected to be absent.

| HW1 file | defs (approx.) | matched | comment |
|---|---:|---:|---|
| Game/AIMoves.c | 270 | 16 |  |
| Game/ETG.c | 243 | 25 | guess: ETG opcode handlers are reached through a dispatch table, so call-graph growth misses them |
| Game/MultiplayerGame.c | 233 | 78 | defs inflated by #if branches |
| Game/SaveGame.c | 180 | 13 | save/load per type |
| Game/Particle.c | 175 | 24 |  |
| Game/NIS.c | 159 | 28 |  |
| Game/Options.c | 154 | 37 |  |
| Game/CommandLayer.c | 144 | 59 |  |
| Game/Dock.c | 113 | 5 | dock callbacks reached through ship headers / function tables |
| Game/SinglePlayer.c | 107 | 13 |  |
| Game/AIUtilities.c | 115 | 11 |  |
| Game/Universe.c | 93 | 23 |  |
| SDL/mainrgn.c | 93 | 18 |  |
| Game/FlightMan.c | 63 | 0 | no match at all |
| Ships/SalCapCorvette.c | 46 | 0 | Cataclysm reworked salvage |
| Game/AIOrders.c | 30 | 0 | no match at all |
| Game/BTG.c | 26 | 0 | `game\btg.c` source-path string is in the exe; candidate 0x00429280 (section 8) |
| Game/MeshAnim.c | 21 | 0 |  |
| Game/Eval.c | 19 | 0 | `game\eval.c` assert path is in the exe |
| Game/Camera.c | 17 | 0 |  |
| Game/ChannelFSM.c | 16 | 0 |  |
| Ships/*.c (40 modules together) | 335 | 22 | most ship-specific code sits behind shipHeaders slots that are not Ghidra function entries (section 7) |

### 5.1 CET anchors inside now-named functions

CET's 50 patch sites were placed in their containing functions at import (old `cet_apply_report.csv`). Each row compares the container's round-1 name with what CET implies.

| patch site(s) | container | name now | source of the name | agrees with CET? |
|---|---|---|---|---|
| 0x00563563, 0x00563645 `InitWindow_DeviceCrcCheck*` | 0x005631f0 | `WinMain` (hw1:main.c) | core + hw1-match + lib-graph | yes: CET's `InitWindow` is HW1 InitWindow inlined into WinMain |
| 0x005abf68, 0x005ac174 `utyGameSystemsInit_*` | 0x005abe70 | `utyGameSystemsInit` (hw1:utility.c) | round 1 (core) | **confirmed, same name** |
| 0x0057b78b, 0x0057b7a1 `rndRenderTask_FrameLimiter1/2` | 0x0057b760 | `rndFlush` (hw1:render.c, high) | sim-presentation | refines CET: the frame limiter (glFlush+SwapBuffers+Sleep) lives in rndFlush, which rndRenderTask calls |
| 0x00524841 `univUpdate1` | 0x00524790 | `statsShowFancyFight` (hw1:Stats.c) | core | consistent: CET patched the `univUpdate(1/16)` loop inside it |
| 0x005256b7 `univUpdate2` | 0x00525640 | `statsGatherFightStats` (hw1:Stats.c) | core | consistent, same pattern |
| 0x00589115 `texreg_unknown_push0_site` | 0x005890c0 | `trNoPalTextureCreate` (hw1:texreg.c, medium) | sim-presentation | consistent with CET's file attribution (texreg) |
| 0x0050a43b `SensorHack_site` | 0x0050a310 | `cat_smShipsDraw` (cat-new) | sim-presentation | consistent (sensors-manager ship drawing) |
| 0x00543f48, 0x00543f51 `aiUpdateExternalShipBuild_*` | 0x00543f10 | `cat_shipUpdateExternalBuildAndAI` (cat-new) | sim-cataclysm | consistent; Cataclysm-only |
| 7 sites in univUpdate 0x0054c3f0, univUpdateReset 0x0054acd0, allianceFormWith 0x00420880, trCramRAMComputeAndScale 0x00589a50 | CET functions | unchanged | CET | n/a |
| 21 sites in 10 containers | 0x0046de60 `etgAtDefine`, 0x0048cb60 `gunShipFirePower`, 0x004cc600 `nisUpdateTask`, 0x004cc8f0 `nisStartup`, 0x004dc6f0 `partRenderBillSystem`, 0x004f17a0 `CollectResources`, 0x0052af00 `taskCallBackInit`, 0x0053e460 `universeUpdateTask`, 0x005485e0 `CheckPlayerWin`, 0x005ab810 `utyGameSystemsPreInit` | CET_container names (ANALYSIS source) from an earlier run | **not re-checked**: no round-1 set proposed these addresses, so the names rest only on CET's hw1_equivalent |

Of the 56 CET functions, 4 appear in round 1, and the sets agree with CET on them: `singlePlayerGameUpdate` 0x005197d0 and `scriptSetTweakableGlobals` 0x00527820 (both via hw1-match). At `memAlloc` 0x006725d0 and `memFree` 0x00673080 the sets add only a class, and that class is crt: CET put HW1 names on the CRT `malloc`/`free` wrappers. The real HW1 Memory.c `memFree` is 0x004ae130, written as `memFree_2` (406 callers).

## 6. Mission code

Full map: [`docs/mission-code-map.md`](mission-code-map.md). Summary:

| item | address | count |
|---|---|---:|
| kas2c `FunctionCall` table (`cat_kasFunctionTable`) | 0x00755048-0x00878137, 0x990-byte entries | 487 (486 + `*`) |
| hash dispatcher `cat_kasCallFunctionByName` / dev-console `KAS` command | 0x0045b9a0 / 0x0045f270 | 486 branches |
| 28-bit case-folded name hash `cat_kasHashString` | 0x00491860 | |
| KAS runtime (KAS.c + HS.c + Cataclysm helpers) | 0x004901b0-0x00491c60 | 30 + 3 + 10 |
| Timer.c callees | 0x0052bfe0-0x0052c860 | 11 |
| kasf library (KASFunc.c + Cataclysm kasf) | 0x00491c70-0x0049ad80 | 291 HW1 + 184 cat-new named |
| generated mission Init/Watch functions (17 missions + Tutorial1) | 0x005d6fe0-0x00670a10 (629,296 bytes) | 2,085 (322 FSMs, 1,256 states); **no Ghidra functions yet** |
| KAS entry points | `singlePlayerGameUpdate` 0x005197d0 (mission01-17), `tutInitTutorial` 0x0052f690 (tutorial1) | 18 |

Applied in round 1: 537 mission-set functions (291 hw1:KASFunc.c, 198 cat-new, 30 hw1:KAS.c, 11 hw1:Timer.c, 3 hw1:HS.c, others) and the data label `cat_kasFunctionTable`. 290 of the 291 HW1 kasf addresses ascend in KASFunc.c source order. All KAS identifiers in mission code are hashes; names were recovered by hashing candidate words, and 28 FSM and 23 state hashes are still unresolved.

## 7. Ship tables

| table | address | layout | finding |
|---|---|---|---|
| `shiptypeinfo` | 0x00889530 | 145 x 8-byte `{type, char *name}`, NULL row at 0x008899b8 | sRecon (0) .. nisLensflareSource1 (144); read by CET `ShipTypeToStr` 0x004d6bc0 |
| `FirstShipTypeOfRace` / `LastShipTypeOfRace` | 0x008894e8 / 0x00889504 | 7 x udword each | Sect 0-32, Beast 33-59, Kushan 60-73, Taiidan 74-91, P1 92-97, Traders 98-108, Derelict 109-144 |
| `shipHeaders` | 0x008b1938 | `CustShipHeader *[145]` by ShipType | 51 distinct headers in 0x008c3260-0x008c6b50 |
| `CustShipHeader` (Cataclysm) | | 0x8c bytes = `sizeofShipSpecifics` + 34 function slots, **no leading shiptype field** (unlike HW1) | copied as 0x23 dwords to ShipStaticInfo+0x108 by `InitStatShipInfo` 0x0053bab0; slots read at staticinfo +0x10c..+0x190 |
| `ShipStaticInfos` | 0x00b3b420 | 145 x 0x458 | `GetShipStaticInfo` 0x0053ea50 |
| `ShipNamesScriptTable` | 0x008935a0 | | Cataclysm-only `ShipNames.c` |

Headers with the most custom slots are Mothership (20), ACV (19), Carrier, HiveFrigate and RammingFrigate (17 each), and HeavyCruiser (16). DoNothingShipHeader has 0, DefaultShipHeader 3, MiningBase and StationaryGun 2 each, RepairDroid 1. Many slot sets are exact matches to HW1 initializers (GravWellGenerator, MissileDestroyer, GenericInterceptor, AttackBomber, CloakedFighter, ProximitySensor, IonCannonFrigate). **Only 72 of the 324 distinct slot targets are Ghidra function entries.** The other 252 were not proposed, and the 57 header data names were dropped by rule 5, so neither the headers nor most ship code are named in Ghidra yet. Applied in round 1: 101 ships functions (46 hw1, 54 cat-new).

## 8. Next-round targets: the 60 largest unnamed functions

All are in game code (0x401000-0x5c9fff). `strings` = first 3 strings referenced (from strings.csv xrefs). `named callers` / `named callees` = up to 3 neighbours that already have a real name, which usually place the function in its HW1 file.

| # | address | size | calls in | strings | named callers | named callees |
|---:|---|---:|---:|---|---|---|
| 1 | 0x004e0640 | 6,718 | 1 | - | partCreationHelper | matMultiplyMatByVec, memAllocFunction, ranRandomFn |
| 2 | 0x004b1d40 | 6,038 | 2 | - | - | rndLightingEnable |
| 3 | 0x0042fd50 | 4,691 | 1 | - | cat_aisPlay | GetShipStaticInfo, cat_bigGunIsCharged, cat_mothershipBigGunActive |
| 4 | 0x00436aa0 | 3,418 | 1 | - | cloudStartup | memAllocFunction, ranRandomFn |
| 5 | 0x00465860 | 3,395 | 2 | - | processDockToDo, processLaunchShipToDo | RangeToTarget, aishipFlyToPointAvoidingObjsFunc, aishipGetTrajectory |
| 6 | 0x004a5e40 | 3,240 | 1 | `(%c) %s %s`; `AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA` | - | _sprintf, fontHeight, fontMakeCurrent |
| 7 | 0x00424110 | 3,177 | 0 | - | - | gunShipFirePower, ranRandomFn, selShipInSelection |
| 8 | 0x005312b0 | 3,147 | 1 | - | rndRenderTask | __ftol, pieCircleSegmentsCompute, primLine2 |
| 9 | 0x00568100 | 3,141 | 1 | - | - | primCircleOutline3, vecNormalize |
| 10 | 0x0057bec0 | 3,049 | 1 | - | rndMainViewRenderFunction | __ftol, primModeClearFunction2, primModeSetFunction2 |
| 11 | 0x00566b60 | 2,983 | 1 | `ClickAttack`; `Game_BandBoxAttack`; `AltFocus` | - | MakeShipsAttackCapable, MakeShipsSpecialTargetCapable, __chkstk |
| 12 | 0x00506720 | 2,968 | 4 | - | cat_smMinesDraw, cat_smResourcesDraw, cat_smShipsDraw | primPointLineIntersection |
| 13 | 0x004b34f0 | 2,964 | 2 | - | partRenderMeshSystem | rndLightingEnable |
| 14 | 0x0050fd60 | 2,923 | 1 | `%.0f`; `%s %d:` | - | __ftol, _sprintf, cat_shipGetDamageMultiplier |
| 15 | 0x005b6f30 | 2,911 | 0 | - | - | ApplyDamageToTarget, MoveReachedDestinationVariable, aishipFlyToPointAvoidingObjsFunc |
| 16 | 0x00467b80 | 2,559 | 1 | - | clProcess | __chkstk, aishipFlyToPointAvoidingObjsFunc, aitrackHeadingAndUp |
| 17 | 0x005c32f0 | 2,548 | 1 | `333?`; `droidUpdateArray`; `\ndroidGetNextDock: %s has no DPT_Repair` | - | ShipTypeToStr, droidIsValidPredockPos, etgEffectCreate |
| 18 | 0x00468650 | 2,346 | 1 | - | clProcess | __chkstk, aitrackHeadingAndUp, cat_shipUpdateExperienceLevel |
| 19 | 0x0045a700 | 2,340 | 0 | - | - | cat_consoleExecuteCommand, cat_consoleOpen, memFree_2 |
| 20 | 0x00460ed0 | 2,334 | 1 | `\nGot %d RU's from crate.`; `\nGot a %d ship type  from crate!` | cratesPlayerFoundACrate | GetShipStaticInfo, clFormation, gameStatsAddShip |
| 21 | 0x005053a0 | 2,312 | 3 | `\nWarning: Tried to select too many ship` | - | nullsubReturnFalse, primRealRectUnion2, sub_53F9F0 |
| 22 | 0x004eb940 | 2,267 | 2 | - | - | __chkstk, cat_shipIsUpgradable, getShipAndItsCommand |
| 23 | 0x004c9430 | 2,191 | 1 | - | - | - |
| 24 | 0x00439130 | 2,161 | 1 | - | cloudSetFog | ranRandomFn, vecNormalize |
| 25 | 0x0056b890 | 2,147 | 2 | - | - | - |
| 26 | 0x004045b0 | 2,132 | 1 | - | aifProcessShipBuildRequests | GetShipStaticInfo, cat_playerHasCompletedShip |
| 27 | 0x004a91c0 | 2,098 | 1 | `%s (%d)`; `%s (%d/%d/%d/%d)`; `%s (%d/%d/%d)` | - | __chkstk, __ftol, _sprintf |
| 28 | 0x004ab840 | 2,062 | 1 | `hyperspace`; `ShipListClick` | - | mrResearch, poPopupFleetIntelligence, smCullAndFocusSelecting |
| 29 | 0x004b0170 | 2,024 | 5 | - | partRenderMeshSystem, rndRenderAHomeworld | glCapFeatureExists, rndAdditiveBlends, rndLightingEnable |
| 30 | 0x004896b0 | 1,938 | 0 | `%d. %s` | - | __ftol, _sprintf, cat_overlayIconDraw |
| 31 | 0x004c8b00 | 1,923 | 1 | - | - | - |
| 32 | 0x0056cc70 | 1,881 | 1 | - | - | - |
| 33 | 0x004aa4f0 | 1,841 | 1 | `\nUnrecognized MAINPRINT_XXX toggle key `; `\nGetCurrentPrintOptions()->bResearch tu`; `\nGetCurrentPrintOptions()->bConstructio` | - | nullsubReturnFalse |
| 34 | 0x005a7310 | 1,835 | 1 | - | rndMainViewRenderFunction | cat_shipGetSpeedMultiplier, glCapFastFeature, matMultiplyMatByVec |
| 35 | 0x004b1610 | 1,809 | 2 | - | - | rndLightingEnable |
| 36 | 0x005c60a0 | 1,806 | 2 | - | - | primCircleSolid3, primSolidTexture3, ranRandomFn |
| 37 | 0x00598c80 | 1,795 | 1 | `TitanFactoryServer`; `TitanRoutingServer`; `%d.%d.%d.%d:%d` | TitanInterface_HandleDirMultiEntityReply | GetCurrentChannel, TitanInterface_PingFactServer, WONCommon__WStringToString |
| 38 | 0x0040ff90 | 1,745 | 1 | - | - | aitCreate |
| 39 | 0x00481ef0 | 1,743 | 1 | `\nWarning: _SetFormationOffsetsForForceF` | FormationCalculateOffsets | nullsubReturnFalse |
| 40 | 0x004a6ff0 | 1,723 | 1 | `1 x %s`; `%i x %s` | - | _sprintf, fontHeight, fontMakeCurrent |
| 41 | 0x00560ba0 | 1,719 | 2 | `3dfxvgl.dll`; `opengl32.dll`; `Fatal Error` | opOptionsAcceptHelper, opTimerExpired | EXTERNAL:KERNEL32.DLL::FreeLibrary, EXTERNAL:KERNEL32.DLL::GetModuleHandleA, EXTERNAL:USER32.DLL::MessageBoxA |
| 42 | 0x004ee5f0 | 1,693 | 2 | - | - | allianceFormWith, gcRUTransferStart, smSensorsClose |
| 43 | 0x004c3490 | 1,687 | 1 | `atrox `; `Matrox` | cat_mgAdBannerDraw | __ftol, _strstr, memAlloc |
| 44 | 0x00434c10 | 1,684 | 1 | - | - | - |
| 45 | 0x004b5c90 | 1,675 | 2 | - | cat_rndRenderObjectList | rndLightingEnable |
| 46 | 0x00505ed0 | 1,660 | 3 | - | - | primPointLineIntersection, sub_53F9F0 |
| 47 | 0x0056ec50 | 1,653 | 2 | - | glcPageFlip, rndRenderTask | primModeClearFunction2, primModeSetFunction2, primTriOutline2 |
| 48 | 0x004a9a00 | 1,651 | 1 | - | - | __ftol, _sprintf, cat_overlayIconDraw |
| 49 | 0x00510e60 | 1,649 | 1 | `%s: %s`; `%s   (%s)`; `   %.0f %%` | smViewportRender | _sprintf, cat_shipCanUpgradeArmour, cat_shipExperienceLevelName |
| 50 | 0x00429280 | 1,636 | 1 | `BTG\\default.btg` | rndBackgroundRender | __ftol, glCapFeatureExists, rgluOrtho2D |
| 51 | 0x004ca0d0 | 1,632 | 1 | - | - | memAllocFunction, ranRandomFn, vecNormalize |
| 52 | 0x0052f8b0 | 1,627 | 1 | - | LoadGame | regChildAlloc, regDrawFunctionSet, regRegionDelete |
| 53 | 0x004b0970 | 1,620 | 2 | - | - | glCapFeatureExists, rndLightingEnable |
| 54 | 0x00484310 | 1,594 | 1 | `333?` | - | FormationCalculateOffsets, __chkstk, matMultiplyMatByVec |
| 55 | 0x004b0fe0 | 1,563 | 2 | - | - | rndLightingEnable |
| 56 | 0x0057d2c0 | 1,563 | 1 | - | rndMainViewRenderFunction | primModeClearFunction2, primModeSetFunction2, trMakeCurrent |
| 57 | 0x005a5570 | 1,553 | 1 | - | - | cat_shipGetSpeedMultiplier, rndAdditiveBlends, rndLightingEnable |
| 58 | 0x004e8850 | 1,552 | 1 | - | cat_smViewportRenderScene | __ftol, fontCurrentGet, fontHeight |
| 59 | 0x004a79d0 | 1,550 | 1 | `dbgAssert()`; `%s (%s)`; `%s (%d)` | - | _sprintf, fontHeight, fontMakeCurrent |
| 60 | 0x0056a610 | 1,510 | 1 | `Game_BandBoxAttack`; `AltFocus`; `Game_BandBoxFocus` | - | MakeShipsAttackCapable, MakeShipsSpecialTargetCapable, clWrapAttack |

Remaining `FUN_*` by 64 KB band (count / bytes): 0x0040xxxx 113/27,207, 0x0041xxxx 133/35,650, 0x0042xxxx 119/37,600, 0x0043xxxx 78/32,019, 0x0044xxxx 58/17,320, 0x0045xxxx 29/9,807, 0x0046xxxx 70/29,051, 0x0047xxxx 30/13,379, 0x0048xxxx 52/21,793, 0x0049xxxx 31/5,566, 0x004axxxx 93/37,494, 0x004bxxxx 73/31,817, 0x004cxxxx 30/18,233, 0x004dxxxx 42/6,098, 0x004exxxx 73/34,538, 0x004fxxxx 91/17,215, 0x0050xxxx 106/36,936, 0x0051xxxx 157/28,224, 0x0052xxxx 61/14,197, 0x0053xxxx 64/18,632, 0x0054xxxx 38/7,339, 0x0055xxxx 57/10,971, 0x0056xxxx 78/37,046, 0x0057xxxx 58/20,147, 0x0058xxxx 57/15,531, 0x0059xxxx 55/12,527, 0x005axxxx 124/29,406, 0x005bxxxx 103/21,941, 0x005cxxxx 87/25,899.

Suggested order for round 2:

1. **Create functions** in 0x5d6fe0-0x670a0f from the 2,085 addresses in mission-code-map.md section 8, and at the 252 ship-header slot targets. Then apply the mission Init/Watch names and the header data names.
2. **Renderer/particle cluster** 0x004b0000-0x004b6fff and 0x004e0640: 7 large functions that call only `rndLightingEnable` (partRenderMeshSystem / rndRenderAHomeworld callers), probably HW1 `Particle.c`/`render.c` mesh and lighting paths.
3. **Command-layer / AI ship movement**: 0x00465860, 0x00467b80, 0x00468650 (callers processDockToDo/processLaunchShipToDo/clProcess); `Dock.c` and `AIShip.c` are under-matched.
4. **Input/region**: 0x00566b60 and 0x0056a610 reference the `Game_BandBoxAttack`/`ClickAttack`/`AltFocus` key-binding strings (HW1 `mainrgn.c`, 18 of 93 matched).
5. **Front-end/taskbar print functions** 0x004a5e40, 0x004a6ff0, 0x004a79d0, 0x004a91c0, 0x004a9a00, 0x004aa4f0 (sprintf + font calls, `MAINPRINT_XXX` strings).
6. Whole files with no match: FlightMan.c, AIOrders.c, BTG.c (0x00429280 references `BTG\default.btg`, caller rndBackgroundRender), Eval.c, Camera.c, MeshAnim.c, ChannelFSM.c.

## 9. Known weaknesses

1. **Class-only names are not names.** 4,579 functions (514,153 bytes) carry `lib_<class>_<addr>` or Ghidra funclet names. They are carved to a library but not identified, so they are counted as "named" only in the narrow not-`FUN_*` sense.
2. **Mission code is invisible to Ghidra.** 0x5d6fe0-0x670a0f (629 KB, 19% of .text) has no function entries, so decompiled.c, call_graph.csv and coverage do not include it. The 2,085 names exist only in mission-code-map.md.
3. **925 data names were dropped** by rule 5 (844 core, 57 ships, 24 sim-cataclysm), all in mapped .data/.rdata. symbols.csv lists only non-default symbols, so unnamed `DAT_` globals fail that rule. Only 34 data labels were applied.
4. **Ship code behind slots is unnamed.** 252 of 324 header slot targets are not function entries.
5. **Unconfirmed CET_container names.** 10 containers (21 patch sites) carry names derived only from CET hw1_equivalent fields at ANALYSIS source; no round-1 set re-checked them (section 5.1).
6. **Naming compromises.** `memFree_2` 0x004ae130 is the real HW1 memFree, because CET's `memFree` sits on CRT free 0x00673080. `utyGameQuitToMain_2` 0x005ab5b0 is a Cataclysm FE wrapper that still needs a `cat_` name. 91 names in all have `_N` suffixes. `HWEnqueue`/`HWDequeue` are HomeworldSDL spellings of Relic's `Enqueue`/`Dequeue`. 623 C++ names are flattened to `A__B`.
7. **Thread-start functions** 0x0067a0a0 / 0x0071cdc0 still show Ghidra's USER_DEFINED `lpStartAddress_*` names. `_threadstart` / `_threadstartex` are only secondary labels.
8. **Class enum gaps.** ImageMagick, IJG and the .cms_t stub are `class=unknown` with a `lib=` note. IJG might better be `hw1:<jfile.c>` because HW1 ships src/JPG. The SecuROM attribution rests only on the section name.
9. **Prototypes are weak.** 6,408 applied symbols have no prototype and 1,006 got degraded ones (`void *`/`undefined4` for HW1 types), so decompiled.c still uses generic types for most game structs. No HW1 struct header has been imported yet.
10. **Confidence mix.** 1,195 medium and 21 low entries were applied. Every set was audited with a 30-entry sample (overall 3 wrong in 270 sampled), but medium entries outside the samples are unverified.
11. **String matcher limits** (hw1-match): 89% name accuracy at score >= 0.8 where kept, and close to useless below that. Inlined helpers give the matcher the wrong name.
12. **`defs` counts in section 5 are approximate** (regex over HomeworldSDL, which is a port: Mac/SDL files are included, Relic Win32 files such as glcompat.c/glcaps.c/ddraw.cpp are missing, and #if branches are counted twice).
13. **qwen-naming.json** (local-model naming) is unaudited and was left out entirely.
14. 3 functions failed to decompile (`lib_crt_00671da2` and two .data artefacts), and 42 one-byte .data "functions" remain as `FUN_*`.

## Round 2

Generated 2026-10-09 from the re-export `ghidra/exports/cataclysm_gog_r2/`, made after `ghidra/symbols_round2.json` (962 symbols) was applied to project `ghidra/projects/cataclysm_gog`. Round 2 had three Ghidra passes on the same project:
- **A1** created the missing functions (export `cataclysm_gog_r2a`).
- **A2** added data names and struct types (export `cataclysm_gog_r2b`, written from the A2 worktree; see `docs/ghidra-round2-prep.md`).
- **Apply** is this pass.

The numbers below come from `tools/binary_map_coverage.py`, which computes tiers, regions, classes and per-file matches the way sections 2, 5 and 8 did. Run on the round-1 export, it reproduces section 2 exactly: 3,867 / 5,125 / 2,202 functions, 55.3% of the game region. Section 5 comes out within 2 rows (2,261 matched where it said 2,259). The `defs` counts move slightly because the regex now also counts C++ `Class::method` definitions.

### R2.1 What round 2 did

| pass | what | result |
|---|---|---|
| A1 create functions (`CreateFunctions.py`, `ghidra/create_functions_r2.json`, 3,258 entries) | mission Init/Watch entries from mission-code-map.md, ship-header slot targets, `.data`/`.rdata` pointer-table targets with a prologue | **3,190 functions created** (11,194 -> 14,384): 1,245 mission-watch, 840 mission-init, 252 ship-slot (the other 72 slot targets already existed), 848 pointer-table and 5 more found by analysing the new code. 2,312 entries were named on creation |
| A2 data + types (`symbols_round1_data.json`, `ImportHeaders.py`) | re-admitted the 925 data names that round-1 rule 5 dropped; parsed CET's reconstructed headers plus the HW1 `Memory.h`/`Switches.h`/`Chatting.h` | **925 of 925 data labels** applied. 1,639 declarations parsed with 0 failures, giving 1,735 types in `/Cataclysm`. All 298 of 298 layout asserts pass in Ghidra. 817 globals typed (811 applied + 6 already the same), including `universe`, `shipHeaders`, `taskData`, `aisTeams` and the 51 `CustShipHeader`s. `datatypes.h` grew from 1,485 to 6,688 lines |
| producers + audits | 6 sets in `ghidra/proposals/r2/`: missions-r2 2,085, gaps-render-fe 479, gaps-ai-flight 328, gaps-input-save-sp 205, libs-unknown 503, ships-r2 378 | each set had a 30-entry audit. Precision was 1.00 for missions-r2, ships-r2 and libs-unknown, and 0.967 for the three gaps sets (1 wrong each, dropped) |
| reconcile (`ghidra/proposals/r2/reconcile_report.md`) | only new names and renames on top of round 1 + A1 + A2 | 962 symbols (911 function, 51 data): 899 new names, 63 renames, 8 cross-set conflicts resolved, 105 deferred (address is not a function entry), 1 dropped, 0 suffixes. 2 CET anchors overridden by the libs-unknown compromise: 0x00673080 `memFree` -> `_free` and 0x006725d0 `memAlloc` -> `_malloc` (`cet_symbols.json` unchanged) |
| **apply** (this pass) | `tools/ghidra_headless.sh --symbols ghidra/symbols_round2.json --export-name cataclysm_gog_r2 original/game/cataclysm.exe cataclysm_gog` (reuse mode, `-noanalysis`, ImportSymbolsJson phase=all, ExportAnalysis with decompile), run detached with `setsid nohup` | 2026-10-09 14:02, rc=0, wall 52 s, no project-lock retry, no `ERROR`/Traceback lines. **962 of 962 applied**: 911 `function` (all `renamed`), 51 `label`. 0 `kept-user-name`, 0 `label-only`, 0 `inside:`, 0 `error`. Prototypes: 30 applied, 1 applied degraded, 880 had none. The 51 data labels had no type. Decompile: ok 14,295, thunk 86, failed 3 (the same 3 as round 1), error 0 |

**Apply failures: 0.** Every round-2 function name is now the primary name at its address. Spot checks: `memFree` 0x004ae130 (was `memFree_2`), `_free` 0x00673080, `_malloc` 0x006725d0, `_malloc_cms` 0x00c60751, `cat_utyGameQuitToMainOrStats` 0x005ab5b0. All 925 A2 data labels and all 51 round-2 data labels appear in `symbols.csv`.

The export has 11 duplicate non-`FUN_*` names. 3 are thunks, which Ghidra names after their target: `cat_kasVarDestroyAll`, `ioUpdateShipTotals` and `WONMisc__EasySocket__stopWinsock` each have a 5-byte DEFAULT-source thunk. The other 8 are `FID_conflict:`/std template names. None is a name collision introduced by round 2.

### R2.2 Before / after

| measure | baseline | round 1 | round 2 |
|---|---:|---:|---:|
| functions in Ghidra | 11,194 | 11,194 | **14,384** |
| function bytes | 2,194,228 | 2,194,228 | 2,679,918 |
| real-named functions | 379 | 3,867 | **7,055** |
| real-named bytes (% of function bytes) | 68,010 (3.1%) | 1,019,110 (46.4%) | **1,599,230 (59.7%)** |
| not `FUN_*` (real + class-only) functions / % bytes | 2,881 / 4.4% | 8,992 / 70.2% | **12,150 / 78.9%** |
| unnamed `FUN_*` functions / bytes | 8,313 / 2,097,320 | 2,202 / 653,625 | 2,234 / 565,795 |
| game region 0x401000-0x5c9fff: functions | 5,079 | 5,079 | 6,073 |
| game region: real-named % of function bytes | 2.8% | 55.3% | **65.7%** |
| game region: `FUN_*` functions / bytes | 4,893 / 1,448,014 | 2,160 / 653,583 | 2,081 / 549,585 |
| mission region 0x5d6fe0-0x670a0f: functions | 0 | 0 | **2,085** (all real-named) |
| mission region: real-named % of function bytes | - | - | **100%** |
| mission region: bytes inside a function body | 0 | 0 | 320,296 of 629,296 (**50.9%**); see R2.6 gap 1 |
| HW1 functions matched (`hw1:*` class + CET) | - | 2,261 | **3,053** in 180 files (verbatim 2,873) |
| HW1 files with 0 matches (files that define something) | - | 69 | 42 |
| data labels applied by the pipeline | 0 | 34 | 34 + 925 (A2) + 51 = **1,010** |
| struct types in the program | 0 | 0 | 1,735 (`/Cataclysm`), 817 globals typed |

The `unnamed` count went up by 32 even though 516 round-1 `FUN_*` functions were named: A1 created 548 functions that are still unnamed (543 pointer-table targets and 5 found by analysis; 437 of them in the game region, 111 in the WON/Magick region). Movement from round 1 to round 2:
- 516 `FUN_*` became real names.
- 30 class-only names became real names (IJG/ImageMagick identifications).
- 24 round-1 real names were replaced by reconcile renames. The other renames hit A1-created functions or round-1 class-only placeholders.
- 2,642 new functions got real names, and 548 new functions stayed `FUN_*`.
- No round-1 function disappeared.

The new `FUN_*` in the WON/Magick region (111 functions, 16,168 bytes) end round 1's statement that library carving was complete. These are pointer-table targets that A1 created inside the library ranges, so they have no class yet.

### R2.3 Per region, section and class

| region | round 1 functions | round 2 functions | real | class-only | `FUN_*` | `FUN_*` bytes | real % of function bytes, round 2 (round 1) |
|---|---:|---:|---:|---:|---:|---:|---|
| game C/C++ 0x401000-0x5ca0ff | 5,079 | 6,073 | 3,862 | 130 | 2,081 | 549,585 | 65.7% (55.3%) |
| IJG libjpeg 0x5ca100-0x5d6fdf | 102 | 102 | 41 | 61 | 0 | 0 | 23.8% (10.7%) |
| kas2c mission code 0x5d6fe0-0x670a0f | 0 | 2,085 | 2,085 | 0 | 0 | 0 | 100.0% (-) |
| WSOCK32 thunks + static CRT 0x670a10-0x69412f | 595 | 595 | 256 | 339 | 0 | 0 | 41.1% (41.1%) |
| WON/Titan + AdApi + Magick 0x694130-0x705c9f | 2,105 | 2,216 | 639 | 1,466 | 111 | 16,168 | 30.7% (31.2%) |
| Dinkumware C++ std lib 0x705ca0-0x71cccf | 708 | 708 | 124 | 584 | 0 | 0 | 9.3% (9.3%) |
| late CRT 0x71ccd0-0x71f62f | 48 | 48 | 28 | 20 | 0 | 0 | 58.3% (58.3%) |
| text$x EH funclets 0x71f630-0x729fff | 2,343 | 2,343 | 0 | 2,343 | 0 | 0 | 0.0% (0.0%) |
| .cms_t stub + CRT 0xc55000-0xc68fff | 172 | 172 | 20 | 152 | 0 | 0 | 5.1% (5.1%) |

| section | functions | bytes | real | class-only | `FUN_*` | `FUN_*` bytes |
|---|---:|---:|---:|---:|---:|---:|
| .text | 14,170 | 2,643,518 | 7,035 | 4,943 | 2,192 | 565,753 |
| .cms_t | 172 | 36,358 | 20 | 152 | 0 | 0 |
| .data | 42 | 42 | 0 | 0 | 42 | 42 |

Per class (round 1 -> round 2 functions: hw1:* 2,217 -> 3,011, cat-new 491 -> 2,864, `unknown, lib=ijg` 76 -> 61 and `unknown, lib=magick` 360 -> 346 as IJG and ImageMagick functions got real names (now classed `lib:libjpeg` 18 and `lib:imagemagick` 12); crt, won, bink, rgl and CET unchanged). The class of a function is taken from the latest symbol round, then the audited r2 sets, then CET. Confidence counts are those of the class source.

| class | functions | bytes | real | class-only | high | medium | low | lowest - highest entry |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| hw1:* (all files) | 3,011 | 847,084 | 3,011 | 0 | 2,515 | 486 | 10 | 0x004018c0 - 0x005c79b0 |
| cat-new | 2,864 | 523,415 | 2,864 | 0 | 1,750 | 1,039 | 75 | 0x00417ec0 - 0x006708a0 |
| won | 3,173 | 310,942 | 603 | 2,570 | 3,021 | 152 | 0 | 0x00694130 - 0x007276f1 |
| crt | 1,912 | 276,949 | 334 | 1,578 | 1,582 | 330 | 0 | 0x00553420 - 0x00c67ca0 |
| unknown, lib=magick | 346 | 78,087 | 15 | 331 | 276 | 70 | 0 | 0x006e3de0 - 0x00727bd0 |
| unknown, lib=ijg | 61 | 19,307 | 0 | 61 | 28 | 33 | 0 | 0x005ca410 - 0x005d6d10 |
| CET (hw1) | 42 | 14,724 | 42 | 0 | 0 | 0 | 0 | 0x00420880 - 0x005a7a70 |
| unknown, lib=cms | 12 | 7,621 | 0 | 12 | 0 | 12 | 0 | 0x00c55000 - 0x00c60540 |
| CET (other) | 10 | 6,862 | 10 | 0 | 0 | 0 | 0 | 0x00431510 - 0x005bc900 |
| lib:libjpeg | 18 | 4,328 | 18 | 0 | 15 | 0 | 3 | 0x005c9cd0 - 0x005cf9d0 |
| rgl | 3 | 3,586 | 3 | 0 | 1 | 2 | 0 | 0x00559a80 - 0x0055dd30 |
| lib:imagemagick | 12 | 3,270 | 12 | 0 | 2 | 10 | 0 | 0x006e41c0 - 0x006e6e00 |
| unknown | 29 | 2,887 | 28 | 0 | 23 | 6 | 0 | 0x0043453b - 0x006fe9a0 |
| bink | 8 | 2,503 | 8 | 0 | 6 | 2 | 0 | 0x00551220 - 0x00551d40 |
| other named | 650 | 12,723 | 107 | 543 | 0 | 0 | 0 | 0x00425ca0 - 0x00727750 |
| unnamed FUN_* | 2,233 | 565,630 | 0 | 0 | 0 | 0 | 0 | 0x00401000 - 0x00892114 |

### R2.4 Per HW1 source file

Same columns as section 5, plus the round-1 count; `defs` from the same rough regex, now also counting C++ `Class::method` definitions. Totals: 3,053 matched functions in 180 files (verbatim 2,873). These files define about 6,676 functions; HomeworldSDL `src/` has 218 .c/.cpp basenames with about 7,176 definitions.

| HW1 file | path under src/ | round 1 | round 2 | of which CET | defs (approx.) | matched / defs | verbatim | bytes |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| KASFunc.c | Game/KASFunc.c | 293 | 293 | 0 | 313 | 93.6% | 293 | 16,460 |
| ETG.c | Game/ETG.c | 25 | 167 | 4 | 244 | 68.4% | 167 | 26,998 |
| Particle.c | Game/Particle.c | 24 | 122 | 0 | 175 | 69.7% | 122 | 27,991 |
| AIMoves.c | Game/AIMoves.c | 16 | 120 | 0 | 270 | 44.4% | 120 | 10,796 |
| TitanInterface.cpp | SDL/TitanInterface.cpp | 112 | 112 | 0 | 165 | 67.9% | 23 | 56,453 |
| UnivUpdate.c | Game/UnivUpdate.c | 97 | 98 | 4 | 139 | 70.5% | 97 | 40,315 |
| MultiplayerGame.c | Game/MultiplayerGame.c | 78 | 78 | 3 | 233 | 33.5% | 77 | 20,984 |
| SaveGame.c | Game/SaveGame.c | 13 | 77 | 1 | 180 | 42.8% | 77 | 23,497 |
| CommandLayer.c | Game/CommandLayer.c | 59 | 68 | 0 | 144 | 47.2% | 67 | 35,949 |
| CommandNetwork.c | Game/CommandNetwork.c | 53 | 53 | 0 | 68 | 77.9% | 52 | 15,236 |
| Tutor.c | Game/Tutor.c | 42 | 44 | 0 | 80 | 55.0% | 44 | 9,283 |
| ConsMgr.c | Game/ConsMgr.c | 40 | 40 | 0 | 87 | 46.0% | 40 | 13,006 |
| FlightMan.c | Game/FlightMan.c | 0 | 39 | 0 | 63 | 61.9% | 39 | 12,355 |
| FEFlow.c | Game/FEFlow.c | 38 | 38 | 1 | 71 | 53.5% | 35 | 11,322 |
| Options.c | Game/Options.c | 37 | 37 | 1 | 154 | 24.0% | 37 | 8,648 |
| SinglePlayer.c | Game/SinglePlayer.c | 13 | 35 | 2 | 107 | 32.7% | 35 | 15,194 |
| TitanNet.c | Game/TitanNet.c | 35 | 35 | 0 | 39 | 89.7% | 35 | 3,507 |
| Mesh.c | Game/Mesh.c | 4 | 32 | 0 | 47 | 68.1% | 32 | 20,669 |
| SoundEvent.c | Game/SoundEvent.c | 32 | 32 | 0 | 51 | 62.7% | 32 | 17,324 |
| Region.c | Game/Region.c | 31 | 31 | 0 | 45 | 68.9% | 30 | 5,265 |
| UIControls.c | Game/UIControls.c | 31 | 31 | 3 | 84 | 36.9% | 31 | 8,276 |
| KAS.c | Game/KAS.c | 30 | 30 | 0 | 44 | 68.2% | 30 | 4,426 |
| NIS.c | Game/NIS.c | 28 | 30 | 0 | 159 | 18.9% | 29 | 18,292 |
| CameraCommand.c | Game/CameraCommand.c | 2 | 28 | 0 | 40 | 70.0% | 28 | 7,312 |
| MultiplayerLANGame.c | Game/MultiplayerLANGame.c | 28 | 28 | 0 | 77 | 36.4% | 27 | 7,697 |
| glcompat.c | (not in HomeworldSDL) | 28 | 28 | 0 | - | - | 0 | 12,438 |
| render.c | SDL/render.c | 28 | 28 | 0 | 60 | 46.7% | 23 | 12,685 |
| FEReg.c | Game/FEReg.c | 26 | 26 | 0 | 35 | 74.3% | 26 | 12,304 |
| Universe.c | Game/Universe.c | 23 | 25 | 2 | 93 | 26.9% | 24 | 11,247 |
| utility.c | SDL/utility.c | 25 | 25 | 0 | 98 | 25.5% | 25 | 13,153 |
| soundlow.c | SDL/soundlow.c | 24 | 24 | 0 | 31 | 77.4% | 24 | 4,628 |
| texreg.c | SDL/texreg.c | 24 | 24 | 1 | 76 | 31.6% | 22 | 12,191 |
| Collision.c | Game/Collision.c | 23 | 23 | 0 | 26 | 88.5% | 23 | 14,582 |
| CommandWrap.c | Game/CommandWrap.c | 23 | 23 | 5 | 24 | 95.8% | 23 | 3,522 |
| AIOrders.c | Game/AIOrders.c | 0 | 22 | 0 | 30 | 73.3% | 22 | 10,994 |
| TradeMgr.c | Game/TradeMgr.c | 22 | 22 | 0 | 41 | 53.7% | 22 | 1,919 |
| StatScript.c | Game/StatScript.c | 21 | 21 | 4 | 60 | 35.0% | 21 | 8,471 |
| font.c | SDL/font.c | 21 | 21 | 0 | 29 | 72.4% | 21 | 5,742 |
| Formation.c | Game/Formation.c | 19 | 20 | 0 | 47 | 42.6% | 19 | 11,960 |
| mainrgn.c | SDL/mainrgn.c | 18 | 20 | 0 | 93 | 21.5% | 20 | 6,487 |
| mouse.c | SDL/mouse.c | 3 | 20 | 0 | 29 | 69.0% | 20 | 8,436 |
| AITrack.c | Game/AITrack.c | 19 | 19 | 0 | 24 | 79.2% | 19 | 6,816 |
| File.c | Game/File.c | 19 | 19 | 0 | 30 | 63.3% | 18 | 6,480 |
| Matrix.c | Game/Matrix.c | 11 | 19 | 0 | 25 | 76.0% | 19 | 2,194 |
| Sensors.c | Game/Sensors.c | 19 | 19 | 0 | 56 | 33.9% | 19 | 8,273 |
| SpeechEvent.c | Game/SpeechEvent.c | 19 | 19 | 1 | 33 | 57.6% | 19 | 7,677 |
| prim2d.c | SDL/prim2d.c | 19 | 19 | 0 | 36 | 52.8% | 18 | 4,741 |
| Dock.c | Game/Dock.c | 5 | 18 | 0 | 113 | 15.9% | 18 | 5,139 |
| GravWellGenerator.c | Ships/GravWellGenerator.c | 1 | 18 | 0 | 21 | 85.7% | 18 | 3,298 |
| BTG.c | Game/BTG.c | 0 | 17 | 0 | 26 | 65.4% | 17 | 5,565 |
| NetCheck.c | Game/NetCheck.c | 17 | 17 | 0 | 24 | 70.8% | 17 | 2,398 |
| main.c | SDL/main.c | 17 | 17 | 1 | 69 | 24.6% | 13 | 7,163 |
| AITeam.c | Game/AITeam.c | 14 | 16 | 0 | 80 | 20.0% | 16 | 2,877 |
| Blobs.c | Game/Blobs.c | 16 | 16 | 0 | 31 | 51.6% | 16 | 7,317 |
| ColPick.c | Game/ColPick.c | 16 | 16 | 0 | 46 | 34.8% | 16 | 3,273 |
| ResearchGUI.c | Game/ResearchGUI.c | 16 | 16 | 0 | 47 | 34.0% | 16 | 6,318 |
| Task.c | Game/Task.c | 15 | 16 | 0 | 16 | 100.0% | 14 | 2,756 |
| AIPlayer.c | Game/AIPlayer.c | 12 | 15 | 0 | 34 | 44.1% | 14 | 3,564 |
| Memory.c | Game/Memory.c | 14 | 14 | 0 | 35 | 40.0% | 14 | 4,243 |
| Subtitle.c | Game/Subtitle.c | 14 | 14 | 0 | 26 | 53.8% | 14 | 4,926 |
| GenericInterceptor.c | Ships/GenericInterceptor.c | 8 | 13 | 0 | 17 | 76.5% | 13 | 6,153 |
| HorseRace.c | Game/HorseRace.c | 13 | 13 | 0 | 29 | 44.8% | 13 | 6,463 |
| LinkedList.c | Game/LinkedList.c | 13 | 13 | 0 | 19 | 68.4% | 13 | 1,351 |
| MinelayerCorvette.c | Ships/MinelayerCorvette.c | 2 | 13 | 0 | 14 | 92.9% | 13 | 4,139 |
| Objectives.c | Game/Objectives.c | 13 | 13 | 0 | 24 | 54.2% | 13 | 2,400 |
| PiePlate.c | Game/PiePlate.c | 13 | 13 | 0 | 20 | 65.0% | 13 | 6,755 |
| sstream.c | SDL/sstream.c | 13 | 13 | 0 | 17 | 76.5% | 13 | 3,294 |
| ChannelFSM.c | Game/ChannelFSM.c | 0 | 12 | 0 | 16 | 75.0% | 12 | 868 |
| Key.c | Game/Key.c | 12 | 12 | 0 | 17 | 70.6% | 12 | 920 |
| LaunchMgr.c | Game/LaunchMgr.c | 12 | 12 | 0 | 38 | 31.6% | 12 | 4,074 |
| ResCollect.c | Game/ResCollect.c | 11 | 12 | 0 | 26 | 46.2% | 12 | 3,209 |
| Vector.c | Game/Vector.c | 12 | 12 | 0 | 15 | 80.0% | 12 | 1,056 |
| AIUtilities.c | Game/AIUtilities.c | 11 | 11 | 0 | 113 | 9.7% | 11 | 2,082 |
| AutoDownloadMap.c | Game/AutoDownloadMap.c | 11 | 11 | 0 | 19 | 57.9% | 11 | 2,109 |
| MeshAnim.c | Game/MeshAnim.c | 0 | 11 | 0 | 21 | 52.4% | 11 | 2,335 |
| Timer.c | Game/Timer.c | 11 | 11 | 0 | 17 | 64.7% | 11 | 2,010 |
| prim3d.c | SDL/prim3d.c | 11 | 11 | 0 | 18 | 61.1% | 11 | 3,328 |
| Captaincy.c | Game/Captaincy.c | 10 | 10 | 0 | 30 | 33.3% | 10 | 3,364 |
| GameChat.c | Game/GameChat.c | 10 | 10 | 0 | 18 | 55.6% | 10 | 1,782 |
| ResearchAPI.c | Game/ResearchAPI.c | 10 | 10 | 2 | 32 | 31.2% | 10 | 1,699 |
| ShipSelect.c | Game/ShipSelect.c | 6 | 10 | 0 | 68 | 14.7% | 10 | 831 |
| AIShip.c | Game/AIShip.c | 9 | 9 | 0 | 29 | 31.0% | 9 | 8,082 |
| ScenPick.c | Game/ScenPick.c | 9 | 9 | 0 | 20 | 45.0% | 9 | 3,776 |
| Stats.c | Game/Stats.c | 9 | 9 | 0 | 61 | 14.8% | 9 | 2,334 |
| BigFile.c | Game/BigFile.c | 8 | 8 | 0 | 35 | 22.9% | 5 | 3,088 |
| Camera.c | Game/Camera.c | 0 | 8 | 0 | 17 | 47.1% | 8 | 2,456 |
| Carrier.c | Ships/Carrier.c | 3 | 8 | 0 | 10 | 80.0% | 8 | 902 |
| DefenseFighter.c | Ships/DefenseFighter.c | 0 | 8 | 0 | 19 | 42.1% | 8 | 669 |
| Demo.c | Game/Demo.c | 8 | 8 | 0 | 10 | 80.0% | 8 | 1,625 |
| GamePick.c | Game/GamePick.c | 8 | 8 | 0 | 31 | 25.8% | 8 | 1,564 |
| Light.c | Game/Light.c | 8 | 8 | 0 | 15 | 53.3% | 8 | 1,792 |
| Mothership.c | Ships/Mothership.c | 4 | 8 | 0 | 18 | 44.4% | 8 | 1,672 |
| Queue.c | SDL/Queue.c | 8 | 8 | 0 | 8 | 100.0% | 8 | 446 |
| SalCapCorvette.c | Ships/SalCapCorvette.c | 0 | 8 | 0 | 46 | 17.4% | 8 | 227 |
| glcaps.c | (not in HomeworldSDL) | 8 | 8 | 0 | - | - | 0 | 1,472 |
| smixer.c | SDL/smixer.c | 8 | 8 | 0 | 9 | 88.9% | 5 | 3,779 |
| AIFleetMan.c | Game/AIFleetMan.c | 7 | 7 | 0 | 21 | 33.3% | 7 | 2,855 |
| AIResourceMan.c | Game/AIResourceMan.c | 7 | 7 | 0 | 22 | 31.8% | 7 | 2,970 |
| Gun.c | Game/Gun.c | 6 | 7 | 0 | 20 | 35.0% | 7 | 6,624 |
| Physics.c | Game/Physics.c | 7 | 7 | 0 | 8 | 87.5% | 7 | 5,646 |
| Ping.c | Game/Ping.c | 5 | 7 | 0 | 25 | 28.0% | 6 | 884 |
| Trails.c | Game/Trails.c | 7 | 7 | 2 | 46 | 15.2% | 7 | 1,933 |
| Eval.c | Game/Eval.c | 0 | 6 | 0 | 19 | 31.6% | 6 | 3,298 |
| FontReg.c | Game/FontReg.c | 6 | 6 | 0 | 6 | 100.0% | 6 | 935 |
| HeavyCorvette.c | Ships/HeavyCorvette.c | 0 | 6 | 0 | 12 | 50.0% | 6 | 1,232 |
| Nebulae.c | Game/Nebulae.c | 4 | 6 | 0 | 71 | 8.5% | 6 | 2,435 |
| Randy.c | Game/Randy.c | 4 | 6 | 1 | 12 | 50.0% | 6 | 578 |
| Scroller.c | Game/Scroller.c | 6 | 6 | 0 | 7 | 85.7% | 6 | 530 |
| Select.c | Game/Select.c | 5 | 6 | 0 | 39 | 15.4% | 6 | 2,094 |
| Shader.c | Game/Shader.c | 6 | 6 | 0 | 32 | 18.8% | 6 | 936 |
| debugwnd.c | SDL/debugwnd.c | 6 | 6 | 0 | 16 | 37.5% | 6 | 2,442 |
| fqeffect.c | SDL/fqeffect.c | 6 | 6 | 1 | 20 | 30.0% | 6 | 557 |
| MissileDestroyer.c | Ships/MissileDestroyer.c | 0 | 5 | 0 | 6 | 83.3% | 5 | 951 |
| P1MissileCorvette.c | Ships/P1MissileCorvette.c | 1 | 5 | 0 | 6 | 83.3% | 5 | 497 |
| Tactics.c | Game/Tactics.c | 4 | 5 | 0 | 37 | 13.5% | 5 | 385 |
| TaskBar.c | Game/TaskBar.c | 5 | 5 | 0 | 49 | 10.2% | 5 | 1,330 |
| ddraw.cpp | (not in HomeworldSDL) | 5 | 5 | 0 | - | - | 0 | 660 |
| rinit.cpp | (not in HomeworldSDL) | 5 | 5 | 0 | - | - | 0 | 2,058 |
| AIAttackMan.c | Game/AIAttackMan.c | 4 | 4 | 0 | 23 | 17.4% | 4 | 2,548 |
| AIDefenseMan.c | Game/AIDefenseMan.c | 4 | 4 | 0 | 23 | 17.4% | 4 | 913 |
| AIEvents.c | Game/AIEvents.c | 4 | 4 | 0 | 32 | 12.5% | 4 | 1,650 |
| Attack.c | Game/Attack.c | 3 | 4 | 0 | 7 | 57.1% | 4 | 2,407 |
| BitIO.c | ThirdParty/LZSS/BitIO.c | 4 | 4 | 0 | 13 | 30.8% | 2 | 226 |
| DefaultShip.c | Ships/DefaultShip.c | 2 | 4 | 0 | 6 | 66.7% | 4 | 399 |
| HS.c | Game/HS.c | 4 | 4 | 0 | 28 | 14.3% | 4 | 1,212 |
| LagPrint.c | Game/LagPrint.c | 4 | 4 | 0 | 4 | 100.0% | 4 | 263 |
| ObjTypes.c | Game/ObjTypes.c | 4 | 4 | 1 | 46 | 8.7% | 4 | 203 |
| ProximitySensor.c | Ships/ProximitySensor.c | 0 | 4 | 0 | 13 | 30.8% | 4 | 816 |
| ShipView.c | Game/ShipView.c | 4 | 4 | 0 | 15 | 26.7% | 4 | 2,421 |
| StringSupport.c | Game/StringSupport.c | 4 | 4 | 0 | 6 | 66.7% | 4 | 778 |
| Tactical.c | Game/Tactical.c | 3 | 4 | 0 | 7 | 57.1% | 4 | 2,106 |
| TimeoutTimer.c | SDL/TimeoutTimer.c | 4 | 4 | 0 | 8 | 50.0% | 4 | 233 |
| Titan.c | SDL/Titan.c | 4 | 4 | 0 | 3 | 133.3% | 2 | 327 |
| AIVar.c | Game/AIVar.c | 1 | 3 | 0 | 18 | 16.7% | 3 | 329 |
| Animatic.c | Game/Animatic.c | 3 | 3 | 0 | 17 | 17.6% | 3 | 725 |
| Chatting.c | Game/Chatting.c | 0 | 3 | 0 | 3 | 100.0% | 3 | 306 |
| Clamp.c | Game/Clamp.c | 3 | 3 | 0 | 3 | 100.0% | 3 | 898 |
| Clouds.c | Game/Clouds.c | 3 | 3 | 0 | 43 | 7.0% | 3 | 1,023 |
| Crates.c | Game/Crates.c | 3 | 3 | 0 | 14 | 21.4% | 3 | 1,109 |
| DFGFrigate.c | Ships/DFGFrigate.c | 0 | 3 | 0 | 3 | 100.0% | 3 | 998 |
| Damage.c | Game/Damage.c | 1 | 3 | 0 | 15 | 20.0% | 3 | 660 |
| GenericDefender.c | Ships/GenericDefender.c | 1 | 3 | 0 | 4 | 75.0% | 3 | 1,730 |
| KeyBindings.c | Game/KeyBindings.c | 2 | 3 | 0 | 14 | 21.4% | 3 | 553 |
| LevelLoad.c | Game/LevelLoad.c | 3 | 3 | 0 | 47 | 6.4% | 3 | 3,202 |
| P1IonArrayFrigate.c | Ships/P1IonArrayFrigate.c | 0 | 3 | 0 | 3 | 100.0% | 3 | 267 |
| P1StandardCorvette.c | Ships/P1StandardCorvette.c | 0 | 3 | 0 | 4 | 75.0% | 3 | 380 |
| fqcodec.c | SDL/fqcodec.c | 3 | 3 | 0 | 5 | 60.0% | 3 | 1,372 |
| fquant.c | SDL/fquant.c | 3 | 3 | 0 | 4 | 75.0% | 3 | 1,270 |
| rglu.c | SDL/rglu.c | 3 | 3 | 0 | 10 | 30.0% | 3 | 976 |
| AIHandler.c | Game/AIHandler.c | 2 | 2 | 0 | 25 | 8.0% | 2 | 83 |
| AdvanceSupportFrigate.c | Ships/AdvanceSupportFrigate.c | 0 | 2 | 0 | 4 | 50.0% | 2 | 141 |
| Alliance.c | Game/Alliance.c | 1 | 2 | 1 | 10 | 20.0% | 2 | 352 |
| AutoLOD.c | Game/AutoLOD.c | 2 | 2 | 0 | 15 | 13.3% | 2 | 157 |
| CRC32.c | ThirdParty/CRC/CRC32.c | 2 | 2 | 0 | 2 | 100.0% | 2 | 104 |
| Debug.c | Game/Debug.c | 2 | 2 | 0 | 9 | 22.2% | 2 | 183 |
| Globals.c | Game/Globals.c | 2 | 2 | 0 | 2 | 100.0% | 1 | 334 |
| InfoOverlay.c | Game/InfoOverlay.c | 1 | 2 | 0 | 10 | 20.0% | 2 | 651 |
| LOD.c | Game/LOD.c | 2 | 2 | 1 | 11 | 18.2% | 2 | 865 |
| MEX.c | Game/MEX.c | 2 | 2 | 0 | 17 | 11.8% | 2 | 496 |
| MadLinkIn.c | Game/MadLinkIn.c | 2 | 2 | 0 | 16 | 12.5% | 2 | 820 |
| MultiGunCorvette.c | Ships/MultiGunCorvette.c | 0 | 2 | 0 | 4 | 50.0% | 2 | 103 |
| P1Mothership.c | Ships/P1Mothership.c | 0 | 2 | 0 | 3 | 66.7% | 2 | 101 |
| ResourceCollector.c | Ships/ResourceCollector.c | 0 | 2 | 0 | 4 | 50.0% | 2 | 85 |
| SoundEventPlay.c | Game/SoundEventPlay.c | 1 | 2 | 0 | 4 | 50.0% | 2 | 3,054 |
| SoundEventStop.c | Game/SoundEventStop.c | 2 | 2 | 0 | 2 | 100.0% | 2 | 136 |
| Transformer.c | Game/Transformer.c | 2 | 2 | 0 | 1 | 200.0% | 0 | 374 |
| Undo.c | Game/Undo.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 62 |
| rinit.c | SDL/rinit.c | 4 | 2 | 2 | 14 | 14.3% | 2 | 653 |
| Battle.c | Game/Battle.c | 1 | 1 | 0 | 26 | 3.8% | 1 | 21 |
| Bounties.c | Game/Bounties.c | 1 | 1 | 0 | 6 | 16.7% | 1 | 190 |
| FloatingCity.c | Ships/FloatingCity.c | 0 | 1 | 0 | 3 | 33.3% | 1 | 51 |
| HeavyCruiser.c | Ships/HeavyCruiser.c | 0 | 1 | 0 | 3 | 33.3% | 1 | 115 |
| IonCannonFrigate.c | Ships/IonCannonFrigate.c | 0 | 1 | 0 | 3 | 33.3% | 1 | 99 |
| P2Swarmer.c | Ships/P2Swarmer.c | 0 | 1 | 0 | 4 | 25.0% | 0 | 41 |
| StandardDestroyer.c | Ships/StandardDestroyer.c | 0 | 1 | 0 | 3 | 33.3% | 1 | 121 |
| StandardFrigate.c | Ships/StandardFrigate.c | 0 | 1 | 0 | 3 | 33.3% | 1 | 68 |
| Star3d.c | Game/Star3d.c | 0 | 1 | 0 | 3 | 33.3% | 1 | 86 |
| Teams.c | Game/Teams.c | 1 | 1 | 0 | 8 | 12.5% | 1 | 142 |
| fastmath.c | (not in HomeworldSDL) | 1 | 1 | 0 | - | - | 0 | 142 |
| screenshot.c | SDL/screenshot.c | 1 | 1 | 0 | 3 | 33.3% | 0 | 710 |

**HW1 files with 0 matches** (42 files that define at least one function; defs approx., most first):

SDL/utility_mac.c (98), SDL/TitanInterfaceC.c (43), SDL/TitanInterfaceC.stub.c (36), Game/File_Mac.c (30), SDL/ClientCDKey.cpp (25), SDL/avi.c (18), Game/PlugScreen.c (17), Ships/CloakGenerator.c (17), Ships/DDDFrigate.c (17), SDL/NetworkInterface.c (15), Ships/RepairCorvette.c (15), Ships/ResearchShip.c (15), SDL/wave.c (14), Game/ProfileTimers.c (12), Game/Color.c (11), SDL/mixfft.c (11), Game/Clipper.c (10), Game/StubTransform.c (9), Game/B-Spline.c (8), Game/Hash.c (6), Game/Types.c (6), Ships/P2MultiBeamFrigate.c (6), Game/NavLights.c (4), Game/Tracking.c (4), Game/Twiddle.c (4), Game/Volume.c (4), Ships/LightCorvette.c (4), Ships/P2AdvanceSwarmer.c (4), Ships/Probe.c (4), Ships/SensorArray.c (4), Game/Tweak.c (3), SDL/TitanPacketMsg.cpp (3), Ships/P2FuelPod.c (3), Ships/P2Mothership.c (3), Ships/P3StandardShip.c (3), ThirdParty/LZSS/LZSS.c (3), Game/BMP.c (2), SDL/dct.c (2), SDL/win32glue.c (2), Ships/Drone.c (2), Ships/ResourceController.c (2), Game/Matrix-mult.c (1)

Files new in round 2 (0 matches in round 1): FlightMan.c, AIOrders.c, BTG.c, ChannelFSM.c, MeshAnim.c, Camera.c, Eval.c, Chatting.c, Star3d.c and 18 Ships/*.c modules (ship-slot code). Largest gains: ETG.c 25 -> 167, Particle.c 24 -> 122, AIMoves.c 16 -> 120, SaveGame.c 13 -> 77, FlightMan.c 0 -> 39, Mesh.c 4 -> 32, CameraCommand.c 2 -> 28, SinglePlayer.c 13 -> 35.

### R2.5 Next-round targets: the 60 largest unnamed functions

Same method as section 8 (strings from strings.csv xrefs, up to 3 named callers/callees from call_graph.csv). `in round 1` = `yes` when the function already existed unnamed in round 1, `new (A1)` when A1 created it.

| # | address | size | calls in | strings | named callers | named callees | in round 1 |
|---:|---|---:|---:|---|---|---|---|
| 1 | 0x0042fd50 | 4,691 | 1 | - | cat_aisPlay | GetShipStaticInfo, cat_bigGunIsCharged, cat_mothershipBigGunActive | yes |
| 2 | 0x00436aa0 | 3,418 | 1 | - | cloudStartup | memAllocFunction, ranRandomFn | yes |
| 3 | 0x00424110 | 3,177 | 0 | - | - | gunShipFirePower, ranRandomFn, selShipInSelection | yes |
| 4 | 0x005312b0 | 3,147 | 1 | - | rndRenderTask | __ftol, pieCircleSegmentsCompute, primLine2 | yes |
| 5 | 0x00568100 | 3,141 | 1 | - | - | primCircleOutline3, vecNormalize | yes |
| 6 | 0x0057bec0 | 3,049 | 1 | - | rndMainViewRenderFunction | __ftol, primModeClearFunction2, primModeSetFunction2 | yes |
| 7 | 0x00506720 | 2,968 | 4 | - | cat_smMinesDraw, cat_smResourcesDraw, cat_smShipsDraw | hmatMultiplyHMatByHVec, primPointLineIntersection | yes |
| 8 | 0x0050fd60 | 2,923 | 1 | `%.0f`; `%s %d:` | - | __ftol, _sprintf, cat_shipGetDamageMultiplier | yes |
| 9 | 0x005b6f30 | 2,911 | 2 | - | LeechOrderChanged, LeechSpecialTarget | ApplyDamageToTarget, ClampToWithin, GetDirectionVectorOfShip | yes |
| 10 | 0x005c32f0 | 2,548 | 1 | `333?`; `droidUpdateArray`; `\ndroidGetNextDock: %s has no DPT_Repair` | - | ShipTypeToStr, cat_meshObjectNearestVertex, droidIsValidPredockPos | yes |
| 11 | 0x004a13f0 | 2,450 | 0 | `Bad shiptype in %s`; `colorScheme(%d)`; `colorScheme` | - | GetShipStaticInfo, GroupShipIntoMilitaryParade, RemoveCommasFromString | new (A1) |
| 12 | 0x0045a700 | 2,340 | 0 | - | - | cat_consoleExecuteCommand, cat_consoleOpen, memFree | yes |
| 13 | 0x00460ed0 | 2,334 | 1 | `\nGot %d RU's from crate.`; `\nGot a %d ship type  from crate!` | cratesPlayerFoundACrate | GetShipStaticInfo, clFormation, gameStatsAddShip | yes |
| 14 | 0x005053a0 | 2,312 | 3 | `\nWarning: Tried to select too many ship` | cat_mrBandBoxRelease, mrSelectHold | nullsubReturnFalse, primRealRectUnion2, sub_53F9F0 | yes |
| 15 | 0x004eb940 | 2,267 | 2 | - | - | __chkstk, cat_shipIsUpgradable, getShipAndItsCommand | yes |
| 16 | 0x004c9430 | 2,191 | 1 | - | - | - | yes |
| 17 | 0x00439130 | 2,161 | 1 | - | cloudSetFog | ranRandomFn, vecNormalize | yes |
| 18 | 0x0056b890 | 2,147 | 2 | - | - | - | yes |
| 19 | 0x004045b0 | 2,132 | 1 | - | aifProcessShipBuildRequests | GetShipStaticInfo, cat_playerHasCompletedShip | yes |
| 20 | 0x004ab840 | 2,062 | 1 | `hyperspace`; `ShipListClick` | cat_mrClickRelease | ccFocus, ioUpdateShipTotals, kbCommandKeyIsHit | yes |
| 21 | 0x004896b0 | 1,938 | 0 | `%d. %s` | - | __ftol, _sprintf, cat_overlayIconDraw | yes |
| 22 | 0x004c8b00 | 1,923 | 1 | - | - | - | yes |
| 23 | 0x0056cc70 | 1,881 | 1 | - | - | - | yes |
| 24 | 0x005a7310 | 1,835 | 1 | - | rndMainViewRenderFunction | cat_shipGetSpeedMultiplier, glCapFastFeature, matMultiplyMatByVec | yes |
| 25 | 0x005c60a0 | 1,806 | 2 | - | - | partFilter, primCircleSolid3, primSolidTexture3 | yes |
| 26 | 0x00598c80 | 1,795 | 1 | `TitanFactoryServer`; `TitanRoutingServer`; `%d.%d.%d.%d:%d` | TitanInterface_HandleDirMultiEntityReply | GetCurrentChannel, TitanInterface_PingFactServer, WONCommon__WStringToString | yes |
| 27 | 0x00481ef0 | 1,743 | 1 | `\nWarning: _SetFormationOffsetsForForceF` | FormationCalculateOffsets | nullsubReturnFalse | yes |
| 28 | 0x00560ba0 | 1,719 | 3 | `3dfxvgl.dll`; `opengl32.dll`; `Fatal Error` | opOptionsAcceptHelper, opTimerExpired | EXTERNAL:KERNEL32.DLL::FreeLibrary, EXTERNAL:KERNEL32.DLL::GetModuleHandleA, EXTERNAL:USER32.DLL::MessageBoxA | yes |
| 29 | 0x004ee5f0 | 1,693 | 2 | - | cat_mrReleaseLeft | allianceFormWith, gcRUTransferStart, smSensorsClose | yes |
| 30 | 0x004c3490 | 1,687 | 1 | `atrox `; `Matrox` | cat_mgAdBannerDraw | __ftol, _free, _malloc | yes |
| 31 | 0x00434c10 | 1,684 | 1 | - | - | - | yes |
| 32 | 0x00510e60 | 1,649 | 1 | `%s: %s`; `%s   (%s)`; `   %.0f %%` | smViewportRender | _sprintf, cat_shipCanUpgradeArmour, cat_shipExperienceLevelName | yes |
| 33 | 0x004ca0d0 | 1,632 | 1 | - | - | hmatMultiplyHMatByHVec, memAllocFunction, ranRandomFn | yes |
| 34 | 0x00484310 | 1,594 | 1 | `333?` | - | FormationCalculateOffsets, __chkstk, matMultiplyMatByVec | yes |
| 35 | 0x0057d2c0 | 1,563 | 1 | - | rndMainViewRenderFunction | primModeClearFunction2, primModeSetFunction2, trMakeCurrent | yes |
| 36 | 0x005a5570 | 1,553 | 1 | - | - | cat_shipGetSpeedMultiplier, hmatMakeHMatFromMat, meshRender | yes |
| 37 | 0x004e8850 | 1,552 | 1 | - | cat_smViewportRenderScene | __ftol, fontCurrentGet, fontHeight | yes |
| 38 | 0x004467c0 | 1,486 | 3 | - | ChangeOrderToMove, clMove | ChangeOrderToMove, RemoveShipsFromDoingStuff, __chkstk | yes |
| 39 | 0x00432760 | 1,462 | 1 | - | cat_aisPlay | clWrapAttack, clWrapMove, clWrapProtect | yes |
| 40 | 0x005a5f90 | 1,439 | 1 | - | - | - | yes |
| 41 | 0x00438090 | 1,436 | 3 | - | - | ranRandomFn | yes |
| 42 | 0x00464b30 | 1,428 | 2 | - | cat_dockShipDocksAtShip, cat_salvageDepotProcessDockable | GetDirectionVectorOfShip, MoveReachedDestinationVariable, aishipFlyToPointAvoidingObjsFunc | yes |
| 43 | 0x004f3380 | 1,375 | 2 | - | aifProcessShipBuildRequests | clWrapResearch | yes |
| 44 | 0x0052ba90 | 1,354 | 2 | - | gameStart, levelPreInit | - | yes |
| 45 | 0x004618c0 | 1,350 | 1 | - | - | pieMovePointClipToLimits, ranRandomFn, vecNormalize | yes |
| 46 | 0x0050e640 | 1,347 | 5 | - | meshMorphedObjectRender, meshSpecObjectRender | - | yes |
| 47 | 0x00513a00 | 1,336 | 4 | - | cat_mrReleaseRight, mouseSetCursorSetting, mrObjectClick | cat_mothershipBigGunFiring, cat_mothershipHasBigGunModule, cat_salvageCheckTargetReasons | yes |
| 48 | 0x00432df0 | 1,306 | 1 | - | cat_aisPlay | clWrapAttack, clWrapFormation, clWrapMpHyperspace | yes |
| 49 | 0x004c41c0 | 1,299 | 0 | `/%s `; `` | - | _sprintf, _strncpy, lgAddChatToRoomChat | new (A1) |
| 50 | 0x00561920 | 1,299 | 2 | `3dfxvgl.dll`; `Emergence`; `opengl32.dll` | opOptionsAcceptHelper | EXTERNAL:KERNEL32.DLL::FreeLibrary, EXTERNAL:KERNEL32.DLL::GetModuleHandleA, EXTERNAL:USER32.DLL::CreateWindowExA | yes |
| 51 | 0x004573d0 | 1,295 | 0 | `%s %s` | - | feStaticRectangleDraw, fontHeight, fontMakeCurrent | new (A1) |
| 52 | 0x004ba280 | 1,274 | 0 | `/%s `; `` | - | BroadcastChatMessage, SendPrivateChatMessage, _mbstowcs | new (A1) |
| 53 | 0x0057a410 | 1,274 | 1 | - | rndRenderTask | __ftol | yes |
| 54 | 0x00438c30 | 1,270 | 1 | - | - | ApplyDamageToTarget, ranRandomFn, vecNormalize | yes |
| 55 | 0x0048cf30 | 1,222 | 0 | - | - | fontHeight, fontMakeCurrent, fontPrint | new (A1) |
| 56 | 0x004ecfe0 | 1,218 | 3 | - | - | cat_uiFireSiegeCannon, cat_uiSpecialEMP, cat_uiSpecialInfect | yes |
| 57 | 0x0057ce00 | 1,206 | 1 | - | rndMainViewRenderFunction | __ftol, btgSetColourMultiplier | yes |
| 58 | 0x004ee120 | 1,199 | 1 | - | smViewportRender | __ftol, fontHeight, fontMakeCurrent | yes |
| 59 | 0x005c24e0 | 1,191 | 4 | - | AdvacedSupportFrigateSpecialTarget, CarrierSpecialTarget, ResourceCollectorSpecialTarget | RangeToTarget, aishipFlyToPointAvoidingObjsFunc, aishipGetTrajectory | yes |
| 60 | 0x00420a20 | 1,183 | 1 | `%s %s` | clSetAlliance | _sprintf, cat_playerGetFOWGrid, cat_shipAffectsFOW | yes |

Remaining `FUN_*` by 64 KB band (count / bytes): 0x0040xxxx 46/13,302, 0x0041xxxx 125/33,601, 0x0042xxxx 61/19,287, 0x0043xxxx 79/35,078, 0x0044xxxx 50/15,687, 0x0045xxxx 37/15,080, 0x0046xxxx 37/12,409, 0x0047xxxx 9/3,441, 0x0048xxxx 58/25,070, 0x0049xxxx 42/8,264, 0x004axxxx 72/20,977, 0x004bxxxx 66/12,960, 0x004cxxxx 64/23,841, 0x004dxxxx 111/12,601, 0x004exxxx 60/26,177, 0x004fxxxx 72/14,234, 0x0050xxxx 72/25,953, 0x0051xxxx 128/20,261, 0x0052xxxx 93/17,454, 0x0053xxxx 85/23,999, 0x0054xxxx 49/8,385, 0x0055xxxx 85/13,431, 0x0056xxxx 49/22,634, 0x0057xxxx 68/21,582, 0x0058xxxx 65/16,468, 0x0059xxxx 55/12,527, 0x005axxxx 162/33,459, 0x005bxxxx 92/16,309, 0x005cxxxx 89/25,114, 0x0069xxxx 16/3,114, 0x006axxxx 36/6,199, 0x006bxxxx 22/3,329, 0x006cxxxx 18/845, 0x006dxxxx 9/1,690, 0x006exxxx 7/654, 0x006fxxxx 3/337, 0x0089xxxx 42/42.

### R2.6 Ranked gaps for round 3

Ranked by bytes that can be won and by how directly the fix follows from data already on disk. The attributions marked "guess" rest only on the callers and callees listed in R2.5.

1. **Mission bodies are truncated.** All 2,085 mission functions are named, but Ghidra's bodies cover only 320,296 of the region's 629,296 bytes. There are 1,994 uncovered runs between function bodies, totalling 309,000 bytes; the largest is 10,731 bytes at 0x00666535. `ghidra/proposals/r2/missions-r2.body-fix.csv` lists 984 functions whose Ghidra size is smaller than the kas2c size documented in mission-code-map.md, with the callees that were lost (for example `Init_Mission01_KushanFlankFrigates_DoBattle` 0x005d7aa0: 45 bytes in Ghidra, 219 documented, 7 lost kasf calls). Fix: a Ghidra stage that re-creates each body up to `doc_end`, for instance with `CreateFunctionCmd` and an explicit body, or by clearing the stop and disassembling the tail. Then re-export so call_graph.csv and decompiled.c cover the mission code. This needs no new naming work.
2. **548 A1-created functions are still `FUN_*`** (101,475 bytes in all). 543 are pointer-table targets and 5 were found by analysis. 546 of them have no direct caller (`calls in` 0) because they are reached only through tables. Five of them are in the top 60:
   - 0x004a13f0 (2,450 bytes; `colorScheme`, `Bad shiptype in %s`)
   - chat handlers 0x004c41c0 and 0x004ba280 (`/%s`, `BroadcastChatMessage`)
   - 0x004573d0 and 0x0048cf30 (font/FE draw callbacks)

   111 of the 548 are in the WON/Magick range and need a library class (libs-unknown method).
3. **105 deferred proposals** (`ghidra/proposals/r2/reconcile_deferred_functions.json`). These are audited round-2 names whose address is free code, not a function entry. Round 3 can create those functions first (the A1 mechanism) and then apply the names.
4. **The game region still holds 2,081 `FUN_*` (549,585 bytes).** By cluster, from R2.5:
   - **Skirmish AI** (callers `cat_aisPlay`): 0x0042fd50 (4,691 bytes), 0x00432760, 0x00432df0.
   - **Clouds/nebulae** (HW1 `Clouds.c`, `Nebulae.c` 6 of 71 matched): 0x00436aa0 (caller `cloudStartup`), 0x00439130 (`cloudSetFog`), 0x00438090, 0x00438c30.
   - **Main-view renderer** (callers `rndMainViewRenderFunction`/`rndRenderTask`): 0x0057bec0, 0x0057d2c0, 0x0057ce00, 0x005a7310, 0x005312b0, 0x0057a410.
   - **Sensors manager** (`cat_sm*` callers): 0x00506720, 0x005053a0, 0x0050fd60, 0x00510e60, 0x004e8850, 0x004ee120.
   - **Ship code:** 0x005b6f30 (callers `LeechOrderChanged`/`LeechSpecialTarget`), 0x005c32f0 (`droidUpdateArray` strings, RepairDroid), 0x005c24e0 (three `*SpecialTarget` callers).
   - **Others:** dev console 0x0045a700; crates 0x00460ed0 (`Got %d RU's from crate`, HW1 `Crates.c`); formation 0x00481ef0 and 0x00484310.
   - **Densest bands:** 0x005axxxx (162 functions), 0x0051xxxx (128), 0x0041xxxx (125) and 0x004dxxxx (111).
5. **HW1 files with large gaps** (defs approx. / matched):
   - MultiplayerGame.c 233/78
   - AIMoves.c 270/120
   - NIS.c 159/30
   - Options.c 154/37
   - SaveGame.c 180/77
   - AIUtilities.c 113/11
   - Dock.c 113/18
   - ETG.c 244/167
   - CommandLayer.c 144/68
   - utility.c 98/25
   - mainrgn.c 93/20
   - SinglePlayer.c 107/35
   - Universe.c 93/25
   - Nebulae.c 71/6
   - AITeam.c 80/16
   - ShipSelect.c 68/10

   Game and ship files with 0 matches, leaving out the Mac/SDL/stub files: Ships/CloakGenerator.c, DDDFrigate.c, RepairCorvette.c, ResearchShip.c; Game/PlugScreen.c, Color.c, Clipper.c, B-Spline.c, Hash.c, NavLights.c, Tracking.c, Twiddle.c, Volume.c, Tweak.c and BMP.c. Some of these ships may not exist in Cataclysm (guess).
6. **Prototypes, now that types exist.** 880 of the 911 round-2 function symbols carry no prototype, and round 1 left 6,408 without one and 1,006 degraded. A2 put 1,735 Cataclysm/HW1 types into the program, so a prototype pass can now apply `Ship *`, `Player *`, `CustShipHeader`-slot and `taskdata` signatures from the HW1 declarations. Decompile quality across all 7,055 named functions improves without any renaming.
7. **Carried over:**
   - `qwen-naming.json` is still unaudited and excluded (STATE.md "Next steps" 1).
   - `universe.world[]` lands at +0x3D9B0, against CET's note of +0x3DA30. The offset is derived and unverified (`docs/ghidra-round2-prep.md`).
   - 2 thread-start functions keep USER_DEFINED `lpStartAddress_*` names.

   Resolved since round 1: the `cet-recheck` subsystem of gaps-input-save-sp CONFIRMED all 10 CET_container names from section 5.1 at high confidence (`etgAtDefine` ... `utyGameSystemsPreInit`). The libs-unknown compromise resolved the `memFree_2` and `utyGameQuitToMain_2` names from section 9 item 6.
