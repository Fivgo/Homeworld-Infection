# Audit: M2-R3 port "closeout-headers" (producer 1)

Audited: `docs/m2-r3-closeout-headers.md` and the uncommitted diff of branch `m2` on top of d2c5494 (28 tracked files,
+3,810 / -1,068 as delivered; earlier ports are committed). Date 2026-10-10. Ghidra was not run. Exe values were read
from `original/game/cataclysm.exe` with read-only helpers of my own (`build/audit-closeout/exetab.py`, `strcheck.py`,
`slotcheck.py`, `fn.py`; scratch, untracked), code from `ghidra/exports/cataclysm_gog_r5/{decompiled.c,
decompiled_index.csv,symbols.csv,datatypes.h,strings.csv}`, HW1 from `src/reference/hw1-ad1bb82/`, CET from
`reference/CataclysmEngineTweaks/` (its `game/` headers are HW1's, so for `SOF_*` and parade slots the exe is the only
authority; `Cataclysm.h` gives `universe` 0x00B62A00 and `realtimeremainder` 0x00A6D9A0).

## 1. Build from clean

- `rm -rf original/scratch-m2/build-audit-closeout-headers`, then `meson setup` with tools/build.sh's arguments
  (`--buildtype=debug -Db_sanitize=address,undefined`; `-Doptimization=2 -Ddebug=false -Db_sanitize=none`) and
  `meson compile`: rc 0 both, `[293/293] Linking target hwi-engine`, 0 `warning:` and 0 `error:` lines (meson.build
  has `werror`). Sizes: debug 76,355,688 B, release 4,809,928 B (the producer's 76,341,616 / 4,809,888 were taken
  before its `soundlow.c` change; its incremental release binary is 4,809,928 B too).
- After the audit's fixes (section 5): a second clean build (`.../clean2/`) rc 0, 0 warnings, debug 76,356,880 B,
  release 4,810,088 B; `tools/build.sh --debug` and `--release` in the worktree rc 0, 0 warnings;
  `tools/m2_layout32.sh` OK (ILP32 sizes as the report: ClampInfo 56, StaticCollInfo 112, AlternativeShips 168,
  MilitaryParadeCommand 260, ranstream 20, SaveChunk 8, ShipStaticInfo 1112 ...).

## 2. Sampled checks (headers, tables, RNG, bookkeeping, runs)

| # | claim | evidence | verdict |
|---:|---|---|---|
| 1 | 27 RNG streams | `ranStartup` 0x004efa30 resets 0x00C2FD80..<0x00C2FFA0 = 0x21C = 27 x 20 B | correct |
| 2 | `ranSave`/`ranLoad` save 27 x 0x14 | loop to 0x00C2FF9C from 0x00C2FD80 (27 x 20) | correct |
| 3 | `cat_ranResetGameStreams` resets streams 1..26 | 0x004efb10 starts at 0x00C2FD94 (stream 1), 0x208 = 26 x 20 | correct |
| 4 | `ranStartup` then advances stream 0 by `wSecond & 0xff` | `GetSystemTime`, `local_10._12_4_ & 0xff` (SYSTEMTIME +12 = wSecond), inlined generator on 0x00C2FD80.. | correct |
| 5 | `ranRandomize` = HW1 ad1bb82 (wSecond) | exe 0x004ef9a0 vs `src/reference/hw1-ad1bb82/Game/randy.c:198`; the map already had a review entry (IDENTICAL, sample MODIFIED/low) | correct |
| 6 | `gameStart`: no `ranStartup`, `cat_ranResetGameStreams` only for non-single-player | decompile 379621-379626 | correct |
| 7 | `realtimeelapsed` right after `totaltimeelapsed` (0x00B62AC4) | symbols.csv `universe_realtimeelapsed` 0x00b62ac4, datatypes.h Universe | correct |
| 8 | `univUpdateReset` zeroes it | 0x0054acd0 body | correct |
| 9 | `cat_univRealTimeDelta` arithmetic (remainder, whole periods / taskFrequency) | 0x0054c360 vs `UnivUpdate.c`; `utyTimerDivisor` is `Uint32` in utility.c | correct |
| 10 | paused branches of `universeUpdateTask` call it | 0x0053e460, both `else cat_univRealTimeDelta()` | correct |
| 11 | Rotatable\|Impactable\|Selectable\|Targetable 0x102003, BigObject 0x20000 | `univCreateShip` 0x00542320 | correct |
| 12 | Resource 0x4000 (0x104003) | `univAddAsteroid` 0x00540a30 | correct |
| 13 | NotBumpable 0x80000 (0x184003) | `univAddDustCloud`, `univAddGasCloud`, `univAddNebula` | correct |
| 14 | Clamped 0x1000000 | `clampObjToObj` 0x004346e0 `\| 0x1000000` | correct |
| 15 | Dead 0x800, DontCreateBlob 0x1000 | `bobListCreate` `& 0x1800`, `univGetChecksum` `& 0x800` | correct |
| 16 | Hide 0x8000, HideSave 0x8000000 | `kasfHideShips` 0x004977a0 | correct |
| 17 | HideSave2 0x10000000 | `univHideJustAboutEverything` / `univUnhideJustAboutEverything` | correct |
| 18 | Hyperspace 0x2000000 | `ShipAlreadyHyperspaceOut` 0x00518850 | correct |
| 19 | Cloaked 0x400, new cloak-device bit 0x200 | `DetectShips` `& 0x400`, `cat_cloakUpdate` set/clear 0x400, `cat_cloakInit` `\| 0x200`, `cat_shipIsCloakedFully` 0x200 && 0x400 | correct |
| 20 | Disabled 0x800000 (`0x808000` = Disabled\|Hide) | `aishipmineaquiretarget`, `getEnemiesWithinProximity` | correct |
| 21 | Crazy 0x200000 | `aitrackForceSteadyShip` 0x00419cd0 | correct |
| 22 | DontDrawTrails 0x400000 | `univSetupShipForControl` `& 0xffbfffff` | correct |
| 23 | `etgEffectCreate` copies attach bits only, EAF_OwnerCoordSys -> 0x20000000; `etgEffectDraw` tests 0x20000000 | 0x00470990 (`&8 -> \|9`, 0x20, 0x10, 0x40, `0x1000000 -> \| 0x20000000`), 0x0046ac20; `EAF_PlayingSpeech` lives in `effectFlags`, unaffected | correct |
| 24 | `ClampInfo` 0x38, list at +0x34 | `clampObjToObj`: `memAllocFunction(0x38)`, `LinkedList[4].tail` = 4 x 12 + 4 = +0x34, host list 0xFA8 | correct |
| 25 | `AlternativeShips` 0xA8, equivalents at +0x58 | `aioCreateReconaissance` 0x0040e3a0 locals -0xC0/-0xB8/-0x68 (20 dwords each) | correct |
| 26 | 63 parade slots in the exe's key order | exe `ParadeInfoScriptTable` 0x0087fb68: 63 rows, all `scriptSetSlotInfoCB` 0x00485090, slot = (dataPtr - 0x00a3e448) / 0x1C = row index = `Formation.h` value, src table in the same order: 0 mismatches | correct |
| 27 | `GetShipSlot` cases | 66 type cases of the exe switch (0x00485b30) vs src: 0 differences (carrier rule `militarySlots[5]/[6]`, `[23]/[24]`; `bHeavyCruiser` -> 36 only around type 0x35 `bMothership`, else 62) | correct |
| 28 | 8 derelict types | exe `derelicttypeinfo` 0x00889b68: Crate 0 .. HyperspaceGate 7, NULL row; src enum and table equal | correct |
| 29 | string enum: 1,705 ids, table in exe row order, rows 564/565 swapped | own PE scan: 1,705 rows, NULL at 0x0089d134, callback 0x00528800 only, ids a permutation of 0..1704, 0 enum mismatches, src rows == exe rows | correct |
| 30 | string offsets | `strsRecon` 0 (`ShipTypeToNiceStr` = `MessageStrings[type]`, ShipDefs `sRecon` 0, `bRecon` 33 = `strbRecon`), `strCLASS_Mothership` 142 (0xbb3858 - 0xbb3620 = 0x238), `strTECH_CLOAKING` 182, R1/R2 info 254/295 (0x3F8/0x49C) | correct |
| 31 | `strNameToEnum` returns the row index (IDENTICAL) | 0x005288f0 loop over `LanguageStrings` to 0x0089d134 | correct |
| 32 | save version 0x10ad0027 | `PreLoadGame` 0x004fdfc0 compare, `SaveGame` 0x004fdaf0 | correct |
| 33 | misc chunks 0x80009000/0x8000A000 0x24, 0x8000B000 0x17A0 | `cat_saveGameMiscChunks` 0x005ba170 (`-0x7fff7000`, `-0x7fff6000`, `-0x7fff5000`) | correct |
| 34 | other `SAVE_CAT32_*` sizes and the ship chunk type | docs/formats/savegame.md rows (0x90, 0x2B0, 0x9C, 0x3D9BC, 0x6530, 0x63C, 0x100, 0x54, 0x0C); `0x40000010` = VARIABLE\|SAVE_SPACEOBJ\|OBJ_ShipType | correct |
| 35 | `StaticCollInfo` lengths +0x78/+0x7C/+0x80, `CustShipFire` +0x11C | `GetCollSizeInDirection` 0x00413f90 reads `Node[7].belongto/.structptr`, `Node[8].next` of the static info (16-byte nodes); `nisFire` calls `[statinfo + 0x11c]` | correct |
| 36 | 19 review entries recorded and merged | 19 `port-R3` entries in `delta_map_review.json`, each reflected in `delta_map.json` (`reviewed`, class); spot checks `cmCloseTextures` (third loop, 0x14/4 = 5), `subTexturesReset` (`trTextureRegister` 4 args), `colPickScalarScan` (`%s%f%f` at 0x007530c8) | correct |
| 37 | stage counts before/after | `tools/m2_stage_counts.py` on HEAD's map and on the new map reproduces every number of the report's table | correct |
| 38 | `tools/m2_strings_gen.py --check` | "up to date"; 590 HW1 names, 364 shared, 226 dormant | correct |
| 39 | `m2_layout32.sh` OK with the new asserts | OK | correct |
| 40 | fe still reaches `Main_game_screen` | `audit-r3-closeout-fe` (and `-final-fe` after the fixes): rc 124 at 40 s, ASan 0 | correct |
| 41 | sp still stops at `GenericInterceptor.c (205)` after the level load | `audit-r3-closeout-sp` / `-final-sp`: same stop as R2's `integrate-exit-sp`, ASan 0 | correct |
| 42 | oracle self-test PASS with 27 streams | `selftest.sh --bin <audit release>`: SELFTEST PASS, A = B sha256 0b1f476e46048a8f (= producer's), 513 `T` records all `rng_streams` 27, valid 0x3f, `real` 0.0625 at tick 1 and 32.0 at tick 512 | correct |
| 43 | debug oracle stream | `audit-r3-closeout-oracle` 140,723 B sha256 56a8721a4b50738d = producer's `r3-closeout-final2-oracle` | correct |
| 44 | statics/meshes/textures quits rc 0 | `audit-r3-closeout-statics` rc 0 (143 of 145); `-final-{statics,meshes,textures}` rc 0, ASan 0, LeakSanitizer clean | correct |
| 45 | retired `SOF_*` = 0 leaves no live code path changed | no literal flag masks in src; negated `SOF_Slaveable` tests only in HW1 research-ship docking (`Dock.c` 5526/5926, `ResearchShip.c` 561), dormant types; no `== SOF_x` with a retired name | correct |
| 46 | dormant derelict ids never index a table | no `[LegacyName]` index and no range comparison in src; `derelicttype` fields are `DerelictType`/`uword` | correct |
| 47 | every Cataclysm change carries `CAT:` | all hunks read; non-Cataclysm changes carry `HWI-FIX`/`HWI-ORACLE` | correct |
| 48 | quit fix: `soundrestore`'s `SDL_CloseAudio` makes quits safe, "6 of 6 oracle quits rc 0" | **wrong**: see finding F1 | wrong |
| 49 | every edited IDENTICAL function has a review entry | **wrong**: `etgEffectDraw` (finding F2) | wrong |
| 50 | DONE WHEN "a static size assert for every struct this port changed: met" | **wrong as stated**: `Universe` changed without any assert (disclosed in the same cell; finding F3) | wrong |
| 51 | "Diff size before the `soundlow.c` fix: 28 files, +3,810 / -1,068 (`src/` 23, +3,635 / -1,042), 134 CAT, 11 HWI-" | **wrong label**: these are exactly the final numbers with `soundlow.c` included (`git diff --shortstat`; `soundlow.c` is one of the 23 `src/` files) (finding F4) | wrong |

**Precision: 47 correct / 51 sampled = 0.92.**

## 3. Findings

**F1 (high, CONFIRMED): the audio-thread fix is dead code on Linux; quits still crashed about 1 in 5.**
`soundrestore` is reached only through `soundEventClose`, and `utyGameSystemsShutdown` calls that only under
`#ifndef _LINUX_FIX_ME` (`src/SDL/utility.c`); `meson.build:163` defines `-D_LINUX_FIX_ME`. Runs: release oracle quits
rc 139 in `audit-r3-closeout-postfix-oracle` and `audit-r3-closeout-relquit-1` (2 of 12 harness release quits incl. the 3 self-test runs
rc 0), debug `AddressSanitizer:DEADLYSIGNAL` rc 1 in `audit-r3-closeout-dbgquit-1` and `-2` (2 of 9 incl. `audit-r3-closeout-oracle`); 1 of 1 under
`strace` (`relquit-2/strace-1.txt`): `SIGSEGV {si_addr=0x787bd1d7cc40}` in thread `SDLAudioP1` 1.2 ms after
`munmap(0x787bcddff000, 67109120)` (the main heap; the address lies inside it), and the `soundstream` thread was also
still alive (`SOUND_STREAM_SLEEP` is 0, so it spins over the stream buffers in the heap). The producer's 6 of 6 was
luck. Fixed (section 5); after the fix 25 of 25 quits rc 0.

**F2 (low, CONFIRMED): `etgEffectDraw` 0x0046ac20 edited without a delta-map review entry.** The map calls it
IDENTICAL/medium (`reviewed: false`); the port changed its test from `EAF_OwnerCoordSys` to `SOF_OwnerCoordSys`
(exe tests `effect->flags & 0x20000000`, HW1 `etg.c:2287` tests 0x01000000). Recorded as IDENTICAL/high, header-level
(precedent: `aitrackForceSteadyShip`'s `SOF_Crazy` entry), with `ranSave`/`ranLoad` header-level entries (27
streams; the report's randy table already calls them changed) and a note on the existing `ranRandomize` entry
(its calibration sample label kept). Stage counts unchanged (re-run: total IDENTICAL 3,161, MODIFIED 1,182).

**F3 (low, disclosed): `Universe` changed without a size assert.** The report says the size-assert row is "met" while
the same cell says `Universe` has none. An absolute pin is not possible yet: ILP32 `totaltimeelapsed` is +0xBC in
src and +0xC0 in the exe (clang -m32 probe, `build/audit-closeout/u32.sh`), an earlier-field difference outside this
port. The audit added the pin that holds: `offsetof(realtimeelapsed) == offsetof(totaltimeelapsed) + 4`.

**F4 (doc): diff-size label.** Section "Files changed" labelled the final numbers "before the `soundlow.c` fix".

Observations (no finding): `ETG.c` belongs to M2-R3 #2, but the two edits are the direct consequence of the `SOF_*`
renumbering and are disclosed; `src/SDL/utility_mac.c` still has `munmap(heap, 0)` but is not in the build
(`build.ninja` has no reference); the `soundrestore` `SDL_CloseAudio` is kept (correct if `soundEventClose` is ever
enabled); the zero-valued retired flags make HW1's cloaked-fighter state machine inert, which matters once
`bCloakedFighter` gets ship code (report "What is left" 2 already assigns it).

## 4. DONE WHEN, re-checked on the plain harness (audit binaries, after the fixes)

| condition | result |
|---|---|
| a quitting harness run returns rc 0 (no `utility.c (3484)` assert) | met after F1's fix: statics, meshes, textures, oracle rc 0; 15 release + 10 debug repeated oracle quits rc 0, LeakSanitizer clean. Before the fix: not reliably (5 of 22 crashed) |
| static size assert for every struct this port changed | met for `ranstream`, `StaticCollInfo`, `AlternativeShips`, `SlotInfo`, `ParadeTypeInfo`, `MilitaryParadeCommand`, `ClampInfo`, `SaveChunk`, `InfoChunkContents`, `CustShipFire`/`StaticCollInfo` offsets; `Universe` only by the audit's adjacency pin (F3) |
| oracle self-test SELFTEST PASS with 27 streams | met (`audit-r3-closeout-final-st`: PASS, sha256 0b1f476e46048a8f, 27 streams) |
| delta-map review entries recorded, stage counts re-run | met (19 + the audit's 3 new entries and 1 note; counts reproduce) |
| fe `Main_game_screen`; sp at least as far as before | met (`audit-r3-closeout-final-fe`, `-final-sp`) |

## 5. Fixes made by the audit (6)

1. `src/SDL/soundlow.c`: new `soundstopthreads()` (`HWI-FIX`): `soundinited = FALSE`, `streamWaitThread()`,
   `SDL_CloseAudio()`; `src/SDL/soundlow.h` prototype.
2. `src/SDL/sstream.c`: `streamStartThread` keeps the thread handle; new `streamWaitThread()` joins it
   (`src/SDL/soundcmn.h` prototype). A first version that only waited for `SOUND_FREE` left the `SDL_Thread` handle
   unjoined, which LeakSanitizer reported (124 B, `audit-r3-closeout-fixquit-dbg-*` rc 1); the join fixes that.
3. `src/SDL/utility.c`: `utyGameSystemsShutdown` calls `soundstopthreads()` in the `_LINUX_FIX_ME` branch, before the
   heaps are freed (includes `soundlow.h`).
4. `src/Game/Universe.h`: static assert `realtimeelapsed` follows `totaltimeelapsed` (F3).
5. `ghidra/delta_map_review.json` / `ghidra/delta_map.json`: `etgEffectDraw`, `ranSave`, `ranLoad` entries and the
   `ranRandomize` note (`tools/delta_map.py mark` + `merge --done m2r3:closeout-audit`; "records changed 4") (F2).
6. `docs/m2-r3-closeout-headers.md`: `[audit: ...]` notes on the quit section, the DONE WHEN rows, the delta-map
   section and the diff-size sentence (F1-F4).

Runs after the fixes: `audit-r3-closeout-fix2quit-rel-1..15` and `-dbg-1..10` rc 0; `audit-r3-closeout-final-{fe,sp,
statics,meshes,textures,oracle}` and `audit-r3-closeout-final-st-*` as in section 4. All run dirs are under
`original/scratch-m2/m2r2-harness/runs/`.

Verdict: PASS-WITH-FIXES - every header value, table, enum and RNG claim checked against the exe holds; the quit fix did not run on Linux (quits crashed ~1 in 5) and is now fixed (25 of 25 quits rc 0), plus one missing delta-map review entry and three doc corrections
