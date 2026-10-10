<!-- done: exit-criteria, items, stage-counts, r2-plan, decisions -->
# M2-R1 summary: the delta inventory

Status: **COMPLETE, 2026-10-10.** Branch `m2`, worktree `.claude/worktrees/m2`, workflow `cycle/workflows/m2-r1.js` (run `wf_cb36c8f7-ad7`). Spec: `cycle/M2_BRIEF.md` R1. This round measured; it ported nothing.

## Exit criteria

| criterion (M2_BRIEF R1) | state | evidence |
|---|---|---|
| `docs/m2-r1-data-deltas.md` exists, audited | met, PASS-WITH-FIXES (precision 0.936) | `docs/audits/m2-r1-data-deltas.md` |
| `docs/formats/*.md` exist (6 formats), audited | met, PASS-WITH-FIXES (0.889) | `docs/audits/m2-r1-formats.md` |
| `docs/m2-r1-function-deltas.md` + `ghidra/delta_map.json`, audited | met, PASS-WITH-FIXES (sample agreement 31/40 = 0.775) | `docs/audits/m2-r1-function-deltas.md` |
| `docs/m2-r1-bootstrap.md` + `src/`, audited | met, PASS-WITH-FIXES (0.935) | `docs/audits/m2-r1-bootstrap.md` |
| `docs/m2-checksum-oracle.md`, audited | met, PASS-WITH-FIXES (0.901) | `docs/audits/m2-r1-checksum-oracle.md` |
| `src/` builds the HW1 engine with no Cataclysm changes | met | integrate re-ran `tools/build.sh --release --clean`: rc 0, `[292/292] Linking target hwi-engine`, 0 compiler warnings under `-Werror`, 8.8 s, `build/release/hwi-engine` 5,157,976 B (text 4,193,125 / data 232,408 / bss 1,685,376), sha256 `0024b0f6…53b297`, the same bytes the producer and the auditor got |
| STATE.md lists the M2-R2 order with delta counts | met | `cycle/STATE.md` "## M2" |

Caveat, not a failed criterion: "unmodified" means "no Cataclysm changes". The tree is Gardens of Kadesh's whole `src/` at `3af31fb`, with GoK's behaviour changes (64 Hz universe by default, save version 0x10ad0029, SSE numerics). Importing ad1bb82 plus hand-picked hunks was measured at 14,257 change blocks and abandoned (decision 1 below).

## The five items

**1. Data deltas (HW1 engine against Cataclysm data).** 34 headless or closed-window runs of the native GoK debug build (31 offscreen, 3 dummy, 1 windowed x11) against a scratch copy of Cataclysm's data, with no input. The engine stops **17 times** before its first front-end frame. 15 stops were cleared in scratch with renames, stubs or row filters, and 1 with `/noSpeech`. One stop has no harmless workaround: the **mesh format**. All 1,727 v0x500 `.geo/.peo` files use 116-byte polygon objects (HW1: 112). With the meshes down-converted (a scratch experiment) the engine reaches `Main_game_screen`, and then stops in level load on `RACE_*` names. ASan caught 2 out-of-bounds writes driven by the data: Cataclysm ship names make `StrToShipType` return -1 in `tweak.script` and `crates.script`. Other numbers: 16,528 archive entries; 461 HW1 path literals checked (72 at the same path, 240 by basename, 149 missing); 145 Cataclysm ship types against HW1's 59 (59 rows map onto 37 HW1 types, 86 are Cataclysm-only, 22 HW1 types are gone, no id kept); 88 `.shp` keys HW1 ignores; 246 of 271 guns use gimbal types unknown to HW1; 226 of HW1's 590 string ids are missing. Audit: **PASS-WITH-FIXES, precision 0.936** (44/47). The archive listing, the missing and renamed entries, the headers, the mesh layout and the first blocking failure all reproduced. 3 minor errors were fixed inline.

**2. Loader formats (from the binary).** Six documents and six read-only probes: `docs/formats/{mesh,textures,script,shipstatic,savegame,fib}.md`, `tools/formats/*_probe.py`.
- **Mesh:** `polygonobject` grows 0x70 → 0x74 bytes. A new per-vertex 16-byte array at +0x1C feeds the glow colour, and the later fields shift by 4. The 0x500 stamp is never checked, and `.peo` is HW1's own paged-mesh variant. All 1,729 files pass the probe.
- **Textures:** `.lif` and `textures.ll` are unchanged (12,096 LiFs pass). The in-memory texreg entry grows 0x2C → 0x34, and `trTextureRegister` gains a 4th argument. `textures.ll` has 12,403 entries, more than HW1's 12,000 limit.
- **Scripts:** HW1's parser with one change. The exe has 105 key tables and 4,428 keys; 1,994 of the keys are new (652 of them functional) and 968 HW1 keys are gone.
- **Ship statics:** `ShipStaticInfo` is 0x370 → 0x458 bytes, with 29 new keys and 8 dropped. `GunStatic` is 0xFC → 0x11C. `CustShipHeader` is 0x48 → 0x8C bytes, with 34 slots against HW1's 16.
- **Save games:** the same container and the same version number as HW1 1.04. Ship is 0x63C bytes, the Universe chunk 0x3D9BC, and there are new chunks. No real save exists to probe yet.
- **FIB:** the format and the loader are unchanged; all 22 files pass.

Audit: **PASS-WITH-FIXES, precision 0.889** (40/45). Every Cataclysm offset, size and chunk type that was checked is correct. The 5 errors were minor and are fixed inline.

**3. Function delta map.** `tools/delta_map.py` classified 4,420 of the 4,426 HW1-matched functions; 6 have no HW1 body the tool could find.

| class | functions | Cataclysm bytes | share of bytes |
|---|---:|---:|---:|
| IDENTICAL | 3,357 | 658,389 | 53.1% |
| MODIFIED | 983 | 569,953 | 46.0% |
| REWRITTEN | 80 | 10,505 | 0.8% |

Corrected by the 168 hand reviews, the split is about 58% / 41% / 0.5% of bytes. Systemic changes to port once:
- fuel removed (18 AI functions);
- the ETG/particle argument-array convention (76 functions);
- KAS timers and variables keyed by hashes, and `kasfShipsSelect*` collapsed into one predicate function;
- `ioUpdateShipTotals` added to 30 selection filters;
- the AI move set-up macro (62 functions);
- the new salvage system;
- renumbered race and ship-type enums.

[M2-R2: after the re-read of all 1,013 IDENTICAL/low functions (143 changed class), the audit reclassifications and the tool fixes, the split is 3,175 / 1,168 / 77 functions, 51.1% / 48.1% / 0.8% of bytes; see `docs/m2-r2-reread.md`.] 386 non-identical functions call Cataclysm-only code. Audit: **PASS-WITH-FIXES**. The tables, totals and the estimate reproduce exactly from the JSON, and the tool reproduces its own classes. But the random 40-function sample agrees only **31/40 (0.775)**. The misses are linker-folded names, the hash-keyed `kasfTimer*` family, callee arity and deletions counted as REWRITTEN. Unbiased IDENTICAL agreement is 12/15 (80%). Treat each class as an estimate: IDENTICAL means "port verbatim, then let the oracle check it".

**4. Source-tree bootstrap.**
- **Contents of `src/`:** 603 imported files, byte-identical to their sources except one build-file line. GoK `3af31fb` provides `Game/` (303), `Ships/` (83), `SDL/` (70), `Missions/` (23), `ThirdParty/` (14) and `tools/kas2c/` (6). Relic's `Win32/` at `ad1bb82` (103 files) is kept as reference and not built. The REULA notice is in `src/COPYING.REULA`, `src/README.md`, `src/PROVENANCE.md` and the root README.
- **Build:** meson builds `hwi-engine` in debug (ASan/UBSan, 74,870,832 B) and release (5,157,976 B), with 0 warnings under `-Werror`, in about 11 s from clean. A clang 18 build also passes.
- **Suppressed warnings:** GoK's suppression list hides 15,662 warning sites, among them 1,792 int/pointer conversions.

Audit: **PASS-WITH-FIXES, precision 0.935** (29/31). Both builds reproduce byte for byte, and 20 of 20 spot-checked files are blobs of the stated commits. 2 minor documentation errors were fixed.

**5. Checksum oracle (design only).**
- **HW1:** each sync packet carries `randcheck`, `univcheck` and `blobcheck`, once per 2 universe ticks.
- **Cataclysm:** 23 machinery functions are located by name. The universe checksum covers ships only. The entry has a new 4-byte field (16 B against 12 B). There are 27 RNG streams, and stream 0 is seeded from the wall clock. The sim reads the wall clock through `realtimeelapsed` (23 readers).
- **Design:** `hwi_oracle.dll` hooks 7 sites (H1-H7) and writes a shared `.hwo` record format. The chosen driving mode is the game's own **recorded-game playback** (`recPack*`). AI does not run in playback and the RNG state comes from the save, so no input is needed. Mode C, a saved game left running, is the second mode and does exercise the AI. The oracle must pass an acceptance ladder against itself first, and a tolerance-based fallback is designed in case bit-exact matching is out of reach (x87 vs SSE, 313 x87 transcendental instructions).

Audit: **PASS-WITH-FIXES, precision 0.901** (82/91). Every address and hook prologue checks out. 9 errors were fixed. The two substantive ones: skirmish stream 6 is already reset by `cat_ranResetGameStreams`, and H6 must also replay `LoadRecordedGame`'s `feScreenDisappear` call and its 3 flag writes.

## Delta counts per M2-R2 stage

Recomputed in M2-R2 after the re-read of the 1,013 IDENTICAL/low functions, the audit reclassifications and the `tools/delta_map.py` fixes (`docs/m2-r2-reread.md`). Script: `tools/m2_stage_counts.py` (`--md` prints this table). Run on the M2-R1 delta map (`git show 13723dd:ghidra/delta_map.json`), it reproduces the M2-R1 table exactly: 3,357 / 983 / 80 HW1-matched and 3,789 cat-new functions.

HW1-matched functions come from `ghidra/delta_map.json`, by HW1 source file. Cataclysm-only (`cat-new`, 3,789 functions / 1,036,252 B, the binary-map round-5 class) were assigned to stages as follows: the 2,085 kas2c mission functions by address region; 297 by name prefix (`kas*`, `ai*`, `mg*`/`titan*`...); the other 1,407 **by nearest HW1-matched neighbour in address order**. That last rule is a GUESS: the game objects link roughly in source-file order, but a Cataclysm-only source file inherits its neighbour's stage. The 121 functions of `MEX.c`, `Trails.c`, `Titan.c` and `utility.c` were placed by group (Game → stage 2, other → stage 3). The cat-new column is unchanged from M2-R1.

| stage | HW1 files (main ones) | IDENTICAL fn / B | MODIFIED fn / B | REWRITTEN fn / B | cat-new fn / B (est.) |
|---|---|---:|---:|---:|---:|
| 1 file formats + data load | Mesh, MeshAnim, texreg, LOD, StatScript, SaveGame, BigFile, File, ObjTypes, StringSupport, Tweak, LZSS/BitIO/CRC32 | 220 / 45,966 | 95 / 49,278 | 0 / 0 | 76 / 27,383 |
| 2 universe and ships | Universe, UnivUpdate, CommandLayer, Collision, Physics, Gun, Dock, Formation, ETG, Particle, Select/ShipSelect, Stats, ResearchAPI, Ships/* | 860 / 184,562 | 512 / 216,998 | 30 / 4,189 | 829 / 187,242 |
| 3 UI and presentation | FEFlow/FEReg, UIControls, ConsMgr, ResearchGUI, Options, NIS, sound/speech, render/GL, main/mainrgn, mouse | 938 / 218,233 | 253 / 172,254 | 7 / 2,355 | 429 / 132,822 |
| 4 AI | AI*.c | 403 / 80,590 | 74 / 43,113 | 2 / 169 | 23 / 14,404 |
| 5 LAN networking | CommandNetwork, MultiplayerGame, MultiplayerLANGame, TitanNet, NetCheck, ChannelFSM, chat | 406 / 70,863 | 114 / 74,848 | 12 / 1,179 | 67 / 17,483 |
| 6 missions | KAS, KASFunc, Timer, Objectives, LevelLoad, SinglePlayer, Tutor + kas2c mission code | 348 / 33,203 | 120 / 39,482 | 26 / 1,565 | 2,365 / 656,918 (2,085 / 612,687 B are kas2c mission functions) |
| **total** | | **3,175 / 633,417** | **1,168 / 595,973** | **77 / 9,457** | **3,789 / 1,036,252** |

Against M2-R1, HW1-matched IDENTICAL went from 3,357 to 3,175 functions and MODIFIED from 983 to 1,168; REWRITTEN went from 80 to 77. Most of the shift is in stage 2 (IDENTICAL 971 → 860; of the 112 functions that left IDENTICAL there, 70 are ETG/particle callables that take a pointer to their argument slot, and 74 are in Particle.c and ETG.c) and stage 6 (MODIFIED 90 → 120, mostly KASFunc.c and Tutor.c, among them the hash-keyed `kasfTimer*` family).

## M2-R2 plan

The order stays M2_BRIEF's: file formats (mesh first) → universe and ships → UI → AI → LAN → missions. R1 changes it in four places.

1. **Stage 1 becomes "formats + data load", with the identifier tables.** Data-deltas showed that the first frame is blocked by the mesh format and also by the enums the loaders key on. Those are 145 ship types, 7 races and 12 ship classes, used by 6 script tables, by `GatherStatsFor`, `colPick`, `.mission` and `Tactical.script`. Two of these blocks are memory-corrupting writes, not clean failures. So the CET-derived enums (`ShipDefs.h`, `RaceDefs.h`, `ClassDefs.h`) and the `StrToShipType`/race string tables move into stage 1 from stage 2. **Stage-1 exit test:** `hwi-engine` debug (ASan) reaches `Main_game_screen` on unmodified Cataclysm data, using none of data-deltas' 15 scratch workaround stages and not the mesh down-conversion, and gets past the `RACE_*` level-load stop of run27. The data-deltas run setup is the test harness.
2. **Port the systemic changes once, in headers, at the start of the stage that owns them.** Stage 2 gets the ETG/particle argument arrays (76 functions), `ioUpdateShipTotals` (30) and `SOF_*`/enum renumbering. Stage 4 gets the AI move macro in `AITeam.h` (62) and the fuel removal (18). Stage 6 gets the KAS hash keys (`Timer.c`, `cat_kasVar*`, the selection predicate).
3. **The oracle is built early, alongside stage 1, not after it.** It is the gate for every IDENTICAL port. The audit sampled IDENTICAL as only about 80% truly identical, so "verbatim then verify" depends on the oracle. The recreation-side stream and the comparison tool can be built now. The GOG-side DLL waits for three user answers (decisions 5-7 below). In the same stage, decide the determinism flags: `-O2 -fno-strict-aliasing`, no fast-math, and 16 Hz as the default.
4. **Savegame waits for real saves.** Two saves from the user's M0 play-test (one mission, one skirmish) are needed before the save layout can be finished. Until then, stage 1 ports only the container and the chunk sizes.

**First M2-R2 producers.** About 10 agents: 5 producers, 5 adversarial auditors, plus integrate. Each producer edits `src/`, keeps `tools/build.sh --debug` and `--release` green under `-Werror`, and verifies its loader with the matching `tools/formats/*_probe.py` and a data-deltas-style headless run. No input is used.

| # | producer | scope | done when |
|---|---|---|---|
| 1 | mesh | Cataclysm `meshLoad`: `polygonobject` 0x74, the per-vertex array at +0x1C wired to the colour path, `.lod` naming `.peo`, `meshObjectFixupUV`; `MeshAnim`. Spec: `docs/formats/mesh.md` | all 1,729 files load in the engine with no ASan report; the 21 engine-glow meshes load at startup |
| 2 | identifiers + tweak tables | 145-type/7-race/12-class enums, `StrToShipType`, race strings, `TOTAL_NUM_SHIPS` tables; `aiship`, `tweak`, `crates`, `Tactical`, parade (12 per-ship files), the dropped `Dock.script`; `.mission` Include/ExcludeShips; `colPick`'s new format | the ranks 2, 3, 5 and 7 stops and both ASan writes are gone; level load passes `RACE_*` |
| 3 | ship statics | `ShipStaticInfo` 0x458, `GunStatic` 0x11C (gimbal types), `CustShipHeader` 0x8C/34 slots (layout only, bodies are stage 2), the 29 new `.shp` keys, Cataclysm's per-race `.shp` paths. Spec: `docs/formats/shipstatic.md` | 170/173 `.shp` files parse as the probe does (the 3 light-count cases handled like the exe); no ship silently disabled |
| 4 | startup names, audio, FE art, textures | `bigFilePrecedence` (`Cataclysm.big`, `Update.big` case), `Music4Cat.wxd`, `SectSpeech.vce`/`BeastSpeech.vce` + Cataclysm speech LUT reader, battle-chatter event table, renamed construction/research art, FE callbacks; texreg 0x34 entry, `trTextureRegister` 4th argument, `LL_MaxElements` above 12,403 | ranks 4, 6, 8, 9 and 10 cleared without `/noSpeech`; `textures.ll` fully registered |
| 5 | checksum oracle, recreation side | `hwi-engine` `.hwo` writer at the `univUpdate`/`netcheck` points of `docs/m2-checksum-oracle.md` section 5, the comparison tool, `-Doracle` build option; DLL skeleton only if decision 7 is answered | two `hwi-engine` runs of the same HW1 recorded game give identical streams |

After stage 1: stage 2 universe and ships (1,402 HW1-matched + about 829 cat-new functions, the largest MODIFIED share at 205 KB) splits over two rounds. Then UI, AI, LAN, and missions last, each split so that one round stays near 10 agents.

## Decisions the user owns (also in cycle/STATE.md)

1. **src/ is GoK's whole tree, not ad1bb82 plus minimal hunks** (14,257 change blocks measured). Accept, or ask for a different base.
2. **GoK behaviour changes in `src/`**: 64 Hz universe (16 Hz with `/vanilla`), save version 0x10ad0029, turbo factor 8, SSE numerics. Proposal: decide per item as each subsystem is ported, and make 16 Hz the default once Cataclysm timing lands.
3. **Relic's original `Game/` and `Ships/` in-tree** as `src/reference/hw1-ad1bb82/` (about 10 MB of REULA text), or rely on any GoK clone.
4. **Function-delta follow-up.** [M2-R2: decided yes and done; all 1,013 read (85.9% identical), audit reclassifications applied, both suspect names re-checked: `docs/m2-r2-reread.md`. M2-R3: 0x00404090 renamed `aieCheckHealthLow` (with 0x00404150 `aieCheckHealthHigh` and 0x004aeb10 `meshObjectFixupPacoUV`), export `cataclysm_gog_r6`.] 1,013 IDENTICAL/low functions were sampled (51 read, 86% identical), not all read. Should a round read the rest? The audit also proposes reclassifying the `kasfTimer*` family to MODIFIED and the 10 `SpaceObjRegistryGetShip` callers to IDENTICAL, and flags 2 suspect binary-map names (`aieCheckEnemyNearby` 0x00404090, `RemoveShipFromFormation` 0x00447950).
5. **Oracle and rule 2.** May the oracle DLL call the game's own functions (`recPackInGameStartCB`, `recPackPlayInGameInit`, `utyLoadMultiPlayerGameGivenFilename`) to start recording or playback? No OS input is synthesised. The alternative is one click on "Load recorded game" per run.
6. **One recording by the user**: which skirmish map and player setup becomes the first reference scenario (the recording needs menu clicks once).
7. **DLL toolchain**: the installed `clang-18` + `lld-link` (untried for this), or install `gcc-mingw-w64-i686`.
8. **Two saves** from the M0 play-test (one mission, one skirmish) for `savegame_probe.py`.
9. Still open from PLAN.md: the M0 sign-off; whether **LAN interop with stock 1.01** is required. That decides whether the recreation must reproduce the 16-byte sync entry, the 0x1C packet header and the version check exactly.
