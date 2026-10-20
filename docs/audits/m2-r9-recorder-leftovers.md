# Audit: M2-R9 producer 4, recorder-leftovers

Base `9c1801f` (branch `m2`). The port is entirely in the working tree: no commits after the base. That is 35 tracked
files, plus the new `docs/m2-r9-recorder-leftovers.md`, `tools/formats/pkts_probe.py` and
`tools/formats/recording_compare.py`. Every engine run went through `tools/m2_harness.sh` (plain harness: silent,
headless, offscreen). Each run used the audit's clean debug build via `--bin`. Run labels are `a9r-*` under
`original/scratch-m2/m2r2-harness/runs/`. No Ghidra, GOG game, oracle DLL, input injection or gamescope.

## 1. Clean builds

Both builds were made in `original/scratch-m2/build-audit-r9-recorder-leftovers/` with tools/build.sh's own arguments:
- debug: `--buildtype=debug -Db_sanitize=address,undefined`
- release: `-Doptimization=2 -Ddebug=false -Db_sanitize=none`

Results:
- Both rc 0, with 0 warnings (meson's `werror` is on by default).
- Sizes: debug `hwi-engine` 88,564,960 B; release 5,614,696 B.
- The audit's comment fixes were rebuilt incrementally: rc 0, 0 warnings, same sizes.

Other checks:
- `m2_save32_gen.py --check`, `m2_kas2c_gen.py --check` and `m2_kasf_table_gen.py --check`: all current.
- `tools/m2_layout32.sh`: OK.

## 2. Bodies compared with the exe (`tools/m2_fdec.py` / `m2_fdis.py` on the cataclysm_gog_r6 export)

| # | function | result |
|---|---|---|
| 1 | univFindNearestResource 0x00549c50 | 0x00549ea4 tests fogOfWar. 0x00549eac `mov eax,[0xa75188]; jne` skips the grid. The grid is read at 0x00549eb5. Tree: `fogOfWar && !recordPackets && grid`. Same |
| 2 | cat_saveSavePlayerFOW 0x004ff6b0 | the first instruction tests 0x00a578f8 and returns, then compares playerIndex with curPlayer. Same |
| 3 | SaveUniverse 0x005034c0 | calls 0x548e00 / 0x549270 / 0x549820 / 0x549ac0 in that order, right after 0x502ff0 (SaveMineFormationList). Same |
| 4 | clSendMove 0x0044eb20 | size `2n+0x124`; 0xCCCC; from; frame 0; numberOfCommands 1 at +0x18; command `(player<<8)|1` at +0x1C; path type +0x1E, from +0x22, numPoints +0x2E, points +0x32, count +0x122, shipNumber (+0x43C) list +0x124. Tree: the same offsets relative to body = +0x1E |
| 5 | clSendMpHyperspace 0x0044ed00 | the same layout: type 0xF, path type 0, one point. Same |
| 6 | clSendFormation 0x0044f690 | returns below 1 ship; size `2n+0x24`; formation +0x1E, level +0x20, count +0x22. Same |
| 7 | clWrapFormation 0x00452ca0 | passes `(short)formation, (short)cat_arg4` to clSendFormation. Same |
| 8 | subTitlesUpdate 0x005842f0 | `shl ecx,5; add ecx,0xabdb20` at 0x0058437b. subTexturesReset bounds the walk at 0xabdb20 + 16 × 0x20. `STT_LocationCard` = 15, `SUB_NumberThemes` = 16. The CAT-FIX only replaces an out-of-table actor |
| 9 | RegisterAllSpaceObjs 0x004fe470 | skips objtype 7; a dead ship gets univRemoveShipReferences; ships inside are registered recursively; the mimic (byte +0x18 & 8) has its fake resource registered. Same |
| 10 | RegisterAllSpaceObjsInsideMe 0x004fe400 | recursive through +0x4B4. Same |
| 11 | recPackInGameStartCBSafeToStart 0x004cbbb0 | the flags 0xa75188 / 0xa75194 / 0xa7519c / 0xa578f8 are set before SaveGame; 0xa578f8 is cleared on both paths; the local branch sets 0xa75190 = 0. Same |
| 12 | recPackRecordSaveHeader 0x004cbdb0 (review row) | writes `0x44414548`, `0x2b0` and 0x2b0 bytes of 0x886028, as the CAT-TODO says |
| 13 | KeepAliveDropPlayerCB 0x00451e70 (review row) | stamps `universe.realtimeelapsed`. The CAT-TODO is correct |
| 14 | mgUnitCapsEnabled 0x004bdce0 (review row) | `flags |= 0x1000; region+0x2c |= 4`. The CAT-TODO is correct |
| 15 | recPackRecordInit 0x004cbd30 | the same as the tree: the `_x.pkts` name, fileDelete 0x00479f60, then recPackRecordSaveHeader. Finding 1: the edit had no review note |
| 16 | aimPreFix_HarassAttack 0x0040c260 (review row, IDENTICAL) | +0x14 to an ID. Same |
| 17 | aimProcessReinforce 0x00409ff0 (IDENTICAL) | the inlined intercept: 0x170 bytes, type 9, 5.0, 0x22 event dwords; 0x0074aaac = 4,000,000.0 = `AIM_INTERCEPT_FINISH_DISTSQ`. Same |
| 18 | aihGravWellEnemyNotNearbyHandler 0x00406830 (IDENTICAL) | `GravWellRadius * 0.8`; 0x0072a470 = 0.8 (double). Same |
| 19 | pingSave 0x004e96f0 (review row) | chunk `userDataSize + 0x58`, IDs at +0x34 / +0x54, the selection after it. This matches the writer: (n+1) dwords from the LP64 slots |
| 20 | SaveHyperspaceGates (review row) | saves count × 0x30 and the block, as the comment says. **The comment's address 0x004904a0 is wrong**: that is an instruction inside the function, which starts at 0x00490470 (finding 2) |
| + | etgMult 0x0046fe50 (review row) | `args[1] * args[0]`. Same |

The delta map's MERGE was idempotent before the audit: a merge onto a copy changed 0 records and the file was
byte-identical. Counts: 2773 / 1564 / 83, reviewed 3511, `m2r9:recorder-leftovers` in done_subsystems. The
report's 165 new entries were confirmed (28 earlier entries were changed).

## 3. The recording: format and replay

**Recordings.** Each audit recording was made by a skirmish run with `--skirmish-ticks 5000 --record audrecNN
--record-stop 4800 --timeout 900`:

| run | map | save | .pkts | record line |
|---|---|---|---|---|
| `a9r-rec01` | Armed8 | `audrec01`, 1,195,596 B | 2,400 packets | `record stopped ... tick=4800 packets=2400 ... pktSyncErr=0`; ASan 0 |
| `a9r-rec02` | LFE2 | `audrec02`, 841,152 B | 2,400 packets | same; ASan 0 |

Each record-run stream is IDENTICAL to the producer's (`r9r-rec01` / `r9r-rec02`).

**Replays.** Each recording was replayed twice: `--mode replay --recording audrecNN --recording-src
<run>/settings/SavedGames/RecordedGames --recording-as audrecNN --timeout 900`.

| pair | `cmp` | run.hwo | load and replay lines |
|---|---|---|---|
| `a9r-play01-a/-b` | identical | `6940f3f65d4b4536` | `1863 chunks converted, 54 ships, 0 size problems`; `replay done ticks=4801 packets=2400 ... pktSyncErr=0` |
| `a9r-play02-a/-b` | identical | `930118d5928dcf3a` | `736 chunks ... 103 ships, 0 size problems` |

Each replay is IDENTICAL to the producer's `r9r-play0N-a` (4,801 ticks / 2,400 packets).

**Recording run against its replay:**

```
python3 -I tools/oracle/hwo_compare.py runs/a9r-recNN/run.hwo runs/a9r-playNN-a/run.hwo --from-tick 1 --to-tick 4800 \
  --fields rng10,ships_n,ships_misc,ships_pos,ships_vel,ships_health,ships_rot,missiles,bullets,derelicts,resources,univcheck,players,dt,total,real,fpcw
```

- The result is IDENTICAL for both: 4,800 ticks and 2,400 packets.
- With all fields, the first divergence is tick 2 in rng0 (plus rng_all).
- rng6 alone first diverges at tick 321, as the report says.

**Format against GOG, byte by byte, with the audit's own parsers (not the producer's tools):**
- **Save:**
  - The version is 0x10ad0027 in both.
  - Both walk chunk by chunk to EOF: 3,377 GOG 10-17 chunks and 3,390 `audrec01` chunks.
  - Chunks 0-27 (the single-player flag, the song, the player count, the 0x90 names and the INFO words) are
    **byte-identical**.
  - Chunk 28 is the 0x2B0 tpGameCreated in both, with 0 non-zero bytes after +0xC8 in both.
  - The 22 fixed-size chunk types both files share have no size mismatch.
- **.pkts:**
  - Every record is `PACK` + size + packet, type 0x5555, from 0.
  - Frames are consecutive from 0, `catExtra` is 0, and numberOfCommands is at +0x18.
  - The producer's `recording_compare.py` gives rc 0, `SAME FORMAT`, for both audit recordings.
- **Content difference not in the report (finding 5):**
  - GOG: the checksum lags 8 packets. Packets 0-7 have randcheck 0, and packetnum = frame - 8.
  - Ours: the lag is 1 (packetnum = frame - 1).
  - The fill (netcheckFillInChecksum 0x004cc2f0) and the 128-entry ring (netcheckShow 0x004cc400, `frame & 0x7f`)
    are the exe's, so a GOG playback should find the entries. Added to the report as an `[audit: ...]` note.

**The user's GOG playback steps (report section 3.6):**
- Every file and command is named:
  - the two `cp` lines, with the four expected hashes; they match the files in `r9r-rec01` / `r9r-rec02`;
  - `ORACLE_NAME=... tools/oracle/record_reference.sh verify`. The main checkout's script is byte-identical to the
    worktree's. Its `verify` installs the newest `build/oracle-dll`, which holds both `hwi_oracle.dll` and
    `hwi_oracle_loader.exe`. It replays twice (`rng0=split`) and collects into
    `original/oracle/<NAME>-play-<ts>/<NAME>-play-a.hwo`, as section 3.6 states;
  - the `hwo_compare` line against `r9r-play0N-a`.

## 4. Gate runs (audit clean debug build, plain harness)

| run | result | stream (sha256/16) | against the reference |
|---|---|---|---|
| `a9r-mission-a/-b` (`tools/oracle/mission_test.sh --ticks 6000`) | MISSION TEST PASS (pair, perturbed run at 1500 rng10, flipped copy at 2000); ASan 0/0 | **`fd82fafbcd9011fc`** pair | `a9m3-mission-a` (`dc2fb8ce71c0d26b`, producer 3's audit): IDENTICAL, 6,001 ticks (only the build identity differs) |
| `a9r-mt1-a/-b` (Mission01, `--mission-test-ticks 30000`) | PASS 6/6; ASan 0/0 | `ca7ee113b86a118b` pair | `a9m3-mt1`: IDENTICAL, 11,426 ticks |
| `a9r-m5-a/-b` (`--mission 5`) | `done mission=5 ticks=16384 objectives=6/6 result=PASS`; ASan 0/0 | `f43d760210197cee` pair | `a9m3-m5`: IDENTICAL |
| `a9r-m13-a/-b` (`--mission 13`) | `objectives=3/3 result=PASS`; ASan 0/0 | `cff52c611a4be724` pair | `a9m3-m13-a`: IDENTICAL |
| `a9r-tut-a/-b` (`--tutorial --mission-test-ticks 30000`) | lessons 12/12 PASS; ASan 0/0 | `7ce11b952c0ef020` pair (= producer) | `a9m3-tut` (`2dbb97754c1ca05f`): IDENTICAL through 4057, first divergence 4058 in rng0 / rng_all (finding 6) |
| `a9r-nis` | `motiontest: done ... result=PASS`; ASan 0 | | |
| `a9r-ai` | PASS; ASan 0 | `89e5fb99fb9494b5` | `a9m3-ai`: IDENTICAL |
| `a9r-lfe2` | PASS; ASan 0 | `310be1aa2c60bd00` | `a9m3-lfe2`: IDENTICAL |
| `a9r-fe` (120 s) | Main_game_screen, still running at the timeout; ASan 0, no leak | | |
| `a9r-sp2` (`--timeout 340`) | universe running, then the known `texreg.c (569)` Mission04 assertion; ASan 0; 40 bytes leaked after the fatal exit | | as `a9m3-sp` |

**The `a9r-sp` incident.** The first sp run, `a9r-sp`, lost its summary to an audit mistake. The audit edited
`tools/m2_harness.sh` (finding 4's comment) while that run was in flight. bash reads a script as it runs, so the run
stopped with `t0: unbound variable`. Afterwards the script passes `bash -n`, and `a9r-sp2` is the clean rerun.

**Subtitle check.** No `Subtitle.c` report appears in sp, nis, the tutorial, Mission01, 05 or 13. The same grep finds
2 reports in each of producer 3's `a9m3-mt1`, `a9m3-m5` and `a9m3-sp`.

**Producer's rung replays.** `r9r-r17` / `r9r-r18` against producer 1's audit runs `r9a2-r17-a` / `r9a2-r18-a`:
IDENTICAL (9,601 / 3,527 ticks).

## 5. The diff

- **Scope:** every changed file is in the producer's list, and nothing outside the port was touched.
- **Labels:** the Cataclysm changes carry `CAT (M2-R9 recorder-leftovers)` / `CAT-FIX` with exe addresses. The
  HWI-only parts are labelled HWI-ORACLE / HWI-TEST / HWI-FIX: the switches, the virtual audio clock and the recorder
  paths.
- **CAT-TODOs** (`grep -a`, since some sources read as binary):
  - `CAT-TODO(stage 5 recorder` count: 0.
  - CAT-TODOs naming M2-R8 / M2-R9: 0.
  - 25 owner-tagged CAT-TODOs in total: stage 5 8, LAN 6, multiplayer options 8, multiplayer 1, M2-R10 physics 2.
  - The 3 new ones are each on one line with the owner first.
- **Generated files:** all current (section 1).
- **Leftovers:**
  - (a) combat: re-targeted to M2-R10 test, with a measured reason;
  - (b) the tutorial pair: done;
  - (c) Subtitle: done;
  - (d) the 123 stage-4 bodies and a 31-row sample of the unflagged rows: done;
  - the 4 recorder sites: done.
- **Mode-B rung:** the user's session.

## 6. Findings

1. **An edited IDENTICAL function had no review note** (fixed).
   - recPackRecordInit 0x004cbd30 had its fileDelete replaced by `recPackDeleteFile`, with no entry. The report's
     section 7 says every edited function with a row has a note.
   - The audit added the entry (body compared, same) and merged: reviewed 3511 → 3512. The report is corrected
     `[audit: ...]`.
2. **Wrong exe address in a CAT comment** (fixed). HS.c `SaveHyperspaceGates 0x004904a0`: the function is at
   0x00490470, and 0x004904a0 is mid-instruction. Corrected `[audit: ...]`.
3. **Stale comment** (fixed). OracleDump.c `oracleRecordTick`'s header said the packets land in the working
   directory. NetCheck.c's HWI-FIX opens them in the settings path. Corrected.
4. **Stale comment** (fixed). In `tools/m2_harness.sh` the `--record` block said "packets: the cwd". Same correction.
5. **Report: format comparison incomplete** (fixed, doc).
   - The sync-checksum lag (GOG packetnum = frame - 8, ours frame - 1) is a content difference that "field by field"
     does not cover. No tool checks it.
   - An `[audit: ...]` note was added to section 3.4.
   - This does not block mode B: it uses the same ring mechanism.
6. **Report: stream move without its first tick** (fixed, doc).
   - The Tutorial1 move was stated without its fields or first tick.
   - Against `a9m3-tut` the stream is IDENTICAL through 4057 and first diverges at 4058 in rng0 / rng_all.
   - Added `[audit: ...]` to section 9.

**Not findings:** both items below were checked but could not be settled without the exe allocator's rounding.
- clSendMove's comment says the exe's block "would overflow only from 25 points on". By the block size alone, 21
  points overflow it for n ≤ 4.
- The tree's allocation is sized for 21 points, so the tree itself cannot overflow.

## 7. Precision and fixes

**Sampled checks: 68.**

| checks | count |
|---|---|
| clean builds | 2 |
| exe bodies | 20 |
| review rows | 5 |
| format claims | 4 |
| replay determinism, identity and exclusion ticks | 6 |
| GOG steps and hashes | 2 |
| gate / guard-rail runs and their reference comparisons | 16 |
| subtitle fix and reports | 2 |
| CAT-TODO counts, generators, layout32, merge state | 5 |
| the six findings | 6 |

Wrong: 6 (findings 1-6). **Precision = 62 / 68 = 0.91.**

**Fixes: 7.**
- 3 source / tool comments: HS.c, OracleDump.c, m2_harness.sh.
- 1 review entry, merged into delta_map.json.
- 3 report notes.

No fix changes code. The builds stay green with 0 warnings, and no stream moved.

## 8. Reference streams for the next port

| stream | hash (debug, audit clean build) | note |
|---|---|---|
| headless Mission01, 6,000 ticks | **`fd82fafbcd9011fc`** | content = `dc2fb8ce71c0d26b` |
| Mission01 self-test | `ca7ee113b86a118b` | |
| Mission05 | `f43d760210197cee` | |
| Mission13 | `cff52c611a4be724` | |
| Tutorial1 leg | `7ce11b952c0ef020` | a pair, on the virtual audio clock |
| ai | `89e5fb99fb9494b5` | |
| LFE2 | `310be1aa2c60bd00` | |
| hwi recordings' replays | `6940f3f65d4b4536` (`audrec01`), `930118d5928dcf3a` (`audrec02`) | content = the producer's `5e45a4da` / `1c9fc262` |

fe: Main_game_screen, ASan 0. sp: ASan 0, with the Mission04 texreg stop (M2-R10 render).

Verdict: PASS-WITH-FIXES - the 4 recorder sites match the exe and are closed; hwi-engine's own recordings replay byte-identical and IDENTICAL to their recording runs; the format matches GOG's; the GOG steps are complete; the Tutorial1 leg is a pair; there are no Subtitle reports; Mission01 and every guard-rail stream are unchanged; 7 fixes (1 review row added, 3 comments, 3 report notes)
