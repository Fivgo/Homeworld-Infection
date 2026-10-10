# Audit: M2-R2 port "oracle" (producer 5)

Audited: `docs/m2-r2-oracle.md` and the uncommitted diff of branch `m2` against HEAD 7de1b18 (13 modified tracked
files, +172/-85, plus the new `src/Game/OracleDump.{c,h}`, `tools/oracle/` and the report). Date 2026-10-10. Ghidra
was not run. The GOG game was not run, and the DLL was never loaded.

Evidence came from these sources:
- Exe: `original/game/cataclysm.exe`, read only with `objdump -d -M intel` and the audit's own PE reader
  `build/audit-oracle/rd.py` (gitignored).
- Ghidra export `ghidra/exports/cataclysm_gog_r5/` (`functions.csv`, `symbols.csv`, `call_graph.csv`), plus
  `ghidra/delta_map.json`.
- The design `docs/m2-checksum-oracle.md` sections 2-5, `src/reference/hw1-ad1bb82/Game/universe.h`, and `src/` at HEAD
  (`git show HEAD:...`).
- Harness runs `original/scratch-m2/m2r2-harness/runs/audit-oracle-*` (plain harness, headless, no input).
- Audit-side tools, independent of the producer's: `build/audit-oracle/perturb.py` (a record-walking byte flipper),
  `fpcw.py`, `flags.py` (counts flags in `compile_commands.json`), `docfix.py`.

## 1. Build from clean

I deleted `original/scratch-m2/build-audit-oracle` and ran `meson setup` there with the arguments of
`tools/build.sh`: debug `--buildtype=debug -Db_sanitize=address,undefined`, release `-Doptimization=2 -Ddebug=false
-Db_sanitize=none`. Then I ran `meson compile` in each. A third configuration, `-Doracle=false`, used the release
arguments.

| build | result | `warning` lines | size before fixes | after fixes |
|---|---|---:|---:|---:|
| debug (audit dir) | rc 0, `[293/293] Linking target hwi-engine` | 0 | 75,806,776 B | 75,806,784 B |
| release (audit dir) | rc 0, `[293/293]` | 0 | 4,672,280 B (sha256 `ab2b5126...`) | 4,672,280 B |
| `-Doracle=false` (audit dir) | rc 0, `[293/293]` | 0 | 4,658,680 B (= the report) | - |
| `build/release` (tree, `tools/build.sh --release`) | rc 0 | 0 | 4,668,184 B (= the report) | 4,668,184 B (sha256 `f68982f5...`) |
| `build/debug` (tree, `tools/build.sh --debug`) | rc 0 | 0 | 75,796,888 B (= the report) | 75,796,896 B |

The audit-dir binaries are larger than the tree's by 4,096 B (release) and about 9.9 KB (debug). meson passes source
paths relative to the build dir, so the audit build embeds `../../../../.claude/worktrees/m2/src/...` where the tree
build embeds `../../src/...` (`strings` on both binaries). This is not a code difference.

Flags (`compile_commands.json`, 249 commands per configuration, counted by `flags.py`):
- every command in all three configurations has `-Werror`, `-fno-strict-aliasing`, `-ffp-contract=off` and
  `-fno-fast-math`, and none has `-ffast-math`;
- release has `-O2` on 249, no `-O3`, no `-g`, and `HW_BUILD_FOR_DISTRIBUTION` on 246; debug has `-O0 -g` on 249;
- `-DHWI_ORACLE=1` is on 246 commands (the 3 without it are kas2c's `parser.c`, `lexer.c` and `kas2c.c`; none of them
  includes `OracleDump.h`). With `-Doracle=false`, `-DHWI_ORACLE=0` is on 246 commands, and the only "oracle" string in
  the binary is the symbol-table file name `OracleDump.c`.

**User decision 2 is met.** The release flags are `-O2 -fno-strict-aliasing`, with no fast-math and FMA contraction
off. 16 Hz is the default: `Universe.c:78` `UNIVERSE_UPDATE_RATE_SHIFT = 0`, every audit stream header says 16
ticks/s, and `dt` is 0x3D800000. `/gok64Hz` gives 64 (run `audit-oracle-64hz`: header 64, `dt` 0x3C800000). In
`main.c` `/vanilla` overrides `/gok64Hz`.

## 2. Writer, record format, self-test, comparison tool, DLL

### 2.1 Writer against design sections 3.4 and 5.1

- **Header.** `hwoPackHeader` writes magic(8), version u16 at 8, producer u8 at 10, mode u8 at 11, options u32 at 12,
  ident1 at 16, ident2 at 20, fpcw u16 at 24, ticks/s u16 at 26 and tag char[36] at 28, 64 bytes in all. These match
  the design's offset table. Mode 5 (self-test) and option bit 8 (perturbed) are additions, and both are documented.
- **`T` payload.** The fields sum to 36 (tick through `rng_all`) + 60 (3 streams) + 2 + 40 (5 ship hashes) + 40 (4 lists) + 64
  (8 players) = 242 bytes (I added them up by hand), and the 52-byte `P` payload
  checks the same way. The field order is the design's with three documented additions: `valid`, `rng_streams`, and a
  separate `ships_health`. `hwo_compare.py` asserts both struct sizes.
- **Emit points.** `ORACLE_UNIV_UPDATE` is used at the 4 `univUpdate` call sites of `universeUpdateTask` (H1), and a
  record is written only when the counter moved. `oracleEmitPacket` is at the end of `netcheckShow`, after
  `netcheckCheckChecksum` (H2). `oracleEmitGameStart` is at the end of `gameStart`, which has no early `return`
  (checked with awk over the function), so the record matches H7's on every path. `univUpdate` itself is untouched.
- **Side effects.** The writer draws no random number; `ranStream` is read only. `univGetChecksum` and
  `statsGetStatChecksum` are called with `netlogfile` set to NULL, and nothing is allocated from the game pools.
- **Finding A1 (defect, fixed).** The hwi-engine `fpcw` recorded `_mm_getcsr() & 0xFFFF`. That includes MXCSR bits
  0-5, which are the sticky exception *status* flags. Every self-test stream carried 0x1FA5, which is the control word
  0x1F80 with IE, ZE and PE set (`fpcw.py`: header and all 513 `T` records). The design (3.3) wants the
  precision/rounding *control* state recorded, and the GOG side's `fnstcw` holds no status bits. A status flag that
  non-simulation code (rendering) sets at a frame-dependent tick would make `fpcw` differ between two otherwise
  identical runs. `hwo_compare.py` compares `fpcw` between files of the same producer, so it would then report a
  spurious divergence (exit 1). The fix masks with `0xFFC0` in `oracleFpControl` (`OracleDump.c`), and the
  `hwo_format.h` comments, the report and design 3.4 are annotated `[audit: ...]`. After the fix the header and every
  `T` record carry 0x1F80. The streams differ from the old ones in exactly 514 bytes: 1 in the header and 1 in each of
  the 513 `T` records.

### 2.2 Self-test, run twice, and perturbation

`tools/oracle/selftest.sh` (2 runs, a perturbed run and a bit-flipped copy), before the fix:

| run | build | result | stream |
|---|---|---|---|
| `audit-oracle-rel1-*` | audit release | SELFTEST PASS (4/4) | 140,723 B, sha256 `2887511c...` = producer's `oracle-final-rel-a` (`cmp` equal) |
| `audit-oracle-rel2-*` | audit release | SELFTEST PASS (4/4) | `cmp` equal to rel1 (A and perturbed) |
| `audit-oracle-dbg1-*` | audit debug (ASan) | SELFTEST PASS, 0 ASan errors, no UBSan report from oracle code | sha256 `211d015a...` = producer's `oracle-final-dbg-a` |

After the fix, with the tree binaries:

| run | build | result | stream |
|---|---|---|---|
| `audit-oracle-fix-rel1-*`, `-fix-rel2-*` | `build/release` | SELFTEST PASS twice | sha256 `4163399b...`, `cmp` equal |
| `audit-oracle-fix-dbg-*` | `build/debug` | SELFTEST PASS, 0 ASan errors | sha256 `4c985187...` |

Debug and release streams differ only in bytes 21-24, the header's `ident2` build-flags hash, both before and after the
fix (`cmp -l`). Comparing them, `hwo_compare.py` prints a `HEADER:` note and then "IDENTICAL: 513 ticks and 256
packets", exit 0.

Perturbations made by the audit's own `perturb.py`, which walks the records itself and changes one byte:

| change | tool output | exit |
|---|---|---|
| `T` tick 200, `statcheck` byte (payload +26) xor 0x10 | FIRST DIVERGENT TICK 200, likely origin `statcheck` | 1 |
| `P` at tick 222, `univcheck` byte xor 1 | T stream identical; FIRST DIVERGENT PACKET 111 (tick 222) `univcheck` | 1 |
| `T` tick 450, `player_ru[1]` xor 2 | FIRST DIVERGENT TICK 450, `players` 35/0 vs 33/0 | 1 |
| file cut at 100,000 B | CAND `(TRUNCATED)`, 149 ticks only in REF, packets 256 vs 182 | 1 |

The producer's perturbations are reproduced: the engine-perturbed run gives tick 300, `rng10`, "CAND is 1 draw(s)
ahead", and first divergent packet 150. The bit-0 flip of `ships_pos` gives tick 400. The tool's `ran_step` is
`Randy.c` `ranRandomFn` (udword arithmetic, `-18`, LCG 69069/1013904243) step for step. The resource-unit claim also
holds: `--dump` shows `ru0` at 10, 35 from tick 256 and 60 at tick 512.

### 2.3 DLL hook-site table (all 7) against the export and the exe

- `python3 tools/oracle/dll/check_sites.py`: **33/33 OK**. That is 3 identity, 7 site byte checks, 6 instruction
  boundaries, 5 copied-prologue checks with no relative branch, and 12 H4 ranges.
- The site names agree with `functions.csv`: H1 0x0054C3F0 `univUpdate`, H2 0x004CC400 `netcheckShow`, H4 0x004EF860
  `ranRandomFn`, H6 0x0052A840 `taskExecuteAllPending` and H7 0x005A9290 `gameStart`. H3 0x0054C57F is
  `FF 15 28 A2 72 00`, i.e. `call [0x0072A228]`. H5 0x00A75248 is `determCompPlayer` in the design's globals table, in
  `.data` past its raw data. The expected bytes equal design table 3.2, including the audited H2 `56 8D 44 24 10 57`.
- The 10 called functions (`CAT_FN_*`) have the right names in `functions.csv`. The DLL's prototypes match the applied
  signatures: `univGetChecksum` returns `real32`, `statsGetStatChecksum` is `(void)`, `feScreenDisappear` takes
  `(char*, featom*)`.
- **H2 ring read.** objdump of `netcheckShow` 0x004CC423-0x004CC478 shows the index computed as `frame & 0x7F`, a
  16-byte entry, +0xC = `[0x00AB7658]`, and the index stored at 0x00C30FA4. The DLL reads exactly this.
- **H3 clock.** objdump 0x0054C585-0x0054C5F6 shows `delta = qpc - [0x00A6D9A0]`. `__aullrem` and `__aulldiv` divide by
  `[0x00AB7520]`, the remainder is kept, and `realtimeelapsed += q / [0x00BB3260]`. So `vqpc` = rem + 15 x div gives
  q = 15 and 15/240 = 1/16 s per tick, as claimed.
- **H6 playback recipe** against objdump of `LoadRecordedGame` 0x00487CF0-0x00487EEA:
  - the directory pointer `[0x00A4372C]` is set to 0x008810BC `"SavedGames\RecordedGames\"` (0x004877FE);
  - the `.pkts` name is built from 0x008812B0;
  - then `VerifySaveFile` (0 = proceed), `feScreenDisappear(0,0)`, `recPackPlayInGameInit`,
    `utyLoadMultiPlayerGameGivenFilename(0x00B39F40)`;
  - then `ebx = 1` is written to 0x00A7519C and 0x00A751A0, and 0x00A75190 is set to 0;
  - the name loop runs i = 1..numPlayers-1 over 0x12-byte names from 0x00C39652 against `sprintf("%s %i",
    *(char**)0x00BB3FB8, i)`.

  All of this matches the DLL. The mode-C directory strings `SavedGames\MultiPlayer\` and `SavedGames\SinglePlayer\`
  are the exe's (0x008810A4 and 0x00881088). `recPackInGameStartCB` is called from 0x004880B4 inside
  `IG_SaveRecordedGame` (objdump), as the report says.
- **No input injection.** The DLL imports 12 kernel32 functions and nothing else (`llvm-objdump -p`). There is no
  user32, no `SendInput`, `keybd_event`, `mouse_event`, `PostMessage` or `SetCursorPos` (grep over both sources). It
  steers the game only through the game's own record/playback/load functions (decision 5).
- **Build (decision 6).** `tools/oracle/dll/build.sh --clean` gives rc 0 and 0 warnings under `-Werror`. The sha256
  values `51de430e...` (DLL) and `9b6480a3...` (loader) are the same as the producer's (`/Brepro`). The DLL entry is at
  RVA 0x1060 and it has a `.reloc` section. The loader has 19 imports. The DLL's disassembly has no SSE or MMX
  register; its x87 instructions are 2 `fnstcw` and 1 `fstps`, which is the `univGetChecksum` rounding at the same
  place `netcheckShow` does it. mingw was not needed.

### 2.4 Cataclysm deltas the oracle needs, checked against the exe

- **16 Hz.** `push 0x3D800000` at 0x0053E8A5, 0x0053E8DB and 0x0053E9B5, each followed by `call 0x54C3F0` (objdump).
  Relic `universe.h:57` has `UNIVERSE_UPDATE_RATE 16`.
- **`univGetChecksum`.** The loop accumulates x at `[esp+0x20]`, y at `[esp+0x1C]` and z at `[esp+0x18]`, with
  `fstp` after each add. The tail at 0x0054A210 is `fld z; fadd y; pop; pop; fadd [esp+0x18]` = x, so the sum is
  (z+y)+x. Rounding happens at the caller's `fstp` 0x004CC411. The `src/` form `(real32)(((real64)z+(real64)y)+(real64)x)`
  is that sum. GoK's derelict and resource loops were inside `#ifdef HW_BUILD_FOR_DEBUGGING` (HEAD lines 5141-5269),
  as claimed.
- **`statsGetStatChecksum`** (objdump 0x00525170-0x0052525F). `ebx` = players+0x35C. The function adds
  `[+0xC]+[+0x4]+[+0x2C]`, then `crc32Compute(+0x30, 0x244)` (one call), then `[+0x35C]`. The CDET `fprintf` (format
  0x00897540) receives +0x2C, +0x4, the CRC, 0xA8 bytes from +0x2A4, and +0x35C, in that order. The dead
  `GetShipStaticInfo(0..0x3A)` loop runs while total < 10.0. `TOTAL_NUM_SHIPS` = 145: I compiled `ShipDefs.h` and
  the value is `.long 145`.
- The report's other cited facts check out: the `/legacy` switch never existed (`docs/RUNBOOK.md:448`), and
  `vanillaGunsMode` is at `Gun.c:1836`.

## 3. Diff review

- **Scope.** Every changed path is on the producer's list. The edit to `docs/m2-r1-bootstrap.md` is three
  `[M2-R2 oracle: ...]` annotations. Nothing outside the oracle's scope changed. No build output, `.hwo` or game data
  is in the tree: `build/` is gitignored, and the `.hwo` files are in `original/scratch-m2/`.
- **Finding A2 (convention, accepted).** `netcheckShow` is **IDENTICAL** (medium) in `delta_map.json`, and it was edited:
  `oracleEmitPacket` was added at the end (`NetCheck.c:967-970`, plus an include). Design 5.1 names this emit point,
  the call sits inside `#if HWI_ORACLE`, it is tagged `HWI-ORACLE:`, and it changes no value `netcheckShow` computes or
  stores. I did not revert it, because the oracle needs this emit point, but it is recorded here as the rule asks.
- **Finding A3 (convention, accepted).** Instrumentation is tagged `HWI-ORACLE:`, not `CAT:`. The report says so
  (line 43), and the porting convention reserves `CAT:` for Cataclysm deltas. Every Cataclysm delta in the diff
  carries `CAT:` with its exe function and address:
  - the 16 Hz default (`Universe.c:76-78`, `Universe.h:46`, `main.c:286`, 903 and 2263);
  - `univGetChecksum` (2 comments);
  - `statsGetStatChecksum`.

  Two edits have no comment of their own. The `/vanilla` help text sits next to the `/gok64Hz` CAT entry. The 4
  `ORACLE_UNIV_UPDATE` call-site edits in `universeUpdateTask` are explained once, at their `#include`. Both are
  traceable, so no fix.
- **Finding A4 (minor, not fixed).** `owner_index` in `hwi_oracle.c` loops `k <= HWO_MAX_PLAYERS`, which tests 9
  slots. A real `playerowner` always equals one of the first 8 players, so there is no observable effect, and the
  exe's player count is not settled (stage 2), so I did not change it.
- **Finding A5 (design deviation, harmless).** The DLL opens the stream with `CREATE_ALWAYS` + `GENERIC_WRITE`. Design
  3.2 said `FILE_APPEND_DATA`. Its purpose, one `WriteFile` per record so that a crash loses at most one record, is
  kept.
- **Finding A6 (defect, fixed).** `tools/m2_harness.sh:176`: `wc -l < FileLoads.log 2>/dev/null` applies the `<`
  redirection before `2>`. Every release-binary run (no `/logFileLoads`) printed `FileLoads.log: No such file or
  directory` to stderr (seen on `audit-oracle-64hz`). The value was right (0). The fix reorders the redirections, and
  the re-run `audit-oracle-harnessfix` is silent: 0 such lines, file loads 0, stream sha256 `4163399b...`.
- **Suggestion.** No C-side static check ties `HWO_TICK_PAYLOAD` to what `hwoPackTick` writes. Today they agree (242,
  verified by hand and by the Python assert on every read). A `_Static_assert` cannot see the packer's byte count, so
  a debug-time check of `hwoPackTick`'s return value would be the guard.

## 4. DONE WHEN, re-checked

| criterion | audit result |
|---|---|
| two hwi-engine runs of the same deterministic scripted start give byte-identical `.hwo` streams (no recording exists: no HW1 data, the GOG recording is the user's open decision 6, and Cataclysm data cannot start a game before stage 2; `audit-oracle-*-sp` was not needed because the producer's `oracle-final-sp` stop at `GenericInterceptor.c (205)` is the known stage-2 stop) | **met**: `cmp` equal for release (twice, two separate self-test invocations, before and after the fix) and debug |
| the comparison tool reports identical | **met**: "IDENTICAL: 513 ticks and 256 packets compared (ticks 0-512)", exit 0 |
| it reports the right tick on a deliberately perturbed copy | **met**: the producer's two perturbations plus the audit's own four (tick 200 `statcheck`, packet 111, tick 450 `players`, truncation) |
| the DLL builds (or the blocker is recorded) | **met**: clang-18 + lld-link, 0 warnings, reproducible bytes; `check_sites.py` 33/33 |
| build green | **met**: audit dirs and `build/` debug + release, 0 warnings, before and after the fixes; `-Doracle=false` too |

Caveat, which the report states itself: the scripted start has no ships. `univcheck` is 0 at every tick, and the
ship, object-list and health hashes stay at their FNV initial values. Only the RNG, the stats checksum, resource
regeneration, `dt`/`total` and the sync check are actually exercised. The ship hashes have been tested only through
bit flips. The first GOG run and stage 2 are what will test them.

## 5. Precision

| # | check | result |
|---:|---|---|
| 1-5 | clean debug / release / `-Doracle=false` builds rc 0, 0 warnings; tree sizes 4,668,184 / 75,796,888 / 4,658,680 as reported | correct |
| 6-8 | flags: `-Werror` and the 3 determinism flags on 249/249, `-O2` without `-g` in release, `HWI_ORACLE` on 246 | correct |
| 9 | 16 Hz default; `/gok64Hz` gives 64; `/vanilla` wins | correct |
| 10-12 | header 64 B and design offsets; `T` 242 B; `P` 52 B | correct |
| 13 | field order = design + documented additions | correct |
| 14-16 | emit points H1 (4 sites), H2, H7 (no early return) | correct |
| 17 | writer draws no RNG, mutes the net log, allocates nothing from pools | correct |
| 18 | `fpcw` records the FP control state | **wrong** (status flags included; fixed) |
| 19-21 | release A/B identical twice, sha `2887511c...` reproduced; debug sha `211d015a...` reproduced; debug vs release differ in `ident2` only | correct |
| 22-23 | perturbed run: tick 300 / `rng10` / 1 draw / packet 150; bit flip: tick 400 / `ships_pos` | correct |
| 24-27 | audit perturbations (statcheck, packet, players, truncation) found | correct |
| 28 | RU 10 -> 35 -> 60 | correct |
| 29 | 64 Hz comparison: tick 0 `dt`, packet 128 `blobcheck` | correct |
| 30 | tool RNG = `Randy.c` generator | correct |
| 31 | `check_sites.py` 33/33 and its count breakdown | correct |
| 32-38 | H1-H7: address/name and expected bytes vs export, exe and design | correct (7) |
| 39 | 10 called functions' names and prototypes | correct |
| 40 | globals vs design 2.1 and `symbols.csv` | correct |
| 41 | H2 ring read | correct |
| 42 | H3 `vqpc` arithmetic (15/240) | correct |
| 43 | H6 replay of `LoadRecordedGame`, mode-C directory strings | correct |
| 44 | no input injection (imports, grep) | correct |
| 45 | DLL/loader build, reproducible hashes, 12/19 imports, entry RVA 0x1060, x87/SSE claim | correct |
| 46 | 16 Hz push addresses; Relic 16 Hz | correct |
| 47 | `univGetChecksum` (z+y)+x and GoK's debug-only loops | correct |
| 48 | `statsGetStatChecksum` offsets, single CRC, CDET order, 145 types | correct |
| 49 | `/legacy`, `Gun.c:1836`, delta classes of the edited functions, run counts (21 lines, 4 defect attempts), new/modified file counts (12/13) | correct |
| 50 | harness handles release binaries cleanly | **wrong** (stderr noise; fixed) |

**Precision = 48 / (48 + 2) = 0.96.**

## 6. Fixes (5)

1. `src/Game/OracleDump.c` `oracleFpControl`: MXCSR `& 0xFFC0`, the control bits only, with an `[audit: ...]` comment
   (A1).
2. `tools/oracle/hwo_format.h`: the two `fpcw` field comments now say what is recorded (A1).
3. `tools/m2_harness.sh:176`: redirection order (A6).
4. `docs/m2-r2-oracle.md`: DONE WHEN row 1 has the reproduced and post-fix stream hashes, and the record-format
   paragraph explains the `fpcw` fix (`[audit: ...]`).
5. `docs/m2-checksum-oracle.md` 3.4, header offset 24: `[audit, M2-R2 oracle: MXCSR & 0xFFC0 ...]`.

After the fixes:
- `tools/build.sh --release` and `--debug` are green with 0 warnings, and so are both audit dirs;
- `selftest.sh` passes twice on release and once on debug, with 0 ASan errors;
- the DLL rebuild has the same sha256 (`hwo_format.h` changed only in comments).

Verdict: PASS-WITH-FIXES - DONE WHEN reproduced (byte-identical self-test streams twice on release and on debug, IDENTICAL from the tool, right tick for the producer's and the audit's own perturbations, DLL builds with clang-18 + lld-link); 7/7 hook sites, 10 called functions and the H2/H3/H6 recipes match the exe and the design; 16 Hz default and -O2/no-fast-math flags confirmed; 2 defects fixed (fpcw recorded MXCSR status flags, harness stderr on release runs), 3 doc annotations; netcheckShow (IDENTICAL) edited for the H2 emit point by design
