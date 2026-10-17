<!-- done: flags, writer, selftest, compare, dll, runs -->
# M2-R2 port: checksum oracle, recreation side, determinism settings, GOG-side DLL skeleton (producer 5)

Status: COMPLETE (2026-10-10), not committed. Branch `m2`, worktree `.claude/worktrees/m2`. Scope:
docs/m2-r1-summary.md "M2-R2 plan", producer 5; design docs/m2-checksum-oracle.md sections 3-6 (with the audit fixes
marked `[audit: ...]` there); user decisions 2, 5, 6 and 7 of 2026-10-10 (cycle/STATE.md "## M2").

Subsystems, in done-marker order: `flags`, `writer`, `selftest`, `compare`, `dll`, `runs`.

**Result.**
- hwi-engine writes the oracle's `.hwo` stream (`/oracleDump <file>`) after every universe tick, every sync-packet
  check and every game start, from the same points the GOG hooks sample. The writer is in the shipping binary
  (`-Doracle`, default on) and inert without the switch. Two Cataclysm deltas were ported so that the checksums it
  records are Cataclysm's: `univGetChecksum` counts ships only and sums `(z+y)+x`, and `statsGetStatChecksum` has one
  CRC.
- Determinism settings: the universe runs at **16 Hz by default** (GoK's 64 Hz only with the new `/gok64Hz`); every
  build carries `-fno-strict-aliasing -ffp-contract=off -fno-fast-math`, and `tools/build.sh --release` is now `-O2`.
- No recording exists (no HW1 data on the machine, the GOG recording is the user's, and Cataclysm data cannot start a
  game before stage 2), so the DONE WHEN ran on the scripted start this item defines (`/oracleSelfTest`: two players,
  no ships, the lockstep branch run by hand). Two runs give byte-identical streams (release and debug). A debug and a
  release stream differ only in the header's build identity. `hwo_compare.py` says IDENTICAL for the pairs, finds
  tick 300 / `rng10` / "1 draw ahead" on a run perturbed by one extra `gamerand()`, and tick 400 / `ships_pos` on a
  copy with one bit flipped (`tools/oracle/selftest.sh`: SELFTEST PASS, release and debug).
- The GOG-side DLL skeleton and its loader **build with clang-18 + lld-link** (mingw not needed). It has the H1-H7
  table, the attach check that refuses to patch on any byte difference, the `.hwo` writer and the calls into the
  game's own record and playback functions. `check_sites.py` checks the table against the exe: 33/33 checks pass. The
  DLL was never loaded.
- Harness: debug `fe` still reaches `Main_game_screen` and debug `sp` still stops at the stage-2
  `GenericInterceptor.c (205)`, so nothing regressed. ASan reports 0 errors in every debug run.

**DONE WHEN.**

| criterion | state | evidence |
|---|---|---|
| two hwi-engine runs of the same HW1 recorded game, or (no recording available) the same deterministic scripted start, give byte-identical .hwo streams | **met, on the scripted start** (no recording exists; why: `selftest`) | `cmp` equal: `oracle-final-rel-a`/`-b` (release, 140,723 B, sha256 `2887511c...`) and `oracle-final-dbg-a`/`-b` (debug ASan, sha256 `211d015a...`); also `oracle-r2-*`, `oracle-r3-rel-*` [audit: reproduced byte for byte (`audit-oracle-rel1-*`, `-rel2-*`, `-dbg1-*`). After the audit's `fpcw` fix (MXCSR control bits only, see `writer`) the streams are release sha256 `4163399b...` and debug `4c985187...` (`audit-oracle-fix-rel1-*`, `-fix-rel2-*`, `-fix-dbg-*`, all SELFTEST PASS); only the header `fpcw` and the 513 `T` `fpcw` bytes changed (514 bytes)] |
| the comparison tool reports identical | **met** | `hwo_compare.py`: "IDENTICAL: 513 ticks and 256 packets compared (ticks 0-512)", exit 0, for both pairs and for debug against release (with one `HEADER:` note: the build identity differs) |
| it reports the right tick on a deliberately perturbed copy | **met** | (a) engine-perturbed run (`/oracleSelfTestPerturb 300`): "FIRST DIVERGENT TICK 300 (last equal tick 299) ... likely origin: rng10 ... CAND is 1 draw(s) ahead of REF", first divergent packet 150 at tick 300, exit 1; (b) copy with bit 0 of `ships_pos` flipped at tick 400: "FIRST DIVERGENT TICK 400 ... likely origin: ships_pos", exit 1. Release and debug |
| the DLL builds (or the blocker is recorded) | **met** | `tools/oracle/dll/build.sh --clean`: `hwi_oracle.dll` 23,040 B + `hwi_oracle_loader.exe` 5,120 B, PE32 i386, 0 warnings under `-Werror`, reproducible (`/Brepro`); `check_sites.py` 33/33 |
| `tools/build.sh --debug` and `--release` green, 0 warnings under `-Werror` | **met** | both `--clean`: rc 0, 0 `warning:` lines; also `-Doracle=false` (0 warnings, no oracle strings in the binary) |

Exe evidence: `ghidra/exports/cataclysm_gog_r5/decompiled.c` (read with a scratch extractor,
`build/oracle-tools/dec.py`, gitignored) and `objdump -d -M intel` on `original/game/cataclysm.exe` (read-only, never
run). Addresses are the exe's VAs. Instrumentation that is not a Cataclysm delta is tagged `HWI-ORACLE:` in the code;
Cataclysm deltas are tagged `CAT:` as the porting conventions ask.

## Subsystem `flags` (determinism settings, user decision 2)

| file | change | why / evidence |
|---|---|---|
| `src/Game/Universe.c` | `UNIVERSE_UPDATE_RATE_SHIFT` default 2 -> **0**: the universe runs at **16 Hz** (`UNIVERSE_UPDATE_PERIOD` = 1/16 s) | CAT: `universeUpdateTask` 0x0053e460 calls `univUpdate` 0x0054c3f0 with `push 0x3D800000` (= 0.0625f) at 0x0053e8a5, 0x0053e8db and 0x0053e9b5 (objdump); Relic HW1 `universe.h:57-58` is 16 Hz too. User decision 2: 16 Hz default |
| `src/Game/Universe.h` | comment of `UNIVERSE_UPDATE_RATE_SHIFT`: 0 by default, 2 only with `/gok64Hz` (the old comment named a `/legacy` switch that never existed, docs/RUNBOOK.md:448) | doc |
| `src/SDL/main.c` | new switch **`/gok64Hz`** (`gok64HzMode`): sets the shift to 2 (GoK's 64 Hz) after the command line is parsed, unless `/vanilla` is also given; `/vanilla` keeps setting 0 and still turns GoK's gun-gimbal change off (`vanillaGunsMode`, Gun.c:1836); its help text says so | "GoK's 64 Hz only behind an option" |
| `meson.build` | `determinism_args` = `-fno-strict-aliasing -ffp-contract=off -fno-fast-math`, added to every build type (`c_base_args`, so `lib_sdl`, the CRC/LZSS libraries and the executable all get them); a `-Dc_args` containing `fast-math`, `-Ofast`, `fp-contract=fast` or `unsafe-math` stops the configure with an error | docs/m2-checksum-oracle.md 6.1 row "fused multiply-add and fast-math": the exe never fuses `a*b+c`; GoK's meson.build set none of these flags |
| `tools/build.sh` | `--release` = `-Doptimization=2 -Ddebug=false -Db_sanitize=none` (was `--buildtype=release`, i.e. **-O3**). `--buildtype=release` cannot be combined with `-Doptimization=2`: meson lets the build type win (tested on a fresh setup: optimization stays 3). With the two options alone meson labels the build type `debug` (fresh) or keeps the old label (reconfigure); the effective options are what count | user decision 2: "-O2 -fno-strict-aliasing, no fast-math" for the release build oracle runs use |
| `meson.build`, `meson_options.txt` | **`-Doracle`** (boolean, default `true`): defines `HWI_ORACLE=1` and generates `hwi_oracle_build.h` (`HWI_GIT_HASH` = `git rev-parse --short=8 HEAD`, `HWI_BUILD_FLAGS` = compiler, version, CPU family, optimization, debug, sanitizer, the determinism flags, `-Dc_args`, the `HW_BUILD_FOR_*` define); `false` defines `HWI_ORACLE=0` and the oracle code compiles to nothing. A non-debug build at an optimisation other than 2 gets a configure warning | design 5.1: the writer is compiled into the shipping binary and switched on at run time, so the oracle tests the binary that ships; the switch exists for anyone who wants it out |

Clean release build (`tools/build.sh --release --clean`): rc 0, 0 warnings, all 249 compile commands carry `-O2
-fno-strict-aliasing -ffp-contract=off -fno-fast-math` and no `-g`; meson prints `Oracle build identity: 7de1b18e /
gcc 13.3.0 x86_64 optimization=2 debug=false sanitize=none -fno-strict-aliasing -ffp-contract=off -fno-fast-math
-DHW_BUILD_FOR_DISTRIBUTION`. `build/release/hwi-engine` 4,668,184 B (the -O3 bootstrap binary was 5,157,976 B).

What 16 Hz changes: every `UNIVERSE_WOODPECKER` rate and `UNIVERSE_UPDATE_RATE_FACTOR` use reverts to Relic's
timing, which is exactly the path GoK's own `/vanilla` already took (the only other effect of `/vanilla`, the gun
gimbal reset, is untouched). Not changed here, still GoK behaviour, each for the subsystem that owns it: save version
0x10ad0029, turbo factor 8, the SSE build itself (x86-64 has no x87 code path to choose).

## Subsystem `writer` (the .hwo stream, design sections 3.4 and 5.1)

| file | change |
|---|---|
| `tools/oracle/hwo_format.h` (new) | the record format shared by both producers: 64-byte header, then `type u8, reserved u8, length u16, payload` records `T` (tick, 242 B), `P` (sync packet, 52 B), `E` (event, 8 B + text); field-by-field little-endian packers (`hwoPackHeader/Tick/Packet/Event`), FNV-1a-64 and bitwise CRC-32 helpers. Header-only, `<stdint.h>`/`<stddef.h>` only, so the freestanding 32-bit DLL uses the same code as the 64-bit engine. Layout table below |
| `src/Game/OracleDump.c`, `.h` (new) | the hwi-engine producer: opens the stream on first use (header: producer 2, mode, options, git hash, FNV-1a-32 of `HWI_BUILD_FLAGS`, MXCSR, `UNIVERSE_UPDATE_RATE`, tag), `oracleUnivUpdate` (= `univUpdate` + a `T` record when `universe.univUpdateCounter` moved, like hook H1), `oracleEmitPacket` (`P`, like H2), `oracleEmitGameStart` (`E` + tick-0 `T`, like H7), and the self-test (subsystem `selftest`). One `fwrite` + `fflush` per record, so a killed process loses at most the record being written. It draws no random number, allocates nothing from the game's pools, and calls `univGetChecksum`/`statsGetStatChecksum` with `netlogfile` set to NULL for the call (both write to the net log) |
| `src/Game/Universe.c` | the 4 `univUpdate(UNIVERSE_UPDATE_PERIOD)` calls of `universeUpdateTask` (lockstep pair, turbo loop, free-running) go through `ORACLE_UNIV_UPDATE`, which is `univUpdate` itself when `HWI_ORACLE` is 0. Wrapping the call sites keeps `univUpdate` as ported (design 5.1) [audit M2-R3 universe-mex: 3 call sites since that port, as in the exe: the free-running turbo loop is gone] |
| `src/Game/NetCheck.c` | end of `netcheckShow`: `oracleEmitPacket(packet, packetnum, randcheck, univcheck, blobcheck)`, after `netcheckCheckChecksum`, so the record carries this packet's sync-error counters |
| `src/SDL/utility.c` | end of `gameStart` (after `gameIsRunning = TRUE`): `oracleEmitGameStart(loadfilename)`; end of `utyGameSystemsInit`: `/oracleSelfTest` runs the self-test and quits, next to the other port self-tests |
| `src/SDL/main.c` | switches `/oracleDump <file>`, `/oracleSelfTest <ticks>`, `/oracleSelfTestPerturb <tick>` (inside `#if HWI_ORACLE`) |
| `src/Game/meson.build` | `OracleDump.c` |
| `src/Game/UnivUpdate.c` | **CAT** `univGetChecksum`: live ships only, sum `(real32)(((real64)z + (real64)y) + (real64)x)` (below) |
| `src/Game/Stats.c` | **CAT** `statsGetStatChecksum`: one CRC (below) |

Cataclysm deltas in this subsystem (the oracle compares the game's own checksums, so they must be Cataclysm's):

| function | exe | change in `src/` | evidence |
|---|---|---|---|
| `univGetChecksum` | 0x00549f50 (MODIFIED, high) | the derelict and resource loops are gone (GoK had them inside `#ifdef HW_BUILD_FOR_DEBUGGING`, so debug and release builds even disagreed); the result is `(z + y) + x` in double, rounded to float once | decompiled.c:297865-297936 has only the `ShipList` loop (flag test `& 0x800`, positions +0x58/+0x5C/+0x60); objdump 0x0054a210-0x0054a221: `fld [esp+0x18]; fadd [esp+0x1c]; ... fadd [esp+0x18]` (after two pops), no store between the adds, rounded by the caller's `fstp` (netcheckShow 0x004cc411). At 53-bit precision control each x87 add rounds like a C `double` add (design 5.1). The per-ship verbose net-log lines keep GoK's (BINNETLOG) format: logging only, not ported |
| `statsGetStatChecksum` | 0x00525170 (MODIFIED, high) | the second CRC (Relic meant `classtotals`, hashed `shiptotals` again) is gone; `classtotals` is logged as 0 | objdump 0x005251bd-0x005251eb: per player `[+0x4] + [+0xC] + [+0x2C] + crc32Compute(player+0x30, 0x244) + [+0x35C]`, one `crc32Compute` call (0x244 = 145 x 4, `TOTAL_NUM_SHIPS` is 145 since the identifiers item). The CDET log call (0x00525203-0x0052522e) prints +0x2C, +0x4, the CRC, the 0xA8-byte block at +0x2A4 and +0x35C, which matches HW1's totalships, resourceUnits, shiptotals CRC, research info, so +0x4 = resourceUnits and +0x2C = totalships; **+0xC and +0x35C are UNVERIFIED** and HW1's `HasTechnology` and `listoftopics.num` stay until the stage-2 Player layout (0x6530 bytes in the exe) names them. The exe's dead loop (`GetShipStaticInfo(0..58)` while `totaltimeelapsed < 10`, result discarded, 0x00525188-0x005251a3) is not ported: no effect |

Not changed (and why): `netcheckShow` itself (IDENTICAL by the delta map, medium) keeps GoK's 12-byte
`NetSyncChecksums`. The exe stores a 4th dword (a copy of 0x00AB7658) in a 16-byte entry and its packet header is
0x1C bytes (design 2.4), but that is the LAN wire format, and user decision 7 says not to spend effort on interop and
not to change the wire format gratuitously; the `P` record's `extra` field is 0 on this side (flag `HWO_PF_EXTRA`
clear). The RNG keeps GoK's 25 streams (`Randy.c`, IDENTICAL by the delta map; the exe has 27 and seeds stream 0 from
`GetSystemTime().wSecond` in `ranStartup` 0x004efa30, design 2.6): the `T` record says how many streams `rng_all`
covers (`rng_streams`), so the comparison tool knows when `rng_all` is not comparable. Cataclysm's `realtimeelapsed`
(design 2.5) does not exist in hwi-engine yet, so `real` is 0 and its valid bit is clear.

### Record format as implemented (`tools/oracle/hwo_format.h`)

Header, 64 bytes: magic `HWIORCL1`; u16 version 1; u8 producer (1 GOG DLL, 2 hwi-engine); u8 mode (1 playback, 2
save-run, 3 record, 4 observe, 5 self-test); u32 options (bit0 clock=virtual, bit1 rng0=split, bit2 determCompPlayer,
bit8 deliberately perturbed self-test); u32 ident1 (GOG: exe `TimeDateStamp`; hwi-engine: commit hash); u32 ident2
(GOG: `SizeOfImage`; hwi-engine: FNV-1a-32 of the build-flags string); u16 x87 control word or MXCSR [audit: MXCSR `& 0xFFC0`, the
control bits only. The writer first stored the low 16 bits, which include the sticky exception flags (bits 0-5): the
self-test streams carried 0x1FA5 = 0x1F80 | IE | ZE | PE. Status flags set by non-simulation code at a frame-dependent
tick would make `fpcw` differ between two otherwise identical runs, and the GOG side's `fnstcw` holds no status bits.
Fixed in `oracleFpControl`; the per-tick `fpcw` below is the same value]; u16 ticks per
second; char[36] tag.

`T` payload (242 bytes), in order: u32 tick (`univUpdateCounter`); u32 dt, total, real (f32 bits); u16 fpcw; u16 valid
bits (real, ships_misc, health, lists, players, checksums); u32 univcheck (f32 bits); u16 nships; u32 statcheck; u16
rng_streams; u32 rng_all (CRC-32 over every stream's x,y,z,c,n); 3 x 20 B raw streams 10 (`gamerand`), 6 (AI), 0;
u16 ships_n; u64 ships_pos, ships_vel, ships_rot, ships_misc, ships_health (FNV-1a-64 per live ship in `ShipList`
order, each starting with the u16 ship ID; misc = type u32, race u32, owner u16, command order u16, attributes u16,
0xFFFF when none); 4 x (u16 count, u64 position hash) for missiles, bullets, derelicts, resources (live objects, list
order); 8 x (u32 resourceUnits, u32 totalships). Changes from the design's table: `ships_health` is its own hash (so
`ships_misc` stays comparable while the exe's health offset, +0x170 by the code or 0x158 by CET, is unresolved), and
the `valid` bits and `rng_streams` were added so each producer says what it could fill.

`P` payload (52 bytes): u32 tick, u16 packetnum, u16 randcheck, u32 univcheck, u32 blobcheck, u32 extra, u16
ncommands, u16 flags, 4 x u32 sync-error counters (rand, univ, blob, pkt), 3 x u32 first-error frames.

`E` payload: u32 tick, u16 code (1 new game, 2 load, 3/4 record start/stop, 5/6 playback start/end, 7 quit, 8/9
self-test start/end, 10 hooks, 11 offset check, 12 note, 13 perturbation), u16 text length, text.

## Subsystem `selftest` (the deterministic scripted start)

No recording exists to drive two runs: there is no HW1 data on the machine (the HW1-named files in
`original/scratch-m2/hw1-assets/` are symlinks to Cataclysm's archives), the first GOG recording is the user's
(decision 6 of the M2 list, still open), and on Cataclysm data hwi-engine cannot start a game before stage 2 (single
player stops in `GenericInterceptorStaticInit`, run `startup-final2-sp`). The design names the acceptance ladder (4.4)
and the staged comparison (5.2) but no recreation-side scripted start, so this item defines one, `/oracleSelfTest
<ticks>` (`oracleRunSelfTest`, `src/Game/OracleDump.c`), run at the end of `utyGameSystemsInit` like the other port
self-tests:

1. the resets `gameStart` does for a new game (`univupdateReset`, `ranStartup`) and a fresh sync ring
   (`netcheckReset`);
2. two players with no ships, set up as `gameStart` sets up a new non-single-player game (`resourceUnits` 10 = the
   default `startingResources` branch, `PLAYER_ALIVE`, no AI); `tpGameCreated.numPlayers` 2, so the stats checksum
   covers them;
3. the lockstep branch of `universeUpdateTask` by hand, per sync packet: `netcheckFillInChecksum` (the captain's copy
   of its last checksum), `netcheckShow` (the game's `univGetChecksum`, `statsGetStatChecksum`, one `gamerand()`, the
   checksum check), then two `univUpdate(UNIVERSE_UPDATE_PERIOD)` through `ORACLE_UNIV_UPDATE`;
4. an `E` record at start and end (the end record carries the game's own sync-error counters) and a tick-0 `T`.

What it exercises: the writer, the record format, `univUpdate` on an empty universe (collision blobs, construction
manager, research, resource regeneration of non-single-player games every 256 ticks at 16 Hz: `resourceUnits` 10 ->
35 at tick 256 -> 60 at tick 512, bounties), the RNG (stream 10 moves once per packet), the stats checksum, and the
game's own sync check (0 errors). What it cannot exercise: ships, objects, AI, commands. Those need stage 2 (save
loader, ship code) and then the GOG recording (design 5.2, stages 1-3).

`/oracleSelfTestPerturb <tick>` draws one extra `gamerand()` just before that tick and writes an `E` record of code 13;
the header gets option bit 8, so a perturbed stream can never pass for a reference.

## Subsystem `compare` (`tools/oracle/hwo_compare.py`, `tools/oracle/selftest.sh`)

`hwo_compare.py REF.hwo CAND.hwo [--from-tick N] [--to-tick M] [--fields ...] [--max-draws K] [--json OUT]`, Python 3
standard library only, layout mirrored from `hwo_format.h` (the script asserts the 242/52-byte payload sizes):

1. headers: ticks per second, option bits; for two files of the same producer also the build identity and the FP
   control word. Differences are printed first (`HEADER:`) and the comparison continues;
2. `T` records joined on the tick (ticks in only one file are listed and count as a divergence);
3. per tick, fields in causal order: `rng10`, `rng6`, `rng0`, `rng_all`, `ships_n`, `ships_misc`, `ships_pos`,
   `ships_vel`, `ships_rot`, `ships_health`, `missiles`, `bullets`, `derelicts`, `resources`, `statcheck`,
   `univcheck` (+`nships`), `players`, `dt`, `total`, `real`, `fpcw`. At the first tick with a difference it prints
   every differing field (REF and CAND values) and flags the first one as the likely origin. For an RNG stream it runs
   the game's generator (`ranRandomFn`, integer-only, ported to Python) forward from the reference state, and from
   the candidate's, up to `--max-draws` (1,000) draws: "CAND is N draw(s) ahead of/behind REF" or "not the same
   stream shifted";
4. `P` records joined on (packetnum, occurrence): the first packet whose `randcheck`/`univcheck`/`blobcheck` differ,
   and any non-zero sync-error counter of the game's own check in either file;
5. a field is compared only when both files fill it (valid bits; `rng_all` only over the same stream count; `fpcw`
   only within one producer); skipped fields are listed ("not compared");
6. `E` records: a difference is reported but is not a divergence by itself.

Exit code 0 identical, 1 diverged, 2 unusable input; `--json` writes `{first_divergent_tick, fields, likely_origin,
last_equal_tick, ticks_compared, packets_compared, first_divergent_packet, sync_errors, rng_draws, verdict, ...}`.
Also `--dump FILE` (one line per record) and `--perturb-copy IN OUT --tick T --field F [--record T|P]` (a copy with
bit 0 of one field flipped, to test the tool).

`tools/oracle/selftest.sh [--bin PATH] [--label L]` is this item's acceptance test: two harness runs of the scripted
start (byte compare + tool), a run perturbed before tick 300 (the tool must say tick 300, `rng10`, 1 draw ahead) and a
copy of run A with `ships_pos` flipped at tick 400 (the tool must say tick 400, `ships_pos`); PASS/FAIL per step and a
non-zero exit on any failure. Results are in `runs` below.

Harness (`tools/m2_harness.sh`, extended): `--mode oracle` (`/oracleSelfTest <ticks> /oracleDump <run>/run.hwo`,
milestone `oracle self-test done`), `--oracle-ticks N`, `--perturb T`, `--hwo` (any other mode also writes
`<run>/run.hwo`); `--bin` now takes a relative path (it was passed unresolved to a process whose cwd is the run's
settings dir: rc 127, runs `oracle-r2-release-*` first attempt); a release binary gets no `/logFileLoads`
(debug-only switch: the release engine refused the whole command line).

## Subsystem `dll` (GOG-side hook DLL skeleton, decisions 5 and 6)

Toolchain (decision 6): **clang-18 + lld-link worked; mingw-w64 was not needed** (and is not installed). Ubuntu clang
18.1.3 `--target=i686-pc-windows-msvc`, Ubuntu LLD 18.1.3 (`lld-link`), `llvm-dlltool-18` for the kernel32 import
library. No Windows SDK and no CRT: the sources declare the 25 kernel32 functions they use and provide `memset`,
`memcpy` and `_fltused` themselves.

| file (all new, `tools/oracle/dll/`) | what |
|---|---|
| `hook_sites.h` | the exe identity (ImageBase 0x00400000, TimeDateStamp 0x3A4265AA, SizeOfImage 0x894000); the H1-H7 table as an X-macro `X(id, name, va, kind, patch_len, "expected bytes")`; the addresses of the 10 game functions and ~50 globals the DLL reads or calls, with their evidence; the ship field offsets; the 12 H4 split ranges |
| `hwi_oracle.c` | the DLL. `DllMain(PROCESS_ATTACH)`: parse `HWI_ORACLE`; refuse (log only) unless the module at 0x00400000 has the PE TimeDateStamp and SizeOfImage of build 12948; open `<out>` and `<out>.log`; compare **every** site's bytes with the table, and on any mismatch write `E hooks "REFUSED..."`, log the first differing address and patch nothing; else install. Entry hooks copy their first 5 bytes into a VirtualAlloc'd trampoline + `jmp` back and patch `jmp rel32`; H3 is `call rel32; nop`; H5 a dword write. Detours: H1 `univUpdate` -> `T` record when the counter moved, plus the record-mode start (`recPackInGameStartCB("SavedGames\\RecordedGames\\NAME")` at the first tick) and stop (`recPackInGameStopCB` at tick `stop`), plus the end-of-playback check (0x00A6C5C8 -> `E`, flush, `ExitProcess(0)`); H2 `netcheckShow` -> `P` from the ring entry the call just wrote (incl. the 4th field and `numberOfCommands` at packet +0x18); H3 `vqpc`; H4 private stream 0 for the 12 caller ranges; H5 `determCompPlayer = 1` in record mode; H6 one-shot start on the main thread once `gameIsRunning == 0 && startingGame == 0` held for `menuwait` calls; H7 `gameStart` -> `E` + tick-0 `T` |
| `hwi_oracle_loader.c` | CET's loader with the design's two fixes: `CreateProcessA(cataclysm.exe, CREATE_SUSPENDED)` from the loader's own directory, the DLL path written into the child, `CreateRemoteThread(LoadLibraryA)`, **its exit code checked** (0 -> the child is terminated, loader exit 105), arguments after argv[0] forwarded untouched; then resume and wait; exit code = the game's |
| `kernel32.def` | the 25 imports, i386-decorated (`CreateFileA@28` ...); `llvm-dlltool -k` strips the decoration from the import names |
| `build.sh` | builds `build/oracle-dll/hwi_oracle.dll` and `hwi_oracle_loader.exe`: `-ffreestanding -nostdlib -O2 -march=i686 -mno-sse -mno-mmx -Wall -Wextra -Werror`, `lld-link /dll /entry:DllMain /nodefaultlib /Brepro` |
| `check_sites.py` | offline check of `hook_sites.h` against `original/game/cataclysm.exe` (read-only): PE identity, the expected bytes of every site, that each patch ends on an instruction boundary and copies no relative branch (objdump), the H5 data site lies in zero-filled `.bss`, and the 12 H4 ranges equal `functions.csv` |

Calls into the game's own record/playback functions (decision 5; no OS input anywhere):

| mode | calls, in order | evidence |
|---|---|---|
| `record:NAME` | at the first tick, `recPackInGameStartCB("SavedGames\\RecordedGames\\NAME")` (0x004CBAF0); at tick `stop`, `recPackInGameStopCB()` (0x004CBD10). H5 sets `determCompPlayer` | HW1 `netcheck.c:526-587` (the start callback only stores the names and sets `startRecordingGameWhenSafe`; `universeUpdateTask` saves and starts at its safe point); `IG_SaveRecordedGame` 0x00487EF0 calls the same function |
| `play:NAME` | `LoadRecordedGame` 0x00487CF0 replayed: names into 0x00B39F40 / 0x00B39F00 (+`.pkts`, string 0x008812B0), `VerifySaveFile` (0x004FDF40, 0 = OK, else `E note` and nothing started), `feScreenDisappear(NULL, NULL)` (0x00475200), `recPackPlayInGameInit()` (0x004CC010), `utyLoadMultiPlayerGameGivenFilename(save)` (0x005AAA50), then `[0x00A7519C] = 1`, `[0x00A751A0] = 1`, `[0x00A75190] = 0`, set to 1 if a player name differs from `sprintf("%s %i", *(char **)0x00BB3FB8, i)` (the game's sprintf 0x00670B30) | objdump 0x00487CF0-0x00487EEA (read for this item); the audit fix of design 4.3 |
| `load:mp:NAME` / `load:sp:NAME` (mode C) | `VerifySaveFile`, `feScreenDisappear`, `utyLoadMultiPlayerGameGivenFilename` (0x005AAA50) or `utyLoadSinglePlayerGameGivenFilename` (0x005AA5F0). UNVERIFIED that the front end needs nothing more | design 4.1 row C |

H3, as built: the code after the `QueryPerformanceCounter` call adds `floor((qpc - [0x00A6D9A0]) / [0x00AB7520]) /
[0x00BB3260]` to `realtimeelapsed` and keeps the remainder (objdump 0x0054C585-0x0054C5F6: `__aullrem` 0x00678070,
`__aulldiv` 0x006761C0, `fdiv [0x00BB3260]`). `vqpc` returns `[0x00A6D9A0] + 15 x [0x00AB7520]` while
`taskFrequency` is 240.0 (bits 0x43700000), so each tick adds exactly 15/240 = 1/16 s; any other value falls back to
the real counter with one log line. Integer only (the compiler emitted `mul` + `lea`, no runtime call).

Build results (`tools/oracle/dll/build.sh --clean`, twice): rc 0, no warnings under `-Werror`;
`hwi_oracle.dll` 23,040 B, PE32 DLL i386, entry `_DllMain@12` at RVA 0x1060, 12 kernel32 imports (undecorated);
`hwi_oracle_loader.exe` 5,120 B, PE32 i386, 19 kernel32 imports. With `/Brepro` both clean builds give the same bytes
(sha256 `51de430e...` and `9b6480a3...`; without it the link timestamp changed them). The object's disassembly has 0
SSE/MMX instructions and exactly one x87 instruction (`fstp dword` storing `univGetChecksum`'s result, the rounding
`netcheckShow` does at 0x004CC411) besides `fnstcw`; `det_univUpdate` passes the float argument as a pushed dword and
`vqpc` ends in `ret 4` (stdcall, like `QueryPerformanceCounter`). `check_sites.py`: **all 33 checks pass** (3
identity, the bytes of the 6 code sites + the `.bss` data site, 6 instruction boundaries, 5 copied-prologue checks,
12 H4 ranges). The DLL was never loaded into the game (the user runs the first live session).

Not in the skeleton, by design or for later: no `S` snapshot records (section 6.2 fallback); `ships_health` and
`players` are filled but their valid bits stay clear until stage 2 settles the Ship health offset (+0x170 by the code,
0x158 by CET) and the 0x6530-byte Player layout (+0x4 resourceUnits and +0x2C totalships are inferred from the CDET log
order, see `writer`); the object lists (+0xFC/+0x108/+0x114/+0x12C, from CET) are checked structurally every tick
(each node's `belongto` must be the list, the walk length must equal the list's `num`) and switched off for the run
on the first mismatch; the first-tick offset self-test (`E` code 11) checks that every `ShipList` node has objtype 0
and that the live count equals `univGetChecksum`'s. A bad pointer in the (unverified) list offsets would still fault:
there is no SEH without a CRT.

## Subsystem `runs` (builds and harness runs)

Builds (final tree):

| build | command | result |
|---|---|---|
| debug | `tools/build.sh --debug --clean` | rc 0, 0 warnings, `[293/293] Linking target hwi-engine`, 75,796,888 B; identity `7de1b18e / gcc 13.3.0 x86_64 optimization=0 debug=true sanitize=address,undefined -fno-strict-aliasing -ffp-contract=off -fno-fast-math -DHW_BUILD_FOR_DEBUGGING` |
| release | `tools/build.sh --release --clean` (twice) | rc 0, 0 warnings, 4,668,184 B, sha256 `5c292c89...0aed936` both times; `-O2` on all 249 compile commands |
| oracle compiled out | `meson setup -Doracle=false -Doptimization=2 -Ddebug=false -Db_sanitize=none` (scratch dir `build/oracle-tools/nooracle`) | rc 0, 0 warnings, 4,658,680 B, `-DHWI_ORACLE=0` on 246 commands, no `oracleDump` string in the binary |
| GOG DLL + loader | `tools/oracle/dll/build.sh --clean` (twice) | rc 0, 0 warnings, the same bytes both times (`dll` subsystem) |

Harness runs (`tools/m2_harness.sh`, plain harness = only what a GOG install has, headless offscreen, no input; output
in `original/scratch-m2/m2r2-harness/runs/<label>/`). "First stop" for every debug run that quits on its own is GoK's
`munmap(heap, 0)` assertion at `utility.c (3484)` during the shutdown, the same as in every earlier R2 self-test run
(data-deltas 5.4), not a data stop.

| label | binary | mode | milestone | first stop | notes |
|---|---|---|---|---|---|
| `oracle-r1-selftest` | debug | oracle, 512 ticks | oracle self-test done | shutdown assert | first run of the writer: 772 records (1 header, 2 `E`, 513 `T`, 256 `P`), 140,723 B, 0 ASan |
| `oracle-r2-debug-a`, `-b` | debug | oracle | oracle self-test done | shutdown assert | byte-identical, sha256 `211d015a...` |
| `oracle-r2-release-a`, `-b` (1st and 2nd attempts) | release | oracle | process started | none (rc 127, then rc 0) | harness defects found and fixed: relative `--bin` (rc 127), then the release binary rejected `/logFileLoads` |
| `oracle-r2-release-a`, `-b` (3rd) | release | oracle | oracle self-test done | none (rc 0) | byte-identical, sha256 `2887511c...`; against debug only header bytes 21-24 differ (`ident2`, the build-flags hash) |
| `oracle-r3-rel-a`, `-b`, `-perturb` | release | oracle (+ perturb 300) | oracle self-test done | none | `selftest.sh`: SELFTEST PASS |
| **`oracle-final-rel-a`, `-b`, `-perturb`** | release, final | oracle (+ perturb 300) | oracle self-test done | none (rc 0) | **`selftest.sh`: SELFTEST PASS** (output `build/oracle-tools/selftest-final-rel.txt`) |
| **`oracle-final-dbg-a`, `-b`, `-perturb`** | debug, final | oracle (+ perturb 300) | oracle self-test done | shutdown assert | **`selftest.sh`: SELFTEST PASS**, 0 ASan errors in all three |
| `oracle-final-rel-64hz` | release | oracle, `/gok64Hz` | oracle self-test done | none | against the 16 Hz stream: `HEADER: ticks per second differ: 16 vs 64`, first divergent tick 0 (`dt` 0x3D800000 vs 0x3C800000), first divergent packet 128 (`blobcheck`: the RU regeneration keeps its 16 Hz cadence, so at 64 Hz it falls at tick 1024) |
| `oracle-final-fe` | debug | fe, 40 s | **Main_game_screen** | shutdown assert after the timeout | unchanged from `startup-final3-fe`; 0 ASan |
| `oracle-final-sp` | debug | sp + `--hwo` | level load (mission file read) | `GenericInterceptor.c (205)` | unchanged stage-2 stop; no `.hwo` written (the stream opens on first use, and `gameStart` never returns here) |

Reproduce: `tools/build.sh --release && tools/oracle/selftest.sh` (and `--bin build/debug/hwi-engine --label X` for
the debug build); `python3 tools/oracle/dll/check_sites.py`; `tools/oracle/dll/build.sh`.

## Files changed

| file | change |
|---|---|
| `src/Game/Universe.c`, `Universe.h` | 16 Hz default (CAT); `ORACLE_UNIV_UPDATE` at the 4 `univUpdate` call sites [audit M2-R3 universe-mex: 3 since that port]; comment |
| `src/SDL/main.c` | `/gok64Hz`; `/oracleDump`, `/oracleSelfTest`, `/oracleSelfTestPerturb`; `/vanilla` help text |
| `src/SDL/utility.c` | `oracleEmitGameStart` at the end of `gameStart`; the self-test call in `utyGameSystemsInit` |
| `src/Game/NetCheck.c` | `oracleEmitPacket` at the end of `netcheckShow` |
| `src/Game/UnivUpdate.c` | `univGetChecksum` Cataclysm form (CAT, 0x00549f50) |
| `src/Game/Stats.c` | `statsGetStatChecksum` one CRC (CAT, 0x00525170) |
| `src/Game/OracleDump.c`, `.h` (new) | the hwi-engine producer and the self-test |
| `src/Game/meson.build` | `OracleDump.c` |
| `meson.build`, `meson_options.txt` | determinism flags, `-Doracle`, `hwi_oracle_build.h`, include dir `tools/oracle` |
| `tools/build.sh` | release = `-Doptimization=2 -Ddebug=false` |
| `tools/m2_harness.sh` | `--mode oracle`, `--oracle-ticks`, `--perturb`, `--hwo`; `--bin` resolved; no `/logFileLoads` for a release binary |
| `tools/oracle/hwo_format.h`, `hwo_compare.py`, `selftest.sh` (new) | the format, the comparison tool, the acceptance test |
| `tools/oracle/dll/{hook_sites.h, hwi_oracle.c, hwi_oracle_loader.c, kernel32.def, build.sh, check_sites.py}` (new) | the GOG-side skeleton |
| `docs/m2-r1-bootstrap.md` | 3 `[M2-R2 oracle: ...]` annotations (release flags, 16 Hz, fidelity flags done) |
| `docs/m2-r2-oracle.md` (new) | this report |

Untracked and not for git: `build/oracle-tools/` (scratch helpers `dec.py`, `pe.py`, a one-shot edit script, logs),
`build/oracle-dll/`, and every `.hwo` (game-derived, in `original/scratch-m2/m2r2-harness/runs/`).

## What is left for later stages

1. **The first live oracle session (the user).** Copy `build/oracle-dll/hwi_oracle.dll` and
   `hwi_oracle_loader.exe` next to `cataclysm.exe`, then run, for example, `HWI_ORACLE="mode=record:oracle01;stop=9600;out=oracle01-rec.hwo" tools/run_game.sh
   --no-gamescope --virtual-desktop 1024x768 --exe hwi_oracle_loader.exe -- /logOn` and start a skirmish from the menus
   (the map and players are the open decision 6 of the M2 list). Then make two `mode=play:oracle01;clock=virtual` runs and compare
   them (design 4.4 rungs 1-3). First things to look at: `hwi_oracle.hwo.log` (identity, site check, hooks) and the
   `E` records (hooks, offset check). UNVERIFIED until then: that the DLL loads under Proton, that the H6 trigger
   from the main loop works, that `vqpc` keeps `realtimeelapsed` at 1/16 s per tick, that the offset self-test passes,
   and the meaning of 0x00A6C5C8 (end of playback).
2. **Stage 2 (universe and ships), so that hwi-engine can produce a stream to compare with GOG's**: the save loader
   (design 5.2 stage 1: compare the tick-0 `T` record H7 writes after loading the recording's start save), then
   `univUpdate` and the command layer (stage 2: mode C), then Cataclysm `.pkts` parsing (stage 3: mode A). Stage 2
   should also settle the Ship health offset (+0x170 or 0x158) and the Player layout (0x6530 B: +0xC, +0x35C, and the
   inferred +0x4/+0x2C). Once settled, set the GOG `HWO_TV_HEALTH`/`HWO_TV_PLAYERS` bits.
3. **Ports the oracle depends on, owned by later stages**: `Randy.c` to 27 streams with the wall-clock advance of
   stream 0 in `ranStartup` (0x004efa30) and `cat_ranResetGameStreams` (0x004efb10) in `gameStart`. Until then
   `rng_all` covers 25 streams on this side and is not compared against GOG (the tool knows). Also Cataclysm's
   `realtimeelapsed` and the QPC read in `univUpdate`. When it lands, hwi-engine needs its own `clock=virtual`, the same
   formula as `vqpc`, and the header's clock bit must then mean what it says.
4. **LAN (stage 5, decision 7: nice-to-have)**: the 16-byte sync entry and the 0x1C packet header. The `P` record's
   `extra` is 0 on this side until then.
5. **Fallback (design 6.2)**: `S` snapshot records and windowed re-sync are not built. Build them only if the first
   GOG-vs-hwi-engine comparisons show bit-exactness is out of reach.
6. The verbose net-log text of `univGetChecksum`/`statsGetStatChecksum` is still GoK's BINNETLOG format, not
   Cataclysm's text lines (logging only).

## Counts

| item | value |
|---|---|
| subsystems | 6 (`flags`, `writer`, `selftest`, `compare`, `dll`, `runs`) |
| Cataclysm deltas ported (CAT) | 3: 16 Hz universe (`universeUpdateTask` 0x0053e460), `univGetChecksum` (0x00549f50), `statsGetStatChecksum` (0x00525170) |
| oracle emit points in hwi-engine | 6 call sites: 4 `univUpdate` (H1) [audit M2-R3 universe-mex: 3 since that port, so 5 call sites], `netcheckShow` (H2), `gameStart` (H7) |
| record format | header 64 B; `T` 242 B, `P` 52 B, `E` 8 B + text (each + 4-byte prefix) |
| self-test stream (512 ticks) | 772 records, 140,723 B |
| hwo_compare fields per tick | 21 in causal order |
| GOG hook sites | 7 (H1-H7), 33/33 offline checks pass; 10 game functions called, ~50 globals named in `hook_sites.h` |
| DLL / loader size | 23,040 B / 5,120 B |
| harness runs | 21 lines in `runs.txt` for this item (4 of them the harness-defect attempts) |
| new tracked files | 12 (2 src, 3 tools/oracle, 6 tools/oracle/dll, 1 doc) |
| modified tracked files | 13 |


## 2026-10-17: first live session, findings

1. **Skirmish only.** The user's first session was campaign Mission 1. The engine's recorded-game feature is
   multiplayer-only (`GamePick.c` guards it with `multiPlayerGame`; single-player saves carry the mission chunks before
   the background chunk, `SaveGame.c:682-797`). Recording it anyway crashed the game with an access violation at the
   first order (tick 3367, the first `P` record with `cmds=1`), and the replay of that recording died in `dbgFatalf`
   with `fileSizeGet: can't find file '?'` from `LoadGame -> btgLoad`, the MP loader reading the wrong chunk as the
   background name. The DLL now refuses `record:` unless `multiPlayerGame && !singlePlayerGame` (0x00A43C94 /
   0x00A43C9C) and logs why; `record_reference.sh` and the shortcuts say SKIRMISH ONLY. The campaign recording was
   set aside as `oracle01-campaign-INVALID`.
2. **Crash reporter in the DLL.** A vectored exception handler logs, for access violations and the game's own
   `dbgFatalf` int3: code, address and module, registers, 24+40 stack dwords, 96 dwords above `ebp` (scan them for
   return addresses with the r6 function table to get the call chain), the ebp chain, the recording name globals and
   the fatal-text buffer at 0x00C42C00. Lines start with `CRASH`. `record_reference.sh` also runs the game with
   `WINEDEBUG=+seh` and keeps the Wine log under `original/oracle/`.
3. The single-player save written during that session (`SavedGames/SinglePlayer/01`, 1.1 MB) is a valid mission
   save for the save-format work.

## 2026-10-17: second live session (skirmish) and the first replay verification

- The user played a 10-minute skirmish. The DLL's record gate (added after the campaign crash) wrongly refused it:
  a Cataclysm skirmish is a local game with `multiPlayerGame == 0`. The gate now refuses only `singlePlayerGame != 0`.
  The game's own in-game record path still wrote a valid skirmish recording, `SavedGames\RecordedGames\sto`
  (sp=0, 2 players, 80 ship chunks, Mission17 background), covering the last ~30 s of the session (the user's words).
- **Rungs 1-3 met on `sto`:** two `mode=play:sto;clock=virtual;rng0=split` replays (plain window; the Wine virtual
  desktop dies with an X11 BadWindow when launched from a script) produced streams of 215 T / 107 P / 4 E records
  each; `univ`, `stat`, ship positions/velocities/rotations/health and all list hashes are identical to the last
  tick; the only differing field is `rng_all` from tick 10451 (presentation-side random streams, expected and
  excluded from GOG-vs-hwi comparison). The game's own sync check against the recording's packets reported
  `rand 0 univ 0` errors and 78 `blob` errors from frame 10499 in BOTH replays identically: the simulation replays
  in sync with the live session; the `blob` counter is really the stats/cheat checksum (design 2.4), which depends on RNG stream 0, seeded from
  the wall clock in `ranStartup` (design 2.6): expected to differ between any two processes, same as `rng_all`;
  not a replay defect.
- `tools/oracle/record_reference.sh verify` now replays in a plain window and warns when a replay writes no stream.
- Still needed for the reference: one full 10-minute skirmish recorded through the Record shortcut (now accepted).

## 2026-10-17 14:44: the reference recording is VERIFIED

`SavedGames\RecordedGames\oracle01` (+ `.pkts`, 384,726 B; GOG stream `original/oracle/oracle01-rec-20261017-140428/oracle01-rec.hwo`,
ticks 0..12281, record-stop at 9600): two `mode=play:oracle01;clock=virtual;rng0=split` replays each produced 9,601 T /
4,800 P records; `univ`, `stat`, positions, velocities, rotations, health and all list hashes are IDENTICAL through
tick 9601 (final univ 4a1ef243, 80 objects). Only `rng_all` differs (from tick 197; the wall-clock-seeded stream 0),
and the game's own sync check reports `rand 0 univ 0` errors against the live session's packets (blob = stats
checksum, stream-0 dependent, expected). Streams kept in `original/oracle/oracle01-play-20261017-144421/`. This is
the reference for oracle mode A (M2-R6 producer 7): the recreation must reproduce `oracle01-rec.hwo` tick for tick.
