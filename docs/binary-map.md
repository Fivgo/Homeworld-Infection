# Binary map, rounds 1-5: cataclysm.exe (GOG build 12948)

> **M2-R3 name fixes (2026-10-10)** is the "M2-R3 name fixes" section at the end: 3 audited renames, no other change. The newest export is `ghidra/exports/cataclysm_gog_r6/`; it supersedes `cataclysm_gog_r5` for these 3 names and otherwise has the same functions, sizes and call edges.

> **Round 5 (2026-10-09)** is the "Round 5" section at the end. 14,705 functions; 9,590 have real names, covering 83.0% of function bytes. No `FUN_*` function is left anywhere. The game region is 99.1% real-named by function bytes (91.1% of all region bytes); the rest of the region is padding, about 70 KB of pointer-only code that Ghidra never made functions of (R5.5), and 140 class-only library/funclet entries. R5.7 gives the verdict: naming is saturated, so stop the naming rounds; R5.6 lists the remaining gaps. Where round 5 changed a fact, the Round 5 section is authoritative over the earlier ones.

> **Round 4 (2026-10-09)** is the "Round 4" section at the end. 14,679 functions; 9,049 have real names, covering 79.5% of function bytes (96.7% are not `FUN_*`). The game region is 93.1% real-named with 458 `FUN_*` left (99,908 bytes); all 2,085 mission functions have a name and an applied prototype. The round-5 targets are in R4.6. Where round 4 changed a fact, the Round 4 section is authoritative over the earlier ones.

> **Round 3 (2026-10-09)** is the "Round 3" section at the end. 14,490 functions; 8,235 have real names, covering 73.9% of function bytes. The game region is 83.5% real-named, and mission bodies now cover 97.4% of the mission region. The round-4 targets are in R3.6. Where round 3 changed a fact, the Round 3 section is authoritative over the earlier ones.

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

## Round 3

Generated 2026-10-09 from the re-export `ghidra/exports/cataclysm_gog_r3/`, made after `ghidra/symbols_round3.json` (3,087 symbols) was applied to project `ghidra/projects/cataclysm_gog`. Round 3 ran on branch `r2-apply-coverage` (worktree `.claude/worktrees/r2-apply`); the Ghidra project and exports stay in the main checkout. Round 3 had two Ghidra passes on the same project:
- **B1** re-grew the truncated mission bodies and created the 105 deferred round-2 functions (export `cataclysm_gog_r3a`; details in `docs/ghidra-round3-prep.md`).
- **Apply** is this pass.

The numbers come from `tools/binary_map_coverage.py`. Round 3 added options to it (`--history`, `--label`, `--before-label`, `--mid-label`, `--new-tag`) and the mission body metric (`body_bytes_union`: bytes of a region inside some function's `[address, address+size)`, the measure B1 used). With its defaults the script still reproduces R2.2 exactly (7,055 real-named functions, 59.7%, game region 65.72%, mission bodies 320,296 bytes, 3,053 HW1 matches). Round-3 command:

```
python3 -I tools/binary_map_coverage.py --export ghidra/exports/cataclysm_gog_r3 \
  --history "round 1=ghidra/exports/cataclysm_gog_r1" --before ghidra/exports/cataclysm_gog_r2 --before-label "round 2" \
  --mid ghidra/exports/cataclysm_gog_r3a --mid-label "after B1" --label "round 3" --new-tag r3 \
  --symbols ghidra/symbols_round1.json ghidra/symbols_round2.json ghidra/symbols_round3.json \
  --props ghidra/proposals/r2/*.verified.json ghidra/proposals/r3/*.verified.json --out /tmp/bmap_r3
```

### R3.1 What round 3 did

| pass | what | result |
|---|---|---|
| B1 bodies + deferred names (`FixFunctionBodies.py`, `CreateFunctions.py`, `ghidra/create_functions_r3.json`) | re-grow the 984 truncated mission bodies of `ghidra/proposals/r2/missions-r2.body-fix.csv` to `doc_end`; create the 105 deferred round-2 names; scan the uncovered mission runs | **984 / 984 bodies fixed** (0 clipped, skipped or failed). **105 / 105 deferred names** created and named. 1 more function found by analysis (`FUN_0067b680`, CRT range). 14,384 -> 14,490 functions. Mission bodies cover 612,691 of 629,296 bytes (97.4%, was 50.9%). 7,713 of 7,717 lost kasf callees are back in `call_graph.csv`. The scan created no function: what is left is padding and inline switch tables |
| producers + audits | 5 sets in `ghidra/proposals/r3/`, each written per subsystem: table-targets (the 548 A1-created `FUN_*`; raw-table reads of `.rdata`/`.data`), game-ai-ships (skirmish AI, sensors, AIMoves, AIUtil/AITeam, dock, CommandLayer, formation, ship code), game-render-ui (clouds/nebulae, main view, NIS, options, ShipSelect, mainrgn, utility, console, zero-match files), game-sim-mp (multiplayer, savegame, single player, universe, crates, ETG and the dense bands 0x5a/0x51/0x41/0x4d), prototypes (no renames; hw1 entries of rounds 1-2 with no or a degraded prototype) | verified sizes: table-targets 556 (audit precision 1.0), game-ai-ships 261 (0.967), game-render-ui 444 (1.0), game-sim-mp 760 (0.967), prototypes 2,007 kept of 2,084 (0.967) |
| reconcile (`ghidra/proposals/r3/reconcile_report.md`) | new names and prototypes on top of rounds 1-2 + B1; signatures cross-checked against `objdump` RET immediates and caller stack cleanup | 3,087 symbols: **1,080 new names** (1,075 function, 5 data), **0 renames**, **2,007 prototype-only entries**, 21 cross-set conflicts resolved, 182 class-only proposals (empty name, nothing to apply), **186 deferred to round 4** (address is not a function entry), 0 dropped, 0 suffixes, 0 CET anchors touched. New names by class: hw1 756, cat-new 256, won 67, crt 1; by confidence: high 768, medium 261, low 51. 420 new names carry a checked signature; 13 signatures were dropped |
| **apply** (this pass) | `tools/ghidra_headless.sh --symbols ghidra/symbols_round3.json --export-name cataclysm_gog_r3 original/game/cataclysm.exe cataclysm_gog` (reuse mode, `-noanalysis`, ImportSymbolsJson phase=all, ExportAnalysis with decompile), detached with `setsid nohup`, log `ghidra/exports/r3_headless.log` | 2026-10-09 15:16, rc=0, wall 63 s, no project-lock retry, 0 `ERROR`/Traceback/lock lines in the log. **3,087 of 3,087 symbols applied**: 3,082 `function` (1,075 `renamed`, 2,007 `same-name`) and 5 `label`. 0 `kept-user-name`, 0 `inside:`, 0 `error`. Prototypes: **2,319 applied, 31 applied degraded, 77 unparsed**, 655 new names had none. Decompile: ok 14,401, thunk 86, failed 3 (the same 3 as rounds 1 and 2), error 0 |

**Apply failures.** Names: 0. Every round-3 function name is the primary name at its address in `functions.csv`, and all 5 data labels are in `symbols.csv`. Prototypes: 77 failed to parse (`proto-unparsed`), all of them `TitanInterface_*` entries of the prototypes set written as `<ret> __thiscall TitanInterface_X(void *this, ...)` (0x0058a920 ... 0x0059d990). The importer's degraded fallback did not parse them either. Probable cause, not verified: Ghidra's C parser rejects the explicit `this` parameter together with `__thiscall`. Of the 2,007 prototype-only entries, 1,930 applied, and 1,900 of those changed the exported signature compared with r2. Spot checks: `cat_aisUpdateTeams` 0x0042fd50 (R2.5 #1), `scriptSetShipsCB` 0x004a13f0 (the A1 `colorScheme` table target), `droidUpdateArray` 0x005c32f0, `aieHandlerSetEnemyNearby` 0x00403f20 now exports as `void aieHandlerSetEnemyNearby(AITeamMove * move, float watchRadius, char oneShot, undefined4 handler)`. The handler typedef did not resolve to a function-pointer type, so that argument stayed `undefined4`.

### R3.2 Before / after

| measure | round 1 | round 2 | after B1 (`r3a`) | round 3 |
|---|---:|---:|---:|---:|
| functions in Ghidra | 11,194 | 14,384 | 14,490 | **14,490** |
| function bytes | 2,194,228 | 2,679,918 | 2,999,412 | 2,999,412 |
| real-named functions | 3,867 | 7,055 | 7,160 | **8,235** |
| real-named bytes (% of function bytes) | 1,019,110 (46.4%) | 1,599,230 (59.7%) | 1,918,604 (64.0%) | **2,216,810 (73.9%)** |
| not `FUN_*` (real + class-only) functions / % bytes | 8,992 / 70.2% | 12,150 / 78.9% | 12,255 / 81.1% | **13,330 / 91.1%** |
| unnamed `FUN_*` functions / bytes | 2,202 / 653,625 | 2,234 / 565,795 | 2,235 / 565,915 | **1,160 / 267,709** |
| game region 0x401000-0x5c9fff: functions | 5,079 | 6,073 | 6,178 | 6,178 |
| game region: real-named % of function bytes | 55.3% | 65.7% | 66.3% | **83.5%** |
| game region: `FUN_*` functions / bytes | 2,160 / 653,583 | 2,081 / 549,585 | 2,081 / 549,585 | **1,064 / 261,804** |
| mission region 0x5d6fe0-0x670a0f: functions (all real-named) | 0 | 2,085 | 2,085 | 2,085 |
| mission region: bytes inside a function body | 0 | 320,296 (50.9%) | 612,691 (97.4%) | **612,691 of 629,296 (97.4%)** |
| mission region: uncovered runs / bytes | - | 1,994 / 309,000 | 1,947 / 16,605 | 1,947 / 16,605 (1,911 padding runs = 14,866 bytes; 36 inline switch tables = 1,739 bytes) |
| HW1 functions matched (`hw1:*` class + CET) | 2,261 | 3,053 in 180 files (verbatim 2,873) | - | **3,946 in 190 files (verbatim 3,726)** |
| HW1 files with 0 matches (files that define something) | 69 | 42 | - | 32 |
| data labels applied by the pipeline | 34 | 1,010 | 1,010 | 1,015 |
| real-named functions with a prototype from a symbol file (full / degraded / unparsed) | - | 889 / 1,007 / 2 | - | **3,208 / 342 / 79** |
| real-named functions with no prototype in their symbol entry / not in any symbol file | - | 2,686 / 2,471 | - | 2,030 / 2,576 |
| A1-created `FUN_*` (the 548 of R2.6 gap 2) still unnamed | - | 548 (101,475 bytes) | 548 | **63 (7,535 bytes)**: 54 WON/Magick, 9 game |
| R2.5 top-60 still unnamed | - | 60 | 60 | 18 |

How to read the jump in bytes: B1 alone moved real-named bytes from 59.7% to 64.0%, because it grew already-named mission bodies by 292,395 bytes (the denominator grew by 319,494). The naming pass moved it from 64.0% to 73.9%. In the game region, where B1 changed almost nothing, real-named bytes went from 65.7% to 83.5%.

The prototype rows take, per real-named function, the latest apply-report status over `cataclysm_gog_r1`, `_r2` and `_r3/cet_apply_report.csv`, where a later name-only entry does not erase an earlier prototype. "Not in any symbol file" is mostly the 2,085 mission functions (created by A1 with a name and no prototype), plus CET, FID and import names.

Movement from round 2 to round 3: 1,074 round-2 `FUN_*` became real names. 106 new functions are real-named: B1's 105 and `FUN_0067b680`, which reconcile named. 0 real names were replaced, 0 class-only names changed, and no round-2 function disappeared. 183 of the remaining 1,160 `FUN_*` carry a class from an audited proposal with an empty name (for example 62 `lib:msvc-stl`), so the "unnamed FUN_*" class row below counts 977.

### R3.3 Per region, section and class

| region | round 2 functions | round 3 functions | real | class-only | `FUN_*` | `FUN_*` bytes | real % of function bytes, round 3 (round 2) |
|---|---:|---:|---:|---:|---:|---:|---|
| game C/C++ 0x401000-0x5ca0ff | 6,073 | 6,178 | 4,984 | 130 | 1,064 | 261,804 | 83.5% (65.7%) |
| IJG libjpeg 0x5ca100-0x5d6fdf | 102 | 102 | 41 | 61 | 0 | 0 | 23.8% (23.8%) |
| kas2c mission code 0x5d6fe0-0x670a0f | 2,085 | 2,085 | 2,085 | 0 | 0 | 0 | 100.0% (100.0%) |
| WSOCK32 thunks + static CRT 0x670a10-0x69412f | 595 | 596 | 257 | 339 | 0 | 0 | 41.1% (41.1%) |
| WON/Titan + AdApi + Magick 0x694130-0x705c9f | 2,216 | 2,216 | 696 | 1,466 | 54 | 5,863 | 33.3% (30.7%) |
| Dinkumware C++ std lib 0x705ca0-0x71cccf | 708 | 708 | 124 | 584 | 0 | 0 | 9.3% (9.3%) |
| late CRT 0x71ccd0-0x71f62f | 48 | 48 | 28 | 20 | 0 | 0 | 58.3% (58.3%) |
| text$x EH funclets 0x71f630-0x729fff | 2,343 | 2,343 | 0 | 2,343 | 0 | 0 | 0.0% (0.0%) |
| .cms_t stub + CRT 0xc55000-0xc68fff | 172 | 172 | 20 | 152 | 0 | 0 | 5.1% (5.1%) |

| section | functions | bytes | real | class-only | `FUN_*` | `FUN_*` bytes |
|---|---:|---:|---:|---:|---:|---:|
| .text | 14,276 | 2,963,012 | 8,215 | 4,943 | 1,118 | 267,667 |
| .cms_t | 172 | 36,358 | 20 | 152 | 0 | 0 |
| .data | 42 | 42 | 0 | 0 | 42 | 42 |

Per class (round 2 -> round 3 functions: hw1:* 3,011 -> 3,904, cat-new 2,864 -> 3,145, won 3,173 -> 3,290, lib:msvc-stl new with 62 still-`FUN_*` functions, lib:imagemagick 12 -> 16). The class of a function comes from the latest symbol round, then the audited r2 and r3 sets, then CET. Confidence counts are those of the class source.

| class | functions | bytes | real | class-only | high | medium | low | lowest - highest entry |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| hw1:* (all files) | 3,904 | 1,092,619 | 3,868 | 0 | 2,371 | 1,410 | 123 | 0x004018c0 - 0x005c79b0 |
| cat-new | 3,145 | 892,569 | 3,118 | 0 | 1,819 | 1,184 | 142 | 0x00417ec0 - 0x006708a0 |
| won | 3,290 | 328,980 | 670 | 2,570 | 3,130 | 160 | 0 | 0x00551dc0 - 0x007276f1 |
| crt | 1,913 | 277,069 | 335 | 1,578 | 1,583 | 330 | 0 | 0x00553420 - 0x00c67ca0 |
| unknown, lib=magick | 346 | 78,087 | 15 | 331 | 276 | 70 | 0 | 0x006e3de0 - 0x00727bd0 |
| unknown, lib=ijg | 61 | 19,307 | 0 | 61 | 28 | 33 | 0 | 0x005ca410 - 0x005d6d10 |
| CET (hw1) | 42 | 14,724 | 42 | 0 | 0 | 0 | 0 | 0x00420880 - 0x005a7a70 |
| lib:msvc-stl | 62 | 12,764 | 0 | 0 | 0 | 0 | 62 | 0x005a0170 - 0x005a4500 |
| unknown, lib=cms | 12 | 7,621 | 0 | 12 | 0 | 12 | 0 | 0x00c55000 - 0x00c60540 |
| CET (other) | 10 | 6,862 | 10 | 0 | 0 | 0 | 0 | 0x00431510 - 0x005bc900 |
| lib:libjpeg | 19 | 4,360 | 19 | 0 | 15 | 0 | 4 | 0x005c9cd0 - 0x005cf9d0 |
| unknown | 32 | 4,252 | 28 | 0 | 23 | 6 | 3 | 0x0043453b - 0x006fe9a0 |
| lib:imagemagick | 16 | 3,635 | 12 | 0 | 3 | 13 | 0 | 0x006e41c0 - 0x006f1710 |
| rgl | 3 | 3,586 | 3 | 0 | 1 | 2 | 0 | 0x00559a80 - 0x0055dd30 |
| bink | 8 | 2,503 | 8 | 0 | 6 | 2 | 0 | 0x00551220 - 0x00551d40 |
| other named | 650 | 12,723 | 107 | 543 | 0 | 0 | 0 | 0x00425ca0 - 0x00727750 |
| unnamed FUN_* | 977 | 237,751 | 0 | 0 | 0 | 0 | 0 | 0x00401000 - 0x00892114 |

### R3.4 Per HW1 source file

Same columns as R2.4, with the round-2 count. Totals: 3,946 matched functions in 190 files (verbatim 3,726). These files define about 6,754 functions; HomeworldSDL `src/` has 218 .c/.cpp basenames with about 7,176 definitions.

| HW1 file | path under src/ | round 2 | round 3 | of which CET | defs (approx.) | matched / defs | verbatim | bytes |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| KASFunc.c | Game/KASFunc.c | 293 | 293 | 0 | 313 | 93.6% | 293 | 16,460 |
| ETG.c | Game/ETG.c | 167 | 205 | 4 | 244 | 84.0% | 205 | 33,365 |
| AIMoves.c | Game/AIMoves.c | 120 | 162 | 0 | 270 | 60.0% | 162 | 28,181 |
| MultiplayerGame.c | Game/MultiplayerGame.c | 78 | 137 | 3 | 233 | 58.8% | 136 | 32,544 |
| Particle.c | Game/Particle.c | 122 | 132 | 0 | 175 | 75.4% | 132 | 28,809 |
| TitanInterface.cpp | SDL/TitanInterface.cpp | 112 | 112 | 0 | 165 | 67.9% | 23 | 56,453 |
| UnivUpdate.c | Game/UnivUpdate.c | 98 | 98 | 4 | 139 | 70.5% | 97 | 40,315 |
| CommandLayer.c | Game/CommandLayer.c | 68 | 97 | 0 | 144 | 67.4% | 96 | 46,672 |
| AIUtilities.c | Game/AIUtilities.c | 11 | 84 | 0 | 113 | 74.3% | 83 | 24,590 |
| SaveGame.c | Game/SaveGame.c | 77 | 83 | 1 | 180 | 46.1% | 83 | 24,606 |
| ShipSelect.c | Game/ShipSelect.c | 10 | 83 | 0 | 68 | 122.1% | 50 | 9,205 |
| NIS.c | Game/NIS.c | 30 | 80 | 0 | 159 | 50.3% | 79 | 25,968 |
| utility.c | SDL/utility.c | 25 | 58 | 0 | 98 | 59.2% | 58 | 17,432 |
| Universe.c | Game/Universe.c | 25 | 55 | 2 | 93 | 59.1% | 54 | 21,088 |
| CommandNetwork.c | Game/CommandNetwork.c | 53 | 53 | 0 | 68 | 77.9% | 52 | 15,236 |
| Options.c | Game/Options.c | 37 | 53 | 1 | 154 | 34.4% | 53 | 10,424 |
| ConsMgr.c | Game/ConsMgr.c | 40 | 48 | 0 | 87 | 55.2% | 48 | 17,091 |
| Tutor.c | Game/Tutor.c | 44 | 46 | 0 | 80 | 57.5% | 46 | 12,969 |
| FlightMan.c | Game/FlightMan.c | 39 | 45 | 0 | 63 | 71.4% | 45 | 13,987 |
| AITeam.c | Game/AITeam.c | 16 | 42 | 0 | 80 | 52.5% | 42 | 6,235 |
| SinglePlayer.c | Game/SinglePlayer.c | 35 | 42 | 2 | 107 | 39.3% | 42 | 16,049 |
| StatScript.c | Game/StatScript.c | 21 | 42 | 4 | 60 | 70.0% | 42 | 9,637 |
| FEFlow.c | Game/FEFlow.c | 38 | 38 | 1 | 71 | 53.5% | 35 | 11,322 |
| UIControls.c | Game/UIControls.c | 31 | 38 | 3 | 84 | 45.2% | 38 | 10,059 |
| MultiplayerLANGame.c | Game/MultiplayerLANGame.c | 28 | 37 | 0 | 77 | 48.1% | 36 | 11,275 |
| ColPick.c | Game/ColPick.c | 16 | 35 | 0 | 46 | 76.1% | 35 | 8,479 |
| TitanNet.c | Game/TitanNet.c | 35 | 35 | 0 | 39 | 89.7% | 35 | 3,507 |
| Mesh.c | Game/Mesh.c | 32 | 33 | 0 | 47 | 70.2% | 33 | 20,829 |
| SoundEvent.c | Game/SoundEvent.c | 32 | 32 | 0 | 51 | 62.7% | 32 | 17,324 |
| Trails.c | Game/Trails.c | 7 | 32 | 2 | 46 | 69.6% | 32 | 13,427 |
| Formation.c | Game/Formation.c | 20 | 31 | 0 | 47 | 66.0% | 29 | 16,469 |
| Region.c | Game/Region.c | 31 | 31 | 0 | 45 | 68.9% | 30 | 5,265 |
| KAS.c | Game/KAS.c | 30 | 30 | 0 | 44 | 68.2% | 30 | 4,426 |
| render.c | SDL/render.c | 28 | 30 | 0 | 60 | 50.0% | 25 | 14,960 |
| ObjTypes.c | Game/ObjTypes.c | 4 | 29 | 1 | 46 | 63.0% | 29 | 1,480 |
| mainrgn.c | SDL/mainrgn.c | 20 | 29 | 0 | 93 | 31.2% | 29 | 12,675 |
| CameraCommand.c | Game/CameraCommand.c | 28 | 28 | 0 | 40 | 70.0% | 28 | 7,312 |
| FEReg.c | Game/FEReg.c | 26 | 28 | 0 | 35 | 80.0% | 28 | 13,275 |
| Nebulae.c | Game/Nebulae.c | 6 | 28 | 0 | 71 | 39.4% | 28 | 16,132 |
| glcompat.c | (not in HomeworldSDL) | 28 | 28 | 0 | - | - | 0 | 12,438 |
| TradeMgr.c | Game/TradeMgr.c | 22 | 27 | 0 | 41 | 65.9% | 27 | 4,720 |
| Clouds.c | Game/Clouds.c | 3 | 25 | 0 | 43 | 58.1% | 25 | 14,255 |
| soundlow.c | SDL/soundlow.c | 24 | 24 | 0 | 31 | 77.4% | 24 | 4,628 |
| texreg.c | SDL/texreg.c | 24 | 24 | 1 | 76 | 31.6% | 22 | 12,191 |
| AIPlayer.c | Game/AIPlayer.c | 15 | 23 | 0 | 34 | 67.6% | 21 | 5,563 |
| Collision.c | Game/Collision.c | 23 | 23 | 0 | 26 | 88.5% | 23 | 14,582 |
| CommandWrap.c | Game/CommandWrap.c | 23 | 23 | 5 | 24 | 95.8% | 23 | 3,522 |
| LevelLoad.c | Game/LevelLoad.c | 3 | 23 | 0 | 47 | 48.9% | 23 | 12,187 |
| AIOrders.c | Game/AIOrders.c | 22 | 22 | 0 | 30 | 73.3% | 22 | 10,994 |
| Subtitle.c | Game/Subtitle.c | 14 | 22 | 0 | 26 | 84.6% | 22 | 5,863 |
| font.c | SDL/font.c | 21 | 21 | 0 | 29 | 72.4% | 21 | 5,742 |
| main.c | SDL/main.c | 17 | 21 | 1 | 69 | 30.4% | 17 | 7,387 |
| AIShip.c | Game/AIShip.c | 9 | 20 | 0 | 29 | 69.0% | 19 | 11,038 |
| mouse.c | SDL/mouse.c | 20 | 20 | 0 | 29 | 69.0% | 20 | 8,436 |
| AITrack.c | Game/AITrack.c | 19 | 19 | 0 | 24 | 79.2% | 19 | 6,816 |
| File.c | Game/File.c | 19 | 19 | 0 | 30 | 63.3% | 18 | 6,480 |
| Matrix.c | Game/Matrix.c | 19 | 19 | 0 | 25 | 76.0% | 19 | 2,194 |
| Sensors.c | Game/Sensors.c | 19 | 19 | 0 | 56 | 33.9% | 19 | 8,273 |
| SpeechEvent.c | Game/SpeechEvent.c | 19 | 19 | 1 | 33 | 57.6% | 19 | 7,677 |
| prim2d.c | SDL/prim2d.c | 19 | 19 | 0 | 36 | 52.8% | 18 | 4,741 |
| Dock.c | Game/Dock.c | 18 | 18 | 0 | 113 | 15.9% | 18 | 5,139 |
| GravWellGenerator.c | Ships/GravWellGenerator.c | 18 | 18 | 0 | 21 | 85.7% | 18 | 3,298 |
| Ping.c | Game/Ping.c | 7 | 18 | 0 | 25 | 72.0% | 17 | 4,208 |
| ResearchGUI.c | Game/ResearchGUI.c | 16 | 18 | 0 | 47 | 38.3% | 18 | 8,062 |
| BTG.c | Game/BTG.c | 17 | 17 | 0 | 26 | 65.4% | 17 | 5,565 |
| NetCheck.c | Game/NetCheck.c | 17 | 17 | 0 | 24 | 70.8% | 17 | 2,398 |
| Blobs.c | Game/Blobs.c | 16 | 16 | 0 | 31 | 51.6% | 16 | 7,317 |
| HorseRace.c | Game/HorseRace.c | 13 | 16 | 0 | 29 | 55.2% | 16 | 8,271 |
| Task.c | Game/Task.c | 16 | 16 | 0 | 16 | 100.0% | 14 | 2,756 |
| AIEvents.c | Game/AIEvents.c | 4 | 15 | 0 | 32 | 46.9% | 15 | 2,365 |
| AIHandler.c | Game/AIHandler.c | 2 | 15 | 0 | 25 | 60.0% | 15 | 2,606 |
| LaunchMgr.c | Game/LaunchMgr.c | 12 | 15 | 0 | 38 | 39.5% | 15 | 4,137 |
| Memory.c | Game/Memory.c | 14 | 14 | 0 | 35 | 40.0% | 14 | 4,243 |
| Objectives.c | Game/Objectives.c | 13 | 14 | 0 | 24 | 58.3% | 14 | 2,412 |
| AIResourceMan.c | Game/AIResourceMan.c | 7 | 13 | 0 | 22 | 59.1% | 11 | 5,578 |
| GenericInterceptor.c | Ships/GenericInterceptor.c | 13 | 13 | 0 | 17 | 76.5% | 13 | 6,153 |
| LinkedList.c | Game/LinkedList.c | 13 | 13 | 0 | 19 | 68.4% | 13 | 1,351 |
| MeshAnim.c | Game/MeshAnim.c | 11 | 13 | 0 | 21 | 61.9% | 13 | 2,582 |
| MinelayerCorvette.c | Ships/MinelayerCorvette.c | 13 | 13 | 0 | 14 | 92.9% | 13 | 4,139 |
| PiePlate.c | Game/PiePlate.c | 13 | 13 | 0 | 20 | 65.0% | 13 | 6,755 |
| Select.c | Game/Select.c | 6 | 13 | 0 | 39 | 33.3% | 13 | 8,328 |
| Stats.c | Game/Stats.c | 9 | 13 | 0 | 61 | 21.3% | 13 | 3,239 |
| sstream.c | SDL/sstream.c | 13 | 13 | 0 | 17 | 76.5% | 13 | 3,294 |
| ChannelFSM.c | Game/ChannelFSM.c | 12 | 12 | 0 | 16 | 75.0% | 12 | 868 |
| Key.c | Game/Key.c | 12 | 12 | 0 | 17 | 70.6% | 12 | 920 |
| ResCollect.c | Game/ResCollect.c | 12 | 12 | 0 | 26 | 46.2% | 12 | 3,209 |
| Timer.c | Game/Timer.c | 11 | 12 | 0 | 17 | 70.6% | 12 | 2,071 |
| Vector.c | Game/Vector.c | 12 | 12 | 0 | 15 | 80.0% | 12 | 1,056 |
| AutoDownloadMap.c | Game/AutoDownloadMap.c | 11 | 11 | 0 | 19 | 57.9% | 11 | 2,109 |
| GameChat.c | Game/GameChat.c | 10 | 11 | 0 | 18 | 61.1% | 11 | 2,444 |
| GamePick.c | Game/GamePick.c | 8 | 11 | 0 | 31 | 35.5% | 11 | 2,383 |
| ScenPick.c | Game/ScenPick.c | 9 | 11 | 0 | 20 | 55.0% | 11 | 3,976 |
| prim3d.c | SDL/prim3d.c | 11 | 11 | 0 | 18 | 61.1% | 11 | 3,328 |
| Captaincy.c | Game/Captaincy.c | 10 | 10 | 0 | 30 | 33.3% | 10 | 3,364 |
| DefenseFighter.c | Ships/DefenseFighter.c | 8 | 10 | 0 | 19 | 52.6% | 10 | 792 |
| Light.c | Game/Light.c | 8 | 10 | 0 | 15 | 66.7% | 10 | 1,957 |
| ResearchAPI.c | Game/ResearchAPI.c | 10 | 10 | 2 | 32 | 31.2% | 10 | 1,699 |
| Shader.c | Game/Shader.c | 6 | 10 | 0 | 32 | 31.2% | 10 | 3,325 |
| Battle.c | Game/Battle.c | 1 | 9 | 0 | 26 | 34.6% | 9 | 823 |
| RepairCorvette.c | Ships/RepairCorvette.c | 0 | 9 | 0 | 15 | 60.0% | 9 | 1,943 |
| BigFile.c | Game/BigFile.c | 8 | 8 | 0 | 35 | 22.9% | 5 | 3,088 |
| Camera.c | Game/Camera.c | 8 | 8 | 0 | 17 | 47.1% | 8 | 2,456 |
| Carrier.c | Ships/Carrier.c | 8 | 8 | 0 | 10 | 80.0% | 8 | 902 |
| Crates.c | Game/Crates.c | 3 | 8 | 0 | 14 | 57.1% | 8 | 5,155 |
| Demo.c | Game/Demo.c | 8 | 8 | 0 | 10 | 80.0% | 8 | 1,625 |
| Mothership.c | Ships/Mothership.c | 8 | 8 | 0 | 18 | 44.4% | 8 | 1,672 |
| Queue.c | SDL/Queue.c | 8 | 8 | 0 | 8 | 100.0% | 8 | 446 |
| SalCapCorvette.c | Ships/SalCapCorvette.c | 8 | 8 | 0 | 46 | 17.4% | 8 | 227 |
| glcaps.c | (not in HomeworldSDL) | 8 | 8 | 0 | - | - | 0 | 1,472 |
| smixer.c | SDL/smixer.c | 8 | 8 | 0 | 9 | 88.9% | 5 | 3,779 |
| AIFleetMan.c | Game/AIFleetMan.c | 7 | 7 | 0 | 21 | 33.3% | 7 | 2,855 |
| Clipper.c | Game/Clipper.c | 0 | 7 | 0 | 10 | 70.0% | 7 | 4,805 |
| Color.c | Game/Color.c | 0 | 7 | 0 | 11 | 63.6% | 7 | 2,270 |
| Gun.c | Game/Gun.c | 7 | 7 | 0 | 20 | 35.0% | 7 | 6,624 |
| Physics.c | Game/Physics.c | 7 | 7 | 0 | 8 | 87.5% | 7 | 5,646 |
| PlugScreen.c | Game/PlugScreen.c | 0 | 7 | 0 | 17 | 41.2% | 7 | 2,222 |
| Damage.c | Game/Damage.c | 3 | 6 | 0 | 15 | 40.0% | 6 | 1,883 |
| Eval.c | Game/Eval.c | 6 | 6 | 0 | 19 | 31.6% | 6 | 3,298 |
| FontReg.c | Game/FontReg.c | 6 | 6 | 0 | 6 | 100.0% | 6 | 935 |
| HeavyCorvette.c | Ships/HeavyCorvette.c | 6 | 6 | 0 | 12 | 50.0% | 6 | 1,232 |
| Randy.c | Game/Randy.c | 6 | 6 | 1 | 12 | 50.0% | 6 | 578 |
| Scroller.c | Game/Scroller.c | 6 | 6 | 0 | 7 | 85.7% | 6 | 530 |
| Tactical.c | Game/Tactical.c | 4 | 6 | 0 | 7 | 85.7% | 6 | 2,229 |
| TaskBar.c | Game/TaskBar.c | 5 | 6 | 0 | 49 | 12.2% | 6 | 1,349 |
| debugwnd.c | SDL/debugwnd.c | 6 | 6 | 0 | 16 | 37.5% | 6 | 2,442 |
| fqeffect.c | SDL/fqeffect.c | 6 | 6 | 1 | 20 | 30.0% | 6 | 557 |
| Attack.c | Game/Attack.c | 4 | 5 | 0 | 7 | 71.4% | 5 | 2,425 |
| MissileDestroyer.c | Ships/MissileDestroyer.c | 5 | 5 | 0 | 6 | 83.3% | 5 | 951 |
| P1MissileCorvette.c | Ships/P1MissileCorvette.c | 5 | 5 | 0 | 6 | 83.3% | 5 | 497 |
| Tactics.c | Game/Tactics.c | 5 | 5 | 0 | 37 | 13.5% | 5 | 385 |
| ddraw.cpp | (not in HomeworldSDL) | 5 | 5 | 0 | - | - | 0 | 660 |
| rinit.cpp | (not in HomeworldSDL) | 5 | 5 | 0 | - | - | 0 | 2,058 |
| AIAttackMan.c | Game/AIAttackMan.c | 4 | 4 | 0 | 23 | 17.4% | 4 | 2,548 |
| AIDefenseMan.c | Game/AIDefenseMan.c | 4 | 4 | 0 | 23 | 17.4% | 4 | 913 |
| AutoLOD.c | Game/AutoLOD.c | 2 | 4 | 0 | 15 | 26.7% | 4 | 195 |
| B-Spline.c | Game/B-Spline.c | 0 | 4 | 0 | 8 | 50.0% | 4 | 1,025 |
| BitIO.c | ThirdParty/LZSS/BitIO.c | 4 | 4 | 0 | 13 | 30.8% | 2 | 226 |
| DefaultShip.c | Ships/DefaultShip.c | 4 | 4 | 0 | 6 | 66.7% | 4 | 399 |
| HS.c | Game/HS.c | 4 | 4 | 0 | 28 | 14.3% | 4 | 1,212 |
| LOD.c | Game/LOD.c | 2 | 4 | 1 | 11 | 36.4% | 4 | 1,146 |
| LagPrint.c | Game/LagPrint.c | 4 | 4 | 0 | 4 | 100.0% | 4 | 263 |
| ProximitySensor.c | Ships/ProximitySensor.c | 4 | 4 | 0 | 13 | 30.8% | 4 | 816 |
| ShipView.c | Game/ShipView.c | 4 | 4 | 0 | 15 | 26.7% | 4 | 2,421 |
| StringSupport.c | Game/StringSupport.c | 4 | 4 | 0 | 6 | 66.7% | 4 | 778 |
| Teams.c | Game/Teams.c | 1 | 4 | 0 | 8 | 50.0% | 4 | 420 |
| TimeoutTimer.c | SDL/TimeoutTimer.c | 4 | 4 | 0 | 8 | 50.0% | 4 | 233 |
| Titan.c | SDL/Titan.c | 4 | 4 | 0 | 3 | 133.3% | 2 | 327 |
| Volume.c | Game/Volume.c | 0 | 4 | 0 | 4 | 100.0% | 4 | 1,130 |
| AIVar.c | Game/AIVar.c | 3 | 3 | 0 | 18 | 16.7% | 3 | 329 |
| Alliance.c | Game/Alliance.c | 2 | 3 | 1 | 10 | 30.0% | 3 | 386 |
| Animatic.c | Game/Animatic.c | 3 | 3 | 0 | 17 | 17.6% | 3 | 725 |
| Chatting.c | Game/Chatting.c | 3 | 3 | 0 | 3 | 100.0% | 3 | 306 |
| Clamp.c | Game/Clamp.c | 3 | 3 | 0 | 3 | 100.0% | 3 | 898 |
| DFGFrigate.c | Ships/DFGFrigate.c | 3 | 3 | 0 | 3 | 100.0% | 3 | 998 |
| GenericDefender.c | Ships/GenericDefender.c | 3 | 3 | 0 | 4 | 75.0% | 3 | 1,730 |
| KeyBindings.c | Game/KeyBindings.c | 3 | 3 | 0 | 14 | 21.4% | 3 | 553 |
| P1IonArrayFrigate.c | Ships/P1IonArrayFrigate.c | 3 | 3 | 0 | 3 | 100.0% | 3 | 267 |
| P1StandardCorvette.c | Ships/P1StandardCorvette.c | 3 | 3 | 0 | 4 | 75.0% | 3 | 380 |
| Tweak.c | Game/Tweak.c | 0 | 3 | 0 | 3 | 100.0% | 3 | 205 |
| fqcodec.c | SDL/fqcodec.c | 3 | 3 | 0 | 5 | 60.0% | 3 | 1,372 |
| fquant.c | SDL/fquant.c | 3 | 3 | 0 | 4 | 75.0% | 3 | 1,270 |
| rglu.c | SDL/rglu.c | 3 | 3 | 0 | 10 | 30.0% | 3 | 976 |
| AdvanceSupportFrigate.c | Ships/AdvanceSupportFrigate.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 141 |
| BMP.c | Game/BMP.c | 0 | 2 | 0 | 2 | 100.0% | 2 | 227 |
| CRC32.c | ThirdParty/CRC/CRC32.c | 2 | 2 | 0 | 2 | 100.0% | 2 | 104 |
| Debug.c | Game/Debug.c | 2 | 2 | 0 | 9 | 22.2% | 2 | 183 |
| Globals.c | Game/Globals.c | 2 | 2 | 0 | 2 | 100.0% | 1 | 334 |
| InfoOverlay.c | Game/InfoOverlay.c | 2 | 2 | 0 | 10 | 20.0% | 2 | 651 |
| MEX.c | Game/MEX.c | 2 | 2 | 0 | 17 | 11.8% | 2 | 496 |
| MadLinkIn.c | Game/MadLinkIn.c | 2 | 2 | 0 | 16 | 12.5% | 2 | 820 |
| MultiGunCorvette.c | Ships/MultiGunCorvette.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 103 |
| NavLights.c | Game/NavLights.c | 0 | 2 | 0 | 4 | 50.0% | 2 | 634 |
| P1Mothership.c | Ships/P1Mothership.c | 2 | 2 | 0 | 3 | 66.7% | 2 | 101 |
| ResourceCollector.c | Ships/ResourceCollector.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 85 |
| SoundEventPlay.c | Game/SoundEventPlay.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 3,054 |
| SoundEventStop.c | Game/SoundEventStop.c | 2 | 2 | 0 | 2 | 100.0% | 2 | 136 |
| Transformer.c | Game/Transformer.c | 2 | 2 | 0 | 1 | 200.0% | 0 | 374 |
| Twiddle.c | Game/Twiddle.c | 0 | 2 | 0 | 4 | 50.0% | 2 | 55 |
| Undo.c | Game/Undo.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 62 |
| fastmath.c | (not in HomeworldSDL) | 1 | 2 | 0 | - | - | 0 | 227 |
| rinit.c | SDL/rinit.c | 2 | 2 | 2 | 14 | 14.3% | 2 | 653 |
| Bounties.c | Game/Bounties.c | 1 | 1 | 0 | 6 | 16.7% | 1 | 190 |
| FloatingCity.c | Ships/FloatingCity.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 51 |
| HeavyCruiser.c | Ships/HeavyCruiser.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 115 |
| IonCannonFrigate.c | Ships/IonCannonFrigate.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 99 |
| P2Swarmer.c | Ships/P2Swarmer.c | 1 | 1 | 0 | 4 | 25.0% | 0 | 41 |
| StandardDestroyer.c | Ships/StandardDestroyer.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 121 |
| StandardFrigate.c | Ships/StandardFrigate.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 68 |
| Star3d.c | Game/Star3d.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 86 |
| screenshot.c | SDL/screenshot.c | 1 | 1 | 0 | 3 | 33.3% | 0 | 710 |

**HW1 files with 0 matches** (32 files that define at least one function; defs approx., most first):

SDL/utility_mac.c (98), SDL/TitanInterfaceC.c (43), SDL/TitanInterfaceC.stub.c (36), Game/File_Mac.c (30), SDL/ClientCDKey.cpp (25), SDL/avi.c (18), Ships/CloakGenerator.c (17), Ships/DDDFrigate.c (17), SDL/NetworkInterface.c (15), Ships/ResearchShip.c (15), SDL/wave.c (14), Game/ProfileTimers.c (12), SDL/mixfft.c (11), Game/StubTransform.c (9), Game/Hash.c (6), Game/Types.c (6), Ships/P2MultiBeamFrigate.c (6), Game/Tracking.c (4), Ships/LightCorvette.c (4), Ships/P2AdvanceSwarmer.c (4), Ships/Probe.c (4), Ships/SensorArray.c (4), SDL/TitanPacketMsg.cpp (3), Ships/P2FuelPod.c (3), Ships/P2Mothership.c (3), Ships/P3StandardShip.c (3), ThirdParty/LZSS/LZSS.c (3), SDL/dct.c (2), SDL/win32glue.c (2), Ships/Drone.c (2), Ships/ResourceController.c (2), Game/Matrix-mult.c (1)

Largest gains: AIUtilities.c 11 -> 84, ShipSelect.c 10 -> 83, MultiplayerGame.c 78 -> 137, NIS.c 30 -> 80, AIMoves.c 120 -> 162, ETG.c 167 -> 205, utility.c 25 -> 58, Universe.c 25 -> 55, CommandLayer.c 68 -> 97, AITeam.c 16 -> 42, Trails.c 7 -> 32, ObjTypes.c 4 -> 29, Nebulae.c 6 -> 28, Clouds.c 3 -> 25, LevelLoad.c 3 -> 23. Files new in round 3: Ships/RepairCorvette.c, Game/Clipper.c, Color.c, PlugScreen.c, B-Spline.c, Volume.c, Tweak.c, BMP.c, NavLights.c and Twiddle.c. ShipSelect.c shows 83 matches against about 68 regex `defs`, and only 50 of the 83 names occur in it verbatim. The other 33 are classed `hw1:ShipSelect.c` under Cataclysm or non-HW1 names, so the per-file attribution of that set should be re-checked (R3.6 gap 7).

### R3.5 Next-round targets: the 60 largest unnamed functions

Same method as R2.5. `in round 2` = `yes` when the function was already unnamed in round 2. 42 of round 2's top 60 are now named, among them `cat_aisUpdateTeams` 0x0042fd50, `cloudUpdateSystem` 0x00439130, `cat_rndLensFlaresRender` 0x0057bec0, `selCircleCompute` 0x00506720, `cat_leechProcess` 0x005b6f30, `droidUpdateArray` 0x005c32f0, `cratesRewardPlayer` 0x00460ed0, `cat_consoleKeyPress` 0x0045a700 and `refuelRepairShips` 0x005c24e0.

| # | address | size | calls in | strings | named callers | named callees | in round 2 |
|---:|---|---:|---:|---|---|---|---|
| 1 | 0x00424110 | 3,177 | 0 | - | - | fsqrt, gunShipFirePower, ranRandomFn | yes |
| 2 | 0x004eb940 | 2,267 | 2 | - | cat_popupMenuDraw, cat_popupMenuOpen | MothershipOrCarrierIndexInSelection, __chkstk, allianceArePlayersAllied | yes |
| 3 | 0x0056b890 | 2,147 | 2 | - | - | - | yes |
| 4 | 0x004045b0 | 2,132 | 1 | - | aifProcessShipBuildRequests | GetShipStaticInfo, cat_playerHasCompletedShip | yes |
| 5 | 0x004ab840 | 2,062 | 1 | `hyperspace`; `ShipListClick` | cat_mrClickRelease | cat_popupMenuIsOpen, cat_shipPrintActive, ccFocus | yes |
| 6 | 0x004896b0 | 1,938 | 0 | `%d. %s` | - | __ftol, _sprintf, allianceArePlayersAllied | yes |
| 7 | 0x0056cc70 | 1,881 | 1 | - | - | - | yes |
| 8 | 0x005c60a0 | 1,806 | 2 | - | - | cat_colBlendRGBA, partFilter, primCircleSolid3 | yes |
| 9 | 0x00598c80 | 1,795 | 1 | `TitanFactoryServer`; `TitanRoutingServer`; `%d.%d.%d.%d:%d` | TitanInterface_HandleDirMultiEntityReply | GetCurrentChannel, TitanInterface_PingFactServer, WONCommon__WStringToString | yes |
| 10 | 0x00560ba0 | 1,719 | 3 | `3dfxvgl.dll`; `opengl32.dll`; `Fatal Error` | opCountdownNo, opOptionsAcceptHelper, opTimerExpired | EXTERNAL:KERNEL32.DLL::FreeLibrary, EXTERNAL:KERNEL32.DLL::GetModuleHandleA, EXTERNAL:USER32.DLL::MessageBoxA | yes |
| 11 | 0x004ee5f0 | 1,693 | 2 | - | cat_mrReleaseLeft | allianceArePlayersAllied, allianceFormWith, cat_popupMenuClose | yes |
| 12 | 0x004c3490 | 1,687 | 1 | `atrox `; `Matrox` | cat_mgAdBannerDraw | __ftol, _free, _malloc | yes |
| 13 | 0x004f3380 | 1,375 | 2 | - | aifProcessShipBuildRequests | clWrapResearch | yes |
| 14 | 0x0052ba90 | 1,354 | 2 | - | gameStart, levelPreInit | colRGBToHSV | yes |
| 15 | 0x00513a00 | 1,336 | 4 | - | cat_mrReleaseRight, mouseSetCursorSetting, mrObjectClick | cat_leechGetAction, cat_leechIsLeeching, cat_mothershipBigGunFiring | yes |
| 16 | 0x00561920 | 1,299 | 2 | `3dfxvgl.dll`; `Emergence`; `opengl32.dll` | opOptionsAcceptHelper | EXTERNAL:KERNEL32.DLL::FreeLibrary, EXTERNAL:KERNEL32.DLL::GetModuleHandleA, EXTERNAL:USER32.DLL::CreateWindowExA | yes |
| 17 | 0x004ecfe0 | 1,218 | 3 | - | - | MakeShipsFriendlyShips, MakeTargetsOnlyNonForceAttackTargets, cat_uiFireSiegeCannon | yes |
| 18 | 0x00420a20 | 1,183 | 1 | `%s %s` | clSetAlliance | RemoveAttackTargetFromExtraAttackInfo, _sprintf, cat_playerGetFOWGrid | yes |
| 19 | 0x004ac050 | 1,168 | 2 | `hyperspace`; `end mission` | ioUpdateShipTotals, smViewportRender | cat_popupMenuIsOpen, cat_shipPrintActive, kbCommandKeyIsHit | yes |
| 20 | 0x005c1010 | 1,166 | 1 | - | cat_rammingFrigateCollide | ApplyDamageToTarget, RemoveShipsFromDoingStuff, cat_shipIsACV | yes |
| 21 | 0x004eac30 | 1,140 | 5 | `%d. %s` | cat_popupMenuDraw, cat_popupMenuOpen | __chkstk, _sprintf | yes |
| 22 | 0x00403130 | 1,131 | 1 | `msdp` | aidDefenseManager | aimCreateDefendMothershipNoAdd, aimInsertMove, aioCreateDefendMothership | yes |
| 23 | 0x00534300 | 1,128 | 4 | - | cmClose, lmCloseIfOpen, lmLaunchCapableShipDied | regChildAlloc, regDrawFunctionSet, regRegionDelete | yes |
| 24 | 0x0058c110 | 1,122 | 1 | - | - | EXTERNAL:KERNEL32.DLL::DeleteCriticalSection, operator_delete | yes |
| 25 | 0x004ef1c0 | 1,117 | 4 | - | - | cat_popupMenuClose, cat_popupMenuIsOpen | yes |
| 26 | 0x005433d0 | 1,110 | 1 | - | cat_univShipDeferredUpdate | cat_beastMothershipSystemActive, cat_shipHasUpgrade, droidUpdateArray | yes |
| 27 | 0x0048fd10 | 1,100 | 1 | - | cat_rndRenderObjectList | __ftol, hmatMakeHMatFromMat, matMultiplyMatByVec | yes |
| 28 | 0x005c89a0 | 1,089 | 1 | - | WorkerSpecialTarget | etgEffectDelete, listDeleteNode, madAnimationStart | yes |
| 29 | 0x00480650 | 1,080 | 1 | - | - | __ftol, vecNormalize | yes |
| 30 | 0x005af940 | 1,071 | 1 | - | processLaunchShipToDo | ranRandomFn, unClampObj, vecNormalize | yes |
| 31 | 0x005c6830 | 1,060 | 1 | - | SentinelHouseKeep | clampObjToObj, formationContentHasChanged, fsqrt | yes |
| 32 | 0x004ecb30 | 1,059 | 1 | - | cat_popupMenuDraw | cat_overlayIconDraw, colBlend, fontPrint | yes |
| 33 | 0x0047fd80 | 1,054 | 1 | - | - | listAddNode, listRemoveNode | yes |
| 34 | 0x004012a0 | 1,032 | 2 | - | - | aioCreateCapture, aioCreateCorvetteStrike, aioCreateFancyTakeoutTarget | yes |
| 35 | 0x00488e70 | 1,029 | 1 | - | - | fontHeight, fontMakeCurrent, fontWidth | yes |
| 36 | 0x004eb3e0 | 1,024 | 1 | - | cat_popupMenuDraw | - | yes |
| 37 | 0x004edc70 | 1,023 | 1 | - | cat_popupMenuDraw | cat_mothershipBigGunFiring, cat_mothershipHasBigGunModule, cat_selSelectionAddShip | yes |
| 38 | 0x00585760 | 1,014 | 11 | - | cat_lfTextureFileSetCB, cat_rndFogLayerTextureLoad, cat_scriptSetFogCB | __strupr, crc32Compute, memAllocFunction | yes |
| 39 | 0x005a2460 | 974 | 4 | - | - | operator_delete | yes |
| 40 | 0x005a2ae0 | 974 | 2 | - | - | operator_delete | yes |
| 41 | 0x005a33d0 | 966 | 7 | - | TitanInterface_HandleCloseCmd, TitanInterface_HandleLeaveGame, TitanInterface_LeaveGameNotify | operator_delete | yes |
| 42 | 0x0049b1b0 | 951 | 1 | - | keyInit | - | yes |
| 43 | 0x00561570 | 935 | 1 | `3dfxvgl.dll`; `Emergence`; `opengl32.dll` | opOptionsAcceptHelper | EXTERNAL:KERNEL32.DLL::FreeLibrary, EXTERNAL:KERNEL32.DLL::GetModuleHandleA, EXTERNAL:USER32.DLL::CreateWindowExA | yes |
| 44 | 0x004b7880 | 925 | 1 | `Rct0`; `Col0` | precalcStaticHeader | - | yes |
| 45 | 0x00542cb0 | 893 | 1 | - | univUpdateAllPosVelBullets | fsqrt, vecNormalize | yes |
| 46 | 0x004802d0 | 882 | 1 | - | fowRender | colBlend | yes |
| 47 | 0x00588b80 | 862 | 6 | `\n** nopal freed %dMB of textures **`; `\n** nopalw/o freed %dMB of textures **` | gameStart, mainRestoreSoftware | nullsubReturnFalse | yes |
| 48 | 0x00507bc0 | 845 | 3 | - | - | cat_acvEMPChargeFraction, cat_beastWeaponChargeFraction, cat_bigGunChargeFraction | yes |
| 49 | 0x0058edb0 | 843 | 2 | `login.ks` | TitanInterface_AuthHandleChallenge, TitanInterface_AuthHandleLoginReply | _fclose, _fgetc, _fopen | yes |
| 50 | 0x004eb0b0 | 816 | 1 | - | - | fontHeight, fontWidth | yes |
| 51 | 0x005718b0 | 814 | 5 | - | cat_moPrintOptionsDraw, cat_moTooltipDraw, cat_shipPrintDraw | primOvalArcOutline2 | yes |
| 52 | 0x004ed550 | 802 | 3 | - | - | - | yes |
| 53 | 0x0049bf30 | 799 | 1 | - | - | - | yes |
| 54 | 0x00576010 | 797 | 4 | - | - | fsqrt, ranRandomFn, vecNormalize | yes |
| 55 | 0x00556990 | 787 | 1 | `DDERR_WRONGMODE`; `DDERR_NODC`; `DDERR_CANTCREATEDC` | - | - | yes |
| 56 | 0x00565f30 | 785 | 3 | - | cat_mrReleaseRight, mrObjectClick | MakeShipsNotIncludeTheseShips, ShipInSelection, __chkstk | yes |
| 57 | 0x005750c0 | 781 | 1 | - | rndMainViewRenderFunction | hmatMakeHMatFromMat, rgluLookAt, rgluPerspective | yes |
| 58 | 0x00561260 | 777 | 2 | `3dfxvgl.dll`; `Emergence`; `opengl32.dll` | opCountdownNo, opTimerExpired | EXTERNAL:KERNEL32.DLL::FreeLibrary, EXTERNAL:KERNEL32.DLL::GetModuleHandleA, EXTERNAL:USER32.DLL::CreateWindowExA | yes |
| 59 | 0x00585b60 | 776 | 1 | - | meshRecolorize | memAllocFunction | yes |
| 60 | 0x005a0e30 | 774 | 2 | - | TitanInterface_ConnectToRoutingServer, TitanInterface_HandleCloseCmd | - | yes |

Remaining `FUN_*` by 64 KB band (count / bytes): 0x0040xxxx 22/10,064, 0x0041xxxx 3/1,160, 0x0042xxxx 39/11,465, 0x0043xxxx 20/5,467, 0x0044xxxx 13/2,998, 0x0045xxxx 16/3,580, 0x0046xxxx 19/3,776, 0x0047xxxx 6/2,385, 0x0048xxxx 37/14,899, 0x0049xxxx 29/5,270, 0x004axxxx 46/10,546, 0x004bxxxx 9/2,659, 0x004cxxxx 1/1,687, 0x004exxxx 38/19,100, 0x004fxxxx 61/10,446, 0x0050xxxx 48/12,161, 0x0051xxxx 61/9,552, 0x0052xxxx 56/12,101, 0x0053xxxx 36/7,733, 0x0054xxxx 33/6,924, 0x0055xxxx 45/7,358, 0x0056xxxx 40/16,446, 0x0057xxxx 41/10,552, 0x0058xxxx 57/15,531, 0x0059xxxx 55/12,527, 0x005axxxx 82/15,887, 0x005bxxxx 77/11,165, 0x005cxxxx 74/18,365, 0x0069xxxx 6/1,276, 0x006axxxx 9/1,018, 0x006bxxxx 14/1,838, 0x006cxxxx 18/845, 0x006dxxxx 2/430, 0x006exxxx 2/119, 0x006fxxxx 3/337, 0x0089xxxx 42/42.

### R3.6 Ranked gaps for round 4

Ranked by bytes that can be won and by how directly the fix follows from data already on disk. Cluster attributions rest on the callers, callees and strings in R3.5; the ones marked "guess" rest on nothing else.

1. **The game region still holds 1,064 `FUN_*` (261,804 bytes).** 89 have no direct caller, and 141 are 32 bytes or smaller. By cluster, from R3.5:
   - **Popup/context menu** (callers `cat_popupMenuDraw`/`cat_popupMenuOpen`, the 0x004exxxx band, 38 functions / 19,100 bytes): 0x004eb940 (2,267 bytes), 0x004eac30, 0x004ecb30, 0x004eb3e0, 0x004edc70, 0x004ef1c0, 0x004ee5f0 (caller `cat_mrReleaseLeft`), 0x004ecfe0 (callees `cat_uiFireSiegeCannon`, `MakeShipsFriendlyShips`), 0x004eb0b0, 0x004ed550.
   - **TitanInterface/WON C++ inside the game region** (0x0058xxxx-0x005axxxx): 0x00598c80 (`TitanFactoryServer`, caller `TitanInterface_HandleDirMultiEntityReply`), 0x0058edb0 (`login.ks`), 0x005a33d0, 0x005a0e30, 0x005a2460, 0x005a2ae0, 0x0058c110. The 0x0059xxxx band is unchanged since round 2 (55 functions / 12,527 bytes), so no round-3 set covered it. This cluster also has the 77 unparsed `__thiscall` prototypes (gap 3).
   - **Renderer selection / video options** (strings `3dfxvgl.dll`, `opengl32.dll`, `Emergence`; callers `opOptionsAcceptHelper`, `opTimerExpired`, `opCountdownNo`): 0x00560ba0, 0x00561920, 0x00561570, 0x00561260. Also 0x00556990 (`DDERR_*` strings) and 0x00588b80 (`nopal freed %dMB of textures`, callers `gameStart`, `mainRestoreSoftware`).
   - **AI** (callers `aifProcessShipBuildRequests`, `aidDefenseManager`): 0x004045b0, 0x004f3380, 0x00403130 (`msdp`), 0x004012a0 (callees `aioCreate*`), 0x00424110 (3,177 bytes, no caller, callees `gunShipFirePower`, `ranRandomFn`).
   - **Mouse/selection UI** (callers `cat_mrClickRelease`, `cat_mrReleaseRight`, `mrObjectClick`): 0x004ab840 and 0x004ac050 (`hyperspace`, `ShipListClick`, `end mission`), 0x00513a00, 0x00565f30.
   - **Ship code:** 0x005c1010 (`cat_rammingFrigateCollide`), 0x005c89a0 (`WorkerSpecialTarget`), 0x005c6830 (`SentinelHouseKeep`), 0x005433d0 (`cat_univShipDeferredUpdate`), 0x005af940 (`processLaunchShipToDo`).
   - **Rendering helpers:** 0x0056b890 and 0x0056cc70 (no named callers or callees: pure math or rasteriser, guess), 0x005c60a0, 0x0048fd10, 0x005750c0 (`rgluLookAt`/`rgluPerspective`, caller `rndMainViewRenderFunction`), 0x00585760 (11 callers, texture load, `crc32Compute`).
   - **Densest bands:** 0x005axxxx (82 functions), 0x005bxxxx (77), 0x005cxxxx (74), 0x004fxxxx and 0x0051xxxx (61 each).
2. **186 deferred round-3 names** (`ghidra/proposals/r3/reconcile_deferred_functions.json`). These are audited names whose address is code but not a function entry: NIS.c 59, MultiplayerGame.c 38, Options.c 24, cat-new 23, MultiplayerLANGame.c 16, utility.c 16, and 10 others. Fix: the B1 mechanism (CreateFunctions.py with the snapshot guard of FixFunctionBodies.py), then apply the names. This needs no new naming work and closes most of the NIS.c and Options.c gaps below.
3. **Prototypes.**
   - 77 `__thiscall` TitanInterface prototypes failed to parse. Fix: rewrite them without the explicit `this` (or as `__fastcall`/`__stdcall` equivalents that match the RET imm), or teach `ImportSymbolsJson.degraded_prototype` to drop it, then re-apply.
   - 2,030 real-named functions have a symbol entry with no prototype, including 655 of round 3's new names.
   - 342 are still applied degraded.
   - The 2,085 mission functions have none. They are uniform kas2c functions; one checked `void f(void)` prototype per Init/Watch kind would make the 612,691 bytes of mission bodies decompile cleanly.
4. **Library leftovers.** 54 of the 111 A1-created `FUN_*` in the WON/Magick range are still unnamed (5,863 bytes) and still have no class. 183 `FUN_*` carry an audited class but no name (62 `lib:msvc-stl` at 0x005a0170-0x005a4500, plus WON/CryptoPP template slots and ICF-ambiguous bodies). Giving them `lib_<class>_<addr>` class-only names would move them out of the unnamed tier at no naming cost.
5. **HW1 files with large gaps** (defs approx. / matched):
   - AIMoves.c 270/162
   - Options.c 154/53 (24 deferred)
   - SaveGame.c 180/83
   - MultiplayerGame.c 233/137 (38 deferred)
   - Dock.c 113/18
   - NIS.c 159/80 (59 deferred)
   - SinglePlayer.c 107/42
   - mainrgn.c 93/29
   - TitanInterface.cpp 165/112
   - texreg.c 76/24
   - main.c 69/21
   - Stats.c 61/13
   - CommandLayer.c 144/97
   - UIControls.c 84/38
   - TaskBar.c 49/6

   Game and ship files with 0 matches, leaving out the Mac/SDL/stub files: Ships/CloakGenerator.c, DDDFrigate.c, ResearchShip.c, P2MultiBeamFrigate.c, LightCorvette.c, P2AdvanceSwarmer.c, Probe.c, SensorArray.c, P2FuelPod.c, P2Mothership.c, P3StandardShip.c, Drone.c, ResourceController.c; Game/ProfileTimers.c, StubTransform.c, Hash.c, Types.c, Tracking.c and Matrix-mult.c. Most of these ships are HW1 race ships that Cataclysm may not ship (guess).
6. **Mission region: done except for small details.** 97.4% of the region is inside a body. The rest is 1,911 padding runs and 36 inline switch tables, which are data. Four call sites still lack their CALL reference: `kasJump` in `Watch_Mission13_Resourcers`, `kasfGetDockHost` in `Watch_Mission16_Corvettes_Dock`, `kasfTeamCount` in `Init_Mission17_Workers` and `kasfCameraFocus` in `Watch_Tutorial1_SpeechEvents_WaitMessage5`. In 38 body-fix rows, 56 bytes stayed undisassembled (`docs/ghidra-round3-prep.md`). Typing the 36 switch tables as pointer arrays would finish it.
7. **Quality checks before more naming.**
   - 51 low-confidence new names in round 3 (and 75 cat-new / 10 hw1 low from round 2) should be sampled again against r3 bodies.
   - Verify the ShipSelect.c attribution (R3.4).
   - The 21 cross-set conflicts show that sets disagree on neighbouring addresses ("one slot off", e.g. 0x0041BB80), so a call-order check of adjacent HW1 names would catch more off-by-one names.
8. **Carried over:**
   - `qwen-naming.json` is still unaudited (STATE.md "Next steps" 1). Its queue was built before round 3, so it must be re-filtered against `cataclysm_gog_r3` first.
   - `universe.world[]` offset +0x3D9B0 vs CET's +0x3DA30 is still unverified.
   - 2 thread-start functions keep `lpStartAddress_*` names.
   - A Ghidra pass that runs analysis on mission code needs the FixFunctionBodies snapshot guard. The no-return analyzer runs even when switched off (`docs/ghidra-round3-prep.md`).

## Round 4

Generated 2026-10-09 from the re-export `ghidra/exports/cataclysm_gog_r4/`, made after `ghidra/symbols_round4.json` (3,526 symbols) was applied to project `ghidra/projects/cataclysm_gog`. Round 4 ran on branch `r2-apply-coverage` (worktree `.claude/worktrees/r2-apply`), like round 3. It had two Ghidra passes on the same project:
- **C1** created the 186 deferred round-3 functions, typed the 46 inline switch tables of the mission region, repaired 44 off-cut holes and restored the 4 missing CALL references (export `cataclysm_gog_r4a`; details in `docs/ghidra-round4-prep.md`).
- **Apply** is this pass.

The numbers come from `tools/binary_map_coverage.py`. Round 4 changed one thing in it: the class-only tier now also matches the titan-libs placeholders `hw1_<File>_<8 hex>` and `cat_new_<8 hex>` (a class, no real name). No export before r4 holds such a name. Re-run with the round-3 command, the script still gives R3.2 exactly (8,235 real-named functions, game region 83.52%, 3,946 HW1 matches in 190 files). Round-4 command:

```
python3 -I tools/binary_map_coverage.py --export ghidra/exports/cataclysm_gog_r4 \
  --history "round 1=ghidra/exports/cataclysm_gog_r1" "round 2=ghidra/exports/cataclysm_gog_r2" \
  --before ghidra/exports/cataclysm_gog_r3 --before-label "round 3" \
  --mid ghidra/exports/cataclysm_gog_r4a --mid-label "after C1" --label "round 4" --new-tag r4 \
  --symbols ghidra/symbols_round1.json ghidra/symbols_round2.json ghidra/symbols_round3.json ghidra/symbols_round4.json \
  --props ghidra/proposals/r2/*.verified.json ghidra/proposals/r3/*.verified.json ghidra/proposals/r4/*.verified.json \
  --out /tmp/bmap_r4
```

The prototype rows of R4.2 use the round-3 method: per real-named function, the latest apply-report status over `cataclysm_gog_r1`, `_r2`, `_r3` and `_r4/cet_apply_report.csv`, where a later name-only entry does not erase an earlier prototype. Run against r3 with the r1-r3 reports, this method gives R3.2's 3,208 / 342 / 79 / 2,030 / 2,576 exactly.

### R4.1 What round 4 did

| pass | what | result |
|---|---|---|
| C1 deferred functions + mission region (`CreateFunctions.py`, `FinishMissionRegion.py`, `ghidra/create_functions_r4.json`) | create and name the 186 deferred round-3 functions; type the inline switch tables; repair the off-cut code behind the 56 undisassembled bytes; check the 4 missing CALL references; guarded analysis (FixFunctionBodies snapshot guard) | **186 / 186** created and named (25,963 bytes), 3 more functions found by the analysis (all unnamed, outside the mission region). 14,490 -> 14,679 functions. **46 switch tables** typed in the 36 runs (42 `pointer[N]`, 4 `byte[N]`), 9 `COMPUTED_JUMP` references added. **44 off-cut holes** repaired in 38 functions; undisassembled bytes in mission bodies 56 -> 0; missing CALL references 4 -> 0. Mission bodies 612,687 bytes (97.4%) |
| producers + audits | 5 sets in `ghidra/proposals/r4/`, each written per subsystem: ui-render (popup menu, main view, renderer/video options, mouse UI, sentinel fields, Mimic menu), ai-ships-sim (AI build/defense/attack, ship code, selection filters, Acolyte/ACV/module code), titan-libs (TitanInterface/WON C++ 0x0058b400-0x005a4870, the 54 WON/Magick `FUN_*`, class-only names for library leftovers), prototypes-r4 (no renames: the 77 `__thiscall` fixes, a `void __cdecl NAME(void)` for each of the 2,085 mission functions, degraded and missing hw1 prototypes), quality-recheck (low-confidence names of rounds 2-3, the ShipSelect.c attribution, call-order check of HW1 files) | verified sizes: ui-render 195 (audit precision 1.0), ai-ships-sim 330 (0.933), titan-libs 270 (0.931), prototypes-r4 2,875 (0.933), quality-recheck 0 of 4 (0.25: 3 of its 4 renames were judged wrong; the 4th, 0x00419E80, was judged correct and taken by reconcile) |
| reconcile (`ghidra/proposals/r4/reconcile_report.md`) | new names and prototypes on top of rounds 1-3 + C1; signatures checked against `objdump` RET immediates and caller stack cleanup | 3,526 symbols: **663 new function names**, **1 rename**, **0 removals**, 0 data names, **2,862 prototype-only entries** (2,785 `__cdecl`, 77 `__fastcall`; 2,085 of them mission functions). 17 cross-set conflicts resolved, 48 titan-libs placeholders superseded by a real name, **54 deferred to round 5** (address not a function entry), 0 dropped, 0 CET anchors touched. New names by confidence: high 171, medium 312, low 180; by class: hw1 173, cat-new 337, lib 89, won 64. 433 new names carry a checked signature, 9 signatures were dropped, 13 prototypes were dropped |
| **apply** (this pass) | `tools/ghidra_headless.sh --symbols ghidra/symbols_round4.json --export-name cataclysm_gog_r4 --post-script "DropStaleComments.py drop=0x00419e80:aiuGenerateRandomVector" original/game/cataclysm.exe cataclysm_gog` (reuse mode, `-noanalysis`, ImportSymbolsJson phase=all, ExportAnalysis with decompile), detached with `setsid nohup`, log `ghidra/exports/r4_headless.log` | 2026-10-09 18:38, rc=0, wall 58 s, no project-lock retry, 0 `ERROR`/Traceback/lock lines in the log (the 46 decompiler `WARN` lines are the same count as in the r3 and r4a logs). **3,526 of 3,526 symbols applied**, all `function`: 664 `renamed` (663 `FUN_*` + the 1 rename), 2,862 `same-name`. 0 `kept-user-name`, 0 `inside:`, 0 `error`. Prototypes: **3,295 applied, 0 degraded, 0 unparsed**, 231 entries had none. 3,486 of the 3,526 entries export a different signature than in r4a. Decompile: ok 14,590, thunk 86, failed 3 (the same 3 as rounds 1-3), error 0 |

**Apply failures: 0.** Every round-4 name is the primary name at its address in `functions.csv` (checked for all 3,526 entries). The 77 TitanInterface prototypes that failed to parse in round 3 were rewritten by prototypes-r4 as `__fastcall(pThis, edx_unused, ...)` with RET = 4*(n-2) checked, and all 77 applied.

**Removals and the stale comment.** No set upheld a removal, so no reset post-script was needed. The one rename (0x00419E80 `aiuGenerateRandomVector` -> `cat_MoveReachedDestinationRadius`, a 3-argument squared-distance test, not HW1's `vector aiuGenerateRandomVector(void)`) left the old `[CET] aiuGenerateRandomVector ...` line in the plate comment, because ImportSymbolsJson only appends comments. The new post-script `ghidra/scripts/DropStaleComments.py` (args `drop=<addr>:<oldname>,...`) removed that line in the same pass (log: `0x00419e80 PLATE: removed 1 line(s)`). The function now exports as `bool32 cat_MoveReachedDestinationRadius(Ship * ship, vector * destination, real32 radius)`.

Spot checks: `Init_Mission01_Objectives` 0x005d6fe0 exports as `void Init_Mission01_Objectives(void)` (all 2,085 mission functions now carry an applied prototype); `cat_popupMenuBuildSelectionItems` 0x004eb940 (R3.5 #2), `battlePingEvaluate` 0x00424110 (R3.5 #1), `mainRestoreRender` 0x00560ba0 and `mainLoadParticularRGL` 0x00561920 (the `3dfxvgl.dll`/`opengl32.dll` functions), `TitanInterface_HandleChannelList` 0x00598c80 (`TitanFactoryServer`).

### R4.2 Before / after

| measure | round 1 | round 2 | round 3 | after C1 (`r4a`) | round 4 |
|---|---:|---:|---:|---:|---:|
| functions in Ghidra | 11,194 | 14,384 | 14,490 | 14,679 | **14,679** |
| function bytes | 2,194,228 | 2,679,918 | 2,999,412 | 3,026,177 | 3,026,177 |
| real-named functions | 3,867 | 7,055 | 8,235 | 8,421 | **9,049** |
| real-named bytes (% of function bytes) | 1,019,110 (46.4%) | 1,599,230 (59.7%) | 2,216,810 (73.9%) | 2,242,769 (74.1%) | **2,407,059 (79.5%)** |
| not `FUN_*` (real + class-only) functions / % bytes | 8,992 / 70.2% | 12,150 / 78.9% | 13,330 / 91.1% | 13,516 / 91.1% | **14,179 / 96.7%** |
| unnamed `FUN_*` functions / bytes | 2,202 / 653,625 | 2,234 / 565,795 | 1,160 / 267,709 | 1,163 / 268,515 | **500 / 99,950** |
| game region 0x401000-0x5c9fff: functions | 5,079 | 6,073 | 6,178 | 6,367 | 6,367 |
| game region: real-named % of function bytes | 55.3% | 65.7% | 83.5% | 83.7% | **93.1%** |
| game region: `FUN_*` functions / bytes | 2,160 / 653,583 | 2,081 / 549,585 | 1,064 / 261,804 | 1,067 / 262,610 | **458 / 99,908** |
| game region: class-only functions | 133 | 130 | 130 | 130 | 155 |
| mission region 0x5d6fe0-0x670a0f: functions (all real-named) | 0 | 2,085 | 2,085 | 2,085 | 2,085 |
| mission region: bytes inside a function body | 0 | 320,296 (50.9%) | 612,691 (97.4%) | 612,687 (97.4%) | **612,687 of 629,296 (97.4%)**; the rest is padding and the 46 switch tables C1 typed as data |
| mission functions with an applied prototype | - | 0 | 0 | 0 | **2,085** |
| HW1 functions matched (`hw1:*` class + CET) | 2,261 | 3,053 in 180 files | 3,946 in 190 files (verbatim 3,726) | - | **4,245 in 192 files (verbatim 4,022)** |
| HW1 files with 0 matches (files that define something) | 69 | 42 | 32 | - | 31 |
| data labels applied by the pipeline | 34 | 1,010 | 1,015 | 1,015 | 1,015 |
| real-named functions with a prototype from a symbol file (full / degraded / unparsed) | - | 889 / 1,007 / 2 | 3,208 / 342 / 79 | - | **6,503 / 46 / 2** |
| real-named functions with no prototype in their symbol entry / not in any symbol file | - | 2,686 / 2,471 | 2,030 / 2,576 | - | 1,821 / 677 |
| R3.5 top-60 still unnamed | - | - | 60 | 60 | **14** |
| deferred names waiting for a function entry | - | 105 | 186 | 0 | 54 |

Movement from round 3 to round 4: 626 round-3 `FUN_*` became real names and 35 became class-only names (21 `lib_*`, 14 titan-libs placeholders `hw1_<File>_<addr>`/`cat_new_<addr>`). 188 new functions are real-named: C1's 186 and 2 of the 3 functions its analysis found. 1 new function is still `FUN_*`. 1 real name was replaced (0x00419E80). No round-3 function disappeared.

The 2 unparsed prototypes are round-1 entries (`univHideShipFromSpheres` 0x00547c20, `univReallyDeleteThisDerelictRightNow` 0x00549700). "Not in any symbol file" is now mostly C1's 186 functions (named by `create_functions_r4.json`, no prototype) plus CET, FID and import names; the 2,085 mission functions left this bucket.

ShipSelect.c drops from 83 to 58 matches. The 25 functions that left it were `FUN_*` in round 3 whose only class came from an audited round-3 proposal (`hw1:ShipSelect.c`, no name). Round 4 named them as Cataclysm-only selection filters (`cat_selMakeShipsScuttleable` 0x00511ce0, `cat_selMakeShipsNotLinkedACVs`, `cat_selMakeShipsLeechCapable`, ...), class cat-new. That settles R3.6 gap 7's ShipSelect.c question. AIUtilities.c drops by 1 (the rename above).

### R4.3 Per region, section and class

| region | round 3 functions | round 4 functions | real | class-only | `FUN_*` | `FUN_*` bytes | real % of function bytes, round 4 (round 3) |
|---|---:|---:|---:|---:|---:|---:|---|
| game C/C++ 0x401000-0x5ca0ff | 6,178 | 6,367 | 5,754 | 155 | 458 | 99,908 | 93.1% (83.5%) |
| IJG libjpeg 0x5ca100-0x5d6fdf | 102 | 102 | 41 | 61 | 0 | 0 | 23.8% (23.8%) |
| kas2c mission code 0x5d6fe0-0x670a0f | 2,085 | 2,085 | 2,085 | 0 | 0 | 0 | 100.0% (100.0%) |
| WSOCK32 thunks + static CRT 0x670a10-0x69412f | 596 | 596 | 257 | 339 | 0 | 0 | 41.1% (41.1%) |
| WON/Titan + AdApi + Magick 0x694130-0x705c9f | 2,216 | 2,216 | 740 | 1,476 | 0 | 0 | 34.6% (33.3%) |
| Dinkumware C++ std lib 0x705ca0-0x71cccf | 708 | 708 | 124 | 584 | 0 | 0 | 9.3% (9.3%) |
| late CRT 0x71ccd0-0x71f62f | 48 | 48 | 28 | 20 | 0 | 0 | 58.3% (58.3%) |
| text$x EH funclets 0x71f630-0x729fff | 2,343 | 2,343 | 0 | 2,343 | 0 | 0 | 0.0% (0.0%) |
| .cms_t stub + CRT 0xc55000-0xc68fff | 172 | 172 | 20 | 152 | 0 | 0 | 5.1% (5.1%) |

| section | functions | bytes | real | class-only | `FUN_*` | `FUN_*` bytes |
|---|---:|---:|---:|---:|---:|---:|
| .text | 14,465 | 2,989,777 | 9,029 | 4,978 | 458 | 99,908 |
| .cms_t | 172 | 36,358 | 20 | 152 | 0 | 0 |
| .data | 42 | 42 | 0 | 0 | 42 | 42 |

Per class (round 3 -> round 4 functions: hw1:* 3,904 -> 4,203, cat-new 3,145 -> 3,479, won 3,290 -> 3,304, lib:msvc-stl 62 -> 86 with 74 now real-named, unnamed `FUN_*` 977 -> 495). The class of a function comes from the latest symbol round, then the audited r2, r3 and r4 sets, then CET. Confidence counts are those of the class source.

| class | functions | bytes | real | class-only | high | medium | low | lowest - highest entry |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| hw1:* (all files) | 4,203 | 1,173,335 | 4,196 | 7 | 2,359 | 1,749 | 95 | 0x00401000 - 0x005c79b0 |
| cat-new | 3,479 | 973,130 | 3,472 | 7 | 2,434 | 791 | 254 | 0x00405210 - 0x006708a0 |
| won | 3,304 | 330,301 | 727 | 2,577 | 3,101 | 194 | 9 | 0x00551dc0 - 0x007276f1 |
| crt | 1,913 | 277,069 | 335 | 1,578 | 1,583 | 330 | 0 | 0x00553420 - 0x00c67ca0 |
| unknown, lib=magick | 346 | 78,087 | 15 | 331 | 276 | 70 | 0 | 0x006e3de0 - 0x00727bd0 |
| unknown, lib=ijg | 61 | 19,307 | 0 | 61 | 28 | 33 | 0 | 0x005ca410 - 0x005d6d10 |
| lib:msvc-stl | 86 | 16,745 | 74 | 11 | 0 | 62 | 24 | 0x0058bf20 - 0x005a4500 |
| CET (hw1) | 42 | 14,724 | 42 | 0 | 0 | 0 | 0 | 0x00420880 - 0x005a7a70 |
| unknown, lib=cms | 12 | 7,621 | 0 | 12 | 0 | 12 | 0 | 0x00c55000 - 0x00c60540 |
| CET (other) | 10 | 6,862 | 10 | 0 | 0 | 0 | 0 | 0x00431510 - 0x005bc900 |
| lib:libjpeg | 19 | 4,360 | 19 | 0 | 15 | 0 | 4 | 0x005c9cd0 - 0x005cf9d0 |
| unknown | 32 | 4,252 | 28 | 0 | 23 | 6 | 3 | 0x0043453b - 0x006fe9a0 |
| lib:imagemagick | 16 | 3,635 | 13 | 3 | 2 | 11 | 3 | 0x006e41c0 - 0x006f1710 |
| rgl | 3 | 3,586 | 3 | 0 | 1 | 2 | 0 | 0x00559a80 - 0x0055dd30 |
| bink | 8 | 2,503 | 8 | 0 | 6 | 2 | 0 | 0x00551220 - 0x00551d40 |
| other named | 650 | 12,723 | 107 | 543 | 0 | 0 | 0 | 0x00425ca0 - 0x00727750 |
| unnamed FUN_* | 495 | 97,937 | 0 | 0 | 0 | 0 | 0 | 0x00421610 - 0x00892114 |

### R4.4 Per HW1 source file

Same columns as R3.4, with the round-3 count. Totals: 4,245 matched functions in 192 files (verbatim 4,022). These files define about 6,765 functions; HomeworldSDL `src/` has 218 .c/.cpp basenames with about 7,176 definitions.

| HW1 file | path under src/ | round 3 | round 4 | of which CET | defs (approx.) | matched / defs | verbatim | bytes |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| KASFunc.c | Game/KASFunc.c | 293 | 293 | 0 | 313 | 93.6% | 293 | 16,460 |
| ETG.c | Game/ETG.c | 205 | 205 | 4 | 244 | 84.0% | 205 | 33,365 |
| MultiplayerGame.c | Game/MultiplayerGame.c | 137 | 175 | 3 | 233 | 75.1% | 174 | 37,576 |
| AIMoves.c | Game/AIMoves.c | 162 | 162 | 0 | 270 | 60.0% | 162 | 28,181 |
| TitanInterface.cpp | SDL/TitanInterface.cpp | 112 | 144 | 0 | 165 | 87.3% | 27 | 67,286 |
| NIS.c | Game/NIS.c | 80 | 139 | 0 | 159 | 87.4% | 138 | 34,289 |
| Particle.c | Game/Particle.c | 132 | 132 | 0 | 175 | 75.4% | 132 | 28,809 |
| UnivUpdate.c | Game/UnivUpdate.c | 98 | 98 | 4 | 139 | 70.5% | 97 | 40,315 |
| CommandLayer.c | Game/CommandLayer.c | 97 | 97 | 0 | 144 | 67.4% | 96 | 46,672 |
| AIUtilities.c | Game/AIUtilities.c | 84 | 83 | 0 | 113 | 73.5% | 82 | 24,517 |
| SaveGame.c | Game/SaveGame.c | 83 | 83 | 1 | 180 | 46.1% | 83 | 24,606 |
| Options.c | Game/Options.c | 53 | 79 | 1 | 154 | 51.3% | 79 | 14,225 |
| utility.c | SDL/utility.c | 58 | 74 | 0 | 98 | 75.5% | 74 | 18,772 |
| ShipSelect.c | Game/ShipSelect.c | 83 | 58 | 0 | 68 | 85.3% | 52 | 5,581 |
| Universe.c | Game/Universe.c | 55 | 56 | 2 | 93 | 60.2% | 55 | 21,115 |
| CommandNetwork.c | Game/CommandNetwork.c | 53 | 53 | 0 | 68 | 77.9% | 52 | 15,236 |
| MultiplayerLANGame.c | Game/MultiplayerLANGame.c | 37 | 53 | 0 | 77 | 68.8% | 52 | 12,950 |
| ConsMgr.c | Game/ConsMgr.c | 48 | 48 | 0 | 87 | 55.2% | 48 | 17,091 |
| Tutor.c | Game/Tutor.c | 46 | 46 | 0 | 80 | 57.5% | 46 | 12,969 |
| FlightMan.c | Game/FlightMan.c | 45 | 45 | 0 | 63 | 71.4% | 45 | 13,987 |
| SinglePlayer.c | Game/SinglePlayer.c | 42 | 45 | 2 | 107 | 42.1% | 45 | 16,323 |
| texreg.c | SDL/texreg.c | 24 | 44 | 1 | 76 | 57.9% | 42 | 20,096 |
| AITeam.c | Game/AITeam.c | 42 | 43 | 0 | 80 | 53.8% | 43 | 6,332 |
| StatScript.c | Game/StatScript.c | 42 | 42 | 4 | 60 | 70.0% | 42 | 9,637 |
| FEFlow.c | Game/FEFlow.c | 38 | 38 | 1 | 71 | 53.5% | 35 | 11,322 |
| UIControls.c | Game/UIControls.c | 38 | 38 | 3 | 84 | 45.2% | 38 | 10,059 |
| ColPick.c | Game/ColPick.c | 35 | 35 | 0 | 46 | 76.1% | 35 | 8,479 |
| TitanNet.c | Game/TitanNet.c | 35 | 35 | 0 | 39 | 89.7% | 35 | 3,507 |
| Mesh.c | Game/Mesh.c | 33 | 33 | 0 | 47 | 70.2% | 33 | 20,829 |
| SoundEvent.c | Game/SoundEvent.c | 32 | 32 | 0 | 51 | 62.7% | 32 | 17,324 |
| Trails.c | Game/Trails.c | 32 | 32 | 2 | 46 | 69.6% | 32 | 13,427 |
| Formation.c | Game/Formation.c | 31 | 31 | 0 | 47 | 66.0% | 29 | 16,469 |
| Region.c | Game/Region.c | 31 | 31 | 0 | 45 | 68.9% | 30 | 5,265 |
| KAS.c | Game/KAS.c | 30 | 30 | 0 | 44 | 68.2% | 30 | 4,426 |
| render.c | SDL/render.c | 30 | 30 | 0 | 60 | 50.0% | 25 | 14,960 |
| ObjTypes.c | Game/ObjTypes.c | 29 | 29 | 1 | 46 | 63.0% | 29 | 1,480 |
| mainrgn.c | SDL/mainrgn.c | 29 | 29 | 0 | 93 | 31.2% | 29 | 12,675 |
| CameraCommand.c | Game/CameraCommand.c | 28 | 28 | 0 | 40 | 70.0% | 28 | 7,312 |
| FEReg.c | Game/FEReg.c | 28 | 28 | 0 | 35 | 80.0% | 28 | 13,275 |
| Nebulae.c | Game/Nebulae.c | 28 | 28 | 0 | 71 | 39.4% | 28 | 16,132 |
| glcompat.c | (not in HomeworldSDL) | 28 | 28 | 0 | - | - | 0 | 12,438 |
| TradeMgr.c | Game/TradeMgr.c | 27 | 27 | 0 | 41 | 65.9% | 27 | 4,720 |
| main.c | SDL/main.c | 21 | 26 | 1 | 69 | 37.7% | 22 | 12,264 |
| Clouds.c | Game/Clouds.c | 25 | 25 | 0 | 43 | 58.1% | 25 | 14,255 |
| soundlow.c | SDL/soundlow.c | 24 | 24 | 0 | 31 | 77.4% | 24 | 4,628 |
| AIPlayer.c | Game/AIPlayer.c | 23 | 23 | 0 | 34 | 67.6% | 21 | 5,563 |
| Collision.c | Game/Collision.c | 23 | 23 | 0 | 26 | 88.5% | 23 | 14,582 |
| CommandWrap.c | Game/CommandWrap.c | 23 | 23 | 5 | 24 | 95.8% | 23 | 3,522 |
| LevelLoad.c | Game/LevelLoad.c | 23 | 23 | 0 | 47 | 48.9% | 23 | 12,187 |
| prim2d.c | SDL/prim2d.c | 19 | 23 | 0 | 36 | 63.9% | 22 | 6,511 |
| AIOrders.c | Game/AIOrders.c | 22 | 22 | 0 | 30 | 73.3% | 22 | 10,994 |
| Stats.c | Game/Stats.c | 13 | 22 | 0 | 61 | 36.1% | 22 | 4,900 |
| Subtitle.c | Game/Subtitle.c | 22 | 22 | 0 | 26 | 84.6% | 22 | 5,863 |
| ResearchAPI.c | Game/ResearchAPI.c | 10 | 21 | 2 | 32 | 65.6% | 21 | 5,282 |
| font.c | SDL/font.c | 21 | 21 | 0 | 29 | 72.4% | 21 | 5,742 |
| AIShip.c | Game/AIShip.c | 20 | 20 | 0 | 29 | 69.0% | 19 | 11,038 |
| Matrix.c | Game/Matrix.c | 19 | 20 | 0 | 25 | 80.0% | 20 | 2,333 |
| mouse.c | SDL/mouse.c | 20 | 20 | 0 | 29 | 69.0% | 20 | 8,436 |
| AITrack.c | Game/AITrack.c | 19 | 19 | 0 | 24 | 79.2% | 19 | 6,816 |
| Battle.c | Game/Battle.c | 9 | 19 | 0 | 26 | 73.1% | 19 | 6,786 |
| File.c | Game/File.c | 19 | 19 | 0 | 30 | 63.3% | 18 | 6,480 |
| Sensors.c | Game/Sensors.c | 19 | 19 | 0 | 56 | 33.9% | 19 | 8,273 |
| SpeechEvent.c | Game/SpeechEvent.c | 19 | 19 | 1 | 33 | 57.6% | 19 | 7,677 |
| Dock.c | Game/Dock.c | 18 | 18 | 0 | 113 | 15.9% | 18 | 5,139 |
| GravWellGenerator.c | Ships/GravWellGenerator.c | 18 | 18 | 0 | 21 | 85.7% | 18 | 3,298 |
| Ping.c | Game/Ping.c | 18 | 18 | 0 | 25 | 72.0% | 17 | 4,208 |
| ResearchGUI.c | Game/ResearchGUI.c | 18 | 18 | 0 | 47 | 38.3% | 18 | 8,062 |
| BTG.c | Game/BTG.c | 17 | 17 | 0 | 26 | 65.4% | 17 | 5,565 |
| HS.c | Game/HS.c | 4 | 17 | 0 | 28 | 60.7% | 17 | 4,805 |
| NetCheck.c | Game/NetCheck.c | 17 | 17 | 0 | 24 | 70.8% | 17 | 2,398 |
| ResCollect.c | Game/ResCollect.c | 12 | 17 | 0 | 26 | 65.4% | 17 | 3,987 |
| Blobs.c | Game/Blobs.c | 16 | 16 | 0 | 31 | 51.6% | 16 | 7,317 |
| HorseRace.c | Game/HorseRace.c | 16 | 16 | 0 | 29 | 55.2% | 16 | 8,271 |
| Task.c | Game/Task.c | 16 | 16 | 0 | 16 | 100.0% | 14 | 2,756 |
| AIEvents.c | Game/AIEvents.c | 15 | 15 | 0 | 32 | 46.9% | 15 | 2,365 |
| AIHandler.c | Game/AIHandler.c | 15 | 15 | 0 | 25 | 60.0% | 15 | 2,606 |
| LaunchMgr.c | Game/LaunchMgr.c | 15 | 15 | 0 | 38 | 39.5% | 15 | 4,137 |
| Memory.c | Game/Memory.c | 14 | 14 | 0 | 35 | 40.0% | 14 | 4,243 |
| Objectives.c | Game/Objectives.c | 14 | 14 | 0 | 24 | 58.3% | 14 | 2,412 |
| AIDefenseMan.c | Game/AIDefenseMan.c | 4 | 13 | 0 | 23 | 56.5% | 13 | 4,421 |
| AIResourceMan.c | Game/AIResourceMan.c | 13 | 13 | 0 | 22 | 59.1% | 13 | 5,578 |
| AIVar.c | Game/AIVar.c | 3 | 13 | 0 | 18 | 72.2% | 13 | 1,374 |
| GenericInterceptor.c | Ships/GenericInterceptor.c | 13 | 13 | 0 | 17 | 76.5% | 13 | 6,153 |
| LinkedList.c | Game/LinkedList.c | 13 | 13 | 0 | 19 | 68.4% | 13 | 1,351 |
| MeshAnim.c | Game/MeshAnim.c | 13 | 13 | 0 | 21 | 61.9% | 13 | 2,582 |
| MinelayerCorvette.c | Ships/MinelayerCorvette.c | 13 | 13 | 0 | 14 | 92.9% | 13 | 4,139 |
| PiePlate.c | Game/PiePlate.c | 13 | 13 | 0 | 20 | 65.0% | 13 | 6,755 |
| ScenPick.c | Game/ScenPick.c | 11 | 13 | 0 | 20 | 65.0% | 13 | 4,326 |
| Select.c | Game/Select.c | 13 | 13 | 0 | 39 | 33.3% | 13 | 8,328 |
| sstream.c | SDL/sstream.c | 13 | 13 | 0 | 17 | 76.5% | 13 | 3,294 |
| AIAttackMan.c | Game/AIAttackMan.c | 4 | 12 | 0 | 23 | 52.2% | 12 | 5,810 |
| ChannelFSM.c | Game/ChannelFSM.c | 12 | 12 | 0 | 16 | 75.0% | 12 | 868 |
| GameChat.c | Game/GameChat.c | 11 | 12 | 0 | 18 | 66.7% | 12 | 3,760 |
| Key.c | Game/Key.c | 12 | 12 | 0 | 17 | 70.6% | 12 | 920 |
| Timer.c | Game/Timer.c | 12 | 12 | 0 | 17 | 70.6% | 12 | 2,071 |
| Vector.c | Game/Vector.c | 12 | 12 | 0 | 15 | 80.0% | 12 | 1,056 |
| AIFleetMan.c | Game/AIFleetMan.c | 7 | 11 | 0 | 21 | 52.4% | 11 | 5,934 |
| AutoDownloadMap.c | Game/AutoDownloadMap.c | 11 | 11 | 0 | 19 | 57.9% | 11 | 2,109 |
| GamePick.c | Game/GamePick.c | 11 | 11 | 0 | 31 | 35.5% | 11 | 2,383 |
| prim3d.c | SDL/prim3d.c | 11 | 11 | 0 | 18 | 61.1% | 11 | 3,328 |
| Captaincy.c | Game/Captaincy.c | 10 | 10 | 0 | 30 | 33.3% | 10 | 3,364 |
| DefenseFighter.c | Ships/DefenseFighter.c | 10 | 10 | 0 | 19 | 52.6% | 10 | 792 |
| Light.c | Game/Light.c | 10 | 10 | 0 | 15 | 66.7% | 10 | 1,957 |
| Shader.c | Game/Shader.c | 10 | 10 | 0 | 32 | 31.2% | 10 | 3,325 |
| mixfft.c | SDL/mixfft.c | 0 | 10 | 0 | 11 | 90.9% | 10 | 4,916 |
| RepairCorvette.c | Ships/RepairCorvette.c | 9 | 9 | 0 | 15 | 60.0% | 9 | 1,943 |
| BigFile.c | Game/BigFile.c | 8 | 8 | 0 | 35 | 22.9% | 5 | 3,088 |
| Camera.c | Game/Camera.c | 8 | 8 | 0 | 17 | 47.1% | 8 | 2,456 |
| Carrier.c | Ships/Carrier.c | 8 | 8 | 0 | 10 | 80.0% | 8 | 902 |
| Crates.c | Game/Crates.c | 8 | 8 | 0 | 14 | 57.1% | 8 | 5,155 |
| Demo.c | Game/Demo.c | 8 | 8 | 0 | 10 | 80.0% | 8 | 1,625 |
| Mothership.c | Ships/Mothership.c | 8 | 8 | 0 | 18 | 44.4% | 8 | 1,672 |
| PlugScreen.c | Game/PlugScreen.c | 7 | 8 | 0 | 17 | 47.1% | 8 | 3,285 |
| Queue.c | SDL/Queue.c | 8 | 8 | 0 | 8 | 100.0% | 8 | 446 |
| SalCapCorvette.c | Ships/SalCapCorvette.c | 8 | 8 | 0 | 46 | 17.4% | 8 | 227 |
| debugwnd.c | SDL/debugwnd.c | 6 | 8 | 0 | 16 | 50.0% | 8 | 2,730 |
| glcaps.c | (not in HomeworldSDL) | 8 | 8 | 0 | - | - | 0 | 1,472 |
| smixer.c | SDL/smixer.c | 8 | 8 | 0 | 9 | 88.9% | 5 | 3,779 |
| Clipper.c | Game/Clipper.c | 7 | 7 | 0 | 10 | 70.0% | 7 | 4,805 |
| Color.c | Game/Color.c | 7 | 7 | 0 | 11 | 63.6% | 7 | 2,270 |
| Gun.c | Game/Gun.c | 7 | 7 | 0 | 20 | 35.0% | 7 | 6,624 |
| Physics.c | Game/Physics.c | 7 | 7 | 0 | 8 | 87.5% | 7 | 5,646 |
| Alliance.c | Game/Alliance.c | 3 | 6 | 1 | 10 | 60.0% | 6 | 1,715 |
| Damage.c | Game/Damage.c | 6 | 6 | 0 | 15 | 40.0% | 6 | 1,883 |
| Eval.c | Game/Eval.c | 6 | 6 | 0 | 19 | 31.6% | 6 | 3,298 |
| FontReg.c | Game/FontReg.c | 6 | 6 | 0 | 6 | 100.0% | 6 | 935 |
| HeavyCorvette.c | Ships/HeavyCorvette.c | 6 | 6 | 0 | 12 | 50.0% | 6 | 1,232 |
| Randy.c | Game/Randy.c | 6 | 6 | 1 | 12 | 50.0% | 6 | 578 |
| Scroller.c | Game/Scroller.c | 6 | 6 | 0 | 7 | 85.7% | 6 | 530 |
| Tactical.c | Game/Tactical.c | 6 | 6 | 0 | 7 | 85.7% | 6 | 2,229 |
| TaskBar.c | Game/TaskBar.c | 6 | 6 | 0 | 49 | 12.2% | 6 | 1,349 |
| fqeffect.c | SDL/fqeffect.c | 6 | 6 | 1 | 20 | 30.0% | 6 | 557 |
| Attack.c | Game/Attack.c | 5 | 5 | 0 | 7 | 71.4% | 5 | 2,425 |
| MissileDestroyer.c | Ships/MissileDestroyer.c | 5 | 5 | 0 | 6 | 83.3% | 5 | 951 |
| P1MissileCorvette.c | Ships/P1MissileCorvette.c | 5 | 5 | 0 | 6 | 83.3% | 5 | 497 |
| Tactics.c | Game/Tactics.c | 5 | 5 | 0 | 37 | 13.5% | 5 | 385 |
| ddraw.cpp | (not in HomeworldSDL) | 5 | 5 | 0 | - | - | 0 | 660 |
| rinit.cpp | (not in HomeworldSDL) | 5 | 5 | 0 | - | - | 0 | 2,058 |
| AutoLOD.c | Game/AutoLOD.c | 4 | 4 | 0 | 15 | 26.7% | 4 | 195 |
| B-Spline.c | Game/B-Spline.c | 4 | 4 | 0 | 8 | 50.0% | 4 | 1,025 |
| BitIO.c | ThirdParty/LZSS/BitIO.c | 4 | 4 | 0 | 13 | 30.8% | 2 | 226 |
| DefaultShip.c | Ships/DefaultShip.c | 4 | 4 | 0 | 6 | 66.7% | 4 | 399 |
| KeyBindings.c | Game/KeyBindings.c | 3 | 4 | 0 | 14 | 28.6% | 4 | 573 |
| LOD.c | Game/LOD.c | 4 | 4 | 1 | 11 | 36.4% | 4 | 1,146 |
| LagPrint.c | Game/LagPrint.c | 4 | 4 | 0 | 4 | 100.0% | 4 | 263 |
| Md52.cpp | (not in HomeworldSDL) | 0 | 4 | 0 | - | - | 0 | 2,568 |
| ProximitySensor.c | Ships/ProximitySensor.c | 4 | 4 | 0 | 13 | 30.8% | 4 | 816 |
| ShipView.c | Game/ShipView.c | 4 | 4 | 0 | 15 | 26.7% | 4 | 2,421 |
| StringSupport.c | Game/StringSupport.c | 4 | 4 | 0 | 6 | 66.7% | 4 | 778 |
| Teams.c | Game/Teams.c | 4 | 4 | 0 | 8 | 50.0% | 4 | 420 |
| TimeoutTimer.c | SDL/TimeoutTimer.c | 4 | 4 | 0 | 8 | 50.0% | 4 | 233 |
| Titan.c | SDL/Titan.c | 4 | 4 | 0 | 3 | 133.3% | 2 | 327 |
| Volume.c | Game/Volume.c | 4 | 4 | 0 | 4 | 100.0% | 4 | 1,130 |
| Animatic.c | Game/Animatic.c | 3 | 3 | 0 | 17 | 17.6% | 3 | 725 |
| Chatting.c | Game/Chatting.c | 3 | 3 | 0 | 3 | 100.0% | 3 | 306 |
| Clamp.c | Game/Clamp.c | 3 | 3 | 0 | 3 | 100.0% | 3 | 898 |
| DFGFrigate.c | Ships/DFGFrigate.c | 3 | 3 | 0 | 3 | 100.0% | 3 | 998 |
| GenericDefender.c | Ships/GenericDefender.c | 3 | 3 | 0 | 4 | 75.0% | 3 | 1,730 |
| P1IonArrayFrigate.c | Ships/P1IonArrayFrigate.c | 3 | 3 | 0 | 3 | 100.0% | 3 | 267 |
| P1StandardCorvette.c | Ships/P1StandardCorvette.c | 3 | 3 | 0 | 4 | 75.0% | 3 | 380 |
| Tweak.c | Game/Tweak.c | 3 | 3 | 0 | 3 | 100.0% | 3 | 205 |
| fqcodec.c | SDL/fqcodec.c | 3 | 3 | 0 | 5 | 60.0% | 3 | 1,372 |
| fquant.c | SDL/fquant.c | 3 | 3 | 0 | 4 | 75.0% | 3 | 1,270 |
| rglu.c | SDL/rglu.c | 3 | 3 | 0 | 10 | 30.0% | 3 | 976 |
| AdvanceSupportFrigate.c | Ships/AdvanceSupportFrigate.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 141 |
| BMP.c | Game/BMP.c | 2 | 2 | 0 | 2 | 100.0% | 2 | 227 |
| CRC32.c | ThirdParty/CRC/CRC32.c | 2 | 2 | 0 | 2 | 100.0% | 2 | 104 |
| Debug.c | Game/Debug.c | 2 | 2 | 0 | 9 | 22.2% | 2 | 183 |
| Globals.c | Game/Globals.c | 2 | 2 | 0 | 2 | 100.0% | 1 | 334 |
| InfoOverlay.c | Game/InfoOverlay.c | 2 | 2 | 0 | 10 | 20.0% | 2 | 651 |
| MEX.c | Game/MEX.c | 2 | 2 | 0 | 17 | 11.8% | 2 | 496 |
| MadLinkIn.c | Game/MadLinkIn.c | 2 | 2 | 0 | 16 | 12.5% | 2 | 820 |
| MultiGunCorvette.c | Ships/MultiGunCorvette.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 103 |
| NavLights.c | Game/NavLights.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 634 |
| P1Mothership.c | Ships/P1Mothership.c | 2 | 2 | 0 | 3 | 66.7% | 2 | 101 |
| ResourceCollector.c | Ships/ResourceCollector.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 85 |
| SoundEventPlay.c | Game/SoundEventPlay.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 3,054 |
| SoundEventStop.c | Game/SoundEventStop.c | 2 | 2 | 0 | 2 | 100.0% | 2 | 136 |
| Transformer.c | Game/Transformer.c | 2 | 2 | 0 | 1 | 200.0% | 0 | 374 |
| Twiddle.c | Game/Twiddle.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 55 |
| Undo.c | Game/Undo.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 62 |
| fastmath.c | (not in HomeworldSDL) | 2 | 2 | 0 | - | - | 0 | 227 |
| rinit.c | SDL/rinit.c | 2 | 2 | 2 | 14 | 14.3% | 2 | 653 |
| Bounties.c | Game/Bounties.c | 1 | 1 | 0 | 6 | 16.7% | 1 | 190 |
| FloatingCity.c | Ships/FloatingCity.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 51 |
| HeavyCruiser.c | Ships/HeavyCruiser.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 115 |
| IonCannonFrigate.c | Ships/IonCannonFrigate.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 99 |
| P2Swarmer.c | Ships/P2Swarmer.c | 1 | 1 | 0 | 4 | 25.0% | 0 | 41 |
| StandardDestroyer.c | Ships/StandardDestroyer.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 121 |
| StandardFrigate.c | Ships/StandardFrigate.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 68 |
| Star3d.c | Game/Star3d.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 86 |
| screenshot.c | SDL/screenshot.c | 1 | 1 | 0 | 3 | 33.3% | 0 | 710 |

**HW1 files with 0 matches** (31 files that define at least one function; defs approx., most first):

SDL/utility_mac.c (98), SDL/TitanInterfaceC.c (43), SDL/TitanInterfaceC.stub.c (36), Game/File_Mac.c (30), SDL/ClientCDKey.cpp (25), SDL/avi.c (18), Ships/CloakGenerator.c (17), Ships/DDDFrigate.c (17), SDL/NetworkInterface.c (15), Ships/ResearchShip.c (15), SDL/wave.c (14), Game/ProfileTimers.c (12), Game/StubTransform.c (9), Game/Hash.c (6), Game/Types.c (6), Ships/P2MultiBeamFrigate.c (6), Game/Tracking.c (4), Ships/LightCorvette.c (4), Ships/P2AdvanceSwarmer.c (4), Ships/Probe.c (4), Ships/SensorArray.c (4), SDL/TitanPacketMsg.cpp (3), Ships/P2FuelPod.c (3), Ships/P2Mothership.c (3), Ships/P3StandardShip.c (3), ThirdParty/LZSS/LZSS.c (3), SDL/dct.c (2), SDL/win32glue.c (2), Ships/Drone.c (2), Ships/ResourceController.c (2), Game/Matrix-mult.c (1)

Largest gains: NIS.c 80 -> 139, MultiplayerGame.c 137 -> 175, TitanInterface.cpp 112 -> 144, Options.c 53 -> 79, texreg.c 24 -> 44, utility.c 58 -> 74, MultiplayerLANGame.c 37 -> 53, HS.c 4 -> 17, ResearchAPI.c 10 -> 21, mixfft.c 0 -> 10, Battle.c 9 -> 19, AIVar.c 3 -> 13, Stats.c 13 -> 22, AIDefenseMan.c 4 -> 13, AIAttackMan.c 4 -> 12. Files new in round 4: SDL/mixfft.c and Md52.cpp. Most of the NIS.c, MultiplayerGame.c, Options.c, MultiplayerLANGame.c and utility.c gains are C1's 186 deferred round-3 names.

### R4.5 Next-round targets: the 60 largest unnamed functions

Same method as R3.5. `in round 3` = `yes` when the function was already unnamed in round 3 (all 60 were). 46 of R3.5's 60 are now named, among them `battlePingEvaluate` 0x00424110, `cat_popupMenuBuildSelectionItems` 0x004eb940, `Transform2` 0x0056b890, `aifBuildRequestedShips` 0x004045b0, `cat_ioClick` 0x004ab840, `cat_gameStatsDraw` 0x004896b0, `twiddleTransf` 0x0056cc70, `TitanInterface_HandleChannelList` 0x00598c80, `mainRestoreRender` 0x00560ba0, `rmResearchTechForShip` 0x004f3380, `allianceSetAlliance` 0x00420a20 and `aidMothershipDefense` 0x00403130.

| # | address | size | calls in | strings | named callers | named callees | in round 3 |
|---:|---|---:|---:|---|---|---|---|
| 1 | 0x004c3490 | 1,687 | 1 | `atrox `; `Matrox` | cat_mgAdBannerDraw | __ftol, _free, _malloc | yes |
| 2 | 0x0052ba90 | 1,354 | 2 | - | gameStart, levelPreInit | colRGBToHSV | yes |
| 3 | 0x00534300 | 1,128 | 4 | - | cmClose, lmCloseIfOpen, lmLaunchCapableShipDied | regChildAlloc, regDrawFunctionSet, regRegionDelete | yes |
| 4 | 0x00480650 | 1,080 | 1 | - | - | __ftol, vecNormalize | yes |
| 5 | 0x0047fd80 | 1,054 | 1 | - | - | listAddNode, listRemoveNode | yes |
| 6 | 0x00488e70 | 1,029 | 1 | - | cat_gameStatsBegin | fontHeight, fontMakeCurrent, fontWidth | yes |
| 7 | 0x0049b1b0 | 951 | 1 | - | keyInit | - | yes |
| 8 | 0x004b7880 | 925 | 1 | `Rct0`; `Col0` | precalcStaticHeader | - | yes |
| 9 | 0x00542cb0 | 893 | 1 | - | univUpdateAllPosVelBullets | fsqrt, vecNormalize | yes |
| 10 | 0x004802d0 | 882 | 1 | - | fowRender | colBlend | yes |
| 11 | 0x00507bc0 | 845 | 3 | - | - | allianceIsShipAlly, cat_acvEMPChargeFraction, cat_beastWeaponChargeFraction | yes |
| 12 | 0x0049bf30 | 799 | 1 | - | - | - | yes |
| 13 | 0x00576010 | 797 | 4 | - | cat_dreadnoughtChargeEffect, cat_nomadMoonBeamEffect | fsqrt, ranRandomFn, vecNormalize | yes |
| 14 | 0x005750c0 | 781 | 1 | - | rndMainViewRenderFunction | hmatMakeHMatFromMat, rgluLookAt, rgluPerspective | yes |
| 15 | 0x0048a2f0 | 741 | 3 | - | gunShootGunsAtMultipleTargets, gunShootGunsAtTarget | getShipAndItsCommand, ranRandomFn, soundEventBurstStop | yes |
| 16 | 0x004e7240 | 731 | 0 | - | - | primCircleOutlineZ, primLine3, rndTextureEnable | yes |
| 17 | 0x00508900 | 725 | 1 | - | cat_mrSelectRectDraw | primRectOutline2, univSpaceObjInRenderList | yes |
| 18 | 0x0047f850 | 676 | 3 | - | cat_saveLoadPlayerFOW, fowClear, fowCreate | fowCellMarkDirty, listInit, memAllocFunction | yes |
| 19 | 0x00571611 | 671 | 0 | - | - | - | yes |
| 20 | 0x0050dfe0 | 664 | 5 | - | cat_meshObjectRenderLitImmediate, cat_meshObjectRenderLitTransformed, cat_meshObjectRenderWarped | - | yes |
| 21 | 0x004605b0 | 663 | 1 | - | - | etgEffectCreate, etgFrequencyExceeded, matCreateCoordSysFromHeading | yes |
| 22 | 0x00460850 | 662 | 1 | - | cat_univShipDeferredUpdate | cat_timerExpired, matMultiplyVecByMat | yes |
| 23 | 0x005298e0 | 646 | 1 | - | - | MakeShipsOnlyFollowConstraints, cat_selMakeShipsSameOwnerAsFirst, cat_selSelectionAddShip | yes |
| 24 | 0x00532df0 | 643 | 1 | `Tut_NextLesson_Off`; `Tut_NextLesson_On` | kasfTutShowImages | regChildAlloc, regDrawFunctionSet, regRegionDelete | yes |
| 25 | 0x00528190 | 634 | 1 | `AsteroidBreakInfo`; `NUMBER_OF_ASTEROID_PIECES` | universeStaticInit | _sscanf, fileClose, fileLineRead | yes |
| 26 | 0x004271d0 | 628 | 1 | - | bobListUpdate | allianceIsShipAlly, battleChatterFleetAttempt, smStrchr | yes |
| 27 | 0x0042ca10 | 624 | 1 | - | cat_mcRender | cat_hmatMultiplyHMatByVec, hmatMultiplyHMatByHMat | yes |
| 28 | 0x004a3a10 | 616 | 2 | `; MissionMan` | gameStart, tutInitTutorial | _strncmp, cat_fowCreatePlayerGrids, ccFocusOnFleet | yes |
| 29 | 0x0050d560 | 601 | 1 | - | - | - | yes |
| 30 | 0x005086a0 | 595 | 0 | - | - | cat_moAltPrintOptionsActive, cat_rmTechSetHas, fontCurrentGet | yes |
| 31 | 0x00508be0 | 591 | 17 | - | aiaSendRecon, aimProcessControlResources, aimProcessFlankAttack | cat_mimicGetSelectRadius | yes |
| 32 | 0x0049b590 | 585 | 4 | - | cat_kbBindingToString, cat_kbCheckBindingConflict, cat_kbCommandKeyString | _sprintf | yes |
| 33 | 0x0043a6b0 | 583 | 3 | - | cat_meshObjectLineIntersect, collCheckMissileShipColl | vecNormalize | yes |
| 34 | 0x00523d00 | 583 | 1 | - | star3dInit | matMakeRotAboutX, matMakeRotAboutY, matMakeRotAboutZ | yes |
| 35 | 0x0050d7c0 | 572 | 2 | - | shSpecularColour | - | yes |
| 36 | 0x00489470 | 567 | 2 | - | cat_gameStatsDraw, cat_moObjectivesFrameDraw | cat_overlayIconDraw, fontHeight, fontMakeCurrent | yes |
| 37 | 0x0054e410 | 565 | 1 | - | kbListWindowCB | cat_kbCommandsForKey | yes |
| 38 | 0x00529cb0 | 553 | 1 | - | - | MakeShipsAttackCapable, MakeShipsOnlyFollowConstraints, cat_selMakeShipsSameOwnerAsFirst | yes |
| 39 | 0x00569030 | 550 | 1 | - | toAllShipsDraw | fsqrt, hmatMakeHMatFromMat, primCircleOutline3 | yes |
| 40 | 0x0043a110 | 549 | 1 | - | - | - | yes |
| 41 | 0x00572010 | 531 | 1 | - | cat_smSensorRangeBlobsDraw | - | yes |
| 42 | 0x005732f0 | 528 | 3 | - | - | EXTERNAL:ADVAPI32.DLL::RegCreateKeyExA, EXTERNAL:ADVAPI32.DLL::RegOpenKeyExA, _memmove | yes |
| 43 | 0x004a03e0 | 515 | 3 | - | cat_scriptSetMeteorStormCB, scriptSetResourcesCB, univCheckRegrowResources | AddResourceToVolumeAtPosition, matMakeRotAboutY, matMakeRotAboutZ | yes |
| 44 | 0x00575c90 | 514 | 2 | - | svShipViewRender, univBulletCollidedWithTarget | - | yes |
| 45 | 0x0052b890 | 510 | 1 | - | - | colHLSToRGB, colRGBToHLS | yes |
| 46 | 0x004361c0 | 496 | 1 | - | CloakedFighterHouseKeep | ranRandomFn, shipHasJustCloaked, soundEventPlay | yes |
| 47 | 0x005556e0 | 496 | 1 | - | fqDecOver | __chkstk, fft | yes |
| 48 | 0x00488c80 | 494 | 1 | - | cat_gameStatsBegin | __ftol, cat_rmTechSetHas | yes |
| 49 | 0x0043df90 | 490 | 2 | - | cpBaseColor, cpHueSaturationDraw | colHSVToRGB, memAllocFunction, memFree | yes |
| 50 | 0x0048cd40 | 487 | 3 | `Hobbes call Mike upgraded weapon does no` | GenericInterceptorCanFire, gunShootGunsAtMultipleTargets, gunShootGunsAtTarget | aishipGetTrajectoryWithVelPrediction, fsqrt, matMultiplyMatByVec | yes |
| 51 | 0x005a1140 | 483 | 5 | - | TitanInterface_HandleRoutingRegister, TitanInterface_RequestCreateGame, TitanInterface_RequestDeleteGame | operator_delete, operator_new | yes |
| 52 | 0x00507740 | 480 | 1 | - | - | cat_acolyteCanLink, cat_acolyteCanLinkFire, fontMakeCurrent | yes |
| 53 | 0x0044f290 | 471 | 1 | - | cat_clWrapSpecialTarget | HWEnqueue, LockQueue, UnLockQueue | yes |
| 54 | 0x0044f490 | 471 | 1 | - | cat_clWrapSpecialTarget | HWEnqueue, LockQueue, UnLockQueue | yes |
| 55 | 0x005c8e30 | 471 | 0 | - | - | etgEffectCreate, etgFrequencyExceeded, fsqrt | yes |
| 56 | 0x00507f10 | 470 | 0 | `333?` | - | cat_salvageGetSalvagerInfo | yes |
| 57 | 0x0043e4c0 | 458 | 3 | - | cpBaseColor, cpPreviewImageDraw, cpPreviousColor | cpScalarFromCurveGet, trBufferColorRGB, trPalettedTextureCreate | yes |
| 58 | 0x00570300 | 456 | 1 | - | cat_cmSUBarDraw | rndTextureEnable | yes |
| 59 | 0x00449790 | 450 | 2 | - | mrHarvestResources, rescollectFindNearestResource | ChangeSingleShipToCollectResource, FindNearestShipToDockAt, FreeLastOrder | yes |
| 60 | 0x005080f0 | 449 | 0 | `333?` | - | cat_salvageGetSalvagerInfo | yes |

Remaining `FUN_*` by 64 KB band (count / bytes): 0x0042xxxx 15/3,086, 0x0043xxxx 20/5,467, 0x0044xxxx 12/2,719, 0x0045xxxx 15/3,458, 0x0046xxxx 15/3,088, 0x0047xxxx 6/2,385, 0x0048xxxx 21/8,138, 0x0049xxxx 24/4,718, 0x004axxxx 33/6,227, 0x004bxxxx 10/2,784, 0x004cxxxx 1/1,687, 0x004exxxx 10/1,373, 0x004fxxxx 6/989, 0x0050xxxx 48/12,161, 0x0051xxxx 2/319, 0x0052xxxx 44/9,451, 0x0053xxxx 36/7,733, 0x0054xxxx 22/4,236, 0x0055xxxx 38/5,599, 0x0056xxxx 8/1,179, 0x0057xxxx 37/8,782, 0x0058xxxx 7/567, 0x0059xxxx 1/182, 0x005axxxx 4/549, 0x005bxxxx 15/1,560, 0x005cxxxx 8/1,471, 0x0089xxxx 42/42.

### R4.6 Ranked gaps for round 5

Ranked by bytes that can be won and by how directly the fix follows from data already on disk. Cluster attributions rest on the callers, callees and strings in R4.5; the ones marked "guess" rest on nothing else.

1. **The game region still holds 458 `FUN_*` (99,908 bytes).** 58 have no direct caller, and 68 are 32 bytes or smaller. Two bands were not touched by round 4 at all: 0x0050xxxx (48 functions / 12,161 bytes) and 0x0053xxxx (36 / 7,733), both identical to round 3. Others: 0x0052xxxx 44 / 9,451, 0x0057xxxx 37 / 8,782, 0x0048xxxx 21 / 8,138, 0x004axxxx 33 / 6,227, 0x0055xxxx 38 / 5,599, 0x0043xxxx 20 / 5,467 (unchanged). By cluster, from R4.5:
   - **Selection / mouse UI and Cataclysm special ships, 0x00507xxx-0x00508xxx:** 0x00508be0 (17 callers, among them `aiaSendRecon`, `aimProcessControlResources`; callee `cat_mimicGetSelectRadius`), 0x00508900 (caller `cat_mrSelectRectDraw`), 0x005086a0, 0x00507bc0 (callees `cat_acvEMPChargeFraction`, `cat_beastWeaponChargeFraction`, `cat_bigGunChargeFraction`), 0x00507740 (`cat_acolyteCanLink*`), 0x00507f10 and 0x005080f0 (`cat_salvageGetSalvagerInfo`); 0x005298e0 and 0x00529cb0 (`MakeShipsOnlyFollowConstraints`, `cat_selMakeShipsSameOwnerAsFirst`).
   - **Fog of war, 0x0047f-0x00480:** 0x00480650, 0x0047fd80, 0x004802d0 (caller `fowRender`), 0x0047f850 (callers `fowClear`, `fowCreate`, `cat_saveLoadPlayerFOW`).
   - **Region-based screens:** 0x00534300 (callers `cmClose`, `lmCloseIfOpen`, `lmLaunchCapableShipDied`), 0x00532df0 (`Tut_NextLesson_On/Off`, caller `kasfTutShowImages`); game stats 0x00488e70, 0x00488c80 (caller `cat_gameStatsBegin`), 0x00489470; key bindings 0x0049b1b0 (caller `keyInit`), 0x0049b590, 0x0054e410 (`kbListWindowCB`).
   - **Rendering:** 0x005750c0 (`rgluLookAt`/`rgluPerspective`, caller `rndMainViewRenderFunction`), 0x00576010 (callers `cat_dreadnoughtChargeEffect`, `cat_nomadMoonBeamEffect`), 0x0050dfe0 (5 `cat_meshObjectRender*` callers), 0x0050d7c0 (`shSpecularColour`), 0x0050d560, 0x00575c90, 0x00572010, 0x00569030, 0x00570300, 0x004e7240 (no caller).
   - **Colour:** 0x0052ba90 (callers `gameStart`, `levelPreInit`; `colRGBToHSV`), 0x0052b890 (`colHLSToRGB`), colour picker 0x0043df90 and 0x0043e4c0 (`cp*` callers).
   - **Combat and ship code:** 0x0048a2f0 and 0x0048cd40 (`gunShootGunsAt*` callers; string `Hobbes call Mike upgraded weapon...`), 0x00542cb0 (caller `univUpdateAllPosVelBullets`), 0x0043a6b0 (collision), 0x004361c0 (`CloakedFighterHouseKeep`), 0x00460850 (`cat_univShipDeferredUpdate`), 0x004605b0 and 0x005c8e30 (ETG effects), 0x0044f290 and 0x0044f490 (`cat_clWrapSpecialTarget`, command queue), 0x00449790 (resource collecting).
   - **Load / init:** 0x004c3490 (`Matrox`, caller `cat_mgAdBannerDraw`, 1,687 bytes, the largest left), 0x004b7880 (`Rct0`/`Col0`, `precalcStaticHeader`), 0x00528190 (`AsteroidBreakInfo`), 0x00523d00 (`star3dInit`), 0x004a3a10 (`; MissionMan`, callers `gameStart`, `tutInitTutorial`), 0x005732f0 (registry), 0x005556e0 (`fft`, caller `fqDecOver`: likely the rest of mixfft.c, guess).
2. **54 deferred round-4 names** (`ghidra/proposals/r4/reconcile_deferred_functions.json`): audited names whose address is code but not a function entry. Options.c 10, UIControls.c 9, AIHandler.c 5, AIPlayer.c 5, cat-new 4, SaveGame.c 3, Stats.c 3, and 15 others (e.g. `aiaPriorityShipsConstraints` 0x004011d0, a constraints callback passed as a code label; `aihHarassFiringSingleShipHandler` 0x00405f70, entry 1 of the AI handler table 0x74a3a4). Fix: the C1 mechanism (CreateFunctions.py as a post-script with `analyze=0`, analysis inside the FixFunctionBodies snapshot guard), then apply the names.
3. **Prototypes.** 1,821 real-named functions have a symbol entry without a prototype, among them 196 of round 4's new names (231 round-4 entries had no signature; 35 of those are class-only names). 677 real-named functions are in no symbol file; 186 of them are C1's functions, and 107 of those have an audited signature in round 3's `ghidra/proposals/r3/reconcile_deferred_functions.json` that can be applied as prototype-only entries after the round-3 binary check. 46 prototypes are still applied degraded; 2 round-1 prototypes do not parse.
4. **Low-confidence names.** Round 4 added 180 low names (of 663), on top of round 3's 51 and round 2's 85. quality-recheck re-checked 120 round-2/3 low names and proposed only 4 changes, 3 of which the audit rejected, so most low names held up; the round-4 lows (136 of the 180 are `cat_*`; by band 0x0051xxxx 35, 0x005cxxxx 30, 0x005axxxx 27, 0x005bxxxx 24, e.g. the Acolyte/ACV/module and sentinel-field families) have not been re-sampled.
5. **HW1 files with large gaps** (defs approx. / matched):
   - AIMoves.c 270/162
   - SaveGame.c 180/83
   - Dock.c 113/18
   - Options.c 154/79 (10 deferred)
   - mainrgn.c 93/29
   - SinglePlayer.c 107/45
   - MultiplayerGame.c 233/175
   - CommandLayer.c 144/97
   - UIControls.c 84/38 (9 deferred)
   - main.c 69/26
   - TaskBar.c 49/6
   - Nebulae.c 71/28
   - Stats.c 61/22
   - SalCapCorvette.c 46/8
   - Sensors.c 56/19

   Game and ship files with 0 matches are the same as in R3.6 (mixfft.c left the list): mostly HW1 race ships that Cataclysm may not ship (guess).
6. **Library and class-only tiers.** The WON/Titan + Magick range has 0 `FUN_*` left (54 in round 3). 5,130 functions carry only a class-only name; 155 are in the game region: 72 Ghidra `Catch@` funclets, 56 `lib_crt_*` (e.g. `lib_crt_00553420`), 11 `lib_msvc_stl_*`, the 14 placeholders, 1 `thunk_FUN_` and 1 `switchD`. The 14 titan-libs placeholders (`hw1_ShipSelect_00511ea0` ... `cat_new_005166b0`) are real functions with a known class and no name yet. The 42 one-byte `FUN_*` "functions" in `.data` at 0x0089xxxx are not code (guess: analysis artefacts); deleting them would make the unnamed tier exactly the 458 game functions.
7. **Mission region: done.** C1 typed the 46 switch tables, repaired the 44 off-cut holes and restored the 4 CALL references; every mission function has a name and an applied `void (void)` prototype. What is outside a body is alignment padding and the typed tables.
8. **Carried over:**
   - `qwen-naming.json` is still unaudited (STATE.md "Next steps" 1); it must be re-filtered against `cataclysm_gog_r4` before an audit, since rounds 3-4 named most of its queue.
   - `universe.world[]` offset +0x3D9B0 vs CET's +0x3DA30 is still unverified.
   - 2 thread-start functions keep `lpStartAddress_*` names.
   - Any Ghidra pass that runs analysis on mission code needs the FixFunctionBodies snapshot guard, and a walk that clears flow overrides must keep the 41 `CALL_RETURN` overrides on tail-call `JMP`s (`docs/ghidra-round4-prep.md`).

## Round 5

Generated 2026-10-09 from the re-export `ghidra/exports/cataclysm_gog_r5/`, made after `ghidra/symbols_round5.json` (1,106 symbols) was applied to project `ghidra/projects/cataclysm_gog`. Round 5 ran on branch `r2-apply-coverage` (worktree `.claude/worktrees/r2-apply`), like rounds 3 and 4. It had two Ghidra passes on the same project:
- **D1** created the 54 deferred round-4 functions and removed the 42 one-byte `.data` "functions" (export `cataclysm_gog_r5a`; details in `docs/ghidra-round5-prep.md`).
- **Apply** is this pass. It also merged the 2 off-cuts and created the 15 functions that reconcile deferred.

The numbers come from `tools/binary_map_coverage.py`, unchanged since round 4. Round-5 command (absolute paths, because the script resolves relative paths against the main checkout, which does not hold the round-5 files):

```
python3 -I tools/binary_map_coverage.py --export <X>/cataclysm_gog_r5 \
  --history "round 1=<X>/cataclysm_gog_r1" "round 2=<X>/cataclysm_gog_r2" "round 3=<X>/cataclysm_gog_r3" \
  --before <X>/cataclysm_gog_r4 --before-label "round 4" \
  --mid <X>/cataclysm_gog_r5a --mid-label "after D1" --label "round 5" --new-tag r5 \
  --symbols <W>/ghidra/symbols_round1.json <W>/ghidra/symbols_round2.json <W>/ghidra/symbols_round3.json \
            <W>/ghidra/symbols_round4.json <W>/ghidra/symbols_round5.json \
  --props <W>/ghidra/proposals/r2/*.verified.json <W>/ghidra/proposals/r3/*.verified.json \
          <W>/ghidra/proposals/r4/*.verified.json <W>/ghidra/proposals/r5/*.verified.json \
  --out /tmp/bmap_r5
```

Here `<X>` is the main checkout's `ghidra/exports` and `<W>` is the worktree. The prototype rows of R5.2 use the round-3/4 method: per real-named function, the latest apply-report status over the `cet_apply_report.csv` of `cataclysm_gog_r1` ... `_r5`, where a later name-only entry does not erase an earlier prototype. Run against r4 with the r1-r4 reports, the method gives R4.2's 6,503 / 46 / 2 / 1,821 / 677 exactly.

### R5.1 What round 5 did

| pass | what | result |
|---|---|---|
| D1 deferred functions + `.data` artefacts (`CreateFunctions.py mark=R5`, new `RemoveDataArtefacts.py`, `FinishMissionRegion.py tables=0`) | create and name the 54 deferred round-4 functions; remove the 42 one-byte `.data` functions at 0x00891a1c-0x00892114 (unbound `.cms_t` IAT slots that Ghidra read as code pointers); guarded analysis of the new bodies | **54 / 54** created and named (21,410 bytes), **42 / 42** artefacts removed, 1 function found by the analysis (`FUN_00528580`, later named `strSetCurKeyboard`). 14,679 -> 14,692 functions. Details in `docs/ghidra-round5-prep.md` |
| producers + audits | 5 sets in `ghidra/proposals/r5/`, each written per subsystem: ui-screens (selection/special-ship UI, region screens, game stats, key bindings, colour picker, tutorial buttons), render-combat (renderer, mesh/shader, fog of war, guns/bullets/collision, AIMoves/Dock/Nebulae ranges), bands-files (every other game-region `FUN_*` by 64 KB band, the 14 titan-libs placeholders, the 3 round-4 analysis-found functions), prototypes-r5 (no renames: the 1,821 real names with no prototype, C1's 186 functions, the 46 degraded and 2 unparsed entries, D1's 54 functions; first use of the ETG function table at 0x0087980c for ETG callback signatures), lowconf-r4 (re-check of the 180 round-4 low names) | verified sizes: ui-screens 157 (audit precision 0.967), render-combat 142 (0.933), bands-files 317 (0.933), lowconf-r4 38 (0.967; 141 of the 180 names confirmed unchanged), prototypes-r5 631 (0.900) |
| reconcile (`ghidra/proposals/r5/reconcile_report.md`) | names and prototypes on top of rounds 1-4 + D1; signatures checked against `objdump` RET immediates and caller stack cleanup | 1,106 symbols: **457 new function names**, **25 renames** (8 real names that lowconf-r4 or ui-screens showed wrong, 17 class-only placeholders), **18 confidence raises**, **9 data names**, **597 prototype-only entries**; a top-level `removals` list with **2 off-cuts**; **15 names deferred** (address not a function entry); 68 cross-set conflicts resolved, 0 names dropped, 0 CET anchors touched. New names by confidence: high 140, medium 124, low 193; by class: hw1 130, cat-new 292, won 26, lib 9. 34 audit-flagged int-for-pointer prototypes dropped |
| **apply** (this pass) | `tools/ghidra_headless.sh --symbols ghidra/symbols_round5.json --export-name cataclysm_gog_r5` (reuse mode, `-noanalysis`, ImportSymbolsJson phase=all, ExportAnalysis with decompile), with three post-scripts in this order: **`MergeOffcuts.py`** (new) for the 2 removals, **`CreateFunctions.py`** for the 15 deferred names (`ghidra/create_functions_r5b.json`, `analyze=0 returning= region= tag=R5_created mark=R5`, as in D1), `DropStaleComments.py` for the 25 renames. Detached with `setsid nohup`, wrapper with the D1 project-lock retry, log `ghidra/exports/r5_headless.log` | 2026-10-09 19:47:14-19:48:06, rc=0, wall 52 s, first attempt (no lock retry), 0 `ERROR`/Traceback/lock lines. **1,106 of 1,106 symbols applied**: 1,097 `function` (482 `renamed` = 457 `FUN_*` + 25 renames; 615 `same-name` = 597 prototype-only + 18 raises) and 9 `label`. 0 `kept-user-name`, 0 `inside:`, 0 `error`. Prototypes: **831 applied, 0 degraded, 0 unparsed**; 266 entries had none. 1,080 of the 1,097 function entries export a different signature than in r5a. **2 / 2 off-cuts merged**, **15 / 15 deferred functions created and named**, 25 stale plate lines removed. Decompile: ok 14,618, thunk 86, failed 1, error 0 |

**Apply failures: 0.** Every round-5 function name is the primary name at its address in `functions.csv`, and every data name is a label at its address in `symbols.csv` (checked for all 1,106 entries). The one decompile failure, `lib_crt_00671da2`, also failed in rounds 1-4; round 4's other two failures were `.data` artefacts that D1 removed. The log has 7 decompiler `WARN` lines: 2 "Unable to read bytes" (0x00465360, 0x004f7c20) and 5 p-code errors in the `.cms_t` stub.

**Removals: the new post-script `ghidra/scripts/MergeOffcuts.py`.** Round 4 had no removal, so round 5 needed a new mechanism. ImportSymbolsJson cannot express "no function here", so reconcile put the two off-cuts in a top-level `removals` list. MergeOffcuts reads that list (`json=`) and checks four things for each entry:
- a function starts at the address and still has its default name (`FUN_0043453b`, `FUN_00571611`);
- the parent named in the entry starts at `parent_address`;
- no CALL reference reaches the address;
- at least one jump into the address comes from the parent's body.

When all four hold, it removes the function object only (code, labels and references stay), then re-grows the parent with `CreateFunctionCmd.fixupFunctionBody`. If the new body did not cover the old parent body and the off-cut, it would set the union explicitly; neither case needed that. Report: `ghidra/merge_offcuts_r5.report.csv`.

| off-cut | size | parent | parent size before -> after | jump from the parent | decompile |
|---|---:|---|---|---|---|
| 0x0043453b `FUN_0043453b` | 165 | `recievedChatPacketCB` 0x00434500 (CET anchor; name and prototype untouched) | 59 -> 224 | 1 (`jle` at 0x0043451f), 0 calls | ok |
| 0x00571611 `FUN_00571611` | 671 | `primBeveledRectSolid` 0x005715e0 | 49 -> 720 | 1 (`je` at 0x005715e7), 0 calls | ok |

R4.5 listed 0x00571611 as #19 of the largest unnamed functions. It is now the GL fallback branch inside `primBeveledRectSolid`. Not merged: `FUN_005549aa`, the third off-cut reconcile noted (`lib_crt_00554930` jumps to it). It now carries the audited class-only name `lib_crt_005549aa`, and a later body-fix pass should merge it the same way.

**The 15 deferred names.** Reconcile deferred them because their addresses were code but not function entries:
- `cpValueProcess` 0x0043e690 and `cpHueSaturationProcess` 0x0043e8f0;
- `selStatusDraw3` 0x00508480 and `selStatusDraw4` 0x00508590;
- 11 tutorial button process/draw callbacks at 0x00532750-0x00533080.

The task asked for one pass, so the apply pass created them instead of leaving them for a round 6, with D1's CreateFunctions settings:
- no analysis;
- the bodies come from CreateFunctionCmd's own flow, 2,953 bytes in total and 41-635 bytes each;
- every one carries the `[R5]` plate and the `R5_created` tag.

The pre-check against r5a found none of the 15 inside a function and none of the names taken. CreateFunctions reported 15 `created`, 15 `named`. Their audited signatures (in `ghidra/proposals/r5/reconcile_deferred_functions.json`) are **not** applied yet. The export shows Ghidra's guesses, e.g. `undefined selStatusDraw3()`.

**Rehearsal.** The whole pass first ran on a copy of the project in `/tmp/r5apply/proj` with `--no-decompile`. It gave the same apply statuses, the same MergeOffcuts sizes (224 and 720), 15 / 15 created and the same 25 comment drops. The copy was deleted before the real pass. No backup of the project was left behind.

**Renames and stale comments.** DropStaleComments removed one `[CET] <old name> ...` plate line at each of the 25 renamed addresses (log: 25 x `PLATE: removed 1 line(s)`). The 8 renames of real names are listed in the reconcile report. For example:
- `cat_rmShipTypeToModuleSlot` -> `cat_rmBeastPrintIndexToShipType` 0x004f4790;
- `cat_shipTypeCanDockFor` -> `cat_shipClassCanUseSlipgate` 0x00513800;
- `std__multimap_Profile2Reply__lower_bound` -> `..._find` 0x005a0bb0;
- the twin tutorial getters: `tutNextButtonClicked` moved to 0x00532a80, the function `kasfTutNextButtonClicked` jumps to, and 0x00532a90 became `cat_tutNextTextButtonClicked`.

Spot checks in the export:
- `cat_mgAdBannerRender` 0x004c3490 (R4.5 #1, the `Matrox` function) and `teTeamColorsSet` 0x0052ba90 (R4.5 #2);
- `strSetCurKeyboard` 0x00528580 (found by D1) exports as `void strSetCurKeyboard(void)`;
- `cat_hsGateRectangle` 0x0048f8b0 now carries the audit-fixed five-argument prototype;
- `animLoadNISScript` 0x00421610 exports as `void * animLoadNISScript(char * scriptname)`.

### R5.2 Before / after

| measure | round 1 | round 2 | round 3 | round 4 | after D1 (`r5a`) | round 5 |
|---|---:|---:|---:|---:|---:|---:|
| functions in Ghidra | 11,194 | 14,384 | 14,490 | 14,679 | 14,692 | **14,705** |
| function bytes | 2,194,228 | 2,679,918 | 2,999,412 | 3,026,177 | 3,047,647 | 3,050,600 |
| real-named functions | 3,867 | 7,055 | 8,235 | 9,049 | 9,103 | **9,590** |
| real-named bytes (% of function bytes) | 1,019,110 (46.4%) | 1,599,230 (59.7%) | 2,216,810 (73.9%) | 2,407,059 (79.5%) | 2,428,469 (79.7%) | **2,533,080 (83.0%)** |
| not `FUN_*` (real + class-only) functions / % bytes | 8,992 / 70.2% | 12,150 / 78.9% | 13,330 / 91.1% | 14,179 / 96.7% | 14,233 / 96.7% | **14,705 / 100%** |
| unnamed `FUN_*` functions / bytes | 2,202 / 653,625 | 2,234 / 565,795 | 1,160 / 267,709 | 500 / 99,950 | 459 / 100,010 | **0 / 0** |
| game region 0x401000-0x5c9fff: functions | 5,079 | 6,073 | 6,178 | 6,367 | 6,422 | 6,435 |
| game region: real-named % of function bytes | 55.3% | 65.7% | 83.5% | 93.1% | 93.2% | **99.1%** |
| game region: real-named % of all region bytes (1,871,872) | 44.1% | 57.6% | 74.5% | 84.4% | 85.5% | **91.1%** |
| game region: bytes inside some function body | 79.7% | 87.7% | 89.1% | 90.6% | 91.7% | 91.9% |
| game region: `FUN_*` functions / bytes | 2,160 / 653,583 | 2,081 / 549,585 | 1,064 / 261,804 | 458 / 99,908 | 459 / 100,010 | **0 / 0** |
| game region: class-only functions / bytes | 133 | 130 | 130 | 155 / 16,617 | 155 / 16,617 | 140 / 14,969 |
| mission region 0x5d6fe0-0x670a0f | - | 2,085 named, bodies 50.9% | 2,085, 97.4% | 2,085, 97.4%, all with a prototype | same | **same** (612,687 of 629,296 bytes in a body; the rest is padding and the 46 typed switch tables) |
| HW1 functions matched (`hw1:*` class + CET) | 2,261 | 3,053 in 180 files | 3,946 in 190 files (verbatim 3,726) | 4,245 in 192 files (verbatim 4,022) | - | **4,426 in 196 files (verbatim 4,207)** |
| HW1 files with 0 matches (files that define something) | 69 | 42 | 32 | 31 | - | **28** |
| data labels applied by the pipeline | 34 | 1,010 | 1,015 | 1,015 | 1,015 | 1,024 |
| real-named functions with a prototype from a symbol file (full / degraded / unparsed) | - | 889 / 1,007 / 2 | 3,208 / 342 / 79 | 6,503 / 46 / 2 | - | **7,309 / 15 / 0** |
| real-named functions with no prototype in their symbol entry / not in any symbol file | - | 2,686 / 2,471 | 2,030 / 2,576 | 1,821 / 677 | - | **1,752 / 514** (game region: 650 / 410) |
| R4.5 top-60 still unnamed | - | - | - | 60 | 60 | **0** (59 named, e.g. 0x005556e0 `idct`; 0x00571611 merged into its parent) |
| deferred names waiting for a function entry | - | 105 | 186 | 54 | 0 | **0** |

Movement from round 4 to round 5:
- **Round-4 `FUN_*`:** 453 became real names and 3 became class-only names (`lib_crt_005549aa`, `lib_msvc_static_init_00574240`, `lib_msvc_static_init_00574340`).
- **Class-only names:** 18 became real names: the 17 placeholders reconcile renamed, plus the Ghidra thunk name `thunk_FUN_00425c90` at 0x00425ca0, now `bobResetProperties`.
- **New functions:** 70 are real-named: D1's 54, the 15 deferred and `strSetCurKeyboard`.
- **Real names replaced:** 8.
- **Round-4 functions gone:** 44, the 42 `.data` artefacts (D1) and the 2 merged off-cuts.

**No `FUN_*` function is left anywhere in the program.** The class-only tier in the game region is now:
- 72 `Catch@` exception funclets (2,749 bytes);
- 57 `lib_crt_*` (10,538 bytes);
- 9 `lib_msvc_*` (1,573 bytes);
- 1 `switchD`;
- 1 placeholder, `cat_new_00515930` (13 bytes, the big-gun blast getter both audits declined to name).

The prototype gains come from prototypes-r5 (596 entries) and the 235 checked signatures on new names. "Not in any symbol file" fell from 677 to 514 because C1's 186 functions got prototype entries. The 1,752 real names whose entry has no prototype are mostly library code that prototypes-r5 did not attempt, because there is no HW1 declaration for it. Of round 4's 1,821, those were won 727, crt 332 and lib 105.

ShipSelect.c drops from 58 to 52 matches: 6 of its `hw1_ShipSelect_<addr>` placeholders were named as Cataclysm-only selection filters. The 4 files new in round 5 are Game/StubTransform.c, SDL/dct.c, ThirdParty/LZSS/LZSS.c and bink.c (not in HomeworldSDL).

### R5.3 Per region, section and class

The class of a function comes from the latest symbol round, then the audited r2-r5 sets, then CET. Confidence counts are those of the class source. Over all classes: high 9,685, medium 3,682, low 636 (the remaining functions have no confidence: library signatures, CET, Ghidra names).

#### Per address region

| region | round 4 functions | round 5 functions | real | class-only | `FUN_*` | `FUN_*` bytes | real % of function bytes, round 5 (round 4) |
|---|---:|---:|---:|---:|---:|---:|---|
| game C/C++ 0x401000-0x5ca0ff | 6,367 | 6,435 | 6,295 | 140 | 0 | 0 | 99.1% (93.1%) |
| IJG libjpeg 0x5ca100-0x5d6fdf | 102 | 102 | 41 | 61 | 0 | 0 | 23.8% (23.8%) |
| kas2c mission code 0x5d6fe0-0x670a0f | 2,085 | 2,085 | 2,085 | 0 | 0 | 0 | 100.0% (100.0%) |
| WSOCK32 thunks + static CRT 0x670a10-0x69412f | 596 | 596 | 257 | 339 | 0 | 0 | 41.1% (41.1%) |
| WON/Titan + AdApi + Magick 0x694130-0x705c9f | 2,216 | 2,216 | 740 | 1,476 | 0 | 0 | 34.6% (34.6%) |
| Dinkumware C++ std lib 0x705ca0-0x71cccf | 708 | 708 | 124 | 584 | 0 | 0 | 9.3% (9.3%) |
| late CRT 0x71ccd0-0x71f62f | 48 | 48 | 28 | 20 | 0 | 0 | 58.3% (58.3%) |
| text$x EH funclets 0x71f630-0x729fff | 2,343 | 2,343 | 0 | 2,343 | 0 | 0 | 0.0% (0.0%) |
| .cms_t stub + CRT 0xc55000-0xc68fff | 172 | 172 | 20 | 152 | 0 | 0 | 5.1% (5.1%) |

#### Per section

| section | functions | bytes | real | class-only | `FUN_*` | `FUN_*` bytes |
|---|---:|---:|---:|---:|---:|---:|
| .text | 14,533 | 3,014,242 | 9,570 | 4,963 | 0 | 0 |
| .cms_t | 172 | 36,358 | 20 | 152 | 0 | 0 |

#### Per class

| class | functions | bytes | real | class-only | high | medium | low | lowest - highest entry |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| hw1:* (all files) | 4,384 | 1,228,260 | 4,384 | 0 | 2,232 | 2,020 | 132 | 0x00401000 - 0x005c79b0 |
| cat-new | 3,789 | 1,036,252 | 3,788 | 1 | 2,410 | 921 | 458 | 0x00405210 - 0x006708a0 |
| won | 3,330 | 335,147 | 753 | 2,577 | 3,109 | 208 | 13 | 0x00551dc0 - 0x007276f1 |
| crt | 1,913 | 277,069 | 335 | 1,578 | 1,583 | 330 | 0 | 0x00553420 - 0x00c67ca0 |
| unknown, lib=magick | 346 | 78,087 | 15 | 331 | 276 | 70 | 0 | 0x006e3de0 - 0x00727bd0 |
| unknown, lib=ijg | 61 | 19,307 | 0 | 61 | 28 | 33 | 0 | 0x005ca410 - 0x005d6d10 |
| lib:msvc-stl | 85 | 16,262 | 78 | 7 | 1 | 66 | 18 | 0x0058bf20 - 0x005a4500 |
| CET (hw1) | 42 | 14,724 | 42 | 0 | 0 | 0 | 0 | 0x00420880 - 0x005a7a70 |
| unknown, lib=cms | 12 | 7,621 | 0 | 12 | 0 | 12 | 0 | 0x00c55000 - 0x00c60540 |
| CET (other) | 10 | 6,862 | 10 | 0 | 0 | 0 | 0 | 0x00431510 - 0x005bc900 |
| lib:libjpeg | 19 | 4,360 | 19 | 0 | 15 | 0 | 4 | 0x005c9cd0 - 0x005cf9d0 |
| lib:imagemagick | 16 | 3,635 | 13 | 3 | 2 | 11 | 3 | 0x006e41c0 - 0x006f1710 |
| rgl | 3 | 3,586 | 3 | 0 | 1 | 2 | 0 | 0x00559a80 - 0x0055dd30 |
| unknown | 28 | 2,722 | 28 | 0 | 22 | 6 | 0 | 0x0049ae30 - 0x006fe9a0 |
| bink | 8 | 2,503 | 8 | 0 | 6 | 2 | 0 | 0x00551220 - 0x00551d40 |
| lib:msvc_stl | 7 | 1,218 | 6 | 1 | 0 | 1 | 6 | 0x00553820 - 0x005a1140 |
| lib:msvc | 2 | 262 | 0 | 2 | 0 | 0 | 2 | 0x00574240 - 0x00574340 |
| other named | 650 | 12,723 | 108 | 542 | 0 | 0 | 0 | 0x00425ca0 - 0x00727750 |

### R5.4 Per HW1 source file

Same columns as R4.4, with the round-4 count. Totals: 4,426 matched functions in 196 files (verbatim 4,207). These files define about 6,779 functions; HomeworldSDL `src/` has 218 .c/.cpp basenames with about 7,176 definitions.

Largest gains:
- Tutor.c 46 -> 63
- Select.c 13 -> 29
- MadLinkIn.c 2 -> 13
- Shader.c 10 -> 20
- Options.c 79 -> 89
- LevelLoad.c 23 -> 33
- UIControls.c 38 -> 47
- MEX.c 2 -> 11
- StubTransform.c 0 -> 5
- ColPick.c, Blobs.c, AIPlayer.c, AIHandler.c +5 each

The only loss is ShipSelect.c, 58 -> 52 (R5.2).

| HW1 file | path under src/ | round 4 | round 5 | of which CET | defs (approx.) | matched / defs | verbatim | bytes |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| KASFunc.c | Game/KASFunc.c | 293 | 294 | 0 | 313 | 93.9% | 294 | 16,573 |
| ETG.c | Game/ETG.c | 205 | 205 | 4 | 244 | 84.0% | 205 | 33,365 |
| MultiplayerGame.c | Game/MultiplayerGame.c | 175 | 175 | 3 | 233 | 75.1% | 174 | 37,576 |
| AIMoves.c | Game/AIMoves.c | 162 | 163 | 0 | 270 | 60.4% | 163 | 28,213 |
| TitanInterface.cpp | SDL/TitanInterface.cpp | 144 | 144 | 0 | 165 | 87.3% | 27 | 67,286 |
| NIS.c | Game/NIS.c | 139 | 139 | 0 | 159 | 87.4% | 138 | 34,289 |
| Particle.c | Game/Particle.c | 132 | 132 | 0 | 175 | 75.4% | 132 | 28,809 |
| UnivUpdate.c | Game/UnivUpdate.c | 98 | 98 | 4 | 139 | 70.5% | 97 | 40,315 |
| CommandLayer.c | Game/CommandLayer.c | 97 | 97 | 0 | 144 | 67.4% | 96 | 46,672 |
| Options.c | Game/Options.c | 79 | 89 | 1 | 154 | 57.8% | 89 | 15,002 |
| SaveGame.c | Game/SaveGame.c | 83 | 86 | 1 | 180 | 47.8% | 86 | 24,681 |
| AIUtilities.c | Game/AIUtilities.c | 83 | 85 | 0 | 113 | 75.2% | 84 | 24,581 |
| utility.c | SDL/utility.c | 74 | 74 | 0 | 98 | 75.5% | 74 | 18,772 |
| Tutor.c | Game/Tutor.c | 46 | 63 | 0 | 80 | 78.8% | 63 | 16,661 |
| Universe.c | Game/Universe.c | 56 | 56 | 2 | 93 | 60.2% | 55 | 21,115 |
| CommandNetwork.c | Game/CommandNetwork.c | 53 | 53 | 0 | 68 | 77.9% | 52 | 15,236 |
| MultiplayerLANGame.c | Game/MultiplayerLANGame.c | 53 | 53 | 0 | 77 | 68.8% | 52 | 12,950 |
| ShipSelect.c | Game/ShipSelect.c | 58 | 52 | 0 | 68 | 76.5% | 52 | 5,106 |
| ConsMgr.c | Game/ConsMgr.c | 48 | 50 | 0 | 87 | 57.5% | 50 | 17,403 |
| UIControls.c | Game/UIControls.c | 38 | 47 | 3 | 84 | 56.0% | 47 | 17,349 |
| SinglePlayer.c | Game/SinglePlayer.c | 45 | 46 | 2 | 107 | 43.0% | 46 | 16,886 |
| FlightMan.c | Game/FlightMan.c | 45 | 45 | 0 | 63 | 71.4% | 45 | 13,987 |
| AITeam.c | Game/AITeam.c | 43 | 44 | 0 | 80 | 55.0% | 44 | 6,473 |
| texreg.c | SDL/texreg.c | 44 | 44 | 1 | 76 | 57.9% | 42 | 20,096 |
| StatScript.c | Game/StatScript.c | 42 | 42 | 4 | 60 | 70.0% | 42 | 9,637 |
| ColPick.c | Game/ColPick.c | 35 | 40 | 0 | 46 | 87.0% | 40 | 11,050 |
| FEFlow.c | Game/FEFlow.c | 38 | 38 | 1 | 71 | 53.5% | 35 | 11,322 |
| TitanNet.c | Game/TitanNet.c | 35 | 35 | 0 | 39 | 89.7% | 35 | 3,507 |
| Formation.c | Game/Formation.c | 31 | 33 | 0 | 47 | 70.2% | 31 | 16,629 |
| LevelLoad.c | Game/LevelLoad.c | 23 | 33 | 0 | 47 | 70.2% | 33 | 15,106 |
| Mesh.c | Game/Mesh.c | 33 | 33 | 0 | 47 | 70.2% | 33 | 20,829 |
| SoundEvent.c | Game/SoundEvent.c | 32 | 32 | 0 | 51 | 62.7% | 32 | 17,324 |
| Trails.c | Game/Trails.c | 32 | 32 | 2 | 46 | 69.6% | 32 | 13,427 |
| mainrgn.c | SDL/mainrgn.c | 29 | 32 | 0 | 93 | 34.4% | 32 | 17,540 |
| render.c | SDL/render.c | 30 | 32 | 0 | 60 | 53.3% | 27 | 15,869 |
| Region.c | Game/Region.c | 31 | 31 | 0 | 45 | 68.9% | 30 | 5,265 |
| KAS.c | Game/KAS.c | 30 | 30 | 0 | 44 | 68.2% | 30 | 4,426 |
| ObjTypes.c | Game/ObjTypes.c | 29 | 29 | 1 | 46 | 63.0% | 29 | 1,480 |
| Select.c | Game/Select.c | 13 | 29 | 0 | 39 | 74.4% | 29 | 13,471 |
| AIPlayer.c | Game/AIPlayer.c | 23 | 28 | 0 | 34 | 82.4% | 26 | 5,805 |
| CameraCommand.c | Game/CameraCommand.c | 28 | 28 | 0 | 40 | 70.0% | 28 | 7,312 |
| FEReg.c | Game/FEReg.c | 28 | 28 | 0 | 35 | 80.0% | 28 | 13,275 |
| Nebulae.c | Game/Nebulae.c | 28 | 28 | 0 | 71 | 39.4% | 28 | 16,132 |
| glcompat.c | (not in HomeworldSDL) | 28 | 28 | 0 | - | - | 0 | 12,438 |
| main.c | SDL/main.c | 26 | 28 | 1 | 69 | 40.6% | 22 | 16,571 |
| TradeMgr.c | Game/TradeMgr.c | 27 | 27 | 0 | 41 | 65.9% | 27 | 4,720 |
| prim2d.c | SDL/prim2d.c | 23 | 26 | 0 | 36 | 72.2% | 25 | 8,098 |
| Clouds.c | Game/Clouds.c | 25 | 25 | 0 | 43 | 58.1% | 25 | 14,255 |
| Collision.c | Game/Collision.c | 23 | 25 | 0 | 26 | 96.2% | 25 | 15,156 |
| Stats.c | Game/Stats.c | 22 | 25 | 0 | 61 | 41.0% | 25 | 5,073 |
| soundlow.c | SDL/soundlow.c | 24 | 25 | 0 | 31 | 80.6% | 25 | 4,671 |
| CommandWrap.c | Game/CommandWrap.c | 23 | 23 | 5 | 24 | 95.8% | 23 | 3,522 |
| AIOrders.c | Game/AIOrders.c | 22 | 22 | 0 | 30 | 73.3% | 22 | 10,994 |
| Subtitle.c | Game/Subtitle.c | 22 | 22 | 0 | 26 | 84.6% | 22 | 5,863 |
| Blobs.c | Game/Blobs.c | 16 | 21 | 0 | 31 | 67.7% | 21 | 8,419 |
| ResearchAPI.c | Game/ResearchAPI.c | 21 | 21 | 2 | 32 | 65.6% | 21 | 5,282 |
| font.c | SDL/font.c | 21 | 21 | 0 | 29 | 72.4% | 21 | 5,742 |
| AIHandler.c | Game/AIHandler.c | 15 | 20 | 0 | 25 | 80.0% | 20 | 4,384 |
| AIShip.c | Game/AIShip.c | 20 | 20 | 0 | 29 | 69.0% | 20 | 11,018 |
| Matrix.c | Game/Matrix.c | 20 | 20 | 0 | 25 | 80.0% | 20 | 2,333 |
| Shader.c | Game/Shader.c | 10 | 20 | 0 | 32 | 62.5% | 20 | 5,059 |
| mouse.c | SDL/mouse.c | 20 | 20 | 0 | 29 | 69.0% | 20 | 8,436 |
| AITrack.c | Game/AITrack.c | 19 | 19 | 0 | 24 | 79.2% | 19 | 6,816 |
| Battle.c | Game/Battle.c | 19 | 19 | 0 | 26 | 73.1% | 19 | 6,786 |
| File.c | Game/File.c | 19 | 19 | 0 | 30 | 63.3% | 18 | 6,480 |
| ResCollect.c | Game/ResCollect.c | 17 | 19 | 0 | 26 | 73.1% | 19 | 4,889 |
| Sensors.c | Game/Sensors.c | 19 | 19 | 0 | 56 | 33.9% | 19 | 8,273 |
| SpeechEvent.c | Game/SpeechEvent.c | 19 | 19 | 1 | 33 | 57.6% | 19 | 7,677 |
| Dock.c | Game/Dock.c | 18 | 18 | 0 | 113 | 15.9% | 18 | 5,139 |
| GravWellGenerator.c | Ships/GravWellGenerator.c | 18 | 18 | 0 | 21 | 85.7% | 18 | 3,298 |
| HS.c | Game/HS.c | 17 | 18 | 0 | 28 | 64.3% | 18 | 4,881 |
| Ping.c | Game/Ping.c | 18 | 18 | 0 | 25 | 72.0% | 17 | 4,208 |
| ResearchGUI.c | Game/ResearchGUI.c | 18 | 18 | 0 | 47 | 38.3% | 18 | 8,062 |
| BTG.c | Game/BTG.c | 17 | 17 | 0 | 26 | 65.4% | 17 | 5,565 |
| NetCheck.c | Game/NetCheck.c | 17 | 17 | 0 | 24 | 70.8% | 17 | 2,398 |
| HorseRace.c | Game/HorseRace.c | 16 | 16 | 0 | 29 | 55.2% | 16 | 8,271 |
| LaunchMgr.c | Game/LaunchMgr.c | 15 | 16 | 0 | 38 | 42.1% | 16 | 4,317 |
| Task.c | Game/Task.c | 16 | 16 | 0 | 16 | 100.0% | 14 | 2,756 |
| AIEvents.c | Game/AIEvents.c | 15 | 15 | 0 | 32 | 46.9% | 15 | 2,365 |
| ScenPick.c | Game/ScenPick.c | 13 | 15 | 0 | 20 | 75.0% | 15 | 4,459 |
| Memory.c | Game/Memory.c | 14 | 14 | 0 | 35 | 40.0% | 14 | 4,243 |
| Objectives.c | Game/Objectives.c | 14 | 14 | 0 | 24 | 58.3% | 14 | 2,412 |
| Timer.c | Game/Timer.c | 12 | 14 | 0 | 17 | 82.4% | 14 | 2,186 |
| Vector.c | Game/Vector.c | 12 | 14 | 0 | 15 | 93.3% | 14 | 1,246 |
| AIAttackMan.c | Game/AIAttackMan.c | 12 | 13 | 0 | 23 | 56.5% | 13 | 5,916 |
| AIDefenseMan.c | Game/AIDefenseMan.c | 13 | 13 | 0 | 23 | 56.5% | 13 | 4,421 |
| AIResourceMan.c | Game/AIResourceMan.c | 13 | 13 | 0 | 22 | 59.1% | 13 | 5,578 |
| AIVar.c | Game/AIVar.c | 13 | 13 | 0 | 18 | 72.2% | 13 | 1,374 |
| GenericInterceptor.c | Ships/GenericInterceptor.c | 13 | 13 | 0 | 17 | 76.5% | 13 | 6,153 |
| LinkedList.c | Game/LinkedList.c | 13 | 13 | 0 | 19 | 68.4% | 13 | 1,351 |
| MadLinkIn.c | Game/MadLinkIn.c | 2 | 13 | 0 | 16 | 81.2% | 13 | 2,865 |
| MeshAnim.c | Game/MeshAnim.c | 13 | 13 | 0 | 21 | 61.9% | 13 | 2,582 |
| MinelayerCorvette.c | Ships/MinelayerCorvette.c | 13 | 13 | 0 | 14 | 92.9% | 13 | 4,139 |
| PiePlate.c | Game/PiePlate.c | 13 | 13 | 0 | 20 | 65.0% | 13 | 6,755 |
| sstream.c | SDL/sstream.c | 13 | 13 | 0 | 17 | 76.5% | 13 | 3,294 |
| ChannelFSM.c | Game/ChannelFSM.c | 12 | 12 | 0 | 16 | 75.0% | 12 | 868 |
| GameChat.c | Game/GameChat.c | 12 | 12 | 0 | 18 | 66.7% | 12 | 3,760 |
| Key.c | Game/Key.c | 12 | 12 | 0 | 17 | 70.6% | 12 | 920 |
| prim3d.c | SDL/prim3d.c | 11 | 12 | 0 | 18 | 66.7% | 12 | 3,409 |
| AIFleetMan.c | Game/AIFleetMan.c | 11 | 11 | 0 | 21 | 52.4% | 11 | 5,934 |
| AutoDownloadMap.c | Game/AutoDownloadMap.c | 11 | 11 | 0 | 19 | 57.9% | 11 | 2,109 |
| GamePick.c | Game/GamePick.c | 11 | 11 | 0 | 31 | 35.5% | 11 | 2,383 |
| Light.c | Game/Light.c | 10 | 11 | 0 | 15 | 73.3% | 11 | 1,967 |
| MEX.c | Game/MEX.c | 2 | 11 | 0 | 17 | 64.7% | 11 | 3,155 |
| Captaincy.c | Game/Captaincy.c | 10 | 10 | 0 | 30 | 33.3% | 10 | 3,364 |
| DefenseFighter.c | Ships/DefenseFighter.c | 10 | 10 | 0 | 19 | 52.6% | 10 | 792 |
| mixfft.c | SDL/mixfft.c | 10 | 10 | 0 | 11 | 90.9% | 10 | 4,916 |
| RepairCorvette.c | Ships/RepairCorvette.c | 9 | 9 | 0 | 15 | 60.0% | 9 | 1,943 |
| debugwnd.c | SDL/debugwnd.c | 8 | 9 | 0 | 16 | 56.2% | 9 | 2,884 |
| BigFile.c | Game/BigFile.c | 8 | 8 | 0 | 35 | 22.9% | 5 | 3,088 |
| Camera.c | Game/Camera.c | 8 | 8 | 0 | 17 | 47.1% | 8 | 2,456 |
| Carrier.c | Ships/Carrier.c | 8 | 8 | 0 | 10 | 80.0% | 8 | 902 |
| Crates.c | Game/Crates.c | 8 | 8 | 0 | 14 | 57.1% | 8 | 5,155 |
| Demo.c | Game/Demo.c | 8 | 8 | 0 | 10 | 80.0% | 8 | 1,625 |
| Gun.c | Game/Gun.c | 7 | 8 | 0 | 20 | 40.0% | 8 | 7,365 |
| LOD.c | Game/LOD.c | 4 | 8 | 1 | 11 | 72.7% | 8 | 1,551 |
| Mothership.c | Ships/Mothership.c | 8 | 8 | 0 | 18 | 44.4% | 8 | 1,672 |
| PlugScreen.c | Game/PlugScreen.c | 8 | 8 | 0 | 17 | 47.1% | 8 | 3,285 |
| Queue.c | SDL/Queue.c | 8 | 8 | 0 | 8 | 100.0% | 8 | 446 |
| Randy.c | Game/Randy.c | 6 | 8 | 1 | 12 | 66.7% | 8 | 710 |
| SalCapCorvette.c | Ships/SalCapCorvette.c | 8 | 8 | 0 | 46 | 17.4% | 8 | 227 |
| Tactics.c | Game/Tactics.c | 5 | 8 | 0 | 37 | 21.6% | 8 | 972 |
| glcaps.c | (not in HomeworldSDL) | 8 | 8 | 0 | - | - | 0 | 1,472 |
| smixer.c | SDL/smixer.c | 8 | 8 | 0 | 9 | 88.9% | 5 | 3,779 |
| Clipper.c | Game/Clipper.c | 7 | 7 | 0 | 10 | 70.0% | 7 | 4,805 |
| Color.c | Game/Color.c | 7 | 7 | 0 | 11 | 63.6% | 7 | 2,270 |
| Damage.c | Game/Damage.c | 6 | 7 | 0 | 15 | 46.7% | 7 | 1,955 |
| Physics.c | Game/Physics.c | 7 | 7 | 0 | 8 | 87.5% | 7 | 5,646 |
| Alliance.c | Game/Alliance.c | 6 | 6 | 1 | 10 | 60.0% | 6 | 1,715 |
| Attack.c | Game/Attack.c | 5 | 6 | 0 | 7 | 85.7% | 6 | 2,559 |
| Eval.c | Game/Eval.c | 6 | 6 | 0 | 19 | 31.6% | 6 | 3,298 |
| FontReg.c | Game/FontReg.c | 6 | 6 | 0 | 6 | 100.0% | 6 | 935 |
| HeavyCorvette.c | Ships/HeavyCorvette.c | 6 | 6 | 0 | 12 | 50.0% | 6 | 1,232 |
| Scroller.c | Game/Scroller.c | 6 | 6 | 0 | 7 | 85.7% | 6 | 530 |
| Tactical.c | Game/Tactical.c | 6 | 6 | 0 | 7 | 85.7% | 6 | 2,229 |
| TaskBar.c | Game/TaskBar.c | 6 | 6 | 0 | 49 | 12.2% | 6 | 1,349 |
| Teams.c | Game/Teams.c | 4 | 6 | 0 | 8 | 75.0% | 6 | 2,284 |
| fqeffect.c | SDL/fqeffect.c | 6 | 6 | 1 | 20 | 30.0% | 6 | 557 |
| AutoLOD.c | Game/AutoLOD.c | 4 | 5 | 0 | 15 | 33.3% | 5 | 329 |
| MissileDestroyer.c | Ships/MissileDestroyer.c | 5 | 5 | 0 | 6 | 83.3% | 5 | 951 |
| P1MissileCorvette.c | Ships/P1MissileCorvette.c | 5 | 5 | 0 | 6 | 83.3% | 5 | 497 |
| StringSupport.c | Game/StringSupport.c | 4 | 5 | 0 | 6 | 83.3% | 5 | 880 |
| StubTransform.c | Game/StubTransform.c | 0 | 5 | 0 | 9 | 55.6% | 5 | 1,040 |
| ddraw.cpp | (not in HomeworldSDL) | 5 | 5 | 0 | - | - | 0 | 660 |
| rinit.cpp | (not in HomeworldSDL) | 5 | 5 | 0 | - | - | 0 | 2,058 |
| Animatic.c | Game/Animatic.c | 3 | 4 | 0 | 17 | 23.5% | 4 | 1,161 |
| B-Spline.c | Game/B-Spline.c | 4 | 4 | 0 | 8 | 50.0% | 4 | 1,025 |
| BitIO.c | ThirdParty/LZSS/BitIO.c | 4 | 4 | 0 | 13 | 30.8% | 2 | 226 |
| Bounties.c | Game/Bounties.c | 1 | 4 | 0 | 6 | 66.7% | 4 | 506 |
| DefaultShip.c | Ships/DefaultShip.c | 4 | 4 | 0 | 6 | 66.7% | 4 | 399 |
| KeyBindings.c | Game/KeyBindings.c | 4 | 4 | 0 | 14 | 28.6% | 4 | 573 |
| LagPrint.c | Game/LagPrint.c | 4 | 4 | 0 | 4 | 100.0% | 4 | 263 |
| Md52.cpp | (not in HomeworldSDL) | 4 | 4 | 0 | - | - | 0 | 2,568 |
| ProximitySensor.c | Ships/ProximitySensor.c | 4 | 4 | 0 | 13 | 30.8% | 4 | 816 |
| ShipView.c | Game/ShipView.c | 4 | 4 | 0 | 15 | 26.7% | 4 | 2,421 |
| TimeoutTimer.c | SDL/TimeoutTimer.c | 4 | 4 | 0 | 8 | 50.0% | 4 | 233 |
| Titan.c | SDL/Titan.c | 4 | 4 | 0 | 3 | 133.3% | 2 | 327 |
| Volume.c | Game/Volume.c | 4 | 4 | 0 | 4 | 100.0% | 4 | 1,130 |
| Chatting.c | Game/Chatting.c | 3 | 3 | 0 | 3 | 100.0% | 3 | 471 |
| Clamp.c | Game/Clamp.c | 3 | 3 | 0 | 3 | 100.0% | 3 | 898 |
| DFGFrigate.c | Ships/DFGFrigate.c | 3 | 3 | 0 | 3 | 100.0% | 3 | 998 |
| GenericDefender.c | Ships/GenericDefender.c | 3 | 3 | 0 | 4 | 75.0% | 3 | 1,730 |
| P1IonArrayFrigate.c | Ships/P1IonArrayFrigate.c | 3 | 3 | 0 | 3 | 100.0% | 3 | 267 |
| P1StandardCorvette.c | Ships/P1StandardCorvette.c | 3 | 3 | 0 | 4 | 75.0% | 3 | 380 |
| SoundEventPlay.c | Game/SoundEventPlay.c | 2 | 3 | 0 | 4 | 75.0% | 3 | 3,107 |
| Tweak.c | Game/Tweak.c | 3 | 3 | 0 | 3 | 100.0% | 3 | 205 |
| fqcodec.c | SDL/fqcodec.c | 3 | 3 | 0 | 5 | 60.0% | 3 | 1,372 |
| fquant.c | SDL/fquant.c | 3 | 3 | 0 | 4 | 75.0% | 3 | 1,270 |
| rglu.c | SDL/rglu.c | 3 | 3 | 0 | 10 | 30.0% | 3 | 976 |
| rinit.c | SDL/rinit.c | 2 | 3 | 2 | 14 | 21.4% | 3 | 693 |
| AdvanceSupportFrigate.c | Ships/AdvanceSupportFrigate.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 141 |
| BMP.c | Game/BMP.c | 2 | 2 | 0 | 2 | 100.0% | 2 | 227 |
| CRC32.c | ThirdParty/CRC/CRC32.c | 2 | 2 | 0 | 2 | 100.0% | 2 | 104 |
| Debug.c | Game/Debug.c | 2 | 2 | 0 | 9 | 22.2% | 2 | 183 |
| Globals.c | Game/Globals.c | 2 | 2 | 0 | 2 | 100.0% | 1 | 334 |
| InfoOverlay.c | Game/InfoOverlay.c | 2 | 2 | 0 | 10 | 20.0% | 2 | 651 |
| MultiGunCorvette.c | Ships/MultiGunCorvette.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 103 |
| NavLights.c | Game/NavLights.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 634 |
| P1Mothership.c | Ships/P1Mothership.c | 2 | 2 | 0 | 3 | 66.7% | 2 | 101 |
| ResourceCollector.c | Ships/ResourceCollector.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 85 |
| SoundEventStop.c | Game/SoundEventStop.c | 2 | 2 | 0 | 2 | 100.0% | 2 | 136 |
| Star3d.c | Game/Star3d.c | 1 | 2 | 0 | 3 | 66.7% | 2 | 669 |
| Transformer.c | Game/Transformer.c | 2 | 2 | 0 | 1 | 200.0% | 0 | 374 |
| Twiddle.c | Game/Twiddle.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 55 |
| Undo.c | Game/Undo.c | 2 | 2 | 0 | 4 | 50.0% | 2 | 62 |
| dct.c | SDL/dct.c | 0 | 2 | 0 | 2 | 100.0% | 2 | 609 |
| fastmath.c | (not in HomeworldSDL) | 2 | 2 | 0 | - | - | 0 | 227 |
| FloatingCity.c | Ships/FloatingCity.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 51 |
| HeavyCruiser.c | Ships/HeavyCruiser.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 115 |
| IonCannonFrigate.c | Ships/IonCannonFrigate.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 99 |
| LZSS.c | ThirdParty/LZSS/LZSS.c | 0 | 1 | 0 | 3 | 33.3% | 1 | 148 |
| P2Swarmer.c | Ships/P2Swarmer.c | 1 | 1 | 0 | 4 | 25.0% | 0 | 41 |
| StandardDestroyer.c | Ships/StandardDestroyer.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 121 |
| StandardFrigate.c | Ships/StandardFrigate.c | 1 | 1 | 0 | 3 | 33.3% | 1 | 68 |
| bink.c | (not in HomeworldSDL) | 0 | 1 | 0 | - | - | 0 | 42 |
| screenshot.c | SDL/screenshot.c | 1 | 1 | 0 | 3 | 33.3% | 0 | 710 |

**HW1 files with 0 matches** (28 files that define at least one function; defs approx., most first):

SDL/utility_mac.c (98), SDL/TitanInterfaceC.c (43), SDL/TitanInterfaceC.stub.c (36), Game/File_Mac.c (30), SDL/ClientCDKey.cpp (25), SDL/avi.c (18), Ships/CloakGenerator.c (17), Ships/DDDFrigate.c (17), SDL/NetworkInterface.c (15), Ships/ResearchShip.c (15), SDL/wave.c (14), Game/ProfileTimers.c (12), Game/Hash.c (6), Game/Types.c (6), Ships/P2MultiBeamFrigate.c (6), Game/Tracking.c (4), Ships/LightCorvette.c (4), Ships/P2AdvanceSwarmer.c (4), Ships/Probe.c (4), Ships/SensorArray.c (4), SDL/TitanPacketMsg.cpp (3), Ships/P2FuelPod.c (3), Ships/P2Mothership.c (3), Ships/P3StandardShip.c (3), SDL/win32glue.c (2), Ships/Drone.c (2), Ships/ResourceController.c (2), Game/Matrix-mult.c (1)

### R5.5 Next-round targets: no unnamed function is left; the largest code outside any function

R4.5's list cannot be continued: the export holds **no `FUN_*` function**. What is left in the game region is code Ghidra never made a function of. `tools/uncovered_code_candidates.py` (new) scans for it, and its output is `ghidra/proposals/r5/uncovered_code_candidates.json`. The scan works like this:
- **What counts as uncovered:** every byte of 0x401000-0x5c9fff outside every `[entry, entry+size)` of `functions.csv`. This is the same approximation the coverage script uses for "bytes inside some function body".
- **What makes a candidate:** a 16-byte aligned address in an uncovered run that:
  - does not start with padding;
  - follows a covered byte, padding or a RET;
  - has its address stored as a dword somewhere in the file.

**Result: 488 candidates, 70,670 bytes (3.8% of the region).** The size runs up to the next function or candidate, so it is an upper bound.
- **Who holds the pointer:** code holds it for 254 candidates (e.g. callbacks that `feRegionsAdd` or `mgListOfGamesInit` pass on, and the thread proc that `streamStartThread` starts). For the other 234 only data holds it: callback tables in `.data`.
- **Size:** 178 are 32 bytes or smaller, 136 are 33-128 bytes, 144 are 129-512 bytes and 30 are larger.
- **Zero `E8` direct calls reach any of them:** they are reached only through pointers.

The scan is not proof. One known false positive is #30 below (0x00464080), a switch table inside `cat_dockGetDockClass`.

Pointer holders by bytes:
- **The table in `.data` above label `NUM_BIG_STARS` (+0x77c...):** 112 candidates, 4,581 bytes, mostly 5-byte `jmp` stubs and 13-byte `push 0; push 0; call` wrappers. This looks like a command/hot-key callback table (e.g. 0x0054ece0 calls `mrRetire`); guess.
- **The tables above `FORMATION_TRAVEL_SLOWEST_SHIP`:** front-end `{fn, name}` entries, 3,120 bytes. Examples: `OverwriteGamePopup`, the recorded-game `.pkts` screens.
- **The table above `lpString2_0072d8b8` (+0x1c76c):** 33 candidates, 2,894 bytes. This is the CRT static-initializer table: `_atexit` callers, among them the `ClientCDKey` path initializer.
- **Code holders:** `piePointModeOnOff`, `feRegionsAdd`, `streamStartThread`, `transStartup`, `feScreenStart`, `glcActivate` and `animBinkPlay`.

The label names are only the nearest label below each holder, not the table's real name.

The 40 largest candidates:

| # | address | size (upper bound) | pointer held by | strings | named callees |
|---:|---|---:|---|---|---|
| 1 | 0x00582a60 | 2,112 | streamStartThread (code) | `>>> Streams: no streams buffer yet..`; `>>> Streams: SoundInited [%2d] Statu` | nullsubReturnFalse, isoundstreamreadheader, __ftol |
| 2 | 0x0049c3b0 | 2,048 | transStartup (code) | - | cat_kniTransGrowVertexLists |
| 3 | 0x004e6530 | 1,621 | piePointModeOnOff (code) | - | cat_shipIsCloakedFully, sub_53F9F0, cat_mimicAppearsAsResource |
| 4 | 0x0057e100 | 1,209 | rinEnumerateDevices (code) | `%08X [%s][%s]`; `driver` | DDRAW.DLL::DirectDrawCreate, _malloc, crc32Compute |
| 5 | 0x004e6e70 | 972 | piePointModeOnOff (code) | - | ranRandomFn, primLine3, primCircleOutline3 |
| 6 | 0x0049b8f0 | 943 | kbListWindowCB (code) | `%s %s`; `??????` | fontMakeCurrent, cat_kbBindingGet, cat_kbCommandNameGet |
| 7 | 0x0055d230 | 941 | glcActivate (code) | `glcDrawPixels: unknown format %d` | glcUpdateExtents, dbgFatalf |
| 8 | 0x00421280 | 899 | animBinkPlay (code) | `what's this RGLtype: %d [binkSimpleDis` | binkGetSurface, rgluOrtho2D, dbgFatalf |
| 9 | 0x00564d70 | 899 | 0x00564f14 (code, no function) | `WaypointEndToEnd`; `WaypointCircular` | soundEventPlay, piePointModeOnOff, MakeShipsMobile |
| 10 | 0x004a3f20 | 805 | SongNumber+0x15c (.data) | `%f %f %f %f` | RemoveCommasFromString, _sscanf, matMakeRotAboutZ |
| 11 | 0x004570c0 | 776 | cmShipInfoDraw (code) | - | cmSelectShipType, tutIsBuildShipRestricted, cmIncrement |
| 12 | 0x0043f330 | 748 | CLOUD_LIGHTNING_MAIN_ALPHA+0x64c (.data); CLOUD_LIGHTNING_MAIN_ALPHA+0x654 (.data) | `CP_RaceOne`; `CP_RaceTwo` | lib_crt_00676c90, cpColorsUpdate, regRecursiveSetDirty |
| 13 | 0x00580210 | 727 | cat_isoundmixerStartThread (code) | - | isoundmixerprocess |
| 14 | 0x00564a90 | 721 | 0x005669e3 (code, no function); 0x0056a197 (code, no function) | - | MakeShipsMobile, lib_crt_006787d0, cat_selSelectionCopy |
| 15 | 0x004f6810 | 720 | rmTechListWindow (code) | - | fontMakeCurrent, fontShadowSet, _sprintf |
| 16 | 0x00455c10 | 645 | RESCONTROLLER_FOLLOW_BEHIND+0x888 (.data) | - | clWrapDeterministicBuild, __ftol, cat_clWrapExternalBuildUpdate |
| 17 | 0x004ea4a0 | 599 | psModeBegin (code) | `exit`; `gameon` | lib_crt_00676c90, regRecursiveSetDirty, soundEventPlayMusic |
| 18 | 0x00539680 | 574 | feScreenStart (code) | - | uicFindFlag, soundEventPlay, regRecursiveSetDirty |
| 19 | 0x00456840 | 573 | cmRightArrowDraw (code) | - | cmSelectShipType, mgAccelerator, tutIsBuildShipRestricted |
| 20 | 0x00487ab0 | 571 | FORMATION_TRAVEL_SLOWEST_SHIP+0x17b8 (.data) | `OverwriteGamePopup` | GeneralMessageBox, fileExists, feScreenStart |
| 21 | 0x00472fc0 | 569 | feRegionsAdd (code) | `atom has no name in feScrollBarProce` | nullsubReturnFalse, tutGameMessage |
| 22 | 0x004883a0 | 565 | FORMATION_TRAVEL_SLOWEST_SHIP+0x17c0 (.data) | `SavedGames\\RecordedGames\\`; `.pkts` | feScreenDisappear, fileDelete, fontMakeCurrent |
| 23 | 0x004bbea0 | 551 | mgListOfGamesInit (code) | - | CheckNetworkVersionCompatibility, fontMakeCurrent, fontWidth |
| 24 | 0x004c28f0 | 550 | mgListOfServersInit (code) | - | fontMakeCurrent, fontWidth, fontHeight |
| 25 | 0x00421050 | 548 | animBinkPlay (code) | - | binkGetSurface, cat_glideLfbLock, subTitlesDraw |
| 26 | 0x00456a80 | 539 | cmLeftArrowDraw (code) | - | cmSelectShipType, mgAccelerator, cmDecrement |
| 27 | 0x00456210 | 531 | RESCONTROLLER_FOLLOW_BEHIND+0x8c8 (.data) | - | fontCurrentGet, fontMakeCurrent, feStaticRectangleDraw |
| 28 | 0x00487ef0 | 529 | FORMATION_TRAVEL_SLOWEST_SHIP+0x17b0 (.data) | `OverwriteGamePopup` | GeneralMessageBox, fileExists, feScreenStart |
| 29 | 0x004bbc90 | 518 | mgListOfGamesInit (code) | - | fontMakeCurrent, fontWidth, fontHeight |
| 30 | 0x00464080 | 517 | cat_dockGetDockClass (code) | - | - |
| 31 | 0x00487cf0 | 507 | FORMATION_TRAVEL_SLOWEST_SHIP+0x17a8 (.data) | `.pkts`; `%s %i` | VerifySaveFile, GeneralMessageBox, feScreenDisappear |
| 32 | 0x005738f0 | 501 | cat_timeCompressionFactorTable+0x118 (.rdata) | - | std___Xran, lib_crt_005541f0, _memmove |
| 33 | 0x005aa370 | 496 | utyGameSystemsInit (code) | `Quit to main menu!`; `Credits\\` | memFree, nullsubReturnFalse, feAllScreensDelete |
| 34 | 0x004ba830 | 476 | mgChatWindowInit (code) | `<%s>`; `<%s>` | fontMakeCurrent, fontWidth, fontHeight |
| 35 | 0x00573600 | 474 | cat_timeCompressionFactorTable+0x114 (.rdata) | - | std___Xran, lib_crt_005541f0, _memmove |
| 36 | 0x0048c980 | 471 | setGunBindInfo (code) | - | hmatMakeHMatFromMat, hmatMakeRotAboutX, hmatMakeRotAboutY |
| 37 | 0x004c5180 | 460 | lgListOfGamesInit (code) | - | CheckNetworkVersionCompatibility, fontMakeCurrent, fontWidth |
| 38 | 0x004bc370 | 447 | mgGameChatWindowInit (code) | `<%s>`; `<%s>` | fontMakeCurrent, fontWidth, fontHeight |
| 39 | 0x004c5480 | 447 | lgGameChatWindowInit (code) | `<%s>`; `<%s>` | fontMakeCurrent, fontWidth, fontHeight |
| 40 | 0x004c4fd0 | 426 | lgListOfGamesInit (code) | - | fontMakeCurrent, fontWidth, fontHeight |

### R5.6 Remaining gaps

Ranked by bytes and by how directly the fix follows from data already on disk. Unlike R2.6-R4.6, none of these is a naming backlog of `FUN_*` functions.

1. **Code outside any function, about 70 KB (R5.5).** There are 488 pointer-referenced entries; most are callbacks reached only through `.data` tables or pointers that code passes on, the same kind of function as the 15 deferred names this round. Ghidra's analysis did not follow these pointers.
   - **Fix, mostly mechanical:** a CreateFunctions pass over the candidates, after a filter. Keep entries that disassemble to a RET and are not inside a switch table. Run the analysis inside the FixFunctionBodies snapshot guard.
   - **Naming after that:** many are trivial stubs (5-byte `jmp` thunks, 13-byte wrappers) that can take their target's name plus a suffix. The bigger ones (#1-#40 above) need one producer, mostly front-end screens, key bindings, the sound stream thread and the GL/Bink paths.
   - **Size:** 70,670 bytes is 3.8% of the region, an upper bound.
2. **The 15 deferred functions have no prototype.** Their audited signatures are in `ghidra/proposals/r5/reconcile_deferred_functions.json`; apply them as prototype-only entries after the round-3/4 binary check. 69 R5-created functions in total carry the `R5_created` tag; D1's 54 got prototypes from prototypes-r5.
3. **Prototypes.**
   - 1,752 real-named functions have a symbol entry without a prototype, and 514 are in no symbol file. In the game region the numbers are 650 and 410.
   - Most of the rest is WON/CRT/library code that has no HW1 declaration and that M2 excludes.
   - 15 prototypes are still applied degraded.
   - 34 int-for-pointer prototypes were dropped by reconcile and need a pointer-typed rewrite.
   - 40 applied prototypes carry a caller-cleanup flag (reconcile report).
4. **Low-confidence names: 636 functions** carry a low-confidence class source (per-class table: cat-new 458, hw1 132, lib 33, won 13). Round 5 alone added 193 low names out of 457 (42%). The two re-checks so far found few errors:
   - **Round 4's quality-recheck:** 120 round-2/3 low names, 1 rename upheld.
   - **Round 5's lowconf-r4:** 180 round-4 low names, 7 renames, 18 raises, 141 confirmed.

   So about 4% of low names are wrong (7 of 180). Expect roughly 25 wrong among the 636, all of them `cat_*`/`hw1` names whose subsystem is right and whose verb may be off (inference from those two samples). These are cheaper to fix during M2, when each function is decompiled anyway, than in a dedicated round.
5. **Class-only names in the game region (140, 14,969 bytes):**
   - 72 `Catch@` funclets;
   - 57 `lib_crt_*` (CRT code linked into `.text`);
   - 9 `lib_msvc_*`;
   - 1 `switchD`;
   - `cat_new_00515930` (13 bytes; the auditor's `cat_blastBigGunMaxRadius` was not proposed by any set).

   `lib_crt_005549aa` is an off-cut of `lib_crt_00554930` and should be merged with `MergeOffcuts.py`.
6. **HW1 files with 0 matches: 28.** Most define code that Cataclysm cannot contain: Mac ports, `TitanInterfaceC*` stubs, `ClientCDKey.cpp`, `avi.c`, `wave.c`, `ProfileTimers.c`, and HW1 race ships Cataclysm does not ship (guess for the ships: `CloakGenerator.c`, `DDDFrigate.c`, `ResearchShip.c`, `P2*`, `Probe.c`...). render-combat recorded that no `FUN_*` remained in the AIMoves.c, Dock.c and Nebulae.c address ranges. Their unmatched HW1 definitions are therefore inlined, folded or absent. M1's exit criterion asks for a documented reason per zero-match file; a short table of those reasons is still missing.
7. **Mission region: done** (unchanged since round 4).
8. **Carried over:**
   - `qwen-naming.json` is still unaudited. With no `FUN_*` left, its proposals can only be compared against the applied names; a full audit no longer pays.
   - The `universe.world[]` offset +0x3D9B0 vs CET's +0x3DA30 is still unverified.
   - 2 thread-start functions keep `lpStartAddress_*` names.
   - Any pass that runs analysis on mission code needs the FixFunctionBodies snapshot guard, and must keep the 41 `CALL_RETURN` overrides (`docs/ghidra-round4-prep.md`).

### R5.7 Verdict: naming is saturated, so stop the naming rounds

**Another naming round is not worth its cost (about 3M subagent tokens per round).** The evidence:
- **Nothing unnamed is left.** Round 5 named all 457 game-region `FUN_*` functions that remained. Real names now cover 99.1% of game-region function bytes and 91.1% of all game-region bytes. M1's exit target is 80% of game-code bytes. There are 0 `FUN_*` functions in the program.
- **Returns per round have shrunk.** Real-named bytes grew by:
  - +580 KB in round 2;
  - +618 KB in round 3;
  - +190 KB in round 4;
  - +126 KB in round 5 (2,407,059 -> 2,533,080, of which 21 KB came from D1).

  A round 6 has no `FUN_*` pool left to name.
- **What remains is not naming work:**
  - about 70 KB of code is outside any function (gap 1). That needs a Ghidra function-discovery pass, not a producer fleet;
  - 140 class-only library/funclet entries (14,969 bytes) are out of M2 scope;
  - 636 low-confidence names hold an expected ~4% error rate;
  - the missing prototypes are mostly in library code.
- **Where a full round would spend its tokens:** it would re-litigate low names and library prototypes, which the M2 decompilation reviews function by function anyway.

**What to do instead:**
1. **M1 close-out (small, optional).** It has three parts:
   - one guarded Ghidra pass that creates functions at the filtered R5.5 candidates and applies the 15 deferred prototypes. This is a Ghidra stage with no LLM tokens;
   - at most one producer and one auditor to name the new functions. Estimate: about 1M tokens at round-5 rates (about 230K tokens per agent);
   - a reasons table for the 28 zero-match HW1 files, which M1's exit criterion requires.
2. **Then M2, per `docs/PLAN.md`.** The user owns two decisions first: approve Option A and sign off M0. M2 starts from the HW1 source tree and decompiles only the Cataclysm delta, and the binary map now gives every function a name and a class for that.
   - **First target:** the version-0x500 mesh / `.peo` loader (PLAN step 3: "nothing renders until it is understood").

## M2-R3 name fixes

Applied 2026-10-10 from `ghidra/symbols_m2r2_namefix.json` (3 symbols, cet_symbols schema). The source is the audited M2-R2 proposal file `ghidra/proposals/m2-r2-name-fixes.json`: the M2-R2 re-read compared each body with HW1 ad1bb82 and found the binary-map name pointed at the wrong HW1 function. The new export is **`ghidra/exports/cataclysm_gog_r6/`**, now the newest; read it in place of `cataclysm_gog_r5`.

| address | old name | new name | prototype applied | HW1 (ad1bb82) |
|---|---|---|---|---|
| 0x00404090 | `aieCheckEnemyNearby` | `aieCheckHealthLow` | `sdword __cdecl aieCheckHealthLow(AITeam *team)` | `Game/AIEvents.c:465` |
| 0x00404150 | `aieCheckEnemyNotNearby` | `aieCheckHealthHigh` | `sdword __cdecl aieCheckHealthHigh(AITeam *team)` | `Game/AIEvents.c:499` |
| 0x004aeb10 | `meshFixupPacoUV` | `meshObjectFixupPacoUV` | `void __cdecl meshObjectFixupPacoUV(polygonobject *object, sdword on)` | `Game/mesh.c:430` |

- **Why.** The two `aieCheck*` names were assigned in round 3 by call order only. Their bodies are HW1's health-low/high watchers: one parameter, the `+0x100`/`+0x10c` watchIndividual flags, no `aiuFindNearbyEnemyShips` call. HW1's enemy-nearby checks are inlined in `aieExecute` 0x00403a70. The body at 0x004aeb10 is the per-object polygon loop (PAC_FIXED 0x1000, 0.6/width nudge); the meshdata-level `meshFixupPacoUV` is inlined into `meshRender` 0x004b4280 and `meshRenderShipHierarchy` 0x004b4520. The objdump checks agree with the prototypes: each function ends in a plain `ret`, and the callers clean 4 bytes (`aieExecute`) or push 2 arguments (the two mesh callers).
- **Pass.** One headless pass on project `ghidra/projects/cataclysm_gog`. It ran in reuse mode with `-noanalysis` and no re-import. ImportSymbolsJson (phase=all) read only the 3-symbol file, so nothing from earlier rounds was re-applied. A `DropStaleComments.py` post-script removed the old names' `[CET]` plate lines, as round 5 did (4 lines at the 3 addresses). ExportAnalysis ran with decompile on. The pass was detached with `setsid nohup`, logged to `ghidra/exports/r6_headless.log`, and finished with `rc=0` in 62 s on the first attempt. The log has 0 `ERROR`/Traceback/lock lines. It has 7 decompiler `WARN` lines, the same 2 "Unable to read bytes" + 5 `.cms_t` p-code lines as round 5.
- **Result.** The apply report `cataclysm_gog_r6/cet_apply_report.csv` gives **3 / 3** `renamed;proto-applied`, with 0 failed, 0 degraded and 0 `kept-user-name`.
  - `functions.csv` r5 vs r6: 14,705 functions in both. Exactly 3 rows differ (name and signature at the 3 addresses). No size changed and no other name changed.
  - `symbols.csv`: exactly the 3 rows differ.
  - `call_graph.csv` (sorted): only the 4 edges into the 3 functions changed, and only in the callee name.
  - Decompile: ok 14,618, thunk 86, failed 1 (`lib_crt_00671da2`, as in every round), error 0. These are the same counts as r5.
- **Tracked references updated:** `ghidra/delta_map.json` (`name`, `hw1_func` and `hw1_src` for the 3 entries; classes kept; the review notes say the features were still scored against the old HW1 function), the matching notes in `ghidra/delta_map_review.json`, `docs/m2-r1-function-deltas.md`, `docs/m2-r1-summary.md`, `docs/m2-r2-mesh.md` and `docs/m2-r2-reread.md`. Earlier symbol files and proposal sets keep the old names as history. The latest applied entry wins in the project, so a full re-apply of rounds 1-5 must be followed by `symbols_m2r2_namefix.json`.
